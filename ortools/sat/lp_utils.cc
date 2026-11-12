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
#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/accurate_sum.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/types.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/graph/connected_components.h"
#include "ortools/linear_solver/linear_solver.pb.h"
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
#include "ortools/util/full_precision_inequalities.h"
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

bool ConstraintIsAlwaysTrue(const MPConstraintProto& mp_constraint) {
  return mp_constraint.lower_bound() == -kInfinity &&
         mp_constraint.upper_bound() == kInfinity;
}

std::pair<double, double> DivInterval(double lb, double ub, double divisor) {
  if (divisor < 0.0) return DivInterval(-ub, -lb, -divisor);
  return {lb == 0.0 ? 0.0 : ExactScaledSumLb(lb, 0.0, divisor),
          ub == 0.0 ? 0.0 : ExactScaledSumUb(ub, 0.0, divisor)};
}

double MaxAbsScaledTightSum(const TightSum& sum, double pos_divisor = 1.0) {
  const auto [lb, ub] = sum.TightBounds();
  const auto [dlb, dub] = DivInterval(lb, ub, pos_divisor);
  return std::max(std::abs(dlb), std::abs(dub));
}

// Rounds a scaled lower bound `y` to an integer, given the allowed tolerance
// `tol >= 0` in the same scaled space:
// - Under `proper_relaxation`, enlarges downward to `floor(y)`, except when `y`
//   is within `tol` below `round(y)` (snapping to `round(y)`).
// - Otherwise, restricts inward to `ceil(y)`, except when `y` is within `tol`
//   above `round(y)` (snapping to `round(y)`).
// Upper bounds `w <= u` are handled symmetrically by passing `-u` and negating
// the result (since `std::round(-u) == -std::round(u)`).
double RoundScaledLowerBound(double y, double tol, bool proper_relaxation) {
  if (!std::isfinite(y)) return y;
  const double r = std::round(y);
  const double w = proper_relaxation
                       ? (TightSum().Add(r).Add(-y) <= tol ? r : std::floor(y))
                       : (TightSum().Add(y).Add(-r) <= tol ? r : std::ceil(y));
  return w == 0.0 ? 0.0 : w;
}

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

double ComputeContinuousVarScaling(const SatParameters& params,
                                   const MipVariableState& state,
                                   double max_bound) {
  if (state.is_integer) return 1.0;
  const double scaling = params.mip_var_scaling();
  if (max_bound == std::numeric_limits<double>::infinity()) {
    return scaling;
  }
  const double magnitude = std::max(std::abs(state.lb), std::abs(state.ub));
  if (magnitude == 0 || magnitude > max_bound) return 1.0;
  return std::min(scaling, max_bound / magnitude);
}

// Multiplies the current domain of `state` by `scaling`.
void ScaleVariableState(double scaling, MipVariableState* state) {
  if (scaling == 1.0) return;
  state->lb *= scaling;
  state->ub *= scaling;
  state->scaling *= scaling;
}

// Fills the given constraint transformed according to `states`, see the
// formula in MipVariableState. Terms with a zero coefficient are kept so that
// the result is aligned with `ct.vars()`.
void TransformConstraint(const MPConstraintProto& ct,
                         absl::Span<const MipVariableState> states,
                         std::vector<double>* coeffs, double* lb, double* ub) {
  const int num_terms = ct.var_index_size();
  coeffs->resize(num_terms);
  TightSum neg_offset;
  bool has_offset = false;
  for (int i = 0; i < num_terms; ++i) {
    const MipVariableState& state = states[ct.var_index(i)];
    const double coeff = ct.coefficient(i);
    (*coeffs)[i] = coeff / state.scaling;
    if (coeff != 0.0 && state.shift != 0.0) {
      neg_offset.AddProd(-coeff, state.shift);
      has_offset = true;
    }
  }
  *lb = has_offset && std::isfinite(ct.lower_bound())
            ? TightSum(neg_offset).Add(ct.lower_bound()).TightLowerBound()
            : ct.lower_bound();
  *ub = has_offset && std::isfinite(ct.upper_bound())
            ? TightSum(neg_offset).Add(ct.upper_bound()).TightUpperBound()
            : ct.upper_bound();
}

}  // namespace

ScopedVarToLocalCol::ScopedVarToLocalCol(ResidualContinuousComponent* compo,
                                         std::vector<int>* buffer)
    : compo_(compo), buffer_(buffer) {
  for (int local_col = 0; local_col < compo_->continuous_vars.size();
       ++local_col) {
    const int var = compo_->continuous_vars[local_col];
    DCHECK_EQ((*buffer_)[var], -1);
    (*buffer_)[var] = local_col;
  }
  compo_->var_to_local_col = *buffer_;
}

ScopedVarToLocalCol::~ScopedVarToLocalCol() {
  for (const int var : compo_->continuous_vars) (*buffer_)[var] = -1;
  compo_->var_to_local_col = {};
}

std::vector<MipVariableState> InitialVariableStates(
    const MPModelProtoWrapper& mp_model) {
  const int num_variables = mp_model.num_variables();
  std::vector<MipVariableState> states(num_variables);
  for (int var = 0; var < num_variables; ++var) {
    const MPVariableProto& mp_var = mp_model.variables(var);
    states[var].lb = mp_var.lower_bound();
    states[var].ub = mp_var.upper_bound();
    states[var].is_integer = mp_var.is_integer();
  }
  return states;
}

void ApplyVariableStatesToModel(absl::Span<const MipVariableState> states,
                                MPModelProto* mp_model) {
  const int num_variables = mp_model->variable_size();
  CHECK_EQ(states.size(), num_variables);
  CHECK(mp_model->general_constraint().empty());
  AccurateSum<double> objective_offset;
  for (int var = 0; var < num_variables; ++var) {
    const MipVariableState& state = states[var];
    MPVariableProto* mp_var = mp_model->mutable_variable(var);
    mp_var->set_lower_bound(state.lb);
    mp_var->set_upper_bound(state.ub);
    mp_var->set_is_integer(state.is_integer);
    const double obj = mp_var->objective_coefficient();
    if (obj != 0.0) {
      if (state.shift != 0.0) objective_offset.Add(obj * state.shift);
      mp_var->set_objective_coefficient(obj / state.scaling);
    }
  }
  if (objective_offset.Value() != 0.0) {
    mp_model->set_objective_offset(mp_model->objective_offset() +
                                   objective_offset.Value());
  }
  std::vector<double> coeffs;
  for (MPConstraintProto& ct : *mp_model->mutable_constraint()) {
    double lb;
    double ub;
    TransformConstraint(ct, states, &coeffs, &lb, &ub);
    for (int i = 0; i < coeffs.size(); ++i) {
      ct.set_coefficient(i, coeffs[i]);
    }
    ct.set_lower_bound(lb);
    ct.set_upper_bound(ub);
  }
  if (mp_model->has_solution_hint()) {
    ::operations_research::PartialVariableAssignment* hint =
        mp_model->mutable_solution_hint();
    for (int i = 0; i < hint->var_index_size(); ++i) {
      const int var = hint->var_index(i);
      if (var < 0 || var >= num_variables) continue;
      const MipVariableState& state = states[var];
      hint->set_var_value(i,
                          (hint->var_value(i) - state.shift) * state.scaling);
    }
  }
}

void ScaleComponentContinuousVariables(const SatParameters& params,
                                       const ResidualContinuousComponent& compo,
                                       absl::Span<MipVariableState> states) {
  const double max_bound = params.mip_scale_large_domain()
                               ? std::numeric_limits<double>::infinity()
                               : params.mip_max_bound();
  for (const int var : compo.continuous_vars) {
    if (states[var].scaling != 1.0) continue;
    ScaleVariableState(
        ComputeContinuousVarScaling(params, states[var], max_bound),
        &states[var]);
  }
}

namespace {

// Returns the largest exponent e >= 0 such that magnitude * 2^e <= max_bound.
// `magnitude` must be strictly positive.
int MaxPowerOfTwoExponentWithinBound(double magnitude, double max_bound) {
  DCHECK_GT(magnitude, 0.0);
  if (magnitude > max_bound) return 0;
  int exp = HighestPowerOfTwoAtOrBelow(max_bound / magnitude);
  if (std::ldexp(magnitude, exp) > max_bound) --exp;
  return std::max(exp, 0);
}

// Bounds [err_lb, err_ub] on (discretized_x - x) for a non-fixed continuous
// variable x, given its objective coefficient in the minimization form.
std::pair<double, double> GetContinuousVarDiscretizationErrorBounds(
    bool use_proper_relaxation, double min_objective_coeff) {
  if (!use_proper_relaxation) return {-1.0, 1.0};
  if (min_objective_coeff > 0.0) return {-1.0, 0.0};
  if (min_objective_coeff < 0.0) return {0.0, 1.0};
  return {-0.5, 0.5};
}

// Per-variable limits (indexed by the position of the variable in the
// component) used to pick power-of-two scaling factors for continuous
// variables, see ComputeComponentPrecisionScalingBounds().
struct ComponentPrecisionScalingBounds {
  // Largest power of two keeping the variable domain within the target bound.
  std::vector<double> max_var_scaling;
  // Scaling needed to reach the wanted precision on the activities of all the
  // constraints (and objective) containing the variable.
  std::vector<double> max_scaling_needed;
  // Same as max_scaling_needed, but considering the constraints only. This is
  // only used to report how many variables are scaled more because of the
  // objective.
  std::vector<double> constraint_scaling_needed;
  // Largest power-of-two scaling that still allows all the constraints (and
  // objective) containing the variable to be scaled to integers.
  std::vector<double> min_headroom;
};

ComponentPrecisionScalingBounds ComputeComponentPrecisionScalingBounds(
    const SatParameters& params, const MPModelProtoWrapper& mp_model,
    const ResidualContinuousComponent& compo,
    absl::Span<const MipVariableState> states, double max_target_bound,
    GlobalObjectivePrecision* global_obj_precision) {
  absl::Span<const int> compo_vars = compo.continuous_vars;
  absl::Span<const int> var_to_local_col = compo.var_to_local_col;
  CHECK(!var_to_local_col.empty());
  const double wanted_precision = params.mip_wanted_precision();
  const double tight_precision = params.mip_tight_precision();
  CHECK_GT(tight_precision, 0.0);
  const int64_t scaling_target = int64_t{1}
                                 << params.mip_max_activity_exponent();
  const double max_domain_bound =
      std::min(max_target_bound,
               std::nextafter(static_cast<double>(kint64max / 2), 0.0));

  const int local_num_vars = compo_vars.size();
  ComponentPrecisionScalingBounds result;
  result.max_var_scaling.assign(local_num_vars, 1.0);
  result.max_scaling_needed.assign(local_num_vars, 0.0);
  result.min_headroom.assign(local_num_vars, kInfinity);

  // If continuous variables were already scaled by a power of two > 1 (e.g.
  // TUM components scaled with a common factor), unscale their coefficients and
  // bounds when computing row/objective headroom so their power-of-two
  // denominator does not artificially reduce headroom for other continuous
  // variables sharing that expression.
  const bool use_proper_relaxation = params.mip_proper_relaxation();
  std::vector<double> var_err_lb(local_num_vars, 0.0);
  std::vector<double> var_err_ub(local_num_vars, 0.0);
  std::vector<double> existing_pow2_scaling(local_num_vars, 1.0);
  bool has_scalable_var_in_obj = false;
  for (int local_col = 0; local_col < local_num_vars; ++local_col) {
    const int var = compo_vars[local_col];
    const MipVariableState& state = states[var];
    if (state.is_integer) continue;
    const double lb = state.lb;
    const double ub = state.ub;
    const double obj_coeff = mp_model.variables(var).objective_coefficient();
    if (lb != 0.0 || ub != 0.0) {
      std::tie(var_err_lb[local_col], var_err_ub[local_col]) =
          GetContinuousVarDiscretizationErrorBounds(use_proper_relaxation,
                                                    obj_coeff);
    }
    if (state.scaling != 1.0) {
      const double s = state.scaling;
      int exp = 0;
      if (s > 1.0 && std::isfinite(s) && std::frexp(s, &exp) == 0.5) {
        existing_pow2_scaling[local_col] = s;
      }
      continue;
    }

    if (!std::isfinite(lb) || !std::isfinite(ub)) continue;
    const double bound_magnitude = std::max(std::abs(lb), std::abs(ub));
    if (bound_magnitude == 0.0) continue;

    const int max_exp =
        MaxPowerOfTwoExponentWithinBound(bound_magnitude, max_domain_bound);
    if (max_exp > 0) {
      result.max_var_scaling[local_col] = std::ldexp(1.0, max_exp);
      if (obj_coeff != 0.0) has_scalable_var_in_obj = true;
    }
  }

  // We only want "discretization_precision" on the constraint/objective
  // activity, if a variable only appears with really small coefficients, we
  // don't need to scale it too much. Note that the objective is processed with
  // a potentially larger precision than the constraints (see
  // GlobalObjectivePrecision).
  std::vector<int> local_cols;
  std::vector<double> coefficients;
  std::vector<double> lower_bounds;
  std::vector<double> upper_bounds;
  std::vector<double> transformed_coeffs;
  auto term_discretization_errors = [&](int local_col, double coeff) {
    if (local_col == -1) return std::make_pair(0.0, 0.0);
    if (coeff > 0.0) {
      return std::make_pair(std::max(0.0, -coeff * var_err_lb[local_col]),
                            std::max(0.0, coeff * var_err_ub[local_col]));
    }
    return std::make_pair(std::max(0.0, -coeff * var_err_ub[local_col]),
                          std::max(0.0, coeff * var_err_lb[local_col]));
  };

  for (const int c : compo.constraints) {
    const MPConstraintProto& mp_constraint = mp_model.constraints(c).proto();
    if (ConstraintIsAlwaysTrue(mp_constraint)) continue;
    double ct_lower_bound;
    double ct_upper_bound;
    TransformConstraint(mp_constraint, states, &transformed_coeffs,
                        &ct_lower_bound, &ct_upper_bound);

    if (params.mip_use_tight_bounds()) {
      ct_lower_bound -= tight_precision;
      ct_upper_bound += tight_precision;
    } else {
      ct_lower_bound -= wanted_precision;
      ct_upper_bound += wanted_precision;
    }

    double min_activity = 0.0;
    double max_activity = 0.0;
    double max_existing_pow2_scaling = 1.0;
    bool has_scalable_var = false;
    local_cols.clear();
    coefficients.clear();
    lower_bounds.clear();
    upper_bounds.clear();
    const int num_coeffs = mp_constraint.var_index_size();
    for (int i = 0; i < num_coeffs; ++i) {
      const int var = mp_constraint.var_index(i);
      const double lb = states[var].lb;
      const double ub = states[var].ub;
      if (lb == 0.0 && ub == 0.0) continue;

      const double coeff = transformed_coeffs[i];
      if (coeff == 0.0) continue;

      const int local_col = var_to_local_col[var];
      double s = 1.0;
      if (local_col != -1) {
        if (result.max_var_scaling[local_col] > 1.0) has_scalable_var = true;
        s = existing_pow2_scaling[local_col];
      }
      max_existing_pow2_scaling = std::max(max_existing_pow2_scaling, s);
      local_cols.push_back(local_col);
      coefficients.push_back(coeff * s);
      lower_bounds.push_back(lb / s);
      upper_bounds.push_back(ub / s);

      double min_term = coeff * lb;
      double max_term = coeff * ub;
      if (min_term > max_term) std::swap(min_term, max_term);

      min_activity += min_term;
      max_activity += max_term;
    }

    if (!has_scalable_var) continue;
    const bool has_lb = min_activity < ct_lower_bound;
    const bool has_ub = max_activity > ct_upper_bound;
    if (!has_lb && !has_ub) continue;

    double relative_coeff_error;
    double scaled_sum_error;
    double max_scaling_factor;
    const double scaling_factor = FindBestScalingAndComputeErrors(
        coefficients, lower_bounds, upper_bounds, scaling_target,
        wanted_precision, &relative_coeff_error, &scaled_sum_error,
        &max_scaling_factor);
    if (scaling_factor == 0.0) {
      for (const int local_col : local_cols) {
        if (local_col != -1) result.min_headroom[local_col] = 1.0;
      }
      continue;
    }

    const double headroom =
        max_existing_pow2_scaling * scaling_factor > max_scaling_factor
            ? 1.0
            : std::ldexp(1.0, HighestPowerOfTwoAtOrBelow(max_scaling_factor /
                                                         scaling_factor));
    int num_lb_floats = 0;
    int num_ub_floats = 0;
    for (int i = 0; i < local_cols.size(); ++i) {
      const auto [lb_err, ub_err] =
          term_discretization_errors(local_cols[i], coefficients[i]);
      if (has_lb && lb_err > 0.0) ++num_lb_floats;
      if (has_ub && ub_err > 0.0) ++num_ub_floats;
    }

    const double discretization_precision = wanted_precision / 2.0;
    for (int i = 0; i < local_cols.size(); ++i) {
      const int local_col = local_cols[i];
      if (local_col == -1) continue;
      result.min_headroom[local_col] =
          std::min(result.min_headroom[local_col], headroom);
      if (result.max_var_scaling[local_col] > 1.0) {
        const auto [lb_err, ub_err] =
            term_discretization_errors(local_col, coefficients[i]);
        const double max_weighted_err = std::max(
            has_lb ? static_cast<double>(num_lb_floats) * lb_err : 0.0,
            has_ub ? static_cast<double>(num_ub_floats) * ub_err : 0.0);
        result.max_scaling_needed[local_col] =
            std::max(result.max_scaling_needed[local_col],
                     max_weighted_err / discretization_precision);
      }
    }
  }

  // Also take the objective into account.
  //
  // Note that a discretization error on the objective is not a feasibility
  // issue but an optimality one, so we use a precision in line with the gap
  // limit. We keep track of the scaling needed by the constraints alone in
  // order to report how many variables are scaled more because of the
  // objective.
  result.constraint_scaling_needed = result.max_scaling_needed;
  if (has_scalable_var_in_obj && global_obj_precision != nullptr) {
    const GlobalObjectivePrecision::Bounds obj_bounds =
        global_obj_precision->GetBounds(mp_model.underlying_model(), states);
    const double discretization_precision = obj_bounds.precision / 2.0;
    for (int local_col = 0; local_col < local_num_vars; ++local_col) {
      const int var = compo_vars[local_col];
      const MipVariableState& state = states[var];
      if (state.is_integer) continue;
      if (state.lb == 0.0 && state.ub == 0.0) continue;
      const double coeff =
          mp_model.variables(var).objective_coefficient() / state.scaling;
      if (coeff == 0.0) continue;

      result.min_headroom[local_col] =
          std::min(result.min_headroom[local_col], obj_bounds.headroom);
      if (result.max_var_scaling[local_col] > 1.0) {
        const double s = existing_pow2_scaling[local_col];
        const auto [lb_err, unused_ub_err] =
            term_discretization_errors(local_col, coeff * s);
        result.max_scaling_needed[local_col] =
            std::max(result.max_scaling_needed[local_col],
                     static_cast<double>(obj_bounds.num_lb_floats) * lb_err /
                         discretization_precision);
      }
    }
  }

  return result;
}

}  // namespace

