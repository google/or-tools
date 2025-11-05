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
#ifndef ORTOOLS_MATH_OPT_CONSTRAINTS_INDICATOR_INDICATOR_CONSTRAINT_H_
#define ORTOOLS_MATH_OPT_CONSTRAINTS_INDICATOR_INDICATOR_CONSTRAINT_H_

#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "ortools/math_opt/constraints/util/model_util.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/elemental/elements.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/storage/model_storage_item.h"

namespace operations_research::math_opt {

// A value type that references an indicator constraint from ModelStorage.
// Usually this type is passed by copy.
class IndicatorConstraint final
    : public ModelStorageElement<ElementType::kIndicatorConstraint,
                                 IndicatorConstraint> {
 public:
  using ModelStorageElement::ModelStorageElement;

  absl::string_view name() const;

  // Returns nullopt if the indicator variable is unset (this is a valid state,
  // in which the constraint is functionally ignored).
  inline std::optional<Variable> indicator_variable() const;

  // The value the indicator variable takes to activate the implied constraint.
  inline bool activate_on_zero() const;

  BoundedLinearExpression ImpliedConstraint() const;

  // Returns all variables that appear in the indicator constraint with a
  // nonzero coefficient. Order is not defined.
  inline std::vector<Variable> NonzeroVariables() const;

  // Returns a detailed string description of the contents of the constraint
  // (not its name, use `<<` for that instead).
  std::string ToString() const;
};

////////////////////////////////////////////////////////////////////////////////
// Inline function implementations
////////////////////////////////////////////////////////////////////////////////

inline absl::string_view IndicatorConstraint::name() const {
  if (storage()->has_constraint(typed_id())) {
    return storage()->constraint_data(typed_id()).name;
  }
  return kDeletedConstraintDefaultDescription;
}

std::optional<Variable> IndicatorConstraint::indicator_variable() const {
  const std::optional<VariableId> maybe_indicator =
      storage()->constraint_data(typed_id()).indicator;
  if (!maybe_indicator.has_value()) {
    return std::nullopt;
  }
  return Variable(storage(), *maybe_indicator);
}

bool IndicatorConstraint::activate_on_zero() const {
  return storage()->constraint_data(typed_id()).activate_on_zero;
}

std::vector<Variable> IndicatorConstraint::NonzeroVariables() const {
  return AtomicConstraintNonzeroVariables(*storage(), typed_id());
}

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_CONSTRAINTS_INDICATOR_INDICATOR_CONSTRAINT_H_
