// Copyright 2010-2024 Google LLC
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

#include "ortools/graph/cliques.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/base/logging.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace {

template <typename NodeIndex>
class CliqueReporter {
 public:
  CliqueReporter() : CliqueReporter(std::numeric_limits<int64_t>::max()) {}
  explicit CliqueReporter(int64_t max_cliques)
      : remaining_cliques_(max_cliques) {}

  const std::vector<std::vector<NodeIndex>>& all_cliques() {
    return all_cliques_;
  }
  bool AppendClique(const std::vector<NodeIndex>& new_clique) {
    all_cliques_.push_back(new_clique);
    --remaining_cliques_;
    return remaining_cliques_ <= 0;
  }

  std::function<CliqueResponse(const std::vector<NodeIndex>&)>
  MakeCliqueCallback() {
    return [this](const std::vector<NodeIndex>& clique) {
      return AppendClique(clique) ? CliqueResponse::STOP
                                  : CliqueResponse::CONTINUE;
    };
  }

 private:
  int64_t remaining_cliques_;
  std::vector<std::vector<NodeIndex>> all_cliques_;
};

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
    EXPECT_GE(expected_max_clique_size_, new_clique.size());
    EXPECT_LE(expected_min_clique_size_, new_clique.size());
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

inline bool FullGraph(int /*index1*/, int /*index2*/) { return true; }

inline bool EmptyGraph(int index1, int index2) { return (index1 == index2); }

inline bool MatchingGraph(int index1, int index2) {
  return (index1 / 2 == index2 / 2);
}

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

template <typename NodeIndex>
void RunBronKerboschAlgorithmUntilCompletion(
    BronKerboschAlgorithm<NodeIndex>* bron_kerbosch, int64_t step_size) {
  CHECK_NE(bron_kerbosch, nullptr);
  while (true) {
    auto status = bron_kerbosch->RunIterations(step_size);
    if (status == BronKerboschAlgorithmStatus::COMPLETED) {
      return;
    }
  }
}

TEST(BronKerbosch, CompleteGraph) {
  constexpr int kNumNodes[] = {1, 5, 50, 500, 5000};
  for (const int num_nodes : kNumNodes) {
    auto graph = FullGraph;
    CliqueReporter<int> reporter;
    auto callback =
        absl::bind_front(&CliqueReporter<int>::AppendClique, &reporter);
    operations_research::FindCliques(graph, num_nodes, callback);
    const std::vector<std::vector<int>>& all_cliques = reporter.all_cliques();
    EXPECT_EQ(1, all_cliques.size());
    EXPECT_EQ(num_nodes, all_cliques[0].size());
  }
}

TEST(BronKerboschAlgorithmTest, CompleteGraph) {
  constexpr int kNumNodes[] = {1, 5, 50, 500, 5000};
  for (const int num_nodes : kNumNodes) {
    SCOPED_TRACE(absl::StrCat("num_nodes = ", num_nodes));
    CliqueReporter<int> reporter;
    operations_research::BronKerboschAlgorithm<int> bron_kerbosch(
        FullGraph, num_nodes, reporter.MakeCliqueCallback());
    bron_kerbosch.Run();
    const std::vector<std::vector<int>>& all_cliques = reporter.all_cliques();
    EXPECT_EQ(1, all_cliques.size());
    EXPECT_EQ(num_nodes, all_cliques[0].size());
  }
}

TEST(BronKerboschAlgorithmTest, RandomGraph) {
  constexpr int kNumNodes = 1000;
  constexpr double kArcProbability = 0.1;
  constexpr int kSeed = 123456789;
  constexpr int kExpectedNumCliques = 100485;
  const absl::flat_hash_set<std::pair<int, int>> adjacency_matrix =
      MakeRandomGraphAdjacencyMatrix(kNumNodes, kArcProbability, kSeed);
  auto graph = [&adjacency_matrix](int index1, int index2) {
    return BitmapGraph(adjacency_matrix, index1, index2);
  };
  CliqueSizeVerifier verifier(0, kNumNodes);
  operations_research::BronKerboschAlgorithm<int> bron_kerbosch(
      graph, kNumNodes, verifier.MakeCliqueCallback());
  bron_kerbosch.Run();
  EXPECT_EQ(kExpectedNumCliques, verifier.num_cliques());
}

