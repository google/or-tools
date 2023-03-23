// Copyright 2010-2022 Google LLC
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

#include "ortools/sat/constraint_violation.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <vector>

#include "absl/log/check.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"

namespace operations_research {
namespace sat {

int64_t ExprValue(const LinearExpressionProto& expr,
                  absl::Span<const int64_t> solution) {
  int64_t result = expr.offset();
  for (int i = 0; i < expr.vars_size(); ++i) {
    result += solution[expr.vars(i)] * expr.coeffs(i);
  }
  return result;
}

bool LiteralValue(int lit, absl::Span<const int64_t> solution) {
  const int var = PositiveRef(lit);
  return (solution[var] == 1) == RefIsPositive(lit);
}

// ---- LinearIncrementalEvaluator -----

int LinearIncrementalEvaluator::NewConstraint(int64_t lb, int64_t ub) {
  const int ct_index = lower_bounds_.size();
  num_enforcement_literals_.push_back(0);
  lower_bounds_.push_back(lb);
  upper_bounds_.push_back(ub);
  offsets_.push_back(0);
  num_true_enforcement_literals_.push_back(0);
  activities_.push_back(0);
  return ct_index;
}

void LinearIncrementalEvaluator::AddEnforcementLiteral(int ct_index, int lit) {
  // Update the row-major storage.
  num_enforcement_literals_[ct_index]++;

  // Update the column-major storage.
  const int var = PositiveRef(lit);
  if (literal_entries_.size() <= var) {
    literal_entries_.resize(var + 1);
  }
  literal_entries_[var].push_back(
      {.ct_index = ct_index, .positive = RefIsPositive(lit)});
}

void LinearIncrementalEvaluator::AddLiteral(int ct_index, int lit) {
  if (RefIsPositive(lit)) {
    AddTerm(ct_index, lit, 1, 0);
  } else {
    AddTerm(ct_index, PositiveRef(lit), -1, 1);
  }
}

void LinearIncrementalEvaluator::AddTerm(int ct_index, int var, int64_t coeff,
                                         int64_t offset) {
  if (var_entries_.size() <= var) {
    var_entries_.resize(var + 1);
  }
  var_entries_[var].push_back({.ct_index = ct_index, .coefficient = coeff});
  AddOffset(ct_index, offset);
}

void LinearIncrementalEvaluator::AddOffset(int ct_index, int64_t offset) {
  offsets_[ct_index] += offset;
}

void LinearIncrementalEvaluator::ComputeInitialActivities(
    absl::Span<const int64_t> solution) {
  const int num_vars = var_entries_.size();
  const int num_constraints = num_enforcement_literals_.size();

  // Resets the activity as the offset.
  activities_ = offsets_;

  // Updates activities from variables and coefficients.
  for (int var = 0; var < num_vars; ++var) {
    if (var >= var_entries_.size()) break;

    const int64_t value = solution[var];
    if (value == 0) continue;
    for (const auto& entry : var_entries_[var]) {
      activities_[entry.ct_index] += entry.coefficient * value;
    }
  }

  // Reset the num_true_enforcement_literals_.
  num_true_enforcement_literals_.assign(num_constraints, 0);

  // Update enforcement literals count.
  for (int var = 0; var < num_vars; ++var) {
    if (var >= literal_entries_.size()) break;

    const bool literal_is_true = solution[var] != 0;
    for (const auto& entry : literal_entries_[var]) {
      if (literal_is_true == entry.positive) {
        num_true_enforcement_literals_[entry.ct_index]++;
      }
    }
  }
}

void LinearIncrementalEvaluator::Update(int var, int64_t old_value,
                                        int64_t new_value) {
  DCHECK_NE(old_value, new_value);
  if (var < literal_entries_.size()) {
    const bool literal_is_true = new_value != 0;
    for (const LiteralEntry& entry : literal_entries_[var]) {
      const int ct_index = entry.ct_index;
      if (literal_is_true == entry.positive) {
        num_true_enforcement_literals_[ct_index]++;
      } else {
        num_true_enforcement_literals_[ct_index]--;
      }
    }
  }

  if (var < var_entries_.size()) {
    for (const auto& entry : var_entries_[var]) {
      activities_[entry.ct_index] +=
          entry.coefficient * (new_value - old_value);
    }
  }

  if (DEBUG_MODE) {
    for (int ct_index = 0; ct_index < num_enforcement_literals_.size();
         ++ct_index) {
      DCHECK_GE(num_true_enforcement_literals_[ct_index], 0);
      DCHECK_LE(num_true_enforcement_literals_[ct_index],
                num_enforcement_literals_[ct_index]);
    }
  }
}

int64_t LinearIncrementalEvaluator::Violation(int ct_index) const {
  if (num_true_enforcement_literals_[ct_index] <
      num_enforcement_literals_[ct_index]) {
    return 0;
  }

  const int64_t act = activities_[ct_index];
  if (act < lower_bounds_[ct_index]) {
    return lower_bounds_[ct_index] - act;
  } else if (act > upper_bounds_[ct_index]) {
    return act - upper_bounds_[ct_index];
  } else {
    return 0;
  }
}

void LinearIncrementalEvaluator::ResetBounds(int ct_index, int64_t lb,
                                             int64_t ub) {
  lower_bounds_[ct_index] = lb;
  upper_bounds_[ct_index] = ub;
}

// ----- CompiledConstraint -----

CompiledConstraint::CompiledConstraint(const ConstraintProto& proto)
    : proto_(proto) {}

// ----- CompiledLinMaxConstraint -----

// The violation of a lin_max constraint is:
// - the sum(max(0, expr_value - target_value) forall expr)
// - min(target_value - expr_value for all expr) if the above sum is 0
class CompiledLinMaxConstraint : public CompiledConstraint {
 public:
  explicit CompiledLinMaxConstraint(const ConstraintProto& proto);
  ~CompiledLinMaxConstraint() override = default;

