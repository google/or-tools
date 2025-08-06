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

#include "ortools/sat/lp_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/strong_vector.h"
#include "ortools/glop/lp_solver.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/boolean_problem.h"
#include "ortools/sat/boolean_problem.pb.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/fp_utils.h"
#include "ortools/util/logging.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

using glop::ColIndex;
using glop::Fractional;
using glop::kInfinity;
using glop::RowIndex;

using operations_research::MPConstraintProto;
using operations_research::MPModelProto;
using operations_research::MPVariableProto;

namespace {

void ScaleConstraint(absl::Span<const double> var_scaling,
                     MPConstraintProto* mp_constraint) {
  const int num_terms = mp_constraint->coefficient_size();
  for (int i = 0; i < num_terms; ++i) {
    const int var_index = mp_constraint->var_index(i);
    mp_constraint->set_coefficient(
        i, mp_constraint->coefficient(i) / var_scaling[var_index]);
  }
}

void ApplyVarScaling(absl::Span<const double> var_scaling,
                     MPModelProto* mp_model) {
  const int num_variables = mp_model->variable_size();
  for (int i = 0; i < num_variables; ++i) {
    const double scaling = var_scaling[i];
    const MPVariableProto& mp_var = mp_model->variable(i);
    const double old_lb = mp_var.lower_bound();
    const double old_ub = mp_var.upper_bound();
    const double old_obj = mp_var.objective_coefficient();
    mp_model->mutable_variable(i)->set_lower_bound(old_lb * scaling);
    mp_model->mutable_variable(i)->set_upper_bound(old_ub * scaling);
    mp_model->mutable_variable(i)->set_objective_coefficient(old_obj / scaling);

    // TODO(user): Make bounds of integer variable integer.
  }
  for (MPConstraintProto& mp_constraint : *mp_model->mutable_constraint()) {
    ScaleConstraint(var_scaling, &mp_constraint);
  }
  for (MPGeneralConstraintProto& general_constraint :
       *mp_model->mutable_general_constraint()) {
    switch (general_constraint.general_constraint_case()) {
      case MPGeneralConstraintProto::kIndicatorConstraint:
        ScaleConstraint(var_scaling,
                        general_constraint.mutable_indicator_constraint()
                            ->mutable_constraint());
        break;
      case MPGeneralConstraintProto::kAndConstraint:
      case MPGeneralConstraintProto::kOrConstraint:
        // These constraints have only Boolean variables and no constants. They
        // don't need scaling.
        break;
      default:
        LOG(FATAL) << "Scaling unsupported for general constraint of type "
                   << general_constraint.general_constraint_case();
    }
  }
}

}  // namespace

std::vector<double> ScaleContinuousVariables(double scaling, double max_bound,
                                             MPModelProto* mp_model) {
  const int num_variables = mp_model->variable_size();
  std::vector<double> var_scaling(num_variables, 1.0);
  for (int i = 0; i < num_variables; ++i) {
    if (mp_model->variable(i).is_integer()) continue;
    if (max_bound == std::numeric_limits<double>::infinity()) {
      var_scaling[i] = scaling;
      continue;
    }
    const double lb = mp_model->variable(i).lower_bound();
    const double ub = mp_model->variable(i).upper_bound();
    const double magnitude = std::max(std::abs(lb), std::abs(ub));
    if (magnitude == 0 || magnitude > max_bound) continue;
    var_scaling[i] = std::min(scaling, max_bound / magnitude);
  }
  ApplyVarScaling(var_scaling, mp_model);
  return var_scaling;
}

// This uses the best rational approximation of x via continuous fractions.
// It is probably not the best implementation, but according to the unit test,
// it seems to do the job.
int64_t FindRationalFactor(double x, int64_t limit, double tolerance) {
  const double initial_x = x;
  x = std::abs(x);
  x -= std::floor(x);
  int64_t current_q = 1;
  int64_t prev_q = 0;
  while (current_q < limit) {
    const double q = static_cast<double>(current_q);
    const double qx = q * initial_x;
    const double qtolerance = q * tolerance;
    if (std::abs(qx - std::round(qx)) < qtolerance) {
      return current_q;
    }
    x = 1 / x;
    const double floored_x = std::floor(x);
    if (floored_x >= static_cast<double>(std::numeric_limits<int64_t>::max())) {
      return 0;
    }
    const int64_t new_q =
        CapAdd(prev_q, CapProd(static_cast<int64_t>(floored_x), current_q));
    prev_q = current_q;
    current_q = new_q;
    x -= std::floor(x);
  }
  return 0;
}

namespace {

// Returns a factor such that factor * var only need to take integer values to
// satisfy the given constraint. Return 0.0 if we didn't find such factor.
//
// Precondition: var must be the only non-integer in the given constraint.
double GetIntegralityMultiplier(const MPModelProto& mp_model,
                                absl::Span<const double> var_scaling, int var,
                                int ct_index, double tolerance) {
  DCHECK(!mp_model.variable(var).is_integer());
  const MPConstraintProto& ct = mp_model.constraint(ct_index);
  double multiplier = 1.0;
  double var_coeff = 0.0;
  const double max_multiplier = 1e4;
  for (int i = 0; i < ct.var_index().size(); ++i) {
    if (var == ct.var_index(i)) {
      var_coeff = ct.coefficient(i);
      continue;
    }

    DCHECK(mp_model.variable(ct.var_index(i)).is_integer());
    // This actually compute the smallest multiplier to make all other
    // terms in the constraint integer.
    const double coeff =
        multiplier * ct.coefficient(i) / var_scaling[ct.var_index(i)];
    multiplier *=
        FindRationalFactor(coeff, /*limit=*/100, multiplier * tolerance);
    if (multiplier == 0 || multiplier > max_multiplier) return 0.0;
  }
  DCHECK_NE(var_coeff, 0.0);

  // The constraint bound need to be infinite or integer.
  for (const double bound : {ct.lower_bound(), ct.upper_bound()}) {
    if (!std::isfinite(bound)) continue;
    if (std::abs(std::round(bound * multiplier) - bound * multiplier) >
        tolerance * multiplier) {
      return 0.0;
    }
  }
  return std::abs(multiplier * var_coeff);
}

}  // namespace

bool MakeBoundsOfIntegerVariablesInteger(const SatParameters& params,
                                         MPModelProto* mp_model,
                                         SolverLogger* logger) {
  const int num_variables = mp_model->variable_size();
  const double tolerance = params.mip_wanted_precision();
  int64_t num_changes = 0;
  for (int i = 0; i < num_variables; ++i) {
    const MPVariableProto& mp_var = mp_model->variable(i);
    if (!mp_var.is_integer()) continue;

    const double lb = mp_var.lower_bound();
    const double new_lb = std::isfinite(lb) ? std::ceil(lb - tolerance) : lb;
    if (lb != new_lb) {
      ++num_changes;
      mp_model->mutable_variable(i)->set_lower_bound(new_lb);
    }

    const double ub = mp_var.upper_bound();
    const double new_ub = std::isfinite(ub) ? std::floor(ub + tolerance) : ub;
    if (ub != new_ub) {
      ++num_changes;
      mp_model->mutable_variable(i)->set_upper_bound(new_ub);
    }

    if (new_ub < new_lb) {
      SOLVER_LOG(logger, "Empty domain for integer variable #", i, ": [", lb,
                 ",", ub, "]");
      return false;
    }
  }
  return true;
}

