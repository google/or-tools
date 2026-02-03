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

#include "ortools/routing/routing.h"

#include <cstdint>
#include <functional>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_set.h"
#include "google/protobuf/duration.pb.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/search_stats.pb.h"
#include "ortools/constraint_solver/solver_parameters.pb.h"
#include "ortools/port/proto_utils.h"  // IWYU: keep
#include "ortools/routing/enums.pb.h"
#include "ortools/routing/heuristic_parameters.pb.h"
#include "ortools/routing/ils.pb.h"
#include "ortools/routing/index_manager.h"
#include "ortools/routing/parameters.h"
#include "ortools/routing/python/doc.h"                // IWYU pragma: keep
#include "ortools/routing/python/index_manager_doc.h"  // IWYU pragma: keep. NOLINT
#include "ortools/routing/python/parameters_doc.h"     // IWYU pragma: keep
#include "ortools/routing/types.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/optional_boolean.pb.h"
#include "ortools/util/sorted_interval_list.h"
#include "pybind11/cast.h"
#include "pybind11/functional.h"
#include "pybind11/gil.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "pybind11_abseil/absl_casters.h"

namespace py = ::pybind11;

using ::operations_research::Assignment;
using ::operations_research::routing::DefaultRoutingModelParameters;
using ::operations_research::routing::DefaultRoutingSearchParameters;
using ::operations_research::routing::Dimension;
using ::operations_research::routing::IndexManager;
using ::operations_research::routing::Model;
using ::operations_research::routing::NodeIndex;
using ::operations_research::routing::RoutingModelParameters;
using ::operations_research::routing::RoutingSearchParameters;

