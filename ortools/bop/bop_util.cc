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

#include "ortools/bop/bop_util.h"

#include <vector>

#include "ortools/base/basictypes.h"
#include "ortools/base/integral_types.h"
#include "ortools/bop/bop_base.h"
#include "ortools/bop/bop_solution.h"
#include "ortools/sat/boolean_problem.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace bop {
namespace {
static const int kMaxLubyIndex = 30;
static const int kMaxBoost = 30;

// Loads the problem state into the SAT solver. If the problem has already been
// loaded in the sat_solver, fixed variables and objective bounds are updated.
// Returns false when the problem is proved UNSAT.
bool InternalLoadStateProblemToSatSolver(const ProblemState& problem_state,
                                         sat::SatSolver* sat_solver) {
  const bool first_time = (sat_solver->NumVariables() == 0);
  if (first_time) {
    sat_solver->SetNumVariables(
        problem_state.original_problem().num_variables());
  } else {
    // Backtrack the solver to be able to add new constraints.
    sat_solver->Backtrack(0);
  }

  // Set the fixed variables first so that loading the problem will be faster.
  for (VariableIndex var(0); var < problem_state.is_fixed().size(); ++var) {
    if (problem_state.is_fixed()[var]) {
      if (!sat_solver->AddUnitClause(
              sat::Literal(sat::BooleanVariable(var.value()),
                           problem_state.fixed_values()[var]))) {
        return false;
      }
    }
  }

  // Load the problem if not done yet.
  if (first_time &&
      !LoadBooleanProblem(problem_state.original_problem(), sat_solver)) {
    return false;
  }

  // Constrain the objective cost to be greater or equal to the lower bound,
  // and to be smaller than the upper bound. If enforcing the strictier upper
  // bound constraint leads to an UNSAT problem, it means the current solution
  // is proved optimal (if the solution is feasible, else the problem is proved
  // infeasible).
  if (!AddObjectiveConstraint(problem_state.original_problem(),
                              problem_state.lower_bound() != kint64min,
                              sat::Coefficient(problem_state.lower_bound()),
                              problem_state.upper_bound() != kint64max,
                              sat::Coefficient(problem_state.upper_bound() - 1),
                              sat_solver)) {
    return false;
  }

  // Adds the new binary clauses.
  sat_solver->TrackBinaryClauses(true);
  if (!sat_solver->AddBinaryClauses(problem_state.NewlyAddedBinaryClauses())) {
    return false;
  }
  sat_solver->ClearNewlyAddedBinaryClauses();

  return true;
}
}  // anonymous namespace

BopOptimizerBase::Status LoadStateProblemToSatSolver(
    const ProblemState& problem_state, sat::SatSolver* sat_solver) {
  if (InternalLoadStateProblemToSatSolver(problem_state, sat_solver)) {
    return BopOptimizerBase::CONTINUE;
  }

  return problem_state.solution().IsFeasible()
             ? BopOptimizerBase::OPTIMAL_SOLUTION_FOUND
             : BopOptimizerBase::INFEASIBLE;
}

void ExtractLearnedInfoFromSatSolver(sat::SatSolver* solver,
                                     LearnedInfo* info) {
  CHECK(nullptr != solver);
  CHECK(nullptr != info);

  // This should never be called if the problem is UNSAT.
  CHECK(!solver->IsModelUnsat());

  // Fixed variables.
  info->fixed_literals.clear();
  const sat::Trail& propagation_trail = solver->LiteralTrail();
  const int root_size = solver->CurrentDecisionLevel() == 0
                            ? propagation_trail.Index()
                            : solver->Decisions().front().trail_index;
  for (int trail_index = 0; trail_index < root_size; ++trail_index) {
    info->fixed_literals.push_back(propagation_trail[trail_index]);
  }

  // Binary clauses.
  info->binary_clauses = solver->NewlyAddedBinaryClauses();
  solver->ClearNewlyAddedBinaryClauses();
}

void SatAssignmentToBopSolution(const sat::VariablesAssignment& assignment,
                                BopSolution* solution) {
  CHECK(solution != nullptr);

  // Only extract the variables of the initial problem.
  CHECK_LE(solution->Size(), assignment.NumberOfVariables());
  for (sat::BooleanVariable var(0); var < solution->Size(); ++var) {
    CHECK(assignment.VariableIsAssigned(var));
    const bool value = assignment.LiteralIsTrue(sat::Literal(var, true));
    const VariableIndex bop_var_id(var.value());
    solution->SetValue(bop_var_id, value);
  }
}

//------------------------------------------------------------------------------
// AdaptiveParameterValue
//------------------------------------------------------------------------------
AdaptiveParameterValue::AdaptiveParameterValue(double initial_value)
    : value_(initial_value), num_changes_(0) {}

void AdaptiveParameterValue::Reset() { num_changes_ = 0; }

void AdaptiveParameterValue::Increase() {
  ++num_changes_;
  const double factor = 1.0 + 1.0 / (num_changes_ / 2.0 + 1);
  value_ = std::min(1.0 - (1.0 - value_) / factor, value_ * factor);
}

void AdaptiveParameterValue::Decrease() {
  ++num_changes_;
  const double factor = 1.0 + 1.0 / (num_changes_ / 2.0 + 1);
  value_ = std::max(value_ / factor, 1.0 - (1.0 - value_) * factor);
}

//------------------------------------------------------------------------------
// LubyAdaptiveParameterValue
//------------------------------------------------------------------------------
LubyAdaptiveParameterValue::LubyAdaptiveParameterValue(double initial_value)
    : luby_id_(0),
      luby_boost_(0),
      luby_value_(0),
      difficulties_(kMaxLubyIndex, AdaptiveParameterValue(initial_value)) {
  Reset();
}

void LubyAdaptiveParameterValue::Reset() {
  luby_id_ = 0;
  luby_boost_ = 0;
  luby_value_ = 0;
  for (int i = 0; i < difficulties_.size(); ++i) {
    difficulties_[i].Reset();
  }
}

void LubyAdaptiveParameterValue::IncreaseParameter() {
  const int luby_msb = MostSignificantBitPosition64(luby_value_);
  difficulties_[luby_msb].Increase();
}

void LubyAdaptiveParameterValue::DecreaseParameter() {
  const int luby_msb = MostSignificantBitPosition64(luby_value_);
  difficulties_[luby_msb].Decrease();
}

double LubyAdaptiveParameterValue::GetParameterValue() const {
  const int luby_msb = MostSignificantBitPosition64(luby_value_);
  return difficulties_[luby_msb].value();
}

bool LubyAdaptiveParameterValue::BoostLuby() {
  ++luby_boost_;
  return luby_boost_ >= kMaxBoost;
}

void LubyAdaptiveParameterValue::UpdateLuby() {
  ++luby_id_;
  luby_value_ = sat::SUniv(luby_id_) << luby_boost_;
}
}  // namespace bop
}  // namespace operations_research
