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

#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/die_if_null.h"
#include "absl/log/initialize.h"
#include "absl/strings/str_format.h"
#include "ortools/sat/python/wrappers.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research::sat::python {

void ParseAndGenerate() {
  absl::PrintF(
      R"(

// This is a generated file, do not edit.
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "google/protobuf/text_format.h"
#include "pybind11/numpy.h"
#include "pybind11/pybind11.h"
#include "pybind11/pytypes.h"
#include "pybind11/stl.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace py = ::pybind11;
namespace operations_research::sat::python {
PYBIND11_MODULE(sat_parameters_builder, py_module) {
%s
}  // PYBIND11_MODULE
}  // namespace operations_research::sat::python
)",
      GeneratePybindCode({ABSL_DIE_IF_NULL(SatParameters::descriptor())}));
}

}  // namespace operations_research::sat::python

int main(int argc, char* argv[]) {
  // We do not use InitGoogle() to avoid linking with or-tools as this would
  // create a circular dependency.
  absl::InitializeLog();
  absl::SetProgramUsageMessage(argv[0]);
  absl::ParseCommandLine(argc, argv);
  operations_research::sat::python::ParseAndGenerate();
  return 0;
}
