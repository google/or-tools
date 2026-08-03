// Copyright 2010-2025 Google LLC
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

// This file defines a generic graph interface on which most algorithms can be
// built and provides a few efficient implementations with a fast construction
// time. Its design is based on the experience acquired by the Operations
// Research team in their various graph algorithm implementations.
//
// Also see README.md#basegraph for a more graphical documentation of the
// concepts presented here.
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
//   - ListGraph<> for the simplest api.
//   - StaticGraph<> for performance, but requires building before usage, see
//     below
//   - CompleteGraph<> if you need a fully connected graph (with self loops).
//   - CompleteBipartiteGraph<> if you need a fully connected bipartite graph
//   - ReverseArcListGraph<> to add reverse arcs to ListGraph<>
//   - ReverseArcStaticGraph<> to add reverse arcs to StaticGraph<>
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
// And two more involved ones:
//
// - OutgoingOrOppositeIncomingArcs(node): This returns both the forward arcs
//   leaving the node (i.e. OutgoingArcs(node)) and the reverse arcs leaving the
//   node (i.e. the opposite arcs of the ones returned by IncomingArcs(node)).
// - OppositeIncomingArcs(node): This returns the reverse arcs leaving the node.
//
// Note on iteration efficiency: When re-indexing the arcs it is not possible to
// have both the outgoing arcs and the incoming ones form a consecutive range.
// It is however possible to do so for the outgoing arcs and the opposite
// incoming arcs. It is why the OutgoingOrOppositeIncomingArcs() and
// OutgoingArcs() iterations are more efficient than the IncomingArcs() one.
//
// Iterators are invalidated by any operation that changes the graph (e.g.
// `AddArc()`).
//
// If you know the graph size in advance, this already set the number of nodes,
// reserve space for the arcs and check in DEBUG mode that you don't go over the
// bounds:
//   ListGraph<> graph(num_nodes, arc_capacity);
//
// Storing and using node annotations:
//   vector<bool> is_visited(graph.num_nodes(), false);
//   ...
//   for (int node = 0; node < graph.num_nodes(); ++node) {
//     if (!is_visited[node]) ...
//   }
//
// Storing and using arc annotations:
//   vector<int> weights;
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
//   // Note: providing the arc capacity is optional, but helps memory usage.
//   Graph::Builder graph_builder(num_nodes, arc_capacity);
//   vector<int> weights;
//   weights.reserve(arc_capacity);  // Optional, but help memory usage.
//   for (...) {
//     graph_builder.AddArc(tail, head);
//     weights.push_back(arc_weight);
//   }
//   ...
//   // Building the graph may permute the arc indices:
//   const Graph graph = std::move(graph_builder).BuildAndPermute(weights);
//   ...
//
//  Note on the permutation:
//  - you can permute more than one vector at construction:
//      std::vector<int> colors, normals;
//      const Graph graph =
//          std::move(graph_builder).BuildAndPermute(colors, normals);
//  - you can also get the permutation explicitly with `Build()` and use it
//    later:
//      std::vector<int> permutation;
//      const Graph graph = std::move(graph_builder).Build(&permutation);
//      ...
//
// Encoding an undirected graph: If you don't need arc annotation, then the best
// is to add two arcs for each edge (one in each direction) to a directed graph.
// Otherwise you can do the following.
//
//   typedef ReverseArc... Graph;
//   Graph::Builder graph_builder;
//   for (...) {
//     graph_builder.AddArc(tail, head);  // or graph.AddArc(head, tail) but not
//                                        // both.
//     edge_annotations.push_back(value);
//   }
//   const Graph graph =
//       std::move(graph_builder).BuildAndPermute(edge_annotations);
//   ...
//   for (const Graph::NodeIndex node : graph->AllNodes()) {
//     for (const Graph::ArcIndex arc :
//          graph.OutgoingOrOppositeIncomingArcs(node)) {
//       destination = graph.Head(arc);
//       annotation = edge_annotations[arc < 0 ? graph.OppositeArc(arc) : arc];
//     }
//   }
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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/base/nullability.h"
#include "absl/debugging/leak_check.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/types/span.h"
#include "ortools/base/constant_divisor.h"
#include "ortools/graph_base/iterators.h"

namespace util {

namespace internal {

template <typename IndexT, typename T>
class SVector;

template <typename IndexT, typename T>
class Vector;

// A base class for graph builders, that adds common functionality to all graph
// builders.
template <typename BuilderImpl, typename GraphImpl, typename NodeIndexType,
          typename ArcIndexType>
class GraphBuilderBase {
 public:
  // Builds the graph and permutes the given arc-indexed arrays so that they
  // correspond to the arc indices of the built graph.
  template <typename... ArcPropertyArray>
  std::unique_ptr<GraphImpl> BuildAndPermute(
      ArcPropertyArray&... arc_properties) &&;

  // Builds the graph and returns it. If `permutation` is not nullptr, it is
  // filled with the arc permutation if the graph implementation reorders arcs.
  GraphImpl BuildGraph(
      std::vector<ArcIndexType>* absl_nullable permutation) && {
    return std::move(
        *static_cast<BuilderImpl&&>(std::move(*this)).Build(permutation));
  }

  // Same as `BuildAndPermute()`, but returns a value.
  template <typename... ArcPropertyArray>
  GraphImpl BuildGraphAndPermute(ArcPropertyArray&... arc_properties) && {
    return std::move(*std::move(*this).BuildAndPermute(arc_properties...));
  }

  void Reserve(NodeIndexType node_capacity, ArcIndexType arc_capacity) {
    static_cast<BuilderImpl&>(*this).ReserveArcs(arc_capacity);
    static_cast<BuilderImpl&>(*this).ReserveNodes(node_capacity);
  }

  // Reserving does nothing by default.
  void ReserveNodes(NodeIndexType bound) {}
  void ReserveArcs(ArcIndexType bound) {}

  // The number of arcs added by the user. Might not be the same as the number
  // of arcs in the built graph for some graph implementations (e.g.
  // `FlowGraph`).
  ArcIndexType num_arcs() const;

  // The number of nodes added by the user. Might not be the same as the number
  // of nodes in the built graph for some graph implementations.
  NodeIndexType num_nodes();
};

// A graph builder for graphs that don't need building (e.g. `ListGraph`).
// Simply forwards mutable calls to the underlying graph.
// TODO(b/501313028): Remove once migration is over.
template <typename Impl, typename NodeIndexType, typename ArcIndexType,
          bool build_permutes_arcs>
class MutableGraphBuilder
    : public GraphBuilderBase<
          MutableGraphBuilder<Impl, NodeIndexType, ArcIndexType,
                              build_permutes_arcs>,
          Impl, NodeIndexType, ArcIndexType> {
  using Base =
      GraphBuilderBase<MutableGraphBuilder<Impl, NodeIndexType, ArcIndexType,
                                           build_permutes_arcs>,
                       Impl, NodeIndexType, ArcIndexType>;

 public:
  static constexpr bool kBuildPermutesArcs = build_permutes_arcs;

  MutableGraphBuilder() : graph_(std::make_unique<Impl>()) {}
  MutableGraphBuilder(NodeIndexType num_nodes, ArcIndexType arc_capacity)
      : graph_(std::make_unique<Impl>(num_nodes, arc_capacity)) {}

  // TODO(b/501313028): This should be `= default` when we no longer mutate
  // the graph.
  MutableGraphBuilder(const MutableGraphBuilder& other) {
    graph_ = std::make_unique<Impl>(*other.graph_);
  }
  MutableGraphBuilder& operator=(const MutableGraphBuilder& other) {
    if (this == &other) return *this;
    graph_ = std::make_unique<Impl>(*other.graph_);
    return *this;
  }
  MutableGraphBuilder(MutableGraphBuilder&&) = default;
  MutableGraphBuilder& operator=(MutableGraphBuilder&&) = default;

  std::unique_ptr<Impl> Build(
      std::vector<ArcIndexType>* absl_nullable permutation) && {
    graph_->Build(permutation);
    return std::move(graph_);
  }

  void AddNode(NodeIndexType node) { graph_->AddNode(node); }

  ArcIndexType AddArc(NodeIndexType tail, NodeIndexType head) {
    return graph_->AddArc(tail, head);
  }

  void ReserveNodes(NodeIndexType bound) { graph_->ReserveNodes(bound); }

  void ReserveArcs(ArcIndexType bound) { graph_->ReserveArcs(bound); }

  void FreezeCapacities() { graph_->FreezeCapacities(); }

  NodeIndexType node_capacity() const { return graph_->node_capacity(); }
  ArcIndexType arc_capacity() const { return graph_->arc_capacity(); }

  ArcIndexType num_arcs() const { return graph_->num_arcs(); }
  NodeIndexType num_nodes() const { return graph_->num_nodes(); }

 protected:
  std::unique_ptr<Impl> graph_;
};
}  // namespace internal

// Base class of all Graphs implemented here. The default value for the graph
// index types is int32_t since almost all graphs that fit into memory do not
// need bigger indices.
//
// Note: The type can be unsigned, except for the graphs with reverse arcs
// where the ArcIndexType must be signed, but not necessarily the NodeIndexType.
//
// `NodeIndexType` and `ArcIndexType` can be any integer type, but can also be
// strong integer types (e.g. `StrongInt`). Strong integer types are types that
// behave like integers (comparison, arithmetic, etc.), and are (explicitly)
// constructible/convertible from/to integers.
template <typename Impl, typename NodeIndexType = int32_t,
          typename ArcIndexType = int32_t, bool HasNegativeReverseArcs = false>
class BaseGraph  //
{
 public:
  // Typedef so you can use Graph::NodeIndex and Graph::ArcIndex to be generic
  // but also to improve the readability of your code. We also recommend
  // that you define a typedef ... Graph; for readability.
  typedef NodeIndexType NodeIndex;
  typedef ArcIndexType ArcIndex;
  static constexpr bool kHasNegativeReverseArcs = HasNegativeReverseArcs;

  BaseGraph() = default;
  BaseGraph(const BaseGraph&) = default;
  BaseGraph& operator=(const BaseGraph&) = default;
  BaseGraph(BaseGraph&&) = default;
  BaseGraph& operator=(BaseGraph&&) = default;

