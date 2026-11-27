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

#include <cmath>
#include <cstdint>
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
#include "ortools/lp_data/lp_utils.h"
#include "ortools/lp_data/sparse_column.h"
#include "ortools/util/full_precision_inequalities.h"
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
  double primal_values[NumCols > 0 ? NumCols : 1];
  double dual_values[NumRows > 0 ? NumRows : 1];
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

  EXPECT_THAT(solution.primal_values,
              FractionalVectorComparable(
                  DenseRow(expected_solution.primal_values,
                           expected_solution.primal_values + NumCols)));
  EXPECT_THAT(solution.dual_values,
              FractionalVectorComparable(
                  DenseColumn(expected_solution.dual_values,
                              expected_solution.dual_values + NumRows)));
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

// Builds the LP 0 <= sum_i a_i x_i <= 3 with x_i in [-1, 1] and a_i in
// [0.5, 1.5], where all the x_i have a zero cost. All the columns are removed
// by the SingletonPreprocessor. In the postsolve, the constraint stays BASIC
// and each restored variable is put at one of its bounds depending on the
// activity of the constraint, so each undo needs this activity.
LinearProgram BuildLongRowWithZeroCostSingletonColumns(int num_cols) {
  std::mt19937 random(12345);
  LinearProgram lp;
  const RowIndex row = lp.CreateNewConstraint();
  lp.SetConstraintBounds(row, 0.0, 3.0);
  for (int i = 0; i < num_cols; ++i) {
    const ColIndex col = lp.CreateNewVariable();
    lp.SetVariableBounds(col, -1.0, 1.0);
    lp.SetCoefficient(row, col, absl::Uniform<double>(random, 0.5, 1.5));
  }
  return lp;
}

// Runs the SingletonPreprocessor on the LP above and postsolves the solution
// of the presolved LP, which has no columns and one empty BASIC row.
ProblemSolution PresolveAndPostsolveLongRow(const LinearProgram& original_lp) {
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  LinearProgram lp;
  lp.PopulateFromLinearProgram(original_lp);
  GlopParameters params;
  SingletonPreprocessor preprocessor(&params);
  preprocessor.SetTimeLimit(time_limit.get());
  const Preprocessor::Result result = preprocessor.Run(&lp);
  EXPECT_TRUE(result.postsolve_is_needed);
  EXPECT_EQ(result.solve_status, std::nullopt);
  EXPECT_EQ(lp.num_variables(), ColIndex(0));
  EXPECT_EQ(lp.num_constraints(), RowIndex(1));
  ProblemSolution solution(lp.num_constraints(), lp.num_variables());
  solution.constraint_statuses.assign(lp.num_constraints(),
                                      ConstraintStatus::BASIC);
  SolveStatus solve_status = OptimalSolveStatus();
  preprocessor.RecoverSolution(solve_status, &solution);
  return solution;
}

TEST(SingletonPreprocessorTest, ZeroCostSingletonColumnsInLongRowSmall) {
  const LinearProgram lp = BuildLongRowWithZeroCostSingletonColumns(6);
  const ProblemSolution solution = PresolveAndPostsolveLongRow(lp);
  // The columns are restored in the order 0, 1, 2, 3, 4, 5. Each one is put at
  // its lower bound if the activity of the already restored columns plus the
  // lower bound contribution is within the constraint bounds at the time of
  // its removal (these bounds were relaxed by the columns removed before), and
  // at its upper bound otherwise.
  EXPECT_THAT(solution.primal_values,
              ContainerEq(DenseRow({-1.0, -1.0, 1.0, 1.0, 1.0, 1.0})));
  EXPECT_EQ(solution.constraint_statuses[RowIndex(0)], ConstraintStatus::BASIC);
}

