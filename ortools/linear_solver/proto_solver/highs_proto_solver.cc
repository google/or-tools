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

#include <cassert>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "Highs.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "google/protobuf/repeated_field.h"
#include "lp_data/HConst.h"
#include "lp_data/HighsInfo.h"
#include "lp_data/HighsStatus.h"
#include "ortools/base/timer.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/model_validator.h"
#include "ortools/util/lazy_mutable_copy.h"
#include "util/HighsInt.h"

namespace operations_research {

absl::Status SetSolverSpecificParameters(const std::string& parameters,
                                         Highs& highs);

absl::StatusOr<MPSolutionResponse> HighsSolveProto(
    LazyMutableCopy<MPModelRequest> request) {
  MPSolutionResponse response;
  const std::optional<LazyMutableCopy<MPModelProto>> optional_model =
      GetMPModelOrPopulateResponse(request, &response);
  if (!optional_model) return response;
  const MPModelProto& model = **optional_model;

  Highs highs;
  // Set model name.
  if (model.has_name()) {
    const std::string model_name = model.name();
    highs.passModelName(model_name);
  }

  if (request->has_solver_specific_parameters()) {
    const auto parameters_status = SetSolverSpecificParameters(
        request->solver_specific_parameters(), highs);
    if (!parameters_status.ok()) {
      response.set_status(MPSOLVER_MODEL_INVALID_SOLVER_PARAMETERS);
      response.set_status_str(parameters_status.message());
      return response;
    }
  }

  if (request->solver_time_limit_seconds() > 0) {
    HighsStatus status = highs.setOptionValue(
        "time_limit", request->solver_time_limit_seconds());
    if (status == HighsStatus::kError) {
      response.set_status(MPSOLVER_MODEL_INVALID_SOLVER_PARAMETERS);
      response.set_status_str("time_limit");
      return response;
    }
  }

  const int variable_size = model.variable_size();
  bool has_integer_variables = false;
  {
    std::vector<double> obj_coeffs(variable_size, 0);
    std::vector<double> lb(variable_size);
    std::vector<double> ub(variable_size);
    std::vector<HighsVarType> integrality(variable_size);
    for (int v = 0; v < variable_size; ++v) {
      const MPVariableProto& variable = model.variable(v);
      obj_coeffs[v] = variable.objective_coefficient();
      lb[v] = variable.lower_bound();
      ub[v] = variable.upper_bound();
      integrality[v] =
          variable.is_integer() &&
                  request->solver_type() ==
                      MPModelRequest::HIGHS_MIXED_INTEGER_PROGRAMMING
              ? HighsVarType::kInteger
              : HighsVarType::kContinuous;
      if (integrality[v] == HighsVarType::kInteger) {
        has_integer_variables = true;
      }
    }

    highs.addVars(variable_size, lb.data(), ub.data());

    // Mark integrality.
    if (has_integer_variables) {
      for (int v = 0; v < variable_size; ++v) {
        highs.changeColIntegrality(v, integrality[v]);
      }
    }

    // Objective coefficients.
    for (int column = 0; column < variable_size; column++) {
      highs.changeColCost(column, obj_coeffs[column]);
    }

    // Variable names.
    for (int v = 0; v < variable_size; ++v) {
      const MPVariableProto& variable = model.variable(v);
      std::string varname_str = "";
      if (!variable.name().empty()) {
        varname_str = variable.name();
        highs.passColName(v, varname_str);
      }
    }

    // Hints.
    int num_hints = model.solution_hint().var_index_size();
    if (num_hints > 0) {
      std::vector<HighsInt> hint_index(0, num_hints);
      std::vector<double> hint_value(0, num_hints);
      for (int i = 0; i < num_hints; ++i) {
        hint_index[i] = model.solution_hint().var_index(i);
        hint_value[i] = model.solution_hint().var_value(i);
      }
      const int* hint_indices = &hint_index[0];
      const double* hint_values = &hint_value[0];
      highs.setSolution((HighsInt)num_hints, hint_indices, hint_values);
    }
  }

  {
    for (int c = 0; c < model.constraint_size(); ++c) {
      const MPConstraintProto& constraint = model.constraint(c);
      if (constraint.lower_bound() ==
          -std::numeric_limits<double>::infinity()) {
        HighsStatus status =
            highs.addRow(/*lhs=*/-kHighsInf,
                         /*rhs=*/constraint.upper_bound(),
                         /*numnz=*/constraint.var_index_size(),
                         /*cind=*/constraint.var_index().data(),
                         /*cval=*/constraint.coefficient().data());
        if (status == HighsStatus::kError) {
          response.set_status(MPSOLVER_MODEL_INVALID);
          response.set_status_str("ct addRow");
          return response;
        }
      } else if (constraint.upper_bound() ==
                 std::numeric_limits<double>::infinity()) {
        HighsStatus status =
            highs.addRow(/*lhs=*/constraint.lower_bound(),
                         /*rhs=*/kHighsInf,
                         /*numnz=*/constraint.var_index_size(),
                         /*cind=*/constraint.var_index().data(),
                         /*cval=*/constraint.coefficient().data());
        if (status == HighsStatus::kError) {
          response.set_status(MPSOLVER_MODEL_INVALID);
          response.set_status_str("ct addRow");
          return response;
        }
      } else {
        HighsStatus status =
            highs.addRow(/*lhs=*/constraint.lower_bound(),
                         /*rhs=*/constraint.upper_bound(),
                         /*numnz=*/constraint.var_index_size(),
                         /*cind=*/constraint.var_index().data(),
                         /*cval=*/constraint.coefficient().data());
        if (status == HighsStatus::kError) {
          response.set_status(MPSOLVER_MODEL_INVALID);
          response.set_status_str("ct addRow");
          return response;
        }
      }

      // Constraint names.
      for (int c = 0; c < model.constraint_size(); ++c) {
        const MPConstraintProto& constraint = model.constraint(c);
        std::string constraint_name_str = "";
        if (!constraint.name().empty()) {
          constraint_name_str = constraint.name();
          highs.passRowName(c, constraint_name_str);
        }
      }
    }

    if (!model.general_constraint().empty()) {
      response.set_status(MPSOLVER_MODEL_INVALID);
      response.set_status_str("general constraints are not supported in Highs");
      return response;
    }
  }

  if (model.maximize()) {
    const ObjSense pass_sense = ObjSense::kMaximize;
    highs.changeObjectiveSense(pass_sense);
  }

  if (model.objective_offset()) {
    const double offset = model.objective_offset();
    highs.changeObjectiveOffset(offset);
  }

  // Logging.
  if (request->enable_internal_solver_output()) {
    highs.setOptionValue("log_to_console", true);
    highs.setOptionValue("output_flag", true);
  } else {
    highs.setOptionValue("log_to_console", false);
    highs.setOptionValue("output_flag", false);
  }

  const absl::Time time_before = absl::Now();
  UserTimer user_timer;
  user_timer.Start();
  HighsStatus run_status = highs.run();
  switch (run_status) {
    case HighsStatus::kError: {
      response.set_status(MPSOLVER_NOT_SOLVED);
      response.set_status_str("Error running HiGHS run()");
      return response;
    }
    case HighsStatus::kWarning: {
      response.set_status_str("Warning HiGHS run()");
      break;
    }
    case HighsStatus::kOk: {
      HighsModelStatus model_status = highs.getModelStatus();
      switch (model_status) {
        case HighsModelStatus::kOptimal:
          response.set_status(MPSOLVER_OPTIMAL);
          break;
        case HighsModelStatus::kUnboundedOrInfeasible:
          response.set_status_str(
              "The model may actually be unbounded: HiGHS returned "
              "kUnboundedOrInfeasible");
          response.set_status(MPSOLVER_INFEASIBLE);
          break;
        case HighsModelStatus::kInfeasible:
          response.set_status(MPSOLVER_INFEASIBLE);
          break;
        case HighsModelStatus::kUnbounded:
          response.set_status(MPSOLVER_UNBOUNDED);
          break;
        default: {
          // TODO(user): report feasible status.
          const HighsInfo& info = highs.getInfo();
          if (info.primal_solution_status == kSolutionStatusFeasible)
            response.set_status(MPSOLVER_FEASIBLE);
          break;
        }
      }
    }
  }

  const absl::Duration solving_duration = absl::Now() - time_before;
  user_timer.Stop();
  response.mutable_solve_info()->set_solve_wall_time_seconds(
      absl::ToDoubleSeconds(solving_duration));
  response.mutable_solve_info()->set_solve_user_time_seconds(
      absl::ToDoubleSeconds(user_timer.GetDuration()));

  if (response.status() == MPSOLVER_OPTIMAL) {
    double objective_value = highs.getObjectiveValue();
    response.set_objective_value(objective_value);
    response.set_best_objective_bound(objective_value);

    response.mutable_variable_value()->Resize(variable_size, 0);
    for (int column = 0; column < variable_size; column++) {
      response.mutable_variable_value()->mutable_data()[column] =
          highs.getSolution().col_value[column];
    }

    if (has_integer_variables) {
      for (int v = 0; v < variable_size; ++v) {
        if (model.variable(v).is_integer()) {
          response.set_variable_value(v,
                                      std::round(response.variable_value(v)));
        }
      }
    }

    if (!has_integer_variables && model.general_constraint_size() == 0) {
      response.mutable_dual_value()->Resize(model.constraint_size(), 0);
      for (int row = 0; row < model.constraint_size(); row++) {
        response.set_dual_value(row, highs.getSolution().row_value[row]);
      }
    }
  }

  return response;
}

absl::Status SetSolverSpecificParameters(const std::string& parameters,
                                         Highs& highs) {
  if (parameters.empty()) return absl::OkStatus();
  std::vector<std::string> error_messages;
  for (absl::string_view line : absl::StrSplit(parameters, '\n')) {
    // Comment tokens end at the next new-line, or the end of the string.
    // The first character must be '#'
    if (line[0] == '#') continue;
    for (absl::string_view token :
         absl::StrSplit(line, ',', absl::SkipWhitespace())) {
      if (token.empty()) continue;
      std::vector<std::string> key_value =
          absl::StrSplit(token, absl::ByAnyChar(" ="), absl::SkipWhitespace());
      // If one parameter fails, we keep processing the list of parameters.
      if (key_value.size() != 2) {
        const std::string current_message =
            absl::StrCat("Cannot parse parameter '", token,
                         "'. Expected format is 'ParameterName value' or "
                         "'ParameterName=value'");
        error_messages.push_back(current_message);
        continue;
      }
      HighsStatus status = highs.setOptionValue(key_value[0], key_value[1]);
      if (status == HighsStatus::kError) {
        const std::string current_message =
            absl::StrCat("Error setting parameter '", key_value[0],
                         "' to value '", key_value[1], "': ");
        error_messages.push_back(current_message);
        continue;
      }
    }
  }

  if (error_messages.empty()) return absl::OkStatus();
  return absl::InvalidArgumentError(absl::StrJoin(error_messages, "\n"));
}

}  // namespace operations_research
