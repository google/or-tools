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

#include "ortools/graph_base/hash_or_tree_container.h"

#include <functional>
#include <string>
#include <type_traits>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "gtest/gtest.h"

namespace util {
namespace graph {
namespace {

TEST(HashOrTreeContainerTest, DefaultSelectsHash) {
  static_assert(
      std::is_same_v<HashOrTreeContainer<int>::Set, absl::flat_hash_set<int>>);
  static_assert(std::is_same_v<HashOrTreeContainer<int>::MapInt,
                               absl::flat_hash_map<int, int>>);
  static_assert(std::is_same_v<HashOrTreeContainer<std::string>::Set,
                               absl::flat_hash_set<std::string>>);
  static_assert(std::is_same_v<HashOrTreeContainer<std::string>::MapInt,
                               absl::flat_hash_map<std::string, int>>);
}

struct NonHashable {
  int x;
  bool operator<(const NonHashable& other) const { return x < other.x; }
};

TEST(HashOrTreeContainerTest, DefaultSelectsBtreeForNonHashable) {
  static_assert(std::is_same_v<HashOrTreeContainer<NonHashable>::Set,
                               absl::btree_set<NonHashable>>);
  static_assert(std::is_same_v<HashOrTreeContainer<NonHashable>::MapInt,
                               absl::btree_map<NonHashable, int>>);
}

TEST(HashOrTreeContainerTest, ExplicitlySelectsHash) {
  static_assert(
      std::is_same_v<HashOrTreeContainer<char, absl::Hash<char>, void>::MapInt,
                     absl::flat_hash_map<char, int>>);
}

TEST(HashOrTreeContainerTest, ExplicitlySelectsBtree) {
  static_assert(std::is_same_v<
                HashOrTreeContainer<std::vector<float>,
                                    std::less<std::vector<float>>, void>::Set,
                absl::btree_set<std::vector<float>>>);
  static_assert(std::is_same_v<
                HashOrTreeContainer<std::vector<int>,
                                    std::less<std::vector<int>>, void>::MapInt,
                absl::btree_map<std::vector<int>, int>>);
}

}  // namespace
}  // namespace graph
}  // namespace util
