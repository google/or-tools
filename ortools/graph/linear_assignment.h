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

//
// An implementation of a cost-scaling push-relabel algorithm for the
// assignment problem (minimum-cost perfect bipartite matching), from
// the paper of Goldberg and Kennedy (1995).
//
//
// This implementation finds the minimum-cost perfect assignment in
// the given graph with integral edge weights set through the
// SetArcCost method.
//
// The running time is O(n*m*log(nC)) where n is the number of nodes,
// m is the number of edges, and C is the largest magnitude of an edge cost.
// In principle it can be worse than the Hungarian algorithm but we don't know
// of any class of problems where that actually happens. An additional sqrt(n)
// factor could be shaved off the running time bound using the technique
// described in http://dx.doi.org/10.1137/S0895480194281185
// (see also http://theory.stanford.edu/~robert/papers/glob_upd.ps).
//
//
// Example usage:
//
//   #include "ortools/graph/graph.h"
//   #include "ortools/graph/linear_assignment.h"
//
//   // Choose a graph implementation (we recommend StaticGraph<>).
//   typedef util::StaticGraph<> Graph;
//
//   // Define a num_nodes / 2 by num_nodes / 2 assignment problem:
//   const int num_nodes = ...
//   const int num_arcs = ...
//   const int num_left_nodes = num_nodes / 2;
//   Graph graph(num_nodes, num_arcs);
//   std::vector<operations_research::CostValue> arc_costs(num_arcs);
//   for (int arc = 0; arc < num_arcs; ++arc) {
//     const int arc_tail = ...   // must be in [0, num_left_nodes)
//     const int arc_head = ...   // must be in [num_left_nodes, num_nodes)
//     graph.AddArc(arc_tail, arc_head);
//     arc_costs[arc] = ...
//   }
//
//   // Build the StaticGraph. You can skip this step by using a ListGraph<>
//   // instead, but then the ComputeAssignment() below will be slower. It is
//   // okay if your graph is small and performance is not critical though.
//   {
//     std::vector<Graph::ArcIndex> arc_permutation;
//     graph.Build(&arc_permutation);
//     util::Permute(arc_permutation, &arc_costs);
//   }
//
//   // Construct the LinearSumAssignment.
//   ::operations_research::LinearSumAssignment<Graph> a(graph, num_left_nodes);
//   for (int arc = 0; arc < num_arcs; ++arc) {
//      // You can also replace 'arc_costs[arc]' by something like
//      //   ComputeArcCost(permutation.empty() ? arc : permutation[arc])
//      // if you don't want to store the costs in arc_costs to save memory.
//      a.SetArcCost(arc, arc_costs[arc]);
//   }
//
//   // Compute the optimum assignment.
//   bool success = a.ComputeAssignment();
//   // Retrieve the cost of the optimum assignment.
//   operations_research::CostValue optimum_cost = a.GetCost();
//   // Retrieve the node-node correspondence of the optimum assignment and the
//   // cost of each node pairing.
//   for (int left_node = 0; left_node < num_left_nodes; ++left_node) {
//     const int right_node = a.GetMate(left_node);
//     operations_research::CostValue node_pair_cost =
//         a.GetAssignmentCost(left_node);
//     ...
//   }
//
// In the following, we consider a bipartite graph
//   G = (V = X union Y, E subset XxY),
// where V denotes the set of nodes (vertices) in the graph, E denotes
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
// see google3/ortools/graph/min_cost_flow.h
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
//     a right-to-left residual arc, the reduced cost must be at
//     most -epsilon/2. The careful reader will note that these thresholds
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
// ISBN: 978-0136175490, http://www.amazon.com/dp/013617549X.
//
// Keywords: linear sum assignment problem, Hungarian method, Goldberg, Kennedy.

#ifndef OR_TOOLS_GRAPH_LINEAR_ASSIGNMENT_H_
#define OR_TOOLS_GRAPH_LINEAR_ASSIGNMENT_H_

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/graph/ebert_graph.h"
#include "ortools/util/permutation.h"
#include "ortools/util/zvector.h"

#ifndef SWIG
DECLARE_int64(assignment_alpha);
DECLARE_int32(assignment_progress_logging_period);
DECLARE_bool(assignment_stack_order);
#endif

namespace operations_research {

// This class does not take ownership of its underlying graph.
template <typename GraphType>
class LinearSumAssignment {
 public:
  typedef typename GraphType::NodeIndex NodeIndex;
  typedef typename GraphType::ArcIndex ArcIndex;

  // Constructor for the case in which we will build the graph
  // incrementally as we discover arc costs, as might be done with any
  // of the dynamic graph representations such as StarGraph or ForwardStarGraph.
  LinearSumAssignment(const GraphType& graph, NodeIndex num_left_nodes);

  // Constructor for the case in which the underlying graph cannot be
  // built until after all the arc costs are known, as is the case
  // with ForwardStarStaticGraph. In this case, the graph is passed to
  // us later via the SetGraph() method, below.
  LinearSumAssignment(NodeIndex num_left_nodes, ArcIndex num_arcs);

  ~LinearSumAssignment() {}

  // Sets the graph used by the LinearSumAssignment instance, for use
  // when the graph layout can be determined only after arc costs are
  // set. This happens, for example, when we use a ForwardStarStaticGraph.
  void SetGraph(const GraphType* graph) {
    DCHECK(graph_ == nullptr);
    graph_ = graph;
  }

  // Sets the cost-scaling divisor, i.e., the amount by which we
  // divide the scaling parameter on each iteration.
  void SetCostScalingDivisor(CostValue factor) { alpha_ = factor; }

  // Returns a permutation cycle handler that can be passed to the
  // TransformToForwardStaticGraph method so that arc costs get
  // permuted along with arcs themselves.
  //
  // Passes ownership of the cycle handler to the caller.
  //
  operations_research::PermutationCycleHandler<typename GraphType::ArcIndex>*
  ArcAnnotationCycleHandler();

