// Copyright 2010-2022 Google LLC
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

#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/check.h"
#include "ortools/base/log.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/status_macros.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/proto_solver/sat_proto_solver.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/inverted_bounds.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/solve_interrupter.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/io/proto_converter.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/callback_validator.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace math_opt {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

constexpr SupportedProblemStructures kCpSatSupportedStructures = {
    .integer_variables = SupportType::kSupported,
    .quadratic_objectives = SupportType::kNotImplemented,
    .quadratic_constraints = SupportType::kNotImplemented,
    .sos1_constraints = SupportType::kNotImplemented,
    .sos2_constraints = SupportType::kNotImplemented,
    .indicator_constraints = SupportType::kNotImplemented};

// Returns true on success.
bool ApplyCutoff(const double cutoff, MPModelProto* model) {
  // TODO(b/204083726): we need to be careful here if we support quadratic
  // objectives
  if (model->has_quadratic_objective()) {
    return false;
  }
  // CP-SAT detects a constraint parallel to the objective and uses it as
  // an objective bound, which is the closest we can get to cutoff.
  // See FindDuplicateConstraints() in CP-SAT codebase.
  MPConstraintProto* const cutoff_constraint = model->add_constraint();
  for (int i = 0; i < model->variable_size(); ++i) {
    const double obj_coef = model->variable(i).objective_coefficient();
    if (obj_coef != 0) {
      cutoff_constraint->add_var_index(i);
      cutoff_constraint->add_coefficient(obj_coef);
    }
  }
  const double cutoff_minus_offset = cutoff - model->objective_offset();
  if (model->maximize()) {
    // Add the constraint obj >= cutoff
    cutoff_constraint->set_lower_bound(cutoff_minus_offset);
  } else {
    // Add the constraint obj <= cutoff
    cutoff_constraint->set_upper_bound(cutoff_minus_offset);
  }
  return true;
}

