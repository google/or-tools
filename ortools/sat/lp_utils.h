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

#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/sat/cp_model.pb.h"
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
//   x = x' / scaling + shift
// and this struct holds the current state of x'. Constraints and objective are
// transformed on the fly when they are converted.
struct MipVariableState {
  // Current bounds of x', i.e. in the scaled and shifted space.
  double lb = -std::numeric_limits<double>::infinity();
  double ub = std::numeric_limits<double>::infinity();
  bool is_integer = false;

  // See the formula above. The shift is expressed in the wrapper space.
  double scaling = 1.0;
  double shift = 0.0;

  // Tight bounds [error_lb, error_ub] (with error_lb <= 0 <= error_ub) on
  // (discretized_x' - x') for variables marked as integer via approximate
  // rational scaling. Zero for exact integers and continuous variables.
  double error_lb = 0.0;
  double error_ub = 0.0;
};

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

// Returns the highest x such that 2^x <= value.
// This is meant to be called with a non-negative value.
inline int HighestPowerOfTwoAtOrBelow(double value) {
  CHECK_GE(value, 0.0);
  int exp;
  std::frexp(value, &exp);
  return exp > 0 ? exp - 1 : 0;
}

// Returns the lowest x such that value <= 2^x.
// This is meant to be called with a non-negative value.
inline int LowestPowerOfTwoAtOrAbove(double value) {
  CHECK_GE(value, 0.0);
  int exp;
  const double left = std::frexp(value, &exp);
  if (left == 0.5) return exp - 1;
  return exp;
}

// Returns the smallest factor f such that f * abs(x) is integer modulo the
// given tolerance relative to f (we use f * tolerance). It is only looking
// for f smaller than the given limit. Returns zero if no such factor exists
// below the limit.
//
// The complexity is a lot less than O(limit), but it is possible that we might
// miss the smallest such factor if the tolerance used is too low. This is
// because we only rely on the best rational approximations of x with increasing
// denominator.
int64_t FindRationalFactor(double x, int64_t limit = 1e4,
                           double tolerance = 1e-6);

// Given a linear expression Sum_i c_i * X_i with each X_i in [lb_i, ub_i],
// this returns a scaling factor f such that
// 1/ the rounded expression cannot overflow given the domains of the X_i:
//    Sum |std::round(f * c_i) * X_i| <= max_absolute_activity
// 2/ the error is bounded:
//    | Sum_i (std::round(f * c_i) - f * c_i) |
//         < f * wanted_absolute_activity_precision
//
// This also fills the exact errors made by using the returned scaling factor.
// The heuristics try to minimize the magnitude of the scaled expression while
// satisfying the requested precision.
//
// Returns 0.0 if no scaling factor can be found under the condition 1/. Note
// that we try really hard to satisfy 2/ but we still return our best shot even
// when 2/ is not satisfied. One can check this by comparing the returned
// scaled_sum_error / f with wanted_absolute_activity_precision.
//
// TODO(user): unit test this and move to fp_utils.
// TODO(user): Ideally the lower/upper should be int64_t so that we can have
// an exact definition for the max_absolute_activity allowed.
double FindBestScalingAndComputeErrors(
    absl::Span<const double> coefficients,
    absl::Span<const double> lower_bounds,
    absl::Span<const double> upper_bounds, int64_t max_absolute_activity,
    double wanted_absolute_activity_precision, double* relative_coeff_error,
    double* scaled_sum_error, double* max_scaling_factor = nullptr);

// Prescribed scaling of a constraint of a residual component that was made
// integral by MarkComponentTotallyUnimodularAsInteger(). The component matrix
// is TUM (`modular_factor = 1`) or 2-modular (`modular_factor = 2`), and with
// `scaling_factor` the scaled constraint has +/-1 coefficients on the component
// variables and multiples of `modular_factor` on the external ones.
struct ModularConstraintScaling {
  double scaling_factor = 0.0;
  int64_t modular_factor = 0;
};

