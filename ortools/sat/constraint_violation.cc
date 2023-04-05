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
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/util.h"
#include "ortools/util/saturated_arithmetic.h"
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

int64_t ExprMin(const LinearExpressionProto& expr, const CpModelProto& model) {
  int64_t result = expr.offset();
  for (int i = 0; i < expr.vars_size(); ++i) {
    const IntegerVariableProto& var_proto = model.variables(expr.vars(i));
    if (expr.coeffs(i) > 0) {
      result += expr.coeffs(i) * var_proto.domain(0);
    } else {
      result += expr.coeffs(i) * var_proto.domain(var_proto.domain_size() - 1);
    }
  }
  return result;
}

int64_t ExprMax(const LinearExpressionProto& expr, const CpModelProto& model) {
  int64_t result = expr.offset();
  for (int i = 0; i < expr.vars_size(); ++i) {
    const IntegerVariableProto& var_proto = model.variables(expr.vars(i));
    if (expr.coeffs(i) > 0) {
      result += expr.coeffs(i) * var_proto.domain(var_proto.domain_size() - 1);
    } else {
      result += expr.coeffs(i) * var_proto.domain(0);
    }
  }
  return result;
}

bool LiteralValue(int lit, absl::Span<const int64_t> solution) {
  if (RefIsPositive(lit)) {
    return solution[lit] != 0;
  } else {
    return solution[PositiveRef(lit)] == 0;
  }
}

// ---- LinearIncrementalEvaluator -----

int LinearIncrementalEvaluator::NewConstraint(Domain domain) {
  DCHECK(creation_phase_);
  domains_.push_back(domain);
  offsets_.push_back(0);
  activities_.push_back(0);
  num_false_enforcement_.push_back(0);
  distances_.push_back(0);
  return num_constraints_++;
}

void LinearIncrementalEvaluator::AddEnforcementLiteral(int ct_index, int lit) {
  DCHECK(creation_phase_);
  const int var = PositiveRef(lit);
  if (literal_entries_.size() <= var) {
    literal_entries_.resize(var + 1);
  }
  literal_entries_[var].push_back(
      {.ct_index = ct_index, .positive = RefIsPositive(lit)});
}

void LinearIncrementalEvaluator::AddLiteral(int ct_index, int lit) {
  DCHECK(creation_phase_);
  if (RefIsPositive(lit)) {
    AddTerm(ct_index, lit, 1, 0);
  } else {
    AddTerm(ct_index, PositiveRef(lit), -1, 1);
  }
}

void LinearIncrementalEvaluator::AddTerm(int ct_index, int var, int64_t coeff,
                                         int64_t offset) {
  DCHECK(creation_phase_);
  if (var_entries_.size() <= var) {
    var_entries_.resize(var + 1);
  }
  if (!var_entries_[var].empty() &&
      var_entries_[var].back().ct_index == ct_index) {
    var_entries_[var].back().coefficient += coeff;
    if (var_entries_[var].back().coefficient == 0) {
      var_entries_[var].pop_back();
    }
  } else {
    var_entries_[var].push_back({.ct_index = ct_index, .coefficient = coeff});
  }
  AddOffset(ct_index, offset);
  DCHECK(VarIsConsistent(var));
}

void LinearIncrementalEvaluator::AddOffset(int ct_index, int64_t offset) {
  DCHECK(creation_phase_);
  offsets_[ct_index] += offset;
}

void LinearIncrementalEvaluator::AddLinearExpression(
    int ct_index, const LinearExpressionProto& expr, int64_t multiplier) {
  DCHECK(creation_phase_);
  AddOffset(ct_index, expr.offset() * multiplier);
  for (int i = 0; i < expr.vars_size(); ++i) {
    AddTerm(ct_index, expr.vars(i), expr.coeffs(i) * multiplier);
  }
}

bool LinearIncrementalEvaluator::VarIsConsistent(int var) const {
  if (var_entries_.size() <= var) return true;

  absl::flat_hash_set<int> visited;
  for (const Entry& entry : var_entries_[var]) {
    if (!visited.insert(entry.ct_index).second) return false;
  }
  return true;
}

