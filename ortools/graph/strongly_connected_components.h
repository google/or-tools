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

// This code computes the strongly connected components of a directed graph,
// and presents them sorted by reverse topological order.
//
// It implements an efficient version of Tarjan's strongly connected components
// algorithm published in: Tarjan, R. E. (1972), "Depth-first search and linear
// graph algorithms", SIAM Journal on Computing.
//
// A description can also be found here:
// http://en.wikipedia.org/wiki/Tarjan%27s_strongly_connected_components_algorithm
//
// SIMPLE EXAMPLE:
//
// Fill a vector<vector<int>> graph; representing your graph adjacency lists.
// That is, graph[i] contains the nodes adjacent to node #i. The nodes must be
// integers in [0, num_nodes). Then just do:
//
// vector<vector<int>> components;
// FindStronglyConnectedComponents(
//     static_cast<int>(graph.size()), graph, &components);
//
// The nodes of each strongly connected components will be listed in each
// subvector of components. The components appear in reverse topological order:
// outgoing arcs from a component will only be towards earlier components.
//
// IMPORTANT: num_nodes will be the number of nodes of the graph. Its type
// is the type used internally by the algorithm. It is why it is better to
// convert it to int or even int32 rather than using size_t which takes 64 bits.

#ifndef UTIL_GRAPH_STRONGLY_CONNECTED_COMPONENTS_H_
#define UTIL_GRAPH_STRONGLY_CONNECTED_COMPONENTS_H_

#include <limits>
#include <vector>

#include "ortools/base/logging.h"
#include "ortools/base/macros.h"

// Finds the strongly connected components of a directed graph. It is templated
// so it can be used in many contexts. See the simple example above for the
// easiest use case.
//
// The requirement of the different types are:
// - The type NodeIndex must be an integer type representing a node of the
//   graph. The nodes must be in [0, num_nodes). It can be unsigned.
// - The type Graph must provide a [] operator such that the following code
//   iterates over the adjacency list of the given node:
//     for (const NodeIndex head : graph[node]) {}
// - The type SccOutput must implement the function:
//     emplace_back(NodeIndex const* begin, NodeIndex const* end);
//   It will be called with the connected components of the given graph as they
//   are found (In the reverse topological order).
//
// More practical details on the algorithm:
// - It deals properly with self-loop and duplicate nodes.
// - It is really fast! and work in O(nodes + edges).
// - Its memory usage is also bounded by O(nodes + edges) but in practice it
//   uses less than the input graph.
template <typename NodeIndex, typename Graph, typename SccOutput>
void FindStronglyConnectedComponents(const NodeIndex num_nodes,
                                     const Graph& graph, SccOutput* components);

// A simple custom output class that just counts the number of SCC. Not
// allocating many vectors can save both space and speed if your graph is large.
//
// Note: If this matters, you probably don't want to use vector<vector<int>> as
// an input either. See StaticGraph in ortools/graph/graph.h
// for an efficient graph data structure compatible with this algorithm.
template <typename NodeIndex>
struct SccCounterOutput {
  int number_of_components = 0;
  void emplace_back(NodeIndex const* b, NodeIndex const* e) {
    ++number_of_components;
  }
  // This is just here so this class can transparently replace a code that
  // use vector<vector<int>> as an SccOutput, and get its size with size().
  int size() const { return number_of_components; }
};

