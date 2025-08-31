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
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

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
#include "ortools/base/strong_vector.h"
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
  IntegerValue known_ub = integer_trail_->LevelZeroUpperBound(expr);
  IntegerValue known_lb = integer_trail_->LevelZeroLowerBound(expr);

  if (lb <= known_lb && ub >= known_ub) return RelationStatus::IS_TRUE;
  if (lb > known_ub || ub < known_lb) return RelationStatus::IS_FALSE;

  expr.SimpleCanonicalization();
  if (expr.coeffs[0] == 0) {
    return RelationStatus::IS_UNKNOWN;
  }
  DCHECK_NE(expr.coeffs[1], 0);
  const IntegerValue gcd = expr.DivideByGcd();
  ub = FloorRatio(ub, gcd);
  const LinearExpression2Index index = lin2_indices_->GetIndex(expr);

  if (index == kNoLinearExpression2Index) {
    return RelationStatus::IS_UNKNOWN;
  }

  known_ub = std::min(known_ub, GetUpperBoundNoTrail(index));
  known_lb = std::max(known_lb, -GetUpperBoundNoTrail(NegationOf(index)));

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

    // Note(user): We used to check
    //   entry.rhs <= root_level_bounds_->LevelZeroUpperBound(index));
    // But that assumed level zero bounds where only added at level zero, if we
    // add them at an higher level, some of the enforced relations here might be
    // worse than the fixed one we have. This is not a big deal, as we will not
    // add then again on bactrack, and we should use the level-zero reason in
    // that case.
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
  // add too many relations per call.
  int total_work = 0;
  const int kWorkLimit = params_->transitive_precedences_work_limit();
  if (kWorkLimit > 0) {
    for (const IntegerVariable var : topological_order_) {
      const int work = root_level_bounds_->AugmentSimpleRelations(
          var, kWorkLimit - total_work);
      total_work += work;
      if (total_work >= kWorkLimit) break;
    }
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

BinaryRelationRepository::~BinaryRelationRepository() {
  if (!VLOG_IS_ON(1)) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back({"BinaryRelationRepository/num_enforced_relations",
                   num_enforced_relations_});
  stats.push_back({"BinaryRelationRepository/num_encoded_equivalences",
                   num_encoded_equivalences_});
  shared_stats_->AddStats(stats);
}

void BinaryRelationRepository::Add(Literal lit, LinearExpression2 expr,
                                   IntegerValue lhs, IntegerValue rhs) {
  expr.SimpleCanonicalization();
  expr.CanonicalizeAndUpdateBounds(lhs, rhs);

  // BinaryRelationRepository uses a different canonicalization than the rest of
  // the code.
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
  lit_to_relations_.Add({});  // One extra unit size to make sure the negation
                              // cannot be out of bounds in lit_to_relations_.

  // If we have "l => (x <= 6)" and "~l => (x >= 9)" we can push
  // "l <=> (x <= 6)" to the repository of fully encoded linear2 bounds.
  // More generally, if we have:
  //
  //    l => (expr <= a)
  //   ~l => (expr >= b)
  //
  // And if moreover a < b, we have the following truth table:
  //
  //   l | expr <= a | a < expr < b | expr >= b
  //   --+-----------+---------------+----------
  //   0 |    false  |     false     |   true    (from "~l => (expr >= b)")
  //   1 |    true   |     false     |   false   (from "l => (expr <= a)")
  //
  //  So we can generalize the expressions to equivalences:
  //    l <=> (expr <= a)
  //   ~l <=> (expr >= b)
  //    (a < expr < b) is impossible
  absl::flat_hash_map<LinearExpression2, IntegerValue> lin2_to_upper_bound;
  for (LiteralIndex lit_index{0}; lit_index < lit_to_relations_.size() - 1;
       ++lit_index) {
    const Literal lit(lit_index);
    lin2_to_upper_bound.clear();
    const absl::Span<const int> relations = lit_to_relations_[lit_index];
    const absl::Span<const int> relations_negation =
        lit_to_relations_[lit.NegatedIndex()];
    if (relations.empty() || relations_negation.empty()) continue;
    for (const int relation_index : relations) {
      const Relation& r = relations_[relation_index];
      LinearExpression2 expr = r.expr;
      if (expr.coeffs[0] == 0) continue;
      expr.SimpleCanonicalization();  // Since relations_ uses a different
                                      // canonicalization convention.
      DCHECK_EQ(expr.DivideByGcd(), 1);
      {
        const auto [it, inserted] = lin2_to_upper_bound.insert({expr, r.rhs});
        if (!inserted) {
          it->second = std::min(it->second, r.rhs);
        }
      }
      {
        expr.Negate();
        const auto [it, inserted] = lin2_to_upper_bound.insert({expr, -r.lhs});
        if (!inserted) {
          it->second = std::min(it->second, -r.lhs);
        }
      }
    }
    for (const int relation_index : relations_negation) {
      const Relation& r = relations_[relation_index];
      LinearExpression2 canonical_expr = r.expr;
      canonical_expr.SimpleCanonicalization();
      if (canonical_expr.coeffs[0] == 0) continue;
      DCHECK_EQ(canonical_expr.DivideByGcd(), 1);

      // Let's work with lower bounds only.
      const IntegerValue lower_bounds[2] = {r.lhs, -r.rhs};
      LinearExpression2 exprs[2] = {canonical_expr, canonical_expr};
      exprs[1].Negate();
      for (int i = 0; i < 2; ++i) {
        // We have here "~l => (exprs[i] >= lower_bounds[i])".
        const auto it = lin2_to_upper_bound.find(exprs[i]);
        if (it != lin2_to_upper_bound.end()) {
          const IntegerValue ub = it->second;
          // Here we have "l => expr <= ub".
          if (ub >= lower_bounds[i]) {
            // Don't obey the "a < b" condition
            continue;
          }
          num_encoded_equivalences_++;

          // Make both relationships two-way.
          reified_linear2_bounds_->AddBoundEncodingIfNonTrivial(lit, exprs[i],
                                                                ub);
          LinearExpression2 expr = exprs[i];
          expr.Negate();
          reified_linear2_bounds_->AddBoundEncodingIfNonTrivial(
              lit.Negated(), expr, -lower_bounds[i]);
        }
      }
    }
  }
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
    : best_root_level_bounds_(model->GetOrCreate<RootLevelLinear2Bounds>()),
      lin2_indices_(model->GetOrCreate<Linear2Indices>()),
      shared_stats_(model->GetOrCreate<SharedStatistics>()) {
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
        for (const auto [l, expr_index, ub] : all_reified_relations_) {
          if (relevant_true_literals.contains(l)) {
            ++num_relations_fixed_at_root_level_;
            best_root_level_bounds_->AddUpperBound(expr_index, ub);
            VLOG(2) << "New fixed precedence: "
                    << lin2_indices_->GetExpression(expr_index) << " <= " << ub
                    << " (was reified by " << l << ")";
          } else if (relevant_true_literals.contains(l.Negated())) {
            ++num_relations_fixed_at_root_level_;
            best_root_level_bounds_->AddLowerBound(expr_index, ub + 1);
            VLOG(2) << "New fixed precedence: "
                    << lin2_indices_->GetExpression(expr_index) << " > " << ub
                    << " (was reified by not(" << l << "))";
          }
        }
        return true;
      });
}

