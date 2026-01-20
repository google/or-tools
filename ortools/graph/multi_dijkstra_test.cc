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

#include "ortools/graph/multi_dijkstra.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <numeric>
#include <random>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/random/distributions.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/map_util.h"
#include "ortools/base/types.h"
#include "ortools/graph_base/connected_components.h"
#include "ortools/graph_base/graph.h"
#include "ortools/graph_base/random_graph.h"
#include "ortools/graph_base/util.h"

namespace operations_research {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(MultiDijkstraTest, SmallTest) {
  // On the following graph (with lengths divided by 10, to test non-integer).
  //
  //  .--------------[6]---------------.
  //  |                                v
  //  0 --[3]--> 1 --[0]--> 2 --[2]--> 4
  //             ^          |
  //             |          |
  //            [0]        [0]
  //             |          |
  //             '--- 3 <---'
  //
  util::Graph graph;
  std::vector<double> arc_lengths;
  graph.AddArc(0, 1);
  arc_lengths.push_back(0.3);
  graph.AddArc(0, 4);
  arc_lengths.push_back(0.6);
  graph.AddArc(1, 2);
  arc_lengths.push_back(0.0);
  graph.AddArc(2, 4);
  arc_lengths.push_back(0.2);
  graph.AddArc(2, 3);
  arc_lengths.push_back(0.0);
  graph.AddArc(3, 1);
  arc_lengths.push_back(0.0);

