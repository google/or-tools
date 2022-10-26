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

#ifndef OR_TOOLS_MATH_OPT_IO_PROTO_CONVERTER_H_
#define OR_TOOLS_MATH_OPT_IO_PROTO_CONVERTER_H_

#include "absl/status/statusor.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/math_opt/model.pb.h"

namespace operations_research::math_opt {

// Returns a ModelProto equivalent to the input linear_solver Model. The input
// MPModelProto must be valid, as checked by `FindErrorInMPModelProto`.
//
// The linear_solver Model stores all general constraints (e.g., quadratic, SOS)
// in a single repeated field, while ModelProto stores then in separate maps.
// The output constraint maps will each be populated with consecutive indices
// starting from 0 (hence the indices may change).
absl::StatusOr<ModelProto> MPModelProtoToMathOptModel(
    const MPModelProto& model);

// Returns a linear_solver MPModelProto equivalent to the input math_opt Model.
// The input Model must be in a valid state, as checked by `ValidateModel`.
//
// Variables are created in the same order as they appear in
// `model.variables`. Hence the returned `.variable(i)` corresponds to input
// `model.variables.ids(i)`.
//
// The linear_solver Model stores all general constraints (e.g., quadratic, SOS)
// in a single repeated field, while ModelProto stores then in separate maps.
// Therefore neither the relative ordering, nor the raw IDs, of general
// constraints are preserved in the resulting Model.
absl::StatusOr<MPModelProto> MathOptModelToMPModelProto(
    const ModelProto& model);

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_IO_PROTO_CONVERTER_H_
