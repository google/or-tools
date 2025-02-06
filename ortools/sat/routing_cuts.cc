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

#include "ortools/sat/routing_cuts.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/cleanup/cleanup.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/strong_vector.h"
#include "ortools/graph/max_flow.h"
#include "ortools/sat/cuts.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/model.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

namespace {

// Helper to compute lower bounds based on binary relations enforced by arc
// literals in a RoutesConstraint.
class LowerBoundsHelper {
 public:
  // See MinOutgoingFlowHelper.
  LowerBoundsHelper(
      std::vector<absl::flat_hash_map<IntegerVariable, IntegerValue>>&
          node_var_lower_bounds,
      std::vector<absl::flat_hash_map<IntegerVariable, IntegerValue>>&
          next_node_var_lower_bounds,
      const IntegerTrail& integer_trail)
      : integer_trail_(integer_trail),
        node_var_lower_bounds_(node_var_lower_bounds),
        next_node_var_lower_bounds_(next_node_var_lower_bounds) {}

  // Updates the bounds of `a.var` if the `arc_index` arc were selected, based
  // on the given relation `a + b \in [lhs, rhs]` (supposedly enforced by the
  // arc literal), and on the bounds of `b.var` (if the tail node appears at the
  // "current" position -- see MinOutgoingFlowHelper).
  void UpdateNextVarBoundsByArc(int tail, int arc_index, LinearTerm a,
                                LinearTerm b, IntegerValue lhs,
                                IntegerValue rhs) {
    if (a.coeff == 0) return;
    a.MakeCoeffPositive();
    b.MakeCoeffPositive();
    // lb(b.y) <= b.y <= ub(b.y) and lhs <= a.x + b.y <= rhs imply
    //   ceil((lhs - ub(b.y)) / a) <= x <= floor((rhs - lb(b.y)) / a)
    lhs = lhs + b.coeff * GetCurrentVarLowerBound(tail, NegationOf(b.var));
    rhs = rhs - b.coeff * GetCurrentVarLowerBound(tail, b.var);
    UpdateLowerBoundByArc(a.var, arc_index,
                          MathUtil::CeilOfRatio(lhs, a.coeff));
    UpdateUpperBoundByArc(a.var, arc_index,
                          MathUtil::FloorOfRatio(rhs, a.coeff));
  }

  // Updates the bounds of the variables if `head` appears at the "next"
  // position in a feasible path, and if it can be reached by
  // `num_reachable_incoming_arcs` arcs. `head` must be the head of the arcs
  // used in all the previous calls to UpdateNextVarBoundsByArc().
  //
  // Returns whether the new bounds of the variables are feasible. If not,
  // `head` cannot appear at the "next" position in a feasible path.
  bool UpdateNextVarBounds(int head, int num_reachable_incoming_arcs) {
    if (num_reachable_incoming_arcs == 0) return false;
    for (const auto& [var, lower_bound_by_arc_index] :
         lower_bound_by_var_and_arc_index_) {
      // If each arc which can reach `head` enforces some lower bound for `var`,
      // then the lower bound of `var` can be increased to the minimum of these
      // arc-specific lower bounds (since at least one of these arcs must be
      // selected to reach `head`).
      if (lower_bound_by_arc_index.size() == num_reachable_incoming_arcs) {
        IntegerValue lb = lower_bound_by_arc_index.begin()->second;
        for (const auto& [arc_index, lower_bound] : lower_bound_by_arc_index) {
          lb = std::min(lb, lower_bound);
        }
        if (!UpdateNextVarLowerBound(head, var, lb)) return false;
      }
    }
    return true;
  }

 private:
  // Returns the lower bound of `var` if `node` appears at the current position
  // in a feasible path.
  IntegerValue GetCurrentVarLowerBound(int node, IntegerVariable var) const {
    auto it = node_var_lower_bounds_[node].find(var);
    if (it != node_var_lower_bounds_[node].end()) return it->second;
    return integer_trail_.LevelZeroLowerBound(var);
  }

  // Returns the lower bound of `var` if `node` appears at the next position in
  // a feasible path.
  IntegerValue GetNextVarLowerBound(int node, IntegerVariable var) const {
    auto it = next_node_var_lower_bounds_[node].find(var);
    if (it != next_node_var_lower_bounds_[node].end()) return it->second;
    return integer_trail_.LevelZeroLowerBound(var);
  }

  // Sets the lower bound of `var` if `node` appears at the next position in a
  // feasible path to `lb`, if `lb` is greater than the current value. Returns
  // whether the new bounds of `var` are feasible.
  bool UpdateNextVarLowerBound(int node, IntegerVariable var, IntegerValue lb) {
    if (lb <= integer_trail_.LevelZeroLowerBound(var)) return true;
    auto it = next_node_var_lower_bounds_[node].find(var);
    if (it != next_node_var_lower_bounds_[node].end()) {
      lb = std::max(lb, it->second);
      it->second = lb;
    } else {
      next_node_var_lower_bounds_[node][var] = lb;
    }
    const IntegerValue ub = -GetNextVarLowerBound(node, NegationOf(var));
    return lb <= ub;
  }

  void UpdateLowerBoundByArc(IntegerVariable var, int arc_index,
                             IntegerValue lb) {
    auto& lower_bound_by_arc_index = lower_bound_by_var_and_arc_index_[var];
    auto it = lower_bound_by_arc_index.find(arc_index);
    if (it != lower_bound_by_arc_index.end()) {
      it->second = std::max(it->second, lb);
    } else {
      lower_bound_by_arc_index[arc_index] = lb;
    }
  };

  void UpdateUpperBoundByArc(IntegerVariable var, int arc_index,
                             IntegerValue ub) {
    UpdateLowerBoundByArc(NegationOf(var), arc_index, -ub);
  };

  const IntegerTrail& integer_trail_;
  // See MinOutgoingFlowHelper.
  std::vector<absl::flat_hash_map<IntegerVariable, IntegerValue>>&
      node_var_lower_bounds_;
  std::vector<absl::flat_hash_map<IntegerVariable, IntegerValue>>&
      next_node_var_lower_bounds_;
  // For each variable, a map from arc index to the lower bound of that variable
  // if this arc were selected, deduced from the binary relations enforced by
  // the arc literal.
  // TODO(user): see if we can use a more efficient data structure.
  absl::flat_hash_map<IntegerVariable, absl::flat_hash_map<int, IntegerValue>>
      lower_bound_by_var_and_arc_index_;
};

}  // namespace

