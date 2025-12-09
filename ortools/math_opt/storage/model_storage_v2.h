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

#ifndef ORTOOLS_MATH_OPT_STORAGE_MODEL_STORAGE_V2_H_
#define ORTOOLS_MATH_OPT_STORAGE_MODEL_STORAGE_V2_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ortools/math_opt/constraints/indicator/storage.h"  // IWYU pragma: export
#include "ortools/math_opt/constraints/quadratic/storage.h"  // IWYU pragma: export
#include "ortools/math_opt/constraints/sos/storage.h"  // IWYU pragma: export
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/attributes.h"
#include "ortools/math_opt/elemental/derived_data.h"
#include "ortools/math_opt/elemental/elemental.h"
#include "ortools/math_opt/elemental/elements.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/atomic_constraint_storage.h"
#include "ortools/math_opt/storage/atomic_constraints_v2.h"
#include "ortools/math_opt/storage/model_storage_types.h"

namespace operations_research::math_opt {

// An index based C++ API for building & storing optimization problems.
//
// Note that this API should usually not be used by C++ users that should prefer
// the math_opt/cpp/model.h API.
//
// It supports the efficient creation and modification of an optimization model,
// and the export of ModelProto and ModelUpdateProto protos.
//
// All methods run in amortized O(1) (as amortized over calls to that exact
// function) unless otherwise specified.
//
// Incrementalism, the ModelUpdate proto, and Checkpoints:
//
// To update an existing model as specified by a Model proto, solvers consume a
// ModelUpdate proto, which describes the changes to a model (e.g. new variables
// or a change in a variable bound). ModelStorage::NewUpdateTracker() tracks the
// changes made and produces a ModelUpdate proto describing these changes with
// the method ModelStorage::ExportModelUpdate(). The changes returned will be
// the modifications since the previous call to
// ModelStorage::AdvanceCheckpoint(). Note that, for newly initialized models,
// before the first checkpoint, there is no additional memory overhead from
// tracking changes.
// On bad input:
//
// Using a bad variable id or constraint id (an id not in the current model,
// which includes ids that have been deleted) on any method will result in an
// immediate failure (either a CHECK failure or an exception, which is an
// implementation detail you should not rely on). We make no attempt to say if a
// model is invalid (e.g. a variable lower bound is infinite, exceeds an upper
// bound, or is NaN). The exported models are validated instead, see
// model_validator.h.
class ModelStorageV2 {
 public:
  // Returns a storage from the input proto. Returns a failure status if the
  // input proto is invalid.
  //
  // Variable/constraint names can be repeated in the input proto but will be
  // considered invalid when solving.
  //
  // See ApplyUpdateProto() for dealing with subsequent updates.
  static absl::StatusOr<absl_nonnull std::unique_ptr<ModelStorageV2>>
  FromModelProto(const ModelProto& model_proto);

  // Creates an empty minimization problem.
  inline explicit ModelStorageV2(absl::string_view model_name = "",
                                 absl::string_view primary_objective_name = "");

  ModelStorageV2(const ModelStorageV2&) = delete;
  ModelStorageV2& operator=(const ModelStorageV2&) = delete;

  // Returns a clone of the model, optionally changing model's name.
  //
  // The variables and constraints have the same ids. The clone will also not
  // reused any id of variable/constraint that was deleted in the original.
  //
  // Note that the returned model does not have any update tracker.
  absl_nonnull std::unique_ptr<ModelStorageV2> Clone(
      std::optional<absl::string_view> new_name = std::nullopt) const;

  inline const std::string& name() const { return elemental_.model_name(); }

  //////////////////////////////////////////////////////////////////////////////
  // Variables
  //////////////////////////////////////////////////////////////////////////////

  // Adds a continuous unbounded variable to the model and returns its id.
  //
  // See AddVariable(double, double, bool, absl::string_view) for details.
  inline VariableId AddVariable(absl::string_view name = "");

  // Adds a variable to the model and returns its id.
  //
  // The returned ids begin at zero and increase by one with each call to
  // AddVariable. Deleted ids are NOT reused. If no variables are deleted,
  // the ids in the model will be consecutive.
  inline VariableId AddVariable(double lower_bound, double upper_bound,
                                bool is_integer, absl::string_view name = "");

  inline double variable_lower_bound(VariableId id) const;
  inline double variable_upper_bound(VariableId id) const;
  inline bool is_variable_integer(VariableId id) const;
  inline absl::string_view variable_name(VariableId id) const;

  inline void set_variable_lower_bound(VariableId id, double lower_bound);
  inline void set_variable_upper_bound(VariableId id, double upper_bound);
  inline void set_variable_is_integer(VariableId id, bool is_integer);
  inline void set_variable_as_integer(VariableId id);
  inline void set_variable_as_continuous(VariableId id);

  // Removes a variable from the model.
  //
  // It is an error to use a deleted variable id as input to any subsequent
  // function calls on the model. Runs in O(#constraints containing the
  // variable).
  void DeleteVariable(VariableId id);

  // The number of variables in the model.
  //
  // Equal to the number of variables created minus the number of variables
  // deleted.
  inline int64_t num_variables() const;

  // The returned id of the next call to AddVariable.
  //
  // Equal to the number of variables created.
  inline VariableId next_variable_id() const;

  // Sets the next variable id to be the maximum of next_variable_id() and id.
  inline void ensure_next_variable_id_at_least(VariableId id);

  // Returns true if this id has been created and not yet deleted.
  inline bool has_variable(VariableId id) const;

