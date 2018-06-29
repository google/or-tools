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
namespace {

// Returns a CpModelProto where the variables at given position where fixed to
// the value they take in the given response.
CpModelProto FixGivenPosition(const CpSolverResponse& response,
                              const std::vector<int> variables_to_fix,
                              CpModelProto model_proto) {
  CHECK_EQ(response.solution_size(), model_proto.variables_size());
  for (const int var : variables_to_fix) {
    model_proto.mutable_variables(var)->clear_domain();
    model_proto.mutable_variables(var)->add_domain(response.solution(var));
    model_proto.mutable_variables(var)->add_domain(response.solution(var));
  }

  // Set the current solution as a hint.
  model_proto.clear_solution_hint();
  for (int var = 0; var < model_proto.variables_size(); ++var) {
    model_proto.mutable_solution_hint()->add_vars(var);
    model_proto.mutable_solution_hint()->add_values(response.solution(var));
  }

  // TODO(user): force better objective? Note that this is already done when the
  // hint above is sucessfully loaded (i.e. if it passes the presolve correctly)
  // since the solver will try to find better solution than the current one.
  return model_proto;
}

}  // namespace

// Base class constructor.
NeighborhoodGenerator::NeighborhoodGenerator(CpModelProto const* model,
                                             bool focus_on_decision_variables,
                                             const std::string& name)
    : model_proto_(*model), name_(name) {
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
        if (!active_variables_set_[var] && !IsConstant(var)) {
          active_variables_set_[var] = true;
          active_variables_.push_back(var);
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

bool NeighborhoodGenerator::IsActive(int var) const {
  return active_variables_set_[var];
}

bool NeighborhoodGenerator::IsConstant(int var) const {
  return model_proto_.variables(var).domain_size() == 2 &&
         model_proto_.variables(var).domain(0) ==
             model_proto_.variables(var).domain(1);
}

CpModelProto NeighborhoodGenerator::RelaxGivenVariables(
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
  return FixGivenPosition(initial_solution, fixed_variables, model_proto_);
}

CpModelProto SimpleNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, int64 seed,
    double difficulty) const {
  random_engine_t random;
  random.seed(seed);

  // TODO(user): we could generate this more efficiently than using random
  // shuffle.
  std::vector<int> fixed_variables = active_variables_;

  std::shuffle(fixed_variables.begin(), fixed_variables.end(), random);
  const int target_size =
      std::ceil((1.0 - difficulty) * fixed_variables.size());
  fixed_variables.resize(target_size);
  return FixGivenPosition(initial_solution, fixed_variables, model_proto_);
}

CpModelProto VariableGraphNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, int64 seed,
    double difficulty) const {
  const int num_active_vars = active_variables_.size();
  const int num_model_vars = model_proto_.variables_size();
  const int target_size = std::ceil(difficulty * num_active_vars);
  if (target_size == num_active_vars) return model_proto_;
  CHECK_GT(target_size, 0);

  random_engine_t random;
  random.seed(seed);

  std::uniform_int_distribution<int> random_var(0, num_active_vars - 1);
  std::vector<bool> visited_variables_set(num_model_vars, false);
  std::vector<int> relaxed_variables;
  std::vector<int> visited_variables;

  const int first_var = active_variables_[random_var(random)];
  visited_variables_set[first_var] = true;
  visited_variables.push_back(first_var);
  relaxed_variables.push_back(first_var);

  std::vector<int> random_variables;
  for (int i = 0; i < visited_variables.size(); ++i) {
    random_variables.clear();
    // Collect all the variables that appears in the same constraints as
    // visited_variables[i].
    for (const int ct : var_to_constraint_[visited_variables[i]]) {
      for (const int var : constraint_to_var_[ct]) {
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
        if (IsActive(var)) {
          relaxed_variables.push_back(var);
        }
      } else {
        break;
      }
    }
    if (relaxed_variables.size() >= target_size) break;
  }

  return RelaxGivenVariables(initial_solution, relaxed_variables);
}

