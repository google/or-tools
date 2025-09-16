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

#ifndef OR_TOOLS_SAT_ENFORCEMENT_H_
#define OR_TOOLS_SAT_ENFORCEMENT_H_

#include <cstdint>
#include <functional>
#include <ostream>
#include <utility>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/rev.h"
#include "ortools/util/strong_integers.h"

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
  CAN_PROPAGATE_ENFORCEMENT = 2,
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

  EnforcementStatus Status(EnforcementId id) const {
    if (id < 0) return EnforcementStatus::IS_ENFORCED;
    return statuses_[id];
  }

  // This recomputes the current status by scanning the given list.
  // It thus has a linear complexity and should not be used in hot loops.
  EnforcementStatus Status(absl::Span<const Literal> enforcement) const;

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
  RevRepository<int> rev_int_repository_;

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

  std::vector<EnforcementId> ids_to_fix_until_next_root_level_;

  friend class EnforcementHelper;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_ENFORCEMENT_H_
