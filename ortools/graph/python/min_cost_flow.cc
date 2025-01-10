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

#include "ortools/graph/min_cost_flow.h"

#include "pybind11/numpy.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

using ::operations_research::MinCostFlowBase;
using ::operations_research::SimpleMinCostFlow;
using ::pybind11::arg;

PYBIND11_MODULE(min_cost_flow, m) {
  pybind11::class_<SimpleMinCostFlow> smcf(m, "SimpleMinCostFlow");
  smcf.def(pybind11::init<>());
  smcf.def("add_arc_with_capacity_and_unit_cost",
           &SimpleMinCostFlow::AddArcWithCapacityAndUnitCost, arg("tail"),
           arg("head"), arg("capacity"), arg("unit_cost"));
  smcf.def(
      "add_arcs_with_capacity_and_unit_cost",
      pybind11::vectorize(&SimpleMinCostFlow::AddArcWithCapacityAndUnitCost));
  smcf.def("set_node_supply", &SimpleMinCostFlow::SetNodeSupply, arg("node"),
           arg("supply"));
  smcf.def("set_nodes_supplies",
           pybind11::vectorize(&SimpleMinCostFlow::SetNodeSupply));
  smcf.def("num_nodes", &SimpleMinCostFlow::NumNodes);
  smcf.def("num_arcs", &SimpleMinCostFlow::NumArcs);
  smcf.def("tail", &SimpleMinCostFlow::Tail, arg("arc"));
  smcf.def("head", &SimpleMinCostFlow::Head, arg("arc"));
  smcf.def("capacity", &SimpleMinCostFlow::Capacity, arg("arc"));
  smcf.def("supply", &SimpleMinCostFlow::Supply, arg("node"));
  smcf.def("unit_cost", &SimpleMinCostFlow::UnitCost, arg("arc"));
  smcf.def("solve", &SimpleMinCostFlow::Solve);
  smcf.def("solve_max_flow_with_min_cost",
           &SimpleMinCostFlow::SolveMaxFlowWithMinCost);
  smcf.def("optimal_cost", &SimpleMinCostFlow::OptimalCost);
  smcf.def("maximum_flow", &SimpleMinCostFlow::MaximumFlow);
  smcf.def("flow", &SimpleMinCostFlow::Flow, arg("arc"));
  smcf.def("flows", pybind11::vectorize(&SimpleMinCostFlow::Flow));

  pybind11::enum_<SimpleMinCostFlow::Status>(smcf, "Status")
      .value("BAD_COST_RANGE", MinCostFlowBase::Status::BAD_COST_RANGE)
      .value("BAD_CAPACITY_RANGE", MinCostFlowBase::Status::BAD_CAPACITY_RANGE)
      .value("BAD_RESULT", MinCostFlowBase::Status::BAD_RESULT)
      .value("FEASIBLE", MinCostFlowBase::Status::FEASIBLE)
      .value("INFEASIBLE", MinCostFlowBase::Status::INFEASIBLE)
      .value("NOT_SOLVED", MinCostFlowBase::Status::NOT_SOLVED)
      .value("OPTIMAL", MinCostFlowBase::Status::OPTIMAL)
      .value("UNBALANCED", MinCostFlowBase::Status::UNBALANCED)
      .export_values();
}
