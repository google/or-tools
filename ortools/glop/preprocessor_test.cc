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

#include "ortools/glop/preprocessor.h"

#include <ios>
#include <memory>
#include <optional>
#include <random>
#include <string>

#include "absl/random/distributions.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/preprocessor_testing.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_parser.h"
#include "ortools/lp_data/lp_test_utils.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace glop {
namespace {

using ::testing::_;
using ::testing::ContainerEq;

// Note that double is used instead of Fractional to be able to use C-style
// const inline arrays.
template <int NumCols, int NumRows>
struct ExpectedSolution {
  double primal_values[NumCols];
  double dual_values[NumRows];
};

template <int NumCols, int NumRows>
void CheckPreprocessorBehavior(
    absl::string_view input_problem,
    absl::string_view expected_preprocessed_problem,
    const ExpectedSolution<NumCols, NumRows>& expected_solution,
    Preprocessor* preprocessor, Fractional initial_solution_value = 0.0) {
  ASSERT_NE(preprocessor, nullptr);

  // Initialize linear_program to the input_problem and preprocess it.
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(input_problem, &linear_program));
  EXPECT_THAT(preprocessor->Run(&linear_program),
              PreprocessorResultIs(/*postsolve_is_needed=*/_,
                                   /*status=*/std::nullopt));

  // Check the preprocessor output.
  LinearProgram expected_program;
  ASSERT_TRUE(ParseLp(expected_preprocessed_problem, &expected_program));
  EXPECT_EQ(linear_program.IsMaximizationProblem(),
            expected_program.IsMaximizationProblem());
  EXPECT_EQ(linear_program.num_variables(), expected_program.num_variables());
  EXPECT_EQ(linear_program.num_constraints(),
            expected_program.num_constraints());

  // Variable quantities.
  EXPECT_EQ(linear_program.num_variables(), expected_program.num_variables());
  for (ColIndex col(0); col < expected_program.num_variables(); ++col) {
    EXPECT_EQ(linear_program.objective_coefficients()[col],
              expected_program.objective_coefficients()[col]);
    EXPECT_EQ(linear_program.variable_lower_bounds()[col],
              expected_program.variable_lower_bounds()[col]);
    EXPECT_EQ(linear_program.variable_upper_bounds()[col],
              expected_program.variable_upper_bounds()[col]);
    EXPECT_TRUE(linear_program.GetSparseColumn(col).IsEqualTo(
        expected_program.GetSparseColumn(col)));
  }
  EXPECT_EQ(expected_program.objective_offset(),
            linear_program.objective_offset());

  // Row quantities.
  EXPECT_EQ(linear_program.num_constraints(),
            expected_program.num_constraints());
  for (RowIndex row(0); row < expected_program.num_constraints(); ++row) {
    EXPECT_EQ(linear_program.constraint_lower_bounds()[row],
              expected_program.constraint_lower_bounds()[row]);
    EXPECT_EQ(linear_program.constraint_upper_bounds()[row],
              expected_program.constraint_upper_bounds()[row]);
  }

  // Solutions.
  const ColIndex num_cols = linear_program.num_variables();
  const RowIndex num_rows = linear_program.num_constraints();
  SolveStatus solve_status = OptimalSolveStatus();
  ProblemSolution solution(num_rows, num_cols);
  solution.primal_values.assign(num_cols, initial_solution_value);
  preprocessor->RecoverSolution(solve_status, &solution);

  CheckFractionalValues(solution.primal_values, NumCols,
                        expected_solution.primal_values);
  CheckFractionalValues(solution.dual_values, NumRows,
                        expected_solution.dual_values);
}

void CheckPreprocessorStatus(absl::string_view input_problem,
                             Preprocessor* preprocessor,
                             ProblemStatus expected_status) {
  LinearProgram lp;
  ASSERT_TRUE(ParseLp(input_problem, &lp));
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  EXPECT_THAT(preprocessor->Run(&lp),
              PreprocessorResultIs(/*postsolve_is_needed=*/false,
                                   expected_status == ProblemStatus::INIT
                                       ? std::nullopt
                                       : std::make_optional(expected_status)));
}

// --------------------------------------------------------
// ColumnDeletionHelperTest
// --------------------------------------------------------

TEST(ColumnDeletionHelperTest, NoDeletions) {
  ColumnDeletionHelper helper;
  EXPECT_TRUE(helper.GetMarkedColumns().empty());
  EXPECT_TRUE(helper.IsEmpty());

  ProblemSolution solution(RowIndex(0), ColIndex(10));
  solution.primal_values.assign(ColIndex(10), 2.0);
  helper.RestoreDeletedColumns(&solution);
  EXPECT_EQ(ColIndex(10), solution.primal_values.size());
}

TEST(ColumnDeletionHelperTest, DeletionsAnyOrder) {
  ColumnDeletionHelper helper;
  helper.MarkColumnForDeletionWithState(ColIndex(3), 1.0, VariableStatus::FREE);
  helper.MarkColumnForDeletionWithState(ColIndex(2), 2.0, VariableStatus::FREE);
  helper.MarkColumnForDeletionWithState(ColIndex(7), 3.0, VariableStatus::FREE);
  EXPECT_FALSE(helper.IsEmpty());

  const DenseBooleanRow deleted_columns = helper.GetMarkedColumns();
  EXPECT_FALSE(deleted_columns[ColIndex(0)]);
  EXPECT_TRUE(deleted_columns[ColIndex(2)]);
  EXPECT_TRUE(deleted_columns[ColIndex(3)]);
  EXPECT_TRUE(deleted_columns[ColIndex(7)]);
  EXPECT_EQ(ColIndex(8), deleted_columns.size());

  ProblemSolution solution(RowIndex(0), ColIndex(10));
  solution.primal_values.assign(ColIndex(10), -1.0);
  helper.RestoreDeletedColumns(&solution);
  EXPECT_EQ(ColIndex(13), solution.primal_values.size());
  EXPECT_EQ(1.0, solution.primal_values[ColIndex(3)]);
  EXPECT_EQ(2.0, solution.primal_values[ColIndex(2)]);
  EXPECT_EQ(3.0, solution.primal_values[ColIndex(7)]);

  EXPECT_EQ(-1.0, solution.primal_values[ColIndex(0)]);
  EXPECT_EQ(-1.0, solution.primal_values[ColIndex(12)]);
}

