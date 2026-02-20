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

#ifndef ORTOOLS_GRAPH_DAG_CONNECTIVITY_H_
#define ORTOOLS_GRAPH_DAG_CONNECTIVITY_H_

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "ortools/util/bitset.h"

namespace operations_research {

// Given a directed graph, as defined by the arc list "arcs", computes either:
//   1. If the graph is acyclic, the matrix of values x, where x[i][j] indicates
//      that there is a directed path from i to j.
//   2. If the graph is cyclic, "error_cycle_out" is set to contain the cycle,
//      and the return value is empty.
//
// The algorithm runs in O(num_nodes^2 + num_nodes*num_arcs).
//
// Inputs:
//   arcs: each a in "arcs" is a directed edge from a.first to a.second.  Must
//         have a.first, a.second >= 0.  The graph is assumed to have nodes
//         {0,1,...,max_{a in arcs} max(a.first, a.second)}, or have no nodes
//         if arcs is the empty list.
//   error_was_cyclic: output arg, is set to true if a cycle is detected.
//   error_cycle_out: output arg, if a cycle is detected, error_cycle_out is
//                    set to contain the nodes of the cycle in order.
//
// Note: useful for computing the transitive closure of a binary relation, e.g.
// given the relation i < j for i,j in S that is transitive and some known
// values i < j, create a node for each i in S and an arc for each known
// relationship.  Then any relationship implied by transitivity is given by
// the resulting matrix produced, or if the relation fails transitivity, a cycle
// proving this is produced.
std::vector<Bitset64<int64_t>> ComputeDagConnectivity(
    absl::Span<const std::pair<int, int>> arcs, bool* error_was_cyclic,
    std::vector<int>* error_cycle_out);

// Like above, but will CHECK fail if the digraph with arc list "arcs"
// contains a cycle.
std::vector<Bitset64<int64_t>> ComputeDagConnectivityOrDie(
    absl::Span<const std::pair<int, int>> arcs);

}  // namespace operations_research

#endif  // ORTOOLS_GRAPH_DAG_CONNECTIVITY_H_
