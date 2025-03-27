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

#include "ortools/set_cover/set_cover_lagrangian.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <tuple>
#include <vector>

#include "absl/log/check.h"
#include "absl/synchronization/blocking_counter.h"
#include "ortools/algorithms/adjustable_k_ary_heap.h"
#include "ortools/base/threadpool.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_invariant.h"
#include "ortools/set_cover/set_cover_model.h"

namespace operations_research {

// Notes from a discussion with Luca Accorsi (accorsi@) and Francesco Cavaliere
// regarding [1]:
// - the 3-phase algorithm in the paper actually uses pricing (which would
//   better be called "partial" pricing),
// - the columns that were used in the preceding solutions should be fixed,
//   because otherwise it diversifies too much and degrades the best solution
//   (under "queue" in the paper).
// - the median algorithm is already in the STL (nth_element).

// Denoted as u in [1], it is a dual vector: a column vector of nonnegative
// (zero is included) multipliers for the different constraints.
// A deterministic way to compute a feasible (non-optimal) u:
// For all element indices i, u_i = min {j \in J_i} c_j / |I_j|, where
// |I_j| denotes the number of elements covered by subset j.
//
// Concerning the fundamental ideas behind this approach, one may refer to [2].
ElementCostVector SetCoverLagrangian::InitializeLagrangeMultipliers() const {
  ElementCostVector multipliers(model()->num_elements(),
                                std::numeric_limits<Cost>::infinity());
  SubsetCostVector marginal_costs(model()->num_subsets());
  // TODO(user): Parallelize this.
  for (const SubsetIndex subset : model()->SubsetRange()) {
    marginal_costs[subset] =
        model()->subset_costs()[subset] / model()->columns()[subset].size();
  }
  // TODO(user): Parallelize this.
  for (const ElementIndex element : model()->ElementRange()) {
    // Minimum marginal cost to cover this element.
    Cost min_marginal_cost = std::numeric_limits<Cost>::infinity();
    const SparseRowView& rows = model()->rows();
    // TODO(user): use std::min_element on rows[element] with a custom
    // comparator that gets marginal_costs[subset]. Check performance.
    for (const SubsetIndex subset : rows[element]) {
      min_marginal_cost = std::min(min_marginal_cost, marginal_costs[subset]);
    }
    multipliers[element] = min_marginal_cost;
  }
  return multipliers;
}

namespace {
// Computes the scalar product between a column and a vector of duals.
// Profiling has shown that this is where most of the time is spent.
// TODO(user): make this visible to other algorithms.
// TODO(user): Investigate.
Cost ScalarProduct(const SparseColumn& column, const ElementCostVector& dual) {
  Cost result = 0.0;
  for (const ColumnEntryIndex pos : column.index_range()) {
    result += dual[column[pos]];
  }
  return result;
}

// Computes the reduced costs for a subset of subsets.
// This is a helper function for ParallelComputeReducedCosts().
// It is called on a slice of subsets, defined by slice_start and slice_end.
// The reduced costs are computed using the multipliers vector.
// The columns of the subsets are given by the columns view.
// The result is stored in reduced_costs.
void FillReducedCostsSlice(SubsetIndex slice_start, SubsetIndex slice_end,
                           const SubsetCostVector& costs,
                           const ElementCostVector& multipliers,
                           const SparseColumnView& columns,
                           SubsetCostVector* reduced_costs) {
  for (SubsetIndex subset = slice_start; subset < slice_end; ++subset) {
    (*reduced_costs)[subset] =
        costs[subset] - ScalarProduct(columns[subset], multipliers);
  }
}

BaseInt BlockSize(BaseInt size, int num_threads) {
  // Traditional formula to compute std::ceil(size / num_threads).
  return (size + num_threads - 1) / num_threads;
}
}  // namespace

// Computes the reduced costs for all subsets in parallel using ThreadPool.
SubsetCostVector SetCoverLagrangian::ParallelComputeReducedCosts(
    const SubsetCostVector& costs, const ElementCostVector& multipliers) const {
  const SubsetIndex num_subsets(model()->num_subsets());
  const SparseColumnView& columns = model()->columns();
  SubsetCostVector reduced_costs(num_subsets);
  // TODO(user): compute a close-to-optimal k-subset partitioning of the columns
  // based on their sizes. [***]
  const SubsetIndex block_size(BlockSize(num_subsets.value(), num_threads_));
  absl::BlockingCounter num_threads_running(num_threads_);
  SubsetIndex slice_start(0);
  for (int thread_index = 0; thread_index < num_threads_; ++thread_index) {
    const SubsetIndex slice_end =
        std::min(slice_start + block_size, num_subsets);
    thread_pool_->Schedule([&num_threads_running, slice_start, slice_end,
                            &costs, &multipliers, &columns, &reduced_costs]() {
      FillReducedCostsSlice(slice_start, slice_end, costs, multipliers, columns,
                            &reduced_costs);
      num_threads_running.DecrementCount();
    });
    slice_start = slice_end;
  }
  num_threads_running.Wait();
  return reduced_costs;
}

// Reduced cost (row vector). Denoted as c_j(u) in [1], right after equation
// (5). For a subset j, c_j(u) = c_j - sum_{i \in I_j}.u_i. I_j is the set of
// indices for elements in subset j. For a general Integer Program A.x <= b,
// this would be:
//         c_j(u) = c_j - sum_{i \in I_j} a_{ij}.u_i
SubsetCostVector SetCoverLagrangian::ComputeReducedCosts(
    const SubsetCostVector& costs, const ElementCostVector& multipliers) const {
  const SparseColumnView& columns = model()->columns();
  SubsetCostVector reduced_costs(costs.size());
  FillReducedCostsSlice(SubsetIndex(0), SubsetIndex(reduced_costs.size()),
                        costs, multipliers, columns, &reduced_costs);
  return reduced_costs;
}

namespace {
// Helper function to compute the subgradient.
// It fills a slice of the subgradient vector from indices slice_start to
// slice_end. This is a helper function for ParallelComputeSubgradient(). The
// subgradient is computed using the reduced costs vector.
void FillSubgradientSlice(SubsetIndex slice_start, SubsetIndex slice_end,
                          const SparseColumnView& columns,
                          const SubsetCostVector& reduced_costs,
                          ElementCostVector* subgradient) {
  for (SubsetIndex subset(slice_start); subset < slice_end; ++subset) {
    if (reduced_costs[subset] < 0.0) {
      for (const ElementIndex element : columns[subset]) {
        (*subgradient)[element] -= 1.0;
      }
    }
  }
}
}  // namespace

// Vector of primal slack variable. Denoted as s_i(u) in [1], equation (6).
// For all element indices i, s_i(u) = 1 - sum_{j \in J_i} x_j(u),
// where J_i denotes the set of indices of subsets j covering element i.
// For a general Integer Program A x <= b, the subgradient cost vector is
// defined as A x - b. See [2].
ElementCostVector SetCoverLagrangian::ComputeSubgradient(
    const SubsetCostVector& reduced_costs) const {
  // NOTE(user): Should the initialization be done with coverage[element]?
  ElementCostVector subgradient(model()->num_elements(), 1.0);
  FillSubgradientSlice(SubsetIndex(0), SubsetIndex(reduced_costs.size()),
                       model()->columns(), reduced_costs, &subgradient);
  return subgradient;
}

ElementCostVector SetCoverLagrangian::ParallelComputeSubgradient(
    const SubsetCostVector& reduced_costs) const {
  const SubsetIndex num_subsets(model()->num_subsets());
  const SparseColumnView& columns = model()->columns();
  ElementCostVector subgradient(model()->num_elements(), 1.0);
  // The subgradient has one component per element, each thread processes
  // several subsets. Hence, have one vector per thread to avoid data races.
  // TODO(user): it may be better to split the elements among the threads,
  // although this might be less well-balanced.
  std::vector<ElementCostVector> subgradients(
      num_threads_, ElementCostVector(model()->num_elements()));
  absl::BlockingCounter num_threads_running(num_threads_);
  const SubsetIndex block_size(BlockSize(num_subsets.value(), num_threads_));
  SubsetIndex slice_start(0);
  for (int thread_index = 0; thread_index < num_threads_; ++thread_index) {
    const SubsetIndex slice_end =
        std::min(slice_start + block_size, num_subsets);
    thread_pool_->Schedule([&num_threads_running, slice_start, slice_end,
                            &reduced_costs, &columns, &subgradients,
                            thread_index]() {
      FillSubgradientSlice(slice_start, slice_end, columns, reduced_costs,
                           &subgradients[thread_index]);
      num_threads_running.DecrementCount();
    });
    slice_start = slice_end;
  }
  num_threads_running.Wait();
  for (int thread_index = 0; thread_index < num_threads_; ++thread_index) {
    for (const ElementIndex element : model()->ElementRange()) {
      subgradient[element] += subgradients[thread_index][element];
    }
  }
  return subgradient;
}

namespace {
// Helper function to compute the value of the Lagrangian.
// This is a helper function for ParallelComputeLagrangianValue().
// It is called on a slice of elements, defined by slice_start and slice_end.
// The value of the Lagrangian is computed using the reduced costs vector and
// the multipliers vector.
// The result is stored in lagrangian_value.
void FillLagrangianValueSlice(SubsetIndex slice_start, SubsetIndex slice_end,
                              const SubsetCostVector& reduced_costs,
                              Cost* lagrangian_value) {
  // This is min \sum_{j \in N} c_j(u) x_j. This captures the remark above
  // (**), taking into account the possible values for x_j, and using them to
  // minimize the terms.
  for (SubsetIndex subset(slice_start); subset < slice_end; ++subset) {
    if (reduced_costs[subset] < 0.0) {
      *lagrangian_value += reduced_costs[subset];
    }
  }
}
}  // namespace

// Compute the (scalar) value of the Lagrangian vector by fixing the value of
// x_j based on the sign of c_j(u). In [1] equation (4), it is:
// L(u) = min \sum_{j \in N} c_j(u) x_j + \sum{i \in M} u_i . This is obtained
// - if c_j(u) < 0: x_j(u) = 1,
// - if c_j(u) > 0: x_j(u) = 0,   (**)
// - if c_j(u) = 0: x_j(u) is unbound, in {0, 1}, we use 0.
// For a general Integer Program A x <= b, the Lagrangian vector L(u) [2] is
// L(u) = min \sum_{j \in N} c_j(u) x_j + \sum{i \in M} b_i.u_i.
Cost SetCoverLagrangian::ComputeLagrangianValue(
    const SubsetCostVector& reduced_costs,
    const ElementCostVector& multipliers) const {
  Cost lagrangian_value = 0.0;
  // This is \sum{i \in M} u_i.
  for (const Cost u : multipliers) {
    lagrangian_value += u;
  }
  FillLagrangianValueSlice(SubsetIndex(0), SubsetIndex(reduced_costs.size()),
                           reduced_costs, &lagrangian_value);
  return lagrangian_value;
}

Cost SetCoverLagrangian::ParallelComputeLagrangianValue(
    const SubsetCostVector& reduced_costs,
    const ElementCostVector& multipliers) const {
  Cost lagrangian_value = 0.0;
  // This is \sum{i \in M} u_i.
  for (const Cost u : multipliers) {
    lagrangian_value += u;
  }
  std::vector<Cost> lagrangian_values(num_threads_, 0.0);
  absl::BlockingCounter num_threads_running(num_threads_);
  const SubsetIndex block_size(BlockSize(model()->num_subsets(), num_threads_));
  const SubsetIndex num_subsets(model()->num_subsets());
  SubsetIndex slice_start(0);
  for (int thread_index = 0; thread_index < num_threads_; ++thread_index) {
    const SubsetIndex slice_end =
        std::min(slice_start + block_size, num_subsets);
    thread_pool_->Schedule([&num_threads_running, slice_start, block_size,
                            num_subsets, thread_index, &reduced_costs,
                            &lagrangian_values]() {
      const SubsetIndex slice_end =
          std::min(slice_start + block_size, num_subsets);
      FillLagrangianValueSlice(slice_start, slice_end, reduced_costs,
                               &lagrangian_values[thread_index]);
      num_threads_running.DecrementCount();
    });
    slice_start = slice_end;
  }
  num_threads_running.Wait();
  for (const Cost l : lagrangian_values) {
    lagrangian_value += l;
  }
  return lagrangian_value;
}

// Perform a subgradient step.
// In the general case, for an Integer Program A.x <=b, the Lagragian
// multipliers vector at step k+1 is defined as: u^{k+1} = u^k + t_k (A x^k -
// b) with term t_k = lambda_k * (UB -  L(u^k)) / |A x^k - b|^2.
// |.| is the 2-norm (i.e. Euclidean)
// In our case, the problem A x <= b is in the form A x >= 1. We need to
// replace A x - b by s_i(u) = 1 - sum_{j \in J_i} x_j(u).
// |A x^k - b|^2 = |s(u)|^2, and t_k is of the form:
// t_k = lambda_k * (UB - L(u^k)) / |s^k(u)|^2.
// Now, the coordinates of the multipliers vectors u^k, u^k_i are nonnegative,
// i.e. u^k_i >= 0. Negative values are simply cut off. Following [3], each of
// the coordinates is defined as: u^{k+1}_i =
//     max(u^k_i + lambda_k * (UB - L(u^k)) / |s^k(u)|^2 * s^k_i(u), 0).
// This is eq. (7) in [1].
void SetCoverLagrangian::UpdateMultipliers(
    double step_size, Cost lagrangian_value, Cost upper_bound,
    const SubsetCostVector& reduced_costs,
    ElementCostVector* multipliers) const {
  // step_size is \lambda_k in [1].
  DCHECK_GT(step_size, 0);
  // Compute the square of the Euclidean norm of the subgradient vector.
  const ElementCostVector subgradient = ComputeSubgradient(reduced_costs);
  Cost subgradient_square_norm = 0.0;
  for (const Cost x : subgradient) {
    subgradient_square_norm += x * x;
  }
  // First compute lambda_k * (UB - L(u^k)).
  const Cost factor =
      step_size * (upper_bound - lagrangian_value) / subgradient_square_norm;
  for (const ElementIndex element : model()->ElementRange()) {
    // Avoid multipliers to go negative and to go over the roof. 1e6 chosen
    // arbitrarily. [***]
    (*multipliers)[element] = std::clamp(
        (*multipliers)[element] + factor * subgradient[element], 0.0, 1e6);
  }
}

void SetCoverLagrangian::ParallelUpdateMultipliers(
    double step_size, Cost lagrangian_value, Cost upper_bound,
    const SubsetCostVector& reduced_costs,
    ElementCostVector* multipliers) const {
  // step_size is \lambda_k in [1].
  DCHECK_GT(step_size, 0);
  // Compute the square of the Euclidean norm of the subgradient vector.
  const ElementCostVector subgradient =
      ParallelComputeSubgradient(reduced_costs);
  Cost subgradient_square_norm = 0.0;
  for (const Cost x : subgradient) {
    subgradient_square_norm += x * x;
  }
  // First compute lambda_k * (UB - L(u^k)).
  const Cost factor =
      step_size * (upper_bound - lagrangian_value) / subgradient_square_norm;
  for (const ElementIndex element : model()->ElementRange()) {
    // Avoid multipliers to go negative and to go through the roof. 1e6 chosen
    const Cost kRoof = 1e6;  // Arbitrary value, from [1].
    (*multipliers)[element] = std::clamp(
        (*multipliers)[element] + factor * subgradient[element], 0.0, kRoof);
  }
}

Cost SetCoverLagrangian::ComputeGap(
    const SubsetCostVector& reduced_costs, const SubsetBoolVector& solution,
    const ElementCostVector& multipliers) const {
  Cost gap = 0.0;
  // TODO(user): Parallelize this, if need be.
  for (const SubsetIndex subset : model()->SubsetRange()) {
    if (solution[subset] && reduced_costs[subset] > 0.0) {
      gap += reduced_costs[subset];
    } else if (!solution[subset] && reduced_costs[subset] < 0.0) {
      // gap += std::abs(reduced_costs[subset]);  We know the sign of rhs.
      gap -= reduced_costs[subset];
    }
  }
  const ElementToIntVector& coverage = inv()->coverage();
  for (const ElementIndex element : model()->ElementRange()) {
    gap += (coverage[element] - 1) * multipliers[element];
  }
  return gap;
}

SubsetCostVector SetCoverLagrangian::ComputeDelta(
    const SubsetCostVector& reduced_costs,
    const ElementCostVector& multipliers) const {
  SubsetCostVector delta(model()->num_subsets());
  const ElementToIntVector& coverage = inv()->coverage();
  // This is definition (9) in [1].
  const SparseColumnView& columns = model()->columns();
  // TODO(user): Parallelize this.
  for (const SubsetIndex subset : model()->SubsetRange()) {
    delta[subset] = std::max(reduced_costs[subset], 0.0);
    for (const ElementIndex element : columns[subset]) {
      const double size = coverage[element];
      delta[subset] += multipliers[element] * (size - 1.0) / size;
    }
  }
  return delta;
}

namespace {
// Helper class to compute the step size for the multipliers.
// The step size is updated every period iterations.
// The step size is updated by multiplying it by a factor 0.5 or 1.5
// on the relative change in the lower bound is greater than 0.01 or less
// than 0.001, respectively. The lower bound is updated every period
// iterations.
class StepSizer {
 public:
  StepSizer(int period, double step_size)
      : period_(period), iter_to_check_(period), step_size_(step_size) {
    ResetBounds();
  }
  double UpdateStepSize(int iter, Cost lower_bound) {
    min_lb_ = std::min(min_lb_, lower_bound);
    max_lb_ = std::max(max_lb_, lower_bound);
    if (iter == iter_to_check_) {
      iter_to_check_ += period_;
      // Bounds can be negative, so we need to take the absolute value.
      // We also need to avoid division by zero. We decide to return 0.0 in
      // that case, which is the same as not updating the step size.
      const Cost lb_gap =
          max_lb_ == 0.0 ? 0.0 : (max_lb_ - min_lb_) / std::abs(max_lb_);
      DCHECK_GE(lb_gap, 0.0);
      if (lb_gap > 0.01) {
        step_size_ *= 0.5;
      } else if (lb_gap <= 0.001) {
        step_size_ *= 1.5;
      }
      step_size_ = std::clamp(step_size_, 1e-6, 10.0);
      ResetBounds();
    }
    return step_size_;
  }

