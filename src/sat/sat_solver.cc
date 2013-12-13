// Copyright 2010-2013 Google
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
#include "sat/sat_solver.h"

#include <algorithm>
#include "base/unique_ptr.h"
#include <string>
#include <vector>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/join.h"
#include "base/stl_util.h"

namespace operations_research {
namespace sat {

namespace {

// Returns true if the given watcher list contains the given clause.
template<typename Watcher>
bool WatcherListContains(const std::vector<Watcher>& list,
                         const SatClause& candidate) {
  for (const Watcher& watcher : list) {
    if (watcher.clause == &candidate) return true;
  }
  return false;
}

// A simple wrapper to simplify the erase(std::remove_if()) pattern.
template<typename Container, typename Predicate>
void RemoveIf(Container c, Predicate p) {
  c->erase(std::remove_if(c->begin(), c->end(), p), c->end());
}

// Removes dettached clauses from a watcher list.
template<typename Watcher>
bool CleanUpPredicate(const Watcher& watcher) {
  return !watcher.clause->IsAttached();
}

// Compares literals by variable first, then sign.
bool CompareLiteral(Literal l1, Literal l2) {
  return l1.Index() < l2.Index();
}
}  // namespace

// ----- LiteralWatchers -----

LiteralWatchers::LiteralWatchers()
    : is_clean_(true),
      num_inspected_clauses_(0),
      stats_("LiteralWatchers")
{}

LiteralWatchers::~LiteralWatchers() {
  IF_STATS_ENABLED(LOG(INFO) << stats_.StatString());
}

void LiteralWatchers::Init() {
  is_clean_ = true;
  num_inspected_clauses_ = 0;
}

void LiteralWatchers::Resize(int num_variables) {
  DCHECK(is_clean_);
  watchers_on_false_.resize(num_variables << 1);
  needs_cleaning_.assign(num_variables << 1, false);
}

// Note that this is the only place where we add Watcher so the DCHECK
// guarantees that there are no duplicates.
void LiteralWatchers::AttachOnFalse(Literal a, Literal b, SatClause* clause) {
  DCHECK(is_clean_);
  DCHECK(!WatcherListContains(watchers_on_false_[a.Index()], *clause));
  watchers_on_false_[a.Index()].push_back(Watcher(clause, b));
}

bool LiteralWatchers::PropagateOnTrue(Literal true_literal, Trail* trail) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(is_clean_);
  const Literal false_literal = true_literal.Negated();
  std::vector<Watcher>* watchers = &watchers_on_false_[false_literal.Index()];
  const int initial_size = watchers->size();
  for (int i = watchers->size() - 1; i >= 0; i--) {
    // Don't even look at the clause memory if the blocking literal is true.
    if (trail->Assignment().IsLiteralTrue((*watchers)[i].blocking_literal)) {
      continue;
    }

    SatClause* clause = (*watchers)[i].clause;
    if (!clause->PropagateOnFalse(false_literal, trail)) {
      // All literals of this clause are false, it can't be satisfied.
      num_inspected_clauses_ += initial_size - i;
      return false;
    }

    // Update the watched literal if clause->GetFirstLiteral() changed.
    // See the contract of PropagateOnFalse().
    if (clause->GetFirstLiteral() != false_literal) {
      // Detach the clause from this dependency watchers.
      (*watchers)[i] = watchers->back();
      watchers->pop_back();

      // Attach it to its new dependency.
      AttachOnFalse(clause->GetFirstLiteral(),
                    clause->GetSecondLiteral(),
                    clause);
    } else {
      (*watchers)[i].blocking_literal = clause->GetSecondLiteral();
    }
  }
  num_inspected_clauses_ += initial_size;
  return true;
}

void LiteralWatchers::LazyDetach(SatClause* clause) {
  clause->LazyDetach();
  is_clean_ = false;
  needs_cleaning_[clause->GetFirstLiteral().Index()] = true;
  needs_cleaning_[clause->GetSecondLiteral().Index()] = true;
}

void LiteralWatchers::CleanUpWatchers() {
  SCOPED_TIME_STAT(&stats_);
  for (int i = 0; i < needs_cleaning_.size(); ++i) {
    if (needs_cleaning_[LiteralIndex(i)]) {
      RemoveIf(&(watchers_on_false_[LiteralIndex(i)]),
               CleanUpPredicate<Watcher>);
      needs_cleaning_[LiteralIndex(i)] = false;
    }
  }
  is_clean_ = true;
}

// ----- BinaryImplicationGraph -----

void BinaryImplicationGraph::Resize(int num_variables) {
  SCOPED_TIME_STAT(&stats_);
  implications_.resize(num_variables << 1);
  num_propagations_ = 0;
}

void BinaryImplicationGraph::AddBinaryClause(Literal a, Literal b) {
  SCOPED_TIME_STAT(&stats_);
  implications_[a.Negated().Index()].push_back(b);
  implications_[b.Negated().Index()].push_back(a);
}

void BinaryImplicationGraph::AddBinaryConflict(
    Literal a, Literal b, Trail* trail) {
  SCOPED_TIME_STAT(&stats_);
  AddBinaryClause(a, b);
  if (trail->Assignment().IsLiteralFalse(a)) {
    trail->EnqueueWithBinaryReason(b, a);
  } else if (trail->Assignment().IsLiteralFalse(b)) {
    trail->EnqueueWithBinaryReason(a, b);
  }
}

bool BinaryImplicationGraph::PropagateOnTrue(
    Literal true_literal, Trail* trail) {
  SCOPED_TIME_STAT(&stats_);
  const VariablesAssignment& assignment = trail->Assignment();
  for (Literal literal : implications_[true_literal.Index()]) {
    if (assignment.IsLiteralTrue(literal)) {
      // Note(user): I tried to update the reason here if the literal was
      // enqueued after the true_literal on the trail. This property is
      // important for ComputeFirstUIPConflict() to work since it needs the
      // trail order to be a topological order for the deduction graph.
      // But the performance where not too good...
      continue;
    }

    ++num_propagations_;
    if (assignment.IsLiteralFalse(literal)) {
      // Conflict.
      temporary_clause_[0] = true_literal.Negated();
      temporary_clause_[1] = literal;
      trail->SetFailingClause(ClauseRef(&temporary_clause_[0],
                                        &temporary_clause_[0] + 2));
      return false;
    } else {
      // Propagation.
      trail->EnqueueWithBinaryReason(literal, true_literal.Negated());
    }
  }
  return true;
}

void BinaryImplicationGraph::MinimizeClause(
    const Trail& trail, std::vector<Literal>* conflict) {
  SCOPED_TIME_STAT(&stats_);
  is_marked_.ClearAndResize(LiteralIndex(implications_.size()));
  is_removed_.ClearAndResize(LiteralIndex(implications_.size()));
  for (Literal lit : *conflict) {
    is_marked_.Set(lit.Index());
  }

  // Identify and remove the redundant literals from the given conflict.
  // 1/ If a -> b then a can be removed from the conflict clause.
  //    This is because not b -> not a.
  // 2/ a -> b can only happen if level(a) <= level(b).
  // 3/ Because of 2/, cycles can appear only at the same level.
  //    The vector is_removed_ is used to avoid removing all elements of a
  //    cycle. Note that this is not optimal in the sense that we may not remove
  //    a literal that can be removed.
  //
  // TODO(user): no need to explore the unique literal of the current decision
  // level since it can't be removed.
  int index = 0;
  for (int i = 0; i < conflict->size(); ++i) {
    const Literal lit = (*conflict)[i];
    const int lit_level = trail.Info(lit.Variable()).level;
    bool keep_literal = true;
    for (Literal implied : implications_[lit.Index()]) {
      if (is_marked_[implied.Index()]) {
        DCHECK_LE(lit_level, trail.Info(implied.Variable()).level);
        if (lit_level == trail.Info(implied.Variable()).level
            && is_removed_[implied.Index()]) continue;
        keep_literal = false;
        break;
      }
    }
    if (keep_literal) {
      (*conflict)[index] = lit;
      ++index;
    } else {
      is_removed_.Set(lit.Index());
    }
  }
  if (index < conflict->size()) {
    ++num_minimization_;
    num_literals_removed_ += conflict->size() - index;
    conflict->erase(conflict->begin() + index, conflict->end());
  }
}

void BinaryImplicationGraph::RemoveFixedVariables(
    const VariablesAssignment& assigment) {
  SCOPED_TIME_STAT(&stats_);
  is_marked_.ClearAndResize(LiteralIndex(implications_.size()));
  for (LiteralIndex i(0); i < implications_.size(); ++i) {
    if (assigment.IsLiteralTrue(Literal(i))) {
      // If b is true and a -> b then because not b -> not a, all the
      // implications list that contains b will be marked by this process.
      for (Literal lit : implications_[Literal(i).NegatedIndex()]) {
        is_marked_.Set(lit.NegatedIndex());
      }
      STLClearObject(&(implications_[i]));
      STLClearObject(&(implications_[Literal(i).NegatedIndex()]));
    }
  }
  for (LiteralIndex i(0); i < implications_.size(); ++i) {
    if (is_marked_[i]) {
      RemoveIf(&implications_[i],
               std::bind1st(std::mem_fun(&VariablesAssignment::IsLiteralTrue),
                            &assigment));
    }
  }
}

// ----- SatClause -----

// static
SatClause* SatClause::Create(const std::vector<Literal>& literals, ClauseType type) {
  CHECK_GE(literals.size(), 2);
  SatClause* clause = reinterpret_cast<SatClause*>(
      ::operator new (sizeof(SatClause) + literals.size() * sizeof(Literal)));
  clause->size_ = literals.size();
  for (int i = 0; i < literals.size(); ++i) {
    clause->literals_[i] = literals[i];
  }
  clause->is_learned_ = (type == LEARNED_CLAUSE);
  clause->is_attached_ = false;
  clause->activity_ = 0.0;
  clause->lbd_ = 0;
  return clause;
}

// This currently only checks for trivially true clause.
SatClause::SimplifyStatus SatClause::Simplify() {
  std::vector<Literal> copy(begin(), end());
  std::sort(copy.begin(), copy.end(), CompareLiteral);
  for (int i = 0; i < copy.size() - 1; ++i) {
    if (copy[i] == copy[i + 1].Negated()) return CLAUSE_ALWAYS_TRUE;
  }
  return CLAUSE_ACTIVE;
}

namespace {

// Support struct to sort literals for ordering.
struct WeightedLiteral {
  WeightedLiteral(Literal l, int w) : literal(l), weight(w) {}