MinOutgoingFlowHelper::MinOutgoingFlowHelper(
    int num_nodes, const std::vector<int>& tails, const std::vector<int>& heads,
    const std::vector<Literal>& literals, Model* model)
    : tails_(tails),
      heads_(heads),
      literals_(literals),
      binary_relation_repository_(
          *model->GetOrCreate<BinaryRelationRepository>()),
      trail_(*model->GetOrCreate<Trail>()),
      integer_trail_(*model->GetOrCreate<IntegerTrail>()),
      in_subset_(num_nodes, false),
      incoming_arc_indices_(num_nodes),
      reachable_(num_nodes, false),
      next_reachable_(num_nodes, false),
      node_var_lower_bounds_(num_nodes),
      next_node_var_lower_bounds_(num_nodes) {}

int MinOutgoingFlowHelper::ComputeMinOutgoingFlow(
    absl::Span<const int> subset) {
  DCHECK_GE(subset.size(), 1);
  DCHECK(absl::c_all_of(in_subset_, [](bool b) { return !b; }));
  DCHECK(absl::c_all_of(incoming_arc_indices_,
                        [](const auto& v) { return v.empty(); }));
  DCHECK(absl::c_all_of(reachable_, [](bool b) { return !b; }));
  DCHECK(absl::c_all_of(next_reachable_, [](bool b) { return !b; }));
  DCHECK(absl::c_all_of(node_var_lower_bounds_,
                        [](const auto& m) { return m.empty(); }));
  DCHECK(absl::c_all_of(next_node_var_lower_bounds_,
                        [](const auto& m) { return m.empty(); }));

  for (const int n : subset) {
    in_subset_[n] = true;
    // Conservatively assume that each subset node is reachable from outside.
    reachable_[n] = true;
  }
  const int num_arcs = tails_.size();
  for (int i = 0; i < num_arcs; ++i) {
    if (in_subset_[tails_[i]] && in_subset_[heads_[i]] &&
        heads_[i] != tails_[i]) {
      incoming_arc_indices_[heads_[i]].push_back(i);
    }
  }

  // Maximum number of nodes of a feasible path inside the subset.
  int longest_path_length = 1;
  while (longest_path_length < subset.size()) {
    bool at_least_one_next_node_reachable = false;
    for (const int head : subset) {
      LowerBoundsHelper lower_bounds_helper(
          node_var_lower_bounds_, next_node_var_lower_bounds_, integer_trail_);
      int num_reachable_incoming_arcs = 0;
      for (const int incoming_arc_index : incoming_arc_indices_[head]) {
        const int tail = tails_[incoming_arc_index];
        const Literal lit = literals_[incoming_arc_index];
        if (!reachable_[tail]) continue;
        ++num_reachable_incoming_arcs;
        for (const int relation_index :
             binary_relation_repository_.relation_indices(lit)) {
          const auto& r = binary_relation_repository_.relation(relation_index);
          lower_bounds_helper.UpdateNextVarBoundsByArc(tail, incoming_arc_index,
                                                       r.a, r.b, r.lhs, r.rhs);
          lower_bounds_helper.UpdateNextVarBoundsByArc(tail, incoming_arc_index,
                                                       r.b, r.a, r.lhs, r.rhs);
        }
      }
      next_reachable_[head] = lower_bounds_helper.UpdateNextVarBounds(
          head, num_reachable_incoming_arcs);
      if (next_reachable_[head]) at_least_one_next_node_reachable = true;
    }
    if (!at_least_one_next_node_reachable) {
      break;
    }
    std::swap(reachable_, next_reachable_);
    std::swap(node_var_lower_bounds_, next_node_var_lower_bounds_);
    for (const int n : subset) {
      next_node_var_lower_bounds_[n].clear();
    }
    ++longest_path_length;
  }

  // The maximum number of distinct paths of length `longest_path_length`.
  int max_longest_paths = 0;
  // Reset the temporary data structures for the next call.
  for (const int n : subset) {
    in_subset_[n] = false;
    incoming_arc_indices_[n].clear();
    if (reachable_[n]) ++max_longest_paths;
    reachable_[n] = false;
    next_reachable_[n] = false;
    node_var_lower_bounds_[n].clear();
    next_node_var_lower_bounds_[n].clear();
  }
  if (max_longest_paths * longest_path_length < subset.size()) {
    // If `longest_path_length` is 1, then `reachable_` has not been modified by
    // the above while loop, and thus `max_longest_paths` is the subset size.
    // This branch can thus not be taken in this case.
    DCHECK_GT(longest_path_length, 1);
    // TODO(user): Consider using information about the number of shorter
    // paths to derive an even better bound.
    return max_longest_paths +
           MathUtil::CeilOfRatio(static_cast<int>(subset.size()) -
                                     max_longest_paths * longest_path_length,
                                 longest_path_length - 1);
  }
  return MathUtil::CeilOfRatio(static_cast<int>(subset.size()),
                               longest_path_length);
}

namespace {

class OutgoingCutHelper {
 public:
  OutgoingCutHelper(int num_nodes, int64_t capacity,
                    absl::Span<const int64_t> demands,
                    const std::vector<int>& tails,
                    const std::vector<int>& heads,
                    const std::vector<Literal>& literals,
                    const std::vector<double>& literal_lp_values,
                    const std::vector<ArcWithLpValue>& relevant_arcs,
                    LinearConstraintManager* manager, Model* model)
      : num_nodes_(num_nodes),
        capacity_(capacity),
        demands_(demands),
        tails_(tails),
        heads_(heads),
        literals_(literals),
        literal_lp_values_(literal_lp_values),
        relevant_arcs_(relevant_arcs),
        manager_(manager),
        random_(model->GetOrCreate<ModelRandomGenerator>()),
        encoder_(model->GetOrCreate<IntegerEncoder>()),
        routing_cut_subset_size_for_binary_relation_bound_(
            model->GetOrCreate<SatParameters>()
                ->routing_cut_subset_size_for_binary_relation_bound()),
        in_subset_(num_nodes, false),
        min_outgoing_flow_helper_(num_nodes, tails, heads, literals, model) {
    // Compute the total demands in order to know the minimum incoming/outgoing
    // flow.
    for (const int64_t demand : demands) total_demand_ += demand;
    complement_of_subset_.reserve(num_nodes_);
  }

  // Try to add an outgoing cut from the given subset.
  bool TrySubsetCut(std::string name, absl::Span<const int> subset);

