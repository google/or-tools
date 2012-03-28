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

// An implementation of a push-relabel algorithm for the max flow problem.
//
// In the following, we consider a graph G = (V,E,s,t) where V denotes the set
// of nodes (vertices) in the graph, E denotes the set of arcs (edges). s and t
// denote distinguished nodes in G called source and target. n = |V| denotes the
// number of nodes in the graph, and m = |E| denotes the number of arcs in the
// graph.
//
// Each arc (v,w) is associated a capacity c(v,w).
// A flow is a function from E to R such that:
// a) f(v,w) <= c(v,w) for all (v,w) in E (capacity constraint.)
// b) f(v,w) = -f(w,v) for all (v,w) in E (flow antisymmetry constraint.)
// c) sum on v f(v,w) = 0  (flow conservation.)
//
// The goal of this algorithm is to find the maximum flow from s to t, i.e.
// for example to maximize sum v f(s,v).
//
// The starting reference for this class of algorithms is:
// A.V. Goldberg and R.E. Tarjan. A new approach to the maximum flow problem.
// ACM Symposium on Theory of Computing, pp. 136-146.
// http://portal.acm.org/citation.cfm?id=12144
//
// The basic idea of the algorithm is to handle preflows instead of flows,
// and to refine preflows until a maximum flow is obtained.
// A preflow is like a flow, except that the inflow can be larger than the
// outflow. If it is the case at a given node v, it is said that there is an
// excess at node v, and inflow = outflow + excess.
// More formally, a preflow is a function f such that:
// 1) f(v,w) <= c(v,w) for all (v,w) in E  (capacity constraint). c(v,w)  is a
//    value representing the maximum capacity for arc (v,w).
// 2) f(v,w) = -f(w,v) for all (v,w) in E (flow antisymmetry constraint)
// 3) excess(v) = sum on u f(u,v) >= 0 is the excess at node v, the
//    algebraic sum of all the incoming preflows at this node.
//
// Each node has an associated "height", in addition to its excess. The
// height of the source is defined to be equal to n, and cannot change. The
// height of the target is defined to be zero, and cannot change either. The
// height of all the other nodes is initialized at zero and is updated during
// the algorithm (see below). For those who want to know the details, the height
// of a node, corresponds to a reduced cost, and this enables one to prove that
// the algorithm actually computes the max flow. Note that the height of a node
// can be initialized to the distance to the target node in terms of number of
// nodes. This has not been tried in this implementation.
//
// A node v is said to be *active* if excess(v) > 0.
// In this case the following operations can be applied to it:
// - if there are *admissible* incident arcs, i.e. arcs which are not saturated,
//   and whose tail's height is lower than the height of the active node
//   considered, a PushFlow operation can be applied. It consists in sending as
//   much flow as both the excess at the node and the capacity of the arc
//   permit.
// - if there are no admissible arcs, the active node considered is relabeled,
//   i.e. its height is increased to 1 + the minimum height of its neighboring
//   nodes on admissible arcs.
// This is implemented in Optimize, which itself calls PushFlow and Relabel.
//
// Before running Optimize, it is necessary to initialize the algorithm with a
// preflow. This is done in InitializePreflow, which saturates all the arcs
// leaving the source node, and sets the excess at the heads of those arcs
// accordingly.
//
// The algorithm terminates when there are no remaining active nodes, i.e. all
// the excesses at all nodes are equal to zero. In this case, a maximum flow is
// obtained.
//
// TODO(user): implement the following active node choice rule.
//
// The complexity of this algorithm depends amongst other things on the choice
// of the next active node. It has been shown, for example in:
// L. Tuncel, "On the Complexity of Preflow-Push Algorithms for Maximum-Flow
// Problems", Algorithmica 11(4): 353-359 (1994).
// and
// J. Cheriyan and K. Mehlhorn, "An analysis of the highest-level selection rule
// in the preflow-push max-flow algorithm", Information processing letters,
// 69(5):239-242 (1999).
// http://www.math.uwaterloo.ca/~jcheriya/PS_files/me3.0.ps
//
// that choosing the active node with the highest level yields a complexity of
// O(n^2 * sqrt(m)).
//
// This has been validated experimentally in:
// R.K. Ahuja, M. Kodialam, A.K. Mishra, and J.B. Orlin, "Computational
// Investigations of Maximum Flow Algorithms", EJOR 97:509-542(1997).
// http://jorlin.scripts.mit.edu/docs/publications/
//     58-comput%20investigations%20of.pdf
//
// TODO(user): an alternative would be to evaluate:
// A.V. Goldberg, "The Partial Augment-Relabel Algorithm for the Maximum Flow
// Problem.” In Proceedings of Algorithms ESA, LNCS 5193:466-477, Springer 2008.
// www.springerlink.com/index/5535k2j1mt646338.pdf
//
// An interesting general reference on network flows is:
// R. K. Ahuja, T. L. Magnanti, J. B. Orlin, "Network Flows: Theory, Algorithms,
// and Applications," Prentice Hall, 1993, ISBN: 978-0136175490,
// http://www.amazon.com/dp/013617549X
//
// Keywords: Push-relabel, max-flow, network, graph, Goldberg, Tarjan, Dinic,
//           Dinitz.