void ChangeLargeBoundsToInfinity(double max_magnitude, MPModelProto* mp_model,
                                 SolverLogger* logger) {
  const int num_variables = mp_model->variable_size();
  int64_t num_variable_bounds_pushed_to_infinity = 0;
  const double infinity = std::numeric_limits<double>::infinity();
  for (int i = 0; i < num_variables; ++i) {
    MPVariableProto* mp_var = mp_model->mutable_variable(i);
    const double lb = mp_var->lower_bound();
    if (std::isfinite(lb) && lb < -max_magnitude) {
      ++num_variable_bounds_pushed_to_infinity;
      mp_var->set_lower_bound(-infinity);
    }

    const double ub = mp_var->upper_bound();
    if (std::isfinite(ub) && ub > max_magnitude) {
      ++num_variable_bounds_pushed_to_infinity;
      mp_var->set_upper_bound(infinity);
    }
  }

  if (num_variable_bounds_pushed_to_infinity > 0) {
    SOLVER_LOG(logger, "Pushed ", num_variable_bounds_pushed_to_infinity,
               " variable bounds to +/-infinity");
  }

  const int num_constraints = mp_model->constraint_size();
  int64_t num_constraint_bounds_pushed_to_infinity = 0;

  for (int i = 0; i < num_constraints; ++i) {
    MPConstraintProto* mp_ct = mp_model->mutable_constraint(i);
    const double lb = mp_ct->lower_bound();
    if (std::isfinite(lb) && lb < -max_magnitude) {
      ++num_constraint_bounds_pushed_to_infinity;
      mp_ct->set_lower_bound(-infinity);
    }

    const double ub = mp_ct->upper_bound();
    if (std::isfinite(ub) && ub > max_magnitude) {
      ++num_constraint_bounds_pushed_to_infinity;
      mp_ct->set_upper_bound(infinity);
    }
  }

  for (int i = 0; i < mp_model->general_constraint_size(); ++i) {
    if (mp_model->general_constraint(i).general_constraint_case() !=
        MPGeneralConstraintProto::kIndicatorConstraint) {
      continue;
    }

    MPConstraintProto* mp_ct = mp_model->mutable_general_constraint(i)
                                   ->mutable_indicator_constraint()
                                   ->mutable_constraint();
    const double lb = mp_ct->lower_bound();
    if (std::isfinite(lb) && lb < -max_magnitude) {
      ++num_constraint_bounds_pushed_to_infinity;
      mp_ct->set_lower_bound(-infinity);
    }

    const double ub = mp_ct->upper_bound();
    if (std::isfinite(ub) && ub > max_magnitude) {
      ++num_constraint_bounds_pushed_to_infinity;
      mp_ct->set_upper_bound(infinity);
    }
  }

  if (num_constraint_bounds_pushed_to_infinity > 0) {
    SOLVER_LOG(logger, "Pushed ", num_constraint_bounds_pushed_to_infinity,
               " constraint bounds to +/-infinity");
  }
}

void RemoveNearZeroTerms(const SatParameters& params, MPModelProto* mp_model,
                         SolverLogger* logger) {
  // Having really low bounds or rhs can be problematic. We set them to zero.
  int num_dropped = 0;
  double max_dropped = 0.0;
  const double drop = params.mip_drop_tolerance();
  const int num_variables = mp_model->variable_size();
  for (int i = 0; i < num_variables; ++i) {
    MPVariableProto* var = mp_model->mutable_variable(i);
    if (var->lower_bound() != 0.0 && std::abs(var->lower_bound()) < drop) {
      ++num_dropped;
      max_dropped = std::max(max_dropped, std::abs(var->lower_bound()));
      var->set_lower_bound(0.0);
    }
    if (var->upper_bound() != 0.0 && std::abs(var->upper_bound()) < drop) {
      ++num_dropped;
      max_dropped = std::max(max_dropped, std::abs(var->upper_bound()));
      var->set_upper_bound(0.0);
    }
  }
  const int num_constraints = mp_model->constraint_size();
  for (int i = 0; i < num_constraints; ++i) {
    MPConstraintProto* ct = mp_model->mutable_constraint(i);
    if (ct->lower_bound() != 0.0 && std::abs(ct->lower_bound()) < drop) {
      ++num_dropped;
      max_dropped = std::max(max_dropped, std::abs(ct->lower_bound()));
      ct->set_lower_bound(0.0);
    }
    if (ct->upper_bound() != 0.0 && std::abs(ct->upper_bound()) < drop) {
      ++num_dropped;
      max_dropped = std::max(max_dropped, std::abs(ct->upper_bound()));
      ct->set_upper_bound(0.0);
    }
  }
  if (num_dropped > 0) {
    SOLVER_LOG(logger, "Set to zero ", num_dropped,
               " variable or constraint bounds with largest magnitude ",
               max_dropped);
  }

  // Compute for each variable its current maximum magnitude. Note that we will
  // only scale variable with a coefficient >= 1, so it is safe to use this
  // bound.
  std::vector<double> max_bounds(num_variables);
  for (int i = 0; i < num_variables; ++i) {
    double value = std::abs(mp_model->variable(i).lower_bound());
    value = std::max(value, std::abs(mp_model->variable(i).upper_bound()));
    value = std::min(value, params.mip_max_bound());
    max_bounds[i] = value;
  }

  // Note that when a variable is fixed to zero, the code here remove all its
  // coefficients. But we do not count them here.
  double largest_removed = 0.0;

  // We want the maximum absolute error while setting coefficients to zero to
  // not exceed our mip wanted precision. So for a binary variable we might set
  // to zero coefficient around 1e-7. But for large domain, we need lower coeff
  // than that, around 1e-12 with the default params.mip_max_bound(). This also
  // depends on the size of the constraint.
  int64_t num_removed = 0;
  for (int c = 0; c < num_constraints; ++c) {
    MPConstraintProto* ct = mp_model->mutable_constraint(c);
    int new_size = 0;
    const int size = ct->var_index().size();
    if (size == 0) continue;
    const double threshold =
        params.mip_wanted_precision() / static_cast<double>(size);
    for (int i = 0; i < size; ++i) {
      const int var = ct->var_index(i);
      const double coeff = ct->coefficient(i);
      if (std::abs(coeff) * max_bounds[var] < threshold) {
        if (max_bounds[var] != 0) {
          largest_removed = std::max(largest_removed, std::abs(coeff));
        }
        continue;
      }
      ct->set_var_index(new_size, var);
      ct->set_coefficient(new_size, coeff);
      ++new_size;
    }
    num_removed += size - new_size;
    ct->mutable_var_index()->Truncate(new_size);
    ct->mutable_coefficient()->Truncate(new_size);
  }

  // We also do the same for the objective coefficient.
  if (num_variables > 0) {
    const double threshold =
        params.mip_wanted_precision() / static_cast<double>(num_variables);
    for (int var = 0; var < num_variables; ++var) {
      const double coeff = mp_model->variable(var).objective_coefficient();
      if (coeff == 0.0) continue;
      if (std::abs(coeff) * max_bounds[var] < threshold) {
        ++num_removed;
        if (max_bounds[var] != 0) {
          largest_removed = std::max(largest_removed, std::abs(coeff));
        }
        mp_model->mutable_variable(var)->clear_objective_coefficient();
      }
    }
  }

  if (num_removed > 0) {
    SOLVER_LOG(logger, "Removed ", num_removed,
               " near zero terms with largest magnitude of ", largest_removed,
               ".");
  }
}

bool MPModelProtoValidationBeforeConversion(const SatParameters& params,
                                            const MPModelProto& mp_model,
                                            SolverLogger* logger) {
  // Abort if there is constraint type we don't currently support.
  for (const MPGeneralConstraintProto& general_constraint :
       mp_model.general_constraint()) {
    switch (general_constraint.general_constraint_case()) {
      case MPGeneralConstraintProto::kIndicatorConstraint:
        break;
      case MPGeneralConstraintProto::kAndConstraint:
        break;
      case MPGeneralConstraintProto::kOrConstraint:
        break;
      default:
        SOLVER_LOG(logger, "General constraints of type ",
                   general_constraint.general_constraint_case(),
                   " are not supported.");
        return false;
    }
  }

  // Abort if finite variable bounds or objective is too large.
  const double threshold = params.mip_max_valid_magnitude();
  const int num_variables = mp_model.variable_size();
  for (int i = 0; i < num_variables; ++i) {
    const MPVariableProto& var = mp_model.variable(i);
    if ((std::isfinite(var.lower_bound()) &&
         std::abs(var.lower_bound()) > threshold) ||
        (std::isfinite(var.upper_bound()) &&
         std::abs(var.upper_bound()) > threshold)) {
      SOLVER_LOG(logger, "Variable bounds are too large [", var.lower_bound(),
                 ",", var.upper_bound(), "]");
      return false;
    }
    if (std::abs(var.objective_coefficient()) > threshold) {
      SOLVER_LOG(logger, "Objective coefficient is too large: ",
                 var.objective_coefficient());
      return false;
    }
  }

  // Abort if finite constraint bounds or coefficients are too large.
  const int num_constraints = mp_model.constraint_size();
  for (int c = 0; c < num_constraints; ++c) {
    const MPConstraintProto& ct = mp_model.constraint(c);
    if ((std::isfinite(ct.lower_bound()) &&
         std::abs(ct.lower_bound()) > threshold) ||
        (std::isfinite(ct.upper_bound()) &&
         std::abs(ct.upper_bound()) > threshold)) {
      SOLVER_LOG(logger, "Constraint bounds are too large [", ct.lower_bound(),
                 ",", ct.upper_bound(), "]");
      return false;
    }
    for (const double coeff : ct.coefficient()) {
      if (std::abs(coeff) > threshold) {
        SOLVER_LOG(logger, "Constraint coefficient is too large: ", coeff);
        return false;
      }
    }
  }

  return true;
}

