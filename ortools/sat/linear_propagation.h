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

#ifndef OR_TOOLS_SAT_LINEAR_PROPAGATION_H_
#define OR_TOOLS_SAT_LINEAR_PROPAGATION_H_

#include <stdint.h>

#include <deque>
#include <functional>
#include <limits>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"
#include "ortools/util/rev.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

DEFINE_STRONG_INDEX_TYPE(EnforcementId);

// An enforced constraint can be in one of these 4 states.
// Note that we rely on the integer encoding to take 2 bits for optimization.
enum class EnforcementStatus {
  // One enforcement literal is false.
  IS_FALSE = 0,
  // More than two literals are unassigned.
  CANNOT_PROPAGATE = 1,
  // All enforcement literals are true but one.
  CAN_PROPAGATE = 2,
  // All enforcement literals are true.
  IS_ENFORCED = 3,
};

std::ostream& operator<<(std::ostream& os, const EnforcementStatus& e);

// This is meant as an helper to deal with enforcement for any constraint.
class EnforcementPropagator : public SatPropagator {
 public:
  explicit EnforcementPropagator(Model* model);

  // SatPropagator interface.
  bool Propagate(Trail* trail) final;
  void Untrail(const Trail& trail, int trail_index) final;

  // Adds a new constraint to the class and register a callback that will
  // be called on status change. Note that we also call the callback with the
  // initial status if different from CANNOT_PROPAGATE when added.
  //
  // It is better to not call this for empty enforcement list, but you can. A
  // negative id means the level zero status will never change, and only the
  // first call to callback() should be necessary, we don't save it.
  EnforcementId Register(
      absl::Span<const Literal> enforcement,
      std::function<void(EnforcementId, EnforcementStatus)> callback = nullptr);

  // Add the enforcement reason to the given vector.
  void AddEnforcementReason(EnforcementId id,
                            std::vector<Literal>* reason) const;

  // Try to propagate when the enforced constraint is not satisfiable.
  // This is currently in O(enforcement_size).
  ABSL_MUST_USE_RESULT bool PropagateWhenFalse(
      EnforcementId id, absl::Span<const Literal> literal_reason,
      absl::Span<const IntegerLiteral> integer_reason);

  EnforcementStatus Status(EnforcementId id) const { return statuses_[id]; }

  // Recompute the status from the current assignment.
  // This should only used in DCHECK().
  EnforcementStatus DebugStatus(EnforcementId id);

  // Returns the enforcement literals of the given id.
  absl::Span<const Literal> GetEnforcementLiterals(EnforcementId id) const {
    if (id < 0) return {};
    return GetSpan(id);
  }

 private:
  absl::Span<Literal> GetSpan(EnforcementId id);
  absl::Span<const Literal> GetSpan(EnforcementId id) const;
  void ChangeStatus(EnforcementId id, EnforcementStatus new_status);

  // Returns kNoLiteralIndex if nothing need to change or a new literal to
  // watch. This also calls the registered callback.
  LiteralIndex ProcessIdOnTrue(Literal watched, EnforcementId id);

  // External classes.
  const Trail& trail_;
  const VariablesAssignment& assignment_;
  IntegerTrail* integer_trail_;
  RevIntRepository* rev_int_repository_;

  // All enforcement will be copied there, and we will create Span out of this.
  // Note that we don't store the span so that we are not invalidated on buffer_
  // resizing.
  util_intops::StrongVector<EnforcementId, int> starts_;
  std::vector<Literal> buffer_;

  util_intops::StrongVector<EnforcementId, EnforcementStatus> statuses_;
  util_intops::StrongVector<
      EnforcementId, std::function<void(EnforcementId, EnforcementStatus)>>
      callbacks_;

  // Used to restore status and call callback on untrail.
  std::vector<std::pair<EnforcementId, EnforcementStatus>> untrail_stack_;
  int rev_stack_size_ = 0;
  int64_t rev_stamp_ = 0;

  // We use a two watcher scheme.
  util_intops::StrongVector<LiteralIndex, absl::InlinedVector<EnforcementId, 6>>
      watcher_;

  std::vector<Literal> temp_literals_;
  std::vector<Literal> temp_reason_;
};

