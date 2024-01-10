// Copyright 2010-2024 Google LLC
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

#include "ortools/sat/2d_orthogonal_packing.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/vlog_is_on.h"
#include "ortools/sat/integer.h"

namespace operations_research {
namespace sat {

OrthogonalPackingInfeasibilityDetector::
    ~OrthogonalPackingInfeasibilityDetector() {
  if (!VLOG_IS_ON(1)) return;
  std::vector<std::pair<std::string, int64_t>> stats;
  stats.push_back(
      {"OrthogonalPackingInfeasibilityDetector/called", num_calls_});
  stats.push_back(
      {"OrthogonalPackingInfeasibilityDetector/conflicts", num_conflicts_});
  stats.push_back({"OrthogonalPackingInfeasibilityDetector/trivial_conflicts",
                   num_trivial_conflicts_});
  stats.push_back({"OrthogonalPackingInfeasibilityDetector/conflicts_two_items",
                   num_conflicts_two_items_});

  shared_stats_->AddStats(stats);
}

namespace {
std::optional<std::pair<int, int>> FindPairwiseConflict(
    absl::Span<const IntegerValue> sizes_x,
    absl::Span<const IntegerValue> sizes_y,
    std::pair<IntegerValue, IntegerValue> bounding_box_size,
    const std::vector<int>& index_by_decreasing_x_size,
    const std::vector<int>& index_by_decreasing_y_size) {
  // Look for pairwise incompatible pairs by using the logic such conflict can
  // only happen between a "tall" item a "wide" item.
  int x_idx = 0;
  int y_idx = 0;
  while (x_idx < index_by_decreasing_x_size.size() &&
         y_idx < index_by_decreasing_y_size.size()) {
    if (index_by_decreasing_x_size[x_idx] ==
        index_by_decreasing_y_size[y_idx]) {
      if (sizes_x[index_by_decreasing_x_size[x_idx]] >
          sizes_y[index_by_decreasing_x_size[x_idx]]) {
        y_idx++;
      } else {
        x_idx++;
      }
      continue;
    }
    const bool overlap_on_x = (sizes_x[index_by_decreasing_x_size[x_idx]] +
                                   sizes_x[index_by_decreasing_y_size[y_idx]] >
                               bounding_box_size.first);
    const bool overlap_on_y = (sizes_y[index_by_decreasing_x_size[x_idx]] +
                                   sizes_y[index_by_decreasing_y_size[y_idx]] >
                               bounding_box_size.second);
    if (overlap_on_x && overlap_on_y) {
      return std::make_pair(index_by_decreasing_x_size[x_idx],
                            index_by_decreasing_y_size[y_idx]);
    } else if (overlap_on_x) {
      x_idx++;
    } else if (overlap_on_y) {
      y_idx++;
    } else {
      y_idx++;
    }
  }
  return std::nullopt;
}

// Check for conflict using the f_0 dual feasible function described on Côté,
// Jean-François, Mohamed Haouari, and Manuel Iori. "A primal decomposition
// algorithm for the two-dimensional bin packing problem." arXiv preprint
// arXiv:1909.06835 (2019). This function tries all possible values of the `k`
// parameter.
//
// The `f_0^k(x)` function for a total bin width `C` and a parameter `k` taking
// values in [0, C/2] is defined as:
//
//            / C, if x > C - k,
// f_0^k(x) = | x, if k <= x <= C - k,
//            \ 0, if x < k.
//
// If a conflict is found, this replaces the conflict in `result` if it detects
// a conflict using less items or if `result` is empty.
// The algorithm is the same if we swap the x and y dimension.
void GetOrReplaceDffConflict(absl::Span<const IntegerValue> sizes_x,
                             absl::Span<const IntegerValue> sizes_y,
                             absl::Span<const int> index_by_decreasing_x_size,
                             IntegerValue x_bb_size, IntegerValue total_energy,
                             IntegerValue bb_area, std::vector<int>* result) {
  // If we found a conflict for a k parameter, which is rare, recompute the
  // total used energy consumed by the items to find the minimal set of
  // conflicting items.
  int num_items = sizes_x.size();
  auto add_result_if_better = [&result, &sizes_x, &sizes_y, num_items,
                               &x_bb_size, &bb_area](const IntegerValue k) {
    std::vector<std::pair<int, IntegerValue>> index_to_energy;
    index_to_energy.reserve(num_items);
    for (int i = 0; i < num_items; i++) {
      IntegerValue x_size = sizes_x[i];
      if (x_size > x_bb_size - k) {
        x_size = x_bb_size;
      } else if (x_size < k) {
        continue;
      }
      index_to_energy.push_back({i, x_size * sizes_y[i]});
    }
    std::sort(index_to_energy.begin(), index_to_energy.end(),
              [](const std::pair<int, IntegerValue>& a,
                 const std::pair<int, IntegerValue>& b) {
                return a.second > b.second;
              });
    IntegerValue recomputed_energy = 0;
    for (int i = 0; i < index_to_energy.size(); i++) {
      recomputed_energy += index_to_energy[i].second;
      if (recomputed_energy > bb_area) {
        if (result->empty() || i + 1 < result->size()) {
          result->resize(i + 1);
          for (int j = 0; j <= i; j++) {
            (*result)[j] = index_to_energy[j].first;
          }
        }
        break;
      }
    }
  };

  // One thing we use in this implementation is that not all values of k are
  // interesting: what can cause an energy conflict is increasing the size of
  // the large items, removing the small ones makes it less constrained and we
  // do it only to preserve correctness. Thus, it is enough to check the values
  // of k that are just small enough to enlarge a large item. That means that
  // large items and small ones are not symmetric with respect to what values of
  // k are important.
  IntegerValue current_energy = total_energy;
  // We keep an index on the largest item yet-to-be enlarged and the smallest
  // one yet-to-be removed.
  int removing_item_index = index_by_decreasing_x_size.size() - 1;
  int enlarging_item_index = 0;
  while (enlarging_item_index < index_by_decreasing_x_size.size()) {
    int index = index_by_decreasing_x_size[enlarging_item_index];
    IntegerValue size = sizes_x[index];
    if (size <= x_bb_size / 2) {
      break;
    }
    // Note that since `size_x` is decreasing, we test increasingly large
    // values of k. Also note that a item with size `k` cannot fit alongside
    // an item with size `size_x`, but smaller ones can.
    const IntegerValue k = x_bb_size - size + 1;
    // First, add the area contribution of enlarging the all the items of size
    // exactly size_x. All larger items were already enlarged in the previous
    // iterations.
    do {
      index = index_by_decreasing_x_size[enlarging_item_index];
      size = sizes_x[index];
      current_energy += (x_bb_size - size) * sizes_y[index];
      enlarging_item_index++;
    } while (enlarging_item_index < index_by_decreasing_x_size.size() &&
             sizes_x[index_by_decreasing_x_size[enlarging_item_index]] == size);

    // Now remove the area contribution of removing all the items smaller than
    // k that were not removed before.
    while (removing_item_index >= 0 &&
           sizes_x[index_by_decreasing_x_size[removing_item_index]] < k) {
      const int remove_idx = index_by_decreasing_x_size[removing_item_index];
      current_energy -= sizes_x[remove_idx] * sizes_y[remove_idx];
      removing_item_index--;
    }

    if (current_energy > bb_area) {
      add_result_if_better(k);
    }
  }
}

}  // namespace

OrthogonalPackingInfeasibilityDetector::Result
OrthogonalPackingInfeasibilityDetector::TestFeasibility(
    absl::Span<const IntegerValue> sizes_x,
    absl::Span<const IntegerValue> sizes_y,
    std::pair<IntegerValue, IntegerValue> bounding_box_size) {
  num_calls_++;

  const int num_items = sizes_x.size();
  DCHECK_EQ(num_items, sizes_y.size());
  const IntegerValue bb_area =
      bounding_box_size.first * bounding_box_size.second;
  IntegerValue total_energy = 0;

  index_by_decreasing_x_size_.resize(num_items);
  index_by_decreasing_y_size_.resize(num_items);
  for (int i = 0; i < num_items; i++) {
    total_energy += sizes_x[i] * sizes_y[i];
    index_by_decreasing_x_size_[i] = i;
    index_by_decreasing_y_size_[i] = i;
    if (sizes_x[i] > bounding_box_size.first ||
        sizes_y[i] > bounding_box_size.second) {
      num_conflicts_++;
      return Result{.result = Status::INFEASIBLE,
                    .minimum_unfeasible_subproblem = {i}};
    }
  }

  if (num_items <= 1) {
    return Result{.result = Status::FEASIBLE};
  }

  std::sort(index_by_decreasing_x_size_.begin(),
            index_by_decreasing_x_size_.end(),
            [&sizes_x](int a, int b) { return sizes_x[a] > sizes_x[b]; });
  std::sort(index_by_decreasing_y_size_.begin(),
            index_by_decreasing_y_size_.end(),
            [&sizes_y](int a, int b) { return sizes_y[a] > sizes_y[b]; });

  // First look for pairwise incompatible pairs.
  if (auto pair = FindPairwiseConflict(sizes_x, sizes_y, bounding_box_size,
                                       index_by_decreasing_x_size_,
                                       index_by_decreasing_y_size_);
      pair.has_value()) {
    num_conflicts_++;
    num_conflicts_two_items_++;
    return Result{.result = Status::INFEASIBLE,
                  .minimum_unfeasible_subproblem = {pair.value().first,
                                                    pair.value().second}};
  }

  if (num_items == 2) {
    return Result{.result = Status::FEASIBLE};
  }

  // If there is no pairwise incompatible pairs, this DFF cannot find a conflict
  // by enlarging a item on both x and y directions: this would create an item
  // as long as the whole box and another item as high as the whole box, which
  // is obviously incompatible, and this incompatibility would be present
  // already before enlarging the items since it is a DFF. So it is enough to
  // test making items wide or high, but no need to try both.

  std::vector<int> result;
  GetOrReplaceDffConflict(sizes_x, sizes_y, index_by_decreasing_x_size_,
                          bounding_box_size.first, total_energy, bb_area,
                          &result);
  if (result.size() == 3) {
    // Since we already checked pairwise conflict, we won't find anything
    // narrower than a three-item conflict.
    num_conflicts_++;
    return Result{.result = Status::INFEASIBLE,
                  .minimum_unfeasible_subproblem = result};
  }
  GetOrReplaceDffConflict(sizes_y, sizes_x, index_by_decreasing_y_size_,
                          bounding_box_size.second, total_energy, bb_area,
                          &result);

  // The items have a trivial conflict and we didn't found any tighter one.
  if (total_energy > bb_area && result.size() == num_items) {
    num_trivial_conflicts_++;
  }
  if (!result.empty()) {
    num_conflicts_++;
    return Result{.result = Status::INFEASIBLE,
                  .minimum_unfeasible_subproblem = result};
  }
  return Result{.result = Status::UNKNOWN};
}

}  // namespace sat
}  // namespace operations_research