// Instead of calling BronKerboschAlgorithm::Run once, this test runs
// BronKerboschAlgorithm::RunIteratons(1) until completion to verify that the
// algorithm can be stopped and resumed.
TEST(BronKerboschAlgorithmTest, CompleteGraphUsingRunIterations) {
  constexpr int kNumNodes[] = {1, 5, 50, 500, 5000};
  constexpr int kStepSize = 1;
  for (const int num_nodes : kNumNodes) {
    SCOPED_TRACE(absl::StrCat("num_nodes = ", num_nodes));
    CliqueReporter<int> reporter;
    operations_research::BronKerboschAlgorithm<int> bron_kerbosch(
        FullGraph, num_nodes, reporter.MakeCliqueCallback());
    RunBronKerboschAlgorithmUntilCompletion(&bron_kerbosch, kStepSize);
    const std::vector<std::vector<int>>& all_cliques = reporter.all_cliques();
    EXPECT_EQ(1, all_cliques.size());
    EXPECT_EQ(num_nodes, all_cliques[0].size());
  }
}

// Verifies that BronKerboschAlgorithm compiles correctly also for int64_t.
TEST(BronKerboschAlgorithmTest, CompleteGraphWithInt64) {
  constexpr int64_t kNumNodes[] = {1, 5, 50, 500, 5000};
  constexpr int kStepSize = 1;
  for (const int num_nodes : kNumNodes) {
    SCOPED_TRACE(absl::StrCat("num_nodes = ", num_nodes));
    CliqueReporter<int64_t> reporter;
    operations_research::BronKerboschAlgorithm<int64_t> bron_kerbosch(
        FullGraph, num_nodes, reporter.MakeCliqueCallback());
    RunBronKerboschAlgorithmUntilCompletion(&bron_kerbosch, kStepSize);
    const std::vector<std::vector<int64_t>>& all_cliques =
        reporter.all_cliques();
    EXPECT_EQ(1, all_cliques.size());
    EXPECT_EQ(num_nodes, all_cliques[0].size());
  }
}

TEST(BronKerbosch, EmptyGraph) {
  auto graph = EmptyGraph;
  CliqueReporter<int> reporter;
  auto callback =
      absl::bind_front(&CliqueReporter<int>::AppendClique, &reporter);
  operations_research::FindCliques(graph, 10, callback);
  const std::vector<std::vector<int>>& all_cliques = reporter.all_cliques();
  EXPECT_EQ(10, all_cliques.size());
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(1, all_cliques[i].size());
  }
}

TEST(BronKerboschAlgorithmTest, EmptyGraph) {
  constexpr int kNumNodes[] = {1, 5, 50, 500, 5000};
  for (const int num_nodes : kNumNodes) {
    SCOPED_TRACE(absl::StrCat("num_nodes = ", num_nodes));
    CliqueReporter<int> reporter;
    operations_research::BronKerboschAlgorithm<int> bron_kerbosch(
        EmptyGraph, num_nodes, reporter.MakeCliqueCallback());
    bron_kerbosch.Run();
    const std::vector<std::vector<int>>& all_cliques = reporter.all_cliques();
    EXPECT_EQ(num_nodes, all_cliques.size());
    for (const std::vector<int>& clique : all_cliques) {
      EXPECT_EQ(1, clique.size());
    }
  }
}