GlobalObjectivePrecision::GlobalObjectivePrecision(const SatParameters& params,
                                                   const MPModelProto& mp_model)
    : scaling_target_(int64_t{1} << params.mip_max_activity_exponent()),
      obj_precision_(std::max(params.mip_wanted_precision(),
                              params.absolute_gap_limit())) {
  CHECK(!mp_model.maximize());
  const int num_variables = mp_model.variable_size();
  for (int var = 0; var < num_variables; ++var) {
    if (mp_model.variable(var).objective_coefficient() != 0.0) {
      obj_vars_.push_back(var);
    }
  }
}

GlobalObjectivePrecision::Bounds GlobalObjectivePrecision::GetBounds(
    const MPModelProto& mp_model, absl::Span<const MipVariableState> states) {
  if (dirty_) {
    Recompute(mp_model, states);
  }
  if (!has_continuous_obj_var_) {
    return {kInfinity, 0, obj_precision_};
  }
  double headroom = 1.0;
  if (scaling_factor_ != 0.0 &&
      max_existing_pow2_scaling_ * scaling_factor_ <= max_scaling_factor_) {
    headroom = std::ldexp(
        1.0, HighestPowerOfTwoAtOrBelow(max_scaling_factor_ / scaling_factor_));
  }
  return {headroom, num_lb_floats_, obj_precision_};
}

void GlobalObjectivePrecision::Recompute(
    const MPModelProto& mp_model, absl::Span<const MipVariableState> states) {
  coeffs_.clear();
  lbs_.clear();
  ubs_.clear();
  num_lb_floats_ = 0;
  max_existing_pow2_scaling_ = 1.0;
  has_continuous_obj_var_ = false;
  for (const int var : obj_vars_) {
    const MipVariableState& state = states[var];
    const double lb = state.lb;
    const double ub = state.ub;
    if (lb == 0.0 && ub == 0.0) continue;
    const double coeff =
        mp_model.variable(var).objective_coefficient() / state.scaling;
    if (coeff == 0.0) continue;

    // Continuous variables already scaled by a power of two are considered in
    // their unscaled form, see ComputeComponentPrecisionScalingBounds().
    double s = 1.0;
    if (!state.is_integer) {
      has_continuous_obj_var_ = true;
      ++num_lb_floats_;
      const double cur = state.scaling;
      int exp = 0;
      if (cur > 1.0 && std::isfinite(cur) && std::frexp(cur, &exp) == 0.5) {
        s = cur;
        max_existing_pow2_scaling_ = std::max(max_existing_pow2_scaling_, s);
      }
    }
    coeffs_.push_back(coeff * s);
    lbs_.push_back(lb / s);
    ubs_.push_back(ub / s);
  }
  scaling_factor_ = 1.0;
  max_scaling_factor_ = kInfinity;
  if (!coeffs_.empty()) {
    double relative_coeff_error;
    double scaled_sum_error;
    scaling_factor_ = FindBestScalingAndComputeErrors(
        coeffs_, lbs_, ubs_, scaling_target_, obj_precision_,
        &relative_coeff_error, &scaled_sum_error, &max_scaling_factor_);
  }
  dirty_ = false;
}

bool GlobalObjectivePrecision::KeepsObjectiveScalable(
    const MPModelProto& mp_model, absl::Span<const MipVariableState> states,
    int var, double scaling, double reserved_headroom) {
  const double obj_coeff = mp_model.variable(var).objective_coefficient();
  if (obj_coeff == 0.0) return true;
  if (dirty_) Recompute(mp_model, states);
  if (scaling_factor_ == 0.0) return false;

  // The current factor makes the other coefficients integer, find the extra
  // factor needed by the new coefficient of var, see GetIntegralityMultiplier()
  // for the tolerance.
  const double coeff = obj_coeff / scaling;
  const double limit = max_scaling_factor_ /
                       (scaling_factor_ * std::max(1.0, reserved_headroom));
  if (limit < 1.0) return false;
  const int64_t int_limit =
      limit >= static_cast<double>(kint64max)
          ? kint64max
          : CapAdd(static_cast<int64_t>(std::floor(limit)), 1);
  const int64_t extra_factor = FindRationalFactor(
      scaling_factor_ * coeff, int_limit,
      scaling_factor_ * obj_precision_ * std::min(1.0, std::abs(coeff)));
  return extra_factor != 0 && static_cast<double>(extra_factor) <= limit;
}

void LogPrecisionScalingStats(const SatParameters& params,
                              const MPModelProto& mp_model,
                              absl::Span<const MipVariableState> states,
                              const PrecisionScalingStats& stats,
                              SolverLogger* logger) {
  if (stats.num_scaled == 0) return;

  // Worst case objective error due to discretizing each continuous variable.
  const double obj_error_factor = params.mip_proper_relaxation() ? 1.0 : 0.5;
  double objective_discretization_error = 0.0;
  const int num_variables = mp_model.variable_size();
  for (int var = 0; var < num_variables; ++var) {
    if (states[var].is_integer) continue;
    const double coeff = mp_model.variable(var).objective_coefficient();
    if (coeff == 0.0) continue;
    objective_discretization_error +=
        obj_error_factor * std::abs(coeff) / states[var].scaling;
  }

  SOLVER_LOG(logger, "[ScaleForPrecision] Scaled ", stats.num_scaled,
             " continuous variables, power of two scaling in [",
             stats.min_exponent, ",", stats.max_exponent, "], ",
             stats.num_objective_driven,
             " scaled more than needed by the constraints because of the "
             "objective. Worst case objective discretization error: ",
             objective_discretization_error);
}

void ScaleComponentContinuousVariablesForGivenPrecision(
    const SatParameters& params, const ResidualContinuousComponent& compo,
    const MPModelProtoWrapper& mp_model, absl::Span<MipVariableState> states,
    GlobalObjectivePrecision* global_obj_precision,
    PrecisionScalingStats* stats) {
  const ComponentPrecisionScalingBounds bounds =
      ComputeComponentPrecisionScalingBounds(params, mp_model, compo, states,
                                             kInfinity, global_obj_precision);

  absl::Span<const int> compo_vars = compo.continuous_vars;
  const int local_num_vars = compo_vars.size();
  for (int local_col = 0; local_col < local_num_vars; ++local_col) {
    if (bounds.max_var_scaling[local_col] <= 1.0 ||
        bounds.min_headroom[local_col] == kInfinity ||
        bounds.max_scaling_needed[local_col] <= 1.0) {
      continue;
    }
    const double needed_scaling = std::ldexp(
        1.0, LowestPowerOfTwoAtOrAbove(bounds.max_scaling_needed[local_col]));
    const double scaling =
        std::min({bounds.max_var_scaling[local_col],
                  bounds.min_headroom[local_col], needed_scaling});
    if (scaling <= 1.0) continue;

    int exp;
    const double mantissa = std::frexp(scaling, &exp);
    DCHECK_EQ(mantissa, 0.5);
    --exp;

    ++stats->num_scaled;
    stats->min_exponent = std::min(stats->min_exponent, exp);
    stats->max_exponent = std::max(stats->max_exponent, exp);

    // Would the constraints alone have resulted in a smaller scaling?
    const double needed_for_constraints =
        std::ldexp(1.0, LowestPowerOfTwoAtOrAbove(std::max(
                            1.0, bounds.constraint_scaling_needed[local_col])));
    const double constraint_scaling =
        std::min({bounds.max_var_scaling[local_col],
                  bounds.min_headroom[local_col], needed_for_constraints});
    if (constraint_scaling < scaling) ++stats->num_objective_driven;

    const int var = compo_vars[local_col];
    MipVariableState& state = states[var];
    CHECK_EQ(state.scaling, 1.0);
    if (global_obj_precision != nullptr &&
        mp_model.variables(var).objective_coefficient() != 0.0 &&
        !(state.lb == 0.0 && state.ub == 0.0)) {
      global_obj_precision->NotifyContinuousPow2Scaling(scaling);
    }
    ScaleVariableState(scaling, &state);
  }
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

// A linear row `lb <= sum coeffs[i] * vars[i] <= ub` expressed in the current
// variable space, see TransformConstraint().
struct LinearRow {
  absl::Span<const int> vars;
  absl::Span<const double> coeffs;
  double lb;
  double ub;
};

double GetVarCoeff(const LinearRow& row, int var) {
  for (int i = 0; i < row.vars.size(); ++i) {
    if (row.vars[i] == var) return row.coeffs[i];
  }
  return 0.0;
}

// For each side (>= and <=) return a factor such that factor * var only needs
// to take integer values to satisfy this side.
//
// We return kInfinity if we failed, and 0.0 if this side is not constrained.
//
// Precondition: var must be the only non-integer in the given row.
std::pair<double, double> GetIntegralityMultiplier(const LinearRow& row,
                                                   int var, double tolerance,
                                                   double max_multiplier) {
  const double kInfinity = std::numeric_limits<double>::infinity();

  const double var_coeff = GetVarCoeff(row, var);
  if (var_coeff == 0.0) return {0.0, 0.0};
  const double limit = max_multiplier / std::abs(var_coeff);
  if (limit < 1.0) return {kInfinity, kInfinity};
  const int64_t int_limit =
      limit >= static_cast<double>(kint64max)
          ? kint64max
          : CapAdd(static_cast<int64_t>(std::floor(limit)), 1);

  double multiplier = 1.0;
  for (int i = 0; i < row.vars.size(); ++i) {
    if (var == row.vars[i]) continue;

    // This actually computes the smallest multiplier to make all other
    // terms in the constraint integer.
    //
    // Note that we want the rational approximation of the coefficient to be
    // within `tolerance` in absolute value but also relatively. Otherwise, on
    // constraints with tiny coefficients (like 1.8e-5), we might accept a 3%
    // relative error (1.75e-5) which is clearly not a "nice" coefficient and
    // leads to huge scaling factors later on.
    const double coeff = multiplier * row.coeffs[i];
    const double local_tolerance =
        multiplier * tolerance * std::min(1.0, std::abs(row.coeffs[i]));
    multiplier *= FindRationalFactor(coeff, int_limit, local_tolerance);
    if (multiplier == 0 || multiplier > limit) {
      multiplier = kInfinity;
      break;
    }
  }
  DCHECK_NE(var_coeff, 0.0);

  // Only the magnitude matter.
  double ct_lb = std::abs(row.lb);
  double ct_ub = std::abs(row.ub);
  if (var_coeff < 0.0) std::swap(ct_lb, ct_ub);

  // Now adjust for the bound. Same remark as above regarding the relative
  // tolerance, but a zero bound is always fine.
  double lb_multiplier = multiplier;
  if (ct_lb == kInfinity) {
    lb_multiplier = 0.0;
  } else if (multiplier != kInfinity) {
    const double lb_tolerance =
        multiplier * tolerance * std::clamp(ct_lb, tolerance, 1.0);
    lb_multiplier = multiplier * FindRationalFactor(multiplier * ct_lb,
                                                    int_limit, lb_tolerance);
    if (lb_multiplier == 0.0 || lb_multiplier > limit) {
      lb_multiplier = kInfinity;
    }
  }

  double ub_multiplier = multiplier;
  if (ct_ub == kInfinity) {
    ub_multiplier = 0.0;
  } else if (multiplier != kInfinity) {
    const double ub_tolerance =
        multiplier * tolerance * std::clamp(ct_ub, tolerance, 1.0);
    ub_multiplier = multiplier * FindRationalFactor(multiplier * ct_ub,
                                                    int_limit, ub_tolerance);
    if (ub_multiplier == 0.0 || ub_multiplier > limit) {
      ub_multiplier = kInfinity;
    }
  }

  return {std::abs(lb_multiplier * var_coeff),
          std::abs(ub_multiplier * var_coeff)};
}

}  // namespace

namespace {

// Snaps the bounds of an integer variable to integer values, allowing an extra
// integer point if it is within tolerance (or within the integerization error
// bound). Returns false if the resulting domain is empty.
bool MakeIntegerVariableBoundsInteger(const SatParameters& params,
                                      double scaling, double error_lb,
                                      double error_ub, double* lb, double* ub,
                                      int64_t* num_changes) {
  // Note that we do not scale the tolerance! we want to interpret variable
  // bounds as exact, but we just want to allow an extra integer point if
  // it is within tolerance (or within the integerization error bound).
  const double tolerance =
      std::min(0.5, scaling * params.mip_wanted_precision());
  const double lb_tol = std::max(tolerance, -error_lb);
  const double ub_tol = std::max(tolerance, error_ub);

  const double new_lb =
      std::isfinite(*lb) ? std::ceil(ExactScaledSumLb(*lb, -lb_tol)) : *lb;
  if (*lb != new_lb) {
    ++*num_changes;
    *lb = new_lb;
  }
  const double new_ub =
      std::isfinite(*ub) ? std::floor(ExactScaledSumUb(*ub, ub_tol)) : *ub;
  if (*ub != new_ub) {
    ++*num_changes;
    *ub = new_ub;
  }
  return new_lb <= new_ub;
}

}  // namespace

bool MakeBoundsOfIntegerVariablesInteger(const SatParameters& params,
                                         MPModelProto* mp_model,
                                         SolverLogger* logger) {
  const int num_variables = mp_model->variable_size();
  const std::vector<double> unit_scaling(num_variables, 1.0);
  const std::vector<double> zero_errors(num_variables, 0.0);
  return MakeBoundsOfIntegerVariablesInteger(params, unit_scaling, zero_errors,
                                             zero_errors, mp_model, logger);
}

bool MakeBoundsOfIntegerVariablesInteger(
    const SatParameters& params, absl::Span<const double> var_scaling,
    absl::Span<const double> integer_error_lb,
    absl::Span<const double> integer_error_ub, MPModelProto* mp_model,
    SolverLogger* logger) {
  const int num_variables = mp_model->variable_size();
  CHECK_EQ(var_scaling.size(), num_variables);
  CHECK_EQ(integer_error_lb.size(), num_variables);
  CHECK_EQ(integer_error_ub.size(), num_variables);
  int64_t num_changes = 0;
  for (int i = 0; i < num_variables; ++i) {
    MPVariableProto* mp_var = mp_model->mutable_variable(i);
    if (!mp_var->is_integer()) continue;
    double lb = mp_var->lower_bound();
    double ub = mp_var->upper_bound();
    if (!MakeIntegerVariableBoundsInteger(
            params, var_scaling[i], integer_error_lb[i], integer_error_ub[i],
            &lb, &ub, &num_changes)) {
      SOLVER_LOG(logger, "Empty domain for integer variable #", i, ": [",
                 mp_var->lower_bound(), ",", mp_var->upper_bound(), "]");
      return false;
    }
    mp_var->set_lower_bound(lb);
    mp_var->set_upper_bound(ub);
  }
  if (num_changes > 0) {
    SOLVER_LOG(logger, "[IntegerBound] Snapped ", num_changes,
               " bound of an integer variable to an exact integer.");
  }
  return true;
}