 private:
  void ResetBounds() {
    min_lb_ = std::numeric_limits<Cost>::infinity();
    max_lb_ = -std::numeric_limits<Cost>::infinity();
  }
  int period_;
  int iter_to_check_;
  double step_size_;
  Cost min_lb_;
  Cost max_lb_;
};

// Helper class to decide whether to stop the algorithm.
// The algorithm stops when the lower bound is not updated for a certain
// number of iterations.
class Stopper {
 public:
  explicit Stopper(int period)
      : period_(period),
        iter_to_check_(period),
        lower_bound_(std::numeric_limits<Cost>::min()) {}
  bool DecideWhetherToStop(int iter, Cost lb) {
    DCHECK_GE(lb, lower_bound_);
    if (iter == iter_to_check_) {
      iter_to_check_ += period_;
      const Cost delta = lb - lower_bound_;
      const Cost relative_delta = delta / lb;
      DCHECK_GE(delta, 0.0);
      DCHECK_GE(relative_delta, 0.0);
      lower_bound_ = lb;
      return relative_delta < 0.001 && delta < 1;
    }
    return false;
  }

 private:
  int period_;
  int iter_to_check_;
  Cost lower_bound_;
};

}  // namespace

namespace {
// TODO(user): Add this to the file defining AdjustableKAryHeap.
template <typename Priority, typename Index, bool IsMaxHeap>
class TopKHeap {
 public:
  explicit TopKHeap(int max_size) : heap_(), max_size_(max_size) {}
  void Clear() { heap_.Clear(); }
  void Add(Priority priority, Index index) {
    if (heap_.Size() < max_size_) {
      heap_.Add(priority, index);
    } else if (heap_.HasPriority(priority, heap_.TopPriority())) {
      heap_.ReplaceTop(priority, index);
    }
  }