// Returns a list of warnings from parameter settings that were
// invalid/unsupported (specific to CP-SAT), one element per bad parameter.
std::vector<std::string> SetSolveParameters(
    const SolveParametersProto& parameters, const bool has_message_callback,
    MPModelRequest& request) {
  std::vector<std::string> warnings;
  if (parameters.has_time_limit()) {
    request.set_solver_time_limit_seconds(absl::ToDoubleSeconds(
        util_time::DecodeGoogleApiProto(parameters.time_limit()).value()));
  }
  if (parameters.has_node_limit()) {
    warnings.push_back("The node_limit parameter is not supported for CP-SAT.");
  }

  // Build CP SAT parameters by first initializing them from the common
  // parameters, and then using the values in `solver_specific_parameters` to
  // overwrite them if necessary.
  //
  // We don't need to set max_time_in_seconds since we already pass it in the
  // `request.solver_time_limit_seconds`. The logic of `SatSolveProto()` will
  // apply the logic we want here.
  sat::SatParameters sat_parameters;

  // By default CP-SAT catches SIGINT (Ctrl-C) to interrupt the solve but we
  // don't want this behavior when the users uses CP-SAT through MathOpt.
  sat_parameters.set_catch_sigint_signal(false);

  if (parameters.has_random_seed()) {
    sat_parameters.set_random_seed(parameters.random_seed());
  }
  if (parameters.has_threads()) {
    sat_parameters.set_num_search_workers(parameters.threads());
  }
  if (parameters.has_relative_gap_tolerance()) {
    sat_parameters.set_relative_gap_limit(parameters.relative_gap_tolerance());
  }

  if (parameters.has_absolute_gap_tolerance()) {
    sat_parameters.set_absolute_gap_limit(parameters.absolute_gap_tolerance());
  }
  // cutoff_limit is handled outside this function as it modifies the model.
  if (parameters.has_best_bound_limit()) {
    warnings.push_back(
        "The best_bound_limit parameter is not supported for CP-SAT.");
  }
  if (parameters.has_objective_limit()) {
    warnings.push_back(
        "The objective_limit parameter is not supported for CP-SAT.");
  }
  if (parameters.has_solution_limit()) {
    if (parameters.solution_limit() == 1) {
      sat_parameters.set_stop_after_first_solution(true);
    } else {
      warnings.push_back(absl::StrCat(
          "The CP-SAT solver only supports value 1 for solution_limit, found: ",
          parameters.solution_limit()));
    }
  }

  if (parameters.lp_algorithm() != LP_ALGORITHM_UNSPECIFIED) {
    warnings.push_back(
        absl::StrCat("Setting the LP Algorithm (was set to ",
                     ProtoEnumToString(parameters.lp_algorithm()),
                     ") is not supported for CP_SAT solver"));
  }
  if (parameters.presolve() != EMPHASIS_UNSPECIFIED) {
    switch (parameters.presolve()) {
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
                   << ProtoEnumToString(parameters.presolve())
                   << " unknown, error setting CP-SAT parameters";
    }
  }
  if (parameters.scaling() != EMPHASIS_UNSPECIFIED) {
    warnings.push_back(absl::StrCat("Setting the scaling (was set to ",
                                    ProtoEnumToString(parameters.scaling()),
                                    ") is not supported for CP_SAT solver"));
  }
  if (parameters.cuts() != EMPHASIS_UNSPECIFIED) {
    switch (parameters.cuts()) {
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
        LOG(FATAL) << "Cut emphasis: " << ProtoEnumToString(parameters.cuts())
                   << " unknown, error setting CP-SAT parameters";
    }
  }
  if (parameters.heuristics() != EMPHASIS_UNSPECIFIED) {
    warnings.push_back(absl::StrCat("Setting the heuristics (was set to ",
                                    ProtoEnumToString(parameters.heuristics()),
                                    ") is not supported for CP_SAT solver"));
  }
  sat_parameters.MergeFrom(parameters.cp_sat());

  // We want to override specifically SAT parameters independently from the user
  // input when a message callback is used to prevent wrongful writes to stdout
  // or disabling of messages via these parameters.
  if (has_message_callback) {
    // When enable_internal_solver_output is used, CP-SAT solver actually has
    // the same effect as setting log_search_progress to true.
    sat_parameters.set_log_search_progress(true);

    // Default value of log_to_stdout is true; but even if it was not the case,
    // we don't want to write to stdout when a message callback is used.
    sat_parameters.set_log_to_stdout(false);
  } else {
    // We only set enable_internal_solver_output when we have no message
    // callback.
    request.set_enable_internal_solver_output(parameters.enable_output());
  }

  request.set_solver_specific_parameters(
      EncodeSatParametersAsString(sat_parameters));
  return warnings;
}

