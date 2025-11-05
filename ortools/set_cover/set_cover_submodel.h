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

#ifndef ORTOOLS_SET_COVER_SET_COVER_SUBMODEL_H
#define ORTOOLS_SET_COVER_SET_COVER_SUBMODEL_H

#include "ortools/set_cover/set_cover_views.h"

namespace operations_research::scp {

// TODO(anyone): since we are working within the scp namespace, the "SetCover*"
// prefix became redundant and can be removed. For now, only redefine it
// locally.
using Model = SetCoverModel;

// Forward declarations, see below for the definition of the classes.
struct PrimalDualState;
struct Solution;
struct DualState;

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
//    a Model object which stores the sub-model explicitly in memory, avoiding
//    looping over "inactive" columns and rows. Both SubModelView and CoreModel
//    can be used as a core model.
//
// Two types of "core-model" representations are implemented, both of which can
// be used interchangeably:
//
// 1. SubModelView: A lightweight view of the original model. It dynamically
//    filters and exposes only the active rows and columns from the original
//    data structures, skipping "inactive" items.
//
// 2. CoreModel: A fully compacted and explicit representation of a sub-model.
//    It stores the filtered data explicitly, making it more suitable
//    for scenarios where compact storage and faster access are required.
//
// While CoreModel stores an explicit representation of the sub-model,
// SubModelView maintains vectors sized according to the original model's
// dimensions. As a result, depending on the dimensions of the original model,
// CoreModel can actually be more memory-efficient.
class SubModelView;
class CoreModel;
using SubModel = CoreModel;

// `SubModelView` provides a mechanism to interact with a subset of the rows and
// columns of a SetCoverModel, effectively creating a filtered view of the
// model. This abstraction allows operations to be performed on a restricted
// portion of the model without modifying the original data structure. The
// filtering is achieved using index lists and sizes vectors, which define the
// active rows and columns. This approach ensures flexibility and avoids
// unnecessary duplication of data. Columns/rows sizes are uses to both keep
// track of the number of elements in them and also provide the "activation"
// status: (item size == 0) <==> inactive
// SubModelView inherits from IndexListSubModelView, which provides the "view"
// machinery.
class SubModelView : public IndexListModelView {
  using base_view = IndexListModelView;

 public:
  // Empty initialization to facilitate delayed construction
  SubModelView() = default;

  // Identity sub-model: all items are considered
  SubModelView(const Model* model);

  // Focus construction: create a sub-model with only the required items
  SubModelView(const Model* model,
               const std::vector<FullSubsetIndex>& columns_focus);

  virtual ~SubModelView() = default;

  ///////// Core-model interface: /////////

  // Current fixed cost: sum of the cost of the fixed columns
  Cost fixed_cost() const { return fixed_cost_; }

  // List of fixed columns.
  const std::vector<FullSubsetIndex>& fixed_columns() const {
    return fixed_columns_;
  }

  // Redefine the active items. The new sub-model will ignore all columns not in
  // focus and (optionally) the rows for which row_flags is not true. It does
  // not overwrite the current fixing.
  void SetFocus(const std::vector<FullSubsetIndex>& columns_focus);

  // Fix the provided columns, removing them for the submodel. Rows now covered
  // by fixed columns are also removed from the submodel along with non-fixed
  // columns that only cover those rows.
  virtual Cost FixMoreColumns(const std::vector<SubsetIndex>& columns_to_fix);

  virtual void ResetColumnFixing(
      const std::vector<FullSubsetIndex>& columns_to_fix,
      const DualState& state);

  // Hook function for specializations. This function can be used to define a
  // "small" core model considering a subset of the full model through the use
  // of column-generation or by only selecting columns with good reduced cost in
  // the full model.
  virtual bool UpdateCore(Cost best_lower_bound,
                          const ElementCostVector& best_multipliers,
                          const Solution& best_solution, bool force) {
    return false;
  }

  StrongModelView StrongTypedFullModelView() const {
    return StrongModelView(full_model_);
  }

 private:
  void ResetToIdentitySubModel();

  // Pointer to the original model
  const Model* full_model_;

  // Columns/rows sizes after filtering (size==0 <==> inactive)
  SubsetToIntVector cols_sizes_;
  ElementToIntVector rows_sizes_;

  // List of columns/rows currectly active
  std::vector<SubsetIndex> cols_focus_;
  std::vector<ElementIndex> rows_focus_;

  // Fixing data
  std::vector<FullSubsetIndex> fixed_columns_;
  Cost fixed_cost_ = .0;
};

// CoreModel stores a subset of the filtered columns and rows in an explicit
// Model object.
// The indices are compacted and mapped to the range [0, <sub-model-size>],
// effectively creating a smaller set-covering model. Similar to SubModelView,
// the core model supports column fixing and focusing on a subset of the
// original model. Mappings are maintained to translate indices back to the
// original model space.
class CoreModel : private Model {
 public:
  // Empty initialization to facilitate delayed construction
  CoreModel() = default;

  // Identity sub-model: all items are considered
  CoreModel(const Model* model);

  // Focus construction: create a sub-model with only the required items
  CoreModel(const Model* model,
            const std::vector<FullSubsetIndex>& columns_focus);

  virtual ~CoreModel() = default;

