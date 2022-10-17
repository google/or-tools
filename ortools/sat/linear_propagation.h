// Copyright 2010-2022 Google LLC
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

#include <deque>
#include <functional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"

namespace operations_research {
namespace sat {

DEFINE_STRONG_INDEX_TYPE(EnforcementId);

// A FIFO queue that allows some form of reordering of its element.
class CustomFifoQueue {
 public:
  CustomFifoQueue() {}

  // Note that this requires the queue to be empty or to never have been poped
  // before.
  void IncreaseSize(int n);

  int Pop();
  void Push(int id);

  bool empty() const { return left_ == right_; }
  bool Contains(int id) const { return pos_[id] != -1; }

  // Reorder the given element to match their given order. They must all be in
  // the queue.
  void Reorder(absl::Span<const int> order);
  void ReorderDense(absl::Span<const int> order);

  // Sorts the given elements by their current position from the top.
  // Elements should all be in the queue.
  void SortByPos(absl::Span<int> elements);

 private:
  // The queue is stored in [left_, right_) with eventual wrap around % size.
  // The positions of each element is in pos_[element] and never changes during
  // normal operation. A position of -1 means that the element is not in the
  // queue.
  std::vector<int> pos_;
  std::vector<int> queue_;
  int left_ = 0;
  int right_ = 0;

  std::vector<int> tmp_positions_;
  std::vector<int> tmp_order_;
};

// An enforced constraint can be in one of these 4 states.
// Note that we rely on the integer encoding to take 2 bits for optimization.
enum EnforcementStatus {
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
class EnforcementPropagator : SatPropagator {
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
      std::function<void(EnforcementStatus)> callback = nullptr);

  // Add the enforcement reason to the given vector.
  void AddEnforcementReason(EnforcementId id,
                            std::vector<Literal>* reason) const;

  // Try to propagate when the enforced constraint is not satisfiable.
  // This is currently in O(enforcement_size).
  ABSL_MUST_USE_RESULT bool PropagateWhenFalse(
      EnforcementId id, absl::Span<const Literal> literal_reason,
      absl::Span<const IntegerLiteral> integer_reason);

  EnforcementStatus Status(EnforcementId id) const { return statuses_[id]; }

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
  absl::StrongVector<EnforcementId, int> starts_;
  std::vector<Literal> buffer_;

  absl::StrongVector<EnforcementId, EnforcementStatus> statuses_;
  absl::StrongVector<EnforcementId, std::function<void(EnforcementStatus)>>
      callbacks_;

  // Used to restore status and call callback on untrail.
  std::vector<std::pair<EnforcementId, EnforcementStatus>> untrail_stack_;
  int rev_stack_size_ = 0;
  int64_t rev_stamp_ = 0;

  // We use a two watcher scheme.
  absl::StrongVector<LiteralIndex, absl::InlinedVector<EnforcementId, 6>>
      watcher_;

  std::vector<Literal> temp_literals_;
  std::vector<Literal> temp_reason_;
};

// This is meant to supersede both IntegerSumLE and the PrecedencePropagator.
//
// TODO(user): This is a work in progress and is currently incomplete:
// - Lack more incremental support for faster propag.
// - Lack detection and propagation of at least one of these linear is true
//   which can be used to propagate more bound if a variable appear in all these
//   constraint.
class LinearPropagator : public PropagatorInterface, ReversibleInterface {
 public:
  explicit LinearPropagator(Model* model);
  ~LinearPropagator() override;
  bool Propagate() final;
  void SetLevel(int level) final;

  // Adds a new constraint to the propagator.
  void AddConstraint(absl::Span<const Literal> enforcement_literals,
                     absl::Span<const IntegerVariable> vars,
                     absl::Span<const IntegerValue> coeffs,
                     IntegerValue upper_bound);

 private:
  // We try to pack the struct as much as possible. Using a maximum size of
  // 1 << 29 should be okay since we split long constraint anyway. Technically
  // we could use int16_t or even int8_t if we wanted, but we just need to make
  // sure we do split ALL constraints, not just the one from the initial mode.
  //
  // TODO(user): We could also move some less often used fields out. like
  // initial size and enf_id that are only needed when we push something.
  struct ConstraintInfo {
    unsigned int enf_status : 2;
    bool all_coeffs_are_one : 1;
    unsigned int initial_size : 29;  // Const. The size including all terms.

