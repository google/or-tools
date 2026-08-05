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

#include "ortools/lp_data/lp_data.h"

#include <limits>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/substitute.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/lp_data/lp_parser.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/permutation.h"
#include "ortools/lp_data/sparse.h"
#include "ortools/lp_data/sparse_column.h"
#include "ortools/util/fp_utils.h"

namespace operations_research {
namespace glop {
namespace {

using ::testing::ElementsAre;

const Fractional kNaN = std::numeric_limits<Fractional>::quiet_NaN();
const Fractional kIntegralityTolerance = 1e-6;

// This test is here because the code in this class relies on the fact that a
// kInvalidRow index will be transformed into a kInvalidCol index by
// functions like ColToRowIndex() or RowToColIndex().
TEST(InvalidIndexTest, IdenticalValues) {
  EXPECT_EQ(kInvalidRow.value(), kInvalidCol.value());
}

TEST(LinearProgramTest, InitialValues) {
  const LinearProgram linear_program;
  EXPECT_FALSE(linear_program.IsMaximizationProblem());
  EXPECT_TRUE(linear_program.IsCleanedUp());
  EXPECT_EQ(0, linear_program.num_variables());
  EXPECT_EQ(0, linear_program.num_constraints());
  EXPECT_EQ(0, linear_program.objective_offset());
  EXPECT_EQ(1.0, linear_program.objective_scaling_factor());

  const std::string kDump = "min:;\n";
  EXPECT_EQ(kDump, linear_program.Dump());
}

TEST(LinearProgramTest, FindOrCreateVariable) {
  LinearProgram linear_program;
  const std::string name_x("x");
  const std::string name_y("y");
  const ColIndex col_x = linear_program.FindOrCreateVariable(name_x);

  const ColIndex num_variables_x = linear_program.num_variables();
  EXPECT_EQ(1, num_variables_x);

  const ColIndex col_y = linear_program.FindOrCreateVariable(name_y);
  const ColIndex num_variables_x_y = linear_program.num_variables();
  EXPECT_EQ(2, num_variables_x_y);

  const ColIndex new_col_x = linear_program.FindOrCreateVariable(name_x);
  const ColIndex new_num_variables_x_y = linear_program.num_variables();
  EXPECT_EQ(2, new_num_variables_x_y);
  EXPECT_EQ(col_x, new_col_x);

  EXPECT_EQ(name_x, linear_program.GetVariableName(col_x));
  EXPECT_EQ(name_y, linear_program.GetVariableName(col_y));
}

TEST(LinearProgramTest, FindOrCreateConstraint) {
  LinearProgram linear_program;
  const std::string name_x("x");
  const std::string name_y("y");
  const RowIndex row_x = linear_program.FindOrCreateConstraint(name_x);

  const RowIndex num_constraints_x = linear_program.num_constraints();
  EXPECT_EQ(1, num_constraints_x);

  linear_program.FindOrCreateConstraint(name_y);
  const RowIndex num_constraints_x_y = linear_program.num_constraints();
  EXPECT_EQ(2, num_constraints_x_y);

  const RowIndex new_row_x = linear_program.FindOrCreateConstraint(name_x);
  const RowIndex new_num_constraints_x_y = linear_program.num_constraints();
  EXPECT_EQ(2, new_num_constraints_x_y);
  EXPECT_EQ(row_x, new_row_x);
}

TEST(LinearProgramTest, SetObjectiveOffset) {
  const Fractional kOffset(2);
  LinearProgram linear_program;
  linear_program.SetObjectiveOffset(kOffset);

  const Fractional offset = linear_program.objective_offset();
  EXPECT_EQ(kOffset, offset);
}

TEST(LinearProgramTest, SetMaximizationProblem) {
  LinearProgram linear_program;
  linear_program.SetMaximizationProblem(true);

  const bool maximize = linear_program.IsMaximizationProblem();
  EXPECT_TRUE(maximize);
}

TEST(LinearProgramTest, SetVariableBounds) {
  LinearProgram linear_program;
  const std::string name("x");
  const Fractional kLowerBound(2);
  const Fractional kUpperBound(3);
  const ColIndex col = linear_program.FindOrCreateVariable(name);
  linear_program.SetVariableBounds(col, kLowerBound, kUpperBound);
  EXPECT_EQ(name, linear_program.GetVariableName(col));
  EXPECT_EQ(kLowerBound, linear_program.variable_lower_bounds()[col]);
  EXPECT_EQ(kUpperBound, linear_program.variable_upper_bounds()[col]);
}

TEST(LinearProgramTest, SetVariableTypeInteger) {
  LinearProgram linear_program;
  const std::string name("x");
  const ColIndex col = linear_program.FindOrCreateVariable(name);
  // Variable type is initially set to continuous.
  EXPECT_EQ(LinearProgram::VariableType::CONTINUOUS,
            linear_program.variable_types()[col]);
  // Setting the variable as integer.
  linear_program.SetVariableType(col, LinearProgram::VariableType::INTEGER);
  EXPECT_EQ(LinearProgram::VariableType::INTEGER,
            linear_program.variable_types()[col]);
  EXPECT_TRUE(linear_program.IsVariableInteger(col));
}

TEST(LinearProgramTest, SetVariableTypeImpliedInteger) {
  LinearProgram linear_program;
  const std::string name("x");
  const ColIndex col = linear_program.FindOrCreateVariable(name);
  // Variable type is initially set to continuous.
  EXPECT_EQ(LinearProgram::VariableType::CONTINUOUS,
            linear_program.variable_types()[col]);
  // Setting the variable as implied integer.
  linear_program.SetVariableType(col,
                                 LinearProgram::VariableType::IMPLIED_INTEGER);
  EXPECT_EQ(LinearProgram::VariableType::IMPLIED_INTEGER,
            linear_program.variable_types()[col]);
  EXPECT_TRUE(linear_program.IsVariableInteger(col));
}

TEST(LinearProgramTest, IsVariableBinary) {
  LinearProgram linear_program;
  const std::string name("x");
  const ColIndex col = linear_program.FindOrCreateVariable(name);
  EXPECT_FALSE(linear_program.IsVariableBinary(col));
  linear_program.SetVariableType(col, LinearProgram::VariableType::INTEGER);
  EXPECT_FALSE(linear_program.IsVariableBinary(col));
  linear_program.SetVariableBounds(col, Fractional(0), Fractional(1));
  EXPECT_TRUE(linear_program.IsVariableBinary(col));
  linear_program.SetVariableBounds(col, Fractional(-1), Fractional(2));
  EXPECT_FALSE(linear_program.IsVariableBinary(col));
  linear_program.SetVariableBounds(col, Fractional(-1) + kEpsilon,
                                   Fractional(2));
  EXPECT_FALSE(linear_program.IsVariableBinary(col));
  linear_program.SetVariableBounds(col, Fractional(-1),
                                   Fractional(2) - kEpsilon);
  EXPECT_FALSE(linear_program.IsVariableBinary(col));
  linear_program.SetVariableBounds(col, Fractional(-1) + kEpsilon,
                                   Fractional(2) - kEpsilon);
  EXPECT_TRUE(linear_program.IsVariableBinary(col));
}

TEST(LinearProgramTest, IntegerVariableListsAreConsistentAfterBoundChange1) {
  LinearProgram lp;
  const ColIndex col = lp.FindOrCreateVariable("x");
  EXPECT_TRUE(lp.BinaryVariablesList().empty());
  lp.SetVariableBounds(col, 0.0, 1.0);
  lp.SetVariableType(col, LinearProgram::VariableType::INTEGER);

  EXPECT_THAT(lp.IntegerVariablesList(), ::testing::ElementsAre(col));
  EXPECT_THAT(lp.BinaryVariablesList(), ::testing::ElementsAre(col));
  EXPECT_TRUE(lp.NonBinaryVariablesList().empty());

  lp.SetVariableBounds(col, 0.0, 10.0);

  EXPECT_THAT(lp.IntegerVariablesList(), ::testing::ElementsAre(col));
  EXPECT_TRUE(lp.BinaryVariablesList().empty());
  EXPECT_THAT(lp.NonBinaryVariablesList(), ::testing::ElementsAre(col));
}

TEST(LinearProgramTest, IntegerVariableListsAreConsistentAfterBoundChange2) {
  LinearProgram lp;
  const ColIndex col = lp.FindOrCreateVariable("x");
  EXPECT_TRUE(lp.BinaryVariablesList().empty());
  lp.SetVariableBounds(col, 0.0, 1.0);
  lp.SetVariableType(col, LinearProgram::VariableType::IMPLIED_INTEGER);

  EXPECT_THAT(lp.IntegerVariablesList(), ::testing::ElementsAre(col));
  EXPECT_THAT(lp.BinaryVariablesList(), ::testing::ElementsAre(col));
  EXPECT_TRUE(lp.NonBinaryVariablesList().empty());

  lp.SetVariableBounds(col, 0.0, 10.0);

  EXPECT_THAT(lp.IntegerVariablesList(), ::testing::ElementsAre(col));
  EXPECT_TRUE(lp.BinaryVariablesList().empty());
  EXPECT_THAT(lp.NonBinaryVariablesList(), ::testing::ElementsAre(col));
}

TEST(LinearProgramTest, IntegerVariableListsAreConsistentAfterTypeChange1) {
  LinearProgram lp;
  const ColIndex col = lp.FindOrCreateVariable("x");
  EXPECT_TRUE(lp.BinaryVariablesList().empty());
  lp.SetVariableBounds(col, 0.0, 1.0);

  EXPECT_TRUE(lp.IntegerVariablesList().empty());
  EXPECT_TRUE(lp.BinaryVariablesList().empty());
  EXPECT_TRUE(lp.NonBinaryVariablesList().empty());

  lp.SetVariableType(col, LinearProgram::VariableType::INTEGER);
  EXPECT_THAT(lp.IntegerVariablesList(), ::testing::ElementsAre(col));
  EXPECT_THAT(lp.BinaryVariablesList(), ::testing::ElementsAre(col));
  EXPECT_TRUE(lp.NonBinaryVariablesList().empty());

  lp.SetVariableType(col, LinearProgram::VariableType::CONTINUOUS);
  EXPECT_TRUE(lp.IntegerVariablesList().empty());
  EXPECT_TRUE(lp.BinaryVariablesList().empty());
  EXPECT_TRUE(lp.NonBinaryVariablesList().empty());

  lp.SetVariableType(col, LinearProgram::VariableType::INTEGER);
  EXPECT_THAT(lp.IntegerVariablesList(), ::testing::ElementsAre(col));
  EXPECT_THAT(lp.BinaryVariablesList(), ::testing::ElementsAre(col));
  EXPECT_TRUE(lp.NonBinaryVariablesList().empty());
}

TEST(LinearProgramTest, IntegerVariableListsAreConsistentAfterTypeChange2) {
  LinearProgram lp;
  const ColIndex col = lp.FindOrCreateVariable("x");
  EXPECT_TRUE(lp.BinaryVariablesList().empty());
  lp.SetVariableBounds(col, 0.0, 1.0);

  EXPECT_TRUE(lp.IntegerVariablesList().empty());
  EXPECT_TRUE(lp.BinaryVariablesList().empty());
  EXPECT_TRUE(lp.NonBinaryVariablesList().empty());

  lp.SetVariableType(col, LinearProgram::VariableType::IMPLIED_INTEGER);
  EXPECT_THAT(lp.IntegerVariablesList(), ::testing::ElementsAre(col));
  EXPECT_THAT(lp.BinaryVariablesList(), ::testing::ElementsAre(col));
  EXPECT_TRUE(lp.NonBinaryVariablesList().empty());

  lp.SetVariableType(col, LinearProgram::VariableType::CONTINUOUS);
  EXPECT_TRUE(lp.IntegerVariablesList().empty());
  EXPECT_TRUE(lp.BinaryVariablesList().empty());
  EXPECT_TRUE(lp.NonBinaryVariablesList().empty());

  lp.SetVariableType(col, LinearProgram::VariableType::IMPLIED_INTEGER);
  EXPECT_THAT(lp.IntegerVariablesList(), ::testing::ElementsAre(col));
  EXPECT_THAT(lp.BinaryVariablesList(), ::testing::ElementsAre(col));
  EXPECT_TRUE(lp.NonBinaryVariablesList().empty());
}

TEST(LinearProgramTest, SetConstraintBoundsBoxedCase) {
  LinearProgram linear_program;
  const ColIndex num_variables = linear_program.num_variables();

  const std::string name("x");
  const Fractional kLowerBound(2);
  const Fractional kUpperBound(3);
  const RowIndex row = linear_program.FindOrCreateConstraint(name);
  linear_program.SetConstraintBounds(row, kLowerBound, kUpperBound);

  EXPECT_EQ(num_variables, linear_program.num_variables());
  EXPECT_EQ(kLowerBound, linear_program.constraint_lower_bounds()[row]);
  EXPECT_EQ(kUpperBound, linear_program.constraint_upper_bounds()[row]);
}

TEST(LinearProgramTest, SetConstraintBoundsOtherCases) {
  LinearProgram linear_program;
  const std::string name("x");
  const ColIndex num_variables = linear_program.num_variables();
  const RowIndex row = linear_program.FindOrCreateConstraint(name);

  linear_program.SetConstraintBounds(row, 4, 4);
  EXPECT_EQ(num_variables, linear_program.num_variables());

  linear_program.SetConstraintBounds(row, 5.2, kInfinity);
  EXPECT_EQ(num_variables, linear_program.num_variables());

  linear_program.SetConstraintBounds(row, -kInfinity, -10);
  EXPECT_EQ(num_variables, linear_program.num_variables());

  linear_program.SetConstraintBounds(row, -kInfinity, kInfinity);
  EXPECT_EQ(num_variables, linear_program.num_variables());
}

TEST(LinearProgramTest, SetCoefficient) {
  LinearProgram linear_program;
  const std::string name("x");
  const ColIndex col = linear_program.FindOrCreateVariable(name);
  const RowIndex kRow(7);
  const Fractional kCoeff(2);
  linear_program.SetCoefficient(kRow, col, kCoeff);

  const SparseColumn& column = linear_program.GetSparseColumn(col);
  EXPECT_EQ(kCoeff, column.LookUpCoefficient(kRow));
}

TEST(LinearProgramTest, SetObjectiveCoefficient) {
  LinearProgram linear_program;
  const std::string name("x");
  const ColIndex col = linear_program.FindOrCreateVariable(name);
  const Fractional kCoeff(2);
  linear_program.SetObjectiveCoefficient(col, kCoeff);
  EXPECT_EQ(kCoeff, linear_program.objective_coefficients()[col]);
  EXPECT_THAT(linear_program.objective_coefficients(),
              testing::ContainerEq(DenseRow{kCoeff}));
}

void AddSampleVariablesToLinearProgram(LinearProgram* linear_program) {
  const std::string name_x("x");
  const ColIndex col_x = linear_program->FindOrCreateVariable(name_x);
  const Fractional kLowerBoundX(2);
  const Fractional kUpperBoundX(3);
  linear_program->SetVariableBounds(col_x, kLowerBoundX, kUpperBoundX);

  const std::string name_y("y");
  const ColIndex col_y = linear_program->FindOrCreateVariable(name_y);
  const Fractional kLowerBoundY(-10);
  const Fractional kUpperBoundY(10);
  const Fractional kObjectiveCoeffY(4);
  linear_program->SetObjectiveCoefficient(col_y, kObjectiveCoeffY);
  linear_program->SetVariableBounds(col_y, kLowerBoundY, kUpperBoundY);
}

void FillSampleLinearProgram(LinearProgram* linear_program) {
  AddSampleVariablesToLinearProgram(linear_program);

  const ColIndex col_x = linear_program->FindOrCreateVariable("x");
  const ColIndex col_y = linear_program->FindOrCreateVariable("y");

  const std::string name_ct("ct");
  const RowIndex row_ct = linear_program->FindOrCreateConstraint(name_ct);
  const Fractional kLowerBoundCT(-7);
  const Fractional kUpperBoundCT(3);
  linear_program->SetConstraintBounds(row_ct, kLowerBoundCT, kUpperBoundCT);

  linear_program->SetCoefficient(row_ct, col_x, Fractional(10));
  linear_program->SetCoefficient(row_ct, col_y, Fractional(-1));
  linear_program->SetMaximizationProblem(true);
  linear_program->CleanUp();
}

TEST(LinearProgramTest, Dump) {
  LinearProgram linear_program;
  FillSampleLinearProgram(&linear_program);
  const std::string kDump(
      "max: + 4 y;\n"
      "ct: -7 <= + 10 x - y <= 3;\n"
      "2 <= x <= 3;\n"
      "-10 <= y <= 10;\n");
  EXPECT_EQ(kDump, linear_program.Dump());
}

TEST(LinearProgramTest, DumpWithInteger) {
  LinearProgram linear_program;
  FillSampleLinearProgram(&linear_program);
  linear_program.SetVariableType(ColIndex(0),
                                 LinearProgram::VariableType::INTEGER);
  linear_program.SetVariableType(ColIndex(1),
                                 LinearProgram::VariableType::INTEGER);
  const std::string kDump(
      "max: + 4 y;\n"
      "ct: -7 <= + 10 x - y <= 3;\n"
      "2 <= x <= 3;\n"
      "-10 <= y <= 10;\n"
      "int x y;\n");
  EXPECT_EQ(kDump, linear_program.Dump());
}

TEST(LinearProgramTest, DumpSolution) {
  LinearProgram linear_program;
  FillSampleLinearProgram(&linear_program);
  const DenseRow kSolution = {1.0, 2.0};
  const std::string kSolutionDump = "x = 1, y = 2";
  EXPECT_EQ(kSolutionDump, linear_program.DumpSolution(kSolution));
}

TEST(LinearProgramTest, Clear) {
  LinearProgram linear_program;
  const std::string name("x");
  linear_program.FindOrCreateVariable(name);
  const ColIndex num_variables = linear_program.num_variables();
  EXPECT_EQ(1, num_variables);

  linear_program.Clear();
  const ColIndex new_num_variables = linear_program.num_variables();
  EXPECT_EQ(0, new_num_variables);
}

TEST(LinearProgramTest, AddSlackVariablesWhereNecessary) {
  LinearProgram linear_program;
  const ColIndex x = linear_program.FindOrCreateVariable("x");
  const ColIndex y = linear_program.FindOrCreateVariable("y");
  linear_program.SetVariableType(y, LinearProgram::VariableType::INTEGER);
  for (int i = 0; i < 3; ++i) {
    linear_program.SetCoefficient(RowIndex(i), x, 1.0);
  }
  linear_program.SetConstraintBounds(RowIndex(0), 0, kInfinity);
  linear_program.SetConstraintBounds(RowIndex(1), -kInfinity, kInfinity);
  linear_program.SetConstraintBounds(RowIndex(2), -10, 20);
  linear_program.SetCoefficient(RowIndex(3), y, 1.0);
  linear_program.SetConstraintBounds(RowIndex(3), -kInfinity, 50);
  linear_program.SetCoefficient(RowIndex(4), x, 1.0);
  linear_program.SetCoefficient(RowIndex(4), y, 1.0);
  linear_program.SetConstraintBounds(RowIndex(4), -10.0, 10.0);
  linear_program.SetCoefficient(RowIndex(5), y, 1.5);
  linear_program.SetConstraintBounds(RowIndex(5), -1.0, 1.0);

  LinearProgram with_int_slack;
  with_int_slack.PopulateFromLinearProgram(linear_program);
  with_int_slack.AddSlackVariablesWhereNecessary(true);
  const std::string kIntSlackDump(
      "min:;\n"
      "r0: + x + s0 = 0;\n"
      "r1: + x + s1 = 0;\n"
      "r2: + x + s2 = 0;\n"
      "r3: + y + s3 = 0;\n"
      "r4: + x + y + s4 = 0;\n"
      "r5: + 1.5 y + s5 = 0;\n"
      "x >= 0;\n"
      "y >= 0;\n"
      "s0 <= -0;\n"
      "-inf <= s1 <= inf;\n"
      "-20 <= s2 <= 10;\n"
      "s3 >= -50;\n"
      "-10 <= s4 <= 10;\n"
      "-1 <= s5 <= 1;\n"
      "int y s3;\n");
  const ColIndex kExpectedFirstSlackVariable(2);
  EXPECT_EQ(kIntSlackDump, with_int_slack.Dump());
  EXPECT_EQ(kExpectedFirstSlackVariable,
            with_int_slack.GetFirstSlackVariable());
  EXPECT_EQ(kExpectedFirstSlackVariable,
            with_int_slack.GetSlackVariable(RowIndex(0)));

  LinearProgram with_real_slack;
  with_real_slack.PopulateFromLinearProgram(linear_program);
  with_real_slack.AddSlackVariablesWhereNecessary(false);
  const std::string kRealSlackDump(
      "min:;\n"
      "r0: + x + s0 = 0;\n"
      "r1: + x + s1 = 0;\n"
      "r2: + x + s2 = 0;\n"
      "r3: + y + s3 = 0;\n"
      "r4: + x + y + s4 = 0;\n"
      "r5: + 1.5 y + s5 = 0;\n"
      "x >= 0;\n"
      "y >= 0;\n"
      "s0 <= -0;\n"
      "-inf <= s1 <= inf;\n"
      "-20 <= s2 <= 10;\n"
      "s3 >= -50;\n"
      "-10 <= s4 <= 10;\n"
      "-1 <= s5 <= 1;\n"
      "int y;\n");
  EXPECT_EQ(kRealSlackDump, with_real_slack.Dump());
  EXPECT_EQ(kExpectedFirstSlackVariable,
            with_real_slack.GetFirstSlackVariable());
}

TEST(LinearProgramTest, AddSlackVariablesForAllEligibleRows) {
  LinearProgram linear_program;
  const ColIndex x = linear_program.FindOrCreateVariable("x");
  const ColIndex y = linear_program.FindOrCreateVariable("y");
  linear_program.SetVariableType(y, LinearProgram::VariableType::INTEGER);
  for (int i = 0; i < 3; ++i) {
    linear_program.SetCoefficient(RowIndex(i), x, 1.0);
  }
  linear_program.SetConstraintBounds(RowIndex(0), 0, kInfinity);
  linear_program.SetConstraintBounds(RowIndex(1), -kInfinity, kInfinity);
  linear_program.SetConstraintBounds(RowIndex(2), -10, 20);
  linear_program.SetCoefficient(RowIndex(3), y, 1.0);
  linear_program.SetConstraintBounds(RowIndex(3), -kInfinity, 50);
  linear_program.SetCoefficient(RowIndex(4), x, 1.0);
  linear_program.SetCoefficient(RowIndex(4), y, 1.0);
  linear_program.SetConstraintBounds(RowIndex(4), -10.0, 10.0);
  linear_program.SetCoefficient(RowIndex(5), y, 1.5);
  linear_program.SetConstraintBounds(RowIndex(5), -1.0, 1.0);

  LinearProgram with_int_slack;
  with_int_slack.PopulateFromLinearProgram(linear_program);
  with_int_slack.AddSlackVariablesWhereNecessary(true);
  with_int_slack.SetCoefficient(RowIndex(6), y, 1.6);
  with_int_slack.SetConstraintBounds(RowIndex(6), -2.0, 2.0);
  with_int_slack.AddSlackVariablesWhereNecessary(true);
  // Calling the same method without adding any new constraint.
  with_int_slack.AddSlackVariablesWhereNecessary(true);
  const std::string kIntSlackDump(
      "min:;\n"
      "r0: + x + s0 = 0;\n"
      "r1: + x + s1 = 0;\n"
      "r2: + x + s2 = 0;\n"
      "r3: + y + s3 = 0;\n"
      "r4: + x + y + s4 = 0;\n"
      "r5: + 1.5 y + s5 = 0;\n"
      "r6: + 1.6 y + s6 = 0;\n"
      "x >= 0;\n"
      "y >= 0;\n"
      "s0 <= -0;\n"
      "-inf <= s1 <= inf;\n"
      "-20 <= s2 <= 10;\n"
      "s3 >= -50;\n"
      "-10 <= s4 <= 10;\n"
      "-1 <= s5 <= 1;\n"
      "-2 <= s6 <= 2;\n"
      "int y s3;\n");
  const ColIndex kExpectedFirstSlackVariable(2);
  EXPECT_EQ(kIntSlackDump, with_int_slack.Dump());
  EXPECT_EQ(kExpectedFirstSlackVariable,
            with_int_slack.GetFirstSlackVariable());
  EXPECT_EQ(kExpectedFirstSlackVariable,
            with_int_slack.GetSlackVariable(RowIndex(0)));
}

TEST(LinearProgramTest, GetTransposeSparseMatrix) {
  LinearProgram linear_program;
  EXPECT_TRUE(linear_program.GetTransposeSparseMatrix().IsEmpty());
  EXPECT_EQ(ColIndex(0), linear_program.GetTransposeSparseMatrix().num_cols());
  EXPECT_EQ(RowIndex(0), linear_program.GetTransposeSparseMatrix().num_rows());
  linear_program.CreateNewConstraint();
  EXPECT_EQ(ColIndex(1), linear_program.GetTransposeSparseMatrix().num_cols());
  EXPECT_EQ(RowIndex(0), linear_program.GetTransposeSparseMatrix().num_rows());
  linear_program.CreateNewVariable();
  EXPECT_EQ(ColIndex(1), linear_program.GetTransposeSparseMatrix().num_cols());
  EXPECT_EQ(RowIndex(1), linear_program.GetTransposeSparseMatrix().num_rows());
}

TEST(LinearProgramTest, GetMutableTransposeSparseMatrix) {
  LinearProgram lp;
  for (int i = 0; i < 2; ++i) {
    lp.CreateNewVariable();
    lp.CreateNewConstraint();
  }
  lp.SetCoefficient(RowIndex(0), ColIndex(0), 1.0);
  lp.SetCoefficient(RowIndex(0), ColIndex(1), 2.0);
  lp.SetCoefficient(RowIndex(1), ColIndex(1), 1.0);

  // Note that the dump is row by row.
  EXPECT_EQ(
      "{ 1 2 }\n"
      "{ 0 1 }\n",
      lp.GetSparseMatrix().Dump());
  EXPECT_EQ(
      "{ 1 0 }\n"
      "{ 2 1 }\n",
      lp.GetTransposeSparseMatrix().Dump());

  // We delete a row of the matrix (a column of the transposed matrix).
  SparseMatrix* transpose = lp.GetMutableTransposeSparseMatrix();
  EXPECT_EQ(
      "{ 1 0 }\n"
      "{ 2 1 }\n",
      transpose->Dump());
  transpose->mutable_column(ColIndex(1))->Clear();
  EXPECT_EQ(
      "{ 1 0 }\n"
      "{ 2 0 }\n",
      transpose->Dump());

  // Calling GetTransposeSparseMatrix() will reset the matrix.
  EXPECT_EQ(
      "{ 1 0 }\n"
      "{ 2 1 }\n",
      lp.GetTransposeSparseMatrix().Dump());
  EXPECT_EQ(
      "{ 1 0 }\n"
      "{ 2 1 }\n",
      transpose->Dump());

  // Calling UseTransposeMatrixAsReference() will push the change.
  transpose->mutable_column(ColIndex(1))->Clear();
  lp.UseTransposeMatrixAsReference();
  EXPECT_EQ(
      "{ 1 2 }\n"
      "{ 0 0 }\n",
      lp.GetSparseMatrix().Dump());
  EXPECT_EQ(
      "{ 1 0 }\n"
      "{ 2 0 }\n",
      lp.GetTransposeSparseMatrix().Dump());
}

// Used as a witness to detect new/updated fields of the "LinearProgram" class,
// so that we can remind authors to update the swap and copy methods.
//
// IMPORTANT:
// - When updating this, respect the ordering of the fields of the original
//   class! It matters for the value of sizeof(LinearProgramClone).
// - Update the Swap() and PopulateFromLinearProgram() methods in
//   LinearProgram.
struct LinearProgramClone {
  SparseMatrix matrix;
  SparseMatrix transpose_matrix;

