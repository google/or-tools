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

#ifndef OR_TOOLS_GRAPH_EBERT_GRAPH_H_
#define OR_TOOLS_GRAPH_EBERT_GRAPH_H_

// A few variations on a theme of the "star" graph representation by
// Ebert, as described in J. Ebert, "A versatile data structure for
// edge-oriented graph algorithms."  Communications of the ACM
// 30(6):513-519 (June 1987).
// http://portal.acm.org/citation.cfm?id=214769
//
// In this file there are three representations that have much in
// common. The general one, called simply EbertGraph, contains both
// forward- and backward-star representations. The other, called
// ForwardEbertGraph, contains only the forward-star representation of
// the graph, and is appropriate for applications where the reverse
// arcs are not needed.
//
// The point of including all the representations in this one file is
// to capitalize, where possible, on the commonalities among them, and
// those commonalities are mostly factored out into base classes as
// described below. Despite the commonalities, however, each of the
// three representations presents a somewhat different interface
// because of their different underlying semantics. A quintessential
// example is that the AddArc() method, very natural for the
// EbertGraph representation, cannot exist for an inherently static
// representation like ForwardStaticGraph.
//
// Many clients are expected to use the interfaces to the graph
// objects directly, but some clients are parameterized by graph type
// and need a consistent interface for their underlying graph
// objects. For such clients, a small library of class templates is
// provided to give a consistent interface to clients where the
// underlying graph interfaces differ. Examples are the
// AnnotatedGraphBuildManager<> template, which provides a uniform
// interface for building the various types of graphs; and the
// TailArrayManager<> template, which provides a uniform interface for
// applications that need to map from arc indices to arc tail nodes,
// accounting for the fact that such a mapping has to be requested
// explicitly from the ForwardStaticGraph and ForwardStarGraph
// representations.
//
// There are two base class templates, StarGraphBase, and
// EbertGraphBase; their purpose is to hold methods and data
// structures that are in common among their descendants. Only classes
// that are leaves in the following hierarchy tree are eligible for
// free-standing instantiation and use by clients. The parentheses
// around StarGraphBase and EbertGraphBase indicate that they should
// not normally be instantiated by clients:
//
//                     (StarGraphBase)                       |
//                       /         \                         |
//                      /           \                        |
//                     /             \                       |
//                    /               \                      |
//           (EbertGraphBase)     ForwardStaticGraph         |
//            /            \                                 |
//           /              \                                |
//      EbertGraph     ForwardEbertGraph                     |
//
// In the general EbertGraph case, the graph is represented with three
// arrays.
// Let n be the number of nodes and m be the number of arcs.
// Let i be an integer in [0..m-1], denoting the index of an arc.
//  * head_[i] contains the end-node of arc i,
//  * head_[-i-1] contains the start-node of arc i.
// Note that in two's-complement arithmetic, -i-1 = ~i.
// Consequently:
//  * head_[~i] contains the end-node of the arc reverse to arc i,
//  * head_[i] contains the start-node of the arc reverse to arc i.
//  Note that if arc (u, v) is defined, then the data structure also stores
//  (v, u).
//  Arc ~i thus denotes the arc reverse to arc i.
//  This is what makes this representation useful for undirected graphs and for
//  implementing algorithms like bidirectional shortest paths.
//  Also note that the representation handles multi-graphs. If several arcs
//  going from node u to node v are added to the graph, they will be handled as
//  separate arcs.
//
// Now, for an integer u in [0..n-1] denoting the index of a node:
//  * first_incident_arc_[u] denotes the first arc in the adjacency list of u.
//  * going from an arc i, the adjacency list can be traversed using
//    j = next_adjacent_arc_[i].
//
// The EbertGraph implementation has the following benefits:
//  * It is able to handle both directed or undirected graphs.
//  * Being based on indices, it is easily serializable. Only the contents
//    of the head_ array need to be stored. Even so, serialization is
//    currently not implemented.
//  * The node indices and arc indices can be stored in 32 bits, while
//    still allowing to go a bit further than the 4-gigabyte
//    limitation (48 gigabytes for a pure graph, without capacities or
//    costs.)
//  * The representation can be recomputed if edges have been loaded from
//  * The representation can be recomputed if edges have been loaded from
//    external memory or if edges have been re-ordered.
//  * The memory consumption is: 2 * m * sizeof(NodeIndexType)
//                             + 2 * m * sizeof(ArcIndexType)
//                             + n * sizeof(ArcIndexType)
//    plus a small constant.
//
// The EbertGraph implementation differs from the implementation described in
// [Ebert 1987] in the following respects:
//  * arcs are represented using an (i, ~i) approach, whereas Ebert used
//    (i, -i). Indices for direct arcs thus start at 0, in a fashion that is
//    compatible with the index numbering in C and C++. Note that we also tested
//    a (2*i, 2*i+1) storage pattern, which did not show any speed benefit, and
//    made the use of the API much more difficult.
//  * because of this, the 'nil' values for nodes and arcs are not 0, as Ebert
//    first described. The value for the 'nil' node is set to -1, while the
//    value for the 'nil' arc is set to the smallest integer representable with
//    ArcIndexSize bytes.
//  * it is possible to add arcs to the graph, with AddArc, in a much simpler
//    way than described by Ebert.
//  * TODO(user) although it is already possible, using the
//    GroupForwardArcsByFunctor method, to group all the outgoing (resp.
//    incoming) arcs of a node, the iterator logic could still be improved to
//    allow traversing the outgoing (resp. incoming) arcs in O(out_degree(node))
//    (resp. O(in_degree(node))) instead of O(degree(node)).
//  * TODO(user) it is possible to implement arc deletion and garbage collection
//    in an efficient (relatively) manner. For the time being we haven't seen an
//    application for this.
//
// The ForwardEbertGraph representation is like the EbertGraph case described
// above, with the following modifications:
//  * The part of the head_[] array with negative indices is absent. In its
//    place is a pointer tail_ which, if assigned, points to an array of tail
//    nodes indexed by (nonnegative) arc index. In typical usage tail_ is NULL
//    and the memory for the tail nodes need not be allocated.
//  * The array of arc tails can be allocated as needed and populated from the
//    adjacency lists of the graph.
//  * Representing only the forward star of each node implies that the graph
//    cannot be serialized directly nor rebuilt from scratch from just the head_
//    array. Rebuilding from scratch requires constructing the array of arc
//    tails from the adjacency lists first, and serialization can be done either
//    by first constructing the array of arc tails from the adjacency lists, or
//    by serializing directly from the adjacency lists.
//  * The memory consumption is: m * sizeof(NodeIndexType)
//                             + m * sizeof(ArcIndexType)
//                             + n * sizeof(ArcIndexType)
//    plus a small constant when the array of arc tails is absent. Allocating
//    the arc tail array adds another m * sizeof(NodeIndexType).
//
// The ForwardStaticGraph representation is restricted yet farther
// than ForwardEbertGraph, with the benefit that it provides higher
// performance to those applications that can use it.
//  * As with ForwardEbertGraph, the presence of the array of arc
//    tails is optional.
//  * The outgoing adjacency list for each node is stored in a
//    contiguous segment of the head_[] array, obviating the
//    next_adjacent_arc_ structure entirely and ensuring good locality
//    of reference for applications that iterate over outgoing
//    adjacency lists.
//  * The memory consumption is: m * sizeof(NodeIndexType)
//                             + n * sizeof(ArcIndexType)
//    plus a small constant when the array of arc tails is absent. Allocating
//    the arc tail array adds another m * sizeof(NodeIndexType).

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/util/permutation.h"
#include "ortools/util/zvector.h"

namespace operations_research {

// Forward declarations.
template <typename NodeIndexType, typename ArcIndexType>
class EbertGraph;
template <typename NodeIndexType, typename ArcIndexType>
class ForwardEbertGraph;
template <typename NodeIndexType, typename ArcIndexType>
class ForwardStaticGraph;

// Standard instantiation of ForwardEbertGraph (named 'ForwardStarGraph') of
// EbertGraph (named 'StarGraph'); and relevant type shortcuts. Unless their use
// cases prevent them from doing so, users are encouraged to use StarGraph or
// ForwardStarGraph according to whether or not they require reverse arcs to be
// represented explicitly. Along with either graph representation, the other
// type shortcuts here will often come in handy.
typedef int32 NodeIndex;
typedef int32 ArcIndex;
typedef int64 FlowQuantity;
typedef int64 CostValue;
typedef EbertGraph<NodeIndex, ArcIndex> StarGraph;
typedef ForwardEbertGraph<NodeIndex, ArcIndex> ForwardStarGraph;
typedef ForwardStaticGraph<NodeIndex, ArcIndex> ForwardStarStaticGraph;
typedef ZVector<NodeIndex> NodeIndexArray;
typedef ZVector<ArcIndex> ArcIndexArray;
typedef ZVector<FlowQuantity> QuantityArray;
typedef ZVector<CostValue> CostArray;

template <typename NodeIndexType, typename ArcIndexType, typename DerivedGraph>
class StarGraphBase {
 public:
  // The index of the 'nil' node in the graph.
  static const NodeIndexType kNilNode;

  // The index of the 'nil' arc in the graph.
  static const ArcIndexType kNilArc;

  // The index of the first node in the graph.
  static const NodeIndexType kFirstNode;

  // The index of the first arc in the graph.
  static const ArcIndexType kFirstArc;

  // The maximum possible number of nodes in the graph.  (The maximum
  // index is kMaxNumNodes-1, since indices start at 0. Unfortunately
  // we waste a value representing this and the max_num_nodes_ member.)
  static const NodeIndexType kMaxNumNodes;

  // The maximum possible number of arcs in the graph.  (The maximum
  // index is kMaxNumArcs-1, since indices start at 0. Unfortunately
  // we waste a value representing this and the max_num_arcs_ member.)
  static const ArcIndexType kMaxNumArcs;
  // Returns the number of nodes in the graph.
  NodeIndexType num_nodes() const { return num_nodes_; }

  // Returns the number of original arcs in the graph
  // (The ones with positive indices.)
  ArcIndexType num_arcs() const { return num_arcs_; }

  // Returns one more than the largest index of an extant node,
  // meaning a node that is mentioned as the head or tail of some arc
  // in the graph. To be used as a helper when clients need to
  // dimension or iterate over arrays of node annotation information.
  NodeIndexType end_node_index() const { return kFirstNode + num_nodes_; }

