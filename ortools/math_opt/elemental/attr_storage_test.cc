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

#include "ortools/math_opt/elemental/attr_storage.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/symmetry.h"

namespace operations_research::math_opt {
namespace {

using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::UnorderedElementsAre;

TEST(Attr0StorageTest, EmptyGetters) {
  const AttrStorage<double, 0, NoSymmetry> attr_storage(1.0);

  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey()), 1.0);
  EXPECT_FALSE(attr_storage.IsNonDefault(AttrKey()));
}

TEST(Attr0StorageTest, SetDefaultToDefault) {
  AttrStorage<double, 0, NoSymmetry> attr_storage(1.0);

  EXPECT_FALSE(attr_storage.Set(AttrKey(), 1.0));

  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey()), 1.0);
  EXPECT_FALSE(attr_storage.IsNonDefault(AttrKey()));
}

TEST(Attr0StorageTest, SetDefaultToNonDefault) {
  AttrStorage<double, 0, NoSymmetry> attr_storage(1.0);

  EXPECT_TRUE(attr_storage.Set(AttrKey(), 10.0));

  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey()), 10.0);
  EXPECT_TRUE(attr_storage.IsNonDefault(AttrKey()));
}

TEST(Attr0StorageTest, SetNonDefaultToDefault) {
  AttrStorage<double, 0, NoSymmetry> attr_storage(1.0);
  EXPECT_TRUE(attr_storage.Set(AttrKey(), 10.0));

  EXPECT_TRUE(attr_storage.Set(AttrKey(), 1.0));

  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey()), 1.0);
  EXPECT_FALSE(attr_storage.IsNonDefault(AttrKey()));
}

TEST(Attr0StorageTest, SetNonDefaultToNonDefaultDifferent) {
  AttrStorage<double, 0, NoSymmetry> attr_storage(1.0);
  attr_storage.Set(AttrKey(), 10.0);

  EXPECT_TRUE(attr_storage.Set(AttrKey(), 20.0));

  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey()), 20.0);
  EXPECT_TRUE(attr_storage.IsNonDefault(AttrKey()));
}

TEST(Attr0StorageTest, SetNonDefaultToNonDefaultSame) {
  AttrStorage<double, 0, NoSymmetry> attr_storage(1.0);
  attr_storage.Set(AttrKey(), 10.0);

  EXPECT_FALSE(attr_storage.Set(AttrKey(), 10.0));

  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey()), 10.0);
  EXPECT_TRUE(attr_storage.IsNonDefault(AttrKey()));
}

////////////////////////////////////////////////////////////////////////////////
// Attr1Storage
////////////////////////////////////////////////////////////////////////////////

TEST(Attr1StorageTest, EmptyGetters) {
  const AttrStorage<double, 1, NoSymmetry> attr_storage(1.0);

  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(0)), 1.0);
  EXPECT_FALSE(attr_storage.IsNonDefault(AttrKey(0)));
  EXPECT_THAT(attr_storage.NonDefaults(), IsEmpty());
  EXPECT_EQ(attr_storage.num_non_defaults(), 0);
  EXPECT_THAT(attr_storage.Slice<0>(0), IsEmpty());
}

TEST(Attr1StorageTest, GettersNonEmpty) {
  AttrStorage<double, 1, NoSymmetry> attr_storage(1.0);
  attr_storage.Set(AttrKey(2), 10.0);
  attr_storage.Set(AttrKey(3), 11.0);
  attr_storage.Set(AttrKey(5), 12.0);

  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(2)), 10.0);
  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(3)), 11.0);
  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(4)), 1.0);
  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(5)), 12.0);
  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(6)), 1.0);

  EXPECT_THAT(attr_storage.NonDefaults(),
              UnorderedElementsAre(AttrKey(2), AttrKey(3), AttrKey(5)));
  EXPECT_EQ(attr_storage.num_non_defaults(), 3);
  EXPECT_THAT(attr_storage.Slice<0>(3), UnorderedElementsAre(AttrKey(3)));
}

