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

#ifndef ORTOOLS_SET_COVER_SET_COVER_SUBMODEL_H_
#define ORTOOLS_SET_COVER_SET_COVER_SUBMODEL_H_

#include <functional>
#include <limits>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_model.h"
#include "ortools/set_cover/set_cover_views.h"

namespace operations_research {

// Forward declarations, see below for the definition of the classes.
struct PrimalDualState;
class SubmodelSolution;
class DualState;

static constexpr SubsetIndex kNullSubsetIndex =
    std::numeric_limits<SubsetIndex>::max();
static constexpr ElementIndex kNullElementIndex =
    std::numeric_limits<ElementIndex>::max();
static constexpr FullModelSubsetIndex kNullFullModelSubsetIndex =
    std::numeric_limits<FullModelSubsetIndex>::max();
static constexpr FullModelElementIndex kNullFullModelElementIndex =
    std::numeric_limits<FullModelElementIndex>::max();

// The CFT algorithm generates sub-models in two distinct ways:
//
// 1. It fixes specific columns (incrementally) into any generated solution.
//    Once a column is fixed, it is excluded from future decisions, as it is
//    already part of the solution. Additionally, rows that are covered by the
//    fixed columns are removed from consideration as well, along with any
//    columns that exclusively cover those rows, as they become redundant. The
//    fixing process starts with the entire model and progressively fixes more
//    columns until it becomes empty. A "view-based" sub-model is well-suited
//    for this part.
//
// 2. The CFT mostly works on a "core" sub-model by focusing on a subset of
//    columns. The core model is derived from the original model but is
//    significantly smaller, as it typically includes only a limited number of
//    columns per row (on average, around six columns per row). Unlike the
//    incremental nature of column fixing, core models are constructed from
//    scratch during each update. This type of small model can take advantage of
//    a SetCoverModel object which stores the sub-model explicitly in memory,
//    avoiding looping over "inactive" columns and rows. Both SubmodelView and
//    CoreModel can be used as a core model.
//
// Two types of "core-model" representations are implemented, both of which can
// be used interchangeably:
//
// 1. SubmodelView: A lightweight view of the original model. It dynamically
//    filters and exposes only the active rows and columns from the original
//    data structures, skipping "inactive" items.
//
// 2. CoreModel: A fully compacted and explicit representation of a sub-model.
//    It stores the filtered data explicitly, making it more suitable
//    for scenarios where compact storage and faster access are required.
//
// While CoreModel stores an explicit representation of the sub-model,
// SubmodelView maintains vectors sized according to the original model's
// dimensions. As a result, depending on the dimensions of the original model,
// CoreModel can actually be more memory-efficient.
class SubmodelView;
class CoreModel;

// `SubmodelView` provides a mechanism to interact with a subset of the rows and
// columns of a SetCoverModel, effectively creating a filtered view of the
// model. This abstraction allows operations to be performed on a restricted
// portion of the model without modifying the original data structure. The
// filtering is achieved using index lists and size vectors, which define the
// active rows and columns. This approach ensures flexibility and avoids
// unnecessary duplication of data. Columns/rows sizes are used to both keep
// track of the number of elements in them and also provide the "activation"
// status: (item size == 0) <==> inactive
// SubmodelView inherits from FocusModelView, which provides the "view"
// machinery.
class SubmodelView : public FocusModelView {
  // Base class view type alias.
  using base_view = FocusModelView;

 public:
  // Identity sub-model: all items are considered
  explicit SubmodelView(const SetCoverModel& model);

  // Focus construction: create a sub-model with only the required items
  SubmodelView(const SetCoverModel& model,
               const std::vector<FullModelSubsetIndex>& columns_in_focus);

  virtual ~SubmodelView() = default;

  ///////// Core-model interface: /////////

  // Current fixed cost: sum of the cost of the fixed columns
  Cost fixed_cost() const {
    DCHECK_GE(fixed_cost_, 0.0);
    return fixed_cost_;
  }

  // List of fixed columns.
  const std::vector<FullModelSubsetIndex>& fixed_columns() const {
    DCHECK(CheckIndices(fixed_columns_,
                        FullModelSubsetIndex(full_model_.get().num_subsets())));
    return fixed_columns_;
  }

  // Redefine the active items. The new sub-model will ignore all columns not in
  // focus and (optionally) the rows for which row_flags is not true. It does
  // not overwrite the current fixing.
  void SetFocus(const std::vector<FullModelSubsetIndex>& columns_in_focus);

  // Fix the provided columns, removing them from the submodel. Rows now covered
  // by fixed columns are also removed from the submodel along with non-fixed
  // columns that only cover those rows.
  virtual Cost FixMoreColumns(const std::vector<SubsetIndex>& columns_to_fix);

  // Resets the column fixing state using the given full column indices.
  virtual void ResetColumnFixing(
      const std::vector<FullModelSubsetIndex>& columns_to_fix,
      const DualState& state);

