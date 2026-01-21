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

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/log_severity.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/init_google.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/timer.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/routing/enums.pb.h"
#include "ortools/routing/index_manager.h"
#include "ortools/routing/parameters.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/routing/parsers/lilim_parser.h"
#include "ortools/routing/parsers/simple_graph.h"
#include "ortools/routing/routing.h"
#include "ortools/routing/types.h"

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
ABSL_FLAG(std::string, routing_model_parameters, "",
          "Text proto RoutingModelParameters (possibly partial) that will "
          "override the DefaultRoutingModelParameters()");

namespace operations_research::routing {
namespace {

// Returns the list of variables to use for the Tabu metaheuristic.
// The current list is:
// - Total cost of the solution,
// - Number of used vehicles,
// - Total schedule duration.
// TODO(user): add total waiting time.
std::vector<IntVar*> GetTabuVars(std::vector<IntVar*> existing_vars,
                                 RoutingModel* routing) {
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

// Scaling factor from callback.
template <typename C>
double ComputeScalingFactorFromCallback(const C& callback, int size) {
  double max_value = 0;
  for (int i = 0; i < size; ++i) {
    for (int j = 0; j < size; ++j) {
      max_value = std::max(max_value, callback(i, j));
    }
  }
  const double max_scaled_total_distance =
      (1LL << (std::numeric_limits<double>::digits - 2)) - 1;
  const double max_scaled_distance = max_scaled_total_distance / size;
  return max_scaled_distance / max_value;
}

void SetupModel(const routing::LiLimParser& parser,
                const RoutingIndexManager& manager, RoutingModel* model,
                RoutingSearchParameters* search_parameters) {
  const int64_t kPenalty = 100000000;
  const int64_t kFixedCost = 100000;
  const int num_nodes = parser.NumberOfNodes();
  const int64_t horizon =
      absl::c_max_element(parser.time_windows(),
                          [](const routing::SimpleTimeWindow<int64_t>& a,
                             const routing::SimpleTimeWindow<int64_t>& b) {
                            return a.end < b.end;
                          })
          ->end;
  const double scaling_factor = ComputeScalingFactorFromCallback(
      [&parser](int64_t i, int64_t j) -> double {
        const int depot = parser.Depot();
        double fixed_cost = 0;
        if (i == depot && j != depot) {
          fixed_cost = kFixedCost;
        } else if (i == j && i != depot) {
          return kPenalty;
        }
        return fixed_cost + parser.GetTravelTime(i, j);
      },
      manager.num_nodes());
  search_parameters->set_log_cost_scaling_factor(1.0 / scaling_factor);
  const int vehicle_cost = model->RegisterTransitCallback(
      [&parser, &manager, scaling_factor](int64_t i, int64_t j) {
        return MathUtil::Round<int64_t>(
            scaling_factor *
            parser.GetDistance(manager.IndexToNode(i).value(),
                               manager.IndexToNode(j).value()));
      });
  model->SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
  model->SetFixedCostOfAllVehicles(
      MathUtil::Round<int64_t>(kFixedCost * scaling_factor));
  RoutingTransitCallback2 demand_evaluator =
      [&parser, &manager](int64_t from_index, int64_t /*to_index*/) {
        return parser.demands()[manager.IndexToNode(from_index).value()];
      };
  model->AddDimension(model->RegisterTransitCallback(demand_evaluator), 0,
                      parser.capacity(), /*fix_start_cumul_to_zero=*/true,
                      "demand");
  RoutingTransitCallback2 time_evaluator = [&parser, &manager, scaling_factor](
                                               int64_t from_index,
                                               int64_t to_index) {
    int64_t value = MathUtil::Round<int64_t>(
        scaling_factor *
        parser.GetTravelTime(manager.IndexToNode(from_index).value(),
                             manager.IndexToNode(to_index).value()));
    return value;
  };
  model->AddDimension(model->RegisterTransitCallback(time_evaluator),
                      MathUtil::FastInt64Round(scaling_factor * horizon),
                      MathUtil::FastInt64Round(scaling_factor * horizon),
                      /*fix_start_cumul_to_zero=*/true, "time");
  const RoutingDimension& time_dimension = model->GetDimensionOrDie("time");
  Solver* const solver = model->solver();
  for (int node = 0; node < num_nodes; ++node) {
    const int64_t index = manager.NodeToIndex(NodeIndex(node));
    if (const std::optional<int> delivery = parser.GetDelivery(node);
        delivery.has_value()) {
      const int64_t delivery_index =
          manager.NodeToIndex(NodeIndex(delivery.value()));
      solver->AddConstraint(solver->MakeEquality(
          model->VehicleVar(index), model->VehicleVar(delivery_index)));
      solver->AddConstraint(
          solver->MakeLessOrEqual(time_dimension.CumulVar(index),
                                  time_dimension.CumulVar(delivery_index)));
      model->AddPickupAndDelivery(index, delivery_index);
    }
    IntVar* const cumul = time_dimension.CumulVar(index);
    const routing::SimpleTimeWindow<int64_t>& window =
        parser.time_windows()[node];
    cumul->SetMin(MathUtil::Round<int64_t>(scaling_factor * window.start));
    cumul->SetMax(MathUtil::Round<int64_t>(scaling_factor * window.end));
  }

  if (search_parameters->local_search_metaheuristic() ==
      LocalSearchMetaheuristic::GENERIC_TABU_SEARCH) {
    // Create variable for the total schedule time of the solution.
    // This will be used as one of the Tabu criteria.
    // This is done here and not in GetTabuVarsCallback as it requires calling
    // AddVariableMinimizedByFinalizer and this method must be called early.
    std::vector<IntVar*> end_cumuls;
    std::vector<IntVar*> start_cumuls;
    for (int i = 0; i < model->vehicles(); ++i) {
      end_cumuls.push_back(time_dimension.CumulVar(model->End(i)));
      start_cumuls.push_back(time_dimension.CumulVar(model->Start(i)));
    }
    IntVar* total_time = solver->MakeIntVar(0, 99999999, "total");
    solver->AddConstraint(solver->MakeEquality(
        solver->MakeDifference(solver->MakeSum(end_cumuls),
                               solver->MakeSum(start_cumuls)),
        total_time));

    model->AddVariableMinimizedByFinalizer(total_time);

    RoutingModel::GetTabuVarsCallback tabu_var_callback =
        [total_time](RoutingModel* model) {
          return GetTabuVars({total_time}, model);
        };
    model->SetTabuVarsCallback(tabu_var_callback);
  }

  // Adding penalty costs to allow skipping orders.
  for (NodeIndex order(1); order < model->nodes(); ++order) {
    std::vector<int64_t> orders(1, manager.NodeToIndex(order));
    model->AddDisjunction(orders,
                          MathUtil::Round<int64_t>(scaling_factor * kPenalty));
  }
}

// Outputs a solution to the current model in a string.
std::string VerboseOutput(const RoutingModel& model,
                          const RoutingIndexManager& manager,
                          const Assignment& assignment,
                          const routing::LiLimParser& parser,
                          double scaling_factor) {
  std::string output;
  const RoutingDimension& time_dimension = model.GetDimensionOrDie("time");
  const RoutingDimension& load_dimension = model.GetDimensionOrDie("demand");
  for (int i = 0; i < model.vehicles(); ++i) {
    absl::StrAppendFormat(&output, "Vehicle %d: ", i);
    int64_t index = model.Start(i);
    if (model.IsEnd(assignment.Value(model.NextVar(index)))) {
      output.append("empty");
    } else {
      while (!model.IsEnd(index)) {
        absl::StrAppendFormat(&output, "%d ",
                              manager.IndexToNode(index).value());
        const IntVar* vehicle = model.VehicleVar(index);
        absl::StrAppendFormat(&output, "Vehicle(%d) ",
                              assignment.Value(vehicle));
        const IntVar* arrival = time_dimension.CumulVar(index);
        absl::StrAppendFormat(
            &output, "Time(%d..%d) ",
            MathUtil::Round<int64_t>(assignment.Min(arrival) * scaling_factor),
            MathUtil::Round<int64_t>(assignment.Max(arrival) * scaling_factor));
        const IntVar* load = load_dimension.CumulVar(index);
        absl::StrAppendFormat(&output, "Load(%d..%d) ", assignment.Min(load),
                              assignment.Max(load));
        const int64_t next_index = assignment.Value(model.NextVar(index));
        absl::StrAppendFormat(
            &output, "Transit(%f) ",
            parser.GetTravelTime(manager.IndexToNode(index).value(),
                                 manager.IndexToNode(next_index).value()));
        index = next_index;
      }
      output.append("Route end ");
      const IntVar* vehicle = model.VehicleVar(index);
      absl::StrAppendFormat(&output, "Vehicle(%d) ", assignment.Value(vehicle));
      const IntVar* arrival = time_dimension.CumulVar(index);
      absl::StrAppendFormat(
          &output, "Time(%d..%d) ",
          MathUtil::Round<int64_t>(assignment.Min(arrival) * scaling_factor),
          MathUtil::Round<int64_t>(assignment.Max(arrival) * scaling_factor));
      const IntVar* load = load_dimension.CumulVar(index);
      absl::StrAppendFormat(&output, "Load(%d..%d) ", assignment.Min(load),
                            assignment.Max(load));
    }
    output.append("\n");
  }
  return output;
}
}  // namespace

// Builds and solves a model from a file in the format defined by Li & Lim
// (https://www.sintef.no/projectweb/top/pdptw/li-lim-benchmark/documentation/).
bool LoadAndSolve(absl::string_view pdp_file,
                  const RoutingModelParameters& model_parameters,
                  RoutingSearchParameters& search_parameters) {
  routing::LiLimParser parser;
  if (!parser.LoadFile(pdp_file)) {
    return false;
  }

  // Build pickup and delivery model.
  const int num_nodes = parser.NumberOfNodes();
  const int num_vehicles = parser.NumberOfVehicles();
  const NodeIndex depot = NodeIndex(parser.Depot());
  RoutingIndexManager manager(num_nodes, num_vehicles, depot);
  RoutingModel model(manager, model_parameters);
  SetupModel(parser, manager, &model, &search_parameters);

  // Solve pickup and delivery problem.
  SimpleCycleTimer timer;
  timer.Start();
  const Assignment* assignment = model.SolveWithParameters(search_parameters);
  timer.Stop();
  LOG(INFO) << model.solver()->LocalSearchProfile();
  if (nullptr != assignment) {
    const double scaling_factor = search_parameters.log_cost_scaling_factor();
    LOG(INFO) << VerboseOutput(model, manager, *assignment, parser,
                               scaling_factor);
    const int64_t cost = assignment->ObjectiveValue();
    LOG(INFO) << absl::StrFormat("Cost: %f", cost * scaling_factor);
    int num_used_vehicles = 0;
    int64_t total_fixed_cost = 0;
    for (int v = 0; v < model.vehicles(); v++) {
      if (model.IsVehicleUsed(*assignment, v)) {
        num_used_vehicles++;
        total_fixed_cost += model.GetFixedCostOfVehicle(v);
      }
    }
    int skipped_nodes = 0;
    int64_t total_penalty = 0;
    for (int node = 0; node < model.Size(); node++) {
      if (!model.IsEnd(node) && !model.IsStart(node) &&
          assignment->Value(model.NextVar(node)) == node) {
        skipped_nodes++;
        for (DisjunctionIndex disjunction : model.GetDisjunctionIndices(node)) {
          total_penalty += model.GetDisjunctionPenalty(disjunction);
        }
      }
    }
    LOG(INFO) << absl::StrFormat(
        "Distance: %.2f",
        (cost - total_fixed_cost - total_penalty) * scaling_factor);
    LOG(INFO) << "Number of skipped nodes: " << skipped_nodes;
    LOG(INFO) << "Number of used vehicles: " << num_used_vehicles;
    LOG(INFO) << "Time: " << timer.Get();
    return true;
  }
  return false;
}

}  // namespace operations_research::routing

namespace o_r = ::operations_research::routing;

int main(int argc, char** argv) {
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  InitGoogle(argv[0], &argc, &argv, true);
  o_r::RoutingModelParameters model_parameters =
      o_r::DefaultRoutingModelParameters();
  model_parameters.set_reduce_vehicle_cost_model(
      absl::GetFlag(FLAGS_reduce_vehicle_cost_model));
  CHECK(google::protobuf::TextFormat::MergeFromString(
      absl::GetFlag(FLAGS_routing_model_parameters), &model_parameters));
  o_r::RoutingSearchParameters search_parameters =
      o_r::DefaultRoutingSearchParameters();
  CHECK(google::protobuf::TextFormat::MergeFromString(
      absl::GetFlag(FLAGS_routing_search_parameters), &search_parameters));
  if (!o_r::LoadAndSolve(absl::GetFlag(FLAGS_pdp_file), model_parameters,
                         search_parameters)) {
    LOG(INFO) << "Error solving " << absl::GetFlag(FLAGS_pdp_file);
  }
  return EXIT_SUCCESS;
}
