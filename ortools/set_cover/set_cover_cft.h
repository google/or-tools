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

#ifndef ORTOOLS_SET_COVER_SET_COVER_CFT_H_
#define ORTOOLS_SET_COVER_SET_COVER_CFT_H_

#include <cmath>
#include <functional>
#include <limits>
#include <vector>

#include "absl/log/check.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_heuristics.h"
#include "ortools/set_cover/set_cover_invariant.h"
#include "ortools/set_cover/set_cover_model.h"
#include "ortools/set_cover/set_cover_submodel.h"
#include "ortools/set_cover/set_cover_views.h"

namespace operations_research {

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
// (Note: columns/subsets and rows/elements will be used interchangeably.)
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

// Common Definitions

// Statistics gathered during CFT algorithm execution.
struct CftExecutionStats {
  // Accumulated time spent in subgradient optimization.
  absl::Duration subgradient_time;

  // Accumulated time spent in greedy heuristics.
  absl::Duration greedy_time;

  // Accumulated time spent in the three-phase core procedure.
  absl::Duration three_phase_time;

  // Accumulated time spent in the outer search refinement.
  absl::Duration refinement_time;

  // Time limit for the optimization.
  absl::Duration time_limit = absl::InfiniteDuration();

  // The start time of the optimization.
  absl::Time start_time;
};

// Parameters configuring the CFT algorithm heuristics.
struct CftParameters {
  // Coefficient multiplied by model focus elements to get max iteration
  // countdown.
  BaseInt max_iter_multiplier = 10;

  // Countdown and period value between exit tests.
  BaseInt exit_test_period = 300;

  // Initial step size during subgradient optimization.
  Cost initial_step_size = 0.1;

  // Period (iterations) between step size updates.
  BaseInt step_size_update_period = 20;

  // Gap limit threshold below which step size is increased.
  Cost gap_limit_for_step_increase = 0.001;

  // Multiplier factor by which step size is amplified.
  Cost step_size_increase_factor = 1.5;

  // Gap limit threshold above which step size is decreased.
  Cost gap_limit_for_step_decrease = 0.01;

  // Divisor factor by which step size is reduced.
  Cost step_size_decrease_factor = 2.0;

  // Minimum distance between lower and upper bounds to consider them different.
  // If costs are all integral, can be set closer to 1.0.
  Cost bound_min_diff_distance = 0.999;

  // Maximum multiplier limit for dual multipliers.
  Cost max_multiplier = 1e9;

  // Time limit for the heuristic.
  absl::Duration time_limit = absl::InfiniteDuration();

  // Minimum fraction of columns to fix.
  double fix_minimum = 0.3;

  // Multiplier factor by which the fraction of fixed columns is increased.
  double fix_increment = 1.1;

  // Tolerance used to check for division in DivideIfNonNegative.
  static constexpr Cost kDivisionTolerance = 1e-6;

  // Coverage counter to decide the number of columns to keep in the
  // core model.
  static constexpr BaseInt kMinimumCoverage = 5;
};

// Small class to store the solution of a sub-model. It contains the cost and
// the subset list.
class SubmodelSolution {
 public:
  // Default constructor.
  SubmodelSolution() = default;

  // Constructor for a submodel solution from a subset list.
  SubmodelSolution(const CoreModel& model,
                   const std::vector<SubsetIndex>& core_subsets);

  // Returns the cost of the solution.
  double cost() const {
    DCHECK_GE(cost_, 0.0);
    return cost_;
  }

  // Returns the list of subsets in the solution.
  const std::vector<FullModelSubsetIndex>& subsets() const {
#ifndef NDEBUG
    for (FullModelSubsetIndex v : subsets_) {
      DCHECK_GE(v, FullModelSubsetIndex(0));
    }
#endif
    return subsets_;
  }

  // Adds a subset to the solution and updates the cost accordingly.
  void AddSubset(FullModelSubsetIndex subset, Cost cost) {
    DCHECK_GE(subset, FullModelSubsetIndex(0));
    DCHECK_GE(cost, 0.0);
    DCHECK_GE(cost_, 0.0);
    subsets_.push_back(subset);
    cost_ += cost;
  }

  // Returns true if the solution represents an empty subset list.
  bool Empty() const { return subsets_.empty(); }

  // Clears the solution resetting cost and subsets list.
  void Clear() {
    cost_ = 0.0;
    subsets_.clear();
  }

 private:
  // The cost of the solution.
  Cost cost_ = std::numeric_limits<Cost>::max();