TEST(BronKerboschAlgorithmTest, EmptyGraphUsingRunIterations) {
  constexpr int kNumNodes[] = {1, 5, 50, 500, 5000};
  constexpr int kStepSize = 1;
  for (const int num_nodes : kNumNodes) {
    SCOPED_TRACE(absl::StrCat("num_nodes = ", num_nodes));
    CliqueReporter<int> reporter;
    operations_research::BronKerboschAlgorithm<int> bron_kerbosch(
        EmptyGraph, num_nodes, reporter.MakeCliqueCallback());
    RunBronKerboschAlgorithmUntilCompletion(&bron_kerbosch, kStepSize);
    const std::vector<std::vector<int>>& all_cliques = reporter.all_cliques();
    EXPECT_EQ(num_nodes, all_cliques.size());
    for (const std::vector<int>& clique : all_cliques) {
      EXPECT_EQ(1, clique.size());
    }
  }
}

// Verifies that the return value from the clique callback is taken into account
// properly. The clique reporter in this case accepts at most three cliques.
TEST(BronKerboschAlgorithmTest, EmptyGraphWithEarlyTermination) {
  constexpr int kNumNodes[] = {1, 5, 50, 500, 5000};
  constexpr int kMaxCliques = 3;
  for (const int num_nodes : kNumNodes) {
    SCOPED_TRACE(absl::StrCat("num_nodes = ", num_nodes));
    CliqueReporter<int> reporter(kMaxCliques);
    operations_research::BronKerboschAlgorithm<int> bron_kerbosch(
        EmptyGraph, num_nodes, reporter.MakeCliqueCallback());
    bron_kerbosch.Run();
    const std::vector<std::vector<int>>& all_cliques = reporter.all_cliques();
    EXPECT_EQ(std::min(kMaxCliques, num_nodes), all_cliques.size());
    for (const std::vector<int>& clique : all_cliques) {
      EXPECT_EQ(1, clique.size());
    }
  }
}

// Verifies that the algorithm can be resumed after it was stopped due to the
// clique callback returning BronKerboschAlgorithmStatus::STOP.
TEST(BronKerboschAlgorithmTest, EmptyGraphStopAfterEveryClique) {
  constexpr int kNumNodes[] = {1, 5, 50, 500, 5000};
  for (const int num_nodes : kNumNodes) {
    SCOPED_TRACE(absl::StrCat("num_nodes = ", num_nodes));
    // NOTE(user): Since we decrement the remaining clique counter every
    // time, it will be negative for the most of the time. However, it will not
    // prevent the Bron-Kerbosch algorithm from running, it will just stop it
    // after every clique it encounters.
    CliqueReporter<int> reporter(1);
    operations_research::BronKerboschAlgorithm<int> bron_kerbosch(
        EmptyGraph, num_nodes, reporter.MakeCliqueCallback());
    int num_iterations = 0;
    while (bron_kerbosch.Run() != BronKerboschAlgorithmStatus::COMPLETED) {
      ++num_iterations;
    }
    const std::vector<std::vector<int>>& all_cliques = reporter.all_cliques();
    EXPECT_EQ(all_cliques.size(), num_iterations);
    EXPECT_EQ(num_nodes, all_cliques.size());
    for (const std::vector<int>& clique : all_cliques) {
      EXPECT_EQ(1, clique.size());
    }
  }
}

TEST(BronKerbosch, MatchingGraph) {
  auto graph = MatchingGraph;
  CliqueReporter<int> reporter;
  auto callback =
      absl::bind_front(&CliqueReporter<int>::AppendClique, &reporter);
  operations_research::FindCliques(graph, 10, callback);
  const std::vector<std::vector<int>>& all_cliques = reporter.all_cliques();
  EXPECT_EQ(5, all_cliques.size());
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(2, all_cliques[i].size());
    EXPECT_EQ(all_cliques[i][0] / 2, all_cliques[i][1] / 2);
  }
}

