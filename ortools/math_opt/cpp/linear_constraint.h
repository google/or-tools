// Copyright 2010-2021 Google LLC
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

// An object oriented wrapper for linear constraints in IndexedModel.
#ifndef OR_TOOLS_MATH_OPT_CPP_LINEAR_CONSTRAINT_H_
#define OR_TOOLS_MATH_OPT_CPP_LINEAR_CONSTRAINT_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "ortools/base/logging.h"
#include "ortools/base/int_type.h"
#include "ortools/math_opt/core/indexed_model.h"
#include "ortools/math_opt/cpp/id_map.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/key_types.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"

namespace operations_research {
namespace math_opt {

// A value type that references a linear constraint from IndexedModel. Usually
// this type is passed by copy.
class LinearConstraint {
 public:
  // The typed integer used for ids.
  using IdType = LinearConstraintId;

  inline LinearConstraint(IndexedModel* model, LinearConstraintId id);

  inline int64_t id() const;

  inline LinearConstraintId typed_id() const;
  inline IndexedModel* model() const;

  inline double lower_bound() const;
  inline double upper_bound() const;
  inline const std::string& name() const;

  inline void set_lower_bound(double lower_bound) const;
  inline void set_upper_bound(double upper_bound) const;

  // Setting a value to 0.0 will delete the {constraint, variable} pair from the
  // underlying sparse matrix representation (and has no effect if the pair is
  // not present).
  inline void set_coefficient(Variable variable, double value) const;

  inline bool is_coefficient_nonzero(Variable variable) const;

  // Returns 0.0 if the variable is not used in the constraint.
  inline double coefficient(Variable variable) const;

  std::vector<Variable> RowNonzeros() const;

  BoundedLinearExpression AsBoundedLinearExpression() const;

  friend inline bool operator==(const LinearConstraint& lhs,
                                const LinearConstraint& rhs);
  friend inline bool operator!=(const LinearConstraint& lhs,
                                const LinearConstraint& rhs);
  template <typename H>
  friend H AbslHashValue(H h, const LinearConstraint& linear_constraint);
  friend std::ostream& operator<<(std::ostream& ostr,
                                  const LinearConstraint& linear_constraint);

 private:
  IndexedModel* model_;
  LinearConstraintId id_;
};

// Implements the API of std::unordered_map<LinearConstraint, V>, but forbids
// LinearConstraints from different models in the same map.
template <typename V>
using LinearConstraintMap = IdMap<LinearConstraint, V>;

inline std::ostream& operator<<(std::ostream& ostr,
                                const LinearConstraint& linear_constraint);

////////////////////////////////////////////////////////////////////////////////
// Inline function implementations
////////////////////////////////////////////////////////////////////////////////

int64_t LinearConstraint::id() const { return id_.value(); }

LinearConstraintId LinearConstraint::typed_id() const { return id_; }

IndexedModel* LinearConstraint::model() const { return model_; }

double LinearConstraint::lower_bound() const {
  return model_->linear_constraint_lower_bound(id_);
}
double LinearConstraint::upper_bound() const {
  return model_->linear_constraint_upper_bound(id_);
}
const std::string& LinearConstraint::name() const {
  return model_->linear_constraint_name(id_);
}

void LinearConstraint::set_lower_bound(const double lower_bound) const {
  model_->set_linear_constraint_lower_bound(id_, lower_bound);
}
void LinearConstraint::set_upper_bound(const double upper_bound) const {
  model_->set_linear_constraint_upper_bound(id_, upper_bound);
}
void LinearConstraint::set_coefficient(const Variable variable,
                                       const double value) const {
  CHECK_EQ(variable.model(), model_) << internal::kObjectsFromOtherIndexedModel;
  model_->set_linear_constraint_coefficient(id_, variable.typed_id(), value);
}
bool LinearConstraint::is_coefficient_nonzero(const Variable variable) const {
  CHECK_EQ(variable.model(), model_) << internal::kObjectsFromOtherIndexedModel;
  return model_->is_linear_constraint_coefficient_nonzero(id_,
                                                          variable.typed_id());
}
double LinearConstraint::coefficient(const Variable variable) const {
  CHECK_EQ(variable.model(), model_) << internal::kObjectsFromOtherIndexedModel;
  return model_->linear_constraint_coefficient(id_, variable.typed_id());
}

bool operator==(const LinearConstraint& lhs, const LinearConstraint& rhs) {
  return lhs.id_ == rhs.id_ && lhs.model_ == rhs.model_;
}

bool operator!=(const LinearConstraint& lhs, const LinearConstraint& rhs) {
  return !(lhs == rhs);
}

template <typename H>
H AbslHashValue(H h, const LinearConstraint& linear_constraint) {
  return H::combine(std::move(h), linear_constraint.id_.value(),
                    linear_constraint.model_);
}

std::ostream& operator<<(std::ostream& ostr,
                         const LinearConstraint& linear_constraint) {
  ostr << linear_constraint.name();
  return ostr;
}

LinearConstraint::LinearConstraint(IndexedModel* const model,
                                   const LinearConstraintId id)
    : model_(model), id_(id) {}

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CPP_LINEAR_CONSTRAINT_H_