  // Hook function for specializations. This function can be used to define a
  // "small" core model considering a subset of the full model through the use
  // of column-generation or by only selecting columns with good reduced cost in
  // the full model.
  virtual bool UpdateCore(Cost best_lower_bound,
                          const ElementCostVector& best_multipliers,
                          const SubmodelSolution& best_solution, bool force) {
    DCHECK_GE(best_lower_bound, 0.0);
#ifndef NDEBUG
    for (const Cost m : best_multipliers) {
      DCHECK_GE(m, 0.0);
    }
#endif
    (void)best_lower_bound;
    (void)best_multipliers;
    (void)best_solution;
    (void)force;
    return false;
  }

  // Returns a typed view of the full model.
  FullModelView StrongTypedFullModelView() const {
    return FullModelView(full_model_.get());
  }

 private:
  // Resets the sub-model back to the identity view.
  void ResetToIdentitySubmodel();

  // Pointer to the original model
  std::reference_wrapper<const SetCoverModel> full_model_;

  // Columns/rows sizes after filtering (size==0 <==> inactive)
  SubsetToIntVector column_sizes_;

  // Array containing size of each row after filtering.
  ElementToIntVector row_sizes_;

  // List of columns/rows currently active
  std::vector<SubsetIndex> columns_in_focus_;

  // List of active row indices in focus.
  std::vector<ElementIndex> rows_in_focus_;

  // List of fixed subset indices in full model space.
  std::vector<FullModelSubsetIndex> fixed_columns_;

  // Accumulated cost of the fixed columns.
  Cost fixed_cost_ = 0.0;
};

// CoreModel stores a subset of the filtered columns and rows in an explicit
// SetCoverModel object.
// The indices are compacted and mapped to the range [0, <sub-model-size>],
// effectively creating a smaller set-covering model. Similar to SubmodelView,
// the core model supports column fixing and focusing on a subset of the
// original model. Mappings are maintained to translate indices back to the
// original model space.
class CoreModel : public SetCoverModel {
 public:
  // Identity sub-model: all items are considered
  explicit CoreModel(const SetCoverModel& model);

  // Focus construction: create a sub-model with only the required items
  CoreModel(const SetCoverModel& model,
            const std::vector<FullModelSubsetIndex>& columns_in_focus);

  virtual ~CoreModel() = default;

  ///////// Sub-model view interface: /////////

  // Returns the number of subsets in the full model.
  BaseInt num_subsets() const { return full_model_.num_subsets(); }

  // Returns the number of elements in the full model.
  BaseInt num_elements() const { return full_model_.num_elements(); }

  // Returns the number of subsets in focus.
  BaseInt num_focus_subsets() const { return SetCoverModel::num_subsets(); }

  // Returns the number of elements in focus.
  BaseInt num_focus_elements() const { return SetCoverModel::num_elements(); }

  // Returns the size of the column for the given subset.
  BaseInt column_size(SubsetIndex j) const {
    DCHECK_GE(j, SubsetIndex());
    DCHECK_LT(j, SubsetIndex(num_subsets()));
    return columns()[j].size();
  }

  // Returns the size of the row for the given element.
  BaseInt row_size(ElementIndex i) const {
    DCHECK_GE(i, ElementIndex());
    DCHECK_LT(i, ElementIndex(num_elements()));
    return rows()[i].size();
  }

  // Maps a core element index to a full element index.
  FullModelElementIndex MapCoreToFullElementIndex(ElementIndex core_i) const {
    DCHECK(ElementIndex() <= core_i && core_i < ElementIndex(num_elements()));
    FullModelElementIndex result = core_to_full_row_map_[core_i];
    DCHECK(result == kNullFullModelElementIndex ||
           (FullModelElementIndex() <= result &&
            result < FullModelElementIndex(num_elements())));
    return result;
  }

  // Maps a full element index to a core element index.
  ElementIndex MapFullToCoreElementIndex(FullModelElementIndex full_i) const {
    DCHECK(FullModelElementIndex() <= full_i &&
           full_i < FullModelElementIndex(num_elements()));
    ElementIndex result = full_to_core_row_map_[full_i];
    DCHECK(result == kNullElementIndex ||
           (ElementIndex() <= result && result < ElementIndex(num_elements())));
    return result;
  }

  // Maps a core subset index to a full subset index.
  FullModelSubsetIndex MapCoreToFullSubsetIndex(SubsetIndex core_j) const {
    DCHECK(SubsetIndex() <= core_j && core_j < SubsetIndex(num_subsets()));
    FullModelSubsetIndex result = core_to_full_column_map_[core_j];
    DCHECK(result == kNullFullModelSubsetIndex ||
           (FullModelSubsetIndex() <= result &&
            result < FullModelSubsetIndex(num_subsets())));
    return result;
  }

  // Member functions relevant for the CFT inherited from SetCoverModel
  using SetCoverModel::columns;