  // Returns one more than the largest index of an extant direct
  // arc. To be used as a helper when clients need to dimension or
  // iterate over arrays of arc annotation information.
  ArcIndexType end_arc_index() const { return kFirstArc + num_arcs_; }

  // Returns the maximum possible number of nodes in the graph.
  NodeIndexType max_num_nodes() const { return max_num_nodes_; }

  // Returns the maximum possible number of original arcs in the graph.
  // (The ones with positive indices.)
  ArcIndexType max_num_arcs() const { return max_num_arcs_; }

  // Returns one more than the largest valid index of a node. To be
  // used as a helper when clients need to dimension or iterate over
  // arrays of node annotation information.
  NodeIndexType max_end_node_index() const {
    return kFirstNode + max_num_nodes_;
  }

  // Returns one more than the largest valid index of a direct arc. To
  // be used as a helper when clients need to dimension or iterate
  // over arrays of arc annotation information.
  ArcIndexType max_end_arc_index() const { return kFirstArc + max_num_arcs_; }

  // Utility function to check that a node index is within the bounds AND
  // different from kNilNode.
  // Returns true if node is in the range [kFirstNode .. max_num_nodes_).
  // It is exported so that users of the DerivedGraph class can use it.
  // To be used in a DCHECK; also used internally to validate
  // arguments passed to our methods from clients (e.g., AddArc()).
  bool IsNodeValid(NodeIndexType node) const {
    return node >= kFirstNode && node < max_num_nodes_;
  }

  // Returns the first arc going from tail to head, if it exists, or kNilArc
  // if such an arc does not exist.
  ArcIndexType LookUpArc(const NodeIndexType tail,
                         const NodeIndexType head) const {
    for (ArcIndexType arc = FirstOutgoingArc(tail); arc != kNilArc;
         arc = ThisAsDerived()->NextOutgoingArc(tail, arc)) {
      if (Head(arc) == head) {
        return arc;
      }
    }
    return kNilArc;
  }

  // Returns the head or end-node of arc.
  NodeIndexType Head(const ArcIndexType arc) const {
    DCHECK(ThisAsDerived()->CheckArcValidity(arc));
    return head_[arc];
  }

  std::string NodeDebugString(const NodeIndexType node) const {
    if (node == kNilNode) {
      return "NilNode";
    } else {
      return absl::StrCat(static_cast<int64>(node));
    }
  }

  std::string ArcDebugString(const ArcIndexType arc) const {
    if (arc == kNilArc) {
      return "NilArc";
    } else {
      return absl::StrCat(static_cast<int64>(arc));
    }
  }

#if !defined(SWIG)
  // Iterator class for traversing all the nodes in the graph.
  class NodeIterator {
   public:
    explicit NodeIterator(const DerivedGraph& graph)
        : graph_(graph), head_(graph_.StartNode(kFirstNode)) {}

    // Returns true unless all the nodes have been traversed.
    bool Ok() const { return head_ != kNilNode; }

    // Advances the current node index.
    void Next() { head_ = graph_.NextNode(head_); }

    // Returns the index of the node currently pointed to by the iterator.
    NodeIndexType Index() const { return head_; }

   private:
    // A reference to the current DerivedGraph considered.
    const DerivedGraph& graph_;

    // The index of the current node considered.
    NodeIndexType head_;
  };

  // Iterator class for traversing the arcs in the graph.
  class ArcIterator {
   public:
    explicit ArcIterator(const DerivedGraph& graph)
        : graph_(graph), arc_(graph_.StartArc(kFirstArc)) {}

    // Returns true unless all the arcs have been traversed.
    bool Ok() const { return arc_ != kNilArc; }

    // Advances the current arc index.
    void Next() { arc_ = graph_.NextArc(arc_); }

    // Returns the index of the arc currently pointed to by the iterator.
    ArcIndexType Index() const { return arc_; }

   private:
    // A reference to the current DerivedGraph considered.
    const DerivedGraph& graph_;

    // The index of the current arc considered.
    ArcIndexType arc_;
  };

  // Iterator class for traversing the outgoing arcs associated to a given node.
  class OutgoingArcIterator {
   public:
    OutgoingArcIterator(const DerivedGraph& graph, NodeIndexType node)
        : graph_(graph),
          node_(graph_.StartNode(node)),
          arc_(graph_.StartArc(graph_.FirstOutgoingArc(node))) {
      DCHECK(CheckInvariant());
    }

    // This constructor takes an arc as extra argument and makes the iterator
    // start at arc.
    OutgoingArcIterator(const DerivedGraph& graph, NodeIndexType node,
                        ArcIndexType arc)
        : graph_(graph),
          node_(graph_.StartNode(node)),
          arc_(graph_.StartArc(arc)) {
      DCHECK(CheckInvariant());
    }

    // Can only assign from an iterator on the same graph.
    void operator=(const OutgoingArcIterator& iterator) {
      DCHECK(&iterator.graph_ == &graph_);
      node_ = iterator.node_;
      arc_ = iterator.arc_;
    }

    // Returns true unless all the outgoing arcs have been traversed.
    bool Ok() const { return arc_ != kNilArc; }

    // Advances the current outgoing arc index.
    void Next() {
      arc_ = graph_.NextOutgoingArc(node_, arc_);
      DCHECK(CheckInvariant());
    }

    // Returns the index of the arc currently pointed to by the iterator.
    ArcIndexType Index() const { return arc_; }

   private:
    // Returns true if the invariant for the iterator is verified.
    // To be used in a DCHECK.
    bool CheckInvariant() const {
      if (arc_ == kNilArc) {
        return true;  // This occurs when the iterator has reached the end.
      }
      DCHECK(graph_.IsOutgoing(arc_, node_));
      return true;
    }

    // A reference to the current DerivedGraph considered.
    const DerivedGraph& graph_;

    // The index of the node on which arcs are iterated.
    NodeIndexType node_;

    // The index of the current arc considered.
    ArcIndexType arc_;
  };
#endif  // SWIG

 protected:
  StarGraphBase()
      : max_num_nodes_(0),
        max_num_arcs_(0),
        num_nodes_(0),
        num_arcs_(0),
        first_incident_arc_() {}

  ~StarGraphBase() {}

  // Returns kNilNode if the graph has no nodes or node if it has at least one
  // node. Useful for initializing iterators correctly in the case of empty
  // graphs.
  NodeIndexType StartNode(NodeIndexType node) const {
    return num_nodes_ == 0 ? kNilNode : node;
  }

  // Returns kNilArc if the graph has no arcs arc if it has at least one arc.
  // Useful for initializing iterators correctly in the case of empty graphs.
  ArcIndexType StartArc(ArcIndexType arc) const {
    return num_arcs_ == 0 ? kNilArc : arc;
  }

  // Returns the node following the argument in the graph.
  // Returns kNilNode (= end) if the range of nodes has been exhausted.
  // It is called by NodeIterator::Next() and as such does not expect to be
  // passed an argument equal to kNilNode.
  // This is why the return line is simplified from
  // return (node == kNilNode || next_node >= num_nodes_)
  //        ? kNilNode : next_node;
  // to
  // return next_node < num_nodes_ ? next_node : kNilNode;
  NodeIndexType NextNode(const NodeIndexType node) const {
    DCHECK(IsNodeValid(node));
    const NodeIndexType next_node = node + 1;
    return next_node < num_nodes_ ? next_node : kNilNode;
  }

  // Returns the arc following the argument in the graph.
  // Returns kNilArc (= end) if the range of arcs has been exhausted.
  // It is called by ArcIterator::Next() and as such does not expect to be
  // passed an argument equal to kNilArc.
  // This is why the return line is simplified from
  // return ( arc == kNilArc || next_arc >= num_arcs_) ? kNilArc : next_arc;
  // to
  // return next_arc < num_arcs_ ? next_arc : kNilArc;
  ArcIndexType NextArc(const ArcIndexType arc) const {
    DCHECK(ThisAsDerived()->CheckArcValidity(arc));
    const ArcIndexType next_arc = arc + 1;
    return next_arc < num_arcs_ ? next_arc : kNilArc;
  }

  // Returns the first outgoing arc for node.
  ArcIndexType FirstOutgoingArc(const NodeIndexType node) const {
    DCHECK(IsNodeValid(node));
    return ThisAsDerived()->FindNextOutgoingArc(
        ThisAsDerived()->FirstOutgoingOrOppositeIncomingArc(node));
  }

  // The maximum number of nodes that the graph can hold.
  NodeIndexType max_num_nodes_;

  // The maximum number of arcs that the graph can hold.
  ArcIndexType max_num_arcs_;

  // The maximum index of the node currently held by the graph.
  NodeIndexType num_nodes_;

  // The current number of arcs held by the graph.
  ArcIndexType num_arcs_;

  // Array of node indices. head_[i] contains the head node of arc i.
  ZVector<NodeIndexType> head_;

  // Array of arc indices. first_incident_arc_[i] contains the first arc
  // incident to node i.
  ZVector<ArcIndexType> first_incident_arc_;

 private:
  // Shorthand: returns a const DerivedGraph*-typed version of our
  // "this" pointer.
  inline const DerivedGraph* ThisAsDerived() const {
    return static_cast<const DerivedGraph*>(this);
  }

  // Shorthand: returns a DerivedGraph*-typed version of our "this"
  // pointer.
  inline DerivedGraph* ThisAsDerived() {
    return static_cast<DerivedGraph*>(this);
  }
};

template <typename NodeIndexType, typename ArcIndexType>
class PermutationIndexComparisonByArcHead {
 public:
  explicit PermutationIndexComparisonByArcHead(
      const ZVector<NodeIndexType>& head)
      : head_(head) {}

  bool operator()(ArcIndexType a, ArcIndexType b) const {
    return head_[a] < head_[b];
  }

