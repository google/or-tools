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

#include "ortools/sat/sat_solver.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <random>
#include <string>
#include <type_traits>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/sysinfo.h"
#include "ortools/base/split.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/base/adjustable_priority_queue-inl.h"

namespace operations_research {
namespace sat {

SatSolver::SatSolver() : SatSolver(new Model()) { owned_model_.reset(model_); }

SatSolver::SatSolver(Model* model)
    : model_(model),
      num_variables_(0),
      pb_constraints_(),
      track_binary_clauses_(false),
      trail_(model->GetOrCreate<Trail>()),
      current_decision_level_(0),
      last_decision_or_backtrack_trail_index_(0),
      assumption_level_(0),
      num_processed_fixed_variables_(0),
      deterministic_time_of_last_fixed_variables_cleanup_(0.0),
      counters_(),
      is_model_unsat_(false),
      var_ordering_is_initialized_(false),
      variable_activity_increment_(1.0),
      clause_activity_increment_(1.0),
      decision_heuristic_is_initialized_(false),
      num_learned_clause_before_cleanup_(0),
      conflicts_until_next_restart_(0),
      restart_count_(0),
      luby_count_(0),
      conflicts_until_next_strategy_change_(0),
      strategy_counter_(0),
      same_reason_identifier_(*trail_),
      is_relevant_for_core_computation_(true),
      time_limit_(TimeLimit::Infinite()),
      deterministic_time_at_last_advanced_time_limit_(0.0),
      problem_is_pure_sat_(true),
      drat_writer_(nullptr),
      stats_("SatSolver") {
  trail_->RegisterPropagator(&binary_implication_graph_);
  trail_->RegisterPropagator(&clauses_propagator_);
  trail_->RegisterPropagator(&pb_constraints_);
  InitializePropagators();
  SetParameters(parameters_);
}

SatSolver::~SatSolver() {
  IF_STATS_ENABLED(LOG(INFO) << stats_.StatString());
  STLDeleteElements(&clauses_);
}

void SatSolver::SetNumVariables(int num_variables) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(!is_model_unsat_);
  CHECK_GE(num_variables, num_variables_);
  const BooleanVariable old_num_variables = num_variables_;

  num_variables_ = num_variables;
  binary_implication_graph_.Resize(num_variables);
  clauses_propagator_.Resize(num_variables);
  trail_->Resize(num_variables);
  pb_constraints_.Resize(num_variables);
  decisions_.resize(num_variables);
  same_reason_identifier_.Resize(num_variables);

  // Used by NextBranch() for the decision heuristic.
  activities_.resize(num_variables, parameters_.initial_variables_activity());
  num_bumps_.resize(num_variables, 0);
  pq_need_update_for_var_at_trail_index_.IncreaseSize(num_variables);
  weighted_sign_.resize(num_variables, 0.0);
  queue_elements_.resize(num_variables);

  // Only reset the polarity of the new variables.
  // Note that this must be called after the trail_ has been resized.
  ResetPolarity(/*from=*/old_num_variables);

  // Important: Because there is new variables, we need to recompute the
  // priority queue. Note that this will not reset the activity, it will however
  // change the order of the element with the same priority.
  //
  // TODO(user): Not even do that and just push the new ones at the end?
  var_ordering_is_initialized_ = false;
}

int64 SatSolver::num_branches() const { return counters_.num_branches; }

int64 SatSolver::num_failures() const { return counters_.num_failures; }

int64 SatSolver::num_propagations() const {
  return trail_->NumberOfEnqueues() - counters_.num_branches;
}

double SatSolver::deterministic_time() const {
  // Each of these counters mesure really basic operations.
  // The weight are just an estimate of the operation complexity.
  //
  // TODO(user): Find a better procedure to fix the weight than just educated
  // guess.
  return 1e-8 * (8.0 * trail_->NumberOfEnqueues() +
                 1.0 * binary_implication_graph_.num_inspections() +
                 4.0 * clauses_propagator_.num_inspected_clauses() +
                 1.0 * clauses_propagator_.num_inspected_clause_literals() +

                 // Here there is a factor 2 because of the untrail.
                 20.0 * pb_constraints_.num_constraint_lookups() +
                 2.0 * pb_constraints_.num_threshold_updates() +
                 1.0 * pb_constraints_.num_inspected_constraint_literals());
}

const SatParameters& SatSolver::parameters() const {
  SCOPED_TIME_STAT(&stats_);
  return parameters_;
}

void SatSolver::SetParameters(const SatParameters& parameters) {
  SCOPED_TIME_STAT(&stats_);
  parameters_ = parameters;
  clauses_propagator_.SetParameters(parameters);
  pb_constraints_.SetParameters(parameters);
  random_.seed(parameters_.random_seed());
  InitRestart();
  time_limit_ = TimeLimit::FromParameters(parameters_);
  dl_running_average_.Reset(parameters_.restart_running_window_size());
  lbd_running_average_.Reset(parameters_.restart_running_window_size());
  trail_size_running_average_.Reset(parameters_.blocking_restart_window_size());
  deterministic_time_at_last_advanced_time_limit_ = deterministic_time();
}

std::string SatSolver::Indent() const {
  SCOPED_TIME_STAT(&stats_);
  const int level = CurrentDecisionLevel();
  std::string result;
  for (int i = 0; i < level; ++i) {
    result.append("|   ");
  }
  return result;
}

bool SatSolver::IsMemoryLimitReached() const {
  const int64 memory_usage = GetProcessMemoryUsage();
  const int64 kMegaByte = 1024 * 1024;
  return memory_usage > kMegaByte * parameters_.max_memory_in_mb();
}

bool SatSolver::SetModelUnsat() {
  is_model_unsat_ = true;
  return false;
}

bool SatSolver::AddUnitClause(Literal true_literal) {
  SCOPED_TIME_STAT(&stats_);
  CHECK_EQ(CurrentDecisionLevel(), 0);
  if (is_model_unsat_) return false;
  if (trail_->Assignment().LiteralIsFalse(true_literal)) return SetModelUnsat();
  if (trail_->Assignment().LiteralIsTrue(true_literal)) return true;
  trail_->EnqueueWithUnitReason(true_literal);
  if (!Propagate()) return SetModelUnsat();
  return true;
}

bool SatSolver::AddBinaryClause(Literal a, Literal b) {
  SCOPED_TIME_STAT(&stats_);
  tmp_pb_constraint_.clear();
  tmp_pb_constraint_.push_back(LiteralWithCoeff(a, 1));
  tmp_pb_constraint_.push_back(LiteralWithCoeff(b, 1));
  return AddLinearConstraint(
      /*use_lower_bound=*/true, /*lower_bound=*/Coefficient(1),
      /*use_upper_bound=*/false, /*upper_bound=*/Coefficient(0),
      &tmp_pb_constraint_);
}

bool SatSolver::AddTernaryClause(Literal a, Literal b, Literal c) {
  SCOPED_TIME_STAT(&stats_);
  tmp_pb_constraint_.clear();
  tmp_pb_constraint_.push_back(LiteralWithCoeff(a, 1));
  tmp_pb_constraint_.push_back(LiteralWithCoeff(b, 1));
  tmp_pb_constraint_.push_back(LiteralWithCoeff(c, 1));
  return AddLinearConstraint(
      /*use_lower_bound=*/true, /*lower_bound=*/Coefficient(1),
      /*use_upper_bound=*/false, /*upper_bound=*/Coefficient(0),
      &tmp_pb_constraint_);
}

bool SatSolver::AddProblemClause(const std::vector<Literal>& literals) {
  SCOPED_TIME_STAT(&stats_);

  // TODO(user): To avoid duplication, we currently just call
  // AddLinearConstraint(). Make a faster specific version if that becomes a
  // performance issue.
  tmp_pb_constraint_.clear();
  for (Literal lit : literals) {
    tmp_pb_constraint_.push_back(LiteralWithCoeff(lit, 1));
  }
  return AddLinearConstraint(
      /*use_lower_bound=*/true, /*lower_bound=*/Coefficient(1),
      /*use_upper_bound=*/false, /*upper_bound=*/Coefficient(0),
      &tmp_pb_constraint_);
}

bool SatSolver::AddProblemClauseInternal(const std::vector<Literal>& literals) {
  SCOPED_TIME_STAT(&stats_);
  CHECK_EQ(CurrentDecisionLevel(), 0);

  // Deals with clause of size 0 (always false) and 1 (set a literal) right away
  // so we guarantee that a SatClause is always of size greater than one. This
  // simplifies the code.
  CHECK_GT(literals.size(), 0);
  if (literals.size() == 1) {
    if (trail_->Assignment().LiteralIsFalse(literals[0])) return false;
    if (trail_->Assignment().LiteralIsTrue(literals[0])) return true;
    trail_->EnqueueWithUnitReason(literals[0]);  // Not assigned.
    return true;
  }

  if (parameters_.treat_binary_clauses_separately() && literals.size() == 2) {
    AddBinaryClauseInternal(literals[0], literals[1]);
  } else {
    std::unique_ptr<SatClause> clause(
        SatClause::Create(literals, /*is_redundant=*/false));
    if (!clauses_propagator_.AttachAndPropagate(clause.get(), trail_)) {
      return SetModelUnsat();
    }
    clauses_.push_back(clause.release());
  }
  return true;
}

bool SatSolver::AddLinearConstraintInternal(
    const std::vector<LiteralWithCoeff>& cst, Coefficient rhs,
    Coefficient max_value) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(BooleanLinearExpressionIsCanonical(cst));
  if (rhs < 0) return SetModelUnsat();  // Unsatisfiable constraint.
  if (rhs >= max_value) return true;    // Always satisfied constraint.

  // Update the weighted_sign_.
  // TODO(user): special case the rhs = 0 which just fix variables...
  if (rhs > 0) {
    for (const LiteralWithCoeff& term : cst) {
      const double weight = static_cast<double>(term.coefficient.value()) /
                            static_cast<double>(rhs.value());
      weighted_sign_[term.literal.Variable()] +=
          term.literal.IsPositive() ? -weight : weight;
    }
  }

  // A linear upper bounded constraint is a clause if the only problematic
  // assignment is the one where all the literals are true. Since they are
  // ordered by coefficient, this is easy to check.
  if (max_value - cst[0].coefficient <= rhs) {
    // This constraint is actually a clause. It is faster to treat it as one.
    literals_scratchpad_.clear();
    for (const LiteralWithCoeff& term : cst) {
      literals_scratchpad_.push_back(term.literal.Negated());
    }
    return AddProblemClauseInternal(literals_scratchpad_);
  }

  problem_is_pure_sat_ = false;

  // TODO(user): If this constraint forces all its literal to false (when rhs is
  // zero for instance), we still add it. Optimize this?
  const bool result = pb_constraints_.AddConstraint(cst, rhs, trail_);
  InitializePropagators();
  return result;
}

bool SatSolver::AddLinearConstraint(bool use_lower_bound,
                                    Coefficient lower_bound,
                                    bool use_upper_bound,
                                    Coefficient upper_bound,
                                    std::vector<LiteralWithCoeff>* cst) {
  SCOPED_TIME_STAT(&stats_);
  CHECK_EQ(CurrentDecisionLevel(), 0);
  if (is_model_unsat_) return false;

  // This block removes assigned literals from the constraint.
  Coefficient fixed_variable_shift(0);
  {
    int index = 0;
    for (const LiteralWithCoeff& term : *cst) {
      if (trail_->Assignment().LiteralIsFalse(term.literal)) continue;
      if (trail_->Assignment().LiteralIsTrue(term.literal)) {
        CHECK(SafeAddInto(-term.coefficient, &fixed_variable_shift));
        continue;
      }
      (*cst)[index] = term;
      ++index;
    }
    cst->resize(index);
  }

  // Canonicalize the constraint.
  Coefficient bound_shift;
  Coefficient max_value;
  CHECK(ComputeBooleanLinearExpressionCanonicalForm(cst, &bound_shift,
                                                    &max_value));
  CHECK(SafeAddInto(fixed_variable_shift, &bound_shift));

  if (use_upper_bound) {
    const Coefficient rhs =
        ComputeCanonicalRhs(upper_bound, bound_shift, max_value);
    if (!AddLinearConstraintInternal(*cst, rhs, max_value)) {
      return SetModelUnsat();
    }
  }
  if (use_lower_bound) {
    // We transform the constraint into an upper-bounded one.
    for (int i = 0; i < cst->size(); ++i) {
      (*cst)[i].literal = (*cst)[i].literal.Negated();
    }
    const Coefficient rhs =
        ComputeNegatedCanonicalRhs(lower_bound, bound_shift, max_value);
    if (!AddLinearConstraintInternal(*cst, rhs, max_value)) {
      return SetModelUnsat();
    }
  }
  if (!Propagate()) return SetModelUnsat();
  return true;
}

