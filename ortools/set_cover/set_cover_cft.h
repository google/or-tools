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

#ifndef OR_TOOLS_ORTOOLS_SET_COVER_SET_COVER_CFT_H
#define OR_TOOLS_ORTOOLS_SET_COVER_SET_COVER_CFT_H

#include <absl/algorithm/container.h>
#include <absl/base/internal/pretty_function.h>

#include <limits>

#include "absl/status/statusor.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_model.h"
#include "ortools/set_cover/set_cover_views.h"

namespace operations_research::scp {

// Implementation of:
// Caprara, Alberto, Matteo Fischetti, and Paolo Toth. 1999. “A Heuristic
// Method for the Set Covering Problem.” Operations Research 47 (5): 730–43.
// https://www.jstor.org/stable/223097
//
// Hereafter referred to as CFT.
//
// SUMMARY
// The CFT algorithm is a heuristic approach to the set-covering problem. At its
// core, it combines a primal greedy heuristic with dual information obtained
// from the optimization of the Lagrangian relaxation of the problem.
//
// STRUCTURE
// The core of the algorithm is the 3Phase:
// 1. Subgradient optimization of the Lagrangian relaxation.
// 2. A primal greedy heuristic guided by the dual information.
// 3. Fixing some of the "best" columns (in terms of reduced costs) into the
//    solution (diving).
// + Repeat until an exit criterion is met.
//
// The paper also considers an optional outer loop, which invokes the 3Phase
// process and then fixes some columns from the current best solution. This
// introduces two levels of diving: the outer loop fixes "primal-good" columns
// (based on the best solution), while the inner loop fixes "dual-good" columns
// (based on reduced costs).
// NOTE: The outer loop is not implemented in this version (yet - April 2025).
//
// Key characteristics of the algorithm:
//
// - The CFT algorithm is tailored for instances where the number of columns is
//   significantly larger than the number of rows.
//
// - To improve efficiency, a core model approach is used. This involves
//   selecting a small subset of columns based on their reduced costs, thereby
//   substantially reducing the problem size handled in the internal steps.
//
// - Due to the use of the core model and column fixing, the algorithm rarely
//   considers the entire problem. Instead, it operates on a small "window" of
//   the problem. Efficiently managing this small window is a central aspect of
//   any CFT implementation.
//
// - The core model scheme also enables an alternative implementation where the
//   algorithm starts with a small model and progressively adds columns through
//   a column-generation procedure. While this column generation procedure is
//   problem-dependent and cannot be implemented here, the architecture of this
//   implementation is designed to be extensible, allowing for such a procedure
//   to be added in the future.
//

////////////////////////////////////////////////////////////////////////
////////////////////////// COMMON DEFINITIONS //////////////////////////
////////////////////////////////////////////////////////////////////////

// TODO(anyone): since we are working within the scp namespace, the "SetCover*"
// prefix became redundant and can be removed. For now, only redefine it
// locally.
using Model = SetCoverModel;

// When a sub-model is created, indicies are compacted to be consecutive and
// strarting from 0 (to reduce memory usage). Core ElementIndex to original
// ElementIndex mappings are stored to translate back to the original model
// space.
using ElementMappingVector =
    util_intops::StrongVector<ElementIndex, ElementIndex>;

// The same applies to SubsetIndex, which also needs to be mapped back to the
// original indexing space.
using SubsetMappingVector = util_intops::StrongVector<SubsetIndex, SubsetIndex>;

// Forward declarations, see below for the definition of the classes.
struct PrimalDualState;
struct Solution;

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
class SubModelView : public IndexListSubModelView {
  using base_view = IndexListSubModelView;

 public:
  // Empty initialization to facilitate delayed construction
  SubModelView() = default;

  // Identity sub-model: all items are considered
  SubModelView(const Model* model);

  // Focus construction: create a sub-model with only the required items
  SubModelView(const Model* model,
               const std::vector<SubsetIndex>& columns_focus,
               const ElementBoolVector& rows_flags = {});