// The postsolve used to be quadratic in this case, because the activity of the
// constraint was recomputed from scratch for each restored column. Note that if
// we are quadratic, this test will just TIMEOUT.
TEST(SingletonPreprocessorTest, ZeroCostSingletonColumnsInLongRowLarge) {
  const int kNumCols = 300000;
  const LinearProgram lp = BuildLongRowWithZeroCostSingletonColumns(kNumCols);
  const ProblemSolution solution = PresolveAndPostsolveLongRow(lp);
  ASSERT_EQ(solution.primal_values.size(), ColIndex(kNumCols));
  EXPECT_EQ(solution.constraint_statuses[RowIndex(0)], ConstraintStatus::BASIC);
  const SparseColumn& column =
      lp.GetTransposeSparseMatrix().column(ColIndex(0));
  EXPECT_EQ(column.num_entries(), EntryIndex(kNumCols));
  for (ColIndex col(0); col < kNumCols; ++col) {
    EXPECT_TRUE(solution.primal_values[col] == -1.0 ||
                solution.primal_values[col] == 1.0);
  }
  const Fractional activity =
      PreciseScalarProduct(solution.primal_values, column);
  EXPECT_GE(activity, -1e-9);
  EXPECT_LE(activity, 3.0 + 1e-9);
}

TEST(SingletonPreprocessorTest, EqualitySingletonColumnsInLongRowBasisFree) {
  const int kNumCols = 1000;
  LinearProgram original_lp;
  const RowIndex row = original_lp.CreateNewConstraint();
  original_lp.SetConstraintBounds(row, -kInfinity, 0.0);
  for (int i = 0; i < kNumCols; ++i) {
    const ColIndex col = original_lp.CreateNewVariable();
    original_lp.SetVariableBounds(col, 0.0, 10.0);
    original_lp.SetObjectiveCoefficient(col,
                                        -static_cast<double>(kNumCols - i));
    original_lp.SetCoefficient(row, col, 1.0);
  }

  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  LinearProgram lp;
  lp.PopulateFromLinearProgram(original_lp);
  GlopParameters params;
  SingletonPreprocessor preprocessor(&params);
  preprocessor.SetRequiresBasis(false);
  preprocessor.SetTimeLimit(time_limit.get());
  const Preprocessor::Result result = preprocessor.Run(&lp);
  EXPECT_TRUE(result.postsolve_is_needed);
  EXPECT_EQ(result.solve_status, std::nullopt);
  EXPECT_EQ(lp.num_variables(), ColIndex(0));
  EXPECT_EQ(lp.num_constraints(), RowIndex(1));

  // With ConstraintStatus::FREE (basis-free solve), SingletonColumnInEquality
  // undos keep the row status FREE across all kNumCols columns because x_i = 0
  // satisfies the equality at every step.
  ProblemSolution solution(lp.num_constraints(), lp.num_variables());
  solution.constraint_statuses.assign(lp.num_constraints(),
                                      ConstraintStatus::FREE);
  SolveStatus solve_status = OptimalSolveStatus();
  preprocessor.RecoverSolution(solve_status, &solution);

  ASSERT_EQ(solution.primal_values.size(), ColIndex(kNumCols));
  for (ColIndex col(0); col < kNumCols; ++col) {
    EXPECT_EQ(solution.primal_values[col], 0.0);
  }
  EXPECT_DOUBLE_EQ(solution.dual_values[RowIndex(0)],
                   -static_cast<double>(kNumCols));
}

