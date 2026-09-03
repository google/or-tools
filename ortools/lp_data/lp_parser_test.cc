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

#include "ortools/lp_data/lp_parser.h"

#include <random>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"

namespace operations_research {
namespace glop {
namespace {

using ::absl::StatusOr;
using ::testing::ElementsAreArray;

struct ExplicitConstraint {
  Fractional lb;
  DenseRow coeffs;
  Fractional ub;
};

struct ExplicitObjective {
  bool maximization;
  DenseRow coeffs;
  Fractional offset;
};

struct ExplicitVariables {
  DenseRow lb;
  DenseRow ub;
  StrictITIVector<ColIndex, bool> is_integer;
};

struct ExplicitLinearProgram {
  ExplicitVariables variables;
  ExplicitObjective objective;
  StrictITIVector<RowIndex, ExplicitConstraint> constraints;
};

static const bool kMax = true;
static const ExplicitObjective kEmptyObjective = {!kMax, {0}, 0};
static const std::vector<ExplicitConstraint> kEmptyConstraints = {};

void ParserSuccess(absl::string_view model, ExplicitLinearProgram explicit_lp) {
  LinearProgram lp;
  ASSERT_TRUE(ParseLp(model, &lp));
  const ColIndex num_cols(explicit_lp.objective.coeffs.size());
  const RowIndex num_rows(explicit_lp.constraints.size());
  DCHECK_EQ(num_cols, explicit_lp.variables.lb.size());
  DCHECK_EQ(num_cols, explicit_lp.variables.ub.size());
  for (const ExplicitConstraint& constraint : explicit_lp.constraints) {
    DCHECK_EQ(num_cols, constraint.coeffs.size());
  }
  EXPECT_EQ(explicit_lp.objective.maximization, lp.IsMaximizationProblem());
  EXPECT_THAT(lp.objective_coefficients(),
              testing::ContainerEq(explicit_lp.objective.coeffs));
  EXPECT_EQ(explicit_lp.objective.offset, lp.objective_offset());
  EXPECT_THAT(lp.variable_lower_bounds(),
              testing::ContainerEq(explicit_lp.variables.lb));
  EXPECT_THAT(lp.variable_upper_bounds(),
              testing::ContainerEq(explicit_lp.variables.ub));
  if (explicit_lp.variables.is_integer.size() > 0) {
    DCHECK_EQ(num_cols, explicit_lp.variables.is_integer.size());
    for (ColIndex col(0); col < num_cols; ++col) {
      EXPECT_EQ(explicit_lp.variables.is_integer[col],
                lp.IsVariableInteger(col));
    }
  }
  for (RowIndex row(0); row < num_rows; ++row) {
    EXPECT_EQ(explicit_lp.constraints[row].lb,
              lp.constraint_lower_bounds()[row]);
    EXPECT_EQ(explicit_lp.constraints[row].ub,
              lp.constraint_upper_bounds()[row]);
    for (ColIndex col(0); col < num_cols; ++col) {
      EXPECT_EQ(explicit_lp.constraints[row].coeffs[col],
                lp.GetSparseMatrix().column(col).LookUpCoefficient(row));
    }
  }
}

void ParserFailure(absl::string_view model) {
  LinearProgram lp;
  EXPECT_FALSE(ParseLp(model, &lp));
}

TEST(LpParserTest, ParsesEmptyProblem) {
  ParserSuccess("", {{{}, {}}, {!kMax, {}, 0}, {}});
  ParserSuccess("min:", {{{}, {}}, {!kMax, {}, 0}, {}});
  ParserSuccess("---inF<=<=0;min:",
                {{{}, {}}, {!kMax, {}, 0}, {{-kInfinity, {}, 0}}});
  ParserSuccess("=0", {{{}, {}}, {!kMax, {}, 0}, {{0, {}, 0}}});
  ParserSuccess("0=", {{{}, {}}, {!kMax, {}, 0}, {{0, {}, 0}}});
  ParserSuccess("int:", {{{}, {}}, {!kMax, {}, 0}, {}});
}

TEST(LpParserTest, ParsesSimpleObjective) {
  ParserSuccess("min:2", {{}, {!kMax, {}, 2}, {}});
  ParserSuccess("min: +2 + x0",
                {{{-kInfinity}, {kInfinity}}, {!kMax, {1}, 2}, {}});
  ParserSuccess("mIn:+2+x0",
                {{{-kInfinity}, {kInfinity}}, {!kMax, {1}, 2}, {}});
  ParserSuccess("miN: 2 + 1 x0",
                {{{-kInfinity}, {kInfinity}}, {!kMax, {1}, 2}, {}});
  ParserSuccess("MAX: 2 +++--+ 1*x0",
                {{{-kInfinity}, {kInfinity}}, {kMax, {1}, 2}, {}});
  ParserSuccess("max:2+1x0x1",
                {{{-kInfinity}, {kInfinity}}, {kMax, {1}, 2}, {}});
  ParserSuccess("max:2-x0", {{{-kInfinity}, {kInfinity}}, {kMax, {-1}, 2}, {}});
  ParserSuccess("max:2-1x0",
                {{{-kInfinity}, {kInfinity}}, {kMax, {-1}, 2}, {}});
  ParserSuccess("max:2-1*x0",
                {{{-kInfinity}, {kInfinity}}, {kMax, {-1}, 2}, {}});
  ParserSuccess("max:2 -1 x0",
                {{{-kInfinity}, {kInfinity}}, {kMax, {-1}, 2}, {}});
  ParserSuccess("min: x0 x1",
                {{{-kInfinity, -kInfinity}, {kInfinity, kInfinity}},
                 {!kMax, {1, 1}, 0},
                 {}});
  ParserSuccess("min: +x0 +x1",
                {{{-kInfinity, -kInfinity}, {kInfinity, kInfinity}},
                 {!kMax, {1, 1}, 0},
                 {}});
  ParserSuccess("min: +1x0 +1x1",
                {{{-kInfinity, -kInfinity}, {kInfinity, kInfinity}},
                 {!kMax, {1, 1}, 0},
                 {}});
  ParserSuccess("min: +1*x0 +1*x1",
                {{{-kInfinity, -kInfinity}, {kInfinity, kInfinity}},
                 {!kMax, {1, 1}, 0},
                 {}});
  ParserSuccess("min: + 1 * x0 + 1 * x1",
                {{{-kInfinity, -kInfinity}, {kInfinity, kInfinity}},
                 {!kMax, {1, 1}, 0},
                 {}});
  ParserSuccess("min:2 1 x0 1 x1",
                {{{-kInfinity, -kInfinity}, {kInfinity, kInfinity}},
                 {!kMax, {1, 1}, 2},
                 {}});
}

TEST(LpParserTest, RejectsSimpleObjective) {
  ParserFailure("min 2 + x");
  ParserFailure("min: *x");
  ParserFailure("min: +*x");
  ParserFailure("min: x + 2");
  ParserFailure("min: x*2");
  ParserFailure("min: x*x");
  ParserFailure("min: 2 1 -x0");
  ParserFailure("min: 2 1 1");
  ParserFailure("min: 2 + x + x");
}

TEST(LpParserTest, ParsesSimpleBounds) {
  ParserSuccess("1x < 1", {{{-kInfinity}, {1}}, {!kMax, {0}, 0}, {}});
  ParserSuccess("1*x <= 1", {{{-kInfinity}, {1}}, {!kMax, {0}, 0}, {}});
  ParserSuccess("1 x > 1", {{{1}, {kInfinity}}, {!kMax, {0}, 0}, {}});
  ParserSuccess("x >= 1", {{{1}, {kInfinity}}, {!kMax, {0}, 0}, {}});
  ParserSuccess("x = 1", {{{1}, {1}}, {!kMax, {0}, 0}, {}});
  ParserSuccess("1 > 1x", {{{-kInfinity}, {1}}, {!kMax, {0}, 0}, {}});
  ParserSuccess("1 >= 1*x", {{{-kInfinity}, {1}}, {!kMax, {0}, 0}, {}});
  ParserSuccess("1 < 1 x", {{{1}, {kInfinity}}, {!kMax, {0}, 0}, {}});
  ParserSuccess("1 <= x", {{{1}, {kInfinity}}, {!kMax, {0}, 0}, {}});
  ParserSuccess("1 = x", {{{1}, {1}}, {!kMax, {0}, 0}, {}});
}

TEST(LpParserTest, ParsesMultipleBounds) {
  ParserSuccess("x < 1; x > 0", {{{0}, {1}}, {!kMax, {0}, 0}, {}});
  ParserSuccess("x < 5; x < 3; x < 1;",
                {{{-kInfinity}, {1}}, {!kMax, {0}, 0}, {}});
  ParserSuccess("x > 5; x > 3; x > 1;",
                {{{5}, {kInfinity}}, {!kMax, {0}, 0}, {}});
  ParserSuccess("0 < x < 5; 2 < x < 7;", {{{2}, {5}}, {!kMax, {0}, 0}, {}});
  ParserSuccess("0 < x < 5; 2 < x < 3; x > 3",
                {{{3}, {3}}, {!kMax, {0}, 0}, {}});
}

TEST(LpParserTest, RejectsBounds) {
  ParserFailure("x < 0; x > 1");
  ParserFailure("0 < x < 1; 2 < x < 3");
  ParserFailure("x < -inf;");
  ParserFailure("x > inf;");
}

TEST(LpParserTest, ParsesSimpleConstraints1) {
  ParserSuccess(
      "2x < 1",
      {{{-kInfinity}, {kInfinity}}, {!kMax, {0}, 0}, {{-kInfinity, {2}, 1}}});
  ParserSuccess(
      "2*x <= 1",
      {{{-kInfinity}, {kInfinity}}, {!kMax, {0}, 0}, {{-kInfinity, {2}, 1}}});
  ParserSuccess(
      "+3x > 1",
      {{{-kInfinity}, {kInfinity}}, {!kMax, {0}, 0}, {{1, {3}, kInfinity}}});
  ParserSuccess(
      "-4x >= 1",
      {{{-kInfinity}, {kInfinity}}, {!kMax, {0}, 0}, {{1, {-4}, kInfinity}}});
  ParserSuccess("-5*x = 1",
                {{{-kInfinity}, {kInfinity}}, {!kMax, {0}, 0}, {{1, {-5}, 1}}});
}

TEST(LpParserTest, ParsesSimpleConstraints2) {
  ParserSuccess("x + y < 1",
                {{{-kInfinity, -kInfinity}, {kInfinity, kInfinity}},
                 {!kMax, {0, 0}, 0},
                 {{-kInfinity, {1, 1}, 1}}});
  ParserSuccess("1x + 2*y <= INF",
                {{{-kInfinity, -kInfinity}, {kInfinity, kInfinity}},
                 {!kMax, {0, 0}, 0},
                 {{-kInfinity, {1, 2}, kInfinity}}});
  ParserSuccess("3*x + 4y > -inf",
                {{{-kInfinity, -kInfinity}, {kInfinity, kInfinity}},
                 {!kMax, {0, 0}, 0},
                 {{-kInfinity, {3, 4}, kInfinity}}});
  ParserSuccess("-5x -6y >= -1",
                {{{-kInfinity, -kInfinity}, {kInfinity, kInfinity}},
                 {!kMax, {0, 0}, 0},
                 {{-1, {-5, -6}, kInfinity}}});
  ParserSuccess("-7*x + -8*y = 1",
                {{{-kInfinity, -kInfinity}, {kInfinity, kInfinity}},
                 {!kMax, {0, 0}, 0},
                 {{1, {-7, -8}, 1}}});
  ParserSuccess("0 <= x - 2 y <= 1",
                {{{-kInfinity, -kInfinity}, {kInfinity, kInfinity}},
                 {!kMax, {0, 0}, 0},
                 {{0, {1, -2}, 1}}});
  ParserSuccess("1 >= x - 2 y >= 0",
                {{{-kInfinity, -kInfinity}, {kInfinity, kInfinity}},
                 {!kMax, {0, 0}, 0},
                 {{0, {1, -2}, 1}}});
  ParserSuccess("0 >= x - 2 y <= 1",
                {{{-kInfinity, -kInfinity}, {kInfinity, kInfinity}},
                 {!kMax, {0, 0}, 0},
                 {{-kInfinity, {1, -2}, 0}}});
  ParserSuccess("0 <= x - 2 y >= 1",
                {{{-kInfinity, -kInfinity}, {kInfinity, kInfinity}},
                 {!kMax, {0, 0}, 0},
                 {{1, {1, -2}, kInfinity}}});
}

TEST(LpParserTest, RejectsSimpleConstraints) {
  ParserFailure("x != 1");
  ParserFailure("0 <= x x <= 1");
  ParserFailure("0 <= x = 1");
  ParserFailure("0 = x <= 1");
  ParserFailure("0 <= x + 1 <= 1");
  ParserFailure("0 <= 1 + x <= 1");
  ParserFailure("inf <= x <= inf");
  ParserFailure("x = inf");
  ParserFailure("x + 1 = 1");
  ParserFailure("1 <= x <= 0");
  ParserFailure("0 + 1 <= x <= 2");
  ParserFailure("x + y <= 2x");
  ParserFailure("x + y = 2 + 2");
  ParserFailure("<= x <= 2");
  ParserFailure("2 <= x <=");
}

TEST(LpParserTest, ParsesScientificNumbers) {
  ParserSuccess("min: 1e1", {{}, {!kMax, {}, 10}, {}});
  ParserSuccess("min: 1 e1",
                {{{-kInfinity}, {kInfinity}}, {!kMax, {1}, 0}, {}});
  ParserSuccess("min: 1 + e1",
                {{{-kInfinity}, {kInfinity}}, {!kMax, {1}, 1}, {}});
  ParserSuccess("min: 1 1*e1",
                {{{-kInfinity}, {kInfinity}}, {!kMax, {1}, 1}, {}});
  ParserSuccess("min: 1 + 1*e1",
                {{{-kInfinity}, {kInfinity}}, {!kMax, {1}, 1}, {}});
  ParserSuccess("min: +2e2 + -1e-2x0",
                {{{-kInfinity}, {kInfinity}}, {!kMax, {-0.01}, 200}, {}});
  ParserSuccess(
      "2e-1x < 1",
      {{{-kInfinity}, {kInfinity}}, {!kMax, {0}, 0}, {{-kInfinity, {.2}, 1}}});
  ParserSuccess("2e1*x <= 1e+1000000000000", {{{-kInfinity}, {kInfinity}},
                                              {!kMax, {0}, 0},
                                              {{-kInfinity, {20}, kInfinity}}});
  ParserSuccess(
      "+3E+2x > 1E-1000000000000",
      {{{-kInfinity}, {kInfinity}}, {!kMax, {0}, 0}, {{0, {300}, kInfinity}}});
  ParserSuccess("-4E-3x >= 1", {{{-kInfinity}, {kInfinity}},
                                {!kMax, {0}, 0},
                                {{1, {-0.004}, kInfinity}}});
  ParserSuccess("-5*x = 1",
                {{{-kInfinity}, {kInfinity}}, {!kMax, {0}, 0}, {{1, {-5}, 1}}});
}

TEST(LpParserTest, RejectsScientificNumbers) {
  ParserFailure("min: 1 + 1e1");
  ParserFailure("1E100000000 * x = 1");
  ParserFailure("min: 1e100000000 + x");
  ParserFailure("min: 1 + 1e+1000000000 * x");
}

TEST(LpParserTest, ParsesIntegerVariableList) {
  ParserSuccess(
      "x1 = 0; x2 = 0; x3 = 0; int x1, x3;",
      {{{0, 0, 0}, {0, 0, 0}, {true, false, true}}, {!kMax, {0, 0, 0}, 0}, {}});
  ParserSuccess("0.2 <= x <= 0.8; int x;",
                {{{0.2}, {0.8}, {true}}, {!kMax, {0}, 0}, {}});
  // The signs are discarded.
  ParserSuccess(
      "x1 = 0; x2 = 0; x3 = 0; int x1 + x3;",
      {{{0, 0, 0}, {0, 0, 0}, {true, false, true}}, {!kMax, {0, 0, 0}, 0}, {}});
  ParserSuccess(
      "x1 = 0; x2 = 0; x3 = 0; int x1; int: x3;",
      {{{0, 0, 0}, {0, 0, 0}, {true, false, true}}, {!kMax, {0, 0, 0}, 0}, {}});
  // The coefficient, if it's equal to 1.0, is discarded.
  ParserSuccess(
      "x1 = 0; x2 = 0; x3 = 0; int 1x1, --1*x3;",
      {{{0, 0, 0}, {0, 0, 0}, {true, false, true}}, {!kMax, {0, 0, 0}, 0}, {}});
  ParserSuccess(
      "int: x1 x2; x1 = 0; x2 = 0; x3 = 0;",
      {{{0, 0, 0}, {0, 0, 0}, {true, true, false}}, {!kMax, {0, 0, 0}, 0}, {}});
}

TEST(LpParserTest, RejectsIntegerVariableList) {
  ParserFailure("int: x1 + 2x2");
  ParserFailure("int: x1 +, x2");
  ParserFailure("int: x1,, x2");
  ParserFailure("int: x1 2*x2");
  ParserFailure("int: 1");
}

TEST(LpParserTest, ParsesBinaryVariableList) {
  ParserSuccess("x1 = 2; bin x2;",
                {{{2, 0}, {2, 1}, {false, true}}, {!kMax, {0, 0}, 0}, {}});
  // The signs are discarded.
  ParserSuccess("bin x1 + x2;",
                {{{0, 0}, {1, 1}, {true, true}}, {!kMax, {0, 0}, 0}, {}});
  ParserSuccess(
      "bin x1; min: x1 + x2 + x3; x2 = 0; bin x1; bin: x3;",
      {{{0, 0, 0}, {1, 0, 1}, {true, false, true}}, {!kMax, {1, 1, 1}, 0}, {}});
  // The coefficient, if it's equal to 1.0, is discarded.
  ParserSuccess("min: x1 + x2 + x3; x2 = 0.5; bin 1x1; bin: --1*x3;",
                {{{0, 0.5, 0}, {1, 0.5, 1}, {true, false, true}},
                 {!kMax, {1, 1, 1}, 0},
                 {}});
  ParserSuccess("bin: x1 x2; x1 >= 0.5; x2 <= 0.5;",
                {{{0.5, 0}, {1, 0.5}, {true, true}}, {!kMax, {0, 0}, 0}, {}});
  ParserSuccess("bin: x1 x2; x1 >= 1; x2 <= 0;",
                {{{1, 0}, {1, 0}, {true, true}}, {!kMax, {0, 0}, 0}, {}});
}

TEST(LpParserTest, RejectsBinaryVariableList) {
  ParserFailure("bin: x1 + 2x2");
  ParserFailure("bin: x1 +, x2");
  ParserFailure("bin: x1,, x2");
  ParserFailure("bin: x1 2*x2");
  ParserFailure("bin: 1");
  ParserFailure("bin: x; x > 2");
  ParserFailure("bin: x; x < -1");
}

//--------------------
// Random Tests
//--------------------
void RandomizeLinearProgram(int seed, int num_cols, int num_rows,
                            double density, LinearProgram* lp) {
  lp->Clear();
  std::mt19937 random(seed);

  if (absl::Bernoulli(random, 1.0 / 2)) {
    lp->SetObjectiveOffset(absl::Uniform<double>(random, -10, 10));
  }
  if (absl::Bernoulli(random, 1.0 / 2)) {
    lp->SetMaximizationProblem(true);
  } else {
    lp->SetMaximizationProblem(false);
  }

  int num_col = absl::Uniform(random, 0, num_cols);
  for (int i = 0; i < num_col; ++i) {
    ColIndex col = lp->CreateNewVariable();
    if (absl::Bernoulli(random, 1.0 / 2)) {
      lp->SetVariableName(col, absl::StrCat("named_var_", i));
    }
    // We put all variables into the objective to ensure the same order of
    // variables after parsing the Dump() string, so that we can simply compare
    // Dump() strings before and after parsing to ensure correctness.
    lp->SetObjectiveCoefficient(col, absl::Uniform<double>(random, -10, 10));
    // Set 10% of variables to integer variables.
    if (absl::Bernoulli(random, 0.1)) {
      lp->SetVariableType(col, LinearProgram::VariableType::INTEGER);
    }
    const float p = absl::Uniform<float>(random, 0.0, 1.0);
    if (p < 0.2) {
      lp->SetVariableBounds(col, -kInfinity,
                            absl::Uniform<double>(random, -10, 10));
    } else if (p < 0.4) {
      lp->SetVariableBounds(col, absl::Uniform<double>(random, -10, 10),
                            kInfinity);
    } else if (p < 0.6) {
      const Fractional lb = absl::Uniform<double>(random, -10, 10);
      lp->SetVariableBounds(col, lb, lb + absl::Uniform<double>(random, 0, 10));
    } else if (p < 0.8) {
      lp->SetVariableBounds(col, -kInfinity, kInfinity);
    }
  }

  int num_row = absl::Uniform(random, 0, num_rows);
  for (int i = 0; i < num_row; ++i) {
    RowIndex row = lp->CreateNewConstraint();
    if (absl::Bernoulli(random, 1.0 / 2)) {
      lp->SetConstraintName(row, absl::StrCat("named_constraint_", i));
    }
    const float p = absl::Uniform(random, 0.0, 1.0);
    if (p < 0.2) {
      lp->SetConstraintBounds(row, -kInfinity,
                              absl::Uniform<double>(random, -10, 10));
    } else if (p < 0.4) {
      lp->SetConstraintBounds(row, absl::Uniform<double>(random, -10, 10),
                              kInfinity);
    } else if (p < 0.6) {
      const Fractional lb = absl::Uniform<double>(random, -10, 10);
      lp->SetConstraintBounds(row, lb,
                              lb + absl::Uniform<double>(random, 0, 10));
    } else if (p < 0.8) {
      lp->SetConstraintBounds(row, -kInfinity, kInfinity);
    }
    for (ColIndex col(0); col < ColIndex(num_col); ++col) {
      if (absl::Bernoulli(random, density)) {
        lp->SetCoefficient(row, col, absl::Uniform<double>(random, -10, 10));
      }
    }
  }
}

static const int kMaxNumCol = 100;
static const int kMaxNumRow = 100;

TEST(LpParserTest, RandomTestSparse) {
  const double kDensity = 0.1;
  LinearProgram lp;
  for (int seed = 0; seed < 10; ++seed) {
    RandomizeLinearProgram(seed, kMaxNumCol, kMaxNumRow, kDensity, &lp);
    std::string model = lp.Dump();
    ASSERT_TRUE(ParseLp(model, &lp));
    EXPECT_EQ(model, lp.Dump());
  }
}

TEST(LpParserTest, RandomTestDense) {
  const double kDensity = 1;
  LinearProgram lp;
  for (int seed = 0; seed < 10; ++seed) {
    RandomizeLinearProgram(seed, kMaxNumCol, kMaxNumRow, kDensity, &lp);
    std::string model = lp.Dump();
    ASSERT_TRUE(ParseLp(model, &lp));
    EXPECT_EQ(model, lp.Dump());
  }
}

TEST(LpParserTest, RandomTestEmpty) {
  const double kDensity = 0;
  LinearProgram lp;
  for (int seed = 0; seed < 10; ++seed) {
    RandomizeLinearProgram(seed, kMaxNumCol, kMaxNumRow, kDensity, &lp);
    std::string model = lp.Dump();
    ASSERT_TRUE(ParseLp(model, &lp));
    EXPECT_EQ(model, lp.Dump());
  }
}

TEST(LpParserTest, SupportsRawStrings) {
  LinearProgram lp_not_raw;
  ASSERT_TRUE(
      ParseLp("min: x + y;"
              "bin: b1, b2, b3;"
              "constraint_num1: 5 b1 + 3b2 + x <= 7;"
              "4 y + b2 - 3 b3 <= 2;"
              "constraint_num2: -4 b1 + b2 - 3 z <= -2;",
              &lp_not_raw));

  LinearProgram lp_raw;
  ASSERT_TRUE(ParseLp(R"(
      min: x + y;
      bin: b1, b2, b3;
      constraint_num1: 5 b1 + 3b2 + x <= 7;
      4 y + b2 - 3 b3 <= 2;
      constraint_num2: -4 b1 + b2 - 3 z <= -2;)",
                      &lp_raw));
  EXPECT_EQ(lp_not_raw.Dump(), lp_raw.Dump());
}

TEST(LpParserTest, CanFindVariablesAndConstraintByName) {
  LinearProgram lp;
  ASSERT_TRUE(ParseLp(R"(
      min: x + y;
      bin: b1, b2, b3;
      1 <= x <= 42;
      constraint_num1: 5 b1 + 3b2 + x <= 7;
      4 y + b2 - 3 b3 <= 2;
      constraint_num2: -4 b1 + b2 - 3 z <= -2;)",
                      &lp));

