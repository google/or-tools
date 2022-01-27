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

#include "ortools/math_opt/solvers/cp_sat_solver.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/time/time.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/sat_proto_solver.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/io/proto_converter.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/base/status_macros.h"
#include "ortools/base/protoutil.h"

namespace operations_research {
namespace math_opt {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

void SetTrivialBounds(const bool maximize, SolveStatsProto& stats) {
  stats.set_best_primal_bound(maximize ? -kInf : kInf);
  stats.set_best_dual_bound(maximize ? kInf : -kInf);
}

// Returns a list of warnings from parameter settings that were
// invalid/unsupported (specific to CP-SAT), one element per bad parameter.
std::vector<std::string> SetSolveParameters(
    const SolveParametersProto& parameters, MPModelRequest& request) {
  std::vector<std::string> warnings;
  const CommonSolveParametersProto& common_parameters =
      parameters.common_parameters();
  if (common_parameters.has_time_limit()) {
    request.set_solver_time_limit_seconds(absl::ToDoubleSeconds(
        util_time::DecodeGoogleApiProto(common_parameters.time_limit())
            .value()));
  }
  if (common_parameters.has_enable_output()) {
    request.set_enable_internal_solver_output(
        common_parameters.enable_output());
  }

  // Build CP SAT parameters by first initializing them from the common
  // parameters, and then using the values in `solver_specific_parameters` to
  // overwrite them if necessary.
  //
  // We don't need to set max_time_in_seconds since we already pass it in the
  // `request.solver_time_limit_seconds`. The logic of `SatSolveProto()` will
  // apply the logic we want here.
  sat::SatParameters sat_parameters;
  if (common_parameters.has_random_seed()) {
    sat_parameters.set_random_seed(common_parameters.random_seed());
  }
  if (common_parameters.has_threads()) {
    sat_parameters.set_num_search_workers(common_parameters.threads());
  }
  if (common_parameters.lp_algorithm() != LP_ALGORITHM_UNSPECIFIED) {
    warnings.push_back(
        absl::StrCat("Setting the LP Algorithm (was set to ",
                     ProtoEnumToString(common_parameters.lp_algorithm()),
                     ") is not supported for CP_SAT solver"));
  }
  if (common_parameters.presolve() != EMPHASIS_UNSPECIFIED) {
    switch (common_parameters.presolve()) {
      case EMPHASIS_OFF:
        sat_parameters.set_cp_model_presolve(false);
        break;
      case EMPHASIS_LOW:
      case EMPHASIS_MEDIUM:
      case EMPHASIS_HIGH:
      case EMPHASIS_VERY_HIGH:
        sat_parameters.set_cp_model_presolve(true);
        break;
      default:
        LOG(FATAL) << "Presolve emphasis: "
                   << ProtoEnumToString(common_parameters.presolve())
                   << " unknown, error setting CP-SAT parameters";
    }
  }
  if (common_parameters.scaling() != EMPHASIS_UNSPECIFIED) {
    warnings.push_back(
        absl::StrCat("Setting the scaling (was set to ",
                     ProtoEnumToString(common_parameters.scaling()),
                     ") is not supported for CP_SAT solver"));
  }
  if (common_parameters.cuts() != EMPHASIS_UNSPECIFIED) {
    switch (common_parameters.cuts()) {
      case EMPHASIS_OFF:
        // This is not very maintainable, but CP-SAT doesn't expose the
        // parameters we need.
        sat_parameters.set_add_cg_cuts(false);
        sat_parameters.set_add_mir_cuts(false);
        sat_parameters.set_add_zero_half_cuts(false);
        sat_parameters.set_add_clique_cuts(false);
        sat_parameters.set_max_all_diff_cut_size(0);
        sat_parameters.set_add_lin_max_cuts(false);
        break;
      case EMPHASIS_LOW:
      case EMPHASIS_MEDIUM:
      case EMPHASIS_HIGH:
      case EMPHASIS_VERY_HIGH:
        break;
      default:
        LOG(FATAL) << "Cut emphasis: "
                   << ProtoEnumToString(common_parameters.cuts())
                   << " unknown, error setting CP-SAT parameters";
    }
  }
  if (common_parameters.heuristics() != EMPHASIS_UNSPECIFIED) {
    warnings.push_back(
        absl::StrCat("Setting the heuristics (was set to ",
                     ProtoEnumToString(common_parameters.heuristics()),
                     ") is not supported for CP_SAT solver"));
  }
  sat_parameters.MergeFrom(parameters.cp_sat_parameters());
  request.set_solver_specific_parameters(
      EncodeSatParametersAsString(sat_parameters));
  return warnings;
}

}  // namespace

absl::StatusOr<std::unique_ptr<SolverInterface>> CpSatSolver::New(
    const ModelProto& model, const SolverInitializerProto& initializer) {
  ASSIGN_OR_RETURN(MPModelProto cp_sat_model,
                   MathOptModelToMPModelProto(model));
  std::vector variable_ids(model.variables().ids().begin(),
                           model.variables().ids().end());
  // We must use WrapUnique here since the constructor is private.
  return absl::WrapUnique(
      new CpSatSolver(std::move(cp_sat_model), std::move(variable_ids)));
}

absl::StatusOr<SolveResultProto> CpSatSolver::Solve(
    const SolveParametersProto& parameters,
    const ModelSolveParametersProto& model_parameters,
    const CallbackRegistrationProto& callback_registration, const Callback cb) {
  SolveResultProto result;
  MPModelRequest req;
  // Here we must make a copy since Solve() can be called multiple times with
  // different parameters. Hence we can't move `cp_sat_model`.
  *req.mutable_model() = cp_sat_model_;
  req.set_solver_type(MPModelRequest::SAT_INTEGER_PROGRAMMING);
  {
    std::vector<std::string> param_warnings =
        SetSolveParameters(parameters, req);
    if (!param_warnings.empty()) {
      if (parameters.common_parameters().strictness().bad_parameter()) {
        return absl::InvalidArgumentError(absl::StrJoin(param_warnings, "; "));
      } else {
        for (std::string& warning : param_warnings) {
          result.add_warnings(std::move(warning));
        }
      }
    }
  }

  // The `response` is not const to be able to move out the solution values.
  ASSIGN_OR_RETURN(const MPSolutionResponse response,
                   SatSolveProto(std::move(req)));

  switch (response.status()) {
    case MPSOLVER_FEASIBLE:
    case MPSOLVER_OPTIMAL: {
      result.set_termination_reason(response.status() == MPSOLVER_OPTIMAL
                                        ? SolveResultProto::OPTIMAL
                                        : SolveResultProto::OTHER_LIMIT);
      result.set_termination_detail(response.status_str());
      result.mutable_solve_stats()->set_best_primal_bound(
          response.objective_value());
      result.mutable_solve_stats()->set_best_dual_bound(
          response.best_objective_bound());
      *result.add_primal_solutions() =
          ExtractSolution(response, model_parameters);
      break;
    }
    case MPSOLVER_INFEASIBLE:
      result.set_termination_reason(SolveResultProto::INFEASIBLE);
      result.set_termination_detail(response.status_str());
      SetTrivialBounds(cp_sat_model_.maximize(), *result.mutable_solve_stats());
      break;
    case MPSOLVER_NOT_SOLVED:
      result.set_termination_reason(SolveResultProto::OTHER_LIMIT);
      result.set_termination_detail(response.status_str());
      SetTrivialBounds(cp_sat_model_.maximize(), *result.mutable_solve_stats());
      break;
    case MPSOLVER_MODEL_INVALID:
      return absl::InternalError(
          absl::StrCat("cp-sat solver returned MODEL_INVALID, details: ",
                       response.status_str()));
    default:
      return absl::InternalError(
          absl::StrCat("unexpected solve status: ", response.status()));
  }

  return result;
}

bool CpSatSolver::CanUpdate(const ModelUpdateProto& model_update) {
  return false;
}

absl::Status CpSatSolver::Update(const ModelUpdateProto& model_update) {
  // This function should never be called since CanUpdate() always returns
  // false.
  return absl::InternalError("CP-SAT solver does not support incrementalism");
}

CpSatSolver::CpSatSolver(MPModelProto cp_sat_model,
                         std::vector<int64_t> variable_ids)
    : cp_sat_model_(std::move(cp_sat_model)),
      variable_ids_(std::move(variable_ids)) {}

PrimalSolutionProto CpSatSolver::ExtractSolution(
    const MPSolutionResponse& response,
    const ModelSolveParametersProto& model_parameters) const {
  PrimalSolutionProto solution;

  solution.set_objective_value(response.objective_value());

  // Pre-condition: we assume one-to-one correspondence of input variables to
  // solution's variables.
  CHECK_EQ(response.variable_value_size(), variable_ids_.size());

  SparseVectorFilterPredicate predicate(
      model_parameters.primal_variables_filter());
  auto* const values = solution.mutable_variable_values();
  for (int i = 0; i < variable_ids_.size(); ++i) {
    const int64_t id = variable_ids_[i];
    const double value = response.variable_value(i);
    if (predicate.AcceptsAndUpdate(id, value)) {
      values->add_ids(id);
      values->add_values(value);
    }
  }

  return solution;
}

MATH_OPT_REGISTER_SOLVER(SOLVER_TYPE_CP_SAT, CpSatSolver::New);

}  // namespace math_opt
}  // namespace operations_research