 private:
  const ZVector<NodeIndexType>& head_;
};

template <typename NodeIndexType, typename ArcIndexType>
class ForwardStaticGraph
    : public StarGraphBase<NodeIndexType, ArcIndexType,
                           ForwardStaticGraph<NodeIndexType, ArcIndexType> > {
  typedef StarGraphBase<NodeIndexType, ArcIndexType,
                        ForwardStaticGraph<NodeIndexType, ArcIndexType> >
      Base;
  friend class StarGraphBase<NodeIndexType, ArcIndexType,
                             ForwardStaticGraph<NodeIndexType, ArcIndexType> >;

  using Base::ArcDebugString;
  using Base::NodeDebugString;

  using Base::first_incident_arc_;
  using Base::head_;
  using Base::max_num_arcs_;
  using Base::max_num_nodes_;
  using Base::num_arcs_;
  using Base::num_nodes_;

 public:
#if !defined(SWIG)
  using Base::end_arc_index;
  using Base::Head;
  using Base::IsNodeValid;

  using Base::kFirstArc;
  using Base::kFirstNode;
  using Base::kNilArc;
#endif  // SWIG

  typedef NodeIndexType NodeIndex;
  typedef ArcIndexType ArcIndex;

// TODO(user): Configure SWIG to handle the
// CycleHandlerForAnnotatedArcs class.
#if !defined(SWIG)
  class CycleHandlerForAnnotatedArcs
      : public ArrayIndexCycleHandler<NodeIndexType, ArcIndexType> {
    typedef ArrayIndexCycleHandler<NodeIndexType, ArcIndexType> Base;

   public:
    CycleHandlerForAnnotatedArcs(
        PermutationCycleHandler<ArcIndexType>* annotation_handler,
        NodeIndexType* data)
        : ArrayIndexCycleHandler<NodeIndexType, ArcIndexType>(&data[kFirstArc]),
          annotation_handler_(annotation_handler) {}

    void SetTempFromIndex(ArcIndexType source) override {
      Base::SetTempFromIndex(source);
      annotation_handler_->SetTempFromIndex(source);
    }

    void SetIndexFromIndex(ArcIndexType source,
                           ArcIndexType destination) const override {
      Base::SetIndexFromIndex(source, destination);
      annotation_handler_->SetIndexFromIndex(source, destination);
    }

    void SetIndexFromTemp(ArcIndexType destination) const override {
      Base::SetIndexFromTemp(destination);
      annotation_handler_->SetIndexFromTemp(destination);
    }

   private:
    PermutationCycleHandler<ArcIndexType>* annotation_handler_;

    DISALLOW_COPY_AND_ASSIGN(CycleHandlerForAnnotatedArcs);
  };
#endif  // SWIG

  // Constructor for use by GraphBuilderFromArcs instances and direct
  // clients that want to materialize a graph in one step.
  // Materializing all at once is the only choice available with a
  // static graph.
  //
  // Args:
  //   sort_arcs_by_head: determines whether arcs incident to each tail
  //     node are sorted by head node.
  //   client_cycle_handler: if non-NULL, mediates the permutation of
  //     arbitrary annotation data belonging to the client according
  //     to the permutation applied to the arcs in forming the
  //     graph. Two permutations may be composed to form the final one
  //     that affects the arcs. First, the arcs are always permuted to
  //     group them by tail node because ForwardStaticGraph requires
  //     this. Second, if each node's outgoing arcs are sorted by head
  //     node (according to sort_arcs_by_head), that sorting implies
  //     an additional permutation on the arcs.
  ForwardStaticGraph(
      const NodeIndexType num_nodes, const ArcIndexType num_arcs,
      const bool sort_arcs_by_head,
      std::vector<std::pair<NodeIndexType, NodeIndexType> >* client_input_arcs,
      // TODO(user): For some reason, SWIG breaks if the
      // operations_research namespace is not explicit in the
      // following argument declaration.
      operations_research::PermutationCycleHandler<ArcIndexType>* const
          client_cycle_handler) {
    max_num_arcs_ = num_arcs;
    num_arcs_ = num_arcs;
    max_num_nodes_ = num_nodes;
    // A more convenient name for a parameter required by style to be
    // a pointer, because we modify its referent.
    std::vector<std::pair<NodeIndexType, NodeIndexType> >& input_arcs =
        *client_input_arcs;

    // We coopt the first_incident_arc_ array as a node-indexed vector
    // used for two purposes related to degree before setting up its
    // final values. First, it counts the out-degree of each
    // node. Second, it is reused to count the number of arcs outgoing
    // from each node that have already been put in place from the
    // given input_arcs. We reserve an extra entry as a sentinel at
    // the end.
    first_incident_arc_.Reserve(kFirstNode, kFirstNode + num_nodes);
    first_incident_arc_.SetAll(0);
    for (ArcIndexType arc = kFirstArc; arc < kFirstArc + num_arcs; ++arc) {
      first_incident_arc_[kFirstNode + input_arcs[arc].first] += 1;
      // Take this opportunity to see how many nodes are really
      // mentioned in the arc list.
      num_nodes_ = std::max(
          num_nodes_, static_cast<NodeIndexType>(input_arcs[arc].first + 1));
      num_nodes_ = std::max(
          num_nodes_, static_cast<NodeIndexType>(input_arcs[arc].second + 1));
    }
    ArcIndexType next_arc = kFirstArc;
    for (NodeIndexType node = 0; node < num_nodes; ++node) {
      ArcIndexType degree = first_incident_arc_[kFirstNode + node];
      first_incident_arc_[kFirstNode + node] = next_arc;
      next_arc += degree;
    }
    DCHECK_EQ(num_arcs, next_arc);
    head_.Reserve(kFirstArc, kFirstArc + num_arcs - 1);
    std::unique_ptr<ArcIndexType[]> arc_permutation;
    if (client_cycle_handler != nullptr) {
      arc_permutation.reset(new ArcIndexType[end_arc_index()]);
      for (ArcIndexType input_arc = 0; input_arc < num_arcs; ++input_arc) {
        NodeIndexType tail = input_arcs[input_arc].first;
        NodeIndexType head = input_arcs[input_arc].second;
        ArcIndexType arc = first_incident_arc_[kFirstNode + tail];
        // The head_ entry will get permuted into the right place
        // later.
        head_[kFirstArc + input_arc] = kFirstNode + head;
        arc_permutation[kFirstArc + arc] = input_arc;
        first_incident_arc_[kFirstNode + tail] += 1;
      }
    } else {
      if (sizeof(input_arcs[0].first) >= sizeof(first_incident_arc_[0])) {
        // We reuse the input_arcs[].first entries to hold our
        // mapping to the head_ array. This allows us to spread out
        // cache badness.
        for (ArcIndexType input_arc = 0; input_arc < num_arcs; ++input_arc) {
          NodeIndexType tail = input_arcs[input_arc].first;
          ArcIndexType arc = first_incident_arc_[kFirstNode + tail];
          first_incident_arc_[kFirstNode + tail] = arc + 1;
          input_arcs[input_arc].first = static_cast<NodeIndexType>(arc);
        }
        for (ArcIndexType input_arc = 0; input_arc < num_arcs; ++input_arc) {
          ArcIndexType arc =
              static_cast<ArcIndexType>(input_arcs[input_arc].first);
          NodeIndexType head = input_arcs[input_arc].second;
          head_[kFirstArc + arc] = kFirstNode + head;
        }
      } else {
        // We cannot reuse the input_arcs[].first entries so we map to
        // the head_ array in a single loop.
        for (ArcIndexType input_arc = 0; input_arc < num_arcs; ++input_arc) {
          NodeIndexType tail = input_arcs[input_arc].first;
          NodeIndexType head = input_arcs[input_arc].second;
          ArcIndexType arc = first_incident_arc_[kFirstNode + tail];
          first_incident_arc_[kFirstNode + tail] = arc + 1;
          head_[kFirstArc + arc] = kFirstNode + head;
        }
      }
    }
    // Shift the entries in first_incident_arc_ to compensate for the
    // counting each one has done through its incident arcs. Note that
    // there is a special sentry element at the end of
    // first_incident_arc_.
    for (NodeIndexType node = kFirstNode + num_nodes; node > /* kFirstNode */ 0;
         --node) {
      first_incident_arc_[node] = first_incident_arc_[node - 1];
    }
    first_incident_arc_[kFirstNode] = kFirstArc;
    if (sort_arcs_by_head) {
      ArcIndexType begin = first_incident_arc_[kFirstNode];
      if (client_cycle_handler != nullptr) {
        for (NodeIndexType node = 0; node < num_nodes; ++node) {
          ArcIndexType end = first_incident_arc_[node + 1];
          std::sort(
              &arc_permutation[begin], &arc_permutation[end],
              PermutationIndexComparisonByArcHead<NodeIndexType, ArcIndexType>(
                  head_));
          begin = end;
        }
      } else {
        for (NodeIndexType node = 0; node < num_nodes; ++node) {
          ArcIndexType end = first_incident_arc_[node + 1];
          // The second argument in the following has a strange index
          // expression because ZVector claims that no index is valid
          // unless it refers to an element in the vector. In particular
          // an index one past the end is invalid.
          ArcIndexType begin_index = (begin < num_arcs ? begin : begin - 1);
          ArcIndexType begin_offset = (begin < num_arcs ? 0 : 1);
          ArcIndexType end_index = (end > 0 ? end - 1 : end);
          ArcIndexType end_offset = (end > 0 ? 1 : 0);
          std::sort(&head_[begin_index] + begin_offset,
                    &head_[end_index] + end_offset);
          begin = end;
        }
      }
    }
    if (client_cycle_handler != nullptr && num_arcs > 0) {
      // Apply the computed permutation if we haven't already.
      CycleHandlerForAnnotatedArcs handler_for_constructor(
          client_cycle_handler, &head_[kFirstArc] - kFirstArc);
      // We use a permutation cycle handler to place the head array
      // indices and permute the client's arc annotation data along
      // with them.
      PermutationApplier<ArcIndexType> permutation(&handler_for_constructor);
      permutation.Apply(&arc_permutation[0], kFirstArc, end_arc_index());
    }
  }

  // Returns the tail or start-node of arc.
  NodeIndexType Tail(const ArcIndexType arc) const {
    DCHECK(CheckArcValidity(arc));
    DCHECK(CheckTailIndexValidity(arc));
    return (*tail_)[arc];
  }

  // Returns true if arc is incoming to node.
  bool IsIncoming(ArcIndexType arc, NodeIndexType node) const {
    return Head(arc) == node;
  }

  // Utility function to check that an arc index is within the bounds.
  // It is exported so that users of the ForwardStaticGraph class can use it.
  // To be used in a DCHECK.
  bool CheckArcBounds(const ArcIndexType arc) const {
    return ((arc == kNilArc) || (arc >= kFirstArc && arc < max_num_arcs_));
  }

  // Utility function to check that an arc index is within the bounds AND
  // different from kNilArc.
  // It is exported so that users of the ForwardStaticGraph class can use it.
  // To be used in a DCHECK.
  bool CheckArcValidity(const ArcIndexType arc) const {
    return ((arc != kNilArc) && (arc >= kFirstArc && arc < max_num_arcs_));
  }

  // Returns true if arc is a valid index into the (*tail_) array.
  bool CheckTailIndexValidity(const ArcIndexType arc) const {
    return ((tail_ != nullptr) && (arc >= kFirstArc) &&
            (arc <= tail_->max_index()));
  }

  ArcIndexType NextOutgoingArc(const NodeIndexType node,
                               ArcIndexType arc) const {
    DCHECK(IsNodeValid(node));
    DCHECK(CheckArcValidity(arc));
    ++arc;
    if (arc < first_incident_arc_[node + 1]) {
      return arc;
    } else {
      return kNilArc;
    }
  }

  // Returns a debug std::string containing all the information contained in the
  // data structure in raw form.
  std::string DebugString() const {
    std::string result = "Arcs:(node) :\n";
    for (ArcIndexType arc = kFirstArc; arc < num_arcs_; ++arc) {
      result += " " + ArcDebugString(arc) + ":(" + NodeDebugString(head_[arc]) +
                ")\n";
    }
    result += "Node:First arc :\n";
    for (NodeIndexType node = kFirstNode; node <= num_nodes_; ++node) {
      result += " " + NodeDebugString(node) + ":" +
                ArcDebugString(first_incident_arc_[node]) + "\n";
    }
    return result;
  }

  bool BuildTailArray() {
    // If (*tail_) is already allocated, we have the invariant that
    // its contents are canonical, so we do not need to do anything
    // here in that case except return true.
    if (tail_ == nullptr) {
      if (!RepresentationClean()) {
        // We have been asked to build the (*tail_) array, but we have
        // no valid information from which to build it. The graph is
        // in an unrecoverable, inconsistent state.
        return false;
      }
      // Reallocate (*tail_) and rebuild its contents from the
      // adjacency lists.
      tail_.reset(new ZVector<NodeIndexType>);
      tail_->Reserve(kFirstArc, max_num_arcs_ - 1);
      typename Base::NodeIterator node_it(*this);
      for (; node_it.Ok(); node_it.Next()) {
        NodeIndexType node = node_it.Index();
        typename Base::OutgoingArcIterator arc_it(*this, node);
        for (; arc_it.Ok(); arc_it.Next()) {
          (*tail_)[arc_it.Index()] = node;
        }
      }
    }
    DCHECK(TailArrayComplete());
    return true;
  }

  void ReleaseTailArray() { tail_.reset(nullptr); }

  // To be used in a DCHECK().
  bool TailArrayComplete() const {
    CHECK(tail_);
    for (ArcIndexType arc = kFirstArc; arc < num_arcs_; ++arc) {
      CHECK(CheckTailIndexValidity(arc));
      CHECK(IsNodeValid((*tail_)[arc]));
    }
    return true;
  }

 private:
  bool IsDirect() const { return true; }
  bool RepresentationClean() const { return true; }
  bool IsOutgoing(const NodeIndexType node,
                  const ArcIndexType unused_arc) const {
    return true;
  }

  // Returns the first arc in node's incidence list.
  ArcIndexType FirstOutgoingOrOppositeIncomingArc(NodeIndexType node) const {
    DCHECK(RepresentationClean());
    DCHECK(IsNodeValid(node));
    ArcIndexType result = first_incident_arc_[node];
    return ((result != first_incident_arc_[node + 1]) ? result : kNilArc);
  }

  // Utility method that finds the next outgoing arc.
  ArcIndexType FindNextOutgoingArc(ArcIndexType arc) const {
    DCHECK(CheckArcBounds(arc));
    return arc;
  }

  // Array of node indices, not always present. (*tail_)[i] contains
  // the tail node of arc i. This array is not needed for normal graph
  // traversal operations, but is used in optimizing the graph's
  // layout so arcs are grouped by tail node, and can be used in one
  // approach to serializing the graph.
  //
  // Invariants: At any time when we are not executing a method of
  // this class, either tail_ == NULL or the tail_ array's contents
  // are kept canonical. If tail_ != NULL, any method that modifies
  // adjacency lists must also ensure (*tail_) is modified
  // correspondingly. The converse does not hold: Modifications to
  // (*tail_) are allowed without updating the adjacency lists. If
  // such modifications take place, representation_clean_ must be set
  // to false, of course, to indicate that the adjacency lists are no
  // longer current.
  std::unique_ptr<ZVector<NodeIndexType> > tail_;
};

// The index of the 'nil' node in the graph.
template <typename NodeIndexType, typename ArcIndexType, typename DerivedGraph>
const NodeIndexType
    StarGraphBase<NodeIndexType, ArcIndexType, DerivedGraph>::kNilNode = -1;

// The index of the 'nil' arc in the graph.
template <typename NodeIndexType, typename ArcIndexType, typename DerivedGraph>
const ArcIndexType
    StarGraphBase<NodeIndexType, ArcIndexType, DerivedGraph>::kNilArc =
        std::numeric_limits<ArcIndexType>::min();

// The index of the first node in the graph.
template <typename NodeIndexType, typename ArcIndexType, typename DerivedGraph>
const NodeIndexType
    StarGraphBase<NodeIndexType, ArcIndexType, DerivedGraph>::kFirstNode = 0;

// The index of the first arc in the graph.
template <typename NodeIndexType, typename ArcIndexType, typename DerivedGraph>
const ArcIndexType
    StarGraphBase<NodeIndexType, ArcIndexType, DerivedGraph>::kFirstArc = 0;

// The maximum possible node index in the graph.
template <typename NodeIndexType, typename ArcIndexType, typename DerivedGraph>
const NodeIndexType
    StarGraphBase<NodeIndexType, ArcIndexType, DerivedGraph>::kMaxNumNodes =
        std::numeric_limits<NodeIndexType>::max();

// The maximum possible number of arcs in the graph.
// (The maximum index is kMaxNumArcs-1, since indices start at 0.)
template <typename NodeIndexType, typename ArcIndexType, typename DerivedGraph>
const ArcIndexType
    StarGraphBase<NodeIndexType, ArcIndexType, DerivedGraph>::kMaxNumArcs =
        std::numeric_limits<ArcIndexType>::max();

// A template for the base class that holds the functionality that exists in
// common between the EbertGraph<> template and the ForwardEbertGraph<>
// template.
//
// This template is for internal use only, and this is enforced by making all
// constructors for this class template protected. Clients should use one of the
// two derived-class templates. Most clients will not even use those directly,
// but will use the StarGraph and ForwardStarGraph typenames declared above.
//
// The DerivedGraph template argument must be the type of the class (typically
// itself built from a template) that:
// 1. implements the full interface expected for either ForwardEbertGraph or
//    EbertGraph, and
// 2. inherits from an instance of this template.
// The base class needs access to some members of the derived class such as, for
// example, NextOutgoingArc(), and it gets this access via the DerivedGraph
// template argument.
template <typename NodeIndexType, typename ArcIndexType, typename DerivedGraph>
class EbertGraphBase
    : public StarGraphBase<NodeIndexType, ArcIndexType, DerivedGraph> {
  typedef StarGraphBase<NodeIndexType, ArcIndexType, DerivedGraph> Base;
  friend class StarGraphBase<NodeIndexType, ArcIndexType, DerivedGraph>;

 protected:
  using Base::first_incident_arc_;
  using Base::head_;
  using Base::max_num_arcs_;
  using Base::max_num_nodes_;
  using Base::num_arcs_;
  using Base::num_nodes_;

 public:
#if !SWIG
  using Base::end_arc_index;
  using Base::IsNodeValid;

  using Base::kFirstArc;
  using Base::kFirstNode;
  using Base::kMaxNumArcs;
  using Base::kMaxNumNodes;
  using Base::kNilArc;
  using Base::kNilNode;
#endif  // SWIG

  // Reserves memory needed for max_num_nodes nodes and max_num_arcs arcs.
  // Returns false if the parameters passed are not OK.
  // It can be used to enlarge the graph, but does not shrink memory
  // if called with smaller values.
  bool Reserve(NodeIndexType new_max_num_nodes, ArcIndexType new_max_num_arcs) {
    if (new_max_num_nodes < 0 || new_max_num_nodes > kMaxNumNodes) {
      return false;
    }
    if (new_max_num_arcs < 0 || new_max_num_arcs > kMaxNumArcs) {
      return false;
    }
    first_incident_arc_.Reserve(kFirstNode, new_max_num_nodes - 1);
    for (NodeIndexType node = max_num_nodes_;
         node <= first_incident_arc_.max_index(); ++node) {
      first_incident_arc_.Set(node, kNilArc);
    }
    ThisAsDerived()->ReserveInternal(new_max_num_nodes, new_max_num_arcs);
    max_num_nodes_ = new_max_num_nodes;
    max_num_arcs_ = new_max_num_arcs;
    return true;
  }

  // Adds an arc to the graph and returns its index.
  // Returns kNilArc if the arc could not be added.
  // Note that for a given pair (tail, head) AddArc does not overwrite an
  // already-existing arc between tail and head: Another arc is created
  // instead. This makes it possible to handle multi-graphs.
  ArcIndexType AddArc(NodeIndexType tail, NodeIndexType head) {
    if (num_arcs_ >= max_num_arcs_ || !IsNodeValid(tail) ||
        !IsNodeValid(head)) {
      return kNilArc;
    }
    if (tail + 1 > num_nodes_) {
      num_nodes_ = tail + 1;  // max does not work on int16.
    }
    if (head + 1 > num_nodes_) {
      num_nodes_ = head + 1;
    }
    ArcIndexType arc = num_arcs_;
    ++num_arcs_;
    ThisAsDerived()->RecordArc(arc, tail, head);
    return arc;
  }

// TODO(user): Configure SWIG to handle the GroupForwardArcsByFunctor
// member template and the CycleHandlerForAnnotatedArcs class.
#if !SWIG
  template <typename ArcIndexTypeStrictWeakOrderingFunctor>
  void GroupForwardArcsByFunctor(
      const ArcIndexTypeStrictWeakOrderingFunctor& compare,
      PermutationCycleHandler<ArcIndexType>* annotation_handler) {
    std::unique_ptr<ArcIndexType[]> arc_permutation(
        new ArcIndexType[end_arc_index()]);

    // Determine the permutation that groups arcs by their tail nodes.
    for (ArcIndexType i = 0; i < end_arc_index(); ++i) {
      // Start with the identity permutation.
      arc_permutation[i] = i;
    }
    std::sort(&arc_permutation[kFirstArc], &arc_permutation[end_arc_index()],
              compare);

    // Now we actually permute the head_ array and the
    // scaled_arc_cost_ array according to the sorting permutation.
    CycleHandlerForAnnotatedArcs cycle_handler(annotation_handler,
                                               ThisAsDerived());
    PermutationApplier<ArcIndexType> permutation(&cycle_handler);
    permutation.Apply(&arc_permutation[0], kFirstArc, end_arc_index());

    // Finally, rebuild the graph from its permuted head_ array.
    ThisAsDerived()->BuildRepresentation();
  }

  class CycleHandlerForAnnotatedArcs
      : public PermutationCycleHandler<ArcIndexType> {
   public:
    CycleHandlerForAnnotatedArcs(
        PermutationCycleHandler<ArcIndexType>* annotation_handler,
        DerivedGraph* graph)
        : annotation_handler_(annotation_handler),
          graph_(graph),
          head_temp_(kNilNode),
          tail_temp_(kNilNode) {}

    void SetTempFromIndex(ArcIndexType source) override {
      if (annotation_handler_ != nullptr) {
        annotation_handler_->SetTempFromIndex(source);
      }
      head_temp_ = graph_->Head(source);
      tail_temp_ = graph_->Tail(source);
    }

    void SetIndexFromIndex(ArcIndexType source,
                           ArcIndexType destination) const override {
      if (annotation_handler_ != nullptr) {
        annotation_handler_->SetIndexFromIndex(source, destination);
      }
      graph_->SetHead(destination, graph_->Head(source));
      graph_->SetTail(destination, graph_->Tail(source));
    }

    void SetIndexFromTemp(ArcIndexType destination) const override {
      if (annotation_handler_ != nullptr) {
        annotation_handler_->SetIndexFromTemp(destination);
      }
      graph_->SetHead(destination, head_temp_);
      graph_->SetTail(destination, tail_temp_);
    }

    // Since we are free to destroy the permutation array we use the
    // kNilArc value to mark entries in the array that have been
    // processed already. There is no need to be able to recover the
    // original permutation array entries once they have been seen.
    void SetSeen(ArcIndexType* permutation_element) const override {
      *permutation_element = kNilArc;
    }

    bool Unseen(ArcIndexType permutation_element) const override {
      return permutation_element != kNilArc;
    }

    ~CycleHandlerForAnnotatedArcs() override {}

   private:
    PermutationCycleHandler<ArcIndexType>* annotation_handler_;
    DerivedGraph* graph_;
    NodeIndexType head_temp_;
    NodeIndexType tail_temp_;

    DISALLOW_COPY_AND_ASSIGN(CycleHandlerForAnnotatedArcs);
  };
#endif  // SWIG

 protected:
  EbertGraphBase() : next_adjacent_arc_(), representation_clean_(true) {}

  ~EbertGraphBase() {}

  void Initialize(NodeIndexType max_num_nodes, ArcIndexType max_num_arcs) {
    if (!Reserve(max_num_nodes, max_num_arcs)) {
      LOG(DFATAL) << "Could not reserve memory for "
                  << static_cast<int64>(max_num_nodes) << " nodes and "
                  << static_cast<int64>(max_num_arcs) << " arcs.";
    }
    first_incident_arc_.SetAll(kNilArc);
    ThisAsDerived()->InitializeInternal(max_num_nodes, max_num_arcs);
  }

  // Returns the first arc in node's incidence list.
  ArcIndexType FirstOutgoingOrOppositeIncomingArc(
      const NodeIndexType node) const {
    DCHECK(representation_clean_);
    DCHECK(IsNodeValid(node));
    return first_incident_arc_[node];
  }

  // Returns the next arc following the passed argument in its adjacency list.
  ArcIndexType NextAdjacentArc(const ArcIndexType arc) const {
    DCHECK(representation_clean_);
    DCHECK(ThisAsDerived()->CheckArcValidity(arc));
    return next_adjacent_arc_[arc];
  }

  // Returns the outgoing arc following the argument in the adjacency list.
  ArcIndexType NextOutgoingArc(const NodeIndexType unused_node,
                               const ArcIndexType arc) const {
    DCHECK(ThisAsDerived()->CheckArcValidity(arc));
    DCHECK(ThisAsDerived()->IsDirect(arc));
    return ThisAsDerived()->FindNextOutgoingArc(NextAdjacentArc(arc));
  }

  // Array of next indices.
  // next_adjacent_arc_[i] contains the next arc in the adjacency list of arc i.
  ZVector<ArcIndexType> next_adjacent_arc_;

  // Flag to indicate that BuildRepresentation() needs to be called
  // before the adjacency lists are examined. Only for DCHECK in debug
  // builds.
  bool representation_clean_;

 private:
  // Shorthand: returns a const DerivedGraph*-typed version of our
  // "this" pointer.
  inline const DerivedGraph* ThisAsDerived() const {
    return static_cast<const DerivedGraph*>(this);
  }

  // Shorthand: returns a DerivedGraph*-typed version of our "this"
  // pointer.
  inline DerivedGraph* ThisAsDerived() {
    return static_cast<DerivedGraph*>(this);
  }

  void InitializeInternal(NodeIndexType max_num_nodes,
                          ArcIndexType max_num_arcs) {
    next_adjacent_arc_.SetAll(kNilArc);
  }

  bool RepresentationClean() const { return representation_clean_; }

  // Using the SetHead() method implies that the BuildRepresentation()
  // method must be called to restore consistency before the graph is
  // used.
  void SetHead(const ArcIndexType arc, const NodeIndexType head) {
    representation_clean_ = false;
    head_.Set(arc, head);
  }
};

// Most users should only use StarGraph, which is EbertGraph<int32, int32>, and
// other type shortcuts; see the bottom of this file.
template <typename NodeIndexType, typename ArcIndexType>
class EbertGraph
    : public EbertGraphBase<NodeIndexType, ArcIndexType,
                            EbertGraph<NodeIndexType, ArcIndexType> > {
  typedef EbertGraphBase<NodeIndexType, ArcIndexType,
                         EbertGraph<NodeIndexType, ArcIndexType> >
      Base;
  friend class EbertGraphBase<NodeIndexType, ArcIndexType,
                              EbertGraph<NodeIndexType, ArcIndexType> >;
  friend class StarGraphBase<NodeIndexType, ArcIndexType,
                             EbertGraph<NodeIndexType, ArcIndexType> >;

  using Base::ArcDebugString;
  using Base::FirstOutgoingOrOppositeIncomingArc;
  using Base::Initialize;
  using Base::NextAdjacentArc;
  using Base::NodeDebugString;

  using Base::first_incident_arc_;
  using Base::head_;
  using Base::max_num_arcs_;
  using Base::max_num_nodes_;
  using Base::next_adjacent_arc_;
  using Base::num_arcs_;
  using Base::num_nodes_;
  using Base::representation_clean_;

 public:
#if !SWIG
  using Base::Head;
  using Base::IsNodeValid;

  using Base::kFirstArc;
  using Base::kFirstNode;
  using Base::kNilArc;
  using Base::kNilNode;
#endif  // SWIG

  typedef NodeIndexType NodeIndex;
  typedef ArcIndexType ArcIndex;

  EbertGraph() {}

  EbertGraph(NodeIndexType max_num_nodes, ArcIndexType max_num_arcs) {
    Initialize(max_num_nodes, max_num_arcs);
  }

  ~EbertGraph() {}

#if !SWIG
  // Iterator class for traversing the arcs incident to a given node in the
  // graph.
  class OutgoingOrOppositeIncomingArcIterator {
   public:
    OutgoingOrOppositeIncomingArcIterator(const EbertGraph& graph,
                                          NodeIndexType node)
        : graph_(graph),
          node_(graph_.StartNode(node)),
          arc_(graph_.StartArc(
              graph_.FirstOutgoingOrOppositeIncomingArc(node))) {
      DCHECK(CheckInvariant());
    }

    // This constructor takes an arc as extra argument and makes the iterator
    // start at arc.
    OutgoingOrOppositeIncomingArcIterator(const EbertGraph& graph,
                                          NodeIndexType node, ArcIndexType arc)
        : graph_(graph),
          node_(graph_.StartNode(node)),
          arc_(graph_.StartArc(arc)) {
      DCHECK(CheckInvariant());
    }

    // Can only assign from an iterator on the same graph.
    void operator=(const OutgoingOrOppositeIncomingArcIterator& iterator) {
      DCHECK(&iterator.graph_ == &graph_);
      node_ = iterator.node_;
      arc_ = iterator.arc_;
    }

    // Returns true unless all the adjancent arcs have been traversed.
    bool Ok() const { return arc_ != kNilArc; }

    // Advances the current adjacent arc index.
    void Next() {
      arc_ = graph_.NextAdjacentArc(arc_);
      DCHECK(CheckInvariant());
    }

    // Returns the index of the arc currently pointed to by the iterator.
    ArcIndexType Index() const { return arc_; }

   private:
    // Returns true if the invariant for the iterator is verified.
    // To be used in a DCHECK.
    bool CheckInvariant() const {
      if (arc_ == kNilArc) {
        return true;  // This occurs when the iterator has reached the end.
      }
      DCHECK(graph_.IsOutgoingOrOppositeIncoming(arc_, node_));
      return true;
    }
    // A reference to the current EbertGraph considered.
    const EbertGraph& graph_;

    // The index of the node on which arcs are iterated.
    NodeIndexType node_;

    // The index of the current arc considered.
    ArcIndexType arc_;
  };

  // Iterator class for traversing the incoming arcs associated to a given node.
  class IncomingArcIterator {
   public:
    IncomingArcIterator(const EbertGraph& graph, NodeIndexType node)
        : graph_(graph),
          node_(graph_.StartNode(node)),
          arc_(graph_.StartArc(graph_.FirstIncomingArc(node))) {
      DCHECK(CheckInvariant());
    }

    // This constructor takes an arc as extra argument and makes the iterator
    // start at arc.
    IncomingArcIterator(const EbertGraph& graph, NodeIndexType node,
                        ArcIndexType arc)
        : graph_(graph),
          node_(graph_.StartNode(node)),
          arc_(arc == kNilArc ? kNilArc
                              : graph_.StartArc(graph_.Opposite(arc))) {
      DCHECK(CheckInvariant());
    }

    // Can only assign from an iterator on the same graph.
    void operator=(const IncomingArcIterator& iterator) {
      DCHECK(&iterator.graph_ == &graph_);
      node_ = iterator.node_;
      arc_ = iterator.arc_;
    }

    // Returns true unless all the incoming arcs have been traversed.
    bool Ok() const { return arc_ != kNilArc; }

    // Advances the current incoming arc index.
    void Next() {
      arc_ = graph_.NextIncomingArc(arc_);
      DCHECK(CheckInvariant());
    }

    // Returns the index of the arc currently pointed to by the iterator.
    ArcIndexType Index() const {
      return arc_ == kNilArc ? kNilArc : graph_.Opposite(arc_);
    }

   private:
    // Returns true if the invariant for the iterator is verified.
    // To be used in a DCHECK.
    bool CheckInvariant() const {
      if (arc_ == kNilArc) {
        return true;  // This occurs when the iterator has reached the end.
      }
      DCHECK(graph_.IsIncoming(Index(), node_));
      return true;
    }
    // A reference to the current EbertGraph considered.
    const EbertGraph& graph_;

    // The index of the node on which arcs are iterated.
    NodeIndexType node_;

    // The index of the current arc considered.
    ArcIndexType arc_;
  };
#endif  // SWIG

  // Utility function to check that an arc index is within the bounds.
  // It is exported so that users of the EbertGraph class can use it.
  // To be used in a DCHECK.
  bool CheckArcBounds(const ArcIndexType arc) const {
    return (arc == kNilArc) || (arc >= -max_num_arcs_ && arc < max_num_arcs_);
  }

  // Utility function to check that an arc index is within the bounds AND
  // different from kNilArc.
  // It is exported so that users of the EbertGraph class can use it.
  // To be used in a DCHECK.
  bool CheckArcValidity(const ArcIndexType arc) const {
    return (arc != kNilArc) && (arc >= -max_num_arcs_ && arc < max_num_arcs_);
  }

  // Returns the tail or start-node of arc.
  NodeIndexType Tail(const ArcIndexType arc) const {
    DCHECK(CheckArcValidity(arc));
    return head_[Opposite(arc)];
  }

  // Returns the tail or start-node of arc if it is positive
  // (i.e. it is taken in the direction it was entered in the graph),
  // and the head or end-node otherwise. 'This' in Ebert's paper.
  NodeIndexType DirectArcTail(const ArcIndexType arc) const {
    return Tail(DirectArc(arc));
  }

  // Returns the head or end-node of arc if it is positive
  // (i.e. it is taken in the direction it was entered in the graph),
  // and the tail or start-node otherwise. 'That' in Ebert's paper.
  NodeIndexType DirectArcHead(const ArcIndexType arc) const {
    return Head(DirectArc(arc));
  }

  // Returns the arc in normal/direct direction.
  ArcIndexType DirectArc(const ArcIndexType arc) const {
    DCHECK(CheckArcValidity(arc));
    return std::max(arc, Opposite(arc));
  }

  // Returns the arc in reverse direction.
  ArcIndexType ReverseArc(const ArcIndexType arc) const {
    DCHECK(CheckArcValidity(arc));
    return std::min(arc, Opposite(arc));
  }

  // Returns the opposite arc, i.e the direct arc is the arc is in reverse
  // direction, and the reverse arc if the arc is direct.
  ArcIndexType Opposite(const ArcIndexType arc) const {
    const ArcIndexType opposite = ~arc;
    DCHECK(CheckArcValidity(arc));
    DCHECK(CheckArcValidity(opposite));
    return opposite;
  }

  // Returns true if the arc is direct.
  bool IsDirect(const ArcIndexType arc) const {
    DCHECK(CheckArcBounds(arc));
    return arc != kNilArc && arc >= 0;
  }

  // Returns true if the arc is in the reverse direction.
  bool IsReverse(const ArcIndexType arc) const {
    DCHECK(CheckArcBounds(arc));
    return arc != kNilArc && arc < 0;
  }

  // Returns true if arc is incident to node.
  bool IsOutgoingOrOppositeIncoming(ArcIndexType arc,
                                    NodeIndexType node) const {
    return Tail(arc) == node;
  }

  // Returns true if arc is incoming to node.
  bool IsIncoming(ArcIndexType arc, NodeIndexType node) const {
    return IsDirect(arc) && Head(arc) == node;
  }

  // Returns true if arc is outgoing from node.
  bool IsOutgoing(ArcIndexType arc, NodeIndexType node) const {
    return IsDirect(arc) && Tail(arc) == node;
  }

  // Recreates the next_adjacent_arc_ and first_incident_arc_ variables from
  // the array head_ in O(n + m) time.
  // This is useful if head_ array has been sorted according to a given
  // criterion, for example.
  void BuildRepresentation() {
    first_incident_arc_.SetAll(kNilArc);
    for (ArcIndexType arc = kFirstArc; arc < max_num_arcs_; ++arc) {
      Attach(arc);
    }
    representation_clean_ = true;
  }

  // Returns a debug std::string containing all the information contained in the
  // data structure in raw form.
  std::string DebugString() const {
    DCHECK(representation_clean_);
    std::string result = "Arcs:(node, next arc) :\n";
    for (ArcIndexType arc = -num_arcs_; arc < num_arcs_; ++arc) {
      result += " " + ArcDebugString(arc) + ":(" + NodeDebugString(head_[arc]) +
                "," + ArcDebugString(next_adjacent_arc_[arc]) + ")\n";
    }
    result += "Node:First arc :\n";
    for (NodeIndexType node = kFirstNode; node < num_nodes_; ++node) {
      result += " " + NodeDebugString(node) + ":" +
                ArcDebugString(first_incident_arc_[node]) + "\n";
    }
    return result;
  }

 private:
  // Handles reserving space in the next_adjacent_arc_ and head_
  // arrays, which are always present and are therefore in the base
  // class. Although they reside in the base class, those two arrays
  // are maintained differently by different derived classes,
  // depending on whether the derived class stores reverse arcs. Hence
  // the code to set those arrays up is in a method of the derived
  // class.
  void ReserveInternal(NodeIndexType new_max_num_nodes,
                       ArcIndexType new_max_num_arcs) {
    head_.Reserve(-new_max_num_arcs, new_max_num_arcs - 1);
    next_adjacent_arc_.Reserve(-new_max_num_arcs, new_max_num_arcs - 1);
    for (ArcIndexType arc = -new_max_num_arcs; arc < -max_num_arcs_; ++arc) {
      head_.Set(arc, kNilNode);
      next_adjacent_arc_.Set(arc, kNilArc);
    }
    for (ArcIndexType arc = max_num_arcs_; arc < new_max_num_arcs; ++arc) {
      head_.Set(arc, kNilNode);
      next_adjacent_arc_.Set(arc, kNilArc);
    }
  }

  // Returns the first incoming arc for node.
  ArcIndexType FirstIncomingArc(const NodeIndexType node) const {
    DCHECK_LE(kFirstNode, node);
    DCHECK_GE(max_num_nodes_, node);
    return FindNextIncomingArc(FirstOutgoingOrOppositeIncomingArc(node));
  }

  // Returns the incoming arc following the argument in the adjacency list.
  ArcIndexType NextIncomingArc(const ArcIndexType arc) const {
    DCHECK(CheckArcValidity(arc));
    DCHECK(IsReverse(arc));
    return FindNextIncomingArc(NextAdjacentArc(arc));
  }

  // Handles the part of AddArc() that is not in common with other
  // graph classes based on the EbertGraphBase template.
  void RecordArc(ArcIndexType arc, NodeIndexType tail, NodeIndexType head) {
    head_.Set(Opposite(arc), tail);
    head_.Set(arc, head);
    Attach(arc);
  }

  // Using the SetTail() method implies that the BuildRepresentation()
  // method must be called to restore consistency before the graph is
  // used.
  void SetTail(const ArcIndexType arc, const NodeIndexType tail) {
    representation_clean_ = false;
    head_.Set(Opposite(arc), tail);
  }

  // Utility method to attach a new arc.
  void Attach(ArcIndexType arc) {
    DCHECK(CheckArcValidity(arc));
    const NodeIndexType tail = head_[Opposite(arc)];
    DCHECK(IsNodeValid(tail));
    next_adjacent_arc_.Set(arc, first_incident_arc_[tail]);
    first_incident_arc_.Set(tail, arc);
    const NodeIndexType head = head_[arc];
    DCHECK(IsNodeValid(head));
    next_adjacent_arc_.Set(Opposite(arc), first_incident_arc_[head]);
    first_incident_arc_.Set(head, Opposite(arc));
  }

  // Utility method that finds the next outgoing arc.
  ArcIndexType FindNextOutgoingArc(ArcIndexType arc) const {
    DCHECK(CheckArcBounds(arc));
    while (IsReverse(arc)) {
      arc = NextAdjacentArc(arc);
      DCHECK(CheckArcBounds(arc));
    }
    return arc;
  }

  // Utility method that finds the next incoming arc.
  ArcIndexType FindNextIncomingArc(ArcIndexType arc) const {
    DCHECK(CheckArcBounds(arc));
    while (IsDirect(arc)) {
      arc = NextAdjacentArc(arc);
      DCHECK(CheckArcBounds(arc));
    }
    return arc;
  }
};

// A forward-star-only graph representation for greater efficiency in
// those algorithms that don't need reverse arcs.
template <typename NodeIndexType, typename ArcIndexType>
class ForwardEbertGraph
    : public EbertGraphBase<NodeIndexType, ArcIndexType,
                            ForwardEbertGraph<NodeIndexType, ArcIndexType> > {
  typedef EbertGraphBase<NodeIndexType, ArcIndexType,
                         ForwardEbertGraph<NodeIndexType, ArcIndexType> >
      Base;
  friend class EbertGraphBase<NodeIndexType, ArcIndexType,
                              ForwardEbertGraph<NodeIndexType, ArcIndexType> >;
  friend class StarGraphBase<NodeIndexType, ArcIndexType,
                             ForwardEbertGraph<NodeIndexType, ArcIndexType> >;

  using Base::ArcDebugString;
  using Base::Initialize;
  using Base::NextAdjacentArc;
  using Base::NodeDebugString;

  using Base::first_incident_arc_;
  using Base::head_;
  using Base::max_num_arcs_;
  using Base::max_num_nodes_;
  using Base::next_adjacent_arc_;
  using Base::num_arcs_;
  using Base::num_nodes_;
  using Base::representation_clean_;

 public:
#if !SWIG
  using Base::Head;
  using Base::IsNodeValid;

  using Base::kFirstArc;
  using Base::kFirstNode;
  using Base::kNilArc;
  using Base::kNilNode;
#endif  // SWIG

  typedef NodeIndexType NodeIndex;
  typedef ArcIndexType ArcIndex;

  ForwardEbertGraph() {}

  ForwardEbertGraph(NodeIndexType max_num_nodes, ArcIndexType max_num_arcs) {
    Initialize(max_num_nodes, max_num_arcs);
  }

  ~ForwardEbertGraph() {}

  // Utility function to check that an arc index is within the bounds.
  // It is exported so that users of the ForwardEbertGraph class can use it.
  // To be used in a DCHECK.
  bool CheckArcBounds(const ArcIndexType arc) const {
    return (arc == kNilArc) || (arc >= kFirstArc && arc < max_num_arcs_);
  }

  // Utility function to check that an arc index is within the bounds AND
  // different from kNilArc.
  // It is exported so that users of the ForwardEbertGraph class can use it.
  // To be used in a DCHECK.
  bool CheckArcValidity(const ArcIndexType arc) const {
    return (arc != kNilArc) && (arc >= kFirstArc && arc < max_num_arcs_);
  }

  // Returns true if arc is a valid index into the (*tail_) array.
  bool CheckTailIndexValidity(const ArcIndexType arc) const {
    return (tail_ != nullptr) && (arc >= kFirstArc) &&
           (arc <= tail_->max_index());
  }

  // Returns the tail or start-node of arc.
  NodeIndexType Tail(const ArcIndexType arc) const {
    DCHECK(CheckArcValidity(arc));
    DCHECK(CheckTailIndexValidity(arc));
    return (*tail_)[arc];
  }

  // Returns true if arc is incoming to node.
  bool IsIncoming(ArcIndexType arc, NodeIndexType node) const {
    return IsDirect(arc) && Head(arc) == node;
  }

  // Recreates the next_adjacent_arc_ and first_incident_arc_
  // variables from the arrays head_ and tail_ in O(n + m) time. This
  // is useful if the head_ and tail_ arrays have been sorted
  // according to a given criterion, for example.
  void BuildRepresentation() {
    first_incident_arc_.SetAll(kNilArc);
    DCHECK(TailArrayComplete());
    for (ArcIndexType arc = kFirstArc; arc < max_num_arcs_; ++arc) {
      DCHECK(CheckTailIndexValidity(arc));
      Attach((*tail_)[arc], arc);
    }
    representation_clean_ = true;
  }

  bool BuildTailArray() {
    // If (*tail_) is already allocated, we have the invariant that
    // its contents are canonical, so we do not need to do anything
    // here in that case except return true.
    if (tail_ == nullptr) {
      if (!representation_clean_) {
        // We have been asked to build the (*tail_) array, but we have
        // no valid information from which to build it. The graph is
        // in an unrecoverable, inconsistent state.
        return false;
      }
      // Reallocate (*tail_) and rebuild its contents from the
      // adjacency lists.
      tail_.reset(new ZVector<NodeIndexType>);
      tail_->Reserve(kFirstArc, max_num_arcs_ - 1);
      typename Base::NodeIterator node_it(*this);
      for (; node_it.Ok(); node_it.Next()) {
        NodeIndexType node = node_it.Index();
        typename Base::OutgoingArcIterator arc_it(*this, node);
        for (; arc_it.Ok(); arc_it.Next()) {
          (*tail_)[arc_it.Index()] = node;
        }
      }
    }
    DCHECK(TailArrayComplete());
    return true;
  }

  void ReleaseTailArray() { tail_.reset(nullptr); }

  // To be used in a DCHECK().
  bool TailArrayComplete() const {
    CHECK(tail_);
    for (ArcIndexType arc = kFirstArc; arc < num_arcs_; ++arc) {
      CHECK(CheckTailIndexValidity(arc));
      CHECK(IsNodeValid((*tail_)[arc]));
    }
    return true;
  }

  // Returns a debug std::string containing all the information contained in the
  // data structure in raw form.
  std::string DebugString() const {
    DCHECK(representation_clean_);
    std::string result = "Arcs:(node, next arc) :\n";
    for (ArcIndexType arc = kFirstArc; arc < num_arcs_; ++arc) {
      result += " " + ArcDebugString(arc) + ":(" + NodeDebugString(head_[arc]) +
                "," + ArcDebugString(next_adjacent_arc_[arc]) + ")\n";
    }
    result += "Node:First arc :\n";
    for (NodeIndexType node = kFirstNode; node < num_nodes_; ++node) {
      result += " " + NodeDebugString(node) + ":" +
                ArcDebugString(first_incident_arc_[node]) + "\n";
    }
    return result;
  }

 private:
  // Reserves space for the (*tail_) array.
  //
  // This method is separate from ReserveInternal() because our
  // practice of making the (*tail_) array optional implies that the
  // tail_ pointer might not be constructed when the ReserveInternal()
  // method is called. Therefore we have this method also, and we
  // ensure that it is called only when tail_ is guaranteed to have
  // been initialized.
  void ReserveTailArray(ArcIndexType new_max_num_arcs) {
    if (tail_ != nullptr) {
      // The (*tail_) values are already canonical, so we're just
      // reserving additional space for new arcs that haven't been
      // added yet.
      if (tail_->Reserve(kFirstArc, new_max_num_arcs - 1)) {
        for (ArcIndexType arc = tail_->max_index() + 1; arc < new_max_num_arcs;
             ++arc) {
          tail_->Set(arc, kNilNode);
        }
      }
    }
  }

  // Reserves space for the arrays indexed by arc indices, except
  // (*tail_) even if it is present. We cannot grow the (*tail_) array
  // in this method because this method is called from
  // Base::Reserve(), which in turn is called from the base template
  // class constructor. That base class constructor is called on *this
  // before tail_ is constructed. Hence when this method is called,
  // tail_ might contain garbage. This method can safely refer only to
  // fields of the base template class, not to fields of *this outside
  // the base template class.
  //
  // The strange situation in which this method of a derived class can
  // refer only to members of the base class arises because different
  // derived classes use the data members of the base class in
  // slightly different ways. The purpose of this derived class
  // method, then, is only to encode the derived-class-specific
  // conventions for how the derived class uses the data members of
  // the base class.
  //
  // To be specific, the forward-star graph representation, lacking
  // reverse arcs, allocates only the positive index range for the
  // head_ and next_adjacent_arc_ arrays, while the general
  // representation allocates space for both positive- and
  // negative-indexed arcs (i.e., both forward and reverse arcs).
  void ReserveInternal(NodeIndexType new_max_num_nodes,
                       ArcIndexType new_max_num_arcs) {
    head_.Reserve(kFirstArc, new_max_num_arcs - 1);
    next_adjacent_arc_.Reserve(kFirstArc, new_max_num_arcs - 1);
    for (ArcIndexType arc = max_num_arcs_; arc < new_max_num_arcs; ++arc) {
      head_.Set(arc, kNilNode);
      next_adjacent_arc_.Set(arc, kNilArc);
    }
    ReserveTailArray(new_max_num_arcs);
  }

  // Handles the part of AddArc() that is not in common wth other
  // graph classes based on the EbertGraphBase template.
  void RecordArc(ArcIndexType arc, NodeIndexType tail, NodeIndexType head) {
    head_.Set(arc, head);
    Attach(tail, arc);
  }

  // Using the SetTail() method implies that the BuildRepresentation()
  // method must be called to restore consistency before the graph is
  // used.
  void SetTail(const ArcIndexType arc, const NodeIndexType tail) {
    DCHECK(CheckTailIndexValidity(arc));
    CHECK(tail_);
    representation_clean_ = false;
    tail_->Set(arc, tail);
  }

  // Utility method to attach a new arc.
  void Attach(NodeIndexType tail, ArcIndexType arc) {
    DCHECK(CheckArcValidity(arc));
    DCHECK(IsNodeValid(tail));
    next_adjacent_arc_.Set(arc, first_incident_arc_[tail]);
    first_incident_arc_.Set(tail, arc);
    const NodeIndexType head = head_[arc];
    DCHECK(IsNodeValid(head));
    // Because Attach() is a public method, keeping (*tail_) canonical
    // requires us to record the new arc's tail here.
    if (tail_ != nullptr) {
      DCHECK(CheckTailIndexValidity(arc));
      tail_->Set(arc, tail);
    }
  }

  // Utility method that finds the next outgoing arc.
  ArcIndexType FindNextOutgoingArc(ArcIndexType arc) const {
    DCHECK(CheckArcBounds(arc));
    return arc;
  }

 private:
  // Always returns true because for any ForwardEbertGraph, only
  // direct arcs are represented, so all valid arc indices refer to
  // arcs that are outgoing from their tail nodes.
  bool IsOutgoing(const ArcIndex unused_arc,
                  const NodeIndex unused_node) const {
    return true;
  }

  // Always returns true because for any ForwardEbertGraph, only
  // outgoing arcs are represented, so all valid arc indices refer to
  // direct arcs.
  bool IsDirect(const ArcIndex unused_arc) const { return true; }

  // Array of node indices, not always present. (*tail_)[i] contains
  // the tail node of arc i. This array is not needed for normal graph
  // traversal operations, but is used in optimizing the graph's
  // layout so arcs are grouped by tail node, and can be used in one
  // approach to serializing the graph.
  //
  // Invariants: At any time when we are not executing a method of
  // this class, either tail_ == NULL or the tail_ array's contents
  // are kept canonical. If tail_ != NULL, any method that modifies
  // adjacency lists must also ensure (*tail_) is modified
  // correspondingly. The converse does not hold: Modifications to
  // (*tail_) are allowed without updating the adjacency lists. If
  // such modifications take place, representation_clean_ must be set
  // to false, of course, to indicate that the adjacency lists are no
  // longer current.
  std::unique_ptr<ZVector<NodeIndexType> > tail_;
};

// Traits for EbertGraphBase types, for use in testing and clients
// that work with both forward-only and forward/reverse graphs.
//
// The default is to assume reverse arcs so if someone forgets to
// specialize the traits of a new forward-only graph type, they will
// get errors from tests rather than incomplete testing.
template <typename GraphType>
struct graph_traits {
  static const bool has_reverse_arcs = true;
  static const bool is_dynamic = true;
};

template <typename NodeIndexType, typename ArcIndexType>
struct graph_traits<ForwardEbertGraph<NodeIndexType, ArcIndexType> > {
  static const bool has_reverse_arcs = false;
  static const bool is_dynamic = true;
};

template <typename NodeIndexType, typename ArcIndexType>
struct graph_traits<ForwardStaticGraph<NodeIndexType, ArcIndexType> > {
  static const bool has_reverse_arcs = false;
  static const bool is_dynamic = false;
};

namespace or_internal {

// The TailArrayBuilder class template is not expected to be used by
// clients. It is a helper for the TailArrayManager template.
//
// The TailArrayBuilder for graphs with reverse arcs does nothing.
template <typename GraphType, bool has_reverse_arcs>
struct TailArrayBuilder {
  explicit TailArrayBuilder(GraphType* unused_graph) {}