  Literal literal;
  int weight;
};

// Lexical order, by smaller weight, then by smaller literal to break ties.
bool LiteralWithSmallerWeightFirst(const WeightedLiteral& wv1,
                                   const WeightedLiteral& wv2) {
  return (wv1.weight < wv2.weight) ||
      (wv1.weight == wv2.weight &&
          wv1.literal.SignedValue() < wv2.literal.SignedValue());
}

// Lexical order, by larger weight, then by smaller literal to break ties.
bool LiteralWithLargerWeightFirst(const WeightedLiteral& wv1,
                                  const WeightedLiteral& wv2) {
  return (wv1.weight > wv2.weight) ||
      (wv1.weight == wv2.weight &&
          wv1.literal.SignedValue() < wv2.literal.SignedValue());
}

}  // namespace

void SatClause::SortLiterals(
    const ITIVector<VariableIndex, VariableInfo>& statistics,
    const SatParameters& parameters) {
  CHECK(!IsAttached());
  const SatParameters::LiteralOrdering literal_order =
      parameters.literal_ordering();
  if (literal_order != SatParameters::LITERAL_IN_ORDER) {
    std::vector<WeightedLiteral> order;
    for (Literal literal : *this) {
      order.push_back(WeightedLiteral(
          literal, statistics[literal.Variable()].num_appearances));
    }
    switch (literal_order) {
      case SatParameters::VAR_MIN_USAGE: {
        std::sort(order.begin(), order.end(), LiteralWithSmallerWeightFirst);
        break;
      }
      case SatParameters::VAR_MAX_USAGE: {
        std::sort(order.begin(), order.end(), LiteralWithLargerWeightFirst);
        break;
      }
      default: {
        break;
      }
    }
    for (int i = 0; i < order.size(); ++i) {
      literals_[i] = order[i].literal;
    }
  }
}

bool SatClause::AttachAndEnqueuePotentialUnitPropagation(
    Trail* trail, LiteralWatchers* demons) {
  CHECK(!IsAttached());
  // Select the first two literals that are not assigned to false and put them
  // on position 0 and 1.
  int num_literal_not_false = 0;
  for (int i = 0; i < size_; ++i) {
    if (!trail->Assignment().IsLiteralFalse(literals_[i])) {
      std::swap(literals_[i], literals_[num_literal_not_false]);
      ++num_literal_not_false;
      if (num_literal_not_false == 2) {
        break;
      }
    }
  }

  // Returns false if all the literals were false.
  // This should only happen on an UNSAT problem, and there is no need to attach
  // the clause in this case.
  if (num_literal_not_false == 0) return false;

  if (num_literal_not_false == 1) {
    // To maintain the validity of the 2-watcher algorithm, we need to watch
    // the false literal with the highest decision levels.
    int max_level = trail->Info(literals_[1].Variable()).level;
    for (int i = 2; i < size_; ++i) {
      const int level = trail->Info(literals_[i].Variable()).level;
      if (level > max_level) {
        max_level = level;
        std::swap(literals_[1], literals_[i]);
      }
    }

    // If there is a propagation, make literals_[1] the propagated literal and
    // enqueue it.
    if (!trail->Assignment().IsLiteralTrue(literals_[0])) {
      std::swap(literals_[0], literals_[1]);
      trail->EnqueueWithSatClauseReason(literals_[1], this);
    }
  }

  // Attach the watchers.
  is_attached_ = true;
  demons->AttachOnFalse(literals_[0], literals_[1], this);
  demons->AttachOnFalse(literals_[1], literals_[0], this);
  return true;
}

// Propagates one watched literal becoming false. This method maintains the
// invariant that watched literals are always in position 0 and 1.
bool SatClause::PropagateOnFalse(Literal watched_literal, Trail* trail) {
  DCHECK(IsAttached());
  DCHECK_GE(size_, 2);
  DCHECK(trail->Assignment().IsLiteralFalse(watched_literal));

  // The instantiated literal should be in position 0.
  if (literals_[1] == watched_literal) {
    literals_[1] = literals_[0];
    literals_[0] = watched_literal;
  }
  DCHECK_EQ(literals_[0], watched_literal);

  // If the other watched literal is true, do nothing.
  if (trail->Assignment().IsLiteralTrue(literals_[1])) return true;

  for (int i = 2; i < size_; ++i) {
    if (trail->Assignment().IsLiteralFalse(literals_[i])) continue;

    // Note(user): If the value of literals_[i] is true, it is possible to leave
    // the watched literal unchanged. However this seems less efficient. Even if
    // we swap it with the literal at position 2 to speed up future checks.

    // literal[i] is undefined or true, it's now the new literal to watch.
    literals_[0] = literals_[i];
    literals_[i] = watched_literal;
    return true;
  }

  // Literals_[1] is either false or undefined, all other literals are false.
  if (trail->Assignment().IsLiteralFalse(literals_[1])) {
    trail->SetFailingSatClause(ToClauseRef(), this);
    return false;
  }

  // Literals_[1] is undefined, set it to true.
  trail->EnqueueWithSatClauseReason(literals_[1], this);
  return true;
}

bool SatClause::IsSatisfied(const VariablesAssignment& assignment) const {
  for (const Literal literal : *this) {
    if (assignment.IsLiteralTrue(literal)) return true;
  }
  return false;
}

string SatClause::DebugString() const {
  string result;
  for (const Literal literal : *this) {
    if (!result.empty()) result.append(" ");
    result.append(literal.DebugString());
  }
  return result;
}

// ----- SatSolver -----

SatSolver::SatSolver()
    : num_variables_(0),
      pb_constraints_(&trail_),
      propagation_trail_index_(0),
      binary_propagation_trail_index_(0),
      is_model_unsat_(false),
      variable_activity_increment_(1.0),
      clause_activity_increment_(1.0),
      learned_clause_limit_(0),
      conflicts_until_next_restart_(0),
      restart_count_(0),
      reason_cache_(trail_),
      stats_("SatSolver") {}

SatSolver::~SatSolver() {
  IF_STATS_ENABLED(LOG(INFO) << stats_.StatString());
  STLDeleteElements(&problem_clauses_);
  STLDeleteElements(&learned_clauses_);
}

void SatSolver::SetNumVariables(int num_variables) {
  SCOPED_TIME_STAT(&stats_);
  num_variables_ = num_variables;
  statistics_.resize(num_variables);
  watched_clauses_.Resize(num_variables);
  binary_implication_graph_.Resize(num_variables);
  trail_.Resize(num_variables);
  pb_constraints_.Resize(num_variables);
  queue_elements_.resize(num_variables);
  variable_activity_increment_ = 1.0;
  clause_activity_increment_ = 1.0;
  activities_.resize(num_variables, 0.0);
  objective_weights_.resize(num_variables << 1, 0.0);
}

int64 SatSolver::num_branches() const {
  return counters_.num_branches;
}

int64 SatSolver::num_failures() const {
  return counters_.num_failures;
}

int64 SatSolver::num_propagations() const {
  return trail_.NumberOfEnqueues() - counters_.num_branches;
}

const SatParameters& SatSolver::parameters() const {
  SCOPED_TIME_STAT(&stats_);
  return parameters_;
}

void SatSolver::SetParameters(const SatParameters& parameters) {
  SCOPED_TIME_STAT(&stats_);
  parameters_ = parameters;
}

string SatSolver::Indent() const {
  SCOPED_TIME_STAT(&stats_);
  const int level = choice_points_.size();
  string result;
  for (int i = 0; i < level; ++i) {
    result.append("|   ");
  }
  return result;
}

bool SatSolver::ModelUnsat() {
  is_model_unsat_ = true;
  return false;
}

bool SatSolver::AddProblemClause(const std::vector<Literal>& literals) {
  SCOPED_TIME_STAT(&stats_);
  CHECK(choice_points_.empty());

  // Deals with clause of size 0 (always false) and 1 (set a literal) right away
  // so we guarantee that a SatClause is always of size greater than one. This
  // simplifies the code.
  if (literals.size() == 0) return ModelUnsat();
  if (literals.size() == 1) return TestValidityAndEnqueueIfNeeded(literals[0]);

  std::unique_ptr<SatClause> clause(
      SatClause::Create(literals, SatClause::PROBLEM_CLAUSE));
  switch (clause->Simplify()) {
    case SatClause::CLAUSE_ALWAYS_FALSE: return ModelUnsat();
    case SatClause::CLAUSE_ALWAYS_TRUE: FALLTHROUGH_INTENDED;
    case SatClause::CLAUSE_SUBSUMED: return true;
    case SatClause::CLAUSE_ACTIVE: {
      if (parameters_.treat_binary_clauses_separately()
          && clause->Size() == 2) {
        binary_implication_graph_.AddBinaryClause(
            clause->GetFirstLiteral(), clause->GetSecondLiteral());
      } else {
        clause->SortLiterals(statistics_, parameters_);
        if (!clause->AttachAndEnqueuePotentialUnitPropagation(
            &trail_, &watched_clauses_)) {
          return ModelUnsat();
        }
        problem_clauses_.push_back(clause.release());
      }
      return true;
    }
  }
}

bool SatSolver::AddLinearConstraintInternal(const std::vector<LiteralWithCoeff>& cst,
                                            Coefficient rhs,
                                            Coefficient max_value) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(LinearConstraintIsCannonical(cst));
  if (rhs < 0) return ModelUnsat();  // Unsatisfiable constraint.
  if (rhs >= max_value) return true;  // Always satisfied constraint.

