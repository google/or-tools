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

#include "ortools/util/fixed_shape_binary_tree.h"

#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/numeric/bits.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_vector.h"

namespace operations_research {
namespace {

using ::testing::ElementsAre;

auto MakeInterval(int a, int b) {
  return std::make_pair(LeafIndex(a), LeafIndex(b));
}

//                 [0, 7]
//                /     \
//              /         \
//            /             \
//         [0, 3]         [4, 7]
//          / \             / \
//         /   \           /   \
//        /     \         /     \
//    [0, 1]  [2, 3]   [4, 5]  [6, 7]
//      / \     / \     / \     / \
//     0   1   2   3   4   5   6   7
TEST(FixedShapeBinaryTreeTest, Basic) {
  const FixedShapeBinaryTree tree(LeafIndex(8));
  const TreeNodeIndex root = tree.Root();
  EXPECT_FALSE(tree.IsLeaf(root));
  EXPECT_EQ(tree.Depth(root), 0);
  EXPECT_EQ(tree.GetInterval(root), MakeInterval(0, 7));
  const TreeNodeIndex left_0_3 = tree.LeftChild(root);
  const TreeNodeIndex right_4_7 = tree.RightChild(root);
  EXPECT_EQ(tree.GetInterval(left_0_3), MakeInterval(0, 3));
  EXPECT_EQ(tree.GetInterval(right_4_7), MakeInterval(4, 7));
  EXPECT_FALSE(tree.IsLeaf(left_0_3));
  EXPECT_FALSE(tree.IsLeaf(right_4_7));
  EXPECT_EQ(tree.Sibling(left_0_3), right_4_7);
  EXPECT_EQ(tree.Parent(left_0_3), root);
  EXPECT_EQ(tree.Depth(left_0_3), 1);
  EXPECT_EQ(tree.Depth(right_4_7), 1);
  const TreeNodeIndex left_0_1 = tree.LeftChild(left_0_3);
  EXPECT_EQ(tree.GetInterval(left_0_1), MakeInterval(0, 1));
  EXPECT_EQ(tree.Depth(left_0_1), 2);
  EXPECT_FALSE(tree.IsLeaf(left_0_1));
  const TreeNodeIndex leaf_0 = tree.LeftChild(left_0_1);
  EXPECT_EQ(tree.Depth(leaf_0), 3);
  EXPECT_TRUE(tree.IsLeaf(leaf_0));
  EXPECT_EQ(tree.GetInterval(leaf_0), MakeInterval(0, 0));
  EXPECT_EQ(tree.GetLeaf(LeafIndex(0)), leaf_0);
  EXPECT_EQ(tree.Parent(leaf_0), left_0_1);

  EXPECT_EQ(
      tree.GetInterval(tree.GetNodeStartOfRange(LeafIndex(4), LeafIndex(6))),
      MakeInterval(4, 5));
  EXPECT_EQ(
      tree.GetInterval(tree.GetNodeStartOfRange(LeafIndex(4), LeafIndex(5))),
      MakeInterval(4, 5));
  EXPECT_EQ(
      tree.GetInterval(tree.GetNodeEndOfRange(LeafIndex(2), LeafIndex(5))),
      MakeInterval(4, 5));

  std::vector<LeafIndex> values;
  for (TreeNodeIndex i = tree.FirstLeafNode(); i <= tree.LastLeafNode();
       i += TreeNodeIndex(1)) {
    values.push_back(tree.LeafValue(i));
  }
  EXPECT_THAT(values, ElementsAre(0, 1, 2, 3, 4, 5, 6, 7));
  std::vector<TreeNodeIndex> nodes;
  tree.PartitionIntervalIntoNodes(LeafIndex(3), LeafIndex(6), &nodes);
  EXPECT_EQ(nodes.size(), 3);
  EXPECT_THAT(tree.GetInterval(nodes[0]), MakeInterval(3, 3));
  EXPECT_THAT(tree.GetInterval(nodes[1]), MakeInterval(4, 5));
  EXPECT_THAT(tree.GetInterval(nodes[2]), MakeInterval(6, 6));
}

//                 [0, 4]
//                /     \
//              /         \
//            /             \
//         [0, 3]         [4, 4]
//          / \             / \
//         /   \           /   \
//        /     \         /     \
//    [0, 1]  [2, 3]   [4, 4]   [-1, -1]
//      / \     / \     /   \     / \
//     0   1   2   3   4    -1  -1  -1
TEST(FixedShapeBinaryTreeTest, Basic2) {
  const FixedShapeBinaryTree tree(LeafIndex(5));
  const TreeNodeIndex root = tree.Root();
  EXPECT_FALSE(tree.IsLeaf(root));
  EXPECT_EQ(tree.Depth(root), 0);
  EXPECT_EQ(tree.GetInterval(root), MakeInterval(0, 4));
  const TreeNodeIndex left_0_3 = tree.LeftChild(root);
  const TreeNodeIndex right_4_4 = tree.RightChild(root);
  EXPECT_EQ(tree.GetInterval(left_0_3), MakeInterval(0, 3));
  EXPECT_EQ(tree.GetInterval(right_4_4), MakeInterval(4, 4));
  EXPECT_FALSE(tree.IsLeaf(left_0_3));
  EXPECT_FALSE(tree.IsLeaf(right_4_4));
  EXPECT_EQ(tree.Sibling(left_0_3), right_4_4);
  EXPECT_EQ(tree.Parent(right_4_4), root);
  EXPECT_EQ(tree.Depth(right_4_4), 1);
  const TreeNodeIndex left_4_4_again = tree.LeftChild(right_4_4);
  const TreeNodeIndex right_empty = tree.RightChild(right_4_4);

  // We could check the interval to be non-negative to stop a needless branch on
  // a DFS...
  EXPECT_EQ(tree.GetInterval(right_empty), MakeInterval(-1, -1));
  EXPECT_EQ(tree.GetInterval(left_4_4_again), MakeInterval(4, 4));
  EXPECT_EQ(tree.Depth(left_4_4_again), 2);
  EXPECT_FALSE(tree.IsLeaf(left_4_4_again));
  const TreeNodeIndex leaf_4 = tree.LeftChild(left_4_4_again);
  const TreeNodeIndex empty_leaf = tree.RightChild(left_4_4_again);
  EXPECT_TRUE(tree.IsLeaf(empty_leaf));
  EXPECT_EQ(tree.LeafValue(empty_leaf), -1);
  EXPECT_EQ(tree.Depth(leaf_4), 3);
  EXPECT_TRUE(tree.IsLeaf(leaf_4));
  EXPECT_EQ(tree.GetInterval(leaf_4), MakeInterval(4, 4));
  EXPECT_EQ(tree.GetLeaf(LeafIndex(4)), leaf_4);
  EXPECT_EQ(tree.Parent(leaf_4), left_4_4_again);

  std::vector<LeafIndex> values;
  for (TreeNodeIndex i = tree.FirstLeafNode(); i <= tree.LastLeafNode();
       i += TreeNodeIndex(1)) {
    values.push_back(tree.LeafValue(i));
  }
  EXPECT_THAT(values, ElementsAre(0, 1, 2, 3, 4));
}

TEST(FixedShapeBinaryTreeTest, Basic3) {
  const FixedShapeBinaryTree tree(LeafIndex(13));
  const TreeNodeIndex root = tree.Root();
  EXPECT_EQ(tree.GetInterval(root), MakeInterval(0, 12));
  const TreeNodeIndex left_0_7 = tree.LeftChild(root);
  const TreeNodeIndex right_8_12 = tree.RightChild(root);
  EXPECT_EQ(tree.GetInterval(left_0_7), MakeInterval(0, 7));
  EXPECT_EQ(tree.GetInterval(right_8_12), MakeInterval(8, 12));
}

void CheckWithDFS(TreeNodeIndex current, LeafIndex branch_min,
                  LeafIndex branch_max, const FixedShapeBinaryTree& tree,
                  util_intops::StrongVector<LeafIndex, bool>& seen_leaves) {
  std::pair<LeafIndex, LeafIndex> interval = tree.GetInterval(current);
  CHECK_EQ(interval.first == -1, interval.second == -1);
  if (branch_min == -1) {
    // Child of an empty branch needs to be empty.
    CHECK_EQ(interval.first, -1);
    CHECK_EQ(interval.second, -1);
  }
  if (interval.first != -1) {
    // If we didn't land on an empty branch its interval should match the
    // expectations.
    CHECK_LE(interval.first, interval.second);
    CHECK_LE(interval.second, branch_max);
    CHECK_GE(interval.first, branch_min);
  }
  if (current != tree.Root()) {
    TreeNodeIndex parent = tree.Parent(current);
    CHECK_EQ(tree.Depth(parent) + 1, tree.Depth(current));
    CHECK(tree.LeftChild(parent) == current ||
          tree.RightChild(parent) == current);
  }
  if (tree.IsLeaf(current)) {
    const LeafIndex leaf = tree.LeafValue(current);
    CHECK_EQ(leaf, interval.first);
    CHECK_EQ(leaf, interval.second);
    if (leaf != -1) {
      CHECK(!seen_leaves[leaf]);
      seen_leaves[leaf] = true;
    }
  } else {
    if (interval.first == -1) {
      branch_min = -1;
      branch_max = -1;
    }
    CheckWithDFS(tree.LeftChild(current), branch_min, interval.second, tree,
                 seen_leaves);
    CheckWithDFS(tree.RightChild(current), interval.first, branch_max, tree,
                 seen_leaves);
  }
}

void CheckIntervalSearch(LeafIndex max_tree_value,
                         const FixedShapeBinaryTree& tree) {
  const int log_size =
      absl::bit_width(static_cast<unsigned int>(max_tree_value.value() + 1));
  std::vector<TreeNodeIndex> nodes;
  for (LeafIndex i = LeafIndex(0); i < max_tree_value; i += LeafIndex(1)) {
    for (LeafIndex j = i + 1; j <= max_tree_value; j += LeafIndex(1)) {
      const TreeNodeIndex start_of_range = tree.GetNodeStartOfRange(i, j);
      const TreeNodeIndex end_of_range = tree.GetNodeEndOfRange(i, j);

      // Check it fulfills the contract.
      EXPECT_EQ(tree.GetInterval(start_of_range).first, i);
      EXPECT_LE(tree.GetInterval(start_of_range).second, j);
      CHECK_EQ(tree.GetInterval(end_of_range).second, j);
      CHECK_GE(tree.GetInterval(end_of_range).first, i);
      // Check that the parent does not fulfill the contract, otherwise it
      // didn't returned the greatest interval that does.
      if (start_of_range != tree.Root()) {
        const std::pair<LeafIndex, LeafIndex> parent_interval =
            tree.GetInterval(tree.Parent(start_of_range));
        CHECK(parent_interval.first != i || parent_interval.second > j);
      }
      if (end_of_range != tree.Root()) {
        const std::pair<LeafIndex, LeafIndex> parent_interval =
            tree.GetInterval(tree.Parent(end_of_range));
        if (parent_interval != tree.GetInterval(end_of_range)) {
          CHECK(parent_interval.second != j || parent_interval.first < i);
        }
      }

      nodes.clear();
      tree.PartitionIntervalIntoNodes(i, j, &nodes);
      CHECK(!nodes.empty());
      CHECK_EQ(tree.GetInterval(nodes.front()).first, i);
      CHECK_EQ(tree.GetInterval(nodes.back()).second, j);
      CHECK_LE(nodes.size(), 2 * log_size);
      for (int k = 1; k < nodes.size(); ++k) {
        CHECK_EQ(tree.GetInterval(nodes[k - 1]).second + 1,
                 tree.GetInterval(nodes[k]).first);
      }
    }
  }
}

TEST(FixedShapeBinaryTreeTest, ExhaustiveTest) {
  constexpr LeafIndex kMaxTreeSize = LeafIndex(1000);
  util_intops::StrongVector<LeafIndex, bool> seen_leaves;
  seen_leaves.reserve(kMaxTreeSize + 1);
  for (LeafIndex max_tree_value = LeafIndex(1); max_tree_value <= kMaxTreeSize;
       max_tree_value += 10) {
    const FixedShapeBinaryTree tree(max_tree_value + 1);
    const TreeNodeIndex root = tree.Root();
    seen_leaves.clear();
    seen_leaves.resize(max_tree_value + 1);
    CheckWithDFS(root, LeafIndex(0), max_tree_value, tree, seen_leaves);
    for (LeafIndex i = LeafIndex(0); i <= max_tree_value; i += LeafIndex(1)) {
      CHECK(seen_leaves[i]);
    }
    CheckIntervalSearch(max_tree_value, tree);
  }
}

}  // namespace
}  // namespace operations_research
