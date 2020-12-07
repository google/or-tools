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

// TopologicalSorter provides topologically sorted traversal of the
// nodes of a directed acyclic graph (DAG) with up to INT_MAX nodes.
// It sorts ancestor nodes before their descendants.
//
// If your graph is not a DAG and you're reading this, you are probably
// looking for ortools/graph/strongly_connected_components.h which does
// the topological decomposition of a directed graph.
//
// EXAMPLE:
//
//   vector<int> result;
//   vector<string> nodes = {"a", "b", "c"};
//   vector<pair<string, string>> arcs = {{"a", "c"}, {"a", "b"}, {"b", "c"}};
//   if (util::StableTopologicalSort(num_nodes, arcs, &result)) {
//     LOG(INFO) << "The topological order is: " << gtl::LogContainer(result);
//   } else {
//     LOG(INFO) << "The graph is cyclic.";
//     // Note: you can extract a cycle with the TopologicalSorter class, or
//     // with the API defined in circularity_detector.h.
//   }
//   // This will be successful and result will be equal to {"a", "b", "c"}.
//
// There are 8 flavors of topological sort, from these 3 bits:
// - There are OrDie() versions that directly return the topological order, or
//   crash if a cycle is detected (and LOG the cycle).
// - There are type-generic versions that can take any node type (including
//   non-dense integers), but slower, or the "dense int" versions which requires
//   nodes to be a dense interval [0..num_nodes-1]. Note that the type must
//   be compatible with LOG << T if you're using the OrDie() version.
// - The sorting can be either stable or not. "Stable" essentially means that it
//   will preserve the order of nodes, if possible. More precisely, the returned
//   topological order will be the lexicographically minimal valid order, where
//   "lexicographic" applies to the indices of the nodes.
//
//   TopologicalSort()
//   TopologicalSortOrDie()
//   StableTopologicalSort()
//   StableTopologicalSortOrDie()
//   DenseIntTopologicalSort()
//   DenseIntTopologicalSortOrDie()
//   DenseIntStableTopologicalSort()
//   DenseIntStableTopologicalSortOrDie()
//
// If you need more control, or a step-by-step topological sort, see the
// TopologicalSorter classes below.

#ifndef UTIL_GRAPH_TOPOLOGICALSORTER_H__
#define UTIL_GRAPH_TOPOLOGICALSORTER_H__

#include <queue>
#include <type_traits>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "ortools/base/container_logging.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"

namespace util {

// Returns true if the graph was a DAG, and outputs the topological order in
// "topological_order". Returns false if the graph is cyclic.
// Works in O(num_nodes + arcs.size()), and is pretty fast.
inline ABSL_MUST_USE_RESULT bool DenseIntTopologicalSort(
    int num_nodes, const std::vector<std::pair<int, int>>& arcs,
    std::vector<int>* topological_order);

// Like DenseIntTopologicalSort, but stable.
inline ABSL_MUST_USE_RESULT bool DenseIntStableTopologicalSort(
    int num_nodes, const std::vector<std::pair<int, int>>& arcs,
    std::vector<int>* topological_order);

// Finds a cycle in the directed graph given as argument: nodes are dense
// integers in 0..num_nodes-1, and (directed) arcs are pairs of nodes
// {from, to}.
// The returned cycle is a list of nodes that form a cycle, eg. {1, 4, 3}
// if the cycle 1->4->3->1 exists.
// If the graph is acyclic, returns an empty vector.
ABSL_MUST_USE_RESULT std::vector<int> FindCycleInDenseIntGraph(
    int num_nodes, const std::vector<std::pair<int, int>>& arcs);

// Like the two above, but with generic node types. The nodes must be provided.
// Can be significantly slower, but still linear.
template <typename T>
ABSL_MUST_USE_RESULT bool TopologicalSort(
    const std::vector<T>& nodes, const std::vector<std::pair<T, T>>& arcs,
    std::vector<T>* topological_order);
template <typename T>
ABSL_MUST_USE_RESULT bool StableTopologicalSort(
    const std::vector<T>& nodes, const std::vector<std::pair<T, T>>& arcs,
    std::vector<T>* topological_order);

// "OrDie()" versions of the 4 functions above. Those directly return the
// topological order, which makes their API even simpler.
inline std::vector<int> DenseIntTopologicalSortOrDie(
    int num_nodes, const std::vector<std::pair<int, int>>& arcs);
inline std::vector<int> DenseIntStableTopologicalSortOrDie(
    int num_nodes, const std::vector<std::pair<int, int>>& arcs);
template <typename T>
std::vector<T> TopologicalSortOrDie(const std::vector<T>& nodes,
                                    const std::vector<std::pair<T, T>>& arcs);
template <typename T>
std::vector<T> StableTopologicalSortOrDie(
    const std::vector<T>& nodes, const std::vector<std::pair<T, T>>& arcs);

namespace internal {
// Internal wrapper around the *TopologicalSort classes.
template <typename T, typename Sorter>
ABSL_MUST_USE_RESULT bool RunTopologicalSorter(
    Sorter* sorter, const std::vector<std::pair<T, T>>& arcs,
    std::vector<T>* topological_order_or_cycle);

// Do not use the templated class directly, instead use one of the
// typedefs DenseIntTopologicalSorter or DenseIntStableTopologicalSorter.
//
// The equivalent of a TopologicalSorter<int> which nodes are the
// N integers from 0 to N-1 (see the toplevel comment).  The API is
// exactly similar to that of TopologicalSorter, please refer to the
// TopologicalSorter class below for more detailed comments.
//
// If the template parameter is true then the sort will be stable.
// This means that the order of the nodes will be maintained as much as
// possible.  A non-stable sort is more efficient, since the complexity
// of getting the next node is O(1) rather than O(log(Nodes)).
template <bool stable_sort = false>
class DenseIntTopologicalSorterTpl {
 public:
  // To store the adjacency lists efficiently.
  typedef std::vector<int> AdjacencyList;