  // Optimizes the layout of the graph for the access pattern our
  // implementation will use.
  //
  // REQUIRES for LinearSumAssignment template instantiation if a call
  // to the OptimizeGraphLayout() method is compiled: GraphType is a
  // dynamic graph, i.e., one that implements the
  // GroupForwardArcsByFunctor() member template method.
  //
  // If analogous optimization is needed for LinearSumAssignment
  // instances based on static graphs, the graph layout should be
  // constructed such that each node's outgoing arcs are sorted by
  // head node index before the
  // LinearSumAssignment<GraphType>::SetGraph() method is called.
  void OptimizeGraphLayout(GraphType* graph);

  // Allows tests, iterators, etc., to inspect our underlying graph.
  inline const GraphType& Graph() const { return *graph_; }

  // These handy member functions make the code more compact, and we
  // expose them to clients so that client code that doesn't have
  // direct access to the graph can learn about the optimum assignment
  // once it is computed.
  inline NodeIndex Head(ArcIndex arc) const { return graph_->Head(arc); }

  // Returns the original arc cost for use by a client that's
  // iterating over the optimum assignment.
  CostValue ArcCost(ArcIndex arc) const {
    DCHECK_EQ(0, scaled_arc_cost_[arc] % cost_scaling_factor_);
    return scaled_arc_cost_[arc] / cost_scaling_factor_;
  }

  // Sets the cost of an arc already present in the given graph.
  void SetArcCost(ArcIndex arc, CostValue cost);

  // Completes initialization after the problem is fully specified.
  // Returns true if we successfully prove that arithmetic
  // calculations are guaranteed not to overflow. ComputeAssignment()
  // calls this method itself, so only clients that care about
  // obtaining a warning about the possibility of arithmetic precision
  // problems need to call this method explicitly.
  //
  // Separate from ComputeAssignment() for white-box testing and for
  // clients that need to react to the possibility that arithmetic
  // overflow is not ruled out.
  //
  // FinalizeSetup() is idempotent.
  bool FinalizeSetup();

  // Computes the optimum assignment. Returns true on success. Return
  // value of false implies the given problem is infeasible.
  bool ComputeAssignment();

  // Returns the cost of the minimum-cost perfect matching.
  // Precondition: success_ == true, signifying that we computed the
  // optimum assignment for a feasible problem.
  CostValue GetCost() const;

  // Returns the total number of nodes in the given problem.
  NodeIndex NumNodes() const {
    if (graph_ == nullptr) {
      // Return a guess that must be true if ultimately we are given a
      // feasible problem to solve.
      return 2 * NumLeftNodes();
    } else {
      return graph_->num_nodes();
    }
  }

  // Returns the number of nodes on the left side of the given
  // problem.
  NodeIndex NumLeftNodes() const { return num_left_nodes_; }

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

  std::string StatsString() const { return total_stats_.StatsString(); }

  class BipartiteLeftNodeIterator {
   public:
    BipartiteLeftNodeIterator(const GraphType& graph, NodeIndex num_left_nodes)
        : num_left_nodes_(num_left_nodes), node_iterator_(0) {}

    explicit BipartiteLeftNodeIterator(const LinearSumAssignment& assignment)
        : num_left_nodes_(assignment.NumLeftNodes()), node_iterator_(0) {}

    NodeIndex Index() const { return node_iterator_; }

    bool Ok() const { return node_iterator_ < num_left_nodes_; }

    void Next() { ++node_iterator_; }

   private:
    const NodeIndex num_left_nodes_;
    typename GraphType::NodeIndex node_iterator_;
  };

 private:
  struct Stats {
    Stats() : pushes_(0), double_pushes_(0), relabelings_(0), refinements_(0) {}
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
    std::string StatsString() const {
      return absl::StrFormat(
          "%d refinements; %d relabelings; "
          "%d double pushes; %d pushes",
          refinements_, relabelings_, double_pushes_, pushes_);
    }
    int64 pushes_;
    int64 double_pushes_;
    int64 relabelings_;
    int64 refinements_;
  };

#ifndef SWIG
  class ActiveNodeContainerInterface {
   public:
    virtual ~ActiveNodeContainerInterface() {}
    virtual bool Empty() const = 0;
    virtual void Add(NodeIndex node) = 0;
    virtual NodeIndex Get() = 0;
  };

  class ActiveNodeStack : public ActiveNodeContainerInterface {
   public:
    ~ActiveNodeStack() override {}

    bool Empty() const override { return v_.empty(); }

    void Add(NodeIndex node) override { v_.push_back(node); }

    NodeIndex Get() override {
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
    ~ActiveNodeQueue() override {}

    bool Empty() const override { return q_.empty(); }

    void Add(NodeIndex node) override { q_.push_front(node); }

    NodeIndex Get() override {
      DCHECK(!Empty());
      NodeIndex result = q_.back();
      q_.pop_back();
      return result;
    }

   private:
    std::deque<NodeIndex> q_;
  };
#endif

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

  // For use by DoublePush()
  inline ImplicitPriceSummary BestArcAndGap(NodeIndex left_node) const;

  // Accumulates stats between iterations and reports them if the
  // verbosity level is high enough.
  void ReportAndAccumulateStats();

  // Utility function to compute the next error parameter value. This
  // is used to ensure that the same sequence of error parameter
  // values is used for computation of price bounds as is used for
  // computing the optimum assignment.
  CostValue NewEpsilon(CostValue current_epsilon) const;

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
  // owned by *this.
  const GraphType* graph_;

  // The number of nodes on the left side of the graph we are given.
  NodeIndex num_left_nodes_;