  virtual ~SubModelView() = default;
  const std::vector<SubsetIndex>& fixed_columns() const {
    return fixed_columns_;
  }
  Cost fixed_cost() const { return fixed_cost_; }

  void SetFocus(const std::vector<SubsetIndex>& columns_focus,
                const ElementBoolVector& rows_flags = {});
  virtual Cost FixColumns(const std::vector<SubsetIndex>& columns_to_fix);
  virtual bool UpdateCore(PrimalDualState& core_state) { return false; }

 private:
  // Pointer to the original model
  const Model* full_model_;

  // Columns/rows sizes after filtering (size==0 <==> inactive)
  SubsetToIntVector cols_sizes_;
  ElementToIntVector rows_sizes_;

  // List of columns/rows currectly active
  std::vector<SubsetIndex> cols_focus_;
  std::vector<ElementIndex> rows_focus_;

  // Fixing data
  std::vector<SubsetIndex> fixed_columns_;
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
  CoreModel(const Model* model, const std::vector<SubsetIndex>& columns_focus,
            const ElementBoolVector& rows_flags = {});
  virtual ~CoreModel() = default;

  // Member function relevant for the CFT inherited from Model
  using Model::columns;
  using Model::ElementRange;
  using Model::rows;
  using Model::subset_costs;
  using Model::SubsetRange;

  const Model& full_model() const { return *full_model_; }
  BaseInt num_subsets() const { return full_model_->num_subsets(); }
  BaseInt num_elements() const { return full_model_->num_elements(); }
  BaseInt num_focus_subsets() const { return Model::num_subsets(); }
  BaseInt num_focus_elements() const { return Model::num_elements(); }
  ElementIndex MapCoreToFullElementIndex(ElementIndex core_i) const {
    DCHECK(ElementIndex() <= core_i && core_i < ElementIndex(num_elements()));
    DCHECK(core2full_row_map_[core_i] != null_element_index);
    return core2full_row_map_[core_i];
  }
  ElementIndex MapFullToCoreElementIndex(ElementIndex full_i) const {
    DCHECK(ElementIndex() <= full_i && full_i < ElementIndex(num_elements()));
    DCHECK(full2core_row_map_[full_i] != null_element_index);
    return full2core_row_map_[full_i];
  }
  SubsetIndex MapCoreToFullSubsetIndex(SubsetIndex core_j) const {
    DCHECK(SubsetIndex() <= core_j && core_j < SubsetIndex(num_subsets()));
    DCHECK(core2full_col_map_[core_j] != null_subset_index);
    return core2full_col_map_[core_j];
  }

  Cost fixed_cost() const { return fixed_cost_; }
  const std::vector<SubsetIndex>& fixed_columns() const {
    return fixed_columns_;
  }

  void SetFocus(const std::vector<SubsetIndex>& columns_focus,
                const ElementBoolVector& rows_flags = {});
  virtual Cost FixColumns(const std::vector<SubsetIndex>& columns_to_fix);
  virtual bool UpdateCore(PrimalDualState& core_state) { return false; }

 private:
  // Pointer to the original model
  const Model* full_model_;

  ElementMappingVector full2core_row_map_;
  ElementMappingVector core2full_row_map_;
  SubsetMappingVector core2full_col_map_;

  // Fixing data
  Cost fixed_cost_ = .0;
  std::vector<SubsetIndex> fixed_columns_;

  static constexpr SubsetIndex null_subset_index =
      std::numeric_limits<SubsetIndex>::max();
  static constexpr ElementIndex null_element_index =
      std::numeric_limits<ElementIndex>::max();
};

class Solution {
 public:
  Solution() = default;
  Solution(const SubModel& model, const std::vector<SubsetIndex>& core_subsets);

  double cost() const { return cost_; }
  const std::vector<SubsetIndex>& subsets() const { return subsets_; }
  std::vector<SubsetIndex>& subsets() { return subsets_; }
  void AddSubset(SubsetIndex subset, Cost cost) {
    subsets_.push_back(subset);
    cost_ += cost;
  }
  bool Empty() const { return subsets_.empty(); }
  void Clear() {
    cost_ = 0.0;
    subsets_.clear();
  }

