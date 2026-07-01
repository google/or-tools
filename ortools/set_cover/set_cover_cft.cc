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

#include "ortools/set_cover/set_cover_cft.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <random>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_cat.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/strong_vector.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_heuristics.h"
#include "ortools/set_cover/set_cover_invariant.h"
#include "ortools/set_cover/set_cover_model.h"
#include "ortools/set_cover/set_cover_submodel.h"
#include "ortools/set_cover/set_cover_views.h"

namespace operations_research {
namespace {

// Verifies that the given subsets are valid for the core model.
bool CheckSubsets(const CoreModel& model,
                  const std::vector<SubsetIndex>& core_subsets) {
  return CheckIndices(core_subsets, SubsetIndex(model.num_focus_subsets()));
}

}  // namespace

SubmodelSolution::SubmodelSolution(const CoreModel& model,
                                   const std::vector<SubsetIndex>& core_subsets)
    : cost_(), subsets_() {
  DCHECK_GE(model.num_subsets(), 0);
  DCHECK(CheckSubsets(model, core_subsets));

  subsets_.reserve(core_subsets.size() + model.fixed_columns().size());
  cost_ = model.fixed_cost();
  for (const FullModelSubsetIndex full_j : model.fixed_columns()) {
    subsets_.push_back(full_j);
  }
  for (const SubsetIndex core_j : core_subsets) {
    const FullModelSubsetIndex full_j = model.MapCoreToFullSubsetIndex(core_j);
    AddSubset(full_j, model.subset_costs()[core_j]);
  }

  DCHECK_GE(cost_, model.fixed_cost());
}

// Common definitions
namespace {

class ScopedDurationAccumulator {
 public:
  explicit ScopedDurationAccumulator(absl::Duration& total_duration)
      : total_duration_(total_duration), start_(absl::Now()) {}
  ~ScopedDurationAccumulator() {
    total_duration_.get() += absl::Now() - start_;
  }

 private:
  std::reference_wrapper<absl::Duration> total_duration_;
  absl::Time start_;
};

#ifdef CFT_MEASURE_TIME
#define CFT_MEASURE_SCOPE_DURATION(duration_ref) \
  ScopedDurationAccumulator pause_timer(duration_ref)
#else
#define CFT_MEASURE_SCOPE_DURATION(duration_ref)
#endif

// CoverCounters.
// TODO(user): see how this can be replaced by SetCoverInvariant.

template <typename IndexT = ElementIndex>
class CoverCountersImpl {
 public:
  explicit CoverCountersImpl(BaseInt nelems = 0) : cov_counters_(nelems, 0) {
    DCHECK_GE(nelems, 0);
  }
  void Reset(BaseInt nelems) {
    DCHECK_GE(nelems, 0);
    cov_counters_.assign(nelems, 0);
    DCHECK_EQ(cov_counters_.size(), nelems);
  }
  BaseInt Size() const {
    const BaseInt result = cov_counters_.size();
    DCHECK_GE(result, 0);
    return result;
  }
  BaseInt operator[](IndexT i) const {
    DCHECK_LE(IndexT(0), i);
    DCHECK_LT(i, IndexT(cov_counters_.size()));
    const BaseInt result = cov_counters_[i];
    DCHECK_GE(result, 0);
    return result;
  }

  template <typename IterableT>
  BaseInt Cover(const IterableT& subset) {
    const BaseInt old_size = cov_counters_.size();
    BaseInt covered = 0;
    for (const IndexT i : subset) {
      DCHECK(IndexT(0) <= i && i < IndexT(old_size));
      covered += cov_counters_[i] == 0 ? 1ULL : 0ULL;
      cov_counters_[i]++;
    }
    DCHECK_EQ(cov_counters_.size(), old_size);
    return covered;
  }

  template <typename IterableT>
  BaseInt Uncover(const IterableT& subset) {
    const BaseInt old_size = cov_counters_.size();
    BaseInt uncovered = 0;
    for (const IndexT i : subset) {
      DCHECK(IndexT(0) <= i && i < IndexT(old_size));
      DCHECK_GT(cov_counters_[i], 0);
      --cov_counters_[i];
      uncovered += cov_counters_[i] == 0 ? 1ULL : 0ULL;
    }
    DCHECK_EQ(cov_counters_.size(), old_size);
    return uncovered;
  }

  // Check if all the elements of a subset are already covered.
  template <typename IterableT>
  bool IsRedundantCover(IterableT const& subset) const {
    return absl::c_all_of(subset,
                          [&](IndexT i) { return cov_counters_[i] > 0; });
  }

  // Check if all the elements would still be covered if the subset were
  // removed.
  template <typename IterableT>
  bool IsRedundantUncover(IterableT const& subset) const {
    return absl::c_all_of(subset,
                          [&](IndexT i) { return cov_counters_[i] > 1; });
  }

 private:
  util_intops::StrongVector<IndexT, BaseInt> cov_counters_;
};

using CoverCounters = CoverCountersImpl<ElementIndex>;
using FullCoverCounters = CoverCountersImpl<FullModelElementIndex>;

// In the narrow scope of the CFT subgradient, there are often divisions
// between non-negative quantities (e.g., to compute a relative gap). In these
// specific cases, the denominator should always be greater than the
// numerator. This function checks that.
inline Cost DivideIfNonNegative(Cost numerator, Cost denominator) {
  const Cost kTol = CftParameters::kDivisionTolerance;
  DCHECK_GE(denominator, numerator);
  DCHECK_GE(denominator, -kTol);
  const Cost result = numerator < kTol ? 0.0 : numerator / denominator;
  DCHECK_GE(result, 0.0);
  return result;
}

// Subgradient

// Utility aggregate used by the SubgradientOptimization procedure to
// pass the needed information to the SubgradientCallback interface.
// Context structure containing parameters and states passed to
// SubgradientCallbacks.
struct SubgradientContext {
  // The CoreModel under optimization.
  const CoreModel& model;

  // The current Lagrangian dual state.
  const DualState& current_dual_state;

  // TODO(user): Avoid copying unused reduced cost during subgradient.
  // The best lower bound value found so far.
  const Cost& best_lower_bound;

  // The best multiplier vector found so far.
  const ElementCostVector& best_multipliers;

  // The best primal solution found so far.
  const SubmodelSolution& best_solution;

  // The current computed subgradient vector.
  const ElementCostVector& subgradient;

  // Mutable execution stats structure.
  CftExecutionStats& stats;
};

// Abstract callback class representing hooks for subgradient optimization
// stages.
class SubgradientCallback {
 public:
  // Returns true if subgradient loop should terminate based on context.
  virtual bool ExitCondition(const SubgradientContext&) = 0;

  // Runs a heuristic to compute a primal solution at the current iteration.
  virtual void RunHeuristic(const SubgradientContext&, SubmodelSolution&) = 0;

  // Computes the step increment vector for dual multipliers.
  virtual void ComputeMultipliersDelta(const SubgradientContext&,
                                       ElementCostVector& delta_mults) = 0;

  // Interface method to dynamically update the active core model size or
  // columns.
  virtual bool UpdateCoreModel(SubgradientContext context,
                               CoreModel& core_model, bool force) = 0;

  // Virtual destructor.
  virtual ~SubgradientCallback() = default;
};

// Subgradient callbacks implementation focused on improving the current best
// dual bound.
class BoundCallback : public SubgradientCallback {
 public:
  // Tolerance limit value.
  static constexpr Cost kTol = 1e-6;

  // Constructor.
  explicit BoundCallback(const CoreModel& model,
                         const CftParameters& params = CftParameters());

  // Returns the step size parameter value.
  Cost step_size() const {
    Cost result = step_size_;
    DCHECK_GE(result, 0.0);
    return result;
  }

  // Bound optimization exit condition test.
  bool ExitCondition(const SubgradientContext& context) override;

  // Computes multipliers increment delta.
  void ComputeMultipliersDelta(const SubgradientContext& context,
                               ElementCostVector& delta_mults) override;

  // Heuristic hook (no-op for bound callback).
  void RunHeuristic(const SubgradientContext& context,
                    SubmodelSolution& solution) override {
    (void)context;
    (void)solution;
  }

  // Core model update helper.
  bool UpdateCoreModel(SubgradientContext context, CoreModel& core_model,
                       bool force) override;

 private:
  // Internal helper to compute minimal subgradient coverage.
  void MakeMinimalCoverageSubgradient(const SubgradientContext& context,
                                      ElementCostVector& subgradient);

 private:
  // Squared norm metric.
  Cost squared_norm_;

  // The search direction vector.
  ElementCostVector direction_;

  // Lagrangian solution column selections.
  std::vector<SubsetIndex> lagrangian_solution_;

  // The previous iteration's best lower bound. Used as a stop condition.
  Cost previous_best_lower_bound_;

  // Remaining countdown of iterations.
  BaseInt max_iter_countdown_;

  // Remaining countdown of exit checks.
  BaseInt exit_test_countdown_;

  // The period between exit tests.
  BaseInt exit_test_period_;

  // Run iteration extension count when unfixed.
  BaseInt unfixed_run_extension_;

  // Updates the step size.
  void UpdateStepSize(SubgradientContext context);

  // The current subgradient step size.
  Cost step_size_;

  // The minimum lower bound seen recently.
  Cost last_min_lower_bound_seen_;

  // The maximum lower bound seen recently.
  Cost last_max_lower_bound_seen_;

  // Countdown before next step size update.
  BaseInt step_size_update_countdown_;

