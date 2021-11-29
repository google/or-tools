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

#include "ortools/math_opt/validators/callback_validator.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "google/protobuf/duration.pb.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/model_parameters_validator.h"
#include "ortools/math_opt/validators/scalar_validator.h"
#include "ortools/math_opt/validators/solution_validator.h"
#include "ortools/math_opt/validators/sparse_vector_validator.h"
#include "ortools/port/proto_utils.h"
#include "absl/status/status.h"
#include "ortools/base/status_macros.h"

namespace operations_research {
namespace math_opt {
namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

absl::Status IsEventRegistered(
    const CallbackEventProto event,
    const CallbackRegistrationProto& callback_registration) {
  // Unfortunatelly the range iterator return ints and not CallbackEventProtos.
  const int num_events = callback_registration.request_registration_size();
  for (int k = 0; k < num_events; ++k) {
    if (callback_registration.request_registration(k) == event) {
      return absl::OkStatus();
    }
  }
  return absl::InvalidArgumentError(absl::StrCat(
      "Event ", ProtoEnumToString(event),
      " not part of the registered_events in callback_registration"));
}

absl::Status ValidateGeneratedLinearConstraint(
    const CallbackResultProto::GeneratedLinearConstraint& linear_constraint,
    const bool add_cuts, const bool add_lazy_constraints,
    const ModelSummary& model_summary) {
  const auto coefficients = MakeView(linear_constraint.linear_expression());
  RETURN_IF_ERROR(CheckIdsAndValues(
      coefficients,
      {.allow_positive_infinity = false, .allow_negative_infinity = false}))
      << "Invalid linear_constraint coefficients";
  RETURN_IF_ERROR(CheckIdsSubset(coefficients.ids(), model_summary.variables,
                                 "cut variables", "model IDs"));
  RETURN_IF_ERROR(CheckScalar(linear_constraint.lower_bound(),
                              {.allow_positive_infinity = false}))
      << "for GeneratedLinearConstraint.lower_bound";
  RETURN_IF_ERROR(CheckScalar(linear_constraint.upper_bound(),
                              {.allow_negative_infinity = false}))
      << "for GeneratedLinearConstraint.upper_bound";
  if (linear_constraint.lower_bound() == -kInf &&
      linear_constraint.upper_bound() == kInf) {
    return absl::InvalidArgumentError(
        "Invalid GeneratedLinearConstraint, bounds [-inf,inf]");
  }
  if (linear_constraint.is_lazy() && !add_lazy_constraints) {
    return absl::InvalidArgumentError(
        "Invalid GeneratedLinearConstraint with lazy attribute set to true, "
        "adding lazy constraints requires "
        "CallbackRegistrationProto.add_lazy_constraints=true.");
  }
  if (!linear_constraint.is_lazy() && !add_cuts) {
    return absl::InvalidArgumentError(
        "Invalid GeneratedLinearConstraint with lazy attribute set to false, "
        "adding cuts requires CallbackRegistrationProto.add_cuts=true.");
  }
  return absl::OkStatus();
}

}  // namespace
absl::Status ValidateCallbackRegistration(
    const CallbackRegistrationProto& callback_registration,
    const ModelSummary& model_summary) {
  RETURN_IF_ERROR(ValidateSparseVectorFilter(
      callback_registration.mip_solution_filter(), model_summary.variables))
      << "Invalid CallbackRegistrationProto.mip_solution_filter";
  RETURN_IF_ERROR(ValidateSparseVectorFilter(
      callback_registration.mip_node_filter(), model_summary.variables))
      << "Invalid CallbackRegistrationProto.mip_node_filter";
  // Unfortunatelly the range iterator return ints and not CallbackEventProtos.
  const int num_events = callback_registration.request_registration_size();
  for (int k = 0; k < num_events; ++k) {
    const CallbackEventProto requested_event =
        callback_registration.request_registration(k);
    if (requested_event == CALLBACK_EVENT_UNSPECIFIED ||
        !CallbackEventProto_IsValid(requested_event)) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Invalid event ", requested_event, " can not be registered"));
    }
  }
  return absl::OkStatus();
}

