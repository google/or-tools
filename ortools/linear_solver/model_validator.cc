// Copyright 2010-2017 Google
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

#include <cmath>
#include <limits>
#include "ortools/base/join.h"
#include "ortools/base/accurate_sum.h"
#include "ortools/util/fp_utils.h"

namespace operations_research {
namespace {
// This code is also used in the android export, which uses a lightweight proto
// library that doesn't have DebugString() nor ShortDebugString().
template <class Proto>
std::string DebugString(const Proto& proto) {
#ifdef ANDROID_JNI
  return std::string("<proto>");
#else   // ANDROID_JNI
  return proto.ShortDebugString();
#endif  // ANDROID_JNI
}

static const double kInfinity = std::numeric_limits<double>::infinity();

// Internal method to detect errors in a single variable.
std::string FindErrorInMPVariable(const MPVariableProto& variable) {
  if (std::isnan(variable.lower_bound()) ||
      std::isnan(variable.upper_bound()) ||
      variable.lower_bound() == kInfinity ||
      variable.upper_bound() == -kInfinity ||
      variable.lower_bound() > variable.upper_bound()) {
    return StrCat("Infeasible bounds: [",
                  LegacyPrecision(variable.lower_bound()), ", ",
                  LegacyPrecision(variable.upper_bound()), "]");
  }
  if (variable.is_integer() &&
      ceil(variable.lower_bound()) > floor(variable.upper_bound())) {
    return StrCat("Infeasible bounds for integer variable: [",
                  LegacyPrecision(variable.lower_bound()), ", ",
                  LegacyPrecision(variable.upper_bound()), "]",
                  " translate to the empty set");
  }
  if (!std::isfinite(variable.objective_coefficient())) {
    return StrCat("Invalid objective_coefficient: ",
                  LegacyPrecision(variable.objective_coefficient()));
  }
  return std::string();
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
    return StrCat("Infeasible bounds: [",
                  LegacyPrecision(constraint.lower_bound()), ", ",
                  LegacyPrecision(constraint.upper_bound()), "]");
  }

  // TODO(user): clarify explicitly, at least in a comment, whether we want
  // to accept empty constraints (i.e. without variables).

  const int num_vars_in_model = var_mask->size();
  const int num_vars_in_ct = constraint.var_index_size();
  const int num_coeffs_in_ct = constraint.coefficient_size();
  if (num_vars_in_ct != num_coeffs_in_ct) {
    return StrCat("var_index_size() != coefficient_size() (", num_vars_in_ct,
                  " VS ", num_coeffs_in_ct);
  }
  for (int i = 0; i < num_vars_in_ct; ++i) {
    const int var_index = constraint.var_index(i);
    if (var_index >= num_vars_in_model || var_index < 0) {
      return StrCat("var_index(", i, ")=", var_index, " is out of bounds");
    }
    const double coeff = constraint.coefficient(i);
    if (!std::isfinite(coeff)) {
      return StrCat("coefficient(", i, ")=", LegacyPrecision(coeff),
                    " is invalid");
    }
  }

  // Verify that the var_index don't have duplicates. We use "var_mask".
  int duplicate_var_index = -1;
  for (const int var_index : constraint.var_index()) {
    if ((*var_mask)[var_index]) duplicate_var_index = var_index;
    (*var_mask)[var_index] = true;
  }
  // Reset "var_mask" to all false, sparsely.
  for (const int var_index : constraint.var_index()) {
    (*var_mask)[var_index] = false;
  }
  if (duplicate_var_index >= 0) {
    return StrCat("var_index #", duplicate_var_index, " appears several times");
  }

  // We found no error, all is fine.
  return std::string();
}

std::string FindErrorInSolutionHint(const PartialVariableAssignment& solution_hint,
                               int num_vars) {
  if (solution_hint.var_index_size() != solution_hint.var_value_size()) {
    return StrCat("var_index_size() != var_value_size() [",
                  solution_hint.var_index_size(), " VS ",
                  solution_hint.var_value_size());
  }
  std::vector<bool> var_in_hint(num_vars, false);
  for (int i = 0; i < solution_hint.var_index_size(); ++i) {
    const int var_index = solution_hint.var_index(i);
    if (var_index >= num_vars || var_index < 0) {
      return StrCat("var_index(", i, ")=", var_index, " is invalid.",
                    " It must be in [0, ", num_vars, ")");
    }
    if (var_in_hint[var_index]) {
      return StrCat("Duplicate var_index = ", var_index);
    }
    var_in_hint[var_index] = true;
    if (!std::isfinite(solution_hint.var_value(i))) {
      return StrCat("var_value(", i,
                    ")=", LegacyPrecision(solution_hint.var_value(i)),
                    " is not a finite number");
    }
  }
  return std::string();
}

}  // namespace