  // Update period for step size adaptation.
  BaseInt step_size_update_period_;

  // Parameters configuring CFT heuristics.
  CftParameters params_;
};

// Three-phase algorithm.
//
// Subgradient callbacks implementation focused on wandering near the optimal
// multipliers and invoking the multiplier-based greedy heuristic at each
// iteration.
class HeuristicCallback : public SubgradientCallback {
 public:
  // Default constructor.
  explicit HeuristicCallback(const CftParameters& params = CftParameters())
      : step_size_(0.1), countdown_(250), params_(params) {}

  // Sets the subgradient step size.
  void set_step_size(Cost step_size) {
    DCHECK_GE(step_size, 0.0);
    step_size_ = step_size;
  }

  // Exit condition logic based on upper/lower bound gap.
  bool ExitCondition(const SubgradientContext& context) override {
    DCHECK_GE(context.model.num_subsets(), 0);
    Cost upper_bound =
        context.best_solution.cost() - context.model.fixed_cost();
    Cost lower_bound = context.best_lower_bound;
    return upper_bound - params_.bound_min_diff_distance < lower_bound ||
           --countdown_ <= 0;
  }

  // Invokes the multiplier-based greedy heuristic.
  void RunHeuristic(const SubgradientContext& context,
                    SubmodelSolution& solution) override;

  // Computes subgradient-directed delta multipliers.
  void ComputeMultipliersDelta(const SubgradientContext& context,
                               ElementCostVector& delta_mults) override;

  // No-op core model update override.
  bool UpdateCoreModel(SubgradientContext context, CoreModel& core_model,
                       bool force) override {
    (void)context;
    (void)core_model;
    (void)force;
    return false;
  }

 private:
  // The step size used during subgradient updates.
  Cost step_size_;

  // Remaining countdown of iterations.
  BaseInt countdown_;

  // Parameters configuring CFT heuristics.
  CftParameters params_;
};

// Full to Core Model.

// CoreModel extractor. Stores a pointer to the full model and specialized
// UpdateCore in such a way to update the CoreModel (stored as base class) and
// focus the search on a small window of the full model.
class PricingModel : public CoreModel {
  // Trigger variables to control update frequency.
  struct UpdateTrigger {
    // Countdown value.
    BaseInt countdown;

    // Update period.
    BaseInt period;

    // Maximum update period.
    BaseInt max_period;
  };

  // This class handles the logic for selecting columns based on their reduced
  // costs and the number of rows they cover. While this implementation is more
  // complex than what would typically be required for static `SetCoverModel`s,
  // it is designed to efficiently handle dynamically updated models where new
  // columns are generated over time. In this case, recomputing the row view
  // from scratch each time would introduce significant overhead. To avoid this,
  // the column selection logic operates solely on the column view, without
  // relying on the row view.
  //
  // NOTE: A cleaner alternative would involve modifying the `SetCoverModel`
  // implementation to support incremental updates to the row view as new
  // columns are added. This approach would reduce overhead while enabling a
  // simpler and more efficient column selection process.
  // TODO(user): implement the above NOTE.
  class ColumnSelector {
   public:
    // Computes subset of columns to be included in the core model.
    const std::vector<FullModelSubsetIndex>& ComputeNewSelection(
        MaskedModelView full_model,
        const std::vector<FullModelSubsetIndex>& forced_columns,
        const SubsetCostVector& reduced_costs);

   private:
    // Helper to select a column.
    bool SelectColumn(MaskedModelView full_model, SubsetIndex j);

    // Finds columns with minimum reduced cost.
    void SelectMinReducedCostColumns(MaskedModelView full_model,
                                     const SubsetCostVector& reduced_costs);

    // Finds columns covering rows that have low reduced cost.
    void SelectMinReducedCostByRow(MaskedModelView full_model,
                                   const SubsetCostVector& reduced_costs);

   private:
    // Indices of candidate columns.
    std::vector<SubsetIndex> candidates_;

    // Iterator tracking the first unselected candidate.
    std::vector<SubsetIndex>::const_iterator first_unselected_;

    // Number of times each row is covered.
    ElementToIntVector row_cover_counts_;

    // Count of rows remaining to be covered.
    BaseInt rows_left_to_cover_;

    // Computed subset selection.
    std::vector<FullModelSubsetIndex> selection_;

    // Boolean vector tracking chosen subsets.
    SubsetBoolVector selected_;
  };

 public:
  // Constructor.
  explicit PricingModel(const SetCoverModel& full_model);

  // Fixes additional columns to be part of the final solution.
  Cost FixMoreColumns(const std::vector<SubsetIndex>& columns_to_fix) override;

  // Resets the column fixing state.
  void ResetColumnFixing(
      const std::vector<FullModelSubsetIndex>& columns_to_fix,
      const DualState& state) override;

  // Updates core model based on best lower bound & multipliers.
  bool UpdateCore(Cost best_lower_bound,
                  const ElementCostVector& best_multipliers,
                  const SubmodelSolution& best_solution, bool force) override;

  // Resets the pricing update trigger period.
  void ResetPricingPeriod();

  // Returns the best dual state.
  const DualState& best_dual_state() const { return best_dual_state_; }

  // Verifies submodel invariants.
  bool SubmodelInvariantCheck();

 protected:
  // Returns true if it is time to perform pricing update.
  bool IsTimeToUpdate(Cost best_lower_bound, bool force);

  // Returns true if column with index j is focused.
  decltype(auto) IsColumnInFocus(FullModelSubsetIndex j) {
    DCHECK_GE(j, FullModelSubsetIndex(0));
    DCHECK_LT(j, FullModelSubsetIndex(is_column_in_focus_.size()));
    return is_column_in_focus_[static_cast<SubsetIndex>(j)];
  }

  // Returns true if row with index i is focused.
  decltype(auto) IsRowInFocus(FullModelElementIndex i) {
    DCHECK_GE(i, FullModelElementIndex(0));
    DCHECK_LT(i, FullModelElementIndex(is_row_in_focus_.size()));
    return is_row_in_focus_[static_cast<ElementIndex>(i)];
  }

  // Updates pricing trigger period dynamically.
  void UpdatePricingPeriod(const DualState& full_dual_state,
                           Cost core_lower_bound, Cost core_upper_bound);

  // Updates and transfers core multipliers to full model space.
  Cost UpdateMultipliers(const ElementCostVector& core_multipliers);

  // Computes and updates focus masks for rows/columns.
  void ComputeAndSetFocus(Cost best_lower_bound,
                          const SubmodelSolution& best_solution);

  // Access the full model filtered by the current columns fixed.
  MaskedModelView FixingFullModelView() const {
    return MaskedModelView(full_model_.get(), is_column_in_focus_,
                           is_row_in_focus_, full_model_.get().num_subsets(),
                           full_model_.get().num_elements());
  }

  // Access the full model with the strongly typed view.
  FullModelView StrongTypedFullModelView() const {
    return FullModelView(full_model_.get());
  }

  // Selects new columns for the core model.
  std::vector<FullModelSubsetIndex> SelectNewCoreColumns(
      const std::vector<FullModelSubsetIndex>& forced_columns = {});

 private:
  // Reference wrapper for the full model.
  std::reference_wrapper<const SetCoverModel> full_model_;

  SubsetBoolVector is_column_in_focus_;

  ElementBoolVector is_row_in_focus_;

  // Covers per row target count.
  BaseInt selection_coefficient_ = CftParameters::kMinimumCoverage;

  // The best bound value from the previous iteration.
  Cost prev_best_lower_bound_;

  // Full model dual state.
  DualState full_dual_state_;

  // Best dual state.
  DualState best_dual_state_;

  // Pricing model update countdown timer.
  BaseInt update_countdown_;

  // Pricing model update period.
  BaseInt update_period_;

  // Pricing model maximum update period.
  BaseInt update_max_period_;

