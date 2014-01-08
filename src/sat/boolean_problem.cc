// Copyright 2010-2013 Google
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
#include "sat/boolean_problem.h"

#include "base/join.h"

namespace operations_research {
namespace sat {

bool LoadBooleanProblem(const LinearBooleanProblem& problem,
                        SatSolver* solver) {
  LOG(INFO) << "Loading problem '" << problem.name() << "', "
            << problem.num_variables() << " variables, "
            << problem.constraints_size() << " constraints.";
  solver->Reset(problem.num_variables());
  std::vector<LiteralWithCoeff> cst;
  int64 num_terms = 0;
  for (const LinearBooleanConstraint& constraint : problem.constraints()) {
    cst.clear();
    num_terms += constraint.literals_size();
    for (int i = 0; i < constraint.literals_size(); ++i) {
      const Literal literal(constraint.literals(i));
      if (literal.Variable() >= problem.num_variables()) {
        LOG(WARNING) << "Literal out of bound: " << literal;
        return false;
      }
      cst.push_back(LiteralWithCoeff(literal, constraint.coefficients(i)));
    }
    if (!solver->AddLinearConstraint(
             constraint.has_lower_bound(), constraint.lower_bound(),
             constraint.has_upper_bound(), constraint.upper_bound(), &cst)) {
      return false;
    }
  }
  LOG(INFO) << "The problem contains " << num_terms << " terms.";

  // Initialize the heuristic to look for a good solution.
  if (problem.type() == LinearBooleanProblem::MINIMIZATION ||
      problem.type() == LinearBooleanProblem::MAXIMIZATION) {
    const int sign =
        (problem.type() == LinearBooleanProblem::MAXIMIZATION) ? -1 : 1;
    const LinearObjective& objective = problem.objective();
    double max_weight = 0;
    for (int i = 0; i < objective.literals_size(); ++i) {
      max_weight =
          std::max(max_weight, fabs(static_cast<double>(objective.coefficients(i))));
    }
    for (int i = 0; i < objective.literals_size(); ++i) {
      const double weight =
          fabs(static_cast<double>(objective.coefficients(i))) / max_weight;
      if (sign * objective.coefficients(i) > 0) {
        solver->SetAssignmentPreference(
            Literal(objective.literals(i)).Negated(), weight);
      } else {
        solver->SetAssignmentPreference(Literal(objective.literals(i)), weight);
      }
    }
  }
  return true;
}

bool AddObjectiveConstraint(const LinearBooleanProblem& problem,
                            bool use_lower_bound, Coefficient lower_bound,
                            bool use_upper_bound, Coefficient upper_bound,
                            SatSolver* solver) {
  if (problem.type() != LinearBooleanProblem::MINIMIZATION &&
      problem.type() != LinearBooleanProblem::MAXIMIZATION) {
    return true;
  }
  std::vector<LiteralWithCoeff> cst;
  const LinearObjective& objective = problem.objective();
  for (int i = 0; i < objective.literals_size(); ++i) {
    const Literal literal(objective.literals(i));
    if (literal.Variable() >= problem.num_variables()) {
      LOG(WARNING) << "Literal out of bound: " << literal;
      return false;
    }
    cst.push_back(LiteralWithCoeff(literal, objective.coefficients(i)));
  }
  return solver->AddLinearConstraint(use_lower_bound, lower_bound,
                                     use_upper_bound, upper_bound, &cst);
}

Coefficient ComputeObjectiveValue(const LinearBooleanProblem& problem,
                                  const VariablesAssignment& assignment) {
  Coefficient sum = 0;
  const LinearObjective& objective = problem.objective();
  for (int i = 0; i < objective.literals_size(); ++i) {
    if (assignment.IsLiteralTrue(objective.literals(i))) {
      sum += objective.coefficients(i);
    }
  }
  return sum;
}

bool IsAssignmentValid(const LinearBooleanProblem& problem,
                       const VariablesAssignment& assignment) {
  // Check that all variables are assigned.
  for (int i = 0; i < problem.num_variables(); ++i) {
    if (!assignment.IsVariableAssigned(VariableIndex(i))) {
      LOG(WARNING) << "Assignment is not complete";
      return false;
    }
  }

  // Check that all constraints are satisfied.
  for (const LinearBooleanConstraint& constraint : problem.constraints()) {
    Coefficient sum = 0;
    for (int i = 0; i < constraint.literals_size(); ++i) {
      if (assignment.IsLiteralTrue(constraint.literals(i))) {
        sum += constraint.coefficients(i);
      }
    }
    if (constraint.has_lower_bound() && constraint.has_upper_bound()) {
      if (sum < constraint.lower_bound() || sum > constraint.upper_bound()) {
        LOG(WARNING) << "Unsatisfied constraint! sum: " << sum << "\n"
                     << constraint.DebugString();
        return false;
      }
    } else if (constraint.has_lower_bound()) {
      if (sum < constraint.lower_bound()) {
        LOG(WARNING) << "Unsatisfied constraint! sum: " << sum << "\n"
                     << constraint.DebugString();
        return false;
      }
    } else if (constraint.has_upper_bound()) {
      if (sum > constraint.upper_bound()) {
        LOG(WARNING) << "Unsatisfied constraint! sum: " << sum << "\n"
                     << constraint.DebugString();
        return false;
      }
    }
  }

  // We also display the objective value of an optimization problem.
  if (problem.type() == LinearBooleanProblem::MINIMIZATION ||
      problem.type() == LinearBooleanProblem::MAXIMIZATION) {
    Coefficient sum = 0;
    Coefficient min_value = 0;
    Coefficient max_value = 0;
    const LinearObjective& objective = problem.objective();
    for (int i = 0; i < objective.literals_size(); ++i) {
      if (objective.coefficients(i) > 0) {
        max_value += objective.coefficients(i);
      } else {
        min_value += objective.coefficients(i);
      }
      if (assignment.IsLiteralTrue(objective.literals(i))) {
        sum += objective.coefficients(i);
      }
    }
    LOG(INFO) << "objective: " << sum + objective.offset()
              << " trivial_bounds: [" << min_value + objective.offset() << ", "
              << max_value + objective.offset() << "]";
  }
  return true;
}

// Note(user): This function make a few assumption for a max-sat or weighted
// max-sat problem. Namely that the slack variable from the constraints are
// always in first position, and appears in the same order in the objective and
// in the constraints.
std::string LinearBooleanProblemToCnfString(const LinearBooleanProblem& problem) {
  std::string output;
  const bool is_wcnf = (problem.type() == LinearBooleanProblem::MINIMIZATION);
  const LinearObjective& objective = problem.objective();

  int64 hard_weigth = 0;
  if (is_wcnf) {
    for (const int64 weight : objective.coefficients()) {
      hard_weigth = std::max(hard_weigth, weight + 1);
    }
    CHECK_GT(hard_weigth, 0);
    output +=
        StringPrintf("p wcnf %d %d %lld\n",
                     problem.num_variables() - objective.literals().size(),
                     problem.constraints_size(), hard_weigth);
  } else {
    output += StringPrintf("p cnf %d %d\n", problem.num_variables(),
                           problem.constraints_size());
  }

  int slack_index = 0;
  for (const LinearBooleanConstraint& constraint : problem.constraints()) {
    if (constraint.literals_size() == 0) return "";
    for (int i = 0; i < constraint.literals_size(); ++i) {
      if (constraint.coefficients(i) != 1) return "";
      if (i == 0 && is_wcnf) {
        if (slack_index < objective.literals_size() &&
            constraint.literals(0) == objective.literals(slack_index)) {
          output += StringPrintf("%lld ", objective.coefficients(slack_index));
          ++slack_index;
          continue;
        } else {
          output += StringPrintf("%lld ", hard_weigth);
        }
      }
      output += Literal(constraint.literals(i)).DebugString();
      output += " ";
    }
    output += "0\n";
  }
  return output;
}

void StoreAssignment(const VariablesAssignment& assignment,
                     BooleanAssignment* output) {
  output->clear_literals();
  for (VariableIndex var(0); var < assignment.NumberOfVariables(); ++var) {
    if (assignment.IsVariableAssigned(var)) {
      output->add_literals(
          assignment.GetTrueLiteralForAssignedVariable(var).SignedValue());
    }
  }
}

}  // namespace sat
}  // namespace operations_research
