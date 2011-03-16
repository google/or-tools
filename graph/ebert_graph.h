// Copyright 2010 Google
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

#ifndef GRAPH_EBERT_GRAPH_H_
#define GRAPH_EBERT_GRAPH_H_

// An implementation (with some improvements) of the star-representation of a
// graph as described in J. Ebert, "A versatile data structure for
// arc-oriented graph algorithms." Communications of the ACM 30(6):513-519
// (June 1987).
// Both forward- and backward-star representations are contained in this
// representation.

// The graph is represented with three arrays.
// Let n be the number of nodes and m be the number of arcs.
// Let i be an integer in [1..m], denoting the index of an arc.
//  * node_[i] contains the end-node of arc i,
//  * node_[-i] contains the start-node of arc i.
// Consequently:
//  * node_[-i] contains the start-node of the arc reverse to arc i,
//  * node_[i] contains the end-node of the arc reverse to arc i.
//  Note that if arc (u,v) is defined, then the data structure also stores
//  (v, u).
//  Arc -i thus denotes the arc reverse to arc i.
//  This is what makes this representation useful for undirected graphs,
//  and for implementing algorithms like two-directions shortest-path.
//
// Now, for an integer u in [1..n] denoting the index of a node:
//  * first_incident_arc_[u] denotes the first arc in the adjacency list of u.
//  * going from an arc i, the adjacency list can be traversed using
//    j = next_adjacent_arc_[i].
//
// Note that arc index 0 is not used (because negative indices denote reverse
// arcs.) Therefore care must be taken about the fact that arc indices (and
// for consistency, node indices) start at 1.
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
//  * The memory consumption is: (2 * m + 1) * NodeIndexSize
//                             + (2 * m + 1) * ArcIndexSize
//                             + (n + 1) * ArcIndexSize
//
// The main drawback of this implementation is that the node and arc indices
// start at 1 instead of the usual 0 in C/C++.
//
// This implementation differs from the implementation described in [Ebert 1987]
// in the following respects:
//  * it is possible to add arcs to the graph, with AddArc, in a much simpler
//    way than described by Ebert.
//  * TODO(user) it is possible to group all the outgoing (resp. incoming) arcs
//    of a node to allow to traverse the outgoing (resp. incoming) arcs in
//    O(out_degree(node)) (resp. O(in_degree(node))) instead of O(degree(node)).
//  * TODO(user) it is possible to implement arc deletion and garbage collection
//    in an efficient (relatively) manner. For the time being we haven't seen an
//    application to this.
//  * TODO(user) implement "interleaved" version of this, with direct arcs having
//    even indices (2*i), and indirect arcs having odd indices (2*i+1). As
//    suggested by lhm this could have better cache properties. This has to be
//    validated on algorithms running with real data.

#include <algorithm>
#include <limits>
#include <string>
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "util/packed_array.h"

