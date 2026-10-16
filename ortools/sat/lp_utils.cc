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
#include <tuple>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/strong_vector.h"
#include "ortools/base/types.h"
#include "ortools/glop/lp_solver.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/graph/connected_components.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/sparse.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/mp_model_wrapper.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/util.h"
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

  // Update the hint if any.
  if (mp_model->has_solution_hint()) {
    ::operations_research::PartialVariableAssignment* hint =
        mp_model->mutable_solution_hint();
    for (int i = 0; i < hint->var_index_size(); ++i) {
      const int var = hint->var_index(i);
      if (var >= 0 && var < num_variables) {
        hint->set_var_value(i, hint->var_value(i) * var_scaling[var]);
      }
    }
  }
}

}  // namespace

std::vector<double> ScaleContinuousVariables(
    const SatParameters& params, MPModelProto* mp_model,
    absl::Span<const double> current_vars_scaling) {
  const double scaling = params.mip_var_scaling();
  const double max_bound = params.mip_scale_large_domain()
                               ? std::numeric_limits<double>::infinity()
                               : params.mip_max_bound();

  const int num_variables = mp_model->variable_size();
  std::vector<double> var_scaling(num_variables, 1.0);
  for (int i = 0; i < num_variables; ++i) {
    if (mp_model->variable(i).is_integer()) continue;
    if (!current_vars_scaling.empty() && current_vars_scaling[i] != 1.0)
      continue;
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

std::vector<double> ScaleContinuousVariablesUpToMaxBound(
    const SatParameters& params, MPModelProto* mp_model, SolverLogger* logger,
    absl::Span<const double> current_vars_scaling) {
  const double wanted_precision = params.mip_tight_precision();
  const double max_bound = params.mip_max_bound();

  CHECK_GT(max_bound, 0);
  CHECK_GT(wanted_precision, 0);
  const int num_variables = mp_model->variable_size();

  std::vector<bool> is_integer(num_variables, false);
  for (int i = 0; i < num_variables; ++i) {
    is_integer[i] = mp_model->variable(i).is_integer();
  }

  // Since we only want "wanted_precision" on the constraint activity, if
  // a variable only appears with really small coefficients, we don't need to
  // scale it too much. If there is a feasible solution, there will be one
  // "integer" close by.
  std::vector<double> max_scaling_needed(num_variables, 0.0);
  for (const MPConstraintProto& mp_constraint : mp_model->constraint()) {
    int num_floats = 0;
    for (const int var : mp_constraint.var_index()) {
      if (!is_integer[var]) ++num_floats;
    }
    if (num_floats == 0) continue;

    const int num_terms = mp_constraint.var_index().size();
    for (int i = 0; i < num_terms; ++i) {
      const int var = mp_constraint.var_index(i);
      if (is_integer[var]) continue;

      // TODO(user): When the float is the unique non-integer in a constraint
      // (num_floats == 1), we could maybe derive a tighter bound? especially if
      // all coeffs are integer. This is a bit what is done in
      // DetectImpliedIntegers() though.
      const double coeff_magnitude = std::abs(mp_constraint.coefficient(i));
      if (coeff_magnitude == 0.0) continue;

      // We assume that we don't care if the activity changes by less than
      // wanted_precision / coeff_magnitude / num_floats.
      max_scaling_needed[var] = std::max(
          max_scaling_needed[var],
          static_cast<double>(num_floats) * coeff_magnitude / wanted_precision);
    }
  }

  // For stats.
  int num_scaled = 0;
  int num_too_large = 0;
  int min_exponent = 100;
  int max_exponent = 0;

  std::vector<double> var_scaling(num_variables, 1.0);
  for (int i = 0; i < num_variables; ++i) {
    if (mp_model->variable(i).is_integer()) continue;
    if (!current_vars_scaling.empty() && current_vars_scaling[i] != 1.0)
      continue;

    const double lb = mp_model->variable(i).lower_bound();
    const double ub = mp_model->variable(i).upper_bound();
    const double bound_magnitude = std::max(std::abs(lb), std::abs(ub));
    if (bound_magnitude == 0) continue;
    if (bound_magnitude > max_bound) {
      // TODO(user): we should probably still scale them a bit. Like we can
      // assume |original variable| < 1e7 and scale to 1e10 or something.
      // And scale all variables to 1e10 ?
      ++num_too_large;
      VLOG(2) << "Unbounded var " << i << " [" << lb << "," << ub
              << "] max_needed:" << max_scaling_needed[i];
      continue;
    }

    // We prefer power of two scaling factors.
    //
    // This has nice properties. In particular if there is an integer solution
    // without scaling, it should still be a solution of the scaled problem,
    // this allows us to compare various scaling factors. As it turns out, we
    // have more trouble finding solutions to highly scaled problems currently.
    const int exp =
        std::min(HighestPowerOfTwoAtOrBelow(max_bound / bound_magnitude),
                 LowestPowerOfTwoAtOrAbove(max_scaling_needed[i]));
    const double scaling = std::ldexp(1.0, exp);

    // This piece of code might be a bit clearer to understand compared to
    // the O(1) formula above.
    if (DEBUG_MODE) {
      double test = 1.0;
      while (test < max_scaling_needed[i] &&
             2 * test * bound_magnitude <= max_bound) {
        test *= 2;
      }
      CHECK_EQ(test, scaling);
    }

    ++num_scaled;
    min_exponent = std::min(min_exponent, exp);
    max_exponent = std::max(max_exponent, exp);

    var_scaling[i] = scaling;
    VLOG(2) << i << " [" << lb << ", " << ub << "] scaling:" << scaling
            << " max_needed:" << max_scaling_needed[i];
  }

  if (num_scaled > 0) {
    SOLVER_LOG(logger, "[ScaleToMaxBound] Scaled ", num_scaled,
               " domains, power of two scaling in [", min_exponent, ",",
               max_exponent, "]");
  }
  if (num_too_large > 0) {
    SOLVER_LOG(logger, "[ScaleToMaxBound] ", num_too_large,
               " variables with domain magnitude larger than mip_max_bound.");
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
    if (floored_x >= static_cast<double>(kint64max)) {
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

// For each side (>= and <=) return a factor such that factor * var only needs
// to take integer values to satisfy this side.
//
// We return kInfinity if we failed, and 0.0 if this side is not constrained.
//
// Precondition: var must be the only non-integer in the given constraint.
std::pair<double, double> GetIntegralityMultiplier(
    const EnforcedLinearConstraint& ct, absl::Span<const double> var_scaling,
    int var, double tolerance, double max_multiplier) {
  const double kInfinity = std::numeric_limits<double>::infinity();

  double var_coeff = 0.0;
  for (int i = 0; i < ct.num_terms(); ++i) {
    if (var == ct.vars(i)) {
      var_coeff = ct.coeffs(i);
      break;
    }
  }
  if (var_coeff == 0.0) return {0.0, 0.0};
  const double limit = max_multiplier / std::abs(var_coeff);
  if (limit < 1.0) return {kInfinity, kInfinity};
  const int64_t int_limit =
      limit >= static_cast<double>(kint64max)
          ? kint64max
          : CapAdd(static_cast<int64_t>(std::floor(limit)), 1);

  double multiplier = 1.0;
  for (int i = 0; i < ct.num_terms(); ++i) {
    if (var == ct.vars(i)) continue;

    // This actually computes the smallest multiplier to make all other
    // terms in the constraint integer.
    const double coeff = multiplier * ct.coeffs(i) / var_scaling[ct.vars(i)];
    multiplier *= FindRationalFactor(coeff, int_limit, multiplier * tolerance);
    if (multiplier == 0 || multiplier > limit) {
      multiplier = kInfinity;
      break;
    }
  }
  DCHECK_NE(var_coeff, 0.0);

  // Only the magnitude matter.
  double ct_lb = std::abs(ct.lower_bound());
  double ct_ub = std::abs(ct.upper_bound());
  if (var_coeff < 0.0) std::swap(ct_lb, ct_ub);

  // Now adjust for the bound.
  double lb_multiplier = multiplier;
  if (ct_lb == kInfinity) {
    lb_multiplier = 0.0;
  } else if (multiplier != kInfinity) {
    lb_multiplier =
        multiplier * FindRationalFactor(multiplier * ct_lb, int_limit,
                                        multiplier * tolerance);
    if (lb_multiplier == 0.0 || lb_multiplier > limit) {
      lb_multiplier = kInfinity;
    }
  }

  double ub_multiplier = multiplier;
  if (ct_ub == kInfinity) {
    ub_multiplier = 0.0;
  } else if (multiplier != kInfinity) {
    ub_multiplier =
        multiplier * FindRationalFactor(multiplier * ct_ub, int_limit,
                                        multiplier * tolerance);
    if (ub_multiplier == 0.0 || ub_multiplier > limit) {
      ub_multiplier = kInfinity;
    }
  }

  return {std::abs(lb_multiplier * var_coeff),
          std::abs(ub_multiplier * var_coeff)};
}

}  // namespace

bool MakeBoundsOfIntegerVariablesInteger(const SatParameters& params,
                                         MPModelProto* mp_model,
                                         SolverLogger* logger) {
  return MakeBoundsOfIntegerVariablesInteger(params, /*var_scaling=*/{},
                                             mp_model, logger);
}

bool MakeBoundsOfIntegerVariablesInteger(const SatParameters& params,
                                         absl::Span<const double> var_scaling,
                                         MPModelProto* mp_model,
                                         SolverLogger* logger) {
  const int num_variables = mp_model->variable_size();
  int64_t num_changes = 0;
  for (int i = 0; i < num_variables; ++i) {
    const MPVariableProto& mp_var = mp_model->variable(i);
    if (!mp_var.is_integer()) continue;

    // Make sure the bound of an integer variable is integer.
    // Note that we do not scale the tolerance! we want to interpret variable
    // bounds as exact, but we just want to allow an extra integer point if
    // it is within tolerance.
    const double scaling = var_scaling.empty() ? 1.0 : var_scaling[i];
    const double tolerance =
        std::min(0.5, scaling * params.mip_wanted_precision());

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
  if (num_changes > 0) {
    SOLVER_LOG(logger, "[IntegerBound] Snapped ", num_changes,
               " bound of an integer variable to an exact integer.");
  }
  return true;
}

// TODO(user): we use LooseLowerBound()/LooseUpperBound() to avoid a quadratic
// complexity. Fix in order to use TightLowerBound()/TightUpperBound().
template <bool exact, bool fill_min, bool fill_max>
void LeftRightActivityBounds::FillFromConstraint(
    absl::Span<const double> var_lbs, absl::Span<const double> var_ubs,
    const MPConstraintProto& ct) {
  static_assert(fill_min || fill_max);
  const int num_terms = ct.var_index().size();
  if constexpr (fill_min) {
    min_activity_of_others.assign(num_terms, 0.0);
    min_left_activity.assign(num_terms, 0.0);
    min_right_activity.assign(num_terms, 0.0);
  }
  if constexpr (fill_max) {
    max_activity_of_others.assign(num_terms, 0.0);
    max_left_activity.assign(num_terms, 0.0);
    max_right_activity.assign(num_terms, 0.0);
  }
  if (num_terms == 0) return;

  // *left*[i] contains the min/max sum of all the terms in [0, i).
  if constexpr (exact) {
    if constexpr (fill_min) tmp_min_sum.Clear();
    if constexpr (fill_max) tmp_max_sum.Clear();
  }
  for (int i = 0; i + 1 < num_terms; ++i) {
    const int var = ct.var_index(i);
    double coeff = ct.coefficient(i);
    double lb = 0.0;
    double ub = 0.0;
    if (coeff != 0.0) {
      lb = var_lbs[var];
      ub = var_ubs[var];
      if (coeff < 0.0) {
        coeff = -coeff;
        std::swap(lb, ub);
        lb = -lb;
        ub = -ub;
      }
    }
    if constexpr (fill_min) {
      if (min_left_activity[i] == -kInfinity || lb == -kInfinity) {
        min_left_activity[i + 1] = -kInfinity;
      } else if constexpr (exact) {
        if (coeff != 0.0) tmp_min_sum.AddProd(coeff, lb);
        min_left_activity[i + 1] = tmp_min_sum.LooseLowerBound();
      } else {
        min_left_activity[i + 1] = min_left_activity[i] + coeff * lb;
      }
    }
    if constexpr (fill_max) {
      if (max_left_activity[i] == kInfinity || ub == kInfinity) {
        max_left_activity[i + 1] = kInfinity;
      } else if constexpr (exact) {
        if (coeff != 0.0) tmp_max_sum.AddProd(coeff, ub);
        max_left_activity[i + 1] = tmp_max_sum.LooseUpperBound();
      } else {
        max_left_activity[i + 1] = max_left_activity[i] + coeff * ub;
      }
    }
  }

  // *right*[i] contains the min/max sum of all the terms in (i, num_terms).
  if constexpr (exact) {
    if constexpr (fill_min) tmp_min_sum.Clear();
    if constexpr (fill_max) tmp_max_sum.Clear();
  }
  for (int i = num_terms - 1; i > 0; --i) {
    const int var = ct.var_index(i);
    double coeff = ct.coefficient(i);
    double lb = 0.0;
    double ub = 0.0;
    if (coeff != 0.0) {
      lb = var_lbs[var];
      ub = var_ubs[var];
      if (coeff < 0.0) {
        coeff = -coeff;
        std::swap(lb, ub);
        lb = -lb;
        ub = -ub;
      }
    }
    if constexpr (fill_min) {
      if (min_right_activity[i] == -kInfinity || lb == -kInfinity) {
        min_right_activity[i - 1] = -kInfinity;
      } else if constexpr (exact) {
        if (coeff != 0.0) tmp_min_sum.AddProd(coeff, lb);
        min_right_activity[i - 1] = tmp_min_sum.LooseLowerBound();
      } else {
        min_right_activity[i - 1] = min_right_activity[i] + coeff * lb;
      }
    }
    if constexpr (fill_max) {
      if (max_right_activity[i] == kInfinity || ub == kInfinity) {
        max_right_activity[i - 1] = kInfinity;
      } else if constexpr (exact) {
        if (coeff != 0.0) tmp_max_sum.AddProd(coeff, ub);
        max_right_activity[i - 1] = tmp_max_sum.LooseUpperBound();
      } else {
        max_right_activity[i - 1] = max_right_activity[i] + coeff * ub;
      }
    }
  }

  // Fill the min_activity_of_others/max_activity_of_others.
  // TODO(user): This could be integrated with the previous loop.
  for (int i = 0; i < num_terms; ++i) {
    if constexpr (fill_min) {
      if constexpr (exact) {
        min_activity_of_others[i] =
            ExactScaledSumLb(min_left_activity[i], min_right_activity[i]);
      } else {
        min_activity_of_others[i] =
            min_left_activity[i] + min_right_activity[i];
      }
    }
    if constexpr (fill_max) {
      if constexpr (exact) {
        max_activity_of_others[i] =
            ExactScaledSumUb(max_left_activity[i], max_right_activity[i]);
      } else {
        max_activity_of_others[i] =
            max_left_activity[i] + max_right_activity[i];
      }
    }
  }
}

template void LeftRightActivityBounds::FillFromConstraint<true, true, true>(
    absl::Span<const double> var_lbs, absl::Span<const double> var_ubs,
    const MPConstraintProto& ct);
template void LeftRightActivityBounds::FillFromConstraint<true, true, false>(
    absl::Span<const double> var_lbs, absl::Span<const double> var_ubs,
    const MPConstraintProto& ct);
template void LeftRightActivityBounds::FillFromConstraint<true, false, true>(
    absl::Span<const double> var_lbs, absl::Span<const double> var_ubs,
    const MPConstraintProto& ct);
template void LeftRightActivityBounds::FillFromConstraint<false, true, true>(
    absl::Span<const double> var_lbs, absl::Span<const double> var_ubs,
    const MPConstraintProto& ct);
template void LeftRightActivityBounds::FillFromConstraint<false, true, false>(
    absl::Span<const double> var_lbs, absl::Span<const double> var_ubs,
    const MPConstraintProto& ct);
template void LeftRightActivityBounds::FillFromConstraint<false, false, true>(
    absl::Span<const double> var_lbs, absl::Span<const double> var_ubs,
    const MPConstraintProto& ct);

void RestrictBoundsWithDualReasoning(const SatParameters& params,
                                     const MPModelProtoWrapper& mp_model,
                                     absl::Span<double> var_lbs,
                                     absl::Span<double> var_ubs,
                                     SolverLogger* logger) {
  // Disable depending on the parameters.
  if (!params.mip_presolve_restrict_bounds()) return;
  if (params.keep_all_feasible_solutions_in_presolve()) return;

  const int num_variables = mp_model.num_variables();
  const double kInfinity = std::numeric_limits<double>::infinity();

  // Note that we don't want to decrease/increase past zero.
  //
  // It is not "incorrect" but can lead to complication. For instance, if we
  // have two variables in [0, infinity) with X only appearing in X - Y >= 0. We
  // don't want to fix X to "infinity" or enforce "X >= any large value"
  // unnecessarily, even if the larger lb, the better from a constraint
  // perspective. We still want to try to find solutions with low magnitude in
  // general.
  std::vector<double> can_freely_decrease_to(num_variables, 0.0);
  std::vector<double> can_freely_increase_to(num_variables, 0.0);

  // Deal with objective. We disable any bound restriction in the wrong
  // direction.
  for (int v = 0; v < num_variables; ++v) {
    const double coeff = mp_model.variables(v).objective_coefficient();
    if (coeff > 0.0) {
      can_freely_increase_to[v] = -kInfinity;
    } else if (coeff < 0.0) {
      can_freely_decrease_to[v] = kInfinity;
    }
  }

  LeftRightActivityBounds bounds;

  const int num_constraints = mp_model.num_constraints();
  for (int c = 0; c < num_constraints; ++c) {
    const EnforcedLinearConstraint& ct = mp_model.constraints(c);
    for (int i = 0; i < ct.enforcement_vars().size(); ++i) {
      const int var = ct.enforcement_vars()[i];
      const int64_t value = ct.enforcement_values()[i];
      if (value < var_ubs[var]) {
        can_freely_decrease_to[var] = kInfinity;
      }
      if (value > var_lbs[var]) {
        can_freely_increase_to[var] = -kInfinity;
      }
    }

    const int num_terms = ct.num_terms();
    if (num_terms == 0) continue;

    const bool fill_min = ct.lower_bound() > -kInfinity;
    const bool fill_max = ct.upper_bound() < kInfinity;
    if (!fill_min && !fill_max) continue;

    // Skip if we can't improve the bound for this constraint. This avoid
    // running exact floating point computation below (slow) when not needed.
    bounds.FillFromConstraint<false>(var_lbs, var_ubs, ct.proto(), fill_min,
                                     fill_max);
    bool skip = true;
    for (int i = 0; i < num_terms; ++i) {
      const int var = ct.vars(i);
      const double coeff = ct.coeffs(i);
      if (coeff == 0.0) continue;
      if (can_freely_increase_to[var] > var_lbs[var]) {
        if (coeff > 0.0 && fill_max) {
          const double dual_lt =
              (ct.upper_bound() - bounds.max_activity_of_others[i]) / coeff;
          if (dual_lt < can_freely_increase_to[var]) {
            skip = false;
            break;
          }
        } else if (coeff < 0.0 && fill_min) {
          const double dual_lt =
              (ct.lower_bound() - bounds.min_activity_of_others[i]) / coeff;
          if (dual_lt < can_freely_increase_to[var]) {
            skip = false;
            break;
          }
        }
      }
      if (can_freely_decrease_to[var] < var_ubs[var]) {
        if (coeff < 0.0 && fill_max) {
          const double dual_gt =
              (ct.upper_bound() - bounds.max_activity_of_others[i]) / coeff;
          if (dual_gt > can_freely_decrease_to[var]) {
            skip = false;
            break;
          }
        } else if (coeff > 0.0 && fill_min) {
          const double dual_gt =
              (ct.lower_bound() - bounds.min_activity_of_others[i]) / coeff;
          if (dual_gt > can_freely_decrease_to[var]) {
            skip = false;
            break;
          }
        }
      }
    }
    if (skip) continue;

    // This is about 10x slower, which justify the skip above.
    // TODO(user): this is still slow, we could compute which position need
    // to be looked at and do less work for the other.
    bounds.FillFromConstraint<true>(var_lbs, var_ubs, ct.proto(), fill_min,
                                    fill_max);

    // For each position, combine *left[i]* and *right*[i] to derive bounds
    // on the variable at position i.
    for (int i = 0; i < num_terms; ++i) {
      const int var = ct.vars(i);
      const double coeff = ct.coeffs(i);
      if (coeff == 0.0) continue;

      // If c>=0, bounds.max_activity_of_others + c.X <= rhs so X <= (rhs -
      // max_activity_of_others) / c
      if (can_freely_increase_to[var] > var_lbs[var]) {
        if (coeff > 0.0 && fill_max) {
          const double dual_lt = ExactScaledSumLb(
              ct.upper_bound(), -bounds.max_activity_of_others[i], coeff);
          can_freely_increase_to[var] =
              std::min(can_freely_increase_to[var], dual_lt);
        } else if (coeff < 0.0 && fill_min) {
          const double dual_lt = ExactScaledSumUb(
              ct.lower_bound(), -bounds.min_activity_of_others[i], coeff);
          can_freely_increase_to[var] =
              std::min(can_freely_increase_to[var], dual_lt);
        }
      }

      // If c>=0, min_activity_of_others + c.X >= rhs so X >= (rhs -
      // min_activity_of_others) / c
      if (can_freely_decrease_to[var] < var_ubs[var]) {
        if (coeff < 0.0 && fill_max) {
          const double dual_gt = ExactScaledSumLb(
              ct.upper_bound(), -bounds.max_activity_of_others[i], coeff);
          can_freely_decrease_to[var] =
              std::max(can_freely_decrease_to[var], dual_gt);
        } else if (coeff > 0.0 && fill_min) {
          const double dual_gt = ExactScaledSumUb(
              ct.lower_bound(), -bounds.min_activity_of_others[i], coeff);
          can_freely_decrease_to[var] =
              std::max(can_freely_decrease_to[var], dual_gt);
        }
      }
    }
  }

  // Use collected info.
  // Note that we keep "integer" solutions.
  int num_changed_bounds = 0;
  double max_change = 0.0;
  for (int v = 0; v < num_variables; ++v) {
    const double ub_target =
        std::ceil(std::max(can_freely_decrease_to[v], var_lbs[v]));
    const double lb_target =
        std::floor(std::min(can_freely_increase_to[v], var_ubs[v]));

    CHECK_GE(ub_target, var_lbs[v]);
    CHECK_LE(lb_target, var_ubs[v]);
    if (ub_target < lb_target) {
      const double target =
          ub_target < 0.0 ? lb_target > 0.0 ? 0.0 : lb_target : ub_target;
      VLOG(2) << "Fixing " << v << " with bound [" << var_lbs[v] << ","
              << var_ubs[v] << "] to " << target;
      if (var_ubs[v] > target) {
        ++num_changed_bounds;
        max_change = std::max(max_change, var_ubs[v] - target);
      }
      if (var_lbs[v] < target) {
        ++num_changed_bounds;
        max_change = std::max(max_change, target - var_lbs[v]);
      }
      var_ubs[v] = target;
      var_lbs[v] = target;
      continue;
    }
    if (var_ubs[v] > ub_target) {
      ++num_changed_bounds;
      max_change = std::max(max_change, var_ubs[v] - ub_target);
      VLOG(2) << v << " UB " << var_ubs[v] << " --> " << ub_target;
      var_ubs[v] = ub_target;
    }
    if (var_lbs[v] < lb_target) {
      ++num_changed_bounds;
      max_change = std::max(max_change, lb_target - var_lbs[v]);
      VLOG(2) << v << " LB " << var_lbs[v] << " --> " << lb_target;
      var_lbs[v] = lb_target;
    }
  }

  if (logger != nullptr && num_changed_bounds > 0) {
    SOLVER_LOG(logger, "[DualReasoning] Restricted ",
               FormatCounter(num_changed_bounds),
               " bounds using dual reasoning. Max change ", max_change, ".");
  }
}

void DisplayVariableStatistics(const MPModelProto& mp_model,
                               SolverLogger* logger) {
  const double kInfinity = std::numeric_limits<double>::infinity();
  double min_lb = +kInfinity;
  double max_ub = -kInfinity;
  int num_integers = 0;
  for (const MPVariableProto& var : mp_model.variable()) {
    if (var.is_integer()) ++num_integers;
    min_lb = std::min(min_lb, var.lower_bound());
    max_ub = std::max(max_ub, var.upper_bound());
  }

  const int num_variables = mp_model.variable().size();
  SOLVER_LOG(logger, "All variables in [", min_lb, ",", max_ub,
             "], num_integers: ", num_integers, "/", num_variables,
             (num_integers == num_variables ? " [IP] " : " [MIP] "));
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

  if (!std::isfinite(mp_model.objective_offset())) {
    SOLVER_LOG(logger,
               "Objective offset is not finite: ", mp_model.objective_offset());
    return false;
  }

  return true;
}

namespace {

// This function is quite specific to its usage, see DetectImpliedIntegers()
// below. Return kInfinity to indicate failure.
double GetDoubleLcm(absl::Span<const double> multipliers, double tolerance,
                    double max_multiplier) {
  const double kInfinity = std::numeric_limits<double>::infinity();
  if (max_multiplier < 1.0) return kInfinity;
  const int64_t int_limit =
      max_multiplier >= static_cast<double>(kint64max)
          ? kint64max
          : CapAdd(static_cast<int64_t>(std::floor(max_multiplier)), 1);
  double scaling = 1.0;
  for (const double m : multipliers) {
    if (m == kInfinity) return kInfinity;
    const double ratio = scaling / m;
    scaling *= FindRationalFactor(ratio, int_limit, ratio * tolerance);
    if (scaling == 0.0 || scaling > max_multiplier) return kInfinity;
  }
  return scaling;
}

}  // namespace

std::vector<double> DetectImpliedIntegers(const SatParameters& params,
                                          MPModelProtoWrapper* mp_model,
                                          SolverLogger* logger) {
  const int num_variables = mp_model->num_variables();
  std::vector<double> var_scaling(num_variables, 1.0);

  int initial_num_integers = 0;
  for (int i = 0; i < num_variables; ++i) {
    if (mp_model->variables(i).is_integer()) ++initial_num_integers;
  }
  VLOG(1) << "Initial num integers: " << initial_num_integers;

  // We will process all equality constraints with exactly one non-integer.
  // If the coefficients and bounds of the constraint are "nice" then this only
  // non-integer must actually be integer (with the correct scaling),
  // independently of its bounds.
  //
  // TODO(user): Unfortunately, it is pretty hard to rigorously define what
  // this transformation means on the original problem with tolerances. Because
  // enforcing that variable to be integer, might make a problem infeasible even
  // if it was "tolerance-feasible" for some definition.
  const double tolerance = params.mip_wanted_precision();
  std::vector<int> constraint_queue;

  const int num_constraints = mp_model->num_constraints();
  std::vector<int> constraint_to_num_non_integer(num_constraints, 0);
  std::vector<std::vector<int>> var_to_constraints(num_variables);
  for (int i = 0; i < num_constraints; ++i) {
    const EnforcedLinearConstraint& mp_constraint = mp_model->constraints(i);

    for (const int var : mp_constraint.vars()) {
      if (!mp_model->variables(var).is_integer()) {
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

  int num_implied_integers = 0;
  double max_scaling = 0.0;
  auto scale_and_mark_as_integer = [&](int var, double scaling) mutable {
    CHECK_NE(var, -1);
    CHECK(!mp_model->variables(var).is_integer());
    CHECK_EQ(var_scaling[var], 1.0);
    if (scaling != 1.0) {
      VLOG(2) << "Scaled " << var << " by " << scaling;
    }

    ++num_implied_integers;
    max_scaling = std::max(max_scaling, scaling);

    // Scale the variable right away and mark it as implied integer.
    // Note that the constraints will be scaled later.
    var_scaling[var] = scaling;
    mp_model->mutable_variables(var)->set_is_integer(true);

    // Update the queue of constraints with a single non-integer.
    for (const int ct_index : var_to_constraints[var]) {
      constraint_to_num_non_integer[ct_index]--;
      if (constraint_to_num_non_integer[ct_index] == 1) {
        constraint_queue.push_back(ct_index);
      }
    }
  };

  const auto compute_max_multiplier = [&](const MPVariableProto& var_proto) {
    if (params.mip_scale_to_max_bound()) {
      double max_magnitude = 1.0;
      if (std::isfinite(var_proto.lower_bound())) {
        max_magnitude =
            std::max(max_magnitude, std::abs(var_proto.lower_bound()));
      }
      if (std::isfinite(var_proto.upper_bound())) {
        max_magnitude =
            std::max(max_magnitude, std::abs(var_proto.upper_bound()));
      }
      return params.mip_max_bound() / max_magnitude;
    } else {
      // TODO(user): This is the old behavior and changing it broke some
      // golden test. Fix.
      return 1e4;
    }
  };

  int num_fail_due_to_rhs = 0;
  int num_fail_due_to_large_multiplier = 0;
  int num_processed_constraints = 0;
  while (!constraint_queue.empty()) {
    const int top_ct_index = constraint_queue.back();
    constraint_queue.pop_back();

    // The non-integer variable was already made integer by one other
    // constraint.
    if (constraint_to_num_non_integer[top_ct_index] == 0) continue;

    // Ignore non-equality or enforced constraint here.
    const EnforcedLinearConstraint& ct = mp_model->constraints(top_ct_index);
    if (ct.IsEnforced()) continue;
    if (ct.lower_bound() + tolerance < ct.upper_bound()) continue;

    // This will be set to the unique non-integer term of this constraint.
    int var = -1;
    for (int i = 0; i < ct.num_terms(); ++i) {
      if (!mp_model->variables(ct.vars(i)).is_integer()) {
        CHECK_EQ(var, -1);
        var = ct.vars(i);
      }
    }
    CHECK_NE(var, -1);

    // The smaller the domain, the more we allow scaling.
    const MPVariableProto& var_proto = mp_model->variables(var);
    const double max_multiplier = compute_max_multiplier(var_proto);

    // Inspect this constraint to see if we can find a small enough multiplier.
    // Note that if this is the case, we will inspect all constraints below to
    // try to find the best. Otherwise, we will wait for the other constraint to
    // be processed. This avoid a bad quadratic complexity if we where to
    // execute the loop below in all cases.
    ++num_processed_constraints;
    double best_scaling = GetIntegralityMultiplier(ct, var_scaling, var,
                                                   tolerance, max_multiplier)
                              .first;
    if (best_scaling == 0.0) continue;  // coeff was 0.0 ??
    if (best_scaling == kInfinity) {
      ++num_fail_due_to_large_multiplier;
      continue;
    }

    // It is possible this variable appear as the only non-integer in many
    // constraints, find the one that lead to the smallest multiplier.
    // See diameterc-msts-v40a100d5i.mps for an instance of this.
    for (const int ct_index : var_to_constraints[var]) {
      if (ct_index == top_ct_index) continue;
      if (constraint_to_num_non_integer[ct_index] != 1) continue;

      // Ignore non-equality or enforced constraint here.
      const EnforcedLinearConstraint& other_ct =
          mp_model->constraints(ct_index);
      if (other_ct.IsEnforced()) continue;
      if (other_ct.lower_bound() + tolerance < other_ct.upper_bound()) continue;

      ++num_processed_constraints;
      const double multiplier =
          GetIntegralityMultiplier(other_ct, var_scaling, var, tolerance,
                                   max_multiplier)
              .first;
      if (multiplier != 0.0 && multiplier < best_scaling) {
        best_scaling = multiplier;
      }
    }

    scale_and_mark_as_integer(var, best_scaling);
  }

  // Process continuous variables that only appear as the unique non-integer
  // in a set of non-equality constraints.
  //
  // Note that turning such a variable into an integer cannot in turn trigger
  // new integer detection, so there is no point doing that in a loop.
  int num_unused = 0;
  int num_fixed = 0;
  int num_in_inequalities = 0;
  int num_inequality_fail_due_to_lcm = 0;
  int num_inequality_fail_due_to_large_multiplier = 0;
  int num_inequality_fail_due_to_domain = 0;
  for (int var = 0; var < num_variables; ++var) {
    const MPVariableProto& var_proto = mp_model->variables(var);
    if (var_proto.is_integer()) continue;
    if (params.keep_all_feasible_solutions_in_presolve()) break;

    // The smaller the domain, the more we allow scaling.
    const double max_multiplier = compute_max_multiplier(var_proto);

    std::vector<double> var_multipliers_ge;
    std::vector<double> var_multipliers_le;
    double scaling_ge = 1.0;
    double scaling_le = 1.0;
    if (!var_to_constraints[var].empty()) {
      // Check that var is the only non-integral variable in each constraint
      // where it appears.
      bool ok = true;
      for (const int ct_index : var_to_constraints[var]) {
        if (constraint_to_num_non_integer[ct_index] != 1) {
          ok = false;
          break;
        }
      }
      if (!ok) continue;

      // For each constraints in which `var` appears, stores a m such that the
      // constraint is feasible if and only if it is feasible with m * var an
      // integer. We actually separate the >= and the <= side for a tighter
      // algorithm.
      for (const int ct_index : var_to_constraints[var]) {
        const EnforcedLinearConstraint& ct = mp_model->constraints(ct_index);
        const auto [lb_mult, ub_mult] = GetIntegralityMultiplier(
            ct, var_scaling, var, tolerance, max_multiplier);
        if (lb_mult != 0.0) var_multipliers_ge.push_back(lb_mult);
        if (ub_mult != 0.0) var_multipliers_le.push_back(ub_mult);
      }

      // Now, we have a bunch of multiplier that make each >= or <= constraint
      // integer on var. We want to compute a kind of least common multiple so
      // that scaling * var will always be lower or upper bounded by integer.
      //
      // If the variable has an objective, we will need the "good objective"
      // bound to be integer. If not, just one of the two bounds work since we
      // will always be able to "snap" the variable value to that one.
      bool fail_because_of_single_constraint = false;
      if (var_proto.objective_coefficient() > 0.0) {
        for (const double m : var_multipliers_ge) {
          if (m == kInfinity) fail_because_of_single_constraint = true;
        }
        scaling_ge =
            GetDoubleLcm(var_multipliers_ge, tolerance, max_multiplier);
        scaling_le = kInfinity;
      } else if (var_proto.objective_coefficient() < 0.0) {
        for (const double m : var_multipliers_le) {
          if (m == kInfinity) fail_because_of_single_constraint = true;
        }
        scaling_ge = kInfinity;
        scaling_le =
            GetDoubleLcm(var_multipliers_le, tolerance, max_multiplier);
      } else {
        for (const double m : var_multipliers_ge) {
          if (m == kInfinity) fail_because_of_single_constraint = true;
        }
        for (const double m : var_multipliers_le) {
          if (m == kInfinity) fail_because_of_single_constraint = true;
        }
        scaling_le =
            GetDoubleLcm(var_multipliers_le, tolerance, max_multiplier);
        scaling_ge =
            GetDoubleLcm(var_multipliers_ge, tolerance, max_multiplier);
      }

      if (scaling_ge == kInfinity && scaling_le == kInfinity) {
        if (fail_because_of_single_constraint) {
          ++num_inequality_fail_due_to_large_multiplier;
        } else {
          ++num_inequality_fail_due_to_lcm;
        }
        continue;
      }
    } else {
      if (var_proto.objective_coefficient() > 0.0) scaling_le = kInfinity;
      if (var_proto.objective_coefficient() < 0.0) scaling_ge = kInfinity;
    }

    // At this point, all constraints only ever enforce an [int_lb, ...] or
    // [..., int_ub] interval on the SCALED variable. We still have a couple of
    // corner cases to deal with.

    // First corner case: If the scaled domain does not contain an integer
    // point, then the domain of the variable will always be either fully
    // feasible or fully infeasible on the snapped side! So we can just fix
    // that variable to the corresponding bound in its domain. No need to scale
    // at all.
    //
    // NOTE(user): This do not trigger on the miplib though.
    const auto domain_has_no_integer = [&](double s) {
      if (s == kInfinity) return false;
      const Fractional scaled_var_lb =
          (var_proto.lower_bound() - tolerance) * s;
      const Fractional scaled_var_ub =
          (var_proto.upper_bound() + tolerance) * s;
      return std::ceil(scaled_var_lb) > std::floor(scaled_var_ub);
    };
    const bool fix_to_lb = domain_has_no_integer(scaling_ge);
    const bool fix_to_ub = domain_has_no_integer(scaling_le);
    if (fix_to_lb || fix_to_ub) {
      double target =
          fix_to_lb ? var_proto.lower_bound() : var_proto.upper_bound();
      if (fix_to_lb && fix_to_ub &&
          std::abs(var_proto.upper_bound()) < std::abs(target)) {
        target = var_proto.upper_bound();
      }
      ++num_fixed;
      mp_model->mutable_variables(var)->set_lower_bound(target);
      mp_model->mutable_variables(var)->set_upper_bound(target);
      continue;
    }

    // Second corner case: when snapping to the lower (or upper) bound, if
    // there is an objective or opposite-side constraints, the variable's own
    // lower (or upper) bound can be the active bound and must also be integer
    // after scaling so we don't cut off the best/only feasible solution.
    //
    // TODO(user): There is a bad interaction with the bound propagator that
    // propagates bound using tolerances, so that they are not exactly integer
    // :( find a cleaner way ?
    const auto scale_for_bound = [&](double s, double bound) {
      if (s == kInfinity || !std::isfinite(bound) || max_multiplier < 1.0) {
        return s == kInfinity || max_multiplier < 1.0 ? kInfinity : s;
      }
      const int64_t int_limit =
          max_multiplier >= static_cast<double>(kint64max)
              ? kint64max
              : CapAdd(static_cast<int64_t>(std::floor(max_multiplier)), 1);
      s *= FindRationalFactor(s * bound, int_limit, s * tolerance);
      if (s == 0 || s > max_multiplier) return kInfinity;
      const double scaled = bound * s;
      if (std::abs(scaled - std::round(scaled)) >
          params.mip_wanted_precision() * s) {
        return kInfinity;
      }
      return s;
    };
    if (var_proto.objective_coefficient() > 0.0 ||
        !var_multipliers_le.empty()) {
      scaling_ge = scale_for_bound(scaling_ge, var_proto.lower_bound());
    }
    if (var_proto.objective_coefficient() < 0.0 ||
        !var_multipliers_ge.empty()) {
      scaling_le = scale_for_bound(scaling_le, var_proto.upper_bound());
    }
    const double scaling = std::min(scaling_ge, scaling_le);
    if (scaling == kInfinity) {
      ++num_inequality_fail_due_to_domain;
      continue;
    }

    if (var_to_constraints[var].empty()) {
      ++num_unused;
    } else {
      ++num_in_inequalities;
    }
    scale_and_mark_as_integer(var, scaling);
  }
  const int num_missed =
      num_fail_due_to_rhs + num_fail_due_to_large_multiplier +
      num_inequality_fail_due_to_large_multiplier +
      num_inequality_fail_due_to_lcm + num_inequality_fail_due_to_domain;
  if (num_missed > 0) {
    VLOG(1) << "num_new_integer: " << num_implied_integers
            << "\n num_processed_constraints: " << num_processed_constraints
            << "\n num_rhs_fail: " << num_fail_due_to_rhs
            << "\n num_multiplier_fail: " << num_fail_due_to_large_multiplier
            << "\n num_inequality_multiplier_fail: "
            << num_inequality_fail_due_to_large_multiplier
            << "\n num_inequality_fail_due_to_lcm: "
            << num_inequality_fail_due_to_lcm
            << "\n num_inequality_fail_due_to_domain: "
            << num_inequality_fail_due_to_domain;
    SOLVER_LOG(logger, "Missed ", num_missed, " potential implied integer.");
  }

  if (num_implied_integers > 0 || num_missed > 0 || num_fixed > 0) {
    const int num_integers = initial_num_integers + num_implied_integers;
    SOLVER_LOG(logger, "[ImpliedIntegers] Num integers: ", num_integers, "/",
               num_variables, " (implied:", num_implied_integers,
               " in_inequalities:", num_in_inequalities, " unused:", num_unused,
               " missed:", num_missed, " fixed:", num_fixed,
               " max_scaling:", max_scaling, ")");
  }

  ApplyVarScaling(var_scaling, mp_model->mutable_underlying_model());
  return var_scaling;
}

absl::Status ConstraintScaler::ScaleAndAddConstraint(
    const EnforcedLinearConstraint& mp_constraint, CpModelProto* cp_model) {
  absl::Span<const IntegerVariableProto* const> var_domains =
      absl::MakeSpan(cp_model->variables());

  // Quick check to see if any enforcement is trivially false.
  // This can happen if we tightenned the bounds afterwards.
  const int num_enforcements = mp_constraint.enforcement_vars().size();
  for (int i = 0; i < num_enforcements; ++i) {
    const int var = mp_constraint.enforcement_vars()[i];
    const int64_t value = mp_constraint.enforcement_values()[i];
    const int64_t var_lb = var_domains[var]->domain(0);
    const int64_t var_ub =
        var_domains[var]->domain(var_domains[var]->domain_size() - 1);
    if (value < var_lb || value > var_ub) return absl::OkStatus();
  }

  ConstraintProto* constraint = cp_model->add_constraints();
  const absl::Status status = ScaleAndAddConstraint(
      mp_constraint.vars(), mp_constraint.coeffs(), mp_constraint.lower_bound(),
      mp_constraint.upper_bound(), mp_constraint.name(), var_domains,
      constraint);
  if (!status.ok()) {
    return absl::Status(
        status.code(),
        absl::StrCat("[Scaling] ", status.message(), " MpConstraintProto: ",
                     ProtobufShortDebugString(mp_constraint.proto())));
  }

  if (!mp_constraint.IsEnforced()) return absl::OkStatus();

  bool has_integer_enforcement = false;
  for (int i = 0; i < num_enforcements; ++i) {
    const int var = mp_constraint.enforcement_vars()[i];
    const int64_t value = mp_constraint.enforcement_values()[i];
    const int64_t var_lb = var_domains[var]->domain(0);
    const int64_t var_ub =
        var_domains[var]->domain(var_domains[var]->domain_size() - 1);
    if (var_lb >= 0 && var_ub <= 1) {
      constraint->add_enforcement_literal(value == 1 ? var : NegatedRef(var));
    } else {
      has_integer_enforcement = true;
    }
  }

  // Add back the "integer" enforcement to constraint >= lb.
  //
  // Note that one issue is that this can break our overflow precondition, for
  // now we will return MODEL_INVALID when this happen.
  if (!has_integer_enforcement) return absl::OkStatus();

  // Enforced constraint should only be of the form >= rhs.
  const LinearConstraintProto& linear = constraint->linear();
  CHECK_EQ(linear.domain_size(), 2);
  CHECK_EQ(linear.domain(1), kint64max);

  // Compute minimum integer activity of the scaled constraint.
  int64_t min_integer_activity = 0;
  for (int i = 0; i < linear.coeffs().size(); ++i) {
    const int64_t coeff = linear.coeffs(i);
    if (coeff > 0) {
      min_integer_activity += coeff * static_cast<int64_t>(lower_bounds[i]);
    } else {
      min_integer_activity += coeff * static_cast<int64_t>(upper_bounds[i]);
    }
  }

  // If min_integer_activity >= rhs, the constraint is just trivial, no need
  // to add extra enforcement.
  const int64_t rhs = linear.domain(0);
  if (min_integer_activity >= rhs) {
    return absl::OkStatus();
  }

  const int64_t big_m = CapSub(rhs, min_integer_activity);
  int64_t rhs_offset = 0;
  auto* mutable_linear = constraint->mutable_linear();
  for (int i = 0; i < num_enforcements; ++i) {
    const int var = mp_constraint.enforcement_vars()[i];
    const int64_t value = mp_constraint.enforcement_values()[i];
    const int64_t var_lb = var_domains[var]->domain(0);
    const int64_t var_ub =
        var_domains[var]->domain(var_domains[var]->domain_size() - 1);
    if (var_lb >= 0 && var_ub <= 1) continue;

    lower_bounds.push_back(var_lb);
    upper_bounds.push_back(var_ub);
    mutable_linear->add_vars(var);
    if (value == var_lb) {
      // We add big_m * (var - var_lb);
      mutable_linear->add_coeffs(big_m);
      rhs_offset += var_lb;
    } else {
      // We add big_m * (var_ub - var);
      CHECK_EQ(value, var_ub);
      mutable_linear->add_coeffs(-big_m);
      rhs_offset -= var_ub;
    }
  }

  // Update the domain. If we ever cap here, the constraint should be
  // trivially true or false so we don't care.
  mutable_linear->set_domain(0, CapAdd(rhs, CapProd(rhs_offset, big_m)));

  // Check precondition and abort if it fail.
  //
  // TODO(user): This didn't fail on miplib so I didn't bother handling this
  // but an option is to recall the scaler a second time without extracting
  // integer enforcement.
  LinearOverflowChecker checker;
  for (int i = 0; i < mutable_linear->coeffs().size(); ++i) {
    if (!checker.AddTerm(mutable_linear->coeffs(i),
                         static_cast<int64_t>(lower_bounds[i]),
                         static_cast<int64_t>(upper_bounds[i]))) {
      return absl::Status(
          absl::StatusCode::kInvalidArgument,
          absl::StrCat("[Scaling] Overflow issue while adding back integer "
                       "enforcement. MpConstraintProto: ",
                       ProtobufShortDebugString(mp_constraint.proto())));
    }
  }

  return absl::OkStatus();
}

absl::Status ConstraintScaler::ScaleAndAddConstraint(
    absl::Span<const int> vars, absl::Span<const double> coeffs,
    double ct_lower_bound, double ct_upper_bound, absl::string_view name,
    absl::Span<const IntegerVariableProto* const> var_domains,
    ConstraintProto* constraint) {
  if (keep_names && !name.empty()) constraint->set_name(name);

  // We relax the constraint bound by the wanted_precision before scaling. Note
  // that this is needed because now that the scaled constraint activity is
  // integer, we will floor/ceil these bounds.
  //
  // It might make more sense to use a relative precision here for large bounds,
  // but absolute is usually what is used in the MIP world. Also if the problem
  // was a pure integer problem, and a user asked for sum == 10k, we want to
  // stay exact here.
  if (use_tight_bounds) {
    ct_lower_bound -= tight_precision;
    ct_upper_bound += tight_precision;
  } else {
    ct_lower_bound -= wanted_precision;
    ct_upper_bound += wanted_precision;
  }

  // Constraint statistic used below.
  double min_activity = 0.0;
  double max_activity = 0.0;

  // Transform the constraint and its variable into an easier to use form.
  var_indices.clear();
  coefficients.clear();
  lower_bounds.clear();
  upper_bounds.clear();
  const int num_coeffs = vars.size();
  for (int i = 0; i < num_coeffs; ++i) {
    const auto& var_proto = var_domains[vars[i]];
    const int64_t lb = var_proto->domain(0);
    const int64_t ub = var_proto->domain(var_proto->domain_size() - 1);
    if (lb == 0 && ub == 0) continue;

    const double coeff = coeffs[i];
    if (coeff == 0.0) continue;

    var_indices.push_back(vars[i]);
    coefficients.push_back(coeff);
    lower_bounds.push_back(lb);
    upper_bounds.push_back(ub);

    double min_term = coeff * static_cast<double>(lb);
    double max_term = coeff * static_cast<double>(ub);
    if (min_term > max_term) std::swap(min_term, max_term);

    min_activity += min_term;
    max_activity += max_term;
  }

  // Relax bound using activity and skip trivial constraints (with double
  // precision + tolerance).
  const double kInfinity = std::numeric_limits<double>::infinity();
  if (min_activity >= ct_lower_bound) {
    ct_lower_bound = -kInfinity;
  }
  if (max_activity <= ct_upper_bound) {
    ct_upper_bound = kInfinity;
  }
  auto* mutable_linear = constraint->mutable_linear();
  if (ct_lower_bound == -kInfinity && ct_upper_bound == kInfinity) {
    ++num_trivial_constraints;
    mutable_linear->add_domain(0);
    mutable_linear->add_domain(0);
    return absl::OkStatus();
  }

  // Finds the best scaling within given parameters.
  //
  // TODO(user): add an option to use the tighter error bounds of the
  // TightScalingErrorHelper to scale the coefficients less while staying under
  // the requested precision.
  double relative_coeff_error;
  double scaled_sum_error;
  const double scaling_factor = FindBestScalingAndComputeErrors(
      coefficients, lower_bounds, upper_bounds, scaling_target,
      wanted_precision, &relative_coeff_error, &scaled_sum_error);
  if (scaling_factor == 0.0) {
    return absl::InvalidArgumentError(
        "Scaling factor of zero while scaling constraint");
  }

  max_relative_coeff_error =
      std::max(relative_coeff_error, max_relative_coeff_error);
  max_scaling_factor = std::max(scaling_factor, max_scaling_factor);
  min_scaling_factor = std::min(scaling_factor, min_scaling_factor);
  max_absolute_rhs_error =
      std::max(max_absolute_rhs_error, scaled_sum_error / scaling_factor);

  if (use_tight_bounds) {
    tight_error_helper.LoadUnscaledConstraint(coefficients, lower_bounds,
                                              upper_bounds);
    tight_error_helper.LoadScalingFactor(scaling_factor);
  }

  // Write the scaled constraint content.
  int new_bound_size = 0;
  const int64_t gcd = ComputeGcdOfRoundedDoubles(coefficients, scaling_factor);
  for (int i = 0; i < coefficients.size(); ++i) {
    const double scaled_value = coefficients[i] * scaling_factor;
    const int64_t coeff = static_cast<int64_t>(std::round(scaled_value)) / gcd;
    if (coeff != 0) {
      mutable_linear->add_vars(var_indices[i]);
      mutable_linear->add_coeffs(coeff);

      lower_bounds[new_bound_size] = lower_bounds[i];
      upper_bounds[new_bound_size] = upper_bounds[i];
      ++new_bound_size;
    }
  }
  lower_bounds.resize(new_bound_size);
  upper_bounds.resize(new_bound_size);

  // Compute the error due to the discretization of the variables.
  double discretization_error = 0.0;
  {
    const LinearConstraintProto& linear = constraint->linear();
    const int num_terms = linear.vars().size();
    for (int i = 0; i < num_terms; ++i) {
      const int var = linear.vars(i);
      const int64_t coeff = linear.coeffs(i);
      if (var_is_integer[var]) continue;

      discretization_error +=
          static_cast<double>(std::abs(coeff)) * static_cast<double>(gcd);
    }
    discretization_error /= scaling_factor;
    max_discretization_error =
        std::max(max_discretization_error, discretization_error);
  }

  // Compute tighter rhs errors on the rounding(1)/unrounding(2) if asked.
  //
  // Note that the "unrounding" errors are just informative, these are the
  // worst error we could make when scaling back a feasible solution, but it
  // might happen than our optimal solution is well within the original bound
  // when unscaled.
  if (use_tight_bounds || use_proper_relaxation) {
    double lb_error1 = 0.0;
    double lb_error2 = 0.0;
    if (ct_lower_bound != -kInfinity) {
      lb_error1 = tight_error_helper.GetLowerBoundRoundingError(
                      ct_lower_bound * scaling_factor) /
                  scaling_factor;
      lb_error2 = tight_error_helper.GetLowerBoundUnroundingError(
                      ct_lower_bound * scaling_factor) /
                  scaling_factor;
    }
    double ub_error1 = 0.0;
    double ub_error2 = 0.0;
    if (ct_upper_bound != kInfinity) {
      ub_error1 = tight_error_helper.GetUpperBoundRoundingError(
                      ct_upper_bound * scaling_factor) /
                  scaling_factor;
      ub_error2 = tight_error_helper.GetUpperBoundUnroundingError(
                      ct_upper_bound * scaling_factor) /
                  scaling_factor;
    }

    max_rounding_error = std::max({max_rounding_error, lb_error1, ub_error1});
    max_unrounding_error =
        std::max({max_unrounding_error, lb_error2, ub_error2});

    // Relax the bound to never loose a feasible solution of the original
    // problem that is within tight_precision. However, we don't relax too much
    // if we are not precise, and just hope for the best
    if (use_proper_relaxation) {
      ct_lower_bound -= lb_error1 + discretization_error;
      ct_upper_bound += ub_error1 + discretization_error;
    } else {
      ct_lower_bound -= std::min(wanted_precision, lb_error1);
      ct_upper_bound += std::min(wanted_precision, ub_error1);
    }
  }

  // Add the constraint bounds. Because we are sure the scaled constraint fits
  // in an int64_t, if the scaled bounds are too large, the constraint is either
  // always true or always false.
  //
  // Note that since the min/max scaled activity magnitude should be under the
  // scaling_target (<< kint64max) it is okay to compare to kint64min/kint64max
  // casted to double.
  const Fractional scaled_lb = std::ceil(ct_lower_bound * scaling_factor);
  if (ct_lower_bound == kInfinity ||
      scaled_lb >= static_cast<double>(kint64max)) {
    // Corner case: infeasible model.
    mutable_linear->add_domain(kint64max);
  } else if (ct_lower_bound == -kInfinity ||
             scaled_lb <= static_cast<double>(kint64min)) {
    mutable_linear->add_domain(kint64min);
  } else {
    mutable_linear->add_domain(
        CeilRatio(IntegerValue(static_cast<int64_t>(scaled_lb)),
                  IntegerValue(gcd))
            .value());
  }

  const Fractional scaled_ub = std::floor(ct_upper_bound * scaling_factor);
  if (ct_upper_bound == -kInfinity ||
      scaled_ub <= static_cast<double>(kint64min)) {
    // Corner case: infeasible model.
    mutable_linear->add_domain(kint64min);
  } else if (ct_upper_bound == kInfinity ||
             scaled_ub >= static_cast<double>(kint64max)) {
    mutable_linear->add_domain(kint64max);
  } else {
    mutable_linear->add_domain(
        FloorRatio(IntegerValue(static_cast<int64_t>(scaled_ub)),
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
                             double tolerance, double fp_limit) {
  double multiplier = 1.0;
  const int64_t limit =
      static_cast<int64_t>(std::min(std::abs(fp_limit), 1e15));
  for (const double coeff : coefficients) {
    multiplier *=
        FindRationalFactor(multiplier * coeff, limit, multiplier * tolerance);
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
  // error of wanted_absolute_activity_precision and still makes sure we will
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
  // not be the best choice. It is also possible the user used rational coeffs
  // and then converted them to double (1/2, 1/3, 4/5, etc...). This scaling
  // will recover such rational input and might result in a smaller overall
  // coefficient which is good.
  //
  // Note that if our current precision is already above the requested one,
  // we choose integer scaling if we get a better precision.
  double integer_factor =
      FindFractionalScaling(coefficients, 1e-8, scaling_factor);
  DCHECK(std::isfinite(integer_factor));
  if (integer_factor != 0 && integer_factor < scaling_factor) {
    double local_relative_coeff_error;
    double local_scaled_sum_error;

    // As long as integer_factor * 2^x is smaller than scaling_factor, consider
    // it!
    bool use_rational = false;
    while (true) {
      ComputeScalingErrors(coefficients, lower_bounds, upper_bounds,
                           integer_factor, &local_relative_coeff_error,
                           &local_scaled_sum_error);

      if (local_scaled_sum_error <
          wanted_absolute_activity_precision * integer_factor) {
        // Smaller factor under the wanted precision, select it.
        use_rational = true;
        break;
      }

      if (integer_factor * 2 >= scaling_factor) break;
      integer_factor *= 2;
    }

    // If use_rational was set, or we have a smaller error, go for it.
    DCHECK_LT(integer_factor, scaling_factor);
    if (use_rational || local_scaled_sum_error * scaling_factor <=
                            *scaled_sum_error * integer_factor) {
      *relative_coeff_error = local_relative_coeff_error;
      *scaled_sum_error = local_scaled_sum_error;
      scaling_factor = integer_factor;
    }
  }

  DCHECK(std::isfinite(scaling_factor));
  return scaling_factor;
}

bool ClampVariableBounds(const SatParameters& params, MPModelProto* mp_model,
                         SolverLogger* logger, int* num_clamped) {
  // To make sure we cannot have integer overflow, we use this bound for any
  // unbounded variable.
  //
  // TODO(user): This could be made larger if needed, so be smarter if we have
  // MIP problems that we cannot "convert" because of this. Note however that we
  // cannot go that much further because we need to make sure we will not run
  // into overflow if we add a big linear combination of such variables. It
  // should always be possible for a user to scale their problem so that all
  // relevant quantities are a couple of millions. An LP/MIP solver has a
  // similar condition in disguise because problems with a difference of more
  // than 6 magnitudes between the variable values will likely run into numeric
  // trouble.
  const double kMaxVariableBound = std::round(params.mip_max_bound());
  CHECK_LT(kMaxVariableBound, static_cast<double>(kint64max));

  int num_truncated_bounds = 0;
  const int num_variables = mp_model->variable_size();
  for (int i = 0; i < num_variables; ++i) {
    MPVariableProto* mp_var = mp_model->mutable_variable(i);

    // Deal with the corner case of a domain far away from zero.
    //
    // TODO(user): We could avoid these cases by shifting the domain of
    // all variables to contain zero. This should also lead to a better scaling,
    // but it has some complications with integer variables and requires some
    // post-solve.
    if (mp_var->lower_bound() > kMaxVariableBound ||
        mp_var->upper_bound() < -kMaxVariableBound) {
      SOLVER_LOG(logger, "Error: variable ", *mp_var,
                 " is outside [-mip_max_bound..mip_max_bound]");
      return false;
    }

    if (mp_var->lower_bound() < -kMaxVariableBound) {
      ++num_truncated_bounds;
      mp_var->set_lower_bound(-kMaxVariableBound);
    }
    if (mp_var->upper_bound() > kMaxVariableBound) {
      ++num_truncated_bounds;
      mp_var->set_upper_bound(kMaxVariableBound);
    }
  }

  if (num_clamped != nullptr) *num_clamped = num_truncated_bounds;
  if (num_truncated_bounds > 0) {
    SOLVER_LOG(logger,
               "[Clamping] Warning: ", FormatCounter(num_truncated_bounds),
               " bounds were truncated to ",
               static_cast<int64_t>(kMaxVariableBound), ".");
  }
  return true;
}

void ExtractEnforcementLiterals(const SatParameters& params,
                                MPModelProtoWrapper* mp_model,
                                SolverLogger* logger) {
  const double precision = params.mip_use_tight_bounds()
                               ? params.mip_tight_precision()
                               : params.mip_wanted_precision();

  int64_t num_enforcements = 0;
  int64_t num_integer_enforcements = 0;
  double max_boolean_enforcement_magnitude = 0.0;
  double max_integer_enforcement_magnitude = 0.0;

  std::vector<int> to_remove;
  const int num_constraints = mp_model->num_constraints();
  for (int c = 0; c < num_constraints; ++c) {
    EnforcedLinearConstraint* ct = mp_model->mutable_constraints(c);

    double ct_lb = ct->lower_bound();
    double ct_ub = ct->upper_bound();
    if (ct_lb == -kInfinity && ct_ub == kInfinity) continue;

    double min_activity = 0.0;
    double max_activity = 0.0;
    double max_integer_magnitude = 0.0;
    const int num_terms = ct->num_terms();
    for (int i = 0; i < num_terms; ++i) {
      const int var = ct->vars(i);
      const double coeff = ct->coeffs(i);
      if (coeff == 0.0) continue;

      const MPVariableProto& mp_var = mp_model->variables(var);
      const double lb = mp_var.lower_bound();
      const double ub = mp_var.upper_bound();

      if (mp_var.is_integer() && lb < ub) {
        max_integer_magnitude =
            std::max(max_integer_magnitude, std::abs(coeff));
      }
      if (coeff > 0) {
        min_activity += coeff * lb;
        max_activity += coeff * ub;
      } else {
        min_activity += coeff * ub;
        max_activity += coeff * lb;
      }
    }

    // Relax bounds and skip trivial constraint.
    if (min_activity >= ct_lb - precision) ct_lb = -kInfinity;
    if (max_activity <= ct_ub + precision) ct_ub = kInfinity;
    if (ct_lb == -kInfinity && ct_ub == kInfinity) continue;

    // If the constraint is one-sided, we try to extract enforcement literals
    // before scaling !! This allows scaling the rest of the constraint better.
    // Especially since on some problems (like neos-3421095-cinca.mps) big-M
    // values are way too high (1e17 instead of 1e6).
    if (ct_lb != -kInfinity && ct_ub != kInfinity) continue;

    // Canonicalize to >= lb. Note that it shouldn't really matter that we
    // negate the constraint for CP-SAT.
    if (ct_lb == -kInfinity) {
      for (int i = 0; i < num_terms; ++i) {
        ct->set_coeffs(i, -ct->coeffs(i));
      }
      ct_lb = -ct_ub;
      ct_ub = kInfinity;
      std::swap(min_activity, max_activity);
      max_activity = -max_activity;
      min_activity = -min_activity;
    } else {
      CHECK_EQ(ct_ub, kInfinity);
    }
    ct->set_lower_bound(ct_lb);
    ct->set_upper_bound(ct_ub);

    to_remove.clear();
    const double limit = (ct_lb - precision) - min_activity;
    DCHECK_GE(limit, 0.0);
    if (max_integer_magnitude < limit) continue;

    for (int i = 0; i < num_terms; ++i) {
      const int var = ct->vars(i);
      const double coeff = ct->coeffs(i);
      if (std::abs(coeff) < limit) continue;

      // We only extract integer enforcement !!
      const MPVariableProto& mp_var = mp_model->variables(var);
      if (!mp_var.is_integer()) continue;

      // The bounds should be integer.
      const double double_lb = mp_var.lower_bound();
      const double double_ub = mp_var.upper_bound();
      if (double_lb >= double_ub) continue;
      if (double_lb < static_cast<double>(kint64min) ||
          double_ub > static_cast<double>(kint64max)) {
        continue;
      }
      if (double_lb != std::round(double_lb)) continue;
      if (double_ub != std::round(double_ub)) continue;
      const int64_t lb = static_cast<int64_t>(double_lb);
      const int64_t ub = static_cast<int64_t>(double_ub);

      if (lb != 0 || ub != 1) {
        ++num_integer_enforcements;
        max_integer_enforcement_magnitude =
            std::max(max_integer_enforcement_magnitude, std::abs(coeff));
      } else {
        ++num_enforcements;
        max_boolean_enforcement_magnitude =
            std::max(max_boolean_enforcement_magnitude, std::abs(coeff));
      }

      if (coeff > 0.0) {
        ct->AddEnforcement(var, lb);
        ct_lb -= coeff * lb;
      } else {
        ct->AddEnforcement(var, ub);
        ct_lb -= coeff * ub;
      }
      to_remove.push_back(i);
    }

    if (!to_remove.empty()) {
      ct->set_lower_bound(ct_lb);
      int new_size = 0;
      int r = 0;
      for (int i = 0; i < num_terms; ++i) {
        if (r < to_remove.size() && to_remove[r] == i) {
          ++r;
          continue;
        }
        ct->set_vars(new_size, ct->vars(i));
        ct->set_coeffs(new_size, ct->coeffs(i));
        ++new_size;
      }
      ct->resize(new_size);
    }
  }

  if (num_enforcements > 0) {
    SOLVER_LOG(logger, "[Enforcement] Extracted ",
               FormatCounter(num_enforcements),
               " enforcement literals, max_magnitude: ",
               max_boolean_enforcement_magnitude);
  }
  if (num_integer_enforcements > 0) {
    SOLVER_LOG(logger, "[Enforcement] Extracted ",
               FormatCounter(num_integer_enforcements),
               " 'integer' enforcements, max_magnitude: ",
               max_integer_enforcement_magnitude);
  }
}

bool ConvertMPModelProtoToCpModelProto(const SatParameters& params,
                                       const MPModelProtoWrapper& mp_model,
                                       int num_clamped, CpModelProto* cp_model,
                                       SolverLogger* logger) {
  CHECK(cp_model != nullptr);
  cp_model->Clear();
  cp_model->set_name(mp_model.name());

  const double kMaxVariableBound = std::round(params.mip_max_bound());
  int num_small_domains = 0;
  const int64_t kSmallDomainSize = 1000;

  const int num_variables = mp_model.num_variables();

  // This is only used to fill scaler.max_discretization_error.
  ConstraintScaler scaler;
  scaler.var_is_integer.assign(num_variables, false);

  // Add the variables.
  const bool keep_names = !params.ignore_names();
  for (int i = 0; i < num_variables; ++i) {
    const MPVariableProto& mp_var = mp_model.variables(i);
    IntegerVariableProto* cp_var = cp_model->add_variables();
    if (keep_names) cp_var->set_name(mp_var.name());

    // Bounds must have been clamped by ClampVariableBounds() first.
    const double var_lb = mp_var.lower_bound();
    const double var_ub = mp_var.upper_bound();
    CHECK_GE(var_lb, -kMaxVariableBound);
    CHECK_LE(var_ub, kMaxVariableBound);
    scaler.var_is_integer[i] = mp_var.is_integer();

    // Note that the cast is "perfect" because we forbid large values.
    if (params.mip_proper_relaxation() && !mp_var.is_integer()) {
      // If we have an feasible floating-point solution, we want the discretized
      // version where we always snap to the point with the best objective to be
      // feasible.
      if (mp_var.objective_coefficient() > 0.0) {
        cp_var->add_domain(static_cast<int64_t>(std::floor(var_lb)));
        cp_var->add_domain(static_cast<int64_t>(std::floor(var_ub)));
      } else if (mp_var.objective_coefficient() < 0.0) {
        cp_var->add_domain(static_cast<int64_t>(std::ceil(var_lb)));
        cp_var->add_domain(static_cast<int64_t>(std::ceil(var_ub)));
      } else {
        // We could choose an arbitrary direction here.
        cp_var->add_domain(static_cast<int64_t>(std::floor(var_lb)));
        cp_var->add_domain(static_cast<int64_t>(std::ceil(var_ub)));
      }
    } else {
      cp_var->add_domain(static_cast<int64_t>(
          std::ceil(var_lb - params.mip_wanted_precision())));
      cp_var->add_domain(static_cast<int64_t>(
          std::floor(var_ub + params.mip_wanted_precision())));
    }

    if (cp_var->domain(0) > cp_var->domain(1)) {
      LOG(WARNING) << "Variable #" << i << " cannot take an integer value. "
                   << ProtobufShortDebugString(mp_var);
      return false;
    }

    // Notify if a continuous variable has a small domain as this is likely to
    // make an all integer solution far from a continuous one.
    if (!mp_var.is_integer()) {
      const double diff = var_ub - var_lb;
      if (diff > params.mip_wanted_precision() && diff < kSmallDomainSize) {
        ++num_small_domains;
      }
    }
  }

  if (num_small_domains > 0) {
    SOLVER_LOG(logger, "[Scaling] Warning: ", FormatCounter(num_small_domains),
               " continuous variable domains with fewer than ",
               kSmallDomainSize, " values.");
  }

  scaler.wanted_precision = params.mip_wanted_precision();
  scaler.use_tight_bounds = params.mip_use_tight_bounds();
  scaler.use_proper_relaxation = params.mip_proper_relaxation();
  scaler.tight_precision = params.mip_tight_precision();
  scaler.scaling_target = int64_t{1} << params.mip_max_activity_exponent();
  scaler.keep_names = keep_names;

  // Add the constraints. We scale each of them individually.
  const int num_constraints = mp_model.num_constraints();
  for (int c = 0; c < num_constraints; ++c) {
    const EnforcedLinearConstraint& mp_constraint = mp_model.constraints(c);
    if (ConstraintIsAlwaysTrue(mp_constraint.proto())) continue;

    const absl::Status status =
        scaler.ScaleAndAddConstraint(mp_constraint, cp_model);
    if (!status.ok()) {
      SOLVER_LOG(logger, status.message());
      return false;
    }
  }

  if (scaler.num_trivial_constraints > 0) {
    SOLVER_LOG(
        logger, "[Scaling] Skipped ",
        FormatCounter(scaler.num_trivial_constraints),
        " trivial constraints (activity computed with doubles, tolerance=",
        params.mip_tight_precision() ? params.mip_tight_precision()
                                     : params.mip_wanted_precision(),
        ").");
  }

  // Display the error/scaling on the constraints.
  SOLVER_LOG(logger,
             "[Scaling] Maximum constraint coefficient relative error: ",
             scaler.max_relative_coeff_error);
  SOLVER_LOG(logger, "[Scaling] Maximum constraint worst-case activity error: ",
             scaler.max_absolute_rhs_error,
             (scaler.max_absolute_rhs_error > params.mip_check_precision()
                  ? " [Potentially IMPRECISE]"
                  : ""));
  if (params.mip_use_tight_bounds()) {
    const bool imprecise =
        scaler.max_rounding_error > params.mip_wanted_precision() ||
        scaler.max_unrounding_error > params.mip_wanted_precision();
    SOLVER_LOG(logger, "[Scaling] Tighter worst case rounding_error: ",
               scaler.max_rounding_error,
               " unrounding_error: ", scaler.max_unrounding_error,
               (imprecise ? " [IMPRECISE]" : ""));
  }
  {
    const bool imprecise =
        scaler.max_discretization_error > params.mip_wanted_precision();
    SOLVER_LOG(
        logger,
        "[Scaling] Maximum constraint error due to variable discretization: ",
        scaler.max_discretization_error, (imprecise ? " [IMPRECISE]" : ""));
  }
  SOLVER_LOG(logger, "[Scaling] Constraint scaling factor range: [",
             scaler.min_scaling_factor, ", ", scaler.max_scaling_factor, "]");

  // Our check precision is pretty large, but this should still be usable in
  // practice. Especially since these are worst case bounds, solution might be
  // good.
  const double sum_of_errors = scaler.max_rounding_error +
                               scaler.max_unrounding_error +
                               scaler.max_discretization_error;
  if (num_clamped == 0 && sum_of_errors < params.mip_check_precision()) {
    // TODO(user): The objective scaling is not counted here and can be way off
    // though, see for example australia-abs-cta.pb.gz. But we should still
    // report correct lower bound as we correct for the error with
    // params.mip_compute_true_objective_bound().
    SOLVER_LOG(logger, "[Scaling] SUCCESS! No clamped variable and errors <= ",
               scaler.max_rounding_error + scaler.max_unrounding_error +
                   scaler.max_discretization_error);
  }

  // Since cp_model supports a floating point objective, we use that. This will
  // allow us to scale the objective a bit later so we can potentially do more
  // domain reduction first.
  auto* float_objective = cp_model->mutable_floating_point_objective();
  const double scaling = mp_model.objective_scaling();
  float_objective->set_maximize(scaling < 0.0);
  if (mp_model.objective_offset() != 0.0) {
    float_objective->set_offset(mp_model.objective_offset() * scaling);
  }
  for (int i = 0; i < num_variables; ++i) {
    const MPVariableProto& mp_var = mp_model.variables(i);
    if (mp_var.objective_coefficient() != 0.0) {
      float_objective->add_vars(i);
      float_objective->add_coeffs(mp_var.objective_coefficient() * scaling);
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

  // Copy constraints.
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
                  "likely indicates an infinite coefficient in the objective.";
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
               ") or reduce your variable ranges and/or objective "
               "coefficient. We will continue the solve, but the final "
               "objective value might be off.");
  }

  const int64_t mult = maximize ? -1 : 1;
  const double new_offset = objective_offset * scaling_factor / gcd * mult;
  const double new_scaling_factor = 1.0 / scaling_factor * gcd * mult;
  if (!std::isfinite(new_offset) || !std::isfinite(new_scaling_factor)) {
    LOG(ERROR)
        << "Non-finite offset or scaling factor while scaling objective!";
    return false;
  }

  // Note that here we set the scaling factor for the inverse operation of
  // getting the "true" objective value from the scaled one. Hence the
  // inverse.
  auto* objective_proto = cp_model->mutable_objective();
  objective_proto->set_offset(new_offset);
  objective_proto->set_scaling_factor(new_scaling_factor);
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

double ComputeTrueObjectiveLowerBound(
    const CpModelProto& model_proto_with_floating_point_objective,
    const CpObjectiveProto& integer_objective,
    const int64_t inner_integer_objective_lower_bound) {
  const CpModelProto& proto = model_proto_with_floating_point_objective;
  const FloatObjectiveProto& float_obj = proto.floating_point_objective();
  const double mult = float_obj.maximize() ? -1.0 : 1.0;

  // Extract the objective terms. Since the floating-point objective is
  // user-given, we need to handle duplicate variable entries.
  std::vector<double> scaled_coeffs;
  std::vector<double> rounded_coeffs;
  std::vector<double> lbs;
  std::vector<double> ubs;

  std::vector<int> var_to_index(proto.variables().size(), -1);
  for (int i = 0; i < float_obj.vars().size(); ++i) {
    const int var = float_obj.vars(i);
    const double coeff = mult * float_obj.coeffs(i);
    if (coeff == 0.0) continue;
    if (var_to_index[var] == -1) {
      var_to_index[var] = scaled_coeffs.size();
      scaled_coeffs.push_back(coeff);
      rounded_coeffs.push_back(0.0);
      const auto& domain = proto.variables(var).domain();
      lbs.push_back(static_cast<double>(domain[0]));
      ubs.push_back(static_cast<double>(domain[domain.size() - 1]));
    } else {
      scaled_coeffs[var_to_index[var]] += coeff;
    }
  }
  for (int i = 0; i < integer_objective.vars().size(); ++i) {
    const int var = integer_objective.vars(i);
    const int index = var_to_index[var];
    if (index != -1) {
      rounded_coeffs[index] += static_cast<double>(integer_objective.coeffs(i));
    }
  }

  // Filter out terms that cancelled out to zero.
  int new_size = 0;
  for (int i = 0; i < scaled_coeffs.size(); ++i) {
    if (scaled_coeffs[i] == 0.0) {
      // This shouldn't happen.
      CHECK_EQ(rounded_coeffs[i], 0.0);
      continue;
    }
    scaled_coeffs[new_size] = scaled_coeffs[i];
    rounded_coeffs[new_size] = rounded_coeffs[i];
    lbs[new_size] = lbs[i];
    ubs[new_size] = ubs[i];
    ++new_size;
  }
  scaled_coeffs.resize(new_size);
  rounded_coeffs.resize(new_size);
  lbs.resize(new_size);
  ubs.resize(new_size);

  TightScalingErrorHelper helper;
  helper.LoadBothConstraints(scaled_coeffs, rounded_coeffs, lbs, ubs);
  return float_obj.offset() +
         mult * helper.GetScaledLowerBoundFromRoundedLowerBound(
                    static_cast<double>(inner_integer_objective_lower_bound));
}

std::vector<int> PartitionGraph(
    int n, absl::Span<const EdgeConstraintForPartition> edges) {
  struct ArConstraintForPartition {
    int target;
    bool same_partition;
  };
  CompactVectorVectorBuilder<int, ArConstraintForPartition> graph_builder;
  for (const EdgeConstraintForPartition& edge : edges) {
    DCHECK_GE(edge.a, 0);
    DCHECK_LT(edge.a, n);
    DCHECK_GE(edge.b, 0);
    DCHECK_LT(edge.b, n);
    graph_builder.Add(edge.a,
                      ArConstraintForPartition{edge.b, edge.same_partition});
    graph_builder.Add(edge.b,
                      ArConstraintForPartition{edge.a, edge.same_partition});
  }
  const CompactVectorVector<int, ArConstraintForPartition> adj(graph_builder,
                                                               n);

  // -2: unvisited, -1: failed component, 0: partition R1, 1: partition R2.
  std::vector<int> partition(n, -2);
  std::vector<int> bfs_queue;

  for (int start_node = 0; start_node < n; ++start_node) {
    if (partition[start_node] != -2) continue;
    if (adj[start_node].empty()) {
      partition[start_node] = 0;
      continue;
    }

    int head = 0;
    bool conflict = false;
    bfs_queue.clear();
    bfs_queue.push_back(start_node);
    partition[start_node] = 0;

    while (head < bfs_queue.size()) {
      const int u = bfs_queue[head++];
      const int color_u = partition[u];

      for (const ArConstraintForPartition& edge : adj[u]) {
        const int v = edge.target;
        const int expected_color_v =
            edge.same_partition ? color_u : (1 - color_u);
        if (partition[v] == -2) {
          partition[v] = expected_color_v;
          bfs_queue.push_back(v);
        } else if (partition[v] != expected_color_v) {
          conflict = true;
        }
      }
    }

    if (conflict) {
      for (const int u : bfs_queue) {
        partition[u] = -1;
      }
    }
  }

  return partition;
}

namespace {

// Iteratively removes rows and columns with at most 1 active entry.
// `row_matrix` must be the transpose of `col_matrix`.
void PeelSingletons(const glop::CompactSparseMatrix& col_matrix,
                    const glop::CompactSparseMatrix& row_matrix,
                    std::vector<bool>* col_deleted,
                    std::vector<bool>* row_deleted) {
  const int num_cols = col_matrix.num_cols().value();
  const int num_rows = col_matrix.num_rows().value();
  col_deleted->assign(num_cols, false);
  row_deleted->assign(num_rows, false);

  std::vector<int> col_degree(num_cols, 0);
  std::vector<int> row_degree(num_rows, 0);
  std::vector<int> cols_to_peel;
  std::vector<int> rows_to_peel;

  for (int col = 0; col < num_cols; ++col) {
    col_degree[col] = col_matrix.ColumnNumEntries(ColIndex(col)).value();
    if (col_degree[col] <= 1) {
      (*col_deleted)[col] = true;
      cols_to_peel.push_back(col);
    }
  }
  for (int row = 0; row < num_rows; ++row) {
    row_degree[row] = row_matrix.ColumnNumEntries(ColIndex(row)).value();
    if (row_degree[row] <= 1) {
      (*row_deleted)[row] = true;
      rows_to_peel.push_back(row);
    }
  }

  while (!cols_to_peel.empty() || !rows_to_peel.empty()) {
    while (!rows_to_peel.empty()) {
      const int row = rows_to_peel.back();
      rows_to_peel.pop_back();
      for (const auto e : row_matrix.column(ColIndex(row))) {
        const int col = e.row().value();
        if (!(*col_deleted)[col] && --col_degree[col] <= 1) {
          (*col_deleted)[col] = true;
          cols_to_peel.push_back(col);
        }
      }
    }
    while (!cols_to_peel.empty()) {
      const int col = cols_to_peel.back();
      cols_to_peel.pop_back();
      for (const auto e : col_matrix.column(ColIndex(col))) {
        const int row = e.row().value();
        if (!(*row_deleted)[row] && --row_degree[row] <= 1) {
          (*row_deleted)[row] = true;
          rows_to_peel.push_back(row);
        }
      }
    }
  }
}

// Attempts to find positive `col_scale` (with min(col_scale) == 1.0) and
// `row_scale` such that for every entry (r, c) with value `coeff`,
// |coeff| == row_scale[r] * col_scale[c].
// `row_matrix` must be the transpose of `col_matrix`, and the bipartite graph
// must be connected when `num_rows > 0`.
bool ComputeRowAndColScales(const glop::CompactSparseMatrix& col_matrix,
                            const glop::CompactSparseMatrix& row_matrix,
                            double tolerance, std::vector<double>* col_scale,
                            std::vector<double>* row_scale) {
  const int num_cols = col_matrix.num_cols().value();
  const int num_rows = col_matrix.num_rows().value();
  col_scale->assign(num_cols, 0.0);
  row_scale->assign(num_rows, 0.0);
  if (num_rows == 0) {
    col_scale->assign(num_cols, 1.0);
    return true;
  }

  std::vector<int> col_queue;
  std::vector<int> row_queue;
  (*col_scale)[0] = 1.0;
  col_queue.push_back(0);
  double min_col_scale = 1.0;

  while (!col_queue.empty() || !row_queue.empty()) {
    while (!col_queue.empty()) {
      const int col = col_queue.back();
      col_queue.pop_back();
      const double c_scale = (*col_scale)[col];
      for (const auto e : col_matrix.column(ColIndex(col))) {
        const int row = e.row().value();
        const double abs_coeff = std::abs(e.coefficient());
        if ((*row_scale)[row] == 0.0) {
          const double r_scale = abs_coeff / c_scale;
          if (r_scale <= 0.0 || !std::isfinite(r_scale)) return false;
          (*row_scale)[row] = r_scale;
          row_queue.push_back(row);
        } else if (std::abs((*row_scale)[row] * c_scale - abs_coeff) >
                   tolerance * abs_coeff) {
          return false;
        }
      }
    }
    while (!row_queue.empty()) {
      const int row = row_queue.back();
      row_queue.pop_back();
      const double r_scale = (*row_scale)[row];
      for (const auto e : row_matrix.column(ColIndex(row))) {
        const int col = e.row().value();
        const double abs_coeff = std::abs(e.coefficient());
        if ((*col_scale)[col] == 0.0) {
          const double c_scale = abs_coeff / r_scale;
          if (c_scale <= 0.0 || !std::isfinite(c_scale)) return false;
          (*col_scale)[col] = c_scale;
          min_col_scale = std::min(min_col_scale, c_scale);
          col_queue.push_back(col);
        } else if (std::abs(r_scale * (*col_scale)[col] - abs_coeff) >
                   tolerance * abs_coeff) {
          return false;
        }
      }
    }
  }

  if (min_col_scale != 1.0) {
    for (double& c_scale : *col_scale) {
      c_scale /= min_col_scale;
      if (c_scale <= 0.0 || !std::isfinite(c_scale)) return false;
    }
    for (double& r_scale : *row_scale) {
      r_scale *= min_col_scale;
      if (r_scale <= 0.0 || !std::isfinite(r_scale)) return false;
    }
  }
  return true;
}

enum class ModularityStatus {
  kNotTwoModular = 0,
  kTotallyUnimodular = 1,
  kTwoModular = 2,
};

// Checks Heller & Tompkins's criterion on the active (unpeeled) entries of
// `matrix`, assuming all entries have been validated to scale to {-1, +1} and
// every active column has at least 2 active entries.
ModularityStatus CheckHellerTompkins(const glop::CompactSparseMatrix& matrix,
                                     const std::vector<bool>& col_deleted,
                                     const std::vector<bool>& row_deleted) {
  std::vector<EdgeConstraintForPartition> edges;
  for (ColIndex col(0); col < matrix.num_cols(); ++col) {
    if (col_deleted[col.value()]) continue;
    int rows[2];
    double vals[2];
    int size = 0;
    for (const auto e : matrix.column(col)) {
      if (row_deleted[e.row().value()]) continue;
      if (size == 2) return ModularityStatus::kNotTwoModular;
      rows[size] = e.row().value();
      vals[size] = e.coefficient();
      ++size;
    }
    if (size == 2) {
      // Same sign: must be in different subsets.
      // Opposite signs: must be in the same subset.
      edges.push_back({rows[0], rows[1],
                       /*same_partition=*/(vals[0] > 0.0) != (vals[1] > 0.0)});
    }
  }
  if (edges.empty()) return ModularityStatus::kTotallyUnimodular;

  const std::vector<int> partition =
      PartitionGraph(matrix.num_rows().value(), edges);
  for (const EdgeConstraintForPartition& edge : edges) {
    if (partition[edge.a] == -1) return ModularityStatus::kTwoModular;
  }
  return ModularityStatus::kTotallyUnimodular;
}

ModularityStatus CheckComponentModularity(
    const glop::CompactSparseMatrix& col_matrix,
    const glop::CompactSparseMatrix& row_matrix) {
  std::vector<bool> col_deleted;
  std::vector<bool> row_deleted;
  PeelSingletons(col_matrix, row_matrix, &col_deleted, &row_deleted);
  const ModularityStatus col_status =
      CheckHellerTompkins(col_matrix, col_deleted, row_deleted);
  if (col_status == ModularityStatus::kTotallyUnimodular) {
    return ModularityStatus::kTotallyUnimodular;
  }
  const ModularityStatus row_status =
      CheckHellerTompkins(row_matrix, row_deleted, col_deleted);
  if (row_status == ModularityStatus::kTotallyUnimodular) {
    return ModularityStatus::kTotallyUnimodular;
  }
  if (col_status == ModularityStatus::kTwoModular ||
      row_status == ModularityStatus::kTwoModular) {
    return ModularityStatus::kTwoModular;
  }
  return ModularityStatus::kNotTwoModular;
}

}  // namespace

std::vector<double> MarkResidualTotallyUnimodularVariableAsInteger(
    const SatParameters& params, MPModelProto* mp_model, SolverLogger* logger) {
  if (!mp_model->general_constraint().empty()) return {};
  if (params.keep_all_feasible_solutions_in_presolve()) return {};

  // 1. Identify and re-index continuous variables.
  const int num_vars = mp_model->variable_size();
  std::vector<int> to_dense_index(num_vars, -1);
  std::vector<int> continuous_var_indices;
  for (int var = 0; var < num_vars; ++var) {
    if (mp_model->variable(var).is_integer()) continue;

    to_dense_index[var] = continuous_var_indices.size();
    continuous_var_indices.push_back(var);
  }
  if (continuous_var_indices.empty()) return {};

  // 2. Partition continuous variables and constraints into connected
  // components.
  const int num_cols = continuous_var_indices.size();
  DenseConnectedComponentsFinder components;
  components.SetNumberOfNodes(num_cols);

  const int num_constraints = mp_model->constraint_size();
  std::vector<int> row_first_col(num_constraints, -1);
  for (int c = 0; c < num_constraints; ++c) {
    const MPConstraintProto& constraint = mp_model->constraint(c);
    int first_col = -1;
    for (int i = 0; i < constraint.var_index_size(); ++i) {
      if (constraint.coefficient(i) == 0.0) continue;
      const int col = to_dense_index[constraint.var_index(i)];
      if (col == -1) continue;  // Skip discrete variables.
      if (first_col == -1) {
        first_col = col;
      } else {
        components.AddEdge(first_col, col);
      }
    }
    row_first_col[c] = first_col;
  }

  const int num_components = components.GetNumberOfComponents();
  const std::vector<int> col_to_components = components.GetComponentIds();
  std::vector<std::vector<int>> compo_vars(num_components);
  std::vector<int> var_to_local_col(num_vars, -1);
  for (int col = 0; col < num_cols; ++col) {
    const int compo = col_to_components[col];
    const int var = continuous_var_indices[col];
    var_to_local_col[var] = compo_vars[compo].size();
    compo_vars[compo].push_back(var);
  }
  std::vector<std::vector<int>> compo_rows(num_components);
  for (int c = 0; c < num_constraints; ++c) {
    if (row_first_col[c] != -1) {
      compo_rows[col_to_components[row_first_col[c]]].push_back(c);
    }
  }

  // 3. Process each connected component independently.
  glop::CompactSparseMatrix row_matrix;
  glop::CompactSparseMatrix col_matrix;
  std::vector<double> col_scale;
  std::vector<double> row_scale;
  std::vector<double> non_integer_values;

  const double tolerance = params.mip_wanted_precision();
  int num_tum_components = 0;
  int num_2_modular_components = 0;
  int num_new_tum_integers = 0;
  int num_new_2_modular_integers = 0;
  int num_common_scaled = 0;
  double min_common_scaling = std::numeric_limits<double>::infinity();
  double max_common_scaling = 0.0;
  bool any_scaled = false;
  std::vector<double> var_scaling(num_vars, 1.0);

  // TODO(user): There is a bad interaction with the bound propagator that
  // propagates bound using tolerances, so that they are not exactly integer :(
  // find a cleaner way ?
  const auto is_almost_integer = [&params](double x) {
    return !std::isfinite(x) ||
           std::abs(x - std::round(x)) <= params.mip_wanted_precision();
  };

  for (int compo = 0; compo < num_components; ++compo) {
    // Computes this component's row_matrix and col_matrix.
    row_matrix.Reset(RowIndex(compo_vars[compo].size()));
    for (const int c : compo_rows[compo]) {
      const MPConstraintProto& constraint = mp_model->constraint(c);
      for (int i = 0; i < constraint.var_index_size(); ++i) {
        const double coeff = constraint.coefficient(i);
        if (coeff == 0.0) continue;
        const int local_col = var_to_local_col[constraint.var_index(i)];
        if (local_col == -1) continue;
        row_matrix.AddEntryToCurrentColumn(RowIndex(local_col), coeff);
      }
      row_matrix.CloseCurrentColumn();
    }
    col_matrix.PopulateFromTranspose(row_matrix);

    // Can we scale it to only +1/-1?
    if (!ComputeRowAndColScales(col_matrix, row_matrix, tolerance, &col_scale,
                                &row_scale)) {
      continue;
    }

    // Is the scaled matrix TUM or 2-modular? Note that positive row/column
    // scaling preserves both the sparsity pattern and entry signs, which is all
    // CheckComponentModularity inspects.
    const ModularityStatus status =
        CheckComponentModularity(col_matrix, row_matrix);
    if (status == ModularityStatus::kNotTwoModular) {
      continue;
    }
    if (status == ModularityStatus::kTotallyUnimodular) {
      ++num_tum_components;
    } else {
      ++num_2_modular_components;
    }

    // Collect bounds and discrete coefficients in this component, pre-scaled
    // by `col_scale` and `1 / row_scale`.
    bool has_col_scaling = false;
    double max_bound = 0.0;
    non_integer_values.clear();
    auto record_value = [&](double v) {
      if (!is_almost_integer(v)) {
        non_integer_values.push_back(std::abs(v));
      }
    };
    for (int local_col = 0; local_col < compo_vars[compo].size(); ++local_col) {
      const int var = compo_vars[compo][local_col];
      const double c_scale = col_scale[local_col];
      if (std::abs(c_scale - 1.0) > tolerance) has_col_scaling = true;
      max_bound = std::max(max_bound, c_scale);
      record_value(c_scale);

      const double lb = mp_model->variable(var).lower_bound() * c_scale;
      const double ub = mp_model->variable(var).upper_bound() * c_scale;
      if (std::isfinite(lb)) max_bound = std::max(max_bound, std::abs(lb));
      if (std::isfinite(ub)) max_bound = std::max(max_bound, std::abs(ub));
      record_value(lb);
      record_value(ub);
    }
    for (int local_row = 0; local_row < compo_rows[compo].size(); ++local_row) {
      const int c = compo_rows[compo][local_row];
      const MPConstraintProto& constraint = mp_model->constraint(c);
      const double r_scale = row_scale[local_row];
      record_value(constraint.lower_bound() / r_scale);
      record_value(constraint.upper_bound() / r_scale);
      for (int i = 0; i < constraint.var_index_size(); ++i) {
        const double coeff = constraint.coefficient(i);
        if (coeff == 0.0) continue;
        if (var_to_local_col[constraint.var_index(i)] == -1) {
          record_value(coeff / r_scale);
        }
      }
    }

    // Determine whether we can mark variables as integer (possibly with a
    // rational scaling factor) or apply a common component power-of-two factor.
    const double base_factor =
        status == ModularityStatus::kTwoModular ? 2.0 : 1.0;
    const double max_s =
        params.mip_max_bound() / (base_factor * std::max(1.0, max_bound));
    double component_multiplier = 1.0;
    bool mark_as_integer = false;

    if (non_integer_values.empty() && base_factor == 1.0 && !has_col_scaling) {
      mark_as_integer = true;
    } else if (params.mip_automatically_scale_variables() && max_s >= 1.0) {
      const int64_t int_limit =
          max_s >= static_cast<double>(kint64max)
              ? kint64max
              : CapAdd(static_cast<int64_t>(std::floor(max_s)), 1);
      double s = 1.0;
      for (const double v : non_integer_values) {
        const int64_t f = FindRationalFactor(s * v, int_limit, s * tolerance);
        if (f == 0 || s * f > max_s) {
          s = 0.0;
          break;
        }
        s *= f;
      }
      if (s > 0.0) {
        component_multiplier = base_factor * s;
        mark_as_integer = true;
      }
    }

    if (!mark_as_integer && !has_col_scaling &&
        params.mip_scale_to_max_bound()) {
      if (max_bound > 0.0 && max_bound <= params.mip_max_bound()) {
        const int exp =
            HighestPowerOfTwoAtOrBelow(params.mip_max_bound() / max_bound);
        component_multiplier = std::ldexp(1.0, exp);
      }
    }

    for (int local_col = 0; local_col < compo_vars[compo].size(); ++local_col) {
      const int var = compo_vars[compo][local_col];
      const double scaling =
          mark_as_integer
              ? std::round(component_multiplier * col_scale[local_col])
              : component_multiplier;
      if (scaling != 1.0) {
        var_scaling[var] = scaling;
        any_scaled = true;
      }
      if (mark_as_integer) {
        mp_model->mutable_variable(var)->set_is_integer(true);
        if (status == ModularityStatus::kTotallyUnimodular) {
          ++num_new_tum_integers;
        } else {
          ++num_new_2_modular_integers;
        }
      } else if (scaling != 1.0) {
        ++num_common_scaled;
        min_common_scaling = std::min(min_common_scaling, scaling);
        max_common_scaling = std::max(max_common_scaling, scaling);
      }
    }
  }

  if (num_tum_components + num_2_modular_components == 0) {
    if (num_components > 1) {
      SOLVER_LOG(logger, "[TUM] 0/", num_components,
                 " continuous residual matrix with simple structure.");
    }
    return {};
  }

  SOLVER_LOG(logger, "[TUM] ", num_tum_components, "/", num_components,
             " continuous residual matrix are Totally Unimodular; +",
             num_new_tum_integers, " variables marked as integer.");
  if (num_new_2_modular_integers > 0) {
    SOLVER_LOG(logger, "[TUM] ", num_2_modular_components, "/",
               num_components - num_tum_components,
               " components left are 2-modular; +", num_new_2_modular_integers,
               " variable scaled and marked as integer.");
  }
  if (num_common_scaled > 0) {
    SOLVER_LOG(logger, "[TUM] ", num_common_scaled,
               " variables scaled with a common component factor in [",
               min_common_scaling, ", ", max_common_scaling, "].");
  }

  if (any_scaled) {
    ApplyVarScaling(var_scaling, mp_model);
    return var_scaling;
  }
  return {};
}

}  // namespace sat
}  // namespace operations_research
