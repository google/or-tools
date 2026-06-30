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
DEFINE_STRONG_INT_TYPE(FullSubsetIndex, BaseInt);
DEFINE_STRONG_INT_TYPE(FullElementIndex, BaseInt);

// Syntactic sugar to define strong-typed indices casts.
// Note: look at `strong_int.h` for more details about `StrongIntConvert`
#define DEFINE_STRONG_INT_CONVERSION(FROM, TO)            \
  constexpr TO StrongIntConvert(FROM j, TO* /*unused*/) { \
    return TO(static_cast<FROM::ValueType>(j));           \
  }
DEFINE_STRONG_INT_CONVERSION(SubsetIndex, FullSubsetIndex);
DEFINE_STRONG_INT_CONVERSION(FullSubsetIndex, SubsetIndex);
DEFINE_STRONG_INT_CONVERSION(ElementIndex, FullElementIndex);
DEFINE_STRONG_INT_CONVERSION(FullElementIndex, ElementIndex);
#undef DEFINE_STRONG_INT_CONVERSION

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

class FullModelView {
 private:
  // Transformations to convert between the core and full model columns.
  struct SparseColumnTransform {
    TransformView<ElementIndex, ColumnEntryIndex,
                  StaticCastTransform<ElementIndex, FullElementIndex>>
    operator()(const SparseColumn& column) const {
      return TransformView<ElementIndex, ColumnEntryIndex,
                           StaticCastTransform<ElementIndex, FullElementIndex>>(
          &column);
    }
  };

  // Transformations to convert between the core and full model rows.
  struct SparseRowTransform {
    TransformView<SubsetIndex, RowEntryIndex,
                  StaticCastTransform<SubsetIndex, FullSubsetIndex>>
    operator()(const SparseRow& row) const {
      return TransformView<SubsetIndex, RowEntryIndex,
                           StaticCastTransform<SubsetIndex, FullSubsetIndex>>(
          &row);
    }
  };

 public:
  explicit FullModelView(const SetCoverModel& model) : model_(model) {}

  BaseInt num_subsets() const { return model_.get().num_subsets(); }
  BaseInt num_elements() const { return model_.get().num_elements(); }

  TransformView<Cost, FullSubsetIndex> subset_costs() const {
    return TransformView<Cost, FullSubsetIndex>(&model_.get().subset_costs());
  }
  TransformView<SparseColumn, FullSubsetIndex, SparseColumnTransform> columns()
      const {
    return TransformView<SparseColumn, FullSubsetIndex, SparseColumnTransform>(
        &model_.get().columns());
  }
  TransformView<SparseRow, FullElementIndex, SparseRowTransform> rows() const {
    return TransformView<SparseRow, FullElementIndex, SparseRowTransform>(
        &model_.get().rows());
  }
  util_intops::StrongIntRange<FullSubsetIndex> SubsetRange() const {
    return {FullSubsetIndex(), FullSubsetIndex(num_subsets())};
  }
  util_intops::StrongIntRange<FullElementIndex> ElementRange() const {
    return {FullElementIndex(), FullElementIndex(num_elements())};
  }
  const SetCoverModel& base() const { return model_.get(); }

 private:
  std::reference_wrapper<const SetCoverModel> model_;
};

class FocusModelView {
 public:
  FocusModelView(const SetCoverModel& model,
                 const SubsetToIntVector& column_sizes,
                 const ElementToIntVector& row_sizes,
                 const std::vector<SubsetIndex>& column_focus,
                 const std::vector<ElementIndex>& row_focus)
      : model_(model),
        column_sizes_(column_sizes),
        row_sizes_(row_sizes),
        column_focus_(column_focus),
        row_focus_(row_focus) {}

  BaseInt num_subsets() const { return model_.get().num_subsets(); }
  BaseInt num_elements() const { return model_.get().num_elements(); }
  BaseInt num_focus_subsets() const { return column_focus_.get().size(); }
  BaseInt num_focus_elements() const { return row_focus_.get().size(); }

