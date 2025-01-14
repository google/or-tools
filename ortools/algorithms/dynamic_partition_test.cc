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

#include "ortools/algorithms/dynamic_partition.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>

#include "absl/random/bit_gen_ref.h"
#include "absl/random/random.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/stl_util.h"

namespace operations_research {
namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::StartsWith;
using ::testing::UnorderedElementsAre;

std::vector<int> GetPart(const DynamicPartition& partition, int i) {
  return std::vector<int>(partition.ElementsInPart(i).begin(),
                          partition.ElementsInPart(i).end());
}

std::vector<std::vector<int>> GetAllParts(const DynamicPartition& partition) {
  std::vector<std::vector<int>> parts;
  for (int i = 0; i < partition.NumParts(); ++i) {
    parts.push_back(GetPart(partition, i));
  }
  return parts;
}

TEST(DynamicPartitionTest, OrderAgnosticPartitioning) {
  DynamicPartition partition(5);
  ASSERT_EQ(5, partition.NumElements());
  ASSERT_EQ(1, partition.NumParts());
  EXPECT_THAT(GetPart(partition, 0), UnorderedElementsAre(0, 1, 2, 3, 4));

  partition.Refine({1, 3, 4});
  EXPECT_EQ(5, partition.NumElements());
  EXPECT_EQ(2, partition.NumParts());
  EXPECT_THAT(GetAllParts(partition),
              UnorderedElementsAre(UnorderedElementsAre(0, 2),
                                   UnorderedElementsAre(1, 3, 4)));

  partition.Refine({0, 3});
  EXPECT_THAT(
      GetAllParts(partition),
      UnorderedElementsAre(UnorderedElementsAre(0), UnorderedElementsAre(1, 4),
                           UnorderedElementsAre(2), UnorderedElementsAre(3)));

  // Corner case: no-op Refine(), on both a singleton and a non-singleton part.
  partition.Refine({0, 1, 4});
  EXPECT_THAT(
      GetAllParts(partition),
      UnorderedElementsAre(UnorderedElementsAre(0), UnorderedElementsAre(1, 4),
                           UnorderedElementsAre(2), UnorderedElementsAre(3)));

  // Corner case: Refine a singleton. Incidentally, our partition becomes
  // fully-split (only singletons), which is also another interesting corner
  // case.
  partition.Refine({4});
  EXPECT_THAT(
      GetAllParts(partition),
      UnorderedElementsAre(UnorderedElementsAre(0), UnorderedElementsAre(1),
                           UnorderedElementsAre(2), UnorderedElementsAre(3),
                           UnorderedElementsAre(4)));

  // Roll back the last 3 parts.
  partition.UndoRefineUntilNumPartsEqual(2);
  EXPECT_THAT(GetAllParts(partition),
              UnorderedElementsAre(UnorderedElementsAre(0, 2),
                                   UnorderedElementsAre(1, 3, 4)));

  // No-op rollback.
  partition.UndoRefineUntilNumPartsEqual(2);

  // Re-apply some refinement.
  partition.Refine({4});
  EXPECT_THAT(GetAllParts(partition),
              UnorderedElementsAre(UnorderedElementsAre(0, 2),
                                   UnorderedElementsAre(1, 3),
                                   UnorderedElementsAre(4)));

  // Roll back until the start.
  partition.UndoRefineUntilNumPartsEqual(1);
  EXPECT_THAT(GetAllParts(partition),
              UnorderedElementsAre(UnorderedElementsAre(0, 1, 2, 3, 4)));
}

TEST(DynamicPartitionTest, PartOrdering) {
  DynamicPartition partition(9);
  partition.Refine({4, 1, 3});
  ASSERT_THAT(GetAllParts(partition),
              ElementsAre(UnorderedElementsAre(0, 2, 5, 6, 7, 8),
                          UnorderedElementsAre(1, 3, 4)));
  partition.Refine({0, 6, 3, 5});
  ASSERT_THAT(
      GetAllParts(partition),
      ElementsAre(UnorderedElementsAre(2, 7, 8), UnorderedElementsAre(1, 4),
                  UnorderedElementsAre(0, 5, 6), UnorderedElementsAre(3)));
  partition.Refine({7, 2, 6, 1});
  ASSERT_THAT(GetAllParts(partition),
              ElementsAre(UnorderedElementsAre(8), UnorderedElementsAre(4),
                          UnorderedElementsAre(0, 5), UnorderedElementsAre(3),
                          UnorderedElementsAre(2, 7), UnorderedElementsAre(1),
                          UnorderedElementsAre(6)));
  partition.Refine({3, 7, 1, 0});
  ASSERT_THAT(GetAllParts(partition),
              ElementsAre(UnorderedElementsAre(8), UnorderedElementsAre(4),
                          UnorderedElementsAre(5), UnorderedElementsAre(3),
                          UnorderedElementsAre(2), UnorderedElementsAre(1),
                          UnorderedElementsAre(6), UnorderedElementsAre(0),
                          UnorderedElementsAre(7)));
}

TEST(DynamicPartitionTest, Accessors) {
  DynamicPartition partition(7);
  partition.Refine({2, 1, 5, 0});
  partition.Refine({2, 4, 3, 6});
  ASSERT_THAT(GetAllParts(partition), ElementsAre(UnorderedElementsAre(3, 4, 6),
                                                  UnorderedElementsAre(0, 1, 5),
                                                  UnorderedElementsAre(2)));

  // Test DebugString(true).
  EXPECT_EQ("0 1 5 | 2 | 3 4 6",
            partition.DebugString(/*sort_parts_lexicographically=*/true));

  // Test DebugString(false).
  EXPECT_EQ("3 4 6 | 0 1 5 | 2",
            partition.DebugString(/*sort_parts_lexicographically=*/false));

  // Test PartOf().
  EXPECT_EQ(1, partition.PartOf(0));
  EXPECT_EQ(1, partition.PartOf(1));
  EXPECT_EQ(2, partition.PartOf(2));
  EXPECT_EQ(0, partition.PartOf(3));
  EXPECT_EQ(0, partition.PartOf(4));
  EXPECT_EQ(1, partition.PartOf(5));
  EXPECT_EQ(0, partition.PartOf(6));

  // Test SizeOfPart().
  EXPECT_EQ(3, partition.SizeOfPart(0));
  EXPECT_EQ(3, partition.SizeOfPart(1));
  EXPECT_EQ(1, partition.SizeOfPart(2));

  // Test ParentOfPart().
  EXPECT_EQ(0, partition.ParentOfPart(0));
  EXPECT_EQ(0, partition.ParentOfPart(1));
  EXPECT_EQ(1, partition.ParentOfPart(2));
}

TEST(DynamicPartitionTest, ConstructWithEmptyPartition) {
  DynamicPartition partition(std::vector<int>(0));
  EXPECT_EQ("", partition.DebugString(/*sort_parts_lexicographically=*/false));
}

TEST(DynamicPartitionTest, ConstructWithPartition) {
  DynamicPartition partition({2, 1, 0, 1, 0, 3, 0});
  EXPECT_EQ("0 | 1 3 | 2 4 6 | 5",
            partition.DebugString(/*sort_parts_lexicographically=*/true));
  EXPECT_EQ("2 4 6 | 1 3 | 0 | 5",
            partition.DebugString(/*sort_parts_lexicographically=*/false));
}

TEST(DynamicPartitionTest, FingerprintBasic) {
  DynamicPartition p1(10);
  DynamicPartition p2(/*initial_part_of_element=*/{2, 0, 1, 0, 1, 3});
  p1.Refine({2, 4, 7});
  p1.Refine({0});
  p1.Refine({5, 7});
  p1.Refine({6, 8, 9});
  // We have to rely on all the other methods working as expected: if any of
  // the other unit tests failed, then this one probably will, too.
  ASSERT_EQ("1 3 | 2 4 | 0 | 5",
            p2.DebugString(/*sort_parts_lexicographically=*/false));
  ASSERT_THAT(p1.DebugString(/*sort_parts_lexicographically=*/false),
              StartsWith("1 3 | 2 4 | 0 | 5 |"));

  for (int p = 0; p < 3; ++p) {
    EXPECT_EQ(p1.FprintOfPart(p), p2.FprintOfPart(p));
  }
  for (int p = 0; p < p1.NumParts(); ++p) {
    for (int q = 0; q < p; ++q) {
      EXPECT_NE(p1.FprintOfPart(p), p1.FprintOfPart(q)) << "Collision!";
    }
  }
}

TEST(DynamicPartitionTest, FingerprintDoesNotDependOnElementOrderNorPartIndex) {
  DynamicPartition p1(3);
  DynamicPartition p2(3);
  p1.Refine({0});
  p2.Refine({2, 1});
  ASSERT_THAT(GetPart(p1, 0), ElementsAre(2, 1));
  ASSERT_THAT(GetPart(p2, 1), ElementsAre(1, 2));
  EXPECT_EQ(p1.FprintOfPart(0), p2.FprintOfPart(1));
}

void ShufflePartition(int num_operations, int max_num_parts_at_the_end,
                      absl::BitGenRef random, DynamicPartition* partition) {
  const int n = partition->NumElements();
  std::vector<int> elements_to_refine_on;
  for (int i = 0; i < num_operations; ++i) {
    if (absl::Bernoulli(random, 1.0 / 2)) {
      // Refine on a random set of elements.
      elements_to_refine_on.clear();
      const int num_elements_to_refine_on = absl::Uniform(random, 0, n);
      for (int j = 0; j < num_elements_to_refine_on; ++j) {
        elements_to_refine_on.push_back(absl::Uniform(random, 0, n));
      }
      gtl::STLSortAndRemoveDuplicates(&elements_to_refine_on);
      partition->Refine(elements_to_refine_on);
    } else {
      // Undo some refines.
      partition->UndoRefineUntilNumPartsEqual(
          absl::Uniform(random, 0, partition->NumParts()) + 1);
    }
  }
  // We're done shuffling. If there are too many parts; un-refine some of them.
  if (partition->NumParts() > max_num_parts_at_the_end) {
    partition->UndoRefineUntilNumPartsEqual(max_num_parts_at_the_end);
  }
}

TEST(DynamicPartitionTest, FingerprintStressTest) {
  // Stress test: create 1000 'random' partitions of {0..9} in up to 3 parts.
  // Since there are 2^10 = 1024 possible subsets of 10 elements; some of
  // the 3000 created parts should coincide; actually we expect about
  // (3000 * 2999) / 2 collisions.
  // The size are just indicative (in opt mode, we stress it a bit more).
  //
  // Timing as of 2014-04-30, on forge: fastbuild=7.5s, opt=22s.
  const int kNumPartitions = DEBUG_MODE ? 1000 : 4000;
  const int kPartitionSize = DEBUG_MODE ? 10 : 12;
  const int kMaxNumParts = 3;
  std::mt19937 random(12345);
  std::vector<std::unique_ptr<DynamicPartition>> partitions(kNumPartitions);
  for (int i = 0; i < kNumPartitions; ++i) {
    partitions[i] = std::make_unique<DynamicPartition>(kPartitionSize);
    ShufflePartition(/*num_operations=*/20, kMaxNumParts, random,
                     partitions[i].get());
  }

  // Do a pairwise comparison of all part fingerprints, and verify that all
  // collisions that we obtain are legit (i.e. the parts are actually equal).
  int num_collisions = 0;
  for (int p1 = 0; p1 < kNumPartitions; ++p1) {
    for (int i1 = 0; i1 < partitions[p1]->NumParts(); ++i1) {
      const uint64_t fprint1 = partitions[p1]->FprintOfPart(i1);
      std::vector<int> part1;
      for (const int i : partitions[p1]->ElementsInPart(i1)) part1.push_back(i);
      std::sort(part1.begin(), part1.end());
      for (int p2 = 0; p2 < p1; ++p2) {
        for (int i2 = 0; i2 < partitions[p2]->NumParts(); ++i2) {
          if (partitions[p2]->FprintOfPart(i2) == fprint1) {
            ++num_collisions;
            std::vector<int> part2;
            for (const int i : partitions[p2]->ElementsInPart(i2)) {
              part2.push_back(i);
            }
            std::sort(part2.begin(), part2.end());
            ASSERT_THAT(part2, ElementsAreArray(part1))
                << "Unexpected collision! Fingerprint=" << fprint1;
          }
        }
      }
    }
  }
  // Verify that we had roughly the expected number of collisions.
  // (if we never had zero collisions, or if we always had collisions, then
  // this test wouldn't be as "stressful" as it should be).
  EXPECT_LE(num_collisions, kNumPartitions * kNumPartitions / 4);
  EXPECT_GE(num_collisions,
            kNumPartitions * kNumPartitions / (1 << kPartitionSize));
  EXPECT_GE(kNumPartitions * kNumPartitions / (1 << kPartitionSize), 100);
}

TEST(DynamicPartitionTest, ElementsInHierarchicalOrder) {
  DynamicPartition partition(/*num_elements=*/5);
  partition.Refine({4, 3});  // Now:(0 1 2 | 3 4)
  partition.Refine({1, 4});  // Now: ((0 2 | 1) | (3 | 4))
  partition.Refine({0});     // Now: (((2 | 0) | 1) | (3 | 4))
  // The parts are sorted differently than the natural order.
  ASSERT_EQ("2 | 3 | 1 | 4 | 0",
            partition.DebugString(/*sort_parts_lexicographically=*/false));
  EXPECT_THAT(partition.ElementsInHierarchicalOrder(),
              ElementsAre(2, 0, 1, 3, 4));
  partition.UndoRefineUntilNumPartsEqual(1);
  EXPECT_THAT(partition.ElementsInHierarchicalOrder(),
              ElementsAre(2, 0, 1, 3, 4));
}

TEST(MergingPartitionTest, Empty) {
  MergingPartition partition;
  EXPECT_EQ("", partition.DebugString());
  std::vector<int> node_equivalence_classes = {345, 234, -123, 45};  // Junk.
  int num_classes = partition.FillEquivalenceClasses(&node_equivalence_classes);
  EXPECT_THAT(node_equivalence_classes, IsEmpty());
  EXPECT_EQ(0, num_classes);
}

TEST(MergingPartitionTest, Reset) {
  MergingPartition partition(4);
  partition.MergePartsOf(2, 3);
  partition.MergePartsOf(1, 0);
  partition.Reset(3);
  EXPECT_EQ("0 | 1 | 2", partition.DebugString());
}

TEST(MergingPartitionTest, EndToEnd) {
  MergingPartition partition(10);
  EXPECT_EQ(4, partition.MergePartsOf(3, 4));
  EXPECT_EQ(5, partition.MergePartsOf(3, 5));
  EXPECT_EQ(6, partition.MergePartsOf(3, 6));
  EXPECT_EQ(-1, partition.MergePartsOf(5, 3));  // Redundant.
  EXPECT_EQ(8, partition.MergePartsOf(2, 8));
  EXPECT_EQ(1, partition.MergePartsOf(2, 1));
  EXPECT_EQ(9, partition.MergePartsOf(9, 7));
  EXPECT_EQ(2, partition.MergePartsOf(1, 4));

  EXPECT_EQ("0 | 1 2 3 4 5 6 8 | 7 9", partition.DebugString());
  // Test GetRoot() without path compression.
  EXPECT_EQ(0, partition.GetRoot(0));
  EXPECT_EQ(3, partition.GetRoot(1));
  EXPECT_EQ(3, partition.GetRoot(2));
  EXPECT_EQ(3, partition.GetRoot(2));
  EXPECT_EQ(3, partition.GetRoot(3));
  EXPECT_EQ(3, partition.GetRoot(4));
  EXPECT_EQ(3, partition.GetRoot(5));
  EXPECT_EQ(3, partition.GetRoot(6));
  EXPECT_EQ(7, partition.GetRoot(7));
  EXPECT_EQ(3, partition.GetRoot(8));

  // Test GetRoot() with path compression.
  EXPECT_EQ(0, partition.GetRootAndCompressPath(0));
  EXPECT_EQ(3, partition.GetRootAndCompressPath(1));
  EXPECT_EQ(3, partition.GetRootAndCompressPath(2));
  EXPECT_EQ(3, partition.GetRootAndCompressPath(2));
  EXPECT_EQ(3, partition.GetRootAndCompressPath(3));
  EXPECT_EQ(3, partition.GetRootAndCompressPath(4));
  EXPECT_EQ(3, partition.GetRootAndCompressPath(5));
  EXPECT_EQ(3, partition.GetRootAndCompressPath(6));
  EXPECT_EQ(7, partition.GetRootAndCompressPath(7));
  EXPECT_EQ(3, partition.GetRootAndCompressPath(8));
  EXPECT_EQ(7, partition.GetRootAndCompressPath(9));

  std::vector<int> node_equivalence_classes = {345, 234, -123, 45};  // Junk.
  int num_classes = partition.FillEquivalenceClasses(&node_equivalence_classes);
  EXPECT_THAT(node_equivalence_classes,
              ElementsAre(0, 1, 1, 1, 1, 1, 1, 2, 1, 2));
  EXPECT_EQ(3, num_classes);

  std::vector<int> nodes = {0, 7, 2, 9, 4, 6, 8};
  partition.KeepOnlyOneNodePerPart(&nodes);
  EXPECT_THAT(nodes, ElementsAre(0, 7, 2));

  EXPECT_EQ(1, partition.NumNodesInSamePartAs(0));
  EXPECT_EQ(7, partition.NumNodesInSamePartAs(1));
  EXPECT_EQ(7, partition.NumNodesInSamePartAs(2));
  EXPECT_EQ(7, partition.NumNodesInSamePartAs(3));
  EXPECT_EQ(7, partition.NumNodesInSamePartAs(4));
  EXPECT_EQ(7, partition.NumNodesInSamePartAs(5));
  EXPECT_EQ(7, partition.NumNodesInSamePartAs(6));
  EXPECT_EQ(2, partition.NumNodesInSamePartAs(7));
  EXPECT_EQ(7, partition.NumNodesInSamePartAs(8));
  EXPECT_EQ(2, partition.NumNodesInSamePartAs(9));

  for (int i = 1; i <= 9; ++i) partition.ResetNode(i);
  EXPECT_EQ("0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9", partition.DebugString());
  num_classes = partition.FillEquivalenceClasses(&node_equivalence_classes);
  EXPECT_THAT(node_equivalence_classes,
              ElementsAre(0, 1, 2, 3, 4, 5, 6, 7, 8, 9));
  EXPECT_EQ(10, num_classes);
}

TEST(SimpleDynamicPartitionTest, EmptyCase) {
  SimpleDynamicPartition partition(0);
  EXPECT_EQ(partition.NumElements(), 0);
  EXPECT_EQ(partition.NumParts(), 0);

  // Do not crash.
  partition.Refine({});

  std::vector<int> buffer;
  EXPECT_TRUE(partition.GetParts(&buffer).empty());
}

TEST(SimpleDynamicPartitionTest, BasicTest) {
  SimpleDynamicPartition partition(7);
  partition.Refine({2, 1, 5, 0});
  partition.Refine({2, 4, 3, 6});

  std::vector<int> buffer;
  const std::vector<absl::Span<const int>> r = partition.GetParts(&buffer);
  ASSERT_THAT(r, ElementsAre(ElementsAre(3, 4, 6), ElementsAre(0, 1, 5),
                             ElementsAre(2)));

  // Test PartOf().
  EXPECT_EQ(1, partition.PartOf(0));
  EXPECT_EQ(1, partition.PartOf(1));
  EXPECT_EQ(2, partition.PartOf(2));
  EXPECT_EQ(0, partition.PartOf(3));
  EXPECT_EQ(0, partition.PartOf(4));
  EXPECT_EQ(1, partition.PartOf(5));
  EXPECT_EQ(0, partition.PartOf(6));

  // Test SizeOfPart().
  EXPECT_EQ(3, partition.SizeOfPart(0));
  EXPECT_EQ(3, partition.SizeOfPart(1));
  EXPECT_EQ(1, partition.SizeOfPart(2));
}

}  // namespace
}  // namespace operations_research
