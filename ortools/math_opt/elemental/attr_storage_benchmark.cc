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

#include <cstdint>
#include <optional>
#include <vector>

#include "benchmark/benchmark.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/attr_storage.h"
#include "ortools/math_opt/elemental/symmetry.h"

namespace operations_research::math_opt {
namespace {

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
