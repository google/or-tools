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

#include "ortools/constraint_solver/reversible_data.h"

#include <stddef.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solver_test_util.h"

namespace operations_research {
namespace {

// TODO(user): Below is implemented a non-regression test.
//              Unit tests for class RevBitSet should be added too.
class RevBitSetTest : public testing::Test {
 protected:
  void SetUp() override {
    const int kBitSetSize = 100;
    solver_ = std::make_unique<Solver>("RevBitSetTest");
    reversible_bitset_ = new RevBitSet(kBitSetSize);
  }
  void TearDown() override { delete reversible_bitset_; }

  std::unique_ptr<Solver> solver_;
  RevBitSet* reversible_bitset_;
};

TEST_IN_PROPAGATION1(RevBitSetTest, OneElement, solver, true, RevBitSet*,
                     reversible_bitset_) {
  reversible_bitset_->SetToOne(solver, 2);
  EXPECT_TRUE(reversible_bitset_->IsCardinalityOne());
  return nullptr;
}

TEST_IN_PROPAGATION1(RevBitSetTest, OneElementInSecondUint64, solver, true,
                     RevBitSet*, reversible_bitset_) {
  reversible_bitset_->SetToOne(solver, 70);
  EXPECT_TRUE(reversible_bitset_->IsCardinalityOne());
  return nullptr;
}

TEST_IN_PROPAGATION1(RevBitSetTest, TwoElementsInFirstUint64, solver, true,
                     RevBitSet*, reversible_bitset_) {
  reversible_bitset_->SetToOne(solver, 2);
  reversible_bitset_->SetToOne(solver, 3);
  EXPECT_FALSE(reversible_bitset_->IsCardinalityOne());
  return nullptr;
}

TEST_IN_PROPAGATION1(RevBitSetTest, TwoElementsInTwoUint64, solver, true,
                     RevBitSet*, reversible_bitset_) {
  reversible_bitset_->SetToOne(solver, 2);
  reversible_bitset_->SetToOne(solver, 70);
  EXPECT_FALSE(reversible_bitset_->IsCardinalityOne());
  return nullptr;
}

TEST_IN_PROPAGATION1(RevBitSetTest, ThreeElementsInTwoUint64, solver, true,
                     RevBitSet*, reversible_bitset_) {
  reversible_bitset_->SetToOne(solver, 2);
  reversible_bitset_->SetToOne(solver, 3);
  reversible_bitset_->SetToOne(solver, 70);
  EXPECT_FALSE(reversible_bitset_->IsCardinalityOne());
  return nullptr;
}

TEST_IN_PROPAGATION1(RevBitSetTest, ThreeElementsWithTwoInSecondUint64, solver,
                     true, RevBitSet*, reversible_bitset_) {
  reversible_bitset_->SetToOne(solver, 2);
  reversible_bitset_->SetToOne(solver, 70);
  reversible_bitset_->SetToOne(solver, 71);
  EXPECT_FALSE(reversible_bitset_->IsCardinalityOne());
  return nullptr;
}

// Test RevIntSet

TEST(RevIntSetTest, iterator) {
  Solver solver("RevIntSetTest::iterator");
  solver.PushState();
  RevIntSet<int> revset(5);
  EXPECT_EQ(5, revset.Capacity());
  revset.Insert(&solver, 2);
  revset.Insert(&solver, 0);
  revset.Insert(&solver, 3);
  EXPECT_EQ(3, revset.Size());
  std::vector<bool> marks(5);
  for (int i : revset) {
    marks[i] = true;
  }
  EXPECT_TRUE(marks[0]);
  EXPECT_FALSE(marks[1]);
  EXPECT_TRUE(marks[2]);
  EXPECT_TRUE(marks[3]);
  EXPECT_FALSE(marks[4]);
  solver.PopState();
}

// Test UnsortedNullableRevBitset.

TEST(UnsortedNullableRevBitsetTest, Init) {
  Solver solver("UnsortedNullableRevBitsetTest::Init");
  // The data structures in the UnsortedNullableRevBitset must be cleared
  // before we delete them, and the solver. Thus we enclose all the life cycle
  // of the reversible bitset by PushState/PopState.
  // In practice, the bitset will be allocated inside a constraint, that will be
  // be cleaned before the solver is cleaned.
  solver.PushState();
  UnsortedNullableRevBitset bits(256);
  std::vector<uint64_t> mask = {5, 0, 5, 0};
  bits.Init(&solver, mask);
  EXPECT_EQ(2, bits.ActiveWordSize());
  EXPECT_EQ(256, bits.bit_size());
  EXPECT_EQ(4, bits.word_size());
  int pos = 0;
  std::vector<uint64_t> mask1 = {1, 0};
  std::vector<uint64_t> mask2 = {0, 5, 0, 5};
  std::vector<uint64_t> mask3 = {2, 4, 4, 1};
  EXPECT_TRUE(bits.Intersects(mask1, &pos));
  EXPECT_EQ(0, pos);
  EXPECT_FALSE(bits.Intersects(mask2, &pos));
  EXPECT_EQ(0, pos);
  EXPECT_TRUE(bits.Intersects(mask3, &pos));
  EXPECT_EQ(2, pos);
  solver.PopState();
}

TEST(UnsortedNullableRevBitsetTest, RevSubtract) {
  Solver solver("UnsortedNullableRevBitsetTest::RevSubtract");
  solver.PushState();
  UnsortedNullableRevBitset bits(256);
  std::vector<uint64_t> mask = {5, 5, 5, 0};
  bits.Init(&solver, mask);
  std::vector<uint64_t> mask1 = {1, 0, 5};
  std::vector<uint64_t> mask2 = {1, 0, 0, 0};
  std::vector<uint64_t> mask3 = {4, 4, 2, 0};
  std::vector<uint64_t> mask4 = {0, 1};
  for (int loop = 0; loop < 10; ++loop) {
    solver.PushState();
    EXPECT_EQ(3, bits.ActiveWordSize());
    EXPECT_TRUE(bits.RevSubtract(&solver, mask1));
    EXPECT_FALSE(bits.RevSubtract(&solver, mask1));
    EXPECT_EQ(2, bits.ActiveWordSize());
    EXPECT_FALSE(bits.Empty());
    solver.PushState();
    EXPECT_FALSE(bits.RevSubtract(&solver, mask2));
    EXPECT_TRUE(bits.RevSubtract(&solver, mask3));
    EXPECT_EQ(1, bits.ActiveWordSize());
    EXPECT_FALSE(bits.Empty());
    solver.PushState();
    bits.RevSubtract(&solver, mask4);
    EXPECT_EQ(0, bits.ActiveWordSize());
    EXPECT_TRUE(bits.Empty());
    solver.PopState();
    EXPECT_EQ(1, bits.ActiveWordSize());
    solver.PopState();
    EXPECT_EQ(2, bits.ActiveWordSize());
    solver.PopState();
    EXPECT_EQ(3, bits.ActiveWordSize());
  }
  solver.PopState();
}

TEST(UnsortedNullableRevBitsetTest, RevAnd) {
  Solver solver("UnsortedNullableRevBitsetTest::RevAnd");
  solver.PushState();
  UnsortedNullableRevBitset bits(256);
  std::vector<uint64_t> mask = {5, 5, 5, 0};
  bits.Init(&solver, mask);
  std::vector<uint64_t> mask1 = {4, 5, 0};
  std::vector<uint64_t> mask2 = {4, 5, 5, 0};
  std::vector<uint64_t> mask3 = {1, 1, 7, 0};
  std::vector<uint64_t> mask4 = {0, 4};
  for (int loop = 0; loop < 10; ++loop) {
    solver.PushState();
    EXPECT_EQ(3, bits.ActiveWordSize());
    EXPECT_TRUE(bits.RevAnd(&solver, mask1));
    EXPECT_FALSE(bits.RevAnd(&solver, mask1));
    EXPECT_EQ(2, bits.ActiveWordSize());
    EXPECT_FALSE(bits.Empty());
    solver.PushState();
    EXPECT_FALSE(bits.RevAnd(&solver, mask2));
    EXPECT_TRUE(bits.RevAnd(&solver, mask3));
    EXPECT_EQ(1, bits.ActiveWordSize());
    EXPECT_FALSE(bits.Empty());
    solver.PushState();
    bits.RevAnd(&solver, mask4);
    EXPECT_EQ(0, bits.ActiveWordSize());
    EXPECT_TRUE(bits.Empty());
    solver.PopState();
    EXPECT_EQ(1, bits.ActiveWordSize());
    solver.PopState();
    EXPECT_EQ(2, bits.ActiveWordSize());
    solver.PopState();
    EXPECT_EQ(3, bits.ActiveWordSize());
  }
  solver.PopState();
}
}  // namespace
}  // namespace operations_research
