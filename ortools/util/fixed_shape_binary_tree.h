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

#ifndef ORTOOLS_UTIL_FIXED_SHAPE_BINARY_TREE_H_
#define ORTOOLS_UTIL_FIXED_SHAPE_BINARY_TREE_H_

#include <algorithm>
#include <utility>

#include "absl/log/check.h"
#include "absl/numeric/bits.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {

DEFINE_STRONG_INDEX_TYPE(TreeNodeIndex);
DEFINE_STRONG_INDEX_TYPE(LeafIndex);

// An abstract representation of a binary tree that can hold integers in the
// range [0, num_leaves - 1] and has a depth of exactly
// 1+ceil(log2(num_leaves)). For example, a FixedShapeBinaryTree(5)
// can be represented by:
//                 [0, 4]
//                /     \
//              /         \
//            /             \
//         [0, 3]         [4, 4]
//         /   \           /   \
//        /     \         /     \
//    [0, 1]  [2, 3]   [4, 4]   [-1, -1]
//      / \     / \     /   \     /  \
//     0   1   2   3   4    -1  -1   -1
//
// The most common use of this class is to have a concrete binary tree by
// defining its storage like:
// StrongVector<TreeNodeIndex, Val> tree(abstract_tree.StorageSize());
//
// Besides the classical binary tree structure of left and right children, this
// class provides an API to inspect and search the intermediate nodes by their
// interval values.
class FixedShapeBinaryTree {
 public:
  explicit FixedShapeBinaryTree(LeafIndex num_leaves)
      : largest_leaf_index_(num_leaves - 1) {
    max_depth_ = absl::bit_width(
        static_cast<unsigned int>(2 * largest_leaf_index_.value() + 1));
    leave_start_index_ = 1 << (max_depth_ - 1);
  }

  int StorageSize() const { return HighestNodeIndex().value() + 1; }

  // If you want to use a different storage for intermediate nodes and leaves.
  TreeNodeIndex HighestIntermediateNodeIndex() const {
    return leave_start_index_ - 1;
  }

  TreeNodeIndex HighestNodeIndex() const { return LastLeafNode(); }

  bool IsLeaf(TreeNodeIndex node) const { return node >= leave_start_index_; }

  TreeNodeIndex Root() const { return TreeNodeIndex(1); }

  TreeNodeIndex FirstLeafNode() const {
    return TreeNodeIndex(leave_start_index_);
  }

  TreeNodeIndex LastLeafNode() const {
    return leave_start_index_ + largest_leaf_index_.value();
  }

  TreeNodeIndex LeftChild(TreeNodeIndex node) const {
    DCHECK(!IsLeaf(node));
    return TreeNodeIndex(node.value() << 1);
  }

  TreeNodeIndex RightChild(TreeNodeIndex node) const {
    DCHECK(!IsLeaf(node));
    return TreeNodeIndex(node.value() << 1) + TreeNodeIndex(1);
  }

  TreeNodeIndex Parent(TreeNodeIndex node) const {
    DCHECK_NE(node, Root());
    return TreeNodeIndex(node.value() >> 1);
  }

  TreeNodeIndex Sibling(TreeNodeIndex node) const {
    DCHECK_NE(node, Root());
    return TreeNodeIndex(node.value() ^ 1);
  }

  LeafIndex LeafValue(TreeNodeIndex node) const {
    const LeafIndex ret = LeafIndex((node - leave_start_index_).value());
    if (ret > largest_leaf_index_) {
      return LeafIndex(-1);
    }
    return ret;
  }

  // Zero for the root.
  int Depth(TreeNodeIndex node) const {
    return absl::bit_width(static_cast<unsigned int>(node.value())) - 1;
  }

  // Will return [0, num_leaves - 1] for the root, [x, x] for a leaf with x
  // and the range of all the descendants of a node for intermediate nodes.
  std::pair<LeafIndex, LeafIndex> GetInterval(TreeNodeIndex node) const {
    if (IsLeaf(node)) {
      const LeafIndex leaf_value = LeafValue(node);
      return {leaf_value, leaf_value};
    }
    const int depth = Depth(node);
    const int pos = node.value() - (1 << depth);
    const int min = pos << (max_depth_ - depth - 1);
    if (min > largest_leaf_index_) {
      return {LeafIndex(-1), LeafIndex(-1)};
    }
    const int max = ((pos + 1) << (max_depth_ - depth - 1)) - 1;
    return {LeafIndex(min),
            LeafIndex(std::min(max, largest_leaf_index_.value()))};
  }