#ifndef OR_TOOLS_GRAPH_MAX_FLOW_H_
#define OR_TOOLS_GRAPH_MAX_FLOW_H_

#include <algorithm>
#include <stack>
#include <string>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "graph/ebert_graph.h"

using std::string;

namespace operations_research {

class MaxFlow {
 public:
  // Different statuses for a given problem.
  typedef enum {
    NOT_SOLVED,  // The problem was not solved, or its data were edited.
    OPTIMAL,     // Solve() was called and found an optimal solution.
    FEASIBLE,    // There is a feasible flow.
    INFEASIBLE,  // There is no feasible flow.
    BAD_INPUT,   // The input is inconsistent.
    BAD_RESULT   // There was an error.
  } Status;

  MaxFlow(const StarGraph* graph, NodeIndex source, NodeIndex target);

  virtual ~MaxFlow() {}

  // Returns the graph associated to the current object.
  const StarGraph* graph() const { return graph_; }

  // Returns the status of last call to Solve(). NOT_SOLVED is returned Solve()
  // has never been called or if the problem has been modified in such a way
  // that the previous solution becomes invalid.
  Status status() const { return status_; }

  // Returns the index of the node corresponding to the source of the network.
  NodeIndex GetSourceNodeIndex() const { return source_; }

  // Returns the index of the node corresponding to the sink of the network.
  NodeIndex GetSinkNodeIndex() const { return sink_; }

  // Sets the capacity for arc to new_capacity.
  void SetArcCapacity(ArcIndex arc, FlowQuantity new_capacity);

  // Sets the flow for arc.
  void SetArcFlow(ArcIndex arc, FlowQuantity new_flow) {
    DCHECK(graph_->CheckArcValidity(arc));
    const FlowQuantity capacity = Capacity(arc);
    DCHECK_GE(capacity, new_flow);
    residual_arc_capacity_.Set(Opposite(arc), -new_flow);
    residual_arc_capacity_.Set(arc, capacity - new_flow);
    status_ = NOT_SOLVED;
  }

  // Returns true if a maximum flow was solved.
  bool Solve();

  // Returns the total flow found by the algorithm.
  FlowQuantity GetOptimalFlow() const { return total_flow_; }

  // Returns the flow on arc using the equations given in the comment on
  // residual_arc_capacity_.
  FlowQuantity Flow(ArcIndex arc) const {
    DCHECK(graph_->CheckArcValidity(arc));
    if (IsDirect(arc)) {
      return residual_arc_capacity_[Opposite(arc)];
    } else {
      return -residual_arc_capacity_[arc];
    }
  }

  // Returns the capacity of arc using the equations given in the comment on
  // residual_arc_capacity_.
  FlowQuantity Capacity(ArcIndex arc) const {
    DCHECK(graph_->CheckArcValidity(arc));
    if (IsDirect(arc)) {
      return residual_arc_capacity_[arc]
           + residual_arc_capacity_[Opposite(arc)];
    } else {
      return 0;
    }
  }

 protected:
  // Returns true if arc is admissible.
  bool IsAdmissible(ArcIndex arc) const {
    return residual_arc_capacity_[arc] > 0
        && node_potential_[Tail(arc)] == node_potential_[Head(arc)] + 1;
  }

  // Returns true if node is active, i.e. if its supply is positive and it
  // is neither the source or the target of the graph.
  bool IsActive(NodeIndex node) const {
    return (node != source_) && (node != sink_) && (node_excess_[node] > 0);
  }

  // Returns the first incident arc of node.
  ArcIndex GetFirstIncidentArc(NodeIndex node) const {
    StarGraph::IncidentArcIterator arc_it(*graph_, node);
    return arc_it.Index();
  }

  // Reduces the residual capacity on arc by flow.
  // Increase the residual capacity on opposite arc by flow.
  // Does not update the excesses at the tail and head of arc.
  // No check is performed either.
  void PushFlowUnsafe(ArcIndex arc, FlowQuantity flow) {
    residual_arc_capacity_.Set(arc, residual_arc_capacity_[arc] - flow);
    const ArcIndex opposite = Opposite(arc);
    residual_arc_capacity_.Set(opposite,
                               residual_arc_capacity_[opposite] + flow);
  }

  // Sets the capacity of arc to 'capacity' and clears the flow on arc.
  void SetCapacityResetFlow(ArcIndex arc, FlowQuantity capacity) {
    residual_arc_capacity_.Set(arc, capacity);
    residual_arc_capacity_.Set(Opposite(arc), 0);
  }

