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

#include "ortools/sat/sat_decision.h"

#include <cstdint>

#include "ortools/sat/util.h"

namespace operations_research {
namespace sat {

SatDecisionPolicy::SatDecisionPolicy(Model* model)
    : parameters_(*(model->GetOrCreate<SatParameters>())),
      trail_(*model->GetOrCreate<Trail>()),
      random_(model->GetOrCreate<ModelRandomGenerator>()) {}

void SatDecisionPolicy::IncreaseNumVariables(int num_variables) {
  const int old_num_variables = activities_.size();
  DCHECK_GE(num_variables, activities_.size());

  activities_.resize(num_variables, parameters_.initial_variables_activity());
  tie_breakers_.resize(num_variables, 0.0);
  num_bumps_.resize(num_variables, 0);
  pq_need_update_for_var_at_trail_index_.IncreaseSize(num_variables);

  weighted_sign_.resize(num_variables, 0.0);

  has_forced_polarity_.resize(num_variables, false);
  forced_polarity_.resize(num_variables);
  has_target_polarity_.resize(num_variables, false);
  target_polarity_.resize(num_variables);
  var_polarity_.resize(num_variables);

  ResetInitialPolarity(/*from=*/old_num_variables);

  // Update the priority queue. Note that each addition is in O(1) because
  // the priority is 0.0.
  var_ordering_.Reserve(num_variables);
  if (var_ordering_is_initialized_) {
    for (BooleanVariable var(old_num_variables); var < num_variables; ++var) {
      var_ordering_.Add({var, 0.0, activities_[var]});
    }
  }
}

void SatDecisionPolicy::BeforeConflict(int trail_index) {
  if (parameters_.use_erwa_heuristic()) {
    ++num_conflicts_;
    num_conflicts_stack_.push_back({trail_.Index(), 1});
  }

  if (trail_index > target_length_) {
    target_length_ = trail_index;
    has_target_polarity_.assign(has_target_polarity_.size(), false);
    for (int i = 0; i < trail_index; ++i) {
      const Literal l = trail_[i];
      has_target_polarity_[l.Variable()] = true;
      target_polarity_[l.Variable()] = l.IsPositive();
    }
  }

  if (trail_index > best_partial_assignment_.size()) {
    best_partial_assignment_.assign(&trail_[0], &trail_[trail_index]);
  }

  --num_conflicts_until_rephase_;
  RephaseIfNeeded();
}

void SatDecisionPolicy::RephaseIfNeeded() {
  if (parameters_.polarity_rephase_increment() <= 0) return;
  if (num_conflicts_until_rephase_ > 0) return;

  VLOG(1) << "End of polarity phase " << polarity_phase_
          << " target_length: " << target_length_
          << " best_length: " << best_partial_assignment_.size();

  ++polarity_phase_;
  num_conflicts_until_rephase_ =
      parameters_.polarity_rephase_increment() * (polarity_phase_ + 1);

  // We always reset the target each time we change phase.
  target_length_ = 0;
  has_target_polarity_.assign(has_target_polarity_.size(), false);

  // Cycle between different initial polarities. Note that we already start by
  // the default polarity, and this code is reached the first time with a
  // polarity_phase_ of 1.
  switch (polarity_phase_ % 8) {
    case 0:
      ResetInitialPolarity(/*from=*/0);
      break;
    case 1:
      UseLongestAssignmentAsInitialPolarity();
      break;
    case 2:
      ResetInitialPolarity(/*from=*/0, /*inverted=*/true);
      break;
    case 3:
      UseLongestAssignmentAsInitialPolarity();
      break;
    case 4:
      RandomizeCurrentPolarity();
      break;
    case 5:
      UseLongestAssignmentAsInitialPolarity();
      break;
    case 6:
      FlipCurrentPolarity();
      break;
    case 7:
      UseLongestAssignmentAsInitialPolarity();
      break;
  }
}

void SatDecisionPolicy::ResetDecisionHeuristic() {
  const int num_variables = activities_.size();
  variable_activity_increment_ = 1.0;
  activities_.assign(num_variables, parameters_.initial_variables_activity());
  tie_breakers_.assign(num_variables, 0.0);
  num_bumps_.assign(num_variables, 0);
  var_ordering_.Clear();

  polarity_phase_ = 0;
  num_conflicts_until_rephase_ = parameters_.polarity_rephase_increment();

  ResetInitialPolarity(/*from=*/0);
  has_target_polarity_.assign(num_variables, false);
  has_forced_polarity_.assign(num_variables, false);
  best_partial_assignment_.clear();

  num_conflicts_ = 0;
  num_conflicts_stack_.clear();

  var_ordering_is_initialized_ = false;
}

void SatDecisionPolicy::ResetInitialPolarity(int from, bool inverted) {
  // Sets the initial polarity.
  //
  // TODO(user): The WEIGHTED_SIGN one are currently slightly broken because the
  // weighted_sign_ is updated after this has been called. It requires a call
  // to ResetDecisionHeuristic() after all the constraint have been added. Fix.
  // On another hand, this is only used with SolveWithRandomParameters() that
  // does call this function.
  const int num_variables = activities_.size();
  for (BooleanVariable var(from); var < num_variables; ++var) {
    switch (parameters_.initial_polarity()) {
      case SatParameters::POLARITY_TRUE:
        var_polarity_[var] = inverted ? false : true;
        break;
      case SatParameters::POLARITY_FALSE:
        var_polarity_[var] = inverted ? true : false;
        break;
      case SatParameters::POLARITY_RANDOM:
        var_polarity_[var] = std::uniform_int_distribution<int>(0, 1)(*random_);
        break;
      case SatParameters::POLARITY_WEIGHTED_SIGN:
        var_polarity_[var] = weighted_sign_[var] > 0;
        break;
      case SatParameters::POLARITY_REVERSE_WEIGHTED_SIGN:
        var_polarity_[var] = weighted_sign_[var] < 0;
        break;
    }
  }
}

void SatDecisionPolicy::UseLongestAssignmentAsInitialPolarity() {
  // In this special case, we just overwrite partially the current fixed
  // polarity and reset the best best_partial_assignment_ for the next such
  // phase.
  for (const Literal l : best_partial_assignment_) {
    var_polarity_[l.Variable()] = l.IsPositive();
  }
  best_partial_assignment_.clear();
}

void SatDecisionPolicy::FlipCurrentPolarity() {
  const int num_variables = var_polarity_.size();
  for (BooleanVariable var; var < num_variables; ++var) {
    var_polarity_[var] = !var_polarity_[var];
  }
}

void SatDecisionPolicy::RandomizeCurrentPolarity() {
  const int num_variables = var_polarity_.size();
  for (BooleanVariable var; var < num_variables; ++var) {
    var_polarity_[var] = std::uniform_int_distribution<int>(0, 1)(*random_);
  }
}

void SatDecisionPolicy::InitializeVariableOrdering() {
  const int num_variables = activities_.size();

  // First, extract the variables without activity, and add the other to the
  // priority queue.
  var_ordering_.Clear();
  tmp_variables_.clear();
  for (BooleanVariable var(0); var < num_variables; ++var) {
    if (!trail_.Assignment().VariableIsAssigned(var)) {
      if (activities_[var] > 0.0) {
        var_ordering_.Add(
            {var, static_cast<float>(tie_breakers_[var]), activities_[var]});
      } else {
        tmp_variables_.push_back(var);
      }
    }
  }

  // Set the order of the other according to the parameters_.
  // Note that this is just a "preference" since the priority queue will kind
  // of randomize this. However, it is more efficient than using the tie_breaker
  // which add a big overhead on the priority queue.
  //
  // TODO(user): Experiment and come up with a good set of heuristics.
  switch (parameters_.preferred_variable_order()) {
    case SatParameters::IN_ORDER:
      break;
    case SatParameters::IN_REVERSE_ORDER:
      std::reverse(tmp_variables_.begin(), tmp_variables_.end());
      break;
    case SatParameters::IN_RANDOM_ORDER:
      std::shuffle(tmp_variables_.begin(), tmp_variables_.end(), *random_);
      break;
  }

  // Add the variables without activity to the queue (in the default order)
  for (const BooleanVariable var : tmp_variables_) {
    var_ordering_.Add({var, static_cast<float>(tie_breakers_[var]), 0.0});
  }

  // Finish the queue initialization.
  pq_need_update_for_var_at_trail_index_.ClearAndResize(num_variables);
  pq_need_update_for_var_at_trail_index_.SetAllBefore(trail_.Index());
  var_ordering_is_initialized_ = true;
}

void SatDecisionPolicy::SetAssignmentPreference(Literal literal,
                                                double weight) {
  if (!parameters_.use_optimization_hints()) return;
  DCHECK_GE(weight, 0.0);
  DCHECK_LE(weight, 1.0);

  has_forced_polarity_[literal.Variable()] = true;
  forced_polarity_[literal.Variable()] = literal.IsPositive();

  // The tie_breaker is changed, so we need to reinitialize the priority queue.
  // Note that this doesn't change the activity though.
  tie_breakers_[literal.Variable()] = weight;
  var_ordering_is_initialized_ = false;
}

std::vector<std::pair<Literal, double>> SatDecisionPolicy::AllPreferences()
    const {
  std::vector<std::pair<Literal, double>> prefs;
  for (BooleanVariable var(0); var < var_polarity_.size(); ++var) {
    // TODO(user): we currently assume that if the tie_breaker is zero then
    // no preference was set (which is not 100% correct). Fix that.
    const double value = var_ordering_.GetElement(var.value()).tie_breaker;
    if (value > 0.0) {
      prefs.push_back(std::make_pair(Literal(var, var_polarity_[var]), value));
    }
  }
  return prefs;
}

void SatDecisionPolicy::UpdateWeightedSign(
    const std::vector<LiteralWithCoeff>& terms, Coefficient rhs) {
  for (const LiteralWithCoeff& term : terms) {
    const double weight = static_cast<double>(term.coefficient.value()) /
                          static_cast<double>(rhs.value());
    weighted_sign_[term.literal.Variable()] +=
        term.literal.IsPositive() ? -weight : weight;
  }
}

void SatDecisionPolicy::BumpVariableActivities(
    const std::vector<Literal>& literals) {
  if (parameters_.use_erwa_heuristic()) {
    for (const Literal literal : literals) {
      // Note that we don't really need to bump level 0 variables since they
      // will never be backtracked over. However it is faster to simply bump
      // them.
      ++num_bumps_[literal.Variable()];
    }
    return;
  }

  const double max_activity_value = parameters_.max_variable_activity_value();
  for (const Literal literal : literals) {
    const BooleanVariable var = literal.Variable();
    const int level = trail_.Info(var).level;
    if (level == 0) continue;
    activities_[var] += variable_activity_increment_;
    pq_need_update_for_var_at_trail_index_.Set(trail_.Info(var).trail_index);
    if (activities_[var] > max_activity_value) {
      RescaleVariableActivities(1.0 / max_activity_value);
    }
  }
}

void SatDecisionPolicy::RescaleVariableActivities(double scaling_factor) {
  variable_activity_increment_ *= scaling_factor;
  for (BooleanVariable var(0); var < activities_.size(); ++var) {
    activities_[var] *= scaling_factor;
  }

  // When rescaling the activities of all the variables, the order of the
  // active variables in the heap will not change, but we still need to update
  // their weights so that newly inserted elements will compare correctly with
  // already inserted ones.
  //
  // IMPORTANT: we need to reset the full heap from scratch because just
  // multiplying the current weight by scaling_factor is not guaranteed to
  // preserve the order. This is because the activity of two entries may go to
  // zero and the tie-breaking ordering may change their relative order.
  //
  // InitializeVariableOrdering() will be called lazily only if needed.
  var_ordering_is_initialized_ = false;
}

void SatDecisionPolicy::UpdateVariableActivityIncrement() {
  variable_activity_increment_ *= 1.0 / parameters_.variable_activity_decay();
}

Literal SatDecisionPolicy::NextBranch() {
  // Lazily initialize var_ordering_ if needed.
  if (!var_ordering_is_initialized_) {
    InitializeVariableOrdering();
  }

  // Choose the variable.
  BooleanVariable var;
  const double ratio = parameters_.random_branches_ratio();
  auto zero_to_one = [this]() {
    return std::uniform_real_distribution<double>()(*random_);
  };
  if (ratio != 0.0 && zero_to_one() < ratio) {
    while (true) {
      // TODO(user): This may not be super efficient if almost all the
      // variables are assigned.
      std::uniform_int_distribution<int> index_dist(0,
                                                    var_ordering_.Size() - 1);
      var = var_ordering_.QueueElement(index_dist(*random_)).var;
      if (!trail_.Assignment().VariableIsAssigned(var)) break;
      pq_need_update_for_var_at_trail_index_.Set(trail_.Info(var).trail_index);
      var_ordering_.Remove(var.value());
    }
  } else {
    // The loop is done this way in order to leave the final choice in the heap.
    DCHECK(!var_ordering_.IsEmpty());
    var = var_ordering_.Top().var;
    while (trail_.Assignment().VariableIsAssigned(var)) {
      var_ordering_.Pop();
      pq_need_update_for_var_at_trail_index_.Set(trail_.Info(var).trail_index);
      DCHECK(!var_ordering_.IsEmpty());
      var = var_ordering_.Top().var;
    }
  }

  // Choose its polarity (i.e. True of False).
  const double random_ratio = parameters_.random_polarity_ratio();
  if (random_ratio != 0.0 && zero_to_one() < random_ratio) {
    return Literal(var, std::uniform_int_distribution<int>(0, 1)(*random_));
  }

  if (has_forced_polarity_[var]) return Literal(var, forced_polarity_[var]);
  if (in_stable_phase_ && has_target_polarity_[var]) {
    return Literal(var, target_polarity_[var]);
  }
  return Literal(var, var_polarity_[var]);
}

void SatDecisionPolicy::PqInsertOrUpdate(BooleanVariable var) {
  const WeightedVarQueueElement element{
      var, static_cast<float>(tie_breakers_[var]), activities_[var]};
  if (var_ordering_.Contains(var.value())) {
    // Note that the new weight should always be higher than the old one.
    var_ordering_.IncreasePriority(element);
  } else {
    var_ordering_.Add(element);
  }
}

void SatDecisionPolicy::Untrail(int target_trail_index) {
  // TODO(user): avoid looping twice over the trail?
  if (maybe_enable_phase_saving_ && parameters_.use_phase_saving()) {
    for (int i = target_trail_index; i < trail_.Index(); ++i) {
      const Literal l = trail_[i];
      var_polarity_[l.Variable()] = l.IsPositive();
    }
  }

  DCHECK_LT(target_trail_index, trail_.Index());
  if (parameters_.use_erwa_heuristic()) {
    // The ERWA parameter between the new estimation of the learning rate and
    // the old one. TODO(user): Expose parameters for these values.
    const double alpha = std::max(0.06, 0.4 - 1e-6 * num_conflicts_);

    // This counts the number of conflicts since the assignment of the variable
    // at the current trail_index that we are about to untrail.
    int num_conflicts = 0;
    int next_num_conflicts_update =
        num_conflicts_stack_.empty() ? -1
                                     : num_conflicts_stack_.back().trail_index;

    int trail_index = trail_.Index();
    while (trail_index > target_trail_index) {
      if (next_num_conflicts_update == trail_index) {
        num_conflicts += num_conflicts_stack_.back().count;
        num_conflicts_stack_.pop_back();
        next_num_conflicts_update =
            num_conflicts_stack_.empty()
                ? -1
                : num_conflicts_stack_.back().trail_index;
      }
      const BooleanVariable var = trail_[--trail_index].Variable();

      // TODO(user): This heuristic can make this code quite slow because
      // all the untrailed variable will cause a priority queue update.
      const int64_t num_bumps = num_bumps_[var];
      double new_rate = 0.0;
      if (num_bumps > 0) {
        DCHECK_GT(num_conflicts, 0);
        num_bumps_[var] = 0;
        new_rate = static_cast<double>(num_bumps) / num_conflicts;
      }
      activities_[var] = alpha * new_rate + (1 - alpha) * activities_[var];
      if (var_ordering_is_initialized_) PqInsertOrUpdate(var);
    }
    if (num_conflicts > 0) {
      if (!num_conflicts_stack_.empty() &&
          num_conflicts_stack_.back().trail_index == trail_.Index()) {
        num_conflicts_stack_.back().count += num_conflicts;
      } else {
        num_conflicts_stack_.push_back({trail_.Index(), num_conflicts});
      }
    }
  } else {
    if (!var_ordering_is_initialized_) return;

    // Trail index of the next variable that will need a priority queue update.
    int to_update = pq_need_update_for_var_at_trail_index_.Top();
    while (to_update >= target_trail_index) {
      DCHECK_LT(to_update, trail_.Index());
      PqInsertOrUpdate(trail_[to_update].Variable());
      pq_need_update_for_var_at_trail_index_.ClearTop();
      to_update = pq_need_update_for_var_at_trail_index_.Top();
    }
  }

  // Invariant.
  if (DEBUG_MODE && var_ordering_is_initialized_) {
    for (int trail_index = trail_.Index() - 1; trail_index > target_trail_index;
         --trail_index) {
      const BooleanVariable var = trail_[trail_index].Variable();
      CHECK(var_ordering_.Contains(var.value()));
      CHECK_EQ(activities_[var], var_ordering_.GetElement(var.value()).weight);
    }
  }
}

}  // namespace sat
}  // namespace operations_research
