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

#include "ortools/algorithms/find_graph_symmetries.h"

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <numeric>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/numeric/bits.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/algorithms/dynamic_partition.h"
#include "ortools/algorithms/dynamic_permutation.h"
#include "ortools/algorithms/sparse_permutation.h"
#include "ortools/base/dump_vars.h"
#include "ortools/base/gmock.h"
#include "ortools/base/helpers.h"
#include "ortools/base/map_util.h"
#include "ortools/base/path.h"
#include "ortools/graph/graph_io.h"
#include "ortools/graph/util.h"

namespace operations_research {
namespace {

using Graph = GraphSymmetryFinder::Graph;
using ::testing::AnyOf;
using ::testing::DoubleEq;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;
using ::util::GraphIsSymmetric;

// Shortcut that calls RecursivelyRefinePartitionByAdjacency() on all nodes
// of a graph, and outputs the resulting partition.
std::string FullyRefineGraph(const std::vector<std::pair<int, int>>& arcs) {
  Graph graph;
  for (const std::pair<int, int>& arc : arcs) {
    graph.AddArc(arc.first, arc.second);
  }
  graph.Build();
  GraphSymmetryFinder symmetry_finder(graph, GraphIsSymmetric(graph));
  DynamicPartition partition(graph.num_nodes());
  symmetry_finder.RecursivelyRefinePartitionByAdjacency(0, &partition);
  return partition.DebugString(/*sort_parts_lexicographically=*/true);
}

TEST(RecursivelyRefinePartitionByAdjacencyTest, DoublyLinkedChain) {
  // Graph: 0 <-> 1 <-> 2 <-> 3 <-> 4
  EXPECT_EQ(
      "0 4 | 1 3 | 2",
      FullyRefineGraph(
          {{0, 1}, {1, 0}, {1, 2}, {2, 1}, {2, 3}, {3, 2}, {3, 4}, {4, 3}}));
}

TEST(RecursivelyRefinePartitionByAdjacencyTest, Singleton) {
  EXPECT_EQ("0", FullyRefineGraph({{0, 0}}));
}

TEST(RecursivelyRefinePartitionByAdjacencyTest, Clique) {
  EXPECT_EQ("0 1 2 3", FullyRefineGraph({{0, 1},
                                         {0, 2},
                                         {0, 3},
                                         {1, 0},
                                         {1, 2},
                                         {1, 3},
                                         {2, 0},
                                         {2, 1},
                                         {2, 3},
                                         {3, 0},
                                         {3, 1},
                                         {3, 2}}));
}

TEST(RecursivelyRefinePartitionByAdjacencyTest, CyclesOfDifferentLength) {
  // The 1-2-3 and 4-5 cycles aren't differentiated: that's precisely the
  // limitation of the refinement algorithm. All these nodes have 1 incoming and
  // 1 outgoing arc.
  EXPECT_EQ("0 | 1 2 3 4 5",
            FullyRefineGraph({{1, 2}, {2, 3}, {3, 1}, {4, 5}, {5, 4}}));
}

TEST(RecursivelyRefinePartitionByAdjacencyTest, Chain) {
  EXPECT_EQ("0 | 1 | 2 | 3 | 4",
            FullyRefineGraph({{0, 1}, {1, 2}, {2, 3}, {3, 4}}));
}

TEST(RecursivelyRefinePartitionByAdjacencyTest, FlowerOfCycles) {
  // A bunch of cycles of different or same sizes that all share node 0.
  // Note(user): this is only fully refined because we refine both on outwards
  // and inward adjacency of node parts.
  EXPECT_EQ("0 | 1 4 | 2 5 | 3 6 | 7 | 8 | 9", FullyRefineGraph({
                                                   {0, 1},
                                                   {1, 0},  // 0-1
                                                   {0, 2},
                                                   {2, 3},
                                                   {3, 0},  // 0-2-3
                                                   {0, 4},
                                                   {4, 0},  // 0-4
                                                   {0, 5},
                                                   {5, 6},
                                                   {6, 0},  // 0-5-6
                                                   {0, 7},
                                                   {7, 8},
                                                   {8, 9},
                                                   {9, 0},  // 0-7-8-9
                                               }));
}

TEST(GraphSymmetryFinderTest, EmptyGraph) {
  for (bool is_undirected : {true, false}) {
    SCOPED_TRACE(DUMP_VARS(is_undirected));
    Graph empty_graph;
    empty_graph.Build();
    GraphSymmetryFinder symmetry_finder(empty_graph, is_undirected);

    EXPECT_TRUE(symmetry_finder.IsGraphAutomorphism(DynamicPermutation(0)));

    std::vector<int> node_equivalence_classes_io;
    std::vector<std::unique_ptr<SparsePermutation>> generators;
    std::vector<int> factorized_automorphism_group_size;
    ASSERT_OK(symmetry_finder.FindSymmetries(
        &node_equivalence_classes_io, &generators,
        &factorized_automorphism_group_size));
    EXPECT_THAT(node_equivalence_classes_io, IsEmpty());
    EXPECT_THAT(generators, IsEmpty());
    EXPECT_THAT(factorized_automorphism_group_size, IsEmpty());
  }
}

TEST(GraphSymmetryFinderTest, EmptyGraphAndDoNothing) {
  Graph empty_graph;
  empty_graph.Build();
  GraphSymmetryFinder symmetry_finder(empty_graph, /*is_undirected=*/true);
}

class IsGraphAutomorphismTest : public testing::Test {
 protected:
  void ExpectIsGraphAutomorphism(
      int num_nodes, const std::vector<std::pair<int, int>>& graph_arcs,
      absl::Span<const std::vector<int>> permutation_cycles,
      bool expected_is_automorphism) {
    Graph graph(num_nodes, graph_arcs.size());
    for (const std::pair<int, int>& arc : graph_arcs) {
      graph.AddArc(arc.first, arc.second);
    }
    graph.Build();
    GraphSymmetryFinder symmetry_finder(graph, GraphIsSymmetric(graph));

    DynamicPermutation permutation(graph.num_nodes());
    for (const std::vector<int>& cycle : permutation_cycles) {
      std::vector<int> shifted_cycle(cycle.begin() + 1, cycle.end());
      shifted_cycle.push_back(cycle[0]);
      permutation.AddMappings(cycle, shifted_cycle);
    }

    const bool is_automorphism =
        symmetry_finder.IsGraphAutomorphism(permutation);
    EXPECT_EQ(expected_is_automorphism, is_automorphism)
        << "\nWith graph: "
        << absl::StrJoin(graph_arcs, ", ", absl::PairFormatter("->"))
        << "\nAnd permutation: " << permutation.DebugString();
  }
};

TEST_F(IsGraphAutomorphismTest, IsolatedNodes) {
  ExpectIsGraphAutomorphism(3, {}, {{0, 1}}, true);
  ExpectIsGraphAutomorphism(3, {}, {{1, 2}}, true);
  ExpectIsGraphAutomorphism(3, {}, {{0, 2}}, true);
  ExpectIsGraphAutomorphism(3, {}, {{0, 1, 2}}, true);
}

TEST_F(IsGraphAutomorphismTest, DirectedCyclesOfDifferentLengths) {
  const std::vector<std::pair<int, int>> graph = {
      {0, 0},                                    // Length 1
      {1, 2}, {2, 1},                            // Length 2
      {3, 4}, {4, 5}, {5, 3},                    // Length 3
      {6, 7}, {7, 8}, {8, 9}, {9, 10}, {10, 6},  // Length 5
  };
  ExpectIsGraphAutomorphism(12, graph, {{0, 10}}, false);
  ExpectIsGraphAutomorphism(12, graph, {{0, 1}}, false);
  ExpectIsGraphAutomorphism(12, graph, {{1, 2}}, true);
  ExpectIsGraphAutomorphism(12, graph, {{3, 4}}, false);
  ExpectIsGraphAutomorphism(12, graph, {{1, 3}}, false);
  ExpectIsGraphAutomorphism(12, graph, {{6, 7, 8}}, false);
  ExpectIsGraphAutomorphism(12, graph, {{6, 8}, {7, 9}}, false);
  ExpectIsGraphAutomorphism(12, graph, {{6, 7, 8, 9}}, false);
  ExpectIsGraphAutomorphism(12, graph, {{6, 7, 8, 9, 10}}, true);
  ExpectIsGraphAutomorphism(12, graph, {{1, 2}, {3, 4, 5}, {6, 7, 8, 9, 10}},
                            true);
  ExpectIsGraphAutomorphism(12, graph, {{1, 2}, {3, 4, 5}, {0, 7}}, false);
}

TEST_F(IsGraphAutomorphismTest, Cliques) {
  const std::vector<std::pair<int, int>> graph = {
      {0, 0},                          // 1
      {1, 1}, {1, 2}, {2, 1}, {2, 2},  // 2
      {3, 3}, {3, 4}, {3, 5}, {4, 3}, {4, 4},
      {4, 5}, {5, 3}, {5, 4}, {5, 5}  // 3
  };
  ExpectIsGraphAutomorphism(6, graph, {{1, 2}}, true);
  ExpectIsGraphAutomorphism(6, graph, {{3, 4}}, true);
  ExpectIsGraphAutomorphism(6, graph, {{4, 5}}, true);
  ExpectIsGraphAutomorphism(6, graph, {{3, 4, 5}}, true);
  ExpectIsGraphAutomorphism(6, graph, {{1, 3}}, false);
}

TEST_F(IsGraphAutomorphismTest, UndirectedChains) {
  const std::vector<std::pair<int, int>> graph = {
      {0, 1}, {1, 0},  // Length 2
      {2, 3}, {3, 4}, {4, 5}, {5, 6},
      {6, 5}, {5, 4}, {4, 3}, {3, 2},  // Length 5
  };
  ExpectIsGraphAutomorphism(7, graph, {{0, 1}}, true);
  ExpectIsGraphAutomorphism(7, graph, {{2, 6}, {3, 5}}, true);
  ExpectIsGraphAutomorphism(7, graph, {{2, 6}}, false);
}

class FindSymmetriesTest : public ::testing::Test {
 protected:
  std::vector<int> GetDensePermutation(const SparsePermutation& permutation) {
    std::vector<int> dense_perm(permutation.Size(), -1);
    for (int i = 0; i < dense_perm.size(); ++i) dense_perm[i] = i;
    for (int c = 0; c < permutation.NumCycles(); ++c) {
      // TODO(user): use the global element->image iterator when it exists.
      int prev = *(permutation.Cycle(c).end() - 1);
      for (const int e : permutation.Cycle(c)) {
        dense_perm[prev] = e;
        prev = e;
      }
    }
    return dense_perm;
  }

