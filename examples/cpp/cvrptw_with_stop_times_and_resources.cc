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

// Capacitated Vehicle Routing Problem with Time Windows, fixed stop times and
// capacitated resources. A stop is defined as consecutive nodes at the same
// location.
// This is an extension to the model in cvrptw.cc so refer to that file for
// more information on the common part of the model. The model implemented here
// limits the number of vehicles which can simultaneously leave or enter a node
// to one.

#include "base/unique_ptr.h"
#include <vector>

#include "base/callback.h"
#include "base/commandlineflags.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/join.h"
#include "constraint_solver/routing.h"
#include "base/random.h"

using operations_research::Assignment;
using operations_research::IntervalVar;
using operations_research::IntExpr;
using operations_research::IntVar;
using operations_research::RoutingDimension;
using operations_research::RoutingModel;
using operations_research::Solver;
using operations_research::ACMRandom;
using operations_research::StrCat;
using operations_research::StringAppendF;
using operations_research::StringPrintf;

DECLARE_string(routing_first_solution);
DECLARE_bool(routing_no_lns);
DEFINE_int32(vrp_stops, 25, "Stop locations in the problem.");
DEFINE_int32(vrp_orders_per_stop, 5, "Nodes for each stop.");
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
  void AddLocation(int64 x, int64 y) { locations_.push_back(Location(x, y)); }
  void AddRandomLocation(int64 x_max, int64 y_max, int duplicates) {
    const int64 x = randomizer_.Uniform(x_max + 1);
    const int64 y = randomizer_.Uniform(y_max + 1);
    for (int i = 0; i < duplicates; ++i) {
      AddLocation(x, y);
    }
  }
  int64 ManhattanDistance(RoutingModel::NodeIndex from,
                          RoutingModel::NodeIndex to) const {
    return locations_[from].DistanceTo(locations_[to]);
  }
  int64 ManhattanTime(RoutingModel::NodeIndex from,
                      RoutingModel::NodeIndex to) const {
    return ManhattanDistance(from, to) / speed_;
  }
  bool SameLocation(RoutingModel::NodeIndex node1,
                    RoutingModel::NodeIndex node2) const {
    if (node1 < locations_.size() && node2 < locations_.size()) {
      return locations_[node1].IsAtSameLocation(locations_[node2]);
    }
    return false;
  }
  int64 SameLocationFromIndex(int64 node1, int64 node2) const {
    // The direct conversion from constraint model indices to routing model
    // nodes is correct because the depot is node 0.
    // TODO(user): Fetch proper indices from routing model.
    return SameLocation(RoutingModel::NodeIndex(node1),
                        RoutingModel::NodeIndex(node2));
  }

 private:
  class Location {
   public:
    Location() : x_(0), y_(0) {}
    Location(int64 x, int64 y) : x_(x), y_(y) {}
    int64 DistanceTo(const Location& location) const {
      return Abs(x_ - location.x_) + Abs(y_ - location.y_);
    }
    bool IsAtSameLocation(const Location& location) const {
      return x_ == location.x_ && y_ == location.y_;
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
    return demand_[from.value()];
  }

 private:
  std::unique_ptr<int64[]> demand_;
  const int size_;
  const RoutingModel::NodeIndex depot_;
};

// Stop service time + transition time callback.
class StopServiceTimePlusTransition {
 public:
  StopServiceTimePlusTransition(int64 stop_time,
                                const LocationContainer& location_container,
                                RoutingModel::NodeEvaluator2* transition_time)
      : stop_time_(stop_time),
        location_container_(location_container),
        transition_time_(transition_time) {}
  int64 Compute(RoutingModel::NodeIndex from,
                RoutingModel::NodeIndex to) const {
    return location_container_.SameLocation(from, to) ? 0
        : stop_time_ + transition_time_->Run(from, to);
  }

 private:
  const int64 stop_time_;
  const LocationContainer& location_container_;
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
  LOG(INFO) << plan_output;

  // Display actual output for each vehicle.
  const RoutingDimension& capacity_dimension =
      routing.GetDimensionOrDie(kCapacity);
  const RoutingDimension& time_dimension = routing.GetDimensionOrDie(kTime);
  for (int route_number = 0; route_number < routing.vehicles();
       ++route_number) {
    std::string route_output;
    int64 order = routing.Start(route_number);
    StringAppendF(&route_output, "Route %d: ", route_number);
    if (routing.IsEnd(plan.Value(routing.NextVar(order)))) {
      route_output += "Empty\n";
    } else {
      while (true) {
        IntVar* const load_var = capacity_dimension.CumulVar(order);
        IntVar* const time_var = time_dimension.CumulVar(order);
        StringAppendF(&route_output, " -> %lld Load(%lld) Time(%lld, %lld)",
                      order, plan.Value(load_var), plan.Min(time_var),
                      plan.Max(time_var));
        if (routing.IsEnd(order)) break;
        order = plan.Value(routing.NextVar(order));
      }
    }
    LOG(INFO) << route_output;
  }
}

