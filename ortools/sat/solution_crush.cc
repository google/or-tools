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
#include <cstdlib>
#include <memory>
#include <optional>
#ifdef CHECK_CRUSH
#include <sstream>
#include <string>
#endif
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/btree_map.h"
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

void SolutionCrush::LoadSolution(absl::Span<const int64_t> solution) {
  CHECK(!solution_is_loaded_);
  CHECK(var_has_value_.empty());
  CHECK(var_values_.empty());
  solution_is_loaded_ = true;
  var_has_value_.assign(solution.size(), true);
  var_values_.assign(solution.begin(), solution.end());
}

void SolutionCrush::Resize(int new_size) {
  if (proto_ != nullptr) {
    proto_->add_steps()->mutable_resize()->set_new_size(new_size);
  }
  if (!solution_is_loaded_) return;
  var_has_value_.resize(new_size, false);
  var_values_.resize(new_size, 0);
}

void SolutionCrush::MaybeSetLiteralToValueEncoding(int literal, int var,
                                                   int64_t value) {
  MaybeSetLiteralToOrderEncoding(literal, var, value, Relation::EQ);
}

void SolutionCrush::MaybeSetLiteralToOrderEncoding(int literal, int var,
                                                   int64_t value,
                                                   Relation relation) {
  DCHECK(RefIsPositive(var));
  if (proto_ != nullptr) {
    auto* step =
        proto_->add_steps()->mutable_maybe_set_literal_to_order_encoding();
    step->set_literal(literal);
    step->set_var(var);
    step->set_value(value);
    step->set_relation(
        relation == Relation::LE
            ? MaybeSetLiteralToOrderEncoding::LESS_THAN_OR_EQUAL
        : relation == Relation::EQ
            ? MaybeSetLiteralToOrderEncoding::EQUAL
            : MaybeSetLiteralToOrderEncoding::GREATER_THAN_OR_EQUAL);
  }
  if (!solution_is_loaded_) return;
  if (!HasValue(PositiveRef(literal)) && HasValue(var)) {
    if (relation == Relation::LE) {
      SetLiteralValue(literal, GetVarValue(var) <= value);
    } else if (relation == Relation::EQ) {
      SetLiteralValue(literal, GetVarValue(var) == value);
    } else {
      SetLiteralValue(literal, GetVarValue(var) >= value);
    }
  }
}

void SolutionCrush::SetVarToLinearExpression(
    int new_var, absl::Span<const std::pair<int, int64_t>> linear,
    int64_t offset) {
  if (proto_ != nullptr) {
    auto* step = proto_->add_steps()->mutable_set_var_to_linear_expression_if();
    step->set_var(new_var);
    LinearExpressionProto* expr = step->mutable_expr();
    for (const auto [var, coeff] : linear) {
      expr->add_vars(var);
      expr->add_coeffs(coeff);
    }
    expr->set_offset(offset);
  }
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
  if (proto_ != nullptr) {
    auto* step = proto_->add_steps()->mutable_set_var_to_linear_expression_if();
    step->set_var(new_var);
    LinearExpressionProto* expr = step->mutable_expr();
    for (int i = 0; i < vars.size(); ++i) {
      expr->add_vars(vars[i]);
      expr->add_coeffs(coeffs[i]);
    }
    expr->set_offset(offset);
  }
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
  if (proto_ != nullptr) {
    auto* step = proto_->add_steps()->mutable_set_var_to_clause();
    step->set_var(new_var);
    for (const int literal : clause) {
      step->add_clause(literal);
    }
  }
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
  if (proto_ != nullptr) {
    auto* step = proto_->add_steps()->mutable_set_var_to_conjunction();
    step->set_var(new_var);
    for (const int literal : conjunction) {
      step->add_conjunction(literal);
    }
  }
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
  if (proto_ != nullptr) {
    auto* step = proto_->add_steps()
                     ->mutable_set_var_to_value_if_linear_constraint_violated();
    step->set_var(new_var);
    step->set_value(value);
    LinearConstraintProto* constraint = step->mutable_linear_constraint();
    for (const auto [var, coeff] : linear) {
      constraint->add_vars(var);
      constraint->add_coeffs(coeff);
    }
    FillDomainInProto(domain, constraint);
  }
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
    int var, const LinearExpressionProto& expr,
    absl::Span<const int> condition_lits) {
  if (proto_ != nullptr) {
    auto* step = proto_->add_steps()->mutable_set_var_to_linear_expression_if();
    step->set_var(var);
    *step->mutable_expr() = expr;
    step->mutable_condition_lits()->Add(condition_lits.begin(),
                                        condition_lits.end());
  }
  if (!solution_is_loaded_) return;
  for (const int condition_lit : condition_lits) {
    if (!HasValue(PositiveRef(condition_lit))) return;
    if (!GetLiteralValue(condition_lit)) return;
  }
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
  if (proto_ != nullptr) {
    auto* step = proto_->add_steps()->mutable_set_var_to_conditional_value();
    step->set_var(var);
    for (const int condition_lit : condition_lits) {
      step->add_condition_lits(condition_lit);
    }
    step->set_value_if_true(value_if_true);
    step->set_value_if_false(value_if_false);
  }
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
  if (proto_ != nullptr) {
    auto* step = proto_->add_steps()->mutable_make_literals_equal();
    step->set_lit1(lit1);
    step->set_lit2(lit2);
  }
  if (!solution_is_loaded_) return;
  if (HasValue(PositiveRef(lit2))) {
    SetLiteralValue(lit1, GetLiteralValue(lit2));
  } else if (HasValue(PositiveRef(lit1))) {
    SetLiteralValue(lit2, GetLiteralValue(lit1));
  }
}

