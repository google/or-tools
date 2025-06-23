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

#include "ortools/sat/precedences.h"

#include <stdint.h>

#include <algorithm>
#include <deque>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/strong_vector.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/topologicalsorter.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/cp_constraints.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"
#include "ortools/util/logging.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

LinearExpression2Index Linear2Indices::AddOrGet(
    LinearExpression2 original_expr) {
  LinearExpression2 expr = original_expr;
  DCHECK(expr.IsCanonicalized());
  DCHECK_EQ(expr.DivideByGcd(), 1);
  DCHECK_NE(expr.coeffs[0], 0);
  DCHECK_NE(expr.coeffs[1], 0);
  const bool negated = expr.NegateForCanonicalization();
  auto [it, inserted] = expr_to_index_.insert({expr, exprs_.size()});
  if (inserted) {
    CHECK_LT(2 * exprs_.size() + 1,
             std::numeric_limits<LinearExpression2Index>::max());
    exprs_.push_back(expr);
  }
  const LinearExpression2Index result =
      negated ? NegationOf(LinearExpression2Index(2 * it->second))
              : LinearExpression2Index(2 * it->second);

  if (!inserted) return result;

  // Update our per-variable and per-pair lookup tables.
  IntegerVariable var1 = PositiveVariable(expr.vars[0]);
  IntegerVariable var2 = PositiveVariable(expr.vars[1]);
  if (var1 > var2) std::swap(var1, var2);
  var_pair_to_bounds_[{var1, var2}].push_back(result);
  var_to_bounds_[var1].push_back(result);
  var_to_bounds_[var2].push_back(result);

  return result;
}

void Linear2Watcher::NotifyBoundChanged(LinearExpression2 expr) {
  DCHECK(expr.IsCanonicalized());
  DCHECK_EQ(expr.DivideByGcd(), 1);
  ++timestamp_;
  for (const int id : propagator_ids_) {
    watcher_->CallOnNextPropagate(id);
  }
  for (IntegerVariable var : expr.non_zero_vars()) {
    var = PositiveVariable(var);  // TODO(user): Be more precise?
    if (var >= var_timestamp_.size()) {
      var_timestamp_.resize(var + 1, 0);
    }
    var_timestamp_[var]++;
  }
}

int64_t Linear2Watcher::VarTimestamp(IntegerVariable var) {
  var = PositiveVariable(var);
  return var < var_timestamp_.size() ? var_timestamp_[var] : 0;
}

bool RootLevelLinear2Bounds::AddUpperBound(LinearExpression2Index index,
                                           IntegerValue ub) {
  const LinearExpression2 expr = lin2_indices_->GetExpression(index);
  const IntegerValue zero_level_ub = integer_trail_->LevelZeroUpperBound(expr);
  if (ub >= zero_level_ub) {
    return false;
  }
  if (best_upper_bounds_.size() <= index) {
    best_upper_bounds_.resize(index.value() + 1, kMaxIntegerValue);
  }
  if (ub >= best_upper_bounds_[index]) {
    return false;
  }
  best_upper_bounds_[index] = ub;

  ++num_updates_;
  linear2_watcher_->NotifyBoundChanged(expr);

  // Simple relations.
  //
  // TODO(user): Remove them each time we go back to level zero and they become
  // trivially true ?
  if (IntTypeAbs(expr.coeffs[0]) == 1 && IntTypeAbs(expr.coeffs[1]) == 1) {
    if (index >= in_coeff_one_lookup_.size()) {
      in_coeff_one_lookup_.resize(index + 1, false);
    }
    if (!in_coeff_one_lookup_[index]) {
      const IntegerVariable a =
          expr.coeffs[0] > 0 ? expr.vars[0] : NegationOf(expr.vars[0]);
      const IntegerVariable b =
          expr.coeffs[1] > 0 ? expr.vars[1] : NegationOf(expr.vars[1]);

      coeff_one_var_lookup_.resize(integer_trail_->NumIntegerVariables());
      in_coeff_one_lookup_[index] = true;
      coeff_one_var_lookup_[a].push_back({b, index});
      coeff_one_var_lookup_[b].push_back({a, index});
    }
  }

  // Share.
  //
  // TODO(user): It seems we could change the canonicalization to only use
  // positive variable? that would simplify a bit the code here and not make it
  // worse elsewhere?
  if (shared_linear2_bounds_ != nullptr) {
    const IntegerValue lb = -LevelZeroUpperBound(NegationOf(index));
    const int proto_var0 =
        cp_model_mapping_->GetProtoVariableFromIntegerVariable(
            PositiveVariable(expr.vars[0]));
    const int proto_var1 =
        cp_model_mapping_->GetProtoVariableFromIntegerVariable(
            PositiveVariable(expr.vars[1]));
    if (proto_var0 >= 0 && proto_var1 >= 0) {
      // This is also a relation between cp_model proto variable. Share it!
      // Note that since expr is canonicalized, this one should too.
      SharedLinear2Bounds::Key key;
      key.vars[0] = proto_var0;
      key.coeffs[0] =
          VariableIsPositive(expr.vars[0]) ? expr.coeffs[0] : -expr.coeffs[0];
      key.vars[1] = proto_var1;
      key.coeffs[1] =
          VariableIsPositive(expr.vars[1]) ? expr.coeffs[1] : -expr.coeffs[1];
      shared_linear2_bounds_->Add(shared_linear2_bounds_id_, key, lb, ub);
    }
  }
  return true;
}

// TODO(user): If we add an indexing for "coeff * var"  this is kind of
// easy to generalize to affine relations, not just "simple one".
int RootLevelLinear2Bounds::AugmentSimpleRelations(IntegerVariable var,
                                                   int work_limit) {
  var = PositiveVariable(var);
  if (var >= coeff_one_var_lookup_.size()) return 0;
  if (NegationOf(var) >= coeff_one_var_lookup_.size()) return 0;

  // Note that this never touches in_coeff_one_lookup_[var/NegationOf(var)],
  // so it should be safe to iterate on it.
  int work_done = 0;
  for (const auto [a, a_index] : coeff_one_var_lookup_[var]) {
    CHECK_NE(PositiveVariable(a), var);
    const IntegerValue a_ub = best_upper_bounds_[a_index];
    for (const auto [b, b_index] : coeff_one_var_lookup_[NegationOf(var)]) {
      if (PositiveVariable(b) == PositiveVariable(a)) continue;
      CHECK_NE(PositiveVariable(b), var);
      if (++work_done > work_limit) return work_done;

      const LinearExpression2 candidate{a, b, 1, 1};
      AddUpperBound(candidate, a_ub + best_upper_bounds_[b_index]);
    }
  }
  return work_done;
}

RootLevelLinear2Bounds::~RootLevelLinear2Bounds() {
  if (!VLOG_IS_ON(1)) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back({"RootLevelLinear2Bounds/num_updates", num_updates_});
  shared_stats_->AddStats(stats);
}

RelationStatus RootLevelLinear2Bounds::GetLevelZeroStatus(
    LinearExpression2 expr, IntegerValue lb, IntegerValue ub) const {
  expr.SimpleCanonicalization();
  if (expr.coeffs[0] == 0 || expr.coeffs[1] == 0) {
    return RelationStatus::IS_UNKNOWN;
  }
  const IntegerValue known_ub = LevelZeroUpperBound(expr);
  expr.Negate();
  const IntegerValue known_lb = -LevelZeroUpperBound(expr);
  if (lb <= known_lb && ub >= known_ub) return RelationStatus::IS_TRUE;
  if (lb > known_ub || ub < known_lb) return RelationStatus::IS_FALSE;

  return RelationStatus::IS_UNKNOWN;
}

IntegerValue RootLevelLinear2Bounds::GetUpperBoundNoTrail(
    LinearExpression2Index index) const {
  if (best_upper_bounds_.size() <= index) {
    return kMaxIntegerValue;
  }
  return best_upper_bounds_[index];
}

std::vector<std::pair<LinearExpression2, IntegerValue>>
RootLevelLinear2Bounds::GetSortedNonTrivialUpperBounds() const {
  std::vector<std::pair<LinearExpression2, IntegerValue>> result;
  for (LinearExpression2Index index = LinearExpression2Index{0};
       index < best_upper_bounds_.size(); ++index) {
    const IntegerValue ub = best_upper_bounds_[index];
    if (ub == kMaxIntegerValue) continue;
    const LinearExpression2 expr = lin2_indices_->GetExpression(index);
    if (ub < integer_trail_->LevelZeroUpperBound(expr)) {
      result.push_back({expr, ub});
    }
  }
  std::sort(result.begin(), result.end());
  return result;
}

std::vector<std::tuple<LinearExpression2, IntegerValue, IntegerValue>>
RootLevelLinear2Bounds::GetAllBoundsContainingVariable(
    IntegerVariable var) const {
  std::vector<std::tuple<LinearExpression2, IntegerValue, IntegerValue>> result;
  for (const LinearExpression2Index index :
       lin2_indices_->GetAllLinear2ContainingVariable(var)) {
    const IntegerValue lb = -GetUpperBoundNoTrail(NegationOf(index));
    const IntegerValue ub = GetUpperBoundNoTrail(index);
    const LinearExpression2 expr = lin2_indices_->GetExpression(index);
    const IntegerValue trail_lb = integer_trail_->LevelZeroLowerBound(expr);
    const IntegerValue trail_ub = integer_trail_->LevelZeroUpperBound(expr);
    if (lb <= trail_lb && ub >= trail_ub) continue;
    LinearExpression2 explicit_vars_expr = expr;
    if (explicit_vars_expr.vars[0] == NegationOf(var)) {
      explicit_vars_expr.vars[0] = NegationOf(explicit_vars_expr.vars[0]);
      explicit_vars_expr.coeffs[0] = -explicit_vars_expr.coeffs[0];
    }
    if (explicit_vars_expr.vars[1] == NegationOf(var)) {
      explicit_vars_expr.vars[1] = NegationOf(explicit_vars_expr.vars[1]);
      explicit_vars_expr.coeffs[1] = -explicit_vars_expr.coeffs[1];
    }
    if (explicit_vars_expr.vars[1] == var) {
      std::swap(explicit_vars_expr.vars[0], explicit_vars_expr.vars[1]);
      std::swap(explicit_vars_expr.coeffs[0], explicit_vars_expr.coeffs[1]);
    }
    DCHECK(explicit_vars_expr.vars[0] == var);
    result.push_back(
        {explicit_vars_expr, std::max(lb, trail_lb), std::min(ub, trail_ub)});
  }
  return result;
}

