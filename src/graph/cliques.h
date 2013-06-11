// Copyright 2010-2013 Google
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
//
// Maximal clique algorithms, based on the Bron-Kerbosch algorithm.
// See http://en.wikipedia.org/wiki/Bron-Kerbosch_algorithm
// and
// C. Bron and J. Kerbosch, Joep, "Algorithm 457: finding all cliques of an
// undirected graph", CACM 16 (9): 575–577, 1973.
// http://dl.acm.org/citation.cfm?id=362367&bnc=1
//
// Keywords: undirected graph, clique, clique cover, Bron, Kerbosch.

#ifndef OR_TOOLS_GRAPH_CLIQUES_H_
#define OR_TOOLS_GRAPH_CLIQUES_H_

#include <vector>

#include "base/callback.h"
#include "base/scoped_ptr.h"

template <class R, class A1, class A2> class ResultCallback2;
template <class R, class A1> class ResultCallback1;

namespace operations_research {

// Finds all maximal cliques, even of size 1, in the
// graph described by the graph callback. graph->Run(i, j) indicates
// if there is an arc between i and j.
// This function takes ownership of 'callback' and deletes it after it has run.
// If 'callback' returns true, then the search for cliques stops.
void FindCliques(ResultCallback2<bool, int, int>* const graph,
                 int node_count,
                 ResultCallback1<bool, const std::vector<int>&>* const callback);

// Covers the maximum number of arcs of the graph with cliques. The graph
// is described by the graph callback. graph->Run(i, j) indicates if
// there is an arc between i and j.
// This function takes ownership of 'callback' and deletes it after it has run.
// It calls 'callback' upon each clique.
// It ignores cliques of size 1.
void CoverArcsByCliques(
    ResultCallback2<bool, int, int>* const graph,
    int node_count,
    ResultCallback1<bool, const std::vector<int>&>* const callback);

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_CLIQUES_H_