// Verifies that postsolving a long row of zero-cost singleton columns does not
// suffer from catastrophic cancellation when large opposite-sign contributions
// (+2^60 and -2^60) are added after row_activities switches to TightSum.
TEST(SingletonPreprocessorTest, CatastrophicCancellationInLongRowPostsolve) {
  const int kNumCols = 120;
  const Fractional kBig = std::ldexp(1.0, 60);
  LinearProgram lp;
  const RowIndex row = lp.CreateNewConstraint();
  lp.SetConstraintBounds(row, 0.0, 0.5);

  // Column 0 is fixed at 0.75. Columns 1..115 trigger > kMaxExactEvaluations
  // queries so row_activities promotes the row to TightSum before column 116
  // adds +2^60 and column 117 adds -2^60. Naive double incremental updates
  // would round (0.75 + 2^60) - 2^60 to 0.0 and set column 119 to 0.0 instead
  // of -0.5, violating the original row upper bound 0.5 by 0.25.
  for (int i = 0; i < kNumCols; ++i) {
    const ColIndex col = lp.CreateNewVariable();
    if (i == 0) {
      lp.SetVariableBounds(col, 0.75, 0.75);
      lp.SetCoefficient(row, col, 1.0);
    } else if (i <= 115) {
      lp.SetVariableBounds(col, 0.0, 1.0);
      lp.SetCoefficient(row, col, 1.0);
    } else if (i == 116) {
      lp.SetVariableBounds(col, kBig, kBig);
      lp.SetCoefficient(row, col, 1.0);
    } else if (i == 117) {
      lp.SetVariableBounds(col, 0.0, kBig);
      lp.SetCoefficient(row, col, -1.0);
    } else if (i == 118) {
      lp.SetVariableBounds(col, -255.0, 0.0);
      lp.SetCoefficient(row, col, 1.0);
    } else {
      lp.SetVariableBounds(col, -0.5, 0.0);
      lp.SetCoefficient(row, col, 1.0);
    }
  }

  const ProblemSolution solution = PresolveAndPostsolveLongRow(lp);
  ASSERT_EQ(solution.primal_values.size(), ColIndex(kNumCols));
  EXPECT_EQ(solution.constraint_statuses[RowIndex(0)], ConstraintStatus::BASIC);
  EXPECT_EQ(solution.primal_values[ColIndex(0)], 0.75);
  EXPECT_EQ(solution.primal_values[ColIndex(116)], kBig);
  EXPECT_EQ(solution.primal_values[ColIndex(117)], kBig);
  EXPECT_EQ(solution.primal_values[ColIndex(118)], 0.0);
  EXPECT_EQ(solution.primal_values[ColIndex(119)], -0.5);

  TightSum exact_activity;
  for (const SparseColumn::Entry e :
       lp.GetTransposeSparseMatrix().column(ColIndex(0))) {
    exact_activity.AddProd(e.coefficient(),
                           solution.primal_values[RowToColIndex(e.row())]);
  }
  EXPECT_EQ(exact_activity.TightLowerBound(), 0.25);
  EXPECT_EQ(exact_activity.TightUpperBound(), 0.25);
}

// --------------------------------------------------------
// IncrementalScalarProducts
// --------------------------------------------------------

TEST(IncrementalScalarProductsTest, FirstEvaluationsAndIncrementalUpdates) {
  const int kSize = 1000;
  SparseColumn vector_a;
  SparseColumn vector_b;
  for (int i = 0; i < kSize; ++i) {
    vector_a.SetCoefficient(RowIndex(i), 1.0 / (i + 1));
    if (i % 2 == 0) {
      vector_b.SetCoefficient(RowIndex(i), -0.5 * (i + 1));
    }
  }
  DenseRow values(ColIndex(kSize), 0.0);
  std::mt19937 random(12345);
  IncrementalScalarProducts<ColIndex> products(kSize);
  products.Add(LocalIndex{3}, vector_b);
  products.Add(LocalIndex{0}, vector_a);

  const IncrementalScalarProducts<ColIndex>& const_products = products;
  for (int step = 0; step < kSize; ++step) {
    const Fractional expected_a = ScalarProduct(values, vector_a);
    const Fractional actual_a = const_products.Get(LocalIndex{0});
    if (step < IncrementalScalarProducts<ColIndex>::kMaxExactEvaluations) {
      EXPECT_EQ(actual_a, expected_a);
      // During the first kMaxExactEvaluations calls, vector_a is not indexed
      // yet, and vector_b is never queried, so no extra entries are scanned.
      const int64_t expected_scanned =
          static_cast<int64_t>(step + 1) * static_cast<int64_t>(kSize);
      EXPECT_EQ(const_products.num_scanned_entries(), expected_scanned);
    } else {
      EXPECT_NEAR(actual_a, expected_a, 1e-12);
    }

    // Like in the postsolve, each value changes from zero exactly once.
    const ColIndex col(step);
    const Fractional new_value = absl::Uniform<double>(random, -1.0, 1.0);
    products.SetValue(col, new_value);
    values[col] = new_value;
  }
  EXPECT_NEAR(const_products.Get(LocalIndex{3}),
              ScalarProduct(values, vector_b), 1e-9);
}

