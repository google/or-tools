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
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/util.h"
#include "ortools/util/sorted_interval_list.h"

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

int LinearIncrementalEvaluator::NewConstraint(Domain domain) {
  domains_.push_back(domain);
  offsets_.push_back(0);
  activities_.push_back(0);
  num_false_enforcement_.push_back(0);
  distances_.push_back(0);
  return num_constraints_++;
}

void LinearIncrementalEvaluator::AddEnforcementLiteral(int ct_index, int lit) {
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
  // Resets the activity as the offset.
  activities_ = offsets_;

  // Updates activities from variables and coefficients.
  for (int var = 0; var < var_entries_.size(); ++var) {
    const int64_t value = solution[var];
    if (value == 0) continue;
    for (const auto [ct_index, coeff] : var_entries_[var]) {
      activities_[ct_index] += coeff * value;
    }
  }

  // Update enforcement literals count.
  num_false_enforcement_.assign(num_constraints_, 0);
  for (int var = 0; var < literal_entries_.size(); ++var) {
    const bool literal_is_true = solution[var] != 0;
    for (const auto [ct_index, positive] : literal_entries_[var]) {
      if (literal_is_true != positive) {
        num_false_enforcement_[ct_index]++;
      }
    }
  }

  // Cache violations (not counting enforcement).
  for (int c = 0; c < num_constraints_; ++c) {
    ComputeAndCacheDistance(c);
  }
}

void LinearIncrementalEvaluator::Update(int var, int64_t delta) {
  DCHECK_NE(delta, 0);
  if (var < literal_entries_.size()) {
    const bool literal_is_true = delta == 1;
    for (const LiteralEntry& entry : literal_entries_[var]) {
      const int ct_index = entry.ct_index;
      if (literal_is_true == entry.positive) {
        num_false_enforcement_[ct_index]--;
      } else {
        num_false_enforcement_[ct_index]++;
      }
    }
  }

  if (var < var_entries_.size()) {
    for (const auto [c, coeff] : var_entries_[var]) {
      activities_[c] += coeff * delta;
      ComputeAndCacheDistance(c);
    }
  }

  if (DEBUG_MODE) {
    for (int ct_index = 0; ct_index < num_constraints_; ++ct_index) {
      DCHECK_GE(num_false_enforcement_[ct_index], 0);
    }
  }
}

int64_t LinearIncrementalEvaluator::Activity(int ct_index) const {
  return activities_[ct_index];
}

bool LinearIncrementalEvaluator::IsEnforced(int ct_index) const {
  return num_false_enforcement_[ct_index] == 0;
}

void LinearIncrementalEvaluator::ComputeAndCacheDistance(int ct_index) {
  distances_[ct_index] = domains_[ct_index].Distance(activities_[ct_index]);
}

int64_t LinearIncrementalEvaluator::Violation(int ct_index) const {
  if (!IsEnforced(ct_index)) return 0;
  return distances_[ct_index];
}

void LinearIncrementalEvaluator::ReduceBounds(int ct_index, int64_t lb,
                                              int64_t ub) {
  domains_[ct_index] = domains_[ct_index].IntersectionWith(Domain(lb, ub));
  ComputeAndCacheDistance(ct_index);
}

double LinearIncrementalEvaluator::WeightedViolation(
    absl::Span<const double> weights) const {
  double result = 0.0;
  DCHECK_GE(weights.size(), num_constraints_);
  for (int c = 0; c < num_constraints_; ++c) {
    if (!IsEnforced(c)) continue;
    result += weights[c] * static_cast<double>(distances_[c]);
  }
  return result;
}

double LinearIncrementalEvaluator::WeightedViolationDelta(
    absl::Span<const double> weights, int var, int64_t delta) const {
  DCHECK_NE(delta, 0);
  double result = 0.0;

  // Compute the delta due to enforcement.
  if (var < literal_entries_.size()) {
    for (const auto [c, is_positive] : literal_entries_[var]) {
      if (IsEnforced(c)) {
        // An enforced constraint can only become unenforced if delta != 0.
        result -= weights[c] * static_cast<double>(distances_[c]);
      } else {
        const bool plus_one_true = (delta == 1) == is_positive;
        if (plus_one_true && num_false_enforcement_[c] == 1) {
          result += weights[c] * static_cast<double>(distances_[c]);
        }
      }
    }
  }

  // Compute the delta due to violations.
  if (var < var_entries_.size()) {
    for (const auto [c, coeff] : var_entries_[var]) {
      if (!IsEnforced(c)) continue;
      const int64_t old_distance = distances_[c];
      const int64_t new_distance =
          domains_[c].Distance(activities_[c] + coeff * delta);
      result += weights[c] * static_cast<double>(new_distance - old_distance);
    }
  }

  return result;
}