TEST(Attr1StorageTest, SetDefaultToDefault) {
  AttrStorage<double, 1, NoSymmetry> attr_storage(1.0);

  EXPECT_FALSE(attr_storage.Set(AttrKey(2), 1.0));

  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(2)), 1.0);
  EXPECT_THAT(attr_storage.NonDefaults(), IsEmpty());
}

TEST(Attr1StorageTest, SetDefaultToNonDefault) {
  AttrStorage<double, 1, NoSymmetry> attr_storage(1.0);

  EXPECT_TRUE(attr_storage.Set(AttrKey(2), 10.0));

  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(2)), 10.0);
  EXPECT_THAT(attr_storage.NonDefaults(), UnorderedElementsAre(AttrKey(2)));
}

TEST(Attr1StorageTest, SetNonDefaultToDefault) {
  AttrStorage<double, 1, NoSymmetry> attr_storage(1.0);
  attr_storage.Set(AttrKey(2), 10.0);

  EXPECT_TRUE(attr_storage.Set(AttrKey(2), 1.0));

  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(2)), 1.0);
  EXPECT_THAT(attr_storage.NonDefaults(), IsEmpty());
}

TEST(Attr1StorageTest, SetNonDefaultToNonDefaultDifferent) {
  AttrStorage<double, 1, NoSymmetry> attr_storage(1.0);
  attr_storage.Set(AttrKey(2), 5.0);

  EXPECT_TRUE(attr_storage.Set(AttrKey(2), 10.0));

  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(2)), 10.0);
  EXPECT_THAT(attr_storage.NonDefaults(), UnorderedElementsAre(AttrKey(2)));
}

TEST(Attr1StorageTest, SetNonDefaultToNonDefaultSame) {
  AttrStorage<double, 1, NoSymmetry> attr_storage(1.0);
  attr_storage.Set(AttrKey(2), 10.0);

  EXPECT_FALSE(attr_storage.Set(AttrKey(2), 10.0));

  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(2)), 10.0);
  EXPECT_THAT(attr_storage.NonDefaults(), UnorderedElementsAre(AttrKey(2)));
}

TEST(Attr1StorageTest, Clear) {
  AttrStorage<double, 1, NoSymmetry> attr_storage(1.0);
  attr_storage.Set(AttrKey(2), 10.0);
  attr_storage.Set(AttrKey(3), 11.0);

  EXPECT_THAT(attr_storage.NonDefaults(),
              UnorderedElementsAre(AttrKey(2), AttrKey(3)));
  EXPECT_EQ(attr_storage.num_non_defaults(), 2);

  attr_storage.Clear();

  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(2)), 1.0);
  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(3)), 1.0);
  EXPECT_THAT(attr_storage.NonDefaults(), IsEmpty());
  EXPECT_EQ(attr_storage.num_non_defaults(), 0);
}

TEST(Attr1StorageTest, Erase) {
  AttrStorage<double, 1, NoSymmetry> attr_storage(1.0);
  attr_storage.Set(AttrKey(2), 10.0);
  attr_storage.Set(AttrKey(3), 11.0);

  attr_storage.Erase(AttrKey(2));

  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(2)), 1.0);
  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(3)), 11.0);
  EXPECT_THAT(attr_storage.NonDefaults(), UnorderedElementsAre(AttrKey(3)));
  EXPECT_EQ(attr_storage.num_non_defaults(), 1);
}

////////////////////////////////////////////////////////////////////////////////
// Attr2Storage
////////////////////////////////////////////////////////////////////////////////

TEST(Attr2StorageTest, EmptyGetters) {
  const AttrStorage<double, 2, NoSymmetry> attr_storage(1.0);

  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(0, 0)), 1.0);
  EXPECT_FALSE(attr_storage.IsNonDefault(AttrKey(0, 0)));
  EXPECT_THAT(attr_storage.NonDefaults(), IsEmpty());
  EXPECT_EQ(attr_storage.num_non_defaults(), 0);
  EXPECT_THAT(attr_storage.Slice<1>(0), IsEmpty());
  EXPECT_THAT(attr_storage.GetSliceSize<1>(0), 0);
  EXPECT_THAT(attr_storage.Slice<0>(0), IsEmpty());
  EXPECT_THAT(attr_storage.GetSliceSize<0>(0), 0);
}

