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

#include "ortools/sat/2d_orthogonal_packing.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/numeric/bits.h"
#include "absl/random/distributions.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/sat/2d_packing_brute_force.h"
#include "ortools/sat/integer_base.h"
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
  stats.push_back({"OrthogonalPackingInfeasibilityDetector/no_energy_conflict",
                   num_scheduling_possible_});
  stats.push_back({"OrthogonalPackingInfeasibilityDetector/brute_force_calls",
                   num_brute_force_calls_});
  stats.push_back(
      {"OrthogonalPackingInfeasibilityDetector/brute_force_conflicts",
       num_brute_force_conflicts_});
  stats.push_back(
      {"OrthogonalPackingInfeasibilityDetector/brute_force_relaxations",
       num_brute_force_relaxation_});

  shared_stats_->AddStats(stats);
}

namespace {
std::optional<std::pair<int, int>> FindPairwiseConflict(
    absl::Span<const IntegerValue> sizes_x,
    absl::Span<const IntegerValue> sizes_y,
    std::pair<IntegerValue, IntegerValue> bounding_box_size,
    absl::Span<const int> index_by_decreasing_x_size,
    absl::Span<const int> index_by_decreasing_y_size) {
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

IntegerValue RoundingLowestInverse(IntegerValue y, IntegerValue c_k,
                                   IntegerValue max_x, IntegerValue k) {
  DCHECK_GE(y, 0);
  DCHECK_LE(y, 2 * c_k);
  IntegerValue ret = std::numeric_limits<IntegerValue>::max();

  // Are we in the case 2 * x == max_x_?
  if (y <= c_k && (max_x.value() & 1) == 0) {
    const IntegerValue inverse_mid = max_x / 2;
    ret = std::min(ret, inverse_mid);
    if (y == c_k && y.value() & 1) {
      // This is the only valid case for odd x.
      return ret;
    }
  }

  // The "perfect odd" case is handled above, round up y to an even value.
  y += y.value() & 1;

  // Check the case 2 * x > max_x_.
  const IntegerValue inverse_high = max_x - k * (c_k - y / 2);
  if (2 * inverse_high > max_x) {
    // We have an inverse in this domain, let's find its minimum value (when
    // the division rounds down the most) but don't let it go outside the
    // domain.
    const IntegerValue lowest_inverse_high =
        std::max(max_x / 2 + 1, inverse_high - k + 1);
    ret = std::min(ret, lowest_inverse_high);
  }

  // Check the case 2 * x < max_x_.
  const IntegerValue inverse_low = k * y / 2;
  if (2 * inverse_low < max_x) {
    ret = std::min(ret, inverse_low);
  }
  return ret;
}
}  // namespace

IntegerValue RoundingDualFeasibleFunction::LowestInverse(IntegerValue y) const {
  return RoundingLowestInverse(y, c_k_, max_x_, k_);
}

IntegerValue RoundingDualFeasibleFunctionPowerOfTwo::LowestInverse(
    IntegerValue y) const {
  return RoundingLowestInverse(y, c_k_, max_x_, IntegerValue(1) << log2_k_);
}

// Check for conflict using the `f_0^k` dual feasible function (see
// documentation for DualFeasibleFunctionF0). This function tries all possible
// values of the `k` parameter and returns the best conflict found (according to
// OrthogonalPackingResult::IsBetterThan) if any.
//
// The current implementation is a bit more general than a simple check using
// f_0 described above. This implementation can take a function g(x) that is
// non-decreasing and satisfy g(0)=0 and it will check for conflict using
// g(f_0^k(x)) for all values of k, but without recomputing g(x) `k` times. This
// is handy if g() is a DFF that is slow to compute. g(x) is described by the
// vector g_x[i] = g(sizes_x[i]) and the variable g_max = g(x_bb_size).
//
// The algorithm is the same if we swap the x and y dimension.
OrthogonalPackingResult OrthogonalPackingInfeasibilityDetector::GetDffConflict(
    absl::Span<const IntegerValue> sizes_x,
    absl::Span<const IntegerValue> sizes_y,
    absl::Span<const int> index_by_decreasing_x_size,
    absl::Span<const IntegerValue> g_x, IntegerValue g_max,
    IntegerValue x_bb_size, IntegerValue total_energy, IntegerValue bb_area,
    IntegerValue* best_k) {
  // If we found a conflict for a k parameter, which is rare, recompute the
  // total used energy consumed by the items to find the minimal set of
  // conflicting items.
  int num_items = sizes_x.size();
  auto build_result = [&sizes_x, &sizes_y, num_items, &x_bb_size, &bb_area,
                       &g_max, &g_x](const IntegerValue k) {
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
        OrthogonalPackingResult result(
            OrthogonalPackingResult::Status::INFEASIBLE);
        result.conflict_type_ = OrthogonalPackingResult::ConflictType::DFF_F0;
        result.items_participating_on_conflict_.resize(i + 1);
        for (int j = 0; j <= i; j++) {
          const int index = index_to_energy[j].first;
          result.items_participating_on_conflict_[j] = {
              .index = index,
              .size_x = sizes_x[index],
              .size_y = sizes_y[index]};
        }
        result.slack_ = 0;
        return result;
      }
    }
    LOG(FATAL) << "build_result called with no conflict";
  };

  // One thing we use in this implementation is that not all values of k are
  // interesting: what can cause an energy conflict is increasing the size of
  // the large items, removing the small ones makes it less constrained and we
  // do it only to preserve correctness. Thus, it is enough to check the values
  // of k that are just small enough to enlarge a large item. That means that
  // large items and small ones are not symmetric with respect to what values of
  // k are important.
  IntegerValue current_energy = total_energy;
  OrthogonalPackingResult best_result;
  if (current_energy > bb_area) {
    best_result = build_result(0);
    *best_k = 0;
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
      OrthogonalPackingResult current_result = build_result(k);
      if (current_result.IsBetterThan(best_result)) {
        best_result = current_result;
        *best_k = k;
      }
    }
  }
  return best_result;
}

