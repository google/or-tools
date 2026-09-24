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

// Trails and propagators.

#ifndef ORTOOLS_SAT_SAT_TRAIL_H_
#define ORTOOLS_SAT_SAT_TRAIL_H_

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/sat_assignment.h"
#include "ortools/sat/sat_clause.h"
#include "ortools/sat/sat_literal.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

// Each literal on the trail will have an associated propagation "type" which is
// either one of these special types or the id of a propagator.
struct AssignmentType {
  static constexpr int kCachedReason = 0;
  static constexpr int kUnitReason = 1;
  static constexpr int kSearchDecision = 2;
  static constexpr int kSameReasonAs = 3;

  // Propagator ids start from there and are created dynamically.
  static constexpr int kFirstFreePropagationId = 4;
};

// A Boolean "decision" taken by the solver.
struct LiteralWithTrailIndex {
  LiteralWithTrailIndex() = default;
  LiteralWithTrailIndex(Literal l, int i) : literal(l), trail_index(i) {}
  Literal literal;
  int trail_index = 0;
};

// Forward declaration.
class SatPropagator;

// The solver trail stores the assignment made by the solver in order.
// This class is responsible for maintaining the assignment of each variable
// and the information of each assignment.
class Trail {
 public:
  Trail() {
    current_info_.trail_index = 0;
    current_info_.level = 0;
  }

  // This type is neither copyable nor movable.
  Trail(const Trail&) = delete;
  Trail& operator=(const Trail&) = delete;

  void Resize(int num_variables);

  // Registers a propagator. This assigns a unique id to this propagator and
  // calls SetPropagatorId() on it.
  void RegisterPropagator(SatPropagator* propagator);

  // Enqueues the assignment that makes the given literal true on the trail.
  // This should only be called on unassigned variables.
  void Enqueue(Literal true_literal, int propagator_id) {
    DCHECK(!assignment_.VariableIsAssigned(true_literal.Variable()));
    trail_[current_info_.trail_index] = true_literal;
    current_info_.type = propagator_id;
    info_[true_literal.Variable()] = current_info_;
    assignment_.AssignFromTrueLiteral(true_literal);
    ++current_info_.trail_index;
  }
  void EnqueueAtLevel(Literal true_literal, int propagator_id, int level) {
    Enqueue(true_literal, propagator_id);
    if (use_chronological_backtracking_) {
      info_[true_literal.Variable()].level = level;
    }
  }

  // Using this is faster as it caches all the vectors data.
  // Warning: calls to this cannot be interleaved with normal enqueue.
  // only use in hot-loops.
  class EnqueueHelper {
   public:
    EnqueueHelper(Literal* trail_ptr, AssignmentInfo* current_info,
                  AssignmentInfo* info_ptr, VariablesAssignment* assignment)
        : trail_ptr_(trail_ptr),
          current_info_(current_info),
          info_ptr_(info_ptr),
          bitset_(assignment->GetBitsetView()) {}

    void EnqueueAtLevel(Literal true_literal, int level) {
      bitset_.Set(true_literal);
      AssignmentInfo* info = info_ptr_ + true_literal.Variable().value();
      *info = *current_info_;
      info->level = level;
      trail_ptr_[current_info_->trail_index++] = true_literal;
    }

    void EnqueueWithUnitReason(Literal true_literal) {
      bitset_.Set(true_literal);
      AssignmentInfo* info = info_ptr_ + true_literal.Variable().value();
      *info = *current_info_;
      info->level = 0;
      info->type = AssignmentType::kUnitReason;
      trail_ptr_[current_info_->trail_index++] = true_literal;
    }

    bool LiteralIsTrue(Literal literal) const {
      return bitset_[literal.Index()];
    }
    bool LiteralIsFalse(Literal literal) const {
      return bitset_[literal.NegatedIndex()];
    }

   private:
    Literal* trail_ptr_;
    AssignmentInfo* current_info_;
    AssignmentInfo* info_ptr_;
    Bitset64<LiteralIndex>::View bitset_;
  };
  EnqueueHelper GetEnqueueHelper(int propagator_id) {
    current_info_.type = propagator_id;
    return EnqueueHelper(trail_.data(), &current_info_, info_.data(),
                         &assignment_);
  }