  // For efficiency, it is best to specify how many nodes are required
  // by using the next constructor.
  DenseIntTopologicalSorterTpl()
      : traversal_started_(false),
        num_edges_(0),
        num_edges_added_since_last_duplicate_removal_(0) {}

  // One may also construct a DenseIntTopologicalSorterTpl with a predefined
  // number of empty nodes. One can thus bypass the AddNode() API,
  // which may yield a lower memory usage.
  explicit DenseIntTopologicalSorterTpl(int num_nodes)
      : adjacency_lists_(num_nodes),
        traversal_started_(false),
        num_edges_(0),
        num_edges_added_since_last_duplicate_removal_(0) {}

  // Performs in constant amortized time.  Calling this will make all
  // node indices in [0 .. node_index] be valid node indices.  If you
  // can avoid using AddNode(), you should!  If you know the number of
  // nodes in advance, you should specify that at construction time --
  // it will be faster and use less memory.
  void AddNode(int node_index);

  // Performs in constant amortized time. Calling this will make all
  // node indices in [0, max(from, to)] be valid node indices.
  void AddEdge(int from, int to);

  // Performs in O(average degree) in average. If a cycle is detected
  // and "output_cycle_nodes" isn't NULL, it will require an additional
  // O(number of edges + number of nodes in the graph) time.
  bool GetNext(int* next_node_index, bool* cyclic,
               std::vector<int>* output_cycle_nodes = NULL);

  int GetCurrentFringeSize() {
    StartTraversal();
    return nodes_with_zero_indegree_.size();
  }

  void StartTraversal();

  bool TraversalStarted() const { return traversal_started_; }

  // Given a vector<AdjacencyList> of size n such that elements of the
  // AdjacencyList are in [0, n-1], remove the duplicates within each
  // AdjacencyList of size greater or equal to skip_lists_smaller_than,
  // in linear time.  Returns the total number of duplicates removed.
  // This method is exposed for unit testing purposes only.
  static int RemoveDuplicates(std::vector<AdjacencyList>* lists,
                              int skip_lists_smaller_than);

  // To extract a cycle. When there is no cycle, cycle_nodes will be empty.
  void ExtractCycle(std::vector<int>* cycle_nodes) const;

 private:
  // Outgoing adjacency lists.
  std::vector<AdjacencyList> adjacency_lists_;

  bool traversal_started_;

  // Only valid after a traversal started.
  int num_nodes_left_;
  typename std::conditional<
      stable_sort,
      // We use greater<int> so that the lowest elements gets popped first.
      std::priority_queue<int, std::vector<int>, std::greater<int>>,
      std::queue<int>>::type nodes_with_zero_indegree_;
  std::vector<int> indegree_;

  // Used internally by AddEdge() to decide whether to trigger
  // RemoveDuplicates(). See the .cc.
  int num_edges_;  // current total number of edges.
  int num_edges_added_since_last_duplicate_removal_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DenseIntTopologicalSorterTpl);
};

extern template class DenseIntTopologicalSorterTpl<false>;
extern template class DenseIntTopologicalSorterTpl<true>;

}  // namespace internal

