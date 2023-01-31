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

// An object oriented wrapper for linear constraints in ModelStorage.
#ifndef OR_TOOLS_MATH_OPT_CPP_LINEAR_CONSTRAINT_H_
#define OR_TOOLS_MATH_OPT_CPP_LINEAR_CONSTRAINT_H_

#include <cstdint>
#include <sstream>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "absl/log/check.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/constraints/util/model_util.h"
#include "ortools/math_opt/cpp/id_map.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/key_types.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/storage/model_storage_types.h"

namespace operations_research {
namespace math_opt {

// A value type that references a linear constraint from ModelStorage. Usually
// this type is passed by copy.
class LinearConstraint {
 public:
  // The typed integer used for ids.
  using IdType = LinearConstraintId;

  inline LinearConstraint(const ModelStorage* storage, LinearConstraintId id);

  inline int64_t id() const;

  inline LinearConstraintId typed_id() const;
  inline const ModelStorage* storage() const;

  inline double lower_bound() const;
  inline double upper_bound() const;
  inline absl::string_view name() const;

  inline bool is_coefficient_nonzero(Variable variable) const;

  // Returns 0.0 if the variable is not used in the constraint.
  inline double coefficient(Variable variable) const;

  inline BoundedLinearExpression AsBoundedLinearExpression() const;

  // Returns a detailed string description of the contents of the constraint
  // (not its name, use `<<` for that instead).
  inline std::string ToString() const;

  friend inline bool operator==(const LinearConstraint& lhs,
                                const LinearConstraint& rhs);
  friend inline bool operator!=(const LinearConstraint& lhs,
                                const LinearConstraint& rhs);
  template <typename H>
  friend H AbslHashValue(H h, const LinearConstraint& linear_constraint);
  friend std::ostream& operator<<(std::ostream& ostr,
                                  const LinearConstraint& linear_constraint);

 private:
  const ModelStorage* storage_;
  LinearConstraintId id_;
};

// Implements the API of std::unordered_map<LinearConstraint, V>, but forbids
// LinearConstraints from different models in the same map.
template <typename V>
using LinearConstraintMap = IdMap<LinearConstraint, V>;

// Streams the name of the constraint, as registered upon constraint creation,
// or a short default if none was provided.
inline std::ostream& operator<<(std::ostream& ostr,
                                const LinearConstraint& linear_constraint);

////////////////////////////////////////////////////////////////////////////////
// Inline function implementations
////////////////////////////////////////////////////////////////////////////////

int64_t LinearConstraint::id() const { return id_.value(); }

LinearConstraintId LinearConstraint::typed_id() const { return id_; }

const ModelStorage* LinearConstraint::storage() const { return storage_; }

double LinearConstraint::lower_bound() const {
  return storage_->linear_constraint_lower_bound(id_);
}

double LinearConstraint::upper_bound() const {
  return storage_->linear_constraint_upper_bound(id_);
}

absl::string_view LinearConstraint::name() const {
  if (storage()->has_linear_constraint(id_)) {
    return storage_->linear_constraint_name(id_);
  }
  return kDeletedConstraintDefaultDescription;
}

bool LinearConstraint::is_coefficient_nonzero(const Variable variable) const {
  CHECK_EQ(variable.storage(), storage_)
      << internal::kObjectsFromOtherModelStorage;
  return storage_->is_linear_constraint_coefficient_nonzero(
      id_, variable.typed_id());
}

double LinearConstraint::coefficient(const Variable variable) const {
  CHECK_EQ(variable.storage(), storage_)
      << internal::kObjectsFromOtherModelStorage;
  return storage_->linear_constraint_coefficient(id_, variable.typed_id());
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
  if (!storage()->has_linear_constraint(id_)) {
    return std::string(kDeletedConstraintDefaultDescription);
  }
  std::stringstream str;
  str << AsBoundedLinearExpression();
  return str.str();
}

bool operator==(const LinearConstraint& lhs, const LinearConstraint& rhs) {
  return lhs.id_ == rhs.id_ && lhs.storage_ == rhs.storage_;
}

bool operator!=(const LinearConstraint& lhs, const LinearConstraint& rhs) {
  return !(lhs == rhs);
}

template <typename H>
H AbslHashValue(H h, const LinearConstraint& linear_constraint) {
  return H::combine(std::move(h), linear_constraint.id_.value(),
                    linear_constraint.storage_);
}

std::ostream& operator<<(std::ostream& ostr,
                         const LinearConstraint& linear_constraint) {
  // TODO(b/170992529): handle quoting of invalid characters in the name.
  const absl::string_view name = linear_constraint.name();
  if (name.empty()) {
    ostr << "__lin_con#" << linear_constraint.id() << "__";
  } else {
    ostr << name;
  }
  return ostr;
}

LinearConstraint::LinearConstraint(const ModelStorage* const storage,
                                   const LinearConstraintId id)
    : storage_(storage), id_(id) {}

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CPP_LINEAR_CONSTRAINT_H_
