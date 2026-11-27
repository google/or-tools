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

// Utility functions to interact with an lp solver from the SAT context.

#ifndef ORTOOLS_SAT_LP_UTILS_H_
#define ORTOOLS_SAT_LP_UTILS_H_

#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/mip_model.pb.h"
#include "ortools/sat/mp_model_wrapper.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/fp_utils.h"
#include "ortools/util/full_precision_inequalities.h"
#include "ortools/util/logging.h"

namespace operations_research {
namespace sat {

// Once a MPModelProtoWrapper is ready to be converted to CP-SAT, we split it
// into disjoint residual continuous components and process them one by one. The
// wrapper is never modified during that phase: instead, each wrapper variable x
// is mapped to a CP-SAT variable x' with
//   x = (x' + shift) / scaling
// and this struct holds the current state of x'. Constraints and objective are
// transformed on the fly when they are converted.
struct MipVariableState {
  // Current bounds of x', i.e. in the scaled and shifted space.
  double lb = -std::numeric_limits<double>::infinity();
  double ub = std::numeric_limits<double>::infinity();
  bool is_integer = false;

  // See the formula above. The shift is expressed in the scaled space.
  double scaling = 1.0;
  double shift = 0.0;

  // Tight bounds [error_lb, error_ub] (with error_lb <= 0 <= error_ub) on
  // (discretized_x' - x') for variables marked as integer via approximate
  // rational scaling. Zero for exact integers and continuous variables.
  double error_lb = 0.0;
  double error_ub = 0.0;
};

// Multiplies the current domain of `state` by `scaling`, rounding finite bounds
// outward so no feasible value is lost when `scaling` is not a power of two.
inline void ScaleVariableState(double scaling, MipVariableState* state) {
  if (scaling == 1.0) return;
  DCHECK_GT(scaling, 0.0);
  std::tie(state->lb, state->ub) =
      RelaxedMulInterval(state->lb, state->ub, scaling);
  if (state->shift != 0.0) state->shift *= scaling;
  state->scaling *= scaling;
}

// A residual continuous component: a set of continuous variables and the
// constraints in which they appear, such that no constraint of the component
// contains a continuous variable outside of it. All indices refer to the
// variables and constraints of the MPModelProtoWrapper being converted.
//
// Note that in `constraints`, all the variables that are not in
// `continuous_vars` must be integer ("external" variables).
struct ResidualContinuousComponent {
  std::vector<int> continuous_vars;
  std::vector<int> constraints;

  // Dense mapping shared by all the component functions below:
  // var_to_local_col[v] is the position of v in `continuous_vars`, or -1 for
  // any other variable (in particular the external integer variables of
  // `constraints`). Since this is of size num_variables, it points to a buffer
  // shared by all the components and is only valid while this component is
  // being processed, see ScopedVarToLocalCol.
  absl::Span<const int> var_to_local_col;
};

// Fills `buffer` (which must be of size num_variables and filled with -1) with
// the mapping of the given component and sets `compo->var_to_local_col`. The
// buffer is restored to -1 and the span cleared on destruction.
class ScopedVarToLocalCol {
 public:
  ScopedVarToLocalCol(ResidualContinuousComponent* compo,
                      std::vector<int>* buffer);
  ~ScopedVarToLocalCol();

  // Not copyable or movable.
  ScopedVarToLocalCol(const ScopedVarToLocalCol&) = delete;
  ScopedVarToLocalCol& operator=(const ScopedVarToLocalCol&) = delete;

 private:
  ResidualContinuousComponent* compo_;
  std::vector<int>* buffer_;
};

// Outward bounds on the exact unscaled activity `sum_i (a_i / s_i) * y_i` of a
// constraint over integer domains `[lb_i, ub_i]`, given `[c_lb_i, c_ub_i]`
// containing each exact coefficient `a_i / s_i`.
struct ExactActivityBounds {
  TightSum min_activity;
  TightSum max_activity;
  bool min_is_neg_inf = false;
  bool max_is_pos_inf = false;