TEST(Attr2StorageTest, GettersNonEmpty) {
  AttrStorage<double, 2, NoSymmetry> attr_storage(1.0);
  attr_storage.Set(AttrKey(2, 3), 10.0);
  attr_storage.Set(AttrKey(2, 5), 11.0);
  attr_storage.Set(AttrKey(5, 5), 12.0);

  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(2, 3)), 10.0);
  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(2, 5)), 11.0);
  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(5, 5)), 12.0);
  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(5, 2)), 1.0);
  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(2, 2)), 1.0);

  EXPECT_THAT(
      attr_storage.NonDefaults(),
      UnorderedElementsAre(AttrKey(2, 3), AttrKey(2, 5), AttrKey(5, 5)));
  EXPECT_EQ(attr_storage.num_non_defaults(), 3);
  EXPECT_THAT(attr_storage.Slice<0>(2),
              UnorderedElementsAre(AttrKey(2, 3), AttrKey(2, 5)));
  EXPECT_THAT(attr_storage.GetSliceSize<0>(2), 2);
  EXPECT_THAT(attr_storage.Slice<0>(3), IsEmpty());
  EXPECT_THAT(attr_storage.GetSliceSize<0>(3), 0);
  EXPECT_THAT(attr_storage.Slice<0>(5), UnorderedElementsAre(AttrKey(5, 5)));
  EXPECT_THAT(attr_storage.GetSliceSize<0>(5), 1);

  EXPECT_THAT(attr_storage.Slice<1>(2), IsEmpty());
  EXPECT_THAT(attr_storage.GetSliceSize<1>(2), 0);
  EXPECT_THAT(attr_storage.Slice<1>(3), UnorderedElementsAre(AttrKey(2, 3)));
  EXPECT_THAT(attr_storage.GetSliceSize<1>(3), 1);
  EXPECT_THAT(attr_storage.Slice<1>(5),
              UnorderedElementsAre(AttrKey(2, 5), AttrKey(5, 5)));
  EXPECT_THAT(attr_storage.GetSliceSize<1>(5), 2);
}

TEST(Attr2StorageTest, GettersNonEmptySymmetric) {
  //                    Dim 0
  //         |  0   1   2   3   4   5
  //       --+------------------------
  //       0 |  0
  // D     1 |  0   0
  // i     2 |  0   0   0
  // m     3 |  0  10   0   0
  // 1     4 |  0   0   0   0   0
  //       5 |  0  11   0   0   0  12
  //
  using Storage = AttrStorage<double, 2, ElementSymmetry<0, 1>>;
  using Key = Storage::Key;
  Storage attr_storage(1.0);
  attr_storage.Set(Key(2, 3), 10.0);
  attr_storage.Set(Key(2, 5), 123.0);
  attr_storage.Set(Key(5, 2), 11.0);  // Overwrites 123.0.
  attr_storage.Set(Key(5, 5), 12.0);

  EXPECT_DOUBLE_EQ(attr_storage.Get(Key(2, 3)), 10.0);
  EXPECT_DOUBLE_EQ(attr_storage.Get(Key(2, 5)), 11.0);
  EXPECT_DOUBLE_EQ(attr_storage.Get(Key(5, 5)), 12.0);
  EXPECT_DOUBLE_EQ(attr_storage.Get(Key(3, 2)), 10.0);
  EXPECT_DOUBLE_EQ(attr_storage.Get(Key(5, 2)), 11.0);
  EXPECT_DOUBLE_EQ(attr_storage.Get(Key(2, 2)), 1.0);

  EXPECT_THAT(attr_storage.NonDefaults(),
              UnorderedElementsAre(Key(2, 3), Key(2, 5), Key(5, 5)));
  EXPECT_EQ(attr_storage.num_non_defaults(), 3);
  EXPECT_THAT(attr_storage.Slice<0>(2),
              UnorderedElementsAre(Key(2, 3), Key(2, 5)));
  EXPECT_THAT(attr_storage.GetSliceSize<0>(2), 2);
  EXPECT_THAT(attr_storage.Slice<0>(3), UnorderedElementsAre(Key(2, 3)));
  EXPECT_THAT(attr_storage.GetSliceSize<0>(3), 1);
  EXPECT_THAT(attr_storage.Slice<0>(4), IsEmpty());
  EXPECT_THAT(attr_storage.GetSliceSize<0>(4), 0);
  EXPECT_THAT(attr_storage.Slice<0>(5),
              UnorderedElementsAre(Key(2, 5), Key(5, 5)));
  EXPECT_THAT(attr_storage.GetSliceSize<0>(5), 2);

  EXPECT_THAT(attr_storage.Slice<1>(2),
              UnorderedElementsAre(Key(2, 3), Key(2, 5)));
  EXPECT_THAT(attr_storage.GetSliceSize<1>(2), 2);
  EXPECT_THAT(attr_storage.Slice<1>(3), UnorderedElementsAre(Key(2, 3)));
  EXPECT_THAT(attr_storage.GetSliceSize<1>(3), 1);
  EXPECT_THAT(attr_storage.Slice<1>(4), IsEmpty());
  EXPECT_THAT(attr_storage.GetSliceSize<1>(4), 0);
  EXPECT_THAT(attr_storage.Slice<1>(5),
              UnorderedElementsAre(Key(2, 5), Key(5, 5)));
  EXPECT_THAT(attr_storage.GetSliceSize<1>(5), 2);
}

