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

#include "ortools/sat/constraint_violation.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/stl_util.h"
#include "ortools/graph/strongly_connected_components.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/util.h"
#include "ortools/util/dense_set.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {

namespace {

int64_t ExprValue(const LinearExpressionProto& expr,
                  absl::Span<const int64_t> solution) {
  int64_t result = expr.offset();
  for (int i = 0; i < expr.vars_size(); ++i) {
    result += solution[expr.vars(i)] * expr.coeffs(i);
  }
  return result;
}

LinearExpressionProto ExprDiff(const LinearExpressionProto& a,
                               const LinearExpressionProto& b) {
  LinearExpressionProto result;
  result.set_offset(a.offset() - b.offset());
  result.mutable_vars()->Reserve(a.vars().size() + b.vars().size());
  result.mutable_coeffs()->Reserve(a.vars().size() + b.vars().size());
  for (int i = 0; i < a.vars().size(); ++i) {
    result.add_vars(a.vars(i));
    result.add_coeffs(a.coeffs(i));
  }
  for (int i = 0; i < b.vars().size(); ++i) {
    result.add_vars(b.vars(i));
    result.add_coeffs(-b.coeffs(i));
  }
  return result;
}

LinearExpressionProto LinearExprSum(LinearExpressionProto a,
                                    LinearExpressionProto b) {
  LinearExpressionProto result;
  result.set_offset(a.offset() + b.offset());
  result.mutable_vars()->Reserve(a.vars().size() + b.vars().size());
  result.mutable_coeffs()->Reserve(a.vars().size() + b.vars().size());
  for (const LinearExpressionProto& p : {a, b}) {
    for (int i = 0; i < p.vars().size(); ++i) {
      result.add_vars(p.vars(i));
      result.add_coeffs(p.coeffs(i));
    }
  }
  return result;
}

LinearExpressionProto NegatedLinearExpression(LinearExpressionProto a) {
  LinearExpressionProto result = a;
  result.set_offset(-a.offset());
  for (int64_t& coeff : *result.mutable_coeffs()) {
    coeff = -coeff;
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

}  // namespace

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
  const int num_vars = columns_.size();
  for (int var = 0; var < num_vars; ++var) {
    const SpanData& data = columns_[var];
    const int64_t value = solution[var];

    if (value == 0 && data.num_pos_literal > 0) {
      const int* ct_indices = &ct_buffer_[data.start];
      for (int k = 0; k < data.num_pos_literal; ++k) {
        num_false_enforcement_[ct_indices[k]]++;
      }
    }

    if (value == 1 && data.num_neg_literal > 0) {
      const int* ct_indices = &ct_buffer_[data.start + data.num_pos_literal];
      for (int k = 0; k < data.num_neg_literal; ++k) {
        num_false_enforcement_[ct_indices[k]]++;
      }
    }

    if (value != 0 && data.num_linear_entries > 0) {
      const int* ct_indices =
          &ct_buffer_[data.start + data.num_pos_literal + data.num_neg_literal];
      const int64_t* coeffs = &coeff_buffer_[data.linear_start];
      for (int k = 0; k < data.num_linear_entries; ++k) {
        activities_[ct_indices[k]] += coeffs[k] * value;
      }
    }
  }

  // Cache violations (not counting enforcement).
  for (int c = 0; c < num_constraints_; ++c) {
    distances_[c] = domains_[c].Distance(activities_[c]);
    is_violated_[c] = Violation(c) > 0;
  }
}

void LinearIncrementalEvaluator::ClearAffectedVariables() {
  if (10 * last_affected_variables_.size() < columns_.size()) {
    // Sparse.
    in_last_affected_variables_.resize(columns_.size(), false);
    for (const int var : last_affected_variables_) {
      in_last_affected_variables_[var] = false;
    }
  } else {
    // Dense.
    in_last_affected_variables_.assign(columns_.size(), false);
  }
  last_affected_variables_.clear();
  DCHECK(std::all_of(in_last_affected_variables_.begin(),
                     in_last_affected_variables_.end(),
                     [](bool b) { return !b; }));
}

// Tricky: Here we re-use in_last_affected_variables_ to resest
// var_to_score_change. And in particular we need to list all variable whose
// score changed here. Not just the one for which we have a decrease.
void LinearIncrementalEvaluator::UpdateScoreOnWeightUpdate(
    int c, absl::Span<const int64_t> jump_deltas,
    absl::Span<double> var_to_score_change) {
  if (c >= rows_.size()) return;

  DCHECK_EQ(num_false_enforcement_[c], 0);
  const SpanData& data = rows_[c];

  // Update enforcement part. Because we only update weight of currently
  // infeasible constraint, all change are 0 -> 1 transition and change by the
  // same amount, which is the current distance.
  const double enforcement_change = static_cast<double>(-distances_[c]);
  if (enforcement_change != 0.0) {
    int i = data.start;
    const int end = data.num_pos_literal + data.num_neg_literal;
    num_ops_ += end;
    for (int k = 0; k < end; ++k, ++i) {
      const int var = row_var_buffer_[i];
      if (!in_last_affected_variables_[var]) {
        var_to_score_change[var] = enforcement_change;
        in_last_affected_variables_[var] = true;
        last_affected_variables_.push_back(var);
      } else {
        var_to_score_change[var] += enforcement_change;
      }
    }
  }

  // Update linear part.
  if (data.num_linear_entries > 0) {
    const int* row_vars = &row_var_buffer_[data.start + data.num_pos_literal +
                                           data.num_neg_literal];
    const int64_t* row_coeffs = &row_coeff_buffer_[data.linear_start];
    num_ops_ += 2 * data.num_linear_entries;

    // Computing general Domain distance is slow.
    // TODO(user): optimize even more for one sided constraints.
    // Note(user): I tried to factor the two usage of this, but it is slower.
    const Domain& rhs = domains_[c];
    const int64_t rhs_min = rhs.Min();
    const int64_t rhs_max = rhs.Max();
    const bool is_simple = rhs.NumIntervals() == 2;
    const auto violation = [&rhs, rhs_min, rhs_max, is_simple](int64_t v) {
      if (v >= rhs_max) {
        return v - rhs_max;
      } else if (v <= rhs_min) {
        return rhs_min - v;
      } else {
        return is_simple ? int64_t{0} : rhs.Distance(v);
      }
    };

    const int64_t old_distance = distances_[c];
    const int64_t activity = activities_[c];
    for (int k = 0; k < data.num_linear_entries; ++k) {
      const int var = row_vars[k];
      const int64_t coeff = row_coeffs[k];
      const int64_t diff =
          violation(activity + coeff * jump_deltas[var]) - old_distance;
      if (!in_last_affected_variables_[var]) {
        var_to_score_change[var] = static_cast<double>(diff);
        in_last_affected_variables_[var] = true;
        last_affected_variables_.push_back(var);
      } else {
        var_to_score_change[var] += static_cast<double>(diff);
      }
    }
  }
}

void LinearIncrementalEvaluator::UpdateScoreOnNewlyEnforced(
    int c, double weight, absl::Span<const int64_t> jump_deltas,
    absl::Span<double> jump_scores) {
  const SpanData& data = rows_[c];

  // Everyone else had a zero cost transition that now become enforced ->
  // unenforced. So they all have better score.
  const double weight_time_violation =
      weight * static_cast<double>(distances_[c]);
  if (weight_time_violation > 0.0) {
    int i = data.start;
    const int end = data.num_pos_literal + data.num_neg_literal;
    num_ops_ += end;
    for (int k = 0; k < end; ++k, ++i) {
      const int var = row_var_buffer_[i];
      jump_scores[var] -= weight_time_violation;
      if (!in_last_affected_variables_[var]) {
        in_last_affected_variables_[var] = true;
        last_affected_variables_.push_back(var);
      }
    }
  }

  // Update linear part! It was zero and is now a diff.
  {
    int i = data.start + data.num_pos_literal + data.num_neg_literal;
    int j = data.linear_start;
    num_ops_ += 2 * data.num_linear_entries;
    const int64_t old_distance = distances_[c];
    for (int k = 0; k < data.num_linear_entries; ++k, ++i, ++j) {
      const int var = row_var_buffer_[i];
      const int64_t coeff = row_coeff_buffer_[j];
      const int64_t new_distance =
          domains_[c].Distance(activities_[c] + coeff * jump_deltas[var]);
      jump_scores[var] +=
          weight * static_cast<double>(new_distance - old_distance);
      if (!in_last_affected_variables_[var]) {
        in_last_affected_variables_[var] = true;
        last_affected_variables_.push_back(var);
      }
    }
  }
}

void LinearIncrementalEvaluator::UpdateScoreOnNewlyUnenforced(
    int c, double weight, absl::Span<const int64_t> jump_deltas,
    absl::Span<double> jump_scores) {
  const SpanData& data = rows_[c];

  // Everyone else had a enforced -> unenforced transition that now become zero.
  // So they all have worst score, and we don't need to update
  // last_affected_variables_.
  const double weight_time_violation =
      weight * static_cast<double>(distances_[c]);
  if (weight_time_violation > 0.0) {
    int i = data.start;
    const int end = data.num_pos_literal + data.num_neg_literal;
    num_ops_ += end;
    for (int k = 0; k < end; ++k, ++i) {
      const int var = row_var_buffer_[i];
      jump_scores[var] += weight_time_violation;
    }
  }

  // Update linear part! It had a diff and is now zero.
  {
    int i = data.start + data.num_pos_literal + data.num_neg_literal;
    int j = data.linear_start;
    num_ops_ += 2 * data.num_linear_entries;
    const int64_t old_distance = distances_[c];
    for (int k = 0; k < data.num_linear_entries; ++k, ++i, ++j) {
      const int var = row_var_buffer_[i];
      const int64_t coeff = row_coeff_buffer_[j];
      const int64_t new_distance =
          domains_[c].Distance(activities_[c] + coeff * jump_deltas[var]);
      jump_scores[var] -=
          weight * static_cast<double>(new_distance - old_distance);
      if (!in_last_affected_variables_[var]) {
        in_last_affected_variables_[var] = true;
        last_affected_variables_.push_back(var);
      }
    }
  }
}

// We just need to modify the old/new transition that decrease the number of
// enforcement literal at false.
void LinearIncrementalEvaluator::UpdateScoreOfEnforcementIncrease(
    int c, double score_change, absl::Span<const int64_t> jump_deltas,
    absl::Span<double> jump_scores) {
  if (score_change == 0.0) return;

  const SpanData& data = rows_[c];
  int i = data.start;
  num_ops_ += data.num_pos_literal;
  for (int k = 0; k < data.num_pos_literal; ++k, ++i) {
    const int var = row_var_buffer_[i];
    if (jump_deltas[var] == 1) {
      jump_scores[var] += score_change;
      if (score_change < 0.0 && !in_last_affected_variables_[var]) {
        in_last_affected_variables_[var] = true;
        last_affected_variables_.push_back(var);
      }
    }
  }
  num_ops_ += data.num_neg_literal;
  for (int k = 0; k < data.num_neg_literal; ++k, ++i) {
    const int var = row_var_buffer_[i];
    if (jump_deltas[var] == -1) {
      jump_scores[var] += score_change;
      if (score_change < 0.0 && !in_last_affected_variables_[var]) {
        in_last_affected_variables_[var] = true;
        last_affected_variables_.push_back(var);
      }
    }
  }
}

void LinearIncrementalEvaluator::UpdateScoreOnActivityChange(
    int c, double weight, int64_t activity_delta,
    absl::Span<const int64_t> jump_deltas, absl::Span<double> jump_scores) {
  if (activity_delta == 0) return;
  const SpanData& data = rows_[c];

  // In some cases, we can know that the score of all the involved variable
  // will not change. This is the case if whatever 1 variable change the
  // violation delta before/after is the same.
  //
  // TODO(user): Maintain more precise bounds.
  // - We could easily compute on each ComputeInitialActivities() the
  //   maximum increase/decrease per variable, and take the max as each
  //   variable changes?
  // - Know if a constraint is only <= or >= !
  const int64_t old_activity = activities_[c];
  const int64_t new_activity = old_activity + activity_delta;
  int64_t min_range;
  int64_t max_range;
  if (new_activity > old_activity) {
    min_range = old_activity - row_max_variations_[c];
    max_range = new_activity + row_max_variations_[c];
  } else {
    min_range = new_activity - row_max_variations_[c];
    max_range = old_activity + row_max_variations_[c];
  }

  // If the violation delta was zero and will still always be zero, we can skip.
  if (Domain(min_range, max_range).IsIncludedIn(domains_[c])) return;

  // Enforcement is always enforced -> un-enforced.
  // So it was -weight_time_distance and is now -weight_time_new_distance.
  const double delta =
      -weight *
      static_cast<double>(domains_[c].Distance(new_activity) - distances_[c]);
  if (delta != 0.0) {
    int i = data.start;
    const int end = data.num_pos_literal + data.num_neg_literal;
    num_ops_ += end;
    for (int k = 0; k < end; ++k, ++i) {
      const int var = row_var_buffer_[i];
      jump_scores[var] += delta;
      if (delta < 0.0 && !in_last_affected_variables_[var]) {
        in_last_affected_variables_[var] = true;
        last_affected_variables_.push_back(var);
      }
    }
  }

  // If we are infeasible and no move can correct it, both old_b - old_a and
  // new_b - new_a will have the same value. We only needed to update the
  // violation of the enforced literal.
  if (min_range >= domains_[c].Max() || max_range <= domains_[c].Min()) return;

  // Update linear part.
  if (data.num_linear_entries > 0) {
    const int* row_vars = &row_var_buffer_[data.start + data.num_pos_literal +
                                           data.num_neg_literal];
    const int64_t* row_coeffs = &row_coeff_buffer_[data.linear_start];
    num_ops_ += 2 * data.num_linear_entries;

    // Computing general Domain distance is slow.
    // TODO(user): optimize even more for one sided constraints.
    // Note(user): I tried to factor the two usage of this, but it is slower.
    const Domain& rhs = domains_[c];
    const int64_t rhs_min = rhs.Min();
    const int64_t rhs_max = rhs.Max();
    const bool is_simple = rhs.NumIntervals() == 2;
    const auto violation = [&rhs, rhs_min, rhs_max, is_simple](int64_t v) {
      if (v >= rhs_max) {
        return v - rhs_max;
      } else if (v <= rhs_min) {
        return rhs_min - v;
      } else {
        return is_simple ? int64_t{0} : rhs.Distance(v);
      }
    };

    const int64_t old_a_minus_new_a =
        distances_[c] - domains_[c].Distance(new_activity);
    for (int k = 0; k < data.num_linear_entries; ++k) {
      const int var = row_vars[k];
      const int64_t impact = row_coeffs[k] * jump_deltas[var];
      const int64_t old_b = violation(old_activity + impact);
      const int64_t new_b = violation(new_activity + impact);

      // The old score was:
      //   weight * static_cast<double>(old_b - old_a);
      // the new score is
      //   weight * static_cast<double>(new_b - new_a); so the diff is:
      //   weight * static_cast<double>(new_b - new_a - old_b + old_a)
      const int64_t diff = old_a_minus_new_a + new_b - old_b;

      // TODO(user): If a variable is at its lower (resp. upper) bound, then
      // we know that the score will always move in the same direction, so we
      // might skip the last_affected_variables_ update.
      jump_scores[var] += weight * static_cast<double>(diff);
      if (!in_last_affected_variables_[var]) {
        in_last_affected_variables_[var] = true;
        last_affected_variables_.push_back(var);
      }
    }
  }
}

// Note that the code assumes that a column has no duplicates ct indices.
void LinearIncrementalEvaluator::UpdateVariableAndScores(
    int var, int64_t delta, absl::Span<const double> weights,
    absl::Span<const int64_t> jump_deltas, absl::Span<double> jump_scores,
    std::vector<int>* constraints_with_changed_violation) {
  DCHECK(!creation_phase_);
  DCHECK_NE(delta, 0);
  if (var >= columns_.size()) return;

  const SpanData& data = columns_[var];
  int i = data.start;
  num_ops_ += data.num_pos_literal;
  for (int k = 0; k < data.num_pos_literal; ++k, ++i) {
    const int c = ct_buffer_[i];
    const int64_t v0 = Violation(c);
    if (delta == 1) {
      num_false_enforcement_[c]--;
      DCHECK_GE(num_false_enforcement_[c], 0);
      if (num_false_enforcement_[c] == 0) {
        UpdateScoreOnNewlyEnforced(c, weights[c], jump_deltas, jump_scores);
      } else if (num_false_enforcement_[c] == 1) {
        const double enforcement_change =
            weights[c] * static_cast<double>(distances_[c]);
        UpdateScoreOfEnforcementIncrease(c, enforcement_change, jump_deltas,
                                         jump_scores);
      }
    } else {
      num_false_enforcement_[c]++;
      if (num_false_enforcement_[c] == 1) {
        UpdateScoreOnNewlyUnenforced(c, weights[c], jump_deltas, jump_scores);
      } else if (num_false_enforcement_[c] == 2) {
        const double enforcement_change =
            weights[c] * static_cast<double>(distances_[c]);
        UpdateScoreOfEnforcementIncrease(c, -enforcement_change, jump_deltas,
                                         jump_scores);
      }
    }
    const int64_t v1 = Violation(c);
    is_violated_[c] = v1 > 0;
    if (v1 != v0) {
      constraints_with_changed_violation->push_back(c);
    }
  }
  num_ops_ += data.num_neg_literal;
  for (int k = 0; k < data.num_neg_literal; ++k, ++i) {
    const int c = ct_buffer_[i];
    const int64_t v0 = Violation(c);
    if (delta == -1) {
      num_false_enforcement_[c]--;
      DCHECK_GE(num_false_enforcement_[c], 0);
      if (num_false_enforcement_[c] == 0) {
        UpdateScoreOnNewlyEnforced(c, weights[c], jump_deltas, jump_scores);
      } else if (num_false_enforcement_[c] == 1) {
        const double enforcement_change =
            weights[c] * static_cast<double>(distances_[c]);
        UpdateScoreOfEnforcementIncrease(c, enforcement_change, jump_deltas,
                                         jump_scores);
      }
    } else {
      num_false_enforcement_[c]++;
      if (num_false_enforcement_[c] == 1) {
        UpdateScoreOnNewlyUnenforced(c, weights[c], jump_deltas, jump_scores);
      } else if (num_false_enforcement_[c] == 2) {
        const double enforcement_change =
            weights[c] * static_cast<double>(distances_[c]);
        UpdateScoreOfEnforcementIncrease(c, -enforcement_change, jump_deltas,
                                         jump_scores);
      }
    }
    const int64_t v1 = Violation(c);
    is_violated_[c] = v1 > 0;
    if (v1 != v0) {
      constraints_with_changed_violation->push_back(c);
    }
  }
  int j = data.linear_start;
  num_ops_ += 2 * data.num_linear_entries;
  for (int k = 0; k < data.num_linear_entries; ++k, ++i, ++j) {
    const int c = ct_buffer_[i];
    const int64_t v0 = Violation(c);
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
            jump_deltas, jump_scores);
      }
    } else if (num_false_enforcement_[c] == 0) {
      UpdateScoreOnActivityChange(c, weights[c], coeff * delta, jump_deltas,
                                  jump_scores);
    }

    activities_[c] += coeff * delta;
    distances_[c] = domains_[c].Distance(activities_[c]);
    const int64_t v1 = Violation(c);
    is_violated_[c] = v1 > 0;
    if (v1 != v0) {
      constraints_with_changed_violation->push_back(c);
    }
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
  num_ops_ += data.num_pos_literal;
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

  num_ops_ += data.num_neg_literal;
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
  num_ops_ += 2 * data.num_linear_entries;
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

    const int64_t slack_min = CapSub(domains_[c].Min(), activity);
    const int64_t slack_max = CapSub(domains_[c].Max(), activity);
    if (slack_min != std::numeric_limits<int64_t>::min()) {
      const int64_t ceil_bp = MathUtil::CeilOfRatio(slack_min, coeff);
      if (ceil_bp != result.back() && var_domain.Contains(ceil_bp)) {
        result.push_back(ceil_bp);
      }
      const int64_t floor_bp = MathUtil::FloorOfRatio(slack_min, coeff);
      if (floor_bp != result.back() && var_domain.Contains(floor_bp)) {
        result.push_back(floor_bp);
      }
    }
    if (slack_min != slack_max &&
        slack_max != std::numeric_limits<int64_t>::min()) {
      const int64_t ceil_bp = MathUtil::CeilOfRatio(slack_max, coeff);
      if (ceil_bp != result.back() && var_domain.Contains(ceil_bp)) {
        result.push_back(ceil_bp);
      }
      const int64_t floor_bp = MathUtil::FloorOfRatio(slack_max, coeff);
      if (floor_bp != result.back() && var_domain.Contains(floor_bp)) {
        result.push_back(floor_bp);
      }
    }
  }

  gtl::STLSortAndRemoveDuplicates(&result);
  return result;
}