  // Given a range of leaf indexes [first_leaf, last_leaf], return the largest
  // node in the tree associated to an interval [int_begin, int_end] that
  // satisfies:
  // - int_begin == first_leaf
  // - int_end <= last_leaf.
  // For example, GetNodeStartOfRange(0, num_leaves - 1) = Root().
  //
  // This corresponds to a starting node to do a DFS traversal (including all
  // its children) to cover all intervals fully contained in the range [begin,
  // end].
  TreeNodeIndex GetNodeStartOfRange(LeafIndex first_leaf,
                                    LeafIndex last_leaf) const {
    DCHECK_LE(first_leaf, last_leaf);
    DCHECK_GE(first_leaf, 0);
    DCHECK_LE(last_leaf, largest_leaf_index_);

    if (last_leaf == largest_leaf_index_) {
      // Since we truncate the intervals to the largest_leaf_index_, this is
      // equivalent on the full binary tree to look for the largest possible
      // value.
      last_leaf = (1 << (max_depth_ - 1)) - 1;
    }
    if (first_leaf == last_leaf) {
      return GetLeaf(first_leaf);
    }

    // To see how high we can go on the tree we need to check the two rules:
    // - we need to start at `first_leaf`, so we need to know which power of two
    //   divides `first_leaf` (odd are leaves, divisible by 2 but not by 4 are
    //   right above the leaves, etc).
    // - the interval needs to be not greater than `last_leaf - first_leaf`. If
    //   `last_leaf - first_leaf` is zero it must be a leaf, if it is one it can
    //   be one step above, etc).
    const int power_of_two_div =
        absl::countr_zero(static_cast<unsigned int>(first_leaf.value()));
    const int log2_size = absl::bit_width(static_cast<unsigned int>(
                              last_leaf.value() - first_leaf.value() + 1)) -
                          1;
    const int height = std::min(log2_size, power_of_two_div);
    const int pos = first_leaf.value() >> height;
    const int depth = max_depth_ - height - 1;
    TreeNodeIndex start;
    start = (1 << depth) + pos;
    return start;
  }

  // Given a range of values, return the largest node in the tree associated to
  // an interval [int_begin, int_end] that satisfies:
  // - int_end == first_leaf
  // - int_begin >= last_leaf.
  // For example, GetNodeEndOfRange(0, largest_leaf_index) = Root().
  //
  // This corresponds to a last node (including all its descendants) to do a DFS
  // traversal to cover all intervals fully contained in the range [begin, end].
  TreeNodeIndex GetNodeEndOfRange(LeafIndex first_leaf,
                                  LeafIndex last_leaf) const {
    DCHECK_LT(first_leaf, last_leaf);
    DCHECK_GE(first_leaf, 0);
    DCHECK_LE(last_leaf, largest_leaf_index_);

    if (first_leaf == last_leaf) {
      return GetLeaf(first_leaf);
    }

    // To see how high we can go on the tree we need to check the two rules:
    // - we need to end at `last_leaf`, so we need to know which power of two
    //   divides `last_leaf+1`.
    // - the interval needs to be not greater than `last_leaf - first_leaf`. If
    // `last_leaf - first_leaf` is zero it must be a leaf, if it is one it can
    // be one step
    //   above, etc).
    const int log2_size = absl::bit_width(static_cast<unsigned int>(
                              last_leaf.value() - first_leaf.value() + 1)) -
                          1;
    const int power_of_two_div =
        absl::countr_zero(static_cast<unsigned int>(last_leaf.value() + 1));
    const int height = std::min(log2_size, power_of_two_div);
    const int pos = last_leaf.value() >> height;
    const int depth = max_depth_ - height - 1;
    TreeNodeIndex int_end;
    int_end = (1 << depth) + pos;
    return int_end;
  }

  // Given an interval [first_leaf, last_leaf], return O(log n) ordered disjoint
  // nodes of the tree that cover the interval. The time complexity is O(log n).
  template <typename TypeWithPushBack>
  void PartitionIntervalIntoNodes(LeafIndex first_leaf, LeafIndex last_leaf,
                                  TypeWithPushBack* result) const {
    DCHECK_LE(first_leaf, last_leaf);
    TreeNodeIndex prev(0);
    TreeNodeIndex current = GetNodeStartOfRange(first_leaf, last_leaf);
    if (current == Root()) {
      result->push_back(current);
      return;
    }
    while (true) {
      const auto& [min, max] = GetInterval(current);
      if (min >= first_leaf && max <= last_leaf) {
        result->push_back(current);
        if (max == last_leaf) {
          return;
        }
        prev = current;
        current = Parent(current);
        continue;
      }
      if (prev == TreeNodeIndex(0)) {
        prev = current;
        current = Parent(current);
      } else if (prev != Root() && current == Parent(prev)) {
        // We just moved up.
        if (prev == LeftChild(current)) {
          prev = current;
          current = RightChild(current);
        } else {
          DCHECK_EQ(prev, RightChild(current));
          prev = current;
          current = Parent(current);
        }
      } else {
        // We just moved down.
        if (IsLeaf(current)) {
          prev = current;
          current = Parent(current);
        } else {
          DCHECK_EQ(prev, Parent(current));
          prev = current;
          current = LeftChild(current);
        }
      }
    }
  }

  TreeNodeIndex GetLeaf(LeafIndex value) const {
    return leave_start_index_ + value.value();
  }

 private:
  TreeNodeIndex leave_start_index_;
  LeafIndex largest_leaf_index_;
  int max_depth_;
};

}  // namespace operations_research

#endif  // ORTOOLS_UTIL_FIXED_SHAPE_BINARY_TREE_H_