  // If we look at the symmetrized version (tail <-> head = tail->head +
  // head->tail) and we split all the edges between a subset of nodes S and the
  // outside into a set A and the other d(S)\A, and |A| is odd, we have a
  // constraint of the form:
  //   "all edge of A at 1" => sum other edges >= 1.
  // This is because a cycle or multiple-cycle must go in/out an even number
  // of time. This enforced constraint simply linearize to:
  //    sum_d(S)\A x_e + sum_A (1 - x_e) >= 1.
  //
  // Given a subset of nodes, it is easy to identify the best subset A of edge
  // to consider.
  bool TryBlossomSubsetCut(std::string name,
                           absl::Span<const ArcWithLpValue> symmetrized_edges,
                           absl::Span<const int> subset);

 private:
  // Add a cut of the form Sum_{outgoing arcs from S} lp >= rhs_lower_bound.
  //
  // Note that we used to also add the same cut for the incoming arcs, but
  // because of flow conservation on these problems, the outgoing flow is always
  // the same as the incoming flow, so adding this extra cut doesn't seem
  // relevant.
  bool AddOutgoingCut(std::string name, int subset_size,
                      const std::vector<bool>& in_subset,
                      int64_t rhs_lower_bound, int ignore_arcs_with_head);

  const int num_nodes_;
  const int64_t capacity_;
  const absl::Span<const int64_t> demands_;
  const std::vector<int>& tails_;
  const std::vector<int>& heads_;
  const std::vector<Literal>& literals_;
  const std::vector<double>& literal_lp_values_;
  const std::vector<ArcWithLpValue>& relevant_arcs_;
  LinearConstraintManager* manager_;
  ModelRandomGenerator* random_;
  IntegerEncoder* encoder_;
  const int routing_cut_subset_size_for_binary_relation_bound_;

