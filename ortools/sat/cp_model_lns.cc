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

#include <unordered_set>
#include "ortools/sat/cp_model_utils.h"
#include "ortools/util/random_engine.h"

namespace operations_research {
namespace sat {

// Base class constructor.
NeighborhoodGenerator::NeighborhoodGenerator(CpModelProto const* model)
    : model_proto_(*model) {
  var_to_constraint_.resize(model_proto_.variables_size());
  constraint_to_var_.resize(model_proto_.constraints_size());
  for (int ct_index = 0; ct_index < model_proto_.constraints_size();
       ++ct_index) {
    for (const int var : UsedVariables(model_proto_.constraints(ct_index))) {
      var_to_constraint_[var].push_back(ct_index);
      constraint_to_var_[ct_index].push_back(var);
      CHECK_GE(var, 0);
      CHECK_LT(var, model_proto_.variables_size());
    }
  }
}

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

CpModelProto SimpleNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, int64 seed,
    double difficulty) const {
  random_engine_t random;
  random.seed(seed);

  // TODO(user): we could generate this more efficiently that using random
  // shuffle.
  const int num_vars = model_proto_.variables_size();
  std::vector<int> fixed_variables(num_vars);
  for (int i = 0; i < num_vars; ++i) fixed_variables[i] = i;
  std::shuffle(fixed_variables.begin(), fixed_variables.end(), random);

  const int target_size = std::ceil((1.0 - difficulty) * num_vars);
  fixed_variables.resize(target_size);
  return FixGivenPosition(initial_solution, fixed_variables, model_proto_);
}

CpModelProto VariableGraphNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, int64 seed,
    double difficulty) const {
  const int num_variables = model_proto_.variables_size();
  const int target_size = std::ceil(difficulty * num_variables);
  if (target_size == num_variables) return model_proto_;
  CHECK_GT(target_size, 0);

  random_engine_t random;
  random.seed(seed);

  std::uniform_int_distribution<int> random_var(0, num_variables - 1);
  std::vector<bool> used_variables(num_variables, false);
  std::vector<int> relaxed_variables;

  relaxed_variables.push_back(random_var(random));
  used_variables[relaxed_variables.back()] = true;

  std::vector<int> random_variables;
  for (int i = 0; i < relaxed_variables.size(); ++i) {
    random_variables.clear();
    // Collect all the variables that appears in the same constraints as
    // relaxed_variables[i].
    for (const int ct : var_to_constraint_[relaxed_variables[i]]) {
      for (const int var : constraint_to_var_[ct]) {
        if (used_variables[var]) continue;
        used_variables[var] = true;
        random_variables.push_back(var);
      }
    }
    if (relaxed_variables.size() + random_variables.size() > target_size) {
      random_variables.resize(target_size - relaxed_variables.size());
    }
    std::shuffle(random_variables.begin(), random_variables.end(), random);
    relaxed_variables.insert(relaxed_variables.end(), random_variables.begin(),
                             random_variables.end());
    if (relaxed_variables.size() >= target_size) break;
  }

  // Compute the complement of relaxed_variables in fixed_variables.
  std::vector<int> fixed_variables;
  {
    used_variables.assign(num_variables, false);
    for (const int var : relaxed_variables) used_variables[var] = true;
    for (int var = 0; var < num_variables; ++var) {
      if (!used_variables[var]) fixed_variables.push_back(var);
    }
  }
  return FixGivenPosition(initial_solution, fixed_variables, model_proto_);
}

CpModelProto ConstraintGraphNeighborhoodGenerator::Generate(
    const CpSolverResponse& initial_solution, int64 seed,
    double difficulty) const {
  const int num_vars = model_proto_.variables_size();
  const int target_size = std::ceil(difficulty * num_vars);
  if (target_size == num_vars) return model_proto_;
  CHECK_GT(target_size, 0);

  random_engine_t random;
  random.seed(seed);

  int num_used_variables = 0;
  std::vector<bool> relaxed_variables(var_to_constraint_.size(), false);
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
  while (num_used_variables < target_size) {
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
      if (!relaxed_variables[var]) {
        ++num_used_variables;
      }
      relaxed_variables[var] = true;
      if (num_used_variables == target_size) break;
      for (const int ct : var_to_constraint_[var]) {
        if (added_constraints[ct]) continue;
        added_constraints[ct] = true;
        next_constraints.push_back(ct);
      }
    }
  }

  std::vector<int> fixed_variables;
  for (int i = 0; i < num_vars; ++i) {
    if (!relaxed_variables[i]) fixed_variables.push_back(i);
  }
  return FixGivenPosition(initial_solution, fixed_variables, model_proto_);
}

}  // namespace sat
}  // namespace operations_research
