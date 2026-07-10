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

#include "ortools/graph_base/index.h"

#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace util {
namespace graph {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;

class SomeClass {};
static_assert(std::is_same_v<Index<SomeClass>::value_type, SomeClass>);

TEST(IndexTest, BasicOperationsHashable) {
  Index<std::string> map;
  EXPECT_EQ(map.size(), 0);
  EXPECT_THAT(map.span(), IsEmpty());

  // Test LookupOrAdd.
  EXPECT_EQ(map.LookupOrAdd("apple"), 0);
  EXPECT_EQ(map.LookupOrAdd("banana"), 1);
  EXPECT_EQ(map.LookupOrAdd("apple"), 0);  // duplicate
  EXPECT_THAT(map.TryInsert("apple"), Pair(0, false));
  EXPECT_EQ(map.size(), 2);
  EXPECT_THAT(map.span(), ElementsAre("apple", "banana"));

  // Test Get.
  EXPECT_EQ(map[0], "apple");
  EXPECT_EQ(map[1], "banana");

  // Test Lookup.
  EXPECT_EQ(map.Lookup("apple"), 0);
  EXPECT_EQ(map.Lookup("banana"), 1);
  EXPECT_EQ(map.Lookup("cherry"), std::nullopt);
  EXPECT_EQ(map.size(), 2);
  EXPECT_THAT(map.span(), ElementsAre("apple", "banana"));

  map.clear();
  EXPECT_EQ(map.size(), 0);
  EXPECT_THAT(map, ElementsAre());
  EXPECT_EQ(map.Lookup("apple"), std::nullopt);
}

TEST(IndexTest, MoveOperationsHashable) {
  Index<std::string> map;
  EXPECT_THAT(map.TryInsert("apple"), Pair(0, true));
  EXPECT_EQ(map.LookupOrAdd("banana"), 1);

  // Move constructor
  Index<std::string> map2 = std::move(map);

  // The old map is moved-from. The new map should work correctly with Hash and
  // Eq pointers updated.
  EXPECT_EQ(map2[0], "apple");
  EXPECT_EQ(map2[1], "banana");
  EXPECT_EQ(map2.Lookup("apple"), 0);
  EXPECT_EQ(map2.Lookup("banana"), 1);
  EXPECT_EQ(map2.Lookup("cherry"), std::nullopt);

  // LookupOrAdd on the moved-to map should still work and detect duplicates
  // correctly.
  EXPECT_EQ(map2.LookupOrAdd("cherry"), 2);
  EXPECT_EQ(map2.LookupOrAdd("apple"), 0);
  EXPECT_EQ(map2.Lookup("cherry"), 2);

  // Move assignment
  Index<std::string> map3;
  map3 = std::move(map2);

  EXPECT_EQ(map3.size(), 3);
  EXPECT_THAT(map3.span(), ElementsAre("apple", "banana", "cherry"));
  EXPECT_THAT(map3, ElementsAre("apple", "banana", "cherry"));
  EXPECT_EQ(map3[0], "apple");
  EXPECT_EQ(map3[1], "banana");
  EXPECT_EQ(map3[2], "cherry");
  EXPECT_EQ(map3.Lookup("apple"), 0);
  EXPECT_EQ(map3.Lookup("banana"), 1);
  EXPECT_EQ(map3.Lookup("cherry"), 2);
  EXPECT_EQ(map3.LookupOrAdd("banana"), 1);

  // Verify that vector() does a move, by comparing the pointer of an object
  // before and after the move.
  const std::string* cherry_ptr = &map3.span()[2];
  std::vector<std::string> objects = std::move(map3).Extract();
  EXPECT_EQ(&objects[2], cherry_ptr);
}

// No eq, no hash, but comparable (via operator<).
struct NonHashable {
  int x;
  bool operator<(const NonHashable& other) const { return x < other.x; }
};

TEST(IndexTest, BasicOperationsNonHashable) {
  Index<NonHashable> map;
  EXPECT_EQ(map.size(), 0);
  EXPECT_THAT(map.span(), IsEmpty());

  // Test LookupOrAdd.
  EXPECT_EQ(map.LookupOrAdd(NonHashable{1}), 0);
  EXPECT_EQ(map.LookupOrAdd(NonHashable{-1}), 1);
  EXPECT_THAT(map.TryInsert(NonHashable{1}), Pair(0, false));  // duplicate
  EXPECT_EQ(map.size(), 2);
  EXPECT_EQ(map.span()[0].x, 1);
  EXPECT_EQ(map.span()[1].x, -1);

  EXPECT_EQ(map.Lookup(NonHashable{2}), std::nullopt);
  EXPECT_EQ(map.Lookup(NonHashable{-1}), 1);
}

TEST(IndexTest, MoveOperationsNonHashable) {
  Index<NonHashable> map;
  EXPECT_THAT(map.TryInsert(NonHashable{1}), Pair(0, true));
  map.LookupOrAdd(NonHashable{-1});

  // Move constructor
  Index<NonHashable> map2 = std::move(map);
  Index<NonHashable> map3;
  map3 = std::move(map2);

  EXPECT_EQ(map3[0].x, 1);
  EXPECT_EQ(map3[1].x, -1);
  EXPECT_EQ(map3.Lookup(NonHashable{1}), 0);
  EXPECT_EQ(map3.Lookup(NonHashable{-1}), 1);
  EXPECT_EQ(map3.Lookup(NonHashable{0}), std::nullopt);
}

TEST(IndexTest, CustomHashAndEq) {
  // Represents an integer modulo 3 (mathematically, e.g. -5 ≡ 1 (mod 3))
  struct T {
    int x;
  };
  static auto math_mod_3 = [](const T& t) -> int { return (t.x % 3 + 3) % 3; };
  struct Hash {
    size_t operator()(const T& t) const { return math_mod_3(t) * 13; }
  };
  struct Eq {
    bool operator()(const T& a, const T& b) const {
      return math_mod_3(a) == math_mod_3(b);
    }
  };
  Index<T, Hash, Eq> vs;
  EXPECT_THAT(vs.TryInsert(T{3}), Pair(0, true));
  EXPECT_EQ(vs.Lookup(T{1}), std::nullopt);
  EXPECT_EQ(vs.LookupOrAdd(T{1}), 1);
  EXPECT_EQ(vs.LookupOrAdd(T{5}), 2);
  EXPECT_THAT(vs.TryInsert(T{9}), Pair(0, false));
  Index<T, Hash, Eq> vs2 = std::move(vs);
  Index<T, Hash, Eq> vs3;
  vs3 = std::move(vs2);
  EXPECT_EQ(vs3.size(), 3);
  EXPECT_EQ(vs3[0].x, 3);
  EXPECT_EQ(vs3[1].x, 1);
  EXPECT_EQ(vs3[2].x, 5);
  EXPECT_EQ(vs3.LookupOrAdd(T{-62}), 1);
  EXPECT_EQ(vs3.Lookup(T{99}), 0);
}

TEST(IndexTest, CustomComparatorNoEq) {
  // Represents an integer modulo 7 (mathematically, e.g. -5 ≡ 1 (mod 3)).
  // Smaller means closer to 0 mod 7.
  struct T {
    int x;
  };
  static auto math_mod_7 = [](const T& t) -> int { return (t.x % 7 + 7) % 7; };
  struct Compare {
    size_t operator()(const T& a, const T& b) const {
      return math_mod_7(a) < math_mod_7(b);
    }
  };
  Index<T, Compare> vs;
  EXPECT_EQ(vs.LookupOrAdd(T{3}), 0);
  EXPECT_EQ(vs.Lookup(T{1}), std::nullopt);
  EXPECT_EQ(vs.LookupOrAdd(T{8}), 1);
  EXPECT_EQ(vs.LookupOrAdd(T{5}), 2);
  Index<T, Compare> vs2 = std::move(vs);
  Index<T, Compare> vs3;
  vs3 = std::move(vs2);
  EXPECT_EQ(vs3.size(), 3);
  EXPECT_EQ(vs3[0].x, 3);
  EXPECT_EQ(vs3[1].x, 8);
  EXPECT_EQ(vs3[2].x, 5);
  EXPECT_EQ(vs3.LookupOrAdd(T{-62}), 1);
  EXPECT_EQ(vs3.Lookup(T{99}), 1);
}

TEST(IndexTest, NonDefaultConstructibleHashAndEq) {
  // Represents an integer modulo M (mathematically, e.g. -5 ≡ 1 (mod 3))
  struct T {
    int x;
  };
  static auto math_mod = [](const T& t, int mod) -> int {
    return (t.x % mod + mod) % mod;
  };
  struct Hash {
    explicit Hash(int _mod) : mod(_mod) {}
    size_t operator()(const T& t) const { return math_mod(t, mod) * 13; }
    const int mod;
  };
  struct Eq {
    explicit Eq(int mod) : mod(mod) {}
    bool operator()(const T& a, const T& b) const {
      return math_mod(a, mod) == math_mod(b, mod);
    }
    const int mod;
  };
  Index<T, Hash, Eq> vs(0, Hash(3), Eq(3));
  EXPECT_EQ(vs.LookupOrAdd(T{3}), 0);
  EXPECT_EQ(vs.Lookup(T{1}), std::nullopt);
  EXPECT_EQ(vs.LookupOrAdd(T{1}), 1);
  EXPECT_EQ(vs.LookupOrAdd(T{5}), 2);
  EXPECT_EQ(vs.LookupOrAdd(T{0}), 0);
  Index<T, Hash, Eq> vs2 = std::move(vs);
  Index<T, Hash, Eq> vs3(0, Hash(2), Eq(2));
  vs3 = std::move(vs2);
  EXPECT_EQ(vs3.size(), 3);
  EXPECT_EQ(vs3[0].x, 3);
  EXPECT_EQ(vs3[1].x, 1);
  EXPECT_EQ(vs3[2].x, 5);
  EXPECT_EQ(vs3.LookupOrAdd(T{-62}), 1);
  EXPECT_EQ(vs3.Lookup(T{99}), 0);
}

TEST(IndexTest, ComparatorFallback) {
  Index<std::vector<int>> map;
  EXPECT_EQ(map.size(), 0);

  EXPECT_EQ(map.LookupOrAdd({1, 2}), 0);
  EXPECT_EQ(map.LookupOrAdd({3, 4}), 1);
  EXPECT_EQ(map.LookupOrAdd({1, 2}), 0);

  EXPECT_EQ(map.size(), 2);
  EXPECT_EQ(map.Lookup({1, 2}), 0);
  EXPECT_EQ(map.Lookup({3, 4}), 1);
  EXPECT_EQ(map.Lookup({5}), std::nullopt);
}

}  // namespace
}  // namespace graph
}  // namespace util
