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
#include <memory>
#include <optional>
#ifdef CHECK_CRUSH
#include <sstream>
#include <string>
#endif
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/types/span.h"
#include "ortools/algorithms/sparse_permutation.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/symmetry_util.h"
#include "ortools/sat/util.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

void SolutionCrush::LoadSolution(
    int num_vars, const absl::flat_hash_map<int, int64_t>& solution) {
  CHECK(!solution_is_loaded_);
  CHECK(var_has_value_.empty());
  CHECK(var_values_.empty());
  solution_is_loaded_ = true;
  var_has_value_.resize(num_vars, false);
  var_values_.resize(num_vars, 0);
  for (const auto [var, value] : solution) {
    var_has_value_[var] = true;
    var_values_[var] = value;
  }
}

void SolutionCrush::Resize(int new_size) {
  if (!solution_is_loaded_) return;
  var_has_value_.resize(new_size, false);
  var_values_.resize(new_size, 0);
}

void SolutionCrush::MaybeSetLiteralToValueEncoding(int literal, int var,
                                                   int64_t value) {
  DCHECK(RefIsPositive(var));
  if (!solution_is_loaded_) return;
  if (!HasValue(PositiveRef(literal)) && HasValue(var)) {
    SetLiteralValue(literal, GetVarValue(var) == value);
  }
}

void SolutionCrush::SetVarToLinearExpression(
    int new_var, absl::Span<const std::pair<int, int64_t>> linear,
    int64_t offset) {
  if (!solution_is_loaded_) return;
  int64_t new_value = offset;
  for (const auto [var, coeff] : linear) {
    if (!HasValue(var)) return;
    new_value += coeff * GetVarValue(var);
  }
  SetVarValue(new_var, new_value);
}

void SolutionCrush::SetVarToLinearExpression(int new_var,
                                             absl::Span<const int> vars,
                                             absl::Span<const int64_t> coeffs,
                                             int64_t offset) {
  DCHECK_EQ(vars.size(), coeffs.size());
  if (!solution_is_loaded_) return;
  int64_t new_value = offset;
  for (int i = 0; i < vars.size(); ++i) {
    const int var = vars[i];
    const int64_t coeff = coeffs[i];
    if (!HasValue(var)) return;
    new_value += coeff * GetVarValue(var);
  }
  SetVarValue(new_var, new_value);
}

void SolutionCrush::SetVarToClause(int new_var, absl::Span<const int> clause) {
  if (!solution_is_loaded_) return;
  int new_value = 0;
  bool all_have_value = true;
  for (const int literal : clause) {
    const int var = PositiveRef(literal);
    if (!HasValue(var)) {
      all_have_value = false;
      break;
    }
    if (GetVarValue(var) == (RefIsPositive(literal) ? 1 : 0)) {
      new_value = 1;
      break;
    }
  }
  // Leave the `new_var` unassigned if any literal is unassigned.
  if (all_have_value) {
    SetVarValue(new_var, new_value);
  }
}

void SolutionCrush::SetVarToConjunction(int new_var,
                                        absl::Span<const int> conjunction) {
  if (!solution_is_loaded_) return;
  int new_value = 1;
  bool all_have_value = true;
  for (const int literal : conjunction) {
    const int var = PositiveRef(literal);
    if (!HasValue(var)) {
      all_have_value = false;
      break;
    }
    if (GetVarValue(var) == (RefIsPositive(literal) ? 0 : 1)) {
      new_value = 0;
      break;
    }
  }
  // Leave the `new_var` unassigned if any literal is unassigned.
  if (all_have_value) {
    SetVarValue(new_var, new_value);
  }
}

