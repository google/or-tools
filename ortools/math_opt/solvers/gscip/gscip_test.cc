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

// TODO(user): The following methods are insufficiently tested:
//  * GScip::SetBranchingPriority(), just a no crash test, but it is tested by
//    the MathOpt integration tests.
//  * Setting options while solving.
//  * Control of SCIP logs. Write a main function and test with gbash, or use a
//    custom message handler (which has other advantages...)
//  * Advanced options for adding variables and constraints. Test by using them
//    e.g. with cut callbacks/column generation.
#include "ortools/math_opt/solvers/gscip/gscip.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <thread>  // NOLINT(build/c++11)
#include <utility>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/helpers.h"
#include "ortools/base/logging.h"
#include "ortools/base/options.h"
#include "ortools/base/temp_file.h"
#include "ortools/math_opt/solvers/gscip/gscip.pb.h"
#include "ortools/math_opt/solvers/gscip/gscip_parameters.h"
#include "ortools/math_opt/solvers/gscip/gscip_testing.h"
#include "scip/def.h"
#include "scip/type_cons.h"
#include "scip/type_var.h"

namespace operations_research {
namespace {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Mock;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;
constexpr auto CaptureTestStdout = testing::internal::CaptureStdout;
constexpr auto GetCapturedTestStdout = testing::internal::GetCapturedStdout;

constexpr double kInf = std::numeric_limits<double>::infinity();

TEST(GScipTest, ConstructDestruct) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
}

TEST(GScipTest, VersionString) {
  EXPECT_THAT(GScip::ScipVersion(), ::testing::StartsWith("SCIP"));
}

// min 3.0 * x
// s.t. x in [-2.0, 4.0]
TEST(GScipTest, CreateAndSolveOneVariableDefaultMinimize) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  SCIP_VAR* x =
      gscip->AddVariable(-2.0, 4.0, 3.0, GScipVarType::kContinuous, "x")
          .value();
  const GScipResult result = gscip->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(result, -6.0, {{x, -2.0}});
}

// max 3*x + 8
// s.t. 0 <= x <= 2
// x in [0, 4]
//
// x* = 2, obj* = 14
TEST(GScipTest, SimpleModelCreateAndSolveContinuousMax) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK(gscip->SetObjectiveOffset(8.0));
  SCIP_VAR* x =
      gscip->AddVariable(0.0, 4.0, 3.0, GScipVarType::kContinuous, "x").value();
  GScipLinearRange range;
  range.lower_bound = 0.0;
  range.upper_bound = 2.0;
  range.variables.push_back(x);
  range.coefficients.push_back(1.0);

  SCIP_CONS* cons = gscip->AddLinearConstraint(range, "x_bound").value();
  ASSERT_OK(gscip->SetMaximize(true));
  const GScipResult result = gscip->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(result, 14.0, {{x, 2.0}});
  EXPECT_THAT(gscip->constraints(), UnorderedElementsAre(cons));
}

// min 3*x + 2y
// s.t. 1 <= x + y <= 3
//    x, y in {0,1}
//
// x* = 0, y* = 1, obj* = 2
TEST(GScipTest, SimpleCreateModelAndSolveIntegerMinNoName) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  SCIP_VAR* x =
      gscip->AddVariable(0.0, 1.0, 3.0, GScipVarType::kInteger).value();
  SCIP_VAR* y =
      gscip->AddVariable(0.0, 1.0, 2.0, GScipVarType::kInteger).value();
  GScipLinearRange range;
  range.lower_bound = 1.0;
  range.upper_bound = 3.0;
  range.variables = {x, y};
  range.coefficients = {1.0, 1.0};
  ASSERT_OK(gscip->AddLinearConstraint(range).status());
  ASSERT_OK(gscip->SetMaximize(false));
  const GScipResult result = gscip->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(result, 2.0, {{x, 0.0}, {y, 1.0}});
}

TEST(GscipTest, Solve_TimeLimitZero_NoSolutionFound) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK(gscip->AddVariable(0.0, 1.0, 3.0, GScipVarType::kInteger));
  ASSERT_OK(gscip->SetMaximize(true));
  GScipParameters params = TestGScipParameters();
  GScipSetTimeLimit(absl::ZeroDuration(), &params);
  ASSERT_OK_AND_ASSIGN(const GScipResult result, gscip->Solve(params));
  EXPECT_EQ(result.gscip_output.status(), GScipOutput::TIME_LIMIT);
  EXPECT_THAT(result.solutions, IsEmpty());
}

// max 3*x + 2*y - 5
// s.t. -inf <= 10x + 11y <= 12
//      1 <= x + y <= inf
//      1.1 <= 3x + y <= 4
//      1.0 <= 10x <= 1.0
//      x in [-2, 2]
//      y in {0, 1}
//
// The problem has solution x* = 0.1, y* = 1, obj* = -2.7
TEST(GScipTest, ModelQuery) {
  // Build the model
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK(gscip->SetMaximize(true));
  ASSERT_OK(gscip->SetObjectiveOffset(-5.0));
  SCIP_VAR* x =
      gscip->AddVariable(-2.0, 2.0, 3.0, GScipVarType::kContinuous, "x")
          .value();
  SCIP_VAR* y =
      gscip->AddVariable(0.0, 1.0, 2.0, GScipVarType::kInteger).value();
  SCIP_CONS* c1;
  {
    GScipLinearRange r1;
    r1.upper_bound = 12.0;
    r1.variables = {x, y};
    r1.coefficients = {10.0, 11.0};
    c1 = gscip->AddLinearConstraint(r1, "c1").value();
  }
  SCIP_CONS* c2;
  {
    GScipLinearRange r2;
    r2.lower_bound = 1.0;
    r2.variables = {x, y};
    r2.coefficients = {1.0, 1.0};
    c2 = gscip->AddLinearConstraint(r2).value();
  }
  SCIP_CONS* c3;
  {
    GScipLinearRange r3;
    r3.lower_bound = 1.1;
    r3.upper_bound = 4.0;
    r3.variables = {x, y};
    r3.coefficients = {3.0, 1.0};
    c3 = gscip->AddLinearConstraint(r3).value();
  }
  SCIP_CONS* c4;
  {
    GScipLinearRange r4;
    r4.lower_bound = 1.0;
    r4.upper_bound = 1.0;
    r4.variables = {x};
    r4.coefficients = {10.0};
    c4 = gscip->AddLinearConstraint(r4).value();
  }
  // Read the model back
  EXPECT_EQ(gscip->ObjectiveOffset(), -5.0);
  EXPECT_EQ(gscip->ObjectiveIsMaximize(), true);
  EXPECT_EQ(gscip->ObjCoef(x), 3.0);
  EXPECT_EQ(gscip->ObjCoef(y), 2.0);

  EXPECT_EQ(gscip->Lb(x), -2.0);
  EXPECT_EQ(gscip->Ub(x), 2.0);
  EXPECT_EQ(gscip->VarType(x), GScipVarType::kContinuous);
  EXPECT_THAT(gscip->Name(x), StrEq("x"));

  EXPECT_EQ(gscip->Lb(y), 0.0);
  EXPECT_EQ(gscip->Ub(y), 1.0);
  EXPECT_EQ(gscip->VarType(y), GScipVarType::kBinary);
  EXPECT_THAT(gscip->Name(y), StrEq(""));

  EXPECT_LE(gscip->LinearConstraintLb(c1), -kInf);
  EXPECT_EQ(gscip->LinearConstraintUb(c1), 12.0);
  EXPECT_THAT(gscip->LinearConstraintCoefficients(c1), ElementsAre(10.0, 11.0));
  EXPECT_THAT(gscip->LinearConstraintVariables(c1), ElementsAre(x, y));
  EXPECT_THAT(gscip->Name(c1), StrEq("c1"));
  EXPECT_THAT(gscip->ConstraintType(c1), StrEq("linear"));
  EXPECT_TRUE(gscip->IsConstraintLinear(c1));

  EXPECT_EQ(gscip->LinearConstraintLb(c2), 1.0);
  EXPECT_GE(gscip->LinearConstraintUb(c2), kInf);
  EXPECT_THAT(gscip->LinearConstraintCoefficients(c2), ElementsAre(1.0, 1.0));
  EXPECT_THAT(gscip->LinearConstraintVariables(c2), ElementsAre(x, y));
  EXPECT_THAT(gscip->Name(c2), StrEq(""));
  EXPECT_THAT(gscip->ConstraintType(c2), StrEq("linear"));
  EXPECT_TRUE(gscip->IsConstraintLinear(c2));

  EXPECT_LE(gscip->LinearConstraintLb(c3), 1.1);
  EXPECT_EQ(gscip->LinearConstraintUb(c3), 4.0);
  EXPECT_THAT(gscip->LinearConstraintCoefficients(c3), ElementsAre(3.0, 1.0));
  EXPECT_THAT(gscip->LinearConstraintVariables(c3), ElementsAre(x, y));
  EXPECT_THAT(gscip->Name(c3), StrEq(""));
  EXPECT_THAT(gscip->ConstraintType(c3), StrEq("linear"));
  EXPECT_TRUE(gscip->IsConstraintLinear(c3));

  EXPECT_EQ(gscip->LinearConstraintLb(c4), 1.0);
  EXPECT_EQ(gscip->LinearConstraintUb(c4), 1.0);
  EXPECT_THAT(gscip->LinearConstraintCoefficients(c4), ElementsAre(10.0));
  EXPECT_THAT(gscip->LinearConstraintVariables(c4), ElementsAre(x));
  EXPECT_THAT(gscip->Name(c4), StrEq(""));
  EXPECT_THAT(gscip->ConstraintType(c4), StrEq("linear"));
  EXPECT_TRUE(gscip->IsConstraintLinear(c4));

  EXPECT_THAT(gscip->variables(), UnorderedElementsAre(x, y));
  EXPECT_THAT(gscip->constraints(), UnorderedElementsAre(c1, c2, c3, c4));

  // Solve the model
  const GScipResult result = gscip->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(result, -2.7, {{x, 0.1}, {y, 1.0}});
}