// Helper to scale linear constraints with floating point coefficients to
// CpModelProto::ConstraintProto. We use a class to reuse the temporary memory
// when we scale many constraints.
//
// Note that this can be used to scale any constraint, it provides an API for
// MPConstraintProto, but also directly from spans of CpModelProto variable
// indices, coefficients and lower and upper bounds.
struct ConstraintScaler {
  // Scales an individual constraint and adds it to the given CpModelProto.
  //
  // We use the domain of the variables to derive error bounds and scale the
  // constraint as best as we can within "wanted_precision" and
  // "scaling_target". We usually scale with a power of two scaling factor or
  // a rational scaling factor if we detect a good one via FindRationalFactor().
  //
  // If `modular` is given, we use its scaling factor instead and round the
  // scaled constraint bounds to multiples of its `modular_factor` (outward if
  // `use_proper_relaxation` is true) so that the LP extreme points of the
  // component stay integral. The caller must make sure that the scaled
  // constraint cannot overflow.
  //
  // Returns an error if the given constraint contained huge coefficients or
  // infinity. Note that we do not consider it an error if the wanted precision
  // is not reached (best effort). One can check the error statistics field
  // below and decide when they are too high and report an error separately.
  absl::Status ScaleAndAddConstraint(
      absl::Span<const int> vars, absl::Span<const double> coeffs,
      double ct_lower_bound, double ct_upper_bound, absl::string_view name,
      absl::Span<const IntegerVariableProto* const> var_domains,
      ConstraintProto* constraint,
      std::optional<ModularConstraintScaling> modular = std::nullopt);

  // Scales an individual EnforcedLinearConstraint and adds it to the given
  // CpModelProto.
  //
  // This is a wrapper around the other ScaleAndAddConstraint() that uses the
  // vars, coeffs, lower_bound, upper_bound and enforcements of the given
  // mp_constraint, transformed according to the scaling and shift of each
  // variable in `states` (see MipVariableState).
  absl::Status ScaleAndAddConstraint(
      const EnforcedLinearConstraint& mp_constraint,
      absl::Span<const MipVariableState> states, CpModelProto* cp_model,
      std::optional<ModularConstraintScaling> modular = std::nullopt);

  // Statistics over all scaled constraints. This can be inspected to know the
  // final error produced by ScaleAndAddConstraint().
  double max_relative_coeff_error = 0.0;
  double max_absolute_rhs_error = 0.0;
  double max_scaling_factor = 0.0;
  double min_scaling_factor = std::numeric_limits<double>::infinity();
  double max_discretization_error = 0.0;
  double max_rounding_error = 0.0;
  double max_unrounding_error = 0.0;

  // Used when params.use_mip_tight_bounds() is true.
  TightScalingErrorHelper tight_error_helper;

  // Statistics for trivial constraints.
  int64_t num_trivial_constraints = 0;

  // Parameters. Whether we ignore or copy the mp_constraint.name() field.
  bool keep_names = false;

  // Bounds [lb, ub] on (discretized_var - original_var) for each variable,
  // used to compute the constraint error due to variable discretization.
  std::vector<double> var_discretization_error_lb;
  std::vector<double> var_discretization_error_ub;

  // Parameters passed to FindBestScalingAndComputeErrors(), see documentation
  // there to understand their meaning.
  bool use_tight_bounds = false;
  bool use_proper_relaxation = false;
  double tight_precision = 1e-8;
  double wanted_precision = 1e-6;
  int64_t scaling_target = int64_t{1} << 50;

  // Private temporary field to reuse memory.
  std::vector<int> var_indices;
  std::vector<double> coefficients;
  std::vector<double> lower_bounds;
  std::vector<double> upper_bounds;
  std::vector<int64_t> int_lower_bounds;
  std::vector<int64_t> int_upper_bounds;
  std::vector<double> tmp_transformed_coeffs;
  std::vector<int64_t> tmp_enforcement_values;
  TightSum tmp_discretization_lb;
  TightSum tmp_discretization_ub;
};

// Given a set of edges on a graph with nodes [0, n), can we partition the
// nodes into two subsets such that all the edge "constraints" (same_partition
// or not) are satisfied?
//
// We are a bit more generic if the set of edges form disjoint components.
// We will return for each node a number partition[i] equal to:
// - -1 if there is no possible partition for this component.
// - 0/1 if this component can be properly partitioned.
struct EdgeConstraintForPartition {
  int a;
  int b;
  bool same_partition;
};
std::vector<int> PartitionGraph(
    int n, absl::Span<const EdgeConstraintForPartition> edges);

// Initializes the state of all variables from the current wrapper bounds and
// integrality (no scaling, no shift, no error).
std::vector<MipVariableState> InitialVariableStates(
    const MPModelProtoWrapper& mp_model);

// Writes back the given states into `mp_model`: variable bounds and
// integrality, scaled constraint coefficients, shifted constraint bounds,
// scaled objective and shifted objective offset, and scaled/shifted solution
// hint. This is mainly useful for tests.
void ApplyVariableStatesToModel(absl::Span<const MipVariableState> states,
                                MPModelProto* mp_model);