// Recommended version for general usage. The stability makes it more
// deterministic, and its behavior is guaranteed to never change.
typedef ::util::internal::DenseIntTopologicalSorterTpl<
    /*stable_sort=*/true>
    DenseIntStableTopologicalSorter;

// Use this version if you are certain you don't care about the
// tie-breaking order and need the 5 to 10% performance gain.  The
// performance gain can be more significant for large graphs with large
// numbers of source nodes (for example 2 Million nodes with 2 Million
// random edges sees a factor of 0.7 difference in completion time).
typedef ::util::internal::DenseIntTopologicalSorterTpl<
    /*stable_sort=*/false>
    DenseIntTopologicalSorter;

// A copy of each Node is stored internally. Duplicated edges are allowed,
// and discarded lazily so that AddEdge() keeps an amortized constant
// time, yet the total memory usage remains O(number of different edges +
// number of nodes).
//
// DenseIntTopologicalSorter implements the core topological sort
// algorithm.  For greater efficiency it can be used directly
// (TopologicalSorter<int> is about 1.5-3x slower).
//
// TopologicalSorter requires that all nodes and edges be added before
// traversing the nodes, otherwise it will die with a fatal error.
//
//
// Note(user): since all the real work is done by
// DenseIntTopologicalSorterTpl, and this class is a template, we inline
// every function here in the .h.
//
// If stable_sort is true then the topological sort will preserve the
// original order of the nodes as much as possible.  Note, the order
// which is preserved is the order in which the nodes are added (if you
// use AddEdge it will add the first argument and then the second).
template <typename T, bool stable_sort = false,
          typename Hash = typename absl::flat_hash_map<T, int>::hasher,
          typename KeyEqual =
              typename absl::flat_hash_map<T, int, Hash>::key_equal>
class TopologicalSorter {
 public:
  TopologicalSorter() {}
  ~TopologicalSorter() {}

  // Adds a node to the graph, if it has not already been added via
  // previous calls to AddNode()/AddEdge(). If no edges are later
  // added connecting this node, then it remains an isolated node in
  // the graph. AddNode() only exists to support isolated nodes. There
  // is no requirement (nor is it an error) to call AddNode() for the
  // endpoints used in a call to AddEdge().  Dies with a fatal error if
  // called after a traversal has been started (see TraversalStarted()),
  // or if more than INT_MAX nodes are being added.
  void AddNode(const T& node) { int_sorter_.AddNode(LookupOrInsertNode(node)); }

  // Adds a directed edge with the given endpoints to the graph. There
  // is no requirement (nor is it an error) to call AddNode() for the
  // endpoints.  Dies with a fatal error if called after a traversal
  // has been started (see TraversalStarted()).
  void AddEdge(const T& from, const T& to) {
    // The lookups are not inlined into AddEdge because we need to ensure that
    // "from" is inserted before "to".
    const int from_int = LookupOrInsertNode(from);
    const int to_int = LookupOrInsertNode(to);
    int_sorter_.AddEdge(from_int, to_int);
  }

  // Visits the least node in topological order over the current set of
  // nodes and edges, and marks that node as visited, so that repeated
  // calls to GetNext() will visit all nodes in order.  Writes the newly
  // visited node in *node and returns true with *cyclic set to false
  // (assuming the graph has not yet been discovered to be cyclic).
  // Returns false if all nodes have been visited, or if the graph is
  // discovered to be cyclic, in which case *cyclic is also set to true.
  //
  // If you set the optional argument "output_cycle_nodes" to non-NULL and
  // a cycle is detected, it will dump an arbitrary cycle of the graph
  // (whose length will be between 1 and #number_of_nodes, inclusive),
  // in the natural order: for example if "output_cycle_nodes" is filled
  // with ["A", "C", "B"], it means that A->C->B->A is a directed cycle
  // of the graph.
  //
  // This starts a traversal (if not started already). Note that the
  // graph can only be traversed once.
  bool GetNext(T* node, bool* cyclic_ptr,
               std::vector<T>* output_cycle_nodes = NULL) {
    StartTraversal();
    int node_index;
    if (!int_sorter_.GetNext(&node_index, cyclic_ptr,
                             output_cycle_nodes ? &cycle_int_nodes_ : NULL)) {
      if (*cyclic_ptr && output_cycle_nodes != NULL) {
        output_cycle_nodes->clear();
        for (const int int_node : cycle_int_nodes_) {
          output_cycle_nodes->push_back(nodes_[int_node]);
        }
      }
      return false;
    }
    *node = nodes_[node_index];
    return true;
  }