  // Specific Enqueue() for search decisions.
  void EnqueueSearchDecision(Literal true_literal) {
    decisions_[current_decision_level_] =
        LiteralWithTrailIndex(true_literal, Index());
    current_info_.level = ++current_decision_level_;
    Enqueue(true_literal, AssignmentType::kSearchDecision);
  }

  // Specific Enqueue() for assumptions.
  void EnqueueAssumption(Literal assumptions) {
    if (current_decision_level_ == 0) {
      // Special decision, make sure it is detectable.
      decisions_[0] = LiteralWithTrailIndex(Literal(kNoLiteralIndex), Index());
      current_info_.level = ++current_decision_level_;
    }
    CHECK_EQ(current_decision_level_, 1);
    Enqueue(assumptions, AssignmentType::kSearchDecision);
  }

  void OverrideDecision(int level, Literal literal) {
    decisions_[level].literal = literal;
  }

  // Allows to recover the list of decisions.
  // Note that the Decisions() vector is always of size NumVariables(), and that
  // only the first CurrentDecisionLevel() entries have a meaning. The decision
  // made at level l is Decisions()[l - 1] (there are no decisions at level 0).
  const std::vector<LiteralWithTrailIndex>& Decisions() const {
    return decisions_;
  }

  // Specific Enqueue() version for unit clauses.
  void EnqueueWithUnitReason(Literal true_literal) {
    EnqueueAtLevel(true_literal, AssignmentType::kUnitReason, 0);
  }

  // Some constraints propagate a lot of literals at once. In these cases, it is
  // more efficient to have all the propagated literals except the first one
  // referring to the reason of the first of them.
  void EnqueueWithSameReasonAs(Literal true_literal,
                               BooleanVariable reference_var) {
    reference_var_with_same_reason_as_[true_literal.Variable()] = reference_var;
    Enqueue(true_literal, AssignmentType::kSameReasonAs);
    if (ChronologicalBacktrackingEnabled()) {
      info_[true_literal.Variable()].level = Info(reference_var).level;
    }
  }

  // Enqueues the given literal using the current content of
  // GetEmptyVectorToStoreReason() as the reason. This API is a bit more
  // lenient and does not require the literal to be unassigned. If it is
  // already assigned to false, then MutableConflict() will be set appropriately
  // and this will return false otherwise this will enqueue the literal and
  // return true.
  ABSL_MUST_USE_RESULT bool EnqueueWithStoredReason(Literal true_literal,
                                                    ClausePtr reason_clause) {
    if (assignment_.LiteralIsTrue(true_literal)) return true;
    if (assignment_.LiteralIsFalse(true_literal)) {
      *MutableConflict() = reasons_repository_[Index()];
      MutableConflict()->push_back(true_literal);
      failing_clause_ptr_ = reason_clause;
      return false;
    }

    MaybeSetReasonClause(true_literal, reason_clause);
    Enqueue(true_literal, AssignmentType::kCachedReason);
    const BooleanVariable var = true_literal.Variable();
    reasons_[var] = reasons_repository_[info_[var].trail_index];
    old_type_[var] = info_[var].type;
    info_[var].type = AssignmentType::kCachedReason;
    DCHECK_EQ(old_type_[var], AssignmentType::kCachedReason);
    if (ChronologicalBacktrackingEnabled()) {
      uint32_t level = 0;
      for (const Literal literal : reasons_[var]) {
        level = std::max(level, Info(literal.Variable()).level);
      }
      info_[var].level = level;
    }
    return true;
  }

