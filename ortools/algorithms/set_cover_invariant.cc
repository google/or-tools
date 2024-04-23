// Copyright 2010-2024 Google LLC
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
#include "ortools/algorithms/set_cover_model.h"
#include "ortools/base/logging.h"

namespace operations_research {
// Note: in many of the member functions, variables have "crypterse" names
// to avoid confusing them with member data. For example mrgnl_impcts is used
// to avoid confusion with marginal_impacts_.
void SetCoverInvariant::Initialize() {
  DCHECK(model_->ComputeFeasibility());
  model_->CreateSparseRowView();

  const SubsetIndex num_subsets(model_->num_subsets());
  is_selected_.assign(num_subsets, false);
  is_removable_.assign(num_subsets, false);
  marginal_impacts_.assign(num_subsets, ElementIndex(0));
  num_overcovered_elements_.assign(num_subsets, ElementIndex(0));

  const SparseColumnView& columns = model_->columns();
  for (SubsetIndex subset(0); subset < num_subsets; ++subset) {
    marginal_impacts_[subset] = columns[subset].size().value();
  }

  const ElementIndex num_elements(model_->num_elements());
  coverage_.assign(num_elements, SubsetIndex(0));
  cost_ = 0.0;
  num_elements_covered_ = ElementIndex(0);
}

bool SetCoverInvariant::CheckConsistency() const {
  auto [cst, cvrg] = ComputeCostAndCoverage(is_selected_);
  CHECK_EQ(cost_, cst);
  for (ElementIndex element(0); element < model_->num_elements(); ++element) {
    CHECK_EQ(cvrg[element], coverage_[element]);
  }
  auto [num_elts_cvrd, mrgnl_impcts, num_ovrcvrd_elts, is_rmvble] =
      ComputeImpliedData(cvrg);
  CHECK_EQ(num_elts_cvrd, num_elements_covered_);
  const SparseColumnView& columns = model_->columns();
  for (SubsetIndex subset(0); subset < columns.size(); ++subset) {
    CHECK_EQ(mrgnl_impcts[subset], marginal_impacts_[subset]);
    CHECK_EQ(num_ovrcvrd_elts[subset], num_overcovered_elements_[subset]);
    CHECK_EQ(is_rmvble[subset], is_removable_[subset]);
    CHECK_EQ(is_rmvble[subset],
             num_ovrcvrd_elts[subset] == columns[subset].size().value());
  }
  return true;
}

void SetCoverInvariant::LoadSolution(const SubsetBoolVector& c) {
  is_selected_ = c;
  RecomputeInvariant();
}

void SetCoverInvariant::RecomputeInvariant() {
  std::tie(cost_, coverage_) = ComputeCostAndCoverage(is_selected_);
  std::tie(num_elements_covered_, marginal_impacts_, num_overcovered_elements_,
           is_removable_) = ComputeImpliedData(coverage_);
  }

std::tuple<Cost, ElementToSubsetVector>
SetCoverInvariant::ComputeCostAndCoverage(
    const SubsetBoolVector& choices) const {
  Cost cst = 0.0;
  ElementToSubsetVector cvrg(model_->num_elements(), SubsetIndex(0));
  const SparseColumnView& columns = model_->columns();
  // Initialize marginal_impacts_, update cost_, and compute the coverage for
  // all the elements covered by the selected subsets.
  const SubsetCostVector& subset_costs = model_->subset_costs();
  for (SubsetIndex subset(0); bool b : choices) {
    if (b) {
      cst += subset_costs[subset];
      for (ElementIndex element : columns[subset]) {
        ++cvrg[element];
      }
    }
    ++subset;
  }
  return {cst, cvrg};
}

std::tuple<ElementIndex, SubsetToElementVector, SubsetToElementVector,
           SubsetBoolVector>
SetCoverInvariant::ComputeImpliedData(const ElementToSubsetVector& cvrg) const {
  ElementIndex num_elts_cvrd(0);
  SubsetIndex num_subsets = model_->num_subsets();
  SubsetToElementVector mrgnl_impcts(num_subsets, ElementIndex(0));
  SubsetToElementVector num_ovrcvrd_elts(num_subsets, ElementIndex(0));
  SubsetBoolVector is_rmvble(num_subsets, false);

  const SparseColumnView& columns = model_->columns();

  // Initialize marginal_impacts_.
  for (SubsetIndex subset(0); subset < num_subsets; ++subset) {
    mrgnl_impcts[subset] = columns[subset].size().value();
}

  // Update num_ovrcvrd_elts[subset] and num_elts_cvrd.
  // When all elements in subset are overcovered, is_removable_[subset] is true.
  const SparseRowView& rows = model_->rows();

  const ElementIndex num_elements(model_->num_elements());
  for (ElementIndex element(0); element < num_elements; ++element) {
    if (cvrg[element] == 1) {
      ++num_elts_cvrd;  // Just need to update this ...
    for (SubsetIndex subset : rows[element]) {
        --mrgnl_impcts[subset];  // ... and marginal_impacts_.
      }
    } else {
      if (cvrg[element] >= 2) {
        ++num_elts_cvrd;  // Same as when cvrg[element] == 1
        for (SubsetIndex subset : rows[element]) {
          --mrgnl_impcts[subset];  // Same as when cvrg[element] >= 2
          ++num_ovrcvrd_elts[subset];
          if (num_ovrcvrd_elts[subset] == columns[subset].size().value()) {
            is_rmvble[subset] = true;
    }
  }
}
  }
}
  return {num_elts_cvrd, mrgnl_impcts, num_ovrcvrd_elts, is_rmvble};
}

std::vector<SubsetIndex> SetCoverInvariant::Toggle(SubsetIndex subset,
                                                   bool value) {
  // Note: "if p then q" is also "not(p) or q", or p <= q (p LE q).
  // If selected, then is_removable, to make sure we still have a solution.
  DCHECK(is_selected_[subset] <= is_removable_[subset])
      << "is_selected_ = " << is_selected_[subset]
      << " is_removable_ = " << is_removable_[subset] << " subset = " << subset;
  // If value, then marginal_impact > 0, to not increase the cost.
  DCHECK((value <= (marginal_impacts_[subset] > 0)));
  return UnsafeToggle(subset, value);
}

std::vector<SubsetIndex> SetCoverInvariant::UnsafeToggle(SubsetIndex subset,
                                                         bool value) {
  DCHECK_NE(is_selected_[subset], value);
  // If already selected, then marginal_impact == 0.
  DCHECK(is_selected_[subset] <= (marginal_impacts_[subset] == 0));
  DVLOG(1) << (value ? "S" : "Des") << "electing subset " << subset;
  is_selected_[subset] = value;
  const SubsetCostVector& subset_costs = model_->subset_costs();
  cost_ += value ? subset_costs[subset] : -subset_costs[subset];
  const int delta = value ? 1 : -1;

  const SparseColumnView& columns = model_->columns();

  SubsetBoolVector subset_seen(columns.size(), false);
  std::vector<SubsetIndex> impacted_subsets;
  impacted_subsets.reserve(columns.size().value());

  // Initialize subset_seen so that `subset` not be in impacted_subsets.
        subset_seen[subset] = true;

  auto collect_impacted_subsets = [&impacted_subsets,
                                   &subset_seen](SubsetIndex s) {
    if (!subset_seen[s]) {
      subset_seen[s] = true;
      impacted_subsets.push_back(s);
      }
  };

  for (const ElementIndex element : columns[subset]) {
    // Update coverage.
    coverage_[element] += delta;

    // There are four different types of events that matter when updating
    // the invariant. Two for each case when value is true or false.
  const SparseRowView& rows = model_->rows();
    if (value) {
      if (coverage_[element] == 1) {
        // `element` is newly covered.
        ++num_elements_covered_;
        for (const SubsetIndex impacted_subset : rows[element]) {
          collect_impacted_subsets(impacted_subset);
          --marginal_impacts_[impacted_subset];
      }
      } else if (coverage_[element] == 2) {
        // `element` is newly overcovered.
        for (const SubsetIndex impacted_subset : rows[element]) {
          collect_impacted_subsets(impacted_subset);
          ++num_overcovered_elements_[impacted_subset];
          if (num_overcovered_elements_[impacted_subset] ==
              columns[impacted_subset].size().value()) {
            // All the elements in impacted_subset are now overcovered, so it
            // is removable. Note that this happens only when the last element
            // of impacted_subset becomes overcovered.
            is_removable_[impacted_subset] = true;
    }
  }
  }
    } else {  // value is false
      if (coverage_[element] == 0) {
        // `element` is no longer covered.
        --num_elements_covered_;
        for (const SubsetIndex impacted_subset : rows[element]) {
          collect_impacted_subsets(impacted_subset);
          ++marginal_impacts_[impacted_subset];
      }
      } else if (coverage_[element] == 1) {
        // `element` is no longer overcovered.
        for (const SubsetIndex impacted_subset : rows[element]) {
          collect_impacted_subsets(impacted_subset);
          --num_overcovered_elements_[impacted_subset];
          if (num_overcovered_elements_[impacted_subset] ==
              columns[impacted_subset].size().value() - 1) {
            // There is one element of impacted_subset which is not overcovered.
            // impacted_subset has just become non-removable.
            is_removable_[impacted_subset] = false;
    }
  }
}
    }
  }
  DCHECK(CheckConsistency());
  return impacted_subsets;
}

SetCoverSolutionResponse SetCoverInvariant::ExportSolutionAsProto() const {
  SetCoverSolutionResponse message;
  message.set_num_subsets(is_selected_.size().value());
  Cost lower_bound = std::numeric_limits<Cost>::max();
  for (SubsetIndex subset(0); subset < model_->num_subsets(); ++subset) {
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
  RecomputeInvariant();
  Cost cost = message.cost();
  CHECK_EQ(cost, cost_);
}

}  // namespace operations_research