ReifiedLinear2Bounds::~ReifiedLinear2Bounds() {
  if (!VLOG_IS_ON(1)) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back(
      {"ReifiedLinear2Bounds/num_linear3_relations", num_linear3_relations_});
  stats.push_back(
      {"ReifiedLinear2Bounds/num_literal_relations", relation_to_lit_.size()});
  stats.push_back({"ReifiedLinear2Bounds/num_relations_fixed_at_root_level",
                   num_relations_fixed_at_root_level_});
  shared_stats_->AddStats(stats);
}

void ReifiedLinear2Bounds::AddBoundEncodingIfNonTrivial(Literal l,
                                                        LinearExpression2 expr,
                                                        IntegerValue ub) {
  DCHECK(expr.IsCanonicalized());
  DCHECK_EQ(expr.DivideByGcd(), 1);
  const RelationStatus status =
      best_root_level_bounds_->GetLevelZeroStatus(expr, kMinIntegerValue, ub);
  if (status != RelationStatus::IS_UNKNOWN) return;

  if (expr.vars[0] == kNoIntegerVariable) {
    // For a single Affine GetEncodedBound() will return the IntegerLiteral
    // without needing any indexing.
    return;
  }
  const LinearExpression2Index expr_index = lin2_indices_->AddOrGet(expr);
  relation_to_lit_.insert({{expr_index, ub}, l});
  variable_appearing_in_reified_relations_.insert(l.Variable());
  all_reified_relations_.push_back({l, expr_index, ub});
}