  // A flag indicating, after FinalizeSetup() has run, whether the
  // arc-incidence precondition required by BestArcAndGap() is
  // satisfied by every left-side node. If not, the problem is
  // infeasible.
  bool incidence_precondition_satisfied_;

  // A flag indicating that an optimal perfect matching has been computed.
  bool success_;

  // The value by which we multiply all the arc costs we are given in
  // order to be able to use integer arithmetic in all our
  // computations. In order to establish optimality of the final
  // matching we compute, we need that
  //   (cost_scaling_factor_ / kMinEpsilon) > graph_->num_nodes().
  const CostValue cost_scaling_factor_;

  // Scaling divisor.
  CostValue alpha_;

  // Minimum value of epsilon. When a flow is epsilon-optimal for
  // epsilon == kMinEpsilon, the flow is optimal.
  static const CostValue kMinEpsilon;

  // Current value of epsilon, the cost scaling parameter.
  CostValue epsilon_;

  // The following two data members, price_lower_bound_ and
  // slack_relabeling_price_, have to do with bounds on the amount by
  // which node prices can change during execution of the algorithm.
  // We need some detailed discussion of this topic because we violate
  // several simplifying assumptions typically made in the theoretical
  // literature. In particular, we use integer arithmetic, we use a
  // reduction to the transportation problem rather than min-cost
  // circulation, we provide detection of infeasible problems rather
  // than assume feasibility, we detect when our computations might
  // exceed the range of representable cost values, and we use the
  // double-push heuristic which relabels nodes that do not have
  // excess.
  //
  // In the following discussion, we prove the following propositions:
  // Proposition 1. [Fidelity of arithmetic precision guarantee] If
  //                FinalizeSetup() returns true, no arithmetic
  //                overflow occurs during ComputeAssignment().
  // Proposition 2. [Fidelity of feasibility detection] If no
  //                arithmetic overflow occurs during
  //                ComputeAssignment(), the return value of
  //                ComputeAssignment() faithfully indicates whether
  //                the given problem is feasible.
  //
  // We begin with some general discussion.
  //
  // The ideas used to prove our two propositions are essentially
  // those that appear in [Goldberg and Tarjan], but several details
  // are different: [Goldberg and Tarjan] assumes a feasible problem,
  // uses a symmetric notion of epsilon-optimality, considers only
  // nodes with excess eligible for relabeling, and does not treat the
  // question of arithmetic overflow. This implementation, on the
  // other hand, detects and reports infeasible problems, uses
  // asymmetric epsilon-optimality, relabels nodes with no excess in
  // the course of the double-push operation, and gives a reasonably
  // tight guarantee of arithmetic precision. No fundamentally new
  // ideas are involved, but the details are a bit tricky so they are
  // explained here.
  //
  // We have two intertwined needs that lead us to compute bounds on
  // the prices nodes can have during the assignment computation, on
  // the assumption that the given problem is feasible:
  // 1. Infeasibility detection: Infeasibility is detected by
  //    observing that some node's price has been reduced too much by
  //    relabeling operations (see [Goldberg and Tarjan] for the
  //    argument -- duplicated in modified form below -- bounding the
  //    running time of the push/relabel min-cost flow algorithm for
  //    feasible problems); and
  // 2. Aggressively relabeling nodes and arcs whose matching is
  //    forced: When a left-side node is incident to only one arc a,
  //    any feasible solution must include a, and reducing the price
  //    of Head(a) by any nonnegative amount preserves epsilon-
  //    optimality. Because of this freedom, we'll call this sort of
  //    relabeling (i.e., a relabeling of a right-side node that is
  //    the only neighbor of the left-side node to which it has been
  //    matched in the present double-push operation) a "slack"
  //    relabeling. Relabelings that are not slack relabelings are
  //    called "confined" relabelings. By relabeling Head(a) to have
  //    p(Head(a))=-infinity, we could guarantee that a never becomes
  //    unmatched during the current iteration, and this would prevent
  //    our wasting time repeatedly unmatching and rematching a. But
  //    there are some details we need to handle:
  //    a. The CostValue type cannot represent -infinity;
  //    b. Low node prices are precisely the signal we use to detect
  //       infeasibility (see (1)), so we must be careful not to
  //       falsely conclude that the problem is infeasible as a result
  //       of the low price we gave Head(a); and
  //    c. We need to indicate accurately to the client when our best
  //       understanding indicates that we can't rule out arithmetic
  //       overflow in our calculations. Most importantly, if we don't
  //       warn the client, we must be certain to avoid overflow. This
  //       means our slack relabelings must not be so aggressive as to
  //       create the possibility of unforeseen overflow. Although we
  //       will not achieve this in practice, slack relabelings would
  //       ideally not introduce overflow unless overflow was
  //       inevitable were even the smallest reasonable price change
  //       (== epsilon) used for slack relabelings.
  //    Using the analysis below, we choose a finite amount of price
  //    change for slack relabelings aggressive enough that we don't
  //    waste time doing repeated slack relabelings in a single
  //    iteration, yet modest enough that we keep a good handle on
  //    arithmetic precision and our ability to detect infeasible
  //    problems.
  //
  // To provide faithful detection of infeasibility, a dependable
  // guarantee of arithmetic precision whenever possible, and good
  // performance by aggressively relabeling nodes whose matching is
  // forced, we exploit these facts:
  // 1. Beyond the first iteration, infeasibility detection isn't needed
  //    because a problem is feasible in some iteration if and only if
  //    it's feasible in all others. Therefore we are free to use an
  //    infeasibility detection mechanism that might work in just one
  //    iteration and switch it off in all other iterations.
  // 2. When we do a slack relabeling, we must choose the amount of
  //    price reduction to use. We choose an amount large enough to
  //    guarantee putting the node's matching to rest, yet (although
  //    we don't bother to prove this explicitly) small enough that
  //    the node's price obeys the overall lower bound that holds if
  //    the slack relabeling amount is small.
  //
  // We will establish Propositions (1) and (2) above according to the
  // following steps:
  // First, we prove Lemma 1, which is a modified form of lemma 5.8 of
  // [Goldberg and Tarjan] giving a bound on the difference in price
  // between the end nodes of certain paths in the residual graph.
  // Second, we prove Lemma 2, which is technical lemma to establish
  // reachability of certain "anchor" nodes in the residual graph from
  // any node where a relabeling takes place.
  // Third, we apply the first two lemmas to prove Lemma 3 and Lemma
  // 4, which give two similar bounds that hold whenever the given
  // problem is feasible: (for feasibility detection) a bound on the
  // price of any node we relabel during any iteration (and the first
  // iteration in particular), and (for arithmetic precision) a bound
  // on the price of any node we relabel during the entire algorithm.
  //
  // Finally, we note that if the whole-algorithm price bound can be
  // represented precisely by the CostValue type, arithmetic overflow
  // cannot occur (establishing Proposition 1), and assuming no
  // overflow occurs during the first iteration, any violation of the
  // first-iteration price bound establishes infeasibility
  // (Proposition 2).
  //
  // The statement of Lemma 1 is perhaps easier to understand when the
  // reader knows how it will be used. To wit: In this lemma, f' and
  // e_0 are the flow and error parameter (epsilon) at the beginning
  // of the current iteration, while f and e_1 are the current
  // pseudoflow and error parameter when a relabeling of interest
  // occurs. Without loss of generality, c is the reduced cost
  // function at the beginning of the current iteration and p is the
  // change in prices that has taken place in the current iteration.
  //
  // Lemma 1 (a variant of lemma 5.8 from [Goldberg and Tarjan]): Let
  // f be a pseudoflow and let f' be a flow. Suppose P is a simple
  // path from right-side node v to right-side node w such that P is
  // residual with respect to f and reverse(P) is residual with
  // respect to f'. Further, suppose c is an arc cost function with
  // respect to which f' is e_0-optimal with the zero price function
  // and p is a price function with respect to which f is e_1-optimal
  // with respect to p. Then
  //   p(v) - p(w) >= -(e_0 + e_1) * (n-2)/2.     (***)
  //
  // Proof: We have c_p(P) = p(v) + c(P) - p(w) and hence
  //   p(v) - p(w) = c_p(P) - c(P).
  // So we seek a bound on c_p(P) - c(P).
  //   p(v) = c_p(P) - c(P).
  // Let arc a lie on P, which implies that a is residual with respect
  // to f and reverse(a) is residual with respect to f'.
  // Case 1: a is a forward arc. Then by e_1-optimality of f with
  //         respect to p, c_p(a) >= 0 and reverse(a) is residual with
  //         respect to f'. By e_0-optimality of f', c(a) <= e_0. So
  //           c_p(a) - c(a) >= -e_0.
  // Case 2: a is a reverse arc. Then by e_1-optimality of f with
  //         respect to p, c_p(a) >= -e_1 and reverse(a) is residual
  //         with respect to f'. By e_0-optimality of f', c(a) <= 0.
  //         So
  //           c_p(a) - c(a) >= -e_1.
  // We assumed v and w are both right-side nodes, so there are at
  // most n - 2 arcs on the path P, of which at most (n-2)/2 are
  // forward arcs and at most (n-2)/2 are reverse arcs, so
  //   p(v) - p(w) = c_p(P) - c(P)
  //               >= -(e_0 + e_1) * (n-2)/2.     (***)
  //
  // Some of the rest of our argument is given as a sketch, omitting
  // several details. Also elided here are some minor technical issues
  // related to the first iteration, inasmuch as our arguments assume
  // on the surface a "previous iteration" that doesn't exist in that
  // case. The issues are not substantial, just a bit messy.
  //
  // Lemma 2 is analogous to lemma 5.7 of [Goldberg and Tarjan], where
  // they have only relabelings that take place at nodes with excess
  // while we have only relabelings that take place as part of the
  // double-push operation at nodes without excess.
  //
  // Lemma 2: If the problem is feasible, for any node v with excess,
  // there exists a path P from v to a node w with deficit such that P
  // is residual with respect to the current pseudoflow, and
  // reverse(P) is residual with respect to the flow at the beginning
  // of the current iteration. (Note that such a path exactly
  // satisfies the conditions of Lemma 1.)
  //
  // Let the bound from Lemma 1 with p(w) = 0 be called B(e_0, e_1),
  // and let us say that when a slack relabeling of a node v occurs,
  // we will change the price of v by B(e_0, e_1) such that v tightly
  // satisfies the bound of Lemma 1. Explicitly, we define
  //   B(e_0, e_1) = -(e_0 + e_1) * (n-2)/2.
  //
  // Lemma 1 and Lemma 2 combine to bound the price change during an
  // iteration for any node with excess. Viewed a different way, Lemma
  // 1 and Lemma 2 tell us that if epsilon-optimality can be preserved
  // by changing the price of a node by B(e_0, e_1), that node will
  // never have excess again during the current iteration unless the
  // problem is infeasible. This insight gives us an approach to
  // detect infeasibility (by observing prices on nodes with excess
  // that violate this bound) and to relabel nodes aggressively enough
  // to avoid unnecessary future work while we also avoid falsely
  // concluding the problem is infeasible.
  //
  // From Lemma 1 and Lemma 2, and taking into account our knowledge
  // of the slack relabeling amount, we have Lemma 3.
  //
  // Lemma 3: During any iteration, if the given problem is feasible
  // the price of any node is reduced by less than
  //   -2 * B(e_0, e_1) = (e_0 + e_1) * (n-2).
  //
  // Proof: Straightforward, omitted for expedience.
  //
  // In the case where e_0 = e_1 * alpha, we can express the bound
  // just in terms of e_1, the current iteration's value of epsilon_:
  //   B(e_1) = B(e_1 * alpha, e_1) = -(1 + alpha) * e_1 * (n-2)/2,
  // so we have that p(v) is reduced by less than 2 * B(e_1).
  //
  // Because we use truncating division to compute each iteration's error
  // parameter from that of the previous iteration, it isn't exactly
  // the case that e_0 = e_1 * alpha as we just assumed. To patch this
  // up, we can use the observation that
  //   e_1 = floor(e_0 / alpha),
  // which implies
  //   -e_0 > -(e_1 + 1) * alpha
  // to rewrite from (***):
  //   p(v) > 2 * B(e_0, e_1) > 2 * B((e_1 + 1) * alpha, e_1)
  //        = 2 * -((e_1 + 1) * alpha + e_1) * (n-2)/2
  //        = 2 * -(1 + alpha) * e_1 * (n-2)/2 - alpha * (n-2)
  //        = 2 * B(e_1) - alpha * (n-2)
  //        = -((1 + alpha) * e_1 + alpha) * (n-2).
  //
  // We sum up the bounds for all the iterations to get Lemma 4:
  //
  // Lemma 4: If the given problem is feasible, after k iterations the
  // price of any node is always greater than
  //   -((1 + alpha) * C + (k * alpha)) * (n-2)
  //
  // Proof: Suppose the price decrease of every node in the iteration
  // with epsilon_ == x is bounded by B(x) which is proportional to x
  // (not surpisingly, this will be the same function B() as
  // above). Assume for simplicity that C, the largest cost magnitude,
  // is a power of alpha. Then the price of each node, tallied across
  // all iterations is bounded
  //   p(v) > 2 * B(C/alpha) + 2 * B(C/alpha^2) + ... + 2 * B(kMinEpsilon)
  //        == 2 * B(C/alpha) * alpha / (alpha - 1)
  //        == 2 * B(C) / (alpha - 1).
  // As above, this needs some patching up to handle the fact that we
  // use truncating arithmetic. We saw that each iteration effectively
  // reduces the price bound by alpha * (n-2), hence if there are k
  // iterations, the bound is
  //   p(v) > 2 * B(C) / (alpha - 1) - k * alpha * (n-2)
  //        = -(1 + alpha) * C * (n-2) / (alpha - 1) - k * alpha * (n-2)
  //        = (n-2) * (C * (1 + alpha) / (1 - alpha) - k * alpha).
  //
  // The bound of lemma 4 can be used to warn for possible overflow of
  // arithmetic precision. But because it involves the number of
  // iterations, k, we might as well count through the iterations
  // simply adding up the bounds given by Lemma 3 to get a tighter
  // result. This is what the implementation does.

