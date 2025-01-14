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

// A pybind11 wrapper for model_builder_helper.

#include "ortools/scheduling/rcpsp_parser.h"
#include "pybind11/pybind11.h"
#include "pybind11/pytypes.h"
#include "pybind11_protobuf/native_proto_caster.h"

using ::operations_research::scheduling::rcpsp::RcpspParser;

namespace py = pybind11;
using ::py::arg;

PYBIND11_MODULE(rcpsp, m) {
  pybind11_protobuf::ImportNativeProtoCasters();

  py::class_<RcpspParser>(m, "RcpspParser")
      .def(py::init<>())
      .def("problem", &RcpspParser::problem)
      .def("parse_file", &RcpspParser::ParseFile, arg("file_name"));
}
