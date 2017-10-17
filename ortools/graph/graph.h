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
//
// This file defines a generic graph interface on which most algorithms can be
// built and provides a few efficient implementations with a fast construction
// time. Its design is based on the experience acquired by the Operations
// Research team in their various graph algorithm implementations.
//
// The main ideas are:
// - Graph nodes and arcs are represented by integers.
// - Node or arc annotations (weight, cost, ...) are not part of the graph
//   class, they can be stored outside in one or more arrays and can be easily
//   retrieved using a node or arc as an index.
//
// Terminology:
// - An arc of a graph is directed and going from a tail node to a head node.
// - Some implementations also store 'reverse' arcs and can be used for
//   undirected graph or flow-like algorithm.
// - A node or arc index is 'valid' if it represents a node or arc of
//   the graph. The validity ranges are always [0, num_nodes()) for nodes and
//   [0, num_arcs()) for forward arcs. Reverse arcs are elements of
//   [-num_arcs(), 0) and are also considered valid by the implementations that
//   store them.
//
// Provided implementations:
//   - ListGraph<> for the simplest api. Also aliased to util::Graph.
//   - StaticGraph<> for performance, but require calling Build(), see below
//   - CompleteGraph<> if you need a fully connected graph
//   - CompleteBipartiteGraph<> if you need a fully connected bipartite graph
//   - ReverseArcListGraph<> to add reverse arcs to ListGraph<>
//   - ReverseArcStaticGraph<> to add reverse arcs to StaticGraph<>
//   - ReverseArcMixedGraph<> for a smaller memory footprint
//
// Utility classes & functions:
//   - Permute() to permute an array according to a given permutation.
//   - SVector<> vector with index range [-size(), size()) for ReverseArcGraph.
//
// Basic usage:
//   typedef ListGraph<> Graph;  // Choose a graph implementation.
//   Graph graph;
//   for (...) {
//     graph.AddArc(tail, head);
//   }
//   ...
//   for (int node = 0; node < graph.num_nodes(); ++node) {
//     for (const int arc : graph.OutgoingArcs(node)) {
//       head = graph.Head(arc);
//       tail = node;  // or graph.Tail(arc) which is fast but not as much.
//     }
//   }
//
// Iteration over the arcs touching a node:
//
// - OutgoingArcs(node): All the forward arcs leaving the node.
// - IncomingArcs(node): All the forward arcs arriving at the node.
//
// And a more involved one:
//
// - OutgoingOrOppositeIncomingArcs(node): This returns both the forward arcs
//   leaving the node (i.e. OutgoingArcs(node)) and the reverse arcs leaving the
//   node (i.e. the opposite arcs of the ones returned by IncomingArcs(node)).
//
// Note on iteration efficiency: When re-indexing the arcs it is not possible to
// have both the outgoing arcs and the incoming ones form a consecutive range.
//
// It is however possible to do so for the outgoing arcs and the opposite
// incoming arcs. It is why the OutgoingOrOppositeIncomingArcs() and
// OutgoingArcs() iterations are more efficient than the IncomingArcs() one. If
// needed, we can add a OppositeIncomingArcs() function to quickly iterate on
// the opposite incoming arcs (and their heads).
//
// If you know the graph size in advance, this already set the number of nodes,
// reserve space for the arcs and check in DEBUG mode that you don't go over the
// bounds:
//   Graph graph(num_nodes, arc_capacity);
//
// Storing and using node annotations:
//   std::vector<bool> is_visited(graph.num_nodes(), false);
//   ...
//   for (int node = 0; node < graph.num_nodes(); ++node) {
//     if (!is_visited[node]) ...
//   }
//
// Storing and using arc annotations:
//   std::vector<int> weights;
//   for (...) {
//     graph.AddArc(tail, head);
//     weights.push_back(arc_weight);
//   }
//   ...
//   for (const int arc : graph.OutgoingArcs(node)) {
//     ... weights[arc] ...;
//   }
//
// More efficient version:
//   typedef StaticGraph<> Graph;
//   Graph graph(num_nodes, arc_capacity);  // Optional, but help memory usage.
//   std::vector<int> weights;
//   weights.reserve(arc_capacity);  // Optional, but help memory usage.
//   for (...) {
//     graph.AddArc(tail, head);
//     weights.push_back(arc_weight);
//   }
//   ...
//   std::vector<Graph::ArcIndex> permutation;
//   graph.Build(&permutation);  // A static graph must be Build() before usage.
//   Permute(permutation, &weights);  // Build() may permute the arc index.
//   ...
//
// Encoding an undirected graph: If you don't need arc annotation, then the best
// is to add two arcs for each edge (one in each direction) to a directed graph.
// Otherwise you can do the following.
//
//   typedef ReverseArc... Graph;
//   Graph graph;
//   for (...) {
//     graph.AddArc(tail, head);  // or graph.AddArc(head, tail) but not both.
//     edge_annotations.push_back(value);
//   }
//   ...
//   for (const Graph::NodeIndex node : graph.AllNodes()) {
//     for (const Graph::ArcIndex arc :
//          graph.OutgoingOrOppositeIncomingArcs(node)) {
//       destination = graph.Head(arc);
//       annotation = edge_annotations[arc < 0 ? graph.OppositeArc(arc) : arc];
//     }
//   }
//
//
// Note: The graphs are primarily designed to be constructed first and then used
// because it covers most of the use cases. It is possible to extend the
// interface with more dynamicity (like removing arcs), but this is not done at
// this point. Note that a "dynamic" implementation will break some assumptions
// we make on what node or arc are valid and also on the indices returned by
// AddArc(). Some arguments for simplifying the interface at the cost of
// dynamicity are:
//
// - It is always possible to construct a static graph from a dynamic one
//   before calling a complex algo.
// - If you really need a dynamic graph, maybe it is better to compute a graph
//   property incrementally rather than calling an algorithm that starts from
//   scratch each time.

#ifndef UTIL_GRAPH_GRAPH_H_
#define UTIL_GRAPH_GRAPH_H_

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/port.h"
#include "ortools/graph/iterators.h"


namespace util {

// Forward declaration.
template <typename T>
class SVector;

// Base class of all Graphs implemented here. The default value for the graph
// index types is int32 since allmost all graphs that fit into memory do not
// need bigger indices.
//
// Note: The type can be unsigned, except for the graphs with reverse arcs
// where the ArcIndexType must be signed, but not necessarly the NodeIndexType.
template <typename NodeIndexType = int32, typename ArcIndexType = int32,
          bool HasReverseArcs = false>
class BaseGraph {
 public:
  // Typedef so you can use Graph::NodeIndex and Graph::ArcIndex to be generic
  // but also to improve the readability of your code. We also recommend
  // that you define a typedef ... Graph; for readability.
  typedef NodeIndexType NodeIndex;
  typedef ArcIndexType ArcIndex;

  BaseGraph()
      : num_nodes_(0),
        node_capacity_(0),
        num_arcs_(0),
        arc_capacity_(0),
        const_capacities_(false) {}
  virtual ~BaseGraph() {}

  // Returns the number of valid nodes in the graph.
  NodeIndexType num_nodes() const { return num_nodes_; }

  // Returns the number of valid arcs in the graph.
  ArcIndexType num_arcs() const { return num_arcs_; }

  // Allows nice range-based for loop:
  //   for (const NodeIndex node : graph.AllNodes()) { ... }
  //   for (const ArcIndex arc : graph.AllForwardArcs()) { ... }
  IntegerRange<NodeIndex> AllNodes() const;
  IntegerRange<ArcIndex> AllForwardArcs() const;

  // Returns true if the given node is a valid node of the graph.
  bool IsNodeValid(NodeIndexType node) const {
    return node >= 0 && node < num_nodes_;
  }

  // Returns true if the given arc is a valid arc of the graph.
  // Note that the arc validity range changes for graph with reverse arcs.
  bool IsArcValid(ArcIndexType arc) const {
    return (HasReverseArcs ? -num_arcs_ : 0) <= arc && arc < num_arcs_;
  }

  // Capacity reserved for future nodes, always >= num_nodes_.
  NodeIndexType node_capacity() const;

  // Capacity reserved for future arcs, always >= num_arcs_.
  ArcIndexType arc_capacity() const;

  // Changes the graph capacities. The functions will fail in debug mode if:
  // - const_capacities_ is true.
  // - A valid node does not fall into the new node range.
  // - A valid arc does not fall into the new arc range.
  // In non-debug mode, const_capacities_ is ignored and nothing will happen
  // if the new capacity value for the arcs or the nodes is too small.
  virtual void ReserveNodes(NodeIndexType bound) {
    DCHECK(!const_capacities_);
    DCHECK_GE(bound, num_nodes_);
    if (bound <= num_nodes_) return;
    node_capacity_ = bound;
  }
  virtual void ReserveArcs(ArcIndexType bound) {
    DCHECK(!const_capacities_);
    DCHECK_GE(bound, num_arcs_);
    if (bound <= num_arcs_) return;
    arc_capacity_ = bound;
  }
  void Reserve(NodeIndexType node_capacity, ArcIndexType arc_capacity) {
    ReserveNodes(node_capacity);
    ReserveArcs(arc_capacity);
  }

  // FreezeCapacities() makes any future attempt to change the graph capacities
  // crash in DEBUG mode.
  void FreezeCapacities();

  // Constants that will never be a valid node or arc.
  // They are the maximum possible node and arc capacity.
  static const NodeIndexType kNilNode;
  static const ArcIndexType kNilArc;

  // TODO(user): remove the public functions below. They are just here during
  // the transition from the old ebert_graph api to this new graph api.
  template <typename A, typename B>
  void GroupForwardArcsByFunctor(const A& a, B* b) {
    LOG(FATAL) << "Not supported";
  }
  ArcIndexType max_end_arc_index() const { return arc_capacity_; }

 protected:
  // Functions commented when defined because they are implementation details.
  void ComputeCumulativeSum(std::vector<ArcIndexType>* v);
  void BuildStartAndForwardHead(SVector<NodeIndexType>* head,
                                std::vector<ArcIndexType>* start,
                                std::vector<ArcIndexType>* permutation);

  NodeIndexType num_nodes_;
  NodeIndexType node_capacity_;
  ArcIndexType num_arcs_;
  ArcIndexType arc_capacity_;
  bool const_capacities_;
};

// Basic graph implementation without reverse arc. This class also serves as a
// documentation for the generic graph interface (minus the part related to
// reverse arcs).
//
// This implementation uses a linked list and compared to StaticGraph:
// - Is a bit faster to construct (if the arcs are not ordered by tail).
// - Does not require calling Build().
// - Has slower outgoing arc iteration.
// - Uses more memory: ArcIndexType * node_capacity()
//   + (ArcIndexType + NodeIndexType) * arc_capacity().
// - Has an efficient Tail() but need an extra NodeIndexType/arc memory for it.
// - Never changes the initial arc index returned by AddArc().
//
template <typename NodeIndexType = int32, typename ArcIndexType = int32>
class ListGraph : public BaseGraph<NodeIndexType, ArcIndexType, false> {
  typedef BaseGraph<NodeIndexType, ArcIndexType, false> Base;
  using Base::num_arcs_;
  using Base::num_nodes_;
  using Base::arc_capacity_;
  using Base::node_capacity_;
  using Base::const_capacities_;

 public:
  using Base::IsArcValid;
  ListGraph() {}