TEST(BronKerboschAlgorithmTest, MatchingGraph) {
  constexpr int kNumNodes[] = {2, 10, 100, 1000};
  for (const int num_nodes : kNumNodes) {
    SCOPED_TRACE(absl::StrCat("num_nodes = ", num_nodes));
    CliqueReporter<int> reporter;
    operations_research::BronKerboschAlgorithm<int> bron_kerbosch(
        MatchingGraph, num_nodes, reporter.MakeCliqueCallback());
    bron_kerbosch.Run();
    const std::vector<std::vector<int>>& all_cliques = reporter.all_cliques();
    EXPECT_EQ(num_nodes / 2, all_cliques.size());
    for (const std::vector<int>& clique : all_cliques) {
      EXPECT_EQ(2, clique.size());
      EXPECT_EQ(clique[0] / 2, clique[1] / 2);
    }
  }
}

TEST(BronKerbosch, ModuloGraph5) {
  constexpr int kNumPartitions = 5;
  auto graph = absl::bind_front(ModuloGraph, kNumPartitions);
  CliqueReporter<int> reporter;
  auto callback =
      absl::bind_front(&CliqueReporter<int>::AppendClique, &reporter);
  operations_research::FindCliques(graph, 40, callback);
  const std::vector<std::vector<int>>& all_cliques = reporter.all_cliques();
  EXPECT_EQ(5, all_cliques.size());
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(8, all_cliques[i].size());
  }
}

TEST(BronKerboschAlgorithmTest, ModuloGraph) {
  constexpr int kNumPartitions[] = {1, 5, 10, 50, 100};
  constexpr int kNumNodes = 200;
  for (const int num_partitions : kNumPartitions) {
    SCOPED_TRACE(absl::StrCat("num_partitions = ", num_partitions));
    const auto graph = [num_partitions](int index1, int index2) {
      return ModuloGraph(num_partitions, index1, index2);
    };

    CliqueReporter<int> reporter;
    operations_research::BronKerboschAlgorithm<int> bron_kerbosch(
        graph, kNumNodes, reporter.MakeCliqueCallback());
    bron_kerbosch.Run();
    const std::vector<std::vector<int>>& all_cliques = reporter.all_cliques();
    EXPECT_EQ(num_partitions, all_cliques.size());
    for (const std::vector<int>& clique : all_cliques) {
      EXPECT_EQ(kNumNodes / num_partitions, clique.size());
    }
  }
}

TEST(BronKerbosch, CompleteGraphCover) {
  auto graph = FullGraph;
  CliqueReporter<int> reporter;
  auto callback =
      absl::bind_front(&CliqueReporter<int>::AppendClique, &reporter);
  operations_research::CoverArcsByCliques(graph, 10, callback);
  const std::vector<std::vector<int>>& all_cliques = reporter.all_cliques();
  EXPECT_EQ(1, all_cliques.size());
  EXPECT_EQ(10, all_cliques[0].size());
}

TEST(WeightedBronKerboschBitsetAlgorithmTest, CompleteGraph) {
  const int num_nodes = 1000;
  WeightedBronKerboschBitsetAlgorithm algo;
  algo.Initialize(num_nodes);
  for (int i = 0; i < num_nodes; ++i) {
    for (int j = i + 1; j < num_nodes; ++j) {
      algo.AddEdge(i, j);
    }
  }
  std::vector<std::vector<int>> cliques = algo.Run();
  EXPECT_EQ(cliques.size(), 1);
  for (const std::vector<int>& clique : cliques) {
    EXPECT_EQ(num_nodes, clique.size());
  }
}

TEST(WeightedBronKerboschBitsetAlgorithmTest, ImplicationGraphClosure) {
  const int num_nodes = 10;
  WeightedBronKerboschBitsetAlgorithm algo;
  algo.Initialize(num_nodes);
  for (int i = 0; i + 2 < num_nodes; i += 2) {
    const int j = i + 2;
    algo.AddEdge(i, j ^ 1);  // i => j
  }
  algo.TakeTransitiveClosureOfImplicationGraph();
  for (int i = 0; i < num_nodes; ++i) {
    for (int j = 0; j < num_nodes; ++j) {
      if (i % 2 == 0 && j % 2 == 0) {
        if (j > i) {
          EXPECT_TRUE(algo.HasEdge(i, j ^ 1));
        } else {
          EXPECT_FALSE(algo.HasEdge(i, j ^ 1));
        }
      }
    }
  }
}