// --------------------------------------------------------
// EmptyColumnPreprocessorTest
// --------------------------------------------------------
TEST(EmptyColumnPreprocessorTest, EmptyLinearProgram) {
  const int kNumCols = 0;
  const int kNumRows = 0;
  const std::string kLinearProgram = "";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedResult = {};
  GlopParameters params;
  EmptyColumnPreprocessor preprocessor(&params);
  CheckPreprocessorBehavior(kLinearProgram, kLinearProgram, kExpectedResult,
                            &preprocessor);
}

TEST(EmptyColumnPreprocessorTest, NoObjectiveButEntries) {
  const int kNumCols = 3;
  const int kNumRows = 3;
  const std::string kLinearProgram =
      "max: 0;"
      "10 x + 10 y + 10 z = 0;"
      "=0;"
      "=0;"
      "0 <= x <= 10;"
      "0 <= y <= 10;"
      "0 <= z <= 10;";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedResult = {
      {0, 0, 0},  // Solution values
      {0, 0, 0}   // Dual values
  };
  GlopParameters params;
  EmptyColumnPreprocessor preprocessor(&params);
  CheckPreprocessorBehavior(kLinearProgram, kLinearProgram, kExpectedResult,
                            &preprocessor);
}

TEST(EmptyColumnPreprocessorTest, OneUnusedColumn) {
  const int kNumCols = 3;
  const int kNumRows = 3;
  // NOTE(user): stating all variables in the objective enforces the order of
  // columns in LinearProgram.
  // NOTE(user): using "=0;" creates an empty constraint, which is required
  // for the number of dual values to be correct.
  const std::string kLinearProgram =
      "max: 0 + 0 x + 0 y + 2 z;"
      "x + z = 0;"
      "=0;"
      "=0;"
      "1 <= x <= 5;"
      "2 <= y <= 6;"
      "3 <= z <= 7;";
  const std::string kOutputProgram =
      "max: 0 + 0 x + 2 z;"
      "x + z = 0;"
      "=0;"
      "=0;"
      "1 <= x <= 5;"
      "3 <= z <= 7;";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedResult = {
      {0, 6, 0},  // Solution values
      {0, 0, 0}   // Dual values
  };
  GlopParameters params;
  EmptyColumnPreprocessor preprocessor(&params);
  CheckPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                            &preprocessor);
}

TEST(EmptyColumnPreprocessorTest, ThreeUnusedColumnsMaximize) {
  const int kNumCols = 5;
  const int kNumRows = 3;
  const std::string kLinearProgram =
      "max: 0 + 0 x1 + 0 x2 + x3 + 0 x4 + 2 x5;"
      "x1 + x5 = 0;"
      "=0;"
      "=0;"
      "1 <= x1 <= 5;"
      "2 <= x2 <= 6;"
      "3 <= x3 <= 7;"
      "4 <= x4 <= 8;"
      "5 <= x5 <= 9;";
  const std::string kOutputProgram =
      "max: 7 + 0 x1 + 2 x5;"
      "x1 + x5 = 0;"
      "=0;"
      "=0;"
      "1 <= x1 <= 5;"
      "5 <= x5 <= 9;";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedResult = {
      {0, 6, 7, 8, 0},  // Solution values
      {0, 0, 0}         // Dual values
  };
  GlopParameters params;
  EmptyColumnPreprocessor preprocessor(&params);
  CheckPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                            &preprocessor);
}

TEST(EmptyColumnPreprocessorTest, ThreeUnusedColumnsMinimize) {
  const int kNumCols = 5;
  const int kNumRows = 3;
  const std::string kLinearProgram =
      "min: 0 + 0 x1 + 0 x2 + x3 + 0 x4 + 2 x5;"
      "x1 + x5 = 0;"
      "=0;"
      "=0;"
      "1 <= x1 <= 5;"
      "2 <= x2 <= 6;"
      "3 <= x3 <= 7;"
      "4 <= x4 <= 8;"
      "5 <= x5 <= 9;";
  const std::string kOutputProgram =
      "min: 3 + 0 x1 + 2 x5;"
      "x1 + x5 = 0;"
      "=0;"
      "=0;"
      "1 <= x1 <= 5;"
      "5 <= x5 <= 9;";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedResult = {
      {0, 6, 3, 8, 0},  // Solution values
      {0, 0, 0}         // Dual values
  };
  GlopParameters params;
  EmptyColumnPreprocessor preprocessor(&params);
  CheckPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                            &preprocessor);
}

TEST(EmptyColumnPreprocessorTest, NoEntriesMaximize) {
  const int kNumCols = 3;
  const int kNumRows = 3;
  const std::string kLinearProgram =
      "max: 0 + x - y + z;"
      "=0;"
      "=0;"
      "=0;"
      "1 <= x <= 7;"
      "2 <= y <= 8;"
      "3 <= z <= 9;";
  const std::string kOutputProgram =
      "max: 14;"
      "=0;"
      "=0;"
      "=0;";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedResult = {
      {7, 2, 9},  // Solution values
      {0, 0, 0}   // Dual values
  };
  GlopParameters params;
  EmptyColumnPreprocessor preprocessor(&params);
  CheckPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                            &preprocessor);
}

TEST(EmptyColumnPreprocessorTest, NoEntriesMinimize) {
  const int kNumCols = 3;
  const int kNumRows = 3;
  const std::string kLinearProgram =
      "min: 0 + x - y + z;"
      "=0;"
      "=0;"
      "=0;"
      "1 <= x <= 7;"
      "2 <= y <= 8;"
      "3 <= z <= 9;";
  const std::string kOutputProgram =
      "min: -4;"
      "=0;"
      "=0;"
      "=0;";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedResult = {
      {1, 8, 3},  // Solution values
      {0, 0, 0}   // Dual values
  };
  GlopParameters params;
  EmptyColumnPreprocessor preprocessor(&params);
  CheckPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                            &preprocessor);
}

TEST(EmptyColumnPreprocessorTest, OneUnusedColumnMaximize) {
  const int kNumCols = 3;
  const int kNumRows = 3;
  const std::string kLinearProgram =
      "max: 0 + 0 x + 1 y + 2 z;"
      "x + z = 0;"
      "=0;"
      "=0;"
      "1 <= x <= 5;"
      "2 <= y <= 6;"
      "3 <= z <= 7;";
  const std::string kOutputProgram =
      "max: 6 + 0 x + 2 z;"
      "x + z = 0;"
      "=0;"
      "=0;"
      "1 <= x <= 5;"
      "3 <= z <= 7;";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedResult = {
      {0, 6, 0},  // Solution values
      {0, 0, 0}   // Dual values
  };
  GlopParameters params;
  EmptyColumnPreprocessor preprocessor(&params);
  CheckPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                            &preprocessor);
}