    EnforcementId enf_id;  // Const. The id in enforcement_propagator_.
    int start;             // Const. The start of the constraint in the buffers.
    int rev_size;          // The size of the non-fixed terms.
    IntegerValue rev_rhs;  // The current rhs, updated on fixed terms.
  };

#if !defined(_MSC_VER)
  static_assert(sizeof(ConstraintInfo) == 24,
                "ERROR_ConstraintInfo_is_not_well_compacted");
#endif  // !defined(_MSC_VER)

  absl::Span<IntegerValue> GetCoeffs(const ConstraintInfo& info);
  absl::Span<IntegerVariable> GetVariables(const ConstraintInfo& info);

  // Returns false on conflict.
  ABSL_MUST_USE_RESULT bool PropagateOneConstraint(int id);
  ABSL_MUST_USE_RESULT bool ReportConflictingCycle();
  ABSL_MUST_USE_RESULT bool DisassembleSubtree(int root_id, int num_pushed);

  void ClearPropagatedBy();
  void CanonicalizeConstraint(int id);
  void AddToQueueIfNeeded(int id);
  void AddWatchedToQueue(IntegerVariable var);
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
  SharedStatistics* shared_stats_ = nullptr;
  const int watcher_id_;

  // To know when we backtracked. See SetLevel().
  int previous_level_ = 0;

  // The key to our incrementality. This will be cleared once the propagation
  // is done, and automatically updated by the integer_trail_ with all the
  // IntegerVariable that changed since the last clear.
  SparseBitset<IntegerVariable> modified_vars_;

  // Per constraint info used during propagation.
  std::vector<ConstraintInfo> infos_;

  // Buffer of the constraints data.
  //
  // TODO(user): A lot of constrains have all their coeffs at one, we could
  // exploit this.
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
  std::vector<bool> in_queue_;
  CustomFifoQueue propagation_queue_;

  int rev_at_false_size_ = 0;
  std::vector<int> in_queue_and_at_false_;

  // Watchers.
  absl::StrongVector<IntegerVariable, bool> is_watched_;
  absl::StrongVector<IntegerVariable, absl::InlinedVector<int, 6>>
      var_to_constraint_ids_;

  // For an heuristic similar to Tarjan contribution to Bellman-Ford algorithm.
  // We mark for each variable the last constraint that pushed it, and also keep
  // the count of propagated variable for each constraint.
  SparseBitset<IntegerVariable> propagated_by_was_set_;
  absl::StrongVector<IntegerVariable, int> propagated_by_;
  std::vector<int> id_to_propagation_count_;

  // Used by DissasembleSubtreeAndAddToQueue().
  struct DissasembleQueueEntry {
    int id;
    IntegerVariable var;
  };
  std::vector<DissasembleQueueEntry> disassemble_queue_;
  std::vector<std::pair<int, IntegerVariable>> disassemble_branch_;
  std::vector<std::pair<IntegerVariable, IntegerValue>> disassemble_candidates_;
  std::vector<int> tmp_to_reorder_;
  SparseBitset<int> disassemble_to_reorder_;
  std::vector<int> disassemble_reverse_topo_order_;

  // Staging queue.
  // Initially, we add the constraint to the priority queue, and we extract
  // them one by one, each time reaching the propagation fixed point.
  std::vector<bool> pq_was_added_;
  bool pq_in_heap_form_ = false;
  std::vector<int> pq_;
  std::vector<int> pq_to_clean_;

  // Stats. Allow to track the time a constraint is scanned more than once.
  // This is only used in --v 1.
  SparseBitset<int> id_scanned_at_least_once_;
  int64_t num_extra_scans_ = 0;

  // Stats.
  int64_t num_pushes_ = 0;
  int64_t num_enforcement_pushes_ = 0;
  int64_t num_simple_cycles_ = 0;
  int64_t num_complex_cycles_ = 0;
  int64_t num_scanned_ = 0;
  int64_t num_explored_in_disassemble_ = 0;
  int64_t num_reordered_ = 0;
  int64_t num_bool_aborts_ = 0;
  int64_t num_ignored_ = 0;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_LINEAR_PROPAGATION_H_