// max x + y - z
// s.t. 1 >= x - y >= -1
//        x - y + z >= 1
//        z in {0, 1}
//
// Primal ray (1, 1, 0) plus the solution (0, 0, 1) leads to unboundedness.
TEST(GScipTest, Unbounded) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK(gscip->SetMaximize(true));
  SCIP_VAR* x =
      gscip->AddVariable(-kInf, kInf, 1.0, GScipVarType::kContinuous, "x")
          .value();
  SCIP_VAR* y =
      gscip->AddVariable(-kInf, kInf, 1.0, GScipVarType::kContinuous, "y")
          .value();
  SCIP_VAR* z =
      gscip->AddVariable(0.0, 1.0, -1.0, GScipVarType::kInteger, "z").value();
  {
    GScipLinearRange r1;
    r1.upper_bound = 1.0;
    r1.lower_bound = -1.0;
    r1.variables = {x, y};
    r1.coefficients = {1.0, -1.0};
    EXPECT_OK(gscip->AddLinearConstraint(r1).status());
  }
  {
    GScipLinearRange r2;
    r2.upper_bound = kInf;
    r2.lower_bound = 1.0;
    r2.variables = {x, y, z};
    r2.coefficients = {1.0, -1.0, 1.0};
    EXPECT_OK(gscip->AddLinearConstraint(r2).status());
  }
  const GScipResult result = gscip->Solve(TestGScipParameters()).value();
  EXPECT_EQ(result.gscip_output.status(), GScipOutput::UNBOUNDED);
  EXPECT_EQ(result.gscip_output.stats().best_objective(), kInf);
  // TODO(b/149858911): There used to be a bug in SCIP 6 (it reported optimal
  // instead of unbounded). The bug seems to be fixed in SCIP 7, but now the
  // primal ray is not accessible anymore. Followup with SCIP.
  if (/* DISABLES CODE */ false) {
    ASSERT_EQ(result.primal_ray.size(), 3);
    EXPECT_NEAR(result.primal_ray.at(x), result.primal_ray.at(y), 1e-5);
    EXPECT_GT(result.primal_ray.at(x), 0.0);
    EXPECT_NEAR(result.primal_ray.at(z), 0.0, 1e-5);
  }
}

// max x + y
// s.t. x + y <= 1.5
//      2*x + y >= 2.5
//        x, y in {0, 1}
//
// The problem is LP feasible (1.0, 0.5), but MIP infeasible.
TEST(GScipTest, Infeasible) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK(gscip->SetMaximize(true));
  SCIP_VAR* x =
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "x").value();
  SCIP_VAR* y =
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "y").value();
  {
    GScipLinearRange r1;
    r1.upper_bound = 1.5;
    r1.variables = {x, y};
    r1.coefficients = {1.0, 1.0};
    ASSERT_OK(gscip->AddLinearConstraint(r1).status());
  }
  {
    GScipLinearRange r2;
    r2.lower_bound = 2.5;
    r2.variables = {x, y};
    r2.coefficients = {2.0, 1.0};
    ASSERT_OK(gscip->AddLinearConstraint(r2).status());
  }
  const GScipResult result = gscip->Solve(TestGScipParameters()).value();
  EXPECT_EQ(result.gscip_output.status(), GScipOutput::INFEASIBLE);
  EXPECT_THAT(result.primal_ray, IsEmpty());
  EXPECT_THAT(result.solutions, IsEmpty());
  EXPECT_THAT(result.objective_values, IsEmpty());
}

// max 3 x1 + 5x2 + 2x3
// s.t. x1 + x2 + x3 <= 1.5
// x1 in {0,1}
// x2, x3 in [0, 1]
//
// x* = (0, 1, 0.5), obj* = 6
class SimpleMIPTest : public ::testing::Test {
 public:
  SimpleMIPTest() {
    gscip_ = std::move(GScip::Create("scip_test")).value();
    CHECK_OK(gscip_->SetMaximize(true));
    x1_ = gscip_->AddVariable(0.0, 1.0, 3.0, GScipVarType::kInteger, "x1")
              .value();
    x2_ = gscip_->AddVariable(0.0, 1.0, 5.0, GScipVarType::kContinuous, "x2")
              .value();
    x3_ = gscip_->AddVariable(0.0, 1.0, 2.0, GScipVarType::kContinuous, "x3")
              .value();
    GScipLinearRange r;
    r.upper_bound = 1.5;
    r.coefficients = {1.0, 1.0, 1.0};
    r.variables = {x1_, x2_, x3_};
    constraint_ = gscip_->AddLinearConstraint(r, "c").value();
  }

 protected:
  std::unique_ptr<GScip> gscip_;
  SCIP_VAR* x1_;
  SCIP_VAR* x2_;
  SCIP_VAR* x3_;
  SCIP_CONS* constraint_;
};

TEST_F(SimpleMIPTest, BaseSolve) {
  const GScipResult result = gscip_->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(result, 6.0,
                                {{x1_, 0.0}, {x2_, 1.0}, {x3_, 0.5}});
}

// max 3 x1 + 5x2 + 2x3 - 4
// s.t. x1 + x2 + x3 <= 1.5
// x1 in {0,1}
// x2, x3 in [0, 1]
//
// x* = (0, 1, 0.5), obj* = 2
TEST_F(SimpleMIPTest, ModifyOffset) {
  ASSERT_OK(gscip_->Solve(TestGScipParameters()).status());
  ASSERT_OK(gscip_->SetObjectiveOffset(-4.0));
  const GScipResult modified_result =
      gscip_->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(modified_result, 2.0,
                                {{x1_, 0.0}, {x2_, 1.0}, {x3_, 0.5}});
}

// min 3 x1 + 5x2 + 2x3
// s.t. x1 + x2 + x3 <= 1.5
// x1 in {0,1}
// x2, x3 in [0, 1]
//
// x* = (0, 0, 0), obj* = 0
TEST_F(SimpleMIPTest, ModifyObjectiveDirection) {
  ASSERT_OK(gscip_->Solve(TestGScipParameters()).status());
  ASSERT_OK(gscip_->SetMaximize(false));
  const GScipResult modified_result =
      gscip_->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(modified_result, 0.0,
                                {{x1_, 0.0}, {x2_, 0.0}, {x3_, 0.0}});
}

// max 4.5 x1 + 5x2 + 2x3
// s.t. x1 + x2 + x3 <= 1.5
// x1 in {0,1}
// x2, x3 in [0, 1]
//
// x* = (1, 0.5, 0), obj* = 7
TEST_F(SimpleMIPTest, ModifyObjective) {
  ASSERT_OK(gscip_->Solve(TestGScipParameters()).status());
  ASSERT_OK(gscip_->SetObjCoef(x1_, 4.5));
  const GScipResult modified_result =
      gscip_->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(modified_result, 7.0,
                                {{x1_, 1.0}, {x2_, 0.5}, {x3_, 0.0}});
}

// max 3x1 + 5x2 + 2x3
// s.t. x1 + x2 + x3 <= 2.5
// x1 in {0,1}
// x2, x3 in [0, 1]
//
// x* = (1, 1, 0.5), obj* = 9
TEST_F(SimpleMIPTest, ModifyConstraintUpperBound) {
  ASSERT_OK(gscip_->Solve(TestGScipParameters()).status());
  ASSERT_OK(gscip_->SetLinearConstraintUb(constraint_, 2.5));
  const GScipResult modified_result =
      gscip_->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(modified_result, 9.0,
                                {{x1_, 1.0}, {x2_, 1.0}, {x3_, 0.5}});
}

// max 3x1 + 5x2 + 2x3
// s.t. 2 <= x1 + x2 + x3 <= 1.5
// x1 in {0,1}
// x2, x3 in [0, 1]
//
// Problem becomes infeasible.
TEST_F(SimpleMIPTest, ModifyConstraintLowerBound) {
  ASSERT_OK(gscip_->Solve(TestGScipParameters()).status());
  ASSERT_OK(gscip_->SetLinearConstraintLb(constraint_, 2.0));
  const GScipResult modified_result =
      gscip_->Solve(TestGScipParameters()).value();
  EXPECT_EQ(modified_result.gscip_output.status(), GScipOutput::INFEASIBLE);
}

// max 3x1 + 5x2 + 2x3
// s.t. x1 + x2 + x3 <= 1.5
// x1 in {0,1}
// x2 in [0, 2]
// x3 in [0, 1]
//
// x* = (0, 1.5, 0), obj* = 7.5
TEST_F(SimpleMIPTest, ModifyVarUb) {
  ASSERT_OK(gscip_->Solve(TestGScipParameters()).status());
  ASSERT_OK(gscip_->SetUb(x2_, 2.0));
  const GScipResult modified_result =
      gscip_->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(modified_result, 7.5,
                                {{x1_, 0.0}, {x2_, 1.5}, {x3_, 0.0}});
}

// max 3x1 + 5x2 + 2x3
// s.t. x1 + x2 + x3 <= 1.5
// x1 in {1}
// x2 in [0, 1]
// x3 in [0, 1]
//
// x* = (1, 0.5, 0), obj* = 5.5
TEST_F(SimpleMIPTest, ModifyVarLb) {
  ASSERT_OK(gscip_->Solve(TestGScipParameters()).status());
  ASSERT_OK(gscip_->SetLb(x1_, 1.0));
  const GScipResult modified_result =
      gscip_->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(modified_result, 5.5,
                                {{x1_, 1.0}, {x2_, 0.5}, {x3_, 0.0}});
}

// max 3x1 + 5x2 + 2x3
// s.t. x1 + x2 + x3 <= 1.5
// x1 in [0, 1]
// x2 in [0, 1]
// x3 in [0, 1]
//
// x* = (0.5, 1.0, 0), obj* = 6.5
TEST_F(SimpleMIPTest, SetVarType) {
  ASSERT_OK(gscip_->Solve(TestGScipParameters()).status());
  ASSERT_OK(gscip_->SetVarType(x1_, GScipVarType::kContinuous));
  const GScipResult modified_result =
      gscip_->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(modified_result, 6.5,
                                {{x1_, 0.5}, {x2_, 1.0}, {x3_, 0.0}});
}

