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

#include "ortools/linear_solver/model_validator.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "ortools/base/accurate_sum.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/port/proto_utils.h"
#include "ortools/util/fp_utils.h"

namespace operations_research {
namespace {

static const double kInfinity = std::numeric_limits<double>::infinity();

// Internal method to detect errors in a single variable.
std::string FindErrorInMPVariable(const MPVariableProto& variable) {
  if (std::isnan(variable.lower_bound()) ||
      std::isnan(variable.upper_bound()) ||
      variable.lower_bound() == kInfinity ||
      variable.upper_bound() == -kInfinity ||
      variable.lower_bound() > variable.upper_bound()) {
    return absl::StrCat("Infeasible bounds: [", (variable.lower_bound()), ", ",
                        (variable.upper_bound()), "]");
  }
  if (variable.is_integer() &&
      ceil(variable.lower_bound()) > floor(variable.upper_bound())) {
    return absl::StrCat(
        "Infeasible bounds for integer variable: [", (variable.lower_bound()),
        ", ", (variable.upper_bound()), "]", " translate to the empty set");
  }
  if (!std::isfinite(variable.objective_coefficient())) {
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
// "var_mask" is a std::vector<bool> whose size is the number of variables in
// the model, and it will be all set to false before and after the call.
std::string FindErrorInMPConstraint(const MPConstraintProto& constraint,
                                    std::vector<bool>* var_mask) {
  if (std::isnan(constraint.lower_bound()) ||
      std::isnan(constraint.upper_bound()) ||
      constraint.lower_bound() == kInfinity ||
      constraint.upper_bound() == -kInfinity ||
      constraint.lower_bound() > constraint.upper_bound()) {
    return absl::StrCat("Infeasible bounds: [", (constraint.lower_bound()),
                        ", ", (constraint.upper_bound()), "]");
  }

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
    if (!std::isfinite(coeff)) {
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

std::string FindErrorInMPIndicatorConstraint(
    const MPModelProto& model, const MPIndicatorConstraint& indicator,
    std::vector<bool>* var_mask) {
  if (!indicator.has_var_index()) {
    return "var_index is required.";
  }
  const int var_index = indicator.var_index();
  if (var_index < 0 || var_index >= model.variable_size()) {
    return absl::StrCat("var_index=", var_index, " is out of bounds.");
  }
  if (!model.variable(var_index).is_integer() ||
      model.variable(var_index).lower_bound() < 0 ||
      model.variable(var_index).upper_bound() > 1) {
    return absl::StrCat("var_index=", var_index, " is not Boolean.");
  }
  const int var_value = indicator.var_value();
  if (var_value < 0 || var_value > 1) {
    return absl::StrCat("var_value=", var_value, " must be 0 or 1.");
  }
  const MPConstraintProto& constraint = indicator.constraint();
  std::string error = FindErrorInMPConstraint(constraint, var_mask);
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
                                       std::vector<bool>* var_mask) {
  if (sos.weight_size() != 0 && sos.weight_size() != sos.var_index_size()) {
    return "weight_size() > 0 and var_index_size() != weight_size()";
  }
  for (const int var_index : sos.var_index()) {
    if (var_index < 0 || var_index >= model.variable_size()) {
      return absl::StrCat("var_index=", var_index, " is out of bounds.");
    }
  }
  for (int i = 0; i < sos.weight_size(); ++i) {
    if (!std::isfinite(sos.weight(i))) {
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

std::string FindErrorInMPQuadraticConstraint(const MPModelProto& model,
                                             const MPQuadraticConstraint& qcst,
                                             std::vector<bool>* var_mask) {
  const int num_vars = model.variable_size();

  if (qcst.var_index_size() != qcst.coefficient_size()) {
    return "var_index_size() != coefficient_size()";
  }
  for (int i = 0; i < qcst.var_index_size(); ++i) {
    if (qcst.var_index(i) < 0 || qcst.var_index(i) >= model.variable_size()) {
      return absl::StrCat("var_index(", i, ")=", qcst.var_index(i),
                          " is invalid.", " It must be in [0, ", num_vars, ")");
    }

    if (!std::isfinite(qcst.coefficient(i))) {
      return absl::StrCat("coefficient(", i, ")=", qcst.coefficient(i),
                          " is invalid");
    }
  }

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

    if (!std::isfinite(qcst.qcoefficient(i))) {
      return absl::StrCat("qcoefficient(", i, ")=", qcst.qcoefficient(i),
                          " is invalid");
    }
  }

  return "";
}

std::string FindErrorInQuadraticObjective(const MPQuadraticObjective& qobj,
                                          int num_vars) {
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

    if (!std::isfinite(qobj.coefficient(i))) {
      return absl::StrCat("coefficient(", i, ")=", (qobj.coefficient(i)),
                          " is invalid");
    }
  }
  return "";
}

std::string FindErrorInSolutionHint(
    const PartialVariableAssignment& solution_hint, int num_vars) {
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
    if (!std::isfinite(solution_hint.var_value(i))) {
      return absl::StrCat("var_value(", i, ")=", (solution_hint.var_value(i)),
                          " is not a finite number");
    }
  }
  return std::string();
}
}  // namespace

std::string FindErrorInMPModelProto(const MPModelProto& model) {
  // NOTE(user): Empty models are considered fine by this function, although
  // it is not clear whether MPSolver::Solve() will always respond in the same
  // way, depending on the solvers.

  if (!std::isfinite(model.objective_offset())) {
    return absl::StrCat("Invalid objective_offset: ",
                        (model.objective_offset()));
  }
  const int num_vars = model.variable_size();
  const int num_cts = model.constraint_size();

  // Validate variables.
  std::string error;
  for (int i = 0; i < num_vars; ++i) {
    error = FindErrorInMPVariable(model.variable(i));
    if (!error.empty()) {
      return absl::StrCat("In variable #", i, ": ", error, ". Variable proto: ",
                          ProtobufShortDebugString(model.variable(i)));
    }
  }

  // Validate constraints.
  std::vector<bool> variable_appears(num_vars, false);
  for (int i = 0; i < num_cts; ++i) {
    const MPConstraintProto& constraint = model.constraint(i);
    error = FindErrorInMPConstraint(constraint, &variable_appears);
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
            model, gen_constraint.indicator_constraint(), &variable_appears);
        break;

      case MPGeneralConstraintProto::kSosConstraint:
        error = FindErrorInMPSosConstraint(
            model, gen_constraint.sos_constraint(), &variable_appears);
        break;

      case MPGeneralConstraintProto::kQuadraticConstraint:
        error = FindErrorInMPQuadraticConstraint(
            model, gen_constraint.quadratic_constraint(), &variable_appears);
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
    error =
        FindErrorInQuadraticObjective(model.quadratic_objective(), num_vars);
    if (!error.empty()) return absl::StrCat("In quadratic_objective: ", error);
  }

  // Validate the solution hint.
  error = FindErrorInSolutionHint(model.solution_hint(), num_vars);
  if (!error.empty()) {
    return absl::StrCat("In solution_hint(): ", error);
  }

  return std::string();
}

bool MPRequestIsEmptyOrInvalid(const MPModelRequest& request,
                               MPSolutionResponse* response) {
  CHECK(response != nullptr);

  if (!request.has_model()) {
    response->set_status(MPSOLVER_OPTIMAL);
    response->set_status_str("Requests without model are considered OPTIMAL");
    return true;
  }
  const MPModelProto& model = request.model();
  if (model.variable_size() == 0 && model.constraint_size() == 0 &&
      model.general_constraint_size() == 0) {
    response->set_status(MPSOLVER_OPTIMAL);
    response->set_objective_value(request.model().objective_offset());
    response->set_best_objective_bound(request.model().objective_offset());
    response->set_status_str(
        "Requests without variables and constraints are considered OPTIMAL");
    return true;
  }

  const std::string error = FindErrorInMPModelProto(model);
  if (!error.empty()) {
    if (request.enable_internal_solver_output()) {
      LOG(ERROR) << absl::StrCat("Invalid model: ", error);
    }
    response->set_status(error.find("Infeasible") == std::string::npos
                             ? MPSOLVER_MODEL_INVALID
                             : MPSOLVER_INFEASIBLE);
    response->set_status_str(error);
    return true;
  }
  return false;
}

// TODO(user): Add a general FindFeasibilityErrorInSolution() and factor out the
// common code.
std::string FindFeasibilityErrorInSolutionHint(const MPModelProto& model,
                                               double tolerance) {
  const int num_vars = model.variable_size();

  // First, we validate the solution hint.
  std::string error = FindErrorInSolutionHint(model.solution_hint(), num_vars);
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
      error = FindErrorInMPVariable(var_override_proto);
    } else {
      tmp_var_proto = model.variable(var_index);
      // NOTE(user): It is OK for the override proto to be empty, i.e. be a
      // non-override.
      tmp_var_proto.MergeFrom(var_override_proto);
      error = FindErrorInMPVariable(tmp_var_proto);
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
      error =
          FindErrorInMPConstraint(constraint_override_proto, &variable_appears);
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
      error = FindErrorInMPConstraint(tmp_constraint_proto, &variable_appears);
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

}  // namespace operations_research