std::vector<double> DetectImpliedIntegers(MPModelProto* mp_model,
                                          SolverLogger* logger) {
  const int num_variables = mp_model->variable_size();
  std::vector<double> var_scaling(num_variables, 1.0);

  int initial_num_integers = 0;
  for (int i = 0; i < num_variables; ++i) {
    if (mp_model->variable(i).is_integer()) ++initial_num_integers;
  }
  VLOG(1) << "Initial num integers: " << initial_num_integers;

  // We will process all equality constraints with exactly one non-integer.
  const double tolerance = 1e-6;
  std::vector<int> constraint_queue;

  const int num_constraints = mp_model->constraint_size();
  std::vector<int> constraint_to_num_non_integer(num_constraints, 0);
  std::vector<std::vector<int>> var_to_constraints(num_variables);
  for (int i = 0; i < num_constraints; ++i) {
    const MPConstraintProto& mp_constraint = mp_model->constraint(i);

    for (const int var : mp_constraint.var_index()) {
      if (!mp_model->variable(var).is_integer()) {
        var_to_constraints[var].push_back(i);
        constraint_to_num_non_integer[i]++;
      }
    }
    if (constraint_to_num_non_integer[i] == 1) {
      constraint_queue.push_back(i);
    }
  }
  VLOG(1) << "Initial constraint queue: " << constraint_queue.size() << " / "
          << num_constraints;

  int num_detected = 0;
  double max_scaling = 0.0;
  auto scale_and_mark_as_integer = [&](int var, double scaling) mutable {
    CHECK_NE(var, -1);
    CHECK(!mp_model->variable(var).is_integer());
    CHECK_EQ(var_scaling[var], 1.0);
    if (scaling != 1.0) {
      VLOG(2) << "Scaled " << var << " by " << scaling;
    }

    ++num_detected;
    max_scaling = std::max(max_scaling, scaling);

    // Scale the variable right away and mark it as implied integer.
    // Note that the constraints will be scaled later.
    var_scaling[var] = scaling;
    mp_model->mutable_variable(var)->set_is_integer(true);

    // Update the queue of constraints with a single non-integer.
    for (const int ct_index : var_to_constraints[var]) {
      constraint_to_num_non_integer[ct_index]--;
      if (constraint_to_num_non_integer[ct_index] == 1) {
        constraint_queue.push_back(ct_index);
      }
    }
  };

  int num_fail_due_to_rhs = 0;
  int num_fail_due_to_large_multiplier = 0;
  int num_processed_constraints = 0;
  while (!constraint_queue.empty()) {
    const int top_ct_index = constraint_queue.back();
    constraint_queue.pop_back();

    // The non integer variable was already made integer by one other
    // constraint.
    if (constraint_to_num_non_integer[top_ct_index] == 0) continue;

    // Ignore non-equality here.
    const MPConstraintProto& ct = mp_model->constraint(top_ct_index);
    if (ct.lower_bound() + tolerance < ct.upper_bound()) continue;

    ++num_processed_constraints;

    // This will be set to the unique non-integer term of this constraint.
    int var = -1;
    double var_coeff;

    // We are looking for a "multiplier" so that the unique non-integer term
    // in this constraint (i.e. var * var_coeff) times this multiplier is an
    // integer.
    //
    // If this is set to zero or becomes too large, we fail to detect a new
    // implied integer and ignore this constraint.
    double multiplier = 1.0;
    const double max_multiplier = 1e4;

    for (int i = 0; i < ct.var_index().size(); ++i) {
      if (!mp_model->variable(ct.var_index(i)).is_integer()) {
        CHECK_EQ(var, -1);
        var = ct.var_index(i);
        var_coeff = ct.coefficient(i);
      } else {
        // This actually compute the smallest multiplier to make all other
        // terms in the constraint integer.
        const double coeff =
            multiplier * ct.coefficient(i) / var_scaling[ct.var_index(i)];
        multiplier *=
            FindRationalFactor(coeff, /*limit=*/100, multiplier * tolerance);
        if (multiplier == 0 || multiplier > max_multiplier) {
          break;
        }
      }
    }

    if (multiplier == 0 || multiplier > max_multiplier) {
      ++num_fail_due_to_large_multiplier;
      continue;
    }

    // These "rhs" fail could be handled by shifting the variable.
    const double rhs = ct.lower_bound();
    if (std::abs(std::round(rhs * multiplier) - rhs * multiplier) >
        tolerance * multiplier) {
      ++num_fail_due_to_rhs;
      continue;
    }

    // We want to multiply the variable so that it is integer. We know that
    // coeff * multiplier is an integer, so we just multiply by that.
    //
    // But if a variable appear in more than one equality, we want to find the
    // smallest integrality factor! See diameterc-msts-v40a100d5i.mps
    // for an instance of this.
    double best_scaling = std::abs(var_coeff * multiplier);
    for (const int ct_index : var_to_constraints[var]) {
      if (ct_index == top_ct_index) continue;
      if (constraint_to_num_non_integer[ct_index] != 1) continue;

      // Ignore non-equality here.
      const MPConstraintProto& ct = mp_model->constraint(top_ct_index);
      if (ct.lower_bound() + tolerance < ct.upper_bound()) continue;

      const double multiplier = GetIntegralityMultiplier(
          *mp_model, var_scaling, var, ct_index, tolerance);
      if (multiplier != 0.0 && multiplier < best_scaling) {
        best_scaling = multiplier;
      }
    }

    scale_and_mark_as_integer(var, best_scaling);
  }

  // Process continuous variables that only appear as the unique non integer
  // in a set of non-equality constraints.
  //
  // Note that turning to integer such variable cannot in turn trigger new
  // integer detection, so there is no point doing that in a loop.
  int num_in_inequalities = 0;
  int num_to_be_handled = 0;
  for (int var = 0; var < num_variables; ++var) {
    if (mp_model->variable(var).is_integer()) continue;

    // This should be presolved and not happen.
    if (var_to_constraints[var].empty()) continue;

    bool ok = true;
    for (const int ct_index : var_to_constraints[var]) {
      if (constraint_to_num_non_integer[ct_index] != 1) {
        ok = false;
        break;
      }
    }
    if (!ok) continue;

    std::vector<double> scaled_coeffs;
    for (const int ct_index : var_to_constraints[var]) {
      const double multiplier = GetIntegralityMultiplier(
          *mp_model, var_scaling, var, ct_index, tolerance);
      if (multiplier == 0.0) {
        ok = false;
        break;
      }
      scaled_coeffs.push_back(multiplier);
    }
    if (!ok) continue;

    // The situation is a bit tricky here, we have a bunch of coeffs c_i, and we
    // know that X * c_i can take integer value without changing the constraint
    // i meaning.
    //
    // For now we take the min, and scale only if all c_i / min are integer.
    double scaling = scaled_coeffs[0];
    for (const double c : scaled_coeffs) {
      scaling = std::min(scaling, c);
    }
    CHECK_GT(scaling, 0.0);
    for (const double c : scaled_coeffs) {
      const double fraction = c / scaling;
      if (std::abs(std::round(fraction) - fraction) > tolerance) {
        ok = false;
        break;
      }
    }
    if (!ok) {
      // TODO(user): be smarter! we should be able to handle these cases.
      ++num_to_be_handled;
      continue;
    }

    // Tricky, we also need the bound of the scaled variable to be integer.
    for (const double bound : {mp_model->variable(var).lower_bound(),
                               mp_model->variable(var).upper_bound()}) {
      if (!std::isfinite(bound)) continue;
      if (std::abs(std::round(bound * scaling) - bound * scaling) >
          tolerance * scaling) {
        ok = false;
        break;
      }
    }
    if (!ok) {
      // TODO(user): If we scale more we migth be able to turn it into an
      // integer.
      ++num_to_be_handled;
      continue;
    }

    ++num_in_inequalities;
    scale_and_mark_as_integer(var, scaling);
  }
  VLOG(1) << "num_new_integer: " << num_detected
          << " num_processed_constraints: " << num_processed_constraints
          << " num_rhs_fail: " << num_fail_due_to_rhs
          << " num_multiplier_fail: " << num_fail_due_to_large_multiplier;

  if (num_to_be_handled > 0) {
    SOLVER_LOG(logger, "Missed ", num_to_be_handled,
               " potential implied integer.");
  }

  const int num_integers = initial_num_integers + num_detected;
  SOLVER_LOG(logger, "Num integers: ", num_integers, "/", num_variables,
             " (implied: ", num_detected,
             " in_inequalities: ", num_in_inequalities,
             " max_scaling: ", max_scaling, ")",
             (num_integers == num_variables ? " [IP] " : " [MIP] "));

  ApplyVarScaling(var_scaling, mp_model);
  return var_scaling;
}