// max 3x1 + 5x2 + 2x3
// s.t. x1 + 5*x2 + x3 <= 1.5
// x1 in {0, 1}
// x2 in [0, 1]
// x3 in [0, 1]
//
// x* = (1, 0.0, 0.5), obj* = 4
TEST_F(SimpleMIPTest, ModifyConstraintCoef) {
  ASSERT_OK(gscip_->Solve(TestGScipParameters()).status());
  ASSERT_OK(gscip_->SetLinearConstraintCoef(constraint_, x2_, 5.0));
  const GScipResult modified_result =
      gscip_->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(modified_result, 4.0,
                                {{x1_, 1.0}, {x2_, 0.0}, {x3_, 0.5}});
}

// max 3x1 + 5x2 + 2x3
// s.t. x1 + (1 - 0.75) * x2 + x3 <= 1.5
// x1 in {0, 1}
// x2 in [0, inf)
// x3 in [0, 1]
//
// x* = (0.0, 6.0, 0.0), obj* = 30
//
// Rationale:
//   x1 + (1 - 0.75) * x2 + x3 <= 1.5
//   x1 + 0.25 * x2 + x3 <= 1.5
//
//   x2 is the variable that has the most impact on the objective and has no
//   upper bound. Its growth is only limited by the constraint:
//
//   0.25 * x2 <= 1.5
//   x2 <= 6.0
TEST_F(SimpleMIPTest, AddConstraintCoef) {
  ASSERT_OK(gscip_->Solve(TestGScipParameters()).status());
  ASSERT_OK(gscip_->SetUb(x2_, kInf));
  ASSERT_OK(gscip_->AddLinearConstraintCoef(constraint_, x2_, -0.75));
  const GScipResult modified_result =
      gscip_->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(modified_result, 30.0,
                                {{x1_, 0.0}, {x2_, 6.0}, {x3_, 0}});
}

// max 3x1 + 5x2 + 2x3 + 4.5x4
// s.t. x1 + x2 + x3 + x4 <= 1.5
// x1, x4 in {0, 1}
// x2, x3 in [0, 1]
//
// x* = (0, 0.5, 0, 1), obj* = 7
TEST_F(SimpleMIPTest, AddVariable) {
  ASSERT_OK(gscip_->Solve(TestGScipParameters()).status());
  SCIP_VAR* x4 =
      gscip_->AddVariable(0.0, 1.0, 4.5, GScipVarType::kInteger, "x4").value();
  ASSERT_OK(gscip_->SetLinearConstraintCoef(constraint_, x4, 1.0));
  const GScipResult modified_result =
      gscip_->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(
      modified_result, 7.0, {{x1_, 0.0}, {x2_, 0.5}, {x3_, 0.0}, {x4, 1.0}});
}

// max 3x1 + 5x2 + 2x3
// s.t. x1 + x2 + x3 <= 1.5
//      x1 +      x3 >= 1
// x1 in {0, 1}
// x2, x3 in [0, 1]
//
// x* = (1, 0.5, 0), obj* = 4.5
TEST_F(SimpleMIPTest, AddConstraint) {
  ASSERT_OK(gscip_->Solve(TestGScipParameters()).status());
  GScipLinearRange r;
  r.variables = {x1_, x3_};
  r.coefficients = {1.0, 1.0};
  r.lower_bound = 1.0;
  ASSERT_OK(gscip_->AddLinearConstraint(r).status());
  const GScipResult modified_result =
      gscip_->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(modified_result, 5.5,
                                {{x1_, 1.0}, {x2_, 0.5}, {x3_, 0.0}});
}

// max 3 x1 + 2x3
// s.t. x1 + x3 <= 1.5
// x1 in {0,1}
// x3 in [0, 1]
//
// x1* = 1, x3* = 0.5 obj* = 4
TEST_F(SimpleMIPTest, DeleteVariableBeforeSolving) {
  ASSERT_OK(gscip_->SetLinearConstraintCoef(constraint_, x2_, 0.0));
  ASSERT_OK(gscip_->DeleteVariable(x2_));
  ASSERT_OK_AND_ASSIGN(const GScipResult result,
                       gscip_->Solve(TestGScipParameters()));
  AssertOptimalWithBestSolution(result, 4.0, {{x1_, 1.0}, {x3_, 0.5}});
}

// max 3 x1 + 2x3
// s.t. x1 + x3 <= 1.5
// x1 in {0,1}
// x3 in [0, 1]
//
// x1* = 1, x3* = 0.5 obj* = 4
TEST_F(SimpleMIPTest, DeleteVariableAfterSolving) {
  ASSERT_OK(gscip_->Solve(TestGScipParameters()).status());
  ASSERT_OK(gscip_->SetLinearConstraintCoef(constraint_, x2_, 0.0));
  ASSERT_OK(gscip_->DeleteVariable(x2_));
  ASSERT_OK_AND_ASSIGN(const GScipResult result,
                       gscip_->Solve(TestGScipParameters()));
  AssertOptimalWithBestSolution(result, 4.0, {{x1_, 1.0}, {x3_, 0.5}});
}

// max 2x3
// s.t. x3 <= 1.5
// x3 in [0, 1]
//
// x3* = 1.0 obj* = 2.0
TEST_F(SimpleMIPTest, SafeBulkDeleteVariableBeforeSolving) {
  ASSERT_OK(gscip_->CanSafeBulkDelete({x1_, x2_}));
  ASSERT_OK(gscip_->SafeBulkDelete({x1_, x2_}));
  ASSERT_OK_AND_ASSIGN(const GScipResult result,
                       gscip_->Solve(TestGScipParameters()));
  AssertOptimalWithBestSolution(result, 2.0, {{x3_, 1.0}});
}

// max 2x3
// s.t. x3 <= 1.5
// x3 in [0, 1]
//
// x3* = 1.0 obj* = 2.0
TEST_F(SimpleMIPTest, SafeBulkDeleteVariableAfterSolving) {
  ASSERT_OK(gscip_->Solve(TestGScipParameters()).status());
  ASSERT_OK(gscip_->CanSafeBulkDelete({x1_, x2_}));
  ASSERT_OK(gscip_->SafeBulkDelete({x1_, x2_}));
  ASSERT_OK_AND_ASSIGN(const GScipResult result,
                       gscip_->Solve(TestGScipParameters()));
  AssertOptimalWithBestSolution(result, 2.0, {{x3_, 1.0}});
}

// max 3x1 + 5x2 + 2x3
// x1 in {0,1}
// x2, x3 in [0, 1]
//
// x* = (1, 1, 1), obj* = 10
TEST_F(SimpleMIPTest, DeleteConstraintBeforeSolving) {
  ASSERT_OK(gscip_->DeleteConstraint(constraint_));
  ASSERT_OK_AND_ASSIGN(const GScipResult result,
                       gscip_->Solve(TestGScipParameters()));
  AssertOptimalWithBestSolution(result, 10.0,
                                {{x1_, 1.0}, {x2_, 1.0}, {x3_, 1.0}});
}

// max 3x1 + 5x2 + 2x3
// x1 in {0,1}
// x2, x3 in [0, 1]
//
// x* = (1, 1, 1), obj* = 10
TEST_F(SimpleMIPTest, DeleteConstraintAfterSolving) {
  ASSERT_OK(gscip_->Solve(TestGScipParameters()).status());
  ASSERT_OK(gscip_->DeleteConstraint(constraint_));
  ASSERT_OK_AND_ASSIGN(const GScipResult result,
                       gscip_->Solve(TestGScipParameters()));
  AssertOptimalWithBestSolution(result, 10.0,
                                {{x1_, 1.0}, {x2_, 1.0}, {x3_, 1.0}});
}

TEST_F(SimpleMIPTest, HintExact) {
  ASSERT_THAT(gscip_->SuggestHint({{x1_, 0.0}, {x2_, 1.0}, {x3_, 0.5}}),
              IsOkAndHolds(GScipHintResult::kAccepted));
  const GScipResult result = gscip_->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(result, 6.0,
                                {{x1_, 0.0}, {x2_, 1.0}, {x3_, 0.5}});
}

TEST_F(SimpleMIPTest, HintPartial) {
  ASSERT_THAT(gscip_->SuggestHint({{x2_, 1.0}, {x3_, 0.5}}),
              IsOkAndHolds(GScipHintResult::kAccepted));
  const GScipResult result = gscip_->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(result, 6.0,
                                {{x1_, 0.0}, {x2_, 1.0}, {x3_, 0.5}});
}

// TODO(user): not clear how to generate a rejected hint.

// This test results in a memory error, it is not clear why. Perhaps an issue
// with SCIP.
TEST_F(SimpleMIPTest, HintInfeasible) {
  ASSERT_THAT(gscip_->SuggestHint({{x1_, 1.0}, {x2_, 1.0}, {x3_, 0.5}}),
              IsOkAndHolds(GScipHintResult::kInfeasible));
  const GScipResult result = gscip_->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(result, 6.0,
                                {{x1_, 0.0}, {x2_, 1.0}, {x3_, 0.5}});
}

TEST_F(SimpleMIPTest, HintPartialAndInfeasible) {
  // Surprisingly, this hint is accepted.
  ASSERT_THAT(gscip_->SuggestHint({{x1_, 1.0}, {x2_, 1.0}}),
              IsOkAndHolds(GScipHintResult::kAccepted));
  const GScipResult result = gscip_->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(result, 6.0,
                                {{x1_, 0.0}, {x2_, 1.0}, {x3_, 0.5}});
}

// NOTE(user): MPSolver has a better test of branching priorities that looks
// at nodes visited.
TEST_F(SimpleMIPTest, BranchingPriorityNoCrash) {
  ASSERT_OK(gscip_->SetBranchingPriority(x1_, 1));
  const GScipResult result = gscip_->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(result, 6.0,
                                {{x1_, 0.0}, {x2_, 1.0}, {x3_, 0.5}});
}