 private:
  Cost cost_;
  std::vector<SubsetIndex> subsets_;
};

template <typename SubModelT>
Cost ComputeReducedCosts(const SubModelT& model,
                         const ElementCostVector& multipliers,
                         SubsetCostVector& reduced_costs) {
  // Compute new reduced costs (O(nnz))
  Cost negative_sum = .0;
  for (SubsetIndex j : model.SubsetRange()) {
    reduced_costs[j] = model.subset_costs()[j];
    for (ElementIndex i : model.columns()[j]) {
      reduced_costs[j] -= multipliers[i];
    }
    if (reduced_costs[j] < .0) {
      negative_sum += reduced_costs[j];
    }
  }
  return negative_sum;
}

class DualState {
 public:
  template <typename SubModelT>
  DualState(const SubModelT& model)
      : lower_bound_(),
        multipliers_(model.num_elements(), std::numeric_limits<Cost>::max()),
        reduced_costs_() {
    DualUpdate(model, [&](ElementIndex i, Cost& i_multiplier) {
      for (SubsetIndex j : model.rows()[i]) {
        Cost candidate = model.subset_costs()[j] / model.columns()[j].size();
        i_multiplier = std::min(i_multiplier, candidate);
      }
    });
  }

  Cost lower_bound() const { return lower_bound_; }
  const ElementCostVector& multipliers() const { return multipliers_; }
  const SubsetCostVector& reduced_costs() const { return reduced_costs_; }

  // NOTE: This function contains one of the two O(nnz) subgradient steps
  template <typename SubModelT, typename Op>
  void DualUpdate(const SubModelT& model, Op multiplier_operator) {
    multipliers_.resize(model.num_elements());
    reduced_costs_.resize(model.num_subsets());
    lower_bound_ = .0;
    // Update multipliers
    for (ElementIndex i : model.ElementRange()) {
      multiplier_operator(i, multipliers_[i]);
      lower_bound_ += multipliers_[i];
      DCHECK_GE(multipliers_[i], .0);
    }
    lower_bound_ += ComputeReducedCosts(model, multipliers_, reduced_costs_);
  }

 private:
  Cost lower_bound_;
  ElementCostVector multipliers_;
  SubsetCostVector reduced_costs_;
};

struct PrimalDualState {
  Solution solution;
  DualState dual_state;
};

// In the narrow scope of the CFT subgradient, there are often divisions
// between non-negative quantities (e.g., to compute a relative gap). In these
// specific cases, the denominator should always be greater than the
// numerator. This function checks that.
inline Cost DivideIfGE0(Cost numerator, Cost denominator) {
  DCHECK_GE(numerator, -1e-6);
  if (numerator < 1e-6) {
    return 0.0;
  }
  return numerator / denominator;
}

///////////////////////////////////////////////////////////////////////
///////////////////////////// SUBGRADIENT /////////////////////////////
///////////////////////////////////////////////////////////////////////

struct SubgradientContext {
  const SubModel& model;
  const DualState& current_dual_state;
  const DualState& best_dual_state;
  const Solution& best_solution;
  const ElementCostVector& subgradient;
};

class SubgradientCBs {
 public:
  virtual bool ExitCondition(const SubgradientContext&) = 0;
  virtual void RunHeuristic(const SubgradientContext&, Solution&) = 0;
  virtual void ComputeMultipliersDelta(const SubgradientContext&,
                                       ElementCostVector& delta_mults) = 0;
  virtual bool UpdateCoreModel(SubModel&, PrimalDualState&) = 0;
  virtual ~SubgradientCBs() = default;
};

class BoundCBs : public SubgradientCBs {
 public:
  static constexpr Cost kTol = 1e-6;

  BoundCBs(const SubModel& model);
  Cost step_size() const { return step_size_; }
  bool ExitCondition(const SubgradientContext& context) override;
  void ComputeMultipliersDelta(const SubgradientContext& context,
                               ElementCostVector& delta_mults) override;
  void RunHeuristic(const SubgradientContext& context,
                    Solution& solution) override {}
  bool UpdateCoreModel(SubModel& core_model,
                       PrimalDualState& best_state) override;

