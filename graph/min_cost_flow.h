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

// An implementation of a cost-scaling push-relabel algorithm for
// the min-cost flow problem.
//
// In the following, we consider a graph G = (V,E) where V denotes the set
// of nodes (vertices) in the graph, E denotes the set of arcs (edges).
// n = |V| denotes the number of nodes in the graph, and m = |E| denotes the
// number of arcs in the graph.
//
// To each arc (v,w) is associated a capacity c(v,w) and a unit cost cost(v,w).
// To each node v is associated quantity named supply(v), which represents a
// supply of fluid (if >0) or a demand (if <0).
// Furthermore, no fluid are created in the graph so
//    sum on v in V supply(v) = 0
//
// A flow is a function from E to R such that:
// a) f(v,w) <= c(v,w) for all (v,w) in E (capacity constraint).
// b) f(v,w) = -f(w,v) for all (v,w) in E (flow antisymmetry constraint).
// c) sum on v f(v,w) = 0  (flow conservation.
//
// The cost of a flow is sum on (v,w) in E ( f(v,w) * cost(v,w) )
//
// The problem to solve is to find a flow of minimum cost such that all the
// fluid flows from the supply nodes to the demand nodes.
//
// The principles behind this algorithm are the following:
//  1/ handle pseudo-flows instead of flows and refine pseudo-flows until an
// epsilon-optimal minimum-cost flow is obtained,
//  2/ deal with epsilon-optimal pseudo-flows.
//
// 1/ A pseudo-flow is like a flow, except that the inflow can be different from
// the outflow. If it is the case at a given node v, it is said that there is an
// excess (or deficit) at node v. A deficit is denoted by a negative excess
// and inflow = outflow + excess.
// (Look at graph/max_flow.h to see that the definition
// of preflow is more restrictive than the one for pseudo-flow in that a preflow
// only allows non-negative excesses, i.e. no deficit.)
// More formally, a pseudo-flow is a function f such that:
// a) f(v,w) <= c(v,w) for all (v,w) in E  (capacity constraint).
// b) f(v,w) = -f(w,v) for all (v,w) in E (flow antisymmetry constraint).
//
// For each v in E, we also define the excess at node v, the algebraic sum of
// all the incoming preflows at this node, added together with the supply at v.
//    excess(v) = sum on u f(u,v) + supply(v)
//
// The goal of the algorithm is to obtain excess(v) = 0 for all v in V, while
// consuming capacity on some arcs, at the lowest possible cost.
//
// 2/ Each node has an associated "price" (or potential), in addition to its
// excess. It is formally a function from E to R (the set of real numbers.).
// For a given price function p, the reduced cost of an arc (v,w) is:
//    cost_p(v,w) = cost(v,w) + p(v) - p(w)
// (cost(v,w) is the cost of arc (v,w) .)
//
// For a constant epsilon >= 0, a pseudo-flow f is said to be epsilon-optimal
// with respect to a pricing function p if for all residual arc (v,w) in E,
//    cost_p(v,w) >= -epsilon.
//
// A flow f is optimal if and only if there exists a price function p such that
// no arc is admissible with respect to f and p.
//
// If the arc costs are integers, and epsilon < 1/n, any epsilon-optimal flow
// is optimal. The integer cost case is handled by multiplying all the arc costs
// and the initial value of epsilon by (n+1). When epsilon reaches 1, and
// the solution is epsilon-optimal, it means: for all residual arc (v,w) in E,
//    (n+1) * cost_p(v,w) >= -1, thus cost_p(v,w) >= -1/(n+1) >= 1/n, and the
// solution is optimal.
//
// A node v is said to be *active* if excess(v) > 0.
// In this case the following operations can be applied to it:
// - if there are *admissible* incident arcs, i.e. arcs which are not saturated,
//   and whose reduced costs are negative, a PushFlow operation can
//   be applied. It consists in sending as  much flow as both the excess at the
//   node and the capacity of the arc permit.
// - if there are no admissible arcs, the active node considered is relabeled,
// This is implemented in Discharge, which itself calls PushFlow and Relabel.
//
// Discharge itself is called by Refine. Refine first saturates all the
// admissible arcs, then builds a stack of active nodes. It then applies
// Discharge for each active node, possibly adding new ones in the process,
// until no nodes are active. In that case an epsilon-optimal flow is obtained.
//
// Optimize iteratively calls Refine, while epsilon > 1, and divides epsilon by
// alpha (set by default to 5) before each iteration.
//
// The algorithm starts with epsilon = C, where C is the maximum absolute value
// of the arc costs. In the integer case which we are dealing with, since all
// costs are multiplied by (n+1), the initial value of epsilon is (n+1)*C.
// The algorithm terminates when epsilon = 1, and Refine() has been called.
// In this case, a minimum-cost flow is obtained.
//
// The complexity of the algorithm is O(n^2*m*log(n*C)) where C is the value of
// the largest arc cost in the graph.
//
// The starting reference for this class of algorithms is:
// A.V. Goldberg and R.E. Tarjan, "Solving Minimum-Cost Flow Problems by
// Successive Approximation." Proceedings of ACM STOC'87, 1987:7-18.
// http://portal.acm.org/citation.cfm?id=28397&CFID=10541307&CFTOKEN=40586317
//
// Implementation issues are tackled in:
// A.V. Goldberg, "An Efficient Implementation of a Scaling Minimum-Cost Flow
// Algorithm," Journal of Algorithms, (1997) 22:1-29
// http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.31.258
//
// A.V. Goldberg and M. Kharitonov, "On Implementing Scaling Push-Relabel
// Algorithms for the Minimum-Cost Flow Problem", Network flows and matching:
// First DIMACS implementation challenge, DIMACS Series in Discrete Mathematics
// and Theoretical Computer Science, (1993) 12:157-198.
// ftp://dimacs.rutgers.edu/pub/netflow/...mincost/scalmin.ps
// and in:
// ﻿U. Bunnagel, B. Korte, and J. Vygen. “Efficient implementation of the
// Goldberg-Tarjan minimum-cost flow algorithm.” Optimization Methods and
// Software (1998) vol. 10, no. 2:157-174.
// http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.84.9897
//
// We have tried as much as possible in this implementation to keep the
// notations and namings of the papers cited above, except for 'demand' or
// 'balance' which have been replaced by 'supply', with the according sign
// changes to better accomodate with the API of the rest of our tools. A demand
// is denoted by a negative supply.
//
// TODO(user): See whether the following can bring any improvements on real-life
// problems.
// R.K. Ahuja, A.V. Goldberg, J.B. Orlin, and R.E. Tarjan, "Finding minimum-cost
// flows by double scaling," Mathematical Programming, (1992) 53:243-266.
// http://www.springerlink.com/index/gu7404218u6kt166.pdf