  // A lower bound on the price of any node at any time throughout the
  // computation. A price below this level proves infeasibility; this
  // value is used for feasibility detection. We use this value also
  // to rule out the possibility of arithmetic overflow or warn the
  // client that we have not been able to rule out that possibility.
  //
  // We can use the value implied by Lemma 4 here, but note that that
  // value includes k, the number of iterations. It's plenty fast if
  // we count through the iterations to compute that value, but if
  // we're going to count through the iterations, we might as well use
  // the two-parameter bound from Lemma 3, summing up as we go. This
  // gives us a tighter bound and more comprehensible code.
  //
  // While computing this bound, if we find the value justified by the
  // theory lies outside the representable range of CostValue, we
  // conclude that the given arc costs have magnitudes so large that
  // we cannot guarantee our calculations don't overflow. If the value
  // justified by the theory lies inside the representable range of
  // CostValue, we commit that our calculation will not overflow. This
  // commitment means we need to be careful with the amount by which
  // we relabel right-side nodes that are incident to any node with
  // only one neighbor.
  CostValue price_lower_bound_;

  // A bound on the amount by which a node's price can be reduced
  // during the current iteration, used only for slack
  // relabelings. Where epsilon is the first iteration's error
  // parameter and C is the largest magnitude of an arc cost, we set
  //   slack_relabeling_price_ = -B(C, epsilon)
  //                           = (C + epsilon) * (n-2)/2.
  //
  // We could use slack_relabeling_price_ for feasibility detection
  // but the feasibility threshold is double the slack relabeling
  // amount and we judge it not to be worth having to multiply by two
  // gratuitously to check feasibility in each double push
  // operation. Instead we settle for feasibility detection using
  // price_lower_bound_ instead, which is somewhat slower in the
  // infeasible case because more relabelings will be required for
  // some node price to attain the looser bound.
  CostValue slack_relabeling_price_;

