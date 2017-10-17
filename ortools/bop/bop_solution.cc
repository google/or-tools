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

#include "ortools/bop/bop_solution.h"

namespace operations_research {
namespace bop {
//------------------------------------------------------------------------------
// BopSolution
//------------------------------------------------------------------------------
BopSolution::BopSolution(const LinearBooleanProblem& problem,
                         const std::string& name)
    : problem_(&problem),
      name_(name),
      values_(problem.num_variables(), false),
      recompute_cost_(true),
      recompute_is_feasible_(true),
      cost_(0),
      is_feasible_(false) {
  // Try the lucky assignment, i.e. the optimal one if feasible.
  const LinearObjective& objective = problem.objective();
  for (int i = 0; i < objective.coefficients_size(); ++i) {
    const VariableIndex var(objective.literals(i) - 1);
    values_[var] = objective.coefficients(i) < 0;
  }
}

int64 BopSolution::ComputeCost() const {
  recompute_cost_ = false;
  int64 sum = 0;
  const LinearObjective& objective = problem_->objective();
  const size_t num_sparse_vars = objective.literals_size();
  CHECK_EQ(num_sparse_vars, objective.coefficients_size());
  for (int i = 0; i < num_sparse_vars; ++i) {
    CHECK_GT(objective.literals(i), 0);
    const VariableIndex var(abs(objective.literals(i)) - 1);
    if (values_[var]) {
      sum += objective.coefficients(i);
    }
  }
  return sum;
}

bool BopSolution::ComputeIsFeasible() const {
  recompute_is_feasible_ = false;
  for (const LinearBooleanConstraint& constraint : problem_->constraints()) {
    int64 sum = 0;
    const size_t num_sparse_vars = constraint.literals_size();
    CHECK_EQ(num_sparse_vars, constraint.coefficients_size());

    for (int i = 0; i < num_sparse_vars; ++i) {
      // The solver doesn't support negative literals yet.
      CHECK_GT(constraint.literals(i), 0);
      const VariableIndex var(abs(constraint.literals(i)) - 1);
      if (values_[var]) {
        sum += constraint.coefficients(i);
      }
    }

    if ((constraint.has_upper_bound() && sum > constraint.upper_bound()) ||
        (constraint.has_lower_bound() && sum < constraint.lower_bound())) {
      return false;
    }
  }
  return true;
}
}  // namespace bop
}  // namespace operations_research
