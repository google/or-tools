// Copyright 2010-2014 Google
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


// This file contains various shortest paths utilities.
//
// Keywords: directed graph, cheapest path, shortest path, Dijkstra, spp.

#ifndef OR_TOOLS_GRAPH_SHORTESTPATHS_H_
#define OR_TOOLS_GRAPH_SHORTESTPATHS_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "base/integral_types.h"
#include "base/macros.h"

namespace operations_research {

// Dijsktra Shortest path with callback based description of the
// graph.  The callback returns the distance between two nodes, a
// distance of 'disconnected_distance' indicates no arcs between these
// two nodes. Ownership of the callback is taken by the function that
// will delete it in the end.  This function returns true if
// 'start_node' and 'end_node' are connected, false otherwise.
bool DijkstraShortestPath(int node_count, int start_node, int end_node,
                          std::function<int64(int, int)> graph,
                          int64 disconnected_distance, std::vector<int>* nodes);

// Bellman-Ford Shortest path with callback-based description of the
// graph.  The callback returns the distance between two nodes, a
// distance of 'disconnected_distance' indicates no arcs between these
// two nodes.  Ownership of the callback is taken by the function that
// will delete it in the end.  This function returns true if
// 'start_node' and 'end_node' are connected, false otherwise. If
// true, it will fill the 'nodes' vector with the sequence of nodes on
// the shortest path between 'start_node' and 'end_node'.
bool BellmanFordShortestPath(int node_count, int start_node, int end_node,
                             std::function<int64(int, int)> graph,
                             int64 disconnected_distance,
                             std::vector<int>* nodes);

// A* Shortest path with function based description of the
// graph.  The graph function returns the distance between two nodes, a
// distance of 'disconnected_distance' indicates no arcs between these
// two nodes. Additionally, the heuristic callback returns a
// an approximate distance between the node and the target, which guides
// the search. If the heuristic is admissible (ie. never overestimates cost),
// the A* algorithm returns an optimal solution.
// This function returns true if 'start_node' and 'end_node' are
// connected, false otherwise.
bool AStarShortestPath(int node_count, int start_node, int end_node,
                       std::function<int64(int, int)> graph,
                       std::function<int64(int)> heuristic,
                       int64 disconnected_distance, std::vector<int>* nodes);

}  // namespace operations_research
#endif  // OR_TOOLS_GRAPH_SHORTESTPATHS_H_