  DenseColumn row_lower_bound;
  DenseColumn row_upper_bound;
  StrictITIVector<RowIndex, std::string> row_name;

  DenseRow objective_coefficient;
  DenseRow column_lower_bound;
  DenseRow column_upper_bound;
  StrictITIVector<ColIndex, std::string> column_name;
  StrictITIVector<ColIndex, VariableType> column_type;
  std::vector<ColIndex> integer_variables_list;
  std::vector<ColIndex> binary_variables_list;
  std::vector<ColIndex> non_binary_variables_list;

  absl::flat_hash_map<std::string, ColIndex> column_table;
  absl::flat_hash_map<std::string, RowIndex> row_table;

  Fractional objective_offset;
  Fractional objective_scaling_factor;
  bool maximize;
  mutable bool are_columns_ordered_by_row;
  mutable bool integer_variables_list_is_consistent;
  std::string name;
  ColIndex first_slack_variable;
};

TEST(LinearProgramTest, PopulateFromLinearProgram) {
  LinearProgram linear_program;
  FillSampleLinearProgram(&linear_program);

  LinearProgram copy;
  copy.PopulateFromLinearProgram(linear_program);
  EXPECT_EQ(linear_program.Dump(), copy.Dump());
  EXPECT_EQ(sizeof(LinearProgramClone), sizeof(LinearProgram));
}

TEST(LinearProgramTest, PopulateFromPermutedLinearProgram) {
  LinearProgram lp;
  const std::string kLinearProgram = R"(
      max: 10 + 1 a + 2 b + 3 c + 4 d + 5 e;
      0 <= a <= 1;
      2 <= b <= 3;
      4 <= c <= 5;
      6 <= d <= 7;
      8 <= e <= 9;
      a + b <= 1;
      b + c <= 2;
      c + d <= 3;
      d + e <= 4;
      -a - e >= 0;
      int: a, e;
      )";
  ASSERT_TRUE(ParseLp(kLinearProgram, &lp));