  // Returns the reason why this variable was assigned.
  //
  // Note that this shouldn't be called on a variable at level zero, because we
  // don't clean up the reason data for these variables but the underlying
  // clauses may have been deleted.
  //
  // If conflict_id >= 0, this indicates that this was called as part of the
  // first-UIP procedure. It has a few implications:
  //  - The reasons do not need to be cached and can be adapted to the current
  //    conflict.
  //  - Some data can be reused between two calls about the same conflict.
  //  - Note however that if the reason is a simple clause, we shouldn't adapt
  //    it because we rely on extra facts in the first UIP code where we detect
  //    subsumed clauses for instance.
  absl::Span<const Literal> Reason(BooleanVariable var,
                                   int64_t conflict_id = -1) const;

  // Returns the "type" of an assignment (see AssignmentType). Note that this
  // function never returns kSameReasonAs or kCachedReason, it instead returns
  // the initial type that caused this assignment. As such, it is different
  // from Info(var).type and the latter should not be used outside this class.
  int AssignmentType(BooleanVariable var) const;

  // Returns the clause which is the reason why the given variable was
  // enqueued, or kNullClausePtr if there is none. The variable must have been
  // enqueued with EnqueueWithStoredReason().
  ClausePtr GetStoredReasonClause(BooleanVariable var) const {
    DCHECK(AssignmentType(var) == AssignmentType::kCachedReason);
    if (var.value() >= reason_clauses_.size()) return kNullClausePtr;
    return reason_clauses_[var];
  }

  // If a variable was propagated with EnqueueWithSameReasonAs(), returns its
  // reference variable. Otherwise returns the given variable.
  BooleanVariable ReferenceVarWithSameReason(BooleanVariable var) const;

  // This can be used to get a location at which the reason for the literal
  // at trail_index on the trail can be stored. This clears the vector before
  // returning it.
  std::vector<Literal>* GetEmptyVectorToStoreReason(int trail_index) const {
    if (trail_index >= reasons_repository_.size()) {
      reasons_repository_.resize(trail_index + 1);
    }
    reasons_repository_[trail_index].clear();
    return &reasons_repository_[trail_index];
  }

  // Shortcut for GetEmptyVectorToStoreReason(Index()).
  std::vector<Literal>* GetEmptyVectorToStoreReason() const {
    return GetEmptyVectorToStoreReason(Index());
  }

  // Explicitly overwrite the reason so that the given propagator will be
  // asked for it. This is currently only used by the BinaryImplicationGraph.
  // Note: Care must be taken not to break the lrat proof!
  void ChangeReason(int trail_index, int propagator_id) {
    const BooleanVariable var = trail_[trail_index].Variable();
    info_[var].type = propagator_id;
    old_type_[var] = propagator_id;
  }

  // On backtrack we should always do:
  //
  // const int target_trail_index = PrepareBacktrack(level);
  // ...
  // Untrail(target_trail_index);
  int PrepareBacktrack(int level) {
    current_decision_level_ = level;
    current_info_.level = level;
    return decisions_[level].trail_index;
  }

  // Reverts the trail and underlying assignment to the given target trail
  // index. Note that we do not touch the assignment info.
  void Untrail(int target_trail_index) {
    const int index = Index();
    num_untrailed_enqueues_ += index - target_trail_index;
    for (int i = target_trail_index; i < index; ++i) {
      assignment_.UnassignLiteral(trail_[i]);
    }
    current_info_.trail_index = target_trail_index;
    if (use_chronological_backtracking_) {
      ReimplyAll(index);
    }
  }

  int CurrentDecisionLevel() const { return current_info_.level; }

  // Generic interface to set the current failing clause.
  //
  // Returns the address of a vector where a client can store the current
  // conflict. This vector will be returned by the FailingClause() call.
  std::vector<Literal>* MutableConflict() {
    ++conflict_timestamp_;
    failing_sat_clause_ = nullptr;
    failing_clause_ptr_ = kNullClausePtr;
    return &conflict_;
  }

  // This should increase on each call to MutableConflict().
  int64_t conflict_timestamp() const { return conflict_timestamp_; }

  // Returns the last conflict.
  absl::Span<const Literal> FailingClause() const {
    if (DEBUG_MODE && debug_checker_ != nullptr) {
      CHECK(debug_checker_(conflict_));
    }
    return conflict_;
  }

