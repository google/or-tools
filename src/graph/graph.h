// Copyright 2010-2012 Google
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
// time. Its design is based on the experience acquired by the Operational
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
//   - ListGraph<> for the simplest api
//   - StaticGraph<> for performance, but require calling Build(), see below
//   - ReverseArcListGraph<> to add reverse arcs to ListGraph<>
//   - ReverseArcStaticGraph<> to add reverse arcs to StaticGraph<>
//   - ReverseArcMixedGraph<> for a smaller memory footprint
//
// Construction performance & memory footprint for a graph on 10M nodes and
// 50M arcs, run:
//   graph:graph_test --benchmarks=all
//   --benchmark_min_iters=1 --heap_check= --benchmark_memory_usage
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
//   for (Graph::NodeIndex node = 0; node < graph.num_nodes(); ++node) {
//     for (Graph::OutgoingArcIterator it(graph, node); it.Ok(); it.Next()) {
//       head = graph.Head(it.Index());
//       tail = node;  // or graph.Tail(it.Index()) which is fast but take extra
//                     // memory on the implementations with no reverse arcs.
//     }
//   }
//
// If you know the graph size in advance, this already set the number of nodes,
// reserve space for the arcs and check in DEBUG mode that you don't go over the
// bounds:
//   Graph graph(num_nodes, arc_capacity);
//
// Storing and using node annotations:
//   std::vector<bool> is_visited(graph.num_nodes(), false);
//   ...
//   for (Graph::NodeIndex node = 0; node < graph.num_nodes(); ++node) {
//     if (!is_visited[node]) ...
//   }
//
// Storing and using arc annotations:
//   std::vector<int> weigths;
//   for (...) {
//     graph.AddArc(tail, head);
//     weigths.push_back(arc_weight);
//   }
//   ...
//   arc = it.Index();
//   weigths[arc];
//
// More efficient version:
//   typedef StaticGraph<> Graph;
//   Graph graph(num_nodes, arc_capacity);  // Optional, but help memory usage.
//   std::vector<int> weigths;
//   weigth.reserve(arc_capacity);  // Optional, but help memory usage.
//   for (...) {
//     graph.AddArc(tail, head);
//     weigths.push_back(arc_weight);
//   }
//   ...
//   std::vector<Graph::ArcIndex> permutation;
//   graph.Build(&permutation);  // A static graph must be Build() before usage.
//   Permute(permutation, &weigths);  // Build() may permute the arc index.
//   ...
//
// Encoding an undirected graph:
//   typedef ReverseArc... Graph;
//   Graph graph;
//   for (...) {
//     graph.AddArc(tail, head);  // or graph.AddArc(head, tail) but not both.
//     edge_annotations.push_back(value);
//   }
//   ...
//   for (Graph::NodeIndex node = 0; node < graph.num_nodes(); ++node) {
//     for (Graph::IncidentArcIterator it(graph, node); it.Ok(); it.Next()) {
//       // We will encounter twice the same edge, once when node == tail and
//       // once when node == head. Both share the same annotation.
//       const Graph::ArcIndex arc = it.Index();
//       destination = graph.Head(arc);
//       annotation = edge_annotations[arc < 0 ? graph.OppositeArc(arc) : arc];
//     }
//   }
//
// Note: The graphs are primarly designed to be constructed first and then used
// because it covers most of the use cases. It is possible to extend the
// interface with more dynamicity (like removing arcs), but this is not done at
// this point. Note that a "dynamic" implementation will break some assumptions
// we make on what node or arc are valid and also on the indices returned by
// AddArc().
// Some arguments for simplifying the interface at the cost of dynamicity are:
// - It is always possible to construct a static graph from a dynamic one
//   before calling a complex algo.
// - If you really need a dynamic graph, maybe it is better to compute a graph
//   property incrementaly rather than calling an algorithm that start from
//   scratch each time.

#ifndef OR_TOOLS_GRAPH_GRAPH_H_
#define OR_TOOLS_GRAPH_GRAPH_H_

#include <stddef.h>
#include <stdlib.h>
#include <limits>
#include <new>
#include <vector>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"


namespace operations_research {

// Forward declaration.
template<typename T> class SVector;

// Base class of all Graphs implemented here. The default value for the graph
// index types is int32 since allmost all graphs that fit into memory do not
// need bigger indices.
//
// Note: The type can be unsigned, except for the graphs with reverse arcs
// where the ArcIndexType must be signed, but not necessarly the NodeIndexType.
template<typename NodeIndexType = int32, typename ArcIndexType = int32,
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

  // Returns true if the given node is a valid node of the graph.
  bool IsNodeValid(NodeIndexType node) const {
    return node >= 0 && node < num_nodes_;
  }

  // Returns true if the given arc is a valid arc of the graph.
  // Note that the arc validity range changes for graph with reverse arcs.
  bool IsArcValid(ArcIndexType arc) const {
    return (HasReverseArcs ? - num_arcs_ : 0) <= arc && arc < num_arcs_;
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
  virtual void ReserveNodes(NodeIndexType bound) = 0;
  virtual void ReserveArcs(ArcIndexType bound) = 0;
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

 protected:
  // Functions commented when defined because they are implementation details.
  void ComputeCumulativeSum(std::vector<ArcIndexType> *v);
  void BuildStartAndForwardHead(
      SVector<NodeIndexType> *head, std::vector<ArcIndexType> *start,
      std::vector<ArcIndexType> *permutation);
  class BaseStaticArcIterator;

  NodeIndexType num_nodes_;
  NodeIndexType node_capacity_;
  ArcIndexType num_arcs_;
  ArcIndexType arc_capacity_;
  bool const_capacities_;
};

// Dummy class to document the interface to iterate over arcs "of" a given node.
// See OutgoingArcIterator, IncomingArcIterator and IncidentArcIterator below.
template<typename NodeIndexType, typename ArcIndexType, typename GraphType>
class ArcIterator {
  // Creates an iterator on a given node, starting either at its first arc,
  // or at the given (absolute, not relative to the node) arc index.
  ArcIterator(const GraphType &graph, NodeIndexType node);
  ArcIterator(const GraphType &graph, NodeIndexType node, ArcIndexType arc);

  // Returns true if the iterator is at a valid arc "of" the node.
  bool Ok() const;

  // Current (absolute, not relative to the node) arc index.
  ArcIndexType Index() const;