TEST(GScipTest, DefaultParameterValues) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  // Default values were taken for SCIP 6.0.2 from documentation here:
  // https://scip.zib.de/doc/html/PARAMETERS.php
  // Parameter names and default values may change in future SCIP releases.
  EXPECT_THAT(gscip->DefaultBoolParamValue("branching/preferbinary"),
              IsOkAndHolds(false));
  EXPECT_THAT(gscip->DefaultLongParamValue("limits/nodes"),
              IsOkAndHolds(static_cast<int64_t>(-1)));
  EXPECT_THAT(gscip->DefaultCharParamValue("branching/scorefunc"),
              IsOkAndHolds('p'));
  EXPECT_THAT(gscip->DefaultIntParamValue("conflict/minmaxvars"),
              IsOkAndHolds(0));
  EXPECT_THAT(gscip->DefaultRealParamValue("branching/scorefac"),
              IsOkAndHolds(0.167));
  EXPECT_THAT(
      gscip->DefaultStringParamValue("heuristics/undercover/fixingalts"),
      IsOkAndHolds("li"));
}

// min x + 2y
// s.t. 1 <= x + y <= 1
//    x, y in {0,1}
//
// With hint (0, 1). The problem has two solutions, and the optimal is (1, 0),
// so we know that the solution pool will contain both at the end.
TEST(GScipTest, MultipleSolutionsAndHint) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  SCIP_VAR* x =
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "x").value();
  SCIP_VAR* y =
      gscip->AddVariable(0.0, 1.0, 2.0, GScipVarType::kInteger, "y").value();
  ASSERT_OK(gscip->SetMaximize(false));
  GScipLinearRange range;
  range.lower_bound = 1.0;
  range.upper_bound = 1.0;
  range.variables = {x, y};
  range.coefficients = {1.0, 1.0};
  ASSERT_OK(gscip->AddLinearConstraint(range).status());
  GScipParameters parameters = TestGScipParameters();
  parameters.set_num_solutions(5);  // only 2 exist, should produce 2.
  ASSERT_THAT(gscip->SuggestHint({{x, 0.0}, {y, 1.0}}),
              IsOkAndHolds(GScipHintResult::kAccepted));
  const GScipResult result = gscip->Solve(parameters).value();
  ASSERT_EQ(result.gscip_output.status(), GScipOutput::OPTIMAL);
  ASSERT_EQ(result.solutions.size(), 2);
  EXPECT_THAT(result.objective_values, ::testing::ElementsAre(1.0, 2.0));
  EXPECT_NEAR(result.gscip_output.stats().best_objective(), 1.0, 1e-5);
  EXPECT_NEAR(result.gscip_output.stats().best_bound(), 1.0, 1e-5);
  EXPECT_THAT(result.solutions[0],
              GScipSolutionAlmostEquals(GScipSolution({{x, 1.0}, {y, 0.0}})));
  EXPECT_THAT(result.solutions[1],
              GScipSolutionAlmostEquals(GScipSolution({{x, 0.0}, {y, 1.0}})));
}

// Like above, but now only request one solution.
TEST(GScipTest, MultipleSolutionsRequestOne) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  SCIP_VAR* x =
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "x").value();
  SCIP_VAR* y =
      gscip->AddVariable(0.0, 1.0, 2.0, GScipVarType::kInteger, "y").value();
  ASSERT_OK(gscip->SetMaximize(false));
  GScipLinearRange range;
  range.lower_bound = 1.0;
  range.upper_bound = 1.0;
  range.variables = {x, y};
  range.coefficients = {1.0, 1.0};
  ASSERT_OK(gscip->AddLinearConstraint(range).status());
  GScipParameters parameters = TestGScipParameters();
  parameters.set_num_solutions(1);  // only 2 exist, should produce 1.
  ASSERT_THAT(gscip->SuggestHint({{x, 0.0}, {y, 1.0}}),
              IsOkAndHolds(GScipHintResult::kAccepted));
  const GScipResult result = gscip->Solve(parameters).value();
  ASSERT_EQ(result.gscip_output.status(), GScipOutput::OPTIMAL);
  ASSERT_EQ(result.solutions.size(), 1);
  EXPECT_THAT(result.objective_values, ::testing::ElementsAre(1.0));
  EXPECT_NEAR(result.gscip_output.stats().best_objective(), 1.0, 1e-5);
  EXPECT_NEAR(result.gscip_output.stats().best_bound(), 1.0, 1e-5);
  EXPECT_THAT(result.solutions[0],
              GScipSolutionAlmostEquals(GScipSolution({{x, 1.0}, {y, 0.0}})));
}

// When presolve, cuts and heuristics are disabled, this problem will require
// branching, as the LP relaxation is
//
// max 3x1 + 5x2 + 2x3
// s.t. x1 + x2 + x3 <= 1.5
// x1 in {0, 1}
// x2, x3 in [0, 1]
//
// x* = (0.5, 1.0, 0), obj* = 6.5
TEST_F(SimpleMIPTest, OutputStats) {
  GScipParameters params = TestGScipParameters();
  params.set_presolve(GScipParameters::OFF);
  params.set_heuristics(GScipParameters::OFF);
  params.set_separating(GScipParameters::OFF);
  GScipSetOutputEnabled(&params, true);
  const GScipResult result = gscip_->Solve(params).value();
  AssertOptimalWithBestSolution(result, 6.0,
                                {{x1_, 0.0}, {x2_, 1.0}, {x3_, 0.5}});

  const GScipSolvingStats& stats = result.gscip_output.stats();
  // We are solving with SoPlex or GLOP as the LP solver, which do not implement
  // the barrier algorithm.
  EXPECT_EQ(stats.barrier_iterations(), 0);
  EXPECT_GE(stats.node_count(), 1);
  EXPECT_GE(stats.primal_simplex_iterations() + stats.dual_simplex_iterations(),
            1);
  EXPECT_GE(stats.total_lp_iterations(), 1);
  // See docs for total_lp_iterations.
  EXPECT_GE(stats.total_lp_iterations(), stats.primal_simplex_iterations() +
                                             stats.dual_simplex_iterations());
  EXPECT_NEAR(stats.first_lp_relaxation_bound(), 6.5, 1e-5);
  // Even with everything disabled, the root node bound is better than the
  // first lp. This remains to be explained. For now, we assert that it falls
  // between the first LP bound and the final bound.
  EXPECT_LE(stats.root_node_bound(), stats.first_lp_relaxation_bound());
  EXPECT_GE(stats.root_node_bound(), stats.best_bound());

  EXPECT_GT(stats.deterministic_time(), 0.0);
}

TEST_F(SimpleMIPTest, HitNodeLimit) {
  GScipParameters params = TestGScipParameters();
  params.set_presolve(GScipParameters::OFF);
  params.set_heuristics(GScipParameters::OFF);
  params.set_separating(GScipParameters::OFF);
  GScipSetOutputEnabled(&params, true);
  (*params.mutable_long_params())["limits/totalnodes"] = 1L;
  const GScipResult result = gscip_->Solve(params).value();
  EXPECT_EQ(result.gscip_output.status(), GScipOutput::TOTAL_NODE_LIMIT);
  EXPECT_NEAR(result.gscip_output.stats().best_bound(), 6.5, 1e-5);
  EXPECT_EQ(result.gscip_output.stats().best_objective(), -kInf);
  EXPECT_EQ(result.gscip_output.stats().node_count(), 1);
  EXPECT_THAT(result.solutions, IsEmpty());
}

TEST(GScipTest, BadConstraint) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  SCIP_VAR* x =
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "x").value();
  GScipLinearRange range;
  range.lower_bound = 1.0;
  range.upper_bound = 1.0;
  range.variables = {x};
  range.coefficients = {1.0, 1.0};
  EXPECT_THAT(gscip->AddLinearConstraint(range, "c1"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Error adding constraint: c1")));
}

TEST(GScipTest, SolveWithInterrupterUninterrupted) {
  // max x, s.t. x binary
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR * x,
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "x"));
  ASSERT_OK(gscip->SetMaximize(true));

  GScip::Interrupter interrupter;
  ASSERT_OK_AND_ASSIGN(
      const GScipResult result,
      gscip->Solve(TestGScipParameters(), nullptr, &interrupter));
  AssertOptimalWithBestSolution(result, 1.0, {{x, 1.0}});
}

TEST(GScipTest, SolveWithInterrupterInterruptedAtStart) {
  // max x, s.t. x binary
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK(gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "x"));
  ASSERT_OK(gscip->SetMaximize(true));

  GScip::Interrupter interrupter;
  interrupter.Interrupt();
  ASSERT_OK_AND_ASSIGN(
      const GScipResult result,
      gscip->Solve(TestGScipParameters(), nullptr, &interrupter));
  ASSERT_EQ(result.gscip_output.status(), GScipOutput::USER_INTERRUPT);
  EXPECT_THAT(result.solutions, IsEmpty());
}

TEST(GScipTest, SolveWithInterrupterInterruptedMidSolve) {
  // max x, s.t. x binary
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK(gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "x"));
  ASSERT_OK(gscip->SetMaximize(true));

  GScip::Interrupter interrupter;
  absl::Notification interrupt_is_triggered;
  bool msg_cb_called = false;

  auto message_cb = [&interrupt_is_triggered, &msg_cb_called](
                        GScipMessageType /*type*/, absl::string_view message) {
    interrupt_is_triggered.WaitForNotification();
    msg_cb_called = true;
    LOG(INFO) << "msg_cb: " << message;
  };
  std::thread interrupt_thread([&interrupter, &interrupt_is_triggered]() {
    absl::SleepFor(absl::Seconds(1));
    interrupter.Interrupt();
    interrupt_is_triggered.Notify();
  });
  const absl::Cleanup interrupt_thread_join([&]() { interrupt_thread.join(); });

  // Weaken the solver a bit so it doesn't solve right away.
  GScipParameters params;
  params.set_heuristics(GScipParameters::OFF);
  params.set_presolve(GScipParameters::OFF);

  ASSERT_OK_AND_ASSIGN(const GScipResult result,
                       gscip->Solve(params, message_cb, &interrupter));
  ASSERT_TRUE(msg_cb_called);
  ASSERT_EQ(result.gscip_output.status(), GScipOutput::USER_INTERRUPT);
}

