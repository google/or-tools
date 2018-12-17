// Copyright 2010-2018 Google LLC
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
#include <set>

#include "absl/strings/str_format.h"
#include "ortools/base/logging.h"
#include "ortools/base/random.h"
#include "ortools/constraint_solver/routing.h"

namespace operations_research {

typedef std::function<int64(RoutingNodeIndex, RoutingNodeIndex)>
    RoutingNodeEvaluator2;

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
  int64 ManhattanDistance(RoutingIndexManager::NodeIndex from,
                          RoutingIndexManager::NodeIndex to) const;
  int64 NegManhattanDistance(RoutingIndexManager::NodeIndex from,
                             RoutingIndexManager::NodeIndex to) const;
  int64 ManhattanTime(RoutingIndexManager::NodeIndex from,
                      RoutingIndexManager::NodeIndex to) const;

  bool SameLocation(RoutingIndexManager::NodeIndex node1,
                    RoutingIndexManager::NodeIndex node2) const;
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
  gtl::ITIVector<RoutingIndexManager::NodeIndex, Location> locations_;
};

// Random demand.
class RandomDemand {
 public:
  RandomDemand(int size, RoutingIndexManager::NodeIndex depot,
               bool use_deterministic_seed);
  void Initialize();
  int64 Demand(RoutingIndexManager::NodeIndex from,
               RoutingIndexManager::NodeIndex to) const;

 private:
  std::unique_ptr<int64[]> demand_;
  const int size_;
  const RoutingIndexManager::NodeIndex depot_;
  const bool use_deterministic_seed_;
};

// Service time (proportional to demand) + transition time callback.
class ServiceTimePlusTransition {
 public:
  ServiceTimePlusTransition(
      int64 time_per_demand_unit,
      RoutingNodeEvaluator2 demand,
      RoutingNodeEvaluator2 transition_time);
  int64 Compute(RoutingIndexManager::NodeIndex from,
                RoutingIndexManager::NodeIndex to) const;

 private:
  const int64 time_per_demand_unit_;
  RoutingNodeEvaluator2 demand_;
  RoutingNodeEvaluator2 transition_time_;
};

// Stop service time + transition time callback.
class StopServiceTimePlusTransition {
 public:
  StopServiceTimePlusTransition(
      int64 stop_time, const LocationContainer& location_container,
      RoutingNodeEvaluator2 transition_time);
  int64 Compute(RoutingIndexManager::NodeIndex from,
                RoutingIndexManager::NodeIndex to) const;

 private:
  const int64 stop_time_;
  const LocationContainer& location_container_;
  RoutingNodeEvaluator2 demand_;
  RoutingNodeEvaluator2 transition_time_;
};

// Route plan displayer.
// TODO(user): Move the display code to the routing library.
void DisplayPlan(
    const operations_research::RoutingIndexManager& manager,
    const operations_research::RoutingModel& routing,
    const operations_research::Assignment& plan, bool use_same_vehicle_costs,
    int64 max_nodes_per_group, int64 same_vehicle_cost,
    const operations_research::RoutingDimension& capacity_dimension,
    const operations_research::RoutingDimension& time_dimension);

using NodeIndex = RoutingIndexManager::NodeIndex;

int32 GetSeed(bool deterministic) {
  if (deterministic) {
    return ACMRandom::DeterministicSeed();
  } else {
    return ACMRandom::HostnamePidTimeSeed();
  }
}

LocationContainer::LocationContainer(int64 speed, bool use_deterministic_seed)
    : randomizer_(GetSeed(use_deterministic_seed)), speed_(speed) {
  CHECK_LT(0, speed_);
}

void LocationContainer::AddRandomLocation(int64 x_max, int64 y_max) {
  AddRandomLocation(x_max, y_max, 1);
}

void LocationContainer::AddRandomLocation(int64 x_max, int64 y_max,
                                          int duplicates) {
  const int64 x = randomizer_.Uniform(x_max + 1);
  const int64 y = randomizer_.Uniform(y_max + 1);
  for (int i = 0; i < duplicates; ++i) {
    AddLocation(x, y);
  }
}

int64 LocationContainer::ManhattanDistance(NodeIndex from, NodeIndex to) const {
  return locations_[from].DistanceTo(locations_[to]);
}

int64 LocationContainer::NegManhattanDistance(NodeIndex from,
                                              NodeIndex to) const {
  return -ManhattanDistance(from, to);
}

int64 LocationContainer::ManhattanTime(NodeIndex from, NodeIndex to) const {
  return ManhattanDistance(from, to) / speed_;
}

bool LocationContainer::SameLocation(NodeIndex node1, NodeIndex node2) const {
  if (node1 < locations_.size() && node2 < locations_.size()) {
    return locations_[node1].IsAtSameLocation(locations_[node2]);
  }
  return false;
}
int64 LocationContainer::SameLocationFromIndex(int64 node1, int64 node2) const {
  // The direct conversion from constraint model indices to routing model
  // nodes is correct because the depot is node 0.
  // TODO(user): Fetch proper indices from routing model.
  return SameLocation(NodeIndex(node1), NodeIndex(node2));
}

LocationContainer::Location::Location() : x_(0), y_(0) {}

LocationContainer::Location::Location(int64 x, int64 y) : x_(x), y_(y) {}

int64 LocationContainer::Location::DistanceTo(const Location& location) const {
  return Abs(x_ - location.x_) + Abs(y_ - location.y_);
}

bool LocationContainer::Location::IsAtSameLocation(
    const Location& location) const {
  return x_ == location.x_ && y_ == location.y_;
}

