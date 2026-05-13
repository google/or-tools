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

#include "ortools/gurobi/gurobi_util.h"

#include <cstring>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "ortools/gurobi/environment.h"

namespace operations_research {

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