// /////////////////////////////////////////////////////////////////////////////
// Test nonlinear constraints
// /////////////////////////////////////////////////////////////////////////////

TEST(GScipTest, SimpleSOS1Test) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK(gscip->SetMaximize(true));
  SCIP_VAR* x1 =
      gscip->AddVariable(0.0, 1.0, 3.0, GScipVarType::kInteger, "x1").value();
  SCIP_VAR* x2 =
      gscip->AddVariable(0.0, 1.0, 5.0, GScipVarType::kContinuous, "x2")
          .value();
  SCIP_VAR* x3 =
      gscip->AddVariable(0.0, 1.0, 2.0, GScipVarType::kContinuous, "x3")
          .value();
  GScipSOSData sos;
  sos.variables = {x1, x2, x3};
  SCIP_CONS* cons = gscip->AddSOS1Constraint(sos).value();
  const GScipResult result = gscip->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(result, 5.0, {{x1, 0.0}, {x2, 1.0}, {x3, 0.0}});
  EXPECT_THAT(gscip->constraints(), UnorderedElementsAre(cons));
}

TEST(GScipTest, SOS1TestWithWeights) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK(gscip->SetMaximize(true));
  SCIP_VAR* x1 =
      gscip->AddVariable(0.0, 1.0, 3.0, GScipVarType::kInteger, "x1").value();
  SCIP_VAR* x2 =
      gscip->AddVariable(0.0, 1.0, 5.0, GScipVarType::kContinuous, "x2")
          .value();
  SCIP_VAR* x3 =
      gscip->AddVariable(0.0, 1.0, 2.0, GScipVarType::kContinuous, "x3")
          .value();
  GScipSOSData sos;
  sos.variables = {x1, x2, x3};
  sos.weights = {2.0, 4.0, 3.5};
  ASSERT_OK(gscip->AddSOS1Constraint(sos).status());
  const GScipResult result = gscip->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(result, 5.0, {{x1, 0.0}, {x2, 1.0}, {x3, 0.0}});
}

TEST(GScipTest, SimpleSOS2Test) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK(gscip->SetMaximize(true));
  SCIP_VAR* x1 =
      gscip->AddVariable(0.0, 1.0, 8.0, GScipVarType::kInteger, "x1").value();
  SCIP_VAR* x2 =
      gscip->AddVariable(0.0, 1.0, 5.0, GScipVarType::kContinuous, "x2")
          .value();
  SCIP_VAR* x3 =
      gscip->AddVariable(0.0, 1.0, 2.0, GScipVarType::kContinuous, "x3")
          .value();
  SCIP_VAR* x4 =
      gscip->AddVariable(0.0, 1.0, 9.0, GScipVarType::kContinuous, "x4")
          .value();
  GScipSOSData sos;
  sos.variables = {x1, x2, x3, x4};
  SCIP_CONS* cons = gscip->AddSOS2Constraint(sos).value();
  const GScipResult result = gscip->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(result, 13.0,
                                {{x1, 1.0}, {x2, 1.0}, {x3, 0.0}, {x4, 0.0}});
  EXPECT_THAT(gscip->constraints(), UnorderedElementsAre(cons));
}

TEST(GScipTest, SOS2TestWithWeights) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK(gscip->SetMaximize(true));
  SCIP_VAR* x1 =
      gscip->AddVariable(0.0, 1.0, 8.0, GScipVarType::kInteger, "x1").value();
  SCIP_VAR* x2 =
      gscip->AddVariable(0.0, 1.0, 5.0, GScipVarType::kContinuous, "x2")
          .value();
  SCIP_VAR* x3 =
      gscip->AddVariable(0.0, 1.0, 2.0, GScipVarType::kContinuous, "x3")
          .value();
  SCIP_VAR* x4 =
      gscip->AddVariable(0.0, 1.0, 9.0, GScipVarType::kContinuous, "x4")
          .value();
  GScipSOSData sos;
  sos.variables = {x1, x2, x3, x4};
  sos.weights = {1.0, 4.0, 3.0, 2.0};
  ASSERT_OK(gscip->AddSOS2Constraint(sos).status());
  const GScipResult result = gscip->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(result, 17.0,
                                {{x1, 1.0}, {x2, 0.0}, {x3, 0.0}, {x4, 1.0}});
}

// We want to minimize y = 2x^2 - 8x + 3
//   First order conditions: dy/dx = 4x - 8
// Solve for zero, x = 2, y = -5
//
// SCIP reformulation:
//
// minimize y
// -3.0 >= 2x^2 - 8x - y >= -inf
// -20 <= x <= 20
TEST(GScipTest, SimpleQuadratic) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK(gscip->SetMaximize(false));
  SCIP_VAR* x =
      gscip->AddVariable(-20.0, 20.0, 0.0, GScipVarType::kContinuous, "x")
          .value();
  SCIP_VAR* y =
      gscip->AddVariable(-kInf, kInf, 1.0, GScipVarType::kContinuous, "y")
          .value();
  GScipQuadraticRange quad;
  quad.upper_bound = -3.0;
  quad.lower_bound = -kInf;
  quad.linear_coefficients = {-8.0, -1.0};
  quad.linear_variables = {x, y};
  quad.quadratic_coefficients = {2.0};
  quad.quadratic_variables1 = {x};
  quad.quadratic_variables2 = {x};
  SCIP_CONS* cons = gscip->AddQuadraticConstraint(quad).value();
  auto params = TestGScipParameters();
  GScipSetOutputEnabled(&params, true);
  const GScipResult result = gscip->Solve(params).value();
  AssertOptimalWithBestSolution(result, -5.0, {{x, 2.0}, {y, -5.0}}, 1e-3);
  EXPECT_THAT(gscip->constraints(), UnorderedElementsAre(cons));
  EXPECT_THAT(gscip->ConstraintType(cons), StrEq("quadratic"));
  EXPECT_FALSE(gscip->IsConstraintLinear(cons));
}

// max 2*z + x - y
// z = AND(x, y)
// x, y, z binary
//
// Solution = (1,1,1), objective = 2.0
TEST(GScipTest, SimpleAnd) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK(gscip->SetMaximize(true));
  SCIP_VAR* x =
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "x").value();
  SCIP_VAR* y =
      gscip->AddVariable(0.0, 1.0, -1.0, GScipVarType::kInteger, "y").value();
  SCIP_VAR* z =
      gscip->AddVariable(0.0, 1.0, 2.0, GScipVarType::kInteger, "z").value();
  GScipLogicalConstraintData and_cons;
  and_cons.resultant = z;
  and_cons.operators = {x, y};
  SCIP_CONS* cons = gscip->AddAndConstraint(and_cons).value();
  auto params = TestGScipParameters();
  GScipSetOutputEnabled(&params, true);
  const GScipResult result = gscip->Solve(params).value();
  AssertOptimalWithBestSolution(result, 2.0, {{x, 1.0}, {y, 1.0}, {z, 1.0}});
  EXPECT_THAT(gscip->constraints(), UnorderedElementsAre(cons));
}

// max 2*z + x - y
// z = OR(x, y)
// x, y, z binary
//
// Solution = (1,0,1), objective = 3.0
TEST(GScipTest, SimpleOr) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK(gscip->SetMaximize(true));
  SCIP_VAR* x =
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "x").value();
  SCIP_VAR* y =
      gscip->AddVariable(0.0, 1.0, -1.0, GScipVarType::kInteger, "y").value();
  SCIP_VAR* z =
      gscip->AddVariable(0.0, 1.0, 2.0, GScipVarType::kInteger, "z").value();
  GScipLogicalConstraintData or_cons;
  or_cons.resultant = z;
  or_cons.operators = {x, y};
  SCIP_CONS* cons = gscip->AddOrConstraint(or_cons).value();
  auto params = TestGScipParameters();
  GScipSetOutputEnabled(&params, true);
  const GScipResult result = gscip->Solve(params).value();
  AssertOptimalWithBestSolution(result, 3.0, {{x, 1.0}, {y, 0.0}, {z, 1.0}});
  EXPECT_THAT(gscip->constraints(), UnorderedElementsAre(cons));
}

// max x + y + 2*z
//     x == y
//     if z then x + y <= 3
//     z binary
//     x, y in [0, 2]
//
// Solution = (1.5, 1.5, 1), objective = 5.0
TEST(GScipTest, SimpleIndicator) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK(gscip->SetMaximize(true));
  SCIP_VAR* x =
      gscip->AddVariable(0.0, 2.0, 1.0, GScipVarType::kContinuous, "x").value();
  SCIP_VAR* y =
      gscip->AddVariable(0.0, 2.0, 1.0, GScipVarType::kContinuous, "y").value();
  SCIP_VAR* z =
      gscip->AddVariable(0.0, 1.0, 2.0, GScipVarType::kBinary, "z").value();
  GScipIndicatorConstraint ind_cons;
  ind_cons.upper_bound = 3.0;
  ind_cons.variables = {x, y};
  ind_cons.coefficients = {1.0, 1.0};
  ind_cons.indicator_variable = z;
  SCIP_CONS* cons1 = gscip->AddIndicatorConstraint(ind_cons).value();

  GScipLinearRange range;
  range.upper_bound = 0.0;
  range.lower_bound = 0.0;
  range.variables = {x, y};
  range.coefficients = {1.0, -1.0};
  SCIP_CONS* cons2 = gscip->AddLinearConstraint(range).value();

  auto params = TestGScipParameters();
  GScipSetOutputEnabled(&params, true);
  const GScipResult result = gscip->Solve(params).value();
  AssertOptimalWithBestSolution(result, 5.0, {{x, 1.5}, {y, 1.5}, {z, 1.0}});
  EXPECT_THAT(gscip->constraints(), UnorderedElementsAre(cons1, cons2));
}

