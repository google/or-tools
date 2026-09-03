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

#include "ortools/lp_data/sparse_vector.h"

#include <limits>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/permutation.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace glop {

namespace {

using ::testing::ContainerEq;

// We use our own specialization of SparseVector<> with a custom integral type.
DEFINE_STRONG_INDEX_TYPE(TestIndexType);

TEST(SparseVectorTest, InitialValues) {
  const SparseVector<TestIndexType> sparse_vector;
  EXPECT_EQ(0, sparse_vector.num_entries());
  EXPECT_TRUE(sparse_vector.IsEmpty());
}

TEST(SparseVectorTest, SetCoefficient) {
  const TestIndexType kIndex(3);
  const Fractional kCoeff(-1);

  SparseVector<TestIndexType> sparse_vector;
  EXPECT_TRUE(sparse_vector.CheckNoDuplicates());
  EXPECT_TRUE(sparse_vector.CheckNoDuplicates());

  sparse_vector.SetCoefficient(kIndex, kCoeff);
  EXPECT_TRUE(sparse_vector.CheckNoDuplicates());
  EXPECT_TRUE(sparse_vector.CheckNoDuplicates());
  EXPECT_EQ(1, sparse_vector.num_entries());

  const Fractional coeff = sparse_vector.LookUpCoefficient(kIndex);
  EXPECT_EQ(kCoeff, coeff);
}

TEST(SparseVectorTest, CleanUpZeroValues) {
  const TestIndexType kIndex(3);
  const Fractional kCoeff(-1);

  SparseVector<TestIndexType> sparse_vector;
  sparse_vector.SetCoefficient(kIndex, kCoeff);
  for (TestIndexType index(0); index < kIndex; ++index) {
    sparse_vector.SetCoefficient(index, Fractional(0.0));
  }
  EXPECT_TRUE(sparse_vector.CheckNoDuplicates());

  EXPECT_EQ(kIndex.value() + 1, sparse_vector.num_entries());
  sparse_vector.CleanUp();
  EXPECT_EQ(1, sparse_vector.num_entries());

  const Fractional coeff = sparse_vector.LookUpCoefficient(kIndex);
  EXPECT_EQ(kCoeff, coeff);
  for (TestIndexType index(0); index < kIndex; ++index) {
    EXPECT_EQ(sparse_vector.LookUpCoefficient(index), 0.0);
  }
  EXPECT_TRUE(sparse_vector.CheckNoDuplicates());
}

TEST(SparseVectorTest, CleanUpDuplicates) {
  const TestIndexType kIndex(3);
  const Fractional kCoeff(-1);

  SparseVector<TestIndexType> sparse_vector;
  sparse_vector.SetCoefficient(kIndex, kCoeff);
  sparse_vector.SetCoefficient(kIndex, kCoeff);
  EXPECT_FALSE(sparse_vector.CheckNoDuplicates());

  sparse_vector.CleanUp();
  EXPECT_EQ(1, sparse_vector.num_entries());

  const Fractional coeff = sparse_vector.LookUpCoefficient(kIndex);
  EXPECT_EQ(kCoeff, coeff);
  EXPECT_TRUE(sparse_vector.CheckNoDuplicates());
}

TEST(SparseVectorTest, CleanUpEmptyVector) {
  SparseVector<TestIndexType> sparse_vector;
  sparse_vector.CleanUp();
  EXPECT_EQ("", sparse_vector.DebugString());
}

TEST(SparseVectorTest, CleanUp) {
  SparseVector<TestIndexType> sparse_vector;

  // [0]=1, [1]=-1, [0]=0, [1]=1
  sparse_vector.SetCoefficient(TestIndexType(0), 1.0);
  sparse_vector.SetCoefficient(TestIndexType(1), -1.0);
  sparse_vector.SetCoefficient(TestIndexType(0), 0.0);
  sparse_vector.SetCoefficient(TestIndexType(1), 1.0);
  EXPECT_FALSE(sparse_vector.CheckNoDuplicates());

  sparse_vector.CleanUp();
  EXPECT_EQ("[1]=1", sparse_vector.DebugString());
}

TEST(SparseVectorTest, CleanUpFirstZero) {
  SparseVector<TestIndexType> sparse_vector;

  // [0]=0, [1]=-1, [0]=0, [1]=1
  sparse_vector.SetCoefficient(TestIndexType(0), 0.0);
  sparse_vector.SetCoefficient(TestIndexType(1), -1.0);
  sparse_vector.SetCoefficient(TestIndexType(0), 0.0);
  sparse_vector.SetCoefficient(TestIndexType(1), 1.0);
  EXPECT_FALSE(sparse_vector.CheckNoDuplicates());

  sparse_vector.CleanUp();
  EXPECT_EQ("[1]=1", sparse_vector.DebugString());
}

TEST(SparseVectorTest, CleanUpManyDuplicates) {
  SparseVector<TestIndexType> sparse_vector;

  // [0]=0, [0]=-1, [0]=0, [1]=1, [3]=0, [3]=1, [3]=0, [5]=2, [5]=0.
  sparse_vector.SetCoefficient(TestIndexType(0), 0.0);
  sparse_vector.SetCoefficient(TestIndexType(0), -1.0);
  sparse_vector.SetCoefficient(TestIndexType(0), 0.0);
  sparse_vector.SetCoefficient(TestIndexType(1), 1.0);
  sparse_vector.SetCoefficient(TestIndexType(3), 0.0);
  sparse_vector.SetCoefficient(TestIndexType(3), 1.0);
  sparse_vector.SetCoefficient(TestIndexType(3), 0.0);
  sparse_vector.SetCoefficient(TestIndexType(5), 2.0);
  sparse_vector.SetCoefficient(TestIndexType(5), 0.0);
  EXPECT_FALSE(sparse_vector.CheckNoDuplicates());

  sparse_vector.CleanUp();
  EXPECT_EQ("[1]=1", sparse_vector.DebugString());
}

TEST(SparseVectorTest, IsCleanedUp) {
  SparseVector<TestIndexType> sparse_vector;
  sparse_vector.SetCoefficient(TestIndexType(1), 1.0);
  sparse_vector.SetCoefficient(TestIndexType(7), 2.0);
  sparse_vector.SetCoefficient(TestIndexType(6), -1.0);
  EXPECT_TRUE(sparse_vector.CheckNoDuplicates());
  EXPECT_FALSE(sparse_vector.IsCleanedUp());
  sparse_vector.CleanUp();
  EXPECT_TRUE(sparse_vector.IsCleanedUp());
}

TEST(SparseVectorTest, IsCleanedUpCheckForZero) {
  SparseVector<TestIndexType> sparse_vector;
  sparse_vector.SetCoefficient(TestIndexType(1), 1.0);
  sparse_vector.SetCoefficient(TestIndexType(4), 0.0);
  sparse_vector.SetCoefficient(TestIndexType(6), -1.0);
  EXPECT_TRUE(sparse_vector.CheckNoDuplicates());
  EXPECT_FALSE(sparse_vector.IsCleanedUp());
  sparse_vector.CleanUp();
  EXPECT_TRUE(sparse_vector.IsCleanedUp());
  EXPECT_EQ("[1]=1, [6]=-1", sparse_vector.DebugString());
}

TEST(SparseVectorTest, DeleteEntry) {
  SparseVector<TestIndexType> a;
  a.SetCoefficient(TestIndexType(1), 1.0);
  a.SetCoefficient(TestIndexType(6), -1.0);
  a.SetCoefficient(TestIndexType(7), 2.0);
  EXPECT_TRUE(a.CheckNoDuplicates());

  SparseVector<TestIndexType> b;
  b.PopulateFromSparseVector(a);
  b.DeleteEntry(TestIndexType(2));
  EXPECT_TRUE(b.IsEqualTo(a));

  b.DeleteEntry(TestIndexType(6));
  EXPECT_EQ("[1]=1, [7]=2", b.DebugString());
}

TEST(SparseVectorTest, RemoveNearZeroEntries) {
  SparseVector<TestIndexType> a, expected;
  a.RemoveNearZeroEntries(0.99);
  EXPECT_TRUE(a.IsEmpty());

  a.SetCoefficient(TestIndexType(1), 1.0);
  a.SetCoefficient(TestIndexType(6), -1.0);
  a.SetCoefficient(TestIndexType(7), 2.0);
  expected.PopulateFromSparseVector(a);
  a.RemoveNearZeroEntries(0.99);
  EXPECT_TRUE(expected.IsEqualTo(a));

  a.RemoveNearZeroEntries(1.01);
  EXPECT_EQ("[7]=2", a.DebugString());

  a.RemoveNearZeroEntries(2.0);
  EXPECT_TRUE(a.IsEmpty());
}

TEST(SparseVectorTest, RemoveNearZeroEntriesWithWeights) {
  SparseVector<TestIndexType> a;
  a.SetCoefficient(TestIndexType(1), 1.0);
  a.SetCoefficient(TestIndexType(6), -1.0);
  a.SetCoefficient(TestIndexType(7), 2.0);

  StrictITIVector<TestIndexType, Fractional> weights(TestIndexType(10), 1.0);
  a.RemoveNearZeroEntriesWithWeights(0.99, weights);
  EXPECT_EQ("[1]=1, [6]=-1, [7]=2", a.DebugString());

  weights[TestIndexType(1)] = 0.5;
  a.RemoveNearZeroEntriesWithWeights(0.99, weights);
  EXPECT_EQ("[6]=-1, [7]=2", a.DebugString());

  weights[TestIndexType(6)] = 0.0;
  weights[TestIndexType(7)] = kInfinity;
  a.RemoveNearZeroEntriesWithWeights(1e9, weights);
  EXPECT_EQ("[7]=2", a.DebugString());
  a.RemoveNearZeroEntriesWithWeights(1e-9, weights);
  EXPECT_EQ("[7]=2", a.DebugString());

  weights.assign(TestIndexType(10), 0.1);
  a.RemoveNearZeroEntriesWithWeights(10.0, weights);
  EXPECT_TRUE(a.IsEmpty());
}

TEST(SparseVectorTest, MoveEntryToFirstPosition) {
  SparseVector<TestIndexType> a;
  a.SetCoefficient(TestIndexType(1), 1.0);
  a.SetCoefficient(TestIndexType(6), -1.0);
  a.SetCoefficient(TestIndexType(7), 2.0);
  EXPECT_TRUE(a.CheckNoDuplicates());

  // Behavior when the entry exist.
  a.MoveEntryToFirstPosition(TestIndexType(7));
  EXPECT_EQ("[7]=2, [6]=-1, [1]=1", a.DebugString());

  // Behavior when the entry doesn't exist.
  a.MoveEntryToFirstPosition(TestIndexType(3));
  EXPECT_EQ("[7]=2, [6]=-1, [1]=1", a.DebugString());
}

TEST(SparseVectorTest, MoveEntryToLastPosition) {
  SparseVector<TestIndexType> a;
  a.SetCoefficient(TestIndexType(1), 1.0);
  a.SetCoefficient(TestIndexType(6), -1.0);
  a.SetCoefficient(TestIndexType(7), 2.0);
  EXPECT_TRUE(a.CheckNoDuplicates());

  // Behavior when the entry exist.
  a.MoveEntryToLastPosition(TestIndexType(1));
  EXPECT_EQ("[7]=2, [6]=-1, [1]=1", a.DebugString());

  // Behavior when the entry doesn't exist.
  a.MoveEntryToLastPosition(TestIndexType(3));
  EXPECT_EQ("[7]=2, [6]=-1, [1]=1", a.DebugString());
}

TEST(SparseVectorTest, AddMultipleToDenseVector) {
  SparseVector<TestIndexType> sparse_vector;
  sparse_vector.SetCoefficient(TestIndexType(1), 1.0);
  sparse_vector.SetCoefficient(TestIndexType(7), 2.0);
  sparse_vector.SetCoefficient(TestIndexType(6), -1.0);

  const TestIndexType kSize(10);
  StrictITIVector<TestIndexType, Fractional> dense_vector(kSize, 0.0);
  dense_vector[TestIndexType(1)] = 10.0;
  dense_vector[TestIndexType(2)] = 5.0;

  StrictITIVector<TestIndexType, Fractional> expected = dense_vector;
  sparse_vector.AddMultipleToDenseVector(0.0, &dense_vector);
  EXPECT_THAT(dense_vector, ContainerEq(expected));

  sparse_vector.AddMultipleToDenseVector(1.0, &dense_vector);
  sparse_vector.AddMultipleToDenseVector(-1.0, &dense_vector);
  EXPECT_THAT(dense_vector, ContainerEq(expected));

  sparse_vector.AddMultipleToDenseVector(10.0, &dense_vector);
  sparse_vector.AddMultipleToDenseVector(-4.0, &dense_vector);
  sparse_vector.AddMultipleToDenseVector(-6.0, &dense_vector);
  EXPECT_THAT(dense_vector, ContainerEq(expected));

  expected[TestIndexType(1)] = 12.0;
  expected[TestIndexType(7)] = 4.0;
  expected[TestIndexType(6)] = -2.0;
  sparse_vector.AddMultipleToDenseVector(2.0, &dense_vector);
  EXPECT_THAT(dense_vector, ContainerEq(expected));
}

TEST(SparseVectorTest,
     AddMultipleToSparseVectorAndDeleteCommonIndex_LongerB_ZeroEntries) {
  SparseVector<TestIndexType> a;
  a.SetCoefficient(TestIndexType(1), 1.0);
  a.SetCoefficient(TestIndexType(6), -1.0);
  a.SetCoefficient(TestIndexType(7), 2.0);

  SparseVector<TestIndexType> b;
  b.SetCoefficient(TestIndexType(1), 1.0);
  b.SetCoefficient(TestIndexType(3), 1.0);
  b.SetCoefficient(TestIndexType(6), 0.5);
  b.SetCoefficient(TestIndexType(7), 4.0);
  b.SetCoefficient(TestIndexType(8), -1.0);

  a.AddMultipleToSparseVectorAndDeleteCommonIndex(-2.0, TestIndexType(1), 1e-12,
                                                  &b);
  EXPECT_EQ("[3]=1, [6]=2.5, [8]=-1", b.DebugString());
}

TEST(SparseVectorTest, AddMultipleToSparseVectorAndDeleteCommonIndex_LongerA) {
  SparseVector<TestIndexType> a;
  a.SetCoefficient(TestIndexType(1), -1.0);
  a.SetCoefficient(TestIndexType(6), 1.0);
  a.SetCoefficient(TestIndexType(7), 2.0);

  SparseVector<TestIndexType> b;
  b.SetCoefficient(TestIndexType(6), 1.0);

  a.AddMultipleToSparseVectorAndDeleteCommonIndex(-2.0, TestIndexType(6), 1e-12,
                                                  &b);
  EXPECT_EQ("[1]=2, [7]=-4", b.DebugString());
}

TEST(SparseVectorTest,
     AddMultipleToSparseVectorAndDeleteCommonIndex_AlmostZero) {
  SparseVector<TestIndexType> a;
  a.SetCoefficient(TestIndexType(1),
                   -4.0 / 3 + std::numeric_limits<double>::epsilon());
  a.SetCoefficient(TestIndexType(6), 4.0);

  SparseVector<TestIndexType> b;
  b.SetCoefficient(TestIndexType(1), 4.0);
  b.SetCoefficient(TestIndexType(6), 1.0);

  ASSERT_NE(0.0, b.LookUpCoefficient(TestIndexType(1)) +
                     a.LookUpCoefficient(TestIndexType(1)) * 3.0)
      << "We aren't testing an almost-zero entry, but an actual-zero entry."
      << " Please adjust the test constants so that we aren't exactly zero.";

  a.AddMultipleToSparseVectorAndDeleteCommonIndex(3.0, TestIndexType(6), 1e-12,
                                                  &b);
  EXPECT_EQ("", b.DebugString());
}

TEST(SparseVectorTest,
     AddMultipleToSparseVectorAndDeleteCommonIndex_AlmostZero2) {
  SparseVector<TestIndexType> a;
  a.SetCoefficient(TestIndexType(1), 1e-7);
  a.SetCoefficient(TestIndexType(6), 1.0);
  a.SetCoefficient(TestIndexType(7), 2.e-7);

  SparseVector<TestIndexType> b;
  b.SetCoefficient(TestIndexType(6), 1.0);

  // Since the drop tolerance is 1e-12, we loose both position 1 and 7.
  a.AddMultipleToSparseVectorAndIgnoreCommonIndex(1e-7, TestIndexType(6), 1e-12,
                                                  &b);
  EXPECT_EQ("[6]=1", b.DebugString());
}

TEST(SparseVectorTest, AddMultipleToSparseVectorAndIgnoreCommonIndex) {
  SparseVector<TestIndexType> a;
  a.SetCoefficient(TestIndexType(1), -1.0);
  a.SetCoefficient(TestIndexType(6), 1.0);
  a.SetCoefficient(TestIndexType(7), 2.0);

  SparseVector<TestIndexType> b;
  b.SetCoefficient(TestIndexType(6), 1.0);

  a.AddMultipleToSparseVectorAndIgnoreCommonIndex(-2.0, TestIndexType(6), 1e-12,
                                                  &b);
  EXPECT_EQ("[1]=2, [6]=1, [7]=-4", b.DebugString());
}

TEST(SparseVectorTest, ApplyIndexPermutationAndPermutedCopy) {
  TestIndexType kNumIndex(30);
  SparseVector<TestIndexType> sparse_vector;
  sparse_vector.SetCoefficient(TestIndexType(1), 1.0);
  sparse_vector.SetCoefficient(TestIndexType(7), 2.0);
  sparse_vector.SetCoefficient(TestIndexType(6), -1.0);
  sparse_vector.SetCoefficient(TestIndexType(19), 0.5);
  sparse_vector.SetCoefficient(TestIndexType(13), -0.25);
  sparse_vector.SetCoefficient(TestIndexType(23), -4.0);

  Permutation<TestIndexType> index_perm(kNumIndex);
  index_perm.PopulateRandomly();

  StrictITIVector<TestIndexType, Fractional> dense_vector;
  StrictITIVector<TestIndexType, Fractional> expected(kNumIndex, 0.0);
  sparse_vector.CopyToDenseVector(kNumIndex, &dense_vector);
  ApplyPermutation(index_perm, dense_vector, &expected);
  // NOTE(user): The probability of our random permutation leaving the
  // sparse_vector identical is 30!/(30-6)! ~ 2e-9, which is low enough to be
  // considered "not flaky".
  EXPECT_THAT(dense_vector, Not(ContainerEq(expected)));

  sparse_vector.PermutedCopyToDenseVector(index_perm, kNumIndex, &dense_vector);
  EXPECT_THAT(dense_vector, ContainerEq(expected));

  sparse_vector.ApplyIndexPermutation(index_perm);
  sparse_vector.CopyToDenseVector(kNumIndex, &dense_vector);
  EXPECT_THAT(dense_vector, ContainerEq(expected));
}

TEST(SparseVectorTest, ApplyPartialIndexPermutationAndIsEqualTo) {
  SparseVector<TestIndexType> a;
  a.SetCoefficient(TestIndexType(1), 1.0);
  a.SetCoefficient(TestIndexType(6), -1.0);
  a.SetCoefficient(TestIndexType(7), 2.0);
  EXPECT_TRUE(a.CheckNoDuplicates());

  SparseVector<TestIndexType> b;
  Permutation<TestIndexType> permutation(TestIndexType(100));
  permutation.PopulateFromIdentity();
  b.PopulateFromSparseVector(a);
  b.ApplyPartialIndexPermutation(permutation);
  EXPECT_TRUE(b.IsEqualTo(a));

  const TestIndexType kToRemove(-1);
  SparseVector<TestIndexType> expected;
  expected.SetCoefficient(TestIndexType(1), 1.0);
  expected.SetCoefficient(TestIndexType(6), 2.0);
  EXPECT_TRUE(expected.CheckNoDuplicates());
  permutation[TestIndexType(3)] = kToRemove;
  permutation[TestIndexType(6)] = kToRemove;
  permutation[TestIndexType(7)] = 6;
  permutation[TestIndexType(8)] = kToRemove;
  b.ApplyPartialIndexPermutation(permutation);
  EXPECT_TRUE(b.IsEqualTo(expected));
}

TEST(SparseVectorTest, MoveTaggedEntriesTo) {
  Permutation<TestIndexType> permutation(TestIndexType(100));
  permutation.PopulateFromIdentity();
  permutation[TestIndexType(1)] = -1;
  permutation[TestIndexType(6)] = -1;
  permutation[TestIndexType(7)] = -1;

  // Empty vectors.
  SparseVector<TestIndexType> a;
  SparseVector<TestIndexType> b;
  a.MoveTaggedEntriesTo(permutation, &b);
  EXPECT_TRUE(a.IsEmpty());
  EXPECT_TRUE(b.IsEmpty());

  // Nothing to move.
  a.SetCoefficient(TestIndexType(1), 1.0);
  a.SetCoefficient(TestIndexType(6), -1.0);
  a.SetCoefficient(TestIndexType(7), 2.0);
  EXPECT_TRUE(a.CheckNoDuplicates());
  a.MoveTaggedEntriesTo(permutation, &b);
  EXPECT_EQ("[1]=1, [6]=-1, [7]=2", a.DebugString());
  EXPECT_TRUE(b.IsEmpty());

  // 1 element to split.
  permutation[TestIndexType(6)] = 0;
  a.MoveTaggedEntriesTo(permutation, &b);
  EXPECT_EQ("[1]=1, [7]=2", a.DebugString());
  EXPECT_EQ("[6]=-1", b.DebugString());

  // Everything to split.
  permutation[TestIndexType(1)] = 0;
  permutation[TestIndexType(7)] = 0;
  a.MoveTaggedEntriesTo(permutation, &b);
  EXPECT_TRUE(a.IsEmpty());
  EXPECT_EQ("[6]=-1, [1]=1, [7]=2", b.DebugString());
}

TEST(SparseVectorTest, AppendEntriesWithOffset) {
  SparseVector<TestIndexType> a;
  a.SetCoefficient(TestIndexType(1), 1.0);
  a.SetCoefficient(TestIndexType(5), -1.0);
  a.SetCoefficient(TestIndexType(6), 2.0);

  SparseVector<TestIndexType> b;
  b.SetCoefficient(TestIndexType(0), 4.0);
  b.SetCoefficient(TestIndexType(3), 5.0);

  a.AppendEntriesWithOffset(b, TestIndexType(6));
  a.CleanUp();
  EXPECT_EQ("[1]=1, [5]=-1, [6]=4, [9]=5", a.DebugString());
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
