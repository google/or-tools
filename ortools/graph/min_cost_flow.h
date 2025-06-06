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

// An implementation of a cost-scaling push-relabel algorithm for
// the min-cost flow problem.
//
// In the following, we consider a graph G = (V,E) where V denotes the set
// of nodes (vertices) in the graph, E denotes the set of arcs (edges).
// n = |V| denotes the number of nodes in the graph, and m = |E| denotes the
// number of arcs in the graph.
//
// With each arc (v,w) is associated a nonnegative capacity u(v,w)
// (where 'u' stands for "upper bound") and a unit cost c(v,w). With
// each node v is associated a quantity named supply(v), which
// represents a supply of fluid (if >0) or a demand (if <0).
// Furthermore, no fluid is created in the graph so
//     sum_{v in V} supply(v) = 0.
//
// A flow is a function from E to R such that:
// a) f(v,w) <= u(v,w) for all (v,w) in E (capacity constraint).
// b) f(v,w) = -f(w,v) for all (v,w) in E (flow antisymmetry constraint).
// c) sum on v f(v,w) + supply(w) = 0  (flow conservation).
//
// The cost of a flow is sum on (v,w) in E ( f(v,w) * c(v,w) ) [Note:
// It can be confusing to beginners that the cost is actually double
// the amount that it might seem at first because of flow
// antisymmetry.]
//
// The problem to solve: find a flow of minimum cost such that all the
// fluid flows from the supply nodes to the demand nodes.
//
// The principles behind this algorithm are the following:
//  1/ handle pseudo-flows instead of flows and refine pseudo-flows until an
// epsilon-optimal minimum-cost flow is obtained,
//  2/ deal with epsilon-optimal pseudo-flows.
//
// 1/ A pseudo-flow is like a flow, except that a node's outflow minus
// its inflow can be different from its supply. If it is the case at a
// given node v, it is said that there is an excess (or deficit) at
// node v. A deficit is denoted by a negative excess and inflow =
// outflow + excess.
// (Look at ortools/graph/max_flow.h to see that the definition
// of preflow is more restrictive than the one for pseudo-flow in that a preflow
// only allows non-negative excesses, i.e., no deficit.)
// More formally, a pseudo-flow is a function f such that:
// a) f(v,w) <= u(v,w) for all (v,w) in E  (capacity constraint).
// b) f(v,w) = -f(w,v) for all (v,w) in E (flow antisymmetry constraint).
//
// For each v in E, we also define the excess at node v, the algebraic sum of
// all the incoming preflows at this node, added together with the supply at v.
//    excess(v) = sum on u f(u,v) + supply(v)
//
// The goal of the algorithm is to obtain excess(v) = 0 for all v in V, while
// consuming capacity on some arcs, at the lowest possible cost.
//
// 2/ Internally to the algorithm and its analysis (but invisibly to
// the client), each node has an associated "price" (or potential), in
// addition to its excess. It is formally a function from E to R (the
// set of real numbers.). For a given price function p, the reduced
// cost of an arc (v,w) is:
//    c_p(v,w) = c(v,w) + p(v) - p(w)
// (c(v,w) is the cost of arc (v,w).) For those familiar with linear
// programming, the price function can be viewed as a set of dual
// variables.
//
// For a constant epsilon >= 0, a pseudo-flow f is said to be epsilon-optimal
// with respect to a price function p if for every residual arc (v,w) in E,
//    c_p(v,w) >= -epsilon.
//
// A flow f is optimal if and only if there exists a price function p such that
// no arc is admissible with respect to f and p.
//
// If the arc costs are integers, and epsilon < 1/n, any epsilon-optimal flow
// is optimal. The integer cost case is handled by multiplying all the arc costs
// and the initial value of epsilon by (n+1). When epsilon reaches 1, and
// the solution is epsilon-optimal, it means: for all residual arc (v,w) in E,
//    (n+1) * c_p(v,w) >= -1, thus c_p(v,w) >= -1/(n+1) > -1/n, and the
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
// IMPORTANT:
// The algorithm is not able to detect the infeasibility of a problem (i.e.,
// when a bottleneck in the network prohibits sending all the supplies.)
// Worse, it could in some cases loop forever. This is why feasibility checking
// is enabled by default (FLAGS_min_cost_flow_check_feasibility=true.)
// Feasibility checking is implemented using a max-flow, which has a much lower
// complexity. The impact on performance is negligible, while the risk of being
// caught in an endless loop is removed. Note that using the feasibility checker
// roughly doubles the memory consumption.
//
// The starting reference for this class of algorithms is:
// A.V. Goldberg and R.E. Tarjan, "Finding Minimum-Cost Circulations by
// Successive Approximation." Mathematics of Operations Research, Vol. 15,
// 1990:430-466.
// http://portal.acm.org/citation.cfm?id=92225
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
// ftp://dimacs.rutgers.edu/pub/netflow/submit/papers/Goldberg-mincost/scalmin.ps
// and in:
// ﻿U. Bunnagel, B. Korte, and J. Vygen. “Efficient implementation of the
// Goldberg-Tarjan minimum-cost flow algorithm.” Optimization Methods and
// Software (1998) vol. 10, no. 2:157-174.
// http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.84.9897
//
// We have tried as much as possible in this implementation to keep the
// notations and namings of the papers cited above, except for 'demand' or
// 'balance' which have been replaced by 'supply', with the according sign
// changes to better accommodate with the API of the rest of our tools. A demand
// is denoted by a negative supply.
//
// TODO(user): See whether the following can bring any improvements on real-life
// problems.
// R.K. Ahuja, A.V. Goldberg, J.B. Orlin, and R.E. Tarjan, "Finding minimum-cost
// flows by double scaling," Mathematical Programming, (1992) 53:243-266.
// http://www.springerlink.com/index/gu7404218u6kt166.pdf
//
// An interesting general reference on network flows is:
// R. K. Ahuja, T. L. Magnanti, J. B. Orlin, "Network Flows: Theory, Algorithms,
// and Applications," Prentice Hall, 1993, ISBN: 978-0136175490,
// http://www.amazon.com/dp/013617549X
//
// Keywords: Push-relabel, min-cost flow, network, graph, Goldberg, Tarjan,
//           Dinic, Dinitz.

