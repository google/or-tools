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

// A collection of functions to be used in unit tests involving the
// ortools/graph/... library.

#ifndef UTIL_GRAPH_RANDOM_GRAPH_H_
#define UTIL_GRAPH_RANDOM_GRAPH_H_

#include <memory>

#include "absl/random/bit_gen_ref.h"
#include "ortools/graph/graph.h"

namespace util {

// Generates a random graph where multi-arcs and self-arcs are allowed (and
// therefore expected): exactly "num_arcs" are generated, each from a node
// picked uniformly at random to another node picked uniformly at random.
// Calls Build() on the graph iff "finalized" is true.
std::unique_ptr<StaticGraph<>> GenerateRandomMultiGraph(int num_nodes,
                                                        int num_arcs,
                                                        bool finalized,
                                                        absl::BitGenRef gen);

// Like GenerateRandomMultiGraph(), but with neither multi-arcs nor self-arcs:
// the generated graph will have exactly num_arcs arcs. It will be picked
// uniformly at random from the set of all simple graphs with that number of
// nodes and arcs.
std::unique_ptr<StaticGraph<>> GenerateRandomDirectedSimpleGraph(
    int num_nodes, int num_arcs, bool finalized, absl::BitGenRef gen);

// Like GenerateRandomDirectedSimpleGraph(), but where an undirected edge is
// represented by two arcs: a->b and b->a. As a result, the amount of arcs in
// the generated graph is 2*num_edges.
std::unique_ptr<StaticGraph<>> GenerateRandomUndirectedSimpleGraph(
    int num_nodes, int num_edges, bool finalized, absl::BitGenRef gen);

}  // namespace util

#endif  // UTIL_GRAPH_RANDOM_GRAPH_H_