absl::Status ValidateCallbackDataProto(
    const CallbackDataProto& cb_data,
    const CallbackRegistrationProto& callback_registration,
    const ModelSummary& model_summary) {
  const CallbackEventProto event = cb_data.event();
  RETURN_IF_ERROR(IsEventRegistered(event, callback_registration))
      << "Invalid CallbackDataProto.event for given CallbackRegistrationProto";

  if (!cb_data.messages().empty() && event != CALLBACK_EVENT_MESSAGE) {
    return absl::InvalidArgumentError(
        absl::StrCat("Can't provide message(s) for event ", event, " (",
                     ProtoEnumToString(event), ")"));
  }

  const bool has_primal_solution = cb_data.has_primal_solution();
  if (has_primal_solution && event != CALLBACK_EVENT_MIP_SOLUTION &&
      event != CALLBACK_EVENT_MIP_NODE) {
    return absl::InvalidArgumentError(
        absl::StrCat("Can't provide primal_solution for event ", event, " (",
                     ProtoEnumToString(event), ")"));
  }

#ifdef RETURN_IF_SCALAR
#error Collision in macro definition RETURN_IF_SCALAR
#endif
#define RETURN_IF_SCALAR(stat, value, option)                                 \
  do {                                                                        \
    if (stat.has_##value()) {                                                 \
      RETURN_IF_ERROR(CheckScalar(static_cast<double>(stat.value()), option)) \
          << "Invalid CallbackDataProto." << #stat << "." << #value;          \
    }                                                                         \
  } while (false)
  const DoubleOptions nonan;
  const DoubleOptions finite = {.allow_positive_infinity = false,
                                .allow_negative_infinity = false};
  const DoubleOptions noneg = {.allow_positive_infinity = false,
                               .allow_negative = false};
  // Check PresolveStats.
  const auto& presolve_stats = cb_data.presolve_stats();
  RETURN_IF_SCALAR(presolve_stats, bound_changes, noneg);
  RETURN_IF_SCALAR(presolve_stats, coefficient_changes, noneg);

  // Check SimplexStats.
  const auto& simplex_stats = cb_data.simplex_stats();
  RETURN_IF_SCALAR(simplex_stats, iteration_count, noneg);
  RETURN_IF_SCALAR(simplex_stats, objective_value, finite);
  RETURN_IF_SCALAR(simplex_stats, primal_infeasibility, noneg);
  RETURN_IF_SCALAR(simplex_stats, dual_infeasibility, noneg);

  // Check BarrierStats.
  const auto& barrier_stats = cb_data.barrier_stats();
  RETURN_IF_SCALAR(barrier_stats, iteration_count, noneg);
  RETURN_IF_SCALAR(barrier_stats, primal_objective, finite);
  RETURN_IF_SCALAR(barrier_stats, dual_objective, finite);
  RETURN_IF_SCALAR(barrier_stats, complementarity, finite);
  RETURN_IF_SCALAR(barrier_stats, primal_infeasibility, noneg);
  RETURN_IF_SCALAR(barrier_stats, dual_infeasibility, noneg);

  // Check MipStats.
  const auto& mip_stats = cb_data.mip_stats();
  RETURN_IF_SCALAR(mip_stats, primal_bound, nonan);
  RETURN_IF_SCALAR(mip_stats, dual_bound, nonan);
  RETURN_IF_SCALAR(mip_stats, explored_nodes, noneg);
  RETURN_IF_SCALAR(mip_stats, open_nodes, noneg);
  RETURN_IF_SCALAR(mip_stats, simplex_iterations, noneg);
  RETURN_IF_SCALAR(mip_stats, number_of_solutions_found, noneg);
  RETURN_IF_SCALAR(mip_stats, cutting_planes_in_lp, noneg);

  // Check runtime.
  RETURN_IF_ERROR(CheckScalar(cb_data.runtime().seconds(), noneg))
      << "Invalid CallbackDataProto.runtime.seconds";
  RETURN_IF_ERROR(CheckScalar(cb_data.runtime().nanos(), noneg))
      << "Invalid CallbackDataProto.runtime.nanos";
#undef RETURN_IF_SCALAR

  // Ensure required fields are available depending on event.
  switch (event) {
    case CALLBACK_EVENT_MIP_NODE:
    case CALLBACK_EVENT_MIP_SOLUTION: {
      if (has_primal_solution) {
        const SparseVectorFilterProto& filter =
            event == CALLBACK_EVENT_MIP_NODE
                ? callback_registration.mip_node_filter()
                : callback_registration.mip_solution_filter();
        RETURN_IF_ERROR(ValidatePrimalSolution(cb_data.primal_solution(),
                                               filter, model_summary))
            << "Invalid CallbackDataProto.primal_solution";
      } else if (event == CALLBACK_EVENT_MIP_SOLUTION) {
        return absl::InvalidArgumentError(
            absl::StrCat("Must provide primal_solution for event ", event, " (",
                         ProtoEnumToString(event), ")"));
      }
      break;
    }

    case CALLBACK_EVENT_MESSAGE: {
      if (!!cb_data.messages().empty()) {
        return absl::InvalidArgumentError(
            absl::StrCat("Invalid CallbackDataProto.messages, must provide "
                         "message(s) for event ",
                         event, " (", ProtoEnumToString(event), ")"));
      }
      for (absl::string_view message : cb_data.messages()) {
        // TODO(b/184047243): prefer StrContains on absl version bump
        if (message.find('\n') != message.npos) {
          return absl::InvalidArgumentError(
              absl::StrCat("Invalid CallbackDataProto.messages[], message '",
                           message, "' contains a new line character '\\n'"));
        }
      }
      break;
    }

    case CALLBACK_EVENT_UNSPECIFIED:
      // This can not happen as a valid callback_registration can not register
      // a CALLBACK_EVENT_UNSPECIFIED.
      LOG(FATAL)
          << "CALLBACK_EVENT_UNSPECIFIED can not be a registered event, this "
             "points to either an invalid CallbackRegistrationProto (which "
             "violates "
             "one of the assumptions of this function), or memory corruption";
    default:
      // The remaining events are just for information collection. No further
      // test required.
      break;
  }

  return absl::OkStatus();
}