void SolutionCrush::SetOrUpdateVarToDomain(int var, const Domain& domain) {
  if (proto_ != nullptr) {
    auto* step = proto_->add_steps()->mutable_set_or_update_var_to_domain();
    step->set_var(var);
    FillDomainInProto(domain, step);
  }
  if (!solution_is_loaded_) return;
  if (HasValue(var)) {
    SetVarValue(var, domain.ClosestValue(GetVarValue(var)));
  } else if (domain.IsFixed()) {
    SetVarValue(var, domain.FixedValue());
  }
}

void SolutionCrush::SetOrUpdateVarToDomainWithOptionalEscapeValue(
    int var, const Domain& reduced_var_domain,
    std::optional<int64_t> unique_escape_value,
    bool push_down_when_not_in_domain,
    const absl::btree_map<int64_t, int>& encoding) {
  if (proto_ != nullptr) {
    auto* step =
        proto_->add_steps()
            ->mutable_set_or_update_var_to_domain_with_optional_escape_value();
    step->set_var(var);
    FillDomainInProto(reduced_var_domain, step);
    if (unique_escape_value.has_value()) {
      step->set_unique_escape_value(unique_escape_value.value());
    }
    step->set_push_down_when_not_in_domain(push_down_when_not_in_domain);
    for (const auto [value, literal] : encoding) {
      step->mutable_encoding()->insert({value, literal});
    }
  }
  if (!solution_is_loaded_) return;
  if (HasValue(var)) {
    const int64_t old_value = GetVarValue(var);
    int64_t new_value = old_value;
    if (reduced_var_domain.Contains(old_value)) return;
    if (unique_escape_value.has_value()) {
      new_value = unique_escape_value.value();
    } else if (push_down_when_not_in_domain) {
      if (old_value < reduced_var_domain.Min()) {
        new_value = reduced_var_domain.Min();
      } else {
        new_value = reduced_var_domain.ValueAtOrBefore(old_value);
      }
    } else {
      if (old_value > reduced_var_domain.Max()) {
        new_value = reduced_var_domain.Max();
      } else {
        new_value = reduced_var_domain.ValueAtOrAfter(old_value);
      }
    }

    SetLiteralValue(encoding.at(new_value), true);
    CHECK(!encoding.contains(old_value));
    SetVarValue(var, new_value);
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
  if (proto_ != nullptr) {
    auto* step = proto_->add_steps()->mutable_update_literals_with_dominance();
    step->set_lit(lit);
    step->set_dominating_lit(dominating_lit);
  }
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
  if (proto_ != nullptr) {
    auto* step = proto_->add_steps()
                     ->mutable_maybe_update_var_with_symmetries_to_value();
    step->set_var(var);
    step->set_value(value);
    for (const auto& generator : generators) {
      auto* gen_proto = step->add_generators();
      for (int i = 0; i < generator->NumCycles(); ++i) {
        for (const int var : generator->Cycle(i)) {
          gen_proto->add_support(var);
        }
        gen_proto->add_cycle_sizes(generator->Cycle(i).size());
      }
    }
  }
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
  if (proto_ != nullptr) {
    auto* step = proto_->add_steps()->mutable_maybe_swap_orbitope_columns();
    for (const auto& row : orbitope) {
      auto* orbitope_row = step->add_orbitope_rows();
      for (const int literal : row) {
        orbitope_row->add_literals(literal);
      }
    }
    step->set_row(row);
    step->set_pivot_col(pivot_col);
    step->set_value(value);
  }
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
    const int src_var = orbitope[i][col];
    const int dst_var = orbitope[i][pivot_col];
    const int64_t src_value = GetVarValue(src_var);
    const int64_t dst_value = GetVarValue(dst_var);
    SetVarValue(src_var, dst_value);
    SetVarValue(dst_var, src_value);
  }
}