  virtual ~BaseGraph() = default;

  // Returns the number of valid nodes in the graph.
  NodeIndexType num_nodes() const {
    return static_cast<const Impl*>(this)->num_nodes();
  }

  [[deprecated("Use num_nodes() instead")]] ABSL_REFACTOR_INLINE NodeIndexType
  size() const {
    return num_nodes();
  }

  // Returns the number of valid arcs in the graph.
  ArcIndexType num_arcs() const {
    return static_cast<const Impl*>(this)->num_arcs();
  }

  // Returns the node capacity of the graph.
  NodeIndexType node_capacity() const {
    // By default (immutable graphs) this is the number of nodes in the graph.
    return num_nodes();
  }

  // Returns the arc capacity of the graph.
  ArcIndexType arc_capacity() const {
    // By default (immutable graphs) this is the number of arcs in the graph.
    return num_arcs();
  }

  // Allows nice range-based for loop:
  //   for (const NodeIndex node : graph.AllNodes()) { ... }
  //   for (const ArcIndex arc : graph.AllForwardArcs()) { ... }
  IntegerRange<NodeIndex> AllNodes() const;
  IntegerRange<ArcIndex> AllForwardArcs() const;

  // Returns true if the given node is a valid node of the graph.
  bool IsNodeValid(NodeIndexType node) const {
    return node >= NodeIndexType(0) && node < num_nodes();
  }

  // Returns true if the given arc is a valid arc of the graph.
  // Note that the arc validity range changes for graph with reverse arcs.
  bool IsArcValid(ArcIndexType arc) const {
    return (HasNegativeReverseArcs ? -num_arcs() : ArcIndexType(0)) <= arc &&
           arc < num_arcs();
  }

  // Constants that will never be a valid node or arc.
  // They are the maximum possible node and arc capacity.
  static_assert(std::numeric_limits<NodeIndexType>::is_specialized);
  static constexpr NodeIndexType kNilNode =
      std::numeric_limits<NodeIndexType>::max();
  static_assert(std::numeric_limits<ArcIndexType>::is_specialized);
  static constexpr ArcIndexType kNilArc =
      std::numeric_limits<ArcIndexType>::max();

 protected:
  // Functions commented when defined because they are implementation details.
  void BuildStartAndForwardHead(
      internal::SVector<ArcIndexType, NodeIndexType>* head,
      internal::Vector<NodeIndexType, ArcIndexType>* start,
      std::vector<ArcIndexType>* permutation);
};

template <typename Impl, typename NodeIndexType = int32_t,
          typename ArcIndexType = int32_t, bool HasNegativeReverseArcs = false>
class MutableGraph : public BaseGraph<Impl, NodeIndexType, ArcIndexType,
                                      HasNegativeReverseArcs> {
 public:
  MutableGraph()
      : num_nodes_(0),
        node_capacity_(0),
        num_arcs_(0),
        arc_capacity_(0),
        const_capacities_(false) {}
  MutableGraph(const MutableGraph&) = default;
  MutableGraph& operator=(const MutableGraph&) = default;

  NodeIndexType num_nodes() const { return num_nodes_; }

  ArcIndexType num_arcs() const { return num_arcs_; }

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

 protected:
  NodeIndexType num_nodes_;
  NodeIndexType node_capacity_;
  ArcIndexType num_arcs_;
  ArcIndexType arc_capacity_;
  // TODO(b/501313028): This is only relevant for mutable graphs.
  bool const_capacities_;
};

// Provide a `size` overload so that `Graph` and `std::vector<std::vector<int>>`
// are compatible.
template <typename Impl, typename NodeIndexType, typename ArcIndexType,
          bool HasNegativeReverseArcs>
constexpr auto size(const BaseGraph<Impl, NodeIndexType, ArcIndexType,
                                    HasNegativeReverseArcs>& graph) {
  return graph.num_nodes();
}
// An iterator that wraps an arc iterator and retrieves a property of the arc.
// The property to retrieve is specified by a `Graph` member function taking an
// `ArcIndex` parameter. For example, `ArcHeadIterator` retrieves the head of an
// arc with `&Graph::Head`.
template <typename Graph, typename ArcIterator, typename PropertyT,
          PropertyT (Graph::*property)(typename Graph::ArcIndex) const>
class ArcPropertyIterator {
 public:
  using value_type = PropertyT;
  using difference_type = std::iterator_traits<ArcIterator>::difference_type;
  using iterator_category = std::forward_iterator_tag;
  // TODO(user): Use the iterator category of the underlying iterator:
  // std::iterator_traits<ArcIterator>::iterator_category;

  ArcPropertyIterator() = default;

  ArcPropertyIterator(const Graph& graph, ArcIterator arc_it)
      : arc_it_(std::move(arc_it)), graph_(&graph) {}

  value_type operator*() const { return (graph_->*property)(*arc_it_); }

  ArcPropertyIterator& operator++() {
    ++arc_it_;
    return *this;
  }
  ArcPropertyIterator operator++(int) {
    auto tmp = *this;
    ++arc_it_;
    return tmp;
  }

  friend bool operator==(const ArcPropertyIterator& l,
                         const ArcPropertyIterator& r) {
    return l.arc_it_ == r.arc_it_;
  }

  friend bool operator!=(const ArcPropertyIterator& l,
                         const ArcPropertyIterator& r) {
    return !(l == r);
  }

 private:
  ArcIterator arc_it_;
  const Graph* graph_;
};

// An iterator that iterates on the heads of the arcs of another iterator.
template <typename Graph, typename ArcIterator>
using ArcHeadIterator =
    ArcPropertyIterator<Graph, ArcIterator, typename Graph::NodeIndex,
                        &Graph::Head>;

// An iterator that iterates on the opposite arcs of another iterator.
template <typename Graph, typename ArcIterator>
using ArcOppositeArcIterator =
    ArcPropertyIterator<Graph, ArcIterator, typename Graph::ArcIndex,
                        &Graph::OppositeArc>;

namespace internal {

// Returns true if `ArcIndexType` is signed. Work for integers and strong
// integer types.
template <typename ArcIndexType>
constexpr bool IsSigned() {
  return ArcIndexType(-1) < ArcIndexType(0);
}

// Allows indexing into a vector with an edge or node index.
template <typename IndexT, typename T>
class Vector : public std::vector<T> {
 public:
  Vector() = default;
  explicit Vector(IndexT size, T value)
      : std::vector<T>(static_cast<size_t>(size), value) {}

  const T& operator[](IndexT index) const {
    DCHECK_GE(index, IndexT(0));
    DCHECK_LT(index, this->size());
    return this->data()[static_cast<size_t>(index)];
  }
  T& operator[](IndexT index) {
    DCHECK_GE(index, IndexT(0));
    DCHECK_LT(index, this->size());
    return this->data()[static_cast<size_t>(index)];
  }

  void resize(IndexT index, const T& value) {
    return std::vector<T>::resize(static_cast<size_t>(index), value);
  }

  void reserve(IndexT index) {
    return std::vector<T>::reserve(static_cast<size_t>(index));
  }

  void assign(IndexT index, const T& value) {
    return std::vector<T>::assign(static_cast<size_t>(index), value);
  }

  IndexT size() const { return IndexT(std::vector<T>::size()); }
};

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
template <typename IndexT, typename T>
class SVector {
 public:
  using value_type = T;

  SVector() : base_(nullptr), size_(0), capacity_(0) {}

  ~SVector() { clear_and_dealloc(); }

  // Copy constructor and assignment operator.
  SVector(const SVector& other) : SVector() { *this = other; }
  SVector& operator=(const SVector& other) {
    if (capacity_ < other.size_) {
      clear_and_dealloc();
      // NOTE(user): Alternatively, our capacity could inherit from the other
      // vector's capacity, which can be (much) greater than its size.
      capacity_ = other.size_;
      base_ = Allocate(capacity_);
      CHECK(base_ != nullptr);
      base_ += static_cast<ptrdiff_t>(capacity_);
    } else {  // capacity_ >= other.size
      clear();
    }
    // Perform the actual copy of the payload.
    size_ = other.size_;
    CopyInternal(other, std::is_integral<T>());
    return *this;
  }

  // Move constructor and move assignment operator.
  SVector(SVector&& other) noexcept : SVector() { swap(other); }
  SVector& operator=(SVector&& other) noexcept {
    // NOTE(user): We could just swap() and let the other's destruction take
    // care of the clean-up, but it is probably less bug-prone to perform the
    // destruction immediately.
    clear_and_dealloc();
    swap(other);
    return *this;
  }

  T& operator[](IndexT n) {
    DCHECK_LT(n, size_);
    DCHECK_GE(n, -size_);
    return base_[static_cast<ptrdiff_t>(n)];
  }

  const T& operator[](IndexT n) const {
    DCHECK_LT(n, size_);
    DCHECK_GE(n, -size_);
    return base_[static_cast<ptrdiff_t>(n)];
  }

  void resize(IndexT n) {
    reserve(n);
    for (IndexT i = -n; i < -size_; ++i) {
      new (base_ + static_cast<ptrdiff_t>(i)) T();
    }
    for (IndexT i = size_; i < n; ++i) {
      new (base_ + static_cast<ptrdiff_t>(i)) T();
    }
    for (IndexT i = -size_; i < -n; ++i) {
      base_[static_cast<ptrdiff_t>(i)].~T();
    }
    for (IndexT i = n; i < size_; ++i) {
      base_[static_cast<ptrdiff_t>(i)].~T();
    }
    size_ = n;
  }

  void clear() { resize(IndexT(0)); }

  T* data() const { return base_; }

  const T* begin() const { return base_; }
  const T* end() const { return base_ + static_cast<ptrdiff_t>(size_); }

  void swap(SVector<IndexT, T>& x) noexcept {
    std::swap(base_, x.base_);
    std::swap(size_, x.size_);
    std::swap(capacity_, x.capacity_);
  }

