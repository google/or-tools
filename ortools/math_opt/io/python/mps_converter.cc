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

#include "ortools/math_opt/io/mps_converter.h"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "pybind11/attr.h"
#include "pybind11/cast.h"
#include "pybind11/gil.h"
#include "pybind11_abseil/import_status_module.h"
#include "pybind11_abseil/status_casters.h"  // IWYU pragma: keep
#include "pybind11_protobuf/native_proto_caster.h"

namespace operations_research {
namespace math_opt {

PYBIND11_MODULE(mps_converter, m) {
  m.def("model_proto_to_mps", &ModelProtoToMps);
  m.def("mps_to_model_proto", &MpsToModelProto);
}

}  // namespace math_opt
}  // namespace operations_research
