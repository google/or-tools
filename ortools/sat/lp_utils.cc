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
#include "ortools/sat/mip_model.pb.h"
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

namespace {

bool ConstraintIsAlwaysTrue(const EnforcedLinearConstraintProto& ct) {
  return ct.lower_bound() == -kInfinity && ct.upper_bound() == kInfinity;
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

}  // namespace

void AddShiftedTermOffset(double c_lb, double c_ub, double shift,
                          TightSum* neg_offset_lb, TightSum* neg_offset_ub) {
  if (shift > 0.0) {
    neg_offset_lb->AddProd(-c_ub, shift);
    neg_offset_ub->AddProd(-c_lb, shift);
  } else if (shift < 0.0) {
    neg_offset_lb->AddProd(-c_lb, shift);
    neg_offset_ub->AddProd(-c_ub, shift);
  }
}

void AddTermErrorBounds(double c_lb, double c_ub, double err_lb, double err_ub,
                        TightSum* sum_lb, TightSum* sum_ub) {
  if (c_ub > 0.0) {
    if (sum_lb != nullptr && err_lb != 0.0) sum_lb->AddProd(c_ub, err_lb);
    if (sum_ub != nullptr && err_ub != 0.0) sum_ub->AddProd(c_ub, err_ub);
  } else {
    if (sum_lb != nullptr && err_ub != 0.0) sum_lb->AddProd(c_lb, err_ub);
    if (sum_ub != nullptr && err_lb != 0.0) sum_ub->AddProd(c_lb, err_lb);
  }
}

void ExactActivityBounds::AddTerm(double c_lb, double c_ub, int64_t lb,
                                  int64_t ub) {
  DCHECK_LE(c_lb, c_ub);
  DCHECK_LE(lb, ub);
  const auto add_min = [&](double c, int64_t y) {
    if (y == 0 || c == 0.0) return;
    if (!std::isfinite(c)) {
      min_is_neg_inf = true;
      return;
    }
    min_activity.AddProd(c, y);
  };
  const auto add_max = [&](double c, int64_t y) {
    if (y == 0 || c == 0.0) return;
    if (!std::isfinite(c)) {
      max_is_pos_inf = true;
      return;
    }
    max_activity.AddProd(c, y);
  };
  if (c_lb >= 0.0) {
    add_min(lb >= 0 ? c_lb : c_ub, lb);
    add_max(ub >= 0 ? c_ub : c_lb, ub);
  } else if (c_ub <= 0.0) {
    add_min(ub <= 0 ? c_ub : c_lb, ub);
    add_max(lb <= 0 ? c_lb : c_ub, lb);
  } else if (ub > 0 && lb < 0) {
    if (!std::isfinite(c_lb) || !std::isfinite(c_ub)) {
      min_is_neg_inf = true;
      max_is_pos_inf = true;
    } else {
      const bool min_uses_ub =
          TightSum().AddProd(c_lb, ub).AddProd(-c_ub, lb) <= 0.0;
      add_min(min_uses_ub ? c_lb : c_ub, min_uses_ub ? ub : lb);
      const bool max_uses_ub =
          TightSum().AddProd(c_ub, ub).AddProd(-c_lb, lb) >= 0.0;
      add_max(max_uses_ub ? c_ub : c_lb, max_uses_ub ? ub : lb);
    }
  } else if (lb >= 0) {
    add_min(c_lb, ub);
    add_max(c_ub, ub);
  } else {
    add_min(c_ub, lb);
    add_max(c_lb, lb);
  }
}

double ExactActivityBounds::MaxLowerViolation(double orig_lb_upper) const {
  if (orig_lb_upper == -kInfinity) return 0.0;
  if (min_is_neg_inf || !std::isfinite(orig_lb_upper)) return kInfinity;
  TightSum diff = min_activity;
  diff.Negate().Add(orig_lb_upper);
  return std::max(0.0, diff.TightUpperBound());
}

double ExactActivityBounds::MaxUpperViolation(double orig_ub_lower) const {
  if (orig_ub_lower == kInfinity) return 0.0;
  if (max_is_pos_inf || !std::isfinite(orig_ub_lower)) return kInfinity;
  TightSum diff = max_activity;
  diff.Add(-orig_ub_lower);
  return std::max(0.0, diff.TightUpperBound());
}

namespace {
// Fills the given constraint transformed according to `states`, see the
// formula in MipVariableState. Terms with a zero coefficient are kept so that
// the result is aligned with `ct.vars()`. When `coeffs_lb` and `coeffs_ub` are
// non-null, also fills 1-ulp bounds on each exact quotient `coeff / scaling`.
// When `lb_upper` and `ub_lower` are non-null, also fills the opposite-side
// bounds of the shifted RHS (`[lb, lb_upper]` and `[ub_lower, ub]`).
void TransformConstraintWithBounds(const EnforcedLinearConstraintProto& ct,
                                   absl::Span<const MipVariableState> states,
                                   std::vector<double>* coeffs,
                                   std::vector<double>* coeffs_lb,
                                   std::vector<double>* coeffs_ub, double* lb,
                                   double* ub, double* lb_upper = nullptr,
                                   double* ub_lower = nullptr) {
  DCHECK_EQ(ct.vars_size(), ct.coeffs_size());
  const int num_terms = ct.vars_size();
  coeffs->resize(num_terms);
  if (coeffs_lb != nullptr) coeffs_lb->resize(num_terms);
  if (coeffs_ub != nullptr) coeffs_ub->resize(num_terms);
  TightSum neg_offset_lb;
  TightSum neg_offset_ub;
  bool has_offset = false;
  for (int i = 0; i < num_terms; ++i) {
    const MipVariableState& state = states[ct.vars(i)];
    const double coeff = ct.coeffs(i);
    (*coeffs)[i] = coeff / state.scaling;
    const auto [c_lb, c_ub] = QuotientBounds(coeff, state.scaling);
    if (coeffs_lb != nullptr) (*coeffs_lb)[i] = c_lb;
    if (coeffs_ub != nullptr) (*coeffs_ub)[i] = c_ub;
    if (coeff != 0.0 && state.shift != 0.0) {
      AddShiftedTermOffset(c_lb, c_ub, state.shift, &neg_offset_lb,
                           &neg_offset_ub);
      has_offset = true;
    }
  }
  if (has_offset && std::isfinite(ct.lower_bound())) {
    *lb = TightSum(neg_offset_lb).Add(ct.lower_bound()).TightLowerBound();
    if (lb_upper != nullptr) {
      *lb_upper =
          TightSum(neg_offset_ub).Add(ct.lower_bound()).TightUpperBound();
    }
  } else {
    *lb = ct.lower_bound();
    if (lb_upper != nullptr) *lb_upper = ct.lower_bound();
  }
  if (has_offset && std::isfinite(ct.upper_bound())) {
    *ub = TightSum(neg_offset_ub).Add(ct.upper_bound()).TightUpperBound();
    if (ub_lower != nullptr) {
      *ub_lower =
          TightSum(neg_offset_lb).Add(ct.upper_bound()).TightLowerBound();
    }
  } else {
    *ub = ct.upper_bound();
    if (ub_lower != nullptr) *ub_lower = ct.upper_bound();
  }
}

}  // namespace

void TransformConstraint(const EnforcedLinearConstraintProto& ct,
                         absl::Span<const MipVariableState> states,
                         std::vector<double>* coeffs, double* lb, double* ub) {
  TransformConstraintWithBounds(ct, states, coeffs, /*coeffs_lb=*/nullptr,
                                /*coeffs_ub=*/nullptr, lb, ub);
}

double ComputeBoundedRowTotalUnscalingError(
    double scaling_factor, int64_t gcd, int64_t final_lb, int64_t final_ub,
    double orig_lb_upper, double orig_ub_lower,
    const ExactActivityBounds& activity_bounds,
    TightScalingErrorHelper* tight_error_helper, double* max_unrounding_error) {
  DCHECK_GT(scaling_factor, 0.0);
  DCHECK(std::isfinite(scaling_factor));
  DCHECK_GT(gcd, 0);
  DCHECK_LE(final_lb, final_ub);

  double row_lb_total = activity_bounds.MaxLowerViolation(orig_lb_upper);
  if (final_lb > kint64min) {
    TightSum cp_scaled_lb;
    cp_scaled_lb.AddProd(gcd, final_lb);
    const TightSum& unround_lb =
        tight_error_helper->GetUnroundingLowerBoundError(cp_scaled_lb);
    if (max_unrounding_error != nullptr) {
      const double lb_unround = std::max(
          0.0, -ExactQuotientLb(unround_lb.TightLowerBound(), scaling_factor));
      *max_unrounding_error = std::max(*max_unrounding_error, lb_unround);
    }
    if (std::isfinite(orig_lb_upper) && unround_lb.IsFinite()) {
      TightSum total_lb;
      total_lb.AddProd(scaling_factor, orig_lb_upper);
      total_lb.AddProd(-gcd, final_lb);
      total_lb.AddProd(unround_lb, /*negate=*/true);
      const double knapsack_lb_viol = std::max(
          0.0, ExactQuotientUb(total_lb.TightUpperBound(), scaling_factor));
      row_lb_total = std::min(row_lb_total, knapsack_lb_viol);
    }
  }

  double row_ub_total = activity_bounds.MaxUpperViolation(orig_ub_lower);
  if (final_ub < kint64max) {
    TightSum cp_scaled_ub;
    cp_scaled_ub.AddProd(gcd, final_ub);
    const TightSum& unround_ub =
        tight_error_helper->GetUnroundingUpperBoundError(cp_scaled_ub);
    if (max_unrounding_error != nullptr) {
      const double ub_unround = std::max(
          0.0, ExactQuotientUb(unround_ub.TightUpperBound(), scaling_factor));
      *max_unrounding_error = std::max(*max_unrounding_error, ub_unround);
    }
    if (std::isfinite(orig_ub_lower) && unround_ub.IsFinite()) {
      TightSum total_ub = cp_scaled_ub;
      total_ub.Add(unround_ub);
      total_ub.AddProd(-scaling_factor, orig_ub_lower);
      const double knapsack_ub_viol = std::max(
          0.0, ExactQuotientUb(total_ub.TightUpperBound(), scaling_factor));
      row_ub_total = std::min(row_ub_total, knapsack_ub_viol);
    }
  }

  return std::max(row_lb_total, row_ub_total);
}

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
    const MipVariableProto& mp_var = mp_model.variables(var);
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
      if (state.shift != 0.0) {
        objective_offset.Add(obj * (state.shift / state.scaling));
      }
      mp_var->set_objective_coefficient(obj / state.scaling);
    }
  }
  if (objective_offset.Value() != 0.0) {
    mp_model->set_objective_offset(mp_model->objective_offset() +
                                   objective_offset.Value());
  }
  std::vector<double> coeffs;
  EnforcedLinearConstraintProto linear;
  for (MPConstraintProto& ct : *mp_model->mutable_constraint()) {
    *linear.mutable_vars() = ct.var_index();
    *linear.mutable_coeffs() = ct.coefficient();
    linear.set_lower_bound(ct.lower_bound());
    linear.set_upper_bound(ct.upper_bound());
    double lb;
    double ub;
    TransformConstraint(linear, states, &coeffs, &lb, &ub);
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
      hint->set_var_value(i, hint->var_value(i) * state.scaling - state.shift);
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
  // normalized by `wanted_precision / side_tol` so reaching its target scaling
  // brings this value down to at most `wanted_precision`.
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
  return FastTwoExp(
      HighestPowerOfTwoAtOrBelow(max_scaling_factor / scaling_factor));
}