  std::vector<int> ComposePermutations(absl::Span<const int> p1,
                                       absl::Span<const int> p2) {
    CHECK_EQ(p1.size(), p2.size());
    std::vector<int> composed(p1.size(), -1);
    for (int i = 0; i < p1.size(); ++i) composed[i] = p1[p2[i]];
    return composed;
  }

  // Brute-force compute the size of the group by computing all of its elements,
  // with some basic, non-through EXPECT(..) that check that each generator
  // does make the group grow.
  int ComputePermutationGroupSizeAndVerifyBasicIrreductibility(
      absl::Span<const std::unique_ptr<SparsePermutation>> generators) {
    if (generators.empty()) return 1;  // The identity.
    const int num_nodes = generators[0]->Size();
    // The group only contains the identity at first.
    std::set<std::vector<int>> permutation_group;
    permutation_group.insert(GetDensePermutation(SparsePermutation(num_nodes)));
    // For each generator...
    for (int i = 0; i < generators.size(); ++i) {
      const SparsePermutation& perm = *generators[i];
      const std::vector<int> dense_perm = GetDensePermutation(perm);
      auto insertion_result = permutation_group.insert(dense_perm);
      if (!insertion_result.second) {
        ADD_FAILURE() << "Unneeded generator: " << perm.DebugString();
        continue;
      }
      std::vector<const std::vector<int>*> new_perms(
          1, &(*insertion_result.first));
      while (!new_perms.empty()) {
        const std::vector<int>* new_perm = new_perms.back();
        new_perms.pop_back();
        std::vector<const std::vector<int>*> old_perms;
        old_perms.reserve(permutation_group.size());
        for (const std::vector<int>& p : permutation_group)
          old_perms.push_back(&p);
        for (const std::vector<int>* old_perm : old_perms) {
          auto insertion_result = permutation_group.insert(
              ComposePermutations(*old_perm, *new_perm));
          if (insertion_result.second) {
            new_perms.push_back(&(*insertion_result.first));
          }
          insertion_result = permutation_group.insert(
              ComposePermutations(*new_perm, *old_perm));
          if (insertion_result.second) {
            new_perms.push_back(&(*insertion_result.first));
          }
        }
      }
    }
    return permutation_group.size();
  }

