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

#include "bop/bop_solver.h"

#include <string>
#include <vector>

#include "base/commandlineflags.h"
#include "base/stringprintf.h"
#include "google/protobuf/text_format.h"
#include "base/threadpool.h"
#include "base/stl_util.h"
#include "bop/bop_fs.h"
#include "bop/bop_lns.h"
#include "bop/bop_ls.h"
#include "bop/bop_portfolio.h"
#include "bop/bop_util.h"
#include "bop/complete_optimizer.h"
#include "glop/lp_solver.h"
#include "lp_data/lp_print_utils.h"
#include "sat/boolean_problem.h"
#include "sat/lp_utils.h"
#include "sat/sat_solver.h"
#include "util/bitset.h"

using operations_research::glop::ColIndex;
using operations_research::glop::DenseRow;
using operations_research::glop::GlopParameters;
using operations_research::glop::LinearProgram;
using operations_research::glop::LPSolver;
using operations_research::LinearBooleanProblem;
using operations_research::LinearBooleanConstraint;
using operations_research::LinearObjective;
using operations_research::glop::ProblemStatus;

namespace operations_research {
namespace bop {
namespace {
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

//------------------------------------------------------------------------------
// SolverSynchronizer
//------------------------------------------------------------------------------
// This internal class is a safety layer that defines the API the solvers have
// to use to synchronize with other solvers.
class SolverSynchronizer {
 public:
  struct Info {
    Info(const BopParameters& params, const LinearBooleanProblem& problem)
        : parameters(params), problem_state(problem), learned_infos() {
      problem_state.SetParameters(params);
    }

    BopParameters parameters;
    ProblemState problem_state;
    StampedLearnedInfo learned_infos;
  };

  SolverSynchronizer(
      int solver_index, const LinearBooleanProblem& problem,
      std::vector<std::unique_ptr<SolverSynchronizer::Info>>* all_infos);

  // Adds the learned info at the given stamp to the solver.
  void AddLearnedInfo(SolverTimeStamp stamp, const LearnedInfo& learned_info);

  // Synchronizes the problem state of the solver as defined by the
  // BopParameters synchronization_type.
  // Returns *problem_changed true when the state problem has changed due to
  // synchronization.
  // Returns *stop_solver true when the solver should be stopped as the solvers
  // it might wait for have stopped.
  void SynchronizeSolverInfos(SolverTimeStamp stamp, bool* problem_changed,
                              bool* stop_solver);

  // Marks the solver as done, i.e. it reached its last stamp.
  void MarkLastStampReached();

  // Returns the mutable problem state of the solver.
  ProblemState* GetMutableProblemState() const;

  // Returns the parameters of the solver.
  const BopParameters& GetParameters() const;

  // Returns the index of the solver.
  int solver_index() const { return solver_index_; }

