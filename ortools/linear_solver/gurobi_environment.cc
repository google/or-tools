// Copyright 2010-2018 Google LLC
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

#if defined(USE_GUROBI)
#include "ortools/linear_solver/gurobi_environment.h"

#include <string>

#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "ortools/base/canonical_errors.h"
// #include "ortools/base/hostname.h"
#include "ortools/base/logging.h"

namespace operations_research {
util::Status LoadGurobiEnvironment(GRBenv** env) {
  constexpr int GRB_OK = 0;
  const char kGurobiEnvErrorMsg[] =
      "Could not load Gurobi environment. Is gurobi correctly installed and "
      "licensed on this machine?";

  if (GRBloadenv(env, nullptr) != 0 || *env == nullptr) {
    return util::FailedPreconditionError(
        absl::StrFormat("%s %s", kGurobiEnvErrorMsg, GRBgeterrormsg(*env)));
  }
  return util::OkStatus();
}
}  // namespace operations_research
#endif  //  #if defined(USE_GUROBI)
