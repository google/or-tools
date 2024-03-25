// Copyright 2010-2024 Google LLC
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

#include "ortools/linear_solver/proto_solver/highs_proto_solver.h"

#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/model_validator.h"
#include "ortools/port/proto_utils.h"
#include "ortools/util/lazy_mutable_copy.h"

namespace operations_research {

absl::StatusOr<MPSolutionResponse> HighsSolveProto(
    LazyMutableCopy<MPModelRequest> request) {
  return absl::UnimplementedError("Highs support is not yet implemented");
}

}  // namespace operations_research
