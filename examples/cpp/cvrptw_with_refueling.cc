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

// Capacitated Vehicle Routing Problem with Time Windows and refueling
// constraints.
// This is an extension to the model in cvrptw.cc so refer to that file for
// more information on the common part of the model. The model implemented here
// takes into account refueling constraints using a specific dimension: vehicles
// must visit certain nodes (refueling nodes) before the quantity of fuel
// reaches zero. Fuel consumption is proportional to the distance traveled.

#include "base/unique_ptr.h"
#include <vector>

#include "base/callback.h"
#include "base/commandlineflags.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "constraint_solver/routing.h"
#include "base/random.h"

using operations_research::Assignment;
using operations_research::IntVar;
using operations_research::RoutingDimension;
using operations_research::RoutingModel;
using operations_research::Solver;
using operations_research::ACMRandom;
using operations_research::StringAppendF;
using operations_research::StringPrintf;


DECLARE_string(routing_first_solution);
DECLARE_bool(routing_no_lns);
DEFINE_int32(vrp_orders, 100, "Nodes in the problem.");
DEFINE_int32(vrp_vehicles, 20, "Size of Traveling Salesman Problem instance.");
DEFINE_bool(vrp_use_deterministic_random_seed, false,
            "Use deterministic random seeds.");

const char* kTime = "Time";
const char* kCapacity = "Capacity";
const char* kFuel = "Fuel";

// Random seed generator.
int32 GetSeed() {
  if (FLAGS_vrp_use_deterministic_random_seed) {
    return ACMRandom::DeterministicSeed();
  } else {
    return ACMRandom::HostnamePidTimeSeed();
  }
}

// Returns true if node is a refueling node (based on node / refuel node ratio).
bool IsRefuelNode(int64 node) {
  const int64 kRefuelNodeRatio = 10;
  return (node % kRefuelNodeRatio == 0);
}

// Location container, contains positions of orders and can be used to obtain
// Manhattan distances/times between locations.
class LocationContainer {
 public:
  explicit LocationContainer(int64 speed)
      : randomizer_(GetSeed()), speed_(speed) {
    CHECK_LT(0, speed_);
  }
  void AddLocation(int64 x, int64 y) { locations_.push_back(Location(x, y)); }
  void AddRandomLocation(int64 x_max, int64 y_max) {
    AddLocation(randomizer_.Uniform(x_max + 1), randomizer_.Uniform(y_max + 1));
  }
  int64 ManhattanDistance(RoutingModel::NodeIndex from,
                          RoutingModel::NodeIndex to) const {
    return locations_[from].DistanceTo(locations_[to]);
  }
  int64 NegManhattanDistance(RoutingModel::NodeIndex from,
                             RoutingModel::NodeIndex to) const {
    return -ManhattanDistance(from, to);
  }
  int64 ManhattanTime(RoutingModel::NodeIndex from,
                      RoutingModel::NodeIndex to) const {
    return ManhattanDistance(from, to) / speed_;
  }

 private:
  class Location {
   public:
    Location() : x_(0), y_(0) {}
    Location(int64 x, int64 y) : x_(x), y_(y) {}
    int64 DistanceTo(const Location& location) const {
      return Abs(x_ - location.x_) + Abs(y_ - location.y_);
    }

   private:
    static int64 Abs(int64 value) { return std::max(value, -value); }

    int64 x_;
    int64 y_;
  };

  ACMRandom randomizer_;
  const int64 speed_;
  ITIVector<RoutingModel::NodeIndex, Location> locations_;
};

// Random demand.
class RandomDemand {
 public:
  RandomDemand(int size, RoutingModel::NodeIndex depot)
      : size_(size), depot_(depot) {
    CHECK_LT(0, size_);
  }
  void Initialize() {
    const int64 kDemandMax = 5;
    const int64 kDemandMin = 1;
    demand_.reset(new int64[size_]);
    ACMRandom randomizer(GetSeed());
    for (int order = 0; order < size_; ++order) {
      if (order == depot_) {
        demand_[order] = 0;
      } else {
        demand_[order] =
            kDemandMin + randomizer.Uniform(kDemandMax - kDemandMin + 1);
      }
    }
  }
  int64 Demand(RoutingModel::NodeIndex from, RoutingModel::NodeIndex to) const {
    // Refuel nodes don't have a demand.
    if (!IsRefuelNode(from.value())) {
      return demand_[from.value()];
    }
    return 0;
  }

 private:
  std::unique_ptr<int64[]> demand_;
  const int size_;
  const RoutingModel::NodeIndex depot_;
};

// Service time (proportional to demand) + transition time callback.
class ServiceTimePlusTransition {
 public:
  ServiceTimePlusTransition(int64 time_per_demand_unit,
                            RoutingModel::NodeEvaluator2* demand,
                            RoutingModel::NodeEvaluator2* transition_time)
      : time_per_demand_unit_(time_per_demand_unit),
        demand_(demand),
        transition_time_(transition_time) {}
  int64 Compute(RoutingModel::NodeIndex from,
                RoutingModel::NodeIndex to) const {
    return time_per_demand_unit_ * demand_->Run(from, to) +
           transition_time_->Run(from, to);
  }

 private:
  const int64 time_per_demand_unit_;
  std::unique_ptr<RoutingModel::NodeEvaluator2> demand_;
  std::unique_ptr<RoutingModel::NodeEvaluator2> transition_time_;
};

