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

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/time/time.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_parameters.pb.h"
#include "ortools/constraint_solver/routing_types.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/optional_boolean.pb.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {
namespace sat {
namespace {

// As of 07/2019, TSPs and VRPs with homogeneous fleets of vehicles are
// supported.
// TODO(user): Support any type of constraints.
// TODO(user): Make VRPs properly support optional nodes.
bool RoutingModelCanBeSolvedBySat(const RoutingModel& model) {
  return model.GetVehicleClassesCount() == 1;
}

// Adds an integer variable to a CpModelProto, returning its index in the proto.
int AddVariable(CpModelProto* cp_model, int64_t lb, int64_t ub) {
  const int index = cp_model->variables_size();
  IntegerVariableProto* const var = cp_model->add_variables();
  var->add_domain(lb);
  var->add_domain(ub);
  return index;
}

// Adds a linear constraint, enforcing
// enforcement_literals -> lower_bound <= sum variable * coeff <= upper_bound.
void AddLinearConstraint(
    CpModelProto* cp_model, int64_t lower_bound, int64_t upper_bound,
    const std::vector<std::pair<int, double>>& variable_coeffs,
    const std::vector<int>& enforcement_literals) {
  CHECK_LE(lower_bound, upper_bound);
  ConstraintProto* ct = cp_model->add_constraints();
  for (const int enforcement_literal : enforcement_literals) {
    ct->add_enforcement_literal(enforcement_literal);
  }
  LinearConstraintProto* arg = ct->mutable_linear();
  arg->add_domain(lower_bound);
  arg->add_domain(upper_bound);
  for (const auto [var, coeff] : variable_coeffs) {
    arg->add_vars(var);
    arg->add_coeffs(coeff);
  }
}

// Adds a linear constraint, enforcing
// lower_bound <= sum variable * coeff <= upper_bound.
void AddLinearConstraint(
    CpModelProto* cp_model, int64_t lower_bound, int64_t upper_bound,
    const std::vector<std::pair<int, double>>& variable_coeffs) {
  AddLinearConstraint(cp_model, lower_bound, upper_bound, variable_coeffs, {});
}

// Returns the unique depot node used in the CP-SAT models (as of 01/2020).
int64_t GetDepotFromModel(const RoutingModel& model) { return model.Start(0); }

// Structure to keep track of arcs created.
struct Arc {
  int tail;
  int head;

