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
#include "ortools/base/types.h"
#include "ortools/graph/connected_components.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/mp_model_wrapper.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/fp_utils.h"
#include "ortools/util/full_precision_inequalities.h"
#include "ortools/util/logging.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {
namespace sat {

using glop::Fractional;
using glop::kInfinity;

using operations_research::MPConstraintProto;
using operations_research::MPModelProto;
using operations_research::MPVariableProto;

double RoundScaledLowerBound(double y, double tol, bool proper_relaxation) {
  if (!std::isfinite(y)) return y;
  const double r = std::round(y);
  const double w = proper_relaxation
                       ? (TightSum().Add(r).Add(-y) <= tol ? r : std::floor(y))
                       : (TightSum().Add(y).Add(-r) <= tol ? r : std::ceil(y));
  return w == 0.0 ? 0.0 : w;
}

namespace {

bool ConstraintIsAlwaysTrue(const MPConstraintProto& mp_constraint) {
  return mp_constraint.lower_bound() == -kInfinity &&
         mp_constraint.upper_bound() == kInfinity;
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

// Fills the given constraint transformed according to `states`, see the
// formula in MipVariableState. Terms with a zero coefficient are kept so that
// the result is aligned with `ct.vars()`. When `lb_upper` and `ub_lower` are
// non-null, also fills the opposite-side bounds of the shifted RHS (`[lb,
// lb_upper]` and `[ub_lower, ub]`).
void TransformConstraint(const MPConstraintProto& ct,
                         absl::Span<const MipVariableState> states,
                         std::vector<double>* coeffs, double* lb, double* ub,
                         double* lb_upper = nullptr,
                         double* ub_lower = nullptr) {
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
  if (has_offset && std::isfinite(ct.lower_bound())) {
    const auto [shifted_lb, shifted_lb_upper] =
        TightSum(neg_offset).Add(ct.lower_bound()).TightBounds();
    *lb = shifted_lb;
    if (lb_upper != nullptr) *lb_upper = shifted_lb_upper;
  } else {
    *lb = ct.lower_bound();
    if (lb_upper != nullptr) *lb_upper = ct.lower_bound();
  }
  if (has_offset && std::isfinite(ct.upper_bound())) {
    const auto [shifted_ub_lower, shifted_ub] =
        TightSum(neg_offset).Add(ct.upper_bound()).TightBounds();
    *ub = shifted_ub;
    if (ub_lower != nullptr) *ub_lower = shifted_ub_lower;
  } else {
    *ub = ct.upper_bound();
    if (ub_lower != nullptr) *ub_lower = ct.upper_bound();
  }
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

// Bounds [err_lb, err_ub] on (discretized_x - x) for a non-fixed continuous
// variable x, given its objective coefficient in the minimization form.
std::pair<double, double> GetContinuousVarDiscretizationErrorBounds(
    bool use_proper_relaxation, double min_objective_coeff) {
  if (!use_proper_relaxation) return {-1.0, 1.0};
  if (min_objective_coeff > 0.0) return {-1.0, 0.0};
  if (min_objective_coeff < 0.0) return {0.0, 1.0};
  return {-0.5, 0.5};
}

// Discretization error bounds and power-of-two scaling limits for a variable
// in `ResidualContinuousComponent::continuous_vars`.
struct ComponentVariableScaling {
  // Bounds on `(discretized_x - x)` before additional precision scaling.
  double err_lb = 0.0;
  double err_ub = 0.0;
  // Largest power of two keeping the variable domain within the valid CP-SAT
  // domain.
  double max_var_scaling = 1.0;
  // Scaling needed to reach the wanted precision on the activities of all the
  // constraints containing the variable.
  double max_scaling_needed = 0.0;
  // Largest power-of-two scaling that still allows all the constraints
  // containing the variable to be scaled to integers.
  double min_headroom = kInfinity;
  // Pre-existing power-of-two scaling factor (> 1 for common-scaled TUM
  // variables), stripped when evaluating constraint headroom.
  double existing_pow2_scaling = 1.0;
  // Worst-case constraint discretization error attributed to this variable,
  // weighted by the number of fractional terms on the active constraint side.
  double max_weighted_disc_err = 0.0;
  // Desired power-of-two scaling `min(max_var_scaling, needed_scaling)` before
  // applying constraint headroom caps.
  double target_scaling = 1.0;

  std::pair<double, double> TermDiscErrors(double coeff) const {
    if (coeff > 0.0) {
      return {std::max(0.0, -coeff * err_lb), std::max(0.0, coeff * err_ub)};
    }
    return {std::max(0.0, -coeff * err_ub), std::max(0.0, coeff * err_lb)};
  }
};

// An active (non-trivial) linear constraint of a residual continuous component,
// expressed with any pre-existing power-of-two component variable scaling
// stripped, along with its baseline integer-conversion scaling parameters.
struct ComponentConstraint {
  struct Term {
    int var;
    int col;  // index in compo.continuous_vars, or -1 for external integers
  };
  // The terms with a non-zero coefficient on a variable not fixed to zero.
  // `unscaled_expr` has one entry per term, in the same order.
  std::vector<Term> terms;
  BoundedLinearExpression unscaled_expr;
  // Whether the lower (resp. upper) bound of the constraint is not implied by
  // the variable domains.
  bool has_lb = false;
  bool has_ub = false;
  // Largest pre-existing power-of-two scaling over its component variables.
  double max_existing_pow2_scaling = 1.0;
  // Result of FindBestScalingAndComputeErrors() on `unscaled_expr`: the chosen
  // scaling factor (zero if there is none), the largest one that does not
  // overflow, and the worst-case activity error with the chosen one.
  double scaling_factor = 0.0;
  double max_scaling_factor = 0.0;
  double unscaled_err = 0.0;
  // Largest power of two by which all the component variables can be scaled
  // while still being able to scale the constraint by `scaling_factor`.
  double uniform_headroom = 1.0;

  int num_terms() const { return terms.size(); }
  void Clear() {
    terms.clear();
    unscaled_expr.Clear();
    has_lb = false;
    has_ub = false;
    max_existing_pow2_scaling = 1.0;
    scaling_factor = 0.0;
    max_scaling_factor = 0.0;
    unscaled_err = 0.0;
    uniform_headroom = 1.0;
  }
  // Fills `out` with `unscaled_expr` once the variable of term `i` is scaled by
  // `2^term_exponents[i]`.
  void GetScaledByTermExponents(absl::Span<const int> term_exponents,
                                BoundedLinearExpression* out) const {
    const int n = num_terms();
    out->coeffs.resize(n);
    out->var_lbs.resize(n);
    out->var_ubs.resize(n);
    for (int i = 0; i < n; ++i) {
      out->coeffs[i] = std::ldexp(unscaled_expr.coeffs[i], -term_exponents[i]);
      out->var_lbs[i] = std::ldexp(unscaled_expr.var_lbs[i], term_exponents[i]);
      out->var_ubs[i] = std::ldexp(unscaled_expr.var_ubs[i], term_exponents[i]);
    }
  }
  // Fills `out` with `unscaled_expr` once each component variable `col` is
  // scaled by `var_scaling[col]`.
  void GetScaledByVariableScalings(absl::Span<const double> var_scaling,
                                   BoundedLinearExpression* out) const {
    const int n = num_terms();
    out->coeffs.resize(n);
    out->var_lbs.resize(n);
    out->var_ubs.resize(n);
    for (int i = 0; i < n; ++i) {
      const int col = terms[i].col;
      const double s = col != -1 ? var_scaling[col] : 1.0;
      out->coeffs[i] = unscaled_expr.coeffs[i] / s;
      out->var_lbs[i] = unscaled_expr.var_lbs[i] * s;
      out->var_ubs[i] = unscaled_expr.var_ubs[i] * s;
    }
  }
};

// Returns the largest power of two by which all the variables of a constraint
// can be scaled while still being able to scale it by `scaling_factor`.
double UniformRowHeadroom(double scaling_factor, double max_scaling_factor,
                          double max_existing_pow2_scaling) {
  if (scaling_factor == 0.0 ||
      max_existing_pow2_scaling * scaling_factor > max_scaling_factor) {
    return 1.0;
  }
  return std::ldexp(
      1.0, HighestPowerOfTwoAtOrBelow(max_scaling_factor / scaling_factor));
}

// Returns the smallest power of two that is at least `max_scaling_needed`.
double NeededPowerOfTwoScaling(double max_scaling_needed) {
  if (max_scaling_needed <= 1.0) return 1.0;
  // std::frexp() leaves the exponent unspecified on a non-finite value.
  if (!std::isfinite(max_scaling_needed)) return kInfinity;
  const int exp = LowestPowerOfTwoAtOrAbove(max_scaling_needed);
  return exp >= 1024 ? kInfinity : std::ldexp(1.0, exp);
}

// Returns the power-of-two scaling of each variable: the needed one, capped by
// the variable domain and constraint headroom limits.
std::vector<double> SelectVariableScalings(
    absl::Span<const ComponentVariableScaling> variables) {
  const int num_vars = variables.size();
  std::vector<double> chosen(num_vars, 1.0);
  for (int col = 0; col < num_vars; ++col) {
    const ComponentVariableScaling& v = variables[col];
    if (v.max_var_scaling <= 1.0 || v.min_headroom == kInfinity ||
        v.max_scaling_needed <= 1.0) {
      continue;
    }
    const double s = std::min({v.max_var_scaling, v.min_headroom,
                               NeededPowerOfTwoScaling(v.max_scaling_needed)});
    if (s > 1.0) chosen[col] = s;
  }
  return chosen;
}

// Per-variable scaling metadata and (optionally) the active constraints of a
// residual continuous component.
struct ComponentPrecisionAnalysis {
  std::vector<ComponentVariableScaling> variables;
  std::vector<ComponentConstraint> constraints;
};

// Analyzes `compo` to compute uniform precision-scaling bounds and optionally
// retains its active unscaled constraints for per-term headroom refinement.
// Any pre-existing power-of-two variable scaling (e.g. from TUM common scaling)
// is stripped on each constraint so its denominator does not penalize other
// variables sharing that constraint.
ComponentPrecisionAnalysis AnalyzeComponentForPrecisionScaling(
    const SatParameters& params, const MPModelProtoWrapper& mp_model,
    const ResidualContinuousComponent& compo,
    absl::Span<const MipVariableState> states, double max_target_bound,
    bool keep_constraints) {
  absl::Span<const int> compo_vars = compo.continuous_vars;
  absl::Span<const int> var_to_local_col = compo.var_to_local_col;
  CHECK(!var_to_local_col.empty());
  CHECK_GT(params.mip_tight_precision(), 0.0);
  const double wanted_precision = params.mip_wanted_precision();
  const double bound_tol = params.mip_use_tight_bounds()
                               ? params.mip_tight_precision()
                               : wanted_precision;
  const int64_t scaling_target = int64_t{1}
                                 << params.mip_max_activity_exponent();
  const double max_domain_bound =
      std::min(max_target_bound,
               std::nextafter(static_cast<double>(kint64max / 2), 0.0));

  const int num_vars = compo_vars.size();
  ComponentPrecisionAnalysis analysis;
  analysis.variables.resize(num_vars);

  for (int col = 0; col < num_vars; ++col) {
    const int var = compo_vars[col];
    const MipVariableState& state = states[var];
    if (state.is_integer) continue;
    ComponentVariableScaling& v = analysis.variables[col];
    if (state.lb != 0.0 || state.ub != 0.0) {
      std::tie(v.err_lb, v.err_ub) = GetContinuousVarDiscretizationErrorBounds(
          params.mip_proper_relaxation(),
          mp_model.variables(var).objective_coefficient());
    }
    if (state.scaling != 1.0) {
      int exp = 0;
      if (state.scaling > 1.0 && std::isfinite(state.scaling) &&
          std::frexp(state.scaling, &exp) == 0.5) {
        v.existing_pow2_scaling = state.scaling;
      }
      continue;
    }
    if (!std::isfinite(state.lb) || !std::isfinite(state.ub)) continue;
    const double mag = std::max(std::abs(state.lb), std::abs(state.ub));
    if (mag == 0.0) continue;
    const int max_exp = MaxPowerOfTwoExponentWithinBound(mag, max_domain_bound);
    if (max_exp > 0) v.max_var_scaling = std::ldexp(1.0, max_exp);
  }

  // We only want `discretization_precision` on the constraint activity; if a
  // variable only appears with really small coefficients, we don't need to
  // scale it too much.
  const double discretization_precision = wanted_precision / 2.0;
  std::vector<double> transformed_coeffs;
  ComponentConstraint ct;
  for (const int c : compo.constraints) {
    const MPConstraintProto& mp_constraint = mp_model.constraints(c).proto();
    if (ConstraintIsAlwaysTrue(mp_constraint)) continue;
    double ct_lb;
    double ct_ub;
    TransformConstraint(mp_constraint, states, &transformed_coeffs, &ct_lb,
                        &ct_ub);
    ct_lb -= bound_tol;
    ct_ub += bound_tol;

    ct.Clear();
    double min_activity = 0.0;
    double max_activity = 0.0;
    bool has_scalable_var = false;
    for (int i = 0; i < mp_constraint.var_index_size(); ++i) {
      const int var = mp_constraint.var_index(i);
      const double lb = states[var].lb;
      const double ub = states[var].ub;
      if (lb == 0.0 && ub == 0.0) continue;
      const double coeff = transformed_coeffs[i];
      if (coeff == 0.0) continue;

      const int col = var_to_local_col[var];
      double s = 1.0;
      if (col != -1) {
        const ComponentVariableScaling& v = analysis.variables[col];
        if (v.max_var_scaling > 1.0) has_scalable_var = true;
        s = v.existing_pow2_scaling;
      }
      ct.max_existing_pow2_scaling = std::max(ct.max_existing_pow2_scaling, s);
      ct.terms.push_back({.var = var, .col = col});
      ct.unscaled_expr.AddTerm(coeff * s, lb / s, ub / s);

      double min_term = coeff * lb;
      double max_term = coeff * ub;
      if (min_term > max_term) std::swap(min_term, max_term);
      min_activity += min_term;
      max_activity += max_term;
    }
    ct.has_lb = min_activity < ct_lb;
    ct.has_ub = max_activity > ct_ub;
    if (!has_scalable_var || (!ct.has_lb && !ct.has_ub)) continue;

    double rel_err = 0.0;
    double sum_err = 0.0;
    ct.scaling_factor = FindBestScalingAndComputeErrors(
        ct.unscaled_expr, scaling_target, wanted_precision, &rel_err, &sum_err,
        &ct.max_scaling_factor);
    ct.uniform_headroom = UniformRowHeadroom(
        ct.scaling_factor, ct.max_scaling_factor, ct.max_existing_pow2_scaling);
    for (const ComponentConstraint::Term& term : ct.terms) {
      if (term.col == -1) continue;
      ComponentVariableScaling& v = analysis.variables[term.col];
      v.min_headroom = std::min(v.min_headroom, ct.uniform_headroom);
    }
    if (ct.scaling_factor != 0.0) {
      ct.unscaled_err = sum_err / ct.scaling_factor;
      int num_lb_floats = 0;
      int num_ub_floats = 0;
      for (int i = 0; i < ct.num_terms(); ++i) {
        const int col = ct.terms[i].col;
        if (col == -1) continue;
        const auto [lb_err, ub_err] =
            analysis.variables[col].TermDiscErrors(ct.unscaled_expr.coeffs[i]);
        if (ct.has_lb && lb_err > 0.0) ++num_lb_floats;
        if (ct.has_ub && ub_err > 0.0) ++num_ub_floats;
      }
      for (int i = 0; i < ct.num_terms(); ++i) {
        const int col = ct.terms[i].col;
        if (col == -1) continue;
        ComponentVariableScaling& v = analysis.variables[col];
        if (v.max_var_scaling <= 1.0) continue;
        const auto [lb_err, ub_err] =
            v.TermDiscErrors(ct.unscaled_expr.coeffs[i]);
        const double max_weighted_err = std::max(
            ct.has_lb ? static_cast<double>(num_lb_floats) * lb_err : 0.0,
            ct.has_ub ? static_cast<double>(num_ub_floats) * ub_err : 0.0);
        v.max_weighted_disc_err =
            std::max(v.max_weighted_disc_err, max_weighted_err);
        v.max_scaling_needed = std::max(
            v.max_scaling_needed, max_weighted_err / discretization_precision);
      }
    }
    if (keep_constraints) {
      analysis.constraints.push_back(std::move(ct));
      ct = ComponentConstraint();
    }
  }
  return analysis;
}

// Returns true if `scaled_expr` can be scaled to integers within
// `ct.max_scaling_factor` without degrading `ct`'s rounding precision or
// collapsing any non-zero rounded coefficient to zero.
bool PreservesConstraintScalingPrecision(
    const ComponentConstraint& ct, const BoundedLinearExpression& scaled_expr,
    int64_t scaling_target, double wanted_precision, double max_err_ratio) {
  double rel_err = 0.0;
  double sum_err = 0.0;
  double test_sf = FindBestScalingAndComputeErrors(
      scaled_expr, scaling_target, wanted_precision, &rel_err, &sum_err);
  if (test_sf <= 0.0) return false;
  if (test_sf > ct.max_scaling_factor) {
    test_sf = ct.max_scaling_factor;
    ComputeScalingErrors(scaled_expr, test_sf, &rel_err, &sum_err);
  }
  const double test_err = sum_err / test_sf;
  if (ct.unscaled_err < wanted_precision
          ? test_err >= wanted_precision
          : test_err > max_err_ratio * ct.unscaled_err) {
    return false;
  }
  for (int i = 0; i < ct.num_terms(); ++i) {
    if (std::round(ct.scaling_factor * ct.unscaled_expr.coeffs[i]) != 0.0 &&
        std::round(test_sf * scaled_expr.coeffs[i]) == 0.0) {
      return false;
    }
  }
  return true;
}

// Updates `variables[col].min_headroom` for each component variable in `ct`
// using its own coefficient and domain bounds rather than the uniform
// constraint ratio `max_scaling_factor / scaling_factor`, while guaranteeing:
// 1. Under the constraint's odd rational factor `sf_q_odd`, no variable gets
//    less headroom than `uniform_exp`.
// 2. Pure power-of-two row scaling (`q_odd = 1`) is only selected over
//    `sf_q_odd > 1` when it reduces worst-case discretization error.
// 3. Non-uniform proposals are verified via
//    `PreservesConstraintScalingPrecision()`.
void UpdatePerTermConstraintHeadroom(
    const ComponentConstraint& ct, int64_t scaling_target,
    double wanted_precision, BoundedLinearExpression* scratch_expr,
    absl::Span<ComponentVariableScaling> variables) {
  if (ct.scaling_factor == 0.0 ||
      ct.max_existing_pow2_scaling * ct.scaling_factor >
          ct.max_scaling_factor) {
    for (const ComponentConstraint::Term& term : ct.terms) {
      if (term.col != -1) variables[term.col].min_headroom = 1.0;
    }
    return;
  }

  const int uniform_exp =
      HighestPowerOfTwoAtOrBelow(ct.max_scaling_factor / ct.scaling_factor);
  const int num_terms = ct.num_terms();
  std::vector<int> hi_exp(num_terms, 0);
  std::vector<int> chosen_exp(num_terms, 0);
  bool any_scalable = false;
  bool any_capped_by_uniform = false;
  for (int i = 0; i < num_terms; ++i) {
    const int col = ct.terms[i].col;
    if (col == -1 || variables[col].target_scaling <= 1.0) continue;
    any_scalable = true;
    int hi = 0;
    const double mantissa = std::frexp(variables[col].target_scaling, &hi);
    DCHECK_EQ(mantissa, 0.5);
    hi_exp[i] = --hi;
    chosen_exp[i] = std::min(hi, uniform_exp);
    if (chosen_exp[i] < hi) any_capped_by_uniform = true;
  }
  if (!any_scalable) return;
  if (!any_capped_by_uniform) {
    for (int i = 0; i < num_terms; ++i) {
      if (hi_exp[i] > 0) {
        const int col = ct.terms[i].col;
        variables[col].min_headroom = std::min(variables[col].min_headroom,
                                               std::ldexp(1.0, chosen_exp[i]));
      }
    }
    return;
  }

  int sf_exp = 0;
  const uint64_t sf_sig = static_cast<uint64_t>(
      std::ldexp(std::frexp(ct.scaling_factor, &sf_exp), 53));
  DCHECK_GT(sf_sig, 0);
  const double sf_q_odd =
      static_cast<double>(sf_sig >> std::countr_zero(sf_sig));

  int max_sf_exp = 0;
  const double max_sf_mantissa = std::frexp(ct.max_scaling_factor, &max_sf_exp);
  DCHECK_EQ(max_sf_mantissa, 0.5);
  --max_sf_exp;

  auto advance_exponents = [&](double q_odd, int base_uniform_exp,
                               std::vector<int>* out_exp) -> bool {
    const double rational_limit = std::min(ct.max_scaling_factor, 1e15);
    if (q_odd > 1.0 && rational_limit < q_odd) return false;
    const int p_max = q_odd == 1.0
                          ? max_sf_exp
                          : HighestPowerOfTwoAtOrBelow(rational_limit / q_odd);
    const double f_star = std::ldexp(q_odd, p_max);

    out_exp->assign(num_terms, 0);
    for (int i = 0; i < num_terms; ++i) {
      if (hi_exp[i] > 0) (*out_exp)[i] = std::min(hi_exp[i], base_uniform_exp);
    }
    ct.GetScaledByTermExponents(*out_exp, scratch_expr);

    double unused_rel = 0.0;
    double base_sum_err = 0.0;
    ComputeScalingErrors(*scratch_expr, f_star, &unused_rel, &base_sum_err);
    const double base_err = base_sum_err / f_star;

    auto term_rounding_error = [&](int idx, int exp_s) -> double {
      const double c = std::abs(ct.unscaled_expr.coeffs[idx]);
      const double mag = std::max(std::abs(ct.unscaled_expr.var_lbs[idx]),
                                  std::abs(ct.unscaled_expr.var_ubs[idx]));
      if (c == 0.0 || mag == 0.0) return 0.0;
      if (ct.unscaled_err >= wanted_precision && std::ldexp(c, -exp_s) < 1e-8) {
        return kInfinity;
      }
      const double kc = std::ldexp(q_odd * c, p_max - exp_s);
      if (!std::isfinite(kc)) return 0.0;
      if (kc < 0.5) return c * mag;
      const double err = std::abs(std::round(kc) - kc);
      if (err == 0.0) return 0.0;
      return std::min(c * mag, std::ldexp(err * mag / q_odd, exp_s - p_max));
    };

    // Exact/dyadic terms consume zero rounding budget.
    for (int i = 0; i < num_terms; ++i) {
      while ((*out_exp)[i] < hi_exp[i] &&
             term_rounding_error(i, (*out_exp)[i] + 1) == 0.0) {
        ++(*out_exp)[i];
      }
    }

    const double remaining_budget =
        std::max(0.0, 0.99 * wanted_precision - base_err);
    if (remaining_budget <= 0.0) return true;

    int num_consumers = 0;
    for (int i = 0; i < num_terms; ++i) {
      if ((*out_exp)[i] < hi_exp[i] &&
          term_rounding_error(i, (*out_exp)[i] + 1) <= remaining_budget) {
        ++num_consumers;
      }
    }
    if (num_consumers == 0) return true;

    const double tol = remaining_budget / num_consumers;
    double used_saturated = 0.0;
    int num_unsaturated = 0;
    for (int i = 0; i < num_terms; ++i) {
      if ((*out_exp)[i] >= hi_exp[i] ||
          term_rounding_error(i, (*out_exp)[i] + 1) > remaining_budget) {
        continue;
      }
      while ((*out_exp)[i] < hi_exp[i] &&
             term_rounding_error(i, (*out_exp)[i] + 1) <= tol) {
        ++(*out_exp)[i];
      }
      if ((*out_exp)[i] == hi_exp[i]) {
        used_saturated += term_rounding_error(i, (*out_exp)[i]);
      } else {
        ++num_unsaturated;
      }
    }
    if (num_unsaturated > 0 && num_unsaturated < num_consumers &&
        remaining_budget > used_saturated) {
      const double refined_tol =
          (remaining_budget - used_saturated) / num_unsaturated;
      for (int i = 0; i < num_terms; ++i) {
        while ((*out_exp)[i] < hi_exp[i] &&
               term_rounding_error(i, (*out_exp)[i] + 1) <= refined_tol) {
          ++(*out_exp)[i];
        }
      }
    }
    return true;
  };

  auto verify_exponents = [&](absl::Span<const int> cand) {
    ct.GetScaledByTermExponents(cand, scratch_expr);
    return PreservesConstraintScalingPrecision(ct, *scratch_expr,
                                               scaling_target, wanted_precision,
                                               /*max_err_ratio=*/1.01);
  };

  std::vector<int> cand_exp;
  if (advance_exponents(sf_q_odd, uniform_exp, &cand_exp) &&
      cand_exp != chosen_exp && verify_exponents(cand_exp)) {
    chosen_exp = cand_exp;
  }

  // If `FindOddRationalScaling()` picked `sf_q_odd > 1` even though pure
  // power-of-two scaling at `max_scaling_factor` already meets
  // `wanted_precision`, evaluate `q_odd = 1` as well.
  if (sf_q_odd > 1.0) {
    double pow2_rel_err = 0.0;
    double pow2_sum_err = 0.0;
    ComputeScalingErrors(ct.unscaled_expr, ct.max_scaling_factor, &pow2_rel_err,
                         &pow2_sum_err);
    if (pow2_sum_err < 0.99 * wanted_precision * ct.max_scaling_factor) {
      double pow2_sf = std::min(ct.max_scaling_factor, 1.0);
      for (; pow2_sf < ct.max_scaling_factor; pow2_sf *= 2.0) {
        double rel = 0.0;
        double sum = 0.0;
        ComputeScalingErrors(ct.unscaled_expr, pow2_sf, &rel, &sum);
        if (sum < wanted_precision * pow2_sf) break;
      }
      if (ct.max_existing_pow2_scaling * pow2_sf <= ct.max_scaling_factor &&
          advance_exponents(
              /*q_odd=*/1.0,
              HighestPowerOfTwoAtOrBelow(ct.max_scaling_factor / pow2_sf),
              &cand_exp) &&
          cand_exp != chosen_exp) {
        auto eval_disc_cost = [&](absl::Span<const int> e) {
          double row_lb = 0.0;
          double row_ub = 0.0;
          double comp_worst = 0.0;
          for (int i = 0; i < num_terms; ++i) {
            const int col = ct.terms[i].col;
            if (col == -1) continue;
            const auto [lb_err, ub_err] =
                variables[col].TermDiscErrors(ct.unscaled_expr.coeffs[i]);
            if (ct.has_lb) row_lb += std::ldexp(lb_err, -e[i]);
            if (ct.has_ub) row_ub += std::ldexp(ub_err, -e[i]);
            if (hi_exp[i] > 0) {
              comp_worst = std::max(
                  comp_worst,
                  std::ldexp(variables[col].max_weighted_disc_err, -e[i]));
            }
          }
          return std::max({row_lb, row_ub, comp_worst});
        };
        const double cost_chosen = eval_disc_cost(chosen_exp);
        const double cost_cand = eval_disc_cost(cand_exp);
        int64_t exp_diff = 0;
        for (int i = 0; i < num_terms; ++i) {
          exp_diff += cand_exp[i] - chosen_exp[i];
        }
        const double tol = 1e-15 * std::max(1.0, cost_chosen);
        if ((cost_cand + tol < cost_chosen ||
             (std::abs(cost_cand - cost_chosen) <= tol && exp_diff > 0)) &&
            verify_exponents(cand_exp)) {
          chosen_exp = std::move(cand_exp);
        }
      }
    }
  }

  for (int i = 0; i < num_terms; ++i) {
    if (hi_exp[i] > 0) {
      const int col = ct.terms[i].col;
      variables[col].min_headroom =
          std::min(variables[col].min_headroom, std::ldexp(1.0, chosen_exp[i]));
    }
  }
}

// Returns the domain that the CP-SAT variable of `var` would get if it was
// scaled by `extra_scaling`: this applies the same final shift as
// FinalCpSatShiftValue() and the same rounding as
// ConvertVariableToCpVariable(), using `tmp_cp_var` as scratch memory. Returns
// the scaled bounds if they are too large to be converted, and {0, 0} if the
// conversion fails.
std::pair<double, double> DiscretizedDomainBounds(
    const SatParameters& params, int var, double obj_coeff,
    MipVariableState state, double extra_scaling,
    IntegerVariableProto* tmp_cp_var) {
  const double kMaxVariableBound =
      std::nextafter(static_cast<double>(kint64max / 2), 0.0);
  ScaleVariableState(extra_scaling, &state);
  if (!(state.lb >= -kMaxVariableBound && state.ub <= kMaxVariableBound)) {
    return {state.lb, state.ub};
  }
  const double tol = params.mip_wanted_precision();
  double shift = 0.0;
  if (!state.is_integer && std::floor(state.ub) < std::ceil(state.lb)) {
    shift = state.lb > 0.0 ? state.lb : (state.ub < 0.0 ? state.ub : 0.0);
  } else if (state.lb > tol) {
    shift = std::ceil(state.lb);
  } else if (state.ub < -tol) {
    shift = std::floor(state.ub);
  }
  if (shift != 0.0) {
    state.lb = ExactSumLb(state.lb, -shift);
    state.ub = ExactSumUb(state.ub, -shift);
  }
  tmp_cp_var->Clear();
  if (!ConvertVariableToCpVariable(params, var, obj_coeff, &state, tmp_cp_var,
                                   /*num_small_domains=*/nullptr)) {
    return {0.0, 0.0};
  }
  return {static_cast<double>(tmp_cp_var->domain(0)),
          static_cast<double>(tmp_cp_var->domain(1))};
}

// Returns the power-of-two scaling to apply to each variable of
// `compo.continuous_vars`, see
// ScaleComponentContinuousVariablesForGivenPrecision().
std::vector<double> ComputePrecisionScalings(
    const SatParameters& params, const MPModelProtoWrapper& mp_model,
    const ResidualContinuousComponent& compo,
    absl::Span<const MipVariableState> states,
    PrecisionHeadroomMode headroom_mode) {
  const bool per_term = headroom_mode == PrecisionHeadroomMode::kPerTerm;
  ComponentPrecisionAnalysis analysis = AnalyzeComponentForPrecisionScaling(
      params, mp_model, compo, states, kInfinity,
      /*keep_constraints=*/per_term);
  if (!per_term) return SelectVariableScalings(analysis.variables);

  const int num_vars = analysis.variables.size();
  bool any_scalable = false;
  bool any_capped_by_uniform = false;
  for (ComponentVariableScaling& v : analysis.variables) {
    if (v.max_var_scaling <= 1.0 || v.max_scaling_needed <= 1.0) continue;
    v.target_scaling = std::min(v.max_var_scaling,
                                NeededPowerOfTwoScaling(v.max_scaling_needed));
    any_scalable = true;
    if (v.target_scaling > v.min_headroom) any_capped_by_uniform = true;
  }
  if (!any_scalable) return std::vector<double>(num_vars, 1.0);

  const int64_t scaling_target = int64_t{1}
                                 << params.mip_max_activity_exponent();
  const double wanted_precision = params.mip_wanted_precision();
  BoundedLinearExpression scratch_expr;

  // The per-term headroom can only help if some variable is limited by the
  // uniform one.
  if (any_capped_by_uniform) {
    for (ComponentVariableScaling& v : analysis.variables) {
      v.min_headroom = kInfinity;
    }
    for (const ComponentConstraint& ct : analysis.constraints) {
      UpdatePerTermConstraintHeadroom(ct, scaling_target, wanted_precision,
                                      &scratch_expr,
                                      absl::MakeSpan(analysis.variables));
    }
  }

  std::vector<double> chosen_scaling =
      SelectVariableScalings(analysis.variables);
  bool any_scaled = false;
  for (const double s : chosen_scaling) {
    if (s > 1.0) any_scaled = true;
  }
  if (!any_scaled) return chosen_scaling;

  // Post-pass verification across constraints for (1) 1-bit outward endpoint
  // snapping overflow in ConvertVariableToCpVariable() and (2) cross-row
  // `min_headroom` combining scalings from different constraints.
  IntegerVariableProto tmp_cp_var;
  for (int pass = 0; pass < 2; ++pass) {
    bool changed = false;
    for (ComponentConstraint& ct : analysis.constraints) {
      if (ct.scaling_factor == 0.0) continue;
      bool row_has_scaled_var = false;
      for (const ComponentConstraint::Term& term : ct.terms) {
        if (term.col != -1 && chosen_scaling[term.col] > 1.0) {
          row_has_scaled_var = true;
        }
      }
      if (!row_has_scaled_var) continue;

      scratch_expr.Clear();
      for (int i = 0; i < ct.num_terms(); ++i) {
        const auto [var, col] = ct.terms[i];
        const double extra_s = col != -1 ? chosen_scaling[col] : 1.0;
        const auto [d_min, d_max] = DiscretizedDomainBounds(
            params, var, mp_model.variables(var).objective_coefficient(),
            states[var], extra_s, &tmp_cp_var);
        if (d_min == 0.0 && d_max == 0.0) continue;
        const double exist_s =
            col != -1 ? analysis.variables[col].existing_pow2_scaling : 1.0;
        scratch_expr.AddTerm(ct.unscaled_expr.coeffs[i] / (exist_s * extra_s),
                             d_min, d_max);
      }
      const double actual_max_sf =
          scratch_expr.coeffs.empty()
              ? ct.max_scaling_factor
              : GetBestScalingOfDoublesToInt64(
                    scratch_expr.coeffs, scratch_expr.var_lbs,
                    scratch_expr.var_ubs, scaling_target);
      if (actual_max_sf > 0.0 && actual_max_sf < ct.max_scaling_factor) {
        ct.max_scaling_factor = actual_max_sf;
        ct.uniform_headroom =
            UniformRowHeadroom(ct.scaling_factor, ct.max_scaling_factor,
                               ct.max_existing_pow2_scaling);
        UpdatePerTermConstraintHeadroom(ct, scaling_target, wanted_precision,
                                        &scratch_expr,
                                        absl::MakeSpan(analysis.variables));
        for (const ComponentConstraint::Term& term : ct.terms) {
          const int col = term.col;
          if (col != -1 &&
              chosen_scaling[col] > analysis.variables[col].min_headroom) {
            chosen_scaling[col] = analysis.variables[col].min_headroom;
            changed = true;
          }
        }
      }

      bool any_above_uniform = false;
      for (const ComponentConstraint::Term& term : ct.terms) {
        if (term.col != -1 && chosen_scaling[term.col] > ct.uniform_headroom) {
          any_above_uniform = true;
        }
      }
      if (any_above_uniform) {
        ct.GetScaledByVariableScalings(chosen_scaling, &scratch_expr);
        if (!PreservesConstraintScalingPrecision(
                ct, scratch_expr, scaling_target, wanted_precision,
                /*max_err_ratio=*/1.05)) {
          for (const ComponentConstraint::Term& term : ct.terms) {
            if (term.col != -1 &&
                chosen_scaling[term.col] > ct.uniform_headroom) {
              chosen_scaling[term.col] = ct.uniform_headroom;
              changed = true;
            }
          }
        }
      }
    }
    if (!changed) break;
  }

  return chosen_scaling;
}

}  // namespace

ComponentPrecisionScalingBounds ComputeComponentPrecisionScalingBounds(
    const SatParameters& params, const MPModelProtoWrapper& mp_model,
    const ResidualContinuousComponent& compo,
    absl::Span<const MipVariableState> states, double max_target_bound) {
  const ComponentPrecisionAnalysis analysis =
      AnalyzeComponentForPrecisionScaling(params, mp_model, compo, states,
                                          max_target_bound,
                                          /*keep_constraints=*/false);
  const int num_vars = analysis.variables.size();
  ComponentPrecisionScalingBounds bounds;
  bounds.max_var_scaling.resize(num_vars);
  bounds.max_scaling_needed.resize(num_vars);
  bounds.min_headroom.resize(num_vars);
  for (int col = 0; col < num_vars; ++col) {
    bounds.max_var_scaling[col] = analysis.variables[col].max_var_scaling;
    bounds.max_scaling_needed[col] = analysis.variables[col].max_scaling_needed;
    bounds.min_headroom[col] = analysis.variables[col].min_headroom;
  }
  return bounds;
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
             stats.min_exponent, ",", stats.max_exponent,
             "]. Worst case objective discretization error: ",
             objective_discretization_error);
}

void ScaleComponentContinuousVariablesForGivenPrecision(
    const SatParameters& params, const ResidualContinuousComponent& compo,
    const MPModelProtoWrapper& mp_model, absl::Span<MipVariableState> states,
    PrecisionScalingStats* stats, PrecisionHeadroomMode headroom_mode) {
  const std::vector<double> chosen_scaling =
      ComputePrecisionScalings(params, mp_model, compo, states, headroom_mode);

  absl::Span<const int> compo_vars = compo.continuous_vars;
  const int local_num_vars = compo_vars.size();
  for (int local_col = 0; local_col < local_num_vars; ++local_col) {
    const double scaling = chosen_scaling[local_col];
    if (scaling <= 1.0) continue;

    int exp;
    const double mantissa = std::frexp(scaling, &exp);
    DCHECK_EQ(mantissa, 0.5);
    --exp;

    ++stats->num_scaled;
    stats->min_exponent = std::min(stats->min_exponent, exp);
    stats->max_exponent = std::max(stats->max_exponent, exp);

    const int var = compo_vars[local_col];
    MipVariableState& state = states[var];
    CHECK_EQ(state.scaling, 1.0);
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
      std::isfinite(*lb) ? std::ceil(ExactSumLb(*lb, -lb_tol)) : *lb;
  if (*lb != new_lb) {
    ++*num_changes;
    *lb = new_lb;
  }
  const double new_ub =
      std::isfinite(*ub) ? std::floor(ExactSumUb(*ub, ub_tol)) : *ub;
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
            ExactSumLb(min_left_activity[i], min_right_activity[i]);
      } else {
        min_activity_of_others[i] =
            min_left_activity[i] + min_right_activity[i];
      }
    }
    if constexpr (fill_max) {
      if constexpr (exact) {
        max_activity_of_others[i] =
            ExactSumUb(max_left_activity[i], max_right_activity[i]);
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
// (integer) variables are read from `states`. Returns `std::nullopt` if a
// finite bound cannot be established (overflow or unbounded external term with
// non-zero remainder).
std::optional<std::pair<double, double>>
TryComputeSingleVarConstraintErrorBounds(
    const LinearRow& row, absl::Span<const MipVariableState> states, int var,
    double scaling, double rhs_lb, double rhs_ub) {
  const double var_coeff = GetVarCoeff(row, var);
  if (var_coeff == 0.0) return std::make_pair(0.0, 0.0);
  if (!std::isfinite(rhs_lb) || !std::isfinite(rhs_ub)) return std::nullopt;

  // Accumulate bounds on `var_coeff * (discretized_var - scaling * var)`, then
  // divide by `var_coeff` once at the end.
  const double r_b =
      std::round((scaling / var_coeff) * (0.5 * rhs_lb + 0.5 * rhs_ub));
  if (!std::isfinite(r_b)) return std::nullopt;
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
    if (!std::isfinite(k_j)) return std::nullopt;
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
    if (!std::isfinite(l_j) || !std::isfinite(u_j)) return std::nullopt;
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
  return std::make_pair(std::min(0.0, err_lb), std::max(0.0, err_ub));
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
        TryComputeSingleVarConstraintErrorBounds(
            best_row, states, var, best_scaling, best_row.lb, best_row.ub)
            .value_or(std::make_pair(-0.5, 0.5));
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

void ScaleOneVariableResidualMatrix(const SatParameters& params,
                                    const ResidualContinuousComponent& compo,
                                    const MPModelProtoWrapper& mp_model,
                                    absl::Span<MipVariableState> states,
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
  const double bound_tol = params.mip_tight_precision();

  // Transform the rows in the current variable space.
  const int num_rows = compo.constraints.size();
  std::vector<std::vector<double>> row_coeffs(num_rows);
  std::vector<LinearRow> rows(num_rows);
  std::vector<double> row_lb_upper(num_rows);
  std::vector<double> row_ub_lower(num_rows);
  for (int r = 0; r < num_rows; ++r) {
    const EnforcedLinearConstraint& ct =
        mp_model.constraints(compo.constraints[r]);
    TransformConstraint(ct.proto(), states, &row_coeffs[r], &rows[r].lb,
                        &rows[r].ub, &row_lb_upper[r], &row_ub_lower[r]);
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

  const auto compute_max_row_err = [&](double s, bool snap_ge,
                                       bool include_bound_tol) {
    double err = 0.0;
    for (int r = 0; r < num_rows; ++r) {
      const LinearRow& row = rows[r];
      const double var_coeff = GetVarCoeff(row, var);
      if (var_coeff == 0.0) continue;
      const MPConstraintProto& ct =
          mp_model.constraints(compo.constraints[r]).proto();
      const bool use_lb = (snap_ge == (var_coeff > 0.0));
      const double orig_active_rhs =
          use_lb ? ct.lower_bound() : ct.upper_bound();
      if (!std::isfinite(orig_active_rhs)) continue;
      const double rhs_lb = use_lb ? (include_bound_tol && bound_tol != 0.0
                                          ? ExactSumLb(row.lb, -bound_tol)
                                          : row.lb)
                                   : row_ub_lower[r];
      const double rhs_ub = use_lb ? row_lb_upper[r]
                                   : (include_bound_tol && bound_tol != 0.0
                                          ? ExactSumUb(row.ub, bound_tol)
                                          : row.ub);
      const std::optional<std::pair<double, double>> row_err =
          TryComputeSingleVarConstraintErrorBounds(row, states, var, s, rhs_lb,
                                                   rhs_ub);
      if (!row_err.has_value() && params.mip_proper_relaxation()) {
        return kInfinity;
      }
      const auto [e_lb, e_ub] = row_err.value_or(std::make_pair(-0.5, 0.5));
      err = std::max({err, -e_lb, e_ub});
    }
    return err;
  };

  // First corner case: If the scaled domain does not contain an integer
  // point, then the domain of the variable will always be either fully
  // feasible or fully infeasible on the snapped side! So we can just fix
  // that variable to the corresponding bound in its domain. No need to scale
  // at all.
  //
  // NOTE(user): This do not trigger on the miplib though.
  const auto domain_has_no_integer = [&](double s, bool snap_ge) {
    if (!(s > 0.0 && s < kInfinity)) return false;
    if (params.mip_proper_relaxation()) {
      if (!std::isfinite(state.lb) || !std::isfinite(state.ub)) return false;
      const double row_err =
          compute_max_row_err(s, snap_ge, /*include_bound_tol=*/true);
      if (!std::isfinite(row_err)) return false;
      const double scaled_var_lb =
          ExactSumLb(ProdBounds(state.lb, s).first, -row_err);
      const double scaled_var_ub =
          ExactSumUb(ProdBounds(state.ub, s).second, row_err);
      return std::ceil(scaled_var_lb) > std::floor(scaled_var_ub);
    }
    const double scaled_var_lb = (state.lb - tolerance) * s;
    const double scaled_var_ub = (state.ub + tolerance) * s;
    return std::ceil(scaled_var_lb) > std::floor(scaled_var_ub);
  };
  const bool fix_to_lb = domain_has_no_integer(scaling_ge, /*snap_ge=*/true);
  const bool fix_to_ub = domain_has_no_integer(scaling_le, /*snap_ge=*/false);
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
  if (!(scaling > 0.0 && scaling < kInfinity)) {
    ++stats->num_missed;
    return;
  }

  // Bound `discretized_var - scaling * var` when snapping `var` to the active
  // side of its rows (or to its own bound). Pushing a tolerant `var` to the
  // tightest active bound only conflicts with an opposite row side when another
  // constraint opposes it (or a single constraint has `lb > ub`), or when the
  // snapped integer could cross the opposite variable bound.
  const bool snap_ge = (scaling == scaling_ge);
  double max_err =
      compute_max_row_err(scaling, snap_ge, /*include_bound_tol=*/false);
  const double active_var_bound = snap_ge ? state.lb : state.ub;
  if ((snap_ge ? need_lb : need_ub) && std::isfinite(active_var_bound)) {
    const double r = std::round(scaling * active_var_bound);
    if (!std::isfinite(r)) {
      max_err = kInfinity;
    } else {
      max_err =
          std::max(max_err, MaxAbsScaledTightSum(TightSum().Add(r).AddProd(
                                -scaling, active_var_bound)));
    }
  }
  double err_lb = -max_err;
  double err_ub = max_err;
  if (bound_tol != 0.0) {
    int num_active_rows = 0;
    int num_opp_rows = 0;
    int sole_active_row = -1;
    int sole_opp_row = -1;
    bool has_inverted_row = false;
    for (int r = 0; r < num_rows; ++r) {
      const double var_coeff = GetVarCoeff(rows[r], var);
      if (var_coeff == 0.0) continue;
      const MPConstraintProto& ct =
          mp_model.constraints(compo.constraints[r]).proto();
      if (ct.lower_bound() > ct.upper_bound()) has_inverted_row = true;
      const bool use_lb = (snap_ge == (var_coeff > 0.0));
      if (std::isfinite(use_lb ? ct.lower_bound() : ct.upper_bound())) {
        ++num_active_rows;
        sole_active_row = r;
      }
      if (std::isfinite(use_lb ? ct.upper_bound() : ct.lower_bound())) {
        ++num_opp_rows;
        sole_opp_row = r;
      }
    }
    bool need_tol_on_opposite_side =
        has_inverted_row || (num_active_rows > 0 && num_opp_rows > 0 &&
                             (num_active_rows > 1 || num_opp_rows > 1 ||
                              sole_active_row != sole_opp_row));
    const double max_err_tol = std::max(
        max_err,
        compute_max_row_err(scaling, snap_ge, /*include_bound_tol=*/true));
    if (!need_tol_on_opposite_side && std::isfinite(max_err_tol)) {
      if (snap_ge && std::isfinite(state.ub)) {
        const double max_snapped = std::floor(
            ExactSumUb(ProdBounds(state.ub, scaling).second, max_err_tol));
        need_tol_on_opposite_side =
            max_snapped >
            ExactSumLb(ProdBounds(state.ub, scaling).first, max_err);
      } else if (!snap_ge && std::isfinite(state.lb)) {
        const double min_snapped = std::ceil(
            ExactSumLb(ProdBounds(state.lb, scaling).first, -max_err_tol));
        need_tol_on_opposite_side =
            min_snapped <
            ExactSumUb(ProdBounds(state.lb, scaling).second, -max_err);
      }
    } else if (!std::isfinite(max_err_tol)) {
      need_tol_on_opposite_side = true;
    }
    if (need_tol_on_opposite_side) {
      if (snap_ge) {
        err_ub = max_err_tol;
      } else {
        err_lb = -max_err_tol;
      }
    }
  }
  const double worst_err = std::max(-err_lb, err_ub);
  if (!std::isfinite(worst_err)) {
    ++stats->num_missed;
    return;
  }

  // With mip_scale_for_precision, compare with the power of two scaling that
  // ScaleComponentContinuousVariablesForGivenPrecision() would use, on the
  // worst case error on var (in its original space).
  if (params.mip_scale_for_precision()) {
    const double pow2_scaling = ComputePrecisionScalings(
        params, mp_model, compo, states, PrecisionHeadroomMode::kPerTerm)[0];
    const auto [pow2_err_lb, pow2_err_ub] =
        GetContinuousVarDiscretizationErrorBounds(
            params.mip_proper_relaxation(), obj_coeff);
    const double pow2_err = std::max(-pow2_err_lb, pow2_err_ub) / pow2_scaling;
    if (worst_err / scaling > pow2_err) {
      ++stats->num_pow2_preferred;
      return;
    }
  }

  if (obj_coeff != 0.0) {
    const double scaled_abs_obj =
        QuotientBounds(std::abs(obj_coeff), scaling).second;
    if (max_err > 0.0) {
      const double obj_err_round =
          std::isfinite(scaled_abs_obj)
              ? ProdBounds(scaled_abs_obj, max_err).second
              : kInfinity;
      stats->objective_error_bound =
          ExactSumUb(stats->objective_error_bound, obj_err_round);
    }
    double compo_sensitivity = 0.0;
    for (int r = 0; r < num_rows; ++r) {
      const double var_coeff = GetVarCoeff(rows[r], var);
      if (var_coeff == 0.0) continue;
      const MPConstraintProto& ct =
          mp_model.constraints(compo.constraints[r]).proto();
      const bool use_lb = (snap_ge == (var_coeff > 0.0));
      if (!std::isfinite(use_lb ? ct.lower_bound() : ct.upper_bound())) {
        continue;
      }
      compo_sensitivity = std::max(
          compo_sensitivity,
          QuotientBounds(std::abs(obj_coeff), std::abs(var_coeff)).second);
    }
    stats->max_violation_sensitivity =
        std::max(stats->max_violation_sensitivity, compo_sensitivity);
  }

  ++stats->num_integers;
  stats->max_scaling = std::max(stats->max_scaling, scaling);
  state.error_lb = err_lb;
  state.error_ub = err_ub;
  state.is_integer = true;
  ScaleVariableState(scaling, &state);
}

void LogOneVariableComponentStats(const OneVariableComponentStats& stats,
                                  SolverLogger* logger) {
  if (stats.num_components == 0) return;
  SOLVER_LOG(logger, "[OneVarComponents] ", stats.num_components,
             " components: ", stats.num_integers,
             " marked as integer (max scaling ", stats.max_scaling,
             ", objective error <= ", stats.objective_error_bound, " + ",
             stats.max_violation_sensitivity, " * sum_compo_max_viol), ",
             stats.num_fixed, " fixed, ", stats.num_pow2_preferred,
             " better scaled by a power of two, ", stats.num_missed,
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
  ct_lower_bound = ExactSumLb(ct_lower_bound, -bound_tol);
  ct_upper_bound = ExactSumUb(ct_upper_bound, bound_tol);

  // Constraint statistic used below.
  double min_activity = 0.0;
  double max_activity = 0.0;

  // Transform the constraint and its variable into an easier to use form.
  var_indices.clear();
  bounded_expr.Clear();
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
    bounded_expr.AddTerm(coeff, lb, ub);
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
  if (bounded_expr.coeffs.empty()) modular.reset();

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

  // Since the activity of the discretized point is always within [min_activity,
  // max_activity], a side cannot be off by more than its distance to these
  // bounds. Note that this must be done before skipping trivial constraints,
  // otherwise a side that is relaxed so much that it becomes trivial would not
  // be accounted for.
  if (std::isfinite(ct_lower_bound)) {
    max_discretization_error = std::max(
        max_discretization_error,
        std::min(-discretization_error_lb, ct_lower_bound - min_activity));
  }
  if (std::isfinite(ct_upper_bound)) {
    max_discretization_error = std::max(
        max_discretization_error,
        std::min(discretization_error_ub, max_activity - ct_upper_bound));
  }

  if (use_proper_relaxation) {
    if (std::isfinite(ct_lower_bound) && discretization_error_lb != 0.0) {
      ct_lower_bound = ExactSumLb(ct_lower_bound, discretization_error_lb);
    }
    if (std::isfinite(ct_upper_bound) && discretization_error_ub != 0.0) {
      ct_upper_bound = ExactSumUb(ct_upper_bound, discretization_error_ub);
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
    ComputeScalingErrors(bounded_expr, scaling_factor, &relative_coeff_error,
                         &scaled_sum_error);
  } else {
    scaling_factor = FindBestScalingAndComputeErrors(
        bounded_expr, scaling_target, wanted_precision, &relative_coeff_error,
        &scaled_sum_error);
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

  if (use_tight_bounds || use_proper_relaxation) {
    tight_error_helper.LoadUnscaledConstraint(
        bounded_expr.coeffs, int_lower_bounds, int_upper_bounds);
    tight_error_helper.LoadScalingFactor(scaling_factor);
  }

  // Accumulate the scaled bounds `ct_bound * scaling_factor` in full precision.
  // Note that TightScalingErrorHelper also bounds the exact scaled coefficients
  // `bounded_expr.coeffs[i] * scaling_factor` (not just their double rounding),
  // so the rounding errors computed below are valid for this exact scaled
  // constraint.
  const bool has_lb = std::isfinite(ct_lower_bound);
  const bool has_ub = std::isfinite(ct_upper_bound);
  tmp_scaled_lb.Clear();
  tmp_scaled_ub.Clear();
  if (has_lb) tmp_scaled_lb.AddProd(ct_lower_bound, scaling_factor);
  if (has_ub) tmp_scaled_ub.AddProd(ct_upper_bound, scaling_factor);

  // Write the scaled constraint content.
  int new_bound_size = 0;
  const int64_t gcd =
      ComputeGcdOfRoundedDoubles(bounded_expr.coeffs, scaling_factor);
  for (int i = 0; i < bounded_expr.coeffs.size(); ++i) {
    const double scaled_value = bounded_expr.coeffs[i] * scaling_factor;
    const int64_t rounded_coeff =
        static_cast<int64_t>(std::round(scaled_value)) / gcd;
    if (rounded_coeff != 0) {
      mutable_linear->add_vars(var_indices[i]);
      mutable_linear->add_coeffs(rounded_coeff);

      int_lower_bounds[new_bound_size] = int_lower_bounds[i];
      int_upper_bounds[new_bound_size] = int_upper_bounds[i];
      ++new_bound_size;
    }
  }
  int_lower_bounds.resize(new_bound_size);
  int_upper_bounds.resize(new_bound_size);

  // Compute tighter rhs errors on the rounding(1)/unrounding(2) if asked, and
  // relax the scaled bounds by the rounding error so that we never lose a
  // feasible solution of the original problem that is within `bound_tol`.
  // However, unless `use_proper_relaxation` is set, we don't relax by more
  // than `wanted_precision` if we are not precise, and just hope for the best.
  //
  // Note that the "unrounding" errors are just informative, these are the
  // worst error we could make when scaling back a feasible solution, but it
  // might happen than our optimal solution is well within the original bound
  // when unscaled.
  if (use_tight_bounds || use_proper_relaxation) {
    double lb_error1 = 0.0;
    double lb_error2 = 0.0;
    if (has_lb) {
      const TightSum& lb_err =
          tight_error_helper.GetRelaxationLowerBoundError(tmp_scaled_lb);
      lb_error1 = -lb_err.TightLowerBound() / scaling_factor;
      if (use_proper_relaxation || lb_error1 <= wanted_precision) {
        tmp_scaled_lb.Add(lb_err);
      } else {
        tmp_scaled_lb.AddProd(-wanted_precision, scaling_factor);
      }
      lb_error2 = tight_error_helper.GetLowerBoundUnroundingError(
                      ct_lower_bound * scaling_factor) /
                  scaling_factor;
    }
    double ub_error1 = 0.0;
    double ub_error2 = 0.0;
    if (has_ub) {
      const TightSum& ub_err =
          tight_error_helper.GetRelaxationUpperBoundError(tmp_scaled_ub);
      ub_error1 = ub_err.TightUpperBound() / scaling_factor;
      if (use_proper_relaxation || ub_error1 <= wanted_precision) {
        tmp_scaled_ub.Add(ub_err);
      } else {
        tmp_scaled_ub.AddProd(wanted_precision, scaling_factor);
      }
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
  }

  if (modular.has_value()) {
    // In a TUM (resp. 2-modular) component, the LP extreme points are only
    // integral if the scaled bounds are integer (resp. even). We thus round
    // the unrelaxed bounds to multiples of `modular_factor`, using `bound_tol`
    // as the snapping tolerance of RoundScaledLowerBound(), instead of the
    // relaxed bounds computed above.
    const double m = static_cast<double>(modular->modular_factor);
    const double f = scaling_factor / m;
    const double tol =
        f * (use_proper_relaxation ? bound_tol : wanted_precision);
    const auto round_lb = [&](double lb) {
      return m * RoundScaledLowerBound(ProdBounds(lb, f).first, tol,
                                       use_proper_relaxation);
    };
    const bool tight = use_tight_bounds || use_proper_relaxation;
    if (has_lb) {
      const double scaled_lb = round_lb(unrelaxed_lower_bound);
      if (tight) {
        // Since the bound might have been tightened, we add its distance to
        // the relaxed bound.
        const double lb = std::max(scaled_lb, min_activity * scaling_factor);
        max_unrounding_error =
            std::max(max_unrounding_error,
                     tight_error_helper.GetLowerBoundUnroundingError(lb) /
                             scaling_factor +
                         std::max(0.0, (tmp_scaled_lb.TightUpperBound() - lb) /
                                           scaling_factor));
      }
      tmp_scaled_lb.Clear();
      tmp_scaled_lb.Add(scaled_lb);
    }
    if (has_ub) {
      const double scaled_ub = -round_lb(-unrelaxed_upper_bound);
      if (tight) {
        const double ub = std::min(scaled_ub, max_activity * scaling_factor);
        max_unrounding_error =
            std::max(max_unrounding_error,
                     tight_error_helper.GetUpperBoundUnroundingError(ub) /
                             scaling_factor +
                         std::max(0.0, (ub - tmp_scaled_ub.TightLowerBound()) /
                                           scaling_factor));
      }
      tmp_scaled_ub.Clear();
      tmp_scaled_ub.Add(scaled_ub);
    }
  }

  // Add the constraint bounds. If a scaled bound does not fit in an int64_t,
  // the constraint is either always true or always false.
  int64_t final_lb;
  if (!has_lb) {
    // Note that `ct_lower_bound == kInfinity` is an infeasible corner case.
    final_lb = ct_lower_bound == kInfinity ? kint64max : kint64min;
  } else {
    const auto [lb_floor, lb_ceil] = tmp_scaled_lb.Int64Bounds();
    if (!lb_ceil.has_value()) {
      final_lb = kint64max;
    } else if (!lb_floor.has_value() || *lb_ceil == kint64min) {
      final_lb = kint64min;
    } else {
      final_lb = CeilRatio(IntegerValue(*lb_ceil), IntegerValue(gcd)).value();
    }
  }

  int64_t final_ub;
  if (!has_ub) {
    // Note that `ct_upper_bound == -kInfinity` is an infeasible corner case.
    final_ub = ct_upper_bound == -kInfinity ? kint64min : kint64max;
  } else {
    const auto [ub_floor, ub_ceil] = tmp_scaled_ub.Int64Bounds();
    if (!ub_floor.has_value()) {
      final_ub = kint64min;
    } else if (!ub_ceil.has_value() || *ub_floor == kint64max) {
      final_ub = kint64max;
    } else {
      final_ub = FloorRatio(IntegerValue(*ub_floor), IntegerValue(gcd)).value();
    }
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
    operations_research::ComputeScalingErrors(
        coefficients, lower_bounds, upper_bounds, x, relative_coeff_error,
        scaled_sum_error);
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
        operations_research::ComputeScalingErrors(
            coefficients, lower_bounds, upper_bounds, integer_factor,
            &local_relative_coeff_error, &local_scaled_sum_error);

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
    state.lb = ExactSumLb(state.lb, -offset);
    state.ub = ExactSumUb(state.ub, -offset);
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
      err_lb = std::max(err_lb, ExactSumLb(d_min, -var_ub));
      err_ub = std::min(err_ub, ExactSumUb(d_min, -var_lb));
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

bool PopulateCpModelFloatingPointObjective(
    const MPModelProtoWrapper& mp_model,
    absl::Span<const MipVariableState> states, CpModelProto* cp_model) {
  // Since cp_model supports a floating point objective, we use that. This will
  // allow us to scale the objective a bit later so we can potentially do more
  // domain reduction first.
  cp_model->clear_floating_point_objective();
  if (!std::isfinite(mp_model.objective_offset())) return false;
  auto* float_objective = cp_model->mutable_floating_point_objective();
  const double scaling = mp_model.objective_scaling();
  float_objective->set_maximize(scaling < 0.0);
  TightSum offset;
  offset.Add(mp_model.objective_offset());
  const int num_variables = mp_model.num_variables();
  DCHECK_EQ(states.size(), num_variables);
  DCHECK_EQ(cp_model->variables_size(), num_variables);
  for (int i = 0; i < num_variables; ++i) {
    const double coeff = mp_model.variables(i).objective_coefficient();
    if (coeff == 0.0) continue;
    const MipVariableState& state = states[i];
    if (!std::isfinite(coeff) || !std::isfinite(state.scaling) ||
        state.scaling <= 0.0 || !std::isfinite(state.shift)) {
      cp_model->clear_floating_point_objective();
      return false;
    }
    if (state.shift != 0.0) offset.AddProd(coeff, state.shift);
    const auto [c_lb, c_ub] = QuotientBounds(coeff, state.scaling);
    if (!std::isfinite(c_lb) || !std::isfinite(c_ub)) {
      cp_model->clear_floating_point_objective();
      return false;
    }
    const auto& domain = cp_model->variables(i).domain();
    DCHECK_GE(domain.size(), 2);
    const int64_t d_lb = domain[0];
    const int64_t d_ub = domain[domain.size() - 1];
    double scaled_coeff = c_lb;
    if (d_lb >= 0) {
      scaled_coeff = c_lb;
    } else if (d_ub <= 0) {
      scaled_coeff = c_ub;
    } else if (d_lb + d_ub >= 0) {
      scaled_coeff = c_lb;
      if (c_lb != c_ub) {
        // Equivalent to `(c_ub - c_lb) * d_lb`, avoiding float subtraction.
        offset.AddProd(c_ub, d_lb);
        offset.AddProd(-c_lb, d_lb);
      }
    } else {
      scaled_coeff = c_ub;
      if (c_lb != c_ub) {
        offset.AddProd(c_lb, d_ub);
        offset.AddProd(-c_ub, d_ub);
      }
    }
    if (scaled_coeff != 0.0) {
      float_objective->add_vars(i);
      float_objective->add_coeffs(scaled_coeff * scaling);
    }
  }
  const double offset_lb = offset.TightLowerBound();
  if (!std::isfinite(offset_lb)) {
    cp_model->clear_floating_point_objective();
    return false;
  }
  if (offset_lb != 0.0) {
    float_objective->set_offset(offset_lb * scaling);
  }

  // If the objective is fixed to zero, we consider there is none.
  if (float_objective->offset() == 0 && float_objective->vars().empty()) {
    cp_model->clear_floating_point_objective();
  }
  return true;
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
  if (!PopulateCpModelFloatingPointObjective(mp_model, states, cp_model)) {
    SOLVER_LOG(logger, "Objective overflow during CP-SAT conversion");
    return false;
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

}  // namespace sat
}  // namespace operations_research