  // Reserve space for the graph at construction and do not allow it to grow
  // beyond that, see FreezeCapacities(). This constructor also makes any nodes
  // in [0, num_nodes) valid.
  ListGraph(NodeIndexType num_nodes, ArcIndexType arc_capacity) {
    this->Reserve(num_nodes, arc_capacity);
    this->FreezeCapacities();
    this->AddNode(num_nodes - 1);
  }

  // If node is not a valid node, sets num_nodes_ to node + 1 so that the given
  // node becomes valid. It will fail in DEBUG mode if the capacities are fixed
  // and the new node is out of range.
  void AddNode(NodeIndexType node);

  // Adds an arc to the graph and returns its current index which will always
  // be num_arcs() - 1. It will also automatically call AddNode(tail)
  // and AddNode(head). It will fail in DEBUG mode if the capacities
  // are fixed and this cause the graph to grow beyond them.
  //
  // Note: Self referencing arcs and duplicate arcs are supported.
  ArcIndexType AddArc(NodeIndexType tail, NodeIndexType head);

  // Some graph implementations need to be finalized with Build() before they
  // can be used. After Build() is called, the arc indices (which had been the
  // return values of previous AddArc() calls) may change: the new index of
  // former arc #i will be stored in permutation[i] if #i is smaller than
  // permutation.size() or will be unchanged otherwise. If you don't care about
  // these, just call the simple no-output version Build().
  //
  // Note that some implementations become immutable after calling Build().
  void Build() { Build(nullptr); }
  void Build(std::vector<ArcIndexType>* permutation);

  // Do not use directly.
  class OutgoingArcIterator;
  class OutgoingHeadIterator;

  // Graph jargon: the "degree" of a node is its number of arcs. The out-degree
  // is the number of outgoing arcs. The in-degree is the number of incoming
  // arcs, and is only available for some graph implementations, below.
  //
  // ListGraph<>::OutDegree() works in O(degree).
  ArcIndexType OutDegree(NodeIndexType node) const;

  // Allows to iterate over the forward arcs that verify Tail(arc) == node.
  // This is meant to be used as:
  //   for (const ArcIndex arc : graph.OutgoingArcs(node)) { ... }
  BeginEndWrapper<OutgoingArcIterator> OutgoingArcs(NodeIndexType node) const;

  // Advanced usage. Same as OutgoingArcs(), but allows to restart the iteration
  // from an already known outgoing arc of the given node.
  BeginEndWrapper<OutgoingArcIterator> OutgoingArcsStartingFrom(
      NodeIndexType node, ArcIndexType from) const;

  // This loops over the heads of the OutgoingArcs(node). It is just a more
  // convenient way to achieve this. Moreover this interface is used by some
  // graph algorithms.
  BeginEndWrapper<OutgoingHeadIterator> operator[](NodeIndexType node) const;

  // Returns the tail/head of a valid arc.
  NodeIndexType Tail(ArcIndexType arc) const;
  NodeIndexType Head(ArcIndexType arc) const;

  void ReserveNodes(NodeIndexType bound) override;
  void ReserveArcs(ArcIndexType bound) override;

 private:
  std::vector<ArcIndexType> start_;
  std::vector<ArcIndexType> next_;
  std::vector<NodeIndexType> head_;
  std::vector<NodeIndexType> tail_;
  DISALLOW_COPY_AND_ASSIGN(ListGraph);
};

// Most efficient implementation of a graph without reverse arcs:
// - Build() needs to be called after the arc and node have been added.
// - The graph is really compact memory wise:
//   ArcIndexType * node_capacity() + 2 * NodeIndexType * arc_capacity(),
//   but when Build() is called it uses a temporary extra space of
//   ArcIndexType * arc_capacity().
// - The construction is really fast.
//
// NOTE(user): if the need arises for very-well compressed graphs, we could
// shave NodeIndexType * arc_capacity() off the permanent memory requirement
// with a similar class that doesn't support Tail(), i.e.
// StaticGraphWithoutTail<>. This almost corresponds to a past implementation
// of StaticGraph<> @CL 116144340.
template <typename NodeIndexType = int32, typename ArcIndexType = int32>
class StaticGraph : public BaseGraph<NodeIndexType, ArcIndexType, false> {
  typedef BaseGraph<NodeIndexType, ArcIndexType, false> Base;
  using Base::num_arcs_;
  using Base::num_nodes_;
  using Base::arc_capacity_;
  using Base::node_capacity_;
  using Base::const_capacities_;

 public:
  using Base::IsArcValid;
  StaticGraph() : is_built_(false), arc_in_order_(true), last_tail_seen_(0) {}
  StaticGraph(NodeIndexType num_nodes, ArcIndexType arc_capacity)
      : is_built_(false), arc_in_order_(true), last_tail_seen_(0) {
    this->Reserve(num_nodes, arc_capacity);
    this->FreezeCapacities();
    this->AddNode(num_nodes - 1);
  }

  // Do not use directly. See instead the arc iteration functions below.
  class OutgoingArcIterator;

  NodeIndexType Head(ArcIndexType arc) const;
  NodeIndexType Tail(ArcIndexType arc) const;
  ArcIndexType OutDegree(NodeIndexType node) const;  // Work in O(1).
  BeginEndWrapper<OutgoingArcIterator> OutgoingArcs(NodeIndexType node) const;
  BeginEndWrapper<OutgoingArcIterator> OutgoingArcsStartingFrom(
      NodeIndexType node, ArcIndexType from) const;

  // This loops over the heads of the OutgoingArcs(node). It is just a more
  // convenient way to achieve this. Moreover this interface is used by some
  // graph algorithms.
  BeginEndWrapper<NodeIndexType const*> operator[](NodeIndexType node) const;

  void ReserveNodes(NodeIndexType bound) override;
  void ReserveArcs(ArcIndexType bound) override;
  void AddNode(NodeIndexType node);
  ArcIndexType AddArc(NodeIndexType tail, NodeIndexType head);

  void Build() { Build(nullptr); }
  void Build(std::vector<ArcIndexType>* permutation);

 private:
  ArcIndexType DirectArcLimit(NodeIndexType node) const {
    DCHECK(is_built_);
    DCHECK(Base::IsNodeValid(node));
    return node + 1 < num_nodes_ ? start_[node + 1] : num_arcs_;
  }