  ///////// Sub-model view interface: /////////
  BaseInt num_subsets() const { return full_model_.num_subsets(); }
  BaseInt num_elements() const { return full_model_.num_elements(); }
  BaseInt num_focus_subsets() const { return Model::num_subsets(); }
  BaseInt num_focus_elements() const { return Model::num_elements(); }
  BaseInt column_size(SubsetIndex j) const {
    DCHECK(SubsetIndex() <= j && j < SubsetIndex(num_subsets()));
    return columns()[j].size();
  }
  BaseInt row_size(ElementIndex i) const {
    DCHECK(ElementIndex() <= i && i < ElementIndex(num_elements()));
    return rows()[i].size();
  }

  FullElementIndex MapCoreToFullElementIndex(ElementIndex core_i) const {
    DCHECK(ElementIndex() <= core_i && core_i < ElementIndex(num_elements()));
    return core2full_row_map_[core_i];
  }
  ElementIndex MapFullToCoreElementIndex(FullElementIndex full_i) const {
    DCHECK(FullElementIndex() <= full_i &&
           full_i < FullElementIndex(num_elements()));
    return full2core_row_map_[full_i];
  }
  FullSubsetIndex MapCoreToFullSubsetIndex(SubsetIndex core_j) const {
    DCHECK(SubsetIndex() <= core_j && core_j < SubsetIndex(num_subsets()));
    return core2full_col_map_[core_j];
  }
  // Member function relevant for the CFT inherited from Model
  using Model::columns;
  using Model::ElementRange;
  using Model::rows;
  using Model::subset_costs;
  using Model::SubsetRange;

  ///////// Core-model interface: /////////

  // Current fixed cost: sum of the cost of the fixed columns
  Cost fixed_cost() const { return fixed_cost_; }
  // List of fixed columns.
  const std::vector<FullSubsetIndex>& fixed_columns() const {
    return fixed_columns_;
  }

  // Redefine the active items. The new sub-model will ignore all columns not in
  // focus and (optionally) the rows for which row_flags is not true. It does
  // not overwrite the current fixing.
  void SetFocus(const std::vector<FullSubsetIndex>& columns_focus);

  // Fix the provided columns, removing them for the submodel. Rows now covered
  // by fixed columns are also removed from the submodel along with non-fixed
  // columns that only cover those rows.
  virtual Cost FixMoreColumns(const std::vector<SubsetIndex>& columns_to_fix);

  virtual void ResetColumnFixing(
      const std::vector<FullSubsetIndex>& columns_to_fix,
      const DualState& state);

  // Hook function for specializations. This function can be used to define a
  // "small" core model considering a subset of the full model through the use
  // of column-generation or by only selecting columns with good reduced cost in
  // the full model.
  virtual bool UpdateCore(Cost best_lower_bound,
                          const ElementCostVector& best_multipliers,
                          const Solution& best_solution, bool force) {
    return false;
  }

  StrongModelView StrongTypedFullModelView() const { return full_model_; }

 private:
  void MarkNewFixingInMaps(const std::vector<SubsetIndex>& columns_to_fix);
  CoreToFullElementMapVector MakeOrFillBothRowMaps();
  Model MakeNewCoreModel(const CoreToFullElementMapVector& new_c2f_col_map);
  void ResetToIdentitySubModel();

  // Pointer to the original model
  StrongModelView full_model_;

  FullToCoreElementMapVector full2core_row_map_;
  CoreToFullElementMapVector core2full_row_map_;
  CoreToFullSubsetMapVector core2full_col_map_;

  // Fixing data
  Cost fixed_cost_ = .0;
  std::vector<FullSubsetIndex> fixed_columns_;

  static constexpr SubsetIndex null_subset_index =
      std::numeric_limits<SubsetIndex>::max();
  static constexpr ElementIndex null_element_index =
      std::numeric_limits<ElementIndex>::max();
  static constexpr FullSubsetIndex null_full_subset_index =
      std::numeric_limits<FullSubsetIndex>::max();
  static constexpr FullElementIndex null_full_element_index =
      std::numeric_limits<FullElementIndex>::max();
};

template <typename SubModelT>
bool ValidateSubModel(const SubModelT& model) {
  if (model.num_elements() <= 0) {
    std::cerr << "Sub-Model has no elements.\n";
    return false;
  }
  if (model.num_subsets() <= 0) {
    std::cerr << "Sub-Model has no subsets.\n";
    return false;
  }

  for (SubsetIndex j : model.SubsetRange()) {
    const auto& column = model.columns()[j];
    if (model.column_size(j) == 0) {
      std::cerr << "Column " << j << " is empty.\n";
      return false;
    }
    BaseInt j_size = std::distance(column.begin(), column.end());
    if (j_size != model.column_size(j)) {
      std::cerr << "Sub-Model size mismatch on column " << j << ", " << j_size
                << " != " << model.column_size(j) << "\n";
      return false;
    }
  }

  for (ElementIndex i : model.ElementRange()) {
    const auto& row = model.rows()[i];
    if (model.row_size(i) == 0) {
      std::cerr << "Row " << i << " is empty.\n";
      return false;
    }
    BaseInt i_size = std::distance(row.begin(), row.end());
    if (i_size != model.row_size(i)) {
      std::cerr << "Sub-Model size mismatch on row " << i << ", " << i_size
                << " != " << model.row_size(i) << "\n";
      return false;
    }
  }
  return true;
}

}  // namespace operations_research::scp
#endif /* ORTOOLS_SET_COVER_SET_COVER_SUBMODEL_H */
