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
#include <string>
#include <utility>
#include <vector>

#include "ortools/base/logging.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "ortools/base/int_type.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/indexed_model.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/cpp/key_types.h"
#include "ortools/math_opt/cpp/map_filter.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "absl/status/status.h"
#include "ortools/base/protoutil.h"

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
//   IndexedModel* T::model() const
// is defined.
//
// CHECKs that the non-null models the same, and returns the unique non-null
// model if it exists, otherwise null.
template <typename Container>
IndexedModel* ConsistentModel(const Container& model_items,
                              IndexedModel* const init_model = nullptr) {
  IndexedModel* result = init_model;
  for (const auto& item : model_items) {
    IndexedModel* const model = item.model();
    if (model != nullptr) {
      if (result == nullptr) {
        result = model;
      } else {
        CHECK_EQ(model, result) << internal::kObjectsFromOtherIndexedModel;
      }
    }
  }
  return result;
}

}  // namespace

CallbackData::CallbackData(IndexedModel* model, const CallbackDataProto& proto)
    : event(proto.event()),
      messages(proto.messages().begin(), proto.messages().end()),
      presolve_stats(proto.presolve_stats()),
      simplex_stats(proto.simplex_stats()),
      barrier_stats(proto.barrier_stats()),
      mip_stats(proto.mip_stats()) {
  if (proto.has_primal_solution()) {
    solution = VariableMap<double>(
        model, MakeView(proto.primal_solution().variable_values())
                   .as_map<VariableId>());
  }
  auto maybe_time = util_time::DecodeGoogleApiProto(proto.runtime());
  CHECK_OK(maybe_time.status());
  runtime = *maybe_time;
}

IndexedModel* CallbackRegistration::model() const {
  return internal::ConsistentModel(
      {mip_node_filter.model(), mip_solution_filter.model()});
}

CallbackRegistrationProto CallbackRegistration::Proto() const {
  // Ensure that the underlying IndexedModel is consistent (or CHECK fail).
  model();

  CallbackRegistrationProto result;
  for (const CallbackEventProto event : events) {
    result.add_request_registration(event);
  }
  std::sort(result.mutable_request_registration()->begin(),
            result.mutable_request_registration()->end());
  *result.mutable_mip_solution_filter() = mip_solution_filter.Proto();
  *result.mutable_mip_node_filter() = mip_node_filter.Proto();
  result.set_add_lazy_constraints(add_lazy_constraints);
  result.set_add_cuts(add_cuts);
  return result;
}

IndexedModel* CallbackResult::model() const {
  IndexedModel* result = ConsistentModel(new_constraints);
  return ConsistentModel(suggested_solutions, result);
}

CallbackResultProto CallbackResult::Proto() const {
  // Ensure that the underlying IndexedModel is consistent (or CHECK fail).
  model();

  CallbackResultProto result;
  result.set_terminate(terminate);
  for (const VariableMap<double>& solution : suggested_solutions) {
    PrimalSolutionProto* solution_proto = result.add_suggested_solution();
    for (const auto& [typed_id, value] : SortedVariableValues(solution)) {
      solution_proto->mutable_variable_values()->add_ids(typed_id.value());
      solution_proto->mutable_variable_values()->add_values(value);
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