  int64_t total_demand_ = 0;
  std::vector<bool> in_subset_;
  std::vector<int> complement_of_subset_;
  MinOutgoingFlowHelper min_outgoing_flow_helper_;
};

bool OutgoingCutHelper::AddOutgoingCut(std::string name, int subset_size,
                                       const std::vector<bool>& in_subset,
                                       int64_t rhs_lower_bound,
                                       int ignore_arcs_with_head) {
  // A node is said to be optional if it can be excluded from the subcircuit,
  // in which case there is a self-loop on that node.
  // If there are optional nodes, use extended formula:
  // sum(cut) >= 1 - optional_loop_in - optional_loop_out
  // where optional_loop_in's node is in subset, optional_loop_out's is out.
  // TODO(user): Favor optional loops fixed to zero at root.
  int num_optional_nodes_in = 0;
  int num_optional_nodes_out = 0;
  int optional_loop_in = -1;
  int optional_loop_out = -1;
  for (int i = 0; i < tails_.size(); ++i) {
    if (tails_[i] != heads_[i]) continue;
    if (in_subset[tails_[i]]) {
      num_optional_nodes_in++;
      if (optional_loop_in == -1 ||
          literal_lp_values_[i] < literal_lp_values_[optional_loop_in]) {
        optional_loop_in = i;
      }
    } else {
      num_optional_nodes_out++;
      if (optional_loop_out == -1 ||
          literal_lp_values_[i] < literal_lp_values_[optional_loop_out]) {
        optional_loop_out = i;
      }
    }
  }

  // TODO(user): The lower bound for CVRP is computed assuming all nodes must be
  // served, if it is > 1 we lower it to one in the presence of optional nodes.
  if (num_optional_nodes_in + num_optional_nodes_out > 0) {
    CHECK_GE(rhs_lower_bound, 1);
    rhs_lower_bound = 1;
    ignore_arcs_with_head = -1;
  }

  // We create the cut and rely on AddCut() for computing its efficacy and
  // rejecting it if it is bad.
  LinearConstraintBuilder outgoing(encoder_, IntegerValue(rhs_lower_bound),
                                   kMaxIntegerValue);

  // Add outgoing arcs, compute outgoing flow.
  for (int i = 0; i < tails_.size(); ++i) {
    if (in_subset[tails_[i]] && !in_subset[heads_[i]]) {
      if (heads_[i] == ignore_arcs_with_head) continue;
      CHECK(outgoing.AddLiteralTerm(literals_[i], IntegerValue(1)));
    }
  }

  // Support optional nodes if any.
  if (num_optional_nodes_in + num_optional_nodes_out > 0) {
    // When all optionals of one side are excluded in lp solution, no cut.
    if (num_optional_nodes_in == subset_size &&
        (optional_loop_in == -1 ||
         literal_lp_values_[optional_loop_in] > 1.0 - 1e-6)) {
      return false;
    }
    if (num_optional_nodes_out == num_nodes_ - subset_size &&
        (optional_loop_out == -1 ||
         literal_lp_values_[optional_loop_out] > 1.0 - 1e-6)) {
      return false;
    }

    // There is no mandatory node in subset, add optional_loop_in.
    if (num_optional_nodes_in == subset_size) {
      CHECK(outgoing.AddLiteralTerm(literals_[optional_loop_in],
                                    IntegerValue(1)));
    }

    // There is no mandatory node out of subset, add optional_loop_out.
    if (num_optional_nodes_out == num_nodes_ - subset_size) {
      CHECK(outgoing.AddLiteralTerm(literals_[optional_loop_out],
                                    IntegerValue(1)));
    }
  }

  return manager_->AddCut(outgoing.Build(), name);
}

bool OutgoingCutHelper::TrySubsetCut(std::string name,
                                     absl::Span<const int> subset) {
  DCHECK_GE(subset.size(), 1);
  DCHECK_LT(subset.size(), num_nodes_);

  // These fields will be left untouched if demands.empty().
  bool contain_depot = false;
  int64_t subset_demand = 0;

  // Initialize "in_subset" and the subset demands.
  for (const int n : subset) {
    in_subset_[n] = true;
    if (!demands_.empty()) {
      if (n == 0) contain_depot = true;
      subset_demand += demands_[n];
    }
  }

  // Compute a lower bound on the outgoing flow.
  //
  // TODO(user): This lower bound assume all nodes in subset must be served.
  // If this is not the case, we are really defensive in AddOutgoingCut().
  // Improve depending on where the self-loop are.
  int64_t min_outgoing_flow = 1;

  // Bounds inferred automatically from the enforced binary relation of the
  // model.
  //
  // TODO(user): This is still not as good as the "capacity" bounds below in
  // some cases. Fix! we should be able to use the same relation to infer the
  // capacity bounds somehow.
  if ((contain_depot ? num_nodes_ - subset.size() : subset.size()) <=
      routing_cut_subset_size_for_binary_relation_bound_) {
    int automatic_bound;
    if (contain_depot) {
      complement_of_subset_.clear();
      for (int i = 0; i < num_nodes_; ++i) {
        if (!in_subset_[i]) complement_of_subset_.push_back(i);
      }
      automatic_bound = min_outgoing_flow_helper_.ComputeMinOutgoingFlow(
          complement_of_subset_);
    } else {
      automatic_bound =
          min_outgoing_flow_helper_.ComputeMinOutgoingFlow(subset);
    }
    if (automatic_bound > min_outgoing_flow) {
      absl::StrAppend(&name, "Automatic");
      min_outgoing_flow = automatic_bound;
    }
  }

  // Bounds coming from the demands_/capacity_ fields (if set).
  std::vector<int> to_ignore_candidates;
  if (!demands_.empty()) {
    const int64_t capacity_bound =
        contain_depot
            ? MathUtil::CeilOfRatio(total_demand_ - subset_demand, capacity_)
            : MathUtil::CeilOfRatio(subset_demand, capacity_);
    if (capacity_bound > min_outgoing_flow) {
      min_outgoing_flow = capacity_bound;
      absl::StrAppend(&name, "Capacity");
    }

    // It is possible to make this cut stronger, using similar reasoning to the
    // Multistar CVRP cuts: if there is a node n (other than the depot) outside
    // of `subset`, with a demand that causes subset_demand + n_demand to
    // strictly increase the `capacity_bound` above, then we still need
    // `capacity_bound` on the outgoing arcs of `subset` other than those
    // towards n (noted A in the diagram below):
    //
    //         ^       ^
    //         |       |
    //    ----------------
    // <--|  subset  | n |-->
    //    ----------------
    //         |       |
    //         v       v
    //
    // \------A------/\--B--/
    //
    // By hypothesis, outgoing_flow(A) + outgoing_flow(B) > capacity_bound
    // and, since n is not the depot, outgoing_flow(B) <= 1. Hence
    // outgoing_flow(A) >= capacity_bound.
    if (!contain_depot && capacity_bound >= min_outgoing_flow) {
      for (int n = 1; n < num_nodes_; ++n) {
        if (in_subset_[n]) continue;
        if (MathUtil::CeilOfRatio(subset_demand + demands_[n], capacity_) >
            capacity_bound) {
          to_ignore_candidates.push_back(n);
        }
      }
    }
  }

  // Out of to_ignore_candidates, use an heuristic to pick one.
  int ignore_arcs_with_head = -1;
  if (!to_ignore_candidates.empty()) {
    absl::StrAppend(&name, "Lifted");

    // Compute the lp weight going from subset to the candidates.
    absl::flat_hash_map<int, double> candidate_weights;
    for (const int n : to_ignore_candidates) candidate_weights[n] = 0;
    for (const auto arc : relevant_arcs_) {
      if (in_subset_[arc.tail] && !in_subset_[arc.head]) {
        auto it = candidate_weights.find(arc.head);
        if (it == candidate_weights.end()) continue;
        it->second += arc.lp_value;
      }
    }

    // Pick the set of candidates with the highest lp weight.
    std::vector<int> bests;
    double best_weight = 0.0;
    for (const int n : to_ignore_candidates) {
      const double weight = candidate_weights.at(n);
      if (bests.empty() || weight > best_weight) {
        bests.clear();
        bests.push_back(n);
        best_weight = weight;
      } else if (weight == best_weight) {
        bests.push_back(n);
      }
    }

    // Randomly pick if we have many "bests".
    ignore_arcs_with_head =
        bests.size() == 1
            ? bests[0]
            : bests[absl::Uniform<int>(*random_, 0, bests.size())];
  }

  // Compute the current outgoing flow out of the subset.
  //
  // This can take a significant portion of the running time, it is why it is
  // faster to do it only on arcs with non-zero lp values which should be in
  // linear number rather than the total number of arc which can be quadratic.
  //
  // TODO(user): For the symmetric case there is an even faster algo. See if
  // it can be generalized to the asymmetric one if become needed.
  // Reference is algo 6.4 of the "The Traveling Salesman Problem" book
  // mentionned above.
  double outgoing_flow = 0.0;
  for (const auto arc : relevant_arcs_) {
    if (in_subset_[arc.tail] && !in_subset_[arc.head]) {
      if (arc.head == ignore_arcs_with_head) continue;
      outgoing_flow += arc.lp_value;
    }
  }

  // Add a cut if the current outgoing flow is not enough.
  bool result = false;
  if (outgoing_flow + 1e-2 < min_outgoing_flow) {
    result = AddOutgoingCut(name, subset.size(), in_subset_,
                            /*rhs_lower_bound=*/min_outgoing_flow,
                            ignore_arcs_with_head);
  }

  // Sparse clean up.
  for (const int n : subset) in_subset_[n] = false;

  return result;
}

bool OutgoingCutHelper::TryBlossomSubsetCut(
    std::string name, absl::Span<const ArcWithLpValue> symmetrized_edges,
    absl::Span<const int> subset) {
  DCHECK_GE(subset.size(), 1);
  DCHECK_LT(subset.size(), num_nodes_);

  // Initialize "in_subset" and the subset demands.
  for (const int n : subset) in_subset_[n] = true;
  auto cleanup = ::absl::MakeCleanup([subset, this]() {
    for (const int n : subset) in_subset_[n] = false;
  });

  // The heuristic assumes non-duplicates arcs, otherwise they are all bundled
  // together in the same symmetric edge, and the result is probably wrong.
  absl::flat_hash_set<std::pair<int, int>> special_edges;
  int num_inverted = 0;
  double sum_inverted = 0.0;
  double sum = 0.0;
  double best_change = 1.0;
  ArcWithLpValue const* best_swap = nullptr;
  for (const ArcWithLpValue& arc : symmetrized_edges) {
    if (in_subset_[arc.tail] == in_subset_[arc.head]) continue;

    if (arc.lp_value > 0.5) {
      ++num_inverted;
      special_edges.insert({arc.tail, arc.head});
      sum_inverted += 1.0 - arc.lp_value;
    } else {
      sum += arc.lp_value;
    }

    const double change = std::abs(2 * arc.lp_value - 1.0);
    if (change < best_change) {
      best_change = change;
      best_swap = &arc;
    }
  }

  // If the we don't have an odd number, we move the best edge from one set to
  // the other.
  if (num_inverted % 2 == 0) {
    if (best_swap == nullptr) return false;
    if (special_edges.contains({best_swap->tail, best_swap->head})) {
      --num_inverted;
      special_edges.erase({best_swap->tail, best_swap->head});
      sum_inverted -= (1.0 - best_swap->lp_value);
      sum += best_swap->lp_value;
    } else {
      ++num_inverted;
      special_edges.insert({best_swap->tail, best_swap->head});
      sum_inverted += (1.0 - best_swap->lp_value);
      sum -= best_swap->lp_value;
    }
  }
  if (sum + sum_inverted > 0.99) return false;

  // For the route constraint, it is actually allowed to have circuit of size
  // 2, so the reasoning is wrong if one of the edges touches the depot.
  if (!demands_.empty()) {
    for (const auto [tail, head] : special_edges) {
      if (tail == 0) return false;
    }
  }

  // If there is just one special edge, and all other node can be ignored, then
  // the reasonning is wrong too since we can have a 2-cycle. In that case
  // we enforce the constraint when an extra self-loop literal is at zero.
  int best_optional_index = -1;
  if (special_edges.size() == 1) {
    int num_other_optional = 0;
    const auto [special_tail, special_head] = *special_edges.begin();
    for (int i = 0; i < tails_.size(); ++i) {
      if (tails_[i] != heads_[i]) continue;
      if (tails_[i] != special_head && tails_[i] != special_tail) {
        ++num_other_optional;
        if (best_optional_index == -1 ||
            literal_lp_values_[i] < literal_lp_values_[best_optional_index]) {
          best_optional_index = i;
        }
      }
    }
    if (num_other_optional + 2 < num_nodes_) best_optional_index = -1;
  }

  // Try to generate the cut.
  //
  // We deal with the corner case with duplicate arcs, or just one side of a
  // "symmetric" edge present.
  int num_actual_inverted = 0;
  absl::flat_hash_set<std::pair<int, int>> processed_arcs;
  LinearConstraintBuilder builder(encoder_, IntegerValue(1), kMaxIntegerValue);

  // Add extra self-loop at zero enforcement if needed.
  // TODO(user): Can we deal with the 2-cycle case in a better way?
  if (best_optional_index != -1) {
    absl::StrAppend(&name, "_opt");

    // This is tricky: The normal cut assume x_e <=1, but in case of a single
    // 2 cycle, x_e can be equal to 2. So we need a coeff of 2 to disable that
    // cut.
    CHECK(builder.AddLiteralTerm(literals_[best_optional_index],
                                 IntegerValue(2)));
  }

  for (int i = 0; i < tails_.size(); ++i) {
    if (tails_[i] == heads_[i]) continue;
    if (in_subset_[tails_[i]] == in_subset_[heads_[i]]) continue;

    const std::pair<int, int> key = {tails_[i], heads_[i]};
    const std::pair<int, int> r_key = {heads_[i], tails_[i]};
    const std::pair<int, int> s_key = std::min(key, r_key);
    if (special_edges.contains(s_key) && !processed_arcs.contains(key)) {
      processed_arcs.insert(key);
      CHECK(builder.AddLiteralTerm(literals_[i], IntegerValue(-1)));
      if (!processed_arcs.contains(r_key)) {
        ++num_actual_inverted;
      }
      continue;
    }

    // Normal edge.
    CHECK(builder.AddLiteralTerm(literals_[i], IntegerValue(1)));
  }
  builder.AddConstant(IntegerValue(num_actual_inverted));
  if (num_actual_inverted % 2 == 0) return false;

  return manager_->AddCut(builder.Build(), name);
}

}  // namespace