// Route plan displayer.
// TODO(user): Move the display code to the routing library.
void DisplayPlan(const RoutingModel& routing, const Assignment& plan) {
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

  // Display actual output for each vehicle.
  const RoutingDimension& capacity_dimension =
      routing.GetDimensionOrDie(kCapacity);
  const RoutingDimension& time_dimension = routing.GetDimensionOrDie(kTime);
  const RoutingDimension& fuel_dimension = routing.GetDimensionOrDie(kFuel);
  for (int route_number = 0; route_number < routing.vehicles();
       ++route_number) {
    int64 order = routing.Start(route_number);
    StringAppendF(&plan_output, "Route %d: ", route_number);
    if (routing.IsEnd(plan.Value(routing.NextVar(order)))) {
      plan_output += "Empty\n";
    } else {
      while (true) {
        IntVar* const load_var = capacity_dimension.CumulVar(order);
        IntVar* const time_var = time_dimension.CumulVar(order);
        IntVar* const fuel_var = fuel_dimension.CumulVar(order);
        StringAppendF(&plan_output,
                      "%lld Load(%lld) Time(%lld, %lld) Fuel(%lld, %lld) -> ",
                      order, plan.Value(load_var), plan.Min(time_var),
                      plan.Max(time_var), plan.Min(fuel_var),
                      plan.Max(fuel_var));
        if (routing.IsEnd(order)) break;
        order = plan.Value(routing.NextVar(order));
      }
    }
    plan_output += "\n";
  }
  LOG(INFO) << plan_output;
}

int main(int argc, char** argv) {
  google::ParseCommandLineFlags( &argc, &argv, true);
  CHECK_LT(0, FLAGS_vrp_orders) << "Specify an instance size greater than 0.";
  CHECK_LT(0, FLAGS_vrp_vehicles) << "Specify a non-null vehicle fleet size.";
  // VRP of size FLAGS_vrp_size.
  // Nodes are indexed from 0 to FLAGS_vrp_orders, the starts and ends of
  // the routes are at node 0.
  const RoutingModel::NodeIndex kDepot(0);
  RoutingModel routing(FLAGS_vrp_orders + 1, FLAGS_vrp_vehicles);
  routing.SetDepot(kDepot);
  // Setting first solution heuristic (cheapest addition).
  FLAGS_routing_first_solution = "PathCheapestArc";
  // Disabling Large Neighborhood Search, comment out to activate it.
  FLAGS_routing_no_lns = true;

  // Setting up locations.
  const int64 kXMax = 100000;
  const int64 kYMax = 100000;
  const int64 kSpeed = 10;
  LocationContainer locations(kSpeed);
  for (int location = 0; location <= FLAGS_vrp_orders; ++location) {
    locations.AddRandomLocation(kXMax, kYMax);
  }

  // Setting the cost function.
  routing.SetArcCostEvaluatorOfAllVehicles(
      NewPermanentCallback(&locations, &LocationContainer::ManhattanDistance));

  // Adding capacity dimension constraints.
  const int64 kVehicleCapacity = 40;
  const int64 kNullCapacitySlack = 0;
  RandomDemand demand(routing.nodes(), kDepot);
  demand.Initialize();
  routing.AddDimension(NewPermanentCallback(&demand, &RandomDemand::Demand),
                       kNullCapacitySlack, kVehicleCapacity,
                       /*fix_start_cumul_to_zero=*/true, kCapacity);

  // Adding time dimension constraints.
  const int64 kTimePerDemandUnit = 300;
  const int64 kHorizon = 24 * 3600;
  ServiceTimePlusTransition time(
      kTimePerDemandUnit, NewPermanentCallback(&demand, &RandomDemand::Demand),
      NewPermanentCallback(&locations, &LocationContainer::ManhattanTime));
  routing.AddDimension(
      NewPermanentCallback(&time, &ServiceTimePlusTransition::Compute),
      kHorizon, kHorizon, /*fix_start_cumul_to_zero=*/true, kTime);
  const RoutingDimension& time_dimension = routing.GetDimensionOrDie(kTime);
  // Adding time windows.
  ACMRandom randomizer(GetSeed());
  const int64 kTWDuration = 5 * 3600;
  for (int order = 1; order < routing.nodes(); ++order) {
    if (!IsRefuelNode(order)) {
      const int64 start = randomizer.Uniform(kHorizon - kTWDuration);
      time_dimension.CumulVar(order)->SetRange(start, start + kTWDuration);
    }
  }

  // Adding fuel dimension. This dimension consumes a quantity equal to the
  // distance traveled. Only refuel nodes can make the quantity of dimension
  // increase by letting slack variable replenish the fuel.
  const int64 kFuelCapacity = kXMax + kYMax;
  routing.AddDimension(
      NewPermanentCallback(&locations,
                           &LocationContainer::NegManhattanDistance),
      kFuelCapacity, kFuelCapacity, /*fix_start_cumul_to_zero=*/false, kFuel);
  const RoutingDimension& fuel_dimension = routing.GetDimensionOrDie(kFuel);
  for (int order = 0; order < routing.Size(); ++order) {
    // Only let slack free for refueling nodes.
    if (!IsRefuelNode(order) || routing.IsStart(order)) {
      fuel_dimension.SlackVar(order)->SetValue(0);
    }
    // Needed to instantiate fuel quantity at each node.
    routing.AddVariableMinimizedByFinalizer(fuel_dimension.CumulVar(order));
  }

  // Adding penalty costs to allow skipping orders.
  const int64 kPenalty = 100000;
  const RoutingModel::NodeIndex kFirstNodeAfterDepot(1);
  for (RoutingModel::NodeIndex order = kFirstNodeAfterDepot;
       order < routing.nodes(); ++order) {
    std::vector<RoutingModel::NodeIndex> orders(1, order);
    routing.AddDisjunction(orders, kPenalty);
  }

  // Solve, returns a solution if any (owned by RoutingModel).
  const Assignment* solution = routing.Solve();
  if (solution != NULL) {
    DisplayPlan(routing, *solution);
  } else {
    LOG(INFO) << "No solution found.";
  }
  return 0;
}