void SatSolver::AddLearnedClauseAndEnqueueUnitPropagation(
    const std::vector<Literal>& literals, bool is_redundant) {
  SCOPED_TIME_STAT(&stats_);

  // Note that we need to output the learned clause before cleaning the clause
  // database. This is because we already backtracked and some of the clauses
  // that where needed to infer the conflict may not be "reasons" anymore and
  // may be deleted.
  if (drat_writer_ != nullptr) {
    drat_writer_->AddClause(literals);
  }

  if (literals.size() == 1) {
    // A length 1 clause fix a literal for all the search.
    // ComputeBacktrackLevel() should have returned 0.
    CHECK_EQ(CurrentDecisionLevel(), 0);
    trail_->EnqueueWithUnitReason(literals[0]);
    lbd_running_average_.Add(1);
  } else if (literals.size() == 2 &&
             parameters_.treat_binary_clauses_separately()) {
    if (track_binary_clauses_) {
      CHECK(binary_clauses_.Add(BinaryClause(literals[0], literals[1])));
    }
    binary_implication_graph_.AddBinaryClauseDuringSearch(literals[0],
                                                          literals[1], trail_);
    lbd_running_average_.Add(2);

    // In case this is the first binary clauses.
    InitializePropagators();
  } else {
    CleanClauseDatabaseIfNeeded();
    SatClause* clause = SatClause::Create(literals, is_redundant);
    clauses_.emplace_back(clause);

    // Important: Even though the only literal at the last decision level has
    // been unassigned, its level was not modified, so ComputeLbd() works.
    const int lbd = ComputeLbd(*clause);
    lbd_running_average_.Add(lbd);

    if (is_redundant && lbd > parameters_.clause_cleanup_lbd_bound()) {
      --num_learned_clause_before_cleanup_;

      // BumpClauseActivity() must be called after clauses_info_[clause] has
      // been created or it will have no effect.
      DCHECK(clauses_info_.find(clause) == clauses_info_.end());
      clauses_info_[clause].lbd = lbd;
      BumpClauseActivity(clause);
    }

    CHECK(clauses_propagator_.AttachAndPropagate(clause, trail_));
  }
}

void SatSolver::AddPropagator(SatPropagator* propagator) {
  CHECK_EQ(CurrentDecisionLevel(), 0);
  problem_is_pure_sat_ = false;
  trail_->RegisterPropagator(propagator);
  external_propagators_.push_back(propagator);
  InitializePropagators();
}

void SatSolver::AddLastPropagator(SatPropagator* propagator) {
  CHECK_EQ(CurrentDecisionLevel(), 0);
  CHECK(last_propagator_ == nullptr);
  problem_is_pure_sat_ = false;
  trail_->RegisterPropagator(propagator);
  last_propagator_ = propagator;
  InitializePropagators();
}

UpperBoundedLinearConstraint* SatSolver::ReasonPbConstraintOrNull(
    BooleanVariable var) const {
  // It is important to deal properly with "SameReasonAs" variables here.
  var = trail_->ReferenceVarWithSameReason(var);
  const AssignmentInfo& info = trail_->Info(var);
  if (trail_->AssignmentType(var) == pb_constraints_.PropagatorId()) {
    return pb_constraints_.ReasonPbConstraint(info.trail_index);
  }
  return nullptr;
}

SatClause* SatSolver::ReasonClauseOrNull(BooleanVariable var) const {
  DCHECK(trail_->Assignment().VariableIsAssigned(var));
  const AssignmentInfo& info = trail_->Info(var);
  if (trail_->AssignmentType(var) == clauses_propagator_.PropagatorId()) {
    return clauses_propagator_.ReasonClause(info.trail_index);
  }
  return nullptr;
}

void SatSolver::SaveDebugAssignment() {
  debug_assignment_.Resize(num_variables_.value());
  for (BooleanVariable i(0); i < num_variables_; ++i) {
    debug_assignment_.AssignFromTrueLiteral(
        trail_->Assignment().GetTrueLiteralForAssignedVariable(i));
  }
}

void SatSolver::AddBinaryClauseInternal(Literal a, Literal b) {
  if (!track_binary_clauses_ || binary_clauses_.Add(BinaryClause(a, b))) {
    binary_implication_graph_.AddBinaryClause(a, b);

    // In case this is the first binary clauses.
    InitializePropagators();
  }
}

bool SatSolver::ClauseIsValidUnderDebugAssignement(
    const std::vector<Literal>& clause) const {
  for (Literal l : clause) {
    if (l.Variable() >= debug_assignment_.NumberOfVariables() ||
        debug_assignment_.LiteralIsTrue(l)) {
      return true;
    }
  }
  return false;
}

bool SatSolver::PBConstraintIsValidUnderDebugAssignment(
    const std::vector<LiteralWithCoeff>& cst, const Coefficient rhs) const {
  Coefficient sum(0.0);
  for (LiteralWithCoeff term : cst) {
    if (term.literal.Variable() >= debug_assignment_.NumberOfVariables()) {
      continue;
    }
    if (debug_assignment_.LiteralIsTrue(term.literal)) {
      sum += term.coefficient;
    }
  }
  return sum <= rhs;
}

namespace {

// Returns true iff 'b' is subsumed by 'a' (i.e 'a' is included in 'b').
// This is slow and only meant to be used in DCHECKs.
bool ClauseSubsumption(const std::vector<Literal>& a, SatClause* b) {
  std::vector<Literal> superset(b->begin(), b->end());
  std::vector<Literal> subset(a.begin(), a.end());
  std::sort(superset.begin(), superset.end());
  std::sort(subset.begin(), subset.end());
  return std::includes(superset.begin(), superset.end(), subset.begin(),
                       subset.end());
}

}  // namespace

int SatSolver::EnqueueDecisionAndBackjumpOnConflict(Literal true_literal) {
  SCOPED_TIME_STAT(&stats_);
  if (is_model_unsat_) return kUnsatTrailIndex;
  CHECK(PropagationIsDone());
  EnqueueNewDecision(true_literal);
  while (!PropagateAndStopAfterOneConflictResolution()) {
    if (is_model_unsat_) return kUnsatTrailIndex;
  }
  CHECK(PropagationIsDone());
  return last_decision_or_backtrack_trail_index_;
}

void SatSolver::RestoreSolverToAssumptionLevel() {
  CHECK(!is_model_unsat_);
  if (CurrentDecisionLevel() > assumption_level_) {
    Backtrack(assumption_level_);
  } else {
    // Finish current propagation.
    while (!PropagateAndStopAfterOneConflictResolution()) {
      if (is_model_unsat_) break;
    }
    // Reapply any assumption that was backtracked over.
    if (CurrentDecisionLevel() < assumption_level_) {
      int unused = 0;
      const int64 old_num_branches = counters_.num_branches;
      ReapplyDecisionsUpTo(assumption_level_ - 1, &unused);
      counters_.num_branches = old_num_branches;
      assumption_level_ = CurrentDecisionLevel();
    }
  }
}

bool SatSolver::PropagateAndStopAfterOneConflictResolution() {
  SCOPED_TIME_STAT(&stats_);
  if (Propagate()) return true;

  ++counters_.num_failures;
  dl_running_average_.Add(current_decision_level_);
  trail_size_running_average_.Add(trail_->Index());

  // Block the restart.
  // Note(user): glucose only activate this after 10000 conflicts.
  if (parameters_.use_blocking_restart()) {
    if (lbd_running_average_.IsWindowFull() &&
        dl_running_average_.IsWindowFull() &&
        trail_size_running_average_.IsWindowFull() &&
        trail_->Index() > parameters_.blocking_restart_multiplier() *
                              trail_size_running_average_.WindowAverage()) {
      dl_running_average_.ClearWindow();
      lbd_running_average_.ClearWindow();
    }
  }

  // A conflict occurred, compute a nice reason for this failure.
  same_reason_identifier_.Clear();
  const int max_trail_index = ComputeMaxTrailIndex(trail_->FailingClause());
  ComputeFirstUIPConflict(max_trail_index, &learned_conflict_,
                          &reason_used_to_infer_the_conflict_,
                          &subsumed_clauses_);

  // An empty conflict means that the problem is UNSAT.
  if (learned_conflict_.empty()) return SetModelUnsat();
  DCHECK(IsConflictValid(learned_conflict_));
  DCHECK(ClauseIsValidUnderDebugAssignement(learned_conflict_));

  // Update the activity of all the variables in the first UIP clause.
  // Also update the activity of the last level variables expanded (and
  // thus discarded) during the first UIP computation. Note that both
  // sets are disjoint.
  const int lbd_limit = parameters_.use_glucose_bump_again_strategy()
                            ? ComputeLbd(learned_conflict_)
                            : 0;
  BumpVariableActivities(learned_conflict_, lbd_limit);
  BumpVariableActivities(reason_used_to_infer_the_conflict_, lbd_limit);
  if (parameters_.also_bump_variables_in_conflict_reasons()) {
    ComputeUnionOfReasons(learned_conflict_, &extra_reason_literals_);
    BumpVariableActivities(extra_reason_literals_, lbd_limit);
  }

  // Bump the clause activities.
  // Note that the activity of the learned clause will be bumped too
  // by AddLearnedClauseAndEnqueueUnitPropagation().
  if (trail_->FailingSatClause() != nullptr) {
    BumpClauseActivity(trail_->FailingSatClause());
  }
  BumpReasonActivities(reason_used_to_infer_the_conflict_);

  // Decay the activities.
  UpdateVariableActivityIncrement();
  UpdateClauseActivityIncrement();
  pb_constraints_.UpdateActivityIncrement();

  // Decrement the restart counter if needed.
  if (conflicts_until_next_restart_ > 0) {
    --conflicts_until_next_restart_;
  }
  if (conflicts_until_next_strategy_change_ > 0) {
    --conflicts_until_next_strategy_change_;
  }

  // Hack from Glucose that seems to perform well.
  const int period = parameters_.glucose_decay_increment_period();
  const double max_decay = parameters_.glucose_max_decay();
  if (counters_.num_failures % period == 0 &&
      parameters_.variable_activity_decay() < max_decay) {
    parameters_.set_variable_activity_decay(
        parameters_.variable_activity_decay() +
        parameters_.glucose_decay_increment());
  }

  // PB resolution.
  // There is no point using this if the conflict and all the reasons involved
  // in its resolution where clauses.
  bool compute_pb_conflict = false;
  if (parameters_.use_pb_resolution()) {
    compute_pb_conflict = (pb_constraints_.ConflictingConstraint() != nullptr);
    if (!compute_pb_conflict) {
      for (Literal lit : reason_used_to_infer_the_conflict_) {
        if (ReasonPbConstraintOrNull(lit.Variable()) != nullptr) {
          compute_pb_conflict = true;
          break;
        }
      }
    }
  }

  // TODO(user): Note that we use the clause above to update the variable
  // activites and not the pb conflict. Experiment.
  if (compute_pb_conflict) {
    pb_conflict_.ClearAndResize(num_variables_.value());
    Coefficient initial_slack(-1);
    if (pb_constraints_.ConflictingConstraint() == nullptr) {
      // Generic clause case.
      Coefficient num_literals(0);
      for (Literal literal : trail_->FailingClause()) {
        pb_conflict_.AddTerm(literal.Negated(), Coefficient(1.0));
        ++num_literals;
      }
      pb_conflict_.AddToRhs(num_literals - 1);
    } else {
      // We have a pseudo-Boolean conflict, so we start from there.
      pb_constraints_.ConflictingConstraint()->AddToConflict(&pb_conflict_);
      pb_constraints_.ClearConflictingConstraint();
      initial_slack =
          pb_conflict_.ComputeSlackForTrailPrefix(*trail_, max_trail_index + 1);
    }

    int pb_backjump_level;
    ComputePBConflict(max_trail_index, initial_slack, &pb_conflict_,
                      &pb_backjump_level);
    if (pb_backjump_level == -1) return SetModelUnsat();

    // Convert the conflict into the std::vector<LiteralWithCoeff> form.
    std::vector<LiteralWithCoeff> cst;
    pb_conflict_.CopyIntoVector(&cst);
    DCHECK(PBConstraintIsValidUnderDebugAssignment(cst, pb_conflict_.Rhs()));

    // Check if the learned PB conflict is just a clause:
    // all its coefficient must be 1, and the rhs must be its size minus 1.
    bool conflict_is_a_clause = (pb_conflict_.Rhs() == cst.size() - 1);
    if (conflict_is_a_clause) {
      for (LiteralWithCoeff term : cst) {
        if (term.coefficient != Coefficient(1)) {
          conflict_is_a_clause = false;
          break;
        }
      }
    }

    if (!conflict_is_a_clause) {
      // Use the PB conflict.
      // Note that we don't need to call InitializePropagators() since when we
      // are here, we are sure we have at least one pb constraint.
      DCHECK_GT(pb_constraints_.NumberOfConstraints(), 0);
      CHECK_LT(pb_backjump_level, CurrentDecisionLevel());
      Backtrack(pb_backjump_level);
      CHECK(pb_constraints_.AddLearnedConstraint(cst, pb_conflict_.Rhs(),
                                                 trail_));
      CHECK_GT(trail_->Index(), last_decision_or_backtrack_trail_index_);
      counters_.num_learned_pb_literals_ += cst.size();
      return false;
    }

    // Continue with the normal clause flow, but use the PB conflict clause
    // if it has a lower backjump level.
    if (pb_backjump_level < ComputeBacktrackLevel(learned_conflict_)) {
      subsumed_clauses_.clear();  // Because the conflict changes.
      learned_conflict_.clear();
      is_marked_.ClearAndResize(num_variables_);
      int max_level = 0;
      int max_index = 0;
      for (LiteralWithCoeff term : cst) {
        DCHECK(Assignment().LiteralIsTrue(term.literal));
        DCHECK_EQ(term.coefficient, 1);
        const int level = trail_->Info(term.literal.Variable()).level;
        if (level == 0) continue;
        if (level > max_level) {
          max_level = level;
          max_index = learned_conflict_.size();
        }
        learned_conflict_.push_back(term.literal.Negated());

        // The minimization functions below expect the conflict to be marked!
        // TODO(user): This is error prone, find a better way?
        is_marked_.Set(term.literal.Variable());
      }
      CHECK(!learned_conflict_.empty());
      std::swap(learned_conflict_.front(), learned_conflict_[max_index]);
      DCHECK(IsConflictValid(learned_conflict_));
    }
  }

  // Minimizing the conflict with binary clauses first has two advantages.
  // First, there is no need to compute a reason for the variables eliminated
  // this way. Second, more variables may be marked (in is_marked_) and
  // MinimizeConflict() can take advantage of that. Because of this, the
  // LBD of the learned conflict can change.
  DCHECK(ClauseIsValidUnderDebugAssignement(learned_conflict_));
  if (binary_implication_graph_.NumberOfImplications() != 0) {
    if (parameters_.binary_minimization_algorithm() ==
        SatParameters::BINARY_MINIMIZATION_FIRST) {
      binary_implication_graph_.MinimizeConflictFirst(
          *trail_, &learned_conflict_, &is_marked_);
    } else if (parameters_.binary_minimization_algorithm() ==
               SatParameters::
                   BINARY_MINIMIZATION_FIRST_WITH_TRANSITIVE_REDUCTION) {
      binary_implication_graph_.MinimizeConflictFirstWithTransitiveReduction(
          *trail_, &learned_conflict_, &is_marked_, &random_);
    }
    DCHECK(IsConflictValid(learned_conflict_));
  }

  // Minimize the learned conflict.
  MinimizeConflict(&learned_conflict_, &reason_used_to_infer_the_conflict_);

  // Minimize it further with binary clauses?
  if (binary_implication_graph_.NumberOfImplications() != 0) {
    // Note that on the contrary to the MinimizeConflict() above that
    // just uses the reason graph, this minimization can change the
    // clause LBD and even the backtracking level.
    switch (parameters_.binary_minimization_algorithm()) {
      case SatParameters::NO_BINARY_MINIMIZATION:
        FALLTHROUGH_INTENDED;
      case SatParameters::BINARY_MINIMIZATION_FIRST:
        FALLTHROUGH_INTENDED;
      case SatParameters::BINARY_MINIMIZATION_FIRST_WITH_TRANSITIVE_REDUCTION:
        break;
      case SatParameters::BINARY_MINIMIZATION_WITH_REACHABILITY:
        binary_implication_graph_.MinimizeConflictWithReachability(
            &learned_conflict_);
        break;
      case SatParameters::EXPERIMENTAL_BINARY_MINIMIZATION:
        binary_implication_graph_.MinimizeConflictExperimental(
            *trail_, &learned_conflict_);
        break;
    }
    DCHECK(IsConflictValid(learned_conflict_));
  }

  // Backtrack and add the reason to the set of learned clause.
  if (parameters_.use_erwa_heuristic()) {
    num_conflicts_stack_.push_back({trail_->Index(), 1});
  }
  counters_.num_literals_learned += learned_conflict_.size();
  Backtrack(ComputeBacktrackLevel(learned_conflict_));
  DCHECK(ClauseIsValidUnderDebugAssignement(learned_conflict_));

  // Detach any subsumed clause. They will actually be deleted on the next
  // clause cleanup phase.
  bool is_redundant = true;
  if (!subsumed_clauses_.empty() &&
      parameters_.subsumption_during_conflict_analysis()) {
    for (SatClause* clause : subsumed_clauses_) {
      DCHECK(ClauseSubsumption(learned_conflict_, clause));
      clauses_propagator_.LazyDetach(clause);
      if (!clause->IsRedundant()) is_redundant = false;
    }
    clauses_propagator_.CleanUpWatchers();
    counters_.num_subsumed_clauses += subsumed_clauses_.size();
  }

  // Create and attach the new learned clause.
  AddLearnedClauseAndEnqueueUnitPropagation(learned_conflict_, is_redundant);
  return false;
}

