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
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/timer.h"
#include "ortools/port/proto_utils.h"
#include "ortools/port/sysinfo.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/enforcement.h"
#include "ortools/sat/lrat_proof_handler.h"
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
      lrat_proof_handler_(model->Mutable<LratProofHandler>()),
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
}

int64_t SatSolver::num_branches() const { return counters_.num_branches; }

int64_t SatSolver::num_failures() const { return counters_.num_failures; }

int64_t SatSolver::num_propagations() const {
  return trail_->NumberOfEnqueues() - counters_.num_branches;
}

int64_t SatSolver::num_backtracks() const { return counters_.num_backtracks; }

int64_t SatSolver::num_restarts() const { return counters_.num_restarts; }
int64_t SatSolver::num_backtracks_to_root() const {
  return counters_.num_backtracks_to_root;
}

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
  const std::optional<uint64_t> memory_usage =
      ::operations_research::sysinfo::MemoryUsageProcess();
  if (!memory_usage.has_value()) return false;
  constexpr uint64_t kMegaByte = 1024 * 1024;
  return *memory_usage > kMegaByte * parameters_->max_memory_in_mb();
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

  // Just disable propagation for these clauses with the new conflict
  // resolution. TODO(user): we should probably just never propagate.
  if (parameters_->use_new_integer_conflict_resolution()) return true;

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

namespace {
// Creates a clause pointer for the given literals. If there are more than 2
// literals, allocates a SatClause and returns its pointer.
ClausePtr NewClausePtr(absl::Span<const Literal> literals) {
  switch (literals.size()) {
    case 0:
      return ClausePtr::EmptyClausePtr();
    case 1:
      return ClausePtr(literals[0]);
    case 2:
      return ClausePtr(literals[0], literals[1]);
    default:
      return ClausePtr(SatClause::Create(literals));
  }
}
}  // namespace

// Note that we will do a bit of presolve here, which might not always be
// necessary if we know we are already adding a "clean" clause with no
// duplicates or literal equivalent to others. However, we found that it is
// better to make sure we always have "clean" clause in the solver rather than
// to over-optimize this. In particular, presolve might be disabled or
// incomplete, so such unclean clause might find their way here.
bool SatSolver::AddProblemClause(absl::Span<const Literal> literals,
                                 int64_t one_based_cnf_index) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK_EQ(CurrentDecisionLevel(), 0);
  if (model_is_unsat_) return false;

  auto maybe_delete_lrat_problem_clause = [&]() {
    if (lrat_proof_handler_ != nullptr && one_based_cnf_index > 0) {
      const ClausePtr clause = ClausePtr(literals);
      lrat_proof_handler_->AddProblemClause(clause, one_based_cnf_index);
      lrat_proof_handler_->DeleteClause(clause);
    }
  };

  // Filter already assigned literals. Note that we also remap literal in case
  // we discovered equivalence later in the search.
  tmp_literals_.clear();
  if (lrat_proof_handler_ != nullptr) {
    tmp_proof_.clear();
    for (const Literal l : literals) {
      const Literal rep = binary_implication_graph_->RepresentativeOf(l);
      if (trail_->Assignment().LiteralIsTrue(rep)) {
        maybe_delete_lrat_problem_clause();
        return true;
      }
      if (trail_->Assignment().LiteralIsFalse(l)) {
        tmp_proof_.push_back(ClausePtr(l.Negated()));
        continue;
      }
      if (trail_->Assignment().LiteralIsFalse(rep)) {
        tmp_proof_.push_back(ClausePtr(rep.Negated()));
      }
      if (rep != l) {
        tmp_proof_.push_back(ClausePtr(l.Negated(), rep));
      }
      if (!trail_->Assignment().LiteralIsFalse(rep)) {
        tmp_literals_.push_back(rep);
      }
    }
  } else {
    for (const Literal l : literals) {
      const Literal rep = binary_implication_graph_->RepresentativeOf(l);
      if (trail_->Assignment().LiteralIsTrue(rep)) return true;
      if (trail_->Assignment().LiteralIsFalse(rep)) continue;
      tmp_literals_.push_back(rep);
    }
  }

  // A clause with l and not(l) is trivially true.
  gtl::STLSortAndRemoveDuplicates(&tmp_literals_);
  for (int i = 0; i + 1 < tmp_literals_.size(); ++i) {
    if (tmp_literals_[i] == tmp_literals_[i + 1].Negated()) {
      maybe_delete_lrat_problem_clause();
      return true;
    }
  }
  ClausePtr clause = kNullClausePtr;
  if (lrat_proof_handler_ != nullptr) {
    clause = NewClausePtr(tmp_literals_);
    // Add the original problem clause.
    const ClausePtr original_clause =
        tmp_proof_.empty() ? clause : NewClausePtr(literals);
    if (one_based_cnf_index > 0) {
      lrat_proof_handler_->AddProblemClause(original_clause,
                                            one_based_cnf_index);
    } else {
      lrat_proof_handler_->AddImportedClause(original_clause);
    }
    // If the filtered clause is different, add it (with proof), and delete the
    // original one.
    if (!tmp_proof_.empty()) {
      tmp_proof_.push_back(original_clause);
      lrat_proof_handler_->AddInferredClause(clause, tmp_proof_);
      lrat_proof_handler_->DeleteClause(original_clause);
    }
  }

  return AddProblemClauseInternal(clause, tmp_literals_);
}