  // Selector object to compute core columns.
  ColumnSelector column_selector_;  // Here to avoid repeated allocations.
};

PrimalDualState RunThreePhase(CoreModel& model,
                              const SubmodelSolution& init_solution,
                              const CftParameters& params,
                              CftExecutionStats& stats);

BoundCallback::BoundCallback(const CoreModel& model,
                             const CftParameters& params)
    : squared_norm_(static_cast<Cost>(model.num_elements())),
      direction_(ElementCostVector(model.num_elements(), 0.0)),
      previous_best_lower_bound_(std::numeric_limits<Cost>::lowest()),
      max_iter_countdown_(params.max_iter_multiplier *
                          model.num_focus_elements()),
      exit_test_countdown_(params.exit_test_period),
      exit_test_period_(params.exit_test_period),
      unfixed_run_extension_(0),
      step_size_(params.initial_step_size),
      last_min_lower_bound_seen_(std::numeric_limits<Cost>::max()),
      last_max_lower_bound_seen_(0.0),
      step_size_update_countdown_(params.step_size_update_period),
      step_size_update_period_(params.step_size_update_period),
      params_(params) {
  DCHECK_GE(model.num_elements(), 0);
  DCHECK_GE(step_size_, 0.0);
}

bool BoundCallback::ExitCondition(const SubgradientContext& context) {
  DCHECK_GE(context.model.num_subsets(), 0);
  const Cost best_lb = context.best_lower_bound;
  const Cost best_ub =
      context.best_solution.cost() - context.model.fixed_cost();
  if (--max_iter_countdown_ <= 0 || squared_norm_ <= kTol) {
    return true;
  }
  if (--exit_test_countdown_ > 0) {
    return false;
  }
  if (previous_best_lower_bound_ >= best_ub - params_.bound_min_diff_distance) {
    return true;
  }
  if (previous_best_lower_bound_ == std::numeric_limits<Cost>::lowest()) {
    previous_best_lower_bound_ = best_lb;
    exit_test_countdown_ = exit_test_period_;
    return false;
  }
  exit_test_countdown_ = exit_test_period_;
  const Cost abs_improvement = best_lb - previous_best_lower_bound_;
  const Cost relative_improvement =
      DivideIfNonNegative(abs_improvement, best_lb);
  previous_best_lower_bound_ = best_lb;

  // TODO(user): Make this relative to the best solution cost?
  // TODO(user): this should be a constant.
  if (abs_improvement >= 1.0 || relative_improvement >= 0.001) {
    return false;
  }

  // (Not in [1]): During the first unfixed iteration we want to converge closer
  // to the optimum
  VLOG(3) << "[SUBG] First iteration extension " << unfixed_run_extension_;
  const BaseInt extension = (context.model.fixed_columns().empty() ? 2 : 1);
  const bool result = unfixed_run_extension_++ >= extension;
  return result;
}

void BoundCallback::ComputeMultipliersDelta(const SubgradientContext& context,
                                            ElementCostVector& delta_mults) {
  DCHECK_GE(context.model.num_subsets(), 0);
  DCHECK_EQ(delta_mults.size(), context.model.num_elements());

  direction_ = context.subgradient;
  const BaseInt extension = (context.model.fixed_columns().empty() ? 2 : 1);
  if (unfixed_run_extension_ < extension) {
    MakeMinimalCoverageSubgradient(context, direction_);
  }

  squared_norm_ = 0.0;
  for (const ElementIndex i : context.model.ElementRange()) {
    const Cost curr_i_mult = context.current_dual_state.multipliers()[i];
    if ((curr_i_mult <= 0.0 && direction_[i] < 0.0) ||
        (curr_i_mult > params_.max_multiplier && direction_[i] > 0.0)) {
      direction_[i] = 0;
    }

    squared_norm_ += direction_[i] * direction_[i];
  }

  if (squared_norm_ <= kTol) {
    delta_mults.assign(context.model.num_elements(), 0.0);
    return;
  }

  UpdateStepSize(context);
  const Cost upper_bound =
      context.best_solution.cost() - context.model.fixed_cost();
  const Cost lower_bound = context.current_dual_state.lower_bound();
  const Cost delta = upper_bound - lower_bound;
  const Cost step_constant = (step_size_ * delta) / squared_norm_;

  for (const ElementIndex i : context.model.ElementRange()) {
    delta_mults[i] = step_constant * direction_[i];
    DCHECK(std::isfinite(delta_mults[i]));
  }
}

void BoundCallback::UpdateStepSize(SubgradientContext context) {
  DCHECK_GE(context.model.num_subsets(), 0);
  const Cost lower_bound = context.current_dual_state.lower_bound();
  last_min_lower_bound_seen_ =
      std::min(last_min_lower_bound_seen_, lower_bound);
  last_max_lower_bound_seen_ =
      std::max(last_max_lower_bound_seen_, lower_bound);

  if (--step_size_update_countdown_ <= 0) {
    step_size_update_countdown_ = step_size_update_period_;

    const Cost delta = last_max_lower_bound_seen_ - last_min_lower_bound_seen_;
    const Cost gap = DivideIfNonNegative(delta, last_max_lower_bound_seen_);
    if (gap <= params_.gap_limit_for_step_increase) {
      step_size_ *= params_.step_size_increase_factor;
      VLOG(4) << "[SUBG] Sep size set at " << step_size_;
    } else if (gap > params_.gap_limit_for_step_decrease) {
      step_size_ /= params_.step_size_decrease_factor;
      VLOG(4) << "[SUBG] Sep size set at " << step_size_;
    }
    last_min_lower_bound_seen_ = std::numeric_limits<Cost>::max();
    last_max_lower_bound_seen_ = 0.0;
    // Not described in the paper, but in rare cases the subgradient diverges
    step_size_ = std::clamp(step_size_, 1e-6, 10.0);  // Arbitrary from c4v4
  }
}

void BoundCallback::MakeMinimalCoverageSubgradient(
    const SubgradientContext& context, ElementCostVector& subgradient) {
  DCHECK_GE(context.model.num_subsets(), 0);
  lagrangian_solution_.clear();
  lagrangian_solution_.reserve(context.model.num_subsets());
  const auto& reduced_costs = context.current_dual_state.reduced_costs();
  for (const SubsetIndex j : context.model.SubsetRange()) {
    if (reduced_costs[j] < 0.0) {
      lagrangian_solution_.push_back(j);
    }
  }

  absl::c_sort(lagrangian_solution_,
               [&](const SubsetIndex j1, const SubsetIndex j2) {
                 return reduced_costs[j1] > reduced_costs[j2];
               });

  const auto& cols = context.model.columns();
  for (const SubsetIndex j : lagrangian_solution_) {
    if (absl::c_all_of(cols[j],
                       [&](const auto i) { return subgradient[i] < 0; })) {
      absl::c_for_each(cols[j], [&](const auto i) { subgradient[i] += 1.0; });
    }
  }
}

bool BoundCallback::UpdateCoreModel(SubgradientContext context,
                                    CoreModel& core_model, bool force) {
  DCHECK_GE(core_model.num_subsets(), 0);
  if (core_model.UpdateCore(context.best_lower_bound, context.best_multipliers,
                            context.best_solution, force)) {
    previous_best_lower_bound_ = std::numeric_limits<Cost>::lowest();
    // Grant at least `min_iters` iterations before the next exit test
    constexpr BaseInt min_iters = 10;
    exit_test_countdown_ = std::max<BaseInt>(exit_test_countdown_, min_iters);
    max_iter_countdown_ = std::max<BaseInt>(max_iter_countdown_, min_iters);
    return true;
  }
  return false;
}

// Subgradient optimization procedure. Optimizes the Lagrangian relaxation of
// the Set-Covering problem until a termination criterion is met.
void SubgradientOptimization(CoreModel& model, SubgradientCallback& cb,
                             const CftParameters& params,
                             PrimalDualState& best_state,
                             CftExecutionStats& stats) {
  DCHECK(ValidateSubmodel(model));
  CFT_MEASURE_SCOPE_DURATION(stats.subgradient_time);

  ElementCostVector subgradient = ElementCostVector(model.num_elements(), 0.0);
  DualState dual_state = best_state.dual_state;
  Cost best_lower_bound = dual_state.lower_bound();
  ElementCostVector best_multipliers = dual_state.multipliers();
  SubmodelSolution solution;

  ElementCostVector multipliers_delta(model.num_elements());  // to avoid allocs
  const SubgradientContext context = {.model = model,
                                      .current_dual_state = dual_state,
                                      .best_lower_bound = best_lower_bound,
                                      .best_multipliers = best_multipliers,
                                      .best_solution = best_state.solution,
                                      .subgradient = subgradient,
                                      .stats = stats};

  for (size_t iter = 1; !cb.ExitCondition(context); ++iter) {
    if (absl::Now() - stats.start_time >= stats.time_limit) {
      break;
    }
    // Poor multipliers can lead to wasted iterations or stagnation in the
    // subgradient method. To address this, we adjust the multipliers to
    // get closer to the trivial lower bound (= 0).
    if (dual_state.lower_bound() < 0.0) {
      VLOG(4) << "[SUBG] Dividing multipliers by 10";
      dual_state.DualUpdate(model, [&](const ElementIndex i, Cost& i_mult) {
        (void)i;
        i_mult /= 10.0;
      });
    }

    // Compute subgradient (O(nnz))
    subgradient.assign(model.num_elements(), 1.0);
    for (const SubsetIndex j : model.SubsetRange()) {
      if (dual_state.reduced_costs()[j] < 0.0) {
        for (const ElementIndex i : model.columns()[j]) {
          subgradient[i] -= 1.0;
        }
      }
    }

    cb.ComputeMultipliersDelta(context, multipliers_delta);
    dual_state.DualUpdate(model, [&](const ElementIndex i, Cost& i_mult) {
      i_mult = std::clamp<Cost>(i_mult + multipliers_delta[i], 0.0,
                                params.max_multiplier);
    });
    if (dual_state.lower_bound() > best_lower_bound) {
      best_lower_bound = dual_state.lower_bound();
      best_multipliers = dual_state.multipliers();
    }

    cb.RunHeuristic(context, solution);
    if (!solution.subsets().empty() &&
        solution.cost() < best_state.solution.cost()) {
      best_state.solution = solution;
    }

    VLOG_EVERY_N(4, 100) << "[SUBG] " << iter << ": Bounds: Lower "
                         << dual_state.lower_bound() << ", best "
                         << best_lower_bound << " - Upper "
                         << best_state.solution.cost() - model.fixed_cost()
                         << ", global " << best_state.solution.cost();

    if (cb.UpdateCoreModel(context, model, false)) {
      dual_state.DualUpdate(model, [&](const ElementIndex i, Cost& i_mult) {
        i_mult = best_multipliers[i];
      });
      best_lower_bound = dual_state.lower_bound();
    }
  }

  if (cb.UpdateCoreModel(context, model, /*force=*/true)) {
    dual_state.DualUpdate(model, [&](const ElementIndex i, Cost& i_mult) {
      i_mult = best_multipliers[i];
    });
    best_lower_bound = dual_state.lower_bound();
  }
  best_state.dual_state.DualUpdate(model,
                                   [&](const ElementIndex i, Cost& i_mult) {
                                     i_mult = best_multipliers[i];
                                   });
  DCHECK_EQ(best_state.dual_state.lower_bound(), best_lower_bound);

  VLOG(3) << "[SUBG] End - Bounds: Lower " << dual_state.lower_bound()
          << ", best " << best_lower_bound << " - Upper "
          << best_state.solution.cost() - model.fixed_cost() << ", global "
          << best_state.solution.cost();
}
}  // namespace

// Multiplier-based Greedy Heuristic.
namespace {
struct Score {
  Cost score;
  SubsetIndex idx;
};

class GreedyScores {
 public:
  static constexpr BaseInt kRemovedIndex = -1;
  static constexpr Cost kMaxScore = std::numeric_limits<Cost>::max();

