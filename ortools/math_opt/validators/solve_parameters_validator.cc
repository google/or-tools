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

#include "ortools/math_opt/validators/solve_parameters_validator.h"

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/parameters.pb.h"

namespace operations_research {
namespace math_opt {

absl::Status ValidateSolveParameters(const SolveParametersProto& parameters) {
  RETURN_IF_ERROR(
      util_time::DecodeGoogleApiProto(parameters.time_limit()).status())
      << "invalid SolveParameters.time_limit";

  if (parameters.has_threads()) {
    if (parameters.threads() <= 0) {
      return absl::InvalidArgumentError(absl::StrCat(
          "SolveParameters.threads = ", parameters.threads(), " <= 0"));
    }
  }

  if (parameters.has_relative_gap_limit()) {
    if (parameters.relative_gap_limit() < 0) {
      return absl::InvalidArgumentError(
          absl::StrCat("SolveParameters.relative_gap_limit = ",
                       parameters.relative_gap_limit(), " < 0"));
    }
  }

  if (parameters.has_absolute_gap_limit()) {
    if (parameters.absolute_gap_limit() < 0) {
      return absl::InvalidArgumentError(
          absl::StrCat("SolveParameters.absolute_gap_limit = ",
                       parameters.absolute_gap_limit(), " < 0"));
    }
  }

  if (parameters.has_solution_limit() && parameters.solution_limit() <= 0) {
    return util::InvalidArgumentErrorBuilder()
           << "SolveParameters.solution_limit = " << parameters.solution_limit()
           << " should be positive.";
  }

  if (std::isnan(parameters.cutoff_limit())) {
    return absl::InvalidArgumentError("SolveParameters.cutoff_limit was NaN");
  }
  if (std::isnan(parameters.objective_limit())) {
    return absl::InvalidArgumentError(
        "SolveParameters.objective_limit was NaN");
  }
  if (std::isnan(parameters.best_bound_limit())) {
    return absl::InvalidArgumentError(
        "SolveParameters.best_bound_limit was NaN");
  }

  return absl::OkStatus();
}

}  // namespace math_opt
}  // namespace operations_research