namespace {

// Tries a simple heuristic to find a solution for the Resource-Constrained
// Project Scheduling Problem (RCPSP). The RCPSP can be mapped to a
// 2d bin packing where one dimension (say, x) is chosen to represent the time,
// and every item is cut into items with size_x = 1 that must remain consecutive
// in the x-axis but do not need to be aligned on the y axis. This is often
// called the cumulative relaxation of the 2d bin packing problem.
//
//  Bin-packing solution     RCPSP solution
//    ---------------       ---------------
//    | **********  |       |   *****     |
//    | **********  |       |   *****     |
//    |   #####     |       | **#####***  |
//    |   #####     |       | **#####***  |
//    ---------------       ---------------
//
// One interesting property is if we find an energy conflict using a
// superadditive function it means the problem is infeasible both interpreted as
// a 2d bin packing and as a RCPSP problem. In practice, that means that if we
// find a RCPSP solution for a 2d bin packing problem, there is no point on
// using Maximal DFFs to search for energy conflicts.
//
// Returns true if it found a feasible solution to the RCPSP problem.
bool FindHeuristicSchedulingSolution(
    absl::Span<const IntegerValue> sizes,
    absl::Span<const IntegerValue> demands,
    absl::Span<const int> heuristic_order, IntegerValue global_end_max,
    IntegerValue capacity_max,
    std::vector<std::pair<IntegerValue, IntegerValue>>& profile,
    std::vector<std::pair<IntegerValue, IntegerValue>>& new_profile) {
  // The profile (and new profile) is a set of (time, capa_left) pairs, ordered
  // by increasing time and capa_left.
  profile.clear();
  profile.emplace_back(kMinIntegerValue, capacity_max);
  profile.emplace_back(kMaxIntegerValue, capacity_max);
  IntegerValue start_of_previous_task = kMinIntegerValue;
  for (int i = 0; i < heuristic_order.size(); i++) {
    const IntegerValue event_size = sizes[heuristic_order[i]];
    const IntegerValue event_demand = demands[heuristic_order[i]];
    const IntegerValue event_start_min = 0;
    const IntegerValue event_start_max = global_end_max - event_size;
    const IntegerValue start_min =
        std::max(event_start_min, start_of_previous_task);

    // Iterate on the profile to find the step that contains start_min.
    // Then push until we find a step with enough capacity.
    int current = 0;
    while (profile[current + 1].first <= start_min ||
           profile[current].second < event_demand) {
      ++current;
    }

    const IntegerValue actual_start =
        std::max(start_min, profile[current].first);
    start_of_previous_task = actual_start;

    // Compatible with the event.start_max ?
    if (actual_start > event_start_max) return false;

    const IntegerValue actual_end = actual_start + event_size;

    // No need to update the profile on the last loop.
    if (i == heuristic_order.size() - 1) break;

    // Update the profile.
    new_profile.clear();
    new_profile.push_back(
        {actual_start, profile[current].second - event_demand});
    ++current;

    while (profile[current].first < actual_end) {
      new_profile.push_back(
          {profile[current].first, profile[current].second - event_demand});
      ++current;
    }

    if (profile[current].first > actual_end) {
      new_profile.push_back(
          {actual_end, new_profile.back().second + event_demand});
    }
    while (current < profile.size()) {
      new_profile.push_back(profile[current]);
      ++current;
    }
    profile.swap(new_profile);
  }
  return true;
}

}  // namespace