TEST(EmptyColumnPreprocessorTest, OneUnusedColumnMinimize) {
  const int kNumCols = 3;
  const int kNumRows = 3;
  const std::string kLinearProgram =
      "min: 0 + 0 x + 1 y + 2 z;"
      "x + z = 0;"
      "=0;"
      "=0;"
      "1 <= x <= 5;"
      "2 <= y <= 6;"
      "3 <= z <= 7;";
  const std::string kOutputProgram =
      "min: 2 + 0 x + 2 z;"
      "x + z = 0;"
      "=0;"
      "=0;"
      "1 <= x <= 5;"
      "3 <= z <= 7;";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedResult = {
      {0, 2, 0},  // Solution values
      {0, 0, 0}   // Dual values
  };
  GlopParameters params;
  EmptyColumnPreprocessor preprocessor(&params);
  CheckPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                            &preprocessor);
}

// --------------------------------------------------------
// FixedVariablePreprocessorTest
// --------------------------------------------------------

TEST(FixedVariablePreprocessorTest, NoFixedColumns) {
  const std::string kLinearProgram =
      "min: 0 + 0 x1 + 0 x2 + 1 x3 + 0 x4 + 2 x5;"
      "x1 + x5 = 0;"
      "x2 + 2 x5 = 0;"
      "x3 + 3 x5 = 0;"
      " 1 <= x1 <= 5;"
      " 2 <= x2 <= 6;"
      " 1 <= x3 <= 3;"
      " 4 <= x4 <= 8;"
      "-8 <= x5 <= -2;";
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(kLinearProgram, &linear_program));
  GlopParameters params;
  FixedVariablePreprocessor preprocessor(&params);
  preprocessor.SetTimeLimit(time_limit.get());
  EXPECT_THAT(preprocessor.Run(&linear_program),
              PreprocessorResultIs(/*postsolve_is_needed=*/false,
                                   /*status=*/std::nullopt));
}

TEST(FixedVariablePreprocessorTest, TwoFixedColumn) {
  const std::string kLinearProgram =
      "min: 0 + 0 x1 + 0 x2 + 0 x3 + 0 x4 + 2 x5;"
      "x1 + x5 = 0;"
      "x2 + 2 x5 = 0;"
      "x3 + 3 x5 = 0;"
      " 1 <= x1 <= 5;"
      " 2 <= x2 <= 6;"
      " 3 <= x3 <= 3;"  // Fixed.
      " 4 <= x4 <= 8;"
      "-2 <= x5 <= -2;";  // Fixed.
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(kLinearProgram, &linear_program));
  GlopParameters params;
  FixedVariablePreprocessor preprocessor(&params);
  preprocessor.SetTimeLimit(time_limit.get());
  EXPECT_THAT(preprocessor.Run(&linear_program),
              PreprocessorResultIs(/*postsolve_is_needed=*/true,
                                   /*status=*/std::nullopt));

  // SimpleOutputCheck.
  EXPECT_EQ(3, linear_program.num_variables());
  EXPECT_EQ(2.0, linear_program.constraint_lower_bounds()[RowIndex(0)]);
  EXPECT_EQ(4.0, linear_program.constraint_lower_bounds()[RowIndex(1)]);
  EXPECT_EQ(6.0 - 3.0, linear_program.constraint_lower_bounds()[RowIndex(2)]);
  for (RowIndex row(0); row < 3; ++row) {
    EXPECT_EQ(linear_program.constraint_lower_bounds()[row],
              linear_program.constraint_upper_bounds()[row]);
  }

  SolveStatus solve_status = OptimalSolveStatus();
  ProblemSolution solution(RowIndex(3), ColIndex(3));
  preprocessor.RecoverSolution(solve_status, &solution);

  DenseRow expected_primal_values({0.0, 0.0, 3.0, 0.0, -2.0});
  EXPECT_THAT(solution.primal_values, ContainerEq(expected_primal_values));

  DenseColumn expected_dual_values({0.0, 0.0, 0.0});
  EXPECT_THAT(solution.dual_values, ContainerEq(expected_dual_values));
}

// --------------------------------------------------------
// ForcingAndImpliedFreeConstraintPreprocessor
// --------------------------------------------------------
TEST(ForcingAndImpliedFreeConstraintPreprocessorTest,
     DoesNotCauseFalseInfeasibility) {
  const std::string kLinearProgram =
      "min: x + y;"
      "-x + y = 0.0;"
      "x - y = 0.0;"
      "4.7299999999999818101059645e+02 <= x <= 4.73e+02;"
      "4.7299999999999818101059645e+02 <= y <= 4.73e+02;";
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(kLinearProgram, &linear_program));
  GlopParameters params;
  ForcingAndImpliedFreeConstraintPreprocessor preprocessor(&params);
  preprocessor.SetTimeLimit(time_limit.get());
  EXPECT_THAT(preprocessor.Run(&linear_program),
              PreprocessorResultIs(/*postsolve_is_needed=*/true,
                                   /*status=*/std::nullopt));
}

// --------------------------------------------------------
// ImpliedFreePreprocessorTest
// --------------------------------------------------------

TEST(ImpliedFreePreprocessorTest, DetectImpliedFreeVariable) {
  const int kNumCols = 3;
  const int kNumRows = 4;
  const std::string kLinearProgram =
      "int x, y, z;"
      "min: 1 + 2x + y;"
      "4 <= 2x + z <= 4;"
      "3 <= x + 2y + 2z <= 3;"
      "1 <= 2z + y <= 10;"
      "0 <= 3z + y <= 10;"
      "0 <= x <= 100;"
      "0 <= y <= 2;"
      "0 <= z <= 2;";
  // x is implied free variable with implied bounds [1 , 2].
  const std::string kOutputProgram =
      "int x, y, z;"
      "min: 1 + 2x + y;"
      "4 <= 2x + z <= 4;"
      "3 <= x + 2y + 2z <= 3;"
      "1 <= 2z + y <= 10;"
      "0 <= 3z + y <= 10;"
      "-inf <= x <= inf;"
      "0 <= y <= 2;"
      "0 <= z <= 2;";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedResult = {
      {0, 0, 0},    // Solution values
      {0, 0, 0, 0}  // Dual values
  };

  GlopParameters params;
  ImpliedFreePreprocessor preprocessor(&params);
  CheckPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                            &preprocessor);
}