  void Clear() {
    min_activity.Clear();
    max_activity.Clear();
    min_is_neg_inf = false;
    max_is_pos_inf = false;
  }
  void AddTerm(double c_lb, double c_ub, int64_t lb, int64_t ub);
  double MaxLowerViolation(double orig_lb_upper) const;
  double MaxUpperViolation(double orig_ub_lower) const;
};

// A constraint that was already scaled to integer coefficients and bounds,
// see ConstraintScaler::AddAlreadyScaledConstraint().
struct ScaledLinearConstraint {
  // The terms with a non-zero coefficient, and the CP-SAT domain
  // `[var_lbs[i], var_ubs[i]]` of each variable.
  std::vector<int> vars;
  std::vector<int64_t> coeffs;
  std::vector<int64_t> var_lbs;
  std::vector<int64_t> var_ubs;

  // The constraint is `lb <= sum_i coeffs[i] * vars[i] <= ub`, where a missing
  // side is left to its default value. It is infeasible if `lb > ub`.
  int64_t lb = std::numeric_limits<int64_t>::min();
  int64_t ub = std::numeric_limits<int64_t>::max();

  // The factor by which the original constraint was scaled and the resulting
  // errors, for the statistics of the ConstraintScaler.
  double scaling_factor = 0.0;
  double relative_coeff_error = 0.0;
  double absolute_rhs_error = 0.0;
  double discretization_error = 0.0;
  double rounding_error = 0.0;
  double unrounding_error = 0.0;
  double total_unscaling_error = 0.0;
};

// Per-constraint options for ConstraintScaler::ScaleAndAddConstraint(),
// controlling initial bound tolerances and how variable discretization errors
// from `states` are accumulated.
struct ConstraintScalingOptions {
  double lb_bound_tolerance = 0.0;
  double ub_bound_tolerance = 0.0;

  // When `>= 0`, only this variable's discretization error is accumulated on
  // the lower-bound (resp. upper-bound) side; other variables' discretization
  // errors are ignored on that side.
  int lb_only_error_var = -1;
  int ub_only_error_var = -1;

  // When `override_error_var >= 0`, uses `[override_error_lb,
  // override_error_ub]` instead of `states[override_error_var]`'s
  // discretization error on this constraint.
  int override_error_var = -1;
  double override_error_lb = 0.0;
  double override_error_ub = 0.0;

  static ConstraintScalingOptions WithBoundTolerance(double tolerance) {
    return {.lb_bound_tolerance = tolerance, .ub_bound_tolerance = tolerance};
  }
};

// Helper to scale linear constraints with floating point coefficients to
// CpModelProto::ConstraintProto. We use a class to reuse the temporary memory
// when we scale many constraints.
struct ConstraintScaler {
  // Scales an individual EnforcedLinearConstraintProto and adds it to the given
  // CpModelProto. This uses the vars, coeffs, lower_bound, upper_bound and
  // enforcements of the given mp_constraint, transformed according to the
  // scaling and shift of each variable in `states` (see MipVariableState).
  //
  // We use the domain of the variables to derive error bounds and scale the
  // constraint as best as we can within "wanted_precision" and
  // "scaling_target". We usually scale with a power of two scaling factor or
  // a rational scaling factor if we detect a good one via FindRationalFactor().
  //
  // Returns an error if the given constraint contained huge coefficients or
  // infinity. Note that we do not consider it an error if the wanted precision
  // is not reached (best effort). One can check the error statistics field
  // below and decide when they are too high and report an error separately.
  absl::Status ScaleAndAddConstraint(
      const EnforcedLinearConstraintProto& mp_constraint,
      absl::Span<const MipVariableState> states, CpModelProto* cp_model,
      const ConstraintScalingOptions& options = {});

  // Adds a constraint that the caller already scaled itself: `scaled` is
  // `mp_constraint` transformed according to `states` and scaled to integers.
  // This adds it with the enforcement of `mp_constraint` to the given
  // CpModelProto, and updates the statistics below. The caller must make sure
  // that `scaled` cannot overflow.
  //
  // This is called by DetectAndConvertTotallyUnimodularComponent() in tum.cc,
  // which needs to scale all the constraints of a component consistently.
  absl::Status AddAlreadyScaledConstraint(
      const EnforcedLinearConstraintProto& mp_constraint,
      absl::Span<const MipVariableState> states,
      const ScaledLinearConstraint& scaled, CpModelProto* cp_model);

