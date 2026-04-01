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

#ifndef ORTOOLS_SET_COVER_SET_COVER_VIEWS_H_
#define ORTOOLS_SET_COVER_SET_COVER_VIEWS_H_

#include <vector>

#include "absl/log/check.h"
#include "ortools/base/strong_int.h"
#include "ortools/base/strong_vector.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_model.h"
#include "ortools/set_cover/views.h"

namespace operations_research {

// In the CFT algorithm, indices from different models are frequently used,
// and mixing them can lead to errors. To prevent such mistakes, strong-typed
// wrappers are employed. There are three available approaches for handling
// these indices:
// 1. Full-model strong-typed indices + {Subset,Element}Index for the core model
// 2. Core-model strong-typed indices + {Subset,Element}Index for the full model
// 3. Define new strong-typed indices for both full-model and core-model
// Introducing a new set of strong-typed indices, however, can lead to a
// cascade of code duplication (or template proliferation). It also requires
// additional "view" boilerplate to properly handle the different types,
// increasing complexity.
// Currently, the simplest approach is to define only full-model indices while
// reusing the original strong types for the core model. The main challenge
// arises in FullToCoreModel, where a "filtered" full-model must be handled. In
// such cases, static casts are employed to manage the type conversions
// effectively.
DEFINE_STRONG_INT_TYPE(FullSubsetIndex, BaseInt);
DEFINE_STRONG_INT_TYPE(FullElementIndex, BaseInt);

// Syntactic sugar to define strong-typed indices casts.
// Note: look at `strong_int.h` for more details about `StrongIntConvert`
#define ENABLE_EXPLICIT_STRONG_TYPE_CAST(FROM, TO)        \
  constexpr TO StrongIntConvert(FROM j, TO* /*unused*/) { \
    return TO(static_cast<FROM::ValueType>(j));           \
  }
ENABLE_EXPLICIT_STRONG_TYPE_CAST(SubsetIndex, FullSubsetIndex);
ENABLE_EXPLICIT_STRONG_TYPE_CAST(FullSubsetIndex, SubsetIndex);
ENABLE_EXPLICIT_STRONG_TYPE_CAST(ElementIndex, FullElementIndex);
ENABLE_EXPLICIT_STRONG_TYPE_CAST(FullElementIndex, ElementIndex);
#undef ENABLE_EXPLICIT_STRONG_TYPE_CAST

using FullElementCostVector = util_intops::StrongVector<FullElementIndex, Cost>;
using FullSubsetCostVector = util_intops::StrongVector<FullSubsetIndex, Cost>;
using FullElementBoolVector = util_intops::StrongVector<FullElementIndex, bool>;
using FullSubsetBoolVector = util_intops::StrongVector<FullSubsetIndex, bool>;
using FullElementToIntVector =
    util_intops::StrongVector<FullElementIndex, BaseInt>;
using FullSubsetToIntVector =
    util_intops::StrongVector<FullSubsetIndex, BaseInt>;

// When a sub-model is created, indices are compacted to be consecutive and
// starting from 0 (to reduce memory usage). Core ElementIndex to original
// ElementIndex mappings are stored to translate back to the original model
// space.
using FullToCoreElementMapVector =
    util_intops::StrongVector<FullElementIndex, ElementIndex>;
using CoreToFullElementMapVector =
    util_intops::StrongVector<ElementIndex, FullElementIndex>;

// The same applies to SubsetIndex, which also needs to be mapped back to the
// original indexing space.
using FullToCoreSubsetMapVector =
    util_intops::StrongVector<FullSubsetIndex, SubsetIndex>;
using CoreToFullSubsetMapVector =
    util_intops::StrongVector<SubsetIndex, FullSubsetIndex>;

class StrongModelView {
 private:
  // Transformations to convert between the core and full model columns.
  struct SparseColTransform {
    TransformView<ElementIndex, ColumnEntryIndex,
                  TypeCastTransform<ElementIndex, FullElementIndex>>
    operator()(const SparseColumn& column) const {
      return TransformView<ElementIndex, ColumnEntryIndex,
                           TypeCastTransform<ElementIndex, FullElementIndex>>(
          &column);
    }
  };

  // Transformations to convert between the core and full model rows.
  struct SparseRowTransform {
    TransformView<SubsetIndex, RowEntryIndex,
                  TypeCastTransform<SubsetIndex, FullSubsetIndex>>
    operator()(const SparseRow& row) const {
      return TransformView<SubsetIndex, RowEntryIndex,
                           TypeCastTransform<SubsetIndex, FullSubsetIndex>>(
          &row);
    }
  };

 public:
  StrongModelView() = default;
  explicit StrongModelView(const SetCoverModel* model) : model_(model) {}

  BaseInt num_subsets() const { return model_->num_subsets(); }
  BaseInt num_elements() const { return model_->num_elements(); }

  TransformView<Cost, FullSubsetIndex> subset_costs() const {
    return TransformView<Cost, FullSubsetIndex>(&model_->subset_costs());
  }
  TransformView<SparseColumn, FullSubsetIndex, SparseColTransform> columns()
      const {
    return TransformView<SparseColumn, FullSubsetIndex, SparseColTransform>(
        &model_->columns());
  }
  TransformView<SparseRow, FullElementIndex, SparseRowTransform> rows() const {
    return TransformView<SparseRow, FullElementIndex, SparseRowTransform>(
        &model_->rows());
  }
  util_intops::StrongIntRange<FullSubsetIndex> SubsetRange() const {
    return {FullSubsetIndex(), FullSubsetIndex(num_subsets())};
  }
  util_intops::StrongIntRange<FullElementIndex> ElementRange() const {
    return {FullElementIndex(), FullElementIndex(num_elements())};
  }
  const SetCoverModel& base() const { return *model_; }