  bool BuildTailArray() const { return true; }
};

// The TailArrayBuilder for graphs without reverse arcs calls the
// appropriate method on the graph from the TailArrayBuilder
// constructor.
template <typename GraphType>
struct TailArrayBuilder<GraphType, false> {
  explicit TailArrayBuilder(GraphType* graph) : graph_(graph) {}

  bool BuildTailArray() const { return graph_->BuildTailArray(); }

  GraphType* const graph_;
};

// The TailArrayReleaser class template is not expected to be used by
// clients. It is a helper for the TailArrayManager template.
//
// The TailArrayReleaser for graphs with reverse arcs does nothing.
template <typename GraphType, bool has_reverse_arcs>
struct TailArrayReleaser {
  explicit TailArrayReleaser(GraphType* unused_graph) {}

  void ReleaseTailArray() const {}
};

// The TailArrayReleaser for graphs without reverse arcs calls the
// appropriate method on the graph from the TailArrayReleaser
// constructor.
template <typename GraphType>
struct TailArrayReleaser<GraphType, false> {
  explicit TailArrayReleaser(GraphType* graph) : graph_(graph) {}

  void ReleaseTailArray() const { graph_->ReleaseTailArray(); }

  GraphType* const graph_;
};

}  // namespace or_internal

template <typename GraphType>
class TailArrayManager {
 public:
  explicit TailArrayManager(GraphType* g) : graph_(g) {}

