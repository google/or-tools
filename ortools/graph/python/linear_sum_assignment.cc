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

#include "ortools/graph/assignment.h"
#include "pybind11/numpy.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

using ::operations_research::SimpleLinearSumAssignment;
using NodeIndex = ::operations_research::SimpleLinearSumAssignment::NodeIndex;
using ::pybind11::arg;

PYBIND11_MODULE(linear_sum_assignment, m) {
  pybind11::class_<SimpleLinearSumAssignment> slsa(m,
                                                   "SimpleLinearSumAssignment");
  slsa.def(pybind11::init<>());
  slsa.def("add_arc_with_cost", &SimpleLinearSumAssignment::AddArcWithCost,
           arg("left_node"), arg("right_node"), arg("cost"));
  slsa.def("add_arcs_with_cost",
           pybind11::vectorize(&SimpleLinearSumAssignment::AddArcWithCost));
  slsa.def("num_nodes", &SimpleLinearSumAssignment::NumNodes);
  slsa.def("num_arcs", &SimpleLinearSumAssignment::NumArcs);
  slsa.def("left_node", &SimpleLinearSumAssignment::LeftNode, arg("arc"));
  slsa.def("right_node", &SimpleLinearSumAssignment::RightNode, arg("arc"));
  slsa.def("cost", &SimpleLinearSumAssignment::Cost, arg("arc"));
  slsa.def("solve", &SimpleLinearSumAssignment::Solve);
  slsa.def("optimal_cost", &SimpleLinearSumAssignment::OptimalCost);
  slsa.def("right_mate", &SimpleLinearSumAssignment::RightMate,
           arg("left_node"));
  slsa.def("assignment_cost", &SimpleLinearSumAssignment::AssignmentCost,
           arg("left_node"));

  pybind11::enum_<SimpleLinearSumAssignment::Status>(slsa, "Status")
      .value("OPTIMAL", SimpleLinearSumAssignment::Status::OPTIMAL)
      .value("INFEASIBLE", SimpleLinearSumAssignment::Status::INFEASIBLE)
      .value("POSSIBLE_OVERFLOW",
             SimpleLinearSumAssignment::Status::POSSIBLE_OVERFLOW)
      .export_values();
}