  void reserve(IndexT n) {
    DCHECK_GE(n, IndexT(0));
    DCHECK_LE(n, max_size());
    if (n > capacity_) {
      const IndexT new_capacity = std::min(n, max_size());
      T* new_storage = Allocate(new_capacity);
      CHECK(new_storage != nullptr);
      T* new_base = new_storage + static_cast<ptrdiff_t>(new_capacity);
      // Note: There is no aliasing between input and output ranges.
      const ptrdiff_t ssize = static_cast<ptrdiff_t>(size_);
      std::uninitialized_move(base_ - ssize, base_ + ssize, new_base - ssize);
      IndexT saved_size = size_;
      clear_and_dealloc();
      size_ = saved_size;
      base_ = new_base;
      capacity_ = new_capacity;
    }
  }

  // NOTE(user): This doesn't currently support movable-only objects, but we
  // could fix that.
  void grow(const T& left = T(), const T& right = T()) {
    if (size_ == capacity_) {
      // We have to copy the elements because they are allowed to be element of
      // *this.
      T left_copy(left);    // NOLINT
      T right_copy(right);  // NOLINT
      reserve(NewCapacity(IndexT(1)));
      new (base_ + static_cast<ptrdiff_t>(size_)) T(right_copy);
      new (base_ - static_cast<ptrdiff_t>(size_) - 1) T(left_copy);
      ++size_;
    } else {
      new (base_ + static_cast<ptrdiff_t>(size_)) T(right);
      new (base_ - static_cast<ptrdiff_t>(size_) - 1) T(left);
      ++size_;
    }
  }

  IndexT size() const { return size_; }

  IndexT capacity() const { return capacity_; }

  IndexT max_size() const { return std::numeric_limits<IndexT>::max(); }

  void clear_and_dealloc() {
    if (base_ == nullptr) return;
    clear();
    if (capacity_ > IndexT(0)) {
      free(base_ - static_cast<ptrdiff_t>(capacity_));
    }
    capacity_ = IndexT(0);
    base_ = nullptr;
  }

 private:
  // Copies other.base_ to base_ in this SVector. Avoids iteration by copying
  // entire memory range in a single shot for the most commonly used integral
  // types which should be safe to copy in this way.
  void CopyInternal(const SVector& other, std::true_type) {
    std::memcpy(base_ - static_cast<ptrdiff_t>(other.size_),
                other.base_ - static_cast<ptrdiff_t>(other.size_),
                2LL * static_cast<ptrdiff_t>(other.size_) * sizeof(T));
  }

  // Copies other.base_ to base_ in this SVector. Safe for all types as it uses
  // constructor for each entry.
  void CopyInternal(const SVector& other, std::false_type) {
    for (IndexT i = -size_; i < size_; ++i) {
      new (base_ + static_cast<ptrdiff_t>(i))
          T(other.base_[static_cast<ptrdiff_t>(i)]);
    }
  }

  T* Allocate(IndexT capacity) const {
    return absl::IgnoreLeak(static_cast<T*>(
        malloc(2LL * static_cast<ptrdiff_t>(capacity) * sizeof(T))));
  }

  IndexT NewCapacity(IndexT delta) {
    // TODO(user): check validity.
    double candidate = 1.3 * static_cast<size_t>(capacity_);
    if (candidate > static_cast<size_t>(max_size())) {
      candidate = static_cast<size_t>(max_size());
    }
    IndexT new_capacity(candidate);
    if (new_capacity > capacity_ + delta) {
      return new_capacity;
    }
    return capacity_ + delta;
  }

  T* base_;          // Pointer to the element of index 0.
  IndexT size_;      // Valid index are [- size_, size_).
  IndexT capacity_;  // Reserved index are [- capacity_, capacity_).
};

}  // namespace internal

// Graph traits, to allow algorithms to manipulate graphs as adjacency lists.
// This works with any graph type, and any object that has:
// - an ADL-compatible `size()` method returning the number of nodes. This
//   includes any object with a `size()` method (e.g. `std::vector`), C-style
//   arrays, ...
// - an operator[] method taking a node index and returning a range of neighbour
//   node indices.
// One common example is using `std::vector<std::vector<int>>` to represent
// adjacency lists.
template <typename Graph>
struct GraphTraits {
 public:
  static constexpr auto num_nodes(const Graph& graph) {
    using std::size;
    return size(graph);
  }

 private:
  // The type of the range returned by `operator[]`.
  using NeighborRangeType = std::decay_t<
      decltype(std::declval<Graph>()[num_nodes(std::declval<Graph>())])>;

