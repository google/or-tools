// Copyright 2010-2025 Google LLC
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

#include "ortools/bop/bop_fs.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ortools/algorithms/sparse_permutation.h"
#include "ortools/base/strong_vector.h"
#include "ortools/bop/bop_base.h"
#include "ortools/bop/bop_parameters.pb.h"
#include "ortools/bop/bop_solution.h"
#include "ortools/bop/bop_types.h"
#include "ortools/bop/bop_util.h"
#include "ortools/glop/lp_solver.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/boolean_problem.h"
#include "ortools/sat/boolean_problem.pb.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/lp_utils.h"
#include "ortools/sat/pb_constraint.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/symmetry.h"
#include "ortools/sat/util.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace bop {
namespace {

using ::operations_research::glop::ColIndex;
using ::operations_research::glop::DenseRow;
using ::operations_research::glop::GlopParameters;
using ::operations_research::glop::RowIndex;

BopOptimizerBase::Status SolutionStatus(const BopSolution& solution,
                                        int64_t lower_bound) {
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
// GuidedSatFirstSolutionGenerator
//------------------------------------------------------------------------------

GuidedSatFirstSolutionGenerator::GuidedSatFirstSolutionGenerator(
    absl::string_view name, Policy policy)
    : BopOptimizerBase(name),
      policy_(policy),
      abort_(false),
      state_update_stamp_(ProblemState::kInitialStampValue),
      sat_solver_() {}

GuidedSatFirstSolutionGenerator::~GuidedSatFirstSolutionGenerator() = default;

BopOptimizerBase::Status GuidedSatFirstSolutionGenerator::SynchronizeIfNeeded(
    const ProblemState& problem_state) {
  if (state_update_stamp_ == problem_state.update_stamp()) {
    return BopOptimizerBase::CONTINUE;
  }
  state_update_stamp_ = problem_state.update_stamp();

  // Create the sat_solver if not already done.
  if (!sat_solver_) {
    sat_solver_ = std::make_unique<sat::SatSolver>();

    // Add in symmetries.
    if (problem_state.GetParameters()
            .exploit_symmetry_in_sat_first_solution()) {
      std::vector<std::unique_ptr<SparsePermutation>> generators;
      sat::FindLinearBooleanProblemSymmetries(problem_state.original_problem(),
                                              &generators);
      std::unique_ptr<sat::SymmetryPropagator> propagator(
          new sat::SymmetryPropagator);
      for (int i = 0; i < generators.size(); ++i) {
        propagator->AddSymmetry(std::move(generators[i]));
      }
      sat_solver_->AddPropagator(propagator.get());
      sat_solver_->TakePropagatorOwnership(std::move(propagator));
    }
  }

  const BopOptimizerBase::Status load_status =
      LoadStateProblemToSatSolver(problem_state, sat_solver_.get());
  if (load_status != BopOptimizerBase::CONTINUE) return load_status;

  switch (policy_) {
    case Policy::kNotGuided:
      break;
    case Policy::kLpGuided:
      for (ColIndex col(0); col < problem_state.lp_values().size(); ++col) {
        const double value = problem_state.lp_values()[col];
        sat_solver_->SetAssignmentPreference(
            sat::Literal(sat::BooleanVariable(col.value()), round(value) == 1),
            1 - fabs(value - round(value)));
      }
      break;
    case Policy::kObjectiveGuided:
      UseObjectiveForSatAssignmentPreference(problem_state.original_problem(),
                                             sat_solver_.get());
      break;
    case Policy::kUserGuided:
      for (int i = 0; i < problem_state.assignment_preference().size(); ++i) {
        sat_solver_->SetAssignmentPreference(
            sat::Literal(sat::BooleanVariable(i),
                         problem_state.assignment_preference()[i]),
            1.0);
      }
      break;
  }
  return BopOptimizerBase::CONTINUE;
}

bool GuidedSatFirstSolutionGenerator::ShouldBeRun(
    const ProblemState& problem_state) const {
  if (abort_) return false;
  if (policy_ == Policy::kLpGuided && problem_state.lp_values().empty()) {
    return false;
  }
  if (policy_ == Policy::kUserGuided &&
      problem_state.assignment_preference().empty()) {
    return false;
  }
  return true;
}

BopOptimizerBase::Status GuidedSatFirstSolutionGenerator::Optimize(
    const BopParameters& parameters, const ProblemState& problem_state,
    LearnedInfo* learned_info, TimeLimit* time_limit) {
  CHECK(learned_info != nullptr);
  CHECK(time_limit != nullptr);
  learned_info->Clear();

  const BopOptimizerBase::Status sync_status =
      SynchronizeIfNeeded(problem_state);
  if (sync_status != BopOptimizerBase::CONTINUE) return sync_status;

  sat::SatParameters sat_params;
  sat_params.set_max_time_in_seconds(time_limit->GetTimeLeft());
  sat_params.set_max_deterministic_time(time_limit->GetDeterministicTimeLeft());
  sat_params.set_random_seed(parameters.random_seed());

  // We use a relatively small conflict limit so that other optimizer get a
  // chance to run if this one is slow. Note that if this limit is reached, we
  // will return BopOptimizerBase::CONTINUE so that Optimize() will be called
  // again later to resume the current work.
  sat_params.set_max_number_of_conflicts(
      parameters.guided_sat_conflicts_chunk());
  sat_solver_->SetParameters(sat_params);

  const double initial_deterministic_time = sat_solver_->deterministic_time();
  const sat::SatSolver::Status sat_status = sat_solver_->Solve();
  time_limit->AdvanceDeterministicTime(sat_solver_->deterministic_time() -
                                       initial_deterministic_time);

  if (sat_status == sat::SatSolver::INFEASIBLE) {
    if (policy_ != Policy::kNotGuided) abort_ = true;
    if (problem_state.upper_bound() != std::numeric_limits<int64_t>::max()) {
      // As the solution in the state problem is feasible, it is proved optimal.
      learned_info->lower_bound = problem_state.upper_bound();
      return BopOptimizerBase::OPTIMAL_SOLUTION_FOUND;
    }
    // The problem is proved infeasible
    return BopOptimizerBase::INFEASIBLE;
  }

  ExtractLearnedInfoFromSatSolver(sat_solver_.get(), learned_info);
  if (sat_status == sat::SatSolver::FEASIBLE) {
    SatAssignmentToBopSolution(sat_solver_->Assignment(),
                               &learned_info->solution);
    return SolutionStatus(learned_info->solution, problem_state.lower_bound());
  }

  return BopOptimizerBase::CONTINUE;
}

//------------------------------------------------------------------------------
// BopRandomFirstSolutionGenerator
//------------------------------------------------------------------------------
BopRandomFirstSolutionGenerator::BopRandomFirstSolutionGenerator(
    absl::string_view name, const BopParameters& parameters,
    sat::SatSolver* sat_propagator, absl::BitGenRef random)
    : BopOptimizerBase(name),
      random_(random),
      sat_propagator_(sat_propagator) {}

BopRandomFirstSolutionGenerator::~BopRandomFirstSolutionGenerator() = default;

// Only run the RandomFirstSolution when there is an objective to minimize.
bool BopRandomFirstSolutionGenerator::ShouldBeRun(
    const ProblemState& problem_state) const {
  return problem_state.original_problem().objective().literals_size() > 0;
}

BopOptimizerBase::Status BopRandomFirstSolutionGenerator::Optimize(
    const BopParameters& parameters, const ProblemState& problem_state,
    LearnedInfo* learned_info, TimeLimit* time_limit) {
  CHECK(learned_info != nullptr);
  CHECK(time_limit != nullptr);
  learned_info->Clear();

  // Save the current solver heuristics.
  const sat::SatParameters saved_params = sat_propagator_->parameters();
  const std::vector<std::pair<sat::Literal, float>> saved_prefs =
      sat_propagator_->AllPreferences();

  const int kMaxNumConflicts = 10;
  int64_t best_cost = problem_state.solution().IsFeasible()
                          ? problem_state.solution().GetCost()
                          : std::numeric_limits<int64_t>::max();
  int64_t remaining_num_conflicts =
      parameters.max_number_of_conflicts_in_random_solution_generation();
  int64_t old_num_failures = 0;

  // Optimization: Since each Solve() is really fast, we want to limit as
  // much as possible the work around one.
  bool objective_need_to_be_overconstrained =
      (best_cost != std::numeric_limits<int64_t>::max());

  bool solution_found = false;
  while (remaining_num_conflicts > 0 && !time_limit->LimitReached()) {
    sat_propagator_->Backtrack(0);
    old_num_failures = sat_propagator_->num_failures();

    sat::SatParameters sat_params = saved_params;
    sat::RandomizeDecisionHeuristic(random_, &sat_params);
    sat_params.set_max_number_of_conflicts(kMaxNumConflicts);
    sat_propagator_->SetParameters(sat_params);
    sat_propagator_->ResetDecisionHeuristic();

    if (objective_need_to_be_overconstrained) {
      if (!AddObjectiveConstraint(
              problem_state.original_problem(), false, sat::Coefficient(0),
              true, sat::Coefficient(best_cost) - 1, sat_propagator_)) {
        // The solution is proved optimal (if any).
        learned_info->lower_bound = best_cost;
        return best_cost == std::numeric_limits<int64_t>::max()
                   ? BopOptimizerBase::INFEASIBLE
                   : BopOptimizerBase::OPTIMAL_SOLUTION_FOUND;
      }
      objective_need_to_be_overconstrained = false;
    }

    // Special assignment preference parameters.
    const int preference = absl::Uniform(random_, 0, 4);
    if (preference == 0) {
      UseObjectiveForSatAssignmentPreference(problem_state.original_problem(),
                                             sat_propagator_);
    } else if (preference == 1 && !problem_state.lp_values().empty()) {
      // Assign SAT assignment preference based on the LP solution.
      for (ColIndex col(0); col < problem_state.lp_values().size(); ++col) {
        const double value = problem_state.lp_values()[col];
        sat_propagator_->SetAssignmentPreference(
            sat::Literal(sat::BooleanVariable(col.value()), round(value) == 1),
            1 - fabs(value - round(value)));
      }
    }

    const sat::SatSolver::Status sat_status =
        sat_propagator_->SolveWithTimeLimit(time_limit);
    if (sat_status == sat::SatSolver::FEASIBLE) {
      objective_need_to_be_overconstrained = true;
      solution_found = true;
      SatAssignmentToBopSolution(sat_propagator_->Assignment(),
                                 &learned_info->solution);
      CHECK_LT(learned_info->solution.GetCost(), best_cost);
      best_cost = learned_info->solution.GetCost();
    } else if (sat_status == sat::SatSolver::INFEASIBLE) {
      // The solution is proved optimal (if any).
      learned_info->lower_bound = best_cost;
      return best_cost == std::numeric_limits<int64_t>::max()
                 ? BopOptimizerBase::INFEASIBLE
                 : BopOptimizerBase::OPTIMAL_SOLUTION_FOUND;
    }

    // The number of failure is a good approximation of the number of conflicts.
    // Note that the number of failures of the SAT solver is not reinitialized.
    remaining_num_conflicts -=
        sat_propagator_->num_failures() - old_num_failures;
  }

  // Restore sat_propagator_ to its original state.
  // Note that if the function is aborted before that, it means we solved the
  // problem to optimality (or proven it to be infeasible), so we don't need
  // to do any extra work in these cases since the sat_propagator_ will not be
  // used anymore.
  CHECK_EQ(0, sat_propagator_->AssumptionLevel());
  sat_propagator_->RestoreSolverToAssumptionLevel();
  sat_propagator_->SetParameters(saved_params);
  sat_propagator_->ResetDecisionHeuristic();
  for (const auto [literal, weight] : saved_prefs) {
    sat_propagator_->SetAssignmentPreference(literal, weight);
  }

  // This can be proved during the call to RestoreSolverToAssumptionLevel().
  if (sat_propagator_->ModelIsUnsat()) {
    // The solution is proved optimal (if any).
    learned_info->lower_bound = best_cost;
    return best_cost == std::numeric_limits<int64_t>::max()
               ? BopOptimizerBase::INFEASIBLE
               : BopOptimizerBase::OPTIMAL_SOLUTION_FOUND;
  }

  ExtractLearnedInfoFromSatSolver(sat_propagator_, learned_info);

  return solution_found ? BopOptimizerBase::SOLUTION_FOUND
                        : BopOptimizerBase::LIMIT_REACHED;
}

//------------------------------------------------------------------------------
// LinearRelaxation
//------------------------------------------------------------------------------
LinearRelaxation::LinearRelaxation(const BopParameters& parameters,
                                   absl::string_view name)
    : BopOptimizerBase(name),
      parameters_(parameters),
      state_update_stamp_(ProblemState::kInitialStampValue),
      lp_model_loaded_(false),
      num_full_solves_(0),
      lp_model_(),
      lp_solver_(),
      scaling_(1),
      offset_(0),
      num_fixed_variables_(-1),
      problem_already_solved_(false),
      scaled_solution_cost_(glop::kInfinity) {}

LinearRelaxation::~LinearRelaxation() = default;

BopOptimizerBase::Status LinearRelaxation::SynchronizeIfNeeded(
    const ProblemState& problem_state) {
  if (state_update_stamp_ == problem_state.update_stamp()) {
    return BopOptimizerBase::CONTINUE;
  }
  state_update_stamp_ = problem_state.update_stamp();

  // If this is a pure feasibility problem, obey
  // `BopParameters.max_lp_solve_for_feasibility_problems`.
  if (problem_state.original_problem().objective().literals_size() == 0 &&
      parameters_.max_lp_solve_for_feasibility_problems() >= 0 &&
      num_full_solves_ >= parameters_.max_lp_solve_for_feasibility_problems()) {
    return BopOptimizerBase::ABORT;
  }

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
      const int64_t coefficient_a = clause.a.IsPositive() ? 1 : -1;
      const int64_t coefficient_b = clause.b.IsPositive() ? 1 : -1;
      const int64_t rhs = 1 + (clause.a.IsPositive() ? 0 : -1) +
                          (clause.b.IsPositive() ? 0 : -1);
      const ColIndex col_a(clause.a.Variable().value());
      const ColIndex col_b(clause.b.Variable().value());
      const std::string name_a = lp_model_.GetVariableName(col_a);
      const std::string name_b = lp_model_.GetVariableName(col_b);

      lp_model_.SetConstraintName(
          constraint_index,
          (clause.a.IsPositive() ? name_a : "not(" + name_a + ")") + " or " +
              (clause.b.IsPositive() ? name_b : "not(" + name_b + ")"));
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

// Always let the LP solver run if there is an objective. If there isn't, only
// let the LP solver run if the user asked for it by setting
// `BopParameters.max_lp_solve_for_feasibility_problems` to a non-zero value
// (a negative value means no limit).
// TODO(user): also deal with problem_already_solved_
bool LinearRelaxation::ShouldBeRun(const ProblemState& problem_state) const {
  return problem_state.original_problem().objective().literals_size() > 0 ||
         parameters_.max_lp_solve_for_feasibility_problems() != 0;
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
          << absl::StrFormat("%.6f", lp_solver_.GetObjectiveValue())
          << "   status: " << GetProblemStatusString(lp_status);

  if (lp_status == glop::ProblemStatus::OPTIMAL ||
      lp_status == glop::ProblemStatus::IMPRECISE) {
    ++num_full_solves_;
    problem_already_solved_ = true;
  }

  if (lp_status == glop::ProblemStatus::INIT) {
    return BopOptimizerBase::LIMIT_REACHED;
  }
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
              << absl::StrFormat("%.6f", lower_bound)
              << "   using strong branching.";
    }

    const int tolerance_sign = scaling_ < 0 ? 1 : -1;
    const double unscaled_cost =
        (lower_bound +
         tolerance_sign *
             lp_solver_.GetParameters().solution_feasibility_tolerance()) /
            scaling_ -
        offset_;
    learned_info->lower_bound = static_cast<int64_t>(ceil(unscaled_cost));

    if (AllIntegralValues(
            learned_info->lp_values,
            lp_solver_.GetParameters().primal_feasibility_tolerance())) {
      DenseRowToBopSolution(learned_info->lp_values, &learned_info->solution);
      CHECK(learned_info->solution.IsFeasible());
      return BopOptimizerBase::OPTIMAL_SOLUTION_FOUND;
    }
  }

  return BopOptimizerBase::INFORMATION_FOUND;
}

// TODO(user): It is possible to stop the search earlier using the glop
//              parameter objective_lower_limit / objective_upper_limit. That
//              can be used when a feasible solution is known, or when the false
//              best bound is computed.
glop::ProblemStatus LinearRelaxation::Solve(bool incremental_solve,
                                            TimeLimit* time_limit) {
  GlopParameters glop_params;
  if (incremental_solve) {
    glop_params.set_use_dual_simplex(true);
    glop_params.set_allow_simplex_algorithm_change(true);
    glop_params.set_use_preprocessing(false);
    lp_solver_.SetParameters(glop_params);
  }
  NestedTimeLimit nested_time_limit(time_limit, time_limit->GetTimeLeft(),
                                    parameters_.lp_max_deterministic_time());
  const glop::ProblemStatus lp_status = lp_solver_.SolveWithTimeLimit(
      lp_model_, nested_time_limit.GetTimeLimit());
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
                ? std::min(best_lp_objective,
                           std::max(objective_true, objective_false))
                : std::max(best_lp_objective,
                           std::min(objective_true, objective_false));
      }
    }

    if (CostIsWorseThanSolution(objective_true, tolerance)) {
      // Having variable col set to true can't possibly lead to and better
      // solution than the current one. Set the variable to false.
      lp_model_.SetVariableBounds(col, 0.0, 0.0);
      learned_info->fixed_literals.push_back(
          sat::Literal(sat::BooleanVariable(col.value()), false));
    } else if (CostIsWorseThanSolution(objective_false, tolerance)) {
      // Having variable col set to false can't possibly lead to and better
      // solution than the current one. Set the variable to true.
      lp_model_.SetVariableBounds(col, 1.0, 1.0);
      learned_info->fixed_literals.push_back(
          sat::Literal(sat::BooleanVariable(col.value()), true));
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