  // Returns the number of nodes that currently have zero indegree.
  // This starts a traversal (if not started already).
  int GetCurrentFringeSize() {
    StartTraversal();
    return int_sorter_.GetCurrentFringeSize();
  }

  // Start a traversal. See TraversalStarted().  This initializes the
  // various data structures of the sorter.  Since this takes O(num_nodes
  // + num_edges) time, users may want to call this at their convenience,
  // instead of making it happen with the first GetNext().
  void StartTraversal() {
    if (TraversalStarted()) return;
    nodes_.resize(node_to_index_.size());
    // We move elements from the absl::flat_hash_map to this vector, without
    // extra copy (if they are movable).
    for (auto& node_and_index : node_to_index_) {
      nodes_[node_and_index.second] = std::move(node_and_index.first);
    }
    gtl::STLClearHashIfBig(&node_to_index_, 1 << 16);
    int_sorter_.StartTraversal();
  }

  // Whether a traversal has started. If true, AddNode() and AddEdge()
  // can no longer be called.
  bool TraversalStarted() const { return int_sorter_.TraversalStarted(); }

 private:
  // A simple mapping from node to their dense index, in 0..num_nodes-1,
  // which will be their index in nodes_[].  Cleared when a traversal
  // starts, and replaced by nodes_[].
  absl::flat_hash_map<T, int, Hash, KeyEqual> node_to_index_;

  // Stores all the nodes as soon as a traversal starts.
  std::vector<T> nodes_;

  // An internal DenseIntTopologicalSorterTpl that does all the real work.
  ::util::internal::DenseIntTopologicalSorterTpl<stable_sort> int_sorter_;

  // Used internally to extract cycles from the underlying
  // DenseIntTopologicalSorterTpl.
  std::vector<int> cycle_int_nodes_;

  // Lookup an existing node's index, or add the node and return the
  // new index that was assigned to it.
  int LookupOrInsertNode(const T& node) {
    return gtl::LookupOrInsert(&node_to_index_, node, node_to_index_.size());
  }

  DISALLOW_COPY_AND_ASSIGN(TopologicalSorter);
};

namespace internal {
// If successful, returns true and outputs the order in "topological_order".
// If not, returns false and outputs a cycle in "cycle" (if not null).
template <typename T, typename Sorter>
ABSL_MUST_USE_RESULT bool RunTopologicalSorter(
    Sorter* sorter, const std::vector<std::pair<T, T>>& arcs,
    std::vector<T>* topological_order, std::vector<T>* cycle) {
  topological_order->clear();
  for (const auto& arc : arcs) {
    sorter->AddEdge(arc.first, arc.second);
  }
  bool cyclic = false;
  sorter->StartTraversal();
  T next;
  while (sorter->GetNext(&next, &cyclic, cycle)) {
    topological_order->push_back(next);
  }
  return !cyclic;
}

template <bool stable_sort = false>
ABSL_MUST_USE_RESULT bool DenseIntTopologicalSortImpl(
    int num_nodes, const std::vector<std::pair<int, int>>& arcs,
    std::vector<int>* topological_order) {
  DenseIntTopologicalSorterTpl<stable_sort> sorter(num_nodes);
  return RunTopologicalSorter<int, decltype(sorter)>(
      &sorter, arcs, topological_order, nullptr);
}

template <typename T, bool stable_sort = false>
ABSL_MUST_USE_RESULT bool TopologicalSortImpl(
    const std::vector<T>& nodes, const std::vector<std::pair<T, T>>& arcs,
    std::vector<T>* topological_order) {
  TopologicalSorter<T, stable_sort> sorter;
  for (const T& node : nodes) {
    sorter.AddNode(node);
  }
  return RunTopologicalSorter<T, decltype(sorter)>(&sorter, arcs,
                                                   topological_order, nullptr);
}

// Now, the OrDie() versions, which directly return the topological order.
template <typename T, typename Sorter>
std::vector<T> RunTopologicalSorterOrDie(
    Sorter* sorter, const std::vector<std::pair<T, T>>& arcs) {
  std::vector<T> topo_order;
  CHECK(RunTopologicalSorter(sorter, arcs, &topo_order, &topo_order))
      << "Found cycle: " << gtl::LogContainer(topo_order);
  return topo_order;
}

template <bool stable_sort = false>
std::vector<int> DenseIntTopologicalSortOrDieImpl(
    int num_nodes, const std::vector<std::pair<int, int>>& arcs) {
  DenseIntTopologicalSorterTpl<stable_sort> sorter(num_nodes);
  return RunTopologicalSorterOrDie(&sorter, arcs);
}

template <typename T, bool stable_sort = false>
std::vector<T> TopologicalSortOrDieImpl(
    const std::vector<T>& nodes, const std::vector<std::pair<T, T>>& arcs) {
  TopologicalSorter<T, stable_sort> sorter;
  for (const T& node : nodes) {
    sorter.AddNode(node);
  }
  return RunTopologicalSorterOrDie(&sorter, arcs);
}
}  // namespace internal