TEST(ImpliedFreePreprocessorTest, OffsetImpliedFreeVariable) {
  const int kNumCols = 3;
  const int kNumRows = 4;
  const std::string kLinearProgram =
      "int x, y, z;"
      "min: 1 + 2x + y;"
      "4 <= 2x + z <= 4;"
      "3 <= x + 2y + 2z <= 3;"
      "1 <= 2z + y <= 10;"
      "0 <= 3z + y <= 10;"
      "-10 <= x <= 100;"
      "0 <= y <= 2;"
      "0 <= z <= 2;";
  // x is implied free variable with implied bounds [1 , 2]. Here the lower
  // bound of x is -10. Hence x is shifted by 10 and all the bounds of
  // constraints involving x and objective offset are modified.
  const std::string kOutputProgram =
      "int x, y, z;"
      "min: -19 + 2x + y;"
      "24 <= 2x + z <= 24;"
      "13 <= x + 2y + 2z <= 13;"
      "1 <= 2z + y <= 10;"
      "0 <= 3z + y <= 10;"
      "-inf <= x <= inf;"
      "0 <= y <= 2;"
      "0 <= z <= 2;";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedResult = {
      {-10, 0, 0},  // Solution values (x is fixed to its lower bound)
      {0, 0, 0, 0}  // Dual values
  };

  GlopParameters params;
  ImpliedFreePreprocessor preprocessor(&params);
  CheckPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                            &preprocessor);
}

TEST(ImpliedFreePreprocessorTest, InfeasibilityDetection) {
  const std::string kLinearProgram =
      "int x, y, z;"
      "min: 1 + 2x + y;"
      "4 <= 2x + z <= 4;"
      "3 <= x + 2y + 2z <= 3;"
      "1 <= 2z + y <= 10;"
      "0 <= 3z + y <= 10;"
      "3 <= x <= 4;"
      "0 <= y <= 2;"
      "0 <= z <= 2;";
  // x is implied free variable with implied bounds [1 , 2]. Here the lower
  // bound of x is 3. Hence the LP is infeasible.

  GlopParameters params;
  ImpliedFreePreprocessor preprocessor(&params);
  CheckPreprocessorStatus(kLinearProgram, &preprocessor,
                          ProblemStatus::PRIMAL_INFEASIBLE);
}

TEST(ImpliedFreePreprocessorTest, Disabled) {
  const std::string kLinearProgram =
      "int x, y, z;"
      "min: 1 + 2x + y;"
      "4 <= 2x + z <= 4;"
      "3 <= x + 2y + 2z <= 3;"
      "1 <= 2z + y <= 10;"
      "0 <= 3z + y <= 10;"
      "0 <= x <= 100;"
      "0 <= y <= 2;"
      "0 <= z <= 2;";
  // x is implied free variable with implied bounds [1 , 2].

  for (bool param_value : {true, false}) {
    LinearProgram lp;
    ASSERT_TRUE(ParseLp(kLinearProgram, &lp));
    GlopParameters params;
    params.set_use_implied_free_preprocessor(param_value);
    ImpliedFreePreprocessor preprocessor(&params);
    EXPECT_THAT(preprocessor.Run(&lp),
                PreprocessorResultIs(/*postsolve_is_needed=*/param_value,
                                     /*status=*/std::nullopt))
        << "param_value: " << std::boolalpha << param_value;
  }
}

// --------------------------------------------------------
// SingletonColumnSignPreprocessorTest
// --------------------------------------------------------

TEST(SingletonColumnSignPreprocessorTest, EmptyProgram) {
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  LinearProgram linear_program;
  GlopParameters params;
  SingletonColumnSignPreprocessor preprocessor(&params);
  preprocessor.SetTimeLimit(time_limit.get());
  EXPECT_THAT(preprocessor.Run(&linear_program),
              PreprocessorResultIs(/*postsolve_is_needed=*/false,
                                   /*status=*/std::nullopt));
}

TEST(SingletonColumnSignPreprocessorTest, ChangeToPositiveSign) {
  const int kNumCols = 4;
  const int kNumRows = 3;
  const std::string kLinearProgram =
      "min: 0 + 0 x1 + 3 x2 + 1 x3 + 0 x4;"
      "r1: x1 = 0;"
      "r2: -x2 - x4 = 0;"
      "r3: x3 = 0;"
      "1 <= x1 <= 5;"
      "2 <= x2 <= 6;"
      "1 <= x3 <= 9;"
      "4 <= x4 <= 8;";
  const std::string kOutputProgram =
      "min: 0 + 0 x1 - 3 x2 + 1 x3 + 0 x4;"
      "r1: x1 = 0;"
      "r2: x2 + x4 = 0;"
      "r3: x3 = 0;"
      "1 <= x1 <= 5;"
      "-6 <= x2 <= -2;"
      "1 <= x3 <= 9;"
      "-8 <= x4 <= -4;";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedResult = {
      {1.0, -1.0, 1.0, -1.0},  // Solution values
      {0, 0, 0}                // Dual values
  };
  GlopParameters params;
  SingletonColumnSignPreprocessor preprocessor(&params);
  CheckPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                            &preprocessor, 1.0);  // Initial solution value.
}

// --------------------------------------------------------
// SingletonPreprocessorTest
// --------------------------------------------------------

// This makes sure that we don't have any quadratic behavior when many singleton
// columns with a non-zero cost appear in the same constraint (like we used to
// have !). Note that if we are quadratic, this test will just TIMEOUT.
TEST(SingletonPreprocessorTest, NonQuadraticPerformance) {
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  std::mt19937 random(12345);
  LinearProgram lp;
  const RowIndex row = lp.CreateNewConstraint();
  const int kNumVariables = 500000;
  lp.SetConstraintBounds(row, 0, kNumVariables / 2);
  for (int i = 0; i < kNumVariables; ++i) {
    const ColIndex col = lp.CreateNewVariable();
    lp.SetVariableBounds(col, 0, 1);
    lp.SetObjectiveCoefficient(col, absl::Uniform<double>(random, 0.0, 1.0));
    lp.SetCoefficient(row, col, absl::Uniform<double>(random, 0.0, 1.0));
  }
  GlopParameters params;
  SingletonPreprocessor preprocessor(&params);
  preprocessor.SetTimeLimit(time_limit.get());
  EXPECT_THAT(preprocessor.Run(&lp),
              PreprocessorResultIs(/*postsolve_is_needed=*/false,
                                   /*status=*/std::nullopt));
}