  EXPECT_THAT(
      MultiDijkstra<double>(
          graph, [&arc_lengths](int arc) { return arc_lengths[arc]; },
          {{0}, {1, 2}, {3, 4}, {4}, {}},
          [](int, int, double) -> bool { return false; }),
      ElementsAre(
          UnorderedElementsAre(Pair(0, DistanceAndParentArc<double>{0, -1}),
                               Pair(1, DistanceAndParentArc<double>{.3, 0}),
                               Pair(2, DistanceAndParentArc<double>{.3, 2}),
                               Pair(3, DistanceAndParentArc<double>{.3, 4}),
                               Pair(4, DistanceAndParentArc<double>{.5, 3})),
          UnorderedElementsAre(Pair(1, DistanceAndParentArc<double>{0, -1}),
                               Pair(2, DistanceAndParentArc<double>{0, -1}),
                               Pair(3, DistanceAndParentArc<double>{0, 4}),
                               Pair(4, DistanceAndParentArc<double>{.2, 3})),
          UnorderedElementsAre(Pair(3, DistanceAndParentArc<double>{0, -1}),
                               Pair(1, DistanceAndParentArc<double>{0, 5}),
                               Pair(2, DistanceAndParentArc<double>{0, 2}),
                               Pair(4, DistanceAndParentArc<double>{0, -1})),
          UnorderedElementsAre(Pair(4, DistanceAndParentArc<double>{0, -1})),
          IsEmpty()));
}

TEST(MultiDijkstraTest, RandomizedStressTest) {
  // Verify on random graphs that a few invariants are respected.
  // Non-exhaustive list:
  // - the output looks good: all nodes and arcs are valid integers, etc.
  //   Also, the parent arcs and their length is consistent with the
  //   node distances.
  // - the arc_length_functor is called at most once on each arc for each
  //   source, and was called for all of the returned "parent arcs".
  // - the settled_node_callback is called at most once on each (node, source)
  //   pair, and with a distance corresponding to the node's distance in the
  //   returned search tree from that source.
  // - the settled node callback may not be called on a source that has stopped
  //   its search.
  // - when a dijkstra search hasn't been stopped, verify that the set of
  //   reached nodes corresponds to that source's connected component.
  auto random = std::mt19937(1234);
  const int num_trials = DEBUG_MODE ? 1000 : 10000;
  const int max_num_nodes = 100;
  const int max_num_arcs = 200;
  const std::vector<int> kNumSources = {0, 1, 3, -1};
  for (int trial = 0; trial < num_trials; ++trial) {
    // Set up the input graph.
    const int num_nodes = absl::Uniform(random, 0, max_num_nodes);
    const int num_arcs =
        num_nodes == 0 ? 0 : absl::Uniform(random, 0, max_num_arcs);
    std::unique_ptr<util::StaticGraph<>> graph = util::GenerateRandomMultiGraph(
        num_nodes, num_arcs, /*finalized=*/true, random);

    // Set up the input source sets.
    int num_sources =
        kNumSources[absl::Uniform<int>(random, 0, kNumSources.size())];
    if (num_sources < 0) {
      num_sources = absl::Uniform(random, 1, num_nodes + 1);
    }
    std::vector<std::vector<int>> source_sets(num_sources);
    // Each source set gets 0 to 3 random nodes, not necessarily distinct.
    // Then, with 50% probability, we'll pick two random source sets and append
    // either Uniform(num_nodes) random nodes to them (not necessarily distinct)
    // or all nodes (distinct).
    for (std::vector<int>& source_set : source_sets) {
      const int size = num_nodes == 0 ? 0 : absl::Uniform(random, 0, 4);
      while (source_set.size() < size) {
        source_set.push_back(absl::Uniform(random, 0, num_nodes));
      }
    }
    if (num_sources > 0 && absl::Bernoulli(random, 0.5)) {
      for (int i = 0; i < 2; ++i) {
        const int source = absl::Uniform(random, 0, num_sources);
        if (absl::Bernoulli(random, 0.5)) {
          // Append Uniform(num_nodes) random nodes, with repetitions.
          const int num = absl::Uniform(random, 0, num_nodes);
          for (int j = 0; j < num; ++j) {
            source_sets[source].push_back(absl::Uniform(random, 0, num_nodes));
          }
        } else {
          // Append all nodes (shuffled).
          std::vector<int> shuffled_nodes(num_nodes, 0);
          std::iota(shuffled_nodes.begin(), shuffled_nodes.end(), 0);
          std::shuffle(shuffled_nodes.begin(), shuffled_nodes.end(), random);
          source_sets[source].insert(source_sets[source].end(),
                                     shuffled_nodes.begin(),
                                     shuffled_nodes.end());
        }
      }
    }

    // Set up the (tracked) arc length functor and settled node callbacks.
    std::vector<bool> search_was_stopped(num_sources, false);
    std::vector<double> search_stop_probability(num_sources, 0.0);
    for (double& stop_proba : search_stop_probability) {
      if (absl::Bernoulli(random, 0.5)) {
        stop_proba = 1.0 / absl::Uniform<int>(random, 1, num_nodes + 1);
      }
    }
    absl::flat_hash_map<int, int> num_arc_length_functor_calls;
    absl::flat_hash_map<int, int64_t> arc_length;
    std::vector<absl::flat_hash_map<int, int64_t>> settled_node_distance(
        num_sources);
    auto arc_length_functor = [&](int arc) -> int64_t {
      CHECK_GE(arc, 0);
      CHECK_LT(arc, graph->num_arcs());
      ++num_arc_length_functor_calls[arc];
      return gtl::LookupOrInsert(&arc_length, arc,
                                 absl::Uniform<int64_t>(random, 0, 1e12));
    };
    auto settled_node_callback = [&](int node, int source_index,
                                     int64_t distance) -> bool {
      CHECK(!search_was_stopped[source_index]);
      CHECK_GE(node, 0);
      CHECK_LT(node, num_nodes);
      CHECK_GE(source_index, 0);
      CHECK_LT(source_index, num_sources);
      CHECK(settled_node_distance[source_index].insert({node, distance}).second)
          << "In search #" << source_index << ", node #" << node
          << " was settled twice!";
      const bool stop =
          absl::Bernoulli(random, search_stop_probability[source_index]);
      if (stop) {
        search_was_stopped[source_index] = true;
      }
      return stop;
    };

    // Run the Dijkstra!
    const std::vector<absl::flat_hash_map<int, DistanceAndParentArc<int64_t>>>
        reached = MultiDijkstra<int64_t>(*graph, arc_length_functor,
                                         source_sets, settled_node_callback);

    // Verify the output.
    ASSERT_EQ(reached.size(), num_sources);
    for (int source_index = 0; source_index < num_sources; ++source_index) {
      // Verify that "reached[source_index]" forms a shortest path tree.
      for (const auto& p : reached[source_index]) {
        const int node = p.first;
        const int parent_arc = p.second.parent_arc;
        const int64_t distance = p.second.distance;
        ASSERT_GE(node, 0);
        ASSERT_LT(node, num_nodes);
        if (parent_arc == -1) {
          ASSERT_EQ(distance, 0);
        } else {
          ASSERT_GE(parent_arc, 0);
          ASSERT_LT(parent_arc, graph->num_arcs());
          ASSERT_TRUE(arc_length.contains(parent_arc));
          const int parent_node = graph->Tail(parent_arc);
          ASSERT_TRUE(reached[source_index].contains(parent_node));
          ASSERT_EQ(gtl::FindOrDie(reached[source_index], parent_node).distance,
                    distance - gtl::FindOrDie(arc_length, parent_arc));
        }
      }
      for (const auto& p : settled_node_distance[source_index]) {
        ASSERT_TRUE(reached[source_index].contains(p.first));
        ASSERT_EQ(gtl::FindOrDie(reached[source_index], p.first).distance,
                  p.second);
      }
      if (!search_was_stopped[source_index]) {
        if (source_sets[source_index].empty()) {
          ASSERT_EQ(reached[source_index].size(), 0);
        } else {
          // All sources have been settled with distance 0.
          for (const int source : source_sets[source_index]) {
            ASSERT_TRUE(settled_node_distance[source_index].contains(source));
            ASSERT_EQ(
                gtl::FindOrDie(settled_node_distance[source_index], source), 0);
          }
          // All reached nodes have been settled.
          ASSERT_EQ(reached[source_index].size(),
                    settled_node_distance[source_index].size());
          // Run a BFS from the source set and verify that we reach the same
          // number of nodes.
          std::vector<int> bfs_queue;
          std::vector<bool> touched(num_nodes, false);
          for (const int src : source_sets[source_index]) {
            if (!touched[src]) {
              touched[src] = true;
              bfs_queue.push_back(src);
            }
          }
          int num_visited = 0;
          while (num_visited < bfs_queue.size()) {
            const int node = bfs_queue[num_visited++];
            for (const int neigh : (*graph)[node]) {
              if (!touched[neigh]) {
                touched[neigh] = true;
                bfs_queue.push_back(neigh);
              }
            }
          }
          ASSERT_EQ(reached[source_index].size(), bfs_queue.size());
        }
      }
    }
    for (const auto& p : num_arc_length_functor_calls) {
      ASSERT_LE(p.second, num_sources);
    }
  }
}

}  // namespace
}  // namespace operations_research
