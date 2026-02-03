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

#include <memory>
#include <string>

#include "absl/strings/str_join.h"     // IWYU pragma: keep
#include "ortools/port/proto_utils.h"  // IWYU: keep
#include "ortools/util/testdata/wrappers_test.pb.h"
#include "pybind11/pybind11.h"

namespace py = pybind11;

namespace operations_research::util::python {

PYBIND11_MODULE(wrappers_test_extension, m) {
#define IMPORT_PROTO_WRAPPER_CODE
#include "ortools/util/python/wrappers_test_pybind11.h"
#undef IMPORT_PROTO_WRAPPER_CODE
}

}  // namespace operations_research::util::python