  // Computes the value of the bound on price reduction for an
  // iteration, given the old and new values of epsilon_.  Because the
  // expression computed here is used in at least one place where we
  // want an additional factor in the denominator, we take that factor
  // as an argument. If extra_divisor == 1, this function computes of
  // the function B() discussed above.
  //
  // Avoids overflow in computing the bound, and sets *in_range =
  // false if the value of the bound doesn't fit in CostValue.
  inline CostValue PriceChangeBound(CostValue old_epsilon,
                                    CostValue new_epsilon,
                                    bool* in_range) const {
    const CostValue n = graph_->num_nodes();
    // We work in double-precision floating point to determine whether
    // we'll overflow the integral CostValue type's range of
    // representation. Switching between integer and double is a
    // rather expensive operation, but we do this only twice per
    // scaling iteration, so we can afford it rather than resort to
    // complex and subtle tricks within the bounds of integer
    // arithmetic.
    //
    // You will want to read the comments above about
    // price_lower_bound_ and slack_relabeling_price_, and have a
    // pencil handy. :-)
    const double result =
        static_cast<double>(std::max<CostValue>(1, n / 2 - 1)) *
        (static_cast<double>(old_epsilon) + static_cast<double>(new_epsilon));
    const double limit =
        static_cast<double>(std::numeric_limits<CostValue>::max());
    if (result > limit) {
      // Our integer computations could overflow.
      if (in_range != nullptr) *in_range = false;
      return std::numeric_limits<CostValue>::max();
    } else {
      // Don't touch *in_range; other computations could already have
      // set it to false and we don't want to overwrite that result.
      return static_cast<CostValue>(result);
    }
  }