std::variant<ReifiedLinear2Bounds::ReifiedBoundType, Literal, IntegerLiteral>
ReifiedLinear2Bounds::GetEncodedBound(LinearExpression2 expr, IntegerValue ub) {
  DCHECK(expr.IsCanonicalized());
  DCHECK_EQ(expr.DivideByGcd(), 1);
  const RelationStatus status =
      best_root_level_bounds_->GetLevelZeroStatus(expr, kMinIntegerValue, ub);
  if (status == RelationStatus::IS_TRUE) {
    return ReifiedBoundType::kAlwaysTrue;
  }
  if (status == RelationStatus::IS_FALSE) {
    return ReifiedBoundType::kAlwaysFalse;
  }
  if (expr.vars[0] == kNoIntegerVariable) {
    DCHECK_NE(expr.vars[1], kNoIntegerVariable);
    DCHECK_EQ(expr.coeffs[1], 1);
    return IntegerLiteral::LowerOrEqual(expr.vars[1], ub);
  }

  const LinearExpression2Index expr_index = lin2_indices_->GetIndex(expr);
  if (expr_index == kNoLinearExpression2Index) {
    return ReifiedBoundType::kNoLiteralStored;
  }
  const auto it = relation_to_lit_.find({expr_index, ub});
  if (it != relation_to_lit_.end()) return it->second;
  if (linear3_bounds_.size() <= expr_index) {
    return ReifiedBoundType::kNoLiteralStored;
  }
  const auto [affine_expr, divisor] = linear3_bounds_[expr_index];
  if (divisor == 0) {
    return ReifiedBoundType::kNoLiteralStored;
  }
  const IntegerValue affine_bound = CapProdI(ub, divisor);
  if (affine_bound == kMaxIntegerValue) {
    return ReifiedBoundType::kNoLiteralStored;
  }
  return affine_expr.LowerOrEqual(affine_bound);
}

void ReifiedLinear2Bounds::AddLinear3(absl::Span<const IntegerVariable> vars,
                                      absl::Span<const int64_t> coeffs,
                                      int64_t activity) {
  DCHECK_EQ(vars.size(), 3);
  DCHECK_EQ(coeffs.size(), 3);
  for (int i = 0; i < vars.size(); ++i) {
    LinearExpression2 expr(vars[i], vars[(i + 1) % 3], coeffs[i],
                           coeffs[(i + 1) % 3]);
    AffineExpression affine_expr(vars[(i + 2) % 3], -coeffs[(i + 2) % 3],
                                 activity);
    expr.SimpleCanonicalization();
    const IntegerValue gcd = expr.DivideByGcd();
    const LinearExpression2Index expr_index = lin2_indices_->AddOrGet(expr);
    if (linear3_bounds_.size() <= expr_index) {
      linear3_bounds_.resize(expr_index + 1, {AffineExpression(), 0});
    }
    auto& [old_affine_expr, old_divisor] = linear3_bounds_[expr_index];
    if (old_divisor == 0 || old_divisor > gcd) {
      linear3_bounds_[expr_index] = {affine_expr, gcd};
      if (old_divisor == 0) ++num_linear3_relations_;
    }
  }
}