void SolutionCrush::SortOrbitopeColumns(
    absl::Span<const std::vector<int>> orbitope, int row) {
  if (proto_ != nullptr) {
    auto* step = proto_->add_steps()->mutable_sort_orbitope_columns();
    for (const auto& row : orbitope) {
      auto* orbitope_row = step->add_orbitope_rows();
      for (const int literal : row) {
        orbitope_row->add_literals(literal);
      }
    }
    step->set_row(row);
  }
  if (!solution_is_loaded_) return;

  // Collect the value and original column of each variable in `row`.
  std::vector<std::pair<int64_t, int>> var_value_and_column;
  for (int col = 0; col < orbitope[row].size(); ++col) {
    const int var = orbitope[row][col];
    if (!HasValue(var)) return;
    var_value_and_column.push_back({GetVarValue(var), col});
  }
  std::sort(var_value_and_column.begin(), var_value_and_column.end());

  // Permute the columns of the orbitope according to the sorted columns.
  std::vector<int64_t> values(orbitope[0].size());
  for (int i = 0; i < orbitope.size(); ++i) {
    for (int j = 0; j < orbitope[i].size(); ++j) {
      values[j] = GetVarValue(orbitope[i][j]);
    }
    for (int j = 0; j < orbitope[i].size(); ++j) {
      SetVarValue(orbitope[i][j], values[var_value_and_column[j].second]);
    }
  }
}

