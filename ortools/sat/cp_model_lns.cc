// Copyright 2010-2017 Google
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

#include <unordered_set>
#include "ortools/sat/cp_model_utils.h"
#include "ortools/util/random_engine.h"

namespace operations_research {
namespace sat {

NeighborhoodGeneratorHelper::NeighborhoodGeneratorHelper(
    CpModelProto const* model, bool focus_on_decision_variables)
    : model_proto_(*model) {
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
}

bool NeighborhoodGeneratorHelper::IsActive(int var) const {
  return active_variables_set_[var];
}

bool NeighborhoodGeneratorHelper::IsConstant(int var) const {
  return model_proto_.variables(var).domain_size() == 2 &&
         model_proto_.variables(var).domain(0) ==
             model_proto_.variables(var).domain(1);
}

CpModelProto NeighborhoodGeneratorHelper::FixGivenVariables(
    const CpSolverResponse& initial_solution,
    const std::vector<int>& variables_to_fix) const {
  CpModelProto result = model_proto_;
  CHECK_EQ(initial_solution.solution_size(), result.variables_size());
  for (const int var : variables_to_fix) {
    result.mutable_variables(var)->clear_domain();
    result.mutable_variables(var)->add_domain(initial_solution.solution(var));
    result.mutable_variables(var)->add_domain(initial_solution.solution(var));
  }

  // Set the current solution as a hint.
  result.clear_solution_hint();
  for (int var = 0; var < result.variables_size(); ++var) {
    result.mutable_solution_hint()->add_vars(var);
    result.mutable_solution_hint()->add_values(initial_solution.solution(var));
  }

  // TODO(user): force better objective? Note that this is already done when the
  // hint above is sucessfully loaded (i.e. if it passes the presolve correctly)
  // since the solver will try to find better solution than the current one.
  return result;
}

CpModelProto NeighborhoodGeneratorHelper::RelaxGivenVariables(
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

CpModelProto SimpleNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, int64 seed,
    double difficulty) const {
  random_engine_t random;
  random.seed(seed);

  // TODO(user): we could generate this more efficiently than using random
  // shuffle.
  std::vector<int> fixed_variables = helper_.ActiveVariables();

  std::shuffle(fixed_variables.begin(), fixed_variables.end(), random);
  const int target_size =
      std::ceil((1.0 - difficulty) * fixed_variables.size());
  fixed_variables.resize(target_size);
  return helper_.FixGivenVariables(initial_solution, fixed_variables);
}

CpModelProto VariableGraphNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, int64 seed,
    double difficulty) const {
  const int num_active_vars = helper_.ActiveVariables().size();
  const int num_model_vars = helper_.ModelProto().variables_size();
  const int target_size = std::ceil(difficulty * num_active_vars);
  if (target_size == num_active_vars) return helper_.ModelProto();
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

CpModelProto ConstraintGraphNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, int64 seed,
    double difficulty) const {
  const int num_active_vars = helper_.ActiveVariables().size();
  const int num_model_vars = helper_.ModelProto().variables_size();
  const int target_size = std::ceil(difficulty * num_active_vars);
  if (target_size == num_active_vars) return helper_.ModelProto();
  CHECK_GT(target_size, 0);

  random_engine_t random;
  random.seed(seed);

  std::vector<bool> visited_variables_set(num_model_vars, false);
  std::vector<int> relaxed_variables;
  std::vector<bool> added_constraints(helper_.ConstraintToVar().size(), false);
  std::vector<int> next_constraints;

  // Start by a random constraint.
  {
    std::uniform_int_distribution<int> random_start(
        0, helper_.ConstraintToVar().size() - 1);
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
    CHECK_LT(contraint_index, helper_.ConstraintToVar().size());
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

}  // namespace sat
}  // namespace operations_research