  // Range of active element indices.
  using SetCoverModel::ElementRange;

  // Sparse rows view in the SetCoverModel.
  using SetCoverModel::rows;

  // Vector of subset costs.
  using SetCoverModel::subset_costs;

  // Range of active subset indices.
  using SetCoverModel::SubsetRange;

  ///////// Core-model interface: /////////

  // Current fixed cost: sum of the cost of the fixed columns
  Cost fixed_cost() const {
    DCHECK_GE(fixed_cost_, 0.0);
    return fixed_cost_;
  }

  // List of fixed columns.
  const std::vector<FullModelSubsetIndex>& fixed_columns() const {
    DCHECK(CheckIndices(fixed_columns_, FullModelSubsetIndex(num_subsets())));
    return fixed_columns_;
  }

  // Redefine the active items. The new sub-model will ignore all columns not in
  // focus and (optionally) the rows for which row_flags is not true. It does
  // not overwrite the current fixing.
  void SetFocus(const std::vector<FullModelSubsetIndex>& columns_in_focus);

  // Fix the provided columns, removing them from the submodel. Rows now covered
  // by fixed columns are also removed from the submodel along with non-fixed
  // columns that only cover those rows.
  virtual Cost FixMoreColumns(const std::vector<SubsetIndex>& columns_to_fix);

  // Resets the column fixing state using the given full column indices.
  virtual void ResetColumnFixing(
      const std::vector<FullModelSubsetIndex>& columns_to_fix,
      const DualState& state);

  // Hook function for specializations. This function can be used to define a
  // "small" core model considering a subset of the full model through the use
  // of column-generation or by only selecting columns with good reduced cost in
  // the full model.
  virtual bool UpdateCore(Cost best_lower_bound,
                          const ElementCostVector& best_multipliers,
                          const SubmodelSolution& best_solution, bool force) {
    DCHECK_GE(best_lower_bound, 0.0);
#ifndef NDEBUG
    for (const Cost m : best_multipliers) {
      DCHECK_GE(m, 0.0);
    }
#endif
    (void)best_lower_bound;
    (void)best_multipliers;
    (void)best_solution;
    (void)force;
    return false;
  }

  FullModelView StrongTypedFullModelView() const { return full_model_; }

 private:
  // Updates row and column index mappings under the given column fixing.
  void UpdateMappingsForFixedColumns(
      const std::vector<SubsetIndex>& columns_to_fix);

  // Computes new row mapping vectors after updating fixed columns.
  CoreToFullElementMapVector ComputeRowMappings();

  // Creates a new compacted core model applying the given row mappings.
  SetCoverModel MakeNewCoreModel(
      const CoreToFullElementMapVector& new_core_to_full_row_map);

  // Resets the core model back to the identity view.
  void ResetToIdentitySubmodel();

  // Pointer to the original model
  FullModelView full_model_;

  // Mapping index array from full element indices to core element indices.
  FullToCoreElementMapVector full_to_core_row_map_;

  // Mapping index array from core element indices to full element indices.
  CoreToFullElementMapVector core_to_full_row_map_;

  // Mapping index array from core subset indices to full subset indices.
  CoreToFullSubsetMapVector core_to_full_column_map_;

  // Fixing data
  Cost fixed_cost_ = 0.0;

  // List of fixed subset indices in full model space.
  std::vector<FullModelSubsetIndex> fixed_columns_;
};

// Validates that the submodel satisfies consistency invariants.
template <typename SubmodelT>
bool ValidateSubmodel(const SubmodelT& model) {
  if (model.num_elements() <= 0) {
    LOG(ERROR) << "Submodel has no elements.\n";
    return false;
  }
  if (model.num_subsets() <= 0) {
    LOG(ERROR) << "Submodel has no subsets.\n";
    return false;
  }

  for (SubsetIndex j : model.SubsetRange()) {
    const auto& column = model.columns()[j];
    if (model.column_size(j) == 0) {
      LOG(ERROR) << "Column " << j << " is empty.\n";
      return false;
    }
    BaseInt j_size = std::distance(column.begin(), column.end());
    if (j_size != model.column_size(j)) {
      LOG(ERROR) << "Submodel size mismatch on column " << j << ", " << j_size
                 << " != " << model.column_size(j) << "\n";
      return false;
    }
  }

  for (ElementIndex i : model.ElementRange()) {
    const auto& row = model.rows()[i];
    if (model.row_size(i) == 0) {
      LOG(ERROR) << "Row " << i << " is empty.\n";
      return false;
    }
    BaseInt i_size = std::distance(row.begin(), row.end());
    if (i_size != model.row_size(i)) {
      LOG(ERROR) << "Submodel size mismatch on row " << i << ", " << i_size
                 << " != " << model.row_size(i) << "\n";
      return false;
    }
  }
  return true;
}

}  // namespace operations_research

#endif  // ORTOOLS_SET_COVER_SET_COVER_SUBMODEL_H_
