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

#include "ortools/routing/parsers/cvrptw_lib.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <utility>

#include "absl/container/btree_set.h"
#include "absl/strings/str_format.h"
#include "ortools/base/logging.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_index_manager.h"

namespace operations_research {

using NodeIndex = RoutingIndexManager::NodeIndex;

int32_t GetSeed(bool deterministic) {
  if (deterministic) {
    return 7777777;
  } else {
    return absl::Uniform<int64_t>(absl::BitGen(), 0,
                                  std::numeric_limits<int64_t>::max());
  }
}

LocationContainer::LocationContainer(int64_t speed, bool use_deterministic_seed)
    : randomizer_(GetSeed(use_deterministic_seed)), speed_(speed) {
  CHECK_LT(0, speed_);
}

void LocationContainer::AddRandomLocation(int64_t x_max, int64_t y_max) {
  AddRandomLocation(x_max, y_max, 1);
}

void LocationContainer::AddRandomLocation(int64_t x_max, int64_t y_max,
                                          int duplicates) {
  const int64_t x = absl::Uniform(randomizer_, 0, x_max + 1);
  const int64_t y = absl::Uniform(randomizer_, 0, y_max + 1);
  for (int i = 0; i < duplicates; ++i) {
    AddLocation(x, y);
  }
}

int64_t LocationContainer::ManhattanDistance(NodeIndex from,
                                             NodeIndex to) const {
  return locations_[from].DistanceTo(locations_[to]);
}

int64_t LocationContainer::NegManhattanDistance(NodeIndex from,
                                                NodeIndex to) const {
  return -ManhattanDistance(from, to);
}

int64_t LocationContainer::ManhattanTime(NodeIndex from, NodeIndex to) const {
  return ManhattanDistance(from, to) / speed_;
}

bool LocationContainer::SameLocation(NodeIndex node1, NodeIndex node2) const {
  if (node1 < locations_.size() && node2 < locations_.size()) {
    return locations_[node1].IsAtSameLocation(locations_[node2]);
  }
  return false;
}
int64_t LocationContainer::SameLocationFromIndex(int64_t node1,
                                                 int64_t node2) const {
  // The direct conversion from constraint model indices to routing model
  // nodes is correct because the depot is node 0.
  // TODO(user): Fetch proper indices from routing model.
  return SameLocation(NodeIndex(node1), NodeIndex(node2));
}

LocationContainer::Location::Location() : x_(0), y_(0) {}

LocationContainer::Location::Location(int64_t x, int64_t y) : x_(x), y_(y) {}

int64_t LocationContainer::Location::DistanceTo(
    const Location& location) const {
  return Abs(x_ - location.x_) + Abs(y_ - location.y_);
}

bool LocationContainer::Location::IsAtSameLocation(
    const Location& location) const {
  return x_ == location.x_ && y_ == location.y_;
}

int64_t LocationContainer::Location::Abs(int64_t value) {
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
  const int64_t kDemandMax = 5;
  const int64_t kDemandMin = 1;
  demand_ = std::make_unique<int64_t[]>(size_);
  std::mt19937 randomizer(GetSeed(use_deterministic_seed_));
  for (int order = 0; order < size_; ++order) {
    if (order == depot_) {
      demand_[order] = 0;
    } else {
      demand_[order] = kDemandMin + absl::Uniform(randomizer, 0,
                                                  kDemandMax - kDemandMin + 1);
    }
  }
}

int64_t RandomDemand::Demand(NodeIndex from, NodeIndex /*to*/) const {
  return demand_[from.value()];
}

ServiceTimePlusTransition::ServiceTimePlusTransition(
    int64_t time_per_demand_unit, RoutingNodeEvaluator2 demand,
    RoutingNodeEvaluator2 transition_time)
    : time_per_demand_unit_(time_per_demand_unit),
      demand_(std::move(demand)),
      transition_time_(std::move(transition_time)) {}

int64_t ServiceTimePlusTransition::Compute(NodeIndex from, NodeIndex to) const {
  return time_per_demand_unit_ * demand_(from, to) + transition_time_(from, to);
}

StopServiceTimePlusTransition::StopServiceTimePlusTransition(
    int64_t stop_time, const LocationContainer& location_container,
    RoutingNodeEvaluator2 transition_time)
    : stop_time_(stop_time),
      location_container_(location_container),
      transition_time_(std::move(transition_time)) {}

int64_t StopServiceTimePlusTransition::Compute(NodeIndex from,
                                               NodeIndex to) const {
  return location_container_.SameLocation(from, to)
             ? 0
             : stop_time_ + transition_time_(from, to);
}

void DisplayPlan(
    const RoutingIndexManager& manager, const RoutingModel& routing,
    const operations_research::Assignment& plan, bool use_same_vehicle_costs,
    int64_t max_nodes_per_group, int64_t same_vehicle_cost,
    const operations_research::RoutingDimension& capacity_dimension,
    const operations_research::RoutingDimension& time_dimension) {
  // Display plan cost.
  std::string plan_output = absl::StrFormat("Cost %d\n", plan.ObjectiveValue());

  // Display dropped orders.
  std::string dropped;
  for (int64_t order = 0; order < routing.Size(); ++order) {
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
    int64_t group_same_vehicle_cost = 0;
    absl::btree_set<int> visited;
    for (int64_t order = 0; order < routing.Size(); ++order) {
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
    int64_t order = routing.Start(route_number);
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
