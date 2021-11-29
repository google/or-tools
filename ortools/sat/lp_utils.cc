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

#include "ortools/sat/lp_utils.h"

#include <stdlib.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "ortools/base/int_type.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/glop/lp_solver.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/boolean_problem.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/fp_utils.h"

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

void ScaleConstraint(const std::vector<double>& var_scaling,
                     MPConstraintProto* mp_constraint) {
  const int num_terms = mp_constraint->coefficient_size();
  for (int i = 0; i < num_terms; ++i) {
    const int var_index = mp_constraint->var_index(i);
    mp_constraint->set_coefficient(
        i, mp_constraint->coefficient(i) / var_scaling[var_index]);
  }
}

void ApplyVarScaling(const std::vector<double> var_scaling,
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
    const double lb = mp_model->variable(i).lower_bound();
    const double ub = mp_model->variable(i).upper_bound();
    const double magnitude = std::max(std::abs(lb), std::abs(ub));
    if (magnitude == 0 || magnitude > max_bound) continue;
    var_scaling[i] = std::min(scaling, max_bound / magnitude);
  }
  ApplyVarScaling(var_scaling, mp_model);
  return var_scaling;
}

// This uses the best rational approximation of x via continuous fractions. It
// is probably not the best implementation, but according to the unit test, it
// seems to do the job.
int FindRationalFactor(double x, int limit, double tolerance) {
  const double initial_x = x;
  x = std::abs(x);
  x -= std::floor(x);
  int q = 1;
  int prev_q = 0;
  while (q < limit) {
    if (std::abs(q * initial_x - std::round(q * initial_x)) < q * tolerance) {
      return q;
    }
    x = 1 / x;
    const int new_q = prev_q + static_cast<int>(std::floor(x)) * q;
    prev_q = q;
    q = new_q;
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
                                const std::vector<double>& var_scaling, int var,
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

void RemoveNearZeroTerms(const SatParameters& params, MPModelProto* mp_model,
                         SolverLogger* logger) {
  const int num_variables = mp_model->variable_size();

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

  // We want the maximum absolute error while setting coefficients to zero to
  // not exceed our mip wanted precision. So for a binary variable we might set
  // to zero coefficient around 1e-7. But for large domain, we need lower coeff
  // than that, around 1e-12 with the default params.mip_max_bound(). This also
  // depends on the size of the constraint.
  int64_t num_removed = 0;
  double largest_removed = 0.0;
  const int num_constraints = mp_model->constraint_size();
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
        largest_removed = std::max(largest_removed, std::abs(coeff));
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

  if (num_removed > 0) {
    SOLVER_LOG(logger, "Removed ", num_removed,
               " near zero terms with largest magnitude of ", largest_removed,
               ".");
  }
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

namespace {

// We use a class to reuse the temporary memory.
struct ConstraintScaler {
  // Scales an individual constraint.
  ConstraintProto* AddConstraint(const MPModelProto& mp_model,
                                 const MPConstraintProto& mp_constraint,
                                 CpModelProto* cp_model);

  double max_relative_coeff_error = 0.0;
  double max_relative_rhs_error = 0.0;
  double max_scaling_factor = 0.0;

  double wanted_precision = 1e-6;
  int64_t scaling_target = int64_t{1} << 50;
  std::vector<int> var_indices;
  std::vector<double> coefficients;
  std::vector<double> lower_bounds;
  std::vector<double> upper_bounds;
};

namespace {

double FindFractionalScaling(const std::vector<double>& coefficients,
                             double tolerance) {
  double multiplier = 1.0;
  for (const double coeff : coefficients) {
    multiplier *=
        FindRationalFactor(coeff, /*limit=*/1e8, multiplier * tolerance);
    if (multiplier == 0.0) break;
  }
  return multiplier;
}

}  // namespace

ConstraintProto* ConstraintScaler::AddConstraint(
    const MPModelProto& mp_model, const MPConstraintProto& mp_constraint,
    CpModelProto* cp_model) {
  if (mp_constraint.lower_bound() == -kInfinity &&
      mp_constraint.upper_bound() == kInfinity) {
    return nullptr;
  }

  auto* constraint = cp_model->add_constraints();
  constraint->set_name(mp_constraint.name());
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

  // We compute the worst case error relative to the magnitude of the bounds.
  Fractional lb = mp_constraint.lower_bound();
  Fractional ub = mp_constraint.upper_bound();
  const double ct_norm = std::max(1.0, std::min(std::abs(lb), std::abs(ub)));

  double scaling_factor = GetBestScalingOfDoublesToInt64(
      coefficients, lower_bounds, upper_bounds, scaling_target);
  if (scaling_factor == 0.0) {
    // TODO(user): Report error properly instead of ignoring constraint. Note
    // however that this likely indicate a coefficient of inf in the constraint,
    // so we should probably abort before reaching here.
    LOG(DFATAL) << "Scaling factor of zero while scaling constraint: "
                << mp_constraint.ShortDebugString();
    return nullptr;
  }

  // Returns the smallest factor of the form 2^i that gives us a relative sum
  // error of wanted_precision and still make sure we will have no integer
  // overflow.
  //
  // TODO(user): Make this faster.
  double x = std::min(scaling_factor, 1.0);
  double relative_coeff_error;
  double scaled_sum_error;
  for (; x <= scaling_factor; x *= 2) {
    ComputeScalingErrors(coefficients, lower_bounds, upper_bounds, x,
                         &relative_coeff_error, &scaled_sum_error);
    if (scaled_sum_error < wanted_precision * x * ct_norm) break;
  }
  scaling_factor = x;

  // Because we deal with an approximate input, scaling with a power of 2 might
  // not be the best choice. It is also possible user used rational coeff and
  // then converted them to double (1/2, 1/3, 4/5, etc...). This scaling will
  // recover such rational input and might result in a smaller overall
  // coefficient which is good.
  const double integer_factor = FindFractionalScaling(coefficients, 1e-8);
  if (integer_factor != 0 && integer_factor < scaling_factor) {
    ComputeScalingErrors(coefficients, lower_bounds, upper_bounds, x,
                         &relative_coeff_error, &scaled_sum_error);
    if (scaled_sum_error < wanted_precision * integer_factor * ct_norm) {
      scaling_factor = integer_factor;
    }
  }

  const int64_t gcd = ComputeGcdOfRoundedDoubles(coefficients, scaling_factor);
  max_relative_coeff_error =
      std::max(relative_coeff_error, max_relative_coeff_error);
  max_scaling_factor = std::max(scaling_factor / gcd, max_scaling_factor);

  // We do not relax the constraint bound if all variables are integer and
  // we made no error at all during our scaling.
  bool relax_bound = scaled_sum_error > 0;

  for (int i = 0; i < coefficients.size(); ++i) {
    const double scaled_value = coefficients[i] * scaling_factor;
    const int64_t value = static_cast<int64_t>(std::round(scaled_value)) / gcd;
    if (value != 0) {
      if (!mp_model.variable(var_indices[i]).is_integer()) {
        relax_bound = true;
      }
      arg->add_vars(var_indices[i]);
      arg->add_coeffs(value);
    }
  }
  max_relative_rhs_error = std::max(
      max_relative_rhs_error, scaled_sum_error / (scaling_factor * ct_norm));

  // Add the constraint bounds. Because we are sure the scaled constraint fit
  // on an int64_t, if the scaled bounds are too large, the constraint is either
  // always true or always false.
  if (relax_bound) {
    lb -= std::max(std::abs(lb), 1.0) * wanted_precision;
  }
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

  if (relax_bound) {
    ub += std::max(std::abs(ub), 1.0) * wanted_precision;
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

  return constraint;
}

}  // namespace

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
  for (int i = 0; i < num_variables; ++i) {
    const MPVariableProto& mp_var = mp_model.variable(i);
    IntegerVariableProto* cp_var = cp_model->add_variables();
    cp_var->set_name(mp_var.name());

    // Deal with the corner case of a domain far away from zero.
    //
    // TODO(user): We should deal with these case by shifting the domain so
    // that it includes zero instead of just fixing the variable. But that is a
    // bit of work as it requires some postsolve.
    if (mp_var.lower_bound() > kMaxVariableBound) {
      // Fix var to its lower bound.
      ++num_truncated_bounds;
      const int64_t value =
          static_cast<int64_t>(std::round(mp_var.lower_bound()));
      cp_var->add_domain(value);
      cp_var->add_domain(value);
      continue;
    } else if (mp_var.upper_bound() < -kMaxVariableBound) {
      // Fix var to its upper_bound.
      ++num_truncated_bounds;
      const int64_t value =
          static_cast<int64_t>(std::round(mp_var.upper_bound()));
      cp_var->add_domain(value);
      cp_var->add_domain(value);
      continue;
    }

    // Note that we must process the lower bound first.
    for (const bool lower : {true, false}) {
      const double bound = lower ? mp_var.lower_bound() : mp_var.upper_bound();
      if (std::abs(bound) >= kMaxVariableBound) {
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
                   << mp_var.ShortDebugString();
      return false;
    }

    // Notify if a continuous variable has a small domain as this is likely to
    // make an all integer solution far from a continuous one.
    if (!mp_var.is_integer() && cp_var->domain(0) != cp_var->domain(1) &&
        cp_var->domain(1) - cp_var->domain(0) < kSmallDomainSize) {
      ++num_small_domains;
    }
  }

  LOG_IF(WARNING, num_truncated_bounds > 0)
      << num_truncated_bounds << " bounds were truncated to "
      << kMaxVariableBound << ".";
  LOG_IF(WARNING, num_small_domains > 0)
      << num_small_domains << " continuous variable domain with fewer than "
      << kSmallDomainSize << " values.";

  ConstraintScaler scaler;
  const int64_t kScalingTarget = int64_t{1}
                                 << params.mip_max_activity_exponent();
  scaler.wanted_precision = kWantedPrecision;
  scaler.scaling_target = kScalingTarget;

  // Add the constraints. We scale each of them individually.
  for (const MPConstraintProto& mp_constraint : mp_model.constraint()) {
    scaler.AddConstraint(mp_model, mp_constraint, cp_model);
  }
  for (const MPGeneralConstraintProto& general_constraint :
       mp_model.general_constraint()) {
    switch (general_constraint.general_constraint_case()) {
      case MPGeneralConstraintProto::kIndicatorConstraint: {
        const auto& indicator_constraint =
            general_constraint.indicator_constraint();
        const MPConstraintProto& mp_constraint =
            indicator_constraint.constraint();
        ConstraintProto* ct =
            scaler.AddConstraint(mp_model, mp_constraint, cp_model);
        if (ct == nullptr) continue;

        // Add the indicator.
        const int var = indicator_constraint.var_index();
        const int value = indicator_constraint.var_value();
        ct->add_enforcement_literal(value == 1 ? var : NegatedRef(var));
        break;
      }
      case MPGeneralConstraintProto::kAndConstraint: {
        const auto& and_constraint = general_constraint.and_constraint();
        const std::string& name = general_constraint.name();

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
        const std::string& name = general_constraint.name();

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
  SOLVER_LOG(logger, "Maximum constraint worst-case activity relative error: ",
             scaler.max_relative_rhs_error,
             (scaler.max_relative_rhs_error > params.mip_check_precision()
                  ? " [Potentially IMPRECISE]"
                  : ""));
  SOLVER_LOG(logger,
             "Maximum constraint scaling factor: ", scaler.max_scaling_factor);

  // Add the objective.
  std::vector<int> var_indices;
  std::vector<double> coefficients;
  std::vector<double> lower_bounds;
  std::vector<double> upper_bounds;
  double min_magnitude = std::numeric_limits<double>::infinity();
  double max_magnitude = 0.0;
  double l1_norm = 0.0;
  for (int i = 0; i < num_variables; ++i) {
    const MPVariableProto& mp_var = mp_model.variable(i);
    if (mp_var.objective_coefficient() == 0.0) continue;

    const auto& var_proto = cp_model->variables(i);
    const int64_t lb = var_proto.domain(0);
    const int64_t ub = var_proto.domain(var_proto.domain_size() - 1);
    if (lb == 0 && ub == 0) continue;

    var_indices.push_back(i);
    coefficients.push_back(mp_var.objective_coefficient());
    lower_bounds.push_back(lb);
    upper_bounds.push_back(ub);
    min_magnitude = std::min(min_magnitude, std::abs(coefficients.back()));
    max_magnitude = std::max(max_magnitude, std::abs(coefficients.back()));
    l1_norm += std::abs(coefficients.back());
  }
  if (!coefficients.empty()) {
    const double average_magnitude =
        l1_norm / static_cast<double>(coefficients.size());
    SOLVER_LOG(logger, "Objective magnitude in [", min_magnitude, ", ",
               max_magnitude, "] average = ", average_magnitude);
  }
  if (!coefficients.empty() || mp_model.objective_offset() != 0.0) {
    double scaling_factor = GetBestScalingOfDoublesToInt64(
        coefficients, lower_bounds, upper_bounds, kScalingTarget);
    if (scaling_factor == 0.0) {
      LOG(ERROR) << "Scaling factor of zero while scaling objective! This "
                    "likely indicate an infinite coefficient in the objective.";
      return false;
    }

    // Returns the smallest factor of the form 2^i that gives us an absolute
    // error of kWantedPrecision and still make sure we will have no integer
    // overflow.
    //
    // TODO(user): Make this faster.
    double x = std::min(scaling_factor, 1.0);
    double relative_coeff_error;
    double scaled_sum_error;
    for (; x <= scaling_factor; x *= 2) {
      ComputeScalingErrors(coefficients, lower_bounds, upper_bounds, x,
                           &relative_coeff_error, &scaled_sum_error);
      if (scaled_sum_error < kWantedPrecision * x) break;
    }
    scaling_factor = x;

    // Same remark as for the constraint.
    // TODO(user): Extract common code.
    const double integer_factor = FindFractionalScaling(coefficients, 1e-8);
    if (integer_factor != 0 && integer_factor < scaling_factor) {
      ComputeScalingErrors(coefficients, lower_bounds, upper_bounds, x,
                           &relative_coeff_error, &scaled_sum_error);
      if (scaled_sum_error < kWantedPrecision * integer_factor) {
        scaling_factor = integer_factor;
      }
    }

    const int64_t gcd =
        ComputeGcdOfRoundedDoubles(coefficients, scaling_factor);

    // Display the objective error/scaling.
    SOLVER_LOG(
        logger, "Objective coefficient relative error: ", relative_coeff_error,
        (relative_coeff_error > params.mip_check_precision() ? " [IMPRECISE]"
                                                             : ""));
    SOLVER_LOG(logger, "Objective worst-case absolute error: ",
               scaled_sum_error / scaling_factor);
    SOLVER_LOG(logger, "Objective scaling factor: ", scaling_factor / gcd);

    // Note that here we set the scaling factor for the inverse operation of
    // getting the "true" objective value from the scaled one. Hence the
    // inverse.
    auto* objective = cp_model->mutable_objective();
    const int mult = mp_model.maximize() ? -1 : 1;
    objective->set_offset(mp_model.objective_offset() * scaling_factor / gcd *
                          mult);
    objective->set_scaling_factor(1.0 / scaling_factor * gcd * mult);
    for (int i = 0; i < coefficients.size(); ++i) {
      const int64_t value =
          static_cast<int64_t>(std::round(coefficients[i] * scaling_factor)) /
          gcd;
      if (value != 0) {
        objective->add_vars(var_indices[i]);
        objective->add_coeffs(value * mult);
      }
    }
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

int FixVariablesFromSat(const SatSolver& solver, glop::LinearProgram* lp) {
  int num_fixed_variables = 0;
  const Trail& trail = solver.LiteralTrail();
  for (int i = 0; i < trail.Index(); ++i) {
    const BooleanVariable var = trail[i].Variable();
    const int value = trail[i].IsPositive() ? 1.0 : 0.0;
    if (trail.Info(var).level == 0) {
      ++num_fixed_variables;
      lp->SetVariableBounds(ColIndex(var.value()), value, value);
    }
  }
  return num_fixed_variables;
}

bool SolveLpAndUseSolutionForSatAssignmentPreference(
    const glop::LinearProgram& lp, SatSolver* sat_solver,
    double max_time_in_seconds) {
  glop::LPSolver solver;
  glop::GlopParameters glop_parameters;
  glop_parameters.set_max_time_in_seconds(max_time_in_seconds);
  solver.SetParameters(glop_parameters);
  const glop::ProblemStatus& status = solver.Solve(lp);
  if (status != glop::ProblemStatus::OPTIMAL &&
      status != glop::ProblemStatus::IMPRECISE &&
      status != glop::ProblemStatus::PRIMAL_FEASIBLE) {
    return false;
  }
  for (ColIndex col(0); col < lp.num_variables(); ++col) {
    const Fractional& value = solver.variable_values()[col];
    sat_solver->SetAssignmentPreference(
        Literal(BooleanVariable(col.value()), round(value) == 1),
        1 - std::abs(value - round(value)));
  }
  return true;
}

bool SolveLpAndUseIntegerVariableToStartLNS(const glop::LinearProgram& lp,
                                            LinearBooleanProblem* problem) {
  glop::LPSolver solver;
  const glop::ProblemStatus& status = solver.Solve(lp);
  if (status != glop::ProblemStatus::OPTIMAL &&
      status != glop::ProblemStatus::PRIMAL_FEASIBLE)
    return false;
  int num_variable_fixed = 0;
  for (ColIndex col(0); col < lp.num_variables(); ++col) {
    const Fractional tolerance = 1e-5;
    const Fractional& value = solver.variable_values()[col];
    if (value > 1 - tolerance) {
      ++num_variable_fixed;
      LinearBooleanConstraint* constraint = problem->add_constraints();
      constraint->set_lower_bound(1);
      constraint->set_upper_bound(1);
      constraint->add_coefficients(1);
      constraint->add_literals(col.value() + 1);
    } else if (value < tolerance) {
      ++num_variable_fixed;
      LinearBooleanConstraint* constraint = problem->add_constraints();
      constraint->set_lower_bound(0);
      constraint->set_upper_bound(0);
      constraint->add_coefficients(1);
      constraint->add_literals(col.value() + 1);
    }
  }
  LOG(INFO) << "LNS with " << num_variable_fixed << " fixed variables.";
  return true;
}

}  // namespace sat
}  // namespace operations_research
