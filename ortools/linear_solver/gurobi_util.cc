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

#include "ortools/linear_solver/gurobi_util.h"

#include <cstring>
#include <string>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "ortools/third_party_solvers/gurobi_environment.h"

namespace operations_research {
namespace {

constexpr int kGurobiOkCode = 0;
constexpr int kIntParam = 1;
constexpr int kDoubleParam = 2;
constexpr int kStringParam = 3;

absl::Status GurobiStatus(const int gurobi_code, GRBenv* const env,
                          const std::string& operation) {
  if (gurobi_code == kGurobiOkCode) {
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(
      absl::StrCat(operation, " failed with Gurobi error ", gurobi_code, ": ",
                   GRBgeterrormsg(env)));
}

}  // namespace

bool GurobiIsCorrectlyInstalled() {
  absl::StatusOr<GRBenv*> status = GetGurobiEnv();
  if (!status.ok() || status.value() == nullptr) {
    LOG(WARNING) << status.status();
    return false;
  }

  GRBfreeenv(status.value());

  return true;
}

absl::StatusOr<GRBenv*> GetGurobiEnv() {
  GRBenv* env = nullptr;

  ABSL_RETURN_IF_ERROR(LoadGurobiDynamicLibrary({}));

  if (GRBloadenv(&env, nullptr) != 0 || env == nullptr) {
    return absl::FailedPreconditionError(
        absl::StrCat("Found the Gurobi shared library, but could not create "
                     "Gurobi environment: is Gurobi licensed on this machine?",
                     GRBgeterrormsg(env)));
  }

  return env;
}

absl::Status CopyGurobiParameters(GRBenv* const dest, GRBenv* const src) {
  if (GRBcopyparams) {
    return GurobiStatus(GRBcopyparams(dest, src), src, "GRBcopyparams()");
  }

  const int num_parameters = GRBgetnumparams(src);
  for (int i = 0; i < num_parameters; ++i) {
    char* param_name = nullptr;
    RETURN_IF_ERROR(GurobiStatus(GRBgetparamname(src, i, &param_name), src,
                                 "GRBgetparamname()"));
    const int param_type = GRBgetparamtype(src, param_name);
    switch (param_type) {
      case kIntParam: {
        int current_value;
        int default_value;
        int min_value;
        int max_value;
        RETURN_IF_ERROR(GurobiStatus(
            GRBgetintparaminfo(src, param_name, &current_value, &min_value,
                               &max_value, &default_value),
            src, absl::StrCat("GRBgetintparaminfo(", param_name, ")")));
        if (current_value != default_value) {
          RETURN_IF_ERROR(GurobiStatus(
              GRBsetintparam(dest, param_name, current_value), dest,
              absl::StrCat("GRBsetintparam(", param_name, ")")));
        }
        break;
      }
      case kDoubleParam: {
        double current_value;
        double default_value;
        double min_value;
        double max_value;
        RETURN_IF_ERROR(GurobiStatus(
            GRBgetdblparaminfo(src, param_name, &current_value, &min_value,
                               &max_value, &default_value),
            src, absl::StrCat("GRBgetdblparaminfo(", param_name, ")")));
        if (current_value != default_value) {
          RETURN_IF_ERROR(GurobiStatus(
              GRBsetdblparam(dest, param_name, current_value), dest,
              absl::StrCat("GRBsetdblparam(", param_name, ")")));
        }
        break;
      }
      case kStringParam: {
        char current_value[GRB_MAX_STRLEN + 1];
        char default_value[GRB_MAX_STRLEN + 1];
        RETURN_IF_ERROR(GurobiStatus(
            GRBgetstrparaminfo(src, param_name, current_value, default_value),
            src, absl::StrCat("GRBgetstrparaminfo(", param_name, ")")));
        // This ensures that strcmp does not go beyond the end of the char array.
        current_value[GRB_MAX_STRLEN] = '\0';
        default_value[GRB_MAX_STRLEN] = '\0';
        if (std::strcmp(current_value, default_value) != 0) {
          RETURN_IF_ERROR(GurobiStatus(
              GRBsetstrparam(dest, param_name, current_value), dest,
              absl::StrCat("GRBsetstrparam(", param_name, ")")));
        }
        break;
      }
      default:
        LOG(WARNING) << "Skipping Gurobi parameter '" << param_name
                     << "' of unknown type " << param_type << ".";
    }
  }
  return absl::OkStatus();
}

std::string GurobiParamInfoForLogging(GRBenv* grb, bool one_liner_output) {
  const absl::ParsedFormat<'s', 's', 's'> kExtendedFormat(
      "  Parameter: '%s' value: %s default: %s");
  const absl::ParsedFormat<'s', 's', 's'> kOneLinerFormat("'%s':%s (%s)");
  const absl::ParsedFormat<'s', 's', 's'>& format =
      one_liner_output ? kOneLinerFormat : kExtendedFormat;
  std::vector<std::string> changed_parameters;
  const int num_parameters = GRBgetnumparams(grb);
  for (int i = 0; i < num_parameters; ++i) {
    char* param_name = nullptr;
    GRBgetparamname(grb, i, &param_name);
    const int param_type = GRBgetparamtype(grb, param_name);
    switch (param_type) {
      case 1:  // integer parameters.
      {
        int default_value;
        int min_value;
        int max_value;
        int current_value;
        GRBgetintparaminfo(grb, param_name, &current_value, &min_value,
                           &max_value, &default_value);
        if (current_value != default_value) {
          changed_parameters.push_back(
              absl::StrFormat(format, param_name, absl::StrCat(current_value),
                              absl::StrCat(default_value)));
        }
        break;
      }
      case 2:  // double parameters.
      {
        double default_value;
        double min_value;
        double max_value;
        double current_value;
        GRBgetdblparaminfo(grb, param_name, &current_value, &min_value,
                           &max_value, &default_value);
        if (current_value != default_value) {
          changed_parameters.push_back(
              absl::StrFormat(format, param_name, absl::StrCat(current_value),
                              absl::StrCat(default_value)));
        }
        break;
      }
      case 3:  // string parameters.
      {
        char current_value[GRB_MAX_STRLEN + 1];
        char default_value[GRB_MAX_STRLEN + 1];
        GRBgetstrparaminfo(grb, param_name, current_value, default_value);
        // This ensure that strcmp does not go beyond the end of the char
        // array.
        current_value[GRB_MAX_STRLEN] = '\0';
        default_value[GRB_MAX_STRLEN] = '\0';
        if (std::strcmp(current_value, default_value) != 0) {
          changed_parameters.push_back(absl::StrFormat(
              format, param_name, current_value, default_value));
        }
        break;
      }
      default:  // unknown parameter types
        changed_parameters.push_back(absl::StrFormat(
            "Parameter '%s' of unknown type %d", param_name, param_type));
    }
  }
  if (changed_parameters.empty()) return "";
  if (one_liner_output) {
    return absl::StrCat("GurobiParams{",
                        absl::StrJoin(changed_parameters, ", "), "}");
  }
  return absl::StrJoin(changed_parameters, "\n");
}

}  //  namespace operations_research
