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

// IWYU pragma: private, include "ortools/math_opt/cpp/math_opt.h"
// IWYU pragma: friend "ortools/math_opt/cpp/.*"
#ifndef OR_TOOLS_MATH_OPT_CONSTRAINTS_INDICATOR_INDICATOR_CONSTRAINT_H_
#define OR_TOOLS_MATH_OPT_CONSTRAINTS_INDICATOR_INDICATOR_CONSTRAINT_H_

#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/constraints/util/model_util.h"
#include "ortools/math_opt/cpp/id_map.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/storage/model_storage.h"

namespace operations_research::math_opt {

// A value type that references an indicator constraint from ModelStorage.
// Usually this type is passed by copy.
class IndicatorConstraint {
 public:
  // The typed integer used for ids.
  using IdType = IndicatorConstraintId;

  inline IndicatorConstraint(const ModelStorage* storage,
                             IndicatorConstraintId id);

  inline int64_t id() const;

  inline IndicatorConstraintId typed_id() const;
  inline const ModelStorage* storage() const;

  inline absl::string_view name() const;

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

  friend inline bool operator==(const IndicatorConstraint& lhs,
                                const IndicatorConstraint& rhs);
  friend inline bool operator!=(const IndicatorConstraint& lhs,
                                const IndicatorConstraint& rhs);
  template <typename H>
  friend H AbslHashValue(H h, const IndicatorConstraint& constraint);
  friend std::ostream& operator<<(std::ostream& ostr,
                                  const IndicatorConstraint& constraint);

 private:
  const ModelStorage* storage_;
  IndicatorConstraintId id_;
};

// Implements the API of std::unordered_map<IndicatorConstraint, V>, but forbids
// IndicatorConstraints from different models in the same map.
template <typename V>
using IndicatorConstraintMap = IdMap<IndicatorConstraint, V>;

// Streams the name of the constraint, as registered upon constraint creation,
// or a short default if none was provided.
inline std::ostream& operator<<(std::ostream& ostr,
                                const IndicatorConstraint& constraint);

////////////////////////////////////////////////////////////////////////////////
// Inline function implementations
////////////////////////////////////////////////////////////////////////////////

int64_t IndicatorConstraint::id() const { return id_.value(); }

IndicatorConstraintId IndicatorConstraint::typed_id() const { return id_; }

const ModelStorage* IndicatorConstraint::storage() const { return storage_; }

absl::string_view IndicatorConstraint::name() const {
  if (storage_->has_constraint(id_)) {
    return storage_->constraint_data(id_).name;
  }
  return kDeletedConstraintDefaultDescription;
}

std::optional<Variable> IndicatorConstraint::indicator_variable() const {
  const std::optional<VariableId> maybe_indicator =
      storage_->constraint_data(id_).indicator;
  if (!maybe_indicator.has_value()) {
    return std::nullopt;
  }
  return Variable(storage_, *maybe_indicator);
}

bool IndicatorConstraint::activate_on_zero() const {
  return storage_->constraint_data(id_).activate_on_zero;
}

std::vector<Variable> IndicatorConstraint::NonzeroVariables() const {
  return AtomicConstraintNonzeroVariables(*storage_, id_);
}

bool operator==(const IndicatorConstraint& lhs,
                const IndicatorConstraint& rhs) {
  return lhs.id_ == rhs.id_ && lhs.storage_ == rhs.storage_;
}

bool operator!=(const IndicatorConstraint& lhs,
                const IndicatorConstraint& rhs) {
  return !(lhs == rhs);
}

template <typename H>
H AbslHashValue(H h, const IndicatorConstraint& constraint) {
  return H::combine(std::move(h), constraint.id_.value(), constraint.storage_);
}

std::ostream& operator<<(std::ostream& ostr,
                         const IndicatorConstraint& constraint) {
  // TODO(b/170992529): handle quoting of invalid characters in the name.
  const absl::string_view name = constraint.name();
  if (name.empty()) {
    ostr << "__indic_con#" << constraint.id() << "__";
  } else {
    ostr << name;
  }
  return ostr;
}

IndicatorConstraint::IndicatorConstraint(const ModelStorage* const storage,
                                         const IndicatorConstraintId id)
    : storage_(storage), id_(id) {}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CONSTRAINTS_INDICATOR_INDICATOR_CONSTRAINT_H_