absl::Status ConstraintScaler::ScaleAndAddConstraint(
    const MPConstraintProto& mp_constraint, CpModelProto* cp_model) {
  auto* constraint = cp_model->add_constraints();
  if (keep_names) constraint->set_name(mp_constraint.name());
  auto* arg = constraint->mutable_linear();

  // First scale the coefficients of the constraints so that the constraint
  // sum can always be computed without integer overflow.
  var_indices.clear();
  coefficients.clear();
  lower_bounds.clear();
  upper_bounds.clear();
  const int num_coeffs = mp_constraint.coefficient_size();
  for (int i = 0; i < num_coeffs; ++i) {
    const auto& var_proto = cp_model->variables(mp_constraint.var_index(i));
    const int64_t lb = var_proto.domain(0);
    const int64_t ub = var_proto.domain(var_proto.domain_size() - 1);
    if (lb == 0 && ub == 0) continue;

    const double coeff = mp_constraint.coefficient(i);
    if (coeff == 0.0) continue;

    var_indices.push_back(mp_constraint.var_index(i));
    coefficients.push_back(coeff);
    lower_bounds.push_back(lb);
    upper_bounds.push_back(ub);
  }

  double relative_coeff_error;
  double scaled_sum_error;
  const double scaling_factor = FindBestScalingAndComputeErrors(
      coefficients, lower_bounds, upper_bounds, scaling_target,
      wanted_precision, &relative_coeff_error, &scaled_sum_error);
  if (scaling_factor == 0.0) {
    return absl::InvalidArgumentError(
        absl::StrCat("Scaling factor of zero while scaling constraint: ",
                     ProtobufShortDebugString(mp_constraint)));
  }

  const int64_t gcd = ComputeGcdOfRoundedDoubles(coefficients, scaling_factor);
  max_relative_coeff_error =
      std::max(relative_coeff_error, max_relative_coeff_error);
  max_scaling_factor = std::max(scaling_factor / gcd, max_scaling_factor);
  min_scaling_factor = std::min(scaling_factor / gcd, min_scaling_factor);

  for (int i = 0; i < coefficients.size(); ++i) {
    const double scaled_value = coefficients[i] * scaling_factor;
    const int64_t value = static_cast<int64_t>(std::round(scaled_value)) / gcd;
    if (value != 0) {
      arg->add_vars(var_indices[i]);
      arg->add_coeffs(value);
    }
  }
  max_absolute_rhs_error =
      std::max(max_absolute_rhs_error, scaled_sum_error / scaling_factor);

  // We relax the constraint bound by the absolute value of the wanted_precision
  // before scaling. Note that this is needed because now that the scaled
  // constraint activity is integer, we will floor/ceil these bound.
  //
  // It might make more sense to use a relative precision here for large bounds,
  // but absolute is usually what is used in the MIP world. Also if the problem
  // was a pure integer problem, and a user asked for sum == 10k, we want to
  // stay exact here.
  const Fractional lb = mp_constraint.lower_bound() - wanted_precision;
  const Fractional ub = mp_constraint.upper_bound() + wanted_precision;

  // Add the constraint bounds. Because we are sure the scaled constraint fit
  // on an int64_t, if the scaled bounds are too large, the constraint is either
  // always true or always false.
  const Fractional scaled_lb = std::ceil(lb * scaling_factor);
  if (lb == kInfinity || scaled_lb >= std::numeric_limits<int64_t>::max()) {
    // Corner case: infeasible model.
    arg->add_domain(std::numeric_limits<int64_t>::max());
  } else if (lb == -kInfinity ||
             scaled_lb <= std::numeric_limits<int64_t>::min()) {
    arg->add_domain(std::numeric_limits<int64_t>::min());
  } else {
    arg->add_domain(CeilRatio(IntegerValue(static_cast<int64_t>(scaled_lb)),
                              IntegerValue(gcd))
                        .value());
  }

  const Fractional scaled_ub = std::floor(ub * scaling_factor);
  if (ub == -kInfinity || scaled_ub <= std::numeric_limits<int64_t>::min()) {
    // Corner case: infeasible model.
    arg->add_domain(std::numeric_limits<int64_t>::min());
  } else if (ub == kInfinity ||
             scaled_ub >= std::numeric_limits<int64_t>::max()) {
    arg->add_domain(std::numeric_limits<int64_t>::max());
  } else {
    arg->add_domain(FloorRatio(IntegerValue(static_cast<int64_t>(scaled_ub)),
                               IntegerValue(gcd))
                        .value());
  }

  return absl::OkStatus();
}

namespace {

bool ConstraintIsAlwaysTrue(const MPConstraintProto& mp_constraint) {
  return mp_constraint.lower_bound() == -kInfinity &&
         mp_constraint.upper_bound() == kInfinity;
}

// TODO(user): unit test this.
double FindFractionalScaling(absl::Span<const double> coefficients,
                             double tolerance) {
  double multiplier = 1.0;
  for (const double coeff : coefficients) {
    multiplier *= FindRationalFactor(multiplier * coeff, /*limit=*/1e8,
                                     multiplier * tolerance);
    if (multiplier == 0.0) break;
  }
  return multiplier;
}

}  // namespace

double FindBestScalingAndComputeErrors(
    absl::Span<const double> coefficients,
    absl::Span<const double> lower_bounds,
    absl::Span<const double> upper_bounds, int64_t max_absolute_activity,
    double wanted_absolute_activity_precision, double* relative_coeff_error,
    double* scaled_sum_error) {
  // Starts by computing the highest possible factor.
  double scaling_factor = GetBestScalingOfDoublesToInt64(
      coefficients, lower_bounds, upper_bounds, max_absolute_activity);
  if (scaling_factor == 0.0) return scaling_factor;

  // Returns the smallest factor of the form 2^i that gives us a relative sum
  // error of wanted_absolute_activity_precision and still make sure we will
  // have no integer overflow.
  //
  // Important: the loop is written in such a way that ComputeScalingErrors()
  // is called on the last factor.
  //
  // TODO(user): Make this faster.
  double x = std::min(scaling_factor, 1.0);
  for (; x <= scaling_factor; x *= 2) {
    ComputeScalingErrors(coefficients, lower_bounds, upper_bounds, x,
                         relative_coeff_error, scaled_sum_error);
    if (*scaled_sum_error < wanted_absolute_activity_precision * x) break;

    // This could happen if we always have enough precision.
    if (x == scaling_factor) break;
  }
  scaling_factor = x;
  DCHECK(std::isfinite(scaling_factor));

  // Because we deal with an approximate input, scaling with a power of 2 might
  // not be the best choice. It is also possible user used rational coeff and
  // then converted them to double (1/2, 1/3, 4/5, etc...). This scaling will
  // recover such rational input and might result in a smaller overall
  // coefficient which is good.
  //
  // Note that if our current precisions is already above the requested one,
  // we choose integer scaling if we get a better precision.
  const double integer_factor = FindFractionalScaling(coefficients, 1e-8);
  DCHECK(std::isfinite(integer_factor));
  if (integer_factor != 0 && integer_factor < scaling_factor) {
    double local_relative_coeff_error;
    double local_scaled_sum_error;
    ComputeScalingErrors(coefficients, lower_bounds, upper_bounds,
                         integer_factor, &local_relative_coeff_error,
                         &local_scaled_sum_error);
    if (local_scaled_sum_error * scaling_factor <=
            *scaled_sum_error * integer_factor ||
        local_scaled_sum_error <
            wanted_absolute_activity_precision * integer_factor) {
      *relative_coeff_error = local_relative_coeff_error;
      *scaled_sum_error = local_scaled_sum_error;
      scaling_factor = integer_factor;
    }
  }

  DCHECK(std::isfinite(scaling_factor));
  return scaling_factor;
}

