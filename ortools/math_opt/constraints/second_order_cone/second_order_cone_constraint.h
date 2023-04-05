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
#ifndef OR_TOOLS_MATH_OPT_CONSTRAINTS_SECOND_ORDER_CONE_SECOND_ORDER_CONE_CONSTRAINT_H_
#define OR_TOOLS_MATH_OPT_CONSTRAINTS_SECOND_ORDER_CONE_SECOND_ORDER_CONE_CONSTRAINT_H_

#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/constraints/util/model_util.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/storage/model_storage.h"

namespace operations_research::math_opt {

// A value type that references a second-order cone constraint from
// ModelStorage. Usually this type is passed by copy.
class SecondOrderConeConstraint {
 public:
  // The typed integer used for ids.
  using IdType = SecondOrderConeConstraintId;

  inline SecondOrderConeConstraint(const ModelStorage* storage,
                                   SecondOrderConeConstraintId id);

  inline int64_t id() const;

  inline SecondOrderConeConstraintId typed_id() const;
  inline const ModelStorage* storage() const;

  inline absl::string_view name() const;

  // Returns "upper_bound" with respect to a constraint of the form
  // ||arguments_to_norm||₂ ≤ upper_bound.
  LinearExpression UpperBound() const;

  // Returns "arguments_to_norm" with respect to a constraint of the form
  // ||arguments_to_norm||₂ ≤ upper_bound.
  std::vector<LinearExpression> ArgumentsToNorm() const;

  // Returns all variables that appear in the second-order cone constraint with
  // a nonzero coefficient. Order is not defined.
  inline std::vector<Variable> NonzeroVariables() const;

  // Returns a detailed string description of the contents of the constraint
  // (not its name, use `<<` for that instead).
  std::string ToString() const;

  friend inline bool operator==(const SecondOrderConeConstraint& lhs,
                                const SecondOrderConeConstraint& rhs);
  friend inline bool operator!=(const SecondOrderConeConstraint& lhs,
                                const SecondOrderConeConstraint& rhs);
  template <typename H>
  friend H AbslHashValue(H h, const SecondOrderConeConstraint& constraint);
  friend std::ostream& operator<<(std::ostream& ostr,
                                  const SecondOrderConeConstraint& constraint);

 private:
  const ModelStorage* storage_;
  SecondOrderConeConstraintId id_;
};

// Streams the name of the constraint, as registered upon constraint creation,
// or a short default if none was provided.
inline std::ostream& operator<<(std::ostream& ostr,
                                const SecondOrderConeConstraint& constraint);

////////////////////////////////////////////////////////////////////////////////
// Inline function implementations
////////////////////////////////////////////////////////////////////////////////

int64_t SecondOrderConeConstraint::id() const { return id_.value(); }

SecondOrderConeConstraintId SecondOrderConeConstraint::typed_id() const {
  return id_;
}

const ModelStorage* SecondOrderConeConstraint::storage() const {
  return storage_;
}

absl::string_view SecondOrderConeConstraint::name() const {
  if (storage_->has_constraint(id_)) {
    return storage_->constraint_data(id_).name;
  }
  return kDeletedConstraintDefaultDescription;
}

std::vector<Variable> SecondOrderConeConstraint::NonzeroVariables() const {
  return AtomicConstraintNonzeroVariables(*storage_, id_);
}

bool operator==(const SecondOrderConeConstraint& lhs,
                const SecondOrderConeConstraint& rhs) {
  return lhs.id_ == rhs.id_ && lhs.storage_ == rhs.storage_;
}

bool operator!=(const SecondOrderConeConstraint& lhs,
                const SecondOrderConeConstraint& rhs) {
  return !(lhs == rhs);
}

template <typename H>
H AbslHashValue(H h, const SecondOrderConeConstraint& constraint) {
  return H::combine(std::move(h), constraint.id_.value(), constraint.storage_);
}

std::ostream& operator<<(std::ostream& ostr,
                         const SecondOrderConeConstraint& constraint) {
  // TODO(b/170992529): handle quoting of invalid characters in the name.
  const absl::string_view name = constraint.name();
  if (name.empty()) {
    ostr << "__soc_con#" << constraint.id() << "__";
  } else {
    ostr << name;
  }
  return ostr;
}

SecondOrderConeConstraint::SecondOrderConeConstraint(
    const ModelStorage* const storage, const SecondOrderConeConstraintId id)
    : storage_(storage), id_(id) {}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CONSTRAINTS_SECOND_ORDER_CONE_SECOND_ORDER_CONE_CONSTRAINT_H_
