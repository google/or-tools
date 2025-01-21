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

#include "ortools/sat/solution_crush.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/algorithms/sparse_permutation.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

void SolutionCrush::Resize(int new_size) {
  hint_.resize(new_size, 0);
  hint_has_value_.resize(new_size, false);
}

void SolutionCrush::LoadSolution(
    const absl::flat_hash_map<int, int64_t>& solution) {
  CHECK(!hint_is_loaded_);
  model_has_hint_ = !solution.empty();
  hint_is_loaded_ = true;
  for (const auto [var, value] : solution) {
    hint_has_value_[var] = true;
    hint_[var] = value;
  }
}

void SolutionCrush::MaybeSetLiteralToValueEncoding(int literal, int var,
                                                   int64_t value) {
  if (hint_is_loaded_) {
    const int bool_var = PositiveRef(literal);
    DCHECK(RefIsPositive(var));
    if (!hint_has_value_[bool_var] && hint_has_value_[var]) {
      const int64_t bool_value = hint_[var] == value ? 1 : 0;
      hint_has_value_[bool_var] = true;
      hint_[bool_var] = RefIsPositive(literal) ? bool_value : 1 - bool_value;
    }
  }
}

void SolutionCrush::SetVarToLinearExpression(
    int new_var, absl::Span<const std::pair<int, int64_t>> linear) {
  // We only fill the hint of the new variable if all the variable involved
  // in its definition have a value.
  if (hint_is_loaded_) {
    int64_t new_value = 0;
    for (const auto [var, coeff] : linear) {
      CHECK_GE(var, 0);
      CHECK_LE(var, hint_.size());
      if (!hint_has_value_[var]) return;
      new_value += coeff * hint_[var];
    }
    hint_has_value_[new_var] = true;
    hint_[new_var] = new_value;
  }
}

void SolutionCrush::SetVarToClause(int new_var, absl::Span<const int> clause) {
  if (hint_is_loaded_) {
    int new_hint = 0;
    bool all_have_hint = true;
    for (const int literal : clause) {
      const int var = PositiveRef(literal);
      if (!hint_has_value_[var]) {
        all_have_hint = false;
        break;
      }
      if (hint_[var] == (RefIsPositive(literal) ? 1 : 0)) {
        new_hint = 1;
        break;
      }
    }
    // Leave the `new_var` hint unassigned if any literal is not hinted.
    if (all_have_hint) {
      hint_has_value_[new_var] = true;
      hint_[new_var] = new_hint;
    }
  }
}

void SolutionCrush::SetVarToConjunction(int new_var,
                                        absl::Span<const int> conjunction) {
  if (hint_is_loaded_) {
    int new_hint = 1;
    bool all_have_hint = true;
    for (const int literal : conjunction) {
      const int var = PositiveRef(literal);
      if (!hint_has_value_[var]) {
        all_have_hint = false;
        break;
      }
      if (hint_[var] == (RefIsPositive(literal) ? 0 : 1)) {
        new_hint = 0;
        break;
      }
    }
    // Leave the `new_var` hint unassigned if any literal is not hinted.
    if (all_have_hint) {
      hint_has_value_[new_var] = true;
      hint_[new_var] = new_hint;
    }
  }
}

void SolutionCrush::SetVarToValueIfLinearConstraintViolated(
    int new_var, int64_t value,
    absl::Span<const std::pair<int, int64_t>> linear, const Domain& domain) {
  if (hint_is_loaded_) {
    int64_t linear_value = 0;
    bool all_have_hint = true;
    for (const auto [var, coeff] : linear) {
      if (!hint_has_value_[var]) {
        all_have_hint = false;
        break;
      }
      linear_value += hint_[var] * coeff;
    }
    if (all_have_hint && !domain.Contains(linear_value)) {
      hint_has_value_[new_var] = true;
      hint_[new_var] = value;
    }
  }
}

void SolutionCrush::SetLiteralToValueIfLinearConstraintViolated(
    int literal, bool value, absl::Span<const std::pair<int, int64_t>> linear,
    const Domain& domain) {
  SetVarToValueIfLinearConstraintViolated(
      PositiveRef(literal), RefIsPositive(literal) ? value : !value, linear,
      domain);
}

