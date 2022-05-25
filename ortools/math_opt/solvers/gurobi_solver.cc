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

#include "ortools/math_opt/solvers/gurobi_solver.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <list>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/cleanup.h"
#include "ortools/base/linked_hash_map.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/non_streamable_solver_init_arguments.h"
#include "ortools/math_opt/core/solve_interrupter.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/solvers/gurobi.pb.h"
#include "ortools/math_opt/solvers/gurobi_callback.h"
#include "ortools/math_opt/solvers/gurobi_init_arguments.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/callback_validator.h"
#include "ortools/port/proto_utils.h"

namespace operations_research {
namespace math_opt {
namespace {

absl::StatusOr<std::unique_ptr<Gurobi>> GurobiFromInitArgs(
    const SolverInterface::InitArgs& init_args) {
  // We don't test or return an error for incorrect non streamable arguments
  // type since it is already tested by the Solver class.
  const NonStreamableGurobiInitArguments* const non_streamable_args =
      init_args.non_streamable != nullptr
          ? init_args.non_streamable->ToNonStreamableGurobiInitArguments()
          : nullptr;
  std::unique_ptr<Gurobi> gurobi;
  if (non_streamable_args != nullptr &&
      non_streamable_args->master_env != nullptr) {
    return Gurobi::NewWithSharedMasterEnv(non_streamable_args->master_env);
  } else if (init_args.streamable.has_gurobi() &&
             init_args.streamable.gurobi().has_isv_key()) {
    ASSIGN_OR_RETURN(
        GRBenvUniquePtr env,
        NewMasterEnvironment(init_args.streamable.gurobi().isv_key()));
    return Gurobi::New(std::move(env));
  } else {
    return Gurobi::New();
  }
}

inline BasisStatusProto ConvertVariableStatus(const int status) {
  switch (status) {
    case GRB_BASIC:
      return BASIS_STATUS_BASIC;
    case GRB_NONBASIC_LOWER:
      return BASIS_STATUS_AT_LOWER_BOUND;
    case GRB_NONBASIC_UPPER:
      return BASIS_STATUS_AT_UPPER_BOUND;
    case GRB_SUPERBASIC:
      return BASIS_STATUS_FREE;
    default:
      return BASIS_STATUS_UNSPECIFIED;
  }
}

inline int GrbVariableStatus(const BasisStatusProto status) {
  switch (status) {
    case BASIS_STATUS_BASIC:
      return GRB_BASIC;
    case BASIS_STATUS_AT_LOWER_BOUND:
    case BASIS_STATUS_FIXED_VALUE:
      return GRB_NONBASIC_LOWER;
    case BASIS_STATUS_AT_UPPER_BOUND:
      return GRB_NONBASIC_UPPER;
    case BASIS_STATUS_FREE:
      return GRB_SUPERBASIC;
    case BASIS_STATUS_UNSPECIFIED:
    default:
      LOG(FATAL) << "Unexpected invalid initial_basis.";
      return 0;
  }
}

GurobiParametersProto MergeParameters(
    const SolveParametersProto& solve_parameters) {
  GurobiParametersProto merged_parameters;

  {
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_INT_PAR_LOGTOCONSOLE);
    parameter->set_value(solve_parameters.enable_output() ? "1" : "0");
  }

  if (solve_parameters.has_time_limit()) {
    const double time_limit = absl::ToDoubleSeconds(
        util_time::DecodeGoogleApiProto(solve_parameters.time_limit()).value());
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_DBL_PAR_TIMELIMIT);
    parameter->set_value(absl::StrCat(time_limit));
  }

  if (solve_parameters.has_node_limit()) {
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_DBL_PAR_NODELIMIT);
    parameter->set_value(absl::StrCat(solve_parameters.node_limit()));
  }

  if (solve_parameters.has_threads()) {
    const int threads = solve_parameters.threads();
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_INT_PAR_THREADS);
    parameter->set_value(absl::StrCat(threads));
  }

  if (solve_parameters.has_absolute_gap_tolerance()) {
    const double absolute_gap_tolerance =
        solve_parameters.absolute_gap_tolerance();
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_DBL_PAR_MIPGAPABS);
    parameter->set_value(absl::StrCat(absolute_gap_tolerance));
  }

  if (solve_parameters.has_relative_gap_tolerance()) {
    const double relative_gap_tolerance =
        solve_parameters.relative_gap_tolerance();
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_DBL_PAR_MIPGAP);
    parameter->set_value(absl::StrCat(relative_gap_tolerance));
  }

  if (solve_parameters.has_cutoff_limit()) {
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_DBL_PAR_CUTOFF);
    parameter->set_value(absl::StrCat(solve_parameters.cutoff_limit()));
  }

  if (solve_parameters.has_objective_limit()) {
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_DBL_PAR_BESTOBJSTOP);
    parameter->set_value(absl::StrCat(solve_parameters.objective_limit()));
  }

  if (solve_parameters.has_best_bound_limit()) {
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_DBL_PAR_BESTBDSTOP);
    parameter->set_value(absl::StrCat(solve_parameters.best_bound_limit()));
  }

  if (solve_parameters.has_solution_limit()) {
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_INT_PAR_SOLUTIONLIMIT);
    parameter->set_value(absl::StrCat(solve_parameters.solution_limit()));
  }

  if (solve_parameters.has_random_seed()) {
    const int random_seed =
        std::min(GRB_MAXINT, std::max(solve_parameters.random_seed(), 0));
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_INT_PAR_SEED);
    parameter->set_value(absl::StrCat(random_seed));
  }

  if (solve_parameters.lp_algorithm() != LP_ALGORITHM_UNSPECIFIED) {
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_INT_PAR_METHOD);
    switch (solve_parameters.lp_algorithm()) {
      case LP_ALGORITHM_PRIMAL_SIMPLEX:
        parameter->set_value(absl::StrCat(GRB_METHOD_PRIMAL));
        break;
      case LP_ALGORITHM_DUAL_SIMPLEX:
        parameter->set_value(absl::StrCat(GRB_METHOD_DUAL));
        break;
      case LP_ALGORITHM_BARRIER:
        parameter->set_value(absl::StrCat(GRB_METHOD_BARRIER));
        break;
      default:
        LOG(FATAL) << "LPAlgorithm: "
                   << ProtoEnumToString(solve_parameters.lp_algorithm())
                   << " unknown, error setting Gurobi parameters";
    }
  }

  if (solve_parameters.scaling() != EMPHASIS_UNSPECIFIED) {
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_INT_PAR_SCALEFLAG);
    switch (solve_parameters.scaling()) {
      case EMPHASIS_OFF:
        parameter->set_value(absl::StrCat(0));
        break;
      case EMPHASIS_LOW:
      case EMPHASIS_MEDIUM:
        parameter->set_value(absl::StrCat(1));
        break;
      case EMPHASIS_HIGH:
        parameter->set_value(absl::StrCat(2));
        break;
      case EMPHASIS_VERY_HIGH:
        parameter->set_value(absl::StrCat(3));
        break;
      default:
        LOG(FATAL) << "Scaling emphasis: "
                   << ProtoEnumToString(solve_parameters.scaling())
                   << " unknown, error setting Gurobi parameters";
    }
  }

  if (solve_parameters.cuts() != EMPHASIS_UNSPECIFIED) {
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_INT_PAR_CUTS);
    switch (solve_parameters.cuts()) {
      case EMPHASIS_OFF:
        parameter->set_value(absl::StrCat(0));
        break;
      case EMPHASIS_LOW:
      case EMPHASIS_MEDIUM:
        parameter->set_value(absl::StrCat(1));
        break;
      case EMPHASIS_HIGH:
        parameter->set_value(absl::StrCat(2));
        break;
      case EMPHASIS_VERY_HIGH:
        parameter->set_value(absl::StrCat(3));
        break;
      default:
        LOG(FATAL) << "Cuts emphasis: "
                   << ProtoEnumToString(solve_parameters.cuts())
                   << " unknown, error setting Gurobi parameters";
    }
  }

  if (solve_parameters.heuristics() != EMPHASIS_UNSPECIFIED) {
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_DBL_PAR_HEURISTICS);
    switch (solve_parameters.heuristics()) {
      case EMPHASIS_OFF:
        parameter->set_value(absl::StrCat(0.0));
        break;
      case EMPHASIS_LOW:
        parameter->set_value(absl::StrCat(0.025));
        break;
      case EMPHASIS_MEDIUM:
        // As of Gurobi 9.1 this is the default value.
        // https://www.gurobi.com/documentation/9.1/refman/heuristics.html
        parameter->set_value(absl::StrCat(0.05));
        break;
      case EMPHASIS_HIGH:
        parameter->set_value(absl::StrCat(0.1));
        break;
      case EMPHASIS_VERY_HIGH:
        parameter->set_value(absl::StrCat(0.2));
        break;
      default:
        LOG(FATAL) << "Heuristics emphasis: "
                   << ProtoEnumToString(solve_parameters.heuristics())
                   << " unknown, error setting Gurobi parameters";
    }
  }

  if (solve_parameters.presolve() != EMPHASIS_UNSPECIFIED) {
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_INT_PAR_PRESOLVE);
    switch (solve_parameters.presolve()) {
      case EMPHASIS_OFF:
        parameter->set_value(absl::StrCat(0));
        break;
      case EMPHASIS_LOW:
      case EMPHASIS_MEDIUM:
        parameter->set_value(absl::StrCat(1));
        break;
      case EMPHASIS_HIGH:
      case EMPHASIS_VERY_HIGH:
        parameter->set_value(absl::StrCat(2));
        break;
      default:
        LOG(FATAL) << "Presolve emphasis: "
                   << ProtoEnumToString(solve_parameters.presolve())
                   << " unknown, error setting Gurobi parameters";
    }
  }

  if (solve_parameters.has_iteration_limit()) {
    GurobiParametersProto::Parameter* const iterationlimit =
        merged_parameters.add_parameters();
    iterationlimit->set_name(GRB_DBL_PAR_ITERATIONLIMIT);
    iterationlimit->set_value(absl::StrCat(solve_parameters.iteration_limit()));
    GurobiParametersProto::Parameter* const bariterlimit =
        merged_parameters.add_parameters();
    bariterlimit->set_name(GRB_INT_PAR_BARITERLIMIT);
    double val = std::min<double>(std::numeric_limits<int32_t>::max(),
                                  solve_parameters.iteration_limit());
    bariterlimit->set_value(absl::StrCat(val));
  }

  for (const GurobiParametersProto::Parameter& parameter :
       solve_parameters.gurobi().parameters()) {
    *merged_parameters.add_parameters() = parameter;
  }

  return merged_parameters;
}

absl::StatusOr<int64_t> SafeInt64FromDouble(const double d) {
  const int64_t result = static_cast<int64_t>(d);
  if (static_cast<double>(result) != d) {
    return absl::InternalError(
        absl::StrCat("Expected double ", d, " to contain an int64_t."));
  }
  return result;
}

const absl::flat_hash_set<CallbackEventProto>& SupportedMIPEvents() {
  static const auto* const kEvents =
      new absl::flat_hash_set<CallbackEventProto>({
          CALLBACK_EVENT_PRESOLVE, CALLBACK_EVENT_SIMPLEX, CALLBACK_EVENT_MIP,
          CALLBACK_EVENT_MIP_SOLUTION, CALLBACK_EVENT_MIP_NODE,
          // CALLBACK_EVENT_BARRIER is not supported when solving MIPs; it turns
          // out that Gurobi uses a barrier algorithm to solve the root node
          // relaxation (from the traces) but does not call the associated
          // callback.
      });
  return *kEvents;
}

