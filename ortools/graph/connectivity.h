// Copyright 2010-2018 Google LLC
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

#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"

namespace operations_research {

// Template class implementing a Union-Find algorithm with path compression for
// maintaining the connected components of a graph.
// See Cormen et al. 2nd Edition. MIT Press, 2001. ISBN 0-262-03293-7.
// Chapter 21: Data structures for Disjoint Sets, pp. 498-524.
// and Tarjan (1975). Efficiency of a Good But Not Linear Set
// Union Algorithm. Journal of the ACM 22(2):215-225
// It is implemented as a template so that the size of NodeIndex can be chosen
// depending on the size of the graphs considered.
// The main interest is that arcs do not need to be kept. Thus the memory
// complexity is O(n) where n is the number of nodes in the graph.
// The complexity of this algorithm is O(n . alpha(n)) where alpha(n) is
// the inverse Ackermann function. alpha(n) <= log(log(log(..log(log(n))..)
// In practice alpha(n) <= 5.
// See Tarjan and van Leeuwen (1984). Worst-case analysis of set union
// algorithms. Journal of the ACM 31(2):245-281.
//
// Usage example:
// ConnectedComponents<int, int> components;
// components.Init(num_nodes);
// for (int arc = 0; arc < num_arcs; ++arc) {
//   components.AddArc(tail[arc], head[arc]);
// }
// int num_connected_components = components.GetNumberOfConnectedComponents();
// if (num_connected_components == 1) {
//   // Graph is completely connected.
// }
// // Group the nodes in the same connected component together.
// // group[class_number][i] contains the i-th node in group class_number.
// hash_map<int, std::vector<int> > group(num_connected_components);
// for (int node = 0; node < num_nodes; ++node) {
//   group[components.GetClassRepresentative(node)].push_back(node);
// }
//
// Keywords: graph, connected components.

template <typename NodeIndex, typename ArcIndex>
class ConnectedComponents {
 public:
  ConnectedComponents() : num_nodes_(0), class_(), class_size_() {}

  // Reserves memory for num_nodes and resets the data structures.
  void Init(NodeIndex num_nodes) {
    CHECK_GE(num_nodes, 0);
    num_nodes_ = num_nodes;
    class_.resize(num_nodes_);
    class_size_.assign(num_nodes_, 1);
    for (NodeIndex node = 0; node < num_nodes_; ++node) {
      class_[node] = node;
    }
  }

  // Adds the information that NodeIndex tail and NodeIndex head are connected.
  void AddArc(NodeIndex tail, NodeIndex head) {
    const NodeIndex tail_class = CompressPath(tail);
    const NodeIndex head_class = CompressPath(head);
    if (tail_class != head_class) {
      MergeClasses(tail_class, head_class);
    }
  }

  // Adds a complete StarGraph to the object. Note that Depth-First Search
  // is a better algorithm for finding connected components on graphs.
  // TODO(user): implement Depth-First Search-based connected components finder.
  template <typename Graph>
  void AddGraph(const Graph& graph) {
    Init(graph.num_nodes());
    for (NodeIndex tail = 0; tail < graph.num_nodes(); ++tail) {
      for (typename Graph::OutgoingArcIterator it(graph, tail); it.Ok();
           it.Next()) {
        AddArc(tail, graph.Head(it.Index()));
      }
    }
  }

  // Compresses the path for node.
  NodeIndex CompressPath(NodeIndex node) {
    CheckNodeBounds(node);
    NodeIndex parent = node;
    while (parent != class_[parent]) {
      CheckNodeBounds(class_[parent]);
      CheckNodeBounds(class_[class_[parent]]);
      parent = class_[parent];
    }
    while (node != class_[node]) {
      const NodeIndex old_parent = class_[node];
      class_[node] = parent;
      node = old_parent;
    }
    return parent;
  }

  // Returns the equivalence class representative for node.
  NodeIndex GetClassRepresentative(NodeIndex node) {
    return CompressPath(node);
  }

  // Returns the number of connected components. Allocates num_nodes_ bits for
  // the computation.
  NodeIndex GetNumberOfConnectedComponents() {
    NodeIndex number = 0;
    for (NodeIndex node = 0; node < num_nodes_; ++node) {
      if (class_[node] == node) ++number;
    }
    return number;
  }

  // Merges the equivalence classes of node1 and node2.
  void MergeClasses(NodeIndex node1, NodeIndex node2) {
    // It's faster (~10%) to swap the two values and have a single piece of
    // code for merging the classes.
    CheckNodeBounds(node1);
    CheckNodeBounds(node2);
    if (class_size_[node1] < class_size_[node2]) {
      std::swap(node1, node2);
    }
    class_[node2] = node1;
    class_size_[node1] += class_size_[node2];
  }

 private:
  void CheckNodeBounds(NodeIndex node_index) {
    DCHECK_LE(0, node_index);
    DCHECK_LT(node_index, num_nodes_);
  }
  // The exact number of nodes in the graph.
  NodeIndex num_nodes_;

  // The equivalence class representative for each node.
  std::vector<NodeIndex> class_;

  // The size of each equivalence class of each node. Used to compress the paths
  // and therefore achieve better time complexity.
  std::vector<NodeIndex> class_size_;

  DISALLOW_COPY_AND_ASSIGN(ConnectedComponents);
};

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_CONNECTIVITY_H_