// Return a list of (lb <= expr <= ub), with expr.vars = {var1, var2}, where
// at least one of the bounds is non-trivial and the potential other
// non-trivial bound is tight.
std::vector<std::tuple<LinearExpression2, IntegerValue, IntegerValue>>
RootLevelLinear2Bounds::GetAllBoundsContainingVariables(
    IntegerVariable var1, IntegerVariable var2) const {
  std::vector<std::tuple<LinearExpression2, IntegerValue, IntegerValue>> result;
  for (const LinearExpression2Index index :
       lin2_indices_->GetAllLinear2ContainingVariables(var1, var2)) {
    const IntegerValue lb = -GetUpperBoundNoTrail(NegationOf(index));
    const IntegerValue ub = GetUpperBoundNoTrail(index);
    const LinearExpression2 expr = lin2_indices_->GetExpression(index);
    const IntegerValue trail_lb = integer_trail_->LevelZeroLowerBound(expr);
    const IntegerValue trail_ub = integer_trail_->LevelZeroUpperBound(expr);
    if (lb <= trail_lb && ub >= trail_ub) continue;

    LinearExpression2 explicit_vars_expr = expr;
    if (explicit_vars_expr.vars[0] == NegationOf(var1) ||
        explicit_vars_expr.vars[0] == NegationOf(var2)) {
      explicit_vars_expr.vars[0] = NegationOf(explicit_vars_expr.vars[0]);
      explicit_vars_expr.coeffs[0] = -explicit_vars_expr.coeffs[0];
    }
    if (explicit_vars_expr.vars[1] == NegationOf(var1) ||
        explicit_vars_expr.vars[1] == NegationOf(var2)) {
      explicit_vars_expr.vars[1] = NegationOf(explicit_vars_expr.vars[1]);
      explicit_vars_expr.coeffs[1] = -explicit_vars_expr.coeffs[1];
    }
    if (explicit_vars_expr.vars[1] == var1) {
      std::swap(explicit_vars_expr.vars[0], explicit_vars_expr.vars[1]);
      std::swap(explicit_vars_expr.coeffs[0], explicit_vars_expr.coeffs[1]);
    }
    DCHECK(explicit_vars_expr.vars[0] == var1 &&
           explicit_vars_expr.vars[1] == var2);
    result.push_back(
        {explicit_vars_expr, std::max(lb, trail_lb), std::min(ub, trail_ub)});
  }
  return result;
}

absl::Span<const std::pair<IntegerVariable, LinearExpression2Index>>
RootLevelLinear2Bounds::GetVariablesInSimpleRelation(
    IntegerVariable var) const {
  if (var >= coeff_one_var_lookup_.size()) return {};
  return coeff_one_var_lookup_[var];
}

EnforcedLinear2Bounds::~EnforcedLinear2Bounds() {
  if (!VLOG_IS_ON(1)) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back({"EnforcedLinear2Bounds/num_conditional_relation_updates",
                   num_conditional_relation_updates_});
  shared_stats_->AddStats(stats);
}

void EnforcedLinear2Bounds::PushConditionalRelation(
    absl::Span<const Literal> enforcements, LinearExpression2Index lin2_index,
    IntegerValue rhs) {
  // This must be currently true.
  if (DEBUG_MODE) {
    for (const Literal l : enforcements) {
      CHECK(trail_->Assignment().LiteralIsTrue(l));
    }
  }

  if (enforcements.empty() || trail_->CurrentDecisionLevel() == 0) {
    root_level_bounds_->AddUpperBound(lin2_index, rhs);
    return;
  }

  if (rhs >= root_level_bounds_->LevelZeroUpperBound(lin2_index)) return;
  const LinearExpression2 expr = lin2_indices_->GetExpression(lin2_index);

  linear2_watcher_->NotifyBoundChanged(expr);
  ++num_conditional_relation_updates_;

  const int new_index = conditional_stack_.size();
  if (conditional_relations_.size() <= lin2_index) {
    conditional_relations_.resize(lin2_index.value() + 1, -1);
  }
  if (conditional_relations_[lin2_index] == -1) {
    conditional_relations_[lin2_index] = new_index;
    CreateLevelEntryIfNeeded();
    conditional_stack_.emplace_back(/*prev_entry=*/-1, rhs, lin2_index,
                                    enforcements);

    if (expr.coeffs[0] == 1 && expr.coeffs[1] == 1) {
      const int new_size =
          std::max(expr.vars[0].value(), expr.vars[1].value()) + 1;
      if (new_size > conditional_var_lookup_.size()) {
        conditional_var_lookup_.resize(new_size);
      }
      conditional_var_lookup_[expr.vars[0]].push_back(
          {expr.vars[1], lin2_index});
      conditional_var_lookup_[expr.vars[1]].push_back(
          {expr.vars[0], lin2_index});
    }
  } else {
    const int prev_entry = conditional_relations_[lin2_index];
    if (rhs >= conditional_stack_[prev_entry].rhs) return;

    // Update.
    conditional_relations_[lin2_index] = new_index;
    CreateLevelEntryIfNeeded();
    conditional_stack_.emplace_back(prev_entry, rhs, lin2_index, enforcements);
  }
}

void EnforcedLinear2Bounds::CreateLevelEntryIfNeeded() {
  const int current = trail_->CurrentDecisionLevel();
  if (!level_to_stack_size_.empty() &&
      level_to_stack_size_.back().first == current)
    return;
  level_to_stack_size_.push_back({current, conditional_stack_.size()});
}

// We only pop what is needed.
void EnforcedLinear2Bounds::SetLevel(int level) {
  while (!level_to_stack_size_.empty() &&
         level_to_stack_size_.back().first > level) {
    const int target = level_to_stack_size_.back().second;
    DCHECK_GE(conditional_stack_.size(), target);
    while (conditional_stack_.size() > target) {
      const ConditionalEntry& back = conditional_stack_.back();
      if (back.prev_entry != -1) {
        conditional_relations_[back.key] = back.prev_entry;
      } else {
        conditional_relations_[back.key] = -1;
        const LinearExpression2 expr = lin2_indices_->GetExpression(back.key);

        if (expr.coeffs[0] == 1 && expr.coeffs[1] == 1) {
          DCHECK_EQ(conditional_var_lookup_[expr.vars[0]].back().first,
                    expr.vars[1]);
          DCHECK_EQ(conditional_var_lookup_[expr.vars[1]].back().first,
                    expr.vars[0]);
          conditional_var_lookup_[expr.vars[0]].pop_back();
          conditional_var_lookup_[expr.vars[1]].pop_back();
        }
      }
      conditional_stack_.pop_back();
    }
    level_to_stack_size_.pop_back();
  }
}

void EnforcedLinear2Bounds::AddReasonForUpperBoundLowerThan(
    LinearExpression2Index index, IntegerValue ub,
    std::vector<Literal>* literal_reason,
    std::vector<IntegerLiteral>* /*unused*/) const {
  if (ub >= root_level_bounds_->LevelZeroUpperBound(index)) return;
  DCHECK_LT(index, conditional_relations_.size());
  const int entry_index = conditional_relations_[index];
  DCHECK_NE(entry_index, -1);

  const ConditionalEntry& entry = conditional_stack_[entry_index];
  if (DEBUG_MODE) {
    for (const Literal l : entry.enforcements) {
      CHECK(trail_->Assignment().LiteralIsTrue(l));
    }
  }
  DCHECK_LE(entry.rhs, ub);
  for (const Literal l : entry.enforcements) {
    literal_reason->push_back(l.Negated());
  }
}

IntegerValue EnforcedLinear2Bounds::GetUpperBoundFromEnforced(
    LinearExpression2Index index) const {
  if (index >= conditional_relations_.size()) {
    return kMaxIntegerValue;
  }
  const int entry_index = conditional_relations_[index];
  if (entry_index == -1) {
    return kMaxIntegerValue;
  } else {
    const ConditionalEntry& entry = conditional_stack_[entry_index];
    if (DEBUG_MODE) {
      for (const Literal l : entry.enforcements) {
        CHECK(trail_->Assignment().LiteralIsTrue(l));
      }
    }
    DCHECK_LT(entry.rhs, root_level_bounds_->LevelZeroUpperBound(index));
    return entry.rhs;
  }
}

