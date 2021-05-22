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
#include <cstdint>
#include <functional>
#include <limits>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "absl/cleanup/cleanup.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/linked_hash_map.h"
#include "ortools/base/map_util.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/solvers/gurobi_callback.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/port/proto_utils.h"
#include "absl/status/status.h"
#include "ortools/base/status_macros.h"
#include "ortools/base/protoutil.h"

#include "ortools/gurobi/environment.h"

namespace operations_research {
namespace math_opt {
namespace {

inline BasisStatus ConvertVariableStatus(const int status) {
  switch (status) {
    case GRB_BASIC:
      return BasisStatus::BASIC;
    case GRB_NONBASIC_LOWER:
      return BasisStatus::AT_LOWER_BOUND;
    case GRB_NONBASIC_UPPER:
      return BasisStatus::AT_UPPER_BOUND;
    case GRB_SUPERBASIC:
      return BasisStatus::FREE;
    default:
      return BasisStatus::INVALID;
  }
}

inline int GrbVariableStatus(const BasisStatus status) {
  switch (status) {
    case BasisStatus::BASIC:
      return GRB_BASIC;
    case BasisStatus::AT_LOWER_BOUND:
    case BasisStatus::FIXED_VALUE:
      return GRB_NONBASIC_LOWER;
    case BasisStatus::AT_UPPER_BOUND:
      return GRB_NONBASIC_UPPER;
    case BasisStatus::FREE:
      return GRB_SUPERBASIC;
    case BasisStatus::INVALID:
    default:
      LOG(FATAL) << "Unexpected invalid initial_basis.";
      return 0;
  }
}

constexpr int kGrbOk = 0;

GurobiParametersProto MergeParameters(
    const CommonSolveParametersProto& common_parameters,
    const GurobiParametersProto& gurobi_parameters) {
  GurobiParametersProto merged_parameters;
  if (common_parameters.has_enable_output()) {
    // TODO(user): Gurobi allows for custom display callbacks. This
    // allows redirecting output to a file, or parsing it and retrieve key
    // information, or redirect it to IDE outputs (such as in Jupiter
    // Notebooks). We should install such a callback to manage output if this is
    // enabled.
    const int enable_output = common_parameters.enable_output() ? 1 : 0;
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_INT_PAR_LOGTOCONSOLE);
    parameter->set_value(absl::StrCat(enable_output));
  }
  if (common_parameters.has_time_limit()) {
    const double time_limit = absl::ToDoubleSeconds(
        util_time::DecodeGoogleApiProto(common_parameters.time_limit())
            .value());
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_DBL_PAR_TIMELIMIT);
    parameter->set_value(absl::StrCat(time_limit));
  }
  if (common_parameters.has_threads()) {
    const int threads = common_parameters.threads();
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_INT_PAR_THREADS);
    parameter->set_value(absl::StrCat(threads));
  }
  if (common_parameters.has_random_seed()) {
    const int random_seed =
        std::min(GRB_MAXINT, std::max(common_parameters.random_seed(), 0));
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_INT_PAR_SEED);
    parameter->set_value(absl::StrCat(random_seed));
  }
  if (common_parameters.lp_algorithm() != LP_ALGORITHM_UNSPECIFIED) {
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_INT_PAR_METHOD);
    switch (common_parameters.lp_algorithm()) {
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
                   << ProtoEnumToString(common_parameters.lp_algorithm())
                   << " unknown, error setting Gurobi parameters";
    }
  }
  if (common_parameters.scaling() != EMPHASIS_UNSPECIFIED) {
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_INT_PAR_SCALEFLAG);
    switch (common_parameters.scaling()) {
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
                   << ProtoEnumToString(common_parameters.scaling())
                   << " unknown, error setting Gurobi parameters";
    }
  }
  if (common_parameters.cuts() != EMPHASIS_UNSPECIFIED) {
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_INT_PAR_CUTS);
    switch (common_parameters.cuts()) {
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
                   << ProtoEnumToString(common_parameters.cuts())
                   << " unknown, error setting Gurobi parameters";
    }
  }
  if (common_parameters.heuristics() != EMPHASIS_UNSPECIFIED) {
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_DBL_PAR_HEURISTICS);
    switch (common_parameters.heuristics()) {
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
                   << ProtoEnumToString(common_parameters.heuristics())
                   << " unknown, error setting Gurobi parameters";
    }
  }

  if (common_parameters.presolve() != EMPHASIS_UNSPECIFIED) {
    GurobiParametersProto::Parameter* const parameter =
        merged_parameters.add_parameters();
    parameter->set_name(GRB_INT_PAR_PRESOLVE);
    switch (common_parameters.presolve()) {
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
                   << ProtoEnumToString(common_parameters.presolve())
                   << " unknown, error setting Gurobi parameters";
    }
  }

  for (const GurobiParametersProto_Parameter& parameter :
       gurobi_parameters.parameters()) {
    *merged_parameters.add_parameters() = parameter;
  }
  return merged_parameters;
}

std::string JoinErrors(const std::vector<absl::Status>& errors) {
  std::string result = "[";
  for (const absl::Status& error : errors) {
    absl::StrAppend(&result, error.message());
  }
  absl::StrAppend(&result, "]");
  return result;
}

absl::StatusOr<int64_t> SafeInt64FromDouble(const double d) {
  const int64_t result = static_cast<int64_t>(d);
  if (static_cast<double>(result) != d) {
    return absl::InternalError(
        absl::StrCat("Expected double ", d, " to contain an int64_t."));
  }
  return result;
}

}  // namespace

std::string GurobiSolver::GurobiErrorMessage(int error_code) const {
  if (error_code == kGrbOk) {
    return {};
  }
  if (active_env_ == nullptr) {
    if (error_code == GRB_ERROR_NO_LICENSE) {
      return absl::StrCat("Gurobi error code:", error_code,
                          " (Failed to obtain a valid license)");
    } else {
      return absl::StrCat("Gurboi error code:", error_code,
                          " (No environment is available)");
    }
  }
  return absl::StrCat("Gurobi error code: ", error_code, ", ",
                      GRBgeterrormsg(active_env_));
}

