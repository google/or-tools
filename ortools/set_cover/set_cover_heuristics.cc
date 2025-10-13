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

#include "ortools/set_cover/set_cover_heuristics.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

#include "absl/base/casts.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/numeric/bits.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/types/span.h"
#include "ortools/algorithms/adjustable_k_ary_heap.h"
#include "ortools/base/logging.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_invariant.h"
#include "ortools/set_cover/set_cover_model.h"

namespace operations_research {

constexpr SubsetIndex kNotFound(-1);
static constexpr Cost kMaxPossibleCost = std::numeric_limits<Cost>::max();
static constexpr double kInfinity = std::numeric_limits<float>::infinity();

using CL = SetCoverInvariant::ConsistencyLevel;

bool SetCoverSolutionGenerator::CheckInvariantConsistency() const {
  return inv_->CheckConsistency(consistency_level_);
}

// TrivialSolutionGenerator.

bool TrivialSolutionGenerator::NextSolution(
    absl::Span<const SubsetIndex> focus) {
  StopWatch stop_watch(&run_time_);
  const SubsetIndex num_subsets(model()->num_subsets());
  SubsetBoolVector choices(num_subsets, false);
  for (const SubsetIndex subset : focus) {
    choices[subset] = true;
  }
  inv()->LoadSolution(choices);
  inv()->Recompute(CL::kCostAndCoverage);
  return true;
}

// RandomSolutionGenerator.

bool RandomSolutionGenerator::NextSolution(
    absl::Span<const SubsetIndex> focus) {
  StopWatch stop_watch(&run_time_);
  inv()->ClearTrace();
  std::vector<SubsetIndex> shuffled(focus.begin(), focus.end());
  std::shuffle(shuffled.begin(), shuffled.end(), absl::BitGen());
  for (const SubsetIndex subset : shuffled) {
    if (inv()->is_selected()[subset]) continue;
    if (inv()->num_free_elements()[subset] != 0) {
      inv()->Select(subset, CL::kFreeAndUncovered);
    }
  }
  inv()->CompressTrace();
  DCHECK(inv()->CheckConsistency(CL::kFreeAndUncovered));
  return true;
}

// GreedySolutionGenerator.

bool GreedySolutionGenerator::NextSolution(
    absl::Span<const SubsetIndex> focus) {
  StopWatch stop_watch(&run_time_);
  DCHECK(inv()->CheckConsistency(CL::kCostAndCoverage));
  inv()->Recompute(CL::kFreeAndUncovered);
  inv()->ClearTrace();
  std::vector<std::pair<float, SubsetIndex::ValueType>> subset_priorities;
  DVLOG(1) << "focus.size(): " << focus.size();
  subset_priorities.reserve(focus.size());
  const SubsetCostVector& costs = model()->subset_costs();
  for (const SubsetIndex subset : focus) {
    if (!inv()->is_selected()[subset] &&
        inv()->num_free_elements()[subset] != 0) {
      // NOMUTANTS -- reason, for C++
      const float priority = inv()->num_free_elements()[subset] / costs[subset];
      subset_priorities.push_back({priority, subset.value()});
    }
  }
  const BaseInt num_subsets = model()->num_subsets();
  const SparseColumnView& colunms = model()->columns();
  const SparseRowView& rows = model()->rows();
  // The priority queue maintains the maximum number of elements covered by unit
  // of cost. We chose 16 as the arity of the heap after some testing.
  // TODO(user): research more about the best value for Arity.
  AdjustableKAryHeap<float, SubsetIndex::ValueType, 16, true> pq(
      subset_priorities, num_subsets);
  SubsetBoolVector subset_seen(num_subsets, false);
  std::vector<SubsetIndex> subsets_to_remove;
  subsets_to_remove.reserve(focus.size());
  while (!pq.IsEmpty() || inv()->num_uncovered_elements() > 0) {
    VLOG_EVERY_N_SEC(1, 5) << "Queue size: " << pq.heap_size()
                           << ", #uncovered elements: "
                           << inv()->num_uncovered_elements();
    const SubsetIndex best_subset(pq.TopIndex());
    pq.Pop();
    inv()->Select(best_subset, CL::kFreeAndUncovered);
    // NOMUTANTS -- reason, for C++
    subset_seen[best_subset] = true;
    subsets_to_remove.push_back(best_subset);
    for (const ElementIndex element : colunms[best_subset]) {
      for (const SubsetIndex subset : rows[element]) {
        if (subset_seen[subset]) continue;
        subset_seen[subset] = true;
        const BaseInt marginal_impact(inv()->num_free_elements()[subset]);
        if (marginal_impact > 0) {
          const float priority = marginal_impact / costs[subset];
          pq.Update({priority, subset.value()});
        } else {
          pq.Remove(subset.value());
        }
        subsets_to_remove.push_back(subset);
      }
    }
    for (const SubsetIndex subset : subsets_to_remove) {
      subset_seen[subset] = false;
    }
    subsets_to_remove.clear();
    DVLOG(1) << "Cost = " << inv()->cost()
             << " num_uncovered_elements = " << inv()->num_uncovered_elements();
  }
  inv()->CompressTrace();
  // Don't expect pq to be empty, because of the break in the while loop.
  DCHECK(inv()->CheckConsistency(CL::kFreeAndUncovered));
  return true;
}

namespace {
// This class gathers statistics about the usefulness of the ratio computation.
class ComputationUsefulnessStats {
 public:
  // If is_active is true, the stats are gathered, otherwise there is no
  // overhead, in particular no memory allocation.
  explicit ComputationUsefulnessStats(const SetCoverInvariant* inv,
                                      bool is_active)
      : inv_(inv),
        is_active_(is_active),
        num_ratio_computations_(),
        num_useless_computations_(),
        num_free_elements_() {
    if (is_active) {
      BaseInt num_subsets = inv_->model()->num_subsets();
      num_ratio_computations_.assign(num_subsets, 0);
      num_useless_computations_.assign(num_subsets, 0);
      num_free_elements_.assign(num_subsets, -1);  // -1 means not computed yet.
    }
  }

