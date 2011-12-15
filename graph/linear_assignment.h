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
//
// An implementation of a cost-scaling push-relabel algorithm for the
// assignment problem (minimum-cost perfect bipartite matching), from
// the paper of Goldberg and Kennedy (1995).
//
// This implementation finds the minimum-cost perfect assignment in
// the given graph with integral edge weights set through the
// SetArcCost method.
//
// Example usage:
//
// #include "graph/ebert_graph.h"
// #include "graph/linear_assignment.h"
// ...
// ::operations_research::NodeIndex num_nodes = ...;
// ::operations_research::NodeIndex num_left_nodes = num_nodes / 2;
// // Define a num_nodes/2 by num_nodes/2 assignment problem:
// ::operations_research::ArcIndex num_forward_arcs = ...;
// ::operations_research::ForwardStarGraph g(num_nodes, num_arcs);
// ::operations_research::LinearSumAssignment<
//     ::operations_research::ForwardStarGraph> a(g, num_left_nodes);
// for (int i = 0; i < num_forward_arcs; ++i) {
//   ::operations_research::NodeIndex this_arc_head = ...;
//   ::operations_research::NodeIndex this_arc_tail = ...;
//   ::operations_research::CostValue this_arc_cost = ...;
//   ::operations_research::ArcIndex this_arc_index =
//       g.AddArc(this_arc_tail, this_arc_head);
//   a.SetArcCost(this_arc_index, this_arc_cost);
//  }
//  // Compute the optimum assignment.
//  bool success = a.ComputeAssignment();
//  // Retrieve the cost of the optimum assignment.
//  CostValue optimum_cost = a.GetCost();
//  // Retrieve the node-node correspondence of the optimum assignment and the
//  // cost of each node pairing.
//  for (::operations_research::LinearSumAssignment::BipartiteLeftNodeIterator
//         node_it(a);
//       node_it.Ok();
//       node_it.Next()) {
//    ::operations_research::NodeIndex left_node = node_it.Index();
//    ::operations_research::NodeIndex right_node = a.GetMate(left_node);
//    ::operations_research::CostValue node_pair_cost =
//        a.GetAssignmentCost(left_node);
//    ...
//  }
//
// In the following, we consider a bipartite graph
//   G = (V = X union Y, E subset XxY),
// where V denodes the set of nodes (vertices) in the graph, E denotes
// the set of arcs (edges), n = |V| denotes the number of nodes in the
// graph, and m = |E| denotes the number of arcs in the graph.
//
// The set of nodes is divided into two parts, X and Y, and every arc
// must go between a node of X and a node of Y. With each arc is
// associated a cost c(v, w). A matching M is a subset of E with the
// property that no two arcs in M have a head or tail node in common,
// and a perfect matching is a matching that touches every node in the
// graph. The cost of a matching M is the sum of the costs of all the
// arcs in M.
//
// The assignment problem is to find a perfect matching of minimum
// cost in the given bipartite graph. The present algorithm reduces
// the assignment problem to an instance of the minimum-cost flow
// problem and takes advantage of special properties of the resulting
// minimum-cost flow problem to solve it efficiently using a
// push-relabel method. For more information about minimum-cost flow
// see google3/graph/min_cost_flow.h
//
// The method used here is the cost-scaling approach for the
// minimum-cost circulation problem as described in [Goldberg and
// Tarjan] with some technical modifications:
// 1. For efficiency, we solve a transportation problem instead of
//    minimum-cost circulation. We might revisit this decision if it
//    is important to handle problems in which no perfect matching
//    exists.
// 2. We use a modified "asymmetric" notion of epsilon-optimality in
//    which left-to-right residual arcs are required to have reduced
//    cost bounded below by zero and right-to-left residual arcs are
//    required to have reduced cost bounded below by -epsilon. For
//    each residual arc direction, the reduced-cost threshold for
//    admissibility is epsilon/2 above the threshold for epsilon
//    optimality.
// 3. We do not limit the applicability of the relabeling operation to
//    nodes with excess. Instead we use the double-push operation
//    (discussed in the Goldberg and Kennedy CSA paper and Kennedy's
//    thesis) which relabels right-side nodes just *after* they have
//    been discharged.
// The above differences are explained in detail in [Kennedy's thesis]
// and explained not quite as cleanly in [Goldberg and Kennedy's CSA
// paper]. But note that the thesis explanation uses a value of
// epsilon that's double what we use here.
//
// Some definitions:
//   Active: A node is called active when it has excess. It is
//     eligible to be pushed from. In this implementation, every active
//     node is on the left side of the graph where prices are determined
//     implicitly, so no left-side relabeling is necessary before
//     pushing from an active node. We do, however, need to compute
//     the implications for price changes on the affected right-side
//     nodes.
//   Admissible: A residual arc (one that can carry more flow) is
//     called admissible when its reduced cost is small enough. We can
//     push additional flow along such an arc without violating
//     epsilon-optimality. In the case of a left-to-right residual
//     arc, the reduced cost must be at most epsilon/2. In the case of
//     a right-to-left residual arc, the reduced cost must be at most
//     -epsilon/2. The careful reader will note that these thresholds
//     are not used explicitly anywhere in this implementation, and
//     the reason is the implicit pricing of left-side nodes.
//   Reduced cost: Essentially an arc's reduced cost is its
//     complementary slackness. In push-relabel algorithms this is
//       c_p(v, w) = p(v) + c(v, w) - p(w),
//     where p() is the node price function and c(v, w) is the cost of
//     the arc from v to w. See min_cost_flow.h for more details.
//   Partial reduced cost: We maintain prices implicitly for left-side
//     nodes in this implementation, so instead of reduced costs we
//     work with partial reduced costs, defined as
//       c'_p(v, w) = c(v, w) - p(w).
//
// We check at initialization time for the possibility of arithmetic
// overflow and warn if the given costs are too large. In many cases
// the bound we use to trigger the warning is pessimistic so the given
// problem can often be solved even if we warn that overflow is
// possible.
//
// We don't use the interface from
// operations_research/algorithms/hungarian.h because we want to be
// able to express sparse problems efficiently.
//
// When asked to solve the given assignment problem we return a
// boolean to indicate whether the given problem was feasible.
//
// References:
// [ Goldberg and Kennedy's CSA paper ] A. V. Goldberg and R. Kennedy,
// "An Efficient Cost Scaling Algorithm for the Assignment Problem."
// Mathematical Programming, Vol. 71, pages 153-178, December 1995.
//
// [ Goldberg and Tarjan ] A. V. Goldberg and R. E. Tarjan, "Finding
// Minimum-Cost Circulations by Successive Approximation." Mathematics
// of Operations Research, Vol. 15, No. 3, pages 430-466, August 1990.
//
// [ Kennedy's thesis ] J. R. Kennedy, Jr., "Solving Unweighted and
// Weighted Bipartite Matching Problems in Theory and Practice."
// Stanford University Doctoral Dissertation, Department of Computer
// Science, 1995.
//
// [ Burkard et al. ] R. Burkard, M. Dell'Amico, S. Martello, "Assignment
// Problems", SIAM, 2009, ISBN: 978-0898716634,
// http://www.amazon.com/dp/0898716632/
//
// [ Ahuja et al. ] R. K. Ahuja, T. L. Magnanti, J. B. Orlin, "Network Flows:
// Theory, Algorithms, and Applications," Prentice Hall, 1993,
// ISBN: 978-0136175490, http://www.amazon.com/dp/013617549X
//
// Keywords: linear sum assignment problem, Hungarian method, Goldberg, Kennedy.