TEST(BronKerbosch, EmptyGraphCover) {
  auto graph = EmptyGraph;
  CliqueReporter<int> reporter;
  auto callback =
      absl::bind_front(&CliqueReporter<int>::AppendClique, &reporter);
  operations_research::CoverArcsByCliques(graph, 10, callback);
  const std::vector<std::vector<int>>& all_cliques = reporter.all_cliques();
  EXPECT_EQ(0, all_cliques.size());
}

TEST(BronKerbosch, MatchingGraphCover) {
  auto graph = MatchingGraph;
  CliqueReporter<int> reporter;
  auto callback =
      absl::bind_front(&CliqueReporter<int>::AppendClique, &reporter);
  operations_research::CoverArcsByCliques(graph, 10, callback);
  const std::vector<std::vector<int>>& all_cliques = reporter.all_cliques();
  EXPECT_EQ(5, all_cliques.size());
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(2, all_cliques[i].size());
    EXPECT_EQ(all_cliques[i][0] / 2, all_cliques[i][1] / 2);
  }
}

TEST(BronKerbosch, ModuloGraph5Cover) {
  constexpr int kNumPartitions = 5;
  auto graph = absl::bind_front(ModuloGraph, kNumPartitions);
  CliqueReporter<int> reporter;
  auto callback =
      absl::bind_front(&CliqueReporter<int>::AppendClique, &reporter);
  operations_research::CoverArcsByCliques(graph, 40, callback);
  const std::vector<std::vector<int>>& all_cliques = reporter.all_cliques();
  EXPECT_EQ(5, all_cliques.size());
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(8, all_cliques[i].size());
  }
}

TEST(BronKerboschAlgorithmTest, FullKPartiteGraph) {
  const int kNumPartitions[] = {2, 3, 4, 5, 6, 7};
  for (const int num_partitions : kNumPartitions) {
    SCOPED_TRACE(absl::StrCat("num_partitions = ", num_partitions));
    const int num_nodes = num_partitions * num_partitions;

    const auto graph = [num_partitions](int index1, int index2) {
      return FullKPartiteGraph(num_partitions, index1, index2);
    };
    CliqueReporter<int> reporter;
    BronKerboschAlgorithm<int> bron_kerbosch(graph, num_nodes,
                                             reporter.MakeCliqueCallback());
    bron_kerbosch.Run();

    EXPECT_LT(1, reporter.all_cliques().size());
    for (const std::vector<int>& clique : reporter.all_cliques()) {
      EXPECT_EQ(num_partitions, clique.size());
    }
  }
}

TEST(WeightedBronKerboschBitsetAlgorithmTest, FullKPartiteGraph) {
  const int kNumPartitions[] = {2, 3, 4, 5, 6, 7};
  for (const int num_partitions : kNumPartitions) {
    SCOPED_TRACE(absl::StrCat("num_partitions = ", num_partitions));
    WeightedBronKerboschBitsetAlgorithm algo;

    const int num_nodes = num_partitions * num_partitions;
    algo.Initialize(num_nodes);

    for (int i = 0; i < num_nodes; ++i) {
      for (int j = i + 1; j < num_nodes; ++j) {
        if (FullKPartiteGraph(num_partitions, i, j)) algo.AddEdge(i, j);
      }
    }

    std::vector<std::vector<int>> cliques = algo.Run();
    EXPECT_EQ(cliques.size(), pow(num_partitions, num_partitions));
    for (const std::vector<int>& clique : cliques) {
      EXPECT_EQ(num_partitions, clique.size());
    }
  }
}