#ifndef OR_TOOLS_GRAPH_MIN_COST_FLOW_H_
#define OR_TOOLS_GRAPH_MIN_COST_FLOW_H_

#include <cstdint>
#include <stack>
#include <string>
#include <vector>

#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "ortools/graph/graph.h"
#include "ortools/util/stats.h"
#include "ortools/util/zvector.h"

namespace operations_research {

// Forward declaration.
template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
class GenericMinCostFlow;

// Different statuses for a solved problem.
// We use a base class to share it between our different interfaces.
class MinCostFlowBase {
 public:
  enum Status {
    NOT_SOLVED,
    OPTIMAL,
    FEASIBLE,
    INFEASIBLE,
    UNBALANCED,
    BAD_RESULT,

    // This is returned when an integer overflow occurred during the algorithm
    // execution.
    //
    // Some details on how to deal with this:
    // - Since we scale cost, each arc unit cost times (num_nodes + 1) should
    //   not overflow. We detect that at the beginning of the Solve().
    // - This is however not sufficient as the node potential depends on the
    //   minimum cost of sending 1 unit of flow through the residual graph. If
    //   the maximum arc unit cost is smaller than kint64max / (2 * n ^ 2) then
    //   one shouldn't run into any overflow. But in pratice this bound is quite
    //   loose. So it is possible to try with higher cost, and the algorithm
    //   will detect if an overflow actually happen and return BAD_COST_RANGE,
    //   so we can retry with smaller cost.
    //
    // And two remarks:
    // - Note that the complexity of the algorithm depends on the maximum cost,
    //   so it is usually a good idea to use unit cost that are as small as
    //   possible.
    // - Even if there is no overflow, note that the total cost can easily not
    //   fit on an int64_t since it is the product of the unit cost times the
    //   actual amount of flow sent. This is easy to detect since the final
    //   optimal cost will be set to kint64max. It is also relatively easy to
    //   deal with since we will still have the proper flow on each arc. It is
    //   thus possible to recompute the total cost in double or using
    //   absl::int128 if the need arise.
    BAD_COST_RANGE,