bool TransitivePrecedencesEvaluator::Build() {
  const int64_t in_timestamp = root_level_bounds_->num_updates();
  if (in_timestamp <= build_timestamp_) return true;
  build_timestamp_ = in_timestamp;

  const std::vector<std::pair<LinearExpression2, IntegerValue>>
      root_relations_sorted =
          root_level_bounds_->GetSortedNonTrivialUpperBounds();
  int max_node = 0;
  for (const auto [expr, _] : root_relations_sorted) {
    max_node = std::max(max_node, PositiveVariable(expr.vars[0]).value());
    max_node = std::max(max_node, PositiveVariable(expr.vars[1]).value());
  }
  max_node++;  // For negation.

  // Is it a DAG?
  // Get a topological order of the DAG formed by all the arcs that are present.
  //
  // TODO(user): This can fail if we don't have a DAG. But in the end we
  // don't really need a topological order, just something that is close to
  // one so that we can compute an approximated transitive closure in O(n^2) and
  // not O(n^3). We could use an heuristic instead, like as long as there is
  // node with an in-degree of zero, add them to the order and update the
  // in-degree of the other (by removing outgoing arcs). If there is a cycle
  // (i.e. no node with no incoming arc), pick one with a small in-degree
  // randomly.
  DenseIntStableTopologicalSorter sorter(max_node);
  for (const auto [expr, negated_offset] : root_relations_sorted) {
    // Coefficients should be positive.
    DCHECK_GT(expr.coeffs[0], 0);
    DCHECK_GT(expr.coeffs[1], 0);

    // TODO(user): Support negative offset?
    //
    // Note that if we only have >= 0 ones, if we do have a cycle, we could
    // make sure all variables are the same, and otherwise, we have a DAG or a
    // conflict.
    const IntegerValue offset = -negated_offset;
    if (offset < 0) continue;

    if (expr.coeffs[0] != 1 || expr.coeffs[1] != 1) {
      // TODO(user): Support non-1 coefficients.
      continue;
    }

    // We have two arcs.
    sorter.AddEdge(expr.vars[0].value(), NegationOf(expr.vars[1]).value());
    sorter.AddEdge(expr.vars[1].value(), NegationOf(expr.vars[0]).value());
  }
  int next;
  bool graph_has_cycle = false;
  topological_order_.clear();
  while (sorter.GetNext(&next, &graph_has_cycle, nullptr)) {
    topological_order_.push_back(IntegerVariable(next));
    if (graph_has_cycle) {
      is_dag_ = false;
      return true;
    }
  }
  is_dag_ = !graph_has_cycle;

  // Lets get the transitive closure if it is cheap. This is also a way not to
  // add too many relations (not more than 1e6) per call.
  int total_work = 0;
  const int kWorkLimit = 1e6;
  for (const IntegerVariable var : topological_order_) {
    const int work = root_level_bounds_->AugmentSimpleRelations(
        var, kWorkLimit - total_work);
    total_work += work;
  }

  build_timestamp_ = root_level_bounds_->num_updates();
  VLOG(2) << "Full precedences. Work=" << total_work
          << " Relations=" << root_relations_sorted.size()
          << " num_added=" << build_timestamp_ - in_timestamp;
  return true;
}

// TODO(user): There is probably little need for that function. For small
// problem, we already augment root_level_bounds_ will all the relation obtained
// by transitive closure, so this algo only need to look at direct dependency in
// root_level_bounds_->GetVariablesInSimpleRelation(). And for large graph, we
// probably do not want this.
void TransitivePrecedencesEvaluator::ComputeFullPrecedences(
    absl::Span<const IntegerVariable> vars,
    std::vector<FullIntegerPrecedence>* output) {
  output->clear();
  Build();  // Will do nothing if we are up to date.
  if (!is_dag_) return;

  // Compute all precedences.
  // We loop over the node in topological order, and we maintain for all
  // variable we encounter, the list of "to_consider" variables that are before.
  //
  // TODO(user): use vector of fixed size.
  absl::flat_hash_set<IntegerVariable> is_interesting;
  absl::flat_hash_set<IntegerVariable> to_consider(vars.begin(), vars.end());
  absl::flat_hash_map<IntegerVariable,
                      absl::flat_hash_map<IntegerVariable, IntegerValue>>
      vars_before_with_offset;
  absl::flat_hash_map<IntegerVariable, IntegerValue> tail_map;
  for (const IntegerVariable tail_var : topological_order_) {
    if (!to_consider.contains(tail_var) &&
        !vars_before_with_offset.contains(tail_var)) {
      continue;
    }

    // We copy the data for tail_var here, because the pointer is not stable.
    // TODO(user): optimize when needed.
    tail_map.clear();
    {
      const auto it = vars_before_with_offset.find(tail_var);
      if (it != vars_before_with_offset.end()) {
        tail_map = it->second;
      }
    }

    // We look for tail_var + offset <= head_var.
    for (const auto [neg_head_var, index] :
         root_level_bounds_->GetVariablesInSimpleRelation(tail_var)) {
      const IntegerVariable head_var = NegationOf(neg_head_var);
      const IntegerValue arc_offset =
          -root_level_bounds_->GetUpperBoundNoTrail(index);

      // No need to create an empty entry in this case.
      if (tail_map.empty() && !to_consider.contains(tail_var)) continue;

      auto& to_update = vars_before_with_offset[head_var];
      for (const auto& [var_before, offset] : tail_map) {
        if (!to_update.contains(var_before)) {
          to_update[var_before] = arc_offset + offset;
        } else {
          to_update[var_before] =
              std::max(arc_offset + offset, to_update[var_before]);
        }
      }
      if (to_consider.contains(tail_var)) {
        if (!to_update.contains(tail_var)) {
          to_update[tail_var] = arc_offset;
        } else {
          to_update[tail_var] = std::max(arc_offset, to_update[tail_var]);
        }
      }

      // Small filtering heuristic: if we have (before) < tail, and tail < head,
      // we really do not need to list (before, tail) < head. We only need that
      // if the list of variable before head contains some variable that are not
      // already before tail.
      if (to_update.size() > tail_map.size() + 1) {
        is_interesting.insert(head_var);
      } else {
        is_interesting.erase(head_var);
      }
    }

    // Extract the output for tail_var. Because of the topological ordering, the
    // data for tail_var is already final now.
    //
    // TODO(user): Release the memory right away.
    if (!is_interesting.contains(tail_var)) continue;
    if (tail_map.size() == 1) continue;

    FullIntegerPrecedence data;
    data.var = tail_var;
    IntegerValue min_offset = kMaxIntegerValue;
    for (int i = 0; i < vars.size(); ++i) {
      const auto offset_it = tail_map.find(vars[i]);
      if (offset_it == tail_map.end()) continue;
      data.indices.push_back(i);
      data.offsets.push_back(offset_it->second);
      min_offset = std::min(data.offsets.back(), min_offset);
    }
    output->push_back(std::move(data));
  }
}

void EnforcedLinear2Bounds::CollectPrecedences(
    absl::Span<const IntegerVariable> vars,
    std::vector<PrecedenceData>* output) {
  const int needed_size = integer_trail_->NumIntegerVariables().value();
  var_to_degree_.resize(needed_size);
  var_to_last_index_.resize(needed_size);
  var_with_positive_degree_.resize(needed_size);
  tmp_precedences_.clear();

  // We first compute the degree/size in order to perform the transposition.
  // Note that we also remove duplicates.
  int num_relevants = 0;
  IntegerVariable* var_with_positive_degree = var_with_positive_degree_.data();
  int* var_to_degree = var_to_degree_.data();
  int* var_to_last_index = var_to_last_index_.data();
  const auto process =
      [&](int var_index,
          absl::Span<const std::pair<IntegerVariable, LinearExpression2Index>>
              v) {
        for (const auto [other, lin2_index] : v) {
          const IntegerVariable after = NegationOf(other);
          DCHECK_LT(after, needed_size);
          if (var_to_degree[after.value()] == 0) {
            var_with_positive_degree[num_relevants++] = after;
          } else {
            // We do not want duplicates.
            if (var_to_last_index[after.value()] == var_index) continue;
          }

          tmp_precedences_.push_back({after, var_index, lin2_index});
          var_to_degree[after.value()]++;
          var_to_last_index[after.value()] = var_index;
        }
      };

  for (int var_index = 0; var_index < vars.size(); ++var_index) {
    const IntegerVariable var = vars[var_index];
    process(var_index, root_level_bounds_->GetVariablesInSimpleRelation(var));
    if (var < conditional_var_lookup_.size()) {
      process(var_index, conditional_var_lookup_[var]);
    }
  }

  // Permute tmp_precedences_ into the output to put it in the correct order.
  // For that we transform var_to_degree to point to the first position of
  // each lbvar in the output vector.
  int start = 0;
  const absl::Span<const IntegerVariable> relevant_variables =
      absl::MakeSpan(var_with_positive_degree, num_relevants);
  for (const IntegerVariable var : relevant_variables) {
    const int degree = var_to_degree[var.value()];
    if (degree > 1) {
      var_to_degree[var.value()] = start;
      start += degree;
    } else {
      // Optimization: we remove degree one relations.
      var_to_degree[var.value()] = -1;
    }
  }

  output->resize(start);
  for (const auto& precedence : tmp_precedences_) {
    // Note that it is okay to increase the -1 pos since they appear only once.
    const int pos = var_to_degree[precedence.var.value()]++;
    if (pos < 0) continue;
    (*output)[pos] = precedence;
  }

  // Cleanup var_to_degree, note that we don't need to clean
  // var_to_last_index_.
  for (const IntegerVariable var : relevant_variables) {
    var_to_degree[var.value()] = 0;
  }
}

namespace {

void AppendLowerBoundReasonIfValid(IntegerVariable var,
                                   const IntegerTrail& i_trail,
                                   std::vector<IntegerLiteral>* reason) {
  if (var != kNoIntegerVariable) {
    reason->push_back(i_trail.LowerBoundAsLiteral(var));
  }
}

}  // namespace

PrecedencesPropagator::~PrecedencesPropagator() {
  if (!VLOG_IS_ON(1)) return;
  if (shared_stats_ == nullptr) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back({"precedences/num_cycles", num_cycles_});
  stats.push_back({"precedences/num_pushes", num_pushes_});
  stats.push_back(
      {"precedences/num_enforcement_pushes", num_enforcement_pushes_});
  shared_stats_->AddStats(stats);
}

bool PrecedencesPropagator::Propagate(Trail* trail) { return Propagate(); }