TEST(SingletonPreprocessorTest, SingletonRowInfeasibilityTest) {
  const std::string kLinearProgram =
      "1 <= x + y <= 2;"
      "1 <= x + 2y <= 5;"
      "2.4 <= 2x <= 6.2;"  // Singleton Row
      "0 <= x <= 1.1;";
  // This LP is infeasible since the implied lower bound for x is 1.2 which is
  // larger than the upper bound of x.
  GlopParameters params;
  SingletonPreprocessor preprocessor(&params);
  CheckPreprocessorStatus(kLinearProgram, &preprocessor,
                          ProblemStatus::PRIMAL_INFEASIBLE);
}

// This used to fail before CL 154811904.
TEST(SingletonPreprocessorTest, ProperToleranceHandling) {
  const std::string kLinearProgram =
      "r1: 4e-16 <= 1e-16 x;"  // Problematic singleton row.
      "r2: 0 <= x;"  // Needed so that the singleton row code is triggered.
      "0 <= x <= 3;";

  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(kLinearProgram, &linear_program));
  GlopParameters params;
  SingletonPreprocessor preprocessor(&params);
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  preprocessor.SetTimeLimit(time_limit.get());
  EXPECT_THAT(preprocessor.Run(&linear_program),
              PreprocessorResultIs(/*postsolve_is_needed=*/true,
                                   /*status=*/std::nullopt));
  EXPECT_THAT(preprocessor.Run(&linear_program),
              PreprocessorResultIs(/*postsolve_is_needed=*/true,
                                   /*status=*/std::nullopt));
}

TEST(SingletonPreprocessorTest, SingletonRowDeletionWithIntegerVariable) {
  const int kNumCols = 2;
  const int kNumRows = 3;
  const std::string kLinearProgram =
      "int x;"
      "1 <= x + y <= 2;"
      "1 <= x + 2y <= 5;"
      "4 <= 2x <= 6;"  // Singleton Row
      "0 <= x <= 7;";
  // Singleton row is removed and the bounds of x are updated. The bounds are
  // rounded to integer bounds since x is an integer variable.
  const std::string kOutputProgram =
      "int x;"
      "1 <= x + y <= 2;"
      "1 <= x + 2y <= 5;"
      "2 <= x <= 3;";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedResult = {
      {0, 0},    // Solution values
      {0, 0, 0}  // Dual values
  };
  GlopParameters params;
  SingletonPreprocessor preprocessor(&params);
  preprocessor.UseInMipContext();
  CheckPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                            &preprocessor);
}

TEST(SingletonPreprocessorTest,
     SingletonRowWithIntegerVariableInfeasibilityTest) {
  const std::string kLinearProgram =
      "int x;"
      "1 <= x + y <= 2;"
      "1 <= x + 2y <= 5;"
      "4 <= 2x <= 6;"  // Singleton Row
      "0 <= x <= 1.5;";

  // We use 1.5 for the upper bound to exercise the integer rounding part of the
  // SingetonPreprocessor (using 1.0 would make not only the MIP, but the LP
  // infeasible too).

  // This LP is infeasible since the implied lower bound for x is 2 which is
  // larger than the upper bound of x.
  GlopParameters params;
  SingletonPreprocessor preprocessor(&params);
  preprocessor.UseInMipContext();
  CheckPreprocessorStatus(kLinearProgram, &preprocessor,
                          ProblemStatus::PRIMAL_INFEASIBLE);
}

TEST(SingletonPreprocessorTest, RemovalOfSingletonIntegerVariable) {
  const int kNumCols = 3;
  const int kNumRows = 2;
  const std::string kLinearProgram =
      "int x, y, z;"
      "min: x + 2y;"
      "6 <= 2x + 4y + 4z <= 6;"
      "5 <= y - z <= 6;"
      "y <= 2;"
      "z >= 0;";
  // x is a singleton variable with cost 1. x is removed from the program
  // and the constraint bounds are updated. In the solution x = 3 - 2y - 2z. The
  // dual value is updated as cost / coefficient = 1/2.
  const std::string kOutputProgram =
      "int y, z;"
      "min: 3 - 2z;"
      "-inf <= 4y + 4z <= inf;"
      "5 <= y - z <= 6;"
      "y <= 2;"
      "z >= 0;";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedResult = {
      {3, 0, 0},  // Solution values
      {0.5, 0}    // Dual values
  };
  GlopParameters params;
  SingletonPreprocessor preprocessor(&params);
  preprocessor.UseInMipContext();
  CheckPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                            &preprocessor);
}

TEST(SingletonPreprocessorTest, RemovalOfSingletonIntegerVariableWithZeroCost) {
  const int kNumCols = 3;
  const int kNumRows = 2;
  const std::string kLinearProgram =
      "int x, y, z;"
      "min: 2y;"
      "6 <= 2x + 4y + 4z <= 8;"
      "5 <= y - z <= 6;"
      "y <= 2;"
      "z >= 0;";
  // x is a singleton variable with cost 0. x is removed from the program
  // and the constraint bounds are updated.
  // Here 3 - 2y - 2z <= x <= 4 - 2y - 2z. In the solution the value of x is
  // updated to its upper bound and the dual values are not changed.
  const std::string kOutputProgram =
      "int y, z;"
      "min: 2y;"
      "-inf <= 4y + 4z <= inf;"
      "5 <= y - z <= 6;"
      "y <= 2;"
      "z >= 0;";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedResult = {
      {4, 0, 0},  // Solution values
      {0, 0}      // Dual values
  };
  GlopParameters params;
  SingletonPreprocessor preprocessor(&params);
  preprocessor.UseInMipContext();
  CheckPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                            &preprocessor);
}

TEST(SingletonPreprocessorTest,
     NoRemovalOfSingletonIntegerVariableWithZeroCost) {
  const int kNumCols = 3;
  const int kNumRows = 2;
  // x is a singleton integer variable but since z is not integer, it is not
  // removed.
  const std::string kLinearProgram =
      "int x, y;"
      "6 <= 2x + 4y + 4z <= 8;"
      "5 <= y - z <= 6;"
      "y <= 2;"
      "z >= 0;";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedResult = {
      {0, 0, 0},  // Solution values
      {0, 0}      // Dual values
  };
  GlopParameters params;
  SingletonPreprocessor preprocessor(&params);
  preprocessor.UseInMipContext();
  CheckPreprocessorBehavior(kLinearProgram, kLinearProgram, kExpectedResult,
                            &preprocessor);
}