  // Specific SatClause interface so we can update the conflict clause activity.
  // Note that MutableConflict() automatically sets this to nullptr, so we can
  // know whether or not the last conflict was caused by a clause.
  void SetFailingSatClause(SatClause* clause) {
    failing_sat_clause_ = clause;
    failing_clause_ptr_ = kNullClausePtr;
  }
  SatClause* FailingSatClause() const { return failing_sat_clause_; }

  // Returns the LRAT failing clause. This is only set if a conflict is detected
  // in EnqueueWithStoredReason().
  ClausePtr FailingClausePtr() const { return failing_clause_ptr_; }

  // Getters.
  int NumVariables() const { return trail_.size(); }
  int64_t NumberOfEnqueues() const { return num_untrailed_enqueues_ + Index(); }
  int Index() const { return current_info_.trail_index; }
  // This accessor can return trail_.end(). operator[] cannot. This allows
  // normal std:vector operations, such as assign(begin, end).
  std::vector<Literal>::const_iterator IteratorAt(int index) const {
    return trail_.begin() + index;
  }
  const Literal& operator[](int index) const { return trail_[index]; }
  const VariablesAssignment& Assignment() const { return assignment_; }
  const AssignmentInfo& Info(BooleanVariable var) const {
    DCHECK_GE(var, 0);
    DCHECK_LT(var, info_.size());
    return info_[var];
  }

  int AssignmentLevel(Literal lit) const { return Info(lit.Variable()).level; }

  // Print the current literals on the trail.
  std::string DebugString() const {
    return absl::StrJoin(trail_.begin(),
                         trail_.begin() + current_info_.trail_index, " ",
                         [](std::string* out, const Literal& literal) {
                           absl::StrAppend(out, literal.DebugString());
                         });
  }

  void RegisterDebugChecker(
      std::function<bool(absl::Span<const Literal> clause)> checker) {
    debug_checker_ = std::move(checker);
  }

  bool ChronologicalBacktrackingEnabled() const {
    return use_chronological_backtracking_;
  }

  void EnableChronologicalBacktracking(bool enable) {
    CHECK_EQ(CurrentDecisionLevel(), 0);
    use_chronological_backtracking_ = enable;
  }

  using ConflictResolutionFunction = std::function<void(
      std::vector<Literal>* conflict,
      std::vector<Literal>* reason_used_to_infer_the_conflict)>;

  void SetConflictResolutionFunction(ConflictResolutionFunction resolution) {
    resolution_ = std::move(resolution);
  }

  ConflictResolutionFunction GetConflictResolutionFunction() {
    return resolution_;
  }

  int NumReimplicationsOnLastUntrail() const { return last_num_reimplication_; }

 private:
  ConflictResolutionFunction resolution_;

  void MaybeSetReasonClause(Literal true_literal, ClausePtr reason_clause) {
    if (reason_clause != kNullClausePtr) {
      const BooleanVariable var = true_literal.Variable();
      if (var.value() >= reason_clauses_.size()) {
        reason_clauses_.resize(var.value() + 1, kNullClausePtr);
      }
      reason_clauses_[var] = reason_clause;
    }
  }

  // Finds all literals between the current trail index and the given one
  // assigned at the current level or lower, and re-enqueues them with the same
  // reason.
  void ReimplyAll(int old_trail_index);

  bool use_chronological_backtracking_ = false;
  int64_t num_reimplied_literals_ = 0;
  int64_t num_untrailed_enqueues_ = 0;
  AssignmentInfo current_info_;
  VariablesAssignment assignment_;
  std::vector<Literal> trail_;
  int64_t conflict_timestamp_ = 0;
  std::vector<Literal> conflict_;
  util_intops::StrongVector<BooleanVariable, AssignmentInfo> info_;
  // The reason clauses for literals enqueued with a stored reason.
  util_intops::StrongVector<BooleanVariable, ClausePtr> reason_clauses_;
  SatClause* failing_sat_clause_;
  ClausePtr failing_clause_ptr_;

