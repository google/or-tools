// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_UTILS_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_UTILS_H_

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace operations_research {

// Tracks whether bins constrained by several nonnegative dimensions can contain
// items added incrementally. Also tracks soft violation costs.
class BinCapacities {
 public:
  explicit BinCapacities(int num_bins)
      : num_bins_(num_bins),
        load_per_bin_(num_bins),
        load_limits_per_bin_(num_bins),
        total_cost_(0) {}

  // Represents the limits of a bin with respect to a dimension:
  // - max_load is a max total load, can cause TryAddItemToBin() to return false
  //   if load would exceed max_load.
  // - soft_max_load is a max load that can be exceeded, causing the TotalCost()
  //   to increase. Initial value *may* be negative, to help with modelling.
  // - cost_above_soft_max_load is the cost incurred per unit by which load
  //   exceeds soft_max_load.
  struct LoadLimit {
    int64_t max_load;
    int64_t soft_max_load;
    int64_t cost_above_soft_max_load;
  };

  void AddDimension(
      std::function<int64_t(int, int)> load_demand_of_item_for_bin,
      std::vector<LoadLimit> load_limit_per_bin);
  int NumDimensions() const { return load_demands_per_dimension_.size(); }

  // Checks whether adding item(s) is feasible w.r.t. dimensions.
  bool CheckAdditionFeasibility(int item, int bin) const;
  bool CheckAdditionsFeasibility(const std::vector<int>& items, int bin) const;
  // Adds item to bin, returns whether the bin is feasible.
  // The item is still added even when infeasible.
  bool AddItemToBin(int item, int bin);
  // Removes item from bin, return whether the bin is feasible.
  bool RemoveItemFromBin(int item, int bin);
  // Returns the total cost incurred by violating soft costs.
  int64_t TotalCost() const { return total_cost_; }
  // Removes all items from given bin, resets the total cost.
  void ClearItemsOfBin(int bin);
  // Removes all items from all bins.
  void ClearItems();

 private:
  const int num_bins_;
  // load_per_bin_[bin][dimension]
  std::vector<std::vector<int64_t>> load_per_bin_;
  // load_limits_per_bin_[bin][dimension].
  std::vector<std::vector<LoadLimit>> load_limits_per_bin_;
  // load_demands_per_dimension_[dimension](item, bin).
  std::vector<std::function<int64_t(int, int)>> load_demands_per_dimension_;
  int64_t total_cost_;
};

// Returns false if the route starting with 'start' is empty. Otherwise sets
// most_expensive_arc_starts_and_ranks and first_expensive_arc_indices according
// to the most expensive chains on the route, and returns true.
bool FindMostExpensiveArcsOnRoute(
    int num_arcs, int64_t start,
    const std::function<int64_t(int64_t)>& next_accessor,
    const std::function<bool(int64_t)>& is_end,
    const std::function<int64_t(int64_t, int64_t, int64_t)>&
        arc_cost_for_route_start,
    std::vector<std::pair<int64_t, int>>* most_expensive_arc_starts_and_ranks,
    std::pair<int, int>* first_expensive_arc_indices);

}  // namespace operations_research

#endif  // OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_UTILS_H_