// Returns the smallest power of two >= 1.0 that is at least
// `max_scaling_needed`.
double NeededPowerOfTwoScaling(double max_scaling_needed) {
  return max_scaling_needed <= 1.0
             ? 1.0
             : FastTwoExp(LowestPowerOfTwoAtOrAbove(max_scaling_needed));
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
  const double bound_tol = params.mip_tight_precision();
  const int64_t scaling_target = int64_t{1}
                                 << params.mip_max_activity_exponent();
  const double max_domain_bound = std::min(
      max_target_bound, FastStepDown(static_cast<double>(kint64max / 2)));

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
      if (state.scaling > 1.0 && IsPowerOfTwo(state.scaling)) {
        v.existing_pow2_scaling = state.scaling;
      }
      continue;
    }
    if (!std::isfinite(state.lb) || !std::isfinite(state.ub)) continue;
    // We bound max(0, ub) - min(0, lb) rather than max(|lb|, |ub|), so that the
    // contribution of the scaled variable to sum_max - sum_min in
    // PossibleIntegerOverflow() is at most max_domain_bound (per unit
    // coefficient).
    const double mag = std::max(0.0, state.ub) - std::min(0.0, state.lb);
    if (mag == 0.0) continue;
    const int max_exp = MaxPowerOfTwoExponentWithinBound(mag, max_domain_bound);
    if (max_exp > 0) v.max_var_scaling = FastTwoExp(max_exp);
  }

  std::vector<double> transformed_coeffs;
  std::vector<double> unscalable_errs;
  std::vector<double> scalable_errs;
  ComponentConstraint ct;
  for (const int c : compo.constraints) {
    const EnforcedLinearConstraintProto& mp_constraint =
        mp_model.constraints(c);
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
    for (int i = 0; i < mp_constraint.vars().size(); ++i) {
      const int var = mp_constraint.vars(i);
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
      const auto subtract_budget = [](double budget, double deduction) {
        if (deduction <= 0.0) return budget;
        const double next = budget - deduction;
        return std::max(0.0,
                        next < budget ? next : std::nextafter(budget, 0.0));
      };
      // Leave `wanted_precision - ct.unscaled_err` of the row's precision
      // budget for variable discretization (up to the full `wanted_precision`
      // when coefficient rounding is exact), floored at `wanted_precision / 2`
      // so rows whose rounding error is close to or above `wanted_precision` do
      // not request unbounded variable scaling.
      const double row_disc_precision =
          std::max(wanted_precision / 2.0,
                   subtract_budget(wanted_precision, ct.unscaled_err));
      auto compute_side_disc_tol = [&](bool active, bool is_lb) -> double {
        if (!active) return kInfinity;
        if (row_disc_precision <= 0.0) return 0.0;
        unscalable_errs.clear();
        scalable_errs.clear();
        for (int i = 0; i < ct.num_terms(); ++i) {
          const int col = ct.terms[i].col;
          if (col == -1) continue;
          const ComponentVariableScaling& v = analysis.variables[col];
          const auto [lb_err, ub_err] =
              v.TermDiscErrors(ct.unscaled_expr.coeffs[i]);
          const double err = is_lb ? lb_err : ub_err;
          if (err <= 0.0) continue;
          if (v.max_var_scaling <= 1.0) {
            unscalable_errs.push_back(err / v.existing_pow2_scaling);
          } else {
            scalable_errs.push_back(err);
          }
        }
        if (scalable_errs.empty()) return kInfinity;
        std::sort(unscalable_errs.begin(), unscalable_errs.end());
        std::sort(scalable_errs.begin(), scalable_errs.end());
        double rem_budget = row_disc_precision;
        int rem_terms = unscalable_errs.size() + scalable_errs.size();
        for (const double err : unscalable_errs) {
          const double share = rem_budget / rem_terms;
          rem_budget = subtract_budget(rem_budget, std::min(err, share));
          --rem_terms;
        }
        double tol = rem_budget / rem_terms;
        for (const double err : scalable_errs) {
          tol = rem_budget / rem_terms;
          if (err > tol) break;
          rem_budget = subtract_budget(rem_budget, err);
          --rem_terms;
        }
        return tol;
      };
      const double lb_tol = compute_side_disc_tol(ct.has_lb, /*is_lb=*/true);
      const double ub_tol = compute_side_disc_tol(ct.has_ub, /*is_lb=*/false);
      const double lb_weight =
          lb_tol < kInfinity ? (lb_tol > 0.0 ? wanted_precision / lb_tol : 1.0)
                             : 0.0;
      const double ub_weight =
          ub_tol < kInfinity ? (ub_tol > 0.0 ? wanted_precision / ub_tol : 1.0)
                             : 0.0;
      for (int i = 0; i < ct.num_terms(); ++i) {
        const int col = ct.terms[i].col;
        if (col == -1) continue;
        ComponentVariableScaling& v = analysis.variables[col];
        if (v.max_var_scaling <= 1.0) continue;
        const auto [lb_err, ub_err] =
            v.TermDiscErrors(ct.unscaled_expr.coeffs[i]);
        v.max_scaling_needed = std::max({
            v.max_scaling_needed,
            lb_err > 0.0 ? lb_err / lb_tol : 0.0,
            ub_err > 0.0 ? ub_err / ub_tol : 0.0,
        });
        v.max_weighted_disc_err = std::max(
            {v.max_weighted_disc_err, lb_weight * lb_err, ub_weight * ub_err});
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
    DCHECK(IsPowerOfTwo(variables[col].target_scaling));
    const int hi = FastILogB(variables[col].target_scaling);
    hi_exp[i] = hi;
    chosen_exp[i] = std::min(hi, uniform_exp);
    if (chosen_exp[i] < hi) any_capped_by_uniform = true;
  }
  if (!any_scalable) return;
  if (!any_capped_by_uniform) {
    for (int i = 0; i < num_terms; ++i) {
      if (hi_exp[i] > 0) {
        const int col = ct.terms[i].col;
        variables[col].min_headroom =
            std::min(variables[col].min_headroom, FastTwoExp(chosen_exp[i]));
      }
    }
    return;
  }

  const double sf_q_odd =
      static_cast<double>(OddSignificand(ct.scaling_factor));

  DCHECK(IsPowerOfTwo(ct.max_scaling_factor));
  const int max_sf_exp = FastILogB(ct.max_scaling_factor);

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
          std::min(variables[col].min_headroom, FastTwoExp(chosen_exp[i]));
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
  const double max_var_bound = MaxCpSatVariableBound();
  ScaleVariableState(extra_scaling, &state);
  if (!(state.lb >= -max_var_bound && state.ub <= max_var_bound)) {
    return {state.lb, state.ub};
  }
  if (state.shift == 0.0) {
    ShiftVariable(FinalCpSatShiftValue(state.lb, state.ub, state.is_integer,
                                       params.mip_wanted_precision()),
                  &state);
  }
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

void LogPrecisionScalingStats(const SatParameters& params,
                              const MPModelProtoWrapper& mp_model,
                              absl::Span<const MipVariableState> states,
                              const PrecisionScalingStats& stats,
                              SolverLogger* logger) {
  if (stats.num_scaled == 0) return;

  // Worst case objective error due to discretizing each continuous variable.
  const double obj_error_factor = params.mip_proper_relaxation() ? 1.0 : 0.5;
  double objective_discretization_error = 0.0;
  const int num_variables = mp_model.num_variables();
  for (int var = 0; var < num_variables; ++var) {
    if (states[var].is_integer) continue;
    const double coeff = mp_model.variables(var).objective_coefficient();
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

    DCHECK(IsPowerOfTwo(scaling));
    const int exp = FastILogB(scaling);

    ++stats->num_scaled;
    stats->min_exponent = std::min(stats->min_exponent, exp);
    stats->max_exponent = std::max(stats->max_exponent, exp);

    const int var = compo_vars[local_col];
    MipVariableState& state = states[var];
    CHECK_EQ(state.scaling, 1.0);
    ScaleVariableState(scaling, &state);
  }
}

namespace {

// A linear row `lb <= sum coeffs[i] * vars[i] <= ub` expressed in the current
// variable space, see TransformConstraintWithBounds(). When `coeffs_lb` and
// `coeffs_ub` are non-empty, they provide 1-ulp bounds on the exact transformed
// coefficients.
struct LinearRow {
  absl::Span<const int> vars;
  absl::Span<const double> coeffs;
  absl::Span<const double> coeffs_lb = {};
  absl::Span<const double> coeffs_ub = {};
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
// to take integer values to satisfy this side. When `states` is non-empty, the
// rational approximation tolerances also account for external variable domains
// and `|var_coeff|` so the resulting error on `var` stays within `tolerance`.
//
// We return kInfinity if we failed, and 0.0 if this side is not constrained.
//
// Precondition: var must be the only non-integer in the given row.
std::pair<double, double> GetIntegralityMultiplier(
    const LinearRow& row, int var, double tolerance, double max_multiplier,
    absl::Span<const MipVariableState> states = {}) {
  const double kInfinity = std::numeric_limits<double>::infinity();

  const double var_coeff = GetVarCoeff(row, var);
  if (var_coeff == 0.0) return {0.0, 0.0};
  const double abs_var_coeff = std::abs(var_coeff);
  const double limit = max_multiplier / abs_var_coeff;
  if (limit < 1.0) return {kInfinity, kInfinity};
  const int64_t int_limit =
      limit >= static_cast<double>(kint64max)
          ? kint64max
          : CapAdd(static_cast<int64_t>(std::floor(limit)), 1);

  const double min_var_scale =
      !states.empty() ? std::min(1.0, abs_var_coeff) : 1.0;
  double multiplier = 1.0;
  for (int i = 0; i < row.vars.size(); ++i) {
    const int j = row.vars[i];
    if (var == j) continue;
    const double abs_coeff = std::abs(row.coeffs[i]);
    if (abs_coeff == 0.0) continue;

    // This actually computes the smallest multiplier to make all other
    // terms in the constraint integer.
    //
    // Note that we want the rational approximation of the coefficient to be
    // within `tolerance` in absolute value and relatively, and (when `states`
    // is provided) tight enough across `states[j].[lb, ub]` so that the domain
    // remainder does not blow up the error on `var`.
    const double coeff = multiplier * row.coeffs[i];
    double coeff_tol_scale = std::min(1.0, abs_coeff);
    if (!states.empty()) {
      const double max_dom =
          std::max(std::abs(states[j].lb), std::abs(states[j].ub));
      if (!std::isfinite(max_dom)) {
        coeff_tol_scale = std::min(coeff_tol_scale, 1e-15 * abs_coeff);
      } else if (max_dom > 0.0) {
        coeff_tol_scale =
            std::min(coeff_tol_scale, min_var_scale / std::max(1.0, max_dom));
      }
    }
    const double local_tolerance = multiplier * tolerance * coeff_tol_scale;
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
  const double min_clamp = std::min(tolerance, 1.0);

  // Now adjust for the bound. Same remark as above regarding the relative
  // tolerance, but a zero bound is always fine.
  double lb_multiplier = multiplier;
  if (ct_lb == kInfinity) {
    lb_multiplier = 0.0;
  } else if (multiplier != kInfinity) {
    const double lb_tolerance = multiplier * tolerance * min_var_scale *
                                std::clamp(ct_lb, min_clamp, 1.0);
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
    const double ub_tolerance = multiplier * tolerance * min_var_scale *
                                std::clamp(ct_ub, min_clamp, 1.0);
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
                                         MPModelProtoWrapper* mp_model,
                                         SolverLogger* logger) {
  const int num_variables = mp_model->num_variables();
  int64_t num_changes = 0;
  for (int i = 0; i < num_variables; ++i) {
    auto* mp_var = mp_model->mutable_variables(i);
    if (!mp_var->is_integer()) continue;
    double lb = mp_var->lower_bound();
    double ub = mp_var->upper_bound();
    if (!MakeIntegerVariableBoundsInteger(params, /*scaling=*/1.0,
                                          /*error_lb=*/0.0, /*error_ub=*/0.0,
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

// TODO(user): we use LooseLowerBound()/LooseUpperBound() to avoid a quadratic
// complexity. Fix in order to use TightLowerBound()/TightUpperBound().
template <bool exact, bool fill_min, bool fill_max>
void LeftRightActivityBounds::FillFromConstraint(
    absl::Span<const double> var_lbs, absl::Span<const double> var_ubs,
    const EnforcedLinearConstraintProto& ct) {
  static_assert(fill_min || fill_max);
  const int num_terms = ct.vars().size();
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
    const int var = ct.vars(i);
    double coeff = ct.coeffs(i);
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
    const int var = ct.vars(i);
    double coeff = ct.coeffs(i);
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
    const EnforcedLinearConstraintProto& ct);
template void LeftRightActivityBounds::FillFromConstraint<true, true, false>(
    absl::Span<const double> var_lbs, absl::Span<const double> var_ubs,
    const EnforcedLinearConstraintProto& ct);
template void LeftRightActivityBounds::FillFromConstraint<true, false, true>(
    absl::Span<const double> var_lbs, absl::Span<const double> var_ubs,
    const EnforcedLinearConstraintProto& ct);
template void LeftRightActivityBounds::FillFromConstraint<false, true, true>(
    absl::Span<const double> var_lbs, absl::Span<const double> var_ubs,
    const EnforcedLinearConstraintProto& ct);
template void LeftRightActivityBounds::FillFromConstraint<false, true, false>(
    absl::Span<const double> var_lbs, absl::Span<const double> var_ubs,
    const EnforcedLinearConstraintProto& ct);
template void LeftRightActivityBounds::FillFromConstraint<false, false, true>(
    absl::Span<const double> var_lbs, absl::Span<const double> var_ubs,
    const EnforcedLinearConstraintProto& ct);

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

  TightSum delta_lb;
  TightSum delta_ub;

  const int num_constraints = mp_model.num_constraints();
  for (int c = 0; c < num_constraints; ++c) {
    const EnforcedLinearConstraintProto& ct = mp_model.constraints(c);
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

    const int num_terms = ct.vars().size();
    if (num_terms == 0) continue;

    const bool has_lb = ct.lower_bound() > -kInfinity;
    const bool has_ub = ct.upper_bound() < kInfinity;
    if (!has_lb && !has_ub) continue;

    // Count infinite term bounds on each active side, and determine whether any
    // variable is still movable in the direction constrained by that side.
    bool need_lb = false;
    bool need_ub = false;
    int num_inf_lbs = 0;
    int num_inf_ubs = 0;
    int lb_inf_idx = -1;
    int ub_inf_idx = -1;
    for (int i = 0; i < num_terms; ++i) {
      const double coeff = ct.coeffs(i);
      if (coeff == 0.0) continue;
      const int var = ct.vars(i);
      if (coeff > 0.0) {
        if (has_lb) {
          if (!std::isfinite(var_lbs[var])) {
            ++num_inf_lbs;
            lb_inf_idx = i;
          }
          need_lb |= can_freely_decrease_to[var] < var_ubs[var];
        }
        if (has_ub) {
          if (!std::isfinite(var_ubs[var])) {
            ++num_inf_ubs;
            ub_inf_idx = i;
          }
          need_ub |= can_freely_increase_to[var] > var_lbs[var];
        }
      } else {
        if (has_lb) {
          if (!std::isfinite(var_ubs[var])) {
            ++num_inf_lbs;
            lb_inf_idx = i;
          }
          need_lb |= can_freely_increase_to[var] > var_lbs[var];
        }
        if (has_ub) {
          if (!std::isfinite(var_lbs[var])) {
            ++num_inf_ubs;
            ub_inf_idx = i;
          }
          need_ub |= can_freely_decrease_to[var] < var_ubs[var];
        }
      }
    }

    // If a side has >= 2 infinite term bounds, every term sees an infinite
    // activity of others and cannot freely move in the constrained direction.
    // If a side has exactly 1 infinite term bound `inf_idx`, every *other* term
    // sees an infinite activity of others (blocked below), and we only compute
    // the finite residual sum for `inf_idx` if its variable is still movable.
    if (need_lb && num_inf_lbs >= 1) {
      for (int i = 0; i < num_terms; ++i) {
        if (num_inf_lbs == 1 && i == lb_inf_idx) continue;
        const double coeff = ct.coeffs(i);
        if (coeff > 0.0) {
          can_freely_decrease_to[ct.vars(i)] = kInfinity;
        } else if (coeff < 0.0) {
          can_freely_increase_to[ct.vars(i)] = -kInfinity;
        }
      }
      if (num_inf_lbs >= 2) {
        need_lb = false;
      } else {
        const int inf_var = ct.vars(lb_inf_idx);
        const double inf_coeff = ct.coeffs(lb_inf_idx);
        need_lb = inf_coeff < 0.0
                      ? can_freely_increase_to[inf_var] > var_lbs[inf_var]
                      : can_freely_decrease_to[inf_var] < var_ubs[inf_var];
      }
    }
    if (need_ub && num_inf_ubs >= 1) {
      for (int i = 0; i < num_terms; ++i) {
        if (num_inf_ubs == 1 && i == ub_inf_idx) continue;
        const double coeff = ct.coeffs(i);
        if (coeff > 0.0) {
          can_freely_increase_to[ct.vars(i)] = -kInfinity;
        } else if (coeff < 0.0) {
          can_freely_decrease_to[ct.vars(i)] = kInfinity;
        }
      }
      if (num_inf_ubs >= 2) {
        need_ub = false;
      } else {
        const int inf_var = ct.vars(ub_inf_idx);
        const double inf_coeff = ct.coeffs(ub_inf_idx);
        need_ub = inf_coeff > 0.0
                      ? can_freely_increase_to[inf_var] > var_lbs[inf_var]
                      : can_freely_decrease_to[inf_var] < var_ubs[inf_var];
      }
    }
    if (!need_lb && !need_ub) continue;

    // Exact residual sums skipping the optional single infinite term:
    //   delta_lb = ct.lower_bound() - sum_{finite} min(c_j * x_j)
    //   delta_ub = ct.upper_bound() - sum_{finite} max(c_j * x_j)
    if (need_lb) {
      delta_lb.Clear();
      delta_lb.Add(ct.lower_bound());
    }
    if (need_ub) {
      delta_ub.Clear();
      delta_ub.Add(ct.upper_bound());
    }
    for (int i = 0; i < num_terms; ++i) {
      const double coeff = ct.coeffs(i);
      if (coeff == 0.0) continue;
      const int var = ct.vars(i);
      if (need_lb && i != lb_inf_idx) {
        const double bnd = coeff < 0.0 ? var_ubs[var] : var_lbs[var];
        DCHECK(std::isfinite(bnd));
        if (bnd != 0.0) delta_lb.AddProd(-coeff, bnd);
      }
      if (need_ub && i != ub_inf_idx) {
        const double bnd = coeff < 0.0 ? var_lbs[var] : var_ubs[var];
        DCHECK(std::isfinite(bnd));
        if (bnd != 0.0) delta_ub.AddProd(-coeff, bnd);
      }
    }

    // With 1 infinite term, update that single term and disable the finite
    // sweep below for that side (all other terms were blocked above).
    bool use_lb_finite = need_lb;
    if (need_lb && num_inf_lbs == 1) {
      const int var = ct.vars(lb_inf_idx);
      const double coeff = ct.coeffs(lb_inf_idx);
      // We need c_i * x_i >= delta_lb, so over-approximate delta_lb by
      // TightUpperBound() before dividing by coeff.
      if (coeff < 0.0) {
        const double dual_lt =
            QuotientBounds(delta_lb.TightUpperBound(), coeff).first;
        can_freely_increase_to[var] =
            std::min(can_freely_increase_to[var], dual_lt);
      } else {
        const double dual_gt =
            QuotientBounds(delta_lb.TightUpperBound(), coeff).second;
        can_freely_decrease_to[var] =
            std::max(can_freely_decrease_to[var], dual_gt);
      }
      use_lb_finite = false;
    }
    bool use_ub_finite = need_ub;
    if (need_ub && num_inf_ubs == 1) {
      const int var = ct.vars(ub_inf_idx);
      const double coeff = ct.coeffs(ub_inf_idx);
      // We need c_i * x_i <= delta_ub, so under-approximate delta_ub by
      // TightLowerBound() before dividing by coeff.
      if (coeff > 0.0) {
        const double dual_lt =
            QuotientBounds(delta_ub.TightLowerBound(), coeff).first;
        can_freely_increase_to[var] =
            std::min(can_freely_increase_to[var], dual_lt);
      } else {
        const double dual_gt =
            QuotientBounds(delta_ub.TightLowerBound(), coeff).second;
        can_freely_decrease_to[var] =
            std::max(can_freely_decrease_to[var], dual_gt);
      }
      use_ub_finite = false;
    }
    if (!use_lb_finite && !use_ub_finite) continue;

    // For a side with all finite term bounds:
    // - Upper bound: sum_{j != i} c_j * x_j <= U - (delta_ub + max(c_i * x_i)),
    //   so c_i * x_i <= delta_ub + max(c_i * x_i) guarantees the upper bound is
    //   satisfied (over-cautious rounding: TightLowerBound() on RHS).
    // - Lower bound: sum_{j != i} c_j * x_j >= L - (delta_lb + min(c_i * x_i)),
    //   so c_i * x_i >= delta_lb + min(c_i * x_i) guarantees the lower bound is
    //   satisfied (over-cautious rounding: TightUpperBound() on RHS).
    for (int i = 0; i < num_terms; ++i) {
      const double coeff = ct.coeffs(i);
      if (coeff == 0.0) continue;
      const int var = ct.vars(i);
      if (can_freely_increase_to[var] > var_lbs[var]) {
        if (coeff > 0.0 && use_ub_finite) {
          const double bnd = var_ubs[var];
          if (bnd != 0.0) delta_ub.AddProd(coeff, bnd);
          const double dual_lt =
              QuotientBounds(delta_ub.TightLowerBound(), coeff).first;
          if (bnd != 0.0) delta_ub.AddProd(-coeff, bnd);
          can_freely_increase_to[var] =
              std::min(can_freely_increase_to[var], dual_lt);
        } else if (coeff < 0.0 && use_lb_finite) {
          const double bnd = var_ubs[var];
          if (bnd != 0.0) delta_lb.AddProd(coeff, bnd);
          const double dual_lt =
              QuotientBounds(delta_lb.TightUpperBound(), coeff).first;
          if (bnd != 0.0) delta_lb.AddProd(-coeff, bnd);
          can_freely_increase_to[var] =
              std::min(can_freely_increase_to[var], dual_lt);
        }
      }
      if (can_freely_decrease_to[var] < var_ubs[var]) {
        if (coeff < 0.0 && use_ub_finite) {
          const double bnd = var_lbs[var];
          if (bnd != 0.0) delta_ub.AddProd(coeff, bnd);
          const double dual_gt =
              QuotientBounds(delta_ub.TightLowerBound(), coeff).second;
          if (bnd != 0.0) delta_ub.AddProd(-coeff, bnd);
          can_freely_decrease_to[var] =
              std::max(can_freely_decrease_to[var], dual_gt);
        } else if (coeff > 0.0 && use_lb_finite) {
          const double bnd = var_lbs[var];
          if (bnd != 0.0) delta_lb.AddProd(coeff, bnd);
          const double dual_gt =
              QuotientBounds(delta_lb.TightUpperBound(), coeff).second;
          if (bnd != 0.0) delta_lb.AddProd(-coeff, bnd);
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

void ChangeLargeBoundsToInfinity(double max_magnitude,
                                 MPModelProtoWrapper* mp_model,
                                 SolverLogger* logger) {
  const int num_variables = mp_model->num_variables();
  int64_t num_variable_bounds_pushed_to_infinity = 0;
  const double infinity = std::numeric_limits<double>::infinity();
  for (int i = 0; i < num_variables; ++i) {
    MipVariableProto* mp_var = mp_model->mutable_variables(i);
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

  const int num_constraints = mp_model->num_constraints();
  int64_t num_constraint_bounds_pushed_to_infinity = 0;

  for (int i = 0; i < num_constraints; ++i) {
    EnforcedLinearConstraintProto* mp_ct = mp_model->mutable_constraints(i);
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

void RemoveNearZeroTerms(const SatParameters& params,
                         MPModelProtoWrapper* mp_model, SolverLogger* logger) {
  // Having really low bounds or rhs can be problematic. We set them to zero.
  int num_dropped = 0;
  double max_dropped = 0.0;
  const double drop = params.mip_drop_tolerance();
  const int num_variables = mp_model->num_variables();
  for (int i = 0; i < num_variables; ++i) {
    auto* var = mp_model->mutable_variables(i);
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
  const int num_constraints = mp_model->num_constraints();
  for (int i = 0; i < num_constraints; ++i) {
    EnforcedLinearConstraintProto* ct = mp_model->mutable_constraints(i);
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
    double value = std::abs(mp_model->variables(i).lower_bound());
    value = std::max(value, std::abs(mp_model->variables(i).upper_bound()));
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
  //
  // The constraint bounds are shifted outward by the exact range of the removed
  // terms, so that the removal is a relaxation that never cuts a feasible
  // point. Since this range has a width of up to twice the threshold for each
  // term, we use half the precision to keep the error within our budget.
  int64_t num_removed = 0;
  for (int c = 0; c < num_constraints; ++c) {
    EnforcedLinearConstraintProto* ct = mp_model->mutable_constraints(c);
    int new_size = 0;
    const int size = ct->vars().size();
    if (size == 0) continue;
    const double threshold =
        params.mip_wanted_precision() / (2.0 * static_cast<double>(size));
    TightSum removed_min;
    TightSum removed_max;
    for (int i = 0; i < size; ++i) {
      const int var = ct->vars(i);
      const double coeff = ct->coeffs(i);
      if (std::abs(coeff) * max_bounds[var] < threshold) {
        if (max_bounds[var] != 0) {
          largest_removed = std::max(largest_removed, std::abs(coeff));
          // Same clamping as for max_bounds.
          const double lb = mp_model->variables(var).lower_bound();
          const double ub = mp_model->variables(var).upper_bound();
          removed_min.AddProd(coeff, coeff > 0.0 ? lb : ub);
          removed_max.AddProd(coeff, coeff > 0.0 ? ub : lb);
        }
        continue;
      }
      ct->set_vars(new_size, var);
      ct->set_coeffs(new_size, coeff);
      ++new_size;
    }
    if (new_size == size) continue;
    num_removed += size - new_size;
    ResizeConstraintTerms(new_size, ct);
    if (std::isfinite(ct->lower_bound())) {
      ct->set_lower_bound(TightSum()
                              .Add(ct->lower_bound())
                              .AddProd(removed_max, /*negate=*/true)
                              .TightLowerBound());
    }
    if (std::isfinite(ct->upper_bound())) {
      ct->set_upper_bound(TightSum()
                              .Add(ct->upper_bound())
                              .AddProd(removed_min, /*negate=*/true)
                              .TightUpperBound());
    }
  }

  // We also do the same for the objective coefficient.
  if (num_variables > 0) {
    const double threshold =
        params.mip_wanted_precision() / static_cast<double>(num_variables);
    for (int var = 0; var < num_variables; ++var) {
      const double coeff = mp_model->variables(var).objective_coefficient();
      if (coeff == 0.0) continue;
      if (std::abs(coeff) * max_bounds[var] < threshold) {
        ++num_removed;
        if (max_bounds[var] != 0) {
          largest_removed = std::max(largest_removed, std::abs(coeff));
        }
        mp_model->mutable_variables(var)->clear_objective_coefficient();
      }
    }
  }

  if (num_removed > 0) {
    SOLVER_LOG(logger, "Removed ", num_removed,
               " near zero terms with largest magnitude of ", largest_removed,
               ".");
  }
}

bool MipModelProtoValidationBeforeConversion(const SatParameters& params,
                                             const MipModelProto& model,
                                             SolverLogger* logger) {
  // Abort if finite variable bounds or objective is too large.
  const double threshold = params.mip_max_valid_magnitude();
  for (const MipVariableProto& var : model.variables()) {
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
  for (const EnforcedLinearConstraintProto& ct : model.constraints()) {
    if ((std::isfinite(ct.lower_bound()) &&
         std::abs(ct.lower_bound()) > threshold) ||
        (std::isfinite(ct.upper_bound()) &&
         std::abs(ct.upper_bound()) > threshold)) {
      SOLVER_LOG(logger, "Constraint bounds are too large [", ct.lower_bound(),
                 ",", ct.upper_bound(), "]");
      return false;
    }
    for (const double coeff : ct.coeffs()) {
      if (std::abs(coeff) > threshold) {
        SOLVER_LOG(logger, "Constraint coefficient is too large: ", coeff);
        return false;
      }
    }
  }

  if (!std::isfinite(model.objective_offset())) {
    SOLVER_LOG(logger,
               "Objective offset is not finite: ", model.objective_offset());
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

// Optional inputs and output pointers for
// TryComputeSingleVarConstraintErrorBounds().
struct SingleVarErrorBoundsOptions {
  // Whether `states[j].[lb, ub]` already bounds the snapped integer target
  // `discretized_j` (`true` after `MakeBoundsOfIntegerVariablesInteger`,
  // `false` when `states[j].[lb, ub]` still bounds the continuous value `j`).
  bool snapped_integer_bounds = false;

  // When non-empty (indexed by variable), overrides `states[j].error_{lb,ub}`
  // for external variables `j != var`.
  absl::Span<const double> override_error_lb = {};
  absl::Span<const double> override_error_ub = {};

  // When set, uses `*fixed_r_b` as the integer RHS offset instead of rounding
  // `(scaling / var_coeff) * midpoint(rhs_lb, rhs_ub)`, so a relaxed RHS
  // interval can be evaluated against the same integer target.
  std::optional<double> fixed_r_b = std::nullopt;

  // When `relaxed_error_ub` is non-null (requiring `var_coeff > 0`), writes an
  // upper bound on `discretized_var - scaling * var` for the same integer
  // target `r_b` when `rhs_lb` is relaxed downward by `rhs_lb_tolerance >= 0`.
  double rhs_lb_tolerance = 0.0;
  double* relaxed_error_ub = nullptr;

  // When `internal_error` is non-null, writes per-side bounds on `row`'s own
  // rounding remainder against `[internal_lb_upper, internal_ub_lower]`
  // excluding external discretization errors (which snapping `var` along `row`
  // cancels), snapping unrounded external domains via `snap_params` when set.
  const SatParameters* snap_params = nullptr;
  double internal_lb_upper = 0.0;
  double internal_ub_lower = 0.0;
  std::pair<double, double>* internal_error = nullptr;

  // When non-null, writes a lower bound on the integer target
  // `r_b - sum_j k_j * y_j` over `states[j].[lb, ub]` (or `-inf` if unbounded).
  double* min_snapped_var = nullptr;
};

// Computes outer bounds `(err_lb, err_ub)` on `discretized_var - scaling * var`
// along `row` for any solution with activity in `[rhs_lb, rhs_ub]` and external
// integerization errors `discretized_j - j` in
// `[states[j].error_lb, states[j].error_ub]` (or `options.override_error_*`).
// Returns `(0, 0)` if `var` does not appear in `row`, and `std::nullopt` if a
// certified finite bound cannot be established (overflow or unbounded external
// term with non-zero remainder).
std::optional<std::pair<double, double>>
TryComputeSingleVarConstraintErrorBounds(
    const LinearRow& row, absl::Span<const MipVariableState> states, int var,
    double scaling, double rhs_lb, double rhs_ub,
    const SingleVarErrorBoundsOptions& options = {}) {
  const double var_coeff = GetVarCoeff(row, var);
  if (var_coeff == 0.0) {
    if (options.relaxed_error_ub != nullptr) *options.relaxed_error_ub = 0.0;
    if (options.internal_error != nullptr) *options.internal_error = {0.0, 0.0};
    if (options.min_snapped_var != nullptr) {
      *options.min_snapped_var = -std::numeric_limits<double>::infinity();
    }
    return std::make_pair(0.0, 0.0);
  }
  if (!(scaling > 0.0) || !std::isfinite(scaling) || !std::isfinite(rhs_lb) ||
      !std::isfinite(rhs_ub) || !std::isfinite(options.rhs_lb_tolerance)) {
    return std::nullopt;
  }

  // Accumulate bounds on `var_coeff * (discretized_var - scaling * var)`, then
  // divide by `var_coeff` once at the end.
  const double r_b =
      options.fixed_r_b.has_value()
          ? *options.fixed_r_b
          : std::round((scaling / var_coeff) * (0.5 * rhs_lb + 0.5 * rhs_ub));
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

  TightSum int_total_lb;
  TightSum int_total_ub;
  if (options.internal_error != nullptr) {
    if (!std::isfinite(options.internal_lb_upper) ||
        !std::isfinite(options.internal_ub_lower)) {
      return std::nullopt;
    }
    int_total_lb.Add(TightSum()
                         .AddProd(r_b, var_coeff)
                         .AddProd(-scaling, options.internal_lb_upper)
                         .TightLowerBound());
    int_total_ub.Add(TightSum()
                         .AddProd(r_b, var_coeff)
                         .AddProd(-scaling, options.internal_ub_lower)
                         .TightUpperBound());
  }

  TightSum min_z_sum;
  bool min_z_finite = (options.min_snapped_var != nullptr);
  if (min_z_finite) min_z_sum.Add(r_b);

  for (int i = 0; i < row.vars.size(); ++i) {
    const int j = row.vars[i];
    if (j == var) continue;
    const double a_j = row.coeffs[i];
    if (a_j == 0.0) continue;
    const double c_lb = row.coeffs_lb.empty() ? a_j : row.coeffs_lb[i];
    const double c_ub = row.coeffs_ub.empty() ? a_j : row.coeffs_ub[i];
    if (!std::isfinite(c_lb) || !std::isfinite(c_ub)) return std::nullopt;

    const double k_j = std::round((scaling * a_j) / var_coeff);
    if (!std::isfinite(k_j)) return std::nullopt;
    if (min_z_finite && k_j != 0.0) {
      const double bound_j = k_j > 0.0 ? states[j].ub : states[j].lb;
      if (std::isfinite(bound_j)) {
        min_z_sum.AddProd(-k_j, bound_j);
      } else {
        min_z_finite = false;
      }
    }
    const double state_err_lb = options.override_error_lb.empty()
                                    ? states[j].error_lb
                                    : options.override_error_lb[j];
    const double state_err_ub = options.override_error_ub.empty()
                                    ? states[j].error_ub
                                    : options.override_error_ub[j];
    if (options.snapped_integer_bounds) {
      if (c_lb == c_ub) {
        const bool nonneg_coeff = (a_j < 0.0) == (scaling > 0.0);
        const double e_lb = nonneg_coeff ? state_err_lb : state_err_ub;
        const double e_ub = nonneg_coeff ? state_err_ub : state_err_lb;
        if (e_lb != 0.0) total_lb.AddProd(-scaling, a_j, e_lb);
        if (e_ub != 0.0) total_ub.AddProd(-scaling, a_j, e_ub);
      } else {
        AddTermErrorBounds(ProdBounds(-scaling, c_ub).first,
                           ProdBounds(-scaling, c_lb).second, state_err_lb,
                           state_err_ub, &total_lb, &total_ub);
      }
    } else if (k_j != 0.0) {
      const bool same_sign = (k_j < 0.0) == (var_coeff > 0.0);
      const double e_lb = same_sign ? state_err_lb : state_err_ub;
      const double e_ub = same_sign ? state_err_ub : state_err_lb;
      if (e_lb != 0.0) total_lb.AddProd(-k_j, var_coeff, e_lb);
      if (e_ub != 0.0) total_ub.AddProd(-k_j, var_coeff, e_ub);
    }

    const double num_lb = c_lb == c_ub ? TightSum()
                                             .AddProd(scaling, a_j)
                                             .AddProd(-k_j, var_coeff)
                                             .TightLowerBound()
                                       : TightSum()
                                             .AddProd(scaling, c_lb)
                                             .AddProd(-k_j, var_coeff)
                                             .TightLowerBound();
    const double num_ub = c_lb == c_ub ? TightSum()
                                             .AddProd(scaling, a_j)
                                             .AddProd(-k_j, var_coeff)
                                             .TightUpperBound()
                                       : TightSum()
                                             .AddProd(scaling, c_ub)
                                             .AddProd(-k_j, var_coeff)
                                             .TightUpperBound();
    if (num_lb == 0.0 && num_ub == 0.0) continue;

    const double l_j = states[j].lb;
    const double u_j = states[j].ub;
    if (!std::isfinite(l_j) || !std::isfinite(u_j)) return std::nullopt;
    const auto [rem_lb, rem_ub] = IntervalProdBounds(num_lb, num_ub, l_j, u_j);
    total_lb.Add(rem_lb);
    total_ub.Add(rem_ub);
    if (options.internal_error != nullptr) {
      double int_l_j = l_j;
      double int_u_j = u_j;
      int64_t unused = 0;
      if (options.snapped_integer_bounds || options.snap_params == nullptr ||
          MakeIntegerVariableBoundsInteger(
              *options.snap_params, states[j].scaling, states[j].error_lb,
              states[j].error_ub, &int_l_j, &int_u_j, &unused)) {
        const auto [int_rem_lb, int_rem_ub] =
            (int_l_j == l_j && int_u_j == u_j)
                ? std::make_pair(rem_lb, rem_ub)
                : IntervalProdBounds(num_lb, num_ub, int_l_j, int_u_j);
        if (!std::isfinite(int_rem_lb) || !std::isfinite(int_rem_ub)) {
          return std::nullopt;
        }
        int_total_lb.Add(int_rem_lb);
        int_total_ub.Add(int_rem_ub);
      }
    }
  }

  const auto [err_lb, err_ub] = RelaxedDivInterval(
      total_lb.TightLowerBound(), total_ub.TightUpperBound(), var_coeff);
  const double clamped_err_lb = std::min(0.0, err_lb);
  const double clamped_err_ub = std::max(0.0, err_ub);
  if (options.internal_error != nullptr) {
    const auto [int_err_lb, int_err_ub] = RelaxedDivInterval(
        std::min(0.0, int_total_lb.TightLowerBound()),
        std::max(0.0, int_total_ub.TightUpperBound()), var_coeff);
    *options.internal_error = {std::min(0.0, int_err_lb),
                               std::max(0.0, int_err_ub)};
  }
  if (options.min_snapped_var != nullptr) {
    *options.min_snapped_var = min_z_finite
                                   ? min_z_sum.TightLowerBound()
                                   : -std::numeric_limits<double>::infinity();
  }
  if (options.relaxed_error_ub != nullptr) {
    DCHECK_GT(var_coeff, 0.0);
    DCHECK_GE(options.rhs_lb_tolerance, 0.0);
    if (options.rhs_lb_tolerance > 0.0) {
      total_ub.AddProd(scaling, options.rhs_lb_tolerance);
      *options.relaxed_error_ub =
          std::max(clamped_err_ub,
                   ExactQuotientUb(total_ub.TightUpperBound(), var_coeff));
    } else {
      *options.relaxed_error_ub = clamped_err_ub;
    }
  }
  return std::make_pair(clamped_err_lb, clamped_err_ub);
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

std::vector<std::pair<int, ConstraintScalingOptions>> DetectImpliedIntegers(
    const SatParameters& params, const MPModelProtoWrapper& mp_model,
    absl::Span<MipVariableState> states, SolverLogger* logger) {
  const int num_variables = mp_model.num_variables();
  CHECK_EQ(states.size(), num_variables);

  int initial_num_integers = 0;
  for (int i = 0; i < num_variables; ++i) {
    if (states[i].is_integer) ++initial_num_integers;
  }
  VLOG(1) << "Initial num integers: " << initial_num_integers;

  // Iteratively peel unenforced near-equality constraints with a single
  // non-integer variable, recording the rational scaling and certified
  // integerization error bounds directly in `states` without mutating
  // `mp_model`. Exact-solution approximation error (`approx_error_{lb,ub}`,
  // excluding `mip_tight_precision`) is gated by `mip_wanted_precision`, while
  // the full relaxation error (including `mip_tight_precision` accumulation) is
  // gated by `< 0.5` scaled and `max(mip_wanted_precision,
  // mip_check_precision)`.
  const double tolerance = params.mip_wanted_precision();
  const double max_total_error =
      std::max(tolerance, params.mip_check_precision());
  const double bound_tol =
      params.mip_proper_relaxation() ? params.mip_tight_precision() : 0.0;
  std::vector<double> approx_error_lb(num_variables);
  std::vector<double> approx_error_ub(num_variables);
  for (int i = 0; i < num_variables; ++i) {
    approx_error_lb[i] = states[i].error_lb;
    approx_error_ub[i] = states[i].error_ub;
  }
  std::vector<int> constraint_queue;

  const int num_constraints = mp_model.num_constraints();
  std::vector<int> constraint_to_num_non_integer(num_constraints, 0);
  std::vector<std::vector<int>> var_to_constraints(num_variables);
  for (int i = 0; i < num_constraints; ++i) {
    const EnforcedLinearConstraintProto& mp_constraint =
        mp_model.constraints(i);
    if (ConstraintIsAlwaysTrue(mp_constraint)) continue;

    for (int j = 0; j < mp_constraint.vars().size(); ++j) {
      if (mp_constraint.coeffs(j) == 0.0) continue;
      const int var = mp_constraint.vars(j);
      if (!states[var].is_integer) {
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

  enum class CandidateStatus {
    kNotApplicable,
    kLargeMultiplier,
    kLargeError,
    kValid,
  };
  struct CandidateEval {
    CandidateStatus status = CandidateStatus::kNotApplicable;
    int ct_index = -1;
    double multiplier = 0.0;
    double err_lb = 0.0;
    double err_ub = 0.0;
    double row_err_lb = 0.0;
    double row_err_ub = 0.0;
    double approx_err_lb = 0.0;
    double approx_err_ub = 0.0;
    double approx_scaled_err = 0.0;
    double approx_unscaled_err = 0.0;
    double total_unscaled_err = 0.0;

    bool IsBetterThan(const CandidateEval& other) const {
      const bool is_approx = approx_scaled_err > 0.0;
      const bool other_is_approx = other.approx_scaled_err > 0.0;
      return std::tie(is_approx, multiplier, approx_unscaled_err,
                      total_unscaled_err) <
             std::tie(other_is_approx, other.multiplier,
                      other.approx_unscaled_err, other.total_unscaled_err);
    }
  };

  std::vector<double> row_coeffs;
  std::vector<double> row_coeffs_lb;
  std::vector<double> row_coeffs_ub;
  int num_processed_constraints = 0;
  const auto evaluate_constraint = [&](int ct_index, int var,
                                       double max_multiplier) -> CandidateEval {
    const EnforcedLinearConstraintProto& ct = mp_model.constraints(ct_index);
    if (!ct.enforcement_vars().empty()) return {};
    double row_lb;
    double row_ub;
    double row_lb_upper;
    double row_ub_lower;
    TransformConstraintWithBounds(ct, states, &row_coeffs, &row_coeffs_lb,
                                  &row_coeffs_ub, &row_lb, &row_ub,
                                  &row_lb_upper, &row_ub_lower);
    if (!std::isfinite(row_lb) || !std::isfinite(row_ub) || row_lb > row_ub ||
        !std::isfinite(row_lb_upper) || !std::isfinite(row_ub_lower)) {
      return {};
    }
    const LinearRow row{.vars = ct.vars(),
                        .coeffs = row_coeffs,
                        .coeffs_lb = row_coeffs_lb,
                        .coeffs_ub = row_coeffs_ub,
                        .lb = row_lb,
                        .ub = row_ub};
    const double var_coeff = GetVarCoeff(row, var);
    if (var_coeff == 0.0) return {};
    if (ExactSumUb(row_ub, -row_lb) >
        tolerance * std::min(1.0, std::abs(var_coeff))) {
      return {};
    }

    ++num_processed_constraints;
    LinearRow mid_row = row;
    const double mid_rhs =
        (row_lb <= 0.0 && 0.0 <= row_ub) ? 0.0 : 0.5 * row_lb + 0.5 * row_ub;
    mid_row.lb = mid_rhs;
    mid_row.ub = mid_rhs;
    const double multiplier = GetIntegralityMultiplier(mid_row, var, tolerance,
                                                       max_multiplier, states)
                                  .first;
    if (multiplier == 0.0) return {};
    if (multiplier == kInfinity) {
      return {.status = CandidateStatus::kLargeMultiplier};
    }

    const double r_b =
        std::round((multiplier / var_coeff) * (0.5 * row_lb + 0.5 * row_ub));
    if (!std::isfinite(r_b)) {
      return {.status = CandidateStatus::kLargeError};
    }

    std::pair<double, double> row_err = {0.0, 0.0};
    const std::optional<std::pair<double, double>> approx_err =
        TryComputeSingleVarConstraintErrorBounds(
            row, states, var, multiplier, row_lb, row_ub,
            {.override_error_lb = approx_error_lb,
             .override_error_ub = approx_error_ub,
             .fixed_r_b = r_b,
             .snap_params = &params,
             .internal_lb_upper = row_lb_upper,
             .internal_ub_lower = row_ub_lower,
             .internal_error = &row_err});
    if (!approx_err.has_value()) {
      return {.status = CandidateStatus::kLargeError};
    }
    const double approx_scaled_err =
        std::max(-approx_err->first, approx_err->second);
    if (!std::isfinite(approx_scaled_err)) {
      return {.status = CandidateStatus::kLargeError};
    }
    const double approx_unscaled_err =
        ExactQuotientUb(approx_scaled_err, multiplier);
    if (!(approx_scaled_err < 0.5) || !(approx_unscaled_err <= tolerance)) {
      return {.status = CandidateStatus::kLargeError};
    }

    std::pair<double, double> total_err = *approx_err;
    if (bound_tol != 0.0) {
      const std::optional<std::pair<double, double>> tol_err =
          TryComputeSingleVarConstraintErrorBounds(
              row, states, var, multiplier, ExactSumLb(row_lb, -bound_tol),
              ExactSumUb(row_ub, bound_tol), {.fixed_r_b = r_b});
      if (!tol_err.has_value()) {
        return {.status = CandidateStatus::kLargeError};
      }
      total_err = *tol_err;
    }
    const double worst_scaled_err =
        std::max(-total_err.first, total_err.second);
    if (!std::isfinite(worst_scaled_err)) {
      return {.status = CandidateStatus::kLargeError};
    }
    const double total_unscaled_err =
        ExactQuotientUb(worst_scaled_err, multiplier);
    if (!(worst_scaled_err < 0.5) ||
        !(std::min(worst_scaled_err, total_unscaled_err) <= max_total_error)) {
      return {.status = CandidateStatus::kLargeError};
    }

    return {
        .status = CandidateStatus::kValid,
        .ct_index = ct_index,
        .multiplier = multiplier,
        .err_lb = total_err.first,
        .err_ub = total_err.second,
        .row_err_lb = row_err.first,
        .row_err_ub = row_err.second,
        .approx_err_lb = approx_err->first,
        .approx_err_ub = approx_err->second,
        .approx_scaled_err = approx_scaled_err,
        .approx_unscaled_err = approx_unscaled_err,
        .total_unscaled_err = total_unscaled_err,
    };
  };

  int num_implied_integers = 0;
  double max_scaling = 0.0;
  double max_unscaled_error = 0.0;
  double objective_error_bound = 0.0;
  auto scale_and_mark_as_integer = [&](int var, double scaling) {
    CHECK_NE(var, -1);
    CHECK(!states[var].is_integer);
    CHECK_EQ(states[var].scaling, 1.0);
    if (scaling != 1.0) {
      VLOG(2) << "Scaled " << var << " by " << scaling;
    }

    ++num_implied_integers;
    max_scaling = std::max(max_scaling, scaling);
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

  std::vector<std::pair<int, ConstraintScalingOptions>> defining_constraints;
  std::vector<CandidateStatus> var_fail_reason(num_variables,
                                               CandidateStatus::kNotApplicable);
  while (!constraint_queue.empty()) {
    const int top_ct_index = constraint_queue.back();
    constraint_queue.pop_back();

    // The non-integer variable was already made integer by one other
    // constraint.
    if (constraint_to_num_non_integer[top_ct_index] == 0) continue;

    const EnforcedLinearConstraintProto& ct =
        mp_model.constraints(top_ct_index);
    int var = -1;
    for (int i = 0; i < ct.vars().size(); ++i) {
      if (ct.coeffs(i) != 0.0 && !states[ct.vars(i)].is_integer) {
        CHECK_EQ(var, -1);
        var = ct.vars(i);
      }
    }
    CHECK_NE(var, -1);

    const double max_multiplier =
        MaxImpliedIntegerMultiplier(params, states[var].lb, states[var].ub);

    // Evaluate this constraint first. Only if it succeeds do we scan the other
    // single-non-integer constraints on `var` to pick the most accurate (and
    // smallest-multiplier) row; otherwise any other valid constraint on `var`
    // is already in `constraint_queue` and will be popped in turn, avoiding
    // quadratic work.
    CandidateEval best = evaluate_constraint(top_ct_index, var, max_multiplier);
    if (best.status != CandidateStatus::kValid) {
      if (best.status == CandidateStatus::kLargeError ||
          (best.status == CandidateStatus::kLargeMultiplier &&
           var_fail_reason[var] == CandidateStatus::kNotApplicable)) {
        var_fail_reason[var] = best.status;
      }
      continue;
    }

    for (const int ct_index : var_to_constraints[var]) {
      if (ct_index == top_ct_index) continue;
      if (constraint_to_num_non_integer[ct_index] != 1) continue;
      const CandidateEval candidate =
          evaluate_constraint(ct_index, var, max_multiplier);
      if (candidate.status == CandidateStatus::kValid &&
          candidate.IsBetterThan(best)) {
        best = candidate;
      }
    }

    if (states[var].shift == 0.0 && std::isfinite(best.row_err_lb) &&
        std::isfinite(best.row_err_ub)) {
      const double active_tol =
          params.mip_proper_relaxation() ? 0.0 : params.mip_tight_precision();
      const int only_var = params.mip_proper_relaxation() ? var : -1;
      defining_constraints.push_back(
          {best.ct_index, ConstraintScalingOptions{
                              .lb_bound_tolerance = active_tol,
                              .ub_bound_tolerance = active_tol,
                              .lb_only_error_var = only_var,
                              .ub_only_error_var = only_var,
                              .override_error_var = var,
                              .override_error_lb = best.row_err_lb,
                              .override_error_ub = best.row_err_ub,
                          }});
    }
    states[var].error_lb = best.err_lb;
    states[var].error_ub = best.err_ub;
    approx_error_lb[var] = best.approx_err_lb;
    approx_error_ub[var] = best.approx_err_ub;
    max_unscaled_error = std::max(max_unscaled_error, best.total_unscaled_err);
    const double obj_coeff = mp_model.variables(var).objective_coefficient();
    if (obj_coeff != 0.0 && best.approx_scaled_err > 0.0) {
      const double scaled_abs_obj =
          ExactQuotientUb(std::abs(obj_coeff), best.multiplier);
      const double obj_err =
          ExactProdUb(scaled_abs_obj, best.approx_scaled_err);
      objective_error_bound = ExactSumUb(objective_error_bound, obj_err);
    }
    scale_and_mark_as_integer(var, best.multiplier);
  }

  int num_fail_due_to_large_multiplier = 0;
  int num_fail_due_to_large_error = 0;
  for (int v = 0; v < num_variables; ++v) {
    if (states[v].is_integer) continue;
    if (var_fail_reason[v] == CandidateStatus::kLargeMultiplier) {
      ++num_fail_due_to_large_multiplier;
    } else if (var_fail_reason[v] == CandidateStatus::kLargeError) {
      ++num_fail_due_to_large_error;
    }
  }

  const int num_missed =
      num_fail_due_to_large_multiplier + num_fail_due_to_large_error;
  if (num_missed > 0) {
    VLOG(1) << "num_new_integer: " << num_implied_integers
            << "\n num_processed_constraints: " << num_processed_constraints
            << "\n num_error_fail: " << num_fail_due_to_large_error
            << "\n num_multiplier_fail: " << num_fail_due_to_large_multiplier;
    if (logger != nullptr) {
      SOLVER_LOG(logger, "Missed ", num_missed, " potential implied integer.");
    }
  }

  if (logger != nullptr && (num_implied_integers > 0 || num_missed > 0)) {
    const int num_integers = initial_num_integers + num_implied_integers;
    SOLVER_LOG(logger, "[ImpliedIntegers] Num integers: ", num_integers, "/",
               num_variables, " (implied:", num_implied_integers,
               " missed:", num_missed, " max_scaling:", max_scaling,
               " max_error:", max_unscaled_error,
               " obj_error:", objective_error_bound, ")");
  }
  std::sort(defining_constraints.begin(), defining_constraints.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
  return defining_constraints;
}

namespace {

// Bounds `target_integer - scaling * original_bound`, clamped to contain 0.
// Returns `std::nullopt` on overflow.
std::optional<std::pair<double, double>> ComputeSnappedBoundError(
    double target_integer, double scaling, double original_bound) {
  if (!std::isfinite(target_integer)) return std::nullopt;
  const auto [error_lb, error_ub] = TightSum()
                                        .Add(target_integer)
                                        .AddProd(-scaling, original_bound)
                                        .TightBounds();
  if (!std::isfinite(error_lb) || !std::isfinite(error_ub)) {
    return std::nullopt;
  }
  return std::make_pair(std::min(0.0, error_lb), std::max(0.0, error_ub));
}

// Discretization error bounds on `discretized_var - var_scaling * var` for a
// single-variable component, both per constraint row and aggregated for `var`'s
// state. All per-row vectors have size `constraint_indices.size()`.
struct OneVarRowErrors {
  // False if certified finite error bounds could not be established (for
  // example under `proper_relaxation` when an external variable with a non-zero
  // remainder has an infinite domain, or when an intermediate bound overflows).
  bool valid = true;

  // Whether each constraint's original lower or upper bound absorbs both
  // `bound_tolerance` and external variables' discretization errors when `var`
  // is snapped against its active bound (only true when that original bound is
  // finite; both can be true on a unique non-inverted two-sided active row
  // whose snapped integer target always dominates `var`'s active domain bound).
  std::vector<bool> lb_is_active;
  std::vector<bool> ub_is_active;

  // Per-constraint outer bounds `[row_error_lb[r], row_error_ub[r]]` (with
  // `row_error_lb[r] <= 0 <= row_error_ub[r]`) on `var`'s contribution to pass
  // to `ConstraintScaler` when converting constraint `r` (excluding external
  // discretization errors that cancel on active sides or with
  // `ConstraintScaler`'s own external-variable error accumulation).
  std::vector<double> row_error_lb;
  std::vector<double> row_error_ub;

  // Component-wide outer bounds `[error_lb, error_ub]` (with
  // `error_lb <= 0 <= error_ub`) on `discretized_var - var_scaling * var`
  // across all constraints and `var`'s own domain bounds.
  double error_lb = 0.0;
  double error_ub = 0.0;

  // Worst-case `|discretized_var - var_scaling * var|` for an exact feasible
  // solution (`bound_tolerance = 0`), used to bound the objective error on
  // exact solutions.
  double max_exact_error = 0.0;
};

// Computes per-row and component-wide discretization error bounds on
// `discretized_var - var_scaling * var` across `rows` in the pre-scaling
// `states` space (where `states[var].scaling == 1.0` and
// `states[var].shift == 0.0`). Because `y_var - x'_var = z_var - s * x_var` is
// invariant under scaling `var` by `var_scaling` and integer shifting, these
// bounds apply directly when converting the constraints after finalization.
OneVarRowErrors ComputeOneVarRowErrors(
    const MPModelProtoWrapper& mp_model,
    absl::Span<const int> constraint_indices, absl::Span<const LinearRow> rows,
    absl::Span<const double> row_lb_upper,
    absl::Span<const double> row_ub_lower,
    absl::Span<const MipVariableState> states, int var, double var_scaling,
    bool snap_to_lower_bound, double bound_tolerance, bool proper_relaxation,
    std::pair<double, double> var_bound_error) {
  const int num_rows = constraint_indices.size();
  OneVarRowErrors result;
  result.lb_is_active.assign(num_rows, false);
  result.ub_is_active.assign(num_rows, false);
  result.row_error_lb.assign(num_rows, 0.0);
  result.row_error_ub.assign(num_rows, 0.0);
  result.max_exact_error =
      std::max(-var_bound_error.first, var_bound_error.second);

  // Substitute `z = snap_to_lower_bound ? var : -var` and negate any row where
  // `z` has a negative coefficient. In `z`-coordinates, every row has a
  // positive coefficient on `z` and `z` is always snapped upward to its lower
  // bounds (`z >= ...`), flipping `[error_lb, error_ub]` back to `var` at the
  // end when `!snap_to_lower_bound`.
  const auto [z_var_bound_error_lb, z_var_bound_error_ub] =
      snap_to_lower_bound
          ? var_bound_error
          : std::make_pair(-var_bound_error.second, -var_bound_error.first);

  std::vector<bool> row_has_lb(num_rows, false);
  std::vector<bool> row_has_ub(num_rows, false);
  std::vector<bool> row_was_negated_vec(num_rows, false);
  std::vector<bool> row_is_inverted(num_rows, false);
  std::vector<bool> row_absorbs_active_ext_err(num_rows, false);
  std::vector<double> row_int_error_ub(num_rows, 0.0);
  std::vector<double> row_relaxed_error_ub(num_rows, 0.0);
  std::vector<double> row_min_snapped_z(
      num_rows, -std::numeric_limits<double>::infinity());
  int num_active_rows = 0;
  double z_error_lb = z_var_bound_error_lb;
  double max_row_exact_error_ub = 0.0;
  double largest_relaxed_error_ub = 0.0;
  int largest_row = -1;
  double second_largest_relaxed_error_ub = 0.0;

  std::vector<double> normalized_coeffs;
  for (int r = 0; r < num_rows; ++r) {
    const EnforcedLinearConstraintProto& mp_constraint =
        mp_model.constraints(constraint_indices[r]);
    const LinearRow& row = rows[r];
    int var_term = -1;
    for (int i = 0; i < row.vars.size(); ++i) {
      if (row.vars[i] == var) {
        var_term = i;
        break;
      }
    }
    if (var_term == -1 || row.coeffs[var_term] == 0.0) continue;

    // Express the row in terms of `z` with a positive coefficient on `z`.
    normalized_coeffs.assign(row.coeffs.begin(), row.coeffs.end());
    if (!snap_to_lower_bound) {
      normalized_coeffs[var_term] = -normalized_coeffs[var_term];
    }
    const bool row_was_negated = normalized_coeffs[var_term] < 0.0;
    if (row_was_negated) {
      for (double& coeff : normalized_coeffs) coeff = -coeff;
    }
    const bool has_lb =
        std::isfinite(row_was_negated ? mp_constraint.upper_bound()
                                      : mp_constraint.lower_bound());
    const bool has_ub =
        std::isfinite(row_was_negated ? mp_constraint.lower_bound()
                                      : mp_constraint.upper_bound());
    const double lower_bound = row_was_negated ? -row.ub : row.lb;
    const double lower_bound_upper =
        row_was_negated ? -row_ub_lower[r] : row_lb_upper[r];
    const double upper_bound = row_was_negated ? -row.lb : row.ub;

    row_has_lb[r] = has_lb;
    row_has_ub[r] = has_ub;
    row_was_negated_vec[r] = row_was_negated;
    row_is_inverted[r] =
        mp_constraint.lower_bound() > mp_constraint.upper_bound();
    if (!has_lb) continue;
    ++num_active_rows;

    if (row_was_negated) {
      result.ub_is_active[r] = true;
    } else {
      result.lb_is_active[r] = true;
    }

    const LinearRow normalized_row{.vars = row.vars,
                                   .coeffs = normalized_coeffs,
                                   .lb = lower_bound,
                                   .ub = upper_bound};
    double relaxed_error_ub = 0.0;
    std::pair<double, double> internal_error = {-0.5, 0.5};
    double min_snapped_z = -std::numeric_limits<double>::infinity();
    const std::optional<std::pair<double, double>> exact_error =
        TryComputeSingleVarConstraintErrorBounds(
            normalized_row, states, var, var_scaling, lower_bound,
            lower_bound_upper,
            {.snapped_integer_bounds = true,
             .rhs_lb_tolerance = bound_tolerance,
             .relaxed_error_ub = &relaxed_error_ub,
             .internal_lb_upper = lower_bound_upper,
             .internal_ub_lower = lower_bound,
             .internal_error = &internal_error,
             .min_snapped_var = &min_snapped_z});
    if (!exact_error.has_value() && proper_relaxation) {
      result.valid = false;
      return result;
    }
    const auto [exact_error_lb, exact_error_ub] =
        exact_error.value_or(std::make_pair(-0.5, 0.5));
    if (!exact_error.has_value()) {
      relaxed_error_ub = 0.5;
      internal_error = {-0.5, 0.5};
      min_snapped_z = -std::numeric_limits<double>::infinity();
    }
    const auto [int_error_lb, int_error_ub] = internal_error;
    result.max_exact_error =
        std::max({result.max_exact_error, -exact_error_lb, exact_error_ub});

    if (!std::isfinite(exact_error_lb) || !std::isfinite(relaxed_error_ub) ||
        !std::isfinite(int_error_lb) || !std::isfinite(int_error_ub)) {
      result.valid = false;
      return result;
    }
    row_absorbs_active_ext_err[r] = exact_error.has_value();
    result.row_error_lb[r] = int_error_lb;
    row_int_error_ub[r] = int_error_ub;
    row_relaxed_error_ub[r] = relaxed_error_ub;
    row_min_snapped_z[r] = min_snapped_z;
    z_error_lb = std::min(z_error_lb, exact_error_lb);

    max_row_exact_error_ub = std::max(max_row_exact_error_ub, exact_error_ub);
    if (relaxed_error_ub >= largest_relaxed_error_ub) {
      second_largest_relaxed_error_ub = largest_relaxed_error_ub;
      largest_relaxed_error_ub = relaxed_error_ub;
      largest_row = r;
    } else if (relaxed_error_ub > second_largest_relaxed_error_ub) {
      second_largest_relaxed_error_ub = relaxed_error_ub;
    }
  }

  const double z_domain_lb =
      snap_to_lower_bound ? states[var].lb : -states[var].ub;
  double z_dom_lo_ub = -std::numeric_limits<double>::infinity();
  if (std::isfinite(z_domain_lb)) {
    const double scaled_z_lb = ProdBounds(z_domain_lb, var_scaling).first;
    z_dom_lo_ub = std::isfinite(scaled_z_lb)
                      ? std::ceil(scaled_z_lb)
                      : std::numeric_limits<double>::infinity();
  }

  double max_row_error_ub = 0.0;
  for (int r = 0; r < num_rows; ++r) {
    if (!row_has_ub[r]) continue;
    // For a non-inverted two-sided row `L <= a * z + e <= U` (`L <= U`),
    // snapping `z` to the row's own lower bound in discretized external space
    // cancels external errors and keeps `a * z + e` within
    // `row_int_error_ub[r]` of `L <= U` regardless of `bound_tolerance`. If no
    // other active row or active domain bound can push `z` higher, the opposite
    // side also absorbs both `bound_tolerance` and external discretization
    // errors.
    const bool opposite_side_is_exact =
        row_has_lb[r] && !row_is_inverted[r] && row_absorbs_active_ext_err[r] &&
        num_active_rows == 1 && std::isfinite(row_min_snapped_z[r]) &&
        z_dom_lo_ub <= row_min_snapped_z[r];
    if (opposite_side_is_exact) {
      result.row_error_ub[r] = row_int_error_ub[r];
      if (row_was_negated_vec[r]) {
        result.lb_is_active[r] = true;
      } else {
        result.ub_is_active[r] = true;
      }
    } else {
      const double same_row_error_ub = !row_has_lb[r] ? 0.0
                                       : row_is_inverted[r]
                                           ? row_relaxed_error_ub[r]
                                           : row_int_error_ub[r];
      const double other_rows_error_ub = (r == largest_row)
                                             ? second_largest_relaxed_error_ub
                                             : largest_relaxed_error_ub;
      result.row_error_ub[r] = std::max(
          {z_var_bound_error_ub, same_row_error_ub, other_rows_error_ub});
    }
    max_row_error_ub = std::max(max_row_error_ub, result.row_error_ub[r]);
  }

  const double max_ub_without_tolerance =
      std::max(z_var_bound_error_ub, max_row_exact_error_ub);
  const double max_ub_with_tolerance =
      std::max(z_var_bound_error_ub, largest_relaxed_error_ub);
  double z_error_ub = std::max(max_ub_without_tolerance, max_row_error_ub);
  const double z_domain_ub =
      snap_to_lower_bound ? states[var].ub : -states[var].lb;
  if (bound_tolerance != 0.0 && std::isfinite(z_domain_ub)) {
    const double max_snapped = std::floor(ExactSumUb(
        ProdBounds(z_domain_ub, var_scaling).second, max_ub_with_tolerance));
    if (max_snapped > ExactSumLb(ProdBounds(z_domain_ub, var_scaling).first,
                                 max_ub_without_tolerance)) {
      z_error_ub = std::max(z_error_ub, max_ub_with_tolerance);
    }
  }

  result.error_lb = z_error_lb;
  result.error_ub = z_error_ub;

  if (!snap_to_lower_bound) {
    const auto negate = [](double v) { return v == 0.0 ? 0.0 : -v; };
    for (int r = 0; r < num_rows; ++r) {
      std::tie(result.row_error_lb[r], result.row_error_ub[r]) = std::make_pair(
          negate(result.row_error_ub[r]), negate(result.row_error_lb[r]));
    }
    std::tie(result.error_lb, result.error_ub) =
        std::make_pair(negate(result.error_ub), negate(result.error_lb));
  }
  return result;
}

}  // namespace

OneVariableComponentStatus DetectAndConvertOneVariableComponent(
    const SatParameters& params, const ResidualContinuousComponent& component,
    const MPModelProtoWrapper& mp_model, absl::Span<MipVariableState> states,
    ConstraintScaler* scaler, CpModelProto* cp_model,
    OneVariableComponentStats* stats, SolverLogger* logger) {
  CHECK_EQ(component.continuous_vars.size(), 1);
  CHECK(scaler != nullptr);
  CHECK(cp_model != nullptr);
  DCHECK_EQ(scaler->use_proper_relaxation, params.mip_proper_relaxation());
  const int var = component.continuous_vars[0];
  CHECK_GE(var, 0);
  CHECK_LT(var, states.size());
  CHECK_LT(var, cp_model->variables_size());
  MipVariableState& state = states[var];
  CHECK(!state.is_integer);
  CHECK_EQ(state.scaling, 1.0);
  ++stats->num_components;
  if (params.keep_all_feasible_solutions_in_presolve()) {
    return OneVariableComponentStatus::kNotConverted;
  }

  const double tolerance = params.mip_wanted_precision();
  const double max_multiplier =
      MaxImpliedIntegerMultiplier(params, state.lb, state.ub);

  // The objective coefficient of var in the (minimization) problem.
  const double objective_coeff =
      mp_model.variables(var).objective_coefficient();
  const double bound_tolerance = params.mip_tight_precision();

  // Transform the rows in the current variable space.
  // TODO(user): Populate `coeffs_lb` / `coeffs_ub` here as well once mirrored
  // in the Lean `ScaleOneVariableResidualMatrix` proof.
  const int num_rows = component.constraints.size();
  std::vector<std::vector<double>> row_coeffs(num_rows);
  std::vector<LinearRow> rows(num_rows);
  std::vector<double> row_lb_upper(num_rows);
  std::vector<double> row_ub_lower(num_rows);
  for (int r = 0; r < num_rows; ++r) {
    const EnforcedLinearConstraintProto& constraint =
        mp_model.constraints(component.constraints[r]);
    TransformConstraintWithBounds(constraint, states, &row_coeffs[r],
                                  /*coeffs_lb=*/nullptr, /*coeffs_ub=*/nullptr,
                                  &rows[r].lb, &rows[r].ub, &row_lb_upper[r],
                                  &row_ub_lower[r]);
    rows[r].vars = constraint.vars();
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
    const auto [lb_multiplier, ub_multiplier] =
        GetIntegralityMultiplier(row, var, tolerance, max_multiplier);
    if (lb_multiplier != 0.0) var_multipliers_ge.push_back(lb_multiplier);
    if (ub_multiplier != 0.0) var_multipliers_le.push_back(ub_multiplier);
  }
  double scaling_ge = kInfinity;
  double scaling_le = kInfinity;
  if (objective_coeff <= 0.0) {
    scaling_le = GetDoubleLcm(var_multipliers_le, tolerance, max_multiplier);
  }
  if (objective_coeff >= 0.0) {
    scaling_ge = GetDoubleLcm(var_multipliers_ge, tolerance, max_multiplier);
  }
  if (scaling_ge == kInfinity && scaling_le == kInfinity) {
    ++stats->num_missed;
    return OneVariableComponentStatus::kNotConverted;
  }

  // At this point, all constraints only ever enforce an [int_lb, ...] or
  // [..., int_ub] interval on the SCALED variable. We still have a couple of
  // corner cases to deal with.

  const auto compute_max_row_error = [&](double candidate_scaling,
                                         bool snap_to_lower_bound) {
    double max_error = 0.0;
    for (int r = 0; r < num_rows; ++r) {
      const LinearRow& row = rows[r];
      const double var_coeff = GetVarCoeff(row, var);
      if (var_coeff == 0.0) continue;
      const EnforcedLinearConstraintProto& constraint =
          mp_model.constraints(component.constraints[r]);
      const bool lb_is_active = (snap_to_lower_bound == (var_coeff > 0.0));
      const double original_active_rhs =
          lb_is_active ? constraint.lower_bound() : constraint.upper_bound();
      if (!std::isfinite(original_active_rhs)) continue;
      const double rhs_lb =
          lb_is_active
              ? (bound_tolerance != 0.0 ? ExactSumLb(row.lb, -bound_tolerance)
                                        : row.lb)
              : row_ub_lower[r];
      const double rhs_ub =
          lb_is_active
              ? row_lb_upper[r]
              : (bound_tolerance != 0.0 ? ExactSumUb(row.ub, bound_tolerance)
                                        : row.ub);
      const std::optional<std::pair<double, double>> row_error =
          TryComputeSingleVarConstraintErrorBounds(
              row, states, var, candidate_scaling, rhs_lb, rhs_ub,
              {.snapped_integer_bounds = true});
      if (!row_error.has_value() && params.mip_proper_relaxation()) {
        return kInfinity;
      }
      const auto [error_lb, error_ub] =
          row_error.value_or(std::make_pair(-0.5, 0.5));
      max_error = std::max({max_error, -error_lb, error_ub});
    }
    return max_error;
  };

  // First corner case: If the scaled domain does not contain an integer
  // point, then the domain of the variable will always be either fully
  // feasible or fully infeasible on the snapped side! So we can just fix
  // that variable to the corresponding bound in its domain. No need to scale
  // at all.
  //
  // NOTE(user): This do not trigger on the miplib though.
  const auto domain_has_no_integer = [&](double candidate_scaling,
                                         bool snap_to_lower_bound) {
    if (!(candidate_scaling > 0.0 && candidate_scaling < kInfinity)) {
      return false;
    }
    if (params.mip_proper_relaxation()) {
      if (!std::isfinite(state.lb) || !std::isfinite(state.ub)) return false;
      const double row_error =
          compute_max_row_error(candidate_scaling, snap_to_lower_bound);
      if (!std::isfinite(row_error)) return false;
      const double scaled_var_lb =
          ExactSumLb(ProdBounds(state.lb, candidate_scaling).first, -row_error);
      const double scaled_var_ub =
          ExactSumUb(ProdBounds(state.ub, candidate_scaling).second, row_error);
      return std::ceil(scaled_var_lb) > std::floor(scaled_var_ub);
    }
    const double scaled_var_lb = (state.lb - tolerance) * candidate_scaling;
    const double scaled_var_ub = (state.ub + tolerance) * candidate_scaling;
    return std::ceil(scaled_var_lb) > std::floor(scaled_var_ub);
  };
  const bool fix_to_lb =
      domain_has_no_integer(scaling_ge, /*snap_to_lower_bound=*/true);
  const bool fix_to_ub =
      domain_has_no_integer(scaling_le, /*snap_to_lower_bound=*/false);
  if (fix_to_lb || fix_to_ub) {
    double target = fix_to_lb ? state.lb : state.ub;
    if (fix_to_lb && fix_to_ub && std::abs(state.ub) < std::abs(target)) {
      target = state.ub;
    }
    ++stats->num_fixed;
    state.lb = target;
    state.ub = target;
    return OneVariableComponentStatus::kNotConverted;
  }

  // Second corner case: when snapping to the lower (or upper) bound, if
  // there is an objective or opposite-side constraints, the variable's own
  // lower (or upper) bound can be the active bound and must also be integer
  // after scaling so we don't cut off the best/only feasible solution.
  //
  // TODO(user): There is a bad interaction with the bound propagator that
  // propagates bound using tolerances, so that they are not exactly integer
  // :( find a cleaner way ?
  const auto scale_for_bound = [&](double candidate_scaling, double bound) {
    if (candidate_scaling == kInfinity || !std::isfinite(bound) ||
        max_multiplier < 1.0) {
      return candidate_scaling == kInfinity || max_multiplier < 1.0
                 ? kInfinity
                 : candidate_scaling;
    }
    const int64_t int_limit =
        max_multiplier >= static_cast<double>(kint64max)
            ? kint64max
            : CapAdd(static_cast<int64_t>(std::floor(max_multiplier)), 1);
    candidate_scaling *= FindRationalFactor(
        candidate_scaling * bound, int_limit, candidate_scaling * tolerance);
    if (candidate_scaling == 0 || candidate_scaling > max_multiplier) {
      return kInfinity;
    }
    const double scaled = bound * candidate_scaling;
    if (!IsIntegerWithinTolerance(scaled, tolerance * candidate_scaling)) {
      return kInfinity;
    }
    return candidate_scaling;
  };
  const bool need_lb = objective_coeff > 0.0 || !var_multipliers_le.empty();
  const bool need_ub = objective_coeff < 0.0 || !var_multipliers_ge.empty();
  if (need_lb) scaling_ge = scale_for_bound(scaling_ge, state.lb);
  if (need_ub) scaling_le = scale_for_bound(scaling_le, state.ub);
  const double scaling = std::min(scaling_ge, scaling_le);
  if (!(scaling > 0.0 && scaling < kInfinity)) {
    ++stats->num_missed;
    return OneVariableComponentStatus::kNotConverted;
  }

  // Bound `discretized_var - scaling * var` per row when snapping `var` to the
  // tightest active bound (or to its own bound).
  const bool snap_to_lower_bound = (scaling == scaling_ge);
  const bool has_active_var_bound = snap_to_lower_bound ? need_lb : need_ub;
  const double original_active_var_bound =
      snap_to_lower_bound ? state.lb : state.ub;

  std::pair<double, double> var_bound_error = {0.0, 0.0};
  if (has_active_var_bound && std::isfinite(original_active_var_bound)) {
    const std::optional<std::pair<double, double>> snapped_error =
        ComputeSnappedBoundError(
            std::round(scaling * original_active_var_bound), scaling,
            original_active_var_bound);
    if (!snapped_error.has_value()) {
      ++stats->num_missed;
      return OneVariableComponentStatus::kNotConverted;
    }
    var_bound_error = *snapped_error;
  }

  const OneVarRowErrors row_errors = ComputeOneVarRowErrors(
      mp_model, component.constraints, rows, row_lb_upper, row_ub_lower, states,
      var, scaling, snap_to_lower_bound, bound_tolerance,
      params.mip_proper_relaxation(), var_bound_error);
  if (!row_errors.valid) {
    ++stats->num_missed;
    return OneVariableComponentStatus::kNotConverted;
  }

  const double error_lb = row_errors.error_lb;
  const double error_ub = row_errors.error_ub;
  const double max_exact_error = row_errors.max_exact_error;
  const double worst_error = std::max({max_exact_error, -error_lb, error_ub});
  if (!std::isfinite(worst_error)) {
    ++stats->num_missed;
    return OneVariableComponentStatus::kNotConverted;
  }

  // With mip_scale_for_precision, compare with the power of two scaling that
  // ScaleComponentContinuousVariablesForGivenPrecision() would use, on the
  // worst case error on var (in its original space).
  if (params.mip_scale_for_precision()) {
    const double pow2_scaling =
        ComputePrecisionScalings(params, mp_model, component, states,
                                 PrecisionHeadroomMode::kPerTerm)[0];
    const auto [pow2_error_lb, pow2_error_ub] =
        GetContinuousVarDiscretizationErrorBounds(
            params.mip_proper_relaxation(), objective_coeff);
    const double pow2_error =
        std::max(-pow2_error_lb, pow2_error_ub) / pow2_scaling;
    if (worst_error / scaling > pow2_error) {
      ++stats->num_pow2_preferred;
      return OneVariableComponentStatus::kNotConverted;
    }
  }

  // Scale `var`, snap its bounds to integers, shift toward zero, and write its
  // CP-SAT domain before converting the component's constraints.
  const MipVariableState original_state = state;
  state.error_lb = error_lb;
  state.error_ub = error_ub;
  state.is_integer = true;
  ScaleVariableState(scaling, &state);
  int64_t bound_changes = 0;
  ShiftStats shift_stats;
  const VariableConversionStatus conversion_status =
      FinalizeAndConvertVariables(params, {var}, mp_model, states, cp_model,
                                  &bound_changes, &shift_stats,
                                  /*num_small_domains=*/nullptr, logger);
  if (conversion_status != VariableConversionStatus::kConverted) {
    state = original_state;
    return conversion_status == VariableConversionStatus::kInfeasible
               ? OneVariableComponentStatus::kInfeasible
               : OneVariableComponentStatus::kInvalid;
  }
  IntegerVariableProto* cp_var = cp_model->mutable_variables(var);

  const int old_num_constraints = cp_model->constraints_size();
  const double active_bound_tolerance =
      scaler->use_proper_relaxation ? 0.0 : bound_tolerance;
  for (int r = 0; r < num_rows; ++r) {
    const EnforcedLinearConstraintProto& mp_constraint =
        mp_model.constraints(component.constraints[r]);
    const ConstraintScalingOptions options{
        .lb_bound_tolerance = row_errors.lb_is_active[r]
                                  ? active_bound_tolerance
                                  : bound_tolerance,
        .ub_bound_tolerance = row_errors.ub_is_active[r]
                                  ? active_bound_tolerance
                                  : bound_tolerance,
        .lb_only_error_var =
            scaler->use_proper_relaxation && row_errors.lb_is_active[r] ? var
                                                                        : -1,
        .ub_only_error_var =
            scaler->use_proper_relaxation && row_errors.ub_is_active[r] ? var
                                                                        : -1,
        .override_error_var = var,
        .override_error_lb = row_errors.row_error_lb[r],
        .override_error_ub = row_errors.row_error_ub[r],
    };
    const absl::Status status =
        scaler->ScaleAndAddConstraint(mp_constraint, states, cp_model, options);
    if (!status.ok()) {
      if (logger != nullptr) SOLVER_LOG(logger, status.message());
      cp_model->mutable_constraints()->DeleteSubrange(
          old_num_constraints,
          cp_model->constraints_size() - old_num_constraints);
      cp_var->clear_domain();
      state = original_state;
      return OneVariableComponentStatus::kInvalid;
    }
  }

  if (objective_coeff != 0.0) {
    const double scaled_abs_objective =
        QuotientBounds(std::abs(objective_coeff), scaling).second;
    if (max_exact_error > 0.0) {
      const double objective_rounding_error =
          ExactProdUb(scaled_abs_objective, max_exact_error);
      stats->objective_error_bound =
          ExactSumUb(stats->objective_error_bound, objective_rounding_error);
    }
    double component_sensitivity = 0.0;
    for (int r = 0; r < num_rows; ++r) {
      const double var_coeff = GetVarCoeff(rows[r], var);
      if (var_coeff == 0.0) continue;
      if (!row_errors.lb_is_active[r] && !row_errors.ub_is_active[r]) {
        continue;
      }
      component_sensitivity = std::max(
          component_sensitivity,
          QuotientBounds(std::abs(objective_coeff), std::abs(var_coeff))
              .second);
    }
    stats->max_violation_sensitivity =
        std::max(stats->max_violation_sensitivity, component_sensitivity);
  }

  ++stats->num_integers;
  stats->num_int_bound_changes += bound_changes;
  stats->shift_stats.num_shifted += shift_stats.num_shifted;
  stats->shift_stats.max_shift =
      std::max(stats->shift_stats.max_shift, shift_stats.max_shift);
  stats->max_scaling = std::max(stats->max_scaling, scaling);
  return OneVariableComponentStatus::kConverted;
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

absl::Status ConstraintScaler::AddAlreadyScaledConstraint(
    const EnforcedLinearConstraintProto& mp_constraint,
    absl::Span<const MipVariableState> states,
    const ScaledLinearConstraint& scaled, CpModelProto* cp_model) {
  absl::Span<const IntegerVariableProto* const> var_domains =
      absl::MakeSpan(cp_model->variables());
  bool is_never_enforced = false;
  const absl::Status status = TransformEnforcementValues(
      mp_constraint, states, var_domains, &is_never_enforced);
  if (!status.ok() || is_never_enforced) return status;

  ConstraintProto* constraint = cp_model->add_constraints();
  if (keep_names && !mp_constraint.name().empty()) {
    constraint->set_name(mp_constraint.name());
  }
  LinearConstraintProto* linear = constraint->mutable_linear();
  max_discretization_error =
      std::max(max_discretization_error, scaled.discretization_error);
  if (scaled.lb <= scaled.ub) {
    max_total_unscaling_error =
        std::max(max_total_unscaling_error, scaled.total_unscaling_error);
  }
  if (scaled.lb == kint64min && scaled.ub == kint64max) {
    ++num_trivial_constraints;
    linear->add_domain(0);
    linear->add_domain(0);
    return absl::OkStatus();
  }
  max_relative_coeff_error =
      std::max(max_relative_coeff_error, scaled.relative_coeff_error);
  max_absolute_rhs_error =
      std::max(max_absolute_rhs_error, scaled.absolute_rhs_error);
  max_scaling_factor = std::max(max_scaling_factor, scaled.scaling_factor);
  min_scaling_factor = std::min(min_scaling_factor, scaled.scaling_factor);
  max_rounding_error = std::max(max_rounding_error, scaled.rounding_error);
  max_unrounding_error =
      std::max(max_unrounding_error, scaled.unrounding_error);

  // An infeasible constraint is written as `0 in {}`, like in
  // ScaleAndAddConstraint().
  int_lower_bounds.clear();
  int_upper_bounds.clear();
  if (scaled.lb <= scaled.ub) {
    linear->mutable_vars()->Assign(scaled.vars.begin(), scaled.vars.end());
    linear->mutable_coeffs()->Assign(scaled.coeffs.begin(),
                                     scaled.coeffs.end());
    linear->add_domain(scaled.lb);
    linear->add_domain(scaled.ub);
    int_lower_bounds = scaled.var_lbs;
    int_upper_bounds = scaled.var_ubs;
  }
  return AddEnforcement(mp_constraint, var_domains, constraint);
}

absl::Status ConstraintScaler::TransformEnforcementValues(
    const EnforcedLinearConstraintProto& mp_constraint,
    absl::Span<const MipVariableState> states,
    absl::Span<const IntegerVariableProto* const> var_domains,
    bool* is_never_enforced) {
  // Enforcement values must be transformed too. This should only involve
  // integer shifts of integer variables.
  tmp_enforcement_values.clear();
  for (int i = 0; i < mp_constraint.enforcement_vars().size(); ++i) {
    const MipVariableState& state = states[mp_constraint.enforcement_vars()[i]];
    TightSum transformed_value;
    transformed_value.AddProd(state.scaling,
                              mp_constraint.enforcement_values()[i]);
    transformed_value.Add(-state.shift);
    const auto [int_floor, int_ceil] = transformed_value.Int64Bounds();
    if (!int_floor.has_value() || int_floor != int_ceil) {
      return absl::InvalidArgumentError(absl::StrCat(
          "[Scaling] Non-integer or overflowing transformed enforcement "
          "value. MpConstraintProto: ",
          ProtobufShortDebugString(mp_constraint)));
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
    if (value < var_lb || value > var_ub) *is_never_enforced = true;
  }
  return absl::OkStatus();
}

absl::Status ConstraintScaler::AddEnforcement(
    const EnforcedLinearConstraintProto& mp_constraint,
    absl::Span<const IntegerVariableProto* const> var_domains,
    ConstraintProto* constraint) {
  if (mp_constraint.enforcement_vars().empty()) return absl::OkStatus();

  // If the constraint is trivially satisfied, there is no need to add any
  // enforcement.
  const LinearConstraintProto& linear = constraint->linear();
  if (linear.vars().empty() && !linear.domain().empty() &&
      linear.domain(0) <= 0 && linear.domain(1) >= 0) {
    return absl::OkStatus();
  }

  bool has_integer_enforcement = false;
  const int num_enforcements = mp_constraint.enforcement_vars().size();
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
                     ProtobufShortDebugString(mp_constraint)));
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
    const EnforcedLinearConstraintProto& mp_constraint,
    absl::Span<const MipVariableState> states, CpModelProto* cp_model,
    const ConstraintScalingOptions& options) {
  absl::Span<const IntegerVariableProto* const> var_domains =
      absl::MakeSpan(cp_model->variables());
  double ct_lower_bound;
  double ct_upper_bound;
  double orig_lb_upper;
  double orig_ub_lower;
  TransformConstraintWithBounds(
      mp_constraint, states, &tmp_transformed_coeffs,
      &tmp_transformed_coeffs_lb, &tmp_transformed_coeffs_ub, &ct_lower_bound,
      &ct_upper_bound, &orig_lb_upper, &orig_ub_lower);

  bool is_never_enforced = false;
  const absl::Status enforcement_status = TransformEnforcementValues(
      mp_constraint, states, var_domains, &is_never_enforced);
  if (!enforcement_status.ok() || is_never_enforced) return enforcement_status;

  ConstraintProto* constraint = cp_model->add_constraints();
  if (keep_names && !mp_constraint.name().empty()) {
    constraint->set_name(mp_constraint.name());
  }

  // Constraint statistic used below.
  double min_activity = 0.0;
  double max_activity = 0.0;

  // Transform the constraint and its variable into an easier to use form.
  var_indices.clear();
  bounded_expr.Clear();
  coefficients_lb.clear();
  coefficients_ub.clear();
  int_lower_bounds.clear();
  int_upper_bounds.clear();
  tmp_discretization_lb.Clear();
  tmp_discretization_ub.Clear();
  tmp_exact_activity.Clear();
  const int num_coeffs = mp_constraint.vars().size();
  for (int i = 0; i < num_coeffs; ++i) {
    const double coeff = tmp_transformed_coeffs[i];
    const double c_lb = tmp_transformed_coeffs_lb[i];
    const double c_ub = tmp_transformed_coeffs_ub[i];
    if (c_lb == 0.0 && c_ub == 0.0) continue;
    const int var = mp_constraint.vars(i);
    const double err_lb = (var == options.override_error_var)
                              ? options.override_error_lb
                              : states[var].error_lb;
    const double err_ub = (var == options.override_error_var)
                              ? options.override_error_ub
                              : states[var].error_ub;
    TightSum* disc_lb =
        (options.lb_only_error_var < 0 || var == options.lb_only_error_var)
            ? &tmp_discretization_lb
            : nullptr;
    TightSum* disc_ub =
        (options.ub_only_error_var < 0 || var == options.ub_only_error_var)
            ? &tmp_discretization_ub
            : nullptr;
    AddTermErrorBounds(c_lb, c_ub, err_lb, err_ub, disc_lb, disc_ub);

    const auto& var_proto = var_domains[var];
    const int64_t lb = var_proto->domain(0);
    const int64_t ub = var_proto->domain(var_proto->domain_size() - 1);
    if (lb == 0 && ub == 0) continue;

    var_indices.push_back(var);
    bounded_expr.AddTerm(coeff, lb, ub);
    coefficients_lb.push_back(c_lb);
    coefficients_ub.push_back(c_ub);
    int_lower_bounds.push_back(lb);
    int_upper_bounds.push_back(ub);
    tmp_exact_activity.AddTerm(c_lb, c_ub, lb, ub);

    double min_term = coeff * static_cast<double>(lb);
    double max_term = coeff * static_cast<double>(ub);
    if (min_term > max_term) std::swap(min_term, max_term);

    min_activity += min_term;
    max_activity += max_term;
  }

  // We relax the constraint bounds by `bound_tol` before scaling: this is
  // needed because now that the scaled constraint activity is integer, we will
  // floor/ceil these bounds. It might make more sense to use a relative
  // precision here for large bounds, but absolute is usually what is used in
  // the MIP world, and if the problem is a pure integer problem and a user
  // asked for sum == 10k, we want to stay exact here.
  //
  // Given the bounds [disc_error_lb, disc_error_ub] on
  // sum_i coeff_i * (discretized_x_i - x_i), any feasible solution x of
  // ct_lower_bound <= sum_i coeff_i * x_i <= ct_upper_bound maps to a
  // discretized point satisfying:
  //   ct_lower_bound + disc_error_lb <= sum_i coeff_i * discretized_x
  //   sum_i coeff_i * discretized_x <= ct_upper_bound + disc_error_ub
  // When `use_proper_relaxation` is true, we also relax the bounds by this
  // error before checking min/max activity and before computing tight rounding
  // errors on the discretized domain.
  //
  // Since the activity of the discretized point is always within [min_activity,
  // max_activity], a side cannot be off by more than its distance to these
  // bounds. Note that this must be done before skipping trivial constraints,
  // otherwise a side that is relaxed so much that it becomes trivial would not
  // be accounted for.
  const double kInfinity = std::numeric_limits<double>::infinity();
  if (std::isfinite(ct_lower_bound)) {
    const double relaxed_lb =
        ExactSumLb(ct_lower_bound, -options.lb_bound_tolerance);
    const double disc_error_lb = tmp_discretization_lb.TightLowerBound();
    max_discretization_error =
        std::max(max_discretization_error,
                 std::min(-disc_error_lb, relaxed_lb - min_activity));
    ct_lower_bound = use_proper_relaxation
                         ? tmp_discretization_lb.Add(ct_lower_bound)
                               .Add(-options.lb_bound_tolerance)
                               .TightLowerBound()
                         : relaxed_lb;
    if (min_activity >= ct_lower_bound) ct_lower_bound = -kInfinity;
  }
  if (std::isfinite(ct_upper_bound)) {
    const double relaxed_ub =
        ExactSumUb(ct_upper_bound, options.ub_bound_tolerance);
    const double disc_error_ub = tmp_discretization_ub.TightUpperBound();
    max_discretization_error =
        std::max(max_discretization_error,
                 std::min(disc_error_ub, max_activity - relaxed_ub));
    ct_upper_bound = use_proper_relaxation
                         ? tmp_discretization_ub.Add(ct_upper_bound)
                               .Add(options.ub_bound_tolerance)
                               .TightUpperBound()
                         : relaxed_ub;
    if (max_activity <= ct_upper_bound) ct_upper_bound = kInfinity;
  }
  auto* mutable_linear = constraint->mutable_linear();
  if (ct_lower_bound == -kInfinity && ct_upper_bound == kInfinity) {
    ++num_trivial_constraints;
    max_total_unscaling_error =
        std::max({max_total_unscaling_error,
                  tmp_exact_activity.MaxLowerViolation(orig_lb_upper),
                  tmp_exact_activity.MaxUpperViolation(orig_ub_lower)});
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
      bounded_expr, scaling_target, wanted_precision, &relative_coeff_error,
      &scaled_sum_error);
  if (scaling_factor == 0.0) {
    return absl::InvalidArgumentError(
        absl::StrCat("[Scaling] Scaling factor of zero while scaling constraint"
                     " MpConstraintProto: ",
                     ProtobufShortDebugString(mp_constraint)));
  }

  max_relative_coeff_error =
      std::max(relative_coeff_error, max_relative_coeff_error);
  max_scaling_factor = std::max(scaling_factor, max_scaling_factor);
  min_scaling_factor = std::min(scaling_factor, min_scaling_factor);
  max_absolute_rhs_error =
      std::max(max_absolute_rhs_error, scaled_sum_error / scaling_factor);

  tight_error_helper.LoadUnscaledConstraint(bounded_expr.coeffs,
                                            coefficients_lb, coefficients_ub,
                                            int_lower_bounds, int_upper_bounds);
  tight_error_helper.LoadScalingFactor(scaling_factor);

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

  // Compute tighter rhs errors on the rounding, and relax the scaled bounds by
  // the rounding error so that we never lose a feasible solution of the
  // original problem that is within `bound_tol`. However, unless
  // `use_proper_relaxation` is set, we don't relax by more than
  // `wanted_precision` if we are not precise, and just hope for the best.
  double lb_error1 = 0.0;
  if (has_lb) {
    const TightSum& lb_err =
        tight_error_helper.GetRelaxationLowerBoundError(tmp_scaled_lb);
    lb_error1 = -lb_err.TightLowerBound() / scaling_factor;
    if (use_proper_relaxation || lb_error1 <= wanted_precision) {
      tmp_scaled_lb.Add(lb_err);
    } else {
      tmp_scaled_lb.AddProd(-wanted_precision, scaling_factor);
    }
  }
  double ub_error1 = 0.0;
  if (has_ub) {
    const TightSum& ub_err =
        tight_error_helper.GetRelaxationUpperBoundError(tmp_scaled_ub);
    ub_error1 = ub_err.TightUpperBound() / scaling_factor;
    if (use_proper_relaxation || ub_error1 <= wanted_precision) {
      tmp_scaled_ub.Add(ub_err);
    } else {
      tmp_scaled_ub.AddProd(wanted_precision, scaling_factor);
    }
  }
  max_rounding_error = std::max({max_rounding_error, lb_error1, ub_error1});

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
    // Note that the unrounding and total unscaling errors are evaluated at the
    // emitted CP-SAT integer bounds `gcd * final_lb` and `gcd * final_ub`:
    // these bound the worst violation we can make when scaling back a CP-SAT
    // feasible solution, though an optimal solution might still lie well within
    // the original bound when unscaled.
    max_total_unscaling_error =
        std::max(max_total_unscaling_error,
                 ComputeBoundedRowTotalUnscalingError(
                     scaling_factor, gcd, final_lb, final_ub, orig_lb_upper,
                     orig_ub_lower, tmp_exact_activity, &tight_error_helper,
                     &max_unrounding_error));
    mutable_linear->add_domain(final_lb);
    mutable_linear->add_domain(final_ub);
  }

  return AddEnforcement(mp_constraint, var_domains, constraint);
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

bool ClampVariableBounds(const SatParameters& params,
                         MPModelProtoWrapper* mp_model, SolverLogger* logger,
                         int* num_clamped) {
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
  const int num_variables = mp_model->num_variables();
  for (int v = 0; v < num_variables; ++v) {
    auto& mp_var = *mp_model->mutable_variables(v);
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
  const double precision = params.mip_tight_precision();

  int64_t num_enforcements = 0;
  int64_t num_integer_enforcements = 0;
  double max_boolean_enforcement_magnitude = 0.0;
  double max_integer_enforcement_magnitude = 0.0;

  TightSum min_activity;
  TightSum max_activity;
  TightSum shift;
  std::vector<int> to_remove;
  std::vector<int64_t> enforcement_values;
  const int num_constraints = mp_model->num_constraints();
  for (int c = 0; c < num_constraints; ++c) {
    EnforcedLinearConstraintProto* ct = mp_model->mutable_constraints(c);

    double ct_lb = ct->lower_bound();
    double ct_ub = ct->upper_bound();
    if (ct_lb == -kInfinity && ct_ub == kInfinity) continue;

    bool skip_constraint = false;

    // Only compute the activity bound(s) of the finite side(s).
    const bool need_min = ct_lb != -kInfinity;
    const bool need_max = ct_ub != kInfinity;
    if (need_min) min_activity.Clear();
    if (need_max) max_activity.Clear();
    double max_integer_magnitude = 0.0;
    const int num_terms = ct->vars().size();
    for (int i = 0; i < num_terms; ++i) {
      const int var = ct->vars(i);
      const double coeff = ct->coeffs(i);
      if (coeff == 0.0) continue;

      const MipVariableProto& mp_var = mp_model->variables(var);
      const double lb = mp_var.lower_bound();
      const double ub = mp_var.upper_bound();

      const double min_bound = coeff > 0 ? lb : ub;
      const double max_bound = coeff > 0 ? ub : lb;

      if ((need_min && !std::isfinite(min_bound)) ||
          (need_max && !std::isfinite(max_bound))) {
        // This is unlikely, since bound propagation should have already
        // occurred.
        skip_constraint = true;
        break;
      }
      if (mp_var.is_integer() && lb < ub) {
        max_integer_magnitude =
            std::max(max_integer_magnitude, std::abs(coeff));
      }
      if (need_min) min_activity.AddProd(coeff, min_bound);
      if (need_max) max_activity.AddProd(coeff, max_bound);
    }

    if (skip_constraint) continue;

    // Relax bounds and skip trivial constraint. The comparisons are exact.
    if (need_min && min_activity.Add(precision).Add(-ct_lb) >= 0.0) {
      ct_lb = -kInfinity;
    }
    if (need_max && max_activity.Add(-precision).Add(-ct_ub) <= 0.0) {
      ct_ub = kInfinity;
    }
    if (ct_lb == -kInfinity && ct_ub == kInfinity) continue;

    // If the constraint is one-sided, we try to extract enforcement literals
    // before scaling !! This allows scaling the rest of the constraint better.
    // Especially since on some problems (like neos-3421095-cinca.mps) big-M
    // values are way too high (1e17 instead of 1e6).
    if (ct_lb != -kInfinity && ct_ub != kInfinity) continue;

    // Canonicalize to >= lb. Note that it shouldn't really matter that we
    // negate the constraint for CP-SAT.
    const bool is_upper_bounded = ct_lb == -kInfinity;
    if (is_upper_bounded) {
      for (int i = 0; i < num_terms; ++i) {
        ct->set_coeffs(i, -ct->coeffs(i));
      }
      ct_lb = -ct_ub;
      ct_ub = kInfinity;
    } else {
      CHECK_EQ(ct_ub, kInfinity);
    }
    ct->set_lower_bound(ct_lb);
    ct->set_upper_bound(ct_ub);

    // A term can be extracted if |coeff| >= (ct_lb - precision) - min_activity.
    // Since |coeff| is a double, this is equivalent to |coeff| >= limit where
    // limit is the exact difference rounded up. Note that `min_activity` holds
    // `orig_min_activity + precision - ct_lb` and `max_activity` holds
    // `orig_max_activity - precision - orig_ct_ub == (ct_lb - precision) -
    // (-orig_max_activity)`.
    to_remove.clear();
    enforcement_values.clear();
    const double limit = is_upper_bounded
                             ? max_activity.TightUpperBound()
                             : min_activity.Negate().TightUpperBound();
    DCHECK_GT(limit, 0.0);
    if (max_integer_magnitude < limit) continue;

    // The bound shift sum(coeff * value) over the extracted terms.
    shift.Clear();
    for (int i = 0; i < num_terms; ++i) {
      const int var = ct->vars(i);
      const double coeff = ct->coeffs(i);
      if (std::abs(coeff) < limit) continue;

      // We only extract integer enforcement !!
      const MipVariableProto& mp_var = mp_model->variables(var);
      if (!mp_var.is_integer()) continue;

      // The bounds should be integer.
      const double double_lb = mp_var.lower_bound();
      const double double_ub = mp_var.upper_bound();
      if (double_lb >= double_ub) continue;
      if (double_lb != std::round(double_lb)) continue;
      if (double_ub != std::round(double_ub)) continue;
      const int64_t lb = SafeDoubleToInt64(double_lb);
      const int64_t ub = SafeDoubleToInt64(double_ub);
      if (AtMinOrMaxInt64(lb) || AtMinOrMaxInt64(ub)) continue;
      const int64_t value = coeff > 0.0 ? lb : ub;
      shift.AddProd(coeff, value);
      enforcement_values.push_back(value);
      to_remove.push_back(i);
    }
    if (to_remove.empty()) continue;

    // We round the shifted bound down so that no feasible point is lost. We do
    // not extract anything if it is not finite.
    const double new_lb = shift.Negate().Add(ct_lb).TightLowerBound();
    if (!std::isfinite(new_lb)) continue;
    ct->set_lower_bound(new_lb);

    int new_size = 0;
    int r = 0;
    for (int i = 0; i < num_terms; ++i) {
      if (r < to_remove.size() && to_remove[r] == i) {
        const int var = ct->vars(i);
        const double coeff = ct->coeffs(i);
        const MipVariableProto& mp_var = mp_model->variables(var);
        if (mp_var.lower_bound() != 0.0 || mp_var.upper_bound() != 1.0) {
          ++num_integer_enforcements;
          max_integer_enforcement_magnitude =
              std::max(max_integer_enforcement_magnitude, std::abs(coeff));
        } else {
          ++num_enforcements;
          max_boolean_enforcement_magnitude =
              std::max(max_boolean_enforcement_magnitude, std::abs(coeff));
        }
        AddEnforcement(var, enforcement_values[r], ct);
        ++r;
        continue;
      }
      ct->set_vars(new_size, ct->vars(i));
      ct->set_coeffs(new_size, ct->coeffs(i));
      ++new_size;
    }
    ResizeConstraintTerms(new_size, ct);
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

void ShiftVariable(double offset, MipVariableState* state, ShiftStats* stats) {
  if (offset == 0.0) return;
  // Currently no caller shifts an already-shifted variable (after
  // ShiftBeforeScalingValue(), 0 is in [state->lb, state->ub] so
  // FinalCpSatShiftValue() returns 0). Assert this and skip in opt builds so
  // `state->shift` is never accumulated with rounding error.
  DCHECK_EQ(state->shift, 0.0);
  if (state->shift != 0.0) return;
  // We should never shift an integer variable by a non-integer value, this
  // is not correct.
  DCHECK(!state->is_integer || offset == std::round(offset));
  if (stats != nullptr) {
    ++stats->num_shifted;
    stats->max_shift = std::max(stats->max_shift, std::abs(offset));
  }
  state->lb = ExactSumLb(state->lb, -offset);
  state->ub = ExactSumUb(state->ub, -offset);
  state->shift = offset;
}

void ShiftVariables(
    const std::function<double(const MipVariableState&)>& get_shift,
    absl::Span<const int> vars, absl::Span<MipVariableState> states,
    ShiftStats* stats) {
  for (const int var : vars) {
    MipVariableState& state = states[var];
    ShiftVariable(get_shift(state), &state, stats);
  }
}

bool ConvertVariableToCpVariable(const SatParameters& params, int var_index,
                                 double objective_coefficient,
                                 MipVariableState* state,
                                 IntegerVariableProto* cp_var,
                                 int* num_small_domains) {
  const double max_var_bound = MaxCpSatVariableBound();
  const int64_t kSmallDomainSize = 1000;

  // Bounds must have been clamped by ClampVariableBounds() first.
  cp_var->clear_domain();
  const double var_lb = state->lb;
  const double var_ub = state->ub;
  if (!(std::abs(var_lb) <= max_var_bound &&
        std::abs(var_ub) <= max_var_bound)) {
    return false;
  }

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

VariableConversionStatus FinalizeAndConvertVariables(
    const SatParameters& params, absl::Span<const int> vars,
    const MPModelProtoWrapper& mp_model, absl::Span<MipVariableState> states,
    CpModelProto* cp_model, int64_t* num_int_bound_changes,
    ShiftStats* shift_stats, int* num_small_domains, SolverLogger* logger) {
  const double max_var_bound = MaxCpSatVariableBound();
  for (const int var : vars) {
    MipVariableState& state = states[var];
    if (state.is_integer) {
      const double lb = state.lb;
      const double ub = state.ub;
      if (!MakeIntegerVariableBoundsInteger(
              params, state.scaling, state.error_lb, state.error_ub, &state.lb,
              &state.ub, num_int_bound_changes)) {
        if (logger != nullptr) {
          SOLVER_LOG(logger, "Empty domain for integer variable #", var, ": [",
                     lb, ",", ub, "]");
        }
        return VariableConversionStatus::kInfeasible;
      }
    }
    if (state.shift == 0.0) {
      ShiftVariable(FinalCpSatShiftValue(state.lb, state.ub, state.is_integer,
                                         params.mip_wanted_precision()),
                    &state, shift_stats);
    }

    IntegerVariableProto* cp_var = cp_model->mutable_variables(var);
    if (!state.is_integer) {
      if (!ConvertVariableToCpVariable(
              params, var, mp_model.variables(var).objective_coefficient(),
              &state, cp_var, num_small_domains)) {
        return VariableConversionStatus::kInvalid;
      }
      continue;
    }

    // The bounds are integer and were already relaxed by the error bounds of
    // the variable above, so they are the final domain.
    if (!(std::abs(state.lb) <= max_var_bound &&
          std::abs(state.ub) <= max_var_bound)) {
      return VariableConversionStatus::kInvalid;
    }
    cp_var->clear_domain();
    cp_var->add_domain(static_cast<int64_t>(state.lb));
    cp_var->add_domain(static_cast<int64_t>(state.ub));
  }
  return VariableConversionStatus::kConverted;
}

double ComputeMaxVariableUnscalingError(
    const MPModelProtoWrapper& mp_model,
    absl::Span<const MipVariableState> states, const CpModelProto& cp_model) {
  const int num_variables = mp_model.num_variables();
  DCHECK_EQ(states.size(), num_variables);
  DCHECK_EQ(cp_model.variables_size(), num_variables);

  // If any variable has an empty domain, the CP-SAT model has no feasible
  // solution, so the maximum unscaling violation over feasible solutions is 0.
  for (int v = 0; v < num_variables; ++v) {
    const auto& domain = cp_model.variables(v).domain();
    if (domain.empty() || domain[0] > domain[domain.size() - 1]) return 0.0;
  }

  double max_error = 0.0;
  for (int v = 0; v < num_variables; ++v) {
    const auto& domain = cp_model.variables(v).domain();
    const int64_t d_min = domain[0];
    const int64_t d_max = domain[domain.size() - 1];
    const MipVariableState& state = states[v];
    const double s = state.scaling;
    const double shift = state.shift;
    if (!(s > 0.0) || !std::isfinite(s) || !std::isfinite(shift)) {
      return kInfinity;
    }

    const MipVariableProto& mp_var = mp_model.variables(v);
    if (mp_var.is_integer() && !(s == 1.0 && shift == std::round(shift))) {
      max_error = std::max(max_error, 0.5);
    }
    const double lb = mp_var.lower_bound();
    if (lb != -kInfinity) {
      if (!std::isfinite(lb)) return kInfinity;
      const double lb_num = TightSum()
                                .AddProd(s, lb)
                                .AddProd(-1.0, d_min)
                                .Add(-shift)
                                .TightUpperBound();
      if (lb_num > 0.0) {
        max_error = std::max(max_error, ExactQuotientUb(lb_num, s));
      }
    }
    const double ub = mp_var.upper_bound();
    if (ub != kInfinity) {
      if (!std::isfinite(ub)) return kInfinity;
      const double ub_num =
          TightSum().Add(d_max).Add(shift).AddProd(-s, ub).TightUpperBound();
      if (ub_num > 0.0) {
        max_error = std::max(max_error, ExactQuotientUb(ub_num, s));
      }
    }
  }
  return max_error;
}

void DisplayConstraintScalingStats(const SatParameters& params,
                                   double max_variable_unscaling_error,
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

  // Display the error/scaling on the variables and constraints.
  const auto log_error = [&](absl::string_view label, double error) {
    const bool imprecise = error > params.mip_wanted_precision();
    SOLVER_LOG(logger, "[Scaling] ", label, ": ", error,
               (imprecise ? " [IMPRECISE]" : ""));
  };
  log_error("Maximum variable unscaling error", max_variable_unscaling_error);
  log_error("Maximum constraint rounding error", scaler.max_rounding_error);
  log_error("Maximum constraint discretization error",
            scaler.max_discretization_error);
  log_error("Maximum constraint total unscaling error",
            scaler.max_total_unscaling_error);
  SOLVER_LOG(logger, "[Scaling] Constraint scaling factor range: [",
             scaler.min_scaling_factor, ", ", scaler.max_scaling_factor, "]");

  // Our check precision is pretty large, but this should still be usable in
  // practice. Especially since these are worst case bounds, solution might be
  // good.
  const double max_constraint_error =
      params.mip_proper_relaxation()
          ? scaler.max_total_unscaling_error
          : std::max(
                scaler.max_total_unscaling_error,
                scaler.max_rounding_error + scaler.max_discretization_error);
  const double max_error =
      std::max(max_variable_unscaling_error, max_constraint_error);
  if (num_clamped == 0 && max_error < params.mip_check_precision()) {
    // TODO(user): The objective scaling is not counted here and can be way off
    // though, see for example australia-abs-cta.pb.gz. But we should still
    // report correct lower bound as we correct for the error with
    // params.mip_compute_true_objective_bound().
    SOLVER_LOG(logger, "[Scaling] SUCCESS! No clamped variable and errors <= ",
               max_error);
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
    const auto [c_lb, c_ub] = QuotientBounds(coeff, state.scaling);
    if (!std::isfinite(c_lb) || !std::isfinite(c_ub)) {
      cp_model->clear_floating_point_objective();
      return false;
    }
    if (state.shift != 0.0) {
      offset.AddProd(state.shift > 0.0 ? c_lb : c_ub, state.shift);
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
  scaler.use_proper_relaxation = params.mip_proper_relaxation();
  scaler.scaling_target = int64_t{1} << params.mip_max_activity_exponent();
  scaler.keep_names = keep_names;

  // Add the constraints. We scale each of them individually.
  const int num_constraints = mp_model.num_constraints();
  for (int c = 0; c < num_constraints; ++c) {
    const EnforcedLinearConstraintProto& mp_constraint =
        mp_model.constraints(c);
    if (ConstraintIsAlwaysTrue(mp_constraint)) continue;

    const absl::Status status = scaler.ScaleAndAddConstraint(
        mp_constraint, states, cp_model,
        ConstraintScalingOptions::WithBoundTolerance(
            params.mip_tight_precision()));
    if (!status.ok()) {
      SOLVER_LOG(logger, status.message());
      return false;
    }
  }

  const double max_var_unscaling_error =
      ComputeMaxVariableUnscalingError(mp_model, states, *cp_model);
  DisplayConstraintScalingStats(params, max_var_unscaling_error, scaler,
                                num_clamped, num_small_domains, logger);
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

std::vector<ResidualContinuousComponent> PartitionResidualComponents(
    const MPModelProtoWrapper& mp_model,
    absl::Span<const int> residual_var_indices,
    absl::Span<const int> to_dense_index,
    std::vector<int>* pure_integer_constraints) {
  const int num_cols = residual_var_indices.size();
  DenseConnectedComponentsFinder cc;
  cc.SetNumberOfNodes(num_cols);

  const int num_constraints = mp_model.num_constraints();
  std::vector<int> row_first_col(num_constraints, -1);
  for (int c = 0; c < num_constraints; ++c) {
    const EnforcedLinearConstraintProto& ct = mp_model.constraints(c);
    if (ConstraintIsAlwaysTrue(ct)) continue;
    int first_col = -1;
    for (int i = 0; i < ct.vars().size(); ++i) {
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
    if (first_col == -1 && pure_integer_constraints != nullptr) {
      pure_integer_constraints->push_back(c);
    }
  }

  const int num_components = cc.GetNumberOfComponents();
  const std::vector<int> col_to_component = cc.GetComponentIds();
  std::vector<ResidualContinuousComponent> components(num_components);
  for (int col = 0; col < num_cols; ++col) {
    components[col_to_component[col]].continuous_vars.push_back(
        residual_var_indices[col]);
  }
  for (int c = 0; c < num_constraints; ++c) {
    if (row_first_col[c] != -1) {
      components[col_to_component[row_first_col[c]]].constraints.push_back(c);
    }
  }
  return components;
}

ResidualContinuousComponents SplitIntoResidualContinuousComponents(
    const MPModelProtoWrapper& mp_model,
    absl::Span<const MipVariableState> states) {
  ResidualContinuousComponents result;
  const int num_vars = mp_model.num_variables();
  DCHECK(states.empty() || states.size() == num_vars);
  std::vector<int> to_dense_index(num_vars, -1);
  std::vector<int> residual_var_indices;
  for (int var = 0; var < num_vars; ++var) {
    const bool is_integer = states.empty()
                                ? mp_model.variables(var).is_integer()
                                : states[var].is_integer;
    if (!is_integer) {
      to_dense_index[var] = residual_var_indices.size();
      residual_var_indices.push_back(var);
    }
  }
  result.components = PartitionResidualComponents(
      mp_model, residual_var_indices, to_dense_index,
      &result.pure_integer_constraints);
  return result;
}

}  // namespace sat
}  // namespace operations_research