void SolutionCrush::SetVarToValueIf(int var, int64_t value, int condition_lit) {
  SetVarToValueIfLinearConstraintViolated(
      var, value, {{PositiveRef(condition_lit), 1}},
      Domain(RefIsPositive(condition_lit) ? 0 : 1));
}

void SolutionCrush::SetLiteralToValueIf(int literal, bool value,
                                        int condition_lit) {
  SetLiteralToValueIfLinearConstraintViolated(
      literal, value, {{PositiveRef(condition_lit), 1}},
      Domain(RefIsPositive(condition_lit) ? 0 : 1));
}

void SolutionCrush::MakeLiteralsEqual(int lit1, int lit2) {
  if (!hint_is_loaded_) return;
  if (hint_has_value_[PositiveRef(lit2)]) {
    SetLiteralHint(lit1, LiteralSolutionHint(lit2));
  } else if (hint_has_value_[PositiveRef(lit1)]) {
    SetLiteralHint(lit2, LiteralSolutionHint(lit1));
  }
}

void SolutionCrush::UpdateVarToDomain(int var, const Domain& domain) {
  if (VarHasSolutionHint(var)) {
    UpdateVarSolutionHint(var, domain.ClosestValue(SolutionHint(var)));
  }

#ifdef CHECK_HINT
  if (model_has_hint_ && hint_is_loaded_ && !domain.Contains(hint_[var])) {
    LOG(FATAL) << "Hint with value " << hint_[var]
               << " infeasible when changing domain of " << var << " to "
               << domain;
  }
#endif
}

void SolutionCrush::UpdateLiteralsToFalseIfDifferent(int lit1, int lit2) {
  // Set lit1 and lit2 to false if "lit1 - lit2 == 0" is violated.
  const int sign1 = RefIsPositive(lit1) ? 1 : -1;
  const int sign2 = RefIsPositive(lit2) ? 1 : -1;
  const std::vector<std::pair<int, int64_t>> linear = {
      {PositiveRef(lit1), sign1}, {PositiveRef(lit2), -sign2}};
  const Domain domain = Domain((sign1 == 1 ? 0 : -1) - (sign2 == 1 ? 0 : -1));
  SetLiteralToValueIfLinearConstraintViolated(lit1, false, linear, domain);
  SetLiteralToValueIfLinearConstraintViolated(lit2, false, linear, domain);
}

void SolutionCrush::UpdateLiteralsWithDominance(int lit, int dominating_lit) {
  if (LiteralSolutionHintIs(lit, true) &&
      LiteralSolutionHintIs(dominating_lit, false)) {
    UpdateLiteralSolutionHint(lit, false);
    UpdateLiteralSolutionHint(dominating_lit, true);
  }
}

void SolutionCrush::UpdateRefsWithDominance(
    int ref, int64_t min_value, int64_t max_value,
    absl::Span<const std::pair<int, Domain>> dominating_refs) {
  const std::optional<int64_t> ref_hint = GetRefSolutionHint(ref);
  if (!ref_hint.has_value()) return;
  // This can happen if the solution hint is not initially feasible (in which
  // case we can't fix it).
  if (*ref_hint < min_value) return;
  // If the solution hint is already in the new domain there is nothing to do.
  if (*ref_hint <= max_value) return;
  // The quantity to subtract from the solution hint of `ref`.
  const int64_t ref_hint_delta = *ref_hint - max_value;

  UpdateRefSolutionHint(ref, *ref_hint - ref_hint_delta);
  int64_t remaining_delta = ref_hint_delta;
  for (const auto& [dominating_ref, dominating_ref_domain] : dominating_refs) {
    const std::optional<int64_t> dominating_ref_hint =
        GetRefSolutionHint(dominating_ref);
    if (!dominating_ref_hint.has_value()) continue;
    const int64_t new_dominating_ref_hint =
        dominating_ref_domain.ValueAtOrBefore(*dominating_ref_hint +
                                              remaining_delta);
    // This might happen if the solution hint is not initially feasible.
    if (!dominating_ref_domain.Contains(new_dominating_ref_hint)) continue;
    UpdateRefSolutionHint(dominating_ref, new_dominating_ref_hint);
    remaining_delta -= (new_dominating_ref_hint - *dominating_ref_hint);
    if (remaining_delta == 0) break;
  }
}

