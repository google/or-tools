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

#include "ortools/linear_solver/proto_solver/preprocessor.h"

#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/preprocessor.h"
#include "ortools/glop/preprocessor_testing.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_parser.h"
#include "ortools/lp_data/lp_test_utils.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/proto_utils.h"
#include "ortools/lp_data/sparse_column.h"
#include "ortools/sat/mip_model.pb.h"
#include "ortools/sat/mp_model_wrapper.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace {

using ::operations_research::glop::ColIndex;
using ::operations_research::glop::DenseRow;
using ::operations_research::glop::Fractional;
using ::operations_research::glop::GlopParameters;
using ::operations_research::glop::kInfinity;
using ::operations_research::glop::LinearProgram;
using ::operations_research::glop::Preprocessor;
using ::operations_research::glop::ProblemSolution;
using ::operations_research::glop::RowIndex;
using ::operations_research::glop::StrictITIVector;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

const double kTolerance = 1e-6;

void ExpectLinearProgramMatches(
    const LinearProgram& linear_program,
    absl::string_view expected_preprocessed_problem) {
  LinearProgram expected_program;
  ASSERT_TRUE(ParseLp(expected_preprocessed_problem, &expected_program));
  EXPECT_EQ(expected_program.IsMaximizationProblem(),
            linear_program.IsMaximizationProblem());
  EXPECT_EQ(expected_program.num_variables(), linear_program.num_variables());
  EXPECT_EQ(expected_program.num_constraints(),
            linear_program.num_constraints());

  // Variable quantities.
  for (ColIndex col(0); col < expected_program.num_variables(); ++col) {
    SCOPED_TRACE(
        absl::StrCat("Variable name: ", linear_program.GetVariableName(col)));
    SCOPED_TRACE(absl::StrCat("Variable index: ", col.value()));
    EXPECT_NEAR(expected_program.objective_coefficients()[col],
                linear_program.objective_coefficients()[col], kTolerance);
    if (expected_program.variable_lower_bounds()[col] != -kInfinity) {
      EXPECT_NEAR(expected_program.variable_lower_bounds()[col],
                  linear_program.variable_lower_bounds()[col], kTolerance);
    } else {
      EXPECT_EQ(expected_program.variable_lower_bounds()[col],
                linear_program.variable_lower_bounds()[col]);
    }
    if (expected_program.variable_upper_bounds()[col] != kInfinity) {
      EXPECT_NEAR(expected_program.variable_upper_bounds()[col],
                  linear_program.variable_upper_bounds()[col], kTolerance);
    } else {
      EXPECT_EQ(expected_program.variable_upper_bounds()[col],
                linear_program.variable_upper_bounds()[col]);
    }
    EXPECT_TRUE(linear_program.GetSparseColumn(col).IsEqualTo(
        expected_program.GetSparseColumn(col)));
    // Check integrality.
    EXPECT_EQ(expected_program.IsVariableInteger(col),
              linear_program.IsVariableInteger(col));
  }
  EXPECT_EQ(expected_program.objective_offset(),
            linear_program.objective_offset());

  // Row quantities.
  EXPECT_EQ(expected_program.num_constraints(),
            linear_program.num_constraints());
  for (RowIndex row(0); row < expected_program.num_constraints(); ++row) {
    SCOPED_TRACE(absl::StrCat("Constraint name: ",
                              linear_program.GetConstraintName(row)));
    SCOPED_TRACE(absl::StrCat("Constraint index: ", row.value()));
    if (expected_program.constraint_lower_bounds()[row] != -kInfinity) {
      EXPECT_NEAR(expected_program.constraint_lower_bounds()[row],
                  linear_program.constraint_lower_bounds()[row], kTolerance);
    } else {
      EXPECT_EQ(expected_program.constraint_lower_bounds()[row],
                linear_program.constraint_lower_bounds()[row]);
    }
    if (expected_program.constraint_upper_bounds()[row] != kInfinity) {
      EXPECT_NEAR(expected_program.constraint_upper_bounds()[row],
                  linear_program.constraint_upper_bounds()[row], kTolerance);
    } else {
      EXPECT_EQ(expected_program.constraint_upper_bounds()[row],
                linear_program.constraint_upper_bounds()[row]);
    }
  }
}

void CheckMipPreprocessorBehavior(
    absl::string_view input_problem,
    absl::string_view expected_preprocessed_problem,
    const DenseRow& expected_solution, Preprocessor* preprocessor,
    Fractional initial_solution_value = 0.0) {
  ASSERT_NE(preprocessor, nullptr);

  // Initialize linear_program to the input_problem and preprocess it.
  LinearProgram linear_program;
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  ASSERT_TRUE(ParseLp(input_problem, &linear_program));
  preprocessor->SetTimeLimit(time_limit.get());
  EXPECT_THAT(preprocessor->Run(&linear_program),
              glop::PreprocessorResultIs(/*postsolve_is_needed=*/false,
                                         /*status=*/std::nullopt));

  // Check the preprocessor output.
  ExpectLinearProgramMatches(linear_program, expected_preprocessed_problem);

  // Solutions. Verify we can recover a solution multiple times.
  for (int i = 0; i < 5; ++i) {
    const ColIndex num_cols(linear_program.num_variables());
    const RowIndex num_rows(linear_program.num_constraints());
    glop::SolveStatus solve_status = glop::OptimalSolveStatus();
    ProblemSolution solution(num_rows, num_cols);
    for (ColIndex col(0); col < num_cols; ++col) {
      solution.primal_values[col] = initial_solution_value;
    }
    preprocessor->RecoverSolution(solve_status, &solution);
    EXPECT_THAT(solution.primal_values,
                glop::FractionalVectorComparable(expected_solution));
  }
}

// Returns the model seen by `wrapper` as an MPModelProto. All its constraints
// must be unenforced.
MPModelProto ToMPModelProto(const sat::MPModelProtoWrapper& wrapper) {
  MPModelProto result;
  if (!wrapper.name().empty()) result.set_name(wrapper.name());
  if (wrapper.objective_offset() != 0.0) {
    result.set_objective_offset(wrapper.objective_offset());
  }
  for (int v = 0; v < wrapper.num_variables(); ++v) {
    const sat::MipVariableProto& var = wrapper.variables(v);
    MPVariableProto* mp_var = result.add_variable();
    mp_var->set_lower_bound(var.lower_bound());
    mp_var->set_upper_bound(var.upper_bound());
    mp_var->set_objective_coefficient(var.objective_coefficient());
    mp_var->set_is_integer(var.is_integer());
    if (!var.name().empty()) mp_var->set_name(var.name());
  }
  for (int c = 0; c < wrapper.num_constraints(); ++c) {
    const sat::EnforcedLinearConstraintProto& ct = wrapper.constraints(c);
    CHECK(ct.enforcement_vars().empty());
    MPConstraintProto* mp_ct = result.add_constraint();
    if (!ct.name().empty()) mp_ct->set_name(ct.name());
    mp_ct->set_lower_bound(ct.lower_bound());
    mp_ct->set_upper_bound(ct.upper_bound());
    mp_ct->mutable_var_index()->Add(ct.vars().begin(), ct.vars().end());
    mp_ct->mutable_coefficient()->Add(ct.coeffs().begin(), ct.coeffs().end());
  }
  return result;
}

