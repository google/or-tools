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

#include "ortools/sat/sat_solver.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/attributes.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/timer.h"
#include "ortools/port/proto_utils.h"
#include "ortools/port/sysinfo.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/drat_proof_handler.h"
#include "ortools/sat/enforcement.h"
#include "ortools/sat/model.h"
#include "ortools/sat/pb_constraint.h"
#include "ortools/sat/restart.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_decision.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"
#include "ortools/util/logging.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/stats.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

SatSolver::SatSolver() : SatSolver(new Model()) {
  owned_model_.reset(model_);
  model_->Register<SatSolver>(this);
}

SatSolver::SatSolver(Model* model)
    : model_(model),
      binary_implication_graph_(model->GetOrCreate<BinaryImplicationGraph>()),
      clauses_propagator_(model->GetOrCreate<ClauseManager>()),
      enforcement_propagator_(model->GetOrCreate<EnforcementPropagator>()),
      pb_constraints_(model->GetOrCreate<PbConstraints>()),
      track_binary_clauses_(false),
      trail_(model->GetOrCreate<Trail>()),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      parameters_(model->GetOrCreate<SatParameters>()),
      restart_(model->GetOrCreate<RestartPolicy>()),
      decision_policy_(model->GetOrCreate<SatDecisionPolicy>()),
      logger_(model->GetOrCreate<SolverLogger>()),
      clause_activity_increment_(1.0),
      same_reason_identifier_(*trail_),
      is_relevant_for_core_computation_(true),
      drat_proof_handler_(nullptr),
      stats_("SatSolver") {
  trail_->EnableChronologicalBacktracking(
      parameters_->use_chronological_backtracking());
  InitializePropagators();
}

SatSolver::~SatSolver() { IF_STATS_ENABLED(LOG(INFO) << stats_.StatString()); }

void SatSolver::SetNumVariables(int num_variables) {
  SCOPED_TIME_STAT(&stats_);
  CHECK_GE(num_variables, num_variables_);

  num_variables_ = num_variables;
  binary_implication_graph_->Resize(num_variables);
  clauses_propagator_->Resize(num_variables);
  trail_->Resize(num_variables);
  decision_policy_->IncreaseNumVariables(num_variables);
  pb_constraints_->Resize(num_variables);
  same_reason_identifier_.Resize(num_variables);

  // The +1 is a bit tricky, it is because in
  // EnqueueDecisionAndBacktrackOnConflict() we artificially enqueue the
  // decision before checking if it is not already assigned.
  decisions_.resize(num_variables + 1);
}

int64_t SatSolver::num_branches() const { return counters_.num_branches; }

int64_t SatSolver::num_failures() const { return counters_.num_failures; }

int64_t SatSolver::num_propagations() const {
  return trail_->NumberOfEnqueues() - counters_.num_branches;
}

int64_t SatSolver::num_backtracks() const { return counters_.num_backtracks; }

int64_t SatSolver::num_restarts() const { return counters_.num_restarts; }

double SatSolver::deterministic_time() const {
  // Each of these counters mesure really basic operations. The weight are just
  // an estimate of the operation complexity. Note that these counters are never
  // reset to zero once a SatSolver is created.
  //
  // TODO(user): Find a better procedure to fix the weight than just educated
  // guess.
  return 1e-8 * (8.0 * trail_->NumberOfEnqueues() +
                 1.0 * binary_implication_graph_->num_inspections() +
                 4.0 * clauses_propagator_->num_inspected_clauses() +
                 1.0 * clauses_propagator_->num_inspected_clause_literals() +

                 // Here there is a factor 2 because of the untrail.
                 20.0 * pb_constraints_->num_constraint_lookups() +
                 2.0 * pb_constraints_->num_threshold_updates() +
                 1.0 * pb_constraints_->num_inspected_constraint_literals());
}

const SatParameters& SatSolver::parameters() const {
  SCOPED_TIME_STAT(&stats_);
  return *parameters_;
}

void SatSolver::SetParameters(const SatParameters& parameters) {
  SCOPED_TIME_STAT(&stats_);
  *parameters_ = parameters;
  restart_->Reset();
  time_limit_->ResetLimitFromParameters(parameters);
  logger_->EnableLogging(parameters.log_search_progress() || VLOG_IS_ON(1));
  logger_->SetLogToStdOut(parameters.log_to_stdout());
}

bool SatSolver::IsMemoryLimitReached() const {
  const int64_t memory_usage =
      ::operations_research::sysinfo::MemoryUsageProcess();
  const int64_t kMegaByte = 1024 * 1024;
  return memory_usage > kMegaByte * parameters_->max_memory_in_mb();
}

bool SatSolver::SetModelUnsat() {
  model_is_unsat_ = true;
  return false;
}

bool SatSolver::AddClauseDuringSearch(absl::Span<const Literal> literals) {
  if (model_is_unsat_) return false;

  // Let filter clauses if we are at level zero
  if (trail_->CurrentDecisionLevel() == 0) {
    return AddProblemClause(literals);
  }

  const int index = trail_->Index();
  if (literals.empty()) return SetModelUnsat();
  if (literals.size() == 1) return AddUnitClause(literals[0]);
  if (literals.size() == 2) {
    // TODO(user): We generate in some corner cases clauses with
    // literals[0].Variable() == literals[1].Variable(). Avoid doing that and
    // adding such binary clauses to the graph?
    if (!binary_implication_graph_->AddBinaryClause(literals[0], literals[1])) {
      CHECK_EQ(CurrentDecisionLevel(), 0);
      return SetModelUnsat();
    }
  } else {
    if (!clauses_propagator_->AddClause(literals)) {
      CHECK_EQ(CurrentDecisionLevel(), 0);
      return SetModelUnsat();
    }
  }

  // Tricky: Even if nothing new is propagated, calling Propagate() might, via
  // the LP, deduce new things. This is problematic because some code assumes
  // that when we create newly associated literals, nothing else changes.
  if (trail_->Index() == index) return true;
  return FinishPropagation();
}

bool SatSolver::AddUnitClause(Literal true_literal) {
  return AddProblemClause({true_literal});
}

bool SatSolver::AddBinaryClause(Literal a, Literal b) {
  return AddProblemClause({a, b});
}

bool SatSolver::AddTernaryClause(Literal a, Literal b, Literal c) {
  return AddProblemClause({a, b, c});
}

// Note that we will do a bit of presolve here, which might not always be
// necessary if we know we are already adding a "clean" clause with no
// duplicates or literal equivalent to others. However, we found that it is
// better to make sure we always have "clean" clause in the solver rather than
// to over-optimize this. In particular, presolve might be disabled or
// incomplete, so such unclean clause might find their way here.
bool SatSolver::AddProblemClause(absl::Span<const Literal> literals) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK_EQ(CurrentDecisionLevel(), 0);
  if (model_is_unsat_) return false;

  // Filter already assigned literals. Note that we also remap literal in case
  // we discovered equivalence later in the search.
  literals_scratchpad_.clear();
  for (Literal l : literals) {
    l = binary_implication_graph_->RepresentativeOf(l);
    if (trail_->Assignment().LiteralIsTrue(l)) return true;
    if (trail_->Assignment().LiteralIsFalse(l)) continue;
    literals_scratchpad_.push_back(l);
  }

  // A clause with l and not(l) is trivially true.
  gtl::STLSortAndRemoveDuplicates(&literals_scratchpad_);
  for (int i = 0; i + 1 < literals_scratchpad_.size(); ++i) {
    if (literals_scratchpad_[i] == literals_scratchpad_[i + 1].Negated()) {
      return true;
    }
  }

  return AddProblemClauseInternal(literals_scratchpad_);
}

bool SatSolver::AddProblemClauseInternal(absl::Span<const Literal> literals) {
  SCOPED_TIME_STAT(&stats_);
  if (DEBUG_MODE && CurrentDecisionLevel() == 0) {
    for (const Literal l : literals) {
      CHECK(!trail_->Assignment().LiteralIsAssigned(l));
    }
  }

  if (literals.empty()) return SetModelUnsat();

  if (literals.size() == 1) {
    if (drat_proof_handler_ != nullptr) {
      // Note that we will output problem unit clauses twice, but that is a
      // small price to pay for having a single variable fixing API.
      drat_proof_handler_->AddClause({literals[0]});
    }
    trail_->EnqueueWithUnitReason(literals[0]);
  } else if (literals.size() == 2) {
    // TODO(user): Make sure the presolve do not generate such clauses.
    if (literals[0] == literals[1]) {
      // Literal must be true.
      trail_->EnqueueWithUnitReason(literals[0]);
    } else if (literals[0] == literals[1].Negated()) {
      // Always true.
      return true;
    } else {
      AddBinaryClauseInternal(literals[0], literals[1]);
    }
  } else {
    if (!clauses_propagator_->AddClause(literals, trail_, /*lbd=*/-1)) {
      return SetModelUnsat();
    }
  }

  // Tricky: The PropagationIsDone() condition shouldn't change anything for a
  // pure SAT problem, however in the CP-SAT context, calling Propagate() can
  // tigger computation (like the LP) even if no domain changed since the last
  // call. We do not want to do that.
  if (!PropagationIsDone() && !Propagate()) {
    return SetModelUnsat();
  }
  return true;
}

bool SatSolver::AddLinearConstraintInternal(
    const std::vector<Literal>& enforcement_literals,
    const std::vector<LiteralWithCoeff>& cst, Coefficient rhs,
    Coefficient max_value) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(BooleanLinearExpressionIsCanonical(enforcement_literals, cst));
  if (rhs < 0) {
    // Unsatisfiable constraint if enforced.
    if (enforcement_literals.empty()) {
      return SetModelUnsat();
    } else {
      literals_scratchpad_.clear();
      for (const Literal& literal : enforcement_literals) {
        literals_scratchpad_.push_back(literal.Negated());
      }
      return AddProblemClauseInternal(literals_scratchpad_);
    }
  }
  if (rhs >= max_value) return true;  // Always satisfied constraint.

  // Since the constraint is in canonical form, the coefficients are sorted.
  const Coefficient min_coeff = cst.front().coefficient;
  const Coefficient max_coeff = cst.back().coefficient;

  // A linear upper bounded constraint is a clause if the only problematic
  // assignment is the one where all the literals are true.
  if (max_value - min_coeff <= rhs) {
    // This constraint is actually a clause. It is faster to treat it as one.
    if (enforcement_literals.empty()) {
      literals_scratchpad_.clear();
      for (const LiteralWithCoeff& term : cst) {
        literals_scratchpad_.push_back(term.literal.Negated());
      }
      return AddProblemClauseInternal(literals_scratchpad_);
    } else {
      std::vector<Literal> literals;
      for (const Literal& literal : enforcement_literals) {
        literals.push_back(literal.Negated());
      }
      for (const LiteralWithCoeff& term : cst) {
        literals.push_back(term.literal.Negated());
      }
      return AddProblemClause(literals);
    }
  }

  // Detect at most one constraints. Note that this use the fact that the
  // coefficient are sorted.
  if (!parameters_->use_pb_resolution() && max_coeff <= rhs &&
      2 * min_coeff > rhs && enforcement_literals.empty()) {
    literals_scratchpad_.clear();
    for (const LiteralWithCoeff& term : cst) {
      literals_scratchpad_.push_back(term.literal);
    }
    if (!binary_implication_graph_->AddAtMostOne(literals_scratchpad_)) {
      return SetModelUnsat();
    }
    return true;
  }

  // TODO(user): fix literals with coefficient larger than rhs to false, or
  // add implication enforcement => not(literal) (and remove them from the
  // constraint)?

  // TODO(user): If this constraint forces all its literal to false (when rhs is
  // zero for instance), we still add it. Optimize this?
  return pb_constraints_->AddConstraint(enforcement_literals, cst, rhs, trail_);
}

