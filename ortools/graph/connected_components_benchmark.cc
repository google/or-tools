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

#include <functional>
#include <vector>

#include "absl/log/check.h"
#include "benchmark/benchmark.h"
#include "ortools/graph/connected_components.h"
#include "ortools/graph/hash_or_tree_container.h"

namespace {

// Common logic for FindConnected benchmarks.
template <typename CompareOrHashT>
int FindConnected_Helper_Old(
    ConnectedComponentsFinder<int, CompareOrHashT>* finder) {
  std::vector<
      typename util::graph::HashOrTreeContainer<int, CompareOrHashT>::Set>
      components;
  finder->FindConnectedComponents(&components);
  return components.size();
}

template <typename CompareOrHashT>
int FindConnected_Helper_New(
    ConnectedComponentsFinder<int, CompareOrHashT>* finder) {
  return finder->FindConnectedComponents().size();
}

// Common logic for OneComponent_FindConnected benchmarks.
// This one has just one big component.
template <typename CompareOrHashT,
          int (*FindConnected_Helper)(
              ConnectedComponentsFinder<int, CompareOrHashT>*)>
void OneComponent_FindConnected(benchmark::State* state) {
  const int size = state->range(0);
  for (auto _ : *state) {
    ConnectedComponentsFinder<int, CompareOrHashT> finder;
    for (int i = 0; i < size; i++) {
      finder.AddNode(i);
    }

    for (int i = 0; i < size / 2; i++) {
      finder.AddEdge(i, i + 1);
      finder.AddEdge(size - 1 - i, size - 2 - i);
    }

    CHECK_EQ(FindConnected_Helper(&finder), 1);
  }
}

static void BM_OneComponent_FindConnected_Old_Less(benchmark::State& state) {
  OneComponent_FindConnected<std::less<int>, FindConnected_Helper_Old>(&state);
}
BENCHMARK(BM_OneComponent_FindConnected_Old_Less)->Range(1, 1 << 19);

static void BM_OneComponent_FindConnected_Old_Hash(benchmark::State& state) {
  OneComponent_FindConnected<std::hash<int>, FindConnected_Helper_Old>(&state);
}
BENCHMARK(BM_OneComponent_FindConnected_Old_Hash)->Range(1, 1 << 19);

static void BM_OneComponent_FindConnected_New_Less(benchmark::State& state) {
  OneComponent_FindConnected<std::less<int>, FindConnected_Helper_New>(&state);
}
BENCHMARK(BM_OneComponent_FindConnected_New_Less)->Range(1, 1 << 19);

static void BM_OneComponent_FindConnected_New_Hash(benchmark::State& state) {
  OneComponent_FindConnected<std::hash<int>, FindConnected_Helper_New>(&state);
}
BENCHMARK(BM_OneComponent_FindConnected_New_Hash)->Range(1, 1 << 19);

// Common logic for TinyComponents_FindConnected benchmarks.
// This one has components of size 3: edges are from 3n to 3n+1, 3n+1 to 3n+2.
template <typename CompareOrHashT,
          int (*FindConnected_Helper)(
              ConnectedComponentsFinder<int, CompareOrHashT>*)>
static void TinyComponents_FindConnected(benchmark::State* state) {
  const int size = state->range(0);
  for (auto _ : *state) {
    ConnectedComponentsFinder<int, CompareOrHashT> finder;

    for (int i = 0; i < size; i++) {
      finder.AddEdge(3 * i, 3 * i + 1);
    }
    for (int i = 0; i < size; i++) {
      finder.AddEdge(3 * i + 1, 3 * i + 2);
    }

    CHECK_EQ(FindConnected_Helper(&finder), size);
  }
}

static void BM_TinyComponents_FindConnected_Old_Less(benchmark::State& state) {
  TinyComponents_FindConnected<std::less<int>, FindConnected_Helper_Old>(
      &state);
}
BENCHMARK(BM_TinyComponents_FindConnected_Old_Less)->Range(1, 1 << 19);

static void BM_TinyComponents_FindConnected_Old_Hash(benchmark::State& state) {
  TinyComponents_FindConnected<std::hash<int>, FindConnected_Helper_Old>(
      &state);
}
BENCHMARK(BM_TinyComponents_FindConnected_Old_Hash)->Range(1, 1 << 19);

static void BM_TinyComponents_FindConnected_New_Less(benchmark::State& state) {
  TinyComponents_FindConnected<std::less<int>, FindConnected_Helper_New>(
      &state);
}
BENCHMARK(BM_TinyComponents_FindConnected_New_Less)->Range(1, 1 << 19);

static void BM_TinyComponents_FindConnected_New_Hash(benchmark::State& state) {
  TinyComponents_FindConnected<std::hash<int>, FindConnected_Helper_New>(
      &state);
}
BENCHMARK(BM_TinyComponents_FindConnected_New_Hash)->Range(1, 1 << 19);

// Common logic for Connected benchmarks.
template <typename CompareOrHashT>
int Connected_Helper(int size,
                     ConnectedComponentsFinder<int, CompareOrHashT>* finder) {
  int num_connected = 0;
  for (int i = 0; i < size - 1; ++i) {
    if (finder->Connected(i, i + 1)) {
      ++num_connected;
    }
  }
  return num_connected;
}

// Measures "connected" queries.
template <typename CompareOrHashT>
static void BM_OneComponent_Connected(benchmark::State* state) {
  const int size = state->range(0);
  for (auto _ : *state) {
    ConnectedComponentsFinder<int, CompareOrHashT> finder;
    for (int i = 0; i < size; i++) {
      finder.AddNode(i);
    }

    for (int i = 0; i < size / 2; i++) {
      finder.AddEdge(i, i + 1);
      finder.AddEdge(size - 1 - i, size - 2 - i);
    }

    CHECK_EQ(Connected_Helper(3 * size, &finder), size - 1);
  }
}

static void BM_OneComponent_Connected_Less(benchmark::State& state) {
  BM_OneComponent_Connected<std::less<int>>(&state);
}
BENCHMARK(BM_OneComponent_Connected_Less)->Range(1, 1 << 19);

static void BM_OneComponent_Connected_Hash(benchmark::State& state) {
  BM_OneComponent_Connected<std::hash<int>>(&state);
}
BENCHMARK(BM_OneComponent_Connected_Hash)->Range(1, 1 << 19);

// This one has components of size 3: edges are from 3n to 3n+1, 3n+1 to 3n+2.
template <typename CompareOrHashT>
static void BM_TinyComponents_Connected(benchmark::State* state) {
  const int size = state->range(0);
  for (auto _ : *state) {
    ConnectedComponentsFinder<int, CompareOrHashT> finder;

    for (int i = 0; i < size; i++) {
      finder.AddEdge(3 * i, 3 * i + 1);
    }
    for (int i = 0; i < size; i++) {
      finder.AddEdge(3 * i + 1, 3 * i + 2);
    }

    CHECK_EQ(Connected_Helper(3 * size, &finder), 2 * size);
  }
}

static void BM_TinyComponents_Connected_Less(benchmark::State& state) {
  BM_TinyComponents_Connected<std::less<int>>(&state);
}
BENCHMARK(BM_TinyComponents_Connected_Less)->Range(1, 1 << 19);

static void BM_TinyComponents_Connected_Hash(benchmark::State& state) {
  BM_TinyComponents_Connected<std::hash<int>>(&state);
}
BENCHMARK(BM_TinyComponents_Connected_Hash)->Range(1, 1 << 19);

}  // namespace
