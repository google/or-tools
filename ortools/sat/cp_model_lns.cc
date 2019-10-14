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

namespace operations_research {
namespace sat {

NeighborhoodGeneratorHelper::NeighborhoodGeneratorHelper(
    int id, CpModelProto const* model_proto, SatParameters const* parameters,
    SharedResponseManager* shared_response, SharedTimeLimit* shared_time_limit,
    SharedBoundsManager* shared_bounds)
    : SubSolver(id, "helper"),
      parameters_(*parameters),
      model_proto_(*model_proto),
      shared_time_limit_(shared_time_limit),
      shared_bounds_(shared_bounds),
      shared_response_(shared_response) {
  CHECK(shared_response_ != nullptr);
  *model_proto_with_only_variables_.mutable_variables() =
      model_proto_.variables();
  RecomputeHelperData();
  Synchronize();
}

void NeighborhoodGeneratorHelper::Synchronize() {
  absl::MutexLock mutex_lock(&mutex_);
  if (shared_bounds_ != nullptr) {
    std::vector<int> model_variables;
    std::vector<int64> new_lower_bounds;
    std::vector<int64> new_upper_bounds;
    shared_bounds_->GetChangedBounds(id_, &model_variables, &new_lower_bounds,
                                     &new_upper_bounds);

    for (int i = 0; i < model_variables.size(); ++i) {
      const int var = model_variables[i];
      const int64 new_lb = new_lower_bounds[i];
      const int64 new_ub = new_upper_bounds[i];
      if (VLOG_IS_ON(3)) {
        const auto& domain =
            model_proto_with_only_variables_.variables(var).domain();
        const int64 old_lb = domain.Get(0);
        const int64 old_ub = domain.Get(domain.size() - 1);
        VLOG(3) << "Variable: " << var << " old domain: [" << old_lb << ", "
                << old_ub << "] new domain: [" << new_lb << ", " << new_ub
                << "]";
      }
      const Domain old_domain =
          ReadDomainFromProto(model_proto_with_only_variables_.variables(var));
      const Domain new_domain =
          old_domain.IntersectionWith(Domain(new_lb, new_ub));
      if (new_domain.IsEmpty()) {
        shared_response_->NotifyThatImprovingProblemIsInfeasible(
            "LNS base problem");
        if (shared_time_limit_ != nullptr) shared_time_limit_->Stop();
        return;
      }
      FillDomainInProto(
          new_domain, model_proto_with_only_variables_.mutable_variables(var));
    }

    // Only trigger the computation if needed.
    if (!model_variables.empty()) {
      RecomputeHelperData();
    }
  }
  shared_response_->MutableSolutionsRepository()->Synchronize();
}

void NeighborhoodGeneratorHelper::RecomputeHelperData() {
  // Recompute all the data in case new variables have been fixed.
  var_to_constraint_.assign(model_proto_.variables_size(), {});
  constraint_to_var_.assign(model_proto_.constraints_size(), {});
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

  type_to_constraints_.clear();
  const int num_constraints = model_proto_.constraints_size();
  for (int c = 0; c < num_constraints; ++c) {
    const int type = model_proto_.constraints(c).constraint_case();
    if (type >= type_to_constraints_.size()) {
      type_to_constraints_.resize(type + 1);
    }
    type_to_constraints_[type].push_back(c);
  }

  bool add_all_variables = true;
  active_variables_set_.assign(model_proto_.variables_size(), false);
  if (parameters_.lns_focus_on_decision_variables()) {
    for (const auto& search_strategy : model_proto_.search_strategy()) {
      for (const int var : search_strategy.variables()) {
        const int pos_var = PositiveRef(var);
        if (!active_variables_set_[pos_var] && !IsConstant(pos_var)) {
          active_variables_set_[pos_var] = true;
          active_variables_.push_back(pos_var);
        }
      }
    }

    // Revert to no focus if active_variables_ is empty().
    if (!active_variables_.empty()) add_all_variables = false;
  }
  if (add_all_variables) {
    for (int i = 0; i < model_proto_.variables_size(); ++i) {
      if (!IsConstant(i)) {
        active_variables_.push_back(i);
        active_variables_set_[i] = true;
      }
    }
  }
}

bool NeighborhoodGeneratorHelper::IsActive(int var) const {
  return active_variables_set_[var];
}

bool NeighborhoodGeneratorHelper::IsConstant(int var) const {
  return model_proto_with_only_variables_.variables(var).domain_size() == 2 &&
         model_proto_with_only_variables_.variables(var).domain(0) ==
             model_proto_with_only_variables_.variables(var).domain(1);
}

Neighborhood NeighborhoodGeneratorHelper::FullNeighborhood() const {
  Neighborhood neighborhood;
  neighborhood.is_reduced = false;
  neighborhood.is_generated = true;
  neighborhood.cp_model = model_proto_;
  *neighborhood.cp_model.mutable_variables() =
      model_proto_with_only_variables_.variables();
  return neighborhood;
}

Neighborhood NeighborhoodGeneratorHelper::FixGivenVariables(
    const CpSolverResponse& initial_solution,
    const std::vector<int>& variables_to_fix) const {
  // TODO(user): Do not include constraint with all fixed variables to save
  // memory and speed-up LNS presolving.
  Neighborhood neighborhood = FullNeighborhood();

  neighborhood.is_reduced = !variables_to_fix.empty();
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

bool NeighborhoodGenerator::ReadyToGenerate() const {
  return (helper_.shared_response().SolutionsRepository().NumSolutions() > 0);
}

double NeighborhoodGenerator::GetUCBScore(int64 total_num_calls) const {
  absl::MutexLock mutex_lock(&mutex_);
  DCHECK_GE(total_num_calls, num_calls_);
  if (num_calls_ <= 10) return std::numeric_limits<double>::infinity();
  return current_average_ + sqrt((2 * log(total_num_calls)) / num_calls_);
}

void NeighborhoodGenerator::Synchronize() {
  absl::MutexLock mutex_lock(&mutex_);

  // To make the whole update process deterministic, we currently sort the
  // SolveData.
  std::sort(solve_data_.begin(), solve_data_.end());

  // This will be used to update the difficulty of this neighborhood.
  int num_fully_solved_in_batch = 0;
  int num_not_fully_solved_in_batch = 0;

  for (const SolveData& data : solve_data_) {
    ++num_calls_;

    // INFEASIBLE or OPTIMAL means that we "fully solved" the local problem.
    // If we didn't, then we cannot be sure that there is no improving solution
    // in that neighborhood.
    if (data.status == CpSolverStatus::INFEASIBLE ||
        data.status == CpSolverStatus::OPTIMAL) {
      ++num_fully_solved_calls_;
      ++num_fully_solved_in_batch;
    } else {
      ++num_not_fully_solved_in_batch;
    }

    // It seems to make more sense to compare the new objective to the base
    // solution objective, not the best one. However this causes issue in the
    // logic below because on some problems the neighborhood can always lead
    // to a better "new objective" if the base solution wasn't the best one.
    //
    // This might not be a final solution, but it does work ok for now.
    const IntegerValue best_objective_improvement =
        data.initial_best_objective - data.new_objective;
    if (best_objective_improvement > 0) {
      num_consecutive_non_improving_calls_ = 0;
    } else {
      ++num_consecutive_non_improving_calls_;
    }

    // TODO(user): Weight more recent data.
    // degrade the current average to forget old learnings.
    const double gain_per_time_unit =
        std::max(0.0, static_cast<double>(best_objective_improvement.value())) /
        (1.0 + data.deterministic_time);
    if (num_calls_ <= 100) {
      current_average_ += (gain_per_time_unit - current_average_) / num_calls_;
    } else {
      current_average_ = 0.9 * current_average_ + 0.1 * gain_per_time_unit;
    }

    deterministic_time_ += data.deterministic_time;
  }

  // Update the difficulty.
  difficulty_.Update(/*num_decreases=*/num_not_fully_solved_in_batch,
                     /*num_increases=*/num_fully_solved_in_batch);

  // Bump the time limit if we saw no better solution in the last few calls.
  // This means that as the search progress, we likely spend more and more time
  // trying to solve individual neighborhood.
  //
  // TODO(user): experiment with resetting the time limit if a solution is
  // found.
  if (num_consecutive_non_improving_calls_ > 20) {
    num_consecutive_non_improving_calls_ = 0;
    deterministic_limit_ *= 1.1;

    // We do not want the limit to go to high. Intuitively, the goal is to try
    // out a lot of neighborhoods, not just spend a lot of time on a few.
    deterministic_limit_ = std::min(60.0, deterministic_limit_);
  }

  solve_data_.clear();
}

namespace {

template <class Random>
void GetRandomSubset(double relative_size, std::vector<int>* base,
                     Random* random) {
  // TODO(user): we could generate this more efficiently than using random
  // shuffle.
  std::shuffle(base->begin(), base->end(), *random);
  const int target_size = std::round(relative_size * base->size());
  base->resize(target_size);
}

}  // namespace

Neighborhood SimpleNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, double difficulty,
    random_engine_t* random) const {
  std::vector<int> fixed_variables = helper_.ActiveVariables();
  GetRandomSubset(1.0 - difficulty, &fixed_variables, random);
  return helper_.FixGivenVariables(initial_solution, fixed_variables);
}

Neighborhood VariableGraphNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, double difficulty,
    random_engine_t* random) const {
  const int num_active_vars = helper_.ActiveVariables().size();
  const int num_model_vars = helper_.ModelProto().variables_size();
  const int target_size = std::ceil(difficulty * num_active_vars);
  if (target_size == num_active_vars) {
    return helper_.FullNeighborhood();
  }
  CHECK_GT(target_size, 0);

  std::uniform_int_distribution<int> random_var(0, num_active_vars - 1);
  std::vector<bool> visited_variables_set(num_model_vars, false);
  std::vector<int> relaxed_variables;
  std::vector<int> visited_variables;

  const int first_var = helper_.ActiveVariables()[random_var(*random)];
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
    std::shuffle(random_variables.begin(), random_variables.end(), *random);
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
    const CpSolverResponse& initial_solution, double difficulty,
    random_engine_t* random) const {
  const int num_active_vars = helper_.ActiveVariables().size();
  const int num_model_vars = helper_.ModelProto().variables_size();
  const int target_size = std::ceil(difficulty * num_active_vars);
  const int num_constraints = helper_.ConstraintToVar().size();
  if (num_constraints == 0 || target_size == num_active_vars) {
    return helper_.FullNeighborhood();
  }
  CHECK_GT(target_size, 0);

  std::vector<bool> visited_variables_set(num_model_vars, false);
  std::vector<int> relaxed_variables;
  std::vector<bool> added_constraints(num_constraints, false);
  std::vector<int> next_constraints;

  // Start by a random constraint.
  {
    std::uniform_int_distribution<int> random_start(0, num_constraints - 1);
    next_constraints.push_back(random_start(*random));
    added_constraints[next_constraints.back()] = true;
  }

  std::vector<int> random_variables;
  while (relaxed_variables.size() < target_size) {
    // Stop if we have a full connected component.
    if (next_constraints.empty()) break;

    // Pick a random unprocessed constraint.
    std::uniform_int_distribution<int> random_constraint(
        0, next_constraints.size() - 1);
    const int i = random_constraint(*random);
    const int contraint_index = next_constraints[i];
    std::swap(next_constraints[i], next_constraints.back());
    next_constraints.pop_back();

    // Add all the variable of this constraint and increase the set of next
    // possible constraints.
    CHECK_LT(contraint_index, num_constraints);
    random_variables = helper_.ConstraintToVar()[contraint_index];
    std::shuffle(random_variables.begin(), random_variables.end(), *random);
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
  Neighborhood neighborhood = helper.FullNeighborhood();
  neighborhood.is_reduced =
      (intervals_to_relax.size() <
       helper.TypeToConstraints(ConstraintProto::kInterval).size());

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
  neighborhood.is_generated = true;

  return neighborhood;
}

Neighborhood SchedulingNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, double difficulty,
    random_engine_t* random) const {
  const auto span = helper_.TypeToConstraints(ConstraintProto::kInterval);
  std::vector<int> intervals_to_relax(span.begin(), span.end());
  GetRandomSubset(difficulty, &intervals_to_relax, random);

  return GenerateSchedulingNeighborhoodForRelaxation(intervals_to_relax,
                                                     initial_solution, helper_);
}

Neighborhood SchedulingTimeWindowNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, double difficulty,
    random_engine_t* random) const {
  std::vector<std::pair<int64, int>> start_interval_pairs;
  for (const int i : helper_.TypeToConstraints(ConstraintProto::kInterval)) {
    const ConstraintProto& interval_ct = helper_.ModelProto().constraints(i);

    const int start_var = interval_ct.interval().start();
    const int64 start_value = initial_solution.solution(start_var);
    start_interval_pairs.push_back({start_value, i});
  }
  std::sort(start_interval_pairs.begin(), start_interval_pairs.end());
  const int relaxed_size = std::floor(difficulty * start_interval_pairs.size());

  std::uniform_int_distribution<int> random_var(
      0, start_interval_pairs.size() - relaxed_size - 1);
  const int random_start_index = random_var(*random);
  std::vector<int> intervals_to_relax;
  // TODO(user,user): Consider relaxing more than one time window intervals.
  // This seems to help with Giza models.
  for (int i = random_start_index; i < relaxed_size; ++i) {
    intervals_to_relax.push_back(start_interval_pairs[i].second);
  }
  return GenerateSchedulingNeighborhoodForRelaxation(intervals_to_relax,
                                                     initial_solution, helper_);
}