// Splits `mp_model` into pure integer constraints (constraints with no
// continuous variable with a non-zero coefficient) and disjoint residual
// continuous components (connected components of continuous variables linked
// by shared constraints). Always-true constraints (`lb == -inf && ub == +inf`)
// are omitted. All indices refer to the MPModelProtoWrapper.
struct ResidualContinuousComponents {
  std::vector<int> pure_integer_constraints;
  std::vector<ResidualContinuousComponent> components;
};
ResidualContinuousComponents SplitIntoResidualContinuousComponents(
    const MPModelProtoWrapper& mp_model);

// Statistics accumulated by the per-component functions below, and the
// corresponding logging functions.
struct TumStats {
  int num_components = 0;
  int num_tum_components = 0;
  int num_2_modular_components = 0;
  int num_new_tum_integers = 0;
  int num_new_2_modular_integers = 0;
  int num_modular_constraints = 0;
  double max_var_bound_error = 0.0;
  int num_common_scaled = 0;
  double min_common_scaling = std::numeric_limits<double>::infinity();
  double max_common_scaling = 0.0;

  int num_bound_tightened_components = 0;
  int num_tightened_bounds = 0;
  double old_min = std::numeric_limits<double>::infinity();
  double old_max = -std::numeric_limits<double>::infinity();
  double new_min = std::numeric_limits<double>::infinity();
  double new_max = -std::numeric_limits<double>::infinity();
};
void LogTumStats(const TumStats& stats, SolverLogger* logger);

struct OneVariableComponentStats {
  int num_components = 0;
  int num_integers = 0;
  int num_fixed = 0;
  int num_pow2_preferred = 0;
  int num_missed = 0;
  double max_scaling = 0.0;
};
void LogOneVariableComponentStats(const OneVariableComponentStats& stats,
                                  SolverLogger* logger);

struct PrecisionScalingStats {
  int num_scaled = 0;
  int min_exponent = std::numeric_limits<int>::max();
  int max_exponent = 0;
};
void LogPrecisionScalingStats(const SatParameters& params,
                              const MPModelProto& mp_model,
                              absl::Span<const MipVariableState> states,
                              const PrecisionScalingStats& stats,
                              SolverLogger* logger);

struct ShiftStats {
  int64_t num_shifted = 0;
  double max_shift = 0.0;
};
void LogShiftStats(const ShiftStats& stats, SolverLogger* logger);

// Once all integer variables have a value, we can split the model into separate
// components. If a given component is totally unimodular, then there exists a
// solution of that component with variables taking only integer values; we mark
// them as such.
//
// Note that we don't detect all totally unimodular residual matrices (TUM), we
// use Heller & Tompkins's criterion (on A or A^T, after iteratively peeling
// singleton rows/columns and scaling rows/columns to {-1, 0, 1}):
//
// A matrix A with entries in {-1, 0, 1} is TUM if:
// 1/ Every column contains at most two non-zero entries.
// 2/ The rows can be partitioned into two sets, R and R' such that for any
//    column with exactly two entries:
//     - if they are of the same sign, one row is in R and the other in R'
//     - if they are of opposite sign, both rows are in the same set (R or R').
//
// Note that in addition to the matrix, we also need the constraint/variable
// bounds of a totally unimodular LP to be integer to guarantee existence of a
// fully integer solution.
//
// Note that if there are duplicate variables in a constraint (not presolved),
// this might fail to detect TUM submatrices. It will however not wrongly detect
// any.
//
// If params.mip_automatically_scale_variables() is true, we also detect
// components that can be made integral via variable scaling (for example,
// 2-modular components where any LP optimal basic solution is half-integral, or
// components with rational bounds/RHS or column scaling). When a component with
// unit column scaling cannot be made integral and mip_scale_for_precision is
// enabled, we still scale all variables in the component by a common
// power-of-two factor to preserve its +/-1 coefficients.
//
// Note that if some constraints in `mp_model` are enforced by external integer
// variables (e.g., via MPModelProtoWrapper), it is valid to pass the underlying
// MPModelProto directly (treating enforced constraints as always active, rather
// than skipping them): for any fixed integer assignment of the external
// variables, inactive enforced constraints simply remove rows from the
// component LP, which preserves total unimodularity (or 2-modularity), vertex
// integrality, and the worst-case vertex bounds computed when tightening.
//
// The variables marked as integer have their bounds enlarged (when
// `mip_proper_relaxation` is true) or snapped to multiples of the
// `modular_factor`, and this returns the scaling to use for each of
// `compo.constraints` in ConstraintScaler::ScaleAndAddConstraint(), or an empty
// vector if the component was not made integral.
//
// This processes a single residual continuous component (whose
// `var_to_local_col` must be set) and only updates `states`.
std::vector<ModularConstraintScaling> MarkComponentTotallyUnimodularAsInteger(
    const SatParameters& params, const ResidualContinuousComponent& compo,
    const MPModelProtoWrapper& mp_model, absl::Span<MipVariableState> states,
    TumStats* stats);

