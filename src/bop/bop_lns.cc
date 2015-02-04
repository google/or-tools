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

#include "bop/bop_lns.h"

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

namespace operations_research {
namespace bop {
namespace {
// Creates and solves a LP model which enforces a solution with a better cost,
// and minimizes the distance to the initial solution.
void CreateLinearProgramFromCurrentBopSolution(
    const LinearBooleanProblem& problem, const BopSolution& initial_solution,
    glop::LinearProgram* lp) {
  const int64 initial_cost = initial_solution.GetCost();

  lp->Clear();
  for (int i = 0; i < 2 * problem.num_variables(); ++i) {
    const glop::ColIndex col = lp->CreateNewVariable();
    lp->SetVariableBounds(col, 0.0, 1.0);
  }
  glop::RowIndex constraint_index(0);
  for (const LinearBooleanConstraint& constraint : problem.constraints()) {
    double sum = 0.0;
    for (int i = 0; i < constraint.literals_size(); ++i) {
      const int literal = constraint.literals(i);
      const double coeff = constraint.coefficients(i);
      const glop::ColIndex variable_index = glop::ColIndex(abs(literal) - 1);
      if (literal < 0) {
        sum += coeff;
        lp->SetCoefficient(constraint_index, variable_index, -coeff);
      } else {
        lp->SetCoefficient(constraint_index, variable_index, coeff);
      }
    }
    lp->SetConstraintBounds(
        constraint_index,
        constraint.has_lower_bound() ? constraint.lower_bound() - sum
                                     : -glop::kInfinity,
        constraint.has_upper_bound() ? constraint.upper_bound() - sum
                                     : glop::kInfinity);
    ++constraint_index;
  }

  const LinearObjective& objective = problem.objective();
  double offset = objective.offset();
  for (int i = 0; i < objective.literals_size(); ++i) {
    const int literal = objective.literals(i);
    const double coeff = objective.coefficients(i);
    const glop::ColIndex variable_index = glop::ColIndex(abs(literal) - 1);
    if (literal < 0) {
      offset += coeff;
      lp->SetCoefficient(constraint_index, variable_index, -coeff);
    } else {
      lp->SetCoefficient(constraint_index, variable_index, coeff);
    }
  }
  const int64 objective_bound = initial_cost - offset - 1;
  lp->SetConstraintBounds(constraint_index, -glop::kInfinity, objective_bound);
  ++constraint_index;

  // Add constraint for each new variable.
  for (glop::ColIndex var_id(problem.num_variables());
       var_id < 2 * problem.num_variables(); ++var_id) {
    const glop::ColIndex lp_var_id(var_id - problem.num_variables());
    const VariableIndex bop_var_id(lp_var_id.value());
    const bool solution_value = initial_solution.Value(bop_var_id);
    if (solution_value) {
      lp->SetCoefficient(constraint_index, lp_var_id, 1);
      lp->SetCoefficient(constraint_index, var_id, -1);
      lp->SetConstraintBounds(constraint_index, 0, 0);
    } else {
      lp->SetCoefficient(constraint_index, lp_var_id, 1);
      lp->SetCoefficient(constraint_index, var_id, 1);
      lp->SetConstraintBounds(constraint_index, 1, 1);
    }
    ++constraint_index;
  }

  // Objective
  for (glop::ColIndex var_id(problem.num_variables());
       var_id < 2 * problem.num_variables(); ++var_id) {
    lp->SetObjectiveCoefficient(var_id, 1);
  }

  lp->SetMaximizationProblem(true);
}

void ComputeVariablesToFixFromToRelax(
    const BopSolution& initial_solution,
    const ITIVector<SparseIndex, BopConstraintTerm>& objective_terms,
    const SparseBitset<VariableIndex>& to_relax, std::vector<sat::Literal>* output) {
  output->clear();
  for (BopConstraintTerm term : objective_terms) {
    if (!to_relax[term.var_id] &&
        ((initial_solution.Value(term.var_id) && term.weight < 0) ||
         (!initial_solution.Value(term.var_id) && term.weight > 0))) {
      output->push_back(sat::Literal(sat::VariableIndex(term.var_id.value()),
                                     initial_solution.Value(term.var_id)));
    }
  }
}

enum DifficultyType {
  OK,
  NOTHING_FIXED,
  EVERYTHING_FIXED,
  UNSAT,
};

DifficultyType ComputeVariablesToFixFromSat(
    const std::vector<bool>& is_in_random_order,
    const std::vector<sat::Literal>& random_order, double target_difficulty,
    sat::SatSolver* sat_solver, std::vector<sat::Literal>* output) {
  // We will use the sat_solver_ to fix some variables of 'random_order'
  // as long as the number_of propagated variable in is_in_random_order is lower
  // than what we want to relax.
  sat_solver->Backtrack(0);
  int num_relaxed_variables = random_order.size();
  const int target_num_relaxed_variables =
      round(target_difficulty * random_order.size());
  if (target_num_relaxed_variables == 0) {
    return DifficultyType::EVERYTHING_FIXED;
  }

  // Remove fixed variables from the picture.
  for (int i = 0; i < sat_solver->LiteralTrail().Index(); ++i) {
    if (is_in_random_order[sat_solver->LiteralTrail()[i].Variable().value()]) {
      --num_relaxed_variables;
    }
  }

  for (int i = 0; i < random_order.size(); ++i) {
    if (sat_solver->Assignment().IsVariableAssigned(
            random_order[i].Variable())) {
      continue;
    }

    // Note that since we only enqueue variable from a feasible solution,
    // they can only lead to a conflict because of the new objective
    // constraint...
    const int current_decision_level = sat_solver->CurrentDecisionLevel();
    int first_trail_index =
        sat_solver->EnqueueDecisionAndBackjumpOnConflict(random_order[i]);
    if (first_trail_index == sat::kUnsatTrailIndex) {
      return DifficultyType::UNSAT;
    }
    if (sat_solver->CurrentDecisionLevel() <= current_decision_level) {
      // there was a conflict, continue.
      // Note that we need to recompute num_relaxed_variables from scratch.
      num_relaxed_variables = random_order.size();
      first_trail_index = 0;
    }
    for (int index = first_trail_index;
         index < sat_solver->LiteralTrail().Index(); ++index) {
      if (is_in_random_order
              [sat_solver->LiteralTrail()[index].Variable().value()]) {
        --num_relaxed_variables;
      }
    }
    if (num_relaxed_variables == target_num_relaxed_variables) break;
    if (num_relaxed_variables < target_num_relaxed_variables) {
      sat_solver->Backtrack(std::max(0, sat_solver->CurrentDecisionLevel() - 1));
      break;
    }
  }
  // When the SAT decision level is zero it means that we didn't fix any
  // variables. This happens when the target_difficulty is too high.
  if (sat_solver->CurrentDecisionLevel() == 0) {
    sat_solver->Backtrack(0);
    return DifficultyType::NOTHING_FIXED;
  }

  output->clear();
  for (int i = 0; i < sat_solver->LiteralTrail().Index(); ++i) {
    output->push_back(sat_solver->LiteralTrail()[i]);
  }
  sat_solver->Backtrack(0);
  return DifficultyType::OK;
}

BopOptimizerBase::Status SolveLNSProblem(
    const LinearBooleanProblem& problem, const BopSolution& initial_solution,
    const BopParameters& bop_parameters,
    const std::vector<sat::Literal>& fixed_variables, BopSolution* solution,
    TimeLimit* time_limit, int* num_conflicts_used) {
  CHECK(solution != nullptr);
  CHECK(time_limit != nullptr);

  const int64 initial_cost = initial_solution.GetCost();
  sat::SatParameters sat_parameters;
  sat_parameters.set_max_number_of_conflicts(
      bop_parameters.max_number_of_conflicts_in_random_lns());

  sat::SatSolver sat_solver;
  sat_parameters.set_max_time_in_seconds(time_limit->GetTimeLeft());
  sat_parameters.set_max_deterministic_time(
      time_limit->GetDeterministicTimeLeft());
  sat_solver.SetParameters(sat_parameters);

  // Starts by adding the unit clauses to fix the variables.
  sat_solver.SetNumVariables(problem.num_variables());
  for (sat::Literal literal : fixed_variables) {
    CHECK(sat_solver.AddUnitClause(literal));
  }

  // Then load the rest of the problem. Since we fixed variables from a
  // feasible assignment, this should always be true.
  if (!LoadBooleanProblem(problem, &sat_solver)) {
    return BopOptimizerBase::INFEASIBLE;
  }
  UseObjectiveForSatAssignmentPreference(problem, &sat_solver);

  // Add the objective constraint and solve the problem. Note that it is
  // possible that the problem is detected to be unsat as soon as this
  // constraint is added.
  sat::SatSolver::Status sat_status = sat::SatSolver::MODEL_UNSAT;
  if (AddObjectiveUpperBound(problem, sat::Coefficient(initial_cost) - 1,
                             &sat_solver)) {
    sat_status = sat_solver.Solve();
  }
  if (sat_status == sat::SatSolver::MODEL_SAT) {
    SatAssignmentToBopSolution(sat_solver.Assignment(), solution);
  }
  *num_conflicts_used = sat_solver.num_failures();

  time_limit->AdvanceDeterministicTime(sat_solver.deterministic_time());
  return sat_status == sat::SatSolver::MODEL_SAT
             ? BopOptimizerBase::SOLUTION_FOUND
             : BopOptimizerBase::LIMIT_REACHED;
}

BopOptimizerBase::Status SolveLNSProblemWithSatPropagator(
    const std::vector<sat::Literal>& fixed_variables, bool setup_already_done,
    const BopParameters& bop_parameters, sat::SatSolver* sat_solver,
    BopSolution* solution, TimeLimit* time_limit, int* num_conflicts_used) {
  const sat::SatParameters old_parameters = sat_solver->parameters();
  sat::SatParameters new_parameters = old_parameters;
  new_parameters.set_max_number_of_conflicts(
      bop_parameters.max_number_of_conflicts_in_random_lns());
  new_parameters.set_max_time_in_seconds(time_limit->GetTimeLeft());
  new_parameters.set_max_deterministic_time(
      time_limit->GetDeterministicTimeLeft());
  sat_solver->SetParameters(new_parameters);

  if (!setup_already_done) {
    // Adds the assumptions before the Solve().
    sat_solver->Backtrack(0);
    for (sat::Literal literal : fixed_variables) {
      if (!sat_solver->Assignment().IsVariableAssigned(literal.Variable())) {
        const int current_decision_level = sat_solver->CurrentDecisionLevel();
        sat_solver->EnqueueDecisionAndBackjumpOnConflict(literal);
        CHECK_GT(sat_solver->CurrentDecisionLevel(), current_decision_level);
      }
    }
  }

  // Solve it.
  const int old_num_conflicts = sat_solver->num_failures();
  sat_solver->SetAssumptionLevel(sat_solver->CurrentDecisionLevel());
  sat::SatSolver::Status sat_status = sat_solver->Solve();
  sat_solver->SetAssumptionLevel(0);
  if (sat_status == sat::SatSolver::MODEL_SAT) {
    SatAssignmentToBopSolution(sat_solver->Assignment(), solution);
  }

  *num_conflicts_used = sat_solver->num_failures() - old_num_conflicts;
  sat_solver->SetParameters(old_parameters);
  return sat_status == sat::SatSolver::MODEL_SAT
             ? BopOptimizerBase::SOLUTION_FOUND
             : BopOptimizerBase::LIMIT_REACHED;
}
}  // anonymous namespace.

//------------------------------------------------------------------------------
// SatPropagator
//------------------------------------------------------------------------------
SatPropagator::SatPropagator(const LinearBooleanProblem& problem)
    : problem_(problem),
      sat_solver_(),
      solution_cost_(kint64max),
      symmetries_generators_(),
      propagated_by_(problem_.num_variables()) {}

bool SatPropagator::OverConstrainObjective(
    const BopSolution& current_solution) {
  if (!sat_solver_) {
    // Save the solution cost to use it as an upper bound when the SAT solver
    // is created.
    solution_cost_ = current_solution.GetCost();
    return true;
  }

  if (sat_solver_->CurrentDecisionLevel() > 0) {
    sat_solver_->Backtrack(0);
  }
  return AddObjectiveUpperBound(
      problem_, sat::Coefficient(current_solution.GetCost() - 1),
      sat_solver_.get());
}

void SatPropagator::AddPropagationRelation(sat::Literal decision_literal,
                                           VariableIndex var_id) {
  const std::vector<sat::Literal>::iterator& iter =
      lower_bound(propagated_by_[var_id].begin(), propagated_by_[var_id].end(),
                  decision_literal);
  if (iter == propagated_by_[var_id].end()) {
    // Add the variable at the end as all variables are smaller.
    propagated_by_[var_id].push_back(decision_literal);
  } else if (iter->Index() != decision_literal.Index()) {
    propagated_by_[var_id].insert(iter, decision_literal);
  }
}

void SatPropagator::AddSymmetries(
    std::vector<std::unique_ptr<SparsePermutation>>* generators) {
  if (!sat_solver_) {
    // Delay the symmetries assignment.
    std::move(generators->begin(), generators->end(),
              std::back_inserter(symmetries_generators_));
  } else {
    sat_solver_->AddSymmetries(generators);
  }
}

sat::SatSolver* SatPropagator::GetSatSolver() {
  if (!sat_solver_) {
    sat_solver_.reset(new sat::SatSolver());
    sat::LoadBooleanProblem(problem_, sat_solver_.get());
    UseObjectiveForSatAssignmentPreference(problem_, sat_solver_.get());
    if (solution_cost_ != kint64max) {
      AddObjectiveUpperBound(problem_, sat::Coefficient(solution_cost_ - 1),
                             sat_solver_.get());
    }
    if (!symmetries_generators_.empty()) {
      sat_solver_->AddSymmetries(&symmetries_generators_);
      symmetries_generators_.clear();
    }
  }
  return sat_solver_.get();
}

//------------------------------------------------------------------------------
// BopCompleteLNSOptimizer
//------------------------------------------------------------------------------
BopCompleteLNSOptimizer::BopCompleteLNSOptimizer(
    const std::string& name, const BopConstraintTerms& objective_terms)
    : BopOptimizerBase(name),
      state_update_stamp_(ProblemState::kInitialStampValue),
      problem_(),
      initial_solution_(),
      objective_terms_(objective_terms) {}

BopCompleteLNSOptimizer::~BopCompleteLNSOptimizer() {}

BopOptimizerBase::Status BopCompleteLNSOptimizer::SynchronizeIfNeeded(
    const ProblemState& problem_state) {
  if (state_update_stamp_ == problem_state.update_stamp()) {
    return BopOptimizerBase::CONTINUE;
  }
  state_update_stamp_ = problem_state.update_stamp();

  problem_ = problem_state.original_problem();
  initial_solution_.reset(new BopSolution(problem_state.solution()));
  return BopOptimizerBase::CONTINUE;
}

BopOptimizerBase::Status BopCompleteLNSOptimizer::Optimize(
    const BopParameters& parameters, const ProblemState& problem_state,
    LearnedInfo* learned_info, TimeLimit* time_limit) {
  SCOPED_TIME_STAT(&stats_);
  CHECK(learned_info != nullptr);
  CHECK(time_limit != nullptr);
  learned_info->Clear();

  const BopOptimizerBase::Status sync_status =
      SynchronizeIfNeeded(problem_state);
  if (sync_status != BopOptimizerBase::CONTINUE) {
    return sync_status;
  }

  LinearBooleanConstraint* constraint = problem_.add_constraints();
  for (BopConstraintTerm term : objective_terms_) {
    if (initial_solution_->Value(term.var_id) && term.weight < 0) {
      constraint->add_literals(term.var_id.value() + 1);
      constraint->add_coefficients(1);
    } else if (!initial_solution_->Value(term.var_id) && term.weight > 0) {
      constraint->add_literals(-(term.var_id.value() + 1));
      constraint->add_coefficients(1);
    }
  }
  constraint->set_lower_bound(constraint->literals_size() -
                              parameters.num_relaxed_vars());

  // Relax all variables of the objective.
  std::vector<sat::Literal> fixed_variables;
  int num_conflicts_used = 0;
  return SolveLNSProblem(problem_, *initial_solution_, parameters,
                         fixed_variables, &learned_info->solution, time_limit,
                         &num_conflicts_used);
}

//------------------------------------------------------------------------------
// BopRandomLNSOptimizer
//------------------------------------------------------------------------------
BopRandomLNSOptimizer::BopRandomLNSOptimizer(
    const std::string& name, const BopConstraintTerms& objective_terms,
    int random_seed, bool use_sat_to_choose_lns_neighbourhood,
    SatPropagator* sat_propagator)
    : BopOptimizerBase(name),
      state_update_stamp_(ProblemState::kInitialStampValue),
      problem_(nullptr),
      initial_solution_(),
      use_sat_to_choose_lns_neighbourhood_(use_sat_to_choose_lns_neighbourhood),
      sat_propagator_(sat_propagator),
      objective_terms_(objective_terms),
      random_(random_seed),
      adaptive_difficulty_(),
      to_relax_() {
  CHECK(sat_propagator != nullptr);
}

BopRandomLNSOptimizer::~BopRandomLNSOptimizer() {}

BopOptimizerBase::Status BopRandomLNSOptimizer::SynchronizeIfNeeded(
    const ProblemState& problem_state) {
  if (state_update_stamp_ == problem_state.update_stamp()) {
    return BopOptimizerBase::CONTINUE;
  }
  state_update_stamp_ = problem_state.update_stamp();

  problem_ = &problem_state.original_problem();
  initial_solution_.reset(new BopSolution(problem_state.solution()));

  literals_.clear();
  is_in_literals_.assign(initial_solution_->Size(), false);
  for (BopConstraintTerm term : objective_terms_) {
    if ((initial_solution_->Value(term.var_id) && term.weight < 0) ||
        (!initial_solution_->Value(term.var_id) && term.weight > 0)) {
      is_in_literals_[term.var_id.value()] = true;
      literals_.push_back(sat::Literal(sat::VariableIndex(term.var_id.value()),
                                       initial_solution_->Value(term.var_id)));
    }
  }

  adaptive_difficulty_.Reset();
  return BopOptimizerBase::CONTINUE;
}

BopOptimizerBase::Status BopRandomLNSOptimizer::GenerateProblemUsingSat(
    const BopSolution& initial_solution, double target_difficulty,
    TimeLimit* time_limit, BopParameters* parameters, BopSolution* solution,
    std::vector<sat::Literal>* fixed_variables) {
  sat::SatSolver* sat_solver = sat_propagator_->GetSatSolver();
  const DifficultyType type = ComputeVariablesToFixFromSat(
      is_in_literals_, literals_, target_difficulty, sat_solver,
      fixed_variables);
  VLOG(1) << "num_decisions: " << sat_solver->CurrentDecisionLevel();

  switch (type) {
    case DifficultyType::NOTHING_FIXED:
      // Our adaptative parameters strategy resulted in an LNS problem where
      // nothing is fixed. Better to abort the LNS and use the full SAT solver
      // with no conflicts limit.
      VLOG(1) << "Aborting LNS.";
      return BopOptimizerBase::ABORT;
    case DifficultyType::EVERYTHING_FIXED:
      // We can't solve anything. In this case we double the base number of
      // conflicts. This heuristic allows to solve "protfold.mps" to optimality.
      if (adaptive_difficulty_.BoostLuby()) {
        VLOG(1) << "Aborting LNS (boost too high).";
        return BopOptimizerBase::ABORT;
      }
      return BopOptimizerBase::CONTINUE;
    case DifficultyType::UNSAT:
      VLOG(1) << "Aborting LNS (the current problem has been proved UNSAT).";
      return initial_solution.IsFeasible()
                 ? BopOptimizerBase::OPTIMAL_SOLUTION_FOUND
                 : BopOptimizerBase::INFEASIBLE;
    case DifficultyType::OK:
      // Since everything is already set-up, we try the sat_solver with
      // a really low conflict limit. This allow to quickly skip over UNSAT
      // cases without the costly new problem setup.
      int num_conflicts_used = 0;
      parameters->set_max_number_of_conflicts_in_random_lns(
          parameters->max_number_of_conflicts_for_quick_check());
      return SolveLNSProblemWithSatPropagator(
          *fixed_variables, /*setup_already_done=*/true, *parameters,
          sat_solver, solution, time_limit, &num_conflicts_used);
  }

  // Not supposed to be here as the switch is exhaustive.
  return BopOptimizerBase::ABORT;
}

BopOptimizerBase::Status BopRandomLNSOptimizer::GenerateProblem(
    const BopSolution& initial_solution, double target_difficulty,
    TimeLimit* time_limit, std::vector<sat::Literal>* fixed_variables) {
  const int target_parameter = round(target_difficulty * literals_.size());
  if (target_parameter == literals_.size()) {
    VLOG(1) << "Aborting LNS.";
    return BopOptimizerBase::ABORT;
  } else if (target_parameter == 0) {
    if (adaptive_difficulty_.BoostLuby()) {
      VLOG(1) << "Aborting LNS (boost too high).";
      return BopOptimizerBase::ABORT;
    }
    return BopOptimizerBase::CONTINUE;
  }
  to_relax_.ClearAndResize(VariableIndex(initial_solution.Size()));
  for (int i = 0; i < target_parameter; ++i) {
    const VariableIndex var(literals_[i].Variable().value());
    RelaxVariable(var, initial_solution);
  }
  ComputeVariablesToFixFromToRelax(initial_solution, objective_terms_,
                                   to_relax_, fixed_variables);
  return BopOptimizerBase::LIMIT_REACHED;
}

BopOptimizerBase::Status BopRandomLNSOptimizer::Optimize(
    const BopParameters& parameters, const ProblemState& problem_state,
    LearnedInfo* learned_info, TimeLimit* time_limit) {
  SCOPED_TIME_STAT(&stats_);
  CHECK(learned_info != nullptr);
  CHECK(time_limit != nullptr);
  learned_info->Clear();

  const BopOptimizerBase::Status sync_status =
      SynchronizeIfNeeded(problem_state);
  if (sync_status != BopOptimizerBase::CONTINUE) {
    return sync_status;
  }

  // For the SAT conflicts limit of each LNS, we follow a luby sequence times
  // the base number of conflicts (num_conflicts_). Note that the numbers of the
  // Luby sequence are always power of two.
  //
  // We dynamically change the size of the neighbourhood depending on the
  // difficulty of the problem. There is one "target" difficulty for each
  // different numbers in the Luby sequence. Note that the initial value is
  // reused from the last run.
  BopParameters local_parameters = parameters;
  const int default_num_conflicts =
      parameters.max_number_of_conflicts_in_random_lns();
  sat::SatSolver* const sat_solver = sat_propagator_->GetSatSolver();
  int num_tries = 0;
  while (!time_limit->LimitReached() &&
         num_tries < local_parameters.num_random_lns_tries()) {
    ++num_tries;
    std::random_shuffle(literals_.begin(), literals_.end(), random_);

    // Compute the target problem difficulty.
    adaptive_difficulty_.UpdateLuby();
    const double difficulty = adaptive_difficulty_.GetParameterValue();
    std::vector<sat::Literal> fixed_variables;
    const double initial_deterministic_time = sat_solver->deterministic_time();
    BopOptimizerBase::Status status =
        use_sat_to_choose_lns_neighbourhood_
            ? GenerateProblemUsingSat(*initial_solution_, difficulty,
                                      time_limit, &local_parameters,
                                      &learned_info->solution, &fixed_variables)
            : GenerateProblem(*initial_solution_, difficulty, time_limit,
                              &fixed_variables);
    time_limit->AdvanceDeterministicTime(sat_solver->deterministic_time() -
                                         initial_deterministic_time);
    if (status == BopOptimizerBase::CONTINUE) {
      continue;
    } else if (status == BopOptimizerBase::ABORT ||
               status == BopOptimizerBase::INFEASIBLE) {
      return status;
    }

    bool used_long_limit = false;
    int num_conflicts_used = 0;
    if (status == BopOptimizerBase::LIMIT_REACHED) {
      used_long_limit = true;
      const int new_num_conflicts =
          adaptive_difficulty_.luby_value() * default_num_conflicts;
      local_parameters.set_max_number_of_conflicts_in_random_lns(
          new_num_conflicts);
      status = SolveLNSProblem(*problem_, *initial_solution_, local_parameters,
                               fixed_variables, &learned_info->solution,
                               time_limit, &num_conflicts_used);
      if (num_conflicts_used < new_num_conflicts / 2) {
        adaptive_difficulty_.IncreaseParameter();
      } else {
        adaptive_difficulty_.DecreaseParameter();
      }
    }

    VLOG(1) << num_tries << " size (" << literals_.size() << ", "
            << objective_terms_.size() << ", " << initial_solution_->Size()
            << ")"
            << " " << difficulty << " fixed (" << fixed_variables.size() << ") "
            << num_conflicts_used << (used_long_limit ? " *" : "");

    if (status == BopOptimizerBase::SOLUTION_FOUND) {
      return BopOptimizerBase::SOLUTION_FOUND;
    }
  }

  return BopOptimizerBase::LIMIT_REACHED;
}

void BopRandomLNSOptimizer::RelaxVariable(VariableIndex var_id,
                                          const BopSolution& solution) {
  to_relax_.Set(var_id);
  // Relax all variables that might set vars[i] which should be relaxed.
  for (const sat::Literal propagator_literal :
       sat_propagator_->propagated_by(var_id)) {
    const VariableIndex propagator_var_id(
        propagator_literal.Variable().value());
    to_relax_.Set(propagator_var_id);
  }
}

//------------------------------------------------------------------------------
// BopRandomConstraintLNSOptimizer
//------------------------------------------------------------------------------
BopRandomConstraintLNSOptimizer::BopRandomConstraintLNSOptimizer(
    const std::string& name, const BopConstraintTerms& objective_terms,
    int random_seed)
    : BopOptimizerBase(name),
      state_update_stamp_(ProblemState::kInitialStampValue),
      problem_(nullptr),
      initial_solution_(),
      objective_terms_(objective_terms),
      random_(random_seed),
      to_relax_() {}

BopRandomConstraintLNSOptimizer::~BopRandomConstraintLNSOptimizer() {}

BopOptimizerBase::Status BopRandomConstraintLNSOptimizer::SynchronizeIfNeeded(
    const ProblemState& problem_state) {
  if (state_update_stamp_ == problem_state.update_stamp()) {
    return BopOptimizerBase::CONTINUE;
  }
  state_update_stamp_ = problem_state.update_stamp();

  problem_ = &problem_state.original_problem();
  initial_solution_.reset(new BopSolution(problem_state.solution()));
  return BopOptimizerBase::CONTINUE;
}

BopOptimizerBase::Status BopRandomConstraintLNSOptimizer::Optimize(
    const BopParameters& parameters, const ProblemState& problem_state,
    LearnedInfo* learned_info, TimeLimit* time_limit) {
  SCOPED_TIME_STAT(&stats_);
  CHECK(learned_info != nullptr);
  CHECK(time_limit != nullptr);
  learned_info->Clear();

  const BopOptimizerBase::Status sync_status =
      SynchronizeIfNeeded(problem_state);
  if (sync_status != BopOptimizerBase::CONTINUE) {
    return sync_status;
  }

  std::vector<int> ct_ids(problem_->constraints_size(), 0);
  for (int ct_id = 0; ct_id < problem_->constraints_size(); ++ct_id) {
    ct_ids[ct_id] = ct_id;
  }

  int num_tries = 0;
  while (!time_limit->LimitReached() &&
         num_tries < parameters.num_random_lns_tries()) {
    std::random_shuffle(ct_ids.begin(), ct_ids.end(), random_);
    ++num_tries;

    to_relax_.ClearAndResize(VariableIndex(initial_solution_->Size()));
    for (int i = 0; i < ct_ids.size(); ++i) {
      const LinearBooleanConstraint& constraint =
          problem_->constraints(ct_ids[i]);
      for (int j = 0; j < constraint.literals_size(); ++j) {
        const VariableIndex var_id(constraint.literals(j) - 1);
        to_relax_.Set(var_id);
      }

      // TODO(user): Use the auto-adaptative code of the RandomLNS instead of
      //              this hard-coded 10% logic.
      const double kHardCodedTenPercent = 0.1;
      if (to_relax_.PositionsSetAtLeastOnce().size() >
          initial_solution_->Size() * kHardCodedTenPercent) {
        break;
      }
    }

    std::vector<sat::Literal> fixed_variables;
    ComputeVariablesToFixFromToRelax(*initial_solution_, objective_terms_,
                                     to_relax_, &fixed_variables);
    int num_conflicts_used;
    const BopOptimizerBase::Status status = SolveLNSProblem(
        *problem_, *initial_solution_, parameters, fixed_variables,
        &learned_info->solution, time_limit, &num_conflicts_used);

    if (status == BopOptimizerBase::SOLUTION_FOUND) {
      return BopOptimizerBase::SOLUTION_FOUND;
    }
  }
  return BopOptimizerBase::LIMIT_REACHED;
}
}  // namespace bop
}  // namespace operations_research