// We want to find the minimum set of values of `k` that would always find a
// conflict if there is a `k` for which it exists. In the literature it is
// often implied (but not stated) that it is sufficient to test the values of
// `k` that correspond to the size of an item. This is not true. To find the
// minimum set of values of `k` we look for all values of `k` that are
// "extreme": ie., the rounding on the division truncates the most (or the
// least) amount, depending on the sign it appears in the formula.
//
// To find these extreme values, we look for all local minima of the energy
// slack after applying the DFF (we multiply by `k` for convenience):
//    k * f_k(H) * W - sum_i k * f_k(h_i) * w_i
// If this value ever becomes negative for a value of `k`, it must happen in a
// local minimum. Then we use the fact that
//    k * floor(x / k) = x - x % k
// and that x%k has a local minimum when k=x/i and a local maximum when k=1+x/i
// for every integer i. The final finer point in the calculation is
// realizing that if
//   sum_{i, h_i > H/2} w_i > W
// then you have more "large" objects than it fits in the box, and you will
// have a conflict using the DFF f_0 for l=H/2. So we can safely ignore this
// case for the more expensive DFF f_2 calculation.
void OrthogonalPackingInfeasibilityDetector::GetAllCandidatesForKForDff2(
    absl::Span<const IntegerValue> sizes, IntegerValue bb_size,
    IntegerValue sqrt_bb_size, Bitset64<IntegerValue>& candidates) {
  // x_bb_size is less than 65536, so this fits in only 4kib.
  candidates.ClearAndResize(bb_size / 2 + 2);

  // `sqrt_bb_size` is lower than 256.
  for (IntegerValue i = 2; i <= sqrt_bb_size; i++) {
    candidates.Set(i);
  }
  for (int i = 1; i <= sqrt_bb_size; i++) {
    const QuickSmallDivision div(i);
    if (i > 1) {
      candidates.Set(div.DivideByDivisor(bb_size.value()));
    }
    for (int k = 0; k < sizes.size(); k++) {
      IntegerValue size = sizes[k];
      if (2 * size > bb_size && size < bb_size) {
        candidates.Set(div.DivideByDivisor(bb_size.value() - size.value() + 1));
      } else if (2 * size < bb_size) {
        candidates.Set(div.DivideByDivisor(size.value()));
      }
    }
  }

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
  candidates.Resize(bb_size / 4 + 1);  // Erase all >= C/4
  candidates.Resize(bb_size / 3 + 2);  // Make room for the two special values
  candidates.Set(bb_size / 4 + 1);
  if (bb_size > 3) {
    candidates.Set(bb_size / 3 + 1);
  }
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
OrthogonalPackingResult
OrthogonalPackingInfeasibilityDetector::CheckFeasibilityWithDualFunction2(
    absl::Span<const IntegerValue> sizes_x,
    absl::Span<const IntegerValue> sizes_y,
    absl::Span<const int> index_by_decreasing_x_size, IntegerValue x_bb_size,
    IntegerValue y_bb_size, int max_number_of_parameters_to_check) {
  if (x_bb_size == 1) {
    return OrthogonalPackingResult();
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

  Bitset64<IntegerValue> candidates;
  const IntegerValue sqrt_bb_size = FloorSquareRoot(x_bb_size.value());
  int num_items = sizes_x.size();
  const IntegerValue max_possible_number_of_parameters =
      std::min(x_bb_size / 4 + 1, sqrt_bb_size * num_items);
  if (5ull * max_number_of_parameters_to_check <
      max_possible_number_of_parameters) {
    // There are many more possible values than what we want to sample. It is
    // not worth to pay the price of computing all optimal values to drop most
    // of them, so let's just pick it randomly.
    candidates.Resize(x_bb_size / 4 + 1);
    int num_candidates = 0;
    while (num_candidates < max_number_of_parameters_to_check) {
      const IntegerValue pick =
          absl::Uniform(random_, 1, x_bb_size.value() / 4);
      if (!candidates.IsSet(pick)) {
        candidates.Set(pick);
        num_candidates++;
      }
    }
  } else {
    GetAllCandidatesForKForDff2(sizes_x, x_bb_size, sqrt_bb_size, candidates);

    if (max_number_of_parameters_to_check < max_possible_number_of_parameters) {
      // We might have produced too many candidates. Let's count them and if it
      // is the case, sample them.
      int count = 0;
      for (auto it = candidates.begin(); it != candidates.end(); ++it) {
        count++;
      }
      if (count > max_number_of_parameters_to_check) {
        std::vector<IntegerValue> sampled_candidates(
            max_number_of_parameters_to_check);
        std::sample(candidates.begin(), candidates.end(),
                    sampled_candidates.begin(),
                    max_number_of_parameters_to_check, random_);
        candidates.ClearAll();
        for (const IntegerValue k : sampled_candidates) {
          candidates.Set(k);
        }
      }
    }
  }
  OrthogonalPackingResult best_result;

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
    IntegerValue dff0_k;
    auto dff0_res =
        GetDffConflict(sizes_x, sizes_y, index_by_decreasing_x_size,
                       modified_sizes, modified_x_bb_size, x_bb_size, energy,
                       modified_x_bb_size * y_bb_size, &dff0_k);
    if (dff0_res.result_ != OrthogonalPackingResult::Status::INFEASIBLE) {
      continue;
    }
    DFFComposedF2F0 composed_dff(x_bb_size, dff0_k, k);
    dff0_res.conflict_type_ = OrthogonalPackingResult::ConflictType::DFF_F2;
    for (auto& item : dff0_res.items_participating_on_conflict_) {
      item.size_x =
          composed_dff.LowestInverse(composed_dff(sizes_x[item.index]));

      // The new size should contribute by the same amount to the energy and
      // correspond to smaller items.
      DCHECK_EQ(composed_dff(item.size_x), composed_dff(sizes_x[item.index]));
      DCHECK_LE(item.size_x, sizes_x[item.index]);

      item.size_y = sizes_y[item.index];
    }
    if (dff0_res.IsBetterThan(best_result)) {
      best_result = dff0_res;
    }
  }

  return best_result;
}

bool OrthogonalPackingInfeasibilityDetector::RelaxConflictWithBruteForce(
    OrthogonalPackingResult& result,
    std::pair<IntegerValue, IntegerValue> bounding_box_size,
    int brute_force_threshold) {
  const int num_items_originally =
      result.items_participating_on_conflict_.size();
  if (num_items_originally > 2 * brute_force_threshold) {
    // Don't even try on problems too big.
    return false;
  }
  std::vector<IntegerValue> sizes_x;
  std::vector<IntegerValue> sizes_y;
  std::vector<int> indexes;
  std::vector<bool> to_be_removed(num_items_originally, false);

  sizes_x.reserve(num_items_originally - 1);
  sizes_y.reserve(num_items_originally - 1);
  for (int i = 0; i < num_items_originally; i++) {
    sizes_x.clear();
    sizes_y.clear();
    // Look for a conflict using all non-removed items but the i-th one.
    for (int j = 0; j < num_items_originally; j++) {
      if (i == j || to_be_removed[j]) {
        continue;
      }
      sizes_x.push_back(result.items_participating_on_conflict_[j].size_x);
      sizes_y.push_back(result.items_participating_on_conflict_[j].size_y);
    }
    const auto solution = BruteForceOrthogonalPacking(
        sizes_x, sizes_y, bounding_box_size, brute_force_threshold);
    if (solution.status == BruteForceResult::Status::kNoSolutionExists) {
      // We still have a conflict if we remove the i-th item!
      to_be_removed[i] = true;
    }
  }
  if (!std::any_of(to_be_removed.begin(), to_be_removed.end(),
                   [](bool b) { return b; })) {
    return false;
  }
  OrthogonalPackingResult original = result;
  result.slack_ = 0;
  result.conflict_type_ = OrthogonalPackingResult::ConflictType::BRUTE_FORCE;
  result.result_ = OrthogonalPackingResult::Status::INFEASIBLE;
  result.items_participating_on_conflict_.clear();
  for (int i = 0; i < num_items_originally; i++) {
    if (to_be_removed[i]) {
      continue;
    }
    result.items_participating_on_conflict_.push_back(
        original.items_participating_on_conflict_[i]);
  }
  return true;
}

OrthogonalPackingResult
OrthogonalPackingInfeasibilityDetector::TestFeasibilityImpl(
    absl::Span<const IntegerValue> sizes_x,
    absl::Span<const IntegerValue> sizes_y,
    std::pair<IntegerValue, IntegerValue> bounding_box_size,
    const OrthogonalPackingOptions& options) {
  using ConflictType = OrthogonalPackingResult::ConflictType;

  const int num_items = sizes_x.size();
  DCHECK_EQ(num_items, sizes_y.size());
  const IntegerValue bb_area =
      bounding_box_size.first * bounding_box_size.second;
  IntegerValue total_energy = 0;

  auto make_item = [sizes_x, sizes_y](int i) {
    return OrthogonalPackingResult::Item{
        .index = i, .size_x = sizes_x[i], .size_y = sizes_y[i]};
  };

  index_by_decreasing_x_size_.resize(num_items);
  index_by_decreasing_y_size_.resize(num_items);
  for (int i = 0; i < num_items; i++) {
    total_energy += sizes_x[i] * sizes_y[i];
    index_by_decreasing_x_size_[i] = i;
    index_by_decreasing_y_size_[i] = i;
    if (sizes_x[i] > bounding_box_size.first ||
        sizes_y[i] > bounding_box_size.second) {
      OrthogonalPackingResult result(
          OrthogonalPackingResult::Status::INFEASIBLE);
      result.conflict_type_ = ConflictType::TRIVIAL;
      result.items_participating_on_conflict_ = {make_item(i)};
      return result;
    }
  }

  if (num_items <= 1) {
    return OrthogonalPackingResult(OrthogonalPackingResult::Status::FEASIBLE);
  }

  std::sort(index_by_decreasing_x_size_.begin(),
            index_by_decreasing_x_size_.end(),
            [&sizes_x, &sizes_y](int a, int b) {
              // Break ties with y-size
              return std::tie(sizes_x[a], sizes_y[a]) >
                     std::tie(sizes_x[b], sizes_y[b]);
            });
  std::sort(index_by_decreasing_y_size_.begin(),
            index_by_decreasing_y_size_.end(),
            [&sizes_y, &sizes_x](int a, int b) {
              return std::tie(sizes_y[a], sizes_x[a]) >
                     std::tie(sizes_y[b], sizes_x[b]);
            });

  // First look for pairwise incompatible pairs.
  if (options.use_pairwise) {
    if (auto pair = FindPairwiseConflict(sizes_x, sizes_y, bounding_box_size,
                                         index_by_decreasing_x_size_,
                                         index_by_decreasing_y_size_);
        pair.has_value()) {
      OrthogonalPackingResult result(
          OrthogonalPackingResult::Status::INFEASIBLE);
      result.conflict_type_ = ConflictType::PAIRWISE;
      result.items_participating_on_conflict_ = {
          make_item(pair.value().first), make_item(pair.value().second)};
      return result;
    }
    if (num_items == 2) {
      return OrthogonalPackingResult(OrthogonalPackingResult::Status::FEASIBLE);
    }
  }

  OrthogonalPackingResult result(OrthogonalPackingResult::Status::UNKNOWN);
  if (total_energy > bb_area) {
    result.conflict_type_ = ConflictType::TRIVIAL;
    result.result_ = OrthogonalPackingResult::Status::INFEASIBLE;
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
        result.items_participating_on_conflict_.resize(i + 1);
        for (int j = 0; j <= i; j++) {
          result.items_participating_on_conflict_[j] =
              make_item(index_to_energy[j].first);
        }
        result.slack_ = recomputed_energy - bb_area - 1;
        break;
      }
    }
  }

  const int minimum_conflict_size = options.use_pairwise ? 3 : 2;
  if (result.items_participating_on_conflict_.size() == minimum_conflict_size) {
    return result;
  }

  if (options.use_dff_f0) {
    // If there is no pairwise incompatible pairs, this DFF cannot find a
    // conflict by enlarging a item on both x and y directions: this would
    // create an item as long as the whole box and another item as high as the
    // whole box, which is obviously incompatible, and this incompatibility
    // would be present already before enlarging the items since it is a DFF. So
    // it is enough to test making items wide or high, but no need to try both.
    IntegerValue best_k;
    auto conflict =
        GetDffConflict(sizes_x, sizes_y, index_by_decreasing_x_size_, sizes_x,
                       bounding_box_size.first, bounding_box_size.first,
                       total_energy, bb_area, &best_k);
    if (conflict.IsBetterThan(result)) {
      result = conflict;
    }

    conflict =
        GetDffConflict(sizes_y, sizes_x, index_by_decreasing_y_size_, sizes_y,
                       bounding_box_size.second, bounding_box_size.second,
                       total_energy, bb_area, &best_k);
    for (auto& item : conflict.items_participating_on_conflict_) {
      std::swap(item.size_x, item.size_y);
    }
    if (conflict.IsBetterThan(result)) {
      result = conflict;
    }
  }

  if (result.items_participating_on_conflict_.size() == minimum_conflict_size) {
    return result;
  }

  bool found_scheduling_solution = false;
  if (options.use_dff_f2) {
    // Checking for conflicts using f_2 is expensive, so first try a quick
    // algorithm to check if there is no conflict to be found. See the comments
    // on top of FindHeuristicSchedulingSolution().
    if (FindHeuristicSchedulingSolution(
            sizes_x, sizes_y, index_by_decreasing_x_size_,
            bounding_box_size.first, bounding_box_size.second,
            scheduling_profile_, new_scheduling_profile_) ||
        FindHeuristicSchedulingSolution(
            sizes_y, sizes_x, index_by_decreasing_y_size_,
            bounding_box_size.second, bounding_box_size.first,
            scheduling_profile_, new_scheduling_profile_)) {
      num_scheduling_possible_++;
      CHECK(result.result_ != OrthogonalPackingResult::Status::INFEASIBLE);
      found_scheduling_solution = true;
    }
  }

  if (!found_scheduling_solution && options.use_dff_f2) {
    // We only check for conflicts applying this DFF on heights and widths, but
    // not on both, which would be too expensive if done naively.
    auto conflict = CheckFeasibilityWithDualFunction2(
        sizes_x, sizes_y, index_by_decreasing_x_size_, bounding_box_size.first,
        bounding_box_size.second,
        options.dff2_max_number_of_parameters_to_check);
    if (conflict.IsBetterThan(result)) {
      result = conflict;
    }

    if (result.items_participating_on_conflict_.size() ==
        minimum_conflict_size) {
      return result;
    }
    conflict = CheckFeasibilityWithDualFunction2(
        sizes_y, sizes_x, index_by_decreasing_y_size_, bounding_box_size.second,
        bounding_box_size.first,
        options.dff2_max_number_of_parameters_to_check);
    for (auto& item : conflict.items_participating_on_conflict_) {
      std::swap(item.size_x, item.size_y);
    }
    if (conflict.IsBetterThan(result)) {
      result = conflict;
    }
  }

  if (result.result_ == OrthogonalPackingResult::Status::UNKNOWN) {
    auto solution = BruteForceOrthogonalPacking(
        sizes_x, sizes_y, bounding_box_size, options.brute_force_threshold);
    num_brute_force_calls_ +=
        (solution.status != BruteForceResult::Status::kTooBig);
    if (solution.status == BruteForceResult::Status::kNoSolutionExists) {
      result.conflict_type_ = ConflictType::BRUTE_FORCE;
      result.result_ = OrthogonalPackingResult::Status::INFEASIBLE;
      result.items_participating_on_conflict_.resize(num_items);
      for (int i = 0; i < num_items; i++) {
        result.items_participating_on_conflict_[i] = make_item(i);
      }
    } else if (solution.status == BruteForceResult::Status::kFoundSolution) {
      result.result_ = OrthogonalPackingResult::Status::FEASIBLE;
    }
  }

  if (result.result_ == OrthogonalPackingResult::Status::INFEASIBLE) {
    num_brute_force_relaxation_ += RelaxConflictWithBruteForce(
        result, bounding_box_size, options.brute_force_threshold);
  }

  return result;
}

