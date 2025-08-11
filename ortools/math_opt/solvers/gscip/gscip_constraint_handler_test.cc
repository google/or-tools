// Copyright 2010-2025 Google LLC
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// TODO(b/314630175): GScipCallbackStats is untested.

#include "ortools/math_opt/solvers/gscip/gscip_constraint_handler.h"

#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/solvers/gscip/gscip.h"
#include "ortools/math_opt/solvers/gscip/gscip.pb.h"
#include "ortools/math_opt/solvers/gscip/gscip_callback_result.h"
#include "ortools/math_opt/solvers/gscip/gscip_parameters.h"
#include "ortools/math_opt/solvers/gscip/gscip_testing.h"
#include "scip/type_cons.h"
#include "scip/type_var.h"

namespace operations_research {

namespace {

constexpr double kTolerance = 1e-6;

struct AtMostOneData {
  std::vector<SCIP_VAR*> vars;
};

// Constraint handler to enforce that the sum of variables is at most one. Used
// to test constraint enforcement (EnforceLp and EnforcePseudoSolution) or
// separation (SeparateLp and SeparateSolution) if use_as_separator is true, but
// also implicitly covers the CheckIsFeasible and RoundingLock code paths.
class AtMostOneConstraintHandler
    : public GScipConstraintHandler<AtMostOneData> {
 public:
  explicit AtMostOneConstraintHandler(const bool use_as_separator)
      : GScipConstraintHandler<AtMostOneData>(
            {.name = "test_constraint_handler",
             .description = "Checks if sum_i x_i <= 1."}),
        use_as_separator_(use_as_separator) {}

  absl::StatusOr<GScipCallbackResult> EnforceLp(
      GScipConstraintHandlerContext context,
      const AtMostOneData& constraint_data, bool) override {
    enforce_lp_is_called_ = true;
    if (IsViolated(context, constraint_data)) {
      RETURN_IF_ERROR(AddConstraint(context, constraint_data));
      return GScipCallbackResult::kConstraintAdded;
    }
    return GScipCallbackResult::kFeasible;
  }

  absl::StatusOr<GScipCallbackResult> EnforcePseudoSolution(
      GScipConstraintHandlerContext context,
      const AtMostOneData& constraint_data, bool, bool) override {
    // Typically we would mimic EnforceLp here. However, we test this code path
    // by disabling LP altogether, and adding a constraint does not induce
    // further propagation. We return infeasible instead to induce branching.
    enforce_pseudo_solution_is_called_ = true;
    if (IsViolated(context, constraint_data)) {
      return GScipCallbackResult::kInfeasible;
    }
    return GScipCallbackResult::kFeasible;
  }

  absl::StatusOr<bool> CheckIsFeasible(GScipConstraintHandlerContext context,
                                       const AtMostOneData& constraint_data,
                                       bool, bool, bool, bool) override {
    return !IsViolated(context, constraint_data);
  }

  std::vector<std::pair<SCIP_VAR*, RoundingLockDirection>> RoundingLock(
      GScip*, const AtMostOneData& constraint_data, bool) override {
    std::vector<std::pair<SCIP_VAR*, RoundingLockDirection>> result;
    for (SCIP_VAR* var : constraint_data.vars) {
      // Lock upwards, i.e. increasing values may violate constraint.
      result.push_back({var, RoundingLockDirection::kUp});
    }
    return result;
  }

  absl::StatusOr<GScipCallbackResult> SeparateLp(
      GScipConstraintHandlerContext context,
      const AtMostOneData& constraint_data) override {
    if (!use_as_separator_) {
      return GScipCallbackResult::kDidNotFind;
    }
    separate_lp_is_called_ = true;
    if (IsViolated(context, constraint_data)) {
      return AddCut(context, constraint_data);
    }
    return GScipCallbackResult::kDidNotFind;
  }

  absl::StatusOr<GScipCallbackResult> SeparateSolution(
      GScipConstraintHandlerContext context,
      const AtMostOneData& constraint_data) override {
    if (!use_as_separator_) {
      return GScipCallbackResult::kDidNotFind;
    }
    separate_solution_is_called_ = true;
    if (IsViolated(context, constraint_data)) {
      return AddCut(context, constraint_data);
    }
    return GScipCallbackResult::kDidNotFind;
  }

  bool enforce_lp_is_called() const { return enforce_lp_is_called_; }
  bool enforce_pseudo_solution_is_called() const {
    return enforce_pseudo_solution_is_called_;
  }
  bool separate_lp_is_called() const { return separate_lp_is_called_; }
  bool separate_solution_is_called() const {
    return separate_solution_is_called_;
  }

 private:
  bool IsViolated(GScipConstraintHandlerContext context,
                  const AtMostOneData& constraint_data) {
    double sum = 0.0;
    for (SCIP_VAR* var : constraint_data.vars) {
      sum += context.VariableValue(var);
    }
    return sum > 1.0 + kTolerance;
  }

  GScipLinearRange InequalityAsLinearRange(
      const AtMostOneData& constraint_data) {
    GScipLinearRange range;
    range.upper_bound = 1.0;
    range.variables = constraint_data.vars;
    range.coefficients = std::vector<double>(constraint_data.vars.size(), 1.0);
    return range;
  }

  absl::Status AddConstraint(GScipConstraintHandlerContext context,
                             const AtMostOneData& constraint_data) {
    return context.AddLazyLinearConstraint(
        InequalityAsLinearRange(constraint_data), "at_most_one");
  }

  absl::StatusOr<GScipCallbackResult> AddCut(
      GScipConstraintHandlerContext context,
      const AtMostOneData& constraint_data) {
    return context.AddCut(InequalityAsLinearRange(constraint_data),
                          "at_most_one");
  }

  bool use_as_separator_;

  bool enforce_lp_is_called_ = false;
  bool enforce_pseudo_solution_is_called_ = false;
  bool separate_lp_is_called_ = false;
  bool separate_solution_is_called_ = false;
};

TEST(GScipConstraintHandlerTest, AtMostOneConstraintIsEnforced) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x1,
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "x1"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x2,
      gscip->AddVariable(0.0, 1.0, 2.0, GScipVarType::kInteger, "x2"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x3,
      gscip->AddVariable(0.0, 1.0, 3.0, GScipVarType::kInteger, "x3"));
  ASSERT_OK(gscip->SetMaximize(true));
  AtMostOneConstraintHandler handler(/*use_as_separator=*/false);
  ASSERT_OK(handler.Register(gscip.get()));
  AtMostOneData constraint_data{.vars = {x1, x2, x3}};
  ASSERT_OK(handler.AddCallbackConstraint(gscip.get(), "AtMostOne_123",
                                          &constraint_data,
                                          GScipConstraintOptions()));
  GScipParameters params;
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip->Solve(params));
  ASSERT_EQ(result.gscip_output.status(), GScipOutput::OPTIMAL);
  EXPECT_NEAR(result.gscip_output.stats().best_bound(), 3.0, kTolerance);
  EXPECT_NEAR(result.gscip_output.stats().best_objective(), 3.0, kTolerance);
  ASSERT_GE(result.solutions.size(), 1);
  GScipSolution expected({{x1, 0.0}, {x2, 0.0}, {x3, 1.0}});
  EXPECT_THAT(result.solutions[0], GScipSolutionAlmostEquals(expected));
  EXPECT_TRUE(handler.enforce_lp_is_called());
}