  // Bunch of extras to make one liner "mutant" testing happy.
  lp.SetName("Some name");
  lp.SetObjectiveScalingFactor(2.0);
  for (ColIndex col(0); col < lp.num_variables(); ++col) {
    lp.SetVariableName(col, absl::Substitute("var_$0", col.value()));
  }
  for (RowIndex row(0); row < lp.num_constraints(); ++row) {
    lp.SetConstraintName(row, absl::Substitute("ct_$0", row.value()));
  }
  // We access transpose matrix to make it consistent (and to verify it will be
  // recomputed).
  lp.GetTransposeSparseMatrix();

  // Create and verify permuted lp.
  RowPermutation row_permutation(lp.num_constraints());
  row_permutation.PopulateRandomly();
  ColumnPermutation col_permutation(lp.num_variables());
  col_permutation.PopulateRandomly();
  LinearProgram permuted_copy;
  // We initialize to the original lp, to verify if permuted copy really gets
  // overwritten.
  ASSERT_TRUE(ParseLp(kLinearProgram, &permuted_copy));
  // We add slacks to ensure they will be removed.
  permuted_copy.AddSlackVariablesWhereNecessary(
      /*detect_integer_constraints=*/true);
  permuted_copy.PopulateFromPermutedLinearProgram(lp, row_permutation,
                                                  col_permutation);
  EXPECT_FALSE(lp.GetTransposeSparseMatrix().Equals(
      permuted_copy.GetTransposeSparseMatrix(), /*tolerance=*/1e-3));
  EXPECT_EQ(permuted_copy.GetFirstSlackVariable(), kInvalidCol);
  EXPECT_NE(lp.Dump(), permuted_copy.Dump());

