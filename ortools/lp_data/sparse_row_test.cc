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

#include "ortools/lp_data/sparse_row.h"

#include "gtest/gtest.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/permutation.h"

namespace operations_research {
namespace glop {
namespace {

// Since SparseRow is just a straightforward clone of SparseVector, we merely
// cover its customisation. The real logic tests should be in
// sparse_vector_test.cc.
class SparseRowTest : public ::testing::Test {
 protected:
  void SetUp() override {
    row_.SetCoefficient(ColIndex(2), 3.3);
    row_.SetCoefficient(ColIndex(4), -2.0);
  }

  SparseRow row_;
};

TEST_F(SparseRowTest, EntryLookUp) {
  EXPECT_EQ(ColIndex(2), row_.EntryCol(EntryIndex(0)));
  EXPECT_EQ(3.3, row_.EntryCoefficient(EntryIndex(0)));
  EXPECT_EQ(ColIndex(4), row_.EntryCol(EntryIndex(1)));
  EXPECT_EQ(-2.0, row_.EntryCoefficient(EntryIndex(1)));
  EXPECT_EQ(ColIndex(2), row_.GetFirstCol());
  EXPECT_EQ(ColIndex(4), row_.GetLastCol());
}

TEST_F(SparseRowTest, Iterator) {
  const ColIndex kExpectedCol[] = {ColIndex(2), ColIndex(4)};
  const Fractional kExpectedCoefficient[] = {3.3, -2.0};
  int i = 0;
  for (const SparseRow::Entry& e : row_) {
    EXPECT_EQ(kExpectedCol[i], e.col());
    EXPECT_EQ(kExpectedCoefficient[i], e.coefficient());
    ++i;
  }
  EXPECT_EQ(2, i);
}

TEST_F(SparseRowTest, IsCleanedUp) {
  SparseRow d;
  d.PopulateFromSparseVector(row_);
  EXPECT_TRUE(d.IsCleanedUp());
  d.SetCoefficient(ColIndex(1), 1.0);
  EXPECT_FALSE(d.IsCleanedUp());
}

TEST_F(SparseRowTest, ApplyColPermutation) {
  ColumnPermutation perm(ColIndex(6));
  perm[ColIndex(2)] = ColIndex(5);
  row_.ApplyColPermutation(perm);
  EXPECT_EQ(EntryIndex(2), row_.num_entries());
  EXPECT_EQ(3.3, row_.LookUpCoefficient(ColIndex(5)));
}

TEST_F(SparseRowTest, ApplyPartialColPermutation) {
  ColumnPermutation perm(ColIndex(6));
  perm[ColIndex(2)] = kInvalidCol;
  row_.ApplyPartialColPermutation(perm);
  EXPECT_EQ(EntryIndex(1), row_.num_entries());
  EXPECT_EQ(0.0, row_.LookUpCoefficient(ColIndex(2)));
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