  // A linear upper bounded constraint is a clause if the only problematic
  // assignment is the one where all the literals are true. Since they are
  // ordered by coefficient, this is easy to check.
  if (max_value - cst[0].coefficient <= rhs) {
    // This constraint is actually a clause. It is faster to treat it as one.
    literals_scratchpad_.clear();
    for (const LiteralWithCoeff& term : cst) {
      literals_scratchpad_.push_back(term.literal.Negated());
    }
    return AddProblemClause(literals_scratchpad_);
  }

  // TODO(user): If this constraint forces all its literal to false (when rhs is
  // zero for instance), we still add it. Optimize this?
  return pb_constraints_.AddConstraint(cst, rhs);
}

bool SatSolver::AddLinearConstraint(
    bool use_lower_bound, Coefficient lower_bound,
    bool use_upper_bound, Coefficient upper_bound,
    std::vector<LiteralWithCoeff>* cst) {
  SCOPED_TIME_STAT(&stats_);
  CHECK(choice_points_.empty());
  Coefficient bound_shift;
  Coefficient max_value;
  CHECK(PbCannonicalForm(cst, &bound_shift, &max_value));
  if (use_upper_bound) {
    if (!AddLinearConstraintInternal(*cst,
                                     upper_bound + bound_shift,
                                     max_value)) {
      return ModelUnsat();
    }
  }
  if (use_lower_bound) {
    for (int i = 0; i < cst->size(); ++i) {
      (*cst)[i].literal = (*cst)[i].literal.Negated();
    }
    if (!AddLinearConstraintInternal(*cst,
                                     max_value - (lower_bound + bound_shift),
                                     max_value)) {
      return ModelUnsat();
    }
  }
  return true;
}

