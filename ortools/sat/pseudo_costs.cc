// Copyright 2010-2024 Google LLC
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

#include "ortools/sat/pseudo_costs.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <tuple>
#include <vector>

#include "absl/log/check.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/util.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

PseudoCosts::PseudoCosts(Model* model)
    : parameters_(*model->GetOrCreate<SatParameters>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      encoder_(model->GetOrCreate<IntegerEncoder>()) {
  const int num_vars = integer_trail_->NumIntegerVariables().value();
  pseudo_costs_.resize(num_vars);
  is_relevant_.resize(num_vars, false);
  scores_.resize(num_vars, 0.0);
}

void PseudoCosts::UpdateCost(
    const std::vector<VariableBoundChange>& bound_changes,
    const IntegerValue obj_bound_improvement) {
  DCHECK_GE(obj_bound_improvement, 0);
  if (obj_bound_improvement == IntegerValue(0)) return;

  const double epsilon = 1e-6;
  for (const auto [var, lb_change] : bound_changes) {
    if (integer_trail_->IsCurrentlyIgnored(var)) continue;
    if (lb_change == IntegerValue(0)) continue;

    if (var >= pseudo_costs_.size()) {
      // Create space for new variable and its negation.
      const int new_size = std::max(var, NegationOf(var)).value() + 1;
      is_relevant_.resize(new_size, false);
      scores_.resize(new_size, 0.0);
      pseudo_costs_.resize(new_size, IncrementalAverage(0.0));
    }

    pseudo_costs_[var].AddData(ToDouble(obj_bound_improvement) /
                               ToDouble(lb_change));

    const IntegerVariable positive_var = PositiveVariable(var);
    const IntegerVariable negative_var = NegationOf(positive_var);
    const int64_t count = pseudo_costs_[positive_var].NumRecords() +
                          pseudo_costs_[negative_var].NumRecords();
    if (count >= parameters_.pseudo_cost_reliability_threshold()) {
      scores_[positive_var] = std::max(GetCost(positive_var), epsilon) *
                              std::max(GetCost(negative_var), epsilon);

      if (!is_relevant_[positive_var]) {
        is_relevant_[positive_var] = true;
        relevant_variables_.push_back(positive_var);
      }
    }
  }
}

// TODO(user): Supports search randomization tolerance.
// TODO(user): Implement generic class to choose the randomized
// solution, and supports sub-linear variable selection.
IntegerVariable PseudoCosts::GetBestDecisionVar() {
  IntegerVariable chosen_var = kNoIntegerVariable;
  double best_score = -std::numeric_limits<double>::infinity();

  // TODO(user): Avoid the O(num_relevant_variable) loop.
  // In practice since a variable only become relevant after 100 records, this
  // list might be small compared to the number of variable though.
  for (const IntegerVariable positive_var : relevant_variables_) {
    if (integer_trail_->IsCurrentlyIgnored(positive_var)) continue;
    const IntegerValue lb = integer_trail_->LowerBound(positive_var);
    const IntegerValue ub = integer_trail_->UpperBound(positive_var);
    if (lb >= ub) continue;
    if (scores_[positive_var] > best_score) {
      chosen_var = positive_var;
      best_score = scores_[positive_var];
    }
  }

  // Pick the direction with best pseudo cost.
  if (chosen_var != kNoIntegerVariable &&
      GetCost(chosen_var) < GetCost(NegationOf(chosen_var))) {
    chosen_var = NegationOf(chosen_var);
  }
  return chosen_var;
}

std::vector<PseudoCosts::VariableBoundChange> PseudoCosts::GetBoundChanges(
    Literal decision) {
  std::vector<PseudoCosts::VariableBoundChange> bound_changes;

  for (const IntegerLiteral l : encoder_->GetIntegerLiterals(decision)) {
    if (integer_trail_->IsCurrentlyIgnored(l.var)) continue;
    PseudoCosts::VariableBoundChange var_bound_change;
    var_bound_change.var = l.var;
    var_bound_change.lower_bound_change =
        l.bound - integer_trail_->LowerBound(l.var);
    bound_changes.push_back(var_bound_change);
  }

  // NOTE: We ignore literal associated to var != value.
  for (const auto [var, value] : encoder_->GetEqualityLiterals(decision)) {
    if (integer_trail_->IsCurrentlyIgnored(var)) continue;
    {
      PseudoCosts::VariableBoundChange var_bound_change;
      var_bound_change.var = var;
      var_bound_change.lower_bound_change =
          value - integer_trail_->LowerBound(var);
      bound_changes.push_back(var_bound_change);
    }

    // Also do the negation.
    {
      PseudoCosts::VariableBoundChange var_bound_change;
      var_bound_change.var = NegationOf(var);
      var_bound_change.lower_bound_change =
          (-value) - integer_trail_->LowerBound(NegationOf(var));
      bound_changes.push_back(var_bound_change);
    }
  }

  return bound_changes;
}

}  // namespace sat
}  // namespace operations_research