  // Sets the capacity of arc to 'capacity' and saturates the flow on arc.
  void SetCapacitySaturate(ArcIndex arc, FlowQuantity capacity) {
    residual_arc_capacity_.Set(arc, 0);
    residual_arc_capacity_.Set(Opposite(arc), capacity);
  }

  // Checks the consistency of the input, i.e. that capacities on the arcs are
  // non-negative (>=0.)
  bool CheckInputConsistency() const;

  // Checks whether the result is valid, i.e. that node excesses are all equal
  // to zero (we have a flow) and that residual capacities are all non-negative
  // (>=0.)
  bool CheckResult() const;

  // Returns true if a precondition for Relabel is met, i.e. the outgoing arcs
  // of node are all either saturated or the heights of their heads are greater
  // or equal to the height of node.
  bool CheckRelabelPrecondition(NodeIndex node) const;

  // Returns context concatenated with information about arc
  // in a human-friendly way.
  string DebugString(const string& context, ArcIndex arc) const;

  // Initializes the container active_nodes_.
  virtual void InitializeActiveNodeContainer();

  // Get the first element from the active node container
  virtual NodeIndex GetAndRemoveFirstActiveNode() {
    const NodeIndex node = active_nodes_.top();
      active_nodes_.pop();
      return node;
  }

  // Push element to the active node container
  virtual void PushActiveNode(const NodeIndex& node) {
    active_nodes_.push(node);
  }

  // Check the emptiness of the container
  virtual bool IsEmptyActiveNodeContainer() {
    return active_nodes_.empty();
  }

  // Performs optimization step.
  void Refine();

  // Discharges an active node node by saturating its admissible adjacent arcs,
  // if any, and by relabelling it when it becomes inactive.
  virtual void Discharge(NodeIndex node);

  // Resets the first_admissible_arc_ array to the first incident arc of each
  // node.
  void ResetFirstAdmissibleArcs();

  // Initializes the preflow to a state that enables to run Optimize.
  void InitializePreflow();

  // Pushes flow on arc,  i.e. consumes flow on residual_arc_capacity_[arc],
  // and consumes -flow on residual_arc_capacity_[Opposite(arc)]. Updates
  // node_excess_ at the tail and head of arc accordingly.
  void PushFlow(FlowQuantity flow, ArcIndex arc);

  // Relabels a node, i.e. increases its height by the minimum necessary amount.
  void Relabel(NodeIndex node);

  // Handy member functions to make the code more compact.
  NodeIndex Head(ArcIndex arc) const { return graph_->Head(arc); }

  NodeIndex Tail(ArcIndex arc) const { return graph_->Tail(arc); }

  ArcIndex Opposite(ArcIndex arc) const { return graph_->Opposite(arc); }

  bool IsDirect(ArcIndex arc) const { return graph_->IsDirect(arc); }

  // A pointer to the graph passed as argument.
  const StarGraph* graph_;

  // A packed array representing the excess for each node in graph_.
  QuantityArray node_excess_;

  // A packed array representing the height function for each node in graph_.
  CostArray node_potential_;

  // A packed array representing the residual_capacity for each arc in graph_.
  // Residual capacities enable one to represent the capacity and flow for all
  // arcs in the graph in the following manner.
  // For all arc, residual_arc_capacity_[arc] = capacity[arc] - flow[arc]
  // Moreover, for reverse arcs, capacity[arc] = 0 by definition,
  // Also flow[Opposite(arc)] = -flow[arc] by definition.
  // Therefore:
  // - for a direct arc:
  //    flow[arc] = 0 - flow[Opposite(arc)]
  //              = capacity[Opposite(arc)] - flow[Opposite(arc)]
  //              = residual_arc_capacity_[Opposite(arc)]
  // - for a reverse arc:
  //    flow[arc] = -residual_arc_capacity_[arc]
  // Using these facts enables one to only maintain residual_arc_capacity_,
  // instead of both capacity and flow, for each direct and indirect arc. This
  // reduces the amount of memory for this information by a factor 2.s reduces
  // the amount of memory for this information by a factor 2.
  QuantityArray residual_arc_capacity_;

  // A packed array representing the first admissible arc for each node
  // in graph_.
  ArcIndexArray first_admissible_arc_;

  // A stack used for managing active nodes in the algorithm.
  // Note that the papers cited above recommend the use of a queue, but
  // benchmarking so far has not proved it is better.
  std::stack<NodeIndex> active_nodes_;

  // The index of the source node in graph_.
  NodeIndex source_;

  // The index of the sink node in graph_.
  NodeIndex sink_;

  // The total flow.
  FlowQuantity total_flow_;

  // The status of the problem.
  Status status_;

  DISALLOW_COPY_AND_ASSIGN(MaxFlow);
};
}  // namespace operations_research
#endif  // OR_TOOLS_GRAPH_MAX_FLOW_H_