  // A scaled record of the largest arc-cost magnitude we've been
  // given during problem setup. This is used to set the initial value
  // of epsilon_, which in turn is used not only as the error
  // parameter but also to determine whether we risk arithmetic
  // overflow during the algorithm.
  //
  // Note: Our treatment of arithmetic overflow assumes the following
  // property of CostValue:
  //   -std::numeric_limits<CostValue>::max() is a representable
  //   CostValue.
  // That property is satisfied if CostValue uses a two's-complement
  // representation.
  CostValue largest_scaled_cost_magnitude_;

  // The total excess in the graph. Given our asymmetric definition of
  // epsilon-optimality and our use of the double-push operation, this
  // equals the number of unmatched left-side nodes.
  NodeIndex total_excess_;

  // Indexed by node index, the price_ values are maintained only for
  // right-side nodes.
  //
  // Note: We use a ZVector to only allocate a vector of size num_left_nodes_
  // instead of 2*num_left_nodes_ since the right-side node indices start at
  // num_left_nodes_.
  ZVector<CostValue> price_;

  // Indexed by left-side node index, the matched_arc_ array gives the
  // arc index of the arc matching any given left-side node, or
  // GraphType::kNilArc if the node is unmatched.
  std::vector<ArcIndex> matched_arc_;

  // Indexed by right-side node index, the matched_node_ array gives
  // the node index of the left-side node matching any given
  // right-side node, or GraphType::kNilNode if the right-side node is
  // unmatched.
  //
  // Note: We use a ZVector for the same reason as for price_.
  ZVector<NodeIndex> matched_node_;

  // The array of arc costs as given in the problem definition, except
  // that they are scaled up by the number of nodes in the graph so we
  // can use integer arithmetic throughout.
  std::vector<CostValue> scaled_arc_cost_;

  // The container of active nodes (i.e., unmatched nodes). This can
  // be switched easily between ActiveNodeStack and ActiveNodeQueue
  // for experimentation.
  std::unique_ptr<ActiveNodeContainerInterface> active_nodes_;

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
    const GraphType& graph, const NodeIndex num_left_nodes)
    : graph_(&graph),
      num_left_nodes_(num_left_nodes),
      success_(false),
      cost_scaling_factor_(1 + num_left_nodes),
      alpha_(FLAGS_assignment_alpha),
      epsilon_(0),
      price_lower_bound_(0),
      slack_relabeling_price_(0),
      largest_scaled_cost_magnitude_(0),
      total_excess_(0),
      price_(num_left_nodes, 2 * num_left_nodes - 1),
      matched_arc_(num_left_nodes, 0),
      matched_node_(num_left_nodes, 2 * num_left_nodes - 1),
      scaled_arc_cost_(graph.max_end_arc_index(), 0),
      active_nodes_(FLAGS_assignment_stack_order
                        ? static_cast<ActiveNodeContainerInterface*>(
                              new ActiveNodeStack())
                        : static_cast<ActiveNodeContainerInterface*>(
                              new ActiveNodeQueue())) {}

template <typename GraphType>
LinearSumAssignment<GraphType>::LinearSumAssignment(
    const NodeIndex num_left_nodes, const ArcIndex num_arcs)
    : graph_(nullptr),
      num_left_nodes_(num_left_nodes),
      success_(false),
      cost_scaling_factor_(1 + num_left_nodes),
      alpha_(FLAGS_assignment_alpha),
      epsilon_(0),
      price_lower_bound_(0),
      slack_relabeling_price_(0),
      largest_scaled_cost_magnitude_(0),
      total_excess_(0),
      price_(num_left_nodes, 2 * num_left_nodes - 1),
      matched_arc_(num_left_nodes, 0),
      matched_node_(num_left_nodes, 2 * num_left_nodes - 1),
      scaled_arc_cost_(num_arcs, 0),
      active_nodes_(FLAGS_assignment_stack_order
                        ? static_cast<ActiveNodeContainerInterface*>(
                              new ActiveNodeStack())
                        : static_cast<ActiveNodeContainerInterface*>(
                              new ActiveNodeQueue())) {}

template <typename GraphType>
void LinearSumAssignment<GraphType>::SetArcCost(ArcIndex arc, CostValue cost) {
  if (graph_ != nullptr) {
    DCHECK_GE(arc, 0);
    DCHECK_LT(arc, graph_->num_arcs());
    NodeIndex head = Head(arc);
    DCHECK_LE(num_left_nodes_, head);
  }
  cost *= cost_scaling_factor_;
  const CostValue cost_magnitude = std::abs(cost);
  largest_scaled_cost_magnitude_ =
      std::max(largest_scaled_cost_magnitude_, cost_magnitude);
  scaled_arc_cost_[arc] = cost;
}

template <typename ArcIndexType>
class CostValueCycleHandler : public PermutationCycleHandler<ArcIndexType> {
 public:
  explicit CostValueCycleHandler(std::vector<CostValue>* cost)
      : temp_(0), cost_(cost) {}

  void SetTempFromIndex(ArcIndexType source) override {
    temp_ = (*cost_)[source];
  }

  void SetIndexFromIndex(ArcIndexType source,
                         ArcIndexType destination) const override {
    (*cost_)[destination] = (*cost_)[source];
  }

