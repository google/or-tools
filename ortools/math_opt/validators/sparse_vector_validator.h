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

#ifndef OR_TOOLS_MATH_OPT_VALIDATORS_SPARSE_VECTOR_VALIDATOR_H_
#define OR_TOOLS_MATH_OPT_VALIDATORS_SPARSE_VECTOR_VALIDATOR_H_
#include <type_traits>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/validators/ids_validator.h"
#include "ortools/math_opt/validators/scalar_validator.h"

namespace operations_research {
namespace math_opt {

template <typename T>
absl::Status CheckIdsAndValuesSize(const SparseVectorView<T>& vector_view,
                                   absl::string_view value_name = "values") {
  if (vector_view.ids_size() != vector_view.values_size()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Ids size= ", vector_view.ids_size(), " should be equal to ",
        value_name, " size= ", vector_view.values_size()));
  }
  return absl::OkStatus();
}

template <typename T,
          typename = std::enable_if_t<!std::is_floating_point<T>::value> >
absl::Status CheckValues(const SparseVectorView<T>& vector_view,
                         absl::string_view value_name = "values") {
  RETURN_IF_ERROR(CheckIdsAndValuesSize(vector_view, value_name));
  return absl::OkStatus();
}

template <typename T,
          typename = std::enable_if_t<!std::is_floating_point<T>::value> >
absl::Status CheckIdsAndValues(const SparseVectorView<T>& vector_view,
                               absl::string_view value_name = "values") {
  RETURN_IF_ERROR(CheckIdsRangeAndStrictlyIncreasing(vector_view.ids()));
  RETURN_IF_ERROR(CheckValues(vector_view, value_name));
  return absl::OkStatus();
}

template <typename T,
          typename = std::enable_if_t<std::is_floating_point<T>::value> >
absl::Status CheckValues(const SparseVectorView<T>& vector_view,
                         const DoubleOptions& options,
                         absl::string_view value_name = "values") {
  RETURN_IF_ERROR(CheckIdsAndValuesSize(vector_view, value_name));
  for (int i = 0, length = vector_view.values_size(); i < length; ++i) {
    RETURN_IF_ERROR(CheckScalar(vector_view.values(i), options))
        << absl::StrCat(" in: ", value_name, " for id: ", vector_view.ids(i),
                        " (at index: ", i, ")");
  }
  return absl::OkStatus();
}

template <typename T,
          typename = std::enable_if_t<std::is_floating_point<T>::value> >
absl::Status CheckIdsAndValues(const SparseVectorView<T>& vector_view,
                               const DoubleOptions& options,
                               absl::string_view value_name = "values") {
  RETURN_IF_ERROR(CheckIdsRangeAndStrictlyIncreasing(vector_view.ids()));
  RETURN_IF_ERROR(CheckValues(vector_view, options, value_name));
  return absl::OkStatus();
}

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_VALIDATORS_SPARSE_VECTOR_VALIDATOR_H_
