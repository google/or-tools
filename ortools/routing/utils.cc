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

#include "ortools/routing/utils.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <queue>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research::routing {

void BinCapacities::AddDimension(
    std::function<int64_t(int, int)> load_demand_of_item_for_bin,
    std::vector<LoadLimit> load_limit_per_bin) {
  DCHECK_EQ(num_bins_, load_limit_per_bin.size());
  for (const LoadLimit& limit : load_limit_per_bin) {
    const int64_t violation = std::max<int64_t>(0, CapOpp(limit.soft_max_load));
    total_cost_ =
        CapAdd(total_cost_, CapProd(violation, limit.cost_above_soft_max_load));
  }
  load_demands_per_dimension_.push_back(std::move(load_demand_of_item_for_bin));
  for (int b = 0; b < num_bins_; ++b) {
    load_per_bin_[b].push_back(0);
    load_limits_per_bin_[b].push_back(load_limit_per_bin[b]);
  }
}

bool BinCapacities::CheckAdditionFeasibility(int item, int bin) const {
  return CheckAdditionsFeasibility({item}, bin);
}

bool BinCapacities::CheckAdditionsFeasibility(absl::Span<const int> items,
                                              int bin) const {
  for (size_t dim = 0; dim < load_demands_per_dimension_.size(); ++dim) {
    const LoadLimit& limit = load_limits_per_bin_[bin][dim];
    int64_t new_load = load_per_bin_[bin][dim];
    for (const int item : items) {
      new_load = CapAdd(new_load, load_demands_per_dimension_[dim](item, bin));
    }
    // TODO(user): try to reorder on failure, so that tight dimensions are
    // checked first.
    if (new_load > limit.max_load) return false;
  }
  return true;
}

bool BinCapacities::AddItemToBin(int item, int bin) {
  bool feasible = true;
  for (size_t dim = 0; dim < load_demands_per_dimension_.size(); ++dim) {
    int64_t& load = load_per_bin_[bin][dim];
    const LoadLimit& limit = load_limits_per_bin_[bin][dim];
    const int64_t prev_violation =
        std::max<int64_t>(0, CapSub(load, limit.soft_max_load));
    load = CapAdd(load, load_demands_per_dimension_[dim](item, bin));
    const int64_t curr_violation =
        std::max<int64_t>(0, CapSub(load, limit.soft_max_load));
    total_cost_ =
        CapAdd(total_cost_, CapProd(CapSub(curr_violation, prev_violation),
                                    limit.cost_above_soft_max_load));
    feasible &= load <= limit.max_load;
  }
  return feasible;
}

bool BinCapacities::RemoveItemFromBin(int item, int bin) {
  bool feasible = true;
  for (size_t dim = 0; dim < load_demands_per_dimension_.size(); ++dim) {
    int64_t& load = load_per_bin_[bin][dim];
    const LoadLimit& limit = load_limits_per_bin_[bin][dim];
    const int64_t prev_violation =
        std::max<int64_t>(0, CapSub(load, limit.soft_max_load));
    load = CapSub(load, load_demands_per_dimension_[dim](item, bin));
    const int64_t curr_violation =
        std::max<int64_t>(0, CapSub(load, limit.soft_max_load));
    total_cost_ =
        CapAdd(total_cost_, CapProd(CapSub(curr_violation, prev_violation),
                                    limit.cost_above_soft_max_load));
    feasible &= load <= limit.max_load;
  }
  return feasible;
}

void BinCapacities::ClearItemsOfBin(int bin) {
  for (size_t dim = 0; dim < load_demands_per_dimension_.size(); ++dim) {
    int64_t& load = load_per_bin_[bin][dim];
    const LoadLimit& limit = load_limits_per_bin_[bin][dim];
    const int64_t prev_violation =
        std::max<int64_t>(0, CapSub(load, limit.soft_max_load));
    load = 0;
    const int64_t curr_violation =
        std::max<int64_t>(0, CapOpp(limit.soft_max_load));
    total_cost_ =
        CapAdd(total_cost_, CapProd(CapSub(curr_violation, prev_violation),
                                    limit.cost_above_soft_max_load));
  }
}

void BinCapacities::ClearItems() {
  for (int bin = 0; bin < num_bins_; ++bin) {
    ClearItemsOfBin(bin);
  }
}

bool FindMostExpensiveArcsOnRoute(
    int num_arcs, int64_t start,
    const std::function<int64_t(int64_t)>& next_accessor,
    const std::function<bool(int64_t)>& is_end,
    const std::function<int64_t(int64_t, int64_t, int64_t)>&
        arc_cost_for_route_start,
    std::vector<std::pair<int64_t, int>>* most_expensive_arc_starts_and_ranks,
    std::pair<int, int>* first_expensive_arc_indices) {
  if (is_end(next_accessor(start))) {
    // Empty route.
    *first_expensive_arc_indices = {-1, -1};
    return false;
  }

  // NOTE: The negative ranks are so that for a given cost, lower ranks are
  // given higher priority.
  using ArcCostNegativeRankStart = std::tuple<int64_t, int, int64_t>;
  std::priority_queue<ArcCostNegativeRankStart,
                      std::vector<ArcCostNegativeRankStart>,
                      std::greater<ArcCostNegativeRankStart>>
      arc_info_pq;

  int64_t before_node = start;
  int rank = 0;
  while (!is_end(before_node)) {
    const int64_t after_node = next_accessor(before_node);
    const int64_t arc_cost =
        arc_cost_for_route_start(before_node, after_node, start);
    arc_info_pq.emplace(arc_cost, -rank, before_node);

    before_node = after_node;
    rank++;

    if (rank > num_arcs) {
      arc_info_pq.pop();
    }
  }

  DCHECK_GE(rank, 2);
  DCHECK_EQ(arc_info_pq.size(), std::min(rank, num_arcs));

  most_expensive_arc_starts_and_ranks->resize(arc_info_pq.size());
  int arc_index = arc_info_pq.size() - 1;
  while (!arc_info_pq.empty()) {
    const ArcCostNegativeRankStart& arc_info = arc_info_pq.top();
    (*most_expensive_arc_starts_and_ranks)[arc_index] = {
        std::get<2>(arc_info), -std::get<1>(arc_info)};
    arc_index--;
    arc_info_pq.pop();
  }

  *first_expensive_arc_indices = {0, 1};
  return true;
}

}  // namespace operations_research::routing