bool ConvertMPModelProtoToCpModelProto(const SatParameters& params,
                                       const MPModelProto& mp_model,
                                       CpModelProto* cp_model,
                                       SolverLogger* logger) {
  CHECK(cp_model != nullptr);
  cp_model->Clear();
  cp_model->set_name(mp_model.name());

  // To make sure we cannot have integer overflow, we use this bound for any
  // unbounded variable.
  //
  // TODO(user): This could be made larger if needed, so be smarter if we have
  // MIP problem that we cannot "convert" because of this. Note however than we
  // cannot go that much further because we need to make sure we will not run
  // into overflow if we add a big linear combination of such variables. It
  // should always be possible for a user to scale its problem so that all
  // relevant quantities are a couple of millions. A LP/MIP solver have a
  // similar condition in disguise because problem with a difference of more
  // than 6 magnitudes between the variable values will likely run into numeric
  // trouble.
  const int64_t kMaxVariableBound =
      static_cast<int64_t>(params.mip_max_bound());

  int num_truncated_bounds = 0;
  int num_small_domains = 0;
  const int64_t kSmallDomainSize = 1000;
  const double kWantedPrecision = params.mip_wanted_precision();

  // Add the variables.
  const int num_variables = mp_model.variable_size();
  const bool keep_names = !params.ignore_names();
  for (int i = 0; i < num_variables; ++i) {
    const MPVariableProto& mp_var = mp_model.variable(i);
    IntegerVariableProto* cp_var = cp_model->add_variables();
    if (keep_names) cp_var->set_name(mp_var.name());

    // Deal with the corner case of a domain far away from zero.
    //
    // TODO(user): We could avoid these cases by shifting the domain of
    // all variables to contain zero. This should also lead to a better scaling,
    // but it has some complications with integer variables and require some
    // post-solve.
    if (mp_var.lower_bound() > static_cast<double>(kMaxVariableBound) ||
        mp_var.upper_bound() < static_cast<double>(-kMaxVariableBound)) {
      SOLVER_LOG(logger, "Error: variable ", mp_var,
                 " is outside [-mip_max_bound..mip_max_bound]");
      return false;
    }

    // Note that we must process the lower bound first.
    for (const bool lower : {true, false}) {
      const double bound = lower ? mp_var.lower_bound() : mp_var.upper_bound();
      if (std::abs(bound) + kWantedPrecision >=
          static_cast<double>(kMaxVariableBound)) {
        ++num_truncated_bounds;
        cp_var->add_domain(bound < 0 ? -kMaxVariableBound : kMaxVariableBound);
        continue;
      }

      // Note that the cast is "perfect" because we forbid large values.
      cp_var->add_domain(
          static_cast<int64_t>(lower ? std::ceil(bound - kWantedPrecision)
                                     : std::floor(bound + kWantedPrecision)));
    }

    if (cp_var->domain(0) > cp_var->domain(1)) {
      LOG(WARNING) << "Variable #" << i << " cannot take integer value. "
                   << ProtobufShortDebugString(mp_var);
      return false;
    }

    // Notify if a continuous variable has a small domain as this is likely to
    // make an all integer solution far from a continuous one.
    if (!mp_var.is_integer()) {
      const double diff = mp_var.upper_bound() - mp_var.lower_bound();
      if (diff > kWantedPrecision && diff < kSmallDomainSize) {
        ++num_small_domains;
      }
    }
  }

  if (num_truncated_bounds > 0) {
    SOLVER_LOG(logger, "Warning: ", num_truncated_bounds,
               " bounds were truncated to ", kMaxVariableBound, ".");
  }
  if (num_small_domains > 0) {
    SOLVER_LOG(logger, "Warning: ", num_small_domains,
               " continuous variable domain with fewer than ", kSmallDomainSize,
               " values.");
  }

  ConstraintScaler scaler;
  const int64_t kScalingTarget = int64_t{1}
                                 << params.mip_max_activity_exponent();
  scaler.wanted_precision = kWantedPrecision;
  scaler.scaling_target = kScalingTarget;
  scaler.keep_names = keep_names;

  // Add the constraints. We scale each of them individually.
  for (const MPConstraintProto& mp_constraint : mp_model.constraint()) {
    if (ConstraintIsAlwaysTrue(mp_constraint)) continue;

    const absl::Status status =
        scaler.ScaleAndAddConstraint(mp_constraint, cp_model);
    if (!status.ok()) {
      SOLVER_LOG(logger, "Error while scaling constraint. ", status.message());
      return false;
    }
  }
  for (const MPGeneralConstraintProto& general_constraint :
       mp_model.general_constraint()) {
    switch (general_constraint.general_constraint_case()) {
      case MPGeneralConstraintProto::kIndicatorConstraint: {
        const auto& indicator_constraint =
            general_constraint.indicator_constraint();
        const MPConstraintProto& mp_constraint =
            indicator_constraint.constraint();
        if (ConstraintIsAlwaysTrue(mp_constraint)) continue;

        const int new_ct_index = cp_model->constraints().size();
        const absl::Status status =
            scaler.ScaleAndAddConstraint(mp_constraint, cp_model);
        if (!status.ok()) {
          SOLVER_LOG(logger, "Error while scaling constraint. ",
                     status.message());
          return false;
        }

        // Add the indicator.
        const int var = indicator_constraint.var_index();
        const int value = indicator_constraint.var_value();
        cp_model->mutable_constraints(new_ct_index)
            ->add_enforcement_literal(value == 1 ? var : NegatedRef(var));
        break;
      }
      case MPGeneralConstraintProto::kAndConstraint: {
        const auto& and_constraint = general_constraint.and_constraint();
        absl::string_view name = general_constraint.name();

        ConstraintProto* ct_pos = cp_model->add_constraints();
        ct_pos->set_name(name.empty() ? "" : absl::StrCat(name, "_pos"));
        ct_pos->add_enforcement_literal(and_constraint.resultant_var_index());
        *ct_pos->mutable_bool_and()->mutable_literals() =
            and_constraint.var_index();

        ConstraintProto* ct_neg = cp_model->add_constraints();
        ct_neg->set_name(name.empty() ? "" : absl::StrCat(name, "_neg"));
        ct_neg->add_enforcement_literal(
            NegatedRef(and_constraint.resultant_var_index()));
        for (const int var_index : and_constraint.var_index()) {
          ct_neg->mutable_bool_or()->add_literals(NegatedRef(var_index));
        }
        break;
      }
      case MPGeneralConstraintProto::kOrConstraint: {
        const auto& or_constraint = general_constraint.or_constraint();
        absl::string_view name = general_constraint.name();

        ConstraintProto* ct_pos = cp_model->add_constraints();
        ct_pos->set_name(name.empty() ? "" : absl::StrCat(name, "_pos"));
        ct_pos->add_enforcement_literal(or_constraint.resultant_var_index());
        *ct_pos->mutable_bool_or()->mutable_literals() =
            or_constraint.var_index();

        ConstraintProto* ct_neg = cp_model->add_constraints();
        ct_neg->set_name(name.empty() ? "" : absl::StrCat(name, "_neg"));
        ct_neg->add_enforcement_literal(
            NegatedRef(or_constraint.resultant_var_index()));
        for (const int var_index : or_constraint.var_index()) {
          ct_neg->mutable_bool_and()->add_literals(NegatedRef(var_index));
        }
        break;
      }
      default:
        LOG(ERROR) << "Can't convert general constraints of type "
                   << general_constraint.general_constraint_case()
                   << " to CpModelProto.";
        return false;
    }
  }

  // Display the error/scaling on the constraints.
  SOLVER_LOG(logger, "Maximum constraint coefficient relative error: ",
             scaler.max_relative_coeff_error);
  SOLVER_LOG(logger, "Maximum constraint worst-case activity error: ",
             scaler.max_absolute_rhs_error,
             (scaler.max_absolute_rhs_error > params.mip_check_precision()
                  ? " [Potentially IMPRECISE]"
                  : ""));
  SOLVER_LOG(logger, "Constraint scaling factor range: [",
             scaler.min_scaling_factor, ", ", scaler.max_scaling_factor, "]");

  // Since cp_model support a floating point objective, we use that. This will
  // allow us to scale the objective a bit later so we can potentially do more
  // domain reduction first.
  auto* float_objective = cp_model->mutable_floating_point_objective();
  float_objective->set_maximize(mp_model.maximize());
  float_objective->set_offset(mp_model.objective_offset());
  for (int i = 0; i < num_variables; ++i) {
    const MPVariableProto& mp_var = mp_model.variable(i);
    if (mp_var.objective_coefficient() != 0.0) {
      float_objective->add_vars(i);
      float_objective->add_coeffs(mp_var.objective_coefficient());
    }
  }

  // If the objective is fixed to zero, we consider there is none.
  if (float_objective->offset() == 0 && float_objective->vars().empty()) {
    cp_model->clear_floating_point_objective();
  }
  return true;
}