  friend bool operator==(const Arc& a, const Arc& b) {
    return a.tail == b.tail && a.head == b.head;
  }
  friend bool operator!=(const Arc& a, const Arc& b) { return !(a == b); }
  friend bool operator<(const Arc& a, const Arc& b) {
    return a.tail == b.tail ? a.head < b.head : a.tail < b.tail;
  }
  friend std::ostream& operator<<(std::ostream& strm, const Arc& arc) {
    return strm << "{" << arc.tail << ", " << arc.head << "}";
  }
  template <typename H>
  friend H AbslHashValue(H h, const Arc& a) {
    return H::combine(std::move(h), a.tail, a.head);
  }
};

using ArcVarMap = std::map<Arc, int>;  // needs to be stable when iterating

// Adds all dimensions to a CpModelProto. Only adds path cumul constraints and
// cumul bounds.
void AddDimensions(const RoutingModel& model, const ArcVarMap& arc_vars,
                   CpModelProto* cp_model) {
  for (const RoutingDimension* dimension : model.GetDimensions()) {
    // Only a single vehicle class.
    const RoutingModel::TransitCallback2& transit =
        dimension->transit_evaluator(0);
    std::vector<int> cumuls(dimension->cumuls().size(), -1);
    const int64_t min_start = dimension->cumuls()[model.Start(0)]->Min();
    const int64_t max_end = std::min(dimension->cumuls()[model.End(0)]->Max(),
                                     dimension->vehicle_capacities()[0]);
    for (int i = 0; i < cumuls.size(); ++i) {
      if (model.IsStart(i) || model.IsEnd(i)) continue;
      // Reducing bounds supposing the triangular inequality.
      const int64_t cumul_min =
          std::max(sat::kMinIntegerValue.value(),
                   std::max(dimension->cumuls()[i]->Min(),
                            CapAdd(transit(model.Start(0), i), min_start)));
      const int64_t cumul_max =
          std::min(sat::kMaxIntegerValue.value(),
                   std::min(dimension->cumuls()[i]->Max(),
                            CapSub(max_end, transit(i, model.End(0)))));
      cumuls[i] = AddVariable(cp_model, cumul_min, cumul_max);
    }
    for (const auto arc_var : arc_vars) {
      const int tail = arc_var.first.tail;
      const int head = arc_var.first.head;
      if (tail == head || model.IsStart(tail) || model.IsStart(head)) continue;
      // arc[tail][head] -> cumuls[head] >= cumuls[tail] + transit.
      // This is a relaxation of the model as it does not consider slack max.
      AddLinearConstraint(
          cp_model, transit(tail, head), std::numeric_limits<int64_t>::max(),
          {{cumuls[head], 1}, {cumuls[tail], -1}}, {arc_var.second});
    }
  }
}

std::vector<int> CreateRanks(const RoutingModel& model,
                             const ArcVarMap& arc_vars,
                             CpModelProto* cp_model) {
  const int depot = GetDepotFromModel(model);
  const int size = model.Size() + model.vehicles();
  const int rank_size = model.Size() - model.vehicles();
  std::vector<int> ranks(size, -1);
  for (int i = 0; i < size; ++i) {
    if (model.IsStart(i) || model.IsEnd(i)) continue;
    ranks[i] = AddVariable(cp_model, 0, rank_size);
  }
  ranks[depot] = AddVariable(cp_model, 0, 0);
  for (const auto arc_var : arc_vars) {
    const int tail = arc_var.first.tail;
    const int head = arc_var.first.head;
    if (tail == head || head == depot) continue;
    // arc[tail][head] -> ranks[head] == ranks[tail] + 1.
    AddLinearConstraint(cp_model, 1, 1, {{ranks[head], 1}, {ranks[tail], -1}},
                        {arc_var.second});
  }
  return ranks;
}

// Vehicle variables do not actually represent the index of the vehicle
// performing a node, but we ensure that the values of two vehicle variables
// are the same if and only if the corresponding nodes are served by the same
// vehicle.
std::vector<int> CreateVehicleVars(const RoutingModel& model,
                                   const ArcVarMap& arc_vars,
                                   CpModelProto* cp_model) {
  const int depot = GetDepotFromModel(model);
  const int size = model.Size() + model.vehicles();
  std::vector<int> vehicles(size, -1);
  for (int i = 0; i < size; ++i) {
    if (model.IsStart(i) || model.IsEnd(i)) continue;
    vehicles[i] = AddVariable(cp_model, 0, size - 1);
  }
  for (const auto arc_var : arc_vars) {
    const int tail = arc_var.first.tail;
    const int head = arc_var.first.head;
    if (tail == head || head == depot) continue;
    if (tail == depot) {
      // arc[depot][head] -> vehicles[head] == head.
      AddLinearConstraint(cp_model, head, head, {{vehicles[head], 1}},
                          {arc_var.second});
      continue;
    }
    // arc[tail][head] -> vehicles[head] == vehicles[tail].
    AddLinearConstraint(cp_model, 0, 0,
                        {{vehicles[head], 1}, {vehicles[tail], -1}},
                        {arc_var.second});
  }
  return vehicles;
}

void AddPickupDeliveryConstraints(const RoutingModel& model,
                                  const ArcVarMap& arc_vars,
                                  CpModelProto* cp_model) {
  if (model.GetPickupAndDeliveryPairs().empty()) return;
  const std::vector<int> ranks = CreateRanks(model, arc_vars, cp_model);
  const std::vector<int> vehicles =
      CreateVehicleVars(model, arc_vars, cp_model);
  for (const auto& pairs : model.GetPickupAndDeliveryPairs()) {
    const int64_t pickup = pairs.first[0];
    const int64_t delivery = pairs.second[0];
    // ranks[pickup] + 1 <= ranks[delivery].
    AddLinearConstraint(cp_model, 1, std::numeric_limits<int64_t>::max(),
                        {{ranks[delivery], 1}, {ranks[pickup], -1}});
    // vehicles[pickup] == vehicles[delivery]
    AddLinearConstraint(cp_model, 0, 0,
                        {{vehicles[delivery], 1}, {vehicles[pickup], -1}});
  }
}

// Converts a RoutingModel to CpModelProto for models with multiple vehicles.
// All non-start/end nodes have the same index in both models. Start/end nodes
// map to a single depot index; its value is arbitrarly the index of the start
// node of the first vehicle in the RoutingModel.
// The map between CPModelProto arcs and their corresponding arc variable is
// returned.
ArcVarMap PopulateMultiRouteModelFromRoutingModel(const RoutingModel& model,
                                                  CpModelProto* cp_model) {
  ArcVarMap arc_vars;
  const int num_nodes = model.Nexts().size();
  const int depot = GetDepotFromModel(model);

  // Create "arc" variables and set their cost.
  for (int tail = 0; tail < num_nodes; ++tail) {
    const int tail_index = model.IsStart(tail) ? depot : tail;
    std::unique_ptr<IntVarIterator> iter(
        model.NextVar(tail)->MakeDomainIterator(false));
    for (int head : InitAndGetValues(iter.get())) {
      // Vehicle start and end nodes are represented as a single node in the
      // CP-SAT model. We choose the start index of the first vehicle to
      // represent both. We can also skip any head representing a vehicle start
      // as the CP solver will reject those.
      if (model.IsStart(head)) continue;
      const int head_index = model.IsEnd(head) ? depot : head;
      if (head_index == tail_index && head_index == depot) continue;
      const int64_t cost = tail != head ? model.GetHomogeneousCost(tail, head)
                                        : model.UnperformedPenalty(tail);
      if (cost == std::numeric_limits<int64_t>::max()) continue;
      const Arc arc = {tail_index, head_index};
      if (gtl::ContainsKey(arc_vars, arc)) continue;
      const int index = AddVariable(cp_model, 0, 1);
      gtl::InsertOrDie(&arc_vars, arc, index);
      cp_model->mutable_objective()->add_vars(index);
      cp_model->mutable_objective()->add_coeffs(cost);
    }
  }

  // Limit the number of routes to the maximum number of vehicles.
  {
    std::vector<std::pair<int, double>> variable_coeffs;
    for (int node = 0; node < num_nodes; ++node) {
      if (model.IsStart(node) || model.IsEnd(node)) continue;
      int* const var = gtl::FindOrNull(arc_vars, {depot, node});
      if (var == nullptr) continue;
      variable_coeffs.push_back({*var, 1});
    }
    AddLinearConstraint(
        cp_model, 0,
        std::min(model.vehicles(), model.GetMaximumNumberOfActiveVehicles()),
        variable_coeffs);
  }

  AddPickupDeliveryConstraints(model, arc_vars, cp_model);

  AddDimensions(model, arc_vars, cp_model);

  // Create Routes constraint, ensuring circuits from and to the depot.
  // This one is a bit tricky, because we need to remap the depot to zero.
  // TODO(user): Make Routes constraints support optional nodes.
  RoutesConstraintProto* routes_ct =
      cp_model->add_constraints()->mutable_routes();
  for (const auto arc_var : arc_vars) {
    const int tail = arc_var.first.tail;
    const int head = arc_var.first.head;
    routes_ct->add_tails(tail == 0 ? depot : tail == depot ? 0 : tail);
    routes_ct->add_heads(head == 0 ? depot : head == depot ? 0 : head);
    routes_ct->add_literals(arc_var.second);
  }

  // Add demands and capacities to improve the LP relaxation and cuts. These are
  // based on the first "unary" dimension in the model if it exists.
  // TODO(user): We might want to try to get demand lower bounds from
  // non-unary dimensions if no unary exist.
  const RoutingDimension* master_dimension = nullptr;
  for (const RoutingDimension* dimension : model.GetDimensions()) {
    // Only a single vehicle class is supported.
    if (dimension->GetUnaryTransitEvaluator(0) != nullptr) {
      master_dimension = dimension;
      break;
    }
  }
  if (master_dimension != nullptr) {
    const RoutingModel::TransitCallback1& transit =
        master_dimension->GetUnaryTransitEvaluator(0);
    for (int node = 0; node < num_nodes; ++node) {
      // Tricky: demand is added for all nodes in the sat model; this means
      // start/end nodes other than the one used for the depot must be ignored.
      if (!model.IsEnd(node) && (!model.IsStart(node) || node == depot)) {
        routes_ct->add_demands(transit(node));
      }
    }
    DCHECK_EQ(routes_ct->demands_size(), num_nodes + 1 - model.vehicles());
    routes_ct->set_capacity(master_dimension->vehicle_capacities()[0]);
  }
  return arc_vars;
}

// Converts a RoutingModel with a single vehicle to a CpModelProto.
// The mapping between CPModelProto arcs and their corresponding arc variables
// is returned.
ArcVarMap PopulateSingleRouteModelFromRoutingModel(const RoutingModel& model,
                                                   CpModelProto* cp_model) {
  ArcVarMap arc_vars;
  const int num_nodes = model.Nexts().size();
  CircuitConstraintProto* circuit =
      cp_model->add_constraints()->mutable_circuit();
  for (int tail = 0; tail < num_nodes; ++tail) {
    std::unique_ptr<IntVarIterator> iter(
        model.NextVar(tail)->MakeDomainIterator(false));
    for (int head : InitAndGetValues(iter.get())) {
      // Vehicle start and end nodes are represented as a single node in the
      // CP-SAT model. We choose the start index to represent both. We can also
      // skip any head representing a vehicle start as the CP solver will reject
      // those.
      if (model.IsStart(head)) continue;
      if (model.IsEnd(head)) head = model.Start(0);
      const int64_t cost = tail != head ? model.GetHomogeneousCost(tail, head)
                                        : model.UnperformedPenalty(tail);
      if (cost == std::numeric_limits<int64_t>::max()) continue;
      const int index = AddVariable(cp_model, 0, 1);
      circuit->add_literals(index);
      circuit->add_tails(tail);
      circuit->add_heads(head);
      cp_model->mutable_objective()->add_vars(index);
      cp_model->mutable_objective()->add_coeffs(cost);
      gtl::InsertOrDie(&arc_vars, {tail, head}, index);
    }
  }
  AddPickupDeliveryConstraints(model, arc_vars, cp_model);
  AddDimensions(model, arc_vars, cp_model);
  return arc_vars;
}

// Converts a RoutingModel to a CpModelProto.
// The mapping between CPModelProto arcs and their corresponding arc variables
// is returned.
ArcVarMap PopulateModelFromRoutingModel(const RoutingModel& model,
                                        CpModelProto* cp_model) {
  if (model.vehicles() == 1) {
    return PopulateSingleRouteModelFromRoutingModel(model, cp_model);
  }
  return PopulateMultiRouteModelFromRoutingModel(model, cp_model);
}

// Converts a CpSolverResponse to an Assignment containing next variables.
bool ConvertToSolution(const CpSolverResponse& response,
                       const RoutingModel& model, const ArcVarMap& arc_vars,
                       Assignment* solution) {
  if (response.status() != CpSolverStatus::OPTIMAL &&
      response.status() != CpSolverStatus::FEASIBLE)
    return false;
  const int depot = GetDepotFromModel(model);
  int vehicle = 0;
  for (const auto& arc_var : arc_vars) {
    if (response.solution(arc_var.second) != 0) {
      const int tail = arc_var.first.tail;
      const int head = arc_var.first.head;
      if (head == depot) continue;
      if (tail != depot) {
        solution->Add(model.NextVar(tail))->SetValue(head);
      } else {
        solution->Add(model.NextVar(model.Start(vehicle)))->SetValue(head);
        ++vehicle;
      }
    }
  }
  // Close open routes.
  for (int v = 0; v < model.vehicles(); ++v) {
    int current = model.Start(v);
    while (solution->Contains(model.NextVar(current))) {
      current = solution->Value(model.NextVar(current));
    }
    solution->Add(model.NextVar(current))->SetValue(model.End(v));
  }
  return true;
}

// Adds dimensions to a CpModelProto for heterogeneous fleet. Adds path
// cumul constraints and cumul bounds.
void AddGeneralizedDimensions(
    const RoutingModel& model, const ArcVarMap& arc_vars,
    const std::vector<absl::flat_hash_map<int, int>>& vehicle_performs_node,
    const std::vector<absl::flat_hash_map<int, int>>&
        vehicle_class_performs_arc,
    CpModelProto* cp_model) {
  const int num_cp_nodes = model.Nexts().size() + model.vehicles() + 1;
  for (const RoutingDimension* dimension : model.GetDimensions()) {
    // Initialize cumuls.
    std::vector<int> cumuls(num_cp_nodes, -1);
    for (int cp_node = 1; cp_node < num_cp_nodes; ++cp_node) {
      const int node = cp_node - 1;
      int64_t cumul_min = dimension->cumuls()[node]->Min();
      int64_t cumul_max = dimension->cumuls()[node]->Max();
      if (model.IsStart(node) || model.IsEnd(node)) {
        const int vehicle = model.VehicleIndex(node);
        cumul_max =
            std::min(cumul_max, dimension->vehicle_capacities()[vehicle]);
      }
      cumuls[cp_node] = AddVariable(cp_model, cumul_min, cumul_max);
    }

    // Constrain cumuls with vehicle capacities.
    for (int vehicle = 0; vehicle < model.vehicles(); vehicle++) {
      for (int cp_node = 1; cp_node < num_cp_nodes; cp_node++) {
        if (!vehicle_performs_node[vehicle].contains(cp_node)) continue;
        const int64_t vehicle_capacity =
            dimension->vehicle_capacities()[vehicle];
        AddLinearConstraint(cp_model, std::numeric_limits<int64_t>::min(),
                            vehicle_capacity, {{cumuls[cp_node], 1}},
                            {vehicle_performs_node[vehicle].at(cp_node)});
      }
    }

    for (auto vehicle_class = RoutingVehicleClassIndex(0);
         vehicle_class < model.GetVehicleClassesCount(); vehicle_class++) {
      std::vector<int> slack(num_cp_nodes, -1);
      const int64_t span_cost =
          dimension->GetSpanCostCoefficientForVehicleClass(vehicle_class);
      for (const auto [arc, arc_var] : arc_vars) {
        const auto [cp_tail, cp_head] = arc;
        if (cp_tail == cp_head || cp_tail == 0 || cp_head == 0) continue;
        if (!vehicle_class_performs_arc[vehicle_class.value()].contains(
                arc_var)) {
          continue;
        }
        // Create slack variable and add span cost to the objective.
        if (slack[cp_tail] == -1) {
          const int64_t slack_max =
              cp_tail - 1 < dimension->slacks().size()
                  ? dimension->slacks()[cp_tail - 1]->Max()
                  : 0;
          slack[cp_tail] = AddVariable(cp_model, 0, slack_max);
          if (slack_max > 0 && span_cost > 0) {
            cp_model->mutable_objective()->add_vars(slack[cp_tail]);
            cp_model->mutable_objective()->add_coeffs(span_cost);
          }
        }
        const int64_t transit = dimension->class_transit_evaluator(
            vehicle_class)(cp_tail - 1, cp_head - 1);
        // vehicle_class_performs_arc[vehicle][arc_var] = 1 ->
        // cumuls[cp_head] - cumuls[cp_tail] - slack[cp_tail] = transit
        AddLinearConstraint(
            cp_model, transit, transit,
            {{cumuls[cp_head], 1}, {cumuls[cp_tail], -1}, {slack[cp_tail], -1}},
            {vehicle_class_performs_arc[vehicle_class.value()].at(arc_var)});
      }
    }

    // Constrain cumuls with span limits.
    for (int vehicle = 0; vehicle < model.vehicles(); vehicle++) {
      const int64_t span_limit =
          dimension->vehicle_span_upper_bounds()[vehicle];
      if (span_limit == std::numeric_limits<int64_t>::max()) continue;
      int cp_start = model.Start(vehicle) + 1;
      int cp_end = model.End(vehicle) + 1;
      AddLinearConstraint(cp_model, std::numeric_limits<int64_t>::min(),
                          span_limit,
                          {{cumuls[cp_end], 1}, {cumuls[cp_start], -1}});
    }

    // Set soft span upper bound costs.
    if (dimension->HasSoftSpanUpperBounds()) {
      for (int vehicle = 0; vehicle < model.vehicles(); vehicle++) {
        const auto [bound, cost] =
            dimension->GetSoftSpanUpperBoundForVehicle(vehicle);
        const int cp_start = model.Start(vehicle) + 1;
        const int cp_end = model.End(vehicle) + 1;
        const int extra =
            AddVariable(cp_model, 0,
                        std::min(dimension->cumuls()[model.End(vehicle)]->Max(),
                                 dimension->vehicle_capacities()[vehicle]));
        // -inf <= cumuls[cp_end] - cumuls[cp_start] - extra <= bound
        AddLinearConstraint(
            cp_model, std::numeric_limits<int64_t>::min(), bound,
            {{cumuls[cp_end], 1}, {cumuls[cp_start], -1}, {extra, -1}});
        // Add extra * cost to objective.
        cp_model->mutable_objective()->add_vars(extra);
        cp_model->mutable_objective()->add_coeffs(cost);
      }
    }
  }
}

std::vector<int> CreateGeneralizedRanks(const RoutingModel& model,
                                        const ArcVarMap& arc_vars,
                                        const std::vector<int>& is_unperformed,
                                        CpModelProto* cp_model) {
  const int depot = 0;
  const int num_cp_nodes = model.Nexts().size() + model.vehicles() + 1;
  // Maximum length of a single route (excluding the depot & vehicle end nodes).
  const int max_rank = num_cp_nodes - 2 * model.vehicles();
  std::vector<int> ranks(num_cp_nodes, -1);
  ranks[depot] = AddVariable(cp_model, 0, 0);
  for (int cp_node = 1; cp_node < num_cp_nodes; cp_node++) {
    if (model.IsEnd(cp_node - 1)) continue;
    ranks[cp_node] = AddVariable(cp_model, 0, max_rank);
    // For unperformed nodes rank is 0.
    AddLinearConstraint(cp_model, 0, 0, {{ranks[cp_node], 1}},
                        {is_unperformed[cp_node]});
  }
  for (const auto [arc, arc_var] : arc_vars) {
    const auto [cp_tail, cp_head] = arc;
    if (model.IsEnd(cp_head - 1)) continue;
    if (cp_tail == cp_head || cp_head == depot) continue;
    // arc[tail][head] -> ranks[head] == ranks[tail] + 1.
    AddLinearConstraint(cp_model, 1, 1,
                        {{ranks[cp_head], 1}, {ranks[cp_tail], -1}}, {arc_var});
  }
  return ranks;
}

void AddGeneralizedPickupDeliveryConstraints(
    const RoutingModel& model, const ArcVarMap& arc_vars,
    const std::vector<absl::flat_hash_map<int, int>>& vehicle_performs_node,
    const std::vector<int>& is_unperformed, CpModelProto* cp_model) {
  if (model.GetPickupAndDeliveryPairs().empty()) return;
  const std::vector<int> ranks =
      CreateGeneralizedRanks(model, arc_vars, is_unperformed, cp_model);
  for (const auto& pairs : model.GetPickupAndDeliveryPairs()) {
    for (const int delivery : pairs.second) {
      const int cp_delivery = delivery + 1;
      for (int vehicle = 0; vehicle < model.vehicles(); vehicle++) {
        const Arc vehicle_start_delivery_arc = {
            static_cast<int>(model.Start(vehicle) + 1), cp_delivery};
        if (gtl::ContainsKey(arc_vars, vehicle_start_delivery_arc)) {
          // Forbid vehicle_start -> delivery arc.
          AddLinearConstraint(cp_model, 0, 0,
                              {{arc_vars.at(vehicle_start_delivery_arc), 1}});
        }
      }

      for (const int pickup : pairs.first) {
        const int cp_pickup = pickup + 1;
        const Arc delivery_pickup_arc = {cp_delivery, cp_pickup};
        if (gtl::ContainsKey(arc_vars, delivery_pickup_arc)) {
          // Forbid delivery -> pickup arc.
          AddLinearConstraint(cp_model, 0, 0,
                              {{arc_vars.at(delivery_pickup_arc), 1}});
        }

        DCHECK_GE(is_unperformed[cp_delivery], 0);
        DCHECK_GE(is_unperformed[cp_pickup], 0);
        // A negative index i refers to NOT the literal at index -i - 1.
        // -i - 1 ~ NOT i, if value of i in [0, 1] (boolean).
        const int delivery_performed = -is_unperformed[cp_delivery] - 1;
        const int pickup_performed = -is_unperformed[cp_pickup] - 1;
        // The same vehicle performs pickup and delivery.
        for (int vehicle = 0; vehicle < model.vehicles(); vehicle++) {
          // delivery_performed & pickup_performed ->
          // vehicle_performs_node[vehicle][cp_delivery] -
          // vehicle_performs_node[vehicle][cp_pickup] = 0
          AddLinearConstraint(
              cp_model, 0, 0,
              {{vehicle_performs_node[vehicle].at(cp_delivery), 1},
               {vehicle_performs_node[vehicle].at(cp_pickup), -1}},
              {delivery_performed, pickup_performed});
        }
      }
    }

    std::vector<std::pair<int, double>> ranks_difference;
    // -SUM(pickup)ranks[pickup].
    for (const int pickup : pairs.first) {
      const int cp_pickup = pickup + 1;
      ranks_difference.push_back({ranks[cp_pickup], -1});
    }
    // SUM(delivery)ranks[delivery].
    for (const int delivery : pairs.second) {
      const int cp_delivery = delivery + 1;
      ranks_difference.push_back({ranks[cp_delivery], 1});
    }
    // SUM(delivery)ranks[delivery] - SUM(pickup)ranks[pickup] >= 1
    AddLinearConstraint(cp_model, 1, std::numeric_limits<int64_t>::max(),
                        ranks_difference);
  }
}

// Converts a RoutingModel to CpModelProto for models with multiple
// vehicles. The node 0 is depot. All nodes in CpModel have index increased
// by 1 in comparison to the RoutingModel. Each start node has only 1
// incoming arc (from depot), each end node has only 1 outgoing arc (to
// depot). The mapping from CPModelProto arcs to their corresponding arc
// variable is returned.
ArcVarMap PopulateGeneralizedRouteModelFromRoutingModel(
    const RoutingModel& model, CpModelProto* cp_model) {
  ArcVarMap arc_vars;
  const int depot = 0;
  const int num_nodes = model.Nexts().size();
  const int num_cp_nodes = num_nodes + model.vehicles() + 1;
  // vehicle_performs_node[vehicle][node] equals to 1 if the vehicle performs
  // the node, and 0 otherwise.
  std::vector<absl::flat_hash_map<int, int>> vehicle_performs_node(
      model.vehicles());
  // Connect vehicles start and end nodes to depot.
  for (int vehicle = 0; vehicle < model.vehicles(); vehicle++) {
    const int cp_start = model.Start(vehicle) + 1;
    const Arc start_arc = {depot, cp_start};
    const int start_arc_var = AddVariable(cp_model, 1, 1);
    DCHECK(!gtl::ContainsKey(arc_vars, start_arc));
    arc_vars.insert({start_arc, start_arc_var});

    const int cp_end = model.End(vehicle) + 1;
    const Arc end_arc = {cp_end, depot};
    const int end_arc_var = AddVariable(cp_model, 1, 1);
    DCHECK(!gtl::ContainsKey(arc_vars, end_arc));
    arc_vars.insert({end_arc, end_arc_var});

    vehicle_performs_node[vehicle][cp_start] = start_arc_var;
    vehicle_performs_node[vehicle][cp_end] = end_arc_var;
  }

  // is_unperformed[node] variable equals to 1 if visit is unperformed, and 0
  // otherwise.
  std::vector<int> is_unperformed(num_cp_nodes, -1);
  // Initialize is_unperformed variables for nodes that must be performed.
  for (int node = 0; node < num_nodes; node++) {
    const int cp_node = node + 1;
    // Forced active and nodes that are not involved in any disjunctions are
    // always performed.
    const std::vector<RoutingDisjunctionIndex>& disjunction_indices =
        model.GetDisjunctionIndices(node);
    if (disjunction_indices.empty() || model.ActiveVar(node)->Min() == 1) {
      is_unperformed[cp_node] = AddVariable(cp_model, 0, 0);
      continue;
    }
    // Check if the node is in a forced active disjunction.
    for (RoutingDisjunctionIndex disjunction_index : disjunction_indices) {
      const int num_nodes =
          model.GetDisjunctionNodeIndices(disjunction_index).size();
      const int64_t penalty = model.GetDisjunctionPenalty(disjunction_index);
      const int64_t max_cardinality =
          model.GetDisjunctionMaxCardinality(disjunction_index);
      if (num_nodes == max_cardinality &&
          (penalty < 0 || penalty == std::numeric_limits<int64_t>::max())) {
        // Nodes in this disjunction are forced active.
        is_unperformed[cp_node] = AddVariable(cp_model, 0, 0);
        break;
      }
    }
  }
  // Add alternative visits. Create self-looped arc variables. Set penalty for
  // not performing disjunctions.
  for (RoutingDisjunctionIndex disjunction_index(0);
       disjunction_index < model.GetNumberOfDisjunctions();
       disjunction_index++) {
    const std::vector<int64_t>& disjunction_indices =
        model.GetDisjunctionNodeIndices(disjunction_index);
    const int disjunction_size = disjunction_indices.size();
    const int64_t penalty = model.GetDisjunctionPenalty(disjunction_index);
    const int64_t max_cardinality =
        model.GetDisjunctionMaxCardinality(disjunction_index);
    // Case when disjunction involves only 1 node, the node is only present in
    // this disjunction, and the node can be unperformed.
    if (disjunction_size == 1 &&
        model.GetDisjunctionIndices(disjunction_indices[0]).size() == 1 &&
        is_unperformed[disjunction_indices[0] + 1] == -1) {
      const int cp_node = disjunction_indices[0] + 1;
      const Arc arc = {cp_node, cp_node};
      DCHECK(!gtl::ContainsKey(arc_vars, arc));
      is_unperformed[cp_node] = AddVariable(cp_model, 0, 1);
      arc_vars.insert({arc, is_unperformed[cp_node]});
      cp_model->mutable_objective()->add_vars(is_unperformed[cp_node]);
      cp_model->mutable_objective()->add_coeffs(penalty);
      continue;
    }
    // num_performed + SUM(node)is_unperformed[node] = disjunction_size
    const int num_performed = AddVariable(cp_model, 0, max_cardinality);
    std::vector<std::pair<int, double>> var_coeffs;
    var_coeffs.push_back({num_performed, 1});
    for (const int node : disjunction_indices) {
      const int cp_node = node + 1;
      // Node can be unperformed.
      if (is_unperformed[cp_node] == -1) {
        const Arc arc = {cp_node, cp_node};
        DCHECK(!gtl::ContainsKey(arc_vars, arc));
        is_unperformed[cp_node] = AddVariable(cp_model, 0, 1);
        arc_vars.insert({arc, is_unperformed[cp_node]});
      }
      var_coeffs.push_back({is_unperformed[cp_node], 1});
    }
    AddLinearConstraint(cp_model, disjunction_size, disjunction_size,
                        var_coeffs);
    // When penalty is negative or max int64_t (forced active), num_violated is
    // 0.
    if (penalty < 0 || penalty == std::numeric_limits<int64_t>::max()) {
      AddLinearConstraint(cp_model, max_cardinality, max_cardinality,
                          {{num_performed, 1}});
      continue;
    }
    // If number of active indices is less than max_cardinality, then for each
    // violated index 'penalty' is paid.
    const int num_violated = AddVariable(cp_model, 0, max_cardinality);
    cp_model->mutable_objective()->add_vars(num_violated);
    cp_model->mutable_objective()->add_coeffs(penalty);
    // num_performed + num_violated = max_cardinality
    AddLinearConstraint(cp_model, max_cardinality, max_cardinality,
                        {{num_performed, 1}, {num_violated, 1}});
  }
  // Create "arc" variables.
  for (int tail = 0; tail < num_nodes; ++tail) {
    const int cp_tail = tail + 1;
    std::unique_ptr<IntVarIterator> iter(
        model.NextVar(tail)->MakeDomainIterator(false));
    for (int head : InitAndGetValues(iter.get())) {
      const int cp_head = head + 1;
      if (model.IsStart(head)) continue;
      // Arcs for unperformed visits have already been created.
      if (tail == head) continue;
      // Direct arcs from start to end nodes should exist only if they are
      // for the same vehicle.
      if (model.IsStart(tail) && model.IsEnd(head) &&
          model.VehicleIndex(tail) != model.VehicleIndex(head)) {
        continue;
      }

      bool feasible = false;
      for (int vehicle = 0; vehicle < model.vehicles(); vehicle++) {
        if (model.GetArcCostForVehicle(tail, head, vehicle) !=
            std::numeric_limits<int64_t>::max()) {
          feasible = true;
          break;
        }
      }
      if (!feasible) continue;

      const Arc arc = {cp_tail, cp_head};
      DCHECK(!gtl::ContainsKey(arc_vars, arc));
      const int arc_var = AddVariable(cp_model, 0, 1);
      arc_vars.insert({arc, arc_var});
    }
  }

  // Set literals for vehicle performing node.
  for (int cp_node = 1; cp_node < num_cp_nodes; cp_node++) {
    // For starts and ends nodes vehicle_performs_node variables already set.
    if (model.IsStart(cp_node - 1) || model.IsEnd(cp_node - 1)) continue;
    // Each node should be performed by 1 vehicle, or be unperformed.
    // SUM(vehicle)(vehicle_performs_node[vehicle][cp_node]) + loop(cp_node) = 1
    std::vector<std::pair<int, double>> var_coeffs;
    for (int vehicle = 0; vehicle < model.vehicles(); vehicle++) {
      vehicle_performs_node[vehicle][cp_node] = AddVariable(cp_model, 0, 1);
      var_coeffs.push_back({vehicle_performs_node[vehicle][cp_node], 1});
    }
    var_coeffs.push_back({is_unperformed[cp_node], 1});
    AddLinearConstraint(cp_model, 1, 1, var_coeffs);
  }
  const int num_vehicle_classes = model.GetVehicleClassesCount();
  // vehicle_class_performs_node[vehicle_class][node] equals to 1 if the
  // vehicle of vehicle_class performs the node, and 0 otherwise.
  std::vector<absl::flat_hash_map<int, int>> vehicle_class_performs_node(
      num_vehicle_classes);
  for (int cp_node = 1; cp_node < num_cp_nodes; cp_node++) {
    const int node = cp_node - 1;
    for (int vehicle_class = 0; vehicle_class < num_vehicle_classes;
         vehicle_class++) {
      if (model.IsStart(node) || model.IsEnd(node)) {
        const int vehicle = model.VehicleIndex(node);
        vehicle_class_performs_node[vehicle_class][cp_node] =
            vehicle_class ==
                    model.GetVehicleClassIndexOfVehicle(vehicle).value()
                ? AddVariable(cp_model, 1, 1)
                : AddVariable(cp_model, 0, 0);
        continue;
      }
      vehicle_class_performs_node[vehicle_class][cp_node] =
          AddVariable(cp_model, 0, 1);
      std::vector<std::pair<int, double>> var_coeffs;
      for (int vehicle = 0; vehicle < model.vehicles(); vehicle++) {
        if (model.GetVehicleClassIndexOfVehicle(vehicle).value() ==
            vehicle_class) {
          var_coeffs.push_back({vehicle_performs_node[vehicle][cp_node], 1});
          // vehicle_performs_node -> vehicle_class_performs_node
          AddLinearConstraint(
              cp_model, 1, 1,
              {{vehicle_class_performs_node[vehicle_class][cp_node], 1}},
              {vehicle_performs_node[vehicle][cp_node]});
        }
      }
      // vehicle_class_performs_node -> exactly one vehicle from this class
      // performs node.
      AddLinearConstraint(
          cp_model, 1, 1, var_coeffs,
          {vehicle_class_performs_node[vehicle_class][cp_node]});
    }
  }
  // vehicle_class_performs_arc[vehicle_class][arc_var] equals to 1 if the
  // vehicle of vehicle_class performs the arc, and 0 otherwise.
  std::vector<absl::flat_hash_map<int, int>> vehicle_class_performs_arc(
      num_vehicle_classes);
  // Set "arc" costs.
  for (const auto [arc, arc_var] : arc_vars) {
    const auto [cp_tail, cp_head] = arc;
    if (cp_tail == depot || cp_head == depot) continue;
    const int tail = cp_tail - 1;
    const int head = cp_head - 1;
    // Costs for unperformed arcs have already been set.
    if (tail == head) continue;
    for (int vehicle = 0; vehicle < model.vehicles(); vehicle++) {
      // The arc can't be performed by the vehicle when vehicle can't perform
      // arc nodes.
      if (!vehicle_performs_node[vehicle].contains(cp_tail) ||
          !vehicle_performs_node[vehicle].contains(cp_head)) {
        continue;
      }
      int64_t cost = model.GetArcCostForVehicle(tail, head, vehicle);
      // Arcs with int64_t's max cost are infeasible.
      if (cost == std::numeric_limits<int64_t>::max()) continue;
      const int vehicle_class =
          model.GetVehicleClassIndexOfVehicle(vehicle).value();
      if (!vehicle_class_performs_arc[vehicle_class].contains(arc_var)) {
        vehicle_class_performs_arc[vehicle_class][arc_var] =
            AddVariable(cp_model, 0, 1);
        // Create constraints to set vehicle_class_performs_arc.
        // vehicle_class_performs_arc ->
        // vehicle_class_performs_tail & vehicle_class_performs_head &
        // arc_is_performed
        ConstraintProto* ct = cp_model->add_constraints();
        ct->add_enforcement_literal(
            vehicle_class_performs_arc[vehicle_class][arc_var]);
        BoolArgumentProto* bool_and = ct->mutable_bool_and();
        bool_and->add_literals(
            vehicle_class_performs_node[vehicle_class][cp_tail]);
        bool_and->add_literals(
            vehicle_class_performs_node[vehicle_class][cp_head]);
        bool_and->add_literals(arc_var);
        // Don't add arcs with zero cost to the objective.
        if (cost != 0) {
          cp_model->mutable_objective()->add_vars(
              vehicle_class_performs_arc[vehicle_class][arc_var]);
          cp_model->mutable_objective()->add_coeffs(cost);
        }
      }
      // (arc_is_performed & vehicle_performs_tail) ->
      // (vehicle_class_performs_arc & vehicle_performs_head)
      ConstraintProto* ct_arc_tail = cp_model->add_constraints();
      ct_arc_tail->add_enforcement_literal(arc_var);
      ct_arc_tail->add_enforcement_literal(
          vehicle_performs_node[vehicle][cp_tail]);
      ct_arc_tail->mutable_bool_and()->add_literals(
          vehicle_class_performs_arc[vehicle_class][arc_var]);
      ct_arc_tail->mutable_bool_and()->add_literals(
          vehicle_performs_node[vehicle][cp_head]);
      // (arc_is_performed & vehicle_performs_head) ->
      // (vehicle_class_performs_arc & vehicle_performs_tail)
      ConstraintProto* ct_arc_head = cp_model->add_constraints();
      ct_arc_head->add_enforcement_literal(arc_var);
      ct_arc_head->add_enforcement_literal(
          vehicle_performs_node[vehicle][cp_head]);
      ct_arc_head->mutable_bool_and()->add_literals(
          vehicle_class_performs_arc[vehicle_class][arc_var]);
      ct_arc_head->mutable_bool_and()->add_literals(
          vehicle_performs_node[vehicle][cp_tail]);
    }
  }

  AddGeneralizedPickupDeliveryConstraints(
      model, arc_vars, vehicle_performs_node, is_unperformed, cp_model);

  AddGeneralizedDimensions(model, arc_vars, vehicle_performs_node,
                           vehicle_class_performs_arc, cp_model);

  // Create Routes constraint, ensuring circuits from and to the depot.
  RoutesConstraintProto* routes_ct =
      cp_model->add_constraints()->mutable_routes();
  for (const auto [arc, arc_var] : arc_vars) {
    const int tail = arc.tail;
    const int head = arc.head;
    routes_ct->add_tails(tail);
    routes_ct->add_heads(head);
    routes_ct->add_literals(arc_var);
  }

  //  Add demands and capacities to improve the LP relaxation and cuts. These
  //  are based on the first "unary" dimension in the model if it exists.
  //  TODO(user): We might want to try to get demand lower bounds from
  //  non-unary dimensions if no unary exist.
  const RoutingDimension* master_dimension = nullptr;
  for (const RoutingDimension* dimension : model.GetDimensions()) {
    bool is_unary = true;
    for (int vehicle = 0; vehicle < model.vehicles(); vehicle++) {
      if (dimension->GetUnaryTransitEvaluator(vehicle) == nullptr) {
        is_unary = false;
        break;
      }
    }
    if (is_unary) {
      master_dimension = dimension;
      break;
    }
  }
  if (master_dimension != nullptr) {
    for (int cp_node = 0; cp_node < num_cp_nodes; ++cp_node) {
      int64_t min_transit = std::numeric_limits<int64_t>::max();
      if (cp_node != 0 && !model.IsEnd(cp_node - 1)) {
        for (int vehicle = 0; vehicle < model.vehicles(); vehicle++) {
          const RoutingModel::TransitCallback1& transit =
              master_dimension->GetUnaryTransitEvaluator(vehicle);
          min_transit = std::min(min_transit, transit(cp_node - 1));
        }
      } else {
        min_transit = 0;
      }
      routes_ct->add_demands(min_transit);
    }
    DCHECK_EQ(routes_ct->demands_size(), num_cp_nodes);
    int64_t max_capacity = std::numeric_limits<int64_t>::min();
    for (int vehicle = 0; vehicle < model.vehicles(); vehicle++) {
      max_capacity = std::max(max_capacity,
                              master_dimension->vehicle_capacities()[vehicle]);
    }
    routes_ct->set_capacity(max_capacity);
  }
  return arc_vars;
}

// Converts a CpSolverResponse to an Assignment containing next variables.
bool ConvertGeneralizedResponseToSolution(const CpSolverResponse& response,
                                          const RoutingModel& model,
                                          const ArcVarMap& arc_vars,
                                          Assignment* solution) {
  if (response.status() != CpSolverStatus::OPTIMAL &&
      response.status() != CpSolverStatus::FEASIBLE) {
    return false;
  }
  const int depot = 0;
  for (const auto [arc, arc_var] : arc_vars) {
    if (response.solution(arc_var) == 0) continue;
    const auto [tail, head] = arc;
    if (head == depot || tail == depot) continue;
    solution->Add(model.NextVar(tail - 1))->SetValue(head - 1);
  }
  return true;
}

// Uses CP solution as hint for CP-SAT.
void AddSolutionAsHintToGeneralizedModel(const Assignment* solution,
                                         const RoutingModel& model,
                                         const ArcVarMap& arc_vars,
                                         CpModelProto* cp_model) {
  if (solution == nullptr) return;
  PartialVariableAssignment* const hint = cp_model->mutable_solution_hint();
  hint->Clear();
  const int num_nodes = model.Nexts().size();
  for (int tail = 0; tail < num_nodes; ++tail) {
    const int cp_tail = tail + 1;
    const int cp_head = solution->Value(model.NextVar(tail)) + 1;
    const int* const arc_var = gtl::FindOrNull(arc_vars, {cp_tail, cp_head});
    // Arcs with a cost of max int64_t are not added to the model (considered as
    // infeasible). In some rare cases CP solutions might contain such arcs in
    // which case they are skipped here and a partial solution is used as a
    // hint.
    if (arc_var == nullptr) continue;
    hint->add_vars(*arc_var);
    hint->add_values(1);
  }
}

void AddSolutionAsHintToModel(const Assignment* solution,
                              const RoutingModel& model,
                              const ArcVarMap& arc_vars,
                              CpModelProto* cp_model) {
  if (solution == nullptr) return;
  PartialVariableAssignment* const hint = cp_model->mutable_solution_hint();
  hint->Clear();
  const int depot = GetDepotFromModel(model);
  const int num_nodes = model.Nexts().size();
  for (int tail = 0; tail < num_nodes; ++tail) {
    const int tail_index = model.IsStart(tail) ? depot : tail;
    const int head = solution->Value(model.NextVar(tail));
    const int head_index = model.IsEnd(head) ? depot : head;
    if (tail_index == depot && head_index == depot) continue;
    const int* const var_index =
        gtl::FindOrNull(arc_vars, {tail_index, head_index});
    // Arcs with a cost of kint64max are not added to the model (considered as
    // infeasible). In some rare cases CP solutions might contain such arcs in
    // which case they are skipped here and a partial solution is used as a
    // hint.
    if (var_index == nullptr) continue;
    hint->add_vars(*var_index);
    hint->add_values(1);
  }
}

// Configures a CP-SAT solver and solves the given (routing) model using it.
// Returns the response of the search.
CpSolverResponse SolveRoutingModel(
    const CpModelProto& cp_model, absl::Duration remaining_time,
    const RoutingSearchParameters& search_parameters,
    const std::function<void(const CpSolverResponse& response)>& observer) {
  // Copying to set remaining time.
  SatParameters sat_parameters = search_parameters.sat_parameters();
  if (!sat_parameters.has_max_time_in_seconds()) {
    sat_parameters.set_max_time_in_seconds(
        absl::ToDoubleSeconds(remaining_time));
  } else {
    sat_parameters.set_max_time_in_seconds(
        std::min(absl::ToDoubleSeconds(remaining_time),
                 sat_parameters.max_time_in_seconds()));
  }
  Model model;
  model.Add(NewSatParameters(sat_parameters));
  if (observer != nullptr) {
    model.Add(NewFeasibleSolutionObserver(observer));
  }
  // TODO(user): Add an option to dump the CP-SAT model or check if the
  // cp_model_dump_file flag in cp_model_solver.cc is good enough.
  return SolveCpModel(cp_model, &model);
}

// Check if all the nodes are present in arcs. Otherwise, CP-SAT solver may
// fail.
bool IsFeasibleArcVarMap(const ArcVarMap& arc_vars, int max_node_index) {
  Bitset64<> present_in_arcs(max_node_index + 1);
  for (const auto [arc, _] : arc_vars) {
    present_in_arcs.Set(arc.head);
    present_in_arcs.Set(arc.tail);
  }
  for (int i = 0; i <= max_node_index; i++) {
    if (!present_in_arcs[i]) return false;
  }
  return true;
}

}  // namespace
}  // namespace sat