  const RowIndex row = lp.FindOrCreateConstraint("constraint_num1");
  EXPECT_EQ(7, lp.constraint_upper_bounds()[row]) << lp.Dump();
  const ColIndex col = lp.FindOrCreateVariable("x");
  EXPECT_EQ(42, lp.variable_upper_bounds()[col]) << lp.Dump();
}

void ConstraintParserSuccess(
    absl::string_view constraint, absl::string_view expected_name,
    Fractional expected_lower_bound, Fractional expected_upper_bound,
    const std::vector<const char*>& expected_variable_names,
    const std::vector<Fractional>& expected_coefficients) {
  SCOPED_TRACE(constraint);
  const StatusOr<ParsedConstraint> parsed_constraint_or_status =
      ParseConstraint(constraint);
  ASSERT_OK(parsed_constraint_or_status);
  const ParsedConstraint& parsed_constraint =
      parsed_constraint_or_status.value();
  EXPECT_EQ(parsed_constraint.name, expected_name);
  EXPECT_EQ(parsed_constraint.lower_bound, expected_lower_bound);
  EXPECT_EQ(parsed_constraint.upper_bound, expected_upper_bound);
  EXPECT_THAT(parsed_constraint.variable_names,
              ElementsAreArray(expected_variable_names));
  EXPECT_THAT(parsed_constraint.coefficients,
              ElementsAreArray(expected_coefficients));
}

TEST(ParseConstraintTest, ParseSimpleConstraint) {
  ConstraintParserSuccess("x_bounds: 1 <= x <= 42", "x_bounds", 1.0, 42.0,
                          {"x"}, {1.0});
}

TEST(ParseConstraintTest, ParseConstraintWithNoName) {
  ConstraintParserSuccess("x + 2 y + 3*z + 2.5w <= 42", "", -kInfinity, 42.0,
                          {"x", "y", "z", "w"}, {1.0, 2.0, 3.0, 2.5});
}

TEST(ParseConstraintTest, ParseConstraintWithNoUpperBound) {
  ConstraintParserSuccess("x + y >= -1.0", "", -1.0, kInfinity, {"x", "y"},
                          {1.0, 1.0});
}

TEST(ParseConstraintTest, ParseConstraintEquality) {
  ConstraintParserSuccess("x y = 1.0", "", 1.0, 1.0, {"x", "y"}, {1.0, 1.0});
}

TEST(ParseConstraintTest, ParseFailures) {
  constexpr const char* kInvalidConstraints[] = {
      "foo? bar!", "x != 1.0", "3.0 == a b c d <= 1.0", "x * y * z == 3.0",
      "x + x - 2x == 0"};
  ParsedConstraint constraint;
  for (const char* const invalid_constraint : kInvalidConstraints) {
    SCOPED_TRACE(invalid_constraint);
    EXPECT_FALSE(ParseConstraint(invalid_constraint).ok());
  }
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