bool MakeBoundsOfIntegerVariablesInteger(const SatParameters& params,
                                         absl::Span<const int> vars,
                                         absl::Span<MipVariableState> states,
                                         int64_t* num_changes,
                                         SolverLogger* logger) {
  for (const int var : vars) {
    MipVariableState& state = states[var];
    if (!state.is_integer) continue;
    const double lb = state.lb;
    const double ub = state.ub;
    if (!MakeIntegerVariableBoundsInteger(params, state.scaling, state.error_lb,
                                          state.error_ub, &state.lb, &state.ub,
                                          num_changes)) {
      SOLVER_LOG(logger, "Empty domain for integer variable #", var, ": [", lb,
                 ",", ub, "]");
      return false;
    }
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
        min_left_activity[i + 1] = tmp_min_sum.TightLowerBound();
      } else {
        min_left_activity[i + 1] = min_left_activity[i] + coeff * lb;
      }
    }
    if constexpr (fill_max) {
      if (max_left_activity[i] == kInfinity || ub == kInfinity) {
        max_left_activity[i + 1] = kInfinity;
      } else if constexpr (exact) {
        if (coeff != 0.0) tmp_max_sum.AddProd(coeff, ub);
        max_left_activity[i + 1] = tmp_max_sum.TightUpperBound();
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
        min_right_activity[i - 1] = tmp_min_sum.TightLowerBound();
      } else {
        min_right_activity[i - 1] = min_right_activity[i] + coeff * lb;
      }
    }
    if constexpr (fill_max) {
      if (max_right_activity[i] == kInfinity || ub == kInfinity) {
        max_right_activity[i - 1] = kInfinity;
      } else if constexpr (exact) {
        if (coeff != 0.0) tmp_max_sum.AddProd(coeff, ub);
        max_right_activity[i - 1] = tmp_max_sum.TightUpperBound();
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

bool RestrictBoundsWithDualReasoning(const SatParameters& params,
                                     const MPModelProtoWrapper& mp_model,
                                     absl::Span<double> var_lbs,
                                     absl::Span<double> var_ubs,
                                     SolverLogger* logger) {
  // Disable depending on the parameters.
  if (!params.mip_presolve_restrict_bounds()) return false;
  if (params.keep_all_feasible_solutions_in_presolve()) return false;

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
  return max_change > params.mip_tight_precision();
}

namespace {

void DisplayVariableStatisticsInternal(double min_lb, double max_ub,
                                       int num_integers, int num_variables,
                                       SolverLogger* logger) {
  SOLVER_LOG(logger, "All variables in [", min_lb, ",", max_ub,
             "], num_integers: ", num_integers, "/", num_variables,
             (num_integers == num_variables ? " [IP] " : " [MIP] "));
}

}  // namespace

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
  DisplayVariableStatisticsInternal(min_lb, max_ub, num_integers,
                                    mp_model.variable().size(), logger);
}

void DisplayVariableStatistics(absl::Span<const MipVariableState> states,
                               SolverLogger* logger) {
  const double kInfinity = std::numeric_limits<double>::infinity();
  double min_lb = +kInfinity;
  double max_ub = -kInfinity;
  int num_integers = 0;
  for (const MipVariableState& state : states) {
    if (state.is_integer) ++num_integers;
    min_lb = std::min(min_lb, state.lb);
    max_ub = std::max(max_ub, state.ub);
  }
  DisplayVariableStatisticsInternal(min_lb, max_ub, num_integers, states.size(),
                                    logger);
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

// Bounds [err_lb, err_ub] (with err_lb <= 0 <= err_ub) on
// (discretized_var - scaling * var) when `var` is the unique non-integer in
// `row` and its scaled value is rounded from the row activity in
// `[rhs_lb, rhs_ub]`. The bounds and integerization errors of the other
// (integer) variables are read from `states`.
std::pair<double, double> ComputeSingleVarConstraintErrorBounds(
    const LinearRow& row, absl::Span<const MipVariableState> states, int var,
    double scaling, double rhs_lb, double rhs_ub) {
  const double var_coeff = GetVarCoeff(row, var);
  if (var_coeff == 0.0) return {0.0, 0.0};
  if (!std::isfinite(rhs_lb) || !std::isfinite(rhs_ub)) return {-0.5, 0.5};

  // Accumulate bounds on `var_coeff * (discretized_var - scaling * var)`, then
  // divide by `var_coeff` once at the end.
  const double r_b =
      std::round((scaling / var_coeff) * (0.5 * rhs_lb + 0.5 * rhs_ub));
  const auto [b1_lb, b1_ub] = TightSum()
                                  .AddProd(r_b, var_coeff)
                                  .AddProd(-scaling, rhs_lb)
                                  .TightBounds();
  const auto [b2_lb, b2_ub] = TightSum()
                                  .AddProd(r_b, var_coeff)
                                  .AddProd(-scaling, rhs_ub)
                                  .TightBounds();

  TightSum total_lb;
  TightSum total_ub;
  total_lb.Add(std::min(b1_lb, b2_lb));
  total_ub.Add(std::max(b1_ub, b2_ub));

  for (int i = 0; i < row.vars.size(); ++i) {
    const int j = row.vars[i];
    if (j == var) continue;
    const double a_j = row.coeffs[i];
    if (a_j == 0.0) continue;

    const double k_j = std::round((scaling * a_j) / var_coeff);
    if (k_j != 0.0) {
      const bool same_sign = (k_j < 0.0) == (var_coeff > 0.0);
      const double e_lb = same_sign ? states[j].error_lb : states[j].error_ub;
      const double e_ub = same_sign ? states[j].error_ub : states[j].error_lb;
      if (e_lb != 0.0) total_lb.AddProd(-k_j, var_coeff, e_lb);
      if (e_ub != 0.0) total_ub.AddProd(-k_j, var_coeff, e_ub);
    }

    const auto [num_lb, num_ub] =
        TightSum().AddProd(scaling, a_j).AddProd(-k_j, var_coeff).TightBounds();
    if (num_lb == 0.0 && num_ub == 0.0) continue;

    const double l_j = states[j].lb;
    const double u_j = states[j].ub;
    if (!std::isfinite(l_j) || !std::isfinite(u_j)) return {-0.5, 0.5};
    if (num_ub > 0.0) {
      total_lb.Add(ProdBounds(l_j < 0.0 ? num_ub : num_lb, l_j).first);
      total_ub.Add(ProdBounds(u_j > 0.0 ? num_ub : num_lb, u_j).second);
    } else {
      total_lb.Add(ProdBounds(u_j > 0.0 ? num_lb : num_ub, u_j).first);
      total_ub.Add(ProdBounds(l_j < 0.0 ? num_lb : num_ub, l_j).second);
    }
  }

  const auto [err_lb, err_ub] = DivInterval(
      total_lb.TightLowerBound(), total_ub.TightUpperBound(), var_coeff);
  return {std::min(0.0, err_lb), std::max(0.0, err_ub)};
}

// Maximum rational scaling factor allowed when marking a continuous variable
// with the given bounds as integer. The smaller the domain, the more we allow
// scaling.
double MaxImpliedIntegerMultiplier(const SatParameters& params, double lb,
                                   double ub) {
  double max_magnitude = 1.0;
  if (std::isfinite(lb)) max_magnitude = std::max(max_magnitude, std::abs(lb));
  if (std::isfinite(ub)) max_magnitude = std::max(max_magnitude, std::abs(ub));
  const double max_multiplier = params.mip_max_bound() / max_magnitude;
  if (params.mip_scale_for_precision()) {
    return max_multiplier;
  }
  // TODO(user): This is the old behavior and changing it broke some golden
  // test. Fix.
  return std::min(1e4, max_multiplier);
}

}  // namespace

std::vector<double> DetectImpliedIntegers(const SatParameters& params,
                                          MPModelProtoWrapper* mp_model,
                                          absl::Span<double> integer_error_lb,
                                          absl::Span<double> integer_error_ub,
                                          SolverLogger* logger) {
  const int num_variables = mp_model->num_variables();
  CHECK_EQ(integer_error_lb.size(), num_variables);
  CHECK_EQ(integer_error_ub.size(), num_variables);
  std::vector<double> var_scaling(num_variables, 1.0);

  // The states mirror the model with the pending scaling applied, so that the
  // helper functions see the constraints in the scaled variable space.
  std::vector<MipVariableState> states = InitialVariableStates(*mp_model);
  for (int var = 0; var < num_variables; ++var) {
    states[var].error_lb = integer_error_lb[var];
    states[var].error_ub = integer_error_ub[var];
  }
  std::vector<double> row_coeffs;
  const auto make_row = [&](const EnforcedLinearConstraint& ct) {
    row_coeffs.resize(ct.num_terms());
    for (int i = 0; i < ct.num_terms(); ++i) {
      row_coeffs[i] = ct.coeffs(i) / states[ct.vars(i)].scaling;
    }
    return LinearRow{.vars = ct.vars(),
                     .coeffs = row_coeffs,
                     .lb = ct.lower_bound(),
                     .ub = ct.upper_bound()};
  };

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
    if (ConstraintIsAlwaysTrue(mp_constraint.proto())) continue;

    for (int j = 0; j < mp_constraint.num_terms(); ++j) {
      if (mp_constraint.coeffs(j) == 0.0) continue;
      const int var = mp_constraint.vars(j);
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
    states[var].is_integer = true;
    ScaleVariableState(scaling, &states[var]);

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
      if (ct.coeffs(i) != 0.0 &&
          !mp_model->variables(ct.vars(i)).is_integer()) {
        CHECK_EQ(var, -1);
        var = ct.vars(i);
      }
    }
    CHECK_NE(var, -1);

    // The smaller the domain, the more we allow scaling.
    const double max_multiplier =
        MaxImpliedIntegerMultiplier(params, states[var].lb, states[var].ub);

    // Inspect this constraint to see if we can find a small enough multiplier.
    // Note that if this is the case, we will inspect all constraints below to
    // try to find the best. Otherwise, we will wait for the other constraint to
    // be processed. This avoid a bad quadratic complexity if we where to
    // execute the loop below in all cases.
    ++num_processed_constraints;
    int best_ct_index = top_ct_index;
    double best_scaling =
        GetIntegralityMultiplier(make_row(ct), var, tolerance, max_multiplier)
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
          GetIntegralityMultiplier(make_row(other_ct), var, tolerance,
                                   max_multiplier)
              .first;
      if (multiplier != 0.0 && multiplier < best_scaling) {
        best_scaling = multiplier;
        best_ct_index = ct_index;
      }
    }

    const LinearRow best_row = make_row(mp_model->constraints(best_ct_index));
    std::tie(integer_error_lb[var], integer_error_ub[var]) =
        ComputeSingleVarConstraintErrorBounds(
            best_row, states, var, best_scaling, best_row.lb, best_row.ub);
    states[var].error_lb = integer_error_lb[var];
    states[var].error_ub = integer_error_ub[var];
    scale_and_mark_as_integer(var, best_scaling);
  }

  const int num_missed = num_fail_due_to_rhs + num_fail_due_to_large_multiplier;
  if (num_missed > 0) {
    VLOG(1) << "num_new_integer: " << num_implied_integers
            << "\n num_processed_constraints: " << num_processed_constraints
            << "\n num_rhs_fail: " << num_fail_due_to_rhs
            << "\n num_multiplier_fail: " << num_fail_due_to_large_multiplier;
    SOLVER_LOG(logger, "Missed ", num_missed, " potential implied integer.");
  }

  if (num_implied_integers > 0 || num_missed > 0) {
    const int num_integers = initial_num_integers + num_implied_integers;
    SOLVER_LOG(logger, "[ImpliedIntegers] Num integers: ", num_integers, "/",
               num_variables, " (implied:", num_implied_integers,
               " missed:", num_missed, " max_scaling:", max_scaling, ")");
  }

  ApplyVarScaling(var_scaling, mp_model->mutable_underlying_model());
  return var_scaling;
}

void ScaleOneVariableResidualMatrix(
    const SatParameters& params, const ResidualContinuousComponent& compo,
    const MPModelProtoWrapper& mp_model, absl::Span<MipVariableState> states,
    GlobalObjectivePrecision* global_obj_precision,
    OneVariableComponentStats* stats) {
  CHECK_EQ(compo.continuous_vars.size(), 1);
  const int var = compo.continuous_vars[0];
  MipVariableState& state = states[var];
  CHECK(!state.is_integer);
  CHECK_EQ(state.scaling, 1.0);
  ++stats->num_components;
  if (params.keep_all_feasible_solutions_in_presolve()) return;

  const double tolerance = params.mip_wanted_precision();
  const double max_multiplier =
      MaxImpliedIntegerMultiplier(params, state.lb, state.ub);

  // The objective coefficient of var in the (minimization) problem.
  const double obj_coeff = mp_model.variables(var).objective_coefficient();

  // Transform the rows in the current variable space.
  const int num_rows = compo.constraints.size();
  std::vector<std::vector<double>> row_coeffs(num_rows);
  std::vector<LinearRow> rows(num_rows);
  for (int r = 0; r < num_rows; ++r) {
    const EnforcedLinearConstraint& ct =
        mp_model.constraints(compo.constraints[r]);
    TransformConstraint(ct.proto(), states, &row_coeffs[r], &rows[r].lb,
                        &rows[r].ub);
    rows[r].vars = ct.vars();
    rows[r].coeffs = row_coeffs[r];
  }

  // For each row, stores a multiplier m such that the row is feasible if and
  // only if it is feasible with m * var an integer. We actually separate the
  // >= and the <= side for a tighter algorithm.
  //
  // Then we compute a kind of least common multiple so that scaling * var will
  // always be lower or upper bounded by integers. If the variable has an
  // objective, we need the "good objective" bound to be integer. If not, just
  // one of the two bounds work since we will always be able to "snap" the
  // variable value to that one.
  std::vector<double> var_multipliers_ge;
  std::vector<double> var_multipliers_le;
  for (const LinearRow& row : rows) {
    const auto [lb_mult, ub_mult] =
        GetIntegralityMultiplier(row, var, tolerance, max_multiplier);
    if (lb_mult != 0.0) var_multipliers_ge.push_back(lb_mult);
    if (ub_mult != 0.0) var_multipliers_le.push_back(ub_mult);
  }
  double scaling_ge = kInfinity;
  double scaling_le = kInfinity;
  if (obj_coeff <= 0.0) {
    scaling_le = GetDoubleLcm(var_multipliers_le, tolerance, max_multiplier);
  }
  if (obj_coeff >= 0.0) {
    scaling_ge = GetDoubleLcm(var_multipliers_ge, tolerance, max_multiplier);
  }
  if (scaling_ge == kInfinity && scaling_le == kInfinity) {
    ++stats->num_missed;
    return;
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
    const double scaled_var_lb = (state.lb - tolerance) * s;
    const double scaled_var_ub = (state.ub + tolerance) * s;
    return std::ceil(scaled_var_lb) > std::floor(scaled_var_ub);
  };
  const bool fix_to_lb = domain_has_no_integer(scaling_ge);
  const bool fix_to_ub = domain_has_no_integer(scaling_le);
  if (fix_to_lb || fix_to_ub) {
    double target = fix_to_lb ? state.lb : state.ub;
    if (fix_to_lb && fix_to_ub && std::abs(state.ub) < std::abs(target)) {
      target = state.ub;
    }
    ++stats->num_fixed;
    state.lb = target;
    state.ub = target;
    return;
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
    if (std::abs(scaled - std::round(scaled)) > tolerance * s) {
      return kInfinity;
    }
    return s;
  };
  const bool need_lb = obj_coeff > 0.0 || !var_multipliers_le.empty();
  const bool need_ub = obj_coeff < 0.0 || !var_multipliers_ge.empty();
  if (need_lb) scaling_ge = scale_for_bound(scaling_ge, state.lb);
  if (need_ub) scaling_le = scale_for_bound(scaling_le, state.ub);
  const double scaling = std::min(scaling_ge, scaling_le);
  if (scaling == kInfinity) {
    ++stats->num_missed;
    return;
  }

  // Bound the error |discretized_var - scaling * var| when snapping var to the
  // active side of its rows (or to its own bound).
  const bool snap_ge = (scaling == scaling_ge);
  double max_err = 0.0;
  for (const LinearRow& row : rows) {
    const double var_coeff = GetVarCoeff(row, var);
    if (var_coeff == 0.0) continue;
    const double active_rhs = (snap_ge == (var_coeff > 0.0)) ? row.lb : row.ub;
    if (!std::isfinite(active_rhs)) continue;
    const auto [e_lb, e_ub] = ComputeSingleVarConstraintErrorBounds(
        row, states, var, scaling, active_rhs, active_rhs);
    max_err = std::max({max_err, -e_lb, e_ub});
  }
  const double active_var_bound = snap_ge ? state.lb : state.ub;
  if ((snap_ge ? need_lb : need_ub) && std::isfinite(active_var_bound)) {
    const double r = std::round(scaling * active_var_bound);
    max_err = std::max(max_err, MaxAbsScaledTightSum(TightSum().Add(r).AddProd(
                                    -scaling, active_var_bound)));
  }

  // With mip_scale_for_precision, compare with the power of two scaling that
  // ScaleComponentContinuousVariablesForGivenPrecision() would use, on the
  // worst case error on var (in its original space).
  if (params.mip_scale_for_precision()) {
    const ComponentPrecisionScalingBounds bounds =
        ComputeComponentPrecisionScalingBounds(params, mp_model, compo, states,
                                               kInfinity, global_obj_precision);
    double pow2_scaling = 1.0;
    if (bounds.max_var_scaling[0] > 1.0 &&
        bounds.min_headroom[0] != kInfinity &&
        bounds.max_scaling_needed[0] > 1.0) {
      pow2_scaling = std::min(
          {bounds.max_var_scaling[0], bounds.min_headroom[0],
           std::ldexp(
               1.0, LowestPowerOfTwoAtOrAbove(bounds.max_scaling_needed[0]))});
    }

    // A rational scaling that is not a power of two also changes the objective
    // coefficient of var to a less "nice" value. We refuse it if the objective
    // can then no longer be scaled to integers with enough headroom left for
    // the other continuous variables, for which we reserve a scaling similar
    // to the power of two one of var. Note that the power of two scalings are
    // all absorbed by a single factor, see GlobalObjectivePrecision.
    int exp = 0;
    const bool is_power_of_two = std::frexp(scaling, &exp) == 0.5;
    if (!is_power_of_two && global_obj_precision != nullptr &&
        !global_obj_precision->KeepsObjectiveScalable(
            mp_model.underlying_model(), states, var, scaling,
            /*reserved_headroom=*/pow2_scaling)) {
      ++stats->num_objective_limited;
      return;
    }
    const auto [pow2_err_lb, pow2_err_ub] =
        GetContinuousVarDiscretizationErrorBounds(
            params.mip_proper_relaxation(), obj_coeff);
    const double pow2_err = std::max(-pow2_err_lb, pow2_err_ub) / pow2_scaling;
    if (max_err / scaling > pow2_err) {
      ++stats->num_pow2_preferred;
      return;
    }
  }

  ++stats->num_integers;
  stats->max_scaling = std::max(stats->max_scaling, scaling);
  state.error_lb = -max_err;
  state.error_ub = max_err;
  state.is_integer = true;
  ScaleVariableState(scaling, &state);
  if (obj_coeff != 0.0 && global_obj_precision != nullptr) {
    global_obj_precision->MarkDirty();
  }
}