bool LinearIncrementalEvaluator::AppearsInViolatedConstraints(int var) const {
  if (var < literal_entries_.size()) {
    for (const LiteralEntry& entry : literal_entries_[var]) {
      if (Violation(entry.ct_index) > 0) return true;
    }
  }
  if (var < var_entries_.size()) {
    for (const Entry& entry : var_entries_[var]) {
      if (Violation(entry.ct_index) > 0) return true;
    }
  }
  return false;
}

std::vector<int64_t> LinearIncrementalEvaluator::SlopeBreakpoints(
    int var, int64_t current_value, const Domain& var_domain) const {
  std::vector<int64_t> result = var_domain.FlattenedIntervals();
  if (var_domain.Size() <= 2) return result;

  if (var < var_entries_.size()) {
    for (const auto [c, coeff] : var_entries_[var]) {
      if (!IsEnforced(c)) continue;

      // We only consider min / max.
      // There is a change when we cross the slack.
      // TODO(user): Deal with holes?
      const int64_t activity = activities_[c] - current_value * coeff;
      const int64_t slack_min = domains_[c].Min() - activity;
      const int64_t slack_max = domains_[c].Max() - activity;
      {
        const int64_t ceil_bp = CeilOfRatio(slack_min, coeff);
        if (ceil_bp != result.back() && var_domain.Contains(ceil_bp)) {
          result.push_back(ceil_bp);
        }
        const int64_t floor_bp = FloorOfRatio(slack_min, coeff);
        if (floor_bp != result.back() && var_domain.Contains(floor_bp)) {
          result.push_back(floor_bp);
        }
      }
      if (slack_min != slack_max) {
        const int64_t ceil_bp = CeilOfRatio(slack_max, coeff);
        if (ceil_bp != result.back() && var_domain.Contains(ceil_bp)) {
          result.push_back(ceil_bp);
        }
        const int64_t floor_bp = FloorOfRatio(slack_max, coeff);
        if (floor_bp != result.back() && var_domain.Contains(floor_bp)) {
          result.push_back(floor_bp);
        }
      }
    }
  }

  // We want a value different from current_value, so we need to include nearby
  // values.
  if (var_domain.Contains(current_value - 1)) {
    result.push_back(current_value - 1);
  }
  if (var_domain.Contains(current_value + 1)) {
    result.push_back(current_value + 1);
  }
  gtl::STLSortAndRemoveDuplicates(&result);

  // Eliminate the current solution value while keeping sorted order.
  for (int i = 0; i < result.size(); ++i) {
    if (result[i] == current_value) {
      result.erase(result.begin() + i);
      break;
    }
  }
  DCHECK(!result.empty());

  return result;
}

void LinearIncrementalEvaluator::PrecomputeTransposedView() {
  if (num_constraints_ == 0) return;

  // Compute the total size.
  int total_size = 0;
  tmp_row_sizes_.assign(num_constraints_, 0);
  for (const auto& column : literal_entries_) {
    for (const auto [c, _] : column) {
      total_size++;
      tmp_row_sizes_[c]++;
    }
  }
  for (const auto& column : var_entries_) {
    for (const auto [c, _] : column) {
      total_size++;
      tmp_row_sizes_[c]++;
    }
  }
  row_buffer_.resize(total_size);

  // Corner case, some constraints but no entry !? we need size 1 for MakeSpan()
  // below not to crash.
  //
  // TODO(user): we should probably filter empty constraint before here (this
  // only happens in test).
  if (total_size == 0) row_buffer_.resize(1);

  // transform tmp_row_sizes_ to starts in the buffer.
  // Initialize the spans.
  int offset = 0;
  rows_.resize(num_constraints_);
  for (int c = 0; c < num_constraints_; ++c) {
    const int start = offset;
    offset += tmp_row_sizes_[c];
    rows_[c] = absl::MakeSpan(&row_buffer_[start], tmp_row_sizes_[c]);
    tmp_row_sizes_[c] = start;
  }
  DCHECK_EQ(offset, total_size);

  // Copy data.
  for (int var = 0; var < literal_entries_.size(); ++var) {
    for (const auto [c, _] : literal_entries_[var]) {
      row_buffer_[tmp_row_sizes_[c]++] = var;
    }
  }
  for (int var = 0; var < var_entries_.size(); ++var) {
    for (const auto [c, _] : var_entries_[var]) {
      row_buffer_[tmp_row_sizes_[c]++] = var;
    }
  }
}

