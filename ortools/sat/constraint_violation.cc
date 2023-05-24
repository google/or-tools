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
#include <memory>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/stl_util.h"
#include "ortools/graph/strongly_connected_components.h"
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
  is_violated_.push_back(false);
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

void LinearIncrementalEvaluator::AddLiteral(int ct_index, int lit,
                                            int64_t coeff) {
  DCHECK(creation_phase_);
  if (RefIsPositive(lit)) {
    AddTerm(ct_index, lit, coeff, 0);
  } else {
    AddTerm(ct_index, PositiveRef(lit), -coeff, coeff);
  }
}

void LinearIncrementalEvaluator::AddTerm(int ct_index, int var, int64_t coeff,
                                         int64_t offset) {
  DCHECK(creation_phase_);
  DCHECK_GE(var, 0);
  if (coeff == 0) return;

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
    if (expr.coeffs(i) * multiplier == 0) continue;
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
  in_last_affected_variables_.resize(columns_.size(), false);
  num_false_enforcement_.assign(num_constraints_, 0);

  // Update these numbers for all columns.
  for (int var = 0; var < columns_.size(); ++var) {
    const SpanData& data = columns_[var];
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
    is_violated_[c] = Violation(c) > 0;
  }
}

// Note that the code assumes that a column has no duplicates ct indices.
void LinearIncrementalEvaluator::Update(int var, int64_t delta) {
  DCHECK(!creation_phase_);
  DCHECK_NE(delta, 0);
  if (var >= columns_.size()) return;

  const SpanData& data = columns_[var];
  int i = data.start;
  for (int k = 0; k < data.num_pos_literal; ++k, ++i) {
    const int c = ct_buffer_[i];
    if (delta == 1) {
      num_false_enforcement_[c]--;
      DCHECK_GE(num_false_enforcement_[c], 0);
    } else {
      num_false_enforcement_[c]++;
    }
    is_violated_[c] = Violation(c) > 0;
  }
  for (int k = 0; k < data.num_neg_literal; ++k, ++i) {
    const int c = ct_buffer_[i];
    if (delta == -1) {
      num_false_enforcement_[c]--;
      DCHECK_GE(num_false_enforcement_[c], 0);
    } else {
      num_false_enforcement_[c]++;
    }
    is_violated_[c] = Violation(c) > 0;
  }
  int j = data.linear_start;
  for (int k = 0; k < data.num_linear_entries; ++k, ++i, ++j) {
    const int c = ct_buffer_[i];
    const int64_t coeff = coeff_buffer_[j];
    activities_[c] += coeff * delta;
    distances_[c] = domains_[c].Distance(activities_[c]);
    is_violated_[c] = Violation(c) > 0;
  }
}

void LinearIncrementalEvaluator::ClearAffectedVariables() {
  in_last_affected_variables_.resize(columns_.size(), false);
  for (const int var : last_affected_variables_) {
    in_last_affected_variables_[var] = false;
  }
  last_affected_variables_.clear();
}

void LinearIncrementalEvaluator::UpdateScoreOnWeightUpdate(
    int c, double weight_delta, absl::Span<const int64_t> solution,
    absl::Span<const int64_t> jump_values, absl::Span<double> jump_scores) {
  if (c >= rows_.size()) return;

  DCHECK_EQ(num_false_enforcement_[c], 0);
  const SpanData& data = rows_[c];

  // Update enforcement part, all change are 0 -> 1 transition and decrease
  // by this.
  const double enforcement_change =
      weight_delta * static_cast<double>(distances_[c]);
  if (enforcement_change != 0.0) {
    int i = data.start;
    const int end = data.num_pos_literal + data.num_neg_literal;
    dtime_ += end;
    for (int k = 0; k < end; ++k, ++i) {
      const int var = row_var_buffer_[i];
      jump_scores[var] -= enforcement_change;
      if (!in_last_affected_variables_[var]) {
        last_affected_variables_.push_back(var);
      }
    }
  }

  // Update linear part.
  int i = data.start + data.num_pos_literal + data.num_neg_literal;
  int j = data.linear_start;
  dtime_ += 2 * data.num_linear_entries;
  const int64_t old_distance = distances_[c];
  for (int k = 0; k < data.num_linear_entries; ++k, ++i, ++j) {
    const int var = row_var_buffer_[i];
    const int64_t coeff = row_coeff_buffer_[j];
    const int64_t delta = jump_values[var] - solution[var];
    const int64_t new_distance =
        domains_[c].Distance(activities_[c] + coeff * delta);
    jump_scores[var] +=
        weight_delta * static_cast<double>(new_distance - old_distance);
    if (!in_last_affected_variables_[var]) {
      last_affected_variables_.push_back(var);
    }
  }
}

