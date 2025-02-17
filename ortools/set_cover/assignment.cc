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

#include "ortools/set_cover/assignment.h"

#include "absl/log/check.h"
#include "ortools/base/logging.h"
#include "ortools/base/mathutil.h"
#include "ortools/set_cover/capacity_invariant.h"
#include "ortools/set_cover/set_cover_invariant.h"
#include "ortools/set_cover/set_cover_model.h"

namespace operations_research {

void SetCoverAssignment::Clear() {
  cost_ = Cost(0.0);
  values_.assign(model_.subset_costs().size(), false);
  DCHECK_EQ(values_.size(), model_.subset_costs().size())
      << "The cost vector (length: " << model_.subset_costs().size()
      << ") is inconsistent with the assignment (length: " << values_.size()
      << ")";
}

void SetCoverAssignment::AttachInvariant(SetCoverInvariant* i) {
  CHECK(constraint_ == nullptr);
  constraint_ = i;
}

void SetCoverAssignment::AttachInvariant(CapacityInvariant* i) {
  CHECK(constraint_ != nullptr);
  side_constraints_.push_back(i);
  // TODO(user): call i->SetAssignment or similar so that each and every
  // constraint uses the same solution storage.
}

void SetCoverAssignment::SetValue(
    SubsetIndex subset, bool is_selected,
    SetCoverInvariant::ConsistencyLevel set_cover_consistency) {
  DVLOG(1) << "[Assignment] Subset " << subset << " becoming " << is_selected
           << "; used to be " << values_[subset];

  DCHECK(CheckConsistency());
  if (values_[subset] == is_selected) return;

  values_[subset] = is_selected;
  if (is_selected) {
    cost_ += model_.subset_costs()[subset];
    if (constraint_) {
      constraint_->Select(subset, set_cover_consistency);
    }
    for (CapacityInvariant* const capacity_constraint : side_constraints_) {
      capacity_constraint->Select(subset);
    }
  } else {
    cost_ -= model_.subset_costs()[subset];
    if (constraint_) {
      constraint_->Deselect(subset, set_cover_consistency);
    }
    for (CapacityInvariant* const capacity_constraint : side_constraints_) {
      capacity_constraint->Deselect(subset);
    }
  }
  DCHECK(CheckConsistency());
}

SetCoverSolutionResponse SetCoverAssignment::ExportSolutionAsProto() const {
  SetCoverSolutionResponse message;
  message.set_num_subsets(values_.size());
  message.set_cost(cost_);
  for (SubsetIndex subset(0);
       subset < SubsetIndex(model_.subset_costs().size()); ++subset) {
    if (values_[subset]) {
      message.add_subset(subset.value());
    }
  }
  return message;
}

void SetCoverAssignment::LoadAssignment(const SubsetBoolVector& solution) {
  DCHECK_EQ(solution.size(), values_.size());
  values_ = solution;
  cost_ = ComputeCost(values_);
}

void SetCoverAssignment::ImportSolutionFromProto(
    const SetCoverSolutionResponse& message) {
  values_.resize(SubsetIndex(message.num_subsets()), false);
  cost_ = Cost(0.0);
  for (auto s : message.subset()) {
    SubsetIndex subset(s);
    values_[subset] = true;
    cost_ += model_.subset_costs()[subset];
  }
  CHECK(MathUtil::AlmostEquals(message.cost(), cost_));
  DCHECK(CheckConsistency());
}

bool SetCoverAssignment::CheckConsistency() const {
  Cost cst = ComputeCost(values_);
  CHECK(MathUtil::AlmostEquals(cost_, cst));
  return true;
}

Cost SetCoverAssignment::ComputeCost(const SubsetBoolVector& choices) const {
  Cost cst = 0.0;
  SubsetIndex subset(0);
  for (const bool b : choices) {
    if (b) {
      cst += model_.subset_costs()[subset];
    }
    ++subset;
  }
  return cst;
}

}  // namespace operations_research