    // This is returned in our initial check if the arc capacity are too large.
    // For each node these quantity should not overflow the FlowQuantity type
    // which is int64_t by default.
    // - Its initial excess (+supply or -demand) + sum incoming arc capacity
    // - Its initial excess (+supply or -demand) - sum outgoing arc capacity.
    //
    // Note that these are upper bounds that guarantee that no overflow will
    // take place during the algorithm execution. It is possible to go over that
    // and still encounter no overflow, but since we cannot guarantee this, we
    // rather check early and return a proper status.
    //
    // Note that we might cap the capacity of the arcs if we can detect it
    // is too large in order to avoid returning this status for simple case,
    // like if a client used int64_t max for all arcs out of the source.
    //
    // TODO(user): Not sure this is a good idea, probably better to make sure
    // client use reasonable capacities. Also we should template by FlowQuantity
    // and allow use of absl::int128 so we never have issue if the input is
    // int64_t.
    BAD_CAPACITY_RANGE,
  };
};

// A simple and efficient min-cost flow interface. This is as fast as
// GenericMinCostFlow<ReverseArcStaticGraph>, which is the fastest, but is uses
// more memory in order to hide the somewhat involved construction of the
// static graph.
//
// TODO(user): If the need arises, extend this interface to support warm start
// and incrementality between solves. Note that this is already supported by the
// GenericMinCostFlow<> interface.
class SimpleMinCostFlow : public MinCostFlowBase {
 public:
  typedef int32_t NodeIndex;
  typedef int32_t ArcIndex;
  typedef int64_t FlowQuantity;
  typedef int64_t CostValue;

  // By default, the constructor takes no size. New node indices are created
  // lazily by AddArcWithCapacityAndUnitCost() or SetNodeSupply() such that the
  // set of valid nodes will always be [0, NumNodes()).
  //
  // You may pre-reserve the internal data structures with a given expected
  // number of nodes and arcs, to potentially gain performance.
  explicit SimpleMinCostFlow(NodeIndex reserve_num_nodes = 0,
                             ArcIndex reserve_num_arcs = 0);

#ifndef SWIG
  // This type is neither copyable nor movable.
  SimpleMinCostFlow(const SimpleMinCostFlow&) = delete;
  SimpleMinCostFlow& operator=(const SimpleMinCostFlow&) = delete;
#endif

  // Adds a directed arc from tail to head to the underlying graph with
  // a given capacity and cost per unit of flow.
  // * Node indices and the capacity must be non-negative (>= 0).
  // * The unit cost can take any integer value (even negative).
  // * Self-looping and duplicate arcs are supported.
  // * After the method finishes, NumArcs() == the returned ArcIndex + 1.
  ArcIndex AddArcWithCapacityAndUnitCost(NodeIndex tail, NodeIndex head,
                                         FlowQuantity capacity,
                                         CostValue unit_cost);

  // Modifies the capacity of the given arc. The arc index must be non-negative
  // (>= 0); it must be an index returned by a previous call to
  // AddArcWithCapacityAndUnitCost().
  void SetArcCapacity(ArcIndex arc, FlowQuantity capacity);

  // Sets the supply of the given node. The node index must be non-negative (>=
  // 0). Nodes implicitly created will have a default supply set to 0. A demand
  // is modeled as a negative supply.
  void SetNodeSupply(NodeIndex node, FlowQuantity supply);

  // Solves the problem, and returns the problem status. This function
  // requires that the sum of all node supply minus node demand is zero and
  // that the graph has enough capacity to send all supplies and serve all
  // demands. Otherwise, it will return INFEASIBLE.
  Status Solve() {
    return SolveWithPossibleAdjustment(SupplyAdjustment::DONT_ADJUST);
  }

