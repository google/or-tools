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

#include "ortools/set_cover/set_cover_submodel.h"

#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "ortools/base/stl_util.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_model.h"
#include "ortools/set_cover/set_cover_views.h"

namespace operations_research {

SubmodelView::SubmodelView(const SetCoverModel& model)
    : base_view(model, column_sizes_, row_sizes_, columns_in_focus_,
                rows_in_focus_),
      full_model_(model) {
  DCHECK_GE(model.num_subsets(), 0);
  DCHECK_GE(model.num_elements(), 0);
  ResetToIdentitySubmodel();
  DCHECK(ValidateSubmodel(*this));
}

SubmodelView::SubmodelView(
    const SetCoverModel& model,
    const std::vector<FullModelSubsetIndex>& columns_in_focus)
    : base_view(model, column_sizes_, row_sizes_, columns_in_focus_,
                rows_in_focus_),
      full_model_(model) {
  DCHECK_GE(model.num_subsets(), 0);
  DCHECK_GE(model.num_elements(), 0);
  DCHECK(CheckIndices(columns_in_focus,
                      FullModelSubsetIndex(model.num_subsets())));

  row_sizes_.resize(full_model_.get().num_elements(), 0);
  for (const ElementIndex i : full_model_.get().ElementRange()) {
    row_sizes_[i] = full_model_.get().rows()[i].size();
  }
  SetFocus(columns_in_focus);
  DCHECK(ValidateSubmodel(*this));
}

void SubmodelView::ResetToIdentitySubmodel() {
  column_sizes_.resize(full_model_.get().num_subsets());
  row_sizes_.resize(full_model_.get().num_elements());
  columns_in_focus_.clear();
  rows_in_focus_.clear();
  columns_in_focus_.reserve(full_model_.get().num_subsets());
  for (const SubsetIndex j : full_model_.get().SubsetRange()) {
    column_sizes_[j] = full_model_.get().columns()[j].size();
    columns_in_focus_.push_back(j);
  }
  rows_in_focus_.reserve(full_model_.get().num_elements());
  for (const ElementIndex i : full_model_.get().ElementRange()) {
    row_sizes_[i] = full_model_.get().rows()[i].size();
    rows_in_focus_.push_back(i);
  }
  fixed_columns_.clear();
  fixed_cost_ = 0.0;

  DCHECK(ValidateSubmodel(*this));
  DCHECK_EQ(fixed_cost_, 0.0);
  DCHECK(fixed_columns_.empty());
}

Cost SubmodelView::FixMoreColumns(
    const std::vector<SubsetIndex>& columns_to_fix) {
  DCHECK(CheckIndices(columns_to_fix,
                      SubsetIndex(full_model_.get().num_subsets())));

  const Cost old_fixed_cost = fixed_cost_;
  if (columns_to_fix.empty()) {
    DCHECK_EQ(fixed_cost_ - old_fixed_cost, 0.0);
    return fixed_cost_ - old_fixed_cost;
  }

  fixed_columns_.reserve(fixed_columns_.size() + columns_to_fix.size());
  for (const SubsetIndex j : columns_to_fix) {
    DCHECK_GT(column_sizes_[j], 0);
    fixed_cost_ += full_model_.get().subset_costs()[j];
    fixed_columns_.push_back(static_cast<FullModelSubsetIndex>(j));
    column_sizes_[j] = 0;
    for (const ElementIndex i : full_model_.get().columns()[j]) {
      row_sizes_[i] = 0;
    }
  }

  gtl::STLEraseAllFromSequenceIf(&columns_in_focus_, [&](const SubsetIndex j) {
    if (column_sizes_[j] > 0) {
      column_sizes_[j] =
          absl::c_count_if(full_model_.get().columns()[j],
                           [&](const auto i) { return row_sizes_[i] > 0; });
    }
    return column_sizes_[j] == 0;
  });

  gtl::STLEraseAllFromSequenceIf(
      &rows_in_focus_, [&](const ElementIndex i) { return !row_sizes_[i]; });

  DCHECK(ValidateSubmodel(*this));
  const Cost result = fixed_cost_ - old_fixed_cost;
  DCHECK_GE(result, 0.0);
  return result;
}

void SubmodelView::ResetColumnFixing(
    const std::vector<FullModelSubsetIndex>& columns_to_fix, const DualState&) {
  DCHECK(CheckIndices(columns_to_fix,
                      FullModelSubsetIndex(full_model_.get().num_subsets())));

  ResetToIdentitySubmodel();
  std::vector<SubsetIndex> core_columns_to_fix;
  core_columns_to_fix.reserve(columns_to_fix.size());
  for (const FullModelSubsetIndex full_j : columns_to_fix) {
    core_columns_to_fix.push_back(static_cast<SubsetIndex>(full_j));
  }
  FixMoreColumns(core_columns_to_fix);

  DCHECK(ValidateSubmodel(*this));
}

void SubmodelView::SetFocus(
    const std::vector<FullModelSubsetIndex>& columns_in_focus) {
  DCHECK(!row_sizes_.empty());
  DCHECK(CheckIndices(columns_in_focus,
                      FullModelSubsetIndex(full_model_.get().num_subsets())));

  if (columns_in_focus.empty()) {
    return;
  }
  columns_in_focus_.clear();
  rows_in_focus_.clear();

  ElementBoolVector enabled_rows(full_model_.get().num_elements(), false);
  for (const ElementIndex i : full_model_.get().ElementRange()) {
    enabled_rows[i] = row_sizes_[i] > 0;
  }
  column_sizes_.assign(full_model_.get().num_subsets(), 0);
  row_sizes_.assign(full_model_.get().num_elements(), 0);
  columns_in_focus_.reserve(columns_in_focus.size());
  for (const FullModelSubsetIndex full_j : columns_in_focus) {
    const SubsetIndex j = static_cast<SubsetIndex>(full_j);
    for (const ElementIndex i : full_model_.get().columns()[j]) {
      if (enabled_rows[i] > 0) {
        ++column_sizes_[j];
        ++row_sizes_[i];
      }
    }
    if (column_sizes_[j] > 0) {
      columns_in_focus_.push_back(j);
    }
  }
  rows_in_focus_.reserve(full_model_.get().num_elements());
  for (const ElementIndex i : full_model_.get().ElementRange()) {
    if (row_sizes_[i] > 0) {
      rows_in_focus_.push_back(i);
    }
  }
  DCHECK(ValidateSubmodel(*this));
}

CoreModel::CoreModel(const SetCoverModel& model)
    : SetCoverModel(), full_model_(model) {
  DCHECK_GE(model.num_elements(), 0);
  DCHECK_GE(model.num_subsets(), 0);

  CHECK(ElementIndex(full_model_.num_elements()) < kNullElementIndex)
      << "Max element index is reserved.";
  CHECK(SubsetIndex(full_model_.num_subsets()) < kNullSubsetIndex)
      << "Max subset index is reserved.";
  ResetToIdentitySubmodel();

  DCHECK(ValidateSubmodel(*this));
}

CoreModel::CoreModel(const SetCoverModel& model,
                     const std::vector<FullModelSubsetIndex>& columns_in_focus)
    : SetCoverModel(),
      full_model_(model),
      full_to_core_row_map_(model.num_elements()),
      core_to_full_row_map_(model.num_elements()) {
  DCHECK_GE(model.num_elements(), 0);
  DCHECK_GE(model.num_subsets(), 0);
  DCHECK(CheckIndices(columns_in_focus,
                      FullModelSubsetIndex(model.num_subsets())));

  CHECK(ElementIndex(full_model_.num_elements()) < kNullElementIndex)
      << "Max element index is reserved.";
  CHECK(SubsetIndex(full_model_.num_subsets()) < kNullSubsetIndex)
      << "Max subset index is reserved.";

  absl::c_iota(core_to_full_row_map_, FullModelElementIndex());
  absl::c_iota(full_to_core_row_map_, ElementIndex());
  SetFocus(columns_in_focus);

  DCHECK(ValidateSubmodel(*this));
}

void CoreModel::ResetToIdentitySubmodel() {
  core_to_full_row_map_.resize(full_model_.num_elements());
  full_to_core_row_map_.resize(full_model_.num_elements());
  core_to_full_column_map_.resize(full_model_.num_subsets());
  // Note: Assumes that columns_in_focus covers all rows for which rows_flags is
  // true (i.e.: non-covered rows should be set to false to rows_flags). This
  // property is exploited to keep the rows in the same ordering as the original
  // model using "clean" code.
  absl::c_iota(core_to_full_row_map_, FullModelElementIndex());
  absl::c_iota(full_to_core_row_map_, ElementIndex());
  absl::c_iota(core_to_full_column_map_, FullModelSubsetIndex());
  fixed_cost_ = 0.0;
  fixed_columns_.clear();
  static_cast<SetCoverModel&>(*this) = SetCoverModel(full_model_.base());

  DCHECK(ValidateSubmodel(*this));
  DCHECK_EQ(fixed_cost_, 0.0);
  DCHECK(fixed_columns_.empty());
}

void CoreModel::SetFocus(
    const std::vector<FullModelSubsetIndex>& columns_in_focus) {
  DCHECK(CheckIndices(columns_in_focus,
                      FullModelSubsetIndex(full_model_.num_subsets())));

  if (columns_in_focus.empty()) {
    return;
  }

  // TODO(user): change model in-place to avoid reallocations.
  SetCoverModel& base_model = static_cast<SetCoverModel&>(*this);
  base_model = SetCoverModel();
  core_to_full_column_map_.clear();
  core_to_full_column_map_.reserve(columns_in_focus.size());

  // Now we can fill the new core model
  for (const FullModelSubsetIndex full_j : columns_in_focus) {
    bool first_row = true;
    for (const FullModelElementIndex full_i : full_model_.columns()[full_j]) {
      const ElementIndex core_i = full_to_core_row_map_[full_i];
      if (core_i != kNullElementIndex) {
        if (first_row) {
          // SetCoverModel lacks a way to remove columns
          first_row = false;
          base_model.AddEmptySubset(full_model_.subset_costs()[full_j]);
        }
        base_model.AddElementToLastSubset(core_i);
      }
    }
    // Handle empty columns
    if (!first_row) {
      core_to_full_column_map_.push_back(full_j);
    }
  }

  base_model.CreateSparseRowView();
  DCHECK(ValidateSubmodel(*this));
}

// Mark columns and rows that will be removed from the core model
// The "to-be-removed" indices are marked by setting the relative core->full
// mappings to null_*_index.
void CoreModel::UpdateMappingsForFixedColumns(
    const std::vector<SubsetIndex>& columns_to_fix) {
  DCHECK(CheckIndices(columns_to_fix, SubsetIndex(num_focus_subsets())));

  fixed_columns_.reserve(fixed_columns_.size() + columns_to_fix.size());
  for (const SubsetIndex old_core_j : columns_to_fix) {
    fixed_cost_ += subset_costs()[old_core_j];
    fixed_columns_.push_back(core_to_full_column_map_[old_core_j]);

    core_to_full_column_map_[old_core_j] = kNullFullModelSubsetIndex;
    for (const ElementIndex old_core_i : columns()[old_core_j]) {
      core_to_full_row_map_[old_core_i] = kNullFullModelElementIndex;
    }
  }

  for (const SubsetIndex old_core_j : columns_to_fix) {
    DCHECK_EQ(core_to_full_column_map_[old_core_j], kNullFullModelSubsetIndex);
  }
}

// Once fixed columns and covered rows are marked, we need to create a new row
// mapping, both core->full(returned) and full->core(modified in-place).
CoreToFullElementMapVector CoreModel::ComputeRowMappings() {
  full_to_core_row_map_.assign(full_model_.num_elements(), kNullElementIndex);
  CoreToFullElementMapVector new_core_to_full_row_map;
  new_core_to_full_row_map.reserve(num_elements());
  for (const ElementIndex old_core_i : ElementRange()) {
    const FullModelElementIndex full_i = core_to_full_row_map_[old_core_i];
    if (full_i != kNullFullModelElementIndex) {
      full_to_core_row_map_[full_i] =
          ElementIndex(new_core_to_full_row_map.size());
      new_core_to_full_row_map.push_back(full_i);
    }
  }

  DCHECK_EQ(full_to_core_row_map_.size(), full_model_.num_elements());
  return new_core_to_full_row_map;
}

// Create a new core model by applying the remapping from the old core model to
// the new one considering the given column fixing.
// Both the old and new core->full row mappings are required to keep track of
// what changed, the old mapping gets overwritten with the new one at the end.
// Empty columns are detected and removed - or better - not added.
SetCoverModel CoreModel::MakeNewCoreModel(
    const CoreToFullElementMapVector& new_core_to_full_row_map) {
  DCHECK(CheckIndices(new_core_to_full_row_map,
                      FullModelElementIndex(full_model_.num_elements())));

  SetCoverModel new_core_model;
  BaseInt new_core_j = 0;
  // Loop over old core column indices.
  for (const SubsetIndex old_core_j : SubsetRange()) {
    // If the column is not marked, then it should be mapped.
    const FullModelSubsetIndex full_j = core_to_full_column_map_[old_core_j];
    if (full_j != kNullFullModelSubsetIndex) {
      bool first_row = true;
      // Loop over the old core column (with old core row indices).
      for (const ElementIndex old_core_i : columns()[old_core_j]) {
        // If the row is not marked, then it should be mapped.
        const FullModelElementIndex full_i = core_to_full_row_map_[old_core_i];
        if (full_i != kNullFullModelElementIndex) {
          if (first_row) {
            // SetCoverModel lacks a way to remove columns
            first_row = false;
            new_core_model.AddEmptySubset(full_model_.subset_costs()[full_j]);

            // Put the full index in the proper (new) position.
            // Note that old_core_j >= new_core_j is always true.
            const SubsetIndex new_j(new_core_j++);
            core_to_full_column_map_[new_j] = full_j;
          }
          const ElementIndex new_core_i = full_to_core_row_map_[full_i];
          DCHECK(new_core_i != kNullElementIndex);
          new_core_model.AddElementToLastSubset(new_core_i);
        }
      }
    }
  }

  core_to_full_column_map_.resize(new_core_j);
  core_to_full_row_map_ = std::move(new_core_to_full_row_map);
  new_core_model.CreateSparseRowView();

  return new_core_model;
}

Cost CoreModel::FixMoreColumns(const std::vector<SubsetIndex>& columns_to_fix) {
  DCHECK(CheckIndices(columns_to_fix, SubsetIndex(num_focus_subsets())));

  if (columns_to_fix.empty()) {
    return 0.0;
  }
  const Cost old_fixed_cost = fixed_cost_;

  // Mark columns to be fixed and rows that will be covered by them
  UpdateMappingsForFixedColumns(columns_to_fix);

  // Compute new core->full(returned) and full->core(modified original) row maps
  const CoreToFullElementMapVector new_core_to_full_row_map =
      ComputeRowMappings();

  // Create new model object applying the computed mappings
  static_cast<SetCoverModel&>(*this) =
      MakeNewCoreModel(new_core_to_full_row_map);

  DCHECK(ValidateSubmodel(*this));
  DCHECK(absl::c_is_sorted(core_to_full_column_map_));
  DCHECK(absl::c_is_sorted(core_to_full_row_map_));

  const Cost result = fixed_cost_ - old_fixed_cost;
  DCHECK_GE(result, 0.0);
  return result;
}

void CoreModel::ResetColumnFixing(
    const std::vector<FullModelSubsetIndex>& columns_to_fix, const DualState&) {
  DCHECK(CheckIndices(columns_to_fix,
                      FullModelSubsetIndex(full_model_.num_subsets())));

  ResetToIdentitySubmodel();
  std::vector<SubsetIndex> core_columns_to_fix;
  core_columns_to_fix.reserve(columns_to_fix.size());
  for (const FullModelSubsetIndex full_j : columns_to_fix) {
    core_columns_to_fix.push_back(static_cast<SubsetIndex>(full_j));
  }
  FixMoreColumns(core_columns_to_fix);

  DCHECK(ValidateSubmodel(*this));
}

}  // namespace operations_research
