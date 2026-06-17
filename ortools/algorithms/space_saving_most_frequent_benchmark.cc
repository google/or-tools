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

#include <array>
#include <random>
#include <utility>
#include <vector>

#include "absl/random/random.h"
#include "benchmark/benchmark.h"
#include "ortools/algorithms/space_saving_most_frequent.h"

namespace operations_research {
namespace {

template <int kElementSize>
struct Element {
  Element() = default;
  explicit Element(int value) : value(value) {}
  int value;
  std::array<int, kElementSize> zeros;
  template <typename H>
  friend H AbslHashValue(H h, const Element& e) {
    return H::combine(std::move(h), e.value);
  }
  friend bool operator==(const Element& a, const Element& b) {
    return a.value == b.value;
  }
};

template <int kSize, int kCapacity, int kElementSize>
void BM_Add_GeometricDistributed(benchmark::State& state) {
  using Element = Element<kElementSize>;
  static constexpr int kNumInputs = 100;
  absl::BitGen random;
  std::vector<std::vector<Element>> inputs;
  inputs.reserve(kNumInputs);
  std::geometric_distribution<int> distribution(1.0 / kCapacity);
  for (int i = 0; i < kNumInputs; ++i) {
    std::vector<Element>& input = inputs.emplace_back();
    input.reserve(kSize);
    for (int j = 0; j < kSize; ++j) {
      input.push_back(Element(distribution(random)));
    }
  }

  // Start the benchmark.
  for (auto _ : state) {
    for (const std::vector<Element>& input : inputs) {
      SpaceSavingMostFrequent<Element> most_frequent(kCapacity);
      for (const Element& value : input) {
        most_frequent.Add(value);
      }
    }
  }
  state.SetItemsProcessed(state.iterations() * kNumInputs * kSize);
}

BENCHMARK(BM_Add_GeometricDistributed<30, 10, 0>);
BENCHMARK(BM_Add_GeometricDistributed<100, 30, 0>);
BENCHMARK(BM_Add_GeometricDistributed<1000, 100, 0>);
BENCHMARK(BM_Add_GeometricDistributed<10000, 1000, 0>);
BENCHMARK(BM_Add_GeometricDistributed<100000, 10000, 0>);

BENCHMARK(BM_Add_GeometricDistributed<30, 10, 4>);
BENCHMARK(BM_Add_GeometricDistributed<100, 30, 4>);
BENCHMARK(BM_Add_GeometricDistributed<1000, 100, 4>);
BENCHMARK(BM_Add_GeometricDistributed<10000, 1000, 4>);
BENCHMARK(BM_Add_GeometricDistributed<100000, 10000, 4>);

}  // namespace
}  // namespace operations_research
