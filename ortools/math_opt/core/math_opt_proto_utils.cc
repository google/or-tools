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

#include "ortools/math_opt/core/math_opt_proto_utils.h"

#include <stdint.h>

#include <algorithm>
#include <functional>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "absl/container/flat_hash_set.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research {
namespace math_opt {

void RemoveSparseDoubleVectorZeros(SparseDoubleVectorProto& sparse_vector) {
  CHECK_EQ(sparse_vector.ids_size(), sparse_vector.values_size());
  // Keep track of the next index that has not yet been used for a non zero
  // value.
  int next = 0;
  for (const auto [id, value] : MakeView(sparse_vector)) {
    // Se use `!(== 0.0)` here so that we keep NaN values for which both `v ==
    // 0` and `v != 0` returns false.
    if (!(value == 0.0)) {
      sparse_vector.set_ids(next, id);
      sparse_vector.set_values(next, value);
      ++next;
    }
  }
  // At the end of the iteration, `next` contains the index of the first unused
  // index. This means it contains the number of used elements.
  sparse_vector.mutable_ids()->Truncate(next);
  sparse_vector.mutable_values()->Truncate(next);
}

SparseVectorFilterPredicate::SparseVectorFilterPredicate(
    const SparseVectorFilterProto& filter)
    : filter_(filter) {
  // We only do this test in non-optimized builds.
  if (DEBUG_MODE && filter_.filter_by_ids()) {
    // Checks that input filtered_ids are strictly increasing.
    // That is: for all i, ids(i) < ids(i+1).
    // Hence here we fail if there exists i such that ids(i) >= ids(i+1).
    const auto& ids = filter_.filtered_ids();
    CHECK(std::adjacent_find(ids.begin(), ids.end(),
                             std::greater_equal<int64_t>()) == ids.end())
        << "The input filter.filtered_ids must be strictly increasing.";
  }
}

absl::flat_hash_set<CallbackEventProto> EventSet(
    const CallbackRegistrationProto& callback_registration) {
  // Here we don't use for-range loop since for repeated enum fields, the type
  // used in C++ is RepeatedField<int>. Using the generated getter instead
  // guarantees type safety.
  absl::flat_hash_set<CallbackEventProto> events;
  for (int i = 0; i < callback_registration.request_registration_size(); ++i) {
    events.emplace(callback_registration.request_registration(i));
  }
  return events;
}

}  // namespace math_opt
}  // namespace operations_research