// Helper class to decide on the constraint propagation order.
//
// Each constraint might push some variables which might in turn make other
// constraint tighter. In general, it seems better to make sure we push first
// constraints that are not affected by other variables and delay the
// propagation of constraint that we know will become tigher.
//
// Note that we can have cycle in this graph, and that this is not necessarily a
// conflict.
class ConstraintPropagationOrder {
 public:
  ConstraintPropagationOrder(
      ModelRandomGenerator* random,
      std::function<absl::Span<const IntegerVariable>(int)> id_to_vars)
      : random_(random), id_to_vars_func_(std::move(id_to_vars)) {}

  void Resize(int num_vars, int num_ids) {
    var_has_entry_.Resize(IntegerVariable(num_vars));
    var_to_id_.resize(num_vars);
    var_to_lb_.resize(num_vars);
    var_to_pos_.resize(num_vars);

    in_ids_.Resize(num_ids);
  }

  void Register(int id, IntegerVariable var, IntegerValue lb) {
    DCHECK_LT(id, in_ids_.size());
    DCHECK_LT(var.value(), var_to_id_.size());
    if (var_has_entry_[var]) {
      if (var_to_lb_[var] >= lb) return;
    } else {
      var_has_entry_.Set(var);
      var_to_pos_[var] = to_clear_.size();
      to_clear_.push_back(var);
    }
    var_to_lb_[var] = lb;
    var_to_id_[var] = id;
    if (!in_ids_[id]) {
      in_ids_.Set(id);

      // All new ids are likely not in a cycle, we want to test them first.
      ids_.push_front(id);
    }
  }

  void Clear() {
    for (const IntegerVariable var : to_clear_) {
      var_has_entry_.Clear(var);
    }
    to_clear_.clear();
    for (const int id : ids_) {
      in_ids_.Clear(id);
    }
    ids_.clear();
  }

  // Return -1 if there is none.
  // This returns a constraint with min degree.
  //
  // TODO(user): fix quadratic algo? We can use var_to_ids_func_() to maintain
  // the degree. But note that with the start_ optim and because we expect
  // mainly degree zero, this seems to be faster.
  int NextId() {
    if (ids_.empty()) return -1;

    int best_id = 0;
    int best_num_vars = 0;
    int best_degree = std::numeric_limits<int>::max();
    const int size = ids_.size();
    const auto var_has_entry = var_has_entry_.const_view();
    for (int i = 0; i < size; ++i) {
      const int id = ids_.front();
      ids_.pop_front();
      DCHECK(in_ids_[id]);

      int degree = 0;
      absl::Span<const IntegerVariable> vars = id_to_vars_func_(id);
      for (const IntegerVariable var : vars) {
        if (var_has_entry[var]) ++degree;
      }

      // We select the min-degree and prefer lower constraint size.
      // We however stop at the first degree zero.
      if (degree < best_degree ||
          (degree == best_degree && vars.size() < best_num_vars)) {
        best_degree = degree;
        best_num_vars = vars.size();
        best_id = id;
        if (best_degree == 0) {
          in_ids_.Clear(best_id);
          return best_id;
        }
      }

      ids_.push_back(id);
    }

    // We didn't find any degree zero, we scanned the whole queue.
    // Extract best_id while keeping the order stable.
    //
    // We tried to randomize the order, it does add more variance but also seem
    // worse overall.
    int new_size = 0;
    for (const int id : ids_) {
      if (id == best_id) continue;
      ids_[new_size++] = id;
    }
    ids_.resize(new_size);
    in_ids_.Clear(best_id);
    return best_id;
  }

  void UpdateBound(IntegerVariable var, IntegerValue lb) {
    if (!var_has_entry_[var]) return;
    if (lb < var_to_lb_[var]) return;

    var_has_entry_.Clear(var);
    const int pos = var_to_pos_[var];
    to_clear_[pos] = to_clear_.back();
    var_to_pos_[to_clear_.back()] = pos;
    to_clear_.pop_back();
  }

  bool IsEmpty() const { return ids_.empty(); }

  bool VarShouldBePushedById(IntegerVariable var, int id) {
    if (!var_has_entry_[var]) return false;
    if (var_to_id_[var] != id) return false;
    return true;
  }

