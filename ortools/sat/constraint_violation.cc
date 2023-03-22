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
#include <functional>
#include <limits>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/util/saturated_arithmetic.h"

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
  enforcement_literals_.resize(ct_index + 1);
  lower_bounds_.push_back(lb);
  upper_bounds_.push_back(ub);
  offsets_.push_back(0);
  enforced_.push_back(true);
  activities_.push_back(0);
  return ct_index;
}

void LinearIncrementalEvaluator::AddEnforcementLiteral(int ct_index, int lit) {
  // Update the row-major storage.
  enforcement_literals_[ct_index].push_back(lit);

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
  // Process enforcement literals.
  for (int ct_index = 0; ct_index < enforced_.size(); ++ct_index) {
    // Resets the activity as the offset.
    activities_[ct_index] = offsets_[ct_index];

    // Checks if the constraint is enforced.
    enforced_[ct_index] = true;
    for (const int lit : enforcement_literals_[ct_index]) {
      if (!LiteralValue(lit, solution)) {
        enforced_[ct_index] = false;
        return;
      }
    }
  }

  // Updates activities from variables and coefficients.
  for (int var = 0; var < var_entries_.size(); ++var) {
    const int64_t value = solution[var];
    if (value == 0) continue;
    for (const auto& entry : var_entries_[var]) {
      activities_[entry.ct_index] += entry.coefficient * value;
    }
  }

  if (VLOG_IS_ON(2)) {
    for (int ct_index = 0; ct_index < enforced_.size(); ++ct_index) {
      VLOG(2) << "Linear(" << ct_index
              << "): enforced = " << enforced_[ct_index]
              << ", activities = " << activities_[ct_index];
    }
  }
}

void LinearIncrementalEvaluator::Update(int var, int64_t old_value,
                                        absl::Span<const int64_t> solution) {
  if (var < literal_entries_.size()) {
    for (const LiteralEntry& entry : literal_entries_[var]) {
      const int ct_index = entry.ct_index;
      if ((solution[var] == 0) == entry.positive) {
        enforced_[ct_index] = false;
      } else {
        enforced_[ct_index] = true;
        for (const int lit : enforcement_literals_[ct_index]) {
          if (!LiteralValue(lit, solution)) {
            enforced_[ct_index] = false;
            return;
          }
        }
      }
    }
  }

  if (var < var_entries_.size()) {
    for (const auto& entry : var_entries_[var]) {
      activities_[entry.ct_index] +=
          entry.coefficient * (solution[var] - old_value);
    }
  }

  if (VLOG_IS_ON(2)) {
    for (int ct_index = 0; ct_index < enforced_.size(); ++ct_index) {
    }
  }
}

int64_t LinearIncrementalEvaluator::Violation(int ct_index) const {
  if (!enforced_[ct_index]) {
    VLOG(2) << "Linear(" << ct_index << "): violation = 0 as not enforced";
    return 0;
  }

  const int64_t act = activities_[ct_index];
  if (act < lower_bounds_[ct_index]) {
    VLOG(2) << "Linear(" << ct_index
            << "): violation = " << lower_bounds_[ct_index] - act;
    return lower_bounds_[ct_index] - act;
  } else if (act > upper_bounds_[ct_index]) {
    VLOG(2) << "Linear(" << ct_index
            << "): violation = " << act - upper_bounds_[ct_index];
    return act - upper_bounds_[ct_index];
  } else {
    VLOG(2) << "Linear(" << ct_index << "): violation = 0";
    return 0;
  }
}

void LinearIncrementalEvaluator::ResetObjectiveBounds(int64_t lb, int64_t ub) {
  lower_bounds_[0] = lb;
  upper_bounds_[0] = ub;
}

// ----- CompiledLinMaxConstraint -----

CompiledLinMaxConstraint::CompiledLinMaxConstraint(
    const LinearArgumentProto& proto)
    : proto_(proto) {}