namespace operations_research {
typedef int64 NodeIndex;
typedef int64 ArcIndex;
typedef int64 FlowQuantity;
typedef int64 CostValue;

template<int NodeIndexSize, int ArcIndexSize> class EbertGraph {
  friend class NodeIterator;
  friend class ArcIterator;
  friend class IncidentArcIterator;
  friend class OutgoingArcIterator;
  friend class IncomingArcIterator;
 public:
  // The index of the 'nil' node in the graph.
  static const NodeIndex kNilNode;

  // The index of the 'nil' arc in the graph.
  static const ArcIndex kNilArc;

  EbertGraph(NodeIndex max_num_nodes, ArcIndex max_num_arcs)
      : max_num_nodes_(0),
        max_num_arcs_(0),
        num_nodes_(0),
        num_arcs_(0),
        node_(),
        next_adjacent_arc_(),
        first_incident_arc_() {
    Reserve(max_num_nodes, max_num_arcs);
    next_adjacent_arc_.Set(0, 0);
    node_.Set(0, 0);
  }
  ~EbertGraph() {}

  // Reserve memory neeeded for max_num_nodes nodes and max_num_arcs arcs.
  // It can be used to enlarge the graph, but does not shrink memory
  // if called with smaller values.
  void Reserve(NodeIndex new_max_num_nodes, ArcIndex new_max_num_arcs) {
    CHECK_LE(1, new_max_num_nodes);
    CHECK_LE(1, new_max_num_arcs);
    node_.Reserve(-new_max_num_arcs, new_max_num_arcs);
    next_adjacent_arc_.Reserve(-new_max_num_arcs, new_max_num_arcs);
    first_incident_arc_.Reserve(kFirstNode, new_max_num_nodes);
    for (NodeIndex node = max_num_nodes_ + 1;
         node <= new_max_num_nodes;
         ++node) {
      first_incident_arc_.Set(node, 0);
    }
    max_num_nodes_ = new_max_num_nodes;
    max_num_arcs_ = new_max_num_arcs;
  }

  // Returns the number of nodes in the graph.
  NodeIndex num_nodes() const { return num_nodes_; }

  // Returns the number of original arcs in the graph
  // (The ones with positive indices.)
  NodeIndex num_arcs() const { return num_arcs_; }

  // Returns the maximum possible number of nodes in the graph.
  NodeIndex max_num_nodes() const { return max_num_nodes_; }

  // Returns the maximum possible number of original arcs in the graph.
  // (The ones with positive indices.)
  NodeIndex max_num_arcs() const { return max_num_arcs_; }

  // Adds an arc to the graph and returns its index.
  ArcIndex AddArc(NodeIndex tail, NodeIndex head) {
    CHECK_LE(kFirstNode, tail);
    CHECK_GE(max_num_nodes_, tail);
    CHECK_LE(kFirstNode, head);
    CHECK_GE(max_num_nodes_, head);
    CHECK_GT(max_num_arcs_, num_arcs_);
    num_nodes_ = std::max(num_nodes_, tail);
    num_nodes_ = std::max(num_nodes_, head);
    ++num_arcs_;
    ArcIndex arc = num_arcs_;
    node_.Set(Opposite(arc), tail);
    node_.Set(arc, head);
    Attach(arc);
    return arc;
  }

  // Iterator class for traversing the nodes in the graph.
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
      CHECK(graph_.IsIncident(arc_, node_));
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
      CHECK(graph_.IsIncoming(arc_, node_));
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
      CHECK(graph_.IsOutgoing(arc_, node_));
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
    DCHECK_LE(-max_num_arcs_, arc);
    DCHECK_GE(max_num_arcs_, arc);
    return true;
  }

  // Utility function to check that an arc index is within the bounds AND
  // different from kNilArc.
  // It is exported so that users of the EbertGraph class can use it.
  // To be used in a DCHECK.
  bool CheckArcValidity(const ArcIndex arc) const {
    DCHECK_NE(kNilArc, arc);
    DCHECK_LE(-max_num_arcs_, arc);
    DCHECK_GE(max_num_arcs_, arc);
    return true;
  }

  // Utility function to check that a node index is within the bounds AND
  // different from kNilNode.
  // It is exported so that users of the EbertGraph class can use it.
  // To be used in a DCHECK.
  bool CheckNodeValidity(const NodeIndex node) const {
    DCHECK_LE(kFirstNode, node);
    DCHECK_GE(max_num_nodes_, node);
    return true;
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
    return max(arc, Opposite(arc));  // abs(arc)
  }

  // Returns the arc in reverse direction.
  ArcIndex ReverseArc(const ArcIndex arc) const {
    DCHECK(CheckArcValidity(arc));
    return min(arc, Opposite(arc));  // -abs(arc)
  }

  // Returns the opposite arc, i.e the direct arc is the arc is in reverse
  // direction, and the reverse arc if the arc is direct.
  ArcIndex Opposite(const ArcIndex arc) const {
    DCHECK(CheckArcValidity(arc));
    return -arc;
  }

  // Returns true if the arc is direct.
  bool IsDirect(const ArcIndex arc) const {
    DCHECK(CheckArcValidity(arc));
    return arc > 0;
  }

  // Returns true if the arc is in the reverse direction.
  bool IsReverse(const ArcIndex arc) const {
    DCHECK(CheckArcValidity(arc));
    return arc < 0;
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
  // the array node_  in O(n + m) time.
  // This is useful if node_ array has been sorted according to a given
  // criterion, for example.
  void BuildRepresentation() {
    first_incident_arc_.Assign(0);
    for (ArcIndex arc = kFirstArc; arc <= max_num_arcs_; ++arc) {
      Attach(arc);
    }
  }

  // Returns a debug string containing all the information contained in the
  // data structure in raw form.
  string DebugString() const {
    string result = "Arcs:(node, next arc) :\n";
    for (int64 arc = -num_arcs_; arc <= num_arcs_; ++arc) {
      result += StringPrintf(" %lld:(%lld,%lld)\n",
                             arc,
                             static_cast<int64>(node_[arc]),
                             static_cast<int64>(next_adjacent_arc_[arc]));
    }
    result += "Node:First arc :\n";
    for (int64 node = kFirstNode; node <= num_nodes_; ++node) {
      result += StringPrintf(" %lld:%lld\n", node,
                             static_cast<int64>(first_incident_arc_[node]));
    }
    return result;
  }

 private:
  // The index of the first node in the graph.
  static const NodeIndex kFirstNode;

  // The index of the first arc in the graph.
  static const ArcIndex kFirstArc;

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
    return FindNextOutcomingArc(FirstIncidentArc(node));
  }

