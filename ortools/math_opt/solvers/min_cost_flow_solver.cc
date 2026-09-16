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

#include "ortools/math_opt/solvers/min_cost_flow_solver.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "ortools/base/map_util.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/strong_int.h"
#include "ortools/base/strong_vector.h"
#include "ortools/graph/min_cost_flow.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/callback_validator.h"
#include "ortools/util/fp_roundtrip_conv.h"
#include "ortools/util/solve_interrupter.h"
#include "ortools/util/status_macros.h"

namespace operations_research::math_opt {

namespace {

using CostValue = SimpleMinCostFlow::CostValue;
using FlowQuantity = SimpleMinCostFlow::FlowQuantity;
using LinearConstraintId = int64_t;
DEFINE_STRONG_INT_TYPE(NodeIndex, int32_t);

struct MinCostFlowArc {
  std::optional<NodeIndex> src = std::nullopt;
  std::optional<NodeIndex> dst = std::nullopt;
};

// MinCostFlow solver does not support any problem structures.
constexpr SupportedProblemStructures kMinCostFlowSupportedStructures;

// This tolerance is used to check if a value is close enough to an integer to
// be considered an integer.
// This is similar to solver-specific tolerances such as `IntFeasTol` for
// Gurobi. The value 1e-6 is derived from the default values of such tolerances
// for other solvers to have consistent behavior.
// Ideally, we should have an integrality tolerance exposed in the
// SolveParametersProto that is propagated to all solvers that need it.
constexpr double kMinCostFlowIntegralityTolerance = 1e-6;

constexpr absl::string_view kModelNotSupportedPrefix =
    "model structure is not supported by MinCostFlow solver: ";

// Rounds a double value to an integer of type IntType, checking that it is
// close enough to an integer and that it is within the range of representable
// numbers for IntType.
template <typename IntType>
absl::StatusOr<IntType> RoundToInteger(const double value,
                                       const absl::string_view type_name) {
  // We use `! <=` here instead of `>` so that we reject NaN as well.
  if (!(std::abs(value - std::round(value)) <=
        kMinCostFlowIntegralityTolerance)) {
    return ortools::InvalidArgumentErrorBuilder()
           << kModelNotSupportedPrefix << "input value "
           << RoundTripDoubleFormat(value)
           << " is not close enough to an integer";
  }
  const double rounded = std::round(value);
  if (rounded <= -static_cast<double>(std::numeric_limits<IntType>::max())) {
    return ortools::InvalidArgumentErrorBuilder()
           << kModelNotSupportedPrefix << "input value "
           << RoundTripDoubleFormat(value)
           << " is too small to be represented as a " << type_name;
  }
  if (rounded >= static_cast<double>(std::numeric_limits<IntType>::max())) {
    return ortools::InvalidArgumentErrorBuilder()
           << kModelNotSupportedPrefix << "input value "
           << RoundTripDoubleFormat(value)
           << " is too large to be represented as a " << type_name;
  }
  return static_cast<IntType>(rounded);
}

absl::Status CheckSolveParameters(const SolveParametersProto& parameters) {
  std::vector<std::string> warnings;

  if (parameters.has_time_limit()) {
    warnings.push_back(
        "MinCostFlow solver does not support 'time_limit' parameter");
  }
  if (parameters.has_iteration_limit()) {
    warnings.push_back(
        "MinCostFlow solver does not support 'iteration_limit' parameter");
  }
  if (parameters.has_node_limit()) {
    warnings.push_back(
        "MinCostFlow solver does not support 'node_limit' parameter");
  }
  if (parameters.has_cutoff_limit()) {
    warnings.push_back(
        "MinCostFlow solver does not support 'cutoff_limit' parameter");
  }
  if (parameters.has_objective_limit()) {
    warnings.push_back(
        "MinCostFlow solver does not support 'objective_limit' parameter");
  }
  if (parameters.has_best_bound_limit()) {
    warnings.push_back(
        "MinCostFlow solver does not support 'best_bound_limit' parameter");
  }
  if (parameters.has_solution_limit()) {
    warnings.push_back(
        "MinCostFlow solver does not support 'solution_limit' parameter");
  }
  if (parameters.has_threads()) {
    warnings.push_back(
        "MinCostFlow solver does not support 'threads' parameter");
  }
  if (parameters.has_random_seed()) {
    warnings.push_back(
        "MinCostFlow solver does not support 'random_seed' parameter");
  }
  if (parameters.has_absolute_gap_tolerance()) {
    warnings.push_back(
        "MinCostFlow solver does not support 'absolute_gap_tolerance' "
        "parameter");
  }
  if (parameters.has_relative_gap_tolerance()) {
    warnings.push_back(
        "MinCostFlow solver does not support 'relative_gap_tolerance' "
        "parameter");
  }
  if (parameters.has_solution_pool_size()) {
    warnings.push_back(
        "MinCostFlow solver does not support 'solution_pool_size' parameter");
  }
  if (parameters.lp_algorithm() != LP_ALGORITHM_UNSPECIFIED) {
    warnings.push_back(
        "MinCostFlow solver does not support 'lp_algorithm' parameter");
  }
  if (parameters.presolve() != EMPHASIS_UNSPECIFIED) {
    warnings.push_back(
        "MinCostFlow solver does not support 'presolve' parameter");
  }
  if (parameters.cuts() != EMPHASIS_UNSPECIFIED) {
    warnings.push_back("MinCostFlow solver does not support 'cuts' parameter");
  }
  if (parameters.heuristics() != EMPHASIS_UNSPECIFIED) {
    warnings.push_back(
        "MinCostFlow solver does not support 'heuristics' parameter");
  }
  if (parameters.scaling() != EMPHASIS_UNSPECIFIED) {
    warnings.push_back(
        "MinCostFlow solver does not support 'scaling' parameter");
  }

  if (!warnings.empty()) {
    return absl::InvalidArgumentError(absl::StrJoin(warnings, "; "));
  }
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<SolverInterface>> MinCostFlowSolver::New(
    const ModelProto& model, const InitArgs& /*init_args*/) {
  ABSL_RETURN_IF_ERROR(
      ModelIsSupported(model, kMinCostFlowSupportedStructures, "MinCostFlow"));

  const bool is_maximize = model.objective().maximize();
  const double objective_offset = model.objective().offset();

  const LinearConstraintsProto& constraints = model.linear_constraints();
  const VariablesProto& variables = model.variables();

  auto mcf = std::make_unique<SimpleMinCostFlow>(
      /*reserve_num_nodes=*/NumConstraints(constraints),
      /*reserve_num_arcs=*/NumVariables(variables));

  // Process constraints: Validate bounds, map IDs, and set supplies.
  absl::flat_hash_map<LinearConstraintId, NodeIndex> constraint_to_node;
  constraint_to_node.reserve(NumConstraints(constraints));
  for (int i = 0; i < NumConstraints(constraints); ++i) {
    const LinearConstraintId c(constraints.ids(i));
    const double lb = constraints.lower_bounds(i);
    const double ub = constraints.upper_bounds(i);

    OR_ASSIGN_OR_RETURN3(const FlowQuantity integer_lb,
                         RoundToInteger<FlowQuantity>(lb, "flow quantity"),
                         _ << "invalid constraint " << c << " lower-bound");
    OR_ASSIGN_OR_RETURN3(const FlowQuantity integer_ub,
                         RoundToInteger<FlowQuantity>(ub, "flow quantity"),
                         _ << "invalid constraint " << c << " upper-bound");
    if (integer_lb != integer_ub) {
      return ortools::InvalidArgumentErrorBuilder()
             << kModelNotSupportedPrefix << "constraint " << c
             << " is not an equality, the lower-bound "
             << RoundTripDoubleFormat(lb) << " rounds to " << integer_lb
             << " but the upper-bound " << RoundTripDoubleFormat(ub)
             << " rounds to " << integer_ub;
    }
    constraint_to_node[c] = NodeIndex(i);

    // Per convention, we interpret the bound as supply.
    mcf->SetNodeSupply(i, integer_lb);
  }

  // Process matrix: Aggregate src and dst nodes for each variable.
  absl::flat_hash_map<VariableId, MinCostFlowArc> var_to_arc;
  var_to_arc.reserve(NumVariables(variables));
  util_intops::StrongVector<NodeIndex, int> node_degree(
      NodeIndex(NumConstraints(constraints)), 0);

  const SparseDoubleMatrixProto& matrix = model.linear_constraint_matrix();
  for (int i = 0; i < NumMatrixNonzeros(matrix); ++i) {
    const LinearConstraintId c(matrix.row_ids(i));
    const VariableId v(matrix.column_ids(i));
    const double val = matrix.coefficients(i);

    auto& arc = var_to_arc[v];
    const NodeIndex node_id = constraint_to_node[c];

    OR_ASSIGN_OR_RETURN3(const FlowQuantity integer_coefficient,
                         RoundToInteger<FlowQuantity>(val, "flow quantity"),
                         _ << "invalid coefficient for constraint " << c
                           << " and variable " << v);

    // Per convention, we interpret +1 as outflow and -1 as inflow, consistent
    // with the interpretation of the constraint bound as supply.
    if (integer_coefficient == 1) {
      if (arc.src.has_value()) {
        return ortools::InvalidArgumentErrorBuilder()
               << kModelNotSupportedPrefix << "variable " << v
               << " has multiple +1 coefficients";
      }
      arc.src = node_id;
      ++node_degree[node_id];
    } else if (integer_coefficient == -1) {
      if (arc.dst.has_value()) {
        return ortools::InvalidArgumentErrorBuilder()
               << kModelNotSupportedPrefix << "variable " << v
               << " has multiple -1 coefficients";
      }
      arc.dst = node_id;
      ++node_degree[node_id];
    } else {
      return ortools::InvalidArgumentErrorBuilder()
             << kModelNotSupportedPrefix << "matrix coefficient for variable "
             << v << " in constraint " << c << " is " << integer_coefficient
             << ", not +1 or -1";
    }
  }

  int64_t max_node_degree = 1;
  for (const NodeIndex index : node_degree.index_range()) {
    max_node_degree = std::max<int64_t>(max_node_degree, node_degree[index]);
  }

  // Process objective: Validate and map costs.
  absl::flat_hash_map<VariableId, CostValue> var_to_obj;
  const SparseDoubleVectorProto& obj_coeffs =
      model.objective().linear_coefficients();
  var_to_obj.reserve(obj_coeffs.ids_size());

  for (int i = 0; i < obj_coeffs.ids_size(); ++i) {
    const VariableId v(obj_coeffs.ids(i));
    const double obj_coeff = obj_coeffs.values(i);

    OR_ASSIGN_OR_RETURN3(
        const CostValue integer_cost,
        RoundToInteger<CostValue>(obj_coeff, "cost value"),
        _ << "invalid objective coefficient for variable " << v);
    var_to_obj[v] = integer_cost;
  }

  // Process variables: Validate bounds, extract arc data, and build graph.
  util_intops::StrongVector<ArcIndex, VariableId> arc_to_var;
  arc_to_var.reserve(ArcIndex(NumVariables(variables)));

  for (int i = 0; i < NumVariables(variables); ++i) {
    const VariableId v(variables.ids(i));

    const double lb = variables.lower_bounds(i);
    const double ub = variables.upper_bounds(i);
    OR_ASSIGN_OR_RETURN3(const FlowQuantity integer_lb,
                         RoundToInteger<FlowQuantity>(lb, "flow quantity"),
                         _ << "invalid lower bound for variable " << v);
    if (integer_lb != 0) {
      return ortools::InvalidArgumentErrorBuilder()
             << kModelNotSupportedPrefix << "variable " << v
             << " lower bound is not 0";
    }
    if (std::isfinite(ub)) {
      ABSL_RETURN_IF_ERROR(
          RoundToInteger<FlowQuantity>(ub, "flow quantity").status())
          << "invalid upper bound for variable " << v;
    }

    const MinCostFlowArc* arc_nodes = gtl::FindOrNull(var_to_arc, v);
    if (arc_nodes == nullptr) {
      return ortools::InvalidArgumentErrorBuilder()
             << kModelNotSupportedPrefix << "variable " << v
             << " does not appear in any constraints";
    }

    if (!arc_nodes->src.has_value()) {
      return ortools::InvalidArgumentErrorBuilder()
             << kModelNotSupportedPrefix << "variable " << v
             << " does not have a +1 coefficient";
    }
    const NodeIndex src = *arc_nodes->src;

    if (!arc_nodes->dst.has_value()) {
      return ortools::InvalidArgumentErrorBuilder()
             << kModelNotSupportedPrefix << "variable " << v
             << " does not have a -1 coefficient";
    }
    const NodeIndex dst = *arc_nodes->dst;

    CostValue cost = gtl::FindWithDefault(var_to_obj, v, CostValue{0});
    if (is_maximize) {
      cost = -cost;
    }

    // Default unbounded capacity. See BAD_CAPACITY_RANGE comment in
    // min_cost_flow.h for why we don't use
    // std::numeric_limits<FlowQuantity>::max(). Note that this scaling does not
    // prevent all possible BAD_CAPACITY_RANGE errors.
    // Ideally, the min-cost flow solver should support unbounded capacities and
    // scale internally.
    const FlowQuantity unbounded_capacity =
        std::numeric_limits<FlowQuantity>::max() / max_node_degree;

    FlowQuantity capacity = unbounded_capacity;
    if (ub < static_cast<double>(unbounded_capacity)) {
      capacity = static_cast<FlowQuantity>(std::round(ub));
    }

    mcf->AddArcWithCapacityAndUnitCost(src.value(), dst.value(), capacity,
                                       cost);
    arc_to_var.push_back(v);
  }

  return absl::WrapUnique(new MinCostFlowSolver(
      std::move(mcf), std::move(arc_to_var), is_maximize, objective_offset));
}

MinCostFlowSolver::MinCostFlowSolver(
    std::unique_ptr<SimpleMinCostFlow> mcf,
    util_intops::StrongVector<ArcIndex, VariableId> arc_to_var,
    bool is_maximize, double objective_offset)
    : mcf_(std::move(mcf)),
      arc_to_var_(std::move(arc_to_var)),
      is_maximize_(is_maximize),
      objective_offset_(objective_offset) {}

absl::StatusOr<SolveResultProto> MinCostFlowSolver::MakeSolveResult(
    SimpleMinCostFlow::Status status) const {
  SolveResultProto result;
  TerminationProto& termination = *result.mutable_termination();

  switch (status) {
    case SimpleMinCostFlow::OPTIMAL: {
      double obj_val = static_cast<double>(mcf_->OptimalCost());
      if (is_maximize_) {
        obj_val = -obj_val;
      }
      obj_val += objective_offset_;

      termination = OptimalTerminationProto(obj_val, obj_val);

      SolutionProto& solution = *result.add_solutions();
      PrimalSolutionProto& primal = *solution.mutable_primal_solution();
      primal.set_objective_value(obj_val);
      primal.set_feasibility_status(SOLUTION_STATUS_FEASIBLE);

      SparseDoubleVectorProto& values = *primal.mutable_variable_values();
      for (const ArcIndex arc : arc_to_var_.index_range()) {
        values.add_ids(arc_to_var_[arc]);
        values.add_values(static_cast<double>(mcf_->Flow(arc.value())));
      }
      return result;
    }
    case SimpleMinCostFlow::INFEASIBLE: {
      termination = InfeasibleTerminationProto(
          is_maximize_,
          /*dual_feasibility_status=*/FEASIBILITY_STATUS_UNDETERMINED,
          "problem is infeasible");
      return result;
    }
    case SimpleMinCostFlow::UNBALANCED: {
      termination = InfeasibleTerminationProto(
          is_maximize_,
          /*dual_feasibility_status=*/FEASIBILITY_STATUS_UNDETERMINED,
          "problem is unbalanced (sum of supplies != sum of demands)");
      return result;
    }
    case SimpleMinCostFlow::FEASIBLE: {
      return ortools::InternalErrorBuilder()
             << "MinCostFlow solver returned status " << StatusName(status)
             << ", which is not expected for this solver";
    }
    case SimpleMinCostFlow::NOT_SOLVED:
    case SimpleMinCostFlow::BAD_RESULT:
    case SimpleMinCostFlow::BAD_COST_RANGE:
    case SimpleMinCostFlow::BAD_CAPACITY_RANGE: {
      termination = TerminateForReason(
          is_maximize_, TERMINATION_REASON_OTHER_ERROR,
          absl::StrCat("MinCostFlow solver failed: ", StatusName(status)));
      return result;
    }
  }

  return ortools::InternalErrorBuilder() << "unrecognized status: " << status;
}

absl::StatusOr<SolveResultProto> MinCostFlowSolver::Solve(
    const SolveParametersProto& parameters,
    const ModelSolveParametersProto& /*model_parameters*/,
    MessageCallback /*message_cb*/,
    const CallbackRegistrationProto& callback_registration, Callback /*cb*/,
    const SolveInterrupter* absl_nullable /*interrupter*/) {
  ABSL_RETURN_IF_ERROR(CheckRegisteredCallbackEvents(callback_registration,
                                                     /*supported_events=*/{}));
  ABSL_RETURN_IF_ERROR(CheckSolveParameters(parameters));

  const absl::Time start = absl::Now();
  const SimpleMinCostFlow::Status status = mcf_->Solve();

  ABSL_ASSIGN_OR_RETURN(SolveResultProto result, MakeSolveResult(status));
  OR_ASSIGN_OR_RETURN3(*result.mutable_solve_stats()->mutable_solve_time(),
                       util_time::EncodeGoogleApiProto(absl::Now() - start),
                       _ << "can't encode solve_time");
  return result;
}

absl::StatusOr<bool> MinCostFlowSolver::Update(const ModelUpdateProto&) {
  return false;
}

absl::StatusOr<ComputeInfeasibleSubsystemResultProto>
MinCostFlowSolver::ComputeInfeasibleSubsystem(
    const SolveParametersProto& /*parameters*/, MessageCallback /*message_cb*/,
    const SolveInterrupter* absl_nullable /*interrupter*/) {
  return absl::UnimplementedError(
      "MinCostFlow solver does not support ComputeInfeasibleSubsystem");
}

MATH_OPT_REGISTER_SOLVER(SOLVER_TYPE_MIN_COST_FLOW, MinCostFlowSolver::New);

}  // namespace operations_research::math_opt
