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
#include <functional>
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/random/distributions.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "ortools/graph/cliques.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace {

// An object used to build callbacks for FindCliques. Verifies that the size of
// the cliques discovered by the algorithm fits into the specified bounds, and
// counts the cliques it receives.
class CliqueSizeVerifier {
 public:
  CliqueSizeVerifier(int expected_min_clique_size, int expected_max_clique_size)
      : expected_min_clique_size_(expected_min_clique_size),
        expected_max_clique_size_(expected_max_clique_size),
        num_cliques_(0) {}

  int64_t num_cliques() const { return num_cliques_; }

  bool AppendClique(absl::Span<const int> new_clique) {
    CHECK_GE(expected_max_clique_size_, new_clique.size());
    CHECK_LE(expected_min_clique_size_, new_clique.size());
    ++num_cliques_;
    return false;
  }

  std::function<CliqueResponse(const std::vector<int>&)> MakeCliqueCallback() {
    return [this](absl::Span<const int> clique) {
      return AppendClique(clique) ? CliqueResponse::STOP
                                  : CliqueResponse::CONTINUE;
    };
  }

 private:
  const int expected_min_clique_size_;
  const int expected_max_clique_size_;
  int64_t num_cliques_;
};

inline bool ModuloGraph(int num_partitions, int index1, int index2) {
  return (index1 % num_partitions == index2 % num_partitions);
}

inline bool FullKPartiteGraph(int num_partitions, int index1, int index2) {
  return index1 % num_partitions != index2 % num_partitions;
}

// A graph that is represented as an adjacency matrix stored in a hash set:
// there is an arc (i, j) if and only if 'arcs' contains the pair
// (min(i, j), max(i, j)).
inline bool BitmapGraph(const absl::flat_hash_set<std::pair<int, int>>& arcs,
                        int index1, int index2) {
  const int lower = std::min(index1, index2);
  const int higher = std::max(index1, index2);
  return arcs.contains(std::make_pair(lower, higher));
}

// Generates a random graph with 'num_vertices' vertices, where each arc exists
// with probability 'arc_probability'. The graph is represented as an adjacency
// matrix stored in a set. There is an unoriented arc (i, j) in the graph if and
// only if the returned set contains the pair (min(i, j), max(i, j)).
absl::flat_hash_set<std::pair<int, int>> MakeRandomGraphAdjacencyMatrix(
    int num_vertices, float arc_probability, int seed) {
  std::mt19937 random(seed);
  absl::flat_hash_set<std::pair<int, int>> arcs;
  for (int v1 = 0; v1 < num_vertices; ++v1) {
    for (int v2 = v1 + 1; v2 < num_vertices; ++v2) {
      const bool edge_exists = absl::Bernoulli(random, arc_probability);
      if (edge_exists) {
        arcs.insert(std::make_pair(v1, v2));
      }
    }
  }
  return arcs;
}

void BM_FindCliquesInModuloGraph(benchmark::State& state) {
  int num_partitions = state.range(0);
  int partition_size = state.range(1);

  const int kExpectedNumCliques = num_partitions;
  const int kExpectedCliqueSize = partition_size;
  const int kGraphSize = num_partitions * partition_size;
  CliqueSizeVerifier verifier(kExpectedCliqueSize, kExpectedCliqueSize);

  for (auto _ : state) {
    auto graph = absl::bind_front(ModuloGraph, num_partitions);
    auto callback =
        absl::bind_front(&CliqueSizeVerifier::AppendClique, &verifier);

    operations_research::FindCliques(graph, kGraphSize, callback);
  }
  CHECK_EQ(state.max_iterations * kExpectedNumCliques, verifier.num_cliques());
}

BENCHMARK(BM_FindCliquesInModuloGraph)
    ->ArgPair(5, 1000)
    ->ArgPair(10, 500)
    ->ArgPair(50, 100)
    ->ArgPair(100, 50)
    ->ArgPair(500, 10)
    ->ArgPair(1000, 5);

void BM_FindCliquesInModuloGraphWithBronKerboschAlgorithm(
    benchmark::State& state) {
  int num_partitions = state.range(0);
  int partition_size = state.range(1);
  const int kExpectedNumCliques = num_partitions;
  const int kExpectedCliqueSize = partition_size;
  const int kGraphSize = num_partitions * partition_size;
  CliqueSizeVerifier verifier(kExpectedCliqueSize, kExpectedCliqueSize);
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();

  auto graph = [num_partitions](int index1, int index2) {
    return ModuloGraph(num_partitions, index1, index2);
  };

  for (auto _ : state) {
    BronKerboschAlgorithm<int> bron_kerbosch(graph, kGraphSize,
                                             verifier.MakeCliqueCallback());
    bron_kerbosch.RunWithTimeLimit(time_limit.get());
  }
  CHECK_EQ(state.max_iterations * kExpectedNumCliques, verifier.num_cliques());
  LOG(INFO) << time_limit->DebugString();
}

BENCHMARK(BM_FindCliquesInModuloGraphWithBronKerboschAlgorithm)
    ->ArgPair(5, 1000)
    ->ArgPair(10, 500)
    ->ArgPair(50, 100)
    ->ArgPair(100, 50)
    ->ArgPair(500, 10)
    ->ArgPair(1000, 5);