void SolutionCrush::UpdateRefsWithDominance(
    int ref, int64_t min_value, int64_t max_value,
    absl::Span<const std::pair<int, Domain>> dominating_refs) {
  if (proto_ != nullptr) {
    auto* step = proto_->add_steps()->mutable_update_terms_with_dominance();
    step->set_var(PositiveRef(ref));
    step->set_coeff(RefIsPositive(ref) ? 1 : -1);
    step->set_min_value(min_value);
    step->set_max_value(max_value);
    for (const auto& [dominating_ref, dominating_ref_domain] :
         dominating_refs) {
      auto* dominating_term_proto = step->add_dominating_terms();
      dominating_term_proto->set_var(PositiveRef(dominating_ref));
      dominating_term_proto->set_coeff(RefIsPositive(dominating_ref) ? 1 : -1);
      FillDomainInProto(dominating_ref_domain, dominating_term_proto);
    }
  }
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
    absl::Span<const int> enforcement_lits, std::optional<int> var_index,
    absl::Span<const int> vars, absl::Span<const int64_t> default_values,
    absl::Span<const int64_t> coeffs, int64_t rhs) {
  DCHECK_EQ(vars.size(), coeffs.size());
  DCHECK(!var_index.has_value() || var_index.value() < vars.size());
  if (proto_ != nullptr) {
    auto* step =
        proto_->add_steps()->mutable_set_var_to_linear_constraint_solution();
    step->mutable_enforcement_lits()->Add(enforcement_lits.begin(),
                                          enforcement_lits.end());
    if (var_index.has_value()) step->set_var_index(var_index.value());
    step->mutable_vars()->Add(vars.begin(), vars.end());
    step->mutable_default_values()->Add(default_values.begin(),
                                        default_values.end());
    step->mutable_coeffs()->Add(coeffs.begin(), coeffs.end());
    step->set_rhs(rhs);
  }
  if (!solution_is_loaded_) return;
  bool constraint_is_enforced = true;
  for (const int lit : enforcement_lits) {
    if (!HasValue(PositiveRef(lit))) return;
    constraint_is_enforced = constraint_is_enforced && GetLiteralValue(lit);
  }
  if (!constraint_is_enforced) {
    for (int i = 0; i < vars.size(); ++i) {
      if (!HasValue(vars[i])) {
        SetVarValue(vars[i], default_values[i]);
      }
    }
    return;
  }
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
  if (proto_ != nullptr) {
    auto* step = proto_->add_steps()->mutable_set_reservoir_circuit_vars();
    *step->mutable_reservoir() = reservoir;
    step->set_min_level(min_level);
    step->set_max_level(max_level);
    step->mutable_level_vars()->Add(level_vars.begin(), level_vars.end());
    *step->mutable_circuit() = circuit;
  }
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
  if (proto_ != nullptr) {
    auto* step =
        proto_->add_steps()->mutable_set_var_to_reified_precedence_literal();
    step->set_var(var);
    *step->mutable_time_i() = time_i;
    *step->mutable_time_j() = time_j;
    step->set_active_i(active_i);
    step->set_active_j(active_j);
  }
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
  if (proto_ != nullptr) {
    auto* step = proto_->add_steps()->mutable_set_int_mod_expanded_vars();
    *step->mutable_ct() = ct;
    step->set_div_var(div_var);
    step->set_prod_var(prod_var);
    step->set_default_div_value(default_div_value);
    step->set_default_prod_value(default_prod_value);
  }
  if (!solution_is_loaded_) return;
  bool constraint_is_enforced = true;
  for (const int lit : ct.enforcement_literal()) {
    if (!HasValue(PositiveRef(lit))) return;
    constraint_is_enforced = constraint_is_enforced && GetLiteralValue(lit);
  }
  if (!constraint_is_enforced) {
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
  if (proto_ != nullptr) {
    auto* step = proto_->add_steps()->mutable_set_int_prod_expanded_vars();
    *step->mutable_int_prod() = int_prod;
    step->mutable_prod_vars()->Add(prod_vars.begin(), prod_vars.end());
  }
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
  DCHECK_EQ(enforcement_lits.size(), lin_max.exprs_size());
  if (proto_ != nullptr) {
    auto* step = proto_->add_steps()->mutable_set_lin_max_expanded_vars();
    *step->mutable_lin_max() = lin_max;
    step->mutable_enforcement_lits()->Add(enforcement_lits.begin(),
                                          enforcement_lits.end());
  }
  if (!solution_is_loaded_) return;
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
  if (proto_ != nullptr) {
    auto* step = proto_->add_steps()->mutable_set_automaton_expanded_vars();
    *step->mutable_automaton() = automaton;
    for (const auto& [var, time, state] : state_vars) {
      auto* state_var = step->add_state_vars();
      state_var->set_var(var);
      state_var->set_time(time);
      state_var->set_state(state);
    }
    for (const auto& [var, time, transition_tail, transition_label] :
         transition_vars) {
      auto* transition_var = step->add_transition_vars();
      transition_var->set_var(var);
      transition_var->set_time(time);
      transition_var->set_transition_tail(transition_tail);
      transition_var->set_transition_label(transition_label);
    }
  }
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
  if (proto_ != nullptr) {
    auto* step = proto_->add_steps()->mutable_set_table_expanded_vars();
    step->mutable_column_vars()->Add(column_vars.begin(), column_vars.end());
    step->mutable_existing_row_lits()->Add(existing_row_lits.begin(),
                                           existing_row_lits.end());
    for (const auto& row_lit : new_row_lits) {
      auto* new_row_lit = step->add_new_row_lits();
      new_row_lit->set_lit(row_lit.lit);
      for (int i = 0; i < row_lit.var_values.size(); ++i) {
        for (const int64_t val : row_lit.var_values[i]) {
          new_row_lit->add_vars(i);
          new_row_lit->add_values(val);
        }
      }
    }
  }
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
          absl::c_find(values, GetVarValue(column_vars[var_index])) ==
              values.end()) {
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
  if (proto_ != nullptr) {
    auto* step = proto_->add_steps()
                     ->mutable_set_linear_with_complex_domain_expanded_vars();
    *step->mutable_linear() = linear;
    step->mutable_bucket_lits()->Add(bucket_lits.begin(), bucket_lits.end());
  }
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

void SolutionCrush::AssignVariableToPackingArea(
    const CompactVectorVector<int, Rectangle>& areas, const CpModelProto& model,
    absl::Span<const int> x_intervals, absl::Span<const int> y_intervals,
    absl::Span<const BoxInAreaLiteral> box_in_area_lits) {
  if (proto_ != nullptr) {
    auto* step = proto_->add_steps()->mutable_assign_variable_to_packing_area();
    for (int i = 0; i < areas.size(); ++i) {
      auto* rectangles_proto = step->add_areas();
      for (const Rectangle& rectangle : areas[i]) {
        auto* rectangle_proto = rectangles_proto->add_rectangles();
        rectangle_proto->set_x_min(rectangle.x_min.value());
        rectangle_proto->set_x_max(rectangle.x_max.value());
        rectangle_proto->set_y_min(rectangle.y_min.value());
        rectangle_proto->set_y_max(rectangle.y_max.value());
      }
    }
    for (const int x_interval : x_intervals) {
      *step->add_x_intervals() = model.constraints(x_interval);
    }
    for (const int y_interval : y_intervals) {
      *step->add_y_intervals() = model.constraints(y_interval);
    }
    for (const auto& box_in_area_lit : box_in_area_lits) {
      auto* box_in_area_lit_proto = step->add_box_in_area_lits();
      box_in_area_lit_proto->set_box_index(box_in_area_lit.box_index);
      box_in_area_lit_proto->set_area_index(box_in_area_lit.area_index);
      box_in_area_lit_proto->set_literal(box_in_area_lit.literal);
    }
  }
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

void SolutionCrush::RemapVariables(absl::Span<const int> old_to_new_mapping) {
  if (proto_ != nullptr) {
    auto* step = proto_->add_steps()->mutable_remap_variables();
    step->mutable_old_to_new_mapping()->Add(old_to_new_mapping.begin(),
                                            old_to_new_mapping.end());
  }
  if (!solution_is_loaded_) return;
  DCHECK_EQ(old_to_new_mapping.size(), var_has_value_.size());

  int new_num_vars = 0;
  for (int new_var : old_to_new_mapping) {
    new_num_vars = std::max(new_num_vars, new_var + 1);
  }
  std::vector<bool> var_has_value(new_num_vars, false);
  std::vector<int64_t> var_values(new_num_vars, 0);
  for (int old_var = 0; old_var < old_to_new_mapping.size(); ++old_var) {
    const int new_var = old_to_new_mapping[old_var];
    if (new_var >= 0 && var_has_value_[old_var]) {
      if (var_has_value[new_var]) {
        DCHECK_EQ(var_values[new_var], var_values_[old_var]);
      }
      var_has_value[new_var] = true;
      var_values[new_var] = var_values_[old_var];
    }
  }

  std::swap(var_has_value_, var_has_value);
  std::swap(var_values_, var_values);
}

void SolutionCrush::ApplySolutionCrushStep(
    const SolutionCrushStep& crush_step) {
  switch (crush_step.step_case()) {
    case SolutionCrushStep::kResize: {
      auto& step = crush_step.resize();
      Resize(step.new_size());
      break;
    }
    case SolutionCrushStep::kMaybeSetLiteralToOrderEncoding: {
      auto& step = crush_step.maybe_set_literal_to_order_encoding();
      MaybeSetLiteralToOrderEncoding(
          step.literal(), step.var(), step.value(),
          step.relation() == MaybeSetLiteralToOrderEncoding::LESS_THAN_OR_EQUAL
              ? Relation::LE
              : (step.relation() == MaybeSetLiteralToOrderEncoding::EQUAL
                     ? Relation::EQ
                     : Relation::GE));
      break;
    }
    case SolutionCrushStep::kSetVarToClause: {
      auto& step = crush_step.set_var_to_clause();
      SetVarToClause(step.var(), step.clause());
      break;
    }
    case SolutionCrushStep::kSetVarToConjunction: {
      auto& step = crush_step.set_var_to_conjunction();
      SetVarToConjunction(step.var(), step.conjunction());
      break;
    }
    case SolutionCrushStep::kSetVarToValueIfLinearConstraintViolated: {
      auto& step = crush_step.set_var_to_value_if_linear_constraint_violated();
      std::vector<std::pair<int, int64_t>> linear;
      linear.reserve(step.linear_constraint().vars_size());
      for (int i = 0; i < step.linear_constraint().vars_size(); ++i) {
        linear.push_back({step.linear_constraint().vars(i),
                          step.linear_constraint().coeffs(i)});
      }
      SetVarToValueIfLinearConstraintViolated(
          step.var(), step.value(), linear,
          ReadDomainFromProto(step.linear_constraint()));
      break;
    }
    case SolutionCrushStep::kSetVarToLinearExpressionIf: {
      auto& step = crush_step.set_var_to_linear_expression_if();
      SetVarToLinearExpressionIf(step.var(), step.expr(),
                                 step.condition_lits());
      break;
    }
    case SolutionCrushStep::kSetVarToConditionalValue: {
      auto& step = crush_step.set_var_to_conditional_value();
      SetVarToConditionalValue(step.var(), step.condition_lits(),
                               step.value_if_true(), step.value_if_false());
      break;
    }
    case SolutionCrushStep::kMakeLiteralsEqual: {
      auto& step = crush_step.make_literals_equal();
      MakeLiteralsEqual(step.lit1(), step.lit2());
      break;
    }
    case SolutionCrushStep::kSetOrUpdateVarToDomain: {
      auto& step = crush_step.set_or_update_var_to_domain();
      SetOrUpdateVarToDomain(step.var(), ReadDomainFromProto(step));
      break;
    }
    case SolutionCrushStep::kSetOrUpdateVarToDomainWithOptionalEscapeValue: {
      auto& step =
          crush_step.set_or_update_var_to_domain_with_optional_escape_value();
      SetOrUpdateVarToDomainWithOptionalEscapeValue(
          step.var(), ReadDomainFromProto(step),
          step.has_unique_escape_value()
              ? std::make_optional(step.unique_escape_value())
              : std::nullopt,
          step.push_down_when_not_in_domain(),
          absl::btree_map<int64_t, int>(step.encoding().begin(),
                                        step.encoding().end()));
      break;
    }
    case SolutionCrushStep::kUpdateLiteralsWithDominance: {
      auto& step = crush_step.update_literals_with_dominance();
      UpdateLiteralsWithDominance(step.lit(), step.dominating_lit());
      break;
    }
    case SolutionCrushStep::kUpdateTermsWithDominance: {
      auto& step = crush_step.update_terms_with_dominance();
      std::vector<std::pair<int, Domain>> dominating_refs;
      dominating_refs.reserve(step.dominating_terms_size());
      for (const auto& dominating_term : step.dominating_terms()) {
        CHECK_EQ(std::abs(dominating_term.coeff()), 1);
        dominating_refs.push_back({dominating_term.coeff() == 1
                                       ? dominating_term.var()
                                       : NegatedRef(dominating_term.var()),
                                   ReadDomainFromProto(dominating_term)});
      }
      CHECK_EQ(std::abs(step.coeff()), 1);
      UpdateRefsWithDominance(
          step.coeff() == 1 ? step.var() : NegatedRef(step.var()),
          step.min_value(), step.max_value(), dominating_refs);
      break;
    }
    case SolutionCrushStep::kMaybeUpdateVarWithSymmetriesToValue: {
      auto& step = crush_step.maybe_update_var_with_symmetries_to_value();
      const int num_vars = var_values_.size();
      std::vector<std::unique_ptr<SparsePermutation>> generators;
      generators.reserve(step.generators_size());
      for (const auto& perm : step.generators()) {
        generators.push_back(CreateSparsePermutationFromProto(num_vars, perm));
      }
      MaybeUpdateVarWithSymmetriesToValue(step.var(), step.value() != 0,
                                          generators);
      break;
    }
    case SolutionCrushStep::kMaybeSwapOrbitopeColumns: {
      auto& step = crush_step.maybe_swap_orbitope_columns();
      std::vector<std::vector<int>> orbitope;
      orbitope.reserve(step.orbitope_rows_size());
      for (const auto& row : step.orbitope_rows()) {
        orbitope.emplace_back(row.literals().begin(), row.literals().end());
      }
      MaybeSwapOrbitopeColumns(orbitope, step.row(), step.pivot_col(),
                               step.value());
      break;
    }
    case SolutionCrushStep::kSortOrbitopeColumns: {
      auto& step = crush_step.sort_orbitope_columns();
      std::vector<std::vector<int>> orbitope;
      orbitope.reserve(step.orbitope_rows_size());
      for (const auto& row : step.orbitope_rows()) {
        orbitope.emplace_back(row.literals().begin(), row.literals().end());
      }
      SortOrbitopeColumns(orbitope, step.row());
      break;
    }
    case SolutionCrushStep::kSetVarToLinearConstraintSolution: {
      auto& step = crush_step.set_var_to_linear_constraint_solution();
      SetVarToLinearConstraintSolution(
          step.enforcement_lits(),
          step.has_var_index() ? std::make_optional(step.var_index())
                               : std::nullopt,
          step.vars(), step.default_values(), step.coeffs(), step.rhs());
      break;
    }
    case SolutionCrushStep::kSetReservoirCircuitVars: {
      auto& step = crush_step.set_reservoir_circuit_vars();
      SetReservoirCircuitVars(step.reservoir(), step.min_level(),
                              step.max_level(), step.level_vars(),
                              step.circuit());
      break;
    }
    case SolutionCrushStep::kSetVarToReifiedPrecedenceLiteral: {
      auto& step = crush_step.set_var_to_reified_precedence_literal();
      SetVarToReifiedPrecedenceLiteral(step.var(), step.time_i(), step.time_j(),
                                       step.active_i(), step.active_j());
      break;
    }
    case SolutionCrushStep::kSetIntModExpandedVars: {
      auto& step = crush_step.set_int_mod_expanded_vars();
      SetIntModExpandedVars(step.ct(), step.div_var(), step.prod_var(),
                            step.default_div_value(),
                            step.default_prod_value());
      break;
    }
    case SolutionCrushStep::kSetIntProdExpandedVars: {
      auto& step = crush_step.set_int_prod_expanded_vars();
      SetIntProdExpandedVars(step.int_prod(), step.prod_vars());
      break;
    }
    case SolutionCrushStep::kSetLinMaxExpandedVars: {
      auto& step = crush_step.set_lin_max_expanded_vars();
      SetLinMaxExpandedVars(step.lin_max(), step.enforcement_lits());
      break;
    }
    case SolutionCrushStep::kSetAutomatonExpandedVars: {
      auto& step = crush_step.set_automaton_expanded_vars();
      std::vector<StateVar> state_vars;
      state_vars.reserve(step.state_vars_size());
      for (const auto& var : step.state_vars()) {
        state_vars.push_back({var.var(), var.time(), var.state()});
      }
      std::vector<TransitionVar> transition_vars;
      transition_vars.reserve(step.transition_vars_size());
      for (const auto& var : step.transition_vars()) {
        transition_vars.push_back({var.var(), var.time(), var.transition_tail(),
                                   var.transition_label()});
      }
      SetAutomatonExpandedVars(step.automaton(), state_vars, transition_vars);
      break;
    }
    case SolutionCrushStep::kSetTableExpandedVars: {
      auto& step = crush_step.set_table_expanded_vars();
      std::vector<TableRowLiteral> new_row_lits;
      new_row_lits.reserve(step.new_row_lits_size());
      for (const auto& r : step.new_row_lits()) {
        TableRowLiteral row_lit;
        row_lit.lit = r.lit();
        row_lit.var_values.resize(step.column_vars_size());
        for (int i = 0; i < r.vars_size(); ++i) {
          row_lit.var_values[r.vars(i)].push_back(r.values(i));
        }
        new_row_lits.push_back(std::move(row_lit));
      }
      SetTableExpandedVars(step.column_vars(), step.existing_row_lits(),
                           new_row_lits);
      break;
    }
    case SolutionCrushStep::kSetLinearWithComplexDomainExpandedVars: {
      auto& step = crush_step.set_linear_with_complex_domain_expanded_vars();
      SetLinearWithComplexDomainExpandedVars(step.linear(), step.bucket_lits());
      break;
    }
    case SolutionCrushStep::kAssignVariableToPackingArea: {
      auto& step = crush_step.assign_variable_to_packing_area();
      CompactVectorVector<int, Rectangle> areas;
      areas.reserve(step.areas_size());
      for (const auto& area : step.areas()) {
        areas.Add({});
        for (const auto& rectangle : area.rectangles()) {
          areas.AppendToLastVector({.x_min = rectangle.x_min(),
                                    .x_max = rectangle.x_max(),
                                    .y_min = rectangle.y_min(),
                                    .y_max = rectangle.y_max()});
        }
      }
      CpModelProto cp_model_proto;
      std::vector<int> x_intervals;
      std::vector<int> y_intervals;
      x_intervals.reserve(step.x_intervals_size());
      y_intervals.reserve(step.y_intervals_size());
      for (const auto& x_interval : step.x_intervals()) {
        x_intervals.push_back(cp_model_proto.constraints_size());
        *cp_model_proto.add_constraints() = x_interval;
      }
      for (const auto& y_interval : step.y_intervals()) {
        y_intervals.push_back(cp_model_proto.constraints_size());
        *cp_model_proto.add_constraints() = y_interval;
      }
      std::vector<BoxInAreaLiteral> box_in_area_lits;
      box_in_area_lits.reserve(step.box_in_area_lits_size());
      for (const auto& box_in_area_lit : step.box_in_area_lits()) {
        box_in_area_lits.push_back({.box_index = box_in_area_lit.box_index(),
                                    .area_index = box_in_area_lit.area_index(),
                                    .literal = box_in_area_lit.literal()});
      }
      AssignVariableToPackingArea(areas, cp_model_proto, x_intervals,
                                  y_intervals, box_in_area_lits);
      break;
    }
    case SolutionCrushStep::kRemapVariables: {
      auto& step = crush_step.remap_variables();
      RemapVariables(step.old_to_new_mapping());
      break;
    }
    case SolutionCrushStep::STEP_NOT_SET:
    default:
      LOG(FATAL) << "Unsupported SolutionCrushStep: " << crush_step.step_case();
  }
}

void SolutionCrush::StoreSolutionAsHint(CpModelProto& model) const {
  if (!solution_is_loaded_) return;
  model.clear_solution_hint();
  if (!var_values_.empty()) {
    StoreSolutionAsHint(*model.mutable_solution_hint());
  }
}

void SolutionCrush::StoreSolutionAsHint(PartialVariableAssignment& hint) const {
  if (!solution_is_loaded_) return;
  for (int i = 0; i < var_values_.size(); ++i) {
    if (var_has_value_[i]) {
      hint.add_vars(i);
      hint.add_values(var_values_[i]);
    }
  }
}

void SolutionCrush::PermuteVariables(const SparsePermutation& permutation) {
  CHECK(solution_is_loaded_);
  permutation.ApplyToDenseCollection(var_has_value_);
  permutation.ApplyToDenseCollection(var_values_);
}

}  // namespace sat
}  // namespace operations_research
