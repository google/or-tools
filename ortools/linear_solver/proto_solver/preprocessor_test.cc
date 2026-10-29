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

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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
using ::operations_research::glop::ProblemStatus;
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
  glop::MPModelProtoToLinearProgram(*wrapper.mutable_underlying_model(),
                                    &linear_program);

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
  glop::MPModelProtoToLinearProgram(*wrapper.mutable_underlying_model(),
                                    &linear_program);

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
  glop::MPModelProtoToLinearProgram(*wrapper.mutable_underlying_model(),
                                    &linear_program);
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
  wrapper.mutable_constraints(0)->AddEnforcement(0, 1);

  EXPECT_TRUE(ExactPresolve(sat::SatParameters(), &wrapper));
  // 2*x >= 5 is satisfied for x >= 2.5, so dual reasoning restricts the upper
  // bound of integer x to ceil(2.5) = 3 (not floor(2.5) = 2).
  EXPECT_EQ(wrapper.variables(1).lower_bound(), 0.0);
  EXPECT_EQ(wrapper.variables(1).upper_bound(), 3.0);
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
  wrapper.mutable_constraints(0)->AddEnforcement(0, 1);

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
  glop::MPModelProtoToLinearProgram(*wrapper.mutable_underlying_model(),
                                    &linear_program);
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
  EXPECT_THAT(wrapper.mutable_underlying_model()->solution_hint().var_index(),
              ElementsAre(0, 1));
  EXPECT_THAT(wrapper.mutable_underlying_model()->solution_hint().var_value(),
              ElementsAre(2.0, -1.0));
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
  wrapper.mutable_constraints(0)->AddEnforcement(0, 2);
  wrapper.mutable_constraints(0)->AddEnforcement(1, -2);

  EXPECT_THAT(ShiftVariableBounds(IntegerShifts(), &wrapper),
              ElementsAre(2.0, -2.0));
  // x1 shifted by +2 (to [0, 3]), so enforcement value 2 becomes 0.
  // x2 shifted by -2 (to [-4, 0]), so enforcement value -2 becomes 0.
  EXPECT_THAT(wrapper.constraints(0).enforcement_vars(), ElementsAre(0, 1));
  EXPECT_THAT(wrapper.constraints(0).enforcement_values(), ElementsAre(0, 0));
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
  glop::MPModelProtoToLinearProgram(*wrapper.mutable_underlying_model(),
                                    &linear_program);
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

}  // namespace
}  // namespace operations_research