absl::StatusOr<std::pair<SolveStatsProto, TerminationProto>>
GetTerminationAndStats(const bool is_interrupted, const bool maximize,
                       const bool used_cutoff,
                       const MPSolutionResponse& response) {
  SolveStatsProto solve_stats;
  TerminationProto termination;

  // Set default status and bounds.
  solve_stats.mutable_problem_status()->set_primal_status(
      FEASIBILITY_STATUS_UNDETERMINED);
  solve_stats.set_best_primal_bound(maximize ? -kInf : kInf);
  solve_stats.mutable_problem_status()->set_dual_status(
      FEASIBILITY_STATUS_UNDETERMINED);
  solve_stats.set_best_dual_bound(maximize ? kInf : -kInf);

  // Set terminations and update status and bounds as appropriate.
  switch (response.status()) {
    case MPSOLVER_OPTIMAL:
      termination =
          TerminateForReason(TERMINATION_REASON_OPTIMAL, response.status_str());
      solve_stats.mutable_problem_status()->set_primal_status(
          FEASIBILITY_STATUS_FEASIBLE);
      solve_stats.set_best_primal_bound(response.objective_value());
      solve_stats.mutable_problem_status()->set_dual_status(
          FEASIBILITY_STATUS_FEASIBLE);
      solve_stats.set_best_dual_bound(response.best_objective_bound());
      break;
    case MPSOLVER_INFEASIBLE:
      if (used_cutoff) {
        termination =
            NoSolutionFoundTermination(LIMIT_CUTOFF, response.status_str());
      } else {
        termination = TerminateForReason(TERMINATION_REASON_INFEASIBLE,
                                         response.status_str());
        solve_stats.mutable_problem_status()->set_primal_status(
            FEASIBILITY_STATUS_INFEASIBLE);
      }
      break;
    case MPSOLVER_UNKNOWN_STATUS:
      // For a basic unbounded problem, CP-SAT internally returns
      // INFEASIBLE_OR_UNBOUNDED after presolve but MPSolver statuses don't
      // support that thus it get transformed in MPSOLVER_UNKNOWN_STATUS with
      // a status_str of
      //
      //   "Problem proven infeasible or unbounded during MIP presolve"
      //
      // There may be some other cases where CP-SAT returns UNKNOWN here so we
      // only return TERMINATION_REASON_INFEASIBLE_OR_UNBOUNDED when the
      // status_str is detected. Otherwise we return OTHER_ERROR.
      //
      // TODO(b/202159173): A better solution would be to use CP-SAT API
      // directly which may help further improve the statuses.
      if (absl::StrContains(response.status_str(), "infeasible or unbounded")) {
        termination = TerminateForReason(
            TERMINATION_REASON_INFEASIBLE_OR_UNBOUNDED, response.status_str());
        solve_stats.mutable_problem_status()->set_primal_or_dual_infeasible(
            true);
      } else {
        termination = TerminateForReason(TERMINATION_REASON_OTHER_ERROR,
                                         response.status_str());
      }
      break;
    case MPSOLVER_FEASIBLE:
      termination = FeasibleTermination(
          is_interrupted ? LIMIT_INTERRUPTED : LIMIT_UNDETERMINED,
          response.status_str());
      solve_stats.mutable_problem_status()->set_primal_status(
          FEASIBILITY_STATUS_FEASIBLE);
      solve_stats.set_best_primal_bound(response.objective_value());
      solve_stats.set_best_dual_bound(response.best_objective_bound());
      if (std::isfinite(response.best_objective_bound())) {
        solve_stats.mutable_problem_status()->set_dual_status(
            FEASIBILITY_STATUS_FEASIBLE);
      }
      break;
    case MPSOLVER_NOT_SOLVED:
      termination = NoSolutionFoundTermination(
          is_interrupted ? LIMIT_INTERRUPTED : LIMIT_UNDETERMINED,
          response.status_str());
      break;
    case MPSOLVER_MODEL_INVALID:
      return absl::InternalError(
          absl::StrCat("cp-sat solver returned MODEL_INVALID, details: ",
                       response.status_str()));
    default:
      return absl::InternalError(
          absl::StrCat("unexpected solve status: ", response.status()));
  }
  return std::make_pair(std::move(solve_stats), std::move(termination));
}

}  // namespace

absl::StatusOr<std::unique_ptr<SolverInterface>> CpSatSolver::New(
    const ModelProto& model, const InitArgs& init_args) {
  RETURN_IF_ERROR(ModelIsSupported(model, kCpSatSupportedStructures, "CP-SAT"));
  ASSIGN_OR_RETURN(MPModelProto cp_sat_model,
                   MathOptModelToMPModelProto(model));
  std::vector variable_ids(model.variables().ids().begin(),
                           model.variables().ids().end());
  std::vector linear_constraint_ids(model.linear_constraints().ids().begin(),
                                    model.linear_constraints().ids().end());
  return absl::WrapUnique(new CpSatSolver(
      std::move(cp_sat_model),
      /*variable_ids=*/std::move(variable_ids),
      /*linear_constraint_ids=*/std::move(linear_constraint_ids)));
}