const absl::flat_hash_set<CallbackEventProto>& SupportedLPEvents() {
  static const auto* const kEvents =
      new absl::flat_hash_set<CallbackEventProto>({
          CALLBACK_EVENT_PRESOLVE,
          CALLBACK_EVENT_SIMPLEX,
          CALLBACK_EVENT_BARRIER,
      });
  return *kEvents;
}

// Gurobi names (model, variables and constraints) must be no longer than 255
// characters; or Gurobi fails with an error.
constexpr std::size_t kMaxNameSize = 255;

// Returns a string of at most kMaxNameSize max size.
std::string TruncateName(const std::string_view original_name) {
  return std::string(
      original_name.substr(0, std::min(kMaxNameSize, original_name.size())));
}

// Truncate the names of variables and constraints.
std::vector<std::string> TruncateNames(
    const google::protobuf::RepeatedPtrField<std::string>& original_names) {
  std::vector<std::string> result;
  result.reserve(original_names.size());
  for (const std::string& original_name : original_names) {
    result.push_back(TruncateName(original_name));
  }
  return result;
}

}  // namespace

GurobiSolver::GurobiSolver(std::unique_ptr<Gurobi> g_gurobi)
    : gurobi_(std::move(g_gurobi)) {}

int GurobiSolver::num_gurobi_constraints() const {
  return linear_constraints_map_.size();
}

absl::StatusOr<TerminationProto> GurobiSolver::ConvertTerminationReason(
    const int gurobi_status, const SolutionClaims solution_claims) {
  switch (gurobi_status) {
    case GRB_OPTIMAL:
      return TerminateForReason(TERMINATION_REASON_OPTIMAL);
    case GRB_INFEASIBLE:
      return TerminateForReason(TERMINATION_REASON_INFEASIBLE);
    case GRB_UNBOUNDED:
      if (solution_claims.primal_feasible_solution_exists) {
        return TerminateForReason(TERMINATION_REASON_UNBOUNDED);
      }
      return TerminateForReason(TERMINATION_REASON_INFEASIBLE_OR_UNBOUNDED,
                                "Gurobi status GRB_UNBOUNDED");
    case GRB_INF_OR_UNBD:
      return TerminateForReason(TERMINATION_REASON_INFEASIBLE_OR_UNBOUNDED,
                                "Gurobi status GRB_INF_OR_UNBD");
    case GRB_CUTOFF:
      return TerminateForLimit(LIMIT_CUTOFF,
                               /*feasible=*/false, "Gurobi status GRB_CUTOFF");
    case GRB_ITERATION_LIMIT:
      return TerminateForLimit(
          LIMIT_ITERATION,
          /*feasible=*/solution_claims.primal_feasible_solution_exists);
    case GRB_NODE_LIMIT:
      return TerminateForLimit(
          LIMIT_NODE,
          /*feasible=*/solution_claims.primal_feasible_solution_exists);
    case GRB_TIME_LIMIT:
      return TerminateForLimit(
          LIMIT_TIME,
          /*feasible=*/solution_claims.primal_feasible_solution_exists);
    case GRB_SOLUTION_LIMIT:
      return TerminateForLimit(
          LIMIT_SOLUTION,
          /*feasible=*/solution_claims.primal_feasible_solution_exists);
    case GRB_INTERRUPTED:
      return TerminateForLimit(
          LIMIT_INTERRUPTED,
          /*feasible=*/solution_claims.primal_feasible_solution_exists);
    case GRB_NUMERIC:
      return TerminateForReason(TERMINATION_REASON_NUMERICAL_ERROR);
    case GRB_SUBOPTIMAL:
      return TerminateForReason(TERMINATION_REASON_IMPRECISE);
    case GRB_USER_OBJ_LIMIT:
      // TODO(b/214567536): maybe we should override
      // solution_claims.primal_feasible_solution_exists to true or false
      // depending on whether objective_limit and best_bound_limit triggered
      // this. Not sure if it's possible to detect this though.
      return TerminateForLimit(
          LIMIT_OBJECTIVE,
          /*feasible=*/solution_claims.primal_feasible_solution_exists);
    case GRB_LOADED:
      return absl::InternalError(
          "Error creating termination reason, unexpected gurobi status code "
          "GRB_LOADED.");
    case GRB_INPROGRESS:
      return absl::InternalError(
          "Error creating termination reason, unexpected gurobi status code "
          "GRB_INPROGRESS.");
    default:
      return absl::InternalError(absl::StrCat(
          "Missing Gurobi optimization status code case: ", gurobi_status));
  }
}

absl::StatusOr<bool> GurobiSolver::IsMaximize() const {
  ASSIGN_OR_RETURN(const int obj_sense,
                   gurobi_->GetIntAttr(GRB_INT_ATTR_MODELSENSE));
  return obj_sense == GRB_MAXIMIZE;
}

absl::StatusOr<bool> GurobiSolver::IsLP() const {
  ASSIGN_OR_RETURN(const int is_mip, gurobi_->GetIntAttr(GRB_INT_ATTR_IS_MIP));
  ASSIGN_OR_RETURN(const int is_qp, gurobi_->GetIntAttr(GRB_INT_ATTR_IS_QP));
  ASSIGN_OR_RETURN(const int is_qcp, gurobi_->GetIntAttr(GRB_INT_ATTR_IS_QCP));
  return !static_cast<bool>(is_mip) && !static_cast<bool>(is_qp) &&
         !static_cast<bool>(is_qcp);
}

// TODO(b/204595455): Revisit logic when nonconvex QP support is decided upon
absl::StatusOr<bool> GurobiSolver::IsQP() const {
  ASSIGN_OR_RETURN(const int is_mip, gurobi_->GetIntAttr(GRB_INT_ATTR_IS_MIP));
  ASSIGN_OR_RETURN(const int is_qp, gurobi_->GetIntAttr(GRB_INT_ATTR_IS_QP));
  ASSIGN_OR_RETURN(const int is_qcp, gurobi_->GetIntAttr(GRB_INT_ATTR_IS_QCP));
  return !static_cast<bool>(is_mip) && static_cast<bool>(is_qp) &&
         !static_cast<bool>(is_qcp);
}

// TODO(user): switch the use of this function to something closer to
// GetGurobiDualRay()
template <typename T>
void GurobiSolver::GurobiVectorToSparseDoubleVector(
    const absl::Span<const double> gurobi_values, const T& map,
    SparseDoubleVectorProto& result,
    const SparseVectorFilterProto& filter) const {
  SparseVectorFilterPredicate predicate(filter);
  for (auto [id, gurobi_data] : map) {
    const double value = gurobi_values[get_model_index(gurobi_data)];
    if (predicate.AcceptsAndUpdate(id, value)) {
      result.add_ids(id);
      result.add_values(value);
    }
  }
}

absl::Status GurobiSolver::SetGurobiBasis(const BasisProto& basis) {
  std::vector<int> gurobi_variable_basis_status(num_gurobi_variables_);
  for (const auto [id, value] : MakeView(basis.variable_status())) {
    gurobi_variable_basis_status[variables_map_.at(id)] =
        GrbVariableStatus(static_cast<BasisStatusProto>(value));
  }

  std::vector<int> gurobi_constraint_basis_status;
  gurobi_constraint_basis_status.reserve(num_gurobi_constraints());
  for (const auto [id, value] : MakeView(basis.constraint_status())) {
    const ConstraintData& constraint_data = linear_constraints_map_.at(id);
    // Non-ranged constraints
    if (constraint_data.slack_index == kUnspecifiedIndex) {
      if (value == BASIS_STATUS_BASIC) {
        gurobi_constraint_basis_status.push_back(kGrbBasicConstraint);
      } else {
        gurobi_constraint_basis_status.push_back(kGrbNonBasicConstraint);
      }
      // Ranged constraints
    } else if (value == BASIS_STATUS_BASIC) {
      // Either constraint or MathOpt slack is basic, but not both (because
      // columns for MathOpt slack and internal Gurobi slack are linearly
      // dependent). We choose the MathOpt slack to be basic.
      gurobi_variable_basis_status[constraint_data.slack_index] = GRB_BASIC;
      gurobi_constraint_basis_status.push_back(kGrbNonBasicConstraint);
    } else {
      gurobi_variable_basis_status[constraint_data.slack_index] =
          GrbVariableStatus(static_cast<BasisStatusProto>(value));
      gurobi_constraint_basis_status.push_back(kGrbNonBasicConstraint);
    }
  }
  RETURN_IF_ERROR(gurobi_->SetIntAttrArray(GRB_INT_ATTR_VBASIS,
                                           gurobi_variable_basis_status));
  RETURN_IF_ERROR(gurobi_->SetIntAttrArray(GRB_INT_ATTR_CBASIS,
                                           gurobi_constraint_basis_status));
  return absl::OkStatus();
}

absl::StatusOr<BasisProto> GurobiSolver::GetGurobiBasis() {
  BasisProto basis;
  ASSIGN_OR_RETURN(
      const std::vector<int> gurobi_variable_basis_status,
      gurobi_->GetIntAttrArray(GRB_INT_ATTR_VBASIS, num_gurobi_variables_));

  for (auto [variable_id, gurobi_variable_index] : variables_map_) {
    basis.mutable_variable_status()->add_ids(variable_id);
    const BasisStatusProto variable_status = ConvertVariableStatus(
        gurobi_variable_basis_status[gurobi_variable_index]);
    if (variable_status == BASIS_STATUS_UNSPECIFIED) {
      return absl::InternalError(
          absl::StrCat("Invalid Gurobi variable basis status: ",
                       gurobi_variable_basis_status[gurobi_variable_index]));
    }
    basis.mutable_variable_status()->add_values(variable_status);
  }

  ASSIGN_OR_RETURN(
      const std::vector<int> gurobi_constraint_basis_status,
      gurobi_->GetIntAttrArray(GRB_INT_ATTR_CBASIS, num_gurobi_constraints()));
  for (auto [constraint_id, gurobi_data] : linear_constraints_map_) {
    basis.mutable_constraint_status()->add_ids(constraint_id);
    const int gurobi_constraint_status =
        gurobi_constraint_basis_status[gurobi_data.constraint_index];
    if ((gurobi_constraint_status != kGrbBasicConstraint) &&
        (gurobi_constraint_status != kGrbNonBasicConstraint)) {
      return absl::InternalError(
          absl::StrCat("Invalid Gurobi constraint basis status: ",
                       gurobi_constraint_status));
    }
    // linear_terms <= upper_bound
    if (gurobi_data.lower_bound <= -GRB_INFINITY &&
        gurobi_data.upper_bound < GRB_INFINITY) {
      if (gurobi_constraint_status == kGrbBasicConstraint) {
        basis.mutable_constraint_status()->add_values(BASIS_STATUS_BASIC);
      } else {
        basis.mutable_constraint_status()->add_values(
            BASIS_STATUS_AT_UPPER_BOUND);
      }
      // linear_terms >= lower_bound
    } else if (gurobi_data.lower_bound > -GRB_INFINITY &&
               gurobi_data.upper_bound >= GRB_INFINITY) {
      if (gurobi_constraint_status == kGrbBasicConstraint) {
        basis.mutable_constraint_status()->add_values(BASIS_STATUS_BASIC);
      } else {
        basis.mutable_constraint_status()->add_values(
            BASIS_STATUS_AT_LOWER_BOUND);
      }
      // linear_terms == xxxxx_bound
    } else if (gurobi_data.lower_bound == gurobi_data.upper_bound) {
      if (gurobi_constraint_status == kGrbBasicConstraint) {
        basis.mutable_constraint_status()->add_values(BASIS_STATUS_BASIC);
      } else {
        // TODO(user): consider refining this to
        // AT_LOWER_BOUND/AT_UPPER_BOUND using the sign of the dual variable.
        basis.mutable_constraint_status()->add_values(BASIS_STATUS_FIXED_VALUE);
      }
      //   linear_term - slack == 0 (ranged constraint)
    } else {
      const BasisStatusProto slack_status = ConvertVariableStatus(
          gurobi_variable_basis_status[gurobi_data.slack_index]);
      if (slack_status == BASIS_STATUS_UNSPECIFIED) {
        return absl::InternalError(absl::StrCat(
            "Invalid Gurobi slack variable basis status: ", slack_status));
      }
      if ((gurobi_constraint_status == kGrbBasicConstraint) ||
          (slack_status == BASIS_STATUS_BASIC)) {
        basis.mutable_constraint_status()->add_values(BASIS_STATUS_BASIC);
      } else {
        basis.mutable_constraint_status()->add_values(slack_status);
      }
    }
  }
  return basis;
}