bool PrecedencesPropagator::Propagate() {
  while (propagation_trail_index_ < trail_->Index()) {
    const Literal literal = (*trail_)[propagation_trail_index_++];
    if (literal.Index() >= literal_to_new_impacted_arcs_.size()) continue;

    // IMPORTANT: Because of the way Untrail() work, we need to add all the
    // potential arcs before we can abort. It is why we iterate twice here.
    for (const ArcIndex arc_index :
         literal_to_new_impacted_arcs_[literal.Index()]) {
      if (--arc_counts_[arc_index] == 0) {
        const ArcInfo& arc = arcs_[arc_index];
        PushConditionalRelations(arc);
        impacted_arcs_[arc.tail_var].push_back(arc_index);
      }
    }

    // Iterate again to check for a propagation and indirectly update
    // modified_vars_.
    for (const ArcIndex arc_index :
         literal_to_new_impacted_arcs_[literal.Index()]) {
      if (arc_counts_[arc_index] > 0) continue;
      const ArcInfo& arc = arcs_[arc_index];
      const IntegerValue new_head_lb =
          integer_trail_->LowerBound(arc.tail_var) + ArcOffset(arc);
      if (new_head_lb > integer_trail_->LowerBound(arc.head_var)) {
        if (!EnqueueAndCheck(arc, new_head_lb, trail_)) return false;
      }
    }
  }

  // Do the actual propagation of the IntegerVariable bounds.
  InitializeBFQueueWithModifiedNodes();
  if (!BellmanFordTarjan(trail_)) return false;

  // We can only test that no propagation is left if we didn't enqueue new
  // literal in the presence of optional variables.
  //
  // TODO(user): Because of our code to deal with InPropagationLoop(), this is
  // not always true. Find a cleaner way to DCHECK() while not failing in this
  // corner case.
  if (/*DISABLES CODE*/ (false) &&
      propagation_trail_index_ == trail_->Index()) {
    DCHECK(NoPropagationLeft(*trail_));
  }

  // Propagate the presence literals of the arcs that can't be added.
  PropagateOptionalArcs(trail_);

  // Clean-up modified_vars_ to do as little as possible on the next call.
  modified_vars_.ClearAndResize(integer_trail_->NumIntegerVariables());
  return true;
}

bool PrecedencesPropagator::PropagateOutgoingArcs(IntegerVariable var) {
  CHECK_NE(var, kNoIntegerVariable);
  if (var >= impacted_arcs_.size()) return true;
  for (const ArcIndex arc_index : impacted_arcs_[var]) {
    const ArcInfo& arc = arcs_[arc_index];
    const IntegerValue new_head_lb =
        integer_trail_->LowerBound(arc.tail_var) + ArcOffset(arc);
    if (new_head_lb > integer_trail_->LowerBound(arc.head_var)) {
      if (!EnqueueAndCheck(arc, new_head_lb, trail_)) return false;
    }
  }
  return true;
}

// TODO(user): Remove literal fixed at level zero from there.
void PrecedencesPropagator::PushConditionalRelations(const ArcInfo& arc) {
  // We currently do not handle variable size in the reasons.
  // TODO(user): we could easily take a level zero ArcOffset() instead, or
  // add this to the reason though.
  if (arc.offset_var != kNoIntegerVariable) return;
  const IntegerValue offset = ArcOffset(arc);
  relations_->PushConditionalRelation(
      arc.presence_literals,
      LinearExpression2::Difference(arc.tail_var, arc.head_var), -offset);
}

void PrecedencesPropagator::Untrail(const Trail& trail, int trail_index) {
  if (propagation_trail_index_ > trail_index) {
    // This means that we already propagated all there is to propagate
    // at the level trail_index, so we can safely clear modified_vars_ in case
    // it wasn't already done.
    modified_vars_.ClearAndResize(integer_trail_->NumIntegerVariables());
  }
  while (propagation_trail_index_ > trail_index) {
    const Literal literal = trail[--propagation_trail_index_];
    if (literal.Index() >= literal_to_new_impacted_arcs_.size()) continue;
    for (const ArcIndex arc_index :
         literal_to_new_impacted_arcs_[literal.Index()]) {
      if (arc_counts_[arc_index]++ == 0) {
        const ArcInfo& arc = arcs_[arc_index];
        impacted_arcs_[arc.tail_var].pop_back();
      }
    }
  }
}

void PrecedencesPropagator::AdjustSizeFor(IntegerVariable i) {
  const int index = std::max(i.value(), NegationOf(i).value());
  if (index >= impacted_arcs_.size()) {
    // TODO(user): only watch lower bound of the relevant variable instead
    // of watching everything in [0, max_index_of_variable_used_in_this_class).
    for (IntegerVariable var(impacted_arcs_.size()); var <= index; ++var) {
      watcher_->WatchLowerBound(var, watcher_id_);
    }
    impacted_arcs_.resize(index + 1);
    impacted_potential_arcs_.resize(index + 1);
  }
}

void PrecedencesPropagator::AddArc(
    IntegerVariable tail, IntegerVariable head, IntegerValue offset,
    IntegerVariable offset_var, absl::Span<const Literal> presence_literals) {
  AdjustSizeFor(tail);
  AdjustSizeFor(head);
  if (offset_var != kNoIntegerVariable) AdjustSizeFor(offset_var);

  // This arc is present iff all the literals here are true.
  absl::InlinedVector<Literal, 6> enforcement_literals;
  {
    for (const Literal l : presence_literals) {
      enforcement_literals.push_back(l);
    }
    gtl::STLSortAndRemoveDuplicates(&enforcement_literals);

    if (trail_->CurrentDecisionLevel() == 0) {
      int new_size = 0;
      for (const Literal l : enforcement_literals) {
        if (trail_->Assignment().LiteralIsTrue(Literal(l))) {
          continue;  // At true, ignore this literal.
        } else if (trail_->Assignment().LiteralIsFalse(Literal(l))) {
          return;  // At false, ignore completely this arc.
        }
        enforcement_literals[new_size++] = l;
      }
      enforcement_literals.resize(new_size);
    }
  }

  if (head == tail) {
    // A self-arc is either plain SAT or plain UNSAT or it forces something on
    // the given offset_var or presence_literal_index. In any case it could be
    // presolved in something more efficient.
    VLOG(1) << "Self arc! This could be presolved. "
            << "var:" << tail << " offset:" << offset
            << " offset_var:" << offset_var
            << " conditioned_by:" << presence_literals;
  }

  // Remove the offset_var if it is fixed.
  // TODO(user): We should also handle the case where tail or head is fixed.
  if (offset_var != kNoIntegerVariable) {
    const IntegerValue lb = integer_trail_->LevelZeroLowerBound(offset_var);
    if (lb == integer_trail_->LevelZeroUpperBound(offset_var)) {
      offset += lb;
      offset_var = kNoIntegerVariable;
    }
  }

  // Deal first with impacted_potential_arcs_/potential_arcs_.
  if (!enforcement_literals.empty()) {
    const OptionalArcIndex arc_index(potential_arcs_.size());
    potential_arcs_.push_back(
        {tail, head, offset, offset_var, enforcement_literals});
    impacted_potential_arcs_[tail].push_back(arc_index);
    impacted_potential_arcs_[NegationOf(head)].push_back(arc_index);
    if (offset_var != kNoIntegerVariable) {
      impacted_potential_arcs_[offset_var].push_back(arc_index);
    }
  }

  // Now deal with impacted_arcs_/arcs_.
  struct InternalArc {
    IntegerVariable tail_var;
    IntegerVariable head_var;
    IntegerVariable offset_var;
  };
  std::vector<InternalArc> to_add;
  if (offset_var == kNoIntegerVariable) {
    // a + offset <= b and -b + offset <= -a
    to_add.push_back({tail, head, kNoIntegerVariable});
    to_add.push_back({NegationOf(head), NegationOf(tail), kNoIntegerVariable});
  } else {
    // tail (a) and offset_var (b) are symmetric, so we add:
    // - a + b + offset <= c
    to_add.push_back({tail, head, offset_var});
    to_add.push_back({offset_var, head, tail});
    // - a - c + offset <= -b
    to_add.push_back({tail, NegationOf(offset_var), NegationOf(head)});
    to_add.push_back({NegationOf(head), NegationOf(offset_var), tail});
    // - b - c + offset <= -a
    to_add.push_back({offset_var, NegationOf(tail), NegationOf(head)});
    to_add.push_back({NegationOf(head), NegationOf(tail), offset_var});
  }
  for (const InternalArc a : to_add) {
    // Since we add a new arc, we will need to consider its tail during the next
    // propagation. Note that the size of modified_vars_ will be automatically
    // updated when new integer variables are created since we register it with
    // IntegerTrail in this class constructor.
    //
    // TODO(user): Adding arcs and then calling Untrail() before Propagate()
    // will cause this mecanism to break. Find a more robust implementation.
    //
    // TODO(user): In some rare corner case, rescanning the whole list of arc
    // leaving tail_var can make AddVar() have a quadratic complexity where it
    // shouldn't. A better solution would be to see if this new arc currently
    // propagate something, and if it does, just update the lower bound of
    // a.head_var and let the normal "is modified" mecanism handle any eventual
    // follow up propagations.
    modified_vars_.Set(a.tail_var);

    // If a.head_var is optional, we can potentially remove some literal from
    // enforcement_literals.
    const ArcIndex arc_index(arcs_.size());
    arcs_.push_back(
        {a.tail_var, a.head_var, offset, a.offset_var, enforcement_literals});
    auto& presence_literals = arcs_.back().presence_literals;

    if (presence_literals.empty()) {
      impacted_arcs_[a.tail_var].push_back(arc_index);
    } else {
      for (const Literal l : presence_literals) {
        if (l.Index() >= literal_to_new_impacted_arcs_.size()) {
          literal_to_new_impacted_arcs_.resize(l.Index().value() + 1);
        }
        literal_to_new_impacted_arcs_[l.Index()].push_back(arc_index);
      }
    }

    if (trail_->CurrentDecisionLevel() == 0) {
      arc_counts_.push_back(presence_literals.size());
    } else {
      arc_counts_.push_back(0);
      for (const Literal l : presence_literals) {
        if (!trail_->Assignment().LiteralIsTrue(l)) {
          ++arc_counts_.back();
        }
      }
      CHECK(presence_literals.empty() || arc_counts_.back() > 0);
    }
  }
}

bool PrecedencesPropagator::AddPrecedenceWithOffsetIfNew(IntegerVariable i1,
                                                         IntegerVariable i2,
                                                         IntegerValue offset) {
  DCHECK_EQ(trail_->CurrentDecisionLevel(), 0);
  if (i1 < impacted_arcs_.size() && i2 < impacted_arcs_.size()) {
    for (const ArcIndex index : impacted_arcs_[i1]) {
      const ArcInfo& arc = arcs_[index];
      if (arc.head_var == i2) {
        const IntegerValue current = ArcOffset(arc);
        if (offset <= current) {
          return false;
        } else {
          // TODO(user): Modify arc in place!
        }
        break;
      }
    }
  }

  AddPrecedenceWithOffset(i1, i2, offset);
  return true;
}

