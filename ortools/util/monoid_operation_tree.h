// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_UTIL_MONOID_OPERATION_TREE_H_
#define OR_TOOLS_UTIL_MONOID_OPERATION_TREE_H_

#include <algorithm>
#include <string>

#include "absl/strings/str_format.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"

namespace operations_research {

// A monoid is an algebraic structure consisting of a set S with an associative
// binary operation * :S x S -> S that has an identity element.
// Associative means a*(b*c) = (a*b)*c for all a,b,c in S.
// An identity element is an element e in S such that for all a in S,
// e*a = a*e = a.
// See http://en.wikipedia.org/wiki/Monoid for more details.
//
// A MonoidOperationTree is a data structure that maintains a product
// a_1 * a_2 * ... * a_n for a given (fixed) n, and that supports the
// following functions:
// - Setting the k-th operand to a given value in O(log n) calls to the *
// operation
// - Querying the result in O(1)
//
// Note that the monoid is not required to be commutative.
//
// The parameter class T represents an element of the set S.
// It must:
//   * Have a public no-argument constructor producing the identity element.
//   * Have a = operator method that sets its value to the given one.
//   * Have a Compute(const T& left, const T& right) method that sets its value
//        to the result of the binary operation for the two given operands.
//   * Have a string DebugString() const method.
//
// Possible use cases are:
// * Maintain a sum or a product of doubles, with a guarantee that the queried
//       result is independent of the order of past numerical issues
// * Maintain a product of identically sized square matrices, which is an
//       example of use with non-commutative operations.
template <class T>
class MonoidOperationTree {
 public:
  // Constructs a MonoidOperationTree able to store 'size' operands.
  explicit MonoidOperationTree(int size);

  // Returns the root of the tree, containing the result of the operation.
  const T& result() const { return *result_; }

  // Resets the argument of given index.
  void Reset(int argument_index);

  // Sets the argument of given index.
  void Set(int argument_index, const T& argument);

  // Resets all arguments.
  void Clear();

  // Returns the leaf node corresponding to the given argument index.
  const T& GetOperand(int argument_index) const {
    return nodes_[PositionOfLeaf(argument_index)];
  }

  // Dive down a branch of the operation tree, and then come back up.
  template <class Diver>
  void DiveInTree(Diver* const diver) const {
    DiveInTree(0, diver);
  }

  std::string DebugString() const;

 private:
  // Computes the index of the first leaf for the given size.
  static int ComputeLeafOffset(int size);

  // Computes the total number of nodes we need to store non-leaf nodes and
  // leaf nodes.
  static int ComputeNumberOfNodes(int leaf_offset);

  // Computes the whole path from the node of given position up to the root,
  // excluding the bottom node.
  void ComputeAbove(int position);

  // Computes the node of given position, and no other.
  void Compute(int position);

  // Returns the position of the leaf node of given index.
  int PositionOfLeaf(int index) const { return leaf_offset_ + index; }

  // Returns true if the node of given position is a leaf.
  bool IsLeaf(int position) const { return position >= leaf_offset_; }

  // Returns the index of the argument stored in the node of given position.
  int ArgumentIndexOfLeafPosition(int position) const {
    DCHECK(IsLeaf(position));
    return position - leaf_offset_;
  }

  template <class Diver>
  void DiveInTree(int position, Diver* diver) const;

  static int father(int pos) { return (pos - 1) >> 1; }
  static int left(int pos) { return (pos << 1) + 1; }
  static int right(int pos) { return (pos + 1) << 1; }

  // The number of arguments that can be stored in this tree. That is, the
  // number of used leaves. (There may be unused leaves, too)
  const int size_;

  // The index of the first leaf.
  const int leaf_offset_;

  // Number of nodes, both non-leaves and leaves.
  const int num_nodes_;

  // All the nodes, both non-leaves and leaves.
  std::vector<T> nodes_;

  // A pointer to the root node
  T const* result_;

  DISALLOW_COPY_AND_ASSIGN(MonoidOperationTree);
};

// --------------------------------------------------------------------- //
//       Implementation
// --------------------------------------------------------------------- //

template <class T>
int MonoidOperationTree<T>::ComputeLeafOffset(int size) {
  int smallest_pow_two_not_less_than_size = 1;
  while (smallest_pow_two_not_less_than_size < size) {
    smallest_pow_two_not_less_than_size <<= 1;
  }
  return std::max(1, smallest_pow_two_not_less_than_size - 1);
}

template <class T>
int MonoidOperationTree<T>::ComputeNumberOfNodes(int leaf_offset) {
  // leaf_offset should be a power of 2 minus 1.
  DCHECK_EQ(0, (leaf_offset) & (leaf_offset + 1));
  const int num_leaves = leaf_offset + 1;
  const int num_nodes = leaf_offset + num_leaves;
  DCHECK_GE(num_nodes, 3);  // We need at least the root and its 2 children
  return num_nodes;
}

template <class T>
MonoidOperationTree<T>::MonoidOperationTree(int size)
    : size_(size),
      leaf_offset_(ComputeLeafOffset(size)),
      num_nodes_(ComputeNumberOfNodes(leaf_offset_)),
      nodes_(num_nodes_, T()),
      result_(&(nodes_[0])) {}

template <class T>
void MonoidOperationTree<T>::Clear() {
  const int size = nodes_.size();
  nodes_.assign(size, T());
}

template <class T>
void MonoidOperationTree<T>::Reset(int argument_index) {
  Set(argument_index, T());
}

template <class T>
void MonoidOperationTree<T>::Set(int argument_index, const T& argument) {
  CHECK_LT(argument_index, size_);
  const int position = leaf_offset_ + argument_index;
  nodes_[position] = argument;
  ComputeAbove(position);
}

template <class T>
void MonoidOperationTree<T>::ComputeAbove(int position) {
  int pos = father(position);
  while (pos > 0) {
    Compute(pos);
    pos = father(pos);
  }
  Compute(0);
}

template <class T>
void MonoidOperationTree<T>::Compute(int position) {
  const T& left_child = nodes_[left(position)];
  const T& right_child = nodes_[right(position)];
  nodes_[position].Compute(left_child, right_child);
}

template <class T>
std::string MonoidOperationTree<T>::DebugString() const {
  std::string out;
  int layer = 0;
  for (int i = 0; i < num_nodes_; ++i) {
    if (((i + 1) & i) == 0) {
      // New layer
      absl::StrAppendFormat(&out, "-------------- Layer %d ---------------\n",
                            layer);
      ++layer;
    }
    absl::StrAppendFormat(&out, "Position %d: %s\n", i,
                          nodes_[i].DebugString());
  }
  return out;
}

template <class T>
template <class Diver>
void MonoidOperationTree<T>::DiveInTree(int position, Diver* diver) const {
  // Are we at a leaf?
  if (IsLeaf(position)) {
    const int index = ArgumentIndexOfLeafPosition(position);
    const T& argument = nodes_[position];
    diver->OnArgumentReached(index, argument);
  } else {
    const T& current = nodes_[position];
    const T& left_child = nodes_[left(position)];
    const T& right_child = nodes_[right(position)];
    if (diver->ChooseGoLeft(current, left_child, right_child)) {
      // Go left
      DiveInTree(left(position), diver);
      // Come back up
      diver->OnComeBackFromLeft(current, left_child, right_child);
    } else {
      // Go right
      DiveInTree(right(position), diver);
      // Come back up
      diver->OnComeBackFromRight(current, left_child, right_child);
    }
  }
}

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_MONOID_OPERATION_TREE_H_