bool RelaxationInducedNeighborhoodGenerator::ReadyToGenerate() const {
  SharedRINSNeighborhoodManager* rins_manager =
      model_->Mutable<SharedRINSNeighborhoodManager>();
  CHECK(rins_manager != nullptr);
  return rins_manager->HasUnexploredNeighborhood();
}

Neighborhood RelaxationInducedNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, double difficulty,
    random_engine_t* random) const {
  Neighborhood neighborhood = helper_.FullNeighborhood();
  neighborhood.is_generated = false;

  SharedRINSNeighborhoodManager* rins_manager =
      model_->Mutable<SharedRINSNeighborhoodManager>();
  if (rins_manager == nullptr) {
    return neighborhood;
  }
  absl::optional<RINSNeighborhood> rins_neighborhood_opt =
      rins_manager->GetUnexploredNeighborhood();

  if (!rins_neighborhood_opt.has_value()) {
    return neighborhood;
  }

  // Fix the variables in the local model.
  for (const std::pair<RINSVariable, int64> fixed_var :
       rins_neighborhood_opt.value().fixed_vars) {
    const int var = fixed_var.first.model_var;
    const int64 value = fixed_var.second;
    if (var >= neighborhood.cp_model.variables_size()) continue;
    if (!helper_.IsActive(var)) continue;

    const Domain domain =
        ReadDomainFromProto(neighborhood.cp_model.variables(var));
    if (!domain.Contains(value)) {
      // TODO(user): Instead of aborting, pick the closest point in the domain?
      return neighborhood;
    }

    neighborhood.cp_model.mutable_variables(var)->clear_domain();
    neighborhood.cp_model.mutable_variables(var)->add_domain(value);
    neighborhood.cp_model.mutable_variables(var)->add_domain(value);
    neighborhood.is_reduced = true;
  }

  for (const std::pair<RINSVariable, /*domain*/ std::pair<int64, int64>>
           reduced_var : rins_neighborhood_opt.value().reduced_domain_vars) {
    const int var = reduced_var.first.model_var;
    const int64 lb = reduced_var.second.first;
    const int64 ub = reduced_var.second.second;
    if (var >= neighborhood.cp_model.variables_size()) continue;
    if (!helper_.IsActive(var)) continue;
    Domain domain = ReadDomainFromProto(neighborhood.cp_model.variables(var));
    domain = domain.IntersectionWith(Domain(lb, ub));
    if (domain.IsEmpty()) {
      // TODO(user): Instead of aborting, pick the closest point in the domain?
      return neighborhood;
    }
    FillDomainInProto(domain, neighborhood.cp_model.mutable_variables(var));
    neighborhood.is_reduced = true;
  }
  neighborhood.is_generated = true;
  return neighborhood;
}

}  // namespace sat
}  // namespace operations_research