  bool is_built_;
  bool arc_in_order_;
  NodeIndexType last_tail_seen_;
  std::vector<ArcIndexType> start_;
  std::vector<NodeIndexType> head_;
  std::vector<NodeIndexType> tail_;
  DISALLOW_COPY_AND_ASSIGN(StaticGraph);
};

// Extends the ListGraph by also storing the reverse arcs.
// This class also documents the Graph interface related to reverse arc.
// - NodeIndexType can be unsigned, but ArcIndexType must be signed.
// - It has most of the same advantanges and disadvantages as ListGraph.
// - It takes 2 * ArcIndexType * node_capacity()
//   + 2 * (ArcIndexType + NodeIndexType) * arc_capacity() memory.
template <typename NodeIndexType = int32, typename ArcIndexType = int32>
class ReverseArcListGraph
    : public BaseGraph<NodeIndexType, ArcIndexType, true> {
  typedef BaseGraph<NodeIndexType, ArcIndexType, true> Base;
  using Base::num_arcs_;
  using Base::num_nodes_;
  using Base::arc_capacity_;
  using Base::node_capacity_;
  using Base::const_capacities_;

 public:
  using Base::IsArcValid;
  ReverseArcListGraph() {}
  ReverseArcListGraph(NodeIndexType num_nodes, ArcIndexType arc_capacity) {
    this->Reserve(num_nodes, arc_capacity);
    this->FreezeCapacities();
    this->AddNode(num_nodes - 1);
  }

  // Returns the opposite arc of a given arc. That is the reverse arc of the
  // given forward arc or the forward arc of a given reverse arc.
  ArcIndexType OppositeArc(ArcIndexType arc) const;

  // Do not use directly. See instead the arc iteration functions below.
  class OutgoingOrOppositeIncomingArcIterator;
  class IncomingArcIterator;
  class OutgoingArcIterator;
  class OutgoingHeadIterator;

  // ReverseArcListGraph<>::OutDegree() and ::InDegree() work in O(degree).
  ArcIndexType OutDegree(NodeIndexType node) const;
  ArcIndexType InDegree(NodeIndexType node) const;

  // Arc iterations functions over the arcs touching a node (see the top-level
  // comment for the different types). To be used as follows:
  //   for (const Graph::ArcIndex arc : IterationFunction(node)) { ... }
  //
  // The StartingFrom() version are similar, but restart the iteration from a
  // given arc position (which must be valid in the iteration context).
  BeginEndWrapper<OutgoingArcIterator> OutgoingArcs(NodeIndexType node) const;
  BeginEndWrapper<IncomingArcIterator> IncomingArcs(NodeIndexType node) const;
  BeginEndWrapper<OutgoingOrOppositeIncomingArcIterator>
  OutgoingOrOppositeIncomingArcs(NodeIndexType node) const;
  BeginEndWrapper<OutgoingArcIterator> OutgoingArcsStartingFrom(
      NodeIndexType node, ArcIndexType from) const;
  BeginEndWrapper<IncomingArcIterator> IncomingArcsStartingFrom(
      NodeIndexType node, ArcIndexType from) const;
  BeginEndWrapper<OutgoingOrOppositeIncomingArcIterator>
  OutgoingOrOppositeIncomingArcsStartingFrom(NodeIndexType node,
                                             ArcIndexType from) const;

  // This loops over the heads of the OutgoingArcs(node). It is just a more
  // convenient way to achieve this. Moreover this interface is used by some
  // graph algorithms.
  BeginEndWrapper<OutgoingHeadIterator> operator[](NodeIndexType node) const;

  NodeIndexType Head(ArcIndexType arc) const;
  NodeIndexType Tail(ArcIndexType arc) const;

  void ReserveNodes(NodeIndexType bound) override;
  void ReserveArcs(ArcIndexType bound) override;
  void AddNode(NodeIndexType node);
  ArcIndexType AddArc(NodeIndexType tail, NodeIndexType head);

  void Build() { Build(nullptr); }
  void Build(std::vector<ArcIndexType>* permutation);

 private:
  std::vector<ArcIndexType> start_;
  std::vector<ArcIndexType> reverse_start_;
  SVector<ArcIndexType> next_;
  SVector<NodeIndexType> head_;
  DISALLOW_COPY_AND_ASSIGN(ReverseArcListGraph);
};

// StaticGraph with reverse arc.
// - NodeIndexType can be unsigned, but ArcIndexType must be signed.
// - It has most of the same advantanges and disadvantages as StaticGraph.
// - It takes 2 * ArcIndexType * node_capacity()
//   + 2 * (ArcIndexType + NodeIndexType) * arc_capacity() memory.
// - If the ArcIndexPermutation is needed, then an extra ArcIndexType *
//   arc_capacity() is needed for it.
// - The reverse arcs from a node are sorted by head (so we could add a log()
//   time lookup function).
template <typename NodeIndexType = int32, typename ArcIndexType = int32>
class ReverseArcStaticGraph
    : public BaseGraph<NodeIndexType, ArcIndexType, true> {
  typedef BaseGraph<NodeIndexType, ArcIndexType, true> Base;
  using Base::num_arcs_;
  using Base::num_nodes_;
  using Base::arc_capacity_;
  using Base::node_capacity_;
  using Base::const_capacities_;

 public:
  using Base::IsArcValid;
  ReverseArcStaticGraph() : is_built_(false) {}
  ReverseArcStaticGraph(NodeIndexType num_nodes, ArcIndexType arc_capacity)
      : is_built_(false) {
    this->Reserve(num_nodes, arc_capacity);
    this->FreezeCapacities();
    this->AddNode(num_nodes - 1);
  }

  // Deprecated.
  class OutgoingOrOppositeIncomingArcIterator;
  class IncomingArcIterator;
  class OutgoingArcIterator;

  // ReverseArcStaticGraph<>::OutDegree() and ::InDegree() work in O(1).
  ArcIndexType OutDegree(NodeIndexType node) const;
  ArcIndexType InDegree(NodeIndexType node) const;

  BeginEndWrapper<OutgoingArcIterator> OutgoingArcs(NodeIndexType node) const;
  BeginEndWrapper<IncomingArcIterator> IncomingArcs(NodeIndexType node) const;
  BeginEndWrapper<OutgoingOrOppositeIncomingArcIterator>
  OutgoingOrOppositeIncomingArcs(NodeIndexType node) const;
  BeginEndWrapper<OutgoingArcIterator> OutgoingArcsStartingFrom(
      NodeIndexType node, ArcIndexType from) const;
  BeginEndWrapper<IncomingArcIterator> IncomingArcsStartingFrom(
      NodeIndexType node, ArcIndexType from) const;
  BeginEndWrapper<OutgoingOrOppositeIncomingArcIterator>
  OutgoingOrOppositeIncomingArcsStartingFrom(NodeIndexType node,
                                             ArcIndexType from) const;

  // This loops over the heads of the OutgoingArcs(node). It is just a more
  // convenient way to achieve this. Moreover this interface is used by some
  // graph algorithms.
  BeginEndWrapper<NodeIndexType const*> operator[](NodeIndexType node) const;

  ArcIndexType OppositeArc(ArcIndexType arc) const;
  // TODO(user): support Head() and Tail() before Build(), like StaticGraph<>.
  NodeIndexType Head(ArcIndexType arc) const;
  NodeIndexType Tail(ArcIndexType arc) const;

  void ReserveArcs(ArcIndexType bound) override;
  void AddNode(NodeIndexType node);
  ArcIndexType AddArc(NodeIndexType tail, NodeIndexType head);

  void Build() { Build(nullptr); }
  void Build(std::vector<ArcIndexType>* permutation);

 private:
  ArcIndexType DirectArcLimit(NodeIndexType node) const {
    DCHECK(is_built_);
    DCHECK(Base::IsNodeValid(node));
    return node + 1 < num_nodes_ ? start_[node + 1] : num_arcs_;
  }
  ArcIndexType ReverseArcLimit(NodeIndexType node) const {
    DCHECK(is_built_);
    DCHECK(Base::IsNodeValid(node));
    return node + 1 < num_nodes_ ? reverse_start_[node + 1] : 0;
  }

  bool is_built_;
  std::vector<ArcIndexType> start_;
  std::vector<ArcIndexType> reverse_start_;
  SVector<NodeIndexType> head_;
  SVector<ArcIndexType> opposite_;
  DISALLOW_COPY_AND_ASSIGN(ReverseArcStaticGraph);
};

// This graph is a mix between the ReverseArcListGraph and the
// ReverseArcStaticGraph. It uses less memory:
// - It takes 2 * ArcIndexType * node_capacity()
//   + (2 * NodeIndexType + ArcIndexType) * arc_capacity() memory.
// - If the ArcIndexPermutation is needed, then an extra ArcIndexType *
//   arc_capacity() is needed for it.
template <typename NodeIndexType = int32, typename ArcIndexType = int32>
class ReverseArcMixedGraph
    : public BaseGraph<NodeIndexType, ArcIndexType, true> {
  typedef BaseGraph<NodeIndexType, ArcIndexType, true> Base;
  using Base::num_arcs_;
  using Base::num_nodes_;
  using Base::arc_capacity_;
  using Base::node_capacity_;
  using Base::const_capacities_;

 public:
  using Base::IsArcValid;
  ReverseArcMixedGraph() : is_built_(false) {}
  ReverseArcMixedGraph(NodeIndexType num_nodes, ArcIndexType arc_capacity)
      : is_built_(false) {
    this->Reserve(num_nodes, arc_capacity);
    this->FreezeCapacities();
    this->AddNode(num_nodes - 1);
  }

  // Deprecated.
  class OutgoingOrOppositeIncomingArcIterator;
  class IncomingArcIterator;
  class OutgoingArcIterator;

  ArcIndexType OutDegree(NodeIndexType node) const;  // O(1)
  ArcIndexType InDegree(NodeIndexType node) const;   // O(in-degree)

  BeginEndWrapper<OutgoingArcIterator> OutgoingArcs(NodeIndexType node) const;
  BeginEndWrapper<IncomingArcIterator> IncomingArcs(NodeIndexType node) const;
  BeginEndWrapper<OutgoingOrOppositeIncomingArcIterator>
  OutgoingOrOppositeIncomingArcs(NodeIndexType node) const;
  BeginEndWrapper<OutgoingArcIterator> OutgoingArcsStartingFrom(
      NodeIndexType node, ArcIndexType from) const;
  BeginEndWrapper<IncomingArcIterator> IncomingArcsStartingFrom(
      NodeIndexType node, ArcIndexType from) const;
  BeginEndWrapper<OutgoingOrOppositeIncomingArcIterator>
  OutgoingOrOppositeIncomingArcsStartingFrom(NodeIndexType node,
                                             ArcIndexType from) const;

  // This loops over the heads of the OutgoingArcs(node). It is just a more
  // convenient way to achieve this. Moreover this interface is used by some
  // graph algorithms.
  BeginEndWrapper<NodeIndexType const*> operator[](NodeIndexType node) const;

  ArcIndexType OppositeArc(ArcIndexType arc) const;
  // TODO(user): support Head() and Tail() before Build(), like StaticGraph<>.
  NodeIndexType Head(ArcIndexType arc) const;
  NodeIndexType Tail(ArcIndexType arc) const;

  void ReserveArcs(ArcIndexType bound) override;
  void AddNode(NodeIndexType node);
  ArcIndexType AddArc(NodeIndexType tail, NodeIndexType head);

  void Build() { Build(nullptr); }
  void Build(std::vector<ArcIndexType>* permutation);

 private:
  ArcIndexType DirectArcLimit(NodeIndexType node) const {
    DCHECK(is_built_);
    DCHECK(Base::IsNodeValid(node));
    return node + 1 < num_nodes_ ? start_[node + 1] : num_arcs_;
  }

  bool is_built_;
  std::vector<ArcIndexType> start_;
  std::vector<ArcIndexType> reverse_start_;
  std::vector<ArcIndexType> next_;
  SVector<NodeIndexType> head_;
  DISALLOW_COPY_AND_ASSIGN(ReverseArcMixedGraph);
};

// Permutes the elements of array_to_permute: element #i will be moved to
// position permutation[i]. permutation must be either empty (in which case
// nothing happens), or a permutation of [0, permutation.size()).
//
// The algorithm is fast but need extra memory for a copy of the permuted part
// of array_to_permute.
//
// TODO(user): consider slower but more memory efficient implementations that
// follow the cycles of the permutation and use a bitmap to indicate what has
// been permuted or to mark the beginning of each cycle.

// Some compiler do not know typeof(), so we have to use this extra function
// internally.
template <class IntVector, class Array, class ElementType>
void PermuteWithExplicitElementType(const IntVector& permutation,
                                    Array* array_to_permute,
                                    ElementType unused) {
  std::vector<ElementType> temp(permutation.size());
  for (int i = 0; i < permutation.size(); ++i) {
    temp[i] = (*array_to_permute)[i];
  }
  for (int i = 0; i < permutation.size(); ++i) {
    (*array_to_permute)[permutation[i]] = temp[i];
  }
}

template <class IntVector, class Array>
void Permute(const IntVector& permutation, Array* array_to_permute) {
  if (permutation.empty()) {
    return;
  }
  PermuteWithExplicitElementType(permutation, array_to_permute,
                                 (*array_to_permute)[0]);
}

// We need a specialization for std::vector<bool>, because the default code uses
// (*array_to_permute)[0] as ElementType, which isn't 'bool' in that case.
template <class IntVector>
void Permute(const IntVector& permutation,
             std::vector<bool>* array_to_permute) {
  if (permutation.empty()) {
    return;
  }
  bool unused = false;
  PermuteWithExplicitElementType(permutation, array_to_permute, unused);
}

// A vector-like class where valid indices are in [- size_, size_) and reserved
// indices for future growth are in [- capacity_, capacity_). It is used to hold
// arc related information for graphs with reverse arcs.
// It supports only up to 2^31-1 elements, for compactness. If you ever need
// more, consider using templates for the size/capacity integer types.
//
// Sample usage:
//
// SVector<int> v;
// v.grow(left_value, right_value);
// v.resize(10);
// v.clear();
// v.swap(new_v);
// std:swap(v[i], v[~i]);
template <typename T>
class SVector {
 public:
  SVector() : base_(nullptr), size_(0), capacity_(0) {}

  ~SVector() {
    clear();
    if (capacity_ > 0) {
      free(base_ - capacity_);
    }
  }

  T& operator[](int n) {
    DCHECK_LT(n, size_);
    DCHECK_GE(n, -size_);
    return base_[n];
  }

  const T& operator[](int n) const {
    DCHECK_LT(n, size_);
    DCHECK_GE(n, -size_);
    return base_[n];
  }

  void resize(int n, T left = T(), T right = T()) {
    reserve(n);
    for (int i = -n; i < -size_; ++i) {
      new (base_ + i) T(left);
    }
    for (int i = size_; i < n; ++i) {
      new (base_ + i) T(right);
    }
    for (int i = -size_; i < -n; ++i) {
      base_[i].~T();
    }
    for (int i = n; i < size_; ++i) {
      base_[i].~T();
    }
    size_ = n;
  }

  void clear() { resize(0); }

  T* data() const { return base_; }

  void swap(SVector<T>& x) {
    std::swap(base_, x.base_);
    std::swap(size_, x.size_);
    std::swap(capacity_, x.capacity_);
  }

  void reserve(int n) {
    DCHECK_GE(n, 0);
    DCHECK_LE(n, max_size());
    if (n > capacity_) {
      int new_capacity = n;
      size_t requested_block_size = 2LL * new_capacity * sizeof(T);
      T* new_storage = static_cast<T*>(malloc(requested_block_size));
      CHECK(new_storage != nullptr);
      size_t block_size = requested_block_size;
      if (block_size > 0) {
        new_capacity = static_cast<int>(std::min(
            static_cast<size_t>(max_size()), block_size / (2 * sizeof(T))));
      }
      T* new_base = new_storage + new_capacity;
      for (int i = -size_; i < size_; ++i) {
        new (new_base + i) T(base_[i]);
      }
      int temp = size_;
      clear();
      if (capacity_ > 0) {
        free(base_ - capacity_);
      }
      size_ = temp;
      base_ = new_base;
      capacity_ = new_capacity;
    }
  }