bool SolutionCrush::MaybeSetVarToAffineEquationSolution(int var_x, int var_y,
                                                        int64_t coeff,
                                                        int64_t offset) {
  if (hint_is_loaded_) {
    if (!hint_has_value_[var_y] && hint_has_value_[var_x]) {
      hint_has_value_[var_y] = true;
      hint_[var_y] = (hint_[var_x] - offset) / coeff;
      if (hint_[var_y] * coeff + offset != hint_[var_x]) {
        // TODO(user): Do we implement a rounding to closest instead of
        // routing towards 0.
        return false;
      }
    }
  }

#ifdef CHECK_HINT
  const int64_t vx = hint_[var_x];
  const int64_t vy = hint_[var_y];
  if (model_has_hint_ && hint_is_loaded_ && vx != vy * coeff + offset) {
    LOG(FATAL) << "Affine relation incompatible with hint: " << vx
               << " != " << vy << " * " << coeff << " + " << offset;
  }
#endif
  return true;
}

void SolutionCrush::SetReservoirCircuitVars(
    const ReservoirConstraintProto& reservoir, int64_t min_level,
    int64_t max_level, absl::Span<const int> level_vars,
    const CircuitConstraintProto& circuit) {
  if (!hint_is_loaded_) return;
  // The hints of the active events, in the order they should appear in the
  // circuit. The hints are collected first, and sorted later.
  struct ReservoirEventHint {
    int index;  // In the reservoir constraint.
    int64_t time;
    int64_t level_change;
  };
  const int num_events = reservoir.time_exprs_size();
  std::vector<ReservoirEventHint> active_event_hints;
  for (int i = 0; i < num_events; ++i) {
    if (!VarHasSolutionHint(PositiveRef(reservoir.active_literals(i)))) return;
    if (LiteralSolutionHint(reservoir.active_literals(i))) {
      const std::optional<int64_t> time_hint =
          GetExpressionSolutionHint(reservoir.time_exprs(i));
      const std::optional<int64_t> change_hint =
          GetExpressionSolutionHint(reservoir.level_changes(i));
      if (!time_hint.has_value() || !change_hint.has_value()) return;
      active_event_hints.push_back({i, time_hint.value(), change_hint.value()});
    }
  }

  // Update the `level_vars` hints by computing the level at each active event.
  std::sort(active_event_hints.begin(), active_event_hints.end(),
            [](const ReservoirEventHint& a, const ReservoirEventHint& b) {
              return a.time < b.time;
            });
  int64_t current_level = 0;
  for (int i = 0; i < active_event_hints.size(); ++i) {
    int j = i;
    // Adjust the order of the events occurring at the same time, in the
    // circuit, so that, at each node, the level is between `var_min` and
    // `var_max`. For instance, if e1 = {t, +1} and e2 = {t, -1}, and if
    // `current_level` = 0, `var_min` = -1 and `var_max` = 0, then e2 must occur
    // before e1.
    while (j < active_event_hints.size() &&
           active_event_hints[j].time == active_event_hints[i].time &&
           (current_level + active_event_hints[j].level_change < min_level ||
            current_level + active_event_hints[j].level_change > max_level)) {
      ++j;
    }
    if (j < active_event_hints.size() &&
        active_event_hints[j].time == active_event_hints[i].time) {
      if (i != j) std::swap(active_event_hints[i], active_event_hints[j]);
      current_level += active_event_hints[i].level_change;
      SetVarHint(level_vars[active_event_hints[i].index], current_level);
    } else {
      return;
    }
  }

  // The index of each event in `active_event_hints`, or -1 if the event's
  // "active" hint is false.
  std::vector<int> active_event_hint_index(num_events, -1);
  for (int i = 0; i < active_event_hints.size(); ++i) {
    active_event_hint_index[active_event_hints[i].index] = i;
  }
  for (int i = 0; i < circuit.literals_size(); ++i) {
    const int head = circuit.heads(i);
    const int tail = circuit.tails(i);
    const int literal = circuit.literals(i);
    if (tail == num_events) {
      if (head == num_events) {
        // Self-arc on the start and end node.
        SetLiteralHint(literal, active_event_hints.empty());
      } else {
        // Arc from the start node to an event node.
        SetLiteralHint(literal, !active_event_hints.empty() &&
                                    active_event_hints.front().index == head);
      }
    } else if (head == num_events) {
      // Arc from an event node to the end node.
      SetLiteralHint(literal, !active_event_hints.empty() &&
                                  active_event_hints.back().index == tail);
    } else if (tail != head) {
      // Arc between two different event nodes.
      const int tail_index = active_event_hint_index[tail];
      const int head_index = active_event_hint_index[head];
      SetLiteralHint(literal, tail_index != -1 && tail_index != -1 &&
                                  head_index == tail_index + 1);
    }
  }
}