SatSolver::Status SatSolver::ReapplyDecisionsUpTo(
    int max_level, int* first_propagation_index) {
  SCOPED_TIME_STAT(&stats_);
  int decision_index = current_decision_level_;
  while (decision_index <= max_level) {
    DCHECK_GE(decision_index, current_decision_level_);
    const Literal previous_decision = decisions_[decision_index].literal;
    ++decision_index;
    if (Assignment().LiteralIsTrue(previous_decision)) continue;
    if (Assignment().LiteralIsFalse(previous_decision)) {
      // Update decision so that GetLastIncompatibleDecisions() works.
      decisions_[current_decision_level_].literal = previous_decision;
      return ASSUMPTIONS_UNSAT;
    }

    // Not assigned, we try to take it.
    const int old_level = current_decision_level_;
    const int index = EnqueueDecisionAndBackjumpOnConflict(previous_decision);
    *first_propagation_index = std::min(*first_propagation_index, index);
    if (index == kUnsatTrailIndex) return MODEL_UNSAT;
    if (current_decision_level_ <= old_level) {
      // A conflict occurred which backjumped to an earlier decision level.
      // We potentially backjumped over some valid decisions, so we need to
      // continue the loop and try to re-enqueue them.
      //
      // Note that there is no need to update max_level, because when we will
      // try to reapply the current "previous_decision" it will result in a
      // conflict. IMPORTANT: we can't actually optimize this and abort the loop
      // earlier though, because we need to check that it is conflicting because
      // it is already propagated to false. There is no guarantee of this
      // because we learn the first-UIP conflict. If it is not the case, we will
      // then learn a new conflict, backjump, and continue the loop.
      decision_index = current_decision_level_;
    }
  }
  return MODEL_SAT;
}

int SatSolver::EnqueueDecisionAndBacktrackOnConflict(Literal true_literal) {
  SCOPED_TIME_STAT(&stats_);
  CHECK(PropagationIsDone());

  if (is_model_unsat_) return kUnsatTrailIndex;
  decisions_[CurrentDecisionLevel()].literal = true_literal;
  int first_propagation_index = trail_->Index();
  ReapplyDecisionsUpTo(CurrentDecisionLevel(), &first_propagation_index);
  return first_propagation_index;
}

bool SatSolver::EnqueueDecisionIfNotConflicting(Literal true_literal) {
  SCOPED_TIME_STAT(&stats_);
  CHECK(PropagationIsDone());

  if (is_model_unsat_) return kUnsatTrailIndex;
  const int current_level = CurrentDecisionLevel();
  EnqueueNewDecision(true_literal);
  if (Propagate()) {
    return true;
  } else {
    Backtrack(current_level);
    return false;
  }
}

void SatSolver::Backtrack(int target_level) {
  SCOPED_TIME_STAT(&stats_);
  // TODO(user): The backtrack method should not be called when the model is
  //              unsat. Add a DCHECK to prevent that, but before fix the
  //              bop::BopOptimizerBase architecture.

  // Do nothing if the CurrentDecisionLevel() is already correct.
  // This is needed, otherwise target_trail_index below will remain at zero and
  // that will cause some problems. Note that we could forbid an user to call
  // Backtrack() with the current level, but that is annoying when you just
  // want to reset the solver with Backtrack(0).
  if (CurrentDecisionLevel() == target_level) return;
  DCHECK_GE(target_level, 0);
  DCHECK_LE(target_level, CurrentDecisionLevel());

  // Per the SatPropagator interface, this is needed before calling Untrail.
  trail_->SetDecisionLevel(target_level);

  int target_trail_index = 0;
  while (current_decision_level_ > target_level) {
    --current_decision_level_;
    target_trail_index = decisions_[current_decision_level_].trail_index;
  }
  Untrail(target_trail_index);
  last_decision_or_backtrack_trail_index_ = trail_->Index();
}

bool SatSolver::AddBinaryClauses(const std::vector<BinaryClause>& clauses) {
  SCOPED_TIME_STAT(&stats_);
  CHECK_EQ(CurrentDecisionLevel(), 0);
  for (BinaryClause c : clauses) {
    if (trail_->Assignment().LiteralIsFalse(c.a) &&
        trail_->Assignment().LiteralIsFalse(c.b)) {
      return SetModelUnsat();
    }
    AddBinaryClauseInternal(c.a, c.b);
  }
  if (!Propagate()) return SetModelUnsat();
  return true;
}

const std::vector<BinaryClause>& SatSolver::NewlyAddedBinaryClauses() {
  return binary_clauses_.newly_added();
}

void SatSolver::ClearNewlyAddedBinaryClauses() {
  binary_clauses_.ClearNewlyAdded();
}

namespace {
// Return the next value that is a multiple of interval.
int NextMultipleOf(int64 value, int64 interval) {
  return interval * (1 + value / interval);
}
}  // namespace

SatSolver::Status SatSolver::ResetAndSolveWithGivenAssumptions(
    const std::vector<Literal>& assumptions) {
  return ResetAndSolveWithGivenAssumptions(assumptions, nullptr);
}

SatSolver::Status SatSolver::ResetAndSolveWithGivenAssumptions(
    const std::vector<Literal>& assumptions, TimeLimit* time_limit) {
  SCOPED_TIME_STAT(&stats_);
  if (is_model_unsat_) return MODEL_UNSAT;
  CHECK_LE(assumptions.size(), num_variables_);
  Backtrack(0);
  assumption_level_ = assumptions.size();
  for (int i = 0; i < assumptions.size(); ++i) {
    decisions_[i].literal = assumptions[i];
  }
  return SolveInternal(time_limit == nullptr ? time_limit_.get() : time_limit);
}

SatSolver::Status SatSolver::StatusWithLog(Status status) {
  if (parameters_.log_search_progress()) {
    LOG(INFO) << RunningStatisticsString();
    LOG(INFO) << StatusString(status);
  }
  return status;
}

void SatSolver::SetAssumptionLevel(int assumption_level) {
  CHECK_GE(assumption_level, 0);
  CHECK_LE(assumption_level, CurrentDecisionLevel());
  assumption_level_ = assumption_level;
}

SatSolver::Status SatSolver::Solve() {
  return SolveInternal(time_limit_.get());
}

