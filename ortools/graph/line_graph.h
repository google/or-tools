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

#ifndef OR_TOOLS_GRAPH_LINE_GRAPH_H_
#define OR_TOOLS_GRAPH_LINE_GRAPH_H_

#include "ortools/base/logging.h"

namespace operations_research {

// Builds a directed line graph for `graph` (see "directed line graph" in
// http://en.wikipedia.org/wiki/Line_graph). Arcs of the original graph
// become nodes and the new graph contains only nodes created from arcs in the
// original graph (we use the notation (a->b) for these new nodes); the index
// of the node (a->b) in the new graph is exactly the same as the index of the
// arc a->b in the original graph.
// An arc from node (a->b) to node (c->d) in the new graph is added if and only
// if b == c in the original graph.
// This method expects that `line_graph` is an empty graph (it has no nodes
// and no arcs).
// Returns false on an error.
template <typename GraphType>
bool BuildLineGraph(const GraphType& graph, GraphType* const line_graph) {
  if (line_graph == nullptr) {
    LOG(DFATAL) << "line_graph must not be NULL";
    return false;
  }
  if (line_graph->num_nodes() != 0) {
    LOG(DFATAL) << "line_graph must be empty";
    return false;
  }
  // Sizing then filling.
  using NodeIndex = typename GraphType::NodeIndex;
  using ArcIndex = typename GraphType::ArcIndex;
  ArcIndex num_arcs = 0;
  for (const ArcIndex arc : graph.AllForwardArcs()) {
    const NodeIndex head = graph.Head(arc);
    num_arcs += graph.OutDegree(head);
  }
  line_graph->Reserve(graph.num_arcs(), num_arcs);
  for (const ArcIndex arc : graph.AllForwardArcs()) {
    const NodeIndex head = graph.Head(arc);
    for (const ArcIndex outgoing_arc : graph.OutgoingArcs(head)) {
      line_graph->AddArc(arc, outgoing_arc);
    }
  }
  return true;
}

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_LINE_GRAPH_H_