// max x + y - 2*z
//       x == y
// if not(z) then x + y <= 3
//
// z binary
// x, y in [0, 2]
//
// Solution = (1.5, 1.5, 0), objective = 3.0
TEST(GScipTest, NegatedIndicator) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK(gscip->SetMaximize(true));
  SCIP_VAR* x =
      gscip->AddVariable(0.0, 2.0, 1.0, GScipVarType::kContinuous, "x").value();
  SCIP_VAR* y =
      gscip->AddVariable(0.0, 2.0, 1.0, GScipVarType::kContinuous, "y").value();
  SCIP_VAR* z =
      gscip->AddVariable(0.0, 1.0, -2.0, GScipVarType::kBinary, "z").value();
  GScipIndicatorConstraint ind_cons;
  ind_cons.upper_bound = 3.0;
  ind_cons.variables = {x, y};
  ind_cons.coefficients = {1.0, 1.0};
  ind_cons.indicator_variable = z;
  ind_cons.negate_indicator = true;
  ASSERT_OK(gscip->AddIndicatorConstraint(ind_cons).status());

  GScipLinearRange range;
  range.upper_bound = 0.0;
  range.lower_bound = 0.0;
  range.variables = {x, y};
  range.coefficients = {1.0, -1.0};
  ASSERT_OK(gscip->AddLinearConstraint(range).status());

  auto params = TestGScipParameters();
  GScipSetOutputEnabled(&params, true);
  const GScipResult result = gscip->Solve(params).value();
  AssertOptimalWithBestSolution(result, 3.0, {{x, 1.5}, {y, 1.5}, {z, 0.0}});
}

TEST(GScipTest, BadQuadraticConstraintLinearPart) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  SCIP_VAR* x =
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kContinuous, "x").value();
  GScipQuadraticRange range;
  range.lower_bound = 1.0;
  range.upper_bound = 1.0;
  range.linear_variables = {x};
  range.linear_coefficients = {1.0, 1.0};
  EXPECT_THAT(
      gscip->AddQuadraticConstraint(range, "c1"),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr("Error adding quadratic constraint: c1 in linear term.")));
}

TEST(GScipTest, BadQuadraticConstraintQuadraticVariables) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  SCIP_VAR* x =
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kContinuous, "x").value();
  GScipQuadraticRange range;
  range.lower_bound = 1.0;
  range.upper_bound = 1.0;
  range.quadratic_variables1 = {x};
  range.quadratic_variables2 = {};
  range.quadratic_coefficients = {1.0};
  EXPECT_THAT(
      gscip->AddQuadraticConstraint(range, "c1"),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr(
              "Error adding quadratic constraint: c1 in quadratic term.")));
}

TEST(GScipTest, BadQuadraticConstraintQuadraticCoefficients) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  SCIP_VAR* x =
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kContinuous, "x").value();
  GScipQuadraticRange range;
  range.lower_bound = 1.0;
  range.upper_bound = 1.0;
  range.quadratic_variables1 = {x};
  range.quadratic_variables2 = {x};
  range.quadratic_coefficients = {1.0, 2.0};
  EXPECT_THAT(
      gscip->AddQuadraticConstraint(range, "c1"),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr(
              "Error adding quadratic constraint: c1 in quadratic term.")));
}

TEST(GScipTest, BadAnd) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  SCIP_VAR* x =
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "x").value();
  GScipLogicalConstraintData and_cons;
  and_cons.operators = {x};
  EXPECT_THAT(
      gscip->AddAndConstraint(and_cons, "c1"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("!= nullptr; Error adding and constraint: c1")));
}

TEST(GScipTest, BadOr) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  SCIP_VAR* x =
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "x").value();
  GScipLogicalConstraintData or_cons;
  or_cons.operators = {x};
  EXPECT_THAT(
      gscip->AddOrConstraint(or_cons, "c1"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("!= nullptr; Error adding or constraint: c1")));
}

TEST(GScipTest, BadIndicatorMissingInd) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  SCIP_VAR* x =
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "x").value();
  GScipIndicatorConstraint ind;
  ind.variables = {x};
  ind.coefficients = {1.0};
  ind.upper_bound = 0.5;
  EXPECT_THAT(
      gscip->AddIndicatorConstraint(ind, "c1"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("!= nullptr; Error adding indicator constraint: c1")));
}

TEST(GScipTest, BadIndicatorBadCoefficients) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  SCIP_VAR* x =
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "x").value();
  SCIP_VAR* y =
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "y").value();
  GScipIndicatorConstraint ind;
  ind.indicator_variable = x;
  ind.variables = {y};
  ind.coefficients = {1.0, 2.0};
  ind.upper_bound = 0.5;
  EXPECT_THAT(gscip->AddIndicatorConstraint(ind, "c1"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Error adding indicator constraint: c1.")));
}

TEST(GScipTest, NoVariablesSOS1) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  GScipSOSData sos1_data;
  sos1_data.variables = {};
  EXPECT_THAT(gscip->AddSOS1Constraint(sos1_data, "c1"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Error adding SOS constraint: c1")));
}

TEST(GScipTest, VariablesMatchWeightsSOS2) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  SCIP_VAR* x =
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "x").value();
  GScipSOSData sos2_data;
  sos2_data.variables = {x};
  sos2_data.weights = {3.0, 4.0};
  EXPECT_THAT(gscip->AddSOS2Constraint(sos2_data, "c1"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Error adding SOS constraint: c1")));
}

TEST(GScipTest, DistinctWeightsSOS1) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  SCIP_VAR* x =
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "x").value();
  SCIP_VAR* y =
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "y").value();
  GScipSOSData sos_data;
  sos_data.variables = {x, y};
  sos_data.weights = {3.0, 3.0};
  EXPECT_THAT(
      gscip->AddSOS1Constraint(sos_data, "c1"),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr(
              "Error adding SOS constraint: c1, weights must be distinct")));
}

// max 3*x + 8
// s.t. 0 <= x <= 2
// x in [0, 4]
//
// x* = 2, obj* = 14
TEST(GScipTest, KeepConstraintAliveFalse) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK(gscip->SetObjectiveOffset(8.0));
  SCIP_VAR* x =
      gscip->AddVariable(0.0, 4.0, 3.0, GScipVarType::kContinuous, "x").value();
  GScipLinearRange range;
  range.lower_bound = 0.0;
  range.upper_bound = 2.0;
  range.variables.push_back(x);
  range.coefficients.push_back(1.0);
  GScipConstraintOptions options = DefaultGScipConstraintOptions();
  options.keep_alive = false;

  EXPECT_OK(gscip->AddLinearConstraint(range, "x_bound", options));
  ASSERT_OK(gscip->SetMaximize(true));
  const GScipResult result = gscip->Solve(TestGScipParameters()).value();
  AssertOptimalWithBestSolution(result, 14.0, {{x, 2.0}});
  EXPECT_THAT(gscip->constraints(), IsEmpty());
}

TEST(GScipTest, SilenceOutput) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));

  // A sub-string expected to have been printed in stdout when silence_output is
  // false.
  const std::string kExpectedNoise = "Gap";

  // First test with `silence_output` unset (using the default parameters).
  CaptureTestStdout();
  EXPECT_OK(gscip->Solve());
  EXPECT_THAT(GetCapturedTestStdout(), HasSubstr(kExpectedNoise));

  // Then test with `silence_output` set to true.
  {
    GScipParameters parameters;
    parameters.set_silence_output(true);
    CaptureTestStdout();
    EXPECT_OK(gscip->Solve(parameters));
    EXPECT_EQ(GetCapturedTestStdout(), "");
  }

  // Then call again the same GSCIP with `silence_output` unset (using the
  // default parameters). We expect GScip to have reset the value and not to
  // have kept the `true` value from last Solve().
  CaptureTestStdout();
  EXPECT_OK(gscip->Solve());
  EXPECT_THAT(GetCapturedTestStdout(), HasSubstr(kExpectedNoise));

  // Then test with `silence_output` set to false.
  {
    GScipParameters parameters;
    parameters.set_silence_output(false);
    CaptureTestStdout();
    EXPECT_OK(gscip->Solve(parameters));
    EXPECT_THAT(GetCapturedTestStdout(), HasSubstr(kExpectedNoise));
  }

  // Finally call again the same GSCIP with `silence_output` unset (using the
  // default parameters).
  CaptureTestStdout();
  EXPECT_OK(gscip->Solve());
  EXPECT_THAT(GetCapturedTestStdout(), HasSubstr(kExpectedNoise));
}

TEST(GScipTest, LogFile) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));

  ASSERT_OK_AND_ASSIGN(
      const std::string temp_file_name,
      file::MakeTempFilename(::testing::TempDir(), "search_logs"));

  // Create the empty file.
  ASSERT_OK(file::SetContents(temp_file_name, "", file::Defaults()));

  // First test with `search_logs_filename` unset (using the default
  // parameters).
  EXPECT_OK(gscip->Solve());
  EXPECT_THAT(file::GetContents(temp_file_name, file::Defaults()),
              IsOkAndHolds(""));

  // Reset the file content between tests.
  ASSERT_OK(file::SetContents(temp_file_name, "", file::Defaults()));

  // Then test with `search_logs_filename` set to the temporary file name.
  {
    GScipParameters parameters;
    parameters.set_search_logs_filename(temp_file_name);
    EXPECT_OK(gscip->Solve(parameters));
    EXPECT_THAT(file::GetContents(temp_file_name, file::Defaults()),
                IsOkAndHolds(HasSubstr("Gap")));
  }

  ASSERT_OK(file::SetContents(temp_file_name, "", file::Defaults()));

  // Then call again the same GSCIP with `search_logs_filename` unset (using the
  // default parameters). We expect GScip to have reset the value and not to
  // have kept the `true` value from last Solve().
  EXPECT_OK(gscip->Solve());
  EXPECT_THAT(file::GetContents(temp_file_name, file::Defaults()),
              IsOkAndHolds(""));
}

