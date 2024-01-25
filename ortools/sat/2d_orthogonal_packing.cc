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
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/numeric/bits.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/vlog_is_on.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"

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
  stats.push_back({"OrthogonalPackingInfeasibilityDetector/dff0_conflicts",
                   num_conflicts_dff0_});
  stats.push_back({"OrthogonalPackingInfeasibilityDetector/dff2_conflicts",
                   num_conflicts_dff2_});
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

// Check for conflict using the f_0 dual feasible function. See for example [1]
// for some discussion. This function tries all possible values of the `k`
// parameter and returns the conflict needing the least number of items.
//
// The `f_0^k(x)` function for a total bin width `C` and a parameter `k` taking
// values in [0, C/2] is defined as:
//
//            / C, if x > C - k,
// f_0^k(x) = | x, if k <= x <= C - k,
//            \ 0, if x < k.
//
// If no conflict is found, returns an empty vector.
//
// The current implementation is a bit more general than a simple check using
// f_0 described above. This implementation can take a function g(x) that is
// non-decreasing and satisfy g(0)=0 and it will check for conflict using
// g(f_0^k(x)) for all values of k, but without recomputing g(x) `k` times. This
// is handy if g() is a DFF that is slow to compute. g(x) is described by the
// vector g_x[i] = g(sizes_x[i]) and the variable g_max = g(x_bb_size).
//
// The algorithm is the same if we swap the x and y dimension.
//
// [1] Côté, Jean-François, Mohamed Haouari, and Manuel Iori. "A primal
// decomposition algorithm for the two-dimensional bin packing problem." arXiv
// preprint https://arxiv.org/abs/1909.06835 (2019).
std::vector<int> GetDffConflict(
    absl::Span<const IntegerValue> sizes_x,
    absl::Span<const IntegerValue> sizes_y,
    absl::Span<const int> index_by_decreasing_x_size,
    absl::Span<const IntegerValue> g_x, IntegerValue g_max,
    IntegerValue x_bb_size, IntegerValue total_energy, IntegerValue bb_area) {
  std::vector<int> result;
  // If we found a conflict for a k parameter, which is rare, recompute the
  // total used energy consumed by the items to find the minimal set of
  // conflicting items.
  int num_items = sizes_x.size();
  auto add_result_if_better = [&result, &sizes_x, &sizes_y, num_items,
                               &x_bb_size, &bb_area, &g_max,
                               &g_x](const IntegerValue k) {
    std::vector<std::pair<int, IntegerValue>> index_to_energy;
    index_to_energy.reserve(num_items);
    for (int i = 0; i < num_items; i++) {
      IntegerValue point_value;
      if (sizes_x[i] > x_bb_size - k) {
        point_value = g_max;
      } else if (sizes_x[i] < k) {
        continue;
      } else {
        point_value = g_x[i];
      }
      index_to_energy.push_back({i, point_value * sizes_y[i]});
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
        if (result.empty() || i + 1 < result.size()) {
          result.resize(i + 1);
          for (int j = 0; j <= i; j++) {
            result[j] = index_to_energy[j].first;
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
  if (current_energy > bb_area) {
    add_result_if_better(0);
  }
  // We keep an index on the largest item yet-to-be enlarged and the smallest
  // one yet-to-be removed.
  int removing_item_index = index_by_decreasing_x_size.size() - 1;
  int enlarging_item_index = 0;
  while (enlarging_item_index < index_by_decreasing_x_size.size()) {
    int index = index_by_decreasing_x_size[enlarging_item_index];
    IntegerValue size = sizes_x[index];
    // Note that since `size_x` is decreasing, we test increasingly large
    // values of k. Also note that a item with size `k` cannot fit alongside
    // an item with size `size_x`, but smaller ones can.
    const IntegerValue k = x_bb_size - size + 1;
    if (2 * k > x_bb_size) {
      break;
    }
    // First, add the area contribution of enlarging the all the items of size
    // exactly size_x. All larger items were already enlarged in the previous
    // iterations.
    do {
      index = index_by_decreasing_x_size[enlarging_item_index];
      size = sizes_x[index];
      current_energy += (g_max - g_x[index]) * sizes_y[index];
      enlarging_item_index++;
    } while (enlarging_item_index < index_by_decreasing_x_size.size() &&
             sizes_x[index_by_decreasing_x_size[enlarging_item_index]] == size);

    // Now remove the area contribution of removing all the items smaller than
    // k that were not removed before.
    while (removing_item_index >= 0 &&
           sizes_x[index_by_decreasing_x_size[removing_item_index]] < k) {
      const int remove_idx = index_by_decreasing_x_size[removing_item_index];
      current_energy -= g_x[remove_idx] * sizes_y[remove_idx];
      removing_item_index--;
    }

    if (current_energy > bb_area) {
      add_result_if_better(k);
    }
  }
  return result;
}

// Check for conflict all combinations of the two Dual Feasible Functions f_0
// (see documentation for GetDffConflict()) and f_2 (see documentation for
// RoundingDualFeasibleFunction). More precisely, check whether there exist `l`
// and `k` so that
//
// sum_i f_2^k(f_0^l(sizes_x[i])) * sizes_y[i] > f_2^k(f_0^l(x_bb_size)) *
//                                                 y_bb_size
//
// The function returns the smallest subset of items enough to make the
// inequality above true or an empty vector if impossible.
std::vector<int> CheckFeasibilityWithDualFunction2(
    absl::Span<const IntegerValue> sizes_x,
    absl::Span<const IntegerValue> sizes_y,
    absl::Span<const int> index_by_decreasing_x_size, IntegerValue x_bb_size,
    IntegerValue y_bb_size) {
  if (x_bb_size == 1) {
    return {};
  }
  std::vector<IntegerValue> sizes_x_rescaled;
  if (x_bb_size >= std::numeric_limits<uint16_t>::max()) {
    // To do fast division we want our sizes to fit in a uint16_t. The simplest
    // way of doing that is to just first apply this DFF with the right
    // power-of-two value of the parameter.
    const int log2_k =
        absl::bit_width(static_cast<uint64_t>(x_bb_size.value() + 1)) - 16 + 1;
    const RoundingDualFeasibleFunctionPowerOfTwo dff(x_bb_size, log2_k);
    sizes_x_rescaled.resize(sizes_x.size());
    for (int i = 0; i < sizes_x.size(); i++) {
      sizes_x_rescaled[i] = dff(sizes_x[i]);
    }
    x_bb_size = dff(x_bb_size);
    CHECK_LT(x_bb_size, std::numeric_limits<uint16_t>::max());
    sizes_x = sizes_x_rescaled;
  }

  // We now want to find the minimum set of values of `k` that would always
  // find a conflict if there is a `k` for which it exists. In the literature
  // it is often implied (but not stated) that it is sufficient to test the
  // values of `k` that correspond to the size of an item. This is not true. To
  // find the minimum set of values of `k` we look for all values of `k` that
  // are "extreme": ie., the rounding on the division truncates the most (or the
  // least) amount, depending on the sign it appears in the formula.
  IntegerValue sum_widths = 0;
  for (int i = 0; i < sizes_x.size(); i++) {
    const IntegerValue x_size = sizes_x[i];
    if (2 * x_size > x_bb_size) {
      sum_widths += 2 * sizes_y[i];
    } else if (2 * x_size == x_bb_size) {
      sum_widths += sizes_y[i];
    }
  }
  const IntegerValue round_up = sum_widths > 2 * y_bb_size ? 0 : 1;
  // x_bb_size is less than 65536, so this fits in only 4kib.
  Bitset64<IntegerValue> candidates(x_bb_size / 2 + 2);

  // `sqrt_bb_size` is lower than 256.
  const IntegerValue sqrt_bb_size = FloorSquareRoot(x_bb_size.value());
  for (IntegerValue i = 2; i <= sqrt_bb_size; i++) {
    candidates.Set(i);
  }
  for (int i = 1; i <= sqrt_bb_size; i++) {
    const QuickSmallDivision div(i);
    if (i > 1) {
      candidates.Set(div.DivideByDivisor(x_bb_size.value() + round_up.value()));
    }
    for (int k = 0; k < sizes_x.size(); k++) {
      IntegerValue x_size = sizes_x[k];
      if (2 * x_size > x_bb_size && x_size < x_bb_size) {
        candidates.Set(
            div.DivideByDivisor(x_bb_size.value() - x_size.value() + 1));
      } else if (2 * x_size < x_bb_size) {
        candidates.Set(div.DivideByDivisor(x_size.value()));
      }
    }
  }

  std::vector<int> result;
  int num_items = sizes_x.size();

  // Remove some bogus candidates added by the logic above.
  candidates.Clear(0);
  candidates.Clear(1);

  // Apply the nice result described on [1]: if we are testing the DFF
  // f_2^k(f_0^l(x)) for all values of `l`, the only values of `k` greater than
  // C/4 we need to test are {C/4+1, C/3+1}.
  //
  // In the same reference there is a proof that this way of composing f_0 and
  // f_2 cover all possible ways of composing the two functions, including
  // composing several times each.
  //
  // [1] F. Clautiaux, PhD thesis, hal/tel-00749411.
  candidates.Resize(x_bb_size / 4 + 1);  // Erase all >= C/4
  candidates.Resize(x_bb_size / 3 + 2);  // Make room for the two special values
  candidates.Set(x_bb_size / 4 + 1);
  if (x_bb_size > 3) {
    candidates.Set(x_bb_size / 3 + 1);
  }

  // Finally run our small loop to look for the conflict!
  std::vector<IntegerValue> modified_sizes(num_items);
  for (const IntegerValue k : candidates) {
    const RoundingDualFeasibleFunction dff(x_bb_size, k);
    IntegerValue energy = 0;
    for (int i = 0; i < num_items; i++) {
      modified_sizes[i] = dff(sizes_x[i]);
      energy += modified_sizes[i] * sizes_y[i];
    }
    const IntegerValue modified_x_bb_size = dff(x_bb_size);
    auto dff0_res = GetDffConflict(
        sizes_x, sizes_y, index_by_decreasing_x_size, modified_sizes,
        modified_x_bb_size, x_bb_size, energy, modified_x_bb_size * y_bb_size);
    if (!dff0_res.empty() &&
        (result.empty() || dff0_res.size() < result.size())) {
      result = dff0_res;
    }
  }

  return result;
}

}  // namespace

OrthogonalPackingInfeasibilityDetector::Result
OrthogonalPackingInfeasibilityDetector::TestFeasibility(
    absl::Span<const IntegerValue> sizes_x,
    absl::Span<const IntegerValue> sizes_y,
    std::pair<IntegerValue, IntegerValue> bounding_box_size) {
  enum class ConflictType {
    NO_CONFLICT,
    TRIVIAL,
    DFF_F0,
    DFF_F2,
  };
  ConflictType conflict_type = ConflictType::NO_CONFLICT;
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
  if (options_.use_pairwise) {
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
  }

  std::vector<int> result;
  if (total_energy > bb_area) {
    conflict_type = ConflictType::TRIVIAL;
    std::vector<std::pair<int, IntegerValue>> index_to_energy;
    index_to_energy.reserve(num_items);
    for (int i = 0; i < num_items; i++) {
      index_to_energy.push_back({i, sizes_x[i] * sizes_y[i]});
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
        result.resize(i + 1);
        for (int j = 0; j <= i; j++) {
          result[j] = index_to_energy[j].first;
        }
        break;
      }
    }
  }

  auto set_conflict_if_better = [&result, &conflict_type](
                                    const std::vector<int>& conflict,
                                    ConflictType type) {
    if (!conflict.empty() &&
        (result.empty() || conflict.size() < result.size())) {
      conflict_type = type;
      result = conflict;
    }
  };

  if (options_.use_dff_f0) {
    // If there is no pairwise incompatible pairs, this DFF cannot find a
    // conflict by enlarging a item on both x and y directions: this would
    // create an item as long as the whole box and another item as high as the
    // whole box, which is obviously incompatible, and this incompatibility
    // would be present already before enlarging the items since it is a DFF. So
    // it is enough to test making items wide or high, but no need to try both.
    auto conflict =
        GetDffConflict(sizes_x, sizes_y, index_by_decreasing_x_size_, sizes_x,
                       bounding_box_size.first, bounding_box_size.first,
                       total_energy, bb_area);
    set_conflict_if_better(conflict, ConflictType::DFF_F0);

    conflict = GetDffConflict(sizes_y, sizes_x, index_by_decreasing_y_size_,
                              sizes_y, bounding_box_size.second,
                              bounding_box_size.second, total_energy, bb_area);
    set_conflict_if_better(conflict, ConflictType::DFF_F0);
  }

  if (options_.use_dff_f2) {
    // We only check for conflicts applying this DFF on heights and widths, but
    // not on both, which would be too expensive if done naively.
    auto conflict = CheckFeasibilityWithDualFunction2(
        sizes_x, sizes_y, index_by_decreasing_x_size_, bounding_box_size.first,
        bounding_box_size.second);
    set_conflict_if_better(conflict, ConflictType::DFF_F2);

    conflict = CheckFeasibilityWithDualFunction2(
        sizes_y, sizes_x, index_by_decreasing_y_size_, bounding_box_size.second,
        bounding_box_size.first);
    set_conflict_if_better(conflict, ConflictType::DFF_F2);
  }

  if (!result.empty()) {
    num_conflicts_++;
    switch (conflict_type) {
      case ConflictType::DFF_F0:
        num_conflicts_dff0_++;
        break;
      case ConflictType::DFF_F2:
        num_conflicts_dff2_++;
        break;
      case ConflictType::TRIVIAL:
        // The total area of the items was larger than the area of the box.
        num_trivial_conflicts_++;
        break;
      case ConflictType::NO_CONFLICT:
        LOG(FATAL) << "Should never happen";
        break;
    }
    return Result{.result = Status::INFEASIBLE,
                  .minimum_unfeasible_subproblem = result};
  }
  return Result{.result = Status::UNKNOWN};
}

}  // namespace sat
}  // namespace operations_research