  static constexpr double kDefaultTimeLimitSeconds = 120.0;

  void ExpectSymmetries(const std::vector<std::pair<int, int>>& arcs,
                        absl::string_view expected_node_equivalence_classes,
                        double log_of_expected_permutation_group_size) {
    Graph graph;
    for (const std::pair<int, int>& arc : arcs)
      graph.AddArc(arc.first, arc.second);
    graph.Build();
    GraphSymmetryFinder symmetry_finder(graph, GraphIsSymmetric(graph));
    std::vector<std::unique_ptr<SparsePermutation>> generators;
    std::vector<int> node_equivalence_classes(graph.num_nodes(), 0);
    std::vector<int> orbit_sizes;
    TimeLimit time_limit(kDefaultTimeLimitSeconds);
    ASSERT_OK(symmetry_finder.FindSymmetries(
        &node_equivalence_classes, &generators, &orbit_sizes, &time_limit));
    std::vector<std::string> permutations_str;
    for (const std::unique_ptr<SparsePermutation>& permutation : generators) {
      permutations_str.push_back(permutation->DebugString());
    }
    SCOPED_TRACE(
        "Graph: " + absl::StrJoin(arcs, ", ", absl::PairFormatter("->")) +
        "\nGenerators found:\n  " + absl::StrJoin(permutations_str, "\n  "));

    // Verify the equivalence classes.
    EXPECT_EQ(expected_node_equivalence_classes,
              DynamicPartition(node_equivalence_classes)
                  .DebugString(/*sort_parts_lexicographically=*/true));

    // Verify the automorphism group size.
    double log_of_permutation_group_size = 0.0;
    for (const int orbit_size : orbit_sizes) {
      log_of_permutation_group_size += log(orbit_size);
    }
    EXPECT_THAT(log_of_permutation_group_size,
                DoubleEq(log_of_expected_permutation_group_size))
        << absl::StrJoin(orbit_sizes, " x ");

    if (log_of_expected_permutation_group_size <= log(1000.0)) {
      const int expected_permutation_group_size =
          static_cast<int>(round(exp(log_of_expected_permutation_group_size)));
      EXPECT_EQ(
          expected_permutation_group_size,
          ComputePermutationGroupSizeAndVerifyBasicIrreductibility(generators));
    }
  }
};

TEST_F(FindSymmetriesTest, CyclesOfDifferentLength) {
  // The same test case as before, but this time we do expect the symmetry
  // detector to figure out that the two cycles of different lengths aren't
  // symmetric.
  ExpectSymmetries({{1, 2}, {2, 3}, {3, 1}, {4, 5}, {5, 4}}, "0 | 1 2 3 | 4 5",
                   log(6));
}

// This can be used to convert a list of M undirected edges into the list of
// 2*M corresponding directed arcs.
std::vector<std::pair<int, int>> AppendReversedPairs(
    absl::Span<const std::pair<int, int>> pairs) {
  std::vector<std::pair<int, int>> out;
  out.reserve(pairs.size() * 2);
  out.insert(out.begin(), pairs.begin(), pairs.end());
  for (const auto [from, to] : pairs) out.push_back({to, from});
  return out;
}

// See: http://en.wikipedia.org/wiki/Petersen_graph, where it looks a lot
// more symmetric than the ASCII art below.
//
//    .---------5---------.
//   /          |          \
//  /           0           \
// 9--------4--/-\--1--------6
//  \        \/   \/        /
//   \       /\   /\       /
//    \     /  `.ˊ  \     /
//     \   3---ˊ `---2   /
//      \ /           \ /
//       8-------------7
std::vector<std::pair<int, int>> PetersenGraphEdges() {
  return {
      {0, 2}, {1, 3}, {2, 4}, {3, 0}, {4, 1},  // Inner star
      {5, 6}, {6, 7}, {7, 8}, {8, 9}, {9, 5},  // Outer pentagon
      {0, 5}, {1, 6}, {2, 7}, {3, 8}, {4, 9},  // Star <-> Pentagon
  };
}

TEST_F(FindSymmetriesTest, PetersenGraph) {
  ExpectSymmetries(AppendReversedPairs(PetersenGraphEdges()),
                   "0 1 2 3 4 5 6 7 8 9", log(120));
}

TEST_F(FindSymmetriesTest, UndirectedCyclesOfDifferentLength) {
  // 0---1  3--4
  //  \ /   |  |
  //   2    6--5
  ExpectSymmetries(
      {
          {0, 1},
          {1, 2},
          {2, 0},  // Triangle, CW.
          {2, 1},
          {1, 0},
          {0, 2},  // Triangle, CCW.
          {3, 4},
          {4, 5},
          {5, 6},
          {6, 3},  // Square, CW.
          {6, 5},
          {5, 4},
          {4, 3},
          {3, 6},  // Square, CCW.
      },
      "0 1 2 | 3 4 5 6", log(48));
}

TEST_F(FindSymmetriesTest, SmallestCyclicGroupUndirectedGraph) {
  // See http://mathworld.wolfram.com/GraphAutomorphism.html.
  //
  //         2
  //        / \
  //   7---0---1
  //  / \ / \ /
  // 8---6---3
  //      \ / \
  //       4---5
  ExpectSymmetries(
      {
          {0, 3}, {3, 0}, {3, 6}, {6, 3},
          {6, 0}, {0, 6},                  // Inner triangle 0-3-6.
          {0, 1}, {1, 0}, {3, 1}, {1, 3},  // Angle 0-1-3.
          {3, 4}, {4, 3}, {6, 4}, {4, 6},  // Angle 3-4-6.
          {6, 7}, {7, 6}, {0, 7}, {7, 0},  // Angle 6-7-0.
          {0, 2}, {2, 0}, {2, 1}, {1, 2},  // Angle 0-2-1.
          {3, 5}, {5, 3}, {5, 4}, {4, 5},  // Angle 3-5-4.
          {6, 8}, {8, 6}, {8, 7}, {7, 8},  // Angle 6-8-7.
      },
      "0 3 6 | 1 4 7 | 2 5 8", log(3));
}

double LogFactorial(int n) {
  double sum = 0.0;
  for (int i = 1; i <= n; ++i) sum += log(i);
  return sum;
}

TEST_F(FindSymmetriesTest, Clique) {
  // Note(user): as of 2014-01-22, the symetry finder is extremely inefficient
  // on this test for size = 6 (7s in fastbuild), while it takes only a
  // fraction of that time for larger sizes.
  // TODO(user): fix this inefficiency and enlarge the test space.
  const int kMaxSize = DEBUG_MODE ? 5 : 120;
  std::vector<std::pair<int, int>> arcs;
  std::vector<int> nodes;
  for (int size = 1; size <= kMaxSize; ++size) {
    SCOPED_TRACE(absl::StrFormat("Size: %d", size));
    const int new_node = size - 1;
    nodes.push_back(new_node);
    for (int old_node = 0; old_node < new_node; ++old_node) {
      arcs.push_back({old_node, new_node});
      arcs.push_back({new_node, old_node});
    }
    // When size = 1; the graph looks empty to ExpectSymmetries() because there
    // are no arcs. Skip to n >= 2.
    if (size == 1) continue;
    ExpectSymmetries(arcs, absl::StrJoin(nodes, " "), LogFactorial(size));
    if (HasFailure()) break;  // Don't spam the user with more than one failure.
  }
}

TEST_F(FindSymmetriesTest, DirectedStar) {
  // Note(user): as of 2014-01-22, the symetry finder is extremely inefficient
  // on this test for size = 6 (and relatively too, for size = 5): it takes only
  // a fraction of time for larger sizes, but about 16s in fastbuild mode for 6.
  // TODO(user): fix this inefficiency and enlarge the test space.
  //
  // Example for size = 4, with outward arcs:
  //
  //        1
  //        ^
  //        |
  //  4<----0---->2
  //        |
  //        v
  //        3
  const int kMaxSize = DEBUG_MODE ? 5 : 120;
  std::vector<std::pair<int, int>> out_arcs;
  std::vector<std::pair<int, int>> in_arcs;
  std::string expected_equivalence_classes = "0 |";
  for (int size = 1; size <= kMaxSize; ++size) {
    SCOPED_TRACE(absl::StrFormat("Size: %d", size));
    absl::StrAppend(&expected_equivalence_classes, " ", size);
    out_arcs.push_back({0, size});
    in_arcs.push_back({size, 0});
    // When size = 1; the formula below doesn't work. Skip to n >= 2.
    if (size == 1) continue;
    ExpectSymmetries(out_arcs, expected_equivalence_classes,
                     LogFactorial(size));
    ExpectSymmetries(in_arcs, expected_equivalence_classes, LogFactorial(size));
    if (HasFailure()) break;  // Don't spam the user with more than one failure.
  }
}

TEST_F(FindSymmetriesTest, UndirectedAntiPrism) {
  // See http://mathworld.wolfram.com/GraphAutomorphism.html .
  // Example for size = 8:
  //
  //        .-0---1-.
  //      .ˊ / `ₓˊ \ `.
  //     7--/--ˊ `--\--2
  //     |\/         \/|
  //     |/\         /\|
  //     6--\--. .--/--3
  //      `. \ .ˣ. / .ˊ
  //        `-5---4-ˊ
  const int kMaxSize = DEBUG_MODE ? 60 : 150;
  std::vector<int> nodes;
  std::vector<std::pair<int, int>> arcs;
  for (int size = 6; size <= kMaxSize; size += 2) {
    SCOPED_TRACE(absl::StrFormat("Size: %d", size));
    arcs.clear();
    nodes.clear();
    for (int i = 0; i < size; ++i) {
      nodes.push_back(i);
      const int next = (i + 1) % size;
      const int next2 = (i + 2) % size;
      arcs.push_back({i, next});
      arcs.push_back({i, next2});
      arcs.push_back({next, i});
      arcs.push_back({next2, i});
    }
    ExpectSymmetries(arcs, absl::StrJoin(nodes, " "),
                     log(size == 6 ? 48 : 2 * size));
    if (HasFailure()) break;  // Don't spam the user with more than one failure.
  }
}

TEST_F(FindSymmetriesTest, UndirectedHypercube) {
  // Example for dimension = 3 (the numbering fits the standard construction,
  // where vertices X and Y have an edge iff they differ by exactly one bit).
  //
  //   0-----1
  //   |\    |\
  //   | 4-----5
  //   | |   | |
  //   2-|---3 |
  //    \|    \|
  //     7-----8
  //
  // See http://mathworld.wolfram.com/GraphAutomorphism.html : the expected size
  // of the automorphism group is (2 * 4 * 6 * ... * (2 * dimension)).
  const int kMaxDimension = DEBUG_MODE ? 7 : 15;
  for (int dimension = 1; dimension <= kMaxDimension; ++dimension) {
    SCOPED_TRACE(absl::StrFormat("Dimension: %d", dimension));
    const int num_nodes = 1 << dimension;
    std::vector<int> nodes(num_nodes, -1);
    for (int i = 0; i < num_nodes; ++i) nodes[i] = i;
    const int num_arcs = num_nodes * dimension;
    std::vector<std::pair<int, int>> arcs;
    arcs.reserve(num_arcs);
    for (int from = 0; from < num_nodes; ++from) {
      for (int bit_order = 0; bit_order < dimension; ++bit_order) {
        arcs.push_back({from, from ^ (1 << bit_order)});
      }
    }
    double log_of_expected_group_size = 0.0;
    for (int i = 1; i <= dimension; ++i) {
      log_of_expected_group_size += log(2 * i);
    }
    ExpectSymmetries(arcs, absl::StrJoin(nodes, " "),
                     log_of_expected_group_size);
    if (HasFailure()) break;  // Don't spam the user with more than one failure.
  }
}

TEST_F(FindSymmetriesTest, DirectedHypercube) {
  // Just like UndirectedHypercube, but arcs are always oriented from lower
  // hamming weight to higher hamming weight.
  // The symmetries are all permutations of the bits of the node indices.
  //
  // Note(user): As of 2014-01-22, dimension=6 exhibits the same peculiar slow
  // behavior (much slower than larger or smaller dimensions).
  //
  // TODO(user): Increase kMaxDimension to at least 15 in opt mode, when the
  // performance gets restored to the state before CL 66308548.
  const int kMaxDimension = DEBUG_MODE ? 5 : 14;
  for (int dimension = 1; dimension <= kMaxDimension; ++dimension) {
    SCOPED_TRACE(absl::StrFormat("Dimension: %d", dimension));
    const int num_nodes = 1 << dimension;
    const int num_arcs = num_nodes * dimension;
    std::vector<std::pair<int, int>> arcs;
    arcs.reserve(num_arcs);
    for (int from = 0; from < num_nodes; ++from) {
      for (int bit_order = 0; bit_order < dimension; ++bit_order) {
        const int to = from ^ (1 << bit_order);
        if (to > from) arcs.push_back({from, to});
      }
    }

    // The equivalence classes are the nodes with the same hamming weight.
    std::vector<std::vector<int>> nodes_by_hamming_weight(dimension + 1);
    for (int i = 0; i < num_nodes; ++i) {
      nodes_by_hamming_weight[absl::popcount(unsigned(i))].push_back(i);
    }
    std::vector<std::string> expected_equivalence_classes;
    for (const std::vector<int>& nodes : nodes_by_hamming_weight) {
      expected_equivalence_classes.push_back(absl::StrJoin(nodes, " "));
    }

    ExpectSymmetries(arcs, absl::StrJoin(expected_equivalence_classes, " | "),
                     LogFactorial(dimension));
    if (HasFailure()) break;  // Don't spam the user with more than one failure.
  }
}

TEST_F(FindSymmetriesTest, InwardGrid) {
  // Directed NxN grids where all arcs are towards the center (if N is even,
  // the arcs between the two middle rows (or columns) are bidirectional).
  // Example for N=3 and N=4:
  //
  //   0 → 1 ← 2    0 → 1 ↔ 2 ← 3
  //   ↓   ↓   ↓    ↓   ↓   ↓   ↓
  //   3 → 4 ← 5    4 → 5 ↔ 6 ← 7
  //   ↑   ↑   ↑    ↕   ↕   ↕   ↕
  //   6 → 7 ← 8    8 → 9 ↔ 10← 11
  //                ↑   ↑   ↑   ↑
  //                12→ 13↔ 14← 15
  //
  // Note(user): this test proved very useful: it exercises the code path where
  // we find a perfect permutation match that is not an automorphism, and it
  // also uncovered the suspected flaw of the code as of CL 59849337 (overly
  // aggressive pruning).
  const int kMaxSize = DEBUG_MODE ? 30 : 100;
  std::vector<std::pair<int, int>> arcs;
  for (int size = 2; size <= kMaxSize; ++size) {
    SCOPED_TRACE(absl::StrFormat("Size: %d", size));
    arcs.clear();
    for (int i = 0; i < size / 2; ++i) {
      const int sym_i = size - 1 - i;
      for (int j = 0; j < size; ++j) {
        arcs.push_back({i * size + j, (i + 1) * size + j});          // Down
        arcs.push_back({sym_i * size + j, (sym_i - 1) * size + j});  // Up
        arcs.push_back({j * size + i, j * size + i + 1});            // Right
        arcs.push_back({j * size + sym_i, j * size + sym_i - 1});    // Left
      }
    }
    // Build the expected equivalence classes.
    std::vector<std::string> expected_equivalence_classes;
    for (int i = 0; i <= (size - 1) / 2; ++i) {
      const int sym_i = size - 1 - i;
      for (int j = i; j <= (size - 1) / 2; ++j) {
        const int sym_j = size - 1 - j;
        std::vector<int> symmetric_nodes = {
            i * size + j,         j * size + i,         sym_i * size + j,
            j * size + sym_i,     i * size + sym_j,     sym_j * size + i,
            sym_i * size + sym_j, sym_j * size + sym_i,
        };
        std::set<int> unique_nodes(symmetric_nodes.begin(),
                                   symmetric_nodes.end());
        expected_equivalence_classes.push_back(
            absl::StrJoin(unique_nodes, " "));
      }
    }
    ExpectSymmetries(arcs, absl::StrJoin(expected_equivalence_classes, " | "),
                     log(8));
    if (HasFailure()) break;  // Don't spam the user with more than one failure.
  }
}

void AddReverseArcs(Graph* graph) {
  const int num_arcs = graph->num_arcs();
  for (int a = 0; a < num_arcs; ++a) {
    graph->AddArc(graph->Head(a), graph->Tail(a));
  }
}

void AddReverseArcsAndFinalize(Graph* graph) {
  AddReverseArcs(graph);
  graph->Build();
}

void SetGraphEdges(absl::Span<const std::pair<int, int>> edges, Graph* graph) {
  DCHECK_EQ(graph->num_arcs(), 0);
  for (const auto [from, to] : edges) graph->AddArc(from, to);
  AddReverseArcsAndFinalize(graph);
}

TEST(CountTrianglesTest, EmptyGraph) {
  EXPECT_THAT(CountTriangles(Graph(0, 0), /*max_degree=*/0), IsEmpty());
  EXPECT_THAT(CountTriangles(Graph(0, 0), /*max_degree=*/9999), IsEmpty());
}

TEST(CountTrianglesTest, SimpleUndirectedExample) {
  // 0--1--2
  //  `.|`.|
  //    3--4--5
  Graph g;
  SetGraphEdges(
      {{0, 1}, {1, 2}, {0, 3}, {1, 4}, {1, 3}, {2, 4}, {3, 4}, {4, 5}}, &g);
  // Reminder: every undirected triangle counts as two directed triangles.
  EXPECT_THAT(CountTriangles(g, /*max_degree=*/999),
              ElementsAre(2, 6, 2, 4, 4, 0));
  EXPECT_THAT(CountTriangles(g, /*max_degree=*/3),
              ElementsAre(2, 0, 2, 4, 0, 0));
  EXPECT_THAT(CountTriangles(g, /*max_degree=*/2),
              ElementsAre(2, 0, 2, 0, 0, 0));
  EXPECT_THAT(CountTriangles(g, /*max_degree=*/1),
              ElementsAre(0, 0, 0, 0, 0, 0));
  EXPECT_THAT(CountTriangles(g, /*max_degree=*/0),
              ElementsAre(0, 0, 0, 0, 0, 0));
}

TEST(CountTrianglesTest, SimpleDirectedExample) {
  //   .-> 1 -> 2 <-.
  //  /    ^    ^    \
  // 0     |    |     5
  //  \    |    v    /
  //   `-> 3 <- 4 <-'
  Graph g;
  for (auto [from, to] : std::vector<std::pair<int, int>>{
           {0, 1},
           {1, 2},
           {0, 3},
           {4, 3},
           {5, 2},
           {5, 4},
           {3, 1},
           {2, 4},
           {4, 2},
       }) {
    g.AddArc(from, to);
  }
  g.Build();
  EXPECT_THAT(CountTriangles(g, /*max_degree=*/999),
              ElementsAre(1, 0, 0, 0, 0, 2));
  EXPECT_THAT(CountTriangles(g, /*max_degree=*/1),
              ElementsAre(0, 0, 0, 0, 0, 0));
}

TEST(LocalBfsTest, SimpleExample) {
  // 0--1--2
  //  `.|`.|
  //    3--4--5
  Graph g;
  SetGraphEdges(
      {{0, 1}, {1, 2}, {0, 3}, {1, 4}, {1, 3}, {2, 4}, {3, 4}, {4, 5}}, &g);
  std::vector<bool> tmp_mask(g.num_nodes(), false);
  std::vector<int> visited;
  std::vector<int> num_within_radius;

  // Run a first unlimited BFS from 0.
  LocalBfs(g, /*source=*/0, /*stop_after_num_nodes=*/99, &visited,
           &num_within_radius, &tmp_mask);
  EXPECT_THAT(
      visited,
      // Nodes should be sorted by distance. (1,3) and (2,4) have the
      // same, so we have 4 possible orders. Though if 3 was settled
      // first, then 4 must be before 2, since 3 is only connected to 4.
      AnyOf(ElementsAre(0, 1, 3, 2, 4, 5), ElementsAre(0, 1, 3, 4, 2, 5),
            ElementsAre(0, 3, 1, 4, 2, 5)));
  EXPECT_THAT(num_within_radius, ElementsAre(1, 3, 5, 6));

  // Then a BFS that stops after visiting 4 nodes: we should finish exploring
  // that distance, i.e. explore 2 and 4, but not 5. Still, 5 is "visited".
  LocalBfs(g, /*source=*/0, /*stop_after_num_nodes=*/4, &visited,
           &num_within_radius, &tmp_mask);
  EXPECT_THAT(visited, AnyOf(ElementsAre(0, 1, 3, 2, 4, 5),
                             ElementsAre(0, 1, 3, 4, 2, 5),
                             ElementsAre(0, 3, 1, 4, 2, 5)));
  EXPECT_THAT(num_within_radius, ElementsAre(1, 3, 5, 6));

  // Then a BFS that stops after visiting 2 nodes.
  LocalBfs(g, /*source=*/0, /*stop_after_num_nodes=*/2, &visited,
           &num_within_radius, &tmp_mask);
  EXPECT_THAT(visited,
              AnyOf(ElementsAre(0, 1, 3, 2, 4), ElementsAre(0, 1, 3, 4, 2),
                    ElementsAre(0, 3, 1, 4, 2)));
  EXPECT_THAT(num_within_radius, ElementsAre(1, 3, 5));

  // Now run a BFS from node 3, stop exploring after 1 node.
  LocalBfs(g, /*source=*/3, /*stop_after_num_nodes=*/1, &visited,
           &num_within_radius, &tmp_mask);
  EXPECT_THAT(visited, UnorderedElementsAre(3, 0, 1, 4));
  EXPECT_THAT(num_within_radius, ElementsAre(1, 4));
  // Now after 2 nodes.
  LocalBfs(g, /*source=*/3, /*stop_after_num_nodes=*/2, &visited,
           &num_within_radius, &tmp_mask);
  EXPECT_THAT(visited, UnorderedElementsAre(3, 0, 1, 4, 2, 5));
  EXPECT_THAT(num_within_radius, ElementsAre(1, 4, 6));
}

}  // namespace
}  // namespace operations_research
