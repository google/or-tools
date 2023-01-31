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

// An object oriented wrapper for quadratic constraints in ModelStorage.
#ifndef OR_TOOLS_MATH_OPT_CONSTRAINTS_QUADRATIC_QUADRATIC_CONSTRAINT_H_
#define OR_TOOLS_MATH_OPT_CONSTRAINTS_QUADRATIC_QUADRATIC_CONSTRAINT_H_

#include <cstdint>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/log/check.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/constraints/util/model_util.h"
#include "ortools/math_opt/cpp/id_map.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/key_types.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"
#include "ortools/math_opt/storage/sparse_matrix.h"

namespace operations_research::math_opt {

// A value type that references a quadratic constraint from ModelStorage.
// Usually this type is passed by copy.
class QuadraticConstraint {
 public:
  // The typed integer used for ids.
  using IdType = QuadraticConstraintId;

  inline QuadraticConstraint(const ModelStorage* storage,
                             QuadraticConstraintId id);

  inline int64_t id() const;

  inline QuadraticConstraintId typed_id() const;
  inline const ModelStorage* storage() const;

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

  BoundedQuadraticExpression AsBoundedQuadraticExpression() const;

  // Returns a detailed string description of the contents of the constraint
  // (not its name, use `<<` for that instead).
  inline std::string ToString() const;

  friend inline bool operator==(const QuadraticConstraint& lhs,
                                const QuadraticConstraint& rhs);
  friend inline bool operator!=(const QuadraticConstraint& lhs,
                                const QuadraticConstraint& rhs);
  template <typename H>
  friend H AbslHashValue(H h, const QuadraticConstraint& quadratic_constraint);
  friend std::ostream& operator<<(
      std::ostream& ostr, const QuadraticConstraint& quadratic_constraint);

 private:
  const ModelStorage* storage_;
  QuadraticConstraintId id_;
};

// Implements the API of std::unordered_map<QuadraticConstraint, V>, but forbids
// QuadraticConstraints from different models in the same map.
template <typename V>
using QuadraticConstraintMap = IdMap<QuadraticConstraint, V>;

// Streams the name of the constraint, as registered upon constraint creation,
// or a short default if none was provided.
inline std::ostream& operator<<(std::ostream& ostr,
                                const QuadraticConstraint& constraint);

////////////////////////////////////////////////////////////////////////////////
// Inline function implementations
////////////////////////////////////////////////////////////////////////////////

int64_t QuadraticConstraint::id() const { return id_.value(); }

QuadraticConstraintId QuadraticConstraint::typed_id() const { return id_; }

const ModelStorage* QuadraticConstraint::storage() const { return storage_; }

double QuadraticConstraint::lower_bound() const {
  return storage_->constraint_data(id_).lower_bound;
}

double QuadraticConstraint::upper_bound() const {
  return storage_->constraint_data(id_).upper_bound;
}

absl::string_view QuadraticConstraint::name() const {
  if (storage_->has_constraint(id_)) {
    return storage_->constraint_data(id_).name;
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
  CHECK_EQ(variable.storage(), storage_)
      << internal::kObjectsFromOtherModelStorage;
  return storage_->constraint_data(id_).linear_terms.get(variable.typed_id());
}

double QuadraticConstraint::quadratic_coefficient(
    const Variable first_variable, const Variable second_variable) const {
  CHECK_EQ(first_variable.storage(), storage_)
      << internal::kObjectsFromOtherModelStorage;
  CHECK_EQ(second_variable.storage(), storage_)
      << internal::kObjectsFromOtherModelStorage;
  return storage_->constraint_data(id_).quadratic_terms.get(
      first_variable.typed_id(), second_variable.typed_id());
}

std::vector<Variable> QuadraticConstraint::NonzeroVariables() const {
  return AtomicConstraintNonzeroVariables(*storage_, id_);
}

std::string QuadraticConstraint::ToString() const {
  if (!storage()->has_constraint(id_)) {
    return std::string(kDeletedConstraintDefaultDescription);
  }
  std::stringstream str;
  str << AsBoundedQuadraticExpression();
  return str.str();
}

bool operator==(const QuadraticConstraint& lhs,
                const QuadraticConstraint& rhs) {
  return lhs.id_ == rhs.id_ && lhs.storage_ == rhs.storage_;
}

bool operator!=(const QuadraticConstraint& lhs,
                const QuadraticConstraint& rhs) {
  return !(lhs == rhs);
}

template <typename H>
H AbslHashValue(H h, const QuadraticConstraint& quadratic_constraint) {
  return H::combine(std::move(h), quadratic_constraint.id_.value(),
                    quadratic_constraint.storage_);
}

std::ostream& operator<<(std::ostream& ostr,
                         const QuadraticConstraint& constraint) {
  // TODO(b/170992529): handle quoting of invalid characters in the name.
  const absl::string_view name = constraint.name();
  if (name.empty()) {
    ostr << "__quad_con#" << constraint.id() << "__";
  } else {
    ostr << name;
  }
  return ostr;
}

QuadraticConstraint::QuadraticConstraint(const ModelStorage* const storage,
                                         const QuadraticConstraintId id)
    : storage_(storage), id_(id) {}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CONSTRAINTS_QUADRATIC_QUADRATIC_CONSTRAINT_H_