void LinearIncrementalEvaluator::ComputeInitialActivities(
    absl::Span<const int64_t> solution) {
  DCHECK(!creation_phase_);

  // Resets the activity as the offset and the number of false enforcement to 0.
  activities_ = offsets_;
  num_false_enforcement_.assign(num_constraints_, 0);
  row_update_will_not_impact_deltas_.assign(num_constraints_, false);

  // Update these numbers for all columns.
  for (int var = 0; var < columns_.size(); ++var) {
    const VarData& data = columns_[var];
    const int64_t value = solution[var];

    int i = data.start;
    for (int k = 0; k < data.num_pos_literal; ++k, ++i) {
      const int c = ct_buffer_[i];
      if (value == 0) num_false_enforcement_[c]++;
    }
    for (int k = 0; k < data.num_neg_literal; ++k, ++i) {
      const int c = ct_buffer_[i];
      if (value == 1) num_false_enforcement_[c]++;
    }

    if (value == 0) continue;
    int j = data.linear_start;
    for (int k = 0; k < data.num_linear_entries; ++k, ++i, ++j) {
      const int c = ct_buffer_[i];
      const int64_t coeff = coeff_buffer_[j];
      activities_[c] += coeff * value;
    }
  }

  // Cache violations (not counting enforcement).
  for (int c = 0; c < num_constraints_; ++c) {
    distances_[c] = domains_[c].Distance(activities_[c]);
  }
}

// Note that the code assumes that a column has no duplicates ct indices.
void LinearIncrementalEvaluator::Update(int var, int64_t delta) {
  DCHECK(!creation_phase_);
  DCHECK_NE(delta, 0);
  if (var >= columns_.size()) return;

  const VarData& data = columns_[var];
  int i = data.start;
  for (int k = 0; k < data.num_pos_literal; ++k, ++i) {
    const int c = ct_buffer_[i];
    if (delta == 1) {
      num_false_enforcement_[c]--;
      DCHECK_GE(num_false_enforcement_[c], 0);

      // A transition 3 -> 2 or 2 -> 1 with a distance of zero can be ignored.
      row_update_will_not_impact_deltas_[c] =
          num_false_enforcement_[c] >= 2 ||
          (num_false_enforcement_[c] == 1 && distances_[c] == 0);
    } else {
      num_false_enforcement_[c]++;

      // A transition 2 -> 3 or 1 -> 2 with a distance of zero can be ignored.
      row_update_will_not_impact_deltas_[c] =
          num_false_enforcement_[c] >= 3 ||
          (num_false_enforcement_[c] == 2 && distances_[c] == 0);
    }
  }
  for (int k = 0; k < data.num_neg_literal; ++k, ++i) {
    const int c = ct_buffer_[i];
    if (delta == -1) {
      num_false_enforcement_[c]--;
      DCHECK_GE(num_false_enforcement_[c], 0);

      // A transition 3 -> 2 or 2 -> 1 with a distance of zero can be ignored.
      row_update_will_not_impact_deltas_[c] =
          num_false_enforcement_[c] >= 2 ||
          (num_false_enforcement_[c] == 1 && distances_[c] == 0);
    } else {
      num_false_enforcement_[c]++;

      // A transition 2 -> 3 or 1 -> 2 with a distance of zero can be ignored.
      row_update_will_not_impact_deltas_[c] =
          num_false_enforcement_[c] >= 3 ||
          (num_false_enforcement_[c] == 2 && distances_[c] == 0);
    }
  }
  int j = data.linear_start;
  for (int k = 0; k < data.num_linear_entries; ++k, ++i, ++j) {
    const int c = ct_buffer_[i];
    const int64_t coeff = coeff_buffer_[j];
    activities_[c] += coeff * delta;

    const int64_t old_distance = distances_[c];
    distances_[c] = domains_[c].Distance(activities_[c]);

    // If this constraint enforcement was >= 2, then the change
    // in activity cannot impact any deltas. Or if enforcement
    // is >= 1 and the distance before and after is the same.
    row_update_will_not_impact_deltas_[c] =
        num_false_enforcement_[c] >= 2 ||
        (num_false_enforcement_[c] >= 1 && distances_[c] == old_distance);
  }
}