TEST(Attr2StorageTest, SetDefaultToDefault) {
  AttrStorage<double, 2, NoSymmetry> attr_storage(1.0);

  EXPECT_FALSE(attr_storage.Set(AttrKey(2, 3), 1.0).has_value());

  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(2, 3)), 1.0);
  EXPECT_THAT(attr_storage.NonDefaults(), IsEmpty());
}

TEST(Attr2StorageTest, SetDefaultToNonDefault) {
  AttrStorage<double, 2, NoSymmetry> attr_storage(1.0);

  EXPECT_THAT(attr_storage.Set(AttrKey(2, 3), 10.0), Optional(1.0));

  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(2, 3)), 10.0);
  EXPECT_THAT(attr_storage.NonDefaults(), UnorderedElementsAre(AttrKey(2, 3)));
}

TEST(Attr2StorageTest, SetNonDefaultToDefault) {
  AttrStorage<double, 2, NoSymmetry> attr_storage(1.0);
  attr_storage.Set(AttrKey(2, 3), 10.0);

  EXPECT_THAT(attr_storage.Set(AttrKey(2, 3), 1.0), Optional(10.0));

  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(2, 3)), 1.0);
  EXPECT_THAT(attr_storage.NonDefaults(), IsEmpty());
}

TEST(Attr2StorageTest, SetNonDefaultToNonDefaultDifferent) {
  AttrStorage<double, 2, NoSymmetry> attr_storage(1.0);
  attr_storage.Set(AttrKey(2, 3), 5.0);

  EXPECT_THAT(attr_storage.Set(AttrKey(2, 3), 10.0), Optional(5.0));

  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(2, 3)), 10.0);
  EXPECT_THAT(attr_storage.NonDefaults(), UnorderedElementsAre(AttrKey(2, 3)));
}

TEST(Attr2StorageTest, SetNonDefaultToNonDefaultSame) {
  AttrStorage<double, 2, NoSymmetry> attr_storage(1.0);
  attr_storage.Set(AttrKey(2, 3), 10.0);

  EXPECT_FALSE(attr_storage.Set(AttrKey(2, 3), 10.0));

  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(2, 3)), 10.0);
  EXPECT_THAT(attr_storage.NonDefaults(), UnorderedElementsAre(AttrKey(2, 3)));
}

TEST(Attr2StorageTest, Clear) {
  AttrStorage<double, 2, NoSymmetry> attr_storage(1.0);
  attr_storage.Set(AttrKey(2, 3), 10.0);
  attr_storage.Set(AttrKey(3, 4), 11.0);

  EXPECT_THAT(attr_storage.NonDefaults(),
              UnorderedElementsAre(AttrKey(2, 3), AttrKey(3, 4)));
  EXPECT_EQ(attr_storage.num_non_defaults(), 2);

  attr_storage.Clear();

  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(2, 3)), 1.0);
  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(3, 4)), 1.0);
  EXPECT_THAT(attr_storage.NonDefaults(), IsEmpty());
  EXPECT_EQ(attr_storage.num_non_defaults(), 0);
}