TEST(GScipConstraintHandlerTest, AtMostOneConstraintCanBeDeleted) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x1,
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "x1"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x2,
      gscip->AddVariable(0.0, 1.0, 2.0, GScipVarType::kInteger, "x2"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x3,
      gscip->AddVariable(0.0, 1.0, 3.0, GScipVarType::kInteger, "x3"));
  ASSERT_OK(gscip->SetMaximize(true));
  AtMostOneConstraintHandler handler(/*use_as_separator=*/false);
  ASSERT_OK(handler.Register(gscip.get()));
  AtMostOneData constraint_data{.vars = {x1, x2, x3}};
  ASSERT_OK_AND_ASSIGN(SCIP_CONS * at_most_cons,
                       handler.AddCallbackConstraint(
                           gscip.get(), "AtMostOne_123", &constraint_data,
                           GScipConstraintOptions()));
  GScipParameters params;
  {
    ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip->Solve(params));
    ASSERT_EQ(result.gscip_output.status(), GScipOutput::OPTIMAL);
    EXPECT_NEAR(result.gscip_output.stats().best_objective(), 3.0, kTolerance);
  }
  ASSERT_OK(gscip->DeleteConstraint(at_most_cons));
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip->Solve(params));
  ASSERT_EQ(result.gscip_output.status(), GScipOutput::OPTIMAL);
  EXPECT_NEAR(result.gscip_output.stats().best_objective(), 6.0, kTolerance);
}