  ABSL_DEPRECATED("Use Variables() instead")
  std::vector<VariableId> variables() const { return Variables(); }

  // The VariableIds in use (not deleted), order not defined.
  std::vector<VariableId> Variables() const;

  // Returns a sorted vector of all existing (not deleted) variables in the
  // model.
  //
  // Runs in O(n log(n)), where n is the number of variables returned.
  std::vector<VariableId> SortedVariables() const;

  //////////////////////////////////////////////////////////////////////////////
  // Linear Constraints
  //////////////////////////////////////////////////////////////////////////////

  // Adds a linear constraint to the model with a lower bound of -inf and an
  // upper bound of +inf and returns its id.
  //
  // See AddLinearConstraint(double, double, absl::string_view) for details.
  inline LinearConstraintId AddLinearConstraint(absl::string_view name = "");

  // Adds a linear constraint to the model returns its id.
  //
  // The returned ids begin at zero and increase by one with each call to
  // AddLinearConstraint. Deleted ids are NOT reused. If no linear
  // constraints are deleted, the ids in the model will be consecutive.
  inline LinearConstraintId AddLinearConstraint(double lower_bound,
                                                double upper_bound,
                                                absl::string_view name = "");

  inline double linear_constraint_lower_bound(LinearConstraintId id) const;
  inline double linear_constraint_upper_bound(LinearConstraintId id) const;
  inline absl::string_view linear_constraint_name(LinearConstraintId id) const;

  inline void set_linear_constraint_lower_bound(LinearConstraintId id,
                                                double lower_bound);
  inline void set_linear_constraint_upper_bound(LinearConstraintId id,
                                                double upper_bound);

  // Removes a linear constraint from the model.
  //
  // It is an error to use a deleted linear constraint id as input to any
  // subsequent function calls on the model. Runs in O(#variables in the linear
  // constraint).
  void DeleteLinearConstraint(LinearConstraintId id);

  // The number of linear constraints in the model.
  //
  // Equal to the number of linear constraints created minus the number of
  // linear constraints deleted.
  inline int64_t num_linear_constraints() const;

  // The returned id of the next call to AddLinearConstraint.
  //
  // Equal to the number of linear constraints created.
  inline LinearConstraintId next_linear_constraint_id() const;

  // Sets the next linear constraint id to be the maximum of
  // next_linear_constraint_id() and id.
  inline void ensure_next_linear_constraint_id_at_least(LinearConstraintId id);

  // Returns true if this id has been created and not yet deleted.
  inline bool has_linear_constraint(LinearConstraintId id) const;

  // The LinearConstraintsIds in use (not deleted), order not defined.
  std::vector<LinearConstraintId> LinearConstraints() const;

  // Returns a sorted vector of all existing (not deleted) linear constraints in
  // the model.
  //
  // Runs in O(n log(n)), where n is the number of linear constraints returned.
  std::vector<LinearConstraintId> SortedLinearConstraints() const;

  //////////////////////////////////////////////////////////////////////////////
  // Linear constraint matrix
  //////////////////////////////////////////////////////////////////////////////

  // Returns 0.0 if the entry is not in matrix.
  inline double linear_constraint_coefficient(LinearConstraintId constraint,
                                              VariableId variable) const;
  inline bool is_linear_constraint_coefficient_nonzero(
      LinearConstraintId constraint, VariableId variable) const;

  // Setting a value to 0.0 will delete the {constraint, variable} pair from the
  // underlying sparse matrix representation (and has no effect if the pair is
  // not present).
  inline void set_linear_constraint_coefficient(LinearConstraintId constraint,
                                                VariableId variable,
                                                double value);

  // The {linear constraint, variable, coefficient} tuples with nonzero linear
  // constraint matrix coefficients.
  inline std::vector<std::tuple<LinearConstraintId, VariableId, double>>
  linear_constraint_matrix() const;

  // Returns the variables with nonzero coefficients in a linear constraint.
  inline std::vector<VariableId> variables_in_linear_constraint(
      LinearConstraintId constraint) const;

  // Returns the linear constraints with nonzero coefficients on a variable.
  inline std::vector<LinearConstraintId> linear_constraints_with_variable(
      VariableId variable) const;

  //////////////////////////////////////////////////////////////////////////////
  // Objectives
  //
  // The primary ObjectiveId is `PrimaryObjectiveId`. All auxiliary objectives
  // are referenced by their corresponding `AuxiliaryObjectiveId`.
  //////////////////////////////////////////////////////////////////////////////

  inline bool is_maximize(ObjectiveId id) const;
  inline int64_t objective_priority(ObjectiveId id) const;
  inline double objective_offset(ObjectiveId id) const;
  // Returns 0.0 if this variable has no linear objective coefficient.
  inline double linear_objective_coefficient(ObjectiveId id,
                                             VariableId variable) const;
  // The ordering of the input variables does not matter.
  inline double quadratic_objective_coefficient(
      ObjectiveId id, VariableId first_variable,
      VariableId second_variable) const;
  inline bool is_linear_objective_coefficient_nonzero(
      ObjectiveId id, VariableId variable) const;
  // The ordering of the input variables does not matter.
  inline bool is_quadratic_objective_coefficient_nonzero(
      ObjectiveId id, VariableId first_variable,
      VariableId second_variable) const;
  inline absl::string_view objective_name(ObjectiveId id) const;

  inline void set_is_maximize(ObjectiveId id, bool is_maximize);
  inline void set_maximize(ObjectiveId id);
  inline void set_minimize(ObjectiveId id);
  inline void set_objective_priority(ObjectiveId id, int64_t value);
  inline void set_objective_offset(ObjectiveId id, double value);