TEST(Attr2StorageTest, Erase) {
  AttrStorage<double, 2, NoSymmetry> attr_storage(1.0);
  attr_storage.Set(AttrKey(2, 3), 10.0);
  attr_storage.Set(AttrKey(3, 4), 11.0);

  attr_storage.Erase(AttrKey(2, 3));

  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(2, 3)), 1.0);
  EXPECT_DOUBLE_EQ(attr_storage.Get(AttrKey(3, 4)), 11.0);
  EXPECT_THAT(attr_storage.NonDefaults(), UnorderedElementsAre(AttrKey(3, 4)));
  EXPECT_EQ(attr_storage.num_non_defaults(), 1);
}

TEST(Attr2StorageTest, EraseColumnLives) {
  AttrStorage<double, 2, NoSymmetry> attr_storage(1.0);
  attr_storage.Set(AttrKey(2, 3), 10.0);
  attr_storage.Set(AttrKey(5, 3), 11.0);

  attr_storage.Erase(AttrKey(2, 3));

  EXPECT_THAT(attr_storage.NonDefaults(), UnorderedElementsAre(AttrKey(5, 3)));
  EXPECT_THAT(attr_storage.Slice<0>(5), UnorderedElementsAre(AttrKey(5, 3)));
  EXPECT_THAT(attr_storage.Slice<1>(3), UnorderedElementsAre(AttrKey(5, 3)));

  // Insert again.
  attr_storage.Set(AttrKey(2, 3), 12.0);
  EXPECT_THAT(attr_storage.NonDefaults(),
              UnorderedElementsAre(AttrKey(2, 3), AttrKey(5, 3)));
  EXPECT_THAT(attr_storage.Slice<0>(5), UnorderedElementsAre(AttrKey(5, 3)));
  EXPECT_THAT(attr_storage.Slice<1>(3),
              UnorderedElementsAre(AttrKey(2, 3), AttrKey(5, 3)));
}

TEST(Attr2StorageTest, EraseRowLives) {
  AttrStorage<double, 2, NoSymmetry> attr_storage(1.0);
  attr_storage.Set(AttrKey(3, 2), 10.0);
  attr_storage.Set(AttrKey(3, 5), 11.0);

  attr_storage.Erase(AttrKey(3, 2));

  EXPECT_THAT(attr_storage.NonDefaults(), UnorderedElementsAre(AttrKey(3, 5)));
  EXPECT_THAT(attr_storage.Slice<0>(3), UnorderedElementsAre(AttrKey(3, 5)));
  EXPECT_THAT(attr_storage.Slice<1>(5), UnorderedElementsAre(AttrKey(3, 5)));
}

// Makes a set of `n` 1-dimensional keys.
std::vector<AttrKey<1>> Make1DKeys(int n) {
  std::vector<AttrKey<1>> keys;
  for (int64_t i = 0; i < n; ++i) {
    keys.emplace_back(i);
  }
  return keys;
}

// Makes a set of `n^2` 2-dimensional keys.
// NOTE: depending in `Symmetry` this might create duplicate keys. This is
// intentional, as we want to have the same number of keys to be able to compare
// the performance of different symmetries.
template <typename Symmetry>
std::vector<AttrKey<2, Symmetry>> Make2DKeys(int n) {
  std::vector<AttrKey<2, Symmetry>> keys;
  for (int64_t i = 0; i < n; ++i) {
    for (int64_t j = 0; j < n; ++j) {
      keys.emplace_back(i, j);
    }
  }
  return keys;
}

// A functor that returns true every N calls, false otherwise.
template <int N>
struct TrueEvery {
  int n = 0;
  bool operator()() {
    if (n == N) {
      n = 0;
      return true;
    }
    ++n;
    return false;
  }
};

void BM_Attr0StorageSet(benchmark::State& state) {
  AttrStorage<double, 0, NoSymmetry> attr_storage(1.0);

  for (const auto s : state) {
    attr_storage.Set(AttrKey(), 10.0);
    benchmark::DoNotOptimize(attr_storage);
  }
}
BENCHMARK(BM_Attr0StorageSet);

