// Copyright 2010-2018 Google LLC
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

#include "ortools/sat/cp_model_lns.h"

#include <numeric>
#include <vector>

#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_loader.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/rins.h"
#include "ortools/util/random_engine.h"

namespace operations_research {
namespace sat {

NeighborhoodGeneratorHelper::NeighborhoodGeneratorHelper(
    CpModelProto const* model_proto, bool focus_on_decision_variables)
    : model_proto_(*model_proto) {
  UpdateHelperData(focus_on_decision_variables);
}

void NeighborhoodGeneratorHelper::UpdateHelperData(
    bool focus_on_decision_variables) {
  var_to_constraint_.resize(model_proto_.variables_size());
  constraint_to_var_.resize(model_proto_.constraints_size());
  for (int ct_index = 0; ct_index < model_proto_.constraints_size();
       ++ct_index) {
    for (const int var : UsedVariables(model_proto_.constraints(ct_index))) {
      if (IsConstant(var)) continue;
      var_to_constraint_[var].push_back(ct_index);
      constraint_to_var_[ct_index].push_back(var);
      CHECK_GE(var, 0);
      CHECK_LT(var, model_proto_.variables_size());
    }
  }
  active_variables_set_.resize(model_proto_.variables_size(), false);

  if (focus_on_decision_variables) {
    for (const auto& search_strategy : model_proto_.search_strategy()) {
      for (const int var : search_strategy.variables()) {
        const int pos_var = PositiveRef(var);
        if (!active_variables_set_[pos_var] && !IsConstant(pos_var)) {
          active_variables_set_[pos_var] = true;
          active_variables_.push_back(pos_var);
        }
      }
    }
    if (!active_variables_.empty()) {
      // No decision variables, then no focus.
      focus_on_decision_variables = false;
    }
  }
  if (!focus_on_decision_variables) {  // Could have be set to false above.
    for (int i = 0; i < model_proto_.variables_size(); ++i) {
      if (!IsConstant(i)) {
        active_variables_.push_back(i);
        active_variables_set_[i] = true;
      }
    }
  }

  type_to_constraints_.clear();
  const int num_constraints = model_proto_.constraints_size();
  for (int c = 0; c < num_constraints; ++c) {
    const int type = model_proto_.constraints(c).constraint_case();
    CHECK_GE(type, 0) << "Negative constraint case ??";
    CHECK_LT(type, 10000) << "Large constraint case ??";
    if (type >= type_to_constraints_.size()) {
      type_to_constraints_.resize(type + 1);
    }
    type_to_constraints_[type].push_back(c);
  }
}

bool NeighborhoodGeneratorHelper::IsActive(int var) const {
  return active_variables_set_[var];
}

bool NeighborhoodGeneratorHelper::IsConstant(int var) const {
  return model_proto_.variables(var).domain_size() == 2 &&
         model_proto_.variables(var).domain(0) ==
             model_proto_.variables(var).domain(1);
}

Neighborhood NeighborhoodGeneratorHelper::FixGivenVariables(
    const CpSolverResponse& initial_solution,
    const std::vector<int>& variables_to_fix) const {
  Neighborhood neighborhood;
  neighborhood.is_reduced = !variables_to_fix.empty();
  neighborhood.cp_model = model_proto_;
  if (!neighborhood.is_reduced) return neighborhood;
  CHECK_EQ(initial_solution.solution_size(),
           neighborhood.cp_model.variables_size());
  for (const int var : variables_to_fix) {
    neighborhood.cp_model.mutable_variables(var)->clear_domain();
    neighborhood.cp_model.mutable_variables(var)->add_domain(
        initial_solution.solution(var));
    neighborhood.cp_model.mutable_variables(var)->add_domain(
        initial_solution.solution(var));
  }

  // Set the current solution as a hint.
  neighborhood.cp_model.clear_solution_hint();
  for (int var = 0; var < neighborhood.cp_model.variables_size(); ++var) {
    neighborhood.cp_model.mutable_solution_hint()->add_vars(var);
    neighborhood.cp_model.mutable_solution_hint()->add_values(
        initial_solution.solution(var));
  }

  // TODO(user): force better objective? Note that this is already done when the
  // hint above is successfully loaded (i.e. if it passes the presolve
  // correctly) since the solver will try to find better solution than the
  // current one.
  return neighborhood;
}

Neighborhood NeighborhoodGeneratorHelper::RelaxGivenVariables(
    const CpSolverResponse& initial_solution,
    const std::vector<int>& relaxed_variables) const {
  std::vector<bool> relaxed_variables_set(model_proto_.variables_size(), false);
  for (const int var : relaxed_variables) relaxed_variables_set[var] = true;
  std::vector<int> fixed_variables;
  for (const int i : active_variables_) {
    if (!relaxed_variables_set[i]) {
      fixed_variables.push_back(i);
    }
  }
  return FixGivenVariables(initial_solution, fixed_variables);
}

Neighborhood NeighborhoodGeneratorHelper::FixAllVariables(
    const CpSolverResponse& initial_solution) const {
  std::vector<int> fixed_variables;
  for (const int i : active_variables_) {
    fixed_variables.push_back(i);
  }
  return FixGivenVariables(initial_solution, fixed_variables);
}

double NeighborhoodGenerator::GetUCBScore(int64 total_num_calls) const {
  DCHECK_GE(total_num_calls, num_calls_);
  if (num_calls_ <= 10) return std::numeric_limits<double>::infinity();
  return current_average_ + sqrt((2 * log(total_num_calls)) / num_calls_);
}

void NeighborhoodGenerator::AddSolveData(double objective_diff,
                                         double deterministic_time) {
  double gain_per_time_unit = objective_diff / (1.0 + deterministic_time);
  // TODO(user): Add more data.
  // TODO(user): Weight more recent data.
  num_calls_++;
  // degrade the current average to forget old learnings.
  if (num_calls_ <= 100) {
    current_average_ += (gain_per_time_unit - current_average_) / num_calls_;
  } else {
    current_average_ = 0.9 * current_average_ + 0.1 * gain_per_time_unit;
  }
}

namespace {

void GetRandomSubset(int seed, double relative_size, std::vector<int>* base) {
  random_engine_t random;
  random.seed(seed);

  // TODO(user): we could generate this more efficiently than using random
  // shuffle.
  std::shuffle(base->begin(), base->end(), random);
  const int target_size = std::ceil(relative_size * base->size());
  base->resize(target_size);
}

}  // namespace

Neighborhood SimpleNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, int64 seed,
    double difficulty) const {
  std::vector<int> fixed_variables = helper_.ActiveVariables();
  GetRandomSubset(seed, 1.0 - difficulty, &fixed_variables);
  return helper_.FixGivenVariables(initial_solution, fixed_variables);
}

Neighborhood VariableGraphNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, int64 seed,
    double difficulty) const {
  const int num_active_vars = helper_.ActiveVariables().size();
  const int num_model_vars = helper_.ModelProto().variables_size();
  const int target_size = std::ceil(difficulty * num_active_vars);
  if (target_size == num_active_vars) {
    Neighborhood neighborhood;
    neighborhood.is_reduced = false;
    neighborhood.cp_model = helper_.ModelProto();
    return neighborhood;
  }
  CHECK_GT(target_size, 0);

  random_engine_t random;
  random.seed(seed);

  std::uniform_int_distribution<int> random_var(0, num_active_vars - 1);
  std::vector<bool> visited_variables_set(num_model_vars, false);
  std::vector<int> relaxed_variables;
  std::vector<int> visited_variables;

  const int first_var = helper_.ActiveVariables()[random_var(random)];
  visited_variables_set[first_var] = true;
  visited_variables.push_back(first_var);
  relaxed_variables.push_back(first_var);

  std::vector<int> random_variables;
  for (int i = 0; i < visited_variables.size(); ++i) {
    random_variables.clear();
    // Collect all the variables that appears in the same constraints as
    // visited_variables[i].
    for (const int ct : helper_.VarToConstraint()[visited_variables[i]]) {
      for (const int var : helper_.ConstraintToVar()[ct]) {
        if (visited_variables_set[var]) continue;
        visited_variables_set[var] = true;
        random_variables.push_back(var);
      }
    }
    // We always randomize to change the partial subgraph explored afterwards.
    std::shuffle(random_variables.begin(), random_variables.end(), random);
    for (const int var : random_variables) {
      if (relaxed_variables.size() < target_size) {
        visited_variables.push_back(var);
        if (helper_.IsActive(var)) {
          relaxed_variables.push_back(var);
        }
      } else {
        break;
      }
    }
    if (relaxed_variables.size() >= target_size) break;
  }

  return helper_.RelaxGivenVariables(initial_solution, relaxed_variables);
}

