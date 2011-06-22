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

#include "graph/linear_assignment.h"

#include <algorithm>
#include <cstdlib>

#include "base/commandlineflags.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "util/permutation.h"

DEFINE_int64(assignment_alpha, 5,
             "Divisor for epsilon at each Refine "
             "step of LinearSumAssignment.");
DEFINE_int32(assignment_progress_logging_period, 5000,
             "Number of relabelings to do between logging progress messages "
             "when verbose level is 4 or more.");
DEFINE_bool(assignment_stack_order, true,
            "Process active nodes in stack (as opposed to queue) order.");

namespace operations_research {

const CostValue LinearSumAssignment::kMinEpsilon = 1;

LinearSumAssignment::LinearSumAssignment(const StarGraph& graph,
                                         NodeIndex num_left_nodes)
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
      price_(num_left_nodes + StarGraph::kFirstNode,
             graph.max_end_node_index() - 1),
      matched_(StarGraph::kFirstNode,
               graph.max_end_node_index() - 1),
      scaled_arc_cost_(StarGraph::kFirstArc,
                       graph.max_end_arc_index() - 1),
      active_nodes_(
          FLAGS_assignment_stack_order ?
          static_cast<ActiveNodeContainerInterface*>(new ActiveNodeStack()) :
          static_cast<ActiveNodeContainerInterface*>(new ActiveNodeQueue())) { }

void LinearSumAssignment::SetArcCost(ArcIndex arc,
                                     CostValue cost) {
  DCHECK(graph_.CheckArcValidity(arc));
  NodeIndex tail = Tail(arc);
  NodeIndex head = Head(arc);
  DCHECK_GT(num_left_nodes_, tail);
  DCHECK_LE(num_left_nodes_, head);
  cost *= cost_scaling_factor_;
  const CostValue cost_magnitude = std::abs(cost);
  largest_scaled_cost_magnitude_ = std::max(largest_scaled_cost_magnitude_,
                                            cost_magnitude);
  scaled_arc_cost_.Set(arc, cost);
}

class CostValueCycleHandler
    : public PermutationCycleHandler<ArcIndex> {
 public:
  explicit CostValueCycleHandler(Int64PackedArray* cost)
      : temp_(0),
        cost_(cost) { }

  virtual void SetTempFromIndex(ArcIndex source) {
    temp_ = cost_->Value(source);
  }

  virtual void SetIndexFromIndex(ArcIndex source,
                                 ArcIndex destination) const {
    cost_->Set(destination, cost_->Value(source));
  }

  virtual void SetIndexFromTemp(ArcIndex destination) const {
    cost_->Set(destination, temp_);
  }

  virtual ~CostValueCycleHandler() { }

 private:
  CostValue temp_;

  Int64PackedArray* cost_;

  DISALLOW_COPY_AND_ASSIGN(CostValueCycleHandler);
};

// Logically this class should be defined inside OptimizeGraphLayout,
// but compilation fails if we do that because C++98 doesn't allow
// instantiation of member templates with function-scoped types as
// template parameters, which in turn is because those function-scoped
// types lack linkage.
class ArcIndexOrderingByTailNode {
 public:
  explicit ArcIndexOrderingByTailNode(const StarGraph& graph)
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
  const StarGraph& graph_;

