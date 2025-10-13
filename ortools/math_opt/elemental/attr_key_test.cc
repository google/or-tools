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

#include "ortools/math_opt/elemental/attr_key.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash_testing.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/elemental/elements.h"
#include "ortools/math_opt/elemental/symmetry.h"
#include "ortools/math_opt/elemental/testing.h"
#include "ortools/math_opt/testing/stream.h"

namespace operations_research::math_opt {
namespace {
using testing::ElementsAre;
using testing::HasSubstr;
using testing::IsEmpty;
using testing::Pair;
using testing::SizeIs;
using testing::UnorderedElementsAre;
using testing::status::IsOkAndHolds;
using testing::status::StatusIs;

static_assert(sizeof(AttrKey<0>) <= sizeof(uint64_t));
static_assert(sizeof(AttrKey<1>) == sizeof(uint64_t));
static_assert(sizeof(AttrKey<2>) == 2 * sizeof(uint64_t));
static_assert(sizeof(AttrKey<2, ElementSymmetry<0, 1>>) ==
              2 * sizeof(uint64_t));

// Make sure that passing AttrKey by value really puts it in registers rather
// than leaving it in the caller's frame (see
// https://itanium-cxx-abi.github.io/cxx-abi/abi.html#non-trivial).
static_assert(absl::is_trivially_relocatable<AttrKey<0>>());
static_assert(absl::is_trivially_relocatable<AttrKey<1>>());
static_assert(absl::is_trivially_relocatable<AttrKey<2>>());
static_assert(
    absl::is_trivially_relocatable<AttrKey<2, ElementSymmetry<0, 1>>>());

TEST(AttrKeyTest, CtorAndIteration) {
  EXPECT_THAT(AttrKey(), ElementsAre());
  EXPECT_THAT(AttrKey(1), ElementsAre(1));
  EXPECT_THAT(AttrKey(1, 2), ElementsAre(1, 2));
}

TEST(AttrKeyTest, ElementIdCtor) {
  EXPECT_THAT(AttrKey(ElementId<ElementType::kVariable>(1)), ElementsAre(1));
  EXPECT_THAT(AttrKey(ElementId<ElementType::kVariable>(1),
                      ElementId<ElementType::kLinearConstraint>(2)),
              ElementsAre(1, 2));
}

TEST(AttrKeyTest, ElementAccess) {
  const AttrKey key(1, 2);
  EXPECT_EQ(key[0], 1);
  EXPECT_EQ(key[1], 2);

  AttrKey mutable_key(1, 2);
  EXPECT_EQ(mutable_key[0], 1);
  EXPECT_EQ(mutable_key[1], 2);
}

TEST(AttrKeyTest, SupportsAbslHash1) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      AttrKey(1),
      AttrKey(2),
      AttrKey(0),
  }));
}

TEST(AttrKeyTest, SupportsAbslHash2) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      AttrKey(1, 2),
      AttrKey(2, 3),
      AttrKey(0, 0),
  }));
}

TEST(AttrKeyTest, Stringify) {
  EXPECT_EQ(absl::StrCat(AttrKey(1, 2, 3)), "AttrKey(1, 2, 3)");
  EXPECT_EQ(StreamToString(AttrKey(1, 2, 3)), "AttrKey(1, 2, 3)");
}

TEST(AttrKeyTest, AddRemove) {
  const AttrKey key0;
  EXPECT_THAT(key0, ElementsAre());
  const AttrKey key1 = key0.AddElement<0, NoSymmetry>(3);
  EXPECT_THAT(key1, ElementsAre(3));
  const AttrKey key2 = key1.AddElement<0, NoSymmetry>(1);
  EXPECT_THAT(key2, ElementsAre(1, 3));
  const AttrKey key3 = key2.AddElement<1, NoSymmetry>(2);
  EXPECT_THAT(key3, ElementsAre(1, 2, 3));
  const AttrKey key4 = key3.AddElement<3, NoSymmetry>(4);
  EXPECT_THAT(key4, ElementsAre(1, 2, 3, 4));
}

TEST(AttrKeyTest, AddRemoveNotSymmetric) {
  using NoSym = NoSymmetry;
  EXPECT_THAT((AttrKey(0, 2).AddElement<1, NoSym>(1)), ElementsAre(0, 1, 2));
  EXPECT_THAT((AttrKey(0, 1).AddElement<2, NoSym>(2)), ElementsAre(0, 1, 2));
  EXPECT_THAT((AttrKey(0, 1).AddElement<1, NoSym>(2)), ElementsAre(0, 2, 1));
  EXPECT_THAT((AttrKey(0, 2).AddElement<2, NoSym>(1)), ElementsAre(0, 2, 1));
}

