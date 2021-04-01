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

#include "ortools/gscip/legacy_scip_params.h"

#include <cstdint>

#include "absl/status/status.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "ortools/base/logging.h"
#include "ortools/linear_solver/scip_helper_macros.h"
#include "scip/scip.h"
#include "scip/scip_numerics.h"
#include "scip/scip_param.h"
#include "scip/struct_paramset.h"
#include "scip/type_paramset.h"

namespace operations_research {

absl::Status LegacyScipSetSolverSpecificParameters(
    const std::string& parameters, SCIP* scip) {
  for (const auto parameter : absl::StrSplit(parameters, absl::ByAnyChar(",\n"),
                                             absl::SkipWhitespace())) {
    std::vector<std::string> key_value = absl::StrSplit(
        parameter, absl::ByAnyChar("= "), absl::SkipWhitespace());
    if (key_value.size() != 2) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Cannot parse parameter '%s'. Expected format is "
                          "'parameter/name = value'",
                          parameter));
    }

    std::string name = key_value[0];
    absl::RemoveExtraAsciiWhitespace(&name);
    std::string value = key_value[1];
    absl::RemoveExtraAsciiWhitespace(&value);
    const double infinity = SCIPinfinity(scip);

    SCIP_PARAM* param = SCIPgetParam(scip, name.c_str());
    if (param == nullptr) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid parameter name '%s'", name));
    }
    switch (param->paramtype) {
      case SCIP_PARAMTYPE_BOOL: {
        bool parsed_value;
        if (absl::SimpleAtob(value, &parsed_value)) {
          RETURN_IF_SCIP_ERROR(
              SCIPsetBoolParam(scip, name.c_str(), parsed_value));
          VLOG(2) << absl::StrFormat("Set parameter %s to %s", name, value);
          continue;
        }
        break;
      }
      case SCIP_PARAMTYPE_INT: {
        int parsed_value;
        if (absl::SimpleAtoi(value, &parsed_value)) {
          RETURN_IF_SCIP_ERROR(
              SCIPsetIntParam(scip, name.c_str(), parsed_value));
          VLOG(2) << absl::StrFormat("Set parameter %s to %s", name, value);
          continue;
        }
        break;
      }
      case SCIP_PARAMTYPE_LONGINT: {
        int64_t parsed_value;
        if (absl::SimpleAtoi(value, &parsed_value)) {
          RETURN_IF_SCIP_ERROR(SCIPsetLongintParam(
              scip, name.c_str(), static_cast<SCIP_Longint>(parsed_value)));
          VLOG(2) << absl::StrFormat("Set parameter %s to %s", name, value);
          continue;
        }
        break;
      }
      case SCIP_PARAMTYPE_REAL: {
        double parsed_value;
        if (absl::SimpleAtod(value, &parsed_value)) {
          if (parsed_value > infinity) parsed_value = infinity;
          RETURN_IF_SCIP_ERROR(
              SCIPsetRealParam(scip, name.c_str(), parsed_value));
          VLOG(2) << absl::StrFormat("Set parameter %s to %s", name, value);
          continue;
        }
        break;
      }
      case SCIP_PARAMTYPE_CHAR: {
        if (value.size() == 1) {
          RETURN_IF_SCIP_ERROR(SCIPsetCharParam(scip, name.c_str(), value[0]));
          VLOG(2) << absl::StrFormat("Set parameter %s to %s", name, value);
          continue;
        }
        break;
      }
      case SCIP_PARAMTYPE_STRING: {
        if (value.front() == '"' && value.back() == '"') {
          value.erase(value.begin());
          value.erase(value.end() - 1);
        }
        RETURN_IF_SCIP_ERROR(
            SCIPsetStringParam(scip, name.c_str(), value.c_str()));
        VLOG(2) << absl::StrFormat("Set parameter %s to %s", name, value);
        continue;
      }
    }
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid parameter value '%s'", parameter));
  }
  return absl::OkStatus();
}

}  // namespace operations_research