  // To be called each time a num_free_elements is computed.
  void Update(SubsetIndex subset, BaseInt new_num_free_elements) {
    if (is_active_) {
      if (new_num_free_elements == num_free_elements_[subset]) {
        ++num_useless_computations_[subset];
      }
      ++num_ratio_computations_[subset];
      num_free_elements_[subset] = new_num_free_elements;
    }
  }

  // To be called at the end of the algorithm.
  void PrintStats() {
    if (is_active_) {
      BaseInt num_subsets_considered = 0;
      BaseInt num_ratio_updates = 0;
      BaseInt num_wasted_ratio_updates = 0;
      for (const SubsetIndex subset : inv_->model()->SubsetRange()) {
        if (num_ratio_computations_[subset] > 0) {
          ++num_subsets_considered;
          if (num_ratio_computations_[subset] > 1) {
            num_ratio_updates += num_ratio_computations_[subset] - 1;
          }
        }
        num_wasted_ratio_updates += num_useless_computations_[subset];
      }
      LOG(INFO) << "num_subsets_considered = " << num_subsets_considered;
      LOG(INFO) << "num_ratio_updates = " << num_ratio_updates;
      LOG(INFO) << "num_wasted_ratio_updates = " << num_wasted_ratio_updates;
    }
  }

 private:
  // The invariant on which the stats are performed.
  const SetCoverInvariant* inv_;

  // Whether the stats are active or not.
  bool is_active_;

  // Number of times the ratio was computed for a subset.
  SubsetToIntVector num_ratio_computations_;

  // Number of times the ratio was computed for a subset and was the same as the
  // previous one.
  SubsetToIntVector num_useless_computations_;

