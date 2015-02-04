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

#include "bop/bop_ls.h"

#include "bop/bop_util.h"
#include "sat/boolean_problem.h"

namespace operations_research {
namespace bop {

//------------------------------------------------------------------------------
// LocalSearchOptimizer
//------------------------------------------------------------------------------

LocalSearchOptimizer::LocalSearchOptimizer(const std::string& name,
                                           int max_num_decisions)
    : BopOptimizerBase(name),
      state_update_stamp_(ProblemState::kInitialStampValue),
      max_num_decisions_(max_num_decisions),
      assignment_iterator_() {}

LocalSearchOptimizer::~LocalSearchOptimizer() {}

BopOptimizerBase::Status LocalSearchOptimizer::SynchronizeIfNeeded(
    const ProblemState& problem_state) {
  if (state_update_stamp_ == problem_state.update_stamp()) {
    return BopOptimizerBase::CONTINUE;
  }
  state_update_stamp_ = problem_state.update_stamp();

  if (assignment_iterator_ == nullptr) {
    assignment_iterator_.reset(
        new LocalSearchAssignmentIterator(problem_state, max_num_decisions_));
  } else {
    assignment_iterator_->Synchronize(problem_state);
  }

  assignment_iterator_->OverConstrainObjective(
      problem_state.solution().GetCost(), 1);
  return BopOptimizerBase::CONTINUE;
}

BopOptimizerBase::Status LocalSearchOptimizer::Optimize(
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

  double prev_deterministic_time = assignment_iterator_->deterministic_time();
  assignment_iterator_->UseTranspositionTable(
      parameters.use_transposition_table_in_ls());
  int64 num_assignments_to_explore =
      parameters.max_number_of_explored_assignments_per_try_in_ls();

  while (!time_limit->LimitReached() && num_assignments_to_explore > 0 &&
         assignment_iterator_->NextAssignment()) {
    time_limit->AdvanceDeterministicTime(
        assignment_iterator_->deterministic_time() - prev_deterministic_time);
    prev_deterministic_time = assignment_iterator_->deterministic_time();
    --num_assignments_to_explore;
  }
  assignment_iterator_->ExtractLearnedInfo(learned_info);

  if (assignment_iterator_->AssignmentIsFeasible()) {
    CHECK(assignment_iterator_->Assignment().IsFeasible());
    learned_info->solution = assignment_iterator_->Assignment();
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
      reference_solution_(problem, "ReferenceSolution"),
      assignment_(problem, "Assignment"),
      is_assigned_(problem.num_variables(), false),
      constraint_values_(),
      literals_applied_stack_() {
  // Add the objective constraint as the first constraint.
  ConstraintIndex num_constraints(0);
  const LinearObjective& objective = problem.objective();
  CHECK_EQ(objective.literals_size(), objective.coefficients_size());
  for (int i = 0; i < objective.literals_size(); ++i) {
    CHECK_GT(objective.literals(i), 0);
    CHECK_NE(objective.coefficients(i), 0);

    const VariableIndex var(objective.literals(i) - 1);
    const int64 weight = objective.coefficients(i);
    by_variable_matrix_[var].push_back(
        ConstraintEntry(num_constraints, weight));
  }
  constraint_lower_bounds_.push_back(kint64min);
  constraint_values_.push_back(0);
  constraint_upper_bounds_.push_back(kint64max);

  // Add each constraint.
  for (const LinearBooleanConstraint& constraint : problem.constraints()) {
    if (constraint.literals_size() <= 2) {
      // Infeasible binary constraints are automatically repaired by propagation
      // (when possible). Then there are no needs to consider the binary
      // constraints here, the propagation is delegated to the SAT propagator.
      continue;
    }

    ++num_constraints;
    CHECK_EQ(constraint.literals_size(), constraint.coefficients_size());
    for (int i = 0; i < constraint.literals_size(); ++i) {
      const VariableIndex var(constraint.literals(i) - 1);
      const int64 weight = constraint.coefficients(i);
      by_variable_matrix_[var].push_back(
          ConstraintEntry(num_constraints, weight));
    }
    constraint_lower_bounds_.push_back(
        constraint.has_lower_bound() ? constraint.lower_bound() : kint64min);
    constraint_values_.push_back(0);
    constraint_upper_bounds_.push_back(
        constraint.has_upper_bound() ? constraint.upper_bound() : kint64max);
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

  reference_solution_ = reference_solution;
  assignment_ = reference_solution;
  is_assigned_.assign(reference_solution_.Size(), false);
  literals_applied_stack_.clear();
  AddBacktrackingLevel();  // To handle initial propagation.

  // Recompute the value of all constraints.
  constraint_values_.assign(NumConstraints(), 0);
  for (VariableIndex var(0); var < reference_solution_.Size(); ++var) {
    if (reference_solution_.Value(var)) {
      for (const ConstraintEntry& entry : by_variable_matrix_[var]) {
        constraint_values_[entry.constraint] += entry.weight;
      }
    }
  }

  // Update the objective constraint upper bound.
  constraint_upper_bounds_[kObjectiveConstraint] = reference_solution.GetCost();
}

void AssignmentAndConstraintFeasibilityMaintainer::OverConstrainObjective(
    int64 delta) {
  constraint_upper_bounds_[kObjectiveConstraint] -= delta;
  infeasible_constraint_set_.BacktrackAll();
  infeasible_constraint_set_.ChangeState(ConstraintIndex(0), true);
  CHECK(!ConstraintIsFeasible(ConstraintIndex(0)));
  if (DEBUG_MODE) {
    for (ConstraintIndex ct(1); ct < NumConstraints(); ++ct) {
      CHECK(ConstraintIsFeasible(ct));
    }
  }
}

void AssignmentAndConstraintFeasibilityMaintainer::Assign(
    const std::vector<sat::Literal>& literals) {
  // Add all literals at the current backtrack level.
  literals_applied_stack_.back().insert(literals_applied_stack_.back().end(),
                                        literals.begin(), literals.end());

  // Apply each literal.
  for (const sat::Literal& literal : literals) {
    const VariableIndex var(literal.Variable().value());
    const bool value = literal.IsPositive();
    CHECK(!is_assigned_[var]);

    is_assigned_[var] = true;
    if (reference_solution_.Value(var) != value) {
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
  literals_applied_stack_.push_back({});
  infeasible_constraint_set_.AddBacktrackingLevel();
}

void AssignmentAndConstraintFeasibilityMaintainer::BacktrackOneLevel() {
  // Backtrack each literal of the last level.
  for (const sat::Literal& literal : literals_applied_stack_.back()) {
    const VariableIndex var(literal.Variable().value());
    const bool ref_value = reference_solution_.Value(var);
    CHECK(is_assigned_[var]);
    is_assigned_[var] = false;

    if (assignment_.Value(var) != ref_value) {
      assignment_.SetValue(var, ref_value);
      for (const ConstraintEntry& entry : by_variable_matrix_[var]) {
        constraint_values_[entry.constraint] +=
            ref_value ? entry.weight : -entry.weight;
      }
    }
  }

  literals_applied_stack_.pop_back();
  infeasible_constraint_set_.BacktrackOneLevel();
}

std::string AssignmentAndConstraintFeasibilityMaintainer::DebugString() const {
  std::string str;
  str += "ref:  ";
  for (bool value : reference_solution_) {
    str += value ? " 1 " : " 0 ";
  }
  str += "\ncurr: ";
  for (bool value : assignment_) {
    str += value ? " 1 " : " 0 ";
  }
  str += "\n      ";
  for (const bool is_assigned : is_assigned_) {
    str += is_assigned ? " 1 " : " 0 ";
  }
  str += "\nApplied Literals: ";
  for (std::vector<sat::Literal> literals : literals_applied_stack_) {
    str += "\n  ";
    for (sat::Literal literal : literals) {
      str += " " + literal.DebugString();
    }
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

//------------------------------------------------------------------------------
// OneFlipConstraintRepairer
//------------------------------------------------------------------------------

OneFlipConstraintRepairer::OneFlipConstraintRepairer(
    const LinearBooleanProblem& problem,
    const AssignmentAndConstraintFeasibilityMaintainer& maintainer)
    : by_constraint_matrix_(problem.constraints_size() + 1),
      maintainer_(maintainer) {
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

  // Add each constraint.
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

  SortTerms(problem.num_variables());
}

const ConstraintIndex OneFlipConstraintRepairer::kInvalidConstraint(-1);
const TermIndex OneFlipConstraintRepairer::kInitTerm(-1);
const TermIndex OneFlipConstraintRepairer::kInvalidTerm(-2);

ConstraintIndex OneFlipConstraintRepairer::ConstraintToRepair() const {
  ConstraintIndex selected_ct = kInvalidConstraint;
  int32 selected_num_branches = kint32max;
  for (const ConstraintIndex& ct :
       maintainer_.PossiblyInfeasibleConstraints()) {
    const int64 constraint_value = maintainer_.ConstraintValue(ct);
    if (maintainer_.ConstraintIsFeasible(ct)) continue;

    int32 num_branches = 0;
    for (const ConstraintTerm& term : by_constraint_matrix_[ct]) {
      if (maintainer_.IsAssigned(term.var)) {
        continue;
      }
      const bool value = maintainer_.Assignment(term.var);
      const int64 new_sum =
          constraint_value + (value ? -term.weight : term.weight);
      if (new_sum >= maintainer_.ConstraintLowerBound(ct) &&
          new_sum <= maintainer_.ConstraintUpperBound(ct)) {
        ++num_branches;
      }
    }

    if (num_branches == 0) {
      // The constraint can't be repaired in one decision.
      continue;
    }

    if (selected_num_branches > num_branches) {
      selected_ct = ct;
      selected_num_branches = num_branches;
    }
  }

  return selected_ct;
}

TermIndex OneFlipConstraintRepairer::NextRepairingTerm(
    ConstraintIndex constraint, TermIndex init_term_index,
    TermIndex start_term_index) const {
  const ITIVector<TermIndex, ConstraintTerm>& terms =
      by_constraint_matrix_[constraint];
  const int64 constraint_value = maintainer_.ConstraintValue(constraint);

  const TermIndex end_term_index(terms.size() + init_term_index + 1);
  for (TermIndex loop_term_index(
           start_term_index + 1 +
           (start_term_index < init_term_index ? terms.size() : 0));
       loop_term_index < end_term_index; ++loop_term_index) {
    const TermIndex term_index(loop_term_index % terms.size());
    const ConstraintTerm term = terms[term_index];
    if (maintainer_.IsAssigned(term.var)) {
      continue;
    }
    const bool value = maintainer_.Assignment(term.var);
    const int64 new_sum =
        constraint_value + (value ? -term.weight : term.weight);
    if (new_sum >= maintainer_.ConstraintLowerBound(constraint) &&
        new_sum <= maintainer_.ConstraintUpperBound(constraint)) {
      return term_index;
    }
  }
  return kInvalidTerm;
}

sat::Literal OneFlipConstraintRepairer::Literal(ConstraintIndex constraint,
                                                TermIndex term_index) const {
  const ConstraintTerm term = by_constraint_matrix_[constraint][term_index];
  const bool value = maintainer_.Assignment(term.var);
  // Return the literal of the flipped value.
  return sat::Literal(sat::VariableIndex(term.var.value()), !value);
}

namespace {
struct CompareConstraintTermByDecreasingImpactOnObjectiveCost {
  explicit CompareConstraintTermByDecreasingImpactOnObjectiveCost(
      const ITIVector<VariableIndex, int64>& o)
      : objective(o) {}

  bool operator()(const OneFlipConstraintRepairer::ConstraintTerm& a,
                  const OneFlipConstraintRepairer::ConstraintTerm& b) const {
    return objective[a.var] > objective[b.var];
  }
  const ITIVector<VariableIndex, int64>& objective;
};
}  // anonymous namespace

void OneFlipConstraintRepairer::SortTerms(int num_variables) {
  ITIVector<VariableIndex, int64> objective(num_variables, 0);
  const ConstraintIndex kObjectiveConstraint(0);
  for (const ConstraintTerm& term :
       by_constraint_matrix_[kObjectiveConstraint]) {
    objective[term.var] = term.weight;
  }

  CompareConstraintTermByDecreasingImpactOnObjectiveCost compare_object(
      objective);

  for (ITIVector<TermIndex, ConstraintTerm>& terms : by_constraint_matrix_) {
    std::sort(terms.begin(), terms.end(), compare_object);
  }
}

//------------------------------------------------------------------------------
// SatWrapper
//------------------------------------------------------------------------------

SatWrapper::SatWrapper(const ProblemState& problem_state)
    : problem_(problem_state.original_problem()), sat_solver_(), unsat_(false) {
  unsat_ = !LoadStateProblemToSatSolver(problem_state, &sat_solver_);
  if (!unsat_) {
    UseObjectiveForSatAssignmentPreference(problem_state.original_problem(),
                                           &sat_solver_);
  }
}

void SatWrapper::Synchronize(const ProblemState& problem_state) {
  sat_solver_.Backtrack(0);
  unsat_ = !LoadStateProblemToSatSolver(problem_state, &sat_solver_);
}

std::vector<sat::Literal> SatWrapper::OverConstrainObjective(int64 reference_cost,
                                                        int64 delta) {
  // Backtrack to the root node.
  sat_solver_.Backtrack(0);

  unsat_ =
      !AddObjectiveUpperBound(
          problem_, sat::Coefficient(reference_cost - delta), &sat_solver_);

  // Return the list of propagated literals at the root node.
  std::vector<sat::Literal> propagated_literals;
  const sat::Trail& propagation_trail = sat_solver_.LiteralTrail();
  for (int trail_index = 0; trail_index < propagation_trail.Index();
       ++trail_index) {
    propagated_literals.push_back(propagation_trail[trail_index]);
  }

  return propagated_literals;
}

int SatWrapper::ApplyDecision(sat::Literal decision_literal,
                              std::vector<sat::Literal>* propagated_literals) {
  CHECK(!sat_solver_.Assignment().IsVariableAssigned(
      decision_literal.Variable()));
  CHECK(propagated_literals != nullptr);

  propagated_literals->clear();
  const int old_decision_level = sat_solver_.CurrentDecisionLevel();
  const int new_trail_index =
      sat_solver_.EnqueueDecisionAndBackjumpOnConflict(decision_literal);
  unsat_ = unsat_ || new_trail_index == sat::kUnsatTrailIndex;
  if (unsat_) {
    return old_decision_level + 1;
  }

  // Return the propagated literals, whenever there is a conflict or not.
  // In case of conflict, these literals will have to be added to the last
  // decision point after backtrack.
  const sat::Trail& propagation_trail = sat_solver_.LiteralTrail();
  for (int trail_index = new_trail_index;
       trail_index < propagation_trail.Index(); ++trail_index) {
    propagated_literals->push_back(propagation_trail[trail_index]);
  }

  return old_decision_level + 1 - sat_solver_.CurrentDecisionLevel();
}

void SatWrapper::BacktrackOneLevel() {
  const int old_decision_level = sat_solver_.CurrentDecisionLevel();
  if (old_decision_level > 0) {
    sat_solver_.Backtrack(old_decision_level - 1);
  }
}

void SatWrapper::ExtractLearnedInfo(LearnedInfo* info) {
  bop::ExtractLearnedInfoFromSatSolver(&sat_solver_, info);
}

double SatWrapper::deterministic_time() const {
  return sat_solver_.deterministic_time();
}

//------------------------------------------------------------------------------
// LocalSearchAssignmentIterator
//------------------------------------------------------------------------------

LocalSearchAssignmentIterator::LocalSearchAssignmentIterator(
    const ProblemState& problem_state, int max_num_decisions)
    : max_num_decisions_(max_num_decisions),
      maintainer_(problem_state.original_problem()),
      sat_wrapper_(problem_state),
      repairer_(problem_state.original_problem(), maintainer_),
      search_nodes_(),
      initial_term_index_(
          problem_state.original_problem().constraints_size() + 1,
          OneFlipConstraintRepairer::kInitTerm),
      num_nodes_(0),
      num_skipped_nodes_(0) {
  maintainer_.SetReferenceSolution(problem_state.solution());
}

void LocalSearchAssignmentIterator::Synchronize(
    const ProblemState& problem_state) {
  maintainer_.SetReferenceSolution(problem_state.solution());
  sat_wrapper_.Synchronize(problem_state);
  for (const SearchNode& node : search_nodes_) {
    initial_term_index_[node.constraint] = node.term_index;
  }
  search_nodes_.clear();
  transposition_table_.clear();
  num_nodes_ = 0;
  num_skipped_nodes_ = 0;
}

void LocalSearchAssignmentIterator::OverConstrainObjective(int64 reference_cost,
                                                           int64 delta) {
  const std::vector<sat::Literal> propagated_literals =
      sat_wrapper_.OverConstrainObjective(reference_cost, delta);
  maintainer_.OverConstrainObjective(delta);
  maintainer_.Assign(propagated_literals);
}

bool LocalSearchAssignmentIterator::NextAssignment() {
  if (AssignmentIsFeasible() || sat_wrapper_.unsat()) {
    return false;
  }

  // If possible, go deeper, i.e. take one more decision.
  if (!GoDeeper()) {
    // If not, backtrack to the first node that still has untried way to fix
    // its associated constraint. Update it to the next untried way.
    Backtrack();
  }

  // All nodes have been explored.
  if (search_nodes_.empty()) {
    VLOG(1) << std::string(25, ' ') + "LS finished."
            << " #explored:" << num_nodes_
            << " #stored:" << transposition_table_.size()
            << " #skipped:" << num_skipped_nodes_;
    return false;
  }

  // Apply the next decision, i.e. the literal of the flipped variable.
  const SearchNode last_node = search_nodes_.back();
  const sat::Literal literal =
      repairer_.Literal(last_node.constraint, last_node.term_index);
  ApplyDecision(literal);
  return !AssignmentIsFeasible();
}

void LocalSearchAssignmentIterator::ExtractLearnedInfo(LearnedInfo* info) {
  sat_wrapper_.ExtractLearnedInfo(info);
}

// TODO(user): The 1.2 multiplier is an approximation only based on the time
//              spent in the SAT wrapper. So far experiments show a good
//              correlation with real time, but we might want to be more
//              accurate.
double LocalSearchAssignmentIterator::deterministic_time() const {
  return sat_wrapper_.deterministic_time() * 1.2;
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
  std::vector<sat::Literal> propagated_literals;
  const int num_backtracks =
      sat_wrapper_.ApplyDecision(literal, &propagated_literals);

  // Sync the maintainer with SAT.
  if (num_backtracks == 0) {
    maintainer_.AddBacktrackingLevel();
    maintainer_.Assign(propagated_literals);
  } else {
    CHECK_GT(num_backtracks, 0);
    CHECK_LE(num_backtracks, search_nodes_.size());

    // Only backtrack -1 decisions as the last one has not been pushed yet.
    for (int i = 0; i < num_backtracks - 1; ++i) {
      maintainer_.BacktrackOneLevel();
    }
    maintainer_.Assign(propagated_literals);
    search_nodes_.resize(search_nodes_.size() - num_backtracks);
  }
}

void LocalSearchAssignmentIterator::InitializeTranspostionTableKey(
    std::array<int32, kStoredMaxDecisions>* a) {
  int i = 0;
  for (const SearchNode& n : search_nodes_) {
    // Negated because we already fixed it to its opposite value!
    (*a)[i] = -repairer_.Literal(n.constraint, n.term_index).SignedValue();
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
  InitializeTranspostionTableKey(&a);
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
  InitializeTranspostionTableKey(&a);
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
            repairer_.Literal(ct_to_repair, term_index))) {
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
    sat_wrapper_.BacktrackOneLevel();
    if (EnqueueNextRepairingTermIfAny(last_node.constraint,
                                      last_node.term_index)) {
      return;
    }
  }
}

}  // namespace bop
}  // namespace operations_research