absl::StatusOr<SolveResultProto> CpSatSolver::Solve(
    const SolveParametersProto& parameters,
    const ModelSolveParametersProto& model_parameters,
    const MessageCallback message_cb,
    const CallbackRegistrationProto& callback_registration, const Callback cb,
    SolveInterrupter* const interrupter) {
  const absl::Time start = absl::Now();

  RETURN_IF_ERROR(CheckRegisteredCallbackEvents(
      callback_registration,
      /*supported_events=*/{CALLBACK_EVENT_MIP_SOLUTION}));
  if (callback_registration.add_lazy_constraints()) {
    return absl::InvalidArgumentError(
        "CallbackRegistrationProto.add_lazy_constraints=true is not supported "
        "for CP-SAT.");
  }
  // We need not check callback_registration.add_cuts, as cuts can only be added
  // at event MIP_NODE which we have already validated is not supported.

  SolveResultProto result;
  MPModelRequest req;
  // Here we must make a copy since Solve() can be called multiple times with
  // different parameters. Hence we can't move `cp_sat_model`.
  *req.mutable_model() = cp_sat_model_;

  req.set_solver_type(MPModelRequest::SAT_INTEGER_PROGRAMMING);
  bool used_cutoff = false;
  {
    std::vector<std::string> param_warnings =
        SetSolveParameters(parameters,
                           /*has_message_callback=*/message_cb != nullptr, req);
    if (parameters.has_cutoff_limit()) {
      used_cutoff = ApplyCutoff(parameters.cutoff_limit(), req.mutable_model());
      if (!used_cutoff) {
        param_warnings.push_back(
            "The cutoff_limit parameter not supported for quadratic objectives "
            "with CP-SAT.");
      }
    }
    if (!param_warnings.empty()) {
      return absl::InvalidArgumentError(absl::StrJoin(param_warnings, "; "));
    }
  }

  if (!model_parameters.solution_hints().empty()) {
    int i = 0;
    for (const auto [id, val] :
         MakeView(model_parameters.solution_hints(0).variable_values())) {
      while (variable_ids_[i] < id) {
        ++i;
      }
      req.mutable_model()->mutable_solution_hint()->add_var_index(i);
      req.mutable_model()->mutable_solution_hint()->add_var_value(val);
    }
  }

  // We need to chain the user interrupter through a local interrupter, because
  // if we termiante early from a callback request, we don't want to incorrectly
  // modify the input state.
  SolveInterrupter local_interrupter;
  std::atomic<bool> interrupt_solve = false;
  local_interrupter.AddInterruptionCallback([&]() { interrupt_solve = true; });

  // Setup a callback on the user provided so that we interrupt the solver.
  const ScopedSolveInterrupterCallback scoped_interrupt_cb(
      interrupter, [&]() { local_interrupter.Interrupt(); });

  std::function<void(const std::string&)> logging_callback;
  if (message_cb != nullptr) {
    logging_callback = [&](const std::string& message) {
      message_cb(absl::StrSplit(message, '\n'));
    };
  }

  const absl::flat_hash_set<CallbackEventProto> events =
      EventSet(callback_registration);
  std::function<void(const MPSolution&)> solution_callback;
  absl::Status callback_error = absl::OkStatus();
  if (events.contains(CALLBACK_EVENT_MIP_SOLUTION)) {
    solution_callback = [this, &cb, &callback_error, &local_interrupter,
                         &model_parameters](const MPSolution& mp_solution) {
      if (!callback_error.ok()) {
        // A previous callback failed.
        return;
      }
      CallbackDataProto cb_data;
      cb_data.set_event(CALLBACK_EVENT_MIP_SOLUTION);
      *cb_data.mutable_primal_solution_vector() =
          ExtractSolution(mp_solution.variable_value(), model_parameters);
      const absl::StatusOr<CallbackResultProto> cb_result = cb(cb_data);
      if (!cb_result.ok()) {
        callback_error = cb_result.status();
        // Note: we will be returning a status error, we do not need to worry
        // about interpreting this as TERMINATION_REASON_INTERRUPTED.
        local_interrupter.Interrupt();
      } else if (cb_result->terminate()) {
        local_interrupter.Interrupt();
      }
      // Note cb_result.cuts and cb_result.suggested solutions are not
      // supported by CP-SAT and we have validated they are empty.
    };
  }

  // CP-SAT returns "infeasible" for inverted bounds.
  RETURN_IF_ERROR(ListInvertedBounds().ToStatus());

  ASSIGN_OR_RETURN(const MPSolutionResponse response,
                   SatSolveProto(std::move(req), &interrupt_solve,
                                 logging_callback, solution_callback));
  RETURN_IF_ERROR(callback_error) << "error in callback";
  ASSIGN_OR_RETURN(
      (auto [solve_stats, termination]),
      GetTerminationAndStats(local_interrupter.IsInterrupted(),
                             /*maximize=*/cp_sat_model_.maximize(),
                             /*used_cutoff=*/used_cutoff, response));
  *result.mutable_solve_stats() = std::move(solve_stats);
  *result.mutable_termination() = std::move(termination);
  if (response.status() == MPSOLVER_OPTIMAL ||
      response.status() == MPSOLVER_FEASIBLE) {
    PrimalSolutionProto& solution =
        *result.add_solutions()->mutable_primal_solution();
    *solution.mutable_variable_values() =
        ExtractSolution(response.variable_value(), model_parameters);
    solution.set_objective_value(response.objective_value());
    solution.set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
  }

  CHECK_OK(util_time::EncodeGoogleApiProto(
      absl::Now() - start, result.mutable_solve_stats()->mutable_solve_time()));

  return result;
}

