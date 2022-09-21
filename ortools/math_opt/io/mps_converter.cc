// Copyright 2010-2022 Google LLC
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

#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/mps_reader.h"
#include "ortools/math_opt/io/proto_converter.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/util/file_util.h"

namespace operations_research::math_opt {

absl::StatusOr<ModelProto> ReadMpsFile(const absl::string_view filename) {
  glop::MPSReader mps_reader;
  MPModelProto mp_model;
  RETURN_IF_ERROR(mps_reader.ParseFile(std::string(filename), &mp_model));
  return MPModelProtoToMathOptModel(mp_model);
}

}  // namespace operations_research::math_opt