TEST(SingletonPreprocessorTest,
     NoRemovalOfSingletonIntegerVariableWithZeroCost2) {
  const int kNumCols = 3;
  const int kNumRows = 2;
  //  x is a singleton integer variable but since coefficient of y is not
  //  divisible by coefficient of x, it is not removed.
  const std::string kLinearProgram =
      "int x, y, z;"
      "6 <= 2x + 3y + 4z <= 8;"
      "5 <= y - z <= 6;"
      "y <= 2;"
      "z >= 0;";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedResult = {
      {0, 0, 0},  // Solution values
      {0, 0}      // Dual values
  };
  GlopParameters params;
  SingletonPreprocessor preprocessor(&params);
  preprocessor.UseInMipContext();
  CheckPreprocessorBehavior(kLinearProgram, kLinearProgram, kExpectedResult,
                            &preprocessor);
}

TEST(SingletonPreprocessorTest,
     NoRemovalOfSingletonIntegerVariableWithZeroCost3) {
  const int kNumCols = 3;
  const int kNumRows = 2;
  //  x is a singleton integer variable but since constraint upper bound is not
  //  divisible by coefficient of x, it is not removed.
  const std::string kLinearProgram =
      "int x, y, z;"
      "6 <= 2x + 4y + 4z <= 7;"
      "5 <= y - z <= 6;"
      "y <= 2;"
      "z >= 0;";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedResult = {
      {0, 0, 0},  // Solution values
      {0, 0}      // Dual values
  };
  GlopParameters params;
  SingletonPreprocessor preprocessor(&params);
  preprocessor.UseInMipContext();
  CheckPreprocessorBehavior(kLinearProgram, kLinearProgram, kExpectedResult,
                            &preprocessor);
}

TEST(SingletonPreprocessorTest,
     NoRemovalOfSingletonIntegerVariableWithZeroCost4) {
  const int kNumCols = 3;
  const int kNumRows = 2;
  //  x is a singleton integer variable but since constraint lower bound is not
  //  divisible by coefficient of x, it is not removed.
  const std::string kLinearProgram =
      "int x, y, z;"
      "5 <= 2x + 4y + 4z <= 8;"
      "5 <= y - z <= 6;"
      "y <= 2;"
      "z >= 0;";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedResult = {
      {0, 0, 0},  // Solution values
      {0, 0}      // Dual values
  };
  GlopParameters params;
  SingletonPreprocessor preprocessor(&params);
  preprocessor.UseInMipContext();
  CheckPreprocessorBehavior(kLinearProgram, kLinearProgram, kExpectedResult,
                            &preprocessor);
}

// --------------------------------------------------------
// ProportionalColumnPreprocessor
// --------------------------------------------------------
TEST(ProportionalColumnPreprocessorTest, MergeProportionalColumn) {
  const int kNumCols = 3;
  const int kNumRows = 2;
  const std::string kLinearProgram =
      "min: x + 5y + 7z;"
      "3 <= x + 5y + z <= 10;"
      "2 <= 2x + 10y + z <= 100;"
      "0 <= x <= 2;"
      "0 <= y <= 2;"
      "0 <= z <= 12;";
  const std::string kOutputProgram =
      "min: x + 7z;"
      "3 <= x + z <= 10;"
      "2 <= 2x + z <= 100;"
      "0 <= x <= 12;"
      "0 <= z <= 12;";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedResult = {
      {2, 0.8, 6},  // Solution values
      {0, 0}        // Dual values
  };
  GlopParameters params;
  ProportionalColumnPreprocessor preprocessor(&params);
  CheckPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                            &preprocessor, 6.0);
}

// --------------------------------------------------------
// DoubletonFreeColumnPreprocessor
// --------------------------------------------------------

TEST(DoubletonFreeColumnPreprocessorTest, DoubletonFreeColumnRemoval) {
  const int kNumCols = 3;
  const int kNumRows = 2;
  const std::string kLinearProgram =
      "min: x + 2y;"
      "1 <= x + 4y + 2z <= 6;"
      "3 <= 2x + y + 3z <= 7;"
      "1 <= x <= 5;"
      "1 <= z <= 5;";
  const std::string kOutputProgram =
      "min: 0.5x + 2y - z;"
      "3 <= 1.75x + y + 2.5z <= 7;"
      "1 <= x <= 5;"
      "0.25 <= y <= 1.5;"
      "1 <= z <= 5;";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedResult = {
      {0, 0, 0},  // Solution values
      {0.5, 0}    // Dual values
  };
  GlopParameters params;
  DoubletonFreeColumnPreprocessor preprocessor(&params);
  CheckPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                            &preprocessor);
}

// --------------------------------------------------------
// UnconstrainedVariablePreprocessorTest
// --------------------------------------------------------

TEST(UnconstrainedVariablePreprocessorTest, SingletonColumns) {
  const std::string kLinearProgram =
      "min: x + 2y - 3z;"
      "x + y - z = 0;"
      "0 <= x <= 100;"
      "0 <= y <= 100;"
      "z >= 0;";
  const std::string kOutputProgram =
      "min: 300 - 3z;"
      "r0: -z = -200;"
      "z >= 0;";
  const ExpectedSolution<3, 1> kExpectedResult = {
      {100, 100, 0},  // Solution values
      {0}             // Dual values
  };
  GlopParameters params;
  UnconstrainedVariablePreprocessor preprocessor(&params);
  CheckPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                            &preprocessor);
}

TEST(UnconstrainedVariablePreprocessorTest, UncontrainedIntegerColumn) {
  // z can be as large a possible (not counting the domain), so we can fix it
  // to 100. This works for integers too.
  const std::string kLinearProgram =
      "min: x + y - z;"
      "x + y = 50;"
      "y + z >= 50;"
      "0 <= x <= 100;"
      "0 <= y <= 100;"
      "0 <= z <= 100;"
      "int x,y,z;";
  const std::string kOutputProgram =
      "min:-100 + x + y;"
      "r0: + x + y = 50;"
      "r1: + y >= -50;"
      "0 <= x <= 100;"
      "0 <= y <= 100;"
      "int x,y;";
  const ExpectedSolution<3, 2> kExpectedResult = {
      {0, 0, 100},  // Solution values
      {0, 0}        // Dual values
  };
  GlopParameters params;
  UnconstrainedVariablePreprocessor preprocessor(&params);
  preprocessor.UseInMipContext();
  CheckPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                            &preprocessor);
}

TEST(UnconstrainedVariablePreprocessorTest, InfeasibilityTolerance) {
  const std::string kLinearProgram =
      "min: 1e-4 x;"
      " x <= 0;";
  GlopParameters params;
  UnconstrainedVariablePreprocessor preprocessor(&params);
  CheckPreprocessorStatus(kLinearProgram, &preprocessor,
                          ProblemStatus::INFEASIBLE_OR_UNBOUNDED);
}