bool RunPropagateBounds(sat::MPModelProtoWrapper* wrapper) {
  const int num_variables = wrapper->num_variables();
  std::vector<double> var_lbs(num_variables);
  std::vector<double> var_ubs(num_variables);
  for (int v = 0; v < num_variables; ++v) {
    var_lbs[v] = wrapper->variables(v).lower_bound();
    var_ubs[v] = wrapper->variables(v).upper_bound();
  }
  if (!PropagateBounds(*wrapper, absl::MakeSpan(var_lbs),
                       absl::MakeSpan(var_ubs))) {
    return false;
  }
  for (int v = 0; v < num_variables; ++v) {
    wrapper->mutable_variables(v)->set_lower_bound(var_lbs[v]);
    wrapper->mutable_variables(v)->set_upper_bound(var_ubs[v]);
  }
  return true;
}

void CheckBoundPropagationPreprocessorBehavior(
    absl::string_view input_problem,
    absl::string_view expected_preprocessed_problem) {
  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(input_problem, &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);
  sat::MPModelProtoWrapper wrapper(std::move(model_proto), nullptr);
  EXPECT_TRUE(RunPropagateBounds(&wrapper));
  glop::MPModelProtoToLinearProgram(ToMPModelProto(wrapper), &linear_program);

  ExpectLinearProgramMatches(linear_program, expected_preprocessed_problem);
}

void CheckMakeBoundsIntegerBehavior(
    absl::string_view input_problem,
    absl::string_view expected_preprocessed_problem,
    Fractional integer_solution_tolerance) {
  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(input_problem, &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);
  sat::MPModelProtoWrapper wrapper(std::move(model_proto), nullptr);
  EXPECT_TRUE(MakeBoundsInteger(integer_solution_tolerance, &wrapper));
  glop::MPModelProtoToLinearProgram(ToMPModelProto(wrapper), &linear_program);

  ExpectLinearProgramMatches(linear_program, expected_preprocessed_problem);
}

// --------------------------------------------------------
// MakeBoundsInteger
// --------------------------------------------------------

TEST(IntegerBoundsPreprocessorTest, MakeColBoundsInteger1) {
  const Fractional integer_solution_tolerance = 1e-3;
  const std::string kLinearProgram =
      "0.5 <= x1 <= 1.5;"
      "0 <= x2 <= 1;"
      "1e-3 <= x3 <= 1;"
      "2e-3 <= x4 <= 1;"
      "-1 <= x5 <= -1e-3;"
      "-1 <= x6 <= -2e-3;"
      "0.5 <= x7 <= 1.5;"
      "int x1, x2, x3, x4, x5, x6";
  const std::string kOutputProgram =
      "x1 = 1;"
      "0 <= x2 <= 1;"
      "0 <= x3 <= 1;"
      "x4 = 1;"
      "-1 <= x5 <= 0;"
      "x6 = -1;"
      "0.5 <= x7 <= 1.5;"  // Non-integer variable has the same bounds.
      "int x1, x2, x3, x4, x5, x6";
  CheckMakeBoundsIntegerBehavior(kLinearProgram, kOutputProgram,
                                 integer_solution_tolerance);
}