TEST(GScipConstraintHandlerTest, MultipleAtMostOneConstraintsAreEnforced) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x1,
      gscip->AddVariable(0.0, 1.0, 2.0, GScipVarType::kInteger, "x1"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x2,
      gscip->AddVariable(0.0, 1.0, 2.0, GScipVarType::kInteger, "x2"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x3,
      gscip->AddVariable(0.0, 1.0, 3.0, GScipVarType::kInteger, "x3"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x4,
      gscip->AddVariable(0.0, 1.0, 2.0, GScipVarType::kInteger, "x4"));
  ASSERT_OK(gscip->SetMaximize(true));
  AtMostOneConstraintHandler handler(/*use_as_separator=*/false);
  ASSERT_OK(handler.Register(gscip.get()));
  AtMostOneData constraint_data_123{.vars = {x1, x2, x3}};
  AtMostOneData constraint_data_234{.vars = {x2, x3, x4}};
  ASSERT_OK(handler.AddCallbackConstraint(gscip.get(), "AtMostOne_123",
                                          &constraint_data_123,
                                          GScipConstraintOptions()));
  ASSERT_OK(handler.AddCallbackConstraint(gscip.get(), "AtMostOne_234",
                                          &constraint_data_234,
                                          GScipConstraintOptions()));
  GScipParameters params;
  // Unique solution is to select x1 and x4 to be one.
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip->Solve(params));
  ASSERT_EQ(result.gscip_output.status(), GScipOutput::OPTIMAL);
  EXPECT_NEAR(result.gscip_output.stats().best_bound(), 4.0, kTolerance);
  EXPECT_NEAR(result.gscip_output.stats().best_objective(), 4.0, kTolerance);
  ASSERT_GE(result.solutions.size(), 1);
  GScipSolution expected({{x1, 1.0}, {x2, 0.0}, {x3, 0.0}, {x4, 1.0}});
  EXPECT_THAT(result.solutions[0], GScipSolutionAlmostEquals(expected));
  EXPECT_TRUE(handler.enforce_lp_is_called());
}

TEST(GScipConstraintHandlerTest, AtMostOneConstraintIsNotEnforcedIfDisabled) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x1,
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "x1"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x2,
      gscip->AddVariable(0.0, 1.0, 2.0, GScipVarType::kInteger, "x2"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x3,
      gscip->AddVariable(0.0, 1.0, 3.0, GScipVarType::kInteger, "x3"));
  ASSERT_OK(gscip->SetMaximize(true));
  AtMostOneConstraintHandler handler(/*use_as_separator=*/false);
  ASSERT_OK(handler.Register(gscip.get()));
  AtMostOneData constraint_data{.vars = {x1, x2, x3}};
  GScipConstraintOptions constraint_options;
  constraint_options.enforce = false;  // Disable enforcement.
  ASSERT_OK(handler.AddCallbackConstraint(
      gscip.get(), "AtMostOne_123", &constraint_data, constraint_options));
  GScipParameters params;
  // Optimal solution should be all variables set to one since this was the only
  // constraint besides bounds and integrality, and it is disabled.
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip->Solve(params));
  ASSERT_EQ(result.gscip_output.status(), GScipOutput::OPTIMAL);
  EXPECT_NEAR(result.gscip_output.stats().best_bound(), 6.0, kTolerance);
  EXPECT_NEAR(result.gscip_output.stats().best_objective(), 6.0, kTolerance);
  ASSERT_GE(result.solutions.size(), 1);
  GScipSolution expected({{x1, 1.0}, {x2, 1.0}, {x3, 1.0}});
  EXPECT_THAT(result.solutions[0], GScipSolutionAlmostEquals(expected));
  EXPECT_FALSE(handler.enforce_lp_is_called());
  EXPECT_FALSE(handler.enforce_pseudo_solution_is_called());
}

