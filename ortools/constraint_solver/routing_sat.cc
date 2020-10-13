// Copyright 2010-2018 Google LLC
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

#include "ortools/constraint_solver/routing.h"
#include "ortools/sat/cp_model.h"

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
int AddVariable(CpModelProto* cp_model, int64 lb, int64 ub) {
  const int index = cp_model->variables_size();
  IntegerVariableProto* const var = cp_model->add_variables();
  var->add_domain(lb);
  var->add_domain(ub);
  return index;
}

// Returns the unique depot node used in the CP-SAT models (as of 01/2020).
int64 GetDepotFromModel(const RoutingModel& model) { return model.Start(0); }

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
    const int64 min_start = dimension->cumuls()[model.Start(0)]->Min();
    const int64 max_end = std::min(dimension->cumuls()[model.End(0)]->Max(),
                                   dimension->vehicle_capacities()[0]);
    for (int i = 0; i < cumuls.size(); ++i) {
      if (model.IsStart(i) || model.IsEnd(i)) continue;
      // Reducing bounds supposing the triangular inequality.
      const int64 cumul_min =
          std::max(sat::kMinIntegerValue.value(),
                   std::max(dimension->cumuls()[i]->Min(),
                            CapAdd(transit(model.Start(0), i), min_start)));
      const int64 cumul_max =
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
      ConstraintProto* ct = cp_model->add_constraints();
      ct->add_enforcement_literal(arc_var.second);
      LinearConstraintProto* arg = ct->mutable_linear();
      arg->add_domain(transit(tail, head));
      arg->add_domain(kint64max);
      arg->add_vars(cumuls[tail]);
      arg->add_coeffs(-1);
      arg->add_vars(cumuls[head]);
      arg->add_coeffs(1);
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
    ConstraintProto* ct = cp_model->add_constraints();
    ct->add_enforcement_literal(arc_var.second);
    LinearConstraintProto* arg = ct->mutable_linear();
    arg->add_domain(1);
    arg->add_domain(1);
    arg->add_vars(ranks[tail]);
    arg->add_coeffs(-1);
    arg->add_vars(ranks[head]);
    arg->add_coeffs(1);
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
      ConstraintProto* ct = cp_model->add_constraints();
      ct->add_enforcement_literal(arc_var.second);
      LinearConstraintProto* arg = ct->mutable_linear();
      arg->add_domain(head);
      arg->add_domain(head);
      arg->add_vars(vehicles[head]);
      arg->add_coeffs(1);
      continue;
    }
    // arc[tail][head] -> vehicles[head] == vehicles[tail].
    ConstraintProto* ct = cp_model->add_constraints();
    ct->add_enforcement_literal(arc_var.second);
    LinearConstraintProto* arg = ct->mutable_linear();
    arg->add_domain(0);
    arg->add_domain(0);
    arg->add_vars(vehicles[tail]);
    arg->add_coeffs(-1);
    arg->add_vars(vehicles[head]);
    arg->add_coeffs(1);
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
    const int64 pickup = pairs.first[0];
    const int64 delivery = pairs.second[0];
    {
      // ranks[pickup] + 1 <= ranks[delivery].
      ConstraintProto* ct = cp_model->add_constraints();
      LinearConstraintProto* arg = ct->mutable_linear();
      arg->add_domain(1);
      arg->add_domain(kint64max);
      arg->add_vars(ranks[delivery]);
      arg->add_coeffs(1);
      arg->add_vars(ranks[pickup]);
      arg->add_coeffs(-1);
    }
    {
      // vehicles[pickup] == vehicles[delivery]
      ConstraintProto* ct = cp_model->add_constraints();
      LinearConstraintProto* arg = ct->mutable_linear();
      arg->add_domain(0);
      arg->add_domain(0);
      arg->add_vars(vehicles[delivery]);
      arg->add_coeffs(1);
      arg->add_vars(vehicles[pickup]);
      arg->add_coeffs(-1);
    }
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
      if (head_index == tail_index) continue;
      const int64 cost = tail != head ? model.GetHomogeneousCost(tail, head)
                                      : model.UnperformedPenalty(tail);
      if (cost == kint64max) continue;
      const Arc arc = {tail_index, head_index};
      if (gtl::ContainsKey(arc_vars, arc)) continue;
      const int index = AddVariable(cp_model, 0, 1);
      gtl::InsertOrDie(&arc_vars, arc, index);
      cp_model->mutable_objective()->add_vars(index);
      cp_model->mutable_objective()->add_coeffs(cost);
    }
  }

  // The following flow constraints seem to be necessary with the Route
  // constraint, greatly improving preformance due to stronger LP relaxation
  // (supposedly).
  // TODO(user): Remove these constraints when the Route constraint handles
  // LP relaxations properly.
  {
    LinearConstraintProto* ct = cp_model->add_constraints()->mutable_linear();
    ct->add_domain(0);
    ct->add_domain(0);
    for (int node = 0; node < num_nodes; ++node) {
      if (model.IsStart(node) || model.IsEnd(node)) continue;
      ct->add_vars(gtl::FindOrDie(arc_vars, {depot, node}));
      ct->add_coeffs(1);
      ct->add_vars(gtl::FindOrDie(arc_vars, {node, depot}));
      ct->add_coeffs(-1);
    }
  }

  {
    LinearConstraintProto* ct = cp_model->add_constraints()->mutable_linear();
    ct->add_domain(0);
    // Taking the min since as of 04/2020 fleet is homogeneous.
    ct->add_domain(
        std::min(model.vehicles(), model.GetMaximumNumberOfActiveVehicles()));
    for (int node = 0; node < num_nodes; ++node) {
      if (model.IsStart(node) || model.IsEnd(node)) continue;
      ct->add_vars(gtl::FindOrDie(arc_vars, {depot, node}));
      ct->add_coeffs(1);
    }
  }

  for (int tail = 0; tail < num_nodes; ++tail) {
    if (model.IsStart(tail) || model.IsEnd(tail)) continue;
    LinearConstraintProto* ct = cp_model->add_constraints()->mutable_linear();
    ct->add_domain(1);
    ct->add_domain(1);
    std::unique_ptr<IntVarIterator> iter(
        model.NextVar(tail)->MakeDomainIterator(false));
    bool depot_added = false;
    for (int head : InitAndGetValues(iter.get())) {
      if (model.IsStart(head)) continue;
      if (tail == head) continue;
      if (model.IsEnd(head)) {
        if (depot_added) continue;
        head = depot;
        depot_added = true;
      }
      ct->add_vars(gtl::FindOrDie(arc_vars, {tail, head}));
      ct->add_coeffs(1);
    }
  }

  for (int head = 0; head < num_nodes; ++head) {
    if (model.IsStart(head) || model.IsEnd(head)) continue;
    LinearConstraintProto* ct = cp_model->add_constraints()->mutable_linear();
    ct->add_domain(1);
    ct->add_domain(1);
    for (int tail = 0; tail < num_nodes; ++tail) {
      if (model.IsEnd(head)) continue;
      if (tail == head) continue;
      if (model.IsStart(tail) && tail != depot) continue;
      ct->add_vars(gtl::FindOrDie(arc_vars, {tail, head}));
      ct->add_coeffs(1);
    }
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
      const int64 cost = tail != head ? model.GetHomogeneousCost(tail, head)
                                      : model.UnperformedPenalty(tail);
      if (cost == kint64max) continue;
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
    const std::function<void(const CpSolverResponse& response)>& observer) {
  // TODO(user): Add CP-SAT parameters to routing parameters.
  SatParameters parameters;
  parameters.set_linearization_level(2);
  parameters.set_max_time_in_seconds(absl::ToDoubleSeconds(remaining_time));
  parameters.set_num_search_workers(1);
  Model model;
  model.Add(NewSatParameters(parameters));
  if (observer != nullptr) {
    model.Add(NewFeasibleSolutionObserver(observer));
  }
  // TODO(user): Add an option to dump the CP-SAT model or check if the
  // cp_model_dump_file flag in cp_model_solver.cc is good enough.
  return SolveCpModel(cp_model, &model);
}

}  // namespace
}  // namespace sat

// Solves a RoutingModel using the CP-SAT solver. Returns false if no solution
// was found.
bool SolveModelWithSat(const RoutingModel& model,
                       const RoutingSearchParameters& search_parameters,
                       const Assignment* initial_solution,
                       Assignment* solution) {
  if (!sat::RoutingModelCanBeSolvedBySat(model)) return false;
  sat::CpModelProto cp_model;
  cp_model.mutable_objective()->set_scaling_factor(
      search_parameters.log_cost_scaling_factor());
  cp_model.mutable_objective()->set_offset(search_parameters.log_cost_offset());
  const sat::ArcVarMap arc_vars =
      sat::PopulateModelFromRoutingModel(model, &cp_model);
  sat::AddSolutionAsHintToModel(initial_solution, model, arc_vars, &cp_model);
  return sat::ConvertToSolution(
      sat::SolveRoutingModel(cp_model, model.RemainingTime(), nullptr), model,
      arc_vars, solution);
}

}  // namespace operations_research