 public:
  // The index type for nodes of the graph.
  using NodeIndex =
      std::decay_t<decltype(*(std::declval<NeighborRangeType>().begin()))>;
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
// OutgoingArcs(), OutgoingArcsStartingFrom(), and the [] operator are all
// deterministic. Specifically, for two graphs constructed from the same
// sequence of node and arc creation calls, these iterators will return the
// result in the same order).
//
template <typename NodeIndexType = int32_t, typename ArcIndexType = int32_t>
class ListGraph : public MutableGraph<ListGraph<NodeIndexType, ArcIndexType>,
                                      NodeIndexType, ArcIndexType, false> {
  typedef MutableGraph<ListGraph<NodeIndexType, ArcIndexType>, NodeIndexType,
                       ArcIndexType, false>
      Base;
  using Base::arc_capacity_;
  using Base::const_capacities_;
  using Base::node_capacity_;
  using Base::num_arcs_;
  using Base::num_nodes_;

 public:
  using Builder = internal::MutableGraphBuilder<ListGraph, NodeIndexType,
                                                ArcIndexType, false>;
  using Base::IsArcValid;
  ListGraph() = default;

  // Reserve space for the graph at construction and do not allow it to grow
  // beyond that, see FreezeCapacities(). This constructor also makes any nodes
  // in [0, num_nodes) valid.
  ListGraph(NodeIndexType num_nodes, ArcIndexType arc_capacity) {
    this->Reserve(num_nodes, arc_capacity);
    this->FreezeCapacities();
    if (num_nodes > NodeIndexType(0)) {
      this->AddNode(num_nodes - NodeIndexType(1));
    }
  }

  // If node is not a valid node, sets num_nodes_ to node + 1 so that the given
  // node becomes valid. It will fail in DEBUG mode if the capacities are fixed
  // and the new node is out of range.
  void AddNode(NodeIndexType node) {
    if (node < num_nodes_) return;
    DCHECK(!const_capacities_ || node < node_capacity_);
    num_nodes_ = node + NodeIndexType(1);
    start_.resize(num_nodes_, Base::kNilArc);
  }

  // Adds an arc to the graph and returns its current index which will always
  // be num_arcs() - 1. It will also automatically call AddNode(tail)
  // and AddNode(head). It will fail in DEBUG mode if the capacities
  // are fixed and this cause the graph to grow beyond them.
  //
  // Note: Self referencing arcs and duplicate arcs are supported.
  ArcIndexType AddArc(NodeIndexType tail, NodeIndexType head) {
    DCHECK_GE(tail, NodeIndexType(0));
    DCHECK_GE(head, NodeIndexType(0));
    AddNode(tail > head ? tail : head);
    head_.push_back(head);
    tail_.push_back(tail);
    next_.push_back(start_[tail]);
    start_[tail] = num_arcs_;
    DCHECK(!const_capacities_ || num_arcs_ < arc_capacity_);
    return num_arcs_++;
  }

  // Returns the tail/head of a valid arc.
  NodeIndexType Tail(ArcIndexType arc) const {
    DCHECK(IsArcValid(arc));
    return tail_[arc];
  }

  NodeIndexType Head(ArcIndexType arc) const {
    DCHECK(IsArcValid(arc));
    return head_[arc];
  }

  // Do not use directly.
  struct OutgoingArcIteratorTag {};
  using OutgoingArcIterator =
      ChasingIterator<ArcIndexType, Base::kNilArc, OutgoingArcIteratorTag>;
  using OutgoingHeadIterator = ArcHeadIterator<ListGraph, OutgoingArcIterator>;

  // Graph jargon: the "degree" of a node is its number of arcs. The out-degree
  // is the number of outgoing arcs. The in-degree is the number of incoming
  // arcs, and is only available for some graph implementations, below.
  //
  // ListGraph<>::OutDegree() works in O(degree).
  ArcIndexType OutDegree(NodeIndexType node) const {
    ArcIndexType degree(0);
    for (auto arc ABSL_ATTRIBUTE_UNUSED : OutgoingArcs(node)) ++degree;
    return degree;
  }

  // Allows to iterate over the forward arcs that verify Tail(arc) == node.
  // This is meant to be used as:
  //   for (const ArcIndex arc : graph.OutgoingArcs(node)) { ... }
  BeginEndWrapper<OutgoingArcIterator> OutgoingArcs(NodeIndexType node) const {
    DCHECK(Base::IsNodeValid(node));
    return {OutgoingArcIterator(start_[node], next_.data()),
            OutgoingArcIterator()};
  }

  // Advanced usage. Same as OutgoingArcs(), but allows to restart the iteration
  // from an already known outgoing arc of the given node. If `from` is
  // `kNilArc`, an empty range is returned.
  BeginEndWrapper<OutgoingArcIterator> OutgoingArcsStartingFrom(
      NodeIndexType node, ArcIndexType from) const {
    DCHECK(Base::IsNodeValid(node));
    if (from == Base::kNilArc) return {};
    DCHECK_EQ(Tail(from), node);
    return {OutgoingArcIterator(from, next_.data()), OutgoingArcIterator()};
  }

  // This loops over the heads of the OutgoingArcs(node). It is just a more
  // convenient way to achieve this. Moreover this interface is used by some
  // graph algorithms.
  BeginEndWrapper<OutgoingHeadIterator> operator[](NodeIndexType node) const {
    return {OutgoingHeadIterator(*this, OutgoingArcs(node).begin()),
            OutgoingHeadIterator()};
  }

  void ReserveNodes(NodeIndexType bound) override {
    Base::ReserveNodes(bound);
    if (bound <= num_nodes_) return;
    start_.reserve(bound);
  }

  void ReserveArcs(ArcIndexType bound) override {
    Base::ReserveArcs(bound);
    if (bound <= num_arcs_) return;
    head_.reserve(bound);
    tail_.reserve(bound);
    next_.reserve(bound);
  }

  ABSL_DEPRECATED("Use `MutableGraphBuilder` instead")
  void Build(std::vector<ArcIndexType>* absl_nullable permutation) {
    if (permutation != nullptr) permutation->clear();
  }

 private:
  internal::Vector<NodeIndexType, ArcIndexType> start_;
  internal::Vector<ArcIndexType, ArcIndexType> next_;
  internal::Vector<ArcIndexType, NodeIndexType> head_;
  internal::Vector<ArcIndexType, NodeIndexType> tail_;
};

// Most efficient implementation of a graph without reverse arcs:
// - The graph is really compact memory wise:
//   ArcIndexType * node_capacity() + 2 * NodeIndexType * arc_capacity().
// - The construction is really fast.
//
// NOTE(user): if the need arises for very-well compressed graphs, we could
// shave NodeIndexType * arc_capacity() off the permanent memory requirement
// with a similar class that doesn't support Tail(), i.e.
// StaticGraphWithoutTail<>. This almost corresponds to a past implementation
// of StaticGraph<> @CL 116144340.
template <typename NodeIndexType = int32_t, typename ArcIndexType = int32_t>
class StaticGraph final
    : public BaseGraph<StaticGraph<NodeIndexType, ArcIndexType>, NodeIndexType,
                       ArcIndexType, false> {
  typedef BaseGraph<StaticGraph<NodeIndexType, ArcIndexType>, NodeIndexType,
                    ArcIndexType, false>
      Base;

 public:
  class Builder;

  using Base::IsArcValid;

  // Default constructor creates an empty graph.
  StaticGraph()
      : StaticGraph(internal::Vector<NodeIndexType, ArcIndexType>(
                        NodeIndexType(1), ArcIndexType(0)),
                    {}, {}) {}

  StaticGraph(const StaticGraph& other) = default;
  StaticGraph(StaticGraph&& other) = default;
  StaticGraph& operator=(const StaticGraph& other) = default;
  StaticGraph& operator=(StaticGraph&& other) = default;

  NodeIndexType Head(ArcIndexType arc) const {
    DCHECK(IsArcValid(arc));
    return head_[arc];
  }

  NodeIndexType Tail(ArcIndexType arc) const {
    DCHECK(IsArcValid(arc));
    return tail_[arc];
  }

  NodeIndexType num_nodes() const {
    DCHECK_GT(start_.size(), NodeIndexType(0));
    return start_.size() - NodeIndexType(1);
  }
  ArcIndexType num_arcs() const { return head_.size(); }

  ArcIndexType OutDegree(NodeIndexType node) const {
    return DirectArcLimit(node) - start_[node];  // Works in O(1).
  };

  IntegerRange<ArcIndexType> OutgoingArcs(NodeIndexType node) const {
    return IntegerRange<ArcIndexType>(start_[node], DirectArcLimit(node));
  }

  IntegerRange<ArcIndexType> OutgoingArcsStartingFrom(NodeIndexType node,
                                                      ArcIndexType from) const {
    DCHECK_GE(from, start_[node]);
    const ArcIndexType limit = DirectArcLimit(node);
    return IntegerRange<ArcIndexType>(from == Base::kNilArc ? limit : from,
                                      limit);
  }

  // This loops over the heads of the OutgoingArcs(node). It is just a more
  // convenient way to achieve this. Moreover this interface is used by some
  // graph algorithms.
  absl::Span<const NodeIndexType> operator[](NodeIndexType node) const {
    return absl::Span<const NodeIndexType>(
        head_.data() + static_cast<size_t>(start_[node]),
        static_cast<size_t>(DirectArcLimit(node) - start_[node]));
  }

 private:
  friend class Builder;

  StaticGraph(internal::Vector<NodeIndexType, ArcIndexType> start,
              internal::Vector<ArcIndexType, NodeIndexType> head,
              internal::Vector<ArcIndexType, NodeIndexType> tail)
      : start_(std::move(start)),
        head_(std::move(head)),
        tail_(std::move(tail)) {}

  ArcIndexType DirectArcLimit(NodeIndexType node) const {
    DCHECK(Base::IsNodeValid(node));
    return start_[node + NodeIndexType(1)];
  }

  // First outgoing arc for each node. If `num_nodes_ > 0`, the "past-the-end"
  // value is a sentinel (`start_[num_nodes_] == num_arcs_`).
  internal::Vector<NodeIndexType, ArcIndexType> start_;
  internal::Vector<ArcIndexType, NodeIndexType> head_;
  internal::Vector<ArcIndexType, NodeIndexType> tail_;
};

// Extends the ListGraph by also storing the reverse arcs.
// This class also documents the Graph interface related to reverse arc.
// - NodeIndexType can be unsigned, but ArcIndexType must be signed.
// - It has most of the same advantanges and disadvantages as ListGraph.
// - It takes 2 * ArcIndexType * node_capacity()
//   + 2 * (ArcIndexType + NodeIndexType) * arc_capacity() memory.
template <typename NodeIndexType = int32_t, typename ArcIndexType = int32_t>
class ReverseArcListGraph
    : public MutableGraph<ReverseArcListGraph<NodeIndexType, ArcIndexType>,
                          NodeIndexType, ArcIndexType, true> {
  static_assert(internal::IsSigned<ArcIndexType>(),
                "ArcIndexType must be signed");

  typedef MutableGraph<ReverseArcListGraph<NodeIndexType, ArcIndexType>,
                       NodeIndexType, ArcIndexType, true>
      Base;
  using Base::arc_capacity_;
  using Base::const_capacities_;
  using Base::node_capacity_;
  using Base::num_arcs_;
  using Base::num_nodes_;

 public:
  using Builder =
      internal::MutableGraphBuilder<ReverseArcListGraph, NodeIndexType,
                                    ArcIndexType, false>;
  using Base::IsArcValid;
  ReverseArcListGraph() = default;
  ReverseArcListGraph(NodeIndexType num_nodes, ArcIndexType arc_capacity) {
    this->Reserve(num_nodes, arc_capacity);
    this->FreezeCapacities();
    if (num_nodes > NodeIndexType(0)) {
      this->AddNode(num_nodes - NodeIndexType(1));
    }
  }

  NodeIndexType Head(ArcIndexType arc) const {
    DCHECK(IsArcValid(arc));
    return head_[arc];
  }

  NodeIndexType Tail(ArcIndexType arc) const { return head_[OppositeArc(arc)]; }

  // Returns the opposite arc of a given arc. That is the reverse arc of the
  // given forward arc or the forward arc of a given reverse arc.
  ArcIndexType OppositeArc(ArcIndexType arc) const {
    DCHECK(IsArcValid(arc));
    return ~arc;
  }

  // Do not use directly. See instead the arc iteration functions below.
  struct OutgoingArcIteratorTag {};
  using OutgoingArcIterator =
      ChasingIterator<ArcIndexType, Base::kNilArc, OutgoingArcIteratorTag>;
  struct OppositeIncomingArcIteratorTag {};
  using OppositeIncomingArcIterator =
      ChasingIterator<ArcIndexType, Base::kNilArc,
                      OppositeIncomingArcIteratorTag>;
  class OutgoingOrOppositeIncomingArcIterator;
  using OutgoingHeadIterator =
      ArcHeadIterator<ReverseArcListGraph, OutgoingArcIterator>;
  using IncomingArcIterator =
      ArcOppositeArcIterator<ReverseArcListGraph, OppositeIncomingArcIterator>;

  // ReverseArcListGraph<>::OutDegree() and ::InDegree() work in O(degree).
  ArcIndexType OutDegree(NodeIndexType node) const {
    ArcIndexType degree(0);
    for (auto arc ABSL_ATTRIBUTE_UNUSED : OutgoingArcs(node)) ++degree;
    return degree;
  }
  ArcIndexType InDegree(NodeIndexType node) const {
    ArcIndexType degree(0);
    for (auto arc ABSL_ATTRIBUTE_UNUSED : OppositeIncomingArcs(node)) ++degree;
    return degree;
  }

  // Arc iterations functions over the arcs touching a node (see the top-level
  // comment for the different types). To be used as follows:
  //   for (const Graph::ArcIndex arc : IterationFunction(node)) { ... }
  //
  // The StartingFrom() version are similar, but restart the iteration from a
  // given arc position (which must be valid in the iteration context), or
  // `kNilArc`, in which case an empty range is returned.
  BeginEndWrapper<OutgoingArcIterator> OutgoingArcs(NodeIndexType node) const {
    DCHECK(Base::IsNodeValid(node));
    return {OutgoingArcIterator(start_[node], next_.data()),
            OutgoingArcIterator()};
  }
  BeginEndWrapper<OutgoingArcIterator> OutgoingArcsStartingFrom(
      NodeIndexType node, ArcIndexType from) const {
    DCHECK(Base::IsNodeValid(node));
    if (from == Base::kNilArc) return {};
    DCHECK_GE(from, ArcIndexType(0));
    DCHECK_EQ(Tail(from), node);
    return {OutgoingArcIterator(from, next_.data()), OutgoingArcIterator()};
  }

  BeginEndWrapper<IncomingArcIterator> IncomingArcs(NodeIndexType node) const {
    return {IncomingArcIterator(*this, OppositeIncomingArcs(node).begin()),
            IncomingArcIterator()};
  }
  BeginEndWrapper<IncomingArcIterator> IncomingArcsStartingFrom(
      NodeIndexType node, ArcIndexType from) const {
    DCHECK(Base::IsNodeValid(node));
    if (from == Base::kNilArc) return {};
    return {
        IncomingArcIterator(
            *this,
            OppositeIncomingArcsStartingFrom(node, OppositeArc(from)).begin()),
        IncomingArcIterator()};
  }

  BeginEndWrapper<OutgoingOrOppositeIncomingArcIterator>
  OutgoingOrOppositeIncomingArcs(NodeIndexType node) const;
  BeginEndWrapper<OppositeIncomingArcIterator> OppositeIncomingArcs(
      NodeIndexType node) const {
    DCHECK(Base::IsNodeValid(node));
    return {OppositeIncomingArcIterator(reverse_start_[node], next_.data()),
            OppositeIncomingArcIterator()};
  }
  BeginEndWrapper<OppositeIncomingArcIterator> OppositeIncomingArcsStartingFrom(
      NodeIndexType node, ArcIndexType from) const {
    DCHECK(Base::IsNodeValid(node));
    if (from == Base::kNilArc) return {};
    DCHECK_LT(from, ArcIndexType(0));
    DCHECK_EQ(Tail(from), node);
    return {OppositeIncomingArcIterator(from, next_.data()),
            OppositeIncomingArcIterator()};
  }

  BeginEndWrapper<OutgoingOrOppositeIncomingArcIterator>
  OutgoingOrOppositeIncomingArcsStartingFrom(NodeIndexType node,
                                             ArcIndexType from) const;

  // This loops over the heads of the OutgoingArcs(node). It is just a more
  // convenient way to achieve this. Moreover this interface is used by some
  // graph algorithms.
  BeginEndWrapper<OutgoingHeadIterator> operator[](NodeIndexType node) const {
    const auto outgoing_arcs = OutgoingArcs(node);
    // Note: `BeginEndWrapper` is a borrowed range
    // (`std::ranges::borrowed_range`) so copying begin/end is safe.
    return BeginEndWrapper<OutgoingHeadIterator>(
        OutgoingHeadIterator(*this, outgoing_arcs.begin()),
        OutgoingHeadIterator(*this, outgoing_arcs.end()));
  }

  void ReserveNodes(NodeIndexType bound) {
    Base::ReserveNodes(bound);
    if (bound <= num_nodes_) return;
    start_.reserve(bound);
    reverse_start_.reserve(bound);
  }

  void ReserveArcs(ArcIndexType bound) {
    Base::ReserveArcs(bound);
    if (bound <= num_arcs_) return;
    head_.reserve(bound);
    next_.reserve(bound);
  }

  void AddNode(NodeIndexType node) {
    if (node < num_nodes_) return;
    DCHECK(!const_capacities_ || node < node_capacity_);
    num_nodes_ = node + NodeIndexType(1);
    start_.resize(num_nodes_, Base::kNilArc);
    reverse_start_.resize(num_nodes_, Base::kNilArc);
  }

  ArcIndexType AddArc(NodeIndexType tail, NodeIndexType head) {
    DCHECK_GE(tail, NodeIndexType(0));
    DCHECK_GE(head, NodeIndexType(0));
    AddNode(tail > head ? tail : head);
    head_.grow(tail, head);
    next_.grow(reverse_start_[head], start_[tail]);
    start_[tail] = num_arcs_;
    reverse_start_[head] = ~num_arcs_;
    DCHECK(!const_capacities_ || num_arcs_ < arc_capacity_);
    return num_arcs_++;
  }

  ABSL_DEPRECATED("Use `MutableGraphBuilder` instead")
  void Build(std::vector<ArcIndexType>* absl_nullable permutation) {
    if (permutation != nullptr) permutation->clear();
  }

 private:
  internal::Vector<NodeIndexType, ArcIndexType> start_;
  internal::Vector<NodeIndexType, ArcIndexType> reverse_start_;
  internal::SVector<ArcIndexType, ArcIndexType> next_;
  internal::SVector<ArcIndexType, NodeIndexType> head_;
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
template <typename NodeIndexType = int32_t, typename ArcIndexType = int32_t>
class ReverseArcStaticGraph final
    : public MutableGraph<ReverseArcStaticGraph<NodeIndexType, ArcIndexType>,
                          NodeIndexType, ArcIndexType, true> {
  static_assert(internal::IsSigned<ArcIndexType>(),
                "ArcIndexType must be signed");

  typedef MutableGraph<ReverseArcStaticGraph<NodeIndexType, ArcIndexType>,
                       NodeIndexType, ArcIndexType, true>
      Base;
  using Base::arc_capacity_;
  using Base::const_capacities_;
  using Base::node_capacity_;
  using Base::num_arcs_;
  using Base::num_nodes_;

 public:
  using Builder =
      internal::MutableGraphBuilder<ReverseArcStaticGraph, NodeIndexType,
                                    ArcIndexType, true>;
  using Base::IsArcValid;
  ReverseArcStaticGraph() : is_built_(false) {}
  ReverseArcStaticGraph(NodeIndexType num_nodes, ArcIndexType arc_capacity)
      : is_built_(false) {
    this->Reserve(num_nodes, arc_capacity);
    this->FreezeCapacities();
    if (num_nodes > NodeIndexType(0)) {
      this->AddNode(num_nodes - NodeIndexType(1));
    }
  }

  ArcIndexType OppositeArc(ArcIndexType arc) const {
    DCHECK(is_built_);
    DCHECK(IsArcValid(arc));
    return opposite_[arc];
  }

  NodeIndexType Head(ArcIndexType arc) const {
    DCHECK(is_built_);
    DCHECK(IsArcValid(arc));
    return head_[arc];
  }
  NodeIndexType Tail(ArcIndexType arc) const {
    DCHECK(is_built_);
    return head_[OppositeArc(arc)];
  }

  // ReverseArcStaticGraph<>::OutDegree() and ::InDegree() work in O(1).
  ArcIndexType OutDegree(NodeIndexType node) const {
    return DirectArcLimit(node) - start_[node];
  }
  ArcIndexType InDegree(NodeIndexType node) const {
    return ReverseArcLimit(node) - reverse_start_[node];
  }

  // Deprecated.
  class OutgoingOrOppositeIncomingArcIterator;
  using OppositeIncomingArcIterator = IntegerRangeIterator<ArcIndexType>;
  using IncomingArcIterator =
      ArcOppositeArcIterator<ReverseArcStaticGraph,
                             OppositeIncomingArcIterator>;
  using OutgoingArcIterator = IntegerRangeIterator<ArcIndexType>;

  IntegerRange<ArcIndexType> OutgoingArcs(NodeIndexType node) const {
    return IntegerRange<ArcIndexType>(start_[node], DirectArcLimit(node));
  }
  IntegerRange<ArcIndexType> OutgoingArcsStartingFrom(NodeIndexType node,
                                                      ArcIndexType from) const {
    DCHECK_GE(from, start_[node]);
    const ArcIndexType limit = DirectArcLimit(node);
    return IntegerRange<ArcIndexType>(from == Base::kNilArc ? limit : from,
                                      limit);
  }

  IntegerRange<ArcIndexType> OppositeIncomingArcs(NodeIndexType node) const {
    return IntegerRange<ArcIndexType>(reverse_start_[node],
                                      ReverseArcLimit(node));
  }
  IntegerRange<ArcIndexType> OppositeIncomingArcsStartingFrom(
      NodeIndexType node, ArcIndexType from) const {
    DCHECK_GE(from, reverse_start_[node]);
    const ArcIndexType limit = ReverseArcLimit(node);
    return IntegerRange<ArcIndexType>(from == Base::kNilArc ? limit : from,
                                      limit);
  }

  BeginEndWrapper<IncomingArcIterator> IncomingArcs(NodeIndexType node) const {
    const auto opposite_incoming_arcs = OppositeIncomingArcs(node);
    return {IncomingArcIterator(*this, opposite_incoming_arcs.begin()),
            IncomingArcIterator(*this, opposite_incoming_arcs.end())};
  }

  BeginEndWrapper<IncomingArcIterator> IncomingArcsStartingFrom(
      NodeIndexType node, ArcIndexType from) const {
    DCHECK(Base::IsNodeValid(node));
    const auto opposite_incoming_arcs = OppositeIncomingArcsStartingFrom(
        node, from == Base::kNilArc ? Base::kNilArc : OppositeArc(from));
    return {IncomingArcIterator(*this, opposite_incoming_arcs.begin()),
            IncomingArcIterator(*this, opposite_incoming_arcs.end())};
  }

  BeginEndWrapper<OutgoingOrOppositeIncomingArcIterator>
  OutgoingOrOppositeIncomingArcs(NodeIndexType node) const;

  BeginEndWrapper<OutgoingOrOppositeIncomingArcIterator>
  OutgoingOrOppositeIncomingArcsStartingFrom(NodeIndexType node,
                                             ArcIndexType from) const;

  // This loops over the heads of the OutgoingArcs(node). It is just a more
  // convenient way to achieve this. Moreover this interface is used by some
  // graph algorithms.
  absl::Span<const NodeIndexType> operator[](NodeIndexType node) const {
    return absl::Span<const NodeIndexType>(
        head_.data() + static_cast<size_t>(start_[node]),
        static_cast<size_t>(DirectArcLimit(node) - start_[node]));
  }

  void ReserveArcs(ArcIndexType bound) override {
    Base::ReserveArcs(bound);
    if (bound <= num_arcs_) return;
    head_.reserve(bound);
  }

  void AddNode(NodeIndexType node) {
    if (node < num_nodes_) return;
    DCHECK(!const_capacities_ || node < node_capacity_);
    num_nodes_ = node + NodeIndexType(1);
  }

  ArcIndexType AddArc(NodeIndexType tail, NodeIndexType head) {
    DCHECK_GE(tail, NodeIndexType(0));
    DCHECK_GE(head, NodeIndexType(0));
    AddNode(tail > head ? tail : head);

    // We inverse head and tail here because it is more convenient this way
    // during build time, see Build().
    head_.grow(head, tail);
    DCHECK(!const_capacities_ || num_arcs_ < arc_capacity_);
    return num_arcs_++;
  }

  ABSL_DEPRECATED("Use `Builder` instead")
  void Build(std::vector<ArcIndexType>* absl_nullable permutation);

 private:
  ArcIndexType DirectArcLimit(NodeIndexType node) const {
    DCHECK(is_built_);
    DCHECK(Base::IsNodeValid(node));
    return start_[node + NodeIndexType(1)];
  }
  ArcIndexType ReverseArcLimit(NodeIndexType node) const {
    DCHECK(is_built_);
    DCHECK(Base::IsNodeValid(node));
    return reverse_start_[node + NodeIndexType(1)];
  }

  bool is_built_;
  // First outgoing arc for each node. If `num_nodes_ > 0`, the "past-the-end"
  // value is a sentinel (`start_[num_nodes_] == num_arcs_`).
  internal::Vector<NodeIndexType, ArcIndexType> start_;
  // First reverse outgoing arc for each node. If `num_nodes_ > 0`,
  // the "past-the-end" value is a sentinel (`reverse_start_[num_nodes_] == 0`).
  internal::Vector<NodeIndexType, ArcIndexType> reverse_start_;
  internal::SVector<ArcIndexType, NodeIndexType> head_;
  internal::SVector<ArcIndexType, ArcIndexType> opposite_;
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
template <class IntVector, class Array>
void Permute(const IntVector& permutation, Array* array_to_permute) {
  if (permutation.empty()) {
    return;
  }
  const auto size = permutation.size();
  auto& array = *array_to_permute;
  using ElementType =
      typename std::iterator_traits<decltype(std::begin(array))>::value_type;
  std::vector<ElementType> temp(size);
  auto array_begin = std::begin(array);
  std::move(array_begin, array_begin + size, temp.begin());
  for (size_t i = 0; i < permutation.size(); ++i) {
    *(array_begin + static_cast<size_t>(permutation[i])) = std::move(temp[i]);
  }
}

namespace internal {

template <typename BuilderImpl, typename GraphImpl, typename NodeIndexType,
          typename ArcIndexType>
template <typename... ArcPropertyArray>
std::unique_ptr<GraphImpl> GraphBuilderBase<
    BuilderImpl, GraphImpl, NodeIndexType,
    ArcIndexType>::BuildAndPermute(ArcPropertyArray&... arc_properties) && {
  if constexpr (BuilderImpl::kBuildPermutesArcs) {
    std::vector<ArcIndexType> permutation;
    auto graph =
        static_cast<BuilderImpl&&>(std::move(*this)).Build(&permutation);
    (Permute(permutation, &arc_properties), ...);
    return graph;
  } else {
    // Optimization: avoid permuting with an identity permutation.
    return static_cast<BuilderImpl&&>(std::move(*this)).Build(nullptr);
  }
}

}  // namespace internal

// BaseGraph implementation ----------------------------------------------------

template <typename Impl, typename NodeIndexType, typename ArcIndexType,
          bool HasNegativeReverseArcs>
IntegerRange<NodeIndexType>
BaseGraph<Impl, NodeIndexType, ArcIndexType, HasNegativeReverseArcs>::AllNodes()
    const {
  return IntegerRange<NodeIndexType>(NodeIndexType(0), num_nodes());
}

template <typename Impl, typename NodeIndexType, typename ArcIndexType,
          bool HasNegativeReverseArcs>
IntegerRange<ArcIndexType> BaseGraph<Impl, NodeIndexType, ArcIndexType,
                                     HasNegativeReverseArcs>::AllForwardArcs()
    const {
  return IntegerRange<ArcIndexType>(ArcIndexType(0), num_arcs());
}

template <typename Impl, typename NodeIndexType, typename ArcIndexType,
          bool HasNegativeReverseArcs>
NodeIndexType MutableGraph<Impl, NodeIndexType, ArcIndexType,
                           HasNegativeReverseArcs>::node_capacity() const {
  // TODO(user): Is it needed? remove completely? return the real capacities
  // at the cost of having a different implementation for each graphs?
  return node_capacity_ > num_nodes_ ? node_capacity_ : num_nodes_;
}

template <typename Impl, typename NodeIndexType, typename ArcIndexType,
          bool HasNegativeReverseArcs>
ArcIndexType MutableGraph<Impl, NodeIndexType, ArcIndexType,
                          HasNegativeReverseArcs>::arc_capacity() const {
  // TODO(user): Same questions as the ones in node_capacity().
  return arc_capacity_ > num_arcs_ ? arc_capacity_ : num_arcs_;
}

template <typename Impl, typename NodeIndexType, typename ArcIndexType,
          bool HasNegativeReverseArcs>
void MutableGraph<Impl, NodeIndexType, ArcIndexType,
                  HasNegativeReverseArcs>::FreezeCapacities() {
  // TODO(user): Only define this in debug mode at the cost of having a lot
  // of ifndef NDEBUG all over the place? remove the function completely ?
  const_capacities_ = true;
  node_capacity_ = std::max(node_capacity_, num_nodes_);
  arc_capacity_ = std::max(arc_capacity_, num_arcs_);
}

namespace graph_internal {

// Computes the cumulative sum of the entries in v.
template <typename NodeIndexType, typename ArcIndexType>
void ComputeCumulativeSum(internal::Vector<NodeIndexType, ArcIndexType>* v) {
  DCHECK(!v->empty());
  const auto num_nodes = v->size() - NodeIndexType(1);
  ArcIndexType sum(0);
  for (NodeIndexType i(0); i < num_nodes; ++i) {
    ArcIndexType temp = (*v)[i];
    (*v)[i] = sum;
    sum += temp;
  }
  (*v)[num_nodes] = sum;  // Sentinel.
}

}  // namespace graph_internal

// Given the tail of arc #i in (*head)[i] and the head of arc #i in (*head)[~i]
// - Reorder the arc by increasing tail.
// - Put the head of the new arc #i in (*head)[i].
// - Put in start[i] the index of the first arc with tail >= i.
// - Update "permutation" to reflect the change, unless it is NULL.
template <typename Impl, typename NodeIndexType, typename ArcIndexType,
          bool HasNegativeReverseArcs>
void BaseGraph<Impl, NodeIndexType, ArcIndexType, HasNegativeReverseArcs>::
    BuildStartAndForwardHead(
        internal::SVector<ArcIndexType, NodeIndexType>* head,
        internal::Vector<NodeIndexType, ArcIndexType>* start,
        std::vector<ArcIndexType>* permutation) {
  // Computes the outgoing degree of each nodes and check if we need to permute
  // something or not. Note that the tails are currently stored in the positive
  // range of the SVector head.
  const auto num_nodes = this->num_nodes();
  const auto num_arcs = this->num_arcs();
  start->assign(num_nodes + NodeIndexType(1), ArcIndexType(0));
  NodeIndexType last_tail_seen(0);
  bool permutation_needed = false;
  for (ArcIndexType i(0); i < num_arcs; ++i) {
    NodeIndexType tail = (*head)[i];
    if (!permutation_needed) {
      permutation_needed = tail < last_tail_seen;
      last_tail_seen = tail;
    }
    (*start)[tail]++;
  }
  graph_internal::ComputeCumulativeSum(start);
  DCHECK_EQ(start->back(), num_arcs);

  // Abort early if we do not need the permutation: we only need to put the
  // heads in the positive range.
  if (!permutation_needed) {
    for (ArcIndexType i(0); i < num_arcs; ++i) {
      (*head)[i] = (*head)[~i];
    }
    if (permutation != nullptr) {
      permutation->clear();
    }
    return;
  }

  // Computes the forward arc permutation.
  // Note that this temporarily alters the start vector.
  std::vector<ArcIndexType> perm(static_cast<size_t>(num_arcs));
  for (ArcIndexType i(0); i < num_arcs; ++i) {
    perm[static_cast<size_t>(i)] = (*start)[(*head)[i]]++;
  }

  // Restore in (*start)[i] the index of the first arc with tail >= i.
  DCHECK_GE(num_nodes, NodeIndexType(1));
  for (NodeIndexType i = num_nodes - NodeIndexType(1); i > NodeIndexType(0);
       --i) {
    (*start)[i] = (*start)[i - NodeIndexType(1)];
  }
  (*start)[NodeIndexType(0)] = ArcIndexType(0);

  // Permutes the head into their final position in head.
  // We do not need the tails anymore at this point.
  for (ArcIndexType i(0); i < num_arcs; ++i) {
    (*head)[perm[static_cast<size_t>(i)]] = (*head)[~i];
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
// - t: the iteration type (Outgoing, Incoming, OutgoingOrOppositeIncoming
//      or OppositeIncoming).
// - e: the "end" ArcIndexType.
#define DEFINE_RANGE_BASED_ARC_ITERATION(c, t)                             \
  template <typename NodeIndexType, typename ArcIndexType>                 \
  BeginEndWrapper<typename c<NodeIndexType, ArcIndexType>::t##ArcIterator> \
      c<NodeIndexType, ArcIndexType>::t##Arcs(NodeIndexType node) const {  \
    return BeginEndWrapper<t##ArcIterator>(                                \
        t##ArcIterator(*this, node),                                       \
        t##ArcIterator(*this, node, Base::kNilArc));                       \
  }                                                                        \
  template <typename NodeIndexType, typename ArcIndexType>                 \
  BeginEndWrapper<typename c<NodeIndexType, ArcIndexType>::t##ArcIterator> \
      c<NodeIndexType, ArcIndexType>::t##ArcsStartingFrom(                 \
          NodeIndexType node, ArcIndexType from) const {                   \
    return BeginEndWrapper<t##ArcIterator>(                                \
        t##ArcIterator(*this, node, from),                                 \
        t##ArcIterator(*this, node, Base::kNilArc));                       \
  }

// StaticGraph implementation --------------------------------------------------

template <typename NodeIndexType, typename ArcIndexType>
class StaticGraph<NodeIndexType, ArcIndexType>::Builder
    : public internal::GraphBuilderBase<Builder, StaticGraph, NodeIndexType,
                                        ArcIndexType> {
  using Base = internal::GraphBuilderBase<Builder, StaticGraph, NodeIndexType,
                                          ArcIndexType>;

 public:
  static constexpr bool kBuildPermutesArcs = true;

  Builder()
      : const_capacities_(false), start_(NodeIndexType(1), ArcIndexType(0)) {}

  Builder(NodeIndexType num_nodes, ArcIndexType arc_capacity)
      : const_capacities_(true),
        num_nodes_(num_nodes),
        start_(num_nodes + NodeIndexType(1), ArcIndexType(0)) {
    head_.reserve(arc_capacity);
    tail_.reserve(arc_capacity);
  }

  NodeIndexType num_nodes() const { return num_nodes_; }
  ArcIndexType num_arcs() const { return head_.size(); }

  void ReserveNodes(NodeIndexType bound) {
    if (bound <= num_nodes_) return;
    start_.reserve(bound + NodeIndexType(1));
  }

  void ReserveArcs(ArcIndexType bound) {
    if (bound <= num_arcs()) return;
    head_.reserve(bound);
    tail_.reserve(bound);
  }

  void AddNode(NodeIndexType node) {
    if (node < num_nodes_) return;
    DCHECK(!const_capacities_) << node;
    num_nodes_ = node + NodeIndexType(1);
    start_.resize(num_nodes_ + NodeIndexType(1), ArcIndexType(0));
  }

  ArcIndexType AddArc(NodeIndexType tail, NodeIndexType head) {
    DCHECK_GE(tail, NodeIndexType(0));
    DCHECK_GE(head, NodeIndexType(0));
    AddNode(tail > head ? tail : head);
    if (arc_in_order_) {
      if (tail >= last_tail_seen_) {
        start_[tail]++;
        last_tail_seen_ = tail;
      } else {
        arc_in_order_ = false;
      }
    }
    const ArcIndexType arc(tail_.size());
    tail_.push_back(tail);
    head_.push_back(head);
    return arc;
  }

  // Implementation details: A reader may be surprised that we do many passes
  // into the data where things could be done in one pass. For instance, during
  // construction, we store the edges first, and then do a second pass at the
  // end to compute the degree distribution.
  //
  // This is because it is a lot more efficient cache-wise to do it this way.
  // This was determined by various experiments, but can also be understood:
  // - during repetitive call to AddArc() a client usually accesses various
  //   areas of memory, and there is no reason to pollute the cache with
  //   possibly random access to degree[i].
  // - When the degrees are needed, we compute them in one go, maximizing the
  //   chance of cache hit during the computation.
  std::unique_ptr<StaticGraph<NodeIndexType, ArcIndexType>> Build(
      std::vector<ArcIndexType>* permutation) {
    const NodeIndexType num_nodes = num_nodes_;
    const ArcIndexType num_arcs = this->num_arcs();
    if (num_nodes == NodeIndexType(0)) {
      return absl::WrapUnique(new StaticGraph());
    }

    // If Arc are in order, start_ already contains the degree distribution.
    if (arc_in_order_) {
      if (permutation != nullptr) {
        permutation->clear();
      }
      graph_internal::ComputeCumulativeSum(&start_);
      DCHECK_EQ(start_.back(), num_arcs);
      return absl::WrapUnique(new StaticGraph(
          std::move(start_), std::move(head_), std::move(tail_)));
    }

    // Computes outgoing degree of each nodes. We have to clear start, since
    // at least the first arc was processed with arc_in_order_ == true.
    start_.assign(num_nodes + NodeIndexType(1), ArcIndexType(0));
    for (ArcIndexType i(0); i < num_arcs; ++i) {
      start_[tail_[i]]++;
    }
    graph_internal::ComputeCumulativeSum(&start_);
    DCHECK_EQ(start_.back(), num_arcs);

    // Computes the forward arc permutation.
    // Note that this temporarily alters the start_ vector.
    std::vector<ArcIndexType> perm(static_cast<size_t>(num_arcs));
    for (ArcIndexType i(0); i < num_arcs; ++i) {
      perm[static_cast<size_t>(i)] = start_[tail_[i]]++;
    }

    // We use "tail_" (which now contains rubbish) to permute "head_" faster.
    CHECK_EQ(tail_.size(), num_arcs);
    tail_.swap(head_);
    for (ArcIndexType i(0); i < num_arcs; ++i) {
      head_[perm[static_cast<size_t>(i)]] = tail_[i];
    }

    if (permutation != nullptr) {
      permutation->swap(perm);
    }

    // Restore in start_[i] the index of the first arc with tail >= i.
    DCHECK_GE(num_nodes, NodeIndexType(1));
    for (NodeIndexType i = num_nodes - NodeIndexType(1); i > NodeIndexType(0);
         --i) {
      start_[i] = start_[i - NodeIndexType(1)];
    }
    start_[NodeIndexType(0)] = ArcIndexType(0);

    // Recompute the correct tail_ vector
    for (NodeIndexType node(0); node < num_nodes; ++node) {
      const ArcIndexType start = start_[node];
      const ArcIndexType end = start_[node + NodeIndexType(1)];
      for (ArcIndexType arc = start; arc < end; ++arc) {
        tail_[arc] = node;
      }
    }

    return absl::WrapUnique(
        new StaticGraph(std::move(start_), std::move(head_), std::move(tail_)));
  }

 private:
  NodeIndexType node_capacity() const {
    if (start_.empty()) return NodeIndexType(0);
    return start_.capacity() - 1;
  }
  ArcIndexType arc_capacity() const { return head_.capacity(); }

  bool const_capacities_;
  bool arc_in_order_ = true;
  NodeIndexType last_tail_seen_ = NodeIndexType(0);

  NodeIndexType num_nodes_ = NodeIndexType(0);

  internal::Vector<NodeIndexType, ArcIndexType> start_;
  // TODO(user): Experiment with using `vector<TmpArc>` like for `FlowGraph`.
  internal::Vector<ArcIndexType, NodeIndexType> head_;
  internal::Vector<ArcIndexType, NodeIndexType> tail_;
};

// ReverseArcListGraph implementation ------------------------------------------
DEFINE_RANGE_BASED_ARC_ITERATION(ReverseArcListGraph,
                                 OutgoingOrOppositeIncoming);

template <typename NodeIndexType, typename ArcIndexType>
class ReverseArcListGraph<NodeIndexType,
                          ArcIndexType>::OutgoingOrOppositeIncomingArcIterator {
 public:
  using difference_type =
      decltype(iterators_internal::GetValue(ArcIndexType(0)));
  using value_type = ArcIndexType;
  using iterator_category = std::input_iterator_tag;
  using pointer = void;
  using reference = void;

  OutgoingOrOppositeIncomingArcIterator(const ReverseArcListGraph& graph,
                                        NodeIndexType node)
      : graph_(&graph), index_(graph.reverse_start_[node]), node_(node) {
    DCHECK(graph.IsNodeValid(node));
    if (index_ == Base::kNilArc) index_ = graph.start_[node];
  }
  OutgoingOrOppositeIncomingArcIterator(const ReverseArcListGraph& graph,
                                        NodeIndexType node, ArcIndexType arc)
      : graph_(&graph), index_(arc), node_(node) {
    DCHECK(graph.IsNodeValid(node));
    DCHECK(arc == Base::kNilArc || graph.Tail(arc) == node);
  }

  ArcIndexType operator*() const { return index_; }

  friend bool operator==(const OutgoingOrOppositeIncomingArcIterator& l,
                         const OutgoingOrOppositeIncomingArcIterator& r) {
    return l.index_ == r.index_;
  }

  friend bool operator!=(const OutgoingOrOppositeIncomingArcIterator& l,
                         const OutgoingOrOppositeIncomingArcIterator& r) {
    return l.index_ != r.index_;
  }

  OutgoingOrOppositeIncomingArcIterator& operator++() {
    DCHECK(index_ != Base::kNilArc);
    if (index_ < ArcIndexType(0)) {
      index_ = graph_->next_[index_];
      if (index_ == Base::kNilArc) {
        index_ = graph_->start_[node_];
      }
    } else {
      index_ = graph_->next_[index_];
    }
    return *this;
  }

  OutgoingOrOppositeIncomingArcIterator operator++(int) {
    OutgoingOrOppositeIncomingArcIterator tmp = *this;
    ++(*this);
    return tmp;
  }

  // TODO(user): Remove all uses.
  bool Ok() const { return index_ != Base::kNilArc; }
  ArcIndexType Index() const { return index_; }
  void Next() { ++*this; }

 private:
  const ReverseArcListGraph* graph_;
  ArcIndexType index_;
  NodeIndexType node_;
};

// ReverseArcStaticGraph implementation ----------------------------------------

DEFINE_RANGE_BASED_ARC_ITERATION(ReverseArcStaticGraph,
                                 OutgoingOrOppositeIncoming);

template <typename NodeIndexType, typename ArcIndexType>
void ReverseArcStaticGraph<NodeIndexType, ArcIndexType>::Build(
    std::vector<ArcIndexType>* permutation) {
  DCHECK(!is_built_);
  if (is_built_) return;
  is_built_ = true;
  node_capacity_ = num_nodes_;
  arc_capacity_ = num_arcs_;
  this->FreezeCapacities();
  if (num_nodes_ == NodeIndexType(0)) {
    return;
  }
  this->BuildStartAndForwardHead(&head_, &start_, permutation);

  // Computes incoming degree of each nodes.
  reverse_start_.assign(num_nodes_ + NodeIndexType(1), ArcIndexType(0));
  for (ArcIndexType i(0); i < num_arcs_; ++i) {
    reverse_start_[head_[i]]++;
  }
  graph_internal::ComputeCumulativeSum(&reverse_start_);
  DCHECK_EQ(reverse_start_.back(), num_arcs_);

  // Computes the reverse arcs of the forward arcs.
  // Note that this sort the reverse arcs with the same tail by head.
  opposite_.reserve(num_arcs_);
  for (ArcIndexType i(0); i < num_arcs_; ++i) {
    // TODO(user): the 0 is wasted here, but minor optimisation.
    opposite_.grow(ArcIndexType(0), reverse_start_[head_[i]]++ - num_arcs_);
  }

  // Computes in reverse_start_ the start index of the reverse arcs.
  DCHECK_GE(num_nodes_, NodeIndexType(1));
  reverse_start_[num_nodes_] = ArcIndexType(0);  // Sentinel.
  for (NodeIndexType i = num_nodes_ - NodeIndexType(1); i > NodeIndexType(0);
       --i) {
    reverse_start_[i] = reverse_start_[i - NodeIndexType(1)] - num_arcs_;
  }
  if (num_nodes_ != NodeIndexType(0)) {
    reverse_start_[NodeIndexType(0)] = -num_arcs_;
  }

  // Fill reverse arc information.
  for (ArcIndexType i(0); i < num_arcs_; ++i) {
    opposite_[opposite_[i]] = i;
  }
  for (const NodeIndexType node : Base::AllNodes()) {
    for (const ArcIndexType arc : OutgoingArcs(node)) {
      head_[opposite_[arc]] = node;
    }
  }
}

template <typename NodeIndexType, typename ArcIndexType>
class ReverseArcStaticGraph<
    NodeIndexType, ArcIndexType>::OutgoingOrOppositeIncomingArcIterator {
 public:
  using difference_type =
      decltype(iterators_internal::GetValue(ArcIndexType(0)));
  using value_type = ArcIndexType;
  using iterator_category = std::input_iterator_tag;
  using pointer = void;
  using reference = void;

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
      : first_limit_(graph.ReverseArcLimit(node)),
        next_start_(graph.start_[node]),
        limit_(graph.DirectArcLimit(node)) {
    index_ = arc == Base::kNilArc ? limit_ : arc;
    DCHECK(graph.IsNodeValid(node));
    DCHECK((index_ >= graph.reverse_start_[node] && index_ < first_limit_) ||
           (index_ >= next_start_));
  }

  ArcIndexType operator*() const { return index_; }

  friend bool operator==(const OutgoingOrOppositeIncomingArcIterator& l,
                         const OutgoingOrOppositeIncomingArcIterator& r) {
    return l.index_ == r.index_;
  }

  friend bool operator!=(const OutgoingOrOppositeIncomingArcIterator& l,
                         const OutgoingOrOppositeIncomingArcIterator& r) {
    return l.index_ != r.index_;
  }

  OutgoingOrOppositeIncomingArcIterator& operator++() {
    index_++;
    if (index_ == first_limit_) {
      index_ = next_start_;
    }
    return *this;
  }

  OutgoingOrOppositeIncomingArcIterator operator++(int) {
    OutgoingOrOppositeIncomingArcIterator tmp = *this;
    ++(*this);
    return tmp;
  }

  // TODO(user): Remove all uses.
  bool Ok() const { return index_ != limit_; }
  ArcIndexType Index() const { return index_; }
  void Next() { ++*this; }

 private:
  ArcIndexType index_;
  const ArcIndexType first_limit_;
  const ArcIndexType next_start_;
  const ArcIndexType limit_;
};

// CompleteGraph implementation ------------------------------------------------
// Nodes and arcs are implicit and not stored.

namespace graph_internal {

// An unsigned type with the same bit-width as the given `ArcIndexType`, which
// may be a signed or unsigned integral type, or a StrongInt.
// Ideally this would be a template type alias, we're using a template struct
// with an inner type alias to work around a parsing bug in gcc11:
// https://godbolt.org/z/TrYhTzYrW.
template <typename ArcIndexType>
struct UnsignedTypeOfSameWidth {
  using type = decltype([](auto x) {
    using T = decltype(x);
    if constexpr (std::is_integral_v<T>) {
      return std::make_unsigned_t<T>(x);
    } else {
      return std::make_unsigned_t<typename T::ValueType>(x);
    }
  }(ArcIndexType(0)));
};

}  // namespace graph_internal

template <typename NodeIndexType = int32_t, typename ArcIndexType = int32_t>
class CompleteGraph
    : public BaseGraph<CompleteGraph<NodeIndexType, ArcIndexType>,
                       NodeIndexType, ArcIndexType, false> {
 public:
  // Builds a complete graph with num_nodes nodes.
  explicit CompleteGraph(NodeIndexType num_nodes)
      : num_nodes_(num_nodes),
        // If there are 0 or 1 nodes, the divisor is arbitrary. We pick 2 as 0
        // and 1 are not supported by `ConstantDivisor`.
        divisor_(num_nodes > NodeIndexType(1) ? InternalType(num_nodes)
                                              : InternalType(2)) {}

  NodeIndexType num_nodes() const { return NodeIndexType(num_nodes_); }

  ArcIndexType num_arcs() const {
    return ArcIndexType(num_nodes_ * num_nodes_);
  }

  NodeIndexType Head(ArcIndexType arc) const {
    DCHECK(this->IsArcValid(arc));
    return NodeIndexType(InternalType(arc) % divisor_);
  }

  NodeIndexType Tail(ArcIndexType arc) const {
    DCHECK(this->IsArcValid(arc));
    return NodeIndexType(InternalType(arc) / divisor_);
  }

  ArcIndexType OutDegree(NodeIndexType) const {
    return ArcIndexType(num_nodes_);
  }

  IntegerRange<ArcIndexType> OutgoingArcs(NodeIndexType node) const {
    const InternalType node_internal(node);
    DCHECK_LT(node_internal, num_nodes_);
    return IntegerRange<ArcIndexType>(
        ArcIndexType(num_nodes_ * node_internal),
        ArcIndexType(num_nodes_ * (node_internal + 1)));
  }

  IntegerRange<ArcIndexType> OutgoingArcsStartingFrom(NodeIndexType node,
                                                      ArcIndexType from) const {
    const InternalType node_internal(node);
    DCHECK_LT(node_internal, num_nodes_);
    return IntegerRange<ArcIndexType>(
        from, ArcIndexType(num_nodes_ * (node_internal + 1)));
  }

  IntegerRange<NodeIndexType> operator[](NodeIndexType node) const {
    DCHECK_LT(InternalType(node), num_nodes_);
    return IntegerRange<NodeIndexType>(NodeIndexType(0),
                                       NodeIndexType(num_nodes_));
  }

 private:
  using InternalType =
      typename graph_internal::UnsignedTypeOfSameWidth<ArcIndexType>::type;

  const InternalType num_nodes_;
  const ::util::math::ConstantDivisor<InternalType> divisor_;
};

// CompleteBipartiteGraph implementation ---------------------------------------
// Nodes and arcs are implicit and not stored.

template <typename NodeIndexType = int32_t, typename ArcIndexType = int32_t>
class CompleteBipartiteGraph
    : public BaseGraph<CompleteBipartiteGraph<NodeIndexType, ArcIndexType>,
                       NodeIndexType, ArcIndexType, false> {
 public:
  // Builds a complete bipartite graph from a set of left nodes to a set of
  // right nodes.
  // Indices of left nodes of the bipartite graph range from 0 to left_nodes-1;
  // indices of right nodes range from left_nodes to left_nodes+right_nodes-1.
  CompleteBipartiteGraph(NodeIndexType left_nodes, NodeIndexType right_nodes)
      : left_nodes_(left_nodes),
        right_nodes_(right_nodes),
        // If there are no right nodes, the divisor is arbitrary. We pick 2 as
        // 0 and 1 are not supported by `ConstantDivisor`. We handle the case
        // where `right_nodes` is 1 explicitly when dividing.
        divisor_(right_nodes > NodeIndexType(1) ? InternalType(right_nodes)
                                                : InternalType(2)) {}

  NodeIndexType num_nodes() const { return left_nodes_ + right_nodes_; }

  ArcIndexType num_arcs() const {
    return ArcIndexType(InternalType(left_nodes_) * InternalType(right_nodes_));
  }

  // Returns the arc index for the arc from `left` to `right`, where `left` is
  // in `[0, left_nodes)` and `right` is in
  // `[left_nodes, left_nodes + right_nodes)`.
  ArcIndexType GetArc(NodeIndexType left_node, NodeIndexType right_node) const {
    DCHECK_LT(left_node, left_nodes_);
    DCHECK_GE(right_node, left_nodes_);
    DCHECK_LT(right_node, num_nodes());
    return ArcIndexType(InternalType(left_node) * InternalType(right_nodes_)) +
           ArcIndexType(right_node - left_nodes_);
  }

  NodeIndexType Head(ArcIndexType arc) const {
    DCHECK(this->IsArcValid(arc));
    // See comment on `divisor_` in the constructor.
    return right_nodes_ > NodeIndexType(1)
               ? left_nodes_ + NodeIndexType(InternalType(arc) % divisor_)
               : left_nodes_;
  }

  NodeIndexType Tail(ArcIndexType arc) const {
    DCHECK(this->IsArcValid(arc));
    // See comment on `divisor_` in the constructor.
    return right_nodes_ > NodeIndexType(1)
               ? NodeIndexType(InternalType(arc) / divisor_)
               : NodeIndexType(arc);
  }

  ArcIndexType OutDegree(NodeIndexType node) const {
    return (node < left_nodes_) ? ArcIndexType(InternalType(right_nodes_))
                                : ArcIndexType(0);
  }

  IntegerRange<ArcIndexType> OutgoingArcs(NodeIndexType node) const {
    if (node < left_nodes_) {
      const InternalType node_internal(node);
      return IntegerRange<ArcIndexType>(
          ArcIndexType(InternalType(right_nodes_) * node_internal),
          ArcIndexType(InternalType(right_nodes_) * (node_internal + 1)));
    } else {
      return IntegerRange<ArcIndexType>(ArcIndexType(0), ArcIndexType(0));
    }
  }

  IntegerRange<ArcIndexType> OutgoingArcsStartingFrom(NodeIndexType node,
                                                      ArcIndexType from) const {
    if (node < left_nodes_) {
      const InternalType node_internal(node);
      return IntegerRange<ArcIndexType>(
          from, ArcIndexType(InternalType(right_nodes_) * (node_internal + 1)));
    } else {
      return IntegerRange<ArcIndexType>(ArcIndexType(0), ArcIndexType(0));
    }
  }

  IntegerRange<NodeIndexType> operator[](NodeIndexType node) const {
    if (node < left_nodes_) {
      return IntegerRange<NodeIndexType>(left_nodes_,
                                         left_nodes_ + right_nodes_);
    } else {
      return IntegerRange<NodeIndexType>(NodeIndexType(0), NodeIndexType(0));
    }
  }

 private:
  using InternalType =
      typename graph_internal::UnsignedTypeOfSameWidth<ArcIndexType>::type;

  const NodeIndexType left_nodes_;
  const NodeIndexType right_nodes_;
  // Note: only valid if `right_nodes_ > 1`.
  const ::util::math::ConstantDivisor<InternalType> divisor_;
};

// Creates a graph from a container of arcs.
template <typename GraphT,
          typename ArcContainer = std::initializer_list<std::pair<
              typename GraphT::NodeIndex, typename GraphT::NodeIndex>>>
GraphT GraphFromArcs(typename GraphT::NodeIndex num_nodes,
                     const ArcContainer& arcs) {
  typename GraphT::Builder builder(num_nodes,
                                   typename GraphT::ArcIndex(arcs.size()));
  for (const auto& [from, to] : arcs) builder.AddArc(from, to);
  return std::move(builder).BuildGraph(nullptr);
}

}  // namespace util

#undef DEFINE_RANGE_BASED_ARC_ITERATION

#endif  // UTIL_GRAPH_GRAPH_H_