#ifndef GRAPH_MIN_COST_FLOW_H_
#define GRAPH_MIN_COST_FLOW_H_

#include <algorithm>
#include <stack>
#include <string>
#include <vector>
#include "base/integral_types.h"
#include "base/logging.h"
#include "graph/ebert_graph.h"

namespace operations_research {

class MinCostFlow {
 public:
  explicit MinCostFlow(const StarGraph& graph);

  // Sets the supply corresponding to node. A demand is modeled as a negative
  // supply.
  void SetNodeSupply(NodeIndex node, FlowQuantity supply) {
    DCHECK(graph_.CheckNodeValidity(node));
    node_excess_.Set(node, supply);
  }

  // Sets the unit cost for arc.
  void SetArcUnitCost(ArcIndex arc, CostValue unit_cost) {
    DCHECK(graph_.CheckArcValidity(arc));
    scaled_arc_unit_cost_.Set(arc, unit_cost);
  }

  // Sets the capacity for arc.
  void SetArcCapacity(ArcIndex arc, FlowQuantity quantity) {
    DCHECK(graph_.CheckArcValidity(arc));
    residual_arc_capacity_.Set(arc, quantity);
  }

  // Runs the algorithm and computes the min-cost flow.
  // The values of the flows for each arc can be accessed through the
  // member function Flow.
  CostValue ComputeMinCostFlow();

  // Returns the flow on arc using the equations given in the comment on
  // residual_arc_capacity_.
  FlowQuantity Flow(ArcIndex arc) const {
    DCHECK(graph_.CheckArcValidity(arc));
    if (IsDirect(arc)) {
      return residual_arc_capacity_[Opposite(arc)];
    } else {
      return -residual_arc_capacity_[arc];
    }
  }

  // Returns the capacity of arc using the equations given in the comment on
  // residual_arc_capacity_.
  FlowQuantity Capacity(ArcIndex arc) const {
    DCHECK(graph_.CheckArcValidity(arc));
    if (IsDirect(arc)) {
      return residual_arc_capacity_[arc]
           + residual_arc_capacity_[Opposite(arc)];
    } else {
      return 0;
    }
  }

  // Returns the unscaled cost for arc.
  FlowQuantity Cost(ArcIndex arc) const {
    DCHECK(graph_.CheckArcValidity(arc));
    DCHECK_EQ(1ULL, cost_scaling_factor_);
    return scaled_arc_unit_cost_[arc];
  }

  // Returns the supply at node. Demands are modelled as negative supplies.
  FlowQuantity Supply(NodeIndex node) const {
    DCHECK(graph_.CheckNodeValidity(node));
    return node_excess_[node];
  }

 private:
  // Returns true if arc is admissible i.e. if its residual capacity is stricly
  // positive, and its reduced cost stricly negative, i.e. pushing more flow
  // into it will result in a reduction of the total cost.
  bool IsAdmissible(ArcIndex arc) const {
    return residual_arc_capacity_[arc] > 0 && ReducedCost(arc) < 0;
  }

  // Returns true if node is active, i.e. if its supply is positive.
  bool IsActive(NodeIndex node) const {
    return node_excess_[node] > 0;
  }

  // Returns the reduced cost for an arc.
  CostValue ReducedCost(ArcIndex arc) const {
    DCHECK(graph_.CheckNodeValidity(Tail(arc)));
    DCHECK(graph_.CheckNodeValidity(Head(arc)));
    DCHECK_LE(node_potential_[Tail(arc)], 0);
    DCHECK_LE(node_potential_[Head(arc)], 0);
    return scaled_arc_unit_cost_[arc]
         + node_potential_[Tail(arc)]
         - node_potential_[Head(arc)];
  }