// TODO(user): Depending on the status of an enforced constraint, we might not
// need to recompute data.
std::vector<int> LinearIncrementalEvaluator::AffectedVariableOnUpdate(int var) {
  std::vector<int> result;
  tmp_in_result_.resize(std::max(literal_entries_.size(), var_entries_.size()),
                        false);
  if (var < literal_entries_.size()) {
    for (const auto [c, _] : literal_entries_[var]) {
      for (const int affected : rows_[c]) {
        if (!tmp_in_result_[affected]) {
          tmp_in_result_[affected] = true;
          result.push_back(affected);
        }
      }
    }
  }
  if (var < var_entries_.size()) {
    for (const auto [c, _] : var_entries_[var]) {
      for (const int affected : rows_[c]) {
        if (!tmp_in_result_[affected]) {
          tmp_in_result_[affected] = true;
          result.push_back(affected);
        }
      }
    }
  }
  for (const int var : result) tmp_in_result_[var] = false;
  return result;
}

std::vector<int> LinearIncrementalEvaluator::ConstraintsToAffectedVariables(
    absl::Span<const int> ct_indices) {
  std::vector<int> result;
  tmp_in_result_.resize(std::max(literal_entries_.size(), var_entries_.size()),
                        false);
  for (const int c : ct_indices) {
    // This is needed since constraint indices can refer to more general
    // constraint than linear ones.
    if (c >= rows_.size()) continue;
    for (const int affected : rows_[c]) {
      if (!tmp_in_result_[affected]) {
        tmp_in_result_[affected] = true;
        result.push_back(affected);
      }
    }
  }
  for (const int var : result) tmp_in_result_[var] = false;
  return result;
}

// ----- CompiledConstraint -----

CompiledConstraint::CompiledConstraint(const ConstraintProto& proto)
    : proto_(proto) {}

void CompiledConstraint::CacheViolation(absl::Span<const int64_t> solution) {
  violation_ = Violation(solution);
}

int64_t CompiledConstraint::violation() const {
  DCHECK_GE(violation_, 0);
  return violation_;
}

const ConstraintProto& CompiledConstraint::proto() const { return proto_; }

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
  DCHECK_EQ(proto().int_prod().exprs_size(), 2);
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
  DCHECK_EQ(proto().int_div().exprs_size(), 2);
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
    const int ct_index = linear_evaluator_.NewConstraint(
        model_.objective().domain().empty()
            ? Domain::AllValues()
            : ReadDomainFromProto(model_.objective()));
    DCHECK_EQ(0, ct_index);
    for (int i = 0; i < model_.objective().vars_size(); ++i) {
      const int var = model_.objective().vars(i);
      const int64_t coeff = model_.objective().coeffs(i);
      linear_evaluator_.AddTerm(ct_index, var, coeff);
    }
  }

  for (const ConstraintProto& ct : model_.constraints()) {
    switch (ct.constraint_case()) {
      case ConstraintProto::ConstraintCase::kBoolOr: {
        const int ct_index = linear_evaluator_.NewConstraint(
            Domain(1, ct.bool_or().literals_size()));
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
            linear_evaluator_.NewConstraint(Domain(num_literals));
        for (const int lit : ct.enforcement_literal()) {
          linear_evaluator_.AddEnforcementLiteral(ct_index, lit);
        }
        for (const int lit : ct.bool_and().literals()) {
          linear_evaluator_.AddLiteral(ct_index, lit);
        }
        break;
      }
      case ConstraintProto::ConstraintCase::kAtMostOne: {
        DCHECK(ct.enforcement_literal().empty());
        const int ct_index = linear_evaluator_.NewConstraint({0, 1});
        for (const int lit : ct.at_most_one().literals()) {
          linear_evaluator_.AddLiteral(ct_index, lit);
        }
        break;
      }
      case ConstraintProto::ConstraintCase::kExactlyOne: {
        DCHECK(ct.enforcement_literal().empty());
        const int ct_index = linear_evaluator_.NewConstraint({1, 1});
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
        const Domain domain = ReadDomainFromProto(ct.linear());
        const int ct_index = linear_evaluator_.NewConstraint(domain);
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
        VLOG(1) << "Not implemented: " << ct.constraint_case();
        model_is_supported_ = false;
        break;
    }
  }
}

void LsEvaluator::ReduceObjectiveBounds(int64_t lb, int64_t ub) {
  if (!model_.has_objective()) return;
  linear_evaluator_.ReduceBounds(/*ct_index=*/0, lb, ub);
}

