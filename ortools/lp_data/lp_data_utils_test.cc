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

#include "ortools/lp_data/lp_data_utils.h"

#include <memory>
#include <string>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_vector.h"
#include "ortools/glop/revised_simplex.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_parser.h"
#include "ortools/lp_data/lp_test_utils.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/lp_types_testing.h"
#include "ortools/lp_data/scattered_vector.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace glop {
namespace {

using ::testing::_;
using ::testing::ContainerEq;

class LpScalerHelperTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const std::string kLinearProgram =
        "min: -5x1 - 6x2 - 9x3 - 8x4;"
        "x1 >= 0;"
        "x2 >= 0;"
        "x3 >= 0;"
        "x4 >= 0;"
        "r1: x1 + 2x2 +3x3 +  x4 <= 50;"
        "r2: x1 +  x2 +2x3 + 3x4 <= 30;";
    LinearProgram lp;
    ASSERT_TRUE(ParseLp(kLinearProgram, &lp));
    LinearProgram scaled_lp;
    scaled_lp.PopulateFromLinearProgram(lp);

    num_cols_ = lp.num_variables();
    num_rows_ = lp.num_constraints();

    // Direct solve.
    std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
    lp.AddSlackVariablesWhereNecessary(false);
    EXPECT_THAT(simplex_.Solve(lp, *time_limit),
                SolveStatusWith<SolveStatus::Optimal>(_));

    // In order to test that we take them properly into account, we don't
    // want factors of ones.
    scaler_.Scale(&scaled_lp);
    EXPECT_NE(scaler_.ObjectiveScalingFactor(), 1.0);
    EXPECT_NE(scaler_.BoundsScalingFactor(), 1.0);

    // Solve scaled problem.
    scaled_lp.AddSlackVariablesWhereNecessary(false);
    EXPECT_THAT(scaled_simplex_.Solve(scaled_lp, *time_limit),
                SolveStatusWith<SolveStatus::Optimal>(_));
  }

  RowIndex num_rows_;
  ColIndex num_cols_;
  LpScalingHelper scaler_;
  RevisedSimplex simplex_;
  RevisedSimplex scaled_simplex_;
};

TEST_F(LpScalerHelperTest, ObjectiveAlreadyScaled) {
  EXPECT_NEAR(simplex_.GetObjectiveValue(), scaled_simplex_.GetObjectiveValue(),
              1e-10);
}

TEST_F(LpScalerHelperTest, ScaleUnscaleVariableValue) {
  for (ColIndex col(0); col < num_cols_; ++col) {
    EXPECT_NEAR(simplex_.GetVariableValue(col),
                scaler_.UnscaleVariableValue(
                    col, scaled_simplex_.GetVariableValue(col)),
                1e-10);
    EXPECT_NEAR(scaler_.ScaleVariableValue(col, simplex_.GetVariableValue(col)),
                scaled_simplex_.GetVariableValue(col), 1e-10);
  }
}

TEST_F(LpScalerHelperTest, ScaleUnscaleReducedCost) {
  for (ColIndex col(0); col < num_cols_; ++col) {
    EXPECT_NEAR(
        simplex_.GetReducedCost(col),
        scaler_.UnscaleReducedCost(col, scaled_simplex_.GetReducedCost(col)),
        1e-10);
    EXPECT_NEAR(scaler_.ScaleReducedCost(col, simplex_.GetReducedCost(col)),
                scaled_simplex_.GetReducedCost(col), 1e-10);
  }
}

TEST_F(LpScalerHelperTest, ScaleUnscaleDualValue) {
  for (RowIndex row(0); row < num_rows_; ++row) {
    EXPECT_NEAR(
        simplex_.GetDualValue(row),
        scaler_.UnscaleDualValue(row, scaled_simplex_.GetDualValue(row)),
        1e-10);
    EXPECT_NEAR(scaler_.ScaleDualValue(row, simplex_.GetDualValue(row)),
                scaled_simplex_.GetDualValue(row), 1e-10);
  }
}

