// Copyright 2010-2021 Google LLC
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
// specified in the data but can be overridden using the --pdp_force_vehicles
// flag.
//
// A further description of the problem can be found here:
// http://en.wikipedia.org/wiki/Vehicle_routing_problem
// http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.123.9965&rep=rep1&type=pdf.
// Reads data in the format defined by Li & Lim
// (https://www.sintef.no/projectweb/top/pdptw/li-lim-benchmark/documentation/).

#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/file.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/timer.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_enums.pb.h"
#include "ortools/constraint_solver/routing_index_manager.h"
#include "ortools/constraint_solver/routing_parameters.h"
#include "ortools/constraint_solver/routing_parameters.pb.h"

ABSL_FLAG(std::string, pdp_file, "",
          "File containing the Pickup and Delivery Problem to solve.");
ABSL_FLAG(int, pdp_force_vehicles, 0,
          "Force the number of vehicles used (maximum number of routes.");
ABSL_FLAG(bool, reduce_vehicle_cost_model, true,
          "Overrides the homonymous field of "
          "DefaultRoutingModelParameters().");
ABSL_FLAG(std::string, routing_search_parameters,
          "first_solution_strategy:ALL_UNPERFORMED",
          "Text proto RoutingSearchParameters (possibly partial) that will "
          "override the DefaultRoutingSearchParameters()");

