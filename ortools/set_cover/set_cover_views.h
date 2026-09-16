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

#include <functional>
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
// arises in PricingModel, where a "filtered" full-model must be handled. In
// such cases, static casts are employed to manage the type conversions
// effectively.
DEFINE_STRONG_INT_TYPE(FullModelSubsetIndex, BaseInt);
DEFINE_STRONG_INT_TYPE(FullModelElementIndex, BaseInt);

// Syntactic sugar to define strong-typed indices casts.
// Note: look at `strong_int.h` for more details about `StrongIntConvert`
#define DEFINE_STRONG_INT_CONVERSION(FROM, TO)            \
  constexpr TO StrongIntConvert(FROM j, TO* /*unused*/) { \
    return TO(static_cast<FROM::ValueType>(j));           \
  }
DEFINE_STRONG_INT_CONVERSION(SubsetIndex, FullModelSubsetIndex);
DEFINE_STRONG_INT_CONVERSION(FullModelSubsetIndex, SubsetIndex);
DEFINE_STRONG_INT_CONVERSION(ElementIndex, FullModelElementIndex);
DEFINE_STRONG_INT_CONVERSION(FullModelElementIndex, ElementIndex);
#undef DEFINE_STRONG_INT_CONVERSION

using FullElementCostVector =
    util_intops::StrongVector<FullModelElementIndex, Cost>;
using FullSubsetCostVector =
    util_intops::StrongVector<FullModelSubsetIndex, Cost>;
using FullElementBoolVector =
    util_intops::StrongVector<FullModelElementIndex, bool>;
using FullSubsetBoolVector =
    util_intops::StrongVector<FullModelSubsetIndex, bool>;
using FullElementToIntVector =
    util_intops::StrongVector<FullModelElementIndex, BaseInt>;
using FullSubsetToIntVector =
    util_intops::StrongVector<FullModelSubsetIndex, BaseInt>;

// When a sub-model is created, indices are compacted to be consecutive and
// starting from 0 (to reduce memory usage). Core ElementIndex to original
// ElementIndex mappings are stored to translate back to the original model
// space.
using FullToCoreElementMapVector =
    util_intops::StrongVector<FullModelElementIndex, ElementIndex>;
using CoreToFullElementMapVector =
    util_intops::StrongVector<ElementIndex, FullModelElementIndex>;

// The same applies to SubsetIndex, which also needs to be mapped back to the
// original indexing space.
using FullToCoreSubsetMapVector =
    util_intops::StrongVector<FullModelSubsetIndex, SubsetIndex>;
using CoreToFullSubsetMapVector =
    util_intops::StrongVector<SubsetIndex, FullModelSubsetIndex>;

// View that exposes the entire SetCoverModel with full-model typed indices.
class FullModelView {
 private:
  // Transform struct to wrap SparseColumns into a transform view converting
  // core element indices to full element indices.
  struct SparseColumnTransform {
    // Converts a sparse column by wrapping it into a TransformView.
    TransformView<ElementIndex, ColumnEntryIndex,
                  StaticCastTransform<ElementIndex, FullModelElementIndex>>
    operator()(const SparseColumn& column) const {
      return TransformView<
          ElementIndex, ColumnEntryIndex,
          StaticCastTransform<ElementIndex, FullModelElementIndex>>(&column);
    }
  };

  // Transform struct to wrap SparseRows into a transform view converting
  // core subset indices to full subset indices.
  struct SparseRowTransform {
    // Converts a sparse row by wrapping it into a TransformView.
    TransformView<SubsetIndex, RowEntryIndex,
                  StaticCastTransform<SubsetIndex, FullModelSubsetIndex>>
    operator()(const SparseRow& row) const {
      return TransformView<
          SubsetIndex, RowEntryIndex,
          StaticCastTransform<SubsetIndex, FullModelSubsetIndex>>(&row);
    }
  };

 public:
  // Constructor.
  explicit FullModelView(const SetCoverModel& model) : model_(model) {
    DCHECK_GE(model.num_subsets(), 0);
    DCHECK_GE(model.num_elements(), 0);
  }

  // Returns the number of subsets in the model.
  BaseInt num_subsets() const { return model_.get().num_subsets(); }

  // Returns the number of elements in the model.
  BaseInt num_elements() const { return model_.get().num_elements(); }

  // Returns a TransformView for the subset costs, mapping index key types.
  TransformView<Cost, FullModelSubsetIndex> subset_costs() const {
    return TransformView<Cost, FullModelSubsetIndex>(
        &model_.get().subset_costs());
  }

