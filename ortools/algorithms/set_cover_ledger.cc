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

#include "ortools/algorithms/set_cover_ledger.h"

#include <algorithm>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "ortools/algorithms/set_cover_model.h"
#include "ortools/base/logging.h"

namespace operations_research {
// Note: in many of the member functions, variables have "crypterse" names
// to avoid confusing them with member data. For example mrgnl_impcts is used
// to avoid confusion with marginal_impacts_.
void SetCoverLedger::Initialize() {
  DCHECK(model_->ComputeFeasibility());
  model_->CreateSparseRowView();

  const SubsetIndex num_subsets(model_->num_subsets());
  is_selected_.assign(num_subsets, false);
  is_removable_.assign(num_subsets, false);
  marginal_impacts_.assign(num_subsets, ElementIndex(0));

  const SparseColumnView& columns = model_->columns();
  for (SubsetIndex subset(0); subset < num_subsets; ++subset) {
    marginal_impacts_[subset] = columns[subset].size().value();
  }
  const ElementIndex num_elements(model_->num_elements());
  coverage_.assign(num_elements, SubsetIndex(0));
  cost_ = 0.0;
  num_elements_covered_ = ElementIndex(0);
}

bool SetCoverLedger::CheckConsistency() const {
  CHECK(CheckCoverageAndMarginalImpacts(is_selected_));
  CHECK(CheckIsRemovable());
  return true;
}

void SetCoverLedger::LoadSolution(const SubsetBoolVector& c) {
  is_selected_ = c;
  MakeDataConsistent();
}

bool SetCoverLedger::CheckSolution() const {
  bool is_ok = true;

  const ElementToSubsetVector cvrg = ComputeCoverage(is_selected_);
  const ElementIndex num_elements(model_->num_elements());
  for (ElementIndex element(0); element < num_elements; ++element) {
    if (cvrg[element] == 0) {
      LOG(ERROR) << "Recomputed coverage_ for element " << element << " = 0";
      is_ok = false;
    }
  }

  const Cost recomputed_cost = ComputeCost(is_selected_);
  if (cost_ != recomputed_cost) {
    LOG(ERROR) << "Cost = " << cost_
               << ", while recomputed cost_ = " << recomputed_cost;
    is_ok = false;
  }
  return is_ok;
}

bool SetCoverLedger::CheckCoverageAgainstSolution(
    const SubsetBoolVector& choices) const {
  const SubsetIndex num_subsets(model_->num_subsets());
  DCHECK_EQ(num_subsets, choices.size());
  const ElementToSubsetVector cvrg = ComputeCoverage(choices);
  bool is_ok = true;
  const ElementIndex num_elements(model_->num_elements());
  for (ElementIndex element(0); element < num_elements; ++element) {
    if (coverage_[element] != cvrg[element]) {
      LOG(ERROR) << "Recomputed coverage_ for element " << element << " = "
                 << cvrg[element]
                 << ", while updated coverage_ = " << coverage_[element];
      is_ok = false;
    }
  }
  return is_ok;
}

bool SetCoverLedger::CheckCoverageAndMarginalImpacts(
    const SubsetBoolVector& choices) const {
  const SubsetIndex num_subsets(model_->num_subsets());
  DCHECK_EQ(num_subsets, choices.size());
  const ElementToSubsetVector cvrg = ComputeCoverage(choices);
  bool is_ok = CheckCoverageAgainstSolution(choices);
  const SubsetToElementVector mrgnl_impcts = ComputeMarginalImpacts(cvrg);
  for (SubsetIndex subset(0); subset < num_subsets; ++subset) {
    if (marginal_impacts_[subset] != mrgnl_impcts[subset]) {
      LOG(ERROR) << "Recomputed marginal impact for subset " << subset << " = "
                 << mrgnl_impcts[subset] << ", while updated marginal impact = "
                 << marginal_impacts_[subset];
      is_ok = false;
    }
  }
  return is_ok;
}

// Used only once, for testing. TODO(user): Merge with
// CheckCoverageAndMarginalImpacts.
SubsetToElementVector SetCoverLedger::ComputeMarginalImpacts(
    const ElementToSubsetVector& cvrg) const {
  const ElementIndex num_elements(model_->num_elements());
  DCHECK_EQ(num_elements, cvrg.size());
  const SparseColumnView& columns = model_->columns();
  const SubsetIndex num_subsets(model_->num_subsets());
  SubsetToElementVector mrgnl_impcts(num_subsets, ElementIndex(0));
  for (SubsetIndex subset(0); subset < num_subsets; ++subset) {
    for (ElementIndex element : columns[subset]) {
      if (cvrg[element] == 0) {
        ++mrgnl_impcts[subset];
      }
    }
    DCHECK_LE(mrgnl_impcts[subset], columns[subset].size().value());
    DCHECK_GE(mrgnl_impcts[subset], 0);
  }
  return mrgnl_impcts;
}

Cost SetCoverLedger::ComputeCost(const SubsetBoolVector& c) const {
  Cost recomputed_cost = 0;
  const SubsetCostVector& subset_costs = model_->subset_costs();
  const SubsetIndex num_subsets(model_->num_subsets());
  for (SubsetIndex subset(0); subset < num_subsets; ++subset) {
    if (c[subset]) {
      recomputed_cost += subset_costs[subset];
    }
  }
  return recomputed_cost;
}

ElementIndex SetCoverLedger::ComputeNumElementsCovered(
    const ElementToSubsetVector& cvrg) const {
  // Use "crypterse" naming to avoid confusing with num_elements_.
  int num_elmnts_cvrd = 0;
  for (ElementIndex element(0); element < model_->num_elements(); ++element) {
    if (cvrg[element] >= 1) {
      ++num_elmnts_cvrd;
    }
  }
  return ElementIndex(num_elmnts_cvrd);
}

ElementToSubsetVector SetCoverLedger::ComputeCoverage(
    const SubsetBoolVector& choices) const {
  const ElementIndex num_elements(model_->num_elements());
  const SparseRowView& rows = model_->rows();
  // Use "crypterse" naming to avoid confusing with coverage_.
  ElementToSubsetVector cvrg(num_elements, SubsetIndex(0));
  for (ElementIndex element(0); element < num_elements; ++element) {
    for (SubsetIndex subset : rows[element]) {
      if (choices[subset]) {
        ++cvrg[element];
      }
    }
    DCHECK_LE(cvrg[element], rows[element].size().value());
    DCHECK_GE(cvrg[element], 0);
  }
  return cvrg;
}

bool SetCoverLedger::CheckSingleSubsetCoverage(SubsetIndex subset) const {
  ElementToSubsetVector cvrg = ComputeSingleSubsetCoverage(subset);
  const SparseColumnView& columns = model_->columns();
  for (const ElementIndex element : columns[subset]) {
    DCHECK_EQ(coverage_[element], cvrg[element]) << " Element = " << element;
  }
  return true;
}

// Used only once, for testing. TODO(user): Merge with
// CheckSingleSubsetCoverage.
ElementToSubsetVector SetCoverLedger::ComputeSingleSubsetCoverage(
    SubsetIndex subset) const {
  const SparseColumnView& columns = model_->columns();
  const ElementIndex num_elements(model_->num_elements());
  // Use "crypterse" naming to avoid confusing with cvrg.
  ElementToSubsetVector cvrg(num_elements, SubsetIndex(0));
  const SparseRowView& rows = model_->rows();
  for (const ElementIndex element : columns[subset]) {
    for (SubsetIndex subset : rows[element]) {
      if (is_selected_[subset]) {
        ++cvrg[element];
      }
    }
    DCHECK_LE(cvrg[element], rows[element].size().value());
    DCHECK_GE(cvrg[element], 0);
  }
  return cvrg;
}

std::vector<SubsetIndex> SetCoverLedger::Toggle(SubsetIndex subset,
                                                bool value) {
  // Note: "if p then q" is also "not(p) or q", or p <= q (p LE q).
  // If selected, then is_removable, to make sure we still have a solution.
  DCHECK(is_selected_[subset] <= is_removable_[subset]);
  // If value, then marginal_impact > 0, to not increase the cost.
  DCHECK((value <= (marginal_impacts_[subset] > 0)));
  return UnsafeToggle(subset, value);
}

std::vector<SubsetIndex> SetCoverLedger::UnsafeToggle(SubsetIndex subset,
                                                      bool value) {
  // We allow to deselect a non-removable subset, but at least the
  // Toggle should be a real change.
  DCHECK_NE(is_selected_[subset], value);
  // If selected, then marginal_impact == 0.
  DCHECK((is_selected_[subset] <= (marginal_impacts_[subset] == 0)));
  DVLOG(1) << (value ? "S" : "Des") << "electing subset " << subset;
  const SubsetCostVector& subset_costs = model_->subset_costs();
  cost_ += value ? subset_costs[subset] : -subset_costs[subset];
  is_selected_[subset] = value;
  UpdateCoverage(subset, value);
  const std::vector<SubsetIndex> impacted_subsets =
      ComputeImpactedSubsets(subset);
  UpdateIsRemovable(impacted_subsets);
  UpdateMarginalImpacts(impacted_subsets);
  DCHECK((is_selected_[subset] <= (marginal_impacts_[subset] == 0)));
  return impacted_subsets;
}

void SetCoverLedger::UpdateCoverage(SubsetIndex subset, bool value) {
  const SparseColumnView& columns = model_->columns();
  const SparseRowView& rows = model_->rows();
  const int delta = value ? 1 : -1;
  for (const ElementIndex element : columns[subset]) {
    DVLOG(2) << "Coverage of element " << element << " changed from "
             << coverage_[element] << " to " << coverage_[element] + delta;
    coverage_[element] += delta;
    DCHECK_GE(coverage_[element], 0);
    DCHECK_LE(coverage_[element], rows[element].size().value());
    if (coverage_[element] == 1) {
      ++num_elements_covered_;
    } else if (coverage_[element] == 0) {
      --num_elements_covered_;
    }
  }
  DCHECK(CheckSingleSubsetCoverage(subset));
}

// Compute the impact of the change in the assignment for each subset
// containing element. Store this in a flat_hash_set so as to buffer the
// change.
std::vector<SubsetIndex> SetCoverLedger::ComputeImpactedSubsets(
    SubsetIndex subset) const {
  const SparseColumnView& columns = model_->columns();
  const SparseRowView& rows = model_->rows();
  absl::flat_hash_set<SubsetIndex> impacted_subsets_collection;
  for (const ElementIndex element : columns[subset]) {
    for (const SubsetIndex subset : rows[element]) {
      impacted_subsets_collection.insert(subset);
    }
  }
  DCHECK(impacted_subsets_collection.find(subset) !=
         impacted_subsets_collection.end());
  std::vector<SubsetIndex> impacted_subsets(impacted_subsets_collection.begin(),
                                            impacted_subsets_collection.end());
  DCHECK_LE(impacted_subsets.size(), model_->num_subsets());
  std::sort(impacted_subsets.begin(), impacted_subsets.end());
  return impacted_subsets;
}

bool SetCoverLedger::ComputeIsRemovable(SubsetIndex subset) const {
  DCHECK(CheckSingleSubsetCoverage(subset));
  const SparseColumnView& columns = model_->columns();
  for (const ElementIndex element : columns[subset]) {
    if (coverage_[element] <= 1) {
      return false;
    }
  }
  return true;
}

void SetCoverLedger::UpdateIsRemovable(
    const std::vector<SubsetIndex>& impacted_subsets) {
  for (const SubsetIndex subset : impacted_subsets) {
    is_removable_[subset] = ComputeIsRemovable(subset);
  }
}

SubsetBoolVector SetCoverLedger::ComputeIsRemovable(
    const ElementToSubsetVector& cvrg) const {
  DCHECK(CheckCoverageAgainstSolution(is_selected_));
  const SubsetIndex num_subsets(model_->num_subsets());
  SubsetBoolVector is_rmvble(num_subsets, true);
  const SparseRowView& rows = model_->rows();
  for (ElementIndex element(0); element < rows.size(); ++element) {
    if (cvrg[element] <= 1) {
      for (const SubsetIndex subset : rows[element]) {
        is_rmvble[subset] = false;
      }
    }
  }
  for (SubsetIndex subset(0); subset < num_subsets; ++subset) {
    DCHECK_EQ(is_rmvble[subset], ComputeIsRemovable(subset));
  }
  return is_rmvble;
}

bool SetCoverLedger::CheckIsRemovable() const {
  const SubsetBoolVector is_rmvble = ComputeIsRemovable(coverage_);
  const SubsetIndex num_subsets(model_->num_subsets());
  for (SubsetIndex subset(0); subset < num_subsets; ++subset) {
    DCHECK_EQ(is_rmvble[subset], ComputeIsRemovable(subset));
  }
  return true;
}

void SetCoverLedger::UpdateMarginalImpacts(
    const std::vector<SubsetIndex>& impacted_subsets) {
  const SparseColumnView& columns = model_->columns();
  for (const SubsetIndex subset : impacted_subsets) {
    ElementIndex impact(0);
    for (const ElementIndex element : columns[subset]) {
      if (coverage_[element] == 0) {
        ++impact;
      }
    }
    DVLOG(2) << "Changing impact of subset " << subset << " from "
             << marginal_impacts_[subset] << " to " << impact;
    marginal_impacts_[subset] = impact;
    DCHECK_LE(marginal_impacts_[subset], columns[subset].size().value());
    DCHECK_GE(marginal_impacts_[subset], 0);
  }
  DCHECK(CheckCoverageAndMarginalImpacts(is_selected_));
}

}  // namespace operations_research