// See go/mathopt-dev-transformations#gurobi-inf for details of this
// transformation, comments inside the function refer to the notation there.
absl::StatusOr<DualRayProto> GurobiSolver::GetGurobiDualRay(
    const SparseVectorFilterProto& linear_constraints_filter,
    const SparseVectorFilterProto& variables_filter, const bool is_maximize) {
  // farkas_dual = lambda
  ASSIGN_OR_RETURN(const std::vector<double> farkas_dual,
                   gurobi_->GetDoubleAttrArray(GRB_DBL_ATTR_FARKASDUAL,
                                               num_gurobi_constraints()));

  DualRayProto dual_ray;

  // Compute y = -lambda
  {
    SparseVectorFilterPredicate predicate(linear_constraints_filter);
    for (auto [constraint_id, gurobi_data] : linear_constraints_map_) {
      // constraint_dual_value = y[constraint_id]
      const double value = -farkas_dual[gurobi_data.constraint_index];
      if (predicate.AcceptsAndUpdate(constraint_id, value)) {
        dual_ray.mutable_dual_values()->add_ids(constraint_id);
        if (is_maximize) {
          dual_ray.mutable_dual_values()->add_values(-value);
        } else {
          dual_ray.mutable_dual_values()->add_values(value);
        }
      }
    }
  }

  // Compute r = \bar{a} = A^T lambda
  {
    SparseVectorFilterPredicate predicate(variables_filter);
    for (auto [var_id, gurobi_variable_index] : variables_map_) {
      // reduced_cost_value = r[gurobi_variable_index]
      //                    = \bar{a}[gurobi_variable_index]
      double reduced_cost_value = 0.0;
      ASSIGN_OR_RETURN(Gurobi::SparseMat column,
                       gurobi_->GetVars(gurobi_variable_index, 1));
      for (int i = 0; i < column.inds.size(); ++i) {
        reduced_cost_value += farkas_dual[column.inds[i]] * column.vals[i];
      }
      if (predicate.AcceptsAndUpdate(var_id, reduced_cost_value)) {
        dual_ray.mutable_reduced_costs()->add_ids(var_id);
        if (is_maximize) {
          dual_ray.mutable_reduced_costs()->add_values(-reduced_cost_value);
        } else {
          dual_ray.mutable_reduced_costs()->add_values(reduced_cost_value);
        }
      }
    }
  }
  return dual_ray;
}

absl::StatusOr<ProblemStatusProto> GurobiSolver::GetProblemStatus(
    const int grb_termination, const SolutionClaims solution_claims) {
  ProblemStatusProto problem_status;

  // Set default statuses
  problem_status.set_primal_status(FEASIBILITY_STATUS_UNDETERMINED);
  problem_status.set_dual_status(FEASIBILITY_STATUS_UNDETERMINED);

  // Set feasibility statuses
  if (solution_claims.primal_feasible_solution_exists) {
    problem_status.set_primal_status(FEASIBILITY_STATUS_FEASIBLE);
  }
  if (solution_claims.dual_feasible_solution_exists) {
    problem_status.set_dual_status(FEASIBILITY_STATUS_FEASIBLE);
  }

  // Process infeasible conclusions from grb_termination.
  switch (grb_termination) {
    case GRB_INFEASIBLE:
      problem_status.set_primal_status(FEASIBILITY_STATUS_INFEASIBLE);
      if (solution_claims.primal_feasible_solution_exists) {
        return absl::InternalError(
            "GRB_INT_ATTR_STATUS == GRB_INFEASIBLE, but a primal feasible "
            "solution was returned.");
      }
      break;
    case GRB_UNBOUNDED:
      // GRB_UNBOUNDED does necessarily imply the primal is feasible
      // https://www.gurobi.com/documentation/9.1/refman/optimization_status_codes.html
      problem_status.set_dual_status(FEASIBILITY_STATUS_INFEASIBLE);
      if (solution_claims.dual_feasible_solution_exists) {
        return absl::InternalError(
            "GRB_INT_ATTR_STATUS == GRB_UNBOUNDED, but a dual feasible "
            "solution was returned or exists.");
      }
      break;
    case GRB_INF_OR_UNBD:
      problem_status.set_primal_or_dual_infeasible(true);
      if (solution_claims.primal_feasible_solution_exists) {
        return absl::InternalError(
            "GRB_INT_ATTR_STATUS == GRB_INF_OR_UNBD, but a primal feasible "
            "solution was returned.");
      }
      if (solution_claims.dual_feasible_solution_exists) {
        return absl::InternalError(
            "GRB_INT_ATTR_STATUS == GRB_INF_OR_UNBD, but a dual feasible "
            "solution was returned or exists.");
      }
      break;
  }
  return problem_status;
}

absl::StatusOr<SolveResultProto> GurobiSolver::ExtractSolveResultProto(
    const absl::Time start, const ModelSolveParametersProto& model_parameters) {
  SolveResultProto result;

  ASSIGN_OR_RETURN((auto [solutions, solution_claims]),
                   GetSolutions(model_parameters));

  // TODO(b/195295177): Add tests for rays in unbounded MIPs
  RETURN_IF_ERROR(FillRays(model_parameters, solution_claims, result));

  for (auto& solution : solutions) {
    *result.add_solutions() = std::move(solution);
  }

  ASSIGN_OR_RETURN(*result.mutable_solve_stats(),
                   GetSolveStats(start, solution_claims));

  ASSIGN_OR_RETURN(const int grb_termination,
                   gurobi_->GetIntAttr(GRB_INT_ATTR_STATUS));
  ASSIGN_OR_RETURN(*result.mutable_termination(),
                   ConvertTerminationReason(grb_termination, solution_claims));
  return std::move(result);
}

absl::StatusOr<GurobiSolver::SolutionsAndClaims> GurobiSolver::GetSolutions(
    const ModelSolveParametersProto& model_parameters) {
  ASSIGN_OR_RETURN(const bool is_lp, IsLP());
  ASSIGN_OR_RETURN(const bool is_qp, IsQP());

  if (is_lp) {
    return GetLpSolution(model_parameters);
  } else if (is_qp) {
    return GetQpSolution(model_parameters);
  } else {
    return GetMipSolutions(model_parameters);
  }
}

absl::StatusOr<SolveStatsProto> GurobiSolver::GetSolveStats(
    const absl::Time start, const SolutionClaims solution_claims) {
  SolveStatsProto solve_stats;

  CHECK_OK(util_time::EncodeGoogleApiProto(absl::Now() - start,
                                           solve_stats.mutable_solve_time()));

  ASSIGN_OR_RETURN(const double best_primal_bound,
                   GetBestPrimalBound(
                       /*has_primal_feasible_solution=*/solution_claims
                           .primal_feasible_solution_exists));
  solve_stats.set_best_primal_bound(best_primal_bound);

  ASSIGN_OR_RETURN(double best_dual_bound, GetBestDualBound());
  solve_stats.set_best_dual_bound(best_dual_bound);

  ASSIGN_OR_RETURN(const int grb_termination,
                   gurobi_->GetIntAttr(GRB_INT_ATTR_STATUS));
  ASSIGN_OR_RETURN((*solve_stats.mutable_problem_status()),
                   GetProblemStatus(grb_termination, solution_claims));

  if (gurobi_->IsAttrAvailable(GRB_DBL_ATTR_ITERCOUNT)) {
    ASSIGN_OR_RETURN(const double simplex_iters_double,
                     gurobi_->GetDoubleAttr(GRB_DBL_ATTR_ITERCOUNT));
    ASSIGN_OR_RETURN(const int64_t simplex_iters,
                     SafeInt64FromDouble(simplex_iters_double));
    solve_stats.set_simplex_iterations(simplex_iters);
  }

  if (gurobi_->IsAttrAvailable(GRB_INT_ATTR_BARITERCOUNT)) {
    ASSIGN_OR_RETURN(const int barrier_iters,
                     gurobi_->GetIntAttr(GRB_INT_ATTR_BARITERCOUNT));
    solve_stats.set_barrier_iterations(barrier_iters);
  }

  if (gurobi_->IsAttrAvailable(GRB_DBL_ATTR_NODECOUNT)) {
    ASSIGN_OR_RETURN(const double nodes_double,
                     gurobi_->GetDoubleAttr(GRB_DBL_ATTR_NODECOUNT));
    ASSIGN_OR_RETURN(const int64_t nodes, SafeInt64FromDouble(nodes_double));
    solve_stats.set_node_count(nodes);
  }
  return solve_stats;
}