void BM_Attr1StorageSet(benchmark::State& state) {
  const int n = state.range(0);

  AttrStorage<double, 1, NoSymmetry> attr_storage(1.0);
  const auto keys = Make1DKeys(n);

  for (const auto s : state) {
    for (const auto& key : keys) {
      attr_storage.Set(key, 10.0);
    }
  }
}
BENCHMARK(BM_Attr1StorageSet)->Arg(900);

template <typename Symmetry>
void BM_Attr2StorageSet(benchmark::State& state) {
  const int n = state.range(0);

  const auto keys = Make2DKeys<Symmetry>(n);

  std::optional<AttrStorage<double, 2, Symmetry>> attr_storage(1.0);
  for (const auto s : state) {
    for (const auto& key : keys) {
      attr_storage->Set(key, 10.0);
    }
    state.PauseTiming();
    attr_storage.emplace(1.0);
    state.ResumeTiming();
  }
}
BENCHMARK(BM_Attr2StorageSet<NoSymmetry>)->Arg(30);
BENCHMARK(BM_Attr2StorageSet<ElementSymmetry<0, 1>>)->Arg(30);

void BM_Attr0StorageGet(benchmark::State& state) {
  AttrStorage<double, 0, NoSymmetry> attr_storage(1.0);

  for (const auto s : state) {
    double v = attr_storage.Get(AttrKey());
    benchmark::DoNotOptimize(v);
  }
}
BENCHMARK(BM_Attr0StorageGet);

void BM_Attr1StorageGet(benchmark::State& state) {
  const int n = state.range(0);

  AttrStorage<double, 1, NoSymmetry> attr_storage(1.0);
  const auto keys = Make1DKeys(n);
  // Insert half the keys.
  TrueEvery<2> sample;
  for (const auto& key : keys) {
    if (sample()) {
      attr_storage.Set(key, 10.0);
    }
  }

  for (const auto s : state) {
    for (const auto& key : keys) {
      double v = attr_storage.Get(key);
      benchmark::DoNotOptimize(v);
    }
  }
}
BENCHMARK(BM_Attr1StorageGet)->Arg(900);

template <typename Symmetry>
void BM_Attr2StorageGet(benchmark::State& state) {
  const int n = state.range(0);

  AttrStorage<double, 2, Symmetry> attr_storage(1.0);
  const auto keys = Make2DKeys<Symmetry>(n);
  // Insert half the keys.
  TrueEvery<2> sample;
  for (const auto& key : keys) {
    if (sample()) {
      attr_storage.Set(key, 10.0);
    }
  }

  for (const auto s : state) {
    for (const auto& key : keys) {
      double v = attr_storage.Get(key);
      benchmark::DoNotOptimize(v);
    }
  }
}
BENCHMARK(BM_Attr2StorageGet<NoSymmetry>)->Arg(30);
BENCHMARK(BM_Attr2StorageGet<ElementSymmetry<0, 1>>)->Arg(30);

template <typename Symmetry>
void BM_Attr2StorageSlice(benchmark::State& state) {
  const int n = state.range(0);

  AttrStorage<double, 2, Symmetry> attr_storage(1.0);
  const auto keys = Make2DKeys<Symmetry>(n);
  // Insert 5% of the keys.
  TrueEvery<20> sample;
  for (const auto& key : keys) {
    if (sample()) {
      attr_storage.Set(key, 10.0);
    }
  }

  for (const auto s : state) {
    for (int key_id = 0; key_id < n; ++key_id) {
      auto slice0 = attr_storage.template Slice<0>(key_id);
      auto slice1 = attr_storage.template Slice<1>(key_id);
      benchmark::DoNotOptimize(slice0);
      benchmark::DoNotOptimize(slice1);
    }
  }
}
BENCHMARK(BM_Attr2StorageSlice<NoSymmetry>)->Arg(30);
BENCHMARK(BM_Attr2StorageSlice<ElementSymmetry<0, 1>>)->Arg(30);

}  // namespace
}  // namespace operations_research::math_opt
