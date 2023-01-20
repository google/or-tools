// Copyright 2010-2022 Google LLC
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

// This file provides topologically sorted traversal of the nodes of a directed
// acyclic graph (DAG) with up to INT_MAX nodes.
// It sorts ancestor nodes before their descendants. Multi-arcs are fine.
//
// If your graph is not a DAG and you're reading this, you are probably
// looking for ortools/graph/strongly_connected_components.h which does
// the topological decomposition of a directed graph.
//
// USAGE:
// - If performance matters, use FastTopologicalSort().
// - If your nodes are non-integers, or you need to break topological ties by
//   node index (like "stable_sort"), use one of the DenseIntTopologicalSort()
//   or TopologicalSort variants (see below).
// - If you need more control (cycle extraction?), or a step-by-step topological
//   sort, see the TopologicalSorter classes below.

#ifndef UTIL_GRAPH_TOPOLOGICALSORTER_H__
#define UTIL_GRAPH_TOPOLOGICALSORTER_H__

#include <functional>
#include <limits>
#include <queue>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/inlined_vector.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "ortools/base/container_logging.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/map_util.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/stl_util.h"
#include "ortools/graph/graph.h"

namespace util {
namespace graph {

// This is the recommended API when performance matters. It's also very simple.
// AdjacencyList is any type that lets you iterate over the neighbors of
// node with the [] operator, for example vector<vector<int>> or util::Graph.
//
// If you don't already have an adjacency list representation, build one using
// StaticGraph<> in ./graph.h: FastTopologicalSort() can take any such graph as
// input.
//
// ERRORS: returns InvalidArgumentError if the input is broken (negative or
// out-of-bounds integers) or if the graph is cyclic. In the latter case, the
// error message will contain "cycle". Note that if cycles may occur in your
// input, you can probably assume that your input isn't broken, and thus rely
// on failures to detect that the graph is cyclic.
//
// TIE BREAKING: the returned topological order is deterministic and fixed, and
// corresponds to iterating on nodes in a LIFO (Breadth-first) order.
//
// Benchmark: gpaste/6147236302946304, 4-10x faster than util_graph::TopoSort().
//
// EXAMPLES:
//   std::vector<std::vector<int>> adj = {{..}, {..}, ..};
//   ASSIGN_OR_RETURN(std::vector<int> topo_order, FastTopologicalSort(adj));
//
// or
//   std::vector<pair<int, int>> arcs = {{.., ..}, ..., };
//   ASSIGN_OR_RETURN(
//       std::vector<int> topo_order,
//       FastTopologicalSort(util::StaticGraph<>::FromArcs(num_nodes, arcs)));
//
template <class AdjacencyLists>  // vector<vector<int>>, util::StaticGraph<>, ..
absl::StatusOr<std::vector<int>> FastTopologicalSort(const AdjacencyLists& adj);

// Finds a cycle in the directed graph given as argument: nodes are dense
// integers in 0..num_nodes-1, and (directed) arcs are pairs of nodes
// {from, to}.
// The returned cycle is a list of nodes that form a cycle, eg. {1, 4, 3}
// if the cycle 1->4->3->1 exists.
// If the graph is acyclic, returns an empty vector.
template <class AdjacencyLists>  // vector<vector<int>>, util::StaticGraph<>, ..
absl::StatusOr<std::vector<int>> FindCycleInGraph(const AdjacencyLists& adj);

}  // namespace graph

// [Stable]TopologicalSort[OrDie]:
//
// These variants are much slower than FastTopologicalSort(), but support
// non-integer (or integer, but sparse) nodes.
// Note that if performance matters, you're probably better off building your
// own mapping from node to dense index with a flat_hash_map and calling
// FastTopologicalSort().

// Returns true if the graph was a DAG, and outputs the topological order in
// "topological_order". Returns false if the graph is cyclic, and outputs the
// detected cycle in "cycle".
template <typename T>
ABSL_MUST_USE_RESULT bool TopologicalSort(
    const std::vector<T>& nodes, const std::vector<std::pair<T, T>>& arcs,
    std::vector<T>* topological_order);
// Override of the above that outputs the detected cycle.
template <typename T>
ABSL_MUST_USE_RESULT bool TopologicalSort(
    const std::vector<T>& nodes, const std::vector<std::pair<T, T>>& arcs,
    std::vector<T>* topological_order, std::vector<T>* cycle);
// OrDie() variant of the above.
template <typename T>
std::vector<T> TopologicalSortOrDie(const std::vector<T>& nodes,
                                    const std::vector<std::pair<T, T>>& arcs);
// The "Stable" variants are a little slower but preserve the input order of
// nodes, if possible. More precisely, the returned topological order will be
// the lexicographically minimal valid order, where "lexicographic" applies to
// the indices of the nodes.
template <typename T>
ABSL_MUST_USE_RESULT bool StableTopologicalSort(
    const std::vector<T>& nodes, const std::vector<std::pair<T, T>>& arcs,
    std::vector<T>* topological_order);
// Override of the above that outputs the detected cycle.
template <typename T>
ABSL_MUST_USE_RESULT bool StableTopologicalSort(
    const std::vector<T>& nodes, const std::vector<std::pair<T, T>>& arcs,
    std::vector<T>* topological_order, std::vector<T>* cycle);
// OrDie() variant of the above.
template <typename T>
std::vector<T> StableTopologicalSortOrDie(
    const std::vector<T>& nodes, const std::vector<std::pair<T, T>>& arcs);

// ______________________ END OF THE RECOMMENDED API ___________________________

// DEPRECATED. Use util::graph::FindCycleInGraph() directly.
inline ABSL_MUST_USE_RESULT std::vector<int> FindCycleInDenseIntGraph(
    int num_nodes, const std::vector<std::pair<int, int>>& arcs) {
  return util::graph::FindCycleInGraph(
             util::StaticGraph<>::FromArcs(num_nodes, arcs))
      .value();
}

// DEPRECATED: DenseInt[Stable]TopologicalSort[OrDie].
// Kept here for legacy reasons, but most new users should use
// FastTopologicalSort():
// - If your input is a list of edges, build you own StaticGraph<> (see
//   ./graph.h) and pass it to FastTopologicalSort().
// - If you need the "stable sort" bit, contact viger@ and/or or-core-team@
//   to see if they can create FastStableTopologicalSort().
ABSL_MUST_USE_RESULT inline bool DenseIntTopologicalSort(
    int num_nodes, const std::vector<std::pair<int, int>>& arcs,
    std::vector<int>* topological_order);
inline std::vector<int> DenseIntStableTopologicalSortOrDie(
    int num_nodes, const std::vector<std::pair<int, int>>& arcs);
ABSL_MUST_USE_RESULT inline bool DenseIntStableTopologicalSort(
    int num_nodes, const std::vector<std::pair<int, int>>& arcs,
    std::vector<int>* topological_order);
inline std::vector<int> DenseIntTopologicalSortOrDie(
    int num_nodes, const std::vector<std::pair<int, int>>& arcs);

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
  typedef absl::InlinedVector<int, 4> AdjacencyList;

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

