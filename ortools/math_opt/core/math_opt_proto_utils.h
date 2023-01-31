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

#ifndef OR_TOOLS_MATH_OPT_CORE_MATH_OPT_PROTO_UTILS_H_
#define OR_TOOLS_MATH_OPT_CORE_MATH_OPT_PROTO_UTILS_H_

#include <cstdint>
#include <optional>
#include <type_traits>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/log/check.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research {
namespace math_opt {

inline int NumVariables(const VariablesProto& variables) {
  return variables.ids_size();
}

inline int NumConstraints(const LinearConstraintsProto& linear_constraints) {
  return linear_constraints.ids_size();
}

inline int NumMatrixNonzeros(const SparseDoubleMatrixProto& matrix) {
  return matrix.row_ids_size();
}

// Returns the id of the first variable if there is one. If the input proto is
// valid, this will also be the smallest id.
inline std::optional<int64_t> FirstVariableId(const VariablesProto& variables) {
  return variables.ids().empty() ? std::nullopt
                                 : std::make_optional(variables.ids()[0]);
}

// Returns the id of the first linear constraint if there is one. If the input
// proto is valid, this will also be the smallest id.
inline std::optional<int64_t> FirstLinearConstraintId(
    const LinearConstraintsProto& linear_constraints) {
  return linear_constraints.ids().empty()
             ? std::nullopt
             : std::make_optional(linear_constraints.ids()[0]);
}

// Removes the items in the sparse double vector for all indices whose value is
// exactly 0.0.
//
// NaN values are kept in place.
//
// The function asserts that input is a valid sparse vector, i.e. that the
// number of values and ids match.
void RemoveSparseDoubleVectorZeros(SparseDoubleVectorProto& sparse_vector);

// A utility class that tests if a pair (id, value) should be filtered based on
// an input SparseVectorFilterProto.
//
// This predicate expects the input is sorted by ids. In non-optimized builds,
// it will check that this is the case.
class SparseVectorFilterPredicate {
 public:
  // Builds a predicate based on the input filter. A reference to this filter is
  // kept so the caller must make sure this filter outlives the predicate.
  //
  // The filter.filtered_ids is expected to be sorted and not contain
  // duplicates. In non-optimized builds, it will be CHECKed.
  explicit SparseVectorFilterPredicate(const SparseVectorFilterProto& filter);

  // Returns true if the input value should be kept, false if it should be
  // ignored since it is not selected by the filter.
  //
  // This function is expected to be called with strictly increasing ids. In
  // non-optimized builds it will CHECK that this is the case. It updates an
  // internal counter when filtering by ids.
  template <typename Value>
  bool AcceptsAndUpdate(const int64_t id, const Value& value);

 private:
  const SparseVectorFilterProto& filter_;

  // Index of the next element to consider in filter_.filtered_ids().
  int next_filtered_id_index_ = 0;

#ifndef NDEBUG
  // Invariant: next input id must be >= next_input_id_lower_bound_.
  //
  // The initial value is 0 since all ids are expected to be non-negative.
  int64_t next_input_id_lower_bound_ = 0;
#endif  // NDEBUG
};

// Returns the callback_registration.request_registration as a set of enums.
absl::flat_hash_set<CallbackEventProto> EventSet(
    const CallbackRegistrationProto& callback_registration);

// Sets the reason to TERMINATION_REASON_FEASIBLE if feasible = true and
// TERMINATION_REASON_NO_SOLUTION_FOUND otherwise.
TerminationProto TerminateForLimit(const LimitProto limit, bool feasible,
                                   absl::string_view detail = {});

TerminationProto FeasibleTermination(const LimitProto limit,
                                     absl::string_view detail = {});

TerminationProto NoSolutionFoundTermination(const LimitProto limit,
                                            absl::string_view detail = {});

TerminationProto TerminateForReason(TerminationReasonProto reason,
                                    absl::string_view detail = {});

enum class SupportType {
  kNotSupported = 1,
  kSupported = 2,
  kNotImplemented = 3,
};

struct SupportedProblemStructures {
  SupportType integer_variables = SupportType::kNotSupported;
  SupportType multi_objectives = SupportType::kNotSupported;
  SupportType quadratic_objectives = SupportType::kNotSupported;
  SupportType quadratic_constraints = SupportType::kNotSupported;
  SupportType sos1_constraints = SupportType::kNotSupported;
  SupportType sos2_constraints = SupportType::kNotSupported;
  SupportType indicator_constraints = SupportType::kNotSupported;
};

// Returns an InvalidArgumentError (respectively, UnimplementedError) if a
// problem structure is present in `model` and not supported (resp., not yet
// implemented) according to `support_menu`.
absl::Status ModelIsSupported(const ModelProto& model,
                              const SupportedProblemStructures& support_menu,
                              absl::string_view solver_name);

// Returns false if a problem structure is present in `update` and not
// not implemented or supported according to `support_menu`.
bool UpdateIsSupported(const ModelUpdateProto& update,
                       const SupportedProblemStructures& support_menu);

////////////////////////////////////////////////////////////////////////////////
// Inline functions implementations.
////////////////////////////////////////////////////////////////////////////////

template <typename Value>
bool SparseVectorFilterPredicate::AcceptsAndUpdate(const int64_t id,
                                                   const Value& value) {
#ifndef NDEBUG
  CHECK_GE(id, next_input_id_lower_bound_)
      << "This function must be called with strictly increasing ids.";

  // Update the range of the next expected id. We expect input to be strictly
  // increasing.
  next_input_id_lower_bound_ = id + 1;
#endif  // NDEBUG

  // For this predicate we use `0` as the zero to test with since as of today we
  // only have SparseDoubleVectorProto and SparseBoolVectorProto. The `bool`
  // type is an integral type so the comparison with 0 will indeed be equivalent
  // to keeping only `true` values.
  if (filter_.skip_zero_values() && value == 0) {
    return false;
  }

  if (!filter_.filter_by_ids()) {
    return true;
  }

  // Skip all filtered_ids that are smaller than the input id.
  while (next_filtered_id_index_ < filter_.filtered_ids_size() &&
         filter_.filtered_ids(next_filtered_id_index_) < id) {
    ++next_filtered_id_index_;
  }

  if (next_filtered_id_index_ == filter_.filtered_ids_size()) {
    // We filter by ids and there are no more ids that should pass.
    return false;
  }

  // The previous loop ensured that the element at next_filtered_id_index_ is
  // the first element greater or equal to id.
  return id == filter_.filtered_ids(next_filtered_id_index_);
}

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CORE_MATH_OPT_PROTO_UTILS_H_