// Processes a residual continuous component with a single continuous variable
// x, which is thus the unique non-integer of all its constraints.
//
// In a minimization problem, if all these constraints are "nice" (integer
// coefficients and bounds after scaling x by a rational factor s, see
// GetIntegralityMultiplier()) then there is an optimal solution where s * x is
// integer: on the side where the objective pushes x, the tightest active
// constraint (or the bound of x) is integral in the scaled space. Note that
// when x has no objective, this is true for both sides and we require both.
// Such a x is marked as integer with scaling s and tight bounds
// [error_lb, error_ub] on (discretized_x' - x') accounting for the approximate
// rationals (see FindRationalFactor()) and the errors on the other variables.
//
// With `mip_scale_for_precision`, this is only done if the resulting worst
// case error on x is not larger than the one of the power of two scaling that
// ScaleComponentContinuousVariablesForGivenPrecision() would use otherwise.
// With a large rational s, the latter is usually as precise and avoids large
// constraint scaling factors.
//
// If the scaled domain of x contains no integer point, x is fixed to the
// relevant bound instead.
//
// This only updates `states`, `compo.var_to_local_col` must be set.
void ScaleOneVariableResidualMatrix(const SatParameters& params,
                                    const ResidualContinuousComponent& compo,
                                    const MPModelProtoWrapper& mp_model,
                                    absl::Span<MipVariableState> states,
                                    OneVariableComponentStats* stats);

// Uses the same TUM detection on the whole model, before it is split into
// components, to only tighten the bounds of the variables with a large domain
// that belong to a TUM component whose LP vertices are integral whatever the
// value of the external integer variables (Hoffman-Kruskal). Integer variables
// with a large domain are considered too (e.g. big-M scheduling formulations).
void TightenResidualTotallyUnimodularVariableBounds(
    const SatParameters& params, MPModelProtoWrapper* mp_model,
    SolverLogger* logger);

// Multiplies all continuous variables of the component by
// params.mip_var_scaling(). If the domain of a variable is going to have larger
// values than mip_max_bound, then we scale to have the max domain magnitude
// equal to mip_max_bound instead. Variables already scaled are left alone.
void ScaleComponentContinuousVariables(const SatParameters& params,
                                       const ResidualContinuousComponent& compo,
                                       absl::Span<MipVariableState> states);

// For each constraint, computes the scaling factor that allows to satisfy
// mip_wanted_precision and records the power-of-two headroom left up to
// mip_max_activity_exponent. Then scales each continuous variable by the
// minimum headroom across all constraints in which it appears, capped by the
// maximum scaling needed for mip_wanted_precision and the maximum valid CP-SAT
// variable domain.
void ScaleComponentContinuousVariablesForGivenPrecision(
    const SatParameters& params, const ResidualContinuousComponent& compo,
    const MPModelProtoWrapper& mp_model, absl::Span<MipVariableState> states,
    PrecisionScalingStats* stats);

// Shifts the domain of each variable in `vars` by `get_shift(state)` (expressed
// in the current scaled space) so that it ends up closer to zero. The shift of
// an integer variable must be integer, and a variable must not be shifted more
// than once (`state.shift == 0.0` whenever `get_shift(state) != 0.0`) so
// `state.shift` stays exact.
void ShiftVariables(
    const std::function<double(const MipVariableState&)>& get_shift,
    absl::Span<const int> vars, absl::Span<MipVariableState> states,
    ShiftStats* stats);

// This simple step helps and should be done first. Returns false if the model
// is trivially infeasible because of crossing bounds. The version with the
// scaling, make the bound of the scaled variable integer.
bool MakeBoundsOfIntegerVariablesInteger(const SatParameters& params,
                                         MPModelProto* mp_model,
                                         SolverLogger* logger);
bool MakeBoundsOfIntegerVariablesInteger(
    const SatParameters& params, absl::Span<const double> var_scaling,
    absl::Span<const double> integer_error_lb,
    absl::Span<const double> integer_error_ub, MPModelProto* mp_model,
    SolverLogger* logger);

// Same as above but only for the integer variables in `vars` and using the
// scaling and error bounds from `states`. Increments `num_changes`.
bool MakeBoundsOfIntegerVariablesInteger(const SatParameters& params,
                                         absl::Span<const int> vars,
                                         absl::Span<MipVariableState> states,
                                         int64_t* num_changes,
                                         SolverLogger* logger);