namespace {

int AppendSumOfLiteral(absl::Span<const int> literals, MPConstraintProto* out) {
  int shift = 0;
  for (const int ref : literals) {
    if (ref >= 0) {
      out->add_coefficient(1);
      out->add_var_index(ref);
    } else {
      out->add_coefficient(-1);
      out->add_var_index(PositiveRef(ref));
      ++shift;
    }
  }
  return shift;
}

}  // namespace

bool ConvertCpModelProtoToMPModelProto(const CpModelProto& input,
                                       MPModelProto* output) {
  CHECK(output != nullptr);
  output->Clear();

  // Copy variables.
  const int num_vars = input.variables().size();
  for (int v = 0; v < num_vars; ++v) {
    if (input.variables(v).domain().size() != 2) {
      VLOG(1) << "Cannot convert "
              << ProtobufShortDebugString(input.variables(v));
      return false;
    }

    MPVariableProto* var = output->add_variable();
    var->set_is_integer(true);
    var->set_lower_bound(input.variables(v).domain(0));
    var->set_upper_bound(input.variables(v).domain(1));
  }

  // Copy integer or float objective.
  if (input.has_objective()) {
    double factor = input.objective().scaling_factor();
    if (factor == 0.0) factor = 1.0;
    const int num_terms = input.objective().vars().size();
    for (int i = 0; i < num_terms; ++i) {
      const int var = input.objective().vars(i);
      if (var < 0) return false;
      CHECK_EQ(output->variable(var).objective_coefficient(), 0.0);
      output->mutable_variable(var)->set_objective_coefficient(
          factor * input.objective().coeffs(i));
    }
    output->set_objective_offset(factor * input.objective().offset());
    if (factor < 0) {
      output->set_maximize(true);
    }
  } else if (input.has_floating_point_objective()) {
    const int num_terms = input.floating_point_objective().vars().size();
    for (int i = 0; i < num_terms; ++i) {
      const int var = input.floating_point_objective().vars(i);
      if (var < 0) return false;
      CHECK_EQ(output->variable(var).objective_coefficient(), 0.0);
      output->mutable_variable(var)->set_objective_coefficient(
          input.floating_point_objective().coeffs(i));
    }
    output->set_objective_offset(input.floating_point_objective().offset());
  }
  if (output->objective_offset() == 0.0) {
    output->clear_objective_offset();
  }

  // Copy constraint.
  const int num_constraints = input.constraints().size();
  std::vector<int> tmp_literals;
  for (int c = 0; c < num_constraints; ++c) {
    const ConstraintProto& ct = input.constraints(c);
    if (!ct.enforcement_literal().empty() &&
        (ct.constraint_case() != ConstraintProto::kBoolAnd &&
         ct.constraint_case() != ConstraintProto::kLinear)) {
      // TODO(user): Support more constraints with enforcement.
      VLOG(1) << "Cannot convert constraint: " << ProtobufDebugString(ct);
      return false;
    }
    switch (ct.constraint_case()) {
      case ConstraintProto::kExactlyOne: {
        MPConstraintProto* out = output->add_constraint();
        const int shift = AppendSumOfLiteral(ct.exactly_one().literals(), out);
        out->set_lower_bound(1 - shift);
        out->set_upper_bound(1 - shift);
        break;
      }
      case ConstraintProto::kAtMostOne: {
        MPConstraintProto* out = output->add_constraint();
        const int shift = AppendSumOfLiteral(ct.at_most_one().literals(), out);
        out->set_lower_bound(-kInfinity);
        out->set_upper_bound(1 - shift);
        break;
      }
      case ConstraintProto::kBoolOr: {
        MPConstraintProto* out = output->add_constraint();
        const int shift = AppendSumOfLiteral(ct.bool_or().literals(), out);
        out->set_lower_bound(1 - shift);
        out->set_upper_bound(kInfinity);
        break;
      }
      case ConstraintProto::kBoolAnd: {
        tmp_literals.clear();
        for (const int ref : ct.enforcement_literal()) {
          tmp_literals.push_back(NegatedRef(ref));
        }
        for (const int ref : ct.bool_and().literals()) {
          MPConstraintProto* out = output->add_constraint();
          tmp_literals.push_back(ref);
          const int shift = AppendSumOfLiteral(tmp_literals, out);
          out->set_lower_bound(1 - shift);
          out->set_upper_bound(kInfinity);
          tmp_literals.pop_back();
        }
        break;
      }
      case ConstraintProto::kLinear: {
        if (ct.linear().domain().size() != 2) {
          VLOG(1) << "Cannot convert constraint: "
                  << ProtobufShortDebugString(ct);
          return false;
        }

        // Compute min/max activity.
        int64_t min_activity = 0;
        int64_t max_activity = 0;
        const int num_terms = ct.linear().vars().size();
        for (int i = 0; i < num_terms; ++i) {
          const int var = ct.linear().vars(i);
          if (var < 0) return false;
          DCHECK_EQ(input.variables(var).domain().size(), 2);
          const int64_t coeff = ct.linear().coeffs(i);
          if (coeff > 0) {
            min_activity += coeff * input.variables(var).domain(0);
            max_activity += coeff * input.variables(var).domain(1);
          } else {
            min_activity += coeff * input.variables(var).domain(1);
            max_activity += coeff * input.variables(var).domain(0);
          }
        }

        if (ct.enforcement_literal().empty()) {
          MPConstraintProto* out_ct = output->add_constraint();
          if (min_activity < ct.linear().domain(0)) {
            out_ct->set_lower_bound(ct.linear().domain(0));
          } else {
            out_ct->set_lower_bound(-kInfinity);
          }
          if (max_activity > ct.linear().domain(1)) {
            out_ct->set_upper_bound(ct.linear().domain(1));
          } else {
            out_ct->set_upper_bound(kInfinity);
          }
          for (int i = 0; i < num_terms; ++i) {
            const int var = ct.linear().vars(i);
            if (var < 0) return false;
            out_ct->add_var_index(var);
            out_ct->add_coefficient(ct.linear().coeffs(i));
          }
          break;
        }

        std::vector<MPConstraintProto*> out_cts;
        if (ct.linear().domain(1) < max_activity) {
          MPConstraintProto* high_out_ct = output->add_constraint();
          high_out_ct->set_lower_bound(-kInfinity);
          int64_t ub = ct.linear().domain(1);
          const int64_t coeff = max_activity - ct.linear().domain(1);
          for (const int lit : ct.enforcement_literal()) {
            if (RefIsPositive(lit)) {
              // term <= ub + coeff * (1 - enf);
              high_out_ct->add_var_index(lit);
              high_out_ct->add_coefficient(coeff);
              ub += coeff;
            } else {
              high_out_ct->add_var_index(PositiveRef(lit));
              high_out_ct->add_coefficient(-coeff);
            }
          }
          high_out_ct->set_upper_bound(ub);
          out_cts.push_back(high_out_ct);
        }
        if (ct.linear().domain(0) > min_activity) {
          MPConstraintProto* low_out_ct = output->add_constraint();
          low_out_ct->set_upper_bound(kInfinity);
          int64_t lb = ct.linear().domain(0);
          int64_t coeff = min_activity - ct.linear().domain(0);
          for (const int lit : ct.enforcement_literal()) {
            if (RefIsPositive(lit)) {
              // term >= lb + coeff * (1 - enf)
              low_out_ct->add_var_index(lit);
              low_out_ct->add_coefficient(coeff);
              lb += coeff;
            } else {
              low_out_ct->add_var_index(PositiveRef(lit));
              low_out_ct->add_coefficient(-coeff);
            }
          }
          low_out_ct->set_lower_bound(lb);
          out_cts.push_back(low_out_ct);
        }
        for (MPConstraintProto* out_ct : out_cts) {
          for (int i = 0; i < num_terms; ++i) {
            const int var = ct.linear().vars(i);
            if (var < 0) return false;
            out_ct->add_var_index(var);
            out_ct->add_coefficient(ct.linear().coeffs(i));
          }
        }
        break;
      }
      default:
        VLOG(1) << "Cannot convert constraint: " << ProtobufDebugString(ct);
        return false;
    }
  }

  return true;
}

