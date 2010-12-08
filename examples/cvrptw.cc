// Copyright 2010 Google
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
//
// Capacitated Vehicle Routing Problem with Time Windows (and optional orders).
// A description of the problem can be found here:
// http://en.wikipedia.org/wiki/Vehicle_routing_problem.
// The variant which is tackled by this model includes a capacity dimension,
// time windows and optional orders, with a penalty cost if orders are not
// performed. For the sake of simplicty, orders are randomly located and
// distances are computed using the Manhattan distance. Distances are assumed
// to be in meters and times in seconds.

#include <vector>

#include "base/callback.h"
#include "constraint_solver/routing.h"
#include "base/random.h"

using operations_research::Assignment;
using operations_research::IntVar;
using operations_research::RoutingModel;
using operations_research::Solver;
using operations_research::ACMRandom;
using operations_research::StringAppendF;
using operations_research::StringPrintf;
using operations_research::scoped_array;
using operations_research::scoped_ptr;

DEFINE_int32(vrp_orders, 100, "Nodes in the problem.");
DEFINE_int32(vrp_vehicles, 20, "Size of Traveling Salesman Problem instance.");
DEFINE_bool(vrp_use_deterministic_random_seed, false,
            "Use deterministic random seeds.");

const char* kTime = "Time";
const char* kCapacity = "Capacity";

// Random seed generator.
int32 GetSeed() {
  if (FLAGS_vrp_use_deterministic_random_seed) {
    return ACMRandom::DeterministicSeed();
  } else {
    return ACMRandom::HostnamePidTimeSeed();
  }
}

// Location container, contains positions of orders and can be used to obtain
// Manhattan distances/times between locations.
class LocationContainer {
 public:
  explicit LocationContainer(int64 speed)
      : randomizer_(GetSeed()), speed_(speed) {
    CHECK_LT(0, speed_);
  }
  void AddLocation(int64 x, int64 y) {
    locations_.push_back(Location(x, y));
  }
  void AddRandomLocation(int64 x_max, int64 y_max) {
    AddLocation(randomizer_.Uniform(x_max + 1), randomizer_.Uniform(y_max + 1));
  }
  int64 ManhattanDistance(int64 from, int64 to) const {
    return locations_[from].DistanceTo(locations_[to]);
  }
  int64 ManhattanTime(int64 from, int64 to) const {
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
  vector<Location> locations_;
};

// Random demand.
class RandomDemand {
 public:
  RandomDemand(int size, int64 depot) : size_(size), depot_(depot) {
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
  int64 Demand(int64 from, int64 to) const {
    return demand_[from];
  }
 private:
  scoped_array<int64> demand_;
  const int size_;
  const int64 depot_;
};

// Service time (proportional to demand) + transition time callback.
class ServiceTimePlusTransition {
 public:
  ServiceTimePlusTransition(int64 time_per_demand_unit,
                            Solver::IndexEvaluator2* demand,
                            Solver::IndexEvaluator2* transition_time)
      : time_per_demand_unit_(time_per_demand_unit),
        demand_(demand),
        transition_time_(transition_time) {}
  int64 Compute(int64 from, int64 to) const {
    return time_per_demand_unit_ * demand_->Run(from, to)
        + transition_time_->Run(from, to);
  }
 private:
  const int64 time_per_demand_unit_;
  scoped_ptr<Solver::IndexEvaluator2> demand_;
  scoped_ptr<Solver::IndexEvaluator2> transition_time_;
};

// Route plan displayer.
// TODO(user): Move the display code to the routing library.
void DisplayPlan(const RoutingModel& routing, const Assignment& plan) {
  // Display plan cost.
  string plan_output = StringPrintf("Cost %lld\n", plan.ObjectiveValue());

  // Display dropped orders.
  string dropped;
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
  for (int route_number = 0;
       route_number < routing.vehicles();
       ++route_number) {
    int64 order = routing.Start(route_number);
    StringAppendF(&plan_output, "Route %d: ", route_number);
    if (routing.IsEnd(plan.Value(routing.NextVar(order)))) {
      plan_output += "Empty\n";
    } else {
      while (!routing.IsEnd(order)) {
        IntVar* load_var = routing.CumulVar(order, kCapacity);
        IntVar* time_var = routing.CumulVar(order, kTime);
        StringAppendF(&plan_output, "%lld Load(%lld) Time(%lld, %lld) -> ",
                      order,
                      plan.Value(load_var),
                      plan.Min(time_var),
                      plan.Max(time_var));
        order = plan.Value(routing.NextVar(order));
      }
      IntVar* load_var = routing.CumulVar(order, kCapacity);
      IntVar* time_var = routing.CumulVar(order, kTime);
      StringAppendF(&plan_output, "%lld Load(%lld) Time(%lld, %lld)\n",
                    order,
                    plan.Value(load_var),
                    plan.Min(time_var),
                    plan.Max(time_var));
    }
  }
  LG << plan_output;
}

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  CHECK_LT(0, FLAGS_vrp_orders) << "Specify an instance size greater than 0.";
  CHECK_LT(0, FLAGS_vrp_vehicles) << "Specify a non-null vehicle fleet size.";
  // VRP of size FLAGS_vrp_size.
  // Nodes are indexed from 0 to FLAGS_vrp_orders, the starts and ends of
  // the routes are at node 0.
  const int64 kDepot = 0;
  RoutingModel routing(FLAGS_vrp_orders + 1, FLAGS_vrp_vehicles);
  routing.SetDepot(kDepot);
  // Setting first solution heuristic (cheapest addition).
  routing.SetCommandLineOption("routing_first_solution", "PathCheapestArc");
  // Disabling Large Neighborhood Search, comment out to activate it.
  routing.SetCommandLineOption("routing_no_lns", "true");

