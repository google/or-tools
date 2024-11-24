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
#include "pybind11/cast.h"
#include "pybind11/functional.h"
#include "pybind11/gil.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "pybind11_protobuf/native_proto_caster.h"

using ::operations_research::Assignment;
using ::operations_research::routing::DefaultRoutingModelParameters;
using ::operations_research::routing::DefaultRoutingSearchParameters;
using ::operations_research::routing::RoutingDimension;
using ::operations_research::routing::RoutingIndexManager;
using ::operations_research::routing::RoutingModel;
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

  pybind11::class_<RoutingIndexManager>(
      m, "RoutingIndexManager", DOC(operations_research, RoutingIndexManager))
      .def(pybind11::init([](int num_nodes, int num_vehicles, int depot) {
             return new RoutingIndexManager(
                 num_nodes, num_vehicles,
                 RoutingIndexManager::NodeIndex(depot));
           }),
           DOC(operations_research, RoutingIndexManager, RoutingIndexManager))
      .def(pybind11::init([](int num_nodes, int num_vehicles,
                             const std::vector<int> starts,
                             const std::vector<int> ends) {
             std::vector<RoutingIndexManager::NodeIndex> start_nodes;
             start_nodes.reserve(starts.size());
             std::transform(starts.cbegin(), starts.cend(),
                            std::back_inserter(start_nodes), [](int node) {
                              return RoutingIndexManager::NodeIndex(node);
                            });

             std::vector<RoutingIndexManager::NodeIndex> end_nodes;
             end_nodes.reserve(ends.size());
             std::transform(
                 ends.cbegin(), ends.cend(), std::back_inserter(end_nodes),
                 [](int node) { return RoutingIndexManager::NodeIndex(node); });

             return new RoutingIndexManager(num_nodes, num_vehicles,
                                            start_nodes, end_nodes);
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

  pybind11::class_<RoutingDimension>(m, "RoutingDimension")
      .def("model", &RoutingDimension::model,
           pybind11::return_value_policy::reference_internal)
      .def("get_transit_value", &RoutingDimension::GetTransitValue,
           arg("from_index"), arg("to_index"), arg("vehicle"))
      .def("cumul_var", &RoutingDimension::CumulVar,
           pybind11::return_value_policy::reference_internal, arg("index"));

  pybind11::class_<RoutingModel> rm(m, "RoutingModel");
  rm.def(pybind11::init([](const RoutingIndexManager& routing_index_manager) {
    return new RoutingModel(routing_index_manager);
  }));
  rm.def(pybind11::init([](const RoutingIndexManager& routing_index_manager,
                           const RoutingModelParameters& parameters) {
    return new RoutingModel(routing_index_manager, parameters);
  }));
  rm.def(
      "register_transit_matrix",
      [](RoutingModel* routing_model,
         std::vector<std::vector<int64_t>> transit_matrix) {
        return routing_model->RegisterTransitMatrix(std::move(transit_matrix));
      });
  rm.def("register_unary_transit_vector",
         [](RoutingModel* routing_model, std::vector<int64_t> transit_vector) {
           return routing_model->RegisterUnaryTransitVector(
               std::move(transit_vector));
         });
  rm.def("register_unary_transit_callback",
         [](RoutingModel* routing_model,
            std::function<int64_t(int64_t)> transit_callback) {
           return routing_model->RegisterUnaryTransitCallback(
               std::move(transit_callback));
         });
  rm.def("register_transit_callback",
         [](RoutingModel* routing_model,
            std::function<int64_t(int64_t, int64_t)> transit_callback) {
           return routing_model->RegisterTransitCallback(
               std::move(transit_callback));
         });
  rm.def("set_arc_cost_evaluator_of_all_vehicles",
         &RoutingModel::SetArcCostEvaluatorOfAllVehicles,
         arg("transit_callback_index"));
  rm.def("add_dimension", &RoutingModel::AddDimension, arg("evaluator_index"),
         arg("slack_max"), arg("capacity"), arg("fix_start_cumul_to_zero"),
         arg("name"));
  rm.def("add_dimension_with_vehicle_capacity",
         &RoutingModel::AddDimensionWithVehicleCapacity, arg("evaluator_index"),
         arg("slack_max"), arg("vehicle_capacities"),
         arg("fix_start_cumul_to_zero"), arg("name"));
  rm.def("add_dimension_with_vehicle_transits",
         &RoutingModel::AddDimensionWithVehicleTransits,
         arg("evaluator_indices"), arg("slack_max"), arg("capacity"),
         arg("fix_start_cumul_to_zero"), arg("name"));
  rm.def("add_dimension_with_vehicle_transit_and_capacity",
         &RoutingModel::AddDimensionWithVehicleTransitAndCapacity,
         arg("evaluator_indices"), arg("slack_max"), arg("vehicle_capacities"),
         arg("fix_start_cumul_to_zero"), arg("name"));
  rm.def("add_constant_dimension", &RoutingModel::AddConstantDimension,
         arg("value"), arg("capacity"), arg("fix_start_cumul_to_zero"),
         arg("name"));
  rm.def("add_vector_dimension", &RoutingModel::AddVectorDimension,
         arg("values"), arg("capacity"), arg("fix_start_cumul_to_zero"),
         arg("name"));
  rm.def("add_matrix_dimension", &RoutingModel::AddMatrixDimension,
         arg("values"), arg("capacity"), arg("fix_start_cumul_to_zero"),
         arg("name"));
  rm.def("get_dimension_or_die", &RoutingModel::GetDimensionOrDie,
         pybind11::return_value_policy::reference_internal,
         arg("dimension_name"));
  rm.def("close_model", &RoutingModel::CloseModel);
  rm.def("close_model_with_parameters", &RoutingModel::CloseModelWithParameters,
         arg("search_parameters"));
  rm.def("solve", &RoutingModel::Solve,
         pybind11::return_value_policy::reference_internal,
         arg("assignment") = nullptr);
  // TODO(user) Add support for solutions parameters too.
  rm.def(
      "solve_with_parameters",
      [](RoutingModel* routing_model,
         const RoutingSearchParameters& search_parameters
         /*,std::vector<const Assignment*>* solutions = nullptr*/)
          -> const Assignment* {
        return routing_model->SolveWithParameters(search_parameters, nullptr);
      },
      pybind11::return_value_policy::reference_internal,
      arg("search_parameters")
      //, arg("solutions") = nullptr
  );
  rm.def("status", &RoutingModel::status);
  rm.def("nodes", &RoutingModel::nodes);
  rm.def("vehicles", &RoutingModel::vehicles);
  rm.def("size", &RoutingModel::Size);
  rm.def("start", &RoutingModel::Start, arg("vehicle"));
  rm.def("end", &RoutingModel::End, arg("vehicle"));
  rm.def("is_start", &RoutingModel::IsStart, arg("index"));
  rm.def("is_end", &RoutingModel::IsEnd, arg("index"));
  rm.def("next", &RoutingModel::Next, arg("assignment"), arg("index"));
  rm.def("next_var", &RoutingModel::NextVar,
         pybind11::return_value_policy::reference_internal, arg("index"));
  rm.def("get_arc_cost_for_vehicle", &RoutingModel::GetArcCostForVehicle,
         arg("from_index"), arg("to_index"), arg("vehicle"));
  rm.def("solver", &RoutingModel::solver,
         pybind11::return_value_policy::reference_internal);

  pybind11::enum_<RoutingModel::PenaltyCostBehavior>(rm, "PenaltyCostBehavior")
      .value("PENALIZE_ONCE", RoutingModel::PenaltyCostBehavior::PENALIZE_ONCE)
      .value("PENALIZE_PER_INACTIVE",
             RoutingModel::PenaltyCostBehavior::PENALIZE_PER_INACTIVE)
      .export_values();

  rm.def(
      "add_disjunction",
      [](RoutingModel* routing_model, const std::vector<int64_t>& indices,
         int64_t penalty, int64_t max_cardinality,
         RoutingModel::PenaltyCostBehavior penalty_cost_behavior) -> int {
        return static_cast<int>(routing_model
                                    ->AddDisjunction(indices, penalty,
                                                     max_cardinality,
                                                     penalty_cost_behavior)
                                    .value());
      },
      // &RoutingModel::AddDisjunction,
      arg("indices"), arg("penalty") = RoutingModel::kNoPenalty,
      arg("max_cardinality") = 1,
      arg("penalty_cost_behavior") =
          RoutingModel::PenaltyCostBehavior::PENALIZE_ONCE);
}