PYBIND11_MODULE(routing, m) {
  pybind11::module::import(
      "ortools.constraint_solver.python.constraint_solver");
  pybind11::module::import("ortools.util.python.sorted_interval_list");

#define IMPORT_PROTO_WRAPPER_CODE
#include "ortools/routing/python/proto_builder_pybind11.h"
#undef IMPORT_PROTO_WRAPPER_CODE

  m.def("default_routing_model_parameters", &DefaultRoutingModelParameters,
        DOC(operations_research, routing, DefaultRoutingModelParameters));

  m.def("default_routing_search_parameters", &DefaultRoutingSearchParameters,
        DOC(operations_research, routing, DefaultRoutingSearchParameters));

  pybind11::class_<operations_research::routing::BoundCost>(m, "BoundCost")
      .def(pybind11::init<>())
      .def(pybind11::init<int64_t, int64_t>(), py::arg("bound"),
           py::arg("cost"))
      .def_readwrite("bound", &operations_research::routing::BoundCost::bound)
      .def_readwrite("cost", &operations_research::routing::BoundCost::cost);

  pybind11::class_<operations_research::PiecewiseLinearFunction>(
      m, "PiecewiseLinearFunction")
      .def(pybind11::init([](std::vector<int64_t> points_x,
                             std::vector<int64_t> points_y,
                             std::vector<int64_t> slopes,
                             std::vector<int64_t> other_points_x) {
             return operations_research::PiecewiseLinearFunction::
                 CreatePiecewiseLinearFunction(
                     std::move(points_x), std::move(points_y),
                     std::move(slopes), std::move(other_points_x));
           }),
           py::arg("points_x"), py::arg("points_y"), py::arg("slopes"),
           py::arg("other_points_x"))
      .def("value", &operations_research::PiecewiseLinearFunction::Value,
           py::arg("x"));

  pybind11::class_<IndexManager>(
      m, "IndexManager", DOC(operations_research, routing, IndexManager))
      .def(pybind11::init([](int num_nodes, int num_vehicles, int depot) {
             return new IndexManager(num_nodes, num_vehicles, NodeIndex(depot));
           }),
           py::arg("num_nodes"), py::arg("num_vehicles"), py::arg("depot"),
           DOC(operations_research, routing, IndexManager, IndexManager))
      .def(pybind11::init([](int num_nodes, int num_vehicles,
                             const std::vector<int>& starts,
                             const std::vector<int>& ends) {
             std::vector<NodeIndex> start_node_indices;
             start_node_indices.reserve(starts.size());
             absl::c_transform(starts, std::back_inserter(start_node_indices),
                               [](int node) { return NodeIndex(node); });

             std::vector<NodeIndex> end_node_indices;
             end_node_indices.reserve(ends.size());
             absl::c_transform(ends, std::back_inserter(end_node_indices),
                               [](int node) { return NodeIndex(node); });

             return new IndexManager(num_nodes, num_vehicles,
                                     start_node_indices, end_node_indices);
           }),
           py::arg("num_nodes"), py::arg("num_vehicles"), py::arg("starts"),
           py::arg("ends"),
           DOC(operations_research, routing, IndexManager, IndexManager))
      .def("num_nodes", &IndexManager::num_nodes,
           DOC(operations_research, routing, IndexManager, num_nodes))
      .def("num_vehicles", &IndexManager::num_vehicles,
           DOC(operations_research, routing, IndexManager, num_vehicles))
      .def("num_indices", &IndexManager::num_indices,
           DOC(operations_research, routing, IndexManager, num_indices))
      .def(
          "index_to_node",
          [](IndexManager* index_manager, int64_t index) {
            return index_manager->IndexToNode(index).value();
          },
          DOC(operations_research, routing, IndexManager, IndexToNode))
      .def(
          "node_to_index",
          [](IndexManager* index_manager, int node) {
            return index_manager->NodeToIndex(NodeIndex(node));
          },
          DOC(operations_research, routing, IndexManager, NodeToIndex))
      .def("get_start_index", &IndexManager::GetStartIndex,
           DOC(operations_research, routing, IndexManager, GetStartIndex))
      .def("get_end_index", &IndexManager::GetEndIndex,
           DOC(operations_research, routing, IndexManager, GetEndIndex))
      .def("num_unique_depots", &IndexManager::num_unique_depots,
           DOC(operations_research, routing, IndexManager, num_unique_depots))
      .def(
          "nodes_to_indices",
          [](const IndexManager& manager, const std::vector<int>& nodes) {
            std::vector<NodeIndex> node_indices;
            node_indices.reserve(nodes.size());
            for (const int node : nodes) {
              node_indices.push_back(NodeIndex(node));
            }
            return manager.NodesToIndices(node_indices);
          },
          py::arg("nodes"),
          DOC(operations_research, routing, IndexManager, NodesToIndices))
      .def(
          "indices_to_nodes",
          [](const IndexManager& manager, const std::vector<int64_t>& indices) {
            std::vector<int> nodes;
            const std::vector<NodeIndex> node_indices =
                manager.IndicesToNodes(absl::MakeConstSpan(indices));
            nodes.reserve(node_indices.size());
            for (const NodeIndex node_index : node_indices) {
              nodes.push_back(node_index.value());
            }
            return nodes;
          },
          py::arg("indices"),
          DOC(operations_research, routing, IndexManager, IndicesToNodes))
      .def_readonly_static("k_unassigned", &IndexManager::kUnassigned);

  pybind11::class_<Dimension>(m, "Dimension",
                              DOC(operations_research, routing, Dimension))
      .def("model", &Dimension::model,
           pybind11::return_value_policy::reference_internal,
           DOC(operations_research, routing, Dimension, model))
      .def("get_transit_value", &Dimension::GetTransitValue,
           py::arg("from_index"), py::arg("to_index"), py::arg("vehicle"),
           DOC(operations_research, routing, Dimension, GetTransitValue))
      .def("cumul_var", &Dimension::CumulVar,
           pybind11::return_value_policy::reference_internal, py::arg("index"),
           DOC(operations_research, routing, Dimension, CumulVar))
      .def("transit_var", &Dimension::TransitVar,
           pybind11::return_value_policy::reference_internal, py::arg("index"),
           DOC(operations_research, routing, Dimension, TransitVar))
      .def("slack_var", &Dimension::SlackVar,
           pybind11::return_value_policy::reference_internal, py::arg("index"),
           DOC(operations_research, routing, Dimension, SlackVar))
      .def("set_span_upper_bound_for_vehicle",
           &Dimension::SetSpanUpperBoundForVehicle, py::arg("upper_bound"),
           py::arg("vehicle"),
           DOC(operations_research, routing, Dimension,
               SetSpanUpperBoundForVehicle))
      .def("set_span_cost_coefficient_for_vehicle",
           &Dimension::SetSpanCostCoefficientForVehicle, py::arg("coefficient"),
           py::arg("vehicle"),
           DOC(operations_research, routing, Dimension,
               SetSpanCostCoefficientForVehicle))
      .def("set_span_cost_coefficient_for_all_vehicles",
           &Dimension::SetSpanCostCoefficientForAllVehicles,
           py::arg("coefficient"),
           DOC(operations_research, routing, Dimension,
               SetSpanCostCoefficientForAllVehicles))
      .def("set_global_span_cost_coefficient",
           &Dimension::SetGlobalSpanCostCoefficient, py::arg("coefficient"),
           DOC(operations_research, routing, Dimension,
               SetGlobalSpanCostCoefficient))
      .def("set_slack_cost_coefficient_for_vehicle",
           &Dimension::SetSlackCostCoefficientForVehicle,
           py::arg("coefficient"), py::arg("vehicle"),
           DOC(operations_research, routing, Dimension,
               SetSlackCostCoefficientForVehicle))
      .def("set_cumul_var_soft_upper_bound",
           &Dimension::SetCumulVarSoftUpperBound, py::arg("node"),
           py::arg("limit"), py::arg("coefficient"),
           DOC(operations_research, routing, Dimension,
               SetCumulVarSoftUpperBound))
      .def("set_cumul_var_soft_lower_bound",
           &Dimension::SetCumulVarSoftLowerBound, py::arg("node"),
           py::arg("limit"), py::arg("coefficient"),
           DOC(operations_research, routing, Dimension,
               SetCumulVarSoftLowerBound))
      .def("get_transit_value_from_class", &Dimension::GetTransitValueFromClass,
           py::arg("from_index"), py::arg("to_index"), py::arg("vehicle_class"),
           DOC(operations_research, routing, Dimension,
               GetTransitValueFromClass))
      .def("set_cumul_var_range", &Dimension::SetCumulVarRange,
           py::arg("index"), py::arg("min"), py::arg("max"),
           DOC(operations_research, routing, Dimension, SetCumulVarRange))
      .def("get_cumul_var_min", &Dimension::GetCumulVarMin, py::arg("index"),
           DOC(operations_research, routing, Dimension, GetCumulVarMin))
      .def("get_cumul_var_max", &Dimension::GetCumulVarMax, py::arg("index"),
           DOC(operations_research, routing, Dimension, GetCumulVarMax))

      .def("has_cumul_var_soft_upper_bound",
           &Dimension::HasCumulVarSoftUpperBound, py::arg("index"),
           DOC(operations_research, routing, Dimension,
               HasCumulVarSoftUpperBound))
      .def("get_cumul_var_soft_upper_bound",
           &Dimension::GetCumulVarSoftUpperBound, py::arg("index"),
           DOC(operations_research, routing, Dimension,
               GetCumulVarSoftUpperBound))
      .def("get_cumul_var_soft_upper_bound_coefficient",
           &Dimension::GetCumulVarSoftUpperBoundCoefficient, py::arg("index"),
           DOC(operations_research, routing, Dimension,
               GetCumulVarSoftUpperBoundCoefficient))
      .def("has_cumul_var_soft_lower_bound",
           &Dimension::HasCumulVarSoftLowerBound, py::arg("index"),
           DOC(operations_research, routing, Dimension,
               HasCumulVarSoftLowerBound))
      .def("get_cumul_var_soft_lower_bound",
           &Dimension::GetCumulVarSoftLowerBound, py::arg("index"),
           DOC(operations_research, routing, Dimension,
               GetCumulVarSoftLowerBound))
      .def("get_cumul_var_soft_lower_bound_coefficient",
           &Dimension::GetCumulVarSoftLowerBoundCoefficient, py::arg("index"),
           DOC(operations_research, routing, Dimension,
               GetCumulVarSoftLowerBoundCoefficient))
      .def("has_soft_span_upper_bounds", &Dimension::HasSoftSpanUpperBounds,
           DOC(operations_research, routing, Dimension, HasSoftSpanUpperBounds))
      .def("set_soft_span_upper_bound_for_vehicle",
           &Dimension::SetSoftSpanUpperBoundForVehicle, py::arg("bound_cost"),
           py::arg("vehicle"),
           DOC(operations_research, routing, Dimension,
               SetSoftSpanUpperBoundForVehicle))
      .def("get_soft_span_upper_bound_for_vehicle",
           &Dimension::GetSoftSpanUpperBoundForVehicle, py::arg("vehicle"),
           DOC(operations_research, routing, Dimension,
               GetSoftSpanUpperBoundForVehicle))
      .def("set_quadratic_cost_soft_span_upper_bound_for_vehicle",
           &Dimension::SetQuadraticCostSoftSpanUpperBoundForVehicle,
           py::arg("bound_cost"), py::arg("vehicle"),
           DOC(operations_research, routing, Dimension,
               SetQuadraticCostSoftSpanUpperBoundForVehicle))
      .def("has_quadratic_cost_soft_span_upper_bounds",
           &Dimension::HasQuadraticCostSoftSpanUpperBounds,
           DOC(operations_research, routing, Dimension,
               HasQuadraticCostSoftSpanUpperBounds))
      .def("get_quadratic_cost_soft_span_upper_bound_for_vehicle",
           &Dimension::GetQuadraticCostSoftSpanUpperBoundForVehicle,
           py::arg("vehicle"),
           DOC(operations_research, routing, Dimension,
               GetQuadraticCostSoftSpanUpperBoundForVehicle))
      .def("set_cumul_var_piecewise_linear_cost",
           &Dimension::SetCumulVarPiecewiseLinearCost, py::arg("index"),
           py::arg("cost"),
           DOC(operations_research, routing, Dimension,
               SetCumulVarPiecewiseLinearCost))
      .def("has_cumul_var_piecewise_linear_cost",
           &Dimension::HasCumulVarPiecewiseLinearCost, py::arg("index"),
           DOC(operations_research, routing, Dimension,
               HasCumulVarPiecewiseLinearCost))
      .def("get_cumul_var_piecewise_linear_cost",
           &Dimension::GetCumulVarPiecewiseLinearCost,
           pybind11::return_value_policy::reference_internal, py::arg("index"),
           DOC(operations_research, routing, Dimension,
               GetCumulVarPiecewiseLinearCost))
      .def("set_break_distance_duration_of_vehicle",
           &Dimension::SetBreakDistanceDurationOfVehicle, py::arg("distance"),
           py::arg("duration"), py::arg("vehicle"),
           DOC(operations_research, routing, Dimension,
               SetBreakDistanceDurationOfVehicle))
      .def("initialize_breaks", &Dimension::InitializeBreaks,
           DOC(operations_research, routing, Dimension, InitializeBreaks))
      .def("has_break_constraints", &Dimension::HasBreakConstraints,
           DOC(operations_research, routing, Dimension, HasBreakConstraints))
      .def("get_break_intervals_of_vehicle",
           &Dimension::GetBreakIntervalsOfVehicle,
           pybind11::return_value_policy::reference_internal,
           py::arg("vehicle"),
           DOC(operations_research, routing, Dimension,
               GetBreakIntervalsOfVehicle))
      .def("get_break_distance_duration_of_vehicle",
           &Dimension::GetBreakDistanceDurationOfVehicle,
           pybind11::return_value_policy::reference_internal,
           py::arg("vehicle"),
           DOC(operations_research, routing, Dimension,
               GetBreakDistanceDurationOfVehicle))
      .def(
          "set_break_intervals_of_vehicle",
          [](Dimension* dimension,
             std::vector<operations_research::IntervalVar*> breaks, int vehicle,
             int pre_travel_evaluator, int post_travel_evaluator) {
            dimension->SetBreakIntervalsOfVehicle(std::move(breaks), vehicle,
                                                  pre_travel_evaluator,
                                                  post_travel_evaluator);
          },
          py::arg("breaks"), py::arg("vehicle"),
          py::arg("pre_travel_evaluator"), py::arg("post_travel_evaluator"),
          DOC(operations_research, routing, Dimension,
              SetBreakIntervalsOfVehicle))
      .def(
          "set_break_intervals_of_vehicle",
          [](Dimension* dimension,
             std::vector<operations_research::IntervalVar*> breaks, int vehicle,
             std::vector<int64_t> node_visit_transits) {
            dimension->SetBreakIntervalsOfVehicle(
                std::move(breaks), vehicle, std::move(node_visit_transits));
          },
          py::arg("breaks"), py::arg("vehicle"), py::arg("node_visit_transits"),
          DOC(operations_research, routing, Dimension,
              SetBreakIntervalsOfVehicle, 2));

  pybind11::class_<operations_research::routing::SearchStats>(m, "SearchStats")
      .def_readonly("num_cp_sat_calls_in_lp_scheduling",
                    &operations_research::routing::SearchStats::
                        num_cp_sat_calls_in_lp_scheduling)
      .def_readonly("num_glop_calls_in_lp_scheduling",
                    &operations_research::routing::SearchStats::
                        num_glop_calls_in_lp_scheduling)
      .def_readonly(
          "num_min_cost_flow_calls",
          &operations_research::routing::SearchStats::num_min_cost_flow_calls)
      .def_readonly("num_cp_sat_calls_in_routing",
                    &operations_research::routing::SearchStats::
                        num_cp_sat_calls_in_routing)
      .def_readonly("num_generalized_cp_sat_calls_in_routing",
                    &operations_research::routing::SearchStats::
                        num_generalized_cp_sat_calls_in_routing);

  pybind11::class_<Model> rm(m, "Model",
                             DOC(operations_research, routing, Model));
  rm.def(pybind11::init([](const IndexManager& routing_index_manager) {
           return new Model(routing_index_manager);
         }),
         py::arg("routing_index_manager"),
         DOC(operations_research, routing, Model, RoutingModel));
  rm.def(pybind11::init([](const IndexManager& routing_index_manager,
                           const RoutingModelParameters& parameters) {
           return new Model(routing_index_manager, parameters);
         }),
         py::arg("routing_index_manager"), py::arg("parameters"),
         DOC(operations_research, routing, Model, RoutingModel));
  rm.def(
      "register_transit_matrix",
      [](Model* routing_model,
         std::vector<std::vector<int64_t>> transit_matrix) {
        return routing_model->RegisterTransitMatrix(std::move(transit_matrix));
      },
      DOC(operations_research, routing, Model, RegisterTransitMatrix));
  rm.def(
      "register_unary_transit_vector",
      [](Model* routing_model, std::vector<int64_t> transit_vector) {
        return routing_model->RegisterUnaryTransitVector(
            std::move(transit_vector));
      },
      DOC(operations_research, routing, Model, RegisterUnaryTransitVector));
  rm.def(
      "register_unary_transit_callback",
      [](Model* routing_model,
         std::function<int64_t(int64_t)> transit_callback) {
        return routing_model->RegisterUnaryTransitCallback(
            std::move(transit_callback));
      },
      DOC(operations_research, routing, Model, RegisterUnaryTransitCallback));
  rm.def(
      "register_transit_callback",
      [](Model* routing_model,
         std::function<int64_t(int64_t, int64_t)> transit_callback) {
        return routing_model->RegisterTransitCallback(
            std::move(transit_callback));
      },
      DOC(operations_research, routing, Model, RegisterTransitCallback));
  rm.def("set_arc_cost_evaluator_of_all_vehicles",
         &Model::SetArcCostEvaluatorOfAllVehicles,
         py::arg("transit_callback_index"),
         DOC(operations_research, routing, Model,
             SetArcCostEvaluatorOfAllVehicles));
  rm.def(
      "set_arc_cost_evaluator_of_vehicle", &Model::SetArcCostEvaluatorOfVehicle,
      py::arg("evaluator_index"), py::arg("vehicle"),
      DOC(operations_research, routing, Model, SetArcCostEvaluatorOfVehicle));
  rm.def("set_fixed_cost_of_all_vehicles", &Model::SetFixedCostOfAllVehicles,
         py::arg("cost"),
         DOC(operations_research, routing, Model, SetFixedCostOfAllVehicles));
  rm.def("set_fixed_cost_of_vehicle", &Model::SetFixedCostOfVehicle,
         py::arg("cost"), py::arg("vehicle"),
         DOC(operations_research, routing, Model, SetFixedCostOfVehicle));
  rm.def("get_fixed_cost_of_vehicle", &Model::GetFixedCostOfVehicle,
         py::arg("vehicle"),
         DOC(operations_research, routing, Model, GetFixedCostOfVehicle));
  rm.def("set_amortized_cost_factors_of_all_vehicles",
         &Model::SetAmortizedCostFactorsOfAllVehicles,
         py::arg("linear_cost_factor"), py::arg("quadratic_cost_factor"),
         DOC(operations_research, routing, Model,
             SetAmortizedCostFactorsOfAllVehicles));
  rm.def("set_amortized_cost_factors_of_vehicle",
         &Model::SetAmortizedCostFactorsOfVehicle,
         py::arg("linear_cost_factor"), py::arg("quadratic_cost_factor"),
         py::arg("vehicle"),
         DOC(operations_research, routing, Model,
             SetAmortizedCostFactorsOfVehicle));
  rm.def("get_amortized_linear_cost_factors_of_vehicles",
         &Model::GetAmortizedLinearCostFactorOfVehicles,
         DOC(operations_research, routing, Model,
             GetAmortizedLinearCostFactorOfVehicles));
  rm.def("get_amortized_quadratic_cost_factors_of_vehicles",
         &Model::GetAmortizedQuadraticCostFactorOfVehicles,
         DOC(operations_research, routing, Model,
             GetAmortizedQuadraticCostFactorOfVehicles));
  rm.def("add_dimension", &Model::AddDimension, py::arg("evaluator_index"),
         py::arg("slack_max"), py::arg("capacity"),
         py::arg("fix_start_cumul_to_zero"), py::arg("name"),
         DOC(operations_research, routing, Model, AddDimension));
  rm.def("add_dimension_with_vehicle_capacity",
         &Model::AddDimensionWithVehicleCapacity, py::arg("evaluator_index"),
         py::arg("slack_max"), py::arg("vehicle_capacities"),
         py::arg("fix_start_cumul_to_zero"), py::arg("name"),
         DOC(operations_research, routing, Model,
             AddDimensionWithVehicleCapacity));
  rm.def("add_dimension_with_vehicle_transits",
         &Model::AddDimensionWithVehicleTransits, py::arg("evaluator_indices"),
         py::arg("slack_max"), py::arg("capacity"),
         py::arg("fix_start_cumul_to_zero"), py::arg("name"),
         DOC(operations_research, routing, Model,
             AddDimensionWithVehicleTransits));
  rm.def("add_dimension_with_vehicle_transit_and_capacity",
         &Model::AddDimensionWithVehicleTransitAndCapacity,
         py::arg("evaluator_indices"), py::arg("slack_max"),
         py::arg("vehicle_capacities"), py::arg("fix_start_cumul_to_zero"),
         py::arg("name"),
         DOC(operations_research, routing, Model,
             AddDimensionWithVehicleTransitAndCapacity));
  rm.def("add_constant_dimension", &Model::AddConstantDimension,
         py::arg("value"), py::arg("capacity"),
         py::arg("fix_start_cumul_to_zero"), py::arg("name"),
         DOC(operations_research, routing, Model, AddConstantDimension));
  rm.def(
      "add_constant_dimension_with_slack",
      &Model::AddConstantDimensionWithSlack, py::arg("value"),
      py::arg("slack_max"), py::arg("capacity"),
      py::arg("fix_start_cumul_to_zero"), py::arg("name"),
      DOC(operations_research, routing, Model, AddConstantDimensionWithSlack));
  rm.def("add_vector_dimension", &Model::AddVectorDimension, py::arg("values"),
         py::arg("capacity"), py::arg("fix_start_cumul_to_zero"),
         py::arg("name"),
         DOC(operations_research, routing, Model, AddVectorDimension));
  rm.def("add_matrix_dimension", &Model::AddMatrixDimension, py::arg("values"),
         py::arg("capacity"), py::arg("fix_start_cumul_to_zero"),
         py::arg("name"),
         DOC(operations_research, routing, Model, AddMatrixDimension));
  rm.def("get_dimension_or_die", &Model::GetDimensionOrDie,
         pybind11::return_value_policy::reference_internal,
         py::arg("dimension_name"),
         DOC(operations_research, routing, Model, GetDimensionOrDie));
  rm.def("close_model", &Model::CloseModel,
         DOC(operations_research, routing, Model, CloseModel));
  rm.def("close_model_with_parameters", &Model::CloseModelWithParameters,
         py::arg("search_parameters"),
         DOC(operations_research, routing, Model, CloseModelWithParameters));
  rm.def("add_to_assignment", &Model::AddToAssignment, py::arg("var"),
         DOC(operations_research, routing, Model, AddToAssignment));
  rm.def("add_interval_to_assignment", &Model::AddIntervalToAssignment,
         py::arg("interval"),
         DOC(operations_research, routing, Model, AddIntervalToAssignment));
  rm.def("solve", &Model::Solve,
         pybind11::return_value_policy::reference_internal,
         py::arg("assignment") = nullptr,
         DOC(operations_research, routing, Model, Solve));
  // TODO(user) Add support for solutions parameters too.
  rm.def(
      "solve_from_assignment_with_parameters",
      [](Model* routing_model, Assignment* assignment,
         const RoutingSearchParameters& search_parameters)
          -> const Assignment* {
        return routing_model->SolveFromAssignmentWithParameters(
            assignment, search_parameters);
      },
      pybind11::return_value_policy::reference_internal, py::arg("assignment"),
      py::arg("search_parameters"),
      DOC(operations_research, routing, Model,
          SolveFromAssignmentWithParameters));
  rm.def(
      "solve_with_parameters",
      [](Model* routing_model, const RoutingSearchParameters& search_parameters
         /*,std::vector<const Assignment*>* solutions = nullptr*/)
          -> const Assignment* {
        return routing_model->SolveWithParameters(search_parameters, nullptr);
      },
      pybind11::return_value_policy::reference_internal,
      py::arg("search_parameters"),
      DOC(operations_research, routing, Model, SolveWithParameters));
  rm.def("status", &Model::status,
         DOC(operations_research, routing, Model, status));
  rm.def("nodes", &Model::nodes,
         DOC(operations_research, routing, Model, nodes));
  rm.def("vehicles", &Model::vehicles,
         DOC(operations_research, routing, Model, vehicles));
  rm.def("size", &Model::Size, DOC(operations_research, routing, Model, Size));
  rm.def("start", &Model::Start, py::arg("vehicle"),
         DOC(operations_research, routing, Model, Start));
  rm.def("end", &Model::End, py::arg("vehicle"),
         DOC(operations_research, routing, Model, End));
  rm.def("is_start", &Model::IsStart, py::arg("index"),
         DOC(operations_research, routing, Model, IsStart));
  rm.def("is_end", &Model::IsEnd, py::arg("index"),
         DOC(operations_research, routing, Model, IsEnd));
  rm.def("next", &Model::Next, py::arg("assignment"), py::arg("index"),
         DOC(operations_research, routing, Model, Next));
  rm.def("next_var", &Model::NextVar,
         pybind11::return_value_policy::reference_internal, py::arg("index"),
         DOC(operations_research, routing, Model, NextVar));
  rm.def("get_arc_cost_for_vehicle", &Model::GetArcCostForVehicle,
         py::arg("from_index"), py::arg("to_index"), py::arg("vehicle"),
         DOC(operations_research, routing, Model, GetArcCostForVehicle));
  rm.def_property_readonly("solver", &Model::solver,
                           pybind11::return_value_policy::reference_internal,
                           DOC(operations_research, routing, Model, solver));

  pybind11::enum_<Model::PenaltyCostBehavior>(rm, "PenaltyCostBehavior")
      .value("PENALIZE_ONCE", Model::PenaltyCostBehavior::PENALIZE_ONCE)
      .value("PENALIZE_PER_INACTIVE",
             Model::PenaltyCostBehavior::PENALIZE_PER_INACTIVE)
      .export_values();

  pybind11::enum_<Model::PickupAndDeliveryPolicy>(rm, "PickupAndDeliveryPolicy")
      .value("PICKUP_AND_DELIVERY_NO_ORDER",
             Model::PickupAndDeliveryPolicy::PICKUP_AND_DELIVERY_NO_ORDER)
      .value("PICKUP_AND_DELIVERY_LIFO",
             Model::PickupAndDeliveryPolicy::PICKUP_AND_DELIVERY_LIFO)
      .value("PICKUP_AND_DELIVERY_FIFO",
             Model::PickupAndDeliveryPolicy::PICKUP_AND_DELIVERY_FIFO)
      .export_values();

  pybind11::enum_<Model::VisitTypePolicy>(rm, "VisitTypePolicy")
      .value("TYPE_ADDED_TO_VEHICLE",
             Model::VisitTypePolicy::TYPE_ADDED_TO_VEHICLE)
      .value("ADDED_TYPE_REMOVED_FROM_VEHICLE",
             Model::VisitTypePolicy::ADDED_TYPE_REMOVED_FROM_VEHICLE)
      .value("TYPE_ON_VEHICLE_UP_TO_VISIT",
             Model::VisitTypePolicy::TYPE_ON_VEHICLE_UP_TO_VISIT)
      .value("TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED",
             Model::VisitTypePolicy::TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED)
      .export_values();

  pybind11::class_<Model::PickupDeliveryPosition>(
      rm, "PickupDeliveryPosition",
      DOC(operations_research, routing, Model, PickupDeliveryPosition))
      .def_readonly("pd_pair_index",
                    &Model::PickupDeliveryPosition::pd_pair_index,
                    DOC(operations_research, routing, Model,
                        PickupDeliveryPosition, pd_pair_index))
      .def_readonly("alternative_index",
                    &Model::PickupDeliveryPosition::alternative_index,
                    DOC(operations_research, routing, Model,
                        PickupDeliveryPosition, alternative_index));

  rm.def(
      "set_allowed_vehicles_for_index",
      [](Model* model, const std::vector<int>& vehicles, int64_t index) {
        model->SetAllowedVehiclesForIndex(vehicles, index);
      },
      py::arg("vehicles"), py::arg("index"),
      DOC(operations_research, routing, Model, SetAllowedVehiclesForIndex));
  rm.def("is_vehicle_allowed_for_index", &Model::IsVehicleAllowedForIndex,
         py::arg("vehicle"), py::arg("index"),
         DOC(operations_research, routing, Model, IsVehicleAllowedForIndex));
  rm.def(
      "add_soft_same_vehicle_constraint", &Model::AddSoftSameVehicleConstraint,
      py::arg("indices"), py::arg("cost"),
      DOC(operations_research, routing, Model, AddSoftSameVehicleConstraint));

  rm.def(
      "add_disjunction",
      [](Model* routing_model, const std::vector<int64_t>& indices,
         int64_t penalty, int64_t max_cardinality,
         Model::PenaltyCostBehavior penalty_cost_behavior) -> int {
        return routing_model
            ->AddDisjunction(indices, penalty, max_cardinality,
                             penalty_cost_behavior)
            .value();
      },
      py::arg("indices"), py::arg("penalty") = Model::kNoPenalty,
      py::arg("max_cardinality") = 1,
      py::arg("penalty_cost_behavior") =
          Model::PenaltyCostBehavior::PENALIZE_ONCE,
      DOC(operations_research, routing, Model, AddDisjunction));
  rm.def("add_pickup_and_delivery", &Model::AddPickupAndDelivery,
         py::arg("pickup"), py::arg("delivery"),
         DOC(operations_research, routing, Model, AddPickupAndDelivery));
  rm.def(
      "add_pickup_and_delivery_sets",
      [](Model* model, int pickup_disjunction, int delivery_disjunction) {
        model->AddPickupAndDeliverySets(
            operations_research::routing::DisjunctionIndex(pickup_disjunction),
            operations_research::routing::DisjunctionIndex(
                delivery_disjunction));
      },
      py::arg("pickup_disjunction"), py::arg("delivery_disjunction"),
      DOC(operations_research, routing, Model, AddPickupAndDeliverySets));
  rm.def("get_pickup_position", &Model::GetPickupPosition,
         py::arg("node_index"));  // Missing doc.
  rm.def("get_delivery_position", &Model::GetDeliveryPosition,
         py::arg("node_index"));  // Missing doc.
  rm.def("is_pickup", &Model::IsPickup, py::arg("node_index"),
         DOC(operations_research, routing, Model, IsPickup));
  rm.def("is_delivery", &Model::IsDelivery, py::arg("node_index"),
         DOC(operations_research, routing, Model, IsDelivery));
  rm.def("unperformed_penalty", &Model::UnperformedPenalty,
         py::arg("var_index"),
         DOC(operations_research, routing, Model, UnperformedPenalty));
  rm.def("unperformed_penalty_or_value", &Model::UnperformedPenaltyOrValue,
         py::arg("default_value"), py::arg("var_index"),
         DOC(operations_research, routing, Model, UnperformedPenaltyOrValue));
  rm.def("get_depot", &Model::GetDepot,
         DOC(operations_research, routing, Model, GetDepot));
  rm.def("set_maximum_number_of_active_vehicles",
         &Model::SetMaximumNumberOfActiveVehicles,
         py::arg("max_active_vehicles"),
         DOC(operations_research, routing, Model,
             SetMaximumNumberOfActiveVehicles));
  rm.def("get_maximum_number_of_active_vehicles",
         &Model::GetMaximumNumberOfActiveVehicles,
         DOC(operations_research, routing, Model,
             GetMaximumNumberOfActiveVehicles));
  rm.def("set_visit_type", &Model::SetVisitType, py::arg("index"),
         py::arg("type"), py::arg("type_policy"),
         DOC(operations_research, routing, Model, SetVisitType));
  rm.def("get_visit_type", &Model::GetVisitType, py::arg("index"),
         DOC(operations_research, routing, Model, GetVisitType));
  rm.def("get_visit_type_policy", &Model::GetVisitTypePolicy, py::arg("index"),
         DOC(operations_research, routing, Model, GetVisitTypePolicy));
  rm.def("add_hard_type_incompatibility", &Model::AddHardTypeIncompatibility,
         py::arg("type1"), py::arg("type2"),
         DOC(operations_research, routing, Model, AddHardTypeIncompatibility));
  rm.def("get_hard_type_incompatibilities_of_type",
         &Model::GetHardTypeIncompatibilitiesOfType, py::arg("type"),
         DOC(operations_research, routing, Model,
             GetHardTypeIncompatibilitiesOfType));
  rm.def(
      "add_temporal_type_incompatibility",
      &Model::AddTemporalTypeIncompatibility, py::arg("type1"),
      py::arg("type2"),
      DOC(operations_research, routing, Model, AddTemporalTypeIncompatibility));
  rm.def("get_temporal_type_incompatibilities_of_type",
         &Model::GetTemporalTypeIncompatibilitiesOfType, py::arg("type"),
         DOC(operations_research, routing, Model,
             GetTemporalTypeIncompatibilitiesOfType));
  rm.def(
      "add_required_type_alternatives_when_adding_type",
      [](Model* model, int dependent_type,
         const absl::flat_hash_set<int>& required_type_alternatives) {
        model->AddRequiredTypeAlternativesWhenAddingType(
            dependent_type, required_type_alternatives);
      },
      py::arg("dependent_type"), py::arg("required_type_alternatives"),
      DOC(operations_research, routing, Model,
          AddRequiredTypeAlternativesWhenAddingType));
  rm.def(
      "add_required_type_alternatives_when_removing_type",
      [](Model* model, int dependent_type,
         const absl::flat_hash_set<int>& required_type_alternatives) {
        model->AddRequiredTypeAlternativesWhenRemovingType(
            dependent_type, required_type_alternatives);
      },
      py::arg("dependent_type"), py::arg("required_type_alternatives"),
      DOC(operations_research, routing, Model,
          AddRequiredTypeAlternativesWhenRemovingType));
  rm.def(
      "add_same_vehicle_required_type_alternatives",
      [](Model* model, int dependent_type,
         const absl::flat_hash_set<int>& required_type_alternatives) {
        model->AddSameVehicleRequiredTypeAlternatives(
            dependent_type, required_type_alternatives);
      },
      py::arg("dependent_type"), py::arg("required_type_alternatives"),
      DOC(operations_research, routing, Model,
          AddSameVehicleRequiredTypeAlternatives));
  rm.def("get_required_type_alternatives_when_adding_type",
         &Model::GetRequiredTypeAlternativesWhenAddingType, py::arg("type"));
  rm.def("get_required_type_alternatives_when_removing_type",
         &Model::GetRequiredTypeAlternativesWhenRemovingType, py::arg("type"));
  rm.def("get_same_vehicle_required_type_alternatives_of_type",
         &Model::GetSameVehicleRequiredTypeAlternativesOfType, py::arg("type"));
  rm.def("set_pickup_and_delivery_policy_of_vehicle",
         &Model::SetPickupAndDeliveryPolicyOfVehicle, py::arg("policy"),
         py::arg("vehicle"),
         DOC(operations_research, routing, Model,
             SetPickupAndDeliveryPolicyOfVehicle));
  rm.def("set_pickup_and_delivery_policy_of_all_vehicles",
         &Model::SetPickupAndDeliveryPolicyOfAllVehicles, py::arg("policy"),
         DOC(operations_research, routing, Model,
             SetPickupAndDeliveryPolicyOfAllVehicles));
  rm.def("get_pickup_and_delivery_policy_of_vehicle",
         &Model::GetPickupAndDeliveryPolicyOfVehicle, py::arg("vehicle"),
         DOC(operations_research, routing, Model,
             GetPickupAndDeliveryPolicyOfVehicle));
  rm.def("get_num_of_singleton_nodes", &Model::GetNumOfSingletonNodes,
         DOC(operations_research, routing, Model, GetNumOfSingletonNodes));
  rm.def("set_path_energy_cost_of_vehicle", &Model::SetPathEnergyCostOfVehicle,
         py::arg("force"), py::arg("distance"), py::arg("cost_per_unit"),
         py::arg("vehicle"),
         DOC(operations_research, routing, Model, SetPathEnergyCostOfVehicle));

  rm.def("read_assignment", &Model::ReadAssignment, py::arg("file_name"),
         pybind11::return_value_policy::take_ownership,
         DOC(operations_research, routing, Model, ReadAssignment));
  rm.def("write_assignment", &Model::WriteAssignment, py::arg("file_name"),
         DOC(operations_research, routing, Model, WriteAssignment));
  rm.def("restore_assignment", &Model::RestoreAssignment, py::arg("solution"),
         pybind11::return_value_policy::take_ownership,
         DOC(operations_research, routing, Model, RestoreAssignment));
  rm.def("read_assignment_from_routes", &Model::ReadAssignmentFromRoutes,
         py::arg("routes"), py::arg("ignore_inactive_indices"),
         pybind11::return_value_policy::reference, py::keep_alive<0, 1>(),
         DOC(operations_research, routing, Model, ReadAssignmentFromRoutes));
  rm.def("routes_to_assignment", &Model::RoutesToAssignment, py::arg("routes"),
         py::arg("ignore_inactive_indices"), py::arg("close_routes"),
         py::arg("assignment"),
         DOC(operations_research, routing, Model, RoutesToAssignment));
  rm.def("compact_assignment", &Model::CompactAssignment, py::arg("assignment"),
         pybind11::return_value_policy::take_ownership,
         DOC(operations_research, routing, Model, CompactAssignment));
  rm.def("compute_lower_bound", &Model::ComputeLowerBound,
         DOC(operations_research, routing, Model, ComputeLowerBound));
  rm.def("is_vehicle_used", &Model::IsVehicleUsed, py::arg("assignment"),
         py::arg("vehicle"),
         DOC(operations_research, routing, Model, IsVehicleUsed));

  // Resource Group
  pybind11::class_<Model::ResourceGroup> resource_group(
      rm, "ResourceGroup",
      DOC(operations_research, routing, Model, ResourceGroup));
  pybind11::class_<Model::ResourceGroup::Attributes>(
      resource_group, "Attributes",
      DOC(operations_research, routing, Model, ResourceGroup, Attributes))
      .def(pybind11::init<>(), DOC(operations_research, routing, Model,
                                   ResourceGroup, Attributes, Attributes))
      .def(pybind11::init<operations_research::Domain,
                          operations_research::Domain>(),
           py::arg("start_domain"), py::arg("end_domain"),
           DOC(operations_research, routing, Model, ResourceGroup, Attributes,
               Attributes, 2));

  resource_group.def(
      "add_resource", &Model::ResourceGroup::AddResource, py::arg("attributes"),
      py::arg("dimension"),
      DOC(operations_research, routing, Model, ResourceGroup, AddResource));
  resource_group.def("notify_vehicle_requires_a_resource",
                     &Model::ResourceGroup::NotifyVehicleRequiresAResource,
                     py::arg("vehicle"),
                     DOC(operations_research, routing, Model, ResourceGroup,
                         NotifyVehicleRequiresAResource));
  resource_group.def("get_vehicles_requiring_a_resource",
                     &Model::ResourceGroup::GetVehiclesRequiringAResource,
                     DOC(operations_research, routing, Model, ResourceGroup,
                         GetVehiclesRequiringAResource));
  resource_group.def("vehicle_requires_a_resource",
                     &Model::ResourceGroup::VehicleRequiresAResource,
                     py::arg("vehicle"),
                     DOC(operations_research, routing, Model, ResourceGroup,
                         VehicleRequiresAResource));
  resource_group.def("set_allowed_resources_for_vehicle",
                     &Model::ResourceGroup::SetAllowedResourcesForVehicle,
                     py::arg("vehicle"), py::arg("allowed_resource_indices"),
                     DOC(operations_research, routing, Model, ResourceGroup,
                         SetAllowedResourcesForVehicle));
  resource_group.def("clear_allowed_resources_for_vehicle",
                     &Model::ResourceGroup::ClearAllowedResourcesForVehicle,
                     py::arg("vehicle"),
                     DOC(operations_research, routing, Model, ResourceGroup,
                         ClearAllowedResourcesForVehicle));
  resource_group.def("is_resource_allowed_for_vehicle",
                     &Model::ResourceGroup::IsResourceAllowedForVehicle,
                     py::arg("resource"), py::arg("vehicle"),
                     DOC(operations_research, routing, Model, ResourceGroup,
                         IsResourceAllowedForVehicle));
  resource_group.def(
      "size", &Model::ResourceGroup::Size,
      DOC(operations_research, routing, Model, ResourceGroup, Size));
  resource_group.def(
      "index", &Model::ResourceGroup::Index,
      DOC(operations_research, routing, Model, ResourceGroup, Index));

  rm.def("add_resource_group", &Model::AddResourceGroup,
         pybind11::return_value_policy::reference_internal,
         DOC(operations_research, routing, Model, AddResourceGroup));
  rm.def("get_resource_group", &Model::GetResourceGroup,
         pybind11::return_value_policy::reference_internal, py::arg("rg_index"),
         DOC(operations_research, routing, Model, GetResourceGroup));
  rm.def("get_dimension_resource_group_indices",
         &Model::GetDimensionResourceGroupIndices, py::arg("dimension"),
         DOC(operations_research, routing, Model,
             GetDimensionResourceGroupIndices));
  rm.def(
      "get_dimension_resource_group_index",
      &Model::GetDimensionResourceGroupIndex, py::arg("dimension"),
      DOC(operations_research, routing, Model, GetDimensionResourceGroupIndex));

  rm.def(
      "add_route_constraint",
      [](Model* model,
         std::function<std::optional<int64_t>(const std::vector<int64_t>&)>
             route_evaluator,
         bool costs_are_homogeneous_across_vehicles) {
        model->AddRouteConstraint(std::move(route_evaluator),
                                  costs_are_homogeneous_across_vehicles);
      },
      py::arg("route_evaluator"),
      py::arg("costs_are_homogeneous_across_vehicles") = false);

  rm.def("get_route_cost", &Model::GetRouteCost,
         py::arg("route"));  // missing doc. NOLINT

  rm.def("set_first_solution_evaluator", &Model::SetFirstSolutionEvaluator,
         py::arg("evaluator"),
         DOC(operations_research, routing, Model, SetFirstSolutionEvaluator));
  rm.def("set_first_solution_hint", &Model::SetFirstSolutionHint,
         py::arg("hint"));
  rm.def("get_first_solution_hint", &Model::GetFirstSolutionHint,
         pybind11::return_value_policy::reference_internal);

  rm.def("add_search_monitor", &Model::AddSearchMonitor, py::arg("monitor"),
         DOC(operations_research, routing, Model, AddSearchMonitor));
  rm.def("add_at_solution_callback", &Model::AddAtSolutionCallback,
         py::arg("callback"), py::arg("track_unchecked_neighbors") = false,
         DOC(operations_research, routing, Model, AddAtSolutionCallback));
  rm.def("add_enter_search_callback", &Model::AddEnterSearchCallback,
         py::arg("callback"));

  rm.def("add_variable_minimized_by_finalizer",
         &Model::AddVariableMinimizedByFinalizer, py::arg("var"),
         DOC(operations_research, routing, Model,
             AddVariableMinimizedByFinalizer));
  rm.def("add_variable_maximized_by_finalizer",
         &Model::AddVariableMaximizedByFinalizer, py::arg("var"),
         DOC(operations_research, routing, Model,
             AddVariableMaximizedByFinalizer));
  rm.def("add_weighted_variable_minimized_by_finalizer",
         &Model::AddWeightedVariableMinimizedByFinalizer, py::arg("var"),
         py::arg("cost"),
         DOC(operations_research, routing, Model,
             AddWeightedVariableMinimizedByFinalizer));
  rm.def("add_weighted_variable_maximized_by_finalizer",
         &Model::AddWeightedVariableMaximizedByFinalizer, py::arg("var"),
         py::arg("cost"),
         DOC(operations_research, routing, Model,
             AddWeightedVariableMaximizedByFinalizer));
  rm.def(
      "add_variable_target_to_finalizer", &Model::AddVariableTargetToFinalizer,
      py::arg("var"), py::arg("target"),
      DOC(operations_research, routing, Model, AddVariableTargetToFinalizer));
  rm.def("add_weighted_variable_target_to_finalizer",
         &Model::AddWeightedVariableTargetToFinalizer, py::arg("var"),
         py::arg("target"), py::arg("cost"),
         DOC(operations_research, routing, Model,
             AddWeightedVariableTargetToFinalizer));

  rm.def("vehicle_var", &Model::VehicleVar,
         pybind11::return_value_policy::reference_internal, py::arg("index"),
         DOC(operations_research, routing, Model, VehicleVar));
  rm.def("active_var", &Model::ActiveVar,
         pybind11::return_value_policy::reference_internal, py::arg("index"),
         DOC(operations_research, routing, Model, ActiveVar));
  rm.def("cost_var", &Model::CostVar,
         pybind11::return_value_policy::reference_internal,
         DOC(operations_research, routing, Model, CostVar));
  rm.def("active_vehicle_var", &Model::ActiveVehicleVar,
         pybind11::return_value_policy::reference_internal, py::arg("vehicle"),
         DOC(operations_research, routing, Model, ActiveVehicleVar));
  rm.def("vehicle_route_considered_var", &Model::VehicleRouteConsideredVar,
         pybind11::return_value_policy::reference_internal, py::arg("vehicle"),
         DOC(operations_research, routing, Model, VehicleRouteConsideredVar));

  rm.def("add_local_search_operator", &Model::AddLocalSearchOperator,
         py::arg("ls_operator"),
         DOC(operations_research, routing, Model, AddLocalSearchOperator));
  rm.def("add_local_search_filter", &Model::AddLocalSearchFilter,
         py::arg("filter"),
         DOC(operations_research, routing, Model, AddLocalSearchFilter));
  rm.def(
      "apply_locks",
      [](Model* model, const std::vector<int64_t>& locks) {
        return model->ApplyLocks(locks);
      },
      pybind11::return_value_policy::reference_internal, py::arg("locks"),
      DOC(operations_research, routing, Model, ApplyLocks));
  rm.def("apply_locks_to_all_vehicles", &Model::ApplyLocksToAllVehicles,
         py::arg("locks"), py::arg("close_routes"),
         DOC(operations_research, routing, Model, ApplyLocksToAllVehicles));
  rm.def("get_sub_solver_statistics", &Model::GetSubSolverStatistics);
  rm.def("search_stats", &Model::search_stats,
         pybind11::return_value_policy::reference_internal);
  rm.def(
      "get_cost_class_index_of_vehicle",
      [](const Model& model, int64_t vehicle) {
        return model.GetCostClassIndexOfVehicle(vehicle).value();
      },
      py::arg("vehicle"),
      DOC(operations_research, routing, Model, GetCostClassIndexOfVehicle));
  rm.def(
      "get_vehicle_class_index_of_vehicle",
      [](const Model& model, int64_t vehicle) {
        return model.GetVehicleClassIndexOfVehicle(vehicle).value();
      },
      py::arg("vehicle"),
      DOC(operations_research, routing, Model, GetVehicleClassIndexOfVehicle));
  rm.def(
      "get_vehicle_of_class",
      [](const Model& model, int vehicle_class) {
        return model.GetVehicleOfClass(
            operations_research::routing::VehicleClassIndex(vehicle_class));
      },
      py::arg("vehicle_class"),
      DOC(operations_research, routing, Model, GetVehicleOfClass));
  rm.def("get_vehicle_classes_count", &Model::GetVehicleClassesCount,
         DOC(operations_research, routing, Model, GetVehicleClassesCount));
  rm.def("get_cost_classes_count", &Model::GetCostClassesCount,
         DOC(operations_research, routing, Model, GetCostClassesCount));
  rm.def("get_non_zero_cost_classes_count", &Model::GetNonZeroCostClassesCount,
         DOC(operations_research, routing, Model, GetNonZeroCostClassesCount));
}  // NOLINT(readability/fn_size)
