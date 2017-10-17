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

#include "ortools/bop/bop_ls.h"

#include "ortools/base/stringprintf.h"
#include "ortools/bop/bop_util.h"
#include "ortools/sat/boolean_problem.h"

namespace operations_research {
namespace bop {

//------------------------------------------------------------------------------
// LocalSearchOptimizer
//------------------------------------------------------------------------------

LocalSearchOptimizer::LocalSearchOptimizer(const std::string& name,
                                           int max_num_decisions,
                                           sat::SatSolver* sat_propagator)
    : BopOptimizerBase(name),
      state_update_stamp_(ProblemState::kInitialStampValue),
      max_num_decisions_(max_num_decisions),
      sat_wrapper_(sat_propagator),
      assignment_iterator_() {}

LocalSearchOptimizer::~LocalSearchOptimizer() {}

bool LocalSearchOptimizer::ShouldBeRun(
    const ProblemState& problem_state) const {
  return problem_state.solution().IsFeasible();
}

BopOptimizerBase::Status LocalSearchOptimizer::Optimize(
    const BopParameters& parameters, const ProblemState& problem_state,
    LearnedInfo* learned_info, TimeLimit* time_limit) {
  CHECK(learned_info != nullptr);
  CHECK(time_limit != nullptr);
  learned_info->Clear();

  if (assignment_iterator_ == nullptr) {
    assignment_iterator_.reset(new LocalSearchAssignmentIterator(
        problem_state, max_num_decisions_,
        parameters.max_num_broken_constraints_in_ls(), &sat_wrapper_));
  }

  if (state_update_stamp_ != problem_state.update_stamp()) {
    // We have a new problem_state.
    state_update_stamp_ = problem_state.update_stamp();
    assignment_iterator_->Synchronize(problem_state);
  }
  assignment_iterator_->SynchronizeSatWrapper();

  double prev_deterministic_time = assignment_iterator_->deterministic_time();
  assignment_iterator_->UseTranspositionTable(
      parameters.use_transposition_table_in_ls());
  assignment_iterator_->UsePotentialOneFlipRepairs(
      parameters.use_potential_one_flip_repairs_in_ls());
  int64 num_assignments_to_explore =
      parameters.max_number_of_explored_assignments_per_try_in_ls();

  while (!time_limit->LimitReached() && num_assignments_to_explore > 0 &&
         assignment_iterator_->NextAssignment()) {
    time_limit->AdvanceDeterministicTime(
        assignment_iterator_->deterministic_time() - prev_deterministic_time);
    prev_deterministic_time = assignment_iterator_->deterministic_time();
    --num_assignments_to_explore;
  }
  if (sat_wrapper_.IsModelUnsat()) {
    // TODO(user): we do that all the time, return an UNSAT satus instead and
    // do this only once.
    return problem_state.solution().IsFeasible()
               ? BopOptimizerBase::OPTIMAL_SOLUTION_FOUND
               : BopOptimizerBase::INFEASIBLE;
  }

  // TODO(user): properly abort when we found a new solution and then finished
  // the ls? note that this is minor.
  sat_wrapper_.ExtractLearnedInfo(learned_info);
  if (assignment_iterator_->BetterSolutionHasBeenFound()) {
    // TODO(user): simply use std::vector<bool> instead of a BopSolution internally.
    learned_info->solution = assignment_iterator_->LastReferenceAssignment();
    return BopOptimizerBase::SOLUTION_FOUND;
  }

  if (time_limit->LimitReached()) {
    // The time limit is reached without finding a solution.
    return BopOptimizerBase::LIMIT_REACHED;
  }

  if (num_assignments_to_explore <= 0) {
    // Explore the remaining assignments in a future call to Optimize().
    return BopOptimizerBase::CONTINUE;
  }

  // All assignments reachable in max_num_decisions_ or less have been explored,
  // don't call optimize() with the same initial solution again.
  return BopOptimizerBase::ABORT;
}

//------------------------------------------------------------------------------
// BacktrackableIntegerSet
//------------------------------------------------------------------------------

template <typename IntType>
void BacktrackableIntegerSet<IntType>::ClearAndResize(IntType n) {
  size_ = 0;
  saved_sizes_.clear();
  saved_stack_sizes_.clear();
  stack_.clear();
  in_stack_.assign(n.value(), false);
}

template <typename IntType>
void BacktrackableIntegerSet<IntType>::ChangeState(IntType i,
                                                   bool should_be_inside) {
  size_ += should_be_inside ? 1 : -1;
  if (!in_stack_[i.value()]) {
    in_stack_[i.value()] = true;
    stack_.push_back(i);
  }
}

template <typename IntType>
void BacktrackableIntegerSet<IntType>::AddBacktrackingLevel() {
  saved_stack_sizes_.push_back(stack_.size());
  saved_sizes_.push_back(size_);
}

template <typename IntType>
void BacktrackableIntegerSet<IntType>::BacktrackOneLevel() {
  if (saved_stack_sizes_.empty()) {
    BacktrackAll();
  } else {
    for (int i = saved_stack_sizes_.back(); i < stack_.size(); ++i) {
      in_stack_[stack_[i].value()] = false;
    }
    stack_.resize(saved_stack_sizes_.back());
    saved_stack_sizes_.pop_back();
    size_ = saved_sizes_.back();
    saved_sizes_.pop_back();
  }
}

template <typename IntType>
void BacktrackableIntegerSet<IntType>::BacktrackAll() {
  for (int i = 0; i < stack_.size(); ++i) {
    in_stack_[stack_[i].value()] = false;
  }
  stack_.clear();
  saved_stack_sizes_.clear();
  size_ = 0;
  saved_sizes_.clear();
}

// Explicit instantiation of BacktrackableIntegerSet.
// TODO(user): move the code in a separate .h and -inl.h to avoid this.
template class BacktrackableIntegerSet<ConstraintIndex>;

//------------------------------------------------------------------------------
// AssignmentAndConstraintFeasibilityMaintainer
//------------------------------------------------------------------------------

AssignmentAndConstraintFeasibilityMaintainer::
    AssignmentAndConstraintFeasibilityMaintainer(
        const LinearBooleanProblem& problem)
    : by_variable_matrix_(problem.num_variables()),
      constraint_lower_bounds_(),
      constraint_upper_bounds_(),
      assignment_(problem, "Assignment"),
      reference_(problem, "Assignment"),
      constraint_values_(),
      flipped_var_trail_backtrack_levels_(),
      flipped_var_trail_() {
  // Add the objective constraint as the first constraint.
  const LinearObjective& objective = problem.objective();
  CHECK_EQ(objective.literals_size(), objective.coefficients_size());
  for (int i = 0; i < objective.literals_size(); ++i) {
    CHECK_GT(objective.literals(i), 0);
    CHECK_NE(objective.coefficients(i), 0);

    const VariableIndex var(objective.literals(i) - 1);
    const int64 weight = objective.coefficients(i);
    by_variable_matrix_[var].push_back(
        ConstraintEntry(kObjectiveConstraint, weight));
  }
  constraint_lower_bounds_.push_back(kint64min);
  constraint_values_.push_back(0);
  constraint_upper_bounds_.push_back(kint64max);

  // Add each constraint.
  ConstraintIndex num_constraints_with_objective(1);
  for (const LinearBooleanConstraint& constraint : problem.constraints()) {
    if (constraint.literals_size() <= 2) {
      // Infeasible binary constraints are automatically repaired by propagation
      // (when possible). Then there are no needs to consider the binary
      // constraints here, the propagation is delegated to the SAT propagator.
      continue;
    }

    CHECK_EQ(constraint.literals_size(), constraint.coefficients_size());
    for (int i = 0; i < constraint.literals_size(); ++i) {
      const VariableIndex var(constraint.literals(i) - 1);
      const int64 weight = constraint.coefficients(i);
      by_variable_matrix_[var].push_back(
          ConstraintEntry(num_constraints_with_objective, weight));
    }
    constraint_lower_bounds_.push_back(
        constraint.has_lower_bound() ? constraint.lower_bound() : kint64min);
    constraint_values_.push_back(0);
    constraint_upper_bounds_.push_back(
        constraint.has_upper_bound() ? constraint.upper_bound() : kint64max);

    ++num_constraints_with_objective;
  }

  // Initialize infeasible_constraint_set_;
  infeasible_constraint_set_.ClearAndResize(
      ConstraintIndex(constraint_values_.size()));

  CHECK_EQ(constraint_values_.size(), constraint_lower_bounds_.size());
  CHECK_EQ(constraint_values_.size(), constraint_upper_bounds_.size());
}

const ConstraintIndex
    AssignmentAndConstraintFeasibilityMaintainer::kObjectiveConstraint(0);

void AssignmentAndConstraintFeasibilityMaintainer::SetReferenceSolution(
    const BopSolution& reference_solution) {
  CHECK(reference_solution.IsFeasible());
  infeasible_constraint_set_.BacktrackAll();

  assignment_ = reference_solution;
  reference_ = assignment_;
  flipped_var_trail_backtrack_levels_.clear();
  flipped_var_trail_.clear();
  AddBacktrackingLevel();  // To handle initial propagation.

  // Recompute the value of all constraints.
  constraint_values_.assign(NumConstraints(), 0);
  for (VariableIndex var(0); var < assignment_.Size(); ++var) {
    if (assignment_.Value(var)) {
      for (const ConstraintEntry& entry : by_variable_matrix_[var]) {
        constraint_values_[entry.constraint] += entry.weight;
      }
    }
  }

  MakeObjectiveConstraintInfeasible(1);
}

void AssignmentAndConstraintFeasibilityMaintainer::
    UseCurrentStateAsReference() {
  for (const VariableIndex var : flipped_var_trail_) {
    reference_.SetValue(var, assignment_.Value(var));
  }
  flipped_var_trail_.clear();
  flipped_var_trail_backtrack_levels_.clear();
  AddBacktrackingLevel();  // To handle initial propagation.
  MakeObjectiveConstraintInfeasible(1);
}

void AssignmentAndConstraintFeasibilityMaintainer::
    MakeObjectiveConstraintInfeasible(int delta) {
  CHECK(IsFeasible());
  CHECK(flipped_var_trail_.empty());
  constraint_upper_bounds_[kObjectiveConstraint] =
      constraint_values_[kObjectiveConstraint] - delta;
  infeasible_constraint_set_.BacktrackAll();
  infeasible_constraint_set_.ChangeState(kObjectiveConstraint, true);
  infeasible_constraint_set_.AddBacktrackingLevel();
  CHECK(!ConstraintIsFeasible(kObjectiveConstraint));
  CHECK(!IsFeasible());
  if (DEBUG_MODE) {
    for (ConstraintIndex ct(1); ct < NumConstraints(); ++ct) {
      CHECK(ConstraintIsFeasible(ct));
    }
  }
}

void AssignmentAndConstraintFeasibilityMaintainer::Assign(
    const std::vector<sat::Literal>& literals) {
  for (const sat::Literal& literal : literals) {
    const VariableIndex var(literal.Variable().value());
    const bool value = literal.IsPositive();
    if (assignment_.Value(var) != value) {
      flipped_var_trail_.push_back(var);
      assignment_.SetValue(var, value);
      for (const ConstraintEntry& entry : by_variable_matrix_[var]) {
        const bool was_feasible = ConstraintIsFeasible(entry.constraint);
        constraint_values_[entry.constraint] +=
            value ? entry.weight : -entry.weight;
        if (ConstraintIsFeasible(entry.constraint) != was_feasible) {
          infeasible_constraint_set_.ChangeState(entry.constraint,
                                                 was_feasible);
        }
      }
    }
  }
}

void AssignmentAndConstraintFeasibilityMaintainer::AddBacktrackingLevel() {
  flipped_var_trail_backtrack_levels_.push_back(flipped_var_trail_.size());
  infeasible_constraint_set_.AddBacktrackingLevel();
}

void AssignmentAndConstraintFeasibilityMaintainer::BacktrackOneLevel() {
  // Backtrack each literal of the last level.
  for (int i = flipped_var_trail_backtrack_levels_.back();
       i < flipped_var_trail_.size(); ++i) {
    const VariableIndex var(flipped_var_trail_[i]);
    const bool new_value = !assignment_.Value(var);
    DCHECK_EQ(new_value, reference_.Value(var));
    assignment_.SetValue(var, new_value);
    for (const ConstraintEntry& entry : by_variable_matrix_[var]) {
      constraint_values_[entry.constraint] +=
          new_value ? entry.weight : -entry.weight;
    }
  }
  flipped_var_trail_.resize(flipped_var_trail_backtrack_levels_.back());
  flipped_var_trail_backtrack_levels_.pop_back();
  infeasible_constraint_set_.BacktrackOneLevel();
}

void AssignmentAndConstraintFeasibilityMaintainer::BacktrackAll() {
  while (!flipped_var_trail_backtrack_levels_.empty()) BacktrackOneLevel();
}

const std::vector<sat::Literal>&
AssignmentAndConstraintFeasibilityMaintainer::PotentialOneFlipRepairs() {
  if (!constraint_set_hasher_.IsInitialized()) {
    InitializeConstraintSetHasher();
  }

  // First, we compute the hash that a Literal should have in order to repair
  // all the infeasible constraint (ignoring the objective).
  //
  // TODO(user): If this starts to show-up in a performance profile, we can
  // easily maintain this hash incrementally.
  uint64 hash = 0;
  for (const ConstraintIndex ci : PossiblyInfeasibleConstraints()) {
    const int64 value = ConstraintValue(ci);
    if (value > ConstraintUpperBound(ci)) {
      hash ^= constraint_set_hasher_.Hash(FromConstraintIndex(ci, false));
    } else if (value < ConstraintLowerBound(ci)) {
      hash ^= constraint_set_hasher_.Hash(FromConstraintIndex(ci, true));
    }
  }

  tmp_potential_repairs_.clear();
  const auto it = hash_to_potential_repairs_.find(hash);
  if (it != hash_to_potential_repairs_.end()) {
    for (const sat::Literal literal : it->second) {
      // We only returns the flips.
      if (assignment_.Value(VariableIndex(literal.Variable().value())) !=
          literal.IsPositive()) {
        tmp_potential_repairs_.push_back(literal);
      }
    }
  }
  return tmp_potential_repairs_;
}

std::string AssignmentAndConstraintFeasibilityMaintainer::DebugString() const {
  std::string str;
  str += "curr: ";
  for (bool value : assignment_) {
    str += value ? " 1 " : " 0 ";
  }
  str += "\nFlipped variables: ";
  // TODO(user): show the backtrack levels.
  for (const VariableIndex var : flipped_var_trail_) {
    str += StringPrintf(" %d", var.value());
  }
  str += "\nmin  curr  max\n";
  for (ConstraintIndex ct(0); ct < constraint_values_.size(); ++ct) {
    if (constraint_lower_bounds_[ct] == kint64min) {
      str += StringPrintf("-  %lld  %lld\n", constraint_values_[ct],
                          constraint_upper_bounds_[ct]);
    } else {
      str += StringPrintf("%lld  %lld  %lld\n", constraint_lower_bounds_[ct],
                          constraint_values_[ct], constraint_upper_bounds_[ct]);
    }
  }
  return str;
}

void AssignmentAndConstraintFeasibilityMaintainer::
    InitializeConstraintSetHasher() {
  const int num_constraints_with_objective = constraint_upper_bounds_.size();

  // Initialize the potential one flip repair. Note that we ignore the
  // objective constraint completely so that we consider a repair even if the
  // objective constraint is not infeasible.
  constraint_set_hasher_.Initialize(2 * num_constraints_with_objective);
  constraint_set_hasher_.IgnoreElement(
      FromConstraintIndex(kObjectiveConstraint, true));
  constraint_set_hasher_.IgnoreElement(
      FromConstraintIndex(kObjectiveConstraint, false));
  for (VariableIndex var(0); var < by_variable_matrix_.size(); ++var) {
    // We add two entries, one for a positive flip (from false to true) and one
    // for a negative flip (from true to false).
    for (const bool flip_is_positive : {true, false}) {
      uint64 hash = 0;
      for (const ConstraintEntry& entry : by_variable_matrix_[var]) {
        const bool coeff_is_positive = entry.weight > 0;
        hash ^= constraint_set_hasher_.Hash(FromConstraintIndex(
            entry.constraint,
            /*up=*/flip_is_positive ? coeff_is_positive : !coeff_is_positive));
      }
      hash_to_potential_repairs_[hash].push_back(
          sat::Literal(sat::BooleanVariable(var.value()), flip_is_positive));
    }
  }
}

//------------------------------------------------------------------------------
// OneFlipConstraintRepairer
//------------------------------------------------------------------------------

OneFlipConstraintRepairer::OneFlipConstraintRepairer(
    const LinearBooleanProblem& problem,
    const AssignmentAndConstraintFeasibilityMaintainer& maintainer,
    const sat::VariablesAssignment& sat_assignment)
    : by_constraint_matrix_(problem.constraints_size() + 1),
      maintainer_(maintainer),
      sat_assignment_(sat_assignment) {
  // Fill the by_constraint_matrix_.
  //
  // IMPORTANT: The order of the constraint needs to exactly match the one of
  // the constraint in the AssignmentAndConstraintFeasibilityMaintainer.

  // Add the objective constraint as the first constraint.
  ConstraintIndex num_constraint(0);
  const LinearObjective& objective = problem.objective();
  CHECK_EQ(objective.literals_size(), objective.coefficients_size());
  for (int i = 0; i < objective.literals_size(); ++i) {
    CHECK_GT(objective.literals(i), 0);
    CHECK_NE(objective.coefficients(i), 0);

    const VariableIndex var(objective.literals(i) - 1);
    const int64 weight = objective.coefficients(i);
    by_constraint_matrix_[num_constraint].push_back(
        ConstraintTerm(var, weight));
  }

  // Add the non-binary problem constraints.
  for (const LinearBooleanConstraint& constraint : problem.constraints()) {
    if (constraint.literals_size() <= 2) {
      // Infeasible binary constraints are automatically repaired by propagation
      // (when possible). Then there are no needs to consider the binary
      // constraints here, the propagation is delegated to the SAT propagator.
      continue;
    }

    ++num_constraint;
    CHECK_EQ(constraint.literals_size(), constraint.coefficients_size());
    for (int i = 0; i < constraint.literals_size(); ++i) {
      const VariableIndex var(constraint.literals(i) - 1);
      const int64 weight = constraint.coefficients(i);
      by_constraint_matrix_[num_constraint].push_back(
          ConstraintTerm(var, weight));
    }
  }

  SortTermsOfEachConstraints(problem.num_variables());
}

const ConstraintIndex OneFlipConstraintRepairer::kInvalidConstraint(-1);
const TermIndex OneFlipConstraintRepairer::kInitTerm(-1);
const TermIndex OneFlipConstraintRepairer::kInvalidTerm(-2);

ConstraintIndex OneFlipConstraintRepairer::ConstraintToRepair() const {
  ConstraintIndex selected_ct = kInvalidConstraint;
  int32 selected_num_branches = kint32max;
  int num_infeasible_constraints_left = maintainer_.NumInfeasibleConstraints();

  // Optimization: We inspect the constraints in reverse order because the
  // objective one will always be first (in our current code) and with some
  // luck, we will break early instead of fully exploring it.
  const std::vector<ConstraintIndex>& infeasible_constraints =
      maintainer_.PossiblyInfeasibleConstraints();
  for (int index = infeasible_constraints.size() - 1; index >= 0; --index) {
    const ConstraintIndex& i = infeasible_constraints[index];
    if (maintainer_.ConstraintIsFeasible(i)) continue;
    --num_infeasible_constraints_left;

    // Optimization: We return the only candidate without inspecting it.
    // This is critical at the beginning of the search or later if the only
    // candidate is the objective constraint which can be really long.
    if (num_infeasible_constraints_left == 0 &&
        selected_ct == kInvalidConstraint) {
      return i;
    }

    const int64 constraint_value = maintainer_.ConstraintValue(i);
    const int64 lb = maintainer_.ConstraintLowerBound(i);
    const int64 ub = maintainer_.ConstraintUpperBound(i);

    int32 num_branches = 0;
    for (const ConstraintTerm& term : by_constraint_matrix_[i]) {
      if (sat_assignment_.VariableIsAssigned(
              sat::BooleanVariable(term.var.value()))) {
        continue;
      }
      const int64 new_value =
          constraint_value +
          (maintainer_.Assignment(term.var) ? -term.weight : term.weight);
      if (new_value >= lb && new_value <= ub) {
        ++num_branches;
        if (num_branches >= selected_num_branches) break;
      }
    }

    // The constraint can't be repaired in one decision.
    if (num_branches == 0) continue;
    if (num_branches < selected_num_branches) {
      selected_ct = i;
      selected_num_branches = num_branches;
      if (num_branches == 1) break;
    }
  }
  return selected_ct;
}

TermIndex OneFlipConstraintRepairer::NextRepairingTerm(
    ConstraintIndex ct_index, TermIndex init_term_index,
    TermIndex start_term_index) const {
  const ITIVector<TermIndex, ConstraintTerm>& terms =
      by_constraint_matrix_[ct_index];
  const int64 constraint_value = maintainer_.ConstraintValue(ct_index);
  const int64 lb = maintainer_.ConstraintLowerBound(ct_index);
  const int64 ub = maintainer_.ConstraintUpperBound(ct_index);

  const TermIndex end_term_index(terms.size() + init_term_index + 1);
  for (TermIndex loop_term_index(
           start_term_index + 1 +
           (start_term_index < init_term_index ? terms.size() : 0));
       loop_term_index < end_term_index; ++loop_term_index) {
    const TermIndex term_index(loop_term_index % terms.size());
    const ConstraintTerm term = terms[term_index];
    if (sat_assignment_.VariableIsAssigned(
            sat::BooleanVariable(term.var.value()))) {
      continue;
    }
    const int64 new_value =
        constraint_value +
        (maintainer_.Assignment(term.var) ? -term.weight : term.weight);
    if (new_value >= lb && new_value <= ub) {
      return term_index;
    }
  }
  return kInvalidTerm;
}

bool OneFlipConstraintRepairer::RepairIsValid(ConstraintIndex ct_index,
                                              TermIndex term_index) const {
  if (maintainer_.ConstraintIsFeasible(ct_index)) return false;
  const ConstraintTerm term = by_constraint_matrix_[ct_index][term_index];
  if (sat_assignment_.VariableIsAssigned(
          sat::BooleanVariable(term.var.value()))) {
    return false;
  }
  const int64 new_value =
      maintainer_.ConstraintValue(ct_index) +
      (maintainer_.Assignment(term.var) ? -term.weight : term.weight);

  const int64 lb = maintainer_.ConstraintLowerBound(ct_index);
  const int64 ub = maintainer_.ConstraintUpperBound(ct_index);
  return (new_value >= lb && new_value <= ub);
}

sat::Literal OneFlipConstraintRepairer::GetFlip(ConstraintIndex ct_index,
                                                TermIndex term_index) const {
  const ConstraintTerm term = by_constraint_matrix_[ct_index][term_index];
  const bool value = maintainer_.Assignment(term.var);
  return sat::Literal(sat::BooleanVariable(term.var.value()), !value);
}

void OneFlipConstraintRepairer::SortTermsOfEachConstraints(int num_variables) {
  ITIVector<VariableIndex, int64> objective(num_variables, 0);
  for (const ConstraintTerm& term :
       by_constraint_matrix_[AssignmentAndConstraintFeasibilityMaintainer::
                                 kObjectiveConstraint]) {
    objective[term.var] = std::abs(term.weight);
  }
  for (ITIVector<TermIndex, ConstraintTerm>& terms : by_constraint_matrix_) {
    std::sort(terms.begin(), terms.end(),
              [&objective](const ConstraintTerm& a, const ConstraintTerm& b) {
                return objective[a.var] > objective[b.var];
              });
  }
}

//------------------------------------------------------------------------------
// SatWrapper
//------------------------------------------------------------------------------

SatWrapper::SatWrapper(sat::SatSolver* sat_solver) : sat_solver_(sat_solver) {}

void SatWrapper::BacktrackAll() { sat_solver_->Backtrack(0); }

std::vector<sat::Literal> SatWrapper::FullSatTrail() const {
  std::vector<sat::Literal> propagated_literals;
  const sat::Trail& trail = sat_solver_->LiteralTrail();
  for (int trail_index = 0; trail_index < trail.Index(); ++trail_index) {
    propagated_literals.push_back(trail[trail_index]);
  }
  return propagated_literals;
}

int SatWrapper::ApplyDecision(sat::Literal decision_literal,
                              std::vector<sat::Literal>* propagated_literals) {
  CHECK(!sat_solver_->Assignment().VariableIsAssigned(
      decision_literal.Variable()));
  CHECK(propagated_literals != nullptr);

  propagated_literals->clear();
  const int old_decision_level = sat_solver_->CurrentDecisionLevel();
  const int new_trail_index =
      sat_solver_->EnqueueDecisionAndBackjumpOnConflict(decision_literal);
  if (sat_solver_->IsModelUnsat()) {
    return old_decision_level + 1;
  }

  // Return the propagated literals, whenever there is a conflict or not.
  // In case of conflict, these literals will have to be added to the last
  // decision point after backtrack.
  const sat::Trail& propagation_trail = sat_solver_->LiteralTrail();
  for (int trail_index = new_trail_index;
       trail_index < propagation_trail.Index(); ++trail_index) {
    propagated_literals->push_back(propagation_trail[trail_index]);
  }

  return old_decision_level + 1 - sat_solver_->CurrentDecisionLevel();
}

void SatWrapper::BacktrackOneLevel() {
  const int old_decision_level = sat_solver_->CurrentDecisionLevel();
  if (old_decision_level > 0) {
    sat_solver_->Backtrack(old_decision_level - 1);
  }
}

void SatWrapper::ExtractLearnedInfo(LearnedInfo* info) {
  bop::ExtractLearnedInfoFromSatSolver(sat_solver_, info);
}

double SatWrapper::deterministic_time() const {
  return sat_solver_->deterministic_time();
}

//------------------------------------------------------------------------------
// LocalSearchAssignmentIterator
//------------------------------------------------------------------------------

LocalSearchAssignmentIterator::LocalSearchAssignmentIterator(
    const ProblemState& problem_state, int max_num_decisions,
    int max_num_broken_constraints, SatWrapper* sat_wrapper)
    : max_num_decisions_(max_num_decisions),
      max_num_broken_constraints_(max_num_broken_constraints),
      maintainer_(problem_state.original_problem()),
      sat_wrapper_(sat_wrapper),
      repairer_(problem_state.original_problem(), maintainer_,
                sat_wrapper->SatAssignment()),
      search_nodes_(),
      initial_term_index_(
          problem_state.original_problem().constraints_size() + 1,
          OneFlipConstraintRepairer::kInitTerm),
      use_transposition_table_(false),
      use_potential_one_flip_repairs_(false),
      num_nodes_(0),
      num_skipped_nodes_(0),
      num_improvements_(0),
      num_improvements_by_one_flip_repairs_(0),
      num_inspected_one_flip_repairs_(0) {}

LocalSearchAssignmentIterator::~LocalSearchAssignmentIterator() {
  VLOG(1) << "LS " << max_num_decisions_
          << "\n  num improvements: " << num_improvements_
          << "\n  num improvements with one flip repairs: "
          << num_improvements_by_one_flip_repairs_
          << "\n  num inspected one flip repairs: "
          << num_inspected_one_flip_repairs_;
}

void LocalSearchAssignmentIterator::Synchronize(
    const ProblemState& problem_state) {
  better_solution_has_been_found_ = false;
  maintainer_.SetReferenceSolution(problem_state.solution());
  for (const SearchNode& node : search_nodes_) {
    initial_term_index_[node.constraint] = node.term_index;
  }
  search_nodes_.clear();
  transposition_table_.clear();
  num_nodes_ = 0;
  num_skipped_nodes_ = 0;
}

// In order to restore the synchronization from any state, we backtrack
// everything and retry to take the same decisions as before. We stop at the
// first one that can't be taken.
void LocalSearchAssignmentIterator::SynchronizeSatWrapper() {
  CHECK_EQ(better_solution_has_been_found_, false);
  const std::vector<SearchNode> copy = search_nodes_;
  sat_wrapper_->BacktrackAll();
  maintainer_.BacktrackAll();

  // Note(user): at this stage, the sat trail contains the fixed variables.
  // There will almost always be at the same value in the reference solution.
  // However since the objective may be over-constrained in the sat_solver, it
  // is possible that some variable where propagated to some other values.
  maintainer_.Assign(sat_wrapper_->FullSatTrail());

  search_nodes_.clear();
  for (const SearchNode& node : copy) {
    if (!repairer_.RepairIsValid(node.constraint, node.term_index)) break;
    search_nodes_.push_back(node);
    ApplyDecision(repairer_.GetFlip(node.constraint, node.term_index));
  }
}

void LocalSearchAssignmentIterator::UseCurrentStateAsReference() {
  better_solution_has_been_found_ = true;
  maintainer_.UseCurrentStateAsReference();
  sat_wrapper_->BacktrackAll();

  // Note(user): Here, there should be no discrepancies between the fixed
  // variable and the new reference, so there is no need to do:
  // maintainer_.Assign(sat_wrapper_->FullSatTrail());

  for (const SearchNode& node : search_nodes_) {
    initial_term_index_[node.constraint] = node.term_index;
  }
  search_nodes_.clear();
  transposition_table_.clear();
  num_nodes_ = 0;
  num_skipped_nodes_ = 0;
  ++num_improvements_;
}

bool LocalSearchAssignmentIterator::NextAssignment() {
  if (sat_wrapper_->IsModelUnsat()) return false;
  if (maintainer_.IsFeasible()) {
    UseCurrentStateAsReference();
    return true;
  }

  // We only look for potential one flip repairs if we reached the end of the
  // LS tree. I tried to do that at every level, but it didn't change the
  // result much on the set-partitionning example I was using.
  //
  // TODO(user): Perform more experiments with this.
  if (use_potential_one_flip_repairs_ &&
      search_nodes_.size() == max_num_decisions_) {
    for (const sat::Literal literal : maintainer_.PotentialOneFlipRepairs()) {
      if (sat_wrapper_->SatAssignment().VariableIsAssigned(
              literal.Variable())) {
        continue;
      }
      ++num_inspected_one_flip_repairs_;

      // Temporarily apply the potential repair and see if it worked!
      ApplyDecision(literal);
      if (maintainer_.IsFeasible()) {
        num_improvements_by_one_flip_repairs_++;
        UseCurrentStateAsReference();
        return true;
      }
      maintainer_.BacktrackOneLevel();
      sat_wrapper_->BacktrackOneLevel();
    }
  }

  // If possible, go deeper, i.e. take one more decision.
  if (!GoDeeper()) {
    // If not, backtrack to the first node that still has untried way to fix
    // its associated constraint. Update it to the next untried way.
    Backtrack();
  }

  // All nodes have been explored.
  if (search_nodes_.empty()) {
    VLOG(1) << std::string(27, ' ') + "LS " << max_num_decisions_ << " finished."
            << " #explored:" << num_nodes_
            << " #stored:" << transposition_table_.size()
            << " #skipped:" << num_skipped_nodes_;
    return false;
  }

  // Apply the next decision, i.e. the literal of the flipped variable.
  const SearchNode node = search_nodes_.back();
  ApplyDecision(repairer_.GetFlip(node.constraint, node.term_index));
  return true;
}

// TODO(user): The 1.2 multiplier is an approximation only based on the time
//              spent in the SAT wrapper. So far experiments show a good
//              correlation with real time, but we might want to be more
//              accurate.
double LocalSearchAssignmentIterator::deterministic_time() const {
  return sat_wrapper_->deterministic_time() * 1.2;
}

std::string LocalSearchAssignmentIterator::DebugString() const {
  std::string str = "Search nodes:\n";
  for (int i = 0; i < search_nodes_.size(); ++i) {
    str +=
        StringPrintf("  %d: %d  %d\n", i, search_nodes_[i].constraint.value(),
                     search_nodes_[i].term_index.value());
  }
  return str;
}

void LocalSearchAssignmentIterator::ApplyDecision(sat::Literal literal) {
  ++num_nodes_;
  const int num_backtracks =
      sat_wrapper_->ApplyDecision(literal, &tmp_propagated_literals_);

  // Sync the maintainer with SAT.
  if (num_backtracks == 0) {
    maintainer_.AddBacktrackingLevel();
    maintainer_.Assign(tmp_propagated_literals_);
  } else {
    CHECK_GT(num_backtracks, 0);
    CHECK_LE(num_backtracks, search_nodes_.size());

    // Only backtrack -1 decisions as the last one has not been pushed yet.
    for (int i = 0; i < num_backtracks - 1; ++i) {
      maintainer_.BacktrackOneLevel();
    }
    maintainer_.Assign(tmp_propagated_literals_);
    search_nodes_.resize(search_nodes_.size() - num_backtracks);
  }
}

void LocalSearchAssignmentIterator::InitializeTranspositionTableKey(
    std::array<int32, kStoredMaxDecisions>* a) {
  int i = 0;
  for (const SearchNode& n : search_nodes_) {
    // Negated because we already fliped this variable, so GetFlip() will
    // returns the old value.
    (*a)[i] = -repairer_.GetFlip(n.constraint, n.term_index).SignedValue();
    ++i;
  }

  // 'a' is not zero-initialized, so we need to complete it with zeros.
  while (i < kStoredMaxDecisions) {
    (*a)[i] = 0;
    ++i;
  }
}

bool LocalSearchAssignmentIterator::NewStateIsInTranspositionTable(
    sat::Literal l) {
  if (search_nodes_.size() + 1 > kStoredMaxDecisions) return false;

  // Fill the transposition table element, i.e the array 'a' of decisions.
  std::array<int32, kStoredMaxDecisions> a;
  InitializeTranspositionTableKey(&a);
  a[search_nodes_.size()] = l.SignedValue();
  std::sort(a.begin(), a.begin() + 1 + search_nodes_.size());

  if (transposition_table_.find(a) == transposition_table_.end()) {
    return false;
  } else {
    ++num_skipped_nodes_;
    return true;
  }
}

void LocalSearchAssignmentIterator::InsertInTranspositionTable() {
  // If there is more decision that kStoredMaxDecisions, do nothing.
  if (search_nodes_.size() > kStoredMaxDecisions) return;

  // Fill the transposition table element, i.e the array 'a' of decisions.
  std::array<int32, kStoredMaxDecisions> a;
  InitializeTranspositionTableKey(&a);
  std::sort(a.begin(), a.begin() + search_nodes_.size());

  transposition_table_.insert(a);
}

bool LocalSearchAssignmentIterator::EnqueueNextRepairingTermIfAny(
    ConstraintIndex ct_to_repair, TermIndex term_index) {
  if (term_index == initial_term_index_[ct_to_repair]) return false;
  if (term_index == OneFlipConstraintRepairer::kInvalidTerm) {
    term_index = initial_term_index_[ct_to_repair];
  }
  while (true) {
    term_index = repairer_.NextRepairingTerm(
        ct_to_repair, initial_term_index_[ct_to_repair], term_index);
    if (term_index == OneFlipConstraintRepairer::kInvalidTerm) return false;
    if (!use_transposition_table_ ||
        !NewStateIsInTranspositionTable(
            repairer_.GetFlip(ct_to_repair, term_index))) {
      search_nodes_.push_back(SearchNode(ct_to_repair, term_index));
      return true;
    }
    if (term_index == initial_term_index_[ct_to_repair]) return false;
  }
}

bool LocalSearchAssignmentIterator::GoDeeper() {
  // Can we add one more decision?
  if (search_nodes_.size() >= max_num_decisions_) {
    return false;
  }

  // Is the number of infeasible constraints reasonable?
  //
  // TODO(user): Make this parameters dynamic. We can either try lower value
  // first and increase it later, or try to dynamically change it during the
  // search. Another idea is to have instead a "max number of constraints that
  // can be repaired in one decision" and to take into account the number of
  // decisions left.
  if (maintainer_.NumInfeasibleConstraints() > max_num_broken_constraints_) {
    return false;
  }

  // Can we find a constraint that can be repaired in one decision?
  const ConstraintIndex ct_to_repair = repairer_.ConstraintToRepair();
  if (ct_to_repair == OneFlipConstraintRepairer::kInvalidConstraint) {
    return false;
  }

  // Add the new decision.
  //
  // TODO(user): Store the last explored term index to not start from -1 each
  // time. This will be very useful when a backtrack occured due to the SAT
  // propagator. Note however that this behavior is already enforced when we use
  // the transposition table, since we will not explore again the branches
  // already explored.
  return EnqueueNextRepairingTermIfAny(ct_to_repair,
                                       OneFlipConstraintRepairer::kInvalidTerm);
}

void LocalSearchAssignmentIterator::Backtrack() {
  while (!search_nodes_.empty()) {
    // We finished exploring this node. Store it in the transposition table so
    // that the same decisions will not be explored again. Note that the SAT
    // solver may have learned more the second time the exact same decisions are
    // seen, but we assume that it is not worth exploring again.
    if (use_transposition_table_) InsertInTranspositionTable();

    const SearchNode last_node = search_nodes_.back();
    search_nodes_.pop_back();
    maintainer_.BacktrackOneLevel();
    sat_wrapper_->BacktrackOneLevel();
    if (EnqueueNextRepairingTermIfAny(last_node.constraint,
                                      last_node.term_index)) {
      return;
    }
  }
}

}  // namespace bop
}  // namespace operations_research