bool ScaleAndSetObjective(const SatParameters& params,
                          absl::Span<const std::pair<int, double>> objective,
                          double objective_offset, bool maximize,
                          CpModelProto* cp_model, SolverLogger* logger) {
  // Make sure the objective is currently empty.
  cp_model->clear_objective();

  // We filter constant terms and compute some needed quantities.
  std::vector<int> var_indices;
  std::vector<double> coefficients;
  std::vector<double> lower_bounds;
  std::vector<double> upper_bounds;
  double min_magnitude = std::numeric_limits<double>::infinity();
  double max_magnitude = 0.0;
  double l1_norm = 0.0;
  for (const auto& [var, coeff] : objective) {
    const auto& var_proto = cp_model->variables(var);
    const int64_t lb = var_proto.domain(0);
    const int64_t ub = var_proto.domain(var_proto.domain_size() - 1);
    if (lb == ub) {
      if (lb != 0) objective_offset += lb * coeff;
      continue;
    }
    var_indices.push_back(var);
    coefficients.push_back(coeff);
    lower_bounds.push_back(lb);
    upper_bounds.push_back(ub);

    min_magnitude = std::min(min_magnitude, std::abs(coeff));
    max_magnitude = std::max(max_magnitude, std::abs(coeff));
    l1_norm += std::abs(coeff);
  }

  if (coefficients.empty() && objective_offset == 0.0) return true;

  if (!coefficients.empty()) {
    const double average_magnitude =
        l1_norm / static_cast<double>(coefficients.size());
    SOLVER_LOG(logger, "[Scaling] Floating point objective has ",
               coefficients.size(), " terms with magnitude in [", min_magnitude,
               ", ", max_magnitude, "] average = ", average_magnitude);
  }

  // These are the parameters used for scaling the objective.
  const int64_t max_absolute_activity = int64_t{1}
                                        << params.mip_max_activity_exponent();
  const double wanted_precision =
      std::max(params.mip_wanted_precision(), params.absolute_gap_limit());

  double relative_coeff_error;
  double scaled_sum_error;
  const double scaling_factor = FindBestScalingAndComputeErrors(
      coefficients, lower_bounds, upper_bounds, max_absolute_activity,
      wanted_precision, &relative_coeff_error, &scaled_sum_error);
  if (scaling_factor == 0.0) {
    LOG(ERROR) << "Scaling factor of zero while scaling objective! This "
                  "likely indicate an infinite coefficient in the objective.";
    return false;
  }

  const int64_t gcd = ComputeGcdOfRoundedDoubles(coefficients, scaling_factor);

  // Display the objective error/scaling.
  SOLVER_LOG(logger, "[Scaling] Objective coefficient relative error: ",
             relative_coeff_error);
  SOLVER_LOG(logger, "[Scaling] Objective worst-case absolute error: ",
             scaled_sum_error / scaling_factor);
  SOLVER_LOG(logger,
             "[Scaling] Objective scaling factor: ", scaling_factor / gcd);

  if (scaled_sum_error / scaling_factor > wanted_precision) {
    SOLVER_LOG(logger,
               "[Scaling] Warning: the worst-case absolute error is greater "
               "than the wanted precision (",
               wanted_precision,
               "). Try to increase mip_max_activity_exponent (default = ",
               params.mip_max_activity_exponent(),
               ") or reduced your variables range and/or objective "
               "coefficient. We will continue the solve, but the final "
               "objective value might be off.");
  }

  // Note that here we set the scaling factor for the inverse operation of
  // getting the "true" objective value from the scaled one. Hence the
  // inverse.
  auto* objective_proto = cp_model->mutable_objective();
  const int64_t mult = maximize ? -1 : 1;
  objective_proto->set_offset(objective_offset * scaling_factor / gcd * mult);
  objective_proto->set_scaling_factor(1.0 / scaling_factor * gcd * mult);
  for (int i = 0; i < coefficients.size(); ++i) {
    const int64_t value =
        static_cast<int64_t>(std::round(coefficients[i] * scaling_factor)) /
        gcd;
    if (value != 0) {
      objective_proto->add_vars(var_indices[i]);
      objective_proto->add_coeffs(value * mult);
    }
  }

  if (scaled_sum_error == 0.0) {
    objective_proto->set_scaling_was_exact(true);
  }

  return true;
}

bool ConvertBinaryMPModelProtoToBooleanProblem(const MPModelProto& mp_model,
                                               LinearBooleanProblem* problem) {
  CHECK(problem != nullptr);
  problem->Clear();
  problem->set_name(mp_model.name());
  const int num_variables = mp_model.variable_size();
  problem->set_num_variables(num_variables);

  // Test if the variables are binary variables.
  // Add constraints for the fixed variables.
  for (int var_id(0); var_id < num_variables; ++var_id) {
    const MPVariableProto& mp_var = mp_model.variable(var_id);
    problem->add_var_names(mp_var.name());

    // This will be changed to false as soon as we detect the variable to be
    // non-binary. This is done this way so we can display a nice error message
    // before aborting the function and returning false.
    bool is_binary = mp_var.is_integer();

    const Fractional lb = mp_var.lower_bound();
    const Fractional ub = mp_var.upper_bound();
    if (lb <= -1.0) is_binary = false;
    if (ub >= 2.0) is_binary = false;
    if (is_binary) {
      // 4 cases.
      if (lb <= 0.0 && ub >= 1.0) {
        // Binary variable. Ok.
      } else if (lb <= 1.0 && ub >= 1.0) {
        // Fixed variable at 1.
        LinearBooleanConstraint* constraint = problem->add_constraints();
        constraint->set_lower_bound(1);
        constraint->set_upper_bound(1);
        constraint->add_literals(var_id + 1);
        constraint->add_coefficients(1);
      } else if (lb <= 0.0 && ub >= 0.0) {
        // Fixed variable at 0.
        LinearBooleanConstraint* constraint = problem->add_constraints();
        constraint->set_lower_bound(0);
        constraint->set_upper_bound(0);
        constraint->add_literals(var_id + 1);
        constraint->add_coefficients(1);
      } else {
        // No possible integer value!
        is_binary = false;
      }
    }

    // Abort if the variable is not binary.
    if (!is_binary) {
      LOG(WARNING) << "The variable #" << var_id << " with name "
                   << mp_var.name() << " is not binary. "
                   << "lb: " << lb << " ub: " << ub;
      return false;
    }
  }

  // Variables needed to scale the double coefficients into int64_t.
  const int64_t kInt64Max = std::numeric_limits<int64_t>::max();
  double max_relative_error = 0.0;
  double max_bound_error = 0.0;
  double max_scaling_factor = 0.0;
  double relative_error = 0.0;
  double scaling_factor = 0.0;
  std::vector<double> coefficients;

  // Add all constraints.
  for (const MPConstraintProto& mp_constraint : mp_model.constraint()) {
    LinearBooleanConstraint* constraint = problem->add_constraints();
    constraint->set_name(mp_constraint.name());

    // First scale the coefficients of the constraints.
    coefficients.clear();
    const int num_coeffs = mp_constraint.coefficient_size();
    for (int i = 0; i < num_coeffs; ++i) {
      coefficients.push_back(mp_constraint.coefficient(i));
    }
    GetBestScalingOfDoublesToInt64(coefficients, kInt64Max, &scaling_factor,
                                   &relative_error);
    const int64_t gcd =
        ComputeGcdOfRoundedDoubles(coefficients, scaling_factor);
    max_relative_error = std::max(relative_error, max_relative_error);
    max_scaling_factor = std::max(scaling_factor / gcd, max_scaling_factor);

    double bound_error = 0.0;
    for (int i = 0; i < num_coeffs; ++i) {
      const double scaled_value = mp_constraint.coefficient(i) * scaling_factor;
      bound_error += std::abs(round(scaled_value) - scaled_value);
      const int64_t value = static_cast<int64_t>(round(scaled_value)) / gcd;
      if (value != 0) {
        constraint->add_literals(mp_constraint.var_index(i) + 1);
        constraint->add_coefficients(value);
      }
    }
    max_bound_error = std::max(max_bound_error, bound_error);

    // Add the bounds. Note that we do not pass them to
    // GetBestScalingOfDoublesToInt64() because we know that the sum of absolute
    // coefficients of the constraint fit on an int64_t. If one of the scaled
    // bound overflows, we don't care by how much because in this case the
    // constraint is just trivial or unsatisfiable.
    const Fractional lb = mp_constraint.lower_bound();
    if (lb != -kInfinity) {
      if (lb * scaling_factor > static_cast<double>(kInt64Max)) {
        LOG(WARNING) << "A constraint is trivially unsatisfiable.";
        return false;
      }
      if (lb * scaling_factor > -static_cast<double>(kInt64Max)) {
        // Otherwise, the constraint is not needed.
        constraint->set_lower_bound(
            static_cast<int64_t>(round(lb * scaling_factor - bound_error)) /
            gcd);
      }
    }
    const Fractional ub = mp_constraint.upper_bound();
    if (ub != kInfinity) {
      if (ub * scaling_factor < -static_cast<double>(kInt64Max)) {
        LOG(WARNING) << "A constraint is trivially unsatisfiable.";
        return false;
      }
      if (ub * scaling_factor < static_cast<double>(kInt64Max)) {
        // Otherwise, the constraint is not needed.
        constraint->set_upper_bound(
            static_cast<int64_t>(round(ub * scaling_factor + bound_error)) /
            gcd);
      }
    }
  }

  // Display the error/scaling without taking into account the objective first.
  LOG(INFO) << "Maximum constraint relative error: " << max_relative_error;
  LOG(INFO) << "Maximum constraint bound error: " << max_bound_error;
  LOG(INFO) << "Maximum constraint scaling factor: " << max_scaling_factor;

  // Add the objective.
  coefficients.clear();
  for (int var_id = 0; var_id < num_variables; ++var_id) {
    const MPVariableProto& mp_var = mp_model.variable(var_id);
    coefficients.push_back(mp_var.objective_coefficient());
  }
  GetBestScalingOfDoublesToInt64(coefficients, kInt64Max, &scaling_factor,
                                 &relative_error);
  const int64_t gcd = ComputeGcdOfRoundedDoubles(coefficients, scaling_factor);
  max_relative_error = std::max(relative_error, max_relative_error);

  // Display the objective error/scaling.
  LOG(INFO) << "objective relative error: " << relative_error;
  LOG(INFO) << "objective scaling factor: " << scaling_factor / gcd;

  LinearObjective* objective = problem->mutable_objective();
  objective->set_offset(mp_model.objective_offset() * scaling_factor / gcd);

  // Note that here we set the scaling factor for the inverse operation of
  // getting the "true" objective value from the scaled one. Hence the inverse.
  objective->set_scaling_factor(1.0 / scaling_factor * gcd);
  for (int var_id = 0; var_id < num_variables; ++var_id) {
    const MPVariableProto& mp_var = mp_model.variable(var_id);
    const int64_t value =
        static_cast<int64_t>(
            round(mp_var.objective_coefficient() * scaling_factor)) /
        gcd;
    if (value != 0) {
      objective->add_literals(var_id + 1);
      objective->add_coefficients(value);
    }
  }

  // If the problem was a maximization one, we need to modify the objective.
  if (mp_model.maximize()) ChangeOptimizationDirection(problem);

  // Test the precision of the conversion.
  const double kRelativeTolerance = 1e-8;
  if (max_relative_error > kRelativeTolerance) {
    LOG(WARNING) << "The relative error during double -> int64_t conversion "
                 << "is too high!";
    return false;
  }
  return true;
}

