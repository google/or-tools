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

// An object oriented wrapper for quadratic constraints in ModelStorage.
#ifndef OR_TOOLS_MATH_OPT_CONSTRAINTS_QUADRATIC_QUADRATIC_CONSTRAINT_H_
#define OR_TOOLS_MATH_OPT_CONSTRAINTS_QUADRATIC_QUADRATIC_CONSTRAINT_H_

#include <sstream>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "ortools/math_opt/constraints/util/model_util.h"
#include "ortools/math_opt/cpp/key_types.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/elemental/elements.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/storage/model_storage_item.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"
#include "ortools/math_opt/storage/sparse_matrix.h"

namespace operations_research::math_opt {

// A value type that references a quadratic constraint from ModelStorage.
// Usually this type is passed by copy.
class QuadraticConstraint final
    : public ModelStorageElement<ElementType::kQuadraticConstraint,
                                 QuadraticConstraint> {
 public:
  using ModelStorageElement::ModelStorageElement;

  inline double lower_bound() const;
  inline double upper_bound() const;
  inline absl::string_view name() const;

  inline bool is_linear_coefficient_nonzero(Variable variable) const;
  inline bool is_quadratic_coefficient_nonzero(Variable first_variable,
                                               Variable second_variable) const;

  // Returns 0.0 if the variable does not appear in the linear part of the
  // constraint.
  inline double linear_coefficient(Variable variable) const;
  // Returns 0.0 if the variable does not appear in the quadratic part of the
  // constraint.
  inline double quadratic_coefficient(Variable first_variable,
                                      Variable second_variable) const;

  // All variables that appear in the quadratic constraint with a nonzero
  // coefficient: in the linear terms, the quadratic terms, or both. Order is
  // not defined.
  inline std::vector<Variable> NonzeroVariables() const;

  // Returns the constraints as a bounded quadratic expression.
  //
  // The quadratic expression will have a zero offset, even if the constraint
  // was created with a non-zero one. For example:
  //
  //   const QuadraticConstraint c =
  //     model.AddQuadraticConstraint(3.2 <= x*x + 1.0 <= 4.2);
  //
  //   // Here `e` will contain 3.2 - 1.0 <= x*x <= 4.2 - 1.0.
  //   const BoundedQuadraticExpression e = c.AsBoundedQuadraticExpression();
  BoundedQuadraticExpression AsBoundedQuadraticExpression() const;

  // Returns a detailed string description of the contents of the constraint
  // (not its name, use `<<` for that instead).
  inline std::string ToString() const;
};

////////////////////////////////////////////////////////////////////////////////
// Inline function implementations
////////////////////////////////////////////////////////////////////////////////

double QuadraticConstraint::lower_bound() const {
  return storage()->constraint_data(typed_id()).lower_bound;
}

double QuadraticConstraint::upper_bound() const {
  return storage()->constraint_data(typed_id()).upper_bound;
}

absl::string_view QuadraticConstraint::name() const {
  if (storage()->has_constraint(typed_id())) {
    return storage()->constraint_data(typed_id()).name;
  }
  return kDeletedConstraintDefaultDescription;
}

bool QuadraticConstraint::is_linear_coefficient_nonzero(
    const Variable variable) const {
  return linear_coefficient(variable) != 0.0;
}

bool QuadraticConstraint::is_quadratic_coefficient_nonzero(
    const Variable first_variable, const Variable second_variable) const {
  return quadratic_coefficient(first_variable, second_variable) != 0.0;
}

double QuadraticConstraint::linear_coefficient(const Variable variable) const {
  CHECK_EQ(variable.storage(), storage())
      << internal::kObjectsFromOtherModelStorage;
  return storage()
      ->constraint_data(typed_id())
      .linear_terms.get(variable.typed_id());
}

double QuadraticConstraint::quadratic_coefficient(
    const Variable first_variable, const Variable second_variable) const {
  CHECK_EQ(first_variable.storage(), storage())
      << internal::kObjectsFromOtherModelStorage;
  CHECK_EQ(second_variable.storage(), storage())
      << internal::kObjectsFromOtherModelStorage;
  return storage()
      ->constraint_data(typed_id())
      .quadratic_terms.get(first_variable.typed_id(),
                           second_variable.typed_id());
}

std::vector<Variable> QuadraticConstraint::NonzeroVariables() const {
  return AtomicConstraintNonzeroVariables(*storage(), typed_id());
}

std::string QuadraticConstraint::ToString() const {
  if (!storage()->has_constraint(typed_id())) {
    return std::string(kDeletedConstraintDefaultDescription);
  }
  std::stringstream str;
  str << AsBoundedQuadraticExpression();
  return str.str();
}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CONSTRAINTS_QUADRATIC_QUADRATIC_CONSTRAINT_H_
