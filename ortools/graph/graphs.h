// Copyright 2010-2017 Google
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

// Temporary utility class needed as long as we have two slightly
// different graph interface: The one in ebert_graph.h and the one in graph.h

#ifndef OR_TOOLS_GRAPH_GRAPHS_H_
#define OR_TOOLS_GRAPH_GRAPHS_H_

#include "ortools/graph/ebert_graph.h"

namespace operations_research {

// Since StarGraph does not have exactly the same interface as the other
// graphs, we define a correspondence there.
template <typename Graph>
struct Graphs {
  typedef typename Graph::ArcIndex ArcIndex;
  typedef typename Graph::NodeIndex NodeIndex;
  static ArcIndex OppositeArc(const Graph& graph, ArcIndex arc) {
    return graph.OppositeArc(arc);
  }
  static bool IsArcValid(const Graph& graph, ArcIndex arc) {
    return graph.IsArcValid(arc);
  }
  static NodeIndex NodeReservation(const Graph& graph) {
    return graph.node_capacity();
  }
  static ArcIndex ArcReservation(const Graph& graph) {
    return graph.arc_capacity();
  }
  static void Build(Graph* graph) { graph->Build(); }
  static void Build(Graph* graph, std::vector<ArcIndex>* permutation) {
    graph->Build(permutation);
  }
};

template <>
struct Graphs<operations_research::StarGraph> {
  typedef operations_research::StarGraph Graph;
#if defined(_MSC_VER)
  typedef Graph::ArcIndex ArcIndex;
  typedef Graph::NodeIndex NodeIndex;
#else
  typedef typename Graph::ArcIndex ArcIndex;
  typedef typename Graph::NodeIndex NodeIndex;
#endif
  static ArcIndex OppositeArc(const Graph& graph, ArcIndex arc) {
    return graph.Opposite(arc);
  }
  static bool IsArcValid(const Graph& graph, ArcIndex arc) {
    return graph.CheckArcValidity(arc);
  }
  static NodeIndex NodeReservation(const Graph& graph) {
    return graph.max_num_nodes();
  }
  static ArcIndex ArcReservation(const Graph& graph) {
    return graph.max_num_arcs();
  }
  static void Build(Graph* graph) {}
  static void Build(Graph* graph, std::vector<ArcIndex>* permutation) {
    permutation->clear();
  }
};

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_GRAPHS_H_