void LogOneVariableComponentStats(const OneVariableComponentStats& stats,
                                  SolverLogger* logger) {
  if (stats.num_components == 0) return;
  SOLVER_LOG(logger, "[OneVarComponents] ", stats.num_components,
             " components: ", stats.num_integers,
             " marked as integer (max scaling ", stats.max_scaling, "), ",
             stats.num_fixed, " fixed, ", stats.num_pow2_preferred,
             " better scaled by a power of two, ", stats.num_objective_limited,
             " kept continuous for the objective, ", stats.num_missed,
             " without integrality multiplier.");
}

absl::Status ConstraintScaler::ScaleAndAddConstraint(
    const EnforcedLinearConstraint& mp_constraint,
    absl::Span<const MipVariableState> states, CpModelProto* cp_model,
    std::optional<ModularConstraintScaling> modular) {
  absl::Span<const IntegerVariableProto* const> var_domains =
      absl::MakeSpan(cp_model->variables());
  const int num_terms = mp_constraint.num_terms();

  // Transform the constraint and refresh the discretization errors of its
  // variables (they are lazily copied from the states).
  double ct_lower_bound;
  double ct_upper_bound;
  TransformConstraint(mp_constraint.proto(), states, &tmp_transformed_coeffs,
                      &ct_lower_bound, &ct_upper_bound);
  if (var_discretization_error_lb.size() != states.size()) {
    var_discretization_error_lb.assign(states.size(), 0.0);
    var_discretization_error_ub.assign(states.size(), 0.0);
  }
  for (int i = 0; i < num_terms; ++i) {
    const int var = mp_constraint.vars(i);
    var_discretization_error_lb[var] = states[var].error_lb;
    var_discretization_error_ub[var] = states[var].error_ub;
  }

  // Enforcement values must be transformed too. This should only involve
  // integer shifts of integer variables.
  tmp_enforcement_values.clear();
  for (int i = 0; i < mp_constraint.enforcement_vars().size(); ++i) {
    const MipVariableState& state = states[mp_constraint.enforcement_vars()[i]];
    TightSum transformed_value;
    transformed_value.AddProd(state.scaling,
                              mp_constraint.enforcement_values()[i]);
    transformed_value.AddProd(-state.scaling, state.shift);
    const auto [int_floor, int_ceil] = transformed_value.Int64Bounds();
    if (!int_floor.has_value() || int_floor != int_ceil) {
      return absl::InvalidArgumentError(absl::StrCat(
          "[Scaling] Non-integer or overflowing transformed enforcement "
          "value. MpConstraintProto: ",
          ProtobufShortDebugString(mp_constraint.proto())));
    }
    tmp_enforcement_values.push_back(*int_floor);
  }

  // Quick check to see if any enforcement is trivially false.
  // This can happen if we tightenned the bounds afterwards.
  const int num_enforcements = mp_constraint.enforcement_vars().size();
  for (int i = 0; i < num_enforcements; ++i) {
    const int var = mp_constraint.enforcement_vars()[i];
    const int64_t value = tmp_enforcement_values[i];
    const int64_t var_lb = var_domains[var]->domain(0);
    const int64_t var_ub =
        var_domains[var]->domain(var_domains[var]->domain_size() - 1);
    if (value < var_lb || value > var_ub) return absl::OkStatus();
  }

  ConstraintProto* constraint = cp_model->add_constraints();
  const absl::Status status = ScaleAndAddConstraint(
      mp_constraint.vars(), tmp_transformed_coeffs, ct_lower_bound,
      ct_upper_bound, mp_constraint.name(), var_domains, constraint, modular);
  if (!status.ok()) {
    return absl::Status(
        status.code(),
        absl::StrCat("[Scaling] ", status.message(), " MpConstraintProto: ",
                     ProtobufShortDebugString(mp_constraint.proto())));
  }

  if (!mp_constraint.IsEnforced()) return absl::OkStatus();

  // If the constraint is trivially satisfied, there is no need to add any
  // enforcement.
  const LinearConstraintProto& linear = constraint->linear();
  if (linear.vars().empty() && !linear.domain().empty() &&
      linear.domain(0) <= 0 && linear.domain(1) >= 0) {
    return absl::OkStatus();
  }

  bool has_integer_enforcement = false;
  for (int i = 0; i < num_enforcements; ++i) {
    const int var = mp_constraint.enforcement_vars()[i];
    const int64_t value = tmp_enforcement_values[i];
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

  if (linear.domain().empty()) {
    // Represent the infeasible base constraint `0 in {}` as `0 >= 1` so the
    // integer enforcement logic below handles it uniformly (with big_m = 1).
    constraint->mutable_linear()->add_domain(1);
    constraint->mutable_linear()->add_domain(kint64max);
  }

  // Enforced constraint should only be of the form >= rhs.
  CHECK_EQ(linear.domain_size(), 2);
  CHECK_EQ(linear.domain(1), kint64max);

  const auto overflow_error = [&]() {
    return absl::Status(
        absl::StatusCode::kInvalidArgument,
        absl::StrCat("[Scaling] Overflow issue while adding back integer "
                     "enforcement. MpConstraintProto: ",
                     ProtobufShortDebugString(mp_constraint.proto())));
  };

  // Compute minimum integer activity of the scaled constraint.
  int64_t min_integer_activity = 0;
  for (int i = 0; i < linear.coeffs().size(); ++i) {
    const int64_t coeff = linear.coeffs(i);
    int64_t term = 0;
    if (MulIntoOverflow(coeff,
                        coeff > 0 ? int_lower_bounds[i] : int_upper_bounds[i],
                        &term) ||
        AddIntoOverflow(term, &min_integer_activity)) {
      return overflow_error();
    }
  }

  // If min_integer_activity >= rhs, the constraint is just trivial, no need
  // to add extra enforcement.
  const int64_t rhs = linear.domain(0);
  if (min_integer_activity >= rhs) {
    return absl::OkStatus();
  }

  int64_t big_m = rhs;
  if (min_integer_activity == kint64min ||
      AddIntoOverflow(-min_integer_activity, &big_m)) {
    return overflow_error();
  }
  int64_t rhs_offset = 0;
  auto* mutable_linear = constraint->mutable_linear();
  for (int i = 0; i < num_enforcements; ++i) {
    const int var = mp_constraint.enforcement_vars()[i];
    const int64_t value = tmp_enforcement_values[i];
    const int64_t var_lb = var_domains[var]->domain(0);
    const int64_t var_ub =
        var_domains[var]->domain(var_domains[var]->domain_size() - 1);
    if (var_lb >= 0 && var_ub <= 1) continue;

    int_lower_bounds.push_back(var_lb);
    int_upper_bounds.push_back(var_ub);
    mutable_linear->add_vars(var);
    if (value == var_lb) {
      // We add big_m * (var - var_lb);
      mutable_linear->add_coeffs(big_m);
      if (AddIntoOverflow(var_lb, &rhs_offset)) return overflow_error();
    } else {
      // We add big_m * (var_ub - var);
      CHECK_EQ(value, var_ub);
      mutable_linear->add_coeffs(-big_m);
      if (var_ub == kint64min || AddIntoOverflow(-var_ub, &rhs_offset)) {
        return overflow_error();
      }
    }
  }

  int64_t scaled_offset = 0;
  int64_t new_rhs = rhs;
  if (MulIntoOverflow(rhs_offset, big_m, &scaled_offset) ||
      AddIntoOverflow(scaled_offset, &new_rhs) || new_rhs == kint64min) {
    return overflow_error();
  }
  mutable_linear->set_domain(0, new_rhs);

  // Check precondition and abort if it fail.
  //
  // TODO(user): This didn't fail on miplib so I didn't bother handling this
  // but an option is to recall the scaler a second time without extracting
  // integer enforcement.
  LinearOverflowChecker checker;
  for (int i = 0; i < mutable_linear->coeffs().size(); ++i) {
    if (!checker.AddTerm(mutable_linear->coeffs(i), int_lower_bounds[i],
                         int_upper_bounds[i])) {
      return overflow_error();
    }
  }

  return absl::OkStatus();
}

absl::Status ConstraintScaler::ScaleAndAddConstraint(
    absl::Span<const int> vars, absl::Span<const double> coeffs,
    double ct_lower_bound, double ct_upper_bound, absl::string_view name,
    absl::Span<const IntegerVariableProto* const> var_domains,
    ConstraintProto* constraint,
    std::optional<ModularConstraintScaling> modular) {
  if (keep_names && !name.empty()) constraint->set_name(name);

  // We relax the constraint bound by the wanted_precision before scaling. Note
  // that this is needed because now that the scaled constraint activity is
  // integer, we will floor/ceil these bounds.
  //
  // It might make more sense to use a relative precision here for large bounds,
  // but absolute is usually what is used in the MIP world. Also if the problem
  // was a pure integer problem, and a user asked for sum == 10k, we want to
  // stay exact here.
  const double bound_tol =
      use_tight_bounds ? tight_precision : wanted_precision;
  const double unrelaxed_lower_bound = ct_lower_bound;
  const double unrelaxed_upper_bound = ct_upper_bound;
  ct_lower_bound = ExactScaledSumLb(ct_lower_bound, -bound_tol);
  ct_upper_bound = ExactScaledSumUb(ct_upper_bound, bound_tol);

  // Constraint statistic used below.
  double min_activity = 0.0;
  double max_activity = 0.0;

  // Transform the constraint and its variable into an easier to use form.
  var_indices.clear();
  coefficients.clear();
  lower_bounds.clear();
  upper_bounds.clear();
  int_lower_bounds.clear();
  int_upper_bounds.clear();
  tmp_discretization_lb.Clear();
  tmp_discretization_ub.Clear();
  const bool has_discretization_errors = !var_discretization_error_lb.empty();
  const int num_coeffs = vars.size();
  for (int i = 0; i < num_coeffs; ++i) {
    const double coeff = coeffs[i];
    if (coeff == 0.0) continue;
    const int var = vars[i];
    if (has_discretization_errors) {
      const double var_err_lb = var_discretization_error_lb[var];
      const double var_err_ub = var_discretization_error_ub[var];
      if (coeff > 0.0) {
        if (var_err_lb != 0.0) tmp_discretization_lb.AddProd(coeff, var_err_lb);
        if (var_err_ub != 0.0) tmp_discretization_ub.AddProd(coeff, var_err_ub);
      } else {
        if (var_err_ub != 0.0) tmp_discretization_lb.AddProd(coeff, var_err_ub);
        if (var_err_lb != 0.0) tmp_discretization_ub.AddProd(coeff, var_err_lb);
      }
    }

    const auto& var_proto = var_domains[var];
    const int64_t lb = var_proto->domain(0);
    const int64_t ub = var_proto->domain(var_proto->domain_size() - 1);
    if (lb == 0 && ub == 0) continue;

    var_indices.push_back(var);
    coefficients.push_back(coeff);
    lower_bounds.push_back(lb);
    upper_bounds.push_back(ub);
    int_lower_bounds.push_back(lb);
    int_upper_bounds.push_back(ub);

    double min_term = coeff * static_cast<double>(lb);
    double max_term = coeff * static_cast<double>(ub);
    if (min_term > max_term) std::swap(min_term, max_term);

    min_activity += min_term;
    max_activity += max_term;
  }

  // The modular rounding is only needed if some component variable is not
  // fixed to zero.
  if (coefficients.empty()) modular.reset();

  // Given the bounds [discretization_error_lb, discretization_error_ub] on
  // sum_i coeff_i * (discretized_x_i - x_i), any feasible solution x of
  // ct_lower_bound <= sum_i coeff_i * x_i <= ct_upper_bound maps to a
  // discretized point satisfying:
  //   ct_lower_bound + discretization_error_lb <= sum_i coeff_i * discretized_x
  //   sum_i coeff_i * discretized_x <= ct_upper_bound + discretization_error_ub
  // Apply this before checking min/max activity and before computing tight
  // rounding errors on the discretized domain.
  const double discretization_error_lb =
      tmp_discretization_lb.TightLowerBound();
  const double discretization_error_ub =
      tmp_discretization_ub.TightUpperBound();
  if (use_proper_relaxation) {
    if (std::isfinite(ct_lower_bound) && discretization_error_lb != 0.0) {
      ct_lower_bound =
          ExactScaledSumLb(ct_lower_bound, discretization_error_lb);
    }
    if (std::isfinite(ct_upper_bound) && discretization_error_ub != 0.0) {
      ct_upper_bound =
          ExactScaledSumUb(ct_upper_bound, discretization_error_ub);
    }
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
  double scaling_factor;
  if (modular.has_value()) {
    scaling_factor = modular->scaling_factor;
    ComputeScalingErrors(coefficients, lower_bounds, upper_bounds,
                         scaling_factor, &relative_coeff_error,
                         &scaled_sum_error);
  } else {
    scaling_factor = FindBestScalingAndComputeErrors(
        coefficients, lower_bounds, upper_bounds, scaling_target,
        wanted_precision, &relative_coeff_error, &scaled_sum_error);
  }
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
  if (ct_lower_bound != -kInfinity) {
    max_discretization_error = std::max(
        max_discretization_error, std::max(0.0, -discretization_error_lb));
  }
  if (ct_upper_bound != kInfinity) {
    max_discretization_error = std::max(max_discretization_error,
                                        std::max(0.0, discretization_error_ub));
  }

  if (use_tight_bounds || use_proper_relaxation) {
    tight_error_helper.LoadUnscaledConstraint(coefficients, int_lower_bounds,
                                              int_upper_bounds);
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

      int_lower_bounds[new_bound_size] = int_lower_bounds[i];
      int_upper_bounds[new_bound_size] = int_upper_bounds[i];
      ++new_bound_size;
    }
  }
  int_lower_bounds.resize(new_bound_size);
  int_upper_bounds.resize(new_bound_size);

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
    // The modular unrounding errors depend on the final bounds, see below.
    if (!modular.has_value()) {
      max_unrounding_error =
          std::max({max_unrounding_error, lb_error2, ub_error2});
    }

    // Relax the bound to never loose a feasible solution of the original
    // problem that is within tight_precision. However, we don't relax too much
    // if we are not precise, and just hope for the best
    const double lb_relax = use_proper_relaxation
                                ? lb_error1
                                : std::min(wanted_precision, lb_error1);
    const double ub_relax = use_proper_relaxation
                                ? ub_error1
                                : std::min(wanted_precision, ub_error1);
    ct_lower_bound = ExactScaledSumLb(ct_lower_bound, -lb_relax);
    ct_upper_bound = ExactScaledSumUb(ct_upper_bound, ub_relax);
  }

  Fractional scaled_lb = std::ceil(ct_lower_bound * scaling_factor);
  Fractional scaled_ub = std::floor(ct_upper_bound * scaling_factor);
  if (modular.has_value()) {
    // In a TUM (resp. 2-modular) component, the LP extreme points are only
    // integral if the scaled bounds are integer (resp. even). We thus round
    // the bounds, before their relaxation by `bound_tol` (which is used as the
    // snapping tolerance of RoundScaledLowerBound()), to multiples of
    // `modular_factor`.
    const double m = static_cast<double>(modular->modular_factor);
    const double f = scaling_factor / m;
    const double tol =
        f * (use_proper_relaxation ? bound_tol : wanted_precision);
    const auto round_lb = [&](double lb) {
      return m * RoundScaledLowerBound(ProdBounds(lb, f).first, tol,
                                       use_proper_relaxation);
    };
    const bool tight = use_tight_bounds || use_proper_relaxation;
    if (std::isfinite(ct_lower_bound)) {
      scaled_lb = round_lb(unrelaxed_lower_bound);
      if (tight) {
        // Since the bound might have been enlarged, we add its distance to the
        // original bound.
        const double lb = std::max(scaled_lb, min_activity * scaling_factor);
        max_unrounding_error =
            std::max(max_unrounding_error,
                     tight_error_helper.GetLowerBoundUnroundingError(lb) /
                             scaling_factor +
                         std::max(0.0, ct_lower_bound - lb / scaling_factor));
      }
    }
    if (std::isfinite(ct_upper_bound)) {
      scaled_ub = -round_lb(-unrelaxed_upper_bound);
      if (tight) {
        const double ub = std::min(scaled_ub, max_activity * scaling_factor);
        max_unrounding_error =
            std::max(max_unrounding_error,
                     tight_error_helper.GetUpperBoundUnroundingError(ub) /
                             scaling_factor +
                         std::max(0.0, ub / scaling_factor - ct_upper_bound));
      }
    }
  }

  // Add the constraint bounds. Because we are sure the scaled constraint fits
  // in an int64_t, if the scaled bounds are too large, the constraint is either
  // always true or always false.
  //
  // Note that since the min/max scaled activity magnitude should be under the
  // scaling_target (<< kint64max) it is okay to compare to kint64min/kint64max
  // casted to double.
  int64_t final_lb;
  if (ct_lower_bound == kInfinity ||
      scaled_lb >= static_cast<double>(kint64max)) {
    // Corner case: infeasible model.
    final_lb = kint64max;
  } else if (ct_lower_bound == -kInfinity ||
             scaled_lb <= static_cast<double>(kint64min)) {
    final_lb = kint64min;
  } else {
    final_lb = CeilRatio(IntegerValue(static_cast<int64_t>(scaled_lb)),
                         IntegerValue(gcd))
                   .value();
  }

  int64_t final_ub;
  if (ct_upper_bound == -kInfinity ||
      scaled_ub <= static_cast<double>(kint64min)) {
    // Corner case: infeasible model.
    final_ub = kint64min;
  } else if (ct_upper_bound == kInfinity ||
             scaled_ub >= static_cast<double>(kint64max)) {
    final_ub = kint64max;
  } else {
    final_ub = FloorRatio(IntegerValue(static_cast<int64_t>(scaled_ub)),
                          IntegerValue(gcd))
                   .value();
  }

  if (ct_lower_bound == kInfinity || ct_upper_bound == -kInfinity ||
      final_lb > final_ub) {
    // Empty domain: emit `0 in {}` so CP-SAT reports INFEASIBLE (or falsifies
    // enforcement literals) rather than MODEL_INVALID.
    mutable_linear->clear_vars();
    mutable_linear->clear_coeffs();
    int_lower_bounds.clear();
    int_upper_bounds.clear();
  } else {
    mutable_linear->add_domain(final_lb);
    mutable_linear->add_domain(final_ub);
  }

  return absl::OkStatus();
}

