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
//  This is what makes this representation useful for undirected graphs,
//  and for implementing algorithms like two-directions shortest-path.
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
//  * The sizes of node indices and arc indices can be specified in number of
//    bytes. For example, it is possible to store indices on 5 bytes or 40 bits,
//    which seems like a good compromise for the architectures of 2010. This
//    represents a 37.5% savings compared to a pointer-based implementation.
//    Taking into account the fact that no extra pointer to the reverse arc is
//    needed, only 80 bits are needed to store an arc instead of 192 bits if a
//    pointer-based representation were used.
//  * The representation can be recomputed if edges have been loaded from
//    external memory or if edges have been re-ordered.
//  * The memory consumption is: 2 * m * NodeIndexSize
//                             + 2 * m * ArcIndexSize
//                             + n * ArcIndexSize
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
#include "util/packed_array.h"
#include "util/permutation.h"

using std::string;

namespace operations_research {
typedef int64 NodeIndex;
typedef int64 ArcIndex;
typedef int64 FlowQuantity;
typedef int64 CostValue;

template<int NodeIndexSize, int ArcIndexSize> class EbertGraph {
 public:
  // The index of the 'nil' node in the graph.
  static const NodeIndex kNilNode;

  // The index of the 'nil' arc in the graph.
  static const ArcIndex kNilArc;

  // The index of the first node in the graph.
  static const NodeIndex kFirstNode;

  // The index of the first arc in the graph.
  static const ArcIndex kFirstArc;

  // The maximum possible node index in the graph.
  static const NodeIndex kMaxNumNodes;

  // The maximun possible arc index in the graph.
  static const ArcIndex kMaxNumArcs;

  // An easy access to NodeIndexSize. Useful when using EbertGraph through
  // StarGraph or other typedef'd types.
  static const int kNodeIndexSize = NodeIndexSize;

  // An easy access to ArcIndexSize. Useful when using EbertGraph through
  // StarGraph or other typedef'd types.
  static const int kArcIndexSize = ArcIndexSize;

  EbertGraph()
      : max_num_nodes_(0),
        max_num_arcs_(0),
        num_nodes_(0),
        num_arcs_(0),
        node_(),
        next_adjacent_arc_(),
        first_incident_arc_(),
        representation_clean_(true) {}

  EbertGraph(NodeIndex max_num_nodes, ArcIndex max_num_arcs)
      : max_num_nodes_(0),
        max_num_arcs_(0),
        num_nodes_(0),
        num_arcs_(0),
        node_(),
        next_adjacent_arc_(),
        first_incident_arc_(),
        representation_clean_(true) {
    if (!Reserve(max_num_nodes, max_num_arcs)) {
      LOG(DFATAL) << "Could not reserve memory for " << max_num_nodes
                  << " nodes and " << max_num_arcs << " arcs.";
    }
    first_incident_arc_.Assign(kNilArc);
    next_adjacent_arc_.Assign(kNilArc);
    node_.Assign(kNilNode);
  }

  ~EbertGraph() {}

  // Reserves memory needed for max_num_nodes nodes and max_num_arcs arcs.
  // Returns false if the parameters passed are not OK.
  // It can be used to enlarge the graph, but does not shrink memory
  // if called with smaller values.
  bool Reserve(NodeIndex new_max_num_nodes, ArcIndex new_max_num_arcs) {
    if (new_max_num_nodes < 1 || new_max_num_nodes > kMaxNumNodes) {
      return false;
    }
    if (new_max_num_arcs < 1 || new_max_num_arcs > kMaxNumArcs) {
      return false;
    }
    node_.Reserve(-new_max_num_arcs, new_max_num_arcs - 1);
    next_adjacent_arc_.Reserve(-new_max_num_arcs, new_max_num_arcs - 1);
    for (ArcIndex arc = -new_max_num_arcs; arc < -max_num_arcs_; ++arc) {
      node_.Set(arc, kNilNode);
      next_adjacent_arc_.Set(arc, kNilArc);
    }
    for (ArcIndex arc = max_num_arcs_; arc < new_max_num_arcs; ++arc) {
      node_.Set(arc, kNilNode);
      next_adjacent_arc_.Set(arc, kNilArc);
    }
    first_incident_arc_.Reserve(kFirstNode, new_max_num_nodes - 1);
    for (NodeIndex node = max_num_nodes_; node < new_max_num_nodes; ++node) {
      first_incident_arc_.Set(node, kNilArc);
    }
    max_num_nodes_ = new_max_num_nodes;
    max_num_arcs_ = new_max_num_arcs;
    return true;
  }

  // Returns the number of nodes in the graph.
  NodeIndex num_nodes() const { return num_nodes_; }

  // Returns the number of original arcs in the graph
  // (The ones with positive indices.)
  NodeIndex num_arcs() const { return num_arcs_; }

  // Returns one more than the largest index of an extant node. To be
  // used as a helper when clients need to dimension or iterate over
  // arrays of node annotation information.
  NodeIndex end_node_index() const { return kFirstNode + num_nodes_; }

  // Returns one more than the largest index of an extant direct
  // arc. To be used as a helper when clients need to dimension or
  // iterate over arrays of arc annotation information.
  ArcIndex end_arc_index() const { return kFirstArc + num_arcs_; }

  // Returns the maximum possible number of nodes in the graph.
  NodeIndex max_num_nodes() const { return max_num_nodes_; }

  // Returns the maximum possible number of original arcs in the graph.
  // (The ones with positive indices.)
  NodeIndex max_num_arcs() const { return max_num_arcs_; }

  // Returns one more than the largest valid index of a node. To be
  // used as a helper when clients need to dimension or iterate over
  // arrays of node annotation information.
  NodeIndex max_end_node_index() const { return kFirstNode + max_num_nodes_; }

  // Returns one more than the largest valid index of a direct arc. To
  // be used as a helper when clients need to dimension or iterate
  // over arrays of arc annotation information.
  ArcIndex max_end_arc_index() const { return kFirstArc + max_num_arcs_; }

  // Returns true if node is in the range [kFirstNode .. max_num_nodes_).
  bool IsNodeValid(NodeIndex node) const {
    return node >= kFirstNode && node < max_num_nodes_;
  }

  // Adds an arc to the graph and returns its index.
  // Returns kNilArc if the arc could not be added.
  ArcIndex AddArc(NodeIndex tail, NodeIndex head) {
    if (num_arcs_ >= max_num_arcs_
        || !IsNodeValid(tail) || !IsNodeValid(head)) {
      return kNilArc;
    }
    num_nodes_ = std::max(num_nodes_, tail + 1);
    num_nodes_ = std::max(num_nodes_, head + 1);
    ArcIndex arc = num_arcs_;
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
  template <typename ArcIndexStrictWeakOrderingFunctor>
  void GroupForwardArcsByFunctor(
      const ArcIndexStrictWeakOrderingFunctor& compare,
      PermutationCycleHandler<ArcIndex>* annotation_handler) {
    // The following cannot be a PackedArray<kArcIndexSize> instance
    // because using the STL sort() requires us to iterate over the
    // permutation with STL iterators. Indices from 0 through
    // kFirstArc - 1 are unused. Today that's only index 0.
    scoped_array<ArcIndex> arc_permutation(new ArcIndex[end_arc_index()]);

    // Determine the permutation that groups arcs by their tail nodes.
    for (ArcIndex i = 0; i < end_arc_index(); ++i) {
      // Start with the identity permutation.
      arc_permutation[i] = i;
    }
    std::sort(&arc_permutation[kFirstArc],
              &arc_permutation[end_arc_index()],
              compare);

    // Now we actually permute the node_ array and the
    // scaled_arc_cost_ array according to the sorting permutation.
    CycleHandlerForAnnotatedArcs cycle_handler(annotation_handler, this);
    PermutationApplier<ArcIndex> permutation(&cycle_handler);
    permutation.Apply(&arc_permutation[0], kFirstArc, end_arc_index());

    // Finally, rebuild the graph from its permuted node_ array.
    BuildRepresentation();
  }

  class CycleHandlerForAnnotatedArcs :
      public PermutationCycleHandler<ArcIndex> {
   public:
    CycleHandlerForAnnotatedArcs(
        PermutationCycleHandler<ArcIndex>* annotation_handler,
        EbertGraph* graph)
        : annotation_handler_(annotation_handler),
          graph_(graph),
          head_temp_(kNilNode),
          tail_temp_(kNilNode) { }

    virtual void SetTempFromIndex(ArcIndex source) {
      if (annotation_handler_ != NULL) {
        annotation_handler_->SetTempFromIndex(source);
      }
      head_temp_ = graph_->Head(source);
      tail_temp_ = graph_->Tail(source);
    }

    virtual void SetIndexFromIndex(ArcIndex source,
                                   ArcIndex destination) const {
      if (annotation_handler_ != NULL) {
        annotation_handler_->SetIndexFromIndex(source, destination);
      }
      graph_->SetHead(destination, graph_->Head(source));
      graph_->SetTail(destination, graph_->Tail(source));
    }

    virtual void SetIndexFromTemp(ArcIndex destination) const {
      if (annotation_handler_ != NULL) {
        annotation_handler_->SetIndexFromTemp(destination);
      }
      graph_->SetHead(destination, head_temp_);
      graph_->SetTail(destination, tail_temp_);
    }

    // Since arc grouping works only with forward arcs, we use the
    // forward/reverse bit of information encoded in the ArcIndex to
    // indicate whether this index has already been seen in processing
    // the permutation. The permutation starts out with all indices
    // referring to forward arcs. As each arc is moved according to
    // the permutation, its index is switched to its opposite to keep
    // track of which arcs have already been moved. In this way we
    // don't need any extra storage to keep track of this information,
    // and ArcIndex is guaranteed to be able to encode it since it has
    // to be able to encode forward/reverse.
    virtual void SetSeen(ArcIndex* permutation_element) const {
      *permutation_element = graph_->Opposite(*permutation_element);
    }

    virtual bool Unseen(ArcIndex permutation_element) const {
      return graph_->IsDirect(permutation_element);
    }

    virtual ~CycleHandlerForAnnotatedArcs() { }

   private:
    PermutationCycleHandler<ArcIndex>* annotation_handler_;
    EbertGraph* graph_;
    NodeIndex head_temp_;
    NodeIndex tail_temp_;
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
    NodeIndex Index() const { return node_; }

   private:
    // A reference to the current EbertGraph considered.
    const EbertGraph& graph_;

    // The index of the current node considered.
    NodeIndex         node_;
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
    ArcIndex Index() const { return arc_; }

   private:
    // A reference to the current EbertGraph considered.
    const EbertGraph& graph_;

    // The index of the current arc considered.
    ArcIndex          arc_;
  };

  // Iterator class for traversing the arcs incident to a given node in the
  // graph.
  class IncidentArcIterator {
   public:
    IncidentArcIterator(const EbertGraph& graph, NodeIndex node)
        : graph_(graph),
          node_(graph_.StartNode(node)),
          arc_(graph_.StartArc(graph_.FirstIncidentArc(node))) {
      DCHECK(CheckInvariant());
    }

    // This constructor takes an arc as extra argument and makes the iterator
    // start at arc.
    IncidentArcIterator(const EbertGraph& graph, NodeIndex node, ArcIndex arc)
        : graph_(graph),
          node_(graph_.StartNode(node)),
          arc_(graph_.StartArc(arc)) {
      DCHECK(CheckInvariant());
    }

    // Returns true unless all the adjancent arcs have been traversed.
    bool Ok() const { return arc_ != kNilArc; }

    // Advances the current adjacent arc index.
    void Next() {
      arc_ = graph_.NextAdjacentArc(arc_);
      DCHECK(CheckInvariant());
    }

    // Returns the index of the arc currently pointed to by the iterator.
    ArcIndex Index() const { return arc_; }

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
    NodeIndex         node_;

    // The index of the current arc considered.
    ArcIndex          arc_;
  };

  // Iterator class for traversing the incoming arcs associated to a given node.
  // Note that the indices of these arc are negative, i.e. it's actually
  // their corresponding direct arcs that are incoming to the node.
  // The API has been designed in this way to have the set of arcs iterated
  // by IncidentArcIterator to be the union of the sets of arcs iterated by
  // IncomingArcIterator and OutgoingArcIterator.
  class IncomingArcIterator {
   public:
    IncomingArcIterator(const EbertGraph& graph, NodeIndex node)
        : graph_(graph),
          node_(graph_.StartNode(node)),
          arc_(graph_.StartArc(graph_.FirstIncomingArc(node))) {
      DCHECK(CheckInvariant());
    }

    // This constructor takes an arc as extra argument and makes the iterator
    // start at arc.
    IncomingArcIterator(const EbertGraph& graph, NodeIndex node, ArcIndex arc)
        : graph_(graph),
          node_(graph_.StartNode(node)),
          arc_(graph_.StartArc(arc)) {
      DCHECK(CheckInvariant());
    }

    // Returns true unless all the incoming arcs have been traversed.
    bool Ok() const { return arc_ != kNilArc; }

    // Advances the current incoming arc index.
    void Next() {
      arc_ = graph_.NextIncomingArc(arc_);
      DCHECK(CheckInvariant());
    }

    // Returns the index of the arc currently pointed to by the iterator.
    ArcIndex Index() const { return arc_; }

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
    NodeIndex         node_;

    // The index of the current arc considered.
    ArcIndex          arc_;
  };

  // Iterator class for traversing the outgoing arcs associated to a given node.
  class OutgoingArcIterator {
   public:
    OutgoingArcIterator(const EbertGraph& graph, NodeIndex node)
        : graph_(graph),
          node_(graph_.StartNode(node)),
          arc_(graph_.StartArc(graph_.FirstOutgoingArc(node))) {
      DCHECK(CheckInvariant());
    }

    // This constructor takes an arc as extra argument and makes the iterator
    // start at arc.
    OutgoingArcIterator(const EbertGraph& graph, NodeIndex node, ArcIndex arc)
        : graph_(graph),
          node_(graph_.StartNode(node)),
          arc_(graph_.StartArc(arc)) {
      DCHECK(CheckInvariant());
    }

    // Returns true unless all the outgoing arcs have been traversed.
    bool Ok() const { return arc_ != kNilArc; }

    // Advances the current outgoing arc index.
    void Next() {
      arc_ = graph_.NextOutgoingArc(arc_);
      DCHECK(CheckInvariant());
    }

    // Returns the index of the arc currently pointed to by the iterator.
    ArcIndex Index() const { return arc_; }

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
    NodeIndex         node_;

    // The index of the current arc considered.
    ArcIndex          arc_;
  };

  // Utility function to check that an arc index is within the bounds.
  // It is exported so that users of the EbertGraph class can use it.
  // To be used in a DCHECK.
  bool CheckArcBounds(const ArcIndex arc) const {
    return (arc == kNilArc) || (arc >= -max_num_arcs_ && arc < max_num_arcs_);
  }

  // Utility function to check that an arc index is within the bounds AND
  // different from kNilArc.
  // It is exported so that users of the EbertGraph class can use it.
  // To be used in a DCHECK.
  bool CheckArcValidity(const ArcIndex arc) const {
    return (arc != kNilArc) && (arc >= -max_num_arcs_ && arc < max_num_arcs_);
  }

  // Utility function to check that a node index is within the bounds AND
  // different from kNilNode.
  // It is exported so that users of the EbertGraph class can use it.
  // To be used in a DCHECK.
  bool CheckNodeValidity(const NodeIndex node) const {
    return node >= kFirstNode && node < max_num_nodes_;
  }

  // Returns the tail or start-node of arc.
  NodeIndex Tail(const ArcIndex arc) const {
    DCHECK(CheckArcValidity(arc));
    return node_[Opposite(arc)];
  }

  // Returns the head or end-node of arc.
  NodeIndex Head(const ArcIndex arc) const {
    DCHECK(CheckArcValidity(arc));
    return node_[arc];
  }

  // Returns the first arc going from tail to head, if it exists, or kNilArc
  // if such an arc does not exist.
  ArcIndex LookUpArc(const NodeIndex tail, const NodeIndex head) const {
    for (ArcIndex arc = FirstOutgoingArc(tail);
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
  NodeIndex DirectArcTail(const ArcIndex arc) const {
    return Tail(DirectArc(arc));
  }

  // Returns the head or end-node of arc if it is positive
  // (i.e. it is taken in the direction it was entered in the graph),
  // and the tail or start-node otherwise. 'That' in Ebert's paper.
  NodeIndex DirectArcHead(const ArcIndex arc) const {
    return Head(DirectArc(arc));
  }

  // Returns the arc in normal/direct direction.
  ArcIndex DirectArc(const ArcIndex arc) const {
    DCHECK(CheckArcValidity(arc));
    return std::max(arc, Opposite(arc));
  }

  // Returns the arc in reverse direction.
  ArcIndex ReverseArc(const ArcIndex arc) const {
    DCHECK(CheckArcValidity(arc));
    return std::min(arc, Opposite(arc));
  }

  // Returns the opposite arc, i.e the direct arc is the arc is in reverse
  // direction, and the reverse arc if the arc is direct.
  ArcIndex Opposite(const ArcIndex arc) const {
    const ArcIndex opposite = ~arc;
    DCHECK(CheckArcValidity(arc));
    DCHECK(CheckArcValidity(opposite));
    return opposite;
  }

  // Returns true if the arc is direct.
  bool IsDirect(const ArcIndex arc) const {
    DCHECK(CheckArcBounds(arc));
    return arc != kNilArc && arc >= 0;
  }

  // Returns true if the arc is in the reverse direction.
  bool IsReverse(const ArcIndex arc) const {
    DCHECK(CheckArcBounds(arc));
    return arc != kNilArc && arc < 0;
  }

  // Returns true if arc is incident to node.
  bool IsIncident(ArcIndex arc, NodeIndex node) const {
    return IsIncoming(arc, node) || IsOutgoing(arc, node);
  }

  // Returns true if arc is incoming to node.
  bool IsIncoming(ArcIndex arc, NodeIndex node) const {
    return DirectArcHead(arc) == node;
  }

  // Returns true if arc is outgoing from node.
  bool IsOutgoing(ArcIndex arc, NodeIndex node) const {
    return DirectArcTail(arc) == node;
  }

  // Recreates the next_adjacent_arc_ and first_incident_arc_ variables from
  // the array node_ in O(n + m) time.
  // This is useful if node_ array has been sorted according to a given
  // criterion, for example.
  void BuildRepresentation() {
    first_incident_arc_.Assign(kNilArc);
    for (ArcIndex arc = kFirstArc; arc <  max_num_arcs_; ++arc) {
      Attach(arc);
    }
    representation_clean_ = true;
  }

  // Returns a debug string containing all the information contained in the
  // data structure in raw form.
  string DebugString() const {
    DCHECK(representation_clean_);
    string result = "Arcs:(node, next arc) :\n";
    for (ArcIndex arc = -num_arcs_; arc < num_arcs_; ++arc) {
      result += " " + ArcDebugString(arc) + ":(" + NodeDebugString(node_[arc])
              + "," + ArcDebugString(next_adjacent_arc_[arc]) + ")\n";
    }
    result += "Node:First arc :\n";
    for (NodeIndex node = kFirstNode; node < num_nodes_; ++node) {
      result += " " + NodeDebugString(node) + ":"
              + ArcDebugString(first_incident_arc_[node]) + "\n";
    }
    return result;
  }

  string NodeDebugString(const NodeIndex node) const {
    if (node == kNilNode) {
      return "NilNode";
    } else {
      return StringPrintf("%lld", static_cast<int64>(node));
    }
  }

  string ArcDebugString(const ArcIndex arc) const {
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
  NodeIndex StartNode(NodeIndex node) const {
    return num_nodes_ == 0 ? kNilNode : node;
  }

  // Returns kNilArc if the graph has no arcs arc if it has at least one arc.
  // Useful for initializing iterators correctly in the case of empty graphs.
  ArcIndex StartArc(ArcIndex arc) const {
    return num_arcs_ == 0 ? kNilArc : arc;
  }

  // Returns the first outgoing arc for node.
  ArcIndex FirstOutgoingArc(const NodeIndex node) const {
    DCHECK(CheckNodeValidity(node));
    return FindNextOutgoingArc(FirstIncidentArc(node));
  }

  // Returns the outgoing arc following the argument in the adjacency list.
  ArcIndex NextOutgoingArc(const ArcIndex arc) const {
    DCHECK(CheckArcValidity(arc));
    return FindNextOutgoingArc(NextAdjacentArc(DirectArc(arc)));
  }

  // Returns the first incoming arc for node.
  ArcIndex FirstIncomingArc(const NodeIndex node) const {
    DCHECK_LE(kFirstNode, node);
    DCHECK_GE(max_num_nodes_, node);
    return FindNextIncomingArc(FirstIncidentArc(node));
  }

  // Returns the incoming arc following the argument in the adjacency list.
  ArcIndex NextIncomingArc(const ArcIndex arc) const {
    DCHECK(CheckArcValidity(arc));
    return FindNextIncomingArc(NextAdjacentArc(ReverseArc(arc)));
  }

  // Returns the first arc in node's incidence list.
  ArcIndex FirstIncidentArc(const NodeIndex node) const {
    DCHECK(representation_clean_);
    DCHECK(CheckNodeValidity(node));
    return first_incident_arc_[node];
  }

  // Returns the next arc following the passed argument in its adjacency list.
  ArcIndex NextAdjacentArc(const ArcIndex arc) const {
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
  NodeIndex NextNode(const NodeIndex node) const {
    DCHECK(CheckNodeValidity(node));
    const NodeIndex next_node = node + 1;
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
  ArcIndex NextArc(const ArcIndex arc) const {
    DCHECK(CheckArcValidity(arc));
    const ArcIndex next_arc = arc + 1;
    return next_arc < num_arcs_ ? next_arc : kNilArc;
  }

  // Using the SetTail() method implies that the BuildRepresentation()
  // method must be called to restore consistency before the graph is
  // used.
  void SetTail(const ArcIndex arc, const NodeIndex tail) {
    representation_clean_ = false;
    node_.Set(Opposite(arc), tail);
  }

  // Using the SetHead() method implies that the BuildRepresentation()
  // method must be called to restore consistency before the graph is
  // used.
  void SetHead(const ArcIndex arc, const NodeIndex head) {
    representation_clean_ = false;
    node_.Set(arc, head);
  }

  // Utility method to attach a new arc.
  void Attach(ArcIndex arc) {
    DCHECK(CheckArcValidity(arc));
    const NodeIndex tail = node_[Opposite(arc)];
    DCHECK(CheckNodeValidity(tail));
    next_adjacent_arc_.Set(arc, first_incident_arc_[tail]);
    first_incident_arc_.Set(tail, arc);
    const NodeIndex head = node_[arc];
    DCHECK(CheckNodeValidity(head));
    next_adjacent_arc_.Set(Opposite(arc), first_incident_arc_[head]);
    first_incident_arc_.Set(head, Opposite(arc));
  }

  // Utility method that finds the next outgoing arc.
  ArcIndex FindNextOutgoingArc(ArcIndex arc) const {
    DCHECK(CheckArcBounds(arc));
    while (IsReverse(arc)) {
      arc = NextAdjacentArc(arc);
      DCHECK(CheckArcBounds(arc));
    }
    return arc;
  }

  // Utility method that finds the next incoming arc.
  ArcIndex FindNextIncomingArc(ArcIndex arc) const {
    DCHECK(CheckArcBounds(arc));
    while (IsDirect(arc)) {
      arc = NextAdjacentArc(arc);
      DCHECK(CheckArcBounds(arc));
    }
    return arc;
  }

  // The maximum number of nodes that the graph can hold.
  NodeIndex max_num_nodes_;

  // The maximum number of arcs that the graph can hold.
  ArcIndex max_num_arcs_;

  // The maximum index of the node currently held by the graph.
  NodeIndex num_nodes_;

  // The current number of arcs held by the graph.
  ArcIndex num_arcs_;

  // Array of node indices. node_[i] contains the tail node of arc i.
  PackedArray<NodeIndexSize> node_;

  // Array of next indices.
  // next_adjacent_arc_[i] contains the next arc in the adjacency list of arc i.
  PackedArray<ArcIndexSize> next_adjacent_arc_;

  // Array of arc indices. first_incident_arc_[i] contains the first arc
  // incident to node i.
  PackedArray<ArcIndexSize> first_incident_arc_;

  // Flag to indicate that BuildRepresentation() needs to be called
  // before the adjacency lists are examined. Only for DCHECK in debug
  // builds.
  bool representation_clean_;
};

#define MAX_LONGLONG_ON_BYTES(n) ((GG_ULONGLONG(1) << (n * CHAR_BIT - 1)) - 1)

// The index of the 'nil' node in the graph.
template<int NodeIndexSize, int ArcIndexSize>
const NodeIndex EbertGraph<NodeIndexSize, ArcIndexSize>::kNilNode =
    GG_LONGLONG(-1);

// The index of the 'nil' arc in the graph.
template<int NodeIndexSize, int ArcIndexSize>
const ArcIndex EbertGraph<NodeIndexSize, ArcIndexSize>::kNilArc =
    ~MAX_LONGLONG_ON_BYTES(ArcIndexSize);

// The index of the first node in the graph.
template<int NodeIndexSize, int ArcIndexSize>
const NodeIndex EbertGraph<NodeIndexSize, ArcIndexSize>::kFirstNode = 0;

// The index of the first arc in the graph.
template<int NodeIndexSize, int ArcIndexSize>
const ArcIndex EbertGraph<NodeIndexSize, ArcIndexSize>::kFirstArc = 0;

// The maximum possible node index in the graph.
template<int NodeIndexSize, int ArcIndexSize>
const NodeIndex EbertGraph<NodeIndexSize, ArcIndexSize>::kMaxNumNodes =
    MAX_LONGLONG_ON_BYTES(NodeIndexSize);

// The maximum possible number of arcs in the graph.
// (The maximum index is kMaxNumArcs-1, since indices start at 0.)
template<int NodeIndexSize, int ArcIndexSize>
const ArcIndex EbertGraph<NodeIndexSize, ArcIndexSize>::kMaxNumArcs =
    MAX_LONGLONG_ON_BYTES(ArcIndexSize);

#undef MAX_LONGLONG_ON_BYTES

// Standard definition of the star representation of a graph, that makes it
// possible to address all the physical memory on a 2010 machine, while keeping
// the sizes of arcs and nodes reasonable. For most purposes it is sufficient
// to use this class.
// kStarGraphNodeIndexSize and kStarGraphArcIndexSize are necessary
// for SWIG but are otherwise deprecated. Use
// StarGraph::kNodeIndexSize and StarGraph::kArcIndexSize instead
// where possible.
const int kStarGraphNodeIndexSize = 5;
const int kStarGraphArcIndexSize = 5;
typedef EbertGraph<kStarGraphNodeIndexSize, kStarGraphArcIndexSize> StarGraph;
typedef PackedArray<StarGraph::kNodeIndexSize> NodeIndexArray;
typedef PackedArray<StarGraph::kArcIndexSize> ArcIndexArray;
}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_EBERT_GRAPH_H_
