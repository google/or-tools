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

// This header provides functions to help create random instances of the
// vehicle routing problem; random capacities and random time windows.
#ifndef OR_TOOLS_ROUTING_CVRPTW_LIB_H_
#define OR_TOOLS_ROUTING_CVRPTW_LIB_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <random>

#include "ortools/base/strong_vector.h"
#include "ortools/constraint_solver/routing.h"

namespace operations_research {

typedef std::function<int64_t(RoutingNodeIndex, RoutingNodeIndex)>
    RoutingNodeEvaluator2;

// Random seed generator.
int32_t GetSeed(bool deterministic);

// Location container, contains positions of orders and can be used to obtain
// Manhattan distances/times between locations.
class LocationContainer {
 public:
  LocationContainer(int64_t speed, bool use_deterministic_seed);
  void AddLocation(int64_t x, int64_t y) {
    locations_.push_back(Location(x, y));
  }
  void AddRandomLocation(int64_t x_max, int64_t y_max);
  void AddRandomLocation(int64_t x_max, int64_t y_max, int duplicates);
  int64_t ManhattanDistance(RoutingIndexManager::NodeIndex from,
                            RoutingIndexManager::NodeIndex to) const;
  int64_t NegManhattanDistance(RoutingIndexManager::NodeIndex from,
                               RoutingIndexManager::NodeIndex to) const;
  int64_t ManhattanTime(RoutingIndexManager::NodeIndex from,
                        RoutingIndexManager::NodeIndex to) const;

  bool SameLocation(RoutingIndexManager::NodeIndex node1,
                    RoutingIndexManager::NodeIndex node2) const;
  int64_t SameLocationFromIndex(int64_t node1, int64_t node2) const;

 private:
  class Location {
   public:
    Location();
    Location(int64_t x, int64_t y);
    int64_t DistanceTo(const Location& location) const;
    bool IsAtSameLocation(const Location& location) const;

   private:
    static int64_t Abs(int64_t value);

    int64_t x_;
    int64_t y_;
  };

  std::mt19937 randomizer_;
  const int64_t speed_;
  absl::StrongVector<RoutingIndexManager::NodeIndex, Location> locations_;
};

// Random demand.
class RandomDemand {
 public:
  RandomDemand(int size, RoutingIndexManager::NodeIndex depot,
               bool use_deterministic_seed);
  void Initialize();
  int64_t Demand(RoutingIndexManager::NodeIndex from,
                 RoutingIndexManager::NodeIndex to) const;

 private:
  std::unique_ptr<int64_t[]> demand_;
  const int size_;
  const RoutingIndexManager::NodeIndex depot_;
  const bool use_deterministic_seed_;
};

// Service time (proportional to demand) + transition time callback.
class ServiceTimePlusTransition {
 public:
  ServiceTimePlusTransition(
      int64_t time_per_demand_unit,
      operations_research::RoutingNodeEvaluator2 demand,
      operations_research::RoutingNodeEvaluator2 transition_time);
  int64_t Compute(RoutingIndexManager::NodeIndex from,
                  RoutingIndexManager::NodeIndex to) const;

 private:
  const int64_t time_per_demand_unit_;
  operations_research::RoutingNodeEvaluator2 demand_;
  operations_research::RoutingNodeEvaluator2 transition_time_;
};

// Stop service time + transition time callback.
class StopServiceTimePlusTransition {
 public:
  StopServiceTimePlusTransition(
      int64_t stop_time, const LocationContainer& location_container,
      operations_research::RoutingNodeEvaluator2 transition_time);
  int64_t Compute(RoutingIndexManager::NodeIndex from,
                  RoutingIndexManager::NodeIndex to) const;

 private:
  const int64_t stop_time_;
  const LocationContainer& location_container_;
  operations_research::RoutingNodeEvaluator2 demand_;
  operations_research::RoutingNodeEvaluator2 transition_time_;
};

// Route plan displayer.
// TODO(user): Move the display code to the routing library.
void DisplayPlan(
    const operations_research::RoutingIndexManager& manager,
    const operations_research::RoutingModel& routing,
    const operations_research::Assignment& plan, bool use_same_vehicle_costs,
    int64_t max_nodes_per_group, int64_t same_vehicle_cost,
    const operations_research::RoutingDimension& capacity_dimension,
    const operations_research::RoutingDimension& time_dimension);

}  // namespace operations_research

#endif  // OR_TOOLS_ROUTING_CVRPTW_LIB_H_