  bool BuildTailArrayFromAdjacencyListsIfForwardGraph() const {
    or_internal::TailArrayBuilder<GraphType,
                                  graph_traits<GraphType>::has_reverse_arcs>
        tail_array_builder(graph_);
    return tail_array_builder.BuildTailArray();
  }

  void ReleaseTailArrayIfForwardGraph() const {
    or_internal::TailArrayReleaser<GraphType,
                                   graph_traits<GraphType>::has_reverse_arcs>
        tail_array_releaser(graph_);
    tail_array_releaser.ReleaseTailArray();
  }

 private:
  GraphType* graph_;
};

template <typename GraphType>
class ArcFunctorOrderingByTailAndHead {
 public:
  explicit ArcFunctorOrderingByTailAndHead(const GraphType& graph)
      : graph_(graph) {}

  bool operator()(typename GraphType::ArcIndex a,
                  typename GraphType::ArcIndex b) const {
    return ((graph_.Tail(a) < graph_.Tail(b)) ||
            ((graph_.Tail(a) == graph_.Tail(b)) &&
             (graph_.Head(a) < graph_.Head(b))));
  }

 private:
  const GraphType& graph_;
};

namespace or_internal {

// The GraphBuilderFromArcs class template is not expected to be used
// by clients. It is a helper for the AnnotatedGraphBuildManager
// template.
//
// Deletes itself upon returning the graph!
template <typename GraphType, bool is_dynamic>
class GraphBuilderFromArcs {
 public:
  GraphBuilderFromArcs(typename GraphType::NodeIndex max_num_nodes,
                       typename GraphType::ArcIndex max_num_arcs,
                       bool sort_arcs)
      : num_arcs_(0), sort_arcs_(sort_arcs) {
    Reserve(max_num_nodes, max_num_arcs);
  }