TEST(IntegerBoundsPreprocessorTest, MakeColBoundsInteger2) {
  LinearProgram linear_program;
  ASSERT_TRUE(
      ParseLp("0.5 <= x <= 0.8;"
              "int x",
              &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);
  sat::MPModelProtoWrapper wrapper(std::move(model_proto), nullptr);
  EXPECT_FALSE(MakeBoundsInteger(1e-6, &wrapper));
}

TEST(IntegerBoundsPreprocessorTest, MakeRowBoundsInteger1) {
  const Fractional integer_solution_tolerance = 1e-3;
  const std::string kLinearProgram =
      "0.5 <= x1 + 2 x2 <= 1.5;"
      "1e-3 <= x1 + 2 x2 <= 1;"
      "2e-3 <= x1 + 2 x2 <= 1;"
      "-1 <= x1 + 2 x2 <= -1e-3;"
      "-1 <= x1 + 2 x2 <= -2e-3;"
      "0.5 <= x1 + 2.5 x2 <= 1.5;"
      "0.5 <= x1 + x3 <= 1.5;"
      "int x1, x2";
  const std::string kOutputProgram =
      "x1 + 2 x2 = 1;"
      "0 <= x1 + 2 x2 <= 1;"
      "x1 + 2 x2 = 1;"
      "-1 <= x1 + 2 x2 <= 0;"
      "x1 + 2 x2 = -1;"
      "0.5 <= x1 + 2.5 x2 <= 1.5;"  // Non-integer coefficient or non-integer
      "0.5 <= x1 + x3 <= 1.5;"      // variable -> bounds remain the same.
      "int x1, x2";
  CheckMakeBoundsIntegerBehavior(kLinearProgram, kOutputProgram,
                                 integer_solution_tolerance);
}

TEST(IntegerBoundsPreprocessorTest, MakeRowBoundsInteger2) {
  LinearProgram linear_program;
  ASSERT_TRUE(
      ParseLp("0.5 <= x1 + x2 <= 0.8;"
              "int x1, x2",
              &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);
  sat::MPModelProtoWrapper wrapper(std::move(model_proto), nullptr);
  EXPECT_FALSE(MakeBoundsInteger(1e-6, &wrapper));
}

TEST(BoundPropagationPreprocessorTest, InfeasibleMerge) {
  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp("x1 <= 10; r1: x1 >= 20;", &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);
  sat::MPModelProtoWrapper wrapper(std::move(model_proto), nullptr);
  EXPECT_FALSE(RunPropagateBounds(&wrapper));
}

TEST(BoundPropagationPreprocessorTest, AlmostInfeasibleMerge) {
  const std::string kLinearProgram =
      "x1 <= 0;"
      "r1: x1 >= 1e-10;"
      "int x1;";
  const std::string kOutputProgram =
      "r1: x1 >= 1e-10;"
      "x1 = 0;"
      "int x1;";
  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(kLinearProgram, &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);
  sat::MPModelProtoWrapper wrapper(std::move(model_proto), nullptr);
  EXPECT_TRUE(ExactPresolve(sat::SatParameters(), &wrapper));
  glop::MPModelProtoToLinearProgram(ToMPModelProto(wrapper), &linear_program);
  ExpectLinearProgramMatches(linear_program, kOutputProgram);
}

TEST(BoundPropagationPreprocessorTest, ExactPresolveDualRestrictionOnIntegers) {
  LinearProgram linear_program;
  ASSERT_TRUE(
      ParseLp("0 <= b <= 1;"
              "0 <= x <= 10;"
              "int b, x;"
              "r1: 2 x >= 5;",
              &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);
  sat::MPModelProtoWrapper wrapper(std::move(model_proto), nullptr);
  sat::AddEnforcement(0, 1, wrapper.mutable_constraints(0));

  EXPECT_TRUE(ExactPresolve(sat::SatParameters(), &wrapper));
  // 2*x >= 5 is satisfied for x >= 2.5, so dual reasoning restricts the upper
  // bound of integer x to ceil(2.5) = 3 (not floor(2.5) = 2).
  EXPECT_EQ(wrapper.variables(1).lower_bound(), 0.0);
  EXPECT_EQ(wrapper.variables(1).upper_bound(), 3.0);
}

TEST(BoundPropagationPreprocessorTest, ExactPresolveClearsTrivialConstraints) {
  LinearProgram linear_program;
  ASSERT_TRUE(
      ParseLp("min: -x - y - z;"
              "0 <= b <= 1;"
              "0 <= x <= 2;"
              "y >= 0;"
              "0 <= z <= 0.101;"
              "int b, x, y;"
              "r_tighten: x + y <= 3;"
              "r_trivial: y + z <= 3.5;"
              "r_enforced_trivial: y + z >= 0;",
              &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);
  sat::MPModelProtoWrapper wrapper(std::move(model_proto), nullptr);
  // Variables: x = 0, y = 1, z = 2, b = 3.
  sat::AddEnforcement(3, 1, wrapper.mutable_constraints(2));
  wrapper.mutable_constraints(1)->set_tolerance(1e-5);

  ASSERT_TRUE(ExactPresolve(sat::SatParameters(), &wrapper));
  // Propagation tightens y to [0, 3]. r_tighten remains active (max activity
  // 2 + 3 = 5 > 3), whereas using the exact z upper bound 0.101 (rather than
  // ceil(0.101) = 1), y + z <= 3.5 has max activity 3.101 <= 3.5 and becomes
  // trivial, as does the enforced constraint y + z >= 0.
  EXPECT_EQ(wrapper.variables(1).lower_bound(), 0.0);
  EXPECT_EQ(wrapper.variables(1).upper_bound(), 3.0);
  EXPECT_EQ(wrapper.constraints(0).upper_bound(), 3.0);
  EXPECT_EQ(wrapper.constraints(0).vars().size(), 2);
  for (const int c : {1, 2}) {
    SCOPED_TRACE(c);
    EXPECT_EQ(wrapper.constraints(c).lower_bound(), -kInfinity);
    EXPECT_EQ(wrapper.constraints(c).upper_bound(), kInfinity);
    EXPECT_THAT(wrapper.constraints(c).vars(), IsEmpty());
    EXPECT_THAT(wrapper.constraints(c).coeffs(), IsEmpty());
    EXPECT_THAT(wrapper.constraints(c).enforcement_vars(), IsEmpty());
  }
  EXPECT_EQ(wrapper.constraints(1).name(), "r_trivial");
  EXPECT_EQ(wrapper.constraints(1).tolerance(), 1e-5);
  EXPECT_EQ(wrapper.constraints(2).name(), "r_enforced_trivial");

  // Both z and b are now unused and fixed by
  // ShiftToZeroAndMarkAsIntegerUnusedVariables().
  EXPECT_THAT(ShiftToZeroAndMarkAsIntegerUnusedVariables(sat::SatParameters(),
                                                         &wrapper),
              ElementsAre(0.0, 0.0, 0.101, 0.0));
  EXPECT_TRUE(wrapper.variables(2).is_integer());
  EXPECT_EQ(wrapper.variables(2).lower_bound(), 0.0);
  EXPECT_EQ(wrapper.variables(2).upper_bound(), 0.0);
}

TEST(BoundPropagationPreprocessorTest,
     ExactPresolveClearsInitiallyTrivialEnforcedConstraintBeforeDualPass) {
  LinearProgram linear_program;
  ASSERT_TRUE(
      ParseLp("min: b - x;"
              "0 <= b <= 1;"
              "0 <= x <= 2;"
              "int b;"
              "r1: x <= 5;",
              &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);
  sat::MPModelProtoWrapper wrapper(std::move(model_proto), nullptr);
  // Variables: b = 0, x = 1. Enforcing b == 0 => x <= 5 would block dual
  // restriction from decreasing b to 0 unless cleared at the start of
  // ExactPresolve().
  sat::AddEnforcement(0, 0, wrapper.mutable_constraints(0));

  ASSERT_TRUE(ExactPresolve(sat::SatParameters(), &wrapper));
  EXPECT_EQ(wrapper.constraints(0).lower_bound(), -kInfinity);
  EXPECT_EQ(wrapper.constraints(0).upper_bound(), kInfinity);
  EXPECT_THAT(wrapper.constraints(0).vars(), IsEmpty());
  EXPECT_THAT(wrapper.constraints(0).enforcement_vars(), IsEmpty());
  EXPECT_EQ(wrapper.constraints(0).name(), "r1");
  EXPECT_EQ(wrapper.variables(0).lower_bound(), 0.0);
  EXPECT_EQ(wrapper.variables(0).upper_bound(), 0.0);
}

TEST(ClearTrivialConstraintsTest, RelaxesSingleTrivialSideOfRangedConstraint) {
  LinearProgram linear_program;
  ASSERT_TRUE(
      ParseLp("0 <= x <= 3;"
              "0 <= y <= 3;"
              "r1: -1 <= x + y <= 5;"
              "r2: 1 <= x + y <= 10;",
              &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);
  sat::MPModelProtoWrapper wrapper(std::move(model_proto), nullptr);

  ClearTrivialConstraints(&wrapper);
  EXPECT_EQ(wrapper.constraints(0).lower_bound(), -kInfinity);
  EXPECT_EQ(wrapper.constraints(0).upper_bound(), 5.0);
  EXPECT_EQ(wrapper.constraints(0).vars().size(), 2);

  EXPECT_EQ(wrapper.constraints(1).lower_bound(), 1.0);
  EXPECT_EQ(wrapper.constraints(1).upper_bound(), kInfinity);
  EXPECT_EQ(wrapper.constraints(1).vars().size(), 2);
}

TEST(ClearTrivialConstraintsTest, KeepsConstraintSideWithUnboundedDirection) {
  LinearProgram linear_program;
  ASSERT_TRUE(
      ParseLp("0 <= x <= 5;"
              "w >= 0;"
              "r1: -1 <= x - w <= 10;",
              &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);
  sat::MPModelProtoWrapper wrapper(std::move(model_proto), nullptr);

  ClearTrivialConstraints(&wrapper);
  EXPECT_EQ(wrapper.constraints(0).lower_bound(), -1.0);
  EXPECT_EQ(wrapper.constraints(0).upper_bound(), kInfinity);
  EXPECT_EQ(wrapper.constraints(0).vars().size(), 2);
}

TEST(ClearTrivialConstraintsTest, HandlesEnforcementOnFixedVariables) {
  LinearProgram linear_program;
  ASSERT_TRUE(
      ParseLp("min: x + b1 + b2 + b3;"
              "0 <= x <= 3;"
              "1 <= b1 <= 1;"
              "0 <= b2 <= 0;"
              "0 <= b3 <= 1;"
              "int b1, b2, b3;"
              "r1: x <= 2;"
              "r2: x <= 2;"
              "r3: x <= 2;"
              "r4: x <= 2;",
              &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);
  sat::MPModelProtoWrapper wrapper(std::move(model_proto), nullptr);
  // Variables: x = 0, b1 = 1, b2 = 2, b3 = 3.
  ASSERT_EQ(wrapper.variables(1).name(), "b1");
  ASSERT_EQ(wrapper.variables(3).name(), "b3");
  sat::AddEnforcement(1, 1, wrapper.mutable_constraints(0));
  sat::AddEnforcement(3, 1, wrapper.mutable_constraints(0));
  sat::AddEnforcement(2, 0, wrapper.mutable_constraints(0));
  sat::AddEnforcement(1, 1, wrapper.mutable_constraints(1));
  sat::AddEnforcement(3, 1, wrapper.mutable_constraints(2));
  sat::AddEnforcement(2, 1, wrapper.mutable_constraints(2));
  sat::AddEnforcement(3, 2, wrapper.mutable_constraints(3));

  ClearTrivialConstraints(&wrapper);
  // The literals b1 == 1 and b2 == 0 always hold and are removed.
  EXPECT_THAT(wrapper.constraints(0).enforcement_vars(), ElementsAre(3));
  EXPECT_THAT(wrapper.constraints(0).enforcement_values(), ElementsAre(1));
  EXPECT_EQ(wrapper.constraints(0).upper_bound(), 2.0);
  EXPECT_TRUE(wrapper.constraints(1).enforcement_vars().empty());
  EXPECT_EQ(wrapper.constraints(1).upper_bound(), 2.0);
  EXPECT_EQ(wrapper.constraints(1).vars().size(), 1);

  // The literals b2 == 1 and b3 == 2 can never hold.
  for (const int c : {2, 3}) {
    SCOPED_TRACE(c);
    EXPECT_EQ(wrapper.constraints(c).lower_bound(), -kInfinity);
    EXPECT_EQ(wrapper.constraints(c).upper_bound(), kInfinity);
    EXPECT_EQ(wrapper.constraints(c).vars().size(), 0);
    EXPECT_TRUE(wrapper.constraints(c).enforcement_vars().empty());
  }
}

TEST(BoundPropagationPreprocessorTest,
     ExactPresolvePropagatesConstraintWithFixedEnforcement) {
  LinearProgram linear_program;
  ASSERT_TRUE(
      ParseLp("min: -x;"
              "0 <= x <= 10;"
              "1 <= b <= 1;"
              "int b, x;"
              "r1: x <= 3;",
              &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);
  sat::MPModelProtoWrapper wrapper(std::move(model_proto), nullptr);
  // Variables: x = 0, b = 1.
  ASSERT_EQ(wrapper.variables(1).name(), "b");
  sat::AddEnforcement(1, 1, wrapper.mutable_constraints(0));

  ASSERT_TRUE(ExactPresolve(sat::SatParameters(), &wrapper));
  // The enforcement b == 1 always holds, so the constraint is propagated.
  EXPECT_EQ(wrapper.variables(0).upper_bound(), 3.0);
  EXPECT_TRUE(wrapper.constraints(0).enforcement_vars().empty());
}

TEST(BoundPropagationPreprocessorTest, BasicPropagation) {
  const std::string kLinearProgram =
      "0 <= x + y + z <= 10;"
      "x >= 0;"
      "y >= 2;"
      "z >= 4;";
  const std::string kOutputProgram =
      "0 <= x + y + z <= 10;"
      "0 <= x <= 4;"
      "2 <= y <= 6;"
      "4 <= z <= 8;";
  CheckBoundPropagationPreprocessorBehavior(kLinearProgram, kOutputProgram);
}

TEST(BoundPropagationPreprocessorTest, ChainedPropagation) {
  const std::string kLinearProgram =
      "x + y = 4;"
      "3z + 4t = 26;"
      "y - 2z = 6;"
      "x = 2;";
  const std::string kOutputProgram =
      "x + y = 4;"
      "3z + 4t = 26;"
      "y - 2z = 6;"
      "x = 2;"
      "y = 2;"
      "z = -2;"
      "t = 8;";
  CheckBoundPropagationPreprocessorBehavior(kLinearProgram, kOutputProgram);
}

TEST(BoundPropagationPreprocessorTest, EnforcedConstraintSkipped) {
  LinearProgram linear_program;
  ASSERT_TRUE(
      ParseLp("0 <= b <= 1;"
              "0 <= x <= 10;"
              "0 <= y <= 4;"
              "int b, x, y;"
              "r1: x <= 3;"
              "r2: x + y <= 7;",
              &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);
  sat::MPModelProtoWrapper wrapper(std::move(model_proto), nullptr);
  // Constraint 0 (r1: x <= 3) is enforced by b == 1, so it is skipped.
  sat::AddEnforcement(0, 1, wrapper.mutable_constraints(0));

  EXPECT_TRUE(RunPropagateBounds(&wrapper));
  // Constraint 1 (r2: x + y <= 7 with y >= 0) propagates x <= 7, while enforced
  // Constraint 0 (r1: x <= 3) is skipped.
  EXPECT_EQ(wrapper.variables(1).lower_bound(), 0.0);
  EXPECT_EQ(wrapper.variables(1).upper_bound(), 7.0);
}

TEST(ImpliedIntegerPreprocessorTest, PrimalDetectionReturnsTrue) {
  const std::string kLinearProgram =
      "int x; int y;"
      "5 <= x + y + 2z <= 5;"
      "5 <= 3x + 2y + z <= 5;"
      "4 <= 3x + y + 2z <= 6;"
      "0 <= x <= 2;"
      "0 <= y <= 2;"
      "0 <= z <= 2.5;";
  // z is implied integer variable because of the second constraint. Note that
  // the inequality constraint is not considered for detection.
  const std::string kOutputProgram =
      "int x; int y; int z;"
      "5 <= x + y + 2z <= 5;"
      "5 <= 3x + 2y + z <= 5;"
      "4 <= 3x + y + 2z <= 6;"
      "0 <= x <= 2;"
      "0 <= y <= 2;"
      "0 <= z <= 2;";
  const DenseRow kExpectedResult = {0, 0, 0};
  GlopParameters mip_parameters;
  ImpliedIntegerPreprocessor preprocessor(&mip_parameters, 1e-6);
  CheckMipPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                               &preprocessor);
}

TEST(ImpliedIntegerPreprocessorTest, PrimalDetectionReturnsFalse) {
  const std::string kLinearProgram =
      "int x; int y;"
      "5 <= 4x + 2y + 2z <= 5;"
      "4 <= 3x + y + z <= 6;"
      "0 <= x <= 2;"
      "0 <= y <= 2;"
      "0 <= z <= 2;";
  // Constraint bound ratio is not integer. Note that the inequality constraint
  // is not considered for detection.
  const DenseRow kExpectedResult = {0, 0, 0};
  GlopParameters mip_parameters;
  ImpliedIntegerPreprocessor preprocessor(&mip_parameters, 1e-6);
  CheckMipPreprocessorBehavior(kLinearProgram, kLinearProgram, kExpectedResult,
                               &preprocessor);
}

TEST(ImpliedIntegerPreprocessorTest, PrimalDetectionReturnsFalse2) {
  const std::string kLinearProgram =
      "int x; int y;"
      "6 <= 3x + 2y + 2z <= 6;"
      "0 <= x <= 2;"
      "0 <= y <= 2;"
      "0 <= z <= 2;";
  // Coefficient ratio for x is not integer.
  const DenseRow kExpectedResult = {0, 0, 0};
  GlopParameters mip_parameters;
  ImpliedIntegerPreprocessor preprocessor(&mip_parameters, 1e-6);
  CheckMipPreprocessorBehavior(kLinearProgram, kLinearProgram, kExpectedResult,
                               &preprocessor);
}

TEST(ImpliedIntegerPreprocessorTest, PrimalDetectionReturnsFalse3) {
  const std::string kLinearProgram =
      "int x;"
      "6 <= 3x + 2y + 2z <= 6;"
      "0 <= x <= 2;"
      "0 <= y <= 2;"
      "0 <= z <= 2;";
  // More than one non integer variables in the constraint.
  const DenseRow kExpectedResult = {0, 0, 0};
  GlopParameters mip_parameters;
  ImpliedIntegerPreprocessor preprocessor(&mip_parameters, 1e-6);
  CheckMipPreprocessorBehavior(kLinearProgram, kLinearProgram, kExpectedResult,
                               &preprocessor);
}

TEST(ImpliedIntegerPreprocessorTest, DualDetectionReturnsTrue) {
  const std::string kLinearProgram =
      "int x; int y;"
      "4 <= 3x + 2y + z <= 6;"
      "3 <= x + 4y + z <= 7;"
      "0 <= x <= 2;"
      "0 <= y <= 2;"
      "0 <= z <= 2;";
  // No equality row present but all rows satisfies primal detection criteria.
  const std::string kOutputProgram =
      "int x; int y; int z;"
      "4 <= 3x + 2y + z <= 6;"
      "3 <= x + 4y + z <= 7;"
      "0 <= x <= 2;"
      "0 <= y <= 2;"
      "0 <= z <= 2;";
  const DenseRow kExpectedResult = {0, 0, 0};
  GlopParameters mip_parameters;
  ImpliedIntegerPreprocessor preprocessor(&mip_parameters, 1e-6);
  CheckMipPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                               &preprocessor);
}

TEST(ImpliedIntegerPreprocessorTest, DualDetectionReturnsFalse) {
  const std::string kLinearProgram =
      "int x; int y;"
      "4 <= 3x + 2y + z <= 6;"
      "3 <= x + 4y + z <= 7.2;"
      "0 <= x <= 2;"
      "0 <= y <= 2;"
      "0 <= z <= 2;";
  // One rows fails to satisfy primal detection criteria.
  const DenseRow kExpectedResult = {0, 0, 0};
  GlopParameters mip_parameters;
  ImpliedIntegerPreprocessor preprocessor(&mip_parameters, 1e-6);
  CheckMipPreprocessorBehavior(kLinearProgram, kLinearProgram, kExpectedResult,
                               &preprocessor);
}

TEST(ImpliedIntegerPreprocessorTest, DualDetectionReturnsFalse2) {
  const std::string kLinearProgram =
      "min:z;"
      "int x; int y;"
      "4 <= 3x + 2y + z <= 6;"
      "3 <= x + 4y + z <= 7;"
      "0 <= x <= 2;"
      "0 <= y <= 2;"
      "0 <= z <= 2.1;";
  // Upper bound of z is not integer and z has a cost.
  const DenseRow kExpectedResult = {0, 0, 0};
  GlopParameters mip_parameters;
  ImpliedIntegerPreprocessor preprocessor(&mip_parameters, 1e-6);
  CheckMipPreprocessorBehavior(kLinearProgram, kLinearProgram, kExpectedResult,
                               &preprocessor);
}

TEST(ImpliedIntegerPreprocessorTest, DualDetectionWithOneSidedConstraints) {
  const std::string kLinearProgram =
      "int x; int y;"
      "2 <= x + y + z;"
      "3 <= 3x + 2y + z;"
      "6x + 2y + 2z <= 4;"
      "0 <= z <= 2;";
  const std::string kOutputProgram =
      "int x; int y; int z;"
      "2 <= x + y + z;"
      "3 <= 3x + 2y + z;"
      "6x + 2y + 2z <= 4;"
      "0 <= z <= 2;";
  const DenseRow kExpectedResult = {0, 0, 0};
  GlopParameters mip_parameters;
  ImpliedIntegerPreprocessor preprocessor(&mip_parameters, 1e-6);
  CheckMipPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                               &preprocessor);
}

TEST(ImpliedIntegerPreprocessorTest,
     DualDetectionWithNonIntegerVariableBounds) {
  const std::string kLinearProgram =
      "int x; int y;"
      "2 <= x + y + z;"
      "3 <= 3x + 2y + z;"
      "4 <= 6x + 2y + 2z;"
      "0 <= z <= 2.7;";
  // Because the constraints only impose integer bounds on z, and z has no
  // cost, we know that it can be set to an integer value even though there are
  // solution with z non-integer.
  const std::string kOutputProgram =
      "int x; int y; int z;"
      "2 <= x + y + z;"
      "3 <= 3x + 2y + z;"
      "4 <= 6x + 2y + 2z;"
      "0 <= z <= 2;";
  const DenseRow kExpectedResult = {0, 0, 0};
  GlopParameters mip_parameters;
  ImpliedIntegerPreprocessor preprocessor(&mip_parameters, 1e-6);
  CheckMipPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                               &preprocessor);
}

TEST(ReduceCostOverExclusiveOrConstraintPreprocessorTest, BasicBehavior) {
  const std::string kLinearProgram =
      "bin: x, y, z;"
      "max: x + 2y + z;"
      "x + y + z = 1;";
  const std::string kOutputProgram =
      "bin: x, y, z;"
      "max: 1 + y;"
      "x + y + z = 1;";
  const DenseRow kExpectedResult = {0, 0, 0};
  GlopParameters mip_parameters;
  ReduceCostOverExclusiveOrConstraintPreprocessor preprocessor(&mip_parameters);
  CheckMipPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                               &preprocessor);
}

TEST(ReduceCostOverExclusiveOrConstraintPreprocessorTest, MultipleConstraint) {
  const std::string kLinearProgram =
      "bin: x, y, z;"
      "max: x + 2y + z;"
      "x + y = 1;"
      "x + z >= 1;"
      "y + z = 1;";
  // The constraints will be processed in order, so the objective will become
  // first 1 + y + z and then 2!
  const std::string kOutputProgram =
      "bin: x, y, z;"
      "max: 2;"
      "x + y = 1;"
      "x + z >= 1;"
      "y + z = 1;";
  const DenseRow kExpectedResult = {0, 0, 0};
  GlopParameters mip_parameters;
  ReduceCostOverExclusiveOrConstraintPreprocessor preprocessor(&mip_parameters);
  CheckMipPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                               &preprocessor);
}

TEST(ReduceCostOverExclusiveOrConstraintPreprocessorTest, TheOrderMatterPart1) {
  const std::string kLinearProgram =
      "bin: x, y, z;"
      "max: x + 2y + 3z;"
      "x + y = 1;"
      "y + z = 1;";
  // After the first constraint, the objective is 1 + y + 3z, and then 2 + 2z.
  const std::string kOutputProgram =
      "bin: x, y, z;"
      "max: 2 + 2z;"
      "x + y = 1;"
      "y + z = 1;";
  const DenseRow kExpectedResult = {0, 0, 0};
  GlopParameters mip_parameters;
  ReduceCostOverExclusiveOrConstraintPreprocessor preprocessor(&mip_parameters);
  CheckMipPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                               &preprocessor);
}

TEST(ReduceCostOverExclusiveOrConstraintPreprocessorTest, TheOrderMatterPart2) {
  const std::string kLinearProgram =
      "bin: x, y, z;"
      "max: x + 2y + 3z;"
      "y + z = 1;"
      "x + y = 1;";
  // After the first constraint, the objective is 2 + x + z, and the second
  // constraint don't do anything because the cost of y is already zero.
  const std::string kOutputProgram =
      "bin: x, y, z;"
      "max: 2 + x + z;"
      "y + z = 1;"
      "x + y = 1;";
  const DenseRow kExpectedResult = {0, 0, 0};
  GlopParameters mip_parameters;
  ReduceCostOverExclusiveOrConstraintPreprocessor preprocessor(&mip_parameters);
  CheckMipPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                               &preprocessor);
}

TEST(ReduceCostOverExclusiveOrConstraintPreprocessorTest,
     OnlyWorkForExclusiveOr) {
  const std::string kLinearProgram =
      "bin: x, y, z;"
      "int: w;"
      "max: x + 2y + z;"
      "x + y + z <= 1;"  // Not =1
      "x + 2y + z = 1;"  // Coefficients are not all 1
      "z + y + w = 1;";  // w is not Binary.
  const DenseRow kExpectedResult = {0, 0, 0, 0};
  GlopParameters mip_parameters;
  ReduceCostOverExclusiveOrConstraintPreprocessor preprocessor(&mip_parameters);
  CheckMipPreprocessorBehavior(kLinearProgram, kLinearProgram, kExpectedResult,
                               &preprocessor);
}

TEST(ReduceCostOverExclusiveOrConstraintPreprocessorTest, NoVars) {
  const std::string kLinearProgram = "1 <=  <= 1;";
  const DenseRow kExpectedResult = {};
  GlopParameters mip_parameters;
  ReduceCostOverExclusiveOrConstraintPreprocessor preprocessor(&mip_parameters);
  CheckMipPreprocessorBehavior(kLinearProgram, kLinearProgram, kExpectedResult,
                               &preprocessor);
}

TEST(ShiftVariableBoundsTest, BoundShift) {
  const std::string kLinearProgram =
      "min: x1 + 2 x2 + 3 x3;"
      "0 <= x1 + x2 + x3 + x4 <= 10;"
      "2 <= x1 <= 3;"
      "-4.5 <= x2 <= -3.5;"
      "3 <= x3 <= 7;"
      "int x1, x3;";
  const std::string kOutputProgram =
      "min: 5 + x1 + 2 x2 + 3 x3;"
      "-2 <= x1 + x2 + x3 + x4 <= 8;"
      "0 <= x1 <= 1;"
      "-1.5 <= x2 <= -0.5;"
      "0 <= x3 <= 4;"
      "int x1, x3;";
  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(kLinearProgram, &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);
  sat::MPModelProtoWrapper wrapper(std::move(model_proto), nullptr);
  EXPECT_THAT(ShiftVariableBounds(IntegerShifts(), &wrapper),
              ElementsAre(2.0, -3.0, 3.0, 0.0));
  glop::MPModelProtoToLinearProgram(ToMPModelProto(wrapper), &linear_program);
  ExpectLinearProgramMatches(linear_program, kOutputProgram);
}

TEST(ShiftVariableBoundsTest, UpdatesSolutionHint) {
  const std::string kLinearProgram =
      "0 <= x1 + x2 <= 10;"
      "2 <= x1 <= 5;"
      "-6 <= x2 <= -2;"
      "int x1, x2;";
  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(kLinearProgram, &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);
  PartialVariableAssignment* hint = model_proto.mutable_solution_hint();
  hint->add_var_index(0);
  hint->add_var_value(4.0);
  hint->add_var_index(1);
  hint->add_var_value(-3.0);

  sat::MPModelProtoWrapper wrapper(std::move(model_proto), nullptr);
  EXPECT_THAT(ShiftVariableBounds(IntegerShifts(), &wrapper),
              ElementsAre(2.0, -2.0));
  EXPECT_THAT(wrapper.solution_hint().vars(), ElementsAre(0, 1));
  EXPECT_THAT(wrapper.solution_hint().values(), ElementsAre(2.0, -1.0));
}

TEST(ShiftVariableBoundsTest, ShiftsEnforcementValues) {
  const std::string kLinearProgram =
      "0 <= x1 + x2 <= 10;"
      "2 <= x1 <= 5;"
      "-6 <= x2 <= -2;"
      "int x1, x2;";
  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(kLinearProgram, &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);
  sat::MPModelProtoWrapper wrapper(std::move(model_proto), nullptr);
  sat::AddEnforcement(0, 2, wrapper.mutable_constraints(0));
  sat::AddEnforcement(1, -2, wrapper.mutable_constraints(0));

  EXPECT_THAT(ShiftVariableBounds(IntegerShifts(), &wrapper),
              ElementsAre(2.0, -2.0));
  // x1 shifted by +2 (to [0, 3]), so enforcement value 2 becomes 0.
  // x2 shifted by -2 (to [-4, 0]), so enforcement value -2 becomes 0.
  EXPECT_THAT(wrapper.constraints(0).enforcement_vars(), ElementsAre(0, 1));
  EXPECT_THAT(wrapper.constraints(0).enforcement_values(), ElementsAre(0, 0));
}

TEST(ShiftVariableBoundsTest, ShiftsFixedVariablesToZero) {
  const std::string kLinearProgram =
      "min: x1 + x2 + x3;"
      "0 <= x1 + x2 + x3 <= 10;"
      "0.5 <= x1 <= 0.5;"
      "-2.25 <= x2 <= -2.25;"
      "3 <= x3 <= 3;"
      "int x3;";
  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(kLinearProgram, &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);
  sat::MPModelProtoWrapper wrapper(std::move(model_proto), nullptr);
  EXPECT_THAT(ShiftVariableBounds(IntegerShifts(), &wrapper),
              ElementsAre(0.5, -2.25, 3.0));
  for (int v = 0; v < 3; ++v) {
    SCOPED_TRACE(v);
    EXPECT_EQ(wrapper.variables(v).lower_bound(), 0.0);
    EXPECT_EQ(wrapper.variables(v).upper_bound(), 0.0);
  }
  EXPECT_EQ(wrapper.objective_offset(), 1.25);
  EXPECT_EQ(wrapper.constraints(0).lower_bound(), -1.25);
  EXPECT_EQ(wrapper.constraints(0).upper_bound(), 8.75);
}

TEST(ShiftVariableBoundsTest, NoShiftWhenAllDomainsContainZero) {
  const std::string kLinearProgram =
      "0 <= x1 + x2 <= 10;"
      "-2 <= x1 <= 3;"
      "0 <= x2 <= 5;";
  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(kLinearProgram, &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);
  sat::MPModelProtoWrapper wrapper(std::move(model_proto), nullptr);
  EXPECT_THAT(ShiftVariableBounds(IntegerShifts(), &wrapper), IsEmpty());
  glop::MPModelProtoToLinearProgram(ToMPModelProto(wrapper), &linear_program);
  ExpectLinearProgramMatches(linear_program, kLinearProgram);
}

TEST(ShiftToZeroAndMarkAsIntegerUnusedVariablesTest,
     FixesShiftsAndMarksUnusedVariables) {
  const std::string kLinearProgram =
      "min: 2 x1 - 3 x2 + 0 x3 + x4 + x5;"
      "1.5 <= x1 <= 10;"
      "0 <= x2 <= 4.25;"
      "2.5 <= x3 <= 5;"
      "0 <= x4 <= 10;"
      "0 <= x5 <= 10;"
      "x4 + x5 >= 1;";
  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(kLinearProgram, &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);
  sat::MPModelProtoWrapper wrapper(std::move(model_proto), nullptr);
  const std::vector<double> shifts = ShiftToZeroAndMarkAsIntegerUnusedVariables(
      sat::SatParameters(), &wrapper);
  EXPECT_THAT(shifts, ElementsAre(1.5, 4.25, 2.5, 0.0, 0.0));
  for (int v : {0, 1, 2}) {
    EXPECT_DOUBLE_EQ(wrapper.variables(v).lower_bound(), 0.0);
    EXPECT_DOUBLE_EQ(wrapper.variables(v).upper_bound(), 0.0);
    EXPECT_DOUBLE_EQ(wrapper.variables(v).objective_coefficient(), 0.0);
    EXPECT_TRUE(wrapper.variables(v).is_integer());
  }
  EXPECT_FALSE(wrapper.variables(3).is_integer());
  EXPECT_FALSE(wrapper.variables(4).is_integer());
  EXPECT_DOUBLE_EQ(wrapper.objective_offset(), 2.0 * 1.5 - 3.0 * 4.25);
}

TEST(MergeExactDuplicateColumnsTest,
     MergesContinuousDuplicatesAndRecoversSolution) {
  const std::string linear_program_str =
      "min: 1.5 u1 + 1.5 u2 + 1.5 u3 + 2 y;"
      "u1 + u2 + u3 + y <= 15;"
      "2 u1 + 2 u2 + 2 u3 - y >= 0;"
      "-1 <= u1 <= 3;"
      "0 <= u2 <= 4;"
      "-2 <= u3 <= 5;"
      "0 <= y <= 10;";
  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(linear_program_str, &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);
  sat::MPModelProtoWrapper wrapper(std::move(model_proto), nullptr);

  const DuplicateColumnsPostsolve postsolve =
      MergeExactDuplicateColumns(sat::SatParameters(), &wrapper);
  EXPECT_EQ(postsolve.num_classes(), 1);
  EXPECT_DOUBLE_EQ(wrapper.variables(0).lower_bound(), -3.0);
  EXPECT_DOUBLE_EQ(wrapper.variables(0).upper_bound(), 12.0);
  EXPECT_DOUBLE_EQ(wrapper.variables(0).objective_coefficient(), 1.5);
  EXPECT_FALSE(wrapper.variables(0).is_integer());
  for (int v : {1, 2}) {
    EXPECT_DOUBLE_EQ(wrapper.variables(v).lower_bound(), 0.0);
    EXPECT_DOUBLE_EQ(wrapper.variables(v).upper_bound(), 0.0);
    EXPECT_DOUBLE_EQ(wrapper.variables(v).objective_coefficient(), 0.0);
    EXPECT_TRUE(wrapper.variables(v).is_integer());
  }
  EXPECT_THAT(wrapper.constraints(0).vars(), ElementsAre(0, 3));
  EXPECT_THAT(wrapper.constraints(0).coeffs(), ElementsAre(1.0, 1.0));
  EXPECT_THAT(wrapper.constraints(1).vars(), ElementsAre(0, 3));
  EXPECT_THAT(wrapper.constraints(1).coeffs(), ElementsAre(2.0, -1.0));

  std::vector<double> pos_solution = {8.0, 0.0, 0.0, 2.0};
  postsolve.Postsolve(absl::MakeSpan(pos_solution));
  // Upper capacities from 0.0 are u3: 5.0, u2: 4.0, u1: 3.0.
  EXPECT_THAT(pos_solution, ElementsAre(0.0, 3.0, 5.0, 2.0));

  std::vector<double> neg_solution = {-2.5, 0.0, 0.0, 2.0};
  postsolve.Postsolve(absl::MakeSpan(neg_solution));
  // Lower capacities from 0.0 are u3: 2.0, u1: 1.0, u2: 0.0.
  EXPECT_THAT(neg_solution, ElementsAre(-0.5, 0.0, -2.0, 2.0));
}

TEST(MergeExactDuplicateColumnsTest,
     MergesShiftedAndOppositeSignContinuousDuplicates) {
  const std::string linear_program_str =
      "min: 3 u1 + 3 u2 + v1 + v2 + w1 + w2 + w3;"
      "u1 + u2 + 2 v1 + 2 v2 + 4 w1 + 4 w2 + 4 w3 >= 9;"
      "2 <= u1 <= 5;"
      "3 <= u2 <= 8;"
      "-10 <= v1 <= -1;"
      "1 <= v2 <= 10;"
      "0 <= w1 <= 1;"
      "0 <= w2 <= 1;"
      "0 <= w3 <= 1;";
  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(linear_program_str, &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);
  // Set w1, w2 upper bounds to 1 + 2^-52 so greedy TightSum subtraction across
  // 3 members is exercised at the boundary 3 + 2^-51 without rounding loss.
  const double one_plus_ulp = std::nextafter(1.0, kInfinity);
  model_proto.mutable_variable(4)->set_upper_bound(one_plus_ulp);
  model_proto.mutable_variable(5)->set_upper_bound(one_plus_ulp);
  sat::MPModelProtoWrapper wrapper(std::move(model_proto), nullptr);

  const DuplicateColumnsPostsolve postsolve =
      MergeExactDuplicateColumns(sat::SatParameters(), &wrapper);
  EXPECT_EQ(postsolve.num_classes(), 3);
  EXPECT_DOUBLE_EQ(wrapper.variables(0).lower_bound(), 5.0);
  EXPECT_DOUBLE_EQ(wrapper.variables(0).upper_bound(), 13.0);
  EXPECT_DOUBLE_EQ(wrapper.variables(2).lower_bound(), -9.0);
  EXPECT_DOUBLE_EQ(wrapper.variables(2).upper_bound(), 9.0);
  const double expected_w_ub = 2.0 * one_plus_ulp + 1.0;
  EXPECT_DOUBLE_EQ(wrapper.variables(4).upper_bound(), expected_w_ub);
  EXPECT_THAT(wrapper.constraints(0).vars(), ElementsAre(0, 2, 4));

  // Base values for {u1, u2} are {2, 3} with capacities {3, 5}, so 12.0 splits
  // into {4.0, 8.0}; opposite-sign {v1, v2} with base values {-1, 1} splits 0.5
  // into {-1.0, 1.5}; {w1, w2, w3} at 3 + 2^-51 splits into
  // {1 + 2^-52, 1 + 2^-52, 1.0} without multi-step subtraction rounding.
  std::vector<double> solution = {12.0, 0.0, 0.5, 0.0, expected_w_ub, 0.0, 0.0};
  postsolve.Postsolve(absl::MakeSpan(solution));
  EXPECT_THAT(solution, ElementsAre(4.0, 8.0, -1.0, 1.5, one_plus_ulp,
                                    one_plus_ulp, 1.0));
}

TEST(MergeExactDuplicateColumnsTest,
     SkipsIneligibleMismatchedAndEnforcedColumns) {
  // Variables appear in order: p1, p2 (proportional, not equal),
  // c1, c2 (different costs), m1, m2 (int vs continuous),
  // f1, f2 (fixed continuous bounds), e1, e2 (e1 is an enforcement variable),
  // i1, i2 (integer duplicate columns, left to CP-SAT presolve).
  // Give each pair a distinct constraint coefficient so pairs do not
  // cross-match each other.
  const std::string linear_program_str =
      "min: p1 + p2 + c1 + 2 c2 + m1 + m2 + f1 + f2 + e1 + e2 + i1 + i2;"
      "p1 + 2 p2 + 3 c1 + 3 c2 + 4 m1 + 4 m2 + 5 f1 + 5 f2 + 6 e1 + 6 e2 + "
      "7 i1 + 7 i2 <= 100;"
      "0 <= p1 <= 5;"
      "0 <= p2 <= 5;"
      "0 <= c1 <= 5;"
      "0 <= c2 <= 5;"
      "0 <= m1 <= 5;"
      "0 <= m2 <= 5;"
      "2 <= f1 <= 2;"
      "2 <= f2 <= 2;"
      "0 <= e1 <= 1;"
      "0 <= e2 <= 1;"
      "0 <= i1 <= 5;"
      "0 <= i2 <= 5;"
      "int m1, e1, i1, i2;";
  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(linear_program_str, &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);
  sat::MPModelProtoWrapper wrapper(std::move(model_proto), nullptr);
  sat::AddEnforcement(8, 1, wrapper.mutable_constraints(0));

  const DuplicateColumnsPostsolve postsolve =
      MergeExactDuplicateColumns(sat::SatParameters(), &wrapper);
  EXPECT_TRUE(postsolve.empty());
  EXPECT_EQ(wrapper.constraints(0).vars().size(), 12);
}

TEST(MergeExactDuplicateColumnsTest, RespectsKeepAllFeasibleSolutionsParams) {
  const std::string linear_program_str =
      "min: u1 + u2;"
      "u1 + u2 <= 5;"
      "0 <= u1 <= 3;"
      "0 <= u2 <= 4;";
  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(linear_program_str, &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);

  sat::MPModelProtoWrapper wrapper1(model_proto, nullptr);
  sat::SatParameters keep_all_params;
  keep_all_params.set_keep_all_feasible_solutions_in_presolve(true);
  EXPECT_TRUE(MergeExactDuplicateColumns(keep_all_params, &wrapper1).empty());

  sat::MPModelProtoWrapper wrapper2(std::move(model_proto), nullptr);
  sat::SatParameters enum_params;
  enum_params.set_enumerate_all_solutions(true);
  EXPECT_TRUE(MergeExactDuplicateColumns(enum_params, &wrapper2).empty());
}

TEST(MergeExactDuplicateColumnsTest, UpdatesSolutionHint) {
  const std::string linear_program_str =
      "min: u1 + u2;"
      "u1 + u2 <= 10;"
      "0 <= u1 <= 4;"
      "0 <= u2 <= 5;";
  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(linear_program_str, &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);
  // Only hint u2 = 3.5; u1 should receive the merged hint 3.5 and u2 -> 0.0.
  PartialVariableAssignment* hint = model_proto.mutable_solution_hint();
  hint->add_var_index(1);
  hint->add_var_value(3.5);

  sat::MPModelProtoWrapper wrapper(std::move(model_proto), nullptr);
  const DuplicateColumnsPostsolve postsolve =
      MergeExactDuplicateColumns(sat::SatParameters(), &wrapper);
  EXPECT_EQ(postsolve.num_classes(), 1);
  EXPECT_THAT(wrapper.solution_hint().vars(), ElementsAre(1, 0));
  EXPECT_THAT(wrapper.solution_hint().values(), ElementsAre(0.0, 3.5));
}

TEST(MergeExactDuplicateColumnsTest,
     MergesFractionalBoundsWithOutwardTightSumRelaxation) {
  // 5 * double(0.1) is slightly larger than 0.5 in R, so TightSum relaxes the
  // merged upper bound outward by 1 ULP to nextafter(0.5, +inf) instead of
  // refusing to merge or cutting feasible solutions.
  const std::string linear_program_str =
      "min: u1 + u2 + u3 + u4 + u5;"
      "u1 + u2 + u3 + u4 + u5 <= 0.4;"
      "0 <= u1 <= 0.1;"
      "0 <= u2 <= 0.1;"
      "0 <= u3 <= 0.1;"
      "0 <= u4 <= 0.1;"
      "0 <= u5 <= 0.1;";
  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(linear_program_str, &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);
  sat::MPModelProtoWrapper wrapper(std::move(model_proto), nullptr);

  const DuplicateColumnsPostsolve postsolve =
      MergeExactDuplicateColumns(sat::SatParameters(), &wrapper);
  EXPECT_EQ(postsolve.num_classes(), 1);
  EXPECT_DOUBLE_EQ(wrapper.variables(0).lower_bound(), 0.0);
  EXPECT_DOUBLE_EQ(wrapper.variables(0).upper_bound(),
                   std::nextafter(0.5, kInfinity));
  EXPECT_THAT(wrapper.constraints(0).vars(), ElementsAre(0));

  std::vector<double> solution = {0.35, 0.0, 0.0, 0.0, 0.0};
  postsolve.Postsolve(absl::MakeSpan(solution));
  double sum = 0.0;
  for (int v = 0; v < 5; ++v) {
    EXPECT_GE(solution[v], 0.0);
    EXPECT_LE(solution[v], 0.1);
    sum += solution[v];
  }
  EXPECT_NEAR(sum, 0.35, 1e-15);
}

TEST(MergeExactDuplicateColumnsTest,
     SplitsIntoMultipleClassesWhenFiniteBoundSumWouldOverflow) {
  const std::string linear_program_str =
      "min: u1 + u2 + u3 + u4 + u5;"
      "u1 + u2 + u3 + u4 + u5 <= 10;"
      "0 <= u1 <= 8e307;"
      "0 <= u2 <= 8e307;"
      "0 <= u3 <= 8e307;"
      "0 <= u4 <= 8e307;"
      "0 <= u5 <= 8e307;";
  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(linear_program_str, &linear_program));
  MPModelProto model_proto;
  glop::LinearProgramToMPModelProto(linear_program, &model_proto);
  sat::MPModelProtoWrapper wrapper(std::move(model_proto), nullptr);

  const DuplicateColumnsPostsolve postsolve =
      MergeExactDuplicateColumns(sat::SatParameters(), &wrapper);
  EXPECT_EQ(postsolve.num_classes(), 2);
  EXPECT_DOUBLE_EQ(wrapper.variables(0).upper_bound(), 1.6e308);
  EXPECT_DOUBLE_EQ(wrapper.variables(1).upper_bound(), 0.0);
  EXPECT_DOUBLE_EQ(wrapper.variables(2).upper_bound(), 1.6e308);
  EXPECT_DOUBLE_EQ(wrapper.variables(3).upper_bound(), 0.0);
  EXPECT_DOUBLE_EQ(wrapper.variables(4).upper_bound(), 8e307);
  EXPECT_THAT(wrapper.constraints(0).vars(), ElementsAre(0, 2, 4));
}

}  // namespace
}  // namespace operations_research
