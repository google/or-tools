// Copyright 2010-2011 Google
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

// An implementation (with some improvements) of the star-representation of a
// graph as described in J. Ebert, "A versatile data structure for
// edge-oriented graph algorithms." Communications of the ACM 30(6):513-519
// (June 1987). http://portal.acm.org/citation.cfm?id=214769
// Both forward- and backward-star representations are contained in this
// representation.

// The graph is represented with three arrays.
// Let n be the number of nodes and m be the number of arcs.
// Let i be an integer in [0..m-1], denoting the index of an arc.
//  * node_[i] contains the end-node of arc i,
//  * node_[-i-1] contains the start-node of arc i.
// Note that in two's-complement arithmetic, -i-1 = ~i.
// Consequently:
//  * node_[~i] contains the start-node of the arc reverse to arc i,
//  * node_[i] contains the end-node of the arc reverse to arc i.
//  Note that if arc (u, v) is defined, then the data structure also stores
//  (v, u).
//  Arc ~i thus denotes the arc reverse to arc i.
//  This is what makes this representation useful for undirected graphs and for
//  implementing algorithms like bi-directional shortest-path.
//  Also note that the representation handles multi-graphs. If several arcs
//  going from node u to node v are added to the graph, they will be handled as
//  separate arcs.
//
// Now, for an integer u in [0..n-1] denoting the index of a node:
//  * first_incident_arc_[u] denotes the first arc in the adjacency list of u.
//  * going from an arc i, the adjacency list can be traversed using
//    j = next_adjacent_arc_[i].
//
// This implementation has the following benefits:
//  * It is able to handle both directed or undirected graphs.
//  * Being based on indices, it is easily serializable. Only the contents
//    of the node_ array needs to be stored.
//  * The sizes of node indices and arc indices can be stored in 32 bits, while
//    still allowing to go a bit further than the 4-gigabyte limitation
//    (48 gigabytes for a pure graph, without capacities or costs.)
//  * The representation can be recomputed if edges have been loaded from
//  * The representation can be recomputed if edges have been loaded from
//    external memory or if edges have been re-ordered.
//  * The memory consumption is: 2 * m * sizeof(NodeIndexType)
//                             + 2 * m * sizeof(ArcIndexType)
//                             + n * sizeof(ArcIndexType)
//
// This implementation differs from the implementation described in [Ebert 1987]
// in the following respects:
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
//  * TODO(user) it is possible to group all the outgoing (resp. incoming) arcs
//    of a node to allow to traverse the outgoing (resp. incoming) arcs in
//    O(out_degree(node)) (resp. O(in_degree(node))) instead of O(degree(node)).
//  * TODO(user) it is possible to implement arc deletion and garbage collection
//    in an efficient (relatively) manner. For the time being we haven't seen an
//    application to this.

#include <limits.h>
#include <stddef.h>
#include <algorithm>
#include <string>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "util/permutation.h"
#include "util/zvector.h"

using std::string;