TEST(IncrementalScalarProductsTest, ExactCancellationWithoutDrift) {
  const int kSize = 50;
  SparseColumn vector;
  for (int i = 0; i < kSize; ++i) {
    vector.SetCoefficient(RowIndex(i), 1.0);
  }
  DenseRow initial_values(ColIndex(kSize), 0.0);
  const Fractional initial_value = 0.75 + std::ldexp(1.0, -20);
  initial_values[ColIndex(0)] = initial_value;

  IncrementalScalarProducts<ColIndex> products(initial_values);
  products.Add(LocalIndex{0}, vector);
  const IncrementalScalarProducts<ColIndex>& const_products = products;
  for (int q = 0;
       q <= IncrementalScalarProducts<ColIndex>::kMaxExactEvaluations; ++q) {
    EXPECT_EQ(const_products.Get(LocalIndex{0}), initial_value);
  }

  // Adding a 2^60 term and later subtracting it from the same entry restores
  // the exact original value bit-for-bit.
  const Fractional kBig = std::ldexp(1.0, 60);
  products.SetValue(ColIndex{1}, kBig);
  EXPECT_EQ(const_products.Get(LocalIndex{0}), kBig);
  products.SetValue(ColIndex{1}, 0.0);
  EXPECT_EQ(const_products.Get(LocalIndex{0}), initial_value);

  // Adding cancelling huge terms across distinct entries also preserves the
  // low-order bits without drift.
  for (int pair = 0; pair < 20; ++pair) {
    const ColIndex pos_idx = ColIndex{2 + 2 * pair};
    const ColIndex neg_idx = ColIndex{3 + 2 * pair};
    const Fractional magnitude = std::ldexp(1.0, 60 + pair);
    products.SetValue(pos_idx, magnitude);
    products.SetValue(neg_idx, -magnitude);
    EXPECT_EQ(const_products.Get(LocalIndex{0}), initial_value);
  }
}