  void grow(const T& left = T(), const T& right = T()) {
    if (size_ == capacity_) {
      // We have to copy the elements because they are allowed to be element of
      // *this.
      T left_copy(left);    // NOLINT
      T right_copy(right);  // NOLINT
      reserve(NewCapacity(1));
      new (base_ + size_) T(right_copy);
      new (base_ - size_ - 1) T(left_copy);
      ++size_;
    } else {
      new (base_ + size_) T(right);
      new (base_ - size_ - 1) T(left);
      ++size_;
    }
  }

  int size() const { return size_; }

  int capacity() const { return capacity_; }

  int max_size() const { return std::numeric_limits<int>::max(); }

 private:
  int NewCapacity(int delta) {
    // TODO(user): check validity.
    double candidate = 1.3 * static_cast<double>(capacity_);
    if (candidate > static_cast<double>(max_size())) {
      candidate = static_cast<double>(max_size());
    }
    int new_capacity = static_cast<int>(candidate);
    if (new_capacity > capacity_ + delta) {
      return new_capacity;
    }
    return capacity_ + delta;
  }

  T* base_;       // Pointer to the element of index 0.
  int size_;      // Valid index are [- size_, size_).
  int capacity_;  // Reserved index are [- capacity_, capacity_).
  DISALLOW_COPY_AND_ASSIGN(SVector);
};

// BaseGraph implementation ----------------------------------------------------

template <typename NodeIndexType, typename ArcIndexType, bool HasReverseArcs>
IntegerRange<NodeIndexType>
BaseGraph<NodeIndexType, ArcIndexType, HasReverseArcs>::AllNodes() const {
  return IntegerRange<NodeIndexType>(0, num_nodes_);
}

template <typename NodeIndexType, typename ArcIndexType, bool HasReverseArcs>
IntegerRange<ArcIndexType>
BaseGraph<NodeIndexType, ArcIndexType, HasReverseArcs>::AllForwardArcs() const {
  return IntegerRange<ArcIndexType>(0, num_arcs_);
}

template <typename NodeIndexType, typename ArcIndexType, bool HasReverseArcs>
const NodeIndexType
    BaseGraph<NodeIndexType, ArcIndexType, HasReverseArcs>::kNilNode =
        std::numeric_limits<NodeIndexType>::max();

template <typename NodeIndexType, typename ArcIndexType, bool HasReverseArcs>
const ArcIndexType
    BaseGraph<NodeIndexType, ArcIndexType, HasReverseArcs>::kNilArc =
        std::numeric_limits<ArcIndexType>::max();

template <typename NodeIndexType, typename ArcIndexType, bool HasReverseArcs>
NodeIndexType
BaseGraph<NodeIndexType, ArcIndexType, HasReverseArcs>::node_capacity() const {
  // TODO(user): Is it needed? remove completely? return the real capacities
  // at the cost of having a different implementation for each graphs?
  return node_capacity_ > num_nodes_ ? node_capacity_ : num_nodes_;
}

template <typename NodeIndexType, typename ArcIndexType, bool HasReverseArcs>
ArcIndexType
BaseGraph<NodeIndexType, ArcIndexType, HasReverseArcs>::arc_capacity() const {
  // TODO(user): Same questions as the ones in node_capacity().
  return arc_capacity_ > num_arcs_ ? arc_capacity_ : num_arcs_;
}

template <typename NodeIndexType, typename ArcIndexType, bool HasReverseArcs>
void BaseGraph<NodeIndexType, ArcIndexType,
               HasReverseArcs>::FreezeCapacities() {
  // TODO(user): Only define this in debug mode at the cost of having a lot
  // of ifndef NDEBUG all over the place? remove the function completely ?
  const_capacities_ = true;
  node_capacity_ = std::max(node_capacity_, num_nodes_);
  arc_capacity_ = std::max(arc_capacity_, num_arcs_);
}

// Computes the cummulative sum of the entry in v. We only use it with
// in/out degree distribution, hence the Check() at the end.
template <typename NodeIndexType, typename ArcIndexType, bool HasReverseArcs>
void BaseGraph<NodeIndexType, ArcIndexType, HasReverseArcs>::
    ComputeCumulativeSum(std::vector<ArcIndexType>* v) {
  ArcIndexType sum = 0;
  for (int i = 0; i < num_nodes_; ++i) {
    ArcIndexType temp = (*v)[i];
    (*v)[i] = sum;
    sum += temp;
  }
  DCHECK(sum == num_arcs_);
}

// Given the tail of arc #i in (*head)[i] and the head of arc #i in (*head)[~i]
// - Reorder the arc by increasing tail.
// - Put the head of the new arc #i in (*head)[i].
// - Put in start[i] the index of the first arc with tail >= i.
// - Update "permutation" to reflect the change, unless it is NULL.
template <typename NodeIndexType, typename ArcIndexType, bool HasReverseArcs>
void BaseGraph<NodeIndexType, ArcIndexType, HasReverseArcs>::
    BuildStartAndForwardHead(SVector<NodeIndexType>* head,
                             std::vector<ArcIndexType>* start,
                             std::vector<ArcIndexType>* permutation) {
  // Computes the outgoing degree of each nodes and check if we need to permute
  // something or not. Note that the tails are currently stored in the positive
  // range of the SVector head.
  start->assign(num_nodes_, 0);
  int last_tail_seen = 0;
  bool permutation_needed = false;
  for (int i = 0; i < num_arcs_; ++i) {
    NodeIndexType tail = (*head)[i];
    if (!permutation_needed) {
      permutation_needed = tail < last_tail_seen;
      last_tail_seen = tail;
    }
    (*start)[tail]++;
  }
  ComputeCumulativeSum(start);

  // Abort early if we do not need the permutation: we only need to put the
  // heads in the positive range.
  if (!permutation_needed) {
    for (int i = 0; i < num_arcs_; ++i) {
      (*head)[i] = (*head)[~i];
    }
    if (permutation != nullptr) {
      permutation->clear();
    }
    return;
  }

  // Computes the forward arc permutation.
  // Note that this temporarily alters the start vector.
  std::vector<ArcIndexType> perm(num_arcs_);
  for (int i = 0; i < num_arcs_; ++i) {
    perm[i] = (*start)[(*head)[i]]++;
  }

  // Restore in (*start)[i] the index of the first arc with tail >= i.
  for (int i = num_nodes_ - 1; i > 0; --i) {
    (*start)[i] = (*start)[i - 1];
  }
  (*start)[0] = 0;

  // Permutes the head into their final position in head.
  // We do not need the tails anymore at this point.
  for (int i = 0; i < num_arcs_; ++i) {
    (*head)[perm[i]] = (*head)[~i];
  }
  if (permutation != nullptr) {
    permutation->swap(perm);
  }
}

// ---------------------------------------------------------------------------
// Macros to wrap old style iteration into the new range-based for loop style.
// ---------------------------------------------------------------------------

// The parameters are:
// - c: the class name.
// - t: the iteration type (Outgoing, Incoming or OutgoingOrOppositeIncoming).
// - e: the "end" ArcIndexType.
#define DEFINE_RANGE_BASED_ARC_ITERATION(c, t, e)                             \
  template <typename NodeIndexType, typename ArcIndexType>                    \
  BeginEndWrapper<typename c<NodeIndexType, ArcIndexType>::t##ArcIterator>    \
      c<NodeIndexType, ArcIndexType>::t##Arcs(NodeIndexType node) const {     \
    return BeginEndWrapper<t##ArcIterator>(t##ArcIterator(*this, node),       \
                                           t##ArcIterator(*this, node, e));   \
  }                                                                           \
  template <typename NodeIndexType, typename ArcIndexType>                    \
  BeginEndWrapper<typename c<NodeIndexType, ArcIndexType>::t##ArcIterator>    \
      c<NodeIndexType, ArcIndexType>::t##ArcsStartingFrom(                    \
          NodeIndexType node, ArcIndexType from) const {                      \
    return BeginEndWrapper<t##ArcIterator>(t##ArcIterator(*this, node, from), \
                                           t##ArcIterator(*this, node, e));   \
  }

// Adapt our old iteration style to support range-based for loops. Add typedefs
// required by std::iterator_traits.
#define DEFINE_STL_ITERATOR_FUNCTIONS(iterator_class_name)  \
  using iterator_category = std::input_iterator_tag;        \
  using difference_type = ptrdiff_t;                        \
  using pointer = const ArcIndexType*;                      \
  using reference = const ArcIndexType&;                    \
  using value_type = ArcIndexType;                          \
  bool operator!=(const iterator_class_name& other) const { \
    return index_ != other.index_;                          \
  }                                                         \
  bool operator==(const iterator_class_name& other) const { \
    return index_ == other.index_;                          \
  }                                                         \
  ArcIndexType operator*() const { return Index(); }        \
  void operator++() { Next(); }

// ListGraph implementation ----------------------------------------------------

DEFINE_RANGE_BASED_ARC_ITERATION(ListGraph, Outgoing, Base::kNilArc);

template <typename NodeIndexType, typename ArcIndexType>
BeginEndWrapper<
    typename ListGraph<NodeIndexType, ArcIndexType>::OutgoingHeadIterator>
    ListGraph<NodeIndexType, ArcIndexType>::operator[](
        NodeIndexType node) const {
  return BeginEndWrapper<OutgoingHeadIterator>(
      OutgoingHeadIterator(*this, node),
      OutgoingHeadIterator(*this, node, Base::kNilArc));
}

template <typename NodeIndexType, typename ArcIndexType>
NodeIndexType ListGraph<NodeIndexType, ArcIndexType>::Tail(
    ArcIndexType arc) const {
  DCHECK(IsArcValid(arc));
  return tail_[arc];
}

template <typename NodeIndexType, typename ArcIndexType>
NodeIndexType ListGraph<NodeIndexType, ArcIndexType>::Head(
    ArcIndexType arc) const {
  DCHECK(IsArcValid(arc));
  return head_[arc];
}

template <typename NodeIndexType, typename ArcIndexType>
ArcIndexType ListGraph<NodeIndexType, ArcIndexType>::OutDegree(
    NodeIndexType node) const {
  ArcIndexType degree(0);
  for (auto arc ATTRIBUTE_UNUSED : OutgoingArcs(node)) ++degree;
  return degree;
}

template <typename NodeIndexType, typename ArcIndexType>
void ListGraph<NodeIndexType, ArcIndexType>::AddNode(NodeIndexType node) {
  if (node < num_nodes_) return;
  DCHECK(!const_capacities_ || node < node_capacity_);
  num_nodes_ = node + 1;
  start_.resize(num_nodes_, Base::kNilArc);
}

template <typename NodeIndexType, typename ArcIndexType>
ArcIndexType ListGraph<NodeIndexType, ArcIndexType>::AddArc(
    NodeIndexType tail, NodeIndexType head) {
  DCHECK_GE(tail, 0);
  DCHECK_GE(head, 0);
  AddNode(tail > head ? tail : head);
  head_.push_back(head);
  tail_.push_back(tail);
  next_.push_back(start_[tail]);
  start_[tail] = num_arcs_;
  DCHECK(!const_capacities_ || num_arcs_ < arc_capacity_);
  return num_arcs_++;
}

template <typename NodeIndexType, typename ArcIndexType>
void ListGraph<NodeIndexType, ArcIndexType>::ReserveNodes(NodeIndexType bound) {
  Base::ReserveNodes(bound);
  if (bound <= num_nodes_) return;
  start_.reserve(bound);
}

