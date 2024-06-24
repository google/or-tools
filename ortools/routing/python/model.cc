// Copyright 2010-2024 Google LLC
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

#include <cstdint>
#include <functional>
#include <utility>

#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/solver_parameters.pb.h"
#include "ortools/routing/index_manager.h"
#include "ortools/routing/parameters.h"
#include "ortools/routing/python/doc.h"
#include "ortools/routing/python/index_manager_doc.h"
#include "ortools/routing/python/parameters_doc.h"
#include "ortools/routing/routing.h"
#include "pybind11/cast.h"
#include "pybind11/functional.h"
#include "pybind11/gil.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "pybind11_protobuf/native_proto_caster.h"

using ::operations_research::DefaultRoutingSearchParameters;
using ::operations_research::RoutingIndexManager;
using ::operations_research::RoutingModel;
using ::pybind11::arg;

PYBIND11_MODULE(model, m) {
  pybind11_protobuf::ImportNativeProtoCasters();

  pybind11::module::import(
      "ortools.constraint_solver.python.constraint_solver");

  m.def("default_routing_search_parameters", &DefaultRoutingSearchParameters,
        DOC(operations_research, DefaultRoutingSearchParameters));

  pybind11::class_<RoutingIndexManager>(
      m, "RoutingIndexManager", DOC(operations_research, RoutingIndexManager))
      .def(pybind11::init([](int num_nodes, int num_vehicles, int depot) {
             return new RoutingIndexManager(
                 num_nodes, num_vehicles,
                 RoutingIndexManager::NodeIndex(depot));
           }),
           DOC(operations_research, RoutingIndexManager, RoutingIndexManager))
      .def("num_nodes", &RoutingIndexManager::num_nodes,
           DOC(operations_research, RoutingIndexManager, num_nodes))
      .def("num_vehicles", &RoutingIndexManager::num_vehicles,
           DOC(operations_research, RoutingIndexManager, num_vehicles))
      .def("num_indices", &RoutingIndexManager::num_indices,
           DOC(operations_research, RoutingIndexManager, num_indices))
      .def(
          "index_to_node",
          [](RoutingIndexManager* routing_manager, int64_t index) {
            return routing_manager->IndexToNode(index).value();
          },
          DOC(operations_research, RoutingIndexManager, IndexToNode))
      .def(
          "node_to_index",
          [](RoutingIndexManager* routing_manager, int node) {
            return routing_manager->NodeToIndex(
                RoutingIndexManager::NodeIndex(node));
          },
          DOC(operations_research, RoutingIndexManager, NodeToIndex))
      .def("get_start_index", &RoutingIndexManager::GetStartIndex,
           DOC(operations_research, RoutingIndexManager, GetStartIndex))
      .def("get_end_index", &RoutingIndexManager::GetEndIndex,
           DOC(operations_research, RoutingIndexManager, GetEndIndex));

  pybind11::class_<RoutingModel>(m, "RoutingModel")
      .def(pybind11::init([](const RoutingIndexManager& routing_index_manager) {
        return new RoutingModel(routing_index_manager);
      }))
      .def("register_transit_callback",
           [](RoutingModel* routing_model,
              std::function<int64_t(int64_t, int64_t)> transit_callback) {
             return routing_model->RegisterTransitCallback(
                 std::move(transit_callback));
           })
      .def("set_arc_cost_evaluator_of_all_vehicles",
           &RoutingModel::SetArcCostEvaluatorOfAllVehicles,
           arg("transit_callback_index"))
      .def("solve", &RoutingModel::Solve,
           pybind11::return_value_policy::reference_internal,
           arg("assignment") = nullptr)
      .def("solve_with_parameters", &RoutingModel::SolveWithParameters,
           pybind11::return_value_policy::reference_internal,
           arg("search_parameters"), arg("solutions") = nullptr)
      .def("status", &RoutingModel::status)
      .def("start", &RoutingModel::Start, arg("vehicle"))
      .def("end", &RoutingModel::End, arg("vehicle"))
      .def("is_start", &RoutingModel::IsStart, arg("index"))
      .def("is_end", &RoutingModel::IsEnd, arg("index"))
      .def("next", &RoutingModel::Next, arg("assignment"), arg("index"))
      .def("next_var", &RoutingModel::NextVar,
           pybind11::return_value_policy::reference_internal, arg("index"))
      .def("get_arc_cost_for_vehicle", &RoutingModel::GetArcCostForVehicle,
           arg("from_index"), arg("to_index"), arg("vehicle"));
}
