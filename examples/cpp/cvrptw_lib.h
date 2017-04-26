// Copyright 2010-2014 Google
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

// This header provides functions to help creating random instaces of the
// vehicle routing problem; random capacities and random time windows.
#ifndef OR_TOOLS_EXAMPLES_CVRPTW_LIB_H_
#define OR_TOOLS_EXAMPLES_CVRPTW_LIB_H_

#include <memory>

#include "ortools/constraint_solver/routing.h"
#include "ortools/base/random.h"

namespace operations_research {

// Random seed generator.
int32 GetSeed(bool deterministic);

// Location container, contains positions of orders and can be used to obtain
// Manhattan distances/times between locations.
class LocationContainer {
 public:
  LocationContainer(int64 speed, bool use_deterministic_seed);
  void AddLocation(int64 x, int64 y) { locations_.push_back(Location(x, y)); }
  void AddRandomLocation(int64 x_max, int64 y_max);
  void AddRandomLocation(int64 x_max, int64 y_max, int duplicates);
  int64 ManhattanDistance(
      operations_research::RoutingModel::NodeIndex from,
      operations_research::RoutingModel::NodeIndex to) const;
  int64 NegManhattanDistance(
      operations_research::RoutingModel::NodeIndex from,
      operations_research::RoutingModel::NodeIndex to) const;
  int64 ManhattanTime(operations_research::RoutingModel::NodeIndex from,
                      operations_research::RoutingModel::NodeIndex to) const;

  bool SameLocation(operations_research::RoutingModel::NodeIndex node1,
                    operations_research::RoutingModel::NodeIndex node2) const;
  int64 SameLocationFromIndex(int64 node1, int64 node2) const;

 private:
  class Location {
   public:
    Location();
    Location(int64 x, int64 y);
    int64 DistanceTo(const Location& location) const;
    bool IsAtSameLocation(const Location& location) const;

   private:
    static int64 Abs(int64 value);

    int64 x_;
    int64 y_;
  };

  MTRandom randomizer_;
  const int64 speed_;
  ITIVector<operations_research::RoutingModel::NodeIndex, Location> locations_;
};

// Random demand.
class RandomDemand {
 public:
  RandomDemand(int size, operations_research::RoutingModel::NodeIndex depot,
               bool use_deterministic_seed);
  void Initialize();
  int64 Demand(operations_research::RoutingModel::NodeIndex from,
               operations_research::RoutingModel::NodeIndex to) const;

 private:
  std::unique_ptr<int64[]> demand_;
  const int size_;
  const operations_research::RoutingModel::NodeIndex depot_;
  const bool use_deterministic_seed_;
};

// Service time (proportional to demand) + transition time callback.
class ServiceTimePlusTransition {
 public:
  ServiceTimePlusTransition(
      int64 time_per_demand_unit,
      operations_research::RoutingModel::NodeEvaluator2* demand,
      operations_research::RoutingModel::NodeEvaluator2* transition_time);
  int64 Compute(operations_research::RoutingModel::NodeIndex from,
                operations_research::RoutingModel::NodeIndex to) const;

 private:
  const int64 time_per_demand_unit_;
  std::unique_ptr<operations_research::RoutingModel::NodeEvaluator2> demand_;
  std::unique_ptr<operations_research::RoutingModel::NodeEvaluator2>
      transition_time_;
};

// Stop service time + transition time callback.
class StopServiceTimePlusTransition {
 public:
  StopServiceTimePlusTransition(
      int64 stop_time, const LocationContainer& location_container,
      operations_research::RoutingModel::NodeEvaluator2* transition_time);
  int64 Compute(operations_research::RoutingModel::NodeIndex from,
                operations_research::RoutingModel::NodeIndex to) const;

 private:
  const int64 stop_time_;
  const LocationContainer& location_container_;
  std::unique_ptr<operations_research::RoutingModel::NodeEvaluator2> demand_;
  std::unique_ptr<operations_research::RoutingModel::NodeEvaluator2>
      transition_time_;
};

// Route plan displayer.
// TODO(user): Move the display code to the routing library.
void DisplayPlan(
    const operations_research::RoutingModel& routing,
    const operations_research::Assignment& plan, bool use_same_vehicle_costs,
    int64 max_nodes_per_group, int64 same_vehicle_cost,
    const operations_research::RoutingDimension& capacity_dimension,
    const operations_research::RoutingDimension& time_dimension);

}  // namespace operations_research

#endif  // OR_TOOLS_EXAMPLES_CVRPTW_LIB_H_