template <typename NodeIndexType, typename ArcIndexType>
void ListGraph<NodeIndexType, ArcIndexType>::ReserveArcs(ArcIndexType bound) {
  Base::ReserveArcs(bound);
  if (bound <= num_arcs_) return;
  head_.reserve(bound);
  tail_.reserve(bound);
  next_.reserve(bound);
}

template <typename NodeIndexType, typename ArcIndexType>
void ListGraph<NodeIndexType, ArcIndexType>::Build(
    std::vector<ArcIndexType>* permutation) {
  if (permutation != nullptr) {
    permutation->clear();
  }
}

template <typename NodeIndexType, typename ArcIndexType>
class ListGraph<NodeIndexType, ArcIndexType>::OutgoingArcIterator {
 public:
  OutgoingArcIterator(const ListGraph& graph, NodeIndexType node)
      : graph_(graph), index_(graph.start_[node]) {
    DCHECK(graph.IsNodeValid(node));
  }
  OutgoingArcIterator(const ListGraph& graph, NodeIndexType node,
                      ArcIndexType arc)
      : graph_(graph), index_(arc) {
    DCHECK(graph.IsNodeValid(node));
    DCHECK(arc == Base::kNilArc || graph.Tail(arc) == node);
  }
  bool Ok() const { return index_ != Base::kNilArc; }
  ArcIndexType Index() const { return index_; }
  void Next() {
    DCHECK(Ok());
    index_ = graph_.next_[index_];
  }

  DEFINE_STL_ITERATOR_FUNCTIONS(OutgoingArcIterator);

 private:
  const ListGraph& graph_;
  ArcIndexType index_;
};

template <typename NodeIndexType, typename ArcIndexType>
class ListGraph<NodeIndexType, ArcIndexType>::OutgoingHeadIterator {
 public:
  using iterator_category = std::input_iterator_tag;
  using difference_type = ptrdiff_t;
  using pointer = const NodeIndexType*;
  using reference = const NodeIndexType&;
  using value_type = NodeIndexType;

  OutgoingHeadIterator(const ListGraph& graph, NodeIndexType node)
      : graph_(graph), index_(graph.start_[node]) {
    DCHECK(graph.IsNodeValid(node));
  }
  OutgoingHeadIterator(const ListGraph& graph, NodeIndexType node,
                       ArcIndexType arc)
      : graph_(graph), index_(arc) {
    DCHECK(graph.IsNodeValid(node));
    DCHECK(arc == Base::kNilArc || graph.Tail(arc) == node);
  }
  bool Ok() const { return index_ != Base::kNilArc; }
  NodeIndexType Index() const { return graph_.Head(index_); }
  void Next() {
    DCHECK(Ok());
    index_ = graph_.next_[index_];
  }

  bool operator!=(
      const typename ListGraph<
          NodeIndexType, ArcIndexType>::OutgoingHeadIterator& other) const {
    return index_ != other.index_;
  }
  NodeIndexType operator*() const { return Index(); }
  void operator++() { Next(); }

 private:
  const ListGraph& graph_;
  ArcIndexType index_;
};

// StaticGraph implementation --------------------------------------------------

DEFINE_RANGE_BASED_ARC_ITERATION(StaticGraph, Outgoing, DirectArcLimit(node));

template <typename NodeIndexType, typename ArcIndexType>
BeginEndWrapper<NodeIndexType const*> StaticGraph<NodeIndexType, ArcIndexType>::
operator[](NodeIndexType node) const {
  return BeginEndWrapper<NodeIndexType const*>(
      head_.data() + start_[node], head_.data() + DirectArcLimit(node));
}

template <typename NodeIndexType, typename ArcIndexType>
ArcIndexType StaticGraph<NodeIndexType, ArcIndexType>::OutDegree(
    NodeIndexType node) const {
  return DirectArcLimit(node) - start_[node];
}

template <typename NodeIndexType, typename ArcIndexType>
void StaticGraph<NodeIndexType, ArcIndexType>::ReserveNodes(
    NodeIndexType bound) {
  Base::ReserveNodes(bound);
  if (bound <= num_nodes_) return;
  start_.reserve(bound);
}

template <typename NodeIndexType, typename ArcIndexType>
void StaticGraph<NodeIndexType, ArcIndexType>::ReserveArcs(ArcIndexType bound) {
  Base::ReserveArcs(bound);
  if (bound <= num_arcs_) return;
  head_.reserve(bound);
  tail_.reserve(bound);
}

template <typename NodeIndexType, typename ArcIndexType>
void StaticGraph<NodeIndexType, ArcIndexType>::AddNode(NodeIndexType node) {
  if (node < num_nodes_) return;
  DCHECK(!const_capacities_ || node < node_capacity_);
  num_nodes_ = node + 1;
  start_.resize(num_nodes_, 0);
}

template <typename NodeIndexType, typename ArcIndexType>
ArcIndexType StaticGraph<NodeIndexType, ArcIndexType>::AddArc(
    NodeIndexType tail, NodeIndexType head) {
  DCHECK_GE(tail, 0);
  DCHECK_GE(head, 0);
  DCHECK(!is_built_);
  AddNode(tail > head ? tail : head);
  if (arc_in_order_) {
    if (tail >= last_tail_seen_) {
      start_[tail]++;
      last_tail_seen_ = tail;
    } else {
      arc_in_order_ = false;
    }
  }
  tail_.push_back(tail);
  head_.push_back(head);
  DCHECK(!const_capacities_ || num_arcs_ < arc_capacity_);
  return num_arcs_++;
}

template <typename NodeIndexType, typename ArcIndexType>
NodeIndexType StaticGraph<NodeIndexType, ArcIndexType>::Tail(
    ArcIndexType arc) const {
  DCHECK(IsArcValid(arc));
  return tail_[arc];
}

template <typename NodeIndexType, typename ArcIndexType>
NodeIndexType StaticGraph<NodeIndexType, ArcIndexType>::Head(
    ArcIndexType arc) const {
  DCHECK(IsArcValid(arc));
  return head_[arc];
}

// Implementation details: A reader may be surprised that we do many passes
// into the data where things could be done in one pass. For instance, during
// construction, we store the edges first, and then do a second pass at the
// end to compute the degree distribution.
//
// This is because it is a lot more efficient cache-wise to do it this way.
// This was determined by various experiments, but can also be understood:
// - during repetitive call to AddArc() a client usually accesses various
//   areas of memory, and there is no reason to polute the cache with
//   possibly random access to degree[i].
// - When the degrees are needed, we compute them in one go, maximizing the
//   chance of cache hit during the computation.
template <typename NodeIndexType, typename ArcIndexType>
void StaticGraph<NodeIndexType, ArcIndexType>::Build(
    std::vector<ArcIndexType>* permutation) {
  DCHECK(!is_built_);
  if (is_built_) return;
  is_built_ = true;
  node_capacity_ = num_nodes_;
  arc_capacity_ = num_arcs_;
  this->FreezeCapacities();

  // If Arc are in order, start_ already contains the degree distribution.
  if (arc_in_order_) {
    if (permutation != nullptr) {
      permutation->clear();
    }
    this->ComputeCumulativeSum(&start_);
    return;
  }

  // Computes outgoing degree of each nodes. We have to clear start_, since
  // at least the first arc was processed with arc_in_order_ == true.
  start_.assign(num_nodes_, 0);
  for (int i = 0; i < num_arcs_; ++i) {
    start_[tail_[i]]++;
  }
  this->ComputeCumulativeSum(&start_);

  // Computes the forward arc permutation.
  // Note that this temporarily alters the start_ vector.
  std::vector<ArcIndexType> perm(num_arcs_);
  for (int i = 0; i < num_arcs_; ++i) {
    perm[i] = start_[tail_[i]]++;
  }

  // We use "tail_" (which now contains rubbish) to permute "head_" faster.
  CHECK_EQ(tail_.size(), num_arcs_);
  tail_.swap(head_);
  for (int i = 0; i < num_arcs_; ++i) {
    head_[perm[i]] = tail_[i];
  }

  if (permutation != nullptr) {
    permutation->swap(perm);
  }

  // Restore in start_[i] the index of the first arc with tail >= i.
  for (int i = num_nodes_ - 1; i > 0; --i) {
    start_[i] = start_[i - 1];
  }
  start_[0] = 0;

  // Recompute the correct tail_ vector
  for (const NodeIndexType node : Base::AllNodes()) {
    for (const ArcIndexType arc : OutgoingArcs(node)) {
      tail_[arc] = node;
    }
  }
}

template <typename NodeIndexType, typename ArcIndexType>
class StaticGraph<NodeIndexType, ArcIndexType>::OutgoingArcIterator {
 public:
  OutgoingArcIterator(const StaticGraph& graph, NodeIndexType node)
      : index_(graph.start_[node]), limit_(graph.DirectArcLimit(node)) {}
  OutgoingArcIterator(const StaticGraph& graph, NodeIndexType node,
                      ArcIndexType arc)
      : index_(arc), limit_(graph.DirectArcLimit(node)) {
    DCHECK_GE(arc, graph.start_[node]);
  }

  bool Ok() const { return index_ < limit_; }
  ArcIndexType Index() const { return index_; }
  void Next() {
    DCHECK(Ok());
    index_++;
  }

  // Note(user): we lose a bit by returning a BeginEndWrapper<> on top of
  // this iterator rather than a simple IntegerRange<> on the arc indices.
  // On my computer: around 420M arcs/sec instead of 440M arcs/sec.
  //
  // However, it is slightly more consistent to do it this way, and we don't
  // have two different codes depending on the way a client iterates on the
  // arcs.
  DEFINE_STL_ITERATOR_FUNCTIONS(OutgoingArcIterator);

 private:
  ArcIndexType index_;
  const ArcIndexType limit_;
};

// ReverseArcListGraph implementation ------------------------------------------

DEFINE_RANGE_BASED_ARC_ITERATION(ReverseArcListGraph, Outgoing, Base::kNilArc);
DEFINE_RANGE_BASED_ARC_ITERATION(ReverseArcListGraph, Incoming, Base::kNilArc);
DEFINE_RANGE_BASED_ARC_ITERATION(ReverseArcListGraph,
                                 OutgoingOrOppositeIncoming, Base::kNilArc);

template <typename NodeIndexType, typename ArcIndexType>
BeginEndWrapper<typename ReverseArcListGraph<
    NodeIndexType, ArcIndexType>::OutgoingHeadIterator>
    ReverseArcListGraph<NodeIndexType, ArcIndexType>::operator[](
        NodeIndexType node) const {
  return BeginEndWrapper<OutgoingHeadIterator>(
      OutgoingHeadIterator(*this, node),
      OutgoingHeadIterator(*this, node, Base::kNilArc));
}

template <typename NodeIndexType, typename ArcIndexType>
ArcIndexType ReverseArcListGraph<NodeIndexType, ArcIndexType>::OutDegree(
    NodeIndexType node) const {
  ArcIndexType degree(0);
  for (auto arc ATTRIBUTE_UNUSED : OutgoingArcs(node)) ++degree;
  return degree;
}

template <typename NodeIndexType, typename ArcIndexType>
ArcIndexType ReverseArcListGraph<NodeIndexType, ArcIndexType>::InDegree(
    NodeIndexType node) const {
  ArcIndexType degree(0);
  for (auto arc ATTRIBUTE_UNUSED : IncomingArcs(node)) ++degree;
  return degree;
}

template <typename NodeIndexType, typename ArcIndexType>
ArcIndexType ReverseArcListGraph<NodeIndexType, ArcIndexType>::OppositeArc(
    ArcIndexType arc) const {
  DCHECK(IsArcValid(arc));
  return ~arc;
}