void SatSolver::CanonicalizeLinear(std::vector<LiteralWithCoeff>* cst,
                                   Coefficient* bound_shift,
                                   Coefficient* max_value) {
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

  // Now we canonicalize.
  // TODO(user): fix variables that must be true/false and remove them.
  Coefficient bound_delta(0);
  CHECK(ComputeBooleanLinearExpressionCanonicalForm(cst, &bound_delta,
                                                    max_value));

  CHECK(SafeAddInto(bound_delta, bound_shift));
  CHECK(SafeAddInto(fixed_variable_shift, bound_shift));
}

bool SatSolver::AddLinearConstraint(bool use_lower_bound,
                                    Coefficient lower_bound,
                                    bool use_upper_bound,
                                    Coefficient upper_bound,
                                    std::vector<Literal>* enforcement_literals,
                                    std::vector<LiteralWithCoeff>* cst) {
  SCOPED_TIME_STAT(&stats_);
  CHECK_EQ(CurrentDecisionLevel(), 0);
  if (model_is_unsat_) return false;

  gtl::STLSortAndRemoveDuplicates(enforcement_literals);
  int num_enforcement_literals = 0;
  for (int i = 0; i < enforcement_literals->size(); ++i) {
    const Literal literal = (*enforcement_literals)[i];
    if (trail_->Assignment().LiteralIsFalse(literal)) {
      return true;
    }
    if (!trail_->Assignment().LiteralIsTrue(literal)) {
      (*enforcement_literals)[num_enforcement_literals++] = literal;
    }
  }
  enforcement_literals->resize(num_enforcement_literals);

  Coefficient bound_shift(0);

  if (use_upper_bound) {
    Coefficient max_value(0);
    CanonicalizeLinear(cst, &bound_shift, &max_value);
    const Coefficient rhs =
        ComputeCanonicalRhs(upper_bound, bound_shift, max_value);
    if (!AddLinearConstraintInternal(*enforcement_literals, *cst, rhs,
                                     max_value)) {
      return SetModelUnsat();
    }
  }

  if (use_lower_bound) {
    // We need to "re-canonicalize" in case some literal were fixed while we
    // processed one direction.
    Coefficient max_value(0);
    CanonicalizeLinear(cst, &bound_shift, &max_value);

    // We transform the constraint into an upper-bounded one.
    for (int i = 0; i < cst->size(); ++i) {
      (*cst)[i].literal = (*cst)[i].literal.Negated();
    }
    const Coefficient rhs =
        ComputeNegatedCanonicalRhs(lower_bound, bound_shift, max_value);
    if (!AddLinearConstraintInternal(*enforcement_literals, *cst, rhs,
                                     max_value)) {
      return SetModelUnsat();
    }
  }

  // Tricky: The PropagationIsDone() condition shouldn't change anything for a
  // pure SAT problem, however in the CP-SAT context, calling Propagate() can
  // tigger computation (like the LP) even if no domain changed since the last
  // call. We do not want to do that.
  if (!PropagationIsDone() && !Propagate()) {
    return SetModelUnsat();
  }
  return true;
}

int SatSolver::AddLearnedClauseAndEnqueueUnitPropagation(
    const std::vector<Literal>& literals, bool is_redundant) {
  SCOPED_TIME_STAT(&stats_);

  if (literals.size() == 1) {
    if (!trail_->ChronologicalBacktrackingEnabled()) {
      // A length 1 clause fix a literal for all the search.
      // ComputeBacktrackLevel() should have returned 0.
      CHECK_EQ(CurrentDecisionLevel(), 0);
    }
    trail_->EnqueueWithUnitReason(literals[0]);
    return /*lbd=*/1;
  }

  if (literals.size() == 2) {
    if (track_binary_clauses_) {
      // This clause MUST be knew, otherwise something is wrong.
      CHECK(binary_clauses_.Add(BinaryClause(literals[0], literals[1])));
    }
    CHECK(binary_implication_graph_->AddBinaryClause(literals[0], literals[1]));
    return /*lbd=*/2;
  }

  CleanClauseDatabaseIfNeeded();

  // Important: Even though the only literal at the last decision level has
  // been unassigned, its level was not modified, so ComputeLbd() works.
  const int lbd = ComputeLbd(literals);
  if (is_redundant && lbd > parameters_->clause_cleanup_lbd_bound()) {
    --num_learned_clause_before_cleanup_;

    SatClause* clause =
        clauses_propagator_->AddRemovableClause(literals, trail_, lbd);

    // BumpClauseActivity() must be called after clauses_info_[clause] has
    // been created or it will have no effect.
    (*clauses_propagator_->mutable_clauses_info())[clause].lbd = lbd;
    BumpClauseActivity(clause);
  } else {
    CHECK(clauses_propagator_->AddClause(literals, trail_, lbd));
  }
  return lbd;
}

void SatSolver::AddPropagator(SatPropagator* propagator) {
  CHECK_EQ(CurrentDecisionLevel(), 0);
  trail_->RegisterPropagator(propagator);
  external_propagators_.push_back(propagator);
  InitializePropagators();
}

void SatSolver::AddLastPropagator(SatPropagator* propagator) {
  CHECK_EQ(CurrentDecisionLevel(), 0);
  CHECK(last_propagator_ == nullptr);
  trail_->RegisterPropagator(propagator);
  last_propagator_ = propagator;
  InitializePropagators();
}

UpperBoundedLinearConstraint* SatSolver::ReasonPbConstraintOrNull(
    BooleanVariable var) const {
  // It is important to deal properly with "SameReasonAs" variables here.
  var = trail_->ReferenceVarWithSameReason(var);
  const AssignmentInfo& info = trail_->Info(var);
  if (trail_->AssignmentType(var) == pb_constraints_->PropagatorId()) {
    return pb_constraints_->ReasonPbConstraint(info.trail_index);
  }
  return nullptr;
}