  // Returns a TransformView of the columns, translating internal indices
  // of each column to full element indices.
  TransformView<SparseColumn, FullModelSubsetIndex, SparseColumnTransform>
  columns() const {
    return TransformView<SparseColumn, FullModelSubsetIndex,
                         SparseColumnTransform>(&model_.get().columns());
  }

  // Returns a TransformView of the rows, translating internal indices
  // of each row to full subset indices.
  TransformView<SparseRow, FullModelElementIndex, SparseRowTransform> rows()
      const {
    return TransformView<SparseRow, FullModelElementIndex, SparseRowTransform>(
        &model_.get().rows());
  }

  // Returns a range of all subset indices in the full model.
  util_intops::StrongIntRange<FullModelSubsetIndex> SubsetRange() const {
    return {FullModelSubsetIndex(), FullModelSubsetIndex(num_subsets())};
  }

  // Returns a range of all element indices in the full model.
  util_intops::StrongIntRange<FullModelElementIndex> ElementRange() const {
    return {FullModelElementIndex(), FullModelElementIndex(num_elements())};
  }

  // Returns the underlying base SetCoverModel.
  const SetCoverModel& base() const {
    const SetCoverModel& result = model_.get();
    return result;
  }

 private:
  // Reference wrapper for the underlying SetCoverModel.
  std::reference_wrapper<const SetCoverModel> model_;
};

// View exposing a subset of columns/rows that are in focus.
class FocusModelView {
 public:
  // Constructor.
  FocusModelView(const SetCoverModel& model,
                 const SubsetToIntVector& column_sizes,
                 const ElementToIntVector& row_sizes,
                 const std::vector<SubsetIndex>& columns_in_focus,
                 const std::vector<ElementIndex>& rows_in_focus)
      : model_(model),
        column_sizes_(column_sizes),
        row_sizes_(row_sizes),
        columns_in_focus_(columns_in_focus),
        rows_in_focus_(rows_in_focus) {
    DCHECK_GE(model.num_subsets(), 0);
    DCHECK_GE(model.num_elements(), 0);
  }

  // Returns the total number of subsets in the base model.
  BaseInt num_subsets() const { return model_.get().num_subsets(); }

  // Returns the total number of elements in the base model.
  BaseInt num_elements() const { return model_.get().num_elements(); }

  // Returns the number of subsets in focus.
  BaseInt num_focus_subsets() const { return columns_in_focus_.get().size(); }

  // Returns the number of elements in focus.
  BaseInt num_focus_elements() const { return rows_in_focus_.get().size(); }

  // Returns index list view of subset costs.
  IndexListView<Cost, SubsetIndex> subset_costs() const {
    return {&model_.get().subset_costs(), &columns_in_focus_.get()};
  }

  // Returns nested masked view of active columns.
  NestedMaskedView<IndexListView<SparseColumn, SubsetIndex>, ElementToIntVector>
  columns() const {
    return {{&model_.get().columns(), &columns_in_focus_.get()},
            row_sizes_.get()};
  }

  // Returns nested masked view of active rows.
  NestedMaskedView<IndexListView<SparseRow, ElementIndex>, SubsetToIntVector>
  rows() const {
    return {{&model_.get().rows(), &rows_in_focus_.get()}, column_sizes_.get()};
  }

  // Returns a reference to the active subsets.
  const std::vector<SubsetIndex>& SubsetRange() const {
    return columns_in_focus_.get();
  }

  // Returns a reference to the active elements.
  const std::vector<ElementIndex>& ElementRange() const {
    return rows_in_focus_.get();
  }

  // Maps a core element index to a full element index.
  FullModelElementIndex MapCoreToFullElementIndex(ElementIndex core_i) const {
    DCHECK(ElementIndex() <= core_i && core_i < ElementIndex(num_elements()));
    return static_cast<FullModelElementIndex>(core_i);
  }

  // Maps a full element index to a core element index.
  ElementIndex MapFullToCoreElementIndex(FullModelElementIndex full_i) const {
    DCHECK(FullModelElementIndex() <= full_i &&
           full_i < FullModelElementIndex(num_elements()));
    return static_cast<ElementIndex>(full_i);
  }

  // Maps a core subset index to a full subset index.
  FullModelSubsetIndex MapCoreToFullSubsetIndex(SubsetIndex core_j) const {
    DCHECK(SubsetIndex() <= core_j && core_j < SubsetIndex(num_subsets()));
    return static_cast<FullModelSubsetIndex>(core_j);
  }

