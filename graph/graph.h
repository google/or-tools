// Copyright 2010-2011 Google
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

#ifndef OR_TOOLS_GRAPH_GRAPH_H_
#define OR_TOOLS_GRAPH_GRAPH_H_

#include <vector>

#include "base/callback.h"
#include "base/scoped_ptr.h"

template <class R, class A1, class A2> class ResultCallback2;
template <class R, class A1> class ResultCallback1;

namespace operations_research {

// This will find all maximal cliques, even with size 1, in the
// graph described by the graph callback. graph->Run(i, j) indicates
// if there is an arc between i and j.
// This method will delete the callbacks after it has run.
// It will call the 'callback' upon each cliques.
// If the 'callback' returns true, then the search for cliques will stop.
void FindCliques(ResultCallback2<bool, int, int>* const graph,
                 int node_count,
                 ResultCallback1<bool, const vector<int>&>* const callback);

// This function will cover the maximum number of arcs of the graph
// with cliques. The graph is described by the graph
// callback. graph->Run(i, j) indicates if there is an arc between i
// and j.  This method will delete the callbacks after it has run.  It
// will call the 'callback' upon each cliques. It will ignore cliques
// of size one.
void CoverArcsByCliques(
    ResultCallback2<bool, int, int>* const graph,
    int node_count,
    ResultCallback1<bool, const vector<int>&>* const callback);

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_GRAPH_H_
