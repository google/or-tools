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

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "ortools/util/python/py_solve_interrupter.h"

namespace operations_research {

namespace py = ::pybind11;

PYBIND11_MODULE(pybind_solve_interrupter, m) {
  py::class_<PySolveInterrupter, std::shared_ptr<PySolveInterrupter>>(
      m, "PySolveInterrupter")
      .def(py::init())
      .def("interrupt", &PySolveInterrupter::Interrupt)
      .def_property_readonly("interrupted", &PySolveInterrupter::IsInterrupted)
      .def("add_trigger_target", &PySolveInterrupter::AddTriggerTarget,
           py::arg("target"))
      .def("remove_trigger_target", &PySolveInterrupter::RemoveTriggerTarget,
           py::arg("target"))
      .def("add_interruption_callback",
           &PySolveInterrupter::AddInterruptionCallback, py::arg("callback"))
      .def("remove_interruption_callback",
           &PySolveInterrupter::RemoveInterruptionCallback,
           py::arg("callback_id"));
}

}  // namespace operations_research