void SatSolver::AddLearnedClauseAndEnqueueUnitPropagation(
    const std::vector<Literal>& literals) {
  SCOPED_TIME_STAT(&stats_);
  if (literals.size() == 1) {
    // A length 1 clause fix a literal for all the search.
    // ComputeBacktrackLevel() should have returned 0.
    CHECK(choice_points_.empty());
    trail_.Enqueue(literals[0], AssignmentInfo::UNIT_REASON);
  } else {
    if (parameters_.treat_binary_clauses_separately() && literals.size() == 2) {
      binary_implication_graph_.AddBinaryConflict(
          literals[0], literals[1], &trail_);
      UpdateStatisticsOnOneClause(literals, literals.size(), /*added=*/ true);
    } else {
      SatClause* clause = SatClause::Create(
          literals, SatClause::LEARNED_CLAUSE);
      UpdateStatisticsOnOneClause(literals, literals.size(), /*added=*/ true);
      CompressLearnedClausesIfNeeded();
      learned_clauses_.emplace_back(clause);
      BumpClauseActivity(clause);

      // Important: Even though the only literal at the last decision level has
      // been unassigned, its level was not modified, so ComputeLbd() works.
      clause->SetLbd(parameters_.use_lbd() ? ComputeLbd(*clause) : 0);
      clause->SortLiterals(statistics_, parameters_);
      CHECK(clause->AttachAndEnqueuePotentialUnitPropagation(
          &trail_, &watched_clauses_));
    }
  }
}

// TODO(user): Clear the solver state and all the constraints.
void SatSolver::Reset(int num_variables) {
  SCOPED_TIME_STAT(&stats_);
  SetNumVariables(num_variables);
  is_model_unsat_ = false;
  leave_initial_activities_unchanged_ = false;
  counters_ = Counters();
  watched_clauses_.Init();
  restart_count_ = 0;
  InitRestart();
  clause_cleanup_count_ = 0;
  InitLearnedClauseLimit();
  random_.Reset(parameters_.random_seed());
}

bool SatSolver::InitialPropagation() {
  SCOPED_TIME_STAT(&stats_);
  CHECK_EQ(choice_points_.size(), 0);
  if (!Propagate()) {
    return ModelUnsat();
  }
  return true;
}