// Because in the presolve we work on the unscaled problem, we are quite
// defensive to detect infeasibility. This will still be reported as unbounded
// by the simplex code, but not by the presolver.
TEST(UnconstrainedVariablePreprocessorTest, InfeasibilityToleranceIsDefensive) {
  const std::string kLinearProgram =
      "min: 1e-5 x;"
      " x <= 0;";
  GlopParameters params;
  UnconstrainedVariablePreprocessor preprocessor(&params);
  CheckPreprocessorStatus(kLinearProgram, &preprocessor, ProblemStatus::INIT);
}

// --------------------------------------------------------
// DoubletonEqualityRowPreprocessorTest
// --------------------------------------------------------

TEST(DoubletonEqualityRowPreprocessorTest, DoubletonEqualityRowRemoval) {
  const int kNumCols = 3;
  const int kNumRows = 2;
  const std::string kLinearProgram =
      "min: x + 2y;"
      "3 <= 2x + 4y + 4z <= 6;"
      "6 <= y + 2z <= 6;"
      "y <= 2;"
      "z >= 0;";
  // y + 2z = 6 is a doubleton equality row. We substitute y = 6 - 2z here.
  const std::string kOutputProgram =
      "min: 12 + x - 4z;"
      "-21 <= 2x - 4z <= -18;"
      "z >= 2;";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedResult = {
      {0, 6, 0},  // Solution values
      {0, 2}      // Dual values
  };
  GlopParameters params;
  DoubletonEqualityRowPreprocessor preprocessor(&params);
  CheckPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                            &preprocessor);
}

// --------------------------------------------------------
// DualizerPreprocessorTest
// --------------------------------------------------------

// Creates a trivial program of a given size.
void SetLinearProgramSize(int num_rows, int num_cols, LinearProgram* program) {
  program->Clear();
  for (int i = 0; i < num_rows; ++i) {
    program->FindOrCreateConstraint(absl::StrFormat("row_%d", i));
  }
  for (int i = 0; i < num_cols; ++i) {
    program->FindOrCreateVariable(absl::StrFormat("col_%d", i));
  }
}

TEST(DualizerPreprocessorTest, ParameterTest) {
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  LinearProgram linear_program;
  GlopParameters params;

  {
    params.set_solve_dual_problem(GlopParameters::ALWAYS_DO);
    DualizerPreprocessor preprocessor(&params);
    preprocessor.SetTimeLimit(time_limit.get());
    SetLinearProgramSize(3, 1, &linear_program);
    EXPECT_THAT(preprocessor.Run(&linear_program),
                PreprocessorResultIs(/*postsolve_is_needed=*/true,
                                     /*status=*/std::nullopt));
    EXPECT_EQ(1, linear_program.num_constraints());
  }
  {
    params.set_solve_dual_problem(GlopParameters::NEVER_DO);
    DualizerPreprocessor preprocessor(&params);
    preprocessor.SetTimeLimit(time_limit.get());
    SetLinearProgramSize(3, 1, &linear_program);
    EXPECT_THAT(preprocessor.Run(&linear_program),
                PreprocessorResultIs(/*postsolve_is_needed=*/false,
                                     /*status=*/std::nullopt));
    EXPECT_EQ(3, linear_program.num_constraints());
  }
  {
    params.set_solve_dual_problem(GlopParameters::LET_SOLVER_DECIDE);
    DualizerPreprocessor preprocessor(&params);
    preprocessor.SetTimeLimit(time_limit.get());
    SetLinearProgramSize(3, 1, &linear_program);
    EXPECT_THAT(preprocessor.Run(&linear_program),
                PreprocessorResultIs(/*postsolve_is_needed=*/true,
                                     /*status=*/std::nullopt));
    EXPECT_EQ(1, linear_program.num_constraints());

    SetLinearProgramSize(3, 50, &linear_program);
    EXPECT_THAT(preprocessor.Run(&linear_program),
                PreprocessorResultIs(/*postsolve_is_needed=*/false,
                                     /*status=*/std::nullopt));
    EXPECT_EQ(3, linear_program.num_constraints());
  }
}

TEST(DualizerPreprocessorTest, RecoverSolution) {
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  GlopParameters params;
  params.set_solve_dual_problem(GlopParameters::ALWAYS_DO);
  DualizerPreprocessor preprocessor(&params);
  preprocessor.SetTimeLimit(time_limit.get());

  LinearProgram linear_program;
  SetLinearProgramSize(2, 3, &linear_program);
  EXPECT_THAT(preprocessor.Run(&linear_program),
              PreprocessorResultIs(/*postsolve_is_needed=*/true,
                                   /*status=*/std::nullopt));

  EXPECT_EQ(RowIndex(3), linear_program.num_constraints());
  EXPECT_EQ(ColIndex(5), linear_program.num_variables());
  SolveStatus solve_status = OptimalSolveStatus();
  ProblemSolution solution(RowIndex(3), ColIndex(5));
  solution.primal_values.assign(ColIndex(5), 0.0);
  solution.dual_values.assign(RowIndex(3), 2.0);
  preprocessor.RecoverSolution(solve_status, &solution);
  EXPECT_EQ(3, solution.primal_values.size());
  EXPECT_EQ(2.0, solution.primal_values[ColIndex(0)]);
  EXPECT_EQ(2.0, solution.primal_values[ColIndex(1)]);
  EXPECT_EQ(2.0, solution.primal_values[ColIndex(2)]);
  EXPECT_EQ(2, solution.dual_values.size());
  EXPECT_EQ(0.0, solution.dual_values[RowIndex(0)]);
  EXPECT_EQ(0.0, solution.dual_values[RowIndex(1)]);
}

