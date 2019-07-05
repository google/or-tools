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
bool RoutingModelCanBeSolvedBySat(const RoutingModel& model) {
  return model.vehicles() == 1;
}

// Converts a RoutingModel to CpModelProto. The mapping back to RoutingModel
// next variables can be obtained from the circuit constraint.
// TODO(user): Support multi-route models and any type of constraints.
CircuitConstraintProto* PopulateBuilderWithRoutingModel(
    const RoutingModel& model, CpModelBuilder* builder) {
  const int num_nodes = model.Nexts().size();
  LinearExpr objective;
  CircuitConstraint circuit = builder->AddCircuitConstraint();
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
  return circuit.MutableProto()->mutable_circuit();
}

// Converts a CpSolverResponse to an Assignment containing next variables,
// using the circuit data to map arcs back to Next variables.
bool ConvertToSolution(const CpSolverResponse& response,
                       const RoutingModel& model,
                       const CircuitConstraintProto& circuit,
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

void AddSolutionAsHintToModel(const Assignment* solution,
                              const RoutingModel& model,
                              const CircuitConstraintProto& circuit,
                              CpModelProto* cp_model) {
  if (solution == nullptr) return;
  PartialVariableAssignment* const hint = cp_model->mutable_solution_hint();
  hint->Clear();
  const int size = circuit.literals_size();
  for (int i = 0; i < size; ++i) {
    hint->add_vars(circuit.literals(i));
    const int tail = circuit.tails(i);
    const int head = circuit.heads(i);
    hint->add_values(solution->Value(model.NextVar(tail)) == head);
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
                       const Assignment* initial_solution,
                       Assignment* solution) {
  if (!sat::RoutingModelCanBeSolvedBySat(model)) return false;
  sat::CpModelBuilder builder;
  const sat::CircuitConstraintProto* const circuit =
      sat::PopulateBuilderWithRoutingModel(model, &builder);
  sat::AddSolutionAsHintToModel(initial_solution, model, *circuit,
                                builder.MutableProto());
  return sat::ConvertToSolution(
      sat::SolveRoutingModel(builder.Proto(), model.RemainingTime(), nullptr),
      model, *circuit, solution);
}

}  // namespace operations_research