bool SatSolver::EnqueueDecision(Literal true_literal) {
  SCOPED_TIME_STAT(&stats_);
  // We are back at level 0. This can happen because of a restart, or because
  // we proved that some variables must take a given value in any satisfiable
  // assignment. Trigger a simplification of the clauses if there is new fixed
  // variables.
  //
  // TODO(user): Do not trigger it all the time if it takes too much time.
  // TODO(user): Do more advanced preprocessing?
  if (choice_points_.empty()) {
    if (num_processed_fixed_variables_ < trail_.Index()) {
      ProcessNewlyFixedVariables();
    }
  }

  NewChoicePoint(true_literal);
  while (!Propagate()) {
    // If Propagate() fail at level 0, then the problem is UNSAT.
    if (choice_points_.size() == 0) return ModelUnsat();
    reason_cache_.Clear();

    // A conflict occured, compute a nice reason for this failure.
    std::vector<Literal> reason;
    std::vector<Literal> discarded_last_level_literals;
    ComputeFirstUIPConflict(trail_.FailingClause(),
                            &reason, &discarded_last_level_literals);
    DCHECK(IsConflictValid(reason));

    // Update the activity of all the variables in the first UIP clause.
    // Also update the activity of the last level variables expanded (and
    // thus discarded) during the first UIP computation. Note that both
    // sets are disjoint.
    const int initial_lbd = ComputeLbd(reason);
    const int lbd_limit = parameters_.use_lbd() &&
        parameters_.use_glucose_bump_again_strategy() ? initial_lbd : 0;
    BumpVariableActivities(reason, lbd_limit);
    BumpVariableActivities(discarded_last_level_literals, lbd_limit);

    // Bump the clause activities.
    // Note that the activity of the learned clause will be bumped too
    // by AddLearnedClauseAndEnqueueUnitPropagation().
    if (trail_.FailingSatClause() != nullptr) {
      BumpClauseActivity(trail_.FailingSatClause());
    }
    BumpReasonActivities(discarded_last_level_literals);

    // Minimize the reason.
    MinimizeConflict(&reason);
    DCHECK(IsConflictValid(reason));
    DCHECK_EQ(initial_lbd, ComputeLbd(reason));
    if (parameters_.treat_binary_clauses_separately() &&
        parameters_.use_binary_clauses_minimization()) {
      // Note that on the contrary to the MinimizeConflict() above that
      // just uses the reason graph, this minimization can change the
      // clause LBD and even the backtracking level.
      binary_implication_graph_.MinimizeClause(trail_, &reason);
      DCHECK(IsConflictValid(reason));
    }

    // Backtrack and add the reason to the set of learned clause.
    counters_.num_literals_learned += reason.size();
    Backtrack(ComputeBacktrackLevel(reason));
    AddLearnedClauseAndEnqueueUnitPropagation(reason);

    // Decay the activities.
    UpdateVariableActivityIncrement();
    UpdateClauseActivityIncrement();

    // Decrement the restart counter if needed.
    if (conflicts_until_next_restart_ > 0) {
      --conflicts_until_next_restart_;
    }
  }
  return true;
}

void SatSolver::Backtrack(int target_level) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(!choice_points_.empty());
  ++counters_.num_failures;
  int target_trail_index = 0;
  while (choice_points_.size() > target_level) {
    VLOG(2) << Indent() << "Compress " << choice_points_.back().decision;
    target_trail_index = choice_points_.back().literal_trail_index;
    choice_points_.pop_back();
  }
  Untrail(target_trail_index);
  trail_.SetDecisionLevel(target_level);
}

SatSolver::Status SatSolver::Solve() {
  SCOPED_TIME_STAT(&stats_);
  if (is_model_unsat_) return MODEL_UNSAT;
  if (!choice_points_.empty()) {
    LOG(ERROR) << "Wrong state when calling SatSolver::Solve()";
    return INTERNAL_ERROR;
  }
  timer_.Restart();

  // Display initial statistics.
  LOG(INFO) << "Initial memory usage: " << MemoryUsage();
  LOG(INFO) << "Number of clauses (size > 2): "
            << problem_clauses_.size();
  LOG(INFO) << "Number of binary clauses: "
            << binary_implication_graph_.NumberOfImplications();
  LOG(INFO) << "Number of linear constraints: "
            << pb_constraints_.NumberOfConstraints();
  LOG(INFO) << "Number of initial fixed variables: " << trail_.Index();

  // Compute statistics.
  ComputeStatistics();

  // Fill in the LiteralWatchers structure and perform the initial propagation
  // if there was any clauses of length 1.
  if (!InitialPropagation()) {
    LOG(INFO) << "UNSAT (initial propagation) \n" << StatusString();
    return MODEL_UNSAT;
  }
  LOG(INFO) << "Number of assigned variables after initial propagation: "
            << trail_.Index();

  // This is called before Preprocess() so the later as more chance to detect
  // variables appearing in only one direction.
  ProcessNewlyFixedVariables();

  // TODO(user): The preprocessing is currenlty broken of the problem contains
  // linear constraints because the literal statistics are wrong. It is also
  // broken if one wants to look for a "good" first solution of an optimization
  // problem.
  if (false) {
    if (!Preprocess() || !Propagate()) {
      LOG(INFO) << "UNSAT (after preprocessing) \n" << StatusString();
      return MODEL_UNSAT;
    }
    LOG(INFO) << "Number of assigned variables after preprocessing: "
              << trail_.Index();
  }

  ComputeInitialVariableOrdering();
  num_inactive_variables_ = trail_.Index();

  // Starts search.
  LOG(INFO) << "Start Search: " << parameters_.ShortDebugString();
  for (;;) {
    if (trail_.Index() == num_variables_.value()) {  // At a leaf.
      LOG(INFO) << "SAT \n" << StatusString();
      if (!IsAssignmentValid(trail_.Assignment())) {
        LOG(INFO) << "Something is wrong, the computed model is not true";
        return INTERNAL_ERROR;
      }
      return MODEL_SAT;
    }

    // Note that ShouldRestart() comes first because it had side effects and
    // should be executed even if choice_points_ is empty.
    if (ShouldRestart() && !choice_points_.empty()) {
      Backtrack(0);
    }

    // Choose the next decision variable and propagate it.
    MakeAllTrailVariablesInactive();
    Literal next_branch = NextBranch();

    // Note that if use_optimization_hints() is false, then objective_weights_
    // will be all zero, so this has no effect.
    if (objective_weights_[next_branch.Index()] != 0.0) {
      next_branch = next_branch.Negated();
    }
    if (!EnqueueDecision(next_branch)) {
      LOG(INFO) << "UNSAT \n" << StatusString();
      return MODEL_UNSAT;
    }
  }
}

void SatSolver::BumpVariableActivities(const std::vector<Literal>& literals,
                                       int bump_again_lbd_limit) {
  SCOPED_TIME_STAT(&stats_);
  if (parameters_.variable_ordering() !=
      SatParameters::DYNAMIC_MAX_ACTIVITY_FIRST) return;
  const double max_activity_value = parameters_.max_variable_activity_value();
  for (const Literal literal : literals) {
    const VariableIndex var = literal.Variable();
    if (DecisionLevel(var) == choice_points_.size() &&
        trail_.Info(var).type == AssignmentInfo::CLAUSE_PROPAGATION &&
        trail_.Info(var).sat_clause->IsLearned() &&
        trail_.Info(var).sat_clause->Lbd() < bump_again_lbd_limit) {
      activities_[var] += variable_activity_increment_;
    }
    activities_[var] += variable_activity_increment_;
    if (activities_[var] > max_activity_value) {
      RescaleVariableActivities(1.0 / max_activity_value);
    }
  }
}

void SatSolver::BumpReasonActivities(const std::vector<Literal>& literals) {
  SCOPED_TIME_STAT(&stats_);
  for (const Literal literal : literals) {
    const VariableIndex var = literal.Variable();
    if (trail_.Info(var).type == AssignmentInfo::CLAUSE_PROPAGATION) {
      BumpClauseActivity(trail_.Info(var).sat_clause);
    }
  }
}