namespace {

// Tries to find an odd rational multiplier for `coefficients` within
// `fp_limit`, shifting any coefficient with `frexp` exponent `exp <
// min_target_exp` up by `2^(min_target_exp - exp)` before rational
// approximation:
// - `min_target_exp = k` (with a 7-bit ULP safety margin relative to
//   `tolerance`) strips up to `k` bits of power-of-two denominator (e.g.
//   recovering `5^6` from `1e-6 = 2^-6 * 5^-6`).
// - `min_target_exp = 0` normalizes `< 0.5` coefficients into `[0.5, 1.0)`,
//   making 8-to-10-digit decimals (such as `333.33333333` in `timtab1CUTS`)
//   invariant to power-of-two variable down-scaling without amplifying their
//   truncation error by `2^k`.
// - `min_target_exp = int::min()` leaves raw fractional parts untouched so
//   unscaled truncated rationals like `1/450` or `1/1440` are not shifted.
double FindOddRationalScaling(absl::Span<const double> coefficients,
                              double tolerance, double fp_limit,
                              int min_target_exp) {
  double multiplier = 1.0;
  const int64_t limit =
      static_cast<int64_t>(std::min(std::abs(fp_limit), 1e15));
  for (const double coeff : coefficients) {
    if (coeff == 0.0) continue;
    const double abs_coeff = std::abs(coeff);
    int exp = 0;
    const double mantissa = std::frexp(abs_coeff, &exp);
    const int shift = min_target_exp > exp ? min_target_exp - exp : 0;
    const double scaled = std::ldexp(mantissa, exp + shift);
    const double frac = scaled - std::floor(scaled);
    int64_t f =
        FindRationalFactor(multiplier * frac, limit, multiplier * tolerance);
    if (f == 0) return 0.0;
    const int tz_f = std::countr_zero(static_cast<uint64_t>(f));
    f >>= tz_f;
    multiplier *= f;
    if (multiplier > limit) return 0.0;
    const int max_pow2 = MaxPowerOfTwoExponentWithinBound(multiplier, limit);
    if (shift + tz_f > max_pow2) {
      // `FindRationalFactor` relied on `2^(shift + tz_f)`, which exceeds
      // `limit / multiplier`. Verify whether `multiplier * 2^r` already
      // realizes an integer on `coeff` within `limit`.
      const int r = std::min(max_pow2, std::max(0, 53 - exp));
      double frac_r = std::ldexp(mantissa, exp + r);
      frac_r -= std::floor(frac_r);
      const double prod = multiplier * frac_r;
      if (std::abs(prod - std::round(prod)) >=
          std::ldexp(multiplier * tolerance, r)) {
        return 0.0;
      }
    }
  }
  return multiplier;
}

}  // namespace