  // Setting a value to 0.0 will delete the variable from the underlying sparse
  // representation (and has no effect if the variable is not present).
  inline void set_linear_objective_coefficient(ObjectiveId id,
                                               VariableId variable,
                                               double value);
  // Setting a value to 0.0 will delete the variable pair from the underlying
  // sparse representation (and has no effect if the pair is not present).
  // The ordering of the input variables does not matter.
  inline void set_quadratic_objective_coefficient(ObjectiveId id,
                                                  VariableId first_variable,
                                                  VariableId second_variable,
                                                  double value);

  // Equivalent to calling set_linear_objective_coefficient(v, 0.0) for every
  // variable with nonzero objective coefficient, and setting the offset and
  // quadratic terms to zero as well. Does not effect priority or direction.
  //
  // Runs in O(# nonzero linear/quadratic objective terms).
  inline void clear_objective(ObjectiveId id);

  // The variables with nonzero linear objective coefficients.
  ABSL_DEPRECATED("Use LinearObjectiveNonzeros instead")
  inline const absl::flat_hash_map<VariableId, double>& linear_objective(
      ObjectiveId id) const;

  // Returns the variable ids where the objective has a nonzero linear
  // objective coefficient in an arbitrary order.
  std::vector<VariableId> LinearObjectiveNonzeros(ObjectiveId id) const;

  inline int64_t num_linear_objective_terms(ObjectiveId id) const;

  inline int64_t num_quadratic_objective_terms(ObjectiveId id) const;

  // The variable pairs with nonzero quadratic objective coefficients. The keys
  // are ordered such that .first <= .second. All values are nonempty.
  //
  // TODO(b/233630053) do no allocate the result, expose an iterator API.
  inline std::vector<std::tuple<VariableId, VariableId, double>>
  quadratic_objective_terms(ObjectiveId id) const;

  //////////////////////////////////////////////////////////////////////////////
  // Auxiliary objectives
  //////////////////////////////////////////////////////////////////////////////

  // Adds an auxiliary objective to the model and returns its id.
  //
  // The returned ids begin at zero and increase by one with each call to
  // AddAuxiliaryObjective. Deleted ids are NOT reused. If no auxiliary
  // objectives are deleted, the ids in the model will be consecutive.
  //
  // Objectives are minimized by default; you can change the sense via, e.g.,
  // `set_is_maximize()`.
  inline AuxiliaryObjectiveId AddAuxiliaryObjective(
      int64_t priority, absl::string_view name = "");

  // Removes an auxiliary objective from the model.
  //
  // It is an error to use a deleted auxiliary objective id as input to any
  // subsequent function calls on the model. Runs in O(#variables in the
  // auxiliary objective).
  inline void DeleteAuxiliaryObjective(AuxiliaryObjectiveId id);

  // The number of auxiliary objectives in the model.
  //
  // Equal to the number of auxiliary objectives created minus the number of
  // auxiliary objectives deleted.
  inline int num_auxiliary_objectives() const;

  // The returned id of the next call to AddAuxiliaryObjective.
  //
  // Equal to the number of auxiliary objectives created.
  inline AuxiliaryObjectiveId next_auxiliary_objective_id() const;

  // Sets the next auxiliary objective id to be the maximum of
  // next_auxiliary_objective_id() and id.
  inline void ensure_next_auxiliary_objective_id_at_least(
      AuxiliaryObjectiveId id);

  // Returns true if this id has been created and not yet deleted.
  inline bool has_auxiliary_objective(AuxiliaryObjectiveId id) const;

  // The AuxiliaryObjectiveIds in use (not deleted), order not defined.
  inline std::vector<AuxiliaryObjectiveId> AuxiliaryObjectives() const;

  // Returns a sorted vector of all existing (not deleted) auxiliary objectives
  // in the model.
  //
  // Runs in O(n log(n)), where n is the number of auxiliary objectives
  // returned.
  inline std::vector<AuxiliaryObjectiveId> SortedAuxiliaryObjectives() const;

  //////////////////////////////////////////////////////////////////////////////
  // Atomic Constraints
  //
  // These methods do not directly require template specializations to add
  // support for new constraint families; this should be handled automatically
  // upon adding a specialization for `AtomicConstraintTraits`.
  //////////////////////////////////////////////////////////////////////////////

  // Adds an atomic constraint to the model and returns its id.
  //
  // The returned ids begin at zero and increase by one with each call to
  // `AddAtomicConstraint<ConstraintData>`. Deleted ids are NOT reused. Callers
  // may use `ensure_next_constraint_id_at_least<ConstraintData>` to configure
  // custom indices.
  template <typename ConstraintData>
  inline typename ConstraintData::IdType AddAtomicConstraint(
      ConstraintData data);

  // Removes an atomic constraint from the model.
  //
  // It is an error to use a deleted constraint id as input to any subsequent
  // function calls on the model. Runs in O(#variables in the constraint).
  template <typename IdType>
  inline void DeleteAtomicConstraint(IdType id);

  // Accesses the data object that fully represents a single atomic constraint.
  template <typename IdType>
  ABSL_DEPRECATED("Prefer GetConstraintData")
  inline const
      typename AtomicConstraintTraits<IdType>::ConstraintData& constraint_data(
          IdType id) const;

  template <typename IdType>
  inline typename AtomicConstraintTraits<IdType>::ConstraintData
  GetConstraintData(IdType id) const;