// This tests disables LPs to test EnforcePseudoSolution.
TEST(GScipConstraintHandlerTest, AtMostOneConstraintIsEnforcedWithoutLp) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x1,
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "x1"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x2,
      gscip->AddVariable(0.0, 1.0, 2.0, GScipVarType::kInteger, "x2"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x3,
      gscip->AddVariable(0.0, 1.0, 3.0, GScipVarType::kInteger, "x3"));
  ASSERT_OK(gscip->SetMaximize(true));
  AtMostOneConstraintHandler handler(/*use_as_separator=*/false);
  ASSERT_OK(handler.Register(gscip.get()));
  AtMostOneData constraint_data{.vars = {x1, x2, x3}};
  ASSERT_OK(handler.AddCallbackConstraint(gscip.get(), "AtMostOne_123",
                                          &constraint_data,
                                          GScipConstraintOptions()));
  GScipParameters params;
  (*params.mutable_int_params())["lp/solvefreq"] = -1;  // Disable LP.
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip->Solve(params));
  ASSERT_EQ(result.gscip_output.status(), GScipOutput::OPTIMAL);
  EXPECT_NEAR(result.gscip_output.stats().best_bound(), 3.0, kTolerance);
  EXPECT_NEAR(result.gscip_output.stats().best_objective(), 3.0, kTolerance);
  ASSERT_GE(result.solutions.size(), 1);
  GScipSolution expected({{x1, 0.0}, {x2, 0.0}, {x3, 1.0}});
  EXPECT_THAT(result.solutions[0], GScipSolutionAlmostEquals(expected));
  EXPECT_FALSE(handler.enforce_lp_is_called());
  EXPECT_TRUE(handler.enforce_pseudo_solution_is_called());
}

// Builds the following MIP:
//   max  2x + 3y
//   s.t. 2x + 4y <= 5
//        x, y in {0, 1}
//
// The LP relaxation solution is (1.0, 0.75) with objective 4.25. The optimal
// solution is (0.0, 1.0) with objective 3.0. This is used to test the
// separation callbacks (SeparateLp, SeparateSolution).
class GScipMipThatBranchesTest : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_OK_AND_ASSIGN(gscip_, GScip::Create("scip_test"));
    ASSERT_OK_AND_ASSIGN(
        x_, gscip_->AddVariable(0.0, 1.0, 2.0, GScipVarType::kInteger, "x"));
    ASSERT_OK_AND_ASSIGN(
        y_, gscip_->AddVariable(0.0, 1.0, 3.0, GScipVarType::kInteger, "y"));
    ASSERT_OK(gscip_->AddLinearConstraint(
        {.variables = {x_, y_}, .coefficients = {2.0, 4.0}, .upper_bound = 5.0},
        "c", {.separate = false}));
    ASSERT_OK(gscip_->SetMaximize(true));
    params_ = TestGScipParameters();
    params_.set_presolve(GScipParameters::OFF);
    params_.set_heuristics(GScipParameters::OFF);
    DisableAllCutsExceptUserDefined(&params_);
  }

 protected:
  std::unique_ptr<GScip> gscip_;
  SCIP_VAR* x_;
  SCIP_VAR* y_;
  GScipParameters params_;
};

TEST_F(GScipMipThatBranchesTest, MipThatBranchesProblemIsFractional) {
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip_->Solve(params_));
  AssertOptimalWithBestSolution(result, 3.0, {{x_, 0.0}, {y_, 1.0}});
  EXPECT_GE(result.gscip_output.stats().node_count(), 2);
  EXPECT_NEAR(result.gscip_output.stats().first_lp_relaxation_bound(), 4.25,
              kTolerance);
}