// Solves a RoutingModel using the CP-SAT solver. Returns false if no solution
// was found.
bool SolveModelWithSat(const RoutingModel& model,
                       const RoutingSearchParameters& search_parameters,
                       const Assignment* initial_solution,
                       Assignment* solution) {
  sat::CpModelProto cp_model;
  cp_model.mutable_objective()->set_scaling_factor(
      search_parameters.log_cost_scaling_factor());
  cp_model.mutable_objective()->set_offset(search_parameters.log_cost_offset());
  if (search_parameters.use_generalized_cp_sat() == BOOL_TRUE) {
    const sat::ArcVarMap arc_vars =
        sat::PopulateGeneralizedRouteModelFromRoutingModel(model, &cp_model);
    const int max_node_index = model.Nexts().size() + model.vehicles();
    if (!sat::IsFeasibleArcVarMap(arc_vars, max_node_index)) return false;
    sat::AddSolutionAsHintToGeneralizedModel(initial_solution, model, arc_vars,
                                             &cp_model);
    return sat::ConvertGeneralizedResponseToSolution(
        sat::SolveRoutingModel(cp_model, model.RemainingTime(),
                               search_parameters, nullptr),
        model, arc_vars, solution);
  }
  if (!sat::RoutingModelCanBeSolvedBySat(model)) return false;
  const sat::ArcVarMap arc_vars =
      sat::PopulateModelFromRoutingModel(model, &cp_model);
  sat::AddSolutionAsHintToModel(initial_solution, model, arc_vars, &cp_model);
  return sat::ConvertToSolution(
      sat::SolveRoutingModel(cp_model, model.RemainingTime(), search_parameters,
                             nullptr),
      model, arc_vars, solution);
}

}  // namespace operations_research