 public:
  ModelRandomGenerator* random_;
  std::function<absl::Span<const IntegerVariable>(int)> id_to_vars_func_;

  // For each variable we only keep the constraint id that pushes it further.
  // In case of tie, we only keep the first to be registered.
  Bitset64<IntegerVariable> var_has_entry_;
  util_intops::StrongVector<IntegerVariable, int> var_to_id_;
  util_intops::StrongVector<IntegerVariable, IntegerValue> var_to_lb_;
  util_intops::StrongVector<IntegerVariable, int> var_to_pos_;
  std::vector<IntegerVariable> to_clear_;

  // Set/queue of constraints to be propagated.
  int start_ = 0;
  Bitset64<int> in_ids_;
  std::deque<int> ids_;
};

// This is meant to supersede both IntegerSumLE and the PrecedencePropagator.
//
// TODO(user): This is a work in progress and is currently incomplete:
// - Lack more incremental support for faster propag.
// - Lack detection and propagation of at least one of these linear is true
//   which can be used to propagate more bound if a variable appear in all these
//   constraint.
class LinearPropagator : public PropagatorInterface,
                         ReversibleInterface,
                         LazyReasonInterface {
 public:
  explicit LinearPropagator(Model* model);
  ~LinearPropagator() override;
  bool Propagate() final;
  void SetLevel(int level) final;

  // Adds a new constraint to the propagator.
  // We support adding constraint at a positive level:
  //  - This will push new propagation right away.
  //  - This will returns false if the constraint is currently a conflict.
  bool AddConstraint(absl::Span<const Literal> enforcement_literals,
                     absl::Span<const IntegerVariable> vars,
                     absl::Span<const IntegerValue> coeffs,
                     IntegerValue upper_bound);

  // For LazyReasonInterface.
  void Explain(int id, IntegerValue propagation_slack,
               IntegerVariable var_to_explain, int trail_index,
               std::vector<Literal>* literals_reason,
               std::vector<int>* trail_indices_reason) final;

 private:
  // We try to pack the struct as much as possible. Using a maximum size of
  // 1 << 29 should be okay since we split long constraint anyway. Technically
  // we could use int16_t or even int8_t if we wanted, but we just need to make
  // sure we do split ALL constraints, not just the one from the initial model.
  //
  // TODO(user): We could also move some less often used fields out. like
  // initial size and enf_id that are only needed when we push something.
  struct ConstraintInfo {
    unsigned int enf_status : 2;
    // With Visual Studio or minGW, using bool here breaks the struct packing.
    unsigned int all_coeffs_are_one : 1;
    unsigned int initial_size : 29;  // Const. The size including all terms.

    EnforcementId enf_id;  // Const. The id in enforcement_propagator_.
    int start;             // Const. The start of the constraint in the buffers.
    int rev_size;          // The size of the non-fixed terms.
    IntegerValue rev_rhs;  // The current rhs, updated on fixed terms.
  };

  static_assert(sizeof(ConstraintInfo) == 24,
                "ERROR_ConstraintInfo_is_not_well_compacted");

  absl::Span<IntegerValue> GetCoeffs(const ConstraintInfo& info);
  absl::Span<IntegerVariable> GetVariables(const ConstraintInfo& info);

  // Called when the lower bound of a variable changed. The id is the constraint
  // id that caused this change or -1 if it comes from an external source.
  void OnVariableChange(IntegerVariable var, IntegerValue lb, int id);

  // Returns false on conflict.
  ABSL_MUST_USE_RESULT bool PropagateOneConstraint(int id);
  ABSL_MUST_USE_RESULT bool PropagateInfeasibleConstraint(int id,
                                                          IntegerValue slack);
  ABSL_MUST_USE_RESULT bool ReportConflictingCycle();
  ABSL_MUST_USE_RESULT bool DisassembleSubtree(int root_id, int num_tight);

  // This loops over the given constraint and returns the coefficient of var and
  // NegationOf(next_var). Both should be non-zero (Checked).
  //
  // If the slack of this constraint was tight for next_var, then pushing var
  // will push next_var again depending on these coefficients.
  std::pair<IntegerValue, IntegerValue> GetCycleCoefficients(
      int id, IntegerVariable var, IntegerVariable next_var);

  // Returns (slack, num_to_push) of the given constraint.
  // If slack < 0 we have a conflict or might push the enforcement.
  // If slack >= 0 the first num_to_push variables can be pushed.
  std::pair<IntegerValue, int> AnalyzeConstraint(int id);

  void ClearPropagatedBy();
  void CanonicalizeConstraint(int id);
  void AddToQueueIfNeeded(int id);
  void SetPropagatedBy(IntegerVariable var, int id);
  std::string ConstraintDebugString(int id);

  // External class needed.
  Trail* trail_;
  IntegerTrail* integer_trail_;
  EnforcementPropagator* enforcement_propagator_;
  GenericLiteralWatcher* watcher_;
  TimeLimit* time_limit_;
  RevIntRepository* rev_int_repository_;
  RevIntegerValueRepository* rev_integer_value_repository_;
  PrecedenceRelations* precedences_;
  ModelRandomGenerator* random_;
  SharedStatistics* shared_stats_ = nullptr;
  const int watcher_id_;

  // To know when we backtracked. See SetLevel().
  int previous_level_ = 0;

  // The key to our incrementality. This will be cleared once the propagation
  // is done, and automatically updated by the integer_trail_ with all the
  // IntegerVariable that changed since the last clear.
  SparseBitset<IntegerVariable> modified_vars_;

  // Per constraint info used during propagation. Note that we keep pointer for
  // the rev_size/rhs there, so we do need a deque.
  std::deque<ConstraintInfo> infos_;
  std::vector<IntegerValue> initial_rhs_;

  // Buffer of the constraints data.
  std::vector<IntegerVariable> variables_buffer_;
  std::vector<IntegerValue> coeffs_buffer_;
  std::vector<IntegerValue> buffer_of_ones_;

  // Filled by PropagateOneConstraint().
  std::vector<IntegerValue> max_variations_;

  // For reasons computation. Parallel vectors.
  std::vector<IntegerLiteral> integer_reason_;
  std::vector<IntegerValue> reason_coeffs_;
  std::vector<Literal> literal_reason_;

  // Queue of constraint to propagate.
  Bitset64<int> in_queue_;
  std::deque<int> propagation_queue_;

  // This only contain constraint that currently push some bounds.
  ConstraintPropagationOrder order_;

  // Unenforced constraints are marked as "in_queue_" but not actually added
  // to the propagation_queue_.
  int rev_unenforced_size_ = 0;
  std::vector<int> unenforced_constraints_;

  // Watchers.
  util_intops::StrongVector<IntegerVariable, bool> is_watched_;
  util_intops::StrongVector<IntegerVariable, absl::InlinedVector<int, 6>>
      var_to_constraint_ids_;

  // For an heuristic similar to Tarjan contribution to Bellman-Ford algorithm.
  // We mark for each variable the last constraint that pushed it, and also keep
  // the count of propagated variable for each constraint.
  SparseBitset<IntegerVariable> propagated_by_was_set_;
  util_intops::StrongVector<IntegerVariable, int> propagated_by_;
  std::vector<int> id_to_propagation_count_;

  // Used by DissasembleSubtreeAndAddToQueue().
  struct DissasembleQueueEntry {
    int id;
    IntegerVariable var;
    IntegerValue increase;
  };
  std::vector<DissasembleQueueEntry> disassemble_queue_;
  std::vector<DissasembleQueueEntry> disassemble_branch_;
  std::vector<std::pair<IntegerVariable, IntegerValue>> disassemble_candidates_;

  // This is used to update the deterministic time.
  int64_t num_terms_for_dtime_update_ = 0;

  // Stats.
  int64_t num_pushes_ = 0;
  int64_t num_enforcement_pushes_ = 0;
  int64_t num_cycles_ = 0;
  int64_t num_failed_cycles_ = 0;
  int64_t num_short_reasons_ = 0;
  int64_t num_long_reasons_ = 0;
  int64_t num_scanned_ = 0;
  int64_t num_explored_in_disassemble_ = 0;
  int64_t num_delayed_ = 0;
  int64_t num_bool_aborts_ = 0;
  int64_t num_loop_aborts_ = 0;
  int64_t num_ignored_ = 0;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_LINEAR_PROPAGATION_H_