int64_t LinearIncrementalEvaluator::Activity(int c) const {
  return activities_[c];
}

int64_t LinearIncrementalEvaluator::Violation(int c) const {
  return num_false_enforcement_[c] > 0 ? 0 : distances_[c];
}

void LinearIncrementalEvaluator::ReduceBounds(int c, int64_t lb, int64_t ub) {
  domains_[c] = domains_[c].IntersectionWith(Domain(lb, ub));
  distances_[c] = domains_[c].Distance(activities_[c]);
}

double LinearIncrementalEvaluator::WeightedViolation(
    absl::Span<const double> weights) const {
  double result = 0.0;
  DCHECK_GE(weights.size(), num_constraints_);
  for (int c = 0; c < num_constraints_; ++c) {
    if (num_false_enforcement_[c] > 0) continue;
    result += weights[c] * static_cast<double>(distances_[c]);
  }
  return result;
}

// Most of the time is spent in this function.
//
// TODO(user): We can safely abort early if we know that delta will be >= 0.
// TODO(user): Maybe we can compute an absolute value instead of removing
// old_distance.
double LinearIncrementalEvaluator::WeightedViolationDelta(
    absl::Span<const double> weights, int var, int64_t delta) const {
  DCHECK_NE(delta, 0);
  if (var >= columns_.size()) return 0.0;
  const VarData& data = columns_[var];

  int i = data.start;
  double result = 0.0;
  dtime_ += data.num_pos_literal;
  for (int k = 0; k < data.num_pos_literal; ++k, ++i) {
    const int c = ct_buffer_[i];
    if (num_false_enforcement_[c] == 0) {
      // Since delta != 0, we are sure this is an enforced -> unenforced change.
      DCHECK_EQ(delta, -1);
      result -= weights[c] * static_cast<double>(distances_[c]);
    } else {
      if (delta == 1 && num_false_enforcement_[c] == 1) {
        result += weights[c] * static_cast<double>(distances_[c]);
      }
    }
  }

  dtime_ += data.num_neg_literal;
  for (int k = 0; k < data.num_neg_literal; ++k, ++i) {
    const int c = ct_buffer_[i];
    if (num_false_enforcement_[c] == 0) {
      // Since delta != 0, we are sure this is an enforced -> unenforced change.
      DCHECK_EQ(delta, 1);
      result -= weights[c] * static_cast<double>(distances_[c]);
    } else {
      if (delta == -1 && num_false_enforcement_[c] == 1) {
        result += weights[c] * static_cast<double>(distances_[c]);
      }
    }
  }

  int j = data.linear_start;
  dtime_ += data.num_linear_entries;
  for (int k = 0; k < data.num_linear_entries; ++k, ++i, ++j) {
    const int c = ct_buffer_[i];
    if (num_false_enforcement_[c] > 0) continue;
    const int64_t coeff = coeff_buffer_[j];
    const int64_t old_distance = distances_[c];
    const int64_t new_distance =
        domains_[c].Distance(activities_[c] + coeff * delta);
    result += weights[c] * static_cast<double>(new_distance - old_distance);
  }

  return result;
}

bool LinearIncrementalEvaluator::AppearsInViolatedConstraints(int var) const {
  if (var >= columns_.size()) return false;
  for (const int c : VarToConstraints(var)) {
    if (Violation(c) > 0) return true;
  }
  return false;
}