TEST_F(LpScalerHelperTest, ScaleUnscaleConstraintActivity) {
  for (RowIndex row(0); row < num_rows_; ++row) {
    EXPECT_NEAR(simplex_.GetConstraintActivity(row),
                scaler_.UnscaleConstraintActivity(
                    row, scaled_simplex_.GetConstraintActivity(row)),
                1e-10);
    EXPECT_NEAR(scaler_.ScaleConstraintActivity(
                    row, simplex_.GetConstraintActivity(row)),
                scaled_simplex_.GetConstraintActivity(row), 1e-10);
  }
}

TEST_F(LpScalerHelperTest, UnscaleUnitRowLeftSolve) {
  // The order in the basis has no reason to be the same, but the BASIS columns
  // should be.
  util_intops::StrongVector<ColIndex, RowIndex> reverse_map(num_cols_.value(),
                                                            RowIndex(-1));
  util_intops::StrongVector<ColIndex, RowIndex> scaled_reverse_map(
      num_cols_.value(), RowIndex(-1));
  for (RowIndex row(0); row < num_rows_; ++row) {
    reverse_map[simplex_.GetBasis(row)] = row;
    scaled_reverse_map[scaled_simplex_.GetBasis(row)] = row;
  }
  for (ColIndex col; col < num_cols_; ++col) {
    if (reverse_map[col] == RowIndex(-1)) continue;
    if (scaled_reverse_map[col] == RowIndex(-1)) continue;

    ScatteredRow inverse;
    simplex_.GetBasisFactorization().LeftSolveForUnitRow(
        RowToColIndex(reverse_map[col]), &inverse);

    ScatteredRow scaled_inverse;
    scaled_simplex_.GetBasisFactorization().LeftSolveForUnitRow(
        RowToColIndex(scaled_reverse_map[col]), &scaled_inverse);
    scaler_.UnscaleUnitRowLeftSolve(col, &scaled_inverse);

    for (ColIndex i(0); i < inverse.values.size(); ++i) {
      EXPECT_NEAR(inverse.values[i], scaled_inverse.values[i], 1e-10);
    }
  }
}

TEST_F(LpScalerHelperTest, UnscaleColumnRightInverse) {
  // There is no reason that the two basis are in the same order.
  util_intops::StrongVector<RowIndex, RowIndex> perm(2);
  if (simplex_.GetBasis(RowIndex(0)) != scaled_simplex_.GetBasis(RowIndex(0))) {
    perm[RowIndex(0)] = RowIndex(1);
    perm[RowIndex(1)] = RowIndex(0);
  } else {
    perm[RowIndex(0)] = RowIndex(0);
    perm[RowIndex(1)] = RowIndex(1);
  }

  for (ColIndex col; col < num_cols_; ++col) {
    ScatteredColumn inverse;
    simplex_.GetBasisFactorization().RightSolveForProblemColumn(col, &inverse);

    ScatteredColumn scaled_inverse;
    scaled_simplex_.GetBasisFactorization().RightSolveForProblemColumn(
        col, &scaled_inverse);
    scaler_.UnscaleColumnRightSolve(scaled_simplex_.GetBasisVector(), col,
                                    &scaled_inverse);

    for (RowIndex i(0); i < inverse.values.size(); ++i) {
      EXPECT_NEAR(inverse.values[i], scaled_inverse.values[perm[i]], 1e-10);
    }
  }
}

