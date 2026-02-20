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

#include "ortools/math_opt/cpp/callback.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/cpp/map_filter.h"
#include "ortools/math_opt/cpp/model.h"
#include "ortools/math_opt/cpp/sparse_containers.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/util/status_macros.h"

namespace operations_research {
namespace math_opt {

std::optional<absl::string_view> Enum<CallbackEvent>::ToOptString(
    CallbackEvent value) {
  switch (value) {
    case CallbackEvent::kPresolve:
      return "presolve";
    case CallbackEvent::kSimplex:
      return "simplex";
    case CallbackEvent::kMip:
      return "mip";
    case CallbackEvent::kMipSolution:
      return "mip_solution";
    case CallbackEvent::kMipNode:
      return "mip_node";
    case CallbackEvent::kBarrier:
      return "barrier";
  }
  return std::nullopt;
}

absl::Span<const CallbackEvent> Enum<CallbackEvent>::AllValues() {
  static constexpr CallbackEvent kCallbackEventValues[] = {
      CallbackEvent::kPresolve, CallbackEvent::kSimplex,
      CallbackEvent::kMip,      CallbackEvent::kMipSolution,
      CallbackEvent::kMipNode,  CallbackEvent::kBarrier,
  };
  return absl::MakeConstSpan(kCallbackEventValues);
}

CallbackData::CallbackData(const CallbackEvent event,
                           const absl::Duration runtime)
    : event(event), runtime(runtime) {}

CallbackData::CallbackData(const ModelStorageCPtr storage,
                           const CallbackDataProto& proto)
    // iOS 11 does not support .value() hence we use operator* here and CHECK
    // below that we have a value.
    : event(*EnumFromProto(proto.event())),
      presolve_stats(proto.presolve_stats()),
      simplex_stats(proto.simplex_stats()),
      barrier_stats(proto.barrier_stats()),
      mip_stats(proto.mip_stats()) {
  CHECK(EnumFromProto(proto.event()).has_value());
  if (proto.has_primal_solution_vector()) {
    solution = VariableValuesFromProto(storage, proto.primal_solution_vector())
                   .value();
  }
  auto maybe_time = util_time::DecodeGoogleApiProto(proto.runtime());
  CHECK_OK(maybe_time.status());
  runtime = *maybe_time;
}

absl::Status CallbackData::CheckModelStorage(
    const ModelStorageCPtr expected_storage) const {
  if (solution.has_value()) {
    for (const auto& [v, _] : solution.value()) {
      RETURN_IF_ERROR(internal::CheckModelStorage(
          /*storage=*/v.storage(), /*expected_storage=*/expected_storage))
          << "invalid variable " << v << " in solution";
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<CallbackDataProto> CallbackData::Proto() const {
  CallbackDataProto proto;
  proto.set_event(EnumToProto(event));
  *proto.mutable_presolve_stats() = presolve_stats;
  *proto.mutable_simplex_stats() = simplex_stats;
  *proto.mutable_barrier_stats() = barrier_stats;
  *proto.mutable_mip_stats() = mip_stats;
  if (solution.has_value()) {
    *proto.mutable_primal_solution_vector() =
        VariableValuesToProto(solution.value());
  }
  OR_ASSIGN_OR_RETURN3(*proto.mutable_runtime(),
                       util_time::EncodeGoogleApiProto(runtime),
                       _ << "failed to encode runtime");
  return proto;
}

absl::StatusOr<CallbackRegistration> CallbackRegistration::FromProto(
    const Model& model, const CallbackRegistrationProto& registration_proto) {
  CallbackRegistration result;

  // Parses `events`.
  for (int e = 0; e < registration_proto.request_registration_size(); ++e) {
    const CallbackEventProto event_proto =
        registration_proto.request_registration(e);
    const std::optional<CallbackEvent> event = EnumFromProto(event_proto);
    if (event == std::nullopt) {
      return util::InvalidArgumentErrorBuilder()
             << "value CallbackRegistrationProto.request_registration[" << e
             << "] is CALLBACK_EVENT_UNSPECIFIED";
    }
    if (!result.events.insert(event.value()).second) {
      return util::InvalidArgumentErrorBuilder()
             << "value " << event
             << " is repeated at "
                "CallbackRegistrationProto.request_registration["
             << e << "]";
    }
  }

  OR_ASSIGN_OR_RETURN3(
      result.mip_solution_filter,
      VariableFilterFromProto(model, registration_proto.mip_solution_filter()),
      _ << "invalid CallbackRegistrationProto.mip_solution_filter");
  OR_ASSIGN_OR_RETURN3(
      result.mip_node_filter,
      VariableFilterFromProto(model, registration_proto.mip_node_filter()),
      _ << "invalid CallbackRegistrationProto.mip_node_filter");

  result.add_cuts = registration_proto.add_cuts();
  result.add_lazy_constraints = registration_proto.add_lazy_constraints();

  return result;
}

absl::Status CallbackRegistration::CheckModelStorage(
    const ModelStorageCPtr expected_storage) const {
  RETURN_IF_ERROR(mip_node_filter.CheckModelStorage(expected_storage))
      << "invalid mip_node_filter";
  RETURN_IF_ERROR(mip_solution_filter.CheckModelStorage(expected_storage))
      << "invalid mip_solution_filter";
  return absl::OkStatus();
}

CallbackRegistrationProto CallbackRegistration::Proto() const {
  CallbackRegistrationProto result;
  for (const CallbackEvent event : events) {
    result.add_request_registration(EnumToProto(event));
  }
  std::sort(result.mutable_request_registration()->begin(),
            result.mutable_request_registration()->end());
  *result.mutable_mip_solution_filter() = mip_solution_filter.Proto();
  *result.mutable_mip_node_filter() = mip_node_filter.Proto();
  result.set_add_lazy_constraints(add_lazy_constraints);
  result.set_add_cuts(add_cuts);
  return result;
}

absl::StatusOr<CallbackResult> CallbackResult::FromProto(
    const Model& model, const CallbackResultProto& result_proto) {
  CallbackResult result = {
      .terminate = result_proto.terminate(),
  };

  // Add new_constraints.
  for (int c = 0; c < result_proto.cuts_size(); ++c) {
    const CallbackResultProto::GeneratedLinearConstraint& constraint_proto =
        result_proto.cuts(c);
    OR_ASSIGN_OR_RETURN3(
        const VariableMap<double> coefficients,
        VariableValuesFromProto(model.storage(),
                                constraint_proto.linear_expression()),
        _ << "invalid CallbackResultProto.cuts[" << c << "].linear_expression");
    LinearExpression expression;
    for (const auto [v, coeff] : coefficients) {
      expression += coeff * v;
    };
    result.new_constraints.push_back({
        .linear_constraint = BoundedLinearExpression(
            /*expression=*/std::move(expression),
            /*lower_bound=*/constraint_proto.lower_bound(),
            /*upper_bound=*/constraint_proto.upper_bound()),
        .is_lazy = constraint_proto.is_lazy(),
    });
  }

  // Add suggested_solutions.
  for (int s = 0; s < result_proto.suggested_solutions_size(); ++s) {
    const SparseDoubleVectorProto suggested_solution_proto =
        result_proto.suggested_solutions(s);
    OR_ASSIGN_OR_RETURN3(
        VariableMap<double> suggested_solution,
        VariableValuesFromProto(model.storage(), suggested_solution_proto),
        _ << "invalid CallbackResultProto.suggested_solutions[" << s << "]");
    result.suggested_solutions.push_back(std::move(suggested_solution));
  }

  return result;
}

absl::Status CallbackResult::CheckModelStorage(
    const ModelStorageCPtr expected_storage) const {
  for (const GeneratedLinearConstraint& constraint : new_constraints) {
    RETURN_IF_ERROR(
        internal::CheckModelStorage(/*storage=*/constraint.storage(),
                                    /*expected_storage=*/expected_storage))
        << "invalid new_constraints";
  }
  for (const VariableMap<double>& solution : suggested_solutions) {
    for (const auto& [v, _] : solution) {
      RETURN_IF_ERROR(internal::CheckModelStorage(
          /*storage=*/v.storage(), /*expected_storage=*/expected_storage))
          << "invalid variable " << v << " in suggested_solutions";
    }
  }
  return absl::OkStatus();
}

CallbackResultProto CallbackResult::Proto() const {
  CallbackResultProto result;
  result.set_terminate(terminate);
  for (const VariableMap<double>& solution : suggested_solutions) {
    *result.add_suggested_solutions() = VariableValuesToProto(solution);
  }
  for (const GeneratedLinearConstraint& constraint : new_constraints) {
    CallbackResultProto::GeneratedLinearConstraint* constraint_proto =
        result.add_cuts();
    constraint_proto->set_is_lazy(constraint.is_lazy);
    constraint_proto->set_lower_bound(
        constraint.linear_constraint.lower_bound_minus_offset());
    constraint_proto->set_upper_bound(
        constraint.linear_constraint.upper_bound_minus_offset());
    *constraint_proto->mutable_linear_expression() =
        VariableValuesToProto(constraint.linear_constraint.expression.terms());
  }
  return result;
}

}  // namespace math_opt
}  // namespace operations_research