void SolutionCrush::SetVarToValueIfLinearConstraintViolated(
    int new_var, int64_t value,
    absl::Span<const std::pair<int, int64_t>> linear, const Domain& domain) {
  if (!solution_is_loaded_) return;
  int64_t linear_value = 0;
  bool all_have_value = true;
  for (const auto [var, coeff] : linear) {
    if (!HasValue(var)) {
      all_have_value = false;
      break;
    }
    linear_value += GetVarValue(var) * coeff;
  }
  if (all_have_value && !domain.Contains(linear_value)) {
    SetVarValue(new_var, value);
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

void SolutionCrush::SetVarToLinearExpressionIf(
    int var, const LinearExpressionProto& expr, int condition_lit) {
  if (!solution_is_loaded_) return;
  if (!HasValue(PositiveRef(condition_lit))) return;
  if (!GetLiteralValue(condition_lit)) return;
  const std::optional<int64_t> expr_value = GetExpressionValue(expr);
  if (expr_value.has_value()) {
    SetVarValue(var, expr_value.value());
  }
}

void SolutionCrush::SetLiteralToValueIf(int literal, bool value,
                                        int condition_lit) {
  SetLiteralToValueIfLinearConstraintViolated(
      literal, value, {{PositiveRef(condition_lit), 1}},
      Domain(RefIsPositive(condition_lit) ? 0 : 1));
}

void SolutionCrush::SetVarToConditionalValue(
    int var, absl::Span<const int> condition_lits, int64_t value_if_true,
    int64_t value_if_false) {
  if (!solution_is_loaded_) return;
  bool condition_value = true;
  for (const int condition_lit : condition_lits) {
    if (!HasValue(PositiveRef(condition_lit))) return;
    if (!GetLiteralValue(condition_lit)) {
      condition_value = false;
      break;
    }
  }
  SetVarValue(var, condition_value ? value_if_true : value_if_false);
}

void SolutionCrush::MakeLiteralsEqual(int lit1, int lit2) {
  if (!solution_is_loaded_) return;
  if (HasValue(PositiveRef(lit2))) {
    SetLiteralValue(lit1, GetLiteralValue(lit2));
  } else if (HasValue(PositiveRef(lit1))) {
    SetLiteralValue(lit2, GetLiteralValue(lit1));
  }
}

void SolutionCrush::SetOrUpdateVarToDomain(int var, const Domain& domain) {
  if (!solution_is_loaded_) return;
  if (HasValue(var)) {
    SetVarValue(var, domain.ClosestValue(GetVarValue(var)));
  } else if (domain.IsFixed()) {
    SetVarValue(var, domain.FixedValue());
  }
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
  if (!solution_is_loaded_) return;
  if (!HasValue(PositiveRef(lit)) || !HasValue(PositiveRef(dominating_lit))) {
    return;
  }
  if (GetLiteralValue(lit) && !GetLiteralValue(dominating_lit)) {
    SetLiteralValue(lit, false);
    SetLiteralValue(dominating_lit, true);
  }
}

void SolutionCrush::MaybeUpdateVarWithSymmetriesToValue(
    int var, bool value,
    absl::Span<const std::unique_ptr<SparsePermutation>> generators) {
  if (!solution_is_loaded_) return;
  if (!HasValue(var)) return;
  if (GetVarValue(var) == static_cast<int64_t>(value)) return;

  std::vector<int> schrier_vector;
  std::vector<int> orbit;
  GetSchreierVectorAndOrbit(var, generators, &schrier_vector, &orbit);

  bool found_target = false;
  int target_var;
  for (int v : orbit) {
    if (HasValue(v) && GetVarValue(v) == static_cast<int64_t>(value)) {
      found_target = true;
      target_var = v;
      break;
    }
  }
  if (!found_target) {
    VLOG(1) << "Couldn't transform solution properly";
    return;
  }

  const std::vector<int> generator_idx =
      TracePoint(target_var, schrier_vector, generators);
  for (const int i : generator_idx) {
    PermuteVariables(*generators[i]);
  }

  DCHECK(HasValue(var));
  DCHECK_EQ(GetVarValue(var), value);
}

void SolutionCrush::MaybeSwapOrbitopeColumns(
    absl::Span<const std::vector<int>> orbitope, int row, int pivot_col,
    bool value) {
  if (!solution_is_loaded_) return;
  int col = -1;
  for (int c = 0; c < orbitope[row].size(); ++c) {
    if (GetLiteralValue(orbitope[row][c]) == value) {
      if (col != -1) {
        VLOG(2) << "Multiple literals in row with given value";
        return;
      }
      col = c;
    }
  }
  if (col < pivot_col) {
    // Nothing to do.
    return;
  }
  // Swap the value of the literals in column `col` with the value of the ones
  // in column `pivot_col`, if they all have a value.
  for (int i = 0; i < orbitope.size(); ++i) {
    if (!HasValue(PositiveRef(orbitope[i][col]))) return;
    if (!HasValue(PositiveRef(orbitope[i][pivot_col]))) return;
  }
  for (int i = 0; i < orbitope.size(); ++i) {
    const int src_lit = orbitope[i][col];
    const int dst_lit = orbitope[i][pivot_col];
    const bool src_value = GetLiteralValue(src_lit);
    const bool dst_value = GetLiteralValue(dst_lit);
    SetLiteralValue(src_lit, dst_value);
    SetLiteralValue(dst_lit, src_value);
  }
}

void SolutionCrush::UpdateRefsWithDominance(
    int ref, int64_t min_value, int64_t max_value,
    absl::Span<const std::pair<int, Domain>> dominating_refs) {
  if (!solution_is_loaded_) return;
  const std::optional<int64_t> ref_value = GetRefValue(ref);
  if (!ref_value.has_value()) return;
  // This can happen if the solution is not initially feasible (in which
  // case we can't fix it).
  if (*ref_value < min_value) return;
  // If the value is already in the new domain there is nothing to do.
  if (*ref_value <= max_value) return;
  // The quantity to subtract from the value of `ref`.
  const int64_t ref_value_delta = *ref_value - max_value;

  SetRefValue(ref, *ref_value - ref_value_delta);
  int64_t remaining_delta = ref_value_delta;
  for (const auto& [dominating_ref, dominating_ref_domain] : dominating_refs) {
    const std::optional<int64_t> dominating_ref_value =
        GetRefValue(dominating_ref);
    if (!dominating_ref_value.has_value()) continue;
    const int64_t new_dominating_ref_value =
        dominating_ref_domain.ValueAtOrBefore(*dominating_ref_value +
                                              remaining_delta);
    // This might happen if the solution is not initially feasible.
    if (!dominating_ref_domain.Contains(new_dominating_ref_value)) continue;
    SetRefValue(dominating_ref, new_dominating_ref_value);
    remaining_delta -= (new_dominating_ref_value - *dominating_ref_value);
    if (remaining_delta == 0) break;
  }
}

void SolutionCrush::SetVarToLinearConstraintSolution(
    std::optional<int> var_index, absl::Span<const int> vars,
    absl::Span<const int64_t> coeffs, int64_t rhs) {
  DCHECK_EQ(vars.size(), coeffs.size());
  DCHECK(!var_index.has_value() || var_index.value() < vars.size());
  if (!solution_is_loaded_) return;
  int64_t term_value = rhs;
  for (int i = 0; i < vars.size(); ++i) {
    if (HasValue(vars[i])) {
      if (i != var_index) {
        term_value -= GetVarValue(vars[i]) * coeffs[i];
      }
    } else if (!var_index.has_value()) {
      var_index = i;
    } else if (var_index.value() != i) {
      return;
    }
  }
  if (!var_index.has_value()) return;
  SetVarValue(vars[var_index.value()], term_value / coeffs[var_index.value()]);

#ifdef CHECK_CRUSH
  if (term_value % coeffs[var_index.value()] != 0) {
    std::stringstream lhs;
    for (int i = 0; i < vars.size(); ++i) {
      lhs << (i == var_index ? "x" : std::to_string(GetVarValue(vars[i])));
      lhs << " * " << coeffs[i];
      if (i < vars.size() - 1) lhs << " + ";
    }
    LOG(FATAL) << "Linear constraint incompatible with solution: " << lhs
               << " != " << rhs;
  }
#endif
}

void SolutionCrush::SetReservoirCircuitVars(
    const ReservoirConstraintProto& reservoir, int64_t min_level,
    int64_t max_level, absl::Span<const int> level_vars,
    const CircuitConstraintProto& circuit) {
  if (!solution_is_loaded_) return;
  // The values of the active events, in the order they should appear in the
  // circuit. The values are collected first, and sorted later.
  struct ReservoirEventValues {
    int index;  // In the reservoir constraint.
    int64_t time;
    int64_t level_change;
  };
  const int num_events = reservoir.time_exprs_size();
  std::vector<ReservoirEventValues> active_event_values;
  for (int i = 0; i < num_events; ++i) {
    if (!HasValue(PositiveRef(reservoir.active_literals(i)))) return;
    if (GetLiteralValue(reservoir.active_literals(i))) {
      const std::optional<int64_t> time_value =
          GetExpressionValue(reservoir.time_exprs(i));
      const std::optional<int64_t> change_value =
          GetExpressionValue(reservoir.level_changes(i));
      if (!time_value.has_value() || !change_value.has_value()) return;
      active_event_values.push_back(
          {i, time_value.value(), change_value.value()});
    }
  }

  // Update the `level_vars` values by computing the level at each active event.
  std::sort(active_event_values.begin(), active_event_values.end(),
            [](const ReservoirEventValues& a, const ReservoirEventValues& b) {
              return a.time < b.time;
            });
  int64_t current_level = 0;
  for (int i = 0; i < active_event_values.size(); ++i) {
    int j = i;
    // Adjust the order of the events occurring at the same time, in the
    // circuit, so that, at each node, the level is between `var_min` and
    // `var_max`. For instance, if e1 = {t, +1} and e2 = {t, -1}, and if
    // `current_level` = 0, `var_min` = -1 and `var_max` = 0, then e2 must occur
    // before e1.
    while (j < active_event_values.size() &&
           active_event_values[j].time == active_event_values[i].time &&
           (current_level + active_event_values[j].level_change < min_level ||
            current_level + active_event_values[j].level_change > max_level)) {
      ++j;
    }
    if (j < active_event_values.size() &&
        active_event_values[j].time == active_event_values[i].time) {
      if (i != j) std::swap(active_event_values[i], active_event_values[j]);
      current_level += active_event_values[i].level_change;
      SetVarValue(level_vars[active_event_values[i].index], current_level);
    } else {
      return;
    }
  }

  // The index of each event in `active_event_values`, or -1 if the event's
  // "active" value is false.
  std::vector<int> active_event_value_index(num_events, -1);
  for (int i = 0; i < active_event_values.size(); ++i) {
    active_event_value_index[active_event_values[i].index] = i;
  }
  // Set the level vars of inactive events to an arbitrary value.
  for (int i = 0; i < num_events; ++i) {
    if (active_event_value_index[i] == -1) {
      SetVarValue(level_vars[i], min_level);
    }
  }

  for (int i = 0; i < circuit.literals_size(); ++i) {
    const int head = circuit.heads(i);
    const int tail = circuit.tails(i);
    const int literal = circuit.literals(i);
    if (tail == num_events) {
      if (head == num_events) {
        // Self-arc on the start and end node.
        SetLiteralValue(literal, active_event_values.empty());
      } else {
        // Arc from the start node to an event node.
        SetLiteralValue(literal, !active_event_values.empty() &&
                                     active_event_values.front().index == head);
      }
    } else if (head == num_events) {
      // Arc from an event node to the end node.
      SetLiteralValue(literal, !active_event_values.empty() &&
                                   active_event_values.back().index == tail);
    } else if (tail != head) {
      // Arc between two different event nodes.
      const int tail_index = active_event_value_index[tail];
      const int head_index = active_event_value_index[head];
      SetLiteralValue(literal, tail_index != -1 && tail_index != -1 &&
                                   head_index == tail_index + 1);
    }
  }
}

void SolutionCrush::SetVarToReifiedPrecedenceLiteral(
    int var, const LinearExpressionProto& time_i,
    const LinearExpressionProto& time_j, int active_i, int active_j) {
  if (!solution_is_loaded_) return;
  std::optional<int64_t> time_i_value = GetExpressionValue(time_i);
  std::optional<int64_t> time_j_value = GetExpressionValue(time_j);
  std::optional<int64_t> active_i_value = GetRefValue(active_i);
  std::optional<int64_t> active_j_value = GetRefValue(active_j);
  if (time_i_value.has_value() && time_j_value.has_value() &&
      active_i_value.has_value() && active_j_value.has_value()) {
    const bool reified_value = (active_i_value.value() != 0) &&
                               (active_j_value.value() != 0) &&
                               (time_i_value.value() <= time_j_value.value());
    SetVarValue(var, reified_value);
  }
}

void SolutionCrush::SetIntModExpandedVars(const ConstraintProto& ct,
                                          int div_var, int prod_var,
                                          int64_t default_div_value,
                                          int64_t default_prod_value) {
  if (!solution_is_loaded_) return;
  bool enforced_value = true;
  for (const int lit : ct.enforcement_literal()) {
    if (!HasValue(PositiveRef(lit))) return;
    enforced_value = enforced_value && GetLiteralValue(lit);
  }
  if (!enforced_value) {
    SetVarValue(div_var, default_div_value);
    SetVarValue(prod_var, default_prod_value);
    return;
  }
  const LinearArgumentProto& int_mod = ct.int_mod();
  std::optional<int64_t> v = GetExpressionValue(int_mod.exprs(0));
  if (!v.has_value()) return;
  const int64_t expr_value = v.value();

  v = GetExpressionValue(int_mod.exprs(1));
  if (!v.has_value()) return;
  const int64_t mod_expr_value = v.value();

  v = GetExpressionValue(int_mod.target());
  if (!v.has_value()) return;
  const int64_t target_expr_value = v.value();

  // target_expr_value should be equal to "expr_value % mod_expr_value".
  SetVarValue(div_var, expr_value / mod_expr_value);
  SetVarValue(prod_var, expr_value - target_expr_value);
}

void SolutionCrush::SetIntProdExpandedVars(const LinearArgumentProto& int_prod,
                                           absl::Span<const int> prod_vars) {
  DCHECK_EQ(int_prod.exprs_size(), prod_vars.size() + 2);
  if (!solution_is_loaded_) return;
  std::optional<int64_t> v = GetExpressionValue(int_prod.exprs(0));
  if (!v.has_value()) return;
  int64_t last_prod_value = v.value();
  for (int i = 1; i < int_prod.exprs_size() - 1; ++i) {
    v = GetExpressionValue(int_prod.exprs(i));
    if (!v.has_value()) return;
    last_prod_value *= v.value();
    SetVarValue(prod_vars[i - 1], last_prod_value);
  }
}

void SolutionCrush::SetLinMaxExpandedVars(
    const LinearArgumentProto& lin_max,
    absl::Span<const int> enforcement_lits) {
  if (!solution_is_loaded_) return;
  DCHECK_EQ(enforcement_lits.size(), lin_max.exprs_size());
  const std::optional<int64_t> target_value =
      GetExpressionValue(lin_max.target());
  if (!target_value.has_value()) return;
  int enforcement_value_sum = 0;
  for (int i = 0; i < enforcement_lits.size(); ++i) {
    const std::optional<int64_t> expr_value =
        GetExpressionValue(lin_max.exprs(i));
    if (!expr_value.has_value()) return;
    if (enforcement_value_sum == 0) {
      const bool enforcement_value = target_value.value() <= expr_value.value();
      SetLiteralValue(enforcement_lits[i], enforcement_value);
      enforcement_value_sum += enforcement_value;
    } else {
      SetLiteralValue(enforcement_lits[i], false);
    }
  }
}

void SolutionCrush::SetAutomatonExpandedVars(
    const AutomatonConstraintProto& automaton,
    absl::Span<const StateVar> state_vars,
    absl::Span<const TransitionVar> transition_vars) {
  if (!solution_is_loaded_) return;
  absl::flat_hash_map<std::pair<int64_t, int64_t>, int64_t> transitions;
  for (int i = 0; i < automaton.transition_tail_size(); ++i) {
    transitions[{automaton.transition_tail(i), automaton.transition_label(i)}] =
        automaton.transition_head(i);
  }

  std::vector<int64_t> label_values;
  std::vector<int64_t> state_values;
  int64_t current_state = automaton.starting_state();
  state_values.push_back(current_state);
  for (int i = 0; i < automaton.exprs_size(); ++i) {
    const std::optional<int64_t> label_value =
        GetExpressionValue(automaton.exprs(i));
    if (!label_value.has_value()) return;
    label_values.push_back(label_value.value());

    const auto it = transitions.find({current_state, label_value.value()});
    if (it == transitions.end()) return;
    current_state = it->second;
    state_values.push_back(current_state);
  }

  for (const auto& [var, time, state] : state_vars) {
    SetVarValue(var, state_values[time] == state);
  }
  for (const auto& [var, time, transition_tail, transition_label] :
       transition_vars) {
    SetVarValue(var, state_values[time] == transition_tail &&
                         label_values[time] == transition_label);
  }
}

void SolutionCrush::SetTableExpandedVars(
    absl::Span<const int> column_vars, absl::Span<const int> existing_row_lits,
    absl::Span<const TableRowLiteral> new_row_lits) {
  if (!solution_is_loaded_) return;
  int row_lit_values_sum = 0;
  for (const int lit : existing_row_lits) {
    if (!HasValue(PositiveRef(lit))) return;
    row_lit_values_sum += GetLiteralValue(lit);
  }
  const int num_vars = column_vars.size();
  for (const auto& [lit, var_values] : new_row_lits) {
    if (row_lit_values_sum >= 1) {
      SetLiteralValue(lit, false);
      continue;
    }
    bool row_lit_value = true;
    for (int var_index = 0; var_index < num_vars; ++var_index) {
      const auto& values = var_values[var_index];
      if (!values.empty() &&
          std::find(values.begin(), values.end(),
                    GetVarValue(column_vars[var_index])) == values.end()) {
        row_lit_value = false;
        break;
      }
    }
    SetLiteralValue(lit, row_lit_value);
    row_lit_values_sum += row_lit_value;
  }
}

void SolutionCrush::SetLinearWithComplexDomainExpandedVars(
    const LinearConstraintProto& linear, absl::Span<const int> bucket_lits) {
  if (!solution_is_loaded_) return;
  int64_t expr_value = 0;
  for (int i = 0; i < linear.vars_size(); ++i) {
    const int var = linear.vars(i);
    if (!HasValue(var)) return;
    expr_value += linear.coeffs(i) * GetVarValue(var);
  }
  DCHECK_LE(bucket_lits.size(), linear.domain_size() / 2);
  for (int i = 0; i < bucket_lits.size(); ++i) {
    const int64_t lb = linear.domain(2 * i);
    const int64_t ub = linear.domain(2 * i + 1);
    SetLiteralValue(bucket_lits[i], expr_value >= lb && expr_value <= ub);
  }
}

void SolutionCrush::StoreSolutionAsHint(CpModelProto& model) const {
  if (!solution_is_loaded_) return;
  model.clear_solution_hint();
  for (int i = 0; i < var_values_.size(); ++i) {
    if (var_has_value_[i]) {
      model.mutable_solution_hint()->add_vars(i);
      model.mutable_solution_hint()->add_values(var_values_[i]);
    }
  }
}

void SolutionCrush::PermuteVariables(const SparsePermutation& permutation) {
  CHECK(solution_is_loaded_);
  permutation.ApplyToDenseCollection(var_has_value_);
  permutation.ApplyToDenseCollection(var_values_);
}

void SolutionCrush::AssignVariableToPackingArea(
    const CompactVectorVector<int, Rectangle>& areas, const CpModelProto& model,
    absl::Span<const int> x_intervals, absl::Span<const int> y_intervals,
    absl::Span<const BoxInAreaLiteral> box_in_area_lits) {
  if (!solution_is_loaded_) return;
  struct RectangleTypeAndIndex {
    enum class Type {
      kHintedBox,
      kArea,
    };

    int index;
    Type type;
  };
  std::vector<Rectangle> rectangles_for_intersections;
  std::vector<RectangleTypeAndIndex> rectangles_index;

  for (int i = 0; i < x_intervals.size(); ++i) {
    const ConstraintProto& x_ct = model.constraints(x_intervals[i]);
    const ConstraintProto& y_ct = model.constraints(y_intervals[i]);

    const std::optional<int64_t> x_min =
        GetExpressionValue(x_ct.interval().start());
    const std::optional<int64_t> x_max =
        GetExpressionValue(x_ct.interval().end());
    const std::optional<int64_t> y_min =
        GetExpressionValue(y_ct.interval().start());
    const std::optional<int64_t> y_max =
        GetExpressionValue(y_ct.interval().end());

    if (!x_min.has_value() || !x_max.has_value() || !y_min.has_value() ||
        !y_max.has_value()) {
      return;
    }
    if (*x_min > *x_max || *y_min > *y_max) {
      VLOG(2) << "Hinted no_overlap_2d coordinate has max lower than min";
      return;
    }
    const Rectangle box = {.x_min = x_min.value(),
                           .x_max = x_max.value(),
                           .y_min = y_min.value(),
                           .y_max = y_max.value()};
    rectangles_for_intersections.push_back(box);
    rectangles_index.push_back(
        {.index = i, .type = RectangleTypeAndIndex::Type::kHintedBox});
  }

  for (int i = 0; i < areas.size(); ++i) {
    for (const Rectangle& area : areas[i]) {
      rectangles_for_intersections.push_back(area);
      rectangles_index.push_back(
          {.index = i, .type = RectangleTypeAndIndex::Type::kArea});
    }
  }

  const std::vector<std::pair<int, int>> intersections =
      FindPartialRectangleIntersections(rectangles_for_intersections);

  absl::flat_hash_set<std::pair<int, int>> box_to_area_pairs;

  for (const auto& [rec1_index, rec2_index] : intersections) {
    RectangleTypeAndIndex rec1 = rectangles_index[rec1_index];
    RectangleTypeAndIndex rec2 = rectangles_index[rec2_index];
    if (rec1.type == rec2.type) {
      DCHECK(rec1.type == RectangleTypeAndIndex::Type::kHintedBox);
      VLOG(2) << "Hinted position of boxes in no_overlap_2d are overlapping";
      return;
    }
    if (rec1.type != RectangleTypeAndIndex::Type::kHintedBox) {
      std::swap(rec1, rec2);
    }

    box_to_area_pairs.insert({rec1.index, rec2.index});
  }

  for (const auto& [box_index, area_index, literal] : box_in_area_lits) {
    SetLiteralValue(literal,
                    box_to_area_pairs.contains({box_index, area_index}));
  }
}

}  // namespace sat
}  // namespace operations_research