 private:
  const SetCoverModel* model_;
};

class IndexListModelView {
 public:
  IndexListModelView() = default;
  IndexListModelView(const SetCoverModel* model,
                     const SubsetToIntVector* cols_sizes,
                     const ElementToIntVector* rows_sizes,
                     const std::vector<SubsetIndex>* cols_focus,
                     const std::vector<ElementIndex>* rows_focus)
      : model_(model),
        cols_sizes_(cols_sizes),
        rows_sizes_(rows_sizes),
        cols_focus_(cols_focus),
        rows_focus_(rows_focus) {}

  BaseInt num_subsets() const { return model_->num_subsets(); }
  BaseInt num_elements() const { return model_->num_elements(); }
  BaseInt num_focus_subsets() const { return cols_focus_->size(); }
  BaseInt num_focus_elements() const { return rows_focus_->size(); }

  IndexListView<Cost, SubsetIndex> subset_costs() const {
    return {&model_->subset_costs(), cols_focus_};
  }
  TwoLevelsView<IndexListView<SparseColumn, SubsetIndex>, ElementToIntVector>
  columns() const {
    return {{&model_->columns(), cols_focus_}, rows_sizes_};
  }
  TwoLevelsView<IndexListView<SparseRow, ElementIndex>, SubsetToIntVector>
  rows() const {
    return {{&model_->rows(), rows_focus_}, cols_sizes_};
  }
  const std::vector<SubsetIndex>& SubsetRange() const { return *cols_focus_; }
  const std::vector<ElementIndex>& ElementRange() const { return *rows_focus_; }
  FullElementIndex MapCoreToFullElementIndex(ElementIndex core_i) const {
    DCHECK(ElementIndex() <= core_i && core_i < ElementIndex(num_elements()));
    return static_cast<FullElementIndex>(core_i);
  }
  ElementIndex MapFullToCoreElementIndex(FullElementIndex full_i) const {
    DCHECK(FullElementIndex() <= full_i &&
           full_i < FullElementIndex(num_elements()));
    return static_cast<ElementIndex>(full_i);
  }
  FullSubsetIndex MapCoreToFullSubsetIndex(SubsetIndex core_j) const {
    DCHECK(SubsetIndex() <= core_j && core_j < SubsetIndex(num_subsets()));
    return static_cast<FullSubsetIndex>(core_j);
  }
  BaseInt column_size(SubsetIndex j) const {
    DCHECK(SubsetIndex() <= j && j < SubsetIndex(num_subsets()));
    return (*cols_sizes_)[j];
  }
  BaseInt row_size(ElementIndex i) const {
    DCHECK(ElementIndex() <= i && i < ElementIndex(num_elements()));
    return (*rows_sizes_)[i];
  }
  const SetCoverModel& base() const { return *model_; }

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
class FilterModelView {
 public:
  FilterModelView() = default;
  FilterModelView(const SetCoverModel* model,
                  const SubsetBoolVector* cols_sizes,
                  const ElementBoolVector* rows_sizes, BaseInt num_subsets,
                  BaseInt num_elements)
      : model_(model),
        is_focus_col_(cols_sizes),
        is_focus_row_(rows_sizes),
        num_subsets_(num_subsets),
        num_elements_(num_elements) {}

  BaseInt num_subsets() const { return model_->num_subsets(); }
  BaseInt num_elements() const { return model_->num_elements(); }
  BaseInt num_focus_subsets() const { return num_subsets_; }
  BaseInt num_focus_elements() const { return num_elements_; }

  IndexFilterView<Cost, SubsetBoolVector> subset_costs() const {
    return {&model_->subset_costs(), is_focus_col_};
  }
  TwoLevelsView<IndexFilterView<SparseColumn, SubsetBoolVector>,
                ElementBoolVector>
  columns() const {
    return {{&model_->columns(), is_focus_col_}, is_focus_row_};
  }
  TwoLevelsView<IndexFilterView<SparseRow, ElementBoolVector>, SubsetBoolVector>
  rows() const {
    return {{&model_->rows(), is_focus_row_}, is_focus_col_};
  }
  FilterIndexRangeView<SubsetIndex, SubsetBoolVector> SubsetRange() const {
    return FilterIndexRangeView<SubsetIndex, SubsetBoolVector>{is_focus_col_};
  }
  FilterIndexRangeView<ElementIndex, ElementBoolVector> ElementRange() const {
    return FilterIndexRangeView<ElementIndex, ElementBoolVector>{is_focus_row_};
  }
  bool IsFocusCol(SubsetIndex j) const { return (*is_focus_col_)[j]; }
  bool IsFocusRow(ElementIndex i) const { return (*is_focus_row_)[i]; }

  const SetCoverModel& base() const { return *model_; }

 private:
  const SetCoverModel* model_;
  const SubsetBoolVector* is_focus_col_;
  const ElementBoolVector* is_focus_row_;
  BaseInt num_subsets_;
  BaseInt num_elements_;
};

}  // namespace operations_research

#endif  // ORTOOLS_SET_COVER_SET_COVER_VIEWS_H_