TEST(WeightedBronKerboschBitsetAlgorithmTest, ModuloGraph) {
  int num_partitions = 50;
  int partition_size = 100;
  WeightedBronKerboschBitsetAlgorithm algo;

  const int num_nodes = num_partitions * partition_size;
  algo.Initialize(num_nodes);

  for (int i = 0; i < num_nodes; ++i) {
    for (int j = i + 1; j < num_nodes; ++j) {
      if (ModuloGraph(num_partitions, i, j)) algo.AddEdge(i, j);
    }
  }

  std::vector<std::vector<int>> cliques = algo.Run();
  EXPECT_EQ(cliques.size(), num_partitions);
  for (const std::vector<int>& clique : cliques) {
    EXPECT_EQ(partition_size, clique.size());
  }
}

// The following two tests run the Bron-Kerbosch algorithm with wall time
// limit and deterministic time limit. They use a full 15-partite graph with
// a one second time limit.
//
// The graph has approximately 2^58.6 maximal cliques, so they could be in
// theory all enumerated within the std::numeric_limits<int64_t>::max()
// iteration limit, but their number is too large that enumerating them would
// take tens to hundreds of years with the current technology (as of August
// 2015). As a result, the algorithm can end in the INTERRUPTED state only when
// the time limit is reached.
TEST(BronKerboschAlgorithmTest, WallTimeLimit) {
  const int kNumPartitions = 15;
  const int kNumNodes = kNumPartitions * kNumPartitions;
  const int kExpectedCliqueSize = kNumPartitions;
  const double kTimeLimitSeconds = 1.0;
  absl::SetFlag(&FLAGS_time_limit_use_usertime, true);

  TimeLimit time_limit(kTimeLimitSeconds);
  const auto graph = [](int index1, int index2) {
    return FullKPartiteGraph(kNumPartitions, index1, index2);
  };
  CliqueSizeVerifier verifier(kExpectedCliqueSize, kExpectedCliqueSize);
  BronKerboschAlgorithm<int> bron_kerbosch(graph, kNumNodes,
                                           verifier.MakeCliqueCallback());
  const BronKerboschAlgorithmStatus status =
      bron_kerbosch.RunWithTimeLimit(&time_limit);
  EXPECT_EQ(BronKerboschAlgorithmStatus::INTERRUPTED, status);
  EXPECT_TRUE(time_limit.LimitReached());
}

TEST(BronKerboschAlgorithmTest, DeterministicTimeLimit) {
  const int kNumPartitions = 15;
  const int kNumNodes = kNumPartitions * kNumPartitions;
  const int kExpectedCliqueSize = kNumPartitions;
  const double kDeterministicLimit = 1.0;
  absl::SetFlag(&FLAGS_time_limit_use_usertime, true);

  std::unique_ptr<TimeLimit> time_limit =
      TimeLimit::FromDeterministicTime(kDeterministicLimit);
  const auto graph = [](int index1, int index2) {
    return FullKPartiteGraph(kNumPartitions, index1, index2);
  };
  CliqueSizeVerifier verifier(kExpectedCliqueSize, kExpectedCliqueSize);
  BronKerboschAlgorithm<int> bron_kerbosch(graph, kNumNodes,
                                           verifier.MakeCliqueCallback());
  const BronKerboschAlgorithmStatus status =
      bron_kerbosch.RunWithTimeLimit(time_limit.get());
  EXPECT_EQ(BronKerboschAlgorithmStatus::INTERRUPTED, status);
  EXPECT_TRUE(time_limit->LimitReached());
}

// A benchmark that finds all maximal cliques in a modulo graph of the given
// size.
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
  EXPECT_EQ(state.max_iterations * kExpectedNumCliques, verifier.num_cliques());
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
  EXPECT_EQ(state.max_iterations * kExpectedNumCliques, verifier.num_cliques());
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
    EXPECT_EQ(cliques.size(), kExpectedNumCliques);
    for (const std::vector<int>& clique : cliques) {
      EXPECT_EQ(kExpectedCliqueSize, clique.size());
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
  EXPECT_EQ(state.max_iterations * kExpectedNumCliques, verifier.num_cliques());
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
  EXPECT_EQ(state.max_iterations * kExpectedNumCliques, verifier.num_cliques());
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
                                     GTEST_FLAG_GET(random_seed));
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
