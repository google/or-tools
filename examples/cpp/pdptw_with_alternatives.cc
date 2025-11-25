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

// Pickup and Delivery Problem with Time Windows and Alternatives.
// This is a variant of the mode in pdptw.cc (see that file for more details
// on pickup and delivery models). In this model both pickups and deliveries
// have alternative locations, of which one of each has to be selected. As in
// the standard pickup and delivery problem, pickups must happen before
// deliveries and must be on the same route.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/log_severity.h"
#include "absl/flags/flag.h"
#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/log/log.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/helpers.h"
#include "ortools/base/init_google.h"
#include "ortools/base/options.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/routing/index_manager.h"
#include "ortools/routing/parameters.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/routing/routing.h"

ABSL_FLAG(std::string, pdp_file, "",
          "File containing the Pickup and Delivery Problem to solve.");
ABSL_FLAG(int, pdp_force_vehicles, 0,
          "Force the number of vehicles used (maximum number of routes.");
ABSL_FLAG(bool, reduce_vehicle_cost_model, true,
          "Overrides the homonymous field of "
          "DefaultRoutingModelParameters().");
ABSL_FLAG(std::string, routing_search_parameters,
          "first_solution_strategy:ALL_UNPERFORMED "
          "local_search_operators { use_node_pair_swap_active:BOOL_FALSE }",
          "Text proto RoutingSearchParameters (possibly partial) that will "
          "override the DefaultRoutingSearchParameters()");

using ::absl::bind_front;

