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

#include "ortools/math_opt/io/lp_converter.h"

#include <string>

#include "absl/status/statusor.h"
#include "ortools/base/status_macros.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/model_exporter.h"
#include "ortools/math_opt/io/proto_converter.h"
#include "ortools/math_opt/model.pb.h"

namespace operations_research::math_opt {

absl::StatusOr<std::string> ModelProtoToLp(const ModelProto& model) {
  ASSIGN_OR_RETURN(const MPModelProto mp_model_proto,
                   MathOptModelToMPModelProto(model));
  return ExportModelAsLpFormat(mp_model_proto, {.show_unused_variables = true});
}

}  // namespace operations_research::math_opt
