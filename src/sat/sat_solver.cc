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
#include "base/sysinfo.h"
#include "base/join.h"
#include "util/saturated_arithmetic.h"
#include "base/stl_util.h"

namespace operations_research {
namespace sat {

SatSolver::SatSolver()
    : num_variables_(0),
      num_constraints_(0),
      pb_constraints_(&trail_),
      symmetry_propagator_(&trail_),
      current_decision_level_(0),
      assumption_level_(0),
      propagation_trail_index_(0),
      binary_propagation_trail_index_(0),
      num_processed_fixed_variables_(0),
      counters_(),
      is_model_unsat_(false),
      is_var_ordering_initialized_(false),
      variable_activity_increment_(1.0),
      clause_activity_increment_(1.0),
      is_decision_heuristic_initialized_(false),
      num_learned_clause_before_cleanup_(0),
      target_number_of_learned_clauses_(0),
      conflicts_until_next_restart_(0),
      restart_count_(0),
      same_reason_identifier_(trail_),
      is_relevant_for_core_computation_(true),
      stats_("SatSolver") {
  SetParameters(parameters_);
}

SatSolver::~SatSolver() {
  IF_STATS_ENABLED(LOG(INFO) << stats_.StatString());
  if (parameters_.unsat_proof()) {
    // We need to free the memory used by the ResolutionNode of the clauses
    for (SatClause* clause : learned_clauses_) {
      unsat_proof_.UnlockNode(clause->ResolutionNodePointer());
    }
    for (SatClause* clause : problem_clauses_) {
      unsat_proof_.UnlockNode(clause->ResolutionNodePointer());
    }
    // We also have to free the ResolutionNode of the variable assigned at
    // level 0.
    for (int i = 0; i < trail_.Index(); ++i) {
      const AssignmentInfo& info = trail_.Info(trail_[i].Variable());
      if (info.type == AssignmentInfo::UNIT_REASON) {
        ResolutionNode* node = info.resolution_node;
        unsat_proof_.UnlockNode(node);
      }
    }
    // And the one from the pseudo-Boolean constraints.
    for (ResolutionNode* node : to_unlock_) {
      unsat_proof_.UnlockNode(node);
    }
  }
  STLDeleteElements(&problem_clauses_);
  STLDeleteElements(&learned_clauses_);
}

void SatSolver::SetNumVariables(int num_variables) {
  SCOPED_TIME_STAT(&stats_);
  CHECK_GE(num_variables, num_variables_);
  num_variables_ = num_variables;
  binary_implication_graph_.Resize(num_variables);
  watched_clauses_.Resize(num_variables);
  trail_.Resize(num_variables);
  pb_constraints_.Resize(num_variables);
  decisions_.resize(num_variables);
  same_reason_identifier_.Resize(num_variables);

  // Used by NextBranch() for the decision heuristic.
  activities_.resize(num_variables, 0.0);
  pq_need_update_for_var_at_trail_index_.Resize(num_variables);
  weighted_sign_.resize(num_variables, 0.0);
  queue_elements_.resize(num_variables);

  // Only reset the polarity of the new variables.
  ResetPolarity(VariableIndex(polarity_.size()));

  // Important: Because there is new variables, we need to recompute the
  // priority queue. Note that this will not reset the activity, it will however
  // change the order of the element with the same priority.
  //
  // TODO(user): Not even do that and just push the new ones at the end?
  is_var_ordering_initialized_ = false;
}

int64 SatSolver::num_branches() const { return counters_.num_branches; }

int64 SatSolver::num_failures() const { return counters_.num_failures; }

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
  watched_clauses_.SetParameters(parameters);
  pb_constraints_.SetParameters(parameters);
  trail_.SetNeedFixedLiteralsInReason(parameters.unsat_proof());
  random_.Reset(parameters_.random_seed());
  InitRestart();
  time_limit_.reset(new TimeLimit(parameters_.max_time_in_seconds()));
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

bool SatSolver::ModelUnsat() {
  is_model_unsat_ = true;
  return false;
}

bool SatSolver::AddUnitClause(Literal true_literal) {
  SCOPED_TIME_STAT(&stats_);
  CHECK_EQ(CurrentDecisionLevel(), 0);
  if (trail_.Assignment().IsLiteralFalse(true_literal)) return ModelUnsat();
  if (trail_.Assignment().IsLiteralTrue(true_literal)) return true;
  trail_.EnqueueWithUnitReason(true_literal, CreateRootResolutionNode());
  ++num_constraints_;
  if (!Propagate()) return ModelUnsat();
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

bool SatSolver::AddProblemClauseInternal(const std::vector<Literal>& literals,
                                         ResolutionNode* node) {
  SCOPED_TIME_STAT(&stats_);
  CHECK_EQ(CurrentDecisionLevel(), 0);

  // Deals with clause of size 0 (always false) and 1 (set a literal) right away
  // so we guarantee that a SatClause is always of size greater than one. This
  // simplifies the code.
  CHECK_GT(literals.size(), 0);
  if (literals.size() == 1) {
    if (trail_.Assignment().IsLiteralFalse(literals[0])) {
      if (node != nullptr) unsat_proof_.UnlockNode(node);
      return false;
    }
    if (trail_.Assignment().IsLiteralTrue(literals[0])) {
      if (node != nullptr) unsat_proof_.UnlockNode(node);
      return true;
    }
    trail_.EnqueueWithUnitReason(literals[0], node);  // Not assigned.
    return true;
  }
  // Create a new clause.
  std::unique_ptr<SatClause> clause(
      SatClause::Create(literals, SatClause::PROBLEM_CLAUSE, node));

  if (parameters_.treat_binary_clauses_separately() && clause->Size() == 2) {
    binary_implication_graph_.AddBinaryClause(clause->FirstLiteral(),
                                              clause->SecondLiteral());
  } else {
    if (!watched_clauses_.AttachAndPropagate(clause.get(), &trail_)) {
      return ModelUnsat();
    }
    problem_clauses_.push_back(clause.release());
  }
  return true;
}

bool SatSolver::AddLinearConstraintInternal(const std::vector<LiteralWithCoeff>& cst,
                                            Coefficient rhs,
                                            Coefficient max_value) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(BooleanLinearExpressionIsCanonical(cst));
  if (rhs < 0) return ModelUnsat();   // Unsatisfiable constraint.
  if (rhs >= max_value) return true;  // Always satisfied constraint.

  // Create the associated resolution node.
  ResolutionNode* node = CreateRootResolutionNode();

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
    return AddProblemClauseInternal(literals_scratchpad_, node);
  }

  // Remember that we need to unlock the node passed to pb constraints.
  // TODO(user): Find a cleaner way. Also, if the pb_constraints_ do not need
  // this node in the end, we delay its memory release because of this.
  if (node != nullptr) to_unlock_.push_back(node);