namespace operations_research::routing {

// Scaling factor used to scale up distances, allowing a bit more precision
// from Euclidean distances.
const int64_t kScalingFactor = 1000;

// Vector of (x,y) node coordinates, *unscaled*, in some imaginary planar,
// metric grid.
typedef std::vector<std::pair<int, int>> Coordinates;

// Returns the scaled Euclidean distance between two nodes, coords holding the
// coordinates of the nodes.
int64_t Travel(const Coordinates* const coords,
               RoutingIndexManager::NodeIndex from,
               RoutingIndexManager::NodeIndex to) {
  DCHECK(coords != nullptr);
  const int xd = coords->at(from.value()).first - coords->at(to.value()).first;
  const int yd =
      coords->at(from.value()).second - coords->at(to.value()).second;
  return static_cast<int64_t>(kScalingFactor *
                              std::sqrt(1.0L * xd * xd + yd * yd));
}

// Returns the scaled service time at a given node, service_times holding the
// service times.
int64_t ServiceTime(const std::vector<int64_t>* const service_times,
                    RoutingIndexManager::NodeIndex node) {
  return kScalingFactor * service_times->at(node.value());
}

// Returns the scaled (distance plus service time) between two indices, coords
// holding the coordinates of the nodes and service_times holding the service
// times.
// The service time is the time spent to execute a delivery or a pickup.
int64_t TravelPlusServiceTime(const RoutingIndexManager& manager,
                              const Coordinates* const coords,
                              const std::vector<int64_t>* const service_times,
                              int64_t from_index, int64_t to_index) {
  const RoutingIndexManager::NodeIndex from = manager.IndexToNode(from_index);
  const RoutingIndexManager::NodeIndex to = manager.IndexToNode(to_index);
  return ServiceTime(service_times, from) + Travel(coords, from, to);
}

// Returns the demand (quantity picked up or delivered) of an index, demands
// holds the demand of each node.
int64_t Demand(const RoutingIndexManager& manager,
               const std::vector<int64_t>* const demands, int64_t from_index,
               int64_t to_index) {
  (void)to_index;
  return demands->at(manager.IndexToNode(from_index).value());
}

// Outputs a solution to the current model in a string.
std::string VerboseOutput(const RoutingModel& routing,
                          const RoutingIndexManager& manager,
                          const Assignment& assignment,
                          const Coordinates& coords,
                          const std::vector<int64_t>& service_times) {
  std::string output;
  const RoutingDimension& time_dimension = routing.GetDimensionOrDie("time");
  const RoutingDimension& load_dimension = routing.GetDimensionOrDie("demand");
  for (int i = 0; i < routing.vehicles(); ++i) {
    absl::StrAppendFormat(&output, "Vehicle %d: ", i);
    int64_t index = routing.Start(i);
    if (routing.IsEnd(assignment.Value(routing.NextVar(index)))) {
      output.append("empty");
    } else {
      while (!routing.IsEnd(index)) {
        absl::StrAppendFormat(&output, "%d ",
                              manager.IndexToNode(index).value());
        const IntVar* vehicle = routing.VehicleVar(index);
        absl::StrAppendFormat(&output, "Vehicle(%d) ",
                              assignment.Value(vehicle));
        const IntVar* arrival = time_dimension.CumulVar(index);
        absl::StrAppendFormat(&output, "Time(%d..%d) ", assignment.Min(arrival),
                              assignment.Max(arrival));
        const IntVar* load = load_dimension.CumulVar(index);
        absl::StrAppendFormat(&output, "Load(%d..%d) ", assignment.Min(load),
                              assignment.Max(load));
        const int64_t next_index = assignment.Value(routing.NextVar(index));
        absl::StrAppendFormat(
            &output, "Transit(%d) ",
            TravelPlusServiceTime(manager, &coords, &service_times, index,
                                  next_index));
        index = next_index;
      }
      output.append("Route end ");
      const IntVar* vehicle = routing.VehicleVar(index);
      absl::StrAppendFormat(&output, "Vehicle(%d) ", assignment.Value(vehicle));
      const IntVar* arrival = time_dimension.CumulVar(index);
      absl::StrAppendFormat(&output, "Time(%d..%d) ", assignment.Min(arrival),
                            assignment.Max(arrival));
      const IntVar* load = load_dimension.CumulVar(index);
      absl::StrAppendFormat(&output, "Load(%d..%d) ", assignment.Min(load),
                            assignment.Max(load));
    }
    output.append("\n");
  }
  return output;
}

namespace {
// An inefficient but convenient method to parse a whitespace-separated list
// of integers. Returns true iff the input string was entirely valid and parsed.
bool SafeParseInt64Array(const std::string& str,
                         std::vector<int64_t>* parsed_int) {
  static const char kWhiteSpaces[] = " \t\n\v\f\r";
  parsed_int->clear();
  for (absl::string_view token :
       absl::StrSplit(str, absl::ByAnyChar(kWhiteSpaces), absl::SkipEmpty())) {
    int value;
    if (!absl::SimpleAtoi(token, &value)) return false;
    parsed_int->push_back(value);
  }
  return true;
}
}  // namespace

// Builds and solves a model from a file in the format defined by Li & Lim
// (https://www.sintef.no/projectweb/top/pdptw/li-lim-benchmark/documentation/).
bool LoadAndSolve(const std::string& pdp_file,
                  const RoutingModelParameters& model_parameters,
                  const RoutingSearchParameters& search_parameters) {
  // Load all the lines of the file in RAM (it shouldn't be too large anyway).
  std::vector<std::string> lines;
  {
    std::string contents;
    CHECK_OK(file::GetContents(pdp_file, &contents, file::Defaults()));
    const int64_t kMaxInputFileSize = 1 << 30;  // 1GB
    if (contents.size() >= kMaxInputFileSize) {
      LOG(WARNING) << "Input file '" << pdp_file << "' is too large (>"
                   << kMaxInputFileSize << " bytes).";
      return false;
    }
    lines = absl::StrSplit(contents, '\n', absl::SkipEmpty());
  }
  // Reading header.
  if (lines.empty()) {
    LOG(WARNING) << "Empty file: " << pdp_file;
    return false;
  }
  // Parse file header.
  std::vector<int64_t> parsed_int;
  if (!SafeParseInt64Array(lines[0], &parsed_int) || parsed_int.size() != 3 ||
      parsed_int[0] < 0 || parsed_int[1] < 0 || parsed_int[2] < 0) {
    LOG(WARNING) << "Malformed header: " << lines[0];
    return false;
  }
  const int num_vehicles = absl::GetFlag(FLAGS_pdp_force_vehicles) > 0
                               ? absl::GetFlag(FLAGS_pdp_force_vehicles)
                               : parsed_int[0];
  const int64_t capacity = parsed_int[1];
  // We do not care about the 'speed' field, in third position.

  // Parse order data.
  std::vector<int> customer_ids;
  std::vector<std::pair<int, int>> coords;
  std::vector<int64_t> demands;
  std::vector<int64_t> open_times;
  std::vector<int64_t> close_times;
  std::vector<int64_t> service_times;
  std::vector<RoutingIndexManager::NodeIndex> pickups;
  std::vector<RoutingIndexManager::NodeIndex> deliveries;
  int64_t horizon = 0;
  RoutingIndexManager::NodeIndex depot(0);
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
    const int64_t demand = parsed_int[3];
    const int64_t open_time = parsed_int[4];
    const int64_t close_time = parsed_int[5];
    const int64_t service_time = parsed_int[6];
    const int pickup = parsed_int[7];
    const int delivery = parsed_int[8];
    customer_ids.push_back(customer_id);
    coords.push_back(std::make_pair(x, y));
    demands.push_back(demand);
    open_times.push_back(open_time);
    close_times.push_back(close_time);
    service_times.push_back(service_time);
    pickups.push_back(RoutingIndexManager::NodeIndex(pickup));
    deliveries.push_back(RoutingIndexManager::NodeIndex(delivery));
    if (pickup == 0 && delivery == 0) {
      depot = RoutingIndexManager::NodeIndex(pickups.size() - 1);
    }
    horizon = std::max(horizon, close_time);
  }