void LsEvaluator::ComputeInitialViolations(absl::Span<const int64_t> solution) {
  current_solution_.assign(solution.begin(), solution.end());

  // Linear constraints.
  linear_evaluator_.ComputeInitialActivities(current_solution_);

  // Generic constraints.
  for (const auto& ct : constraints_) {
    ct->CacheViolation(current_solution_);
  }
}

void LsEvaluator::UpdateNonLinearViolations() {
  // Generic constraints.
  for (const auto& ct : constraints_) {
    ct->CacheViolation(current_solution_);
  }
}

void LsEvaluator::UpdateVariableValueAndRecomputeViolations(
    int var, int64_t new_value, bool focus_on_linear) {
  DCHECK(RefIsPositive(var));
  const int64_t old_value = current_solution_[var];
  if (old_value == new_value) return;

  // Linear part.
  linear_evaluator_.Update(var, new_value - old_value);
  current_solution_[var] = new_value;
  if (focus_on_linear) return;

  // Non linear part.
  for (const int ct_index : var_to_constraint_graph_[var]) {
    constraints_[ct_index]->CacheViolation(current_solution_);
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
    evaluation += ct->violation();
  }
  return evaluation;
}

int64_t LsEvaluator::ObjectiveActivity() const {
  DCHECK(model_.has_objective());
  return linear_evaluator_.Activity(/*ct_index=*/0);
}

int LsEvaluator::NumLinearConstraints() const {
  return linear_evaluator_.num_constraints();
}

int LsEvaluator::NumNonLinearConstraints() const {
  return static_cast<int>(constraints_.size());
}

int LsEvaluator::NumEvaluatorConstraints() const {
  return linear_evaluator_.num_constraints() +
         static_cast<int>(constraints_.size());
}

int LsEvaluator::NumInfeasibleConstraints() const {
  int result = 0;
  for (int c = 0; c < linear_evaluator_.num_constraints(); ++c) {
    if (linear_evaluator_.Violation(c) > 0) {
      ++result;
    }
  }
  for (const auto& constraint : constraints_) {
    if (constraint->violation() > 0) {
      ++result;
    }
  }
  return result;
}

int64_t LsEvaluator::Violation(int c) const {
  if (c < linear_evaluator_.num_constraints()) {
    return linear_evaluator_.Violation(c);
  } else {
    return constraints_[c - linear_evaluator_.num_constraints()]->violation();
  }
}

double LsEvaluator::WeightedViolation(absl::Span<const double> weights) const {
  DCHECK_EQ(weights.size(), NumEvaluatorConstraints());
  double violations = linear_evaluator_.WeightedViolation(weights);

  const int num_linear_constraints = linear_evaluator_.num_constraints();
  for (int c = 0; c < constraints_.size(); ++c) {
    violations += static_cast<double>(constraints_[c]->violation()) *
                  weights[num_linear_constraints + c];
  }
  return violations;
}

double LsEvaluator::WeightedViolationDelta(absl::Span<const double> weights,
                                           int var, int64_t delta) const {
  DCHECK_LT(var, current_solution_.size());
  ++num_deltas_computed_;
  double violation_delta =
      linear_evaluator_.WeightedViolationDelta(weights, var, delta);

  // We change the mutable solution here, are restore it after the evaluation.
  current_solution_[var] += delta;
  const int num_linear_constraints = linear_evaluator_.num_constraints();
  for (const int ct_index : var_to_constraint_graph_[var]) {
    DCHECK_LT(ct_index, constraints_.size());
    const int64_t ct_eval_before = constraints_[ct_index]->violation();
    const int64_t ct_eval_after =
        constraints_[ct_index]->Violation(current_solution_);
    violation_delta += static_cast<double>(ct_eval_after - ct_eval_before) *
                       weights[ct_index + num_linear_constraints];
  }
  // Restore.
  current_solution_[var] -= delta;

  return violation_delta;
}

// TODO(user): Speed-up:
//    - Use a row oriented representation of the model (could reuse the Apply
//       methods on proto).
//    - Maintain the list of violated constraints ?
std::vector<int> LsEvaluator::VariablesInViolatedConstraints() const {
  std::vector<int> variables;
  for (int var = 0; var < model_.variables_size(); ++var) {
    if (linear_evaluator_.AppearsInViolatedConstraints(var)) {
      variables.push_back(var);
    } else {
      for (const int ct_index : var_to_constraint_graph_[var]) {
        if (constraints_[ct_index]->violation() > 0) {
          variables.push_back(var);
          break;
        }
      }
    }
  }

  gtl::STLSortAndRemoveDuplicates(&variables);
  return variables;
}

}  // namespace sat
}  // namespace operations_research