void GenerateInterestingSubsets(int num_nodes,
                                absl::Span<const std::pair<int, int>> arcs,
                                int stop_at_num_components,
                                std::vector<int>* subset_data,
                                std::vector<absl::Span<const int>>* subsets) {
  // We will do a union-find by adding one by one the arc of the lp solution
  // in the order above. Every intermediate set during this construction will
  // be a candidate for a cut.
  //
  // In parallel to the union-find, to efficiently reconstruct these sets (at
  // most num_nodes), we construct a "decomposition forest" of the different
  // connected components. Note that we don't exploit any asymmetric nature of
  // the graph here. This is exactly the algo 6.3 in the book above.
  int num_components = num_nodes;
  std::vector<int> parent(num_nodes);
  std::vector<int> root(num_nodes);
  for (int i = 0; i < num_nodes; ++i) {
    parent[i] = i;
    root[i] = i;
  }
  auto get_root_and_compress_path = [&root](int node) {
    int r = node;
    while (root[r] != r) r = root[r];
    while (root[node] != r) {
      const int next = root[node];
      root[node] = r;
      node = next;
    }
    return r;
  };
  for (const auto& [initial_tail, initial_head] : arcs) {
    if (num_components <= stop_at_num_components) break;
    const int tail = get_root_and_compress_path(initial_tail);
    const int head = get_root_and_compress_path(initial_head);
    if (tail != head) {
      // Update the decomposition forest, note that the number of nodes is
      // growing.
      const int new_node = parent.size();
      parent.push_back(new_node);
      parent[head] = new_node;
      parent[tail] = new_node;
      --num_components;

      // It is important that the union-find representative is the same node.
      root.push_back(new_node);
      root[head] = new_node;
      root[tail] = new_node;
    }
  }

  // For each node in the decomposition forest, try to add a cut for the set
  // formed by the nodes and its children. To do that efficiently, we first
  // order the nodes so that for each node in a tree, the set of children forms
  // a consecutive span in the subset_data vector. This vector just lists the
  // nodes in the "pre-order" graph traversal order. The Spans will point inside
  // the subset_data vector, it is why we initialize it once and for all.
  ExtractAllSubsetsFromForest(parent, subset_data, subsets,
                              /*node_limit=*/num_nodes);
}

