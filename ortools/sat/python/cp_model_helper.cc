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

#include <Python.h>

#if PY_VERSION_HEX >= 0x030E00A7 && !defined(PYPY_VERSION)
#define Py_BUILD_CORE
#include "internal/pycore_frame.h"
#include "internal/pycore_interpframe.h"
#undef Py_BUILD_CORE
#endif

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#define RunningOnValgrind AbslRunningOnValgrind
#define ValgrindSlowdown AbslValgrindSlowdown
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/check.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#undef RunningOnValgrind
#undef ValgrindSlowdown
#include "ortools/port/proto_utils.h"  // IWYU: keep
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/python/pybind_constraint.h"
#include "ortools/sat/python/pybind_linearexpr.h"
#include "ortools/sat/python/pybind_solver.h"
#include "ortools/sat/sat_parameters.pb.h"  // IWYU: keep
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;

namespace operations_research::sat::python {

PYBIND11_MODULE(cp_model_helper, m) {
  py::module::import("ortools.util.python.sorted_interval_list");

  DefinePybindWrapperForLinearExpr(m);
  DefinePybindWrapperForConstraints(m);
  DefinePybindWrapperForSolver(m);

#define IMPORT_PROTO_WRAPPER_CODE
#include "ortools/sat/python/proto_builder_pybind11.h"
#undef IMPORT_PROTO_WRAPPER_CODE
}  // NOLINT(readability/fn_size)

}  // namespace operations_research::sat::python
