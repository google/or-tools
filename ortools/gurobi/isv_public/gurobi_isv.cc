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

#include "ortools/gurobi/isv_public/gurobi_isv.h"

#include <string>
#include <utility>

#include "absl/cleanup/cleanup.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/solvers/gurobi.pb.h"

namespace operations_research::math_opt {

absl::StatusOr<GRBenv*> NewPrimaryEnvFromISVKey(const GurobiIsvKey& isv_key) {
  GRBenv* primary_env = nullptr;
  // Surprisingly, even when Gurobi fails to load the environment, it still
  // creates one. Here we make sure to free it properly. If initialization
  // succeeds, we Cancel() this below.
  absl::Cleanup primary_env_cleanup = [primary_env] {
    GRBfreeenv(primary_env);
  };
  const auto handle_failure =
      [primary_env](const int err_code,
                    const absl::string_view operation_name) -> absl::Status {
    if (err_code == 0) {
      return absl::OkStatus();
    }
    // We use GRBgeterrormsg() to get the associated error message that goes
    // with the error and the contains additional data like the user, the host,
    // and the hostid.
    return util::InvalidArgumentErrorBuilder()
           << "failed to create Gurobi primary environment with ISV key, "
           << operation_name << " returned the error (" << err_code
           << "): " << GRBgeterrormsg(primary_env);
  };
  RETURN_IF_ERROR(handle_failure(GRBemptyenv(&primary_env), "GRBemptyenv()"));
  // We want to turn off logging before setting the ISV key so that it doesn't
  // leak. We store the original logging state, and reset it at the end.
  int original_output_flag;
  RETURN_IF_ERROR(
      handle_failure(GRBgetintparam(primary_env, GRB_INT_PAR_OUTPUTFLAG,
                                    &original_output_flag),
                     "getting original GRB_INT_PAR_OUTPUTFLAG value"));
  RETURN_IF_ERROR(
      handle_failure(GRBsetintparam(primary_env, GRB_INT_PAR_OUTPUTFLAG, 0),
                     "turning off GRB_INT_PAR_OUTPUTFLAG"));
  RETURN_IF_ERROR(handle_failure(
      GRBsetstrparam(primary_env, "GURO_PAR_ISVNAME", isv_key.name.c_str()),
      "setting GURO_PAR_ISVNAME"));
  RETURN_IF_ERROR(
      handle_failure(GRBsetstrparam(primary_env, "GURO_PAR_ISVAPPNAME",
                                    isv_key.application_name.c_str()),
                     "setting GURO_PAR_ISVAPPNAME"));
  if (isv_key.expiration != 0) {
    RETURN_IF_ERROR(
        handle_failure(GRBsetintparam(primary_env, "GURO_PAR_ISVEXPIRATION",
                                      isv_key.expiration),
                       "setting GURO_PAR_ISVEXPIRATION"));
  }
  RETURN_IF_ERROR(handle_failure(
      GRBsetstrparam(primary_env, "GURO_PAR_ISVKEY", isv_key.key.c_str()),
      "setting GURO_PAR_ISVKEY"));
  RETURN_IF_ERROR(handle_failure(GRBstartenv(primary_env), "GRBstartenv()"));
  // Reset output flag to its original value.
  RETURN_IF_ERROR(handle_failure(
      GRBsetintparam(primary_env, GRB_INT_PAR_OUTPUTFLAG, original_output_flag),
      "resetting GRB_INT_PAR_OUTPUTFLAG"));
  // Environment initialization succeeded, we don't want to free it upon exiting
  // this function.
  std::move(primary_env_cleanup).Cancel();
  return primary_env;
}

}  // namespace operations_research::math_opt