TEST(GScipTest, MessageHandler) {
  // We want to test both values of silence_output to make sure that when a
  // message handler is used, the messages are always generated.
  for (const bool silence_output : {true, false}) {
    SCOPED_TRACE(
        absl::StrCat("silence_output ", silence_output ? "true" : "false"));

    ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                         GScip::Create("scip_test"));

    GScipParameters parameters;
    parameters.set_silence_output(silence_output);

    testing::MockFunction<void(GScipMessageType type,
                               absl::string_view message)>
        message_handler_mock;

    // We call a GScip with a message handler. It should be called at least once
    // with the sub-string "Gap" that is part of the final message printed by
    // SCIP at the end of a solve.
    EXPECT_CALL(message_handler_mock, Call(_, _)).Times(AtLeast(1));
    EXPECT_CALL(message_handler_mock, Call(_, HasSubstr("Gap")))
        .Times(AtLeast(1));

    // We test that nothing is printed to stdout.
    CaptureTestStdout();
    EXPECT_OK(gscip->Solve(parameters, message_handler_mock.AsStdFunction()));
    EXPECT_EQ(GetCapturedTestStdout(), "");

    // We call the same GScip without the message_handler. The previous message
    // handler should not be called.
    Mock::VerifyAndClearExpectations(&message_handler_mock);

    EXPECT_CALL(message_handler_mock, Call(_, _)).Times(0);

    EXPECT_OK(gscip->Solve(parameters));
  }
}

// max 3*x
// x in {0, 1, 2}
//
// x* = 2, obj* = 6
//
// Objective limit is 7, so solve should return infeasible.
TEST(GScipTest, ObjectiveLimitInfeasibleAndRemoveLimitIncrementalSolve) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  SCIP_VAR* const x =
      gscip->AddVariable(0.0, 2.0, 3.0, GScipVarType::kInteger, "x").value();
  ASSERT_OK(gscip->SetMaximize(true));
  GScipParameters params = TestGScipParameters();
  params.set_objective_limit(7);
  {
    const GScipResult result = gscip->Solve(params).value();
    EXPECT_EQ(result.gscip_output.status(), GScipOutput::INFEASIBLE);
  }
  {
    // Solve again with the limit removed, make sure we get optimal.
    params.clear_objective_limit();
    const GScipResult result = gscip->Solve(params).value();
    AssertOptimalWithBestSolution(result, 6.0, {{x, 2.0}});
  }
}

// max x + 2y + 3z
// s.t. x + y + z == 1
//    x, y, z in {0,1}
//
// Use hints to ensure the solver sees all 3 solutions.
//
// Set the objective limit to 1.5. Ensure that the two best solutions are
// returned.
TEST(GScipTest, MultipleSolutionsAndObjectiveLimitMaximize) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  SCIP_VAR* const x =
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "x").value();
  SCIP_VAR* const y =
      gscip->AddVariable(0.0, 1.0, 2.0, GScipVarType::kInteger, "y").value();
  SCIP_VAR* const z =
      gscip->AddVariable(0.0, 1.0, 3.0, GScipVarType::kInteger, "z").value();
  ASSERT_OK(gscip->SetMaximize(true));
  GScipLinearRange range;
  range.lower_bound = 1.0;
  range.upper_bound = 1.0;
  range.variables = {x, y, z};
  range.coefficients = {1.0, 1.0, 1.0};
  ASSERT_OK(gscip->AddLinearConstraint(range).status());
  GScipParameters parameters = TestGScipParameters();
  parameters.set_objective_limit(1.5);
  parameters.set_num_solutions(5);  // only 2 solutions better than limit.
  // Use hints to ensure that all solutions are found.
  ASSERT_THAT(gscip->SuggestHint({{x, 1.0}, {y, 0.0}, {z, 0.0}}),
              IsOkAndHolds(GScipHintResult::kAccepted));
  ASSERT_THAT(gscip->SuggestHint({{x, 0.0}, {y, 1.0}, {z, 0.0}}),
              IsOkAndHolds(GScipHintResult::kAccepted));
  const GScipResult result = gscip->Solve(parameters).value();
  ASSERT_EQ(result.gscip_output.status(), GScipOutput::OPTIMAL);
  ASSERT_GE(result.solutions.size(), 2);
  EXPECT_THAT(
      result.solutions[0],
      GScipSolutionAlmostEquals(GScipSolution({{x, 0.0}, {y, 0.0}, {z, 1.0}})));
  EXPECT_THAT(
      result.solutions[1],
      GScipSolutionAlmostEquals(GScipSolution({{x, 0.0}, {y, 1.0}, {z, 0.0}})));
}

// min x + 2y + 3z
// s.t. x + y + z == 1
//    x, y, z in {0,1}
//
// Use hints to ensure the solver sees all 3 solutions.
//
// Set the objective limit to 2.5. Ensure that only two solutions are returned.
TEST(GScipTest, MultipleSolutionsAndObjectiveLimitMinimize) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  SCIP_VAR* const x =
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "x").value();
  SCIP_VAR* const y =
      gscip->AddVariable(0.0, 1.0, 2.0, GScipVarType::kInteger, "y").value();
  SCIP_VAR* const z =
      gscip->AddVariable(0.0, 1.0, 3.0, GScipVarType::kInteger, "z").value();
  ASSERT_OK(gscip->SetMaximize(false));
  GScipLinearRange range;
  range.lower_bound = 1.0;
  range.upper_bound = 1.0;
  range.variables = {x, y, z};
  range.coefficients = {1.0, 1.0, 1.0};
  ASSERT_OK(gscip->AddLinearConstraint(range).status());
  GScipParameters parameters = TestGScipParameters();
  parameters.set_objective_limit(2.5);
  parameters.set_num_solutions(5);  // only 2 solutions better than limit.
  // Use hints to ensure that all solutions are found.
  ASSERT_THAT(gscip->SuggestHint({{x, 0.0}, {y, 0.0}, {z, 1.0}}),
              IsOkAndHolds(GScipHintResult::kAccepted));
  ASSERT_THAT(gscip->SuggestHint({{x, 0.0}, {y, 1.0}, {z, 0.0}}),
              IsOkAndHolds(GScipHintResult::kAccepted));
  const GScipResult result = gscip->Solve(parameters).value();
  ASSERT_EQ(result.gscip_output.status(), GScipOutput::OPTIMAL);
  ASSERT_GE(result.solutions.size(), 2);
  EXPECT_THAT(
      result.solutions[0],
      GScipSolutionAlmostEquals(GScipSolution({{x, 1.0}, {y, 0.0}, {z, 0.0}})));
  EXPECT_THAT(
      result.solutions[1],
      GScipSolutionAlmostEquals(GScipSolution({{x, 0.0}, {y, 1.0}, {z, 0.0}})));
}

// max 3*x
// x in {0, 1, 2}
//
// x* = 2, obj* = 6
//
// Objective limit is 6, so solve should return optimal. Users are encouraged to
// use a tolerance on more complex problems to avoid numerical issues.
TEST(GScipTest, ObjectiveLimitIsExactOptimum) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  SCIP_VAR* const x =
      gscip->AddVariable(0.0, 2.0, 3.0, GScipVarType::kInteger, "x").value();
  ASSERT_OK(gscip->SetMaximize(true));
  GScipParameters params = TestGScipParameters();
  params.set_objective_limit(6);
  const GScipResult result = gscip->Solve(params).value();
  AssertOptimalWithBestSolution(result, 6.0, {{x, 2.0}});
}

// The purpose of this test is to ensure that solutions not meeting the
// objective limit do not count towards the solution limit.
//
// max w + 2x + 3y + 4z
// s.t. w + x + y + z == 1
//    w, x, y, z in {0,1}
//
// Use hints to ensure the solver sees (w=1), (x=1) and (y=1).
//
// Set the objective limit to 1.5 and solution limit 2. Ensure that solutions
// for x and y are returned, but not z, as we have hit the solution limit. Note
// that w will still be returned, but it does not meet the limit.
TEST(GScipTest, ObjectiveLimitAndSolutionLimit) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  SCIP_VAR* const w =
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "w").value();
  SCIP_VAR* const x =
      gscip->AddVariable(0.0, 1.0, 2.0, GScipVarType::kInteger, "x").value();
  SCIP_VAR* const y =
      gscip->AddVariable(0.0, 1.0, 3.0, GScipVarType::kInteger, "y").value();
  SCIP_VAR* const z =
      gscip->AddVariable(0.0, 1.0, 4.0, GScipVarType::kInteger, "z").value();
  ASSERT_OK(gscip->SetMaximize(true));
  GScipLinearRange range;
  range.lower_bound = 1.0;
  range.upper_bound = 1.0;
  range.variables = {w, x, y, z};
  range.coefficients = {1.0, 1.0, 1.0, 1.0};
  ASSERT_OK(gscip->AddLinearConstraint(range).status());
  GScipParameters parameters = TestGScipParameters();
  parameters.set_objective_limit(1.5);
  parameters.set_num_solutions(5);
  (*parameters.mutable_int_params())["limits/solutions"] = 2;
  ASSERT_THAT(gscip->SuggestHint({{w, 1.0}, {x, 0.0}, {y, 0.0}, {z, 0.0}}),
              IsOkAndHolds(GScipHintResult::kAccepted));
  ASSERT_THAT(gscip->SuggestHint({{w, 0.0}, {x, 1.0}, {y, 0.0}, {z, 0.0}}),
              IsOkAndHolds(GScipHintResult::kAccepted));
  ASSERT_THAT(gscip->SuggestHint({{w, 0.0}, {x, 0.0}, {y, 1.0}, {z, 0.0}}),
              IsOkAndHolds(GScipHintResult::kAccepted));

  const GScipResult result = gscip->Solve(parameters).value();
  ASSERT_EQ(result.gscip_output.status(), GScipOutput::SOL_LIMIT);
  ASSERT_GE(result.solutions.size(), 2);
  EXPECT_THAT(result.solutions[0],
              GScipSolutionAlmostEquals(
                  GScipSolution({{w, 0.0}, {x, 0.0}, {y, 1.0}, {z, 0.0}})));
  EXPECT_THAT(result.solutions[1],
              GScipSolutionAlmostEquals(
                  GScipSolution({{w, 0.0}, {x, 1.0}, {y, 0.0}, {z, 0.0}})));
}

