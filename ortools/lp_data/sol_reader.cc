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

#include "ortools/lp_data/sol_reader.h"

#include <limits>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ortools/base/numbers.h"
#include "ortools/base/status_macros.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/util/file_util.h"

namespace operations_research {

absl::StatusOr<glop::DenseRow> ParseSolFile(absl::string_view file_name,
                                            const glop::LinearProgram& model) {
  ASSIGN_OR_RETURN(std::string sol_file, ReadFileToString(file_name));
  return ParseSolString(sol_file, model);
}

absl::StatusOr<MPSolutionResponse> ParseSolFile(absl::string_view file_name,
                                                const MPModelProto& model) {
  ASSIGN_OR_RETURN(std::string sol_file, ReadFileToString(file_name));
  return ParseSolString(sol_file, model);
}

absl::StatusOr<glop::DenseRow> ParseSolString(
    const std::string& solution, const glop::LinearProgram& model) {
  constexpr double kInfinity = std::numeric_limits<double>::infinity();

  absl::flat_hash_map<std::string, glop::ColIndex> var_index_by_name;
  for (glop::ColIndex col(0); col < model.num_variables(); ++col) {
    var_index_by_name[model.GetVariableName(col)] = col;
  }

  glop::ColIndex num_variables = model.num_variables();
  glop::DenseRow dense_row(num_variables.value(), 0);
  std::vector<std::string> lines =
      absl::StrSplit(solution, '\n', absl::SkipEmpty());
  for (const std::string& line : lines) {
    std::vector<std::string> fields =
        absl::StrSplit(line, absl::ByAnyChar(" \t"), absl::SkipEmpty());

    // Remove the comments.
    for (int i = 0; i < fields.size(); ++i) {
      if (fields[i][0] == '#') {
        fields.resize(i);
        break;
      }
    }

    if (fields.empty()) continue;

    if (fields.size() == 1) {
      return absl::InvalidArgumentError(
          absl::StrCat("Found only one field on line '", line, "'."));
    }
    if (fields.size() > 2) {
      return absl::InvalidArgumentError(
          absl::StrCat("Found more than two fields on line '", line, "'."));
    }

    const std::string var_name = fields[0];
    const double var_value =
        strings::ParseLeadingDoubleValue(fields[1], kInfinity);
    if (var_value == kInfinity) {
      return absl::InvalidArgumentError(
          absl::StrCat("Couldn't parse value on line '", line, "'."));
    }

    if (var_name == "=obj=") continue;

    auto iter = var_index_by_name.find(var_name);
    if (iter == var_index_by_name.end()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Couldn't find variable named '", var_name, "' in the model."));
    }
    dense_row[iter->second] = var_value;
  }

  return dense_row;
}

absl::StatusOr<MPSolutionResponse> ParseSolString(const std::string& solution,
                                                  const MPModelProto& model) {
  constexpr double kInfinity = std::numeric_limits<double>::infinity();

  absl::flat_hash_map<std::string, int> var_index_by_name;
  for (int var_index = 0; var_index < model.variable_size(); ++var_index) {
    if (model.variable(var_index).name().empty()) {
      return absl::InvalidArgumentError("Found variable without name.");
    }
    var_index_by_name[model.variable(var_index).name()] = var_index;
  }

  MPSolutionResponse response;
  std::vector<double> var_values(model.variable_size(), 0);
  std::vector<std::string> lines =
      absl::StrSplit(solution, '\n', absl::SkipEmpty());
  for (const std::string& line : lines) {
    std::vector<std::string> fields =
        absl::StrSplit(line, absl::ByAnyChar(" \t"), absl::SkipEmpty());

    // Remove the comments.
    for (int i = 0; i < fields.size(); ++i) {
      if (fields[i][0] == '#') {
        fields.resize(i);
        break;
      }
    }

    if (fields.empty()) continue;

    if (fields.size() == 1) {
      return absl::InvalidArgumentError(
          absl::StrCat("Found only one field on line '", line, "'."));
    }
    if (fields.size() > 2) {
      return absl::InvalidArgumentError(
          absl::StrCat("Found more than two fields on line '", line, "'."));
    }

    const std::string var_name = fields[0];
    const double var_value =
        strings::ParseLeadingDoubleValue(fields[1], kInfinity);
    if (var_value == kInfinity) {
      return absl::InvalidArgumentError(
          absl::StrCat("Couldn't parse value on line '", line, "'."));
    }

    if (var_name == "=obj=") {
      response.set_objective_value(var_value);
      continue;
    }

    auto iter = var_index_by_name.find(var_name);
    if (iter == var_index_by_name.end()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Couldn't find variable named '", var_name, "' in the model."));
    }
    var_values[iter->second] = var_value;
  }

  for (const double value : var_values) {
    response.add_variable_value(value);
  }
  return response;
}

}  // namespace operations_research
