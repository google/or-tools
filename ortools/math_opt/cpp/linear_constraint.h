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

// IWYU pragma: private, include "ortools/math_opt/cpp/math_opt.h"
// IWYU pragma: friend "ortools/math_opt/cpp/.*"

// An object oriented wrapper for linear constraints in ModelStorage.
#ifndef OR_TOOLS_MATH_OPT_CPP_LINEAR_CONSTRAINT_H_
#define OR_TOOLS_MATH_OPT_CPP_LINEAR_CONSTRAINT_H_

#include <sstream>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "ortools/math_opt/constraints/util/model_util.h"
#include "ortools/math_opt/cpp/key_types.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/storage/model_storage_item.h"
#include "ortools/math_opt/storage/model_storage_types.h"

namespace operations_research {
namespace math_opt {

// A value type that references a linear constraint from ModelStorage. Usually
// this type is passed by copy.
class LinearConstraint final
    : public ModelStorageElement<ElementType::kLinearConstraint,
                                 LinearConstraint> {
 public:
  using ModelStorageElement::ModelStorageElement;

  inline double lower_bound() const;
  inline double upper_bound() const;
  inline absl::string_view name() const;

  inline bool is_coefficient_nonzero(Variable variable) const;

  // Returns 0.0 if the variable is not used in the constraint.
  inline double coefficient(Variable variable) const;

  // Returns the constraints as a bounded linear expression.
  //
  // The linear expression will have a zero offset, even if the constraint was
  // created with a non-zero one. For example:
  //
  //   const LinearConstraint c =
  //     model.AddLinearConstraint(3.2 <= x + 1.0 <= 4.2);
  //
  //   // Here `e` will contain 3.2 - 1.0 <= x <= 4.2 - 1.0.
  //   const BoundedLinearExpression e = c.AsBoundedLinearExpression();
  inline BoundedLinearExpression AsBoundedLinearExpression() const;

  // Returns a detailed string description of the contents of the constraint
  // (not its name, use `<<` for that instead).
  inline std::string ToString() const;
};

template <typename V>
using LinearConstraintMap = absl::flat_hash_map<LinearConstraint, V>;

////////////////////////////////////////////////////////////////////////////////
// Inline function implementations
////////////////////////////////////////////////////////////////////////////////

double LinearConstraint::lower_bound() const {
  return storage()->linear_constraint_lower_bound(typed_id());
}

double LinearConstraint::upper_bound() const {
  return storage()->linear_constraint_upper_bound(typed_id());
}

absl::string_view LinearConstraint::name() const {
  if (storage()->has_linear_constraint(typed_id())) {
    return storage()->linear_constraint_name(typed_id());
  }
  return kDeletedConstraintDefaultDescription;
}

bool LinearConstraint::is_coefficient_nonzero(const Variable variable) const {
  CHECK_EQ(variable.storage(), storage())
      << internal::kObjectsFromOtherModelStorage;
  return storage()->is_linear_constraint_coefficient_nonzero(
      typed_id(), variable.typed_id());
}

double LinearConstraint::coefficient(const Variable variable) const {
  CHECK_EQ(variable.storage(), storage())
      << internal::kObjectsFromOtherModelStorage;
  return storage()->linear_constraint_coefficient(typed_id(),
                                                  variable.typed_id());
}

BoundedLinearExpression LinearConstraint::AsBoundedLinearExpression() const {
  LinearExpression terms;
  for (const VariableId var :
       storage()->variables_in_linear_constraint(typed_id())) {
    terms += Variable(storage(), var) *
             storage()->linear_constraint_coefficient(typed_id(), var);
  }
  return storage()->linear_constraint_lower_bound(typed_id()) <=
         std::move(terms) <=
         storage()->linear_constraint_upper_bound(typed_id());
}

std::string LinearConstraint::ToString() const {
  if (!storage()->has_linear_constraint(typed_id())) {
    return std::string(kDeletedConstraintDefaultDescription);
  }
  std::stringstream str;
  str << AsBoundedLinearExpression();
  return str.str();
}

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CPP_LINEAR_CONSTRAINT_H_
