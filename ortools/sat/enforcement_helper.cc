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

#include "ortools/sat/enforcement_helper.h"

#include <functional>
#include <vector>

#include "absl/types/span.h"
#include "ortools/sat/enforcement.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

EnforcementHelper::EnforcementHelper(Model* model)
    : enforcement_propagator_(*model->GetOrCreate<EnforcementPropagator>()),
      assignment_(model->GetOrCreate<Trail>()->Assignment()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()) {}

EnforcementId EnforcementHelper::Register(
    absl::Span<const Literal> enforcement_literals,
    GenericLiteralWatcher* watcher, int literal_watcher_id) {
  return enforcement_propagator_.Register(
      enforcement_literals, [=](EnforcementId, EnforcementStatus status) {
        if (status == EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT ||
            status == EnforcementStatus::IS_ENFORCED) {
          watcher->CallOnNextPropagate(literal_watcher_id);
        }
      });
}

// Try to propagate when the enforced constraint is not satisfiable.
// This is currently in O(enforcement_size);
bool EnforcementHelper::PropagateWhenFalse(
    EnforcementId id, absl::Span<const Literal> literal_reason,
    absl::Span<const IntegerLiteral> integer_reason) {
  temp_reason_.clear();
  LiteralIndex unique_unassigned = kNoLiteralIndex;
  for (const Literal l : enforcement_propagator_.GetSpan(id)) {
    if (assignment_.LiteralIsFalse(l)) return true;
    if (assignment_.LiteralIsTrue(l)) {
      temp_reason_.push_back(l.Negated());
      continue;
    }
    if (unique_unassigned != kNoLiteralIndex) return true;
    unique_unassigned = l.Index();
  }

  temp_reason_.insert(temp_reason_.end(), literal_reason.begin(),
                      literal_reason.end());
  if (unique_unassigned == kNoLiteralIndex) {
    return integer_trail_->ReportConflict(temp_reason_, integer_reason);
  }

  // We also change the status right away.
  enforcement_propagator_.ChangeStatus(id, EnforcementStatus::IS_FALSE);
  return integer_trail_->SafeEnqueueLiteral(
      Literal(unique_unassigned).Negated(), temp_reason_, integer_reason);
}

bool EnforcementHelper::Enqueue(
    EnforcementId id, IntegerLiteral i_lit,
    absl::Span<const Literal> literal_reason,
    absl::Span<const IntegerLiteral> integer_reason) {
  temp_reason_.clear();
  temp_reason_.insert(temp_reason_.end(), literal_reason.begin(),
                      literal_reason.end());
  enforcement_propagator_.AddEnforcementReason(id, &temp_reason_);
  return integer_trail_->Enqueue(i_lit, temp_reason_, integer_reason);
}

bool EnforcementHelper::SafeEnqueue(
    EnforcementId id, IntegerLiteral i_lit,
    absl::Span<const IntegerLiteral> integer_reason) {
  temp_reason_.clear();
  enforcement_propagator_.AddEnforcementReason(id, &temp_reason_);
  return integer_trail_->SafeEnqueue(i_lit, temp_reason_, integer_reason);
}

bool EnforcementHelper::ConditionalEnqueue(
    EnforcementId id, Literal lit, IntegerLiteral i_lit,
    absl::Span<const Literal> literal_reason,
    absl::Span<const IntegerLiteral> integer_reason) {
  temp_reason_.clear();
  temp_reason_.insert(temp_reason_.end(), literal_reason.begin(),
                      literal_reason.end());
  enforcement_propagator_.AddEnforcementReason(id, &temp_reason_);
  temp_integer_reason_.clear();
  temp_integer_reason_.insert(temp_integer_reason_.end(),
                              integer_reason.begin(), integer_reason.end());
  return integer_trail_->ConditionalEnqueue(lit, i_lit, &temp_reason_,
                                            &temp_integer_reason_);
}

bool EnforcementHelper::EnqueueLiteral(
    EnforcementId id, Literal literal, absl::Span<const Literal> literal_reason,
    absl::Span<const IntegerLiteral> integer_reason) {
  temp_reason_.clear();
  temp_reason_.insert(temp_reason_.end(), literal_reason.begin(),
                      literal_reason.end());
  enforcement_propagator_.AddEnforcementReason(id, &temp_reason_);
  return integer_trail_->EnqueueLiteral(literal, temp_reason_, integer_reason);
}

bool EnforcementHelper::ReportConflict(
    EnforcementId id, absl::Span<const Literal> literal_reason,
    absl::Span<const IntegerLiteral> integer_reason) {
  temp_reason_.clear();
  temp_reason_.insert(temp_reason_.end(), literal_reason.begin(),
                      literal_reason.end());
  enforcement_propagator_.AddEnforcementReason(id, &temp_reason_);
  return integer_trail_->ReportConflict(temp_reason_, integer_reason);
}

}  // namespace sat
}  // namespace operations_research