Linear2BoundsFromLinear3::Linear2BoundsFromLinear3(Model* model)
    : integer_trail_(model->GetOrCreate<IntegerTrail>()),
      trail_(model->GetOrCreate<Trail>()),
      linear2_watcher_(model->GetOrCreate<Linear2Watcher>()),
      watcher_(model->GetOrCreate<GenericLiteralWatcher>()),
      shared_stats_(model->GetOrCreate<SharedStatistics>()),
      root_level_bounds_(model->GetOrCreate<RootLevelLinear2Bounds>()),
      lin2_indices_(model->GetOrCreate<Linear2Indices>()) {}

Linear2BoundsFromLinear3::~Linear2BoundsFromLinear3() {
  if (!VLOG_IS_ON(1)) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back(
      {"Linear2BoundsFromLinear3/num_affine_updates", num_affine_updates_});
  shared_stats_->AddStats(stats);
}

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

RelationStatus Linear2Bounds::GetStatus(LinearExpression2 expr, IntegerValue lb,
                                        IntegerValue ub) const {
  expr.SimpleCanonicalization();
  const IntegerValue gcd = expr.DivideByGcd();
  const LinearExpression2Index index = lin2_indices_->GetIndex(expr);
  IntegerValue known_ub;
  IntegerValue known_lb;
  if (index == kNoLinearExpression2Index) {
    known_ub = CapProdI(gcd, integer_trail_->UpperBound(expr));
    expr.Negate();
    known_lb = -CapProdI(gcd, integer_trail_->UpperBound(expr));
  } else {
    known_ub = CapProdI(gcd, UpperBound(index));
    known_lb = -CapProdI(gcd, UpperBound(NegationOf(index)));
  }
  if (lb <= known_lb && ub >= known_ub) return RelationStatus::IS_TRUE;
  if (lb > known_ub || ub < known_lb) return RelationStatus::IS_FALSE;

  return RelationStatus::IS_UNKNOWN;
}

