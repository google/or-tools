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

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iterator>
#include <utility>
#include <vector>

#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/solver_parameters.pb.h"
#include "ortools/routing/index_manager.h"
#include "ortools/routing/parameters.h"
#include "ortools/routing/python/doc.h"
#include "ortools/routing/python/index_manager_doc.h"
#include "ortools/routing/python/parameters_doc.h"
#include "ortools/routing/routing.h"
#include "ortools/routing/types.h"
#include "pybind11/cast.h"
#include "pybind11/functional.h"
#include "pybind11/gil.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "pybind11_protobuf/native_proto_caster.h"

using ::operations_research::Assignment;
using ::operations_research::routing::DefaultRoutingModelParameters;
using ::operations_research::routing::DefaultRoutingSearchParameters;
using ::operations_research::routing::Dimension;
using ::operations_research::routing::IndexManager;
using ::operations_research::routing::Model;
using ::operations_research::routing::NodeIndex;
using ::operations_research::routing::RoutingModelParameters;
using ::operations_research::routing::RoutingSearchParameters;
using ::pybind11::arg;

PYBIND11_MODULE(model, m) {
  pybind11_protobuf::ImportNativeProtoCasters();

  pybind11::module::import(
      "ortools.constraint_solver.python.constraint_solver");

  m.def("default_routing_model_parameters", &DefaultRoutingModelParameters,
        DOC(operations_research, DefaultRoutingModelParameters));

  m.def("default_routing_search_parameters", &DefaultRoutingSearchParameters,
        DOC(operations_research, DefaultRoutingSearchParameters));

  pybind11::class_<IndexManager>(m, "IndexManager",
                                 DOC(operations_research, IndexManager))
      .def(pybind11::init([](int num_nodes, int num_vehicles, int depot) {
             return new IndexManager(num_nodes, num_vehicles, NodeIndex(depot));
           }),
           DOC(operations_research, IndexManager, IndexManager))
      .def(pybind11::init([](int num_nodes, int num_vehicles,
                             const std::vector<int> starts,
                             const std::vector<int> ends) {
             std::vector<NodeIndex> start_nodes;
             start_nodes.reserve(starts.size());
             std::transform(starts.cbegin(), starts.cend(),
                            std::back_inserter(start_nodes),
                            [](int node) { return NodeIndex(node); });

             std::vector<NodeIndex> end_nodes;
             end_nodes.reserve(ends.size());
             std::transform(ends.cbegin(), ends.cend(),
                            std::back_inserter(end_nodes),
                            [](int node) { return NodeIndex(node); });

             return new IndexManager(num_nodes, num_vehicles, start_nodes,
                                     end_nodes);
           }),
           DOC(operations_research, IndexManager, IndexManager))
      .def("num_nodes", &IndexManager::num_nodes,
           DOC(operations_research, IndexManager, num_nodes))
      .def("num_vehicles", &IndexManager::num_vehicles,
           DOC(operations_research, IndexManager, num_vehicles))
      .def("num_indices", &IndexManager::num_indices,
           DOC(operations_research, IndexManager, num_indices))
      .def(
          "index_to_node",
          [](IndexManager* routing_manager, int64_t index) {
            return routing_manager->IndexToNode(index).value();
          },
          DOC(operations_research, IndexManager, IndexToNode))
      .def(
          "node_to_index",
          [](IndexManager* routing_manager, int node) {
            return routing_manager->NodeToIndex(NodeIndex(node));
          },
          DOC(operations_research, IndexManager, NodeToIndex))
      .def("get_start_index", &IndexManager::GetStartIndex,
           DOC(operations_research, IndexManager, GetStartIndex))
      .def("get_end_index", &IndexManager::GetEndIndex,
           DOC(operations_research, IndexManager, GetEndIndex));

  pybind11::class_<Dimension>(m, "Dimension")
      .def("model", &Dimension::model,
           pybind11::return_value_policy::reference_internal)
      .def("get_transit_value", &Dimension::GetTransitValue, arg("from_index"),
           arg("to_index"), arg("vehicle"))
      .def("cumul_var", &Dimension::CumulVar,
           pybind11::return_value_policy::reference_internal, arg("index"));

  pybind11::class_<Model> rm(m, "Model");
  rm.def(pybind11::init([](const IndexManager& routing_index_manager) {
    return new Model(routing_index_manager);
  }));
  rm.def(pybind11::init([](const IndexManager& routing_index_manager,
                           const RoutingModelParameters& parameters) {
    return new Model(routing_index_manager, parameters);
  }));
  rm.def(
      "register_transit_matrix",
      [](Model* routing_model,
         std::vector<std::vector<int64_t>> transit_matrix) {
        return routing_model->RegisterTransitMatrix(std::move(transit_matrix));
      });
  rm.def("register_unary_transit_vector",
         [](Model* routing_model, std::vector<int64_t> transit_vector) {
           return routing_model->RegisterUnaryTransitVector(
               std::move(transit_vector));
         });
  rm.def("register_unary_transit_callback",
         [](Model* routing_model,
            std::function<int64_t(int64_t)> transit_callback) {
           return routing_model->RegisterUnaryTransitCallback(
               std::move(transit_callback));
         });
  rm.def("register_transit_callback",
         [](Model* routing_model,
            std::function<int64_t(int64_t, int64_t)> transit_callback) {
           return routing_model->RegisterTransitCallback(
               std::move(transit_callback));
         });
  rm.def("set_arc_cost_evaluator_of_all_vehicles",
         &Model::SetArcCostEvaluatorOfAllVehicles,
         arg("transit_callback_index"));
  rm.def("add_dimension", &Model::AddDimension, arg("evaluator_index"),
         arg("slack_max"), arg("capacity"), arg("fix_start_cumul_to_zero"),
         arg("name"));
  rm.def("add_dimension_with_vehicle_capacity",
         &Model::AddDimensionWithVehicleCapacity, arg("evaluator_index"),
         arg("slack_max"), arg("vehicle_capacities"),
         arg("fix_start_cumul_to_zero"), arg("name"));
  rm.def("add_dimension_with_vehicle_transits",
         &Model::AddDimensionWithVehicleTransits, arg("evaluator_indices"),
         arg("slack_max"), arg("capacity"), arg("fix_start_cumul_to_zero"),
         arg("name"));
  rm.def("add_dimension_with_vehicle_transit_and_capacity",
         &Model::AddDimensionWithVehicleTransitAndCapacity,
         arg("evaluator_indices"), arg("slack_max"), arg("vehicle_capacities"),
         arg("fix_start_cumul_to_zero"), arg("name"));
  rm.def("add_constant_dimension", &Model::AddConstantDimension, arg("value"),
         arg("capacity"), arg("fix_start_cumul_to_zero"), arg("name"));
  rm.def("add_vector_dimension", &Model::AddVectorDimension, arg("values"),
         arg("capacity"), arg("fix_start_cumul_to_zero"), arg("name"));
  rm.def("add_matrix_dimension", &Model::AddMatrixDimension, arg("values"),
         arg("capacity"), arg("fix_start_cumul_to_zero"), arg("name"));
  rm.def("get_dimension_or_die", &Model::GetDimensionOrDie,
         pybind11::return_value_policy::reference_internal,
         arg("dimension_name"));
  rm.def("close_model", &Model::CloseModel);
  rm.def("close_model_with_parameters", &Model::CloseModelWithParameters,
         arg("search_parameters"));
  rm.def("solve", &Model::Solve,
         pybind11::return_value_policy::reference_internal,
         arg("assignment") = nullptr);
  // TODO(user) Add support for solutions parameters too.
  rm.def(
      "solve_with_parameters",
      [](Model* routing_model, const RoutingSearchParameters& search_parameters
         /*,std::vector<const Assignment*>* solutions = nullptr*/)
          -> const Assignment* {
        return routing_model->SolveWithParameters(search_parameters, nullptr);
      },
      pybind11::return_value_policy::reference_internal,
      arg("search_parameters")
      //, arg("solutions") = nullptr
  );
  rm.def("status", &Model::status);
  rm.def("nodes", &Model::nodes);
  rm.def("vehicles", &Model::vehicles);
  rm.def("size", &Model::Size);
  rm.def("start", &Model::Start, arg("vehicle"));
  rm.def("end", &Model::End, arg("vehicle"));
  rm.def("is_start", &Model::IsStart, arg("index"));
  rm.def("is_end", &Model::IsEnd, arg("index"));
  rm.def("next", &Model::Next, arg("assignment"), arg("index"));
  rm.def("next_var", &Model::NextVar,
         pybind11::return_value_policy::reference_internal, arg("index"));
  rm.def("get_arc_cost_for_vehicle", &Model::GetArcCostForVehicle,
         arg("from_index"), arg("to_index"), arg("vehicle"));
  rm.def_property_readonly("solver", &Model::solver,
                           pybind11::return_value_policy::reference_internal);

  pybind11::enum_<Model::PenaltyCostBehavior>(rm, "PenaltyCostBehavior")
      .value("PENALIZE_ONCE", Model::PenaltyCostBehavior::PENALIZE_ONCE)
      .value("PENALIZE_PER_INACTIVE",
             Model::PenaltyCostBehavior::PENALIZE_PER_INACTIVE)
      .export_values();

  rm.def(
      "add_disjunction",
      [](Model* routing_model, const std::vector<int64_t>& indices,
         int64_t penalty, int64_t max_cardinality,
         Model::PenaltyCostBehavior penalty_cost_behavior) -> int {
        return static_cast<int>(routing_model
                                    ->AddDisjunction(indices, penalty,
                                                     max_cardinality,
                                                     penalty_cost_behavior)
                                    .value());
      },
      // &Model::AddDisjunction,
      arg("indices"), arg("penalty") = Model::kNoPenalty,
      arg("max_cardinality") = 1,
      arg("penalty_cost_behavior") = Model::PenaltyCostBehavior::PENALIZE_ONCE);
}