TEST(LpScalingHelperTest, UnscaleUnitRowLeftSolve) {
  const std::string kLinearProgram = R"(
    max:   x1 + x2 + x3;
    -8 <= -x1       -x3 <= -1;
    -7 <= -x1  -x2      <= -1;
           x1 +2x2      <= 12;
    x1 >= 0;
    x2 >= 0;
    x3 >= 0;
  )";
  LinearProgram lp;
  ASSERT_TRUE(ParseLp(kLinearProgram, &lp));
  lp.AddSlackVariablesWhereNecessary(false);

  LpScalingHelper scaler;
  scaler.Scale(&lp);

  RevisedSimplex simplex;
  std::unique_ptr<TimeLimit> limit = TimeLimit::Infinite();
  ASSERT_THAT(simplex.Solve(lp, *limit),
              Not(SolveStatusWith<SolveStatus::Abnormal>(_)));

  // The basis matrix is:
  // | -1       |
  // |    1  -1 |
  // |        2 |
  const RowToColMapping basis = {ColIndex(2), ColIndex(4), ColIndex(1)};
  EXPECT_EQ(simplex.GetBasisVector(), basis);

  ScatteredRow row0, row1, row2;
  simplex.GetBasisFactorization().LeftSolveForUnitRow(ColIndex(0), &row0);
  simplex.GetBasisFactorization().LeftSolveForUnitRow(ColIndex(1), &row1);
  simplex.GetBasisFactorization().LeftSolveForUnitRow(ColIndex(2), &row2);

  scaler.UnscaleUnitRowLeftSolve(basis[RowIndex(0)], &row0);
  scaler.UnscaleUnitRowLeftSolve(basis[RowIndex(1)], &row1);
  scaler.UnscaleUnitRowLeftSolve(basis[RowIndex(2)], &row2);
  EXPECT_THAT(row0.values,
              FractionalVectorComparable(DenseRow{-1.0, 0.0, 0.0}));
  EXPECT_THAT(row1.values, FractionalVectorComparable(DenseRow{0.0, 1.0, 0.5}));
  EXPECT_THAT(row2.values, FractionalVectorComparable(DenseRow{0.0, 0.0, 0.5}));
}

TEST(LpScalingHelperTest, AverageCostScaling) {
  DenseRow objective(10, 0);
  objective[ColIndex(2)] = 2.0;
  objective[ColIndex(4)] = 5.0;
  objective[ColIndex(7)] = 8.0;

  LpScalingHelper helper;
  helper.AverageCostScaling(&objective);

  const double factor = 1.0 / 5.0;
  EXPECT_EQ(helper.ObjectiveScalingFactor(), factor);

  EXPECT_EQ(objective[ColIndex(2)], 2.0 * factor);
  EXPECT_EQ(objective[ColIndex(4)], 5.0 * factor);
  EXPECT_EQ(objective[ColIndex(7)], 8.0 * factor);
}

TEST(LpScalingHelperTest, ContainOneBoundScalingDown) {
  DenseRow lbs(10, 0);
  DenseRow ubs(10, 0);
  lbs[ColIndex(2)] = 2.0;
  lbs[ColIndex(4)] = -5.0;
  lbs[ColIndex(7)] = -8.0;
  ubs[ColIndex(7)] = 3.0;

  // Note that we don't care about the magnitude position, only the non-zero
  // ones are taken into account.
  LpScalingHelper helper;
  helper.ContainOneBoundScaling(&lbs, &ubs);

  const double factor = 1.0 / 2.0;
  EXPECT_EQ(helper.BoundsScalingFactor(), factor);

  EXPECT_EQ(lbs[ColIndex(2)], 2.0 * factor);
  EXPECT_EQ(lbs[ColIndex(4)], -5.0 * factor);
  EXPECT_EQ(lbs[ColIndex(7)], -8.0 * factor);
  EXPECT_EQ(ubs[ColIndex(7)], 3.0 * factor);
}

TEST(LpScalingHelperTest, ContainOneBoundScalingUp) {
  DenseRow lbs(10, 0);
  DenseRow ubs(10, 0);
  lbs[ColIndex(2)] = 2e-2;
  lbs[ColIndex(4)] = -5e-2;
  lbs[ColIndex(7)] = -8e-2;
  ubs[ColIndex(7)] = 3e-2;

  // Note that we don't care about the magnitude position, only the non-zero
  // ones are taken into account.
  LpScalingHelper helper;
  helper.ContainOneBoundScaling(&lbs, &ubs);

  const double factor = 1.0 / 8e-2;
  EXPECT_EQ(helper.BoundsScalingFactor(), factor);

  EXPECT_EQ(lbs[ColIndex(2)], 2e-2 * factor);
  EXPECT_EQ(lbs[ColIndex(4)], -5e-2 * factor);
  EXPECT_EQ(lbs[ColIndex(7)], -8e-2 * factor);
  EXPECT_EQ(ubs[ColIndex(7)], 3e-2 * factor);
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