  // TODO(user): If this constraint forces all its literal to false (when rhs is
  // zero for instance), we still add it. Optimize this?
  return pb_constraints_.AddConstraint(cst, rhs, node);
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
  //
  // Note(user): We could make this work with unsat_proof() on by adding the
  // removed literals (with a coeff of the good sign) as dependencies to the
  // ResolutionNode associated with this constraint. However, for pseudo-Boolean
  // constraints, we would loose the minimization of the reason which seems
  // important in order to get smaller core.
  Coefficient fixed_variable_shift(0);
  if (!parameters_.unsat_proof()) {
    int index = 0;
    for (const LiteralWithCoeff& term : *cst) {
      if (trail_.Assignment().IsLiteralFalse(term.literal)) continue;
      if (trail_.Assignment().IsLiteralTrue(term.literal)) {
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
    if (!AddLinearConstraintInternal(*cst, rhs, max_value)) return ModelUnsat();
  }
  if (use_lower_bound) {
    // We transform the constraint into an upper-bounded one.
    for (int i = 0; i < cst->size(); ++i) {
      (*cst)[i].literal = (*cst)[i].literal.Negated();
    }
    const Coefficient rhs =
        ComputeNegatedCanonicalRhs(lower_bound, bound_shift, max_value);
    if (!AddLinearConstraintInternal(*cst, rhs, max_value)) return ModelUnsat();
  }
  ++num_constraints_;
  if (!Propagate()) return ModelUnsat();
  return true;
}

void SatSolver::AddLearnedClauseAndEnqueueUnitPropagation(
    const std::vector<Literal>& literals, ResolutionNode* node) {
  SCOPED_TIME_STAT(&stats_);
  if (literals.size() == 1) {
    // A length 1 clause fix a literal for all the search.
    // ComputeBacktrackLevel() should have returned 0.
    CHECK_EQ(CurrentDecisionLevel(), 0);
    trail_.EnqueueWithUnitReason(literals[0], node);
  } else {
    if (parameters_.treat_binary_clauses_separately() && literals.size() == 2) {
      binary_implication_graph_.AddBinaryConflict(literals[0], literals[1],
                                                  &trail_);
    } else {
      SatClause* clause =
          SatClause::Create(literals, SatClause::LEARNED_CLAUSE, node);
      CompressLearnedClausesIfNeeded();
      --num_learned_clause_before_cleanup_;
      learned_clauses_.emplace_back(clause);
      BumpClauseActivity(clause);

      // Important: Even though the only literal at the last decision level has
      // been unassigned, its level was not modified, so ComputeLbd() works.
      clause->SetLbd(parameters_.use_lbd() ? ComputeLbd(*clause) : 0);
      CHECK(watched_clauses_.AttachAndPropagate(clause, &trail_));
    }
  }
}

namespace {

// Returns the UpperBoundedLinearConstraint used as a reason if var was
// propagated by such constraint, or nullptr otherwise.
UpperBoundedLinearConstraint* PBReasonOrNull(VariableIndex var,
                                             const Trail& trail) {
  const AssignmentInfo& info = trail.Info(var);
  if (trail.InitialAssignmentType(var) == AssignmentInfo::PB_PROPAGATION) {
    return info.pb_constraint;
  }
  if (trail.InitialAssignmentType(var) == AssignmentInfo::SAME_REASON_AS &&
      trail.InitialAssignmentType(info.reference_var) ==
          AssignmentInfo::PB_PROPAGATION) {
    const AssignmentInfo& ref_info = trail.Info(info.reference_var);
    return ref_info.pb_constraint;
  }
  return nullptr;
}

}  // namespace

void SatSolver::SaveDebugAssignment() {
  debug_assignment_.Resize(num_variables_.value());
  for (VariableIndex i(0); i < num_variables_; ++i) {
    debug_assignment_.AssignFromTrueLiteral(
        trail_.Assignment().GetTrueLiteralForAssignedVariable(i));
  }
}

bool SatSolver::ClauseIsValidUnderDebugAssignement(
    const std::vector<Literal>& clause) const {
  for (Literal l : clause) {
    if (l.Variable() >= debug_assignment_.NumberOfVariables() ||
        debug_assignment_.IsLiteralTrue(l)) {
      return true;
    }
  }
  return false;
}

bool SatSolver::PBConstraintIsValidUnderDebugAssignment(
    const std::vector<LiteralWithCoeff>& cst, const Coefficient rhs) const {
  Coefficient sum(0.0);
  for (LiteralWithCoeff term : cst) {
    if (term.literal.Variable() >= debug_assignment_.NumberOfVariables())
      continue;
    if (debug_assignment_.IsLiteralTrue(term.literal)) {
      sum += term.coefficient;
    }
  }
  return sum <= rhs;
}

int SatSolver::EnqueueDecisionAndBackjumpOnConflict(Literal true_literal) {
  SCOPED_TIME_STAT(&stats_);
  CHECK_EQ(propagation_trail_index_, trail_.Index());

  // We are back at level 0. This can happen because of a restart, or because
  // we proved that some variables must take a given value in any satisfiable
  // assignment. Trigger a simplification of the clauses if there is new fixed
  // variables.
  //
  // TODO(user): Do not trigger it all the time if it takes too much time.
  // TODO(user): Do more advanced preprocessing?
  if (CurrentDecisionLevel() == 0) {
    if (num_processed_fixed_variables_ < trail_.Index()) {
      ProcessNewlyFixedVariableResolutionNodes();
      ProcessNewlyFixedVariables();
    }
  }

  int first_propagation_index = trail_.Index();
  NewDecision(true_literal);
  while (!Propagate()) {
    ++counters_.num_failures;
    const int max_trail_index = ComputeMaxTrailIndex(trail_.FailingClause());

    // Optimization. All the activity of the variables assigned after the trail
    // index below will not change, so there is no need to update them. This is
    // also slightly better cache-wise, since we just enqueued these literals.
    UntrailWithoutPQUpdate(std::max(max_trail_index + 1, first_propagation_index));

    // A conflict occured, compute a nice reason for this failure.
    same_reason_identifier_.Clear();
    ComputeFirstUIPConflict(trail_.FailingClause(), max_trail_index,
                            &learned_conflict_,
                            &reason_used_to_infer_the_conflict_);

    // An empty conflict means that the problem is UNSAT.
    if (learned_conflict_.empty()) {
      is_model_unsat_ = true;
      return kUnsatTrailIndex;
    }
    DCHECK(IsConflictValid(learned_conflict_));
    DCHECK(ClauseIsValidUnderDebugAssignement(learned_conflict_));

    // Update the activity of all the variables in the first UIP clause.
    // Also update the activity of the last level variables expanded (and
    // thus discarded) during the first UIP computation. Note that both
    // sets are disjoint.
    const int lbd_limit =
        parameters_.use_lbd() && parameters_.use_glucose_bump_again_strategy()
            ? ComputeLbd(learned_conflict_)
            : 0;
    BumpVariableActivities(learned_conflict_, lbd_limit);
    BumpVariableActivities(reason_used_to_infer_the_conflict_, lbd_limit);

    // Bump the clause activities.
    // Note that the activity of the learned clause will be bumped too
    // by AddLearnedClauseAndEnqueueUnitPropagation().
    if (trail_.FailingSatClause() != nullptr) {
      BumpClauseActivity(trail_.FailingSatClause());
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
      compute_pb_conflict =
          (pb_constraints_.ConflictingConstraint() != nullptr);
      if (!compute_pb_conflict) {
        for (Literal lit : reason_used_to_infer_the_conflict_) {
          if (PBReasonOrNull(lit.Variable(), trail_) != nullptr) {
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
        for (Literal literal : trail_.FailingClause()) {
          pb_conflict_.AddTerm(literal.Negated(), Coefficient(1.0));
          ++num_literals;
        }
        pb_conflict_.AddToRhs(num_literals - 1);
      } else {
        // We have a pseudo-Boolean conflict, so we start from there.
        pb_constraints_.ConflictingConstraint()->AddToConflict(&pb_conflict_);
        pb_constraints_.ClearConflictingConstraint();
        initial_slack = pb_conflict_.ComputeSlackForTrailPrefix(
            trail_, max_trail_index + 1);
      }

      int pb_backjump_level;
      ComputePBConflict(max_trail_index, initial_slack, &pb_conflict_,
                        &pb_backjump_level);
      if (pb_backjump_level == -1) {
        is_model_unsat_ = true;
        return kUnsatTrailIndex;
      }

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
        CHECK_LT(pb_backjump_level, CurrentDecisionLevel());
        Backtrack(pb_backjump_level);
        first_propagation_index = trail_.Index();
        CHECK(pb_constraints_.AddLearnedConstraint(cst, pb_conflict_.Rhs(),
                                                   nullptr));
        CHECK_GT(trail_.Index(), first_propagation_index);
        counters_.num_learned_pb_literals_ += cst.size();
        continue;
      }

      // Continue with the normal clause flow, but use the PB conflict clause
      // if it has a lower backjump level.
      if (pb_backjump_level < ComputeBacktrackLevel(learned_conflict_)) {
        learned_conflict_.clear();
        is_marked_.ClearAndResize(num_variables_);
        int max_level = 0;
        int max_index = 0;
        for (LiteralWithCoeff term : cst) {
          DCHECK(Assignment().IsLiteralTrue(term.literal));
          DCHECK_EQ(term.coefficient, 1);
          const int level = trail_.Info(term.literal.Variable()).level;
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
    if (binary_implication_graph_.NumberOfImplications() != 0 &&
        parameters_.binary_minimization_algorithm() ==
            SatParameters::BINARY_MINIMIZATION_FIRST) {
      binary_implication_graph_.MinimizeConflictFirst(
          trail_, &learned_conflict_, &is_marked_);
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
          break;
        case SatParameters::BINARY_MINIMIZATION_WITH_REACHABILITY:
          binary_implication_graph_.MinimizeConflictWithReachability(
              &learned_conflict_);
          break;
        case SatParameters::EXPERIMENTAL_BINARY_MINIMIZATION:
          binary_implication_graph_.MinimizeConflictExperimental(
              trail_, &learned_conflict_);
          break;
      }
      DCHECK(IsConflictValid(learned_conflict_));
    }

    // Compute the resolution node if needed.
    // TODO(user): This is wrong if the clause comes from PB resolution.
    ResolutionNode* node =
        parameters_.unsat_proof()
            ? CreateResolutionNode(
                  trail_.FailingResolutionNode(),
                  ClauseRef(reason_used_to_infer_the_conflict_))
            : nullptr;

    // Backtrack and add the reason to the set of learned clause.
    counters_.num_literals_learned += learned_conflict_.size();
    Backtrack(ComputeBacktrackLevel(learned_conflict_));
    first_propagation_index = trail_.Index();

    DCHECK(ClauseIsValidUnderDebugAssignement(learned_conflict_));
    AddLearnedClauseAndEnqueueUnitPropagation(learned_conflict_, node);
  }
  return first_propagation_index;
}

SatSolver::Status SatSolver::ReapplyDecisionsUpTo(
    int max_level, int* first_propagation_index) {
  SCOPED_TIME_STAT(&stats_);
  int decision_index = current_decision_level_;
  while (decision_index <= max_level) {
    DCHECK_GE(decision_index, current_decision_level_);
    const Literal previous_decision = decisions_[decision_index].literal;
    ++decision_index;
    if (Assignment().IsLiteralTrue(previous_decision)) continue;
    if (Assignment().IsLiteralFalse(previous_decision)) {
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
      // A conflict occured which backjumped to an earlier decision level.
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
  CHECK_EQ(propagation_trail_index_, trail_.Index());
  decisions_[CurrentDecisionLevel()].literal = true_literal;
  int first_propagation_index = trail_.Index();
  ReapplyDecisionsUpTo(CurrentDecisionLevel(), &first_propagation_index);
  return first_propagation_index;
}

bool SatSolver::EnqueueDecisionIfNotConflicting(Literal true_literal) {
  SCOPED_TIME_STAT(&stats_);
  CHECK_EQ(propagation_trail_index_, trail_.Index());
  const int current_level = CurrentDecisionLevel();
  NewDecision(true_literal);
  if (Propagate()) {
    return true;
  } else {
    Backtrack(current_level);
    return false;
  }
}

void SatSolver::Backtrack(int target_level) {
  SCOPED_TIME_STAT(&stats_);

  // Do nothing if the CurrentDecisionLevel() is already correct.
  // This is needed, otherwise target_trail_index below will remain at zero and
  // that will cause some problems. Note that we could forbid an user to call
  // Backtrack() with the current level, but that is annoying when you just
  // want to reset the solver with Backtrack(0).
  if (CurrentDecisionLevel() == target_level) return;
  DCHECK_GE(target_level, 0);
  DCHECK_LE(target_level, CurrentDecisionLevel());
  int target_trail_index = 0;
  while (current_decision_level_ > target_level) {
    --current_decision_level_;
    target_trail_index = decisions_[current_decision_level_].trail_index;
  }
  if (is_var_ordering_initialized_) {
    Untrail(target_trail_index);
  } else {
    UntrailWithoutPQUpdate(target_trail_index);
  }
  trail_.SetDecisionLevel(target_level);
}

namespace {
// Return the next value that is a multiple of interval.
int NextMultipleOf(int64 value, int64 interval) {
  return interval * (1 + value / interval);
}
}  // namespace

void SatSolver::SetAssignmentPreference(Literal literal, double weight) {
  SCOPED_TIME_STAT(&stats_);
  if (!is_decision_heuristic_initialized_) ResetDecisionHeuristic();
  if (!parameters_.use_optimization_hints()) return;
  DCHECK_GE(weight, 0.0);
  DCHECK_LE(weight, 1.0);
  polarity_[literal.Variable()].value = literal.IsPositive();
  polarity_[literal.Variable()].use_phase_saving = false;

  // The tie_breaker is changed, so we need to reinitialize the priority queue.
  // Note that this doesn't change the activity though.
  queue_elements_[literal.Variable()].tie_breaker = weight;
  is_var_ordering_initialized_ = false;
}

SatSolver::Status SatSolver::ResetAndSolveWithGivenAssumptions(
    const std::vector<Literal>& assumptions) {
  SCOPED_TIME_STAT(&stats_);
  CHECK_LE(assumptions.size(), num_variables_);
  Backtrack(0);
  for (int i = 0; i < assumptions.size(); ++i) {
    decisions_[i].literal = assumptions[i];
  }
  assumption_level_ = assumptions.size();
  return Solve();
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
  SCOPED_TIME_STAT(&stats_);
  if (is_model_unsat_) return MODEL_UNSAT;
  timer_.Restart();

  // This is done this way, so heuristics like the weighted_sign_ one can
  // wait for all the constraint to be added before beeing initialized.
  if (!is_decision_heuristic_initialized_) ResetDecisionHeuristic();

  // Display initial statistics.
  if (parameters_.log_search_progress()) {
    LOG(INFO) << "Initial memory usage: " << MemoryUsage();
    LOG(INFO) << "Number of clauses (size > 2): " << problem_clauses_.size();
    LOG(INFO) << "Number of binary clauses: "
              << binary_implication_graph_.NumberOfImplications();
    LOG(INFO) << "Number of linear constraints: "
              << pb_constraints_.NumberOfConstraints();
    LOG(INFO) << "Number of fixed variables: " << trail_.Index();
    LOG(INFO) << "Number of watched clauses: "
              << watched_clauses_.num_watched_clauses();
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

  // Starts search.
  for (;;) {
    // Test if a limit is reached.
    if (time_limit_ != nullptr && time_limit_->LimitReached()) {
      if (parameters_.log_search_progress()) {
        LOG(INFO) << "The time limit has been reached. Aborting.";
      }
      return StatusWithLog(LIMIT_REACHED);
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

    // We need to reapply any assumptions that are not currently applied.
    if (CurrentDecisionLevel() < assumption_level_) {
      int unused = 0;
      const Status status =
          ReapplyDecisionsUpTo(assumption_level_ - 1, &unused);
      if (status != MODEL_SAT) return StatusWithLog(status);
      assumption_level_ = CurrentDecisionLevel();
    }

    // At a leaf?
    if (trail_.Index() == num_variables_.value()) {
      return StatusWithLog(MODEL_SAT);
    }

    // Trigger the restart policy?
    if (conflicts_until_next_restart_ == 0) {
      restart_count_++;
      conflicts_until_next_restart_ =
          parameters_.restart_period() * SUniv(restart_count_ + 1);
      Backtrack(assumption_level_);
    }

    // Choose the next decision variable.
    DCHECK_GE(CurrentDecisionLevel(), assumption_level_);
    const Literal next_branch = NextBranch();
    if (EnqueueDecisionAndBackjumpOnConflict(next_branch) == -1) {
      return StatusWithLog(MODEL_UNSAT);
    }
  }
}

std::vector<Literal> SatSolver::GetLastIncompatibleDecisions() {
  SCOPED_TIME_STAT(&stats_);
  std::vector<Literal> unsat_assumptions;
  const Literal false_assumption = decisions_[CurrentDecisionLevel()].literal;
  DCHECK(trail_.Assignment().IsLiteralFalse(false_assumption));
  unsat_assumptions.push_back(false_assumption);

  // This will be used to mark all the literals inspected while we process the
  // false_assumption and the reasons behind each of its variable assignments.
  is_marked_.ClearAndResize(num_variables_);
  is_marked_.Set(false_assumption.Variable());

  int trail_index = trail_.Info(false_assumption.Variable()).trail_index;
  while (true) {
    // Find next marked literal to expand from the trail.
    while (trail_index >= 0 && !is_marked_[trail_[trail_index].Variable()]) {
      --trail_index;
    }
    if (trail_index < 0) break;
    const Literal marked_literal = trail_[trail_index];
    --trail_index;

    if (trail_.InitialAssignmentType(marked_literal.Variable()) ==
        AssignmentInfo::SEARCH_DECISION) {
      unsat_assumptions.push_back(marked_literal);
    } else {
      // Marks all the literals of its reason.
      for (const Literal literal : Reason(marked_literal.Variable())) {
        const VariableIndex var = literal.Variable();
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
  const double max_activity_value = parameters_.max_variable_activity_value();
  for (const Literal literal : literals) {
    const VariableIndex var = literal.Variable();
    const int level = DecisionLevel(var);
    if (level == 0) continue;
    if (level == CurrentDecisionLevel() &&
        trail_.Info(var).type == AssignmentInfo::CLAUSE_PROPAGATION &&
        trail_.Info(var).sat_clause->IsLearned() &&
        trail_.Info(var).sat_clause->Lbd() < bump_again_lbd_limit) {
      activities_[var] += variable_activity_increment_;
    }
    activities_[var] += variable_activity_increment_;
    pq_need_update_for_var_at_trail_index_.Set(trail_.Info(var).trail_index);
    if (activities_[var] > max_activity_value) {
      RescaleVariableActivities(1.0 / max_activity_value);
    }
  }
}

void SatSolver::BumpReasonActivities(const std::vector<Literal>& literals) {
  SCOPED_TIME_STAT(&stats_);
  for (const Literal literal : literals) {
    const VariableIndex var = literal.Variable();
    if (DecisionLevel(var) > 0) {
      if (trail_.Info(var).type == AssignmentInfo::CLAUSE_PROPAGATION) {
        BumpClauseActivity(trail_.Info(var).sat_clause);
      } else if (trail_.InitialAssignmentType(var) ==
                 AssignmentInfo::PB_PROPAGATION) {
        // TODO(user): Because one pb constraint may propagate many literals,
        // this may bias the constraint activity... investigate other policy.
        pb_constraints_.BumpActivity(trail_.Info(var).pb_constraint);
      }
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
  is_var_ordering_initialized_ = false;
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
                      watched_clauses_.num_inspected_clauses()) +
         StringPrintf(
             "  num learned literals: %lld  (avg: %.1f /clause)\n",
             counters_.num_literals_learned,
             1.0 * counters_.num_literals_learned / counters_.num_failures) +
         StringPrintf("  num learned PB literals: %lld  (avg: %.1f /clause)\n",
                      counters_.num_learned_pb_literals_,
                      1.0 * counters_.num_learned_pb_literals_ /
                          counters_.num_failures) +
         StringPrintf("  num restarts: %d\n", restart_count_);
}

std::string SatSolver::RunningStatisticsString() const {
  const double time_in_s = timer_.Get();
  const int learned = learned_clauses_.size();
  return StringPrintf("%6.2lfs, mem:%s, fails:%" GG_LL_FORMAT
                      "d, "
                      "depth:%d, learned:%d, restarts:%d, vars:%d",
                      time_in_s, MemoryUsage().c_str(), counters_.num_failures,
                      CurrentDecisionLevel(), learned, restart_count_,
                      num_variables_.value() - num_processed_fixed_variables_);
}

void SatSolver::ProcessNewlyFixedVariableResolutionNodes() {
  if (!parameters_.unsat_proof()) return;
  CHECK_GE(num_processed_fixed_variables_, 0);
  for (int i = num_processed_fixed_variables_; i < trail_.Index(); ++i) {
    const AssignmentInfo& info = trail_.Info(trail_[i].Variable());
    if (info.type == AssignmentInfo::UNIT_REASON) continue;
    CHECK_NE(info.type, AssignmentInfo::SEARCH_DECISION);
    CHECK_NE(info.type, AssignmentInfo::BINARY_PROPAGATION);

    // We need this loop to remove the propagated literal from the reason.
    // TODO(user): The reason should probably not contains the propagated
    // literal in the first place. Clean that up.
    literals_scratchpad_.clear();
    for (Literal literal : Reason(trail_[i].Variable())) {
      if (literal != trail_[i]) literals_scratchpad_.push_back(literal);
    }

    // Note that this works because level 0 literals are part of the reason
    // at this point.
    ResolutionNode* new_node =
        CreateResolutionNode(info.type == AssignmentInfo::CLAUSE_PROPAGATION
                                 ? info.sat_clause->ResolutionNodePointer()
                                 : info.pb_constraint->ResolutionNodePointer(),
                             ClauseRef(literals_scratchpad_));
    trail_.SetFixedVariableInfo(trail_[i].Variable(), new_node);
  }
}

void SatSolver::ProcessNewlyFixedVariables() {
  SCOPED_TIME_STAT(&stats_);
  DCHECK_EQ(CurrentDecisionLevel(), 0);
  std::vector<Literal> removed_literals;
  std::vector<ResolutionNode*> resolution_nodes;
  int num_detached_clauses = 0;
  int num_binary = 0;

  // We remove the clauses that are always true and the fixed literals from the
  // others.
  for (int i = 0; i < 2; ++i) {
    for (SatClause* clause : (i == 0) ? problem_clauses_ : learned_clauses_) {
      if (clause->IsAttached()) {
        if (clause->RemoveFixedLiteralsAndTestIfTrue(trail_.Assignment(),
                                                     &removed_literals)) {
          // The clause is always true, detach it.
          // TODO(user): Unlock its associated resolution node right away since
          // the solver will not be able to reach it again.
          watched_clauses_.LazyDetach(clause);
          ++num_detached_clauses;
        } else if (!removed_literals.empty()) {
          if (clause->Size() == 2 &&
              parameters_.treat_binary_clauses_separately()) {
            // The clause is now a binary clause, treat it separately.
            binary_implication_graph_.AddBinaryClause(clause->FirstLiteral(),
                                                      clause->SecondLiteral());
            watched_clauses_.LazyDetach(clause);
            ++num_binary;
          } else if (parameters_.unsat_proof()) {
            // The "new" clause is derived from the old one plus the level 0
            // literals.
            ResolutionNode* new_node = CreateResolutionNode(
                clause->ResolutionNodePointer(), ClauseRef(removed_literals));
            unsat_proof_.UnlockNode(clause->ResolutionNodePointer());
            clause->ChangeResolutionNode(new_node);
          }
        }
      }
    }
  }
  watched_clauses_.CleanUpWatchers();
  if (num_detached_clauses > 0) {
    VLOG(1) << trail_.Index() << " fixed variables at level 0. "
            << "Detached " << num_detached_clauses << " clauses. " << num_binary
            << " converted to binary.";

    // Free-up learned clause memory. Note that this also postpone a bit the
    // next clause cleaning phase since we removed some clauses.
    std::vector<SatClause*>::iterator iter = std::partition(
        learned_clauses_.begin(), learned_clauses_.end(),
        std::bind1st(std::mem_fun(&SatSolver::IsClauseAttachedOrUsedAsReason),
                     this));
    if (parameters_.unsat_proof()) {
      for (std::vector<SatClause*>::iterator it = iter; it != learned_clauses_.end();
           ++it) {
        unsat_proof_.UnlockNode((*it)->ResolutionNodePointer());
      }
    }
    STLDeleteContainerPointers(iter, learned_clauses_.end());
    learned_clauses_.erase(iter, learned_clauses_.end());
  }

  // We also clean the binary implication graph.
  binary_implication_graph_.RemoveFixedVariables(trail_.Assignment());
  num_processed_fixed_variables_ = trail_.Index();
}

bool SatSolver::Propagate() {
  SCOPED_TIME_STAT(&stats_);

  // Inspect all the assignements that still need to be propagated.
  // To have reasons as short as possible and easy to compute, we prioritize
  // the propagation order between the different constraint types.
  while (true) {
    // First we inspect ALL the binary clauses.
    //
    // All the literals with trail index smaller than
    // binary_propagation_trail_index_ are the assignments that were already
    // propagated using binary clauses and thus do not need to be inspected
    // again.
    if (binary_implication_graph_.NumberOfImplications() != 0) {
      while (binary_propagation_trail_index_ < trail_.Index()) {
        const Literal literal = trail_[binary_propagation_trail_index_];
        ++binary_propagation_trail_index_;
        if (!binary_implication_graph_.PropagateOnTrue(literal, &trail_)) {
          return false;
        }
      }
    }

    // For the other types, the idea is to abort the inspection as soon as at
    // least one propagation occur so we can loop over and test again the
    // highest priority constraint types using the new information.
    const int old_index = trail_.Index();

    // Non-binary clauses.
    while (trail_.Index() == old_index &&
           propagation_trail_index_ < old_index) {
      const Literal literal = trail_[propagation_trail_index_];
      ++propagation_trail_index_;
      DCHECK_EQ(DecisionLevel(literal.Variable()), CurrentDecisionLevel());
      if (!watched_clauses_.PropagateOnFalse(literal.Negated(), &trail_)) {
        return false;
      }
    }
    if (trail_.Index() > old_index) continue;

    // Symmetry.
    while (trail_.Index() == old_index &&
           symmetry_propagator_.PropagationNeeded()) {
      if (!symmetry_propagator_.PropagateNext()) {
        // This is a bit more involved since we need to fetch a reason in order
        // to compute the conflict.
        trail_.SetFailingClause(ClauseRef(symmetry_propagator_.LastConflict(
            Reason(symmetry_propagator_.VariableAtTheSourceOfLastConflict()))));
        return false;
      }
    }
    if (trail_.Index() > old_index) continue;

    // General linear constraints.
    while (trail_.Index() == old_index && pb_constraints_.PropagationNeeded()) {
      if (!pb_constraints_.PropagateNext()) return false;
    }
    if (trail_.Index() > old_index) continue;
    break;
  }
  return true;
}

ClauseRef SatSolver::Reason(VariableIndex var) {
  DCHECK(trail_.Assignment().IsVariableAssigned(var));
  const AssignmentInfo& info = trail_.Info(var);
  switch (info.type) {
    case AssignmentInfo::SEARCH_DECISION:
    case AssignmentInfo::UNIT_REASON:
      return ClauseRef();
    case AssignmentInfo::CLAUSE_PROPAGATION:
      return info.sat_clause->PropagationReason();
    case AssignmentInfo::BINARY_PROPAGATION: {
      const Literal* literal = &info.literal;
      return ClauseRef(literal, literal + 1);
    }
    case AssignmentInfo::PB_PROPAGATION:
      pb_constraints_.ReasonFor(var, trail_.CacheReasonAtReturnedAddress(var));
      return trail_.CachedReason(var);
    case AssignmentInfo::SYMMETRY_PROPAGATION: {
      // TODO(user): Switch to iterative code to avoid possible issue with the
      // depth of the recursion.
      const Literal source = trail_[info.source_trail_index];
      symmetry_propagator_.Permute(info.symmetry_index,
                                   Reason(source.Variable()),
                                   trail_.CacheReasonAtReturnedAddress(var));
      return trail_.CachedReason(var);
    }
    case AssignmentInfo::SAME_REASON_AS:
      // Note that this should recurse only once.
      return Reason(info.reference_var);
    case AssignmentInfo::CACHED_REASON:
      return trail_.CachedReason(var);
  }
}

bool SatSolver::ResolvePBConflict(VariableIndex var,
                                  MutableUpperBoundedLinearConstraint* conflict,
                                  Coefficient* slack) {
  const AssignmentInfo& info = trail_.Info(var);

  // This is the slack of the conflict < info.trail_index
  DCHECK_EQ(*slack,
            conflict->ComputeSlackForTrailPrefix(trail_, info.trail_index));

  // Pseudo-Boolean case.
  UpperBoundedLinearConstraint* pb_reason = PBReasonOrNull(var, trail_);
  if (pb_reason != nullptr) {
    pb_reason->ResolvePBConflict(trail_, var, conflict, slack);
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
      conflict->ReduceSlackTo(trail_, info.trail_index, *slack, Coefficient(0));
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
      trail_.Assignment().GetTrueLiteralForAssignedVariable(var).Negated(),
      multiplier);
  for (Literal literal : Reason(var)) {
    DCHECK_NE(literal.Variable(), var);
    DCHECK(Assignment().IsLiteralFalse(literal));
    conflict->AddTerm(literal.Negated(), multiplier);
    ++num_literals;
  }
  conflict->AddToRhs((num_literals - 1) * multiplier);

  // All the algorithms above result in a new slack of -1.
  *slack = -1;
  DCHECK_EQ(*slack,
            conflict->ComputeSlackForTrailPrefix(trail_, info.trail_index));
  return true;
}

void SatSolver::NewDecision(Literal literal) {
  SCOPED_TIME_STAT(&stats_);
  CHECK(!Assignment().IsVariableAssigned(literal.Variable()));
  counters_.num_branches++;
  decisions_[current_decision_level_] = Decision(trail_.Index(), literal);
  ++current_decision_level_;
  trail_.SetDecisionLevel(current_decision_level_);
  trail_.Enqueue(literal, AssignmentInfo::SEARCH_DECISION);
}

Literal SatSolver::NextBranch() {
  SCOPED_TIME_STAT(&stats_);

  // Lazily initialize var_ordering_ if needed.
  if (!is_var_ordering_initialized_) {
    InitializeVariableOrdering();
  }

  // Choose the variable.
  VariableIndex var;
  const double ratio = parameters_.random_branches_ratio();
  if (ratio != 0.0 && random_.RandDouble() < ratio) {
    ++counters_.num_random_branches;
    while (true) {
      // TODO(user): This may not be super efficient if almost all the
      // variables are assigned.
      var = (*var_ordering_.Raw())[random_.Uniform(var_ordering_.Raw()->size())]
                ->variable;
      if (!trail_.Assignment().IsVariableAssigned(var)) break;
      pq_need_update_for_var_at_trail_index_.Set(trail_.Info(var).trail_index);
      var_ordering_.Remove(&queue_elements_[var]);
    }
  } else {
    // The loop is done this way in order to leave the final choice in the heap.
    DCHECK(!var_ordering_.IsEmpty());
    var = var_ordering_.Top()->variable;
    while (trail_.Assignment().IsVariableAssigned(var)) {
      var_ordering_.Pop();
      pq_need_update_for_var_at_trail_index_.Set(trail_.Info(var).trail_index);
      DCHECK(!var_ordering_.IsEmpty());
      var = var_ordering_.Top()->variable;
    }
  }

  // Choose its polarity (i.e. True of False).
  const double random_ratio = parameters_.random_polarity_ratio();
  if (random_ratio != 0.0 && random_.RandDouble() < random_ratio) {
    return Literal(var, random_.OneIn(2));
  }
  return Literal(var, polarity_[var].value);
}

void SatSolver::ResetPolarity(VariableIndex from) {
  SCOPED_TIME_STAT(&stats_);
  const int size = num_variables_.value();
  polarity_.resize(size);
  for (VariableIndex var(from); var < size; ++var) {
    Polarity p;
    switch (parameters_.initial_polarity()) {
      case SatParameters::POLARITY_TRUE:
        p.value = true;
        break;
      case SatParameters::POLARITY_FALSE:
        p.value = false;
        break;
      case SatParameters::POLARITY_RANDOM:
        p.value = random_.OneIn(2);
        break;
      case SatParameters::POLARITY_WEIGHTED_SIGN:
        p.value = weighted_sign_[var] > 0;
        break;
      case SatParameters::POLARITY_REVERSE_WEIGHTED_SIGN:
        p.value = weighted_sign_[var] < 0;
        break;
    }
    p.use_phase_saving = parameters_.use_phase_saving();
    polarity_[var] = p;
  }
}

void SatSolver::InitializeVariableOrdering() {
  SCOPED_TIME_STAT(&stats_);
  var_ordering_.Clear();
  pq_need_update_for_var_at_trail_index_.ClearAndResize(num_variables_.value());

  // First, extract the variables without activity, and add the other to the
  // priority queue.
  std::vector<VariableIndex> variables;
  for (VariableIndex var(0); var < num_variables_; ++var) {
    queue_elements_[var].variable = var;  // May not be yet initialized.
    if (!trail_.Assignment().IsVariableAssigned(var)) {
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
      std::random_shuffle(variables.begin(), variables.end(), random_);
      break;
  }

  // Add the variables without activity to the queue (in the default order)
  for (VariableIndex var : variables) {
    queue_elements_[var].weight = 0.0;
    var_ordering_.Add(&queue_elements_[var]);
  }

  // Finish the queue initialization.
  for (int i = 0; i < trail_.Index(); ++i) {
    pq_need_update_for_var_at_trail_index_.Set(i);
  }
  is_var_ordering_initialized_ = true;
}

void SatSolver::ResetDecisionHeuristic() {
  // Note that this will never be false again.
  is_decision_heuristic_initialized_ = true;

  // Reset the polarity heurisitic.
  ResetPolarity(VariableIndex(0));

  // Reset the branching variable heuristic.
  activities_.assign(num_variables_.value(), 0.0);
  is_var_ordering_initialized_ = false;

  // Reset the tie breaking.
  for (VariableIndex var(0); var < num_variables_; ++var) {
    queue_elements_[var].tie_breaker = 0.0;
  }
}

void SatSolver::UntrailWithoutPQUpdate(int target_trail_index) {
  SCOPED_TIME_STAT(&stats_);
  pb_constraints_.Untrail(target_trail_index);
  symmetry_propagator_.Untrail(target_trail_index);
  propagation_trail_index_ = std::min(propagation_trail_index_, target_trail_index);
  binary_propagation_trail_index_ =
      std::min(binary_propagation_trail_index_, target_trail_index);
  while (trail_.Index() > target_trail_index) {
    const Literal literal = trail_.Dequeue();
    const VariableIndex var = literal.Variable();
    polarity_[var].SetLastAssignmentValue(literal.IsPositive());

    // We check that the priority queue doesn't need to be updated.
    if (DEBUG_MODE && is_var_ordering_initialized_) {
      DCHECK(var_ordering_.Contains(&(queue_elements_[var])));
      DCHECK_EQ(activities_[var], queue_elements_[var].weight);
    }
  }
}

void SatSolver::Untrail(int target_trail_index) {
  SCOPED_TIME_STAT(&stats_);
  pb_constraints_.Untrail(target_trail_index);
  symmetry_propagator_.Untrail(target_trail_index);
  propagation_trail_index_ = std::min(propagation_trail_index_, target_trail_index);
  binary_propagation_trail_index_ =
      std::min(binary_propagation_trail_index_, target_trail_index);
  while (trail_.Index() > target_trail_index) {
    const Literal literal = trail_.Dequeue();
    const VariableIndex var = literal.Variable();
    polarity_[var].SetLastAssignmentValue(literal.IsPositive());

    // Update the priority queue if needed.
    // Note that the first test is just here for optimization, and that the
    // code inside would work without it.
    DCHECK_EQ(trail_.Index(), trail_.Info(var).trail_index);
    if (pq_need_update_for_var_at_trail_index_[trail_.Index()]) {
      pq_need_update_for_var_at_trail_index_.Clear(trail_.Index());
      WeightedVarQueueElement* element = &(queue_elements_[var]);
      const double new_weight = activities_[var];
      if (var_ordering_.Contains(element)) {
        if (new_weight != element->weight) {
          element->weight = new_weight;
          var_ordering_.NoteChangedPriority(element);
        }
      } else {
        element->weight = new_weight;
        var_ordering_.Add(element);
      }
    } else {
      DCHECK(var_ordering_.Contains(&(queue_elements_[var])));
      DCHECK_EQ(activities_[var], queue_elements_[var].weight);
    }
  }
}

void SatSolver::ComputeUnsatCore(std::vector<int>* core) {
  SCOPED_TIME_STAT(&stats_);
  CHECK(parameters_.unsat_proof());
  CHECK_EQ(is_model_unsat_, true);

  ProcessNewlyFixedVariableResolutionNodes();

  // Generate the resolution node corresponding to the last conflict.
  ResolutionNode* final_node = CreateResolutionNode(
      trail_.FailingResolutionNode(), trail_.FailingClause());
  CHECK(final_node != nullptr);

  // Compute the core and free up the final_node.
  unsat_proof_.ComputeUnsatCore(final_node, core);
  unsat_proof_.UnlockNode(final_node);
}

std::string SatSolver::DebugString(const SatClause& clause) const {
  std::string result;
  for (const Literal literal : clause) {
    if (!result.empty()) {
      result.append(" || ");
    }
    const std::string value =
        trail_.Assignment().IsLiteralTrue(literal)
            ? "true"
            : (trail_.Assignment().IsLiteralFalse(literal) ? "false" : "undef");
    result.append(
        StringPrintf("%s(%s)", literal.DebugString().c_str(), value.c_str()));
  }
  return result;
}

ResolutionNode* SatSolver::CreateRootResolutionNode() {
  SCOPED_TIME_STAT(&stats_);
  return parameters_.unsat_proof() && is_relevant_for_core_computation_
             ? unsat_proof_.CreateNewRootNode(num_constraints_)
             : nullptr;
}

// We currently support only two reason types.
ResolutionNode* SatSolver::ResolutionNodeForAssignment(
    VariableIndex var) const {
  ResolutionNode* node = nullptr;
  const AssignmentInfo& info = trail_.Info(var);
  switch (trail_.InitialAssignmentType(var)) {
    case AssignmentInfo::CLAUSE_PROPAGATION:
      CHECK(info.sat_clause != nullptr);
      node = info.sat_clause->ResolutionNodePointer();
      break;
    case AssignmentInfo::UNIT_REASON:
      node = info.resolution_node;
      break;
    case AssignmentInfo::PB_PROPAGATION:
      CHECK(info.pb_constraint != nullptr);
      node = info.pb_constraint->ResolutionNodePointer();
      break;
    case AssignmentInfo::SAME_REASON_AS:
      // There should be only one recursion level.
      return ResolutionNodeForAssignment(info.reference_var);
      break;
    case AssignmentInfo::CACHED_REASON:
    case AssignmentInfo::SEARCH_DECISION:
    case AssignmentInfo::BINARY_PROPAGATION:
    case AssignmentInfo::SYMMETRY_PROPAGATION:
      LOG(FATAL) << "This shouldn't happen";
      break;
  }
  return node;
}

ResolutionNode* SatSolver::CreateResolutionNode(
    ResolutionNode* failing_clause_resolution_node,
    ClauseRef reason_used_to_infer_the_conflict) {
  SCOPED_TIME_STAT(&stats_);
  tmp_parents_.clear();

  // Note that nullptr is a valid resolution node. It means that the associated
  // deduction doesn't depend on the set of constraint we care about.
  if (failing_clause_resolution_node != nullptr) {
    tmp_parents_.push_back(failing_clause_resolution_node);
  }
  for (Literal literal : reason_used_to_infer_the_conflict) {
    const VariableIndex var = literal.Variable();
    ResolutionNode* node = ResolutionNodeForAssignment(var);
    if (node != nullptr) tmp_parents_.push_back(node);
  }
  return tmp_parents_.empty()
             ? nullptr
             : unsat_proof_.CreateNewResolutionNode(&tmp_parents_);
}

int SatSolver::ComputeMaxTrailIndex(ClauseRef clause) const {
  SCOPED_TIME_STAT(&stats_);
  int trail_index = -1;
  for (const Literal literal : clause) {
    trail_index = std::max(trail_index, trail_.Info(literal.Variable()).trail_index);
  }
  return trail_index;
}

// This method will compute a first UIP conflict
//   http://www.cs.tau.ac.il/~msagiv/courses/ATP/iccad2001_final.pdf
//   http://gauss.ececs.uc.edu/SAT/articles/FAIA185-0131.pdf
void SatSolver::ComputeFirstUIPConflict(
    ClauseRef failing_clause, int max_trail_index, std::vector<Literal>* conflict,
    std::vector<Literal>* reason_used_to_infer_the_conflict) {
  SCOPED_TIME_STAT(&stats_);

  // This will be used to mark all the literals inspected while we process the
  // conflict and the reasons behind each of its variable assignments.
  is_marked_.ClearAndResize(num_variables_);

  conflict->clear();
  reason_used_to_infer_the_conflict->clear();
  if (max_trail_index == -1) return;

  // max_trail_index is the maximum trail index appearing in the failing_clause
  // and its level (Which is almost always equals to the CurrentDecisionLevel(),
  // except for symmetry propagation).
  DCHECK_EQ(max_trail_index, ComputeMaxTrailIndex(failing_clause));
  int trail_index = max_trail_index;
  const int highest_level = DecisionLevel(trail_[trail_index].Variable());
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
  ClauseRef clause_to_expand = failing_clause;
  DCHECK(!clause_to_expand.IsEmpty());
  int num_literal_at_highest_level_that_needs_to_be_processed = 0;
  while (true) {
    for (const Literal literal : clause_to_expand) {
      const VariableIndex var = literal.Variable();
      if (!is_marked_[var]) {
        is_marked_.Set(var);
        const int level = DecisionLevel(var);
        if (level == highest_level) {
          ++num_literal_at_highest_level_that_needs_to_be_processed;
        } else if (level > 0) {
          // Note that all these literals are currently false since the clause
          // to expand was used to infer the value of a literal at this level.
          DCHECK(trail_.Assignment().IsLiteralFalse(literal));
          conflict->push_back(literal);
        } else {
          reason_used_to_infer_the_conflict->push_back(literal);
        }
      }
    }

    // Find next marked literal to expand from the trail.
    DCHECK_GT(num_literal_at_highest_level_that_needs_to_be_processed, 0);
    while (!is_marked_[trail_[trail_index].Variable()]) {
      --trail_index;
      DCHECK_GE(trail_index, 0);
      DCHECK_EQ(DecisionLevel(trail_[trail_index].Variable()), highest_level);
    }

    if (num_literal_at_highest_level_that_needs_to_be_processed == 1) {
      // We have the first UIP. Add its negation to the conflict clause.
      // This way, after backtracking to the proper level, the conflict clause
      // will be unit, and infer the negation of the UIP that caused the fail.
      conflict->push_back(trail_[trail_index].Negated());

      // To respect the function API move the first UIP in the first position.
      std::swap(conflict->back(), conflict->front());
      break;
    }

    const Literal literal = trail_[trail_index];
    reason_used_to_infer_the_conflict->push_back(literal);

    // If we already encountered the same reason, we can just skip this literal
    // which is what setting clause_to_expand to the empty clause do.
    if (same_reason_identifier_.FirstVariableWithSameReason(
            literal.Variable()) != literal.Variable()) {
      clause_to_expand = ClauseRef();
    } else {
      clause_to_expand = Reason(literal.Variable());
      DCHECK(!clause_to_expand.IsEmpty());
    }

    --num_literal_at_highest_level_that_needs_to_be_processed;
    --trail_index;
  }
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
            conflict->ComputeSlackForTrailPrefix(trail_, trail_index + 1));
  CHECK_LT(slack, 0) << "We don't have a conflict!";

  // Iterate backward over the trail.
  int backjump_level = 0;
  while (true) {
    const VariableIndex var = trail_[trail_index].Variable();
    --trail_index;

    if (conflict->GetCoefficient(var) > 0 &&
        trail_.Assignment().IsLiteralTrue(conflict->GetLiteral(var))) {
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
        const VariableIndex previous_var = trail_[i].Variable();
        if (conflict->GetCoefficient(previous_var) > 0 &&
            trail_.Assignment().IsLiteralTrue(
                conflict->GetLiteral(previous_var))) {
          break;
        }
        --i;
      }
      if (i < 0 || DecisionLevel(trail_[i].Variable()) < current_level) {
        backjump_level = i < 0 ? 0 : DecisionLevel(trail_[i].Variable());
        break;
      }

      // We can't abort, So resolve the current variable.
      DCHECK_NE(trail_.Info(var).type, AssignmentInfo::SEARCH_DECISION);
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
              ? conflict->ComputeSlackForTrailPrefix(trail_, trail_index + 1)
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
          slack = conflict->ComputeSlackForTrailPrefix(trail_, trail_index + 1);
        } else {
          slack = conflict->ReduceCoefficientsAndComputeSlackForTrailPrefix(
              trail_, trail_index + 1);
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
  for (VariableIndex var : conflict->PossibleNonZeros()) {
    const Coefficient coeff = conflict->GetCoefficient(var);
    if (coeff == 0) continue;
    max_sum += coeff;
    ++size;
    if (!trail_.Assignment().IsVariableAssigned(var) ||
        DecisionLevel(var) > backjump_level) {
      max_coeff_for_ge_level[backjump_level + 1] =
          std::max(max_coeff_for_ge_level[backjump_level + 1], coeff);
    } else {
      const int level = DecisionLevel(var);
      if (trail_.Assignment().IsLiteralTrue(conflict->GetLiteral(var))) {
        sum_for_le_level[level] += coeff;
      }
      max_coeff_for_ge_level[level] = std::max(max_coeff_for_ge_level[level], coeff);
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

  // TODO(user): This has been only checked with the RECURSIVE algorithm at
  // this point.
  if (parameters_.unsat_proof()) {
    CHECK_EQ(parameters_.minimization_algorithm(), SatParameters::RECURSIVE);

    // Loop over all the marked variable. The reason of the one that are not of
    // the last level (already added) and are not independent where used to
    // minimize the clause.
    const int current_level = CurrentDecisionLevel();
    const std::vector<VariableIndex>& marked = is_marked_.PositionsSetAtLeastOnce();
    for (int i = 0; i < marked.size(); ++i) {
      if (DecisionLevel(marked[i]) == current_level) continue;
      if (!is_independent_[marked[i]]) {
        reason_used_to_infer_the_conflict->push_back(Literal(marked[i], true));
      }
    }
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
    const VariableIndex var = (*conflict)[i].Variable();
    bool can_be_removed = false;
    if (DecisionLevel(var) != current_level) {
      // It is important not to call Reason(var) when it can be avoided.
      const ClauseRef reason = Reason(var);
      if (!reason.IsEmpty()) {
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
  for (VariableIndex var : is_marked_.PositionsSetAtLeastOnce()) {
    const int level = DecisionLevel(var);
    min_trail_index_per_level_[level] =
        std::min(min_trail_index_per_level_[level], trail_.Info(var).trail_index);
  }

  // Remove the redundant variable from the conflict. That is the ones that can
  // be infered by some other variables in the conflict.
  // Note that we can skip the first position since this is the 1-UIP.
  int index = 1;
  for (int i = 1; i < conflict->size(); ++i) {
    const VariableIndex var = (*conflict)[i].Variable();
    if (trail_.Info(var).trail_index <=
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
    for (VariableIndex var : is_marked_.PositionsSetAtLeastOnce()) {
      min_trail_index_per_level_[DecisionLevel(var)] =
          std::numeric_limits<int>::max();
    }
  } else {
    min_trail_index_per_level_.clear();
  }
}

bool SatSolver::CanBeInferedFromConflictVariables(VariableIndex variable) {
  // Test for an already processed variable with the same reason.
  {
    DCHECK(is_marked_[variable]);
    const VariableIndex v =
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
  DCHECK(!Reason(variable).IsEmpty());
  for (Literal literal : Reason(variable)) {
    const VariableIndex var = literal.Variable();
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
    if (trail_.Info(var).trail_index <= min_trail_index_per_level_[level] ||
        is_independent_[var]) {
      return false;
    }
    variable_to_process_.push_back(var);
  }

  // Then we start the DFS.
  while (!variable_to_process_.empty()) {
    const VariableIndex current_var = variable_to_process_.back();
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
      const VariableIndex v =
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
    DCHECK(!Reason(current_var).IsEmpty());
    for (Literal literal : Reason(current_var)) {
      const VariableIndex var = literal.Variable();
      DCHECK_NE(var, current_var);
      const int level = DecisionLevel(var);
      if (level == 0 || is_marked_[var]) continue;
      if (trail_.Info(var).trail_index <= min_trail_index_per_level_[level] ||
          is_independent_[var]) {
        abort_early = true;
        break;
      }
      variable_to_process_.push_back(var);
    }
    if (abort_early) break;
  }

  // All the variable left on the dfs_stack_ are independent.
  for (const VariableIndex var : dfs_stack_) {
    is_independent_.Set(var);
  }
  return dfs_stack_.empty();
}

namespace {

struct WeightedVariable {
  WeightedVariable(VariableIndex v, int w) : var(v), weight(w) {}

  VariableIndex var;
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
    const VariableIndex var = literal.Variable();
    is_marked_.Set(var);
    const int level = DecisionLevel(var);
    if (level < current_level) {
      variables_sorted_by_level.push_back(WeightedVariable(var, level));
    }
  }
  std::sort(variables_sorted_by_level.begin(), variables_sorted_by_level.end(),
       VariableWithLargerWeightFirst());

  // Then process the reason of the variable with highest level first.
  std::vector<VariableIndex> to_remove;
  for (WeightedVariable weighted_var : variables_sorted_by_level) {
    const VariableIndex var = weighted_var.var;

    // A nullptr reason means that this was a decision variable from the
    // previous levels.
    const ClauseRef reason = Reason(var);
    if (reason.IsEmpty()) continue;

    // Compute how many and which literals from the current reason do not appear
    // in the current conflict. Level 0 literals are ignored.
    std::vector<Literal> not_contained_literals;
    for (const Literal reason_literal : reason) {
      const VariableIndex reason_var = reason_literal.Variable();

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
  for (VariableIndex var : to_remove) {
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

namespace {

// Order the clause by increasing LBD (Literal Blocks Distance) first. For the
// same LBD they are ordered by decreasing activity.
bool ClauseOrdering(SatClause* a, SatClause* b) {
  if (a->Lbd() == b->Lbd()) return a->Activity() > b->Activity();
  return a->Lbd() < b->Lbd();
}

}  // namespace

void SatSolver::InitLearnedClauseLimit() {
  const int num_learned_clauses = learned_clauses_.size();
  target_number_of_learned_clauses_ =
      num_learned_clauses + parameters_.clause_cleanup_increment();
  num_learned_clause_before_cleanup_ =
      target_number_of_learned_clauses_ / parameters_.clause_cleanup_ratio() -
      num_learned_clauses;
  VLOG(1) << "reduced learned database to " << num_learned_clauses
          << " clauses. Next cleanup in " << num_learned_clause_before_cleanup_
          << " conflicts.";
}

void SatSolver::CompressLearnedClausesIfNeeded() {
  if (num_learned_clause_before_cleanup_ > 0) return;
  SCOPED_TIME_STAT(&stats_);

  // First time?
  if (learned_clauses_.size() == 0) {
    InitLearnedClauseLimit();
    return;
  }

  // Move the clause that should be kept at the beginning and sort the other
  // using the ClauseOrdering order.
  std::vector<SatClause*>::iterator clause_to_keep_end = std::partition(
      learned_clauses_.begin(), learned_clauses_.end(),
      std::bind1st(std::mem_fun(&SatSolver::ClauseShouldBeKept), this));
  std::sort(clause_to_keep_end, learned_clauses_.end(), ClauseOrdering);

  // Compute the index of the first clause to delete.
  const int num_learned_clauses = learned_clauses_.size();
  const int first_clause_to_delete =
      std::max(static_cast<int>(clause_to_keep_end - learned_clauses_.begin()),
          std::min(num_learned_clauses, target_number_of_learned_clauses_));

  // Delete all the learned clause after 'first_clause_to_delete'.
  for (int i = first_clause_to_delete; i < num_learned_clauses; ++i) {
    SatClause* clause = learned_clauses_[i];
    watched_clauses_.LazyDetach(clause);
    if (clause->ResolutionNodePointer() != nullptr) {
      unsat_proof_.UnlockNode(clause->ResolutionNodePointer());
    }
  }
  watched_clauses_.CleanUpWatchers();
  for (int i = first_clause_to_delete; i < num_learned_clauses; ++i) {
    counters_.num_literals_forgotten += learned_clauses_[i]->Size();
    delete learned_clauses_[i];
  }
  learned_clauses_.resize(first_clause_to_delete);
  InitLearnedClauseLimit();
}

void SatSolver::InitRestart() {
  SCOPED_TIME_STAT(&stats_);
  restart_count_ = 0;
  if (parameters_.restart_period() > 0) {
    DCHECK_EQ(SUniv(1), 1);
    conflicts_until_next_restart_ = parameters_.restart_period();
  } else {
    conflicts_until_next_restart_ = -1;
  }
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