absl::StatusOr<GurobiSolver::SolutionsAndClaims> GurobiSolver::GetMipSolutions(
    const ModelSolveParametersProto& model_parameters) {
  int num_solutions = 0;
  if (gurobi_->IsAttrAvailable(GRB_INT_ATTR_SOLCOUNT)) {
    ASSIGN_OR_RETURN(num_solutions, gurobi_->GetIntAttr(GRB_INT_ATTR_SOLCOUNT));
  }
  std::vector<SolutionProto> solutions;
  solutions.reserve(num_solutions);
  for (int i = 0; i < num_solutions; ++i) {
    RETURN_IF_ERROR(gurobi_->SetIntParam(GRB_INT_PAR_SOLUTIONNUMBER, i));

    PrimalSolutionProto primal_solution;
    ASSIGN_OR_RETURN(const double sol_val,
                     gurobi_->GetDoubleAttr(GRB_DBL_ATTR_POOLOBJVAL));
    primal_solution.set_objective_value(sol_val);
    primal_solution.set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
    ASSIGN_OR_RETURN(
        const std::vector<double> grb_var_values,
        gurobi_->GetDoubleAttrArray(GRB_DBL_ATTR_XN, num_gurobi_variables_));
    GurobiVectorToSparseDoubleVector(grb_var_values, variables_map_,
                                     *primal_solution.mutable_variable_values(),
                                     model_parameters.variable_values_filter());
    *solutions.emplace_back(SolutionProto()).mutable_primal_solution() =
        std::move(primal_solution);
  }

  // Set solution claims
  ASSIGN_OR_RETURN(const double best_dual_bound, GetBestDualBound());
  // Note: here the existence of a dual solution refers to a dual solution to
  // some convex relaxation of the MIP. This convex relaxation can likely be
  // interpreted as an LP between the LP relaxation of the MIP and the convex
  // hull of feasible solutions of the MIP. However, here we only use the fact
  // that best_dual_bound being finite implies the existence of the trivial
  // convex relaxation given by (assuming a minimization problem with objective
  // function c^T x): min{c^T x : c^T x >= best_dual_bound}.
  const SolutionClaims solution_claims = {
      .primal_feasible_solution_exists = num_solutions > 0,
      .dual_feasible_solution_exists = std::isfinite(best_dual_bound)};

  // Check consistency of solutions, bounds and statuses.
  ASSIGN_OR_RETURN(const int grb_termination,
                   gurobi_->GetIntAttr(GRB_INT_ATTR_STATUS));
  if (grb_termination == GRB_OPTIMAL && num_solutions == 0) {
    return absl::InternalError(
        "GRB_INT_ATTR_STATUS == GRB_OPTIMAL, but solution pool is empty.");
  }
  if (grb_termination == GRB_OPTIMAL && !std::isfinite(best_dual_bound)) {
    return absl::InternalError(
        "GRB_INT_ATTR_STATUS == GRB_OPTIMAL, but GRB_DBL_ATTR_OBJBOUND is "
        "unavailable or infinite.");
  }

  return SolutionsAndClaims{.solutions = std::move(solutions),
                            .solution_claims = solution_claims};
}

absl::StatusOr<GurobiSolver::SolutionAndClaim<PrimalSolutionProto>>
GurobiSolver::GetConvexPrimalSolutionIfAvailable(
    const ModelSolveParametersProto& model_parameters) {
  if (!gurobi_->IsAttrAvailable(GRB_DBL_ATTR_X)) {
    return SolutionAndClaim<PrimalSolutionProto>{
        .solution = std::nullopt, .feasible_solution_exists = false};
  }
  ASSIGN_OR_RETURN(const int grb_termination,
                   gurobi_->GetIntAttr(GRB_INT_ATTR_STATUS));

  // Get primal solutions if available.
  ASSIGN_OR_RETURN(
      const std::vector<double> grb_var_values,
      gurobi_->GetDoubleAttrArray(GRB_DBL_ATTR_X, num_gurobi_variables_));

  PrimalSolutionProto primal_solution;
  // As noted in go/gurobi-objval-bug the objective value may be missing for
  // primal feasible solutions for unbounded problems.
  // TODO(b/195295177): for GRB_ITERATION_LIMIT an objective value of 0.0 is
  // returned which breaks LpIncompleteSolveTest.PrimalSimplexAlgorithm. Explore
  // more and make simple example to file a bug.
  if (gurobi_->IsAttrAvailable(GRB_DBL_ATTR_OBJVAL) &&
      grb_termination != GRB_ITERATION_LIMIT) {
    ASSIGN_OR_RETURN(const double sol_val,
                     gurobi_->GetDoubleAttr(GRB_DBL_ATTR_OBJVAL));
    primal_solution.set_objective_value(sol_val);
  } else {
    double objective_value = 0.0;
    ASSIGN_OR_RETURN(
        const std::vector<double> linear_obj_coefs,
        gurobi_->GetDoubleAttrArray(GRB_DBL_ATTR_OBJ, num_gurobi_variables_));
    for (int i = 0; i < num_gurobi_variables_; ++i) {
      objective_value += linear_obj_coefs[i] * grb_var_values[i];
    }
    primal_solution.set_objective_value(objective_value);
  }

  primal_solution.set_feasibility_status(SOLUTION_STATUS_UNDETERMINED);
  if (grb_termination == GRB_OPTIMAL) {
    primal_solution.set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
  } else if (grb_termination == GRB_INFEASIBLE) {
    primal_solution.set_feasibility_status(SOLUTION_STATUS_INFEASIBLE);
  } else if (PrimalSolutionQualityAvailable()) {
    ASSIGN_OR_RETURN(const double solution_quality, GetPrimalSolutionQuality());
    ASSIGN_OR_RETURN(const double tolerance,
                     gurobi_->GetDoubleParam(GRB_DBL_PAR_FEASIBILITYTOL));
    if (solution_quality <= tolerance) {
      primal_solution.set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
    } else {
      primal_solution.set_feasibility_status(SOLUTION_STATUS_INFEASIBLE);
    }
  }

  GurobiVectorToSparseDoubleVector(grb_var_values, variables_map_,
                                   *primal_solution.mutable_variable_values(),
                                   model_parameters.variable_values_filter());
  const bool primal_feasible_solution_exists =
      (primal_solution.feasibility_status() == SOLUTION_STATUS_FEASIBLE);
  return SolutionAndClaim<PrimalSolutionProto>{
      .solution = std::move(primal_solution),
      .feasible_solution_exists = primal_feasible_solution_exists};
}

bool GurobiSolver::PrimalSolutionQualityAvailable() const {
  return gurobi_->IsAttrAvailable(GRB_DBL_ATTR_CONSTR_RESIDUAL) &&
         gurobi_->IsAttrAvailable(GRB_DBL_ATTR_CONSTR_VIO) &&
         gurobi_->IsAttrAvailable(GRB_DBL_ATTR_BOUND_VIO) &&
         gurobi_->IsAttrAvailable(GRB_DBL_ATTR_CONSTR_SRESIDUAL) &&
         gurobi_->IsAttrAvailable(GRB_DBL_ATTR_CONSTR_SVIO) &&
         gurobi_->IsAttrAvailable(GRB_DBL_ATTR_BOUND_SVIO);
}

absl::StatusOr<double> GurobiSolver::GetPrimalSolutionQuality() const {
  ASSIGN_OR_RETURN(const double constraint_residual,
                   gurobi_->GetDoubleAttr(GRB_DBL_ATTR_CONSTR_RESIDUAL));
  ASSIGN_OR_RETURN(const double constraint_violation,
                   gurobi_->GetDoubleAttr(GRB_DBL_ATTR_CONSTR_VIO));
  ASSIGN_OR_RETURN(const double bound_violation,
                   gurobi_->GetDoubleAttr(GRB_DBL_ATTR_BOUND_VIO));
  ASSIGN_OR_RETURN(const double constraint_scaled_residual,
                   gurobi_->GetDoubleAttr(GRB_DBL_ATTR_CONSTR_SRESIDUAL));
  ASSIGN_OR_RETURN(const double constraint_scaled_violation,
                   gurobi_->GetDoubleAttr(GRB_DBL_ATTR_CONSTR_SVIO));
  ASSIGN_OR_RETURN(const double bound_scaled_violation,
                   gurobi_->GetDoubleAttr(GRB_DBL_ATTR_BOUND_SVIO));
  return std::max({constraint_residual, constraint_violation, bound_violation,
                   constraint_scaled_residual, constraint_scaled_violation,
                   bound_scaled_violation});
}

absl::StatusOr<double> GurobiSolver::GetBestPrimalBound(
    const bool has_primal_feasible_solution) {
  ASSIGN_OR_RETURN(const bool is_maximize, IsMaximize());
  // We need has_primal_feasible_solution because, as noted in
  // go/gurobi-objval-bug, GRB_DBL_ATTR_OBJVAL may be available and finite for
  // primal infeasible solutions.
  if (has_primal_feasible_solution &&
      gurobi_->IsAttrAvailable(GRB_DBL_ATTR_OBJVAL)) {
    // TODO(b/195295177): Discuss if this should be removed. Unlike the dual
    // case below, it appears infesible models do not return GRB_DBL_ATTR_OBJVAL
    // equal to GRB_INFINITY (GRB_DBL_ATTR_OBJVAL is just unavailable). Hence,
    // this may not be needed and may not be consistent (e.g. we should explore
    // whether GRB_DBL_ATTR_OBJVAL = GRB_INFINITY may happen for a primal
    // feasible solution, in which the conversion of +/-GRB_INFINITY to +/-kInf
    // would not be consistent). Note that unlike the dual case removing this
    // does not break any test.
    ASSIGN_OR_RETURN(const double obj_val,
                     gurobi_->GetDoubleAttr(GRB_DBL_ATTR_OBJVAL));
    if (std::abs(obj_val) < GRB_INFINITY) {
      return obj_val;
    }
  }
  return is_maximize ? -kInf : kInf;
}

absl::StatusOr<double> GurobiSolver::GetBestDualBound() {
  if (gurobi_->IsAttrAvailable(GRB_DBL_ATTR_OBJBOUND)) {
    ASSIGN_OR_RETURN(const double obj_bound,
                     gurobi_->GetDoubleAttr(GRB_DBL_ATTR_OBJBOUND));
    // Note: Unbounded models return GRB_DBL_ATTR_OBJBOUND = GRB_INFINITY so
    // the conversion of +/-GRB_INFINITY to +/-kInf is needed and consistent.
    if (std::abs(obj_bound) < GRB_INFINITY) {
      return obj_bound;
    }
  }
  ASSIGN_OR_RETURN(const bool is_maximize, IsMaximize());
  return is_maximize ? kInf : -kInf;
}

absl::StatusOr<std::optional<BasisProto>> GurobiSolver::GetBasisIfAvailable() {
  if (gurobi_->IsAttrAvailable(GRB_INT_ATTR_VBASIS) &&
      gurobi_->IsAttrAvailable(GRB_INT_ATTR_CBASIS)) {
    ASSIGN_OR_RETURN(BasisProto basis, GetGurobiBasis());
    ASSIGN_OR_RETURN(const int grb_termination,
                     gurobi_->GetIntAttr(GRB_INT_ATTR_STATUS));
    basis.set_basic_dual_feasibility(SOLUTION_STATUS_UNDETERMINED);
    if (grb_termination == GRB_OPTIMAL) {
      basis.set_basic_dual_feasibility(SOLUTION_STATUS_FEASIBLE);
    } else if (grb_termination == GRB_UNBOUNDED) {
      basis.set_basic_dual_feasibility(SOLUTION_STATUS_INFEASIBLE);
    }
    // TODO(b/195295177): double check if the move is needed
    return std::move(basis);
  }
  return std::nullopt;
}

absl::StatusOr<GurobiSolver::SolutionsAndClaims> GurobiSolver::GetLpSolution(
    const ModelSolveParametersProto& model_parameters) {
  ASSIGN_OR_RETURN(auto primal_solution_and_claim,
                   GetConvexPrimalSolutionIfAvailable(model_parameters));
  ASSIGN_OR_RETURN(auto dual_solution_and_claim,
                   GetLpDualSolutionIfAvailable(model_parameters));
  ASSIGN_OR_RETURN(auto basis, GetBasisIfAvailable());
  const SolutionClaims solution_claims = {
      .primal_feasible_solution_exists =
          primal_solution_and_claim.feasible_solution_exists,
      .dual_feasible_solution_exists =
          dual_solution_and_claim.feasible_solution_exists};

  if (!primal_solution_and_claim.solution.has_value() &&
      !dual_solution_and_claim.solution.has_value() && !basis.has_value()) {
    return SolutionsAndClaims{.solution_claims = solution_claims};
  }
  SolutionsAndClaims solution_and_claims{.solution_claims = solution_claims};
  SolutionProto& solution =
      solution_and_claims.solutions.emplace_back(SolutionProto());
  if (primal_solution_and_claim.solution.has_value()) {
    *solution.mutable_primal_solution() =
        std::move(*primal_solution_and_claim.solution);
  }
  if (dual_solution_and_claim.solution.has_value()) {
    *solution.mutable_dual_solution() =
        std::move(*dual_solution_and_claim.solution);
  }
  if (basis.has_value()) {
    *solution.mutable_basis() = std::move(*basis);
  }
  return solution_and_claims;
}