  // Returns the first incident arc of node.
  ArcIndex GetFirstIncidentArc(NodeIndex node) const {
    StarGraph::IncidentArcIterator arc_it(graph_, node);
    return arc_it.Index();
  }

  // Checks the consistency of the input, i.e. whether the sum of the supplies
  // for all nodes is equal to zero. To be used in a DCHECK.
  bool CheckInputConsistency() const;

  // Checks whether the result is valid, i.e. whether for each arc,
  // residual_arc_capacity_[arc] == 0 || ReducedCost(arc) >= -epsilon_ .
  // (A solution is epsilon-optimal if ReducedCost(arc) >= -epsilon.)
  // To be used in a DCHECK.
  bool CheckResult() const;

  // Checks that the cost range fits in the range of int64's.
  // To be used in a DCHECK.
  bool CheckCostRange() const;

  // Checks the relabel precondition, i.e. that none of the arc incident to
  // node is admissible. To be used in a DCHECK.
  bool CheckRelabelPrecondition(NodeIndex node) const;

  // Returns context concatenated with information about arc
  // in a human-friendly way.
  string DebugString(const string& context, ArcIndex arc) const;

  // Completes the graph by setting the capacity of reverse arcs to zero,
  // and their unit costs to the opposite of the costs of the correponding
  // direct arcs.
  void CompleteGraph();

  // Resets the first_admissible_arc_ array to the first incident arc of each
  // node.
  void ResetFirstAdmissibleArcs();

  // Scales the costs, by multiplying them by (graph_.num_nodes() + 1).
  void ScaleCosts();

  // Unscales the costs, by dividing them by (graph_.num_nodes() + 1).
  void UnscaleCosts();

  // Optimizes the cost by dividing epsilon_ by alpha_ and calling Refine().
  void Optimize();

  // Saturates the admissible arcs, i.e. push as much flow as possible.
  void SaturateAdmissibleArcs();

  // Pushes flow on arc,  i.e. consumes flow on residual_arc_capacity_[arc],
  // and consumes -flow on residual_arc_capacity_[Opposite(arc)]. Updates
  // node_excess_ at the tail and head of arc accordingly.
  void PushFlow(FlowQuantity flow, ArcIndex arc);

  // Initializes the stack active_nodes_.
  void InitializeActiveNodeStack();

  // Performs an epsilon-optimization step by saturating admissible arcs
  // and discharging the active nodes.
  void Refine();

  // Discharges an active node node by saturating its admissible adjacent arcs,
  // if any, and by relabelling it when it becomes inactive.
  void Discharge(NodeIndex node);

  // Relabels node, i.e. increases the its node_potential_ of node.
  // The preconditions are that node active, and no arc incident to node is
  // admissible.
  void Relabel(NodeIndex node);

  // Performs a set-relabel operation.
  void GlobalRelabel();

  // Handy member functions to make the code more compact.
  NodeIndex Head(ArcIndex arc) const { return graph_.Head(arc); }

  NodeIndex Tail(ArcIndex arc) const { return graph_.Tail(arc); }

  ArcIndex Opposite(ArcIndex arc) const { return graph_.Opposite(arc); }

  bool IsDirect(ArcIndex arc) const { return graph_.IsDirect(arc); }

  // A pointer to the graph passed as argument.
  const StarGraph& graph_;

  // A packed array representing the supply (if > 0) or the demand (if < 0)
  // for each node in graph_.
  Int40PackedArray node_excess_;

  // A packed array representing the potential (or price function) for
  // each node in graph_.
  Int64PackedArray node_potential_;

  // A packed array representing the residual_capacity for each arc in graph_.
  // Residual capacities enable one to represent the capacity and flow for all
  // arcs in the graph in the following manner.
  // For all arc, residual_arc_capacity_[arc] = capacity[arc] - flow[arc]
  // Moreover, for reverse arcs, capacity[arc] = 0 by definition.
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
  // reduces the amount of memory for this information by a factor 2.
  Int40PackedArray residual_arc_capacity_;

  // A packed array representing the first admissible arc for each node
  // in graph_.
  ArcIndexArray    first_admissible_arc_;

  // A stack used for managing active nodes in the algorithm.
  // Note that the papers cited above recommend the use of a queue, but
  // benchmarking so far has not proved it is better.
  stack<NodeIndex> active_nodes_;

  // epsilon_ is the tolerance for optimality.
  CostValue        epsilon_;

  // alpha_ is the factor by which epsilon_ is divided at each iteration of
  // Refine().
  const int64      alpha_;

  // cost_scaling_factor_ is the scaling factor for cost.
  CostValue        cost_scaling_factor_;

  // A packed array representing the scaled unit cost for each arc in graph_.
  Int64PackedArray scaled_arc_unit_cost_;

  DISALLOW_COPY_AND_ASSIGN(MinCostFlow);
};
}  // namespace operations_research
#endif  // GRAPH_MIN_COST_FLOW_H_