  // Performs AddEdge() in bulk. Much faster if you add *all* edges at once.
  void AddEdges(const std::vector<std::pair<int, int>>& edges);

  // Performs in constant amortized time. Calling this will make all
  // node indices in [0, max(from, to)] be valid node indices.
  // THIS IS MUCH SLOWER than calling AddEdges() if you already have all the
  // edges.
  void AddEdge(int from, int to);

  // Performs in O(average degree) in average. If a cycle is detected
  // and "output_cycle_nodes" isn't NULL, it will require an additional
  // O(number of edges + number of nodes in the graph) time.
  bool GetNext(int* next_node_index, bool* cyclic,
               std::vector<int>* output_cycle_nodes = nullptr);

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
// TopologicalSorter is -compatible
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

  // Shortcut to AddEdge() in bulk. Not optimized.
  void AddEdges(const std::vector<std::pair<T, T>>& edges) {
    for (const auto& [from, to] : edges) AddEdge(from, to);
  }

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
               std::vector<T>* output_cycle_nodes = nullptr) {
    StartTraversal();
    int node_index;
    if (!int_sorter_.GetNext(
            &node_index, cyclic_ptr,
            output_cycle_nodes ? &cycle_int_nodes_ : nullptr)) {
      if (*cyclic_ptr && output_cycle_nodes != nullptr) {
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
  sorter->AddEdges(arcs);
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
  topological_order->reserve(num_nodes);
  return RunTopologicalSorter<int, decltype(sorter)>(
      &sorter, arcs, topological_order, nullptr);
}

template <typename T, bool stable_sort = false>
ABSL_MUST_USE_RESULT bool TopologicalSortImpl(
    const std::vector<T>& nodes, const std::vector<std::pair<T, T>>& arcs,
    std::vector<T>* topological_order, std::vector<T>* cycle) {
  TopologicalSorter<T, stable_sort> sorter;
  for (const T& node : nodes) {
    sorter.AddNode(node);
  }
  return RunTopologicalSorter<T, decltype(sorter)>(&sorter, arcs,
                                                   topological_order, cycle);
}

// Now, the OrDie() versions, which directly return the topological order.
template <typename T, typename Sorter>
std::vector<T> RunTopologicalSorterOrDie(
    Sorter* sorter, int num_nodes, const std::vector<std::pair<T, T>>& arcs) {
  std::vector<T> topo_order;
  topo_order.reserve(num_nodes);
  CHECK(RunTopologicalSorter(sorter, arcs, &topo_order, &topo_order))
      << "Found cycle: " << gtl::LogContainer(topo_order);
  return topo_order;
}

template <bool stable_sort = false>
std::vector<int> DenseIntTopologicalSortOrDieImpl(
    int num_nodes, const std::vector<std::pair<int, int>>& arcs) {
  DenseIntTopologicalSorterTpl<stable_sort> sorter(num_nodes);
  return RunTopologicalSorterOrDie(&sorter, num_nodes, arcs);
}

template <typename T, bool stable_sort = false>
std::vector<T> TopologicalSortOrDieImpl(
    const std::vector<T>& nodes, const std::vector<std::pair<T, T>>& arcs) {
  TopologicalSorter<T, stable_sort> sorter;
  for (const T& node : nodes) {
    sorter.AddNode(node);
  }
  return RunTopologicalSorterOrDie(&sorter, nodes.size(), arcs);
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
  return internal::TopologicalSortImpl<T, false>(nodes, arcs, topological_order,
                                                 nullptr);
}

template <typename T>
bool TopologicalSort(const std::vector<T>& nodes,
                     const std::vector<std::pair<T, T>>& arcs,
                     std::vector<T>* topological_order, std::vector<T>* cycle) {
  return internal::TopologicalSortImpl<T, false>(nodes, arcs, topological_order,
                                                 cycle);
}

template <typename T>
bool StableTopologicalSort(const std::vector<T>& nodes,
                           const std::vector<std::pair<T, T>>& arcs,
                           std::vector<T>* topological_order) {
  return internal::TopologicalSortImpl<T, true>(nodes, arcs, topological_order,
                                                nullptr);
}

template <typename T>
bool StableTopologicalSort(const std::vector<T>& nodes,
                           const std::vector<std::pair<T, T>>& arcs,
                           std::vector<T>* topological_order,
                           std::vector<T>* cycle) {
  return internal::TopologicalSortImpl<T, true>(nodes, arcs, topological_order,
                                                cycle);
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

template <class AdjacencyLists>
absl::StatusOr<std::vector<int>> FastTopologicalSort(
    const AdjacencyLists& adj) {
  const size_t num_nodes = adj.size();
  if (num_nodes > std::numeric_limits<int>::max()) {
    return absl::InvalidArgumentError("More than kint32max nodes");
  }
  std::vector<int> indegree(num_nodes, 0);
  std::vector<int> topo_order;
  topo_order.reserve(num_nodes);
  for (int from = 0; from < num_nodes; ++from) {
    for (const int head : adj[from]) {
      // We cast to unsigned int to test "head < 0 || head ≥ num_nodes" with a
      // single test. Microbenchmarks showed a ~1% overall performance gain.
      if (static_cast<uint32_t>(head) >= num_nodes) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Invalid arc in adj[%d]: %d (num_nodes=%d)", from,
                            head, num_nodes));
      }
      // NOTE(user): We could detect self-arcs here (head == from) and exit
      // early, but microbenchmarks show a 2 to 4% slow-down if we do it, so we
      // simply rely on self-arcs being detected as cycles in the topo sort.
      ++indegree[head];
    }
  }
  for (int i = 0; i < num_nodes; ++i) {
    if (!indegree[i]) topo_order.push_back(i);
  }
  size_t num_visited = 0;
  while (num_visited < topo_order.size()) {
    const int from = topo_order[num_visited++];
    for (const int head : adj[from]) {
      if (!--indegree[head]) topo_order.push_back(head);
    }
  }
  if (topo_order.size() < static_cast<size_t>(num_nodes)) {
    return absl::InvalidArgumentError("The graph has a cycle");
  }
  return topo_order;
}