void SatSolver::BumpClauseActivity(SatClause* clause) {
  if (!clause->IsLearned()) return;
  clause->IncreaseActivity(clause_activity_increment_);
  if (clause->Activity() > parameters_.max_clause_activity_value()) {
    RescaleClauseActivities(1.0 / parameters_.max_clause_activity_value());
  }
}

void SatSolver::RescaleVariableActivities(double scaling_factor) {
  SCOPED_TIME_STAT(&stats_);
  variable_activity_increment_ *= scaling_factor;
  for (VariableIndex var(0); var < activities_.size(); ++var) {
    activities_[var] *= scaling_factor;
  }

  // When rescaling the activities of all the variables, the order of the active
  // variables in the heap will not change, but we still need to update their
  // weights so that newly inserted elements will compare correctly with already
  // inserted ones.
  for (VariableIndex var(0); var < activities_.size(); ++var) {
    queue_elements_[var].set_weight(activities_[var]);
  }
}

void SatSolver::RescaleClauseActivities(double scaling_factor) {
  SCOPED_TIME_STAT(&stats_);
  clause_activity_increment_ *= scaling_factor;
  for (SatClause* clause : learned_clauses_) {
    clause->MultiplyActivity(scaling_factor);
  }
}

void SatSolver::UpdateVariableActivityIncrement() {
  SCOPED_TIME_STAT(&stats_);
  variable_activity_increment_ *= 1.0 / parameters_.variable_activity_decay();
}

void SatSolver::UpdateClauseActivityIncrement() {
  SCOPED_TIME_STAT(&stats_);
  clause_activity_increment_ *= 1.0 / parameters_.clause_activity_decay();
}

bool SatSolver::IsConflictValid(const std::vector<Literal>& literals) {
  SCOPED_TIME_STAT(&stats_);
  if (literals.empty()) return false;
  const int current_level = choice_points_.size();
  int num_literal_at_current_level = 0;
  for (const Literal literal : literals) {
    const int level = DecisionLevel(literal.Variable());
    if (level <= 0 || level > current_level) return false;
    if (level == current_level) {
      ++num_literal_at_current_level;
      if (num_literal_at_current_level > 1) return false;
    }
  }
  if (num_literal_at_current_level != 1) return false;
  return true;
}

int SatSolver::ComputeBacktrackLevel(const std::vector<Literal>& literals) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(!choice_points_.empty());

  // Note(user): if the learned clause is of size 1, we backtack all the way to
  // the beginning. It may be possible to follow another behavior, but then the
  // code require some special cases in
  // AddLearnedClauseAndEnqueueUnitPropagation() to fix the literal and not
  // backtrack over it. Also, subsequent propagated variables may not have a
  // correct level in this case.
  if (literals.size() == 1) {
    return 0;
  }

  // We want the highest level which is not the current one.
  int backtrack_level = 0;
  const int current_level = choice_points_.size();
  for (const Literal literal : literals) {
    const int level = DecisionLevel(literal.Variable());
    if (level != current_level) {
      backtrack_level = std::max(backtrack_level, level);
    }
  }
  VLOG(2) << Indent() << "backtrack_level: " << backtrack_level;
  DCHECK_LT(backtrack_level, current_level);
  return backtrack_level;
}

template<typename LiteralList>
int SatSolver::ComputeLbd(const LiteralList& literals) {
  SCOPED_TIME_STAT(&stats_);
  is_level_marked_.ClearAndResize(SatDecisionLevel(choice_points_.size() + 1));
  for (const Literal literal : literals) {
    const SatDecisionLevel level(DecisionLevel(literal.Variable()));
    DCHECK_GE(level, 0);
    if (level > 0 && !is_level_marked_[level]) {
      is_level_marked_.Set(level);
    }
  }
  return is_level_marked_.NumberOfSetCallsWithDifferentArguments();
}

string SatSolver::StatusString() const {
  const double time_in_s = timer_.Get();
  return
     StringPrintf("  time: %fs\n", time_in_s)
     + StringPrintf("  memory: %s\n", MemoryUsage().c_str())
     + StringPrintf(
        "  num failures: %" GG_LL_FORMAT "d  (%.0f /sec)\n",
        counters_.num_failures,
        static_cast<double>(counters_.num_failures) / time_in_s)
     + StringPrintf("  num branches: %" GG_LL_FORMAT "d"
        "  (%.2f%% random) (%.0f /sec)\n",
        counters_.num_branches,
        100.0 * static_cast<double>(counters_.num_random_branches)
            / counters_.num_branches,
        static_cast<double>(counters_.num_branches) / time_in_s)
     + StringPrintf("  num propagations: %" GG_LL_FORMAT "d  (%.0f /sec)\n",
        num_propagations(),
        static_cast<double>(num_propagations()) / time_in_s)
     + StringPrintf("  num binary propagations: %" GG_LL_FORMAT "d\n",
        binary_implication_graph_.num_propagations())
     + StringPrintf("  num classic minimizations: %" GG_LL_FORMAT "d"
                  "  (literals removed: %" GG_LL_FORMAT "d)\n",
        counters_.num_minimizations,
        counters_.num_literals_removed)
     + StringPrintf("  num binary minimizations: %" GG_LL_FORMAT "d"
                  "  (literals removed: %" GG_LL_FORMAT "d)\n",
        binary_implication_graph_.num_minimization(),
        binary_implication_graph_.num_literals_removed())
     + StringPrintf("  num inspected clauses: %" GG_LL_FORMAT "d\n",
                  watched_clauses_.num_inspected_clauses())
     + StringPrintf("  num learned literals: %lld  (%.2f%% forgotten)\n",
        counters_.num_literals_learned, 100.0 *
            static_cast<double>(counters_.num_literals_forgotten)
            / static_cast<double>(counters_.num_literals_learned))
     + StringPrintf("  num learned clause cleanups: %d\n",
                    clause_cleanup_count_)
     + StringPrintf("  num restarts: %d\n", restart_count_);
}