  // Iterate to the next arc. Undefined behavior if Ok() is false.
  void Next();
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
template<typename NodeIndexType = int32,
         typename ArcIndexType = int32> class ListGraph
    : public BaseGraph<NodeIndexType, ArcIndexType, false> {
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
  void Build() { Build(NULL); }
  void Build(std::vector<ArcIndexType> *permutation);

  // Iterator over the forward arcs that verify Tail(arc) == node.
  class OutgoingArcIterator;

  // Returns the head of a valid arc.
  NodeIndexType Head(ArcIndexType arc) const;

  // Returns the tail of a valid arc.
  //
  // Note that for non-Reverse graphs, calling Tail() uses more memory. The
  // first time Tail() is called, we lazily initialize the tail_ array. If a
  // client prefers to initialize the array before (to mesure an algo timing
  // for instance) it is possible to call BuildTailArray(), see below.
  NodeIndexType Tail(ArcIndexType arc) const;

  // Build the tail array, this can be optionally called to initialize a
  // possible structure for subsequent Tail() calls.
  void BuildTailArray();

  // Clear the tail array, this can be used on the forward graph implementation
  // to free some memory if you know that no more calls to Tail() are needed.
  void FreeTailArray();

  virtual void ReserveNodes(NodeIndexType bound);
  virtual void ReserveArcs(ArcIndexType bound);

 private:
  std::vector<ArcIndexType> start_;
  std::vector<ArcIndexType> next_;
  std::vector<NodeIndexType> head_;
  mutable std::vector<NodeIndexType> tail_;
  DISALLOW_COPY_AND_ASSIGN(ListGraph);
};

// Most efficient implementation of a graph without reverse arcs:
// - Build() needs to be called after the arc and node have been added.
// - The graph is really compact memory wise:
//   ArcIndexType * node_capacity() + NodeIndexType * arc_capacity(),
//   but it needs more space during construction:
//   + (ArcIndexType + NodeIndexType) * arc_capacity().
// - The construction is really fast and needs no extra memory if the arcs are
//   given ordered by ascending tail!!
// - The Tail() is efficient but needs an extra NodeIndexType per arc.
template<typename NodeIndexType = int32,
         typename ArcIndexType = int32> class StaticGraph
    : public BaseGraph<NodeIndexType, ArcIndexType, false> {
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

  class OutgoingArcIterator;

  NodeIndexType Head(ArcIndexType arc) const;
  NodeIndexType Tail(ArcIndexType arc) const;

  virtual void ReserveNodes(NodeIndexType bound);
  virtual void ReserveArcs(ArcIndexType bound);
  void AddNode(NodeIndexType node);
  ArcIndexType AddArc(NodeIndexType tail, NodeIndexType head);

  void Build() { Build(NULL); }
  void Build(std::vector<ArcIndexType> *permutation);
  void BuildTailArray();
  void FreeTailArray();

 private:
  ArcIndexType DirectArcLimit(NodeIndexType node) const {
    return node + 1 < num_nodes_ ? start_[node + 1] : num_arcs_;
  }

  bool is_built_;
  bool arc_in_order_;
  NodeIndexType last_tail_seen_;
  std::vector<ArcIndexType> start_;
  std::vector<NodeIndexType> head_;
  mutable std::vector<NodeIndexType> tail_;
  DISALLOW_COPY_AND_ASSIGN(StaticGraph);
};

// Extends the ListGraph by also storing the reverse arcs.
// This class also documents the Graph interface related to reverse arc.
// - NodeIndexType can be unsigned, but ArcIndexType must be signed.
// - It has most of the same advantanges and inconvenients as ListGraph.
// - It takes 2 * ArcIndexType * node_capacity()
//   + 2 * (ArcIndexType + NodeIndexType) * arc_capacity() memory.
template<typename NodeIndexType = int32,
         typename ArcIndexType = int32> class ReverseArcListGraph
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

  // Iterates over the arcs (forward and reverse) that verify Tail(arc) == node.
  class IncidentArcIterator;

  // Iterates over the reverse arcs that verify Tail(arc) == node.
  class IncomingArcIterator;
  class OutgoingArcIterator;

  NodeIndexType Head(ArcIndexType arc) const;
  NodeIndexType Tail(ArcIndexType arc) const;

  virtual void ReserveNodes(NodeIndexType bound);
  virtual void ReserveArcs(ArcIndexType bound);
  void AddNode(NodeIndexType node);
  ArcIndexType AddArc(NodeIndexType tail, NodeIndexType head);

  void Build() { Build(NULL); }
  void Build(std::vector<ArcIndexType> *permutation);
  void BuildTailArray() {}
  void FreeTailArray() {}

 private:
  class BaseArcIterator;

  std::vector<ArcIndexType> start_;
  std::vector<ArcIndexType> reverse_start_;
  SVector<ArcIndexType> next_;
  SVector<NodeIndexType> head_;
  DISALLOW_COPY_AND_ASSIGN(ReverseArcListGraph);
};

// StaticGraph with reverse arc.
// - NodeIndexType can be unsigned, but ArcIndexType must be signed.
// - It has most of the same advantanges and inconvenients as StaticGraph.
// - It takes 2 * ArcIndexType * node_capacity()
//   + 2 * (ArcIndexType + NodeIndexType) * arc_capacity() memory.
// - If the ArcIndexPermutation is needed, then an extra ArcIndexType *
//   arc_capacity() is needed for it.
// - The reverse arcs from a node are sorted by head (so we could add a log()
//   time lookup function).
template<typename NodeIndexType = int32,
         typename ArcIndexType = int32> class ReverseArcStaticGraph
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

  ArcIndexType OppositeArc(ArcIndexType arc) const;
  class IncidentArcIterator;
  class IncomingArcIterator;
  class OutgoingArcIterator;

  NodeIndexType Head(ArcIndexType arc) const;
  NodeIndexType Tail(ArcIndexType arc) const;

  virtual void ReserveNodes(NodeIndexType bound);
  virtual void ReserveArcs(ArcIndexType bound);
  void AddNode(NodeIndexType node);
  ArcIndexType AddArc(NodeIndexType tail, NodeIndexType head);

  void Build() { Build(NULL); }
  void Build(std::vector<ArcIndexType> *permutation);
  void BuildTailArray() {}
  void FreeTailArray() {}