  // Returns the number of atomic constraints in the model of the family
  // corresponding to `ConstraintData`.
  //
  // Equal to the number of such constraints created minus the number of such
  // constraints deleted.
  template <typename IdType>
  inline int64_t num_constraints() const;

  // Returns the smallest valid ID for a new atomic constraint of the family
  // corresponding to `ConstraintData`.
  template <typename IdType>
  inline IdType next_constraint_id() const;

  // Sets the next atomic constraint id of the family corresponding to
  // `ConstraintData` to be the maximum of
  // `next_constraint_id<ConstraintData>()` and `id`.
  template <typename IdType>
  inline void ensure_next_constraint_id_at_least(IdType id);

  // Returns true if this id has been created and not yet deleted.
  template <typename IdType>
  inline bool has_constraint(IdType id) const;

  // Returns the constraint IDs in use (not deleted); order is not defined.
  template <typename IdType>
  std::vector<IdType> Constraints() const;

  // Returns a sorted vector of all existing (not deleted) atomic
  // constraints in the model of the family corresponding to
  // `ConstraintData`.
  //
  // Runs in O(n log(n)), where n is the number of constraints returned.
  template <typename IdType>
  std::vector<IdType> SortedConstraints() const;

  // Returns the constraint in the given family in which the variable appears
  // structurally (i.e., has a coefficient, possibly zero). Order is not
  // defined.
  template <typename IdType>
  ABSL_DEPRECATED("Will be deleted when Elemental turns on")
  inline std::vector<IdType> ConstraintsWithVariable(
      VariableId variable_id) const;

  // Returns the variables appearing in the constraint. Order is not defined.
  template <typename IdType>
  ABSL_DEPRECATED("Will be deleted when Elemental turns on")
  inline std::vector<VariableId> VariablesInConstraint(IdType id) const;

  //////////////////////////////////////////////////////////////////////////////
  // Export
  //////////////////////////////////////////////////////////////////////////////

  // Returns a proto representation of the optimization model.
  //
  // Returns an error if the model is too large to fit in a proto (requires
  // putting more than 2**31 -1 elements in a RepeatedField).
  //
  // See FromModelProto() to build a ModelStorage from a proto.
  absl::StatusOr<ModelProto> ExportModelV2(bool remove_names = false) const;

  ABSL_DEPRECATED("Use ExportModelV2 instead")
  ModelProto ExportModel(const bool remove_names = false) const {
    auto result = ExportModelV2(remove_names);
    CHECK_OK(result);
    return *std::move(result);
  }

  // Creates a tracker that can be used to generate a ModelUpdateProto with the
  // updates that happened since the last checkpoint. The tracker initial
  // checkpoint corresponds to the current state of the model.
  //
  // Thread-safety: this method must not be used while modifying the
  // ModelStorage. The user is expected to use proper synchronization primitive
  // to serialize changes to the model and trackers creations. That said
  // multiple trackers can be created concurrently.
  //
  // For each update tracker we define a checkpoint that is the starting point
  // used to compute the ModelUpdateProto.
  UpdateTrackerId NewUpdateTracker();

  // Deletes the input tracker.
  //
  // It must not be used anymore after its destruction. It can be deleted once,
  // trying to delete it a second time or use it will raise an assertion
  // (CHECK).
  //
  // The update trackers are automatically deleted when the ModelStorage is
  // destroyed. Calling this function is thus only useful for performance
  // reasons, to ensure the ModelStorage does not keep data for update trackers
  // that are not needed anymore.
  //
  // Thread-safety: this method can be called at any time, even during the
  // creation of other trackers or during model modification. It must not be
  // called concurrently with ExportModelUpdate() or AdvanceCheckpoint() though.
  void DeleteUpdateTracker(UpdateTrackerId update_tracker);

  // Returns a proto representation of the changes to the model since the most
  // recent checkpoint (i.e. last time AdvanceCheckpoint() was called); nullopt
  // if the update would have been empty.
  //
  // Thread-safety: this method must not be used while modifying the
  // ModelStorage or after calling DeleteUpdateTracker(). The user is expected
  // to use proper synchronization primitive to serialize changes to the model
  // and the use of this method.
  //
  // It can be called concurrently for different update trackers though.
  absl::StatusOr<std::optional<ModelUpdateProto>> ExportModelUpdateV2(
      UpdateTrackerId update_tracker, bool remove_names = false) const;

  ABSL_DEPRECATED("Use ExportUpdateModelV2 instead")
  std::optional<ModelUpdateProto> ExportModelUpdate(
      const UpdateTrackerId update_tracker,
      const bool remove_names = false) const {
    auto result = ExportModelUpdateV2(update_tracker, remove_names);
    CHECK_OK(result);
    return *std::move(result);
  }

  // Uses the current model state as the starting point to calculate the
  // ModelUpdateProto next time ExportModelUpdate() is called.
  //
  // Thread-safety: this method must not be used while modifying the
  // ModelStorage or after calling DeleteUpdateTracker(). The user is expected
  // to use proper synchronization primitive to serialize changes to the model
  // and the use of this method.
  //
  // It can be called concurrently for different update trackers though.
  void AdvanceCheckpoint(UpdateTrackerId update_tracker);

  // Apply the provided update to this model. Returns a failure if the update is
  // not valid.
  //
  // As with FromModelProto(), duplicated names are ignored.
  //
  // It takes O(num_variables + num_constraints) extra memory and execution to
  // apply the update (due to the need to build a ModelSummary). So even a small
  // update will have some cost.
  absl::Status ApplyUpdateProto(const ModelUpdateProto& update_proto);