  // Same as Solve(), but does not have the restriction that the supply
  // must match the demand or that the graph has enough capacity to serve
  // all the demand or use all the supply. This will compute a maximum-flow
  // with minimum cost. The value of the maximum-flow will be given by
  // MaximumFlow().
  Status SolveMaxFlowWithMinCost() {
    return SolveWithPossibleAdjustment(SupplyAdjustment::ADJUST);
  }

  // Returns the cost of the minimum-cost flow found by the algorithm when
  // the returned Status is OPTIMAL.
  CostValue OptimalCost() const;

  // Returns the total flow of the minimum-cost flow found by the algorithm
  // when the returned Status is OPTIMAL.
  FlowQuantity MaximumFlow() const;

  // Returns the flow on arc, this only make sense for a successful Solve().
  //
  // Note: It is possible that there is more than one optimal solution. The
  // algorithm is deterministic so it will always return the same solution for
  // a given problem. However, there is no guarantee of this from one code
  // version to the next (but the code does not change often).
  FlowQuantity Flow(ArcIndex arc) const;

  // Accessors for the user given data. The implementation will crash if "arc"
  // is not in [0, NumArcs()) or "node" is not in [0, NumNodes()).
  NodeIndex NumNodes() const;
  ArcIndex NumArcs() const;
  NodeIndex Tail(ArcIndex arc) const;
  NodeIndex Head(ArcIndex arc) const;
  FlowQuantity Capacity(ArcIndex arc) const;
  FlowQuantity Supply(NodeIndex node) const;
  CostValue UnitCost(ArcIndex arc) const;

  // Advanced usage. The default is true.
  //
  // Without cost scaling, the algorithm will return a 1-optimal solution to the
  // given problem. The solution will be an optimal solution to a perturbed
  // problem where some of the arc unit costs are changed by at most 1.
  //
  // If the cost are initially integer and we scale them by (num_nodes + 1),
  // then we can show that such 1-optimal solution is actually optimal. This
  // is what happen by default or when SetPriceScaling(true) is called.
  //
  // However, if your cost were originally double, you don't really care to
  // solve optimally a problem where the weights are approximated in the first
  // place. It is better to multiply your double by a scaling_factor (prefer a
  // power of 2) so that the maximum rounded arc unit cost is under kint64max /
  // (num_nodes + 1) to prevent any overflow. You can then solve without any
  // cost scaling. The final result will be the optimal to a problem were the
  // unit cost of some arc as been changed by at most 1 / scaling_factor.
  void SetPriceScaling(bool value) { scale_prices_ = value; }

 private:
  typedef ::util::ReverseArcStaticGraph<NodeIndex, ArcIndex> Graph;
  enum SupplyAdjustment { ADJUST, DONT_ADJUST };

  // Applies the permutation in arc_permutation_ to the given arc index.
  ArcIndex PermutedArc(ArcIndex arc);
  // Solves the problem, potentially applying supply and demand adjustment,
  // and returns the problem status.
  Status SolveWithPossibleAdjustment(SupplyAdjustment adjustment);
  void ResizeNodeVectors(NodeIndex node);

  std::vector<NodeIndex> arc_tail_;
  std::vector<NodeIndex> arc_head_;
  std::vector<FlowQuantity> arc_capacity_;
  std::vector<FlowQuantity> node_supply_;
  std::vector<CostValue> arc_cost_;
  std::vector<ArcIndex> arc_permutation_;
  std::vector<FlowQuantity> arc_flow_;
  CostValue optimal_cost_;
  FlowQuantity maximum_flow_;