void BM_FindCliquesInModuloGraphWithBitsetBK(benchmark::State& state) {
  int num_partitions = state.range(0);
  int partition_size = state.range(1);
  const int kExpectedNumCliques = num_partitions;
  const int kExpectedCliqueSize = partition_size;
  const int num_nodes = num_partitions * partition_size;
  for (auto _ : state) {
    WeightedBronKerboschBitsetAlgorithm algo;
    algo.Initialize(num_nodes);
    for (int i = 0; i < num_nodes; ++i) {
      for (int j = i + 1; j < num_nodes; ++j) {
        if (ModuloGraph(num_partitions, i, j)) algo.AddEdge(i, j);
      }
    }

    std::vector<std::vector<int>> cliques = algo.Run();
    CHECK_EQ(cliques.size(), kExpectedNumCliques);
    for (const std::vector<int>& clique : cliques) {
      CHECK_EQ(kExpectedCliqueSize, clique.size());
    }
  }
}

BENCHMARK(BM_FindCliquesInModuloGraphWithBitsetBK)
    ->ArgPair(5, 1000)
    ->ArgPair(10, 500)
    ->ArgPair(50, 100)
    ->ArgPair(100, 50)
    ->ArgPair(500, 10)
    ->ArgPair(1000, 5);

// A benchmark that finds all maximal cliques in a 7-partite graph (a graph
// where the nodes are divided into 7 groups of size 7; each node is connected
// to all nodes in other groups but to no node in the same group). This graph
// contains a large number of relatively small cliques.
void BM_FindCliquesInFull7PartiteGraph(benchmark::State& state) {
  constexpr int kNumPartitions = 7;
  constexpr int kExpectedNumCliques = 7 * 7 * 7 * 7 * 7 * 7 * 7;
  constexpr int kExpectedCliqueSize = kNumPartitions;
  CliqueSizeVerifier verifier(kExpectedCliqueSize, kExpectedCliqueSize);

  for (auto _ : state) {
    auto graph = absl::bind_front(FullKPartiteGraph, kNumPartitions);
    auto callback =
        absl::bind_front(&CliqueSizeVerifier::AppendClique, &verifier);

    operations_research::FindCliques(graph, kNumPartitions * kNumPartitions,
                                     callback);
  }
  CHECK_EQ(state.max_iterations * kExpectedNumCliques, verifier.num_cliques());
}

BENCHMARK(BM_FindCliquesInFull7PartiteGraph);

void BM_FindCliquesInFullKPartiteGraphWithBronKerboschAlgorithm(
    benchmark::State& state) {
  int num_partitions = state.range(0);
  const int kExpectedNumCliques = std::pow(num_partitions, num_partitions);
  const int kExpectedCliqueSize = num_partitions;

  const auto graph = [num_partitions](int index1, int index2) {
    return FullKPartiteGraph(num_partitions, index1, index2);
  };
  CliqueSizeVerifier verifier(kExpectedCliqueSize, kExpectedCliqueSize);
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();

  for (auto _ : state) {
    BronKerboschAlgorithm<int> bron_kerbosch(
        graph, num_partitions * num_partitions, verifier.MakeCliqueCallback());
    bron_kerbosch.RunWithTimeLimit(time_limit.get());
  }
  CHECK_EQ(state.max_iterations * kExpectedNumCliques, verifier.num_cliques());
  LOG(INFO) << time_limit->DebugString();
}

BENCHMARK(BM_FindCliquesInFullKPartiteGraphWithBronKerboschAlgorithm)
    ->Arg(5)
    ->Arg(6)
    ->Arg(7);

void BM_FindCliquesInRandomGraphWithBronKerboschAlgorithm(
    benchmark::State& state) {
  int num_nodes = state.range(0);
  int arc_probability_permille = state.range(1);
  const double arc_probability = arc_probability_permille / 1000.0;
  const absl::flat_hash_set<std::pair<int, int>> adjacency_matrix =
      MakeRandomGraphAdjacencyMatrix(num_nodes, arc_probability,
                                     12345);  // Fixed seed
  const auto graph = [adjacency_matrix](int index1, int index2) {
    return BitmapGraph(adjacency_matrix, index1, index2);
  };
  CliqueSizeVerifier verifier(0, num_nodes);
  std::unique_ptr<TimeLimit> time_limit = TimeLimit::Infinite();
  for (auto _ : state) {
    BronKerboschAlgorithm<int> bron_kerbosch(graph, num_nodes,
                                             verifier.MakeCliqueCallback());
    bron_kerbosch.RunWithTimeLimit(time_limit.get());
  }
  LOG(INFO) << time_limit->DebugString();
}

BENCHMARK(BM_FindCliquesInRandomGraphWithBronKerboschAlgorithm)
    ->ArgPair(50, 800)
    ->ArgPair(100, 500)
    ->ArgPair(200, 100)
    ->ArgPair(1000, 10)
    ->ArgPair(1000, 20)
    ->ArgPair(1000, 50)
    ->ArgPair(1000, 100)
    ->ArgPair(10000, 1);

}  // namespace
}  // namespace operations_research