string SatSolver::RunningStatisticsString() const {
  const double time_in_s = timer_.Get();
  const int learned = learned_clauses_.size();
  return StringPrintf("%6.2lfs, mem:%s, fails:%" GG_LL_FORMAT "d, "
                      "depth:%d, conflicts:%d/%d, restarts:%d, "
                      "vars:%d",
                      time_in_s,
                      MemoryUsage().c_str(),
                      counters_.num_failures,
                      static_cast<int>(choice_points_.size()),
                      learned,
                      learned_clause_limit_,
                      restart_count_,
                      num_variables_.value() - num_processed_fixed_variables_);
}

double SatSolver::ComputeInitialVariableWeight(VariableIndex var) const {
  if (leave_initial_activities_unchanged_) return activities_[var];
  const double usage = statistics_[var].weighted_num_appearances;
  switch (parameters_.variable_ordering()) {
    case SatParameters::STATIC_IN_ORDER: return -var.value();
    case SatParameters::STATIC_MIN_USAGE_FIRST: return -usage;
    case SatParameters::STATIC_MAX_USAGE_FIRST: return usage;
    case SatParameters::DYNAMIC_MAX_ACTIVITY_FIRST:
      // TODO(user): Rescale this to a double in [0, 1] so that as soon as a
      // variable activity is bumped, it gets an higher priority than the other
      // variables?
      return usage;
  }
}

void SatSolver::ComputeStatistics() {
  SCOPED_TIME_STAT(&stats_);
  for (int i = 0; i < problem_clauses_.size(); ++i) {
    UpdateStatisticsOnOneClause(
        *problem_clauses_[i], problem_clauses_[i]->Size(), /*added=*/ true);
  }
  for (VariableIndex var(0); var < num_variables_; ++var) {
    activities_[var] = ComputeInitialVariableWeight(var);
  }
}

void SatSolver::ProcessNewlyFixedVariables() {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(choice_points_.empty());
  int num_detached_clauses = 0;
  int num_binary = 0;
  std::vector<Literal> binary;
  for (int i = 0; i < 2; ++i) {
    for (SatClause* clause : (i == 0) ? problem_clauses_ : learned_clauses_) {
      if (clause->IsAttached()) {
        binary.clear();
        for (Literal literal : *clause) {
          if (trail_.Assignment().IsLiteralTrue(literal)) {
            // The clause is always true, detach it.
            DCHECK_EQ(0, DecisionLevel(literal.Variable()));
            UpdateStatisticsOnOneClause(
                *clause, clause->Size(), /*added=*/ false);
            watched_clauses_.LazyDetach(clause);
            ++num_detached_clauses;
            binary.clear();
            break;
          } else if (binary.size() <= 2 &&
              !trail_.Assignment().IsLiteralFalse(literal)) {
            binary.push_back(literal);
          }
        }
        // The clause is now a binary clause, treat it separately.
        DCHECK_NE(binary.size(), 1);
        if (parameters_.treat_binary_clauses_separately() &&
            binary.size() == 2) {
          binary_implication_graph_.AddBinaryClause(binary[0], binary[1]);
          watched_clauses_.LazyDetach(clause);
          ++num_binary;
        }
      }
    }
  }
  watched_clauses_.CleanUpWatchers();
  if (num_detached_clauses > 0) {
    VLOG(1) << trail_.Index() << " fixed variables at level 0. "
            << "Detached " << num_detached_clauses
            << " clauses. " << num_binary << " converted to binary.";

    // Free-up learned clause memory. Note that this also postpone a bit the
    // next clause cleaning phase since we removed some clauses.
    std::vector<SatClause*>::iterator iter = std::partition(
        learned_clauses_.begin(),
        learned_clauses_.end(),
        std::bind1st(std::mem_fun(&SatSolver::IsClauseAttachedOrUsedAsReason),
                     this));
    STLDeleteContainerPointers(iter, learned_clauses_.end());
    learned_clauses_.erase(iter, learned_clauses_.end());
  }

  // We also clean the binary implication graph.
  binary_implication_graph_.RemoveFixedVariables(trail_.Assignment());
  num_processed_fixed_variables_ = trail_.Index();
}

bool SatSolver::Preprocess() {
  SCOPED_TIME_STAT(&stats_);
  // Remove variables appearing at most on one direction.
  // Note that we can't force an already assigned variable.
  for (VariableIndex v(0); v < num_variables_; ++v) {
    if (trail_.Assignment().IsVariableAssigned(v)) continue;
    if (statistics_[v].num_positive_clauses == 0) {
      const Literal forced(v, false);
      VLOG(1) << "  - forcing " << forced;
      if (!TestValidityAndEnqueueIfNeeded(forced)) return false;
    } else if (statistics_[v].num_negative_clauses == 0) {
      const Literal forced(v, true);
      VLOG(1) << "  - forcing " << forced;
      if (!TestValidityAndEnqueueIfNeeded(forced)) return false;
    }
  }
  return true;
}

bool SatSolver::Propagate() {
  SCOPED_TIME_STAT(&stats_);
  // Inspect all the assignements that still need to be propagated.
  DCHECK_GE(binary_propagation_trail_index_, propagation_trail_index_);
  do {
    // We want to prioritize the propagations due to binary clauses. To do so,
    // before each assignement is propagated with the non-binary clauses, we
    // propagate ALL the assignment on the trail using the binary clauses.
    //
    // All the literals with trail index smaller than
    // binary_propagation_trail_index_ are the assignments that were already
    // propagated using binary clauses and thus do not need to be inspected
    // again.
    while (binary_propagation_trail_index_ < trail_.Index()) {
      const Literal literal = trail_[binary_propagation_trail_index_];
      ++binary_propagation_trail_index_;
      if (!binary_implication_graph_.PropagateOnTrue(literal, &trail_)) {
        return false;
      }
    }

    // Propagate one literal using non-binary clauses.
    bool more_work = false;
    if (propagation_trail_index_ < trail_.Index()) {
      const Literal literal = trail_[propagation_trail_index_];
      ++propagation_trail_index_;
      DCHECK_EQ(DecisionLevel(literal.Variable()), choice_points_.size());
      if (!watched_clauses_.PropagateOnTrue(literal, &trail_)) {
        return false;
      }
      more_work = true;
    }

    // Propagate one literal using linear constraints.
    if (pb_constraints_.PropagationNeeded()) {
      if (!pb_constraints_.PropagateNext()) return false;
      more_work = true;
    }
    if (!more_work) break;
  } while (true);
  return true;
}