TEST(DualizerPreprocessorTest, RecoverSolutionMoreComplexCase) {
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  GlopParameters params;
  params.set_solve_dual_problem(GlopParameters::ALWAYS_DO);
  DualizerPreprocessor preprocessor(&params);
  preprocessor.SetTimeLimit(time_limit.get());

  LinearProgram linear_program;
  SetLinearProgramSize(2, 5, &linear_program);

  EXPECT_THAT(preprocessor.Run(&linear_program),
              PreprocessorResultIs(/*postsolve_is_needed=*/true,
                                   /*status=*/std::nullopt));
  EXPECT_EQ(RowIndex(5), linear_program.num_constraints());
  EXPECT_EQ(ColIndex(7), linear_program.num_variables());
  SolveStatus solve_status = OptimalSolveStatus();
  ProblemSolution solution(RowIndex(5), ColIndex(7));
  solution.primal_values.assign(ColIndex(7), 0.0);
  solution.dual_values.assign(RowIndex(5), 2.0);
  solution.dual_values[RowIndex(1)] = 3.0;
  solution.dual_values[RowIndex(4)] = 10.0;
  preprocessor.RecoverSolution(solve_status, &solution);
  EXPECT_EQ(5, solution.primal_values.size());
  EXPECT_EQ(2.0, solution.primal_values[ColIndex(0)]);
  EXPECT_EQ(3.0, solution.primal_values[ColIndex(1)]);
  EXPECT_EQ(2.0, solution.primal_values[ColIndex(2)]);
  EXPECT_EQ(2.0, solution.primal_values[ColIndex(3)]);
  EXPECT_EQ(10.0, solution.primal_values[ColIndex(4)]);
  EXPECT_EQ(2, solution.dual_values.size());
  EXPECT_EQ(0.0, solution.dual_values[RowIndex(0)]);
  EXPECT_EQ(0.0, solution.dual_values[RowIndex(1)]);
}

TEST(DualizerPreprocessorTest, SmallProgram) {
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  GlopParameters params;
  params.set_solve_dual_problem(GlopParameters::ALWAYS_DO);
  DualizerPreprocessor dualizer_preprocessor(&params);
  dualizer_preprocessor.SetTimeLimit(time_limit.get());

  // The program is
  // min:;
  // c0 <= 2;
  LinearProgram lp;
  ColIndex var = lp.CreateNewVariable();
  lp.SetVariableBounds(var, -kInfinity, 2);

  // The dual is (with the variable bounds shifted to 0)
  // max:;
  // r0: + c0 = 0;
  // c0 <= 0;
  EXPECT_THAT(dualizer_preprocessor.Run(&lp),
              PreprocessorResultIs(/*postsolve_is_needed=*/true,
                                   /*status=*/std::nullopt));

  // A possible basic solution: All zero + row ConstraintStatus::BASIC.
  SolveStatus solve_status = OptimalSolveStatus();
  ProblemSolution solution(RowIndex(1), ColIndex(1));
  solution.variable_statuses[ColIndex(0)] = VariableStatus::AT_UPPER_BOUND;
  solution.constraint_statuses[RowIndex(0)] = ConstraintStatus::BASIC;

  // The reconstructed solution.
  dualizer_preprocessor.RecoverSolution(solve_status, &solution);
}

// --------------------------------------------------------
// ToMinimizationPreprocessor
// --------------------------------------------------------
TEST(ToMinimizationPeprocessorTest, TurnsMaximizationIntoMinimization) {
  const int kNumCols = 3;
  const int kNumRows = 2;
  const std::string kLinearProgram =
      "max: 10 + 0 x1 + 1 x2 - 2 x3;"
      "x1 - x2 + x3 <= 5;"
      "x1 + x2 - x3 >= 10;";
  // The preprocessor does not remove 0 coefficients from the objective.
  const std::string kOutputProgram =
      "min: -10 + 0 x1 - 1 x2 + 2 x3;"
      "x1 - x2 + x3 <= 5;"
      "x1 + x2 - x3 >= 10;";
  // The preprocessor does not modify the solution.
  const ExpectedSolution<kNumCols, kNumRows> kExpectedResult = {{}, {}};
  GlopParameters params;
  ToMinimizationPreprocessor preprocessor(&params);
  CheckPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                            &preprocessor);
}

TEST(ToMinimizationPeprocessorTest, LeavesMinimization) {
  const int kNumCols = 3;
  const int kNumRows = 2;
  const std::string kLinearProgram =
      "min: -10 + 0 x1 - 1 x2 + 2 x3;"
      "x1 - x2 + x3 <= 5;"
      "x1 + x2 - x3 >= 10;";
  // The preprocessor does not change minimization problem at all.
  const std::string kOutputProgram =
      "min: -10 + 0 x1 - 1 x2 + 2 x3;"
      "x1 - x2 + x3 <= 5;"
      "x1 + x2 - x3 >= 10;";
  // The preprocessor does not modify the solution.
  const ExpectedSolution<kNumCols, kNumRows> kExpectedResult = {{}, {}};
  GlopParameters params;
  ToMinimizationPreprocessor preprocessor(&params);
  CheckPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                            &preprocessor);
}

// --------------------------------------------------------
// AddSlackVariablesPreprocessor
// --------------------------------------------------------
TEST(AddSlackVariablesPreprocessorTest, Run) {
  const std::string kLinearProgram =
      "max: 10 + 0 x1 + 1 x2 - 2 x3;"
      "x1 - x2 + x3 <= 5;"
      "x1 + x2 - x3 >= 10;"
      "-3 <= 3 x1 - 4 x2 <= 10;";
  const std::string kOutputProgram =
      "max: 10 + 0 x1 + 1 x2 - 2 x3;"
      "x1 - x2 + x3 + s0 = 0;"
      "x1 + x2 - x3 + s1 = 0;"
      "3 x1 - 4 x2 + s2 = 0;"
      "s0 >= -5; s1 <= -10; -10 <= s2 <= 3;";
  const int kNumCols = 3;
  const int kNumRows = 3;
  // The preprocessor does not modify the values in the solution, it just
  // removes them for the slack variables.
  const ExpectedSolution<kNumCols, kNumRows> kExpectedResult = {{}, {}};
  GlopParameters params;
  AddSlackVariablesPreprocessor preprocessor(&params);
  CheckPreprocessorBehavior(kLinearProgram, kOutputProgram, kExpectedResult,
                            &preprocessor);
}

TEST(FixConstraintWithFixedStatusesTest, BasicTest) {
  DenseColumn row_lower_bounds = {0, 0, 0};
  DenseColumn row_upper_bounds = {1, 0, 1};
  ProblemSolution solution(RowIndex(3), ColIndex(0));
  solution.dual_values = {-1, 0, 1};
  solution.constraint_statuses = {ConstraintStatus::FIXED_VALUE,
                                  ConstraintStatus::FIXED_VALUE,
                                  ConstraintStatus::FIXED_VALUE};

  FixConstraintWithFixedStatuses(row_lower_bounds, row_upper_bounds, &solution);
  EXPECT_EQ(solution.constraint_statuses,
            (ConstraintStatusColumn{ConstraintStatus::AT_UPPER_BOUND,
                                    ConstraintStatus::FIXED_VALUE,
                                    ConstraintStatus::AT_LOWER_BOUND}));
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
