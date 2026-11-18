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

#ifndef ORTOOLS_SAT_TUM_H_
#define ORTOOLS_SAT_TUM_H_

#include <limits>
#include <vector>

#include "absl/types/span.h"
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

// Statistics accumulated by the TUM functions below, and the corresponding
// logging function.
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
