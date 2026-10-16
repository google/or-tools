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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <utility>
#include <vector>

#include "absl/random/distributions.h"
#include "benchmark/benchmark.h"
#include "ortools/util/full_precision_inequalities.h"

namespace operations_research {
namespace {

// Args for all benchmarks with parameters for vector size and radius exponent.
#define BENCHMARK_DEFAULT_ARGS(benchmark_fn) \
  BENCHMARK(benchmark_fn)                    \
      ->ArgPair(4, 10)                       \
      ->ArgPair(4, 50)                       \
      ->ArgPair(4, 100)                      \
      ->ArgPair(16, 10)                      \
      ->ArgPair(16, 50)                      \
      ->ArgPair(16, 100)                     \
      ->ArgPair(64, 10)                      \
      ->ArgPair(64, 50)                      \
      ->ArgPair(64, 100)                     \
      ->ArgPair(256, 10)                     \
      ->ArgPair(256, 50)                     \
      ->ArgPair(256, 100)                    \
      ->ArgPair(1024, 10)                    \
      ->ArgPair(1024, 50)                    \
      ->ArgPair(1024, 100)

// Pre-generates a set of random finite vectors of length `vector_size`.
// We want enough vectors to form a large number of pairs while still fitting
// comfortably in L3 cache.
std::vector<std::vector<double>> GenerateRandomVectors(const int vector_size,
                                                       const double radius) {
  std::mt19937 random(12345);
  const int num_vectors =
      std::clamp(static_cast<int>(1000 / std::sqrt(vector_size)), 2, 500);
  std::vector<std::vector<double>> vectors(num_vectors,
                                           std::vector<double>(vector_size));
  for (int i = 0; i < num_vectors; ++i) {
    for (int k = 0; k < vector_size; ++k) {
      vectors[i][k] = absl::Uniform<double>(random, -radius, radius);
    }
  }
  return vectors;
}

void BM_GetLooseDotProductBounds(benchmark::State& state) {
  const int vector_size = state.range(0);
  const std::vector<std::vector<double>> vectors =
      GenerateRandomVectors(vector_size, std::ldexp(1.0, state.range(1)));
  const int num_vectors = vectors.size();

  int i = 0;
  int j = 0;
  for (auto _ : state) {
    auto bnds = GetLooseDotProductBounds(vectors[i], vectors[j]);
    benchmark::DoNotOptimize(bnds);
    if (++j == num_vectors) {
      j = 0;
      if (++i == num_vectors) i = 0;
    }
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK_DEFAULT_ARGS(BM_GetLooseDotProductBounds);

void BM_GetTightDotProductBounds(benchmark::State& state) {
  const int vector_size = state.range(0);
  const std::vector<std::vector<double>> vectors =
      GenerateRandomVectors(vector_size, std::ldexp(1.0, state.range(1)));
  const int num_vectors = vectors.size();

  int i = 0;
  int j = 0;
  for (auto _ : state) {
    benchmark::DoNotOptimize(
        TightDotProduct<double, double>(vectors[i], vectors[j]).TightBounds());
    if (++j == num_vectors) {
      j = 0;
      if (++i == num_vectors) i = 0;
    }
  }
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK_DEFAULT_ARGS(BM_GetTightDotProductBounds);

void BM_PrefixTightDotProductBounds(benchmark::State& state) {
  const int vector_size = state.range(0);
  const std::vector<std::vector<double>> vectors =
      GenerateRandomVectors(vector_size, std::ldexp(1.0, state.range(1)));
  const int num_vectors = vectors.size();

  int i = 0;
  int j = 0;
  for (auto _ : state) {
    TightSum sum;
    for (int k = 0; k < vector_size; ++k) {
      benchmark::DoNotOptimize(
          sum.AddProd(vectors[i][k], vectors[j][k]).TightBounds());
    }
    if (++j == num_vectors) {
      j = 0;
      if (++i == num_vectors) i = 0;
    }
  }
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_PrefixTightDotProductBounds)->RangeMultiplier(2)->Range(4, 1024);

void BM_DotProductIsSmallerOrEqual_FastPath(benchmark::State& state) {
  const int vector_size = state.range(0);
  const std::vector<std::vector<double>> vectors =
      GenerateRandomVectors(vector_size, std::ldexp(1.0, state.range(1)));
  const int num_vectors = vectors.size();

  std::mt19937 random(54321);
  std::vector<double> bounds(num_vectors);
  for (int i = 0; i < num_vectors; ++i) {
    bounds[i] = absl::Uniform<double>(random, -1000.0, 1000.0);
  }

  int i = 0;
  int j = 0;
  for (auto _ : state) {
    benchmark::DoNotOptimize(
        DotProductIsSmallerOrEqual(vectors[i], vectors[j], bounds[i]));
    if (++j == num_vectors) {
      j = 0;
      if (++i == num_vectors) i = 0;
    }
  }
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK_DEFAULT_ARGS(BM_DotProductIsSmallerOrEqual_FastPath);

void BM_DotProductIsSmallerOrEqual_SlowPath(benchmark::State& state) {
  const int vector_size = state.range(0);
  const std::vector<std::vector<double>> vectors =
      GenerateRandomVectors(vector_size, std::ldexp(1.0, state.range(1)));
  const int num_vectors = vectors.size();

  // Precompute the floating-point dot product for each pair (i, j) so the bound
  // falls inside GetLooseDotProductBounds(vectors[i], vectors[j]), forcing the
  // full-precision slow path.
  std::vector<std::vector<double>> bounds(num_vectors,
                                          std::vector<double>(num_vectors));
  for (int i = 0; i < num_vectors; ++i) {
    for (int j = 0; j < num_vectors; ++j) {
      bounds[i][j] = std::inner_product(vectors[i].begin(), vectors[i].end(),
                                        vectors[j].begin(), 0.0);
    }
  }

  int i = 0;
  int j = 0;
  for (auto _ : state) {
    benchmark::DoNotOptimize(
        DotProductIsSmallerOrEqual(vectors[i], vectors[j], bounds[i][j]));
    if (++j == num_vectors) {
      j = 0;
      if (++i == num_vectors) i = 0;
    }
  }
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK_DEFAULT_ARGS(BM_DotProductIsSmallerOrEqual_SlowPath);

// Comparison between direct computation and using TightSum.
template <typename N1, typename N2>
void BM_ProdBounds(benchmark::State& state) {
  std::mt19937 random(12345);
  constexpr int kNumPairs = 1'000'000;
  std::vector<std::pair<N1, N2>> pairs(kNumPairs);
  for (int i = 0; i < kNumPairs; ++i) {
    pairs[i] = {N1(absl::Uniform<N1>(random, std::numeric_limits<N1>::lowest(),
                                     std::numeric_limits<N1>::max())),
                N2(absl::Uniform<N2>(random, std::numeric_limits<N2>::lowest(),
                                     std::numeric_limits<N2>::max()))};
  }
  int i = 0;
  for (auto _ : state) {
    benchmark::DoNotOptimize(ProdBounds(pairs[i].first, pairs[i].second));
    if (++i == kNumPairs) i = 0;
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ProdBounds<double, double>);
BENCHMARK(BM_ProdBounds<double, int64_t>);
BENCHMARK(BM_ProdBounds<int64_t, double>);
BENCHMARK(BM_ProdBounds<int64_t, int64_t>);

template <typename N1, typename N2>
void BM_ProdBoundsViaTightSum(benchmark::State& state) {
  std::mt19937 random(12345);
  constexpr int kNumPairs = 1'000'000;
  std::vector<std::pair<N1, N2>> pairs(kNumPairs);
  for (int i = 0; i < kNumPairs; ++i) {
    pairs[i] = {N1(absl::Uniform<N1>(random, std::numeric_limits<N1>::lowest(),
                                     std::numeric_limits<N1>::max())),
                N2(absl::Uniform<N2>(random, std::numeric_limits<N2>::lowest(),
                                     std::numeric_limits<N2>::max()))};
  }
  int i = 0;
  for (auto _ : state) {
    benchmark::DoNotOptimize(
        TightSum().AddProd(pairs[i].first, pairs[i].second).TightBounds());
    if (++i == kNumPairs) i = 0;
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ProdBoundsViaTightSum<double, double>);
BENCHMARK(BM_ProdBoundsViaTightSum<double, int64_t>);
BENCHMARK(BM_ProdBoundsViaTightSum<int64_t, double>);
BENCHMARK(BM_ProdBoundsViaTightSum<int64_t, int64_t>);

void BM_IterativeSumBounds(benchmark::State& state) {
  const int vector_size = state.range(0);
  const double radius = std::ldexp(1.0, state.range(1));
  std::vector<double> vector(vector_size);
  std::mt19937 random(12345);
  for (int k = 0; k < vector_size; ++k) {
    vector[k] = absl::Uniform<double>(random, -radius, radius);
  }

  TightSum sum;

  int i = 0;
  for (auto _ : state) {
    sum.Add(vector[i]);
    benchmark::DoNotOptimize(sum.TightBounds());
    if (++i == vector_size) i = 0;
  }
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK_DEFAULT_ARGS(BM_IterativeSumBounds);

}  // namespace
}  // namespace operations_research
