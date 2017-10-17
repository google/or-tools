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

#include "ortools/bop/bop_solver.h"

#include <string>
#include <vector>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/stringprintf.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/stl_util.h"
#include "ortools/bop/bop_fs.h"
#include "ortools/bop/bop_lns.h"
#include "ortools/bop/bop_ls.h"
#include "ortools/bop/bop_portfolio.h"
#include "ortools/bop/bop_util.h"
#include "ortools/bop/complete_optimizer.h"
#include "ortools/glop/lp_solver.h"
#include "ortools/lp_data/lp_print_utils.h"
#include "ortools/sat/boolean_problem.h"
#include "ortools/sat/lp_utils.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/bitset.h"

namespace operations_research {
namespace bop {
namespace {

using ::operations_research::LinearBooleanProblem;
using ::operations_research::glop::ColIndex;
using ::operations_research::glop::DenseRow;

// Updates the problem_state when the solution is proved optimal or the problem
// is proved infeasible.
// Returns true when the problem_state has been changed.
bool UpdateProblemStateBasedOnStatus(BopOptimizerBase::Status status,
                                     ProblemState* problem_state) {
  CHECK(nullptr != problem_state);

  if (BopOptimizerBase::OPTIMAL_SOLUTION_FOUND == status) {
    problem_state->MarkAsOptimal();
    return true;
  }

  if (BopOptimizerBase::INFEASIBLE == status) {
    problem_state->MarkAsInfeasible();
    return true;
  }

  return false;
}

}  // anonymous namespace

//------------------------------------------------------------------------------
// BopSolver
//------------------------------------------------------------------------------
BopSolver::BopSolver(const LinearBooleanProblem& problem)
    : problem_(problem),
      problem_state_(problem),
      parameters_(),
      stats_("BopSolver") {
  SCOPED_TIME_STAT(&stats_);
  CHECK_OK(sat::ValidateBooleanProblem(problem));
}

BopSolver::~BopSolver() { IF_STATS_ENABLED(VLOG(1) << stats_.StatString()); }

BopSolveStatus BopSolver::Solve() {
  std::unique_ptr<TimeLimit> time_limit =
      TimeLimit::FromParameters(parameters_);
  return SolveWithTimeLimit(time_limit.get());
}

BopSolveStatus BopSolver::SolveWithTimeLimit(TimeLimit* time_limit) {
  CHECK(time_limit != nullptr);
  SCOPED_TIME_STAT(&stats_);

  UpdateParameters();

  return parameters_.number_of_solvers() > 1
             ? InternalMultithreadSolver(time_limit)
             : InternalMonothreadSolver(time_limit);
}

BopSolveStatus BopSolver::InternalMonothreadSolver(TimeLimit* time_limit) {
  CHECK(time_limit != nullptr);
  LearnedInfo learned_info(problem_state_.original_problem());
  PortfolioOptimizer optimizer(problem_state_, parameters_,
                               parameters_.solver_optimizer_sets(0),
                               "Portfolio");
  while (!time_limit->LimitReached()) {
    const BopOptimizerBase::Status optimization_status = optimizer.Optimize(
        parameters_, problem_state_, &learned_info, time_limit);
    problem_state_.MergeLearnedInfo(learned_info, optimization_status);

    if (optimization_status == BopOptimizerBase::SOLUTION_FOUND) {
      CHECK(problem_state_.solution().IsFeasible());
      VLOG(1) << problem_state_.solution().GetScaledCost()
              << "  New solution! ";
    }

    if (problem_state_.IsOptimal()) {
      CHECK(problem_state_.solution().IsFeasible());
      return BopSolveStatus::OPTIMAL_SOLUTION_FOUND;
    } else if (problem_state_.IsInfeasible()) {
      return BopSolveStatus::INFEASIBLE_PROBLEM;
    }

    if (optimization_status == BopOptimizerBase::ABORT) {
      break;
    }
    learned_info.Clear();
  }

  return problem_state_.solution().IsFeasible()
             ? BopSolveStatus::FEASIBLE_SOLUTION_FOUND
             : BopSolveStatus::NO_SOLUTION_FOUND;
}

BopSolveStatus BopSolver::InternalMultithreadSolver(TimeLimit* time_limit) {
  CHECK(time_limit != nullptr);
  // Not implemented.
  return BopSolveStatus::INVALID_PROBLEM;
}

BopSolveStatus BopSolver::Solve(const BopSolution& first_solution) {
  std::unique_ptr<TimeLimit> time_limit =
      TimeLimit::FromParameters(parameters_);
  return SolveWithTimeLimit(first_solution, time_limit.get());
}

BopSolveStatus BopSolver::SolveWithTimeLimit(const BopSolution& first_solution,
                                             TimeLimit* time_limit) {
  SCOPED_TIME_STAT(&stats_);

  if (first_solution.IsFeasible()) {
    VLOG(1) << "First solution is feasible.";
    LearnedInfo learned_info(problem_);
    learned_info.solution = first_solution;
    if (problem_state_.MergeLearnedInfo(learned_info,
                                        BopOptimizerBase::CONTINUE) &&
        problem_state_.IsOptimal()) {
      return BopSolveStatus::OPTIMAL_SOLUTION_FOUND;
    }
  } else {
    VLOG(1)
        << "First solution is infeasible. Using it as assignment preference.";
    std::vector<bool> assignment_preference;
    for (int i = 0; i < first_solution.Size(); ++i) {
      assignment_preference.push_back(first_solution.Value(VariableIndex(i)));
    }
    problem_state_.set_assignment_preference(assignment_preference);
  }
  return SolveWithTimeLimit(time_limit);
}

double BopSolver::GetScaledBestBound() const {
  return sat::AddOffsetAndScaleObjectiveValue(
      problem_, sat::Coefficient(problem_state_.lower_bound()));
}

double BopSolver::GetScaledGap() const {
  return 100.0 * std::abs(problem_state_.solution().GetScaledCost() -
                          GetScaledBestBound()) /
         std::abs(problem_state_.solution().GetScaledCost());
}

void BopSolver::UpdateParameters() {
  if (parameters_.solver_optimizer_sets_size() == 0) {
    // No user defined optimizers, use the default std::string to define the
    // behavior.
    CHECK(::google::protobuf::TextFormat::ParseFromString(
        parameters_.default_solver_optimizer_sets(),
        parameters_.add_solver_optimizer_sets()));
  }

  problem_state_.SetParameters(parameters_);
}
}  // namespace bop
}  // namespace operations_research