// Tests if adding the valid cut x + y <= 1 (as separation) results in finding
// an optimal solution at the root, via the SeparateLp function in the callback.
TEST_F(GScipMipThatBranchesTest, SeparationCallbackAvoidsBranching) {
  AtMostOneConstraintHandler handler(/*use_as_separator=*/true);
  ASSERT_OK(handler.Register(gscip_.get()));
  AtMostOneData constraint_data{.vars = {x_, y_}};
  ASSERT_OK(handler.AddCallbackConstraint(gscip_.get(), "SeparationConstraint",
                                          &constraint_data,
                                          GScipConstraintOptions()));
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip_->Solve(params_));
  ASSERT_EQ(result.gscip_output.status(), GScipOutput::OPTIMAL);
  AssertOptimalWithBestSolution(result, 3.0, {{x_, 0.0}, {y_, 1.0}});
  EXPECT_TRUE(handler.separate_lp_is_called());
  // Expect that SCIP did not need to branch.
  EXPECT_EQ(result.gscip_output.stats().node_count(), 1);
}

TEST_F(GScipMipThatBranchesTest, SeparateSolutionIsCalled) {
  AtMostOneConstraintHandler handler(/*use_as_separator=*/true);
  ASSERT_OK(handler.Register(gscip_.get()));
  AtMostOneData constraint_data{.vars = {x_, y_}};
  ASSERT_OK(handler.AddCallbackConstraint(gscip_.get(), "SeparationConstraint",
                                          &constraint_data,
                                          GScipConstraintOptions()));
  // We turn on the closecuts separator which uses solution separation.
  (*params_.mutable_int_params())["separating/closecuts/freq"] = 0;
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip_->Solve(params_));
  ASSERT_EQ(result.gscip_output.status(), GScipOutput::OPTIMAL);
  AssertOptimalWithBestSolution(result, 3.0, {{x_, 0.0}, {y_, 1.0}});
  EXPECT_TRUE(handler.separate_solution_is_called());
  // Expect that SCIP did not need to branch.
  EXPECT_EQ(result.gscip_output.stats().node_count(), 1);
}

TEST(GScipConstraintHandlerTest, ConstraintHandlerFailsCorrectly) {
  struct EmptyConstraintData {};
  class ConstraintHandlerThatFails
      : public GScipConstraintHandler<EmptyConstraintData> {
   public:
    ConstraintHandlerThatFails()
        : GScipConstraintHandler<EmptyConstraintData>(
              {.name = "test_failure"}) {}

    absl::StatusOr<bool> CheckIsFeasible(GScipConstraintHandlerContext,
                                         const EmptyConstraintData&, bool, bool,
                                         bool, bool) override {
      return absl::InternalError("Failed inside callback");
    }
  };
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK(gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "x"));
  ConstraintHandlerThatFails handler;
  ASSERT_OK(handler.Register(gscip.get()));
  EmptyConstraintData constraint_data;
  ASSERT_OK(handler.AddCallbackConstraint(gscip.get(), "FailureConstraint",
                                          &constraint_data,
                                          GScipConstraintOptions()));
  GScipParameters params;
  EXPECT_THAT(
      gscip->Solve(params),
      testing::status::StatusIs(absl::StatusCode::kInternal,
                                testing::HasSubstr("Failed inside callback")));
}

// Circle constraint handler test. Used to test a more complicated handler that
// may require multiple inequalities.

struct Point {
  double x1 = 0.0;
  double x2 = 0.0;
};

struct Circle {
  Point center;
  double r = 0.0;
};

struct CircleConstraintData {
  SCIP_VAR* x1 = nullptr;
  SCIP_VAR* x2 = nullptr;
  Circle circle;
};

Point Difference(const Point left, const Point right) {
  return {.x1 = left.x1 - right.x1, .x2 = left.x2 - right.x2};
}

Point Multiply(const Point left, const double right) {
  return {.x1 = left.x1 * right, .x2 = left.x2 * right};
}

double InnerProduct(const Point left, const Point right) {
  return left.x1 * right.x1 + left.x2 * right.x2;
}

double NormSquared(const Point p) { return InnerProduct(p, p); }

