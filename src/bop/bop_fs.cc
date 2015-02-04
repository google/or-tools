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

#include "bop/bop_fs.h"

#include <string>
#include <vector>

#include "base/commandlineflags.h"
#include "base/stringprintf.h"
#include "google/protobuf/text_format.h"
#include "base/stl_util.h"
#include "glop/lp_solver.h"
#include "lp_data/lp_print_utils.h"
#include "sat/boolean_problem.h"
#include "sat/lp_utils.h"
#include "sat/optimization.h"
#include "sat/sat_solver.h"
#include "util/bitset.h"

using operations_research::glop::ColIndex;
using operations_research::glop::DenseRow;
using operations_research::glop::GlopParameters;
using operations_research::glop::LinearProgram;
using operations_research::glop::LPSolver;
using operations_research::glop::RowIndex;
using operations_research::LinearBooleanProblem;
using operations_research::LinearBooleanConstraint;
using operations_research::LinearObjective;

namespace operations_research {
namespace bop {
namespace {
BopOptimizerBase::Status SolutionStatus(const BopSolution& solution,
                                        int64 lower_bound) {
  // The lower bound might be greater that the cost of a feasible solution due
  // to rounding errors in the problem scaling and Glop.
  return solution.IsFeasible() ? (solution.GetCost() <= lower_bound
                                      ? BopOptimizerBase::OPTIMAL_SOLUTION_FOUND
                                      : BopOptimizerBase::SOLUTION_FOUND)
                               : BopOptimizerBase::LIMIT_REACHED;
}

bool AllIntegralValues(const DenseRow& values, double tolerance) {
  for (const glop::Fractional value : values) {
    // Note that this test is correct because in this part of the code, Bop
    // only deals with boolean variables.
    if (value >= tolerance && value + tolerance < 1.0) {
      return false;
    }
  }
  return true;
}

void DenseRowToBopSolution(const DenseRow& values, BopSolution* solution) {
  CHECK(solution != nullptr);
  CHECK_EQ(solution->Size(), values.size());
  for (VariableIndex var(0); var < solution->Size(); ++var) {
    solution->SetValue(var, round(values[ColIndex(var.value())]));
  }
}
}  // anonymous namespace
//------------------------------------------------------------------------------
// BopSatObjectiveFirstSolutionGenerator
//------------------------------------------------------------------------------
BopSatObjectiveFirstSolutionGenerator::BopSatObjectiveFirstSolutionGenerator(
    const std::string& name, double time_limit_ratio)
    : BopOptimizerBase(name),
      state_update_stamp_(ProblemState::kInitialStampValue),
      time_limit_ratio_(time_limit_ratio),
      first_solve_(true),
      sat_solver_(),
      lower_bound_(kint64min),
      upper_bound_(kint64max) {}

BopSatObjectiveFirstSolutionGenerator::
    ~BopSatObjectiveFirstSolutionGenerator() {}

BopOptimizerBase::Status
BopSatObjectiveFirstSolutionGenerator::SynchronizeIfNeeded(
    const ProblemState& problem_state) {
  if (state_update_stamp_ == problem_state.update_stamp()) {
    return BopOptimizerBase::CONTINUE;
  }
  state_update_stamp_ = problem_state.update_stamp();

  if (!problem_state.solution().IsFeasible()) {
    first_solve_ = true;
  }
  if (!first_solve_) return BopOptimizerBase::ABORT;

  // Create the sat_solver if not already done.
  if (!sat_solver_) sat_solver_.reset(new sat::SatSolver());

  const BopOptimizerBase::Status load_status =
      LoadStateProblemToSatSolver(problem_state, sat_solver_.get());
  if (load_status != BopOptimizerBase::CONTINUE) return load_status;
  UseObjectiveForSatAssignmentPreference(problem_state.original_problem(),
                                         sat_solver_.get());
  lower_bound_ = problem_state.lower_bound();
  upper_bound_ = problem_state.upper_bound();
  return BopOptimizerBase::CONTINUE;
}

BopOptimizerBase::Status BopSatObjectiveFirstSolutionGenerator::Optimize(
    const BopParameters& parameters, const ProblemState& problem_state,
    LearnedInfo* learned_info, TimeLimit* time_limit) {
  CHECK(learned_info != nullptr);
  CHECK(time_limit != nullptr);
  learned_info->Clear();

  const BopOptimizerBase::Status sync_status =
      SynchronizeIfNeeded(problem_state);
  if (sync_status != BopOptimizerBase::CONTINUE) {
    return sync_status;
  }

  if (!first_solve_) return BopOptimizerBase::ABORT;
  first_solve_ = false;

  const double initial_deterministic_time = sat_solver_->deterministic_time();
  sat::SatParameters sat_parameters;
  sat_parameters.set_max_time_in_seconds(
      std::min(time_limit->GetTimeLeft(),
          time_limit_ratio_ * time_limit->GetTimeLeft()));
  sat_parameters.set_max_deterministic_time(
      std::min(time_limit->GetDeterministicTimeLeft(),
          time_limit_ratio_ * time_limit->GetDeterministicTimeLeft()));
  sat_solver_->SetParameters(sat_parameters);

  // Doubling the time limit for the next call to Optimize().
  if (first_solve_) time_limit_ratio_ *= 2.0;

  const sat::SatSolver::Status sat_status = sat_solver_->Solve();
  time_limit->AdvanceDeterministicTime(sat_solver_->deterministic_time() -
                                       initial_deterministic_time);

  if (sat_status == sat::SatSolver::MODEL_UNSAT) {
    if (upper_bound_ != kint64max) {
      // As the solution in the state problem is feasible, it is proved optimal.
      learned_info->lower_bound = upper_bound_;
      return BopOptimizerBase::OPTIMAL_SOLUTION_FOUND;
    }
    // The problem is proved infeasible
    return BopOptimizerBase::INFEASIBLE;
  }
  if (sat_status == sat::SatSolver::MODEL_SAT) {
    ExtractLearnedInfoFromSatSolver(sat_solver_.get(), learned_info);
    SatAssignmentToBopSolution(sat_solver_->Assignment(),
                               &learned_info->solution);
    return SolutionStatus(learned_info->solution, lower_bound_);
  }

  return BopOptimizerBase::LIMIT_REACHED;
}

//------------------------------------------------------------------------------
// BopSatLpFirstSolutionGenerator
//------------------------------------------------------------------------------
BopSatLpFirstSolutionGenerator::BopSatLpFirstSolutionGenerator(
    const std::string& name, double time_limit_ratio)
    : BopOptimizerBase(name),
      state_update_stamp_(ProblemState::kInitialStampValue),
      time_limit_ratio_(time_limit_ratio),
      first_solve_(true),
      sat_solver_(),
      lower_bound_(kint64min),
      upper_bound_(kint64max),
      lp_values_() {}

BopSatLpFirstSolutionGenerator::~BopSatLpFirstSolutionGenerator() {}

BopOptimizerBase::Status BopSatLpFirstSolutionGenerator::SynchronizeIfNeeded(
    const ProblemState& problem_state) {
  if (state_update_stamp_ == problem_state.update_stamp()) {
    return BopOptimizerBase::CONTINUE;
  }
  state_update_stamp_ = problem_state.update_stamp();

  if (!problem_state.solution().IsFeasible()) {
    first_solve_ = true;
  }
  if (!first_solve_ || problem_state.lp_values().empty()) {
    return BopOptimizerBase::ABORT;
  }

  lower_bound_ = problem_state.lower_bound();
  upper_bound_ = problem_state.upper_bound();
  lp_values_ = problem_state.lp_values();

  // Create the sat_solver if not already done.
  if (!sat_solver_) sat_solver_.reset(new sat::SatSolver());

  const BopOptimizerBase::Status load_status =
      LoadStateProblemToSatSolver(problem_state, sat_solver_.get());
  if (load_status != BopOptimizerBase::CONTINUE) return load_status;

  // Assign SAT preferences based on the LP solution.
  for (ColIndex col(0); col < lp_values_.size(); ++col) {
    const double value = lp_values_[col];
    sat_solver_->SetAssignmentPreference(
        sat::Literal(sat::VariableIndex(col.value()), round(value) == 1),
        1 - fabs(value - round(value)));
  }
  return BopOptimizerBase::CONTINUE;
}

BopOptimizerBase::Status BopSatLpFirstSolutionGenerator::Optimize(
    const BopParameters& parameters, const ProblemState& problem_state,
    LearnedInfo* learned_info, TimeLimit* time_limit) {
  CHECK(learned_info != nullptr);
  CHECK(time_limit != nullptr);
  learned_info->Clear();

  const BopOptimizerBase::Status sync_status =
      SynchronizeIfNeeded(problem_state);
  if (sync_status != BopOptimizerBase::CONTINUE) {
    return sync_status;
  }

  if (!first_solve_ || lp_values_.empty()) return BopOptimizerBase::ABORT;
  first_solve_ = false;

  const double sat_deterministic_time = sat_solver_->deterministic_time();
  sat::SatParameters sat_parameters;
  sat_parameters.set_max_time_in_seconds(
      std::min(time_limit->GetTimeLeft(),
          time_limit_ratio_ * time_limit->GetTimeLeft()));
  sat_parameters.set_max_deterministic_time(
      std::min(time_limit->GetDeterministicTimeLeft(),
          time_limit_ratio_ * time_limit->GetDeterministicTimeLeft()));
  sat_solver_->SetParameters(sat_parameters);

  // Doubling the time limit for the next call to Optimize().
  if (first_solve_) time_limit_ratio_ *= 2.0;

  const sat::SatSolver::Status sat_status = sat_solver_->Solve();
  time_limit->AdvanceDeterministicTime(sat_solver_->deterministic_time() -
                                       sat_deterministic_time);
  if (sat_status == sat::SatSolver::MODEL_UNSAT) {
    if (upper_bound_ != kint64max) {
      // As the solution in the state problem is feasible,it is proved optimal.
      learned_info->lower_bound = upper_bound_;
      return BopOptimizerBase::OPTIMAL_SOLUTION_FOUND;
    }
    // The problem is proved infeasible
    return BopOptimizerBase::INFEASIBLE;
  }
  if (sat_status == sat::SatSolver::MODEL_SAT) {
    ExtractLearnedInfoFromSatSolver(sat_solver_.get(), learned_info);
    SatAssignmentToBopSolution(sat_solver_->Assignment(),
                               &learned_info->solution);
    return SolutionStatus(learned_info->solution, lower_bound_);
  }

  return BopOptimizerBase::LIMIT_REACHED;
}


//------------------------------------------------------------------------------
// BopRandomFirstSolutionGenerator
//------------------------------------------------------------------------------
BopRandomFirstSolutionGenerator::BopRandomFirstSolutionGenerator(
    int random_seed, const std::string& name, double time_limit_ratio)
    : BopOptimizerBase(name),
      state_update_stamp_(ProblemState::kInitialStampValue),
      time_limit_ratio_(time_limit_ratio),
      problem_(nullptr),
      initial_solution_(),
      random_seed_(random_seed),
      random_(),
      sat_solver_(),
      sat_seed_(0),
      first_solve_(true) {}

BopRandomFirstSolutionGenerator::~BopRandomFirstSolutionGenerator() {}

BopOptimizerBase::Status BopRandomFirstSolutionGenerator::SynchronizeIfNeeded(
    const ProblemState& problem_state) {
  if (state_update_stamp_ == problem_state.update_stamp()) {
    return BopOptimizerBase::CONTINUE;
  }
  state_update_stamp_ = problem_state.update_stamp();

  // Create the random generator if not already done.
  if (!random_) random_.reset(new MTRandom(random_seed_));

  // Create the sat_solver if not already done.
  if (!sat_solver_) sat_solver_.reset(new sat::SatSolver());

  const BopOptimizerBase::Status load_status =
      LoadStateProblemToSatSolver(problem_state, sat_solver_.get());
  if (load_status != BopOptimizerBase::CONTINUE) return load_status;

  problem_ = &problem_state.original_problem();
  initial_solution_.reset(new BopSolution(problem_state.solution()));
  lp_values_ = problem_state.lp_values();
  if (!problem_state.solution().IsFeasible()) {
    first_solve_ = true;
  }
  return BopOptimizerBase::CONTINUE;
}

BopOptimizerBase::Status BopRandomFirstSolutionGenerator::Optimize(
    const BopParameters& parameters, const ProblemState& problem_state,
    LearnedInfo* learned_info, TimeLimit* time_limit) {
  CHECK(learned_info != nullptr);
  CHECK(time_limit != nullptr);
  learned_info->Clear();

  const BopOptimizerBase::Status sync_status =
      SynchronizeIfNeeded(problem_state);
  if (sync_status != BopOptimizerBase::CONTINUE) {
    return sync_status;
  }

  // We want to check both the local time limit and the global time limit, as
  // during first solution phase the local time limit can be more restrictive.
  TimeLimit local_time_limit(
      std::min(time_limit->GetTimeLeft(),
          time_limit_ratio_ * time_limit->GetTimeLeft()),
      std::min(time_limit->GetDeterministicTimeLeft(),
          time_limit_ratio_ * time_limit->GetDeterministicTimeLeft()));

  const int kMaxNumConflicts = 10;
  learned_info->solution = *initial_solution_;
  learned_info->solution.set_name(
      initial_solution_->IsFeasible()
          ? StringPrintf("RandomFirstSolutionFrom_%lld",
                         initial_solution_->GetCost())
          : "RandomFirstSolution");
  int64 best_cost = initial_solution_->IsFeasible()
                        ? initial_solution_->GetCost()
                        : kint64max;
  int64 remaining_num_conflicts =
      first_solve_
          ? kint64max
          : parameters.max_number_of_conflicts_in_random_solution_generation();
  first_solve_ = false;
  int64 old_num_failures = 0;

  while (remaining_num_conflicts > 0 && !local_time_limit.LimitReached()) {
    ++sat_seed_;
    sat_solver_->Backtrack(0);
    old_num_failures = sat_solver_->num_failures();

    sat::SatParameters sat_parameters;
    sat::RandomizeDecisionHeuristic(random_.get(), &sat_parameters);
    sat_parameters.set_max_number_of_conflicts(kMaxNumConflicts);
    sat_parameters.set_random_seed(sat_seed_);
    sat_solver_->SetParameters(sat_parameters);
    sat_solver_->ResetDecisionHeuristic();

    // Constrain the solution to be better than the current one.
    if (best_cost == kint64max ||
        AddObjectiveConstraint(*problem_, false, sat::Coefficient(0), true,
                               sat::Coefficient(best_cost) - 1,
                               sat_solver_.get())) {
      // Special assignment preference parameters.
      const int preference = random_->Uniform(4);
      if (preference == 0) {
        UseObjectiveForSatAssignmentPreference(*problem_, sat_solver_.get());
      } else if (preference == 1 && !lp_values_.empty()) {
        // Assign SAT assignment preference based on the LP solution.
        for (ColIndex col(0); col < lp_values_.size(); ++col) {
          const double value = lp_values_[col];
          sat_solver_->SetAssignmentPreference(
              sat::Literal(sat::VariableIndex(col.value()), round(value) == 1),
              1 - fabs(value - round(value)));
        }
      }

      const sat::SatSolver::Status sat_status =
          sat_solver_->SolveWithTimeLimit(&local_time_limit);
      if (sat_status == sat::SatSolver::MODEL_SAT) {
        SatAssignmentToBopSolution(sat_solver_->Assignment(),
                                   &learned_info->solution);
        CHECK_LT(learned_info->solution.GetCost(), best_cost);
        best_cost = learned_info->solution.GetCost();
      } else if (sat_status == sat::SatSolver::MODEL_UNSAT) {
        // The solution is proved optimal (if any).
        learned_info->lower_bound = best_cost;
        return best_cost == kint64max
                   ? BopOptimizerBase::INFEASIBLE
                   : BopOptimizerBase::OPTIMAL_SOLUTION_FOUND;
      }
    } else {
      // The solution is proved optimal (if any).
      learned_info->lower_bound = best_cost;
      return best_cost == kint64max ? BopOptimizerBase::INFEASIBLE
                                    : BopOptimizerBase::OPTIMAL_SOLUTION_FOUND;
    }

    // The number of failure is a good approximation of the number of conflicts.
    // Note that the number of failures of the SAT solver is not reinitialized.
    remaining_num_conflicts -= sat_solver_->num_failures() - old_num_failures;
  }

  ExtractLearnedInfoFromSatSolver(sat_solver_.get(), learned_info);
  time_limit->AdvanceDeterministicTime(
      local_time_limit.GetElapsedDeterministicTime());
  return learned_info->solution.IsFeasible() &&
                 (!initial_solution_->IsFeasible() ||
                  learned_info->solution.GetCost() <
                      initial_solution_->GetCost())
             ? BopOptimizerBase::SOLUTION_FOUND
             : BopOptimizerBase::LIMIT_REACHED;
}

//------------------------------------------------------------------------------
// LinearRelaxation
//------------------------------------------------------------------------------
LinearRelaxation::LinearRelaxation(const BopParameters& parameters,
                                   const std::string& name, double time_limit_ratio)
    : BopOptimizerBase(name),
      parameters_(parameters),
      time_limit_ratio_(time_limit_ratio),
      state_update_stamp_(ProblemState::kInitialStampValue),
      lp_model_loaded_(false),
      lp_model_(),
      lp_solver_(),
      scaling_(1),
      offset_(0),
      num_fixed_variables_(-1),
      problem_already_solved_(false),
      scaled_solution_cost_(glop::kInfinity) {}

LinearRelaxation::~LinearRelaxation() {}

BopOptimizerBase::Status LinearRelaxation::SynchronizeIfNeeded(
    const ProblemState& problem_state) {
  if (state_update_stamp_ == problem_state.update_stamp()) {
    return BopOptimizerBase::CONTINUE;
  }
  state_update_stamp_ = problem_state.update_stamp();

  // Check if the number of fixed variables is greater than last time.
  // TODO(user): Consider checking changes in number of conflicts too.
  int num_fixed_variables = 0;
  for (const bool is_fixed : problem_state.is_fixed()) {
    if (is_fixed) {
      ++num_fixed_variables;
    }
  }
  problem_already_solved_ =
      problem_already_solved_ && num_fixed_variables_ >= num_fixed_variables;
  if (problem_already_solved_) return BopOptimizerBase::ABORT;

  // Create the LP model based on the current problem state.
  num_fixed_variables_ = num_fixed_variables;
  if (!lp_model_loaded_) {
    lp_model_.Clear();
    sat::ConvertBooleanProblemToLinearProgram(problem_state.original_problem(),
                                              &lp_model_);
    lp_model_loaded_ = true;
  }
  for (VariableIndex var(0); var < problem_state.is_fixed().size(); ++var) {
    if (problem_state.IsVariableFixed(var)) {
      const glop::Fractional value =
          problem_state.GetVariableFixedValue(var) ? 1.0 : 0.0;
      lp_model_.SetVariableBounds(ColIndex(var.value()), value, value);
    }
  }

  // Add learned binary clauses.
  if (parameters_.use_learned_binary_clauses_in_lp()) {
    for (const sat::BinaryClause& clause :
         problem_state.NewlyAddedBinaryClauses()) {
      const RowIndex constraint_index = lp_model_.CreateNewConstraint();
      const int64 coefficient_a = clause.a.IsPositive() ? 1 : -1;
      const int64 coefficient_b = clause.b.IsPositive() ? 1 : -1;
      const int64 rhs = 1 + (clause.a.IsPositive() ? 0 : -1) +
                        (clause.b.IsPositive() ? 0 : -1);
      const ColIndex col_a(clause.a.Variable().value());
      const ColIndex col_b(clause.b.Variable().value());
      lp_model_.SetConstraintName(
          constraint_index,
          StringPrintf((clause.a.IsPositive() ? "%s" : "not(%s)"),
                       lp_model_.GetVariableName(col_a).c_str()) +
              " or " + StringPrintf((clause.b.IsPositive() ? "%s" : "not(%s)"),
                                    lp_model_.GetVariableName(col_b).c_str()));
      lp_model_.SetCoefficient(constraint_index, col_a, coefficient_a);
      lp_model_.SetCoefficient(constraint_index, col_b, coefficient_b);
      lp_model_.SetConstraintBounds(constraint_index, rhs, glop::kInfinity);
    }
  }

  scaling_ = problem_state.original_problem().objective().scaling_factor();
  offset_ = problem_state.original_problem().objective().offset();
  scaled_solution_cost_ =
      problem_state.solution().IsFeasible()
          ? problem_state.solution().GetScaledCost()
          : (lp_model_.IsMaximizationProblem() ? -glop::kInfinity
                                               : glop::kInfinity);
  return BopOptimizerBase::CONTINUE;
}

BopOptimizerBase::Status LinearRelaxation::Optimize(
    const BopParameters& parameters, const ProblemState& problem_state,
    LearnedInfo* learned_info, TimeLimit* time_limit) {
  CHECK(learned_info != nullptr);
  CHECK(time_limit != nullptr);
  learned_info->Clear();

  const BopOptimizerBase::Status sync_status =
      SynchronizeIfNeeded(problem_state);
  if (sync_status != BopOptimizerBase::CONTINUE) {
    return sync_status;
  }

  const glop::ProblemStatus lp_status = Solve(false, time_limit);
  VLOG(1) << "                          LP: "
          << StringPrintf("%.6f", lp_solver_.GetObjectiveValue())
          << "   status: " << GetProblemStatusString(lp_status);

  problem_already_solved_ = true;

  if (lp_status != glop::ProblemStatus::OPTIMAL &&
      lp_status != glop::ProblemStatus::IMPRECISE &&
      lp_status != glop::ProblemStatus::PRIMAL_FEASIBLE) {
    return BopOptimizerBase::ABORT;
  }
  learned_info->lp_values = lp_solver_.variable_values();

  if (lp_status == glop::ProblemStatus::OPTIMAL) {
    // The lp returns the objective with the offset and scaled, so we need to
    // unscale it and then remove the offset.
    double lower_bound = lp_solver_.GetObjectiveValue();
    if (parameters_.use_lp_strong_branching()) {
      lower_bound =
          ComputeLowerBoundUsingStrongBranching(learned_info, time_limit);
      VLOG(1) << "                          LP: "
              << StringPrintf("%.6f", lower_bound)
              << "   using strong branching.";
    }

    const int tolerance_sign = scaling_ < 0 ? 1 : -1;
    const double unscaled_cost =
        (lower_bound +
         tolerance_sign *
             lp_solver_.GetParameters().solution_feasibility_tolerance()) /
            scaling_ -
        offset_;
    learned_info->lower_bound = static_cast<int64>(ceil(unscaled_cost));

    if (AllIntegralValues(
            learned_info->lp_values,
            lp_solver_.GetParameters().primal_feasibility_tolerance())) {
      DenseRowToBopSolution(learned_info->lp_values, &learned_info->solution);
      CHECK(learned_info->solution.IsFeasible());
      return BopOptimizerBase::OPTIMAL_SOLUTION_FOUND;
    }
  }

  return BopOptimizerBase::LIMIT_REACHED;
}

// TODO(user): It is possible to stop the search earlier using the glop
//              parameter objective_lower_limit / objective_upper_limit. That
//              can be used when a feasible solution is known, or when the false
//              best bound is computed.
glop::ProblemStatus LinearRelaxation::Solve(bool incremental_solve,
                                            TimeLimit* time_limit) {
  GlopParameters glop_parameters;
  if (incremental_solve) {
    glop_parameters.set_use_dual_simplex(true);
    glop_parameters.set_allow_simplex_algorithm_change(true);
    glop_parameters.set_use_preprocessing(false);
  }
  glop_parameters.set_max_time_in_seconds(
      std::min(time_limit->GetTimeLeft(),
          time_limit_ratio_ * time_limit->GetTimeLeft()));
  glop_parameters.set_max_deterministic_time(
      std::min(time_limit->GetDeterministicTimeLeft(),
          time_limit_ratio_ * time_limit->GetDeterministicTimeLeft()));
  lp_solver_.SetParameters(glop_parameters);
  const double initial_deterministic_time = lp_solver_.DeterministicTime();
  const glop::ProblemStatus lp_status = lp_solver_.Solve(lp_model_);
  time_limit->AdvanceDeterministicTime(lp_solver_.DeterministicTime() -
                                       initial_deterministic_time);
  return lp_status;
}

double LinearRelaxation::ComputeLowerBoundUsingStrongBranching(
    LearnedInfo* learned_info, TimeLimit* time_limit) {
  const glop::DenseRow initial_lp_values = lp_solver_.variable_values();
  const double tolerance =
      lp_solver_.GetParameters().primal_feasibility_tolerance();
  double best_lp_objective = lp_solver_.GetObjectiveValue();
  for (glop::ColIndex col(0); col < initial_lp_values.size(); ++col) {
    // TODO(user): Order the variables by some meaningful quantity (probably
    //              the cost variation when we snap it to one of its bound) so
    //              we can try the one that seems the most promising first.
    //              That way we can stop the strong branching earlier.
    if (time_limit->LimitReached()) break;

    // Skip fixed variables.
    if (lp_model_.variable_lower_bounds()[col] ==
        lp_model_.variable_upper_bounds()[col]) {
      continue;
    }
    CHECK_EQ(0.0, lp_model_.variable_lower_bounds()[col]);
    CHECK_EQ(1.0, lp_model_.variable_upper_bounds()[col]);

    // Note(user): Experiments show that iterating on all variables can be
    // costly and doesn't lead to better solutions when a SAT optimizer is used
    // afterward, e.g. BopSatLpFirstSolutionGenerator, and no feasible solutions
    // are available.
    // No variables are skipped when a feasible solution is know as the best
    // bound / cost comparison can be used to deduce fixed variables, and be
    // useful for other optimizers.
    if ((scaled_solution_cost_ == glop::kInfinity ||
         scaled_solution_cost_ == -glop::kInfinity) &&
        (initial_lp_values[col] < tolerance ||
         initial_lp_values[col] + tolerance > 1)) {
      continue;
    }

    double objective_true = best_lp_objective;
    double objective_false = best_lp_objective;

    // Set to true.
    lp_model_.SetVariableBounds(col, 1.0, 1.0);
    const glop::ProblemStatus status_true = Solve(true, time_limit);
    // TODO(user): Deal with PRIMAL_INFEASIBLE, DUAL_INFEASIBLE and
    //              INFEASIBLE_OR_UNBOUNDED statuses. In all cases, if the
    //              original lp was feasible, this means that the variable can
    //              be fixed to the other bound.
    if (status_true == glop::ProblemStatus::OPTIMAL ||
        status_true == glop::ProblemStatus::DUAL_FEASIBLE) {
      objective_true = lp_solver_.GetObjectiveValue();

      // Set to false.
      lp_model_.SetVariableBounds(col, 0.0, 0.0);
      const glop::ProblemStatus status_false = Solve(true, time_limit);
      if (status_false == glop::ProblemStatus::OPTIMAL ||
          status_false == glop::ProblemStatus::DUAL_FEASIBLE) {
        objective_false = lp_solver_.GetObjectiveValue();

        // Compute the new min.
        best_lp_objective =
            lp_model_.IsMaximizationProblem()
                ? std::min(best_lp_objective, std::max(objective_true, objective_false))
                : std::max(best_lp_objective, std::min(objective_true, objective_false));
      }
    }

    if (CostIsWorseThanSolution(objective_true, tolerance)) {
      // Having variable col set to true can't possibly lead to and better
      // solution than the current one. Set the variable to false.
      lp_model_.SetVariableBounds(col, 0.0, 0.0);
      learned_info->fixed_literals.push_back(
          sat::Literal(sat::VariableIndex(col.value()), false));
    } else if (CostIsWorseThanSolution(objective_false, tolerance)) {
      // Having variable col set to false can't possibly lead to and better
      // solution than the current one. Set the variable to true.
      lp_model_.SetVariableBounds(col, 1.0, 1.0);
      learned_info->fixed_literals.push_back(
          sat::Literal(sat::VariableIndex(col.value()), true));
    } else {
      // Unset. This is safe to use 0.0 and 1.0 as the variable is not fixed.
      lp_model_.SetVariableBounds(col, 0.0, 1.0);
    }
  }
  return best_lp_objective;
}

bool LinearRelaxation::CostIsWorseThanSolution(double scaled_cost,
                                               double tolerance) const {
  return lp_model_.IsMaximizationProblem()
             ? scaled_cost + tolerance < scaled_solution_cost_
             : scaled_cost > scaled_solution_cost_ + tolerance;
}

}  // namespace bop
}  // namespace operations_research
