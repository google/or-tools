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

//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Finds the connected components in an undirected graph:
// https://en.wikipedia.org/wiki/Connected_component_(graph_theory)
//
// If you have a fixed graph where the node are dense integers, use
// GetConnectedComponents(): it's very fast and uses little memory.
//
// If you have a more dynamic scenario where you want to incrementally
// add nodes or edges and query the connectivity between them, use the
// [Dense]ConnectedComponentsFinder class, which uses the union-find algorithm
// aka disjoint sets: https://en.wikipedia.org/wiki/Disjoint-set_data_structure.

#ifndef UTIL_GRAPH_CONNECTED_COMPONENTS_H_
#define UTIL_GRAPH_CONNECTED_COMPONENTS_H_

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <type_traits>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/meta/type_traits.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/ptr_util.h"

namespace util {
// Finds the connected components of the graph, using BFS internally.
// Works on any *undirected* graph class whose nodes are dense integers and that
// supports the [] operator for adjacency lists: graph[x] must be an integer
// container listing the nodes that are adjacent to node #x.
// Example: std::vector<std::vector<int>>.
//
// "Undirected" means that for all y in graph[x], x is in graph[y].
//
// Returns the mapping from node to component index. The component indices are
// deterministic: Component #0 will be the one that has node #0, component #1
// the one that has the lowest-index node that isn't in component #0, and so on.
//
// Example on the following 6-node graph: 5--3--0--1  2--4
// vector<vector<int>> graph = {{1, 3}, {0}, {4}, {0, 5}, {2}, {3}};
// GetConnectedComponents(graph);  // returns [0, 0, 1, 0, 1, 0].
template <class UndirectedGraph>
std::vector<int> GetConnectedComponents(int num_nodes,
                                        const UndirectedGraph& graph);
}  // namespace util

// NOTE(user): The rest of the functions below should also be in namespace
// util, but for historical reasons it hasn't been done yet.

// A connected components finder that only works on dense ints.
class DenseConnectedComponentsFinder {
 public:
  DenseConnectedComponentsFinder() {}

  DenseConnectedComponentsFinder(const DenseConnectedComponentsFinder&) =
      delete;
  DenseConnectedComponentsFinder& operator=(
      const DenseConnectedComponentsFinder&) = delete;

  // The main API is the same as ConnectedComponentsFinder (below): see the
  // homonymous functions there.
  void AddEdge(int node1, int node2);
  bool Connected(int node1, int node2);
  int GetSize(int node);
  int GetNumberOfComponents() const { return num_components_; }
  int GetNumberOfNodes() const { return parent_.size(); }

  // Gets the current set of root nodes in sorted order. Runs in amortized
  // O(#components) time.
  const std::vector<int>& GetComponentRoots();

  // Sets the number of nodes in the graph. The graph can only grow: this
  // dies if "num_nodes" is lower or equal to any of the values ever given
  // to AddEdge(), or lower than a previous value given to SetNumberOfNodes().
  // You need this if there are nodes that don't have any edges.
  void SetNumberOfNodes(int num_nodes);

  // Returns the root of the set for the given node. node must be in
  // [0;GetNumberOfNodes()-1].
  // Non-const because it does path compression internally.
  int FindRoot(int node);

  // Returns the same as GetConnectedComponents().
  std::vector<int> GetComponentIds();

 private:
  // parent[i] is the id of an ancestor for node i. A node is a root iff
  // parent[i] == i.
  std::vector<int> parent_;
  // If i is a root, component_size_[i] is the number of elements in the
  // component. If i is not a root, component_size_[i] is meaningless.
  std::vector<int> component_size_;
  // rank[i] is the depth of the tree.
  std::vector<int> rank_;
  // Number of connected components.
  int num_components_ = 0;
  // The current roots.  This is maintained lazily by GetComponentRoots().
  std::vector<int> roots_;
  // The number of nodes that existed the last time GetComponentRoots() was
  // called.
  int num_nodes_at_last_get_roots_call_ = 0;
};

namespace internal {
// A helper to deduce the type of map to use depending on whether CompareOrHashT
// is a comparator or a hasher (prefer the latter).
template <typename T, typename CompareOrHashT>
struct ConnectedComponentsTypeHelper {
  // SFINAE trait to detect hash functors and select unordered containers if so,
  // and ordered containers otherwise (= by default).
  template <typename U, typename E = void>
  struct SelectContainer {
    using Set = std::set<T, CompareOrHashT>;
    using Map = std::map<T, int, CompareOrHashT>;
  };

  // The expression inside decltype is basically saying that "H(x)" is
  // well-formed, where H is an instance of U and x is an instance of T, and is
  // a value of integral type. That is, we are "duck-typing" on whether U looks
  // like a hash functor.
  template <typename U>
  struct SelectContainer<
      U, absl::enable_if_t<std::is_integral<decltype(std::declval<const U&>()(
             std::declval<const T&>()))>::value>> {
    using Set = absl::flat_hash_set<T, CompareOrHashT>;
    using Map = absl::flat_hash_map<T, int, CompareOrHashT>;
  };

  using Set = typename SelectContainer<CompareOrHashT>::Set;
  using Map = typename SelectContainer<CompareOrHashT>::Map;
};

}  // namespace internal

