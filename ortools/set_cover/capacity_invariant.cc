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

#include "ortools/set_cover/capacity_invariant.h"

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "ortools/set_cover/capacity_model.h"
#include "ortools/set_cover/set_cover_model.h"

namespace operations_research {

void CapacityInvariant::Clear() {
  current_slack_ = 0.0;
  is_selected_.assign(set_cover_model_->num_subsets(), false);
}

double CapacityInvariant::ComputeSlackChange(const SubsetIndex subset) const {
  double slack_change = 0.0;
  for (CapacityTermIndex term : model_->TermRange()) {
    if (model_->GetTermSubsetIndex(term) == subset) {
      // Hypothesis: GetTermSubsetIndex(term) is an element of the subset.
      // This information is stored in a SetCoverModel instance.
      slack_change += model_->GetTermCapacityWeight(term);
    }
  }
  return slack_change;
}

bool CapacityInvariant::SlackChangeFitsConstraint(
    const double slack_change) const {
  const double new_slack = current_slack_ + slack_change;
  return new_slack >= model_->GetMinimumCapacity() &&
         new_slack <= model_->GetMaximumCapacity();
}

bool CapacityInvariant::Flip(SubsetIndex subset) {
  DCHECK_LT(subset.value(), set_cover_model_->num_subsets())
      << "Invalid subset: " << subset;
  return !is_selected_[subset] ? Select(subset) : Deselect(subset);
}

bool CapacityInvariant::CanFlip(SubsetIndex subset) const {
  DCHECK_LT(subset.value(), set_cover_model_->num_subsets())
      << "Invalid subset: " << subset;
  return !is_selected_[subset] ? CanSelect(subset) : CanDeselect(subset);
}

bool CapacityInvariant::Select(SubsetIndex subset) {
  DVLOG(1) << "[Capacity constraint] Selecting subset " << subset;
  DCHECK(!is_selected_[subset]);

  const double slack_change = ComputeSlackChange(subset);
  if (!SlackChangeFitsConstraint(slack_change)) {
    DVLOG(1) << "[Capacity constraint] Selecting subset " << subset
             << ": infeasible";
    return false;
  }

  is_selected_[subset] = true;
  current_slack_ += slack_change;
  DVLOG(1) << "[Capacity constraint] New slack: " << current_slack_;
  return true;
}

bool CapacityInvariant::CanSelect(SubsetIndex subset) const {
  DVLOG(1) << "[Capacity constraint] Can select subset " << subset << "?";
  DCHECK(!is_selected_[subset]);

  const double slack_change = ComputeSlackChange(subset);
  DVLOG(1) << "[Capacity constraint] New slack if selecting: "
           << current_slack_ + slack_change;
  return SlackChangeFitsConstraint(slack_change);
}

bool CapacityInvariant::Deselect(SubsetIndex subset) {
  DVLOG(1) << "[Capacity constraint] Deselecting subset " << subset;
  DCHECK(is_selected_[subset]);

  const double slack_change = -ComputeSlackChange(subset);
  if (!SlackChangeFitsConstraint(slack_change)) {
    DVLOG(1) << "[Capacity constraint] Deselecting subset " << subset
             << ": infeasible";
    return false;
  }

  is_selected_[subset] = false;
  current_slack_ += slack_change;
  DVLOG(1) << "[Capacity constraint] New slack: " << current_slack_;
  return true;
}

bool CapacityInvariant::CanDeselect(SubsetIndex subset) const {
  DVLOG(1) << "[Capacity constraint] Can deselect subset " << subset << "?";
  DCHECK(is_selected_[subset]);

  const double slack_change = -ComputeSlackChange(subset);
  DVLOG(1) << "[Capacity constraint] New slack if deselecting: "
           << current_slack_ + slack_change;
  return SlackChangeFitsConstraint(slack_change);
}
}  // namespace operations_research