  bool scale_prices_ = true;
};

// Generic MinCostFlow that works with all the graphs handling reverse arcs from
// graph.h, see the end of min_cost_flow.cc for the exact types this class is
// compiled for.
//
// One can greatly decrease memory usage by using appropriately small integer
// types:
// - For the Graph<> types, i.e. NodeIndexType and ArcIndexType, see graph.h.
// - ArcFlowType is used for the *per-arc* flow quantity. It must be signed, and
//   large enough to hold the maximum arc capacity and its negation.
// - ArcScaledCostType is used for a per-arc scaled cost. It must be signed
//   and large enough to hold the maximum unit cost of an arc times
//   (num_nodes + 1).
//
// Note that the latter two are different than FlowQuantity and CostValue, which
// are used for global, aggregated values and may need to be larger.
//
// Also uses the Arc*Type where there is no risk of overflow in more places.
template <typename Graph, typename ArcFlowType = int64_t,
          typename ArcScaledCostType = int64_t>
class GenericMinCostFlow : public MinCostFlowBase {
 public:
  typedef typename Graph::NodeIndex NodeIndex;
  typedef typename Graph::ArcIndex ArcIndex;
  typedef int64_t CostValue;
  typedef int64_t FlowQuantity;
  typedef typename Graph::OutgoingOrOppositeIncomingArcIterator
      OutgoingOrOppositeIncomingArcIterator;
  typedef ZVector<ArcIndex> ArcIndexArray;

  // Initialize a MinCostFlow instance on the given graph. The graph does not
  // need to be fully built yet, but its capacity reservation is used to
  // initialize the memory of this class.
  explicit GenericMinCostFlow(const Graph* graph);

#ifndef SWIG
  // This type is neither copyable nor movable.
  GenericMinCostFlow(const GenericMinCostFlow&) = delete;
  GenericMinCostFlow& operator=(const GenericMinCostFlow&) = delete;
#endif

  // Returns the graph associated to the current object.
  const Graph* graph() const { return graph_; }

  // Returns the status of last call to Solve(). NOT_SOLVED is returned if
  // Solve() has never been called or if the problem has been modified in such a
  // way that the previous solution becomes invalid.
  Status status() const { return status_; }

  // Sets the supply corresponding to node. A demand is modeled as a negative
  // supply.
  void SetNodeSupply(NodeIndex node, FlowQuantity supply);

  // Sets the unit cost for the given arc.
  void SetArcUnitCost(ArcIndex arc, ArcScaledCostType unit_cost);

  // Sets the capacity for the given arc.
  void SetArcCapacity(ArcIndex arc, ArcFlowType new_capacity);

  // Solves the problem, returning true if a min-cost flow could be found.
  bool Solve();

  // Returns the cost of the minimum-cost flow found by the algorithm. This
  // works in O(num_arcs). This will only work if the last Solve() call was
  // successful and returned true, otherwise it will return 0. Note that the
  // computation might overflow, in which case we will cap the cost at
  // std::numeric_limits<CostValue>::max().
  CostValue GetOptimalCost();

  // Returns the flow on the given arc using the equations given in the
  // comment on residual_arc_capacity_.
  FlowQuantity Flow(ArcIndex arc) const;

  // Returns the capacity of the given arc.
  //
  // Warning: If the capacity were close to ArcFlowType::max() we might have
  // adjusted them in order to avoid overflow.
  FlowQuantity Capacity(ArcIndex arc) const;

  // Returns the unscaled cost for the given arc.
  CostValue UnitCost(ArcIndex arc) const;

  // Returns the supply at a given node.
  // Demands are modelled as negative supplies.
  FlowQuantity Supply(NodeIndex node) const;

  // Whether to check the feasibility of the problem with a max-flow, prior to
  // solving it. This uses about twice as much memory, but detects infeasible
  // problems (where the flow can't be satisfied) and makes Solve() return
  // INFEASIBLE. If you disable this check, you will spare memory but you must
  // make sure that your problem is feasible, otherwise the code can loop
  // forever.
  void SetCheckFeasibility(bool value) { check_feasibility_ = value; }

  // Algorithm options.
  void SetUseUpdatePrices(bool value) { use_price_update_ = value; }
  void SetPriceScaling(bool value) { scale_prices_ = value; }

 private:
  // Checks for feasibility, i.e., that all the supplies and demands can be
  // matched without exceeding bottlenecks in the network.
  // Note that CheckFeasibility is called by Solve() when SetCheckFeasibility()
  // is set to true, which is the default.
  bool CheckFeasibility();

  // Returns true if the given arc is admissible i.e. if its residual capacity
  // is strictly positive, and its reduced cost strictly negative, i.e., pushing
  // more flow into it will result in a reduction of the total cost.
  bool IsAdmissible(ArcIndex arc) const;
  bool FastIsAdmissible(ArcIndex arc, CostValue tail_potential) const;