absl::StatusOr<GurobiSolver::SolutionAndClaim<DualSolutionProto>>
GurobiSolver::GetLpDualSolutionIfAvailable(
    const ModelSolveParametersProto& model_parameters) {
  if (!gurobi_->IsAttrAvailable(GRB_DBL_ATTR_PI) ||
      !gurobi_->IsAttrAvailable(GRB_DBL_ATTR_RC)) {
    return SolutionAndClaim<DualSolutionProto>{
        .solution = std::nullopt, .feasible_solution_exists = false};
  }

  // Note that we can ignore the reduced costs of the slack variables for
  // ranged constraints because of
  // go/mathopt-dev-transformations#slack-var-range-constraint
  DualSolutionProto dual_solution;
  bool dual_feasible_solution_exists = false;
  ASSIGN_OR_RETURN(
      const std::vector<double> grb_constraint_duals,
      gurobi_->GetDoubleAttrArray(GRB_DBL_ATTR_PI, num_gurobi_constraints()));
  GurobiVectorToSparseDoubleVector(grb_constraint_duals,
                                   linear_constraints_map_,
                                   *dual_solution.mutable_dual_values(),
                                   model_parameters.dual_values_filter());

  ASSIGN_OR_RETURN(
      const std::vector<double> grb_reduced_cost_values,
      gurobi_->GetDoubleAttrArray(GRB_DBL_ATTR_RC, num_gurobi_variables_));
  GurobiVectorToSparseDoubleVector(grb_reduced_cost_values, variables_map_,
                                   *dual_solution.mutable_reduced_costs(),
                                   model_parameters.reduced_costs_filter());

  ASSIGN_OR_RETURN(const int grb_termination,
                   gurobi_->GetIntAttr(GRB_INT_ATTR_STATUS));
  if (grb_termination == GRB_OPTIMAL &&
      gurobi_->IsAttrAvailable(GRB_DBL_ATTR_OBJVAL)) {
    ASSIGN_OR_RETURN(const double obj_val,
                     gurobi_->GetDoubleAttr(GRB_DBL_ATTR_OBJVAL));
    dual_solution.set_objective_value(obj_val);
  }
  // TODO(b/195295177): explore using GRB_DBL_ATTR_OBJBOUND to set the dual
  // objective. As described in go/gurobi-objval-bug, this could provide the
  // dual objective in some cases.

  dual_solution.set_feasibility_status(SOLUTION_STATUS_UNDETERMINED);
  if (grb_termination == GRB_OPTIMAL) {
    dual_solution.set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
    dual_feasible_solution_exists = true;
  } else if (grb_termination == GRB_UNBOUNDED) {
    dual_solution.set_feasibility_status(SOLUTION_STATUS_INFEASIBLE);
  }
  // TODO(b/195295177): We could use gurobi's dual solution quality measures
  // for further upgrade the dual feasibility but it likely is only useful
  // for phase II of dual simplex because:
  //   * the quality measures seem to evaluate if the basis is dual feasible
  //     so for primal simplex we would not improve over checking
  //     GRB_OPTIMAL.
  //   * for phase I dual simplex we cannot rely on the quality measures
  //     because of go/gurobi-solution-quality-bug.
  // We could also use finiteness of GRB_DBL_ATTR_OBJBOUND to deduce dual
  // feasibility as described in go/gurobi-objval-bug.

  // Note: as shown in go/gurobi-objval-bug, GRB_DBL_ATTR_OBJBOUND can
  // sometimes provide the objective value of a sub-optimal dual feasible
  // solution. Here we only use it to possibly update
  // dual_feasible_solution_exists (Otherwise
  // StatusTest.PrimalInfeasibleAndDualFeasible for pure dual simplex would
  // fail because go/gurobi-solution-quality-bug prevents us from certifying
  // feasibility of the dual solution found in this case).
  ASSIGN_OR_RETURN(const double best_dual_bound, GetBestDualBound());
  if (dual_feasible_solution_exists || std::isfinite(best_dual_bound)) {
    dual_feasible_solution_exists = true;
  } else if (grb_termination == GRB_OPTIMAL) {
    return absl::InternalError(
        "GRB_INT_ATTR_STATUS == GRB_OPTIMAL, but GRB_DBL_ATTR_OBJBOUND is "
        "unavailable or infinite, and no dual feasible solution is returned");
  }
  return SolutionAndClaim<DualSolutionProto>{
      .solution = std::move(dual_solution),
      .feasible_solution_exists = dual_feasible_solution_exists};
}

absl::Status GurobiSolver::FillRays(
    const ModelSolveParametersProto& model_parameters,
    const SolutionClaims solution_claims, SolveResultProto& result) {
  ASSIGN_OR_RETURN(const bool is_maximize, IsMaximize());
  // GRB_DBL_ATTR_UNBDRAY is sometimes incorrectly available for problems
  // without variables. We also give priority to the conclusions obtained from
  // dual solutions or bounds.
  if (!solution_claims.dual_feasible_solution_exists &&
      num_gurobi_variables_ > 0 &&
      gurobi_->IsAttrAvailable(GRB_DBL_ATTR_UNBDRAY)) {
    ASSIGN_OR_RETURN(const std::vector<double> grb_ray_var_values,
                     gurobi_->GetDoubleAttrArray(GRB_DBL_ATTR_UNBDRAY,
                                                 num_gurobi_variables_));
    PrimalRayProto* const primal_ray = result.add_primal_rays();
    GurobiVectorToSparseDoubleVector(grb_ray_var_values, variables_map_,
                                     *primal_ray->mutable_variable_values(),
                                     model_parameters.variable_values_filter());
  }
  // GRB_DBL_ATTR_FARKASDUAL is sometimes incorrectly available for problems
  // without constraints. We also give priority to the conclusions obtained from
  // primal solutions.
  if (!solution_claims.primal_feasible_solution_exists &&
      num_gurobi_constraints() > 0 &&
      gurobi_->IsAttrAvailable(GRB_DBL_ATTR_FARKASDUAL)) {
    ASSIGN_OR_RETURN(
        DualRayProto dual_ray,
        GetGurobiDualRay(model_parameters.dual_values_filter(),
                         model_parameters.reduced_costs_filter(), is_maximize));
    result.mutable_dual_rays()->Add(std::move(dual_ray));
  }
  return absl::OkStatus();
}

absl::StatusOr<GurobiSolver::SolutionsAndClaims> GurobiSolver::GetQpSolution(
    const ModelSolveParametersProto& model_parameters) {
  ASSIGN_OR_RETURN((auto [primal_solution, found_primal_feasible_solution]),
                   GetConvexPrimalSolutionIfAvailable(model_parameters));
  // TODO(b/225189115): Expand QpDualsTest to check maximization problems and
  // other edge cases.
  ASSIGN_OR_RETURN((auto [dual_solution, found_dual_feasible_solution]),
                   GetLpDualSolutionIfAvailable(model_parameters));
  // Basis information is available when Gurobi uses QP simplex. As of v9.1 this
  // is not the default [1], so a user will need to explicitly set the Method
  // parameter in order for the following call to do anything interesting.
  //  [1] https://www.gurobi.com/documentation/9.1/refman/method.html
  ASSIGN_OR_RETURN(auto basis, GetBasisIfAvailable());

  const SolutionClaims solution_claims = {
      .primal_feasible_solution_exists = found_primal_feasible_solution,
      .dual_feasible_solution_exists = found_dual_feasible_solution};

  if (!primal_solution.has_value() && !basis.has_value()) {
    return GurobiSolver::SolutionsAndClaims{.solution_claims = solution_claims};
  }
  SolutionsAndClaims solution_and_claims{.solution_claims = solution_claims};
  SolutionProto& solution =
      solution_and_claims.solutions.emplace_back(SolutionProto());
  if (primal_solution.has_value()) {
    *solution.mutable_primal_solution() = *std::move(primal_solution);
  }
  if (dual_solution.has_value()) {
    *solution.mutable_dual_solution() = *std::move(dual_solution);
  }
  if (basis.has_value()) {
    *solution.mutable_basis() = *std::move(basis);
  }
  return solution_and_claims;
}

absl::Status GurobiSolver::SetParameters(
    const SolveParametersProto& parameters) {
  const GurobiParametersProto gurobi_parameters = MergeParameters(parameters);
  std::vector<std::string> parameter_errors;
  for (const GurobiParametersProto::Parameter& parameter :
       gurobi_parameters.parameters()) {
    absl::Status param_status =
        gurobi_->SetParam(parameter.name().c_str(), parameter.value());
    if (!param_status.ok()) {
      parameter_errors.emplace_back(std::move(param_status).message());
    }
  }
  if (!parameter_errors.empty()) {
    return absl::InvalidArgumentError(absl::StrJoin(parameter_errors, "; "));
  }
  return absl::OkStatus();
}

absl::Status GurobiSolver::AddNewVariables(
    const VariablesProto& new_variables) {
  const int num_new_variables = new_variables.lower_bounds().size();
  std::vector<char> variable_type(num_new_variables);
  for (int j = 0; j < num_new_variables; ++j) {
    const VariableId id = new_variables.ids(j);
    gtl::InsertOrDie(&variables_map_, id, j + num_gurobi_variables_);
    variable_type[j] = new_variables.integers(j) ? GRB_INTEGER : GRB_CONTINUOUS;
  }
  // We need to copy the names, RepeatedPtrField cannot be converted to
  // absl::Span<std::string>.
  const std::vector<std::string> variable_names =
      TruncateNames(new_variables.names());
  RETURN_IF_ERROR(gurobi_->AddVars(
      /*obj=*/{},
      /*lb=*/new_variables.lower_bounds(),
      /*ub=*/new_variables.upper_bounds(),
      /*vtype=*/variable_type, variable_names));
  num_gurobi_variables_ += num_new_variables;

  return absl::OkStatus();
}