template <typename NodeIndexType, typename ArcIndexType>
NodeIndexType ReverseArcListGraph<NodeIndexType, ArcIndexType>::Head(
    ArcIndexType arc) const {
  DCHECK(IsArcValid(arc));
  return head_[arc];
}

template <typename NodeIndexType, typename ArcIndexType>
NodeIndexType ReverseArcListGraph<NodeIndexType, ArcIndexType>::Tail(
    ArcIndexType arc) const {
  return head_[OppositeArc(arc)];
}

template <typename NodeIndexType, typename ArcIndexType>
void ReverseArcListGraph<NodeIndexType, ArcIndexType>::ReserveNodes(
    NodeIndexType bound) {
  Base::ReserveNodes(bound);
  if (bound <= num_nodes_) return;
  start_.reserve(bound);
  reverse_start_.reserve(bound);
}

template <typename NodeIndexType, typename ArcIndexType>
void ReverseArcListGraph<NodeIndexType, ArcIndexType>::ReserveArcs(
    ArcIndexType bound) {
  Base::ReserveArcs(bound);
  if (bound <= num_arcs_) return;
  head_.reserve(bound);
  next_.reserve(bound);
}

template <typename NodeIndexType, typename ArcIndexType>
void ReverseArcListGraph<NodeIndexType, ArcIndexType>::AddNode(
    NodeIndexType node) {
  if (node < num_nodes_) return;
  DCHECK(!const_capacities_ || node < node_capacity_);
  num_nodes_ = node + 1;
  start_.resize(num_nodes_, Base::kNilArc);
  reverse_start_.resize(num_nodes_, Base::kNilArc);
}

template <typename NodeIndexType, typename ArcIndexType>
ArcIndexType ReverseArcListGraph<NodeIndexType, ArcIndexType>::AddArc(
    NodeIndexType tail, NodeIndexType head) {
  DCHECK_GE(tail, 0);
  DCHECK_GE(head, 0);
  AddNode(tail > head ? tail : head);
  head_.grow(tail, head);
  next_.grow(reverse_start_[head], start_[tail]);
  start_[tail] = num_arcs_;
  reverse_start_[head] = ~num_arcs_;
  DCHECK(!const_capacities_ || num_arcs_ < arc_capacity_);
  return num_arcs_++;
}

template <typename NodeIndexType, typename ArcIndexType>
void ReverseArcListGraph<NodeIndexType, ArcIndexType>::Build(
    std::vector<ArcIndexType>* permutation) {
  if (permutation != nullptr) {
    permutation->clear();
  }
}

template <typename NodeIndexType, typename ArcIndexType>
class ReverseArcListGraph<NodeIndexType, ArcIndexType>::OutgoingArcIterator {
 public:
  OutgoingArcIterator(const ReverseArcListGraph& graph, NodeIndexType node)
      : graph_(graph), index_(graph.start_[node]) {
    DCHECK(graph.IsNodeValid(node));
  }
  OutgoingArcIterator(const ReverseArcListGraph& graph, NodeIndexType node,
                      ArcIndexType arc)
      : graph_(graph), index_(arc) {
    DCHECK(graph.IsNodeValid(node));
    DCHECK(arc == Base::kNilArc || arc >= 0);
    DCHECK(arc == Base::kNilArc || graph.Tail(arc) == node);
  }
  bool Ok() const { return index_ != Base::kNilArc; }
  ArcIndexType Index() const { return index_; }
  void Next() {
    DCHECK(Ok());
    index_ = graph_.next_[index_];
  }

  DEFINE_STL_ITERATOR_FUNCTIONS(OutgoingArcIterator);

 private:
  const ReverseArcListGraph& graph_;
  ArcIndexType index_;
};

template <typename NodeIndexType, typename ArcIndexType>
class ReverseArcListGraph<NodeIndexType, ArcIndexType>::IncomingArcIterator {
 public:
  IncomingArcIterator(const ReverseArcListGraph& graph, NodeIndexType node)
      : graph_(graph), index_(graph.reverse_start_[node]) {
    DCHECK(graph.IsNodeValid(node));
  }
  IncomingArcIterator(const ReverseArcListGraph& graph, NodeIndexType node,
                      ArcIndexType arc)
      : graph_(graph),
        index_(arc == Base::kNilArc ? Base::kNilArc : graph.OppositeArc(arc)) {
    DCHECK(graph.IsNodeValid(node));
    DCHECK(arc == Base::kNilArc || arc >= 0);
    DCHECK(arc == Base::kNilArc || graph.Head(arc) == node);
  }

  bool Ok() const { return index_ != Base::kNilArc; }
  ArcIndexType Index() const {
    return index_ == Base::kNilArc ? Base::kNilArc : graph_.OppositeArc(index_);
  }
  void Next() {
    DCHECK(Ok());
    index_ = graph_.next_[index_];
  }

  DEFINE_STL_ITERATOR_FUNCTIONS(IncomingArcIterator);

 private:
  const ReverseArcListGraph& graph_;
  ArcIndexType index_;
};

template <typename NodeIndexType, typename ArcIndexType>
class ReverseArcListGraph<NodeIndexType,
                          ArcIndexType>::OutgoingOrOppositeIncomingArcIterator {
 public:
  OutgoingOrOppositeIncomingArcIterator(const ReverseArcListGraph& graph,
                                        NodeIndexType node)
      : graph_(graph), index_(graph.reverse_start_[node]), node_(node) {
    DCHECK(graph.IsNodeValid(node));
    if (index_ == Base::kNilArc) index_ = graph.start_[node];
  }
  OutgoingOrOppositeIncomingArcIterator(const ReverseArcListGraph& graph,
                                        NodeIndexType node, ArcIndexType arc)
      : graph_(graph), index_(arc), node_(node) {
    DCHECK(graph.IsNodeValid(node));
    DCHECK(arc == Base::kNilArc || graph.Tail(arc) == node);
  }

  bool Ok() const { return index_ != Base::kNilArc; }
  ArcIndexType Index() const { return index_; }
  void Next() {
    DCHECK(Ok());
    if (index_ < 0) {
      index_ = graph_.next_[index_];
      if (index_ == Base::kNilArc) {
        index_ = graph_.start_[node_];
      }
    } else {
      index_ = graph_.next_[index_];
    }
  }

  DEFINE_STL_ITERATOR_FUNCTIONS(OutgoingOrOppositeIncomingArcIterator);

 private:
  const ReverseArcListGraph& graph_;
  ArcIndexType index_;
  const NodeIndexType node_;
};

template <typename NodeIndexType, typename ArcIndexType>
class ReverseArcListGraph<NodeIndexType, ArcIndexType>::OutgoingHeadIterator {
 public:
  OutgoingHeadIterator(const ReverseArcListGraph& graph, NodeIndexType node)
      : graph_(&graph), index_(graph.start_[node]) {
    DCHECK(graph.IsNodeValid(node));
  }
  OutgoingHeadIterator(const ReverseArcListGraph& graph, NodeIndexType node,
                       ArcIndexType arc)
      : graph_(&graph), index_(arc) {
    DCHECK(graph.IsNodeValid(node));
    DCHECK(arc == Base::kNilArc || arc >= 0);
    DCHECK(arc == Base::kNilArc || graph.Tail(arc) == node);
  }
  bool Ok() const { return index_ != Base::kNilArc; }
  ArcIndexType Index() const { return graph_->Head(index_); }
  void Next() {
    DCHECK(Ok());
    index_ = graph_->next_[index_];
  }

  DEFINE_STL_ITERATOR_FUNCTIONS(OutgoingHeadIterator);

 private:
  const ReverseArcListGraph* graph_;
  ArcIndexType index_;
};

// ReverseArcStaticGraph implementation ----------------------------------------

DEFINE_RANGE_BASED_ARC_ITERATION(ReverseArcStaticGraph, Outgoing,
                                 DirectArcLimit(node));
DEFINE_RANGE_BASED_ARC_ITERATION(ReverseArcStaticGraph, Incoming,
                                 ReverseArcLimit(node));
DEFINE_RANGE_BASED_ARC_ITERATION(ReverseArcStaticGraph,
                                 OutgoingOrOppositeIncoming,
                                 DirectArcLimit(node));

template <typename NodeIndexType, typename ArcIndexType>
ArcIndexType ReverseArcStaticGraph<NodeIndexType, ArcIndexType>::OutDegree(
    NodeIndexType node) const {
  return DirectArcLimit(node) - start_[node];
}

template <typename NodeIndexType, typename ArcIndexType>
ArcIndexType ReverseArcStaticGraph<NodeIndexType, ArcIndexType>::InDegree(
    NodeIndexType node) const {
  return ReverseArcLimit(node) - reverse_start_[node];
}

template <typename NodeIndexType, typename ArcIndexType>
BeginEndWrapper<NodeIndexType const*> ReverseArcStaticGraph<
    NodeIndexType, ArcIndexType>::operator[](NodeIndexType node) const {
  return BeginEndWrapper<NodeIndexType const*>(
      head_.data() + start_[node], head_.data() + DirectArcLimit(node));
}

template <typename NodeIndexType, typename ArcIndexType>
ArcIndexType ReverseArcStaticGraph<NodeIndexType, ArcIndexType>::OppositeArc(
    ArcIndexType arc) const {
  DCHECK(is_built_);
  DCHECK(IsArcValid(arc));
  return opposite_[arc];
}

template <typename NodeIndexType, typename ArcIndexType>
NodeIndexType ReverseArcStaticGraph<NodeIndexType, ArcIndexType>::Head(
    ArcIndexType arc) const {
  DCHECK(is_built_);
  DCHECK(IsArcValid(arc));
  return head_[arc];
}

template <typename NodeIndexType, typename ArcIndexType>
NodeIndexType ReverseArcStaticGraph<NodeIndexType, ArcIndexType>::Tail(
    ArcIndexType arc) const {
  DCHECK(is_built_);
  return head_[OppositeArc(arc)];
}

template <typename NodeIndexType, typename ArcIndexType>
void ReverseArcStaticGraph<NodeIndexType, ArcIndexType>::ReserveArcs(
    ArcIndexType bound) {
  Base::ReserveArcs(bound);
  if (bound <= num_arcs_) return;
  head_.reserve(bound);
}

template <typename NodeIndexType, typename ArcIndexType>
void ReverseArcStaticGraph<NodeIndexType, ArcIndexType>::AddNode(
    NodeIndexType node) {
  if (node < num_nodes_) return;
  DCHECK(!const_capacities_ || node < node_capacity_);
  num_nodes_ = node + 1;
}

template <typename NodeIndexType, typename ArcIndexType>
ArcIndexType ReverseArcStaticGraph<NodeIndexType, ArcIndexType>::AddArc(
    NodeIndexType tail, NodeIndexType head) {
  DCHECK_GE(tail, 0);
  DCHECK_GE(head, 0);
  AddNode(tail > head ? tail : head);

  // We inverse head and tail here because it is more convenient this way
  // during build time, see Build().
  head_.grow(head, tail);
  DCHECK(!const_capacities_ || num_arcs_ < arc_capacity_);
  return num_arcs_++;
}

