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

// Converts a RoutingModel to CpModelProto. The mapping back to RoutingModel
// next variables can be obtained from the circuit constraint.
// TODO(user): Support multi-route models and any type of constraints.
bool PopulateBuilderWithRoutingModel(
    const RoutingModel& model, CpModelBuilder* builder,
    const CircuitConstraintProto** circuit_proto) {
  if (model.vehicles() > 1) return false;
  const int num_nodes = model.Nexts().size();
  LinearExpr objective;
  CircuitConstraint circuit = builder->AddCircuitConstraint();
  *circuit_proto = circuit.MutableProto()->mutable_circuit();
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
      const BoolVar arc = builder->NewBoolVar();
      circuit.AddArc(tail, head, arc);
      objective.AddTerm(arc, cost);
    }
  }
  builder->Minimize(objective);
  return true;
}

// Converts a CpSolverResponse to an Assignment containing next variables,
// using the circuit data to map arcs back to Next variables.
bool ConvertToSolution(const CpSolverResponse& response,
                       const RoutingModel& model,
                       const sat::CircuitConstraintProto& circuit,
                       Assignment* solution) {
  if (response.status() != CpSolverStatus::OPTIMAL &&
      response.status() != CpSolverStatus::FEASIBLE)
    return false;
  const int size = circuit.literals_size();
  for (int i = 0; i < size; ++i) {
    if (response.solution(circuit.literals(i)) != 0) {
      const int tail = circuit.tails(i);
      const int head = circuit.heads(i);
      if (!model.IsStart(head)) {
        solution->Add(model.NextVar(tail))->SetValue(head);
      } else {
        solution->Add(model.NextVar(tail))->SetValue(model.End(0));
      }
    }
  }
  return true;
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
bool SolveModelWithSat(RoutingModel* model, Assignment* solution) {
  sat::CpModelBuilder builder;
  const sat::CircuitConstraintProto* circuit = nullptr;
  return sat::PopulateBuilderWithRoutingModel(*model, &builder, &circuit) &&
         sat::ConvertToSolution(
             sat::SolveRoutingModel(builder.Build(), model->RemainingTime(),
                                    nullptr),
             *model, *circuit, solution);
}

}  // namespace operations_research
