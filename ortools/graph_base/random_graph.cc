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

#include "ortools/graph_base/random_graph.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/random.h"
#include "ortools/base/logging.h"
#include "ortools/base/types.h"

namespace util {

namespace {
// This function initializes the graph used by GenerateRandomMultiGraph() and
// GenerateRandomMultiGraph(), given their arguments.
// See the .h for documentation on those arguments.
std::unique_ptr<StaticGraph<>> CreateGraphMaybeReserved(int num_nodes,
                                                        int num_arcs,
                                                        bool finalized,
                                                        absl::BitGenRef gen) {
  std::unique_ptr<StaticGraph<>> graph;
  // We either "reserve" the number of nodes and arcs or not, depending on
  // randomness and on the "finalized" bit (if false, we can't assume that the
  // user won't add some nodes or arcs after we return the graph, so we can't
  // cap those).
  if (finalized && absl::Bernoulli(gen, 1.0 / 2)) {
    graph = std::make_unique<StaticGraph<>>(num_nodes, num_arcs);
  } else {
    graph = std::make_unique<StaticGraph<>>();
    graph->AddNode(num_nodes - 1);
  }
  return graph;
}
}  // namespace

std::unique_ptr<StaticGraph<>> GenerateRandomMultiGraph(int num_nodes,
                                                        int num_arcs,
                                                        bool finalized,
                                                        absl::BitGenRef gen) {
  std::unique_ptr<StaticGraph<>> graph =
      CreateGraphMaybeReserved(num_nodes, num_arcs, finalized, gen);
  if (num_nodes != 0) {
    CHECK_GT(num_nodes, 0);
    CHECK_GE(num_arcs, 0);
    graph->AddNode(num_nodes - 1);
    for (int a = 0; a < num_arcs; ++a) {
      graph->AddArc(absl::Uniform(gen, 0, num_nodes),
                    absl::Uniform(gen, 0, num_nodes));
    }
  } else {
    CHECK_EQ(num_arcs, 0);
  }
  if (finalized) graph->Build();
  return graph;
}

namespace {
// Parameterized method to generate both directed and undirected simple graphs.
std::unique_ptr<StaticGraph<>> GenerateRandomSimpleGraph(int num_nodes,
                                                         int num_arcs,
                                                         bool finalized,
                                                         bool directed,
                                                         absl::BitGenRef gen) {
  CHECK_GE(num_nodes, 0);
  // For an undirected graph, the number of arcs should be even: a->b and b->a.
  CHECK(directed || (num_arcs % 2 == 0));
  const int64_t max_num_arcs =
      static_cast<int64_t>(num_nodes) * (num_nodes - 1);
  CHECK_LE(num_arcs, max_num_arcs);
  std::unique_ptr<StaticGraph<>> graph =
      CreateGraphMaybeReserved(num_nodes, num_arcs, finalized, gen);

  // If the number of arcs is greater than half the possible arcs of the graph,
  // we generate the inverse graph and convert non-arcs to arcs.
  if (num_arcs > max_num_arcs / 2) {
    std::unique_ptr<StaticGraph<>> inverse_graph =
        GenerateRandomSimpleGraph(num_nodes, max_num_arcs - num_arcs,
                                  /*finalized=*/true, directed, gen);
    std::vector<bool> node_mask(num_nodes, false);
    for (int from = 0; from < num_nodes; ++from) {
      for (const int to : (*inverse_graph)[from]) {
        node_mask[to] = true;
      }
      for (int to = 0; to < num_nodes; ++to) {
        if (node_mask[to]) {
          node_mask[to] = false;  // So that the mask is reset to all false.
        } else if (to != from) {
          graph->AddArc(from, to);
        }
      }
    }
    if (finalized) graph->Build();
    return graph;
  }

  // We use a trivial algorithm: pick an arc at random, uniformly, and add it to
  // the graph unless it was already added. As we sometimes have to discard an
  // arc, we expect to do this slightly more times than the desired number "m"
  // of distinct arcs. But in the worst case, which is when m = M/2 (where M =
  // N*(N-1) is the number of possible arcs), the expected number of steps is
  // only ln(2)*M ~ 0.69*M, to produce 0.5*M arcs. So it's fine.
  //
  // Proof: The expected number of steps to get "m" distinct arcs across the M
  // possible arcs is M/M + M/(M-1) + M/(M-2) + ... + M/(M-m+1), which is equal
  // to M * (H(M) - H(M-m)), where H(x) is the harmonic sum up to x.
  // H(M) - H(M-m) converges to ln(M) - ln(M-m) = ln(1 + m/(M-m)) as M grows,
  // which stricly grows with m and is equal to ln(2) in the worst case m=M/2.
  //
  // NOTE(user): If some specialized users want a uniform generation method
  // that uses less memory (this one uses a flat hash map on the arcs, which
  // uses significant memory), it could be done. Reach out to me.
  absl::flat_hash_set<std::pair<int, int>> arc_set;
  // To detect bad user-provided random number generator which could lead to
  // infinite loops, we bound the number of iterations to a value well beyond
  // the expected number of iterations (which is less than 0.69 * max_num_arcs).
  int64_t num_iterations = 0;
  const int64_t max_num_iterations = 1000 + max_num_arcs;
  while (graph->num_arcs() < num_arcs) {
    ++num_iterations;
    CHECK_LE(num_iterations, max_num_iterations)
        << "The random number generator supplied to GenerateRandomSimpleGraph()"
        << " is likely biased or broken.";
    const int tail = absl::Uniform(gen, 0, num_nodes);
    const int head = absl::Uniform(gen, 0, num_nodes);
    if (tail == head) continue;
    if (directed) {
      if (!arc_set.insert({tail, head}).second) continue;
      graph->AddArc(tail, head);
    } else {  // undirected
      const std::pair<int, int> arc = {
          std::min(tail, head),
          std::max(tail, head)};  // Canonic edge representative.
      if (!arc_set.insert(arc).second) continue;
      graph->AddArc(tail, head);
      graph->AddArc(head, tail);
    }
  }
  if (finalized) graph->Build();
  return graph;
}
}  // namespace

std::unique_ptr<StaticGraph<>> GenerateRandomDirectedSimpleGraph(
    int num_nodes, int num_arcs, bool finalized, absl::BitGenRef gen) {
  return GenerateRandomSimpleGraph(num_nodes, num_arcs, finalized,
                                   /*directed=*/true, gen);
}

std::unique_ptr<StaticGraph<>> GenerateRandomUndirectedSimpleGraph(
    int num_nodes, int num_edges, bool finalized, absl::BitGenRef gen) {
  return GenerateRandomSimpleGraph(num_nodes, 2 * num_edges, finalized,
                                   /*directed=*/false, gen);
}

}  // namespace util