  // Copy and assign are allowed; they have to be for STL to work
  // with this functor, although it seems like a bug for STL to be
  // written that way.
};

void LinearSumAssignment::OptimizeGraphLayout(StarGraph *graph) {
  // The graph argument is only to give us a non-const-qualified
  // handle on the graph we already have. Any different graph is
  // nonsense.
  DCHECK_EQ(&graph_, graph);
  const ArcIndexOrderingByTailNode compare(graph_);
  CostValueCycleHandler cycle_handler(&scaled_arc_cost_);
  graph->GroupForwardArcsByFunctor(compare, &cycle_handler);
}

bool LinearSumAssignment::UpdateEpsilon() {
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

inline bool LinearSumAssignment::IsActive(NodeIndex node) const {
  // In a non-debug compilation we could assert that this is a
  // left-side node, but in debug compilations we use this routine to
  // check invariants for right-side nodes. So no assertion.
  return matched_[node] == StarGraph::kNilArc;
}

void LinearSumAssignment::InitializeActiveNodeContainer() {
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
void LinearSumAssignment::SaturateNegativeArcs() {
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
      matched_.Set(mate, StarGraph::kNilArc);
      matched_.Set(node, StarGraph::kNilArc);
    }
  }
}

// Returns true for success, false for infeasible.
bool LinearSumAssignment::DoublePush(NodeIndex source) {
    DCHECK(IsActive(source));
  ImplicitPriceSummary summary = BestArcAndGap(source);
  const ArcIndex best_arc = summary.first;
  const CostValue gap = summary.second;
  // Now we have the best arc incident to source, i.e., the one with
  // minimum reduced cost. Match that arc, unmatching its head if
  // necessary.
  if (best_arc == StarGraph::kNilArc) {
    return false;
  }
  const NodeIndex new_mate = Head(best_arc);
  const ArcIndex to_unmatch = matched_[new_mate];
  if (to_unmatch != StarGraph::kNilArc) {
    // Unmatch new_mate from its current mate, pushing the unit of
    // flow back to a node on the left side as a unit of excess.
    const NodeIndex destination = Tail(to_unmatch);
    matched_.Set(destination, StarGraph::kNilArc);
    active_nodes_->Add(destination);
    // This counts as a double push.
    iteration_stats_.double_pushes_ += 1;
  } else {
    // We are about to increase the cardinality of the matching.
    total_excess_ -= 1;
    // This counts as a single push.
    iteration_stats_.pushes_ += 1;
  }
  matched_.Set(source, best_arc);
  matched_.Set(new_mate, best_arc);
  // Finally, relabel new_mate.
  iteration_stats_.relabelings_ += 1;
  price_.Set(new_mate, price_[new_mate] - gap - epsilon_);
  return price_[new_mate] >= price_lower_bound_;
}

bool LinearSumAssignment::Refine() {
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
inline LinearSumAssignment::ImplicitPriceSummary
LinearSumAssignment::BestArcAndGap(NodeIndex left_node) const {
    DCHECK(IsActive(left_node));
  DCHECK_GT(epsilon_, 0);
  // During any scaling iteration, the price of an active node
  // decreases by at most price_reduction_bound_ and all left-side
  // nodes are made active at the beginning of Refine(), so the bound
  // applies to all left-side nodes.
  StarGraph::OutgoingArcIterator arc_it(graph_, left_node);
  ArcIndex best_arc = arc_it.Index();
  CostValue min_partial_reduced_cost = PartialReducedCost(best_arc);
  CostValue second_min_partial_reduced_cost =
      min_partial_reduced_cost + price_reduction_bound_;
  for (arc_it.Next(); arc_it.Ok(); arc_it.Next()) {
    const ArcIndex arc = arc_it.Index();
    const CostValue partial_reduced_cost = PartialReducedCost(arc);
    if (partial_reduced_cost < min_partial_reduced_cost) {
      best_arc = arc;
      second_min_partial_reduced_cost = min_partial_reduced_cost;
      min_partial_reduced_cost = partial_reduced_cost;
    } else if (partial_reduced_cost < second_min_partial_reduced_cost) {
      second_min_partial_reduced_cost = partial_reduced_cost;
    }
  }
  const CostValue gap =
      second_min_partial_reduced_cost - min_partial_reduced_cost;
  DCHECK_GE(gap, 0);
  return std::make_pair(best_arc, gap);
}

// Only for debugging.
inline CostValue
LinearSumAssignment::ImplicitPrice(NodeIndex left_node) const {
  DCHECK_GT(epsilon_, 0);
  StarGraph::OutgoingArcIterator arc_it(graph_, left_node);
  // If the input problem is feasible, it is always the case that
  // arc_it.Ok(), i.e., that there is at least one arc incident to
  // left_node.
  DCHECK(arc_it.Ok());
  ArcIndex best_arc = arc_it.Index();
  if (best_arc == matched_[left_node]) {
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
    if (arc != matched_[left_node]) {
      const CostValue partial_reduced_cost = PartialReducedCost(arc);
      if (partial_reduced_cost < min_partial_reduced_cost) {
        min_partial_reduced_cost = partial_reduced_cost;
      }
    }
  }
  return -min_partial_reduced_cost;
}

// Only for debugging.
bool LinearSumAssignment::AllMatched() const {
  for (StarGraph::NodeIterator node_it(graph_);
       node_it.Ok();
       node_it.Next()) {
    if (IsActive(node_it.Index())) {
      return false;
    }
  }
  return true;
}

// Only for debugging.
bool LinearSumAssignment::EpsilonOptimal() const {
  for (BipartiteLeftNodeIterator node_it(graph_, num_left_nodes_);
       node_it.Ok();
       node_it.Next()) {
    const NodeIndex left_node = node_it.Index();
    // Get the implicit price of left_node and make sure the reduced
    // costs of left_node's incident arcs are in bounds.
    CostValue left_node_price = ImplicitPrice(left_node);
    for (StarGraph::OutgoingArcIterator arc_it(graph_, left_node);
         arc_it.Ok();
         arc_it.Next()) {
      const ArcIndex arc = arc_it.Index();
      const CostValue reduced_cost =
          left_node_price + PartialReducedCost(arc);
      // Note the asymmetric definition of epsilon-optimality that we
      // use because it means we can saturate all admissible arcs in
      // the beginning of Refine() just by unmatching all matched
      // nodes.
      if (matched_[left_node] == arc) {
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

bool LinearSumAssignment::FinalizeSetup() {
  epsilon_ = largest_scaled_cost_magnitude_;
  VLOG(2) << "Largest given cost magnitude: " <<
      largest_scaled_cost_magnitude_ / cost_scaling_factor_;
  // Initialize left-side node-indexed arrays.
  StarGraph::NodeIterator node_it(graph_);
  for (; node_it.Ok(); node_it.Next()) {
    const NodeIndex node = node_it.Index();
    if (node >= num_left_nodes_) {
      break;
    }
    matched_.Set(node, StarGraph::kNilArc);
  }
  // Initialize right-side node-indexed arrays. Example: prices are
  // stored only for right-side nodes.
  for (; node_it.Ok(); node_it.Next()) {
    const NodeIndex node = node_it.Index();
    price_.Set(node, 0);
    matched_.Set(node, StarGraph::kNilArc);
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

void LinearSumAssignment::ReportAndAccumulateStats() {
  total_stats_.Add(iteration_stats_);
  VLOG(3) << "Iteration stats: " << iteration_stats_.StatsString();
  iteration_stats_.Clear();
}

bool LinearSumAssignment::ComputeAssignment() {
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
  return ok;
}

CostValue LinearSumAssignment::GetCost() const {
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