void ExtractAllSubsetsFromForest(absl::Span<const int> parent,
                                 std::vector<int>* subset_data,
                                 std::vector<absl::Span<const int>>* subsets,
                                 int node_limit) {
  // To not reallocate memory since we need the span to point inside this
  // vector, we resize subset_data right away.
  int out_index = 0;
  const int num_nodes = parent.size();
  subset_data->resize(std::min(num_nodes, node_limit));
  subsets->clear();

  // Starts by creating the corresponding graph and find the root.
  util::StaticGraph<int> graph(num_nodes, num_nodes - 1);
  for (int i = 0; i < num_nodes; ++i) {
    if (parent[i] != i) {
      graph.AddArc(parent[i], i);
    }
  }
  graph.Build();

  // Perform a dfs on the rooted tree.
  // The subset_data will just be the node in post-order.
  std::vector<int> subtree_starts(num_nodes, -1);
  std::vector<int> stack;
  stack.reserve(num_nodes);
  for (int i = 0; i < parent.size(); ++i) {
    if (parent[i] != i) continue;

    stack.push_back(i);  // root.
    while (!stack.empty()) {
      const int node = stack.back();

      // The node was already explored, output its subtree and pop it.
      if (subtree_starts[node] >= 0) {
        stack.pop_back();
        if (node < node_limit) {
          (*subset_data)[out_index++] = node;
        }
        const int start = subtree_starts[node];
        const int size = out_index - start;
        subsets->push_back(absl::MakeSpan(&(*subset_data)[start], size));
        continue;
      }

      // Explore.
      subtree_starts[node] = out_index;
      for (const int child : graph[node]) {
        stack.push_back(child);
      }
    }
  }
}

std::vector<int> ComputeGomoryHuTree(
    int num_nodes, absl::Span<const ArcWithLpValue> relevant_arcs) {
  // Initialize the graph. Note that we use only arcs with a relevant lp
  // value, so this should be small in practice.
  SimpleMaxFlow max_flow;
  for (const auto& [tail, head, lp_value] : relevant_arcs) {
    max_flow.AddArcWithCapacity(tail, head, std::round(1.0e6 * lp_value));
    max_flow.AddArcWithCapacity(head, tail, std::round(1.0e6 * lp_value));
  }

  // Compute an equivalent max-flow tree, according to the paper.
  // This version should actually produce a Gomory-Hu cut tree.
  std::vector<int> min_cut_subset;
  std::vector<int> parent(num_nodes, 0);
  for (int s = 1; s < num_nodes; ++s) {
    const int t = parent[s];
    if (max_flow.Solve(s, t) != SimpleMaxFlow::OPTIMAL) break;
    max_flow.GetSourceSideMinCut(&min_cut_subset);
    bool parent_of_t_in_subset = false;
    for (const int i : min_cut_subset) {
      if (i == parent[t]) parent_of_t_in_subset = true;
      if (i != s && parent[i] == t) parent[i] = s;
    }
    if (parent_of_t_in_subset) {
      parent[s] = parent[t];
      parent[t] = s;
    }
  }

  return parent;
}

void SymmetrizeArcs(std::vector<ArcWithLpValue>* arcs) {
  for (ArcWithLpValue& arc : *arcs) {
    if (arc.tail <= arc.head) continue;
    std::swap(arc.tail, arc.head);
  }
  std::sort(arcs->begin(), arcs->end(),
            [](const ArcWithLpValue& a, const ArcWithLpValue& b) {
              return std::tie(a.tail, a.head) < std::tie(b.tail, b.head);
            });

  int new_size = 0;
  int last_tail = -1;
  int last_head = -1;
  for (const ArcWithLpValue& arc : *arcs) {
    if (arc.tail == last_tail && arc.head == last_head) {
      (*arcs)[new_size - 1].lp_value += arc.lp_value;
      continue;
    }
    (*arcs)[new_size++] = arc;
    last_tail = arc.tail;
    last_head = arc.head;
  }
  arcs->resize(new_size);
}

// We roughly follow the algorithm described in section 6 of "The Traveling
// Salesman Problem, A computational Study", David L. Applegate, Robert E.
// Bixby, Vasek Chvatal, William J. Cook.
//
// Note that this is mainly a "symmetric" case algo, but it does still work for
// the asymmetric case.
void SeparateSubtourInequalities(
    int num_nodes, const std::vector<int>& tails, const std::vector<int>& heads,
    const std::vector<Literal>& literals, absl::Span<const int64_t> demands,
    int64_t capacity, LinearConstraintManager* manager, Model* model) {
  if (num_nodes <= 2) return;

  // We will collect only the arcs with a positive lp_values to speed up some
  // computation below.
  std::vector<ArcWithLpValue> relevant_arcs;

  // Sort the arcs by non-increasing lp_values.
  const auto& lp_values = manager->LpValues();
  std::vector<double> literal_lp_values(literals.size());
  std::vector<std::pair<double, int>> arc_by_decreasing_lp_values;
  auto* encoder = model->GetOrCreate<IntegerEncoder>();
  for (int i = 0; i < literals.size(); ++i) {
    double lp_value;
    const IntegerVariable direct_view = encoder->GetLiteralView(literals[i]);
    if (direct_view != kNoIntegerVariable) {
      lp_value = lp_values[direct_view];
    } else {
      lp_value =
          1.0 - lp_values[encoder->GetLiteralView(literals[i].Negated())];
    }
    literal_lp_values[i] = lp_value;

    if (lp_value < 1e-6) continue;
    relevant_arcs.push_back({tails[i], heads[i], lp_value});
    arc_by_decreasing_lp_values.push_back({lp_value, i});
  }
  std::sort(arc_by_decreasing_lp_values.begin(),
            arc_by_decreasing_lp_values.end(),
            std::greater<std::pair<double, int>>());

  std::vector<std::pair<int, int>> ordered_arcs;
  for (const auto& [score, arc] : arc_by_decreasing_lp_values) {
    ordered_arcs.push_back({tails[arc], heads[arc]});
  }
  std::vector<int> subset_data;
  std::vector<absl::Span<const int>> subsets;
  GenerateInterestingSubsets(num_nodes, ordered_arcs,
                             /*stop_at_num_components=*/2, &subset_data,
                             &subsets);

  const int depot = 0;
  if (!demands.empty()) {
    // Add the depot so that we have a trivial bound on the number of
    // vehicle.
    subsets.push_back(absl::MakeSpan(&depot, 1));
  }

  OutgoingCutHelper helper(num_nodes, capacity, demands, tails, heads, literals,
                           literal_lp_values, relevant_arcs, manager, model);

  // Hack/optim: we exploit the tree structure of the subsets to not add a cut
  // for a larger subset if we added a cut from one included in it.
  //
  // TODO(user): Currently if we add too many not so relevant cuts, our generic
  // MIP cut heuritic are way too slow on TSP/VRP problems.
  int last_added_start = -1;

  // Process each subsets and add any violated cut.
  int num_added = 0;
  for (const absl::Span<const int> subset : subsets) {
    if (subset.size() <= 1) continue;
    const int start = static_cast<int>(subset.data() - subset_data.data());
    if (start <= last_added_start) continue;
    if (helper.TrySubsetCut("Circuit", subset)) {
      ++num_added;
      last_added_start = start;
    }
  }

  // If there were no cut added by the heuristic above, we try exact separation.
  //
  // With n-1 max_flow from a source to all destination, we can get the global
  // min-cut. Here, we use a slightly more advanced algorithm that will find a
  // min-cut for all possible pair of nodes. This is achieved by computing a
  // Gomory-Hu tree, still with n-1 max flow call.
  //
  // Note(user): Compared to any min-cut, these cut have some nice properties
  // since they are "included" in each other. This might help with combining
  // them within our generic IP cuts framework.
  //
  // TODO(user): I had an older version that tried the n-cuts generated during
  // the course of the algorithm. This could also be interesting. But it is
  // hard to tell with our current benchmark setup.
  if (num_added != 0) return;
  SymmetrizeArcs(&relevant_arcs);
  std::vector<int> parent = ComputeGomoryHuTree(num_nodes, relevant_arcs);

  // Try all interesting subset from the Gomory-Hu tree.
  ExtractAllSubsetsFromForest(parent, &subset_data, &subsets);
  last_added_start = -1;
  for (const absl::Span<const int> subset : subsets) {
    if (subset.size() <= 1) continue;
    if (subset.size() == num_nodes) continue;
    const int start = static_cast<int>(subset.data() - subset_data.data());
    if (start <= last_added_start) continue;
    if (helper.TrySubsetCut("CircuitExact", subset)) {
      ++num_added;
      last_added_start = start;
    }
  }

  // Exact separation of symmetric Blossom cut. We use the algorithm in the
  // paper: "A Faster Exact Separation Algorithm for Blossom Inequalities", Adam
  // N. Letchford, Gerhard Reinelt, Dirk Oliver Theis, 2004.
  //
  // Note that the "relevant_arcs" were symmetrized above.
  if (num_added != 0) return;
  if (num_nodes <= 2) return;
  std::vector<ArcWithLpValue> for_blossom;
  for_blossom.reserve(relevant_arcs.size());
  for (ArcWithLpValue arc : relevant_arcs) {
    if (arc.lp_value > 0.5) arc.lp_value = 1.0 - arc.lp_value;
    if (arc.lp_value < 1e-6) continue;
    for_blossom.push_back(arc);
  }
  parent = ComputeGomoryHuTree(num_nodes, for_blossom);
  ExtractAllSubsetsFromForest(parent, &subset_data, &subsets);
  last_added_start = -1;
  for (const absl::Span<const int> subset : subsets) {
    if (subset.size() <= 1) continue;
    if (subset.size() == num_nodes) continue;
    const int start = static_cast<int>(subset.data() - subset_data.data());
    if (start <= last_added_start) continue;
    if (helper.TryBlossomSubsetCut("CircuitBlossom", relevant_arcs, subset)) {
      ++num_added;
      last_added_start = start;
    }
  }
}