namespace operations_research {

// Most users should only use StarGraph, which is EbertGraph<int32, int32>, and
// other type shortcuts; see the bottom of this file.
template<typename NodeIndexType, typename ArcIndexType> class EbertGraph {
 public:
  // The index of the 'nil' node in the graph.
  static const NodeIndexType kNilNode;

  // The index of the 'nil' arc in the graph.
  static const ArcIndexType kNilArc;

  // The index of the first node in the graph.
  static const NodeIndexType kFirstNode;

  // The index of the first arc in the graph.
  static const ArcIndexType kFirstArc;

  // The maximum possible node index in the graph.
  static const NodeIndexType kMaxNumNodes;

  // The maximun possible arc index in the graph.
  static const ArcIndexType kMaxNumArcs;

  EbertGraph()
      : max_num_nodes_(0),
        max_num_arcs_(0),
        num_nodes_(0),
        num_arcs_(0),
        node_(),
        next_adjacent_arc_(),
        first_incident_arc_(),
        representation_clean_(true) {}

  EbertGraph(NodeIndexType max_num_nodes, ArcIndexType max_num_arcs)
      : max_num_nodes_(0),
        max_num_arcs_(0),
        num_nodes_(0),
        num_arcs_(0),
        node_(),
        next_adjacent_arc_(),
        first_incident_arc_(),
        representation_clean_(true) {
    if (!Reserve(max_num_nodes, max_num_arcs)) {
      LOG(DFATAL) << "Could not reserve memory for "
                  << static_cast<int64>(max_num_nodes) << " nodes and "
                  << static_cast<int64>(max_num_arcs) << " arcs.";
    }
    first_incident_arc_.SetAll(kNilArc);
    next_adjacent_arc_.SetAll(kNilArc);
    node_.SetAll(kNilNode);
  }

  ~EbertGraph() {}

  // Reserves memory needed for max_num_nodes nodes and max_num_arcs arcs.
  // Returns false if the parameters passed are not OK.
  // It can be used to enlarge the graph, but does not shrink memory
  // if called with smaller values.
  bool Reserve(NodeIndexType new_max_num_nodes, ArcIndexType new_max_num_arcs) {
    if (new_max_num_nodes < 1 || new_max_num_nodes > kMaxNumNodes) {
      return false;
    }
    if (new_max_num_arcs < 1 || new_max_num_arcs > kMaxNumArcs) {
      return false;
    }
    node_.Reserve(-new_max_num_arcs, new_max_num_arcs - 1);
    next_adjacent_arc_.Reserve(-new_max_num_arcs, new_max_num_arcs - 1);
    for (ArcIndexType arc = -new_max_num_arcs; arc < -max_num_arcs_; ++arc) {
      node_.Set(arc, kNilNode);
      next_adjacent_arc_.Set(arc, kNilArc);
    }
    for (ArcIndexType arc = max_num_arcs_; arc < new_max_num_arcs; ++arc) {
      node_.Set(arc, kNilNode);
      next_adjacent_arc_.Set(arc, kNilArc);
    }
    first_incident_arc_.Reserve(kFirstNode, new_max_num_nodes - 1);
    for (NodeIndexType node = max_num_nodes_;
         node < new_max_num_nodes; ++node) {
      first_incident_arc_.Set(node, kNilArc);
    }
    max_num_nodes_ = new_max_num_nodes;
    max_num_arcs_ = new_max_num_arcs;
    return true;
  }

  // Returns the number of nodes in the graph.
  NodeIndexType num_nodes() const { return num_nodes_; }

  // Returns the number of original arcs in the graph
  // (The ones with positive indices.)
  ArcIndexType num_arcs() const { return num_arcs_; }

  // Returns one more than the largest index of an extant node. To be
  // used as a helper when clients need to dimension or iterate over
  // arrays of node annotation information.
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

  // Returns true if node is in the range [kFirstNode .. max_num_nodes_).
  bool IsNodeValid(NodeIndexType node) const {
    return node >= kFirstNode && node < max_num_nodes_;
  }

  // Adds an arc to the graph and returns its index.
  // Returns kNilArc if the arc could not be added.
  // Note that for a given pair (tail, head) AddArc does not overwrite an
  // already-existing arc between tail and head: Another arc is created
  // instead. This makes it possible to handle multi-graphs.
  ArcIndexType AddArc(NodeIndexType tail, NodeIndexType head) {
    if (num_arcs_ >= max_num_arcs_
        || !IsNodeValid(tail) || !IsNodeValid(head)) {
      return kNilArc;
    }
    if (tail + 1 > num_nodes_) {
      num_nodes_ = tail + 1;   // std::max does not work on int16.
    }
    if (head + 1 > num_nodes_) {
      num_nodes_ = head + 1;
    }
    ArcIndexType arc = num_arcs_;
    ++num_arcs_;
    node_.Set(Opposite(arc), tail);
    node_.Set(arc, head);
    Attach(arc);
    return arc;
  }

  // For some reason SWIG seems unable to handle the definition of
  // class CycleHandlerForAnnotatedArcs. I have no idea why we use
  // such a horrible tool that doesn't say anything except "error."
#if !SWIG
  template <typename ArcIndexTypeStrictWeakOrderingFunctor>
  void GroupForwardArcsByFunctor(
      const ArcIndexTypeStrictWeakOrderingFunctor& compare,
      PermutationCycleHandler<ArcIndexType>* annotation_handler) {
    // The following cannot be a PackedArray<kArcIndexSize> instance
    // because using the STL sort() requires us to iterate over the
    // permutation with STL iterators. Indices from 0 through
    // kFirstArc - 1 are unused. Today that's only index 0.
    scoped_array<ArcIndexType>
        arc_permutation(new ArcIndexType[end_arc_index()]);

    // Determine the permutation that groups arcs by their tail nodes.
    for (ArcIndexType i = 0; i < end_arc_index(); ++i) {
      // Start with the identity permutation.
      arc_permutation[i] = i;
    }
    std::sort(&arc_permutation[kFirstArc],
              &arc_permutation[end_arc_index()],
              compare);

    // Now we actually permute the node_ array and the
    // scaled_arc_cost_ array according to the sorting permutation.
    CycleHandlerForAnnotatedArcs cycle_handler(annotation_handler, this);
    PermutationApplier<ArcIndexType> permutation(&cycle_handler);
    permutation.Apply(&arc_permutation[0], kFirstArc, end_arc_index());

    // Finally, rebuild the graph from its permuted node_ array.
    BuildRepresentation();
  }

  class CycleHandlerForAnnotatedArcs :
      public PermutationCycleHandler<ArcIndexType> {
   public:
    CycleHandlerForAnnotatedArcs(
        PermutationCycleHandler<ArcIndexType>* annotation_handler,
        EbertGraph* graph)
        : annotation_handler_(annotation_handler),
          graph_(graph),
          head_temp_(kNilNode),
          tail_temp_(kNilNode) { }

    virtual void SetTempFromIndex(ArcIndexType source) {
      if (annotation_handler_ != NULL) {
        annotation_handler_->SetTempFromIndex(source);
      }
      head_temp_ = graph_->Head(source);
      tail_temp_ = graph_->Tail(source);
    }

    virtual void SetIndexFromIndex(ArcIndexType source,
                                   ArcIndexType destination) const {
      if (annotation_handler_ != NULL) {
        annotation_handler_->SetIndexFromIndex(source, destination);
      }
      graph_->SetHead(destination, graph_->Head(source));
      graph_->SetTail(destination, graph_->Tail(source));
    }

    virtual void SetIndexFromTemp(ArcIndexType destination) const {
      if (annotation_handler_ != NULL) {
        annotation_handler_->SetIndexFromTemp(destination);
      }
      graph_->SetHead(destination, head_temp_);
      graph_->SetTail(destination, tail_temp_);
    }

    // Since arc grouping works only with forward arcs, we use the
    // forward/reverse bit of information encoded in the ArcIndexType to
    // indicate whether this index has already been seen in processing
    // the permutation. The permutation starts out with all indices
    // referring to forward arcs. As each arc is moved according to
    // the permutation, its index is switched to its opposite to keep
    // track of which arcs have already been moved. In this way we
    // don't need any extra storage to keep track of this information,
    // and ArcIndexType is guaranteed to be able to encode it since it has
    // to be able to encode forward/reverse.
    virtual void SetSeen(ArcIndexType* permutation_element) const {
      *permutation_element = graph_->Opposite(*permutation_element);
    }

    virtual bool Unseen(ArcIndexType permutation_element) const {
      return graph_->IsDirect(permutation_element);
    }

    virtual ~CycleHandlerForAnnotatedArcs() { }

   private:
    PermutationCycleHandler<ArcIndexType>* annotation_handler_;
    EbertGraph* graph_;
    NodeIndexType head_temp_;
    NodeIndexType tail_temp_;
  };
#endif  // SWIG

  // Iterator class for traversing all the nodes in the graph.
  class NodeIterator {
   public:
    explicit NodeIterator(const EbertGraph& graph)
        : graph_(graph), node_(graph_.StartNode(kFirstNode)) {}

    // Returns true unless all the nodes have been traversed.
    bool Ok() const { return node_ != kNilNode; }

    // Advances the current node index.
    void Next() { node_ = graph_.NextNode(node_); }

    // Returns the index of the node currently pointed to by the iterator.
    NodeIndexType Index() const { return node_; }

   private:
    // A reference to the current EbertGraph considered.
    const EbertGraph& graph_;

    // The index of the current node considered.
    NodeIndexType node_;
  };

  // Iterator class for traversing the arcs in the graph.
  class ArcIterator {
   public:
    explicit ArcIterator(const EbertGraph& graph)
        : graph_(graph), arc_(graph_.StartArc(kFirstArc)) {}

    // Returns true unless all the arcs have been traversed.
    bool Ok() const { return arc_ != kNilArc; }

    // Advances the current arc index.
    void Next() { arc_ = graph_.NextArc(arc_); }

    // Returns the index of the arc currently pointed to by the iterator.
    ArcIndexType Index() const { return arc_; }

   private:
    // A reference to the current EbertGraph considered.
    const EbertGraph& graph_;

    // The index of the current arc considered.
    ArcIndexType arc_;
  };

  // Iterator class for traversing the arcs incident to a given node in the
  // graph.
  class IncidentArcIterator {
   public:
    IncidentArcIterator(const EbertGraph& graph, NodeIndexType node)
        : graph_(graph),
          node_(graph_.StartNode(node)),
          arc_(graph_.StartArc(graph_.FirstIncidentArc(node))) {
      DCHECK(CheckInvariant());
    }

    // This constructor takes an arc as extra argument and makes the iterator
    // start at arc.
    IncidentArcIterator(const EbertGraph& graph,
                        NodeIndexType node,
                        ArcIndexType arc)
        : graph_(graph),
          node_(graph_.StartNode(node)),
          arc_(graph_.StartArc(arc)) {
      DCHECK(CheckInvariant());
    }

    // Can only assign from an iterator on the same graph.
    void operator=(const IncidentArcIterator& iterator) {
      DCHECK(&iterator.graph_ ==  &graph_);
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
      DCHECK(graph_.IsIncident(arc_, node_));
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
  // Note that the indices of these arc are negative, i.e. it's actually
  // their corresponding direct arcs that are incoming to the node.
  // The API has been designed in this way to have the set of arcs iterated
  // by IncidentArcIterator to be the union of the sets of arcs iterated by
  // IncomingArcIterator and OutgoingArcIterator.
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
    IncomingArcIterator(const EbertGraph& graph,
                        NodeIndexType node,
                        ArcIndexType arc)
        : graph_(graph),
          node_(graph_.StartNode(node)),
          arc_(graph_.StartArc(arc)) {
      DCHECK(CheckInvariant());
    }

    // Can only assign from an iterator on the same graph.
    void operator=(const IncomingArcIterator& iterator) {
      DCHECK(&iterator.graph_ ==  &graph_);
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
    ArcIndexType Index() const { return arc_; }

   private:
    // Returns true if the invariant for the iterator is verified.
    // To be used in a DCHECK.
    bool CheckInvariant() const {
      if (arc_ == kNilArc) {
        return true;  // This occurs when the iterator has reached the end.
      }
      DCHECK(graph_.IsIncoming(arc_, node_));
      return true;
    }
    // A reference to the current EbertGraph considered.
    const EbertGraph& graph_;

    // The index of the node on which arcs are iterated.
    NodeIndexType node_;

    // The index of the current arc considered.
    ArcIndexType arc_;
  };

  // Iterator class for traversing the outgoing arcs associated to a given node.
  class OutgoingArcIterator {
   public:
    OutgoingArcIterator(const EbertGraph& graph, NodeIndexType node)
        : graph_(graph),
          node_(graph_.StartNode(node)),
          arc_(graph_.StartArc(graph_.FirstOutgoingArc(node))) {
      DCHECK(CheckInvariant());
    }

    // This constructor takes an arc as extra argument and makes the iterator
    // start at arc.
    OutgoingArcIterator(const EbertGraph& graph,
                        NodeIndexType node,
                        ArcIndexType arc)
        : graph_(graph),
          node_(graph_.StartNode(node)),
          arc_(graph_.StartArc(arc)) {
      DCHECK(CheckInvariant());
    }

    // Can only assign from an iterator on the same graph.
    void operator=(const OutgoingArcIterator& iterator) {
      DCHECK(&iterator.graph_ ==  &graph_);
      node_ = iterator.node_;
      arc_ = iterator.arc_;
    }

    // Returns true unless all the outgoing arcs have been traversed.
    bool Ok() const { return arc_ != kNilArc; }

    // Advances the current outgoing arc index.
    void Next() {
      arc_ = graph_.NextOutgoingArc(arc_);
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

    // A reference to the current EbertGraph considered.
    const EbertGraph& graph_;

    // The index of the node on which arcs are iterated.
    NodeIndexType node_;

    // The index of the current arc considered.
    ArcIndexType arc_;
  };

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

  // Utility function to check that a node index is within the bounds AND
  // different from kNilNode.
  // It is exported so that users of the EbertGraph class can use it.
  // To be used in a DCHECK.
  bool CheckNodeValidity(const NodeIndexType node) const {
    return node >= kFirstNode && node < max_num_nodes_;
  }

  // Returns the tail or start-node of arc.
  NodeIndexType Tail(const ArcIndexType arc) const {
    DCHECK(CheckArcValidity(arc));
    return node_[Opposite(arc)];
  }

  // Returns the head or end-node of arc.
  NodeIndexType Head(const ArcIndexType arc) const {
    DCHECK(CheckArcValidity(arc));
    return node_[arc];
  }

  // Returns the first arc going from tail to head, if it exists, or kNilArc
  // if such an arc does not exist.
  ArcIndexType LookUpArc(const NodeIndexType tail,
                         const NodeIndexType head) const {
    for (ArcIndexType arc = FirstOutgoingArc(tail);
         arc != kNilArc;
         arc = NextOutgoingArc(arc)) {
      if (Head(arc) == head) {
        return arc;
      }
    }
    return kNilArc;
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
  bool IsIncident(ArcIndexType arc, NodeIndexType node) const {
    return IsIncoming(arc, node) || IsOutgoing(arc, node);
  }

  // Returns true if arc is incoming to node.
  bool IsIncoming(ArcIndexType arc, NodeIndexType node) const {
    return DirectArcHead(arc) == node;
  }

  // Returns true if arc is outgoing from node.
  bool IsOutgoing(ArcIndexType arc, NodeIndexType node) const {
    return DirectArcTail(arc) == node;
  }

  // Recreates the next_adjacent_arc_ and first_incident_arc_ variables from
  // the array node_ in O(n + m) time.
  // This is useful if node_ array has been sorted according to a given
  // criterion, for example.
  void BuildRepresentation() {
    first_incident_arc_.SetAll(kNilArc);
    for (ArcIndexType arc = kFirstArc; arc <  max_num_arcs_; ++arc) {
      Attach(arc);
    }
    representation_clean_ = true;
  }

  // Returns a debug string containing all the information contained in the
  // data structure in raw form.
  string DebugString() const {
    DCHECK(representation_clean_);
    string result = "Arcs:(node, next arc) :\n";
    for (ArcIndexType arc = -num_arcs_; arc < num_arcs_; ++arc) {
      result += " " + ArcDebugString(arc) + ":(" + NodeDebugString(node_[arc])
              + "," + ArcDebugString(next_adjacent_arc_[arc]) + ")\n";
    }
    result += "Node:First arc :\n";
    for (NodeIndexType node = kFirstNode; node < num_nodes_; ++node) {
      result += " " + NodeDebugString(node) + ":"
              + ArcDebugString(first_incident_arc_[node]) + "\n";
    }
    return result;
  }

  string NodeDebugString(const NodeIndexType node) const {
    if (node == kNilNode) {
      return "NilNode";
    } else {
      return StringPrintf("%lld", static_cast<int64>(node));
    }
  }

  string ArcDebugString(const ArcIndexType arc) const {
    if (arc == kNilArc) {
      return "NilArc";
    } else {
      return StringPrintf("%lld", static_cast<int64>(arc));
    }
  }

 private:
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

  // Returns the first outgoing arc for node.
  ArcIndexType FirstOutgoingArc(const NodeIndexType node) const {
    DCHECK(CheckNodeValidity(node));
    return FindNextOutgoingArc(FirstIncidentArc(node));
  }

  // Returns the outgoing arc following the argument in the adjacency list.
  ArcIndexType NextOutgoingArc(const ArcIndexType arc) const {
    DCHECK(CheckArcValidity(arc));
    DCHECK(IsDirect(arc));
    return FindNextOutgoingArc(NextAdjacentArc(arc));
  }

  // Returns the first incoming arc for node.
  ArcIndexType FirstIncomingArc(const NodeIndexType node) const {
    DCHECK_LE(kFirstNode, node);
    DCHECK_GE(max_num_nodes_, node);
    return FindNextIncomingArc(FirstIncidentArc(node));
  }

  // Returns the incoming arc following the argument in the adjacency list.
  ArcIndexType NextIncomingArc(const ArcIndexType arc) const {
    DCHECK(CheckArcValidity(arc));
    DCHECK(IsReverse(arc));
    return FindNextIncomingArc(NextAdjacentArc(arc));
  }

  // Returns the first arc in node's incidence list.
  ArcIndexType FirstIncidentArc(const NodeIndexType node) const {
    DCHECK(representation_clean_);
    DCHECK(CheckNodeValidity(node));
    return first_incident_arc_[node];
  }

  // Returns the next arc following the passed argument in its adjacency list.
  ArcIndexType NextAdjacentArc(const ArcIndexType arc) const {
    DCHECK(representation_clean_);
    DCHECK(CheckArcValidity(arc));
    return next_adjacent_arc_[arc];
  }

  // Returns the node following the argument in the graph.
  // Returns kNilNode (= end) if the range of nodes has been exhausted.
  // It is called by NodeIterator::Next() and as such does not expect do be
  // passed an argument equal to kNilNode.
  // This is why the return line is simplified from
  // return (node == kNilNode || next_node >= num_nodes_)
  //        ? kNilNode : next_node;
  // to
  // return next_node < num_nodes_ ? next_node : kNilNode;
  NodeIndexType NextNode(const NodeIndexType node) const {
    DCHECK(CheckNodeValidity(node));
    const NodeIndexType next_node = node + 1;
    return next_node < num_nodes_ ? next_node : kNilNode;
  }

  // Returns the arc following the argument in the graph.
  // Returns kNilArc (= end) if the range of arcs has been exhausted.
  // It is called by ArcIterator::Next() and as such does not expect do be
  // passed an argument equal to kNilArc.
  // This is why the return line is simplified from
  // return ( arc == kNilArc || next_arc >= num_arcs_) ? kNilArc : next_arc;
  // to
  // return next_arc < num_arcs_ ? next_arc : kNilArc;
  ArcIndexType NextArc(const ArcIndexType arc) const {
    DCHECK(CheckArcValidity(arc));
    const ArcIndexType next_arc = arc + 1;
    return next_arc < num_arcs_ ? next_arc : kNilArc;
  }

  // Using the SetTail() method implies that the BuildRepresentation()
  // method must be called to restore consistency before the graph is
  // used.
  void SetTail(const ArcIndexType arc, const NodeIndexType tail) {
    representation_clean_ = false;
    node_.Set(Opposite(arc), tail);
  }

  // Using the SetHead() method implies that the BuildRepresentation()
  // method must be called to restore consistency before the graph is
  // used.
  void SetHead(const ArcIndexType arc, const NodeIndexType head) {
    representation_clean_ = false;
    node_.Set(arc, head);
  }

  // Utility method to attach a new arc.
  void Attach(ArcIndexType arc) {
    DCHECK(CheckArcValidity(arc));
    const NodeIndexType tail = node_[Opposite(arc)];
    DCHECK(CheckNodeValidity(tail));
    next_adjacent_arc_.Set(arc, first_incident_arc_[tail]);
    first_incident_arc_.Set(tail, arc);
    const NodeIndexType head = node_[arc];
    DCHECK(CheckNodeValidity(head));
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

  // The maximum number of nodes that the graph can hold.
  NodeIndexType max_num_nodes_;

  // The maximum number of arcs that the graph can hold.
  ArcIndexType max_num_arcs_;

  // The maximum index of the node currently held by the graph.
  NodeIndexType num_nodes_;

  // The current number of arcs held by the graph.
  ArcIndexType num_arcs_;

  // Array of node indices. node_[i] contains the tail node of arc i.
  ZVector<NodeIndexType> node_;

  // Array of next indices.
  // next_adjacent_arc_[i] contains the next arc in the adjacency list of arc i.
  ZVector<ArcIndexType> next_adjacent_arc_;

  // Array of arc indices. first_incident_arc_[i] contains the first arc
  // incident to node i.
  ZVector<ArcIndexType> first_incident_arc_;

  // Flag to indicate that BuildRepresentation() needs to be called
  // before the adjacency lists are examined. Only for DCHECK in debug
  // builds.
  bool representation_clean_;
};

template<typename NodeIndexType, typename ArcIndexType>
const NodeIndexType EbertGraph<NodeIndexType, ArcIndexType>::kNilNode = -1;

// The index of the 'nil' arc in the graph.
template<typename NodeIndexType, typename ArcIndexType>
const ArcIndexType EbertGraph<NodeIndexType, ArcIndexType>::kNilArc =
    std::numeric_limits<ArcIndexType>::min();

// The index of the first node in the graph.
template<typename NodeIndexType, typename ArcIndexType>
const NodeIndexType EbertGraph<NodeIndexType, ArcIndexType>::kFirstNode = 0;

// The index of the first arc in the graph.
template<typename NodeIndexType, typename ArcIndexType>
const ArcIndexType EbertGraph<NodeIndexType, ArcIndexType>::kFirstArc = 0;

// The maximum possible node index in the graph.
template<typename NodeIndexType, typename ArcIndexType>
const NodeIndexType EbertGraph<NodeIndexType, ArcIndexType>::kMaxNumNodes =
    std::numeric_limits<NodeIndexType>::max();

// The maximum possible number of arcs in the graph.
// (The maximum index is kMaxNumArcs-1, since indices start at 0.)
template<typename NodeIndexType, typename ArcIndexType>
const ArcIndexType EbertGraph<NodeIndexType, ArcIndexType>::kMaxNumArcs =
    std::numeric_limits<ArcIndexType>::max();

// Standard instantiation of EbertGraph, named 'StarGraph', and relevant type
// shortcuts. Users are encouraged to use StarGraph, and the other type
// shortcuts below unless their use cases prevents them from doing so.

typedef int32 NodeIndex;
typedef int32 ArcIndex;
typedef int64 FlowQuantity;
typedef int64 CostValue;
typedef EbertGraph<NodeIndex, ArcIndex> StarGraph;
typedef ZVector<NodeIndex> NodeIndexArray;
typedef ZVector<ArcIndex> ArcIndexArray;
typedef ZVector<FlowQuantity> QuantityArray;
typedef ZVector<CostValue> CostArray;

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
template<typename NodeIndexType, typename ArcIndexType>
bool BuildLineGraph(const EbertGraph<NodeIndexType, ArcIndexType>& graph,
                    EbertGraph<NodeIndexType, ArcIndexType>* const line_graph) {
  if (line_graph == NULL) {
    LOG(DFATAL) << "line_graph must not be NULL";
    return false;
  }
  if (line_graph->num_nodes() != 0) {
    LOG(DFATAL) << "line_graph must be empty";
    return false;
  }
  typedef EbertGraph<NodeIndexType, ArcIndexType> Graph;
  typedef typename Graph::ArcIterator ArcIterator;
  typedef typename Graph::OutgoingArcIterator OutgoingArcIterator;
  // Sizing then filling.
  const NodeIndexType num_nodes = graph.num_arcs();
  ArcIndexType num_arcs = 0;
  for (ArcIterator arc_iterator(graph);
       arc_iterator.Ok();
       arc_iterator.Next()) {
    const ArcIndexType arc = arc_iterator.Index();
    const NodeIndexType head = graph.Head(arc);
    for (OutgoingArcIterator iterator(graph, head);
         iterator.Ok();
         iterator.Next()) {
      ++num_arcs;
    }
  }
  line_graph->Reserve(num_nodes, num_arcs);
  for (ArcIterator arc_iterator(graph);
       arc_iterator.Ok();
       arc_iterator.Next()) {
    const ArcIndexType arc = arc_iterator.Index();
    const NodeIndexType head = graph.Head(arc);
    for (OutgoingArcIterator iterator(graph, head);
         iterator.Ok();
         iterator.Next()) {
      line_graph->AddArc(arc, iterator.Index());
    }
  }
  return true;
}

}  // namespace operations_research
#endif  // OR_TOOLS_GRAPH_EBERT_GRAPH_H_