int64_t CompiledLinMaxConstraint::Evaluate(absl::Span<const int64_t> solution) {
  const int64_t target_value = ExprValue(proto_.target(), solution);
  int64_t sum_of_excesses = 0;
  int64_t min_missing_quantities = std::numeric_limits<int64_t>::max();
  for (const LinearExpressionProto& expr : proto_.exprs()) {
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

void CompiledLinMaxConstraint::CallOnEachVariable(
    std::function<void(int)> func) const {
  for (const int var : proto_.target().vars()) {
    func(var);
  }
  for (const LinearExpressionProto& expr : proto_.exprs()) {
    for (const int var : expr.vars()) {
      func(var);
    }
  }
}

// ----- CompiledProductConstraint -----

CompiledProductConstraint::CompiledProductConstraint(
    const LinearArgumentProto& proto)
    : proto_(proto) {}

int64_t CompiledProductConstraint::Evaluate(
    absl::Span<const int64_t> solution) {
  const int64_t target_value = ExprValue(proto_.target(), solution);
  CHECK_EQ(proto_.exprs_size(), 2);
  const int64_t prod_value = ExprValue(proto_.exprs(0), solution) *
                             ExprValue(proto_.exprs(1), solution);
  return std::abs(target_value - prod_value);
}

void CompiledProductConstraint::CallOnEachVariable(
    std::function<void(int)> func) const {
  for (const int var : proto_.target().vars()) {
    func(var);
  }
  for (const LinearExpressionProto& expr : proto_.exprs()) {
    for (const int var : expr.vars()) {
      func(var);
    }
  }
}

// ----- LsEvaluator -----

LsEvaluator::LsEvaluator(const CpModelProto& model) : model_(model) {
  var_to_constraint_graph_.resize(model_.variables_size());
}

void LsEvaluator::UpdateVariableDomains(const CpModelProto& variables_only) {
  *model_.mutable_variables() = variables_only.variables();
  CompileModel();
}

void LsEvaluator::CompileModel() {
  CompileConstraintsAndObjective();
  BuildVarConstraintGraph();
}

void LsEvaluator::BuildVarConstraintGraph() {
  // Clear the var <-> constraint graph.
  for (std::vector<int>& ct_indices : var_to_constraint_graph_) {
    ct_indices.clear();
  }

  // Build the constraint graph.
  const auto collect = [this](int var) {
    if (VariableIsFixed(var)) return;
    tmp_vars_.insert(var);
  };

  for (int ct_index = 0; ct_index < constraints_.size(); ++ct_index) {
    tmp_vars_.clear();
    constraints_[ct_index]->CallOnEachVariable(collect);
    for (const int var : tmp_vars_) {
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
      if (VariableIsFixed(var)) {
        linear_evaluator_.AddOffset(ct_index, VariableValue(var) * coeff);
      } else {
        linear_evaluator_.AddTerm(ct_index, var, coeff);
      }
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
      case ConstraintProto::ConstraintCase::kBoolXor:
        LOG(FATAL) << "Not implemented" << ct.constraint_case();
        break;
      case ConstraintProto::ConstraintCase::kIntDiv:
        LOG(FATAL) << "Not implemented" << ct.constraint_case();
        break;
      case ConstraintProto::ConstraintCase::kIntMod:
        LOG(FATAL) << "Not implemented" << ct.constraint_case();
        break;
      case ConstraintProto::ConstraintCase::kLinMax: {
        CompiledLinMaxConstraint* lin_max =
            new CompiledLinMaxConstraint(ct.lin_max());
        constraints_.emplace_back(lin_max);
        break;
      }
      case ConstraintProto::ConstraintCase::kIntProd: {
        CompiledProductConstraint* int_prod =
            new CompiledProductConstraint(ct.int_prod());
        constraints_.emplace_back(int_prod);
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
          if (VariableIsFixed(var)) {
            linear_evaluator_.AddOffset(ct_index, VariableValue(var) * coeff);
          } else {
            linear_evaluator_.AddTerm(ct_index, var, coeff);
          }
        }
        break;
      }
      case ConstraintProto::ConstraintCase::kAllDiff:
        LOG(FATAL) << "Not implemented" << ct.constraint_case();
        break;
      case ConstraintProto::ConstraintCase::kDummyConstraint:
        LOG(FATAL) << "Not implemented" << ct.constraint_case();
        break;
      case ConstraintProto::ConstraintCase::kElement:
        LOG(FATAL) << "Not implemented" << ct.constraint_case();
        break;
      case ConstraintProto::ConstraintCase::kCircuit:
        LOG(FATAL) << "Not implemented" << ct.constraint_case();
        break;
      case ConstraintProto::ConstraintCase::kRoutes:
        LOG(FATAL) << "Not implemented" << ct.constraint_case();
        break;
      case ConstraintProto::ConstraintCase::kInverse:
        LOG(FATAL) << "Not implemented" << ct.constraint_case();
        break;
      case ConstraintProto::ConstraintCase::kReservoir:
        LOG(FATAL) << "Not implemented" << ct.constraint_case();
        break;
      case ConstraintProto::ConstraintCase::kTable:
        LOG(FATAL) << "Not implemented" << ct.constraint_case();
        break;
      case ConstraintProto::ConstraintCase::kAutomaton:
        LOG(FATAL) << "Not implemented" << ct.constraint_case();
        break;
      case ConstraintProto::ConstraintCase::kInterval:
        LOG(FATAL) << "Not implemented" << ct.constraint_case();
        break;
      case ConstraintProto::ConstraintCase::kNoOverlap:
        LOG(FATAL) << "Not implemented" << ct.constraint_case();
        break;
      case ConstraintProto::ConstraintCase::kNoOverlap2D:
        LOG(FATAL) << "Not implemented" << ct.constraint_case();
        break;
      case ConstraintProto::ConstraintCase::kCumulative:
        LOG(FATAL) << "Not implemented" << ct.constraint_case();
        break;
      case ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET:
        break;
    }
  }
}

void LsEvaluator::SetObjectiveBounds(int64_t lb, int64_t ub) {
  if (!model_.has_objective()) return;
  linear_evaluator_.ResetObjectiveBounds(lb, ub);
}

void LsEvaluator::ComputeInitialViolations(absl::Span<const int64_t> solution) {
  current_solution_.assign(solution.begin(), solution.end());

  // Linear constraints.
  linear_evaluator_.ComputeInitialActivities(solution);

  // Generic constraints.
  for (const auto& ct : constraints_) {
    const int64_t ct_eval = ct->Evaluate(solution);
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
  linear_evaluator_.Update(var, old_value, current_solution_);
  for (const int ct_index : var_to_constraint_graph_[var]) {
    const int64_t ct_eval = constraints_[ct_index]->Evaluate(current_solution_);
    constraints_[ct_index]->clear_violations();
    constraints_[ct_index]->push_violation(ct_eval);
  }
}

int64_t LsEvaluator::SumOfViolations() {
  int64_t evaluation = 0;

  // Process the linear part.
  for (int lin_index = 0; lin_index < linear_evaluator_.num_constraints();
       ++lin_index) {
    evaluation += linear_evaluator_.Violation(lin_index);
  }

  // Process the generic constraint part.
  for (const auto& ct : constraints_) {
    evaluation += ct->current_violation();
  }
  return evaluation;
}

bool LsEvaluator::VariableIsFixed(int ref) const {
  const int var = PositiveRef(ref);
  const IntegerVariableProto& var_proto = model_.variables(var);
  return var_proto.domain_size() == 2 &&
         var_proto.domain(0) == var_proto.domain(1);
}

int64_t LsEvaluator::VariableMin(int var) const {
  CHECK(RefIsPositive(var));
  const IntegerVariableProto& var_proto = model_.variables(var);
  return var_proto.domain(0);
}

int64_t LsEvaluator::VariableMax(int var) const {
  CHECK(RefIsPositive(var));
  const IntegerVariableProto& var_proto = model_.variables(var);
  return var_proto.domain(var_proto.domain_size() - 1);
}

int64_t LsEvaluator::VariableValue(int var) const {
  CHECK(VariableIsFixed(var));
  return VariableMin(var);
}

bool LsEvaluator::LiteralValue(int lit) const {
  CHECK(VariableIsFixed(lit));
  return RefIsPositive(lit) == (VariableValue(PositiveRef(lit)) == 1);
}

bool LsEvaluator::LiteralIsFalse(int lit) const {
  if (!VariableIsFixed(lit)) return false;
  return RefIsPositive(lit) == (VariableValue(PositiveRef(lit)) == 0);
}

bool LsEvaluator::LiteralIsTrue(int lit) const {
  if (!VariableIsFixed(lit)) return false;
  return RefIsPositive(lit) == (VariableValue(PositiveRef(lit)) == 1);
}

}  // namespace sat
}  // namespace operations_research