CpModelProto ConstraintGraphNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, int64 seed,
    double difficulty) const {
  const int num_active_vars = active_variables_.size();
  const int num_model_vars = model_proto_.variables_size();
  const int target_size = std::ceil(difficulty * num_active_vars);
  if (target_size == num_active_vars) return model_proto_;
  CHECK_GT(target_size, 0);

  random_engine_t random;
  random.seed(seed);

  std::vector<bool> visited_variables_set(num_model_vars, false);
  std::vector<int> relaxed_variables;
  std::vector<bool> added_constraints(constraint_to_var_.size(), false);
  std::vector<int> next_constraints;

  // Start by a random constraint.
  {
    std::uniform_int_distribution<int> random_start(
        0, constraint_to_var_.size() - 1);
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
    CHECK_LT(contraint_index, constraint_to_var_.size());
    random_variables = constraint_to_var_[contraint_index];
    std::shuffle(random_variables.begin(), random_variables.end(), random);
    for (const int var : random_variables) {
      if (visited_variables_set[var]) continue;
      visited_variables_set[var] = true;
      if (IsActive(var)) {
        relaxed_variables.push_back(var);
      }
      if (relaxed_variables.size() == target_size) break;

      for (const int ct : var_to_constraint_[var]) {
        if (added_constraints[ct]) continue;
        added_constraints[ct] = true;
        next_constraints.push_back(ct);
      }
    }
  }
  return RelaxGivenVariables(initial_solution, relaxed_variables);
}

CpModelProto ObjectiveGraphNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, int64 seed,
    double difficulty) const {
  const int num_active_vars = active_variables_.size();
  const int num_model_vars = model_proto_.variables_size();
  const int target_size = std::ceil(difficulty * num_active_vars);
  if (target_size == num_active_vars) return model_proto_;
  CHECK_GT(target_size, 0);

  random_engine_t random;
  random.seed(seed);

  std::vector<bool> visited_variables_set(num_model_vars, false);
  std::vector<bool> visited_constraints_set(constraint_to_var_.size(), false);

  std::vector<int> visited_variables;
  std::vector<int> relaxed_variables;

  std::vector<int> objective_variables(model_proto_.objective().vars().begin(),
                                       model_proto_.objective().vars().end());
  std::shuffle(objective_variables.begin(), objective_variables.end(), random);


  while (!objective_variables.empty() &&
         relaxed_variables.size() < target_size ) {
    const int obj_var = PositiveRef(objective_variables.back());
    objective_variables.pop_back();

    visited_variables_set[obj_var] = true;
    visited_variables.push_back(obj_var);
    relaxed_variables.push_back(obj_var);

    std::vector<int> random_variables;
    for (int i = 0; i < visited_variables.size(); ++i) {
      random_variables.clear();
      // Collect all the variables that appears in the same constraints as
      // visited_variables[i].
      std::vector<int> constraints = var_to_constraint_[visited_variables[i]];
      std::shuffle(constraints.begin(), constraints.end(), random);
      for (const int ct : constraints) {
        if (visited_constraints_set[ct]) continue;
        visited_constraints_set[ct] = true;
        std::vector<int> ct_vars = constraint_to_var_[ct];
        std::shuffle(ct_vars.begin(), ct_vars.end(), random);
        int credit = max_variables_per_constraint_;
        for (const int var : ct_vars) {
          if (visited_variables_set[var]) continue;
          visited_variables_set[var] = true;
          random_variables.push_back(var);
          --credit;
          if (credit == 0) break;
        }
      }
      // Always randomize to make sure we do not always visit constraints in the
      // same order.
      std::shuffle(random_variables.begin(), random_variables.end(), random);
      for (const int var : random_variables) {
        if (relaxed_variables.size() < target_size) {
          if (IsActive(var)) {
            relaxed_variables.push_back(var);
          }
        } else {  // We have reached quota. Break out of loop.
          break;
        }
      }
      if (relaxed_variables.size() >= target_size) break;
    }
  }

  return RelaxGivenVariables(initial_solution, relaxed_variables);
}


}  // namespace sat
}  // namespace operations_research