  // The value num_free_elements_ for the subset the last time it was computed.
  // Used to detect useless computations.
  SubsetToIntVector num_free_elements_;
};
}  // namespace

namespace {
// Clearly not the fastest radix sort, but its complexity is the right one.
// Furthermore:
// - it is as memory-safe as std::vectors can be (no pointers),
// - no multiplication is performed,
// - it is stable
// - it handles the cases of signed and unsigned integers automatically,
// - bounds on the keys are optional, or they can be computed automatically,
// - based on those bounds, the number of passes is automatically computed,
// - a payload is associated to each key, and it is sorted in the same way
//   as the keys. This payload can be a vector of integers or a vector of
//   pointers to larger objects.
// TODO(user): Make it an independent library.
// - add support for decreasing counting sort,
// - make payloads optional,
// - support floats and doubles,
// - improve performance.
// - use vectorized code.
namespace internal {
uint32_t RawBits(uint32_t x) { return x; }                           // NOLINT
uint32_t RawBits(int x) { return absl::bit_cast<uint32_t>(x); }      // NOLINT
uint32_t RawBits(float x) { return absl::bit_cast<uint32_t>(x); }    // NOLINT
uint64_t RawBits(uint64_t x) { return x; }                           // NOLINT
uint64_t RawBits(int64_t x) { return absl::bit_cast<uint64_t>(x); }  // NOLINT
uint64_t RawBits(double x) { return absl::bit_cast<uint64_t>(x); }   // NOLINT

inline uint32_t Bucket(uint32_t x, uint32_t shift, uint32_t radix) {
  DCHECK_EQ(0, radix & (radix - 1));  // Must be a power of two.
  // NOMUTANTS -- a way to compute the remainder of a division when radix is a
  // power of two.
  return (RawBits(x) >> shift) & (radix - 1);
}

template <typename T>
int NumBitsToRepresent(T value) {
  DCHECK_LE(absl::countl_zero(RawBits(value)), sizeof(T) * CHAR_BIT);
  return sizeof(T) * CHAR_BIT - absl::countl_zero(RawBits(value));
}

template <typename Key, typename Counter>
void UpdateCounters(uint32_t radix, int shift, std::vector<Key>& keys,
                    std::vector<Counter>& counts) {
  std::fill(counts.begin(), counts.end(), 0);
  DCHECK_EQ(counts[0], 0);
  DCHECK_EQ(0, radix & (radix - 1));  // Must be a power of two.
  const auto num_keys = keys.size();
  for (int64_t i = 0; i < num_keys; ++i) {
    ++counts[Bucket(keys[i], shift, radix)];
  }
  // Now the counts will contain the sum of the sizes below and including each
  // bucket.
  for (uint64_t i = 1; i < radix; ++i) {
    counts[i] += counts[i - 1];
  }
}

template <typename Key, typename Payload, typename Counter>
void IncreasingCountingSort(uint32_t radix, int shift, std::vector<Key>& keys,
                            std::vector<Payload>& payloads,
                            std::vector<Key>& scratch_keys,
                            std::vector<Payload>& scratch_payloads,
                            std::vector<Counter>& counts) {
  DCHECK_EQ(0, radix & (radix - 1));  // Must be a power of two.
  UpdateCounters(radix, shift, keys, counts);
  const auto num_keys = keys.size();
  // In this order for stability.
  for (int64_t i = num_keys - 1; i >= 0; --i) {
    Counter c = --counts[Bucket(keys[i], shift, radix)];
    scratch_keys[c] = keys[i];
    scratch_payloads[c] = payloads[i];
  }
  std::swap(keys, scratch_keys);
  std::swap(payloads, scratch_payloads);
}
}  // namespace internal

template <typename Key, typename Payload>
void RadixSort(int radix_log, std::vector<Key>& keys,
               std::vector<Payload>& payloads, Key /*min_key*/, Key max_key) {
  // range_log is the number of bits necessary to represent the max_key
  // We could as well use max_key - min_key, but it is more expensive to
  // compute.
  const int range_log = internal::NumBitsToRepresent(max_key);
  DCHECK_EQ(internal::NumBitsToRepresent(0), 0);
  DCHECK_LE(internal::NumBitsToRepresent(std::numeric_limits<Key>::max()),
            std::numeric_limits<Key>::digits);
  const int radix = 1 << radix_log;  // By definition.
  std::vector<uint32_t> counters(radix, 0);
  std::vector<Key> scratch_keys(keys.size());
  std::vector<Payload> scratch_payloads(payloads.size());
  for (int shift = 0; shift < range_log; shift += radix_log) {
    DCHECK_LE(1 << shift, max_key);
    internal::IncreasingCountingSort(radix, shift, keys, payloads, scratch_keys,
                                     scratch_payloads, counters);
  }
}

// TODO(user): Move this to SetCoverInvariant.

std::vector<ElementIndex> GetUncoveredElementsSortedByDegree(
    const SetCoverInvariant* const inv) {
  const BaseInt num_elements = inv->model()->num_elements();
  std::vector<ElementIndex> degree_sorted_elements;  // payloads
  degree_sorted_elements.reserve(num_elements);
  std::vector<BaseInt> keys;
  keys.reserve(num_elements);
  const SparseRowView& rows = inv->model()->rows();
  BaseInt max_degree = 0;
  for (const ElementIndex element : inv->model()->ElementRange()) {
    // Already covered elements should not be considered.
    if (inv->coverage()[element] != 0) continue;
    degree_sorted_elements.push_back(element);
    const BaseInt size = rows[element].size();
    max_degree = std::max(max_degree, size);
    keys.push_back(size);
  }
  RadixSort(11, keys, degree_sorted_elements, 1, max_degree);
#ifndef NDEBUG
  BaseInt prev_key = -1;
  for (const auto key : keys) {
    DCHECK_LE(prev_key, key);
    prev_key = key;
  }
#endif
  return degree_sorted_elements;
}

// Computes: d = c1 * n2 - c2 * n1. This is an easy way to compare two ratios
// without having to use a full division.
// If d < 0 then c1 / n1 < c2 / n2,
// If d == 0 then c1 / n1 == c2 / n2, etc...
// NOTE(user): This can be implemented using SSE2 with a gain of 5-10%.
double Determinant(Cost c1, BaseInt n1, Cost c2, BaseInt n2) {
  return c1 * n2 - n1 * c2;
}
}  // namespace

// ElementDegreeSolutionGenerator.
// There is no need to use a priority queue here, as the ratios are computed
// on-demand. Also elements are sorted based on degree once and for all and
// moved past when the elements become already covered.

bool ElementDegreeSolutionGenerator::NextSolution(
    const SubsetBoolVector& in_focus) {
  StopWatch stop_watch(&run_time_);
  DVLOG(1) << "Entering ElementDegreeSolutionGenerator::NextSolution";
  inv()->Recompute(CL::kFreeAndUncovered);
  // Create the list of all the indices in the problem.
  std::vector<ElementIndex> degree_sorted_elements =
      GetUncoveredElementsSortedByDegree(inv());
  ComputationUsefulnessStats stats(inv(), false);
  const SparseRowView& rows = model()->rows();
  const SubsetCostVector& costs = model()->subset_costs();
  for (const ElementIndex element : degree_sorted_elements) {
    // No need to cover an element that is already covered.
    if (inv()->coverage()[element] != 0) continue;
    SubsetIndex best_subset(-1);
    Cost best_subset_cost = 0.0;
    BaseInt best_subset_num_free_elts = 0;
    for (const SubsetIndex subset : rows[element]) {
      if (!in_focus[subset]) continue;
      const BaseInt num_free_elements = inv()->num_free_elements()[subset];
      stats.Update(subset, num_free_elements);
      const Cost det = Determinant(costs[subset], num_free_elements,
                                   best_subset_cost, best_subset_num_free_elts);
      // Compare R = costs[subset] / num_free_elements with
      //         B = best_subset_cost / best_subset_num_free_elts.
      // If R < B, we choose subset.
      // If the ratios are the same, we choose the subset with the most free
      // elements.
      // TODO(user): What about adding a tolerance for equality, which could
      // further favor larger columns?
      if (det < 0 ||
          (det == 0 && num_free_elements > best_subset_num_free_elts)) {
        best_subset = subset;
        best_subset_cost = costs[subset];
        best_subset_num_free_elts = num_free_elements;
      }
    }
    if (best_subset.value() == -1) {
      LOG(WARNING) << "Best subset not found. Algorithmic error or invalid "
                      "input.";
      continue;
    }
    DCHECK_NE(best_subset.value(), -1);
    inv()->Select(best_subset, CL::kFreeAndUncovered);
    DVLOG(1) << "Cost = " << inv()->cost()
             << " num_uncovered_elements = " << inv()->num_uncovered_elements();
  }
  inv()->CompressTrace();
  stats.PrintStats();
  DCHECK(inv()->CheckConsistency(CL::kFreeAndUncovered));
  return true;
}

// LazyElementDegreeSolutionGenerator.
// There is no need to use a priority queue here, as the ratios are computed
// on-demand. Also elements are sorted based on degree once and for all and
// moved past when the elements become already covered.

bool LazyElementDegreeSolutionGenerator::NextSolution(
    const SubsetBoolVector& in_focus) {
  StopWatch stop_watch(&run_time_);
  inv()->CompressTrace();
  DVLOG(1) << "Entering LazyElementDegreeSolutionGenerator::NextSolution";
  DCHECK(inv()->CheckConsistency(CL::kCostAndCoverage));
  // Create the list of all the indices in the problem.
  std::vector<ElementIndex> degree_sorted_elements =
      GetUncoveredElementsSortedByDegree(inv());
  const SparseRowView& rows = model()->rows();
  const SparseColumnView& columns = model()->columns();
  ComputationUsefulnessStats stats(inv(), false);
  const SubsetCostVector& costs = model()->subset_costs();
  for (const ElementIndex element : degree_sorted_elements) {
    // No need to cover an element that is already covered.
    if (inv()->coverage()[element] != 0) continue;
    SubsetIndex best_subset(-1);
    Cost best_subset_cost = 0.0;  // Cost of the best subset.
    BaseInt best_subset_num_free_elts = 0;
    for (const SubsetIndex subset : rows[element]) {
      if (!in_focus[subset]) continue;
      const Cost filtering_det =
          Determinant(costs[subset], columns[subset].size(), best_subset_cost,
                      best_subset_num_free_elts);
      // If the ratio with the initial number elements is greater, we skip this
      // subset.
      if (filtering_det > 0) continue;
      const BaseInt num_free_elements = inv()->ComputeNumFreeElements(subset);
      stats.Update(subset, num_free_elements);
      const Cost det = Determinant(costs[subset], num_free_elements,
                                   best_subset_cost, best_subset_num_free_elts);
      // Same as ElementDegreeSolutionGenerator.
      if (det < 0 ||
          (det == 0 && num_free_elements > best_subset_num_free_elts)) {
        best_subset = subset;
        best_subset_cost = costs[subset];
        best_subset_num_free_elts = num_free_elements;
      }
    }
    DCHECK_NE(best_subset, SubsetIndex(-1));
    inv()->Select(best_subset, CL::kCostAndCoverage);
    DVLOG(1) << "Cost = " << inv()->cost()
             << " num_uncovered_elements = " << inv()->num_uncovered_elements();
  }
  DCHECK(inv()->CheckConsistency(CL::kCostAndCoverage));
  stats.PrintStats();
  return true;
}

// SteepestSearch.

bool SteepestSearch::NextSolution(const SubsetBoolVector& in_focus) {
  StopWatch stop_watch(&run_time_);
  const int64_t num_iterations = max_iterations();
  DCHECK(inv()->CheckConsistency(CL::kCostAndCoverage));
  inv()->Recompute(CL::kFreeAndUncovered);
  DVLOG(1) << "Entering SteepestSearch::NextSolution, num_iterations = "
           << num_iterations;
  // Return false if inv() contains no solution.
  // TODO(user): This should be relaxed for partial solutions.
  if (inv()->num_uncovered_elements() != 0) {
    return false;
  }

  // Create priority queue with cost of using a subset, by decreasing order.
  // Do it only for selected AND removable subsets.
  const SubsetCostVector& costs = model()->subset_costs();
  std::vector<std::pair<float, SubsetIndex::ValueType>> subset_priorities;
  subset_priorities.reserve(in_focus.size());
  for (const SetCoverDecision& decision : inv()->trace()) {
    const SubsetIndex subset = decision.subset();
    if (in_focus[subset] && inv()->is_selected()[subset] &&
        inv()->ComputeIsRedundant(subset)) {
      const float delta_per_element = costs[subset];
      subset_priorities.push_back({delta_per_element, subset.value()});
    }
  }
  DVLOG(1) << "subset_priorities.size(): " << subset_priorities.size();
  AdjustableKAryHeap<float, SubsetIndex::ValueType, 16, true> pq(
      subset_priorities, model()->num_subsets());
  for (int iteration = 0; iteration < num_iterations && !pq.IsEmpty();
       ++iteration) {
    const SubsetIndex best_subset(pq.TopIndex());
    pq.Pop();
    DCHECK(inv()->is_selected()[best_subset]);
    DCHECK(inv()->ComputeIsRedundant(best_subset));
    DCHECK_GT(costs[best_subset], 0.0);
    inv()->Deselect(best_subset, CL::kFreeAndUncovered);
    for (const SubsetIndex subset :
         IntersectingSubsetsRange(*model(), best_subset)) {
      if (!inv()->ComputeIsRedundant(subset)) {
        pq.Remove(subset.value());
      }
    }
    DVLOG(1) << "Cost = " << inv()->cost();
  }
  inv()->CompressTrace();
  // TODO(user): change this to enable working on partial solutions.
  DCHECK_EQ(inv()->num_uncovered_elements(), 0);
  DCHECK(inv()->CheckConsistency(CL::kFreeAndUncovered));
  return true;
}

// LazySteepestSearch.

bool LazySteepestSearch::NextSolution(const SubsetBoolVector& in_focus) {
  StopWatch stop_watch(&run_time_);
  const int64_t num_iterations = max_iterations();
  DCHECK(inv()->CheckConsistency(CL::kCostAndCoverage));
  DVLOG(1) << "Entering LazySteepestSearch::NextSolution, num_iterations = "
           << num_iterations;
  const BaseInt num_selected_subsets = inv()->trace().size();
  std::vector<Cost> selected_costs;
  selected_costs.reserve(num_selected_subsets);
  // First part of the trick:
  // Since the heuristic is greedy, it only considers subsets that are selected
  // and in_focus.
  const SubsetCostVector& costs = model()->subset_costs();
  for (const SetCoverDecision& decision : inv()->trace()) {
    if (in_focus[decision.subset()]) {
      selected_costs.push_back(costs[decision.subset()]);
    }
  }
  std::vector<BaseInt> cost_permutation(selected_costs.size());
  std::iota(cost_permutation.begin(), cost_permutation.end(), 0);
  // TODO(user): use RadixSort with doubles and payloads.
  std::sort(cost_permutation.begin(), cost_permutation.end(),
            [&selected_costs](const BaseInt i, const BaseInt j) {
              if (selected_costs[i] == selected_costs[j]) {
                return i < j;
              }
              return selected_costs[i] > selected_costs[j];
            });
  std::vector<SubsetIndex> cost_sorted_subsets;
  cost_sorted_subsets.reserve(num_selected_subsets);
  for (const BaseInt i : cost_permutation) {
    cost_sorted_subsets.push_back(inv()->trace()[i].subset());
  }
  for (const SubsetIndex subset : cost_sorted_subsets) {
    // Second part of the trick:
    // ComputeIsRedundant is expensive, but it is going to be called only once
    // per subset in the solution. Once this has been done, there is no need to
    // update any priority queue, nor to use a stronger level of consistency
    // than kCostAndCoverage.
    // In the non-lazy version, the redundancy of a subset may be updated many
    // times and the priority queue must be updated accordingly, including just
    // for removing the subset that was just considered,
    // A possible optimization would be to sort the elements by coverage and
    // run ComputeIsRedundant with the new element order. This would make the
    // subsets which cover only one element easier to prove non-redundant.
    if (inv()->is_selected()[subset] && inv()->ComputeIsRedundant(subset)) {
      inv()->Deselect(subset, CL::kCostAndCoverage);
    }
  }
  inv()->CompressTrace();
  DCHECK(inv()->CheckConsistency(CL::kCostAndCoverage));
  return true;
}

// Guided Tabu Search

void GuidedTabuSearch::Initialize() {
  const SubsetIndex num_subsets(model()->num_subsets());
  const SubsetCostVector& subset_costs = model()->subset_costs();
  times_penalized_.assign(num_subsets.value(), 0);
  augmented_costs_ = subset_costs;
  utilities_ = subset_costs;
}

namespace {
bool FlipCoin() {
  // TODO(user): use STL for repeatable testing.
  return absl::Bernoulli(absl::BitGen(), 0.5);
}
}  // namespace

void GuidedTabuSearch::UpdatePenalties(absl::Span<const SubsetIndex> focus) {
  const SubsetCostVector& subset_costs = model()->subset_costs();
  Cost max_utility = -1.0;
  for (const SubsetIndex subset : focus) {
    if (inv()->is_selected()[subset]) {
      max_utility = std::max(max_utility, utilities_[subset]);
    }
  }
  const double epsilon_utility = epsilon_ * max_utility;
  for (const SubsetIndex subset : focus) {
    if (inv()->is_selected()[subset]) {
      const double utility = utilities_[subset];
      if ((max_utility - utility <= epsilon_utility) && FlipCoin()) {
        ++times_penalized_[subset];
        const int times_penalized = times_penalized_[subset];
        const Cost cost =
            subset_costs[subset];  // / columns[subset].size().value();
        utilities_[subset] = cost / (1 + times_penalized);
        augmented_costs_[subset] =
            cost * (1 + penalty_factor_ * times_penalized);
      }
    }
  }
}

bool GuidedTabuSearch::NextSolution(absl::Span<const SubsetIndex> focus) {
  StopWatch stop_watch(&run_time_);
  const int64_t num_iterations = max_iterations();
  DCHECK(inv()->CheckConsistency(CL::kFreeAndUncovered));
  DVLOG(1) << "Entering GuidedTabuSearch::NextSolution, num_iterations = "
           << num_iterations;
  const SubsetCostVector& subset_costs = model()->subset_costs();
  Cost best_cost = inv()->cost();
  SubsetBoolVector best_choices = inv()->is_selected();
  Cost augmented_cost =
      std::accumulate(augmented_costs_.begin(), augmented_costs_.end(), 0.0);
  BaseInt trace_size = inv()->trace().size();
  for (int iteration = 0; iteration < num_iterations; ++iteration) {
    if (inv()->trace().size() > 2 * trace_size) {
      inv()->CompressTrace();
      trace_size = inv()->trace().size();
    }
    Cost best_delta = kMaxPossibleCost;
    SubsetIndex best_subset = kNotFound;
    for (const SubsetIndex subset : focus) {
      const Cost delta = augmented_costs_[subset];
      DVLOG(1) << "Subset, " << subset.value() << ", at ,"
               << inv()->is_selected()[subset] << ", delta =, " << delta
               << ", best_delta =, " << best_delta;
      if (inv()->is_selected()[subset]) {
        // Try to remove subset from solution, if the gain from removing is
        // worth it:
        if (-delta < best_delta &&
            // and it can be removed, and
            inv()->ComputeIsRedundant(subset) &&
            // it is not Tabu OR decreases the actual cost (aspiration):
            (!tabu_list_.Contains(subset) ||
             inv()->cost() - subset_costs[subset] < best_cost)) {
          best_delta = -delta;
          best_subset = subset;
        }
      } else {
        // Try to use subset in solution, if its penalized delta is good.
        if (delta < best_delta) {
          // The limit kMaxPossibleCost is ill-defined,
          // there is always a best_subset. Is it intended?
          if (!tabu_list_.Contains(subset)) {
            best_delta = delta;
            best_subset = subset;
          }
        }
      }
    }
    if (best_subset == kNotFound) {  // Local minimum reached.
      inv()->LoadSolution(best_choices);
      return true;
    }
    DVLOG(1) << "Best subset, " << best_subset.value() << ", at ,"
             << inv()->is_selected()[best_subset] << ", best_delta = ,"
             << best_delta;

    UpdatePenalties(focus);
    tabu_list_.Add(best_subset);
    if (inv()->is_selected()[best_subset]) {
      inv()->Deselect(best_subset, CL::kFreeAndUncovered);
    } else {
      inv()->Select(best_subset, CL::kFreeAndUncovered);
    }
    // TODO(user): make the cost computation incremental.
    augmented_cost =
        std::accumulate(augmented_costs_.begin(), augmented_costs_.end(), 0.0);

    DVLOG(1) << "Iteration, " << iteration << ", current cost = ,"
             << inv()->cost() << ", best cost = ," << best_cost
             << ", penalized cost = ," << augmented_cost;
    if (inv()->cost() < best_cost) {
      VLOG(1) << "Updated best cost, " << "Iteration, " << iteration
              << ", current cost = ," << inv()->cost() << ", best cost = ,"
              << best_cost << ", penalized cost = ," << augmented_cost;
      best_cost = inv()->cost();
      best_choices = inv()->is_selected();
    }
  }
  inv()->LoadSolution(best_choices);
  inv()->CompressTrace();
  DCHECK(inv()->CheckConsistency(CL::kFreeAndUncovered));
  return true;
}

// Guided Local Search
void GuidedLocalSearch::Initialize() {
  const SparseColumnView& columns = model()->columns();
  penalties_.assign(columns.size(), 0);
  penalization_factor_ = alpha_ * inv()->cost() * 1.0 / (columns.size());
  for (const SetCoverDecision& decision : inv()->trace()) {
    const SubsetIndex subset = decision.subset();
    if (inv()->is_selected()[subset]) {
      utility_heap_.Insert({static_cast<float>(model()->subset_costs()[subset] /
                                               (1 + penalties_[subset])),
                            subset.value()});
    }
  }
}

Cost GuidedLocalSearch::ComputeDelta(SubsetIndex subset) const {
  const float delta = (penalization_factor_ * penalties_[subset] +
                       model()->subset_costs()[subset]);
  if (inv()->is_selected()[subset] && inv()->ComputeIsRedundant(subset)) {
    return delta;
  } else if (!inv()->is_selected()[subset]) {
    return -delta;
  }
  return kInfinity;
}

bool GuidedLocalSearch::NextSolution(absl::Span<const SubsetIndex> focus) {
  StopWatch stop_watch(&run_time_);
  const int64_t num_iterations = max_iterations();
  inv()->Recompute(CL::kRedundancy);
  Cost best_cost = inv()->cost();
  SubsetBoolVector best_choices = inv()->is_selected();

  for (const SubsetIndex& subset : focus) {
    const float delta = ComputeDelta(subset);
    if (delta < kInfinity) {
      priority_heap_.Insert({delta, subset.value()});
    }
  }

  for (int iteration = 0;
       !priority_heap_.IsEmpty() && iteration < num_iterations; ++iteration) {
    // Improve current solution respective to the current penalties by flipping
    // the best subset.
    const SubsetIndex best_subset(priority_heap_.TopIndex());
    if (inv()->is_selected()[best_subset]) {
      utility_heap_.Insert({0, best_subset.value()});
      inv()->Deselect(best_subset, CL::kRedundancy);
    } else {
      utility_heap_.Insert(
          {static_cast<float>(model()->subset_costs()[best_subset] /
                              (1 + penalties_[best_subset])),
           best_subset.value()});
      inv()->Select(best_subset, CL::kRedundancy);
    }
    DCHECK(!utility_heap_.IsEmpty());

    // Getting the subset with highest utility. utility_heap_ is not empty,
    // because we just inserted a pair.
    const SubsetIndex penalized_subset(utility_heap_.TopIndex());
    utility_heap_.Pop();
    ++penalties_[penalized_subset];
    utility_heap_.Insert(
        {static_cast<float>(model()->subset_costs()[penalized_subset] /
                            (1 + penalties_[penalized_subset])),
         penalized_subset.value()});
    DCHECK(!utility_heap_.IsEmpty());

    // Get removable subsets (Add them to the heap).
    for (const SubsetIndex subset : inv()->newly_removable_subsets()) {
      const float delta_selected = (penalization_factor_ * penalties_[subset] +
                                    model()->subset_costs()[subset]);
      priority_heap_.Insert({delta_selected, subset.value()});
    }
    DCHECK(!priority_heap_.IsEmpty());

    for (const SubsetIndex subset : {penalized_subset, best_subset}) {
      const float delta = ComputeDelta(subset);
      if (delta < kInfinity) {
        priority_heap_.Insert({delta, subset.value()});
      }
    }
    DCHECK(!priority_heap_.IsEmpty());

    // Get new non removable subsets and remove them from the heap.
    // This is when the priority_heap_ can become empty and end the outer loop
    // early.
    for (const SubsetIndex subset : inv()->newly_non_removable_subsets()) {
      priority_heap_.Remove(subset.value());
    }

    if (inv()->cost() < best_cost) {
      best_cost = inv()->cost();
      best_choices = inv()->is_selected();
    }
  }
  inv()->LoadSolution(best_choices);

  // Improve the solution by removing redundant subsets.
  for (const SubsetIndex& subset : focus) {
    if (inv()->is_selected()[subset] && inv()->ComputeIsRedundant(subset))
      inv()->Deselect(subset, CL::kRedundancy);
  }
  DCHECK_EQ(inv()->num_uncovered_elements(), 0);
  return true;
}

namespace {
void SampleSubsets(std::vector<SubsetIndex>* list, BaseInt num_subsets) {
  num_subsets = std::min<BaseInt>(num_subsets, list->size());
  CHECK_GE(num_subsets, 0);
  std::shuffle(list->begin(), list->end(), absl::BitGen());
  list->resize(num_subsets);
}
}  // namespace

std::vector<SubsetIndex> ClearRandomSubsets(BaseInt num_subsets,
                                            SetCoverInvariant* inv) {
  return ClearRandomSubsets(inv->model()->all_subsets(), num_subsets, inv);
}

std::vector<SubsetIndex> ClearRandomSubsets(absl::Span<const SubsetIndex> focus,
                                            BaseInt num_subsets_to_choose,
                                            SetCoverInvariant* inv) {
  num_subsets_to_choose =
      std::min<BaseInt>(num_subsets_to_choose, focus.size());
  CHECK_GE(num_subsets_to_choose, 0);
  std::vector<SubsetIndex> chosen_indices;
  for (const SubsetIndex subset : focus) {
    if (inv->is_selected()[subset]) {
      chosen_indices.push_back(subset);
    }
  }
  SampleSubsets(&chosen_indices, num_subsets_to_choose);
  BaseInt num_deselected = 0;
  for (const SubsetIndex subset : chosen_indices) {
    if (inv->is_selected()[subset]) {  // subset may have been deselected in a
                                       // previous iteration.
      inv->Deselect(subset, CL::kCostAndCoverage);
      ++num_deselected;
    }
    for (const SubsetIndex connected_subset :
         IntersectingSubsetsRange(*inv->model(), subset)) {
      // connected_subset may have been deselected in a previous iteration.
      if (inv->is_selected()[connected_subset]) {
        inv->Deselect(connected_subset, CL::kCostAndCoverage);
        ++num_deselected;
      }
    }
    // Note that num_deselected may exceed num_subsets_to_choose by more than 1.
    if (num_deselected > num_subsets_to_choose) break;
  }
  return chosen_indices;
}

std::vector<SubsetIndex> ClearMostCoveredElements(BaseInt max_num_subsets,
                                                  SetCoverInvariant* inv) {
  return ClearMostCoveredElements(inv->model()->all_subsets(), max_num_subsets,
                                  inv);
}

std::vector<SubsetIndex> ClearMostCoveredElements(
    absl::Span<const SubsetIndex> focus, BaseInt max_num_subsets,
    SetCoverInvariant* inv) {
  // This is the vector we will return.
  std::vector<SubsetIndex> sampled_subsets;

  const ElementToIntVector& coverage = inv->coverage();
  const BaseInt num_subsets = inv->model()->num_subsets();
  const SparseRowView& rows = inv->model()->rows();

  // Collect the sets which have at least one element whose coverage > 1,
  // even if those sets are not removable.
  SubsetBoolVector subset_is_collected(num_subsets, false);
  for (const ElementIndex element : inv->model()->ElementRange()) {
    if (coverage[element] <= 1) continue;
    for (const SubsetIndex subset : rows[element]) {
      if (inv->is_selected()[subset] && !subset_is_collected[subset]) {
        subset_is_collected[subset] = true;
      }
    }
  }

  // Now intersect with focus: sampled_subsets = focus ⋂ impacted_subsets.
  // NOTE(user): this might take too long. TODO(user):find another algorithm if
  // necessary.
  for (const SubsetIndex subset : focus) {
    if (subset_is_collected[subset]) {
      sampled_subsets.push_back(subset);
    }
  }

  // Actually *sample* sampled_subset.
  // TODO(user): find another algorithm if necessary.
  std::shuffle(sampled_subsets.begin(), sampled_subsets.end(), absl::BitGen());
  sampled_subsets.resize(
      std::min<BaseInt>(sampled_subsets.size(), max_num_subsets));

  // Testing has shown that sorting sampled_subsets is not necessary.
  // Now, un-select the subset in sampled_subsets.
  for (const SubsetIndex subset : sampled_subsets) {
    inv->Deselect(subset, CL::kCostAndCoverage);
  }
  return sampled_subsets;
}

}  // namespace operations_research