SatSolver::Status SatSolver::SolveInternal(TimeLimit* time_limit) {
  SCOPED_TIME_STAT(&stats_);
  if (is_model_unsat_) return MODEL_UNSAT;

  // TODO(user): Because the counter are not reset to zero, this cause the
  // metrics / sec to be completely broken except when the solver is used
  // for exactly one Solve().
  timer_.Restart();

  // This is done this way, so heuristics like the weighted_sign_ one can
  // wait for all the constraint to be added before beeing initialized.
  if (!decision_heuristic_is_initialized_) ResetDecisionHeuristic();

  // Display initial statistics.
  if (parameters_.log_search_progress()) {
    LOG(INFO) << "Initial memory usage: " << MemoryUsage();
    LOG(INFO) << "Number of variables: " << num_variables_;
    LOG(INFO) << "Number of clauses (size > 2): " << clauses_.size();
    LOG(INFO) << "Number of binary clauses: "
              << binary_implication_graph_.NumberOfImplications();
    LOG(INFO) << "Number of linear constraints: "
              << pb_constraints_.NumberOfConstraints();
    LOG(INFO) << "Number of fixed variables: " << trail_->Index();
    LOG(INFO) << "Number of watched clauses: "
              << clauses_propagator_.num_watched_clauses();
    LOG(INFO) << "Parameters: " << parameters_.ShortDebugString();
  }

  // Variables used to show the search progress.
  const int kDisplayFrequency = 10000;
  int next_display = parameters_.log_search_progress()
                         ? NextMultipleOf(num_failures(), kDisplayFrequency)
                         : std::numeric_limits<int>::max();

  // Variables used to check the memory limit every kMemoryCheckFrequency.
  const int kMemoryCheckFrequency = 10000;
  int next_memory_check = NextMultipleOf(num_failures(), kMemoryCheckFrequency);

  // The max_number_of_conflicts is per solve but the counter is for the whole
  // solver.
  const int64 kFailureLimit =
      parameters_.max_number_of_conflicts() == std::numeric_limits<int64>::max()
          ? std::numeric_limits<int64>::max()
          : counters_.num_failures + parameters_.max_number_of_conflicts();

  // Compute the repeated field of restart algorithms using the std::string default
  // if empty.
  auto restart_algorithms = parameters_.restart_algorithms();
  if (restart_algorithms.empty()) {
    SatParameters::RestartAlgorithm tmp;
    const std::vector<std::string> string_values = strings::Split(
        parameters_.default_restart_algorithms(), ',', strings::SkipEmpty());
    for (const std::string& string_value : string_values) {
      if (!SatParameters::RestartAlgorithm_Parse(string_value, &tmp)) {
        LOG(WARNING) << "Couldn't parse the RestartAlgorithm name: '"
                     << string_value << "'.";
      }
      restart_algorithms.Add(tmp);
    }
    if (restart_algorithms.empty()) {
      restart_algorithms.Add(SatParameters::NO_RESTART);
    }
  }

  // Starts search.
  for (;;) {
    // Test if a limit is reached.
    if (time_limit != nullptr) {
      const double current_deterministic_time = deterministic_time();
      time_limit->AdvanceDeterministicTime(
          current_deterministic_time -
          deterministic_time_at_last_advanced_time_limit_);
      deterministic_time_at_last_advanced_time_limit_ =
          current_deterministic_time;
      if (time_limit->LimitReached()) {
        if (parameters_.log_search_progress()) {
          LOG(INFO) << "The time limit has been reached. Aborting.";
        }
        return StatusWithLog(LIMIT_REACHED);
      }
    }
    if (num_failures() >= kFailureLimit) {
      if (parameters_.log_search_progress()) {
        LOG(INFO) << "The conflict limit has been reached. Aborting.";
      }
      return StatusWithLog(LIMIT_REACHED);
    }

    // The current memory checking takes time, so we only execute it every
    // kMemoryCheckFrequency conflict. We use >= because counters_.num_failures
    // may augment by more than one at each iteration.
    //
    // TODO(user): Find a better way.
    if (counters_.num_failures >= next_memory_check) {
      next_memory_check = NextMultipleOf(num_failures(), kMemoryCheckFrequency);
      if (IsMemoryLimitReached()) {
        if (parameters_.log_search_progress()) {
          LOG(INFO) << "The memory limit has been reached. Aborting.";
        }
        return StatusWithLog(LIMIT_REACHED);
      }
    }

    // Display search progression. We use >= because counters_.num_failures may
    // augment by more than one at each iteration.
    if (counters_.num_failures >= next_display) {
      LOG(INFO) << RunningStatisticsString();
      next_display = NextMultipleOf(num_failures(), kDisplayFrequency);
    }

    if (!PropagateAndStopAfterOneConflictResolution()) {
      // A conflict occurred, continue the loop.
      if (is_model_unsat_) return StatusWithLog(MODEL_UNSAT);
    } else {
      // We need to reapply any assumptions that are not currently applied.
      // Note that we do not count these as "branches" for a reporting purpose.
      if (CurrentDecisionLevel() < assumption_level_) {
        int unused = 0;
        const int64 old_num_branches = counters_.num_branches;
        const Status status =
            ReapplyDecisionsUpTo(assumption_level_ - 1, &unused);
        counters_.num_branches = old_num_branches;
        if (status != MODEL_SAT) return StatusWithLog(status);
        assumption_level_ = CurrentDecisionLevel();
      }

      // At a leaf?
      if (trail_->Index() == num_variables_.value()) {
        return StatusWithLog(MODEL_SAT);
      }

      // Restart?
      bool restart = false;
      switch (restart_algorithms.Get(strategy_counter_ %
                                     restart_algorithms.size())) {
        case SatParameters::NO_RESTART:
          break;
        case SatParameters::LUBY_RESTART:
          if (conflicts_until_next_restart_ == 0) {
            ++luby_count_;
            restart = true;
          }
          break;
        case SatParameters::DL_MOVING_AVERAGE_RESTART:
          if (dl_running_average_.IsWindowFull() &&
              dl_running_average_.GlobalAverage() <
                  parameters_.restart_dl_average_ratio() *
                      dl_running_average_.WindowAverage()) {
            restart = true;
          }
          break;
        case SatParameters::LBD_MOVING_AVERAGE_RESTART:
          if (lbd_running_average_.IsWindowFull() &&
              lbd_running_average_.GlobalAverage() <
                  parameters_.restart_lbd_average_ratio() *
                      lbd_running_average_.WindowAverage()) {
            restart = true;
          }
          break;
      }
      if (restart) {
        restart_count_++;
        Backtrack(assumption_level_);

        // Strategy switch?
        if (conflicts_until_next_strategy_change_ == 0) {
          strategy_counter_++;
          strategy_change_conflicts_ +=
              parameters_.strategy_change_increase_ratio() *
              strategy_change_conflicts_;
          conflicts_until_next_strategy_change_ = strategy_change_conflicts_;
        }

        // Reset the various restart strategies.
        dl_running_average_.ClearWindow();
        lbd_running_average_.ClearWindow();
        conflicts_until_next_restart_ =
            parameters_.luby_restart_period() * SUniv(luby_count_ + 1);
      }

      DCHECK_GE(CurrentDecisionLevel(), assumption_level_);
      EnqueueNewDecision(NextBranch());
    }
  }
}

SatSolver::Status SatSolver::SolveWithTimeLimit(TimeLimit* time_limit) {
  if (time_limit == nullptr) SolveInternal(time_limit_.get());
  deterministic_time_at_last_advanced_time_limit_ = deterministic_time();
  return SolveInternal(time_limit);
}

std::vector<Literal> SatSolver::GetLastIncompatibleDecisions() {
  SCOPED_TIME_STAT(&stats_);
  std::vector<Literal> unsat_assumptions;
  const Literal false_assumption = decisions_[CurrentDecisionLevel()].literal;
  DCHECK(trail_->Assignment().LiteralIsFalse(false_assumption));
  unsat_assumptions.push_back(false_assumption);

  // This will be used to mark all the literals inspected while we process the
  // false_assumption and the reasons behind each of its variable assignments.
  is_marked_.ClearAndResize(num_variables_);
  is_marked_.Set(false_assumption.Variable());

  int trail_index = trail_->Info(false_assumption.Variable()).trail_index;
  const int limit =
      CurrentDecisionLevel() > 0 ? decisions_[0].trail_index : trail_->Index();
  CHECK_LT(trail_index, trail_->Index());
  while (true) {
    // Find next marked literal to expand from the trail.
    while (trail_index >= 0 && !is_marked_[(*trail_)[trail_index].Variable()]) {
      --trail_index;
    }
    if (trail_index < limit) break;
    const Literal marked_literal = (*trail_)[trail_index];
    --trail_index;

    if (trail_->AssignmentType(marked_literal.Variable()) ==
        AssignmentType::kSearchDecision) {
      unsat_assumptions.push_back(marked_literal);
    } else {
      // Marks all the literals of its reason.
      for (const Literal literal : trail_->Reason(marked_literal.Variable())) {
        const BooleanVariable var = literal.Variable();
        const int level = DecisionLevel(var);
        if (level > 0 && !is_marked_[var]) is_marked_.Set(var);
      }
    }
  }

  // We reverse the assumptions so they are in the same order as the one in
  // which the decision where made.
  std::reverse(unsat_assumptions.begin(), unsat_assumptions.end());
  return unsat_assumptions;
}

void SatSolver::BumpVariableActivities(const std::vector<Literal>& literals,
                                       int bump_again_lbd_limit) {
  SCOPED_TIME_STAT(&stats_);
  if (parameters_.use_erwa_heuristic()) {
    for (const Literal literal : literals) {
      // Note that we don't really need to bump level 0 variables since they
      // will never be backtracked over. However it is faster to simply bump
      // them.
      ++num_bumps_[literal.Variable()];
    }
    return;
  }

  const double max_activity_value = parameters_.max_variable_activity_value();
  for (const Literal literal : literals) {
    const BooleanVariable var = literal.Variable();
    const int level = DecisionLevel(var);
    if (level == 0) continue;
    if (level == CurrentDecisionLevel() && bump_again_lbd_limit > 0) {
      SatClause* clause = ReasonClauseOrNull(var);
      if (clause != nullptr && clause->IsRedundant() &&
          FindWithDefault(clauses_info_, clause, ClauseInfo()).lbd <
              bump_again_lbd_limit) {
        activities_[var] += variable_activity_increment_;
      }
    }
    activities_[var] += variable_activity_increment_;
    pq_need_update_for_var_at_trail_index_.Set(trail_->Info(var).trail_index);
    if (activities_[var] > max_activity_value) {
      RescaleVariableActivities(1.0 / max_activity_value);
    }
  }
}

void SatSolver::BumpReasonActivities(const std::vector<Literal>& literals) {
  SCOPED_TIME_STAT(&stats_);
  for (const Literal literal : literals) {
    const BooleanVariable var = literal.Variable();
    if (DecisionLevel(var) > 0) {
      SatClause* clause = ReasonClauseOrNull(var);
      if (clause != nullptr) {
        BumpClauseActivity(clause);
      } else {
        UpperBoundedLinearConstraint* pb_constraint =
            ReasonPbConstraintOrNull(var);
        if (pb_constraint != nullptr) {
          // TODO(user): Because one pb constraint may propagate many literals,
          // this may bias the constraint activity... investigate other policy.
          pb_constraints_.BumpActivity(pb_constraint);
        }
      }
    }
  }
}

void SatSolver::BumpClauseActivity(SatClause* clause) {
  if (!clause->IsRedundant()) return;

  // We only bump the activity of the clauses that have some info. So if we know
  // that we will keep a clause forever, we don't need to create its Info. More
  // than the speed, this allows to limit as much as possible the activity
  // rescaling.
  auto it = clauses_info_.find(clause);
  if (it == clauses_info_.end()) return;

  // Check if the new clause LBD is below our threshold to keep this clause
  // indefinitely. Note that we use a +1 here because the LBD of a newly learned
  // clause decrease by 1 just after the backjump.
  const int new_lbd = ComputeLbd(*clause);
  if (new_lbd + 1 <= parameters_.clause_cleanup_lbd_bound()) {
    clauses_info_.erase(clause);
    return;
  }

  // Eventually protect this clause for the next cleanup phase.
  switch (parameters_.clause_cleanup_protection()) {
    case SatParameters::PROTECTION_NONE:
      break;
    case SatParameters::PROTECTION_ALWAYS:
      it->second.protected_during_next_cleanup = true;
      break;
    case SatParameters::PROTECTION_LBD:
      // This one is similar to the one used by the Glucose SAT solver.
      //
      // TODO(user): why the +1? one reason may be that the LBD of a conflict
      // decrease by 1 just afer the backjump...
      if (new_lbd + 1 < it->second.lbd) {
        it->second.protected_during_next_cleanup = true;
        it->second.lbd = new_lbd;
      }
  }

  // Increase the activity.
  const double activity = it->second.activity += clause_activity_increment_;
  if (activity > parameters_.max_clause_activity_value()) {
    RescaleClauseActivities(1.0 / parameters_.max_clause_activity_value());
  }
}

void SatSolver::RescaleVariableActivities(double scaling_factor) {
  SCOPED_TIME_STAT(&stats_);
  variable_activity_increment_ *= scaling_factor;
  for (BooleanVariable var(0); var < activities_.size(); ++var) {
    activities_[var] *= scaling_factor;
  }

  // When rescaling the activities of all the variables, the order of the
  // active variables in the heap will not change, but we still need to update
  // their weights so that newly inserted elements will compare correctly with
  // already inserted ones.
  //
  // IMPORTANT: we need to reset the full heap from scratch because just
  // multiplying the current weight by scaling_factor is not guaranteed to
  // preserve the order. This is because the activity of two entries may go to
  // zero and the tie-breaking ordering may change their relative order.
  //
  // InitializeVariableOrdering() will be called lazily only if needed.
  var_ordering_is_initialized_ = false;
}

void SatSolver::RescaleClauseActivities(double scaling_factor) {
  SCOPED_TIME_STAT(&stats_);
  clause_activity_increment_ *= scaling_factor;
  for (auto& entry : clauses_info_) {
    entry.second.activity *= scaling_factor;
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
  const int highest_level = DecisionLevel(literals[0].Variable());
  for (int i = 1; i < literals.size(); ++i) {
    const int level = DecisionLevel(literals[i].Variable());
    if (level <= 0 || level >= highest_level) return false;
  }
  return true;
}

int SatSolver::ComputeBacktrackLevel(const std::vector<Literal>& literals) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK_GT(CurrentDecisionLevel(), 0);

  // We want the highest decision level among literals other than the first one.
  // Note that this level will always be smaller than that of the first literal.
  //
  // Note(user): if the learned clause is of size 1, we backtrack all the way to
  // the beginning. It may be possible to follow another behavior, but then the
  // code require some special cases in
  // AddLearnedClauseAndEnqueueUnitPropagation() to fix the literal and not
  // backtrack over it. Also, subsequent propagated variables may not have a
  // correct level in this case.
  int backtrack_level = 0;
  for (int i = 1; i < literals.size(); ++i) {
    const int level = DecisionLevel(literals[i].Variable());
    backtrack_level = std::max(backtrack_level, level);
  }
  VLOG(2) << Indent() << "backtrack_level: " << backtrack_level;
  DCHECK_LT(backtrack_level, DecisionLevel(literals[0].Variable()));
  DCHECK_LE(DecisionLevel(literals[0].Variable()), CurrentDecisionLevel());
  return backtrack_level;
}

template <typename LiteralList>
int SatSolver::ComputeLbd(const LiteralList& conflict) {
  SCOPED_TIME_STAT(&stats_);
  const int limit =
      parameters_.count_assumption_levels_in_lbd() ? 0 : assumption_level_;

  // We know that the first literal of the conflict is always of the highest
  // level.
  is_level_marked_.ClearAndResize(
      SatDecisionLevel(DecisionLevel(conflict.begin()->Variable()) + 1));
  for (const Literal literal : conflict) {
    const SatDecisionLevel level(DecisionLevel(literal.Variable()));
    DCHECK_GE(level, 0);
    if (level > limit && !is_level_marked_[level]) {
      is_level_marked_.Set(level);
    }
  }
  return is_level_marked_.NumberOfSetCallsWithDifferentArguments();
}

