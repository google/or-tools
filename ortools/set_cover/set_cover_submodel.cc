// Copyright 2025 Francesco Cavaliere
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

#include <limits>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "ortools/base/stl_util.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_model.h"
#include "ortools/set_cover/set_cover_views.h"

namespace operations_research {

static constexpr SubsetIndex kNullSubsetIndex =
    std::numeric_limits<SubsetIndex>::max();
static constexpr ElementIndex kNullElementIndex =
    std::numeric_limits<ElementIndex>::max();
static constexpr FullSubsetIndex kNullFullSubsetIndex =
    std::numeric_limits<FullSubsetIndex>::max();
static constexpr FullElementIndex kNullFullElementIndex =
    std::numeric_limits<FullElementIndex>::max();

SubModelView::SubModelView(const SetCoverModel* model)
    : base_view(model, &cols_sizes_, &rows_sizes_, &cols_focus_, &rows_focus_),
      full_model_(model) {
  ResetToIdentitySubModel();
  DCHECK(ValidateSubModel(*this));
}

SubModelView::SubModelView(const SetCoverModel* model,
                           const std::vector<FullSubsetIndex>& columns_focus)
    : base_view(model, &cols_sizes_, &rows_sizes_, &cols_focus_, &rows_focus_),
      full_model_(model) {
  rows_sizes_.resize(full_model_->num_elements(), 0);
  for (ElementIndex i : full_model_->ElementRange()) {
    rows_sizes_[i] = full_model_->rows()[i].size();
  }
  SetFocus(columns_focus);
}

void SubModelView::ResetToIdentitySubModel() {
  cols_sizes_.resize(full_model_->num_subsets());
  rows_sizes_.resize(full_model_->num_elements());
  cols_focus_.clear();
  rows_focus_.clear();
  for (SubsetIndex j : full_model_->SubsetRange()) {
    cols_sizes_[j] = full_model_->columns()[j].size();
    cols_focus_.push_back(j);
  }
  for (ElementIndex i : full_model_->ElementRange()) {
    rows_sizes_[i] = full_model_->rows()[i].size();
    rows_focus_.push_back(i);
  }
  fixed_columns_.clear();
  fixed_cost_ = .0;
}

Cost SubModelView::FixMoreColumns(
    const std::vector<SubsetIndex>& columns_to_fix) {
  DCHECK(full_model_ != nullptr);
  Cost old_fixed_cost = fixed_cost_;
  if (columns_to_fix.empty()) {
    return fixed_cost_ - old_fixed_cost;
  }

  for (SubsetIndex j : columns_to_fix) {
    DCHECK_GT(cols_sizes_[j], 0);
    fixed_cost_ += full_model_->subset_costs()[j];
    fixed_columns_.push_back(static_cast<FullSubsetIndex>(j));
    cols_sizes_[j] = 0;
    for (ElementIndex i : full_model_->columns()[j]) {
      rows_sizes_[i] = 0;
    }
  }

  gtl::STLEraseAllFromSequenceIf(&cols_focus_, [&](SubsetIndex j) {
    DCHECK(full_model_ != nullptr);
    if (cols_sizes_[j] > 0) {
      cols_sizes_[j] = absl::c_count_if(full_model_->columns()[j], [&](auto i) {
        return rows_sizes_[i] > 0;
      });
    }
    return cols_sizes_[j] == 0;
  });
  gtl::STLEraseAllFromSequenceIf(
      &rows_focus_, [&](ElementIndex i) { return !rows_sizes_[i]; });

  DCHECK(ValidateSubModel(*this));
  return fixed_cost_ - old_fixed_cost;
}

void SubModelView::ResetColumnFixing(
    const std::vector<FullSubsetIndex>& columns_to_fix, const DualState&) {
  ResetToIdentitySubModel();
  std::vector<SubsetIndex> core_column_to_fix;
  for (FullSubsetIndex full_j : columns_to_fix) {
    core_column_to_fix.push_back(static_cast<SubsetIndex>(full_j));
  }
  FixMoreColumns(core_column_to_fix);
}

void SubModelView::SetFocus(const std::vector<FullSubsetIndex>& columns_focus) {
  DCHECK(full_model_ != nullptr);
  DCHECK(!rows_sizes_.empty());
  if (columns_focus.empty()) {
    return;
  }
  cols_focus_.clear();
  rows_focus_.clear();

  ElementBoolVector enabled_rows(full_model_->num_elements(), false);
  for (ElementIndex i : full_model_->ElementRange()) {
    enabled_rows[i] = rows_sizes_[i] > 0;
  }
  cols_sizes_.assign(full_model_->num_subsets(), 0);
  rows_sizes_.assign(full_model_->num_elements(), 0);
  for (FullSubsetIndex full_j : columns_focus) {
    SubsetIndex j = static_cast<SubsetIndex>(full_j);
    for (ElementIndex i : full_model_->columns()[j]) {
      if (enabled_rows[i] > 0) {
        ++cols_sizes_[j];
        ++rows_sizes_[i];
      }
    }
    if (cols_sizes_[j] > 0) {
      cols_focus_.push_back(j);
    }
  }
  for (ElementIndex i : full_model_->ElementRange()) {
    if (rows_sizes_[i] > 0) {
      rows_focus_.push_back(i);
    }
  }
  DCHECK(ValidateSubModel(*this));
}

CoreModel::CoreModel(const SetCoverModel* model)
    : SetCoverModel(), full_model_(model) {
  CHECK(ElementIndex(full_model_.num_elements()) < kNullElementIndex)
      << "Max element index is reserved.";
  CHECK(SubsetIndex(full_model_.num_subsets()) < kNullSubsetIndex)
      << "Max subset index is reserved.";
  ResetToIdentitySubModel();
}

CoreModel::CoreModel(const SetCoverModel* model,
                     const std::vector<FullSubsetIndex>& columns_focus)
    : SetCoverModel(),
      full_model_(model),
      full2core_row_map_(model->num_elements()),
      core2full_row_map_(model->num_elements()) {
  CHECK(ElementIndex(full_model_.num_elements()) < kNullElementIndex)
      << "Max element index is reserved.";
  CHECK(SubsetIndex(full_model_.num_subsets()) < kNullSubsetIndex)
      << "Max subset index is reserved.";

  absl::c_iota(core2full_row_map_, FullElementIndex());
  absl::c_iota(full2core_row_map_, ElementIndex());
  SetFocus(columns_focus);
}

void CoreModel::ResetToIdentitySubModel() {
  core2full_row_map_.resize(full_model_.num_elements());
  full2core_row_map_.resize(full_model_.num_elements());
  core2full_col_map_.resize(full_model_.num_subsets());
  absl::c_iota(core2full_row_map_, FullElementIndex());
  absl::c_iota(full2core_row_map_, ElementIndex());
  absl::c_iota(core2full_col_map_, FullSubsetIndex());
  fixed_cost_ = .0;
  fixed_columns_.clear();
  static_cast<SetCoverModel&>(*this) = SetCoverModel(full_model_.base());
}

// Note: Assumes that columns_focus covers all rows for which rows_flags is true
// (i.e.: non-covered rows should be set to false to rows_flags). This property
// is exploited to keep the rows in the same ordering as the original model
// using "clean" code.
void CoreModel::SetFocus(const std::vector<FullSubsetIndex>& columns_focus) {
  if (columns_focus.empty()) {
    return;
  }

  // TODO(user): change model in-place to avoid reallocations.
  SetCoverModel& submodel = static_cast<SetCoverModel&>(*this);
  submodel = SetCoverModel();
  core2full_col_map_.clear();

  // Now we can fill the new core model
  for (FullSubsetIndex full_j : columns_focus) {
    bool first_row = true;
    for (FullElementIndex full_i : full_model_.columns()[full_j]) {
      ElementIndex core_i = full2core_row_map_[full_i];
      if (core_i != kNullElementIndex) {
        if (first_row) {
          // SetCoverModel lacks a way to remove columns
          first_row = false;
          submodel.AddEmptySubset(full_model_.subset_costs()[full_j]);
        }
        submodel.AddElementToLastSubset(core_i);
      }
    }
    // Handle empty columns
    if (!first_row) {
      core2full_col_map_.push_back(full_j);
    }
  }

  submodel.CreateSparseRowView();
  DCHECK(ValidateSubModel(*this));
}

// Mark columns and rows that will be removed from the core model
// The "to-be-removed" indices are marked by setting the relative core->full
// mappings to null_*_index.
void CoreModel::MarkNewFixingInMaps(
    const std::vector<SubsetIndex>& columns_to_fix) {
  for (SubsetIndex old_core_j : columns_to_fix) {
    fixed_cost_ += subset_costs()[old_core_j];
    fixed_columns_.push_back(core2full_col_map_[old_core_j]);

    core2full_col_map_[old_core_j] = kNullFullSubsetIndex;
    for (ElementIndex old_core_i : columns()[old_core_j]) {
      core2full_row_map_[old_core_i] = kNullFullElementIndex;
    }
  }
}

// Once fixed columns and covered rows are marked, we need to create a new row
// mapping, both core->full(returned) and full->core(modified in-place).
CoreToFullElementMapVector CoreModel::MakeOrFillBothRowMaps() {
  full2core_row_map_.assign(full_model_.num_elements(), kNullElementIndex);
  CoreToFullElementMapVector new_c2f_row_map;  // Future core->full mapping.
  for (ElementIndex old_core_i : ElementRange()) {
    FullElementIndex full_i = core2full_row_map_[old_core_i];
    if (full_i != kNullFullElementIndex) {
      full2core_row_map_[full_i] = ElementIndex(new_c2f_row_map.size());
      new_c2f_row_map.push_back(full_i);
    }
  }
  return new_c2f_row_map;
}

// Create a new core model by applying the remapping from the old core model to
// the new one considering the given column fixing.
// Both the old and new core->full row mappings are required to keep track of
// what changed, the old mapping gets overwritten with the new one at the end.
// Empty columns are detected and removed - or better - not added.
SetCoverModel CoreModel::MakeNewCoreModel(
    const CoreToFullElementMapVector& new_c2f_row_map) {
  SetCoverModel new_submodel;
  BaseInt new_core_j = 0;
  // Loop over old core column indices.
  for (SubsetIndex old_core_j : SubsetRange()) {
    // If the column is not marked, then it should be mapped.
    FullSubsetIndex full_j = core2full_col_map_[old_core_j];
    if (full_j != kNullFullSubsetIndex) {
      bool first_row = true;
      // Loop over the old core column (with old core row indices).
      for (ElementIndex old_core_i : columns()[old_core_j]) {
        // If the row is not marked, then it should be mapped.
        FullElementIndex full_i = core2full_row_map_[old_core_i];
        if (full_i != kNullFullElementIndex) {
          if (first_row) {
            // SetCoverModel lacks a way to remove columns
            first_row = false;
            new_submodel.AddEmptySubset(full_model_.subset_costs()[full_j]);

            // Put the full index in the proper (new) position.
            // Note that old_core_j >= new_core_j is always true.
            SubsetIndex new_j(new_core_j++);
            core2full_col_map_[new_j] = full_j;
          }
          ElementIndex new_core_i = full2core_row_map_[full_i];
          DCHECK(new_core_i != kNullElementIndex);
          new_submodel.AddElementToLastSubset(new_core_i);
        }
      }
    }
  }

  core2full_col_map_.resize(new_core_j);
  core2full_row_map_ = std::move(new_c2f_row_map);
  new_submodel.CreateSparseRowView();

  return new_submodel;
}

Cost CoreModel::FixMoreColumns(const std::vector<SubsetIndex>& columns_to_fix) {
  if (columns_to_fix.empty()) {
    return .0;
  }
  Cost old_fixed_cost = fixed_cost_;

  // Mark columns to be fixed and rows that will be covered by them
  MarkNewFixingInMaps(columns_to_fix);

  // Compute new core->full(returned) and full->core(modified original) row maps
  CoreToFullElementMapVector new_c2f_row_map = MakeOrFillBothRowMaps();

  // Create new model object applying the computed mappings
  static_cast<SetCoverModel&>(*this) = MakeNewCoreModel(new_c2f_row_map);

  DCHECK(ValidateSubModel(*this));
  DCHECK(absl::c_is_sorted(core2full_col_map_));
  DCHECK(absl::c_is_sorted(core2full_row_map_));

  return fixed_cost_ - old_fixed_cost;
}

void CoreModel::ResetColumnFixing(
    const std::vector<FullSubsetIndex>& columns_to_fix, const DualState&) {
  ResetToIdentitySubModel();
  std::vector<SubsetIndex> core_column_to_fix;
  for (FullSubsetIndex full_j : columns_to_fix) {
    core_column_to_fix.push_back(static_cast<SubsetIndex>(full_j));
  }
  FixMoreColumns(core_column_to_fix);
}

}  // namespace operations_research
