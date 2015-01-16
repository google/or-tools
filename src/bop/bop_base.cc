// Copyright 2010-2014 Google
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

#include "bop/bop_base.h"

#include <string>
#include <vector>

#include "sat/boolean_problem.h"

using operations_research::LinearBooleanProblem;

namespace operations_research {
namespace bop {
//------------------------------------------------------------------------------
// ProblemState
//------------------------------------------------------------------------------
ProblemState::ProblemState(const LinearBooleanProblem& problem)
    : original_problem_(problem),
      parameters_(),
      is_fixed_(problem.num_variables(), false),
      fixed_values_(problem.num_variables(), false),
      lp_values_(),
      solution_(problem, "AllZero"),
      lower_bound_(kint64min),
      upper_bound_(kint64max) {
  // TODO(user): Extract to a function used by all solvers.
  // Compute trivial unscaled lower bound.
  const LinearObjective& objective = problem.objective();
  lower_bound_ = 0;
  for (int i = 0; i < objective.coefficients_size(); ++i) {
    lower_bound_ += std::min<int64>(0LL, objective.coefficients(i));
  }
  upper_bound_ = solution_.IsFeasible() ? solution_.GetCost() : kint64max;
}

bool ProblemState::MergeLearnedInfo(const LearnedInfo& learned_info) {
  const std::string kIndent(25, ' ');

  bool new_lp_values = false;
  if (!learned_info.lp_values.empty()) {
    if (lp_values_ != learned_info.lp_values) {
      lp_values_ = learned_info.lp_values;
      new_lp_values = true;
      LOG(INFO) << kIndent + "New LP values.";
    }
  }

  bool new_binary_clauses = false;
  if (!learned_info.binary_clauses.empty()) {
    const int old_num = binary_clause_manager_.NumClauses();
    for (sat::BinaryClause c : learned_info.binary_clauses) {
      sat::VariableIndex num_vars(original_problem_.num_variables());
      if (c.a.Variable() < num_vars && c.b.Variable() < num_vars) {
        binary_clause_manager_.Add(c);
      }
    }
    if (binary_clause_manager_.NumClauses() > old_num) {
      new_binary_clauses = true;
      LOG(INFO) << kIndent + "Num binary clauses: "
                << binary_clause_manager_.NumClauses();
    }
  }

  bool new_solution = false;
  if (learned_info.solution.IsFeasible() &&
      (!solution_.IsFeasible() ||
       learned_info.solution.GetCost() < solution_.GetCost())) {
    solution_ = learned_info.solution;
    new_solution = true;
    LOG(INFO) << kIndent + "New solution.";
  }

  bool new_lower_bound = false;
  if (learned_info.lower_bound > lower_bound()) {
    lower_bound_ = learned_info.lower_bound;
    new_lower_bound = true;
    LOG(INFO) << kIndent + "New lower bound.";
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
    LOG(INFO) << kIndent << num_newly_fixed_variables
              << " newly fixed variables (" << num_fixed_variables << " / "
              << is_fixed_.size() << ").";
    if (num_fixed_variables == is_fixed_.size() && solution_.IsFeasible()) {
      MarkAsOptimal();
      LOG(INFO) << kIndent << "Optimal";
    }
  }

  return new_lp_values || new_binary_clauses || new_solution ||
         new_lower_bound || num_newly_fixed_variables > 0;
}

LearnedInfo ProblemState::GetLearnedInfo() const {
  LearnedInfo learned_info(original_problem_);
  for (VariableIndex var(0); var < is_fixed_.size(); ++var) {
    if (is_fixed_[var]) {
      learned_info.fixed_literals.push_back(
          sat::Literal(sat::VariableIndex(var.value()), fixed_values_[var]));
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
}

const std::vector<sat::BinaryClause>& ProblemState::NewlyAddedBinaryClauses() const {
  return binary_clause_manager_.newly_added();
}

void ProblemState::SynchronizationDone() {
  binary_clause_manager_.ClearNewlyAdded();
}

//------------------------------------------------------------------------------
// StampedLearnedInfo
//------------------------------------------------------------------------------
StampedLearnedInfo::StampedLearnedInfo()
    : learned_infos_(), last_stamp_reached_(false), mutex_() {}

void StampedLearnedInfo::AddLearnedInfo(SolverTimeStamp stamp,
                                        const LearnedInfo& learned_info) {
  // MutexLock mutex_lock(&mutex_);
  CHECK_EQ(stamp, learned_infos_.size());
  learned_infos_.push_back(learned_info);
}

bool StampedLearnedInfo::GetLearnedInfo(SolverTimeStamp stamp,
                                        LearnedInfo* learned_info) {
  CHECK_LE(0, stamp);
  CHECK(nullptr != learned_info);

  learned_info->Clear();

  {
    // MutexLock mutex_lock(&mutex_);
    if (ContainsStamp(stamp)) {
      *learned_info = learned_infos_[stamp];
      return true;
    }
    if (LastStampReached()) {
      return false;
    }
  }
  // MutexConditionInfo m(this, stamp);
  // mutex_.LockWhen(Condition(&SatisfiesStampCondition, &m));
  // mutex_.AssertHeld();
  if (LastStampReached()) {
    // mutex_.Unlock();
    return false;
  }

  CHECK(ContainsStamp(stamp));
  *learned_info = learned_infos_[stamp];
  // mutex_.Unlock();
  return true;
}

void StampedLearnedInfo::MarkLastStampReached() {
  // MutexLock mutex_lock(&mutex_);
  last_stamp_reached_ = true;
}

bool StampedLearnedInfo::LastStampReached() const {
  // mutex_.AssertHeld();
  return last_stamp_reached_;
}

bool StampedLearnedInfo::ContainsStamp(SolverTimeStamp stamp) const {
  // mutex_.AssertHeld();
  return stamp < learned_infos_.size();
}

bool StampedLearnedInfo::SatisfiesStampCondition(MutexConditionInfo* m) {
  CHECK(nullptr != m);
  return m->learned_infos->ContainsStamp(m->stamp) ||
         m->learned_infos->LastStampReached();
}

//------------------------------------------------------------------------------
// BopOptimizerBase
//------------------------------------------------------------------------------
BopOptimizerBase::BopOptimizerBase(const std::string& name)
    : name_(name), stats_(name) {
  SCOPED_TIME_STAT(&stats_);
}

BopOptimizerBase::~BopOptimizerBase() {
  IF_STATS_ENABLED(LOG(INFO) << stats_.StatString());
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

}  // namespace bop
}  // namespace operations_research