std::vector<int64_t> LinearIncrementalEvaluator::SlopeBreakpoints(
    int var, int64_t current_value, const Domain& var_domain) const {
  std::vector<int64_t> result = var_domain.FlattenedIntervals();
  if (var_domain.Size() <= 2 || var >= columns_.size()) return result;

  const VarData& data = columns_[var];
  int i = data.start + data.num_pos_literal + data.num_neg_literal;
  int j = data.linear_start;
  for (int k = 0; k < data.num_linear_entries; ++k, ++i, ++j) {
    const int c = ct_buffer_[i];
    if (num_false_enforcement_[c] > 0) continue;

    // We only consider min / max.
    // There is a change when we cross the slack.
    // TODO(user): Deal with holes?
    const int64_t coeff = coeff_buffer_[j];
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

  gtl::STLSortAndRemoveDuplicates(&result);
  return result;
}

void LinearIncrementalEvaluator::PrecomputeCompactView() {
  creation_phase_ = false;
  if (num_constraints_ == 0) return;

  // Compactify for faster WeightedViolationDelta().
  columns_.resize(std::max(literal_entries_.size(), var_entries_.size()));
  for (int var = 0; var < columns_.size(); ++var) {
    columns_[var].start = static_cast<int>(ct_buffer_.size());
    columns_[var].linear_start = static_cast<int>(coeff_buffer_.size());
    if (var < literal_entries_.size()) {
      for (const auto [c, positive] : literal_entries_[var]) {
        if (positive) {
          columns_[var].num_pos_literal++;
          ct_buffer_.push_back(c);
        }
      }
      for (const auto [c, positive] : literal_entries_[var]) {
        if (!positive) {
          columns_[var].num_neg_literal++;
          ct_buffer_.push_back(c);
        }
      }
    }
    if (var < var_entries_.size()) {
      for (const auto [c, coeff] : var_entries_[var]) {
        columns_[var].num_linear_entries++;
        ct_buffer_.push_back(c);
        coeff_buffer_.push_back(coeff);
      }
    }
  }

  // Compute the total size.
  // Note that at this point the constraint indices are not "encoded" yet.
  int total_size = 0;
  tmp_row_sizes_.assign(num_constraints_, 0);
  for (auto& column : literal_entries_) {
    for (auto& entry : column) {
      total_size++;
      tmp_row_sizes_[entry.ct_index]++;
    }
  }
  for (auto& column : var_entries_) {
    for (auto& entry : column) {
      total_size++;
      tmp_row_sizes_[entry.ct_index]++;
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

  // We do not need var_entries_ or literal_entries_ anymore.
  //
  // TODO(user): We could delete them before. But at the time of this
  // optimization, I didn't want to change the behavior of the algorithm at all.
  gtl::STLClearObject(&var_entries_);
  gtl::STLClearObject(&literal_entries_);
}

// TODO(user): Depending on the status of an enforced constraint, we might not
// need to recompute data. We do a bit of that but we could do more.
std::vector<int> LinearIncrementalEvaluator::AffectedVariableOnUpdate(int var) {
  std::vector<int> result;
  if (var >= columns_.size()) return result;
  tmp_in_result_.resize(columns_.size(), false);
  for (const int c : VarToConstraints(var)) {
    if (row_update_will_not_impact_deltas_[c]) continue;
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

// Note that this is meant to be called on weight update and as such only on
// enforced constraint. No need to check row_update_will_not_impact_deltas_.
std::vector<int> LinearIncrementalEvaluator::ConstraintsToAffectedVariables(
    absl::Span<const int> ct_indices) {
  std::vector<int> result;
  tmp_in_result_.resize(columns_.size(), false);
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

CompiledConstraint::CompiledConstraint(const ConstraintProto& ct_proto)
    : ct_proto_(ct_proto) {}

void CompiledConstraint::CacheViolation(absl::Span<const int64_t> solution) {
  violation_ = Violation(solution);
}

int64_t CompiledConstraint::violation() const {
  DCHECK_GE(violation_, 0);
  return violation_;
}

const ConstraintProto& CompiledConstraint::ct_proto() const {
  return ct_proto_;
}

// ----- CompiledLinMaxConstraint -----

// The violation of a lin_max constraint is:
// - the sum(max(0, expr_value - target_value) forall expr). This part will be
//   maintained by the linear part.
// - target_value - max(expressions) if positive.
class CompiledLinMaxConstraint : public CompiledConstraint {
 public:
  explicit CompiledLinMaxConstraint(const ConstraintProto& ct_proto);
  ~CompiledLinMaxConstraint() override = default;

  int64_t Violation(absl::Span<const int64_t> solution) override;
};

CompiledLinMaxConstraint::CompiledLinMaxConstraint(
    const ConstraintProto& ct_proto)
    : CompiledConstraint(ct_proto) {}

int64_t CompiledLinMaxConstraint::Violation(
    absl::Span<const int64_t> solution) {
  const int64_t target_value =
      ExprValue(ct_proto().lin_max().target(), solution);
  int64_t max_of_expressions = std::numeric_limits<int64_t>::min();
  for (const LinearExpressionProto& expr : ct_proto().lin_max().exprs()) {
    const int64_t expr_value = ExprValue(expr, solution);
    max_of_expressions = std::max(max_of_expressions, expr_value);
  }
  return std::max(target_value - max_of_expressions, int64_t{0});
}

// ----- CompiledIntProdConstraint -----

// The violation of an int_prod constraint is
//     abs(value(target) - prod(value(expr)).
class CompiledIntProdConstraint : public CompiledConstraint {
 public:
  explicit CompiledIntProdConstraint(const ConstraintProto& ct_proto);
  ~CompiledIntProdConstraint() override = default;

  int64_t Violation(absl::Span<const int64_t> solution) override;
};

CompiledIntProdConstraint::CompiledIntProdConstraint(
    const ConstraintProto& ct_proto)
    : CompiledConstraint(ct_proto) {}

int64_t CompiledIntProdConstraint::Violation(
    absl::Span<const int64_t> solution) {
  const int64_t target_value =
      ExprValue(ct_proto().int_prod().target(), solution);
  DCHECK_EQ(ct_proto().int_prod().exprs_size(), 2);
  const int64_t prod_value =
      ExprValue(ct_proto().int_prod().exprs(0), solution) *
      ExprValue(ct_proto().int_prod().exprs(1), solution);
  return std::abs(target_value - prod_value);
}

// ----- CompiledIntDivConstraint -----

// The violation of an int_div constraint is
//     abs(value(target) - value(expr0) / value(expr1)).
class CompiledIntDivConstraint : public CompiledConstraint {
 public:
  explicit CompiledIntDivConstraint(const ConstraintProto& ct_proto);
  ~CompiledIntDivConstraint() override = default;

  int64_t Violation(absl::Span<const int64_t> solution) override;
};

CompiledIntDivConstraint::CompiledIntDivConstraint(
    const ConstraintProto& ct_proto)
    : CompiledConstraint(ct_proto) {}

int64_t CompiledIntDivConstraint::Violation(
    absl::Span<const int64_t> solution) {
  const int64_t target_value =
      ExprValue(ct_proto().int_div().target(), solution);
  DCHECK_EQ(ct_proto().int_div().exprs_size(), 2);
  const int64_t div_value = ExprValue(ct_proto().int_div().exprs(0), solution) /
                            ExprValue(ct_proto().int_div().exprs(1), solution);
  return std::abs(target_value - div_value);
}

// ----- CompiledAllDiffConstraint -----

// The violation of a all_diff is the number of unordered pairs of expressions
// with the same value.
class CompiledAllDiffConstraint : public CompiledConstraint {
 public:
  explicit CompiledAllDiffConstraint(const ConstraintProto& ct_proto);
  ~CompiledAllDiffConstraint() override = default;

  int64_t Violation(absl::Span<const int64_t> solution) override;

 private:
  std::vector<int64_t> values_;
};

CompiledAllDiffConstraint::CompiledAllDiffConstraint(
    const ConstraintProto& ct_proto)
    : CompiledConstraint(ct_proto) {}

int64_t CompiledAllDiffConstraint::Violation(
    absl::Span<const int64_t> solution) {
  values_.clear();
  for (const LinearExpressionProto& expr : ct_proto().all_diff().exprs()) {
    values_.push_back(ExprValue(expr, solution));
  }
  std::sort(values_.begin(), values_.end());

  int64_t value = values_[0];
  int counter = 1;
  int64_t violation = 0;
  for (int i = 1; i < values_.size(); ++i) {
    const int64_t new_value = values_[i];
    if (new_value == value) {
      counter++;
    } else {
      violation += counter * (counter - 1) / 2;
      counter = 1;
      value = new_value;
    }
  }
  violation += counter * (counter - 1) / 2;
  return violation;
}

// ----- CompiledNoOverlapConstraint -----

namespace {
int64_t ComputeOverloadArea(
    absl::Span<const int> intervals,
    absl::Span<const LinearExpressionProto* const> demands,
    const CpModelProto& cp_model, const absl::Span<const int64_t> solution,
    int64_t max_capacity, std::vector<std::pair<int64_t, int64_t>>& events) {
  events.clear();
  for (int i = 0; i < intervals.size(); ++i) {
    const int i_var = intervals[i];
    const ConstraintProto& interval_proto = cp_model.constraints(i_var);
    if (!interval_proto.enforcement_literal().empty() &&
        !LiteralValue(interval_proto.enforcement_literal(0), solution)) {
      continue;
    }

    const int64_t demand =
        demands.empty() ? 1 : ExprValue(*demands[i], solution);
    if (demand == 0) continue;

    const int64_t start =
        ExprValue(interval_proto.interval().start(), solution);
    const int64_t size = ExprValue(interval_proto.interval().size(), solution);
    const int64_t end = ExprValue(interval_proto.interval().end(), solution);
    const int64_t max_end = std::max(start + size, end);
    if (start >= max_end) continue;

    events.push_back({start, demand});
    events.push_back({max_end, -demand});
  }

  if (events.empty()) return 0;
  std::sort(events.begin(), events.end(),
            [](const std::pair<int64_t, int64_t>& e1,
               const std::pair<int64_t, int64_t>& e2) {
              return e1.first < e2.first ||
                     (e1.first == e2.first && e1.second < e2.second);
            });
  int64_t overload = 0;
  int64_t current_load = 0;
  int64_t previous_load = 0;
  int64_t previous_time = events.front().first;

  for (int i = 0; i < events.size();) {
    if (previous_load > max_capacity) {
      overload =
          CapAdd(overload, CapProd(CapSub(previous_load, max_capacity),
                                   CapSub(events[i].first, previous_time)));
    }
    const int start_index = i;
    while (i < events.size() && events[i].first == events[start_index].first) {
      current_load += events[i].second;
      i++;
    }
    CHECK_GE(current_load, 0);
    previous_time = events[start_index].first;
    previous_load = current_load;
  }
  CHECK_EQ(current_load, 0);
  return overload;
}

}  // namespace

// The violation of a no_overlap is the sum of overloads over time.
class CompiledNoOverlapConstraint : public CompiledConstraint {
 public:
  explicit CompiledNoOverlapConstraint(const ConstraintProto& ct_proto,
                                       const CpModelProto& cp_model);
  ~CompiledNoOverlapConstraint() override = default;

  int64_t Violation(absl::Span<const int64_t> solution) override;

 private:
  const CpModelProto& cp_model_;
  std::vector<std::pair<int64_t, int64_t>> events_;
};

CompiledNoOverlapConstraint::CompiledNoOverlapConstraint(
    const ConstraintProto& ct_proto, const CpModelProto& cp_model)
    : CompiledConstraint(ct_proto), cp_model_(cp_model) {}

int64_t CompiledNoOverlapConstraint::Violation(
    absl::Span<const int64_t> solution) {
  return ComputeOverloadArea(ct_proto().no_overlap().intervals(), {}, cp_model_,
                             solution, 1, events_);
}

// ----- CompiledCumulativeConstraint -----

// The violation of a cumulative is the sum of overloads over time.
class CompiledCumulativeConstraint : public CompiledConstraint {
 public:
  explicit CompiledCumulativeConstraint(const ConstraintProto& ct_proto,
                                        const CpModelProto& cp_model);
  ~CompiledCumulativeConstraint() override = default;

  int64_t Violation(absl::Span<const int64_t> solution) override;

 private:
  const CpModelProto& cp_model_;
  std::vector<std::pair<int64_t, int64_t>> events_;
};

CompiledCumulativeConstraint::CompiledCumulativeConstraint(
    const ConstraintProto& ct_proto, const CpModelProto& cp_model)
    : CompiledConstraint(ct_proto), cp_model_(cp_model) {}

int64_t CompiledCumulativeConstraint::Violation(
    absl::Span<const int64_t> solution) {
  return ComputeOverloadArea(
      ct_proto().cumulative().intervals(), ct_proto().cumulative().demands(),
      cp_model_, solution,
      ExprValue(ct_proto().cumulative().capacity(), solution), events_);
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
    for (const int var : UsedVariables(constraints_[ct_index]->ct_proto())) {
      var_to_constraint_graph_[var].push_back(ct_index);
    }
    for (const int i_var : UsedIntervals(constraints_[ct_index]->ct_proto())) {
      const ConstraintProto& interval_proto = model_.constraints(i_var);
      for (const int var : UsedVariables(interval_proto)) {
        var_to_constraint_graph_[var].push_back(ct_index);
      }
    }
  }

  // Remove duplicates.
  for (std::vector<int>& deps : var_to_constraint_graph_) {
    gtl::STLSortAndRemoveDuplicates(&deps);
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
        // Encoding using enforcement literal is sligthly more efficient.
        const int ct_index = linear_evaluator_.NewConstraint(Domain(1, 1));
        for (const int lit : ct.enforcement_literal()) {
          linear_evaluator_.AddEnforcementLiteral(ct_index, lit);
        }
        for (const int lit : ct.bool_or().literals()) {
          linear_evaluator_.AddEnforcementLiteral(ct_index, NegatedRef(lit));
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
      case ConstraintProto::ConstraintCase::kAllDiff: {
        constraints_.emplace_back(new CompiledAllDiffConstraint(ct));
        break;
      }
      case ConstraintProto::ConstraintCase::kLinMax: {
        // This constraint is split into linear precedences and its max
        // maintenance.
        const LinearExpressionProto& target = ct.lin_max().target();
        for (const LinearExpressionProto& expr : ct.lin_max().exprs()) {
          const int64_t max_value =
              ExprMax(target, model_) - ExprMin(expr, model_);
          const int precedence_index =
              linear_evaluator_.NewConstraint({0, max_value});
          linear_evaluator_.AddLinearExpression(precedence_index, target, 1);
          linear_evaluator_.AddLinearExpression(precedence_index, expr, -1);
        }

        // Penalty when target > all expressions.
        constraints_.emplace_back(new CompiledLinMaxConstraint(ct));
        break;
      }
      case ConstraintProto::ConstraintCase::kIntProd: {
        constraints_.emplace_back(new CompiledIntProdConstraint(ct));
        break;
      }
      case ConstraintProto::ConstraintCase::kIntDiv: {
        constraints_.emplace_back(new CompiledIntDivConstraint(ct));
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
      case ConstraintProto::ConstraintCase::kNoOverlap: {
        CompiledNoOverlapConstraint* no_overlap =
            new CompiledNoOverlapConstraint(ct, model_);
        constraints_.emplace_back(no_overlap);
        break;
      }
      case ConstraintProto::ConstraintCase::kCumulative: {
        constraints_.emplace_back(new CompiledCumulativeConstraint(ct, model_));
        break;
      }
      default:
        VLOG(1) << "Not implemented: " << ct.constraint_case();
        model_is_supported_ = false;
        break;
    }
  }

  // Make sure we have access to the data in an efficient way.
  linear_evaluator_.PrecomputeCompactView();
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
