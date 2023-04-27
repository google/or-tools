// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_ALGORITHMS_BINARY_INDEXED_TREE_H_
#define OR_TOOLS_ALGORITHMS_BINARY_INDEXED_TREE_H_

#include <vector>

#include "absl/log/check.h"

namespace operations_research {

// A binary indexed tree is a data structure that represents an array of
// numbers and supports two operations:
// 1) add a number to i-th element of the array.
// 2) find the sum of a prefix of the array (elements from 0-th to j-th).
// See http://en.wikipedia.org/wiki/Fenwick_tree.
template <typename T>
class BinaryIndexedTree {
 public:
  // Initializes the storage for a binary indexed tree of a given size. The
  // tree contains all zeros initially.
  explicit BinaryIndexedTree(int n) : tree_(n + 1, 0) {}

  // Adds value to index-th number.
  void AddItem(int index, T value) {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, tree_.size() - 1);
    // Internal indices of BinaryIndexedTree are 1 based.
    ++index;
    while (index < tree_.size()) {
      tree_[index] += value;
      index += index & -index;
    }
  }

  // Finds the sum of a prefix [0, index]. Passing index == -1 is allowed,
  // will return 0 in that case.
  T GetPrefixSum(int index) const {
    DCHECK_GE(index, -1);
    DCHECK_LT(index + 1, tree_.size());
    // Internal indices of BinaryIndexedTree are 1 based.
    ++index;
    T prefix_sum = 0;
    while (index > 0) {
      prefix_sum += tree_[index];
      index -= index & -index;
    }
    return prefix_sum;
  }

  // Returns the size of the tree.
  int size() const { return tree_.size() - 1; }

 private:
  std::vector<T> tree_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_ALGORITHMS_BINARY_INDEXED_TREE_H_