void SolutionCrush::SetVarToReifiedPrecedenceLiteral(
    int var, const LinearExpressionProto& time_i,
    const LinearExpressionProto& time_j, int active_i, int active_j) {
  // Take care of hints.
  if (hint_is_loaded_) {
    std::optional<int64_t> time_i_hint = GetExpressionSolutionHint(time_i);
    std::optional<int64_t> time_j_hint = GetExpressionSolutionHint(time_j);
    std::optional<int64_t> active_i_hint = GetRefSolutionHint(active_i);
    std::optional<int64_t> active_j_hint = GetRefSolutionHint(active_j);
    if (time_i_hint.has_value() && time_j_hint.has_value() &&
        active_i_hint.has_value() && active_j_hint.has_value()) {
      const bool reified_hint = (active_i_hint.value() != 0) &&
                                (active_j_hint.value() != 0) &&
                                (time_i_hint.value() <= time_j_hint.value());
      SetNewVariableHint(var, reified_hint);
    }
  }
}

void SolutionCrush::SetIntModExpandedVars(const ConstraintProto& ct,
                                          int div_var, int prod_var,
                                          int64_t default_div_value,
                                          int64_t default_prod_value) {
  if (!hint_is_loaded_) return;
  bool enforced_hint = true;
  for (const int lit : ct.enforcement_literal()) {
    if (!VarHasSolutionHint(PositiveRef(lit))) return;
    enforced_hint = enforced_hint && LiteralSolutionHint(lit);
  }
  if (!enforced_hint) {
    SetVarHint(div_var, default_div_value);
    SetVarHint(prod_var, default_prod_value);
    return;
  }
  const LinearArgumentProto& int_mod = ct.int_mod();
  std::optional<int64_t> hint = GetExpressionSolutionHint(int_mod.exprs(0));
  if (!hint.has_value()) return;
  const int64_t expr_hint = hint.value();

  hint = GetExpressionSolutionHint(int_mod.exprs(1));
  if (!hint.has_value()) return;
  const int64_t mod_expr_hint = hint.value();

  hint = GetExpressionSolutionHint(int_mod.target());
  if (!hint.has_value()) return;
  const int64_t target_expr_hint = hint.value();

  // target_expr_hint should be equal to "expr_hint % mod_expr_hint".
  SetVarHint(div_var, expr_hint / mod_expr_hint);
  SetVarHint(prod_var, expr_hint - target_expr_hint);
}

void SolutionCrush::SetIntProdExpandedVars(const LinearArgumentProto& int_prod,
                                           absl::Span<const int> prod_vars) {
  DCHECK_EQ(int_prod.exprs_size(), prod_vars.size() + 2);
  if (!hint_is_loaded_) return;
  std::optional<int64_t> hint = GetExpressionSolutionHint(int_prod.exprs(0));
  if (!hint.has_value()) return;
  int64_t last_prod_hint = hint.value();
  for (int i = 1; i < int_prod.exprs_size() - 1; ++i) {
    hint = GetExpressionSolutionHint(int_prod.exprs(i));
    if (!hint.has_value()) return;
    last_prod_hint *= hint.value();
    SetVarHint(prod_vars[i - 1], last_prod_hint);
  }
}

void SolutionCrush::PermuteVariables(const SparsePermutation& permutation) {
  CHECK(hint_is_loaded_);
  permutation.ApplyToDenseCollection(hint_);
  permutation.ApplyToDenseCollection(hint_has_value_);
}

}  // namespace sat
}  // namespace operations_research
