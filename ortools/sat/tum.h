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

// Totally unimodular (TUM) and 2-modular residual matrix detection, variable
// bound tightening, and modular scaling for MIP-to-CP-SAT conversion.

#ifndef ORTOOLS_SAT_TUM_H_
#define ORTOOLS_SAT_TUM_H_

#include <limits>
#include <vector>

#include "absl/types/span.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/lp_utils.h"
#include "ortools/sat/mp_model_wrapper.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/logging.h"

namespace operations_research {
namespace sat {

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

// Statistics accumulated by the per-component TUM functions below, and the
// corresponding logging function.
struct TumStats {
  int num_components = 0;
  int num_tum_components = 0;
  int num_2_modular_components = 0;
  int num_new_tum_integers = 0;
  int num_new_2_modular_integers = 0;
  int num_modular_constraints = 0;
  double max_var_bound_error = 0.0;
  double objective_error_bound = 0.0;

  // The variables made integral are shifted like any other integer variable,
  // see ShiftVariables().
  ShiftStats shift_stats;

  // Components made integral via outward variable or row bound rounding when
  // exact unrelaxed modular scaling was not applicable, see
  // DetectAndConvertTotallyUnimodularComponent().
  int num_relaxed_modular_components = 0;
  int num_relaxed_modular_integers = 0;
  double min_relaxed_modular_scaling = std::numeric_limits<double>::infinity();
  double max_relaxed_modular_scaling = 0.0;

  int num_bound_tightened_components = 0;
  int num_tightened_bounds = 0;
  double old_min = std::numeric_limits<double>::infinity();
  double old_max = -std::numeric_limits<double>::infinity();
  double new_min = std::numeric_limits<double>::infinity();
  double new_max = -std::numeric_limits<double>::infinity();
};
void LogTumStats(const TumStats& stats, SolverLogger* logger);

// Once all integer variables have a value, we can split the model into separate
// components. If params.mip_automatically_scale_variables() is true and a given
// component is totally unimodular (or 2-modular), we try to scale it so that
// its variables can be marked as integer (preserving
// `mip_tight_precision`-feasibility), and we then convert its constraints
// directly.
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
// Note that if there are duplicate variables in a constraint (not presolved),
// this might fail to detect TUM submatrices. It will however not wrongly detect
// any.
//
// A TUM (`modular_factor = 1`) or 2-modular (`modular_factor = 2`) component is
// made integral by scaling it with a multiplier `s = q_odd * 2^k`, as long as
// the scaled variable domains stay below 1e11 and the constraint activities
// below `2^mip_max_activity_exponent`. We try, by increasing value of `s`:
// - An odd `q_odd` (found via continued fractions) making all the column
//   scales, external integer coefficients, variable bounds and active
//   constraint bounds integer within `mip_tight_precision`: the bounds are then
//   snapped to these integers.
// - An odd `q_odd` making only the column scales and external coefficients
//   integer, or `q_odd = 1` when all the column scales are one: the bounds are
//   then rounded outward, with `2^k` large enough for the rounding errors to be
//   within `mip_wanted_precision`. With `mip_proper_relaxation`, if this
//   precision cannot be reached with `q_odd = 1`, we fall back to the largest
//   `2^k >= 2` that does not overflow.
// In all cases, the constraint bounds are only rounded inward (within
// `mip_tight_precision`) while the total of these tightenings over the
// component stays below one, which keeps an integer solution for any
// `mip_tight_precision`-feasible one.
//
// Note that if some constraints in `mp_model` are enforced by external integer
// variables (e.g., via MPModelProtoWrapper), it is valid to pass the underlying
// MPModelProto directly (treating enforced constraints as always active, rather
// than skipping them): for any fixed integer assignment of the external
// variables, inactive enforced constraints simply remove rows from the
// component LP, which preserves total unimodularity (or 2-modularity), vertex
// integrality, and the worst-case vertex bounds computed when tightening.
//
// Returns true if the component was made integral. Its variables are then
// marked as integer, scaled, with bounds rounded outward (or snapped within
// `mip_tight_precision`) to multiples of `modular_factor`, shifted like any
// other integer variable, and written to `cp_model`, and each of
// `compo.constraints` was scaled accordingly and added to `cp_model` via
// `scaler`. Otherwise, nothing is modified besides the bounds in `states`,
// which may have been tightened.
//
// This processes a single residual continuous component (whose
// `var_to_local_col` must be set). All variables must have finite bounds
// (clamped by ClampVariableBounds()), the external variables of its constraints
// must be in their final state, and the ones used as enforcement must already
// have their domain in `cp_model`.
bool DetectAndConvertTotallyUnimodularComponent(
    const SatParameters& params, const ResidualContinuousComponent& compo,
    const MPModelProtoWrapper& mp_model, absl::Span<MipVariableState> states,
    ConstraintScaler* scaler, CpModelProto* cp_model, TumStats* stats);

// Uses the same TUM detection on the whole model, before it is split into
// components, to only tighten the bounds of the variables with a large domain
// that belong to a TUM component whose LP vertices are integral whatever the
// value of the external integer variables (Hoffman-Kruskal). Integer variables
// with a large domain are considered too (e.g. big-M scheduling formulations).
void TightenResidualTotallyUnimodularVariableBounds(
    const SatParameters& params, MPModelProtoWrapper* mp_model,
    SolverLogger* logger);

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_TUM_H_