template <typename NodeIndexType, typename ArcIndexType>
void ReverseArcStaticGraph<NodeIndexType, ArcIndexType>::Build(
    std::vector<ArcIndexType>* permutation) {
  DCHECK(!is_built_);
  if (is_built_) return;
  is_built_ = true;
  node_capacity_ = num_nodes_;
  arc_capacity_ = num_arcs_;
  this->FreezeCapacities();
  this->BuildStartAndForwardHead(&head_, &start_, permutation);

  // Computes incoming degree of each nodes.
  reverse_start_.assign(num_nodes_, 0);
  for (int i = 0; i < num_arcs_; ++i) {
    reverse_start_[head_[i]]++;
  }
  this->ComputeCumulativeSum(&reverse_start_);

  // Computes the reverse arcs of the forward arcs.
  // Note that this sort the reverse arcs with the same tail by head.
  opposite_.reserve(num_arcs_);
  for (int i = 0; i < num_arcs_; ++i) {
    // TODO(user): the 0 is wasted here, but minor optimisation.
    opposite_.grow(0, reverse_start_[head_[i]]++ - num_arcs_);
  }

  // Computes in reverse_start_ the start index of the reverse arcs.
  for (int i = num_nodes_ - 1; i > 0; --i) {
    reverse_start_[i] = reverse_start_[i - 1] - num_arcs_;
  }
  if (num_nodes_ != 0) {
    reverse_start_[0] = -num_arcs_;
  }

  // Fill reverse arc information.
  for (int i = 0; i < num_arcs_; ++i) {
    opposite_[opposite_[i]] = i;
  }
  for (const NodeIndexType node : Base::AllNodes()) {
    for (const ArcIndexType arc : OutgoingArcs(node)) {
      head_[opposite_[arc]] = node;
    }
  }
}

template <typename NodeIndexType, typename ArcIndexType>
class ReverseArcStaticGraph<NodeIndexType, ArcIndexType>::OutgoingArcIterator {
 public:
  OutgoingArcIterator(const ReverseArcStaticGraph& graph, NodeIndexType node)
      : index_(graph.start_[node]), limit_(graph.DirectArcLimit(node)) {}
  OutgoingArcIterator(const ReverseArcStaticGraph& graph, NodeIndexType node,
                      ArcIndexType arc)
      : index_(arc), limit_(graph.DirectArcLimit(node)) {
    DCHECK_GE(arc, graph.start_[node]);
  }

  bool Ok() const { return index_ < limit_; }
  ArcIndexType Index() const { return index_; }
  void Next() {
    DCHECK(Ok());
    index_++;
  }

  // TODO(user): we lose a bit by returning a BeginEndWrapper<> on top of this
  // iterator rather than a simple IntegerRange on the arc indices.
  DEFINE_STL_ITERATOR_FUNCTIONS(OutgoingArcIterator);

 private:
  ArcIndexType index_;
  const ArcIndexType limit_;
};

template <typename NodeIndexType, typename ArcIndexType>
class ReverseArcStaticGraph<NodeIndexType, ArcIndexType>::IncomingArcIterator {
 public:
  IncomingArcIterator(const ReverseArcStaticGraph& graph, NodeIndexType node)
      : graph_(graph),
        limit_(graph.ReverseArcLimit(node)),
        index_(graph.reverse_start_[node]) {
    DCHECK(graph.IsNodeValid(node));
    DCHECK_LE(index_, limit_);
  }
  IncomingArcIterator(const ReverseArcStaticGraph& graph, NodeIndexType node,
                      ArcIndexType arc)
      : graph_(graph),
        limit_(graph.ReverseArcLimit(node)),
        index_(arc == limit_ ? limit_ : graph_.OppositeArc(arc)) {
    DCHECK(graph.IsNodeValid(node));
    DCHECK_GE(index_, graph.reverse_start_[node]);
    DCHECK_LE(index_, limit_);
  }

  bool Ok() const { return index_ < limit_; }
  ArcIndexType Index() const {
    return index_ == limit_ ? limit_ : graph_.OppositeArc(index_);
  }
  void Next() {
    DCHECK(Ok());
    index_++;
  }

  DEFINE_STL_ITERATOR_FUNCTIONS(IncomingArcIterator);

 private:
  const ReverseArcStaticGraph& graph_;
  const ArcIndexType limit_;
  ArcIndexType index_;
};

template <typename NodeIndexType, typename ArcIndexType>
class ReverseArcStaticGraph<
    NodeIndexType, ArcIndexType>::OutgoingOrOppositeIncomingArcIterator {
 public:
  OutgoingOrOppositeIncomingArcIterator(const ReverseArcStaticGraph& graph,
                                        NodeIndexType node)
      : index_(graph.reverse_start_[node]),
        first_limit_(graph.ReverseArcLimit(node)),
        next_start_(graph.start_[node]),
        limit_(graph.DirectArcLimit(node)) {
    if (index_ == first_limit_) index_ = next_start_;
    DCHECK(graph.IsNodeValid(node));
    DCHECK((index_ < first_limit_) || (index_ >= next_start_));
  }
  OutgoingOrOppositeIncomingArcIterator(const ReverseArcStaticGraph& graph,
                                        NodeIndexType node, ArcIndexType arc)
      : index_(arc),
        first_limit_(graph.ReverseArcLimit(node)),
        next_start_(graph.start_[node]),
        limit_(graph.DirectArcLimit(node)) {
    DCHECK(graph.IsNodeValid(node));
    DCHECK((index_ >= graph.reverse_start_[node] && index_ < first_limit_) ||
           (index_ >= next_start_));
  }

  ArcIndexType Index() const { return index_; }
  bool Ok() const { return index_ < limit_; }
  void Next() {
    DCHECK(Ok());
    index_++;
    if (index_ == first_limit_) {
      index_ = next_start_;
    }
  }

  DEFINE_STL_ITERATOR_FUNCTIONS(OutgoingOrOppositeIncomingArcIterator);

 private:
  ArcIndexType index_;
  const ArcIndexType first_limit_;
  const ArcIndexType next_start_;
  const ArcIndexType limit_;
};

// ReverseArcMixedGraph implementation -----------------------------------------

DEFINE_RANGE_BASED_ARC_ITERATION(ReverseArcMixedGraph, Outgoing,
                                 DirectArcLimit(node));
DEFINE_RANGE_BASED_ARC_ITERATION(ReverseArcMixedGraph, Incoming, Base::kNilArc);
DEFINE_RANGE_BASED_ARC_ITERATION(ReverseArcMixedGraph,
                                 OutgoingOrOppositeIncoming,
                                 DirectArcLimit(node));

template <typename NodeIndexType, typename ArcIndexType>
ArcIndexType ReverseArcMixedGraph<NodeIndexType, ArcIndexType>::OutDegree(
    NodeIndexType node) const {
  return DirectArcLimit(node) - start_[node];
}

template <typename NodeIndexType, typename ArcIndexType>
ArcIndexType ReverseArcMixedGraph<NodeIndexType, ArcIndexType>::InDegree(
    NodeIndexType node) const {
  ArcIndexType degree(0);
  for (auto arc ATTRIBUTE_UNUSED : IncomingArcs(node)) ++degree;
  return degree;
}

template <typename NodeIndexType, typename ArcIndexType>
BeginEndWrapper<NodeIndexType const*> ReverseArcMixedGraph<
    NodeIndexType, ArcIndexType>::operator[](NodeIndexType node) const {
  return BeginEndWrapper<NodeIndexType const*>(
      head_.data() + start_[node], head_.data() + DirectArcLimit(node));
}

template <typename NodeIndexType, typename ArcIndexType>
ArcIndexType ReverseArcMixedGraph<NodeIndexType, ArcIndexType>::OppositeArc(
    ArcIndexType arc) const {
  DCHECK(IsArcValid(arc));
  return ~arc;
}

template <typename NodeIndexType, typename ArcIndexType>
NodeIndexType ReverseArcMixedGraph<NodeIndexType, ArcIndexType>::Head(
    ArcIndexType arc) const {
  DCHECK(is_built_);
  DCHECK(IsArcValid(arc));
  return head_[arc];
}

template <typename NodeIndexType, typename ArcIndexType>
NodeIndexType ReverseArcMixedGraph<NodeIndexType, ArcIndexType>::Tail(
    ArcIndexType arc) const {
  DCHECK(is_built_);
  return head_[OppositeArc(arc)];
}

template <typename NodeIndexType, typename ArcIndexType>
void ReverseArcMixedGraph<NodeIndexType, ArcIndexType>::ReserveArcs(
    ArcIndexType bound) {
  Base::ReserveArcs(bound);
  if (bound <= num_arcs_) return;
  head_.reserve(bound);
}

template <typename NodeIndexType, typename ArcIndexType>
void ReverseArcMixedGraph<NodeIndexType, ArcIndexType>::AddNode(
    NodeIndexType node) {
  if (node < num_nodes_) return;
  DCHECK(!const_capacities_ || node < node_capacity_);
  num_nodes_ = node + 1;
}

template <typename NodeIndexType, typename ArcIndexType>
ArcIndexType ReverseArcMixedGraph<NodeIndexType, ArcIndexType>::AddArc(
    NodeIndexType tail, NodeIndexType head) {
  DCHECK_GE(tail, 0);
  DCHECK_GE(head, 0);
  AddNode(tail > head ? tail : head);

  // We inverse head and tail here because it is more convenient this way
  // during build time, see Build().
  head_.grow(head, tail);
  DCHECK(!const_capacities_ || num_arcs_ < arc_capacity_);
  return num_arcs_++;
}

template <typename NodeIndexType, typename ArcIndexType>
void ReverseArcMixedGraph<NodeIndexType, ArcIndexType>::Build(
    std::vector<ArcIndexType>* permutation) {
  DCHECK(!is_built_);
  if (is_built_) return;
  is_built_ = true;
  node_capacity_ = num_nodes_;
  arc_capacity_ = num_arcs_;
  this->FreezeCapacities();
  this->BuildStartAndForwardHead(&head_, &start_, permutation);

  // Fill tails.
  for (const NodeIndexType node : Base::AllNodes()) {
    for (const ArcIndexType arc : OutgoingArcs(node)) {
      head_[~arc] = node;
    }
  }

  // Fill information for iterating over reverse arcs.
  reverse_start_.assign(num_nodes_, Base::kNilArc);
  next_.reserve(num_arcs_);
  for (const ArcIndexType arc : Base::AllForwardArcs()) {
    next_.push_back(reverse_start_[Head(arc)]);
    reverse_start_[Head(arc)] = -next_.size();
  }
}

template <typename NodeIndexType, typename ArcIndexType>
class ReverseArcMixedGraph<NodeIndexType, ArcIndexType>::OutgoingArcIterator {
 public:
  OutgoingArcIterator(const ReverseArcMixedGraph& graph, NodeIndexType node)
      : index_(graph.start_[node]), limit_(graph.DirectArcLimit(node)) {}
  OutgoingArcIterator(const ReverseArcMixedGraph& graph, NodeIndexType node,
                      ArcIndexType arc)
      : index_(arc), limit_(graph.DirectArcLimit(node)) {
    DCHECK_GE(arc, graph.start_[node]);
  }

  bool Ok() const { return index_ < limit_; }
  ArcIndexType Index() const { return index_; }
  void Next() {
    DCHECK(Ok());
    index_++;
  }

  // TODO(user): we lose a bit by returning a BeginEndWrapper<> on top of this
  // iterator rather than a simple IntegerRange on the arc indices.
  DEFINE_STL_ITERATOR_FUNCTIONS(OutgoingArcIterator);