int64 LocationContainer::Location::Abs(int64 value) {
  return std::max(value, -value);
}

RandomDemand::RandomDemand(int size, NodeIndex depot,
                           bool use_deterministic_seed)
    : size_(size),
      depot_(depot),
      use_deterministic_seed_(use_deterministic_seed) {
  CHECK_LT(0, size_);
}

void RandomDemand::Initialize() {
  const int64 kDemandMax = 5;
  const int64 kDemandMin = 1;
  demand_ = absl::make_unique<int64[]>(size_);
  MTRandom randomizer(GetSeed(use_deterministic_seed_));
  for (int order = 0; order < size_; ++order) {
    if (order == depot_) {
      demand_[order] = 0;
    } else {
      demand_[order] =
          kDemandMin + randomizer.Uniform(kDemandMax - kDemandMin + 1);
    }
  }
}

int64 RandomDemand::Demand(NodeIndex from, NodeIndex /*to*/) const {
  return demand_[from.value()];
}

ServiceTimePlusTransition::ServiceTimePlusTransition(
    int64 time_per_demand_unit, RoutingNodeEvaluator2 demand,
    RoutingNodeEvaluator2 transition_time)
    : time_per_demand_unit_(time_per_demand_unit),
      demand_(std::move(demand)),
      transition_time_(std::move(transition_time)) {}

int64 ServiceTimePlusTransition::Compute(NodeIndex from, NodeIndex to) const {
  return time_per_demand_unit_ * demand_(from, to) + transition_time_(from, to);
}

StopServiceTimePlusTransition::StopServiceTimePlusTransition(
    int64 stop_time, const LocationContainer& location_container,
    RoutingNodeEvaluator2 transition_time)
    : stop_time_(stop_time),
      location_container_(location_container),
      transition_time_(std::move(transition_time)) {}

int64 StopServiceTimePlusTransition::Compute(NodeIndex from,
                                             NodeIndex to) const {
  return location_container_.SameLocation(from, to)
             ? 0
             : stop_time_ + transition_time_(from, to);
}

void DisplayPlan(
    const RoutingIndexManager& manager, const RoutingModel& routing,
    const operations_research::Assignment& plan, bool use_same_vehicle_costs,
    int64 max_nodes_per_group, int64 same_vehicle_cost,
    const operations_research::RoutingDimension& capacity_dimension,
    const operations_research::RoutingDimension& time_dimension) {
  // Display plan cost.
  std::string plan_output = absl::StrFormat("Cost %d\n", plan.ObjectiveValue());

  // Display dropped orders.
  std::string dropped;
  for (int64 order = 0; order < routing.Size(); ++order) {
    if (routing.IsStart(order) || routing.IsEnd(order)) continue;
    if (plan.Value(routing.NextVar(order)) == order) {
      if (dropped.empty()) {
        absl::StrAppendFormat(&dropped, " %d",
                              manager.IndexToNode(order).value());
      } else {
        absl::StrAppendFormat(&dropped, ", %d",
                              manager.IndexToNode(order).value());
      }
    }
  }
  if (!dropped.empty()) {
    plan_output += "Dropped orders:" + dropped + "\n";
  }

  if (use_same_vehicle_costs) {
    int group_size = 0;
    int64 group_same_vehicle_cost = 0;
    std::set<int> visited;
    for (int64 order = 0; order < routing.Size(); ++order) {
      if (routing.IsStart(order) || routing.IsEnd(order)) continue;
      ++group_size;
      visited.insert(plan.Value(routing.VehicleVar(order)));
      if (group_size == max_nodes_per_group) {
        if (visited.size() > 1) {
          group_same_vehicle_cost += (visited.size() - 1) * same_vehicle_cost;
        }
        group_size = 0;
        visited.clear();
      }
    }
    if (visited.size() > 1) {
      group_same_vehicle_cost += (visited.size() - 1) * same_vehicle_cost;
    }
    LOG(INFO) << "Same vehicle costs: " << group_same_vehicle_cost;
  }

  // Display actual output for each vehicle.
  for (int route_number = 0; route_number < routing.vehicles();
       ++route_number) {
    int64 order = routing.Start(route_number);
    absl::StrAppendFormat(&plan_output, "Route %d: ", route_number);
    if (routing.IsEnd(plan.Value(routing.NextVar(order)))) {
      plan_output += "Empty\n";
    } else {
      while (true) {
        operations_research::IntVar* const load_var =
            capacity_dimension.CumulVar(order);
        operations_research::IntVar* const time_var =
            time_dimension.CumulVar(order);
        operations_research::IntVar* const slack_var =
            routing.IsEnd(order) ? nullptr : time_dimension.SlackVar(order);
        if (slack_var != nullptr && plan.Contains(slack_var)) {
          absl::StrAppendFormat(
              &plan_output, "%d Load(%d) Time(%d, %d) Slack(%d, %d)",
              manager.IndexToNode(order).value(), plan.Value(load_var),
              plan.Min(time_var), plan.Max(time_var), plan.Min(slack_var),
              plan.Max(slack_var));
        } else {
          absl::StrAppendFormat(&plan_output, "%d Load(%d) Time(%d, %d)",
                                manager.IndexToNode(order).value(),
                                plan.Value(load_var), plan.Min(time_var),
                                plan.Max(time_var));
        }
        if (routing.IsEnd(order)) break;
        plan_output += " -> ";
        order = plan.Value(routing.NextVar(order));
      }
      plan_output += "\n";
    }
  }
  LOG(INFO) << plan_output;
}
}  // namespace operations_research

#endif  // OR_TOOLS_EXAMPLES_CVRPTW_LIB_H_