Neighborhood ConstraintGraphNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, int64 seed,
    double difficulty) const {
  const int num_active_vars = helper_.ActiveVariables().size();
  const int num_model_vars = helper_.ModelProto().variables_size();
  const int target_size = std::ceil(difficulty * num_active_vars);
  const int num_constraints = helper_.ConstraintToVar().size();
  if (num_constraints == 0 || target_size == num_active_vars) {
    Neighborhood neighborhood;
    neighborhood.is_reduced = false;
    neighborhood.cp_model = helper_.ModelProto();
    return neighborhood;
  }
  CHECK_GT(target_size, 0);

  random_engine_t random;
  random.seed(seed);

  std::vector<bool> visited_variables_set(num_model_vars, false);
  std::vector<int> relaxed_variables;
  std::vector<bool> added_constraints(num_constraints, false);
  std::vector<int> next_constraints;

  // Start by a random constraint.
  {
    std::uniform_int_distribution<int> random_start(0, num_constraints - 1);
    next_constraints.push_back(random_start(random));
    added_constraints[next_constraints.back()] = true;
  }

  std::vector<int> random_variables;
  while (relaxed_variables.size() < target_size) {
    // Stop if we have a full connected component.
    if (next_constraints.empty()) break;

    // Pick a random unprocessed constraint.
    std::uniform_int_distribution<int> random_constraint(
        0, next_constraints.size() - 1);
    const int i = random_constraint(random);
    const int contraint_index = next_constraints[i];
    std::swap(next_constraints[i], next_constraints.back());
    next_constraints.pop_back();

    // Add all the variable of this constraint and increase the set of next
    // possible constraints.
    CHECK_LT(contraint_index, num_constraints);
    random_variables = helper_.ConstraintToVar()[contraint_index];
    std::shuffle(random_variables.begin(), random_variables.end(), random);
    for (const int var : random_variables) {
      if (visited_variables_set[var]) continue;
      visited_variables_set[var] = true;
      if (helper_.IsActive(var)) {
        relaxed_variables.push_back(var);
      }
      if (relaxed_variables.size() == target_size) break;

      for (const int ct : helper_.VarToConstraint()[var]) {
        if (added_constraints[ct]) continue;
        added_constraints[ct] = true;
        next_constraints.push_back(ct);
      }
    }
  }
  return helper_.RelaxGivenVariables(initial_solution, relaxed_variables);
}