  int64_t Violation(absl::Span<const int64_t> solution) override;
};

CompiledLinMaxConstraint::CompiledLinMaxConstraint(const ConstraintProto& proto)
    : CompiledConstraint(proto) {}

int64_t CompiledLinMaxConstraint::Violation(
    absl::Span<const int64_t> solution) {
  const int64_t target_value = ExprValue(proto().lin_max().target(), solution);
  int64_t sum_of_excesses = 0;
  int64_t min_missing_quantities = std::numeric_limits<int64_t>::max();
  for (const LinearExpressionProto& expr : proto().lin_max().exprs()) {
    const int64_t expr_value = ExprValue(expr, solution);
    if (expr_value <= target_value) {
      min_missing_quantities =
          std::min(min_missing_quantities, target_value - expr_value);
    } else {
      sum_of_excesses += expr_value - target_value;
    }
  }
  if (sum_of_excesses > 0) {
    return sum_of_excesses;
  } else {
    return min_missing_quantities;
  }
}

// ----- CompiledIntProdConstraint -----

// The violation of an int_prod constraint is
//     abs(value(target) - prod(value(expr)).
class CompiledIntProdConstraint : public CompiledConstraint {
 public:
  explicit CompiledIntProdConstraint(const ConstraintProto& proto);
  ~CompiledIntProdConstraint() override = default;

  int64_t Violation(absl::Span<const int64_t> solution) override;
};

CompiledIntProdConstraint::CompiledIntProdConstraint(
    const ConstraintProto& proto)
    : CompiledConstraint(proto) {}

int64_t CompiledIntProdConstraint::Violation(
    absl::Span<const int64_t> solution) {
  const int64_t target_value = ExprValue(proto().int_prod().target(), solution);
  CHECK_EQ(proto().int_prod().exprs_size(), 2);
  const int64_t prod_value = ExprValue(proto().int_prod().exprs(0), solution) *
                             ExprValue(proto().int_prod().exprs(1), solution);
  return std::abs(target_value - prod_value);
}

// ----- CompiledIntDivConstraint -----

// The violation of an int_div constraint is
//     abs(value(target) - value(expr0) / value(expr1)).
class CompiledIntDivConstraint : public CompiledConstraint {
 public:
  explicit CompiledIntDivConstraint(const ConstraintProto& proto);
  ~CompiledIntDivConstraint() override = default;

