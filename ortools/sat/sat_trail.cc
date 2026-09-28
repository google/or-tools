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

#include "ortools/sat/sat_trail.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/log_severity.h"
#include "ortools/sat/sat_assignment.h"
#include "ortools/sat/sat_literal.h"

namespace operations_research {
namespace sat {

void Trail::Resize(int num_variables) {
  assignment_.Resize(num_variables);
  info_.resize(num_variables);
  trail_.resize(num_variables);
  reasons_.resize(num_variables);

  // TODO(user): these vectors are not always used. Initialize them
  // dynamically.
  old_type_.resize(num_variables);
  reference_var_with_same_reason_as_.resize(num_variables);

  // The +1 is a bit tricky, it is because in
  // EnqueueDecisionAndBacktrackOnConflict() we artificially enqueue the
  // decision before checking if it is not already assigned.
  decisions_.resize(num_variables + 1);
}

absl::Span<const Literal> Trail::Reason(BooleanVariable var,
                                        int64_t conflict_id) const {
  // Special case for AssignmentType::kSameReasonAs to avoid a recursive call.
  var = ReferenceVarWithSameReason(var);

  // Fast-track for cached reason.
  if (info_[var].type == AssignmentType::kCachedReason) {
    if (DEBUG_MODE && debug_checker_ != nullptr) {
      std::vector<Literal> clause;
      clause.assign(reasons_[var].begin(), reasons_[var].end());
      clause.push_back(assignment_.GetTrueLiteralForAssignedVariable(var));
      CHECK(debug_checker_(clause)) << " for cached reason";
    }
    return reasons_[var];
  }

  const AssignmentInfo& info = info_[var];
  if (info.type == AssignmentType::kUnitReason ||
      info.type == AssignmentType::kSearchDecision) {
    reasons_[var] = {};
  } else {
    DCHECK_LT(info.type, propagators_.size());
    DCHECK(propagators_[info.type] != nullptr) << info.type;
    reasons_[var] =
        propagators_[info.type]->Reason(*this, info.trail_index, conflict_id);
  }
  old_type_[var] = info.type;
  info_[var].type = AssignmentType::kCachedReason;
  if (DEBUG_MODE && debug_checker_ != nullptr) {
    std::vector<Literal> clause;
    clause.assign(reasons_[var].begin(), reasons_[var].end());
    clause.push_back(assignment_.GetTrueLiteralForAssignedVariable(var));
    CHECK(debug_checker_(clause)) << "for propagator_id=" << old_type_[var];
  }
  return reasons_[var];
}

void Trail::ReimplyAll(int old_trail_index) {
  const int64_t initial_num_reimplied = num_reimplied_literals_;
  for (int i = Index(); i < old_trail_index; ++i) {
    const Literal literal = trail_[i];
    const AssignmentInfo& info = Info(literal.Variable());
    if (info.level > current_info_.level) continue;
    CHECK_LE(Index(), i);
    CHECK(!Assignment().VariableIsAssigned(literal.Variable()));
    if (info.type == AssignmentType::kSameReasonAs) {
      // The reference variable must already be re-implied at this level, so we
      // can just re-enqueue it without having to tell the propagator.
      DCHECK_EQ(Info(ReferenceVarWithSameReason(literal.Variable())).level,
                info.level);
      DCHECK_LT(
          Info(ReferenceVarWithSameReason(literal.Variable())).trail_index,
          Index());
      EnqueueAtLevel(literal, AssignmentType::kSameReasonAs, info.level);
    } else {
      const int original_type = AssignmentType(literal.Variable());
      if (original_type >= AssignmentType::kFirstFreePropagationId) {
        propagators_[original_type]->Reimply(this, i);
      } else if (original_type == AssignmentType::kCachedReason) {
        std::swap(reasons_repository_[Index()], reasons_repository_[i]);
        reasons_[literal.Variable()] = reasons_repository_[Index()];
        EnqueueAtLevel(literal, original_type, info.level);
      } else if (info.type == AssignmentType::kUnitReason || info.level == 0) {
        CHECK(!Assignment().LiteralIsFalse(literal));
        EnqueueAtLevel(literal, AssignmentType::kUnitReason, info.level);
      }
    }
    num_reimplied_literals_ += assignment_.LiteralIsTrue(literal);
  }
  last_num_reimplication_ = num_reimplied_literals_ - initial_num_reimplied;
}

}  // namespace sat
}  // namespace operations_research