TEST(AttrKeyDeathTest, AddRemoveSymmetric) {
  using Sym01 = ElementSymmetry<1, 2>;
  EXPECT_THAT((AttrKey(0, 2).AddElement<1, Sym01>(1)), ElementsAre(0, 1, 2));
  EXPECT_THAT((AttrKey(0, 1).AddElement<2, Sym01>(2)), ElementsAre(0, 1, 2));
#ifndef NDEBUG
  EXPECT_DEATH(
      (AttrKey(0, 1).AddElement<1, Sym01>(2)),
      HasSubstr(
          "AttrKey(0, 2, 1) does not have `ElementSymmetry<1, 2>` symmetry"));
  EXPECT_DEATH(
      (AttrKey(0, 2).AddElement<2, Sym01>(1)),
      HasSubstr(
          "AttrKey(0, 2, 1) does not have `ElementSymmetry<1, 2>` symmetry"));
#endif
}

TEST(AttrKeyTest, ComparisonOperators) {
  // a[0] < a[1] < a[2] < a[3] < a[4]
  const std::vector<AttrKey<4>> a = {AttrKey(1, 0, 0, 0), AttrKey(2, 5, 1, 12),
                                     AttrKey(2, 5, 3, 10), AttrKey(2, 5, 3, 11),
                                     AttrKey(3, 0, 0, 0)};

  // Now test each of the operators
  for (int i = 0; i < a.size(); ++i) {
    SCOPED_TRACE(absl::StrCat(i));
    for (int j = 0; j < a.size(); ++j) {
      SCOPED_TRACE(absl::StrCat(j));
      if (i == j) {
        EXPECT_FALSE(a[i] < a[j]);
        EXPECT_TRUE(a[i] <= a[j]);
        EXPECT_TRUE(a[i] == a[j]);
        EXPECT_TRUE(a[i] >= a[j]);
        EXPECT_FALSE(a[i] > a[j]);
      } else if (i < j) {
        EXPECT_TRUE(a[i] < a[j]);
        EXPECT_TRUE(a[i] <= a[j]);
        EXPECT_FALSE(a[i] == a[j]);
        EXPECT_FALSE(a[i] >= a[j]);
        EXPECT_FALSE(a[i] > a[j]);
      } else {
        EXPECT_FALSE(a[i] < a[j]);
        EXPECT_FALSE(a[i] <= a[j]);
        EXPECT_FALSE(a[i] == a[j]);
        EXPECT_TRUE(a[i] >= a[j]);
        EXPECT_TRUE(a[i] > a[j]);
      }
    }
  }
}

TEST(AttrKey0SetTest, Works) {
  AttrKeyHashSet<AttrKey<0>> set;

  EXPECT_THAT(set, IsEmpty());
  EXPECT_THAT(set, SizeIs(0));
  EXPECT_THAT(set, UnorderedElementsAre());
  EXPECT_FALSE(set.contains(AttrKey()));
  EXPECT_TRUE(set.find(AttrKey()) == set.end());
  EXPECT_EQ(set.erase(AttrKey()), 0);

  set.insert(AttrKey());

  EXPECT_THAT(set, Not(IsEmpty()));
  EXPECT_THAT(set, SizeIs(1));
  EXPECT_THAT(set, UnorderedElementsAre(AttrKey()));
  EXPECT_TRUE(set.contains(AttrKey()));
  EXPECT_TRUE(set.find(AttrKey()) == set.begin());
  EXPECT_EQ(set.erase(AttrKey()), 1);
  EXPECT_THAT(set, IsEmpty());

  set.insert(AttrKey());
  set.clear();
  EXPECT_THAT(set, IsEmpty());

  set.insert(AttrKey());
  set.erase(AttrKey());
  EXPECT_THAT(set, IsEmpty());
}

TEST(AttrKey0MapTest, Works) {
  AttrKeyHashMap<AttrKey<0>, int> map;

  EXPECT_THAT(map, IsEmpty());
  EXPECT_THAT(map, SizeIs(0));
  EXPECT_THAT(map, UnorderedElementsAre());
  EXPECT_FALSE(map.contains(AttrKey()));
  EXPECT_TRUE(map.find(AttrKey()) == map.end());
  EXPECT_EQ(map.erase(AttrKey()), 0);

  map.try_emplace(AttrKey(), 42);

  EXPECT_THAT(map, Not(IsEmpty()));
  EXPECT_THAT(map, SizeIs(1));
  EXPECT_THAT(map, UnorderedElementsAre(Pair(AttrKey(), 42)));
  EXPECT_EQ(map.begin()->first, AttrKey());
  EXPECT_EQ(map.begin()->second, 42);
  EXPECT_TRUE(map.contains(AttrKey()));
  EXPECT_TRUE(map.find(AttrKey()) == map.begin());
  EXPECT_EQ(map.erase(AttrKey()), 1);
  EXPECT_THAT(map, IsEmpty());

  map.insert({AttrKey(), 43});
  map.clear();
  EXPECT_THAT(map, IsEmpty());

  map.try_emplace(AttrKey(), 43);
  map.erase(AttrKey());
  EXPECT_THAT(map, IsEmpty());

  map.try_emplace(AttrKey(), 43);
  map.erase(map.begin());
  EXPECT_THAT(map, IsEmpty());
}