bool SatSolver::AddProblemClauseInternal(ClausePtr ptr,
                                         absl::Span<const Literal> literals) {
  SCOPED_TIME_STAT(&stats_);
  if (DEBUG_MODE && CurrentDecisionLevel() == 0) {
    for (const Literal l : literals) {
      CHECK(!trail_->Assignment().LiteralIsAssigned(l));
    }
  }

  if (literals.empty()) return SetModelUnsat();

  if (literals.size() == 1) {
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
    SatClause* clause =
        ptr.IsSatClausePtr() ? ptr.GetSatClause() : SatClause::Create(literals);
    if (!clauses_propagator_->AddClause(clause, trail_,
                                        /*lbd=*/-1)) {
      return SetModelUnsat();
    }
  }

  // Tricky: The PropagationIsDone() condition shouldn't change anything for a
  // pure SAT problem, however in the CP-SAT context, calling Propagate() can
  // tigger computation (like the LP) even if no domain changed since the last
  // call. We do not want to do that.
  if (!PropagationIsDone() && !Propagate()) {
    // This adds the UNSAT proof to the LRAT handler, if any.
    ProcessCurrentConflict();
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
      tmp_literals_.clear();
      for (const Literal& literal : enforcement_literals) {
        tmp_literals_.push_back(literal.Negated());
      }
      return AddProblemClauseInternal(kNullClausePtr, tmp_literals_);
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
      tmp_literals_.clear();
      for (const LiteralWithCoeff& term : cst) {
        tmp_literals_.push_back(term.literal.Negated());
      }
      return AddProblemClauseInternal(kNullClausePtr, tmp_literals_);
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
    tmp_literals_.clear();
    for (const LiteralWithCoeff& term : cst) {
      tmp_literals_.push_back(term.literal);
    }
    if (!binary_implication_graph_->AddAtMostOne(tmp_literals_)) {
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
    ClausePtr ptr, absl::Span<const Literal> literals, bool is_redundant,
    int min_lbd_of_subsumed_clauses) {
  SCOPED_TIME_STAT(&stats_);

  // Note that we might learn more than one conflict per "failure" actually.
  // TODO(user): this should be called num_conflicts.
  ++counters_.num_failures;

  if (literals.size() == 1) {
    // Corner case where we "learn" more than one conflict.
    if (Assignment().LiteralIsTrue(literals[0])) return 1;
    CHECK(!Assignment().LiteralIsFalse(literals[0]));

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
      // This clause MUST be new, otherwise something is wrong.
      CHECK(binary_clauses_.Add(BinaryClause(literals[0], literals[1])));
    }
    CHECK(binary_implication_graph_->AddBinaryClause(literals[0], literals[1]));
    return /*lbd=*/2;
  }

  CleanClauseDatabaseIfNeeded();

  // Important: Even though the only literal at the last decision level has
  // been unassigned, its level was not modified, so ComputeLbd() works.
  SatClause* clause =
      ptr.IsSatClausePtr() ? ptr.GetSatClause() : SatClause::Create(literals);
  const int lbd = std::min(min_lbd_of_subsumed_clauses, ComputeLbd(literals));
  if (is_redundant && lbd > parameters_->clause_cleanup_lbd_bound()) {
    --num_learned_clause_before_cleanup_;

    clauses_propagator_->AddRemovableClause(clause, trail_, lbd);
    BumpClauseActivity(clause);
  } else {
    CHECK(clauses_propagator_->AddClause(clause, trail_, lbd));
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

int SatSolver::EnqueueDecisionAndBackjumpOnConflict(
    Literal true_literal, std::optional<ConflictCallback> callback) {
  SCOPED_TIME_STAT(&stats_);
  if (model_is_unsat_) return kUnsatTrailIndex;
  DCHECK(PropagationIsDone());

  // We should never enqueue before the assumptions_.
  if (DEBUG_MODE && !assumptions_.empty()) {
    CHECK_GE(trail_->CurrentDecisionLevel(), assumption_level_);
  }

  EnqueueNewDecision(true_literal);
  if (!FinishPropagation(callback)) return kUnsatTrailIndex;
  DCHECK(PropagationIsDone());
  return last_decision_or_backtrack_trail_index_;
}

bool SatSolver::FinishPropagation(std::optional<ConflictCallback> callback) {
  if (model_is_unsat_) return false;
  int num_loop = 0;
  while (true) {
    const int old_decision_level = trail_->CurrentDecisionLevel();
    if (!Propagate()) {
      ProcessCurrentConflict(callback);
      if (model_is_unsat_) return false;
      if (trail_->CurrentDecisionLevel() == old_decision_level) {
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
  // many binary clauses as possible. This is because we do not want to miss any
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

  DCHECK(PropagationIsDone());
  while (CurrentDecisionLevel() == 0 && !assumptions_.empty()) {
    // When assumptions_ is not empty, the first "decision" actually contains
    // multiple ones, and we should never use its literal.
    CHECK_EQ(trail_->CurrentDecisionLevel(), 0);
    last_decision_or_backtrack_trail_index_ = trail_->Index();

    // We enqueue all assumptions at once at decision level 1.
    int num_decisions = 0;
    for (const Literal lit : assumptions_) {
      if (Assignment().LiteralIsTrue(lit)) continue;
      if (Assignment().LiteralIsFalse(lit)) {
        // See GetLastIncompatibleDecisions().
        *trail_->MutableConflict() = {lit.Negated(), lit};
        return false;
      }
      ++num_decisions;
      trail_->EnqueueAssumption(lit);
    }

    // Corner case: all assumptions are fixed at level zero, we ignore them.
    if (num_decisions == 0) {
      return ResetToLevelZero();
    }

    // Now that everything is enqueued, we propagate.
    // This can backjump if we learn a unit clause, so we must loop.
    if (!FinishPropagation()) return false;
    if (CurrentDecisionLevel() > 0) return true;
  }

  DCHECK(PropagationIsDone());
  DCHECK(assumptions_.empty());
  const int64_t old_num_branches = counters_.num_branches;
  const SatSolver::Status status = ReapplyDecisionsUpTo(assumption_level_ - 1);
  counters_.num_branches = old_num_branches;
  assumption_level_ = CurrentDecisionLevel();
  return (status == SatSolver::FEASIBLE);
}

void SatSolver::ProcessCurrentConflict(
    std::optional<ConflictCallback> callback) {
  SCOPED_TIME_STAT(&stats_);
  if (model_is_unsat_) return;

  const int conflict_trail_index = trail_->Index();

  // A conflict occurred, compute a nice reason for this failure.
  //
  // If the trail as a registered "higher level conflict resolution", pick
  // this one instead.
  learned_conflict_.clear();
  same_reason_identifier_.Clear();

  // This is used by ComputeFirstUIPConflict().
  subsuming_lrat_index_.clear();
  subsuming_clauses_.clear();
  subsuming_groups_.clear();
  subsumed_clauses_.clear();

  if (trail_->GetConflictResolutionFunction() == nullptr) {
    const int max_trail_index = ComputeMaxTrailIndex(trail_->FailingClause());
    if (!assumptions_.empty() && !trail_->FailingClause().empty()) {
      // If the failing clause only contains literal at the assumptions level,
      // we cannot use the ComputeFirstUIPConflict() code as we might have more
      // than one decision.
      //
      // TODO(user): We might still want to "learn" the clause, especially if
      // it reduces to only one literal in which case we can just fix it.
      int highest_level = 0;
      for (const Literal l : trail_->FailingClause()) {
        highest_level =
            std::max<int>(highest_level, trail_->Info(l.Variable()).level);
      }
      if (highest_level == assumption_level_) return;
    }

    ComputeFirstUIPConflict(max_trail_index, &learned_conflict_,
                            &reason_used_to_infer_the_conflict_);
  } else {
    trail_->GetConflictResolutionFunction()(
        &learned_conflict_, &reason_used_to_infer_the_conflict_);
    if (!assumptions_.empty() && !learned_conflict_.empty() &&
        AssignmentLevel(learned_conflict_[0].Variable()) <= assumption_level_) {
      // We have incompatible assumptions, store them there and return.
      *trail_->MutableConflict() = learned_conflict_;
      return;
    }

    CHECK(IsConflictValid(learned_conflict_));

    // Recompute is_marked_.
    is_marked_.ClearAndResize(num_variables_);
    for (const Literal l : learned_conflict_) {
      is_marked_.Set(l.Variable());
    }
    for (const Literal l : reason_used_to_infer_the_conflict_) {
      is_marked_.Set(l.Variable());
    }
  }

  DCHECK(IsConflictValid(learned_conflict_));
  DCHECK(ClauseIsValidUnderDebugAssignment(learned_conflict_));

  std::vector<ClausePtr>* proof_for_1iup = &tmp_proof_for_1uip_;
  if (lrat_proof_handler_ != nullptr) {
    proof_for_1iup->clear();
    AppendLratProofFromReasons(reason_used_to_infer_the_conflict_,
                               proof_for_1iup);
    AppendLratProofForFailingClause(proof_for_1iup);
  }

  // An empty conflict means that the problem is UNSAT.
  if (learned_conflict_.empty()) {
    ClausePtr clause = kNullClausePtr;
    if (lrat_proof_handler_ != nullptr) {
      if (!lrat_proof_handler_->AddInferredClause(ClausePtr::EmptyClausePtr(),
                                                  *proof_for_1iup)) {
        VLOG(1) << "WARNING: invalid LRAT inferred clause!";
      }
    }
    if (callback.has_value()) {
      (*callback)(clause, learned_conflict_);
    }
    return (void)SetModelUnsat();
  }

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
  //
  // Note that the activity of the learned clause will be bumped too by
  // AddLearnedClauseAndEnqueueUnitPropagation() after we update the increment.
  if (trail_->FailingSatClause() != nullptr) {
    BumpClauseActivity(trail_->FailingSatClause());
  }
  BumpReasonActivities(reason_used_to_infer_the_conflict_);
  UpdateClauseActivityIncrement();

  // Decay the activities.
  decision_policy_->UpdateVariableActivityIncrement();
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
    const int max_trail_index = ComputeMaxTrailIndex(trail_->FailingClause());
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
  std::vector<ClausePtr>* proof_for_minimization = &tmp_proof_for_minimization_;
  proof_for_minimization->clear();
  // This resizes internal data structures which can be used by
  // MinimizeConflict() even if the binary implication graph is empty.
  binary_implication_graph_->ClearImpliedLiterals();
  DCHECK(ClauseIsValidUnderDebugAssignment(learned_conflict_));
  if (!binary_implication_graph_->IsEmpty()) {
    switch (parameters_->binary_minimization_algorithm()) {
      case SatParameters::NO_BINARY_MINIMIZATION:
        break;
      case SatParameters::BINARY_MINIMIZATION_FROM_UIP:
        binary_implication_graph_->MinimizeConflictFirst(
            *trail_, &learned_conflict_, &is_marked_, proof_for_minimization,
            /*also_use_decisions=*/false);
        break;
      case SatParameters::BINARY_MINIMIZATION_FROM_UIP_AND_DECISIONS:
        binary_implication_graph_->MinimizeConflictFirst(
            *trail_, &learned_conflict_, &is_marked_, proof_for_minimization,
            /*also_use_decisions=*/true);
        break;
    }
    DCHECK(IsConflictValid(learned_conflict_));
  }

  // Minimize the learned conflict.
  MinimizeConflict(&learned_conflict_, proof_for_minimization);

  // Note that we need to output the learned clause before cleaning the clause
  // database. This is because we already backtracked and some of the clauses
  // that were needed to infer the conflict may not be "reasons" anymore and
  // may be deleted.
  ClausePtr learned_conflict_clause = kNullClausePtr;
  if (lrat_proof_handler_ != nullptr) {
    if (!proof_for_minimization->empty()) {
      // Concatenate the minimized conflict proof with the learned conflict
      // proof, in this order, and remove duplicate clause pointers.
      tmp_clauses_set_.clear();
      tmp_clauses_set_.insert(proof_for_minimization->begin(),
                              proof_for_minimization->end());
      proof_for_minimization->reserve(proof_for_minimization->size() +
                                      proof_for_1iup->size());
      for (const ClausePtr clause : *proof_for_1iup) {
        if (!tmp_clauses_set_.contains(clause)) {
          proof_for_minimization->push_back(clause);
        }
      }
      proof_for_1iup = proof_for_minimization;
    }
    learned_conflict_clause = NewClausePtr(learned_conflict_);
    if (!lrat_proof_handler_->AddInferredClause(learned_conflict_clause,
                                                *proof_for_1iup)) {
      VLOG(1) << "WARNING: invalid LRAT inferred clause!";
    }
  }
  if (callback.has_value()) {
    (*callback)(learned_conflict_clause, learned_conflict_);
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

  // Note that this should happen after the new_conflict "proof", but before
  // we backtrack and add the new conflict to the clause_propagator_.
  const auto [is_redundant, min_lbd_of_subsumed_clauses] =
      SubsumptionsInConflictResolution(learned_conflict_clause,
                                       learned_conflict_,
                                       reason_used_to_infer_the_conflict_);

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

  // Add the conflict here, so we process all "newly learned" clause in the
  // same way.
  learned_clauses_.push_back({learned_conflict_clause, is_redundant,
                              min_lbd_of_subsumed_clauses,
                              std::move(learned_conflict_)});

  // Preprocess the new clauses.
  // We might need to backtrack further !
  for (auto& [clause, is_redundant, min_lbd, literals] : learned_clauses_) {
    if (literals.empty()) return (void)SetModelUnsat();

    // Make sure each clause is "canonicalized" with respect to equivalent
    // literals.
    //
    // TODO(user): Maybe we should do that on each reason before we use them in
    // conflict analysis/minimization, but it might be a bit costly.
    bool some_change = false;
    tmp_proof_.clear();
    for (Literal& lit : literals) {
      const Literal rep = binary_implication_graph_->RepresentativeOf(lit);
      if (rep != lit) {
        some_change = true;
        if (lrat_proof_handler_ != nullptr) {
          // We need not(rep) => not(lit) for the proof.
          tmp_proof_.push_back(ClausePtr(lit.Negated(), rep));
        }
        lit = rep;
      }
    }
    if (some_change) {
      gtl::STLSortAndRemoveDuplicates(&literals);

      // This shouldn't happen since it is a new learned clause, otherwise
      // something is wrong.
      for (int i = 1; i < literals.size(); ++i) {
        CHECK_NE(literals[i], literals[i - 1].Negated())
            << "trivial new clause?";
      }
      // We need a new clause for the canonicalized version, and the proof for
      // how we derived that canonicalization.
      const ClausePtr new_clause = NewClausePtr(literals);
      if (lrat_proof_handler_ != nullptr) {
        DCHECK_NE(new_clause, clause);
        tmp_proof_.push_back(clause);
        lrat_proof_handler_->AddInferredClause(new_clause, tmp_proof_);
        lrat_proof_handler_->DeleteClause(clause,
                                          /*delete_sat_clause=*/false);
      }
      if (clause.IsSatClausePtr()) {
        delete clause.GetSatClause();
      }
      clause = new_clause;
    }

    // Tricky: in case of propagation not at the right level we might need to
    // backjump further.
    int num_false = 0;
    for (const Literal l : literals) {
      if (Assignment().LiteralIsFalse(l)) ++num_false;
    }
    if (num_false == literals.size() || literals.size() == 1) {
      int max_level = 0;
      for (const Literal l : literals) {
        const int level = AssignmentLevel(l.Variable());
        max_level = std::max(max_level, level);
      }
      int propag_level = 0;
      for (const Literal l : literals) {
        const int level = AssignmentLevel(l.Variable());
        if (level < max_level) {
          propag_level = std::max(propag_level, level);
        }
      }
      Backtrack(propag_level);
    }
  }

  // Learn the new clauses.
  int best_lbd = std::numeric_limits<int>::max();
  for (const auto& [clause, is_redundant, min_lbd, literals] :
       learned_clauses_) {
    DCHECK((lrat_proof_handler_ == nullptr) || (clause != kNullClausePtr));
    const int lbd = AddLearnedClauseAndEnqueueUnitPropagation(
        clause, literals, is_redundant, min_lbd);
    best_lbd = std::min(best_lbd, lbd);
  }
  restart_->OnConflict(conflict_trail_index, conflict_level, best_lbd);
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

std::pair<bool, int> SatSolver::SubsumptionsInConflictResolution(
    ClausePtr learned_conflict, absl::Span<const Literal> conflict,
    absl::Span<const Literal> reason_used) {
  CHECK_NE(CurrentDecisionLevel(), 0);
  learned_clauses_.clear();

  // This is used to see if the learned conflict subsumes some clauses.
  // Note that conflict is not yet in the clauses_propagator_.
  tmp_literal_set_.Resize(Literal(num_variables_, true).Index());
  for (const Literal l : conflict) tmp_literal_set_.Set(l);
  const auto in_conflict = tmp_literal_set_.const_view();

  // This is used to see if the set of decision that implies the conflict
  // (further resolution) subsumes some clauses.
  //
  // TODO(user): Also consider the ALL UIP conflict ? I know other solver do
  // learn this version sometimes, or anything in-between. See the concept of
  // conflict "shrinking" in the literature.
  std::vector<SatClause*> subsumed_by_decisions;
  bool decision_is_redundant = true;
  int decision_min_lbd = std::numeric_limits<int>::max();
  int decisions_clause_size = 0;
  if (assumption_level_ == 0 &&
      parameters_->decision_subsumption_during_conflict_analysis()) {
    if (/* DISABLES CODE */ (false)) {
      // This is shorter but more costly... Note that if any subsumption occur,
      // this is the one we will use.
      for (const Literal l : GetDecisionsFixing(conflict)) {
        ++decisions_clause_size;
        tmp_decision_set_.Set(l.Negated());
      }
    } else {
      // Add all the decision up to max_non_decision_level + the one after that
      // from the conflict.
      tmp_decision_set_.Resize(Literal(num_variables_, true).Index());
      int max_non_decision_level = 0;
      for (const Literal l : conflict) {
        const auto& info = trail_->Info(l.Variable());
        if (info.type != AssignmentType::kSearchDecision) {
          max_non_decision_level =
              std::max<int>(max_non_decision_level, info.level);
        }
      }

      for (int i = 0; i < max_non_decision_level; ++i) {
        // To act like conflict.
        const Literal l = Decisions()[i].literal.Negated();
        ++decisions_clause_size;
        tmp_decision_set_.Set(l);
      }
      for (const Literal l : conflict) {
        const auto& info = trail_->Info(l.Variable());
        if (info.type == AssignmentType::kSearchDecision &&
            !tmp_decision_set_[l]) {
          ++decisions_clause_size;
          tmp_decision_set_.Set(l);
        }
      }
    }
  }

  // Deal with subsuming_groups_.
  // We need to infer the intermediary clause before we subsume them.
  //
  // TODO(user): We can use the intermediary step to shorten the conflict proof.
  ClausePtr last_clause = kNullClausePtr;
  int reason_index = 0;
  for (int i = 0; i < subsuming_groups_.size(); ++i) {
    // If the conflict subsume subsumed_clauses_[i], it will subsume all
    // the other clause too, so that will be covered below, and we don't need
    // to create that intermediary at all.
    const int limit = subsuming_clauses_[i].size() - conflict.size();
    int missing = 0;
    for (const Literal l : subsuming_clauses_[i]) {
      if (!in_conflict[l]) {
        ++missing;
        if (missing > limit) break;
      }
    }

    // Intermediary conflict is sumbsumed, skip.
    if (missing <= limit) continue;

    // Intermediary proof to reach this step in the conflict resolution.
    ClausePtr new_clause = NewClausePtr(subsuming_clauses_[i]);
    if (lrat_proof_handler_ != nullptr) {
      tmp_proof_.clear();
      is_marked_.ClearAndResize(num_variables_);  // Make sure not used anymore

      AppendLratProofFromReasons(
          reason_used.subspan(reason_index,
                              subsuming_lrat_index_[i] - reason_index),
          &tmp_proof_);
      if (last_clause == kNullClausePtr) {
        AppendLratProofForFailingClause(&tmp_proof_);
      } else {
        tmp_proof_.push_back(last_clause);
      }

      reason_index = subsuming_lrat_index_[i];
      lrat_proof_handler_->AddInferredClause(new_clause, tmp_proof_);
      last_clause = new_clause;
    }

    // Then this clause subsumes all entry in the group.
    bool new_clause_is_redundant = true;
    int new_clause_min_lbd = std::numeric_limits<int>::max();
    for (SatClause* clause : subsuming_groups_[i]) {
      CHECK_NE(clause->size(), 0);  // Not subsumed yet.
      if (clauses_propagator_->IsRemovable(clause)) {
        new_clause_min_lbd =
            std::min(new_clause_min_lbd,
                     clauses_propagator_->LbdOrZeroIfNotRemovable(clause));
      } else {
        new_clause_is_redundant = false;
      }
      DCHECK(ClauseSubsumption(subsuming_clauses_[i], clause));
      clauses_propagator_->LazyDelete(
          clause, DeletionSourceForStat::SUBSUMPTION_CONFLICT_EXTRA);
    }

    // We can only add them after backtracking, since these are currently
    // conflict.
    learned_clauses_.push_back(
        {new_clause, new_clause_is_redundant, new_clause_min_lbd,
         std::vector<Literal>(subsuming_clauses_[i].begin(),
                              subsuming_clauses_[i].end())});
  }

  bool is_redundant = true;
  int min_lbd_of_subsumed_clauses = std::numeric_limits<int>::max();
  const auto in_decision = tmp_decision_set_.const_view();
  const auto maybe_subsume = [&is_redundant, &min_lbd_of_subsumed_clauses,
                              in_conflict, conflict, in_decision,
                              decisions_clause_size, &subsumed_by_decisions,
                              &decision_is_redundant, &decision_min_lbd,
                              this](SatClause* clause,
                                    DeletionSourceForStat source) {
    if (clause == nullptr || clause->empty()) return;

    if (IsStrictlyIncluded(in_conflict, conflict.size(), clause->AsSpan())) {
      ++counters_.num_subsumed_clauses;
      DCHECK(ClauseSubsumption(conflict, clause));
      if (clauses_propagator_->IsRemovable(clause)) {
        min_lbd_of_subsumed_clauses =
            std::min(min_lbd_of_subsumed_clauses,
                     clauses_propagator_->LbdOrZeroIfNotRemovable(clause));
      } else {
        is_redundant = false;
      }
      clauses_propagator_->LazyDelete(clause, source);
      return;
    }

    if (decisions_clause_size > 0 &&
        IsStrictlyIncluded(in_decision, decisions_clause_size,
                           clause->AsSpan())) {
      if (clauses_propagator_->IsRemovable(clause)) {
        decision_min_lbd =
            std::min(decision_min_lbd,
                     clauses_propagator_->LbdOrZeroIfNotRemovable(clause));
      } else {
        decision_is_redundant = false;
      }
      subsumed_by_decisions.push_back(clause);
    }
  };

  // This is faster than conflict analysis, and stronger than the old assumption
  // mecanism we had. This is because once the conflict is minimized, we might
  // have more subsumptions than the one found during conflict analysis.
  //
  // Note however that we migth still have subsumption using the intermediate
  // conflict. See ComputeFirstUIPConflict().
  if (parameters_->subsumption_during_conflict_analysis()) {
    for (const Literal l : reason_used) {
      // Tricky: these clause might have been deleted by the subsumption above.
      // So ReasonClauseOrNull() must handle that case.
      maybe_subsume(clauses_propagator_->ReasonClauseOrNull(l.Variable()),
                    DeletionSourceForStat::SUBSUMPTION_CONFLICT);
    }
  }

  if (parameters_->eagerly_subsume_last_n_conflicts() > 0) {
    for (SatClause* clause : clauses_propagator_->LastNClauses(
             parameters_->eagerly_subsume_last_n_conflicts())) {
      maybe_subsume(clause, DeletionSourceForStat::SUBSUMPTION_EAGER);
    }
  }

  if (!subsumed_by_decisions.empty()) {
    // This one should always be a subset of the one we tried.
    std::vector<Literal> decision_clause;
    for (const Literal l : GetDecisionsFixing(conflict)) {
      DCHECK(in_decision[l.Negated()]);
      decision_clause.push_back(l.Negated());
    }

    // Construct the proof.
    ClausePtr new_clause = NewClausePtr(decision_clause);
    if (lrat_proof_handler_ != nullptr) {
      tmp_proof_.clear();
      clauses_propagator_->AppendClausesFixing(conflict, &tmp_proof_);
      tmp_proof_.push_back(learned_conflict);
      lrat_proof_handler_->AddInferredClause(new_clause, tmp_proof_);
    }

    // Remove subsumed clause.
    for (SatClause* clause : subsumed_by_decisions) {
      if (clause->empty()) continue;
      DCHECK(ClauseSubsumption(decision_clause, clause));
      clauses_propagator_->LazyDelete(
          clause, DeletionSourceForStat::SUBSUMPTION_DECISIONS);
    }

    // Also learn the "decision" conflict.
    learned_clauses_.push_back(
        {new_clause, decision_is_redundant, decision_min_lbd, decision_clause});
  }

  // Sparse clear.
  for (const Literal l : conflict) tmp_literal_set_.Clear(l);
  if (decisions_clause_size > 0) {
    for (int i = 0; i < CurrentDecisionLevel(); ++i) {
      tmp_decision_set_.Clear(Decisions()[i].literal.Negated());
    }
  }

  clauses_propagator_->CleanUpWatchers();
  return {is_redundant, min_lbd_of_subsumed_clauses};
}

void SatSolver::AppendLratProofForFixedLiterals(
    absl::Span<const Literal> literals, std::vector<ClausePtr>* proof) {
  for (const Literal literal : literals) {
    const BooleanVariable var = literal.Variable();
    if (!is_marked_[var] && trail_->AssignmentLevel(literal) == 0) {
      is_marked_.Set(var);
      DCHECK(trail_->Assignment().LiteralIsFalse(literal));
      proof->push_back(ClausePtr(literal.Negated()));
    }
  }
}

void SatSolver::AppendLratProofForFailingClause(std::vector<ClausePtr>* proof) {
  // Add all the non-yet marked unit-clause.
  AppendLratProofForFixedLiterals(trail_->FailingClause(), proof);

  // Add the failing SAT clause.
  ClausePtr failing_clause = kNullClausePtr;
  const SatClause* failing_sat_clause = trail_->FailingSatClause();
  if (failing_sat_clause != nullptr) {
    failing_clause = ClausePtr(failing_sat_clause);
  } else if (trail_->FailingClausePtr() != kNullClausePtr) {
    failing_clause = trail_->FailingClausePtr();
  } else {
    absl::Span<const Literal> failing_clause_literals = trail_->FailingClause();
    if (failing_clause_literals.size() == 2) {
      failing_clause =
          ClausePtr(failing_clause_literals[0], failing_clause_literals[1]);
    }
  }
  if (failing_clause == kNullClausePtr) {
    lrat_proof_handler_->AddAssumedClause(ClausePtr(trail_->FailingClause()));
  }
  proof->push_back(failing_clause);
}

void SatSolver::AppendLratProofFromReasons(absl::Span<const Literal> reasons,
                                           std::vector<ClausePtr>* proof) {
  // First add all the unit clauses used in the reasons to infer the conflict.
  // They can be added in any order since they don't depend on each other.
  for (const Literal literal : reasons) {
    DCHECK_NE(trail_->AssignmentLevel(literal), 0);
    AppendLratProofForFixedLiterals(trail_->Reason(literal.Variable()), proof);
  }

  // Then add the clauses which become unit when all the unit clauses above and
  // all the literals in learned_conflict_ are assumed to be false, in unit
  // propagation order.
  for (int i = reasons.size() - 1; i >= 0; --i) {
    const Literal literal = reasons[i];
    ClausePtr clause = clauses_propagator_->ReasonClausePtr(literal);
    if (clause == kNullClausePtr) {
      DCHECK_NE(trail_->AssignmentLevel(literal), 0);
      lrat_proof_handler_->AddAssumedClause(
          ClausePtr(trail_->Reason(literal.Variable())));
    }
    proof->push_back(clause);
  }
}

SatSolver::Status SatSolver::ReapplyDecisionsUpTo(
    int max_level, int* first_propagation_index) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(assumptions_.empty());
  int decision_index = trail_->CurrentDecisionLevel();
  const auto& decisions = trail_->Decisions();
  while (decision_index <= max_level) {
    DCHECK_GE(decision_index, trail_->CurrentDecisionLevel());
    const Literal previous_decision = decisions[decision_index].literal;
    ++decision_index;
    if (Assignment().LiteralIsTrue(previous_decision)) {
      // Note that this particular position in decisions will be overridden,
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
    const int old_level = trail_->CurrentDecisionLevel();
    const int index = EnqueueDecisionAndBackjumpOnConflict(previous_decision);
    if (first_propagation_index != nullptr) {
      *first_propagation_index = std::min(*first_propagation_index, index);
    }
    if (index == kUnsatTrailIndex) return INFEASIBLE;
    if (trail_->CurrentDecisionLevel() <= old_level) {
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
      decision_index = trail_->CurrentDecisionLevel();
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
  DCHECK_LT(CurrentDecisionLevel(), trail_->Decisions().size());
  trail_->OverrideDecision(CurrentDecisionLevel(), true_literal);
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
  DCHECK_GE(target_level, 0);
  DCHECK_LE(target_level, CurrentDecisionLevel());
  if (CurrentDecisionLevel() == target_level) return;

  // Any backtrack to the root from a positive one is counted as a restart.
  counters_.num_backtracks++;
  if (target_level == 0) {
    counters_.num_backtracks_to_root++;
  }

  // Per the SatPropagator interface, this is needed before calling Untrail.
  const int target_trail_index = trail_->PrepareBacktrack(target_level);
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
  DCHECK(PropagationIsDone());
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

    const int old_level = trail_->CurrentDecisionLevel();
    if (!Propagate()) {
      // A conflict occurred, continue the loop.
      ProcessCurrentConflict();
      if (model_is_unsat_) return StatusWithLog(INFEASIBLE);
      if (old_level == trail_->CurrentDecisionLevel()) {
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
        ++counters_.num_restarts;
        if (!BacktrackAndPropagateReimplications(assumption_level_)) {
          return StatusWithLog(INFEASIBLE);
        }
      }

      DCHECK_GE(CurrentDecisionLevel(), assumption_level_);
      EnqueueNewDecision(decision_policy_->NextBranch());
    }
  }
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

  tmp_mark_.ClearAndResize(num_variables_);

  int trail_index = 0;
  for (const Literal lit : literals) {
    CHECK(Assignment().LiteralIsAssigned(lit));
    trail_index =
        std::max(trail_index, trail_->Info(lit.Variable()).trail_index);
    tmp_mark_.Set(lit.Variable());
  }

  // We just expand the reasons recursively until we only have decisions.
  const auto& decisions = trail_->Decisions();
  const int limit =
      CurrentDecisionLevel() > 0 ? decisions[0].trail_index : trail_->Index();
  CHECK_LT(trail_index, trail_->Index());
  while (true) {
    // Find next marked literal to expand from the trail.
    while (trail_index >= limit &&
           !tmp_mark_[(*trail_)[trail_index].Variable()]) {
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
        if (level > 0 && !tmp_mark_[var]) tmp_mark_.Set(var);
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
      SatClause* clause = clauses_propagator_->ReasonClauseOrNull(var);
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

  it->second.num_cleanup_rounds_since_last_bumped = 0;

  // Increase the activity.
  const double activity = it->second.activity += clause_activity_increment_;
  if (activity > parameters_->max_clause_activity_value()) {
    RescaleClauseActivities(1.0 / parameters_->max_clause_activity_value());
  }

  // Update this clause LBD using the new decision orders.
  // Note that this can keep the clause forever depending on the parameters.
  //
  // TODO(user): This cause one more hash lookup, probably not a big deal, but
  // could be optimized away.
  clauses_propagator_->ChangeLbdIfBetter(clause, ComputeLbd(clause->AsSpan()));
}

void SatSolver::RescaleClauseActivities(double scaling_factor) {
  SCOPED_TIME_STAT(&stats_);
  clause_activity_increment_ *= scaling_factor;
  clauses_propagator_->RescaleClauseActivities(scaling_factor);
}

void SatSolver::UpdateClauseActivityIncrement() {
  SCOPED_TIME_STAT(&stats_);
  clause_activity_increment_ *= 1.0 / parameters_->clause_activity_decay();
}

bool SatSolver::IsConflictValid(absl::Span<const Literal> literals) {
  SCOPED_TIME_STAT(&stats_);
  if (literals.empty()) return true;
  const int highest_level = AssignmentLevel(literals[0].Variable());
  if (!trail_->Assignment().LiteralIsFalse(literals[0])) {
    VLOG(2) << "not false " << literals[0];
    return false;
  }
  for (int i = 1; i < literals.size(); ++i) {
    if (!trail_->Assignment().LiteralIsFalse(literals[i])) {
      VLOG(2) << "not false " << literals[i];
      return false;
    }
    const int level = AssignmentLevel(literals[i].Variable());
    if (level <= 0 || level >= highest_level) {
      VLOG(2) << "Another at highest level or at level zero. level:" << level
              << " highest: " << highest_level;
      return false;
    }
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

// Tricky: When a new conflict clause is learned, the conflicting literal will
// be the only literal at the maximum level. This will not be the case for a
// clause used in resolution as the last literal will be propagated. To ensure
// this function returns the same value for both new conflicts and existing
// clauses, we add 1 to the LBD if this clause has more than 1 literal at the
// maximum level, so existing clauses will have an LBD as if they were a new
// conflict.
int SatSolver::ComputeLbd(absl::Span<const Literal> literals) {
  SCOPED_TIME_STAT(&stats_);
  const int limit =
      parameters_->count_assumption_levels_in_lbd() ? 0 : assumption_level_;
  int max_level = 0;
  for (const Literal literal : literals) {
    max_level = std::max(max_level, AssignmentLevel(literal.Variable()));
  }
  if (max_level <= limit) return 0;

  int num_at_max_level = 0;
  is_level_marked_.ClearAndResize(SatDecisionLevel(max_level + 1));
  for (const Literal literal : literals) {
    const SatDecisionLevel level(AssignmentLevel(literal.Variable()));
    DCHECK_GE(level, 0);
    num_at_max_level += (level == max_level) ? 1 : 0;
    if (level > limit) is_level_marked_.Set(level);
  }

  return is_level_marked_.NumberOfSetCallsWithDifferentArguments() +
         (num_at_max_level > 1 ? 1 : 0);
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

void SatSolver::ProcessNewlyFixedVariables() {
  SCOPED_TIME_STAT(&stats_);
  DCHECK_EQ(CurrentDecisionLevel(), 0);
  if (num_processed_fixed_variables_ == trail_->Index()) return;
  num_processed_fixed_variables_ = trail_->Index();

  int num_detached_clauses = 0;
  int num_binary = 0;

  // We remove the clauses that are always true and the fixed literals from the
  // others. Note that none of the clause should be all false because we should
  // have detected a conflict before this is called.
  int saved_index = trail_->Index();
  for (SatClause* clause : clauses_propagator_->AllClausesInCreationOrder()) {
    if (clause->IsRemoved()) continue;

    const size_t old_size = clause->size();
    if (clauses_propagator_->RemoveFixedLiteralsAndTestIfTrue(clause)) {
      ++num_detached_clauses;
      continue;
    }

    const size_t new_size = clause->size();
    if (new_size == old_size) continue;

    if (new_size == 2) {
      // This clause is now a binary clause, treat it separately. Note that
      // it is safe to do that because this clause can't be used as a reason
      // since we are at level zero and the clause is not satisfied.
      AddBinaryClauseInternal(clause->FirstLiteral(), clause->SecondLiteral());
      clauses_propagator_->LazyDelete(
          clause, DeletionSourceForStat::PROMOTED_TO_BINARY);
      ++num_binary;

      // Tricky: AddBinaryClauseInternal() might fix literal if there is some
      // unprocessed equivalent literal, and the binary clause turn out to be
      // unary. This shouldn't happen otherwise the logic of
      // RemoveFixedLiteralsAndTestIfTrue() might fail.
      //
      // To prevent issues, we reach the fix point propagation each time this is
      // triggered. Note that the already processed clauses might contain new
      // fixed literals, that is okay, we will clean them up on the next call to
      // ProcessNewlyFixedVariables().
      //
      // TODO(user): This still happen in SAT22.Carry_Save_Fast_1.cnf.cnf.xz, A
      // better alternative is probably to make sure we only ever have cleaned
      // clauses. We must clean them each time
      // binary_implication_graph_->DetectEquivalence() is called, and we need
      // to make sure we don't generate new clauses that are not cleaned up.
      if (trail_->Index() > saved_index) {
        if (!FinishPropagation()) {
          SetModelUnsat();
          return;
        }
        saved_index = trail_->Index();
      }
      continue;
    }
  }

  // Note that we will only delete the clauses during the next database cleanup.
  clauses_propagator_->CleanUpWatchers();
  if (num_detached_clauses > 0 || num_binary > 0) {
    VLOG(1) << trail_->Index() << " fixed variables at level 0. " << "Detached "
            << num_detached_clauses << " clauses. " << num_binary
            << " converted to binary.";
    if (saved_index > num_processed_fixed_variables_) {
      VLOG(1) << "Extra propagation due to unclean clauses! #fixed: "
              << saved_index - num_processed_fixed_variables_;
    }
  }

  // We also clean the binary implication graph.
  // Tricky: If we added the first binary clauses above, the binary graph
  // is not in "propagated" state as it should be, so we call Propagate() so
  // all the checks are happy.
  CHECK(binary_implication_graph_->Propagate(trail_));
  binary_implication_graph_->RemoveFixedVariables();
  deterministic_time_of_last_fixed_variables_cleanup_ = deterministic_time();
}

bool SatSolver::PropagationIsDone() const {
  // If the time limit is reached, then this invariant do not hold.
  if (time_limit_->LimitReached()) return true;
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
    std::vector<Literal>* reason_used_to_infer_the_conflict) {
  SCOPED_TIME_STAT(&stats_);
  const int64_t conflict_id = counters_.num_failures;

  Literal previous_literal;

  // This will be used to mark all the literals inspected while we process the
  // conflict and the reasons behind each of its variable assignments.
  is_marked_.ClearAndResize(num_variables_);

  conflict->clear();
  reason_used_to_infer_the_conflict->clear();
  if (max_trail_index == -1) return;

  absl::Span<const Literal> conflict_or_reason_to_expand =
      trail_->FailingClause();

  // max_trail_index is the maximum trail index appearing in the failing_clause
  // and its level (Which is almost always equals to the CurrentDecisionLevel(),
  // except for symmetry propagation).
  DCHECK_EQ(max_trail_index, ComputeMaxTrailIndex(trail_->FailingClause()));
  int highest_level = trail_->Info((*trail_)[max_trail_index].Variable()).level;
  if (trail_->ChronologicalBacktrackingEnabled()) {
    for (const Literal literal : conflict_or_reason_to_expand) {
      highest_level =
          std::max(highest_level, AssignmentLevel(literal.Variable()));
    }
  }
  if (highest_level == 0) return;

  // We use a max-heap to find the literal to eliminate.
  struct LiteralWithIndex {
    Literal literal;
    int index;

    bool operator<(const LiteralWithIndex& other) const {
      return index < other.index;
    }
  };
  std::vector<LiteralWithIndex> last_level_heap;

  // To find the 1-UIP conflict clause, we start by the failing_clause, and
  // expand each of its literal using the reason for this literal assignment to
  // false. The is_marked_ set allow us to never expand the same literal twice.
  //
  // The expansion is not done (i.e. stop) for literals that were assigned at a
  // decision level below the current one. If the level of such literal is not
  // zero, it is added to the conflict clause.
  //
  // We use a heap to expand the literals of the highest_level by decreasing
  // assignment order, aka trail index. We stop when there is a single literal
  // left at the higest level.
  //
  // This last literal will be the first UIP because by definition all the
  // propagation done at the current level will pass though it at some point.
  SatClause* sat_clause = trail_->FailingSatClause();
  DCHECK(!conflict_or_reason_to_expand.empty());
  while (true) {
    const int old_conflict_size = conflict->size();
    const int old_heap_size = last_level_heap.size();

    int num_new_vars_at_positive_level = 0;
    int num_vars_at_positive_level_in_clause_to_expand = 0;
    for (const Literal literal : conflict_or_reason_to_expand) {
      const BooleanVariable var = literal.Variable();
      const int level = AssignmentLevel(var);
      if (level == 0) continue;
      DCHECK_LE(level, highest_level);
      ++num_vars_at_positive_level_in_clause_to_expand;
      if (!is_marked_[var]) {
        is_marked_.Set(var);
        ++num_new_vars_at_positive_level;
        if (level == highest_level) {
          last_level_heap.push_back({literal, trail_->Info(var).trail_index});
        } else {
          // Note that all these literals are currently false since the clause
          // to expand was used to infer the value of a literal at this level.
          DCHECK(trail_->Assignment().LiteralIsFalse(literal));
          conflict->push_back(literal);
        }
      }
    }

    // If there is new variables, then all the previously subsumed clauses are
    // not subsumed by the current conflict anymore. However they are still
    // subsumed by the state of the conflict just before.
    //
    // TODO(user): Think about minimization of these intermediate conflicts.
    if (num_new_vars_at_positive_level > 0) {
      if (parameters_->extra_subsumption_during_conflict_analysis() &&
          !subsumed_clauses_.empty() &&
          reason_used_to_infer_the_conflict->size() > 1) {
        // The "old" conflict should subsume all of that.
        tmp_literals_.clear();
        tmp_literals_.push_back(previous_literal.Negated());
        for (int i = 0; i < old_conflict_size; ++i) {
          tmp_literals_.push_back((*conflict)[i]);
        }
        for (int i = 0; i < old_heap_size; ++i) {
          tmp_literals_.push_back(last_level_heap[i].literal);
        }
        if (DEBUG_MODE) {
          for (SatClause* clause : subsumed_clauses_) {
            CHECK(ClauseSubsumption(tmp_literals_, clause))
                << tmp_literals_ << " " << clause->AsSpan();
          }
        }

        subsuming_lrat_index_.push_back(
            reason_used_to_infer_the_conflict->size() - 1);
        subsuming_clauses_.Add(tmp_literals_);
        subsuming_groups_.Add(subsumed_clauses_);
      }
      subsumed_clauses_.clear();
    }

    // Restore the heap property.
    for (int i = old_heap_size + 1; i <= last_level_heap.size(); ++i) {
      std::push_heap(last_level_heap.begin(), last_level_heap.begin() + i);
    }

    // This check if the new conflict is exactly equal to
    // conflict_or_reason_to_expand. Since we just performed an union, comparing
    // the size is enough.
    //
    // When this is true, then the current conflict is equal to the reason we
    // just expanded and subsumbes the clause (which has just one extra
    // literal).
    if (sat_clause != nullptr &&
        num_vars_at_positive_level_in_clause_to_expand ==
            conflict->size() + last_level_heap.size()) {
      subsumed_clauses_.push_back(sat_clause);
    }

    DCHECK(!last_level_heap.empty());
    const Literal literal = (*trail_)[last_level_heap.front().index];
    DCHECK(is_marked_[literal.Variable()]);

    if (last_level_heap.size() == 1) {
      // We have the first UIP. Add its negation to the conflict clause.
      // This way, after backtracking to the proper level, the conflict clause
      // will be unit, and infer the negation of the UIP that caused the fail.
      conflict->push_back(literal.Negated());

      // To respect the function API move the first UIP in the first position.
      std::swap(conflict->back(), conflict->front());
      break;
    }

    reason_used_to_infer_the_conflict->push_back(literal);

    // If we already encountered the same reason, we can just skip this literal
    // which is what setting conflict_or_reason_to_expand to the empty clause
    // do.
    if (same_reason_identifier_.FirstVariableWithSameReason(
            literal.Variable()) != literal.Variable()) {
      conflict_or_reason_to_expand = {};
    } else {
      conflict_or_reason_to_expand =
          trail_->Reason(literal.Variable(), conflict_id);
    }
    sat_clause = clauses_propagator_->ReasonClauseOrNull(literal.Variable());

    previous_literal = literal;
    absl::c_pop_heap(last_level_heap);
    last_level_heap.pop_back();
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

  // Reduce the conflict coefficients if it is not already done.
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

void SatSolver::MinimizeConflict(std::vector<Literal>* conflict,
                                 std::vector<ClausePtr>* proof) {
  SCOPED_TIME_STAT(&stats_);

  const int old_size = conflict->size();
  switch (parameters_->minimization_algorithm()) {
    case SatParameters::NONE:
      return;
    case SatParameters::SIMPLE: {
      MinimizeConflictSimple(conflict, proof);
      break;
    }
    case SatParameters::RECURSIVE: {
      MinimizeConflictRecursively(conflict, proof);
      break;
    }
  }
  if (conflict->size() < old_size) {
    ++counters_.num_minimizations;
    counters_.num_literals_removed += old_size - conflict->size();
  }
}

// This simple version just looks for any literal that is directly inferred by
// other literals of the conflict. It is directly inferred if the literals of
// its reason clause are either from level 0 or from the conflict itself.
//
// Note that because of the assignment structure, there is no need to process
// the literals of the conflict in order (except to fill the LRAT proof in
// `proof`). While exploring the reason for a literal assignment, there will
// be no cycles.
void SatSolver::MinimizeConflictSimple(std::vector<Literal>* conflict,
                                       std::vector<ClausePtr>* proof) {
  SCOPED_TIME_STAT(&stats_);
  const int current_level = CurrentDecisionLevel();

  // Note that is_marked_ is already initialized and that we can start at 1
  // since the first literal of the conflict is the 1-UIP literal.
  int index = 1;
  tmp_literals_.clear();
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
        if (can_be_removed && lrat_proof_handler_ != nullptr) {
          // If BinaryImplicationGraph::MinimizeConflictFirst() was called, some
          // marked literals might be indirectly inferred by the 1-UIP literal,
          // via some chains of binary clauses. These implication chains must be
          // added to the LRAT proof.
          binary_implication_graph_->AppendImplicationChains(reason, proof);
          // The reasons of the conflict literals must be added to `proof` in
          // trail index order. For this we collect these literals in a vector
          // which is sorted at the end.
          tmp_literals_.push_back((*conflict)[i].Negated());
        }
      }
    }
    if (!can_be_removed) {
      (*conflict)[index] = (*conflict)[i];
      ++index;
    }
  }
  conflict->erase(conflict->begin() + index, conflict->end());
  if (!tmp_literals_.empty()) {
    // Note: it is possible to compute the conflict directly in trail index
    // order in ComputeFirstUIPConflict(), in linear time (of max - min trail
    // index), to avoid this sort. But this sort does not seem to be costly.
    std::sort(tmp_literals_.begin(), tmp_literals_.end(),
              [&](Literal a, Literal b) {
                return trail_->Info(a.Variable()).trail_index <
                       trail_->Info(b.Variable()).trail_index;
              });
    for (const Literal literal : tmp_literals_) {
      proof->push_back(clauses_propagator_->ReasonClausePtr(literal));
      DCHECK_NE(proof->back(), kNullClausePtr);
    }
  }
}

// This is similar to MinimizeConflictSimple() except that for each literal of
// the conflict, the literals of its reason are recursively expanded using their
// reason and so on. The recursion loops until we show that the initial literal
// can be inferred from the conflict variables alone, or if we show that this is
// not the case. The result of any variable expansion will be cached in order
// not to be expanded again.
void SatSolver::MinimizeConflictRecursively(std::vector<Literal>* conflict,
                                            std::vector<ClausePtr>* proof) {
  SCOPED_TIME_STAT(&stats_);

  // is_marked_ will contain all the conflict literals plus the literals that
  // have been shown to depend only on the conflict literals. is_independent_
  // will contain the literals that have been shown NOT to depend only on the
  // conflict literals. The two sets are exclusive for non-conflict literals,
  // but a conflict literal (which is always marked) can be independent if we
  // showed that it can't be removed from the clause.
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

  // Compute the number of variables at each decision level. This will be used
  // to prune the DFS because we know that the minimized conflict will have at
  // least one variable of each decision level. Because such variable can't be
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

  // Remove the redundant variables from the conflict. These are the ones that
  // can be inferred by some other variables in the conflict. Note that we can
  // skip the first position since this is the 1-UIP.
  int index = 1;
  TimeLimitCheckEveryNCalls time_limit_check(100, time_limit_);

  std::vector<Literal>& removed_literals = tmp_literals_;
  if (lrat_proof_handler_ != nullptr) {
    removed_literals.clear();
    is_marked_for_lrat_.CopyFrom(is_marked_);
  }

  for (int i = 1; i < conflict->size(); ++i) {
    const BooleanVariable var = (*conflict)[i].Variable();
    const AssignmentInfo& info = trail_->Info(var);
    if (time_limit_check.LimitReached() ||
        info.type == AssignmentType::kSearchDecision ||
        info.trail_index <= min_trail_index_per_level_[info.level] ||
        !CanBeInferredFromConflictVariables(var)) {
      // Mark the conflict variable as independent. Note that is_marked_[var]
      // will still be true.
      is_independent_.Set(var);
      (*conflict)[index] = (*conflict)[i];
      ++index;
    } else if (lrat_proof_handler_ != nullptr) {
      removed_literals.push_back((*conflict)[i]);
    }
  }
  conflict->resize(index);

  if (lrat_proof_handler_ != nullptr && !removed_literals.empty()) {
    // Note: it is possible to compute the conflict directly in trail index
    // order in ComputeFirstUIPConflict(), in linear time (of max - min trail
    // index), to avoid this sort. But this sort does not seem to be costly.
    std::sort(removed_literals.begin(), removed_literals.end(),
              [&](Literal a, Literal b) {
                return trail_->Info(a.Variable()).trail_index <
                       trail_->Info(b.Variable()).trail_index;
              });
    for (const Literal literal : removed_literals) {
      AppendInferenceChain(literal.Variable(), proof);
    }
  }

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

bool SatSolver::CanBeInferredFromConflictVariables(BooleanVariable variable) {
  // Test for an already processed variable with the same reason.
  {
    DCHECK(is_marked_[variable]);
    const BooleanVariable v =
        same_reason_identifier_.FirstVariableWithSameReason(variable);
    if (v != variable) return !is_independent_[v];
  }

  // This function implements an iterative DFS from the given variable. It uses
  // the reason clause as adjacency lists. dfs_stack_ can be seen as the
  // recursive call stack of the variable we are currently processing. All its
  // adjacent variables will be pushed into variable_to_process_, and we will
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

  // All the variables left on the dfs_stack_ are independent.
  for (const BooleanVariable var : dfs_stack_) {
    is_independent_.Set(var);
  }
  return dfs_stack_.empty();
}

void SatSolver::AppendInferenceChain(BooleanVariable variable,
                                     std::vector<ClausePtr>* clauses) {
  DCHECK(is_marked_[variable]);
  DCHECK(is_marked_for_lrat_[variable]);

  // This function implements the same iterative DFS as in
  // CanBeInferredFromConflictVariables(). See this method for more details.
  dfs_stack_.clear();
  dfs_stack_.push_back(variable);
  variable_to_process_.clear();
  variable_to_process_.push_back(variable);

  // First we expand the reason for the given variable.
  const auto expand_variable = [&](BooleanVariable variable_to_expand) {
    for (const Literal literal : trail_->Reason(variable_to_expand)) {
      const BooleanVariable var = literal.Variable();
      const AssignmentInfo& info = trail_->Info(var);
      if (is_marked_for_lrat_[var] || info.level == 0) {
        // If BinaryImplicationGraph::MinimizeConflictFirst() was called, some
        // marked literals might be indirectly inferred by the 1-UIP literal,
        // via some chains of binary clauses. These implication chains must be
        // added to the LRAT proof.
        binary_implication_graph_->AppendImplicationChains({literal}, clauses);
        is_marked_for_lrat_.Set(var);
        continue;
      }
      variable_to_process_.push_back(var);
    }
  };
  expand_variable(variable);

  // Then we start the DFS.
  while (!variable_to_process_.empty()) {
    const BooleanVariable current_var = variable_to_process_.back();
    DCHECK(is_marked_[current_var]);
    if (current_var == dfs_stack_.back()) {
      // We finished the DFS of the variable dfs_stack_.back(), this can be seen
      // as a recursive call terminating.
      variable_to_process_.pop_back();
      dfs_stack_.pop_back();

      const Literal current_literal =
          trail_->Assignment().GetTrueLiteralForAssignedVariable(current_var);
      clauses->push_back(clauses_propagator_->ReasonClausePtr(current_literal));
      DCHECK_NE(clauses->back(), kNullClausePtr);
      is_marked_for_lrat_.Set(current_var);
      continue;
    }

    // If this variable became marked since the we pushed it, we can skip it.
    if (is_marked_for_lrat_[current_var]) {
      variable_to_process_.pop_back();
      continue;
    }

    // Test for an already processed variable with the same reason.
    {
      const BooleanVariable v =
          same_reason_identifier_.FirstVariableWithSameReason(current_var);
      if (v != current_var) {
        DCHECK(is_marked_for_lrat_[v]);
        variable_to_process_.pop_back();
        continue;
      }
    }

    // Expand the variable. This can be seen as making a recursive call.
    dfs_stack_.push_back(current_var);
    expand_variable(current_var);
  }
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

void SatSolver::CleanClauseDatabaseIfNeeded() {
  if (num_learned_clause_before_cleanup_ > 0) return;
  SCOPED_TIME_STAT(&stats_);

  // Creates a list of clauses that can be deleted. Note that only the clauses
  // that appear in clauses_info can potentially be removed.
  typedef std::pair<SatClause*, ClauseInfo> Entry;
  std::vector<Entry> entries;
  auto& clauses_info = *(clauses_propagator_->mutable_clauses_info());
  for (auto& entry : clauses_info) {
    DCHECK(!entry.first->empty());  // Should have been deleted !
    entry.second.num_cleanup_rounds_since_last_bumped++;
    if (clauses_propagator_->ClauseIsUsedAsReason(entry.first)) continue;

    if (entry.second.lbd <= parameters_->clause_cleanup_lbd_tier1() &&
        entry.second.num_cleanup_rounds_since_last_bumped <= 32) {
      continue;
    }

    if (entry.second.lbd <= parameters_->clause_cleanup_lbd_tier2() &&
        entry.second.num_cleanup_rounds_since_last_bumped <= 1) {
      continue;
    }

    // The LBD should always have been updated to be <= size.
    DCHECK_LE(entry.second.lbd, entry.first->size());
    entries.push_back(entry);
  }
  const int num_protected_clauses =
      clauses_propagator_->num_removable_clauses() - entries.size();

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
      clauses_propagator_->LazyDelete(clause,
                                      DeletionSourceForStat::GARBAGE_COLLECTED);
    }
    clauses_propagator_->CleanUpWatchers();

    // TODO(user): If the need arise, we could avoid this linear scan on the
    // full list of clauses by not keeping the clauses from clauses_info there.
    if (!block_clause_deletion_) {
      clauses_propagator_->DeleteRemovedClauses();
    }
  }

  num_learned_clause_before_cleanup_ =
      parameters_->clause_cleanup_period() +
      (++counters_.num_cleanup_rounds) *
          parameters_->clause_cleanup_period_increment();
  VLOG(1) << "Database cleanup, #protected:" << num_protected_clauses
          << " #kept:" << num_kept_clauses
          << " #deleted:" << num_deleted_clauses;
  counters_.num_deleted_clauses += num_deleted_clauses;
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
