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

#include "ortools/lp_data/sparse_column.h"

#include "gtest/gtest.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/permutation.h"

namespace operations_research {
namespace glop {
namespace {

// --------------------------------------------------------
// SparseColumn
// --------------------------------------------------------

// Since SparseColumn is just a straightforward clone of SparseVector, we
// merely cover its customisation. The real logic tests should be in
// sparse_vector_test.cc.
class SparseColumnTest : public ::testing::Test {
 protected:
  void SetUp() override {
    c_.SetCoefficient(RowIndex(2), 3.3);
    c_.SetCoefficient(RowIndex(4), -2.0);
  }

  SparseColumn c_;
};

TEST_F(SparseColumnTest, EntryLookUp) {
  EXPECT_EQ(RowIndex(2), c_.EntryRow(EntryIndex(0)));
  EXPECT_EQ(3.3, c_.EntryCoefficient(EntryIndex(0)));
  EXPECT_EQ(RowIndex(4), c_.EntryRow(EntryIndex(1)));
  EXPECT_EQ(-2.0, c_.EntryCoefficient(EntryIndex(1)));
  EXPECT_EQ(RowIndex(2), c_.GetFirstRow());
  EXPECT_EQ(RowIndex(4), c_.GetLastRow());
}

TEST_F(SparseColumnTest, Iterator) {
  const RowIndex kExpectedRow[] = {RowIndex(2), RowIndex(4)};
  const Fractional kExpectedCoefficient[] = {3.3, -2.0};
  int i = 0;
  for (const SparseColumn::Entry& e : c_) {
    EXPECT_EQ(kExpectedRow[i], e.row());
    EXPECT_EQ(kExpectedCoefficient[i], e.coefficient());
    ++i;
  }
  EXPECT_EQ(2, i);
}

TEST_F(SparseColumnTest, IsCleanedUp) {
  SparseColumn d;
  d.PopulateFromSparseVector(c_);
  EXPECT_TRUE(d.IsCleanedUp());
  d.SetCoefficient(RowIndex(1), 1.0);
  EXPECT_FALSE(d.IsCleanedUp());
}

TEST_F(SparseColumnTest, ApplyRowPermutation) {
  RowPermutation perm(RowIndex(6));
  perm[RowIndex(2)] = RowIndex(5);
  c_.ApplyRowPermutation(perm);
  EXPECT_EQ(EntryIndex(2), c_.num_entries());
  EXPECT_EQ(3.3, c_.LookUpCoefficient(RowIndex(5)));
}

TEST_F(SparseColumnTest, ApplyPartialRowPermutation) {
  RowPermutation perm(RowIndex(6));
  perm[RowIndex(2)] = kInvalidRow;
  c_.ApplyPartialRowPermutation(perm);
  EXPECT_EQ(EntryIndex(1), c_.num_entries());
  EXPECT_EQ(0.0, c_.LookUpCoefficient(RowIndex(2)));
}

// --------------------------------------------------------
// ColumnView
// --------------------------------------------------------

TEST(ColumnViewTest, Iterator) {
  SparseColumn d;
  d.SetCoefficient(RowIndex(2), 2.0);
  d.SetCoefficient(RowIndex(4), -2.0);
  const RowIndex kExpectedRow[] = {RowIndex(2), RowIndex(4)};
  const Fractional kExpectedCoefficient[] = {2.0, -2.0};
  int i = 0;
  for (const auto e : ColumnView(d)) {
    EXPECT_EQ(kExpectedRow[i], e.row());
    EXPECT_EQ(kExpectedCoefficient[i], e.coefficient());
    ++i;
  }
  EXPECT_EQ(2, i);
}

// --------------------------------------------------------
// RandomAccessSparseColumn
// --------------------------------------------------------
TEST(RandomAccessSparseColumnTest, InitialState) {
  const RowIndex kNumRows(3);
  RandomAccessSparseColumn column(kNumRows);

  // Check initial state.
  EXPECT_EQ(kNumRows, column.GetNumberOfRows());

  SparseColumn sparse_column;
  column.PopulateSparseColumn(&sparse_column);
  EXPECT_EQ(0, sparse_column.num_entries());

  // Check initial state remains the same after a Clear.
  column.Clear();
  EXPECT_EQ(kNumRows, column.GetNumberOfRows());

  column.PopulateSparseColumn(&sparse_column);
  EXPECT_EQ(0, sparse_column.num_entries());
}

TEST(RandomAccessSparseColumnTest, AddToCoefficient) {
  const RowIndex kNumRows(3);
  const RowIndex kRow(1);
  const Fractional kValue1(17);
  const Fractional kValue2(-10);

  RandomAccessSparseColumn column(kNumRows);
  SparseColumn sparse_column;

  column.AddToCoefficient(kRow, kValue1);
  column.PopulateSparseColumn(&sparse_column);

  EXPECT_EQ(kNumRows, column.GetNumberOfRows());
  EXPECT_EQ(kValue1, column.GetCoefficient(kRow));
  EXPECT_EQ(1, sparse_column.num_entries());

  column.AddToCoefficient(kRow, kValue2);
  column.PopulateSparseColumn(&sparse_column);

  EXPECT_EQ(kNumRows, column.GetNumberOfRows());
  EXPECT_EQ(kValue1 + kValue2, column.GetCoefficient(kRow));
  EXPECT_EQ(1, sparse_column.num_entries());
}

TEST(RandomAccessSparseColumnTest, Clear) {
  const RowIndex kNumRows(3);
  const RowIndex kRow(1);
  const Fractional kValue(17);

  RandomAccessSparseColumn column(kNumRows);
  SparseColumn sparse_column;

  column.AddToCoefficient(kRow, kValue);
  column.Clear();

  EXPECT_EQ(kNumRows, column.GetNumberOfRows());
  column.PopulateSparseColumn(&sparse_column);
  EXPECT_EQ(0, sparse_column.num_entries());
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