Neighborhood GenerateSchedulingNeighborhoodForRelaxation(
    const absl::Span<const int> intervals_to_relax,
    const CpSolverResponse& initial_solution,
    const NeighborhoodGeneratorHelper& helper) {
  Neighborhood neighborhood;
  neighborhood.is_reduced =
      (intervals_to_relax.size() <
       helper.TypeToConstraints(ConstraintProto::kInterval).size());
  neighborhood.cp_model = helper.ModelProto();
  // We will extend the set with some interval that we cannot fix.
  std::set<int> ignored_intervals(intervals_to_relax.begin(),
                                  intervals_to_relax.end());

  // Fix the presence/absence of non-relaxed intervals.
  for (const int i : helper.TypeToConstraints(ConstraintProto::kInterval)) {
    if (ignored_intervals.count(i)) continue;

    const ConstraintProto& interval_ct = neighborhood.cp_model.constraints(i);
    if (interval_ct.enforcement_literal().empty()) continue;

    CHECK_EQ(interval_ct.enforcement_literal().size(), 1);
    const int enforcement_ref = interval_ct.enforcement_literal(0);
    const int enforcement_var = PositiveRef(enforcement_ref);
    const int value = initial_solution.solution(enforcement_var);

    // Fix the value.
    neighborhood.cp_model.mutable_variables(enforcement_var)->clear_domain();
    neighborhood.cp_model.mutable_variables(enforcement_var)->add_domain(value);
    neighborhood.cp_model.mutable_variables(enforcement_var)->add_domain(value);

    // If the interval is ignored, skip for the loop below as there is no
    // point adding precedence on it.
    if (RefIsPositive(enforcement_ref) == (value == 0)) {
      ignored_intervals.insert(i);
    }
  }

  for (const int c : helper.TypeToConstraints(ConstraintProto::kNoOverlap)) {
    // Sort all non-relaxed intervals of this constraint by current start time.
    std::vector<std::pair<int64, int>> start_interval_pairs;
    for (const int i :
         neighborhood.cp_model.constraints(c).no_overlap().intervals()) {
      if (ignored_intervals.count(i)) continue;
      const ConstraintProto& interval_ct = neighborhood.cp_model.constraints(i);

      // TODO(user): we ignore size zero for now.
      const int size_var = interval_ct.interval().size();
      if (initial_solution.solution(size_var) == 0) continue;

      const int start_var = interval_ct.interval().start();
      const int64 start_value = initial_solution.solution(start_var);
      start_interval_pairs.push_back({start_value, i});
    }
    std::sort(start_interval_pairs.begin(), start_interval_pairs.end());

    // Add precedence between the remaining intervals, forcing their order.
    for (int i = 0; i + 1 < start_interval_pairs.size(); ++i) {
      const int before_var =
          neighborhood.cp_model.constraints(start_interval_pairs[i].second)
              .interval()
              .end();
      const int after_var =
          neighborhood.cp_model.constraints(start_interval_pairs[i + 1].second)
              .interval()
              .start();
      CHECK_LE(initial_solution.solution(before_var),
               initial_solution.solution(after_var));

      LinearConstraintProto* linear =
          neighborhood.cp_model.add_constraints()->mutable_linear();
      linear->add_domain(kint64min);
      linear->add_domain(0);
      linear->add_vars(before_var);
      linear->add_coeffs(1);
      linear->add_vars(after_var);
      linear->add_coeffs(-1);
    }
  }

  // Set the current solution as a hint.
  //
  // TODO(user): Move to common function?
  neighborhood.cp_model.clear_solution_hint();
  for (int var = 0; var < neighborhood.cp_model.variables_size(); ++var) {
    neighborhood.cp_model.mutable_solution_hint()->add_vars(var);
    neighborhood.cp_model.mutable_solution_hint()->add_values(
        initial_solution.solution(var));
  }

  return neighborhood;
}

