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
// SetArcCost function.
//
// Example usage:
//
// #include "graph/assignment.h"
// #include "graph/ebert_graph.h"
// ...
// ::operations_research::NodeIndex num_nodes = ...;
// ::operations_research::NodeIndex num_left_nodes = num_nodes / 2;
// // Define a num_nodes/2 by num_nodes/2 assignment problem:
// ::operations_research::ArcIndex num_forward_arcs = ...;
// ::operations_research::StarGraph g(num_nodes, num_arcs);
// ::operations_research::LinearSumAssignment a(g, num_left_nodes);
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
//         node_it;
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
// TODO(user): What changes are needed to open-source this code?
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

#ifndef OR_TOOLS_GRAPH_ASSIGNMENT_H_
#define OR_TOOLS_GRAPH_ASSIGNMENT_H_

#include <deque>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "graph/ebert_graph.h"
#include "util/packed_array.h"

using std::string;

namespace operations_research {

class LinearSumAssignment {
 public:

  // This class modifies the given graph by adding arcs to it as costs
  // are specified via SetArcCost, but does not take ownership.
  LinearSumAssignment(const StarGraph& graph,
                      NodeIndex num_left_nodes);
  virtual ~LinearSumAssignment() {}

  // Sets the cost-scaling divisor, i.e., the amount by which we
  // divide the scaling parameter on each iteration.
  void SetCostScalingDivisor(CostValue factor) {
    alpha_ = factor;
  }

  // Optimizes the layout of the graph for the access pattern our
  // implementation will use.
  void OptimizeGraphLayout(StarGraph *graph);

  // Allows tests, iterators, etc., to inspect our underlying graph.
  inline const StarGraph& Graph() const {
    return graph_;
  }

  // These handy member functions make the code more compact, and we
  // expose them to clients so that client code that doesn't have
  // direct access to the graph can learn about the optimum assignment
  // once it is computed.
  inline NodeIndex Head(ArcIndex arc) const {
    return graph_.Head(arc);
  }

  inline NodeIndex Tail(ArcIndex arc) const {
    return graph_.Tail(arc);
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
  inline ArcIndex GetAssignmentArc(NodeIndex node) const {
    return matched_[node];
  }

  // Returns the cost of the assignment arc incident to the given
  // node.
  inline CostValue GetAssignmentCost(NodeIndex node) const {
    return ArcCost(GetAssignmentArc(node));
  }

  // Returns the node to which the given node is matched.
  inline NodeIndex GetMate(NodeIndex left_node) const {
    DCHECK_LE(left_node, num_left_nodes_);
    ArcIndex matching_arc = GetAssignmentArc(left_node);
    DCHECK_NE(StarGraph::kNilArc, matching_arc);
    return Head(matching_arc);
  }

  string StatsString() const {
    return total_stats_.StatsString();
  }

  // TODO(user): This should probably be derived from a common base
  // class of the node iterators in ebert_graph.h instead of the
  // pseudo-duck-typing we do today.
  class BipartiteLeftNodeIterator {
   public:
    BipartiteLeftNodeIterator(const StarGraph& graph,
                              NodeIndex num_left_nodes)
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
    StarGraph::NodeIterator node_iterator_;
  };

 private:
  // TODO(user): Share the following structure among all the
  // preflow-push flow algorithms.
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

  // TODO(user): Make this interface and its two implementations
  // below templates parameterized so they can handle both nodes and
  // arcs, and share it among all the preflow-push flow algorithms.
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

  // Indicates whether the given node has positive excess. Called only
  // for nodes on the left side.
  inline bool IsActive(NodeIndex node) const;

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
    DCHECK(graph_.IsDirect(arc));
    return scaled_arc_cost_[arc] - price_[Head(arc)];
  }

  // The graph underlying the problem definition we are given. Not
  // const because we add arcs to the graph via our SetArcCost()
  // method.
  const StarGraph& graph_;

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
                                    bool *in_range) const {
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
  // right-side nodes. These are kept as int64 values instead of
  // CostValues because we scale the initial arc costs up by the
  // number of nodes in order to use integer arithmetic everywhere,
  // and such scaling up increases the risk of overflow.
  Int64PackedArray price_;

  // Indexed by node index, the matched_ array gives the arc index of
  // the arc matching any given node, or StarGraph::kNilArc if the
  // node is unmatched.
  ArcIndexArray matched_;

  // The array of arc costs as given in the problem definition, except
  // that they are scaled up by the number of nodes in the graph so we
  // can use integer arithmetic throughout. Consequently we make this
  // a packed array of int64 values just to stave off overflow that
  // little extra bit.
  Int64PackedArray scaled_arc_cost_;

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

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_ASSIGNMENT_H_
