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

// Utility for finding connected components in an undirected graph.
// A set of nodes and edges between the nodes form an undirected graph.
// If two nodes can be reached from one another, then they are in the
// same connected component.
//
// To use this:
//   ConnectedComponentsFinder<MyNodeType> cc;
//   cc.AddNode(node1);
//   cc.AddNode(node2);
//   cc.AddEdge(node1, node2);
// ... repeating, adding nodes and edges as needed.  Adding an edge
// will automatically also add the two nodes at its ends, if they
// haven't already been added.
//   std::vector<std::set<MyNodeType> > components;
//   cc.FindConnectedComponents(&components);
// Each entry in components now contains all the nodes in a single
// connected component.
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

#ifndef UTIL_GRAPH_CONNECTED_COMPONENTS_H_
#define UTIL_GRAPH_CONNECTED_COMPONENTS_H_

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/ptr_util.h"

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

  // Sets the number of nodes in the graph. The graph can only grow: this
  // dies if "num_nodes" is lower or equal to any of the values ever given
  // to AddEdge(), or lower than a previous value given to SetNumberOfNodes().
  // You need this if there are nodes that don't have any edges.
  void SetNumberOfNodes(int num_nodes);

  // Returns the root of the set for the given node. node must be in
  // [0;GetNumberOfNodes()-1].
  // Non-const because it does path compression internally.
  int FindRoot(int node);

  // Returns a vector of size GetNumberOfNodes() with the "component id" each
  // node. Two nodes are in the same component iff their component id is equal,
  // and components are numbered 0 to (GetNumberOfComponents() - 1).
  // The order is deterministic: for two nodes b and c, b < c =>
  //     component_id(b) <= component_id(c)
  //   or
  //     there exists a < b, component_id(a) = component_id(c)
  // Non-const because it does path compression internally.
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
};

namespace internal {
// A helper to deduce the type of map to use depending on whether CompareOrHashT
// is a comparator or a hasher (prefer the latter).
template <typename T, typename CompareOrHashT>
struct ConnectedComponentsTypeHelper {
  // SFINAE helpers to detect a hash functor.
  template <typename U, size_t (U::*)(const T&) const>
  struct hash_by_ref {};
  template <typename U, size_t (U::*)(T) const>
  struct hash_by_value {};

  // SFINAE dispatchers that return the right kind of set depending on the
  // functor.
  template <typename U>
  static std::unordered_set<T, CompareOrHashT> ReturnSet(
      hash_by_ref<U, &U::operator()>*);
  template <typename U>
  static std::unordered_set<T, CompareOrHashT> ReturnSet(
      hash_by_value<U, &U::operator()>*);
  template <typename U>
  static std::set<T, CompareOrHashT> ReturnSet(...);
  using Set = decltype(ReturnSet<CompareOrHashT>(nullptr));

  // SFINAE dispatchers that return the right kind of map depending on the
  // functor.
  template <typename U>
  static std::unordered_map<T, int, CompareOrHashT> ReturnMap(
      hash_by_ref<U, &U::operator()>*);
  template <typename U>
  static std::unordered_map<T, int, CompareOrHashT> ReturnMap(
      hash_by_value<U, &U::operator()>*);
  template <typename U>
  static std::map<T, int, CompareOrHashT> ReturnMap(...);
  using Map = decltype(ReturnMap<CompareOrHashT>(nullptr));
};

}  // namespace internal

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

#endif  // UTIL_GRAPH_CONNECTED_COMPONENTS_H_
