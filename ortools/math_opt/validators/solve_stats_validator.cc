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

#include "ortools/math_opt/validators/solve_stats_validator.h"

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "ortools/base/protoutil.h"
#include "ortools/math_opt/result.pb.h"

namespace operations_research {
namespace math_opt {

absl::Status ValidateSolveStats(const SolveStatsProto& solve_stats) {
  const absl::StatusOr<absl::Duration> solve_time =
      util_time::DecodeGoogleApiProto(solve_stats.solve_time());
  if (!solve_time.ok()) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid solve_time, ", solve_time.status().message()));
  }
  if (solve_time.value() < absl::ZeroDuration()) {
    return absl::InvalidArgumentError("solve_time must be non-negative");
  }
  if (solve_stats.simplex_iterations() < 0) {
    return absl::InvalidArgumentError(
        "simplex_iterations must be non-negative");
  }
  if (solve_stats.barrier_iterations() < 0) {
    return absl::InvalidArgumentError(
        "barrier_iterations must be non-negative");
  }
  if (solve_stats.node_count() < 0) {
    return absl::InvalidArgumentError("node_count must be non-negative");
  }
  return absl::OkStatus();
}

}  // namespace math_opt
}  // namespace operations_research