  // Statistics over all scaled constraints. This can be inspected to know the
  // final error produced by ScaleAndAddConstraint().
  double max_relative_coeff_error = 0.0;
  double max_absolute_rhs_error = 0.0;
  double max_scaling_factor = 0.0;
  double min_scaling_factor = std::numeric_limits<double>::infinity();
  double max_discretization_error = 0.0;
  double max_rounding_error = 0.0;
  double max_unrounding_error = 0.0;

  // Upper bound across all constraints on the exact-unscaling MIP constraint
  // violation of any CP-SAT feasible integer solution `y` (mapped back via
  // `x_i = (y_i + shift_i) / scaling_i`), evaluated per row at the emitted
  // CP-SAT integer bounds.
  double max_total_unscaling_error = 0.0;

  // Helper to compute tight rounding and unrounding error bounds.
  TightScalingErrorHelper tight_error_helper;

  // Statistics for trivial constraints.
  int64_t num_trivial_constraints = 0;

  // Parameters. Whether we ignore or copy the mp_constraint.name() field.
  bool keep_names = false;

  // Parameters passed to FindBestScalingAndComputeErrors(), see documentation
  // there to understand their meaning.
  bool use_proper_relaxation = false;
  double wanted_precision = 1e-6;
  int64_t scaling_target = int64_t{1} << 50;

  // Private temporary field to reuse memory.
  std::vector<int> var_indices;
  BoundedLinearExpression bounded_expr;
  std::vector<double> coefficients_lb;
  std::vector<double> coefficients_ub;
  std::vector<int64_t> int_lower_bounds;
  std::vector<int64_t> int_upper_bounds;
  std::vector<double> tmp_transformed_coeffs;
  std::vector<double> tmp_transformed_coeffs_lb;
  std::vector<double> tmp_transformed_coeffs_ub;
  std::vector<int64_t> tmp_enforcement_values;
  TightSum tmp_discretization_lb;
  TightSum tmp_discretization_ub;
  TightSum tmp_scaled_lb;
  TightSum tmp_scaled_ub;
  ExactActivityBounds tmp_exact_activity;