  // Returns the outgoing arc following the argument in the adjacency list.
  ArcIndex NextOutgoingArc(const ArcIndex arc) const {
    DCHECK(CheckArcValidity(arc));
    return FindNextOutcomingArc(NextAdjacentArc(DirectArc(arc)));
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
    DCHECK(CheckNodeValidity(node));
    return first_incident_arc_[node];
  }

  // Returns the next arc following the passed argument in its adjacency list.
  ArcIndex NextAdjacentArc(const ArcIndex arc) const {
    DCHECK(CheckArcValidity(arc));
    return next_adjacent_arc_[arc];
  }

  // Returns the node following the argument in the graph.
  // Returns kNilNode (= end) if the range of nodes has been exhausted.
  // It is called by NodeIterator::Next() and as such does not expect do be
  // passed an argument equal to kNilNode.
  // This is why the return line is simplified from
  // return ( node == kNilNode || node >= num_nodes_) ? kNilNode : node + 1;
  // to
  // return node >= num_nodes_ ? kNilNode : node + 1;
  NodeIndex NextNode(const NodeIndex node) const {
    DCHECK(CheckNodeValidity(node));
    return node >= num_nodes_ ? kNilNode : node + 1;
  }

  // Returns the arc following the argument in the graph.
  // Returns kNilArc (= end) if the range of arcs has been exhausted.
  // It is called by ArcIterator::Next() and as such does not expect do be
  // passed an argument equal to kNilArc.
  // This is why the return line is simplified from
  // return ( arc == kNilArc || arc >= num_arcs_) ? kNilArc : arc + 1;
  // to
  // return arc >= num_arcs_ ? kNilArc : arc + 1;
  ArcIndex NextArc(const ArcIndex arc) const {
    DCHECK(CheckArcValidity(arc));
    return arc >= num_arcs_ ? kNilArc : arc + 1;
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
  ArcIndex FindNextOutcomingArc(ArcIndex arc) const {
    DCHECK(CheckArcBounds(arc));
    while (arc < 0) {
      arc = NextAdjacentArc(arc);
      DCHECK(CheckArcBounds(arc));
    }
    return arc;
  }

  // Utility method that finds the next incoming arc.
  ArcIndex FindNextIncomingArc(ArcIndex arc) const {
    DCHECK(CheckArcBounds(arc));
    while (arc > 0) {
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
};

template<int NodeIndexSize, int ArcIndexSize>
const NodeIndex EbertGraph<NodeIndexSize, ArcIndexSize>::kNilNode = 0;
template<int NodeIndexSize, int ArcIndexSize>
const ArcIndex EbertGraph<NodeIndexSize, ArcIndexSize>::kNilArc = 0;
template<int NodeIndexSize, int ArcIndexSize>
const NodeIndex EbertGraph<NodeIndexSize, ArcIndexSize>::kFirstNode = 1;
template<int NodeIndexSize, int ArcIndexSize>
const ArcIndex EbertGraph<NodeIndexSize, ArcIndexSize>::kFirstArc = 1;

// Standard definition of the star representation of a graph, that makes it
// possible to address all the physical memory on a 2010 machine, while keeping
// the sizes of arcs and nodes reasonable. For most purposes it is sufficient
// to use this class.
const int kStarGraphNodeIndexSize = 5;
const int kStarGraphArcIndexSize = 5;
typedef EbertGraph<kStarGraphNodeIndexSize, kStarGraphArcIndexSize> StarGraph;
typedef PackedArray<kStarGraphNodeIndexSize> NodeIndexArray;
typedef PackedArray<kStarGraphArcIndexSize> ArcIndexArray;
}  // namespace operations_research

#endif  // GRAPH_EBERT_GRAPH_H_