  // Data used by EnqueueWithSameReasonAs().
  util_intops::StrongVector<BooleanVariable, BooleanVariable>
      reference_var_with_same_reason_as_;

  // Reason cache. Mutable since we want the API to be the same whether the
  // reasons are cached or not.
  //
  // When a reason is computed for the first time, we change the type of the
  // variable assignment to kCachedReason so that we know that if it is needed
  // again the reason can just be retrieved by a direct access to reasons_. The
  // old type is saved in old_type_ and can be retrieved by
  // AssignmentType().
  //
  // Note(user): Changing the type is not "clean" but it is efficient. The idea
  // is that it is important to do as little as possible when pushing/popping
  // literals on the trail. Computing the reason happens a lot less often, so it
  // is okay to do slightly more work then. Note also, that we don't need to
  // do anything on "untrail", the kCachedReason type will be overwritten when
  // the same variable is assigned again.
  //
  // TODO(user): An alternative would be to change the sign of the type. This
  // would remove the need for a separate old_type_ vector, but it requires
  // more bits for the type field in AssignmentInfo.
  //
  // Note that we use a deque for the reason repository so that if we add
  // variables, the memory address of the vectors (kept in reasons_) are still
  // valid.
  mutable std::deque<std::vector<Literal>> reasons_repository_;
  mutable util_intops::StrongVector<BooleanVariable, absl::Span<const Literal>>
      reasons_;
  mutable util_intops::StrongVector<BooleanVariable, int> old_type_;

  // This is used by RegisterPropagator() and Reason().
  std::vector<SatPropagator*> propagators_;

  std::function<bool(absl::Span<const Literal> clause)> debug_checker_ =
      nullptr;

  int last_num_reimplication_ = 0;

  // The stack of decisions taken by the solver. They are stored in [0,
  // current_decision_level_). The vector is of size num_variables_ so it can
  // store all the decisions. This is done this way because in some situations
  // we need to remember the previously taken decisions after a backtrack.
  int current_decision_level_ = 0;
  std::vector<LiteralWithTrailIndex> decisions_;
};

// Base class for all the SAT constraints.
class SatPropagator {
 public:
  explicit SatPropagator(const std::string& name)
      : name_(name), propagator_id_(-1), propagation_trail_index_(0) {}

  // This type is neither copyable nor movable.
  SatPropagator(const SatPropagator&) = delete;
  SatPropagator& operator=(const SatPropagator&) = delete;
  virtual ~SatPropagator() = default;

  // Sets/Gets this propagator's unique id.
  void SetPropagatorId(int id) { propagator_id_ = id; }
  int PropagatorId() const { return propagator_id_; }

  // Inspects the trail from propagation_trail_index_ until at least one literal
  // is propagated. Returns false iff a conflict is detected (in which case
  // trail->SetFailingClause() must be called).
  //
  // This must update propagation_trail_index_ so that all the literals before
  // it have been propagated. In particular, if nothing was propagated, then
  // PropagationIsDone() must return true.
  virtual bool Propagate(Trail* trail) = 0;

  // Reverts the state so that all the literals with a trail index greater or
  // equal to the given one are not processed for propagation. Note that the
  // trail current decision level is already reverted before this is called.
  //
  // TODO(user): Currently this is called at each Backtrack(), but we could
  // bundle the calls in case multiple conflicts one after the other are
  // detected even before the Propagate() call of a SatPropagator is called.
  //
  // TODO(user): It is not yet 100% the case, but this can be guaranteed to be
  // called with a trail index that will always be the start of a new decision
  // level.
  virtual void Untrail(const Trail& /*trail*/, int trail_index) {
    propagation_trail_index_ = std::min(propagation_trail_index_, trail_index);
  }

  // Called if the implication at `old_trail_index` remains true after
  // backtracking. If this propagator supports reimplication it should call
  // `trail->EnqueueAtLevel`.
  // This will be called after Untrail() when backtracking.
  virtual void Reimply(Trail* /*trail*/, int /*old_trail_index*/) {
    // It is inefficient and unexpected to call this on a propagator that
    // doesn't support reimplication.
    LOG(DFATAL) << "Reimply not implemented for " << name_ << ".";
  }