std::string SatSolver::StatusString(Status status) const {
  const double time_in_s = timer_.Get();
  return StringPrintf("\n  status: %s\n", SatStatusString(status).c_str()) +
         StringPrintf("  time: %fs\n", time_in_s) +
         StringPrintf("  memory: %s\n", MemoryUsage().c_str()) +
         StringPrintf("  num failures: %" GG_LL_FORMAT "d  (%.0f /sec)\n",
                      counters_.num_failures,
                      static_cast<double>(counters_.num_failures) / time_in_s) +
         StringPrintf("  num branches: %" GG_LL_FORMAT
                      "d"
                      "  (%.2f%% random) (%.0f /sec)\n",
                      counters_.num_branches,
                      100.0 *
                          static_cast<double>(counters_.num_random_branches) /
                          counters_.num_branches,
                      static_cast<double>(counters_.num_branches) / time_in_s) +
         StringPrintf("  num propagations: %" GG_LL_FORMAT "d  (%.0f /sec)\n",
                      num_propagations(),
                      static_cast<double>(num_propagations()) / time_in_s) +
         StringPrintf("  num binary propagations: %" GG_LL_FORMAT "d\n",
                      binary_implication_graph_.num_propagations()) +
         StringPrintf("  num binary inspections: %" GG_LL_FORMAT "d\n",
                      binary_implication_graph_.num_inspections()) +
         StringPrintf("  num binary redundant implications: %" GG_LL_FORMAT
                      "d\n",
                      binary_implication_graph_.num_redundant_implications()) +
         StringPrintf("  num classic minimizations: %" GG_LL_FORMAT
                      "d"
                      "  (literals removed: %" GG_LL_FORMAT "d)\n",
                      counters_.num_minimizations,
                      counters_.num_literals_removed) +
         StringPrintf("  num binary minimizations: %" GG_LL_FORMAT
                      "d"
                      "  (literals removed: %" GG_LL_FORMAT "d)\n",
                      binary_implication_graph_.num_minimization(),
                      binary_implication_graph_.num_literals_removed()) +
         StringPrintf("  num inspected clauses: %" GG_LL_FORMAT "d\n",
                      clauses_propagator_.num_inspected_clauses()) +
         StringPrintf("  num inspected clause_literals: %" GG_LL_FORMAT "d\n",
                      clauses_propagator_.num_inspected_clause_literals()) +
         StringPrintf(
             "  num learned literals: %lld  (avg: %.1f /clause)\n",
             counters_.num_literals_learned,
             1.0 * counters_.num_literals_learned / counters_.num_failures) +
         StringPrintf("  num learned PB literals: %lld  (avg: %.1f /clause)\n",
                      counters_.num_learned_pb_literals_,
                      1.0 * counters_.num_learned_pb_literals_ /
                          counters_.num_failures) +
         StringPrintf("  num subsumed clauses: %lld\n",
                      counters_.num_subsumed_clauses) +
         StringPrintf("  num restarts: %d\n", restart_count_) +
         StringPrintf("  pb num threshold updates: %lld\n",
                      pb_constraints_.num_threshold_updates()) +
         StringPrintf("  pb num constraint lookups: %lld\n",
                      pb_constraints_.num_constraint_lookups()) +
         StringPrintf("  pb num inspected constraint literals: %lld\n",
                      pb_constraints_.num_inspected_constraint_literals()) +
         StringPrintf("  conflict decision level avg: %f\n",
                      dl_running_average_.GlobalAverage()) +
         StringPrintf("  conflict lbd avg: %f\n",
                      lbd_running_average_.GlobalAverage()) +
         StringPrintf("  conflict trail size avg: %f\n",
                      trail_size_running_average_.GlobalAverage()) +
         StringPrintf("  deterministic time: %f\n", deterministic_time());
}

std::string SatSolver::RunningStatisticsString() const {
  const double time_in_s = timer_.Get();
  return StringPrintf(
      "%6.2lfs, mem:%s, fails:%" GG_LL_FORMAT
      "d, "
      "depth:%d, clauses:%lu, tmp:%lu, bin:%llu, restarts:%d, vars:%d",
      time_in_s, MemoryUsage().c_str(), counters_.num_failures,
      CurrentDecisionLevel(), clauses_.size() - clauses_info_.size(),
      clauses_info_.size(), binary_implication_graph_.NumberOfImplications(),
      restart_count_, num_variables_.value() - num_processed_fixed_variables_);
}

void SatSolver::ProcessNewlyFixedVariables() {
  SCOPED_TIME_STAT(&stats_);
  DCHECK_EQ(CurrentDecisionLevel(), 0);
  int num_detached_clauses = 0;
  int num_binary = 0;

  // We remove the clauses that are always true and the fixed literals from the
  // others.
  for (SatClause* clause : clauses_) {
    if (clause->IsAttached()) {
      const size_t old_size = clause->Size();
      if (clause->RemoveFixedLiteralsAndTestIfTrue(trail_->Assignment())) {
        // The clause is always true, detach it.
        clauses_propagator_.LazyDetach(clause);
        ++num_detached_clauses;
      } else if (clause->Size() != old_size) {
        if (clause->Size() == 2 &&
            parameters_.treat_binary_clauses_separately()) {
          // This clause is now a binary clause, treat it separately. Note that
          // it is safe to do that because this clause can't be used as a reason
          // since we are at level zero and the clause is not satisfied.
          AddBinaryClauseInternal(clause->FirstLiteral(),
                                  clause->SecondLiteral());
          clauses_propagator_.LazyDetach(clause);
          ++num_binary;
        }
      }

      const size_t new_size = clause->Size();
      if (new_size != old_size && drat_writer_ != nullptr) {
        // TODO(user): Instead delete the original clause in
        // DeleteDetachedClause(). The problem is that we currently don't have
        // the initial size anywhere.
        drat_writer_->AddClause({clause->begin(), new_size});
        drat_writer_->DeleteClause(
            {clause->begin(), old_size},
            /*ignore_call=*/clauses_info_.find(clause) == clauses_info_.end());
      }
    }
  }

  // Note that we will only delete the clauses during the next database cleanup.
  clauses_propagator_.CleanUpWatchers();
  if (num_detached_clauses > 0 || num_binary > 0) {
    VLOG(1) << trail_->Index() << " fixed variables at level 0. "
            << "Detached " << num_detached_clauses << " clauses. " << num_binary
            << " converted to binary.";
  }

  // We also clean the binary implication graph.
  binary_implication_graph_.RemoveFixedVariables(num_processed_fixed_variables_,
                                                 *trail_);
  num_processed_fixed_variables_ = trail_->Index();
  deterministic_time_of_last_fixed_variables_cleanup_ = deterministic_time();
}

bool SatSolver::Propagate() {
  SCOPED_TIME_STAT(&stats_);
  while (true) {
    // The idea here is to abort the inspection as soon as at least one
    // propagation occurs so we can loop over and test again the highest
    // priority constraint types using the new information.
    //
    // Note that the first propagators_ should be the binary_implication_graph_
    // and that its Propagate() functions will not abort on the first
    // propagation to be slightly more efficient.
    const int old_index = trail_->Index();
    for (SatPropagator* propagator : propagators_) {
      DCHECK(propagator->PropagatePreconditionsAreSatisfied(*trail_));
      if (!propagator->Propagate(trail_)) return false;
      if (trail_->Index() > old_index) break;
    }
    if (trail_->Index() == old_index) break;
  }
  return true;
}

void SatSolver::InitializePropagators() {
  propagators_.clear();

  // To make Propagate() as fast as possible, we only add the
  // binary_implication_graph_/pb_constraints_ propagators if there is anything
  // to propagate. Because of this, it is important to call
  // InitializePropagators() after the first constraint of this kind is added.
  //
  // TODO(user): uses the Model classes here to only call
  // model.GetOrCreate<BinaryImplicationGraph>() when the first binary
  // constraint is needed, and have a mecanism to always make this propagator
  // first. Same for the linear constraints.
  if (binary_implication_graph_.NumberOfImplications() > 0) {
    propagators_.push_back(&binary_implication_graph_);
  }
  propagators_.push_back(&clauses_propagator_);
  if (pb_constraints_.NumberOfConstraints() > 0) {
    propagators_.push_back(&pb_constraints_);
  }
  for (int i = 0; i < external_propagators_.size(); ++i) {
    propagators_.push_back(external_propagators_[i]);
  }
  if (last_propagator_ != nullptr) {
    propagators_.push_back(last_propagator_);
  }
}

bool SatSolver::PropagationIsDone() const {
  for (SatPropagator* propagator : propagators_) {
    if (!propagator->PropagationIsDone(*trail_)) return false;
  }
  return true;
}

bool SatSolver::ResolvePBConflict(BooleanVariable var,
                                  MutableUpperBoundedLinearConstraint* conflict,
                                  Coefficient* slack) {
  const int trail_index = trail_->Info(var).trail_index;

  // This is the slack of the conflict < trail_index
  DCHECK_EQ(*slack, conflict->ComputeSlackForTrailPrefix(*trail_, trail_index));

  // Pseudo-Boolean case.
  UpperBoundedLinearConstraint* pb_reason = ReasonPbConstraintOrNull(var);
  if (pb_reason != nullptr) {
    pb_reason->ResolvePBConflict(*trail_, var, conflict, slack);
    return false;
  }

  // Generic clause case.
  Coefficient multiplier(1);

  // TODO(user): experiment and choose the "best" algo.
  const int algorithm = 1;
  switch (algorithm) {
    case 1:
      // We reduce the conflict slack to 0 before adding the clause.
      // The advantage of this method is that the coefficients stay small.
      conflict->ReduceSlackTo(*trail_, trail_index, *slack, Coefficient(0));
      break;
    case 2:
      // No reduction, we add the lower possible multiple.
      multiplier = *slack + 1;
      break;
    default:
      // No reduction, the multiple is chosen to cancel var.
      multiplier = conflict->GetCoefficient(var);
  }

  Coefficient num_literals(1);
  conflict->AddTerm(
      trail_->Assignment().GetTrueLiteralForAssignedVariable(var).Negated(),
      multiplier);
  for (Literal literal : trail_->Reason(var)) {
    DCHECK_NE(literal.Variable(), var);
    DCHECK(Assignment().LiteralIsFalse(literal));
    conflict->AddTerm(literal.Negated(), multiplier);
    ++num_literals;
  }
  conflict->AddToRhs((num_literals - 1) * multiplier);

  // All the algorithms above result in a new slack of -1.
  *slack = -1;
  DCHECK_EQ(*slack, conflict->ComputeSlackForTrailPrefix(*trail_, trail_index));
  return true;
}

void SatSolver::EnqueueNewDecision(Literal literal) {
  SCOPED_TIME_STAT(&stats_);
  CHECK(!Assignment().VariableIsAssigned(literal.Variable()));

  // We are back at level 0. This can happen because of a restart, or because
  // we proved that some variables must take a given value in any satisfiable
  // assignment. Trigger a simplification of the clauses if there is new fixed
  // variables. Note that for efficiency reason, we don't do that too often.
  //
  // TODO(user): Do more advanced preprocessing?
  if (CurrentDecisionLevel() == 0) {
    const double kMinDeterministicTimeBetweenCleanups = 1.0;
    if (num_processed_fixed_variables_ < trail_->Index() &&
        deterministic_time() >
            deterministic_time_of_last_fixed_variables_cleanup_ +
                kMinDeterministicTimeBetweenCleanups) {
      ProcessNewlyFixedVariables();
    }
  }

  counters_.num_branches++;
  last_decision_or_backtrack_trail_index_ = trail_->Index();
  decisions_[current_decision_level_] = Decision(trail_->Index(), literal);
  ++current_decision_level_;
  trail_->SetDecisionLevel(current_decision_level_);
  trail_->EnqueueSearchDecision(literal);
}

Literal SatSolver::NextBranch() {
  SCOPED_TIME_STAT(&stats_);

  // Lazily initialize var_ordering_ if needed.
  if (!var_ordering_is_initialized_) {
    InitializeVariableOrdering();
  }

  // Choose the variable.
  BooleanVariable var;
  const double ratio = parameters_.random_branches_ratio();
  auto zero_to_one = [this]() {
    return std::uniform_real_distribution<double>()(random_);
  };
  if (ratio != 0.0 && zero_to_one() < ratio) {
    ++counters_.num_random_branches;
    while (true) {
      // TODO(user): This may not be super efficient if almost all the
      // variables are assigned.
      std::uniform_int_distribution<int> index_dist(
          0, var_ordering_.Raw()->size() - 1);
      var = BooleanVariable((*var_ordering_.Raw())[index_dist(random_)] -
                            &queue_elements_.front());
      if (!trail_->Assignment().VariableIsAssigned(var)) break;
      pq_need_update_for_var_at_trail_index_.Set(trail_->Info(var).trail_index);
      var_ordering_.Remove(&queue_elements_[var]);
    }
  } else {
    // The loop is done this way in order to leave the final choice in the heap.
    DCHECK(!var_ordering_.IsEmpty());
    var = BooleanVariable(var_ordering_.Top() - &queue_elements_.front());
    while (trail_->Assignment().VariableIsAssigned(var)) {
      var_ordering_.Pop();
      pq_need_update_for_var_at_trail_index_.Set(trail_->Info(var).trail_index);
      DCHECK(!var_ordering_.IsEmpty());
      var = BooleanVariable(var_ordering_.Top() - &queue_elements_.front());
    }
  }

  // Choose its polarity (i.e. True of False).
  const double random_ratio = parameters_.random_polarity_ratio();
  if (random_ratio != 0.0 && zero_to_one() < random_ratio) {
    return Literal(var, std::uniform_int_distribution<int>(0, 1)(random_));
  }
  return Literal(var, var_use_phase_saving_[var]
                          ? trail_->Info(var).last_polarity
                          : var_polarity_[var]);
}

