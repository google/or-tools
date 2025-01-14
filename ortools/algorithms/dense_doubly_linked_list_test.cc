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

#include "ortools/algorithms/dense_doubly_linked_list.h"

#include <vector>

#include "gtest/gtest.h"

namespace operations_research {
namespace {

TEST(DenseDoublyLinkedListTest, EndToEnd) {
  DenseDoublyLinkedList list(std::vector<int>({3, 6, 4, 5, 2, 1, 0}));
  list.Remove(2);
  list.Remove(1);
  list.Remove(3);
  list.Remove(0);
  // The list that remains is: 6, 4, 5.
  EXPECT_EQ(-1, list.Prev(6));
  EXPECT_EQ(6, list.Prev(4));
  EXPECT_EQ(4, list.Prev(5));
  EXPECT_EQ(4, list.Next(6));
  EXPECT_EQ(5, list.Next(4));
  EXPECT_EQ(-1, list.Next(5));
}

// TODO(user): test the DCHECKs with death tests.

}  // namespace
}  // namespace operations_research