void LinearIncrementalEvaluator::UpdateScoreOnNewlyEnforced(
    int c, double weight, absl::Span<const int64_t> solution,
    absl::Span<const int64_t> jump_values, absl::Span<double> jump_scores) {
  const SpanData& data = rows_[c];

  // Everyone else had a zero cost transition that now become enforced ->
  // unenforced.
  const double weight_time_violation =
      weight * static_cast<double>(distances_[c]);
  if (weight_time_violation > 0.0) {
    int i = data.start;
    const int end = data.num_pos_literal + data.num_neg_literal;
    dtime_ += end;
    for (int k = 0; k < end; ++k, ++i) {
      const int var = row_var_buffer_[i];
      jump_scores[var] -= weight_time_violation;
      if (!in_last_affected_variables_[var]) {
        last_affected_variables_.push_back(var);
      }
    }
  }

  // Update linear part! It was zero and is now a diff.
  {
    int i = data.start + data.num_pos_literal + data.num_neg_literal;
    int j = data.linear_start;
    dtime_ += 2 * data.num_linear_entries;
    const int64_t old_distance = distances_[c];
    for (int k = 0; k < data.num_linear_entries; ++k, ++i, ++j) {
      const int var = row_var_buffer_[i];
      const int64_t coeff = row_coeff_buffer_[j];
      const int64_t delta = jump_values[var] - solution[var];
      const int64_t new_distance =
          domains_[c].Distance(activities_[c] + coeff * delta);
      jump_scores[var] +=
          weight * static_cast<double>(new_distance - old_distance);
      if (!in_last_affected_variables_[var]) {
        last_affected_variables_.push_back(var);
      }
    }
  }
}

void LinearIncrementalEvaluator::UpdateScoreOnNewlyUnenforced(
    int c, double weight, absl::Span<const int64_t> solution,
    absl::Span<const int64_t> jump_values, absl::Span<double> jump_scores) {
  const SpanData& data = rows_[c];

  // Everyone else had a enforced -> unenforced transition that now become zero.
  const double weight_time_violation =
      weight * static_cast<double>(distances_[c]);
  if (weight_time_violation > 0.0) {
    int i = data.start;
    const int end = data.num_pos_literal + data.num_neg_literal;
    dtime_ += end;
    for (int k = 0; k < end; ++k, ++i) {
      const int var = row_var_buffer_[i];
      jump_scores[var] += weight_time_violation;
      if (!in_last_affected_variables_[var]) {
        last_affected_variables_.push_back(var);
      }
    }
  }

  // Update linear part! It had a diff and is now zero.
  {
    int i = data.start + data.num_pos_literal + data.num_neg_literal;
    int j = data.linear_start;
    dtime_ += 2 * data.num_linear_entries;
    const int64_t old_distance = distances_[c];
    for (int k = 0; k < data.num_linear_entries; ++k, ++i, ++j) {
      const int var = row_var_buffer_[i];
      const int64_t coeff = row_coeff_buffer_[j];
      const int64_t delta = jump_values[var] - solution[var];
      const int64_t new_distance =
          domains_[c].Distance(activities_[c] + coeff * delta);
      jump_scores[var] -=
          weight * static_cast<double>(new_distance - old_distance);
      if (!in_last_affected_variables_[var]) {
        last_affected_variables_.push_back(var);
      }
    }
  }
}

// We just need to modifie the old/new transition that decrease the number of
// enforcement literal at false.
void LinearIncrementalEvaluator::UpdateScoreOfEnforcementIncrease(
    int c, double score_change, absl::Span<const int64_t> jump_values,
    absl::Span<double> jump_scores) {
  if (score_change == 0.0) return;

  const SpanData& data = rows_[c];
  int i = data.start;
  dtime_ += data.num_pos_literal;
  for (int k = 0; k < data.num_pos_literal; ++k, ++i) {
    const int var = row_var_buffer_[i];
    if (jump_values[var] == 1) {
      jump_scores[var] += score_change;
      if (!in_last_affected_variables_[var]) {
        last_affected_variables_.push_back(var);
      }
    }
  }
  dtime_ += data.num_neg_literal;
  for (int k = 0; k < data.num_neg_literal; ++k, ++i) {
    const int var = row_var_buffer_[i];
    if (jump_values[var] == 0) {
      jump_scores[var] += score_change;
      if (!in_last_affected_variables_[var]) {
        last_affected_variables_.push_back(var);
      }
    }
  }
}

