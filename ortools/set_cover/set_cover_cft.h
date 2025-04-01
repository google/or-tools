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

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_model.h"
#include "ortools/set_cover/set_cover_views.h"

namespace operations_research::scp {

using Model = SetCoverModel;

////////////////////////////////////////////////////////////////////////
////////////////////////// COMMON DEFINITIONS //////////////////////////
////////////////////////////////////////////////////////////////////////

class Solution {
 public:
  Solution() = default;
  template <typename SubsetsT>
  Solution(const Model& model, SubsetsT&& subsets)
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

Cost ComputeReducedCosts(const Model& model,
                         const ElementCostVector& multipliers,
                         SubsetCostVector& reduced_costs);

class DualState {
 public:
  DualState(const Model& model);
  Cost lower_bound() const { return lower_bound_; }
  const ElementCostVector& multipliers() const { return multipliers_; }
  const SubsetCostVector& reduced_costs() const { return reduced_costs_; }

  // NOTE: This function contains one of the two O(nnz) subgradient steps
  template <typename Op>
  void DualUpdate(const Model& model, Op multiplier_operator) {
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
class SubModelView {
 public:
  SubModelView(const Model& model) { RestoreFullModel(model); }

  virtual ~SubModelView() = default;

  auto subset_costs() const
      -> IndexListView<SubsetCostVector, std::vector<SubsetIndex>> {
    return IndexListView(model_->subset_costs(), &columns_focus_);
  }
  auto columns() const
      -> SparseFilteredView<SparseColumnView, std::vector<SubsetIndex>,
                            ElementBoolVector> {
    return SparseFilteredView(model_->columns(), columns_focus(), rows_flags());
  }
  auto rows() const
      -> SparseFilteredView<SparseRowView, std::vector<ElementIndex>,
                            SubsetBoolVector> {
    return SparseFilteredView(model_->rows(), rows_focus(), columns_flags());
  }
  auto SubsetRange() const
      -> IndexListView<IntRange<SubsetIndex>, std::vector<SubsetIndex>> {
    return IndexListView(all_columns_range_, columns_focus());
  }
  auto ElementRange() const
      -> IndexListView<IntRange<ElementIndex>, std::vector<ElementIndex>> {
    return IndexListView(all_rows_range_, rows_focus());
  }

  virtual void RestoreFullModel(const Model& model);
  virtual Cost FixColumns(const std::vector<SubsetIndex>& columns_to_fix);
  virtual bool UpdateCore(PrimalDualState& state) { return false; }

 private:
  const std::vector<SubsetIndex>* columns_focus() const {
    return col_view_is_valid_ ? &columns_focus_ : nullptr;
  };
  const std::vector<ElementIndex>* rows_focus() const {
    return row_view_is_valid_ ? &rows_focus_ : nullptr;
  };
  const SubsetBoolVector* columns_flags() const {
    return col_view_is_valid_ ? &columns_flags_ : nullptr;
  };
  const ElementBoolVector* rows_flags() const {
    return row_view_is_valid_ ? &rows_flags_ : nullptr;
  };

  const Model* model_;
  bool col_view_is_valid_;
  bool row_view_is_valid_;
  IntRange<SubsetIndex> all_columns_range_;
  IntRange<ElementIndex> all_rows_range_;
  std::vector<SubsetIndex> columns_focus_;
  std::vector<ElementIndex> rows_focus_;
  SubsetBoolVector columns_flags_;
  ElementBoolVector rows_flags_;

  std::vector<SubsetIndex> fixed_columns_;
  Cost fixed_cost_;
};

// In the narrow scope of the CFT subgradient, there are often divisions
// between non-negative quantities (e.g., to compute a relative gap). In these
// specific cases, the denominator should always be greater than the
// numerator. This function checks that.
inline Cost DivideIfGE0(Cost numerator, Cost denominator) {
  DCHECK_GE(numerator, .0);
  if (numerator < 1e-6) {
    return 0.0;
  }
  return numerator / denominator;
}

absl::Status ValidateModel(Model& model);
absl::Status ValidateFeasibleSolution(const Model& model,
                                      const Solution& solution,
                                      Cost tolerance = 1e-6);

///////////////////////////////////////////////////////////////////////
///////////////////////////// SUBGRADIENT /////////////////////////////
///////////////////////////////////////////////////////////////////////

struct SubgradientContext {
  const Model& model;
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
  virtual bool UpdateCoreModel(SubModelView&, PrimalDualState&) = 0;
  virtual ~SubgradientCBs() = default;
};

class BoundCBs : public SubgradientCBs {
 public:
  static constexpr Cost kTol = 1e-6;

  BoundCBs(const Model& model);
  Cost step_size() const { return step_size_; }
  bool ExitCondition(const SubgradientContext& context) override;
  void ComputeMultipliersDelta(const SubgradientContext& context,
                               ElementCostVector& delta_mults) override;
  void RunHeuristic(const SubgradientContext& context,
                    Solution& solution) override {
    solution.Clear();
  }
  bool UpdateCoreModel(SubModelView& core_model,
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

  // Step size
  void UpdateStepSize(Cost lower_bound);
  Cost step_size_;
  Cost last_min_lb_seen_;
  Cost last_max_lb_seen_;
  BaseInt step_size_update_countdown_;
  BaseInt step_size_update_period_;
};

void SubgradientOptimization(SubModelView& core_model, SubgradientCBs& cbs,
                             PrimalDualState& best_state);

////////////////////////////////////////////////////////////////////////
/////////////////////// MULTIPLIERS BASED GREEDY ///////////////////////
////////////////////////////////////////////////////////////////////////

Solution RunMultiplierBasedGreedy(
    const Model& model, const DualState& dual_state,
    Cost cost_cutoff = std::numeric_limits<BaseInt>::max());

Cost CoverGreedly(const Model& model, const DualState& dual_state,
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
    return --countdown_ <= 0;
  }
  void RunHeuristic(const SubgradientContext& context,
                    Solution& solution) override;
  void ComputeMultipliersDelta(const SubgradientContext& context,
                               ElementCostVector& delta_mults) override;
  bool UpdateCoreModel(SubModelView& model, PrimalDualState& state) override {
    return false;
  }

 private:
  Cost step_size_;
  BaseInt countdown_;
};

absl::StatusOr<PrimalDualState> RunThreePhase(
    SubModelView& model, const Solution& init_solution = {});

///////////////////////////////////////////////////////////////////////
//////////////////////// FULL TO CORE PRICING /////////////////////////
///////////////////////////////////////////////////////////////////////

class FullToCoreModel : public SubModelView {
  struct UpdateTrigger {
    BaseInt countdown;
    BaseInt period;
    BaseInt max_period;
  };

 public:
  FullToCoreModel(Model&& full_model);

  template <typename... Args>
  FullToCoreModel(Args&&... args)
      : FullToCoreModel(Model(std::forward<Args>(args)...)) {}

  bool UpdateCore(PrimalDualState& core_state) override;

 private:
  void UpdatePricingPeriod(const DualState& full_dual_state,
                           const PrimalDualState& core_state);

  SubsetMapVector columns_map_;
  Model full_model_;
  DualState full_dual_state_;

  BaseInt update_countdown_;
  BaseInt update_period_;
  BaseInt update_max_period_;
};

}  // namespace operations_research::scp

#endif /* OR_TOOLS_ORTOOLS_SET_COVER_SET_COVER_CFT_H */