namespace {

// Returns for each literal its integer view, or the view of its negation.
std::vector<IntegerVariable> GetAssociatedVariables(
    absl::Span<const Literal> literals, Model* model) {
  auto* encoder = model->GetOrCreate<IntegerEncoder>();
  std::vector<IntegerVariable> result;
  for (const Literal l : literals) {
    const IntegerVariable direct_view = encoder->GetLiteralView(l);
    if (direct_view != kNoIntegerVariable) {
      result.push_back(direct_view);
    } else {
      result.push_back(encoder->GetLiteralView(l.Negated()));
      DCHECK_NE(result.back(), kNoIntegerVariable);
    }
  }
  return result;
}

// This is especially useful to remove fixed self loop.
void FilterFalseArcsAtLevelZero(std::vector<int>& tails,
                                std::vector<int>& heads,
                                std::vector<Literal>& literals, Model* model) {
  const Trail& trail = *model->GetOrCreate<Trail>();
  if (trail.CurrentDecisionLevel() != 0) return;

  int new_size = 0;
  const int size = static_cast<int>(tails.size());
  const VariablesAssignment& assignment = trail.Assignment();
  for (int i = 0; i < size; ++i) {
    if (assignment.LiteralIsFalse(literals[i])) continue;
    tails[new_size] = tails[i];
    heads[new_size] = heads[i];
    literals[new_size] = literals[i];
    ++new_size;
  }
  if (new_size < size) {
    tails.resize(new_size);
    heads.resize(new_size);
    literals.resize(new_size);
  }
}

}  // namespace

// We use a basic algorithm to detect components that are not connected to the
// rest of the graph in the LP solution, and add cuts to force some arcs to
// enter and leave this component from outside.
CutGenerator CreateStronglyConnectedGraphCutGenerator(
    int num_nodes, std::vector<int> tails, std::vector<int> heads,
    std::vector<Literal> literals, Model* model) {
  CutGenerator result;
  result.vars = GetAssociatedVariables(literals, model);
  result.generate_cuts = [=](LinearConstraintManager* manager) mutable {
    FilterFalseArcsAtLevelZero(tails, heads, literals, model);
    SeparateSubtourInequalities(num_nodes, tails, heads, literals,
                                /*demands=*/{}, /*capacity=*/0, manager, model);
    return true;
  };
  return result;
}

CutGenerator CreateCVRPCutGenerator(int num_nodes, std::vector<int> tails,
                                    std::vector<int> heads,
                                    std::vector<Literal> literals,
                                    std::vector<int64_t> demands,
                                    int64_t capacity, Model* model) {
  CutGenerator result;
  result.vars = GetAssociatedVariables(literals, model);
  result.generate_cuts = [=](LinearConstraintManager* manager) mutable {
    FilterFalseArcsAtLevelZero(tails, heads, literals, model);
    SeparateSubtourInequalities(num_nodes, tails, heads, literals, demands,
                                capacity, manager, model);
    return true;
  };
  return result;
}