OrthogonalPackingResult OrthogonalPackingInfeasibilityDetector::TestFeasibility(
    absl::Span<const IntegerValue> sizes_x,
    absl::Span<const IntegerValue> sizes_y,
    std::pair<IntegerValue, IntegerValue> bounding_box_size,
    const OrthogonalPackingOptions& options) {
  using ConflictType = OrthogonalPackingResult::ConflictType;

  num_calls_++;
  OrthogonalPackingResult result =
      TestFeasibilityImpl(sizes_x, sizes_y, bounding_box_size, options);

  if (result.result_ == OrthogonalPackingResult::Status::INFEASIBLE) {
    num_conflicts_++;
    switch (result.conflict_type_) {
      case ConflictType::DFF_F0:
        num_conflicts_dff0_++;
        break;
      case ConflictType::DFF_F2:
        num_conflicts_dff2_++;
        break;
      case ConflictType::PAIRWISE:
        num_conflicts_two_items_++;
        break;
      case ConflictType::TRIVIAL:
        // The total area of the items was larger than the area of the box.
        num_trivial_conflicts_++;
        break;
      case ConflictType::BRUTE_FORCE:
        num_brute_force_conflicts_++;
        break;
      case ConflictType::NO_CONFLICT:
        LOG(FATAL) << "Should never happen";
        break;
    }
  }
  return result;
}

bool OrthogonalPackingResult::TryUseSlackToReduceItemSize(
    int i, Coord coord, IntegerValue lower_bound) {
  Item& item = items_participating_on_conflict_[i];
  IntegerValue& size = coord == Coord::kCoordX ? item.size_x : item.size_y;
  const IntegerValue orthogonal_size =
      coord == Coord::kCoordX ? item.size_y : item.size_x;

  if (size <= lower_bound || orthogonal_size > slack_) {
    return false;
  }
  const IntegerValue new_size =
      std::max(lower_bound, size - slack_ / orthogonal_size);
  slack_ -= (size - new_size) * orthogonal_size;
  DCHECK_NE(size, new_size);
  DCHECK_GE(slack_, 0);
  size = new_size;
  return true;
}

}  // namespace sat
}  // namespace operations_research