TEST(IncrementalScalarProductsTest, LinearWork) {
  const int kSize = 200000;
  SparseColumn vector;
  for (int i = 0; i < kSize; ++i) {
    vector.SetCoefficient(RowIndex(i), 1.0);
  }
  IncrementalScalarProducts<RowIndex> products(kSize);
  products.Add(LocalIndex{0}, vector);
  for (int step = 0; step < kSize; ++step) {
    EXPECT_EQ(products.Get(LocalIndex{0}), step);
    products.SetValue(RowIndex{step}, 1.0);
  }
  EXPECT_LE(products.num_scanned_entries(),
            (IncrementalScalarProducts<RowIndex>::kMaxExactEvaluations + 2) *
                static_cast<int64_t>(kSize));
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

TEST(DualizerPreprocessorTest, BasisFreeRecoversInteriorPrimalAndDualValues) {
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  GlopParameters params;
  params.set_solve_dual_problem(GlopParameters::ALWAYS_DO);

  // c0: lower-bounded [2, inf), shift = 2.
  // c1: upper-bounded (-inf, -3], shift = -3.
  // c2: boxed [1, 5], shift = 1.
  // c3: fixed [4, 4], shift = 4.
  // c4: free (-inf, inf), shift = 0.
  const std::string kLinearProgram =
      "min: 0 c0 + 0 c1 + 0 c2 + 0 c3 + 0 c4;"
      "r0: 1 c0 + 2 c2 = 5;"
      "r1: -1 c1 + 1 c3 + 3 c4 <= 10;"
      "c0 >= 2;"
      "c1 <= -3;"
      "1 <= c2 <= 5;"
      "4 <= c3 <= 4;";
  LinearProgram lp;
  ASSERT_TRUE(ParseLp(kLinearProgram, &lp));

  DualizerPreprocessor basis_free_preprocessor(&params);
  basis_free_preprocessor.SetRequiresBasis(false);
  basis_free_preprocessor.SetTimeLimit(time_limit.get());
  EXPECT_THAT(basis_free_preprocessor.Run(&lp),
              PreprocessorResultIs(/*postsolve_is_needed=*/true,
                                   /*status=*/std::nullopt));
  // Dual LP has 5 rows (from 5 primal cols) and 8 cols:
  // - cols 0..1: 2 primal constraints
  // - cols 2..4: lower bounds of primal cols 0, 2, 3
  // - cols 5..7: upper bounds of primal cols 1, 2, 3
  ASSERT_EQ(RowIndex(5), lp.num_constraints());
  ASSERT_EQ(ColIndex(8), lp.num_variables());

  SolveStatus solve_status = OptimalSolveStatus();
  ProblemSolution solution(RowIndex(5), ColIndex(8));
  // Simulate a basis-free solver that populates dummy BASIC statuses.
  solution.variable_statuses.assign(ColIndex(8), VariableStatus::BASIC);
  solution.constraint_statuses.assign(RowIndex(5), ConstraintStatus::BASIC);
  // Shifted interior primal values (dual_values of the dual LP).
  solution.dual_values = {1.5, -0.5, 2.2, 0.0, -7.25};
  // Dual variables (primal_values of the dual LP):
  // [y0, y1, z_l0, z_l2, z_l3, z_u1, z_u2, z_u3].
  solution.primal_values = {10.0, -4.0, 0.75, 1.25, 5.0, -2.5, -0.25, -1.5};

  basis_free_preprocessor.RecoverSolution(solve_status, &solution);

  // Interior primal values must be preserved (unshifted) without snapping to
  // bounds despite all dual statuses being BASIC.
  EXPECT_EQ(ColIndex(5), solution.primal_values.size());
  EXPECT_DOUBLE_EQ(3.5, solution.primal_values[ColIndex(0)]);
  EXPECT_DOUBLE_EQ(-3.5, solution.primal_values[ColIndex(1)]);
  EXPECT_DOUBLE_EQ(3.2, solution.primal_values[ColIndex(2)]);
  EXPECT_DOUBLE_EQ(4.0, solution.primal_values[ColIndex(3)]);
  EXPECT_DOUBLE_EQ(-7.25, solution.primal_values[ColIndex(4)]);

  EXPECT_EQ(RowIndex(2), solution.dual_values.size());
  EXPECT_DOUBLE_EQ(10.0, solution.dual_values[RowIndex(0)]);
  EXPECT_DOUBLE_EQ(-4.0, solution.dual_values[RowIndex(1)]);

  // Basis-free mode leaves all recovered statuses as FREE.
  for (ColIndex col(0); col < ColIndex(5); ++col) {
    EXPECT_EQ(VariableStatus::FREE, solution.variable_statuses[col]);
  }
  for (RowIndex row(0); row < RowIndex(2); ++row) {
    EXPECT_EQ(ConstraintStatus::FREE, solution.constraint_statuses[row]);
  }
}

TEST(DualizerPreprocessorTest, BasisFreeMaximizationAndRangedRow) {
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  GlopParameters params;
  params.set_solve_dual_problem(GlopParameters::ALWAYS_DO);

  // r0 is a ranged constraint, which PopulateFromDual duplicates.
  const std::string kLinearProgram =
      "max: 0 c0 + 0 c1;"
      "r0: -2 <= 1 c0 + 2 c1 <= 6;"
      "r1: 3 c0 >= 1;"
      "1 <= c0 <= 4;"
      "c1 >= 0;";
  LinearProgram lp;
  ASSERT_TRUE(ParseLp(kLinearProgram, &lp));

  DualizerPreprocessor preprocessor(&params);
  preprocessor.SetRequiresBasis(false);
  preprocessor.SetTimeLimit(time_limit.get());
  EXPECT_THAT(preprocessor.Run(&lp),
              PreprocessorResultIs(/*postsolve_is_needed=*/true,
                                   /*status=*/std::nullopt));
  // Dual LP has 2 rows and 6 cols:
  // - col 0: r0 upper bound
  // - col 1: r1 lower bound
  // - col 2: c0 lower bound
  // - col 3: c1 lower bound
  // - col 4: c0 upper bound
  // - col 5: duplicated row for r0 lower bound
  ASSERT_EQ(RowIndex(2), lp.num_constraints());
  ASSERT_EQ(ColIndex(6), lp.num_variables());

  SolveStatus solve_status = OptimalSolveStatus();
  ProblemSolution solution(RowIndex(2), ColIndex(6));
  solution.variable_statuses.assign(ColIndex(6), VariableStatus::BASIC);
  solution.constraint_statuses.assign(RowIndex(2), ConstraintStatus::BASIC);
  solution.dual_values = {1.5, 2.5};
  solution.primal_values = {-3.0, 2.0, 1.2, 0.4, -0.2, 0.5};

  preprocessor.RecoverSolution(solve_status, &solution);

  EXPECT_DOUBLE_EQ(2.5, solution.primal_values[ColIndex(0)]);
  EXPECT_DOUBLE_EQ(2.5, solution.primal_values[ColIndex(1)]);
  // For maximization, sign = -1.0 is applied to both primary and duplicated
  // row columns.
  EXPECT_DOUBLE_EQ(2.5, solution.dual_values[RowIndex(0)]);
  EXPECT_DOUBLE_EQ(-2.0, solution.dual_values[RowIndex(1)]);
}

TEST(DualizerPreprocessorTest, MainLpPreprocessorBasisFreeEndToEnd) {
  // Original minimization LP with 3 rows, 1 empty column x0, and 2 active
  // columns x1, x2 with shifted bounds:
  //   min 3.5 x0 + 4.0 x1 - 2.0 x2
  //   s.t.
  //     r0: x1 + 2 x2 <= 14
  //     r1: 2 x1 - x2 <= 8
  //     r2: x1 + x2 >= 5
  //     1 <= x0 <= 5, 2 <= x1 <= 6, 1 <= x2 <= 6
  const std::string kLinearProgram =
      "min: 3.5 x0 + 4 x1 - 2 x2;"
      "0 x0 + 1 x1 + 2 x2 <= 14;"
      "0 x0 + 2 x1 - 1 x2 <= 8;"
      "0 x0 + 1 x1 + 1 x2 >= 5;"
      "1 <= x0 <= 5;"
      "2 <= x1 <= 6;"
      "1 <= x2 <= 6;";
  LinearProgram lp;
  ASSERT_TRUE(ParseLp(kLinearProgram, &lp));

  GlopParameters params;
  params.set_solve_dual_problem(GlopParameters::ALWAYS_DO);
  params.set_use_scaling(false);
  // Keep EmptyColumnPreprocessor, ShiftVariableBoundsPreprocessor, and
  // DualizerPreprocessor active while disabling reductions that would
  // eliminate rows/cols prior to dualization on this small test instance.
  params.set_use_singleton_preprocessor(false);
  params.set_use_forcing_and_implied_free_constraint_preprocessor(false);
  params.set_use_bound_propagation_preprocessor(false);

  MainLpPreprocessor main_preprocessor(&params);
  main_preprocessor.SetRequiresBasis(false);
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  main_preprocessor.SetTimeLimit(time_limit.get());
  EXPECT_THAT(main_preprocessor.Run(&lp),
              PreprocessorResultIs(/*postsolve_is_needed=*/true,
                                   /*status=*/std::nullopt));
  // After EmptyColumnPreprocessor removes x0, the 3x2 LP is dualized into a
  // 2x7 dual LP.
  ASSERT_EQ(RowIndex(2), lp.num_constraints());
  ASSERT_EQ(ColIndex(7), lp.num_variables());

  SolveStatus solve_status = OptimalSolveStatus();
  ProblemSolution solution(RowIndex(2), ColIndex(7));
  solution.variable_statuses.assign(ColIndex(7), VariableStatus::BASIC);
  solution.constraint_statuses.assign(RowIndex(2), ConstraintStatus::BASIC);
  // Shifted primal values for x1 (shift 2) and x2 (shift 1):
  // x1 = 3.25 -> 1.25, x2 = 4.5 -> 3.5.
  solution.dual_values = {1.25, 3.5};
  // Dual cols: [y0, y1, y2, z_l1, z_l2, z_u1, z_u2].
  solution.primal_values = {-1.0, -0.5, 2.0, 4.0, 0.0, 0.0, -2.5};

  main_preprocessor.RecoverSolution(solve_status, &solution);

  ASSERT_EQ(ColIndex(3), solution.primal_values.size());
  EXPECT_DOUBLE_EQ(1.0, solution.primal_values[ColIndex(0)]);
  EXPECT_DOUBLE_EQ(3.25, solution.primal_values[ColIndex(1)]);
  EXPECT_DOUBLE_EQ(4.5, solution.primal_values[ColIndex(2)]);

  ASSERT_EQ(RowIndex(3), solution.dual_values.size());
  EXPECT_DOUBLE_EQ(-1.0, solution.dual_values[RowIndex(0)]);
  EXPECT_DOUBLE_EQ(-0.5, solution.dual_values[RowIndex(1)]);
  EXPECT_DOUBLE_EQ(2.0, solution.dual_values[RowIndex(2)]);
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

// --------------------------------------------------------
// ConfigurablePreprocessorFlagsTest
// --------------------------------------------------------

TEST(ConfigurablePreprocessorFlagsTest, DisableFixedVariablePreprocessor) {
  const std::string kLinearProgram =
      "min: 0 + 0 x1 + 0 x2 + 0 x3;"
      " 1 <= x1 <= 5;"
      " 3 <= x2 <= 3;"  // Fixed.
      " 4 <= x3 <= 8;";
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();

  // With flag enabled (default), the fixed variable is removed.
  {
    LinearProgram lp;
    ASSERT_TRUE(ParseLp(kLinearProgram, &lp));
    GlopParameters params;
    FixedVariablePreprocessor preprocessor(&params);
    preprocessor.SetTimeLimit(time_limit.get());
    EXPECT_THAT(preprocessor.Run(&lp),
                PreprocessorResultIs(/*postsolve_is_needed=*/true,
                                     /*status=*/std::nullopt));
    EXPECT_EQ(2, lp.num_variables());
  }

  // With flag disabled, the preprocessor returns early and leaves variables.
  {
    LinearProgram lp;
    ASSERT_TRUE(ParseLp(kLinearProgram, &lp));
    GlopParameters params;
    params.set_use_fixed_variable_preprocessor(false);
    FixedVariablePreprocessor preprocessor(&params);
    preprocessor.SetTimeLimit(time_limit.get());
    EXPECT_THAT(preprocessor.Run(&lp),
                PreprocessorResultIs(/*postsolve_is_needed=*/false,
                                     /*status=*/std::nullopt));
    EXPECT_EQ(3, lp.num_variables());
  }
}

TEST(ConfigurablePreprocessorFlagsTest, DisableEmptyColumnPreprocessor) {
  const std::string kLinearProgram =
      "min: 0 + 0 x1 + 2 x2;"
      " 1 <= x1 <= 5;"
      " 2 <= x2 <= 6;";
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();

  // With flag disabled, empty columns are preserved.
  {
    LinearProgram lp;
    ASSERT_TRUE(ParseLp(kLinearProgram, &lp));
    GlopParameters params;
    params.set_use_empty_column_preprocessor(false);
    EmptyColumnPreprocessor preprocessor(&params);
    preprocessor.SetTimeLimit(time_limit.get());
    EXPECT_THAT(preprocessor.Run(&lp),
                PreprocessorResultIs(/*postsolve_is_needed=*/false,
                                     /*status=*/std::nullopt));
    EXPECT_EQ(2, lp.num_variables());
  }
}

TEST(ConfigurablePreprocessorFlagsTest, DisableFreeConstraintPreprocessor) {
  const std::string kLinearProgram =
      "min: 0 + x1 + x2;"
      "-inf <= x1 + x2 <= inf;"  // Free constraint.
      " 1 <= x1 <= 5;"
      " 2 <= x2 <= 6;";
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();

  // With flag disabled, free constraints are preserved.
  {
    LinearProgram lp;
    ASSERT_TRUE(ParseLp(kLinearProgram, &lp));
    GlopParameters params;
    params.set_use_free_constraint_preprocessor(false);
    FreeConstraintPreprocessor preprocessor(&params);
    preprocessor.SetTimeLimit(time_limit.get());
    EXPECT_THAT(preprocessor.Run(&lp),
                PreprocessorResultIs(/*postsolve_is_needed=*/false,
                                     /*status=*/std::nullopt));
    EXPECT_EQ(1, lp.num_constraints());
  }
}

TEST(ConfigurablePreprocessorFlagsTest, DisableShiftVariableBounds) {
  const std::string kLinearProgram =
      "min: 0 + x1;"
      " 10 <= x1 <= 20;";
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();

  // With flag disabled, bounds are not shifted to include zero.
  {
    LinearProgram lp;
    ASSERT_TRUE(ParseLp(kLinearProgram, &lp));
    GlopParameters params;
    params.set_use_shift_variable_bounds_preprocessor(false);
    ShiftVariableBoundsPreprocessor preprocessor(&params);
    preprocessor.SetTimeLimit(time_limit.get());
    EXPECT_THAT(preprocessor.Run(&lp),
                PreprocessorResultIs(/*postsolve_is_needed=*/false,
                                     /*status=*/std::nullopt));
    EXPECT_EQ(10.0, lp.variable_lower_bounds()[ColIndex(0)]);
  }
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