// This is really similar to SeparateSubtourInequalities, see the reference
// there.
void SeparateFlowInequalities(
    int num_nodes, absl::Span<const int> tails, absl::Span<const int> heads,
    absl::Span<const AffineExpression> arc_capacities,
    std::function<void(const std::vector<bool>& in_subset,
                       IntegerValue* min_incoming_flow,
                       IntegerValue* min_outgoing_flow)>
        get_flows,
    const util_intops::StrongVector<IntegerVariable, double>& lp_values,
    LinearConstraintManager* manager, Model* model) {
  // We will collect only the arcs with a positive lp capacity value to speed up
  // some computation below.
  struct Arc {
    int tail;
    int head;
    double lp_value;
    IntegerValue offset;
  };
  std::vector<Arc> relevant_arcs;

  // Often capacities have a coeff > 1.
  // We currently exploit this if all coeff have a gcd > 1.
  int64_t gcd = 0;

  // Sort the arcs by non-increasing lp_values.
  std::vector<std::pair<double, int>> arc_by_decreasing_lp_values;
  for (int i = 0; i < arc_capacities.size(); ++i) {
    const double lp_value = arc_capacities[i].LpValue(lp_values);
    if (!arc_capacities[i].IsConstant()) {
      gcd = MathUtil::GCD64(gcd, std::abs(arc_capacities[i].coeff.value()));
    }
    if (lp_value < 1e-6 && arc_capacities[i].constant == 0) continue;
    relevant_arcs.push_back(
        {tails[i], heads[i], lp_value, arc_capacities[i].constant});
    arc_by_decreasing_lp_values.push_back({lp_value, i});
  }
  if (gcd == 0) return;
  std::sort(arc_by_decreasing_lp_values.begin(),
            arc_by_decreasing_lp_values.end(),
            std::greater<std::pair<double, int>>());

  std::vector<std::pair<int, int>> ordered_arcs;
  for (const auto& [score, arc] : arc_by_decreasing_lp_values) {
    if (tails[arc] == -1) continue;
    if (heads[arc] == -1) continue;
    ordered_arcs.push_back({tails[arc], heads[arc]});
  }
  std::vector<int> subset_data;
  std::vector<absl::Span<const int>> subsets;
  GenerateInterestingSubsets(num_nodes, ordered_arcs,
                             /*stop_at_num_components=*/1, &subset_data,
                             &subsets);

  // Process each subsets and add any violated cut.
  std::vector<bool> in_subset(num_nodes, false);
  for (const absl::Span<const int> subset : subsets) {
    DCHECK(!subset.empty());
    DCHECK_LE(subset.size(), num_nodes);

    // Initialize "in_subset" and the subset demands.
    for (const int n : subset) in_subset[n] = true;

    IntegerValue min_incoming_flow;
    IntegerValue min_outgoing_flow;
    get_flows(in_subset, &min_incoming_flow, &min_outgoing_flow);

    // We will sum the offset of all incoming/outgoing arc capacities.
    // Note that all arcs with a non-zero offset are part of relevant_arcs.
    IntegerValue incoming_offset(0);
    IntegerValue outgoing_offset(0);

    // Compute the current flow in and out of the subset.
    //
    // This can take a significant portion of the running time, it is why it is
    // faster to do it only on arcs with non-zero lp values which should be in
    // linear number rather than the total number of arc which can be quadratic.
    double lp_outgoing_flow = 0.0;
    double lp_incoming_flow = 0.0;
    for (const auto arc : relevant_arcs) {
      if (arc.tail != -1 && in_subset[arc.tail]) {
        if (arc.head == -1 || !in_subset[arc.head]) {
          incoming_offset += arc.offset;
          lp_outgoing_flow += arc.lp_value;
        }
      } else {
        if (arc.head != -1 && in_subset[arc.head]) {
          outgoing_offset += arc.offset;
          lp_incoming_flow += arc.lp_value;
        }
      }
    }

    // If the gcd is greater than one, because all variables are integer we
    // can round the flow lower bound to the next multiple of the gcd.
    //
    // TODO(user): Alternatively, try MIR heuristics if the coefficients in
    // the capacities are not all the same.
    if (gcd > 1) {
      const IntegerValue test_incoming = min_incoming_flow - incoming_offset;
      const IntegerValue new_incoming =
          CeilRatio(test_incoming, IntegerValue(gcd)) * IntegerValue(gcd);
      const IntegerValue incoming_delta = new_incoming - test_incoming;
      if (incoming_delta > 0) min_incoming_flow += incoming_delta;
    }
    if (gcd > 1) {
      const IntegerValue test_outgoing = min_outgoing_flow - outgoing_offset;
      const IntegerValue new_outgoing =
          CeilRatio(test_outgoing, IntegerValue(gcd)) * IntegerValue(gcd);
      const IntegerValue outgoing_delta = new_outgoing - test_outgoing;
      if (outgoing_delta > 0) min_outgoing_flow += outgoing_delta;
    }

    if (lp_incoming_flow < ToDouble(min_incoming_flow) - 1e-6) {
      VLOG(2) << "INCOMING CUT " << lp_incoming_flow
              << " >= " << min_incoming_flow << " size " << subset.size()
              << " offset " << incoming_offset << " gcd " << gcd;
      LinearConstraintBuilder cut(model, min_incoming_flow, kMaxIntegerValue);
      for (int i = 0; i < tails.size(); ++i) {
        if ((tails[i] == -1 || !in_subset[tails[i]]) &&
            (heads[i] != -1 && in_subset[heads[i]])) {
          cut.AddTerm(arc_capacities[i], 1.0);
        }
      }
      manager->AddCut(cut.Build(), "IncomingFlow");
    }

    if (lp_outgoing_flow < ToDouble(min_outgoing_flow) - 1e-6) {
      VLOG(2) << "OUGOING CUT " << lp_outgoing_flow
              << " >= " << min_outgoing_flow << " size " << subset.size()
              << " offset " << outgoing_offset << " gcd " << gcd;
      LinearConstraintBuilder cut(model, min_outgoing_flow, kMaxIntegerValue);
      for (int i = 0; i < tails.size(); ++i) {
        if ((tails[i] != -1 && in_subset[tails[i]]) &&
            (heads[i] == -1 || !in_subset[heads[i]])) {
          cut.AddTerm(arc_capacities[i], 1.0);
        }
      }
      manager->AddCut(cut.Build(), "OutgoingFlow");
    }

    // Sparse clean up.
    for (const int n : subset) in_subset[n] = false;
  }
}

CutGenerator CreateFlowCutGenerator(
    int num_nodes, const std::vector<int>& tails, const std::vector<int>& heads,
    const std::vector<AffineExpression>& arc_capacities,
    std::function<void(const std::vector<bool>& in_subset,
                       IntegerValue* min_incoming_flow,
                       IntegerValue* min_outgoing_flow)>
        get_flows,
    Model* model) {
  CutGenerator result;
  for (const AffineExpression expr : arc_capacities) {
    if (!expr.IsConstant()) result.vars.push_back(expr.var);
  }
  result.generate_cuts = [=](LinearConstraintManager* manager) {
    SeparateFlowInequalities(num_nodes, tails, heads, arc_capacities, get_flows,
                             manager->LpValues(), manager, model);
    return true;
  };
  return result;
}

}  // namespace sat
}  // namespace operations_research