// This implementation is slightly different than a classical iterative version
// of Tarjan's strongly connected components algorithm. But basically it is
// still an iterative DFS. We use a class so memory can be reused if one needs
// to compute many SCC in a row. It also allows more complex behavior in the
// Graph or SccOutput class that might inspect the current state of the
// algorithm.
//
// TODO(user): Possible optimizations:
// - Try to reserve the vectors which sizes are bounded by num_nodes.
// - Use an index rather than doing push_back(), pop_back() on them.
template <typename NodeIndex, typename Graph, typename SccOutput>
class StronglyConnectedComponentsFinder {
 public:
  void FindStronglyConnectedComponents(const NodeIndex num_nodes,
                                       const Graph& graph,
                                       SccOutput* components) {
    // Reset the class fields.
    scc_stack_.clear();
    scc_start_index_.clear();
    node_index_.assign(num_nodes, 0);
    node_to_process_.clear();

    // Optimization. This will always be equal to scc_start_index_.back() except
    // when scc_stack_ is empty, in which case its value does not matter.
    NodeIndex current_scc_start = 0;

    // Loop over all the nodes not yet settled and start a DFS from each of
    // them.
    for (NodeIndex base_node = 0; base_node < num_nodes; ++base_node) {
      if (node_index_[base_node] != 0) continue;
      DCHECK_EQ(0, node_to_process_.size());
      node_to_process_.push_back(base_node);
      do {
        const NodeIndex node = node_to_process_.back();
        const NodeIndex index = node_index_[node];
        if (index == 0) {
          // We continue the dfs from this node and set its 1-based index.
          scc_stack_.push_back(node);
          current_scc_start = scc_stack_.size();
          node_index_[node] = current_scc_start;
          scc_start_index_.push_back(current_scc_start);

          // Enqueue all its adjacent nodes.
          NodeIndex min_head_index = kSettledIndex;
          for (const NodeIndex head : graph[node]) {
            const NodeIndex head_index = node_index_[head];
            if (head_index == 0) {
              node_to_process_.push_back(head);
            } else {
              // Note that if head_index == kSettledIndex, nothing happens.
              min_head_index = std::min(min_head_index, head_index);
            }
          }

          // Update the start of this strongly connected component.
          // Note that scc_start_index_ can never be empty since it first
          // element is 1 and by definition min_head_index is 1-based and can't
          // be 0.
          while (current_scc_start > min_head_index) {
            scc_start_index_.pop_back();
            current_scc_start = scc_start_index_.back();
          }
        } else {
          node_to_process_.pop_back();
          if (current_scc_start == index) {
            // We found a strongly connected component.
            components->emplace_back(&scc_stack_[current_scc_start - 1],
                                     &scc_stack_[0] + scc_stack_.size());
            for (int i = current_scc_start - 1; i < scc_stack_.size(); ++i) {
              node_index_[scc_stack_[i]] = kSettledIndex;
            }
            scc_stack_.resize(current_scc_start - 1);
            scc_start_index_.pop_back();
            current_scc_start =
                scc_start_index_.empty() ? 0 : scc_start_index_.back();
          }
        }
      } while (!node_to_process_.empty());
    }
  }

  // Advanced usage. This can be used in either the Graph or SccOutput template
  // class to query the current state of the algorithm. It allows to build more
  // complex variant based on the core DFS algo.
  bool NodeIsInCurrentDfsPath(NodeIndex node) const {
    return node_index_[node] > 0 && node_index_[node] < kSettledIndex;
  }

 private:
  static constexpr NodeIndex kSettledIndex =
      std::numeric_limits<NodeIndex>::max();

  // Each node expanded by the DFS will be pushed on this stack. A node is only
  // popped back when its strongly connected component has been explored and
  // outputted.
  std::vector<NodeIndex> scc_stack_;

  // This is equivalent to the "low link" of a node in Tarjan's algorithm.
  // Basically, scc_start_index_.back() represent the 1-based index in
  // scc_stack_ of the beginning of the current strongly connected component.
  // All the nodes after this index will be on the same component.
  std::vector<NodeIndex> scc_start_index_;

  // Each node is assigned an index which changes 2 times in the algorithm:
  // - Everyone starts with an index of 0 which means unexplored.
  // - The first time they are explored by the DFS and pushed on scc_stack_,
  //   they get their 1-based index on this stack.
  // - Once they have been processed and outputted to components, they are said
  //   to be settled, and their index become kSettledIndex.
  std::vector<NodeIndex> node_index_;

  // This is a well known way to do an efficient iterative DFS. Each time a node
  // is explored, all its adjacent nodes are pushed on this stack. The iterative
  // dfs processes the nodes one by one by popping them back from here.
  std::vector<NodeIndex> node_to_process_;
};

// Simple wrapper function for most usage.
template <typename NodeIndex, typename Graph, typename SccOutput>
void FindStronglyConnectedComponents(const NodeIndex num_nodes,
                                     const Graph& graph,
                                     SccOutput* components) {
  StronglyConnectedComponentsFinder<NodeIndex, Graph, SccOutput> helper;
  return helper.FindStronglyConnectedComponents(num_nodes, graph, components);
}

#endif  // UTIL_GRAPH_STRONGLY_CONNECTED_COMPONENTS_H_
