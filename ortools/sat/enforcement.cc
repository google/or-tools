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

#include "ortools/sat/enforcement.h"

#include <algorithm>
#include <functional>
#include <ostream>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

std::ostream& operator<<(std::ostream& os, const EnforcementStatus& e) {
  switch (e) {
    case EnforcementStatus::IS_FALSE:
      os << "IS_FALSE";
      break;
    case EnforcementStatus::CANNOT_PROPAGATE:
      os << "CANNOT_PROPAGATE";
      break;
    case EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT:
      os << "CAN_PROPAGATE_ENFORCEMENT";
      break;
    case EnforcementStatus::IS_ENFORCED:
      os << "IS_ENFORCED";
      break;
  }
  return os;
}

EnforcementPropagator::EnforcementPropagator(Model* model)
    : SatPropagator("EnforcementPropagator"),
      trail_(*model->GetOrCreate<Trail>()),
      assignment_(trail_.Assignment()) {
  // Sentinel - also start of next Register().
  starts_.push_back(0);
}

bool EnforcementPropagator::Propagate(Trail* /*trail*/) {
  rev_int_repository_.SetLevel(trail_.CurrentDecisionLevel());
  rev_int_repository_.SaveStateWithStamp(&rev_stack_size_, &rev_stamp_);
  while (propagation_trail_index_ < trail_.Index()) {
    const Literal literal = trail_[propagation_trail_index_++];
    if (literal.Index() >= static_cast<int>(watcher_.size())) continue;

    int new_size = 0;
    auto& watch_list = watcher_[literal.Index()];
    for (const EnforcementId id : watch_list) {
      const LiteralIndex index = ProcessIdOnTrue(literal, id);
      if (index == kNoLiteralIndex) {
        // We keep the same watcher.
        watch_list[new_size++] = id;
      } else {
        // Change the watcher.
        CHECK_NE(index, literal.Index());
        watcher_[index].push_back(id);
      }
    }
    watch_list.resize(new_size);

    // We also mark some constraint false.
    for (const EnforcementId id : watcher_[literal.NegatedIndex()]) {
      ChangeStatus(id, EnforcementStatus::IS_FALSE);
    }
  }
  rev_stack_size_ = static_cast<int>(untrail_stack_.size());

  // Compute the enforcement status of any constraint added at a positive level.
  // This is only needed until we are back to level zero.
  for (const EnforcementId id : ids_to_fix_until_next_root_level_) {
    ChangeStatus(id, DebugStatus(id));
  }
  if (trail_.CurrentDecisionLevel() == 0) {
    ids_to_fix_until_next_root_level_.clear();
  }

  return true;
}

void EnforcementPropagator::Untrail(const Trail& /*trail*/, int trail_index) {
  rev_int_repository_.SetLevel(trail_.CurrentDecisionLevel());
  // Simply revert the status change.
  const int size = static_cast<int>(untrail_stack_.size());
  for (int i = size - 1; i >= rev_stack_size_; --i) {
    const auto [id, status] = untrail_stack_[i];
    statuses_[id] = status;
    if (callbacks_[id] != nullptr) callbacks_[id](id, status);
  }
  untrail_stack_.resize(rev_stack_size_);
  propagation_trail_index_ = trail_index;
}

// Adds a new constraint to the class and returns the constraint id.
//
// Note that we accept empty enforcement list so that client code can be used
// regardless of the presence of enforcement or not. A negative id means the
// constraint is never enforced, and should be ignored.
EnforcementId EnforcementPropagator::Register(
    absl::Span<const Literal> enforcement,
    std::function<void(EnforcementId, EnforcementStatus)> callback) {
  int num_true = 0;
  int num_false = 0;
  temp_literals_.clear();
  const int level = trail_.CurrentDecisionLevel();
  for (const Literal l : enforcement) {
    // Make sure we always have enough room for the literal and its negation.
    const int size = std::max(l.Index().value(), l.NegatedIndex().value()) + 1;
    if (size > static_cast<int>(watcher_.size())) {
      watcher_.resize(size);
    }
    if (assignment_.LiteralIsTrue(l)) {
      if (level == 0 || trail_.Info(l.Variable()).level == 0) continue;
      ++num_true;
    } else if (assignment_.LiteralIsFalse(l)) {
      ++num_false;
    }
    temp_literals_.push_back(l);
  }
  gtl::STLSortAndRemoveDuplicates(&temp_literals_);

  // Return special index if always enforced.
  if (temp_literals_.empty()) {
    if (callback != nullptr)
      callback(EnforcementId(-1), EnforcementStatus::IS_ENFORCED);
    return EnforcementId(-1);
  }

  const EnforcementId id(static_cast<int>(callbacks_.size()));
  callbacks_.push_back(std::move(callback));

  CHECK(!temp_literals_.empty());
  buffer_.insert(buffer_.end(), temp_literals_.begin(), temp_literals_.end());
  starts_.push_back(buffer_.size());  // Sentinel/next-start.

  // The default status at level zero.
  statuses_.push_back(temp_literals_.size() == 1
                          ? EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT
                          : EnforcementStatus::CANNOT_PROPAGATE);

  if (temp_literals_.size() == 1) {
    watcher_[temp_literals_[0].Index()].push_back(id);
  } else {
    // Make sure we watch correct literals.
    const auto span = GetSpan(id);
    int num_not_true = 0;
    for (int i = 0; i < span.size(); ++i) {
      if (assignment_.LiteralIsTrue(span[i])) continue;
      std::swap(span[num_not_true], span[i]);
      ++num_not_true;
      if (num_not_true == 2) break;
    }

    // We need to watch one of the literals at highest level.
    if (num_not_true == 1) {
      int max_level = trail_.Info(span[1].Variable()).level;
      for (int i = 2; i < span.size(); ++i) {
        const int level = trail_.Info(span[i].Variable()).level;
        if (level > max_level) {
          max_level = level;
          std::swap(span[1], span[i]);
        }
      }
    }

    watcher_[span[0].Index()].push_back(id);
    watcher_[span[1].Index()].push_back(id);
  }

  // Change status, call callback and set up untrail if the status is different
  // from EnforcementStatus::CANNOT_PROPAGATE.
  if (num_false > 0) {
    ChangeStatus(id, EnforcementStatus::IS_FALSE);
  } else if (num_true == temp_literals_.size()) {
    ChangeStatus(id, EnforcementStatus::IS_ENFORCED);
  } else if (num_true + 1 == temp_literals_.size()) {
    ChangeStatus(id, EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT);
    // Because this is the default status, we still need to call the callback.
    if (temp_literals_.size() == 1) {
      if (callbacks_[id] != nullptr) {
        callbacks_[id](id, EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT);
      }
    }
  }

  // Tricky: if we added something at a positive level, and its status is
  // not CANNOT_PROPAGATE, then we might need to fix it on backtrack.
  if (trail_.CurrentDecisionLevel() > 0 &&
      statuses_[id] != EnforcementStatus::CANNOT_PROPAGATE) {
    ids_to_fix_until_next_root_level_.push_back(id);
  }

  return id;
}