 private:
  AdjustableKAryHeap<Priority, Index, /*Arity=*/2, !IsMaxHeap> heap_;
  int max_size_;
};
}  // namespace

// Computes a lower bound on the optimal cost.
std::tuple<Cost, SubsetCostVector, ElementCostVector>
SetCoverLagrangian::ComputeLowerBound(const SubsetCostVector& costs,
                                      Cost upper_bound) {
  StopWatch stop_watch(&run_time_);
  Cost lower_bound = 0.0;
  ElementCostVector multipliers = InitializeLagrangeMultipliers();
  double step_size = 0.1;               // [***] arbitrary, from [1].
  StepSizer step_sizer(20, step_size);  // [***] arbitrary, from [1].
  Stopper stopper(100);                 // [***] arbitrary, from [1].
  SubsetCostVector reduced_costs(costs);
  // For the time being, 4 threads seems to be the fastest.
  // Running linux perf of the process shows that up to 60% of the cycles are
  // lost as idle cycles in the CPU backend, probably because the algorithm is
  // memory bound.
  for (int iter = 0; iter < 1000; ++iter) {
    reduced_costs = ParallelComputeReducedCosts(costs, multipliers);
    const Cost lagrangian_value =
        ParallelComputeLagrangianValue(reduced_costs, multipliers);
    ParallelUpdateMultipliers(step_size, lagrangian_value, upper_bound,
                              reduced_costs, &multipliers);
    lower_bound = std::max(lower_bound, lagrangian_value);
    // step_size should be updated like this. For the time besing, we keep the
    // step size, because the implementation of the rest is not adequate yet
    // step_size = step_sizer.UpdateStepSize(iter, lagrangian_value);
    // if (stopper.DecideWhetherToStop(iter, lower_bound)) {
    //   break;
    // }
  }
  inv()->ReportLowerBound(lower_bound, /*is_cost_consistent=*/false);
  return std::make_tuple(lower_bound, reduced_costs, multipliers);
}

}  // namespace operations_research
