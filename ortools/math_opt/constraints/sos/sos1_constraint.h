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
#ifndef OR_TOOLS_MATH_OPT_CONSTRAINTS_SOS_SOS1_CONSTRAINT_H_
#define OR_TOOLS_MATH_OPT_CONSTRAINTS_SOS_SOS1_CONSTRAINT_H_

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/constraints/sos/util.h"
#include "ortools/math_opt/constraints/util/model_util.h"
#include "ortools/math_opt/cpp/id_map.h"  // IWYU pragma: export
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/storage/model_storage.h"

namespace operations_research::math_opt {

// A value type that references a SOS1 constraint from ModelStorage.
// Usually this type is passed by copy.
class Sos1Constraint {
 public:
  // The typed integer used for ids.
  using IdType = Sos1ConstraintId;

  inline Sos1Constraint(const ModelStorage* storage, Sos1ConstraintId id);

  inline int64_t id() const;

  inline Sos1ConstraintId typed_id() const;
  inline const ModelStorage* storage() const;

  inline int64_t num_expressions() const;
  LinearExpression Expression(int index) const;
  inline bool has_weights() const;
  inline double weight(int index) const;
  inline absl::string_view name() const;

  // All variables that appear in the SOS1 constraint with a nonzero coefficient
  // in any of the expressions. Order is not defined.
  inline std::vector<Variable> NonzeroVariables() const;

  // Returns a detailed string description of the constraint.
  inline std::string ToString() const;

  friend inline bool operator==(const Sos1Constraint& lhs,
                                const Sos1Constraint& rhs);
  friend inline bool operator!=(const Sos1Constraint& lhs,
                                const Sos1Constraint& rhs);
  template <typename H>
  friend H AbslHashValue(H h, const Sos1Constraint& constraint);
  friend std::ostream& operator<<(std::ostream& ostr,
                                  const Sos1Constraint& constraint);

 private:
  const ModelStorage* storage_;
  Sos1ConstraintId id_;
};

// Implements the API of std::unordered_map<Sos1Constraint, V>, but forbids
// Sos1Constraints from different models in the same map.
template <typename V>
using Sos1ConstraintMap = IdMap<Sos1Constraint, V>;

// Streams the name of the constraint, as registered upon constraint creation,
// or a short default if none was provided.
inline std::ostream& operator<<(std::ostream& ostr,
                                const Sos1Constraint& constraint);

////////////////////////////////////////////////////////////////////////////////
// Inline function implementations
////////////////////////////////////////////////////////////////////////////////

int64_t Sos1Constraint::id() const { return id_.value(); }

Sos1ConstraintId Sos1Constraint::typed_id() const { return id_; }

const ModelStorage* Sos1Constraint::storage() const { return storage_; }

int64_t Sos1Constraint::num_expressions() const {
  return storage_->constraint_data(id_).num_expressions();
}

bool Sos1Constraint::has_weights() const {
  return storage_->constraint_data(id_).has_weights();
}

double Sos1Constraint::weight(int index) const {
  return storage_->constraint_data(id_).weight(index);
}

absl::string_view Sos1Constraint::name() const {
  if (storage_->has_constraint(id_)) {
    return storage_->constraint_data(id_).name();
  }
  return kDeletedConstraintDefaultDescription;
}

std::vector<Variable> Sos1Constraint::NonzeroVariables() const {
  return AtomicConstraintNonzeroVariables(*storage_, id_);
}

bool operator==(const Sos1Constraint& lhs, const Sos1Constraint& rhs) {
  return lhs.id_ == rhs.id_ && lhs.storage_ == rhs.storage_;
}

bool operator!=(const Sos1Constraint& lhs, const Sos1Constraint& rhs) {
  return !(lhs == rhs);
}

template <typename H>
H AbslHashValue(H h, const Sos1Constraint& constraint) {
  return H::combine(std::move(h), constraint.id_.value(), constraint.storage_);
}

std::ostream& operator<<(std::ostream& ostr, const Sos1Constraint& constraint) {
  // TODO(b/170992529): handle quoting of invalid characters in the name.
  const absl::string_view name = constraint.name();
  if (name.empty()) {
    ostr << "__sos1_con#" << constraint.id() << "__";
  } else {
    ostr << name;
  }
  return ostr;
}

std::string Sos1Constraint::ToString() const {
  if (storage_->has_constraint(id_)) {
    return internal::SosConstraintToString(*this, "SOS1");
  }
  return std::string(kDeletedConstraintDefaultDescription);
}

Sos1Constraint::Sos1Constraint(const ModelStorage* const storage,
                               const Sos1ConstraintId id)
    : storage_(storage), id_(id) {}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CONSTRAINTS_SOS_SOS1_CONSTRAINT_H_
