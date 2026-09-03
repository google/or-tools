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

#include "ortools/glop/variables_info.h"

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/sparse.h"

namespace operations_research {
namespace glop {
namespace {

TEST(VariablesInfoTest, Initialization) {
  const ColIndex kNumCols(11);
  SparseMatrix matrix;
  matrix.PopulateFromIdentity(kNumCols - 1);
  matrix.AppendEmptyColumn();
  CompactSparseMatrix compact_matrix(matrix);
  const Fractional kInf = kInfinity;
  DenseRow lower_bounds(
      {-kInf, -kInf, 0.0, 0.0, 0.0, -kInf, -kInf, 0.0, 0.0, 0.0, 0.0});
  DenseRow upper_bounds(
      {kInf, 0.0, kInf, 0.0, 10.0, kInf, 0.0, kInf, 0.0, 10.0, 10.0});

  VariablesInfo info(compact_matrix);
  const bool bounds_are_unchanged =
      info.LoadBoundsAndReturnTrueIfUnchanged(lower_bounds, upper_bounds);
  info.InitializeToDefaultStatus();
  EXPECT_FALSE(bounds_are_unchanged);
  VariableStatusRow statuses(
      {VariableStatus::BASIC, VariableStatus::BASIC, VariableStatus::BASIC,
       VariableStatus::BASIC, VariableStatus::BASIC, VariableStatus::FREE,
       VariableStatus::AT_UPPER_BOUND, VariableStatus::AT_LOWER_BOUND,
       VariableStatus::FIXED_VALUE, VariableStatus::AT_LOWER_BOUND,
       VariableStatus::AT_LOWER_BOUND});
  for (ColIndex col(0); col < kNumCols; ++col) {
    if (statuses[col] == VariableStatus::BASIC) {
      info.UpdateToBasicStatus(col);
    } else {
      info.UpdateToNonBasicStatus(col, statuses[col]);
    }
  }

  EXPECT_THAT(info.GetStatusRow(), statuses);
  EXPECT_THAT(
      info.GetTypeRow(),
      VariableTypeRow(
          {VariableType::UNCONSTRAINED, VariableType::UPPER_BOUNDED,
           VariableType::LOWER_BOUNDED, VariableType::FIXED_VARIABLE,
           VariableType::UPPER_AND_LOWER_BOUNDED, VariableType::UNCONSTRAINED,
           VariableType::UPPER_BOUNDED, VariableType::LOWER_BOUNDED,
           VariableType::FIXED_VARIABLE, VariableType::UPPER_AND_LOWER_BOUNDED,
           VariableType::UPPER_AND_LOWER_BOUNDED}));
  EXPECT_EQ(4, info.GetNumEntriesInRelevantColumns());

  // Bitset are more annoying to test...
  const DenseBitRow& can_increase = info.GetCanIncreaseBitRow();
  const DenseBitRow& can_decrease = info.GetCanDecreaseBitRow();
  const DenseBitRow& is_relevant = info.GetIsRelevantBitRow();
  const DenseBitRow& is_basic = info.GetIsBasicBitRow();
  const DenseBitRow& not_basic = info.GetNotBasicBitRow();

  for (ColIndex col(0); col < 5; ++col) {
    EXPECT_FALSE(can_increase.IsSet(col));
    EXPECT_FALSE(can_decrease.IsSet(col));
    EXPECT_FALSE(is_relevant.IsSet(col));
    EXPECT_TRUE(is_basic.IsSet(col));
    EXPECT_FALSE(not_basic.IsSet(col));
  }
  for (ColIndex col(5); col < kNumCols; ++col) {
    EXPECT_FALSE(is_basic.IsSet(col));
    EXPECT_TRUE(not_basic.IsSet(col));
  }

  EXPECT_TRUE(can_increase.IsSet(ColIndex(5)));
  EXPECT_TRUE(can_decrease.IsSet(ColIndex(5)));
  EXPECT_TRUE(is_relevant.IsSet(ColIndex(5)));

  EXPECT_FALSE(can_increase.IsSet(ColIndex(6)));
  EXPECT_TRUE(can_decrease.IsSet(ColIndex(6)));
  EXPECT_TRUE(is_relevant.IsSet(ColIndex(6)));

  EXPECT_TRUE(can_increase.IsSet(ColIndex(7)));
  EXPECT_FALSE(can_decrease.IsSet(ColIndex(7)));
  EXPECT_TRUE(is_relevant.IsSet(ColIndex(7)));

  EXPECT_FALSE(can_increase.IsSet(ColIndex(8)));
  EXPECT_FALSE(can_decrease.IsSet(ColIndex(8)));
  EXPECT_FALSE(is_relevant.IsSet(ColIndex(8)));

  EXPECT_TRUE(can_increase.IsSet(ColIndex(9)));
  EXPECT_FALSE(can_decrease.IsSet(ColIndex(9)));
  EXPECT_TRUE(is_relevant.IsSet(ColIndex(9)));

  EXPECT_FALSE(can_decrease.IsSet(ColIndex(10)));
  EXPECT_TRUE(can_increase.IsSet(ColIndex(10)));
  EXPECT_TRUE(is_relevant.IsSet(ColIndex(10)));
}

TEST(VariablesInfoTest, WarmStart) {
  const ColIndex kNumCols(10);
  SparseMatrix matrix;
  matrix.PopulateFromIdentity(kNumCols - 1);
  matrix.AppendEmptyColumn();
  CompactSparseMatrix compact_matrix(matrix);
  const Fractional kInf = kInfinity;
  DenseRow lower_bounds(
      {-kInf, -kInf, 0.0, 0.0, 0.0, -kInf, -kInf, 0.0, 0.0, 0.0});
  DenseRow upper_bounds(
      {kInf, 0.0, kInf, 0.0, 10.0, kInf, 0.0, kInf, 0.0, 10.0});

  VariablesInfo info(compact_matrix);
  info.LoadBoundsAndReturnTrueIfUnchanged(lower_bounds, upper_bounds);
  info.InitializeToDefaultStatus();

  BasisState state;
  state.statuses = VariableStatusRow(kNumCols, VariableStatus::AT_LOWER_BOUND);
  info.InitializeFromBasisState(kNumCols, ColIndex(0), state);
  EXPECT_THAT(
      info.GetStatusRow(),
      VariableStatusRow(
          {VariableStatus::FREE, VariableStatus::AT_UPPER_BOUND,
           VariableStatus::AT_LOWER_BOUND, VariableStatus::FIXED_VALUE,
           VariableStatus::AT_LOWER_BOUND, VariableStatus::FREE,
           VariableStatus::AT_UPPER_BOUND, VariableStatus::AT_LOWER_BOUND,
           VariableStatus::FIXED_VALUE, VariableStatus::AT_LOWER_BOUND}));

  state.statuses = VariableStatusRow(kNumCols, VariableStatus::AT_UPPER_BOUND);
  info.InitializeFromBasisState(kNumCols, ColIndex(0), state);
  EXPECT_THAT(
      info.GetStatusRow(),
      VariableStatusRow(
          {VariableStatus::FREE, VariableStatus::AT_UPPER_BOUND,
           VariableStatus::AT_LOWER_BOUND, VariableStatus::FIXED_VALUE,
           VariableStatus::AT_UPPER_BOUND, VariableStatus::FREE,
           VariableStatus::AT_UPPER_BOUND, VariableStatus::AT_LOWER_BOUND,
           VariableStatus::FIXED_VALUE, VariableStatus::AT_UPPER_BOUND}));
}

TEST(VariablesInfoTest, ChangeUnusedBasicVariablesToFree) {
  const ColIndex num_cols(6);
  SparseMatrix matrix;
  matrix.PopulateFromIdentity(num_cols);
  CompactSparseMatrix compact_matrix(matrix);
  DenseRow lower_bounds(num_cols, 0.0);
  DenseRow upper_bounds(num_cols, 1.0);

  VariablesInfo info(compact_matrix);
  info.LoadBoundsAndReturnTrueIfUnchanged(lower_bounds, upper_bounds);
  info.InitializeToDefaultStatus();

  BasisState state;
  state.statuses = VariableStatusRow(num_cols, VariableStatus::BASIC);
  info.InitializeFromBasisState(num_cols, ColIndex(0), state);

  RowToColMapping basis;
  basis.push_back(ColIndex(2));
  basis.push_back(ColIndex(4));
  EXPECT_EQ(4, info.ChangeUnusedBasicVariablesToFree(basis));
  EXPECT_THAT(info.GetStatusRow(),
              VariableStatusRow({VariableStatus::FREE, VariableStatus::FREE,
                                 VariableStatus::BASIC, VariableStatus::FREE,
                                 VariableStatus::BASIC, VariableStatus::FREE}));
}

TEST(VariablesInfoTest, SnapFreeVariablesToBound) {
  const ColIndex num_cols(6);
  SparseMatrix matrix;
  matrix.PopulateFromIdentity(num_cols);
  CompactSparseMatrix compact_matrix(matrix);
  DenseRow lower_bounds(num_cols, -1.0);
  DenseRow upper_bounds(num_cols, 1.0);

  VariablesInfo info(compact_matrix);
  info.LoadBoundsAndReturnTrueIfUnchanged(lower_bounds, upper_bounds);
  info.InitializeToDefaultStatus();

  // All should be free after this.
  BasisState state;
  state.statuses = VariableStatusRow(num_cols, VariableStatus::BASIC);
  info.InitializeFromBasisState(num_cols, ColIndex(0), state);
  RowToColMapping basis;
  EXPECT_EQ(num_cols, info.ChangeUnusedBasicVariablesToFree(basis));

  // The vector does not need to be of full size, non-listed variables will be
  // assumed to be at zero.
  DenseRow values{0.0, -0.5, 0.5, 1.0, -2.0};

  // Only the one past their bound here.
  info.SnapFreeVariablesToBound(0.0, values);
  EXPECT_THAT(info.GetStatusRow(),
              VariableStatusRow(
                  {VariableStatus::FREE, VariableStatus::FREE,
                   VariableStatus::FREE, VariableStatus::AT_UPPER_BOUND,
                   VariableStatus::AT_LOWER_BOUND, VariableStatus::FREE}));

  // Increase the distance, only two left.
  info.SnapFreeVariablesToBound(0.5, values);
  EXPECT_THAT(
      info.GetStatusRow(),
      VariableStatusRow(
          {VariableStatus::FREE, VariableStatus::AT_LOWER_BOUND,
           VariableStatus::AT_UPPER_BOUND, VariableStatus::AT_UPPER_BOUND,
           VariableStatus::AT_LOWER_BOUND, VariableStatus::FREE}));

  // If distance is the same, we prefer lower bound.
  info.SnapFreeVariablesToBound(kInfinity, values);
  EXPECT_THAT(
      info.GetStatusRow(),
      VariableStatusRow(
          {VariableStatus::AT_LOWER_BOUND, VariableStatus::AT_LOWER_BOUND,
           VariableStatus::AT_UPPER_BOUND, VariableStatus::AT_UPPER_BOUND,
           VariableStatus::AT_LOWER_BOUND, VariableStatus::AT_LOWER_BOUND}));
}

TEST(VariablesInfoTest, SnapFreeVariablesToBoundDoNothingForRealFreeVariable) {
  const ColIndex num_cols(6);
  SparseMatrix matrix;
  matrix.PopulateFromIdentity(num_cols);
  CompactSparseMatrix compact_matrix(matrix);
  DenseRow lower_bounds(num_cols, -kInfinity);
  DenseRow upper_bounds(num_cols, kInfinity);

  VariablesInfo info(compact_matrix);
  info.LoadBoundsAndReturnTrueIfUnchanged(lower_bounds, upper_bounds);
  info.InitializeToDefaultStatus();

  // We cannot snap free variables to a bound.
  DenseRow unused;
  info.SnapFreeVariablesToBound(kInfinity, unused);
  EXPECT_THAT(info.GetStatusRow(),
              VariableStatusRow({VariableStatus::FREE, VariableStatus::FREE,
                                 VariableStatus::FREE, VariableStatus::FREE,
                                 VariableStatus::FREE, VariableStatus::FREE}));
}

TEST(VariablesInfoTest, DualPhaseI) {
  const ColIndex kNumCols(6);
  SparseMatrix matrix;
  matrix.PopulateFromIdentity(kNumCols - 1);
  matrix.AppendEmptyColumn();
  CompactSparseMatrix compact_matrix(matrix);
  const Fractional kInf = kInfinity;
  DenseRow lower_bounds({-kInf, -kInf, 0.0, 0.0, 0.0, -10.0});
  DenseRow upper_bounds({kInf, 0.0, kInf, kInf, 10.0, 10.0});

  VariablesInfo info(compact_matrix);
  info.LoadBoundsAndReturnTrueIfUnchanged(lower_bounds, upper_bounds);
  info.InitializeToDefaultStatus();

  DenseRow reduced_costs({-1.0, 1.0, -1.0, 1.0, -1.0, 1.0});
  info.TransformToDualPhaseIProblem(1e-6, reduced_costs.const_view());

  EXPECT_THAT(info.GetVariableLowerBounds(),
              DenseRow({-1000, -1.0, 0.0, 0.0, 0.0, 0.0}));
  EXPECT_THAT(info.GetVariableUpperBounds(),
              DenseRow({1000, 0.0, 1.0, 1.0, 0.0, 0.0}));
  EXPECT_THAT(
      info.GetStatusRow(),
      VariableStatusRow(
          {VariableStatus::AT_UPPER_BOUND, VariableStatus::AT_LOWER_BOUND,
           VariableStatus::AT_UPPER_BOUND, VariableStatus::AT_LOWER_BOUND,
           VariableStatus::FIXED_VALUE, VariableStatus::FIXED_VALUE}));
}

TEST(VariablesInfoTest, NotInEquationForm) {
  const ColIndex num_cols(3);
  const RowIndex num_rows(4);
  SparseMatrix matrix;
  matrix.PopulateFromZero(num_rows, num_cols + RowToColIndex(num_rows));
  CompactSparseMatrix compact_matrix(matrix);

  DenseRow var_lbs({-1, -2, -3});
  DenseRow var_ubs({10, 10, 10});
  DenseColumn constraint_lbs({1, 2, 3, 4});
  DenseColumn constraint_ubs({9, 8, 7, 6});

  VariablesInfo info(compact_matrix);
  info.LoadBoundsAndReturnTrueIfUnchanged(var_lbs, var_ubs, constraint_lbs,
                                          constraint_ubs);
  EXPECT_THAT(info.GetVariableLowerBounds(),
              DenseRow({-1, -2, -3, -9, -8, -7, -6}));
  EXPECT_THAT(info.GetVariableUpperBounds(),
              DenseRow({10, 10, 10, -1, -2, -3, -4}));
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