namespace operations_research {

// Scaling factor used to scale up distances, allowing a bit more precision
// from Euclidean distances.
const int64_t kScalingFactor = 1000;

// Vector of (x,y) node coordinates, *unscaled*, in some imaginary planar,
// metric grid.
typedef std::vector<std::pair<int, int> > Coordinates;

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

// Returns the list of variables to use for the Tabu metaheuristic.
// The current list is:
// - Total cost of the solution,
// - Number of used vehicles,
// - Total schedule duration.
// TODO(user): add total waiting time.
std::vector<IntVar*> GetTabuVars(std::vector<IntVar*> existing_vars,
                                 operations_research::RoutingModel* routing) {
  Solver* const solver = routing->solver();
  std::vector<IntVar*> vars(std::move(existing_vars));
  vars.push_back(routing->CostVar());

  IntVar* used_vehicles = solver->MakeIntVar(0, routing->vehicles());
  std::vector<IntVar*> is_used_vars;
  // Number of vehicle used
  is_used_vars.reserve(routing->vehicles());
  for (int v = 0; v < routing->vehicles(); v++) {
    is_used_vars.push_back(solver->MakeIsDifferentCstVar(
        routing->NextVar(routing->Start(v)), routing->End(v)));
  }
  solver->AddConstraint(
      solver->MakeEquality(solver->MakeSum(is_used_vars), used_vehicles));
  vars.push_back(used_vehicles);

  return vars;
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
  std::istringstream input(str);
  int64_t x;
  parsed_int->clear();
  while (input >> x) parsed_int->push_back(x);
  return input.eof();
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
  std::vector<std::pair<int, int> > coords;
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
  RoutingTransitCallback2 demand_evaluator = [&](int64_t from_index,
                                                 int64_t to_index) {
    return demands[manager.IndexToNode(from_index).value()];
  };
  routing.AddDimension(routing.RegisterTransitCallback(demand_evaluator), 0,
                       capacity, /*fix_start_cumul_to_zero=*/true, "demand");
  RoutingTransitCallback2 time_evaluator = [&](int64_t from_index,
                                               int64_t to_index) {
    return TravelPlusServiceTime(manager, &coords, &service_times, from_index,
                                 to_index);
  };
  routing.AddDimension(routing.RegisterTransitCallback(time_evaluator),
                       kScalingFactor * horizon, kScalingFactor * horizon,
                       /*fix_start_cumul_to_zero=*/true, "time");
  const RoutingDimension& time_dimension = routing.GetDimensionOrDie("time");
  Solver* const solver = routing.solver();
  for (int node = 0; node < num_nodes; ++node) {
    const int64_t index =
        manager.NodeToIndex(RoutingIndexManager::NodeIndex(node));
    if (pickups[node] == 0 && deliveries[node] != 0) {
      const int64_t delivery_index = manager.NodeToIndex(deliveries[node]);
      solver->AddConstraint(solver->MakeEquality(
          routing.VehicleVar(index), routing.VehicleVar(delivery_index)));
      solver->AddConstraint(
          solver->MakeLessOrEqual(time_dimension.CumulVar(index),
                                  time_dimension.CumulVar(delivery_index)));
      routing.AddPickupAndDelivery(index,
                                   manager.NodeToIndex(deliveries[node]));
    }
    IntVar* const cumul = time_dimension.CumulVar(index);
    cumul->SetMin(kScalingFactor * open_times[node]);
    cumul->SetMax(kScalingFactor * close_times[node]);
  }

  if (search_parameters.local_search_metaheuristic() ==
      LocalSearchMetaheuristic::GENERIC_TABU_SEARCH) {
    // Create variable for the total schedule time of the solution.
    // This will be used as one of the Tabu criteria.
    // This is done here and not in GetTabuVarsCallback as it requires calling
    // AddVariableMinimizedByFinalizer and this method must be called early.
    std::vector<IntVar*> end_cumuls;
    std::vector<IntVar*> start_cumuls;
    for (int i = 0; i < routing.vehicles(); ++i) {
      end_cumuls.push_back(time_dimension.CumulVar(routing.End(i)));
      start_cumuls.push_back(time_dimension.CumulVar(routing.Start(i)));
    }
    IntVar* total_time = solver->MakeIntVar(0, 99999999, "total");
    solver->AddConstraint(solver->MakeEquality(
        solver->MakeDifference(solver->MakeSum(end_cumuls),
                               solver->MakeSum(start_cumuls)),
        total_time));

    routing.AddVariableMinimizedByFinalizer(total_time);

    RoutingModel::GetTabuVarsCallback tabu_var_callback =
        [total_time](RoutingModel* model) {
          return GetTabuVars({total_time}, model);
        };
    routing.SetTabuVarsCallback(tabu_var_callback);
  }

  // Adding penalty costs to allow skipping orders.
  const int64_t kPenalty = 10000000;
  for (RoutingIndexManager::NodeIndex order(1); order < routing.nodes();
       ++order) {
    std::vector<int64_t> orders(1, manager.NodeToIndex(order));
    routing.AddDisjunction(orders, kPenalty);
  }

  // Solve pickup and delivery problem.
  SimpleCycleTimer timer;
  timer.Start();
  const Assignment* assignment = routing.SolveWithParameters(search_parameters);
  timer.Stop();
  LOG(INFO) << routing.solver()->LocalSearchProfile();
  if (nullptr != assignment) {
    LOG(INFO) << VerboseOutput(routing, manager, *assignment, coords,
                               service_times);
    LOG(INFO) << "Cost: " << assignment->ObjectiveValue();
    int skipped_nodes = 0;
    for (int node = 0; node < routing.Size(); node++) {
      if (!routing.IsEnd(node) && !routing.IsStart(node) &&
          assignment->Value(routing.NextVar(node)) == node) {
        skipped_nodes++;
      }
    }
    LOG(INFO) << "Number of skipped nodes: " << skipped_nodes;
    int num_used_vehicles = 0;
    for (int v = 0; v < routing.vehicles(); v++) {
      if (routing.IsVehicleUsed(*assignment, v)) {
        num_used_vehicles++;
      }
    }
    LOG(INFO) << "Number of used vehicles: " << num_used_vehicles;
    LOG(INFO) << "Time: " << timer.Get();
    return true;
  }
  return false;
}

}  // namespace operations_research

int main(int argc, char** argv) {
  absl::SetFlag(&FLAGS_logtostderr, true);
  google::InitGoogleLogging(argv[0]);
  absl::ParseCommandLine(argc, argv);
  operations_research::RoutingModelParameters model_parameters =
      operations_research::DefaultRoutingModelParameters();
  model_parameters.set_reduce_vehicle_cost_model(
      absl::GetFlag(FLAGS_reduce_vehicle_cost_model));
  operations_research::RoutingSearchParameters search_parameters =
      operations_research::DefaultRoutingSearchParameters();
  CHECK(google::protobuf::TextFormat::MergeFromString(
      absl::GetFlag(FLAGS_routing_search_parameters), &search_parameters));
  if (!operations_research::LoadAndSolve(absl::GetFlag(FLAGS_pdp_file),
                                         model_parameters, search_parameters)) {
    LOG(INFO) << "Error solving " << absl::GetFlag(FLAGS_pdp_file);
  }
  return EXIT_SUCCESS;
}