  GreedyScores(const CoreModel& model, const DualState& dual_state)
      : bad_size_(),
        worst_good_score_(std::numeric_limits<Cost>::lowest()),
        scores_(),
        reduced_costs_(dual_state.reduced_costs()),
        covering_counts_(model.num_subsets()),
        score_map_(model.num_subsets()) {
    DCHECK_GE(model.num_subsets(), 0);
    scores_.reserve(model.num_subsets());
    BaseInt s = 0;
    for (const SubsetIndex j : model.SubsetRange()) {
      DCHECK_GT(model.column_size(j), 0);
      covering_counts_[j] = model.column_size(j);
      const Cost j_score = ComputeScore(reduced_costs_[j], covering_counts_[j]);
      scores_.push_back({j_score, j});
      score_map_[j] = s++;
      DCHECK(std::isfinite(reduced_costs_[j]));
      DCHECK(std::isfinite(j_score));
    }
    bad_size_ = scores_.size();
  }

  std::pair<SubsetIndex, bool> FindMinScoreColumn(const CoreModel& model,
                                                  const DualState& dual_state) {
    (void)dual_state;  // Unused.
    DCHECK_GE(model.num_subsets(), 0);
    // Check if the bad/good partition should be updated
    if (bad_size_ == scores_.size()) {
      if (bad_size_ > model.num_focus_elements()) {
        bad_size_ = bad_size_ - model.num_focus_elements();
        absl::c_nth_element(
            scores_, scores_.begin() + bad_size_,
            [](const Score a, const Score b) { return a.score > b.score; });
        worst_good_score_ = scores_[bad_size_].score;
        for (BaseInt s = 0; s < scores_.size(); ++s) {
          score_map_[scores_[s].idx] = s;
        }
      } else {
        bad_size_ = 0;
        worst_good_score_ = kMaxScore;
      }
      DCHECK(bad_size_ > 0 || worst_good_score_ == kMaxScore);
    }

    const Score min_score = *std::min_element(
        scores_.begin() + bad_size_, scores_.end(),
        [](const Score a, const Score b) { return a.score < b.score; });
    // If min_score.score == kMaxScore, all remaining candidate columns cover
    // zero uncovered rows. Under this condition, no column j_star is valid
    // (they are all redundant/useless) and the subproblem remains infeasible
    // under the current focus or column fixing.
    const SubsetIndex j_star = min_score.idx;
    return {j_star, min_score.score == kMaxScore};
  }

  // For each row in the given set, if `cond` returns true, the row is
  // considered newly covered. The function then iterates over the columns of
  // that row, updating the scores of the columns accordingly.
  template <typename RowT, typename ElementSpanT, typename CondT>
  BaseInt UpdateColumnsScoreOfRowsIf(const RowT& rows,
                                     const ElementCostVector& multipliers,
                                     const ElementSpanT& row_idxs, CondT cond) {
    BaseInt processed_rows_count = 0;
    for (const ElementIndex i : row_idxs) {
      if (!cond(i)) {
        continue;
      }

      ++processed_rows_count;
      for (const SubsetIndex j : rows[i]) {
        covering_counts_[j] -= 1;
        reduced_costs_[j] += multipliers[i];

        BaseInt s = score_map_[j];
        DCHECK_NE(s, kRemovedIndex) << "Column is not in the score map";
        scores_[s].score = ComputeScore(reduced_costs_[j], covering_counts_[j]);

        if (covering_counts_[j] == 0) {
          // Column is redundant: its score can be removed
          if (s < bad_size_) {
            // Column is bad: promote to good partition before removal
            SwapScores(s, bad_size_ - 1);
            s = --bad_size_;
          }
          SwapScores(s, scores_.size() - 1);
          scores_.pop_back();
        } else if (s >= bad_size_ && scores_[s].score > worst_good_score_) {
          // Column not good anymore: move it into bad partition
          SwapScores(s, bad_size_++);
        }
      }
    }
    return processed_rows_count;
  }

 private:
  void SwapScores(BaseInt s1, BaseInt s2) {
    DCHECK_GE(s1, 0);
    DCHECK_LT(s1, scores_.size());
    DCHECK_GE(s2, 0);
    DCHECK_LT(s2, scores_.size());

    const SubsetIndex j1 = scores_[s1].idx, j2 = scores_[s2].idx;
    std::swap(scores_[s1], scores_[s2]);
    std::swap(score_map_[j1], score_map_[j2]);
  }

  // Score computed as described in [1]
  static Cost ComputeScore(Cost adjusted_reduced_cost,
                           BaseInt num_rows_covered) {
    DCHECK(std::isfinite(adjusted_reduced_cost)) << "Gamma is not finite";
    return num_rows_covered == 0
               ? kMaxScore
               : (adjusted_reduced_cost > 0.0
                      ? adjusted_reduced_cost / num_rows_covered
                      : adjusted_reduced_cost * num_rows_covered);
  }

 private:
  // scores_ is partitioned into bad-scores / good-scores
  BaseInt bad_size_;

  // sentinel level to trigger a partition update of the scores
  Cost worst_good_score_;

  // column scores kept updated
  std::vector<Score> scores_;

  // reduced costs adjusted to currently uncovered rows (size=n)
  SubsetCostVector reduced_costs_;

  // number of uncovered rows covered by each column (size=n)
  SubsetToIntVector covering_counts_;

  // position of each column score into the scores_
  SubsetToIntVector score_map_;
};

// Stores the redundancy set and related information
class RedundancyRemover {
 public:
  RedundancyRemover(const CoreModel& model, CoverCounters& total_coverage)
      : redund_set_(),
        total_coverage_(total_coverage),
        partial_coverage_(model.num_elements()),
        partial_cost_(0.0),
        partial_size_(0),
        partial_cov_count_(0),
        cols_to_remove_() {}

  Cost TryRemoveRedundantCols(const CoreModel& model, Cost cost_cutoff,
                              std::vector<SubsetIndex>& sol_subsets) {
    DCHECK_GE(model.num_subsets(), 0);
    redund_set_.reserve(sol_subsets.size());
    for (const SubsetIndex j : sol_subsets) {
      if (total_coverage_.IsRedundantUncover(model.columns()[j])) {
        redund_set_.push_back({model.subset_costs()[j], j});
      } else {
        ++partial_size_;
        partial_cost_ += model.subset_costs()[j];
        partial_cov_count_ += partial_coverage_.Cover(model.columns()[j]);
      }
      if (partial_cost_ >= cost_cutoff) {
        return partial_cost_;
      }
    }
    if (redund_set_.empty()) {
      return partial_cost_;
    }
    absl::c_sort(redund_set_, [](const Score a, const Score b) {
      return a.score < b.score;
    });

    if (partial_cov_count_ < model.num_focus_elements()) {
      // Complete partial solution heuristically
      HeuristicRedundancyRemoval(model, cost_cutoff);
    } else {
      // All redundant columns can be removed
      cols_to_remove_.reserve(redund_set_.size());
      for (const Score redund_col : redund_set_) {
        cols_to_remove_.push_back(redund_col.idx);
      }
    }

    // Note: In [1], an enumeration to select the best redundant columns to
    // remove is performed when the number of redundant columns is <= 10.
    // However, based on experiments with github.com/c4v4/cft/, it appears
    // that this enumeration does not provide significant benefits to justify
    // the added complexity.

    if (partial_cost_ < cost_cutoff) {
      gtl::STLEraseAllFromSequenceIf(&sol_subsets, [&](const auto j) {
        return absl::c_any_of(cols_to_remove_,
                              [j](const auto j_r) { return j_r == j; });
      });
    }
    return partial_cost_;
  }

 private:
  // Remove redundant columns from the redundancy set using a heuristic
  void HeuristicRedundancyRemoval(const CoreModel& model, Cost cost_cutoff) {
    DCHECK_GE(model.num_subsets(), 0);
    (void)cost_cutoff;
    cols_to_remove_.reserve(cols_to_remove_.size() + redund_set_.size());
    while (!redund_set_.empty()) {
      if (partial_cov_count_ == model.num_focus_elements()) {
        return;
      }
      const SubsetIndex j = redund_set_.back().idx;
      const auto& j_col = model.columns()[j];
      redund_set_.pop_back();

      if (total_coverage_.IsRedundantUncover(j_col)) {
        total_coverage_.Uncover(j_col);
        cols_to_remove_.push_back(j);
      } else {
        partial_cost_ += model.subset_costs()[j];
        partial_cov_count_ += partial_coverage_.Cover(j_col);
      }
    }
  }

 private:
  // redundant columns + their cost
  std::vector<Score> redund_set_;