void LinearIncrementalEvaluator::PrecomputeCompactView(
    absl::Span<const int64_t> var_max_variation) {
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

  row_max_variations_.assign(num_constraints_, 0);
  for (int var = 0; var < var_entries_.size(); ++var) {
    const int64_t range = var_max_variation[var];
    const auto& column = var_entries_[var];
    total_size += column.size();
    total_linear_size += column.size();
    for (const auto [c, coeff] : column) {
      tmp_row_sizes_[c]++;
      tmp_row_num_linear_entries_[c]++;
      row_max_variations_[c] =
          std::max(row_max_variations_[c], range * std::abs(coeff));
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
  last_affected_variables_.ClearAndReserve(columns_.size());
}

bool LinearIncrementalEvaluator::ViolationChangeIsConvex(int var) const {
  for (const int c : VarToConstraints(var)) {
    if (domains_[c].NumIntervals() > 2) return false;
  }
  return true;
}

// ----- CompiledConstraint -----

void CompiledConstraint::InitializeViolation(
    absl::Span<const int64_t> solution) {
  violation_ = ComputeViolation(solution);
}

void CompiledConstraint::PerformMove(
    int var, int64_t old_value,
    absl::Span<const int64_t> solution_with_new_value) {
  violation_ += ViolationDelta(var, old_value, solution_with_new_value);
}

int64_t CompiledConstraint::ViolationDelta(int, int64_t,
                                           absl::Span<const int64_t> solution) {
  return ComputeViolation(solution) - violation_;
}

// ----- CompiledConstraintWithProto -----

CompiledConstraintWithProto::CompiledConstraintWithProto(
    const ConstraintProto& ct_proto)
    : ct_proto_(ct_proto) {}

std::vector<int> CompiledConstraintWithProto::UsedVariables(
    const CpModelProto& model_proto) const {
  std::vector<int> result = sat::UsedVariables(ct_proto_);
  for (const int i_var : UsedIntervals(ct_proto_)) {
    const ConstraintProto& interval_proto = model_proto.constraints(i_var);
    for (const int var : sat::UsedVariables(interval_proto)) {
      result.push_back(var);
    }
  }
  gtl::STLSortAndRemoveDuplicates(&result);
  result.shrink_to_fit();
  return result;
}

// ----- CompiledBoolXorConstraint -----

CompiledBoolXorConstraint::CompiledBoolXorConstraint(
    const ConstraintProto& ct_proto)
    : CompiledConstraintWithProto(ct_proto) {}

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
    : CompiledConstraintWithProto(ct_proto) {}

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
    : CompiledConstraintWithProto(ct_proto) {}

int64_t CompiledIntProdConstraint::ComputeViolation(
    absl::Span<const int64_t> solution) {
  const int64_t target_value =
      ExprValue(ct_proto().int_prod().target(), solution);
  int64_t prod_value = 1;
  for (const LinearExpressionProto& expr : ct_proto().int_prod().exprs()) {
    prod_value *= ExprValue(expr, solution);
  }
  return std::abs(target_value - prod_value);
}

// ----- CompiledIntDivConstraint -----

CompiledIntDivConstraint::CompiledIntDivConstraint(
    const ConstraintProto& ct_proto)
    : CompiledConstraintWithProto(ct_proto) {}

int64_t CompiledIntDivConstraint::ComputeViolation(
    absl::Span<const int64_t> solution) {
  const int64_t target_value =
      ExprValue(ct_proto().int_div().target(), solution);
  DCHECK_EQ(ct_proto().int_div().exprs_size(), 2);
  const int64_t div_value = ExprValue(ct_proto().int_div().exprs(0), solution) /
                            ExprValue(ct_proto().int_div().exprs(1), solution);
  return std::abs(target_value - div_value);
}

// ----- CompiledIntModConstraint -----

CompiledIntModConstraint::CompiledIntModConstraint(
    const ConstraintProto& ct_proto)
    : CompiledConstraintWithProto(ct_proto) {}

int64_t CompiledIntModConstraint::ComputeViolation(
    absl::Span<const int64_t> solution) {
  const int64_t target_value =
      ExprValue(ct_proto().int_mod().target(), solution);
  DCHECK_EQ(ct_proto().int_mod().exprs_size(), 2);
  // Note: The violation computation assumes the modulo is constant.
  const int64_t expr_value = ExprValue(ct_proto().int_mod().exprs(0), solution);
  const int64_t mod_value = ExprValue(ct_proto().int_mod().exprs(1), solution);
  const int64_t rhs = expr_value % mod_value;
  if ((expr_value >= 0 && target_value >= 0) ||
      (expr_value <= 0 && target_value <= 0)) {
    // Easy case.
    return std::min({std::abs(target_value - rhs),
                     std::abs(target_value) + std::abs(mod_value - rhs),
                     std::abs(rhs) + std::abs(mod_value - target_value)});
  } else {
    // Different signs.
    // We use the sum of the absolute value to have a better gradiant.
    // We could also use the min of target_move and the expr_move.
    return std::abs(target_value) + std::abs(expr_value);
  }
}

// ----- CompiledAllDiffConstraint -----

CompiledAllDiffConstraint::CompiledAllDiffConstraint(
    const ConstraintProto& ct_proto)
    : CompiledConstraintWithProto(ct_proto) {}

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

// ----- NoOverlapBetweenTwoIntervals -----

NoOverlapBetweenTwoIntervals::NoOverlapBetweenTwoIntervals(
    int interval_0, int interval_1, const CpModelProto& cp_model) {
  const ConstraintProto& ct0 = cp_model.constraints(interval_0);
  const ConstraintProto& ct1 = cp_model.constraints(interval_1);

  // The more compact the better, hence the size + int[].
  num_enforcements_ =
      ct0.enforcement_literal().size() + ct1.enforcement_literal().size();
  if (num_enforcements_ > 0) {
    enforcements_.reset(new int[num_enforcements_]);
    int i = 0;
    for (const int lit : ct0.enforcement_literal()) enforcements_[i++] = lit;
    for (const int lit : ct1.enforcement_literal()) enforcements_[i++] = lit;
  }

  // We prefer to use start + size instead of end so that moving "start" moves
  // the whole interval around (for the non-fixed duration case).
  end_minus_start_1_ =
      ExprDiff(LinearExprSum(ct0.interval().start(), ct0.interval().size()),
               ct1.interval().start());
  end_minus_start_2_ =
      ExprDiff(LinearExprSum(ct1.interval().start(), ct1.interval().size()),
               ct0.interval().start());
}

// Same as NoOverlapMinRepairDistance().
int64_t NoOverlapBetweenTwoIntervals::ComputeViolationInternal(
    absl::Span<const int64_t> solution) {
  for (int i = 0; i < num_enforcements_; ++i) {
    if (!LiteralValue(enforcements_[i], solution)) return 0;
  }
  const int64_t diff1 = ExprValue(end_minus_start_1_, solution);
  const int64_t diff2 = ExprValue(end_minus_start_2_, solution);
  return std::max(std::min(diff1, diff2), int64_t{0});
}

std::vector<int> NoOverlapBetweenTwoIntervals::UsedVariables(
    const CpModelProto& /*model_proto*/) const {
  std::vector<int> result;
  for (int i = 0; i < num_enforcements_; ++i) {
    result.push_back(PositiveRef(enforcements_[i]));
  }
  for (const int var : end_minus_start_1_.vars()) {
    result.push_back(PositiveRef(var));
  }
  for (const int var : end_minus_start_2_.vars()) {
    result.push_back(PositiveRef(var));
  }
  gtl::STLSortAndRemoveDuplicates(&result);
  result.shrink_to_fit();
  return result;
}

// ----- CompiledNoOverlap2dConstraint -----

int64_t OverlapOfTwoIntervals(const ConstraintProto& interval1,
                              const ConstraintProto& interval2,
                              absl::Span<const int64_t> solution) {
  for (const int lit : interval1.enforcement_literal()) {
    if (!LiteralValue(lit, solution)) return 0;
  }
  for (const int lit : interval2.enforcement_literal()) {
    if (!LiteralValue(lit, solution)) return 0;
  }

  const int64_t start1 = ExprValue(interval1.interval().start(), solution);
  const int64_t end1 = ExprValue(interval1.interval().end(), solution);

  const int64_t start2 = ExprValue(interval2.interval().start(), solution);
  const int64_t end2 = ExprValue(interval2.interval().end(), solution);

  if (start1 >= end2 || start2 >= end1) return 0;  // Disjoint.

  // We force a min cost of 1 to cover the case where a interval of size 0 is in
  // the middle of another interval.
  return std::max(std::min(std::min(end2 - start2, end1 - start1),
                           std::min(end2 - start1, end1 - start2)),
                  int64_t{1});
}

int64_t NoOverlapMinRepairDistance(const ConstraintProto& interval1,
                                   const ConstraintProto& interval2,
                                   absl::Span<const int64_t> solution) {
  for (const int lit : interval1.enforcement_literal()) {
    if (!LiteralValue(lit, solution)) return 0;
  }
  for (const int lit : interval2.enforcement_literal()) {
    if (!LiteralValue(lit, solution)) return 0;
  }

  const int64_t start1 = ExprValue(interval1.interval().start(), solution);
  const int64_t end1 = ExprValue(interval1.interval().end(), solution);

  const int64_t start2 = ExprValue(interval2.interval().start(), solution);
  const int64_t end2 = ExprValue(interval2.interval().end(), solution);

  return std::max(std::min(end2 - start1, end1 - start2), int64_t{0});
}

CompiledNoOverlap2dConstraint::CompiledNoOverlap2dConstraint(
    const ConstraintProto& ct_proto, const CpModelProto& cp_model)
    : CompiledConstraintWithProto(ct_proto), cp_model_(cp_model) {}

int64_t CompiledNoOverlap2dConstraint::ComputeViolation(
    absl::Span<const int64_t> solution) {
  DCHECK_GE(ct_proto().no_overlap_2d().x_intervals_size(), 2);
  const int size = ct_proto().no_overlap_2d().x_intervals_size();

  int64_t violation = 0;
  for (int i = 0; i + 1 < size; ++i) {
    const ConstraintProto& x_i =
        cp_model_.constraints(ct_proto().no_overlap_2d().x_intervals(i));
    const ConstraintProto& y_i =
        cp_model_.constraints(ct_proto().no_overlap_2d().y_intervals(i));
    for (int j = i + 1; j < size; ++j) {
      const ConstraintProto& x_j =
          cp_model_.constraints(ct_proto().no_overlap_2d().x_intervals(j));
      const ConstraintProto& y_j =
          cp_model_.constraints(ct_proto().no_overlap_2d().y_intervals(j));

      // TODO(user): Experiment with
      // violation +=
      //     std::max(std::min(NoOverlapMinRepairDistance(x_i, x_j, solution),
      //                       NoOverlapMinRepairDistance(y_i, y_j, solution)),
      //              int64_t{0});
      // Currently, the effect is unclear on 2d packing problems.
      violation +=
          std::max(std::min(NoOverlapMinRepairDistance(x_i, x_j, solution) *
                                OverlapOfTwoIntervals(y_i, y_j, solution),
                            NoOverlapMinRepairDistance(y_i, y_j, solution) *
                                OverlapOfTwoIntervals(x_i, x_j, solution)),
                   int64_t{0});
    }
  }
  return violation;
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
class CompiledCircuitConstraint : public CompiledConstraintWithProto {
 public:
  explicit CompiledCircuitConstraint(const ConstraintProto& ct_proto);
  ~CompiledCircuitConstraint() override = default;

  int64_t ComputeViolation(absl::Span<const int64_t> solution) override;
  void PerformMove(int var, int64_t old_value,
                   absl::Span<const int64_t> new_solution) override;
  int64_t ViolationDelta(
      int var, int64_t old_value,
      absl::Span<const int64_t> solution_with_new_value) override;

 private:
  struct SccOutput {
    void emplace_back(const int* start, const int* end);
    void reset(int num_nodes);

    int num_components = 0;
    std::vector<bool> skipped;
    std::vector<int> root;
  };
  void InitGraph(absl::Span<const int64_t> solution);
  bool UpdateGraph(int var, int64_t value);
  int64_t ViolationForCurrentGraph();

  absl::flat_hash_map<int, std::vector<int>> arcs_by_lit_;
  absl::Span<const int> literals_;
  absl::Span<const int> tails_;
  absl::Span<const int> heads_;
  // Stores the currently active arcs per tail node.
  std::vector<DenseSet<int>> graph_;
  SccOutput sccs_;
  SccOutput committed_sccs_;
  std::vector<bool> has_in_arc_;
  StronglyConnectedComponentsFinder<int, std::vector<DenseSet<int>>, SccOutput>
      scc_finder_;
};

void CompiledCircuitConstraint::SccOutput::emplace_back(int const* start,
                                                        int const* end) {
  const int root_node = *start;
  const int size = end - start;
  if (size > 1) {
    ++num_components;
  }
  for (; start != end; ++start) {
    root[*start] = root_node;
    skipped[*start] = (size == 1);
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
    : CompiledConstraintWithProto(ct_proto) {
  const bool routes = ct_proto.has_routes();
  tails_ = routes ? ct_proto.routes().tails() : ct_proto.circuit().tails();
  heads_ = absl::MakeConstSpan(routes ? ct_proto.routes().heads()
                                      : ct_proto.circuit().heads());
  literals_ = absl::MakeConstSpan(routes ? ct_proto.routes().literals()
                                         : ct_proto.circuit().literals());
  graph_.resize(*absl::c_max_element(tails_) + 1);
  for (int i = 0; i < literals_.size(); ++i) {
    arcs_by_lit_[literals_[i]].push_back(i);
  }
}

void CompiledCircuitConstraint::InitGraph(absl::Span<const int64_t> solution) {
  for (DenseSet<int>& edges : graph_) {
    edges.clear();
  }
  for (int i = 0; i < tails_.size(); ++i) {
    if (!LiteralValue(literals_[i], solution)) continue;
    graph_[tails_[i]].insert(heads_[i]);
  }
}

bool CompiledCircuitConstraint::UpdateGraph(int var, int64_t value) {
  bool needs_update = false;
  const int enabled_lit =
      value != 0 ? PositiveRef(var) : NegatedRef(PositiveRef(var));
  const int disabled_lit = NegatedRef(enabled_lit);
  for (const int arc : arcs_by_lit_[disabled_lit]) {
    const int tail = tails_[arc];
    const int head = heads_[arc];
    // Removing a self arc cannot change violation.
    needs_update = needs_update || tail != head;
    graph_[tails_[arc]].erase(heads_[arc]);
  }
  for (const int arc : arcs_by_lit_[enabled_lit]) {
    const int tail = tails_[arc];
    const int head = heads_[arc];
    // Adding an arc can only change violation if it connects new SCCs.
    needs_update = needs_update ||
                   committed_sccs_.root[tail] != committed_sccs_.root[head];
    graph_[tails_[arc]].insert(heads_[arc]);
  }
  return needs_update;
}

void CompiledCircuitConstraint::PerformMove(
    int var, int64_t, absl::Span<const int64_t> new_solution) {
  UpdateGraph(var, new_solution[var]);
  violation_ = ViolationForCurrentGraph();
  std::swap(committed_sccs_, sccs_);
}

int64_t CompiledCircuitConstraint::ComputeViolation(
    absl::Span<const int64_t> solution) {
  InitGraph(solution);
  int64_t result = ViolationForCurrentGraph();
  std::swap(committed_sccs_, sccs_);
  return result;
}

int64_t CompiledCircuitConstraint::ViolationDelta(
    int var, int64_t old_value,
    absl::Span<const int64_t> solution_with_new_value) {
  int64_t result = 0;
  if (UpdateGraph(var, solution_with_new_value[var])) {
    result = ViolationForCurrentGraph() - violation_;
  }
  UpdateGraph(var, old_value);
  return result;
}

int64_t CompiledCircuitConstraint::ViolationForCurrentGraph() {
  const int num_nodes = graph_.size();
  sccs_.reset(num_nodes);
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

LsEvaluator::LsEvaluator(const CpModelProto& cp_model,
                         const SatParameters& params)
    : cp_model_(cp_model), params_(params) {
  var_to_constraints_.resize(cp_model_.variables_size());
  jump_value_optimal_.resize(cp_model_.variables_size(), true);
  num_violated_constraint_per_var_ignoring_objective_.assign(
      cp_model_.variables_size(), 0);

  std::vector<bool> ignored_constraints(cp_model_.constraints_size(), false);
  std::vector<ConstraintProto> additional_constraints;
  CompileConstraintsAndObjective(ignored_constraints, additional_constraints);
  BuildVarConstraintGraph();
  violated_constraints_.reserve(NumEvaluatorConstraints());
}

LsEvaluator::LsEvaluator(
    const CpModelProto& cp_model, const SatParameters& params,
    const std::vector<bool>& ignored_constraints,
    const std::vector<ConstraintProto>& additional_constraints)
    : cp_model_(cp_model), params_(params) {
  var_to_constraints_.resize(cp_model_.variables_size());
  jump_value_optimal_.resize(cp_model_.variables_size(), true);
  num_violated_constraint_per_var_ignoring_objective_.assign(
      cp_model_.variables_size(), 0);
  CompileConstraintsAndObjective(ignored_constraints, additional_constraints);
  BuildVarConstraintGraph();
  violated_constraints_.reserve(NumEvaluatorConstraints());
}

void LsEvaluator::BuildVarConstraintGraph() {
  // Clear the var <-> constraint graph.
  for (std::vector<int>& ct_indices : var_to_constraints_) ct_indices.clear();
  constraint_to_vars_.resize(constraints_.size());

  // Build the var <-> constraint graph.
  for (int ct_index = 0; ct_index < constraints_.size(); ++ct_index) {
    constraint_to_vars_[ct_index] =
        constraints_[ct_index]->UsedVariables(cp_model_);
    for (const int var : constraint_to_vars_[ct_index]) {
      var_to_constraints_[var].push_back(ct_index);
    }
  }

  // Remove duplicates.
  for (std::vector<int>& constraints : var_to_constraints_) {
    gtl::STLSortAndRemoveDuplicates(&constraints);
  }
  for (std::vector<int>& vars : constraint_to_vars_) {
    gtl::STLSortAndRemoveDuplicates(&vars);
  }

  // Scan the model to decide if a variable is linked to a convex evaluation.
  jump_value_optimal_.resize(cp_model_.variables_size());
  for (int i = 0; i < cp_model_.variables_size(); ++i) {
    if (!var_to_constraints_[i].empty()) {
      jump_value_optimal_[i] = false;
      continue;
    }

    const IntegerVariableProto& var_proto = cp_model_.variables(i);
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
            ExprMax(target, cp_model_) - ExprMin(expr, cp_model_);
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
    case ConstraintProto::ConstraintCase::kIntMod: {
      DCHECK_EQ(ExprMin(ct.int_mod().exprs(1), cp_model_),
                ExprMax(ct.int_mod().exprs(1), cp_model_));
      constraints_.emplace_back(new CompiledIntModConstraint(ct));
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
      const int size = ct.no_overlap().intervals_size();
      if (size <= 1) break;
      if (size > params_.feasibility_jump_max_expanded_constraint_size()) {
        // Similar code to the kCumulative constraint.
        // The violation will be the area above the capacity.
        LinearExpressionProto one;
        one.set_offset(1);
        std::vector<std::optional<int>> is_active;
        std::vector<LinearExpressionProto> times;
        std::vector<LinearExpressionProto> demands;
        const int num_intervals = ct.no_overlap().intervals().size();
        for (int i = 0; i < num_intervals; ++i) {
          const ConstraintProto& interval_ct =
              cp_model_.constraints(ct.no_overlap().intervals(i));
          if (interval_ct.enforcement_literal().empty()) {
            is_active.push_back(std::nullopt);
            is_active.push_back(std::nullopt);
          } else {
            CHECK_EQ(interval_ct.enforcement_literal().size(), 1);
            is_active.push_back(interval_ct.enforcement_literal(0));
            is_active.push_back(interval_ct.enforcement_literal(0));
          }

          times.push_back(interval_ct.interval().start());
          times.push_back(LinearExprSum(interval_ct.interval().start(),
                                        interval_ct.interval().size()));
          demands.push_back(one);
          demands.push_back(NegatedLinearExpression(one));
        }
        constraints_.emplace_back(new CompiledReservoirConstraint(
            std::move(one), std::move(is_active), std::move(times),
            std::move(demands)));
      } else {
        // We expand the no_overlap constraints into a quadratic number of
        // disjunctions.
        for (int i = 0; i + 1 < size; ++i) {
          const IntervalConstraintProto& interval_i =
              cp_model_.constraints(ct.no_overlap().intervals(i)).interval();
          const int64_t min_start_i = ExprMin(interval_i.start(), cp_model_);
          const int64_t max_end_i = ExprMax(interval_i.end(), cp_model_);
          for (int j = i + 1; j < size; ++j) {
            const IntervalConstraintProto& interval_j =
                cp_model_.constraints(ct.no_overlap().intervals(j)).interval();
            const int64_t min_start_j = ExprMin(interval_j.start(), cp_model_);
            const int64_t max_end_j = ExprMax(interval_j.end(), cp_model_);
            if (min_start_i >= max_end_j || min_start_j >= max_end_i) continue;

            constraints_.emplace_back(new NoOverlapBetweenTwoIntervals(
                ct.no_overlap().intervals(i), ct.no_overlap().intervals(j),
                cp_model_));
          }
        }
      }
      break;
    }
    case ConstraintProto::ConstraintCase::kCumulative: {
      LinearExpressionProto capacity = ct.cumulative().capacity();
      std::vector<std::optional<int>> is_active;
      std::vector<LinearExpressionProto> times;
      std::vector<LinearExpressionProto> demands;
      const int num_intervals = ct.cumulative().intervals().size();
      for (int i = 0; i < num_intervals; ++i) {
        const ConstraintProto& interval_ct =
            cp_model_.constraints(ct.cumulative().intervals(i));
        if (interval_ct.enforcement_literal().empty()) {
          is_active.push_back(std::nullopt);
          is_active.push_back(std::nullopt);
        } else {
          CHECK_EQ(interval_ct.enforcement_literal().size(), 1);
          is_active.push_back(interval_ct.enforcement_literal(0));
          is_active.push_back(interval_ct.enforcement_literal(0));
        }

        // Start.
        times.push_back(interval_ct.interval().start());
        demands.push_back(ct.cumulative().demands(i));

        // End.
        // I tried 3 alternatives: end, max(end, start+size) and just start +
        // size. The most performing one was "start + size" on the multi-mode
        // RCPSP.
        //
        // Note that for fixed size, this do not matter. It is easy enough to
        // try any expression by creating a small wrapper class to use instead
        // of a LinearExpressionProto for time.
        times.push_back(LinearExprSum(interval_ct.interval().start(),
                                      interval_ct.interval().size()));
        demands.push_back(NegatedLinearExpression(ct.cumulative().demands(i)));
      }

      constraints_.emplace_back(new CompiledReservoirConstraint(
          std::move(capacity), std::move(is_active), std::move(times),
          std::move(demands)));
      break;
    }
    case ConstraintProto::ConstraintCase::kNoOverlap2D: {
      const auto& x_intervals = ct.no_overlap_2d().x_intervals();
      const auto& y_intervals = ct.no_overlap_2d().y_intervals();
      const int size = x_intervals.size();
      if (size <= 1) break;
      if (size == 2 ||
          size > params_.feasibility_jump_max_expanded_constraint_size()) {
        CompiledNoOverlap2dConstraint* no_overlap_2d =
            new CompiledNoOverlap2dConstraint(ct, cp_model_);
        constraints_.emplace_back(no_overlap_2d);
        break;
      }

      for (int i = 0; i + 1 < size; ++i) {
        const IntervalConstraintProto& x_interval_i =
            cp_model_.constraints(x_intervals[i]).interval();
        const int64_t x_min_start_i = ExprMin(x_interval_i.start(), cp_model_);
        const int64_t x_max_end_i = ExprMax(x_interval_i.end(), cp_model_);
        const IntervalConstraintProto& y_interval_i =
            cp_model_.constraints(y_intervals[i]).interval();
        const int64_t y_min_start_i = ExprMin(y_interval_i.start(), cp_model_);
        const int64_t y_max_end_i = ExprMax(y_interval_i.end(), cp_model_);
        for (int j = i + 1; j < size; ++j) {
          const IntervalConstraintProto& x_interval_j =
              cp_model_.constraints(x_intervals[j]).interval();
          const int64_t x_min_start_j =
              ExprMin(x_interval_j.start(), cp_model_);
          const int64_t x_max_end_j = ExprMax(x_interval_j.end(), cp_model_);
          const IntervalConstraintProto& y_interval_j =
              cp_model_.constraints(y_intervals[j]).interval();
          const int64_t y_min_start_j =
              ExprMin(y_interval_j.start(), cp_model_);
          const int64_t y_max_end_j = ExprMax(y_interval_j.end(), cp_model_);
          if (x_min_start_i >= x_max_end_j || x_min_start_j >= x_max_end_i ||
              y_min_start_i >= y_max_end_j || y_min_start_j >= y_max_end_i) {
            continue;
          }
          ConstraintProto* diffn = expanded_constraints_.add_constraints();
          diffn->mutable_no_overlap_2d()->add_x_intervals(x_intervals[i]);
          diffn->mutable_no_overlap_2d()->add_x_intervals(x_intervals[j]);
          diffn->mutable_no_overlap_2d()->add_y_intervals(y_intervals[i]);
          diffn->mutable_no_overlap_2d()->add_y_intervals(y_intervals[j]);
          CompiledNoOverlap2dConstraint* no_overlap_2d =
              new CompiledNoOverlap2dConstraint(*diffn, cp_model_);
          constraints_.emplace_back(no_overlap_2d);
        }
      }
      break;
    }
    case ConstraintProto::ConstraintCase::kCircuit:
    case ConstraintProto::ConstraintCase::kRoutes:
      constraints_.emplace_back(new CompiledCircuitConstraint(ct));
      AddCircuitFlowConstraints(linear_evaluator_, ct);
      break;
    default:
      VLOG(1) << "Not implemented: " << ct.constraint_case();
      break;
  }
}

void LsEvaluator::CompileConstraintsAndObjective(
    const std::vector<bool>& ignored_constraints,
    const std::vector<ConstraintProto>& additional_constraints) {
  constraints_.clear();

  // The first compiled constraint is always the objective if present.
  if (cp_model_.has_objective()) {
    const int ct_index = linear_evaluator_.NewConstraint(
        cp_model_.objective().domain().empty()
            ? Domain::AllValues()
            : ReadDomainFromProto(cp_model_.objective()));
    DCHECK_EQ(0, ct_index);
    for (int i = 0; i < cp_model_.objective().vars_size(); ++i) {
      const int var = cp_model_.objective().vars(i);
      const int64_t coeff = cp_model_.objective().coeffs(i);
      linear_evaluator_.AddTerm(ct_index, var, coeff);
    }
  }

  for (int c = 0; c < cp_model_.constraints_size(); ++c) {
    if (ignored_constraints[c]) continue;
    CompileOneConstraint(cp_model_.constraints(c));
  }

  for (const ConstraintProto& ct : additional_constraints) {
    CompileOneConstraint(ct);
  }

  // Make sure we have access to the data in an efficient way.
  std::vector<int64_t> var_max_variations(cp_model_.variables().size());
  for (int var = 0; var < cp_model_.variables().size(); ++var) {
    const auto& domain = cp_model_.variables(var).domain();
    var_max_variations[var] = domain[domain.size() - 1] - domain[0];
  }
  linear_evaluator_.PrecomputeCompactView(var_max_variations);
}

bool LsEvaluator::ReduceObjectiveBounds(int64_t lb, int64_t ub) {
  if (!cp_model_.has_objective()) return false;
  if (linear_evaluator_.ReduceBounds(/*c=*/0, lb, ub)) {
    UpdateViolatedList(0);
    return true;
  }
  return false;
}

void LsEvaluator::ComputeAllViolations(absl::Span<const int64_t> solution) {
  // Linear constraints.
  linear_evaluator_.ComputeInitialActivities(solution);

  // Generic constraints.
  for (const auto& ct : constraints_) {
    ct->InitializeViolation(solution);
  }

  RecomputeViolatedList(/*linear_only=*/false);
}

void LsEvaluator::ComputeAllNonLinearViolations(
    absl::Span<const int64_t> solution) {
  // Generic constraints.
  for (const auto& ct : constraints_) {
    ct->InitializeViolation(solution);
  }
}

void LsEvaluator::UpdateNonLinearViolations(
    int var, int64_t old_value, absl::Span<const int64_t> new_solution) {
  for (const int general_ct_index : var_to_constraints_[var]) {
    const int c = general_ct_index + linear_evaluator_.num_constraints();
    const int64_t v0 = constraints_[general_ct_index]->violation();
    constraints_[general_ct_index]->PerformMove(var, old_value, new_solution);
    const int64_t violation_delta =
        constraints_[general_ct_index]->violation() - v0;
    if (violation_delta != 0) {
      last_update_violation_changes_.push_back(c);
    }
  }
}

void LsEvaluator::UpdateLinearScores(int var, int64_t old_value,
                                     int64_t new_value,
                                     absl::Span<const double> weights,
                                     absl::Span<const int64_t> jump_deltas,
                                     absl::Span<double> jump_scores) {
  DCHECK(RefIsPositive(var));
  if (old_value == new_value) return;
  last_update_violation_changes_.clear();
  linear_evaluator_.ClearAffectedVariables();
  linear_evaluator_.UpdateVariableAndScores(var, new_value - old_value, weights,
                                            jump_deltas, jump_scores,
                                            &last_update_violation_changes_);
}

void LsEvaluator::UpdateViolatedList() {
  // Maintain the list of violated constraints.
  dtime_ += 1e-8 * last_update_violation_changes_.size();
  for (const int c : last_update_violation_changes_) {
    UpdateViolatedList(c);
  }
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
  DCHECK(cp_model_.has_objective());
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
  double result = linear_evaluator_.WeightedViolation(weights);

  const int num_linear_constraints = linear_evaluator_.num_constraints();
  for (int c = 0; c < constraints_.size(); ++c) {
    result += static_cast<double>(constraints_[c]->violation()) *
              weights[num_linear_constraints + c];
  }
  return result;
}

double LsEvaluator::WeightedViolationDelta(
    bool linear_only, absl::Span<const double> weights, int var, int64_t delta,
    absl::Span<int64_t> mutable_solution) const {
  double result = linear_evaluator_.WeightedViolationDelta(weights, var, delta);
  if (linear_only) return result;

  // We change the mutable solution here, and restore it after the evaluation.
  const int64_t old_value = mutable_solution[var];
  mutable_solution[var] += delta;

  const int num_linear_constraints = linear_evaluator_.num_constraints();
  for (const int ct_index : var_to_constraints_[var]) {
    // We assume linear time delta computation in number of variables.
    // TODO(user): refine on a per constraint basis.
    dtime_ += 1e-8 * static_cast<double>(constraint_to_vars_[ct_index].size());

    DCHECK_LT(ct_index, constraints_.size());
    const int64_t ct_delta = constraints_[ct_index]->ViolationDelta(
        var, old_value, mutable_solution);
    result += static_cast<double>(ct_delta) *
              weights[ct_index + num_linear_constraints];
  }

  // Restore.
  mutable_solution[var] = old_value;
  return result;
}

bool LsEvaluator::VariableOnlyInLinearConstraintWithConvexViolationChange(
    int var) const {
  return jump_value_optimal_[var];
}

void LsEvaluator::RecomputeViolatedList(bool linear_only) {
  num_violated_constraint_per_var_ignoring_objective_.assign(
      cp_model_.variables_size(), 0);
  violated_constraints_.clear();
  const int num_constraints =
      linear_only ? NumLinearConstraints() : NumEvaluatorConstraints();
  for (int c = 0; c < num_constraints; ++c) {
    UpdateViolatedList(c);
  }
}

void LsEvaluator::UpdateViolatedList(const int c) {
  if (Violation(c) > 0) {
    auto [it, inserted] = violated_constraints_.insert(c);
    // The constraint is violated. Add if needed.
    if (!inserted) return;
    if (IsObjectiveConstraint(c)) return;
    dtime_ += 1e-8 * ConstraintToVars(c).size();
    for (const int v : ConstraintToVars(c)) {
      num_violated_constraint_per_var_ignoring_objective_[v] += 1;
    }
    return;
  }
  if (violated_constraints_.erase(c) == 1) {
    if (IsObjectiveConstraint(c)) return;
    dtime_ += 1e-8 * ConstraintToVars(c).size();
    for (const int v : ConstraintToVars(c)) {
      num_violated_constraint_per_var_ignoring_objective_[v] -= 1;
    }
  }
}

int64_t CompiledReservoirConstraint::BuildProfileAndReturnViolation(
    absl::Span<const int64_t> solution) {
  // Starts by filling the cache and profile_.
  capacity_value_ = ExprValue(capacity_, solution);
  const int num_events = time_values_.size();
  profile_.clear();
  for (int i = 0; i < num_events; ++i) {
    time_values_[i] = ExprValue(times_[i], solution);
    if (is_active_[i] != std::nullopt &&
        LiteralValue(*is_active_[i], solution) == 0) {
      demand_values_[i] = 0;
    } else {
      demand_values_[i] = ExprValue(demands_[i], solution);
      if (demand_values_[i] != 0) {
        profile_.push_back({time_values_[i], demand_values_[i]});
      }
    }
  }

  if (profile_.empty()) return 0;
  absl::c_sort(profile_);

  // Compress the profile for faster incremental evaluation.
  {
    int p = 0;
    for (int i = 1; i < profile_.size(); ++i) {
      if (profile_[i].time == profile_[p].time) {
        profile_[p].demand += profile_[i].demand;
      } else {
        profile_[++p] = profile_[i];
      }
    }
    profile_.resize(p + 1);
  }

  int64_t overload = 0;
  int64_t current_load = 0;
  int64_t previous_time = std::numeric_limits<int64_t>::min();
  for (int i = 0; i < profile_.size(); ++i) {
    // At this point, current_load is the load at previous_time.
    const int64_t time = profile_[i].time;
    if (current_load > capacity_value_) {
      overload = CapAdd(overload, CapProd(current_load - capacity_value_,
                                          time - previous_time));
    }

    current_load += profile_[i].demand;
    previous_time = time;
  }
  return overload;
}

int64_t CompiledReservoirConstraint::IncrementalViolation(
    int var, absl::Span<const int64_t> solution) {
  const int64_t capacity = ExprValue(capacity_, solution);
  profile_delta_.clear();
  CHECK(RefIsPositive(var));
  for (const int i : dense_index_to_events_[var_to_dense_index_.at(var)]) {
    const int64_t time = ExprValue(times_[i], solution);
    int64_t demand = 0;
    if (is_active_[i] == std::nullopt ||
        LiteralValue(*is_active_[i], solution) == 1) {
      demand = ExprValue(demands_[i], solution);
    }

    if (time == time_values_[i]) {
      if (demand != demand_values_[i]) {
        // Update the demand at time.
        profile_delta_.push_back({time, demand - demand_values_[i]});
      }
    } else {
      // Remove previous.
      if (demand_values_[i] != 0) {
        profile_delta_.push_back({time_values_[i], -demand_values_[i]});
      }
      // Add new.
      if (demand != 0) {
        profile_delta_.push_back({time, demand});
      }
    }
  }

  // Abort early if there is no change.
  // This might happen because we use max(start + size, end) for the time and
  // even if some variable changed there, the time might not have.
  if (capacity == capacity_value_ && profile_delta_.empty()) {
    return violation_;
  }
  absl::c_sort(profile_delta_);

  // Similar algo, but we scan the two vectors at once.
  int64_t overload = 0;
  int64_t current_load = 0;
  int64_t previous_time = std::numeric_limits<int64_t>::min();

  // TODO(user): This code is the hotspot for our local search on cumulative.
  // It can probably be slighlty improved. We might also be able to abort early
  // if we know that capacity is high enough compared to the highest point of
  // the profile.
  int i = 0;
  int j = 0;
  const absl::Span<const Event> i_profile(profile_);
  const absl::Span<const Event> j_profile(profile_delta_);
  while (true) {
    int64_t time;
    if (i < i_profile.size() && j < j_profile.size()) {
      time = std::min(i_profile[i].time, j_profile[j].time);
    } else if (i < i_profile.size()) {
      time = i_profile[i].time;
    } else if (j < j_profile.size()) {
      time = j_profile[j].time;
    } else {
      // End of loop.
      break;
    }

    // Update overload if needed.
    // At this point, current_load is the load at previous_time.
    if (current_load > capacity) {
      overload = CapAdd(overload,
                        CapProd(current_load - capacity, time - previous_time));
    }

    // Update i and current load.
    while (i < i_profile.size() && i_profile[i].time == time) {
      current_load += i_profile[i].demand;
      i++;
    }

    // Update j and current load.
    while (j < j_profile.size() && j_profile[j].time == time) {
      current_load += j_profile[j].demand;
      j++;
    }

    previous_time = time;
  }
  return overload;
}

void CompiledReservoirConstraint::AppendVariablesForEvent(
    int i, std::vector<int>* result) const {
  if (is_active_[i] != std::nullopt) {
    result->push_back(PositiveRef(*is_active_[i]));
  }
  for (const int var : times_[i].vars()) {
    result->push_back(PositiveRef(var));
  }
  for (const int var : demands_[i].vars()) {
    result->push_back(PositiveRef(var));
  }
}

void CompiledReservoirConstraint::InitializeDenseIndexToEvents() {
  // We scan the constraint a few times, but this is called once, so we don't
  // care too much.
  CpModelProto unused;
  int num_dense_indices = 0;
  for (const int var : UsedVariables(unused)) {
    var_to_dense_index_[var] = num_dense_indices++;
  }

  CompactVectorVector<int, int> event_to_dense_indices;
  event_to_dense_indices.reserve(times_.size());
  const int num_events = times_.size();
  std::vector<int> result;
  for (int i = 0; i < num_events; ++i) {
    result.clear();
    AppendVariablesForEvent(i, &result);

    // Remap and add.
    for (int& var : result) {
      var = var_to_dense_index_.at(var);
    }
    gtl::STLSortAndRemoveDuplicates(&result);
    event_to_dense_indices.Add(result);
  }

  // Note that because of the capacity (which might be variable) it is important
  // to resize this to num_dense_indices.
  dense_index_to_events_.ResetFromTranspose(event_to_dense_indices,
                                            num_dense_indices);
}

std::vector<int> CompiledReservoirConstraint::UsedVariables(
    const CpModelProto& /*model_proto*/) const {
  std::vector<int> result;
  const int num_events = times_.size();
  for (int i = 0; i < num_events; ++i) {
    AppendVariablesForEvent(i, &result);
  }
  for (const int var : capacity_.vars()) {
    result.push_back(PositiveRef(var));
  }
  gtl::STLSortAndRemoveDuplicates(&result);
  result.shrink_to_fit();
  return result;
}

}  // namespace sat
}  // namespace operations_research