// Helpers used in bound propagation. We special case the coefficient == 1.0 as
// this is frequent and we don't want to artificially add a really small error.
double ExactScaledSumUb(double a, double b, double divisor);  // Forward.
inline double ExactScaledSumLb(double a, double b, double divisor = 1.0) {
  DCHECK(std::isfinite(divisor));
  if (divisor < 0.0) return ExactScaledSumUb(-a, -b, -divisor);
  if (!std::isfinite(a)) return a;
  if (!std::isfinite(b)) return b;
  const double exact_sum_lb = TightSum().Add(a).Add(b).TightLowerBound();
  return divisor == 1.0 ? exact_sum_lb : FastStepDown(exact_sum_lb / divisor);
}
inline double ExactScaledSumUb(double a, double b, double divisor = 1.0) {
  DCHECK(std::isfinite(divisor));
  if (divisor < 0.0) return ExactScaledSumLb(-a, -b, -divisor);
  if (!std::isfinite(a)) return a;
  if (!std::isfinite(b)) return b;
  const double exact_sum_ub = TightSum().Add(a).Add(b).TightUpperBound();
  return divisor == 1.0 ? exact_sum_ub : FastStepUp(exact_sum_ub / divisor);
}

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
                          const MPConstraintProto& ct);

  template <bool exact = true>
  void FillFromConstraint(absl::Span<const double> var_lbs,
                          absl::Span<const double> var_ubs,
                          const MPConstraintProto& ct, bool fill_min,
                          bool fill_max) {
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
void ChangeLargeBoundsToInfinity(double max_magnitude, MPModelProto* mp_model,
                                 SolverLogger* logger);

// Performs some extra tests on the given MPModelProto and returns false if one
// is not satisfied. These are needed before trying to convert it to the native
// CP-SAT format.
bool MPModelProtoValidationBeforeConversion(const SatParameters& params,
                                            const MPModelProto& mp_model,
                                            SolverLogger* logger);

// To satisfy our scaling requirements, any term that is almost zero can just
// be set to zero. We need to do that before operations like
// DetectImpliedIntegers(), because really low coefficients can cause issues
// and might lead to less detection.
void RemoveNearZeroTerms(const SatParameters& params, MPModelProto* mp_model,
                         SolverLogger* logger);

// This will mark as integer the continuous variables that are the unique
// non-integer of an equality constraint with "nice" coefficients and bounds
// (see GetIntegralityMultiplier()), possibly with a rational scaling. Note that
// it can also discover variables of the form coeff * Integer + offset, and will
// change the model so that these are marked as integers. It is why we return
// both a scaling and an offset to transform the solution back to its original
// domain. The continuous variables that are the unique non-integer of
// inequalities only are handled later by ScaleOneVariableResidualMatrix().
//
// TODO(user): Actually implement the offset part. This currently only happens
// on the 3 neos-46470* miplib problems where we have a non-integer rhs.
// Updates in-place tight bounds `[integer_error_lb[v], integer_error_ub[v]]`
// (with `integer_error_lb[v] <= 0 <= integer_error_ub[v]`, sized to
// `mp_model->num_variables()`) on `(discretized_v - scaled_v)` in the scaled
// variable space for variables marked as integer by this pass.
std::vector<double> DetectImpliedIntegers(const SatParameters& params,
                                          MPModelProtoWrapper* mp_model,
                                          absl::Span<double> integer_error_lb,
                                          absl::Span<double> integer_error_ub,
                                          SolverLogger* logger);

// Clamps variable bounds to [-mip_max_bound, mip_max_bound]. Returns false if
// a variable domain does not intersect this interval.
bool ClampVariableBounds(const SatParameters& params, MPModelProto* mp_model,
                         SolverLogger* logger, int* num_clamped = nullptr);

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
// discretization error bounds. Returns false if the variable cannot take an
// integer value. `objective_coefficient` is the one from the wrapper (only its
// sign matters).
bool ConvertVariableToCpVariable(const SatParameters& params, int var_index,
                                 double objective_coefficient,
                                 MipVariableState* state,
                                 IntegerVariableProto* cp_var,
                                 int* num_small_domains);

// Logs constraint scaling and discretization statistics collected in `scaler`.
void DisplayConstraintScalingStats(const SatParameters& params,
                                   const ConstraintScaler& scaler,
                                   int num_clamped, int num_small_domains,
                                   SolverLogger* logger);

// Populates `cp_model->floating_point_objective()` from `mp_model` transformed
// according to `states`.
void PopulateCpModelFloatingPointObjective(
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
