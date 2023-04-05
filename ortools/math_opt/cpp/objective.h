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
//
// An object oriented wrapper for objectives in ModelStorage.
#ifndef OR_TOOLS_MATH_OPT_CPP_OBJECTIVE_H_
#define OR_TOOLS_MATH_OPT_CPP_OBJECTIVE_H_

#include <cstdint>
#include <optional>
#include <ostream>
#include <string>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/cpp/key_types.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/storage/model_storage_types.h"

namespace operations_research::math_opt {

constexpr absl::string_view kDeletedObjectiveDefaultDescription =
    "[objective deleted from model]";

// A value type that references an objective (either primary or auxiliary) from
// ModelStorage. Usually this type is passed by copy.
class Objective {
 public:
  // The type used for ids.
  using IdType = AuxiliaryObjectiveId;

  // Returns an object that refers to the primary objective of the model.
  inline static Objective Primary(const ModelStorage* storage);
  // Returns an object that refers to an auxiliary objective of the model.
  inline static Objective Auxiliary(const ModelStorage* storage,
                                    AuxiliaryObjectiveId id);

  // Returns the raw integer ID associated with the objective: nullopt for the
  // primary objective, a nonnegative int64_t for an auxiliary objective.
  inline std::optional<int64_t> id() const;
  // Returns the strong int ID associated with the objective: nullopt for the
  // primary objective, an AuxiliaryObjectiveId for an auxiliary objective.
  inline ObjectiveId typed_id() const;
  // Returns a const-pointer to the underlying storage object for the model.
  inline const ModelStorage* storage() const;

  // Returns true if the ID corresponds to the primary objective, and false if
  // it is an auxiliary objective.
  inline bool is_primary() const;

  // Returns true if the objective is the maximization sense.
  inline bool maximize() const;

  // Returns the priority (lower is more important) of the objective.
  inline int64_t priority() const;

  // Returns the name of the objective.
  inline absl::string_view name() const;

  // Returns the constant offset of the objective.
  inline double offset() const;

  // Returns the linear coefficient for the variable in the model.
  inline double coefficient(Variable variable) const;
  // Returns the quadratic coefficient for the pair of variables in the model.
  inline double coefficient(Variable first_variable,
                            Variable second_variable) const;

  // Returns true if the variable has a nonzero linear coefficient in the model.
  inline bool is_coefficient_nonzero(Variable variable) const;
  // Returns true if the pair of variables has a nonzero quadratic coefficient
  // in the model.
  inline bool is_coefficient_nonzero(Variable first_variable,
                                     Variable second_variable) const;

  // Returns a representation of the objective as a LinearExpression.
  // NOTE: This will CHECK fail if the objective has quadratic terms.
  LinearExpression AsLinearExpression() const;
  // Returns a representation of the objective as a QuadraticExpression.
  QuadraticExpression AsQuadraticExpression() const;

  // Returns a detailed string description of the contents of the objective
  // (not its name, use `<<` for that instead).
  std::string ToString() const;

  friend inline bool operator==(const Objective& lhs, const Objective& rhs);
  friend inline bool operator!=(const Objective& lhs, const Objective& rhs);
  template <typename H>
  friend H AbslHashValue(H h, const Objective& objective);
  friend std::ostream& operator<<(std::ostream& ostr,
                                  const Objective& objective);

 private:
  inline Objective(const ModelStorage* storage, ObjectiveId id);

  const ModelStorage* storage_;
  ObjectiveId id_;
};

template <typename V>
using ObjectiveMap = absl::flat_hash_map<Objective, V>;

// Streams the name of the objective, as registered upon objective creation, or
// a short default if none was provided.
std::ostream& operator<<(std::ostream& ostr, const Objective& objective);

////////////////////////////////////////////////////////////////////////////////
// Inline function implementations
////////////////////////////////////////////////////////////////////////////////

std::optional<int64_t> Objective::id() const {
  if (is_primary()) {
    return std::nullopt;
  }
  return id_->value();
}

ObjectiveId Objective::typed_id() const { return id_; }

const ModelStorage* Objective::storage() const { return storage_; }

bool Objective::is_primary() const { return id_ == kPrimaryObjectiveId; }

int64_t Objective::priority() const {
  return storage_->objective_priority(id_);
}

bool Objective::maximize() const { return storage_->is_maximize(id_); }

absl::string_view Objective::name() const {
  if (is_primary() || storage_->has_auxiliary_objective(*id_)) {
    return storage_->objective_name(id_);
  }
  return kDeletedObjectiveDefaultDescription;
}

double Objective::offset() const { return storage_->objective_offset(id_); }

double Objective::coefficient(const Variable variable) const {
  CHECK_EQ(variable.storage(), storage_)
      << internal::kObjectsFromOtherModelStorage;
  return storage_->linear_objective_coefficient(id_, variable.typed_id());
}

double Objective::coefficient(const Variable first_variable,
                              const Variable second_variable) const {
  CHECK_EQ(first_variable.storage(), storage_)
      << internal::kObjectsFromOtherModelStorage;
  CHECK_EQ(second_variable.storage(), storage_)
      << internal::kObjectsFromOtherModelStorage;
  return storage_->quadratic_objective_coefficient(
      id_, first_variable.typed_id(), second_variable.typed_id());
}

bool Objective::is_coefficient_nonzero(const Variable variable) const {
  CHECK_EQ(variable.storage(), storage_)
      << internal::kObjectsFromOtherModelStorage;
  return storage_->is_linear_objective_coefficient_nonzero(id_,
                                                           variable.typed_id());
}

bool Objective::is_coefficient_nonzero(const Variable first_variable,
                                       const Variable second_variable) const {
  CHECK_EQ(first_variable.storage(), storage_)
      << internal::kObjectsFromOtherModelStorage;
  CHECK_EQ(second_variable.storage(), storage_)
      << internal::kObjectsFromOtherModelStorage;
  return storage_->is_quadratic_objective_coefficient_nonzero(
      id_, first_variable.typed_id(), second_variable.typed_id());
}

bool operator==(const Objective& lhs, const Objective& rhs) {
  return lhs.id_ == rhs.id_ && lhs.storage_ == rhs.storage_;
}

bool operator!=(const Objective& lhs, const Objective& rhs) {
  return !(lhs == rhs);
}

template <typename H>
H AbslHashValue(H h, const Objective& objective) {
  return H::combine(std::move(h), objective.id_, objective.storage_);
}

Objective::Objective(const ModelStorage* const storage, const ObjectiveId id)
    : storage_(storage), id_(id) {}

Objective Objective::Primary(const ModelStorage* const storage) {
  return Objective(storage, kPrimaryObjectiveId);
}

Objective Objective::Auxiliary(const ModelStorage* const storage,
                               const AuxiliaryObjectiveId id) {
  return Objective(storage, id);
}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CPP_OBJECTIVE_H_