int main(int argc, char** argv) {
  google::ParseCommandLineFlags( &argc, &argv, true);
  CHECK_LT(0, FLAGS_vrp_stops) << "Specify an instance size greater than 0.";
  CHECK_LT(0, FLAGS_vrp_orders_per_stop)
      << "Specify an instance size greater than 0.";
  CHECK_LT(0, FLAGS_vrp_vehicles) << "Specify a non-null vehicle fleet size.";
  const int vrp_orders = FLAGS_vrp_stops * FLAGS_vrp_orders_per_stop;
  // Nodes are indexed from 0 to vrp_orders, the starts and ends of the routes
  // are at node 0.
  const RoutingModel::NodeIndex kDepot(0);
  RoutingModel routing(vrp_orders + 1, FLAGS_vrp_vehicles);
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
  for (int stop = 0; stop <= FLAGS_vrp_stops; ++stop) {
    const int num_orders = stop == 0 ? 1 : FLAGS_vrp_orders_per_stop;
    locations.AddRandomLocation(kXMax, kYMax, num_orders);
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
  const int64 kStopTime = 300;
  const int64 kHorizon = 24 * 3600;
  StopServiceTimePlusTransition time(
      kStopTime,
      locations,
      NewPermanentCallback(&locations, &LocationContainer::ManhattanTime));
  routing.AddDimension(
      NewPermanentCallback(&time, &StopServiceTimePlusTransition::Compute),
      kHorizon, kHorizon, /*fix_start_cumul_to_zero=*/false, kTime);

  // Adding time windows, for the sake of simplicty same for each stop.
  ACMRandom randomizer(GetSeed());
  const int64 kTWDuration = 5 * 3600;
  for (int stop = 0; stop < FLAGS_vrp_stops; ++stop) {
    const int64 start = randomizer.Uniform(kHorizon - kTWDuration);
    for (int stop_order = 0;
         stop_order < FLAGS_vrp_orders_per_stop;
         ++stop_order) {
      const int order = stop * FLAGS_vrp_orders_per_stop + stop_order + 1;
      routing.CumulVar(order, kTime)->SetRange(start, start + kTWDuration);
    }
  }

  // Adding resource constraints at order locations.
  Solver* const solver = routing.solver();
  std::vector<IntervalVar*> intervals;
  for (int stop = 0; stop < FLAGS_vrp_stops; ++stop) {
    std::vector<IntervalVar*> stop_intervals;
    for (int stop_order = 0;
         stop_order < FLAGS_vrp_orders_per_stop;
         ++stop_order) {
      const int order = stop * FLAGS_vrp_orders_per_stop + stop_order + 1;
      IntervalVar* const interval =
          solver->MakeFixedDurationIntervalVar(
              0, kHorizon, kStopTime, true, StrCat("Order", order));
      intervals.push_back(interval);
      stop_intervals.push_back(interval);
      // Link order and interval.
      IntVar* const order_start = routing.CumulVar(order, kTime);
      solver->AddConstraint(solver->MakeIsEqualCt(
          interval->SafeStartExpr(0),
          order_start,
          interval->PerformedExpr()->Var()));
      // Make interval performed iff corresponding order has service time.
      // An order has no service time iff it is at the same location as the
      // next order on the route.
      IntVar* const is_null_duration =
          solver->MakeElement(
              NewPermanentCallback(&locations,
                                   &LocationContainer::SameLocationFromIndex,
                                   order),
              routing.NextVar(order))->Var();
      solver->AddConstraint(solver->MakeNonEquality(interval->PerformedExpr(),
                                                    is_null_duration));
      routing.AddIntervalToAssignment(interval);
      // We are minimizing route durations by minimizing route ends; so we can
      // maximize order starts to pack them together.
      routing.AddVariableMaximizedByFinalizer(order_start);
    }
    // Only one order can happen at the same time at a given location.
    std::vector<int64> location_usage(stop_intervals.size(), 1);
    solver->AddConstraint(
        solver->MakeCumulative(
            stop_intervals, location_usage, 1, StrCat("Client", stop)));
  }
  // Minimizing route duration.
  for (int vehicle = 0 ; vehicle < routing.vehicles(); ++vehicle) {
    routing.AddVariableMinimizedByFinalizer(
        routing.CumulVar(routing.End(vehicle), kTime));
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
    LOG(INFO) << "Stop intervals:";
    for (IntervalVar* const interval : intervals) {
      if (solution->PerformedValue(interval)) {
        LOG(INFO) << interval->name() << ": " << solution->StartValue(interval);
      }
    }
  } else {
    LOG(INFO) << "No solution found.";
  }
  return 0;
}
