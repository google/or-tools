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
#include <memory>
#include <numeric>
#include <string>
#include <tuple>
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
#include "ortools/graph/graph.h"
#include "ortools/graph/max_flow.h"
#include "ortools/sat/cuts.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/model.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/util.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

namespace {

// If we have many sets of (var >= values relations) and we know at least one
// set is true, then we can derive valid global lower bounds on the variable
// that appear in all such sets.
//
// This represents "one step" of computing such bounds by adding the sets one at
// the time. When we process a set 'new_lbs':
// - For the first set, then 'new_lbs' is globally valid.
// - For a new set, we need to erase variables not appearing in new_lbs and
//   take the min for the ones appearing in both.
//
// TODO(user): The operation is symmetric, so we could swap both hash_map if
// new_lbs is smaller than current_lbs as a small optimization.
void ComputeMinLowerBoundOfSharedVariables(
    const absl::flat_hash_map<IntegerVariable, IntegerValue>& new_lbs,
    absl::flat_hash_map<IntegerVariable, IntegerValue>* current_lbs) {
  if (new_lbs.empty()) current_lbs->clear();
  for (auto it = current_lbs->begin(); it != current_lbs->end();) {
    const IntegerVariable var = it->first;
    auto other_it = new_lbs.find(var);
    if (other_it == new_lbs.end()) {
      // We have no bounds about var in the new_fact, we need to erase it.
      //
      // Note: it = current_lbs->erase(it) does not work for flat_hash_map, this
      // is the recommended way.
      current_lbs->erase(it++);
    } else {
      // Update.
      it->second = std::min(it->second, other_it->second);
      ++it;
    }
  }
}

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
      index_in_subset_(num_nodes, -1),
      incoming_arc_indices_(num_nodes),
      outgoing_arc_indices_(num_nodes),
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
  absl::flat_hash_map<IntegerVariable, IntegerValue> tmp_lbs;
  while (longest_path_length < subset.size()) {
    bool at_least_one_next_node_reachable = false;
    for (const int head : subset) {
      bool is_reachable = false;
      for (const int incoming_arc_index : incoming_arc_indices_[head]) {
        const int tail = tails_[incoming_arc_index];
        const Literal lit = literals_[incoming_arc_index];
        if (!reachable_[tail]) continue;

        // If this arc cannot be taken skip.
        tmp_lbs.clear();
        if (!binary_relation_repository_.PropagateLocalBounds(
                integer_trail_, lit, node_var_lower_bounds_[tail], &tmp_lbs)) {
          continue;
        }

        if (!is_reachable) {
          // This is the first arc that reach this node.
          is_reachable = true;
          next_node_var_lower_bounds_[head] = tmp_lbs;
        } else {
          // Combine information with previous one.
          ComputeMinLowerBoundOfSharedVariables(
              tmp_lbs, &next_node_var_lower_bounds_[head]);
        }
      }

      next_reachable_[head] = is_reachable;
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
  return GetMinOutgoingFlow(subset.size(), longest_path_length,
                            max_longest_paths);
}

int MinOutgoingFlowHelper::GetMinOutgoingFlow(int subset_size,
                                              int longest_path_length,
                                              int max_longest_paths) {
  if (max_longest_paths * longest_path_length < subset_size) {
    // If `longest_path_length` is 1, there should be one such path per node,
    // and thus `max_longest_paths` should be the subset size. This branch can
    // thus not be taken in this case.
    DCHECK_GT(longest_path_length, 1);
    // TODO(user): Consider using information about the number of shorter
    // paths to derive an even better bound.
    return max_longest_paths +
           MathUtil::CeilOfRatio(
               subset_size - max_longest_paths * longest_path_length,
               longest_path_length - 1);
  }
  return MathUtil::CeilOfRatio(subset_size, longest_path_length);
}

namespace {
struct Path {
  uint32_t node_set;  // Bit i is set iif node subset[i] is in the path.
  int last_node;      // The last node in the path.

  bool operator==(const Path& p) const {
    return node_set == p.node_set && last_node == p.last_node;
  }

  template <typename H>
  friend H AbslHashValue(H h, const Path& p) {
    return H::combine(std::move(h), p.node_set, p.last_node);
  }
};

struct PathVariableBounds {
  absl::flat_hash_set<int> incoming_arc_indices;
  absl::flat_hash_map<IntegerVariable, absl::flat_hash_map<int, IntegerValue>>
      lower_bound_by_var_and_arc_index;
};
}  // namespace

int MinOutgoingFlowHelper::ComputeTightMinOutgoingFlow(
    absl::Span<const int> subset) {
  DCHECK_GE(subset.size(), 1);
  DCHECK_LE(subset.size(), 32);
  DCHECK(absl::c_all_of(index_in_subset_, [](int i) { return i == -1; }));
  DCHECK(absl::c_all_of(outgoing_arc_indices_,
                        [](const auto& v) { return v.empty(); }));

  std::vector<int> longest_path_length_by_end_node(subset.size(), 1);
  for (int i = 0; i < subset.size(); ++i) {
    index_in_subset_[subset[i]] = i;
  }
  for (int i = 0; i < tails_.size(); ++i) {
    if (index_in_subset_[tails_[i]] != -1 &&
        index_in_subset_[heads_[i]] != -1 && heads_[i] != tails_[i]) {
      outgoing_arc_indices_[tails_[i]].push_back(i);
    }
  }

  absl::flat_hash_map<Path, PathVariableBounds> path_var_bounds;
  std::vector<Path> paths;
  std::vector<Path> next_paths;
  for (int i = 0; i < subset.size(); ++i) {
    paths.push_back(
        {.node_set = static_cast<uint32_t>(1 << i), .last_node = subset[i]});
  }
  int longest_path_length = 1;
  for (int path_length = 1; path_length <= subset.size(); ++path_length) {
    for (const Path& path : paths) {
      // Merge the bounds by variable and arc incoming to the last node of the
      // path into bounds by variable, if possible, and check whether they are
      // feasible or not.
      const auto& var_bounds = path_var_bounds[path];
      absl::flat_hash_map<IntegerVariable, IntegerValue> lower_bound_by_var;
      for (const auto& [var, lower_bound_by_arc_index] :
           var_bounds.lower_bound_by_var_and_arc_index) {
        // If each arc which can reach the last node of the path enforces some
        // lower bound for `var`, then the lower bound of `var` can be increased
        // to the minimum of these arc-specific lower bounds (since at least one
        // of these arcs must be selected to reach this node).
        if (lower_bound_by_arc_index.size() !=
            var_bounds.incoming_arc_indices.size()) {
          continue;
        }
        IntegerValue lb = lower_bound_by_arc_index.begin()->second;
        for (const auto& [_, lower_bound] : lower_bound_by_arc_index) {
          lb = std::min(lb, lower_bound);
        }
        if (lb > integer_trail_.LevelZeroLowerBound(var)) {
          lower_bound_by_var[var] = lb;
        }
      }
      path_var_bounds.erase(path);
      auto get_lower_bound = [&](IntegerVariable var) {
        const auto it = lower_bound_by_var.find(var);
        if (it != lower_bound_by_var.end()) return it->second;
        return integer_trail_.LevelZeroLowerBound(var);
      };
      auto get_upper_bound = [&](IntegerVariable var) {
        return -get_lower_bound(NegationOf(var));
      };
      bool feasible_path = true;
      for (const auto& [var, lb] : lower_bound_by_var) {
        if (get_upper_bound(var) < lb) {
          feasible_path = false;
          break;
        }
      }
      if (!feasible_path) continue;
      // We have found a feasible path, update the path length statistics...
      longest_path_length = path_length;
      longest_path_length_by_end_node[index_in_subset_[path.last_node]] =
          path_length;

      // ... and try to extend the path with each arc going out of its last
      // node.
      for (const int outgoing_arc_index :
           outgoing_arc_indices_[path.last_node]) {
        const int head = heads_[outgoing_arc_index];
        const int head_index_in_subset = index_in_subset_[head];
        DCHECK_NE(head_index_in_subset, -1);
        if (path.node_set & (1 << head_index_in_subset)) continue;
        const Path new_path = {
            .node_set = path.node_set | (1 << head_index_in_subset),
            .last_node = head};
        if (!path_var_bounds.contains(new_path)) {
          next_paths.push_back(new_path);
        }
        auto& new_var_bounds = path_var_bounds[new_path];
        new_var_bounds.incoming_arc_indices.insert(outgoing_arc_index);
        auto update_lower_bound_by_var_and_arc_index =
            [&](IntegerVariable var, int arc_index, IntegerValue lb) {
              auto& lower_bound_by_arc_index =
                  new_var_bounds.lower_bound_by_var_and_arc_index[var];
              auto it = lower_bound_by_arc_index.find(arc_index);
              if (it != lower_bound_by_arc_index.end()) {
                it->second = std::max(it->second, lb);
              } else {
                lower_bound_by_arc_index[arc_index] = lb;
              }
            };
        auto update_upper_bound_by_var_and_arc_index =
            [&](IntegerVariable var, int arc_index, IntegerValue ub) {
              update_lower_bound_by_var_and_arc_index(NegationOf(var),
                                                      arc_index, -ub);
            };
        auto update_var_bounds = [&](int arc_index, LinearTerm a, LinearTerm b,
                                     IntegerValue lhs, IntegerValue rhs) {
          if (a.coeff == 0) return;
          a.MakeCoeffPositive();
          b.MakeCoeffPositive();
          // lb(b.y) <= b.y <= ub(b.y) and lhs <= a.x + b.y <= rhs imply
          //   ceil((lhs - ub(b.y)) / a) <= x <= floor((rhs - lb(b.y)) / a)
          lhs = lhs - b.coeff * get_upper_bound(b.var);
          rhs = rhs - b.coeff * get_lower_bound(b.var);
          update_lower_bound_by_var_and_arc_index(
              a.var, arc_index, MathUtil::CeilOfRatio(lhs, a.coeff));
          update_upper_bound_by_var_and_arc_index(
              a.var, arc_index, MathUtil::FloorOfRatio(rhs, a.coeff));
        };
        const Literal lit = literals_[outgoing_arc_index];
        for (const int relation_index :
             binary_relation_repository_.relation_indices(lit)) {
          const auto& r = binary_relation_repository_.relation(relation_index);
          update_var_bounds(outgoing_arc_index, r.a, r.b, r.lhs, r.rhs);
          update_var_bounds(outgoing_arc_index, r.b, r.a, r.lhs, r.rhs);
        }
      }
    }
    std::swap(paths, next_paths);
    next_paths.clear();
  }

  int max_longest_paths = 0;
  for (int i = 0; i < subset.size(); ++i) {
    if (longest_path_length_by_end_node[i] == longest_path_length) {
      ++max_longest_paths;
    }
  }
  // Reset the temporary data structures for the next call.
  for (const int n : subset) {
    index_in_subset_[n] = -1;
    outgoing_arc_indices_[n].clear();
  }
  return GetMinOutgoingFlow(subset.size(), longest_path_length,
                            max_longest_paths);
}

namespace {

class OutgoingCutHelper {
 public:
  OutgoingCutHelper(int num_nodes, bool is_route_constraint, int64_t capacity,
                    absl::Span<const int64_t> demands,
                    absl::Span<const int> tails, absl::Span<const int> heads,
                    absl::Span<const Literal> literals, Model* model)
      : num_nodes_(num_nodes),
        is_route_constraint_(is_route_constraint),
        capacity_(capacity),
        demands_(demands.begin(), demands.end()),
        tails_(tails.begin(), tails.end()),
        heads_(heads.begin(), heads.end()),
        literals_(literals.begin(), literals.end()),
        literal_lp_values_(literals.size()),
        params_(*model->GetOrCreate<SatParameters>()),
        trail_(*model->GetOrCreate<Trail>()),
        random_(model->GetOrCreate<ModelRandomGenerator>()),
        encoder_(model->GetOrCreate<IntegerEncoder>()),
        in_subset_(num_nodes, false),
        min_outgoing_flow_helper_(num_nodes, tails_, heads_, literals_, model) {
    // Compute the total demands in order to know the minimum incoming/outgoing
    // flow.
    for (const int64_t demand : demands) total_demand_ += demand;
    complement_of_subset_.reserve(num_nodes_);
  }

  int num_nodes() const { return num_nodes_; }

  bool is_route_constraint() const { return is_route_constraint_; }

  // Returns the arcs computed by InitializeForNewLpSolution().
  absl::Span<const ArcWithLpValue> relevant_arcs() const {
    return relevant_arcs_;
  }

  // Returns the arcs computed in InitializeForNewLpSolution(), sorted by
  // decreasing lp_value.
  absl::Span<const std::pair<int, int>> ordered_arcs() const {
    return ordered_arcs_;
  }

  // This should be called each time a new LP solution is available, before
  // using the other methods.
  void InitializeForNewLpSolution(LinearConstraintManager* manager);

  // Merge the relevant arcs (tail, head) and (head, tail) into a single (tail,
  // head) arc, with the sum of their lp_values.
  absl::Span<const ArcWithLpValue> SymmetrizedRelevantArcs() {
    symmetrized_relevant_arcs_ = relevant_arcs_;
    SymmetrizeArcs(&symmetrized_relevant_arcs_);
    return symmetrized_relevant_arcs_;
  }

  // Try to add an outgoing cut from the given subset.
  bool TrySubsetCut(std::string name, absl::Span<const int> subset,
                    LinearConstraintManager* manager);

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
                           absl::Span<const int> subset,
                           LinearConstraintManager* manager);

 private:
  // Removes the arcs with a literal fixed at false at level zero. This is
  // especially useful to remove fixed self loop.
  void FilterFalseArcsAtLevelZero();

  // Add a cut of the form Sum_{outgoing arcs from S} lp >= rhs_lower_bound.
  //
  // Note that we used to also add the same cut for the incoming arcs, but
  // because of flow conservation on these problems, the outgoing flow is always
  // the same as the incoming flow, so adding this extra cut doesn't seem
  // relevant.
  bool AddOutgoingCut(LinearConstraintManager* manager, std::string name,
                      int subset_size, const std::vector<bool>& in_subset,
                      int64_t rhs_lower_bound, int ignore_arcs_with_head);

  const int num_nodes_;
  const bool is_route_constraint_;
  const int64_t capacity_;
  std::vector<int64_t> demands_;
  std::vector<int> tails_;
  std::vector<int> heads_;
  std::vector<Literal> literals_;
  std::vector<double> literal_lp_values_;
  std::vector<ArcWithLpValue> relevant_arcs_;
  std::vector<ArcWithLpValue> symmetrized_relevant_arcs_;
  std::vector<std::pair<int, int>> ordered_arcs_;

  const SatParameters& params_;
  const Trail& trail_;
  ModelRandomGenerator* random_;
  IntegerEncoder* encoder_;

  int64_t total_demand_ = 0;
  std::vector<bool> in_subset_;
  std::vector<int> complement_of_subset_;

  MaxBoundedSubsetSum max_bounded_subset_sum_;
  MaxBoundedSubsetSumExact max_bounded_subset_sum_exact_;
  MinOutgoingFlowHelper min_outgoing_flow_helper_;
};

void OutgoingCutHelper::FilterFalseArcsAtLevelZero() {
  if (trail_.CurrentDecisionLevel() != 0) return;

  int new_size = 0;
  const int size = static_cast<int>(tails_.size());
  const VariablesAssignment& assignment = trail_.Assignment();
  for (int i = 0; i < size; ++i) {
    if (assignment.LiteralIsFalse(literals_[i])) continue;
    tails_[new_size] = tails_[i];
    heads_[new_size] = heads_[i];
    literals_[new_size] = literals_[i];
    ++new_size;
  }
  if (new_size < size) {
    tails_.resize(new_size);
    heads_.resize(new_size);
    literals_.resize(new_size);
    literal_lp_values_.resize(new_size);
  }
}

void OutgoingCutHelper::InitializeForNewLpSolution(
    LinearConstraintManager* manager) {
  FilterFalseArcsAtLevelZero();

  // We will collect only the arcs with a positive lp_values to speed up some
  // computation below.
  relevant_arcs_.clear();

  // Sort the arcs by non-increasing lp_values.
  const auto& lp_values = manager->LpValues();
  std::vector<std::pair<double, int>> relevant_arc_by_decreasing_lp_values;
  for (int i = 0; i < literals_.size(); ++i) {
    double lp_value;
    const IntegerVariable direct_view = encoder_->GetLiteralView(literals_[i]);
    if (direct_view != kNoIntegerVariable) {
      lp_value = lp_values[direct_view];
    } else {
      lp_value =
          1.0 - lp_values[encoder_->GetLiteralView(literals_[i].Negated())];
    }
    literal_lp_values_[i] = lp_value;

    if (lp_value < 1e-6) continue;
    relevant_arcs_.push_back({tails_[i], heads_[i], lp_value});
    relevant_arc_by_decreasing_lp_values.push_back({lp_value, i});
  }
  std::sort(relevant_arc_by_decreasing_lp_values.begin(),
            relevant_arc_by_decreasing_lp_values.end(),
            std::greater<std::pair<double, int>>());

  ordered_arcs_.clear();
  for (const auto& [score, arc] : relevant_arc_by_decreasing_lp_values) {
    ordered_arcs_.push_back({tails_[arc], heads_[arc]});
  }
}

bool OutgoingCutHelper::AddOutgoingCut(LinearConstraintManager* manager,
                                       std::string name, int subset_size,
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

  return manager->AddCut(outgoing.Build(), name);
}

bool OutgoingCutHelper::TrySubsetCut(std::string name,
                                     absl::Span<const int> subset,
                                     LinearConstraintManager* manager) {
  DCHECK_GE(subset.size(), 1);
  DCHECK_LT(subset.size(), num_nodes_);

  // Initialize "in_subset" and contain_depot.
  bool contain_depot = false;
  for (const int n : subset) {
    in_subset_[n] = true;
    if (n == 0 && is_route_constraint_) {
      contain_depot = true;
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
  const int subset_or_complement_size =
      contain_depot ? num_nodes_ - subset.size() : subset.size();
  if (subset_or_complement_size <=
          params_.routing_cut_subset_size_for_binary_relation_bound() &&
      subset_or_complement_size >=
          params_.routing_cut_subset_size_for_tight_binary_relation_bound()) {
    int bound;
    if (contain_depot) {
      complement_of_subset_.clear();
      for (int i = 0; i < num_nodes_; ++i) {
        if (!in_subset_[i]) complement_of_subset_.push_back(i);
      }
      bound = min_outgoing_flow_helper_.ComputeMinOutgoingFlow(
          complement_of_subset_);
    } else {
      bound = min_outgoing_flow_helper_.ComputeMinOutgoingFlow(subset);
    }
    if (bound > min_outgoing_flow) {
      absl::StrAppend(&name, "Automatic");
      min_outgoing_flow = bound;
    }
  }
  if (subset_or_complement_size <
      params_.routing_cut_subset_size_for_tight_binary_relation_bound()) {
    int bound;
    if (contain_depot) {
      complement_of_subset_.clear();
      for (int i = 0; i < num_nodes_; ++i) {
        if (!in_subset_[i]) complement_of_subset_.push_back(i);
      }
      bound = min_outgoing_flow_helper_.ComputeTightMinOutgoingFlow(
          complement_of_subset_);
    } else {
      bound = min_outgoing_flow_helper_.ComputeTightMinOutgoingFlow(subset);
    }
    if (bound > min_outgoing_flow) {
      absl::StrAppend(&name, "AutomaticTight");
      min_outgoing_flow = bound;
    }
  }

  // Bounds coming from the demands_/capacity_ fields (if set).
  std::vector<int> to_ignore_candidates;
  if (!demands_.empty()) {
    // If subset contains depot, we actually look at the subset complement to
    // derive a bound on the outgoing flow. If we cannot reach the capacity
    // given the demands in the subset, we can derive tighter bounds.
    int64_t has_excessive_demands = false;
    int64_t has_negative_demands = false;
    int64_t sum_of_elements = 0;
    std::vector<int64_t> elements;
    const auto process_demand = [&](int64_t d) {
      if (d < 0) has_negative_demands = true;
      if (d > capacity_) has_excessive_demands = true;
      sum_of_elements += d;
      elements.push_back(d);
    };
    if (contain_depot) {
      for (int n = 0; n < num_nodes_; ++n) {
        if (!in_subset_[n]) {
          process_demand(demands_[n]);
        }
      }
    } else {
      for (const int n : subset) {
        process_demand(demands_[n]);
      }
    }

    // Lets wait for these to disappear before adding cuts.
    if (has_excessive_demands) return false;

    // Try to tighten the capacity using DP. Note that there is no point doing
    // anything if one route can serve all demands since then the bound is
    // already tight.
    //
    // TODO(user): Compute a bound in the presence of negative demands?
    bool exact_was_used = false;
    int64_t tightened_capacity = capacity_;
    if (!has_negative_demands && sum_of_elements > capacity_) {
      max_bounded_subset_sum_.Reset(capacity_);
      for (const int64_t e : elements) {
        max_bounded_subset_sum_.Add(e);
      }
      tightened_capacity = max_bounded_subset_sum_.CurrentMax();

      // If the complexity looks ok, try a more expensive DP than the quick one
      // above.
      if (max_bounded_subset_sum_exact_.ComplexityEstimate(
              elements.size(), capacity_) < params_.routing_cut_dp_effort()) {
        const int64_t exact =
            max_bounded_subset_sum_exact_.MaxSubsetSum(elements, capacity_);
        CHECK_LE(exact, tightened_capacity);
        if (exact < tightened_capacity) {
          tightened_capacity = exact;
          exact_was_used = true;
        }
      }
    }

    const int64_t flow_lower_bound =
        MathUtil::CeilOfRatio(sum_of_elements, tightened_capacity);
    if (flow_lower_bound > min_outgoing_flow) {
      min_outgoing_flow = flow_lower_bound;
      absl::StrAppend(&name, exact_was_used ? "Tightened" : "Capacity");
    }

    if (!contain_depot && flow_lower_bound >= min_outgoing_flow) {
      // We compute the biggest extra item that could fit in 'flow_lower_bound'
      // bins. If the first (flow_lower_bound - 1) bins are tight, i.e. all
      // their tightened_capacity is filled, then the last bin will have
      // 'last_bin_fillin' stuff, which will leave 'space_left' to fit an extra
      // 'item.
      const int64_t last_bin_fillin =
          sum_of_elements - (flow_lower_bound - 1) * tightened_capacity;
      const int64_t space_left = capacity_ - last_bin_fillin;
      DCHECK_GE(space_left, 0);
      DCHECK_LT(space_left, capacity_);

      // It is possible to make this cut stronger, using similar reasoning to
      // the Multistar CVRP cuts: if there is a node n (other than the depot)
      // outside of `subset`, with a demand that is greater than space_left,
      // then the outgoing flow of (subset + n) is >= flow_lower_bound + 1.
      // Using this, we can show that we still need `flow_lower_bound` on the
      // outgoing arcs of `subset` other than those towards n (noted A in the
      // diagram below):
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
      // By hypothesis, outgoing_flow(A) + outgoing_flow(B) > flow_lower_bound
      // and, since n is not the depot, outgoing_flow(B) <= 1. Hence
      // outgoing_flow(A) >= flow_lower_bound.
      for (int n = 1; n < num_nodes_; ++n) {
        if (in_subset_[n]) continue;
        if (demands_[n] > space_left) {
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
    result = AddOutgoingCut(manager, name, subset.size(), in_subset_,
                            /*rhs_lower_bound=*/min_outgoing_flow,
                            ignore_arcs_with_head);
  }

  // Sparse clean up.
  for (const int n : subset) in_subset_[n] = false;

  return result;
}

bool OutgoingCutHelper::TryBlossomSubsetCut(
    std::string name, absl::Span<const ArcWithLpValue> symmetrized_edges,
    absl::Span<const int> subset, LinearConstraintManager* manager) {
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

  return manager->AddCut(builder.Build(), name);
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
void SeparateSubtourInequalities(OutgoingCutHelper& helper,
                                 LinearConstraintManager* manager) {
  const int num_nodes = helper.num_nodes();
  if (num_nodes <= 2) return;

  helper.InitializeForNewLpSolution(manager);

  std::vector<int> subset_data;
  std::vector<absl::Span<const int>> subsets;
  GenerateInterestingSubsets(num_nodes, helper.ordered_arcs(),
                             /*stop_at_num_components=*/2, &subset_data,
                             &subsets);

  const int depot = 0;
  if (helper.is_route_constraint()) {
    // Add the depot so that we have a trivial bound on the number of
    // vehicle.
    subsets.push_back(absl::MakeSpan(&depot, 1));
  }

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
    if (helper.TrySubsetCut("Circuit", subset, manager)) {
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
  absl::Span<const ArcWithLpValue> symmetrized_relevant_arcs =
      helper.SymmetrizedRelevantArcs();
  std::vector<int> parent =
      ComputeGomoryHuTree(num_nodes, symmetrized_relevant_arcs);

  // Try all interesting subset from the Gomory-Hu tree.
  ExtractAllSubsetsFromForest(parent, &subset_data, &subsets);
  last_added_start = -1;
  for (const absl::Span<const int> subset : subsets) {
    if (subset.size() <= 1) continue;
    if (subset.size() == num_nodes) continue;
    const int start = static_cast<int>(subset.data() - subset_data.data());
    if (start <= last_added_start) continue;
    if (helper.TrySubsetCut("CircuitExact", subset, manager)) {
      ++num_added;
      last_added_start = start;
    }
  }

  // Exact separation of symmetric Blossom cut. We use the algorithm in the
  // paper: "A Faster Exact Separation Algorithm for Blossom Inequalities", Adam
  // N. Letchford, Gerhard Reinelt, Dirk Oliver Theis, 2004.
  if (num_added != 0) return;
  if (num_nodes <= 2) return;
  std::vector<ArcWithLpValue> for_blossom;
  for_blossom.reserve(symmetrized_relevant_arcs.size());
  for (ArcWithLpValue arc : symmetrized_relevant_arcs) {
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
    if (helper.TryBlossomSubsetCut("CircuitBlossom", symmetrized_relevant_arcs,
                                   subset, manager)) {
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

}  // namespace

// We use a basic algorithm to detect components that are not connected to the
// rest of the graph in the LP solution, and add cuts to force some arcs to
// enter and leave this component from outside.
CutGenerator CreateStronglyConnectedGraphCutGenerator(
    int num_nodes, absl::Span<const int> tails, absl::Span<const int> heads,
    absl::Span<const Literal> literals, Model* model) {
  auto helper = std::make_unique<OutgoingCutHelper>(
      num_nodes, /*is_route_constraint=*/false, /*capacity=*/0,
      /*demands=*/absl::Span<const int64_t>(), tails, heads, literals, model);
  CutGenerator result;
  result.vars = GetAssociatedVariables(literals, model);
  result.generate_cuts =
      [helper = std::move(helper)](LinearConstraintManager* manager) {
        SeparateSubtourInequalities(*helper, manager);
        return true;
      };
  return result;
}

CutGenerator CreateCVRPCutGenerator(int num_nodes, absl::Span<const int> tails,
                                    absl::Span<const int> heads,
                                    absl::Span<const Literal> literals,
                                    absl::Span<const int64_t> demands,
                                    int64_t capacity, Model* model) {
  auto helper = std::make_unique<OutgoingCutHelper>(
      num_nodes, /*is_route_constraint=*/true, capacity, demands, tails, heads,
      literals, model);
  CutGenerator result;
  result.vars = GetAssociatedVariables(literals, model);
  result.generate_cuts =
      [helper = std::move(helper)](LinearConstraintManager* manager) {
        SeparateSubtourInequalities(*helper, manager);
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
      gcd = std::gcd(gcd, std::abs(arc_capacities[i].coeff.value()));
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
