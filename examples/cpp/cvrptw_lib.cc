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

#include "examples/cpp/cvrptw_lib.h"

#include <set>

#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/base/random.h"
#include "ortools/base/random.h"

namespace operations_research {

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

int64 LocationContainer::ManhattanDistance(RoutingModel::NodeIndex from,
                                           RoutingModel::NodeIndex to) const {
  return locations_[from].DistanceTo(locations_[to]);
}

int64 LocationContainer::NegManhattanDistance(
    RoutingModel::NodeIndex from, RoutingModel::NodeIndex to) const {
  return -ManhattanDistance(from, to);
}

int64 LocationContainer::ManhattanTime(RoutingModel::NodeIndex from,
                                       RoutingModel::NodeIndex to) const {
  return ManhattanDistance(from, to) / speed_;
}

bool LocationContainer::SameLocation(RoutingModel::NodeIndex node1,
                                     RoutingModel::NodeIndex node2) const {
  if (node1 < locations_.size() && node2 < locations_.size()) {
    return locations_[node1].IsAtSameLocation(locations_[node2]);
  }
  return false;
}
int64 LocationContainer::SameLocationFromIndex(int64 node1, int64 node2) const {
  // The direct conversion from constraint model indices to routing model
  // nodes is correct because the depot is node 0.
  // TODO(user): Fetch proper indices from routing model.
  return SameLocation(RoutingModel::NodeIndex(node1),
                      RoutingModel::NodeIndex(node2));
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

RandomDemand::RandomDemand(int size, RoutingModel::NodeIndex depot,
                           bool use_deterministic_seed)
    : size_(size),
      depot_(depot),
      use_deterministic_seed_(use_deterministic_seed) {
  CHECK_LT(0, size_);
}

void RandomDemand::Initialize() {
  const int64 kDemandMax = 5;
  const int64 kDemandMin = 1;
  demand_.reset(new int64[size_]);
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

int64 RandomDemand::Demand(RoutingModel::NodeIndex from,
                           RoutingModel::NodeIndex /*to*/) const {
  return demand_[from.value()];
}

ServiceTimePlusTransition::ServiceTimePlusTransition(
    int64 time_per_demand_unit, RoutingModel::NodeEvaluator2* demand,
    RoutingModel::NodeEvaluator2* transition_time)
    : time_per_demand_unit_(time_per_demand_unit),
      demand_(demand),
      transition_time_(transition_time) {}

int64 ServiceTimePlusTransition::Compute(RoutingModel::NodeIndex from,
                                         RoutingModel::NodeIndex to) const {
  return time_per_demand_unit_ * demand_->Run(from, to) +
         transition_time_->Run(from, to);
}

StopServiceTimePlusTransition::StopServiceTimePlusTransition(
    int64 stop_time, const LocationContainer& location_container,
    RoutingModel::NodeEvaluator2* transition_time)
    : stop_time_(stop_time),
      location_container_(location_container),
      transition_time_(transition_time) {}

int64 StopServiceTimePlusTransition::Compute(RoutingModel::NodeIndex from,
                                             RoutingModel::NodeIndex to) const {
  return location_container_.SameLocation(from, to)
             ? 0
             : stop_time_ + transition_time_->Run(from, to);
}

void DisplayPlan(
    const RoutingModel& routing, const operations_research::Assignment& plan,
    bool use_same_vehicle_costs, int64 max_nodes_per_group,
    int64 same_vehicle_cost,
    const operations_research::RoutingDimension& capacity_dimension,
    const operations_research::RoutingDimension& time_dimension) {
  // Display plan cost.
  std::string plan_output = StringPrintf("Cost %lld\n", plan.ObjectiveValue());

  // Display dropped orders.
  std::string dropped;
  for (int order = 1; order < routing.nodes(); ++order) {
    if (plan.Value(routing.NextVar(order)) == order) {
      if (dropped.empty()) {
        StringAppendF(&dropped, " %d", order);
      } else {
        StringAppendF(&dropped, ", %d", order);
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
    const RoutingModel::NodeIndex kFirstNodeAfterDepot(1);
    for (RoutingModel::NodeIndex order = kFirstNodeAfterDepot;
         order < routing.nodes(); ++order) {
      ++group_size;
      visited.insert(
          plan.Value(routing.VehicleVar(routing.NodeToIndex(order))));
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
    StringAppendF(&plan_output, "Route %d: ", route_number);
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
          StringAppendF(
              &plan_output,
              "%lld Load(%lld) Time(%lld, %lld) Slack(%lld, %lld) -> ", order,
              plan.Value(load_var), plan.Min(time_var), plan.Max(time_var),
              plan.Min(slack_var), plan.Max(slack_var));
        } else {
          StringAppendF(&plan_output, "%lld Load(%lld) Time(%lld, %lld) -> ",
                        order, plan.Value(load_var), plan.Min(time_var),
                        plan.Max(time_var));
        }
        if (routing.IsEnd(order)) break;
        order = plan.Value(routing.NextVar(order));
      }
      plan_output += "\n";
    }
  }
  LOG(INFO) << plan_output;
}
}  // namespace operations_research