void ConvertBooleanProblemToLinearProgram(const LinearBooleanProblem& problem,
                                          glop::LinearProgram* lp) {
  lp->Clear();
  for (int i = 0; i < problem.num_variables(); ++i) {
    const ColIndex col = lp->CreateNewVariable();
    lp->SetVariableType(col, glop::LinearProgram::VariableType::INTEGER);
    lp->SetVariableBounds(col, 0.0, 1.0);
  }

  // Variables name are optional.
  if (problem.var_names_size() != 0) {
    CHECK_EQ(problem.var_names_size(), problem.num_variables());
    for (int i = 0; i < problem.num_variables(); ++i) {
      lp->SetVariableName(ColIndex(i), problem.var_names(i));
    }
  }

  for (const LinearBooleanConstraint& constraint : problem.constraints()) {
    const RowIndex constraint_index = lp->CreateNewConstraint();
    lp->SetConstraintName(constraint_index, constraint.name());
    double sum = 0.0;
    for (int i = 0; i < constraint.literals_size(); ++i) {
      const int literal = constraint.literals(i);
      const double coeff = constraint.coefficients(i);
      const ColIndex variable_index = ColIndex(abs(literal) - 1);
      if (literal < 0) {
        sum += coeff;
        lp->SetCoefficient(constraint_index, variable_index, -coeff);
      } else {
        lp->SetCoefficient(constraint_index, variable_index, coeff);
      }
    }
    lp->SetConstraintBounds(
        constraint_index,
        constraint.has_lower_bound() ? constraint.lower_bound() - sum
                                     : -kInfinity,
        constraint.has_upper_bound() ? constraint.upper_bound() - sum
                                     : kInfinity);
  }

  // Objective.
  {
    double sum = 0.0;
    const LinearObjective& objective = problem.objective();
    const double scaling_factor = objective.scaling_factor();
    for (int i = 0; i < objective.literals_size(); ++i) {
      const int literal = objective.literals(i);
      const double coeff =
          static_cast<double>(objective.coefficients(i)) * scaling_factor;
      const ColIndex variable_index = ColIndex(abs(literal) - 1);
      if (literal < 0) {
        sum += coeff;
        lp->SetObjectiveCoefficient(variable_index, -coeff);
      } else {
        lp->SetObjectiveCoefficient(variable_index, coeff);
      }
    }
    lp->SetObjectiveOffset((sum + objective.offset()) * scaling_factor);
    lp->SetMaximizationProblem(scaling_factor < 0);
  }

  lp->CleanUp();
}

double ComputeTrueObjectiveLowerBound(
    const CpModelProto& model_proto_with_floating_point_objective,
    const CpObjectiveProto& integer_objective,
    const int64_t inner_integer_objective_lower_bound) {
  // Create an LP with the correct variable domain.
  glop::LinearProgram lp;
  const CpModelProto& proto = model_proto_with_floating_point_objective;
  for (int i = 0; i < proto.variables().size(); ++i) {
    const auto& domain = proto.variables(i).domain();
    lp.SetVariableBounds(lp.CreateNewVariable(), static_cast<double>(domain[0]),
                         static_cast<double>(domain[domain.size() - 1]));
  }

  // Add the original problem floating point objective.
  // This is user given, so we do need to deal with duplicate entries.
  const FloatObjectiveProto& float_obj = proto.floating_point_objective();
  lp.SetObjectiveOffset(float_obj.offset());
  lp.SetMaximizationProblem(float_obj.maximize());
  for (int i = 0; i < float_obj.vars().size(); ++i) {
    const glop::ColIndex col(float_obj.vars(i));
    const double old_value = lp.objective_coefficients()[col];
    lp.SetObjectiveCoefficient(col, old_value + float_obj.coeffs(i));
  }

  // Add a single constraint "integer_objective >= lower_bound".
  const glop::RowIndex ct = lp.CreateNewConstraint();
  lp.SetConstraintBounds(
      ct, static_cast<double>(inner_integer_objective_lower_bound),
      std::numeric_limits<double>::infinity());
  for (int i = 0; i < integer_objective.vars().size(); ++i) {
    lp.SetCoefficient(ct, glop::ColIndex(integer_objective.vars(i)),
                      static_cast<double>(integer_objective.coeffs(i)));
  }

  lp.CleanUp();

  // This should be fast. However, in case of numerical difficulties, we bound
  // the number of iterations.
  glop::LPSolver solver;
  glop::GlopParameters glop_parameters;
  glop_parameters.set_max_number_of_iterations(100 * proto.variables().size());
  glop_parameters.set_change_status_to_imprecise(false);
  solver.SetParameters(glop_parameters);
  const glop::ProblemStatus& status = solver.Solve(lp);
  if (status == glop::ProblemStatus::OPTIMAL) {
    return solver.GetObjectiveValue();
  }

  // Error. Hoperfully this shouldn't happen.
  return float_obj.maximize() ? std::numeric_limits<double>::infinity()
                              : -std::numeric_limits<double>::infinity();
}

}  // namespace sat
}  // namespace operations_research
