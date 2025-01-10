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

#include "ortools/algorithms/binary_indexed_tree.h"

#include "gtest/gtest.h"

namespace operations_research {
namespace {

template <typename T>
void GetAllPrefixSums(const BinaryIndexedTree<T>& tree,
                      std::vector<T>* prefix_sums) {
  int tree_size = tree.size();
  prefix_sums->clear();
  prefix_sums->reserve(tree_size + 1);
  for (int index = -1; index < tree_size; ++index) {
    prefix_sums->emplace_back(tree.GetPrefixSum(index));
  }
}

template <typename T>
class BinaryIndexedTreeTest : public ::testing::Test {};
typedef ::testing::Types<int, float> TestTypes;
TYPED_TEST_SUITE(BinaryIndexedTreeTest, TestTypes);

TYPED_TEST(BinaryIndexedTreeTest, BinaryIndexedTree) {
  BinaryIndexedTree<TypeParam> tree(5);
  std::vector<TypeParam> prefix_sum;
  tree.AddItem(1, 1);
  // {0, 1, 0, 0, 0}
  GetAllPrefixSums(tree, &prefix_sum);
  EXPECT_EQ((std::vector<TypeParam>{0, 0, 1, 1, 1, 1}), prefix_sum);
  tree.AddItem(0, 2);
  // {2, 1, 0, 0, 0}
  GetAllPrefixSums(tree, &prefix_sum);
  EXPECT_EQ((std::vector<TypeParam>{0, 2, 3, 3, 3, 3}), prefix_sum);
  tree.AddItem(2, 3);
  // {2, 1, 3, 0, 0}
  GetAllPrefixSums(tree, &prefix_sum);
  EXPECT_EQ((std::vector<TypeParam>{0, 2, 3, 6, 6, 6}), prefix_sum);
  tree.AddItem(4, 4);
  // {2, 1, 3, 0, 4}
  GetAllPrefixSums(tree, &prefix_sum);
  EXPECT_EQ((std::vector<TypeParam>{0, 2, 3, 6, 6, 10}), prefix_sum);
  tree.AddItem(3, 5);
  // {2, 1, 3, 5, 4}
  GetAllPrefixSums(tree, &prefix_sum);
  EXPECT_EQ((std::vector<TypeParam>{0, 2, 3, 6, 11, 15}), prefix_sum);
}

}  // namespace
}  // namespace operations_research