SatClause* SatSolver::ReasonClauseOrNull(BooleanVariable var) const {
  DCHECK(trail_->Assignment().VariableIsAssigned(var));
  const AssignmentInfo& info = trail_->Info(var);
  if (trail_->AssignmentType(var) == clauses_propagator_->PropagatorId()) {
    return clauses_propagator_->ReasonClause(info.trail_index);
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

void SatSolver::LoadDebugSolution(absl::Span<const Literal> solution) {
  debug_assignment_.Resize(num_variables_.value());
  for (BooleanVariable var(0); var < num_variables_; ++var) {
    if (!debug_assignment_.VariableIsAssigned(var)) continue;
    debug_assignment_.UnassignLiteral(Literal(var, true));
  }
  for (const Literal l : solution) {
    debug_assignment_.AssignFromTrueLiteral(l);
  }

  // We should only call this with complete solution.
  for (BooleanVariable var(0); var < num_variables_; ++var) {
    CHECK(debug_assignment_.VariableIsAssigned(var));
  }
}

void SatSolver::AddBinaryClauseInternal(Literal a, Literal b) {
  if (track_binary_clauses_) {
    // Abort if this clause was already added.
    if (!binary_clauses_.Add(BinaryClause(a, b))) return;
  }

  if (!binary_implication_graph_->AddBinaryClause(a, b)) {
    CHECK_EQ(CurrentDecisionLevel(), 0);
    SetModelUnsat();
  }
}

bool SatSolver::ClauseIsValidUnderDebugAssignment(
    absl::Span<const Literal> clause) const {
  if (debug_assignment_.NumberOfVariables() == 0) return true;
  for (Literal l : clause) {
    if (l.Variable() >= debug_assignment_.NumberOfVariables() ||
        debug_assignment_.LiteralIsTrue(l)) {
      return true;
    }
  }
  return false;
}

bool SatSolver::PBConstraintIsValidUnderDebugAssignment(
    absl::Span<const LiteralWithCoeff> cst, const Coefficient rhs) const {
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
bool ClauseSubsumption(absl::Span<const Literal> a, SatClause* b) {
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
  if (model_is_unsat_) return kUnsatTrailIndex;
  DCHECK(PropagationIsDone());

  // We should never enqueue before the assumptions_.
  if (DEBUG_MODE && !assumptions_.empty()) {
    CHECK_GE(current_decision_level_, assumption_level_);
  }

  EnqueueNewDecision(true_literal);
  if (!FinishPropagation()) return kUnsatTrailIndex;
  return last_decision_or_backtrack_trail_index_;
}

bool SatSolver::RestoreSolverToAssumptionLevel() {
  if (model_is_unsat_) return false;
  if (CurrentDecisionLevel() > assumption_level_) {
    Backtrack(assumption_level_);
    return true;
  }
  if (!FinishPropagation()) return false;
  return ReapplyAssumptionsIfNeeded();
}

bool SatSolver::FinishPropagation() {
  if (model_is_unsat_) return false;
  int num_loop = 0;
  while (true) {
    const int old_decision_level = current_decision_level_;
    if (!Propagate()) {
      ProcessCurrentConflict();
      if (model_is_unsat_) return false;
      if (current_decision_level_ == old_decision_level) {
        CHECK(!assumptions_.empty());
        return false;
      }

      if (++num_loop % 16 == 0 && time_limit_->LimitReached()) {
        // TODO(user): Exiting like this might cause issue since the propagation
        // is not "finished" but some code might assume it is. However since we
        // already might repropagate in the LP constraint, most of the code
        // should support "not finished propagation".
        return true;
      }
      continue;
    }
    break;
  }
  DCHECK(PropagationIsDone());
  return true;
}

bool SatSolver::ResetToLevelZero() {
  if (model_is_unsat_) return false;
  assumption_level_ = 0;
  assumptions_.clear();
  Backtrack(0);
  return FinishPropagation();
}

bool SatSolver::ResetWithGivenAssumptions(
    const std::vector<Literal>& assumptions) {
  if (!ResetToLevelZero()) return false;
  if (assumptions.empty()) return true;

  // For assumptions and core-based search, it is really important to add as
  // many binary clauses as possible. This is because we do not wan to miss any
  // early core of size 2.
  ProcessNewlyFixedVariables();

  DCHECK(assumptions_.empty());
  assumption_level_ = 1;
  assumptions_ = assumptions;
  return ReapplyAssumptionsIfNeeded();
}

// Note that we do not count these as "branches" for a reporting purpose.
bool SatSolver::ReapplyAssumptionsIfNeeded() {
  if (model_is_unsat_) return false;
  if (CurrentDecisionLevel() >= assumption_level_) return true;

  if (CurrentDecisionLevel() == 0 && !assumptions_.empty()) {
    // When assumptions_ is not empty, the first "decision" actually contains
    // multiple one, and we should never use its literal.
    CHECK_EQ(current_decision_level_, 0);
    last_decision_or_backtrack_trail_index_ = trail_->Index();
    decisions_[0] = Decision(trail_->Index(), Literal());

    ++current_decision_level_;
    trail_->SetDecisionLevel(current_decision_level_);

    // We enqueue all assumptions at once at decision level 1.
    int num_decisions = 0;
    for (const Literal lit : assumptions_) {
      if (Assignment().LiteralIsTrue(lit)) continue;
      if (Assignment().LiteralIsFalse(lit)) {
        // See GetLastIncompatibleDecisions().
        *trail_->MutableConflict() = {lit.Negated(), lit};
        if (num_decisions == 0) {
          // This is needed to avoid an empty level that cause some CHECK fail.
          current_decision_level_ = 0;
          trail_->SetDecisionLevel(0);
        }
        return false;
      }
      ++num_decisions;
      trail_->EnqueueSearchDecision(lit);
    }

    // Corner case: all assumptions are fixed at level zero, we ignore them.
    if (num_decisions == 0) {
      current_decision_level_ = 0;
      trail_->SetDecisionLevel(0);
      return ResetToLevelZero();
    }

    // Now that everything is enqueued, we propagate.
    return FinishPropagation();
  }

  DCHECK(assumptions_.empty());
  const int64_t old_num_branches = counters_.num_branches;
  const SatSolver::Status status = ReapplyDecisionsUpTo(assumption_level_ - 1);
  counters_.num_branches = old_num_branches;
  assumption_level_ = CurrentDecisionLevel();
  return (status == SatSolver::FEASIBLE);
}

void SatSolver::ProcessCurrentConflict() {
  SCOPED_TIME_STAT(&stats_);
  if (model_is_unsat_) return;

  ++counters_.num_failures;
  const int conflict_trail_index = trail_->Index();

  // A conflict occurred, compute a nice reason for this failure.
  same_reason_identifier_.Clear();
  const int max_trail_index = ComputeMaxTrailIndex(trail_->FailingClause());
  if (!assumptions_.empty() && !trail_->FailingClause().empty()) {
    // If the failing clause only contains literal at the assumptions level,
    // we cannot use the ComputeFirstUIPConflict() code as we might have more
    // than one decision.
    //
    // TODO(user): We might still want to "learn" the clause, especially if
    // it reduces to only one literal in which case we can just fix it.
    const bool all_literals_at_assumption_level =
        absl::c_all_of(trail_->FailingClause(), [&](Literal l) {
          return trail_->Info(l.Variable()).level <= assumption_level_;
        });
    if (all_literals_at_assumption_level) return;
  }

  ComputeFirstUIPConflict(max_trail_index, &learned_conflict_,
                          &reason_used_to_infer_the_conflict_,
                          &subsumed_clauses_);

  // An empty conflict means that the problem is UNSAT.
  if (learned_conflict_.empty()) return (void)SetModelUnsat();
  DCHECK(IsConflictValid(learned_conflict_));
  DCHECK(ClauseIsValidUnderDebugAssignment(learned_conflict_));

  // Update the activity of all the variables in the first UIP clause.
  // Also update the activity of the last level variables expanded (and
  // thus discarded) during the first UIP computation. Note that both
  // sets are disjoint.
  decision_policy_->BumpVariableActivities(learned_conflict_);
  decision_policy_->BumpVariableActivities(reason_used_to_infer_the_conflict_);
  if (parameters_->also_bump_variables_in_conflict_reasons()) {
    ComputeUnionOfReasons(learned_conflict_, &extra_reason_literals_);
    decision_policy_->BumpVariableActivities(extra_reason_literals_);
  }

  // Bump the clause activities.
  // Note that the activity of the learned clause will be bumped too
  // by AddLearnedClauseAndEnqueueUnitPropagation().
  if (trail_->FailingSatClause() != nullptr) {
    BumpClauseActivity(trail_->FailingSatClause());
  }
  BumpReasonActivities(reason_used_to_infer_the_conflict_);

  // Decay the activities.
  decision_policy_->UpdateVariableActivityIncrement();
  UpdateClauseActivityIncrement();
  pb_constraints_->UpdateActivityIncrement();

  // Hack from Glucose that seems to perform well.
  const int period = parameters_->glucose_decay_increment_period();
  const double max_decay = parameters_->glucose_max_decay();
  if (counters_.num_failures % period == 0 &&
      parameters_->variable_activity_decay() < max_decay) {
    parameters_->set_variable_activity_decay(
        parameters_->variable_activity_decay() +
        parameters_->glucose_decay_increment());
  }

  // PB resolution.
  // There is no point using this if the conflict and all the reasons involved
  // in its resolution were clauses.
  bool compute_pb_conflict = false;
  if (parameters_->use_pb_resolution()) {
    compute_pb_conflict = (pb_constraints_->ConflictingConstraint() != nullptr);
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
  // activities and not the pb conflict. Experiment.
  if (compute_pb_conflict) {
    pb_conflict_.ClearAndResize(num_variables_.value());
    Coefficient initial_slack(-1);
    if (pb_constraints_->ConflictingConstraint() == nullptr) {
      // Generic clause case.
      Coefficient num_literals(0);
      for (Literal literal : trail_->FailingClause()) {
        pb_conflict_.AddTerm(literal.Negated(), Coefficient(1.0));
        ++num_literals;
      }
      pb_conflict_.AddToRhs(num_literals - 1);
    } else {
      // We have a pseudo-Boolean conflict, so we start from there.
      pb_constraints_->ConflictingConstraint()->AddToConflict(&pb_conflict_);
      pb_constraints_->ClearConflictingConstraint();
      initial_slack =
          pb_conflict_.ComputeSlackForTrailPrefix(*trail_, max_trail_index + 1);
    }

    int pb_backjump_level;
    ComputePBConflict(max_trail_index, initial_slack, &pb_conflict_,
                      &pb_backjump_level);
    if (pb_backjump_level == -1) return (void)SetModelUnsat();

    // Convert the conflict into the vector<LiteralWithCoeff> form.
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
      DCHECK_GT(pb_constraints_->NumberOfConstraints(), 0);
      CHECK_LT(pb_backjump_level, CurrentDecisionLevel());
      Backtrack(pb_backjump_level);
      CHECK(pb_constraints_->AddLearnedConstraint(cst, pb_conflict_.Rhs(),
                                                  trail_));
      CHECK_GT(trail_->Index(), last_decision_or_backtrack_trail_index_);
      counters_.num_learned_pb_literals += cst.size();
      return;
    }

    // Continue with the normal clause flow, but use the PB conflict clause
    // if it has a lower backjump level.
    if (pb_backjump_level < ComputePropagationLevel(learned_conflict_)) {
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
  DCHECK(ClauseIsValidUnderDebugAssignment(learned_conflict_));
  if (!binary_implication_graph_->IsEmpty()) {
    if (parameters_->binary_minimization_algorithm() ==
        SatParameters::BINARY_MINIMIZATION_FIRST) {
      binary_implication_graph_->MinimizeConflictFirst(
          *trail_, &learned_conflict_, &is_marked_);
    } else if (parameters_->binary_minimization_algorithm() ==
               SatParameters::
                   BINARY_MINIMIZATION_FIRST_WITH_TRANSITIVE_REDUCTION) {
      binary_implication_graph_->MinimizeConflictFirstWithTransitiveReduction(
          *trail_, &learned_conflict_,
          *model_->GetOrCreate<ModelRandomGenerator>());
    }
    DCHECK(IsConflictValid(learned_conflict_));
  }

  // Minimize the learned conflict.
  MinimizeConflict(&learned_conflict_);

  // Minimize it further with binary clauses?
  if (!binary_implication_graph_->IsEmpty()) {
    // Note that on the contrary to the MinimizeConflict() above that
    // just uses the reason graph, this minimization can change the
    // clause LBD and even the backtracking level.
    switch (parameters_->binary_minimization_algorithm()) {
      case SatParameters::NO_BINARY_MINIMIZATION:
        ABSL_FALLTHROUGH_INTENDED;
      case SatParameters::BINARY_MINIMIZATION_FIRST:
        ABSL_FALLTHROUGH_INTENDED;
      case SatParameters::BINARY_MINIMIZATION_FIRST_WITH_TRANSITIVE_REDUCTION:
        break;
      case SatParameters::BINARY_MINIMIZATION_WITH_REACHABILITY:
        binary_implication_graph_->MinimizeConflictWithReachability(
            &learned_conflict_);
        break;
      case SatParameters::EXPERIMENTAL_BINARY_MINIMIZATION:
        binary_implication_graph_->MinimizeConflictExperimental(
            *trail_, &learned_conflict_);
        break;
    }
    DCHECK(IsConflictValid(learned_conflict_));
  }

  // We notify the decision before backtracking so that we can save the phase.
  // The current heuristic is to try to take a trail prefix for which there is
  // currently no conflict (hence just before the last decision was taken).
  //
  // TODO(user): It is unclear what the best heuristic is here. Both the current
  // trail index or the trail before the current decision perform well, but
  // using the full trail seems slightly better even though it will contain the
  // current conflicting literal.
  decision_policy_->BeforeConflict(trail_->Index());

  // Backtrack and add the reason to the set of learned clause.
  counters_.num_literals_learned += learned_conflict_.size();
  const int conflict_level =
      trail_->Info(learned_conflict_[0].Variable()).level;
  const int backjump_levels = CurrentDecisionLevel() - conflict_level;
  const bool should_backjump =
      !trail_->ChronologicalBacktrackingEnabled() ||
      (num_failures() > parameters_->chronological_backtrack_min_conflicts() &&
       backjump_levels > parameters_->max_backjump_levels());
  const int backtrack_level = should_backjump
                                  ? ComputePropagationLevel(learned_conflict_)
                                  : std::max(0, conflict_level - 1);
  Backtrack(backtrack_level);
  DCHECK(ClauseIsValidUnderDebugAssignment(learned_conflict_));

  // Note that we need to output the learned clause before cleaning the clause
  // database. This is because we already backtracked and some of the clauses
  // that were needed to infer the conflict may not be "reasons" anymore and
  // may be deleted.
  if (drat_proof_handler_ != nullptr) {
    drat_proof_handler_->AddClause(learned_conflict_);
  }

  // Because we might change the conflict with this minimization algorithm, we
  // cannot just subsume clauses with it blindly.
  //
  // TODO(user): Either remove that algorithm or support subsumption by just
  // checking if it is okay to do so, or doing it on the fly while computing the
  // first UIP.
  if (parameters_->minimization_algorithm() == SatParameters::EXPERIMENTAL) {
    subsumed_clauses_.clear();
  }

  // Detach any subsumed clause. They will actually be deleted on the next
  // clause cleanup phase.
  bool is_redundant = true;
  if (!subsumed_clauses_.empty() &&
      parameters_->subsumption_during_conflict_analysis()) {
    for (SatClause* clause : subsumed_clauses_) {
      DCHECK(ClauseSubsumption(learned_conflict_, clause));
      if (!clauses_propagator_->IsRemovable(clause)) {
        is_redundant = false;
      }
      clauses_propagator_->LazyDetach(clause);
    }
    clauses_propagator_->CleanUpWatchers();
    counters_.num_subsumed_clauses += subsumed_clauses_.size();
  }

  // Create and attach the new learned clause.
  const int conflict_lbd = AddLearnedClauseAndEnqueueUnitPropagation(
      learned_conflict_, is_redundant);
  restart_->OnConflict(conflict_trail_index, conflict_level, conflict_lbd);
}

SatSolver::Status SatSolver::ReapplyDecisionsUpTo(
    int max_level, int* first_propagation_index) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(assumptions_.empty());
  int decision_index = current_decision_level_;
  while (decision_index <= max_level) {
    DCHECK_GE(decision_index, current_decision_level_);
    const Literal previous_decision = decisions_[decision_index].literal;
    ++decision_index;
    if (Assignment().LiteralIsTrue(previous_decision)) {
      // Note that this particular position in decisions_ will be overridden,
      // but that is fine since this is a consequence of the previous decision,
      // so we will never need to take it into account again.
      continue;
    }
    if (Assignment().LiteralIsFalse(previous_decision)) {
      // See GetLastIncompatibleDecisions().
      *trail_->MutableConflict() = {previous_decision.Negated(),
                                    previous_decision};
      return ASSUMPTIONS_UNSAT;
    }

    // Not assigned, we try to take it.
    const int old_level = current_decision_level_;
    const int index = EnqueueDecisionAndBackjumpOnConflict(previous_decision);
    if (first_propagation_index != nullptr) {
      *first_propagation_index = std::min(*first_propagation_index, index);
    }
    if (index == kUnsatTrailIndex) return INFEASIBLE;
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
  return FEASIBLE;
}

SatSolver::Status SatSolver::EnqueueDecisionAndBacktrackOnConflict(
    Literal true_literal, int* first_propagation_index) {
  SCOPED_TIME_STAT(&stats_);
  CHECK(PropagationIsDone());
  CHECK(assumptions_.empty());
  if (model_is_unsat_) return SatSolver::INFEASIBLE;
  DCHECK_LT(CurrentDecisionLevel(), decisions_.size());
  decisions_[CurrentDecisionLevel()].literal = true_literal;
  if (first_propagation_index != nullptr) {
    *first_propagation_index = trail_->Index();
  }
  return ReapplyDecisionsUpTo(CurrentDecisionLevel(), first_propagation_index);
}

bool SatSolver::EnqueueDecisionIfNotConflicting(Literal true_literal) {
  SCOPED_TIME_STAT(&stats_);
  if (model_is_unsat_) return false;
  DCHECK(PropagationIsDone());

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
  // that will cause some problems. Note that we could forbid a user to call
  // Backtrack() with the current level, but that is annoying when you just
  // want to reset the solver with Backtrack(0).
  DCHECK(target_level == 0 || !Decisions().empty());
  if (CurrentDecisionLevel() == target_level || Decisions().empty()) return;
  DCHECK_GE(target_level, 0);
  DCHECK_LE(target_level, CurrentDecisionLevel());

  // Any backtrack to the root from a positive one is counted as a restart.
  counters_.num_backtracks++;
  if (target_level == 0) counters_.num_restarts++;

  // Per the SatPropagator interface, this is needed before calling Untrail.
  trail_->SetDecisionLevel(target_level);

  current_decision_level_ = target_level;
  const int target_trail_index =
      decisions_[current_decision_level_].trail_index;

  DCHECK_LT(target_trail_index, trail_->Index());
  for (SatPropagator* propagator : propagators_) {
    if (propagator->IsEmpty()) continue;
    propagator->Untrail(*trail_, target_trail_index);
  }
  decision_policy_->Untrail(target_trail_index);
  trail_->Untrail(target_trail_index);

  last_decision_or_backtrack_trail_index_ = trail_->Index();
}

bool SatSolver::AddBinaryClauses(absl::Span<const BinaryClause> clauses) {
  SCOPED_TIME_STAT(&stats_);
  CHECK_EQ(CurrentDecisionLevel(), 0);
  for (const BinaryClause c : clauses) {
    if (!AddBinaryClause(c.a, c.b)) return false;
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
int64_t NextMultipleOf(int64_t value, int64_t interval) {
  return interval * (1 + value / interval);
}
}  // namespace

SatSolver::Status SatSolver::ResetAndSolveWithGivenAssumptions(
    const std::vector<Literal>& assumptions, int64_t max_number_of_conflicts) {
  SCOPED_TIME_STAT(&stats_);
  if (!ResetWithGivenAssumptions(assumptions)) return UnsatStatus();
  return SolveInternal(time_limit_,
                       max_number_of_conflicts >= 0
                           ? max_number_of_conflicts
                           : parameters_->max_number_of_conflicts());
}

SatSolver::Status SatSolver::StatusWithLog(Status status) {
  SOLVER_LOG(logger_, RunningStatisticsString());
  SOLVER_LOG(logger_, StatusString(status));
  return status;
}

void SatSolver::SetAssumptionLevel(int assumption_level) {
  CHECK_GE(assumption_level, 0);
  CHECK_LE(assumption_level, CurrentDecisionLevel());
  assumption_level_ = assumption_level;

  // New assumption code.
  if (!assumptions_.empty()) {
    CHECK_EQ(assumption_level, 0);
    assumptions_.clear();
  }
}

SatSolver::Status SatSolver::SolveWithTimeLimit(TimeLimit* time_limit) {
  return SolveInternal(time_limit == nullptr ? time_limit_ : time_limit,
                       parameters_->max_number_of_conflicts());
}

SatSolver::Status SatSolver::Solve() {
  return SolveInternal(time_limit_, parameters_->max_number_of_conflicts());
}

void SatSolver::KeepAllClausesUsedToInfer(BooleanVariable variable) {
  CHECK(Assignment().VariableIsAssigned(variable));
  if (trail_->Info(variable).level == 0) return;
  int trail_index = trail_->Info(variable).trail_index;
  std::vector<bool> is_marked(trail_index + 1, false);  // move to local member.
  is_marked[trail_index] = true;
  int num = 1;
  for (; num > 0 && trail_index >= 0; --trail_index) {
    if (!is_marked[trail_index]) continue;
    is_marked[trail_index] = false;
    --num;

    const BooleanVariable var = (*trail_)[trail_index].Variable();
    SatClause* clause = ReasonClauseOrNull(var);
    if (clause != nullptr) {
      // Keep this clause.
      clauses_propagator_->mutable_clauses_info()->erase(clause);
    }
    if (trail_->AssignmentType(var) == AssignmentType::kSearchDecision) {
      continue;
    }
    for (const Literal l : trail_->Reason(var)) {
      const AssignmentInfo& info = trail_->Info(l.Variable());
      if (info.level == 0) continue;
      if (!is_marked[info.trail_index]) {
        is_marked[info.trail_index] = true;
        ++num;
      }
    }
  }
}

bool SatSolver::SubsumptionIsInteresting(BooleanVariable variable,
                                         int max_size) {
  // TODO(user): other id should probably be safe as long as we do not delete
  // the propagators. Note that symmetry is tricky since we would need to keep
  // the symmetric clause around in KeepAllClauseUsedToInfer().
  const int binary_id = binary_implication_graph_->PropagatorId();
  const int clause_id = clauses_propagator_->PropagatorId();

  CHECK(Assignment().VariableIsAssigned(variable));
  if (trail_->Info(variable).level == 0) return true;
  int trail_index = trail_->Info(variable).trail_index;
  std::vector<bool> is_marked(trail_index + 1, false);  // move to local member.
  is_marked[trail_index] = true;
  int num = 1;
  int num_clause_to_mark_as_non_deletable = 0;
  for (; num > 0 && trail_index >= 0; --trail_index) {
    if (!is_marked[trail_index]) continue;
    is_marked[trail_index] = false;
    --num;

    const BooleanVariable var = (*trail_)[trail_index].Variable();
    const int type = trail_->AssignmentType(var);
    if (type == AssignmentType::kSearchDecision) continue;
    if (type != binary_id && type != clause_id) return false;
    SatClause* clause = ReasonClauseOrNull(var);
    if (clause != nullptr && clauses_propagator_->IsRemovable(clause)) {
      if (clause->size() > max_size) {
        return false;
      }
      if (++num_clause_to_mark_as_non_deletable > 1) return false;
    }
    for (const Literal l : trail_->Reason(var)) {
      const AssignmentInfo& info = trail_->Info(l.Variable());
      if (info.level == 0) continue;
      if (!is_marked[info.trail_index]) {
        is_marked[info.trail_index] = true;
        ++num;
      }
    }
  }
  return num_clause_to_mark_as_non_deletable <= 1;
}

// TODO(user): this is really an in-processing stuff and should be moved out
// of here. Ideally this should be scheduled after other faster in-processing
// techniques. This implements "vivification" as described in
// https://doi.org/10.1016/j.artint.2019.103197, with one significant tweak:
// we sort each clause by current trail index before trying to minimize it so
// that we can reuse the trail from previous calls in case there are overlaps.
void SatSolver::TryToMinimizeClause(SatClause* clause) {
  CHECK(clause != nullptr);
  ++counters_.minimization_num_clauses;

  std::vector<Literal> candidate;
  candidate.reserve(clause->size());

  // Note that CP-SAT presolve detects clauses that share n-1 literals and
  // transforms them into (n-1 enforcement) => (1 literal per clause). We
  // currently do not support that internally, but these clauses will still
  // likely be loaded one after the other, so there is a high chance that if we
  // call TryToMinimizeClause() on consecutive clauses, there will be a long
  // prefix in common!
  //
  // TODO(user): Exploit this more by choosing a good minimization order?
  int longest_valid_prefix = 0;
  if (CurrentDecisionLevel() > 0) {
    candidate.resize(clause->size());
    // Insert any compatible decisions into their correct place in candidate
    for (Literal lit : *clause) {
      if (!Assignment().LiteralIsFalse(lit)) continue;
      const AssignmentInfo& info = trail_->Info(lit.Variable());
      if (info.level <= 0 || info.level > clause->size()) continue;
      if (decisions_[info.level - 1].literal == lit.Negated()) {
        candidate[info.level - 1] = lit;
      }
    }
    // Then compute the matching prefix and discard the rest
    for (int i = 0; i < candidate.size(); ++i) {
      if (candidate[i] != Literal()) {
        ++longest_valid_prefix;
      } else {
        break;
      }
    }
    counters_.minimization_num_reused += longest_valid_prefix;
    candidate.resize(longest_valid_prefix);
  }
  // Then do a second pass to add the remaining literals in order.
  for (Literal lit : *clause) {
    const AssignmentInfo& info = trail_->Info(lit.Variable());
    // Skip if this literal is already in the prefix.
    if (info.level >= 1 && info.level <= longest_valid_prefix &&
        candidate[info.level - 1] == lit) {
      continue;
    }
    candidate.push_back(lit);
  }
  CHECK_EQ(candidate.size(), clause->size());

  Backtrack(longest_valid_prefix);
  absl::btree_set<LiteralIndex> moved_last;
  while (!model_is_unsat_) {
    // We want each literal in candidate to appear last once in our propagation
    // order. We want to do that while maximizing the reutilization of the
    // current assignment prefix, that is minimizing the number of
    // decision/progagation we need to perform.
    const int target_level = MoveOneUnprocessedLiteralLast(
        moved_last, CurrentDecisionLevel(), &candidate);
    if (target_level == -1) break;
    Backtrack(target_level);

    while (CurrentDecisionLevel() < candidate.size()) {
      if (time_limit_->LimitReached()) return;
      const int level = CurrentDecisionLevel();
      const Literal literal = candidate[level];
      // Remove false literals
      if (Assignment().LiteralIsFalse(literal)) {
        candidate[level] = candidate.back();
        candidate.pop_back();
        continue;
      } else if (Assignment().LiteralIsTrue(literal)) {
        const int variable_level =
            LiteralTrail().Info(literal.Variable()).level;
        if (variable_level == 0) {
          ProcessNewlyFixedVariablesForDratProof();
          counters_.minimization_num_true++;
          counters_.minimization_num_removed_literals += clause->size();
          Backtrack(0);
          clauses_propagator_->Detach(clause);
          return;
        }

        if (parameters_->inprocessing_minimization_use_conflict_analysis()) {
          // Replace the clause with the reason for the literal being true, plus
          // the literal itself.
          candidate.clear();
          for (Literal lit :
               GetDecisionsFixing(trail_->Reason(literal.Variable()))) {
            candidate.push_back(lit.Negated());
          }
        } else {
          candidate.resize(variable_level);
        }
        candidate.push_back(literal);

        // If a (true) literal wasn't propagated by this clause, then we know
        // that this clause is subsumed by other clauses in the database, so we
        // can remove it so long as the subsumption is due to non-removable
        // clauses. If we can subsume this clause by making only 1 additional
        // clause permanent and that clause is no longer than this one, we will
        // do so.
        if (ReasonClauseOrNull(literal.Variable()) != clause &&
            SubsumptionIsInteresting(literal.Variable(), candidate.size())) {
          counters_.minimization_num_subsumed++;
          counters_.minimization_num_removed_literals += clause->size();
          KeepAllClausesUsedToInfer(literal.Variable());
          Backtrack(0);
          clauses_propagator_->Detach(clause);
          return;
        }

        break;
      } else {
        ++counters_.minimization_num_decisions;
        EnqueueDecisionAndBackjumpOnConflict(literal.Negated());
        if (clause->IsRemoved()) {
          Backtrack(0);
          return;
        }
        if (model_is_unsat_) return;
        if (CurrentDecisionLevel() < level) {
          // There was a conflict, consider the conflicting literal next so we
          // should be able to exploit the conflict in the next iteration.
          // TODO(user): I *think* this is sufficient to ensure pushing
          // the same literal to the new trail fails, immediately on the next
          // iteration, if not we may be able to analyse the last failure and
          // skip some propagation steps?
          std::swap(candidate[level], candidate[CurrentDecisionLevel()]);
        }
      }
    }
    if (candidate.empty()) {
      model_is_unsat_ = true;
      return;
    }
    if (!parameters_->inprocessing_minimization_use_all_orderings()) break;
    moved_last.insert(candidate.back().Index());
  }

  if (candidate.empty()) {
    model_is_unsat_ = true;
    return;
  }

  // Returns if we don't have any minimization.
  if (candidate.size() == clause->size()) return;
  Backtrack(0);

  if (candidate.size() == 1) {
    if (drat_proof_handler_ != nullptr) {
      drat_proof_handler_->AddClause(candidate);
    }
    if (!Assignment().VariableIsAssigned(candidate[0].Variable())) {
      counters_.minimization_num_removed_literals += clause->size();
      trail_->EnqueueWithUnitReason(candidate[0]);
      return (void)FinishPropagation();
    }
    return;
  }

  if (candidate.size() == 2) {
    counters_.minimization_num_removed_literals += clause->size() - 2;

    // The order is important for the drat proof.
    AddBinaryClauseInternal(candidate[0], candidate[1]);
    clauses_propagator_->Detach(clause);

    // This is needed in the corner case where this was the first binary clause
    // of the problem so that PropagationIsDone() returns true on the newly
    // created BinaryImplicationGraph.
    return (void)FinishPropagation();
  }

  counters_.minimization_num_removed_literals +=
      clause->size() - candidate.size();

  // TODO(user): If the watched literal didn't change, we could just rewrite
  // the clause while keeping the two watched literals at the beginning.
  if (!clauses_propagator_->InprocessingRewriteClause(clause, candidate)) {
    model_is_unsat_ = true;
  }
}

SatSolver::Status SatSolver::SolveInternal(TimeLimit* time_limit,
                                           int64_t max_number_of_conflicts) {
  SCOPED_TIME_STAT(&stats_);
  if (model_is_unsat_) return INFEASIBLE;

  // TODO(user): Because the counter are not reset to zero, this cause the
  // metrics / sec to be completely broken except when the solver is used
  // for exactly one Solve().
  timer_.Restart();

  // Display initial statistics.
  if (logger_->LoggingIsEnabled()) {
    SOLVER_LOG(logger_, "Initial memory usage: ", MemoryUsage());
    SOLVER_LOG(logger_, "Number of variables: ", num_variables_.value());
    SOLVER_LOG(logger_, "Number of clauses (size > 2): ",
               clauses_propagator_->num_clauses());
    SOLVER_LOG(logger_, "Number of binary clauses: ",
               binary_implication_graph_->ComputeNumImplicationsForLog());
    SOLVER_LOG(logger_, "Number of linear constraints: ",
               pb_constraints_->NumberOfConstraints());
    SOLVER_LOG(logger_, "Number of fixed variables: ", trail_->Index());
    SOLVER_LOG(logger_, "Number of watched clauses: ",
               clauses_propagator_->num_watched_clauses());
    SOLVER_LOG(logger_, "Parameters: ", ProtobufShortDebugString(*parameters_));
  }

  // Variables used to show the search progress.
  const int64_t kDisplayFrequency = 10000;
  int64_t next_display = parameters_->log_search_progress()
                             ? NextMultipleOf(num_failures(), kDisplayFrequency)
                             : std::numeric_limits<int64_t>::max();

  // Variables used to check the memory limit every kMemoryCheckFrequency.
  const int64_t kMemoryCheckFrequency = 10000;
  int64_t next_memory_check =
      NextMultipleOf(num_failures(), kMemoryCheckFrequency);

  // The max_number_of_conflicts is per solve but the counter is for the whole
  // solver.
  const int64_t kFailureLimit =
      max_number_of_conflicts == std::numeric_limits<int64_t>::max()
          ? std::numeric_limits<int64_t>::max()
          : counters_.num_failures + max_number_of_conflicts;

  // Starts search.
  for (;;) {
    // Test if a limit is reached.
    if (time_limit != nullptr) {
      AdvanceDeterministicTime(time_limit);
      if (time_limit->LimitReached()) {
        SOLVER_LOG(logger_, "The time limit has been reached. Aborting.");
        return StatusWithLog(LIMIT_REACHED);
      }
    }
    if (num_failures() >= kFailureLimit) {
      SOLVER_LOG(logger_, "The conflict limit has been reached. Aborting.");
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
        SOLVER_LOG(logger_, "The memory limit has been reached. Aborting.");
        return StatusWithLog(LIMIT_REACHED);
      }
    }

    // Display search progression. We use >= because counters_.num_failures may
    // augment by more than one at each iteration.
    if (counters_.num_failures >= next_display) {
      SOLVER_LOG(logger_, RunningStatisticsString());
      next_display = NextMultipleOf(num_failures(), kDisplayFrequency);
    }

    const int old_level = current_decision_level_;
    if (!Propagate()) {
      // A conflict occurred, continue the loop.
      ProcessCurrentConflict();
      if (model_is_unsat_) return StatusWithLog(INFEASIBLE);
      if (old_level == current_decision_level_) {
        CHECK(!assumptions_.empty());
        return StatusWithLog(ASSUMPTIONS_UNSAT);
      }
    } else {
      // We need to reapply any assumptions that are not currently applied.
      if (!ReapplyAssumptionsIfNeeded()) return StatusWithLog(UnsatStatus());

      // At a leaf?
      if (trail_->Index() == num_variables_.value()) {
        return StatusWithLog(FEASIBLE);
      }

      if (restart_->ShouldRestart()) {
        Backtrack(assumption_level_);
      }

      DCHECK_GE(CurrentDecisionLevel(), assumption_level_);
      EnqueueNewDecision(decision_policy_->NextBranch());
    }
  }
}

bool SatSolver::MinimizeByPropagation(double dtime,
                                      bool minimize_new_clauses_only) {
  CHECK(time_limit_ != nullptr);
  AdvanceDeterministicTime(time_limit_);
  const double threshold = time_limit_->GetElapsedDeterministicTime() + dtime;

  // Tricky: we don't want TryToMinimizeClause() to delete to_minimize
  // while we are processing it.
  block_clause_deletion_ = true;

  int num_resets = 0;
  while (!time_limit_->LimitReached() &&
         time_limit_->GetElapsedDeterministicTime() < threshold) {
    SatClause* to_minimize = clauses_propagator_->NextNewClauseToMinimize();
    if (!minimize_new_clauses_only && to_minimize == nullptr) {
      to_minimize = clauses_propagator_->NextClauseToMinimize();
    }

    if (to_minimize != nullptr) {
      TryToMinimizeClause(to_minimize);
      if (model_is_unsat_) return false;
    } else if (minimize_new_clauses_only) {
      break;
    } else {
      ++num_resets;
      VLOG(1) << "Minimized all clauses, restarting from first one.";
      clauses_propagator_->ResetToMinimizeIndex();
      if (num_resets > 1) break;
    }

    AdvanceDeterministicTime(time_limit_);
  }

  // Note(user): In some corner cases, the function above might find a
  // feasible assignment. I think it is okay to ignore this special case
  // that should only happen on trivial problems and just reset the solver.
  const bool result = ResetToLevelZero();

  block_clause_deletion_ = false;
  clauses_propagator_->DeleteRemovedClauses();
  return result;
}

std::vector<Literal> SatSolver::GetLastIncompatibleDecisions() {
  std::vector<Literal>* clause = trail_->MutableConflict();
  int num_true = 0;
  for (int i = 0; i < clause->size(); ++i) {
    const Literal literal = (*clause)[i];
    if (Assignment().LiteralIsTrue(literal)) {
      // literal at true in the conflict must be the last decision/assumption
      // that could not be taken. Put it at the front to add to the result
      // later.
      std::swap((*clause)[i], (*clause)[num_true++]);
    }
  }
  CHECK_LE(num_true, 1);
  std::vector<Literal> result =
      GetDecisionsFixing(absl::MakeConstSpan(*clause).subspan(num_true));
  for (int i = 0; i < num_true; ++i) {
    result.push_back((*clause)[i].Negated());
  }
  return result;
}

std::vector<Literal> SatSolver::GetDecisionsFixing(
    absl::Span<const Literal> literals) {
  SCOPED_TIME_STAT(&stats_);
  std::vector<Literal> unsat_assumptions;

  is_marked_.ClearAndResize(num_variables_);

  int trail_index = 0;
  for (const Literal lit : literals) {
    CHECK(Assignment().LiteralIsAssigned(lit));
    trail_index =
        std::max(trail_index, trail_->Info(lit.Variable()).trail_index);
    is_marked_.Set(lit.Variable());
  }

  // We just expand the conflict until we only have decisions.
  const int limit =
      CurrentDecisionLevel() > 0 ? decisions_[0].trail_index : trail_->Index();
  CHECK_LT(trail_index, trail_->Index());
  while (true) {
    // Find next marked literal to expand from the trail.
    while (trail_index >= limit &&
           !is_marked_[(*trail_)[trail_index].Variable()]) {
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
        const int level = AssignmentLevel(var);
        if (level > 0 && !is_marked_[var]) is_marked_.Set(var);
      }
    }
  }

  // We reverse the assumptions so they are in the same order as the one in
  // which the decision were made.
  std::reverse(unsat_assumptions.begin(), unsat_assumptions.end());
  return unsat_assumptions;
}

void SatSolver::BumpReasonActivities(absl::Span<const Literal> literals) {
  SCOPED_TIME_STAT(&stats_);
  for (const Literal literal : literals) {
    const BooleanVariable var = literal.Variable();
    if (AssignmentLevel(var) > 0) {
      SatClause* clause = ReasonClauseOrNull(var);
      if (clause != nullptr) {
        BumpClauseActivity(clause);
      } else {
        UpperBoundedLinearConstraint* pb_constraint =
            ReasonPbConstraintOrNull(var);
        if (pb_constraint != nullptr) {
          // TODO(user): Because one pb constraint may propagate many literals,
          // this may bias the constraint activity... investigate other policy.
          pb_constraints_->BumpActivity(pb_constraint);
        }
      }
    }
  }
}

void SatSolver::BumpClauseActivity(SatClause* clause) {
  // We only bump the activity of the clauses that have some info. So if we know
  // that we will keep a clause forever, we don't need to create its Info. More
  // than the speed, this allows to limit as much as possible the activity
  // rescaling.
  auto it = clauses_propagator_->mutable_clauses_info()->find(clause);
  if (it == clauses_propagator_->mutable_clauses_info()->end()) return;

  // Check if the new clause LBD is below our threshold to keep this clause
  // indefinitely. Note that we use a +1 here because the LBD of a newly learned
  // clause decrease by 1 just after the backjump.
  const int new_lbd = ComputeLbd(*clause);
  if (new_lbd + 1 <= parameters_->clause_cleanup_lbd_bound()) {
    clauses_propagator_->mutable_clauses_info()->erase(clause);
    return;
  }

  // Eventually protect this clause for the next cleanup phase.
  switch (parameters_->clause_cleanup_protection()) {
    case SatParameters::PROTECTION_NONE:
      break;
    case SatParameters::PROTECTION_ALWAYS:
      it->second.protected_during_next_cleanup = true;
      break;
    case SatParameters::PROTECTION_LBD:
      // This one is similar to the one used by the Glucose SAT solver.
      //
      // TODO(user): why the +1? one reason may be that the LBD of a conflict
      // decrease by 1 just after the backjump...
      if (new_lbd + 1 < it->second.lbd) {
        it->second.protected_during_next_cleanup = true;
        it->second.lbd = new_lbd;
      }
  }

  // Increase the activity.
  const double activity = it->second.activity += clause_activity_increment_;
  if (activity > parameters_->max_clause_activity_value()) {
    RescaleClauseActivities(1.0 / parameters_->max_clause_activity_value());
  }
}

void SatSolver::RescaleClauseActivities(double scaling_factor) {
  SCOPED_TIME_STAT(&stats_);
  clause_activity_increment_ *= scaling_factor;
  for (auto& entry : *clauses_propagator_->mutable_clauses_info()) {
    entry.second.activity *= scaling_factor;
  }
}

void SatSolver::UpdateClauseActivityIncrement() {
  SCOPED_TIME_STAT(&stats_);
  clause_activity_increment_ *= 1.0 / parameters_->clause_activity_decay();
}

bool SatSolver::IsConflictValid(absl::Span<const Literal> literals) {
  SCOPED_TIME_STAT(&stats_);
  if (literals.empty()) return false;
  const int highest_level = AssignmentLevel(literals[0].Variable());
  for (int i = 1; i < literals.size(); ++i) {
    const int level = AssignmentLevel(literals[i].Variable());
    if (level <= 0 || level >= highest_level) return false;
  }
  return true;
}

int SatSolver::ComputePropagationLevel(absl::Span<const Literal> literals) {
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
  int propagation_level = 0;
  for (int i = 1; i < literals.size(); ++i) {
    const int level = AssignmentLevel(literals[i].Variable());
    propagation_level = std::max(propagation_level, level);
  }
  DCHECK_LT(propagation_level, AssignmentLevel(literals[0].Variable()));
  DCHECK_LE(AssignmentLevel(literals[0].Variable()), CurrentDecisionLevel());
  return propagation_level;
}

template <typename LiteralList>
int SatSolver::ComputeLbd(const LiteralList& literals) {
  SCOPED_TIME_STAT(&stats_);
  const int limit =
      parameters_->count_assumption_levels_in_lbd() ? 0 : assumption_level_;
  int max_level = AssignmentLevel(literals.begin()->Variable());
  if (trail_->ChronologicalBacktrackingEnabled()) {
    for (const Literal literal : literals) {
      max_level = std::max(max_level, AssignmentLevel(literal.Variable()));
    }
  }

  // We know that the first literal is always of the highest level.
  is_level_marked_.ClearAndResize(SatDecisionLevel(max_level + 1));
  for (const Literal literal : literals) {
    const SatDecisionLevel level(AssignmentLevel(literal.Variable()));
    DCHECK_GE(level, 0);
    if (level > limit && !is_level_marked_[level]) {
      is_level_marked_.Set(level);
    }
  }
  return is_level_marked_.NumberOfSetCallsWithDifferentArguments();
}

std::string SatSolver::StatusString(Status status) const {
  const double time_in_s = timer_.Get();
  return absl::StrFormat("\n  status: %s\n", SatStatusString(status)) +
         absl::StrFormat("  time: %fs\n", time_in_s) +
         absl::StrFormat("  memory: %s\n", MemoryUsage()) +
         absl::StrFormat(
             "  num failures: %d  (%.0f /sec)\n", counters_.num_failures,
             static_cast<double>(counters_.num_failures) / time_in_s) +
         absl::StrFormat(
             "  num branches: %d (%.0f /sec)\n", counters_.num_branches,
             static_cast<double>(counters_.num_branches) / time_in_s) +
         absl::StrFormat("  num propagations: %d  (%.0f /sec)\n",
                         num_propagations(),
                         static_cast<double>(num_propagations()) / time_in_s) +
         absl::StrFormat("  num binary propagations: %d\n",
                         binary_implication_graph_->num_propagations()) +
         absl::StrFormat("  num binary inspections: %d\n",
                         binary_implication_graph_->num_inspections()) +
         absl::StrFormat(
             "  num binary redundant implications: %d\n",
             binary_implication_graph_->num_redundant_implications()) +
         absl::StrFormat(
             "  num classic minimizations: %d"
             "  (literals removed: %d)\n",
             counters_.num_minimizations, counters_.num_literals_removed) +
         absl::StrFormat(
             "  num binary minimizations: %d"
             "  (literals removed: %d)\n",
             binary_implication_graph_->num_minimization(),
             binary_implication_graph_->num_literals_removed()) +
         absl::StrFormat("  num inspected clauses: %d\n",
                         clauses_propagator_->num_inspected_clauses()) +
         absl::StrFormat("  num inspected clause_literals: %d\n",
                         clauses_propagator_->num_inspected_clause_literals()) +
         absl::StrFormat(
             "  num learned literals: %d  (avg: %.1f /clause)\n",
             counters_.num_literals_learned,
             1.0 * counters_.num_literals_learned / counters_.num_failures) +
         absl::StrFormat(
             "  num learned PB literals: %d  (avg: %.1f /clause)\n",
             counters_.num_learned_pb_literals,
             1.0 * counters_.num_learned_pb_literals / counters_.num_failures) +
         absl::StrFormat("  num subsumed clauses: %d\n",
                         counters_.num_subsumed_clauses) +
         absl::StrFormat("  minimization_num_clauses: %d\n",
                         counters_.minimization_num_clauses) +
         absl::StrFormat("  minimization_num_decisions: %d\n",
                         counters_.minimization_num_decisions) +
         absl::StrFormat("  minimization_num_true: %d\n",
                         counters_.minimization_num_true) +
         absl::StrFormat("  minimization_num_subsumed: %d\n",
                         counters_.minimization_num_subsumed) +
         absl::StrFormat("  minimization_num_removed_literals: %d\n",
                         counters_.minimization_num_removed_literals) +
         absl::StrFormat("  pb num threshold updates: %d\n",
                         pb_constraints_->num_threshold_updates()) +
         absl::StrFormat("  pb num constraint lookups: %d\n",
                         pb_constraints_->num_constraint_lookups()) +
         absl::StrFormat("  pb num inspected constraint literals: %d\n",
                         pb_constraints_->num_inspected_constraint_literals()) +
         restart_->InfoString() +
         absl::StrFormat("  deterministic time: %f\n", deterministic_time());
}

std::string SatSolver::RunningStatisticsString() const {
  const double time_in_s = timer_.Get();
  return absl::StrFormat(
      "%6.2fs, mem:%s, fails:%d, depth:%d, clauses:%d, tmp:%d, bin:%u, "
      "restarts:%d, vars:%d",
      time_in_s, MemoryUsage(), counters_.num_failures, CurrentDecisionLevel(),
      clauses_propagator_->num_clauses() -
          clauses_propagator_->num_removable_clauses(),
      clauses_propagator_->num_removable_clauses(),
      binary_implication_graph_->ComputeNumImplicationsForLog(),
      restart_->NumRestarts(),
      num_variables_.value() - num_processed_fixed_variables_);
}

void SatSolver::ProcessNewlyFixedVariablesForDratProof() {
  if (drat_proof_handler_ == nullptr) return;
  if (CurrentDecisionLevel() != 0) return;

  // We need to output the literals that are fixed so we can remove all
  // clauses that contains them. Note that this doesn't seems to be needed
  // for drat-trim.
  //
  // TODO(user): Ideally we could output such literal as soon as they are fixed,
  // but this is not that easy to do. Spend some time to find a cleaner
  // alternative? Currently this works, but:
  // - We will output some fixed literals twice since we already output learnt
  //   clauses of size one.
  // - We need to call this function when needed.
  Literal temp;
  for (; drat_num_processed_fixed_variables_ < trail_->Index();
       ++drat_num_processed_fixed_variables_) {
    temp = (*trail_)[drat_num_processed_fixed_variables_];
    drat_proof_handler_->AddClause({&temp, 1});
  }
}

void SatSolver::ProcessNewlyFixedVariables() {
  SCOPED_TIME_STAT(&stats_);
  DCHECK_EQ(CurrentDecisionLevel(), 0);
  int num_detached_clauses = 0;
  int num_binary = 0;

  ProcessNewlyFixedVariablesForDratProof();

  // We remove the clauses that are always true and the fixed literals from the
  // others. Note that none of the clause should be all false because we should
  // have detected a conflict before this is called.
  const int saved_index = trail_->Index();
  for (SatClause* clause : clauses_propagator_->AllClausesInCreationOrder()) {
    if (clause->IsRemoved()) continue;

    const size_t old_size = clause->size();
    if (clause->RemoveFixedLiteralsAndTestIfTrue(trail_->Assignment())) {
      // The clause is always true, detach it.
      clauses_propagator_->LazyDetach(clause);
      ++num_detached_clauses;
      continue;
    }

    const size_t new_size = clause->size();
    if (new_size == old_size) continue;

    if (drat_proof_handler_ != nullptr) {
      CHECK_GT(new_size, 0);
      drat_proof_handler_->AddClause({clause->begin(), new_size});
      drat_proof_handler_->DeleteClause({clause->begin(), old_size});
    }

    if (new_size == 2) {
      // This clause is now a binary clause, treat it separately. Note that
      // it is safe to do that because this clause can't be used as a reason
      // since we are at level zero and the clause is not satisfied.
      AddBinaryClauseInternal(clause->FirstLiteral(), clause->SecondLiteral());
      clauses_propagator_->LazyDetach(clause);
      ++num_binary;

      // Tricky: AddBinaryClauseInternal() might fix literal if there is some
      // unprocessed equivalent literal, and the binary clause turn out to be
      // unary. This shouldn't happen otherwise the logic of
      // RemoveFixedLiteralsAndTestIfTrue() might fail.
      //
      // TODO(user): This still happen in SAT22.Carry_Save_Fast_1.cnf.cnf.xz,
      // it might not directly lead to a bug, but should still be fixed.
      DCHECK_EQ(trail_->Index(), saved_index);
      continue;
    }
  }

  // Note that we will only delete the clauses during the next database cleanup.
  clauses_propagator_->CleanUpWatchers();
  if (num_detached_clauses > 0 || num_binary > 0) {
    VLOG(1) << trail_->Index() << " fixed variables at level 0. " << "Detached "
            << num_detached_clauses << " clauses. " << num_binary
            << " converted to binary.";
  }

  // We also clean the binary implication graph.
  // Tricky: If we added the first binary clauses above, the binary graph
  // is not in "propagated" state as it should be, so we call Propagate() so
  // all the checks are happy.
  CHECK(binary_implication_graph_->Propagate(trail_));
  binary_implication_graph_->RemoveFixedVariables();
  num_processed_fixed_variables_ = trail_->Index();
  deterministic_time_of_last_fixed_variables_cleanup_ = deterministic_time();
}

bool SatSolver::PropagationIsDone() const {
  for (SatPropagator* propagator : propagators_) {
    if (propagator->IsEmpty()) continue;
    if (!propagator->PropagationIsDone(*trail_)) return false;
  }
  return true;
}

// TODO(user): Support propagating only the "first" propagators. That can
// be useful for probing/in-processing, so we can control if we do only the SAT
// part or the full integer part...
bool SatSolver::Propagate() {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(!ModelIsUnsat());

  while (true) {
    // Because we might potentially iterate often on this list below, we remove
    // empty propagators.
    //
    // TODO(user): This might not really be needed.
    non_empty_propagators_.clear();
    for (SatPropagator* propagator : propagators_) {
      if (!propagator->IsEmpty()) {
        non_empty_propagators_.push_back(propagator);
      }
    }

    while (true) {
      // The idea here is to abort the inspection as soon as at least one
      // propagation occurs so we can loop over and test again the highest
      // priority constraint types using the new information.
      //
      // Note that the first propagators_ should be the
      // binary_implication_graph_ and that its Propagate() functions will not
      // abort on the first propagation to be slightly more efficient.
      const int old_index = trail_->Index();
      for (SatPropagator* propagator : non_empty_propagators_) {
        DCHECK(propagator->PropagatePreconditionsAreSatisfied(*trail_));
        if (!propagator->Propagate(trail_)) return false;
        if (trail_->Index() > old_index) break;
      }
      if (trail_->Index() == old_index) break;
    }

    // In some corner cases, we might add new constraint during propagation,
    // which might trigger new propagator addition or some propagator to become
    // non-empty() now.
    if (PropagationIsDone()) return true;
  }
  return true;
}

void SatSolver::InitializePropagators() {
  propagators_.clear();
  propagators_.push_back(binary_implication_graph_);
  propagators_.push_back(clauses_propagator_);
  propagators_.push_back(enforcement_propagator_);
  propagators_.push_back(pb_constraints_);
  for (int i = 0; i < external_propagators_.size(); ++i) {
    propagators_.push_back(external_propagators_[i]);
  }
  if (last_propagator_ != nullptr) {
    propagators_.push_back(last_propagator_);
  }
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
    result.append(absl::StrFormat("%s(%s)", literal.DebugString(), value));
  }
  return result;
}

int SatSolver::ComputeMaxTrailIndex(absl::Span<const Literal> clause) const {
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
  const int64_t conflict_id = counters_.num_failures;

  // This will be used to mark all the literals inspected while we process the
  // conflict and the reasons behind each of its variable assignments.
  is_marked_.ClearAndResize(num_variables_);

  conflict->clear();
  reason_used_to_infer_the_conflict->clear();
  subsumed_clauses->clear();
  if (max_trail_index == -1) return;

  absl::Span<const Literal> clause_to_expand = trail_->FailingClause();

  // max_trail_index is the maximum trail index appearing in the failing_clause
  // and its level (Which is almost always equals to the CurrentDecisionLevel(),
  // except for symmetry propagation).
  DCHECK_EQ(max_trail_index, ComputeMaxTrailIndex(trail_->FailingClause()));
  int trail_index = max_trail_index;
  int highest_level = trail_->Info((*trail_)[max_trail_index].Variable()).level;
  if (trail_->ChronologicalBacktrackingEnabled()) {
    for (const Literal literal : clause_to_expand) {
      highest_level =
          std::max(highest_level, AssignmentLevel(literal.Variable()));
    }
  }
  if (highest_level == 0) return;

  // To find the 1-UIP conflict clause, we start by the failing_clause, and
  // expand each of its literal using the reason for this literal assignment to
  // false. The is_marked_ set allow us to never expand the same literal twice.
  //
  // The expansion is not done (i.e. stop) for literals that were assigned at a
  // decision level below the current one. If the level of such literal is not
  // zero, it is added to the conflict clause.
  //
  // Now, the trick is that we use the trail to expand the literal of the
  // current level in a very specific order. Namely the reverse order of the one
  // in which they were inferred. We stop as soon as
  // num_literal_at_highest_level_that_needs_to_be_processed is exactly one.
  //
  // This last literal will be the first UIP because by definition all the
  // propagation done at the current level will pass though it at some point.
  SatClause* sat_clause = trail_->FailingSatClause();
  DCHECK(!clause_to_expand.empty());
  int num_literal_at_highest_level_that_needs_to_be_processed = 0;
  while (true) {
    int num_new_vars_at_positive_level = 0;
    int num_vars_at_positive_level_in_clause_to_expand = 0;
    for (const Literal literal : clause_to_expand) {
      const BooleanVariable var = literal.Variable();
      const int level = AssignmentLevel(var);
      if (level == 0) continue;
      DCHECK_LE(level, highest_level);
      ++num_vars_at_positive_level_in_clause_to_expand;
      if (!is_marked_[var]) {
        is_marked_.Set(var);
        ++num_new_vars_at_positive_level;
        if (level == highest_level) {
          ++num_literal_at_highest_level_that_needs_to_be_processed;
        } else {
          // Note that all these literals are currently false since the clause
          // to expand was used to infer the value of a literal at this level.
          DCHECK(trail_->Assignment().LiteralIsFalse(literal));
          conflict->push_back(literal);
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
    while (
        !is_marked_[(*trail_)[trail_index].Variable()] ||
        (trail_->ChronologicalBacktrackingEnabled() &&
         AssignmentLevel((*trail_)[trail_index].Variable()) < highest_level)) {
      --trail_index;
      DCHECK_GE(trail_index, 0);
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
      clause_to_expand = trail_->Reason(literal.Variable(), conflict_id);
    }
    sat_clause = ReasonClauseOrNull(literal.Variable());

    --num_literal_at_highest_level_that_needs_to_be_processed;
    --trail_index;
  }
}

void SatSolver::ComputeUnionOfReasons(absl::Span<const Literal> input,
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
      if (parameters_->minimize_reduction_during_pb_resolution()) {
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
      const int current_level = AssignmentLevel(var);
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
      if (i < 0 || AssignmentLevel((*trail_)[i].Variable()) < current_level) {
        backjump_level = i < 0 ? 0 : AssignmentLevel((*trail_)[i].Variable());
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
        if (!parameters_->minimize_reduction_during_pb_resolution()) {
          conflict->ReduceCoefficients();
        }
      } else {
        // TODO(user): The function below can take most of the running time on
        // some instances. The goal is to have slack updated to its new value
        // incrementally, but we are not here yet.
        if (parameters_->minimize_reduction_during_pb_resolution()) {
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
  if (!parameters_->minimize_reduction_during_pb_resolution()) {
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
        AssignmentLevel(var) > backjump_level) {
      max_coeff_for_ge_level[backjump_level + 1] =
          std::max(max_coeff_for_ge_level[backjump_level + 1], coeff);
    } else {
      const int level = AssignmentLevel(var);
      if (trail_->Assignment().LiteralIsTrue(conflict->GetLiteral(var))) {
        sum_for_le_level[level] += coeff;
      }
      max_coeff_for_ge_level[level] =
          std::max(max_coeff_for_ge_level[level], coeff);
    }
  }

  // Compute the cumulative version.
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

void SatSolver::MinimizeConflict(std::vector<Literal>* conflict) {
  SCOPED_TIME_STAT(&stats_);

  const int old_size = conflict->size();
  switch (parameters_->minimization_algorithm()) {
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
// Note that because of the assignment structure, there is no need to process
// the literals of the conflict in order. While exploring the reason for a
// literal assignment, there will be no cycles.
void SatSolver::MinimizeConflictSimple(std::vector<Literal>* conflict) {
  SCOPED_TIME_STAT(&stats_);
  const int current_level = CurrentDecisionLevel();

  // Note that is_marked_ is already initialized and that we can start at 1
  // since the first literal of the conflict is the 1-UIP literal.
  int index = 1;
  for (int i = 1; i < conflict->size(); ++i) {
    const BooleanVariable var = (*conflict)[i].Variable();
    bool can_be_removed = false;
    if (AssignmentLevel(var) != current_level) {
      // It is important not to call Reason(var) when it can be avoided.
      const absl::Span<const Literal> reason = trail_->Reason(var);
      if (!reason.empty()) {
        can_be_removed = true;
        for (Literal literal : reason) {
          if (AssignmentLevel(literal.Variable()) == 0) continue;
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
// reason and so on. The recursion loops until we show that the initial literal
// can be infered from the conflict variables alone, or if we show that this is
// not the case. The result of any variable expansion will be cached in order
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
    const int level = AssignmentLevel(var);
    min_trail_index_per_level_[level] = std::min(
        min_trail_index_per_level_[level], trail_->Info(var).trail_index);
  }

  // Remove the redundant variable from the conflict. That is the ones that can
  // be infered by some other variables in the conflict.
  // Note that we can skip the first position since this is the 1-UIP.
  int index = 1;
  TimeLimitCheckEveryNCalls time_limit_check(100, time_limit_);
  for (int i = 1; i < conflict->size(); ++i) {
    const BooleanVariable var = (*conflict)[i].Variable();
    const AssignmentInfo& info = trail_->Info(var);
    if (time_limit_check.LimitReached() ||
        info.type == AssignmentType::kSearchDecision ||
        info.trail_index <= min_trail_index_per_level_[info.level] ||
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
      min_trail_index_per_level_[AssignmentLevel(var)] =
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
  for (const Literal literal : trail_->Reason(variable)) {
    const BooleanVariable var = literal.Variable();
    DCHECK_NE(var, variable);
    if (is_marked_[var]) continue;
    const AssignmentInfo& info = trail_->Info(var);
    if (info.level == 0) {
      // Note that this is not needed if the solver is not configured to produce
      // an unsat proof. However, the (level == 0) test should always be false
      // in this case because there will never be literals of level zero in any
      // reason when we don't want a proof.
      is_marked_.Set(var);
      continue;
    }
    if (info.trail_index <= min_trail_index_per_level_[info.level] ||
        info.type == AssignmentType::kSearchDecision || is_independent_[var]) {
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
      DCHECK_NE(var, current_var) << trail_->Info(var).DebugString()
                                  << " old: " << trail_->AssignmentType(var);
      const AssignmentInfo& info = trail_->Info(var);
      if (info.level == 0 || is_marked_[var]) continue;
      if (info.trail_index <= min_trail_index_per_level_[info.level] ||
          info.type == AssignmentType::kSearchDecision ||
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
    const int level = AssignmentLevel(var);
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
    const absl::Span<const Literal> reason = trail_->Reason(var);
    if (reason.empty()) continue;

    // Compute how many and which literals from the current reason do not appear
    // in the current conflict. Level 0 literals are ignored.
    std::vector<Literal> not_contained_literals;
    for (const Literal reason_literal : reason) {
      const BooleanVariable reason_var = reason_literal.Variable();

      // We ignore level 0 variables.
      if (AssignmentLevel(reason_var) == 0) continue;

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

void SatSolver::CleanClauseDatabaseIfNeeded() {
  if (num_learned_clause_before_cleanup_ > 0) return;
  SCOPED_TIME_STAT(&stats_);

  // Creates a list of clauses that can be deleted. Note that only the clauses
  // that appear in clauses_info can potentially be removed.
  typedef std::pair<SatClause*, ClauseInfo> Entry;
  std::vector<Entry> entries;
  auto& clauses_info = *(clauses_propagator_->mutable_clauses_info());
  for (auto& entry : clauses_info) {
    if (ClauseIsUsedAsReason(entry.first)) continue;
    if (entry.second.protected_during_next_cleanup) {
      entry.second.protected_during_next_cleanup = false;
      continue;
    }
    entries.push_back(entry);
  }
  const int num_protected_clauses = clauses_info.size() - entries.size();

  if (parameters_->clause_cleanup_ordering() == SatParameters::CLAUSE_LBD) {
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
  int num_kept_clauses =
      (parameters_->clause_cleanup_target() > 0)
          ? std::min(static_cast<int>(entries.size()),
                     parameters_->clause_cleanup_target())
          : static_cast<int>(parameters_->clause_cleanup_ratio() *
                             static_cast<double>(entries.size()));

  int num_deleted_clauses = entries.size() - num_kept_clauses;

  // Tricky: Because the order of the clauses_info iteration is NOT
  // deterministic (pointer keys), we also keep all the clauses which have the
  // same LBD and activity as the last one so the behavior is deterministic.
  if (num_kept_clauses > 0) {
    while (num_deleted_clauses > 0) {
      const ClauseInfo& a = entries[num_deleted_clauses].second;
      const ClauseInfo& b = entries[num_deleted_clauses - 1].second;
      if (a.activity != b.activity || a.lbd != b.lbd) break;
      --num_deleted_clauses;
      ++num_kept_clauses;
    }
  }
  if (num_deleted_clauses > 0) {
    entries.resize(num_deleted_clauses);
    for (const Entry& entry : entries) {
      SatClause* clause = entry.first;
      counters_.num_literals_forgotten += clause->size();
      clauses_propagator_->LazyDetach(clause);
    }
    clauses_propagator_->CleanUpWatchers();

    // TODO(user): If the need arise, we could avoid this linear scan on the
    // full list of clauses by not keeping the clauses from clauses_info there.
    if (!block_clause_deletion_) {
      clauses_propagator_->DeleteRemovedClauses();
    }
  }

  num_learned_clause_before_cleanup_ = parameters_->clause_cleanup_period();
  VLOG(1) << "Database cleanup, #protected:" << num_protected_clauses
          << " #kept:" << num_kept_clauses
          << " #deleted:" << num_deleted_clauses;
}

std::string SatStatusString(SatSolver::Status status) {
  switch (status) {
    case SatSolver::ASSUMPTIONS_UNSAT:
      return "ASSUMPTIONS_UNSAT";
    case SatSolver::INFEASIBLE:
      return "INFEASIBLE";
    case SatSolver::FEASIBLE:
      return "FEASIBLE";
    case SatSolver::LIMIT_REACHED:
      return "LIMIT_REACHED";
  }
  // Fallback. We don't use "default:" so the compiler will return an error
  // if we forgot one enum case above.
  LOG(DFATAL) << "Invalid SatSolver::Status " << status;
  return "UNKNOWN";
}

void MinimizeCore(SatSolver* solver, std::vector<Literal>* core) {
  std::vector<Literal> result;
  if (!solver->ResetToLevelZero()) return;
  for (const Literal lit : *core) {
    if (solver->Assignment().LiteralIsTrue(lit)) continue;
    result.push_back(lit);
    if (solver->Assignment().LiteralIsFalse(lit)) break;
    if (!solver->EnqueueDecisionIfNotConflicting(lit)) break;
  }
  if (result.size() < core->size()) {
    VLOG(1) << "minimization " << core->size() << " -> " << result.size();
    *core = result;
  }
}

}  // namespace sat
}  // namespace operations_research
