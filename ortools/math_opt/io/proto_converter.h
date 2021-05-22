// Copyright 2010-2021 Google LLC
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

#ifndef OR_TOOLS_MATH_OPT_IO_PROTO_CONVERTER_H_
#define OR_TOOLS_MATH_OPT_IO_PROTO_CONVERTER_H_

#include "absl/status/statusor.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/math_opt/model.pb.h"

namespace operations_research {
namespace math_opt {

absl::StatusOr<::operations_research::math_opt::ModelProto>
MPModelProtoToMathOptModel(const ::operations_research::MPModelProto& model);

// Returns a MPModelProto equivalent to the input math_opt Model.
//
// Variables are created in the same order as they appear in
// `model.variables`. Hence the returned `.variable(i)` corresponds to input
// `model.variables.ids(i)`.
absl::StatusOr<::operations_research::MPModelProto> MathOptModelToMPModelProto(
    const ::operations_research::math_opt::ModelProto& model);

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_IO_PROTO_CONVERTER_H_