  // Explains why the literal at given trail_index was propagated by returning a
  // reason for this propagation. This will only be called for literals that are
  // on the trail and were propagated by this class.
  //
  // The interpretation is that because all the literals of a reason were
  // assigned to false, we could deduce the assignment of the given variable.
  //
  // The returned Span has to be valid until the literal is untrailed. A client
  // can use trail_.GetEmptyVectorToStoreReason() if it doesn't have a memory
  // location that already contains the reason.
  //
  // If conflict id is positive, then this is called during first UIP resolution
  // and we will backtrack over this literal right away, so we don't need to
  // have a span that survives more than once.
  virtual absl::Span<const Literal> Reason(const Trail& /*trail*/,
                                           int /*trail_index*/,
                                           int64_t /*conflict_id*/) const {
    LOG(FATAL) << "Not implemented.";
    return {};
  }

  // Returns true if all the preconditions for Propagate() are satisfied.
  // This is just meant to be used in a DCHECK.
  bool PropagatePreconditionsAreSatisfied(const Trail& trail) const;

  // Returns true iff all the trail was inspected by this propagator.
  bool PropagationIsDone(const Trail& trail) const {
    return propagation_trail_index_ == trail.Index();
  }

  const std::string& name() const { return name_; }

  // Small optimization: If a propagator does not contain any "constraints"
  // there is no point calling propagate on it. Before each propagation, the
  // solver will check for emptiness, and construct an optimized list of
  // propagators before looping many times over the list.
  virtual bool IsEmpty() const { return false; }

 protected:
  const std::string name_;
  int propagator_id_;
  int propagation_trail_index_;
};

// ########################  Implementations below  ########################

inline bool SatPropagator::PropagatePreconditionsAreSatisfied(
    const Trail& trail) const {
  if (propagation_trail_index_ > trail.Index()) {
    LOG(INFO) << "Issue in '" << name_ << ":"
              << " propagation_trail_index_=" << propagation_trail_index_
              << " trail_.Index()=" << trail.Index();
    return false;
  }
  if (propagation_trail_index_ < trail.Index() &&
      trail.Info(trail[propagation_trail_index_].Variable()).level >
          trail.CurrentDecisionLevel()) {
    LOG(INFO) << "Issue in '" << name_ << "':"
              << " propagation_trail_index_=" << propagation_trail_index_
              << " trail_.Index()=" << trail.Index()
              << " level_at_propagation_index="
              << trail.Info(trail[propagation_trail_index_].Variable()).level
              << " current_decision_level=" << trail.CurrentDecisionLevel();
    return false;
  }
  return true;
}

inline void Trail::RegisterPropagator(SatPropagator* propagator) {
  if (propagators_.empty()) {
    propagators_.resize(AssignmentType::kFirstFreePropagationId);
  }
  CHECK_LT(propagators_.size(), 16);
  VLOG(2) << "Registering propagator " << propagator->name() << " with id "
          << propagators_.size();
  propagator->SetPropagatorId(propagators_.size());
  propagators_.push_back(propagator);
}

inline BooleanVariable Trail::ReferenceVarWithSameReason(
    BooleanVariable var) const {
  DCHECK(Assignment().VariableIsAssigned(var));
  // Note that we don't use AssignmentType() here.
  if (info_[var].type == AssignmentType::kSameReasonAs) {
    var = reference_var_with_same_reason_as_[var];
    DCHECK(Assignment().VariableIsAssigned(var));
    DCHECK_NE(info_[var].type, AssignmentType::kSameReasonAs);
  }
  return var;
}

inline int Trail::AssignmentType(BooleanVariable var) const {
  if (info_[var].type == AssignmentType::kSameReasonAs) {
    var = reference_var_with_same_reason_as_[var];
    DCHECK_NE(info_[var].type, AssignmentType::kSameReasonAs);
  }
  const int type = info_[var].type;
  return type != AssignmentType::kCachedReason ? type : old_type_[var];
}

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_SAT_TRAIL_H_
