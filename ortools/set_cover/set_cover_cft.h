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

////////////////////////////////////////////////////////////////////////
////////////////////////// COMMON DEFINITIONS //////////////////////////
////////////////////////////////////////////////////////////////////////
using ElementMappingVector =
    util_intops::StrongVector<ElementIndex, ElementIndex>;
using SubsetMappingVector = util_intops::StrongVector<SubsetIndex, SubsetIndex>;
using Model = SetCoverModel;

// Forward declarations, see below for the definition of the classes.
class SubModelView;
class SubModel;

// The CFT algorithm generates sub-models in two distinct ways:
//
// 1. This approach incrementally fixes specific columns into the
// solution. Once a column is fixed, it is excluded from future decisions, as it
// is already part of the solution. Additionally, rows that are covered by the
// fixed columns are removed from consideration as well, along with any columns
// that exclusively cover those rows, as they become redundant. The fixing
// process starts with the entire model and progressively fixes more columns
// until the model becomes empty. This method is well-suited for a "view-based"
// approach, which avoids duplicating data and minimizes memory usage.
//
// 2. This method creates a "core" sub-model by focusing on a
// subset of columns and optionally a subset of rows. The core model is derived
// from the original model but is significantly smaller, as it typically
// includes only a limited number of columns per row (on average, around six
// columns per row). Unlike the incremental nature of `FixingModel`, core models
// are constructed from scratch during each update. This type of small model can
// probably take advantage of the `SubModel` class, which stores the sub-modle
// explicitly in memory, avoiding looping over "inactive" columns and rows.
// Both SubModelView (lightweight but potentially slower) and SubModel (heavier
// but faster) can be used as a core model.
using CoreModel = SubModel;
// using CoreModel = SubModelView;

struct PrimalDualState;
struct Solution;

// The SubModelView abstraction provides a mechanism to interact with a subset
// of the rows and columns of a SetCoverModel, effectively creating a filtered
// view of the model. This abstraction allows operations to be performed on a
// restricted portion of the model without modifying the original data
// structure.
//
// The filtering is achieved using index lists and boolean vectors, which define
// the active rows and columns. These filters are applied dynamically, meaning
// that the views are constructed on-the-fly as needed, based on the provided
// indices and flags. This approach ensures flexibility and avoids unnecessary
// duplication of data.
//
// To optimize performance, all the underneath views handle explicitly the
// "identity view" case (where no filtering is applied, and the view represents
// the entire model). In this scenario, explicit boolean flags
// (`col_view_is_valid_` and `row_view_is_valid_`) are used to indicate that the
// views are unfiltered. This allows the compiler to optimize operations by
// treating the identity view as a special case, enabling techniques like loop
// hoisting.
class SubModelView : public IndexListSubModelView {
 public:
  using base_view = IndexListSubModelView;
  SubModelView() = default;
  SubModelView(const Model* model);
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
  Solution MakeGloabalSolution(const Solution& core_solution) const;

 private:
  void MakeIdentityColumnsView();
  void MakeIdentityRowsView();

  const Model* model_;
  SubsetToIntVector cols_sizes_;
  ElementToIntVector rows_sizes_;

  std::vector<SubsetIndex> cols_focus_;
  std::vector<ElementIndex> rows_focus_;

  std::vector<SubsetIndex> fixed_columns_;
  Cost fixed_cost_ = .0;
};

// SumModel represent a subset of the original model excplicitly storing the
// mappend indices in a SetCoverModel object.
class SubModel : Model {
 public:
  SubModel() = default;
  SubModel(const Model* model);
  SubModel(const Model* model, const std::vector<SubsetIndex>& columns_focus,
           const ElementBoolVector& rows_flags = {});
  virtual ~SubModel() = default;

  // Member function relevant for the CFT inherited from Model
  using Model::columns;
  using Model::ElementRange;
  using Model::num_elements;
  using Model::num_subsets;
  using Model::rows;
  using Model::subset_costs;
  using Model::SubsetRange;

  BaseInt num_focus_subsets() const { return num_subsets(); }
  BaseInt num_focus_elements() const { return num_elements(); }
  ElementIndex MapCoreToFullElementIndex(ElementIndex core_i) const {
    return core2full_row_map_[core_i];
  }
  ElementIndex MapFullToCoreElementIndex(ElementIndex full_i) const {
    return full2core_row_map_[full_i];
  }
  SubsetIndex MapCoreToFullSubsetIndex(SubsetIndex core_j) const {
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
  Solution MakeGloabalSolution(const Solution& core_solution) const;

 private:
  static constexpr SubsetIndex null_subset_index =
      std::numeric_limits<SubsetIndex>::max();
  static constexpr ElementIndex null_element_index =
      std::numeric_limits<ElementIndex>::max();

  const Model* model_;
  ElementMappingVector core2full_row_map_;
  ElementMappingVector full2core_row_map_;
  SubsetMappingVector core2full_col_map_;

  Cost fixed_cost_ = .0;
  std::vector<SubsetIndex> fixed_columns_;
};

class Solution {
 public:
  Solution() = default;
  template <typename SubModelT, typename SubsetsT>
  Solution(const SubModelT& model, SubsetsT&& subsets)
      : cost_(.0), subsets_(std::forward<SubsetsT>(subsets)) {
    for (SubsetIndex j : subsets_) {
      cost_ += model.subset_costs()[j];
    }
  }

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
  const CoreModel& model;
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
  virtual bool UpdateCoreModel(CoreModel&, PrimalDualState&) = 0;
  virtual ~SubgradientCBs() = default;
};

class BoundCBs : public SubgradientCBs {
 public:
  static constexpr Cost kTol = 1e-6;

  BoundCBs(const CoreModel& model);
  Cost step_size() const { return step_size_; }
  bool ExitCondition(const SubgradientContext& context) override;
  void ComputeMultipliersDelta(const SubgradientContext& context,
                               ElementCostVector& delta_mults) override;
  void RunHeuristic(const SubgradientContext& context,
                    Solution& solution) override {}
  bool UpdateCoreModel(CoreModel& core_model,
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

void SubgradientOptimization(CoreModel& core_model, SubgradientCBs& cbs,
                             PrimalDualState& best_state);

////////////////////////////////////////////////////////////////////////
/////////////////////// MULTIPLIERS BASED GREEDY ///////////////////////
////////////////////////////////////////////////////////////////////////

Solution RunMultiplierBasedGreedy(
    const CoreModel& model, const DualState& dual_state,
    Cost cost_cutoff = std::numeric_limits<BaseInt>::max());

Cost CoverGreedly(const CoreModel& model, const DualState& dual_state,
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
  bool UpdateCoreModel(CoreModel& model, PrimalDualState& state) override {
    return false;
  }

 private:
  Cost step_size_;
  BaseInt countdown_;
};

absl::StatusOr<PrimalDualState> RunThreePhase(
    CoreModel& model, const Solution& init_solution = {});

///////////////////////////////////////////////////////////////////////
//////////////////////// FULL TO CORE PRICING /////////////////////////
///////////////////////////////////////////////////////////////////////

class FullToCoreModel : public CoreModel {
  using base = CoreModel;
  struct UpdateTrigger {
    BaseInt countdown;
    BaseInt period;
    BaseInt max_period;
  };

 public:
  FullToCoreModel(const Model* full_model);
  Cost FixColumns(const std::vector<SubsetIndex>& columns_to_fix) override;
  bool UpdateCore(PrimalDualState& core_state) override;

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