 private:
  ArcIndexType DirectArcLimit(NodeIndexType node) const {
    return node + 1 < num_nodes_ ? start_[node + 1] : num_arcs_;
  }
  ArcIndexType ReverseArcLimit(NodeIndexType node) const {
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
template<typename NodeIndexType = int32,
         typename ArcIndexType = int32> class ReverseArcMixedGraph
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

  ArcIndexType OppositeArc(ArcIndexType arc) const;
  class IncidentArcIterator;
  class IncomingArcIterator;
  class OutgoingArcIterator;

  NodeIndexType Head(ArcIndexType arc) const;
  NodeIndexType Tail(ArcIndexType arc) const;

  virtual void ReserveNodes(NodeIndexType bound);
  virtual void ReserveArcs(ArcIndexType bound);
  void AddNode(NodeIndexType node);
  ArcIndexType AddArc(NodeIndexType tail, NodeIndexType head);

  void Build() { Build(NULL); }
  void Build(std::vector<ArcIndexType> *permutation);
  void BuildTailArray() {}
  void FreeTailArray() {}

 private:
  ArcIndexType DirectArcLimit(NodeIndexType node) const {
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
template<class IntVector, class Array, class ElementType>
void PermuteWithExplicitElementType(
    const IntVector& permutation, Array* array_to_permute, ElementType unused) {
  std::vector<ElementType> temp(permutation.size());
  for (int i = 0; i < permutation.size(); ++i) {
    temp[i] = (*array_to_permute)[i];
  }
  for (int i = 0; i < permutation.size(); ++i) {
    (*array_to_permute)[permutation[i]] = temp[i];
  }
}

template<class IntVector, class Array>
void Permute(const IntVector& permutation, Array* array_to_permute) {
  if (permutation.size() == 0) {
    return;
  }
  PermuteWithExplicitElementType(
      permutation, array_to_permute, (*array_to_permute)[0]);
}

// A vector-like class where valid indices are in [- size_, size_) and reserved
// indices for future growth are in [- capacity_, capacity_). It is used to hold
// arc related information for graphs with reverse arcs.
//
// Sample usage:
//
// SVector<int> v;
// v.grow(left_value, right_value);
// v.resize(10);
// v.clear();
// v.swap(new_v);
// std:swap(v[i], v[~i]);
template<typename T>
class SVector {
 public:
  SVector() : base_(NULL), size_(0), capacity_(0) {}

  ~SVector() {
    clear();
    if (capacity_ > 0) {
      free(base_ - capacity_);
    }
  }

  T& operator[](int n) {
    DCHECK_LT(n, size_);
    DCHECK_GE(n, - size_);
    return base_[n];
  }

  const T& operator[](int n) const {
    DCHECK_LT(n, size_);
    DCHECK_GE(n, - size_);
    return base_[n];
  }

  void resize(int n, T left = T(), T right = T()) {
    reserve(n);
    for (int i = -n; i < - size_; ++i) {
      new(base_ + i) T(left);
    }
    for (int i = size_; i < n; ++i) {
      new(base_ + i) T(right);
    }
    for (int i = - size_; i < n; ++i) {
      base_[i].~T();
    }
    for (int i = n; i < size_; ++i) {
      base_[i].~T();
    }
    size_ = n;
  }

  void clear() {
    resize(0);
  }

  void swap(SVector<T> &x) {
    std::swap(base_, x.base_);
    std::swap(size_, x.size_);
    std::swap(capacity_, x.capacity_);
  }

  void reserve(int n) {
    DCHECK_GE(n, 0);
    DCHECK_LE(n, max_size());
    if (n > capacity_) {
      int new_capacity = n;
      int requested_block_size = 2 * new_capacity * sizeof(T);
      T *new_storage = static_cast<T*>(malloc(requested_block_size));
      CHECK(new_storage != NULL);
size_t block_size = requested_block_size;
      if (block_size > 0) {
        DCHECK_GE(block_size, requested_block_size);
        new_capacity = block_size / (2 * sizeof(T));
      }
      T *new_base = new_storage + new_capacity;
      for (int i = - size_; i < size_; ++i) {
        new(new_base + i) T(base_[i]);
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
      // We have to copy the elements because they are allowed to be element
      // of *this.
      T left_copy(left);
      T right_copy(right);
      reserve(NewCapacity(1));
      new(base_ + size_) T(right_copy);
      new(base_ - size_ - 1) T(left_copy);
      ++size_;
    } else {
      new(base_ + size_) T(right);
      new(base_ - size_ - 1) T(left);
      ++size_;
    }
  }

  int size() const {
    return size_;
  }

  int capacity() const {
    return capacity_;
  }

  int max_size() const {
    return std::numeric_limits<int>::max() / (2 * sizeof(T));
  }

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

  T *base_;  // Pointer to the element of index 0.
  int size_;  // Valid index are [- size_, size_).
  int capacity_;  // Reserved index are [- capacity_, capacity_).
  DISALLOW_COPY_AND_ASSIGN(SVector);
};

// BaseGraph definition ------------------------------------------------------

template<typename NodeIndexType, typename ArcIndexType, bool HasReverseArcs>
const NodeIndexType BaseGraph<NodeIndexType, ArcIndexType, HasReverseArcs>
    ::kNilNode = std::numeric_limits<NodeIndexType>::max();

template<typename NodeIndexType, typename ArcIndexType, bool HasReverseArcs>
const ArcIndexType BaseGraph<NodeIndexType, ArcIndexType, HasReverseArcs>
    ::kNilArc = std::numeric_limits<ArcIndexType>::max();

template<typename NodeIndexType, typename ArcIndexType, bool HasReverseArcs>
NodeIndexType BaseGraph<NodeIndexType, ArcIndexType, HasReverseArcs>
    ::node_capacity() const {
  // TODO(user): Is it needed? remove completely? return the real capacities
  // at the cost of having a different implementation for each graphs?
  return node_capacity_ > num_nodes_ ? node_capacity_ : num_nodes_;
}

template<typename NodeIndexType, typename ArcIndexType, bool HasReverseArcs>
ArcIndexType BaseGraph<NodeIndexType, ArcIndexType, HasReverseArcs>
    ::arc_capacity() const {
  // TODO(user): Same questions as the ones in node_capacity().
  return arc_capacity_ > num_arcs_ ? arc_capacity_ : num_arcs_;
}

template<typename NodeIndexType, typename ArcIndexType, bool HasReverseArcs>
void BaseGraph<NodeIndexType, ArcIndexType, HasReverseArcs>
    ::FreezeCapacities() {
  // TODO(user): Only define this in debug mode at the cost of having a lot
  // of indef NDEBUG all over the place? remove the function completely ?
  const_capacities_ = true;
  DCHECK_LE(num_nodes_, node_capacity_);
  DCHECK_LE(num_arcs_, arc_capacity_);
}

// Computes the cummulative sum of the entry in v. We only use it with
// in/out degree distribution, hence the Check() at the end.
template<typename NodeIndexType, typename ArcIndexType, bool HasReverseArcs>
void BaseGraph<NodeIndexType, ArcIndexType, HasReverseArcs>
    ::ComputeCumulativeSum(std::vector<ArcIndexType> *v) {
  ArcIndexType sum = 0;
  for (int i = 0; i < num_nodes_; ++i) {
    ArcIndexType temp = (*v)[i];
    (*v)[i] = sum;
    sum += temp;
  }
  DCHECK(sum == num_arcs_);
}

// Base class for StaticGraph arc iterator.
template<typename NodeIndexType, typename ArcIndexType, bool HasReverseArcs>
class BaseGraph<NodeIndexType, ArcIndexType, HasReverseArcs>
    ::BaseStaticArcIterator {
 public:
  BaseStaticArcIterator(ArcIndexType index, ArcIndexType limit)
      : index_(index), limit_(limit) {}

  bool Ok() const {
    return index_ < limit_;
  }

  ArcIndexType Index() const {
    return index_;
  }

  void Next() {
    DCHECK(Ok());
    index_++;
  }

 protected:
  ArcIndexType index_;
  ArcIndexType limit_;
};

// Given the tail of arc #i in (*head)[i] and the head of arc #i in (*head)[~i]
// - Reorder the arc by increasing tail.
// - Put the head of the new arc #i in (*head)[i].
// - Put in start[i] the index of the first arc with tail >= i.
// - Update "permutation" to reflect the change, unless it is NULL.
template<typename NodeIndexType, typename ArcIndexType, bool HasReverseArcs>
void BaseGraph<NodeIndexType, ArcIndexType, HasReverseArcs>
    ::BuildStartAndForwardHead(
        SVector<NodeIndexType> *head,
        std::vector<ArcIndexType> *start,
        std::vector<ArcIndexType> *permutation) {
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
    if (permutation != NULL) {
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
  if (permutation != NULL) {
    permutation->swap(perm);
  }
}

// ListGraph definition ------------------------------------------------------

template<typename NodeIndexType, typename ArcIndexType>
NodeIndexType ListGraph<NodeIndexType, ArcIndexType>
    ::Head(ArcIndexType arc) const {
  DCHECK(IsArcValid(arc));
  return head_[arc];
}

template<typename NodeIndexType, typename ArcIndexType>
void ListGraph<NodeIndexType, ArcIndexType>
    ::AddNode(NodeIndexType node) {
  if (node < num_nodes_) return;
  DCHECK(!const_capacities_ || node < node_capacity_);
  num_nodes_ = node + 1;
  start_.resize(num_nodes_, Base::kNilArc);
}

template<typename NodeIndexType, typename ArcIndexType>
ArcIndexType ListGraph<NodeIndexType, ArcIndexType>
    ::AddArc(NodeIndexType tail, NodeIndexType head) {
  AddNode(tail > head ? tail : head);
  head_.push_back(head);
  next_.push_back(start_[tail]);
  start_[tail] = num_arcs_;
  DCHECK(!const_capacities_ || num_arcs_ < arc_capacity_);
  return num_arcs_++;
}

template<typename NodeIndexType, typename ArcIndexType>
void ListGraph<NodeIndexType, ArcIndexType>
    ::ReserveNodes(NodeIndexType bound) {
  DCHECK(!const_capacities_);
  DCHECK_GE(bound, num_nodes_);
  if (bound <= num_nodes_) return;
  node_capacity_ = bound;
  start_.reserve(bound);
}

template<typename NodeIndexType, typename ArcIndexType>
void ListGraph<NodeIndexType, ArcIndexType>
    ::ReserveArcs(ArcIndexType bound) {
  DCHECK(!const_capacities_);
  DCHECK_GE(bound, num_arcs_);
  if (bound <= num_arcs_) return;
  arc_capacity_ = bound;
  head_.reserve(bound);
  next_.reserve(bound);
}

template<typename NodeIndexType, typename ArcIndexType>
void ListGraph<NodeIndexType, ArcIndexType>
    ::Build(std::vector<ArcIndexType> *permutation) {
  if (permutation != NULL) {
    permutation->clear();
  }
}

template<typename NodeIndexType, typename ArcIndexType>
NodeIndexType ListGraph<NodeIndexType, ArcIndexType>
    ::Tail(ArcIndexType arc) const {
  DCHECK(IsArcValid(arc));

  // Build the full tail array if needed.
  if (tail_.size() <= arc) {
    tail_.resize(num_arcs_);
    for (NodeIndexType i = 0; i < num_nodes_; ++i) {
      for (OutgoingArcIterator it(*this, i); it.Ok(); it.Next()) {
        tail_[it.Index()] = i;
      }
    }
  }
  return tail_[arc];
}

template<typename NodeIndexType, typename ArcIndexType>
void ListGraph<NodeIndexType, ArcIndexType>
    ::BuildTailArray() {
  Tail(num_arcs_ - 1);
}

template<typename NodeIndexType, typename ArcIndexType>
void ListGraph<NodeIndexType, ArcIndexType>
    ::FreeTailArray() {
  std::vector<NodeIndexType> tmp;
  tmp.swap(tail_);
}

template<typename NodeIndexType, typename ArcIndexType>
class ListGraph<NodeIndexType, ArcIndexType>::OutgoingArcIterator {
 public:
  OutgoingArcIterator(const ListGraph &graph, NodeIndexType node)
      : graph_(&graph), index_(graph.start_[node]) {
    DCHECK(graph.IsNodeValid(node));
  }
  OutgoingArcIterator(
      const ListGraph &graph, NodeIndexType node, ArcIndexType arc)
      : graph_(&graph), index_(arc) {
    DCHECK(graph.IsNodeValid(node));
    DCHECK(arc == Base::kNilArc || graph.Tail(arc) == node);
  }
  bool Ok() const {
    return index_ != Base::kNilArc;
  }
  ArcIndexType Index() const {
    return index_;
  }
  void Next() {
    DCHECK(Ok());
    index_ = graph_->next_[index_];
  }

 private:
  const ListGraph *graph_;
  ArcIndexType index_;
};

// StaticGraph definition ----------------------------------------------------

template<typename NodeIndexType, typename ArcIndexType>
void StaticGraph<NodeIndexType, ArcIndexType>
    ::ReserveNodes(NodeIndexType bound) {
  DCHECK(!const_capacities_);
  DCHECK_GE(bound, num_nodes_);
  if (bound <= num_nodes_) return;
  node_capacity_ = bound;
  start_.reserve(bound);
}

template<typename NodeIndexType, typename ArcIndexType>
void StaticGraph<NodeIndexType, ArcIndexType>
    ::ReserveArcs(ArcIndexType bound) {
  DCHECK(!const_capacities_);
  DCHECK_GE(bound, num_arcs_);
  if (bound <= num_arcs_) return;
  arc_capacity_ = bound;
  head_.reserve(bound);
  if (!arc_in_order_) {
    tail_.reserve(bound);
  }
}

template<typename NodeIndexType, typename ArcIndexType>
void StaticGraph<NodeIndexType, ArcIndexType>
    ::AddNode(NodeIndexType node) {
  if (node < num_nodes_) return;
  DCHECK(!const_capacities_ || node < node_capacity_);
  num_nodes_ = node + 1;
  start_.resize(num_nodes_, 0);
}

template<typename NodeIndexType, typename ArcIndexType>
ArcIndexType StaticGraph<NodeIndexType, ArcIndexType>
    ::AddArc(NodeIndexType tail, NodeIndexType head) {
  AddNode(tail > head ? tail : head);
  if (arc_in_order_) {
    if (tail >= last_tail_seen_) {
      start_[tail]++;
      last_tail_seen_ = tail;
    } else {
      arc_in_order_ = false;
      int sum = 0;
      tail_.reserve(arc_capacity_);
      for (int i = 0; i <= last_tail_seen_; ++i) {
        sum += start_[i];
        tail_.resize(sum, i);
      }
      DCHECK(sum == num_arcs_);
      tail_.push_back(tail);
    }
  } else {
    tail_.push_back(tail);
  }
  head_.push_back(head);
  DCHECK(!const_capacities_ || num_arcs_ < arc_capacity_);
  return num_arcs_++;
}

template<typename NodeIndexType, typename ArcIndexType>
NodeIndexType StaticGraph<NodeIndexType, ArcIndexType>
    ::Head(ArcIndexType arc) const {
  DCHECK(is_built_);
  DCHECK(IsArcValid(arc));
  return head_[arc];
}

template<typename NodeIndexType, typename ArcIndexType>
NodeIndexType StaticGraph<NodeIndexType, ArcIndexType>
    ::Tail(ArcIndexType arc) const {
  DCHECK(is_built_);
  DCHECK(IsArcValid(arc));

  // Build the full tail array if needed.
  if (tail_.size() <= arc) {
    tail_.resize(num_arcs_);
    for (NodeIndexType i = 0; i < num_nodes_; ++i) {
      for (OutgoingArcIterator it(*this, i); it.Ok(); it.Next()) {
        tail_[it.Index()] = i;
      }
    }
  }
  return tail_[arc];
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
template<typename NodeIndexType, typename ArcIndexType>
void StaticGraph<NodeIndexType, ArcIndexType>
    ::Build(std::vector<ArcIndexType> *permutation) {
  DCHECK(!is_built_);
  if (is_built_) return;
  is_built_ = true;
  node_capacity_ = num_nodes_;
  arc_capacity_ = num_arcs_;
  this->FreezeCapacities();

  // If Arc are in order, start_ already contains the degree distribution.
  if (arc_in_order_) {
    if (permutation != NULL) {
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

  // We do not need tail_ anymore, so we use it to permute head_ faster.
  tail_.swap(head_);
  for (int i = 0; i < num_arcs_; ++i) {
    head_[perm[i]] = tail_[i];
  }
  FreeTailArray();
  if (permutation != NULL) {
    permutation->swap(perm);
  }

  // Restore in start_[i] the index of the first arc with tail >= i.
  for (int i = num_nodes_ - 1; i > 0; --i) {
    start_[i] = start_[i - 1];
  }
  start_[0] = 0;
}

template<typename NodeIndexType, typename ArcIndexType>
void StaticGraph<NodeIndexType, ArcIndexType>
    ::BuildTailArray() {
  DCHECK(is_built_);
  Tail(num_arcs_ - 1);
}

template<typename NodeIndexType, typename ArcIndexType>
void StaticGraph<NodeIndexType, ArcIndexType>
    ::FreeTailArray() {
  DCHECK(is_built_);
  std::vector<NodeIndexType> tmp;
  tmp.swap(tail_);
}

template<typename NodeIndexType, typename ArcIndexType>
class StaticGraph<NodeIndexType, ArcIndexType>
    ::OutgoingArcIterator : public Base::BaseStaticArcIterator {
 public:
  OutgoingArcIterator(const StaticGraph &graph, NodeIndexType node)
      : Base::BaseStaticArcIterator(
          graph.start_[node], graph.DirectArcLimit(node)) {
    DCHECK(graph.is_built_);
    DCHECK(graph.IsNodeValid(node));
  }
  OutgoingArcIterator(
      const StaticGraph &graph, NodeIndexType node, ArcIndexType arc)
      : Base::BaseStaticArcIterator(arc, graph.DirectArcLimit(node)) {
    DCHECK(graph.is_built_);
    DCHECK(graph.IsNodeValid(node));
    DCHECK_GE(arc, graph.start_[node]);
  }
};

// ReverseArcListGraph definition --------------------------------------------

template<typename NodeIndexType, typename ArcIndexType>
ArcIndexType ReverseArcListGraph<NodeIndexType, ArcIndexType>
    ::OppositeArc(ArcIndexType arc) const {
  DCHECK(IsArcValid(arc));
  return ~arc;
}

template<typename NodeIndexType, typename ArcIndexType>
NodeIndexType ReverseArcListGraph<NodeIndexType, ArcIndexType>
    ::Head(ArcIndexType arc) const {
  DCHECK(IsArcValid(arc));
  return head_[arc];
}

template<typename NodeIndexType, typename ArcIndexType>
NodeIndexType ReverseArcListGraph<NodeIndexType, ArcIndexType>
    ::Tail(ArcIndexType arc) const {
  return head_[OppositeArc(arc)];
}

template<typename NodeIndexType, typename ArcIndexType>
void ReverseArcListGraph<NodeIndexType, ArcIndexType>
    ::ReserveNodes(NodeIndexType bound) {
  DCHECK(!const_capacities_);
  DCHECK_GE(bound, num_nodes_);
  if (bound <= num_nodes_) return;
  node_capacity_ = bound;
  start_.reserve(bound);
  reverse_start_.reserve(bound);
}

template<typename NodeIndexType, typename ArcIndexType>
void ReverseArcListGraph<NodeIndexType, ArcIndexType>
    ::ReserveArcs(ArcIndexType bound) {
  DCHECK(!const_capacities_);
  DCHECK_GE(bound, num_arcs_);
  if (bound <= num_arcs_) return;
  arc_capacity_ = bound;
  head_.reserve(bound);
  next_.reserve(bound);
}

template<typename NodeIndexType, typename ArcIndexType>
void ReverseArcListGraph<NodeIndexType, ArcIndexType>
    ::AddNode(NodeIndexType node) {
  if (node < num_nodes_) return;
  DCHECK(!const_capacities_ || node < node_capacity_);
  num_nodes_ = node + 1;
  start_.resize(num_nodes_, Base::kNilArc);
  reverse_start_.resize(num_nodes_, Base::kNilArc);
}

template<typename NodeIndexType, typename ArcIndexType>
ArcIndexType ReverseArcListGraph<NodeIndexType, ArcIndexType>
    ::AddArc(NodeIndexType tail, NodeIndexType head) {
  AddNode(tail > head ? tail : head);
  head_.grow(tail, head);
  next_.grow(reverse_start_[head], start_[tail]);
  start_[tail] = num_arcs_;
  reverse_start_[head] = ~num_arcs_;
  DCHECK(!const_capacities_ || num_arcs_ < arc_capacity_);
  return num_arcs_++;
}

template<typename NodeIndexType, typename ArcIndexType>
void ReverseArcListGraph<NodeIndexType, ArcIndexType>
    ::Build(std::vector<ArcIndexType> *permutation) {
  if (permutation != NULL) {
    permutation->clear();
  }
}

template<typename NodeIndexType, typename ArcIndexType>
class ReverseArcListGraph<NodeIndexType, ArcIndexType>::BaseArcIterator {
 public:
  BaseArcIterator(const ReverseArcListGraph &graph, ArcIndexType index)
      : graph_(&graph), index_(index) {}
  bool Ok() const {
    return index_ != Base::kNilArc;
  }
  ArcIndexType Index() const {
    return index_;
  }
  void Next() {
    DCHECK(Ok());
    index_ = graph_->next_[index_];
  }

 protected:
  const ReverseArcListGraph *graph_;
  ArcIndexType index_;
};

template<typename NodeIndexType, typename ArcIndexType>
class ReverseArcListGraph<NodeIndexType, ArcIndexType>
    ::OutgoingArcIterator : public BaseArcIterator {
 public:
  OutgoingArcIterator(const ReverseArcListGraph &graph, NodeIndexType node)
      : BaseArcIterator(graph, graph.start_[node]) {
    DCHECK(graph.IsNodeValid(node));
  }
  OutgoingArcIterator(const ReverseArcListGraph &graph,
      NodeIndexType node, ArcIndexType arc) : BaseArcIterator(graph, arc) {
    DCHECK(graph.IsNodeValid(node));
    DCHECK(arc == Base::kNilArc || arc >= 0);
    DCHECK(arc == Base::kNilArc || graph.Tail(arc) == node);
  }
};

template<typename NodeIndexType, typename ArcIndexType>
class ReverseArcListGraph<NodeIndexType, ArcIndexType>
    ::IncomingArcIterator : public BaseArcIterator {
 public:
  IncomingArcIterator(const ReverseArcListGraph &graph, NodeIndexType node)
      : BaseArcIterator(graph, graph.reverse_start_[node]) {
    DCHECK(graph.IsNodeValid(node));
  }
  IncomingArcIterator(const ReverseArcListGraph &graph,
      NodeIndexType node, ArcIndexType arc) : BaseArcIterator(graph, arc) {
    DCHECK(graph.IsNodeValid(node));
    DCHECK(arc == Base::kNilArc || arc < 0);
    DCHECK(arc == Base::kNilArc || graph.Tail(arc) == node);
  }
};

template<typename NodeIndexType, typename ArcIndexType>
class ReverseArcListGraph<NodeIndexType, ArcIndexType>
    ::IncidentArcIterator : public BaseArcIterator {
  using BaseArcIterator::graph_;
  using BaseArcIterator::index_;

 public:
  IncidentArcIterator(const ReverseArcListGraph &graph, NodeIndexType node)
      : BaseArcIterator(graph, graph.reverse_start_[node]), node_(node) {
    DCHECK(graph.IsNodeValid(node));
    if (index_ == Base::kNilArc) index_ = graph.start_[node];
  }
  IncidentArcIterator(const ReverseArcListGraph &graph,
      NodeIndexType node, ArcIndexType arc)
      : BaseArcIterator(graph, arc), node_(node) {
    DCHECK(graph.IsNodeValid(node));
    DCHECK(arc == Base::kNilArc || graph.Tail(arc) == node);
  }
  void Next() {
    DCHECK(BaseArcIterator::Ok());
    if (index_ < 0) {
      index_ = graph_->next_[index_];
      if (index_ == Base::kNilArc) {
        index_ = graph_->start_[node_];
      }
    } else {
      index_ = graph_->next_[index_];
    }
  }

 private:
  NodeIndexType node_;
};

// ReverseArcStaticGraph definition ------------------------------------------

template<typename NodeIndexType, typename ArcIndexType>
ArcIndexType ReverseArcStaticGraph<NodeIndexType, ArcIndexType>
    ::OppositeArc(ArcIndexType arc) const {
  DCHECK(is_built_);
  DCHECK(IsArcValid(arc));
  return opposite_[arc];
}

template<typename NodeIndexType, typename ArcIndexType>
NodeIndexType ReverseArcStaticGraph<NodeIndexType, ArcIndexType>
    ::Head(ArcIndexType arc) const {
  DCHECK(is_built_);
  DCHECK(IsArcValid(arc));
  return head_[arc];
}

template<typename NodeIndexType, typename ArcIndexType>
NodeIndexType ReverseArcStaticGraph<NodeIndexType, ArcIndexType>
    ::Tail(ArcIndexType arc) const {
  DCHECK(is_built_);
  return head_[OppositeArc(arc)];
}

template<typename NodeIndexType, typename ArcIndexType>
void ReverseArcStaticGraph<NodeIndexType, ArcIndexType>
    ::ReserveNodes(NodeIndexType bound) {
  DCHECK(!const_capacities_);
  DCHECK_GE(bound, num_nodes_);
  if (bound <= num_nodes_) return;
  node_capacity_ = bound;
}

template<typename NodeIndexType, typename ArcIndexType>
void ReverseArcStaticGraph<NodeIndexType, ArcIndexType>
    ::ReserveArcs(ArcIndexType bound) {
  DCHECK(!const_capacities_);
  DCHECK_GE(bound, num_arcs_);
  if (bound <= num_arcs_) return;
  arc_capacity_ = bound;
  head_.reserve(bound);
}

template<typename NodeIndexType, typename ArcIndexType>
void ReverseArcStaticGraph<NodeIndexType, ArcIndexType>
    ::AddNode(NodeIndexType node) {
  if (node < num_nodes_) return;
  DCHECK(!const_capacities_ || node < node_capacity_);
  num_nodes_ = node + 1;
}

template<typename NodeIndexType, typename ArcIndexType>
ArcIndexType ReverseArcStaticGraph<NodeIndexType, ArcIndexType>
    ::AddArc(NodeIndexType tail, NodeIndexType head) {
  AddNode(tail > head ? tail : head);

  // We inverse head and tail here because it is more convenient this way
  // during build time, see Build().
  head_.grow(head, tail);
  DCHECK(!const_capacities_ || num_arcs_ < arc_capacity_);
  return num_arcs_++;
}

template<typename NodeIndexType, typename ArcIndexType>
void ReverseArcStaticGraph<NodeIndexType, ArcIndexType>
    ::Build(std::vector<ArcIndexType> *permutation) {
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
  reverse_start_[0] = - num_arcs_;

  // Fill reverse arc information.
  for (int i = 0; i < num_arcs_; ++i) {
    opposite_[opposite_[i]] = i;
  }
  for (NodeIndexType node = 0 ; node < num_nodes_; ++node) {
    for (OutgoingArcIterator it(*this, node); it.Ok(); it.Next()) {
      head_[opposite_[it.Index()]] = node;
    }
  }
}

template<typename NodeIndexType, typename ArcIndexType>
class ReverseArcStaticGraph<NodeIndexType, ArcIndexType>
    ::OutgoingArcIterator : public Base::BaseStaticArcIterator {
 public:
  OutgoingArcIterator(
      const ReverseArcStaticGraph &graph, NodeIndexType node)
      : Base::BaseStaticArcIterator(
          graph.start_[node], graph.DirectArcLimit(node)) {
    DCHECK(graph.is_built_);
    DCHECK(graph.IsNodeValid(node));
  }
  OutgoingArcIterator(const ReverseArcStaticGraph &graph,
      NodeIndexType node, ArcIndexType arc)
      : Base::BaseStaticArcIterator(arc, graph.DirectArcLimit(node)) {
    DCHECK(graph.is_built_);
    DCHECK(graph.IsNodeValid(node));
    DCHECK_GE(arc, graph.start_[node]);
  }
};

template<typename NodeIndexType, typename ArcIndexType>
class ReverseArcStaticGraph<NodeIndexType, ArcIndexType>
    ::IncomingArcIterator : public Base::BaseStaticArcIterator {
 public:
  IncomingArcIterator(
      const ReverseArcStaticGraph &graph, NodeIndexType node)
      : Base::BaseStaticArcIterator(
          graph.reverse_start_[node], graph.ReverseArcLimit(node)) {
    DCHECK(graph.is_built_);
    DCHECK(graph.IsNodeValid(node));
  }
  IncomingArcIterator(const ReverseArcStaticGraph &graph,
      NodeIndexType node, ArcIndexType arc)
      : Base::BaseStaticArcIterator(arc, graph.ReverseArcLimit(node)) {
    DCHECK(graph.is_built_);
    DCHECK(graph.IsNodeValid(node));
    DCHECK_GE(arc, graph.reverse_start_[node]);
  }
};

template<typename NodeIndexType, typename ArcIndexType>
class ReverseArcStaticGraph<NodeIndexType, ArcIndexType>
    ::IncidentArcIterator : public Base::BaseStaticArcIterator {
  using Base::BaseStaticArcIterator::index_;
  using Base::BaseStaticArcIterator::limit_;

 public:
  IncidentArcIterator(const ReverseArcStaticGraph &graph, NodeIndexType node)
      : Base::BaseStaticArcIterator(
          graph.reverse_start_[node], graph.DirectArcLimit(node)),
        next_start_(graph.start_[node]),
        first_limit_(graph.ReverseArcLimit(node)) {
    if (index_ == first_limit_) index_ = next_start_;
    DCHECK(graph.IsNodeValid(node));
    DCHECK((index_ >= graph.reverse_start_[node] && index_ < first_limit_)
        || (index_ >= next_start_));
  }
  IncidentArcIterator(const ReverseArcStaticGraph &graph,
      NodeIndexType node, ArcIndexType arc)
      : Base::BaseStaticArcIterator(arc, graph.DirectArcLimit(node)),
        next_start_(graph.start_[node]),
        first_limit_(graph.ReverseArcLimit(node)) {
    DCHECK(graph.IsNodeValid(node));
    DCHECK((index_ >= graph.reverse_start_[node] && index_ < first_limit_)
        || (index_ >= next_start_));
  }
  void Next() {
    DCHECK(Base::BaseStaticArcIterator::Ok());
    index_++;
    if (index_ == first_limit_) {
      index_ = next_start_;
    }
  }

 private:
  ArcIndexType next_start_;
  ArcIndexType first_limit_;
};

// ReverseArcMixedGraph definition -------------------------------------------

template<typename NodeIndexType, typename ArcIndexType>
ArcIndexType ReverseArcMixedGraph<NodeIndexType, ArcIndexType>
    ::OppositeArc(ArcIndexType arc) const {
  DCHECK(IsArcValid(arc));
  return ~arc;
}

template<typename NodeIndexType, typename ArcIndexType>
NodeIndexType ReverseArcMixedGraph<NodeIndexType, ArcIndexType>
    ::Head(ArcIndexType arc) const {
  DCHECK(is_built_);
  DCHECK(IsArcValid(arc));
  return head_[arc];
}

template<typename NodeIndexType, typename ArcIndexType>
NodeIndexType ReverseArcMixedGraph<NodeIndexType, ArcIndexType>
    ::Tail(ArcIndexType arc) const {
  DCHECK(is_built_);
  return head_[OppositeArc(arc)];
}

template<typename NodeIndexType, typename ArcIndexType>
void ReverseArcMixedGraph<NodeIndexType, ArcIndexType>
    ::ReserveNodes(NodeIndexType bound) {
  DCHECK(!const_capacities_);
  DCHECK_GE(bound, num_nodes_);
  if (bound <= num_nodes_) return;
  node_capacity_ = bound;
}

template<typename NodeIndexType, typename ArcIndexType>
void ReverseArcMixedGraph<NodeIndexType, ArcIndexType>
    ::ReserveArcs(ArcIndexType bound) {
  DCHECK(!const_capacities_);
  DCHECK_GE(bound, num_arcs_);
  if (bound <= num_arcs_) return;
  arc_capacity_ = bound;
  head_.reserve(bound);
}

template<typename NodeIndexType, typename ArcIndexType>
void ReverseArcMixedGraph<NodeIndexType, ArcIndexType>
    ::AddNode(NodeIndexType node) {
  if (node < num_nodes_) return;
  DCHECK(!const_capacities_ || node < node_capacity_);
  num_nodes_ = node + 1;
}

template<typename NodeIndexType, typename ArcIndexType>
ArcIndexType ReverseArcMixedGraph<NodeIndexType, ArcIndexType>
    ::AddArc(NodeIndexType tail, NodeIndexType head) {
  AddNode(tail > head ? tail : head);

  // We inverse head and tail here because it is more convenient this way
  // during build time, see Build().
  head_.grow(head, tail);
  DCHECK(!const_capacities_ || num_arcs_ < arc_capacity_);
  return num_arcs_++;
}

template<typename NodeIndexType, typename ArcIndexType>
void ReverseArcMixedGraph<NodeIndexType, ArcIndexType>
    ::Build(std::vector<ArcIndexType> *permutation) {
  DCHECK(!is_built_);
  if (is_built_) return;
  is_built_ = true;
  node_capacity_ = num_nodes_;
  arc_capacity_ = num_arcs_;
  this->FreezeCapacities();
  this->BuildStartAndForwardHead(&head_, &start_, permutation);

  // Fill tails.
  for (NodeIndexType node = 0 ; node < num_nodes_; ++node) {
    for (OutgoingArcIterator it(*this, node); it.Ok(); it.Next()) {
      head_[~it.Index()] = node;
    }
  }

  // Fill information for iterating over reverse arcs.
  reverse_start_.assign(num_nodes_, Base::kNilArc);
  next_.reserve(num_arcs_);
  for (ArcIndexType i = 0; i < num_arcs_; ++i) {
    next_.push_back(reverse_start_[Head(i)]);
    reverse_start_[Head(i)] = - next_.size();
  }
}

template<typename NodeIndexType, typename ArcIndexType>
class ReverseArcMixedGraph<NodeIndexType, ArcIndexType>
    ::OutgoingArcIterator : public Base::BaseStaticArcIterator {
 public:
  OutgoingArcIterator(
      const ReverseArcMixedGraph &graph, NodeIndexType node)
      : Base::BaseStaticArcIterator(
          graph.start_[node], graph.DirectArcLimit(node)) {
    DCHECK(graph.is_built_);
    DCHECK(graph.IsNodeValid(node));
  }
  OutgoingArcIterator(const ReverseArcMixedGraph &graph,
      NodeIndexType node, ArcIndexType arc)
      : Base::BaseStaticArcIterator(arc, graph.DirectArcLimit(node)) {
    DCHECK(graph.is_built_);
    DCHECK(graph.IsNodeValid(node));
    DCHECK_GE(arc, graph.start_[node]);
  }
};

template<typename NodeIndexType, typename ArcIndexType>
class ReverseArcMixedGraph<NodeIndexType, ArcIndexType>::IncomingArcIterator {
 public:
  IncomingArcIterator(const ReverseArcMixedGraph &graph,
      NodeIndexType node) : graph_(&graph) {
    DCHECK(graph.is_built_);
    DCHECK(graph.IsNodeValid(node));
    index_ = graph.reverse_start_[node];
  }
  IncomingArcIterator(const ReverseArcMixedGraph &graph,
      NodeIndexType node, ArcIndexType arc) : graph_(&graph) {
    DCHECK(graph.is_built_);
    DCHECK(graph.IsNodeValid(node));
    DCHECK(arc == Base::kNilArc || arc < 0);
    DCHECK(arc == Base::kNilArc || graph.Tail(arc) == node);
    index_ = arc;
  }
  bool Ok() const {
    return index_ != Base::kNilArc;
  }
  ArcIndexType Index() const {
    return index_;
  }
  void Next() {
    DCHECK(Ok());
    index_ = graph_->next_[~index_];
  }

 private:
  const ReverseArcMixedGraph *graph_;
  ArcIndexType index_;
};

template<typename NodeIndexType, typename ArcIndexType>
class ReverseArcMixedGraph<NodeIndexType, ArcIndexType>::IncidentArcIterator {
 public:
  IncidentArcIterator(const ReverseArcMixedGraph &graph,
      NodeIndexType node) : graph_(&graph) {
    DCHECK(graph.is_built_);
    DCHECK(graph.IsNodeValid(node));
    index_ = graph.reverse_start_[node];
    restart_ = graph.start_[node];
    limit_ = graph.DirectArcLimit(node);
    if (index_ == Base::kNilArc) {
      index_ = restart_;
    }
  }
  IncidentArcIterator(const ReverseArcMixedGraph &graph,
      NodeIndexType node, ArcIndexType arc) : graph_(&graph) {
    DCHECK(graph.is_built_);
    DCHECK(graph.IsNodeValid(node));
    index_ = arc;
    restart_ = graph.start_[node];
    limit_ = graph.DirectArcLimit(node);
    DCHECK(arc == Base::kNilArc || arc == limit_ || graph.Tail(arc) == node);
  }
  bool Ok() const {
    // Note that we always have limit_ <= Base::kNilArc.
    return index_ < limit_;
  }
  ArcIndexType Index() const {
    return index_;
  }
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

 private:
  const ReverseArcMixedGraph *graph_;
  ArcIndexType index_;
  ArcIndexType restart_;
  ArcIndexType limit_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_GRAPH_H_