// TODO(user): On jobshop problems with a lot of tasks per machine (500), this
// takes up a big chunk of the running time even before we find a solution.
// This is because, for each lower bound changed, we inspect 500 arcs even
// though they will never be propagated because the other bound is still at the
// horizon. Find an even sparser algorithm?
void PrecedencesPropagator::PropagateOptionalArcs(Trail* trail) {
  for (const IntegerVariable var : modified_vars_.PositionsSetAtLeastOnce()) {
    // The variables are not in increasing order, so we need to continue.
    if (var >= impacted_potential_arcs_.size()) continue;

    // Note that we can currently check the same ArcInfo up to 3 times, one for
    // each of the arc variables: tail, NegationOf(head) and offset_var.
    for (const OptionalArcIndex arc_index : impacted_potential_arcs_[var]) {
      const ArcInfo& arc = potential_arcs_[arc_index];
      int num_not_true = 0;
      Literal to_propagate;
      for (const Literal l : arc.presence_literals) {
        if (!trail->Assignment().LiteralIsTrue(l)) {
          ++num_not_true;
          to_propagate = l;
        }
      }
      if (num_not_true != 1) continue;
      if (trail->Assignment().LiteralIsFalse(to_propagate)) continue;

      // Test if this arc can be present or not.
      // Important arc.tail_var can be different from var here.
      const IntegerValue tail_lb = integer_trail_->LowerBound(arc.tail_var);
      const IntegerValue head_ub = integer_trail_->UpperBound(arc.head_var);
      if (tail_lb + ArcOffset(arc) > head_ub) {
        integer_reason_.clear();
        integer_reason_.push_back(
            integer_trail_->LowerBoundAsLiteral(arc.tail_var));
        integer_reason_.push_back(
            integer_trail_->UpperBoundAsLiteral(arc.head_var));
        AppendLowerBoundReasonIfValid(arc.offset_var, *integer_trail_,
                                      &integer_reason_);
        literal_reason_.clear();
        for (const Literal l : arc.presence_literals) {
          if (l != to_propagate) literal_reason_.push_back(l.Negated());
        }
        ++num_enforcement_pushes_;
        integer_trail_->EnqueueLiteral(to_propagate.Negated(), literal_reason_,
                                       integer_reason_);
      }
    }
  }
}

IntegerValue PrecedencesPropagator::ArcOffset(const ArcInfo& arc) const {
  return arc.offset + (arc.offset_var == kNoIntegerVariable
                           ? IntegerValue(0)
                           : integer_trail_->LowerBound(arc.offset_var));
}

bool PrecedencesPropagator::EnqueueAndCheck(const ArcInfo& arc,
                                            IntegerValue new_head_lb,
                                            Trail* trail) {
  ++num_pushes_;
  DCHECK_GT(new_head_lb, integer_trail_->LowerBound(arc.head_var));

  // Compute the reason for new_head_lb.
  //
  // TODO(user): do like for clause and keep the negation of
  // arc.presence_literals? I think we could change the integer.h API to accept
  // true literal like for IntegerVariable, it is really confusing currently.
  literal_reason_.clear();
  for (const Literal l : arc.presence_literals) {
    literal_reason_.push_back(l.Negated());
  }

  integer_reason_.clear();
  integer_reason_.push_back(integer_trail_->LowerBoundAsLiteral(arc.tail_var));
  AppendLowerBoundReasonIfValid(arc.offset_var, *integer_trail_,
                                &integer_reason_);

  // The code works without this block since Enqueue() below can already take
  // care of conflicts. However, it is better to deal with the conflict
  // ourselves because we can be smarter about the reason this way.
  //
  // The reason for a "precedence" conflict is always a linear reason
  // involving the tail lower_bound, the head upper bound and eventually the
  // size lower bound. Because of that, we can use the RelaxLinearReason()
  // code.
  if (new_head_lb > integer_trail_->UpperBound(arc.head_var)) {
    const IntegerValue slack =
        new_head_lb - integer_trail_->UpperBound(arc.head_var) - 1;
    integer_reason_.push_back(
        integer_trail_->UpperBoundAsLiteral(arc.head_var));
    std::vector<IntegerValue> coeffs(integer_reason_.size(), IntegerValue(1));
    integer_trail_->RelaxLinearReason(slack, coeffs, &integer_reason_);
    return integer_trail_->ReportConflict(literal_reason_, integer_reason_);
  }

  return integer_trail_->Enqueue(
      IntegerLiteral::GreaterOrEqual(arc.head_var, new_head_lb),
      literal_reason_, integer_reason_);
}

bool PrecedencesPropagator::NoPropagationLeft(const Trail& trail) const {
  const int num_nodes = impacted_arcs_.size();
  for (IntegerVariable var(0); var < num_nodes; ++var) {
    for (const ArcIndex arc_index : impacted_arcs_[var]) {
      const ArcInfo& arc = arcs_[arc_index];
      if (integer_trail_->LowerBound(arc.tail_var) + ArcOffset(arc) >
          integer_trail_->LowerBound(arc.head_var)) {
        return false;
      }
    }
  }
  return true;
}

void PrecedencesPropagator::InitializeBFQueueWithModifiedNodes() {
  // Sparse clear of the queue. TODO(user): only use the sparse version if
  // queue.size() is small or use SparseBitset.
  const int num_nodes = impacted_arcs_.size();
  bf_in_queue_.resize(num_nodes, false);
  for (const int node : bf_queue_) bf_in_queue_[node] = false;
  bf_queue_.clear();
  DCHECK(std::none_of(bf_in_queue_.begin(), bf_in_queue_.end(),
                      [](bool v) { return v; }));
  for (const IntegerVariable var : modified_vars_.PositionsSetAtLeastOnce()) {
    if (var >= num_nodes) continue;
    bf_queue_.push_back(var.value());
    bf_in_queue_[var.value()] = true;
  }
}

void PrecedencesPropagator::CleanUpMarkedArcsAndParents() {
  // To be sparse, we use the fact that each node with a parent must be in
  // modified_vars_.
  const int num_nodes = impacted_arcs_.size();
  for (const IntegerVariable var : modified_vars_.PositionsSetAtLeastOnce()) {
    if (var >= num_nodes) continue;
    const ArcIndex parent_arc_index = bf_parent_arc_of_[var.value()];
    if (parent_arc_index != -1) {
      arcs_[parent_arc_index].is_marked = false;
      bf_parent_arc_of_[var.value()] = -1;
      bf_can_be_skipped_[var.value()] = false;
    }
  }
  DCHECK(std::none_of(bf_parent_arc_of_.begin(), bf_parent_arc_of_.end(),
                      [](ArcIndex v) { return v != -1; }));
  DCHECK(std::none_of(bf_can_be_skipped_.begin(), bf_can_be_skipped_.end(),
                      [](bool v) { return v; }));
}

bool PrecedencesPropagator::DisassembleSubtree(
    int source, int target, std::vector<bool>* can_be_skipped) {
  // Note that we explore a tree, so we can do it in any order, and the one
  // below seems to be the fastest.
  tmp_vector_.clear();
  tmp_vector_.push_back(source);
  while (!tmp_vector_.empty()) {
    const int tail = tmp_vector_.back();
    tmp_vector_.pop_back();
    for (const ArcIndex arc_index : impacted_arcs_[IntegerVariable(tail)]) {
      const ArcInfo& arc = arcs_[arc_index];
      if (arc.is_marked) {
        arc.is_marked = false;  // mutable.
        if (arc.head_var.value() == target) return true;
        DCHECK(!(*can_be_skipped)[arc.head_var.value()]);
        (*can_be_skipped)[arc.head_var.value()] = true;
        tmp_vector_.push_back(arc.head_var.value());
      }
    }
  }
  return false;
}

void PrecedencesPropagator::AnalyzePositiveCycle(
    ArcIndex first_arc, Trail* trail, std::vector<Literal>* must_be_all_true,
    std::vector<Literal>* literal_reason,
    std::vector<IntegerLiteral>* integer_reason) {
  must_be_all_true->clear();
  literal_reason->clear();
  integer_reason->clear();

  // Follow bf_parent_arc_of_[] to find the cycle containing first_arc.
  const IntegerVariable first_arc_head = arcs_[first_arc].head_var;
  ArcIndex arc_index = first_arc;
  std::vector<ArcIndex> arc_on_cycle;

  // Just to be safe and avoid an infinite loop we use the fact that the maximum
  // cycle size on a graph with n nodes is of size n. If we have more in the
  // code below, it means first_arc is not part of a cycle according to
  // bf_parent_arc_of_[], which should never happen.
  const int num_nodes = impacted_arcs_.size();
  while (arc_on_cycle.size() <= num_nodes) {
    arc_on_cycle.push_back(arc_index);
    const ArcInfo& arc = arcs_[arc_index];
    if (arc.tail_var == first_arc_head) break;
    arc_index = bf_parent_arc_of_[arc.tail_var.value()];
    CHECK_NE(arc_index, ArcIndex(-1));
  }
  CHECK_NE(arc_on_cycle.size(), num_nodes + 1) << "Infinite loop.";

  // Compute the reason for this cycle.
  IntegerValue sum(0);
  for (const ArcIndex arc_index : arc_on_cycle) {
    const ArcInfo& arc = arcs_[arc_index];
    sum += ArcOffset(arc);
    AppendLowerBoundReasonIfValid(arc.offset_var, *integer_trail_,
                                  integer_reason);
    for (const Literal l : arc.presence_literals) {
      literal_reason->push_back(l.Negated());
    }
  }

  // TODO(user): what if the sum overflow? this is just a check so I guess
  // we don't really care, but fix the issue.
  CHECK_GT(sum, 0);
}