Neighborhood SchedulingNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, int64 seed,
    double difficulty) const {
  const auto span = helper_.TypeToConstraints(ConstraintProto::kInterval);
  std::vector<int> intervals_to_relax(span.begin(), span.end());
  GetRandomSubset(seed, difficulty, &intervals_to_relax);

  return GenerateSchedulingNeighborhoodForRelaxation(intervals_to_relax,
                                                     initial_solution, helper_);
}

Neighborhood SchedulingTimeWindowNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, int64 seed,
    double difficulty) const {
  std::vector<std::pair<int64, int>> start_interval_pairs;
  for (const int i : helper_.TypeToConstraints(ConstraintProto::kInterval)) {
    const ConstraintProto& interval_ct = helper_.ModelProto().constraints(i);

    const int start_var = interval_ct.interval().start();
    const int64 start_value = initial_solution.solution(start_var);
    start_interval_pairs.push_back({start_value, i});
  }
  std::sort(start_interval_pairs.begin(), start_interval_pairs.end());
  const int relaxed_size = std::floor(difficulty * start_interval_pairs.size());
  random_engine_t random;
  random.seed(seed);

  std::uniform_int_distribution<int> random_var(
      0, start_interval_pairs.size() - relaxed_size - 1);
  const int random_start_index = random_var(random);
  std::vector<int> intervals_to_relax;
  // TODO(user,user): Consider relaxing more than one time window intervals.
  // This seems to help with Giza models.
  for (int i = random_start_index; i < relaxed_size; ++i) {
    intervals_to_relax.push_back(start_interval_pairs[i].second);
  }
  return GenerateSchedulingNeighborhoodForRelaxation(intervals_to_relax,
                                                     initial_solution, helper_);
}

Neighborhood RelaxationInducedNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, int64 seed,
    double difficulty) const {
  Neighborhood neighborhood;
  neighborhood.cp_model = helper_.ModelProto();

  const int num_model_vars = helper_.ModelProto().variables_size();
  SharedRINSNeighborhoodManager* rins_manager =
      model_->Mutable<SharedRINSNeighborhoodManager>();
  std::vector<int> all_vars;
  for (int i = 0; i < num_model_vars; ++i) {
    all_vars.push_back(i);
  }
  if (rins_manager == nullptr) {
    // TODO(user): Support skipping this neighborhood instead of going
    // through solving process for the trivial model.
    return helper_.FixAllVariables(initial_solution);
  }
  absl::optional<RINSNeighborhood> rins_neighborhood_opt =
      rins_manager->GetUnexploredNeighborhood();

  if (!rins_neighborhood_opt.has_value()) {
    return helper_.FixAllVariables(initial_solution);
  }

  // Fix the variables in the local model.
  for (const std::pair<RINSVariable, int64> fixed_var :
       rins_neighborhood_opt.value().fixed_vars) {
    int var = fixed_var.first.model_var;
    int64 value = fixed_var.second;
    if (!helper_.IsActive(var)) continue;
    neighborhood.cp_model.mutable_variables(var)->clear_domain();
    neighborhood.cp_model.mutable_variables(var)->add_domain(value);
    neighborhood.cp_model.mutable_variables(var)->add_domain(value);
    neighborhood.is_reduced = true;
  }
  return neighborhood;
}

}  // namespace sat
}  // namespace operations_research