  typename GraphType::ArcIndex AddArc(typename GraphType::NodeIndex tail,
                                      typename GraphType::NodeIndex head) {
    DCHECK_LT(num_arcs_, max_num_arcs_);
    DCHECK_LT(tail, GraphType::kFirstNode + max_num_nodes_);
    DCHECK_LT(head, GraphType::kFirstNode + max_num_nodes_);
    if (num_arcs_ < max_num_arcs_ &&
        tail < GraphType::kFirstNode + max_num_nodes_ &&
        head < GraphType::kFirstNode + max_num_nodes_) {
      typename GraphType::ArcIndex result = GraphType::kFirstArc + num_arcs_;
      arcs_.push_back(std::make_pair(tail, head));
      num_arcs_ += 1;
      return result;
    } else {
      // Too many arcs or node index out of bounds!
      return GraphType::kNilArc;
    }
  }

  // Builds the graph from the given arcs.
  GraphType* Graph(PermutationCycleHandler<typename GraphType::ArcIndex>*
                       client_cycle_handler) {
    GraphType* graph = new GraphType(max_num_nodes_, num_arcs_, sort_arcs_,
                                     &arcs_, client_cycle_handler);
    delete this;
    return graph;
  }

 private:
  bool Reserve(typename GraphType::NodeIndex new_max_num_nodes,
               typename GraphType::ArcIndex new_max_num_arcs) {
    max_num_nodes_ = new_max_num_nodes;
    max_num_arcs_ = new_max_num_arcs;
    arcs_.reserve(new_max_num_arcs);
    return true;
  }