bool SatSolver::TestValidityAndEnqueueIfNeeded(Literal literal) {
  SCOPED_TIME_STAT(&stats_);
  if (trail_.Assignment().IsLiteralFalse(literal)) return false;
  if (trail_.Assignment().IsLiteralTrue(literal)) return true;
  trail_.Enqueue(literal, AssignmentInfo::PREPROCESSING);  // Not assigned.
  return true;
}

ClauseRef SatSolver::Reason(VariableIndex var) const {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(trail_.Assignment().IsVariableAssigned(var));
  switch (trail_.Info(var).type) {
    case AssignmentInfo::PREPROCESSING:
    case AssignmentInfo::SEARCH_DECISION:
    case AssignmentInfo::UNIT_REASON:
      return ClauseRef();
    case AssignmentInfo::CLAUSE_PROPAGATION:
      return trail_.Info(var).sat_clause->ToClauseRef();
    case AssignmentInfo::BINARY_PROPAGATION: {
      const Literal* literal = &trail_.Info(var).literal;
      return ClauseRef(literal, literal + 1);
    }
    case AssignmentInfo::PB_PROPAGATION:
      return ClauseRef(pb_constraints_.ReasonFor(var));
  }
}

void SatSolver::NewChoicePoint(Literal literal) {
  SCOPED_TIME_STAT(&stats_);
  counters_.num_branches++;
  choice_points_.push_back(ChoicePoint(trail_.Index(), literal));
  trail_.SetDecisionLevel(choice_points_.size());
  trail_.Enqueue(literal, AssignmentInfo::SEARCH_DECISION);
}

Literal SatSolver::NextBranch() {
  SCOPED_TIME_STAT(&stats_);
  DCHECK_EQ(trail_.Index(), num_inactive_variables_);
  DCHECK_EQ(var_ordering_.Size(),
            num_variables_.value() - num_inactive_variables_);
  DCHECK(!var_ordering_.IsEmpty());

  // Choose the variable.
  VariableIndex var = var_ordering_.Top()->variable();
  if (random_.RandDouble() < parameters_.random_branches_ratio()) {
    var = (*var_ordering_.Raw())[
        random_.Uniform(var_ordering_.Raw()->size())]->variable();
    ++counters_.num_random_branches;
  }
  CHECK(!trail_.Assignment().IsVariableAssigned(var));

  // Choose its assignment (i.e. True of False).
  const bool sign = statistics_[var].num_positive_clauses
                  > statistics_[var].num_negative_clauses;
  const bool polarity = trail_.Assignment()
      .GetLastVariableValueIfEverAssignedOrDefault(var, sign);
  switch (parameters_.variable_branching()) {
    case SatParameters::FIXED_POSITIVE: return Literal(var, true);
    case SatParameters::FIXED_NEGATIVE: return Literal(var, false);
    case SatParameters::SIGN: return Literal(var, sign);
    case SatParameters::REVERSE_SIGN: return Literal(var, !sign);
    case SatParameters::POLARITY: return Literal(var, polarity);
    case SatParameters::REVERSE_POLARITY: return Literal(var, !polarity);
  }
}

void SatSolver::MakeAllTrailVariablesInactive() {
  SCOPED_TIME_STAT(&stats_);
  DCHECK_LE(num_inactive_variables_, trail_.Index());
  DCHECK_EQ(var_ordering_.Size(),
      num_variables_.value() - num_inactive_variables_);
  for (int i = num_inactive_variables_; i < trail_.Index(); ++i) {
    const VariableIndex var = trail_[i].Variable();
    DCHECK(var_ordering_.Contains(&(queue_elements_[var])));
    var_ordering_.Remove(&queue_elements_[var]);
  }
  num_inactive_variables_ = trail_.Index();
}

void SatSolver::ComputeInitialVariableOrdering() {
  SCOPED_TIME_STAT(&stats_);
  var_ordering_.Clear();
  for (VariableIndex var(0); var < num_variables_; ++var) {
    queue_elements_[var].set_variable(var);
    if (!trail_.Assignment().IsVariableAssigned(var)) {
      queue_elements_[var].set_weight(activities_[var]);
      var_ordering_.Add(&queue_elements_[var]);
    }
  }
}

void SatSolver::Untrail(int target_trail_index) {
  SCOPED_TIME_STAT(&stats_);
  pb_constraints_.Untrail(target_trail_index);
  while (trail_.Index() > target_trail_index) {
    const Literal assigned_literal = trail_.Dequeue();

    // Update the variable weight, and make sure the priority queue is updated.
    const VariableIndex var = assigned_literal.Variable();
    WeightedVarQueueElement* element = &(queue_elements_[var]);

    const double new_weight = activities_[var];
    if (var_ordering_.Contains(element)) {
      if (new_weight != element->weight()) {
        element->set_weight(new_weight);
        var_ordering_.NoteChangedPriority(element);
      }
    } else {
      element->set_weight(new_weight);
      var_ordering_.Add(element);
    }
  }
  propagation_trail_index_ = target_trail_index;
  binary_propagation_trail_index_ = target_trail_index;
  num_inactive_variables_ = trail_.Index();
}

bool SatSolver::IsAssignmentValid(const VariablesAssignment& assignment) const {
  SCOPED_TIME_STAT(&stats_);
  VLOG(2) << "Checking solution";
  // Check that all variables are assigned.
  for (VariableIndex var(0); var < num_variables_; ++var) {
    if (!assignment.IsVariableAssigned(var)) {
      LOG(ERROR) << "  - var " << var.value() << " undefined";
      return false;
    }
  }
  // Check that all clauses are satisfied.
  for (SatClause* clause : problem_clauses_) {
    if (!clause->IsSatisfied(assignment)) {
      LOG(ERROR) << "  - clause error: " << DebugString(*clause);
      return false;
    }
  }
  string result;
  for (VariableIndex var(0); var < num_variables_; ++var) {
    result.append(trail_.Assignment()
        .GetTrueLiteralForAssignedVariable(var).DebugString());
    result.append(" ");
  }
  return true;
}

string SatSolver::DebugString(const SatClause& clause) const {
  string result;
  for (const Literal literal : clause) {
    if (!result.empty()) {
      result.append(" || ");
    }
    const string value = trail_.Assignment().IsLiteralTrue(literal)
        ? "true" : (trail_.Assignment().IsLiteralFalse(literal) ? "false"
                                                                : "undef");
    result.append(
        StringPrintf("%s(%s)", literal.DebugString().c_str(), value.c_str()));
  }
  return result;
}

}  // namespace sat
}  // namespace operations_research
