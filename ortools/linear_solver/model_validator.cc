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

#include "ortools/linear_solver/model_validator.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/types/optional.h"
#include "ortools/base/accurate_sum.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/port/file.h"
#include "ortools/port/proto_utils.h"
#include "ortools/util/fp_utils.h"
#include "ortools/util/lazy_mutable_copy.h"

ABSL_FLAG(
    double, model_validator_infinity, 1e100,
    "Anything above or equal to this magnitude will be considered infinity.");

namespace operations_research {
namespace {

bool IsNanOrAbsGreaterThanOrEqual(double value, double abs_value_threshold) {
  return std::isnan(value) || std::abs(value) >= abs_value_threshold;
}

// Internal method to detect errors in bounds. The object passed as parameter
// must have "lower_bound" and "upper_bound" fields.
template <typename BoundedElement>
std::string FindErrorInBounds(const BoundedElement& element,
                              double abs_value_threshold,
                              const bool accept_trivially_infeasible_bounds) {
  if (std::isnan(element.lower_bound()) || std::isnan(element.upper_bound()) ||
      element.lower_bound() >= abs_value_threshold ||
      element.upper_bound() <= -abs_value_threshold ||
      (!accept_trivially_infeasible_bounds &&
       element.lower_bound() > element.upper_bound())) {
    return absl::StrFormat("Infeasible bounds: [%f, %f]", element.lower_bound(),
                           element.upper_bound());
  }
  return "";
}

// Internal method to detect errors in a single variable.
std::string FindErrorInMPVariable(
    const MPVariableProto& variable, double abs_value_threshold,
    const bool accept_trivially_infeasible_bounds) {
  const std::string bound_error = FindErrorInBounds(
      variable, abs_value_threshold, accept_trivially_infeasible_bounds);
  if (!bound_error.empty()) return bound_error;

  if (!accept_trivially_infeasible_bounds && variable.is_integer() &&
      ceil(variable.lower_bound()) > floor(variable.upper_bound())) {
    return absl::StrCat(
        "Infeasible bounds for integer variable: [", (variable.lower_bound()),
        ", ", (variable.upper_bound()), "]", " translate to the empty set");
  }
  if (IsNanOrAbsGreaterThanOrEqual(variable.objective_coefficient(),
                                   abs_value_threshold)) {
    return absl::StrCat("Invalid objective_coefficient: ",
                        (variable.objective_coefficient()));
  }
  return std::string();
}

// Returns an error message if 'var_indices' contains a duplicate index.
template <typename Iterable>
std::string FindDuplicateVarIndex(const Iterable& var_indices,
                                  std::vector<bool>* var_mask) {
  int duplicate_var_index = -1;
  for (const int var_index : var_indices) {
    if ((*var_mask)[var_index]) duplicate_var_index = var_index;
    (*var_mask)[var_index] = true;
  }
  // Reset "var_mask" to all false, sparsely.
  for (const int var_index : var_indices) {
    (*var_mask)[var_index] = false;
  }
  if (duplicate_var_index >= 0) {
    return absl::StrCat("var_index #", duplicate_var_index,
                        " appears several times");
  }
  return "";
}

// Internal method to detect errors in a single constraint.
// "var_mask" is a vector<bool> whose size is the number of variables in
// the model, and it will be all set to false before and after the call.
std::string FindErrorInMPConstraint(
    const MPConstraintProto& constraint, std::vector<bool>* var_mask,
    double abs_value_threshold, const bool accept_trivially_infeasible_bounds) {
  const std::string bound_error = FindErrorInBounds(
      constraint, abs_value_threshold, accept_trivially_infeasible_bounds);
  if (!bound_error.empty()) return bound_error;

  // TODO(user): clarify explicitly, at least in a comment, whether we want
  // to accept empty constraints (i.e. without variables).

  const int num_vars_in_model = var_mask->size();
  const int num_vars_in_ct = constraint.var_index_size();
  const int num_coeffs_in_ct = constraint.coefficient_size();
  if (num_vars_in_ct != num_coeffs_in_ct) {
    return absl::StrCat("var_index_size() != coefficient_size() (",
                        num_vars_in_ct, " VS ", num_coeffs_in_ct);
  }
  for (int i = 0; i < num_vars_in_ct; ++i) {
    const int var_index = constraint.var_index(i);
    if (var_index >= num_vars_in_model || var_index < 0) {
      return absl::StrCat("var_index(", i, ")=", var_index,
                          " is out of bounds");
    }
    const double coeff = constraint.coefficient(i);
    if (IsNanOrAbsGreaterThanOrEqual(coeff, abs_value_threshold)) {
      return absl::StrCat("coefficient(", i, ")=", (coeff), " is invalid");
    }
  }

  const std::string error =
      FindDuplicateVarIndex(constraint.var_index(), var_mask);
  if (!error.empty()) return error;

  // We found no error, all is fine.
  return std::string();
}

std::string CroppedConstraintDebugString(const MPConstraintProto& constraint) {
  const int kMaxPrintedVars = 10;

  MPConstraintProto constraint_light = constraint;
  std::string suffix_str;
  if (constraint.var_index_size() > kMaxPrintedVars) {
    constraint_light.mutable_var_index()->Truncate(kMaxPrintedVars);
    absl::StrAppend(&suffix_str,
                    " (var_index cropped; size=", constraint.var_index_size(),
                    ").");
  }
  if (constraint.coefficient_size() > kMaxPrintedVars) {
    constraint_light.mutable_coefficient()->Truncate(kMaxPrintedVars);
    absl::StrAppend(&suffix_str, " (coefficient cropped; size=",
                    constraint.coefficient_size(), ").");
  }
  return absl::StrCat("Constraint proto: ",
                      ProtobufShortDebugString(constraint_light), suffix_str);
}

bool IsBoolean(const MPVariableProto& variable) {
  if (variable.lower_bound() < 0) return false;
  if (variable.upper_bound() > 1) return false;
  return variable.is_integer();
}

std::string FindErrorInMPIndicatorConstraint(
    const MPModelProto& model, const MPIndicatorConstraint& indicator,
    std::vector<bool>* var_mask, double abs_value_threshold,
    bool accept_trivially_infeasible_bounds) {
  if (!indicator.has_var_index()) {
    return "var_index is required.";
  }
  const int var_index = indicator.var_index();
  if (var_index < 0 || var_index >= model.variable_size()) {
    return absl::StrCat("var_index=", var_index, " is out of bounds.");
  }
  if (!IsBoolean(model.variable(var_index))) {
    return absl::StrCat("var_index=", var_index, " is not Boolean.");
  }
  const int var_value = indicator.var_value();
  if (var_value < 0 || var_value > 1) {
    return absl::StrCat("var_value=", var_value, " must be 0 or 1.");
  }
  const MPConstraintProto& constraint = indicator.constraint();
  std::string error =
      FindErrorInMPConstraint(constraint, var_mask, abs_value_threshold,
                              accept_trivially_infeasible_bounds);
  if (!error.empty()) {
    // Constraint protos can be huge, theoretically. So we guard against
    // that.
    return absl::StrCat(error, " in constraint ",
                        CroppedConstraintDebugString(constraint));
  }
  return "";
}

std::string FindErrorInMPSosConstraint(const MPModelProto& model,
                                       const MPSosConstraint& sos,
                                       std::vector<bool>* var_mask,
                                       double abs_value_threshold) {
  if (sos.weight_size() != 0 && sos.weight_size() != sos.var_index_size()) {
    return "weight_size() > 0 and var_index_size() != weight_size()";
  }
  for (const int var_index : sos.var_index()) {
    if (var_index < 0 || var_index >= model.variable_size()) {
      return absl::StrCat("var_index=", var_index, " is out of bounds.");
    }
  }
  for (int i = 0; i < sos.weight_size(); ++i) {
    if (IsNanOrAbsGreaterThanOrEqual(sos.weight(i), abs_value_threshold)) {
      return absl::StrCat("Invalid weight: ", sos.weight(i));
    }
    if (i == 0) continue;
    if (sos.weight(i - 1) >= sos.weight(i)) {
      return "SOS weights must be strictly increasing";
    }
  }

  const std::string error = FindDuplicateVarIndex(sos.var_index(), var_mask);
  if (!error.empty()) return error;

  return "";
}

std::string FindErrorInMPQuadraticConstraint(
    const MPModelProto& model, const MPQuadraticConstraint& qcst,
    std::vector<bool>* var_mask, double abs_value_threshold,
    bool accept_trivially_infeasible_bounds) {
  const int num_vars = model.variable_size();

  if (qcst.var_index_size() != qcst.coefficient_size()) {
    return "var_index_size() != coefficient_size()";
  }

  const std::string bound_error = FindErrorInBounds(
      qcst, abs_value_threshold, accept_trivially_infeasible_bounds);
  if (!bound_error.empty()) return bound_error;

  for (int i = 0; i < qcst.var_index_size(); ++i) {
    if (qcst.var_index(i) < 0 || qcst.var_index(i) >= num_vars) {
      return absl::StrCat("var_index(", i, ")=", qcst.var_index(i),
                          " is invalid.", " It must be in [0, ", num_vars, ")");
    }

    if (IsNanOrAbsGreaterThanOrEqual(qcst.coefficient(i),
                                     abs_value_threshold)) {
      return absl::StrCat("coefficient(", i, ")=", qcst.coefficient(i),
                          " is invalid");
    }
  }
  const std::string duplicate_error =
      FindDuplicateVarIndex(qcst.var_index(), var_mask);
  if (!duplicate_error.empty()) return duplicate_error;

  if (qcst.qvar1_index_size() != qcst.qvar2_index_size() ||
      qcst.qvar1_index_size() != qcst.qcoefficient_size()) {
    return "quadratic indices and coefficients must have the same size";
  }
  for (int i = 0; i < qcst.qvar1_index_size(); ++i) {
    if (qcst.qvar1_index(i) >= num_vars || qcst.qvar1_index(i) < 0) {
      return absl::StrCat("qvar1_index(", i, ")=", qcst.qvar1_index(i),
                          " is invalid.", " It must be in [0, ", num_vars, ")");
    }

    if (qcst.qvar2_index(i) >= num_vars || qcst.qvar2_index(i) < 0) {
      return absl::StrCat("qvar2_index(", i, ")=", qcst.qvar2_index(i),
                          " is invalid.", " It must be in [0, ", num_vars, ")");
    }

    if (IsNanOrAbsGreaterThanOrEqual(qcst.qcoefficient(i),
                                     abs_value_threshold)) {
      return absl::StrCat("qcoefficient(", i, ")=", qcst.qcoefficient(i),
                          " is invalid");
    }
  }

  return "";
}

std::string FindErrorInMPAbsConstraint(const MPModelProto& model,
                                       const MPAbsConstraint& abs) {
  if (!abs.has_var_index()) {
    return "var_index is required.";
  }
  if (!abs.has_resultant_var_index()) {
    return "resultant_var_index is required.";
  }

  const int num_vars = model.variable_size();
  if (abs.var_index() < 0 || abs.var_index() >= num_vars) {
    return absl::StrCat("var_index=", abs.var_index(), " is invalid.",
                        " It must be in [0, ", num_vars, ")");
  }
  if (abs.resultant_var_index() < 0 || abs.resultant_var_index() >= num_vars) {
    return absl::StrCat("var_index=", abs.resultant_var_index(), " is invalid.",
                        " It must be in [0, ", num_vars, ")");
  }
  return "";
}

std::string FindErrorInMPAndOrConstraint(const MPModelProto& model,
                                         const MPArrayConstraint& and_or) {
  if (and_or.var_index_size() == 0) {
    return "var_index cannot be empty.";
  }
  if (!and_or.has_resultant_var_index()) {
    return "resultant_var_index is required.";
  }

  const int num_vars = model.variable_size();
  for (int i = 0; i < and_or.var_index_size(); ++i) {
    if (and_or.var_index(i) < 0 || and_or.var_index(i) >= num_vars) {
      return absl::StrCat("var_index(", i, ")=", and_or.var_index(i),
                          " is invalid.", " It must be in [0, ", num_vars, ")");
    }
    if (!IsBoolean(model.variable(and_or.var_index(i)))) {
      return absl::StrCat("var_index=", i, " is not Boolean.");
    }
  }
  if (and_or.resultant_var_index() < 0 ||
      and_or.resultant_var_index() >= num_vars) {
    return absl::StrCat("resultant_var_index=", and_or.resultant_var_index(),
                        " is invalid.", " It must be in [0, ", num_vars, ")");
  }
  if (!IsBoolean(model.variable(and_or.resultant_var_index()))) {
    return absl::StrCat("resultant_var_index is not Boolean.");
  }
  return "";
}

std::string FindErrorInMPMinMaxConstraint(
    const MPModelProto& model, const MPArrayWithConstantConstraint& min_max,
    double abs_value_threshold) {
  if (min_max.var_index_size() == 0) {
    return "var_index cannot be empty.";
  }
  if (!min_max.has_resultant_var_index()) {
    return "resultant_var_index is required.";
  }

  if (IsNanOrAbsGreaterThanOrEqual(min_max.constant(), abs_value_threshold)) {
    return absl::StrCat("Invalid constant: ", (min_max.constant()));
  }

  const int num_vars = model.variable_size();
  for (int i = 0; i < min_max.var_index_size(); ++i) {
    if (min_max.var_index(i) < 0 || min_max.var_index(i) >= num_vars) {
      return absl::StrCat("var_index(", i, ")=", min_max.var_index(i),
                          " is invalid.", " It must be in [0, ", num_vars, ")");
    }
  }
  if (min_max.resultant_var_index() < 0 ||
      min_max.resultant_var_index() >= num_vars) {
    return absl::StrCat("resultant_var_index=", min_max.resultant_var_index(),
                        " is invalid.", " It must be in [0, ", num_vars, ")");
  }
  return "";
}

std::string FindErrorInQuadraticObjective(const MPQuadraticObjective& qobj,
                                          int num_vars,
                                          double abs_value_threshold) {
  if (qobj.qvar1_index_size() != qobj.qvar2_index_size() ||
      qobj.qvar1_index_size() != qobj.coefficient_size()) {
    return "indices and coefficients must have the same size";
  }

  for (int i = 0; i < qobj.qvar1_index_size(); ++i) {
    if (qobj.qvar1_index(i) >= num_vars || qobj.qvar1_index(i) < 0) {
      return absl::StrCat("qvar1_index(", i, ")=", qobj.qvar1_index(i),
                          " is invalid.", " It must be in [0, ", num_vars, ")");
    }

    if (qobj.qvar2_index(i) >= num_vars || qobj.qvar2_index(i) < 0) {
      return absl::StrCat("qvar2_index(", i, ")=", qobj.qvar2_index(i),
                          " is invalid.", " It must be in [0, ", num_vars, ")");
    }

    if (IsNanOrAbsGreaterThanOrEqual(qobj.coefficient(i),
                                     abs_value_threshold)) {
      return absl::StrCat("coefficient(", i, ")=", (qobj.coefficient(i)),
                          " is invalid");
    }
  }
  return "";
}

std::string FindErrorInSolutionHint(
    const PartialVariableAssignment& solution_hint, int num_vars,
    double abs_value_threshold) {
  if (solution_hint.var_index_size() != solution_hint.var_value_size()) {
    return absl::StrCat("var_index_size() != var_value_size() [",
                        solution_hint.var_index_size(), " VS ",
                        solution_hint.var_value_size());
  }
  std::vector<bool> var_in_hint(num_vars, false);
  for (int i = 0; i < solution_hint.var_index_size(); ++i) {
    const int var_index = solution_hint.var_index(i);
    if (var_index >= num_vars || var_index < 0) {
      return absl::StrCat("var_index(", i, ")=", var_index, " is invalid.",
                          " It must be in [0, ", num_vars, ")");
    }
    if (var_in_hint[var_index]) {
      return absl::StrCat("Duplicate var_index = ", var_index);
    }
    var_in_hint[var_index] = true;
    if (IsNanOrAbsGreaterThanOrEqual(solution_hint.var_value(i),
                                     abs_value_threshold)) {
      return absl::StrCat("var_value(", i, ")=", (solution_hint.var_value(i)),
                          " is invalid");
    }
  }
  return std::string();
}

}  // namespace

std::string FindErrorInMPModelProto(
    const MPModelProto& model, double abs_value_threshold,
    const bool accept_trivially_infeasible_bounds) {
  // NOTE(user): Empty models are considered fine by this function, although
  // it is not clear whether MPSolver::Solve() will always respond in the same
  // way, depending on the solvers.
  if (abs_value_threshold == 0.0) {
    abs_value_threshold = absl::GetFlag(FLAGS_model_validator_infinity);
  }

  if (IsNanOrAbsGreaterThanOrEqual(model.objective_offset(),
                                   abs_value_threshold)) {
    return absl::StrCat("Invalid objective_offset: ",
                        (model.objective_offset()));
  }
  const int num_vars = model.variable_size();
  const int num_cts = model.constraint_size();

  // Validate variables.
  std::string error;
  for (int i = 0; i < num_vars; ++i) {
    error = FindErrorInMPVariable(model.variable(i), abs_value_threshold,
                                  accept_trivially_infeasible_bounds);
    if (!error.empty()) {
      return absl::StrCat("In variable #", i, ": ", error, ". Variable proto: ",
                          ProtobufShortDebugString(model.variable(i)));
    }
  }

  // Validate constraints.
  std::vector<bool> variable_appears(num_vars, false);
  for (int i = 0; i < num_cts; ++i) {
    const MPConstraintProto& constraint = model.constraint(i);
    error = FindErrorInMPConstraint(constraint, &variable_appears,
                                    abs_value_threshold,
                                    accept_trivially_infeasible_bounds);
    if (!error.empty()) {
      // Constraint protos can be huge, theoretically. So we guard against that.
      return absl::StrCat("In constraint #", i, ": ", error, ". ",
                          CroppedConstraintDebugString(constraint));
    }
  }

  // Validate general constraints.
  for (int i = 0; i < model.general_constraint_size(); ++i) {
    const MPGeneralConstraintProto& gen_constraint =
        model.general_constraint(i);
    std::string error;
    switch (gen_constraint.general_constraint_case()) {
      case MPGeneralConstraintProto::kIndicatorConstraint:
        error = FindErrorInMPIndicatorConstraint(
            model, gen_constraint.indicator_constraint(), &variable_appears,
            abs_value_threshold, accept_trivially_infeasible_bounds);
        break;

      case MPGeneralConstraintProto::kSosConstraint:
        error =
            FindErrorInMPSosConstraint(model, gen_constraint.sos_constraint(),
                                       &variable_appears, abs_value_threshold);
        break;

      case MPGeneralConstraintProto::kQuadraticConstraint:
        error = FindErrorInMPQuadraticConstraint(
            model, gen_constraint.quadratic_constraint(), &variable_appears,
            abs_value_threshold, accept_trivially_infeasible_bounds);
        break;

      case MPGeneralConstraintProto::kAbsConstraint:
        error =
            FindErrorInMPAbsConstraint(model, gen_constraint.abs_constraint());
        break;

      case MPGeneralConstraintProto::kAndConstraint:
        error = FindErrorInMPAndOrConstraint(model,
                                             gen_constraint.and_constraint());
        break;

      case MPGeneralConstraintProto::kOrConstraint:
        error =
            FindErrorInMPAndOrConstraint(model, gen_constraint.or_constraint());
        break;

      case MPGeneralConstraintProto::kMinConstraint:
        error = FindErrorInMPMinMaxConstraint(
            model, gen_constraint.min_constraint(), abs_value_threshold);
        break;

      case MPGeneralConstraintProto::kMaxConstraint:
        error = FindErrorInMPMinMaxConstraint(
            model, gen_constraint.max_constraint(), abs_value_threshold);
        break;
      default:
        return absl::StrCat("Unknown general constraint type ",
                            gen_constraint.general_constraint_case());
    }
    if (!error.empty()) {
      return absl::StrCat("In general constraint #", i, ": ", error);
    }
  }

  // Validate objectives.
  if (model.has_quadratic_objective()) {
    error = FindErrorInQuadraticObjective(model.quadratic_objective(), num_vars,
                                          abs_value_threshold);
    if (!error.empty()) return absl::StrCat("In quadratic_objective: ", error);
  }

  // Validate the solution hint.
  error = FindErrorInSolutionHint(model.solution_hint(), num_vars,
                                  abs_value_threshold);
  if (!error.empty()) {
    return absl::StrCat("In solution_hint(): ", error);
  }

  return std::string();
}

absl::optional<LazyMutableCopy<MPModelProto>>
ExtractValidMPModelOrPopulateResponseStatus(const MPModelRequest& request,
                                            MPSolutionResponse* response) {
  CHECK(response != nullptr);

  if (!request.has_model() && !request.has_model_delta()) {
    response->set_status(MPSOLVER_OPTIMAL);
    response->set_status_str("Requests without model are considered OPTIMAL");
    return absl::nullopt;
  }
  if (request.has_model() && request.has_model_delta()) {
    response->set_status(MPSOLVER_MODEL_INVALID);
    response->set_status_str(
        "Fields 'model' and 'model_delta' are mutually exclusive");
    return absl::nullopt;
  }

  // Extract the baseline model.
  LazyMutableCopy<MPModelProto> model(request.model());
  if (request.has_model_delta()) {
    // NOTE(user): This library needs to be portable, so we can't include
    // ortools/base/file.h; see ../port/file.h.
    std::string contents;
    const absl::Status file_read_status = PortableFileGetContents(
        request.model_delta().baseline_model_file_path(), &contents);
    if (!file_read_status.ok()) {
      response->set_status(MPSOLVER_MODEL_INVALID);
      response->set_status_str(
          "Error when reading model_delta.baseline_model_file_path: '" +
          file_read_status.ToString());
      return absl::nullopt;
    }
    if (!model.get_mutable()->ParseFromString(contents)) {
      response->set_status(MPSOLVER_MODEL_INVALID);
      response->set_status_str(
          absl::StrFormat("The contents of baseline model file '%s' couldn't "
                          "be parsed as a raw serialized MPModelProto",
                          request.model_delta().baseline_model_file_path()));
      return absl::nullopt;
    }
  }

  // Validate the baseline model.
  std::string error = FindErrorInMPModelProto(model.get());

  // If the baseline is valid and we have a model delta, validate the delta,
  // then apply it.
  if (error.empty() && request.has_model_delta()) {
    const MPModelDeltaProto& delta = request.model_delta();
    error = FindErrorInMPModelDeltaProto(delta, model.get());
    if (error.empty()) ApplyVerifiedMPModelDelta(delta, model.get_mutable());
  }

  // Deal with errors.
  if (!error.empty()) {
    if (request.enable_internal_solver_output()) {
      LOG(ERROR) << absl::StrCat("Invalid model: ", error);
    }
    response->set_status(absl::StrContains(error, "Infeasible")
                             ? MPSOLVER_INFEASIBLE
                             : MPSOLVER_MODEL_INVALID);
    response->set_status_str(error);
    return absl::nullopt;
  }

  if (model.get().variable_size() == 0 && model.get().constraint_size() == 0 &&
      model.get().general_constraint_size() == 0) {
    response->set_status(MPSOLVER_OPTIMAL);
    response->set_objective_value(model.get().objective_offset());
    response->set_best_objective_bound(response->objective_value());
    response->set_status_str(
        "Requests without variables and constraints are considered OPTIMAL");
    return absl::nullopt;
  }

  return std::move(model);
}

bool ExtractValidMPModelInPlaceOrPopulateResponseStatus(
    MPModelRequest* request, MPSolutionResponse* response) {
  absl::optional<LazyMutableCopy<MPModelProto>> lazy_copy =
      ExtractValidMPModelOrPopulateResponseStatus(*request, response);
  if (!lazy_copy) return false;
  if (lazy_copy->was_copied()) {
    lazy_copy->get_mutable()->Swap(request->mutable_model());
  }
  return true;
}

// TODO(user): Add a general FindFeasibilityErrorInSolution() and factor out the
// common code.
std::string FindFeasibilityErrorInSolutionHint(const MPModelProto& model,
                                               double tolerance) {
  const int num_vars = model.variable_size();

  // First, we validate the solution hint.
  std::string error =
      FindErrorInSolutionHint(model.solution_hint(), num_vars,
                              absl::GetFlag(FLAGS_model_validator_infinity));
  if (!error.empty()) return absl::StrCat("Invalid solution_hint: ", error);

  // Special error message for the empty case.
  if (num_vars > 0 && model.solution_hint().var_index_size() == 0) {
    return "Empty solution_hint.";
  }

  // To be feasible, the hint must not be partial.
  if (model.solution_hint().var_index_size() != num_vars) {
    return absl::StrCat("Partial solution_hint: only ",
                        model.solution_hint().var_index_size(), " out of the ",
                        num_vars, " problem variables are set.");
  }

  // All the values must be exactly in the variable bounds.
  std::vector<double> var_value(num_vars);
  for (int i = 0; i < model.solution_hint().var_index_size(); ++i) {
    const int var_index = model.solution_hint().var_index(i);
    const double value = model.solution_hint().var_value(i);
    var_value[var_index] = value;
    const double lb = model.variable(var_index).lower_bound();
    const double ub = model.variable(var_index).upper_bound();
    if (!IsSmallerWithinTolerance(value, ub, tolerance) ||
        !IsSmallerWithinTolerance(lb, value, tolerance)) {
      return absl::StrCat("Variable '", model.variable(var_index).name(),
                          "' is set to ", (value),
                          " which is not in the variable bounds [", (lb), ", ",
                          (ub), "] modulo a tolerance of ", (tolerance), ".");
    }
  }

  // All the constraints must be satisfiable.
  for (int cst_index = 0; cst_index < model.constraint_size(); ++cst_index) {
    const MPConstraintProto& constraint = model.constraint(cst_index);
    AccurateSum<double> activity;
    for (int j = 0; j < constraint.var_index_size(); ++j) {
      activity.Add(constraint.coefficient(j) *
                   var_value[constraint.var_index(j)]);
    }
    const double lb = model.constraint(cst_index).lower_bound();
    const double ub = model.constraint(cst_index).upper_bound();
    if (!IsSmallerWithinTolerance(activity.Value(), ub, tolerance) ||
        !IsSmallerWithinTolerance(lb, activity.Value(), tolerance)) {
      return absl::StrCat(
          "Constraint '", model.constraint(cst_index).name(), "' has activity ",
          (activity.Value()), " which is not in the constraint bounds [", (lb),
          ", ", (ub), "] modulo a tolerance of ", (tolerance), ".");
    }
  }

  return "";
}

std::string FindErrorInMPModelDeltaProto(const MPModelDeltaProto& delta,
                                         const MPModelProto& model) {
  const double abs_value_threshold =
      absl::GetFlag(FLAGS_model_validator_infinity);
  int num_vars = model.variable_size();
  // Validate delta variables.
  std::string error;
  absl::flat_hash_set<int> new_var_indices;
  int max_var_index = num_vars - 1;
  MPVariableProto tmp_var_proto;
  for (const auto& pair : delta.variable_overrides()) {
    const int var_index = pair.first;
    const MPVariableProto& var_override_proto = pair.second;
    if (var_index < 0) {
      error = "Invalid key";
    } else if (var_index >= num_vars) {
      max_var_index = std::max(max_var_index, var_index);
      new_var_indices.insert(var_index);
      error =
          FindErrorInMPVariable(var_override_proto, abs_value_threshold,
                                /*accept_trivially_infeasible_bounds=*/false);
    } else {
      tmp_var_proto = model.variable(var_index);
      // NOTE(user): It is OK for the override proto to be empty, i.e. be a
      // non-override.
      tmp_var_proto.MergeFrom(var_override_proto);
      error =
          FindErrorInMPVariable(tmp_var_proto, abs_value_threshold,
                                /*accept_trivially_infeasible_bounds=*/false);
    }
    if (!error.empty()) {
      return absl::StrFormat(
          "variable_overrides with key (eg. var index) = %d: %s", var_index,
          error);
    }
  }
  if (max_var_index != num_vars + new_var_indices.size() - 1) {
    return absl::StrFormat(
        "The added and existing variable indices do not form a dense integer "
        "interval: oldmax=%d, max=%d, num added=%d",
        num_vars - 1, max_var_index, new_var_indices.size());
  }
  // Now we "officially" add the new variables to "num_vars".
  num_vars += new_var_indices.size();

  // Validate delta constraints. We can avoid going over the full
  // var_index/coefficient of the original constraint, since the overrides are
  // self-sufficient (i.e. the override var_index/coefficients are valid iff
  // they would be valid in a standalone, new constraint). So we use a partial
  // proto merger to avoid those in the baseline constraint.
  std::vector<bool> variable_appears(num_vars, false);
  MPConstraintProto tmp_constraint_proto;
  const int num_constraints = model.constraint_size();
  absl::flat_hash_set<int> new_ct_indices;
  int max_ct_index = num_constraints - 1;
  for (const auto& pair : delta.constraint_overrides()) {
    const int ct_index = pair.first;
    const MPConstraintProto& constraint_override_proto = pair.second;
    if (ct_index < 0) {
      error = "Invalid constraint index";
    } else if (ct_index >= num_constraints) {
      max_ct_index = std::max(max_ct_index, ct_index);
      new_ct_indices.insert(ct_index);
      error = FindErrorInMPConstraint(
          constraint_override_proto, &variable_appears, abs_value_threshold,
          /*accept_trivially_infeasible_bounds=*/false);
    } else {
      // NOTE(user): We don't need to do the merging of var_index/coefficient:
      // that part of the merged constraint will be valid iff the override is
      // valid as a standalone var_index/coefficient map.
      // So we simply validate a reduced version of the actual "merged"
      // constraint, by removing the var_index/coefficient of the baseline.
      // Benefit: the complexity is O(|constraint override|) even if the
      // baseline constraint was huge.
      tmp_constraint_proto.Clear();
      MergeMPConstraintProtoExceptTerms(model.constraint(ct_index),
                                        &tmp_constraint_proto);
      tmp_constraint_proto.MergeFrom(constraint_override_proto);
      error = FindErrorInMPConstraint(
          tmp_constraint_proto, &variable_appears, abs_value_threshold,
          /*accept_trivially_infeasible_bounds=*/false);
    }
    if (!error.empty()) {
      return absl::StrFormat(
          "constraint_overrides with key (eg. constraint index) = %d: %s",
          ct_index, error);
    }
  }
  if (max_ct_index != num_constraints + new_ct_indices.size() - 1) {
    return absl::StrFormat(
        "The added and existing constraint indices do not form a dense integer "
        "interval: oldmax=%d, max=%d, num added=%d",
        num_constraints - 1, max_ct_index, new_ct_indices.size());
  }

  return "";
}

void MergeMPConstraintProtoExceptTerms(const MPConstraintProto& from,
                                       MPConstraintProto* to) {
#define COPY_FIELD_IF_PRESENT(field) \
  if (from.has_##field()) to->set_##field(from.field())
  COPY_FIELD_IF_PRESENT(lower_bound);
  COPY_FIELD_IF_PRESENT(upper_bound);
  COPY_FIELD_IF_PRESENT(name);
  COPY_FIELD_IF_PRESENT(is_lazy);
#undef COPY_FIELD_IF_PRESENT
}

namespace {
void PruneZeroTermsInMpConstraint(MPConstraintProto* ct) {
  // Optimize the fast path (when no term is pruned) by doing a first quick scan
  // until the first zero.
  int first_zero = 0;
  while (first_zero < ct->var_index_size() &&
         ct->coefficient(first_zero) != 0.0) {
    ++first_zero;
  }
  int num_kept = first_zero;
  for (int i = first_zero; i < ct->var_index_size(); ++i) {
    if (ct->coefficient(i) == 0.0) continue;
    if (num_kept != i) {
      ct->set_var_index(num_kept, ct->var_index(i));
      ct->set_coefficient(num_kept, ct->coefficient(i));
    }
    ++num_kept;
  }
  ct->mutable_var_index()->Truncate(num_kept);
  ct->mutable_coefficient()->Truncate(num_kept);
}

// Adds default entries to a repeated message field until it has the wanted
// size. We don't use google::protobuf::util::Resize() because it's not
// compatible with 'light' protos.
template <class T>
void ExtendRepeatedPtrFieldToSize(const int size, T* repeated_messages) {
  DCHECK_GE(size, repeated_messages->size());
  while (repeated_messages->size() < size) repeated_messages->Add();
}
}  // namespace

void ApplyVerifiedMPModelDelta(const MPModelDeltaProto& delta,
                               MPModelProto* model) {
  // Apply the delta to the variables: first, resize the variable array.
  int max_var_index = -1;
  for (const auto& p : delta.variable_overrides()) {
    max_var_index = std::max(max_var_index, p.first);
  }
  if (max_var_index >= model->variable_size()) {
    ExtendRepeatedPtrFieldToSize(max_var_index + 1, model->mutable_variable());
  }
  // Then, apply the variable overrides.
  for (const auto& p : delta.variable_overrides()) {
    model->mutable_variable(p.first)->MergeFrom(p.second);
  }

  // Apply the delta to the constraints: first, resize the constraint array.
  int max_ct_index = -1;
  for (const auto& p : delta.constraint_overrides()) {
    max_ct_index = std::max(max_ct_index, p.first);
  }
  const int old_num_constraints = model->constraint_size();
  if (max_ct_index >= old_num_constraints) {
    ExtendRepeatedPtrFieldToSize(max_ct_index + 1, model->mutable_constraint());
  }
  // Then, apply the constraint overrides.
  for (const auto& p : delta.constraint_overrides()) {
    const MPConstraintProto& override_ct = p.second;
    MPConstraintProto* baseline = model->mutable_constraint(p.first);
    // Fast path for added constraints.
    if (p.first >= old_num_constraints) {
      *baseline = override_ct;
      continue;
    }
    MergeMPConstraintProtoExceptTerms(/*from=*/override_ct, /*to=*/baseline);
    // Special case: the override is neutralized.
    if (override_ct.has_lower_bound() &&
        override_ct.lower_bound() <=
            -absl::GetFlag(FLAGS_model_validator_infinity) &&
        override_ct.has_upper_bound() &&
        override_ct.upper_bound() >=
            absl::GetFlag(FLAGS_model_validator_infinity)) {
      baseline->clear_var_index();
      baseline->clear_coefficient();
      continue;
    }
    // Otherwise we have to apply the term overrides. We can't do that in less
    // than O(|baseline| + |override_ct|) because the baseline doesn't have a
    // lookup-friendly data structure. But we still try to do it as efficiently
    // as possible. In particular, we only use O(|override_ct|) extra memory.
    absl::flat_hash_map<int, double> term_overrides;
    term_overrides.reserve(override_ct.var_index_size());
    for (int i = 0; i < override_ct.var_index_size(); ++i) {
      term_overrides[override_ct.var_index(i)] = override_ct.coefficient(i);
    }
    for (int i = 0; i < baseline->var_index_size(); ++i) {
      auto it = term_overrides.find(baseline->var_index(i));
      if (it == term_overrides.end()) continue;
      baseline->set_coefficient(i, it->second);
      it->second = 0.0;  // To mark this term override as 'has been applied'.
    }
    PruneZeroTermsInMpConstraint(baseline);
    // Add the term overrides which haven't been used: those are added terms.
    for (const auto& p : term_overrides) {
      if (p.second != 0.0) {
        baseline->add_var_index(p.first);
        baseline->add_coefficient(p.second);
      }
    }
  }
}

}  // namespace operations_research