// Note that in our settings it is important to use an algorithm that tries to
// minimize the number of integer_trail_->Enqueue() as much as possible.
//
// TODO(user): The current algorithm is quite efficient, but there is probably
// still room for improvements.
bool PrecedencesPropagator::BellmanFordTarjan(Trail* trail) {
  const int num_nodes = impacted_arcs_.size();

  // These vector are reset by CleanUpMarkedArcsAndParents() so resize is ok.
  bf_can_be_skipped_.resize(num_nodes, false);
  bf_parent_arc_of_.resize(num_nodes, ArcIndex(-1));
  const auto cleanup =
      ::absl::MakeCleanup([this]() { CleanUpMarkedArcsAndParents(); });

  // The queue initialization is done by InitializeBFQueueWithModifiedNodes().
  while (!bf_queue_.empty()) {
    const int node = bf_queue_.front();
    bf_queue_.pop_front();
    bf_in_queue_[node] = false;

    // TODO(user): we don't need bf_can_be_skipped_ since we can detect this
    // if this node has a parent arc which is not marked. Investigate if it is
    // faster without the vector<bool>.
    //
    // TODO(user): An alternative algorithm is to remove all these nodes from
    // the queue instead of simply marking them. This should also lead to a
    // better "relaxation" order of the arcs. It is however a bit more work to
    // remove them since we need to track their position.
    if (bf_can_be_skipped_[node]) {
      DCHECK_NE(bf_parent_arc_of_[node], -1);
      DCHECK(!arcs_[bf_parent_arc_of_[node]].is_marked);
      continue;
    }

    const IntegerValue tail_lb =
        integer_trail_->LowerBound(IntegerVariable(node));
    for (const ArcIndex arc_index : impacted_arcs_[IntegerVariable(node)]) {
      const ArcInfo& arc = arcs_[arc_index];
      DCHECK_EQ(arc.tail_var, node);
      const IntegerValue candidate = tail_lb + ArcOffset(arc);
      if (candidate > integer_trail_->LowerBound(arc.head_var)) {
        if (!EnqueueAndCheck(arc, candidate, trail)) return false;

        // This is the Tarjan contribution to Bellman-Ford. This code detect
        // positive cycle, and because it disassemble the subtree while doing
        // so, the cost is amortized during the algorithm execution. Another
        // advantages is that it will mark the node explored here as skippable
        // which will avoid to propagate them too early (knowing that they will
        // need to be propagated again later).
        if (DisassembleSubtree(arc.head_var.value(), arc.tail_var.value(),
                               &bf_can_be_skipped_)) {
          std::vector<Literal> must_be_all_true;
          AnalyzePositiveCycle(arc_index, trail, &must_be_all_true,
                               &literal_reason_, &integer_reason_);
          if (must_be_all_true.empty()) {
            ++num_cycles_;
            return integer_trail_->ReportConflict(literal_reason_,
                                                  integer_reason_);
          } else {
            gtl::STLSortAndRemoveDuplicates(&must_be_all_true);
            for (const Literal l : must_be_all_true) {
              if (trail_->Assignment().LiteralIsFalse(l)) {
                literal_reason_.push_back(l);
                return integer_trail_->ReportConflict(literal_reason_,
                                                      integer_reason_);
              }
            }
            for (const Literal l : must_be_all_true) {
              if (trail_->Assignment().LiteralIsTrue(l)) continue;
              integer_trail_->EnqueueLiteral(l, literal_reason_,
                                             integer_reason_);
            }

            // We just marked some optional variable as ignored, no need
            // to update bf_parent_arc_of_[].
            continue;
          }
        }

        // We need to enforce the invariant that only the arc_index in
        // bf_parent_arc_of_[] are marked (but not necessarily all of them
        // since we unmark some in DisassembleSubtree()).
        if (bf_parent_arc_of_[arc.head_var.value()] != -1) {
          arcs_[bf_parent_arc_of_[arc.head_var.value()]].is_marked = false;
        }

        // Tricky: We just enqueued the fact that the lower-bound of head is
        // candidate. However, because the domain of head may be discrete, it is
        // possible that the lower-bound of head is now higher than candidate!
        // If this is the case, we don't update bf_parent_arc_of_[] so that we
        // don't wrongly detect a positive weight cycle because of this "extra
        // push".
        const IntegerValue new_bound = integer_trail_->LowerBound(arc.head_var);
        if (new_bound == candidate) {
          bf_parent_arc_of_[arc.head_var.value()] = arc_index;
          arcs_[arc_index].is_marked = true;
        } else {
          // We still unmark any previous dependency, since we have pushed the
          // value of arc.head_var further.
          bf_parent_arc_of_[arc.head_var.value()] = -1;
        }

        // We do not re-enqueue if we are in a propagation loop and new_bound
        // was not pushed to candidate or higher.
        bf_can_be_skipped_[arc.head_var.value()] = false;
        if (!bf_in_queue_[arc.head_var.value()] && new_bound >= candidate) {
          bf_queue_.push_back(arc.head_var.value());
          bf_in_queue_[arc.head_var.value()] = true;
        }
      }
    }
  }
  return true;
}

void BinaryRelationRepository::Add(Literal lit, LinearExpression2 expr,
                                   IntegerValue lhs, IntegerValue rhs) {
  expr.MakeVariablesPositive();
  CHECK_NE(lit.Index(), kNoLiteralIndex);
  num_enforced_relations_++;
  DCHECK(expr.coeffs[0] == 0 || expr.vars[0] != kNoIntegerVariable);
  DCHECK(expr.coeffs[1] == 0 || expr.vars[1] != kNoIntegerVariable);

  relations_.push_back(
      {.enforcement = lit, .expr = expr, .lhs = lhs, .rhs = rhs});
}

void BinaryRelationRepository::AddPartialRelation(Literal lit,
                                                  IntegerVariable a,
                                                  IntegerVariable b) {
  DCHECK_NE(a, kNoIntegerVariable);
  DCHECK_NE(b, kNoIntegerVariable);
  DCHECK_NE(a, b);
  Add(lit, LinearExpression2(a, b, 1, 1), 0, 0);
}

void BinaryRelationRepository::Build() {
  DCHECK(!is_built_);
  is_built_ = true;
  std::vector<std::pair<LiteralIndex, int>> literal_key_values;
  const int num_relations = relations_.size();
  literal_key_values.reserve(num_enforced_relations_);
  for (int i = 0; i < num_relations; ++i) {
    const Relation& r = relations_[i];
    literal_key_values.emplace_back(r.enforcement.Index(), i);
  }
  lit_to_relations_.ResetFromPairs(literal_key_values);
}

bool BinaryRelationRepository::PropagateLocalBounds(
    const IntegerTrail& integer_trail,
    const RootLevelLinear2Bounds& root_level_bounds, Literal lit,
    const absl::flat_hash_map<IntegerVariable, IntegerValue>& input,
    absl::flat_hash_map<IntegerVariable, IntegerValue>* output) const {
  DCHECK_NE(lit.Index(), kNoLiteralIndex);

  auto get_lower_bound = [&](IntegerVariable var) {
    const auto it = input.find(var);
    if (it != input.end()) return it->second;
    return integer_trail.LevelZeroLowerBound(var);
  };
  auto get_upper_bound = [&](IntegerVariable var) {
    return -get_lower_bound(NegationOf(var));
  };
  auto update_lower_bound_by_var = [&](IntegerVariable var, IntegerValue lb) {
    if (lb <= integer_trail.LevelZeroLowerBound(var)) return;
    const auto [it, inserted] = output->insert({var, lb});
    if (!inserted) {
      it->second = std::max(it->second, lb);
    }
  };
  auto update_upper_bound_by_var = [&](IntegerVariable var, IntegerValue ub) {
    update_lower_bound_by_var(NegationOf(var), -ub);
  };
  auto update_var_bounds = [&](const LinearExpression2& expr, IntegerValue lhs,
                               IntegerValue rhs) {
    if (expr.coeffs[0] == 0) return;

    // lb(b.y) <= b.y <= ub(b.y) and lhs <= a.x + b.y <= rhs imply
    //   ceil((lhs - ub(b.y)) / a) <= x <= floor((rhs - lb(b.y)) / a)
    if (expr.coeffs[1] != 0) {
      lhs = lhs - expr.coeffs[1] * get_upper_bound(expr.vars[1]);
      rhs = rhs - expr.coeffs[1] * get_lower_bound(expr.vars[1]);
    }
    update_lower_bound_by_var(expr.vars[0],
                              MathUtil::CeilOfRatio(lhs, expr.coeffs[0]));
    update_upper_bound_by_var(expr.vars[0],
                              MathUtil::FloorOfRatio(rhs, expr.coeffs[0]));
  };
  auto update_var_bounds_from_relation = [&](Relation r) {
    r.expr.SimpleCanonicalization();

    update_var_bounds(r.expr, r.lhs, r.rhs);
    std::swap(r.expr.vars[0], r.expr.vars[1]);
    std::swap(r.expr.coeffs[0], r.expr.coeffs[1]);
    update_var_bounds(r.expr, r.lhs, r.rhs);
  };
  if (lit.Index() < lit_to_relations_.size()) {
    for (const int relation_index : lit_to_relations_[lit]) {
      update_var_bounds_from_relation(relations_[relation_index]);
    }
  }
  for (const auto& [var, _] : input) {
    for (const auto& [expr, lb, ub] :
         root_level_bounds.GetAllBoundsContainingVariable(var)) {
      update_var_bounds_from_relation(
          Relation{Literal(kNoLiteralIndex), expr, lb, ub});
    }
  }

  // Check feasibility.
  // TODO(user): we might do that earlier?
  for (const auto [var, lb] : *output) {
    if (lb > integer_trail.LevelZeroUpperBound(var)) return false;
  }
  return true;
}

