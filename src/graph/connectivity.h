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

// Graph connectivity algorithm for undirected graphs.
// Memory consumption: O(n) where m is the number of arcs and n the number
// of nodes.
// TODO(user): add depth-first-search based connectivity for directed graphs.
// TODO(user): add depth-first-search based biconnectivity for directed graphs.

#ifndef OR_TOOLS_GRAPH_CONNECTIVITY_H_
#define OR_TOOLS_GRAPH_CONNECTIVITY_H_

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "graph/ebert_graph.h"

namespace operations_research {

// Template class implementing a Union-Find algorithm with path compression for
// maintaining the connected components of a graph.
// See Cormen et al. 2nd Edition. MIT Press, 2001. ISBN 0-262-03293-7.
// Chapter 21: Data structures for Disjoint Sets, pp. 498–524.
// and Tarjan (1975). Efficiency of a Good But Not Linear Set
// Union Algorithm. Journal of the ACM 22(2):215–225
// It is implemented as a template so that the size of NodeIndex can be chosen
// depending on the size of the graphs considered.
// The main interest is that arcs do not need to be kept. Thus the memory
// complexity is O(n) where n is the number of nodes in the graph.
// The complexity of this algorithm is O(n . alpha(n)) where alpha(n) is
// the inverse Ackermann function. alpha(n) <= log(log(log(..log(log(n))..)
// In practice alpha(n) <= 5.
// See Tarjan and van Leeuwen (1984). Worst-case analysis of set union
// algorithms. Journal of the ACM 31(2):245–281.
//
// Usage example:
// ConnectedComponents components;
// components.Init(num_nodes);
// for (ArcIndex arc = 0; arc < num_arcs; ++arc) {
//   components.AddArc(tail[arc], head[arc]);
// }
// int num_connected_components = components.GetNumberOfConnectedComponents();
// if (num_connected_components == 1) {
//   // Graph is completely connected.
// }
// // Group the nodes in the same connected component together.
// // group[class_number][i] contains the i-th node in group class_number.
// hash_map<NodeIndex, std::vector<NodeIndex> > group(num_connected_components);
// for (NodeIndex node = 0; node < num_nodes; ++node) {
//   group[components.GetClassRepresentative(node)].push_back(node);
// }
//
// NodeIndex is used to denote both a node index and a number of nodes,
// as passed as parameter to Init.
//
// Keywords: graph, connected components.

class ConnectedComponents {
 public:
  ConnectedComponents() : min_index_(0),
                          max_index_(StarGraph::kMaxNumNodes),
                          max_seen_index_(0),
                          class_(),
                          class_size_() {}

  ~ConnectedComponents() {}

  // Reserves memory for num_nodes and resets the data structures.
  void Init(NodeIndex num_nodes) {
    Init(0, num_nodes - 1);
  }

  // Adds the information that NodeIndex tail and NodeIndex head are connected.
  void AddArc(NodeIndex tail, NodeIndex head);

  // Adds a complete StarGraph to the object. Note that Depth-First Search
  // is a better algorithm for finding connected components on graphs.
  // TODO(user): implement Depth-First Search-based connected components finder.
  void AddGraph(const StarGraph& graph);

  // Compresses the path for node.
  NodeIndex CompressPath(NodeIndex node);

  // Returns the equivalence class representative for node.
  NodeIndex GetClassRepresentative(NodeIndex node);

  // Returns the number of connected components. Allocates num_nodes_ bits for
  // the computation.
  NodeIndex GetNumberOfConnectedComponents();

  // Merges the equivalence classes of node1 and node2.
  void MergeClasses(NodeIndex node1, NodeIndex node2);

 private:
  // Initializes the object and allocates memory.
  void Init(NodeIndex min_index, NodeIndex max_index);

  // The minimum index for nodes in the graph.
  NodeIndex      min_index_;

  // The exact number of nodes in the graph.
  NodeIndex      max_index_;

  // The maximum node index seen during AddArc. (set to Graph::num_nodes() by
  // AddGraph.)
  NodeIndex      max_seen_index_;

  // The equivalence class representative for each node.
  NodeIndexArray class_;

  // The size of each equivalence class of each node. Used to compress the paths
  // and therefore achieve better time complexity.
  NodeIndexArray class_size_;

  DISALLOW_COPY_AND_ASSIGN(ConnectedComponents);
};

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_CONNECTIVITY_H_