absl::StatusOr<bool> CpSatSolver::Update(const ModelUpdateProto& model_update) {
  return false;
}

CpSatSolver::CpSatSolver(MPModelProto cp_sat_model,
                         std::vector<int64_t> variable_ids,
                         std::vector<int64_t> linear_constraint_ids)
    : cp_sat_model_(std::move(cp_sat_model)),
      variable_ids_(std::move(variable_ids)),
      linear_constraint_ids_(std::move(linear_constraint_ids)) {}

SparseDoubleVectorProto CpSatSolver::ExtractSolution(
    const absl::Span<const double> cp_sat_variable_values,
    const ModelSolveParametersProto& model_parameters) const {
  // Pre-condition: we assume one-to-one correspondence of input variables to
  // solution's variables.
  CHECK_EQ(cp_sat_variable_values.size(), variable_ids_.size());

  SparseVectorFilterPredicate predicate(
      model_parameters.variable_values_filter());
  SparseDoubleVectorProto result;
  for (int i = 0; i < variable_ids_.size(); ++i) {
    const int64_t id = variable_ids_[i];
    const double value = cp_sat_variable_values[i];
    if (predicate.AcceptsAndUpdate(id, value)) {
      result.add_ids(id);
      result.add_values(value);
    }
  }
  return result;
}

InvertedBounds CpSatSolver::ListInvertedBounds() const {
  InvertedBounds inverted_bounds;
  for (int v = 0; v < cp_sat_model_.variable_size(); ++v) {
    const MPVariableProto& var = cp_sat_model_.variable(v);
    if (var.lower_bound() > var.upper_bound()) {
      inverted_bounds.variables.push_back(variable_ids_[v]);
    }
  }
  for (int c = 0; c < cp_sat_model_.constraint_size(); ++c) {
    const MPConstraintProto& cstr = cp_sat_model_.constraint(c);
    if (cstr.lower_bound() > cstr.upper_bound()) {
      inverted_bounds.linear_constraints.push_back(linear_constraint_ids_[c]);
    }
  }

  return inverted_bounds;
}

MATH_OPT_REGISTER_SOLVER(SOLVER_TYPE_CP_SAT, CpSatSolver::New);

}  // namespace math_opt
}  // namespace operations_research