bool GreaterThanAtLeastOneOfDetector::AddRelationFromIndices(
    IntegerVariable var, absl::Span<const Literal> clause,
    absl::Span<const int> indices, Model* model) {
  std::vector<AffineExpression> exprs;
  std::vector<Literal> selectors;
  absl::flat_hash_set<LiteralIndex> used;
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();

  const IntegerValue var_lb = integer_trail->LevelZeroLowerBound(var);
  for (const int index : indices) {
    Relation r = repository_.relation(index);
    if (r.expr.vars[0] != PositiveVariable(var)) {
      std::swap(r.expr.vars[0], r.expr.vars[1]);
      std::swap(r.expr.coeffs[0], r.expr.coeffs[1]);
    }
    CHECK_EQ(r.expr.vars[0], PositiveVariable(var));

    if ((r.expr.coeffs[0] == 1) == VariableIsPositive(var)) {
      //  a + b >= lhs
      if (r.lhs <= kMinIntegerValue) continue;
      exprs.push_back(
          AffineExpression(r.expr.vars[1], -r.expr.coeffs[1], r.lhs));
    } else {
      // -a + b <= rhs.
      if (r.rhs >= kMaxIntegerValue) continue;
      exprs.push_back(
          AffineExpression(r.expr.vars[1], r.expr.coeffs[1], -r.rhs));
    }

    // Ignore this entry if it is always true.
    if (var_lb >= integer_trail->LevelZeroUpperBound(exprs.back())) {
      exprs.pop_back();
      continue;
    }

    // Note that duplicate selector are supported.
    selectors.push_back(r.enforcement);
    used.insert(r.enforcement);
  }

  // The enforcement of the new constraint are simply the literal not used
  // above.
  std::vector<Literal> enforcements;
  for (const Literal l : clause) {
    if (!used.contains(l.Index())) {
      enforcements.push_back(l.Negated());
    }
  }

  // No point adding a constraint if there is not at least two different
  // literals in selectors.
  if (used.size() <= 1) return false;

  // Add the constraint.
  GreaterThanAtLeastOneOfPropagator* constraint =
      new GreaterThanAtLeastOneOfPropagator(var, exprs, selectors, enforcements,
                                            model);
  constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
  model->TakeOwnership(constraint);
  return true;
}

int GreaterThanAtLeastOneOfDetector::
    AddGreaterThanAtLeastOneOfConstraintsFromClause(
        const absl::Span<const Literal> clause, Model* model) {
  CHECK_EQ(model->GetOrCreate<Trail>()->CurrentDecisionLevel(), 0);
  if (clause.size() < 2) return 0;

  // Collect all relations impacted by this clause.
  std::vector<std::pair<IntegerVariable, int>> infos;
  for (const Literal l : clause) {
    for (const int index :
         repository_.IndicesOfRelationsEnforcedBy(l.Index())) {
      const Relation& r = repository_.relation(index);
      if (r.expr.vars[0] != kNoIntegerVariable &&
          IntTypeAbs(r.expr.coeffs[0]) == 1) {
        infos.push_back({r.expr.vars[0], index});
      }
      if (r.expr.vars[1] != kNoIntegerVariable &&
          IntTypeAbs(r.expr.coeffs[1]) == 1) {
        infos.push_back({r.expr.vars[1], index});
      }
    }
  }
  if (infos.size() <= 1) return 0;

  // Stable sort to regroup by var.
  std::stable_sort(infos.begin(), infos.end());

  // We process the info with same variable together.
  int num_added_constraints = 0;
  std::vector<int> indices;
  for (int i = 0; i < infos.size();) {
    const int start = i;
    const IntegerVariable var = infos[start].first;

    indices.clear();
    for (; i < infos.size() && infos[i].first == var; ++i) {
      indices.push_back(infos[i].second);
    }

    // Skip single relations, we are not interested in these.
    if (indices.size() < 2) continue;

    // Heuristic. Look for full or almost full clauses.
    //
    // TODO(user): We could add GreaterThanAtLeastOneOf() with more enforcement
    // literals. Experiment.
    if (indices.size() + 1 < clause.size()) continue;

    if (AddRelationFromIndices(var, clause, indices, model)) {
      ++num_added_constraints;
    }
    if (AddRelationFromIndices(NegationOf(var), clause, indices, model)) {
      ++num_added_constraints;
    }
  }
  return num_added_constraints;
}

int GreaterThanAtLeastOneOfDetector::
    AddGreaterThanAtLeastOneOfConstraintsWithClauseAutoDetection(Model* model) {
  auto* time_limit = model->GetOrCreate<TimeLimit>();
  auto* solver = model->GetOrCreate<SatSolver>();

  // Fill the set of interesting relations for each variables.
  util_intops::StrongVector<IntegerVariable, std::vector<int>> var_to_relations;
  for (int index = 0; index < repository_.size(); ++index) {
    const Relation& r = repository_.relation(index);
    if (r.enforcement.Index() == kNoLiteralIndex) continue;
    if (r.expr.vars[0] != kNoIntegerVariable &&
        IntTypeAbs(r.expr.coeffs[0]) == 1) {
      if (r.expr.vars[0] >= var_to_relations.size()) {
        var_to_relations.resize(r.expr.vars[0] + 1);
      }
      var_to_relations[r.expr.vars[0]].push_back(index);
    }
    if (r.expr.vars[1] != kNoIntegerVariable &&
        IntTypeAbs(r.expr.coeffs[1]) == 1) {
      if (r.expr.vars[1] >= var_to_relations.size()) {
        var_to_relations.resize(r.expr.vars[1] + 1);
      }
      var_to_relations[r.expr.vars[1]].push_back(index);
    }
  }

  int num_added_constraints = 0;
  for (IntegerVariable target(0); target < var_to_relations.size(); ++target) {
    if (var_to_relations[target].size() <= 1) continue;
    if (time_limit->LimitReached()) return num_added_constraints;

    // Detect set of incoming arcs for which at least one must be present.
    // TODO(user): Find more than one disjoint set of incoming arcs.
    // TODO(user): call MinimizeCoreWithPropagation() on the clause.
    solver->Backtrack(0);
    if (solver->ModelIsUnsat()) return num_added_constraints;
    std::vector<Literal> clause;
    for (const int index : var_to_relations[target]) {
      const Literal literal = repository_.relation(index).enforcement;
      if (solver->Assignment().LiteralIsFalse(literal)) continue;
      const SatSolver::Status status =
          solver->EnqueueDecisionAndBacktrackOnConflict(literal.Negated());
      if (status == SatSolver::INFEASIBLE) return num_added_constraints;
      if (status == SatSolver::ASSUMPTIONS_UNSAT) {
        // We need to invert it, since a clause is not all false.
        clause = solver->GetLastIncompatibleDecisions();
        for (Literal& ref : clause) ref = ref.Negated();
        break;
      }
    }
    solver->Backtrack(0);
    if (clause.size() <= 1) continue;

    // Recover the indices corresponding to this clause.
    const absl::btree_set<Literal> clause_set(clause.begin(), clause.end());

    std::vector<int> indices;
    for (const int index : var_to_relations[target]) {
      const Literal literal = repository_.relation(index).enforcement;
      if (clause_set.contains(literal)) {
        indices.push_back(index);
      }
    }

    // Try both direction.
    if (AddRelationFromIndices(target, clause, indices, model)) {
      ++num_added_constraints;
    }
    if (AddRelationFromIndices(NegationOf(target), clause, indices, model)) {
      ++num_added_constraints;
    }
  }

  solver->Backtrack(0);
  return num_added_constraints;
}

int GreaterThanAtLeastOneOfDetector::AddGreaterThanAtLeastOneOfConstraints(
    Model* model, bool auto_detect_clauses) {
  auto* time_limit = model->GetOrCreate<TimeLimit>();
  auto* solver = model->GetOrCreate<SatSolver>();
  auto* clauses = model->GetOrCreate<ClauseManager>();
  auto* logger = model->GetOrCreate<SolverLogger>();

  int num_added_constraints = 0;
  SOLVER_LOG(logger, "[Precedences] num_relations=", repository_.size(),
             " num_clauses=", clauses->AllClausesInCreationOrder().size());

  // We have two possible approaches. For now, we prefer the first one except if
  // there is too many clauses in the problem.
  //
  // TODO(user): Do more extensive experiment. Remove the second approach as
  // it is more time consuming? or identify when it make sense. Note that the
  // first approach also allows to use "incomplete" at least one between arcs.
  if (!auto_detect_clauses &&
      clauses->AllClausesInCreationOrder().size() < 1e6) {
    // TODO(user): This does not take into account clause of size 2 since they
    // are stored in the BinaryImplicationGraph instead. Some ideas specific
    // to size 2:
    // - There can be a lot of such clauses, but it might be nice to consider
    //   them. we need to experiments.
    // - The automatic clause detection might be a better approach and it
    //   could be combined with probing.
    for (const SatClause* clause : clauses->AllClausesInCreationOrder()) {
      if (time_limit->LimitReached()) return num_added_constraints;
      if (solver->ModelIsUnsat()) return num_added_constraints;
      num_added_constraints += AddGreaterThanAtLeastOneOfConstraintsFromClause(
          clause->AsSpan(), model);
    }

    // It is common that there is only two alternatives to push a variable.
    // In this case, our presolve most likely made sure that the two are
    // controlled by a single Boolean. This allows to detect this and add the
    // appropriate greater than at least one of.
    const int num_booleans = solver->NumVariables();
    if (num_booleans < 1e6) {
      for (int i = 0; i < num_booleans; ++i) {
        if (time_limit->LimitReached()) return num_added_constraints;
        if (solver->ModelIsUnsat()) return num_added_constraints;
        num_added_constraints +=
            AddGreaterThanAtLeastOneOfConstraintsFromClause(
                {Literal(BooleanVariable(i), true),
                 Literal(BooleanVariable(i), false)},
                model);
      }
    }

  } else {
    num_added_constraints +=
        AddGreaterThanAtLeastOneOfConstraintsWithClauseAutoDetection(model);
  }

  if (num_added_constraints > 0) {
    SOLVER_LOG(logger, "[Precedences] Added ", num_added_constraints,
               " GreaterThanAtLeastOneOf() constraints.");
  }

  return num_added_constraints;
}