double Norm(const Point p) { return std::sqrt(NormSquared(p)); }

bool PointInCircle(const Point x, const Circle& circle,
                   const double tolerance) {
  return NormSquared(Difference(x, circle.center)) <=
         std::pow(circle.r, 2) + tolerance;
}

class CircleConstraintHandler
    : public GScipConstraintHandler<CircleConstraintData> {
 public:
  CircleConstraintHandler()
      : GScipConstraintHandler<CircleConstraintData>(
            {.name = "circle_callback",
             .description =
                 "Constraint of the form ||center - x||^2 <= r^2."}) {}

  absl::StatusOr<GScipCallbackResult> EnforceLp(
      GScipConstraintHandlerContext context,
      const CircleConstraintData& constraint_data, bool) override {
    const Point current_point = GetCurrentPoint(context, constraint_data);
    if (PointInCircle(current_point, constraint_data.circle, kTolerance)) {
      return GScipCallbackResult::kFeasible;
    }
    RETURN_IF_ERROR(context.AddLazyLinearConstraint(
        SeparationInequality(current_point, constraint_data),
        "circle_constraint"));
    return GScipCallbackResult::kConstraintAdded;
  }

  absl::StatusOr<GScipCallbackResult> EnforcePseudoSolution(
      GScipConstraintHandlerContext context,
      const CircleConstraintData& constraint_data, bool, bool) override {
    const Point current_point = GetCurrentPoint(context, constraint_data);
    if (PointInCircle(current_point, constraint_data.circle, kTolerance)) {
      return GScipCallbackResult::kFeasible;
    }
    RETURN_IF_ERROR(context.AddLazyLinearConstraint(
        SeparationInequality(current_point, constraint_data),
        "circle_constraint"));
    return GScipCallbackResult::kConstraintAdded;
  }

  absl::StatusOr<bool> CheckIsFeasible(
      GScipConstraintHandlerContext context,
      const CircleConstraintData& constraint_data, bool, bool, bool,
      bool) override {
    return PointInCircle(GetCurrentPoint(context, constraint_data),
                         constraint_data.circle, kTolerance);
  }

 private:
  // Given a circle with center c and radius r, and a point p not in this
  // circle, returns an inequality that separates p from the circle. The
  // inequality is:
  //   <d, x - c> <= r^2
  // or equivalently:
  //   <d, x> <= r^2 + <d, c>
  // where d = r * (p - c) / ||p - c||.
  GScipLinearRange SeparationInequality(
      const Point p, const CircleConstraintData circle_constraint) {
    const Circle circle = circle_constraint.circle;
    CHECK(!PointInCircle(p, circle, 0.0));
    const Point p_centered = Difference(p, circle.center);
    const double p_centered_norm = Norm(p_centered);
    const Point coefficients = Multiply(p_centered, circle.r / p_centered_norm);
    const double upper_bound =
        circle.r * circle.r + InnerProduct(coefficients, circle.center);
    return {.variables = {circle_constraint.x1, circle_constraint.x2},
            .coefficients = {coefficients.x1, coefficients.x2},
            .upper_bound = upper_bound};
  }

  Point GetCurrentPoint(GScipConstraintHandlerContext context,
                        const CircleConstraintData& constraint_data) {
    return {.x1 = context.VariableValue(constraint_data.x1),
            .x2 = context.VariableValue(constraint_data.x2)};
  }
};