// Given a vector of pairs<LinearConstraintId,ConstraintData&> add a slack
// variable for each of the constraints in the underlying `gurobi_`
// using the referenced bounds.
absl::Status GurobiSolver::AddNewSlacks(
    const std::vector<SlackInfo>& new_slacks) {
  // Note that we are really adding the sub-matrix
  //    D * slack
  // to the set of linear constraints, and the D matrix is stored in compressed
  // sparse column (CSC) format. In our particular case, D is a diagonal matrix
  // with -1.0 coefficients for each new slack in the row indicated in the
  // row_indices vector.
  const int num_slacks = new_slacks.size();
  if (num_slacks == 0) {
    return absl::OkStatus();
  }
  // Build the D matrix in CSC format.
  const std::vector<double> column_non_zeros(num_slacks, -1.0);
  std::vector<double> lower_bounds;
  std::vector<double> upper_bounds;
  const std::vector<char> vtypes(num_slacks, GRB_CONTINUOUS);
  std::vector<GurobiLinearConstraintIndex> row_indices;
  std::vector<int> column_non_zero_begin;
  column_non_zero_begin.reserve(num_slacks);
  row_indices.reserve(num_slacks);
  lower_bounds.reserve(num_slacks);
  upper_bounds.reserve(num_slacks);
  for (int k = 0; k < num_slacks; ++k) {
    auto& [id, constraint_data] = new_slacks[k];
    gtl::InsertOrDie(&slack_map_, id, constraint_data);
    row_indices.emplace_back(constraint_data.constraint_index);
    lower_bounds.emplace_back(constraint_data.lower_bound);
    upper_bounds.emplace_back(constraint_data.upper_bound);
    column_non_zero_begin.emplace_back(k);
  }
  // Add variables to the underlying model.
  RETURN_IF_ERROR(gurobi_->AddVars(/*vbegin=*/column_non_zero_begin,
                                   /*vind=*/row_indices,
                                   /*vval=*/column_non_zeros, /*obj=*/{},
                                   /*lb=*/lower_bounds, /*ub=*/upper_bounds,
                                   /*vtype=*/vtypes, /*names=*/{}));
  num_gurobi_variables_ += num_slacks;
  return absl::OkStatus();
}

absl::Status GurobiSolver::AddNewConstraints(
    const LinearConstraintsProto& constraints) {
  const int num_model_constraints = num_gurobi_constraints();
  const int num_new_constraints = constraints.lower_bounds().size();

  // We need to copy the names, RepeatedPtrField cannot be converted to
  // absl::Span<std::string>.
  const std::vector<std::string> constraint_names =
      TruncateNames(constraints.names());
  // Constraints are translated into:
  // 1.  ax <= upper_bound (if lower bound <= -GRB_INFINITY, and upper_bound
  //                        is finite and less than GRB_INFINITY)
  // 2.  ax >= lower_bound (if upper bound >= GRB_INFINITY, and lower_bound is
  //                        finite and greater than -GRB_INFINITY)
  // 3.  ax == xxxxx_bound (if both bounds are finite, equal, and their
  //                        absolute values less than GRB_INFINITY)
  // 4.  ax - slack = 0.0  (otherwise,
  //                        slack bounds == [lower_bound, upper_bound])
  std::vector<double> constraint_rhs;
  std::vector<char> constraint_sense;
  std::vector<SlackInfo> new_slacks;
  constraint_rhs.reserve(num_new_constraints);
  constraint_sense.reserve(num_new_constraints);
  new_slacks.reserve(num_new_constraints);
  for (int i = 0; i < num_new_constraints; ++i) {
    const int64_t id = constraints.ids(i);
    ConstraintData& constraint_data =
        gtl::InsertKeyOrDie(&linear_constraints_map_, id);
    constraint_data.lower_bound = constraints.lower_bounds(i);
    constraint_data.upper_bound = constraints.upper_bounds(i);
    constraint_data.constraint_index = i + num_model_constraints;
    char sense = GRB_EQUAL;
    double rhs = 0.0;
    // Detect the type of constraint to add and store RHS and bounds.
    if (constraint_data.lower_bound <= -GRB_INFINITY &&
        constraint_data.upper_bound < GRB_INFINITY) {
      rhs = constraint_data.upper_bound;
      sense = GRB_LESS_EQUAL;
    } else if (constraint_data.lower_bound > -GRB_INFINITY &&
               constraint_data.upper_bound >= GRB_INFINITY) {
      rhs = constraint_data.lower_bound;
      sense = GRB_GREATER_EQUAL;
    } else if (constraint_data.lower_bound == constraint_data.upper_bound) {
      rhs = constraint_data.lower_bound;
      sense = GRB_EQUAL;
    } else {
      // Note that constraints where the lower bound and the upper bound are
      // -+infinity translate into a range constraint with an unbounded slack.
      constraint_data.slack_index = new_slacks.size() + num_gurobi_variables_;
      new_slacks.emplace_back(id, constraint_data);
    }
    constraint_rhs.emplace_back(rhs);
    constraint_sense.emplace_back(sense);
  }
  // Add all constraints in one call.
  RETURN_IF_ERROR(
      gurobi_->AddConstrs(constraint_sense, constraint_rhs, constraint_names));
  // Add slacks for true ranged constraints (if needed)
  if (!new_slacks.empty()) {
    RETURN_IF_ERROR(AddNewSlacks(new_slacks));
  }
  return absl::OkStatus();
}

absl::Status GurobiSolver::ChangeCoefficients(
    const SparseDoubleMatrixProto& matrix) {
  const int num_coefficients = matrix.row_ids().size();
  std::vector<GurobiLinearConstraintIndex> row_index(num_coefficients);
  std::vector<GurobiVariableIndex> col_index(num_coefficients);
  for (int k = 0; k < num_coefficients; ++k) {
    row_index[k] =
        linear_constraints_map_.at(matrix.row_ids(k)).constraint_index;
    col_index[k] = variables_map_.at(matrix.column_ids(k));
  }
  return gurobi_->ChgCoeffs(row_index, col_index, matrix.coefficients());
}

absl::Status GurobiSolver::UpdateDoubleListAttribute(
    const SparseDoubleVectorProto& update, const char* attribute_name,
    const IdHashMap& id_hash_map) {
  if (update.ids_size() == 0) {
    return absl::OkStatus();
  }
  std::vector<int> index;
  index.reserve(update.ids_size());
  for (const int64_t id : update.ids()) {
    index.push_back(id_hash_map.at(id));
  }
  return gurobi_->SetDoubleAttrList(attribute_name, index, update.values());
}

absl::Status GurobiSolver::UpdateInt32ListAttribute(
    const SparseInt32VectorProto& update, const char* attribute_name,
    const IdHashMap& id_hash_map) {
  if (update.ids_size() == 0) {
    return absl::OkStatus();
  }
  std::vector<int> index;
  index.reserve(update.ids_size());
  for (const int64_t id : update.ids()) {
    index.push_back(id_hash_map.at(id));
  }
  return gurobi_->SetIntAttrList(attribute_name, index, update.values());
}

absl::Status GurobiSolver::LoadModel(const ModelProto& input_model) {
  CHECK(gurobi_ != nullptr);
  RETURN_IF_ERROR(gurobi_->SetStringAttr(GRB_STR_ATTR_MODELNAME,
                                         TruncateName(input_model.name())));
  RETURN_IF_ERROR(AddNewVariables(input_model.variables()));

  RETURN_IF_ERROR(AddNewConstraints(input_model.linear_constraints()));

  RETURN_IF_ERROR(ChangeCoefficients(input_model.linear_constraint_matrix()));

  const int model_sense =
      input_model.objective().maximize() ? GRB_MAXIMIZE : GRB_MINIMIZE;
  RETURN_IF_ERROR(gurobi_->SetIntAttr(GRB_INT_ATTR_MODELSENSE, model_sense));
  RETURN_IF_ERROR(gurobi_->SetDoubleAttr(GRB_DBL_ATTR_OBJCON,
                                         input_model.objective().offset()));

  RETURN_IF_ERROR(
      UpdateDoubleListAttribute(input_model.objective().linear_coefficients(),
                                GRB_DBL_ATTR_OBJ, variables_map_));
  RETURN_IF_ERROR(ResetQuadraticObjectiveTerms(
      input_model.objective().quadratic_coefficients()));
  return absl::OkStatus();
}

absl::Status GurobiSolver::ResetQuadraticObjectiveTerms(
    const SparseDoubleMatrixProto& terms) {
  quadratic_objective_coefficients_.clear();
  RETURN_IF_ERROR(gurobi_->DelQ());
  const int num_terms = terms.row_ids().size();
  if (num_terms > 0) {
    std::vector<GurobiVariableIndex> first_var_index(num_terms);
    std::vector<GurobiVariableIndex> second_var_index(num_terms);
    for (int k = 0; k < num_terms; ++k) {
      const VariableId row_id = terms.row_ids(k);
      const VariableId column_id = terms.column_ids(k);
      first_var_index[k] = variables_map_.at(row_id);
      second_var_index[k] = variables_map_.at(column_id);
      quadratic_objective_coefficients_[{row_id, column_id}] =
          terms.coefficients(k);
    }
    RETURN_IF_ERROR(gurobi_->AddQpTerms(first_var_index, second_var_index,
                                        terms.coefficients()));
  }
  return absl::OkStatus();
}

absl::Status GurobiSolver::UpdateQuadraticObjectiveTerms(
    const SparseDoubleMatrixProto& terms) {
  CHECK(gurobi_ != nullptr);
  const int num_terms = terms.row_ids().size();
  if (num_terms > 0) {
    std::vector<GurobiVariableIndex> first_var_index(num_terms);
    std::vector<GurobiVariableIndex> second_var_index(num_terms);
    std::vector<double> coefficient_updates(num_terms);
    for (int k = 0; k < num_terms; ++k) {
      const VariableId row_id = terms.row_ids(k);
      const VariableId column_id = terms.column_ids(k);
      first_var_index[k] = variables_map_.at(row_id);
      second_var_index[k] = variables_map_.at(column_id);
      const std::pair<VariableId, VariableId> qp_term_key(row_id, column_id);
      const double new_coefficient = terms.coefficients(k);
      // Gurobi will maintain any existing quadratic coefficients unless we
      // call GRBdelq (which we don't). So, since stored entries in terms
      // specify the target coefficients, we need to compute the difference from
      // the existing coefficient with Gurobi, if any.
      coefficient_updates[k] =
          new_coefficient - quadratic_objective_coefficients_[qp_term_key];
      quadratic_objective_coefficients_[qp_term_key] = new_coefficient;
    }
    RETURN_IF_ERROR(gurobi_->AddQpTerms(first_var_index, second_var_index,
                                        coefficient_updates));
  }
  return absl::OkStatus();
}

