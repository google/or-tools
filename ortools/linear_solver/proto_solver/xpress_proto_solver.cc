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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/cleanup/cleanup.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "ortools/base/logging.h"
#include "ortools/base/status_macros.h"
#include "ortools/base/timer.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/model_validator.h"
#include "ortools/linear_solver/proto_solver/xpress_proto_solver.h"
#include "ortools/util/lazy_mutable_copy.h"
#include "ortools/xpress/environment.h"

namespace operations_research {

absl::StatusOr<MPSolutionResponse> XpressSolveProto(
    const MPModelRequest& request) {
  MPSolutionResponse response;
  response.set_status(MPSOLVER_SOLVER_TYPE_UNAVAILABLE);
  response.set_status_str(std::string("XPressMP not supported"));  // NOLINT

  return response;
}

}  // namespace operations_research