absl::Status ValidateCallbackResultProto(
    const CallbackResultProto& callback_result,
    const CallbackEventProto callback_event,
    const CallbackRegistrationProto& callback_registration,
    const ModelSummary& model_summary) {
  // We assume that all arguments but the first are valid and concordant with
  // each other. Otherwise this is an internal implementation error.
  CHECK_OK(IsEventRegistered(callback_event, callback_registration));

  if (!callback_result.cuts().empty()) {
    if (callback_event != CALLBACK_EVENT_MIP_NODE &&
        callback_event != CALLBACK_EVENT_MIP_SOLUTION) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Invalid CallbackResultProto, can't return cuts for callback_event ",
          callback_event, "(", ProtoEnumToString(callback_event), ")"));
    }
    for (const CallbackResultProto::GeneratedLinearConstraint& cut :
         callback_result.cuts()) {
      RETURN_IF_ERROR(ValidateGeneratedLinearConstraint(
          cut, callback_registration.add_cuts(),
          callback_registration.add_lazy_constraints(), model_summary));
    }
  }
  if (!callback_result.suggested_solution().empty()) {
    if (callback_event != CALLBACK_EVENT_MIP_NODE) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Invalid CallbackResultProto, can't return suggested solutions for "
          "callback_event ",
          callback_event, "(", ProtoEnumToString(callback_event), ")"));
    }
    for (const PrimalSolutionProto& primal_solution :
         callback_result.suggested_solution()) {
      RETURN_IF_ERROR(ValidatePrimalSolution(
          primal_solution, SparseVectorFilterProto(), model_summary))
          << "Invalid CallbackResultProto.suggested_solution";
    }
  }

  return absl::OkStatus();
}

}  // namespace math_opt
}  // namespace operations_research