  IndexListView<Cost, SubsetIndex> subset_costs() const {
    return {&model_.get().subset_costs(), &column_focus_.get()};
  }
  NestedMaskedView<IndexListView<SparseColumn, SubsetIndex>, ElementToIntVector>
  columns() const {
    return {{&model_.get().columns(), &column_focus_.get()}, row_sizes_.get()};
  }
  NestedMaskedView<IndexListView<SparseRow, ElementIndex>, SubsetToIntVector>
  rows() const {
    return {{&model_.get().rows(), &row_focus_.get()}, column_sizes_.get()};
  }
  const std::vector<SubsetIndex>& SubsetRange() const {
    return column_focus_.get();
  }
  const std::vector<ElementIndex>& ElementRange() const {
    return row_focus_.get();
  }
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
    return column_sizes_.get()[j];
  }
  BaseInt row_size(ElementIndex i) const {
    DCHECK(ElementIndex() <= i && i < ElementIndex(num_elements()));
    return row_sizes_.get()[i];
  }
  const SetCoverModel& base() const { return model_.get(); }

 private:
  std::reference_wrapper<const SetCoverModel> model_;
  std::reference_wrapper<const SubsetToIntVector> column_sizes_;
  std::reference_wrapper<const ElementToIntVector> row_sizes_;
  std::reference_wrapper<const std::vector<SubsetIndex>> column_focus_;
  std::reference_wrapper<const std::vector<ElementIndex>> row_focus_;
};

// A lightweight sub-model view that uses boolean vectors to enable or disable
// specific items. Iterating over all active columns or rows is less efficient,
// particularly when only a now small subset is active.
// NOTE: this view does **not** store any size-related information.
class MaskedModelView {
 public:
  MaskedModelView(const SetCoverModel& model,
                  const SubsetBoolVector& column_mask,
                  const ElementBoolVector& row_mask, BaseInt num_subsets,
                  BaseInt num_elements)
      : model_(model),
        column_mask_(column_mask),
        row_mask_(row_mask),
        num_subsets_(num_subsets),
        num_elements_(num_elements) {}

  BaseInt num_subsets() const { return model_.get().num_subsets(); }
  BaseInt num_elements() const { return model_.get().num_elements(); }
  BaseInt num_focus_subsets() const { return num_subsets_; }
  BaseInt num_focus_elements() const { return num_elements_; }

  MaskedValuesView<Cost, SubsetBoolVector> subset_costs() const {
    return {&model_.get().subset_costs(), column_mask_.get()};
  }
  NestedMaskedView<MaskedValuesView<SparseColumn, SubsetBoolVector>,
                   ElementBoolVector>
  columns() const {
    return {{&model_.get().columns(), column_mask_.get()}, row_mask_.get()};
  }
  NestedMaskedView<MaskedValuesView<SparseRow, ElementBoolVector>,
                   SubsetBoolVector>
  rows() const {
    return {{&model_.get().rows(), row_mask_.get()}, column_mask_.get()};
  }
  MaskedIndicesView<SubsetIndex, SubsetBoolVector> SubsetRange() const {
    return MaskedIndicesView<SubsetIndex, SubsetBoolVector>{column_mask_.get()};
  }
  MaskedIndicesView<ElementIndex, ElementBoolVector> ElementRange() const {
    return MaskedIndicesView<ElementIndex, ElementBoolVector>{row_mask_.get()};
  }
  bool IsColumnInFocus(SubsetIndex j) const { return column_mask_.get()[j]; }
  bool IsRowInFocus(ElementIndex i) const { return row_mask_.get()[i]; }

  const SetCoverModel& base() const { return model_.get(); }

 private:
  std::reference_wrapper<const SetCoverModel> model_;
  std::reference_wrapper<const SubsetBoolVector> column_mask_;
  std::reference_wrapper<const ElementBoolVector> row_mask_;
  BaseInt num_subsets_;
  BaseInt num_elements_;
};

}  // namespace operations_research

#endif  // ORTOOLS_SET_COVER_SET_COVER_VIEWS_H_