// Add the enforcement reason to the given vector.
void EnforcementPropagator::AddEnforcementReason(
    EnforcementId id, std::vector<Literal>* reason) const {
  for (const Literal l : GetSpan(id)) {
    reason->push_back(l.Negated());
  }
}

absl::Span<Literal> EnforcementPropagator::GetSpan(EnforcementId id) {
  if (id < 0) return {};
  DCHECK_LE(id + 1, starts_.size());
  const int size = starts_[id + 1] - starts_[id];
  DCHECK_NE(size, 0);
  return absl::MakeSpan(&buffer_[starts_[id]], size);
}

absl::Span<const Literal> EnforcementPropagator::GetSpan(
    EnforcementId id) const {
  if (id < 0) return {};
  DCHECK_LE(id + 1, starts_.size());
  const int size = starts_[id + 1] - starts_[id];
  DCHECK_NE(size, 0);
  return absl::MakeSpan(&buffer_[starts_[id]], size);
}

LiteralIndex EnforcementPropagator::ProcessIdOnTrue(Literal watched,
                                                    EnforcementId id) {
  const EnforcementStatus status = statuses_[id];
  if (status == EnforcementStatus::IS_FALSE) return kNoLiteralIndex;

  const auto span = GetSpan(id);
  if (span.size() == 1) {
    CHECK_EQ(status, EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT);
    ChangeStatus(id, EnforcementStatus::IS_ENFORCED);
    return kNoLiteralIndex;
  }

  const int watched_pos = (span[0] == watched) ? 0 : 1;
  CHECK_EQ(span[watched_pos], watched);
  if (assignment_.LiteralIsFalse(span[watched_pos ^ 1])) {
    ChangeStatus(id, EnforcementStatus::IS_FALSE);
    return kNoLiteralIndex;
  }

  for (int i = 2; i < span.size(); ++i) {
    const Literal l = span[i];
    if (assignment_.LiteralIsFalse(l)) {
      ChangeStatus(id, EnforcementStatus::IS_FALSE);
      return kNoLiteralIndex;
    }
    if (!assignment_.LiteralIsAssigned(l)) {
      // Replace the watched literal. Note that if the other watched literal is
      // true, it should be processed afterwards. We do not change the status
      std::swap(span[watched_pos], span[i]);
      return span[watched_pos].Index();
    }
  }

  // All literal with index > 1 are true. Two case.
  if (assignment_.LiteralIsTrue(span[watched_pos ^ 1])) {
    // All literals are true.
    ChangeStatus(id, EnforcementStatus::IS_ENFORCED);
    return kNoLiteralIndex;
  } else {
    // The other watched literal is the last unassigned
    CHECK_EQ(status, EnforcementStatus::CANNOT_PROPAGATE);
    ChangeStatus(id, EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT);
    return kNoLiteralIndex;
  }
}

void EnforcementPropagator::ChangeStatus(EnforcementId id,
                                         EnforcementStatus new_status) {
  const EnforcementStatus old_status = statuses_[id];
  if (old_status == new_status) return;
  if (trail_.CurrentDecisionLevel() != 0) {
    untrail_stack_.push_back({id, old_status});
  }
  statuses_[id] = new_status;
  if (callbacks_[id] != nullptr) callbacks_[id](id, new_status);
}

EnforcementStatus EnforcementPropagator::Status(
    absl::Span<const Literal> enforcement) const {
  int num_true = 0;
  for (const Literal l : enforcement) {
    if (assignment_.LiteralIsFalse(l)) {
      return EnforcementStatus::IS_FALSE;
    }
    if (assignment_.LiteralIsTrue(l)) ++num_true;
  }
  const int size = enforcement.size();
  if (num_true == size) return EnforcementStatus::IS_ENFORCED;
  if (num_true + 1 == size) return EnforcementStatus::CAN_PROPAGATE_ENFORCEMENT;
  return EnforcementStatus::CANNOT_PROPAGATE;
}

EnforcementStatus EnforcementPropagator::DebugStatus(EnforcementId id) {
  if (id < 0) return EnforcementStatus::IS_ENFORCED;
  return Status(GetSpan(id));
}

}  // namespace sat
}  // namespace operations_research