  // The list of full subset indices in the solution.
  std::vector<FullModelSubsetIndex> subsets_;
};

// Dual information related to a CoreModel.
// Stores multipliers, reduced costs, and the lower bound, and provides an
// interface that keeps them aligned.
class DualState {
 public:
  // Default constructor.
  DualState() = default;

  // Copy constructor.
  DualState(const DualState&) = default;

  // Constructor initializing with dummy values.
  template <typename SubmodelT>
  explicit DualState(const SubmodelT& model)
      : lower_bound_(0.0),
        multipliers_(model.num_elements(), 0.0),
        reduced_costs_(model.subset_costs().begin(),
                       model.subset_costs().end()) {
    DCHECK_GE(model.num_elements(), 0);
    DCHECK_GE(model.num_subsets(), 0);
  }

  // Returns the Lagrangian lower bound.
  Cost lower_bound() const { return lower_bound_; }

  // Returns the Lagrangian multiplier vectors.
  const ElementCostVector& multipliers() const {
#ifndef NDEBUG
    for (Cost m : multipliers_) {
      DCHECK_GE(m, 0.0);
    }
#endif
    return multipliers_;
  }

  // Returns the reduced costs vector.
  const SubsetCostVector& reduced_costs() const { return reduced_costs_; }

  // Updates dual multipliers and recomputes lower bound.
  // NOTE: This function contains one of the two O(nnz) subgradient steps
  template <typename SubmodelT, typename OperatorFn>
  void DualUpdate(const SubmodelT& model, OperatorFn multiplier_operator) {
    DCHECK_GE(model.num_elements(), 0);
    DCHECK_GE(model.num_subsets(), 0);

    multipliers_.resize(model.num_elements());
    reduced_costs_.resize(model.num_subsets());
    lower_bound_ = 0.0;
    // Update multipliers
    for (ElementIndex i : model.ElementRange()) {
      multiplier_operator(i, multipliers_[i]);
      lower_bound_ += multipliers_[i];
      DCHECK(std::isfinite(multipliers_[i]));
      DCHECK_GE(multipliers_[i], 0.0);
    }
    lower_bound_ += ComputeReducedCosts(model, multipliers_, reduced_costs_);
  }

 private:
  // Computes reduced costs for the active elements in the model.
  // Single hot point to optimize once for the different use cases.
  template <typename ModelT>
  static Cost ComputeReducedCosts(const ModelT& model,
                                  const ElementCostVector& multipliers,
                                  SubsetCostVector& reduced_costs) {
    DCHECK_GE(model.num_subsets(), 0);
    DCHECK_EQ(reduced_costs.size(), model.num_subsets());

    // Compute new reduced costs (O(nnz))
    Cost negative_sum = 0.0;
    for (SubsetIndex j : model.SubsetRange()) {
      reduced_costs[j] = model.subset_costs()[j];
      for (ElementIndex i : model.columns()[j]) {
        reduced_costs[j] -= multipliers[i];
      }
      if (reduced_costs[j] < 0.0) {
        negative_sum += reduced_costs[j];
      }
    }
    return negative_sum;
  }

  // The dual lower bound value.
  Cost lower_bound_;

  // The vector containing multipliers.
  ElementCostVector multipliers_;

  // The vector of reduced costs.
  SubsetCostVector reduced_costs_;
};

// Utility aggregate to store and pass around both primal and dual states.
struct PrimalDualState {
  // Primal state solution.
  SubmodelSolution solution;

  // Active dual state information.
  DualState dual_state;
};

// Outer refinement procedure.
//
// Runs the outer CFT heuristic with automatic model refinement.
PrimalDualState RunCftHeuristic(CoreModel& model,
                                const SubmodelSolution& init_solution = {},
                                const CftParameters& params = {});

// SetCoverCftOptimizer is a solution generator based on CFT heuristics.
class SetCoverCftOptimizer : public SetCoverOptimizer {
 public:
  explicit SetCoverCftOptimizer(SetCoverInvariant* inv);

  ~SetCoverCftOptimizer() override = default;

  // Computes the next full solution taking into account all the subsets.
  bool Optimize() override;

  // Computes the next partial solution considering only the subsets whose
  // indices are in focus.
  bool Optimize(absl::Span<const SubsetIndex> focus) override;

  // Same as above, but with a vector of Booleans as focus.
  bool Optimize(const SubsetBoolVector& in_focus) override;

  // Returns mutable parameters of the CFT heuristic.
  CftParameters& params() { return params_; }

  // Returns parameters of the CFT heuristic.
  const CftParameters& params() const { return params_; }

 private:
  CftParameters params_;
};

}  // namespace operations_research

#endif  // ORTOOLS_SET_COVER_SET_COVER_CFT_H_