 private:
  int solver_index_;
  std::vector<std::unique_ptr<SolverSynchronizer::Info>>* all_infos_;
  std::vector<int> solvers_to_sync_;
  LearnedInfo learned_info_;
};

SolverSynchronizer::SolverSynchronizer(
    int solver_index, const LinearBooleanProblem& problem,
    std::vector<std::unique_ptr<SolverSynchronizer::Info>>* all_infos)
    : solver_index_(solver_index),
      all_infos_(all_infos),
      solvers_to_sync_(),
      learned_info_(problem) {
  CHECK(nullptr != all_infos_);
  CHECK_LT(solver_index_, all_infos_->size());

  switch ((*all_infos_)[solver_index]->parameters.synchronization_type()) {
    case BopParameters::NO_SYNCHRONIZATION:
      solvers_to_sync_.push_back(solver_index);
      break;
    case BopParameters::SYNCHRONIZE_ALL:
      for (int index = 0; index < all_infos_->size(); ++index) {
        solvers_to_sync_.push_back(index);
      }
      break;
    case BopParameters::SYNCHRONIZE_ON_RIGHT:
      for (int index = 0; index <= solver_index; ++index) {
        solvers_to_sync_.push_back(index);
      }
      break;
    default:
      LOG(FATAL) << "Unknown synchronization type.";
  }
}

void SolverSynchronizer::AddLearnedInfo(SolverTimeStamp stamp,
                                        const LearnedInfo& learned_info) {
  (*all_infos_)[solver_index_]->learned_infos.AddLearnedInfo(stamp,
                                                             learned_info);
}

void SolverSynchronizer::SynchronizeSolverInfos(SolverTimeStamp stamp,
                                                bool* problem_changed,
                                                bool* stop_solver) {
  CHECK(nullptr != problem_changed);
  CHECK(nullptr != stop_solver);

  *problem_changed = false;
  *stop_solver = false;
  for (const int index : solvers_to_sync_) {
    learned_info_.Clear();
    if (!(*all_infos_)[index]->learned_infos.GetLearnedInfo(stamp,
                                                            &learned_info_)) {
      // Limit reached on solver index, stop search.
      *stop_solver = true;
      return;
    }
    *problem_changed =
        GetMutableProblemState()->MergeLearnedInfo(learned_info_) ||
        *problem_changed;
  }
}

void SolverSynchronizer::MarkLastStampReached() {
  (*all_infos_)[solver_index_]->learned_infos.MarkLastStampReached();
}

const BopParameters& SolverSynchronizer::GetParameters() const {
  return (*all_infos_)[solver_index_]->parameters;
}

ProblemState* SolverSynchronizer::GetMutableProblemState() const {
  return &((*all_infos_)[solver_index_]->problem_state);
}

// Runs a new solver until the time limit is reached. The result of the run
// is submitted to the SolverSynchronizer.
void RunOptimizer(const std::string& name, SolverSynchronizer* solver_sync) {
  CHECK(nullptr != solver_sync);

  const BopParameters& parameters = solver_sync->GetParameters();
  ProblemState* const problem_state = solver_sync->GetMutableProblemState();
  TimeLimit time_limit(parameters.max_time_in_seconds(),
                       parameters.max_deterministic_time());
  LOG(INFO) << "limit: " << parameters.max_time_in_seconds() << " -- "
            << parameters.max_deterministic_time();
  const int solver_index = solver_sync->solver_index();

  CHECK_LT(0, parameters.solver_optimizer_sets_size());
  // Use the last defined method set if needed.
  const BopSolverOptimizerSet& solver_optimizer_sets =
      solver_index >= parameters.solver_optimizer_sets_size()
          ? parameters.solver_optimizer_sets(
                parameters.solver_optimizer_sets_size() - 1)
          : parameters.solver_optimizer_sets(solver_index);
  PortfolioOptimizer optimizer(*problem_state, parameters,
                               solver_optimizer_sets,
                               StringPrintf("Portfolio_%d", solver_index));
  const BopOptimizerBase::Status sync_status =
      optimizer.Synchronize(*problem_state);
  if (UpdateProblemStateBasedOnStatus(sync_status, problem_state)) return;

  LearnedInfo learned_info(problem_state->original_problem());
  SolverTimeStamp stamp(0);
  while (!time_limit.LimitReached()) {
    const BopOptimizerBase::Status optimization_status =
        optimizer.Optimize(parameters, &learned_info, &time_limit);

    solver_sync->AddLearnedInfo(stamp, learned_info);
    bool problem_changed = false;
    bool stop_solver = false;
    solver_sync->SynchronizeSolverInfos(stamp, &problem_changed, &stop_solver);
    if (stop_solver || problem_state->IsOptimal() ||
        problem_state->IsInfeasible()) {
      return;
    }

    if (optimization_status == BopOptimizerBase::SOLUTION_FOUND) {
      CHECK(problem_state->solution().IsFeasible());
      LOG(INFO) << problem_state->solution().GetScaledCost()
                << "  New solution! " << name;
    }

    if (problem_changed) {
      // The problem has changed, re-synchronize the optimizer.
      const BopOptimizerBase::Status sync_status =
          optimizer.Synchronize(*problem_state);
      if (UpdateProblemStateBasedOnStatus(sync_status, problem_state)) return;
      problem_state->SynchronizationDone();
    }

    if (optimization_status == BopOptimizerBase::ABORT) {
      break;
    }

    ++stamp;
  }

  // The search is finished for this run, mark the last stamped reached
  // to not block other runs waiting for updates on the learned_infos.
  solver_sync->MarkLastStampReached();
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
  CHECK(sat::ValidateBooleanProblem(problem).ok());
}

BopSolver::~BopSolver() { IF_STATS_ENABLED(LOG(INFO) << stats_.StatString()); }

BopSolveStatus BopSolver::Solve() {
  SCOPED_TIME_STAT(&stats_);

  UpdateParameters();

  // Create parameters, learned info and problem states for each solver.
  const int num_solvers = parameters_.number_of_solvers();
  LearnedInfo learned_info = problem_state_.GetLearnedInfo();
  std::vector<std::unique_ptr<SolverSynchronizer::Info>> all_infos;
  for (int index = 0; index < num_solvers; ++index) {
    all_infos.emplace_back(new SolverSynchronizer::Info(parameters_, problem_));
    all_infos.back()->parameters.set_random_seed(parameters_.random_seed() +
                                                 index);
    all_infos.back()->problem_state.MergeLearnedInfo(learned_info);
  }

  // Build dedicated synchronizers to forbid unsafe access to the memory of the
  // other solvers.
  std::vector<SolverSynchronizer> synchronizers;
  for (int index = 0; index < num_solvers; ++index) {
    synchronizers.push_back(SolverSynchronizer(index, problem_, &all_infos));
  }

  if (num_solvers > 1) {
    LOG(FATAL) << "Multi threading is not supported";
    // ThreadPool thread_pool("ParallelSolve", num_solvers);
    // thread_pool.StartWorkers();
    // for (int index = 0; index < num_solvers; ++index) {
    //   const std::string solver_name = StringPrintf("Solver_%d", index);
    //   SolverSynchronizer* const solver_sync = &synchronizers[index];
    //   thread_pool.Schedule([solver_name, solver_sync]() {
    //     RunOptimizer(solver_name, solver_sync);
    //   });
    // }
  } else {
    // TODO(user): Consider having a dedicated method to solve with only one
    //              solver.
    const std::string solver_name = StringPrintf("Solver_%d", 0);
    SolverSynchronizer* const solver_sync = &synchronizers[0];
    RunOptimizer(solver_name, solver_sync);
  }

  // Synchronize the BopSolver::problem_state_ from the solvers' problem states.
  for (int index = 0; index < num_solvers; ++index) {
    LOG(INFO) << "Solution of solver " << index << ": "
              << all_infos[index]->problem_state.solution().GetScaledCost();
    learned_info.Clear();
    learned_info.solution = all_infos[index]->problem_state.solution();
    learned_info.lower_bound = all_infos[index]->problem_state.lower_bound();
    problem_state_.MergeLearnedInfo(learned_info);
    if (all_infos[index]->problem_state.IsInfeasible()) {
      problem_state_.MarkAsInfeasible();
    }
  }

  return problem_state_.solution().IsFeasible()
             ? (problem_state_.IsOptimal()
                    ? BopSolveStatus::OPTIMAL_SOLUTION_FOUND
                    : BopSolveStatus::FEASIBLE_SOLUTION_FOUND)
             : (problem_state_.IsInfeasible()
                    ? BopSolveStatus::INFEASIBLE_PROBLEM
                    : BopSolveStatus::NO_SOLUTION_FOUND);
}

BopSolveStatus BopSolver::Solve(const BopSolution& first_solution) {
  SCOPED_TIME_STAT(&stats_);
  CHECK(first_solution.IsFeasible());

  LearnedInfo learned_info(problem_);
  learned_info.solution = first_solution;
  if (problem_state_.MergeLearnedInfo(learned_info) &&
      problem_state_.IsOptimal()) {
    return BopSolveStatus::OPTIMAL_SOLUTION_FOUND;
  }
  CHECK(problem_state_.solution().IsFeasible());
  return Solve();
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
    // No user defined optimizers: Implement the default behavior.
    const std::vector<std::string> default_optimizers = {
        "type:LINEAR_RELAXATION initial_score:1.0 time_limit_ratio:0.15",
        "type:LP_FIRST_SOLUTION               initial_score:0.1",
        "type:OBJECTIVE_FIRST_SOLUTION        initial_score:0.1",
        "type:RANDOM_FIRST_SOLUTION           initial_score:0.1",
        "type:SAT_CORE_BASED                  initial_score:1.0",
        "type:LOCAL_SEARCH                    initial_score:2.0",
        "type:RANDOM_CONSTRAINT_LNS           initial_score:0.1",
        "type:RANDOM_LNS_PROPAGATION          initial_score:1.0",
        "type:RANDOM_LNS_SAT                  initial_score:1.0",
        "type:COMPLETE_LNS                    initial_score:0.1"};
    BopSolverOptimizerSet* const solver_methods =
        parameters_.add_solver_optimizer_sets();
    for (const std::string& default_optimizer : default_optimizers) {
      CHECK(::google::protobuf::TextFormat::ParseFromString(
          default_optimizer, solver_methods->add_methods()));
    }
  }

  problem_state_.SetParameters(parameters_);
}
}  // namespace bop
}  // namespace operations_research