void LinearIncrementalEvaluator::UpdateScoreOnActivityChange(
    int c, double weight, int64_t activity_delta,
    absl::Span<const int64_t> solution, absl::Span<const int64_t> jump_values,
    absl::Span<double> jump_scores) {
  if (activity_delta == 0) return;
  const SpanData& data = rows_[c];

  // Enforcement is always enforced -> unforced.
  // So it was -weight_time_distance and is now -weight_time_new_distance.
  const double delta =
      -weight * static_cast<double>(
                    domains_[c].Distance(activities_[c] + activity_delta) -
                    distances_[c]);
  if (delta != 0.0) {
    int i = data.start;
    const int end = data.num_pos_literal + data.num_neg_literal;
    dtime_ += end;
    for (int k = 0; k < end; ++k, ++i) {
      const int var = row_var_buffer_[i];
      jump_scores[var] += delta;
      if (!in_last_affected_variables_[var]) {
        last_affected_variables_.push_back(var);
      }
    }
  }

  // Update linear part.
  {
    int i = data.start + data.num_pos_literal + data.num_neg_literal;
    int j = data.linear_start;
    dtime_ += 2 * data.num_linear_entries;
    const int64_t old_a_minus_new_a =
        distances_[c] - domains_[c].Distance(activities_[c] + activity_delta);
    for (int k = 0; k < data.num_linear_entries; ++k, ++i, ++j) {
      const int var = row_var_buffer_[i];
      const int64_t coeff = row_coeff_buffer_[j];
      const int64_t delta = jump_values[var] - solution[var];
      const int64_t old_b =
          domains_[c].Distance(activities_[c] + coeff * delta);
      const int64_t new_b =
          domains_[c].Distance(activities_[c] + activity_delta + coeff * delta);

      // The old score was:
      //   weight * static_cast<double>(old_b - old_a);
      // the new score is
      //   weight * static_cast<double>(new_b - new_a); so the diff is:
      jump_scores[var] +=
          weight * static_cast<double>(old_a_minus_new_a + new_b - old_b);
      if (!in_last_affected_variables_[var]) {
        last_affected_variables_.push_back(var);
      }
    }
  }
}

void LinearIncrementalEvaluator::UpdateVariableAndScores(
    int var, int64_t delta, absl::Span<const int64_t> solution,
    absl::Span<const double> weights, absl::Span<const int64_t> jump_values,
    absl::Span<double> jump_scores) {
  DCHECK(!creation_phase_);
  DCHECK_NE(delta, 0);
  if (var >= columns_.size()) return;

  const SpanData& data = columns_[var];
  int i = data.start;
  for (int k = 0; k < data.num_pos_literal; ++k, ++i) {
    const int c = ct_buffer_[i];
    if (delta == 1) {
      num_false_enforcement_[c]--;
      DCHECK_GE(num_false_enforcement_[c], 0);
      if (num_false_enforcement_[c] == 0) {
        UpdateScoreOnNewlyEnforced(c, weights[c], solution, jump_values,
                                   jump_scores);
      } else if (num_false_enforcement_[c] == 1) {
        const double enforcement_change =
            weights[c] * static_cast<double>(distances_[c]);
        UpdateScoreOfEnforcementIncrease(c, enforcement_change, jump_values,
                                         jump_scores);
      }
    } else {
      num_false_enforcement_[c]++;
      if (num_false_enforcement_[c] == 1) {
        UpdateScoreOnNewlyUnenforced(c, weights[c], solution, jump_values,
                                     jump_scores);
      } else if (num_false_enforcement_[c] == 2) {
        const double enforcement_change =
            weights[c] * static_cast<double>(distances_[c]);
        UpdateScoreOfEnforcementIncrease(c, -enforcement_change, jump_values,
                                         jump_scores);
      }
    }
    is_violated_[c] = Violation(c) > 0;
  }
  for (int k = 0; k < data.num_neg_literal; ++k, ++i) {
    const int c = ct_buffer_[i];
    if (delta == -1) {
      num_false_enforcement_[c]--;
      DCHECK_GE(num_false_enforcement_[c], 0);
      if (num_false_enforcement_[c] == 0) {
        UpdateScoreOnNewlyEnforced(c, weights[c], solution, jump_values,
                                   jump_scores);
      } else if (num_false_enforcement_[c] == 1) {
        const double enforcement_change =
            weights[c] * static_cast<double>(distances_[c]);
        UpdateScoreOfEnforcementIncrease(c, enforcement_change, jump_values,
                                         jump_scores);
      }
    } else {
      num_false_enforcement_[c]++;
      if (num_false_enforcement_[c] == 1) {
        UpdateScoreOnNewlyUnenforced(c, weights[c], solution, jump_values,
                                     jump_scores);
      } else if (num_false_enforcement_[c] == 2) {
        const double enforcement_change =
            weights[c] * static_cast<double>(distances_[c]);
        UpdateScoreOfEnforcementIncrease(c, -enforcement_change, jump_values,
                                         jump_scores);
      }
    }
    is_violated_[c] = Violation(c) > 0;
  }
  int j = data.linear_start;
  for (int k = 0; k < data.num_linear_entries; ++k, ++i, ++j) {
    const int c = ct_buffer_[i];
    const int64_t coeff = coeff_buffer_[j];

    if (num_false_enforcement_[c] == 1) {
      // Only the 1 -> 0 are impacted.
      // This is the same as the 1->2 transition, but the old 1->0 needs to
      // be changed from - weight * distance to - weight * new_distance.
      const int64_t new_distance =
          domains_[c].Distance(activities_[c] + coeff * delta);
      if (new_distance != distances_[c]) {
        UpdateScoreOfEnforcementIncrease(
            c, -weights[c] * static_cast<double>(distances_[c] - new_distance),
            jump_values, jump_scores);
      }
    } else if (num_false_enforcement_[c] == 0) {
      UpdateScoreOnActivityChange(c, weights[c], coeff * delta, solution,
                                  jump_values, jump_scores);
    }

    activities_[c] += coeff * delta;
    distances_[c] = domains_[c].Distance(activities_[c]);
    is_violated_[c] = Violation(c) > 0;
  }
}