std::string GurobiSolver::LogGurobiCode(
    int error_code, const char* source_file, int source_line,
    const char* statement, absl::string_view extra_message = "") const {
  if (error_code == kGrbOk) {
    return "";
  }
  const std::string error_message =
      absl::StrFormat("%s:%d, on '%s' : %s%s", source_file, source_line,
                      statement, GurobiErrorMessage(error_code), extra_message);
  VLOG(1) << error_message;
  return error_message;
}

absl::Status GurobiSolver::GurobiCodeToUtilStatus(int error_code,
                                                  const char* source_file,
                                                  int source_line,
                                                  const char* statement) const {
  if (error_code == kGrbOk) {
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(
      LogGurobiCode(error_code, source_file, source_line, statement));
}

// This macro is intended to be used only in class functions of GurobiSolver.
#define RETURN_IF_GUROBI_ERROR(_x_) \
  RETURN_IF_ERROR(GurobiCodeToUtilStatus(_x_, __FILE__, __LINE__, #_x_))

// TODO(user): Use empty environments once we move to Gurobi 9. Here
// we would also setup a log callback.
absl::Status GurobiSolver::LoadEnvironment() {
  CHECK(master_env_ == nullptr);
  RETURN_IF_GUROBI_ERROR(GRBloadenv(&master_env_,
                                    /*logfilename=*/nullptr));
  active_env_ = master_env_;
  CHECK(active_env_ != nullptr);
  RETURN_IF_GUROBI_ERROR(GRBnewmodel(master_env_, &gurobi_model_,
                                     /*Pname=*/nullptr,
                                     /*numvars=*/0,
                                     /*obj=*/nullptr, /*lb=*/nullptr,
                                     /*ub=*/nullptr, /*vtype=*/nullptr,
                                     /*varnames=*/nullptr));
  CHECK(gurobi_model_ != nullptr);
  active_env_ = GRBgetenv(gurobi_model_);
  RETURN_IF_GUROBI_ERROR(
      GRBsetintattr(gurobi_model_, GRB_INT_ATTR_MODELSENSE, GRB_MINIMIZE));

  if (VLOG_IS_ON(3)) {
    int gurobi_major, gurobi_minor, gurobi_technical;
    GRBversion(&gurobi_major, &gurobi_minor, &gurobi_technical);
    VLOG(3) << absl::StrFormat(
        "Successfully opened environment for Gurobi v%d.%d.%d (%s)",
        gurobi_major, gurobi_minor, gurobi_technical, GRBplatform());
  }
  return absl::OkStatus();
}

absl::StatusOr<int> GurobiSolver::GetIntAttr(const char* name) const {
  int result;
  RETURN_IF_GUROBI_ERROR(GRBgetintattr(gurobi_model_, name, &result))
      << "Error getting Gurobi int attribute: " << name;
  return result;
}

absl::StatusOr<double> GurobiSolver::GetDoubleAttr(const char* name) const {
  double result;
  RETURN_IF_GUROBI_ERROR(GRBgetdblattr(gurobi_model_, name, &result))
      << "Error getting Gurobi double attribute: " << name;
  return result;
}

absl::Status GurobiSolver::GetIntAttrArray(const char* name,
                                           absl::Span<int> attr_out) const {
  RETURN_IF_GUROBI_ERROR(GRBgetintattrarray(gurobi_model_, name, 0,
                                            attr_out.size(), attr_out.data()))
      << "Error getting Gurobi int array attribute: " << name;
  return absl::OkStatus();
}

absl::Status GurobiSolver::GetDoubleAttrArray(
    const char* name, absl::Span<double> attr_out) const {
  RETURN_IF_GUROBI_ERROR(GRBgetdblattrarray(gurobi_model_, name, 0,
                                            attr_out.size(), attr_out.data()))
      << "Error getting Gurobi double array attribute: " << name;
  return absl::OkStatus();
}

absl::StatusOr<std::pair<SolveResultProto::TerminationReason, std::string>>
GurobiSolver::ConvertTerminationReason(const int gurobi_status,
                                       const bool has_feasible_solution) {
  switch (gurobi_status) {
    case GRB_OPTIMAL:
      return std::make_pair(SolveResultProto::OPTIMAL, "");
    case GRB_INFEASIBLE:
      return std::make_pair(SolveResultProto::INFEASIBLE, "");
    case GRB_UNBOUNDED: {
      if (has_feasible_solution) {
        return std::make_pair(SolveResultProto::UNBOUNDED, "");
      } else {
        return std::make_pair(SolveResultProto::DUAL_INFEASIBLE,
                              "Gurobi status GRB_UNBOUNDED, but no feasible "
                              "point found, only primal ray.");
      }
    }
    case GRB_INF_OR_UNBD:
      return std::make_pair(SolveResultProto::DUAL_INFEASIBLE,
                            "Gurobi status GRB_INF_OR_UNBD.");
    case GRB_CUTOFF:
      return std::make_pair(SolveResultProto::OBJECTIVE_LIMIT,
                            "Gurobi status GRB_CUTOFF.");
    case GRB_ITERATION_LIMIT:
      return std::make_pair(SolveResultProto::ITERATION_LIMIT, "");
    case GRB_NODE_LIMIT:
      return std::make_pair(SolveResultProto::NODE_LIMIT, "");
    case GRB_TIME_LIMIT:
      return std::make_pair(SolveResultProto::TIME_LIMIT, "");
    case GRB_SOLUTION_LIMIT:
      return std::make_pair(SolveResultProto::SOLUTION_LIMIT, "");
    case GRB_INTERRUPTED:
      return std::make_pair(SolveResultProto::INTERRUPTED, "");
    case GRB_NUMERIC:
      return std::make_pair(SolveResultProto::NUMERICAL_ERROR, "");
    case GRB_SUBOPTIMAL:
      return std::make_pair(SolveResultProto::IMPRECISE, "");
    case GRB_USER_OBJ_LIMIT:
      return std::make_pair(SolveResultProto::OBJECTIVE_LIMIT,
                            "Gurobi status GRB_USR_OBJ_LIMIT.");
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

int GurobiSolver::num_gurobi_constraints() const {
  return linear_constraints_map_.size();
}

absl::StatusOr<bool> GurobiSolver::IsLP() const {
  ASSIGN_OR_RETURN(const int is_mip, GetIntAttr(GRB_INT_ATTR_IS_MIP));
  ASSIGN_OR_RETURN(const int is_qp, GetIntAttr(GRB_INT_ATTR_IS_QP));
  ASSIGN_OR_RETURN(const int is_qcp, GetIntAttr(GRB_INT_ATTR_IS_QCP));
  return !static_cast<bool>(is_mip) && !static_cast<bool>(is_qp) &&
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
        GrbVariableStatus(static_cast<BasisStatus>(value));
  }

  std::vector<int> gurobi_constraint_basis_status;
  gurobi_constraint_basis_status.reserve(num_gurobi_constraints());
  for (const auto [id, value] : MakeView(basis.constraint_status())) {
    const ConstraintData& constraint_data = linear_constraints_map_.at(id);
    // Non-ranged constraints
    if (constraint_data.slack_index == kUnspecifiedIndex) {
      if (value == BasisStatus::BASIC) {
        gurobi_constraint_basis_status.push_back(kGrbBasicConstraint);
      } else {
        gurobi_constraint_basis_status.push_back(kGrbNonBasicConstraint);
      }
      // Ranged constraints
    } else if (value == BasisStatus::BASIC) {
      // Either constraint or MathOpt slack is basic, but not both (because
      // columns for MathOpt slack and internal Gurobi slack are linearly
      // dependent). We choose the MathOpt slack to be basic.
      gurobi_variable_basis_status[constraint_data.slack_index] = GRB_BASIC;
      gurobi_constraint_basis_status.push_back(kGrbNonBasicConstraint);
    } else {
      gurobi_variable_basis_status[constraint_data.slack_index] =
          GrbVariableStatus(static_cast<BasisStatus>(value));
      gurobi_constraint_basis_status.push_back(kGrbNonBasicConstraint);
    }
  }
  RETURN_IF_GUROBI_ERROR(GRBsetintattrarray(
      /*model=*/gurobi_model_, /*attrname=*/GRB_INT_ATTR_VBASIS,
      /*first=*/0, /*len=*/gurobi_variable_basis_status.size(),
      /*newvalues=*/gurobi_variable_basis_status.data()));
  RETURN_IF_GUROBI_ERROR(GRBsetintattrarray(
      /*model=*/gurobi_model_, /*attrname=*/GRB_INT_ATTR_CBASIS,
      /*first=*/0, /*len=*/gurobi_constraint_basis_status.size(),
      /*newvalues=*/gurobi_constraint_basis_status.data()));

  return absl::OkStatus();
}

absl::StatusOr<BasisProto> GurobiSolver::GetGurobiBasis() {
  BasisProto basis;

  std::vector<int> gurobi_variable_basis_status(num_gurobi_variables_);
  RETURN_IF_ERROR(GetIntAttrArray(
      GRB_INT_ATTR_VBASIS, absl::MakeSpan(gurobi_variable_basis_status)));

  for (auto [variable_id, gurobi_variable_index] : variables_map_) {
    basis.mutable_variable_status()->add_ids(variable_id);
    const BasisStatus variable_status = ConvertVariableStatus(
        gurobi_variable_basis_status[gurobi_variable_index]);
    if (variable_status == BasisStatus::INVALID) {
      return absl::InternalError(
          absl::StrCat("Invalid Gurobi variable basis status: ",
                       gurobi_variable_basis_status[gurobi_variable_index]));
    }
    basis.mutable_variable_status()->add_values(variable_status);
  }

  std::vector<int> gurobi_constraint_basis_status(num_gurobi_constraints());
  RETURN_IF_ERROR(GetIntAttrArray(
      GRB_INT_ATTR_CBASIS, absl::MakeSpan(gurobi_constraint_basis_status)));
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
        basis.mutable_constraint_status()->add_values(BasisStatus::BASIC);
      } else {
        basis.mutable_constraint_status()->add_values(
            BasisStatus::AT_UPPER_BOUND);
      }
      // linear_terms >= lower_bound
    } else if (gurobi_data.lower_bound > -GRB_INFINITY &&
               gurobi_data.upper_bound >= GRB_INFINITY) {
      if (gurobi_constraint_status == kGrbBasicConstraint) {
        basis.mutable_constraint_status()->add_values(BasisStatus::BASIC);
      } else {
        basis.mutable_constraint_status()->add_values(
            BasisStatus::AT_LOWER_BOUND);
      }
      // linear_terms == xxxxx_bound
    } else if (gurobi_data.lower_bound == gurobi_data.upper_bound) {
      if (gurobi_constraint_status == kGrbBasicConstraint) {
        basis.mutable_constraint_status()->add_values(BasisStatus::BASIC);
      } else {
        // TODO(user): consider refining this to
        // AT_LOWER_BOUND/AT_UPPER_BOUND using the sign of the dual variable.
        basis.mutable_constraint_status()->add_values(BasisStatus::FIXED_VALUE);
      }
      //   linear_term - slack == 0 (ranged constraint)
    } else {
      const BasisStatus slack_status = ConvertVariableStatus(
          gurobi_variable_basis_status[gurobi_data.slack_index]);
      if (slack_status == BasisStatus::INVALID) {
        return absl::InternalError(absl::StrCat(
            "Invalid Gurobi slack variable basis status: ", slack_status));
      }
      if ((gurobi_constraint_status == kGrbBasicConstraint) ||
          (slack_status == BasisStatus::BASIC)) {
        basis.mutable_constraint_status()->add_values(BasisStatus::BASIC);
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
  std::vector<double> farkas_dual(num_gurobi_constraints());
  RETURN_IF_ERROR(
      GetDoubleAttrArray(GRB_DBL_ATTR_FARKASDUAL, absl::MakeSpan(farkas_dual)));
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
  std::vector<int> constraint_indices(num_gurobi_constraints());
  std::vector<double> coefficients(num_gurobi_constraints());
  {
    SparseVectorFilterPredicate predicate(variables_filter);
    for (auto [var_id, gurobi_variable_index] : variables_map_) {
      // reduced_cost_value = r[gurobi_variable_index]
      //                    = \bar{a}[gurobi_variable_index]
      double reduced_cost_value = 0.0;
      int result_size;
      int vbeg;
      // coefficients = column gurobi_variable_index of A
      RETURN_IF_GUROBI_ERROR(GRBgetvars(
          gurobi_model_, &result_size, &vbeg, constraint_indices.data(),
          coefficients.data(), gurobi_variable_index, 1));
      for (int i = 0; i < result_size; ++i) {
        reduced_cost_value +=
            farkas_dual[constraint_indices[i]] * coefficients[i];
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

absl::Status GurobiSolver::ExtractSolveResultProto(
    const bool is_maximize, SolveResultProto& result,
    const ModelSolveParametersProto& model_parameters) {
  int num_solutions = 0;
  if (GRBisattravailable(gurobi_model_, GRB_INT_ATTR_SOLCOUNT)) {
    ASSIGN_OR_RETURN(num_solutions, GetIntAttr(GRB_INT_ATTR_SOLCOUNT));
  }
  if (GRBisattravailable(gurobi_model_, GRB_INT_ATTR_STATUS)) {
    ASSIGN_OR_RETURN(const int grb_termination,
                     GetIntAttr(GRB_INT_ATTR_STATUS));

    const bool has_feasible_solution = num_solutions > 0;

    ASSIGN_OR_RETURN(
        const auto reason_and_detail,
        ConvertTerminationReason(grb_termination, has_feasible_solution));
    result.set_termination_reason(reason_and_detail.first);
    result.set_termination_detail(reason_and_detail.second);
  }
  if (GRBisattravailable(gurobi_model_, GRB_DBL_ATTR_OBJVAL)) {
    ASSIGN_OR_RETURN(const double obj_val, GetDoubleAttr(GRB_DBL_ATTR_OBJVAL));
    result.mutable_solve_stats()->set_best_primal_bound(obj_val);
  } else {
    result.mutable_solve_stats()->set_best_primal_bound(is_maximize ? -kInf
                                                                    : kInf);
  }
  if (GRBisattravailable(gurobi_model_, GRB_DBL_ATTR_OBJBOUND)) {
    ASSIGN_OR_RETURN(const double obj_bound,
                     GetDoubleAttr(GRB_DBL_ATTR_OBJBOUND));
    result.mutable_solve_stats()->set_best_dual_bound(obj_bound);
  } else {
    result.mutable_solve_stats()->set_best_dual_bound(is_maximize ? kInf
                                                                  : -kInf);
  }
  if (GRBisattravailable(gurobi_model_, GRB_DBL_ATTR_ITERCOUNT)) {
    ASSIGN_OR_RETURN(const double simplex_iters_double,
                     GetDoubleAttr(GRB_DBL_ATTR_ITERCOUNT));
    ASSIGN_OR_RETURN(const int64_t simplex_iters,
                     SafeInt64FromDouble(simplex_iters_double));
    result.mutable_solve_stats()->set_simplex_iterations(simplex_iters);
  }
  if (GRBisattravailable(gurobi_model_, GRB_INT_ATTR_BARITERCOUNT)) {
    ASSIGN_OR_RETURN(const int barrier_iters,
                     GetIntAttr(GRB_INT_ATTR_BARITERCOUNT));
    result.mutable_solve_stats()->set_barrier_iterations(barrier_iters);
  }
  if (GRBisattravailable(gurobi_model_, GRB_DBL_ATTR_NODECOUNT)) {
    ASSIGN_OR_RETURN(const double nodes_double,
                     GetDoubleAttr(GRB_DBL_ATTR_NODECOUNT));
    ASSIGN_OR_RETURN(const int64_t nodes, SafeInt64FromDouble(nodes_double));
    result.mutable_solve_stats()->set_node_count(nodes);
  }

  for (int i = 0; i < num_solutions; ++i) {
    PrimalSolutionProto* const primal_solution = result.add_primal_solutions();
    double sol_val;
    std::vector<double> grb_var_values(num_gurobi_variables_);
    // TODO(user): there seems to be some kind of issue with Gurobi where
    // GRB_DBL_ATTR_POOLOBJVAL is not always filled in when when there is
    // one, solution, probably a bug.
    if (i == 0) {
      ASSIGN_OR_RETURN(sol_val, GetDoubleAttr(GRB_DBL_ATTR_OBJVAL));
      RETURN_IF_ERROR(
          GetDoubleAttrArray(GRB_DBL_ATTR_X, absl::MakeSpan(grb_var_values)));
    } else {
      RETURN_IF_GUROBI_ERROR(
          GRBsetintparam(active_env_, GRB_INT_PAR_SOLUTIONNUMBER, i))
          << "Error setting solution to " << i;
      ASSIGN_OR_RETURN(sol_val, GetDoubleAttr(GRB_DBL_ATTR_POOLOBJVAL));
      RETURN_IF_ERROR(
          GetDoubleAttrArray(GRB_DBL_ATTR_XN, absl::MakeSpan(grb_var_values)));
    }
    primal_solution->set_objective_value(sol_val);
    GurobiVectorToSparseDoubleVector(
        grb_var_values, variables_map_,
        *primal_solution->mutable_variable_values(),
        model_parameters.primal_variables_filter());
  }
  // TODO(user): support getting infeasibility proofs, and basis.
  // If model is LP, get dual solutions
  // Note that we can ignore the reduced costs of the slack variables for ranged
  // constraints because of
  // go/mathopt-dev-transformations#slack-var-range-constraint
  ASSIGN_OR_RETURN(const bool is_lp, IsLP());
  if (is_lp && GRBisattravailable(gurobi_model_, GRB_DBL_ATTR_PI) &&
      GRBisattravailable(gurobi_model_, GRB_DBL_ATTR_RC)) {
    std::vector<double> grb_constraint_duals(num_gurobi_constraints());
    RETURN_IF_ERROR(GetDoubleAttrArray(GRB_DBL_ATTR_PI,
                                       absl::MakeSpan(grb_constraint_duals)));
    DualSolutionProto* const dual_solution = result.add_dual_solutions();
    ASSIGN_OR_RETURN(const double obj_val, GetDoubleAttr(GRB_DBL_ATTR_OBJVAL));
    dual_solution->set_objective_value(obj_val);
    GurobiVectorToSparseDoubleVector(
        grb_constraint_duals, linear_constraints_map_,
        *dual_solution->mutable_dual_values(),
        model_parameters.dual_linear_constraints_filter());

    std::vector<double> grb_reduced_cost_values(num_gurobi_variables_);
    RETURN_IF_ERROR(GetDoubleAttrArray(
        GRB_DBL_ATTR_RC, absl::MakeSpan(grb_reduced_cost_values)));
    GurobiVectorToSparseDoubleVector(grb_reduced_cost_values, variables_map_,
                                     *dual_solution->mutable_reduced_costs(),
                                     model_parameters.dual_variables_filter());
  }
  if (is_lp && GRBisattravailable(gurobi_model_, GRB_DBL_ATTR_UNBDRAY)) {
    std::vector<double> grb_ray_var_values(num_gurobi_variables_);
    RETURN_IF_ERROR(GetDoubleAttrArray(GRB_DBL_ATTR_UNBDRAY,
                                       absl::MakeSpan(grb_ray_var_values)));
    PrimalRayProto* const primal_ray = result.add_primal_rays();
    GurobiVectorToSparseDoubleVector(
        grb_ray_var_values, variables_map_,
        *primal_ray->mutable_variable_values(),
        model_parameters.primal_variables_filter());
  }
  if (is_lp && GRBisattravailable(gurobi_model_, GRB_DBL_ATTR_FARKASDUAL)) {
    ASSIGN_OR_RETURN(
        DualRayProto dual_ray,
        GetGurobiDualRay(model_parameters.dual_linear_constraints_filter(),
                         model_parameters.dual_variables_filter(),
                         is_maximize));
    result.mutable_dual_rays()->Add(std::move(dual_ray));
  }
  if (is_lp && GRBisattravailable(gurobi_model_, GRB_INT_ATTR_VBASIS) &&
      GRBisattravailable(gurobi_model_, GRB_INT_ATTR_CBASIS)) {
    ASSIGN_OR_RETURN(BasisProto basis, GetGurobiBasis());
    result.mutable_basis()->Add(std::move(basis));
  }
  return absl::OkStatus();
}

absl::Status GurobiSolver::SetParameter(const std::string& param_name,
                                        const std::string& param_value) {
  RETURN_IF_GUROBI_ERROR(
      GRBsetparam(active_env_, param_name.c_str(), param_value.c_str()))
      << "Error setting parameter: " << param_name << " to value "
      << param_value;
  return absl::OkStatus();
}

absl::Status GurobiSolver::ResetParameters() {
  RETURN_IF_GUROBI_ERROR(GRBresetparams(active_env_))
      << "Error reseting parameters";
  return absl::OkStatus();
}

std::vector<absl::Status> GurobiSolver::SetParameters(
    const SolveParametersProto& parameters) {
  CHECK(active_env_ != nullptr);
  const GurobiParametersProto gurobi_parameters = MergeParameters(
      parameters.common_parameters(), parameters.gurobi_parameters());
  std::vector<absl::Status> parameter_errors;
  for (const GurobiParametersProto::Parameter& parameter :
       gurobi_parameters.parameters()) {
    absl::Status param_status =
        SetParameter(parameter.name(), parameter.value());
    if (!param_status.ok()) {
      parameter_errors.emplace_back(std::move(param_status));
    }
  }
  return parameter_errors;
}

absl::Status GurobiSolver::AddNewVariables(
    const VariablesProto& new_variables) {
  CHECK(gurobi_model_ != nullptr);
  const int num_new_variables = new_variables.lower_bounds().size();

  std::vector<char> variable_type(num_new_variables);
  std::vector<char*> variable_names;
  const bool has_variable_names = !new_variables.names().empty();
  if (has_variable_names) {
    variable_names.reserve(num_new_variables);
  }
  for (int j = 0; j < num_new_variables; ++j) {
    const VariableId id = new_variables.ids(j);
    gtl::InsertOrDie(&variables_map_, id, j + num_gurobi_variables_);
    variable_type[j] = new_variables.integers(j) ? GRB_INTEGER : GRB_CONTINUOUS;
    if (has_variable_names) {
      variable_names.emplace_back(
          const_cast<char*>(new_variables.names(j).c_str()));
    }
  }

  RETURN_IF_GUROBI_ERROR(GRBaddvars(
      gurobi_model_, /*numvars=*/num_new_variables,
      /*numnz=*/0, /*vbeg=*/nullptr, /*vind=*/nullptr, /*vval=*/nullptr,
      /*obj=*/nullptr,
      /*lb=*/const_cast<double*>(new_variables.lower_bounds().data()),
      /*ub=*/const_cast<double*>(new_variables.upper_bounds().data()),
      /*vtype=*/variable_type.data(),
      /*varnames=*/has_variable_names ? variable_names.data() : nullptr));
  num_gurobi_variables_ += num_new_variables;

  return absl::OkStatus();
}

// Given a vector of pairs<LinearConstraintId,ConstraintData&> add a slack
// variable for each of the constraints in the underlying `gurobi_model_`
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
  std::vector<double> column_non_zeros(num_slacks, -1.0);
  std::vector<double> lower_bounds;
  std::vector<double> upper_bounds;
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
  RETURN_IF_GUROBI_ERROR(GRBaddvars(
      /*model=*/gurobi_model_, /*numvars=*/num_slacks, /*numnz=*/num_slacks,
      /*vbeg=*/column_non_zero_begin.data(), /*vind=*/row_indices.data(),
      /*vval=*/column_non_zeros.data(), /*obj=*/nullptr,
      /*lb=*/lower_bounds.data(), /*ub=*/upper_bounds.data(),
      /*vtype=*/nullptr, /*varnames=*/nullptr));
  num_gurobi_variables_ += num_slacks;
  return absl::OkStatus();
}

absl::Status GurobiSolver::AddNewConstraints(
    const LinearConstraintsProto& constraints) {
  CHECK(gurobi_model_ != nullptr);
  const int num_model_constraints = num_gurobi_constraints();
  const int num_new_constraints = constraints.lower_bounds().size();

  const bool has_constraint_names = !constraints.names().empty();
  std::vector<char*> constraint_names;
  if (has_constraint_names) {
    constraint_names.reserve(num_new_constraints);
  }
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
    if (has_constraint_names) {
      constraint_names.emplace_back(
          const_cast<char*>(constraints.names(i).c_str()));
    }
  }
  // Add all constraints in one call.
  RETURN_IF_GUROBI_ERROR(GRBaddconstrs(
      gurobi_model_, /*numconstrs=*/num_new_constraints,
      /*numnz=*/0, /*cbeg=*/nullptr, /*cind=*/nullptr, /*cval=*/nullptr,
      /*sense=*/constraint_sense.data(), /*rhs=*/constraint_rhs.data(),
      /*constrnames=*/
      has_constraint_names ? constraint_names.data() : nullptr));

  // Add slacks for true ranged constraints (if needed)
  if (!new_slacks.empty()) {
    RETURN_IF_ERROR(AddNewSlacks(new_slacks));
  }
  return absl::OkStatus();
}

absl::Status GurobiSolver::ChangeCoefficients(
    const SparseDoubleMatrixProto& matrix) {
  CHECK(gurobi_model_ != nullptr);
  const int num_coefficients = matrix.row_ids().size();
  std::vector<GurobiLinearConstraintIndex> row_index(num_coefficients);
  std::vector<GurobiVariableIndex> col_index(num_coefficients);
  for (int k = 0; k < num_coefficients; ++k) {
    row_index[k] =
        linear_constraints_map_.at(matrix.row_ids(k)).constraint_index;
    col_index[k] = variables_map_.at(matrix.column_ids(k));
  }
  double* const new_values = const_cast<double*>(matrix.coefficients().data());
  RETURN_IF_GUROBI_ERROR(
      GRBchgcoeffs(/*model=*/gurobi_model_,
                   /*cnt=*/num_coefficients, /*cind=*/row_index.data(),
                   /*vind=*/col_index.data(), /*val=*/new_values));
  return absl::OkStatus();
}

absl::Status GurobiSolver::UpdateDoubleListAttribute(
    const SparseDoubleVectorProto& update, const char* attribute_name,
    const IdHashMap& id_hash_map) {
  CHECK(gurobi_model_ != nullptr);
  const int sparse_length = update.ids().size();
  if (sparse_length == 0) {
    return absl::OkStatus();
  }

  std::vector<int> index(sparse_length);
  for (int k = 0; k < sparse_length; ++k) {
    index[k] = id_hash_map.at(update.ids(k));
  }

  double* const new_values = const_cast<double*>(update.values().data());
  RETURN_IF_GUROBI_ERROR(
      GRBsetdblattrlist(/*model=*/gurobi_model_, /*attrname=*/attribute_name,
                        /*len=*/sparse_length, /*ind=*/index.data(),
                        /*newvalues=*/new_values));
  return absl::OkStatus();
}

GurobiSolver::~GurobiSolver() {
  if (gurobi_model_ != nullptr) {
    std::string free_error = GurobiErrorMessage(GRBfreemodel(gurobi_model_));
    if (!free_error.empty()) {
      LOG(ERROR) << free_error;
    }
  }
  if (master_env_ != nullptr) {
    GRBfreeenv(master_env_);
  }
  active_env_ = nullptr;
  gurobi_model_ = nullptr;
  master_env_ = nullptr;
  VLOG(3) << "Freed unmanaged Gurobi pointers";
}

absl::Status GurobiSolver::LoadModel(const ModelProto& input_model) {
  CHECK(gurobi_model_ != nullptr);
  RETURN_IF_GUROBI_ERROR(GRBsetstrattr(gurobi_model_, GRB_STR_ATTR_MODELNAME,
                                       input_model.name().c_str()));
  RETURN_IF_ERROR(AddNewVariables(input_model.variables()));

  RETURN_IF_ERROR(AddNewConstraints(input_model.linear_constraints()));

  RETURN_IF_ERROR(ChangeCoefficients(input_model.linear_constraint_matrix()));

  const int model_sense =
      input_model.objective().maximize() ? GRB_MAXIMIZE : GRB_MINIMIZE;
  RETURN_IF_GUROBI_ERROR(
      GRBsetintattr(gurobi_model_, GRB_INT_ATTR_MODELSENSE, model_sense));
  RETURN_IF_GUROBI_ERROR(GRBsetdblattr(gurobi_model_, GRB_DBL_ATTR_OBJCON,
                                       input_model.objective().offset()));
  RETURN_IF_ERROR(
      UpdateDoubleListAttribute(input_model.objective().linear_coefficients(),
                                GRB_DBL_ATTR_OBJ, variables_map_));

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
    // the stored slack_index in linear_constraints_map_[id] and save the index
    // in the list of variables to be deleted later on.
    if (delete_slack && update_data.source.slack_index != kUnspecifiedIndex) {
      deleted_variables_index.emplace_back(update_data.source.slack_index);
      update_data.source.slack_index = kUnspecifiedIndex;
    }
  }

  // Pass down changes to Gurobi.
  if (!rhs_index.empty()) {
    RETURN_IF_GUROBI_ERROR(GRBsetdblattrlist(
        /*model=*/gurobi_model_, /*attrname=*/GRB_DBL_ATTR_RHS,
        /*len=*/rhs_index.size(), /*ind=*/rhs_index.data(),
        /*newvalues=*/rhs_data.data()));
    RETURN_IF_GUROBI_ERROR(GRBsetcharattrlist(
        /*model=*/gurobi_model_, /*attrname=*/GRB_CHAR_ATTR_SENSE,
        /*len=*/rhs_index.size(), /*ind=*/rhs_index.data(),
        /*newvalues=*/sense_data.data()));
  }  // rhs changes
  if (!bound_index.empty()) {
    RETURN_IF_GUROBI_ERROR(GRBsetdblattrlist(
        /*model=*/gurobi_model_, /*attrname=*/GRB_DBL_ATTR_LB,
        /*len=*/bound_index.size(), /*ind=*/bound_index.data(),
        /*newvalues=*/lower_bound_data.data()));
    RETURN_IF_GUROBI_ERROR(GRBsetdblattrlist(
        /*model=*/gurobi_model_, /*attrname=*/GRB_DBL_ATTR_UB,
        /*len=*/bound_index.size(), /*ind=*/bound_index.data(),
        /*newvalues=*/upper_bound_data.data()));
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
  CHECK(gurobi_model_ != nullptr);

  RETURN_IF_ERROR(AddNewVariables(model_update.new_variables()));

  RETURN_IF_ERROR(AddNewConstraints(model_update.new_linear_constraints()));

  RETURN_IF_ERROR(
      ChangeCoefficients(model_update.linear_constraint_matrix_updates()));

  if (model_update.objective_updates().has_direction_update()) {
    const int model_sense = model_update.objective_updates().direction_update()
                                ? GRB_MAXIMIZE
                                : GRB_MINIMIZE;
    RETURN_IF_GUROBI_ERROR(
        GRBsetintattr(gurobi_model_, GRB_INT_ATTR_MODELSENSE, model_sense));
  }

  if (model_update.objective_updates().has_offset_update()) {
    RETURN_IF_GUROBI_ERROR(
        GRBsetdblattr(gurobi_model_, GRB_DBL_ATTR_OBJCON,
                      model_update.objective_updates().offset_update()));
  }

  RETURN_IF_ERROR(UpdateDoubleListAttribute(
      model_update.objective_updates().linear_coefficients(), GRB_DBL_ATTR_OBJ,
      variables_map_));

  RETURN_IF_ERROR(
      UpdateDoubleListAttribute(model_update.variable_updates().lower_bounds(),
                                GRB_DBL_ATTR_LB, variables_map_));

  RETURN_IF_ERROR(
      UpdateDoubleListAttribute(model_update.variable_updates().upper_bounds(),
                                GRB_DBL_ATTR_UB, variables_map_));

  if (model_update.variable_updates().has_integers()) {
    const SparseBoolVectorProto& update =
        model_update.variable_updates().integers();
    const int sparse_length = update.ids().size();
    std::vector<GurobiVariableIndex> index(sparse_length);
    std::vector<char> value(sparse_length);
    for (int k = 0; k < sparse_length; ++k) {
      index[k] = variables_map_.at(update.ids(k));
      value[k] = update.values(k) ? GRB_INTEGER : GRB_CONTINUOUS;
    }
    RETURN_IF_GUROBI_ERROR(GRBsetcharattrlist(
        gurobi_model_, /*attrname=*/GRB_CHAR_ATTR_VTYPE, /*len=*/sparse_length,
        /*ind=*/index.data(), /*newvalues=*/value.data()));
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
    RETURN_IF_GUROBI_ERROR(
        GRBdelconstrs(/*model=*/gurobi_model_,
                      /*len=*/deleted_constraints_index.size(),
                      /*ind=*/deleted_constraints_index.data()));
  }

  if (!deleted_variables_index.empty()) {
    RETURN_IF_GUROBI_ERROR(GRBdelvars(/*model=*/gurobi_model_,
                                      /*len=*/deleted_variables_index.size(),
                                      /*ind=*/deleted_variables_index.data()));
    num_gurobi_variables_ -= deleted_variables_index.size();
  }

  // If we removed variables or constraints we must flush all pending changes
  // to synchronize the number of variables and constraints with the Gurobi
  // model.
  RETURN_IF_GUROBI_ERROR(GRBupdatemodel(gurobi_model_));
  // Regenerate indices.
  RETURN_IF_ERROR(UpdateGurobiIndices());

  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<GurobiSolver>> GurobiSolver::New(
    const ModelProto& input_model, const SolverInitializerProto& initializer) {
  if(!GurobiIsCorrectlyInstalled()) {
    return absl::InvalidArgumentError("Gurobi is not correctly installed.");
  }
  auto gurobi_data = absl::WrapUnique(new GurobiSolver);
  RETURN_IF_ERROR(gurobi_data->LoadEnvironment());
  RETURN_IF_ERROR(gurobi_data->LoadModel(input_model));
  return gurobi_data;
}

int GurobiSolver::GurobiCallback(GRBmodel* model, void* cbdata, int where,
                                 void* usrdata) {
  DCHECK_NE(usrdata, nullptr);
  DCHECK_NE(model, nullptr);
  auto gurobi_cb_data = static_cast<GurobiSolver::GurobiCallbackData*>(usrdata);
  // NOTE: if a previous callback failed, we never run the callback again. It
  // is the responsibility of the failing call to GurobiCallbackImpl to request
  // early termination.
  if (!gurobi_cb_data->status.ok()) {
    return kGrbOk;
  }
  gurobi_cb_data->status =
      GurobiCallbackImpl(model, cbdata, where, gurobi_cb_data->callback_input,
                         gurobi_cb_data->message_callback_data);
  if (!gurobi_cb_data->status.ok()) {
    return GRB_ERROR_CALLBACK;
  }
  return kGrbOk;
}

absl::StatusOr<std::unique_ptr<GurobiSolver::GurobiCallbackData>>
GurobiSolver::RegisterCallback(const CallbackRegistrationProto& registration,
                               Callback cb, const absl::Time start) {
  const absl::flat_hash_set<CallbackEventProto> events = EventSet(registration);
  // Set Gurobi parameters.
  if (events.contains(CALLBACK_EVENT_MESSAGE)) {
    // Disable logging messages to the console the user wants to handle
    // messages.
    RETURN_IF_ERROR(SetParameter(GRB_INT_PAR_LOGTOCONSOLE, "0"));
  }
  if (registration.add_cuts() || registration.add_lazy_constraints()) {
    // This is to signal the solver presolve to limit primal transformations
    // that precludes crushing cuts to the presolved model.
    RETURN_IF_ERROR(SetParameter(GRB_INT_PAR_PRECRUSH, "1"));
  }
  if (registration.add_lazy_constraints()) {
    // This is needed so that the solver knows that some presolve reductions
    // can not be performed safely.
    RETURN_IF_ERROR(SetParameter(GRB_INT_PAR_LAZYCONSTRAINTS, "1"));
  }
  return absl::make_unique<GurobiCallbackData>(GurobiCallbackInput{
      .user_cb = cb,
      .variable_ids = variables_map_,
      .num_gurobi_vars = num_gurobi_variables_,
      .events = EventToGurobiWhere(events),
      .mip_solution_filter = registration.mip_solution_filter(),
      .mip_node_filter = registration.mip_node_filter(),
      .start = start});
}

absl::StatusOr<SolveResultProto> GurobiSolver::Solve(
    const SolveParametersProto& parameters,
    const ModelSolveParametersProto& model_parameters,
    const CallbackRegistrationProto& callback_registration, const Callback cb) {
  const absl::Time start = absl::Now();
  SolveResultProto solve_result;
  // We must set the parameters before calling RegisterCallback since it changes
  // some parameters depending on the callback registration.
  const std::vector<absl::Status> param_errors = SetParameters(parameters);
  if (!param_errors.empty()) {
    if (parameters.common_parameters().strictness().bad_parameter()) {
      return absl::InvalidArgumentError(JoinErrors(param_errors));
    } else {
      for (const absl::Status& param_error : param_errors) {
        solve_result.add_warnings(std::string(param_error.message()));
      }
    }
  }

  // TODO(user): any warnings above will be lost if we get an error Status
  // below, reconsider the Solve API.
  std::unique_ptr<GurobiCallbackData> gurobi_cb_data;
  if (cb != nullptr) {
    ASSIGN_OR_RETURN(gurobi_cb_data,
                     RegisterCallback(callback_registration, cb, start));
    RETURN_IF_GUROBI_ERROR(GRBsetcallbackfunc(gurobi_model_, GurobiCallback,
                                              gurobi_cb_data.get()));
  }
  auto callback_cleanup = absl::MakeCleanup([&]() {
    if (cb != nullptr) {
      GRBsetcallbackfunc(gurobi_model_, nullptr, nullptr);
    }
  });

  // Need to run GRBupdatemodel before setting basis and getting the obj sense.
  CHECK(gurobi_model_ != nullptr);
  RETURN_IF_GUROBI_ERROR(GRBupdatemodel(gurobi_model_));

  ASSIGN_OR_RETURN(const int obj_sense, GetIntAttr(GRB_INT_ATTR_MODELSENSE));
  const bool is_maximize = obj_sense == GRB_MAXIMIZE;

  if (model_parameters.has_initial_basis()) {
    RETURN_IF_ERROR(SetGurobiBasis(model_parameters.initial_basis()));
  }

  const int gurobi_error = GRBoptimize(gurobi_model_);
  // Check for callback errors when GurobiSolver::GurobiCallback signals their
  // existence through GRB_ERROR_CALLBACK and when GRBterminate is called (in
  // which case gurobi_error could also be kGrbOk)
  if (gurobi_error == kGrbOk && gurobi_cb_data != nullptr &&
      gurobi_cb_data->status.ok()) {
    gurobi_cb_data->status = GurobiCallbackImplFlush(
        gurobi_cb_data->callback_input, gurobi_cb_data->message_callback_data);
  }
  if ((gurobi_error == GRB_ERROR_CALLBACK || gurobi_error == kGrbOk) &&
      gurobi_cb_data != nullptr) {
    RETURN_IF_ERROR(gurobi_cb_data->status) << "Error in callback";
  }
  RETURN_IF_GUROBI_ERROR(gurobi_error);
  if (cb != nullptr) {
    std::move(callback_cleanup).Cancel();
    RETURN_IF_GUROBI_ERROR(GRBsetcallbackfunc(gurobi_model_, nullptr, nullptr));
  }

  RETURN_IF_ERROR(
      ExtractSolveResultProto(is_maximize, solve_result, model_parameters));
  // Reset Gurobi parameters.
  // TODO(user): ensure that resetting parameters does not degrade
  // incrementalism performance.
  RETURN_IF_ERROR(ResetParameters());
  CHECK_OK(util_time::EncodeGoogleApiProto(
      absl::Now() - start,
      solve_result.mutable_solve_stats()->mutable_solve_time()));
  return solve_result;
}

bool GurobiSolver::CanUpdate(const ModelUpdateProto& model_update) {
  return true;
}

#undef RETURN_IF_GUROBI_ERROR

MATH_OPT_REGISTER_SOLVER(SOLVER_TYPE_GUROBI, GurobiSolver::New)

}  // namespace math_opt
}  // namespace operations_research