  // Setting up locations.
  const int64 kXMax = 100000;
  const int64 kYMax = 100000;
  const int64 kSpeed = 10;
  LocationContainer locations(kSpeed);
  for (int location = 0; location <= FLAGS_vrp_orders; ++location) {
    locations.AddRandomLocation(kXMax, kYMax);
  }

  // Setting the cost function.
  routing.SetCost(NewPermanentCallback(&locations,
                                       &LocationContainer::ManhattanDistance));

  // Adding capacity dimension constraints.
  const int64 kVehicleCapacity = 40;
  const int64 kNullCapacitySlack = 0;
  RandomDemand demand(routing.nodes(), kDepot);
  demand.Initialize();
  routing.AddDimension(NewPermanentCallback(&demand, &RandomDemand::Demand),
                       kNullCapacitySlack, kVehicleCapacity, kCapacity);

  // Adding time dimension constraints.
  const int64 kTimePerDemandUnit = 300;
  const int64 kHorizon = 24 * 3600;
  ServiceTimePlusTransition time(
      kTimePerDemandUnit,
      NewPermanentCallback(&demand, &RandomDemand::Demand),
      NewPermanentCallback(&locations, &LocationContainer::ManhattanTime));
  routing.AddDimension(
      NewPermanentCallback(&time,
                           &ServiceTimePlusTransition::Compute),
      kHorizon, kHorizon, kTime);
  // Adding time windows.
  ACMRandom randomizer(GetSeed());
  const int64 kTWDuration = 5 * 3600;
  for (int order = 1; order < routing.nodes(); ++order) {
    const int64 start = randomizer.Uniform(kHorizon - kTWDuration);
    routing.CumulVar(order, kTime)->SetRange(start, start + kTWDuration);
  }

  // Adding penalty costs to allow skipping orders.
  const int64 kPenalty = 100000;
  for (int order = 1; order < routing.nodes(); ++order) {
    vector<int64> orders(1, order);
    routing.AddDisjunction(orders, kPenalty);
  }

  // Solve, returns a solution if any (owned by RoutingModel).
  const Assignment* solution = routing.Solve();
  if (solution != NULL) {
    DisplayPlan(routing, *solution);
  } else {
    LG << "No solution found.";
  }
  return 0;
}