void SatSolver::ResetPolarity(BooleanVariable from) {
  SCOPED_TIME_STAT(&stats_);
  const int size = num_variables_.value();
  var_use_phase_saving_.resize(size, parameters_.use_phase_saving());
  var_polarity_.resize(size);
  for (BooleanVariable var(from); var < size; ++var) {
    bool initial_polarity;
    switch (parameters_.initial_polarity()) {
      case SatParameters::POLARITY_TRUE:
        initial_polarity = true;
        break;
      case SatParameters::POLARITY_FALSE:
        initial_polarity = false;
        break;
      case SatParameters::POLARITY_RANDOM:
        initial_polarity = std::uniform_int_distribution<int>(0, 1)(random_);
        break;
      case SatParameters::POLARITY_WEIGHTED_SIGN:
        initial_polarity = weighted_sign_[var] > 0;
        break;
      case SatParameters::POLARITY_REVERSE_WEIGHTED_SIGN:
        initial_polarity = weighted_sign_[var] < 0;
        break;
    }
    var_polarity_[var] = initial_polarity;
    trail_->SetLastPolarity(var, initial_polarity);
  }
}

void SatSolver::InitializeVariableOrdering() {
  SCOPED_TIME_STAT(&stats_);
  var_ordering_.Clear();
  pq_need_update_for_var_at_trail_index_.ClearAndResize(num_variables_.value());

  // First, extract the variables without activity, and add the other to the
  // priority queue.
  std::vector<BooleanVariable> variables;
  for (BooleanVariable var(0); var < num_variables_; ++var) {
    if (!trail_->Assignment().VariableIsAssigned(var)) {
      if (activities_[var] > 0) {
        queue_elements_[var].weight = activities_[var];
        var_ordering_.Add(&queue_elements_[var]);
      } else {
        variables.push_back(var);
      }
    }
  }

  // Set the order of the other according to the parameters_.
  // Note that this is just a "preference" since the priority queue will kind
  // of randomize this. However, it is more efficient than using the tie_breaker
  // which add a big overhead on the priority queue.
  //
  // TODO(user): Experiment and come up with a good set of heuristics.
  switch (parameters_.preferred_variable_order()) {
    case SatParameters::IN_ORDER:
      break;
    case SatParameters::IN_REVERSE_ORDER:
      std::reverse(variables.begin(), variables.end());
      break;
    case SatParameters::IN_RANDOM_ORDER:
      std::shuffle(variables.begin(), variables.end(), random_);
      break;
  }

  // Add the variables without activity to the queue (in the default order)
  for (BooleanVariable var : variables) {
    queue_elements_[var].weight = 0.0;
    var_ordering_.Add(&queue_elements_[var]);
  }

  // Finish the queue initialization.
  for (int i = 0; i < trail_->Index(); ++i) {
    pq_need_update_for_var_at_trail_index_.Set(i);
  }
  var_ordering_is_initialized_ = true;
}

void SatSolver::SetAssignmentPreference(Literal literal, double weight) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(!is_model_unsat_);
  if (!decision_heuristic_is_initialized_) ResetDecisionHeuristic();
  if (!parameters_.use_optimization_hints()) return;
  DCHECK_GE(weight, 0.0);
  DCHECK_LE(weight, 1.0);

  var_use_phase_saving_[literal.Variable()] = false;
  var_polarity_[literal.Variable()] = literal.IsPositive();

  // The tie_breaker is changed, so we need to reinitialize the priority queue.
  // Note that this doesn't change the activity though.
  queue_elements_[literal.Variable()].tie_breaker = weight;
  var_ordering_is_initialized_ = false;
}

std::vector<std::pair<Literal, double>> SatSolver::AllPreferences() const {
  std::vector<std::pair<Literal, double>> prefs;
  for (BooleanVariable var(0); var < var_polarity_.size(); ++var) {
    // TODO(user): we currently assume that if the tie_breaker is zero then
    // no preference was set (which is not 100% correct). Fix that.
    if (queue_elements_[var].tie_breaker > 0.0) {
      prefs.push_back(std::make_pair(Literal(var, var_polarity_[var]),
                                     queue_elements_[var].tie_breaker));
    }
  }
  return prefs;
}

void SatSolver::ResetDecisionHeuristic() {
  DCHECK(!is_model_unsat_);

  // Note that this will never be false again.
  decision_heuristic_is_initialized_ = true;

  // Reset the polarity heurisitic.
  ResetPolarity(/*from=*/BooleanVariable(0));

  // Reset the branching variable heuristic.
  activities_.assign(num_variables_.value(),
                     parameters_.initial_variables_activity());
  num_bumps_.assign(num_variables_.value(), 0);
  var_ordering_is_initialized_ = false;

  // Reset the tie breaking.
  for (BooleanVariable var(0); var < num_variables_; ++var) {
    queue_elements_[var].tie_breaker = 0.0;
  }
}

void SatSolver::ResetDecisionHeuristicAndSetAllPreferences(
    const std::vector<std::pair<Literal, double>>& prefs) {
  ResetDecisionHeuristic();
  for (const std::pair<Literal, double> p : prefs) {
    SetAssignmentPreference(p.first, p.second);
  }
}

void SatSolver::Untrail(int target_trail_index) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK_LT(target_trail_index, trail_->Index());

  // Untrail the propagators.
  for (SatPropagator* propagator : propagators_) {
    propagator->Untrail(*trail_, target_trail_index);
  }

  // Trail index of the next variable that will need a priority queue update.
  int to_update = var_ordering_is_initialized_
                      ? pq_need_update_for_var_at_trail_index_.Top()
                      : -1;
  DCHECK_LT(to_update, trail_->Index());

  // The ERWA parameter between the new estimation of the learning rate and the
  // old one. TODO(user): Expose parameters for these values. Note that
  // num_failures() count the number of failures since the solver creation.
  const double alpha = std::max(0.06, 0.4 - 1e-6 * num_failures());

  // This counts the number of conflicts since the assignment of the variable at
  // the current trail_index that we are about to untrail.
  int num_conflicts = 0;
  int next_num_conflicts_update = num_conflicts_stack_.empty()
                                      ? -1
                                      : num_conflicts_stack_.back().trail_index;

  // Note(user): Depending on the value of use_erwa_heuristic(), we could
  // optimize a bit more this loop, but the extra tests didn't seems to change
  // the run time that much.
  while (trail_->Index() > target_trail_index) {
    if (next_num_conflicts_update == trail_->Index()) {
      num_conflicts += num_conflicts_stack_.back().count;
      num_conflicts_stack_.pop_back();
      next_num_conflicts_update = num_conflicts_stack_.empty()
                                      ? -1
                                      : num_conflicts_stack_.back().trail_index;
    }
    const BooleanVariable var = trail_->Dequeue().Variable();
    DCHECK_EQ(trail_->Index(), trail_->Info(var).trail_index);

    bool update_pq = false;
    if (parameters_.use_erwa_heuristic()) {
      // TODO(user): This heuristic can make this code quite slow because
      // all the untrailed variable will cause a priority queue update.
      const int64 num_bumps = num_bumps_[var];
      double new_rate = 0.0;
      if (num_bumps > 0) {
        DCHECK_GT(num_conflicts, 0);
        num_bumps_[var] = 0;
        new_rate = static_cast<double>(num_bumps) / num_conflicts;
      }
      activities_[var] = alpha * new_rate + (1 - alpha) * activities_[var];
      update_pq = true;
    } else {
      if (trail_->Index() == to_update) {
        pq_need_update_for_var_at_trail_index_.ClearTop();
        to_update = pq_need_update_for_var_at_trail_index_.Top();
        update_pq = true;
      }
    }

    // Update the priority queue if needed. Note that the to_update logic is
    // just here for optimization and that the code works without it.
    if (update_pq) {
      WeightedVarQueueElement* element = &(queue_elements_[var]);
      if (var_ordering_.Contains(element)) {
        // Note that because of the pq_need_update_for_var_at_trail_index_
        // optimization the new weight should always be higher than the old one.
        DCHECK_GT(activities_[var], element->weight);
        element->weight = activities_[var];
        var_ordering_.NoteChangedPriority(element);
      } else {
        element->weight = activities_[var];
        var_ordering_.Add(element);
      }
    } else if (DEBUG_MODE && var_ordering_is_initialized_) {
      DCHECK(var_ordering_.Contains(&(queue_elements_[var])));
      DCHECK_EQ(activities_[var], queue_elements_[var].weight);
    }
  }
  if (num_conflicts > 0) {
    if (!num_conflicts_stack_.empty() &&
        num_conflicts_stack_.back().trail_index == trail_->Index()) {
      num_conflicts_stack_.back().count += num_conflicts;
    } else {
      num_conflicts_stack_.push_back({trail_->Index(), num_conflicts});
    }
  }
}

std::string SatSolver::DebugString(const SatClause& clause) const {
  std::string result;
  for (const Literal literal : clause) {
    if (!result.empty()) {
      result.append(" || ");
    }
    const std::string value =
        trail_->Assignment().LiteralIsTrue(literal)
            ? "true"
            : (trail_->Assignment().LiteralIsFalse(literal) ? "false"
                                                            : "undef");
    result.append(
        StringPrintf("%s(%s)", literal.DebugString().c_str(), value.c_str()));
  }
  return result;
}

int SatSolver::ComputeMaxTrailIndex(gtl::Span<Literal> clause) const {
  SCOPED_TIME_STAT(&stats_);
  int trail_index = -1;
  for (const Literal literal : clause) {
    trail_index =
        std::max(trail_index, trail_->Info(literal.Variable()).trail_index);
  }
  return trail_index;
}

// This method will compute a first UIP conflict
//   http://www.cs.tau.ac.il/~msagiv/courses/ATP/iccad2001_final.pdf
//   http://gauss.ececs.uc.edu/SAT/articles/FAIA185-0131.pdf
void SatSolver::ComputeFirstUIPConflict(
    int max_trail_index, std::vector<Literal>* conflict,
    std::vector<Literal>* reason_used_to_infer_the_conflict,
    std::vector<SatClause*>* subsumed_clauses) {
  SCOPED_TIME_STAT(&stats_);

  // This will be used to mark all the literals inspected while we process the
  // conflict and the reasons behind each of its variable assignments.
  is_marked_.ClearAndResize(num_variables_);

  conflict->clear();
  reason_used_to_infer_the_conflict->clear();
  subsumed_clauses->clear();
  if (max_trail_index == -1) return;

  // max_trail_index is the maximum trail index appearing in the failing_clause
  // and its level (Which is almost always equals to the CurrentDecisionLevel(),
  // except for symmetry propagation).
  DCHECK_EQ(max_trail_index, ComputeMaxTrailIndex(trail_->FailingClause()));
  int trail_index = max_trail_index;
  const int highest_level = DecisionLevel((*trail_)[trail_index].Variable());
  if (highest_level == 0) return;

  // To find the 1-UIP conflict clause, we start by the failing_clause, and
  // expand each of its literal using the reason for this literal assignement to
  // false. The is_marked_ set allow us to never expand the same literal twice.
  //
  // The expansion is not done (i.e. stop) for literal that where assigned at a
  // decision level below the current one. If the level of such literal is not
  // zero, it is added to the conflict clause.
  //
  // Now, the trick is that we use the trail to expand the literal of the
  // current level in a very specific order. Namely the reverse order of the one
  // in which they where infered. We stop as soon as
  // num_literal_at_highest_level_that_needs_to_be_processed is exactly one.
  //
  // This last literal will be the first UIP because by definition all the
  // propagation done at the current level will pass though it at some point.
  gtl::Span<Literal> clause_to_expand = trail_->FailingClause();
  SatClause* sat_clause = trail_->FailingSatClause();
  DCHECK(!clause_to_expand.empty());
  int num_literal_at_highest_level_that_needs_to_be_processed = 0;
  while (true) {
    int num_new_vars_at_positive_level = 0;
    int num_vars_at_positive_level_in_clause_to_expand = 0;
    for (const Literal literal : clause_to_expand) {
      const BooleanVariable var = literal.Variable();
      const int level = DecisionLevel(var);
      if (level > 0) ++num_vars_at_positive_level_in_clause_to_expand;
      if (!is_marked_[var]) {
        is_marked_.Set(var);
        if (level == highest_level) {
          ++num_new_vars_at_positive_level;
          ++num_literal_at_highest_level_that_needs_to_be_processed;
        } else if (level > 0) {
          ++num_new_vars_at_positive_level;
          // Note that all these literals are currently false since the clause
          // to expand was used to infer the value of a literal at this level.
          DCHECK(trail_->Assignment().LiteralIsFalse(literal));
          conflict->push_back(literal);
        } else {
          reason_used_to_infer_the_conflict->push_back(literal);
        }
      }
    }

    // If there is new variables, then all the previously subsumed clauses are
    // not subsumed anymore.
    if (num_new_vars_at_positive_level > 0) {
      // TODO(user): We could still replace all these clauses with the current
      // conflict.
      subsumed_clauses->clear();
    }

    // This check if the new conflict is exactly equal to clause_to_expand.
    // Since we just performed an union, comparing the size is enough. When this
    // is true, then the current conflict subsumes the reason whose underlying
    // clause is given by sat_clause.
    if (sat_clause != nullptr &&
        num_vars_at_positive_level_in_clause_to_expand ==
            conflict->size() +
                num_literal_at_highest_level_that_needs_to_be_processed) {
      subsumed_clauses->push_back(sat_clause);
    }

    // Find next marked literal to expand from the trail.
    DCHECK_GT(num_literal_at_highest_level_that_needs_to_be_processed, 0);
    while (!is_marked_[(*trail_)[trail_index].Variable()]) {
      --trail_index;
      DCHECK_GE(trail_index, 0);
      DCHECK_EQ(DecisionLevel((*trail_)[trail_index].Variable()),
                highest_level);
    }

    if (num_literal_at_highest_level_that_needs_to_be_processed == 1) {
      // We have the first UIP. Add its negation to the conflict clause.
      // This way, after backtracking to the proper level, the conflict clause
      // will be unit, and infer the negation of the UIP that caused the fail.
      conflict->push_back((*trail_)[trail_index].Negated());

      // To respect the function API move the first UIP in the first position.
      std::swap(conflict->back(), conflict->front());
      break;
    }

    const Literal literal = (*trail_)[trail_index];
    reason_used_to_infer_the_conflict->push_back(literal);

    // If we already encountered the same reason, we can just skip this literal
    // which is what setting clause_to_expand to the empty clause do.
    if (same_reason_identifier_.FirstVariableWithSameReason(
            literal.Variable()) != literal.Variable()) {
      clause_to_expand = {};
    } else {
      clause_to_expand = trail_->Reason(literal.Variable());
    }
    sat_clause = ReasonClauseOrNull(literal.Variable());

    --num_literal_at_highest_level_that_needs_to_be_processed;
    --trail_index;
  }
}

