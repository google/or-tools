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

#include "ortools/bop/bop_base.h"

#include <limits>
#include <string>
#include <vector>

#include "ortools/sat/boolean_problem.h"

namespace operations_research {
namespace bop {

using ::operations_research::LinearBooleanProblem;

BopOptimizerBase::BopOptimizerBase(const std::string& name)
    : name_(name),
      stats_(name) {
  SCOPED_TIME_STAT(&stats_);
}

BopOptimizerBase::~BopOptimizerBase() {
  IF_STATS_ENABLED(VLOG(1) << stats_.StatString());
}

std::string BopOptimizerBase::GetStatusString(Status status) {
  switch (status) {
    case OPTIMAL_SOLUTION_FOUND:
      return "OPTIMAL_SOLUTION_FOUND";
    case SOLUTION_FOUND:
      return "SOLUTION_FOUND";
    case INFEASIBLE:
      return "INFEASIBLE";
    case LIMIT_REACHED:
      return "LIMIT_REACHED";
    case INFORMATION_FOUND:
      return "INFORMATION_FOUND";
    case CONTINUE:
      return "CONTINUE";
    case ABORT:
      return "ABORT";
  }
  // Fallback. We don't use "default:" so the compiler will return an error
  // if we forgot one enum case above.
  LOG(DFATAL) << "Invalid Status " << static_cast<int>(status);
  return "UNKNOWN Status";
}

//------------------------------------------------------------------------------
// ProblemState
//------------------------------------------------------------------------------
const int64 ProblemState::kInitialStampValue(0);

ProblemState::ProblemState(const LinearBooleanProblem& problem)
    : original_problem_(problem),
      parameters_(),
      update_stamp_(kInitialStampValue + 1),
      is_fixed_(problem.num_variables(), false),
      fixed_values_(problem.num_variables(), false),
      lp_values_(),
      solution_(problem, "AllZero"),
      assignment_preference_(),
      lower_bound_(kint64min),
      upper_bound_(kint64max) {
  // TODO(user): Extract to a function used by all solvers.
  // Compute trivial unscaled lower bound.
  const LinearObjective& objective = problem.objective();
  lower_bound_ = 0;
  for (int i = 0; i < objective.coefficients_size(); ++i) {
    // Fix template version for or-tools.
    lower_bound_ += std::min<int64>(0LL, objective.coefficients(i));
  }
  upper_bound_ = solution_.IsFeasible() ? solution_.GetCost() : kint64max;
}

// TODO(user): refactor this to not rely on the optimization status.
// All the information can be encoded in the learned_info bounds.
bool ProblemState::MergeLearnedInfo(
    const LearnedInfo& learned_info,
    BopOptimizerBase::Status optimization_status) {
  const std::string kIndent(25, ' ');

  bool new_lp_values = false;
  if (!learned_info.lp_values.empty()) {
    if (lp_values_ != learned_info.lp_values) {
      lp_values_ = learned_info.lp_values;
      new_lp_values = true;
      VLOG(1) << kIndent + "New LP values.";
    }
  }

  bool new_binary_clauses = false;
  if (!learned_info.binary_clauses.empty()) {
    const int old_num = binary_clause_manager_.NumClauses();
    for (sat::BinaryClause c : learned_info.binary_clauses) {
      const int num_vars = original_problem_.num_variables();
      if (c.a.Variable() < num_vars && c.b.Variable() < num_vars) {
        binary_clause_manager_.Add(c);
      }
    }
    if (binary_clause_manager_.NumClauses() > old_num) {
      new_binary_clauses = true;
      VLOG(1) << kIndent + "Num binary clauses: "
              << binary_clause_manager_.NumClauses();
    }
  }

  bool new_solution = false;
  if (learned_info.solution.IsFeasible() &&
      (!solution_.IsFeasible() ||
       learned_info.solution.GetCost() < solution_.GetCost())) {
    solution_ = learned_info.solution;
    new_solution = true;
    VLOG(1) << kIndent + "New solution.";
  }

  bool new_lower_bound = false;
  if (learned_info.lower_bound > lower_bound()) {
    lower_bound_ = learned_info.lower_bound;
    new_lower_bound = true;
    VLOG(1) << kIndent + "New lower bound.";
  }

  if (solution_.IsFeasible()) {
    upper_bound_ = std::min(upper_bound(), solution_.GetCost());
    if (upper_bound() <= lower_bound() ||
        (upper_bound() - lower_bound() <=
         parameters_.relative_gap_limit() *
             std::max(std::abs(upper_bound()), std::abs(lower_bound())))) {
      // The lower bound might be greater that the cost of a feasible solution
      // due to rounding errors in the problem scaling and Glop.
      // As a feasible solution was found, the solution is proved optimal.
      MarkAsOptimal();
    }
  }

  // Merge fixed variables. Note that variables added during search, i.e. not
  // in the original problem, are ignored.
  int num_newly_fixed_variables = 0;
  for (const sat::Literal literal : learned_info.fixed_literals) {
    const VariableIndex var(literal.Variable().value());
    if (var >= original_problem_.num_variables()) {
      continue;
    }
    const bool value = literal.IsPositive();
    if (is_fixed_[var]) {
      if (fixed_values_[var] != value) {
        MarkAsInfeasible();
        return true;
      }
    } else {
      is_fixed_[var] = true;
      fixed_values_[var] = value;
      ++num_newly_fixed_variables;
    }
  }
  if (num_newly_fixed_variables > 0) {
    int num_fixed_variables = 0;
    for (const bool is_fixed : is_fixed_) {
      if (is_fixed) {
        ++num_fixed_variables;
      }
    }
    VLOG(1) << kIndent << num_newly_fixed_variables
            << " newly fixed variables (" << num_fixed_variables << " / "
            << is_fixed_.size() << ").";
    if (num_fixed_variables == is_fixed_.size()) {
      // Set the solution to the fixed variables.
      BopSolution fixed_solution = solution_;
      for (VariableIndex var(0); var < is_fixed_.size(); ++var) {
        fixed_solution.SetValue(var, fixed_values_[var]);
      }
      if (fixed_solution.IsFeasible()) {
        solution_ = fixed_solution;
      }
      if (solution_.IsFeasible()) {
        MarkAsOptimal();
        VLOG(1) << kIndent << "Optimal";
      } else {
        MarkAsInfeasible();
      }
    }
  }

  bool known_status = false;
  if (optimization_status == BopOptimizerBase::OPTIMAL_SOLUTION_FOUND) {
    MarkAsOptimal();
    known_status = true;
  } else if (optimization_status == BopOptimizerBase::INFEASIBLE) {
    MarkAsInfeasible();
    known_status = true;
  }

  const bool updated = new_lp_values || new_binary_clauses || new_solution ||
                       new_lower_bound || num_newly_fixed_variables > 0 ||
                       known_status;
  if (updated) ++update_stamp_;
  return updated;
}

LearnedInfo ProblemState::GetLearnedInfo() const {
  LearnedInfo learned_info(original_problem_);
  for (VariableIndex var(0); var < is_fixed_.size(); ++var) {
    if (is_fixed_[var]) {
      learned_info.fixed_literals.push_back(
          sat::Literal(sat::BooleanVariable(var.value()), fixed_values_[var]));
    }
  }
  learned_info.solution = solution_;
  learned_info.lower_bound = lower_bound();
  learned_info.lp_values = lp_values_;
  learned_info.binary_clauses = NewlyAddedBinaryClauses();

  return learned_info;
}

void ProblemState::MarkAsOptimal() {
  CHECK(solution_.IsFeasible());
  lower_bound_ = upper_bound();
  ++update_stamp_;
}

void ProblemState::MarkAsInfeasible() {
  // Mark as infeasible, i.e. set a lower_bound greater than the upper_bound.
  CHECK(!solution_.IsFeasible());
  if (upper_bound() == kint64max) {
    lower_bound_ = kint64max;
    upper_bound_ = kint64max - 1;
  } else {
    lower_bound_ = upper_bound_ - 1;
  }
  ++update_stamp_;
}

const std::vector<sat::BinaryClause>& ProblemState::NewlyAddedBinaryClauses()
    const {
  return binary_clause_manager_.newly_added();
}

void ProblemState::SynchronizationDone() {
  binary_clause_manager_.ClearNewlyAdded();
}

}  // namespace bop
}  // namespace operations_research