  // Private helpers. The first one fills `tmp_enforcement_values` with the
  // enforcement values of `mp_constraint` transformed according to `states`,
  // and sets `is_never_enforced` if one of them is outside the domain of its
  // variable. The second one adds the enforcement to a `constraint` whose
  // variable domains are in `int_lower_bounds` and `int_upper_bounds`.
  absl::Status TransformEnforcementValues(
      const EnforcedLinearConstraintProto& mp_constraint,
      absl::Span<const MipVariableState> states,
      absl::Span<const IntegerVariableProto* const> var_domains,
      bool* is_never_enforced);
  absl::Status AddEnforcement(
      const EnforcedLinearConstraintProto& mp_constraint,
      absl::Span<const IntegerVariableProto* const> var_domains,
      ConstraintProto* constraint);
};

// Initializes the state of all variables from the current wrapper bounds and
// integrality (no scaling, no shift, no error).
std::vector<MipVariableState> InitialVariableStates(
    const MPModelProtoWrapper& mp_model);

// Helpers for transforming constraints and propagating discretization errors
// according to MipVariableState.
void AddShiftedTermOffset(double c_lb, double c_ub, double shift,
                          TightSum* neg_offset_lb, TightSum* neg_offset_ub);
void AddTermErrorBounds(double c_lb, double c_ub, double err_lb, double err_ub,
                        TightSum* sum_lb, TightSum* sum_ub);
void TransformConstraint(const EnforcedLinearConstraintProto& ct,
                         absl::Span<const MipVariableState> states,
                         std::vector<double>* coeffs, double* lb, double* ub);

// Computes an upper bound on the exact-unscaling MIP constraint violation of
// any integer solution `y` satisfying `final_lb <= sum_i (r_i / gcd) * y_i <=
// final_ub` (with `final_lb <= final_ub`) within the bounded variable domains
// loaded in `tight_error_helper`. When `max_unrounding_error` is non-null, also
// updates it with the unrounding error evaluated at `gcd * final_lb` and `gcd *
// final_ub`.
double ComputeBoundedRowTotalUnscalingError(
    double scaling_factor, int64_t gcd, int64_t final_lb, int64_t final_ub,
    double orig_lb_upper, double orig_ub_lower,
    const ExactActivityBounds& activity_bounds,
    TightScalingErrorHelper* tight_error_helper,
    double* max_unrounding_error = nullptr);

// Writes back the given states into `mp_model`: variable bounds and
// integrality, scaled constraint coefficients, shifted constraint bounds,
// scaled objective and shifted objective offset, and scaled/shifted solution
// hint. This is mainly useful for tests.
void ApplyVariableStatesToModel(absl::Span<const MipVariableState> states,
                                MPModelProto* mp_model);

// Partitions `residual_var_indices` (mapped via `to_dense_index`, with `-1` for
// external variables) and non-trivial constraints of `mp_model` into connected
// residual components. Optionally records constraints with no residual variable
// in `pure_integer_constraints`.
std::vector<ResidualContinuousComponent> PartitionResidualComponents(
    const MPModelProtoWrapper& mp_model,
    absl::Span<const int> residual_var_indices,
    absl::Span<const int> to_dense_index,
    std::vector<int>* pure_integer_constraints = nullptr);

// Splits `mp_model` into pure integer constraints (constraints with no
// continuous variable with a non-zero coefficient) and disjoint residual
// continuous components (connected components of continuous variables linked
// by shared constraints). When `states` is non-empty, `states[v].is_integer`
// determines variable integrality instead of `mp_model.variables(v)`.
// Always-true constraints (`lb == -inf && ub == +inf`) are omitted. All indices
// refer to the MPModelProtoWrapper.
struct ResidualContinuousComponents {
  std::vector<int> pure_integer_constraints;
  std::vector<ResidualContinuousComponent> components;
};
ResidualContinuousComponents SplitIntoResidualContinuousComponents(
    const MPModelProtoWrapper& mp_model,
    absl::Span<const MipVariableState> states = {});

// Statistics accumulated by the per-component functions below, and the
// corresponding logging functions.
struct ShiftStats {
  int64_t num_shifted = 0;
  double max_shift = 0.0;
};
void LogShiftStats(const ShiftStats& stats, SolverLogger* logger);

struct OneVariableComponentStats {
  int num_components = 0;
  int num_integers = 0;
  int num_fixed = 0;
  int num_pow2_preferred = 0;
  int num_missed = 0;
  int64_t num_int_bound_changes = 0;
  double max_scaling = 0.0;
  ShiftStats shift_stats;

