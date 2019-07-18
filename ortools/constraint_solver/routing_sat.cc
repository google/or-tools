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

// For now only TSPs are supported.
// TODO(user): Support multi-route models and any type of constraints.
bool RoutingModelCanBeSolvedBySat(const RoutingModel& model) {
  return model.vehicles() == 1;
}

// Adds an integer variable to a CpModelProto, returning its index in the proto.
int AddVariable(CpModelProto* cp_model, int64 lb, int64 ub) {
  const int index = cp_model->variables_size();
  IntegerVariableProto* const var = cp_model->add_variables();
  var->add_domain(lb);
  var->add_domain(ub);
  return index;
}

// Structure to keep track of arcs created.
struct Arc {
  int tail;
  int head;

  friend bool operator==(const Arc& a, const Arc& b) {
    return a.tail == b.tail && a.head == b.head;
  }
  friend bool operator!=(const Arc& a, const Arc& b) { return !(a == b); }
  friend std::ostream& operator<<(std::ostream& strm, const Arc& arc) {
    return strm << "{" << arc.tail << ", " << arc.head << "}";
  }
  template <typename H>
  friend H AbslHashValue(H h, const Arc& a) {
    return H::combine(std::move(h), a.tail, a.head);
  }
};

using ArcVarMap = absl::flat_hash_map<Arc, int>;

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
  // TODO(user): Add support for multi-vehicle models.
  return {};
}

// Converts a CpSolverResponse to an Assignment containing next variables.
// Note: supports multiple routes.
bool ConvertToSolution(const CpSolverResponse& response,
                       const RoutingModel& model, const ArcVarMap& arc_vars,
                       Assignment* solution) {
  if (response.status() != CpSolverStatus::OPTIMAL &&
      response.status() != CpSolverStatus::FEASIBLE)
    return false;
  const int depot = model.Start(0);
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
  const int depot = model.Start(0);
  const int num_nodes = model.Nexts().size();
  for (int tail = 0; tail < num_nodes; ++tail) {
    const int tail_index = model.IsStart(tail) ? depot : tail;
    const int head = solution->Value(model.NextVar(tail));
    const int head_index = model.IsEnd(head) ? depot : head;
    if (tail_index == depot && head_index == depot) continue;
    hint->add_vars(gtl::FindOrDie(arc_vars, {tail_index, head_index}));
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
      1.0 / search_parameters.log_cost_scaling_factor());
  cp_model.mutable_objective()->set_offset(
      -search_parameters.log_cost_offset() *
      search_parameters.log_cost_scaling_factor());
  const sat::ArcVarMap arc_vars =
      sat::PopulateModelFromRoutingModel(model, &cp_model);
  sat::AddSolutionAsHintToModel(initial_solution, model, arc_vars, &cp_model);
  return sat::ConvertToSolution(
      sat::SolveRoutingModel(cp_model, model.RemainingTime(), nullptr), model,
      arc_vars, solution);
}

}  // namespace operations_research