  // Build pickup and delivery model.
  const int num_nodes = customer_ids.size();
  RoutingIndexManager manager(num_nodes, num_vehicles, depot);
  RoutingModel routing(manager, model_parameters);
  const int vehicle_cost = routing.RegisterTransitCallback(
      [&coords, &manager](int64_t i, int64_t j) {
        return Travel(const_cast<const Coordinates*>(&coords),
                      manager.IndexToNode(i), manager.IndexToNode(j));
      });
  routing.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  routing.AddDimension(
      routing.RegisterTransitCallback(absl::bind_front(
          TravelPlusServiceTime, manager,
          const_cast<const Coordinates*>(&coords),
          const_cast<const std::vector<int64_t>*>(&service_times))),
      kScalingFactor * horizon, kScalingFactor * horizon,
      /*fix_start_cumul_to_zero=*/true, "time");
  const RoutingDimension& time_dimension = routing.GetDimensionOrDie("time");
  Solver* const solver = routing.solver();

  // Collect pickup and delivery pairs and set time windows.
  std::vector<std::pair<int64_t, int64_t>> pickup_delivery_pairs;
  for (RoutingIndexManager::NodeIndex order(0); order < routing.nodes();
       ++order) {
    const int64_t index = manager.NodeToIndex(order);
    IntVar* const cumul = time_dimension.CumulVar(index);
    cumul->SetMin(kScalingFactor * open_times[order.value()]);
    cumul->SetMax(kScalingFactor * close_times[order.value()]);
    RoutingIndexManager::NodeIndex delivery = deliveries[order.value()];
    if (pickups[order.value()] == 0 && delivery != 0) {
      pickup_delivery_pairs.push_back({index, manager.NodeToIndex(delivery)});
    }
  }

