// Copyright 2025 Francesco Cavaliere
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

#ifndef OR_TOOLS_SET_COVER_SET_COVER_VIEWS_H
#define OR_TOOLS_SET_COVER_SET_COVER_VIEWS_H

#include <absl/meta/type_traits.h>

#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_model.h"
#include "views.h"

namespace operations_research::scp {

class IndexListSubModelView {
 public:
  IndexListSubModelView() = default;
  IndexListSubModelView(const SetCoverModel* model,
                        const SubsetToIntVector* cols_sizes,
                        const ElementToIntVector* rows_sizes,
                        const std::vector<SubsetIndex>* cols_focus,
                        const std::vector<ElementIndex>* rows_focus)
      : model_(model),
        cols_sizes_(cols_sizes),
        rows_sizes_(rows_sizes),
        cols_focus_(cols_focus),
        rows_focus_(rows_focus) {}

  const SetCoverModel& full_model() const { return *model_; }
  BaseInt num_subsets() const { return model_->num_subsets(); }
  BaseInt num_elements() const { return model_->num_elements(); }
  BaseInt num_focus_subsets() const { return cols_focus_->size(); }
  BaseInt num_focus_elements() const { return rows_focus_->size(); }

  auto subset_costs() const -> util_intops::IndexListView<Cost, SubsetIndex> {
    return {&model_->subset_costs(), cols_focus_};
  }
  auto columns() const -> util_intops::TwoLevelsView<
      util_intops::IndexListView<SparseColumn, SubsetIndex>,
      ElementToIntVector> {
    return {{&model_->columns(), cols_focus_}, rows_sizes_};
  }
  auto rows() const -> util_intops::TwoLevelsView<
      util_intops::IndexListView<SparseRow, ElementIndex>, SubsetToIntVector> {
    return {{&model_->rows(), rows_focus_}, cols_sizes_};
  }
  const std::vector<SubsetIndex>& SubsetRange() const { return *cols_focus_; }
  const std::vector<ElementIndex>& ElementRange() const { return *rows_focus_; }
  ElementIndex MapCoreToFullElementIndex(ElementIndex i) const {
    DCHECK(ElementIndex() <= i && i < ElementIndex(num_elements()));
    return i;
  }
  ElementIndex MapFullToCoreElementIndex(ElementIndex i) const {
    DCHECK(ElementIndex() <= i && i < ElementIndex(num_elements()));
    return i;
  }
  SubsetIndex MapCoreToFullSubsetIndex(SubsetIndex j) const {
    DCHECK(SubsetIndex() <= j && j < SubsetIndex(num_subsets()));
    return j;
  }
  BaseInt column_size(SubsetIndex j) const {
    DCHECK(SubsetIndex() <= j && j < SubsetIndex(num_subsets()));
    return (*cols_sizes_)[j];
  }
  BaseInt row_size(ElementIndex i) const {
    DCHECK(ElementIndex() <= i && i < ElementIndex(num_elements()));
    return (*rows_sizes_)[i];
  }

 private:
  const SetCoverModel* model_;
  const SubsetToIntVector* cols_sizes_;
  const ElementToIntVector* rows_sizes_;
  const std::vector<SubsetIndex>* cols_focus_;
  const std::vector<ElementIndex>* rows_focus_;
};

// A lightweight sub-model view that uses boolean vectors to enable or disable
// specific items. Iterating over all active columns or rows is less efficient,
// particularly when only a small subset is active.
// NOTE: this view does **not** store any size-related information.
class FilteredSubModelView {
 public:
  FilteredSubModelView() = default;
  FilteredSubModelView(const SetCoverModel* model,
                       const SubsetBoolVector* cols_sizes,
                       const ElementBoolVector* rows_sizes, BaseInt num_subsets,
                       BaseInt num_elements)
      : model_(model),
        is_focus_col_(cols_sizes),
        is_focus_row_(rows_sizes),
        num_subsets_(num_subsets),
        num_elements_(num_elements) {}

  const SetCoverModel& full_model() const { return *model_; }
  BaseInt num_subsets() const { return model_->num_subsets(); }
  BaseInt num_elements() const { return model_->num_elements(); }
  BaseInt num_focus_subsets() const { return num_subsets_; }
  BaseInt num_focus_elements() const { return num_elements_; }

  auto subset_costs() const
      -> util_intops::IndexFilterView<Cost, SubsetBoolVector> {
    return {&model_->subset_costs(), is_focus_col_};
  }
  auto columns() const -> util_intops::TwoLevelsView<
      util_intops::IndexFilterView<SparseColumn, SubsetBoolVector>,
      ElementBoolVector> {
    return {{&model_->columns(), is_focus_col_}, is_focus_row_};
  }
  auto rows() const -> util_intops::TwoLevelsView<
      util_intops::IndexFilterView<SparseRow, ElementBoolVector>,
      SubsetBoolVector> {
    return {{&model_->rows(), is_focus_row_}, is_focus_col_};
  }
  auto SubsetRange() const
      -> util_intops::FilterIndicesView<SubsetIndex, SubsetBoolVector> {
    return {is_focus_col_};
  }
  auto ElementRange() const
      -> util_intops::FilterIndicesView<ElementIndex, ElementBoolVector> {
    return {is_focus_row_};
  }
  ElementIndex MapCoreToFullElementIndex(ElementIndex core_i) const {
    DCHECK(ElementIndex() <= core_i && core_i < ElementIndex(num_elements()));
    return core_i;
  }
  ElementIndex MapFullToCoreElementIndex(ElementIndex full_i) const {
    DCHECK(ElementIndex() <= full_i && full_i < ElementIndex(num_elements()));
    return full_i;
  }
  SubsetIndex MapCoreToFullSubsetIndex(SubsetIndex core_j) const {
    DCHECK(SubsetIndex() <= core_j && core_j < SubsetIndex(num_subsets()));
    return core_j;
  }

 private:
  const SetCoverModel* model_;
  const SubsetBoolVector* is_focus_col_;
  const ElementBoolVector* is_focus_row_;
  BaseInt num_subsets_;
  BaseInt num_elements_;
};

}  // namespace operations_research::scp

#endif /* OR_TOOLS_SET_COVER_SET_COVER_VIEWS_H */
