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

#include "ortools/util/piecewise_linear_function.h"

#include "pybind11/pybind11.h"

namespace py = pybind11;
using ::operations_research::PiecewiseLinearFunction;

PYBIND11_MODULE(piecewise_linear_function, m) {
  py::class_<operations_research::PiecewiseLinearFunction>(
      m, "PiecewiseLinearFunction")
      .def_static("create_piecewise_linear_function",
                  &PiecewiseLinearFunction::CreatePiecewiseLinearFunction,
                  py::arg("points_x"), py::arg("points_y"), py::arg("slopes"),
                  py::arg("other_points_x"),
                  py::return_value_policy::take_ownership)
      .def_static(
          "create_step_function", &PiecewiseLinearFunction::CreateStepFunction,
          py::arg("points_x"), py::arg("points_y"), py::arg("other_points_x"),
          py::return_value_policy::take_ownership)
      .def_static("create_full_domain_function",
                  &PiecewiseLinearFunction::CreateFullDomainFunction,
                  py::arg("initial_level"), py::arg("points_x"),
                  py::arg("slopes"), py::return_value_policy::take_ownership)
      .def_static("create_one_segment_function",
                  &PiecewiseLinearFunction::CreateOneSegmentFunction,
                  py::arg("point_x"), py::arg("point_y"), py::arg("slope"),
                  py::arg("other_point_x"),
                  py::return_value_policy::take_ownership)
      .def_static("create_right_ray_function",
                  &PiecewiseLinearFunction::CreateRightRayFunction,
                  py::arg("point_x"), py::arg("point_y"), py::arg("slope"),
                  py::return_value_policy::take_ownership)
      .def_static("create_left_ray_function",
                  &PiecewiseLinearFunction::CreateLeftRayFunction,
                  py::arg("point_x"), py::arg("point_y"), py::arg("slope"),
                  py::return_value_policy::take_ownership)
      .def_static("create_fixed_charge_function",
                  &PiecewiseLinearFunction::CreateFixedChargeFunction,
                  py::arg("slope"), py::arg("value"),
                  py::return_value_policy::take_ownership)
      .def_static("create_early_tardy_function",
                  &PiecewiseLinearFunction::CreateEarlyTardyFunction,
                  py::arg("reference"), py::arg("earliness_slope"),
                  py::arg("tardiness_slope"),
                  py::return_value_policy::take_ownership)
      .def_static("create_early_tardy_function_with_slack",
                  &PiecewiseLinearFunction::CreateEarlyTardyFunctionWithSlack,
                  py::arg("early_slack"), py::arg("late_slack"),
                  py::arg("earliness_slope"), py::arg("tardiness_slope"),
                  py::return_value_policy::take_ownership)
      .def("in_domain", &PiecewiseLinearFunction::InDomain, py::arg("x"))
      .def("is_convex", &PiecewiseLinearFunction::IsConvex)
      .def("is_non_decreasing", &PiecewiseLinearFunction::IsNonDecreasing)
      .def("is_non_increasing", &PiecewiseLinearFunction::IsNonIncreasing)
      .def("value", &PiecewiseLinearFunction::Value, py::arg("x"))
      .def("__str__", &PiecewiseLinearFunction::DebugString);
}