// Bound changes in constraints can induce new variables, and also remove
// some slacks. We first add all new variables, and queue all deletions to be
// dealt with later on.
absl::Status GurobiSolver::UpdateLinearConstraints(
    const LinearConstraintUpdatesProto& constraints_update,
    std::vector<GurobiVariableIndex>& deleted_variables_index) {
  const SparseDoubleVectorProto& constraint_lower_bounds =
      constraints_update.lower_bounds();
  const SparseDoubleVectorProto& constraint_upper_bounds =
      constraints_update.upper_bounds();

  // If no update, just return.
  if (constraint_lower_bounds.ids().empty() &&
      constraint_upper_bounds.ids().empty()) {
    return absl::OkStatus();
  }

  // We want to avoid changing the right-hand-side, sense, or slacks of each
  // constraint more than once. Since we can refer to the same constraint ID
  // both in the `constraint_upper_bounds` and `constraint_lower_bounds` sparse
  // vectors, we collect all changes into a single structure:
  struct UpdateConstraintData {
    LinearConstraintId constraint_id;
    ConstraintData& source;
    double new_lower_bound;
    double new_upper_bound;
    UpdateConstraintData(const LinearConstraintId id, ConstraintData& reference)
        : constraint_id(id),
          source(reference),
          new_lower_bound(reference.lower_bound),
          new_upper_bound(reference.upper_bound) {}
  };
  const int upper_bounds_size = constraint_upper_bounds.ids().size();
  const int lower_bounds_size = constraint_lower_bounds.ids().size();
  std::vector<UpdateConstraintData> update_vector;
  update_vector.reserve(upper_bounds_size + lower_bounds_size);
  // We exploit the fact that IDs are sorted in increasing order to merge
  // changes into a vector of aggregated changes.
  for (int lower_index = 0, upper_index = 0;
       lower_index < lower_bounds_size || upper_index < upper_bounds_size;) {
    VariableId lower_id = std::numeric_limits<int64_t>::max();
    if (lower_index < lower_bounds_size) {
      lower_id = constraint_lower_bounds.ids(lower_index);
    }
    VariableId upper_id = std::numeric_limits<int64_t>::max();
    if (upper_index < upper_bounds_size) {
      upper_id = constraint_upper_bounds.ids(upper_index);
    }
    const VariableId id = std::min(lower_id, upper_id);
    DCHECK(id < std::numeric_limits<int64_t>::max());
    UpdateConstraintData update(id, linear_constraints_map_.at(id));
    if (lower_id == upper_id) {
      update.new_lower_bound = constraint_lower_bounds.values(lower_index++);
      update.new_upper_bound = constraint_upper_bounds.values(upper_index++);
    } else if (lower_id < upper_id) {
      update.new_lower_bound = constraint_lower_bounds.values(lower_index++);
    } else { /* upper_id < lower_id */
      update.new_upper_bound = constraint_upper_bounds.values(upper_index++);
    }
    update_vector.emplace_back(update);
  }

  // We have grouped all changes in update_vector, now generate changes in
  // slack bounds, rhs, senses, new slacks, and deleted_slacks (to be dealt
  // with later, outside this function).
  // These three vectors keep changes to right-hand-side and senses.
  std::vector<char> sense_data;
  std::vector<double> rhs_data;
  std::vector<GurobiLinearConstraintIndex> rhs_index;
  // These three vectors keep changes to bounds on existing slack.
  std::vector<double> lower_bound_data;
  std::vector<double> upper_bound_data;
  std::vector<GurobiVariableIndex> bound_index;
  // This vector keep newly introduced slacks.
  std::vector<SlackInfo> new_slacks;
  // Iterate on the changes, and populate the three possible changes.
  for (UpdateConstraintData& update_data : update_vector) {
    const bool same_lower_bound =
        (update_data.source.lower_bound == update_data.new_lower_bound) ||
        ((update_data.source.lower_bound <= -GRB_INFINITY) &&
         (update_data.new_lower_bound <= -GRB_INFINITY));
    const bool same_upper_bound =
        (update_data.source.upper_bound == update_data.new_upper_bound) ||
        ((update_data.source.upper_bound >= GRB_INFINITY) &&
         (update_data.new_upper_bound >= GRB_INFINITY));
    if (same_upper_bound && same_lower_bound) continue;
    // Save into linear_constraints_map_[id] the new bounds for the linear
    // constraint.
    update_data.source.lower_bound = update_data.new_lower_bound;
    update_data.source.upper_bound = update_data.new_upper_bound;
    bool delete_slack = false;
    // Detect the type of constraint to add and store RHS and bounds.
    if (update_data.new_lower_bound <= -GRB_INFINITY &&
        update_data.new_upper_bound < GRB_INFINITY) {
      delete_slack = true;
      rhs_index.emplace_back(update_data.source.constraint_index);
      rhs_data.emplace_back(update_data.new_upper_bound);
      sense_data.emplace_back(GRB_LESS_EQUAL);
    } else if (update_data.new_lower_bound > -GRB_INFINITY &&
               update_data.new_upper_bound >= GRB_INFINITY) {
      delete_slack = true;
      rhs_index.emplace_back(update_data.source.constraint_index);
      rhs_data.emplace_back(update_data.new_lower_bound);
      sense_data.emplace_back(GRB_GREATER_EQUAL);
    } else if (update_data.new_lower_bound == update_data.new_upper_bound) {
      delete_slack = true;
      rhs_index.emplace_back(update_data.source.constraint_index);
      rhs_data.emplace_back(update_data.new_lower_bound);
      sense_data.emplace_back(GRB_EQUAL);
    } else {
      // Note that constraints where the lower bound and the upper bound are
      // -+infinity translated into a range constraint with an unbounded
      // slack.
      if (update_data.source.slack_index != kUnspecifiedIndex) {
        bound_index.emplace_back(update_data.source.slack_index);
        lower_bound_data.emplace_back(update_data.new_lower_bound);
        upper_bound_data.emplace_back(update_data.new_upper_bound);
      } else {
        // Note that if we add a new slack, we must both reset the sense and
        // right hand side for the inequality.
        rhs_index.emplace_back(update_data.source.constraint_index);
        rhs_data.emplace_back(0.0);
        sense_data.emplace_back(GRB_EQUAL);
        // Update the slack_index in the linear_constraints_map_[id]
        update_data.source.slack_index =
            new_slacks.size() + num_gurobi_variables_;
        // Save the data needed to add the new slack.
        new_slacks.emplace_back(update_data.constraint_id, update_data.source);
      }
    }
    // If the constraint had a slack, and now is marked for deletion, we reset
    // the stored slack_index in linear_constraints_map_[id], save the index
    // in the list of variables to be deleted later on and remove the constraint
    // from slack_map_.
    if (delete_slack && update_data.source.slack_index != kUnspecifiedIndex) {
      deleted_variables_index.emplace_back(update_data.source.slack_index);
      update_data.source.slack_index = kUnspecifiedIndex;
      slack_map_.erase(update_data.constraint_id);
    }
  }

  // Pass down changes to Gurobi.
  if (!rhs_index.empty()) {
    RETURN_IF_ERROR(
        gurobi_->SetDoubleAttrList(GRB_DBL_ATTR_RHS, rhs_index, rhs_data));
    RETURN_IF_ERROR(
        gurobi_->SetCharAttrList(GRB_CHAR_ATTR_SENSE, rhs_index, sense_data));
  }  // rhs changes
  if (!bound_index.empty()) {
    RETURN_IF_ERROR(gurobi_->SetDoubleAttrList(GRB_DBL_ATTR_LB, bound_index,
                                               lower_bound_data));
    RETURN_IF_ERROR(gurobi_->SetDoubleAttrList(GRB_DBL_ATTR_UB, bound_index,
                                               upper_bound_data));
  }  // Slack bound changes.

  if (!new_slacks.empty()) {
    RETURN_IF_ERROR(AddNewSlacks(new_slacks));
  }
  return absl::OkStatus();
}

// This function re-assign indices for variables and constraints after
// deletion. The updated indices are computed from the previous indices, sorted
// in incremental form, but re-assigned so that all indices are contiguous
// between [0, num_variables-1] and [0, num_linear_constraints-1].
// This implementation exploit the fact that gtl::linked_hash_map preserves the
// insertion order of whatever elements remain in the hash tables.
absl::Status GurobiSolver::UpdateGurobiIndices() {
  {  // Recover index of variables.
    GurobiVariableIndex next_index = 0;
    GurobiVariableIndex prev_index = kUnspecifiedIndex;
    auto variable_it = variables_map_.begin();
    auto slack_it = slack_map_.begin();
    while (variable_it != variables_map_.end() ||
           slack_it != slack_map_.end()) {
      GurobiVariableIndex variable_index = std::numeric_limits<int32_t>::max();
      if (variable_it != variables_map_.end()) {
        variable_index = variable_it->second;
      }
      GurobiVariableIndex slack_index = std::numeric_limits<int32_t>::max();
      if (slack_it != slack_map_.end()) {
        slack_index = slack_it->second.slack_index;
      }
      DCHECK_LT(prev_index, variable_index);
      DCHECK_LT(prev_index, slack_index);
      DCHECK_NE(variable_index, slack_index);
      if (slack_index < variable_index) {
        prev_index = slack_index;
        slack_it->second.slack_index = next_index++;
        ++slack_it;
      } else {
        prev_index = variable_index;
        variable_it->second = next_index++;
        ++variable_it;
      }
    }
    DCHECK_EQ(next_index, num_gurobi_variables_);
  }
  {  // Recover index of constraints.
    GurobiLinearConstraintIndex next_constraint = 0;
    GurobiLinearConstraintIndex prev_constraint = kUnspecifiedConstraint;
    for (auto& constraint_iterator : linear_constraints_map_) {
      DCHECK_LT(prev_constraint, constraint_iterator.second.constraint_index);
      prev_constraint = constraint_iterator.second.constraint_index;
      constraint_iterator.second.constraint_index = next_constraint++;
    }
    DCHECK_EQ(next_constraint, num_gurobi_constraints());
  }
  return absl::OkStatus();
}