  // Returns the column size of the specified subset.
  BaseInt column_size(SubsetIndex j) const {
    DCHECK(SubsetIndex() <= j && j < SubsetIndex(num_subsets()));
    return column_sizes_.get()[j];
  }

  // Returns the row size of the specified element.
  BaseInt row_size(ElementIndex i) const {
    DCHECK(ElementIndex() <= i && i < ElementIndex(num_elements()));
    return row_sizes_.get()[i];
  }

  // Returns the underlying base SetCoverModel.
  const SetCoverModel& base() const { return model_.get(); }

 private:
  // Reference wrapper for the underlying SetCoverModel.
  std::reference_wrapper<const SetCoverModel> model_;

  // Reference wrapper for column sizes.
  std::reference_wrapper<const SubsetToIntVector> column_sizes_;

  // Reference wrapper for row sizes.
  std::reference_wrapper<const ElementToIntVector> row_sizes_;

  // Reference wrapper for the subset focus vector.
  std::reference_wrapper<const std::vector<SubsetIndex>> columns_in_focus_;

  // Reference wrapper for the element focus vector.
  std::reference_wrapper<const std::vector<ElementIndex>> rows_in_focus_;
};

// A lightweight sub-model view that uses boolean vectors to enable or disable
// specific items. Iterating over all active columns or rows is less efficient,
// particularly when only a now small subset is active.
// NOTE: this view does **not** store any size-related information.
class MaskedModelView {
 public:
  // Constructor.
  MaskedModelView(const SetCoverModel& model,
                  const SubsetBoolVector& column_mask,
                  const ElementBoolVector& row_mask, BaseInt num_subsets,
                  BaseInt num_elements)
      : model_(model),
        column_mask_(column_mask),
        row_mask_(row_mask),
        num_subsets_(num_subsets),
        num_elements_(num_elements) {}

  // Returns the number of subsets in the model.
  BaseInt num_subsets() const { return model_.get().num_subsets(); }

  // Returns the number of elements in the model.
  BaseInt num_elements() const { return model_.get().num_elements(); }

  // Returns the number of active subsets currently in focus.
  BaseInt num_focus_subsets() const { return num_subsets_; }

  // Returns the number of active elements currently in focus.
  BaseInt num_focus_elements() const { return num_elements_; }

  // Returns a MaskedValuesView of subset costs.
  MaskedValuesView<Cost, SubsetBoolVector> subset_costs() const {
    return {&model_.get().subset_costs(), column_mask_.get()};
  }

  // Returns a NestedMaskedView of the columns.
  NestedMaskedView<MaskedValuesView<SparseColumn, SubsetBoolVector>,
                   ElementBoolVector>
  columns() const {
    return {{&model_.get().columns(), column_mask_.get()}, row_mask_.get()};
  }

  // Returns a NestedMaskedView of the rows.
  NestedMaskedView<MaskedValuesView<SparseRow, ElementBoolVector>,
                   SubsetBoolVector>
  rows() const {
    return {{&model_.get().rows(), row_mask_.get()}, column_mask_.get()};
  }

  // Returns a MaskedIndicesView of active subset indices.
  MaskedIndicesView<SubsetIndex, SubsetBoolVector> SubsetRange() const {
    return MaskedIndicesView<SubsetIndex, SubsetBoolVector>{column_mask_.get()};
  }

  // Returns a MaskedIndicesView of active element indices.
  MaskedIndicesView<ElementIndex, ElementBoolVector> ElementRange() const {
    return MaskedIndicesView<ElementIndex, ElementBoolVector>{row_mask_.get()};
  }

  // Returns true if the column is in focus.
  bool IsColumnInFocus(SubsetIndex j) const { return column_mask_.get()[j]; }

  // Returns true if the row is in focus.
  bool IsRowInFocus(ElementIndex i) const { return row_mask_.get()[i]; }

  // Returns the underlying base SetCoverModel.
  const SetCoverModel& base() const { return model_.get(); }

 private:
  // Reference wrapper for the underlying SetCoverModel.
  std::reference_wrapper<const SetCoverModel> model_;

  // Reference wrapper for the subset enable mask.
  std::reference_wrapper<const SubsetBoolVector> column_mask_;

  // Reference wrapper for the element enable mask.
  std::reference_wrapper<const ElementBoolVector> row_mask_;

  // Number of active subsets.
  BaseInt num_subsets_;

  // Number of active elements.
  BaseInt num_elements_;
};

}  // namespace operations_research

#endif  // ORTOOLS_SET_COVER_SET_COVER_VIEWS_H_