std::string FindErrorInMPModelProto(const MPModelProto& model) {
  // TODO(user): enhance the error reporting: report several errors instead of
  // stopping at the first one.

  // TODO(user): clarify explicitly, at least in a comment, whether we
  // accept models without variables and/or constraints.

  if (!std::isfinite(model.objective_offset())) {
    return StrCat("Invalid objective_offset: ",
                  LegacyPrecision(model.objective_offset()));
  }
  const int num_vars = model.variable_size();
  const int num_cts = model.constraint_size();

  // Validate variables.
  std::string error;
  for (int i = 0; i < num_vars; ++i) {
    error = FindErrorInMPVariable(model.variable(i));
    if (!error.empty()) {
      return StrCat("In variable #", i, ": ", error, ". Variable proto: ",
                    DebugString(model.variable(i)));
    }
  }

  // Validate constraints.
  std::vector<bool> variable_appears(num_vars, false);
  const int kMaxNumVarsInPrintedConstraint = 10;
  for (int i = 0; i < num_cts; ++i) {
    const MPConstraintProto& constraint = model.constraint(i);
    error = FindErrorInMPConstraint(constraint, &variable_appears);
    if (!error.empty()) {
      // Constraint protos can be huge, theoretically. So we guard against that.
      MPConstraintProto constraint_light = constraint;
      std::string suffix_str;
      if (constraint.var_index_size() > kMaxNumVarsInPrintedConstraint) {
        constraint_light.mutable_var_index()->Truncate(
            kMaxNumVarsInPrintedConstraint);
        StrAppend(&suffix_str, " (var_index cropped; size=",
                  constraint.var_index_size(), ").");
      }
      if (constraint.coefficient_size() > kMaxNumVarsInPrintedConstraint) {
        constraint_light.mutable_coefficient()->Truncate(
            kMaxNumVarsInPrintedConstraint);
        StrAppend(&suffix_str, " (coefficient cropped; size=",
                  constraint.coefficient_size(), ").");
      }
      return StrCat("In constraint #", i, ": ", error, ". Constraint proto: ",
                    DebugString(constraint_light), suffix_str);
    }
  }

  // Validate the solution hint.
  error = FindErrorInSolutionHint(model.solution_hint(), num_vars);
  if (!error.empty()) {
    return StrCat("In solution_hint(): ", error);
  }

  return std::string();
}

// TODO(user): Add a general FindFeasibilityErrorInSolution() and factor out the
// common code.
std::string FindFeasibilityErrorInSolutionHint(const MPModelProto& model,
                                          double tolerance) {
  const int num_vars = model.variable_size();

  // First, we validate the solution hint.
  std::string error = FindErrorInSolutionHint(model.solution_hint(), num_vars);
  if (!error.empty()) return StrCat("Invalid solution_hint: ", error);

  // Special error message for the empty case.
  if (num_vars > 0 && model.solution_hint().var_index_size() == 0) {
    return "Empty solution_hint.";
  }

  // To be feasible, the hint must not be partial.
  if (model.solution_hint().var_index_size() != num_vars) {
    return StrCat("Partial solution_hint: only ",
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
      return StrCat("Variable '", model.variable(var_index).name(),
                    "' is set to ", LegacyPrecision(value),
                    " which is not in the variable bounds [",
                    LegacyPrecision(lb), ", ", LegacyPrecision(ub),
                    "] modulo a tolerance of ",
                    LegacyPrecision(tolerance), ".");
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
      return StrCat("Constraint '", model.constraint(cst_index).name(),
                    "' has activity ", LegacyPrecision(activity.Value()),
                    " which is not in the constraint bounds [",
                    LegacyPrecision(lb), ", ", LegacyPrecision(ub),
                    "] modulo a tolerance of ",
                    LegacyPrecision(tolerance), ".");
    }
  }

  return "";
}

}  // namespace operations_research