void SatSolver::ComputeUnionOfReasons(const std::vector<Literal>& input,
                                      std::vector<Literal>* literals) {
  tmp_mark_.ClearAndResize(num_variables_);
  literals->clear();
  for (const Literal l : input) tmp_mark_.Set(l.Variable());
  for (const Literal l : input) {
    for (const Literal r : trail_->Reason(l.Variable())) {
      if (!tmp_mark_[r.Variable()]) {
        tmp_mark_.Set(r.Variable());
        literals->push_back(r);
      }
    }
  }
  for (const Literal l : input) tmp_mark_.Clear(l.Variable());
  for (const Literal l : *literals) tmp_mark_.Clear(l.Variable());
}

// TODO(user): Remove the literals assigned at level 0.
void SatSolver::ComputePBConflict(int max_trail_index,
                                  Coefficient initial_slack,
                                  MutableUpperBoundedLinearConstraint* conflict,
                                  int* pb_backjump_level) {
  SCOPED_TIME_STAT(&stats_);
  int trail_index = max_trail_index;

  // First compute the slack of the current conflict for the assignment up to
  // trail_index. It must be negative since this is a conflict.
  Coefficient slack = initial_slack;
  DCHECK_EQ(slack,
            conflict->ComputeSlackForTrailPrefix(*trail_, trail_index + 1));
  CHECK_LT(slack, 0) << "We don't have a conflict!";

  // Iterate backward over the trail.
  int backjump_level = 0;
  while (true) {
    const BooleanVariable var = (*trail_)[trail_index].Variable();
    --trail_index;

    if (conflict->GetCoefficient(var) > 0 &&
        trail_->Assignment().LiteralIsTrue(conflict->GetLiteral(var))) {
      if (parameters_.minimize_reduction_during_pb_resolution()) {
        // When this parameter is true, we don't call ReduceCoefficients() at
        // every loop. However, it is still important to reduce the "current"
        // variable coefficient, because this can impact the value of the new
        // slack below.
        conflict->ReduceGivenCoefficient(var);
      }

      // This is the slack one level before (< Info(var).trail_index).
      slack += conflict->GetCoefficient(var);

      // This can't happen at the beginning, but may happen later.
      // It means that even without var assigned, we still have a conflict.
      if (slack < 0) continue;

      // At this point, just removing the last assignment lift the conflict.
      // So we can abort if the true assignment before that is at a lower level
      // TODO(user): Somewhat inefficient.
      // TODO(user): We could abort earlier...
      const int current_level = DecisionLevel(var);
      int i = trail_index;
      while (i >= 0) {
        const BooleanVariable previous_var = (*trail_)[i].Variable();
        if (conflict->GetCoefficient(previous_var) > 0 &&
            trail_->Assignment().LiteralIsTrue(
                conflict->GetLiteral(previous_var))) {
          break;
        }
        --i;
      }
      if (i < 0 || DecisionLevel((*trail_)[i].Variable()) < current_level) {
        backjump_level = i < 0 ? 0 : DecisionLevel((*trail_)[i].Variable());
        break;
      }

      // We can't abort, So resolve the current variable.
      DCHECK_NE(trail_->AssignmentType(var), AssignmentType::kSearchDecision);
      const bool clause_used = ResolvePBConflict(var, conflict, &slack);

      // At this point, we have a negative slack. Note that ReduceCoefficients()
      // will not change it. However it may change the slack value of the next
      // iteration (when we will no longer take into account the true literal
      // with highest trail index).
      //
      // Note that the trail_index has already been decremented, it is why
      // we need the +1 in the slack computation.
      const Coefficient slack_only_for_debug =
          DEBUG_MODE
              ? conflict->ComputeSlackForTrailPrefix(*trail_, trail_index + 1)
              : Coefficient(0);

      if (clause_used) {
        // If a clause was used, we know that slack has the correct value.
        if (!parameters_.minimize_reduction_during_pb_resolution()) {
          conflict->ReduceCoefficients();
        }
      } else {
        // TODO(user): The function below can take most of the running time on
        // some instances. The goal is to have slack updated to its new value
        // incrementally, but we are not here yet.
        if (parameters_.minimize_reduction_during_pb_resolution()) {
          slack =
              conflict->ComputeSlackForTrailPrefix(*trail_, trail_index + 1);
        } else {
          slack = conflict->ReduceCoefficientsAndComputeSlackForTrailPrefix(
              *trail_, trail_index + 1);
        }
      }
      DCHECK_EQ(slack, slack_only_for_debug);
      CHECK_LT(slack, 0);
      if (conflict->Rhs() < 0) {
        *pb_backjump_level = -1;
        return;
      }
    }
  }

  // Reduce the conflit coefficients if it is not already done.
  // This is important to avoid integer overflow.
  if (!parameters_.minimize_reduction_during_pb_resolution()) {
    conflict->ReduceCoefficients();
  }

  // Double check.
  // The sum of the literal with level <= backjump_level must propagate.
  std::vector<Coefficient> sum_for_le_level(backjump_level + 2, Coefficient(0));
  std::vector<Coefficient> max_coeff_for_ge_level(backjump_level + 2,
                                                  Coefficient(0));
  int size = 0;
  Coefficient max_sum(0);
  for (BooleanVariable var : conflict->PossibleNonZeros()) {
    const Coefficient coeff = conflict->GetCoefficient(var);
    if (coeff == 0) continue;
    max_sum += coeff;
    ++size;
    if (!trail_->Assignment().VariableIsAssigned(var) ||
        DecisionLevel(var) > backjump_level) {
      max_coeff_for_ge_level[backjump_level + 1] =
          std::max(max_coeff_for_ge_level[backjump_level + 1], coeff);
    } else {
      const int level = DecisionLevel(var);
      if (trail_->Assignment().LiteralIsTrue(conflict->GetLiteral(var))) {
        sum_for_le_level[level] += coeff;
      }
      max_coeff_for_ge_level[level] =
          std::max(max_coeff_for_ge_level[level], coeff);
    }
  }

  // Compute the cummulative version.
  for (int i = 1; i < sum_for_le_level.size(); ++i) {
    sum_for_le_level[i] += sum_for_le_level[i - 1];
  }
  for (int i = max_coeff_for_ge_level.size() - 2; i >= 0; --i) {
    max_coeff_for_ge_level[i] =
        std::max(max_coeff_for_ge_level[i], max_coeff_for_ge_level[i + 1]);
  }

  // Compute first propagation level. -1 means that the problem is UNSAT.
  // Note that the first propagation level may be < backjump_level!
  if (sum_for_le_level[0] > conflict->Rhs()) {
    *pb_backjump_level = -1;
    return;
  }
  for (int i = 0; i <= backjump_level; ++i) {
    const Coefficient level_sum = sum_for_le_level[i];
    CHECK_LE(level_sum, conflict->Rhs());
    if (conflict->Rhs() - level_sum < max_coeff_for_ge_level[i + 1]) {
      *pb_backjump_level = i;
      return;
    }
  }
  LOG(FATAL) << "The code should never reach here.";
}

void SatSolver::MinimizeConflict(
    std::vector<Literal>* conflict,
    std::vector<Literal>* reason_used_to_infer_the_conflict) {
  SCOPED_TIME_STAT(&stats_);

  const int old_size = conflict->size();
  switch (parameters_.minimization_algorithm()) {
    case SatParameters::NONE:
      return;
    case SatParameters::SIMPLE: {
      MinimizeConflictSimple(conflict);
      break;
    }
    case SatParameters::RECURSIVE: {
      MinimizeConflictRecursively(conflict);
      break;
    }
    case SatParameters::EXPERIMENTAL: {
      MinimizeConflictExperimental(conflict);
      break;
    }
  }
  if (conflict->size() < old_size) {
    ++counters_.num_minimizations;
    counters_.num_literals_removed += old_size - conflict->size();
  }
}

// This simple version just looks for any literal that is directly infered by
// other literals of the conflict. It is directly infered if the literals of its
// reason clause are either from level 0 or from the conflict itself.
//
// Note that because of the assignement struture, there is no need to process
// the literals of the conflict in order. While exploring the reason for a
// literal assignement, there will be no cycles.
void SatSolver::MinimizeConflictSimple(std::vector<Literal>* conflict) {
  SCOPED_TIME_STAT(&stats_);
  const int current_level = CurrentDecisionLevel();

  // Note that is_marked_ is already initialized and that we can start at 1
  // since the first literal of the conflict is the 1-UIP literal.
  int index = 1;
  for (int i = 1; i < conflict->size(); ++i) {
    const BooleanVariable var = (*conflict)[i].Variable();
    bool can_be_removed = false;
    if (DecisionLevel(var) != current_level) {
      // It is important not to call Reason(var) when it can be avoided.
      const gtl::Span<Literal> reason = trail_->Reason(var);
      if (!reason.empty()) {
        can_be_removed = true;
        for (Literal literal : reason) {
          if (DecisionLevel(literal.Variable()) == 0) continue;
          if (!is_marked_[literal.Variable()]) {
            can_be_removed = false;
            break;
          }
        }
      }
    }
    if (!can_be_removed) {
      (*conflict)[index] = (*conflict)[i];
      ++index;
    }
  }
  conflict->erase(conflict->begin() + index, conflict->end());
}

// This is similar to MinimizeConflictSimple() except that for each literal of
// the conflict, the literals of its reason are recursively expanded using their
// reason and so on. The recusion stop until we show that the initial literal
// can be infered from the conflict variables alone, or if we show that this is
// not the case. The result of any variable expension will be cached in order
// not to be expended again.
void SatSolver::MinimizeConflictRecursively(std::vector<Literal>* conflict) {
  SCOPED_TIME_STAT(&stats_);

  // is_marked_ will contains all the conflict literals plus the literals that
  // have been shown to depends only on the conflict literals. is_independent_
  // will contains the literals that have been shown NOT to depends only on the
  // conflict literals. The too set are exclusive for non-conflict literals, but
  // a conflict literal (which is always marked) can be independent if we showed
  // that it can't be removed from the clause.
  //
  // Optimization: There is no need to call is_marked_.ClearAndResize() or to
  // mark the conflict literals since this was already done by
  // ComputeFirstUIPConflict().
  is_independent_.ClearAndResize(num_variables_);

  // min_trail_index_per_level_ will always be reset to all
  // std::numeric_limits<int>::max() at the end. This is used to prune the
  // search because any literal at a given level with an index smaller or equal
  // to min_trail_index_per_level_[level] can't be redundant.
  if (CurrentDecisionLevel() >= min_trail_index_per_level_.size()) {
    min_trail_index_per_level_.resize(CurrentDecisionLevel() + 1,
                                      std::numeric_limits<int>::max());
  }

  // Compute the number of variable at each decision levels. This will be used
  // to pruned the DFS because we know that the minimized conflict will have at
  // least one variable of each decision levels. Because such variable can't be
  // eliminated using lower decision levels variable otherwise it will have been
  // propagated.
  //
  // Note(user): Because is_marked_ may actually contains literals that are
  // implied if the 1-UIP literal is false, we can't just iterate on the
  // variables of the conflict here.
  for (BooleanVariable var : is_marked_.PositionsSetAtLeastOnce()) {
    const int level = DecisionLevel(var);
    min_trail_index_per_level_[level] = std::min(
        min_trail_index_per_level_[level], trail_->Info(var).trail_index);
  }

  // Remove the redundant variable from the conflict. That is the ones that can
  // be infered by some other variables in the conflict.
  // Note that we can skip the first position since this is the 1-UIP.
  int index = 1;
  for (int i = 1; i < conflict->size(); ++i) {
    const BooleanVariable var = (*conflict)[i].Variable();
    if (trail_->Info(var).trail_index <=
            min_trail_index_per_level_[DecisionLevel(var)] ||
        !CanBeInferedFromConflictVariables(var)) {
      // Mark the conflict variable as independent. Note that is_marked_[var]
      // will still be true.
      is_independent_.Set(var);
      (*conflict)[index] = (*conflict)[i];
      ++index;
    }
  }
  conflict->resize(index);

  // Reset min_trail_index_per_level_. We use the sparse version only if it
  // involves less than half the size of min_trail_index_per_level_.
  const int threshold = min_trail_index_per_level_.size() / 2;
  if (is_marked_.PositionsSetAtLeastOnce().size() < threshold) {
    for (BooleanVariable var : is_marked_.PositionsSetAtLeastOnce()) {
      min_trail_index_per_level_[DecisionLevel(var)] =
          std::numeric_limits<int>::max();
    }
  } else {
    min_trail_index_per_level_.clear();
  }
}