int64_t LinearIncrementalEvaluator::Activity(int c) const {
  return activities_[c];
}

int64_t LinearIncrementalEvaluator::Violation(int c) const {
  return num_false_enforcement_[c] > 0 ? 0 : distances_[c];
}

bool LinearIncrementalEvaluator::IsViolated(int c) const {
  DCHECK_EQ(is_violated_[c], Violation(c) > 0);
  return is_violated_[c];
}

bool LinearIncrementalEvaluator::ReduceBounds(int c, int64_t lb, int64_t ub) {
  if (domains_[c].Min() >= lb && domains_[c].Max() <= ub) return false;
  domains_[c] = domains_[c].IntersectionWith(Domain(lb, ub));
  distances_[c] = domains_[c].Distance(activities_[c]);
  return true;
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
  const SpanData& data = columns_[var];

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
  dtime_ += 2 * data.num_linear_entries;
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

  const SpanData& data = columns_[var];
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

  // Compute the total size.
  // Note that at this point the constraint indices are not "encoded" yet.
  int total_size = 0;
  int total_linear_size = 0;
  tmp_row_sizes_.assign(num_constraints_, 0);
  tmp_row_num_positive_literals_.assign(num_constraints_, 0);
  tmp_row_num_negative_literals_.assign(num_constraints_, 0);
  tmp_row_num_linear_entries_.assign(num_constraints_, 0);
  for (const auto& column : literal_entries_) {
    total_size += column.size();
    for (const auto [c, is_positive] : column) {
      tmp_row_sizes_[c]++;
      if (is_positive) {
        tmp_row_num_positive_literals_[c]++;
      } else {
        tmp_row_num_negative_literals_[c]++;
      }
    }
  }
  for (const auto& column : var_entries_) {
    total_size += column.size();
    total_linear_size += column.size();
    for (const auto& entry : column) {
      tmp_row_sizes_[entry.ct_index]++;
      tmp_row_num_linear_entries_[entry.ct_index]++;
    }
  }

  // Compactify for faster WeightedViolationDelta().
  ct_buffer_.reserve(total_size);
  coeff_buffer_.reserve(total_linear_size);
  columns_.resize(std::max(literal_entries_.size(), var_entries_.size()));
  for (int var = 0; var < columns_.size(); ++var) {
    columns_[var].start = static_cast<int>(ct_buffer_.size());
    columns_[var].linear_start = static_cast<int>(coeff_buffer_.size());
    if (var < literal_entries_.size()) {
      for (const auto [c, is_positive] : literal_entries_[var]) {
        if (is_positive) {
          columns_[var].num_pos_literal++;
          ct_buffer_.push_back(c);
        }
      }
      for (const auto [c, is_positive] : literal_entries_[var]) {
        if (!is_positive) {
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

  // We do not need var_entries_ or literal_entries_ anymore.
  //
  // TODO(user): We could delete them before. But at the time of this
  // optimization, I didn't want to change the behavior of the algorithm at all.
  gtl::STLClearObject(&var_entries_);
  gtl::STLClearObject(&literal_entries_);

  // Initialize the SpanData.
  // Transform tmp_row_sizes_ to starts in the row_var_buffer_.
  // Transform tmp_row_num_linear_entries_ to starts in the row_coeff_buffer_.
  int offset = 0;
  int linear_offset = 0;
  rows_.resize(num_constraints_);
  for (int c = 0; c < num_constraints_; ++c) {
    rows_[c].num_pos_literal = tmp_row_num_positive_literals_[c];
    rows_[c].num_neg_literal = tmp_row_num_negative_literals_[c];
    rows_[c].num_linear_entries = tmp_row_num_linear_entries_[c];

    rows_[c].start = offset;
    offset += tmp_row_sizes_[c];
    tmp_row_sizes_[c] = rows_[c].start;

    rows_[c].linear_start = linear_offset;
    linear_offset += tmp_row_num_linear_entries_[c];
    tmp_row_num_linear_entries_[c] = rows_[c].linear_start;
  }
  DCHECK_EQ(offset, total_size);
  DCHECK_EQ(linear_offset, total_linear_size);

  // Copy data.
  row_var_buffer_.resize(total_size);
  row_coeff_buffer_.resize(total_linear_size);
  for (int var = 0; var < columns_.size(); ++var) {
    const SpanData& data = columns_[var];
    int i = data.start;
    for (int k = 0; k < data.num_pos_literal; ++i, ++k) {
      const int c = ct_buffer_[i];
      row_var_buffer_[tmp_row_sizes_[c]++] = var;
    }
  }
  for (int var = 0; var < columns_.size(); ++var) {
    const SpanData& data = columns_[var];
    int i = data.start + data.num_pos_literal;
    for (int k = 0; k < data.num_neg_literal; ++i, ++k) {
      const int c = ct_buffer_[i];
      row_var_buffer_[tmp_row_sizes_[c]++] = var;
    }
  }
  for (int var = 0; var < columns_.size(); ++var) {
    const SpanData& data = columns_[var];
    int i = data.start + data.num_pos_literal + data.num_neg_literal;
    int j = data.linear_start;
    for (int k = 0; k < data.num_linear_entries; ++i, ++j, ++k) {
      const int c = ct_buffer_[i];
      row_var_buffer_[tmp_row_sizes_[c]++] = var;
      row_coeff_buffer_[tmp_row_num_linear_entries_[c]++] = coeff_buffer_[j];
    }
  }

  cached_deltas_.assign(columns_.size(), 0);
  cached_scores_.assign(columns_.size(), 0);
}

bool LinearIncrementalEvaluator::ViolationChangeIsConvex(int var) const {
  for (const int c : VarToConstraints(var)) {
    if (domains_[c].intervals().size() > 2) return false;
  }
  return true;
}

// ----- CompiledConstraint -----

CompiledConstraint::CompiledConstraint(const ConstraintProto& ct_proto)
    : ct_proto_(ct_proto) {}

void CompiledConstraint::InitializeViolation(
    absl::Span<const int64_t> solution) {
  violation_ = ComputeViolation(solution);
}

void CompiledConstraint::PerformMove(
    int var, int64_t old_value,
    absl::Span<const int64_t> solution_with_new_value) {
  violation_ += ViolationDelta(var, old_value, solution_with_new_value);
}

int64_t CompiledConstraint::ViolationDelta(int /*var*/, int64_t /*old_value*/,
                                           absl::Span<const int64_t> solution) {
  return ComputeViolation(solution) - violation_;
}

// ----- CompiledBoolXorConstraint -----

CompiledBoolXorConstraint::CompiledBoolXorConstraint(
    const ConstraintProto& ct_proto)
    : CompiledConstraint(ct_proto) {}

int64_t CompiledBoolXorConstraint::ComputeViolation(
    absl::Span<const int64_t> solution) {
  int64_t sum_of_literals = 0;
  for (const int lit : ct_proto().bool_xor().literals()) {
    sum_of_literals += LiteralValue(lit, solution);
  }
  return 1 - (sum_of_literals % 2);
}

int64_t CompiledBoolXorConstraint::ViolationDelta(
    int /*var*/, int64_t /*old_value*/,
    absl::Span<const int64_t> /*solution_with_new_value*/) {
  return violation() == 0 ? 1 : -1;
}

// ----- CompiledLinMaxConstraint -----

CompiledLinMaxConstraint::CompiledLinMaxConstraint(
    const ConstraintProto& ct_proto)
    : CompiledConstraint(ct_proto) {}

int64_t CompiledLinMaxConstraint::ComputeViolation(
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

CompiledIntProdConstraint::CompiledIntProdConstraint(
    const ConstraintProto& ct_proto)
    : CompiledConstraint(ct_proto) {}

int64_t CompiledIntProdConstraint::ComputeViolation(
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

CompiledIntDivConstraint::CompiledIntDivConstraint(
    const ConstraintProto& ct_proto)
    : CompiledConstraint(ct_proto) {}

int64_t CompiledIntDivConstraint::ComputeViolation(
    absl::Span<const int64_t> solution) {
  const int64_t target_value =
      ExprValue(ct_proto().int_div().target(), solution);
  DCHECK_EQ(ct_proto().int_div().exprs_size(), 2);
  const int64_t div_value = ExprValue(ct_proto().int_div().exprs(0), solution) /
                            ExprValue(ct_proto().int_div().exprs(1), solution);
  return std::abs(target_value - div_value);
}

// ----- CompiledAllDiffConstraint -----

CompiledAllDiffConstraint::CompiledAllDiffConstraint(
    const ConstraintProto& ct_proto)
    : CompiledConstraint(ct_proto) {}

int64_t CompiledAllDiffConstraint::ComputeViolation(
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

    events.emplace_back(start, demand);
    events.emplace_back(max_end, -demand);
  }

  if (events.empty()) return 0;
  std::sort(events.begin(), events.end(),
            [](const std::pair<int64_t, int64_t>& e1,
               const std::pair<int64_t, int64_t>& e2) {
              return e1.first < e2.first;
            });

  int64_t overload = 0;
  int64_t current_load = 0;
  int64_t previous_time = events.front().first;
  for (int i = 0; i < events.size();) {
    // At this point, current_load is the load at previous_time.
    const int64_t time = events[i].first;
    if (current_load > max_capacity) {
      overload = CapAdd(
          overload, CapProd(current_load - max_capacity, time - previous_time));
    }
    while (i < events.size() && events[i].first == time) {
      current_load += events[i].second;
      i++;
    }
    DCHECK_GE(current_load, 0);
    previous_time = time;
  }
  DCHECK_EQ(current_load, 0);
  return overload;
}

}  // namespace

CompiledNoOverlapConstraint::CompiledNoOverlapConstraint(
    const ConstraintProto& ct_proto, const CpModelProto& cp_model)
    : CompiledConstraint(ct_proto), cp_model_(cp_model) {}

int64_t CompiledNoOverlapConstraint::ComputeViolation(
    absl::Span<const int64_t> solution) {
  return ComputeOverloadArea(ct_proto().no_overlap().intervals(), {}, cp_model_,
                             solution, 1, events_);
}

// ----- CompiledCumulativeConstraint -----

CompiledCumulativeConstraint::CompiledCumulativeConstraint(
    const ConstraintProto& ct_proto, const CpModelProto& cp_model)
    : CompiledConstraint(ct_proto), cp_model_(cp_model) {}

int64_t CompiledCumulativeConstraint::ComputeViolation(
    absl::Span<const int64_t> solution) {
  return ComputeOverloadArea(
      ct_proto().cumulative().intervals(), ct_proto().cumulative().demands(),
      cp_model_, solution,
      ExprValue(ct_proto().cumulative().capacity(), solution), events_);
}

// ----- CompiledCircuitConstraint -----

// The violation of a circuit has three parts:
//   1. Flow imbalance, maintained by the linear part.
//   2. The number of non-skipped SCCs in the graph minus 1.
//   3. The number of non-skipped SCCs that cannot be reached from any other
//      component minus 1.
//
// #3 is not necessary for correctness, but makes the function much smoother.
//
// The only difference between single and multi circuit is flow balance at the
// depot, so we use the same compiled constraint for both.
class CompiledCircuitConstraint : public CompiledConstraint {
 public:
  explicit CompiledCircuitConstraint(const ConstraintProto& ct_proto);
  ~CompiledCircuitConstraint() override = default;

  int64_t ComputeViolation(absl::Span<const int64_t> solution) override;

 private:
  struct SccOutput {
    void emplace_back(const int* start, const int* end);
    void reset(int num_nodes);

    int num_components = 0;
    std::vector<bool> skipped;
    std::vector<int> root;
  };
  void UpdateGraph(absl::Span<const int64_t> solution);
  absl::Span<const int> literals_;
  absl::Span<const int> tails_;
  absl::Span<const int> heads_;
  // Stores the currently active arcs per tail node.
  std::vector<std::vector<int>> graph_;
  SccOutput sccs_;
  std::vector<bool> has_in_arc_;
  StronglyConnectedComponentsFinder<int, std::vector<std::vector<int>>,
                                    SccOutput>
      scc_finder_;
};

void CompiledCircuitConstraint::SccOutput::emplace_back(int const* start,
                                                        int const* end) {
  const int root_node = *start;
  const int size = end - start;
  if (size == 1) {
    skipped[root_node] = true;
  } else {
    ++num_components;
  }
  for (; start != end; ++start) {
    root[*start] = root_node;
  }
}
void CompiledCircuitConstraint::SccOutput::reset(int num_nodes) {
  num_components = 0;
  root.clear();
  root.resize(num_nodes);
  skipped.clear();
  skipped.resize(num_nodes);
}

CompiledCircuitConstraint::CompiledCircuitConstraint(
    const ConstraintProto& ct_proto)
    : CompiledConstraint(ct_proto) {
  const bool routes = ct_proto.has_routes();
  tails_ = routes ? ct_proto.routes().tails() : ct_proto.circuit().tails();
  heads_ = absl::MakeConstSpan(routes ? ct_proto.routes().heads()
                                      : ct_proto.circuit().heads());
  literals_ = absl::MakeConstSpan(routes ? ct_proto.routes().literals()
                                         : ct_proto.circuit().literals());
  graph_.resize(*absl::c_max_element(tails_) + 1);
}

void CompiledCircuitConstraint::UpdateGraph(
    absl::Span<const int64_t> solution) {
  for (std::vector<int>& edges : graph_) {
    edges.clear();
  }
  for (int i = 0; i < tails_.size(); ++i) {
    if (!LiteralValue(literals_[i], solution)) continue;
    graph_[tails_[i]].push_back(heads_[i]);
  }
}
int64_t CompiledCircuitConstraint::ComputeViolation(
    absl::Span<const int64_t> solution) {
  const int num_nodes = graph_.size();
  sccs_.reset(num_nodes);
  UpdateGraph(solution);
  scc_finder_.FindStronglyConnectedComponents(num_nodes, graph_, &sccs_);
  // Skipping all nodes causes off-by-one errors below, so it's simpler to
  // handle explicitly.
  if (sccs_.num_components == 0) return 0;
  // Count the number of SCCs that have inbound cross-component arcs
  // as a smoother measure of progress towards strong connectivity.
  int num_half_connected_components = 0;
  has_in_arc_.clear();
  has_in_arc_.resize(num_nodes, false);
  for (int tail = 0; tail < graph_.size(); ++tail) {
    if (sccs_.skipped[tail]) continue;
    for (const int head : graph_[tail]) {
      const int head_root = sccs_.root[head];
      if (sccs_.root[tail] == head_root) continue;
      if (has_in_arc_[head_root]) continue;
      if (sccs_.skipped[head_root]) continue;
      has_in_arc_[head_root] = true;
      ++num_half_connected_components;
    }
  }
  const int64_t violation = sccs_.num_components - 1 + sccs_.num_components -
                            num_half_connected_components - 1 +
                            (ct_proto().has_routes() ? sccs_.skipped[0] : 0);
  VLOG(2) << "#SCCs=" << sccs_.num_components << " #nodes=" << num_nodes
          << " #half_connected_components=" << num_half_connected_components
          << " violation=" << violation;
  return violation;
}

void AddCircuitFlowConstraints(LinearIncrementalEvaluator& linear_evaluator,
                               const ConstraintProto& ct_proto) {
  const bool routes = ct_proto.has_routes();
  auto heads = routes ? ct_proto.routes().heads() : ct_proto.circuit().heads();
  auto tails = routes ? ct_proto.routes().tails() : ct_proto.circuit().tails();
  auto literals =
      routes ? ct_proto.routes().literals() : ct_proto.circuit().literals();

  std::vector<std::vector<int>> inflow_lits;
  std::vector<std::vector<int>> outflow_lits;
  for (int i = 0; i < heads.size(); ++i) {
    if (heads[i] >= inflow_lits.size()) {
      inflow_lits.resize(heads[i] + 1);
    }
    inflow_lits[heads[i]].push_back(literals[i]);
    if (tails[i] >= outflow_lits.size()) {
      outflow_lits.resize(tails[i] + 1);
    }
    outflow_lits[tails[i]].push_back(literals[i]);
  }
  if (routes) {
    const int depot_net_flow = linear_evaluator.NewConstraint({0, 0});
    for (const int lit : inflow_lits[0]) {
      linear_evaluator.AddLiteral(depot_net_flow, lit, 1);
    }
    for (const int lit : outflow_lits[0]) {
      linear_evaluator.AddLiteral(depot_net_flow, lit, -1);
    }
  }
  for (int i = routes ? 1 : 0; i < inflow_lits.size(); ++i) {
    const int inflow_ct = linear_evaluator.NewConstraint({1, 1});
    for (const int lit : inflow_lits[i]) {
      linear_evaluator.AddLiteral(inflow_ct, lit);
    }
  }
  for (int i = routes ? 1 : 0; i < outflow_lits.size(); ++i) {
    const int outflow_ct = linear_evaluator.NewConstraint({1, 1});
    for (const int lit : outflow_lits[i]) {
      linear_evaluator.AddLiteral(outflow_ct, lit);
    }
  }
}

// ----- LsEvaluator -----

LsEvaluator::LsEvaluator(const CpModelProto& model) : model_(model) {
  ignored_constraints_.resize(model.constraints_size(), false);
  var_to_constraint_graph_.resize(model_.variables_size());
  jump_value_optimal_.resize(model_.variables_size(), true);
  CompileConstraintsAndObjective();
  BuildVarConstraintGraph();
}

LsEvaluator::LsEvaluator(
    const CpModelProto& model, const std::vector<bool>& ignored_constraints,
    const std::vector<ConstraintProto>& additional_constraints)
    : model_(model),
      ignored_constraints_(ignored_constraints),
      additional_constraints_(additional_constraints) {
  var_to_constraint_graph_.resize(model_.variables_size());
  jump_value_optimal_.resize(model_.variables_size(), true);
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

  // Scan the model to decide if a variable is linked to a convex evaluation.
  jump_value_optimal_.resize(model_.variables_size());
  for (int i = 0; i < model_.variables_size(); ++i) {
    if (!var_to_constraint_graph_[i].empty()) {
      jump_value_optimal_[i] = false;
      continue;
    }

    const IntegerVariableProto& var_proto = model_.variables(i);
    if (var_proto.domain_size() == 2 && var_proto.domain(0) == 0 &&
        var_proto.domain(1) == 1) {
      // Boolean variables violation change is always convex.
      jump_value_optimal_[i] = true;
      continue;
    }

    jump_value_optimal_[i] = linear_evaluator_.ViolationChangeIsConvex(i);
  }
}

void LsEvaluator::CompileOneConstraint(const ConstraintProto& ct) {
  switch (ct.constraint_case()) {
    case ConstraintProto::ConstraintCase::kBoolOr: {
      // Encoding using enforcement literal is slightly more efficient.
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
    case ConstraintProto::ConstraintCase::kBoolXor: {
      constraints_.emplace_back(new CompiledBoolXorConstraint(ct));
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
    case ConstraintProto::ConstraintCase::kCircuit:
    case ConstraintProto::ConstraintCase::kRoutes:
      constraints_.emplace_back(new CompiledCircuitConstraint(ct));
      AddCircuitFlowConstraints(linear_evaluator_, ct);
      break;
    default:
      VLOG(1) << "Not implemented: " << ct.constraint_case();
      model_is_supported_ = false;
      break;
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

  for (int c = 0; c < model_.constraints_size(); ++c) {
    if (ignored_constraints_[c]) continue;
    CompileOneConstraint(model_.constraints(c));
  }

  for (const ConstraintProto& ct : additional_constraints_) {
    CompileOneConstraint(ct);
  }

  // Make sure we have access to the data in an efficient way.
  linear_evaluator_.PrecomputeCompactView();
}

bool LsEvaluator::ReduceObjectiveBounds(int64_t lb, int64_t ub) {
  if (!model_.has_objective()) return false;
  return linear_evaluator_.ReduceBounds(/*c=*/0, lb, ub);
}

void LsEvaluator::OverwriteCurrentSolution(absl::Span<const int64_t> solution) {
  current_solution_.assign(solution.begin(), solution.end());
}

void LsEvaluator::ComputeAllViolations() {
  // Linear constraints.
  linear_evaluator_.ComputeInitialActivities(current_solution_);

  // Generic constraints.
  for (const auto& ct : constraints_) {
    ct->InitializeViolation(current_solution_);
  }
}

void LsEvaluator::UpdateAllNonLinearViolations() {
  // Generic constraints.
  for (const auto& ct : constraints_) {
    ct->InitializeViolation(current_solution_);
  }
}

void LsEvaluator::UpdateNonLinearViolations(int var, int64_t new_value) {
  const int64_t old_value = current_solution_[var];
  if (old_value == new_value) return;

  current_solution_[var] = new_value;
  for (const int ct_index : var_to_constraint_graph_[var]) {
    constraints_[ct_index]->PerformMove(var, old_value, current_solution_);
  }
  current_solution_[var] = old_value;
}

void LsEvaluator::UpdateLinearScores(int var, int64_t value,
                                     absl::Span<const double> weights,
                                     absl::Span<const int64_t> jump_values,
                                     absl::Span<double> jump_scores) {
  DCHECK(RefIsPositive(var));
  const int64_t old_value = current_solution_[var];
  if (old_value == value) return;

  linear_evaluator_.UpdateVariableAndScores(var, value - old_value,
                                            current_solution_, weights,
                                            jump_values, jump_scores);
}

void LsEvaluator::UpdateVariableValue(int var, int64_t new_value) {
  current_solution_[var] = new_value;
}

int64_t LsEvaluator::SumOfViolations() {
  int64_t evaluation = 0;

  // Process the linear part.
  for (int i = 0; i < linear_evaluator_.num_constraints(); ++i) {
    evaluation += linear_evaluator_.Violation(i);
    DCHECK_GE(linear_evaluator_.Violation(i), 0);
  }

  // Process the generic constraint part.
  for (const auto& ct : constraints_) {
    evaluation += ct->violation();
    DCHECK_GE(ct->violation(), 0);
  }
  return evaluation;
}

int64_t LsEvaluator::ObjectiveActivity() const {
  DCHECK(model_.has_objective());
  return linear_evaluator_.Activity(/*c=*/0);
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

bool LsEvaluator::IsViolated(int c) const {
  if (c < linear_evaluator_.num_constraints()) {
    return linear_evaluator_.IsViolated(c);
  } else {
    return constraints_[c - linear_evaluator_.num_constraints()]->violation() >
           0;
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

double LsEvaluator::WeightedNonLinearViolationDelta(
    absl::Span<const double> weights, int var, int64_t delta) const {
  const int64_t old_value = current_solution_[var];
  double violation_delta = 0;
  // We change the mutable solution here, are restore it after the evaluation.
  current_solution_[var] += delta;
  const int num_linear_constraints = linear_evaluator_.num_constraints();
  for (const int ct_index : var_to_constraint_graph_[var]) {
    DCHECK_LT(ct_index, constraints_.size());
    const int64_t delta = constraints_[ct_index]->ViolationDelta(
        var, old_value, current_solution_);
    violation_delta +=
        static_cast<double>(delta) * weights[ct_index + num_linear_constraints];
  }
  // Restore.
  current_solution_[var] -= delta;
  return violation_delta;
}

double LsEvaluator::WeightedViolationDelta(absl::Span<const double> weights,
                                           int var, int64_t delta) const {
  DCHECK_LT(var, current_solution_.size());
  return linear_evaluator_.WeightedViolationDelta(weights, var, delta) +
         WeightedNonLinearViolationDelta(weights, var, delta);
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

  return variables;
}

bool LsEvaluator::VariableOnlyInLinearConstraintWithConvexViolationChange(
    int var) const {
  return jump_value_optimal_[var];
}

}  // namespace sat
}  // namespace operations_research
