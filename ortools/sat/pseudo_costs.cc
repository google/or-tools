// Copyright 2010-2021 Google LLC
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

#include <cmath>
#include <vector>

#include "ortools/sat/integer.h"
#include "ortools/sat/sat_decision.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/util.h"

namespace operations_research {
namespace sat {

PseudoCosts::PseudoCosts(Model* model)
    : integer_trail_(*model->GetOrCreate<IntegerTrail>()),
      parameters_(*model->GetOrCreate<SatParameters>()) {
  const int num_vars = integer_trail_.NumIntegerVariables().value();
  pseudo_costs_.resize(num_vars);
}

void PseudoCosts::UpdateCostForVar(IntegerVariable var, double new_cost) {
  if (var >= pseudo_costs_.size()) {
    // Create space for new variable and its negation.
    const int new_size = std::max(var, NegationOf(var)).value() + 1;
    pseudo_costs_.resize(new_size, IncrementalAverage(0.0));
  }
  DCHECK_LT(var, pseudo_costs_.size());
  pseudo_costs_[var].AddData(new_cost);
}

void PseudoCosts::UpdateCost(
    const std::vector<VariableBoundChange>& bound_changes,
    const IntegerValue obj_bound_improvement) {
  DCHECK_GE(obj_bound_improvement, 0);
  if (obj_bound_improvement == IntegerValue(0)) return;

  for (const VariableBoundChange& decision : bound_changes) {
    if (integer_trail_.IsCurrentlyIgnored(decision.var)) continue;
    if (decision.lower_bound_change == IntegerValue(0)) continue;

    const double current_pseudo_cost =
        ToDouble(obj_bound_improvement) / ToDouble(decision.lower_bound_change);
    UpdateCostForVar(decision.var, current_pseudo_cost);
  }
}

IntegerVariable PseudoCosts::GetBestDecisionVar() {
  if (pseudo_costs_.empty()) return kNoIntegerVariable;

  const double epsilon = 1e-6;

  double best_cost = -std::numeric_limits<double>::infinity();
  IntegerVariable chosen_var = kNoIntegerVariable;

  for (IntegerVariable positive_var(0); positive_var < pseudo_costs_.size();
       positive_var += 2) {
    const IntegerVariable negative_var = NegationOf(positive_var);
    if (integer_trail_.IsCurrentlyIgnored(positive_var)) continue;
    const IntegerValue lb = integer_trail_.LowerBound(positive_var);
    const IntegerValue ub = integer_trail_.UpperBound(positive_var);
    if (lb >= ub) continue;
    if (GetRecordings(positive_var) + GetRecordings(negative_var) <
        parameters_.pseudo_cost_reliability_threshold()) {
      continue;
    }

    // TODO(user): Experiment with different ways to merge the costs.
    const double current_merged_cost =
        std::max(GetCost(positive_var), epsilon) *
        std::max(GetCost(negative_var), epsilon);

    if (current_merged_cost > best_cost) {
      chosen_var = positive_var;
      best_cost = current_merged_cost;
    }
  }

  // Pick the direction with best pseudo cost.
  if (chosen_var != kNoIntegerVariable &&
      GetCost(chosen_var) < GetCost(NegationOf(chosen_var))) {
    chosen_var = NegationOf(chosen_var);
  }
  return chosen_var;
}

std::vector<PseudoCosts::VariableBoundChange> GetBoundChanges(
    LiteralIndex decision, Model* model) {
  std::vector<PseudoCosts::VariableBoundChange> bound_changes;
  if (decision == kNoLiteralIndex) return bound_changes;
  auto* encoder = model->GetOrCreate<IntegerEncoder>();
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  // NOTE: We ignore negation of equality decisions.
  for (const IntegerLiteral l :
       encoder->GetAllIntegerLiterals(Literal(decision))) {
    if (l.var == kNoIntegerVariable) continue;
    if (integer_trail->IsCurrentlyIgnored(l.var)) continue;
    PseudoCosts::VariableBoundChange var_bound_change;
    var_bound_change.var = l.var;
    var_bound_change.lower_bound_change =
        l.bound - integer_trail->LowerBound(l.var);
    bound_changes.push_back(var_bound_change);
  }

  return bound_changes;
}

}  // namespace sat
}  // namespace operations_research