bool Linear2Bounds::EnqueueLowerOrEqual(
    LinearExpression2 expr, IntegerValue ub,
    absl::Span<const Literal> literal_reason,
    absl::Span<const IntegerLiteral> integer_reason) {
  using ReifiedBoundType = ReifiedLinear2Bounds::ReifiedBoundType;
  expr.SimpleCanonicalization();
  const IntegerValue gcd = expr.DivideByGcd();
  ub = FloorRatio(ub, gcd);
  // We have many different scenarios here, each one pushing something different
  // in the trail.

  // Trivial.
  if (expr.coeffs[0] == 0 && expr.coeffs[1] == 0) {
    if (ub >= 0) {
      return true;
    } else {
      return integer_trail_->ReportConflict(literal_reason, integer_reason);
    }
  }

  // Degenerate single variable case, just push the IntegerLiteral.
  if (expr.coeffs[0] == 0) {
    return integer_trail_->Enqueue(
        IntegerLiteral::LowerOrEqual(expr.vars[1], ub), literal_reason,
        integer_reason);
  }

  // TODO(user): also check partially-encoded bounds, e.g. (expr <= ub) => l,
  // which might be in BinaryRelationRepository as ~l => (-expr <= - ub - 1).
  const auto reified_bound = reified_lin2_bounds_->GetEncodedBound(expr, ub);

  // Already true.
  if (std::holds_alternative<ReifiedBoundType>(reified_bound) &&
      std::get<ReifiedBoundType>(reified_bound) ==
          ReifiedBoundType::kAlwaysTrue) {
    return true;
  }

  // Conflict (ub < lb).
  if (std::holds_alternative<ReifiedBoundType>(reified_bound) &&
      std::get<ReifiedBoundType>(reified_bound) ==
          ReifiedBoundType::kAlwaysFalse) {
    LinearExpression2 negated_expr = expr;
    negated_expr.Negate();
    DCHECK_LT(ub, -UpperBound(negated_expr));
    std::vector<Literal> tmp_literal_reason(literal_reason.begin(),
                                            literal_reason.end());
    std::vector<IntegerLiteral> tmp_integer_reason(integer_reason.begin(),
                                                   integer_reason.end());
    AddReasonForUpperBoundLowerThan(negated_expr, -ub - 1, &tmp_literal_reason,
                                    &tmp_integer_reason);
    return integer_trail_->ReportConflict(tmp_literal_reason,
                                          tmp_integer_reason);
  }

  // Now all the cases below are pushing a proper linear2 bound. If we are at
  // level zero, store this in the root_level_bounds_.
  if (trail_->CurrentDecisionLevel() == 0) {
    root_level_bounds_->AddUpperBound(expr, ub);
  }

  // We don't have anything encoding this linear2 bound. Push the bounds of
  // its two variables.
  if (std::holds_alternative<ReifiedBoundType>(reified_bound) &&
      std::get<ReifiedBoundType>(reified_bound) ==
          ReifiedBoundType::kNoLiteralStored) {
    // TODO(user): create a Linear2 trail and enqueue this bound there.
    std::vector<IntegerLiteral> tmp_integer_reason(integer_reason.begin(),
                                                   integer_reason.end());
    const IntegerValue var_0_ub = FloorRatio(
        ub - integer_trail_->LowerBound(expr.vars[1]) * expr.coeffs[1],
        expr.coeffs[0]);
    const IntegerValue var_1_ub = FloorRatio(
        ub - integer_trail_->LowerBound(expr.vars[0]) * expr.coeffs[0],
        expr.coeffs[1]);

    tmp_integer_reason.push_back(IntegerLiteral::GreaterOrEqual(
        expr.vars[1], integer_trail_->LowerBound(expr.vars[1])));
    if (!integer_trail_->Enqueue(
            IntegerLiteral::LowerOrEqual(expr.vars[0], var_0_ub),
            literal_reason, tmp_integer_reason)) {
      return false;
    }
    tmp_integer_reason.pop_back();

    tmp_integer_reason.push_back(IntegerLiteral::GreaterOrEqual(
        expr.vars[0], integer_trail_->LowerBound(expr.vars[0])));
    if (!integer_trail_->Enqueue(
            IntegerLiteral::LowerOrEqual(expr.vars[1], var_1_ub),
            literal_reason, tmp_integer_reason)) {
      return false;
    }
    return true;
  }

  // We have a literal encoding for this linear2 bound, push it.
  if (std::holds_alternative<Literal>(reified_bound)) {
    // TODO(user): push this to EnforcedLinear2Bounds so the same
    // propagator that enqueued this bound will get the new linear2 bound if
    // request it to this class without needing to wait the propagation fix
    // point.
    const Literal literal = std::get<Literal>(reified_bound);

    integer_trail_->SafeEnqueueLiteral(literal, literal_reason, integer_reason);
    return true;
  }

  // We can encode this linear2 bound with an IntegerLiteral (probably coming
  // from a linear3). Push the IntegerLiteral.
  if (std::holds_alternative<IntegerLiteral>(reified_bound)) {
    // TODO(user): update Linear2BoundsFromLinear3.
    const IntegerLiteral literal = std::get<IntegerLiteral>(reified_bound);
    return integer_trail_->Enqueue(literal, literal_reason, integer_reason);
  }
  LOG(FATAL) << "Unknown reified bound type";
  return false;
}

}  // namespace sat
}  // namespace operations_research