  typename GraphType::NodeIndex max_num_nodes_;
  typename GraphType::ArcIndex max_num_arcs_;
  typename GraphType::ArcIndex num_arcs_;

  std::vector<
      std::pair<typename GraphType::NodeIndex, typename GraphType::NodeIndex> >
      arcs_;

  const bool sort_arcs_;
};

// Trivial delegating specialization for dynamic graphs.
//
// Deletes itself upon returning the graph!
template <typename GraphType>
class GraphBuilderFromArcs<GraphType, true> {
 public:
  GraphBuilderFromArcs(typename GraphType::NodeIndex max_num_nodes,
                       typename GraphType::ArcIndex max_num_arcs,
                       bool sort_arcs)
      : graph_(new GraphType(max_num_nodes, max_num_arcs)),
        sort_arcs_(sort_arcs) {}

  bool Reserve(const typename GraphType::NodeIndex new_max_num_nodes,
               const typename GraphType::ArcIndex new_max_num_arcs) {
    return graph_->Reserve(new_max_num_nodes, new_max_num_arcs);
  }

  typename GraphType::ArcIndex AddArc(
      const typename GraphType::NodeIndex tail,
      const typename GraphType::NodeIndex head) {
    return graph_->AddArc(tail, head);
  }

  GraphType* Graph(PermutationCycleHandler<typename GraphType::ArcIndex>*
                       client_cycle_handler) {
    if (sort_arcs_) {
      TailArrayManager<GraphType> tail_array_manager(graph_);
      tail_array_manager.BuildTailArrayFromAdjacencyListsIfForwardGraph();
      ArcFunctorOrderingByTailAndHead<GraphType> arc_ordering(*graph_);
      graph_->GroupForwardArcsByFunctor(arc_ordering, client_cycle_handler);
      tail_array_manager.ReleaseTailArrayIfForwardGraph();
    }
    GraphType* result = graph_;
    delete this;
    return result;
  }