TEST(AttrKeyTest, FromRange) {
  EXPECT_THAT((AttrKey<0>::FromRange({})), IsOkAndHolds(AttrKey()));
  EXPECT_THAT((AttrKey<1>::FromRange({1})), IsOkAndHolds(AttrKey(1)));
  EXPECT_THAT((AttrKey<2>::FromRange({1, 2})), IsOkAndHolds(AttrKey(1, 2)));

  EXPECT_THAT((AttrKey<0>::FromRange({1})),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT((AttrKey<1>::FromRange({})),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT((AttrKey<2>::FromRange({1})),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(AttrKeyTest, FromRangeSymmetric) {
  using Key = AttrKey<3, ElementSymmetry<1, 2>>;
  EXPECT_THAT((Key::FromRange({0, 1, 2})), IsOkAndHolds(Key(0, 1, 2)));
  EXPECT_THAT((Key::FromRange({0, 2, 1})), IsOkAndHolds(Key(0, 1, 2)));
  EXPECT_THAT((Key::FromRange({3, 1, 2})), IsOkAndHolds(Key(3, 1, 2)));
  EXPECT_THAT((Key::FromRange({3, 2, 1})), IsOkAndHolds(Key(3, 1, 2)));
}

TEST(AttrKeyTest, IsAttrKey) {
  EXPECT_TRUE(is_attr_key_v<AttrKey<0>>);
  EXPECT_TRUE(is_attr_key_v<AttrKey<1>>);
  EXPECT_FALSE(is_attr_key_v<int>);
}

constexpr int kBenchmarkSize = 30;

template <typename SetT>
void BM_HashSet0(benchmark::State& state) {
  SetT set;
  for (const auto s : state) {
    auto it = set.find(AttrKey());
    benchmark::DoNotOptimize(it);
  }
}
BENCHMARK(BM_HashSet0<AttrKeyHashSet<AttrKey<0>>>);
BENCHMARK(BM_HashSet0<absl::flat_hash_set<AttrKey<0>>>);

template <typename T>
void BM_HashMap1(benchmark::State& state) {
  absl::flat_hash_map<T, int> map;
  for (int i = 0; i < kBenchmarkSize * kBenchmarkSize; ++i) {
    if (i % 2 > 0) {  // Half of the lookups are hits.
      map[T(i)] = i;
    }
  }
  for (const auto s : state) {
    for (int i = 0; i < kBenchmarkSize * kBenchmarkSize; ++i) {
      auto it = map.find(T(i));
      benchmark::DoNotOptimize(it);
    }
  }
}
BENCHMARK(BM_HashMap1<AttrKey<1>>);
BENCHMARK(BM_HashMap1<int64_t>);

template <typename T>
void BM_HashMap2(benchmark::State& state) {
  absl::flat_hash_map<T, int> map;
  for (int i = 0; i < kBenchmarkSize; ++i) {
    for (int j = 0; j < kBenchmarkSize; ++j) {
      if ((i * kBenchmarkSize + j) % 2 > 0) {  // Half of the lookups are hits.
        map[T(i, j)] = i;
      }
    }
  }
  for (const auto s : state) {
    for (int i = 0; i < kBenchmarkSize; ++i) {
      for (int j = 0; j < kBenchmarkSize; ++j) {
        auto it = map.find(T(i, j));
        benchmark::DoNotOptimize(it);
      }
    }
  }
}
BENCHMARK(BM_HashMap2<AttrKey<2>>);
BENCHMARK(BM_HashMap2<std::pair<int64_t, int64_t>>);

template <int n>
void BM_SortAttrKeys(benchmark::State& state) {
  const std::vector<AttrKey<n>> keys =
      MakeRandomAttrKeys<n, NoSymmetry>(state.range(0), state.range(0));

  for (const auto s : state) {
    auto copy = keys;
    absl::c_sort(copy);
    benchmark::DoNotOptimize(copy);
  }
}
BENCHMARK(BM_SortAttrKeys<1>)->Arg(100)->Arg(10000);
BENCHMARK(BM_SortAttrKeys<2>)->Arg(100)->Arg(10000);

}  // namespace
}  // namespace operations_research::math_opt
