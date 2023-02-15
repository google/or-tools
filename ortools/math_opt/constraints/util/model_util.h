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

#ifndef OR_TOOLS_MATH_OPT_CONSTRAINTS_UTIL_MODEL_UTIL_H_
#define OR_TOOLS_MATH_OPT_CONSTRAINTS_UTIL_MODEL_UTIL_H_

#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/string_view.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"

namespace operations_research::math_opt {

// A default way to describe a constraint that has been deleted from its
// associated model.
constexpr absl::string_view kDeletedConstraintDefaultDescription =
    "[constraint deleted from model]";

// Converts data from "raw ID" format to a LinearExpression, in the C++ API,
// associated with `storage`.
LinearExpression ToLinearExpression(const ModelStorage& storage,
                                    const SparseCoefficientMap& coeffs,
                                    double offset);

// Converts a `LinearExpression` to the associated "raw ID" format.
std::pair<SparseCoefficientMap, double> FromLinearExpression(
    const LinearExpression& expression);

template <typename IdType>
std::vector<Variable> AtomicConstraintNonzeroVariables(
    const ModelStorage& storage, const IdType id) {
  const std::vector<VariableId> raw_vars =
      storage.constraint_data(id).RelatedVariables();
  std::vector<Variable> vars;
  vars.reserve(raw_vars.size());
  for (const VariableId raw_var : raw_vars) {
    vars.push_back(Variable(&storage, raw_var));
  }
  return vars;
}

// Duck-types on `ConstraintType` having a typedef for the associated `IdType`,
// and having a `(const ModelStorage*, IdType)` constructor.
template <typename ConstraintType>
std::vector<ConstraintType> AtomicConstraints(const ModelStorage& storage) {
  using IdType = typename ConstraintType::IdType;
  std::vector<ConstraintType> result;
  result.reserve(storage.num_constraints<IdType>());
  for (const IdType con_id : storage.Constraints<IdType>()) {
    result.push_back(ConstraintType(&storage, con_id));
  }
  return result;
}

// * Duck-types on `ConstraintType` having a `typed_id()` getter.
template <typename ConstraintType>
std::vector<ConstraintType> SortedAtomicConstraints(
    const ModelStorage& storage) {
  std::vector<ConstraintType> constraints =
      AtomicConstraints<ConstraintType>(storage);
  absl::c_sort(constraints,
               [](const ConstraintType& l, const ConstraintType& r) {
                 return l.typed_id() < r.typed_id();
               });
  return constraints;
}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CONSTRAINTS_UTIL_MODEL_UTIL_H_