  void SetIndexFromTemp(ArcIndexType destination) const override {
    (*cost_)[destination] = temp_;
  }

  ~CostValueCycleHandler() override {}

 private:
  CostValue temp_;
  std::vector<CostValue>* const cost_;

  DISALLOW_COPY_AND_ASSIGN(CostValueCycleHandler);
};

// Logically this class should be defined inside OptimizeGraphLayout,
// but compilation fails if we do that because C++98 doesn't allow
// instantiation of member templates with function-scoped types as
// template parameters, which in turn is because those function-scoped
// types lack linkage.
template <typename GraphType>
class ArcIndexOrderingByTailNode {
 public:
  explicit ArcIndexOrderingByTailNode(const GraphType& graph) : graph_(graph) {}

  // Says ArcIndex a is less than ArcIndex b if arc a's tail is less
  // than arc b's tail. If their tails are equal, orders according to
  // heads.
  bool operator()(typename GraphType::ArcIndex a,
                  typename GraphType::ArcIndex b) const {
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

// Passes ownership of the cycle handler to the caller.
template <typename GraphType>
PermutationCycleHandler<typename GraphType::ArcIndex>*
LinearSumAssignment<GraphType>::ArcAnnotationCycleHandler() {
  return new CostValueCycleHandler<typename GraphType::ArcIndex>(
      &scaled_arc_cost_);
}

template <typename GraphType>
void LinearSumAssignment<GraphType>::OptimizeGraphLayout(GraphType* graph) {
  // The graph argument is only to give us a non-const-qualified
  // handle on the graph we already have. Any different graph is
  // nonsense.
  DCHECK_EQ(graph_, graph);
  const ArcIndexOrderingByTailNode<GraphType> compare(*graph_);
  CostValueCycleHandler<typename GraphType::ArcIndex> cycle_handler(
      &scaled_arc_cost_);
  TailArrayManager<GraphType> tail_array_manager(graph);
  tail_array_manager.BuildTailArrayFromAdjacencyListsIfForwardGraph();
  graph->GroupForwardArcsByFunctor(compare, &cycle_handler);
  tail_array_manager.ReleaseTailArrayIfForwardGraph();
}

template <typename GraphType>
CostValue LinearSumAssignment<GraphType>::NewEpsilon(
    const CostValue current_epsilon) const {
  return std::max(current_epsilon / alpha_, kMinEpsilon);
}

template <typename GraphType>
bool LinearSumAssignment<GraphType>::UpdateEpsilon() {
  CostValue new_epsilon = NewEpsilon(epsilon_);
  slack_relabeling_price_ = PriceChangeBound(epsilon_, new_epsilon, nullptr);
  epsilon_ = new_epsilon;
  VLOG(3) << "Updated: epsilon_ == " << epsilon_;
  VLOG(4) << "slack_relabeling_price_ == " << slack_relabeling_price_;
  DCHECK_GT(slack_relabeling_price_, 0);
  // For today we always return true; in the future updating epsilon
  // in sophisticated ways could conceivably detect infeasibility
  // before the first iteration of Refine().
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
  for (BipartiteLeftNodeIterator node_it(*graph_, num_left_nodes_);
       node_it.Ok(); node_it.Next()) {
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
  for (BipartiteLeftNodeIterator node_it(*graph_, num_left_nodes_);
       node_it.Ok(); node_it.Next()) {
    const NodeIndex node = node_it.Index();
    if (IsActive(node)) {
      // This can happen in the first iteration when nothing is
      // matched yet.
      total_excess_ += 1;
    } else {
      // We're about to create a unit of excess by unmatching these nodes.
      total_excess_ += 1;
      const NodeIndex mate = GetMate(node);
      matched_arc_[node] = GraphType::kNilArc;
      matched_node_[mate] = GraphType::kNilNode;
    }
  }
}

// Returns true for success, false for infeasible.
template <typename GraphType>
bool LinearSumAssignment<GraphType>::DoublePush(NodeIndex source) {
  DCHECK_GT(num_left_nodes_, source);
  DCHECK(IsActive(source)) << "Node " << source
                           << "must be active (unmatched)!";
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
    matched_arc_[to_unmatch] = GraphType::kNilArc;
    active_nodes_->Add(to_unmatch);
    // This counts as a double push.
    iteration_stats_.double_pushes_ += 1;
  } else {
    // We are about to increase the cardinality of the matching.
    total_excess_ -= 1;
    // This counts as a single push.
    iteration_stats_.pushes_ += 1;
  }
  matched_arc_[source] = best_arc;
  matched_node_[new_mate] = source;
  // Finally, relabel new_mate.
  iteration_stats_.relabelings_ += 1;
  const CostValue new_price = price_[new_mate] - gap - epsilon_;
  price_[new_mate] = new_price;
  return new_price >= price_lower_bound_;
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
      //
      // If infeasibility is detected after the first iteration, we
      // have a bug. We don't crash production code in this case but
      // we know we're returning a wrong answer so we we leave a
      // message in the logs to increase our hope of chasing down the
      // problem.
      LOG_IF(DFATAL, total_stats_.refinements_ > 0)
          << "Infeasibility detection triggered after first iteration found "
          << "a feasible assignment!";
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
// Precondition: left_node is unmatched and has at least one incident
// arc. This allows us to simplify the code. The debug-only
// counterpart to this routine is LinearSumAssignment::ImplicitPrice()
// and it assumes there is an incident arc but does not assume the
// node is unmatched. The condition that each left node has at least
// one incident arc is explicitly computed during FinalizeSetup().
//
// This function is large enough that our suggestion that the compiler
// inline it might be pointless.
template <typename GraphType>
inline typename LinearSumAssignment<GraphType>::ImplicitPriceSummary
LinearSumAssignment<GraphType>::BestArcAndGap(NodeIndex left_node) const {
  DCHECK(IsActive(left_node))
      << "Node " << left_node << " must be active (unmatched)!";
  DCHECK_GT(epsilon_, 0);
  typename GraphType::OutgoingArcIterator arc_it(*graph_, left_node);
  ArcIndex best_arc = arc_it.Index();
  CostValue min_partial_reduced_cost = PartialReducedCost(best_arc);
  // We choose second_min_partial_reduced_cost so that in the case of
  // the largest possible gap (which results from a left-side node
  // with only a single incident residual arc), the corresponding
  // right-side node will be relabeled by an amount that exactly
  // matches slack_relabeling_price_.
  const CostValue max_gap = slack_relabeling_price_ - epsilon_;
  CostValue second_min_partial_reduced_cost =
      min_partial_reduced_cost + max_gap;
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
  const CostValue gap = std::min<CostValue>(
      second_min_partial_reduced_cost - min_partial_reduced_cost, max_gap);
  DCHECK_GE(gap, 0);
  return std::make_pair(best_arc, gap);
}

// Only for debugging.
//
// Requires the precondition, explicitly computed in FinalizeSetup(),
// that every left-side node has at least one incident arc.
template <typename GraphType>
inline CostValue LinearSumAssignment<GraphType>::ImplicitPrice(
    NodeIndex left_node) const {
  DCHECK_GT(num_left_nodes_, left_node);
  DCHECK_GT(epsilon_, 0);
  typename GraphType::OutgoingArcIterator arc_it(*graph_, left_node);
  // We must not execute this method if left_node has no incident arc.
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
    return -(min_partial_reduced_cost + slack_relabeling_price_);
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
  for (NodeIndex node = 0; node < graph_->num_nodes(); ++node) {
    if (IsActiveForDebugging(node)) {
      return false;
    }
  }
  return true;
}

// Only for debugging.
template <typename GraphType>
bool LinearSumAssignment<GraphType>::EpsilonOptimal() const {
  for (BipartiteLeftNodeIterator node_it(*graph_, num_left_nodes_);
       node_it.Ok(); node_it.Next()) {
    const NodeIndex left_node = node_it.Index();
    // Get the implicit price of left_node and make sure the reduced
    // costs of left_node's incident arcs are in bounds.
    CostValue left_node_price = ImplicitPrice(left_node);
    for (typename GraphType::OutgoingArcIterator arc_it(*graph_, left_node);
         arc_it.Ok(); arc_it.Next()) {
      const ArcIndex arc = arc_it.Index();
      const CostValue reduced_cost = left_node_price + PartialReducedCost(arc);
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
  incidence_precondition_satisfied_ = true;
  // epsilon_ must be greater than kMinEpsilon so that in the case
  // where the largest arc cost is zero, we still do a Refine()
  // iteration.
  epsilon_ = std::max(largest_scaled_cost_magnitude_, kMinEpsilon + 1);
  VLOG(2) << "Largest given cost magnitude: "
          << largest_scaled_cost_magnitude_ / cost_scaling_factor_;
  // Initialize left-side node-indexed arrays and check incidence
  // precondition.
  for (NodeIndex node = 0; node < num_left_nodes_; ++node) {
    matched_arc_[node] = GraphType::kNilArc;
    typename GraphType::OutgoingArcIterator arc_it(*graph_, node);
    if (!arc_it.Ok()) {
      incidence_precondition_satisfied_ = false;
    }
  }
  // Initialize right-side node-indexed arrays. Example: prices are
  // stored only for right-side nodes.
  for (NodeIndex node = num_left_nodes_; node < graph_->num_nodes(); ++node) {
    price_[node] = 0;
    matched_node_[node] = GraphType::kNilNode;
  }
  bool in_range = true;
  double double_price_lower_bound = 0.0;
  CostValue new_error_parameter;
  CostValue old_error_parameter = epsilon_;
  do {
    new_error_parameter = NewEpsilon(old_error_parameter);
    double_price_lower_bound -=
        2.0 * static_cast<double>(PriceChangeBound(
                  old_error_parameter, new_error_parameter, &in_range));
    old_error_parameter = new_error_parameter;
  } while (new_error_parameter != kMinEpsilon);
  const double limit =
      -static_cast<double>(std::numeric_limits<CostValue>::max());
  if (double_price_lower_bound < limit) {
    in_range = false;
    price_lower_bound_ = -std::numeric_limits<CostValue>::max();
  } else {
    price_lower_bound_ = static_cast<CostValue>(double_price_lower_bound);
  }
  VLOG(4) << "price_lower_bound_ == " << price_lower_bound_;
  DCHECK_LE(price_lower_bound_, 0);
  if (!in_range) {
    LOG(WARNING) << "Price change bound exceeds range of representable "
                 << "costs; arithmetic overflow is not ruled out and "
                 << "infeasibility might go undetected.";
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
  CHECK(graph_ != nullptr);
  bool ok = graph_->num_nodes() == 2 * num_left_nodes_;
  if (!ok) return false;
  // Note: FinalizeSetup() might have been called already by white-box
  // test code or by a client that wants to react to the possibility
  // of overflow before solving the given problem, but FinalizeSetup()
  // is idempotent and reasonably fast, so we call it unconditionally
  // here.
  FinalizeSetup();
  ok = ok && incidence_precondition_satisfied_;
  DCHECK(!ok || EpsilonOptimal());
  while (ok && epsilon_ > kMinEpsilon) {
    ok = ok && UpdateEpsilon();
    ok = ok && Refine();
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
  for (BipartiteLeftNodeIterator node_it(*this); node_it.Ok(); node_it.Next()) {
    cost += GetAssignmentCost(node_it.Index());
  }
  return cost;
}

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_LINEAR_ASSIGNMENT_H_