  int64_t Violation(absl::Span<const int64_t> solution) override;
};

CompiledIntDivConstraint::CompiledIntDivConstraint(const ConstraintProto& proto)
    : CompiledConstraint(proto) {}

int64_t CompiledIntDivConstraint::Violation(
    absl::Span<const int64_t> solution) {
  const int64_t target_value = ExprValue(proto().int_div().target(), solution);
  CHECK_EQ(proto().int_div().exprs_size(), 2);
  const int64_t div_value = ExprValue(proto().int_div().exprs(0), solution) /
                            ExprValue(proto().int_div().exprs(1), solution);
  return std::abs(target_value - div_value);
}

// ----- LsEvaluator -----

LsEvaluator::LsEvaluator(const CpModelProto& model) : model_(model) {
  var_to_constraint_graph_.resize(model_.variables_size());
  CompileConstraintsAndObjective();
  BuildVarConstraintGraph();
}

void LsEvaluator::BuildVarConstraintGraph() {
  // Clear the var <-> constraint graph.
  for (std::vector<int>& ct_indices : var_to_constraint_graph_) {
    ct_indices.clear();
  }

  // Build the constraint graph.
  for (int ct_index = 0; ct_index < constraints_.size(); ++ct_index) {
    for (const int var : UsedVariables(constraints_[ct_index]->proto())) {
      var_to_constraint_graph_[var].push_back(ct_index);
    }
  }
}

void LsEvaluator::CompileConstraintsAndObjective() {
  constraints_.clear();

  // The first compiled constraint is always the objective if present.
  if (model_.has_objective()) {
    const int ct_index =
        linear_evaluator_.NewConstraint(std::numeric_limits<int64_t>::min(),
                                        std::numeric_limits<int64_t>::max());
    CHECK_EQ(0, ct_index);
    for (int i = 0; i < model_.objective().vars_size(); ++i) {
      const int var = model_.objective().vars(i);
      const int64_t coeff = model_.objective().coeffs(i);
      linear_evaluator_.AddTerm(ct_index, var, coeff);
    }
  }

  for (const ConstraintProto& ct : model_.constraints()) {
    switch (ct.constraint_case()) {
      case ConstraintProto::ConstraintCase::kBoolOr: {
        const int ct_index =
            linear_evaluator_.NewConstraint(1, ct.bool_or().literals_size());
        for (const int lit : ct.enforcement_literal()) {
          linear_evaluator_.AddEnforcementLiteral(ct_index, lit);
        }
        for (const int lit : ct.bool_or().literals()) {
          linear_evaluator_.AddLiteral(ct_index, lit);
        }
        break;
      }
      case ConstraintProto::ConstraintCase::kBoolAnd: {
        const int num_literals = ct.bool_and().literals_size();
        const int ct_index =
            linear_evaluator_.NewConstraint(num_literals, num_literals);
        for (const int lit : ct.enforcement_literal()) {
          linear_evaluator_.AddEnforcementLiteral(ct_index, lit);
        }
        for (const int lit : ct.bool_and().literals()) {
          linear_evaluator_.AddLiteral(ct_index, lit);
        }
        break;
      }
      case ConstraintProto::ConstraintCase::kAtMostOne: {
        CHECK(ct.enforcement_literal().empty());
        const int ct_index = linear_evaluator_.NewConstraint(0, 1);
        for (const int lit : ct.at_most_one().literals()) {
          linear_evaluator_.AddLiteral(ct_index, lit);
        }
        break;
      }
      case ConstraintProto::ConstraintCase::kExactlyOne: {
        CHECK(ct.enforcement_literal().empty());
        const int ct_index = linear_evaluator_.NewConstraint(1, 1);
        for (const int lit : ct.exactly_one().literals()) {
          linear_evaluator_.AddLiteral(ct_index, lit);
        }
        break;
      }
      case ConstraintProto::ConstraintCase::kLinMax: {
        CompiledLinMaxConstraint* lin_max = new CompiledLinMaxConstraint(ct);
        constraints_.emplace_back(lin_max);
        break;
      }
      case ConstraintProto::ConstraintCase::kIntProd: {
        CompiledIntProdConstraint* int_prod = new CompiledIntProdConstraint(ct);
        constraints_.emplace_back(int_prod);
        break;
      }
      case ConstraintProto::ConstraintCase::kIntDiv: {
        CompiledIntDivConstraint* int_div = new CompiledIntDivConstraint(ct);
        constraints_.emplace_back(int_div);
        break;
      }
      case ConstraintProto::ConstraintCase::kLinear: {
        CHECK_EQ(ct.linear().domain_size(), 2);
        const int ct_index = linear_evaluator_.NewConstraint(
            ct.linear().domain(0), ct.linear().domain(1));
        for (const int lit : ct.enforcement_literal()) {
          linear_evaluator_.AddEnforcementLiteral(ct_index, lit);
        }
        for (int i = 0; i < ct.linear().vars_size(); ++i) {
          const int var = ct.linear().vars(i);
          const int64_t coeff = ct.linear().coeffs(i);
          linear_evaluator_.AddTerm(ct_index, var, coeff);
        }
        break;
      }
      default:
        LOG(FATAL) << "Not implemented" << ct.constraint_case();
        break;
    }
  }
}

void LsEvaluator::SetObjectiveBounds(int64_t lb, int64_t ub) {
  if (!model_.has_objective()) return;
  linear_evaluator_.ResetBounds(/*ct_index=*/0, lb, ub);
}

void LsEvaluator::ComputeInitialViolations(absl::Span<const int64_t> solution) {
  current_solution_.assign(solution.begin(), solution.end());

  // Linear constraints.
  linear_evaluator_.ComputeInitialActivities(solution);

  // Generic constraints.
  for (const auto& ct : constraints_) {
    const int64_t ct_eval = ct->Violation(solution);
    ct->clear_violations();
    ct->push_violation(ct_eval);
  }
}

void LsEvaluator::UpdateVariableValueAndRecomputeViolations(int var,
                                                            int64_t new_value) {
  CHECK(RefIsPositive(var));
  const int64_t old_value = current_solution_[var];
  if (old_value == new_value) return;

  current_solution_[var] = new_value;
  linear_evaluator_.Update(var, old_value, new_value);
  for (const int ct_index : var_to_constraint_graph_[var]) {
    const int64_t ct_eval =
        constraints_[ct_index]->Violation(current_solution_);
    constraints_[ct_index]->clear_violations();
    constraints_[ct_index]->push_violation(ct_eval);
  }
}

int64_t LsEvaluator::SumOfViolations() {
  int64_t evaluation = 0;

  // Process the linear part.
  for (int i = 0; i < linear_evaluator_.num_constraints(); ++i) {
    evaluation += linear_evaluator_.Violation(i);
  }

  // Process the generic constraint part.
  for (const auto& ct : constraints_) {
    evaluation += ct->current_violation();
  }
  return evaluation;
}

}  // namespace sat
}  // namespace operations_research