// Test that GScip::ScipInf() returns GScip::kDefaultScipInf on a new instance.
TEST(GScipTest, DefaultScipInf) {
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  EXPECT_EQ(gscip->ScipInf(), GScip::kDefaultScipInf);
}

TEST(GScipTest, ScipInfClamp) {
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  EXPECT_THAT(gscip->ScipInfClamp(kInf), IsOkAndHolds(gscip->ScipInf()));
  EXPECT_THAT(gscip->ScipInfClamp(1.0e30),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(gscip->ScipInfClamp(30.0), IsOkAndHolds(30.0));
  EXPECT_THAT(gscip->ScipInfClamp(-30.0), IsOkAndHolds(-30.0));
  EXPECT_THAT(gscip->ScipInfClamp(-1.0e30),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(gscip->ScipInfClamp(-kInf), IsOkAndHolds(-gscip->ScipInf()));
}

TEST(GScipTest, ScipInfUnclamp) {
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  EXPECT_EQ(gscip->ScipInfUnclamp(kInf), kInf);
  EXPECT_EQ(gscip->ScipInfUnclamp(1.0e30), kInf);
  EXPECT_EQ(gscip->ScipInfUnclamp(gscip->ScipInf()), kInf);
  EXPECT_EQ(gscip->ScipInfUnclamp(30.0), 30.0);
  EXPECT_EQ(gscip->ScipInfUnclamp(-30.0), -30.0);
  EXPECT_EQ(gscip->ScipInfUnclamp(-gscip->ScipInf()), -kInf);
  EXPECT_EQ(gscip->ScipInfUnclamp(-1.0e30), -kInf);
  EXPECT_EQ(gscip->ScipInfUnclamp(-kInf), -kInf);
}

// Even though we create x with vartype kInteger, SCIP converts it to kBinary
// internally (due to the bounds). Here we test that, if we do explicitly
// (re)set the vartype to kInteger, we can update the bounds.
TEST(GScipTest, IntegerVariableConvertedToBinary) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x,
      gscip->AddVariable(0.0, 1.0, 1.0, GScipVarType::kInteger, "x"));
  ASSERT_OK(gscip->SetMaximize(true));

  EXPECT_EQ(gscip->VarType(x), GScipVarType::kBinary);

  EXPECT_OK(gscip->SetVarType(x, GScipVarType::kInteger));
  ASSERT_OK(gscip->SetUb(x, -1.0));
  ASSERT_OK(gscip->SetUb(x, 2.0));

  EXPECT_EQ(gscip->VarType(x), GScipVarType::kInteger);
  EXPECT_EQ(gscip->Lb(x), 0.0);
  EXPECT_EQ(gscip->Ub(x), 2.0);
}

// Tests for bounds out of SCIP's finite range but not floating point actual
// infinities.
class GScipBoundOutOfRangeTest : public testing::TestWithParam<double> {};

TEST_P(GScipBoundOutOfRangeTest, AddVariableLowerBound) {
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  EXPECT_THAT(
      gscip->AddVariable(/*lb=*/GetParam(), /*ub=*/kInf, /*obj_coef=*/0.0,
                         GScipVarType::kContinuous),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("lower bound")));
}

TEST_P(GScipBoundOutOfRangeTest, AddVariableObjCoeff) {
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  EXPECT_THAT(
      gscip->AddVariable(/*lb=*/-kInf, /*ub=*/kInf, /*obj_coef=*/GetParam(),
                         GScipVarType::kContinuous),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("objective coefficient")));
}

TEST_P(GScipBoundOutOfRangeTest, AddLinearConstraintLowerBound) {
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  EXPECT_THAT(
      gscip->AddLinearConstraint({.lower_bound = GetParam()}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("lower bound")));
}

TEST_P(GScipBoundOutOfRangeTest, AddLinearConstraintCoefficient) {
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x,
      gscip->AddVariable(-2.0, 4.0, 3.0, GScipVarType::kContinuous, "x"));
  EXPECT_THAT(
      gscip->AddLinearConstraint(
          {.variables = {x}, .coefficients = {GetParam()}}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("coefficient")));
}

TEST_P(GScipBoundOutOfRangeTest, AddQuadraticConstraintLowerBound) {
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  EXPECT_THAT(
      gscip->AddQuadraticConstraint({.lower_bound = GetParam()}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("lower bound")));
}

TEST_P(GScipBoundOutOfRangeTest, AddQuadraticConstraintLinearCoefficient) {
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x,
      gscip->AddVariable(-2.0, 4.0, 3.0, GScipVarType::kContinuous, "x"));
  EXPECT_THAT(
      gscip->AddQuadraticConstraint(
          {.linear_variables = {x}, .linear_coefficients = {GetParam()}}),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("linear coefficient")));
}

TEST_P(GScipBoundOutOfRangeTest, AddQuadraticConstraintQuadraticCoefficient) {
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x,
      gscip->AddVariable(-2.0, 4.0, 3.0, GScipVarType::kContinuous, "x"));
  EXPECT_THAT(
      gscip->AddQuadraticConstraint({.quadratic_variables1 = {x},
                                     .quadratic_variables2 = {x},
                                     .quadratic_coefficients = {GetParam()}}),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("quadratic coefficient")));
}

TEST_P(GScipBoundOutOfRangeTest, SetVariableLowerBound) {
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x,
      gscip->AddVariable(/*lb=*/-3.0, /*ub=*/kInf, /*obj_coef=*/3.0,
                         GScipVarType::kContinuous, "x"));
  EXPECT_THAT(
      gscip->SetLb(x, GetParam()),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("lower bound")));
}

TEST_P(GScipBoundOutOfRangeTest, SetVariableObjectiveCoefficient) {
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x,
      gscip->AddVariable(/*lb=*/-3.0, /*ub=*/kInf, /*obj_coef=*/3.0,
                         GScipVarType::kContinuous, "x"));
  EXPECT_THAT(gscip->SetObjCoef(x, GetParam()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid objective coefficient")));
}

TEST_P(GScipBoundOutOfRangeTest, SetObjectiveOffset) {
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  EXPECT_THAT(gscip->SetObjectiveOffset(GetParam()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid objective offset")));
}

TEST_P(GScipBoundOutOfRangeTest, SetLinearConstraintLowerBound) {
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK_AND_ASSIGN(SCIP_CONS* const c, gscip->AddLinearConstraint({}, "c"));
  EXPECT_THAT(
      gscip->SetLinearConstraintLb(c, GetParam()),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("lower bound")));
}

TEST_P(GScipBoundOutOfRangeTest, AddLinearConstraintCoef) {
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x,
      gscip->AddVariable(/*lb=*/-3.0, /*ub=*/kInf, /*obj_coef=*/3.0,
                         GScipVarType::kContinuous, "x"));
  ASSERT_OK_AND_ASSIGN(SCIP_CONS* const c, gscip->AddLinearConstraint({}, "c"));
  EXPECT_THAT(gscip->AddLinearConstraintCoef(c, x, GetParam()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid coefficient")));
}

TEST_P(GScipBoundOutOfRangeTest, SetLinearConstraintCoef) {
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x,
      gscip->AddVariable(/*lb=*/-3.0, /*ub=*/kInf, /*obj_coef=*/3.0,
                         GScipVarType::kContinuous, "x"));
  ASSERT_OK_AND_ASSIGN(SCIP_CONS* const c,
                       gscip->AddLinearConstraint(
                           {.variables = {x}, .coefficients = {3.5}}, "c"));
  EXPECT_THAT(gscip->SetLinearConstraintCoef(c, x, GetParam()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid coefficient")));
}

TEST_P(GScipBoundOutOfRangeTest, AddVariableUpperBound) {
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  EXPECT_THAT(
      gscip->AddVariable(/*lb=*/-kInf, /*ub=*/GetParam(), /*obj_coef=*/0.0,
                         GScipVarType::kContinuous),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("upper bound")));
}

TEST_P(GScipBoundOutOfRangeTest, AddLinearConstraintUpperBound) {
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  EXPECT_THAT(
      gscip->AddLinearConstraint({.upper_bound = GetParam()}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("upper bound")));
}

TEST_P(GScipBoundOutOfRangeTest, AddQuadraticConstraintUpperBound) {
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  EXPECT_THAT(
      gscip->AddQuadraticConstraint({.upper_bound = GetParam()}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("upper bound")));
}

TEST_P(GScipBoundOutOfRangeTest, AddIndicatorConstraintUpperBound) {
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x,
      gscip->AddVariable(/*lb=*/0.0, /*ub=*/1.0, /*obj_coef=*/3.0,
                         GScipVarType::kInteger, "x"));
  EXPECT_THAT(
      gscip->AddIndicatorConstraint(
          {.indicator_variable = x, .upper_bound = GetParam()}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("upper bound")));
}

TEST_P(GScipBoundOutOfRangeTest, SetVariableUpperBound) {
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK_AND_ASSIGN(
      SCIP_VAR* const x,
      gscip->AddVariable(/*lb=*/-kInf, /*ub=*/3.0, /*obj_coef=*/3.0,
                         GScipVarType::kContinuous, "x"));
  EXPECT_THAT(
      gscip->SetUb(x, GetParam()),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("upper bound")));
}

TEST_P(GScipBoundOutOfRangeTest, SetLinearConstraintUpperBound) {
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<GScip> gscip,
                       GScip::Create("scip_test"));
  ASSERT_OK_AND_ASSIGN(SCIP_CONS* const c, gscip->AddLinearConstraint({}, "c"));
  EXPECT_THAT(
      gscip->SetLinearConstraintUb(c, GetParam()),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("upper bound")));
}

INSTANTIATE_TEST_SUITE_P(
    AllValues, GScipBoundOutOfRangeTest,
    testing::Values(-GScip::kDefaultScipInf, GScip::kDefaultScipInf,
                    -2.0 * GScip::kDefaultScipInf, 2.0 * GScip::kDefaultScipInf,
                    // Some assertions in SCIP use SCIP_INVALID.
                    -SCIP_INVALID, SCIP_INVALID));

}  // namespace
}  // namespace operations_research