// Implementations of the "simple API" functions declared at the top.
inline bool DenseIntTopologicalSort(
    int num_nodes, const std::vector<std::pair<int, int>>& arcs,
    std::vector<int>* topological_order) {
  return internal::DenseIntTopologicalSortImpl<false>(num_nodes, arcs,
                                                      topological_order);
}

inline bool DenseIntStableTopologicalSort(
    int num_nodes, const std::vector<std::pair<int, int>>& arcs,
    std::vector<int>* topological_order) {
  return internal::DenseIntTopologicalSortImpl<true>(num_nodes, arcs,
                                                     topological_order);
}

template <typename T>
bool TopologicalSort(const std::vector<T>& nodes,
                     const std::vector<std::pair<T, T>>& arcs,
                     std::vector<T>* topological_order) {
  return internal::TopologicalSortImpl<T, false>(nodes, arcs,
                                                 topological_order);
}

template <typename T>
bool StableTopologicalSort(const std::vector<T>& nodes,
                           const std::vector<std::pair<T, T>>& arcs,
                           std::vector<T>* topological_order) {
  return internal::TopologicalSortImpl<T, true>(nodes, arcs, topological_order);
}

inline std::vector<int> DenseIntTopologicalSortOrDie(
    int num_nodes, const std::vector<std::pair<int, int>>& arcs) {
  return internal::DenseIntTopologicalSortOrDieImpl<false>(num_nodes, arcs);
}

inline std::vector<int> DenseIntStableTopologicalSortOrDie(
    int num_nodes, const std::vector<std::pair<int, int>>& arcs) {
  return internal::DenseIntTopologicalSortOrDieImpl<true>(num_nodes, arcs);
}

template <typename T>
std::vector<T> TopologicalSortOrDie(const std::vector<T>& nodes,
                                    const std::vector<std::pair<T, T>>& arcs) {
  return internal::TopologicalSortOrDieImpl<T, false>(nodes, arcs);
}

template <typename T>
std::vector<T> StableTopologicalSortOrDie(
    const std::vector<T>& nodes, const std::vector<std::pair<T, T>>& arcs) {
  return internal::TopologicalSortOrDieImpl<T, true>(nodes, arcs);
}

}  // namespace util

// BACKWARDS COMPATIBILITY
// Some of the classes or functions have been exposed under the global namespace
// or the util::graph:: namespace. Until all clients are fixed to use the
// util:: namespace, we keep those versions around.
typedef ::util::DenseIntStableTopologicalSorter DenseIntStableTopologicalSorter;
typedef ::util::DenseIntTopologicalSorter DenseIntTopologicalSorter;
template <typename T, bool stable_sort = false,
          typename Hash = typename absl::flat_hash_map<T, int>::hasher,
          typename KeyEqual =
              typename absl::flat_hash_map<T, int, Hash>::key_equal>
class TopologicalSorter
    : public ::util::TopologicalSorter<T, stable_sort, Hash, KeyEqual> {};

namespace util {
namespace graph {
inline std::vector<int> DenseIntTopologicalSortOrDie(
    int num_nodes, const std::vector<std::pair<int, int>>& arcs) {
  return ::util::DenseIntTopologicalSortOrDie(num_nodes, arcs);
}
inline std::vector<int> DenseIntStableTopologicalSortOrDie(
    int num_nodes, const std::vector<std::pair<int, int>>& arcs) {
  return ::util::DenseIntStableTopologicalSortOrDie(num_nodes, arcs);
}
template <typename T>
std::vector<T> StableTopologicalSortOrDie(
    const std::vector<T>& nodes, const std::vector<std::pair<T, T>>& arcs) {
  return ::util::StableTopologicalSortOrDie<T>(nodes, arcs);
}

}  // namespace graph
}  // namespace util

#endif  // UTIL_GRAPH_TOPOLOGICALSORTER_H__