  // Returns true if node is active, i.e., if its supply is positive.
  bool IsActive(NodeIndex node) const;

  // Returns the reduced cost for a given arc.
  CostValue ReducedCost(ArcIndex arc) const;
  CostValue FastReducedCost(ArcIndex arc, CostValue tail_potential) const;

  // Returns the first incident arc of a given node.
  ArcIndex GetFirstOutgoingOrOppositeIncomingArc(NodeIndex node) const;

  // Checks the consistency of the input, i.e., whether the sum of the supplies
  // for all nodes is equal to zero. To be used in a DCHECK.
  bool CheckInputConsistency();

  // Checks whether the result is valid, i.e. whether for each arc,
  // residual_arc_capacity_[arc] == 0 || ReducedCost(arc) >= -epsilon_.
  // (A solution is epsilon-optimal if ReducedCost(arc) >= -epsilon.)
  // To be used in a DCHECK.
  bool CheckResult() const;

  // Checks the relabel precondition (to be used in a DCHECK):
  // - The node must be active, or have a 0 excess (relaxation for the Push
  //   Look-Ahead heuristic).
  // - The node must have no admissible arcs.
  bool CheckRelabelPrecondition(NodeIndex node) const;

  // Returns context concatenated with information about a given arc
  // in a human-friendly way.
  std::string DebugString(absl::string_view context, ArcIndex arc) const;

  // Resets the first_admissible_arc_ array to the first incident arc of each
  // node.
  void ResetFirstAdmissibleArcs();

  // Scales the costs, by multiplying them by (graph_->num_nodes() + 1).
  // Returns false on integer overflow.
  bool ScaleCosts();

  // Unscales the costs, by dividing them by (graph_->num_nodes() + 1).
  void UnscaleCosts();

  // Optimizes the cost by dividing epsilon_ by alpha_ and calling Refine().
  // Returns false on integer overflow.
  bool Optimize();

  // Saturates the admissible arcs, i.e., push as much flow as possible.
  void SaturateAdmissibleArcs();

  // Pushes flow on a given arc,  i.e., consumes flow on
  // residual_arc_capacity_[arc], and consumes -flow on
  // residual_arc_capacity_[Opposite(arc)]. Updates node_excess_ at the tail
  // and head of the arc accordingly.
  void PushFlow(FlowQuantity flow, ArcIndex arc);
  void FastPushFlow(FlowQuantity flow, ArcIndex arc, NodeIndex tail);

  // Initializes the stack active_nodes_.
  void InitializeActiveNodeStack();

  // Price update heuristics as described in A.V. Goldberg, "An Efficient
  // Implementation of a Scaling Minimum-Cost Flow Algorithm," Journal of
  // Algorithms, (1997) 22:1-29
  // http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.31.258
  // Returns false on overflow or infeasibility.
  bool UpdatePrices();

  // Performs an epsilon-optimization step by saturating admissible arcs
  // and discharging the active nodes.
  // Returns false on overflow or infeasibility.
  bool Refine();

  // Discharges an active node by saturating its admissible adjacent arcs,
  // if any, and by relabelling it when it becomes inactive.
  // Returns false on overflow or infeasibility.
  bool Discharge(NodeIndex node);

  // Part of the Push LookAhead heuristic.
  // Returns true iff the node has admissible arc.
  bool NodeHasAdmissibleArc(NodeIndex node);

  // Relabels node, i.e., decreases its potential while keeping the
  // epsilon-optimality of the pseudo flow. See CheckRelabelPrecondition() for
  // details on the preconditions.
  // Returns false on overflow or infeasibility.
  bool Relabel(NodeIndex node);

  // Handy member functions to make the code more compact.
  NodeIndex Head(ArcIndex arc) const { return graph_->Head(arc); }
  NodeIndex Tail(ArcIndex arc) const { return graph_->Tail(arc); }
  ArcIndex Opposite(ArcIndex arc) const;
  bool IsArcDirect(ArcIndex arc) const;
  bool IsArcValid(ArcIndex arc) const;