 private:
  explicit ModelStorageV2(Elemental elemental)
      : elemental_(std::move(elemental)) {
    CHECK_EQ(elemental_.NumDiffs(), 0);
  }

  Elemental elemental_;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Inlined function implementations
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

ModelStorageV2::ModelStorageV2(const absl::string_view model_name,
                               const absl::string_view primary_objective_name)
    : elemental_(std::string(model_name), std::string(primary_objective_name)) {
}

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////

VariableId ModelStorageV2::AddVariable(const absl::string_view name) {
  return VariableId{
      elemental_.AddElement<ElementType::kVariable>(name).value()};
}

VariableId ModelStorageV2::AddVariable(const double lower_bound,
                                       const double upper_bound,
                                       const bool is_integer,
                                       const absl::string_view name) {
  const VariableId id = AddVariable(name);
  set_variable_lower_bound(id, lower_bound);
  set_variable_upper_bound(id, upper_bound);
  set_variable_is_integer(id, is_integer);
  return id;
}

double ModelStorageV2::variable_lower_bound(const VariableId id) const {
  return elemental_.GetAttr(DoubleAttr1::kVarLb, AttrKey(id.value()));
}

double ModelStorageV2::variable_upper_bound(const VariableId id) const {
  return elemental_.GetAttr(DoubleAttr1::kVarUb, AttrKey(id.value()));
}

bool ModelStorageV2::is_variable_integer(const VariableId id) const {
  return elemental_.GetAttr(BoolAttr1::kVarInteger, AttrKey(id.value()));
}

absl::string_view ModelStorageV2::variable_name(const VariableId id) const {
  const auto name = elemental_.GetElementName(id);
  CHECK_OK(name);
  return *name;
}

void ModelStorageV2::set_variable_lower_bound(const VariableId id,
                                              const double lower_bound) {
  elemental_.SetAttr(DoubleAttr1::kVarLb, AttrKey(id.value()), lower_bound);
}

void ModelStorageV2::set_variable_upper_bound(const VariableId id,
                                              const double upper_bound) {
  elemental_.SetAttr(DoubleAttr1::kVarUb, AttrKey(id.value()), upper_bound);
}

void ModelStorageV2::set_variable_is_integer(const VariableId id,
                                             const bool is_integer) {
  elemental_.SetAttr(BoolAttr1::kVarInteger, AttrKey(id.value()), is_integer);
}

void ModelStorageV2::set_variable_as_integer(const VariableId id) {
  set_variable_is_integer(id, true);
}

void ModelStorageV2::set_variable_as_continuous(const VariableId id) {
  set_variable_is_integer(id, false);
}

int64_t ModelStorageV2::num_variables() const {
  return elemental_.NumElements(ElementType::kVariable);
}

VariableId ModelStorageV2::next_variable_id() const {
  return VariableId{elemental_.NextElementId(ElementType::kVariable)};
}

void ModelStorageV2::ensure_next_variable_id_at_least(const VariableId id) {
  elemental_.EnsureNextElementIdAtLeast(id);
}

bool ModelStorageV2::has_variable(const VariableId id) const {
  return elemental_.ElementExists(id);
}

////////////////////////////////////////////////////////////////////////////////
// Linear Constraints
////////////////////////////////////////////////////////////////////////////////

LinearConstraintId ModelStorageV2::AddLinearConstraint(
    const absl::string_view name) {
  return elemental_.AddElement<ElementType::kLinearConstraint>(name);
}

LinearConstraintId ModelStorageV2::AddLinearConstraint(
    const double lower_bound, const double upper_bound,
    const absl::string_view name) {
  const LinearConstraintId id = AddLinearConstraint(name);
  set_linear_constraint_lower_bound(id, lower_bound);
  set_linear_constraint_upper_bound(id, upper_bound);
  return id;
}

double ModelStorageV2::linear_constraint_lower_bound(
    const LinearConstraintId id) const {
  return elemental_.GetAttr(DoubleAttr1::kLinConLb, AttrKey(id.value()));
}

double ModelStorageV2::linear_constraint_upper_bound(
    const LinearConstraintId id) const {
  return elemental_.GetAttr(DoubleAttr1::kLinConUb, AttrKey(id.value()));
}

absl::string_view ModelStorageV2::linear_constraint_name(
    const LinearConstraintId id) const {
  const auto name = elemental_.GetElementName(id);
  CHECK_OK(name);
  return *name;
}

void ModelStorageV2::set_linear_constraint_lower_bound(
    const LinearConstraintId id, const double lower_bound) {
  elemental_.SetAttr(DoubleAttr1::kLinConLb, AttrKey(id.value()), lower_bound);
}

void ModelStorageV2::set_linear_constraint_upper_bound(
    const LinearConstraintId id, const double upper_bound) {
  elemental_.SetAttr(DoubleAttr1::kLinConUb, AttrKey(id.value()), upper_bound);
}

int64_t ModelStorageV2::num_linear_constraints() const {
  return elemental_.NumElements(ElementType::kLinearConstraint);
}

LinearConstraintId ModelStorageV2::next_linear_constraint_id() const {
  return LinearConstraintId{
      elemental_.NextElementId(ElementType::kLinearConstraint)};
}

void ModelStorageV2::ensure_next_linear_constraint_id_at_least(
    const LinearConstraintId id) {
  elemental_.EnsureNextElementIdAtLeast(id);
}

bool ModelStorageV2::has_linear_constraint(const LinearConstraintId id) const {
  return elemental_.ElementExists(id);
}

////////////////////////////////////////////////////////////////////////////////
// Linear Constraint Matrix
////////////////////////////////////////////////////////////////////////////////

double ModelStorageV2::linear_constraint_coefficient(
    const LinearConstraintId constraint, const VariableId variable) const {
  return elemental_.GetAttr(DoubleAttr2::kLinConCoef,
                            AttrKey(constraint.value(), variable.value()));
}

bool ModelStorageV2::is_linear_constraint_coefficient_nonzero(
    const LinearConstraintId constraint, const VariableId variable) const {
  return elemental_.AttrIsNonDefault(
      DoubleAttr2::kLinConCoef, AttrKey(constraint.value(), variable.value()));
}

void ModelStorageV2::set_linear_constraint_coefficient(
    const LinearConstraintId constraint, const VariableId variable,
    const double value) {
  elemental_.SetAttr(DoubleAttr2::kLinConCoef,
                     AttrKey(constraint.value(), variable.value()), value);
}

std::vector<std::tuple<LinearConstraintId, VariableId, double>>
ModelStorageV2::linear_constraint_matrix() const {
  std::vector<std::tuple<LinearConstraintId, VariableId, double>> result;
  result.reserve(elemental_.AttrNumNonDefaults(DoubleAttr2::kLinConCoef));
  for (const auto attr_key :
       elemental_.AttrNonDefaults(DoubleAttr2::kLinConCoef)) {
    result.push_back({LinearConstraintId{attr_key[0]}, VariableId{attr_key[1]},
                      elemental_.GetAttr(DoubleAttr2::kLinConCoef, attr_key)});
  }
  return result;
}

std::vector<VariableId> ModelStorageV2::variables_in_linear_constraint(
    const LinearConstraintId constraint) const {
  const std::vector<AttrKeyFor<DoubleAttr2>> slice =
      elemental_.Slice<0>(DoubleAttr2::kLinConCoef, constraint.value());
  std::vector<VariableId> result;
  result.reserve(slice.size());
  for (const auto key : slice) {
    result.push_back(VariableId{key[1]});
  }
  return result;
}

std::vector<LinearConstraintId>
ModelStorageV2::linear_constraints_with_variable(
    const VariableId variable) const {
  const std::vector<AttrKeyFor<DoubleAttr2>> slice =
      elemental_.Slice<1>(DoubleAttr2::kLinConCoef, variable.value());
  std::vector<LinearConstraintId> result;
  result.reserve(slice.size());
  for (const auto key : slice) {
    result.push_back(LinearConstraintId{key[0]});
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// Objectives
////////////////////////////////////////////////////////////////////////////////

bool ModelStorageV2::is_maximize(const ObjectiveId id) const {
  return id.has_value() ? elemental_.GetAttr(BoolAttr1::kAuxObjMaximize,
                                             AttrKey(id->value()))
                        : elemental_.GetAttr(BoolAttr0::kMaximize, AttrKey());
}

int64_t ModelStorageV2::objective_priority(const ObjectiveId id) const {
  return id.has_value() ? elemental_.GetAttr(IntAttr1::kAuxObjPriority,
                                             AttrKey(id->value()))
                        : elemental_.GetAttr(IntAttr0::kObjPriority, AttrKey());
}

double ModelStorageV2::objective_offset(const ObjectiveId id) const {
  return id.has_value()
             ? elemental_.GetAttr(DoubleAttr1::kAuxObjOffset,
                                  AttrKey(id->value()))
             : elemental_.GetAttr(DoubleAttr0::kObjOffset, AttrKey());
}

double ModelStorageV2::linear_objective_coefficient(
    const ObjectiveId id, const VariableId variable) const {
  return id.has_value()
             ? elemental_.GetAttr(DoubleAttr2::kAuxObjLinCoef,
                                  AttrKey(id->value(), variable.value()))
             : elemental_.GetAttr(DoubleAttr1::kObjLinCoef,
                                  AttrKey(variable.value()));
}

double ModelStorageV2::quadratic_objective_coefficient(
    const ObjectiveId id, const VariableId first_variable,
    const VariableId second_variable) const {
  CHECK(!id.has_value()) << "multiple objectives not supported";
  return elemental_.GetAttr(
      SymmetricDoubleAttr2::kObjQuadCoef,
      AttrKeyFor<SymmetricDoubleAttr2>(first_variable.value(),
                                       second_variable.value()));
}

bool ModelStorageV2::is_linear_objective_coefficient_nonzero(
    const ObjectiveId id, const VariableId variable) const {
  return id.has_value()
             ? elemental_.AttrIsNonDefault(
                   DoubleAttr2::kAuxObjLinCoef,
                   AttrKey(id->value(), variable.value()))
             : elemental_.AttrIsNonDefault(DoubleAttr1::kObjLinCoef,
                                           AttrKey(variable.value()));
}

bool ModelStorageV2::is_quadratic_objective_coefficient_nonzero(
    const ObjectiveId id, const VariableId first_variable,
    const VariableId second_variable) const {
  CHECK(!id.has_value()) << "multiple objectives not supported";
  return elemental_.AttrIsNonDefault(
      SymmetricDoubleAttr2::kObjQuadCoef,
      AttrKeyFor<SymmetricDoubleAttr2>(first_variable.value(),
                                       second_variable.value()));
}

absl::string_view ModelStorageV2::objective_name(const ObjectiveId id) const {
  if (!id.has_value()) return elemental_.primary_objective_name();
  const auto name =
      elemental_.GetElementName(AuxiliaryObjectiveId(id->value()));
  CHECK_OK(name);
  return *name;
}

void ModelStorageV2::set_is_maximize(const ObjectiveId id,
                                     const bool is_maximize) {
  if (id.has_value()) {
    elemental_.SetAttr(BoolAttr1::kAuxObjMaximize, AttrKey(id->value()),
                       is_maximize);
  } else {
    elemental_.SetAttr(BoolAttr0::kMaximize, AttrKey(), is_maximize);
  }
}

void ModelStorageV2::set_maximize(const ObjectiveId id) {
  set_is_maximize(id, true);
}

void ModelStorageV2::set_minimize(const ObjectiveId id) {
  set_is_maximize(id, false);
}

void ModelStorageV2::set_objective_priority(const ObjectiveId id,
                                            const int64_t value) {
  if (id.has_value()) {
    elemental_.SetAttr(IntAttr1::kAuxObjPriority, AttrKey(id->value()), value);
  } else {
    elemental_.SetAttr(IntAttr0::kObjPriority, AttrKey(), value);
  }
}

void ModelStorageV2::set_objective_offset(const ObjectiveId id,
                                          const double value) {
  if (id.has_value()) {
    elemental_.SetAttr(DoubleAttr1::kAuxObjOffset, AttrKey(id->value()), value);
  } else {
    elemental_.SetAttr(DoubleAttr0::kObjOffset, AttrKey(), value);
  }
}

void ModelStorageV2::set_linear_objective_coefficient(const ObjectiveId id,
                                                      const VariableId variable,
                                                      const double value) {
  if (id.has_value()) {
    elemental_.SetAttr(DoubleAttr2::kAuxObjLinCoef,
                       AttrKey(id->value(), variable.value()), value);
  } else {
    elemental_.SetAttr(DoubleAttr1::kObjLinCoef, AttrKey(variable.value()),
                       value);
  }
}

void ModelStorageV2::set_quadratic_objective_coefficient(
    const ObjectiveId id, const VariableId first_variable,
    const VariableId second_variable, const double value) {
  CHECK(!id.has_value()) << "multiple objectives not supported";
  elemental_.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef,
                     AttrKeyFor<SymmetricDoubleAttr2>(first_variable.value(),
                                                      second_variable.value()),
                     value);
}

void ModelStorageV2::clear_objective(const ObjectiveId id) {
  if (id.has_value()) {
    // TODO(b/372645273): Consider adding a `ResetAttr()` method.
    elemental_.SetAttr(IntAttr1::kAuxObjPriority, AttrKey(id->value()),
                       GetAttrDefaultValue<IntAttr1::kAuxObjPriority>());
    elemental_.SetAttr(DoubleAttr1::kAuxObjOffset, AttrKey(id->value()),
                       GetAttrDefaultValue<DoubleAttr1::kAuxObjOffset>());
    // TODO(b/372645273): Consider adding a `ClearSlice()` method.
    for (const auto key :
         elemental_.Slice<0>(DoubleAttr2::kAuxObjLinCoef, id->value())) {
      elemental_.SetAttr(DoubleAttr2::kAuxObjLinCoef, key,
                         GetAttrDefaultValue<DoubleAttr2::kAuxObjLinCoef>());
    }
  } else {
    elemental_.AttrClear(IntAttr0::kObjPriority);
    elemental_.AttrClear(DoubleAttr0::kObjOffset);
    elemental_.AttrClear(DoubleAttr1::kObjLinCoef);
  }
}

const absl::flat_hash_map<VariableId, double>& ModelStorageV2::linear_objective(
    const ObjectiveId id) const {
  LOG(FATAL) << "cannot be implemented";
}

int64_t ModelStorageV2::num_linear_objective_terms(const ObjectiveId id) const {
  if (id.has_value()) {
    return elemental_.GetSliceSize<0>(DoubleAttr2::kAuxObjLinCoef, id->value());
  } else {
    return elemental_.AttrNumNonDefaults(DoubleAttr1::kObjLinCoef);
  }
}

int64_t ModelStorageV2::num_quadratic_objective_terms(
    const ObjectiveId id) const {
  CHECK(!id.has_value()) << "multiple objectives not supported";
  return elemental_.AttrNumNonDefaults(SymmetricDoubleAttr2::kObjQuadCoef);
}

std::vector<std::tuple<VariableId, VariableId, double>>
ModelStorageV2::quadratic_objective_terms(const ObjectiveId id) const {
  CHECK(!id.has_value()) << "multiple objectives not supported";
  std::vector<std::tuple<VariableId, VariableId, double>> result;
  result.reserve(
      elemental_.AttrNumNonDefaults(SymmetricDoubleAttr2::kObjQuadCoef));
  for (const auto attr_key :
       elemental_.AttrNonDefaults(SymmetricDoubleAttr2::kObjQuadCoef)) {
    result.push_back(
        {VariableId(attr_key[0]), VariableId(attr_key[1]),
         elemental_.GetAttr(SymmetricDoubleAttr2::kObjQuadCoef, attr_key)});
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// Auxiliary objectives
////////////////////////////////////////////////////////////////////////////////

AuxiliaryObjectiveId ModelStorageV2::AddAuxiliaryObjective(
    const int64_t priority, const absl::string_view name) {
  const auto id = elemental_.AddElement<ElementType::kAuxiliaryObjective>(name);
  elemental_.SetAttr(IntAttr1::kAuxObjPriority, AttrKey(id), priority);
  return id;
}

void ModelStorageV2::DeleteAuxiliaryObjective(const AuxiliaryObjectiveId id) {
  CHECK(elemental_.DeleteElement(id)) << "no auxiliary objective " << id;
}

int ModelStorageV2::num_auxiliary_objectives() const {
  return static_cast<int>(
      elemental_.NumElements(ElementType::kAuxiliaryObjective));
}

AuxiliaryObjectiveId ModelStorageV2::next_auxiliary_objective_id() const {
  return AuxiliaryObjectiveId(
      elemental_.NextElementId(ElementType::kAuxiliaryObjective));
}

void ModelStorageV2::ensure_next_auxiliary_objective_id_at_least(
    const AuxiliaryObjectiveId id) {
  elemental_.EnsureNextElementIdAtLeast(id);
}

bool ModelStorageV2::has_auxiliary_objective(
    const AuxiliaryObjectiveId id) const {
  return elemental_.ElementExists(id);
}

std::vector<AuxiliaryObjectiveId> ModelStorageV2::AuxiliaryObjectives() const {
  const auto ids = elemental_.AllElements<ElementType::kAuxiliaryObjective>();
  std::vector<AuxiliaryObjectiveId> result;
  result.reserve(ids.size());
  for (const auto id : ids) {
    result.emplace_back(id);
  }
  return result;
}

std::vector<AuxiliaryObjectiveId> ModelStorageV2::SortedAuxiliaryObjectives()
    const {
  auto ids = AuxiliaryObjectives();
  absl::c_sort(ids);
  return ids;
}

////////////////////////////////////////////////////////////////////////////////
// Atomic constraint template inline implementations.
////////////////////////////////////////////////////////////////////////////////

template <typename ConstraintData>
typename ConstraintData::IdType ModelStorageV2::AddAtomicConstraint(
    const ConstraintData data) {
  if constexpr (!ConstraintData::kSupportsElemental) {
    LOG(FATAL) << "elemental not supported yet";
  } else {
    return internal::AddAtomicConstraint(data, elemental_);
  }
}

template <typename IdType>
void ModelStorageV2::DeleteAtomicConstraint(const IdType id) {
  if constexpr (!AtomicConstraintTraits<IdType>::kSupportsElemental) {
    LOG(FATAL) << "elemental not supported yet";
  } else {
    CHECK(elemental_.DeleteElement(id))
        << "no constraint in the model with id: " << id;
  }
}

template <typename IdType>
typename AtomicConstraintTraits<IdType>::ConstraintData
ModelStorageV2::GetConstraintData(IdType id) const {
  if constexpr (!AtomicConstraintTraits<IdType>::kSupportsElemental) {
    LOG(FATAL) << "elemental not supported yet";
  } else {
    return internal::GetAtomicConstraint(id, elemental_);
  }
}

template <typename IdType>
const typename AtomicConstraintTraits<IdType>::ConstraintData&
ModelStorageV2::constraint_data(const IdType) const {
  LOG(FATAL) << "not implementable for ModelStorageV2";
}

template <typename IdType>
int64_t ModelStorageV2::num_constraints() const {
  if constexpr (!AtomicConstraintTraits<IdType>::kSupportsElemental) {
    LOG(FATAL) << "elemental not supported yet";
  } else {
    return elemental_.NumElements(AtomicConstraintTraits<IdType>::kElementType);
  }
}

template <typename IdType>
IdType ModelStorageV2::next_constraint_id() const {
  if constexpr (!AtomicConstraintTraits<IdType>::kSupportsElemental) {
    LOG(FATAL) << "elemental not supported yet";
  } else {
    return IdType{
        elemental_.NextElementId(AtomicConstraintTraits<IdType>::kElementType)};
  }
}

template <typename IdType>
void ModelStorageV2::ensure_next_constraint_id_at_least(const IdType id) {
  if constexpr (!AtomicConstraintTraits<IdType>::kSupportsElemental) {
    LOG(FATAL) << "elemental not supported yet";
  } else {
    elemental_.EnsureNextElementIdAtLeast(id);
  }
}

template <typename IdType>
bool ModelStorageV2::has_constraint(const IdType id) const {
  if constexpr (!AtomicConstraintTraits<IdType>::kSupportsElemental) {
    LOG(FATAL) << "elemental not supported yet";
  } else {
    return elemental_.ElementExists(id);
  }
}

template <typename IdType>
std::vector<IdType> ModelStorageV2::Constraints() const {
  if constexpr (!AtomicConstraintTraits<IdType>::kSupportsElemental) {
    LOG(FATAL) << "elemental not supported yet";
  } else {
    constexpr ElementType e = AtomicConstraintTraits<IdType>::kElementType;
    auto els = elemental_.AllElements<e>();
    std::vector<ElementId<e>> result;
    result.reserve(els.size());
    for (const ElementId<e> el : els) {
      result.push_back(el);
    }
    return result;
  }
}

template <typename IdType>
std::vector<IdType> ModelStorageV2::SortedConstraints() const {
  if constexpr (!AtomicConstraintTraits<IdType>::kSupportsElemental) {
    LOG(FATAL) << "elemental not supported yet";
  } else {
    auto result = Constraints<IdType>();
    absl::c_sort(result);
    return result;
  }
}

template <typename IdType>
std::vector<IdType> ModelStorageV2::ConstraintsWithVariable(
    const VariableId) const {
  LOG(FATAL) << "not implementable for ModelStorageV2";
}

template <typename IdType>
std::vector<VariableId> ModelStorageV2::VariablesInConstraint(
    const IdType) const {
  LOG(FATAL) << "not implementable for ModelStorageV2";
}

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_STORAGE_MODEL_STORAGE_V2_H_