#ifndef OR_TOOLS_GRAPH_LINEAR_ASSIGNMENT_H_
#define OR_TOOLS_GRAPH_LINEAR_ASSIGNMENT_H_

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "graph/ebert_graph.h"
#include "util/permutation.h"

using std::string;

DECLARE_int64(assignment_alpha);
DECLARE_int32(assignment_progress_logging_period);
DECLARE_bool(assignment_stack_order);

namespace operations_research {

template <typename GraphType> class LinearSumAssignment {
 public:
  // This class modifies the given graph by adding arcs to it as costs
  // are specified via SetArcCost, but does not take ownership.
  LinearSumAssignment(const GraphType& graph, NodeIndex num_left_nodes);
  virtual ~LinearSumAssignment() {}

  // Sets the cost-scaling divisor, i.e., the amount by which we
  // divide the scaling parameter on each iteration.
  void SetCostScalingDivisor(CostValue factor) {
    alpha_ = factor;
  }

  // Optimizes the layout of the graph for the access pattern our
  // implementation will use.
  void OptimizeGraphLayout(GraphType* graph);

  // Allows tests, iterators, etc., to inspect our underlying graph.
  inline const GraphType& Graph() const { return graph_; }

  // These handy member functions make the code more compact, and we
  // expose them to clients so that client code that doesn't have
  // direct access to the graph can learn about the optimum assignment
  // once it is computed.
  inline NodeIndex Head(ArcIndex arc) const {
    return graph_.Head(arc);
  }

  // Returns the original arc cost for use by a client that's
  // iterating over the optimum assignment.
  virtual CostValue ArcCost(ArcIndex arc) const {
    DCHECK_EQ(0, scaled_arc_cost_[arc] % cost_scaling_factor_);
    return scaled_arc_cost_[arc] / cost_scaling_factor_;
  }

  // Sets the cost of an arc already present in the given graph.
  virtual void SetArcCost(ArcIndex arc,
                          CostValue cost);

  // Computes the optimum assignment. Returns true on success. Return
  // value of false implies the given problem is infeasible.
  virtual bool ComputeAssignment();

  // Returns the cost of the minimum-cost perfect matching.
  // Precondition: success_ == true, signifying that we computed the
  // optimum assignment for a feasible problem.
  virtual CostValue GetCost() const;

  // Returns the total number of nodes in the given problem.
  virtual NodeIndex NumNodes() const {
    return graph_.num_nodes();
  }

  // Returns the number of nodes on the left side of the given
  // problem.
  virtual NodeIndex NumLeftNodes() const {
    return num_left_nodes_;
  }

  // Returns the arc through which the given node is matched.
  inline ArcIndex GetAssignmentArc(NodeIndex left_node) const {
    DCHECK_LT(left_node, num_left_nodes_);
    return matched_arc_[left_node];
  }

  // Returns the cost of the assignment arc incident to the given
  // node.
  inline CostValue GetAssignmentCost(NodeIndex node) const {
    return ArcCost(GetAssignmentArc(node));
  }

  // Returns the node to which the given node is matched.
  inline NodeIndex GetMate(NodeIndex left_node) const {
    DCHECK_LT(left_node, num_left_nodes_);
    ArcIndex matching_arc = GetAssignmentArc(left_node);
    DCHECK_NE(GraphType::kNilArc, matching_arc);
    return Head(matching_arc);
  }

  string StatsString() const {
    return total_stats_.StatsString();
  }

  class BipartiteLeftNodeIterator {
   public:
    BipartiteLeftNodeIterator(const GraphType& graph, NodeIndex num_left_nodes)
        : num_left_nodes_(num_left_nodes),
          node_iterator_(graph) { }

    explicit BipartiteLeftNodeIterator(const LinearSumAssignment& assignment)
        : num_left_nodes_(assignment.NumLeftNodes()),
          node_iterator_(assignment.Graph()) { }

    NodeIndex Index() const { return node_iterator_.Index(); }

    bool Ok() const {
      return node_iterator_.Ok() && (node_iterator_.Index() < num_left_nodes_);
    }

    void Next() { node_iterator_.Next(); }

   private:
    const NodeIndex num_left_nodes_;
    typename GraphType::NodeIterator node_iterator_;
  };

 private:
  struct Stats {
    Stats()
        : pushes_(0),
          double_pushes_(0),
          relabelings_(0),
          refinements_(0) { }
    void Clear() {
      pushes_ = 0;
      double_pushes_ = 0;
      relabelings_ = 0;
      refinements_ = 0;
    }
    void Add(const Stats& that) {
      pushes_ += that.pushes_;
      double_pushes_ += that.double_pushes_;
      relabelings_ += that.relabelings_;
      refinements_ += that.refinements_;
    }
    string StatsString() const {
      return StringPrintf("%lld refinements; %lld relabelings; "
                          "%lld double pushes; %lld pushes",
                          refinements_,
                          relabelings_,
                          double_pushes_,
                          pushes_);
    }
    int64 pushes_;
    int64 double_pushes_;
    int64 relabelings_;
    int64 refinements_;
  };

  class ActiveNodeContainerInterface {
   public:
    virtual ~ActiveNodeContainerInterface() {}
    virtual bool Empty() const = 0;
    virtual void Add(NodeIndex node) = 0;
    virtual NodeIndex Get() = 0;
  };

  class ActiveNodeStack : public ActiveNodeContainerInterface {
   public:
    virtual ~ActiveNodeStack() {}

    virtual bool Empty() const {
      return v_.empty();
    }

    virtual void Add(NodeIndex node) {
      v_.push_back(node);
    }

    virtual NodeIndex Get() {
      DCHECK(!Empty());
      NodeIndex result = v_.back();
      v_.pop_back();
      return result;
    }

   private:
    std::vector<NodeIndex> v_;
  };

  class ActiveNodeQueue : public ActiveNodeContainerInterface {
   public:
    virtual ~ActiveNodeQueue() {}

    virtual bool Empty() const {
      return q_.empty();
    }

    virtual void Add(NodeIndex node) {
      q_.push_front(node);
    }

    virtual NodeIndex Get() {
      DCHECK(!Empty());
      NodeIndex result= q_.back();
      q_.pop_back();
      return result;
    }

   private:
    std::deque<NodeIndex> q_;
  };

  // Type definition for a pair
  //   (arc index, reduced cost gap)
  // giving the arc along which we will push from a given left-side
  // node and the gap between that arc's partial reduced cost and the
  // reduced cost of the next-best (necessarily residual) arc out of
  // the node. This information helps us efficiently relabel
  // right-side nodes during DoublePush operations.
  typedef std::pair<ArcIndex, CostValue> ImplicitPriceSummary;

  // Returns true if and only if the current pseudoflow is
  // epsilon-optimal. To be used in a DCHECK.
  bool EpsilonOptimal() const;

  // Checks that all nodes are matched.
  // To be used in a DCHECK.
  bool AllMatched() const;

  // Calculates the implicit price of the given node.
  // Only for debugging, for use in EpsilonOptimal().
  inline CostValue ImplicitPrice(NodeIndex left_node) const;

  // Separate from ComputeAssignment() for white-box testing only.
  // Completes initialization after the problem is fully
  // specified. Returns true if we successfully prove that arithmetic
  // calculations are guaranteed not to overflow.
  //
  // FinalizeSetup is idempotent.
  virtual bool FinalizeSetup();

  // For use by DoublePush()
  inline ImplicitPriceSummary BestArcAndGap(NodeIndex left_node) const;

  // Accumulates stats between iterations and reports them if the
  // verbosity level is high enough.
  void ReportAndAccumulateStats();

  // Advances internal state to prepare for the next scaling
  // iteration. Returns false if infeasibility is detected, true
  // otherwise.
  bool UpdateEpsilon();

  // Indicates whether the given left_node has positive excess. Called
  // only for nodes on the left side.
  inline bool IsActive(NodeIndex left_node) const;

  // Indicates whether the given node has nonzero excess. The idea
  // here is the same as the IsActive method above, but that method
  // contains a safety DCHECK() that its argument is a left-side node,
  // while this method is usable for any node.
  // To be used in a DCHECK.
  inline bool IsActiveForDebugging(NodeIndex node) const;

  // Performs the push/relabel work for one scaling iteration.
  bool Refine();

  // Puts all left-side nodes in the active set in preparation for the
  // first scaling iteration.
  void InitializeActiveNodeContainer();

  // Saturates all negative-reduced-cost arcs at the beginning of each
  // scaling iteration. Note that according to the asymmetric
  // definition of admissibility, this action is different from
  // saturating all admissible arcs (which we never do). All negative
  // arcs are admissible, but not all admissible arcs are negative. It
  // is alwsys enough to saturate only the negative ones.
  void SaturateNegativeArcs();

  // Performs an optimized sequence of pushing a unit of excess out of
  // the left-side node v and back to another left-side node if no
  // deficit is cancelled with the first push.
  bool DoublePush(NodeIndex source);

  // Returns the partial reduced cost of the given arc.
  inline CostValue PartialReducedCost(ArcIndex arc) const {
    return scaled_arc_cost_[arc] - price_[Head(arc)];
  }

  // The graph underlying the problem definition we are given. Not
  // const because we add arcs to the graph via our SetArcCost()
  // method.
  const GraphType& graph_;

  // The number of nodes on the left side of the graph we are given.
  NodeIndex num_left_nodes_;

  // A flag indicating that an optimal perfect matching has been computed.
  bool success_;

  // The value by which we multiply all the arc costs we are given in
  // order to be able to use integer arithmetic in all our
  // computations. In order to establish optimality of the final
  // matching we compute, we need that
  //   (cost_scaling_factor_ / kMinEpsilon) > graph_.num_nodes().
  const CostValue cost_scaling_factor_;

  // Scaling divisor.
  CostValue alpha_;

  // Minimum value of epsilon. When a flow is epsilon-optimal for
  // epsilon == kMinEpsilon, the flow is optimal.
  static const CostValue kMinEpsilon;

  // Current value of epsilon, the cost scaling parameter.
  CostValue epsilon_;

  // A lower bound on the price of any node at any time throughout the
  // computation. A price below this level proves infeasibility.
  //
  // The value of this lower bound is determined according to the
  // following sketch: Suppose the price decrease of every node in the
  // iteration with epsilon_ == x is bounded by B(x) which is
  // proportional to x. Then the total price decrease of every node
  // across all iterations is bounded above by
  //   B(C/alpha) + B(C/alpha^2) + ... + B(kMinEpsilon)
  //   == B(C/alpha) * alpha / (alpha - 1)
  //   == B(C) / (alpha - 1).
  // Therefore we set price_lower_bound_ = -ceil(B(C) / (alpha - 1))
  // where B() is the expression that determines
  // price_reduction_bound_, discussed below.
  CostValue price_lower_bound_;

  // An upper bound on the amount that a single node's price can
  // decrease in a single scaling iteration. In each iteration, this
  // value corresponds to B(epsilon_) in the comments describing
  // price_lower_bound_ above. Exceeding this amount of price decrease
  // in one iteration proves that there is some excess that cannot
  // reach a deficit, i.e., that the problem is infeasible.
  //
  // Let v be a node with excess and suppose P is a simple residual
  // path P from v to some node w with deficit such that reverse(P) is
  // residual at the beginning of this iteration (such a path is
  // guaranteed to exist by feasibility -- see lemma 5.7 in Goldberg
  // and Tarjan). We have c_p(P) = p(v) + c(P) - p(w) and of those
  // three terms, only p(v) may have changed during this iteration
  // because w has a deficit and nodes with deficits are not
  // relabeled. Assuming without loss of generality that p == 0 and
  // c_p == c at the beginning of this iteration, we seek a bound on
  // simply
  //   p(v) = c_p(P) - c(P).
  // Let arc a lie on P.
  // Case 1: a is a forward arc. Then c_p(a) >= 0 and the reverse of a
  //         was residual when this iteration began. By
  //         approximate optimality at the end of the prior iteration,
  //         c(a) < alpha * epsilon. So
  //           c_p(a) - c(a) > -alpha * epsilon_.
  // Case 2: a is a reverse arc. Then c_p(a) >= -epsilon_ and the
  //         reverse of a was residual when this iteration began. By
  //         approximate optimality at the end of the prior iteration,
  //         c(a) < 0. So
  //           c_p(a) - c(a) > -epsilon_.
  // Nodes with excess are only on the left and nodes with deficit are
  // only on the right; there are at most n - 1 arcs on the path P,
  // making up at most (n-1)/2 left-right-left arc pairs. Each
  // pair's contribution to c_p(P) - c(P) is bounded below by
  // most (n-1)/2 of those are forward arcs and (n-2)/2 of them are reverse
  // arcs, so
  //   p(v) = c_p(P) - c(P)
  //        > (n-1)/2 * (-alpha * epsilon_ - epsilon_)
  //        = -(n-1)/2 * epsilon_ * (1 + alpha).
  // So we set
  //   price_reduction_bound_ = ceil((n-1)/2 * epsilon * (1 + alpha)).
  CostValue price_reduction_bound_;

  // Computes the value of price_reduction_bound_ for an iteration,
  // given the new value of epsilon_, on the assumption that the value
  // of epsilon_ for the previous iteration was no more than a factor
  // of alpha_ times the new value. Because the expression computed
  // here is used in at least one place where we want an additional
  // factor in the denominator, we take that factor as an argument.
  //
  // Avoids overflow in computing the bound.
  inline CostValue PriceChangeBound(CostValue extra_divisor,
                                    bool* in_range) const {
    const CostValue n = graph_.num_nodes();
    // We work in double-precision floating point to determine whether
    // we'll overflow the integral CostValue type's range of
    // representation. Switching between integer and double is a
    // rather expensive operation, but we do this only once per
    // scaling iteration, so we can afford it rather than resort to
    // complex and subtle tricks within the bounds of integer
    // arithmetic.
    //
    // To understand the values of numerator and denominator here, you
    // will want to read the comments above about price_lower_bound_
    // and price_reduction_bound_, and have a pencil handy. :-)
    const double numerator = (static_cast<double>(n - 1) *
                              static_cast<double>(epsilon_ * (1 + alpha_)));
    const double denominator = static_cast<double>(2 * extra_divisor);
    const double quotient = numerator / denominator;
    const double limit =
        static_cast<double>(std::numeric_limits<CostValue>::max());
    if (quotient > limit) {
      // Our integer computations could overflow.
      if (in_range != NULL) *in_range = false;
      return std::numeric_limits<CostValue>::max();
    } else {
      if (in_range != NULL) *in_range = true;
      return static_cast<CostValue>(quotient);
    }
  }

  // A scaled record of the largest arc-cost magnitude we've been
  // given during problem setup. This is used to set the initial value
  // of epsilon_, which in turn is used not only as the error
  // parameter but also to determine whether we risk arithmetic
  // overflow during the algorithm.
  CostValue largest_scaled_cost_magnitude_;

  // The total excess in the graph. Given our asymmetric definition of
  // epsilon-optimality and our use of the double-push operation, this
  // equals the number of unmatched left-side nodes.
  NodeIndex total_excess_;

  // Indexed by node index, the price_ values are maintained only for
  // right-side nodes.
  CostArray price_;

  // Indexed by left-side node index, the matched_arc_ array gives the
  // arc index of the arc matching any given left-side node, or
  // GraphType::kNilArc if the node is unmatched.
  ArcIndexArray matched_arc_;

  // Indexed by right-side node index, the matched_node_ array gives
  // the node index of the left-side node matching any given
  // right-side node, or GraphType::kNilNode if the right-side node is
  // unmatched.
  NodeIndexArray matched_node_;

  // The array of arc costs as given in the problem definition, except
  // that they are scaled up by the number of nodes in the graph so we
  // can use integer arithmetic throughout.
  CostArray scaled_arc_cost_;

  // The container of active nodes (i.e., unmatched nodes). This can
  // be switched easily between ActiveNodeStack and ActiveNodeQueue
  // for experimentation.
  scoped_ptr<ActiveNodeContainerInterface> active_nodes_;

  // Statistics giving the overall numbers of various operations the
  // algorithm performs.
  Stats total_stats_;

  // Statistics giving the numbers of various operations the algorithm
  // has performed in the current iteration.
  Stats iteration_stats_;

  DISALLOW_COPY_AND_ASSIGN(LinearSumAssignment);
};

// Implementation of out-of-line LinearSumAssignment template member
// functions.

template <typename GraphType>
const CostValue LinearSumAssignment<GraphType>::kMinEpsilon = 1;

template <typename GraphType>
LinearSumAssignment<GraphType>::LinearSumAssignment(
    const GraphType& graph, NodeIndex num_left_nodes)
    : graph_(graph),
      num_left_nodes_(num_left_nodes),
      success_(false),
      cost_scaling_factor_(1 + (graph.max_num_nodes() / 2)),
      alpha_(FLAGS_assignment_alpha),
      epsilon_(0),
      price_lower_bound_(0),
      price_reduction_bound_(0),
      largest_scaled_cost_magnitude_(0),
      total_excess_(0),
      price_(num_left_nodes + GraphType::kFirstNode,
             graph.max_end_node_index() - 1),
      matched_arc_(GraphType::kFirstNode, num_left_nodes - 1),
      matched_node_(num_left_nodes, graph.max_end_node_index() - 1),
      scaled_arc_cost_(GraphType::kFirstArc, graph.max_end_arc_index() - 1),
      active_nodes_(
          FLAGS_assignment_stack_order ?
          static_cast<ActiveNodeContainerInterface*>(new ActiveNodeStack()) :
          static_cast<ActiveNodeContainerInterface*>(new ActiveNodeQueue())) { }

template <typename GraphType>
void LinearSumAssignment<GraphType>::SetArcCost(ArcIndex arc, CostValue cost) {
  DCHECK(graph_.CheckArcValidity(arc));
  NodeIndex head = Head(arc);
  DCHECK_LE(num_left_nodes_, head);
  cost *= cost_scaling_factor_;
  const CostValue cost_magnitude = std::abs(cost);
  largest_scaled_cost_magnitude_ = std::max(largest_scaled_cost_magnitude_,
                                            cost_magnitude);
  scaled_arc_cost_.Set(arc, cost);
}

template <typename ArcIndexType>
class CostValueCycleHandler
    : public PermutationCycleHandler<ArcIndexType> {
 public:
  explicit CostValueCycleHandler(CostArray* cost)
      : temp_(0),
        cost_(cost) { }

  virtual void SetTempFromIndex(ArcIndexType source) {
    temp_ = cost_->Value(source);
  }

  virtual void SetIndexFromIndex(ArcIndexType source,
                                 ArcIndexType destination) const {
    cost_->Set(destination, cost_->Value(source));
  }

  virtual void SetIndexFromTemp(ArcIndexType destination) const {
    cost_->Set(destination, temp_);
  }

  virtual ~CostValueCycleHandler() { }

 private:
  CostValue temp_;

  CostArray* cost_;

  DISALLOW_COPY_AND_ASSIGN(CostValueCycleHandler);
};

// Logically this class should be defined inside OptimizeGraphLayout,
// but compilation fails if we do that because C++98 doesn't allow
// instantiation of member templates with function-scoped types as
// template parameters, which in turn is because those function-scoped
// types lack linkage.
template <typename GraphType> class ArcIndexOrderingByTailNode {
 public:
  explicit ArcIndexOrderingByTailNode(const GraphType& graph)
      : graph_(graph) { }

  // Says ArcIndex a is less than ArcIndex b if arc a's tail is less
  // than arc b's tail. If their tails are equal, orders according to
  // heads.
  bool operator()(ArcIndex a, ArcIndex b) const {
    return ((graph_.Tail(a) < graph_.Tail(b)) ||
            ((graph_.Tail(a) == graph_.Tail(b)) &&
             (graph_.Head(a) < graph_.Head(b))));
  }

 private:
  const GraphType& graph_;

  // Copy and assign are allowed; they have to be for STL to work
  // with this functor, although it seems like a bug for STL to be
  // written that way.
};

template <typename GraphType>
void LinearSumAssignment<GraphType>::OptimizeGraphLayout(GraphType* graph) {
  // The graph argument is only to give us a non-const-qualified
  // handle on the graph we already have. Any different graph is
  // nonsense.
  DCHECK_EQ(&graph_, graph);
  const ArcIndexOrderingByTailNode<GraphType> compare(graph_);
  CostValueCycleHandler<typename GraphType::ArcIndex>
      cycle_handler(&scaled_arc_cost_);
  TailArrayManager<GraphType> tail_array_manager(graph);
  tail_array_manager.BuildTailArrayFromAdjacencyListsIfForwardGraph();
  graph->GroupForwardArcsByFunctor(compare, &cycle_handler);
  tail_array_manager.ReleaseTailArrayIfForwardGraph();
}

template <typename GraphType>
bool LinearSumAssignment<GraphType>::UpdateEpsilon() {
  // There are some somewhat subtle issues around using integer
  // arithmetic to compute successive values of epsilon_, the error
  // parameter.
  //
  // First, the value of price_reduction_bound_ is chosen under the
  // assumption that it is truly an upper bound on the amount by which
  // a node's price can change during the current iteration. The value
  // of this bound in turn depends on the assumption that the flow
  // computed by the previous iteration was
  // (epsilon_ * alpha_)-optimal. If epsilon_ decreases by more than a
  // factor of alpha_ due to truncating arithmetic, that bound might
  // not hold, and the consequence is that BestArcAndGap could return
  // an overly cautious admissibility gap in the case where a
  // left-side node has only one incident arc. This is not a big deal
  // at all. At worst it could lead to a few extra relabelings.
  //
  // Second, it is not a problem currently, but in the future if we
  // use an arc-fixing heuristic, we cannot permit truncating integer
  // division to decrease epsilon_ by a factor greater than alpha_
  // because our bounds on price changes (and hence on the reduced
  // cost of an arc that might ever become admissible in the future)
  // depend on the ratio between the values of the error parameter at
  // successive iterations. One consequence will be that we might
  // occasionally do an extra iteration today in the interest of being
  // able to "price arcs out" (which we don't do today). Today we
  // simply use truncating integer division, but note that this will
  // have to change if we ever price arcs out.
  //
  // Since neither of those issues is a problem today, we simply use
  // truncating integer arithmetic. But future changes might
  // necessitate rounding epsilon upward in the division.
  epsilon_ = std::max(epsilon_ / alpha_, kMinEpsilon);
  VLOG(3) << "Updated: epsilon_ == " << epsilon_;
  price_reduction_bound_ = PriceChangeBound(1, NULL);
  DCHECK_GT(price_reduction_bound_, 0);
  // For today we always return true; in the future updating epsilon
  // in sophisticated ways could conceivably detect infeasibility.
  return true;
}

// For production code that checks whether a left-side node is active.
template <typename GraphType>
inline bool LinearSumAssignment<GraphType>::IsActive(
    NodeIndex left_node) const {
  DCHECK_LT(left_node, num_left_nodes_);
  return matched_arc_[left_node] == GraphType::kNilArc;
}

// Only for debugging. Separate from the production IsActive() method
// so that method can assert that its argument is a left-side node,
// while for debugging we need to be able to test any node.
template <typename GraphType>
inline bool LinearSumAssignment<GraphType>::IsActiveForDebugging(
    NodeIndex node) const {
  if (node < num_left_nodes_) {
    return IsActive(node);
  } else {
    return matched_node_[node] == GraphType::kNilNode;
  }
}

template <typename GraphType>
void LinearSumAssignment<GraphType>::InitializeActiveNodeContainer() {
  DCHECK(active_nodes_->Empty());
  for (BipartiteLeftNodeIterator node_it(graph_, num_left_nodes_);
       node_it.Ok();
       node_it.Next()) {
    const NodeIndex node = node_it.Index();
    if (IsActive(node)) {
      active_nodes_->Add(node);
    }
  }
}

// There exists a price function such that the admissible arcs at the
// beginning of an iteration are exactly the reverse arcs of all
// matching arcs. Saturating all admissible arcs with respect to that
// price function therefore means simply unmatching every matched
// node.
//
// In the future we will price out arcs, which will reduce the set of
// nodes we unmatch here. If a matching arc is priced out, we will not
// unmatch its endpoints since that element of the matching is
// guaranteed not to change.
template <typename GraphType>
void LinearSumAssignment<GraphType>::SaturateNegativeArcs() {
  total_excess_ = 0;
  for (BipartiteLeftNodeIterator node_it(graph_, num_left_nodes_);
       node_it.Ok();
       node_it.Next()) {
    const NodeIndex node = node_it.Index();
    if (IsActive(node)) {
      // This can happen in the first iteration when nothing is
      // matched yet.
      total_excess_ += 1;
    } else {
      // We're about to create a unit of excess by unmatching these nodes.
      total_excess_ += 1;
      const NodeIndex mate = GetMate(node);
      matched_arc_.Set(node, GraphType::kNilArc);
      matched_node_.Set(mate, GraphType::kNilNode);
    }
  }
}

// Returns true for success, false for infeasible.
template <typename GraphType>
bool LinearSumAssignment<GraphType>::DoublePush(NodeIndex source) {
  DCHECK_GT(num_left_nodes_, source);
  DCHECK(IsActive(source));
  ImplicitPriceSummary summary = BestArcAndGap(source);
  const ArcIndex best_arc = summary.first;
  const CostValue gap = summary.second;
  // Now we have the best arc incident to source, i.e., the one with
  // minimum reduced cost. Match that arc, unmatching its head if
  // necessary.
  if (best_arc == GraphType::kNilArc) {
    return false;
  }
  const NodeIndex new_mate = Head(best_arc);
  const NodeIndex to_unmatch = matched_node_[new_mate];
  if (to_unmatch != GraphType::kNilNode) {
    // Unmatch new_mate from its current mate, pushing the unit of
    // flow back to a node on the left side as a unit of excess.
    matched_arc_.Set(to_unmatch, GraphType::kNilArc);
    active_nodes_->Add(to_unmatch);
    // This counts as a double push.
    iteration_stats_.double_pushes_ += 1;
  } else {
    // We are about to increase the cardinality of the matching.
    total_excess_ -= 1;
    // This counts as a single push.
    iteration_stats_.pushes_ += 1;
  }
  matched_arc_.Set(source, best_arc);
  matched_node_.Set(new_mate, source);
  // Finally, relabel new_mate.
  iteration_stats_.relabelings_ += 1;
  price_.Set(new_mate, price_[new_mate] - gap - epsilon_);
  return price_[new_mate] >= price_lower_bound_;
}

template <typename GraphType>
bool LinearSumAssignment<GraphType>::Refine() {
  SaturateNegativeArcs();
  InitializeActiveNodeContainer();
  while (total_excess_ > 0) {
    // Get an active node (i.e., one with excess == 1) and discharge
    // it using DoublePush.
    const NodeIndex node = active_nodes_->Get();
    if (!DoublePush(node)) {
      // Infeasibility detected.
      return false;
    }
  }
  DCHECK(active_nodes_->Empty());
  iteration_stats_.refinements_ += 1;
  return true;
}

// Computes best_arc, the minimum reduced-cost arc incident to
// left_node and admissibility_gap, the amount by which the reduced
// cost of best_arc must be increased to make it equal in reduced cost
// to another residual arc incident to left_node.
//
// Precondition: left_node is unmatched. This allows us to simplify
// the code. The debug-only counterpart to this routine is
// LinearSumAssignment::ImplicitPrice() and it does not assume this
// precondition.
//
// This function is large enough that our suggestion that the compiler
// inline it might be pointless.
template <typename GraphType>
inline typename LinearSumAssignment<GraphType>::ImplicitPriceSummary
LinearSumAssignment<GraphType>::BestArcAndGap(NodeIndex left_node) const {
    DCHECK(IsActive(left_node));
  DCHECK_GT(epsilon_, 0);
  // During any scaling iteration, the price of an active node
  // decreases by at most price_reduction_bound_ and all left-side
  // nodes are made active at the beginning of Refine(), so the bound
  // applies to all left-side nodes.
  typename GraphType::OutgoingArcIterator arc_it(graph_, left_node);
  ArcIndex best_arc = arc_it.Index();
  CostValue min_partial_reduced_cost = PartialReducedCost(best_arc);
  // We choose second_min_partial_reduced_cost so that in the case of
  // the largest possible gap (which results from a left-side node
  // with only a single incident residual arc), the corresponding
  // right-side node will be relabeled by an amount that exactly
  // matches price_reduction_bound_. The overall price_lower_bound_ is
  // computed tightly enough that if we relabel by an amount even
  // epsilon_ greater than that, we can incorrectly conclude
  // infeasibility in DoublePush().
  CostValue second_min_partial_reduced_cost =
      min_partial_reduced_cost + price_reduction_bound_ - epsilon_;
  for (arc_it.Next(); arc_it.Ok(); arc_it.Next()) {
    const ArcIndex arc = arc_it.Index();
    const CostValue partial_reduced_cost = PartialReducedCost(arc);
    if (partial_reduced_cost < second_min_partial_reduced_cost) {
      if (partial_reduced_cost < min_partial_reduced_cost) {
        best_arc = arc;
        second_min_partial_reduced_cost = min_partial_reduced_cost;
        min_partial_reduced_cost = partial_reduced_cost;
      } else {
        second_min_partial_reduced_cost = partial_reduced_cost;
      }
    }
  }
  const CostValue gap =
      second_min_partial_reduced_cost - min_partial_reduced_cost;
  DCHECK_GE(gap, 0);
  return std::make_pair(best_arc, gap);
}

// Only for debugging.
template <typename GraphType> inline CostValue
LinearSumAssignment<GraphType>::ImplicitPrice(NodeIndex left_node) const {
  DCHECK_GT(num_left_nodes_, left_node);
  DCHECK_GT(epsilon_, 0);
  typename GraphType::OutgoingArcIterator arc_it(graph_, left_node);
  // If the input problem is feasible, it is always the case that
  // arc_it.Ok(), i.e., that there is at least one arc incident to
  // left_node.
  DCHECK(arc_it.Ok());
  ArcIndex best_arc = arc_it.Index();
  if (best_arc == matched_arc_[left_node]) {
    arc_it.Next();
    if (arc_it.Ok()) {
      best_arc = arc_it.Index();
    }
  }
  CostValue min_partial_reduced_cost = PartialReducedCost(best_arc);
  if (!arc_it.Ok()) {
    // Only one arc is incident to left_node, and the node is
    // currently matched along that arc, which must be the case in any
    // feasible solution. Therefore we implicitly price this node so
    // low that we will never consider unmatching it.
    return -(min_partial_reduced_cost + price_reduction_bound_);
  }
  for (arc_it.Next(); arc_it.Ok(); arc_it.Next()) {
    const ArcIndex arc = arc_it.Index();
    if (arc != matched_arc_[left_node]) {
      const CostValue partial_reduced_cost = PartialReducedCost(arc);
      if (partial_reduced_cost < min_partial_reduced_cost) {
        min_partial_reduced_cost = partial_reduced_cost;
      }
    }
  }
  return -min_partial_reduced_cost;
}

// Only for debugging.
template <typename GraphType>
bool LinearSumAssignment<GraphType>::AllMatched() const {
  for (typename GraphType::NodeIterator node_it(graph_);
       node_it.Ok();
       node_it.Next()) {
    if (IsActiveForDebugging(node_it.Index())) {
      return false;
    }
  }
  return true;
}

// Only for debugging.
template <typename GraphType>
bool LinearSumAssignment<GraphType>::EpsilonOptimal() const {
  for (BipartiteLeftNodeIterator node_it(graph_, num_left_nodes_);
       node_it.Ok();
       node_it.Next()) {
    const NodeIndex left_node = node_it.Index();
    // Get the implicit price of left_node and make sure the reduced
    // costs of left_node's incident arcs are in bounds.
    CostValue left_node_price = ImplicitPrice(left_node);
    for (typename GraphType::OutgoingArcIterator arc_it(graph_, left_node);
         arc_it.Ok();
         arc_it.Next()) {
      const ArcIndex arc = arc_it.Index();
      const CostValue reduced_cost =
          left_node_price + PartialReducedCost(arc);
      // Note the asymmetric definition of epsilon-optimality that we
      // use because it means we can saturate all admissible arcs in
      // the beginning of Refine() just by unmatching all matched
      // nodes.
      if (matched_arc_[left_node] == arc) {
        // The reverse arc is residual. Epsilon-optimality requires
        // that the reduced cost of the forward arc be at most
        // epsilon_.
        if (reduced_cost > epsilon_) {
          return false;
        }
      } else {
        // The forward arc is residual. Epsilon-optimality requires
        // that the reduced cost of the forward arc be at least zero.
        if (reduced_cost < 0) {
          return false;
        }
      }
    }
  }
  return true;
}

template <typename GraphType>
bool LinearSumAssignment<GraphType>::FinalizeSetup() {
  epsilon_ = largest_scaled_cost_magnitude_;
  VLOG(2) << "Largest given cost magnitude: " <<
      largest_scaled_cost_magnitude_ / cost_scaling_factor_;
  // Initialize left-side node-indexed arrays.
  typename GraphType::NodeIterator node_it(graph_);
  for (; node_it.Ok(); node_it.Next()) {
    const NodeIndex node = node_it.Index();
    if (node >= num_left_nodes_) {
      break;
    }
    matched_arc_.Set(node, GraphType::kNilArc);
  }
  // Initialize right-side node-indexed arrays. Example: prices are
  // stored only for right-side nodes.
  for (; node_it.Ok(); node_it.Next()) {
    const NodeIndex node = node_it.Index();
    price_.Set(node, 0);
    matched_node_.Set(node, GraphType::kNilNode);
  }
  bool in_range;
  price_lower_bound_ = -PriceChangeBound(alpha_ - 1, &in_range);
  DCHECK_LE(price_lower_bound_, 0);
  if (!in_range) {
    LOG(WARNING) << "Price change bound exceeds range of representable "
                 << "costs; arithmetic overflow is not ruled out.";
  }
  return in_range;
}

template <typename GraphType>
void LinearSumAssignment<GraphType>::ReportAndAccumulateStats() {
  total_stats_.Add(iteration_stats_);
  VLOG(3) << "Iteration stats: " << iteration_stats_.StatsString();
  iteration_stats_.Clear();
}

template <typename GraphType>
bool LinearSumAssignment<GraphType>::ComputeAssignment() {
  // Note: FinalizeSetup() might have been called already by white-box
  // test code or by a client that wants to react to the possibility
  // of overflow before solving the given problem, but FinalizeSetup()
  // is idempotent and reasonably fast, so we call it unconditionally
  // here.
  FinalizeSetup();
  bool ok = graph_.num_nodes() == 2 * num_left_nodes_;
  DCHECK(!ok || EpsilonOptimal());
  while (ok && epsilon_ > kMinEpsilon) {
    ok &= UpdateEpsilon();
    ok &= Refine();
    ReportAndAccumulateStats();
    DCHECK(!ok || EpsilonOptimal());
    DCHECK(!ok || AllMatched());
  }
  success_ = ok;
  VLOG(1) << "Overall stats: " << total_stats_.StatsString();
  return ok;
}

template <typename GraphType>
CostValue LinearSumAssignment<GraphType>::GetCost() const {
  // It is illegal to call this method unless we successfully computed
  // an optimum assignment.
  DCHECK(success_);
  CostValue cost = 0;
  for (BipartiteLeftNodeIterator node_it(*this);
       node_it.Ok();
       node_it.Next()) {
    cost += GetAssignmentCost(node_it.Index());
  }
  return cost;
}

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_LINEAR_ASSIGNMENT_H_