template <class AdjacencyLists>
absl::StatusOr<std::vector<int>> FindCycleInGraph(const AdjacencyLists& adj) {
  const size_t num_nodes = adj.size();
  if (num_nodes > std::numeric_limits<int>::max()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Too many nodes: adj.size()=%d", adj.size()));
  }

  // To find a cycle, we start a DFS from each yet-unvisited node and
  // try to find a cycle, if we don't find it then we know for sure that
  // no cycle is reachable from any of the explored nodes (so, we don't
  // explore them in later DFSs).
  std::vector<bool> no_cycle_reachable_from(num_nodes, false);
  // The DFS stack will contain a chain of nodes, from the root of the
  // DFS to the current leaf.
  struct DfsState {
    int node;
    // Points at the first child node that we did *not* yet look at.
    int adj_list_index;
    explicit DfsState(int _node) : node(_node), adj_list_index(0) {}
  };
  std::vector<DfsState> dfs_stack;
  std::vector<bool> in_cur_stack(num_nodes, false);
  for (int start_node = 0; start_node < static_cast<int>(num_nodes);
       ++start_node) {
    if (no_cycle_reachable_from[start_node]) continue;
    // Start the DFS.
    dfs_stack.push_back(DfsState(start_node));
    in_cur_stack[start_node] = true;
    while (!dfs_stack.empty()) {
      DfsState* cur_state = &dfs_stack.back();
      if (static_cast<size_t>(cur_state->adj_list_index) >=
          adj[cur_state->node].size()) {
        no_cycle_reachable_from[cur_state->node] = true;
        in_cur_stack[cur_state->node] = false;
        dfs_stack.pop_back();
        continue;
      }
      // Look at the current child, and increase the current state's
      // adj_list_index.
      // TODO(user): Caching adj[cur_state->node] in a local stack to improve
      // locality and so that the [] operator is called exactly once per node.
      const int child = adj[cur_state->node][cur_state->adj_list_index++];
      if (static_cast<size_t>(child) >= num_nodes) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "Invalid child %d in adj[%d]", child, cur_state->node));
      }
      if (no_cycle_reachable_from[child]) continue;
      if (in_cur_stack[child]) {
        // We detected a cycle! It corresponds to the tail end of dfs_stack,
        // in reverse order, until we find "child".
        int cycle_start = dfs_stack.size() - 1;
        while (dfs_stack[cycle_start].node != child) --cycle_start;
        const int cycle_size = dfs_stack.size() - cycle_start;
        std::vector<int> cycle(cycle_size);
        for (int c = 0; c < cycle_size; ++c) {
          cycle[c] = dfs_stack[cycle_start + c].node;
        }
        return cycle;
      }
      // Push the child onto the stack.
      dfs_stack.push_back(DfsState(child));
      in_cur_stack[child] = true;
      // Verify that its adjacency list seems valid.
      if (adj[child].size() > std::numeric_limits<int>::max()) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "Invalid adj[%d].size() = %d", child, adj[child].size()));
      }
    }
  }
  // If we're here, then all the DFS stopped, and there is no cycle.
  return std::vector<int>{};
}

}  // namespace graph
}  // namespace util

#endif  // UTIL_GRAPH_TOPOLOGICALSORTER_H__
