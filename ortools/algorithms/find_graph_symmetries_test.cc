// Copyright 2010-2022 Google LLC
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
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ortools/algorithms/dynamic_partition.h"
#include "ortools/algorithms/dynamic_permutation.h"
#include "ortools/algorithms/sparse_permutation.h"
#include "ortools/base/dump_vars.h"
#include "ortools/base/file.h"
#include "ortools/base/helpers.h"
#include "ortools/base/map_util.h"
#include "ortools/base/path.h"
#include "ortools/base/walltime.h"
#include "ortools/graph/io.h"
#include "ortools/graph/random_graph.h"
#include "ortools/graph/util.h"
#include "ortools/util/filelineiter.h"
#include "strings/stringpiece_utils.h"

namespace operations_research {
namespace {

using Graph = GraphSymmetryFinder::Graph;
using ::testing::AnyOf;
using ::testing::DoubleEq;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;
using ::util::GraphIsSymmetric;
using ::util::ReadGraphFile;

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
  return partition.DebugString(DynamicPartition::SORT_LEXICOGRAPHICALLY);
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

class IsGraphAutomorphismTest : public testing::Test {
 protected:
  void ExpectIsGraphAutomorphism(
      int num_nodes, const std::vector<std::pair<int, int>>& graph_arcs,
      const std::vector<std::vector<int>>& permutation_cycles,
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

  std::vector<int> ComposePermutations(const std::vector<int>& p1,
                                       const std::vector<int>& p2) {
    CHECK_EQ(p1.size(), p2.size());
    std::vector<int> composed(p1.size(), -1);
    for (int i = 0; i < p1.size(); ++i) composed[i] = p1[p2[i]];
    return composed;
  }

  // Brute-force compute the size of the group by computing all of its elements,
  // with some basic, non-through EXPECT(..) that check that each generator
  // does make the group grow.
  int ComputePermutationGroupSizeAndVerifyBasicIrreductibility(
      const std::vector<std::unique_ptr<SparsePermutation>>& generators) {
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
                  .DebugString(DynamicPartition::SORT_LEXICOGRAPHICALLY));

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
    const std::vector<std::pair<int, int>>& pairs) {
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

// TODO(user): unit test the early abortions caused by the time limit.

class BigGraphFindSymmetriesTest
    : public ::testing::TestWithParam</*graph name*/ std::string> {};

#ifdef NDEBUG  // opt mode
INSTANTIATE_TEST_CASE_P(
    RealGraph, BigGraphFindSymmetriesTest,
    ::testing::Values(
        // Some SAT graphs from the DAC'08 paper.
        "2pipe", "3pipe", "4pipe", "5pipe", "6pipe", "7pipe",
        // Graphs associated to some US state; from the DAC'08 paper.
        "DE", "ME", "IL", "CA",
        // Very sparse graphs. They are all 2-colored except "internet".
        "internet", "adaptec1", "adaptec2", "adaptec3", "adaptec4", "bigblue1",
        "bigblue2", "bigblue3", "bigblue4",
        // SAT graphs from the pigeonhole problem. For those, it is important
        // to reduce the support size.
        "sat_hole002", "sat_hole010", "sat_hole010_shuffled",
        "sat_hole020_shuffled", "sat_hole030", "sat_hole030_shuffled",
        "sat_hole050"));
// TODO(user): add the other test graphs when the code is fast enough.
// See  for the latest timings.
#else   // non-opt mode
INSTANTIATE_TEST_SUITE_P(RealGraph, BigGraphFindSymmetriesTest,
                         ::testing::Values("2pipe", "3pipe", "sat_hole002",
                                           "sat_hole010",
                                           "sat_hole010_shuffled",
                                           "sat_hole020_shuffled"));
#endif  // NDEBUG

TEST_P(BigGraphFindSymmetriesTest, VerifyAutomorphismGroupSize) {
  const std::string graph_name = GetParam();
  LOG(INFO) << "Graph: " << graph_name;
  const char kTestDataDir[] = "ortools/algorithms/testdata";
  const char kSaucyBenchmarksDir[] = "operations_research_saucy_benchmarks";

  // Parse the CSV benchmark result file to look up the expected characteristics
  // of the graphs that we'll be looking at.
  int num_nodes = -1;
  int num_edges = -1;
  // The size of the automorphism group can be huge, and overflow (by far) an
  // extended double; so we parse its mantissa and its exponent separately. We
  // even support double exponents -- like a googolplex: 1(1e100).
  int dig0_size = -1;
  int64_t frac_size = -1;
  double exp_size = -1;
  int num_generators = -1;
  double avg_support_size = -1.0;
  int num_refines = -1;
  double time_seconds = -1.0;
  int num_refines_saucy = -1;
  double time_seconds_saucy = -1.0;
  {
    const std::string csv_filename =
        file::JoinPath(absl::GetFlag(FLAGS_test_srcdir), kTestDataDir,
                       "graph_benchmark_results.csv");
    std::string csv_contents;
    ASSERT_OK(file::GetContents(csv_filename, &csv_contents, file::Defaults()))
        << csv_filename;
    std::vector<std::string> csv_lines = absl::StrSplit(csv_contents, '\n');
    bool found = false;
    for (const std::string& csv_line : csv_lines) {
      if (absl::StartsWith(csv_line, graph_name + ",")) {
        ASSERT_GE(
            sscanf(
                csv_line.c_str(),
                (graph_name + ",%d,%d,%d.%" SCNd64 "e%lf,%d,%lf,%d,%lf,%d,%lf")
                    .c_str(),
                &num_nodes, &num_edges, &dig0_size, &frac_size, &exp_size,
                &num_generators, &avg_support_size, &num_refines, &time_seconds,
                &num_refines_saucy, &time_seconds_saucy),
            9)
            << csv_line;
        found = true;
        break;
      }
    }
    ASSERT_TRUE(found) << "Graph '" << graph_name << "' not found in "
                       << csv_filename;
  }

  // Load the graph.
  const std::string filename = file::JoinPath(
      absl::GetFlag(FLAGS_test_srcdir),
      absl::StartsWith(graph_name, "sat_") ? kTestDataDir : kSaucyBenchmarksDir,
      graph_name + ".g");
  std::vector<int> num_nodes_per_color;
  absl::StatusOr<Graph*> error_or_graph =
      ReadGraphFile<Graph>(filename, /*directed=*/false, &num_nodes_per_color);
  ASSERT_OK(error_or_graph.status());
  std::unique_ptr<Graph> graph(error_or_graph.value());
  // TODO(user): merge this code with the one in the _main.cc.
  std::vector<int> node_equivalence_classes;
  for (int color = 0; color < num_nodes_per_color.size(); ++color) {
    node_equivalence_classes.insert(node_equivalence_classes.end(),
                                    num_nodes_per_color[color], color);
  }
  LOG(INFO) << "Found " << num_nodes_per_color.size() << " colors!";

  // Verify that it matches the benchmark's announced size.
  ASSERT_EQ(num_nodes, graph->num_nodes());
  int num_self_edges = 0;
  for (Graph::NodeIndex node : graph->AllNodes()) {
    for (Graph::ArcIndex arc : graph->OutgoingArcs(node)) {
      if (graph->Head(arc) == node) ++num_self_edges;
    }
  }
  if (num_self_edges > 0) {
    LOG(INFO) << "Found " << num_self_edges << " self-edges.";
  }
  ASSERT_EQ(num_edges * 2 - num_self_edges, graph->num_arcs());

  // Compute the automorphism group.
  ASSERT_TRUE(GraphIsSymmetric(*graph));  // By construction.
  GraphSymmetryFinder symmetry_finder(*graph, /*is_undirected=*/true);
  std::vector<std::unique_ptr<SparsePermutation>> generators;
  std::vector<int> factorized_automorphism_group_size;
  const absl::Time time0 = absl::Now();
  ASSERT_OK(
      symmetry_finder.FindSymmetries(&node_equivalence_classes, &generators,
                                     &factorized_automorphism_group_size));
  const double time_spent = absl::ToDoubleSeconds(absl::Now() - time0);
  LOG(INFO) << "Time: " << time_spent << "s (ref: " << time_seconds << ").";
  // NOTE(user): we no longer log time_seconds_saucy, because:
  // - It's misleading: the "ref" results are actually those from the SAUCY team
  //   after an improvement they made: the times we used to label as "saucy"
  //   were the times they got before that.
  // - We've reached the performance of the "ref", and no longer need to compare
  //   against the old, inferior results.

  // Verify the size of the automorphism group.
  double log10_of_permutation_group_size = 0.0;
  for (const int orbit_size : factorized_automorphism_group_size) {
    log10_of_permutation_group_size += log10(orbit_size);
  }
  const int64_t floor_log10 =
      static_cast<int64_t>(floor(log10_of_permutation_group_size));
  LOG(INFO) << "|Aut(G)| = "
            << exp10(log10_of_permutation_group_size - floor_log10) << "e"
            << floor_log10;

  // Parse the expected group size. Unfortunately it's a bit convoluted.
  double expected_log10_of_permutation_group_size = -1;
  if (dig0_size < 0) {
    // We don't know the expected group size. So we don't expect anything.
  } else if (frac_size <= 0) {
    // The expected group size is known, but with low precision: we just have
    // the exponent. So we also round our computed size down, for comparison
    // purposes.
    ASSERT_EQ(1, dig0_size);
    log10_of_permutation_group_size = floor_log10;
    expected_log10_of_permutation_group_size = exp_size;
  } else {
    // The conversion of the two integers (integer part and fractional part) to
    // a mantissa is a bit tedious. Note that 1+floor(log10(x)) is the number of
    // digits of x.
    expected_log10_of_permutation_group_size =
        exp_size +
        log10(dig0_size + frac_size / exp10(1.0 + floor(log10(frac_size))));
  }
  if (expected_log10_of_permutation_group_size >= 0) {
    // The error we allow is quite large. Either our accounting isn't great, or
    // the benchmark result's precision isn't; but either way it's good enough
    // to get a pretty good precision [0.1^10 < 2; so we can't be missing
    // a generator if our log10(sizes) agree within 0.1].
    EXPECT_NEAR(expected_log10_of_permutation_group_size,
                log10_of_permutation_group_size, 1e-1)
        << absl::StrJoin(factorized_automorphism_group_size, " x ");
  }

  // The rest can't be verified, but is an informative comparison to look
  // at, in terms of performance of the algorithm.
  LOG(INFO) << "Number of generators: " << generators.size()
            << " (ref: " << num_generators << ").";

  double sum_support_sizes = 0.0;
  for (const std::unique_ptr<SparsePermutation>& perm : generators) {
    sum_support_sizes += perm->Support().size();
  }
  LOG(INFO) << "Average support size: " << sum_support_sizes / generators.size()
            << " (ref: " << avg_support_size << ").";

  // TODO(user): also output the number of Refine() operations.
};

TEST(SimpleConnectedGraphsAreIsomorphicTest, EmptyGraphs) {
  Graph g1;
  Graph g2;
  EXPECT_EQ(
      SimpleConnectedGraphsAreIsomorphic(g1, g2, absl::InfiniteDuration()),
      GRAPHS_ARE_ISOMORPHIC);
}

TEST(SimpleConnectedGraphsAreIsomorphicTest, NotSameNumberOfNodes) {
  Graph g1;
  g1.AddArc(0, 1);
  g1.AddArc(1, 2);
  g1.Build();
  Graph g2;
  g2.AddArc(0, 1);
  g2.AddArc(1, 0);
  g2.Build();
  EXPECT_EQ(
      SimpleConnectedGraphsAreIsomorphic(g1, g2, absl::InfiniteDuration()),
      GRAPHS_ARE_NOT_ISOMORPHIC);
}

TEST(SimpleConnectedGraphsAreIsomorphicTest, NotSameNumberOfArcs) {
  Graph g1;
  g1.AddArc(0, 1);
  g1.AddArc(1, 2);
  g1.Build();
  Graph g2;
  g2.AddArc(0, 1);
  g2.AddArc(1, 0);
  g2.AddArc(1, 2);
  g2.Build();
  EXPECT_EQ(
      SimpleConnectedGraphsAreIsomorphic(g1, g2, absl::InfiniteDuration()),
      GRAPHS_ARE_NOT_ISOMORPHIC);
}

TEST(SimpleConnectedGraphsAreIsomorphicTest, OneSymmetricTheOtherNot) {
  Graph g1;
  g1.AddArc(0, 1);
  g1.AddArc(0, 0);
  g1.Build();
  Graph g2;
  g2.AddArc(0, 1);
  g2.AddArc(1, 0);
  g2.Build();
  EXPECT_EQ(
      SimpleConnectedGraphsAreIsomorphic(g1, g2, absl::InfiniteDuration()),
      GRAPHS_ARE_NOT_ISOMORPHIC);
}

TEST(SimpleConnectedGraphsAreIsomorphicTest, DisconnectedGraphs) {
  Graph g;
  g.AddArc(1, 2);
  g.AddArc(0, 0);
  g.Build();
  EXPECT_EQ(SimpleConnectedGraphsAreIsomorphic(g, g, absl::InfiniteDuration()),
            GRAPH_ISOMORHPISM_SOME_GRAPH_IS_DISCONNECTED);
}

TEST(SimpleConnectedGraphsAreIsomorphicTest, GraphsWithMultiArcs) {
  Graph g;
  g.AddArc(0, 1);
  g.AddArc(0, 1);
  g.Build();
  EXPECT_EQ(SimpleConnectedGraphsAreIsomorphic(g, g, absl::InfiniteDuration()),
            GRAPH_ISOMORPHISM_SOME_GRAPH_HAS_MULTI_ARCS);
}

TEST(SimpleConnectedGraphsAreIsomorphicTest, GraphsWithSelfArcs) {
  Graph g1;
  g1.AddArc(0, 1);
  g1.AddArc(2, 2);
  g1.AddArc(2, 0);
  g1.Build();
  Graph g2;
  g2.AddArc(0, 0);
  g2.AddArc(1, 2);
  g2.AddArc(0, 1);
  g2.Build();
  EXPECT_EQ(
      SimpleConnectedGraphsAreIsomorphic(g1, g2, absl::InfiniteDuration()),
      GRAPHS_ARE_ISOMORPHIC);
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

std::unique_ptr<Graph> RandomlyPermutedGraph(const Graph& graph,
                                             absl::BitGenRef gen) {
  std::vector<int> new_indices(graph.num_nodes(), -1);
  std::iota(new_indices.begin(), new_indices.end(), 0);
  std::shuffle(new_indices.begin(), new_indices.end(), gen);
  auto new_graph = std::make_unique<Graph>(graph.num_nodes(), graph.num_arcs());
  for (int a = 0; a < graph.num_arcs(); ++a) {
    new_graph->AddArc(new_indices[graph.Tail(a)], new_indices[graph.Head(a)]);
  }
  new_graph->Build();
  return new_graph;
}

TEST(SimpleConnectedGraphsAreIsomorphicTest, NonTrivialNodeDifference) {
  // Graphs (writing the node degrees instead of the node indices; node are
  // then numbered like the US reading order, left to right and top to bottom).
  //
  // 1                  1--2
  //  \                     \
  //   3--2--2--1   VS       3--2--1
  //  /                     /
  // 1                     1
  Graph g1;
  g1.AddArc(0, 1);
  g1.AddArc(1, 2);
  g1.AddArc(2, 3);
  g1.AddArc(3, 4);
  g1.AddArc(5, 1);
  AddReverseArcsAndFinalize(&g1);
  Graph g2;
  g2.AddArc(0, 1);
  g2.AddArc(1, 2);
  g2.AddArc(2, 3);
  g2.AddArc(3, 4);
  g2.AddArc(5, 2);
  AddReverseArcsAndFinalize(&g2);
  EXPECT_EQ(
      SimpleConnectedGraphsAreIsomorphic(g1, g2, absl::InfiniteDuration()),
      GRAPHS_ARE_NOT_ISOMORPHIC);
}

void SetGraphEdges(const std::vector<std::pair<int, int>>& edges,
                   Graph* graph) {
  DCHECK_EQ(graph->num_arcs(), 0);
  for (const auto [from, to] : edges) graph->AddArc(from, to);
  AddReverseArcsAndFinalize(graph);
}

// The Petersen graph (left) has a 'sibling' that's not isomorphic, where the
// inner "star" is a pentagon. See: http://en.wikipedia.org/wiki/Petersen_graph.
//
//    .---------5---------.             .---------5---------.
//   /          |          \           /          |          \
//  /           0           \         /         .-0-.         \
// 9--------4--/-\--1--------6   VS  9--------4´     `1--------6
//  \        \/   \/        /         \      /         \      /
//   \       /\   /\       /           \     |         |     /
//    \     /  `.ˊ  \     /             \    |         |    /
//     \   3---ˊ `---2   /               \   3---------2   /
//      \ /           \ /                 \ /           \ /
//       8-------------7                   8-------------7
std::vector<std::pair<int, int>> PetersenSiblingGraphEdges() {
  return {
      {0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 0},  // Inner pentagon
      {5, 6}, {6, 7}, {7, 8}, {8, 9}, {9, 5},  // Outer pentagon
      {0, 5}, {1, 6}, {2, 7}, {3, 8}, {4, 9},  // Rays between the two
  };
}

TEST(SimpleConnectedGraphsAreIsomorphicTest,
     PetersenGraphAndNonIsomorphicSibling) {
  Graph g1;
  SetGraphEdges(PetersenGraphEdges(), &g1);
  Graph g2;
  SetGraphEdges(PetersenSiblingGraphEdges(), &g2);

  EXPECT_EQ(
      SimpleConnectedGraphsAreIsomorphic(g1, g2, absl::InfiniteDuration()),
      GRAPHS_ARE_NOT_ISOMORPHIC);

  // Both g1 and g2 should be isomorphic to a random permutation of themselves.
  auto random = std::mt19937(1234);
  std::unique_ptr<Graph> permuted_g1 = RandomlyPermutedGraph(g1, random);
  std::unique_ptr<Graph> permuted_g2 = RandomlyPermutedGraph(g2, random);
  EXPECT_EQ(SimpleConnectedGraphsAreIsomorphic(g1, *permuted_g1,
                                               absl::InfiniteDuration()),
            GRAPHS_ARE_ISOMORPHIC);
  EXPECT_EQ(SimpleConnectedGraphsAreIsomorphic(g2, *permuted_g2,
                                               absl::InfiniteDuration()),
            GRAPHS_ARE_ISOMORPHIC);
}

TEST(SimpleConnectedGraphsAreIsomorphicAndComputeGraphFingerprintTest,
     RandomizedStressTestWithIsomorphicGraphs) {
  auto random = std::mt19937(1234);
  const int num_tests = DEBUG_MODE ? 10000 : 50000;
  for (int test = 0; test < num_tests;) {
    // Generate a "pure" simple random graph:
    // https://en.wikipedia.org/wiki/Erd%C5%91s%E2%80%93R%C3%A9nyi_model
    const int num_nodes = absl::LogUniform(random, 3, 8);
    const double avg_degree = 3 * absl::Exponential<double>(random);
    const bool symmetric = absl::Bernoulli(random, 1.0 / 2);
    const int num_arcs = static_cast<int>(num_nodes * avg_degree);
    if (num_arcs > num_nodes * (num_nodes - 1)) continue;
    const int num_edges = symmetric ? num_arcs / 2 : num_arcs;
    SCOPED_TRACE(absl::StrCat("Nodes: ", num_nodes, ", Edges: ", num_edges,
                              ", symmetric:", (symmetric ? "Yes" : "No")));
    std::unique_ptr<Graph> graph = util::GenerateRandomDirectedSimpleGraph(
        num_nodes, num_arcs, /*finalized=*/false, random);
    if (!util::GraphIsWeaklyConnected(*graph)) {
      continue;
    } else {
      test++;
    }
    if (symmetric) {
      AddReverseArcsAndFinalize(graph.get());
      graph = util::RemoveSelfArcsAndDuplicateArcs(*graph);
    } else {
      // Optionally add self arcs.
      if (absl::Bernoulli(random, 1.0 / 2)) {
        std::vector<int> all_nodes(num_nodes, -1);
        std::iota(all_nodes.begin(), all_nodes.end(), 0);
        std::shuffle(all_nodes.begin(), all_nodes.end(), random);
        const int num_self_arcs = absl::Uniform(random, 0, num_nodes + 1);
        for (int a = 0; a < num_self_arcs; ++a) {
          const int node = all_nodes[a];
          graph->AddArc(node, node);
        }
      }
      graph->Build();
    }
    std::unique_ptr<Graph> permuted_graph =
        RandomlyPermutedGraph(*graph, random);
    // Abort on the first failure: if the code is broken and they all fail, it
    // would generate a lot of output and make debugging harder.
    ASSERT_EQ(SimpleConnectedGraphsAreIsomorphic(*graph, *permuted_graph,
                                                 absl::InfiniteDuration()),
              GRAPHS_ARE_ISOMORPHIC);
    ASSERT_EQ(ComputeGraphFingerprint(*graph),
              ComputeGraphFingerprint(*permuted_graph))
        << "Graph:\n"
        << util::GraphToString(*graph, util::PRINT_GRAPH_ADJACENCY_LISTS_SORTED)
        << "\nPermuted Graph:\n"
        << util::GraphToString(*permuted_graph,
                               util::PRINT_GRAPH_ADJACENCY_LISTS_SORTED);
  }
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

TEST(ComputeGraphFingerprintTest, QuasiIsomorphicGraphs) {
  Graph g1;
  SetGraphEdges(PetersenGraphEdges(), &g1);
  Graph g2;
  SetGraphEdges(PetersenSiblingGraphEdges(), &g2);

  for (int effort = 0; effort < 30; ++effort) {
    SCOPED_TRACE(absl::StrCat("effort: ", effort));
    if (effort < kGraphFingerprintExtraRefineThreshold) {
      ASSERT_EQ(ComputeGraphFingerprint(g1, effort),
                ComputeGraphFingerprint(g2, effort));
    } else {
      // The refine heuristics kick in and can disambiguate the two graphs.
      ASSERT_NE(ComputeGraphFingerprint(g1, effort),
                ComputeGraphFingerprint(g2, effort));
    }
  }
}

TEST(ComputeGraphFingerprintTest, CollisionImpliesIsomorphismInPractice) {
  // We generate a bunch of small graphs and verify that if they do have the
  // same fingerprint, they are in fact isomorphic.
  std::mt19937 random(1234);
  constexpr int kNumGraphs = DEBUG_MODE ? 50'000 : 200'000;
  std::vector<std::unique_ptr<Graph>> graphs;
  absl::flat_hash_map<absl::uint128, int> fprint_to_index;
  int num_disconnected_graphs = 0;
  std::map<int, int> num_nodes_to_num_collisions;
  int num_symmetric_collisions = 0;
  int num_asymmetric_collisions = 0;
  int num_distinct_collisions_non_isomorphic = 0;
  absl::flat_hash_set<int> collision_indices;
  while (graphs.size() < kNumGraphs) {
    const int num_nodes = absl::Uniform<int>(random, 6, 16);
    const bool symmetric = absl::Bernoulli(random, 0.5);
    const int max_num_edges = num_nodes * (num_nodes - 1) / 2;
    const int num_arcs_or_edges = absl::Uniform<int>(
        absl::IntervalClosed, random, num_nodes - 1, max_num_edges);
    graphs.push_back(
        symmetric
            ? util::GenerateRandomUndirectedSimpleGraph(
                  num_nodes, num_arcs_or_edges, /*finalized=*/true, random)
            : util::GenerateRandomDirectedSimpleGraph(
                  num_nodes, num_arcs_or_edges, /*finalized=*/true, random));
    if (!util::GraphIsWeaklyConnected(*graphs.back())) {
      graphs.pop_back();
      ++num_disconnected_graphs;
      continue;
    }
    const int cur_index = graphs.size() - 1;
    const int index = gtl::LookupOrInsert(
        &fprint_to_index,
        ComputeGraphFingerprint(*graphs.back(),
                                kGraphFingerprintExtraRefineThreshold),
        cur_index);
    if (index != cur_index) {
      if (symmetric) {
        ++num_symmetric_collisions;
      } else {
        ++num_asymmetric_collisions;
      }
      ++num_nodes_to_num_collisions[num_nodes];
      VLOG(1) << "Collision: "
              << DUMP_VARS(num_nodes, num_arcs_or_edges, symmetric);
      const GraphIsomorphismDiagnosis diagnosis =
          SimpleConnectedGraphsAreIsomorphic(*graphs[index], *graphs[cur_index],
                                             absl::InfiniteDuration());
      if (diagnosis != GRAPHS_ARE_ISOMORPHIC) {
        ASSERT_EQ(diagnosis, GRAPHS_ARE_NOT_ISOMORPHIC);
        collision_indices.insert(cur_index);
        if (collision_indices.insert(index).second) {
          ++num_distinct_collisions_non_isomorphic;
          LOG(INFO) << "#" << index << ":\n"
                    << util::GraphToString(
                           *graphs[index],
                           util::PRINT_GRAPH_ADJACENCY_LISTS_SORTED)
                    << "\n#" << cur_index << ":\n"
                    << util::GraphToString(
                           *graphs[cur_index],
                           util::PRINT_GRAPH_ADJACENCY_LISTS_SORTED);
        }
      }
    }
  }

  // A run on 2022-01-20 printed:
  // kNumGraphs = 200000, num_distinct_collisions_non_isomorphic = 24,
  // num_disconnected_graphs = 25001, num_nodes_to_num_collisions = [
  // {6, 10950}, {7, 9452}, {8, 6084}, {9, 3474}, {10, 2499}, {11, 1955},
  // {12, 1569}, {13, 1238}, {14, 1086}, {15, 944}]
  LOG(INFO) << DUMP_VARS(kNumGraphs, num_distinct_collisions_non_isomorphic,
                         num_disconnected_graphs, num_nodes_to_num_collisions);
  EXPECT_LE(num_distinct_collisions_non_isomorphic, kNumGraphs / 5000);
  EXPECT_GE(num_symmetric_collisions, kNumGraphs / 10);
  EXPECT_GE(num_asymmetric_collisions, 30);
  // Verify that we did generate enough graph diversity.
  EXPECT_GE(fprint_to_index.size(), kNumGraphs / 2);
}

}  // namespace
}  // namespace operations_research
