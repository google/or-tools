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
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/cpp/map_filter.h"
#include "ortools/math_opt/cpp/sparse_containers.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/model_storage.h"

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