  // row-cov if all the remaining columns are selected
  CoverCounters total_coverage_;

  // row coverage if we selected the current column.
  CoverCounters partial_coverage_;

  // current partial solution cost
  Cost partial_cost_;

  // current partial solution size
  BaseInt partial_size_;

  // number of covered rows
  Cost partial_cov_count_;

  // list of columns to remove
  std::vector<SubsetIndex> cols_to_remove_;
};

// Lower level greedy function which creates or completes a "solution" (seen as
// set of subsets of the current CoreModel) until a cutoff size or cost is
// reached.
// Note: since the cutoff might be reached, the returned solution might not be
// feasible.
//
// Computes a greedy cover on the core model until a cost or size cutoff is
// reached.
Cost ComputeGreedyCover(const CoreModel& model, const DualState& dual_state,
                        Cost cost_cutoff, BaseInt size_cutoff,
                        std::vector<SubsetIndex>& sol_subsets,
                        CftExecutionStats& stats) {
  DCHECK_GE(model.num_subsets(), 0);
  CFT_MEASURE_SCOPE_DURATION(stats.greedy_time);

  Cost sol_cost = 0.0;
  for (const SubsetIndex j : sol_subsets) {
    sol_cost += model.subset_costs()[j];
  }
  if (sol_cost >= cost_cutoff) {
    sol_subsets.clear();
    return std::numeric_limits<Cost>::max();
  }
  if (sol_subsets.size() >= size_cutoff) {
    // Solution already has required size -> early exit
    return sol_cost;
  }

  // Process input solution (if not empty)
  BaseInt num_rows_to_cover = model.num_focus_elements();
  CoverCounters covered_rows(model.num_elements());
  for (const SubsetIndex j : sol_subsets) {
    num_rows_to_cover -= covered_rows.Cover(model.columns()[j]);
    if (num_rows_to_cover == 0) {
      return sol_cost;
    }
  }

  // Initialize column scores taking into account the rows already covered
  GreedyScores scores(model, dual_state);  // TODO(user): cache it!
  if (!sol_subsets.empty()) {
    scores.UpdateColumnsScoreOfRowsIf(
        model.rows(), dual_state.multipliers(), model.ElementRange(),
        [&](const auto i) { return covered_rows[i] > 0; });
  }

  // Fill up partial solution
  sol_subsets.reserve(size_cutoff);
  while (num_rows_to_cover > 0 && sol_subsets.size() < size_cutoff) {
    const auto [j_star, is_infeasible] =
        scores.FindMinScoreColumn(model, dual_state);
    if (is_infeasible) {
      break;
    }
    const auto& column = model.columns()[j_star];
    num_rows_to_cover -= scores.UpdateColumnsScoreOfRowsIf(
        model.rows(), dual_state.multipliers(), column,
        [&](const auto i) { return covered_rows[i] == 0; });
    sol_subsets.push_back(j_star);
    covered_rows.Cover(column);
  }

  if (num_rows_to_cover > 0) {
    sol_subsets.clear();
    return std::numeric_limits<Cost>::max();
  }

  // Either remove redundant columns or discard solution
  RedundancyRemover remover(model, covered_rows);  // TODO(user): cache it!

  return remover.TryRemoveRedundantCols(model, cost_cutoff, sol_subsets);
}

// Multiplier-based greedy heuristic.
//
// Higher level function that returns a solution obtained by the dual
// multiplier-based greedy heuristic.
// Runs the multiplier-based greedy covering heuristic on the core model.
SubmodelSolution RunMultiplierBasedGreedy(const CoreModel& model,
                                          const DualState& dual_state,
                                          Cost cost_cutoff,
                                          CftExecutionStats& stats) {
  DCHECK_GE(model.num_subsets(), 0);
  std::vector<SubsetIndex> sol_subsets;
  ComputeGreedyCover(model, dual_state, cost_cutoff,
                     /*size_cutoff=*/std::numeric_limits<BaseInt>::max(),
                     sol_subsets, stats);
  return SubmodelSolution(model, sol_subsets);
}

// 3-phase algorithm

DualState MakeTentativeDualState(const CoreModel& model) {
  DCHECK_GE(model.num_subsets(), 0);
  DualState tentative_dual_state(model);
  tentative_dual_state.DualUpdate(
      model, [&](const ElementIndex i, Cost& i_multiplier) {
        i_multiplier = std::numeric_limits<Cost>::max();
        for (const SubsetIndex j : model.rows()[i]) {
          const Cost candidate = model.subset_costs()[j] / model.column_size(j);
          i_multiplier = std::min(i_multiplier, candidate);
        }
      });
  return tentative_dual_state;
}

void FixBestColumns(CoreModel& model, PrimalDualState& state) {
  DCHECK_GE(model.num_subsets(), 0);
  auto& [best_sol, dual_state] = state;

  // This approach is not the most efficient but prioritizes clarity and the
  // current abstraction system. We save the current core multipliers, mapped to
  // the full model's element indices. By organizing the multipliers using the
  // full model indices, we can easily map them to the new core model indices
  // after fixing columns. Note: This mapping isn't strictly necessary because
  // fixing columns effectively removes rows, and the remaining multipliers
  // naturally shift to earlier positions. A simple iteration would suffice to
  // discard multipliers for rows no longer in the new core model.
  FullElementCostVector full_multipliers(model.num_elements(), 0.0);
  for (const ElementIndex core_i : model.ElementRange()) {
    const FullModelElementIndex full_i =
        model.MapCoreToFullElementIndex(core_i);
    full_multipliers[full_i] = dual_state.multipliers()[core_i];
  }

  std::vector<SubsetIndex> cols_to_fix;
  cols_to_fix.reserve(model.num_subsets());
  CoverCounters row_coverage(model.num_elements());
  for (const SubsetIndex j : model.SubsetRange()) {
    if (dual_state.reduced_costs()[j] < -0.001) {
      cols_to_fix.push_back(j);
      row_coverage.Cover(model.columns()[j]);
    }
  }

  // Remove columns that overlap with each other.
  gtl::STLEraseAllFromSequenceIf(&cols_to_fix, [&](const SubsetIndex j) {
    return absl::c_any_of(model.columns()[j], [&](const ElementIndex i) {
      return row_coverage[i] > 1;
    });
  });

  // Ensure at least a minimum number of columns is fixed.
  const BaseInt fix_at_least =
      cols_to_fix.size() + std::max<BaseInt>(1, model.num_elements() / 200);
  CftExecutionStats unused_stats;
  ComputeGreedyCover(model, state.dual_state,
                     /*cost_cutoff=*/std::numeric_limits<Cost>::max(),
                     /*size_cutoff=*/fix_at_least, cols_to_fix, unused_stats);

  // Fix columns and update the model.
  const Cost fixed_cost_delta = model.FixMoreColumns(cols_to_fix);

  VLOG(3) << "[3FIX] Fixed " << cols_to_fix.size()
          << " new columns with cost: " << fixed_cost_delta;
  VLOG(3) << "[3FIX] Globally fixed " << model.fixed_columns().size()
          << " columns, with cost " << model.fixed_cost();

  // Update multipliers for the reduced model.
  dual_state.DualUpdate(
      model, [&](const ElementIndex core_i, Cost& multiplier) {
        // Note: if SubmodelView is used as CoreModel, then this mapping is
        // always the identity mapping and can be removed.
        multiplier = full_multipliers[model.MapCoreToFullElementIndex(core_i)];
      });
}

void RandomizeDualState(const CoreModel& model, DualState& dual_state,
                        std::mt19937& rnd) {
  DCHECK_GE(model.num_subsets(), 0);
  // In [1] this step is described, not completely sure if it actually helps
  // or not. Seems to me one of those "throw in some randomness, it never hurts"
  // thing.
  dual_state.DualUpdate(model, [&](const ElementIndex, Cost& i_multiplier) {
    i_multiplier *= absl::Uniform(rnd, 0.9, 1.1);
  });
}

void HeuristicCallback::RunHeuristic(const SubgradientContext& context,
                                     SubmodelSolution& solution) {
  DCHECK_GE(context.model.num_subsets(), 0);
  solution = RunMultiplierBasedGreedy(
      context.model, context.current_dual_state,
      context.best_solution.cost() - context.model.fixed_cost(), context.stats);
}

void HeuristicCallback::ComputeMultipliersDelta(
    const SubgradientContext& context, ElementCostVector& delta_mults) {
  DCHECK_GE(context.model.num_subsets(), 0);
  DCHECK_EQ(delta_mults.size(), context.model.num_elements());

  Cost squared_norm = 0.0;
  for (const ElementIndex i : context.model.ElementRange()) {
    squared_norm += context.subgradient[i] * context.subgradient[i];
  }

  // NOMUTANTS -- the code would work even with other constants and different
  // bounds, but this is the original 3-phase implementation.
  const Cost lower_bound = context.current_dual_state.lower_bound();
  const Cost upper_bound =
      context.best_solution.cost() - context.model.fixed_cost();
  DCHECK_GE(upper_bound, lower_bound);
  const Cost delta = upper_bound - lower_bound;
  const Cost step_constant = step_size_ * delta / squared_norm;
  for (const ElementIndex i : context.model.ElementRange()) {
    delta_mults[i] = step_constant * context.subgradient[i];
  }
}

PrimalDualState RunThreePhase(CoreModel& model,
                              const SubmodelSolution& init_solution,
                              const CftParameters& params,
                              CftExecutionStats& stats) {
  DCHECK(ValidateSubmodel(model));
  CFT_MEASURE_SCOPE_DURATION(stats.three_phase_time);

  PrimalDualState best_state = {.solution = init_solution,
                                .dual_state = MakeTentativeDualState(model)};
  if (best_state.solution.Empty()) {
    best_state.solution =
        RunMultiplierBasedGreedy(model, best_state.dual_state,
                                 std::numeric_limits<BaseInt>::max(), stats);
  }
  VLOG(2) << "[3PHS] Initial lower bound: "
          << best_state.dual_state.lower_bound()
          << ", Initial solution cost: " << best_state.solution.cost()
          << ", Starting 3-phase algorithm";

  PrimalDualState curr_state = best_state;
  BaseInt iter_count = 0;
  std::mt19937 rnd(0xcf7);
  while (model.num_focus_elements() > 0) {
    if (absl::Now() - stats.start_time >= stats.time_limit) {
      break;
    }
    ++iter_count;
    VLOG(2) << "[3PHS] 3Phase iteration: " << iter_count;
    VLOG(2) << "[3PHS] Active size: rows " << model.num_focus_elements() << "/"
            << model.num_elements() << ", columns " << model.num_focus_subsets()
            << "/" << model.num_subsets();

    // Phase 1: refine the current dual_state and model
    BoundCallback dual_bound_cb(model, params);
    VLOG(2) << "[3PHS] Subgradient Phase:";
    SubgradientOptimization(model, dual_bound_cb, params, curr_state, stats);
    if (iter_count == 1) {
      best_state.dual_state = curr_state.dual_state;
    }
    if (curr_state.dual_state.lower_bound() >=
        best_state.solution.cost() - model.fixed_cost() -
            params.bound_min_diff_distance) {
      break;
    }

    // Phase 2: search for good solutions
    HeuristicCallback heuristic_cb(params);
    heuristic_cb.set_step_size(dual_bound_cb.step_size());
    VLOG(2) << "[3PHS] Heuristic Phase:";
    SubgradientOptimization(model, heuristic_cb, params, curr_state, stats);
    if (iter_count == 1 && best_state.dual_state.lower_bound() <
                               curr_state.dual_state.lower_bound()) {
      best_state.dual_state = curr_state.dual_state;
    }
    if (curr_state.solution.cost() < best_state.solution.cost()) {
      best_state.solution = curr_state.solution;
    }
    if (curr_state.dual_state.lower_bound() >=
        best_state.solution.cost() - model.fixed_cost() -
            params.bound_min_diff_distance) {
      break;
    }

    VLOG(2) << "[3PHS] 3Phase Bounds: Residual Lower "
            << curr_state.dual_state.lower_bound() + model.fixed_cost()
            << ", Upper " << best_state.solution.cost();

    // Phase 3: Fix the best columns (diving)
    FixBestColumns(model, curr_state);
    RandomizeDualState(model, curr_state.dual_state, rnd);
  }

  VLOG(2) << "[3PHS] 3Phase End: ";
  VLOG(2) << "[3PHS] Bounds: Residual Lower "
          << curr_state.dual_state.lower_bound() + model.fixed_cost()
          << ", Upper " << best_state.solution.cost();

  return best_state;
}

// Outer refinement procedure.

struct GapContribution {
  Cost gap;
  FullModelSubsetIndex idx;
};

std::vector<FullModelSubsetIndex> SelectColumnByGapContribution(
    const CoreModel& model, const PrimalDualState& best_state,
    BaseInt nrows_to_fix) {
  DCHECK_GE(model.num_subsets(), 0);
  DCHECK_GE(nrows_to_fix, 0);
  const auto& [solution, dual_state] = best_state;

  FullCoverCounters row_coverage(model.num_elements());
  const auto full_model = model.StrongTypedFullModelView();

  for (const FullModelSubsetIndex j : solution.subsets()) {
    row_coverage.Cover(full_model.columns()[j]);
  }

  std::vector<GapContribution> gap_contributions;
  gap_contributions.reserve(solution.subsets().size());
  for (const FullModelSubsetIndex j : solution.subsets()) {
    Cost j_gap = 0.0;
    Cost reduced_cost = dual_state.reduced_costs()[static_cast<SubsetIndex>(j)];
    for (const FullModelElementIndex i : full_model.columns()[j]) {
      const Cost i_mult =
          dual_state.multipliers()[static_cast<ElementIndex>(i)];
      j_gap += i_mult * (1.0 - 1.0 / row_coverage[i]);
      reduced_cost -= i_mult;
    }
    j_gap += std::max<Cost>(reduced_cost, 0.0);
    gap_contributions.push_back({j_gap, j});
  }
  absl::c_sort(gap_contributions,
               [](const GapContribution g1, const GapContribution g2) {
                 return g1.gap < g2.gap;
               });

  BaseInt covered_rows = 0;
  row_coverage.Reset(model.num_elements());
  std::vector<FullModelSubsetIndex> cols_to_fix;
  cols_to_fix.reserve(gap_contributions.size());
  for (const auto [j_gap, j] : gap_contributions) {
    covered_rows += row_coverage.Cover(full_model.columns()[j]);
    if (covered_rows > nrows_to_fix) {
      break;
    }
    cols_to_fix.push_back(static_cast<FullModelSubsetIndex>(j));
  }
  return cols_to_fix;
}

// Full To Core Pricing.

std::vector<FullModelSubsetIndex> ComputeTentativeFocus(
    FullModelView full_model) {
  DCHECK_GE(full_model.num_subsets(), 0);
  std::vector<FullModelSubsetIndex> columns_in_focus;

  if (full_model.num_subsets() <=
      2 * CftParameters::kMinimumCoverage * full_model.num_elements()) {
    columns_in_focus.resize(full_model.num_subsets());
    absl::c_iota(columns_in_focus, FullModelSubsetIndex(0));
    return columns_in_focus;
  }

  columns_in_focus.reserve(full_model.num_elements() *
                           CftParameters::kMinimumCoverage);
  FullSubsetBoolVector selected(full_model.num_subsets(), false);

  // Select the first min_row_coverage columns for each row
  for (const auto& row : full_model.rows()) {
    BaseInt countdown = CftParameters::kMinimumCoverage;
    for (const FullModelSubsetIndex j : row) {
      if (--countdown <= 0) {
        break;
      }
      if (!selected[j]) {
        selected[j] = true;
        columns_in_focus.push_back(j);
      }
    }
  }
  // NOTE: unnecessary, but it keeps equivalence between SubmodelView/CoreModel
  absl::c_sort(columns_in_focus);
  return columns_in_focus;
}

const std::vector<FullModelSubsetIndex>&
PricingModel::ColumnSelector::ComputeNewSelection(
    MaskedModelView full_model,
    const std::vector<FullModelSubsetIndex>& forced_columns,
    const SubsetCostVector& reduced_costs) {
  DCHECK_GE(full_model.num_subsets(), 0);
  selected_.assign(full_model.num_subsets(), false);
  row_cover_counts_.assign(full_model.num_elements(), 0);
  rows_left_to_cover_ = full_model.num_focus_elements();
  selection_.clear();
  candidates_.clear();

  // Always retain the best solution in the core model (if possible)
  for (const FullModelSubsetIndex full_j : forced_columns) {
    const SubsetIndex j = static_cast<SubsetIndex>(full_j);
    if (full_model.IsColumnInFocus(j)) {
      SelectColumn(full_model, j);
    }
  }

  const auto subset_range = full_model.SubsetRange();
  candidates_ = {subset_range.begin(), subset_range.end()};
  absl::c_sort(candidates_, [&](const auto j1, const auto j2) {
    return reduced_costs[j1] < reduced_costs[j2];
  });
  first_unselected_ = candidates_.begin();

  SelectMinReducedCostColumns(full_model, reduced_costs);
  SelectMinReducedCostByRow(full_model, reduced_costs);
  absl::c_sort(selection_);
  return selection_;
}

bool PricingModel::ColumnSelector::SelectColumn(MaskedModelView full_model,
                                                SubsetIndex j) {
  DCHECK_GE(j, SubsetIndex(0));
  DCHECK_LT(j, SubsetIndex(full_model.num_subsets()));
  if (selected_[j]) {
    return false;
  }
  for (const ElementIndex i : full_model.columns()[j]) {
    selected_[j] = true;  // Detect empty columns
    if (++row_cover_counts_[i] == CftParameters::kMinimumCoverage) {
      --rows_left_to_cover_;
    }
  }
  if (selected_[j]) {  // Skip empty columns
    selection_.push_back(static_cast<FullModelSubsetIndex>(j));
  }
  return selected_[j];
}

void PricingModel::ColumnSelector::SelectMinReducedCostColumns(
    MaskedModelView full_model, const SubsetCostVector& reduced_costs) {
  DCHECK_GE(full_model.num_subsets(), 0);
  BaseInt selected_size = 0;
  const BaseInt max_size =
      CftParameters::kMinimumCoverage * full_model.num_elements();
  auto it = first_unselected_;
  while (it != candidates_.end() && reduced_costs[*it] < 0.1 &&
         selected_size < max_size) {
    selected_size += SelectColumn(full_model, *it++) ? 1 : 0;
  }
  first_unselected_ = it;
}

void PricingModel::ColumnSelector::SelectMinReducedCostByRow(
    MaskedModelView full_model, const SubsetCostVector& reduced_costs) {
  DCHECK_GE(full_model.num_subsets(), 0);
  (void)reduced_costs;
  auto it = first_unselected_;
  while (it != candidates_.end() && rows_left_to_cover_ > 0) {
    const SubsetIndex j = *it++;
    if (selected_[j]) {
      continue;
    }
    for (const ElementIndex i : full_model.columns()[j]) {
      ++row_cover_counts_[i];
      rows_left_to_cover_ +=
          (row_cover_counts_[i] == CftParameters::kMinimumCoverage ? 1 : 0);
      selected_[j] = selected_[j] ||
                     (row_cover_counts_[i] <= CftParameters::kMinimumCoverage);
    }
    if (selected_[j]) {
      selection_.push_back(static_cast<FullModelSubsetIndex>(j));
    }
  }
}

PricingModel::PricingModel(const SetCoverModel& full_model)
    : CoreModel(full_model, ComputeTentativeFocus(FullModelView(full_model))),
      full_model_(full_model),
      is_column_in_focus_(full_model.num_subsets(), true),
      is_row_in_focus_(full_model.num_elements(), true),
      prev_best_lower_bound_(std::numeric_limits<Cost>::lowest()),
      full_dual_state_(full_model),
      best_dual_state_(full_dual_state_) {
  ResetPricingPeriod();
  DCHECK(ValidateSubmodel(*this));
  DCHECK(SubmodelInvariantCheck());
}

void PricingModel::ResetPricingPeriod() {
  update_countdown_ = 10;
  update_period_ = 10;
  update_max_period_ =
      std::min<BaseInt>(1000, full_model_.get().num_elements() / 3);
  DCHECK_GE(update_max_period_, 0);
}

Cost PricingModel::FixMoreColumns(
    const std::vector<SubsetIndex>& columns_to_fix) {
  DCHECK(CheckIndices(columns_to_fix, SubsetIndex(num_focus_subsets())));

  const FullModelView typed_full_model = StrongTypedFullModelView();
  for (const SubsetIndex core_j : columns_to_fix) {
    const FullModelSubsetIndex full_j =
        CoreModel::MapCoreToFullSubsetIndex(core_j);
    DCHECK(IsColumnInFocus(full_j));
    IsColumnInFocus(full_j) = false;
    bool any_active = false;
    for (const FullModelElementIndex full_i :
         typed_full_model.columns()[full_j]) {
      any_active |= IsRowInFocus(full_i);
      IsRowInFocus(full_i) = false;
    }
    DCHECK(any_active);
  }
  for (const FullModelSubsetIndex full_j : typed_full_model.SubsetRange()) {
    if (!IsColumnInFocus(full_j)) {
      continue;
    }
    IsColumnInFocus(full_j) = false;
    for (const FullModelElementIndex full_i :
         typed_full_model.columns()[full_j]) {
      if (IsRowInFocus(full_i)) {
        IsColumnInFocus(full_j) = true;
        break;
      }
    }
  }

  ResetPricingPeriod();
  const Cost fixed_cost = CoreModel::FixMoreColumns(columns_to_fix);
  DCHECK(SubmodelInvariantCheck());
  return fixed_cost;
}

void PricingModel::ResetColumnFixing(
    const std::vector<FullModelSubsetIndex>& full_columns_to_fix,
    const DualState& dual_state) {
  DCHECK(CheckIndices(full_columns_to_fix,
                      FullModelSubsetIndex(full_model_.get().num_subsets())));

  is_column_in_focus_.assign(full_model_.get().num_subsets(), true);
  is_row_in_focus_.assign(full_model_.get().num_elements(), true);

  full_dual_state_ = dual_state;

  // We could implement an in-place core-model update that removes old fixings,
  // sets the new fixings while also updating the column focus. This solution is
  // much simpler. It just creates a new core-model object from scratch and then
  // uses the existing interface.
  const auto& selection = column_selector_.ComputeNewSelection(
      FixingFullModelView(), full_columns_to_fix,
      full_dual_state_.reduced_costs());
  static_cast<CoreModel&>(*this) = CoreModel(full_model_.get(), selection);

  // TODO(user): Improve this. It's inefficient but hardly a bottleneck and it
  // also avoids storing a full->core column map.
  std::vector<SubsetIndex> columns_to_fix;
  for (const SubsetIndex core_j : SubsetRange()) {
    for (const FullModelSubsetIndex full_j : full_columns_to_fix) {
      if (full_j == MapCoreToFullSubsetIndex(core_j)) {
        columns_to_fix.push_back(core_j);
        break;
      }
    }
  }
  DCHECK_EQ(columns_to_fix.size(), full_columns_to_fix.size());
  FixMoreColumns(columns_to_fix);
  DCHECK(SubmodelInvariantCheck());
}

bool PricingModel::IsTimeToUpdate(Cost best_lower_bound, bool force) {
  if (!force && --update_countdown_ > 0) {
    return false;
  }
  if (best_lower_bound == prev_best_lower_bound_) {
    return false;
  }
  prev_best_lower_bound_ = best_lower_bound;
  return true;
}

void PricingModel::ComputeAndSetFocus(Cost best_lower_bound,
                                      const SubmodelSolution& best_solution) {
  DCHECK_GE(best_lower_bound, 0.0);
  const auto& selection = column_selector_.ComputeNewSelection(
      FixingFullModelView(), best_solution.subsets(),
      full_dual_state_.reduced_costs());
  CoreModel::SetFocus(selection);
  UpdatePricingPeriod(full_dual_state_, best_lower_bound,
                      best_solution.cost() - fixed_cost());
  VLOG(3) << "[F2CU] Core-update: Lower bounds: real "
          << full_dual_state_.lower_bound() << ", core " << best_lower_bound
          << ", core size: " << num_focus_elements() << "x"
          << num_focus_subsets();
  DCHECK(SubmodelInvariantCheck());
}

bool PricingModel::UpdateCore(Cost best_lower_bound,
                              const ElementCostVector& best_multipliers,
                              const SubmodelSolution& best_solution,
                              bool force) {
  DCHECK_GE(best_lower_bound, 0.0);
  if (!IsTimeToUpdate(best_lower_bound, force)) {
    return false;
  }
  UpdateMultipliers(best_multipliers);
  ComputeAndSetFocus(best_lower_bound, best_solution);
  DCHECK(SubmodelInvariantCheck());
  return true;
}

void PricingModel::UpdatePricingPeriod(const DualState& full_dual_state,
                                       Cost core_lower_bound,
                                       Cost core_upper_bound) {
  DCHECK_GE(core_lower_bound + 1e-6, full_dual_state.lower_bound());
  DCHECK_GE(core_upper_bound, 0.0);

  const Cost delta = core_lower_bound - full_dual_state.lower_bound();
  const Cost ratio =
      DivideIfNonNegative(std::min(delta, core_upper_bound), core_upper_bound);
  if (ratio <= 1e-6) {
    update_period_ = std::min<BaseInt>(update_max_period_, 10 * update_period_);
  } else if (ratio <= 0.02) {
    update_period_ = std::min<BaseInt>(update_max_period_, 5 * update_period_);
  } else if (ratio <= 0.2) {
    update_period_ = std::min<BaseInt>(update_max_period_, 2 * update_period_);
  } else {
    update_period_ = 10;
  }
  update_countdown_ = update_period_;
}

Cost PricingModel::UpdateMultipliers(
    const ElementCostVector& core_multipliers) {
  DCHECK_EQ(core_multipliers.size(), num_focus_elements());
  const auto fixing_full_model = FixingFullModelView();
  full_dual_state_.DualUpdate(fixing_full_model, [&](const ElementIndex full_i,
                                                     Cost& i_mult) {
    const ElementIndex core_i =
        MapFullToCoreElementIndex(static_cast<FullModelElementIndex>(full_i));
    i_mult = core_multipliers[core_i];
  });

  // Here, we simply check if any columns have been fixed, and only update the
  // best dual state when no fixing is in place.
  //
  // Mapping a "local" dual state to a global one is possible. This would
  // involve keeping the multipliers for non-focused elements fixed, updating
  // the multipliers for focused elements, and then computing the dual state for
  // the entire model. However, this approach is not implemented here. Such a
  // strategy is unlikely to improve the best dual state unless the focus is
  // *always* limited to a small subset of elements (and therefore the LB sucks
  // and it is easy to improve) and the CFT is applied exclusively within that
  // narrow scope, but this falls outside the current scope of this project.
  if (fixed_columns().empty() &&
      full_dual_state_.lower_bound() > best_dual_state_.lower_bound()) {
    best_dual_state_ = full_dual_state_;
  }
  return full_dual_state_.lower_bound();
}

bool PricingModel::SubmodelInvariantCheck() {
  const CoreModel& sub_model = *this;
  const FullModelView typed_full_model = StrongTypedFullModelView();

  if (typed_full_model.num_subsets() < sub_model.num_subsets()) {
    LOG(WARNING) << absl::StrCat("SubmodelView has ", sub_model.num_subsets(),
                                 " subsets, but the full model has ",
                                 typed_full_model.num_subsets(), " subsets.\n");
    return false;
  }
  if (typed_full_model.num_elements() != sub_model.num_elements()) {
    LOG(WARNING) << absl::StrCat("SubmodelView has ", sub_model.num_elements(),
                                 " elements, but the full model has ",
                                 typed_full_model.num_elements(),
                                 " elements.\n");
    return false;
  }
  for (const SubsetIndex core_j : sub_model.SubsetRange()) {
    const FullModelSubsetIndex full_j =
        sub_model.MapCoreToFullSubsetIndex(core_j);
    if (!is_column_in_focus_[static_cast<SubsetIndex>(full_j)]) {
      LOG(WARNING) << absl::StrCat("Subset ", core_j,
                                   " in sub-model but its mapped subset ",
                                   full_j, " not found in full model view.\n");
      return false;
    }

    const auto& core_column = sub_model.columns()[core_j];
    if (core_column.begin() == core_column.end()) {
      LOG(WARNING) << absl::StrCat("Core subset ", core_j, " empty.\n");
      return false;
    }

    const auto& full_column = typed_full_model.columns()[full_j];
    if (full_column.begin() == full_column.end()) {
      LOG(WARNING) << absl::StrCat("Full subset ", full_j, " empty.\n");
      return false;
    }

    // Assumes corresponding elements have the same order in both models
    auto core_it = core_column.begin();
    for (const FullModelElementIndex full_i :
         typed_full_model.columns()[full_j]) {
      if (sub_model.MapFullToCoreElementIndex(full_i) != *core_it) {
        continue;
      }
      if (sub_model.MapCoreToFullElementIndex(*core_it) != full_i) {
        LOG(WARNING) << absl::StrCat(
            "Subset ", core_j, " in sub-model has mapped element ", *core_it,
            " but it is not the same as the full model.\n");
        return false;
      }
      if (++core_it == core_column.end()) {
        break;
      }
    }
    if (core_it != core_column.end()) {
      LOG(WARNING) << absl::StrCat("Subset ", core_j,
                                   " in sub-model has no mapped element ",
                                   *core_it, " in full model view.\n");
      return false;
    }
  }

  for (const ElementIndex core_i : sub_model.ElementRange()) {
    const auto& core_row = sub_model.rows()[core_i];
    if (core_row.begin() == core_row.end()) {
      LOG(WARNING) << absl::StrCat("Core row ", core_i, " empty.\n");
      return false;
    }

    const FullModelElementIndex full_i =
        sub_model.MapCoreToFullElementIndex(core_i);
    if (!is_row_in_focus_[static_cast<ElementIndex>(full_i)]) {
      LOG(WARNING) << absl::StrCat("Element ", core_i,
                                   " in sub-model but its mapped element ",
                                   full_i, " not found in full model view.\n");
      return false;
    }
  }

  for (const FullModelElementIndex full_i : typed_full_model.ElementRange()) {
    if (!IsRowInFocus(full_i)) {
      continue;
    }
    const ElementIndex core_i = sub_model.MapFullToCoreElementIndex(full_i);
    if (core_i < ElementIndex() ||
        ElementIndex(sub_model.num_elements()) < core_i) {
      LOG(WARNING) << absl::StrCat(
          "Element ", full_i,
          " in full model view but has no mapped element in sub-model.\n");
      return false;
    }
  }
  return true;
}
}  // namespace

PrimalDualState RunCftHeuristic(CoreModel& model,
                                const SubmodelSolution& init_solution,
                                const CftParameters& params) {
  DCHECK(ValidateSubmodel(model));
  CftExecutionStats stats;
  stats.start_time = absl::Now();
  stats.time_limit = params.time_limit;
  CFT_MEASURE_SCOPE_DURATION(stats.refinement_time);

  PrimalDualState best_state = {.solution = init_solution,
                                .dual_state = MakeTentativeDualState(model)};
  if (best_state.solution.Empty()) {
    best_state.solution =
        RunMultiplierBasedGreedy(model, best_state.dual_state,
                                 std::numeric_limits<BaseInt>::max(), stats);
  }

  Cost cost_cutoff = std::numeric_limits<Cost>::lowest();
  const double fix_minimum = params.fix_minimum;
  const double fix_increment = params.fix_increment;
  double fix_fraction = fix_minimum;

  for (BaseInt iter_counter = 0; model.num_elements() > 0; ++iter_counter) {
    if (absl::Now() - stats.start_time >= stats.time_limit) {
      break;
    }
    VLOG(1) << "[CFTH] Refinement iteration: " << iter_counter;
    const Cost fixed_cost_before = model.fixed_cost();
    const auto [solution, dual_state] =
        RunThreePhase(model, best_state.solution, params, stats);

    // Keep the best dual state found so far, because the `dual_state` might
    // move.
    const Cost dual_lower_bound = dual_state.lower_bound();
    if (iter_counter == 0) {
      best_state.dual_state = std::move(dual_state);
    }

    if (solution.cost() < best_state.solution.cost()) {
      best_state.solution = solution;
      fix_fraction = fix_minimum;
    }
    cost_cutoff =
        std::max<Cost>(cost_cutoff, fixed_cost_before + dual_lower_bound);
    VLOG(1) << "[CFTH] Refinement Bounds: Residual Lower " << cost_cutoff
            << ", Real Lower " << dual_lower_bound << ", Upper "
            << best_state.solution.cost();
    if (best_state.solution.cost() - params.bound_min_diff_distance <=
        cost_cutoff) {
      break;
    }

    fix_fraction = std::min(1.0, fix_fraction * fix_increment);
    const std::vector<FullModelSubsetIndex> cols_to_fix =
        SelectColumnByGapContribution(model, best_state,
                                      model.num_elements() * fix_fraction);

    if (!cols_to_fix.empty()) {
      model.ResetColumnFixing(cols_to_fix, best_state.dual_state);
    }
    VLOG(1) << "[CFTH] Fixed " << cols_to_fix.size()
            << " new columns with cost: " << model.fixed_cost();
    VLOG(1) << "[CFTH] SetCoverModel sizes: rows " << model.num_focus_elements()
            << "/" << model.num_elements() << ", columns "
            << model.num_focus_subsets() << "/" << model.num_subsets();
  }

#ifdef CFT_MEASURE_TIME
  const double subg_t = absl::ToDoubleSeconds(stats.subgradient_time);
  const double greedy_t = absl::ToDoubleSeconds(stats.greedy_time);
  const double three_phase_t = absl::ToDoubleSeconds(stats.three_phase_time);
  const double refinement_t = absl::ToDoubleSeconds(stats.refinement_time);

  printf("Subgradient time:   %8.2f (%.1f%%)\n", subg_t,
         100 * subg_t / refinement_t);
  printf("Greedy Heur time:   %8.2f (%.1f%%)\n", greedy_t,
         100 * greedy_t / refinement_t);
  printf("SubG - Greedy time: %8.2f (%.1f%%)\n", subg_t - greedy_t,
         100 * (subg_t - greedy_t) / refinement_t);
  printf("3Phase time:        %8.2f (%.1f%%)\n", three_phase_t,
         100 * three_phase_t / refinement_t);
  printf("3Phase - Subg time: %8.2f (%.1f%%)\n", three_phase_t - subg_t,
         100 * (three_phase_t - subg_t) / refinement_t);
  printf("Total CFT time:     %8.2f (%.1f%%)\n", refinement_t, 100.0);
#endif

  return best_state;
}

SetCoverCftOptimizer::SetCoverCftOptimizer(SetCoverInvariant* inv)
    : SetCoverOptimizer(inv,
                        SetCoverInvariant::ConsistencyLevel::kCostAndCoverage,
                        "SetCoverCftOptimizer", "cft") {}

bool SetCoverCftOptimizer::Optimize() {
  DCHECK(inv()->CheckConsistency(
      SetCoverInvariant::ConsistencyLevel::kCostAndCoverage));
  return Optimize(model()->all_subsets());
}

bool SetCoverCftOptimizer::Optimize(const SubsetBoolVector& in_focus) {
  DCHECK(inv()->CheckConsistency(
      SetCoverInvariant::ConsistencyLevel::kCostAndCoverage));
  std::vector<SubsetIndex> focus;
  focus.reserve(in_focus.size());
  SubsetIndex i(0);
  for (const auto bit : in_focus) {
    if (bit) {
      focus.push_back(i);
    }
    ++i;
  }
  return Optimize(focus);
}

bool SetCoverCftOptimizer::Optimize(absl::Span<const SubsetIndex> focus) {
  StopWatch stop_watch(&run_time_);
  DCHECK(inv()->CheckConsistency(
      SetCoverInvariant::ConsistencyLevel::kCostAndCoverage));
  std::vector<FullModelSubsetIndex> full_focus;
  full_focus.reserve(focus.size());
  for (const SubsetIndex j : focus) {
    full_focus.push_back(static_cast<FullModelSubsetIndex>(j));
  }
  CoreModel core_model(*inv()->model(), full_focus);
  std::vector<SubsetIndex> initial_subsets;
  initial_subsets.reserve(core_model.num_focus_subsets());
  for (const SubsetIndex core_j : core_model.SubsetRange()) {
    const FullModelSubsetIndex full_j =
        core_model.MapCoreToFullSubsetIndex(core_j);
    if (inv()->is_selected()[static_cast<SubsetIndex>(full_j)]) {
      initial_subsets.push_back(core_j);
    }
  }
  SubmodelSolution initial_solution(core_model, initial_subsets);

  CftParameters params = params_;
  params.time_limit = time_limit();

  PrimalDualState best_state =
      RunCftHeuristic(core_model, initial_solution, params);

  SubsetBoolVector new_solution(inv()->model()->num_subsets(), false);
  for (const FullModelSubsetIndex full_j : best_state.solution.subsets()) {
    new_solution[static_cast<SubsetIndex>(full_j)] = true;
  }
  inv()->LoadSolution(new_solution);
  inv()->ReportLowerBound(best_state.dual_state.lower_bound(),
                          /*is_cost_consistent=*/false);
  return true;
}

}  // namespace operations_research
