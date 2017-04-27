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

//
// Pickup and Delivery Problem with Time Windows.
// The overall objective is to minimize the length of the routes delivering
// quantities of goods between pickup and delivery locations, taking into
// account vehicle capacities and node time windows.
// Given a set of pairs of pickup and delivery nodes, find the set of routes
// visiting all the nodes, such that
// - corresponding pickup and delivery nodes are visited on the same route,
// - the pickup node is visited before the corresponding delivery node,
// - the quantity picked up at the pickup node is the same as the quantity
//   delivered at the delivery node,
// - the total quantity carried by a vehicle at any time is less than its
//   capacity,
// - each node must be visited within its time window (time range during which
//   the node is accessible).
// The maximum number of vehicles used (i.e. the number of routes used) is
// specified in the data but can be overriden using the --pdp_force_vehicles
// flag.
//
// A further description of the problem can be found here:
// http://en.wikipedia.org/wiki/Vehicle_routing_problem
// http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.123.9965&rep=rep1&type=pdf.
// Reads data in the format defined by Li & Lim
// (https://www.sintef.no/projectweb/top/pdptw/li-lim-benchmark/documentation/).

#include <vector>

#include "ortools/base/callback.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/strtoint.h"
#include "ortools/base/file.h"
#include "ortools/base/split.h"
#include "ortools/base/mathutil.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_enums.pb.h"
#include "ortools/constraint_solver/routing_flags.h"

DEFINE_string(pdp_file, "",
              "File containing the Pickup and Delivery Problem to solve.");
DEFINE_int32(pdp_force_vehicles, 0,
             "Force the number of vehicles used (maximum number of routes.");
DECLARE_string(routing_first_solution);