  // Pointer to the graph passed as argument.
  const Graph* graph_;

  // An array representing the supply (if > 0) or the demand (if < 0)
  // for each node in graph_.
  std::unique_ptr<FlowQuantity[]> node_excess_;

  // An array representing the potential (or price function) for
  // each node in graph_.
  std::unique_ptr<CostValue[]> node_potential_;

  // An array representing the residual_capacity for each arc in graph_.
  // Residual capacities enable one to represent the capacity and flow for all
  // arcs in the graph in the following manner.
  // For all arcs, residual_arc_capacity_[arc] = capacity[arc] - flow[arc]
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
  // Note that the sum of the largest capacity of an arc in the graph and of
  // the total flow in the graph mustn't exceed the largest 64 bit integer
  // to avoid errors. CheckInputConsistency() verifies this constraint.
  ZVector<ArcFlowType> residual_arc_capacity_;

  // An array representing the first admissible arc for each node in graph_.
  std::unique_ptr<ArcIndex[]> first_admissible_arc_;

  // A stack used for managing active nodes in the algorithm.
  // Note that the papers cited above recommend the use of a queue, but
  // benchmarking so far has not proved it is better.
  std::stack<NodeIndex> active_nodes_;

  // epsilon_ is the tolerance for optimality.
  CostValue epsilon_ = 0;

  // alpha_ is the factor by which epsilon_ is divided at each iteration of
  // Refine().
  const int64_t alpha_;

  // cost_scaling_factor_ is the scaling factor for cost.
  CostValue cost_scaling_factor_ = 1;

  // Our node potentials should be >= this at all time.
  // The first time a potential cross this, BAD_COST_RANGE status is returned.
  CostValue overflow_threshold_;

  // An array representing the scaled unit cost for each arc in graph_.
  ZVector<ArcScaledCostType> scaled_arc_unit_cost_;

  // The status of the problem.
  Status status_ = NOT_SOLVED;

  // An array containing the initial excesses (i.e. the supplies) for each
  // node. This is used to create the max-flow-based feasibility checker.
  std::unique_ptr<FlowQuantity[]> initial_node_excess_;

  // Statistics about this class.
  StatsGroup stats_;

  // Number of Relabel() since last UpdatePrices().
  int num_relabels_since_last_price_update_ = 0;

  // A Boolean which is true when feasibility has been checked.
  bool feasibility_checked_ = false;

  // Whether to use the UpdatePrices() heuristic.
  bool use_price_update_ = false;

  // Whether to check the problem feasibility with a max-flow.
  bool check_feasibility_ = false;

  // Whether to scale prices, see SimpleMinCostFlow::SetPriceScaling().
  bool scale_prices_ = true;
};

#if !SWIG

// Note: SWIG does not seem to understand explicit template specialization and
// instantiation declarations.

extern template class GenericMinCostFlow<::util::ReverseArcListGraph<>>;
extern template class GenericMinCostFlow<::util::ReverseArcStaticGraph<>>;
extern template class GenericMinCostFlow<
    ::util::ReverseArcStaticGraph<uint16_t, int32_t>>;
extern template class GenericMinCostFlow<
    ::util::ReverseArcListGraph<int64_t, int64_t>, int64_t, int64_t>;
extern template class GenericMinCostFlow<
    ::util::ReverseArcStaticGraph<uint16_t, int32_t>,
    /*ArcFlowType=*/int16_t,
    /*ArcScaledCostType=*/int32_t>;

// TODO(b/385094969): Remove this alias after 2025-07-01 to give or-tools users
// a grace period.
struct MinCostFlow : public MinCostFlowBase {
  template <typename = void>
  MinCostFlow() {
    LOG(FATAL) << "MinCostFlow is deprecated. Use `SimpleMinCostFlow` or "
                  "`GenericMinCostFlow` with a specific graph type instead.";
  }
};

#endif  // SWIG

}  // namespace operations_research
#endif  // OR_TOOLS_GRAPH_MIN_COST_FLOW_H_