 private:
  ArcIndexType index_;
  const ArcIndexType limit_;
};

template <typename NodeIndexType, typename ArcIndexType>
class ReverseArcMixedGraph<NodeIndexType, ArcIndexType>::IncomingArcIterator {
 public:
  IncomingArcIterator(const ReverseArcMixedGraph& graph, NodeIndexType node)
      : graph_(&graph) {
    DCHECK(graph.is_built_);
    DCHECK(graph.IsNodeValid(node));
    index_ = graph.reverse_start_[node];
  }
  IncomingArcIterator(const ReverseArcMixedGraph& graph, NodeIndexType node,
                      ArcIndexType arc)
      : graph_(&graph) {
    DCHECK(graph.is_built_);
    DCHECK(graph.IsNodeValid(node));
    DCHECK(arc == Base::kNilArc || arc >= 0);
    DCHECK(arc == Base::kNilArc || graph.Head(arc) == node);
    index_ = arc == Base::kNilArc ? arc : graph.OppositeArc(arc);
  }
  bool Ok() const { return index_ != Base::kNilArc; }
  ArcIndexType Index() const {
    return index_ == Base::kNilArc ? Base::kNilArc
                                   : graph_->OppositeArc(index_);
  }
  void Next() {
    DCHECK(Ok());
    index_ = graph_->next_[~index_];
  }

  DEFINE_STL_ITERATOR_FUNCTIONS(IncomingArcIterator);

 private:
  const ReverseArcMixedGraph* graph_;
  ArcIndexType index_;
};

template <typename NodeIndexType, typename ArcIndexType>
class ReverseArcMixedGraph<
    NodeIndexType, ArcIndexType>::OutgoingOrOppositeIncomingArcIterator {
 public:
  OutgoingOrOppositeIncomingArcIterator(const ReverseArcMixedGraph& graph,
                                        NodeIndexType node)
      : graph_(&graph) {
    limit_ = graph.DirectArcLimit(node);  // also DCHECKs node and is_built_.
    index_ = graph.reverse_start_[node];
    restart_ = graph.start_[node];
    if (index_ == Base::kNilArc) {
      index_ = restart_;
    }
  }
  OutgoingOrOppositeIncomingArcIterator(const ReverseArcMixedGraph& graph,
                                        NodeIndexType node, ArcIndexType arc)
      : graph_(&graph) {
    limit_ = graph.DirectArcLimit(node);
    index_ = arc;
    restart_ = graph.start_[node];
    DCHECK(arc == Base::kNilArc || arc == limit_ || graph.Tail(arc) == node);
  }
  bool Ok() const {
    // Note that we always have limit_ <= Base::kNilArc.
    return index_ < limit_;
  }
  ArcIndexType Index() const { return index_; }
  void Next() {
    DCHECK(Ok());
    if (index_ < 0) {
      index_ = graph_->next_[graph_->OppositeArc(index_)];
      if (index_ == Base::kNilArc) {
        index_ = restart_;
      }
    } else {
      index_++;
    }
  }

  DEFINE_STL_ITERATOR_FUNCTIONS(OutgoingOrOppositeIncomingArcIterator);

 private:
  const ReverseArcMixedGraph* graph_;
  ArcIndexType index_;
  ArcIndexType restart_;
  ArcIndexType limit_;
};

// CompleteGraph implementation ------------------------------------------------
// Nodes and arcs are implicit and not stored.

template <typename NodeIndexType = int32, typename ArcIndexType = int32>
class CompleteGraph : public BaseGraph<NodeIndexType, ArcIndexType, false> {
  typedef BaseGraph<NodeIndexType, ArcIndexType, false> Base;
  using Base::num_arcs_;
  using Base::num_nodes_;
  using Base::arc_capacity_;
  using Base::node_capacity_;
  using Base::const_capacities_;

 public:
  // Builds a complete graph with num_nodes nodes.
  explicit CompleteGraph(NodeIndexType num_nodes) {
    this->Reserve(num_nodes, num_nodes * num_nodes);
    this->FreezeCapacities();
    num_nodes_ = num_nodes;
    num_arcs_ = num_nodes * num_nodes;
  }

  NodeIndexType Head(ArcIndexType arc) const;
  NodeIndexType Tail(ArcIndexType arc) const;
  ArcIndexType OutDegree(NodeIndexType node) const;
  IntegerRange<ArcIndexType> OutgoingArcs(NodeIndexType node) const;
  IntegerRange<ArcIndexType> OutgoingArcsStartingFrom(NodeIndexType node,
                                                      ArcIndexType from) const;
  IntegerRange<NodeIndexType> operator[](NodeIndexType node) const;
};

template <typename NodeIndexType, typename ArcIndexType>
NodeIndexType CompleteGraph<NodeIndexType, ArcIndexType>::Head(
    ArcIndexType arc) const {
  DCHECK(this->IsArcValid(arc));
  return arc % num_nodes_;
}

template <typename NodeIndexType, typename ArcIndexType>
NodeIndexType CompleteGraph<NodeIndexType, ArcIndexType>::Tail(
    ArcIndexType arc) const {
  DCHECK(this->IsArcValid(arc));
  return arc / num_nodes_;
}

template <typename NodeIndexType, typename ArcIndexType>
ArcIndexType CompleteGraph<NodeIndexType, ArcIndexType>::OutDegree(
    NodeIndexType node) const {
  return num_nodes_;
}

template <typename NodeIndexType, typename ArcIndexType>
IntegerRange<ArcIndexType> CompleteGraph<
    NodeIndexType, ArcIndexType>::OutgoingArcs(NodeIndexType node) const {
  DCHECK_LT(node, num_nodes_);
  return IntegerRange<ArcIndexType>(
      static_cast<ArcIndexType>(num_nodes_) * node,
      static_cast<ArcIndexType>(num_nodes_) * (node + 1));
}

template <typename NodeIndexType, typename ArcIndexType>
IntegerRange<ArcIndexType>
CompleteGraph<NodeIndexType, ArcIndexType>::OutgoingArcsStartingFrom(
    NodeIndexType node, ArcIndexType from) const {
  DCHECK_LT(node, num_nodes_);
  return IntegerRange<ArcIndexType>(
      from, static_cast<ArcIndexType>(num_nodes_) * (node + 1));
}

template <typename NodeIndexType, typename ArcIndexType>
IntegerRange<NodeIndexType> CompleteGraph<NodeIndexType, ArcIndexType>::
operator[](NodeIndexType node) const {
  DCHECK_LT(node, num_nodes_);
  return IntegerRange<NodeIndexType>(0, num_nodes_);
}

// CompleteBipartiteGraph implementation ---------------------------------------
// Nodes and arcs are implicit and not stored.

template <typename NodeIndexType = int32, typename ArcIndexType = int32>
class CompleteBipartiteGraph
    : public BaseGraph<NodeIndexType, ArcIndexType, false> {
  typedef BaseGraph<NodeIndexType, ArcIndexType, false> Base;
  using Base::num_arcs_;
  using Base::num_nodes_;
  using Base::arc_capacity_;
  using Base::node_capacity_;
  using Base::const_capacities_;

 public:
  // Builds a complete bipartite graph from a set of left nodes to a set of
  // right nodes.
  // Indices of left nodes of the bipartite graph range from 0 to left_nodes-1;
  // indices of right nodes range from left_nodes to left_nodes+right_nodes-1.
  CompleteBipartiteGraph(NodeIndexType left_nodes, NodeIndexType right_nodes)
      : left_nodes_(left_nodes), right_nodes_(right_nodes) {
    this->Reserve(left_nodes + right_nodes, left_nodes * right_nodes);
    this->FreezeCapacities();
    num_nodes_ = left_nodes + right_nodes;
    num_arcs_ = left_nodes * right_nodes;
  }

  NodeIndexType Head(ArcIndexType arc) const;
  NodeIndexType Tail(ArcIndexType arc) const;
  ArcIndexType OutDegree(NodeIndexType node) const;
  IntegerRange<ArcIndexType> OutgoingArcs(NodeIndexType node) const;
  IntegerRange<ArcIndexType> OutgoingArcsStartingFrom(NodeIndexType node,
                                                      ArcIndexType from) const;
  IntegerRange<NodeIndexType> operator[](NodeIndexType node) const;

  // Deprecated interface.
  class OutgoingArcIterator {
   public:
    OutgoingArcIterator(const CompleteBipartiteGraph& graph, NodeIndexType node)
        : index_(graph.right_nodes_ * node),
          limit_(node >= graph.left_nodes_ ? index_
                                           : graph.right_nodes_ * (node + 1)) {}

    bool Ok() const { return index_ < limit_; }
    ArcIndexType Index() const { return index_; }
    void Next() { index_++; }

   private:
    ArcIndexType index_;
    const ArcIndexType limit_;
  };

 private:
  const NodeIndexType left_nodes_;
  const NodeIndexType right_nodes_;
};

template <typename NodeIndexType, typename ArcIndexType>
NodeIndexType CompleteBipartiteGraph<NodeIndexType, ArcIndexType>::Head(
    ArcIndexType arc) const {
  DCHECK(this->IsArcValid(arc));
  return left_nodes_ + arc % right_nodes_;
}

template <typename NodeIndexType, typename ArcIndexType>
NodeIndexType CompleteBipartiteGraph<NodeIndexType, ArcIndexType>::Tail(
    ArcIndexType arc) const {
  DCHECK(this->IsArcValid(arc));
  return arc / right_nodes_;
}

template <typename NodeIndexType, typename ArcIndexType>
ArcIndexType CompleteBipartiteGraph<NodeIndexType, ArcIndexType>::OutDegree(
    NodeIndexType node) const {
  return (node < left_nodes_) ? right_nodes_ : 0;
}

template <typename NodeIndexType, typename ArcIndexType>
IntegerRange<ArcIndexType> CompleteBipartiteGraph<
    NodeIndexType, ArcIndexType>::OutgoingArcs(NodeIndexType node) const {
  if (node < left_nodes_) {
    return IntegerRange<ArcIndexType>(right_nodes_ * node,
                                      right_nodes_ * (node + 1));
  } else {
    return IntegerRange<ArcIndexType>(0, 0);
  }
}

template <typename NodeIndexType, typename ArcIndexType>
IntegerRange<ArcIndexType>
CompleteBipartiteGraph<NodeIndexType, ArcIndexType>::OutgoingArcsStartingFrom(
    NodeIndexType node, ArcIndexType from) const {
  if (node < left_nodes_) {
    return IntegerRange<ArcIndexType>(from, right_nodes_ * (node + 1));
  } else {
    return IntegerRange<ArcIndexType>(0, 0);
  }
}

template <typename NodeIndexType, typename ArcIndexType>
IntegerRange<NodeIndexType> CompleteBipartiteGraph<
    NodeIndexType, ArcIndexType>::operator[](NodeIndexType node) const {
  if (node < left_nodes_) {
    return IntegerRange<NodeIndexType>(left_nodes_, left_nodes_ + right_nodes_);
  } else {
    return IntegerRange<NodeIndexType>(0, 0);
  }
}

// Defining the simplest Graph interface as Graph for convenience.
typedef ListGraph<> Graph;

}  // namespace util

#undef DEFINE_RANGE_BASED_ARC_ITERATION
#undef DEFINE_STL_ITERATOR_FUNCTIONS

#endif  // UTIL_GRAPH_GRAPH_H_
