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

#include "ortools/linear_solver/proto_solver/sat_proto_solver.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/preprocessor.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/model_validator.h"
#include "ortools/linear_solver/proto_solver/preprocessor.h"
#include "ortools/linear_solver/proto_solver/proto_utils.h"
#include "ortools/linear_solver/proto_solver/sat_solver_utils.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/lp_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/mp_model_wrapper.h"
#include "ortools/sat/parameters_validation.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/lazy_mutable_copy.h"
#include "ortools/util/logging.h"
#include "ortools/util/mp_model_solution_checker.h"
#include "ortools/util/time_limit.h"

namespace operations_research {

namespace {

MPSolverResponseStatus ToMPSolverResponseStatus(sat::CpSolverStatus status,
                                                bool has_objective) {
  switch (status) {
    case sat::CpSolverStatus::UNKNOWN:
      return MPSOLVER_NOT_SOLVED;
    case sat::CpSolverStatus::MODEL_INVALID:
      return MPSOLVER_MODEL_INVALID;
    case sat::CpSolverStatus::FEASIBLE:
      return MPSOLVER_FEASIBLE;
    case sat::CpSolverStatus::INFEASIBLE:
      return MPSOLVER_INFEASIBLE;
    case sat::CpSolverStatus::OPTIMAL:
      return MPSOLVER_OPTIMAL;
    default: {
    }
  }
  return MPSOLVER_ABNORMAL;
}

sat::CpSolverStatus FromMPSolverResponseStatus(MPSolverResponseStatus status) {
  switch (status) {
    case MPSolverResponseStatus::MPSOLVER_OPTIMAL:
      return sat::OPTIMAL;
    case MPSolverResponseStatus::MPSOLVER_INFEASIBLE:
      return sat::INFEASIBLE;
    case MPSolverResponseStatus::MPSOLVER_MODEL_INVALID:
      return sat::MODEL_INVALID;
    default: {
    }
  }
  return sat::UNKNOWN;
}

MPSolutionResponse InfeasibleResponse(SolverLogger& logger,
                                      std::string message) {
  SOLVER_LOG(&logger, "Infeasible model detected in sat_solve_proto.\n",
             message);

  // This is needed for our benchmark scripts.
  if (logger.LoggingIsEnabled()) {
    sat::CpSolverResponse cp_response;
    cp_response.set_status(sat::CpSolverStatus::INFEASIBLE);
    SOLVER_LOG(&logger, CpSolverResponseStats(cp_response));
  }

  MPSolutionResponse response;
  response.set_status(MPSolverResponseStatus::MPSOLVER_INFEASIBLE);
  response.set_status_str(message);
  return response;
}

MPSolutionResponse InvalidModelResponse(SolverLogger& logger,
                                        std::string message) {
  SOLVER_LOG(&logger, "Invalid model in sat_solve_proto.\n", message);

  // This is needed for our benchmark scripts.
  if (logger.LoggingIsEnabled()) {
    sat::CpSolverResponse cp_response;
    cp_response.set_status(sat::CpSolverStatus::MODEL_INVALID);
    SOLVER_LOG(&logger, CpSolverResponseStats(cp_response));
  }

  MPSolutionResponse response;
  response.set_status(MPSolverResponseStatus::MPSOLVER_MODEL_INVALID);
  response.set_status_str(message);
  return response;
}

MPSolutionResponse InvalidParametersResponse(SolverLogger& logger,
                                             std::string message) {
  SOLVER_LOG(&logger, "Invalid parameters in sat_solve_proto.\n", message);

  // This is needed for our benchmark scripts.
  if (logger.LoggingIsEnabled()) {
    sat::CpSolverResponse cp_response;
    cp_response.set_status(sat::CpSolverStatus::MODEL_INVALID);
    SOLVER_LOG(&logger, CpSolverResponseStats(cp_response));
  }

  MPSolutionResponse response;
  response.set_status(
      MPSolverResponseStatus::MPSOLVER_MODEL_INVALID_SOLVER_PARAMETERS);
  response.set_status_str(message);
  return response;
}

MPSolutionResponse TimeLimitResponse(SolverLogger& logger) {
  SOLVER_LOG(&logger, "Time limit reached in sat_solve_proto.\n");

  // This is needed for our benchmark scripts.
  if (logger.LoggingIsEnabled()) {
    sat::CpSolverResponse cp_response;
    cp_response.set_status(sat::CpSolverStatus::UNKNOWN);
    SOLVER_LOG(&logger, CpSolverResponseStats(cp_response));
  }

  MPSolutionResponse response;
  response.set_status(MPSolverResponseStatus::MPSOLVER_NOT_SOLVED);
  response.set_status_str("Time limit reached in sat_solve_proto.");
  return response;
}

// Splits the residual continuous variables and constraints into disjoint
// components (plus pure-integer constraints) and runs the TUM detection,
// shifting, scaling and conversion stack per component. The wrapper is not
// modified, all the per-variable transformations are recorded in `states`
// instead (see sat::MipVariableState). Returns an error response if the
// conversion failed.
std::optional<MPSolutionResponse> ConvertComponentsToCpModelProto(
    const sat::SatParameters& params, int num_clamped,
    const sat::MPModelProtoWrapper& mp_model_wrapper,
    std::vector<sat::MipVariableState>* states, sat::CpModelProto* cp_model,
    SolverLogger* logger) {
  const int num_variables = mp_model_wrapper.num_variables();
  const MPModelProto& mp_model = mp_model_wrapper.underlying_model();
  sat::ResidualContinuousComponents components =
      sat::SplitIntoResidualContinuousComponents(mp_model_wrapper);

  cp_model->Clear();
  cp_model->set_name(mp_model_wrapper.name());
  const bool keep_names = !params.ignore_names();
  for (int var = 0; var < num_variables; ++var) {
    sat::IntegerVariableProto* cp_var = cp_model->add_variables();
    if (keep_names) cp_var->set_name(mp_model_wrapper.variables(var).name());
  }

  sat::ConstraintScaler scaler;
  scaler.wanted_precision = params.mip_wanted_precision();
  scaler.use_tight_bounds = params.mip_use_tight_bounds();
  scaler.use_proper_relaxation = params.mip_proper_relaxation();
  scaler.tight_precision = params.mip_tight_precision();
  scaler.scaling_target = int64_t{1} << params.mip_max_activity_exponent();
  scaler.keep_names = keep_names;

  int num_small_domains = 0;
  int64_t num_int_bound_changes = 0;
  sat::ShiftStats before_scaling_shift_stats;
  sat::ShiftStats final_shift_stats;
  sat::OneVariableComponentStats one_var_stats;
  sat::TumStats tum_stats;
  sat::PrecisionScalingStats precision_scaling_stats;
  int num_extra_clamp = 0;

  const auto final_shift = [&params](const sat::MipVariableState& state) {
    return FinalCpSatShiftValue(state.lb, state.ub, state.is_integer,
                                params.mip_wanted_precision());
  };
  const auto shift_before_scaling = [](const sat::MipVariableState& state) {
    return ShiftBeforeScalingValue(state.lb, state.ub, state.is_integer);
  };
  const auto convert_variables = [&](absl::Span<const int> vars) {
    for (const int var : vars) {
      if (!sat::ConvertVariableToCpVariable(
              params, var, mp_model.variable(var).objective_coefficient(),
              &(*states)[var], cp_model->mutable_variables(var),
              &num_small_domains)) {
        return false;
      }
    }
    return true;
  };
  // `modular` is either empty or gives the scaling of each constraint, see
  // MarkComponentTotallyUnimodularAsInteger().
  const auto convert_constraints =
      [&](absl::Span<const int> constraints,
          absl::Span<const sat::ModularConstraintScaling> modular = {}) {
        for (int i = 0; i < constraints.size(); ++i) {
          const absl::Status status = scaler.ScaleAndAddConstraint(
              mp_model_wrapper.constraints(constraints[i]), *states, cp_model,
              modular.empty() ? std::nullopt : std::make_optional(modular[i]));
          if (!status.ok()) {
            SOLVER_LOG(logger, status.message());
            return false;
          }
        }
        return true;
      };

  // Finalize the variables that are already integer upfront since the
  // constraints of the components may reference them.
  std::vector<int> integer_vars;
  for (int var = 0; var < num_variables; ++var) {
    if ((*states)[var].is_integer) integer_vars.push_back(var);
  }
  if (!sat::MakeBoundsOfIntegerVariablesInteger(
          params, integer_vars, absl::MakeSpan(*states), &num_int_bound_changes,
          logger)) {
    return InfeasibleResponse(*logger,
                              "An integer variable has an empty domain");
  }
  sat::ShiftVariables(final_shift, integer_vars, absl::MakeSpan(*states),
                      &final_shift_stats);
  if (!convert_variables(integer_vars)) {
    return InvalidModelResponse(*logger,
                                "Failed to convert model into CP-SAT model");
  }

  std::optional<sat::GlobalObjectivePrecision> global_obj_precision;
  if (params.mip_scale_for_precision()) {
    global_obj_precision.emplace(params, mp_model);
  }
  std::vector<int> var_to_local_col_buffer(num_variables, -1);
  for (sat::ResidualContinuousComponent& compo : components.components) {
    const sat::ScopedVarToLocalCol scoped(&compo, &var_to_local_col_buffer);
    bool compo_has_obj_var = false;
    for (const int var : compo.continuous_vars) {
      if (mp_model.variable(var).objective_coefficient() != 0.0) {
        compo_has_obj_var = true;
        break;
      }
    }
    const auto mark_objective_dirty_if_changed = [&](int64_t before,
                                                     int64_t after) {
      if (global_obj_precision.has_value() && compo_has_obj_var &&
          after > before) {
        global_obj_precision->MarkDirty();
      }
    };

    // Detects if at optimality some variables could always be made integer.
    // Ignoring constraint enforcement is still valid after
    // ExtractEnforcementLiterals() because all extracted enforcement variables
    // are integer and therefore external here.
    std::vector<sat::ModularConstraintScaling> modular_scalings;
    if (compo.continuous_vars.size() == 1) {
      sat::ScaleOneVariableResidualMatrix(
          params, compo, mp_model_wrapper, absl::MakeSpan(*states),
          global_obj_precision.has_value() ? &*global_obj_precision : nullptr,
          &one_var_stats);
    } else {
      modular_scalings = sat::MarkComponentTotallyUnimodularAsInteger(
          params, compo, mp_model_wrapper, absl::MakeSpan(*states),
          global_obj_precision.has_value() ? &*global_obj_precision : nullptr,
          &tum_stats);
    }

    // Same as MakeBoundsInteger() for the newly detected integers.
    {
      const int64_t before = num_int_bound_changes;
      if (!sat::MakeBoundsOfIntegerVariablesInteger(
              params, compo.continuous_vars, absl::MakeSpan(*states),
              &num_int_bound_changes, logger)) {
        return InfeasibleResponse(
            *logger, "A detected integer variable has an empty domain");
      }
      mark_objective_dirty_if_changed(before, num_int_bound_changes);
    }

    bool compo_has_continuous = false;
    for (const int var : compo.continuous_vars) {
      if (!(*states)[var].is_integer) compo_has_continuous = true;
    }

    // Abort if one only wants to solve pure-IP model and we don't have one.
    if (params.only_solve_ip() && compo_has_continuous) {
      sat::DisplayVariableStatistics(*states, logger);
      return InvalidModelResponse(
          *logger,
          "The model contains non-integer variables but the parameter "
          "'only_solve_ip' was set. Change this parameter if you "
          "still want to solve a more constrained version of the original MIP "
          "where non-integer variables can only take a finite set of values.");
    }

    if (params.mip_scale_for_precision()) {
      const int64_t before = before_scaling_shift_stats.num_shifted;
      sat::ShiftVariables(shift_before_scaling, compo.continuous_vars,
                          absl::MakeSpan(*states), &before_scaling_shift_stats);
      mark_objective_dirty_if_changed(before,
                                      before_scaling_shift_stats.num_shifted);
    }
    if (compo_has_continuous) {
      if (params.mip_scale_for_precision()) {
        sat::ScaleComponentContinuousVariablesForGivenPrecision(
            params, compo, mp_model_wrapper, absl::MakeSpan(*states),
            &*global_obj_precision, &precision_scaling_stats);
      } else if (params.mip_var_scaling() != 1.0) {
        sat::ScaleComponentContinuousVariables(params, compo,
                                               absl::MakeSpan(*states));

        // Clamp again now that we potentially scaled more some variable.
        if (!sat::ClampVariableBounds(params, compo.continuous_vars,
                                      absl::MakeSpan(*states), logger,
                                      &num_extra_clamp)) {
          return InvalidModelResponse(
              *logger, "Failed to convert model into CP-SAT model");
        }
      }
    }

    // Final shift before the conversion.
    {
      const int64_t before = final_shift_stats.num_shifted;
      sat::ShiftVariables(final_shift, compo.continuous_vars,
                          absl::MakeSpan(*states), &final_shift_stats);
      mark_objective_dirty_if_changed(before, final_shift_stats.num_shifted);
    }

    if (!convert_variables(compo.continuous_vars) ||
        !convert_constraints(compo.constraints, modular_scalings)) {
      return InvalidModelResponse(*logger,
                                  "Failed to convert model into CP-SAT model");
    }
  }

  // Convert the pure-integer constraints and the objective.
  if (!convert_constraints(components.pure_integer_constraints)) {
    return InvalidModelResponse(*logger,
                                "Failed to convert model into CP-SAT model");
  }
  sat::PopulateCpModelFloatingPointObjective(mp_model_wrapper, *states,
                                             cp_model);

  // Display all the stats.
  sat::LogOneVariableComponentStats(one_var_stats, logger);
  sat::LogTumStats(tum_stats, logger);
  if (num_int_bound_changes > 0) {
    SOLVER_LOG(logger, "[IntegerBound] Snapped ", num_int_bound_changes,
               " bound of an integer variable to an exact integer.");
  }
  sat::LogShiftStats(before_scaling_shift_stats, logger);
  if (params.mip_scale_for_precision()) {
    sat::LogPrecisionScalingStats(params, mp_model, *states,
                                  precision_scaling_stats, logger);
  } else if (num_extra_clamp > 0) {
    SOLVER_LOG(logger, "[Clamping] Warning: ", FormatCounter(num_extra_clamp),
               " bounds were truncated to ",
               static_cast<int64_t>(std::round(params.mip_max_bound())), ".");
  }
  sat::DisplayVariableStatistics(*states, logger);
  sat::LogShiftStats(final_shift_stats, logger);
  sat::DisplayConstraintScalingStats(
      params, scaler, num_clamped + num_extra_clamp, num_small_domains, logger);
  return std::nullopt;
}

}  // namespace

MPSolutionResponse SatSolveProtoInternal(
    LazyMutableCopy<MPModelRequest> request, sat::Model* sat_model,
    sat::CpSolverResponse* cp_response,
    std::function<void(const MPSolution&)> solution_callback) {
  SolverLogger* logger = sat_model->GetOrCreate<SolverLogger>();
  sat::SatParameters& params = *sat_model->GetOrCreate<sat::SatParameters>();
  TimeLimit* time_limit = sat_model->GetOrCreate<TimeLimit>();

  // Model validation and delta handling.
  MPSolutionResponse response;
  std::optional<LazyMutableCopy<MPModelProto>> optional_model =
      GetMPModelOrPopulateResponse(request, &response);
  if (!optional_model) {
    // Note that the ExtractValidMPModelInPlaceOrPopulateResponseStatus() can
    // also close trivial model (empty or trivially infeasible). So this is not
    // always the MODEL_INVALID status.
    //
    // The logging is only needed for our benchmark script, so we use UNKNOWN
    // here, but we could log the proper status instead.
    if (logger->LoggingIsEnabled()) {
      sat::CpSolverResponse cp_response;
      cp_response.set_status(FromMPSolverResponseStatus(response.status()));
      SOLVER_LOG(logger, CpSolverResponseStats(cp_response));
    }
    return response;
  }

  // We will presolve directly on the MPModelProto, so get a copy or transfer
  // ownership from the LazyMutableCopy<MPModelProto>().
  std::unique_ptr<MPModelProto> mp_model =
      std::move(optional_model).value().copy_or_move_as_unique_ptr();

  // To enable solution checking we do need to keep a copy of the input model :(
  std::optional<sat::MPModelProtoWrapper> copy_for_solution_check;
  if (params.mip_verify_solution()) {
    copy_for_solution_check.emplace(MPModelProto(*mp_model), logger);
  }

  // The request is no longer needed after this.
  // Important: we need to copy the model above before clearing this.
  std::move(request).dispose();

  // We start by some extra validation since our code do not accept any kind
  // of input.
  if (params.mip_treat_high_magnitude_bounds_as_infinity()) {
    sat::ChangeLargeBoundsToInfinity(params.mip_max_valid_magnitude(),
                                     mp_model.get(), logger);
  }
  if (!sat::MPModelProtoValidationBeforeConversion(params, *mp_model, logger)) {
    return InvalidModelResponse(*logger, "Extra CP-SAT validation failed.");
  }

  // This is good to do before any presolve.
  if (!sat::MakeBoundsOfIntegerVariablesInteger(params, mp_model.get(),
                                                logger)) {
    return InfeasibleResponse(*logger,
                              "An integer variable has an empty domain");
  }

  // Coefficients really close to zero can cause issues.
  // We remove them right away according to our parameters.
  RemoveNearZeroTerms(params, mp_model.get(), logger);

  // NOTE(user): the LP presolvers API is a bit weird and keep a reference to
  // the given GlopParameters, so we need to make sure it outlive them.
  const glop::GlopParameters glop_params;
  std::vector<std::unique_ptr<glop::Preprocessor>> for_postsolve;
  if (!params.enumerate_all_solutions() && params.mip_presolve_level() > 0) {
    const glop::ProblemStatus status = ApplyMipPresolveSteps(
        glop_params, mp_model.get(), &for_postsolve, logger);
    switch (status) {
      case glop::ProblemStatus::INIT:
        // Continue with the solve.
        break;
      case glop::ProblemStatus::PRIMAL_INFEASIBLE:
        return InfeasibleResponse(
            *logger, "Problem proven infeasible during MIP presolve");
      case glop::ProblemStatus::INVALID_PROBLEM:
        return InvalidModelResponse(
            *logger, "Problem detected invalid during MIP presolve");
      default:
        // TODO(user): We put the INFEASIBLE_OR_UNBOUNBED case here since there
        // is no return status that exactly matches it.
        if (params.log_search_progress()) {
          // This is needed for our benchmark scripts.
          sat::CpSolverResponse cp_response;
          cp_response.set_status(sat::CpSolverStatus::UNKNOWN);
          SOLVER_LOG(logger, "MIP presolve: problem infeasible or unbounded.");
          LOG(INFO) << CpSolverResponseStats(cp_response);
        }
        response.set_status(MPSolverResponseStatus::MPSOLVER_UNKNOWN_STATUS);
        if (status == glop::ProblemStatus::INFEASIBLE_OR_UNBOUNDED) {
          response.set_status_str(
              "Problem proven infeasible or unbounded during MIP presolve");
        }
        return response;
    }
  }

  // Note that as we wrap the model, each general constraint get their own
  // new basic linear constraint, so the number of constraint can grow.
  const int old_num_variables = mp_model->variable().size();
  const int old_num_constraints = mp_model->constraint().size();
  const bool is_maximize = mp_model->maximize();
  auto mp_model_wrapper =
      std::make_unique<sat::MPModelProtoWrapper>(std::move(*mp_model), logger);
  mp_model.reset();  // Prevent using empty mp_model.

  SOLVER_LOG(logger, "");
  SOLVER_LOG(logger, "Scaling to pure integer problem.");

  if (!MakeBoundsInteger(params.mip_wanted_precision(), mp_model_wrapper.get(),
                         logger)) {
    return InfeasibleResponse(
        *logger, "Problem proven infeasible in MakeBoundsInteger()");
  }

  std::vector<double> var_shifts;
  if (params.mip_presolve_level() > 0) {
    if (!ExactPresolve(params, mp_model_wrapper.get(), logger)) {
      return InfeasibleResponse(*logger,
                                "Problem proven infeasible in ExactPresolve()");
    }
    var_shifts =
        ShiftVariableBounds(IntegerShifts(), mp_model_wrapper.get(), logger);
  }

  if (time_limit->LimitReached()) {
    return TimeLimitResponse(*logger);
  }

  const int num_variables = mp_model_wrapper->num_variables();
  std::vector<double> var_scaling(num_variables, 1.0);
  std::vector<double> integer_error_lb(num_variables, 0.0);
  std::vector<double> integer_error_ub(num_variables, 0.0);
  const auto update_scaling = [&var_scaling](absl::Span<const double> scaling) {
    for (int var = 0; var < scaling.size(); ++var) {
      if (scaling[var] == 1.0) continue;

      // We should only scale variable that are not already scaled.
      CHECK_EQ(var_scaling[var], 1.0);
      var_scaling[var] *= scaling[var];
    }
  };
  const auto update_shifts = [&](absl::Span<const double> new_shifts) {
    if (new_shifts.size() > var_shifts.size()) {
      var_shifts.resize(num_variables);
    }
    for (int var = 0; var < new_shifts.size(); ++var) {
      if (new_shifts[var] != 0) {
        var_shifts[var] += new_shifts[var] / var_scaling[var];
      }
    }
  };

  // We need to do that before the automatic detection of integers.
  RemoveNearZeroTerms(params, mp_model_wrapper->mutable_underlying_model(),
                      logger);

  if (params.mip_presolve_level() > 0) {
    update_shifts(ShiftToZeroAndMarkAsIntegerUnusedVariables(
        params, mp_model_wrapper.get(), logger));
    // Note that it is valid to ignore constraint enforcement here and below:
    // for any fixed assignment of the external integer variables, inactive
    // enforced constraints simply drop rows from the residual LP, and any
    // row-submatrix of a TUM (or 2-modular) matrix preserves its modularity,
    // vertex integrality, and the top-n Cramer's rule bound in
    // TightenComponentBounds(). This relies on all enforcement variables being
    // treated as external, which holds here even with include_large_integers
    // because only Boolean [0, 1] indicator enforcements exist before
    // ExtractEnforcementLiterals().
    sat::TightenResidualTotallyUnimodularVariableBounds(
        params, mp_model_wrapper.get(), logger);
  }

  // Clamp variable.
  int num_clamped = 0;
  if (!sat::ClampVariableBounds(params,
                                mp_model_wrapper->mutable_underlying_model(),
                                logger, &num_clamped)) {
    return InvalidModelResponse(*logger,
                                "Failed to convert model into CP-SAT model");
  }

  // Redo a round of propagation after clamping.
  if (params.mip_presolve_level() > 0) {
    update_shifts(ShiftToZeroAndMarkAsIntegerUnusedVariables(
        params, mp_model_wrapper.get(), logger));
    if (!ExactPresolve(params, mp_model_wrapper.get(), logger)) {
      return InfeasibleResponse(
          *logger,
          "Problem proven infeasible in ExactPresolve() after clamping");
    }
  }

  // TODO(user): move into ExactPresolve().
  sat::ExtractEnforcementLiterals(params, mp_model_wrapper.get(), logger);

  if (params.mip_automatically_scale_variables()) {
    // Note that this is more than "implied": it also detect variable that could
    // always be made integer even if there are optimal solutions with such
    // variable at a non-integer value.
    update_scaling(sat::DetectImpliedIntegers(
        params, mp_model_wrapper.get(), absl::MakeSpan(integer_error_lb),
        absl::MakeSpan(integer_error_ub), logger));
  }

  // From now on, the model is no longer modified: we convert it component by
  // component and record the per-variable transformations in `states`.
  std::vector<sat::MipVariableState> states =
      sat::InitialVariableStates(*mp_model_wrapper);
  for (int var = 0; var < num_variables; ++var) {
    states[var].error_lb = integer_error_lb[var];
    states[var].error_ub = integer_error_ub[var];
  }
  sat::CpModelProto cp_model;
  if (std::optional<MPSolutionResponse> error_response =
          ConvertComponentsToCpModelProto(params, num_clamped,
                                          *mp_model_wrapper, &states, &cp_model,
                                          logger);
      error_response.has_value()) {
    return *std::move(error_response);
  }
  const MPModelProto& underlying_model =
      *mp_model_wrapper->mutable_underlying_model();
  DCHECK_EQ(cp_model.variables().size(), num_variables);

  // Combine the transformations done before and after the split so that:
  //   original_value = cp_value / var_scaling + var_shifts.
  if (var_shifts.empty()) var_shifts.resize(num_variables, 0.0);
  for (int var = 0; var < num_variables; ++var) {
    var_shifts[var] += states[var].shift / var_scaling[var];
    var_scaling[var] *= states[var].scaling;
  }

  // Copy the hint if there is one.
  if (underlying_model.has_solution_hint()) {
    auto* cp_model_hint = cp_model.mutable_solution_hint();
    const int size = underlying_model.solution_hint().var_index().size();
    for (int i = 0; i < size; ++i) {
      const int var = underlying_model.solution_hint().var_index(i);
      if (var < 0 || var >= num_variables) continue;

      // The hint in the wrapper is in the wrapper space, transform it to the
      // CP-SAT space and clamp it to the variable domain to handle weird hint
      // input values.
      const sat::MipVariableState& state = states[var];
      double value = underlying_model.solution_hint().var_value(i);
      value = (value - state.shift) * state.scaling;
      value = std::max(value, state.lb);
      value = std::min(value, state.ub);

      cp_model_hint->add_vars(var);
      cp_model_hint->add_values(static_cast<int64_t>(std::round(value)));
    }
  }

  // We no longer need the mp_model after this, reclaim its memory.
  mp_model_wrapper.reset();

  // Configure model.
  auto post_solve = [&](const sat::CpSolverResponse& sat_response) {
    MPSolution mp_solution;
    mp_solution.set_objective_value(sat_response.objective_value());
    // Postsolve the bound shift and scaling.
    glop::SolveStatus solve_status = glop::OptimalSolveStatus();
    glop::ProblemSolution glop_solution((glop::RowIndex(old_num_constraints)),
                                        (glop::ColIndex(old_num_variables)));
    for (int v = 0; v < glop_solution.primal_values.size(); ++v) {
      glop_solution.primal_values[glop::ColIndex(v)] =
          static_cast<double>(sat_response.solution(v)) / var_scaling[v] +
          var_shifts[v];
    }
    for (int i = for_postsolve.size(); --i >= 0;) {
      for_postsolve[i]->RecoverSolution(solve_status, &glop_solution);
    }
    for (int v = 0; v < glop_solution.primal_values.size(); ++v) {
      mp_solution.add_variable_value(
          glop_solution.primal_values[glop::ColIndex(v)]);
    }
    return mp_solution;
  };

  if (solution_callback != nullptr) {
    sat_model->Add(sat::NewFeasibleSolutionObserver(
        [&](const sat::CpSolverResponse& sat_response) {
          solution_callback(post_solve(sat_response));
        }));
  }

  // Solve.
  *cp_response = sat::SolveCpModel(cp_model, sat_model);

  // Convert the response.
  //
  // TODO(user): Implement the row and column status.
  response.mutable_solve_info()->set_solve_wall_time_seconds(
      cp_response->wall_time());
  response.mutable_solve_info()->set_solve_user_time_seconds(
      cp_response->user_time());
  response.set_status(ToMPSolverResponseStatus(cp_response->status(),
                                               cp_model.has_objective()));
  if (response.status() == MPSOLVER_FEASIBLE ||
      response.status() == MPSOLVER_OPTIMAL) {
    response.set_objective_value(cp_response->objective_value());
    response.set_best_objective_bound(cp_response->best_objective_bound());
    MPSolution post_solved_solution = post_solve(*cp_response);
    *response.mutable_variable_value() =
        std::move(*post_solved_solution.mutable_variable_value());

    if (params.mip_verify_solution()) {
      // We should have no variable out of bounds nor integral variable that
      // are not integral, and we use the default tolerance of zero for these.
      //
      // Note that we ignore the checker result, this is just for informative
      // display. We also use a low tolerance to have an idea of the
      // max-violation, even if it is small.
      //
      // TODO(user): What I would actually prefer is just to always display
      // the maximum violation if it is non-zero.
      const std::string old_prefix(logger->LoggingPrefix());
      logger->SetLoggingPrefix(
          absl::StrCat(old_prefix, "[MIP solution checker] "));
      CheckerOptions options;
      options.constraint_tolerance = params.mip_tight_precision();
      options.logger = logger;
      absl::StatusOr<bool> status_or_verified = SolutionIsFeasible(
          copy_for_solution_check.value(), response.variable_value(), options);
      if (!status_or_verified.ok()) {
        SOLVER_LOG(logger,
                   "Preconditions failed during solution verification ?? ",
                   status_or_verified.status());
      } else {
        const bool verified = *status_or_verified;
        if (verified) {
          SOLVER_LOG(logger, "The solution passed verification with tolerance ",
                     params.mip_tight_precision());
        } else {
          SOLVER_LOG(logger, "The solution FAILED verification.");
        }
      }
      logger->SetLoggingPrefix(old_prefix);
    }
  }

  // Copy and postsolve any additional solutions.
  //
  // TODO(user): Remove the postsolve hack of copying to a response.
  for (const sat::CpSolverSolution& additional_solution :
       cp_response->additional_solutions()) {
    if (absl::MakeConstSpan(additional_solution.values()) ==
        absl::MakeConstSpan(cp_response->solution())) {
      continue;
    }
    double obj = cp_model.floating_point_objective().offset();
    for (int i = 0; i < cp_model.floating_point_objective().vars_size(); ++i) {
      const int32_t var = cp_model.floating_point_objective().vars(i);
      const double obj_coef = cp_model.floating_point_objective().coeffs(i);
      obj += additional_solution.values(var) * obj_coef;
    }
    // If the scaling factor is unset/zero, it is assumed to be one.
    if (cp_model.objective().scaling_factor() != 0.0) {
      obj *= cp_model.objective().scaling_factor();
    }
    sat::CpSolverResponse temp;
    *temp.mutable_solution() = additional_solution.values();
    temp.set_objective_value(obj);
    *response.add_additional_solutions() = post_solve(temp);
  }
  std::sort(response.mutable_additional_solutions()->pointer_begin(),
            response.mutable_additional_solutions()->pointer_end(),
            [is_maximize](const MPSolution* left, const MPSolution* right) {
              if (is_maximize) {
                return left->objective_value() > right->objective_value();
              } else {
                return left->objective_value() < right->objective_value();
              }
            });
  return response;
}

MPSolutionResponse SatSolveProto(
    LazyMutableCopy<MPModelRequest> request, std::atomic<bool>* interrupt_solve,
    std::function<void(const std::string&)> logging_callback,
    std::function<void(const MPSolution&)> solution_callback,
    std::function<void(const double)> best_bound_callback) {
  sat::Model sat_model;
  sat::SatParameters& params = *sat_model.GetOrCreate<sat::SatParameters>();
  params.set_log_search_progress(request->enable_internal_solver_output());

  // TODO(user): We do not support all the parameters here. In particular the
  // logs before the solver is called will not be appended to the response. Fix
  // that, and remove code duplication for the logger config. One way should be
  // to not touch/configure anything if the logger is already created while
  // calling SolveCpModel() and call a common config function from here or from
  // inside Solve()?
  SolverLogger* logger = sat_model.GetOrCreate<SolverLogger>();
  if (logging_callback != nullptr) {
    logger->AddInfoLoggingCallback(logging_callback);
  }
  logger->EnableLogging(params.log_search_progress());
  logger->SetLogToStdOut(params.log_to_stdout());

  // Set it now so that it can be overwritten by the solver specific parameters.
  if (request->has_solver_specific_parameters()) {
    // See EncodeSatParametersAsString() documentation.
    if constexpr (!std::is_base_of<Message, sat::SatParameters>::value) {
      if (!params.MergeFromString(request->solver_specific_parameters())) {
        return InvalidParametersResponse(
            *logger,
            "solver_specific_parameters is not a valid binary stream of the "
            "SatParameters proto");
      }
    } else {
      if (!ProtobufTextFormatMergeFromString(
              request->solver_specific_parameters(), &params)) {
        return InvalidParametersResponse(
            *logger,
            "solver_specific_parameters is not a valid textual representation "
            "of the SatParameters proto");
      }
    }
  }

  // Validate parameters.
  {
    const std::string error = sat::ValidateParameters(params);
    if (!error.empty()) {
      return InvalidParametersResponse(
          *logger, absl::StrCat("Invalid CP-SAT parameters: ", error));
    }
  }

  // Reconfigure the logger in case the solver_specific_parameters overwrite its
  // configuration. Note that the invalid parameter message will be logged
  // before that though according to request.enable_internal_solver_output().
  logger->EnableLogging(params.log_search_progress());
  logger->SetLogToStdOut(params.log_to_stdout());

  if (request->has_solver_time_limit_seconds()) {
    params.set_max_time_in_seconds(request->solver_time_limit_seconds());
  }

  sat_model.GetOrCreate<TimeLimit>()->ResetLimitFromParameters(params);

  if (interrupt_solve != nullptr) {
    sat_model.GetOrCreate<TimeLimit>()->RegisterExternalBooleanAsLimit(
        interrupt_solve);
  }

  if (best_bound_callback != nullptr) {
    sat_model.Add(sat::NewBestBoundCallback(best_bound_callback));
  }

  sat::CpSolverResponse cp_response;
  return SatSolveProtoInternal(std::move(request), &sat_model, &cp_response,
                               solution_callback);
}

std::string SatSolverVersion() { return sat::CpSatSolverVersion(); }

}  // namespace operations_research