  // Build groups of pickup and delivery pairs representing the alternatives of
  // pickup and delivery locations for a given shipment, and add the
  // corresponding constraints.
  const int kGroupSize = 4;
  const int64_t kPenalty = 10000000;
  // Collecting demands per group computed as the average demand for the group.
  std::vector<int64_t> group_demands(demands.size());
  for (int pair_index = 0; pair_index < pickup_delivery_pairs.size();) {
    std::vector<int64_t> pickup_indices;
    std::vector<int64_t> delivery_indices;
    std::vector<IntVar*> pickup_vehicle_variables;
    std::vector<IntVar*> delivery_vehicle_variables;
    int64_t demand_sum = 0;
    int pair_start = pair_index;
    for (int i = 0; i < kGroupSize && pair_index < pickup_delivery_pairs.size();
         ++i, ++pair_index) {
      const int64_t pickup = pickup_delivery_pairs[pair_index].first;
      const int64_t delivery = pickup_delivery_pairs[pair_index].second;
      pickup_indices.push_back(pickup);
      delivery_indices.push_back(delivery);
      pickup_vehicle_variables.push_back(routing.VehicleVar(pickup));
      delivery_vehicle_variables.push_back(routing.VehicleVar(delivery));
      demand_sum += demands[manager.IndexToNode(pickup).value()];
    }
    // Computing demand average.
    int64_t demand_avg = demand_sum / (pair_index - pair_start);
    for (int i = pair_start; i < pair_index; ++i) {
      group_demands[pickup_delivery_pairs[i].first] = demand_avg;
      group_demands[pickup_delivery_pairs[i].second] = -demand_avg;
    }
    // Unperformed pickups or deliveries will have their vehicle variable set
    // to -1. Therefore the vehicle performing the performed pickup (resp. the
    // performed delivery) is the maximum of the vehicle variables of the
    // pickups (resp. deliveries). Using this to ensure the performed pickup
    // and delivery are on the same route.
    solver->AddConstraint(
        solver->MakeEquality(solver->MakeMax(pickup_vehicle_variables),
                             solver->MakeMax(delivery_vehicle_variables)));
    // Only one pickup and one delivery must be performed and notify the solver
    // about the pickup and delivery alternatives.
    routing.AddPickupAndDeliverySets(
        routing.AddDisjunction(pickup_indices, kPenalty),
        routing.AddDisjunction(delivery_indices, kPenalty));
  }
  // Add demand dimension where the demand corresponds to the average demand
  // of the group.
  routing.AddDimension(
      routing.RegisterTransitCallback(absl::bind_front(
          Demand, manager,
          const_cast<const std::vector<int64_t>*>(&group_demands))),
      0, capacity, /*fix_start_cumul_to_zero=*/true, "demand");

  // Solve pickup and delivery problem.
  const Assignment* assignment = routing.SolveWithParameters(search_parameters);
  LOG(INFO) << routing.solver()->LocalSearchProfile();
  if (nullptr != assignment) {
    LOG(INFO) << "Cost: " << assignment->ObjectiveValue();
    LOG(INFO) << VerboseOutput(routing, manager, *assignment, coords,
                               service_times);
    return true;
  }
  return false;
}

}  // namespace operations_research::routing

int main(int argc, char** argv) {
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  InitGoogle(argv[0], &argc, &argv, true);
  // Set up model and search parameters.
  operations_research::routing::RoutingModelParameters model_parameters =
      operations_research::routing::DefaultRoutingModelParameters();
  model_parameters.set_reduce_vehicle_cost_model(
      absl::GetFlag(FLAGS_reduce_vehicle_cost_model));
  operations_research::routing::RoutingSearchParameters search_parameters =
      operations_research::routing::DefaultRoutingSearchParameters();
  CHECK(google::protobuf::TextFormat::MergeFromString(
      absl::GetFlag(FLAGS_routing_search_parameters), &search_parameters));
  if (!operations_research::routing::LoadAndSolve(
          absl::GetFlag(FLAGS_pdp_file), model_parameters, search_parameters)) {
    LOG(INFO) << "Error solving " << absl::GetFlag(FLAGS_pdp_file);
  }
  return 0;
}
