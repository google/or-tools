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
#include "ortools/math_opt/core/model_storage.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/cpp/map_filter.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research {
namespace math_opt {
namespace {

std::vector<std::pair<VariableId, double>> SortedVariableValues(
    const VariableMap<double>& var_map) {
  std::vector<std::pair<VariableId, double>> result(var_map.raw_map().begin(),
                                                    var_map.raw_map().end());
  std::sort(result.begin(), result.end());
  return result;
}

// Container must be an iterable on some type T where
//   const ModelStorage* T::storage() const
// is defined.
//
// CHECKs that the non-null model storages are the same, and returns the unique
// non-null model storage if it exists, otherwise null.
template <typename Container>
const ModelStorage* ConsistentModelStorage(
    const Container& model_items,
    const ModelStorage* const init_model = nullptr) {
  const ModelStorage* result = init_model;
  for (const auto& item : model_items) {
    const ModelStorage* const storage = item.storage();
    if (storage != nullptr) {
      if (result == nullptr) {
        result = storage;
      } else {
        CHECK_EQ(storage, result) << internal::kObjectsFromOtherModelStorage;
      }
    }
  }
  return result;
}

}  // namespace

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

CallbackData::CallbackData(const ModelStorage* storage,
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
    solution = VariableMap<double>(
        storage, MakeView(proto.primal_solution_vector()).as_map<VariableId>());
  }
  auto maybe_time = util_time::DecodeGoogleApiProto(proto.runtime());
  CHECK_OK(maybe_time.status());
  runtime = *maybe_time;
}

const ModelStorage* CallbackRegistration::storage() const {
  return internal::ConsistentModelStorage(
      {mip_node_filter.storage(), mip_solution_filter.storage()});
}

CallbackRegistrationProto CallbackRegistration::Proto() const {
  // Ensure that the underlying ModelStorage is consistent (or CHECK fail).
  storage();

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

const ModelStorage* CallbackResult::storage() const {
  const ModelStorage* result = ConsistentModelStorage(new_constraints);
  return ConsistentModelStorage(suggested_solutions, result);
}

CallbackResultProto CallbackResult::Proto() const {
  // Ensure that the underlying ModelStorage is consistent (or CHECK fail).
  storage();

  CallbackResultProto result;
  result.set_terminate(terminate);
  for (const VariableMap<double>& solution : suggested_solutions) {
    SparseDoubleVectorProto* solution_vector = result.add_suggested_solutions();
    for (const auto& [typed_id, value] : SortedVariableValues(solution)) {
      solution_vector->add_ids(typed_id.value());
      solution_vector->add_values(value);
    }
  }
  for (const GeneratedLinearConstraint& constraint : new_constraints) {
    CallbackResultProto::GeneratedLinearConstraint* constraint_proto =
        result.add_cuts();
    constraint_proto->set_is_lazy(constraint.is_lazy);
    constraint_proto->set_lower_bound(
        constraint.linear_constraint.lower_bound_minus_offset());
    constraint_proto->set_upper_bound(
        constraint.linear_constraint.upper_bound_minus_offset());
    for (const auto& [typed_id, value] : SortedVariableValues(
             constraint.linear_constraint.expression.terms())) {
      constraint_proto->mutable_linear_expression()->add_ids(typed_id.value());
      constraint_proto->mutable_linear_expression()->add_values(value);
    }
  }
  return result;
}

}  // namespace math_opt
}  // namespace operations_research