double FindBestScalingAndComputeErrors(
    absl::Span<const double> coefficients,
    absl::Span<const double> lower_bounds,
    absl::Span<const double> upper_bounds, int64_t max_absolute_activity,
    double wanted_absolute_activity_precision, double* relative_coeff_error,
    double* scaled_sum_error, double* max_scaling_factor) {
  // Starts by computing the highest possible factor.
  double scaling_factor = GetBestScalingOfDoublesToInt64(
      coefficients, lower_bounds, upper_bounds, max_absolute_activity);
  if (max_scaling_factor != nullptr) *max_scaling_factor = scaling_factor;
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
  //
  // We try two tolerances and three mantissa shift targets (`k`, `0`, and no
  // shift). FindRationalFactor() accepts a denominator q as soon as
  // |q * x - round(q * x)| < q * tolerance, so with a tolerance of 1e-8 any
  // convergent with q >= 1e4 is accepted and we cannot reliably recover
  // denominators like 15625 (i.e. coefficients that are multiples of 1e-6),
  // the result even depends on the coefficient order. We thus first try a
  // tight tolerance to recover exact rationals. But the loose tolerance also
  // acts as a search for a smaller factor with an acceptable error when the
  // coefficients are not exact rationals (this matters for the headroom of
  // continuous variables), so we still try it afterwards.
  for (const double rational_tolerance : {1e-10, 1e-8}) {
    int tol_exp = 0;
    std::frexp(rational_tolerance, &tol_exp);
    const int k = std::clamp(53 + tol_exp - 7, 0, 52);
    for (const int min_target_exp : {k, 0, std::numeric_limits<int>::min()}) {
      double integer_factor = FindOddRationalScaling(
          coefficients, rational_tolerance, scaling_factor, min_target_exp);
      DCHECK(std::isfinite(integer_factor));
      if (integer_factor == 0 || integer_factor >= scaling_factor) continue;

      double local_relative_coeff_error;
      double local_scaled_sum_error;

      // As long as integer_factor * 2^x is smaller than scaling_factor,
      // consider it!
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
  }

  DCHECK(std::isfinite(scaling_factor));
  return scaling_factor;
}

namespace {

// Clamps [lb, ub] to [-max_bound, max_bound] and returns the number of bounds
// that were truncated, or -1 if the domain does not intersect this interval.
int ClampBounds(double max_bound, double* lb, double* ub) {
  // Deal with the corner case of a domain far away from zero.
  //
  // TODO(user): We could avoid these cases by shifting the domain of
  // all variables to contain zero. This should also lead to a better scaling,
  // but it has some complications with integer variables and requires some
  // post-solve.
  if (*lb > max_bound || *ub < -max_bound) return -1;
  int num_truncated_bounds = 0;
  if (*lb < -max_bound) {
    ++num_truncated_bounds;
    *lb = -max_bound;
  }
  if (*ub > max_bound) {
    ++num_truncated_bounds;
    *ub = max_bound;
  }
  return num_truncated_bounds;
}

}  // namespace

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
  for (MPVariableProto& mp_var : *mp_model->mutable_variable()) {
    double lb = mp_var.lower_bound();
    double ub = mp_var.upper_bound();
    const int num_truncated = ClampBounds(kMaxVariableBound, &lb, &ub);
    if (num_truncated < 0) {
      SOLVER_LOG(logger, "Error: variable ", mp_var,
                 " is outside [-mip_max_bound..mip_max_bound]");
      return false;
    }
    num_truncated_bounds += num_truncated;
    mp_var.set_lower_bound(lb);
    mp_var.set_upper_bound(ub);
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

bool ClampVariableBounds(const SatParameters& params,
                         absl::Span<const int> vars,
                         absl::Span<MipVariableState> states,
                         SolverLogger* logger, int* num_clamped) {
  const double kMaxVariableBound = std::round(params.mip_max_bound());
  for (const int var : vars) {
    MipVariableState& state = states[var];
    const int num_truncated =
        ClampBounds(kMaxVariableBound, &state.lb, &state.ub);
    if (num_truncated < 0) {
      SOLVER_LOG(logger, "Error: variable #", var, " in [", state.lb, ", ",
                 state.ub, "] is outside [-mip_max_bound..mip_max_bound]");
      return false;
    }
    *num_clamped += num_truncated;
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

void LogShiftStats(const ShiftStats& stats, SolverLogger* logger) {
  if (stats.num_shifted > 0) {
    SOLVER_LOG(logger, "[Shifting] Shifted ", FormatCounter(stats.num_shifted),
               " variable towards zero with max shift magnitude ",
               stats.max_shift, ".");
  }
}

void ShiftVariables(
    const std::function<double(const MipVariableState&)>& get_shift,
    absl::Span<const int> vars, absl::Span<MipVariableState> states,
    ShiftStats* stats) {
  for (const int var : vars) {
    MipVariableState& state = states[var];
    const double offset = get_shift(state);
    if (offset == 0.0) continue;
    // Currently no caller shifts an already-shifted variable (after
    // ShiftBeforeScalingValue(), 0 is in [state.lb, state.ub] so
    // FinalCpSatShiftValue() returns 0). Assert this and skip in opt builds so
    // `state.shift` is never accumulated with rounding error.
    DCHECK_EQ(state.shift, 0.0);
    if (state.shift != 0.0) continue;
    // We should never shift an integer variable by a non-integer value, this
    // is not correct.
    DCHECK(!state.is_integer || offset == std::round(offset));
    ++stats->num_shifted;
    stats->max_shift = std::max(stats->max_shift, std::abs(offset));
    state.lb = ExactScaledSumLb(state.lb, -offset);
    state.ub = ExactScaledSumUb(state.ub, -offset);
    state.shift = offset / state.scaling;
  }
}

bool ConvertVariableToCpVariable(const SatParameters& params, int var_index,
                                 double objective_coefficient,
                                 MipVariableState* state,
                                 IntegerVariableProto* cp_var,
                                 int* num_small_domains) {
  const double kMaxVariableBound =
      std::nextafter(static_cast<double>(kint64max / 2), 0.0);
  const int64_t kSmallDomainSize = 1000;

  // Bounds must have been clamped by ClampVariableBounds() first.
  const double var_lb = state->lb;
  const double var_ub = state->ub;
  CHECK_GE(var_lb, -kMaxVariableBound);
  CHECK_LE(var_ub, kMaxVariableBound);

  // Note that the cast is "perfect" because we forbid large values.
  if (params.mip_proper_relaxation() && !state->is_integer) {
    // If we have a feasible floating-point solution, we want the discretized
    // version where we always snap to the integer point with the best
    // objective to remain feasible, and record tight bounds [err_lb, err_ub]
    // on (snapped_var - var) for constraint relaxation.
    double d_min;
    double d_max;
    double err_lb;
    double err_ub;
    if (objective_coefficient > 0.0) {
      d_min = std::floor(var_lb);
      d_max = std::floor(var_ub);
      err_lb = -1.0;
      err_ub = 0.0;
    } else if (objective_coefficient < 0.0) {
      d_min = std::ceil(var_lb);
      d_max = std::ceil(var_ub);
      err_lb = 0.0;
      err_ub = 1.0;
    } else {
      d_min = std::round(var_lb);
      d_max = std::round(var_ub);
      err_lb = -0.5;
      err_ub = 0.5;
    }
    if (d_min == d_max) {
      err_lb = std::max(err_lb, ExactScaledSumLb(d_min, -var_ub));
      err_ub = std::min(err_ub, ExactScaledSumUb(d_min, -var_lb));
    }
    cp_var->add_domain(static_cast<int64_t>(d_min));
    cp_var->add_domain(static_cast<int64_t>(d_max));
    state->error_lb = err_lb;
    state->error_ub = err_ub;
  } else {
    const double lb_tol =
        std::max(params.mip_wanted_precision(), -state->error_lb);
    const double ub_tol =
        std::max(params.mip_wanted_precision(), state->error_ub);
    cp_var->add_domain(static_cast<int64_t>(std::ceil(var_lb - lb_tol)));
    cp_var->add_domain(static_cast<int64_t>(std::floor(var_ub + ub_tol)));
    if (!state->is_integer && (var_lb != 0.0 || var_ub != 0.0)) {
      state->error_lb = -1.0;
      state->error_ub = 1.0;
    }
  }

  if (cp_var->domain(0) > cp_var->domain(1)) {
    LOG(WARNING) << "Variable #" << var_index
                 << " cannot take an integer value. [" << var_lb << ", "
                 << var_ub << "] is_integer: " << state->is_integer;
    return false;
  }

  // Notify if a continuous variable has a small domain as this is likely to
  // make an all integer solution far from a continuous one.
  if (!state->is_integer && num_small_domains != nullptr) {
    const double diff = var_ub - var_lb;
    if (diff > params.mip_wanted_precision() && diff < kSmallDomainSize) {
      ++*num_small_domains;
    }
  }
  return true;
}

void DisplayConstraintScalingStats(const SatParameters& params,
                                   const ConstraintScaler& scaler,
                                   int num_clamped, int num_small_domains,
                                   SolverLogger* logger) {
  const int64_t kSmallDomainSize = 1000;
  if (num_small_domains > 0) {
    SOLVER_LOG(logger, "[Scaling] Warning: ", FormatCounter(num_small_domains),
               " continuous variable domains with fewer than ",
               kSmallDomainSize, " values.");
  }

  if (scaler.num_trivial_constraints > 0) {
    SOLVER_LOG(
        logger, "[Scaling] Skipped ",
        FormatCounter(scaler.num_trivial_constraints),
        " trivial constraints (activity computed with doubles, tolerance=",
        params.mip_tight_precision() != 0.0 ? params.mip_tight_precision()
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
}

void PopulateCpModelFloatingPointObjective(
    const MPModelProtoWrapper& mp_model,
    absl::Span<const MipVariableState> states, CpModelProto* cp_model) {
  // Since cp_model supports a floating point objective, we use that. This will
  // allow us to scale the objective a bit later so we can potentially do more
  // domain reduction first.
  auto* float_objective = cp_model->mutable_floating_point_objective();
  float_objective->Clear();
  const double scaling = mp_model.objective_scaling();
  float_objective->set_maximize(scaling < 0.0);
  AccurateSum<double> offset;
  offset.Add(mp_model.objective_offset());
  const int num_variables = mp_model.num_variables();
  for (int i = 0; i < num_variables; ++i) {
    const double coeff = mp_model.variables(i).objective_coefficient();
    if (coeff == 0.0) continue;
    const MipVariableState& state = states[i];
    if (state.shift != 0.0) offset.Add(coeff * state.shift);
    float_objective->add_vars(i);
    float_objective->add_coeffs(coeff / state.scaling * scaling);
  }
  if (offset.Value() != 0.0) {
    float_objective->set_offset(offset.Value() * scaling);
  }

  // If the objective is fixed to zero, we consider there is none.
  if (float_objective->offset() == 0 && float_objective->vars().empty()) {
    cp_model->clear_floating_point_objective();
  }
}

bool ConvertMPModelProtoToCpModelProto(
    const SatParameters& params, const MPModelProtoWrapper& mp_model,
    int num_clamped, absl::Span<const double> integer_error_lb,
    absl::Span<const double> integer_error_ub, CpModelProto* cp_model,
    SolverLogger* logger) {
  CHECK(cp_model != nullptr);
  cp_model->Clear();
  cp_model->set_name(mp_model.name());

  int num_small_domains = 0;
  const int num_variables = mp_model.num_variables();
  CHECK_EQ(integer_error_lb.size(), num_variables);
  CHECK_EQ(integer_error_ub.size(), num_variables);
  std::vector<MipVariableState> states = InitialVariableStates(mp_model);
  for (int i = 0; i < num_variables; ++i) {
    states[i].error_lb = integer_error_lb[i];
    states[i].error_ub = integer_error_ub[i];
  }

  // Add the variables.
  const bool keep_names = !params.ignore_names();
  for (int i = 0; i < num_variables; ++i) {
    IntegerVariableProto* cp_var = cp_model->add_variables();
    if (keep_names) cp_var->set_name(mp_model.variables(i).name());
    if (!ConvertVariableToCpVariable(
            params, i, mp_model.variables(i).objective_coefficient(),
            &states[i], cp_var, &num_small_domains)) {
      return false;
    }
  }

  ConstraintScaler scaler;
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
        scaler.ScaleAndAddConstraint(mp_constraint, states, cp_model);
    if (!status.ok()) {
      SOLVER_LOG(logger, status.message());
      return false;
    }
  }

  DisplayConstraintScalingStats(params, scaler, num_clamped, num_small_domains,
                                logger);
  PopulateCpModelFloatingPointObjective(mp_model, states, cp_model);
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
  return true;
}

double ComputeTrueObjectiveLowerBound(
    const CpModelProto& model_proto_with_floating_point_objective,
    const CpModelProto& model_proto_with_integer_objective_and_domains,
    const int64_t inner_integer_objective_lower_bound) {
  const CpModelProto& proto = model_proto_with_floating_point_objective;
  const FloatObjectiveProto& float_obj = proto.floating_point_objective();
  if (inner_integer_objective_lower_bound == kint64min) {
    return float_obj.maximize() ? std::numeric_limits<double>::infinity()
                                : -std::numeric_limits<double>::infinity();
  }
  if (inner_integer_objective_lower_bound == kint64max) {
    return float_obj.maximize() ? -std::numeric_limits<double>::infinity()
                                : std::numeric_limits<double>::infinity();
  }

  // Extract and normalize the objective terms so each variable appears once.
  std::vector<int> vars;
  std::vector<absl::InlinedVector<double, 1>> float_coeffs;
  std::vector<int64_t> int_coeffs;
  std::vector<int> var_to_index(proto.variables().size(), -1);
  auto get_or_create_index = [&](int var) {
    int& index = var_to_index[var];
    if (index == -1) {
      index = vars.size();
      vars.push_back(var);
      float_coeffs.emplace_back();
      int_coeffs.push_back(0);
    }
    return index;
  };

  for (int i = 0; i < float_obj.vars().size(); ++i) {
    const double coeff =
        float_obj.maximize() ? -float_obj.coeffs(i) : float_obj.coeffs(i);
    if (coeff == 0.0) continue;
    float_coeffs[get_or_create_index(float_obj.vars(i))].push_back(coeff);
  }
  const CpObjectiveProto& integer_objective =
      model_proto_with_integer_objective_and_domains.objective();
  for (int i = 0; i < integer_objective.vars().size(); ++i) {
    const int64_t coeff = integer_objective.coeffs(i);
    if (coeff == 0) continue;
    int64_t& c = int_coeffs[get_or_create_index(integer_objective.vars(i))];
    c = CapAdd(c, coeff);
  }

  std::vector<double> scaled_coeffs(vars.size());
  std::vector<double> rounded_coeffs(vars.size());
  std::vector<int64_t> lbs(vars.size());
  std::vector<int64_t> ubs(vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    const auto& domain =
        model_proto_with_integer_objective_and_domains.variables(vars[i])
            .domain();
    lbs[i] = domain[0];
    ubs[i] = domain[domain.size() - 1];
  }
  const FloatObjectiveProto& mapping_float_obj =
      model_proto_with_integer_objective_and_domains.floating_point_objective();
  if (!mapping_float_obj.domain_mins().empty()) {
    DCHECK_EQ(mapping_float_obj.vars_size(),
              mapping_float_obj.domain_mins_size());
    DCHECK_EQ(mapping_float_obj.vars_size(),
              mapping_float_obj.domain_maxs_size());
    for (int i = 0; i < mapping_float_obj.vars_size(); ++i) {
      const int var = mapping_float_obj.vars(i);
      if (var < var_to_index.size() && var_to_index[var] != -1) {
        const int idx = var_to_index[var];
        lbs[idx] = mapping_float_obj.domain_mins(i);
        ubs[idx] = mapping_float_obj.domain_maxs(i);
      }
    }
  }

  TightSum adjusted_lb;
  adjusted_lb.Add(inner_integer_objective_lower_bound);
  TightSum offset_sum;
  offset_sum.Add(float_obj.maximize() ? -float_obj.offset()
                                      : float_obj.offset());
  for (int i = 0; i < vars.size(); ++i) {
    const auto [c_lb, c_ub] =
        float_coeffs[i].size() == 1
            ? std::make_pair(float_coeffs[i][0], float_coeffs[i][0])
            : TightSumOf<double>(float_coeffs[i]).TightBounds();
    if (c_lb >= 0.0) {
      scaled_coeffs[i] = c_lb;
      if (c_lb != c_ub && lbs[i] < 0) {
        // Adjust for any eventual rounding of down c_lb.
        // Note: the subtraction of two terms that differ by one ULP is exact.
        offset_sum.AddProd(c_ub - c_lb, lbs[i]);
      }
    } else {
      scaled_coeffs[i] = c_ub;
      if (c_lb != c_ub && ubs[i] > 0) {
        offset_sum.AddProd(c_lb - c_ub, ubs[i]);
      }
    }

    const auto [r_lb, r_ub] = Int64DoubleBounds(int_coeffs[i]);
    rounded_coeffs[i] = r_lb;
    if (r_lb != r_ub) {
      adjusted_lb.AddProd(static_cast<int64_t>(r_lb) - int_coeffs[i], ubs[i]);
    }
  }

  TightScalingErrorHelper helper;
  helper.LoadBothConstraints(scaled_coeffs, rounded_coeffs, lbs, ubs);
  const double result = helper.GetScaledLowerBoundFromRoundedLowerBound(
      std::move(adjusted_lb), offset_sum.TightLowerBound());
  // Fix negative zero.
  return result == 0.0 ? 0.0 : (float_obj.maximize() ? -result : result);
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

  // Fail if the bipartite graph was not connected.
  for (const double c_scale : *col_scale) {
    if (c_scale == 0.0) return false;
  }
  for (const double r_scale : *row_scale) {
    if (r_scale == 0.0) return false;
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

// Tightens variable bounds in a TUM (`base_factor = 1.0`) or 2-modular
// (`base_factor = 2.0`) component using Cramer's rule.
//
// The bounds computed here are valid for the vertices of the component LP. If
// the component contains integer variables, the caller must have checked that
// these vertices are integral for every integer assignment of the external
// variables (Hoffman-Kruskal), otherwise integer feasible solutions might be
// outside the bounds computed here.
//
// In scaled coordinates `\tilde{x}_j = col_scale[j] * x_j` and row-scaled
// constraints `\tilde{A}_{r, j} = A_{r, j} / (row_scale[r] * col_scale[j])`,
// the component matrix `\tilde{A}` has entries in `{-1, 0, +1}`.
//
// For any fixed assignment `z*` of the external variables, consider a basic
// optimal solution (vertex) of the component LP:
// - A subset `C_B` of `k` basic variables is uniquely determined by `k` active
//   rows `R_B` (where `\tilde{A}_{R_B, C_B}` is `k x k` non-singular):
//     `\tilde{x}_{C_B} = \tilde{A}_{R_B, C_B}^{-1} (b'_{R_B} - \tilde{A}_{R_B,
//     C_N} \tilde{x}_{C_N})`
// - The remaining non-basic variables `C_N` sit at one of their finite bounds
//   (or `0` along free directions in the lineality space).
// - By Cramer's rule, because every nonsingular submatrix of `\tilde{A}` has
//   determinant `\pm 1` (or `\pm 1, \pm 2` when 2-modular), every entry of
//   `\tilde{A}_{R_B, C_B}^{-1}` and `\tilde{A}_{R_B, C_B}^{-1} \tilde{A}_{R_B,
//   C_N}` has magnitude at most `base_factor`.
// - By the triangle inequality, every coordinate of the vertex satisfies:
//     `|\tilde{x}_j| <= base_factor * (\sum_{r in R_B} M_row(r)
//                                      + \sum_{j' in C_N} M_col(j'))`
//   where `M_row(r)` is the maximum finite magnitude of the effective RHS
//   `b'_r = (b_r - A_{r, ext} z*) / row_scale[r]`, and `M_col(j')` is the
//   maximum finite bound magnitude of variable `j'`. Since `|R_B| + |C_N| = n`,
//   this is at most `base_factor` times the sum of the `n` largest magnitudes.
// Bounds of a variable expressed in the wrapper space (see MipVariableState).
// Used for the external variables of a component, whose coefficients are read
// from the (untransformed) model. Note that this is exact for integer
// variables, which are the only external variables we have in practice.
std::pair<double, double> WrapperSpaceBounds(const MipVariableState& state) {
  return {state.lb / state.scaling + state.shift,
          state.ub / state.scaling + state.shift};
}

void TightenComponentBounds(
    absl::Span<const int> compo_vars, absl::Span<const int> compo_rows,
    absl::Span<const double> col_scale, absl::Span<const double> row_scale,
    absl::Span<const int> var_to_local_col, double base_factor,
    const MPModelProtoWrapper& mp_model, absl::Span<MipVariableState> states,
    TumStats* stats) {
  if (compo_rows.empty()) return;

  // Some half-spaces (variable bounds or constraint sides) can be relaxed
  // (dropped) without changing the optimal value as long as they contain the
  // vertex box `[-L_low, L_low]^n` of the relaxed polyhedron `P_low`.
  //
  // Specifically, in a minimization problem with objective `c_j`, provided
  // every variable in the component is bounded in its improving direction
  // (`\tilde{l}_j > -inf` whenever `c_j > 0`, and `\tilde{u}_j < +inf` whenever
  // `c_j < 0`) and these bounds are kept, any recession direction `d` of
  // `P_low` satisfies `d_j >= 0` whenever `c_j > 0` and `d_j <= 0` whenever
  // `c_j < 0`, so `c^T d >= 0`. Thus `P_low` has no improving recession rays,
  // and its minimum is still achieved at a basic solution `v*` of `P_low` with
  // `|\tilde{v}^*_j| <= L_low`. As long as every dropped half-space contains
  // `[-L_low, +L_low]^n`, `v*` automatically satisfies it and is optimal for
  // the original polyhedron.
  //
  // Note that the wrapper always holds a minimization problem.
  bool can_drop = true;
  for (int local_col = 0; local_col < compo_vars.size(); ++local_col) {
    const int var = compo_vars[local_col];
    const double obj = mp_model.variables(var).objective_coefficient();
    if ((obj > 0.0 && !std::isfinite(states[var].lb)) ||
        (obj < 0.0 && !std::isfinite(states[var].ub))) {
      can_drop = false;
      break;
    }
  }

  // A droppable half-space is described by the largest vertex limit
  // `drop_limit` such that `[-drop_limit, +drop_limit]^n` is contained in it,
  // and by the increase `delta` of the magnitude of its row or column `item`
  // if it must be kept.
  struct DroppableHalfSpace {
    double drop_limit;
    double delta;
    int item;
    bool operator<(const DroppableHalfSpace& other) const {
      return drop_limit < other.drop_limit;
    }
  };
  std::vector<DroppableHalfSpace> droppable;

  // Magnitudes `M_row(r)` of the rows followed by `M_col(j)` of the columns,
  // assuming all droppable half-spaces are dropped.
  const int num_cols = compo_vars.size();
  std::vector<double> magnitude(compo_rows.size() + num_cols, 0.0);
  const auto add_to_magnitude = [&magnitude](int item, double value) {
    magnitude[item] =
        TightSum().Add(magnitude[item]).Add(value).TightUpperBound();
  };

  // 1. Rows.
  //
  // If any connected external variable has an infinite bound, the effective RHS
  // `b_r - A_{r, ext} z*` cannot be bounded across all feasible `z*`, so we
  // abort.
  TightSum min_activity;
  TightSum max_activity;
  for (int local_row = 0; local_row < compo_rows.size(); ++local_row) {
    const MPConstraintProto& constraint =
        mp_model.constraints(compo_rows[local_row]).proto();
    const double r_scale = row_scale[local_row];
    min_activity.Clear();
    max_activity.Clear();
    int row_nnz = 0;
    bool all_external_integer = true;
    double min_external_step = std::numeric_limits<double>::infinity();
    for (int i = 0; i < constraint.var_index_size(); ++i) {
      const double coeff = constraint.coefficient(i);
      if (coeff == 0.0) continue;
      const int var = constraint.var_index(i);
      if (var_to_local_col[var] != -1) {
        ++row_nnz;
        continue;
      }
      const MipVariableState& external = states[var];
      const auto [var_lb, var_ub] = WrapperSpaceBounds(external);
      if (!std::isfinite(var_lb) || !std::isfinite(var_ub)) return;
      if (coeff > 0.0) {
        min_activity.AddProd(coeff, var_lb);
        max_activity.AddProd(coeff, var_ub);
      } else {
        min_activity.AddProd(coeff, var_ub);
        max_activity.AddProd(coeff, var_lb);
      }
      if (!external.is_integer) all_external_integer = false;
      min_external_step = std::min(
          min_external_step, ExactScaledSumLb(std::abs(coeff), 0.0, r_scale));
    }
    const double min_ext_activity = min_activity.TightLowerBound();
    const double max_ext_activity = max_activity.TightUpperBound();
    if (!std::isfinite(min_ext_activity) || !std::isfinite(max_ext_activity)) {
      return;
    }

    const bool has_lb = std::isfinite(constraint.lower_bound());
    const bool has_ub = std::isfinite(constraint.upper_bound());
    if (!can_drop || has_lb == has_ub) {
      // Keep both sides: `M_row(r)` is the maximum finite magnitude of the
      // effective RHS.
      double max_of_finite_rhs_magnitude = 0.0;
      if (has_lb) {
        max_of_finite_rhs_magnitude =
            std::max({max_of_finite_rhs_magnitude,
                      std::abs(ExactScaledSumLb(constraint.lower_bound(),
                                                -max_ext_activity, r_scale)),
                      std::abs(ExactScaledSumUb(constraint.lower_bound(),
                                                -min_ext_activity, r_scale))});
      }
      if (has_ub) {
        max_of_finite_rhs_magnitude =
            std::max({max_of_finite_rhs_magnitude,
                      std::abs(ExactScaledSumLb(constraint.upper_bound(),
                                                -max_ext_activity, r_scale)),
                      std::abs(ExactScaledSumUb(constraint.upper_bound(),
                                                -min_ext_activity, r_scale))});
      }
      if (!std::isfinite(max_of_finite_rhs_magnitude)) return;
      magnitude[local_row] = max_of_finite_rhs_magnitude;
      continue;
    }

    // One-sided row, written as `\tilde{a}_r \tilde{x} >= rhs(z)` (negating
    // the row for an upper bound). `tightest` is an upper bound on the largest
    // value of `rhs(z)` over the external domain, and `loosest` a lower bound
    // on its smallest value. Since `\tilde{a}_r` has `row_nnz` entries in
    // `{-1, +1}`, the half-space `\tilde{a}_r \tilde{x} >= rhs` contains the
    // box `[-L, +L]^n` iff `L <= -rhs / row_nnz`.
    double tightest;
    double loosest;
    if (has_lb) {
      tightest = ExactScaledSumUb(constraint.lower_bound(), -min_ext_activity,
                                  r_scale);
      loosest = ExactScaledSumLb(constraint.lower_bound(), -max_ext_activity,
                                 r_scale);
    } else {
      tightest = -ExactScaledSumLb(constraint.upper_bound(), -max_ext_activity,
                                   r_scale);
      loosest = -ExactScaledSumUb(constraint.upper_bound(), -min_ext_activity,
                                  r_scale);
    }
    if (!std::isfinite(tightest) || !std::isfinite(loosest)) return;
    const auto drop_limit_for = [row_nnz](double rhs) {
      return ExactScaledSumLb(-rhs, 0.0, static_cast<double>(row_nnz));
    };

    // When the row is kept only for the assignments `z*` where `rhs(z*)` is
    // the tightest, its contribution to the vertex bound is `|tightest|`.
    const double tightest_magnitude = std::abs(tightest);
    if (tightest < 0.0) {
      droppable.push_back(
          {drop_limit_for(tightest), tightest_magnitude, local_row});
    } else {
      magnitude[local_row] = tightest_magnitude;
    }

    // Otherwise, its contribution is the maximum magnitude over all `z*`.
    //
    // This is where big-M constraints `x - y + M b >= rhs` with a binary `b`
    // shine: when all external variables are integer, any `rhs(z*)` that is not
    // the tightest is at most `second = tightest - min_external_step`, which is
    // very negative, so the half-space is only ever needed with `rhs(z*) ==
    // tightest`.
    const double static_magnitude =
        std::max(tightest_magnitude, std::abs(loosest));
    if (static_magnitude > tightest_magnitude) {
      const double delta = TightSum()
                               .Add(static_magnitude)
                               .Add(-tightest_magnitude)
                               .TightUpperBound();
      const double second = all_external_integer
                                ? ExactScaledSumUb(tightest, -min_external_step)
                                : tightest;
      if (second < 0.0) {
        droppable.push_back({drop_limit_for(second), delta, local_row});
      } else {
        add_to_magnitude(local_row, delta);
      }
    }
  }

  // 2. Variable bounds: `M_col(j)` is the kept bound magnitude, and large
  // bounds in the non-improving direction of each variable are droppable.
  for (int local_col = 0; local_col < num_cols; ++local_col) {
    const int var = compo_vars[local_col];
    const int item = compo_rows.size() + local_col;
    const double c_scale = col_scale[local_col];
    const double raw_lb = states[var].lb;
    const double raw_ub = states[var].ub;
    if (std::isfinite(raw_lb) && std::isfinite(raw_ub)) {
      const double obj = mp_model.variables(var).objective_coefficient();
      if (can_drop && obj >= 0.0 && raw_ub > std::abs(raw_lb)) {
        const double kept_unscaled = std::abs(raw_lb);
        magnitude[item] = ProdBounds(kept_unscaled, c_scale).second;
        const double delta = TightSum()
                                 .AddProd(raw_ub, c_scale)
                                 .AddProd(-kept_unscaled, c_scale)
                                 .TightUpperBound();
        droppable.push_back({ProdBounds(raw_ub, c_scale).first, delta, item});
      } else if (can_drop && obj <= 0.0 && -raw_lb > std::abs(raw_ub)) {
        const double kept_unscaled = std::abs(raw_ub);
        magnitude[item] = ProdBounds(kept_unscaled, c_scale).second;
        const double delta = TightSum()
                                 .AddProd(-raw_lb, c_scale)
                                 .AddProd(-kept_unscaled, c_scale)
                                 .TightUpperBound();
        droppable.push_back({ProdBounds(-raw_lb, c_scale).first, delta, item});
      } else {
        magnitude[item] =
            ProdBounds(std::max(std::abs(raw_lb), std::abs(raw_ub)), c_scale)
                .second;
      }
    } else if (std::isfinite(raw_lb)) {
      magnitude[item] = ProdBounds(std::abs(raw_lb), c_scale).second;
    } else if (std::isfinite(raw_ub)) {
      magnitude[item] = ProdBounds(std::abs(raw_ub), c_scale).second;
    }
  }

  // The vertex limit is `base_factor` times the sum of the `n` largest
  // magnitudes: Cramer's rule involves `k` active rows and `n - k` non-basic
  // variables, i.e. `n` distinct rows or columns in total.
  std::vector<double> sorted_magnitudes;
  const auto compute_vertex_limit = [&]() {
    sorted_magnitudes = magnitude;
    if (sorted_magnitudes.size() > num_cols) {
      std::nth_element(sorted_magnitudes.begin(),
                       sorted_magnitudes.begin() + num_cols,
                       sorted_magnitudes.end(), std::greater<double>());
      sorted_magnitudes.resize(num_cols);
    }
    TightSum sum;
    for (const double m : sorted_magnitudes) sum.Add(m);
    return base_factor * sum.TightUpperBound();
  };

  // 3. Sweep droppable half-spaces from smallest to largest `drop_limit`: any
  // half-space with `drop_limit` smaller than the current vertex limit
  // `scaled_bound_limit` could be hit by a vertex of `P_low` and must be kept,
  // which increases the limit. We iterate until all the remaining half-spaces
  // have a `drop_limit >= scaled_bound_limit` and can be dropped. Note that
  // keeping a half-space is always valid, so to bound the complexity we keep
  // all the remaining ones after a few rounds.
  std::sort(droppable.begin(), droppable.end());
  double scaled_bound_limit = compute_vertex_limit();
  constexpr int kMaxNumRounds = 20;
  for (int round = 0, next = 0; next < droppable.size(); ++round) {
    const int old_next = next;
    while (next < droppable.size() &&
           (round >= kMaxNumRounds ||
            droppable[next].drop_limit < scaled_bound_limit)) {
      if (!std::isfinite(droppable[next].delta)) return;
      add_to_magnitude(droppable[next].item, droppable[next].delta);
      ++next;
    }
    if (next == old_next) break;
    scaled_bound_limit = compute_vertex_limit();
  }
  if (!std::isfinite(scaled_bound_limit)) return;

  // 4. Apply the limit to all variables of the component.
  bool component_touched = false;
  for (int local_col = 0; local_col < compo_vars.size(); ++local_col) {
    const double limit = std::ceil(
        ExactScaledSumUb(scaled_bound_limit, 0.0, col_scale[local_col]));
    if (!std::isfinite(limit)) continue;
    MipVariableState& state = states[compo_vars[local_col]];
    const double lb = state.lb;
    const double ub = state.ub;
    if (lb < -limit || ub > limit) {
      component_touched = true;
      stats->old_min = std::min(stats->old_min, lb);
      stats->old_max = std::max(stats->old_max, ub);
      if (lb < -limit) {
        state.lb = -limit;
        ++stats->num_tightened_bounds;
      }
      if (ub > limit) {
        state.ub = limit;
        ++stats->num_tightened_bounds;
      }
      stats->new_min = std::min(stats->new_min, state.lb);
      stats->new_max = std::max(stats->new_max, state.ub);
    }
  }
  if (component_touched) {
    ++stats->num_bound_tightened_components;
  }
}

// Returns true if, for any integer assignment of the external variables, all
// the vertices of the component LP are integral (Hoffman-Kruskal). This
// requires the scaled matrix to be TUM, the column scaling to be unit so that
// scaled vertices are the actual variable values, and all variable bounds,
// scaled RHS and scaled external coefficients to be integer (so that the
// effective RHS `(b_r - A_{r, ext} z*) / row_scale[r]` is integer).
bool ComponentHasIntegralVertices(const MPModelProtoWrapper& mp_model,
                                  absl::Span<const MipVariableState> states,
                                  absl::Span<const int> compo_vars,
                                  absl::Span<const int> compo_rows,
                                  absl::Span<const double> col_scale,
                                  absl::Span<const double> row_scale,
                                  absl::Span<const int> var_to_local_col,
                                  ModularityStatus status, double tolerance) {
  if (status != ModularityStatus::kTotallyUnimodular) return false;
  const auto is_almost_integer = [tolerance](double x) {
    return !std::isfinite(x) || std::abs(x - std::round(x)) <= tolerance;
  };
  for (int local_col = 0; local_col < compo_vars.size(); ++local_col) {
    if (std::abs(col_scale[local_col] - 1.0) > tolerance) return false;
    const MipVariableState& state = states[compo_vars[local_col]];
    if (!is_almost_integer(state.lb) || !is_almost_integer(state.ub)) {
      return false;
    }
  }
  for (int local_row = 0; local_row < compo_rows.size(); ++local_row) {
    const MPConstraintProto& constraint =
        mp_model.constraints(compo_rows[local_row]).proto();
    const double r_scale = row_scale[local_row];
    if (!is_almost_integer(constraint.lower_bound() / r_scale) ||
        !is_almost_integer(constraint.upper_bound() / r_scale)) {
      return false;
    }
    for (int i = 0; i < constraint.var_index_size(); ++i) {
      const double coeff = constraint.coefficient(i);
      if (coeff == 0.0) continue;
      const int var = constraint.var_index(i);
      if (var_to_local_col[var] != -1) continue;
      if (!states[var].is_integer) return false;
      if (!is_almost_integer(coeff / r_scale)) return false;
    }
  }
  return true;
}

// Tries to make a TUM (`modular_factor = 1`) or 2-modular (`modular_factor =
// 2`) component integral with the rational factor `chosen_s`: each variable is
// scaled by `modular_factor * round(chosen_s * col_scale)` and its bounds are
// rounded to multiples of `modular_factor`, see RoundScaledLowerBound(). Each
// constraint is then scaled by `modular_factor * chosen_s / row_scale`, which
// gives +/-1 coefficients on the component variables and multiples of
// `modular_factor` on the external ones.
//
// Returns false, without changing `states`, if this does not hold, or if the
// scaled constraints could overflow or are not precise enough. Otherwise
// updates `states` and fills the scaling of each constraint.
bool TryScaleModularComponent(
    const SatParameters& params, const ResidualContinuousComponent& compo,
    const MPModelProtoWrapper& mp_model, absl::Span<const double> col_scale,
    absl::Span<const double> row_scale, int64_t modular_factor, double chosen_s,
    absl::Span<MipVariableState> states, TumStats* stats,
    std::vector<ModularConstraintScaling>* row_scalings) {
  const double kMaxVarBound =
      std::nextafter(static_cast<double>(kint64max / 2), 0.0);
  const int64_t scaling_target = int64_t{1}
                                 << params.mip_max_activity_exponent();
  const bool proper_relaxation = params.mip_proper_relaxation();
  const double wanted_tol = params.mip_wanted_precision();
  // Same tolerances as in ConstraintScaler::ScaleAndAddConstraint().
  const double bound_tol =
      params.mip_use_tight_bounds() ? params.mip_tight_precision() : wanted_tol;
  const double round_tol = proper_relaxation ? bound_tol : wanted_tol;
  const double m = static_cast<double>(modular_factor);

  // Scale the variables, the states are only updated on success.
  absl::Span<const int> compo_vars = compo.continuous_vars;
  std::vector<MipVariableState> new_states(compo_vars.size());
  double max_bound_error = 0.0;
  const auto is_too_large = [kMaxVarBound](double b) {
    return std::isfinite(b) && std::abs(b) > kMaxVarBound;
  };
  for (int local_col = 0; local_col < compo_vars.size(); ++local_col) {
    const MipVariableState& state = states[compo_vars[local_col]];
    const double s = std::round(chosen_s * col_scale[local_col]);
    if (!(s >= 1.0)) return false;
    const auto round_lb = [&](double lb) {
      if (!std::isfinite(lb)) return lb;
      const double w = RoundScaledLowerBound(ProdBounds(s, lb).first,
                                             s * round_tol, proper_relaxation);
      const double error =
          MaxAbsScaledTightSum(TightSum().Add(w).AddProd(-s, lb), s);
      if (error > bound_tol) max_bound_error = std::max(max_bound_error, error);
      return m * w;
    };
    MipVariableState& new_state = new_states[local_col];
    new_state = state;
    new_state.lb = round_lb(state.lb);
    new_state.ub = -round_lb(-state.ub);
    new_state.is_integer = true;
    new_state.scaling *= m * s;
    new_state.error_lb = 0.0;
    new_state.error_ub = 0.0;
    if (new_state.lb > new_state.ub || new_state.lb == kInfinity ||
        new_state.ub == -kInfinity || is_too_large(new_state.lb) ||
        is_too_large(new_state.ub) || !(new_state.scaling <= kMaxVarBound)) {
      return false;
    }
  }

  // Check the scaled constraints.
  std::vector<ModularConstraintScaling> scalings;
  std::vector<double> coeffs;
  std::vector<double> lbs;
  std::vector<double> ubs;
  for (int local_row = 0; local_row < compo.constraints.size(); ++local_row) {
    const EnforcedLinearConstraint& ct =
        mp_model.constraints(compo.constraints[local_row]);
    const double factor = m * chosen_s / row_scale[local_row];
    if (!(factor > 0.0 && std::isfinite(factor))) return false;
    coeffs.clear();
    lbs.clear();
    ubs.clear();
    LinearOverflowChecker checker;
    for (int i = 0; i < ct.num_terms(); ++i) {
      if (ct.coeffs(i) == 0.0) continue;
      const int local_col = compo.var_to_local_col[ct.vars(i)];
      const MipVariableState& state =
          local_col == -1 ? states[ct.vars(i)] : new_states[local_col];

      // The CP-SAT domain of the variable after the final shift, see
      // FinalCpSatShiftValue() and ConvertVariableToCpVariable(). Note that the
      // external variables are already shifted.
      const double shift =
          state.lb > 0.0 ? state.lb : (state.ub < 0.0 ? state.ub : 0.0);
      const double lb =
          std::ceil(state.lb - shift - std::max(wanted_tol, -state.error_lb));
      const double ub =
          std::floor(state.ub - shift + std::max(wanted_tol, state.error_ub));
      if (lb == 0.0 && ub == 0.0) continue;

      const double coeff = ct.coeffs(i) / state.scaling;
      const double scaled_coeff = coeff * factor;
      if (!(std::abs(scaled_coeff) < static_cast<double>(kint64max))) {
        return false;
      }
      const int64_t int_coeff = std::llround(scaled_coeff);
      if (local_col == -1 ? int_coeff % modular_factor != 0
                          : std::abs(int_coeff) != 1) {
        return false;
      }
      if (!std::isfinite(lb) || !std::isfinite(ub)) {
        // The coefficient of an unbounded variable must be exact.
        if (std::abs(scaled_coeff - static_cast<double>(int_coeff)) >
            1e-12 * std::max(1.0, std::abs(scaled_coeff))) {
          return false;
        }
        continue;
      }
      if (!checker.AddTerm(int_coeff, static_cast<int64_t>(lb),
                           static_cast<int64_t>(ub)) ||
          -checker.sum_min > scaling_target ||
          checker.sum_max > scaling_target) {
        return false;
      }
      coeffs.push_back(coeff);
      lbs.push_back(lb);
      ubs.push_back(ub);
    }
    double relative_coeff_error;
    double scaled_sum_error;
    ComputeScalingErrors(coeffs, lbs, ubs, factor, &relative_coeff_error,
                         &scaled_sum_error);
    if (scaled_sum_error > params.mip_check_precision() * factor) return false;
    scalings.push_back(
        {.scaling_factor = factor, .modular_factor = modular_factor});
  }

  for (int local_col = 0; local_col < compo_vars.size(); ++local_col) {
    states[compo_vars[local_col]] = new_states[local_col];
  }
  stats->num_modular_constraints += scalings.size();
  stats->max_var_bound_error =
      std::max(stats->max_var_bound_error, max_bound_error);
  *row_scalings = std::move(scalings);
  return true;
}

// Scales a continuous TUM component (with unit column scaling and finite
// non-zero max bound) by a common power-of-two factor when
// mip_scale_for_precision is enabled.
void ApplyTumCommonPrecisionScaling(
    const SatParameters& params, const ResidualContinuousComponent& compo,
    double compo_max_bound, const MPModelProtoWrapper& mp_model,
    absl::Span<MipVariableState> states,
    GlobalObjectivePrecision* global_obj_precision, TumStats* stats) {
  absl::Span<const int> compo_vars = compo.continuous_vars;
  // Use a higher target bound than mip_max_bound (1e7) so components with
  // large or clamped bounds can still be scaled together, while keeping scaled
  // continuous TUM domains within 1e11 so double-precision LP simplex
  // tolerances (1e-4 in CP space) remain accurate on unit-coefficient rows.
  constexpr double kTumPrecisionMaxBound = 1e11;
  const double max_domain_bound =
      std::min(std::max(params.mip_max_bound(), kTumPrecisionMaxBound),
               std::nextafter(static_cast<double>(kint64max / 2), 0.0));
  const int max_exp =
      MaxPowerOfTwoExponentWithinBound(compo_max_bound, max_domain_bound);
  if (max_exp <= 0) return;
  const double compo_max_scaling = std::ldexp(1.0, max_exp);

  const ComponentPrecisionScalingBounds bounds =
      ComputeComponentPrecisionScalingBounds(params, mp_model, compo, states,
                                             max_domain_bound,
                                             global_obj_precision);

  double compo_min_headroom = kInfinity;
  double compo_max_needed = 0.0;
  for (int local_col = 0; local_col < compo_vars.size(); ++local_col) {
    compo_min_headroom =
        std::min(compo_min_headroom, bounds.min_headroom[local_col]);
    compo_max_needed =
        std::max(compo_max_needed, bounds.max_scaling_needed[local_col]);
  }
  if (compo_min_headroom == kInfinity || compo_max_needed <= 1.0) return;

  const double needed_scaling =
      std::ldexp(1.0, LowestPowerOfTwoAtOrAbove(compo_max_needed));
  const double scaling =
      std::min({compo_max_scaling, compo_min_headroom, needed_scaling});
  if (scaling <= 1.0) return;

  for (const int var : compo_vars) {
    MipVariableState& state = states[var];
    CHECK_EQ(state.scaling, 1.0);
    ++stats->num_common_scaled;
    if (global_obj_precision != nullptr &&
        mp_model.variables(var).objective_coefficient() != 0.0 &&
        !(state.lb == 0.0 && state.ub == 0.0)) {
      global_obj_precision->NotifyContinuousPow2Scaling(scaling);
    }
    ScaleVariableState(scaling, &state);
  }
  stats->min_common_scaling = std::min(stats->min_common_scaling, scaling);
  stats->max_common_scaling = std::max(stats->max_common_scaling, scaling);
}

struct TumComponentScratch {
  struct NonIntegerValue {
    double value;
    double tol;
  };
  glop::CompactSparseMatrix row_matrix;
  glop::CompactSparseMatrix col_matrix;
  std::vector<double> col_scale;
  std::vector<double> row_scale;
  std::vector<NonIntegerValue> non_integer_values;
};

// Evaluates a single residual continuous component for total unimodularity /
// 2-modularity, tightens bounds, and (when `!only_tighten_bounds`) either
// makes its variables integral (filling `row_scalings`, see
// TryScaleModularComponent()) or returns true with
// `*common_precision_max_bound` set for `ApplyTumCommonPrecisionScaling()`.
//
// The matrix coefficients are read from `mp_model`, the variables are read
// from and written to `states`. Note that when `only_tighten_bounds` is true,
// `compo.continuous_vars` may also contain integer variables.
bool TryProcessTumComponent(
    const SatParameters& params, const ResidualContinuousComponent& compo,
    bool only_tighten_bounds, bool include_large_integers,
    const MPModelProtoWrapper& mp_model, absl::Span<MipVariableState> states,
    GlobalObjectivePrecision* global_obj_precision, TumStats* stats,
    TumComponentScratch* scratch, double* common_precision_max_bound,
    std::vector<ModularConstraintScaling>* row_scalings) {
  absl::Span<const int> compo_vars = compo.continuous_vars;
  absl::Span<const int> compo_rows = compo.constraints;
  absl::Span<const int> var_to_local_col = compo.var_to_local_col;
  CHECK(!var_to_local_col.empty());
  ++stats->num_components;
  const double tolerance = params.mip_wanted_precision();

  // Compute this component's row_matrix and col_matrix.
  scratch->row_matrix.Reset(RowIndex(compo_vars.size()));
  for (const int c : compo_rows) {
    const MPConstraintProto& constraint = mp_model.constraints(c).proto();
    for (int i = 0; i < constraint.var_index_size(); ++i) {
      const double coeff = constraint.coefficient(i);
      if (coeff == 0.0) continue;
      const int local_col = var_to_local_col[constraint.var_index(i)];
      if (local_col == -1) continue;
      scratch->row_matrix.AddEntryToCurrentColumn(RowIndex(local_col), coeff);
    }
    scratch->row_matrix.CloseCurrentColumn();
  }
  scratch->col_matrix.PopulateFromTranspose(scratch->row_matrix);

  // Can we scale it to only +1/-1?
  if (!ComputeRowAndColScales(scratch->col_matrix, scratch->row_matrix,
                              tolerance, &scratch->col_scale,
                              &scratch->row_scale)) {
    return false;
  }

  // Is the scaled matrix TUM or 2-modular? Note that positive row/column
  // scaling preserves both the sparsity pattern and entry signs, which is all
  // CheckComponentModularity inspects.
  const ModularityStatus status =
      CheckComponentModularity(scratch->col_matrix, scratch->row_matrix);
  if (status == ModularityStatus::kNotTwoModular) {
    return false;
  }

  // If the component contains integer variables, we can only tighten bounds
  // if all the vertices of the component LP are integral whatever the
  // (integer) value of the external variables. By Hoffman-Kruskal, this is
  // the case if the matrix is TUM with unit column scaling and if all the
  // variable bounds, scaled RHS and scaled external coefficients are integer.
  if (include_large_integers) {
    bool has_integer_vars = false;
    for (const int var : compo_vars) {
      if (states[var].is_integer) {
        has_integer_vars = true;
        break;
      }
    }
    if (has_integer_vars &&
        !ComponentHasIntegralVertices(mp_model, states, compo_vars, compo_rows,
                                      scratch->col_scale, scratch->row_scale,
                                      var_to_local_col, status, tolerance)) {
      return false;
    }
  }

  if (status == ModularityStatus::kTotallyUnimodular) {
    ++stats->num_tum_components;
  } else {
    ++stats->num_2_modular_components;
  }

  const double base_factor =
      status == ModularityStatus::kTwoModular ? 2.0 : 1.0;
  TightenComponentBounds(compo_vars, compo_rows, scratch->col_scale,
                         scratch->row_scale, var_to_local_col, base_factor,
                         mp_model, states, stats);
  if (only_tighten_bounds) return false;

  // Collect bounds and discrete coefficients in this component, pre-scaled
  // by `col_scale` and `1 / row_scale`.
  bool has_col_scaling = false;
  double max_bound = 0.0;
  scratch->non_integer_values.clear();
  auto record_value = [&](double raw_v, double v_tol) {
    if (!std::isfinite(raw_v)) return;
    const double v = std::abs(raw_v);
    if (std::abs(v - std::round(v)) > v_tol) {
      scratch->non_integer_values.push_back({v, v_tol});
    }
  };
  for (int local_col = 0; local_col < compo_vars.size(); ++local_col) {
    const int var = compo_vars[local_col];
    const double c_scale = scratch->col_scale[local_col];
    const double col_tol = tolerance * std::min(1.0, c_scale);
    if (std::abs(c_scale - 1.0) > col_tol) has_col_scaling = true;
    max_bound = std::max(max_bound, c_scale);
    record_value(c_scale, col_tol);

    const double lb = states[var].lb * c_scale;
    const double ub = states[var].ub * c_scale;
    if (std::isfinite(lb)) max_bound = std::max(max_bound, std::abs(lb));
    if (std::isfinite(ub)) max_bound = std::max(max_bound, std::abs(ub));
    record_value(lb, col_tol);
    record_value(ub, col_tol);
  }
  for (int local_row = 0; local_row < compo_rows.size(); ++local_row) {
    const int c = compo_rows[local_row];
    const MPConstraintProto& constraint = mp_model.constraints(c).proto();
    const double r_scale = scratch->row_scale[local_row];
    AccurateSum<double> ext_offset;
    for (int i = 0; i < constraint.var_index_size(); ++i) {
      const double coeff = constraint.coefficient(i);
      if (coeff == 0.0) continue;
      const int v = constraint.var_index(i);
      if (var_to_local_col[v] != -1) continue;
      const MipVariableState& ext_state = states[v];
      if (ext_state.shift != 0.0) ext_offset.Add(coeff * ext_state.shift);
      if (ext_state.lb == 0.0 && ext_state.ub == 0.0) continue;
      const double val = std::abs(coeff / ext_state.scaling) / r_scale;
      const double max_dom =
          std::max(std::abs(ext_state.lb), std::abs(ext_state.ub));
      const double ext_tol =
          std::isfinite(max_dom)
              ? std::min(tolerance, 0.5 * params.mip_check_precision() /
                                        std::max(1.0, r_scale * max_dom))
              : tolerance * std::min(1.0, val);
      record_value(val, ext_tol);
    }
    record_value((constraint.lower_bound() - ext_offset.Value()) / r_scale,
                 tolerance);
    record_value((constraint.upper_bound() - ext_offset.Value()) / r_scale,
                 tolerance);
  }

  // Determine whether we can mark variables as integer (possibly with a
  // rational scaling factor) or apply a common component power-of-two factor.
  const int64_t modular_factor =
      status == ModularityStatus::kTwoModular ? 2 : 1;
  const double max_s =
      params.mip_max_bound() / (base_factor * std::max(1.0, max_bound));
  bool mark_as_integer = false;

  double chosen_s = 1.0;
  if (scratch->non_integer_values.empty() && modular_factor == 1 &&
      !has_col_scaling) {
    mark_as_integer = true;
  } else if (params.mip_automatically_scale_variables() && max_s >= 1.0) {
    const int64_t int_limit =
        max_s >= static_cast<double>(kint64max)
            ? kint64max
            : CapAdd(static_cast<int64_t>(std::floor(max_s)), 1);
    double s = 1.0;
    for (const auto& [v, v_tol] : scratch->non_integer_values) {
      const int64_t f = FindRationalFactor(s * v, int_limit, s * v_tol);
      if (f == 0 || s * f > max_s) {
        s = 0.0;
        break;
      }
      s *= f;
    }
    if (s > 0.0) {
      chosen_s = s;
      mark_as_integer = true;
    }
  }

  if (mark_as_integer &&
      TryScaleModularComponent(params, compo, mp_model, scratch->col_scale,
                               scratch->row_scale, modular_factor, chosen_s,
                               states, stats, row_scalings)) {
    if (status == ModularityStatus::kTotallyUnimodular) {
      stats->num_new_tum_integers += compo_vars.size();
    } else {
      stats->num_new_2_modular_integers += compo_vars.size();
    }
    if (global_obj_precision != nullptr) {
      for (const int var : compo_vars) {
        if (mp_model.variables(var).objective_coefficient() != 0.0) {
          global_obj_precision->MarkDirty();
          break;
        }
      }
    }
    return false;
  }

  if (!has_col_scaling && params.mip_scale_for_precision()) {
    // Skip components with an infinite or zero bound.
    double compo_max_bound = 0.0;
    for (const int var : compo_vars) {
      compo_max_bound = std::max({compo_max_bound, std::abs(states[var].lb),
                                  std::abs(states[var].ub)});
    }
    if (compo_max_bound > 0.0 && std::isfinite(compo_max_bound)) {
      *common_precision_max_bound = compo_max_bound;
      return true;
    }
  }
  return false;
}

void LogTumBoundTighteningStats(const TumStats& stats, SolverLogger* logger) {
  if (stats.num_tightened_bounds > 0) {
    SOLVER_LOG(logger, "[TUM] Tightened ",
               FormatCounter(stats.num_tightened_bounds),
               " variable bounds across ", stats.num_bound_tightened_components,
               " components from [", stats.old_min, ", ", stats.old_max,
               "] to [", stats.new_min, ", ", stats.new_max, "].");
  }
}

}  // namespace

ResidualContinuousComponents SplitIntoResidualContinuousComponents(
    const MPModelProtoWrapper& mp_model) {
  ResidualContinuousComponents result;
  const int num_vars = mp_model.num_variables();
  std::vector<int> to_dense_index(num_vars, -1);
  std::vector<int> residual_var_indices;
  for (int var = 0; var < num_vars; ++var) {
    if (!mp_model.variables(var).is_integer()) {
      to_dense_index[var] = residual_var_indices.size();
      residual_var_indices.push_back(var);
    }
  }

  const int num_cols = residual_var_indices.size();
  DenseConnectedComponentsFinder cc;
  cc.SetNumberOfNodes(num_cols);

  const int num_constraints = mp_model.num_constraints();
  std::vector<int> row_first_col(num_constraints, -1);
  for (int c = 0; c < num_constraints; ++c) {
    const EnforcedLinearConstraint& ct = mp_model.constraints(c);
    if (ConstraintIsAlwaysTrue(ct.proto())) continue;
    int first_col = -1;
    for (int i = 0; i < ct.num_terms(); ++i) {
      if (ct.coeffs(i) == 0.0) continue;
      const int col = to_dense_index[ct.vars(i)];
      if (col == -1) continue;
      if (first_col == -1) {
        first_col = col;
      } else {
        cc.AddEdge(first_col, col);
      }
    }
    row_first_col[c] = first_col;
    if (first_col == -1) {
      result.pure_integer_constraints.push_back(c);
    }
  }

  const int num_components = cc.GetNumberOfComponents();
  const std::vector<int> col_to_component = cc.GetComponentIds();
  result.components.resize(num_components);
  for (int col = 0; col < num_cols; ++col) {
    result.components[col_to_component[col]].continuous_vars.push_back(
        residual_var_indices[col]);
  }
  for (int c = 0; c < num_constraints; ++c) {
    if (row_first_col[c] != -1) {
      result.components[col_to_component[row_first_col[c]]]
          .constraints.push_back(c);
    }
  }
  return result;
}

void LogTumStats(const TumStats& stats, SolverLogger* logger) {
  if (stats.num_tum_components + stats.num_2_modular_components == 0) {
    if (stats.num_components > 1) {
      SOLVER_LOG(logger, "[TUM] 0/", stats.num_components,
                 " continuous residual matrix with simple structure.");
    }
    LogTumBoundTighteningStats(stats, logger);
    return;
  }

  SOLVER_LOG(logger, "[TUM] ", stats.num_tum_components, "/",
             stats.num_components,
             " continuous residual matrix are Totally Unimodular; +",
             stats.num_new_tum_integers, " variables marked as integer.");
  if (stats.num_new_2_modular_integers > 0) {
    SOLVER_LOG(logger, "[TUM] ", stats.num_2_modular_components, "/",
               stats.num_components - stats.num_tum_components,
               " components left are 2-modular; +",
               stats.num_new_2_modular_integers,
               " variable scaled and marked as integer.");
  }
  if (stats.num_modular_constraints > 0) {
    SOLVER_LOG(logger, "[TUM] ", stats.num_modular_constraints,
               " modular constraints scaled directly (max bound error: ",
               stats.max_var_bound_error, ").");
  }
  if (stats.num_common_scaled > 0) {
    SOLVER_LOG(logger, "[TUM] ", stats.num_common_scaled,
               " variables scaled with a common component factor in [",
               stats.min_common_scaling, ", ", stats.max_common_scaling, "].");
  }
  LogTumBoundTighteningStats(stats, logger);
}

std::vector<ModularConstraintScaling> MarkComponentTotallyUnimodularAsInteger(
    const SatParameters& params, const ResidualContinuousComponent& compo,
    const MPModelProtoWrapper& mp_model, absl::Span<MipVariableState> states,
    GlobalObjectivePrecision* global_obj_precision, TumStats* stats) {
  DCHECK(mp_model.underlying_model().general_constraint().empty());
  CHECK_EQ(compo.var_to_local_col.size(), mp_model.num_variables());
  CHECK_EQ(states.size(), mp_model.num_variables());
  std::vector<ModularConstraintScaling> row_scalings;
  if (compo.continuous_vars.empty()) return row_scalings;
  if (params.keep_all_feasible_solutions_in_presolve()) return row_scalings;

  TumComponentScratch scratch;
  double common_precision_max_bound = 0.0;
  if (TryProcessTumComponent(params, compo, /*only_tighten_bounds=*/false,
                             /*include_large_integers=*/false, mp_model, states,
                             global_obj_precision, stats, &scratch,
                             &common_precision_max_bound, &row_scalings)) {
    ApplyTumCommonPrecisionScaling(params, compo, common_precision_max_bound,
                                   mp_model, states, global_obj_precision,
                                   stats);
  }
  return row_scalings;
}

void TightenResidualTotallyUnimodularVariableBounds(
    const SatParameters& params, MPModelProtoWrapper* mp_model,
    SolverLogger* logger) {
  if (!mp_model->underlying_model().general_constraint().empty()) return;
  if (params.keep_all_feasible_solutions_in_presolve()) return;

  const int num_vars = mp_model->num_variables();
  const int num_constraints = mp_model->num_constraints();
  std::vector<MipVariableState> states(num_vars);
  for (int var = 0; var < num_vars; ++var) {
    const MPVariableProto& mp_var = mp_model->variables(var);
    states[var].lb = mp_var.lower_bound();
    states[var].ub = mp_var.upper_bound();
    states[var].is_integer = mp_var.is_integer();
  }

  // Integer variables with a large domain (typically unbounded ones) are
  // first included in the residual matrix: this allows to tighten them when
  // they are part of an integral TUM component (e.g. big-M scheduling
  // formulations), and this also provides finite bounds for the external
  // variables of the second pass. The second pass only considers continuous
  // variables, since it can lead to different (smaller) components.
  //
  // In both passes, we only tighten the bounds of components whose LP vertices
  // are guaranteed to be integral for any integer assignment of the external
  // variables (Hoffman-Kruskal).
  TumStats stats;
  TumComponentScratch scratch;
  std::vector<int> to_dense_index(num_vars, -1);
  std::vector<int> residual_var_indices;
  std::vector<int> row_first_col(num_constraints, -1);
  std::vector<int> var_to_local_col_buffer(num_vars, -1);
  const double tighten_split = 0.1 * params.mip_max_bound();
  for (const bool include_large_integers : {true, false}) {
    // 1. Identify and re-index residual variables.
    //
    // Note: Rather than considering only integer variable as external, we also
    // mark as external any variable that have a "small" domain. This should
    // allow to split the problem into more component, increasing the chance
    // that some of them are TUM and allow to restrict their large bounds via
    // TightenComponentBounds().
    to_dense_index.assign(num_vars, -1);
    residual_var_indices.clear();
    for (int var = 0; var < num_vars; ++var) {
      const double lb = states[var].lb;
      const double ub = states[var].ub;
      if (std::abs(lb) <= tighten_split && std::abs(ub) <= tighten_split) {
        continue;
      }
      if (!include_large_integers && states[var].is_integer) continue;
      to_dense_index[var] = residual_var_indices.size();
      residual_var_indices.push_back(var);
    }
    if (residual_var_indices.empty()) continue;

    // 2. Partition residual variables and constraints into connected
    // components.
    const int num_cols = residual_var_indices.size();
    DenseConnectedComponentsFinder components;
    components.SetNumberOfNodes(num_cols);
    for (int c = 0; c < num_constraints; ++c) {
      const MPConstraintProto& constraint = mp_model->constraints(c).proto();
      int first_col = -1;
      for (int i = 0; i < constraint.var_index_size(); ++i) {
        if (constraint.coefficient(i) == 0.0) continue;
        const int col = to_dense_index[constraint.var_index(i)];
        if (col == -1) continue;  // Skip external variables.
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
    std::vector<ResidualContinuousComponent> compos(num_components);
    for (int col = 0; col < num_cols; ++col) {
      compos[col_to_components[col]].continuous_vars.push_back(
          residual_var_indices[col]);
    }
    for (int c = 0; c < num_constraints; ++c) {
      if (row_first_col[c] != -1) {
        compos[col_to_components[row_first_col[c]]].constraints.push_back(c);
      }
    }

    // 3. Process each connected component independently.
    for (ResidualContinuousComponent& compo : compos) {
      const ScopedVarToLocalCol scoped(&compo, &var_to_local_col_buffer);
      double unused_common_precision_max_bound = 0.0;
      TryProcessTumComponent(params, compo, /*only_tighten_bounds=*/true,
                             include_large_integers, *mp_model,
                             absl::MakeSpan(states),
                             /*global_obj_precision=*/nullptr, &stats, &scratch,
                             &unused_common_precision_max_bound,
                             /*row_scalings=*/nullptr);
    }
  }
  LogTumBoundTighteningStats(stats, logger);

  // Only the bounds can change.
  for (int var = 0; var < num_vars; ++var) {
    MPVariableProto* mp_var = mp_model->mutable_variables(var);
    mp_var->set_lower_bound(states[var].lb);
    mp_var->set_upper_bound(states[var].ub);
  }
}

}  // namespace sat
}  // namespace operations_research