 private:
  void MakeMinimalCoverageSubgradient(const SubgradientContext& context,
                                      ElementCostVector& subgradient);

 private:
  Cost squared_norm_;
  ElementCostVector direction_;  // stabilized subgradient
  std::vector<SubsetIndex> lagrangian_solution_;

  // Stopping condition
  Cost prev_best_lb_;
  BaseInt max_iter_countdown_;
  BaseInt exit_test_countdown_;
  BaseInt exit_test_period_;
  BaseInt last_core_update_countdown_;

  // Step size
  void UpdateStepSize(Cost lower_bound);
  Cost step_size_;
  Cost last_min_lb_seen_;
  Cost last_max_lb_seen_;
  BaseInt step_size_update_countdown_;
  BaseInt step_size_update_period_;
};

void SubgradientOptimization(SubModel& core_model, SubgradientCBs& cbs,
                             PrimalDualState& best_state);

////////////////////////////////////////////////////////////////////////
/////////////////////// MULTIPLIERS BASED GREEDY ///////////////////////
////////////////////////////////////////////////////////////////////////

Solution RunMultiplierBasedGreedy(
    const SubModel& model, const DualState& dual_state,
    Cost cost_cutoff = std::numeric_limits<BaseInt>::max());

Cost CoverGreedly(const SubModel& model, const DualState& dual_state,
                  Cost cost_cutoff, BaseInt size_cutoff,
                  std::vector<SubsetIndex>& sol_subsets);

///////////////////////////////////////////////////////////////////////
//////////////////////// THREE PHASE ALGORITHM ////////////////////////
///////////////////////////////////////////////////////////////////////

class HeuristicCBs : public SubgradientCBs {
 public:
  HeuristicCBs() : step_size_(0.1), countdown_(250) {};
  void set_step_size(Cost step_size) { step_size_ = step_size; }
  bool ExitCondition(const SubgradientContext& context) override {
    Cost upper_bound =
        context.best_solution.cost() - context.model.fixed_cost();
    Cost lower_bound = context.best_dual_state.lower_bound();
    return upper_bound - .999 < lower_bound || --countdown_ <= 0;
  }
  void RunHeuristic(const SubgradientContext& context,
                    Solution& solution) override;
  void ComputeMultipliersDelta(const SubgradientContext& context,
                               ElementCostVector& delta_mults) override;
  bool UpdateCoreModel(SubModel& model, PrimalDualState& state) override {
    return false;
  }

 private:
  Cost step_size_;
  BaseInt countdown_;
};

absl::StatusOr<PrimalDualState> RunThreePhase(
    SubModel& model, const Solution& init_solution = {});

///////////////////////////////////////////////////////////////////////
//////////////////////// FULL TO CORE PRICING /////////////////////////
///////////////////////////////////////////////////////////////////////

class FullToCoreModel : public SubModel {
  using base = SubModel;
  struct UpdateTrigger {
    BaseInt countdown;
    BaseInt period;
    BaseInt max_period;
  };

 public:
  FullToCoreModel(const Model* full_model);
  Cost FixColumns(const std::vector<SubsetIndex>& columns_to_fix) override;
  bool UpdateCore(PrimalDualState& core_state) override;
  void ResetPricingPeriod();

 private:
  void UpdatePricingPeriod(const DualState& full_dual_state,
                           const PrimalDualState& core_state);

  FilteredSubModelView FullModelWithFixingView() const {
    return FilteredSubModelView(full_model_, &cols_sizes_, &rows_sizes_,
                                num_subsets_, num_elements_);
  }

  const Model* full_model_;
  SubsetToIntVector cols_sizes_;
  ElementToIntVector rows_sizes_;
  BaseInt num_subsets_;
  BaseInt num_elements_;

  DualState full_dual_state_;

  BaseInt update_countdown_;
  BaseInt update_period_;
  BaseInt update_max_period_;
};

}  // namespace operations_research::scp

#endif /* OR_TOOLS_ORTOOLS_SET_COVER_SET_COVER_CFT_H */