 private:
  GraphType* const graph_;
  const bool sort_arcs_;
};

}  // namespace or_internal

template <typename GraphType>
class AnnotatedGraphBuildManager
    : public or_internal::GraphBuilderFromArcs<
          GraphType, graph_traits<GraphType>::is_dynamic> {
 public:
  AnnotatedGraphBuildManager(typename GraphType::NodeIndex num_nodes,
                             typename GraphType::ArcIndex num_arcs,
                             bool sort_arcs)
      : or_internal::GraphBuilderFromArcs<GraphType,
                                          graph_traits<GraphType>::is_dynamic>(
            num_nodes, num_arcs, sort_arcs) {}
};

// Builds a directed line graph for 'graph' (see "directed line graph" in
// http://en.wikipedia.org/wiki/Line_graph). Arcs of the original graph
// become nodes and the new graph contains only nodes created from arcs in the
// original graph (we use the notation (a->b) for these new nodes); the index
// of the node (a->b) in the new graph is exactly the same as the index of the
// arc a->b in the original graph.
// An arc from node (a->b) to node (c->d) in the new graph is added if and only
// if b == c in the original graph.
// This method expects that 'line_graph' is an empty graph (it has no nodes
// and no arcs).
// Returns false on an error.
template <typename GraphType>
bool BuildLineGraph(const GraphType& graph, GraphType* const line_graph) {
  if (line_graph == nullptr) {
    LOG(DFATAL) << "line_graph must not be NULL";
    return false;
  }
  if (line_graph->num_nodes() != 0) {
    LOG(DFATAL) << "line_graph must be empty";
    return false;
  }
  typedef typename GraphType::ArcIterator ArcIterator;
  typedef typename GraphType::OutgoingArcIterator OutgoingArcIterator;
  // Sizing then filling.
  typename GraphType::ArcIndex num_arcs = 0;
  for (ArcIterator arc_iterator(graph); arc_iterator.Ok();
       arc_iterator.Next()) {
    const typename GraphType::ArcIndex arc = arc_iterator.Index();
    const typename GraphType::NodeIndex head = graph.Head(arc);
    for (OutgoingArcIterator iterator(graph, head); iterator.Ok();
         iterator.Next()) {
      ++num_arcs;
    }
  }
  line_graph->Reserve(graph.num_arcs(), num_arcs);
  for (ArcIterator arc_iterator(graph); arc_iterator.Ok();
       arc_iterator.Next()) {
    const typename GraphType::ArcIndex arc = arc_iterator.Index();
    const typename GraphType::NodeIndex head = graph.Head(arc);
    for (OutgoingArcIterator iterator(graph, head); iterator.Ok();
         iterator.Next()) {
      line_graph->AddArc(arc, iterator.Index());
    }
  }
  return true;
}

}  // namespace operations_research
#endif  // OR_TOOLS_GRAPH_EBERT_GRAPH_H_
