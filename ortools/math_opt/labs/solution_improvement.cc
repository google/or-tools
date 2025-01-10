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

#include "ortools/math_opt/labs/solution_improvement.h"

#include <cmath>
#include <ios>
#include <limits>
#include <tuple>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/validators/model_validator.h"
#include "ortools/util/fp_roundtrip_conv.h"

namespace operations_research::math_opt {

namespace {

// Returns an error if:
// * the solution contains variables not in the correct model
// * or the solution does not have a value for each variable in the model
// * or some of the solution values are not finite.
absl::Status ValidateFullFiniteSolution(const Model& model,
                                        const VariableMap<double>& solution) {
  for (const auto& [v, value] : solution) {
    RETURN_IF_ERROR(model.ValidateExistingVariableOfThisModel(v));
    if (!std::isfinite(value)) {
      return util::InvalidArgumentErrorBuilder()
             << "the solution contains non-finite value " << value
             << " for variable " << v;
    }
  }
  for (const Variable v : model.SortedVariables()) {
    if (!solution.contains(v)) {
      return util::InvalidArgumentErrorBuilder()
             << "the solution does not contain a value for variable " << v;
    }
  }
  return absl::OkStatus();
}

// Returns the constraints' value based on the input full-solution.
//
// This CHECKs if the input solution does not contain values for every variable
// in the constraint.
double ConstraintValue(const LinearConstraint c,
                       const VariableMap<double>& solution) {
  const BoundedLinearExpression c_bexpr = c.AsBoundedLinearExpression();
  CHECK_EQ(c_bexpr.expression.offset(), 0.0);
  // Evaluate() CHECKs that the input solution has values for each variable.
  return c_bexpr.expression.Evaluate(solution);
}

absl::Status ValidateOptions(
    const MoveVariablesToTheirBestFeasibleValueOptions& options) {
  if (!std::isfinite(options.integrality_tolerance) ||
      options.integrality_tolerance < 0.0 ||
      options.integrality_tolerance > kMaxIntegralityTolerance) {
    return util::InvalidArgumentErrorBuilder()
           << "integrality_tolerance = "
           << RoundTripDoubleFormat(options.integrality_tolerance)
           << " is not in [0, "
           << RoundTripDoubleFormat(kMaxIntegralityTolerance) << "] range";
  }
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<VariableMap<double>> MoveVariablesToTheirBestFeasibleValue(
    const Model& model, const VariableMap<double>& input_solution,
    absl::Span<const Variable> variables,
    const MoveVariablesToTheirBestFeasibleValueOptions& options) {
  // Validate the inputs.
  {
    // TODO(b/193121090): here we build the proto as the APIs of MathOpt only
    // works with the proto and can't use the C++ Model (or ModelStorage).
    const ModelProto model_proto = model.ExportModel();
    RETURN_IF_ERROR(ValidateModel(model_proto).status()) << "invalid model";
    RETURN_IF_ERROR(ModelIsSupported(
        model_proto,
        SupportedProblemStructures{
            .integer_variables = SupportType::kSupported,
        },
        /*solver_name=*/"MoveVariablesToTheirBestFeasibleValue"));
  }
  for (const Variable v : variables) {
    RETURN_IF_ERROR(model.ValidateExistingVariableOfThisModel(v))
        << "invalid `variables`";
    if (v.lower_bound() > v.upper_bound()) {
      return util::InvalidArgumentErrorBuilder()
             << "variable " << v << " bounds ["
             << RoundTripDoubleFormat(v.lower_bound()) << ", "
             << RoundTripDoubleFormat(v.upper_bound())
             << "] integer: " << std::boolalpha << v.is_integer()
             << " are inverted";
    }
    if (RoundedLowerBound(v, options.integrality_tolerance) >
        RoundedUpperBound(v, options.integrality_tolerance)) {
      return util::InvalidArgumentErrorBuilder()
             << "integer variable " << v << " has bounds ["
             << RoundTripDoubleFormat(v.lower_bound()) << ", "
             << RoundTripDoubleFormat(v.upper_bound())
             << "] that contain no integer value";
    }
  }
  RETURN_IF_ERROR(ValidateFullFiniteSolution(model, input_solution))
      << "invalid `input_solution`";
  RETURN_IF_ERROR(ValidateOptions(options)) << "invalid `options`";

  // We maintain a solution with updated value for each variable in the order of
  // traversal.
  //
  // Invariant: values are finite.
  VariableMap<double> new_solution = input_solution;
  // We also maintain the values of each constraint in sync with the values in
  // new_solution.
  //
  // Invariant: constraints_value.at(c) == ConstraintValue(c, new_solution)
  LinearConstraintMap<double> constraint_values;
  for (const LinearConstraint c : model.LinearConstraints()) {
    constraint_values.try_emplace(c, ConstraintValue(c, new_solution));
  }
  for (const Variable v : variables) {
    const double obj_coeff = model.objective_coefficient(v);

    // The variable can't change the objective. We ignore it.
    if (obj_coeff == 0.0) continue;

    const double v_current_value = new_solution.at(v);

    // We will then compute the best bound of the variable value based on its
    // own bounds and all the constraints it belongs to. This best bound is
    // based on the sign of the objective coefficient and the objective
    // direction (min or max). The positive_v_change value tracks which
    // direction this is.
    const bool positive_v_change = model.is_maximize() == (obj_coeff > 0.0);

    // The best_v_bound is the maximum variable's value. We initialize it with
    // the variable's bounds, we use +/-inf if bounds are infinite.
    double best_v_bound =
        positive_v_change ? RoundedUpperBound(v, options.integrality_tolerance)
                          : RoundedLowerBound(v, options.integrality_tolerance);

    // Now iterate on constraints that contain the variable to find the most
    // limiting one.
    //
    // For reason explained below we also keep track of the fact that we found a
    // limiting constraint, i.e. a constraint with a finite bound in the
    // direction of improvement of v.
    bool some_constraints_are_limiting = false;
    for (const LinearConstraint& c : model.ColumnNonzeros(v)) {
      const double c_coeff = c.coefficient(v);
      // The ValidateModel() should have failed.
      CHECK(std::isfinite(c_coeff)) << v << ": " << c_coeff;

      // The variable has no influence on the constraint.
      if (c_coeff == 0.0) continue;

      // Based on the constraint coefficient's sign and the variable change
      // sign, we compute which constraint bound we need to consider.
      const bool use_constraint_upper_bound =
          (c_coeff >= 0.0) == positive_v_change;

      // If the bound is not finite, ignore this constraint.
      const double used_bound =
          use_constraint_upper_bound ? c.upper_bound() : c.lower_bound();
      if (!std::isfinite(used_bound)) continue;

      // We have one constraint with a finite bound if we reach this point.
      some_constraints_are_limiting = true;

      // Compute the bound that the constraint put on the variable.
      const double c_v_bound = [&]() {
        const double c_value = constraint_values.at(c);

        // If the constraint value is not finite (could be +/-inf or NaN due to
        // computation), we consider we can't improve the value of v.
        //
        // We could here recompute the constraint value without v and do
        // something if the value is finite but in practice it is likely to be
        // an issue.
        if (!std::isfinite(c_value)) return v_current_value;

        // If we are out of the bounds of the constraint, we can't improve v.
        //
        // Note that when use_constraint_upper_bound is false we return when
        // `c_value < used_bound`, i.e. we use < instead of <=. In practice
        // though the case c_value == used_bound is covered by the computation
        // and will return v_current_value.
        if ((c_value >= used_bound) == use_constraint_upper_bound) {
          return v_current_value;
        }
        // Can be +/-inf; see comment about some_constraints_are_limiting below.
        return v_current_value + ((used_bound - c_value) / c_coeff);
      }();

      // Update best_v_bound based on the constraint.
      if (positive_v_change) {
        best_v_bound = std::fmin(best_v_bound, c_v_bound);
      } else {
        best_v_bound = std::fmax(best_v_bound, c_v_bound);
      }
    }

    if (!std::isfinite(best_v_bound)) {
      if (some_constraints_are_limiting) {
        // Here we don't fail if constraints have finite bounds but computations
        // lead to infinite values. This typically occurs when the limiting
        // constraint has a huge bound and the variable coefficient in the
        // constraint is small. We could improve the algorithm to pick a finite
        // value for the variable that does not lead to an overflow but this is
        // non trivial.
        continue;
      }
      // If there is no limiting constraint with a finite bound and the variable
      // own bound is infinite, the model is actually unbounded.
      return util::FailedPreconditionErrorBuilder()
             << "the model is unbounded regarding variable " << v;
    }

    const double v_improved_value = [&]() {
      if (!v.is_integer()) {
        return best_v_bound;
      }
      // Make sure the value is integral for integer variables. If we have a
      // constraint limiting x <= 1.5 we want to use x = 1.
      //
      // Note that since best_v_bound is finite, floor or ceil also are.
      return positive_v_change ? std::floor(best_v_bound)
                               : std::ceil(best_v_bound);
    }();

    // If we have find no improvement; skip this variable.
    if (positive_v_change ? v_improved_value <= v_current_value
                          : v_improved_value >= v_current_value)
      continue;

    // Apply the change to new_solution.
    //
    // As v_improved_value is finite the invariant holds.
    new_solution.at(v) = v_improved_value;
    // Restore the invariant of constraint_values based on the new_solution.
    for (const LinearConstraint& c : model.ColumnNonzeros(v)) {
      // Here we could incrementally update values based on the change of
      // new_solution.at(v) and the coefficient for (c, v). But since we are
      // doing floating point computation, we may introduce some errors for each
      // variable being changed. It is easier to recompute the constraints
      // values from scratch instead.
      constraint_values.at(c) = ConstraintValue(c, new_solution);
    }
  }

  return new_solution;
}

}  // namespace operations_research::math_opt