  // For any constraint-tolerant MIP solution x, crushing the integerized
  // single-variable components increases the minimization objective by at most
  //   objective_error_bound +
  //   max_violation_sensitivity * sum_{1-var compo c} max_{r in c} viol_r(x),
  // which is at most
  // `objective_error_bound + mip_tight_precision * num_integers *
  // max_violation_sensitivity` when every constraint holds within the
  // configured mip_tight_precision tolerance.
  double objective_error_bound = 0.0;
  double max_violation_sensitivity = 0.0;
};
void LogOneVariableComponentStats(const OneVariableComponentStats& stats,
                                  SolverLogger* logger);

struct PrecisionScalingStats {
  int num_scaled = 0;
  int min_exponent = std::numeric_limits<int>::max();
  int max_exponent = 0;
};
void LogPrecisionScalingStats(const SatParameters& params,
                              const MPModelProtoWrapper& mp_model,
                              absl::Span<const MipVariableState> states,
                              const PrecisionScalingStats& stats,
                              SolverLogger* logger);

enum class OneVariableComponentStatus {
  kNotConverted,
  kConverted,
  kInfeasible,
  kInvalid,
};

// Processes a residual continuous component with a single continuous variable
// x, which is thus the unique non-integer of all its constraints.
//
// In a minimization problem, if all these constraints are "nice" (integer
// coefficients and bounds after scaling x by a rational factor s, see
// GetIntegralityMultiplier()) then there is an optimal solution where s * x is
// integer: on the side where the objective pushes x, the tightest active
// constraint (or the bound of x) is integral in the scaled space. Note that
// when x has no objective, this is true for both sides and we require both.
//
// With `mip_scale_for_precision`, this is only done if the resulting worst
// case error on x is not larger than the one of the power of two scaling that
// ScaleComponentContinuousVariablesForGivenPrecision() would use otherwise.
// With a large rational s, the latter is usually as precise and avoids large
// constraint scaling factors.
//
// If `x` is made integer (`kConverted`), it is scaled, snapped to integer
// bounds, shifted toward zero, and written to `cp_model->mutable_variables(x)`,
// and all `component.constraints` are scaled and added to `cp_model` via
// `scaler` (omitting the initial bound tolerance on each constraint's snapped
// side under `use_proper_relaxation` and using per-constraint discretization
// error bounds). External integer variables referenced by
// `component.constraints` must already be in their final state and converted in
// `cp_model`.
//
// If the scaled domain of x contains no integer point, x is fixed to the
// relevant bound instead and `kNotConverted` is returned. Returns `kInfeasible`
// if the snapped integer domain of x is empty, or `kInvalid` on scaling
// overflow.
OneVariableComponentStatus DetectAndConvertOneVariableComponent(
    const SatParameters& params, const ResidualContinuousComponent& component,
    const MPModelProtoWrapper& mp_model, absl::Span<MipVariableState> states,
    ConstraintScaler* scaler, CpModelProto* cp_model,
    OneVariableComponentStats* stats, SolverLogger* logger = nullptr);

// Multiplies all continuous variables of the component by
// params.mip_var_scaling(). If the domain of a variable is going to have larger
// values than mip_max_bound, then we scale to have the max domain magnitude
// equal to mip_max_bound instead. Variables already scaled are left alone.
void ScaleComponentContinuousVariables(const SatParameters& params,
                                       const ResidualContinuousComponent& compo,
                                       absl::Span<MipVariableState> states);

// Controls how constraint headroom is computed in
// ScaleComponentContinuousVariablesForGivenPrecision():
// - kPerTerm computes each continuous variable's headroom from its own
//   coefficient and domain in the constraint, so exact/dyadic coefficients or
//   small-domain terms are not penalized by scaling needed for other terms.
// - kUniformPerConstraint caps every variable in a constraint by the uniform
//   row headroom `max_scaling_factor / scaling_factor`.
enum class PrecisionHeadroomMode {
  kPerTerm,
  kUniformPerConstraint,
};

// For each constraint, computes the power-of-two headroom left up to
// mip_max_activity_exponent while satisfying mip_wanted_precision. Then scales
// each continuous variable by the minimum headroom across all constraints in
// which it appears, capped by the maximum scaling needed for
// mip_wanted_precision and the maximum valid CP-SAT variable domain.
void ScaleComponentContinuousVariablesForGivenPrecision(
    const SatParameters& params, const ResidualContinuousComponent& compo,
    const MPModelProtoWrapper& mp_model, absl::Span<MipVariableState> states,
    PrecisionScalingStats* stats,
    PrecisionHeadroomMode headroom_mode = PrecisionHeadroomMode::kPerTerm);

// Largest finite variable bound magnitude accepted by CP-SAT variable
// conversion (`< kint64max / 2`, represented exactly as a `double`).
inline double MaxCpSatVariableBound() {
  return FastStepDown(
      static_cast<double>(std::numeric_limits<int64_t>::max() / 2));
}

// Returns the shift (in the variable's current scaled space) to apply right
// before converting `[lb, ub]` to a CP-SAT integer domain so that the shifted
// domain contains the integer 0 without shifting more than necessary:
// 1/ Shift small continuous domains like `[0.3, 0.4]` to `[0, 0.1]` so at least
//    the integer zero can be a solution.
// 2/ Shift domains like `[1.5, 10]` to `[-0.5, 8]` so the integer zero is
//    inside.
inline double FinalCpSatShiftValue(double lb, double ub, bool is_integer,
                                   double tolerance) {
  if (!is_integer && std::floor(ub) < std::ceil(lb)) {
    if (std::isfinite(lb) && lb > 0.0) return lb;
    if (std::isfinite(ub) && ub < 0.0) return ub;
  }
  if (std::isfinite(lb) && lb > tolerance) return std::ceil(lb);
  if (std::isfinite(ub) && ub < -tolerance) return std::floor(ub);
  return 0.0;
}

// Shifts `state` toward zero by `offset` (expressed in `state`'s current scaled
// space). The shift of an integer variable must be integer, and a variable must
// not be shifted more than once (`state->shift == 0.0` whenever `offset != 0`)
// so `state->shift` stays exact.
void ShiftVariable(double offset, MipVariableState* state,
                   ShiftStats* stats = nullptr);

// Applies `ShiftVariable(get_shift(state), &state, stats)` to each variable in
// `vars`.
void ShiftVariables(
    const std::function<double(const MipVariableState&)>& get_shift,
    absl::Span<const int> vars, absl::Span<MipVariableState> states,
    ShiftStats* stats);

// This simple step helps and should be done first. Returns false if the model
// is trivially infeasible because of crossing bounds.
bool MakeBoundsOfIntegerVariablesInteger(const SatParameters& params,
                                         MPModelProtoWrapper* mp_model,
                                         SolverLogger* logger);

// Utility useful for RestrictBoundsWithDualReasoning() but also the
// PropagateBounds() MIP presolve.
struct LeftRightActivityBounds {
  // Fills the struct from the given constraint.
  //
  // If exact is true, then min_activity_of_others/max_activity_of_others will
  // contains EXACT bounds. Otherwise, the computation will just be down with
  // double arithmetic (a lot faster).
  template <bool exact = true, bool fill_min = true, bool fill_max = true>
  void FillFromConstraint(absl::Span<const double> var_lbs,
                          absl::Span<const double> var_ubs,
                          const EnforcedLinearConstraintProto& ct);