absl::Status GurobiSolver::Update(const ModelUpdateProto& model_update) {
  RETURN_IF_ERROR(AddNewVariables(model_update.new_variables()));

  RETURN_IF_ERROR(AddNewConstraints(model_update.new_linear_constraints()));

  RETURN_IF_ERROR(
      ChangeCoefficients(model_update.linear_constraint_matrix_updates()));

  if (model_update.objective_updates().has_direction_update()) {
    const int model_sense = model_update.objective_updates().direction_update()
                                ? GRB_MAXIMIZE
                                : GRB_MINIMIZE;
    RETURN_IF_ERROR(gurobi_->SetIntAttr(GRB_INT_ATTR_MODELSENSE, model_sense));
  }

  if (model_update.objective_updates().has_offset_update()) {
    RETURN_IF_ERROR(gurobi_->SetDoubleAttr(
        GRB_DBL_ATTR_OBJCON, model_update.objective_updates().offset_update()));
  }

  RETURN_IF_ERROR(UpdateDoubleListAttribute(
      model_update.objective_updates().linear_coefficients(), GRB_DBL_ATTR_OBJ,
      variables_map_));

  RETURN_IF_ERROR(UpdateQuadraticObjectiveTerms(
      model_update.objective_updates().quadratic_coefficients()));

  RETURN_IF_ERROR(
      UpdateDoubleListAttribute(model_update.variable_updates().lower_bounds(),
                                GRB_DBL_ATTR_LB, variables_map_));

  RETURN_IF_ERROR(
      UpdateDoubleListAttribute(model_update.variable_updates().upper_bounds(),
                                GRB_DBL_ATTR_UB, variables_map_));

  if (model_update.variable_updates().has_integers()) {
    const SparseBoolVectorProto& update =
        model_update.variable_updates().integers();
    std::vector<GurobiVariableIndex> index;
    index.reserve(update.ids_size());
    for (const int64_t id : update.ids()) {
      index.push_back(variables_map_.at(id));
    }
    std::vector<char> value;
    value.reserve(update.values_size());
    for (const bool val : update.values()) {
      value.push_back(val ? GRB_INTEGER : GRB_CONTINUOUS);
    }
    RETURN_IF_ERROR(
        gurobi_->SetCharAttrList(GRB_CHAR_ATTR_VTYPE, index, value));
  }

  // Now we update quadratic_objective_coefficients_, removing any terms where
  // either or both of the involved variables are about to be deleted.
  const absl::flat_hash_set<VariableId> variable_ids_to_be_deleted(
      model_update.deleted_variable_ids().begin(),
      model_update.deleted_variable_ids().end());
  // NOTE: Introducing more state and complexity should speed this up, but we
  // opt for the simpler approach for now.
  for (auto it = quadratic_objective_coefficients_.cbegin();
       it != quadratic_objective_coefficients_.cend();
       /*incremented in loop*/) {
    if (variable_ids_to_be_deleted.contains(it->first.first) ||
        variable_ids_to_be_deleted.contains(it->first.second)) {
      quadratic_objective_coefficients_.erase(it++);
    } else {
      ++it;
    }
  }
  // We cache all Gurobi variables and constraint indices that must be deleted,
  // and perform deletions at the end of the update call.
  std::vector<GurobiVariableIndex> deleted_variables_index;
  std::vector<GurobiLinearConstraintIndex> deleted_constraints_index;

  RETURN_IF_ERROR(UpdateLinearConstraints(
      model_update.linear_constraint_updates(), deleted_variables_index));

  for (const VariableId id : model_update.deleted_variable_ids()) {
    deleted_variables_index.emplace_back(variables_map_.at(id));
    variables_map_.erase(id);
  }

  for (const LinearConstraintId id :
       model_update.deleted_linear_constraint_ids()) {
    ConstraintData& constraint_data = linear_constraints_map_.at(id);
    deleted_constraints_index.emplace_back(constraint_data.constraint_index);
    if (constraint_data.slack_index != kUnspecifiedIndex) {
      deleted_variables_index.emplace_back(constraint_data.slack_index);
      constraint_data.slack_index = kUnspecifiedIndex;
      slack_map_.erase(id);
    }
    linear_constraints_map_.erase(id);
  }

  // If no cached deletions, we are done.
  if (deleted_variables_index.empty() && deleted_constraints_index.empty()) {
    return absl::OkStatus();
  }
  // If we are removing variables or constraints we remove them after adding
  // any variable or constraint. This is to avoid problems with
  // the numbering of possibly new variables and constraints.
  // After that we must update the model so that sequence of updates don't
  // interfere with one-another.
  if (!deleted_constraints_index.empty()) {
    RETURN_IF_ERROR(gurobi_->DelConstrs(deleted_constraints_index));
  }

  if (!deleted_variables_index.empty()) {
    RETURN_IF_ERROR(gurobi_->DelVars(deleted_variables_index));
    num_gurobi_variables_ -= deleted_variables_index.size();
  }

  // If we removed variables or constraints we must flush all pending changes
  // to synchronize the number of variables and constraints with the Gurobi
  // model.
  RETURN_IF_ERROR(gurobi_->UpdateModel());
  // Regenerate indices.
  RETURN_IF_ERROR(UpdateGurobiIndices());

  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<GurobiSolver>> GurobiSolver::New(
    const ModelProto& input_model, const SolverInterface::InitArgs& init_args) {
  if (!GurobiIsCorrectlyInstalled()) {
    return absl::InvalidArgumentError("Gurobi is not correctly installed.");
  }
  ASSIGN_OR_RETURN(std::unique_ptr<Gurobi> gurobi,
                   GurobiFromInitArgs(init_args));
  auto gurobi_solver = absl::WrapUnique(new GurobiSolver(std::move(gurobi)));
  RETURN_IF_ERROR(gurobi_solver->LoadModel(input_model));
  return gurobi_solver;
}

absl::StatusOr<std::unique_ptr<GurobiSolver::GurobiCallbackData>>
GurobiSolver::RegisterCallback(const CallbackRegistrationProto& registration,
                               const Callback cb,
                               const MessageCallback message_cb,
                               const absl::Time start,
                               SolveInterrupter* const local_interrupter) {
  const absl::flat_hash_set<CallbackEventProto> events = EventSet(registration);

  // Note that IS_MIP does not necessarily mean the problem has integer
  // variables. Please refer to Gurobi's doc for details:
  // https://www.gurobi.com/documentation/9.1/refman/ismip.html.
  //
  // Here we assume that we get MIP related events and use a MIP solving
  // stragegy when IS_MIP is true.
  ASSIGN_OR_RETURN(const int is_mip, gurobi_->GetIntAttr(GRB_INT_ATTR_IS_MIP));

  RETURN_IF_ERROR(CheckRegisteredCallbackEvents(
      registration, is_mip ? SupportedMIPEvents() : SupportedLPEvents()))
      << "for a " << (is_mip ? "MIP" : "LP") << " model";

  // Set Gurobi parameters.
  if (message_cb != nullptr) {
    // Disable logging messages to the console the user wants to handle
    // messages.
    RETURN_IF_ERROR(gurobi_->SetIntParam(GRB_INT_PAR_LOGTOCONSOLE, 0));
  }
  if (registration.add_cuts() || registration.add_lazy_constraints()) {
    // This is to signal the solver presolve to limit primal transformations
    // that precludes crushing cuts to the presolved model.
    RETURN_IF_ERROR(gurobi_->SetIntParam(GRB_INT_PAR_PRECRUSH, 1));
  }
  if (registration.add_lazy_constraints()) {
    // This is needed so that the solver knows that some presolve reductions
    // can not be performed safely.
    RETURN_IF_ERROR(gurobi_->SetIntParam(GRB_INT_PAR_LAZYCONSTRAINTS, 1));
  }
  return std::make_unique<GurobiCallbackData>(
      GurobiCallbackInput{
          .user_cb = cb,
          .message_cb = message_cb,
          .variable_ids = variables_map_,
          .num_gurobi_vars = num_gurobi_variables_,
          .events = EventToGurobiWhere(events),
          .mip_solution_filter = registration.mip_solution_filter(),
          .mip_node_filter = registration.mip_node_filter(),
          .start = start},
      local_interrupter);
}

absl::StatusOr<InvertedBounds> GurobiSolver::ListInvertedBounds() const {
  InvertedBounds inverted_bounds;
  {
    ASSIGN_OR_RETURN(
        const std::vector<double> var_lbs,
        gurobi_->GetDoubleAttrArray(GRB_DBL_ATTR_LB, num_gurobi_variables_));
    ASSIGN_OR_RETURN(
        const std::vector<double> var_ubs,
        gurobi_->GetDoubleAttrArray(GRB_DBL_ATTR_UB, num_gurobi_variables_));
    for (const auto& [id, index] : variables_map_) {
      if (var_lbs[index] > var_ubs[index]) {
        inverted_bounds.variables.push_back(id);
      }
    }
  }
  for (const auto& [id, cstr_data] : linear_constraints_map_) {
    if (cstr_data.lower_bound > cstr_data.upper_bound) {
      inverted_bounds.linear_constraints.push_back(id);
    }
  }

  // Above code have inserted ids in non-stable order.
  std::sort(inverted_bounds.variables.begin(), inverted_bounds.variables.end());
  std::sort(inverted_bounds.linear_constraints.begin(),
            inverted_bounds.linear_constraints.end());
  return inverted_bounds;
}

absl::StatusOr<SolveResultProto> GurobiSolver::Solve(
    const SolveParametersProto& parameters,
    const ModelSolveParametersProto& model_parameters,
    const MessageCallback message_cb,
    const CallbackRegistrationProto& callback_registration, const Callback cb,
    SolveInterrupter* const interrupter) {
  const absl::Time start = absl::Now();
  // We must set the parameters before calling RegisterCallback since it changes
  // some parameters depending on the callback registration.
  RETURN_IF_ERROR(SetParameters(parameters));

  // We use a local interrupter that will triggers the calls to GRBterminate()
  // when either the user interrupter is triggered or when a callback returns a
  // true `terminate`.
  std::unique_ptr<SolveInterrupter> local_interrupter;
  if (cb != nullptr || interrupter != nullptr) {
    local_interrupter = std::make_unique<SolveInterrupter>();
  }
  const ScopedSolveInterrupterCallback scoped_terminate_callback(
      local_interrupter.get(), [&]() {
        // Make an immediate call to GRBterminate() as soon as this interrupter
        // is triggered (which may immediately happen in the code below when it
        // is chained with the optional user interrupter).
        //
        // This call may happen too early. This is not an issue since we will
        // repeat this call at each call of the Gurobi callback. See the comment
        // in GurobiCallbackImpl() for details.
        gurobi_->Terminate();
      });

  // Chain the user interrupter to the local interrupter. If/when the user
  // interrupter is triggered, this triggers the local interrupter. This may
  // happen immediately if the user interrupter is already triggered.
  //
  // The local interrupter can also be triggered by a callback returning a true
  // `terminate`.
  const ScopedSolveInterrupterCallback scoped_chaining_callback(
      interrupter, [&]() { local_interrupter->Interrupt(); });

  // Need to run GRBupdatemodel before registering callbacks (to test if the
  // problem is a MIP), setting basis and getting the obj sense.
  RETURN_IF_ERROR(gurobi_->UpdateModel());

  if (model_parameters.has_initial_basis()) {
    RETURN_IF_ERROR(SetGurobiBasis(model_parameters.initial_basis()));
  }
  RETURN_IF_ERROR(gurobi_->SetIntAttr(GRB_INT_ATTR_NUMSTART,
                                      model_parameters.solution_hints_size()));
  for (int i = 0; i < model_parameters.solution_hints_size(); ++i) {
    RETURN_IF_ERROR(gurobi_->SetIntParam(GRB_INT_PAR_STARTNUMBER, i));
    RETURN_IF_ERROR(UpdateDoubleListAttribute(
        model_parameters.solution_hints(i).variable_values(),
        GRB_DBL_ATTR_START, variables_map_));
  }
  RETURN_IF_ERROR(
      UpdateInt32ListAttribute(model_parameters.branching_priorities(),
                               GRB_INT_ATTR_BRANCHPRIORITY, variables_map_));

  // Here we register the callback when we either have a user callback or a
  // local interrupter. The rationale for doing so when we have only an
  // interrupter is explained in GurobiCallbackImpl().
  Gurobi::Callback grb_cb = nullptr;
  std::unique_ptr<GurobiCallbackData> gurobi_cb_data;
  if (cb != nullptr || local_interrupter != nullptr || message_cb != nullptr) {
    ASSIGN_OR_RETURN(gurobi_cb_data,
                     RegisterCallback(callback_registration, cb, message_cb,
                                      start, local_interrupter.get()));
    grb_cb = [&gurobi_cb_data](
                 const Gurobi::CallbackContext& cb_context) -> absl::Status {
      return GurobiCallbackImpl(cb_context, gurobi_cb_data->callback_input,
                                gurobi_cb_data->message_callback_data,
                                gurobi_cb_data->local_interrupter);
    };
  }

  // Gurobi returns "infeasible" when bounds are inverted.
  {
    ASSIGN_OR_RETURN(const InvertedBounds inverted_bounds,
                     ListInvertedBounds());
    RETURN_IF_ERROR(inverted_bounds.ToStatus());
  }

  RETURN_IF_ERROR(gurobi_->Optimize(grb_cb));

  // We flush message callbacks before testing for Gurobi error in case where
  // the unfinished line of message would help with the error.
  if (gurobi_cb_data != nullptr) {
    GurobiCallbackImplFlush(gurobi_cb_data->callback_input,
                            gurobi_cb_data->message_callback_data);
  }

  ASSIGN_OR_RETURN(SolveResultProto solve_result,
                   ExtractSolveResultProto(start, model_parameters));
  // Reset Gurobi parameters.
  // TODO(user): ensure that resetting parameters does not degrade
  // incrementalism performance.
  RETURN_IF_ERROR(gurobi_->ResetParameters());

  return solve_result;
}

bool GurobiSolver::CanUpdate(const ModelUpdateProto& model_update) {
  return true;
}

MATH_OPT_REGISTER_SOLVER(SOLVER_TYPE_GUROBI, GurobiSolver::New)

}  // namespace math_opt
}  // namespace operations_research