ReifiedLinear2Bounds::ReifiedLinear2Bounds(Model* model)
    : integer_encoder_(model->GetOrCreate<IntegerEncoder>()),
      best_root_level_bounds_(model->GetOrCreate<RootLevelLinear2Bounds>()) {
  int index = 0;
  model->GetOrCreate<LevelZeroCallbackHelper>()->callbacks.push_back(
      [index = index, trail = model->GetOrCreate<Trail>(), this]() mutable {
        DCHECK_EQ(trail->CurrentDecisionLevel(), 0);
        absl::flat_hash_set<Literal> relevant_true_literals;
        for (; index < trail->Index(); ++index) {
          const Literal l = (*trail)[index];
          if (variable_appearing_in_reified_relations_.contains(l.Variable())) {
            relevant_true_literals.insert(l);
          }
        }
        if (relevant_true_literals.empty()) return true;

        // Linear scan.
        for (const auto [l, expr, ub] : all_reified_relations_) {
          if (relevant_true_literals.contains(l)) {
            best_root_level_bounds_->Add(expr, kMinIntegerValue, ub);
            VLOG(2) << "New fixed precedence: " << expr << " <= " << ub
                    << " (was reified by " << l << ")";
          } else if (relevant_true_literals.contains(l.Negated())) {
            best_root_level_bounds_->Add(expr, ub + 1, kMaxIntegerValue);
            VLOG(2) << "New fixed precedence: " << expr << " > " << ub
                    << " (was reified by not(" << l << "))";
          }
        }
        return true;
      });
}

Linear2BoundsFromLinear3::~Linear2BoundsFromLinear3() {
  if (!VLOG_IS_ON(1)) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back(
      {"Linear2BoundsFromLinear3/num_affine_updates", num_affine_updates_});
  shared_stats_->AddStats(stats);
}

RelationStatus ReifiedLinear2Bounds::GetLevelZeroPrecedenceStatus(
    AffineExpression a, AffineExpression b) const {
  const auto [expr, ub] = EncodeDifferenceLowerThan(a, b, 0);
  return best_root_level_bounds_->GetLevelZeroStatus(expr, kMinIntegerValue,
                                                     ub);
}

void ReifiedLinear2Bounds::AddReifiedPrecedenceIfNonTrivial(
    Literal l, AffineExpression a, AffineExpression b) {
  const auto [expr, ub] = EncodeDifferenceLowerThan(a, b, 0);
  const RelationStatus status =
      best_root_level_bounds_->GetLevelZeroStatus(expr, kMinIntegerValue, ub);
  if (status != RelationStatus::IS_UNKNOWN) return;

  relation_to_lit_.insert({{expr, ub}, l});

  variable_appearing_in_reified_relations_.insert(l.Variable());
  all_reified_relations_.push_back({l, expr, ub});
}

LiteralIndex ReifiedLinear2Bounds::GetReifiedPrecedence(AffineExpression a,
                                                        AffineExpression b) {
  const auto [expr, ub] = EncodeDifferenceLowerThan(a, b, 0);
  const RelationStatus status =
      best_root_level_bounds_->GetLevelZeroStatus(expr, kMinIntegerValue, ub);
  if (status == RelationStatus::IS_TRUE) {
    return integer_encoder_->GetTrueLiteral().Index();
  }
  if (status == RelationStatus::IS_FALSE) {
    return integer_encoder_->GetFalseLiteral().Index();
  }

  const auto it = relation_to_lit_.find({expr, ub});
  if (it == relation_to_lit_.end()) return kNoLiteralIndex;
  return it->second;
}

Linear2BoundsFromLinear3::Linear2BoundsFromLinear3(Model* model)
    : integer_trail_(model->GetOrCreate<IntegerTrail>()),
      trail_(model->GetOrCreate<Trail>()),
      linear2_watcher_(model->GetOrCreate<Linear2Watcher>()),
      watcher_(model->GetOrCreate<GenericLiteralWatcher>()),
      shared_stats_(model->GetOrCreate<SharedStatistics>()),
      root_level_bounds_(model->GetOrCreate<RootLevelLinear2Bounds>()),
      lin2_indices_(model->GetOrCreate<Linear2Indices>()) {}

// Note that for speed we do not compare to the trivial or root level bounds.
//
// It is okay to still store it in the hash-map, since at worst we will have no
// more entries than 3 * number_of_linear3_in_the_problem.
bool Linear2BoundsFromLinear3::AddAffineUpperBound(
    LinearExpression2Index lin2_index, IntegerValue lin_expr_gcd,
    AffineExpression affine_ub) {
  // At level zero, just add it to root_level_bounds_.
  if (trail_->CurrentDecisionLevel() == 0 || affine_ub.IsConstant()) {
    root_level_bounds_->AddUpperBound(
        lin2_index, FloorRatio(integer_trail_->LevelZeroUpperBound(affine_ub),
                               lin_expr_gcd));
    return false;  // Not important.
  }

  // We have gcd * canonical_expr <= affine_ub,
  // so we do need to store a "divisor".
  if (lin2_index >= best_affine_ub_.size()) {
    best_affine_ub_.resize(lin2_index.value() + 1, {AffineExpression(), 0});
  }
  auto& [old_affine_ub, old_divisor] = best_affine_ub_[lin2_index];
  if (old_divisor != 0) {
    // We have an affine bound for this expr in the map. Can be exactly the
    // same, a better one or a worse one.
    //
    // Note that we expect exactly the same most of the time as it should be
    // rare to have many linear3 "competing" for the same linear2 bound.
    if (old_affine_ub == affine_ub && old_divisor == lin_expr_gcd) {
      linear2_watcher_->NotifyBoundChanged(
          lin2_indices_->GetExpression(lin2_index));
      return false;
    }

    const IntegerValue new_ub =
        FloorRatioWithTest(integer_trail_->UpperBound(affine_ub), lin_expr_gcd);
    const IntegerValue old_ub = FloorRatioWithTest(
        integer_trail_->UpperBound(old_affine_ub), old_divisor);
    if (old_ub <= new_ub) return false;  // old bound is better.

    best_affine_ub_[lin2_index] = {affine_ub, lin_expr_gcd};  // Overwrite.
  } else {
    // Note that this should almost never happen (only once per lin2).
    best_affine_ub_[lin2_index] = {affine_ub, lin_expr_gcd};
  }

  ++num_affine_updates_;
  linear2_watcher_->NotifyBoundChanged(
      lin2_indices_->GetExpression(lin2_index));
  return true;
}

IntegerValue Linear2BoundsFromLinear3::GetUpperBoundFromLinear3(
    LinearExpression2Index lin2_index) const {
  if (lin2_index >= best_affine_ub_.size()) return kMaxIntegerValue;
  auto [affine, divisor] = best_affine_ub_[lin2_index];
  if (divisor == 0) return kMaxIntegerValue;
  return FloorRatio(integer_trail_->UpperBound(affine), divisor);
}

void Linear2BoundsFromLinear3::AddReasonForUpperBoundLowerThan(
    LinearExpression2Index lin2_index, IntegerValue ub,
    std::vector<Literal>* /*literal_reason*/,
    std::vector<IntegerLiteral>* integer_reason) const {
  DCHECK_LE(GetUpperBoundFromLinear3(lin2_index), ub);
  DCHECK_LT(lin2_index, best_affine_ub_.size());

  // We want the reason for "expr <= ub"
  // knowing that expr <= affine / divisor.
  const auto [affine, divisor] = best_affine_ub_[lin2_index];
  DCHECK_NE(divisor, 0);
  integer_reason->push_back(affine.LowerOrEqual(CapProdI(ub + 1, divisor) - 1));
}

IntegerValue Linear2Bounds::UpperBound(
    LinearExpression2Index lin2_index) const {
  return std::min(
      NonTrivialUpperBound(lin2_index),
      integer_trail_->UpperBound(lin2_indices_->GetExpression(lin2_index)));
}

IntegerValue Linear2Bounds::UpperBound(LinearExpression2 expr) const {
  expr.SimpleCanonicalization();
  if (expr.coeffs[0] == 0) {
    return integer_trail_->UpperBound(expr);
  }
  DCHECK_NE(expr.coeffs[1], 0);
  const IntegerValue gcd = expr.DivideByGcd();
  IntegerValue ub = integer_trail_->UpperBound(expr);
  const LinearExpression2Index index = lin2_indices_->GetIndex(expr);
  if (index != kNoLinearExpression2Index) {
    ub = std::min(ub, root_level_bounds_->GetUpperBoundNoTrail(index));
    ub = std::min(ub, enforced_bounds_->GetUpperBoundFromEnforced(index));
    ub = std::min(ub, linear3_bounds_->GetUpperBoundFromLinear3(index));
  }
  return CapProdI(gcd, ub);
}

void Linear2Bounds::AddReasonForUpperBoundLowerThan(
    LinearExpression2 expr, IntegerValue ub,
    std::vector<Literal>* literal_reason,
    std::vector<IntegerLiteral>* integer_reason) const {
  DCHECK_LE(UpperBound(expr), ub);

  // Explanation are by order of preference, with no reason needed first.
  if (integer_trail_->LevelZeroUpperBound(expr) <= ub) {
    return;
  }
  expr.SimpleCanonicalization();
  const IntegerValue gcd = expr.DivideByGcd();
  ub = FloorRatio(ub, gcd);
  const LinearExpression2Index index = lin2_indices_->GetIndex(expr);
  if (index != kNoLinearExpression2Index) {
    // No reason.
    if (root_level_bounds_->GetUpperBoundNoTrail(index) <= ub) {
      return;
    }
    // This one is a single literal.
    if (enforced_bounds_->GetUpperBoundFromEnforced(index) <= ub) {
      return enforced_bounds_->AddReasonForUpperBoundLowerThan(
          index, ub, literal_reason, integer_reason);
    }
    // This one is a single var upper bound.
    if (linear3_bounds_->GetUpperBoundFromLinear3(index) <= ub) {
      return linear3_bounds_->AddReasonForUpperBoundLowerThan(
          index, ub, literal_reason, integer_reason);
    }
  }

  // Trivial linear2 bounds from its variables.
  const IntegerValue implied_ub = integer_trail_->UpperBound(expr);
  const IntegerValue slack = ub - implied_ub;
  DCHECK_GE(slack, 0);
  expr.Negate();  // AppendRelaxedLinearReason() explains a lower bound.
  absl::Span<const IntegerVariable> vars = expr.non_zero_vars();
  absl::Span<const IntegerValue> coeffs = expr.non_zero_coeffs();
  integer_trail_->AppendRelaxedLinearReason(slack, coeffs, vars,
                                            integer_reason);
}

}  // namespace sat
}  // namespace operations_research