  template <bool exact = true>
  void FillFromConstraint(absl::Span<const double> var_lbs,
                          absl::Span<const double> var_ubs,
                          const EnforcedLinearConstraintProto& ct,
                          bool fill_min, bool fill_max) {
    if (fill_min && fill_max) {
      FillFromConstraint<exact, true, true>(var_lbs, var_ubs, ct);
    } else if (fill_min) {
      FillFromConstraint<exact, true, false>(var_lbs, var_ubs, ct);
    } else if (fill_max) {
      FillFromConstraint<exact, false, true>(var_lbs, var_ubs, ct);
    }
  }

  // User facing data. Contains for each position i, the min/max activity of the
  // other term. This does not include the constraint bounds.
  std::vector<double> min_activity_of_others;
  std::vector<double> max_activity_of_others;

  // Internal data. Contains for each position i in a constraint, the min/max
  // activities of the terms to the right (resp. left) of i (i is NOT included).
  std::vector<double> min_left_activity;
  std::vector<double> min_right_activity;
  std::vector<double> max_left_activity;
  std::vector<double> max_right_activity;

  // Temporary data to compute the exact bound. Here to be reused.
  TightSum tmp_min_sum;
  TightSum tmp_max_sum;
};

// If a variable only appears in >= constraints (resp. <=) it is sometimes
// possible to compute a bound such that any value above it will just always
// satisfy all constraints, so there is no point looking for such a high value.
//
// Doing this before scaling can reduce the domain of variables and help.
//
// TODO(user): This should probably be done within a fixed-point loop and
// mixed with bound propagation. Doing a single pass can still help remove
// all [0, infinity) variables on models like uccase7.mps for instance.
// Returns true if at least one bound was restricted by more than
// params.mip_tight_precision().
bool RestrictBoundsWithDualReasoning(const SatParameters& params,
                                     const MPModelProtoWrapper& mp_model,
                                     absl::Span<double> var_lbs,
                                     absl::Span<double> var_ubs,
                                     SolverLogger* logger);

// Prints a single-line with variable stats and whether the problem is a MIP or
// a pure IP.
void DisplayVariableStatistics(const MPModelProto& mp_model,
                               SolverLogger* logger);
void DisplayVariableStatistics(absl::Span<const MipVariableState> states,
                               SolverLogger* logger);

// This function changes bounds of variables or constraints that have a
// magnitude greater than mip_max_valid_magnitude.
void ChangeLargeBoundsToInfinity(double max_magnitude,
                                 MPModelProtoWrapper* mp_model,
                                 SolverLogger* logger);

// Performs some extra tests on the given model and returns false if one is not
// satisfied. These are needed before trying to convert it to the native CP-SAT
// format.
bool MipModelProtoValidationBeforeConversion(const SatParameters& params,
                                             const MipModelProto& model,
                                             SolverLogger* logger);

// To satisfy our scaling requirements, any term that is almost zero can just
// be set to zero. We need to do that before operations like
// DetectImpliedIntegers(), because really low coefficients can cause issues
// and might lead to less detection.
void RemoveNearZeroTerms(const SatParameters& params,
                         MPModelProtoWrapper* mp_model, SolverLogger* logger);

// Marks as integer in `states` the continuous variables that are the unique
// non-integer of an unenforced near-equality constraint with a rational
// integrality multiplier `s` (see GetIntegralityMultiplier()) and certified
// error bounds `[error_lb, error_ub]` on `(discretized_v - s * v)` in the
// scaled variable space within precision. Does not modify `mp_model`.
// Returns `(constraint_index, ConstraintScalingOptions)` pairs (sorted by
// `constraint_index`) for the constraints used to integerize variables, so
// callers can scale those constraints without double-counting bound tolerances
// or external variable discretization errors. Continuous variables that appear
// only in inequalities are handled later by
// DetectAndConvertOneVariableComponent().
//
// TODO(user): Support non-integer affine offsets (e.g. on the 3 neos-46470*
// MIPLIB problems where we have a non-integer RHS).
std::vector<std::pair<int, ConstraintScalingOptions>> DetectImpliedIntegers(
    const SatParameters& params, const MPModelProtoWrapper& mp_model,
    absl::Span<MipVariableState> states, SolverLogger* logger = nullptr);

// Clamps variable bounds to [-mip_max_bound, mip_max_bound]. Returns false if
// a variable domain does not intersect this interval.
bool ClampVariableBounds(const SatParameters& params,
                         MPModelProtoWrapper* mp_model, SolverLogger* logger,
                         int* num_clamped = nullptr);

// Same as above, but only for the variables in `vars` and without logging the
// number of clamped bounds (which is added to `num_clamped`).
bool ClampVariableBounds(const SatParameters& params,
                         absl::Span<const int> vars,
                         absl::Span<MipVariableState> states,
                         SolverLogger* logger, int* num_clamped);

// Extracts enforcement literals (and integer enforcements) from one-sided
// constraints in the given MPModelProtoWrapper.
void ExtractEnforcementLiterals(const SatParameters& params,
                                MPModelProtoWrapper* mp_model,
                                SolverLogger* logger);

// Converts a single variable into an IntegerVariableProto and updates its
// discretization error bounds. Returns false if a bound of the variable is not
// within `MaxCpSatVariableBound()` or if it cannot take an integer value.
// `objective_coefficient` is the one from the wrapper (only its sign matters).
bool ConvertVariableToCpVariable(const SatParameters& params, int var_index,
                                 double objective_coefficient,
                                 MipVariableState* state,
                                 IntegerVariableProto* cp_var,
                                 int* num_small_domains);

enum class VariableConversionStatus {
  kConverted,
  kInfeasible,
  kInvalid,
};

// Puts each variable of `vars` in its final state and writes its domain to
// `cp_model->mutable_variables(var)`: the bounds of an integer variable are
// snapped to integers like in MakeBoundsOfIntegerVariablesInteger(), then the
// domain is shifted toward zero by FinalCpSatShiftValue(), and a continuous
// variable is finally discretized by ConvertVariableToCpVariable().
//
// Returns `kInfeasible` if an integer variable has an empty domain and
// `kInvalid` if a bound is not within `MaxCpSatVariableBound()`. The variables
// after the failing one are left untouched.
VariableConversionStatus FinalizeAndConvertVariables(
    const SatParameters& params, absl::Span<const int> vars,
    const MPModelProtoWrapper& mp_model, absl::Span<MipVariableState> states,
    CpModelProto* cp_model, int64_t* num_int_bound_changes,
    ShiftStats* shift_stats, int* num_small_domains = nullptr,
    SolverLogger* logger = nullptr);

// Computes an upper bound across all variables on the exact-unscaling MIP
// variable violation of any CP-SAT feasible integer solution `y` (mapped back
// via `x_v = (y_v + states[v].shift) / states[v].scaling`).
double ComputeMaxVariableUnscalingError(
    const MPModelProtoWrapper& mp_model,
    absl::Span<const MipVariableState> states, const CpModelProto& cp_model);

// Logs variable and constraint scaling and discretization statistics.
void DisplayConstraintScalingStats(const SatParameters& params,
                                   double max_variable_unscaling_error,
                                   const ConstraintScaler& scaler,
                                   int num_clamped, int num_small_domains,
                                   SolverLogger* logger);

// Populates `cp_model->floating_point_objective()` from `mp_model` transformed
// according to `states`, using the integer variable domains already populated
// in `cp_model` so that non-exact scaled coefficients and offset sums never
// overestimate a minimization objective (or underestimate a maximization
// objective). Returns false if a scaled objective coefficient or offset
// overflows `double`.
bool PopulateCpModelFloatingPointObjective(
    const MPModelProtoWrapper& mp_model,
    absl::Span<const MipVariableState> states, CpModelProto* cp_model);

// Converts a MIP problem to a CpModel. Returns false if the coefficients
// couldn't be converted to integers with a good enough precision.
//
// `integer_error_lb` and `integer_error_ub` (sized to
// `mp_model.num_variables()`) pass precomputed discretization error bounds
// `[lb, ub]` on `(discretized_v - scaled_v)` for variables marked as integer
// via approximate rational scaling (use `0.0` for exact integers and continuous
// variables).
//
// There are a bunch of caveats and you can find more details on the
// SatParameters proto documentation for the mip_* parameters.
bool ConvertMPModelProtoToCpModelProto(
    const SatParameters& params, const MPModelProtoWrapper& mp_model,
    int num_clamped, absl::Span<const double> integer_error_lb,
    absl::Span<const double> integer_error_ub, CpModelProto* cp_model,
    SolverLogger* logger);

// Converts a CP-SAT model to a MPModelProto one.
// This only works for pure linear models (otherwise it returns false). This is
// mainly useful for debugging or using CP-SAT presolve and then trying other
// MIP solvers.
//
// TODO(user): This first version does not even handle basic Boolean constraint.
// Support more constraints as needed.
bool ConvertCpModelProtoToMPModelProto(const CpModelProto& input,
                                       MPModelProto* output);

// Scales a double objective to its integer version and fills it in the proto.
// The variables listed in the objective must be already defined in the cp_model
// proto as this uses the variable bounds to compute a proper scaling.
//
// This uses params.mip_wanted_tolerance() and
// params.mip_max_activity_exponent() to compute the scaling. Note however that
// if the wanted tolerance is not satisfied this still scales with best effort.
// You can see in the log the tolerance guaranteed by this automatic scaling.
//
// This will almost always return true except for really bad cases like having
// infinity in the objective.
bool ScaleAndSetObjective(const SatParameters& params,
                          absl::Span<const std::pair<int, double>> objective,
                          double objective_offset, bool maximize,
                          CpModelProto* cp_model, SolverLogger* logger);

// Given a CpModelProto with a floating point objective, and its scaled integer
// version with a known lower bound, this uses the variable bounds to derive a
// correct lower bound on the original objective.
//
// Note that the integer version can be way different, but then the bound is
// likely to be bad.
double ComputeTrueObjectiveLowerBound(
    const CpModelProto& model_proto_with_floating_point_objective,
    const CpModelProto& model_proto_with_integer_objective_and_domains,
    int64_t inner_integer_objective_lower_bound);

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_LP_UTILS_H_
