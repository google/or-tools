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
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/util.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

// We prefer the product to combine the cost of two branches.
double PseudoCosts::CombineScores(double down_branch, double up_branch) const {
  if (true) {
    return std::max(1e-6, down_branch) * std::max(1e-6, up_branch);
  } else {
    const double min_value = std::min(up_branch, down_branch);
    const double max_value = std::max(up_branch, down_branch);
    const double mu = 1.0 / 6.0;
    return (1.0 - mu) * min_value + mu * max_value;
  }
}

std::string PseudoCosts::ObjectiveInfo::DebugString() const {
  return absl::StrCat("lb: ", lb, " ub:", ub, " lp_bound:", lp_bound);
}

PseudoCosts::PseudoCosts(Model* model)
    : parameters_(*model->GetOrCreate<SatParameters>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      encoder_(model->GetOrCreate<IntegerEncoder>()),
      lp_values_(model->GetOrCreate<ModelLpValues>()),
      lps_(model->GetOrCreate<LinearProgrammingConstraintCollection>()) {
  const int num_vars = integer_trail_->NumIntegerVariables().value();
  pseudo_costs_.resize(num_vars);
  is_relevant_.resize(num_vars, false);
  scores_.resize(num_vars, 0.0);

  // If objective_var == kNoIntegerVariable, there is not really any point using
  // this class.
  auto* objective = model->Get<ObjectiveDefinition>();
  if (objective != nullptr) {
    objective_var_ = objective->objective_var;
  }
}

PseudoCosts::ObjectiveInfo PseudoCosts::GetCurrentObjectiveInfo() {
  ObjectiveInfo result;
  if (objective_var_ == kNoIntegerVariable) return result;

  result.lb = integer_trail_->LowerBound(objective_var_);
  result.ub = integer_trail_->UpperBound(objective_var_);

  // We sum the objectives over the LP components.
  // Note that in practice, when we use the pseudo-costs, there is just one.
  result.lp_bound = 0.0;
  result.lp_at_optimal = true;
  for (const auto* lp : *lps_) {
    if (!lp->AtOptimal()) result.lp_at_optimal = false;
    result.lp_bound += lp->ObjectiveLpLowerBound();
  }
  return result;
}

bool PseudoCosts::SaveLpInfo() {
  saved_info_ = GetCurrentObjectiveInfo();
  return saved_info_.lp_at_optimal;
}

void PseudoCosts::SaveBoundChanges(Literal decision,
                                   absl::Span<const double> lp_values) {
  bound_changes_.clear();
  for (const IntegerLiteral l : encoder_->GetIntegerLiterals(decision)) {
    PseudoCosts::VariableBoundChange entry;
    entry.var = l.var;
    entry.lower_bound_change = l.bound - integer_trail_->LowerBound(l.var);
    if (l.var < lp_values.size()) {
      entry.lp_increase =
          std::max(0.0, ToDouble(l.bound) - lp_values[l.var.value()]);
    }
    bound_changes_.push_back(entry);
  }

  // NOTE: We ignore literal associated to var != value.
  for (const auto [var, value] : encoder_->GetEqualityLiterals(decision)) {
    {
      PseudoCosts::VariableBoundChange entry;
      entry.var = var;
      entry.lower_bound_change = value - integer_trail_->LowerBound(var);
      bound_changes_.push_back(entry);
    }

    // Also do the negation.
    {
      PseudoCosts::VariableBoundChange entry;
      entry.var = NegationOf(var);
      entry.lower_bound_change =
          (-value) - integer_trail_->LowerBound(NegationOf(var));
      bound_changes_.push_back(entry);
    }
  }
}

void PseudoCosts::BeforeTakingDecision(Literal decision) {
  if (objective_var_ == kNoIntegerVariable) return;
  SaveLpInfo();
  SaveBoundChanges(decision, *lp_values_);
}

PseudoCosts::BranchingInfo PseudoCosts::EvaluateVar(
    IntegerVariable var, absl::Span<const double> lp_values) {
  DCHECK_NE(var, kNoIntegerVariable);
  BranchingInfo result;
  const IntegerValue lb = integer_trail_->LowerBound(var);
  const IntegerValue ub = integer_trail_->UpperBound(var);
  if (lb == ub) {
    result.is_fixed = true;
    return result;
  }

  const double lp_value = lp_values[var.value()];
  double down_fractionality = lp_value - std::floor(lp_value);
  IntegerValue down_target = IntegerValue(std::floor(lp_value));
  if (lp_value >= ToDouble(ub)) {
    down_fractionality = 1.0;
    down_target = ub - 1;
  } else if (lp_value <= ToDouble(lb)) {
    down_fractionality = 0.0;
    down_target = lb;
  }

  result.is_integer = std::abs(lp_value - std::round(lp_value)) < 1e-6;
  result.down_fractionality = down_fractionality;
  result.down_branch = IntegerLiteral::LowerOrEqual(var, down_target);

  const int max_index = std::max(var.value(), NegationOf(var).value());
  if (max_index < average_unit_objective_increase_.size()) {
    result.down_score =
        down_fractionality *
        average_unit_objective_increase_[NegationOf(var)].CurrentAverage();
    result.up_score = (1.0 - down_fractionality) *
                      average_unit_objective_increase_[var].CurrentAverage();
    result.score = CombineScores(result.down_score, result.up_score);

    const int reliablitity = std::min(
        average_unit_objective_increase_[var].NumRecords(),
        average_unit_objective_increase_[NegationOf(var)].NumRecords());
    result.is_reliable = reliablitity >= 4;
  }

  return result;
}

void PseudoCosts::UpdateBoolPseudoCosts(absl::Span<const Literal> reason,
                                        IntegerValue objective_increase) {
  const double relative_increase =
      ToDouble(objective_increase) / static_cast<double>(reason.size());
  for (const Literal lit : reason) {
    if (lit.Index() >= lit_pseudo_costs_.size()) {
      lit_pseudo_costs_.resize(lit.Index() + 1);
    }
    lit_pseudo_costs_[lit].AddData(relative_increase);
  }
}

double PseudoCosts::BoolPseudoCost(Literal lit, double lp_value) const {
  if (lit.Index() >= lit_pseudo_costs_.size()) return 0.0;

  const double down_fractionality = lp_value;
  const double up_fractionality = 1.0 - lp_value;
  const double up_branch =
      up_fractionality * lit_pseudo_costs_[lit].CurrentAverage();
  const double down_branch =
      down_fractionality *
      lit_pseudo_costs_[lit.NegatedIndex()].CurrentAverage();
  return CombineScores(down_branch, up_branch);
}

double PseudoCosts::ObjectiveIncrease(bool conflict) {
  const ObjectiveInfo new_info = GetCurrentObjectiveInfo();
  const double obj_lp_diff =
      std::max(0.0, new_info.lp_bound - saved_info_.lp_bound);
  const IntegerValue obj_int_diff = new_info.lb - saved_info_.lb;

  double obj_increase =
      obj_lp_diff > 0.0 ? obj_lp_diff : ToDouble(obj_int_diff);
  if (conflict) {
    // We count a conflict as a max increase + 1.0
    obj_increase = ToDouble(saved_info_.ub) - ToDouble(saved_info_.lb) + 1.0;
  }
  return obj_increase;
}

void PseudoCosts::AfterTakingDecision(bool conflict) {
  if (objective_var_ == kNoIntegerVariable) return;
  const ObjectiveInfo new_info = GetCurrentObjectiveInfo();

  // We store a pseudo cost for this literal. We prefer the pure LP version, but
  // revert to integer version if there is no lp. TODO(user): tune that.
  //
  // We only collect lp increase when the lp is at optimal, otherwise it might
  // just be the "artificial" continuing of the current lp solve that create the
  // increase.
  if (saved_info_.lp_at_optimal) {
    // Update the average unit increases.
    const double obj_increase = ObjectiveIncrease(conflict);
    for (const auto [var, lb_change, lp_increase] : bound_changes_) {
      if (lp_increase < 1e-6) continue;
      if (var >= average_unit_objective_increase_.size()) {
        average_unit_objective_increase_.resize(var + 1);
      }
      average_unit_objective_increase_[var].AddData(obj_increase / lp_increase);
    }
  }

  // TODO(user): Handle this case.
  if (conflict) return;

  // We also store one for any associated IntegerVariable.
  const IntegerValue obj_bound_improvement =
      (new_info.lb - saved_info_.lb) + (saved_info_.ub - new_info.ub);
  DCHECK_GE(obj_bound_improvement, 0);
  if (obj_bound_improvement == IntegerValue(0)) return;

  for (const auto [var, lb_change, lp_increase] : bound_changes_) {
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
      scores_[positive_var] =
          CombineScores(GetCost(positive_var), GetCost(negative_var));
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

}  // namespace sat
}  // namespace operations_research