TEST(ScipCallbackTest, SimpleCircleTest) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_circle_test"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x1,
      gscip->AddVariable(-10.0, 10.0, 1.0, GScipVarType::kInteger, "x1"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x2,
      gscip->AddVariable(-10.0, 10.0, 1.0, GScipVarType::kInteger, "x2"));
  ASSERT_OK(gscip->SetMaximize(true));
  GScipParameters params;
  CircleConstraintHandler circle_constraint_handler;
  ASSERT_OK(circle_constraint_handler.Register(gscip.get()));
  CircleConstraintData circle_constraint;
  circle_constraint.x1 = x1;
  circle_constraint.x2 = x2;
  circle_constraint.circle.r = 3.0;
  circle_constraint.circle.center.x1 = 4.0;
  circle_constraint.circle.center.x2 = 5.0;

  GScipConstraintOptions constraint_options;
  ASSERT_OK(circle_constraint_handler.AddCallbackConstraint(
      gscip.get(), "circle_constraint", &circle_constraint,
      constraint_options));
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip->Solve(params));
  ASSERT_EQ(result.gscip_output.status(), GScipOutput::OPTIMAL);
  // The unique integer solution that maximizes x1 + x2 within a circle of
  // radius 3 is (center.x1 + 2, center.x2 + 2).
  EXPECT_NEAR(result.gscip_output.stats().best_objective(), 13.0, 1e-5);
  ASSERT_GE(result.solutions.size(), 1);
  const GScipSolution expected({{x1, 6.0}, {x2, 7.0}});
  EXPECT_THAT(result.solutions.at(0), GScipSolutionAlmostEquals(expected));
}

TEST(ScipCallbackTest, CircleIntersectionTest) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_circle_test"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x1,
      gscip->AddVariable(-3.0, 3.0, 0.0, GScipVarType::kInteger, "x1"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x2,
      gscip->AddVariable(-3.0, 3.0, 1.0, GScipVarType::kInteger, "x2"));
  ASSERT_OK(gscip->SetMaximize(true));
  GScipParameters params;
  CircleConstraintHandler circle_constraint_handler;
  ASSERT_OK(circle_constraint_handler.Register(gscip.get()));
  GScipConstraintOptions constraint_options;

  CircleConstraintData circle_right;
  circle_right.x1 = x1;
  circle_right.x2 = x2;
  circle_right.circle.r = 2.0;
  circle_right.circle.center.x1 = 1.0;
  circle_right.circle.center.x2 = 0.0;
  ASSERT_OK(circle_constraint_handler.AddCallbackConstraint(
      gscip.get(), "right_constraint", &circle_right, constraint_options));

  CircleConstraintData circle_left;
  circle_left.x1 = x1;
  circle_left.x2 = x2;
  circle_left.circle.r = 2.0;
  circle_left.circle.center.x1 = -1.0;
  circle_left.circle.center.x2 = 0.0;
  ASSERT_OK(circle_constraint_handler.AddCallbackConstraint(
      gscip.get(), "left_constraint", &circle_left, constraint_options));

  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip->Solve(params));
  ASSERT_EQ(result.gscip_output.status(), GScipOutput::OPTIMAL);
  EXPECT_NEAR(result.gscip_output.stats().best_objective(), 1.0, 1e-5);
  ASSERT_GE(result.solutions.size(), 1);
  const GScipSolution expected({{x1, 0.0}, {x2, 1.0}});
  EXPECT_THAT(result.solutions.at(0), GScipSolutionAlmostEquals(expected));
}

TEST(ConstraintHandlerResultPriorityTest, NoCrash) {
  EXPECT_EQ(
      ConstraintHandlerResultPriority(GScipCallbackResult::kUnbounded,
                                      ConstraintHandlerCallbackType::kSepaSol),
      14);
  EXPECT_EQ(ConstraintHandlerResultPriority(
                GScipCallbackResult::kDelayNode,
                ConstraintHandlerCallbackType::kConsCheck),
            0);
}

TEST(MergeConstraintHandlerResults, NoCrash) {
  EXPECT_EQ(
      MergeConstraintHandlerResults(GScipCallbackResult::kUnbounded,
                                    GScipCallbackResult::kDelayNode,
                                    ConstraintHandlerCallbackType::kSepaSol),
      GScipCallbackResult::kUnbounded);
  EXPECT_EQ(MergeConstraintHandlerResults(
                GScipCallbackResult::kBranched, GScipCallbackResult::kSolveLp,
                ConstraintHandlerCallbackType::kEnfoLp),
            GScipCallbackResult::kSolveLp);
  EXPECT_EQ(MergeConstraintHandlerResults(
                GScipCallbackResult::kBranched, GScipCallbackResult::kSolveLp,
                ConstraintHandlerCallbackType::kSepaSol),
            GScipCallbackResult::kBranched);
}

}  // namespace
}  // namespace operations_research