namespace operations_research {

// Scaling factor used to scale up distances, allowing a bit more precision
// from Euclidean distances.
const int64 kScalingFactor = 1000;

// Vector of (x,y) node coordinates, *unscaled*, in some imaginary planar,
// metric grid.
typedef std::vector<std::pair<int, int> > Coordinates;

// Returns the scaled Euclidean distance between two nodes, coords holding the
// coordinates of the nodes.
int64 Travel(const Coordinates* const coords, RoutingModel::NodeIndex from,
             RoutingModel::NodeIndex to) {
  DCHECK(coords != nullptr);
  const int xd = coords->at(from.value()).first - coords->at(to.value()).first;
  const int yd =
      coords->at(from.value()).second - coords->at(to.value()).second;
  return static_cast<int64>(kScalingFactor * sqrt(1.0L * xd * xd + yd * yd));
}

// Returns the scaled service time at a given node, service_times holding the
// service times.
int64 ServiceTime(const std::vector<int64>* const service_times,
                  RoutingModel::NodeIndex node) {
  return kScalingFactor * service_times->at(node.value());
}

// Returns the scaled (distance plus service time) between two nodes, coords
// holding the coordinates of the nodes and service_times holding the service
// times.
// The service time is the time spent to execute a delivery or a pickup.
int64 TravelPlusServiceTime(const Coordinates* const coords,
                            const std::vector<int64>* const service_times,
                            RoutingModel::NodeIndex from,
                            RoutingModel::NodeIndex to) {
  return ServiceTime(service_times, from) + Travel(coords, from, to);
}

// Returns the demand (quantity picked up or delivered) of a node, demands
// holds the demand of each node.
int64 Demand(const std::vector<int64>* const demands,
             RoutingModel::NodeIndex from, RoutingModel::NodeIndex to) {
  return demands->at(from.value());
}

// Outputs a solution to the current model in a std::string.
std::string VerboseOutput(const RoutingModel& routing, const Assignment& assignment,
                     const Coordinates& coords,
                     const std::vector<int64>& service_times) {
  std::string output;
  const RoutingDimension& time_dimension = routing.GetDimensionOrDie("time");
  const RoutingDimension& load_dimension = routing.GetDimensionOrDie("demand");
  for (int i = 0; i < routing.vehicles(); ++i) {
    StringAppendF(&output, "Vehicle %d: ", i);
    int64 index = routing.Start(i);
    if (routing.IsEnd(assignment.Value(routing.NextVar(index)))) {
      output.append("empty");
    } else {
      while (!routing.IsEnd(index)) {
        RoutingModel::NodeIndex real_node = routing.IndexToNode(index);
        StringAppendF(&output, "%d ", real_node.value());
        const IntVar* vehicle = routing.VehicleVar(index);
        StringAppendF(&output, "Vehicle(%lld) ", assignment.Value(vehicle));
        const IntVar* arrival = time_dimension.CumulVar(index);
        StringAppendF(&output, "Time(%lld..%lld) ", assignment.Min(arrival),
                      assignment.Max(arrival));
        const IntVar* load = load_dimension.CumulVar(index);
        StringAppendF(&output, "Load(%lld..%lld) ", assignment.Min(load),
                      assignment.Max(load));
        index = assignment.Value(routing.NextVar(index));
        StringAppendF(&output, "Transit(%lld) ",
                      TravelPlusServiceTime(&coords, &service_times, real_node,
                                            routing.IndexToNode(index)));
      }
      output.append("Route end ");
      const IntVar* vehicle = routing.VehicleVar(index);
      StringAppendF(&output, "Vehicle(%lld) ", assignment.Value(vehicle));
      const IntVar* arrival = time_dimension.CumulVar(index);
      StringAppendF(&output, "Time(%lld..%lld) ", assignment.Min(arrival),
                    assignment.Max(arrival));
      const IntVar* load = load_dimension.CumulVar(index);
      StringAppendF(&output, "Load(%lld..%lld) ", assignment.Min(load),
                    assignment.Max(load));
    }
    output.append("\n");
  }
  return output;
}

namespace {
// An inefficient but convenient method to parse a whitespace-separated list
// of integers. Returns true iff the input std::string was entirely valid and parsed.
bool SafeParseInt64Array(const std::string& str, std::vector<int64>* parsed_int) {
  static const char kWhiteSpaces[] = " \t\n\v\f\r";
  std::vector<std::string> items = strings::Split(
      str, strings::delimiter::AnyOf(kWhiteSpaces), strings::SkipEmpty());
  parsed_int->assign(items.size(), 0);
  for (int i = 0; i < items.size(); ++i) {
    const char* item = items[i].c_str();
    char* endptr = nullptr;
    (*parsed_int)[i] = strto64(item, &endptr, 10);
    // The whole item should have been consumed.
    if (*endptr != '\0') return false;
  }
  return true;
}
}  // namespace

// Builds and solves a model from a file in the format defined by Li & Lim
// (https://www.sintef.no/projectweb/top/pdptw/li-lim-benchmark/documentation/).
bool LoadAndSolve(const std::string& pdp_file) {
  // Load all the lines of the file in RAM (it shouldn't be too large anyway).
  std::vector<std::string> lines;
  {
    std::string contents;
    CHECK_OK(file::GetContents(pdp_file, &contents, file::Defaults()));
    const int64 kMaxInputFileSize = 1 << 30;  // 1GB
    if (contents.size() >= kMaxInputFileSize) {
      LOG(WARNING) << "Input file '" << pdp_file << "' is too large (>"
                   << kMaxInputFileSize << " bytes).";
      return false;
    }
    lines = strings::Split(contents, '\n', strings::SkipEmpty());
  }
  // Reading header.
  if (lines.empty()) {
    LOG(WARNING) << "Empty file: " << pdp_file;
    return false;
  }
  // Parse file header.
  std::vector<int64> parsed_int;
  if (!SafeParseInt64Array(lines[0], &parsed_int) || parsed_int.size() != 3 ||
      parsed_int[0] < 0 || parsed_int[1] < 0 || parsed_int[2] < 0) {
    LOG(WARNING) << "Malformed header: " << lines[0];
    return false;
  }
  const int num_vehicles =
      FLAGS_pdp_force_vehicles > 0 ? FLAGS_pdp_force_vehicles : parsed_int[0];
  const int64 capacity = parsed_int[1];
  // We do not care about the 'speed' field, in third position.

  // Parse order data.
  std::vector<int> customer_ids;
  std::vector<std::pair<int, int> > coords;
  std::vector<int64> demands;
  std::vector<int64> open_times;
  std::vector<int64> close_times;
  std::vector<int64> service_times;
  std::vector<RoutingModel::NodeIndex> pickups;
  std::vector<RoutingModel::NodeIndex> deliveries;
  int64 horizon = 0;
  RoutingModel::NodeIndex depot(0);
  for (int line_index = 1; line_index < lines.size(); ++line_index) {
    if (!SafeParseInt64Array(lines[line_index], &parsed_int) ||
        parsed_int.size() != 9 || parsed_int[0] < 0 || parsed_int[4] < 0 ||
        parsed_int[5] < 0 || parsed_int[6] < 0 || parsed_int[7] < 0 ||
        parsed_int[8] < 0) {
      LOG(WARNING) << "Malformed line #" << line_index << ": "
                   << lines[line_index];
      return false;
    }
    const int customer_id = parsed_int[0];
    const int x = parsed_int[1];
    const int y = parsed_int[2];
    const int64 demand = parsed_int[3];
    const int64 open_time = parsed_int[4];
    const int64 close_time = parsed_int[5];
    const int64 service_time = parsed_int[6];
    const int pickup = parsed_int[7];
    const int delivery = parsed_int[8];
    customer_ids.push_back(customer_id);
    coords.push_back(std::make_pair(x, y));
    demands.push_back(demand);
    open_times.push_back(open_time);
    close_times.push_back(close_time);
    service_times.push_back(service_time);
    pickups.push_back(RoutingModel::NodeIndex(pickup));
    deliveries.push_back(RoutingModel::NodeIndex(delivery));
    if (pickup == 0 && delivery == 0) {
      depot = RoutingModel::NodeIndex(pickups.size() - 1);
    }
    horizon = std::max(horizon, close_time);
  }

  // Build pickup and delivery model.
  const int num_nodes = customer_ids.size();
  RoutingModelParameters model_parameters = BuildModelParametersFromFlags();
  RoutingModel routing(num_nodes, num_vehicles, depot, model_parameters);
  routing.SetArcCostEvaluatorOfAllVehicles(
      NewPermanentCallback(Travel, const_cast<const Coordinates*>(&coords)));
  routing.AddDimension(
      NewPermanentCallback(&Demand,
                           const_cast<const std::vector<int64>*>(&demands)),
      0, capacity, /*fix_start_cumul_to_zero=*/true, "demand");
  routing.AddDimension(
      NewPermanentCallback(
          &TravelPlusServiceTime, const_cast<const Coordinates*>(&coords),
          const_cast<const std::vector<int64>*>(&service_times)),
      kScalingFactor * horizon, kScalingFactor * horizon,
      /*fix_start_cumul_to_zero=*/true, "time");
  const RoutingDimension& time_dimension = routing.GetDimensionOrDie("time");
  Solver* const solver = routing.solver();
  for (RoutingModel::NodeIndex i(0); i < num_nodes; ++i) {
    const int64 index = routing.NodeToIndex(i);
    if (pickups[i.value()] == 0 && deliveries[i.value()] != 0) {
      const int64 delivery_index = routing.NodeToIndex(deliveries[i.value()]);
      solver->AddConstraint(solver->MakeEquality(
          routing.VehicleVar(index), routing.VehicleVar(delivery_index)));
      solver->AddConstraint(
          solver->MakeLessOrEqual(time_dimension.CumulVar(index),
                                  time_dimension.CumulVar(delivery_index)));
      routing.AddPickupAndDelivery(i, deliveries[i.value()]);
    }
    IntVar* const cumul = time_dimension.CumulVar(index);
    cumul->SetMin(kScalingFactor * open_times[i.value()]);
    cumul->SetMax(kScalingFactor * close_times[i.value()]);
  }
  // Adding penalty costs to allow skipping orders.
  const int64 kPenalty = 10000000;
  for (RoutingModel::NodeIndex order(1); order < routing.nodes(); ++order) {
    std::vector<RoutingModel::NodeIndex> orders(1, order);
    routing.AddDisjunction(orders, kPenalty);
  }

  // Set up search parameters.
  RoutingSearchParameters parameters = BuildSearchParametersFromFlags();
  if (FLAGS_routing_first_solution.empty()) {
    parameters.set_first_solution_strategy(
        operations_research::FirstSolutionStrategy::ALL_UNPERFORMED);
  }
  parameters.mutable_local_search_operators()->set_use_path_lns(false);

  // Solve pickup and delivery problem.
  const Assignment* assignment = routing.SolveWithParameters(parameters);
  LOG(INFO) << routing.solver()->LocalSearchProfile();
  if (nullptr != assignment) {
    LOG(INFO) << "Cost: " << assignment->ObjectiveValue();
    LOG(INFO) << VerboseOutput(routing, *assignment, coords, service_times);
    return true;
  }
  return false;
}

}  // namespace operations_research

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags( &argc, &argv, true);
  if (!operations_research::LoadAndSolve(FLAGS_pdp_file)) {
    LOG(INFO) << "Error solving " << FLAGS_pdp_file;
  }
  return 0;
}