bool SatSolver::CanBeInferedFromConflictVariables(BooleanVariable variable) {
  // Test for an already processed variable with the same reason.
  {
    DCHECK(is_marked_[variable]);
    const BooleanVariable v =
        same_reason_identifier_.FirstVariableWithSameReason(variable);
    if (v != variable) return !is_independent_[v];
  }

  // This function implement an iterative DFS from the given variable. It uses
  // the reason clause as adjacency lists. dfs_stack_ can be seens as the
  // recursive call stack of the variable we are currently processing. All its
  // adjacent variable will be pushed into variable_to_process_, and we will
  // then dequeue them one by one and process them.
  //
  // Note(user): As of 03/2014, --cpu_profile seems to indicate that using
  // dfs_stack_.assign(1, variable) is slower. My explanation is that the
  // function call is not inlined.
  dfs_stack_.clear();
  dfs_stack_.push_back(variable);
  variable_to_process_.clear();
  variable_to_process_.push_back(variable);

  // First we expand the reason for the given variable.
  for (Literal literal : trail_->Reason(variable)) {
    const BooleanVariable var = literal.Variable();
    DCHECK_NE(var, variable);
    if (is_marked_[var]) continue;
    const int level = DecisionLevel(var);
    if (level == 0) {
      // Note that this is not needed if the solver is not configured to produce
      // an unsat proof. However, the (level == 0) test shoud always be false in
      // this case because there will never be literals of level zero in any
      // reason when we don't want a proof.
      is_marked_.Set(var);
      continue;
    }
    if (trail_->Info(var).trail_index <= min_trail_index_per_level_[level] ||
        is_independent_[var]) {
      return false;
    }
    variable_to_process_.push_back(var);
  }

  // Then we start the DFS.
  while (!variable_to_process_.empty()) {
    const BooleanVariable current_var = variable_to_process_.back();
    if (current_var == dfs_stack_.back()) {
      // We finished the DFS of the variable dfs_stack_.back(), this can be seen
      // as a recursive call terminating.
      if (dfs_stack_.size() > 1) {
        DCHECK(!is_marked_[current_var]);
        is_marked_.Set(current_var);
      }
      variable_to_process_.pop_back();
      dfs_stack_.pop_back();
      continue;
    }

    // If this variable became marked since the we pushed it, we can skip it.
    if (is_marked_[current_var]) {
      variable_to_process_.pop_back();
      continue;
    }

    // This case will never be encountered since we abort right away as soon
    // as an independent variable is found.
    DCHECK(!is_independent_[current_var]);

    // Test for an already processed variable with the same reason.
    {
      const BooleanVariable v =
          same_reason_identifier_.FirstVariableWithSameReason(current_var);
      if (v != current_var) {
        if (is_independent_[v]) break;
        DCHECK(is_marked_[v]);
        variable_to_process_.pop_back();
        continue;
      }
    }

    // Expand the variable. This can be seen as making a recursive call.
    dfs_stack_.push_back(current_var);
    bool abort_early = false;
    for (Literal literal : trail_->Reason(current_var)) {
      const BooleanVariable var = literal.Variable();
      DCHECK_NE(var, current_var);
      const int level = DecisionLevel(var);
      if (level == 0 || is_marked_[var]) continue;
      if (trail_->Info(var).trail_index <= min_trail_index_per_level_[level] ||
          is_independent_[var]) {
        abort_early = true;
        break;
      }
      variable_to_process_.push_back(var);
    }
    if (abort_early) break;
  }

  // All the variable left on the dfs_stack_ are independent.
  for (const BooleanVariable var : dfs_stack_) {
    is_independent_.Set(var);
  }
  return dfs_stack_.empty();
}

namespace {

struct WeightedVariable {
  WeightedVariable(BooleanVariable v, int w) : var(v), weight(w) {}

  BooleanVariable var;
  int weight;
};

// Lexical order, by larger weight, then by smaller variable number
// to break ties
struct VariableWithLargerWeightFirst {
  bool operator()(const WeightedVariable& wv1,
                  const WeightedVariable& wv2) const {
    return (wv1.weight > wv2.weight ||
            (wv1.weight == wv2.weight && wv1.var < wv2.var));
  }
};
}  // namespace.

// This function allows a conflict variable to be replaced by another variable
// not originally in the conflict. Greater reduction and backtracking can be
// achieved this way, but the effect of this is not clear.
//
// TODO(user): More investigation needed. This seems to help on the Hanoi
// problems, but degrades performance on others.
//
// TODO(user): Find a reference for this? neither minisat nor glucose do that,
// they just do MinimizeConflictRecursively() with a different implementation.
// Note that their behavior also make more sense with the way they (and we) bump
// the variable activities.
void SatSolver::MinimizeConflictExperimental(std::vector<Literal>* conflict) {
  SCOPED_TIME_STAT(&stats_);

  // First, sort the variables in the conflict by decreasing decision levels.
  // Also initialize is_marked_ to true for all conflict variables.
  is_marked_.ClearAndResize(num_variables_);
  const int current_level = CurrentDecisionLevel();
  std::vector<WeightedVariable> variables_sorted_by_level;
  for (Literal literal : *conflict) {
    const BooleanVariable var = literal.Variable();
    is_marked_.Set(var);
    const int level = DecisionLevel(var);
    if (level < current_level) {
      variables_sorted_by_level.push_back(WeightedVariable(var, level));
    }
  }
  std::sort(variables_sorted_by_level.begin(), variables_sorted_by_level.end(),
            VariableWithLargerWeightFirst());

  // Then process the reason of the variable with highest level first.
  std::vector<BooleanVariable> to_remove;
  for (WeightedVariable weighted_var : variables_sorted_by_level) {
    const BooleanVariable var = weighted_var.var;

    // A nullptr reason means that this was a decision variable from the
    // previous levels.
    const gtl::Span<Literal> reason = trail_->Reason(var);
    if (reason.empty()) continue;

    // Compute how many and which literals from the current reason do not appear
    // in the current conflict. Level 0 literals are ignored.
    std::vector<Literal> not_contained_literals;
    for (const Literal reason_literal : reason) {
      const BooleanVariable reason_var = reason_literal.Variable();

      // We ignore level 0 variables.
      if (DecisionLevel(reason_var) == 0) continue;

      // We have a reason literal whose variable is not yet seen.
      // If there is more than one, break right away, we will not minimize the
      // current conflict with this variable.
      if (!is_marked_[reason_var]) {
        not_contained_literals.push_back(reason_literal);
        if (not_contained_literals.size() > 1) break;
      }
    }
    if (not_contained_literals.empty()) {
      // This variable will be deleted from the conflict. Note that we don't
      // unmark it. This is because this variable can be infered from the other
      // variables in the conflict, so it is okay to skip it when processing the
      // reasons of other variables.
      to_remove.push_back(var);
    } else if (not_contained_literals.size() == 1) {
      // Replace the literal from variable var with the only
      // not_contained_literals from the current reason.
      to_remove.push_back(var);
      is_marked_.Set(not_contained_literals.front().Variable());
      conflict->push_back(not_contained_literals.front());
    }
  }

  // Unmark the variable that should be removed from the conflict.
  for (BooleanVariable var : to_remove) {
    is_marked_.Clear(var);
  }

  // Remove the now unmarked literals from the conflict.
  int index = 0;
  for (int i = 0; i < conflict->size(); ++i) {
    const Literal literal = (*conflict)[i];
    if (is_marked_[literal.Variable()]) {
      (*conflict)[index] = literal;
      ++index;
    }
  }
  conflict->erase(conflict->begin() + index, conflict->end());
}

void SatSolver::DeleteDetachedClauses() {
  std::vector<SatClause*>::iterator iter =
      std::stable_partition(clauses_.begin(), clauses_.end(),
                            [](SatClause* a) { return a->IsAttached(); });
  for (std::vector<SatClause*>::iterator it = iter; it != clauses_.end();
       ++it) {
    // We do not want to mark as deleted clause of size 2 because they are
    // still kept in the solver inside the BinaryImplicationGraph.
    const size_t size = (*it)->Size();
    if (drat_writer_ != nullptr && size > 2) {
      drat_writer_->DeleteClause(
          {(*it)->begin(), size},
          /*ignore_call=*/clauses_info_.find(*it) == clauses_info_.end());
    }
    clauses_info_.erase(*it);
  }
  STLDeleteContainerPointers(iter, clauses_.end());
  clauses_.erase(iter, clauses_.end());
}

void SatSolver::CleanClauseDatabaseIfNeeded() {
  if (num_learned_clause_before_cleanup_ > 0) return;
  SCOPED_TIME_STAT(&stats_);

  // Creates a list of clauses that can be deleted. Note that only the clauses
  // that appear in clauses_info_ can potentially be removed.
  typedef std::pair<SatClause*, ClauseInfo> Entry;
  std::vector<Entry> entries;
  for (auto& entry : clauses_info_) {
    if (!entry.first->IsAttached()) continue;
    if (ClauseIsUsedAsReason(entry.first)) continue;
    if (entry.second.protected_during_next_cleanup) {
      entry.second.protected_during_next_cleanup = false;
      continue;
    }
    entries.push_back(entry);
  }
  const int num_protected_clauses = clauses_info_.size() - entries.size();

  if (parameters_.clause_cleanup_ordering() == SatParameters::CLAUSE_LBD) {
    // Order the clauses by decreasing LBD and then increasing activity.
    std::sort(entries.begin(), entries.end(),
              [](const Entry& a, const Entry& b) {
                if (a.second.lbd == b.second.lbd) {
                  return a.second.activity < b.second.activity;
                }
                return a.second.lbd > b.second.lbd;
              });
  } else {
    // Order the clauses by increasing activity and then decreasing LBD.
    std::sort(entries.begin(), entries.end(),
              [](const Entry& a, const Entry& b) {
                if (a.second.activity == b.second.activity) {
                  return a.second.lbd > b.second.lbd;
                }
                return a.second.activity < b.second.activity;
              });
  }

  // The clause we want to keep are at the end of the vector.
  int num_kept_clauses = std::min(static_cast<int>(entries.size()),
                                  parameters_.clause_cleanup_target());
  int num_deleted_clauses = entries.size() - num_kept_clauses;

  // Tricky: Because the order of the clauses_info_ iteration is NOT
  // deterministic (pointer keys), we also keep all the clauses wich have the
  // same LBD and activity as the last one so the behavior is deterministic.
  while (num_deleted_clauses > 0) {
    const ClauseInfo& a = entries[num_deleted_clauses].second;
    const ClauseInfo& b = entries[num_deleted_clauses - 1].second;
    if (a.activity != b.activity || a.lbd != b.lbd) break;
    --num_deleted_clauses;
    ++num_kept_clauses;
  }
  if (num_deleted_clauses > 0) {
    entries.resize(num_deleted_clauses);
    for (const Entry& entry : entries) {
      SatClause* clause = entry.first;
      counters_.num_literals_forgotten += clause->Size();
      clauses_propagator_.LazyDetach(clause);
    }
    clauses_propagator_.CleanUpWatchers();

    // TODO(user): If the need arise, we could avoid this linear scan on the
    // full list of clauses by not keeping the clauses from clauses_info_ there.
    DeleteDetachedClauses();
  }

  num_learned_clause_before_cleanup_ = parameters_.clause_cleanup_period();
  VLOG(1) << "Database cleanup, #protected:" << num_protected_clauses
          << " #kept:" << num_kept_clauses
          << " #deleted:" << num_deleted_clauses;
}

void SatSolver::InitRestart() {
  SCOPED_TIME_STAT(&stats_);
  restart_count_ = 0;
  luby_count_ = 0;
  strategy_counter_ = 0;
  strategy_change_conflicts_ =
      parameters_.num_conflicts_before_strategy_changes();
  conflicts_until_next_strategy_change_ = strategy_change_conflicts_;
  conflicts_until_next_restart_ = parameters_.luby_restart_period();
}

std::string SatStatusString(SatSolver::Status status) {
  switch (status) {
    case SatSolver::ASSUMPTIONS_UNSAT:
      return "ASSUMPTIONS_UNSAT";
    case SatSolver::MODEL_UNSAT:
      return "MODEL_UNSAT";
    case SatSolver::MODEL_SAT:
      return "MODEL_SAT";
    case SatSolver::LIMIT_REACHED:
      return "LIMIT_REACHED";
  }
  // Fallback. We don't use "default:" so the compiler will return an error
  // if we forgot one enum case above.
  LOG(DFATAL) << "Invalid SatSolver::Status " << status;
  return "UNKNOWN";
}

}  // namespace sat
}  // namespace operations_research