// Usage:
//   ConnectedComponentsFinder<MyNodeType> cc;
//   cc.AddNode(node1);
//   cc.AddNode(node2);
//   cc.AddEdge(node1, node2);
// ... repeating, adding nodes and edges as needed.  Adding an edge
// will automatically also add the two nodes at its ends, if they
// haven't already been added.
//   vector<set<MyNodeType> > components;
//   cc.FindConnectedComponents(&components);
// Each entry in components now contains all the nodes in a single
// connected component.
//
// Usage with flat_hash_set:
//   using ConnectedComponentType = flat_hash_set<MyNodeType>;
//   ConnectedComponentsFinder<ConnectedComponentType::key_type,
//                             ConnectedComponentType::hasher>
//   cc;
//   ...
//   vector<ConnectedComponentType> components;
//   cc.FindConnectedComponents(&components);
//
// If you want to, you can continue adding nodes and edges after calling
// FindConnectedComponents, then call it again later.
//
// If your node type isn't STL-friendly, then you can use pointers to
// it instead:
//   ConnectedComponentsFinder<MySTLUnfriendlyNodeType*> cc;
//   cc.AddNode(&node1);
// ... and so on...
// Of course, in this usage, the connected components finder retains
// these pointers through its lifetime (though it doesn't dereference them).
template <typename T, typename CompareOrHashT = std::less<T>>
class ConnectedComponentsFinder {
 public:
  // Constructs a connected components finder.
  ConnectedComponentsFinder() {}

  ConnectedComponentsFinder(const ConnectedComponentsFinder&) = delete;
  ConnectedComponentsFinder& operator=(const ConnectedComponentsFinder&) =
      delete;

  // Adds a node in the graph.  It is OK to add the same node more than
  // once; additions after the first have no effect.
  void AddNode(T node) { LookupOrInsertNode<true>(node); }

  // Adds an edge in the graph.  Also adds both endpoint nodes as necessary.
  // It is not an error to add the same edge twice.  Self-edges are OK too.
  void AddEdge(T node1, T node2) {
    delegate_.AddEdge(LookupOrInsertNode<false>(node1),
                      LookupOrInsertNode<false>(node2));
  }

  // Returns true iff both nodes are in the same connected component.
  // Returns false if either node has not been already added with AddNode.
  bool Connected(T node1, T node2) {
    return delegate_.Connected(gtl::FindWithDefault(index_, node1, -1),
                               gtl::FindWithDefault(index_, node2, -1));
  }

  // Finds the connected component containing a node, and returns the
  // total number of nodes in that component.  Returns zero iff the
  // node has not been already added with AddNode.
  int GetSize(T node) {
    return delegate_.GetSize(gtl::FindWithDefault(index_, node, -1));
  }

  // Finds all the connected components and assigns them to components.
  // Components are ordered in the same way nodes were added, i.e. if node 'b'
  // was added before node 'c', then either:
  //  - 'c' belongs to the same component as a node 'a' added before 'b', or
  //  - the component for 'c' comes after the one for 'b'.
  // There are two versions:
  //  - The first one returns the result, and stores each component in a vector.
  //    This is the preferred version.
  //  - The second one populates the result, and stores each component in a set.
  std::vector<std::vector<T>> FindConnectedComponents() {
    const auto component_ids = delegate_.GetComponentIds();
    std::vector<std::vector<T>> components(delegate_.GetNumberOfComponents());
    for (const auto& elem_id : index_) {
      components[component_ids[elem_id.second]].push_back(elem_id.first);
    }
    return components;
  }
  void FindConnectedComponents(
      std::vector<typename internal::ConnectedComponentsTypeHelper<
          T, CompareOrHashT>::Set>* components) {
    const auto component_ids = delegate_.GetComponentIds();
    components->clear();
    components->resize(delegate_.GetNumberOfComponents());
    for (const auto& elem_id : index_) {
      components->at(component_ids[elem_id.second]).insert(elem_id.first);
    }
  }

  // Returns the current number of connected components.
  // This number can change as the new nodes or edges are added.
  int GetNumberOfComponents() const {
    return delegate_.GetNumberOfComponents();
  }

  // Returns the current number of added distinct nodes.
  // This includes nodes added explicitly via the calls to AddNode() method
  // and implicitly via the calls to AddEdge() method.
  // Nodes that were added several times only count once.
  int GetNumberOfNodes() const { return delegate_.GetNumberOfNodes(); }

 private:
  // Returns the index for the given node. If the node does not exist and
  // update_delegate is true, explicitly add the node to the delegate.
  template <bool update_delegate>
  int LookupOrInsertNode(T node) {
    const auto result = index_.emplace(node, index_.size());
    const int node_id = result.first->second;
    if (update_delegate && result.second) {
      // A new index was created.
      delegate_.SetNumberOfNodes(node_id + 1);
    }
    return node_id;
  }

  DenseConnectedComponentsFinder delegate_;
  typename internal::ConnectedComponentsTypeHelper<T, CompareOrHashT>::Map
      index_;
};

// =============================================================================
// Implementations of the method templates
// =============================================================================
namespace util {
template <class UndirectedGraph>
std::vector<int> GetConnectedComponents(int num_nodes,
                                        const UndirectedGraph& graph) {
  std::vector<int> component_of_node(num_nodes, -1);
  std::vector<int> bfs_queue;
  int num_components = 0;
  for (int src = 0; src < num_nodes; ++src) {
    if (component_of_node[src] >= 0) continue;
    bfs_queue.push_back(src);
    component_of_node[src] = num_components;
    for (int num_visited = 0; num_visited < bfs_queue.size(); ++num_visited) {
      const int node = bfs_queue[num_visited];
      for (const int neighbor : graph[node]) {
        if (component_of_node[neighbor] >= 0) continue;
        component_of_node[neighbor] = num_components;
        bfs_queue.push_back(neighbor);
      }
    }
    ++num_components;
    bfs_queue.clear();
  }
  return component_of_node;
}
}  // namespace util

#endif  // UTIL_GRAPH_CONNECTED_COMPONENTS_H_
