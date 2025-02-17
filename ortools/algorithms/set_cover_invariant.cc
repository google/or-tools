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

#include "ortools/algorithms/set_cover_invariant.h"

#include <algorithm>
#include <limits>
#include <tuple>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/algorithms/set_cover_model.h"
#include "ortools/base/logging.h"
#include "ortools/base/mathutil.h"

namespace operations_research {

using CL = SetCoverInvariant::ConsistencyLevel;

// Note: in many of the member functions, variables have "crypterse" names
// to avoid confusing them with member data. For example mrgnl_impcts is used
// to avoid confusion with num_free_elements_.
void SetCoverInvariant::Initialize() {
  DCHECK(model_->ComputeFeasibility());
  model_->CreateSparseRowView();
  Clear();
}

void SetCoverInvariant::Clear() {
  cost_ = 0.0;

  const BaseInt num_subsets = model_->num_subsets();
  const BaseInt num_elements = model_->num_elements();

  is_selected_.assign(num_subsets, false);
  num_free_elements_.assign(num_subsets, 0);
  num_non_overcovered_elements_.assign(num_subsets, 0);
  is_redundant_.assign(num_subsets, false);

  const SparseColumnView& columns = model_->columns();
  for (const SubsetIndex subset : model_->SubsetRange()) {
    num_free_elements_[subset] = columns[subset].size();
    num_non_overcovered_elements_[subset] = columns[subset].size();
  }

  coverage_.assign(num_elements, 0);

  // No need to reserve for trace_ and other vectors as extending with
  // push_back is fast enough.
  trace_.clear();
  newly_removable_subsets_.clear();
  newly_non_removable_subsets_.clear();

  num_uncovered_elements_ = num_elements;
  consistency_level_ = CL::kRedundancy;
}

bool SetCoverInvariant::CheckConsistency(ConsistencyLevel consistency) const {
  CHECK(consistency <= CL::kRedundancy);
  if (consistency == CL::kInconsistent) {
    return true;
  }
  auto [cst, cvrg] = ComputeCostAndCoverage(is_selected_);
  CHECK(MathUtil::AlmostEquals(cost_, cst));
  for (const ElementIndex element : model_->ElementRange()) {
    CHECK_EQ(cvrg[element], coverage_[element]);
  }
  if (consistency == CL::kCostAndCoverage) {
    return true;
  }
  auto [num_uncvrd_elts, num_free_elts] =
      ComputeNumUncoveredAndFreeElements(coverage_);
  for (const SubsetIndex subset : model_->SubsetRange()) {
    CHECK_EQ(num_free_elts[subset], num_free_elements_[subset]);
  }
  if (consistency == CL::kFreeAndUncovered) {
    return true;
  }
  auto [num_non_ovrcvrd_elts, is_rdndnt] = ComputeRedundancyInfo(coverage_);
  for (const SubsetIndex subset : model_->SubsetRange()) {
    CHECK_EQ(is_rdndnt[subset], is_redundant_[subset]);
    CHECK_EQ(is_rdndnt[subset], num_non_ovrcvrd_elts[subset] == 0);
  }
  if (consistency == CL::kRedundancy) {
    return true;
  }
  LOG(FATAL) << "Consistency level not supported: "
             << static_cast<int>(consistency);
  return false;
}

void SetCoverInvariant::LoadSolution(const SubsetBoolVector& solution) {
  is_selected_ = solution;
  ClearTrace();
  ClearRemovabilityInformation();
  SubsetIndex subset(0);
  for (const bool b : solution) {
    if (b) {
      trace_.push_back(SetCoverDecision(subset, true));
    }
    ++subset;
  }
  consistency_level_ = CL::kInconsistent;
  Recompute(CL::kCostAndCoverage);
}

bool SetCoverInvariant::NeedToRecompute(ConsistencyLevel cheched_consistency,
                                        ConsistencyLevel target_consistency) {
  return consistency_level_ < cheched_consistency &&
         cheched_consistency <= target_consistency;
}

void SetCoverInvariant::Recompute(ConsistencyLevel target_consistency) {
  CHECK(target_consistency >= CL::kCostAndCoverage);
  CHECK(target_consistency <= CL::kRedundancy);
  DCHECK(CheckConsistency(consistency_level_));
  if (NeedToRecompute(CL::kCostAndCoverage, target_consistency)) {
    std::tie(cost_, coverage_) = ComputeCostAndCoverage(is_selected_);
  }
  if (NeedToRecompute(CL::kFreeAndUncovered, target_consistency)) {
    std::tie(num_uncovered_elements_, num_free_elements_) =
        ComputeNumUncoveredAndFreeElements(coverage_);
  }
  if (NeedToRecompute(CL::kRedundancy, target_consistency)) {
    std::tie(num_non_overcovered_elements_, is_redundant_) =
        ComputeRedundancyInfo(coverage_);
  }
  consistency_level_ = target_consistency;
}

// NOTE(user): This piece of code is for reference because it seems to be
// faster to update the invariant. const BaseInt num_subsets =
// model_->num_subsets(); is_redundant_.assign(num_subsets, false);
// num_non_overcovered_elements_.assign(num_subsets, 0);
// const SparseColumnView& columns = model_->columns();
// for (const ElementIndex element : model_->ElementRange()) {
//   if (coverage_[element] >= 1) {
//     --num_uncovered_elements_;
//   }
// }
// for (const SubsetIndex subset : model_->SubsetRange()) {
//   for (const ElementIndex element : columns[subset]) {
//     if (coverage_[element] <= 1) {
//       ++num_non_overcovered_elements_[subset];
//     }
//     if (coverage_[element] >= 1) {
//       --num_free_elements_[subset];
//     }
//   }
//   is_redundant_[subset] = (num_non_overcovered_elements_[subset] == 0);
// }

std::tuple<Cost, ElementToIntVector> SetCoverInvariant::ComputeCostAndCoverage(
    const SubsetBoolVector& choices) const {
  Cost cst = 0.0;
  ElementToIntVector cvrg(model_->num_elements(), 0);
  const SparseColumnView& columns = model_->columns();
  // Initialize coverage, update cost, and compute the coverage for
  // all the elements covered by the selected subsets.
  const SubsetCostVector& subset_costs = model_->subset_costs();
  SubsetIndex subset(0);
  for (const bool b : choices) {
    if (b) {
      cst += subset_costs[subset];
      for (const ElementIndex element : columns[subset]) {
        ++cvrg[element];
      }
    }
    ++subset;
  }
  return {cst, cvrg};
}

ElementToIntVector SetCoverInvariant::ComputeCoverageInFocus(
    const absl::Span<const SubsetIndex> focus) const {
  ElementToIntVector coverage(coverage_.size());
  for (const SubsetIndex subset : focus) {
    if (is_selected_[subset]) {
      for (const ElementIndex element : model_->columns()[subset]) {
        ++coverage[element];
      }
    }
  }
  return coverage;
}

std::tuple<BaseInt, SubsetToIntVector>
SetCoverInvariant::ComputeNumUncoveredAndFreeElements(
    const ElementToIntVector& cvrg) const {
  BaseInt num_uncvrd_elts = model_->num_elements();

  const BaseInt num_subsets(model_->num_subsets());
  SubsetToIntVector num_free_elts(num_subsets, 0);

  const SparseColumnView& columns = model_->columns();
  // Initialize number of free elements and number of elements covered 0 or 1.
  for (const SubsetIndex subset : model_->SubsetRange()) {
    num_free_elts[subset] = columns[subset].size();
  }

  const SparseRowView& rows = model_->rows();
  for (const ElementIndex element : model_->ElementRange()) {
    if (cvrg[element] >= 1) {
      --num_uncvrd_elts;
      for (const SubsetIndex subset : rows[element]) {
        --num_free_elts[subset];
      }
    }
  }
  return {num_uncvrd_elts, num_free_elts};
}

std::tuple<SubsetToIntVector, SubsetBoolVector>
SetCoverInvariant::ComputeRedundancyInfo(const ElementToIntVector& cvrg) const {
  const BaseInt num_subsets(model_->num_subsets());
  SubsetToIntVector num_cvrg_le_1_elts(num_subsets, 0);
  SubsetBoolVector is_rdndnt(num_subsets, false);

  const SparseColumnView& columns = model_->columns();
  // Initialize number of free elements and number of elements covered 0 or 1.
  for (const SubsetIndex subset : model_->SubsetRange()) {
    num_cvrg_le_1_elts[subset] = columns[subset].size();
  }

  const SparseRowView& rows = model_->rows();
  for (const ElementIndex element : model_->ElementRange()) {
    if (cvrg[element] >= 2) {
      for (const SubsetIndex subset : rows[element]) {
        --num_cvrg_le_1_elts[subset];
        if (num_cvrg_le_1_elts[subset] == 0) {
          is_rdndnt[subset] = true;
        }
      }
    }
  }
  return {num_cvrg_le_1_elts, is_rdndnt};
}

void SetCoverInvariant::CompressTrace() {
  // As of 2024-05-14, this is as fast as "smarter" alternatives that try to
  // avoid some memory writes by keeping track of already visited subsets.
  // We also tried to use is_selected_ as a helper, which slowed down
  // everything.
  const SubsetIndex num_subsets(model_->num_subsets());
  SubsetBoolVector last_value_seen(num_subsets, false);
  for (BaseInt i = 0; i < trace_.size(); ++i) {
    const SubsetIndex subset(trace_[i].subset());
    last_value_seen[subset] = trace_[i].decision();
  }
  BaseInt w = 0;  // Write index.
  for (BaseInt i = 0; i < trace_.size(); ++i) {
    const SubsetIndex subset(trace_[i].subset());
    if (last_value_seen[subset]) {
      last_value_seen[subset] = false;
      trace_[w] = SetCoverDecision(subset, true);
      ++w;
    }
  }
  trace_.resize(w);
}

bool SetCoverInvariant::ComputeIsRedundant(SubsetIndex subset) const {
  if (consistency_level_ >= CL::kRedundancy) {
    return is_redundant_[subset];
  }
  if (is_selected_[subset]) {
    for (const ElementIndex element : model_->columns()[subset]) {
      if (coverage_[element] <= 1) {  // If deselected, it will be <= 0...
        return false;
      }
    }
  } else {
    for (const ElementIndex element : model_->columns()[subset]) {
      if (coverage_[element] == 0) {  // Cannot be removed from the problem.
        return false;
      }
    }
  }
  return true;
}

BaseInt SetCoverInvariant::ComputeNumFreeElements(SubsetIndex subset) const {
  BaseInt num_free_elements = model_->columns()[subset].size();
  for (const ElementIndex element : model_->columns()[subset]) {
    if (coverage_[element] != 0) {
      --num_free_elements;
    }
  }
  return num_free_elements;
}

void SetCoverInvariant::Flip(SubsetIndex subset,
                             ConsistencyLevel target_consistency) {
  if (!is_selected_[subset]) {
    Select(subset, target_consistency);
  } else {
    Deselect(subset, target_consistency);
  }
}

void SetCoverInvariant::Select(SubsetIndex subset,
                               ConsistencyLevel target_consistency) {
  const bool update_redundancy_info = target_consistency >= CL::kRedundancy;
  if (update_redundancy_info) {
    ClearRemovabilityInformation();
  }
  consistency_level_ = std::min(consistency_level_, target_consistency);
  DVLOG(1) << "Selecting subset " << subset;
  DCHECK(!is_selected_[subset]);
  DCHECK(CheckConsistency(target_consistency));
  trace_.push_back(SetCoverDecision(subset, true));
  is_selected_[subset] = true;
  const SubsetCostVector& subset_costs = model_->subset_costs();
  cost_ += subset_costs[subset];
  const SparseColumnView& columns = model_->columns();
  const SparseRowView& rows = model_->rows();
  // Fast path for kCostAndCoverage.
  if (target_consistency == CL::kCostAndCoverage) {
    for (const ElementIndex element : columns[subset]) {
      ++coverage_[element];
    }
    return;
  }
  for (const ElementIndex element : columns[subset]) {
    if (coverage_[element] == 0) {
      // `element` will be newly covered.
      --num_uncovered_elements_;
      for (const SubsetIndex impacted_subset : rows[element]) {
        --num_free_elements_[impacted_subset];
      }
    } else if (update_redundancy_info && coverage_[element] == 1) {
      // `element` will be newly overcovered.
      for (const SubsetIndex impacted_subset : rows[element]) {
        --num_non_overcovered_elements_[impacted_subset];
        if (num_non_overcovered_elements_[impacted_subset] == 0) {
          // All the elements in impacted_subset are now overcovered, so it
          // is removable. Note that this happens only when the last element
          // of impacted_subset becomes overcovered.
          DCHECK(!is_redundant_[impacted_subset]);
          if (is_selected_[impacted_subset]) {
            newly_removable_subsets_.push_back(impacted_subset);
          }
          is_redundant_[impacted_subset] = true;
        }
      }
    }
    // Update coverage. Notice the asymmetry with Deselect where coverage is
    // **decremented** before being tested. This allows to have more
    // symmetrical code for conditions.
    ++coverage_[element];
  }
  if (update_redundancy_info) {
    if (is_redundant_[subset]) {
      newly_removable_subsets_.push_back(subset);
    } else {
      newly_non_removable_subsets_.push_back(subset);
    }
  }
  DCHECK(CheckConsistency(target_consistency));
}

void SetCoverInvariant::Deselect(SubsetIndex subset,
                                 ConsistencyLevel target_consistency) {
  DCHECK(CheckConsistency(target_consistency));
  const bool update_redundancy_info = target_consistency >= CL::kRedundancy;
  if (update_redundancy_info) {
    ClearRemovabilityInformation();
  }
  consistency_level_ = std::min(consistency_level_, target_consistency);
  DVLOG(1) << "Deselecting subset " << subset;
  // If already selected, then num_free_elements == 0.
  DCHECK(is_selected_[subset]);
  DCHECK_EQ(num_free_elements_[subset], 0);
  trace_.push_back(SetCoverDecision(subset, false));
  is_selected_[subset] = false;
  const SubsetCostVector& subset_costs = model_->subset_costs();
  cost_ -= subset_costs[subset];
  const SparseColumnView& columns = model_->columns();
  const SparseRowView& rows = model_->rows();
  // Fast path for kCostAndCoverage.
  if (target_consistency == CL::kCostAndCoverage) {
    for (const ElementIndex element : columns[subset]) {
      --coverage_[element];
    }
    return;
  }
  for (const ElementIndex element : columns[subset]) {
    // Update coverage. Notice the asymmetry with Select where coverage is
    // incremented after being tested.
    --coverage_[element];
    if (coverage_[element] == 0) {
      // `element` is no longer covered.
      ++num_uncovered_elements_;
      for (const SubsetIndex impacted_subset : rows[element]) {
        ++num_free_elements_[impacted_subset];
      }
    } else if (update_redundancy_info && coverage_[element] == 1) {
      // `element` will be no longer overcovered.
      for (const SubsetIndex impacted_subset : rows[element]) {
        if (num_non_overcovered_elements_[impacted_subset] == 0) {
          // There is one element of impacted_subset which is not overcovered.
          // impacted_subset has just become non-removable.
          DCHECK(is_redundant_[impacted_subset]);
          if (is_selected_[impacted_subset]) {
            newly_non_removable_subsets_.push_back(impacted_subset);
          }
          is_redundant_[impacted_subset] = false;
        }
        ++num_non_overcovered_elements_[impacted_subset];
      }
    }
  }
  // Since subset is now deselected, there is no need
  // nor meaning in adding it a list of removable or non-removable subsets.
  // This is a dissymmetry with Select.
  DCHECK(CheckConsistency(target_consistency));
}

SetCoverSolutionResponse SetCoverInvariant::ExportSolutionAsProto() const {
  SetCoverSolutionResponse message;
  message.set_num_subsets(is_selected_.size());
  Cost lower_bound = std::numeric_limits<Cost>::max();
  for (const SubsetIndex subset : model_->SubsetRange()) {
    if (is_selected_[subset]) {
      message.add_subset(subset.value());
    }
    lower_bound = std::min(model_->subset_costs()[subset], lower_bound);
  }
  message.set_cost(cost_);
  message.set_cost_lower_bound(lower_bound);
  return message;
}

void SetCoverInvariant::ImportSolutionFromProto(
    const SetCoverSolutionResponse& message) {
  is_selected_.resize(SubsetIndex(message.num_subsets()), false);
  for (auto s : message.subset()) {
    is_selected_[SubsetIndex(s)] = true;
  }
  Cost cost = message.cost();
  CHECK(MathUtil::AlmostEquals(cost, cost_));
}
}  // namespace operations_research
