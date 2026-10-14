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
#include <numeric>
#include <random>
#include <vector>

#include "absl/random/distributions.h"
#include "benchmark/benchmark.h"
#include "ortools/util/full_precision_inequalities.h"

namespace operations_research {
namespace {

// Pre-generates a set of random finite vectors of length `vector_size`.
// We want enough vectors to form a large number of pairs while still fitting
// comfortably in L3 cache.
std::vector<std::vector<double>> GenerateRandomVectors(int vector_size) {
  std::mt19937 random(12345);
  const int num_vectors =
      std::clamp(static_cast<int>(1000 / std::sqrt(vector_size)), 2, 500);
  std::vector<std::vector<double>> vectors(num_vectors,
                                           std::vector<double>(vector_size));
  for (int i = 0; i < num_vectors; ++i) {
    for (int k = 0; k < vector_size; ++k) {
      vectors[i][k] = absl::Uniform<double>(random, -1000.0, 1000.0);
    }
  }
  return vectors;
}

void BM_GetLooseDotProductBounds(benchmark::State& state) {
  const int vector_size = state.range(0);
  const std::vector<std::vector<double>> vectors =
      GenerateRandomVectors(vector_size);
  const int num_vectors = vectors.size();

  int i = 0;
  int j = 0;
  for (auto _ : state) {
    benchmark::DoNotOptimize(GetLooseDotProductBounds(vectors[i], vectors[j]));
    if (++j == num_vectors) {
      j = 0;
      if (++i == num_vectors) i = 0;
    }
  }
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_GetLooseDotProductBounds)->RangeMultiplier(2)->Range(4, 1024);

void BM_GetTightDotProductBounds(benchmark::State& state) {
  const int vector_size = state.range(0);
  const std::vector<std::vector<double>> vectors =
      GenerateRandomVectors(vector_size);
  const int num_vectors = vectors.size();

  int i = 0;
  int j = 0;
  for (auto _ : state) {
    benchmark::DoNotOptimize(GetTightDotProductBounds(vectors[i], vectors[j]));
    if (++j == num_vectors) {
      j = 0;
      if (++i == num_vectors) i = 0;
    }
  }
  state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_GetTightDotProductBounds)->RangeMultiplier(2)->Range(4, 1024);

void BM_DotProductIsSmallerOrEqual_FastPath(benchmark::State& state) {
  const int vector_size = state.range(0);
  const std::vector<std::vector<double>> vectors =
      GenerateRandomVectors(vector_size);
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

BENCHMARK(BM_DotProductIsSmallerOrEqual_FastPath)
    ->RangeMultiplier(2)
    ->Range(4, 1024);

void BM_DotProductIsSmallerOrEqual_SlowPath(benchmark::State& state) {
  const int vector_size = state.range(0);
  const std::vector<std::vector<double>> vectors =
      GenerateRandomVectors(vector_size);
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

BENCHMARK(BM_DotProductIsSmallerOrEqual_SlowPath)
    ->RangeMultiplier(2)
    ->Range(4, 1024);

}  // namespace
}  // namespace operations_research
