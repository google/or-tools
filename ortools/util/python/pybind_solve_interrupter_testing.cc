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

#include "ortools/util/python/py_solve_interrupter_testing.h"

namespace operations_research {

namespace py = ::pybind11;

PYBIND11_MODULE(pybind_solve_interrupter_testing, m) {
  py::class_<PySolveInterrupterReference>(m, "PySolveInterrupterReference")
      .def(py::init<std::shared_ptr<PySolveInterrupter>>())
      .def_property_readonly("use_count",
                             &PySolveInterrupterReference::use_count)
      .def_property_readonly("is_interrupted",
                             &PySolveInterrupterReference::is_interrupted);
  m.def("IsInterrupted", &IsInterrupted, py::arg("interrupter"));
}

}  // namespace operations_research