  // Create and verify unpermuted lp.
  RowPermutation inverse_row_permutation(lp.num_constraints());
  inverse_row_permutation.PopulateFromInverse(row_permutation);
  ColumnPermutation inverse_col_permutation(lp.num_variables());
  inverse_col_permutation.PopulateFromInverse(col_permutation);
  // We populate the unpermuted copy into an empty lp to verify that
  // objective direction, objective scaling factor, etc. gets propagated.
  LinearProgram unpermuted_copy;
  unpermuted_copy.PopulateFromPermutedLinearProgram(
      permuted_copy, inverse_row_permutation, inverse_col_permutation);
  EXPECT_TRUE(lp.GetTransposeSparseMatrix().Equals(
      unpermuted_copy.GetTransposeSparseMatrix(), /*tolerance=*/0.0));
  EXPECT_EQ(unpermuted_copy.GetFirstSlackVariable(), kInvalidCol);
  EXPECT_EQ(lp.Dump(), unpermuted_copy.Dump());
}

TEST(LinearProgramTest, PopulateFromLinearProgramVariables) {
  LinearProgram linear_program;
  FillSampleLinearProgram(&linear_program);
  EXPECT_LT(0, linear_program.num_constraints());

  LinearProgram cleaned_linear_program;
  cleaned_linear_program.PopulateFromLinearProgram(linear_program);
  DenseBooleanColumn all_constraints(cleaned_linear_program.num_constraints(),
                                     true);
  cleaned_linear_program.DeleteRows(all_constraints);

  LinearProgram copy;
  copy.PopulateFromLinearProgramVariables(linear_program);
  EXPECT_EQ(linear_program.num_variables(), copy.num_variables());
  EXPECT_EQ(cleaned_linear_program.Dump(), copy.Dump());
}

TEST(LinearProgramTest, AddConstraints) {
  LinearProgram linear_program;
  FillSampleLinearProgram(&linear_program);

  const SparseMatrix coefficients{
      {3, 5},  // 0
      {2, 1}   // 1
  };
  const DenseColumn left_hand_sides = {-kInfinity, 1};
  const DenseColumn right_hand_sides = {5, kInfinity};
  const StrictITIVector<RowIndex, std::string> names = {"ct2", "ct3"};

  const std::string kExpectedDump =
      "max: + 4 y;\n"
      "ct: -7 <= + 10 x - y <= 3;\n"
      "ct2: + 3 x + 5 y <= 5;\n"
      "ct3: + 2 x + y >= 1;\n"
      "2 <= x <= 3;\n"
      "-10 <= y <= 10;\n";

  linear_program.AddConstraints(coefficients, left_hand_sides, right_hand_sides,
                                names);
  EXPECT_EQ(kExpectedDump, linear_program.Dump());
}

TEST(LinearProgramTest, AddConstraintsWithSlackVariables) {
  LinearProgram linear_program;
  FillSampleLinearProgram(&linear_program);
  linear_program.AddSlackVariablesWhereNecessary(true);

  const SparseMatrix coefficients{
      {3, 5, 0},  // 0
      {2, 1, 0}   // 1
  };
  const DenseColumn left_hand_sides = {-kInfinity, 1};
  const DenseColumn right_hand_sides = {5, kInfinity};
  const StrictITIVector<RowIndex, std::string> names = {"ct2", "ct3"};

  const std::string kExpectedDump =
      "max: + 4 y;\n"
      "ct: + 10 x - y + s0 = 0;\n"
      "ct2: + 3 x + 5 y + s1 = 0;\n"
      "ct3: + 2 x + y + s2 = 0;\n"
      "2 <= x <= 3;\n"
      "-10 <= y <= 10;\n"
      "-3 <= s0 <= 7;\n"
      "s1 >= -5;\n"
      "s2 <= -1;\n";

  linear_program.AddConstraintsWithSlackVariables(
      coefficients, left_hand_sides, right_hand_sides, names, true);
  EXPECT_EQ(kExpectedDump, linear_program.Dump());
}

TEST(LinearProgramTest, UpdateVariableBoundsToIntersection) {
  LinearProgram linear_program;
  FillSampleLinearProgram(&linear_program);

  const DenseRow lower_bounds = {-kInfinity, -1};
  const DenseRow upper_bounds = {kInfinity, 5};
  const DenseRow expected_lower_bounds = {2, -1};
  const DenseRow expected_upper_bounds = {3, 5};
  EXPECT_TRUE(linear_program.UpdateVariableBoundsToIntersection(lower_bounds,
                                                                upper_bounds));
  EXPECT_EQ(expected_lower_bounds, linear_program.variable_lower_bounds());
  EXPECT_EQ(expected_upper_bounds, linear_program.variable_upper_bounds());

  // Verify that if the new variable bounds are not feasible,
  // UpdateVariableBoundsToIntersection returns false and does not modify the
  // bounds in the linear program.
  const DenseRow infeasible_lower_bounds = {2, -30};
  const DenseRow infeasible_upper_bounds = {3, -24};
  EXPECT_FALSE(linear_program.UpdateVariableBoundsToIntersection(
      infeasible_lower_bounds, infeasible_upper_bounds));
  EXPECT_EQ(expected_lower_bounds, linear_program.variable_lower_bounds());
  EXPECT_EQ(expected_upper_bounds, linear_program.variable_upper_bounds());
}

TEST(LinearProgramTest, Swap) {
  LinearProgram copy;
  LinearProgram linear_program;
  FillSampleLinearProgram(&linear_program);

  EXPECT_EQ(0, copy.num_variables());
  EXPECT_EQ(2, linear_program.num_variables());

  copy.Swap(&linear_program);
  EXPECT_EQ(0, linear_program.num_variables());

  const std::string kDump(
      "max: + 4 y;\n"
      "ct: -7 <= + 10 x - y <= 3;\n"
      "2 <= x <= 3;\n"
      "-10 <= y <= 10;\n");
  EXPECT_EQ(kDump, copy.Dump());

  EXPECT_EQ(sizeof(LinearProgramClone), sizeof(LinearProgram));
}

TEST(LinearProgram, SwapWithSlackVariables) {
  LinearProgram copy;
  LinearProgram linear_program;
  FillSampleLinearProgram(&linear_program);
  linear_program.AddSlackVariablesWhereNecessary(false);

  EXPECT_EQ(0, copy.num_variables());
  EXPECT_EQ(3, linear_program.num_variables());
  EXPECT_EQ(ColIndex(2), linear_program.GetFirstSlackVariable());

  copy.Swap(&linear_program);
  EXPECT_EQ(0, linear_program.num_variables());
  EXPECT_EQ(kInvalidCol, linear_program.GetFirstSlackVariable());

  const std::string kDump(
      "max: + 4 y;\n"
      "ct: + 10 x - y + s0 = 0;\n"
      "2 <= x <= 3;\n"
      "-10 <= y <= 10;\n"
      "-3 <= s0 <= 7;\n");
  EXPECT_EQ(kDump, copy.Dump());
  EXPECT_EQ(ColIndex(2), copy.GetFirstSlackVariable());

  EXPECT_EQ(sizeof(LinearProgramClone), sizeof(LinearProgram));
}

TEST(LinearProgramTest, DeleteSlackVariables) {
  LinearProgram linear_program;
  FillSampleLinearProgram(&linear_program);
  // Add slack variables and delete them right away. The result should be an
  // unmodified linear program.
  linear_program.AddSlackVariablesWhereNecessary(false);
  linear_program.DeleteSlackVariables();
  const std::string kDump(
      "max: + 4 y;\n"
      "ct: -7 <= + 10 x - y <= 3;\n"
      "2 <= x <= 3;\n"
      "-10 <= y <= 10;\n");
  EXPECT_EQ(kDump, linear_program.Dump());
}

TEST(LinearProgramTest, DeleteColumns) {
  LinearProgram linear_program;
  linear_program.FindOrCreateVariable("a");
  linear_program.FindOrCreateVariable("b");
  linear_program.FindOrCreateVariable("c");
  linear_program.FindOrCreateVariable("d");
  linear_program.FindOrCreateVariable("e");
  linear_program.FindOrCreateVariable("f");
  EXPECT_EQ(6, linear_program.num_variables());

  // Note that the Boolean vector is shorter on purpose.
  DenseBooleanRow to_delete(ColIndex(4), false);
  to_delete[ColIndex(1)] = true;
  to_delete[ColIndex(3)] = true;

  linear_program.DeleteColumns(to_delete);
  EXPECT_EQ(4, linear_program.num_variables());
  EXPECT_EQ(0, linear_program.FindOrCreateVariable("a").value());
  EXPECT_EQ(1, linear_program.FindOrCreateVariable("c").value());
  EXPECT_EQ(2, linear_program.FindOrCreateVariable("e").value());
  EXPECT_EQ(3, linear_program.FindOrCreateVariable("f").value());
}

TEST(LinearProgramTest, DeleteRows) {
  LinearProgram linear_program;
  linear_program.FindOrCreateConstraint("a");
  linear_program.FindOrCreateConstraint("b");
  linear_program.FindOrCreateConstraint("c");
  linear_program.FindOrCreateConstraint("d");
  linear_program.FindOrCreateConstraint("e");
  linear_program.SetConstraintBounds(RowIndex(4), -10.0, 10.0);
  linear_program.FindOrCreateConstraint("f");
  EXPECT_EQ(6, linear_program.num_constraints());

  // Note that the Boolean vector is shorter on purpose.
  DenseBooleanColumn to_delete(RowIndex(4), false);
  to_delete[RowIndex(1)] = true;
  to_delete[RowIndex(3)] = true;

  linear_program.DeleteRows(to_delete);
  EXPECT_EQ(4, linear_program.num_constraints());
  EXPECT_EQ(0, linear_program.FindOrCreateConstraint("a").value());
  EXPECT_EQ(1, linear_program.FindOrCreateConstraint("c").value());
  EXPECT_EQ(2, linear_program.FindOrCreateConstraint("e").value());
  EXPECT_EQ(3, linear_program.FindOrCreateConstraint("f").value());
  EXPECT_EQ(-10.0, linear_program.constraint_lower_bounds()[RowIndex(2)]);
  EXPECT_EQ(10.0, linear_program.constraint_upper_bounds()[RowIndex(2)]);
}

// TODO(user): find a better place for this test.
TEST(LinearProgramTest, IsFinite) {
  EXPECT_FALSE(IsFinite(std::numeric_limits<double>::quiet_NaN()));
  EXPECT_FALSE(IsFinite(std::numeric_limits<double>::infinity()));
  EXPECT_FALSE(IsFinite(-std::numeric_limits<double>::infinity()));
  EXPECT_TRUE(IsFinite(0.0));
}

TEST(LinearProgramTest, EmptySolutionIsLPFeasible) {
  LinearProgram linear_program;
  const Fractional tolerance = 1e-7;
  EXPECT_EQ(true, linear_program.SolutionIsLPFeasible({}, tolerance));
}

TEST(LinearProgramTest, EmptySolutionIsInteger) {
  LinearProgram linear_program;
  const Fractional tolerance = 1e-7;
  EXPECT_EQ(true, linear_program.SolutionIsInteger({}, tolerance));
}

TEST(LinearProgramTest, EmptySolutionIsMIPFeasible) {
  LinearProgram linear_program;
  const Fractional tolerance = 1e-7;
  EXPECT_EQ(true, linear_program.SolutionIsMIPFeasible({}, tolerance));
}

TEST(LinearProgramTest, SolutionIsLPFeasibleWithinTolerance) {
  LinearProgram linear_program;
  const Fractional tolerance = 1e-7;
  const std::string kLinearProgram =
      "min: x + y;"
      "x <= 10;"
      "y >= 0;"
      "x + y <= 11;";
  ASSERT_TRUE(ParseLp(kLinearProgram, &linear_program));
  EXPECT_TRUE(linear_program.SolutionIsLPFeasible({5, 6}, tolerance));
  EXPECT_TRUE(linear_program.SolutionIsLPFeasible({10 + tolerance, -tolerance},
                                                  tolerance));
  EXPECT_FALSE(linear_program.SolutionIsLPFeasible(
      {10 + 1.01 * tolerance, -tolerance}, tolerance));
  EXPECT_FALSE(linear_program.SolutionIsLPFeasible(
      {10 + tolerance, -1.01 * tolerance}, tolerance));
  EXPECT_FALSE(linear_program.SolutionIsLPFeasible({-kInfinity, 1}, tolerance));
  EXPECT_FALSE(linear_program.SolutionIsLPFeasible({1.0, kNaN}, tolerance));
}

TEST(LinearProgramTest, ComputeSlackVariableValues) {
  LinearProgram linear_program;
  const std::string kLinearProgram =
      "min: x + y;"
      "x - 2 * y <= 20;"
      "x <= 10;"
      "y >= 0;"
      "x + y <= 11;";
  ASSERT_TRUE(ParseLp(kLinearProgram, &linear_program));
  linear_program.AddSlackVariablesWhereNecessary(true);
  DenseRow solution1 = {5, 6, 0.0, 0.0};
  linear_program.ComputeSlackVariableValues(&solution1);
  EXPECT_THAT(solution1, ElementsAre(5.0, 6.0, 7.0, -11.0));
  DenseRow solution2 = {10.5, 3, 0.0, 0.0};
  linear_program.ComputeSlackVariableValues(&solution2);
  EXPECT_THAT(solution2, ElementsAre(10.5, 3, -4.5, -13.5));
}

TEST(LinearProgramTest, SolutionIsIntegerWithinTolerance) {
  LinearProgram linear_program;
  const Fractional tolerance = 1e-7;
  ASSERT_TRUE(ParseLp("max: a + b + c; int b, c", &linear_program));
  EXPECT_TRUE(linear_program.SolutionIsInteger({0.5, 1.0, 2.0}, tolerance));
  EXPECT_TRUE(linear_program.SolutionIsInteger(
      {0.5, 1.0 - tolerance, 2.0 + tolerance}, tolerance));
  EXPECT_FALSE(linear_program.SolutionIsInteger(
      {0.5, 1.0 - 1.1 * tolerance, 2.0}, tolerance));
  EXPECT_TRUE(
      linear_program.SolutionIsInteger({kInfinity, 1.0, 2.0}, tolerance));
  EXPECT_FALSE(linear_program.SolutionIsInteger(
      {0.5, kInfinity, 2.0 + tolerance}, tolerance));
  EXPECT_FALSE(linear_program.SolutionIsInteger({0.5, kNaN, 2.0}, tolerance));
}

TEST(LinearProgramTest, ApplyAndRemoveObjectiveScalingAndOffset) {
  LinearProgram linear_program;
  const Fractional kEpsilon = 1e-6;
  // Test applying and removing the factor and offset on some "random" cases.
  for (const Fractional factor : {1.0, -1.0, 13.13, -1001.0}) {
    for (const Fractional offset : {0.0, -1.0, 1.0, -123.49, 1e9}) {
      linear_program.SetObjectiveScalingFactor(factor);
      linear_program.SetObjectiveOffset(offset);
      for (const Fractional original_value : {0.0, -1.0, 2.3, -11.4, 27.2}) {
        const Fractional translated_value = factor * (offset + original_value);
        EXPECT_COMPARABLE(
            translated_value,
            linear_program.ApplyObjectiveScalingAndOffset(original_value),
            kEpsilon);
        EXPECT_COMPARABLE(
            original_value,
            linear_program.RemoveObjectiveScalingAndOffset(translated_value),
            kEpsilon);
      }
    }
  }
}

// Note that not all cases are covered, but thanks to input fuzzer, any test we
// miss will likely result in a floating point overflow in cause issues.
TEST(LinearProgramTest, IsValidWithLargeValueThreshold) {
  LinearProgram linear_program;
  EXPECT_TRUE(linear_program.IsValid());

  const Fractional limit = 10.0;
  const Fractional epsilon = 1e-6;

  // Test variable bounds.
  {
    linear_program.Clear();
    const ColIndex col = linear_program.CreateNewVariable();
    linear_program.SetVariableBounds(col, -limit, limit);
    EXPECT_TRUE(linear_program.IsValid(limit));
    linear_program.SetVariableBounds(col, 0.0, limit + epsilon);
    EXPECT_FALSE(linear_program.IsValid(limit));
  }

  // Test objective.
  {
    linear_program.Clear();
    const ColIndex col = linear_program.CreateNewVariable();
    linear_program.SetObjectiveCoefficient(col, limit);
    EXPECT_TRUE(linear_program.IsValid(limit));
    linear_program.SetObjectiveCoefficient(col, limit + epsilon);
    EXPECT_FALSE(linear_program.IsValid(limit));
  }

  // Test constraints bounds.
  {
    linear_program.Clear();
    const RowIndex row = linear_program.CreateNewConstraint();
    linear_program.SetConstraintBounds(row, -limit, limit);
    EXPECT_TRUE(linear_program.IsValid(limit));
    linear_program.SetConstraintBounds(row, -limit - epsilon, 0);
    EXPECT_FALSE(linear_program.IsValid(limit));
  }

  // Test matrix coefficients.
  {
    linear_program.Clear();
    const ColIndex col = linear_program.CreateNewVariable();
    const RowIndex row = linear_program.CreateNewConstraint();
    linear_program.SetCoefficient(row, col, limit);
    EXPECT_TRUE(linear_program.IsValid(limit));
    linear_program.SetCoefficient(row, col, limit + epsilon);
    EXPECT_FALSE(linear_program.IsValid(limit));
  }
}

#ifdef NDEBUG

TEST(LinearProgramTest, IsValid) {
  LinearProgram linear_program;
  EXPECT_TRUE(linear_program.IsValid());
  const Fractional infinity = std::numeric_limits<double>::infinity();
  const Fractional nan = std::numeric_limits<double>::quiet_NaN();
  const ColIndex col = linear_program.CreateNewVariable();
  linear_program.SetVariableBounds(col, 0.0, infinity);
  EXPECT_TRUE(linear_program.IsValid());
  linear_program.SetVariableBounds(col, 0.0, nan);
  EXPECT_FALSE(linear_program.IsValid());

  linear_program.Clear();
  const ColIndex col2 = linear_program.CreateNewVariable();
  EXPECT_TRUE(linear_program.IsValid());
  linear_program.SetVariableBounds(col2, 10, -10);
  EXPECT_FALSE(linear_program.IsValid());

  linear_program.Clear();
  const ColIndex col3 = linear_program.CreateNewVariable();
  EXPECT_TRUE(linear_program.IsValid());
  linear_program.SetObjectiveCoefficient(col3, kInfinity);
  EXPECT_FALSE(linear_program.IsValid());
  linear_program.SetObjectiveCoefficient(col3, 10.0);
  EXPECT_TRUE(linear_program.IsValid());

  linear_program.SetObjectiveOffset(infinity);
  EXPECT_FALSE(linear_program.IsValid());
}

TEST(LinearProgramDeathTest, SolutionIsLPFeasibleDoesNotDieInProd) {
  LinearProgram linear_program;
  const Fractional tolerance = 1e-7;
  EXPECT_FALSE(linear_program.SolutionIsLPFeasible({1, 2, 3}, tolerance));
}

TEST(LinearProgramDeathTest, SolutionIsIntegerDoesNotDieInProd) {
  LinearProgram linear_program;
  const Fractional tolerance = 1e-7;
  EXPECT_FALSE(linear_program.SolutionIsInteger({1, 2, 3}, tolerance));
}

TEST(LinearProgramDeathTest, SolutionIsMIPFeasibleDoesNotDieInProd) {
  LinearProgram linear_program;
  const Fractional tolerance = 1e-7;
  EXPECT_FALSE(linear_program.SolutionIsMIPFeasible({1, 2, 3}, tolerance));
}

#else  // NDEBUG

TEST(LinearProgramDeathTest, IsValid) {
  LinearProgram linear_program;
  EXPECT_TRUE(linear_program.IsValid());
  const Fractional infinity = std::numeric_limits<double>::infinity();
  const Fractional nan = std::numeric_limits<double>::quiet_NaN();
  const ColIndex col = linear_program.CreateNewVariable();
  const RowIndex row(0);
  linear_program.SetVariableBounds(col, 0.0, infinity);
  EXPECT_TRUE(linear_program.IsValid());
  ASSERT_DEATH(linear_program.SetVariableBounds(col, 0.0, nan), "");
  ASSERT_DEATH(linear_program.SetVariableBounds(col, nan, 0.0), "");
  ASSERT_DEATH(linear_program.SetVariableBounds(col, infinity, infinity), "");
  ASSERT_DEATH(linear_program.SetVariableBounds(col, -infinity, -infinity), "");
  ASSERT_DEATH(linear_program.SetVariableBounds(col, 10, -10), "");
  linear_program.SetConstraintBounds(row, -infinity, 0.0);
  ASSERT_DEATH(linear_program.SetConstraintBounds(row, nan, 0.0), "");
  ASSERT_DEATH(linear_program.SetConstraintBounds(row, 0.0, nan), "");
  ASSERT_DEATH(linear_program.SetConstraintBounds(row, infinity, infinity), "");
  ASSERT_DEATH(linear_program.SetConstraintBounds(row, -infinity, -infinity),
               "");
  ASSERT_DEATH(linear_program.SetCoefficient(row, col, nan), "");
  ASSERT_DEATH(linear_program.SetCoefficient(row, col, -infinity), "");
  ASSERT_DEATH(linear_program.SetCoefficient(row, col, infinity), "");
  ASSERT_DEATH(linear_program.SetObjectiveOffset(nan), "");
  ASSERT_DEATH(linear_program.SetObjectiveOffset(infinity), "");
  ASSERT_DEATH(linear_program.SetObjectiveOffset(-infinity), "");
  ASSERT_DEATH(linear_program.SetObjectiveCoefficient(col, infinity), "");
}

TEST(LinearProgramDeathTest, SolutionIsLPFeasibleDiesInDebug) {
  LinearProgram linear_program;
  const Fractional tolerance = 1e-7;
  ASSERT_DEATH(linear_program.SolutionIsLPFeasible({1, 2, 3}, tolerance), "");
}

TEST(LinearProgramDeathTest, SolutionIsIntegerDiesInDebug) {
  LinearProgram linear_program;
  const Fractional tolerance = 1e-7;
  ASSERT_DEATH(linear_program.SolutionIsInteger({1, 2, 3}, tolerance), "");
}

TEST(LinearProgramDeathTest, SolutionIsMIPFeasibleDiesInDebug) {
  LinearProgram linear_program;
  const Fractional tolerance = 1e-7;
  ASSERT_DEATH(linear_program.SolutionIsMIPFeasible({1, 2, 3}, tolerance), "");
}

#endif

TEST(LinearProgramTest, AreBoundsValid) {
  const Fractional infinity = std::numeric_limits<double>::infinity();
  const Fractional nan = std::numeric_limits<double>::quiet_NaN();
  EXPECT_TRUE(AreBoundsValid(0, 1));
  EXPECT_TRUE(AreBoundsValid(-1, -1));
  EXPECT_TRUE(AreBoundsValid(-infinity, infinity));
  EXPECT_FALSE(AreBoundsValid(-infinity, -infinity));
  EXPECT_FALSE(AreBoundsValid(infinity, infinity));
  EXPECT_FALSE(AreBoundsValid(nan, 0));
  EXPECT_FALSE(AreBoundsValid(1, 0));
  EXPECT_FALSE(AreBoundsValid(100, 100 - 1e-10));
}

TEST(LinearProgramTest, IsInEquationForm) {
  LinearProgram linear_program;
  // An empty linear program is in the equations form, but it does not have
  // slack variables added to it.
  EXPECT_FALSE(linear_program.IsInEquationForm());

  const ColIndex v1 = linear_program.CreateNewVariable();
  const ColIndex v2 = linear_program.CreateNewVariable();
  linear_program.AddSlackVariablesWhereNecessary(true);

  // A linear program with no constraints is in the equations form.
  EXPECT_TRUE(linear_program.IsInEquationForm());

  // Adding a constraint with bounds (0.0, 0.0) does not break the equations
  // form.
  linear_program.DeleteSlackVariables();
  const RowIndex c1 = linear_program.CreateNewConstraint();
  linear_program.SetCoefficient(c1, v1, 1.0);
  linear_program.SetCoefficient(c1, v2, -1.0);
  linear_program.SetConstraintBounds(c1, 0.0, 0.0);
  linear_program.AddSlackVariablesWhereNecessary(true);
  EXPECT_TRUE(linear_program.IsInEquationForm());

  // Add a second constraint and exercise all possible combinations of values.
  // Only the (0.0, 0.0) bounds preserve the equations form.
  linear_program.DeleteSlackVariables();
  const RowIndex c2 = linear_program.CreateNewConstraint();
  linear_program.AddSlackVariablesWhereNecessary(true);
  linear_program.SetCoefficient(c2, v1, 2.0);
  linear_program.SetCoefficient(c2, v2, 3.0);
  linear_program.SetConstraintBounds(c2, 0.0, 1.0);
  EXPECT_FALSE(linear_program.IsInEquationForm());
  linear_program.SetConstraintBounds(c2, -kInfinity, 0.0);
  EXPECT_FALSE(linear_program.IsInEquationForm());
  linear_program.SetConstraintBounds(c2, 1.0, kInfinity);
  EXPECT_FALSE(linear_program.IsInEquationForm());
  linear_program.SetConstraintBounds(c2, -kInfinity, kInfinity);
  EXPECT_FALSE(linear_program.IsInEquationForm());
  linear_program.SetConstraintBounds(c2, 0.0, 0.0);
  EXPECT_TRUE(linear_program.IsInEquationForm());
}

TEST(LinearProgramTest, BoundsOfIntegerVariablesAreInteger) {
  // Check empty states.
  LinearProgram linear_program;
  EXPECT_TRUE(
      linear_program.BoundsOfIntegerVariablesAreInteger(kIntegralityTolerance));
  const ColIndex col = linear_program.CreateNewVariable();
  EXPECT_TRUE(
      linear_program.BoundsOfIntegerVariablesAreInteger(kIntegralityTolerance));

  // Check integer variable.
  linear_program.SetVariableBounds(col, -kInfinity, kInfinity);
  linear_program.SetVariableType(col, LinearProgram::VariableType::INTEGER);
  EXPECT_TRUE(
      linear_program.BoundsOfIntegerVariablesAreInteger(kIntegralityTolerance));
  linear_program.SetVariableBounds(col, 0.0, 1.0);
  EXPECT_TRUE(
      linear_program.BoundsOfIntegerVariablesAreInteger(kIntegralityTolerance));
  linear_program.SetVariableBounds(col, 0.0, 1.5);
  EXPECT_FALSE(
      linear_program.BoundsOfIntegerVariablesAreInteger(kIntegralityTolerance));

  // Check implied-integer variable.
  linear_program.SetVariableBounds(col, -kInfinity, kInfinity);
  linear_program.SetVariableType(col,
                                 LinearProgram::VariableType::IMPLIED_INTEGER);
  EXPECT_TRUE(
      linear_program.BoundsOfIntegerVariablesAreInteger(kIntegralityTolerance));
  linear_program.SetVariableBounds(col, 0.0, 1.0);
  EXPECT_TRUE(
      linear_program.BoundsOfIntegerVariablesAreInteger(kIntegralityTolerance));
  linear_program.SetVariableBounds(col, 0.0, 1.5);
  EXPECT_FALSE(
      linear_program.BoundsOfIntegerVariablesAreInteger(kIntegralityTolerance));
}

TEST(BoundsOfIntegerConstraintsAreIntegerTest, NoConstraint) {
  // Check empty states.
  LinearProgram linear_program;
  EXPECT_TRUE(linear_program.BoundsOfIntegerConstraintsAreInteger(
      kIntegralityTolerance));
}

TEST(BoundsOfIntegerConstraintsAreIntegerTest,
     IntegerConstraintWithDefaultBounds) {
  LinearProgram linear_program;
  const ColIndex col = linear_program.CreateNewVariable();
  linear_program.SetVariableType(col, LinearProgram::VariableType::INTEGER);
  const ColIndex col2 = linear_program.CreateNewVariable();
  linear_program.SetVariableType(col2, LinearProgram::VariableType::INTEGER);
  const RowIndex row = linear_program.CreateNewConstraint();
  linear_program.SetCoefficient(row, col, 1.0);
  linear_program.SetCoefficient(row, col2, 2.0);
  linear_program.CleanUp();
  EXPECT_TRUE(linear_program.BoundsOfIntegerConstraintsAreInteger(
      kIntegralityTolerance));
}

TEST(BoundsOfIntegerConstraintsAreIntegerTest,
     IntegerConstraintWithInfiniteBounds) {
  LinearProgram linear_program;
  const ColIndex col = linear_program.CreateNewVariable();
  linear_program.SetVariableType(col, LinearProgram::VariableType::INTEGER);
  const ColIndex col2 = linear_program.CreateNewVariable();
  linear_program.SetVariableType(col2, LinearProgram::VariableType::INTEGER);
  const RowIndex row = linear_program.CreateNewConstraint();
  linear_program.SetCoefficient(row, col, 1.0);
  linear_program.SetCoefficient(row, col2, 2.0);
  linear_program.SetConstraintBounds(row, -kInfinity, 22.7);
  linear_program.CleanUp();
  EXPECT_FALSE(linear_program.BoundsOfIntegerConstraintsAreInteger(
      kIntegralityTolerance));

  linear_program.SetConstraintBounds(row, -kInfinity, 22.0);
  linear_program.CleanUp();
  EXPECT_TRUE(linear_program.BoundsOfIntegerConstraintsAreInteger(
      kIntegralityTolerance));

  linear_program.SetConstraintBounds(row, 0.5, kInfinity);
  linear_program.CleanUp();
  EXPECT_FALSE(linear_program.BoundsOfIntegerConstraintsAreInteger(
      kIntegralityTolerance));

  linear_program.SetConstraintBounds(row, 0.0, kInfinity);
  linear_program.CleanUp();
  EXPECT_TRUE(linear_program.BoundsOfIntegerConstraintsAreInteger(
      kIntegralityTolerance));

  linear_program.SetConstraintBounds(row, -kInfinity, kInfinity);
  linear_program.CleanUp();
  EXPECT_TRUE(linear_program.BoundsOfIntegerConstraintsAreInteger(
      kIntegralityTolerance));
}

TEST(BoundsOfIntegerConstraintsAreIntegerTest,
     IntegerConstraintWithImpliedIntegerVariable) {
  LinearProgram linear_program;
  const ColIndex col = linear_program.CreateNewVariable();
  linear_program.SetVariableType(col,
                                 LinearProgram::VariableType::IMPLIED_INTEGER);
  const ColIndex col2 = linear_program.CreateNewVariable();
  linear_program.SetVariableType(col2, LinearProgram::VariableType::INTEGER);
  const RowIndex row = linear_program.CreateNewConstraint();
  linear_program.SetCoefficient(row, col, 1.0);
  linear_program.SetCoefficient(row, col2, 2.0);
  linear_program.SetConstraintBounds(row, 0.0, 22.7);
  linear_program.CleanUp();
  EXPECT_FALSE(linear_program.BoundsOfIntegerConstraintsAreInteger(
      kIntegralityTolerance));

  linear_program.SetConstraintBounds(row, 0.5, 22.0);
  linear_program.CleanUp();
  EXPECT_FALSE(linear_program.BoundsOfIntegerConstraintsAreInteger(
      kIntegralityTolerance));

  linear_program.SetConstraintBounds(row, 0.0, 22.0);
  linear_program.CleanUp();
  EXPECT_TRUE(linear_program.BoundsOfIntegerConstraintsAreInteger(
      kIntegralityTolerance));
}

TEST(BoundsOfIntegerConstraintsAreIntegerTest,
     NonIntegerConstraintWithContinuousVariable) {
  LinearProgram linear_program;
  const ColIndex col = linear_program.CreateNewVariable();
  linear_program.SetVariableType(col, LinearProgram::VariableType::CONTINUOUS);
  const ColIndex col2 = linear_program.CreateNewVariable();
  linear_program.SetVariableType(col2, LinearProgram::VariableType::INTEGER);
  const RowIndex row = linear_program.CreateNewConstraint();
  linear_program.SetCoefficient(row, col, 1.0);
  linear_program.SetCoefficient(row, col2, 2.0);
  linear_program.SetConstraintBounds(row, 0.0, 22.7);
  linear_program.CleanUp();
  EXPECT_TRUE(linear_program.BoundsOfIntegerConstraintsAreInteger(
      kIntegralityTolerance));

  linear_program.SetConstraintBounds(row, 0.5, 22.0);
  linear_program.CleanUp();
  EXPECT_TRUE(linear_program.BoundsOfIntegerConstraintsAreInteger(
      kIntegralityTolerance));

  linear_program.SetConstraintBounds(row, 0.0, 22.0);
  linear_program.CleanUp();
  EXPECT_TRUE(linear_program.BoundsOfIntegerConstraintsAreInteger(
      kIntegralityTolerance));
}

TEST(BoundsOfIntegerConstraintsAreIntegerTest,
     NonIntegerConstraintWithNonIntegerCoefficient) {
  LinearProgram linear_program;
  const ColIndex col = linear_program.CreateNewVariable();
  linear_program.SetVariableType(col, LinearProgram::VariableType::INTEGER);
  const ColIndex col2 = linear_program.CreateNewVariable();
  linear_program.SetVariableType(col2, LinearProgram::VariableType::INTEGER);
  const RowIndex row = linear_program.CreateNewConstraint();
  linear_program.SetCoefficient(row, col, 1.1);
  linear_program.SetCoefficient(row, col2, 2.0);
  linear_program.SetConstraintBounds(row, 0.0, 22.7);
  linear_program.CleanUp();
  EXPECT_TRUE(linear_program.BoundsOfIntegerConstraintsAreInteger(
      kIntegralityTolerance));

  linear_program.SetConstraintBounds(row, 0.5, 22.0);
  linear_program.CleanUp();
  EXPECT_TRUE(linear_program.BoundsOfIntegerConstraintsAreInteger(
      kIntegralityTolerance));

  linear_program.SetConstraintBounds(row, 0.0, 22.0);
  linear_program.CleanUp();
  EXPECT_TRUE(linear_program.BoundsOfIntegerConstraintsAreInteger(
      kIntegralityTolerance));
}

TEST(BoundsOfIntegerConstraintsAreIntegerTest, MultipleConstraints) {
  LinearProgram linear_program;
  const ColIndex col = linear_program.CreateNewVariable();
  linear_program.SetVariableType(col, LinearProgram::VariableType::INTEGER);
  const ColIndex col2 = linear_program.CreateNewVariable();
  linear_program.SetVariableType(col2, LinearProgram::VariableType::INTEGER);
  const ColIndex col3 = linear_program.CreateNewVariable();
  linear_program.SetVariableType(col3, LinearProgram::VariableType::CONTINUOUS);
  const RowIndex row = linear_program.CreateNewConstraint();
  const RowIndex row2 = linear_program.CreateNewConstraint();
  linear_program.SetCoefficient(row, col3, 1.0);
  linear_program.SetCoefficient(row, col2, 2.1);
  linear_program.SetConstraintBounds(row, 0.0, 22.7);
  linear_program.SetCoefficient(row2, col, 1.0);
  linear_program.SetCoefficient(row2, col2, 2.0);
  linear_program.SetConstraintBounds(row2, 0.5, 23.7);
  linear_program.CleanUp();
  EXPECT_FALSE(linear_program.BoundsOfIntegerConstraintsAreInteger(
      kIntegralityTolerance));
  linear_program.SetConstraintBounds(row, 0.5, 22.0);
  linear_program.CleanUp();
  EXPECT_FALSE(linear_program.BoundsOfIntegerConstraintsAreInteger(
      kIntegralityTolerance));

  linear_program.SetConstraintBounds(row2, 0.0, 22.0);
  linear_program.CleanUp();
  EXPECT_TRUE(linear_program.BoundsOfIntegerConstraintsAreInteger(
      kIntegralityTolerance));
}

TEST(BoundsOfIntegerConstraintsAreIntegerTest,
     CoefficientWithinIntegralityTolerance) {
  LinearProgram linear_program;
  const ColIndex col = linear_program.CreateNewVariable();
  linear_program.SetVariableType(col, LinearProgram::VariableType::INTEGER);
  const ColIndex col2 = linear_program.CreateNewVariable();
  linear_program.SetVariableType(col2, LinearProgram::VariableType::INTEGER);
  const RowIndex row = linear_program.CreateNewConstraint();
  linear_program.SetCoefficient(row, col, 1.0 + kIntegralityTolerance / 2.0);
  linear_program.SetCoefficient(row, col2, 2.0);
  linear_program.SetConstraintBounds(row, 0.0, 22.5);
  linear_program.CleanUp();

  // We require exact integrality of the coefficient for a constraint to be
  // considered as integer.
  EXPECT_TRUE(linear_program.BoundsOfIntegerConstraintsAreInteger(
      kIntegralityTolerance));
}

TEST(BoundsOfIntegerConstraintsAreIntegerTest,
     ConstraintBoundWithinIntegralityTolerance) {
  LinearProgram linear_program;
  const ColIndex col = linear_program.CreateNewVariable();
  linear_program.SetVariableType(col, LinearProgram::VariableType::INTEGER);
  const ColIndex col2 = linear_program.CreateNewVariable();
  linear_program.SetVariableType(col2, LinearProgram::VariableType::INTEGER);
  const RowIndex row = linear_program.CreateNewConstraint();
  linear_program.SetCoefficient(row, col, 1.0);
  linear_program.SetCoefficient(row, col2, 2.0);
  linear_program.SetConstraintBounds(row, 0.0,
                                     22.0 + kIntegralityTolerance / 2.0);
  linear_program.CleanUp();
  EXPECT_TRUE(linear_program.BoundsOfIntegerConstraintsAreInteger(
      kIntegralityTolerance));

  linear_program.SetConstraintBounds(row, 0.0 + kIntegralityTolerance / 2.0,
                                     22.0);
  linear_program.CleanUp();
  EXPECT_TRUE(linear_program.BoundsOfIntegerConstraintsAreInteger(
      kIntegralityTolerance));
}

TEST(ScaleObjectiveTest, NoScaling) {
  LinearProgram lp;
  lp.SetObjectiveCoefficient(lp.CreateNewVariable(), 0.0);
  lp.SetObjectiveCoefficient(lp.CreateNewVariable(), 2.0);
  lp.SetObjectiveCoefficient(lp.CreateNewVariable(), 3.0);
  lp.SetObjectiveCoefficient(lp.CreateNewVariable(), 10.0);
  lp.ScaleObjective(GlopParameters::NO_COST_SCALING);
  EXPECT_EQ(lp.objective_scaling_factor(), 1.0);
}

TEST(ScaleObjectiveTest, ContainOneScaling) {
  LinearProgram lp;
  lp.SetObjectiveCoefficient(lp.CreateNewVariable(), 0.0);
  lp.SetObjectiveCoefficient(lp.CreateNewVariable(), 2.0);
  lp.SetObjectiveCoefficient(lp.CreateNewVariable(), 3.0);
  lp.SetObjectiveCoefficient(lp.CreateNewVariable(), 10.0);
  lp.ScaleObjective(GlopParameters::CONTAIN_ONE_COST_SCALING);
  EXPECT_EQ(lp.objective_scaling_factor(), 2.0);
}

TEST(ScaleObjectiveTest, MeanScaling) {
  LinearProgram lp;
  lp.SetObjectiveCoefficient(lp.CreateNewVariable(), 0.0);
  lp.SetObjectiveCoefficient(lp.CreateNewVariable(), 2.0);
  lp.SetObjectiveCoefficient(lp.CreateNewVariable(), 3.0);
  lp.SetObjectiveCoefficient(lp.CreateNewVariable(), 10.0);
  lp.ScaleObjective(GlopParameters::MEAN_COST_SCALING);
  EXPECT_EQ(lp.objective_scaling_factor(), 5.0);
}

TEST(ScaleObjectiveTest, MedianScaling) {
  LinearProgram lp;
  lp.SetObjectiveCoefficient(lp.CreateNewVariable(), 0.0);
  lp.SetObjectiveCoefficient(lp.CreateNewVariable(), 2.0);
  lp.SetObjectiveCoefficient(lp.CreateNewVariable(), 3.0);
  lp.SetObjectiveCoefficient(lp.CreateNewVariable(), 10.0);
  lp.ScaleObjective(GlopParameters::MEDIAN_COST_SCALING);
  EXPECT_EQ(lp.objective_scaling_factor(), 3.0);
}

TEST(LinearProgramTest, RemoveNearZeroEntries) {
  const std::string kLinearProgram =
      "min: 0 + 0 x1 + 0 x2 + 1 x3 + 1e-15 x4 + 2 x5;"
      "x1 + x5 = 0;"
      "x2 + 1e-20 x3 + 1e-20 x4 + 2 x5 = 0;"
      "1e-20 x2 + x3 + 3 x5 = 0;"
      " 1 <= x1 <= 5;"
      " 2 <= x2 <= 6;"
      " 1 <= x3;"
      " 4 <= x4 <= 8;"
      "-8 <= x5 <= -2;";
  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(kLinearProgram, &linear_program));
  linear_program.CleanUp();
  linear_program.GetTransposeSparseMatrix();

  // Note that the function does not remove empty columns.
  const std::string kOutputProgram =
      "min: 0 + 0 x1 + 0 x2 + 1 x3 + 0 x4 + 2 x5;"
      "x1 + x5 = 0;"
      "x2 + 2 x5 = 0;"
      "x3 + 3 x5 = 0;"
      " 1 <= x1 <= 5;"
      " 2 <= x2 <= 6;"
      " 1 <= x3;"
      " 4 <= x4 <= 8;"
      "-8 <= x5 <= -2;";
  LinearProgram expected_program;
  ASSERT_TRUE(ParseLp(kOutputProgram, &expected_program));

  linear_program.RemoveNearZeroEntries(1e-10);
  EXPECT_EQ(linear_program.Dump(), expected_program.Dump());
  EXPECT_EQ(linear_program.GetSparseMatrix().num_entries(),
            linear_program.GetTransposeSparseMatrix().num_entries());
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
