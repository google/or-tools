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

#include "ortools/bop/bop_lns.h"

#include <deque>
#include <string>
#include <vector>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/stringprintf.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/cleanup.h"
#include "ortools/base/stl_util.h"
#include "ortools/glop/lp_solver.h"
#include "ortools/lp_data/lp_print_utils.h"
#include "ortools/sat/boolean_problem.h"
#include "ortools/sat/lp_utils.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/bitset.h"

namespace operations_research {
namespace bop {

using ::operations_research::LinearBooleanConstraint;
using ::operations_research::LinearBooleanProblem;
using ::operations_research::glop::ColIndex;
using ::operations_research::glop::DenseRow;
using ::operations_research::glop::LinearProgram;
using ::operations_research::glop::LPSolver;

//------------------------------------------------------------------------------
// BopCompleteLNSOptimizer
//------------------------------------------------------------------------------

namespace {
void UseBopSolutionForSatAssignmentPreference(const BopSolution& solution,
                                              sat::SatSolver* solver) {
  for (int i = 0; i < solution.Size(); ++i) {
    solver->SetAssignmentPreference(
        sat::Literal(sat::BooleanVariable(i), solution.Value(VariableIndex(i))),
        1.0);
  }
}
}  // namespace

BopCompleteLNSOptimizer::BopCompleteLNSOptimizer(
    const std::string& name, const BopConstraintTerms& objective_terms)
    : BopOptimizerBase(name),
      state_update_stamp_(ProblemState::kInitialStampValue),
      objective_terms_(objective_terms) {}

BopCompleteLNSOptimizer::~BopCompleteLNSOptimizer() {}

BopOptimizerBase::Status BopCompleteLNSOptimizer::SynchronizeIfNeeded(
    const ProblemState& problem_state, int num_relaxed_vars) {
  if (state_update_stamp_ == problem_state.update_stamp()) {
    return BopOptimizerBase::CONTINUE;
  }
  state_update_stamp_ = problem_state.update_stamp();

  // Load the current problem to the solver.
  sat_solver_.reset(new sat::SatSolver());
  const BopOptimizerBase::Status status =
      LoadStateProblemToSatSolver(problem_state, sat_solver_.get());
  if (status != BopOptimizerBase::CONTINUE) return status;

  // Add the constraint that forces the solver to look for a solution
  // at a distance <= num_relaxed_vars from the curent one. Note that not all
  // the terms appear in this constraint.
  //
  // TODO(user): if the current solution didn't change, there is no need to
  // re-run this optimizer if we already proved UNSAT.
  std::vector<sat::LiteralWithCoeff> cst;
  for (BopConstraintTerm term : objective_terms_) {
    if (problem_state.solution().Value(term.var_id) && term.weight < 0) {
      cst.push_back(sat::LiteralWithCoeff(
          sat::Literal(sat::BooleanVariable(term.var_id.value()), false), 1.0));
    } else if (!problem_state.solution().Value(term.var_id) &&
               term.weight > 0) {
      cst.push_back(sat::LiteralWithCoeff(
          sat::Literal(sat::BooleanVariable(term.var_id.value()), true), 1.0));
    }
  }
  sat_solver_->AddLinearConstraint(
      /*use_lower_bound=*/false, sat::Coefficient(0),
      /*use_upper_bound=*/true, sat::Coefficient(num_relaxed_vars), &cst);

  if (sat_solver_->IsModelUnsat()) return BopOptimizerBase::ABORT;

  // It sounds like a good idea to force the solver to find a similar solution
  // from the current one. On another side, this is already somewhat enforced by
  // the constraint above, so it will need more investigation.
  UseBopSolutionForSatAssignmentPreference(problem_state.solution(),
                                           sat_solver_.get());
  return BopOptimizerBase::CONTINUE;
}

bool BopCompleteLNSOptimizer::ShouldBeRun(
    const ProblemState& problem_state) const {
  return problem_state.solution().IsFeasible();
}

BopOptimizerBase::Status BopCompleteLNSOptimizer::Optimize(
    const BopParameters& parameters, const ProblemState& problem_state,
    LearnedInfo* learned_info, TimeLimit* time_limit) {
  SCOPED_TIME_STAT(&stats_);
  CHECK(learned_info != nullptr);
  CHECK(time_limit != nullptr);
  learned_info->Clear();

  const BopOptimizerBase::Status sync_status =
      SynchronizeIfNeeded(problem_state, parameters.num_relaxed_vars());
  if (sync_status != BopOptimizerBase::CONTINUE) {
    return sync_status;
  }

  CHECK(sat_solver_ != nullptr);
  const double initial_dt = sat_solver_->deterministic_time();
  auto advance_dt = ::operations_research::util::MakeCleanup([initial_dt, this, &time_limit]() {
    time_limit->AdvanceDeterministicTime(sat_solver_->deterministic_time() -
                                         initial_dt);
  });

  // Set the parameters for this run.
  // TODO(user): Because of this, we actually loose the perfect continuity
  // between runs, and the restart policy is resetted... Fix this.
  sat::SatParameters sat_params;
  sat_params.set_max_number_of_conflicts(
      parameters.max_number_of_conflicts_in_random_lns());
  sat_params.set_max_time_in_seconds(time_limit->GetTimeLeft());
  sat_params.set_max_deterministic_time(time_limit->GetDeterministicTimeLeft());
  sat_params.set_random_seed(parameters.random_seed());

  sat_solver_->SetParameters(sat_params);
  const sat::SatSolver::Status sat_status = sat_solver_->Solve();
  if (sat_status == sat::SatSolver::MODEL_SAT) {
    SatAssignmentToBopSolution(sat_solver_->Assignment(),
                               &learned_info->solution);
    return BopOptimizerBase::SOLUTION_FOUND;
  }
  if (sat_status == sat::SatSolver::LIMIT_REACHED) {
    return BopOptimizerBase::CONTINUE;
  }

  // Because of the "LNS" constraint, we can't deduce anything about the problem
  // in this case.
  return BopOptimizerBase::ABORT;
}

//------------------------------------------------------------------------------
// BopAdaptiveLNSOptimizer
//------------------------------------------------------------------------------

namespace {
// Returns false if the limit is reached while solving the LP.
bool UseLinearRelaxationForSatAssignmentPreference(
    const BopParameters& parameters, const LinearBooleanProblem& problem,
    sat::SatSolver* sat_solver, TimeLimit* time_limit) {
  // TODO(user): Re-use the lp_model and lp_solver or build a model with only
  //              needed constraints and variables.
  glop::LinearProgram lp_model;
  sat::ConvertBooleanProblemToLinearProgram(problem, &lp_model);

  // Set bounds of variables fixed by the sat_solver.
  const sat::Trail& propagation_trail = sat_solver->LiteralTrail();
  for (int trail_index = 0; trail_index < propagation_trail.Index();
       ++trail_index) {
    const sat::Literal fixed_literal = propagation_trail[trail_index];
    const glop::Fractional value = fixed_literal.IsPositive() ? 1.0 : 0.0;
    lp_model.SetVariableBounds(ColIndex(fixed_literal.Variable().value()),
                               value, value);
  }

  glop::LPSolver lp_solver;
  NestedTimeLimit nested_time_limit(time_limit, time_limit->GetTimeLeft(),
                                    parameters.lp_max_deterministic_time());
  const glop::ProblemStatus lp_status =
      lp_solver.SolveWithTimeLimit(lp_model, nested_time_limit.GetTimeLimit());

  if (lp_status != glop::ProblemStatus::OPTIMAL &&
      lp_status != glop::ProblemStatus::PRIMAL_FEASIBLE &&
      lp_status != glop::ProblemStatus::IMPRECISE) {
    // We have no useful information from the LP, we will abort this LNS.
    return false;
  }

  // Set preferences based on the solution of the relaxation.
  for (ColIndex col(0); col < lp_solver.variable_values().size(); ++col) {
    const double value = lp_solver.variable_values()[col];
    sat_solver->SetAssignmentPreference(
        sat::Literal(sat::BooleanVariable(col.value()), round(value) == 1),
        1 - fabs(value - round(value)));
  }
  return true;
}
}  // namespace

// Note(user): We prefer to start with a really low difficulty as this works
// better for large problem, and for small ones, it will be really quickly
// increased anyway. Maybe a better appproach is to start by relaxing something
// like 10 variables instead of having a fixed percentage.
BopAdaptiveLNSOptimizer::BopAdaptiveLNSOptimizer(
    const std::string& name, bool use_lp_to_guide_sat,
    NeighborhoodGenerator* neighborhood_generator,
    sat::SatSolver* sat_propagator)
    : BopOptimizerBase(name),
      use_lp_to_guide_sat_(use_lp_to_guide_sat),
      neighborhood_generator_(neighborhood_generator),
      sat_propagator_(sat_propagator),
      adaptive_difficulty_(0.001) {
  CHECK(sat_propagator != nullptr);
}

BopAdaptiveLNSOptimizer::~BopAdaptiveLNSOptimizer() {}

bool BopAdaptiveLNSOptimizer::ShouldBeRun(
    const ProblemState& problem_state) const {
  return problem_state.solution().IsFeasible();
}

BopOptimizerBase::Status BopAdaptiveLNSOptimizer::Optimize(
    const BopParameters& parameters, const ProblemState& problem_state,
    LearnedInfo* learned_info, TimeLimit* time_limit) {
  SCOPED_TIME_STAT(&stats_);
  CHECK(learned_info != nullptr);
  CHECK(time_limit != nullptr);
  learned_info->Clear();

  // Set-up a sat_propagator_ cleanup task to catch all the exit cases.
  const double initial_dt = sat_propagator_->deterministic_time();
  auto sat_propagator_cleanup =
      ::operations_research::util::MakeCleanup([initial_dt, this, &learned_info, &time_limit]() {
        if (!sat_propagator_->IsModelUnsat()) {
          sat_propagator_->SetAssumptionLevel(0);
          sat_propagator_->RestoreSolverToAssumptionLevel();
          ExtractLearnedInfoFromSatSolver(sat_propagator_, learned_info);
        }
        time_limit->AdvanceDeterministicTime(
            sat_propagator_->deterministic_time() - initial_dt);
      });

  // For the SAT conflicts limit of each LNS, we follow a luby sequence times
  // the base number of conflicts (num_conflicts_). Note that the numbers of the
  // Luby sequence are always power of two.
  //
  // We dynamically change the size of the neighborhood depending on the
  // difficulty of the problem. There is one "target" difficulty for each
  // different numbers in the Luby sequence. Note that the initial value is
  // reused from the last run.
  BopParameters local_parameters = parameters;
  int num_tries = 0;  // TODO(user): remove? our limit is 1 by default.
  while (!time_limit->LimitReached() &&
         num_tries < local_parameters.num_random_lns_tries()) {
    // Compute the target problem difficulty and generate the neighborhood.
    adaptive_difficulty_.UpdateLuby();
    const double difficulty = adaptive_difficulty_.GetParameterValue();
    neighborhood_generator_->GenerateNeighborhood(problem_state, difficulty,
                                                  sat_propagator_);

    ++num_tries;
    VLOG(2) << num_tries << "  difficulty:" << difficulty
            << "  luby:" << adaptive_difficulty_.luby_value()
            << "  fixed:" << sat_propagator_->LiteralTrail().Index() << "/"
            << problem_state.original_problem().num_variables();

    // Special case if the difficulty is too high.
    if (!sat_propagator_->IsModelUnsat()) {
      if (sat_propagator_->CurrentDecisionLevel() == 0) {
        VLOG(2) << "Nothing fixed!";
        adaptive_difficulty_.DecreaseParameter();
        continue;
      }
    }

    // Since everything is already set-up, we try the sat_propagator_ with
    // a really low conflict limit. This allow to quickly skip over UNSAT
    // cases without the costly new problem setup.
    if (!sat_propagator_->IsModelUnsat()) {
      sat::SatParameters params;
      params.set_max_number_of_conflicts(
          local_parameters.max_number_of_conflicts_for_quick_check());
      params.set_max_time_in_seconds(time_limit->GetTimeLeft());
      params.set_max_deterministic_time(time_limit->GetDeterministicTimeLeft());
      params.set_random_seed(parameters.random_seed());
      sat_propagator_->SetParameters(params);
      sat_propagator_->SetAssumptionLevel(
          sat_propagator_->CurrentDecisionLevel());

      const sat::SatSolver::Status status = sat_propagator_->Solve();
      if (status == sat::SatSolver::MODEL_SAT) {
        adaptive_difficulty_.IncreaseParameter();
        SatAssignmentToBopSolution(sat_propagator_->Assignment(),
                                   &learned_info->solution);
        return BopOptimizerBase::SOLUTION_FOUND;
      } else if (status == sat::SatSolver::ASSUMPTIONS_UNSAT) {
        // Local problem is infeasible.
        adaptive_difficulty_.IncreaseParameter();
        continue;
      }
    }

    // Restore to the assumption level.
    // This is call is important since all the fixed variable in the
    // propagator_ will be used to construct the local problem below.
    // Note that calling RestoreSolverToAssumptionLevel() might actually prove
    // the infeasibility. It is important to check the UNSAT status afterward.
    if (!sat_propagator_->IsModelUnsat()) {
      sat_propagator_->RestoreSolverToAssumptionLevel();
    }

    // Check if the problem is proved UNSAT, by previous the search or the
    // RestoreSolverToAssumptionLevel() call above.
    if (sat_propagator_->IsModelUnsat()) {
      return problem_state.solution().IsFeasible()
                 ? BopOptimizerBase::OPTIMAL_SOLUTION_FOUND
                 : BopOptimizerBase::INFEASIBLE;
    }

    // Construct and Solve the LNS subproblem.
    //
    // Note that we don't use the sat_propagator_ all the way because using a
    // clean solver on a really small problem is usually a lot faster (even we
    // the time to create the subproblem) that running a long solve under
    // assumption (like we did above with a really low conflit limit).
    const int conflict_limit =
        adaptive_difficulty_.luby_value() *
        parameters.max_number_of_conflicts_in_random_lns();

    sat::SatParameters params;
    params.set_max_number_of_conflicts(conflict_limit);
    params.set_max_time_in_seconds(time_limit->GetTimeLeft());
    params.set_max_deterministic_time(time_limit->GetDeterministicTimeLeft());
    params.set_random_seed(parameters.random_seed());

    sat::SatSolver sat_solver;
    sat_solver.SetParameters(params);

    // Starts by adding the unit clauses to fix the variables.
    const LinearBooleanProblem& problem = problem_state.original_problem();
    sat_solver.SetNumVariables(problem.num_variables());
    for (int i = 0; i < sat_propagator_->LiteralTrail().Index(); ++i) {
      CHECK(sat_solver.AddUnitClause(sat_propagator_->LiteralTrail()[i]));
    }

    // Load the rest of the problem. This will automatically create the small
    // local subproblem using the already fixed variable.
    //
    // TODO(user): modify LoadStateProblemToSatSolver() so that we can call it
    // instead and don't need to over constraint the objective below. As a
    // bonus we will also have the learned binary clauses.
    if (!LoadBooleanProblem(problem, &sat_solver)) {
      // The local problem is infeasible.
      adaptive_difficulty_.IncreaseParameter();
      continue;
    }

    if (use_lp_to_guide_sat_) {
      if (!UseLinearRelaxationForSatAssignmentPreference(
              parameters, problem, &sat_solver, time_limit)) {
        return BopOptimizerBase::LIMIT_REACHED;
      }
    } else {
      UseObjectiveForSatAssignmentPreference(problem, &sat_solver);
    }

    if (!AddObjectiveUpperBound(
            problem, sat::Coefficient(problem_state.solution().GetCost()) - 1,
            &sat_solver)) {
      // The local problem is infeasible.
      adaptive_difficulty_.IncreaseParameter();
      continue;
    }

    // Solve the local problem.
    const sat::SatSolver::Status status = sat_solver.Solve();
    time_limit->AdvanceDeterministicTime(sat_solver.deterministic_time());
    if (status == sat::SatSolver::MODEL_SAT) {
      // We found a solution! abort now.
      SatAssignmentToBopSolution(sat_solver.Assignment(),
                                 &learned_info->solution);
      return BopOptimizerBase::SOLUTION_FOUND;
    }

    // Adapt the difficulty.
    if (sat_solver.num_failures() < 0.5 * conflict_limit) {
      adaptive_difficulty_.IncreaseParameter();
    } else if (sat_solver.num_failures() > 0.95 * conflict_limit) {
      adaptive_difficulty_.DecreaseParameter();
    }
  }

  return BopOptimizerBase::CONTINUE;
}

//------------------------------------------------------------------------------
// Neighborhood generators.
//------------------------------------------------------------------------------

namespace {

std::vector<sat::Literal> ObjectiveVariablesAssignedToTheirLowCostValue(
    const ProblemState& problem_state,
    const BopConstraintTerms& objective_terms) {
  std::vector<sat::Literal> result;
  DCHECK(problem_state.solution().IsFeasible());
  for (const BopConstraintTerm& term : objective_terms) {
    if (((problem_state.solution().Value(term.var_id) && term.weight < 0) ||
         (!problem_state.solution().Value(term.var_id) && term.weight > 0))) {
      result.push_back(
          sat::Literal(sat::BooleanVariable(term.var_id.value()),
                       problem_state.solution().Value(term.var_id)));
    }
  }
  return result;
}

}  // namespace

void ObjectiveBasedNeighborhood::GenerateNeighborhood(
    const ProblemState& problem_state, double difficulty,
    sat::SatSolver* sat_propagator) {
  // Generate the set of variable we may fix and randomize their order.
  std::vector<sat::Literal> candidates =
      ObjectiveVariablesAssignedToTheirLowCostValue(problem_state,
                                                    objective_terms_);
  std::random_shuffle(candidates.begin(), candidates.end(), *random_);

  // We will use the sat_propagator to fix some variables as long as the number
  // of propagated variables in the solver is under our target.
  const int num_variables = sat_propagator->NumVariables();
  const int target = round((1.0 - difficulty) * num_variables);

  sat_propagator->Backtrack(0);
  for (const sat::Literal literal : candidates) {
    if (sat_propagator->LiteralTrail().Index() == target) break;
    if (sat_propagator->LiteralTrail().Index() > target) {
      // We prefer to error on the large neighborhood side, so we backtrack the
      // last enqueued literal.
      sat_propagator->Backtrack(
          std::max(0, sat_propagator->CurrentDecisionLevel() - 1));
      break;
    }
    sat_propagator->EnqueueDecisionAndBacktrackOnConflict(literal);
    if (sat_propagator->IsModelUnsat()) return;
  }
}

void ConstraintBasedNeighborhood::GenerateNeighborhood(
    const ProblemState& problem_state, double difficulty,
    sat::SatSolver* sat_propagator) {
  // Randomize the set of constraint
  const LinearBooleanProblem& problem = problem_state.original_problem();
  const int num_constraints = problem.constraints_size();
  std::vector<int> ct_ids(num_constraints, 0);
  for (int ct_id = 0; ct_id < num_constraints; ++ct_id) ct_ids[ct_id] = ct_id;
  std::random_shuffle(ct_ids.begin(), ct_ids.end(), *random_);

  // Mark that we want to relax all the variables of these constraints as long
  // as the number of relaxed variable is lower than our difficulty target.
  const int num_variables = sat_propagator->NumVariables();
  const int target = round(difficulty * num_variables);
  int num_relaxed = 0;
  std::vector<bool> variable_is_relaxed(problem.num_variables(), false);
  for (int i = 0; i < ct_ids.size(); ++i) {
    if (num_relaxed >= target) break;
    const LinearBooleanConstraint& constraint = problem.constraints(ct_ids[i]);

    // We exclude really large constraints since they are probably note helpful
    // in picking a nice neighborhood.
    if (constraint.literals_size() > 0.7 * num_variables) continue;

    for (int j = 0; j < constraint.literals_size(); ++j) {
      const VariableIndex var_id(constraint.literals(j) - 1);
      if (!variable_is_relaxed[var_id.value()]) {
        ++num_relaxed;
        variable_is_relaxed[var_id.value()] = true;
      }
    }
  }

  // Basic version: simply fix all the "to_fix" variable that are not relaxed.
  //
  // TODO(user): Not fixing anything that propagates a variable in
  // variable_is_relaxed may be better. It is actually a lot better in the
  // RelationGraphBasedNeighborhood. To investigate.
  sat_propagator->Backtrack(0);
  const std::vector<sat::Literal> to_fix =
      ObjectiveVariablesAssignedToTheirLowCostValue(problem_state,
                                                    objective_terms_);
  for (const sat::Literal literal : to_fix) {
    if (variable_is_relaxed[literal.Variable().value()]) continue;
    sat_propagator->EnqueueDecisionAndBacktrackOnConflict(literal);
    if (sat_propagator->IsModelUnsat()) return;
  }
}

RelationGraphBasedNeighborhood::RelationGraphBasedNeighborhood(
    const LinearBooleanProblem& problem, MTRandom* random)
    : random_(random) {
  const int num_variables = problem.num_variables();
  columns_.resize(num_variables);

  // We will ignore constraints that have more variables than this percentage of
  // the total number of variables in this neighborhood computation.
  //
  // TODO(user): Factor this out with the similar factor in
  // ConstraintBasedNeighborhood? also maybe a better approach is to order the
  // constraint, and stop the neighborhood extension without considering all of
  // them.
  const double kSizeThreshold = 0.1;
  for (int i = 0; i < problem.constraints_size(); ++i) {
    const LinearBooleanConstraint& constraint = problem.constraints(i);
    if (constraint.literals_size() > kSizeThreshold * num_variables) continue;
    for (int j = 0; j < constraint.literals_size(); ++j) {
      const sat::Literal literal(constraint.literals(j));
      columns_[VariableIndex(literal.Variable().value())].push_back(
          ConstraintIndex(i));
    }
  }
}

void RelationGraphBasedNeighborhood::GenerateNeighborhood(
    const ProblemState& problem_state, double difficulty,
    sat::SatSolver* sat_propagator) {
  // Simply walk the graph until enough variable are relaxed.
  const int num_variables = sat_propagator->NumVariables();
  const int target = round(difficulty * num_variables);
  int num_relaxed = 1;
  std::vector<bool> variable_is_relaxed(num_variables, false);
  std::deque<int> queue;

  // TODO(user): If one plan to try of lot of different LNS, maybe it will be
  // better to try to bias the distribution of "center" to be as spread as
  // possible.
  queue.push_back(random_->Uniform(num_variables));
  variable_is_relaxed[queue.back()] = true;
  while (!queue.empty() && num_relaxed < target) {
    const int var = queue.front();
    queue.pop_front();
    for (ConstraintIndex ct_index : columns_[VariableIndex(var)]) {
      const LinearBooleanConstraint& constraint =
          problem_state.original_problem().constraints(ct_index.value());
      for (int i = 0; i < constraint.literals_size(); ++i) {
        const sat::Literal literal(constraint.literals(i));
        const int next_var = literal.Variable().value();
        if (!variable_is_relaxed[next_var]) {
          ++num_relaxed;
          variable_is_relaxed[next_var] = true;
          queue.push_back(next_var);
        }
      }
    }
  }

  // Loops over all the variables in order and only fix the ones that don't
  // propagate any relaxed variables.
  DCHECK(problem_state.solution().IsFeasible());
  sat_propagator->Backtrack(0);
  for (sat::BooleanVariable var(0); var < num_variables; ++var) {
    const sat::Literal literal(
        var, problem_state.solution().Value(VariableIndex(var.value())));
    if (variable_is_relaxed[literal.Variable().value()]) continue;
    const int index =
        sat_propagator->EnqueueDecisionAndBacktrackOnConflict(literal);
    if (sat_propagator->CurrentDecisionLevel() > 0) {
      for (int i = index; i < sat_propagator->LiteralTrail().Index(); ++i) {
        if (variable_is_relaxed
                [sat_propagator->LiteralTrail()[i].Variable().value()]) {
          sat_propagator->Backtrack(sat_propagator->CurrentDecisionLevel() - 1);
        }
      }
    }
    if (sat_propagator->IsModelUnsat()) return;
  }
  VLOG(2) << "target:" << target << " relaxed:" << num_relaxed << " actual:"
          << num_variables - sat_propagator->LiteralTrail().Index();
}

}  // namespace bop
}  // namespace operations_research
