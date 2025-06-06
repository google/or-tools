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

#ifndef OR_TOOLS_MATH_OPT_STORAGE_MODEL_STORAGE_H_
#define OR_TOOLS_MATH_OPT_STORAGE_MODEL_STORAGE_H_

#ifdef MATHOPT_STORAGE_V2

#include "ortools/math_opt/storage/model_storage_v2.h"

namespace operations_research::math_opt {

using ModelStorage = ModelStorageV2;

}  // namespace operations_research::math_opt

#else

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ortools/math_opt/constraints/indicator/storage.h"  // IWYU pragma: export
#include "ortools/math_opt/constraints/quadratic/storage.h"  // IWYU pragma: export
#include "ortools/math_opt/constraints/second_order_cone/storage.h"
#include "ortools/math_opt/constraints/sos/storage.h"  // IWYU pragma: export
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/atomic_constraint_storage.h"  // IWYU pragma: export
#include "ortools/math_opt/storage/iterators.h"
#include "ortools/math_opt/storage/linear_constraint_storage.h"
#include "ortools/math_opt/storage/model_storage_types.h"
#include "ortools/math_opt/storage/objective_storage.h"
#include "ortools/math_opt/storage/update_trackers.h"
#include "ortools/math_opt/storage/variable_storage.h"

namespace operations_research {
namespace math_opt {

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
// Models problems of the form:
//   min sum_{j in J} c_j * x_j + d
//   s.t. lb^c_i <= sum_{j in J} A_ij * x_j <= ub^c_i        for all i in I,
//        lb^v_j <= x_j <= ub^v_j                            for all j in J,
//        x_j integer                                        for all j in Z,
// where above:
//  * I: the set of linear constraints,
//  * J: the set of variables,
//  * Z: a subset of J, the integer variables,
//  * x: the decision variables (indexed by J),
//  * c: the linear objective, one double per variable,
//  * d: the objective offset, a double scalar,
//  * lb^c: the constraint lower bounds, one double per linear constraint,
//  * ub^c: the constraint upper bounds, one double per linear constraint,
//  * lb^v: the variable lower bounds, one double per variable,
//  * ub^v: the variable upper bounds, one double per variable,
//  * A: the linear constraint matrix, a double per variable/constraint pair.
//
// The min in the objective can also be changed to a max.
//
// A simple example:
//
// Model the problem:
//   max 2.0 * x + y
//   s.t. x + y <= 1.5
//            x in {0.0, 1.0}
//       0 <= y <= 2.5
//
//   using ::operations_research::math_opt::ModelStorage;
//   using ::operations_research::math_opt::VariableId;
//   using ::operations_research::math_opt::LinearConstraintId;
//   using ::operations_research::math_opt::ModelProto;
//   using ::operations_research::math_opt::ModelProtoUpdate;
//
//   ModelStorage model("my_model");
//   const VariableId x = model.AddVariable(0.0, 1.0, true, "x");
//   const VariableId y = model.AddVariable(0.0, 2.5, false, "y");
//   const LinearConstraintId c = model.AddLinearConstraint(
//       -std::numeric_limits<double>::infinity, 1.5, "c");
//   model.set_linear_constraint_coefficient(x, c, 1.0);
//   model.set_linear_constraint_coefficient(y, c, 1.0);
//   model.set_linear_objective_coefficient(kPrimaryObjectiveId, x, 2.0);
//   model.set_linear_objective_coefficient(kPrimaryObjectiveId, y, 1.0);
//   model.set_maximize(kPrimaryObjectiveId);
//
// Now, export to a proto describing the model:
//
//   const ModelProto model_proto = model.ExportModel();
//
// Modify the problem and get a model update proto:
//
//   const UpdateTrackerId update_tracker = model.NewUpdateTracker();
//   c.set_upper_bound(2.0);
//   const std::optional<ModelUpdateProto> update_proto =
//     model.ExportModelUpdate(update_tracker);
//   model.AdvanceCheckpoint(update_tracker);
//
// Reading and writing model properties:
//
// Properties of the model (e.g. variable/constraint bounds) can be written
// and read in amortized O(1) time. Deleting a variable will take time
// O(#constraints containing the variable), and likewise deleting a constraint
// will take time O(#variables in the constraint). The constraint matrix is
// stored as hash map where the key is a {LinearConstraintId, VariableId}
// pair and the value is the coefficient. The nonzeros of the matrix are
// additionally stored by row and by column.
//
// Exporting the Model proto:
//
// The Model proto is an equivalent representation to ModelStorage. It has a
// smaller memory footprint and optimized for storage/transport, rather than
// efficient modification. It is also the format consumed by solvers in this
// library. The Model proto can be generated by calling
// ModelStorage::ExportModel().
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
// tracking changes. See
// g3doc/ortools/math_opt/g3doc/model_building_complexity.md
// for details.
//
// On bad input:
//
// Using a bad variable id or constraint id (an id not in the current model,
// which includes ids that have been deleted) on any method will result in an
// immediate failure (either a CHECK failure or an exception, which is an
// implementation detail you should not rely on). We make no attempt to say if a
// model is invalid (e.g. a variable lower bound is infinite, exceeds an upper
// bound, or is NaN). The exported models are validated instead, see
// model_validator.h.
class ModelStorage {
 public:
  // Returns a storage from the input proto. Returns a failure status if the
  // input proto is invalid.
  //
  // Variable/constraint names can be repeated in the input proto but will be
  // considered invalid when solving.
  //
  // See ApplyUpdateProto() for dealing with subsequent updates.
  static absl::StatusOr<absl::Nonnull<std::unique_ptr<ModelStorage> > >
  FromModelProto(const ModelProto& model_proto);

  // Creates an empty minimization problem.
  inline explicit ModelStorage(absl::string_view model_name = "",
                               absl::string_view primary_objective_name = "");

  ModelStorage& operator=(const ModelStorage&) = delete;

  // Returns a clone of the model, optionally changing model's name.
  //
  // The variables and constraints have the same ids. The clone will also not
  // reused any id of variable/constraint that was deleted in the original.
  //
  // Note that the returned model does not have any update tracker.
  absl::Nonnull<std::unique_ptr<ModelStorage> > Clone(
      std::optional<absl::string_view> new_name = std::nullopt) const;

  inline const std::string& name() const { return copyable_data_.name; }

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
  VariableId AddVariable(double lower_bound, double upper_bound,
                         bool is_integer, absl::string_view name = "");

  inline double variable_lower_bound(VariableId id) const;
  inline double variable_upper_bound(VariableId id) const;
  inline bool is_variable_integer(VariableId id) const;
  inline const std::string& variable_name(VariableId id) const;

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
  inline int num_variables() const;

  // The returned id of the next call to AddVariable.
  //
  // Equal to the number of variables created.
  inline VariableId next_variable_id() const;

  // Sets the next variable id to be the maximum of next_variable_id() and id.
  inline void ensure_next_variable_id_at_least(VariableId id);

  // Returns true if this id has been created and not yet deleted.
  inline bool has_variable(VariableId id) const;

  // The VariableIds in use (not deleted), order not defined.
  std::vector<VariableId> variables() const;

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
  LinearConstraintId AddLinearConstraint(double lower_bound, double upper_bound,
                                         absl::string_view name = "");

  inline double linear_constraint_lower_bound(LinearConstraintId id) const;
  inline double linear_constraint_upper_bound(LinearConstraintId id) const;
  inline const std::string& linear_constraint_name(LinearConstraintId id) const;

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
  inline int num_linear_constraints() const;

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
  inline std::vector<std::tuple<LinearConstraintId, VariableId, double> >
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
  inline const std::string& objective_name(ObjectiveId id) const;

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
  // variable with nonzero objective coefficient.
  //
  // Runs in O(# nonzero linear/quadratic objective terms).
  inline void clear_objective(ObjectiveId id);

  // The variables with nonzero linear objective coefficients.
  inline const absl::flat_hash_map<VariableId, double>& linear_objective(
      ObjectiveId id) const;

  inline int64_t num_linear_objective_terms(ObjectiveId id) const;

  inline int64_t num_quadratic_objective_terms(ObjectiveId id) const;

  // The variable pairs with nonzero quadratic objective coefficients. The keys
  // are ordered such that .first <= .second. All values are nonempty.
  //
  // TODO(b/233630053) do no allocate the result, expose an iterator API.
  inline std::vector<std::tuple<VariableId, VariableId, double> >
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
  inline const typename AtomicConstraintTraits<IdType>::ConstraintData&
  constraint_data(IdType id) const;

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

  // Returns a sorted vector of all existing (not deleted) atomic constraints
  // in the model of the family corresponding to `ConstraintData`.
  //
  // Runs in O(n log(n)), where n is the number of constraints returned.
  template <typename IdType>
  std::vector<IdType> SortedConstraints() const;

  // Returns the constraint in the given family in which the variable appears
  // structurally (i.e., has a coefficient, possibly zero). Order is not
  // defined.
  template <typename IdType>
  inline std::vector<IdType> ConstraintsWithVariable(
      VariableId variable_id) const;

  // Returns the variables appearing in the constraint. Order is not defined.
  template <typename IdType>
  inline std::vector<VariableId> VariablesInConstraint(IdType id) const;

  //////////////////////////////////////////////////////////////////////////////
  // Export
  //////////////////////////////////////////////////////////////////////////////

  // Returns a proto representation of the optimization model.
  //
  // See FromModelProto() to build a ModelStorage from a proto.
  ModelProto ExportModel(bool remove_names = false) const;

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
  //
  // Example:
  //   ModelStorage model;
  //   ...
  //   ASSIGN_OR_RETURN(const auto solver,
  //                    Solver::New(solver_type, model.ExportModel(),
  //                                /*initializer=*/{}));
  //   const UpdateTrackerId update_tracker = model.NewUpdatesTracker();
  //
  //   ASSIGN_OR_RETURN(const auto result_1,
  //                    solver->Solve(/*parameters=*/{});
  //
  //   model.AddVariable(0.0, 1.0, true, "y");
  //   model.set_maximize(true);
  //
  //   const std::optional<ModelUpdateProto> update_proto =
  //     model.ExportModelUpdate(update_tracker);
  //   model.AdvanceCheckpoint(update_tracker);
  //
  //   if (update_proto) {
  //     ASSIGN_OR_RETURN(const bool updated, solver->Update(*update_proto));
  //     if (!updated) {
  //       // The update is not supported by the solver, we create a new one.
  //       ASSIGN_OR_RETURN(const auto new_model_proto, model.ExportModel());
  //       ASSIGN_OR_RETURN(solver,
  //                        Solver::New(solver_type, new_model_proto,
  //                                    /*initializer=*/{}));
  //     }
  //   }
  //   ASSIGN_OR_RETURN(const auto result_2,
  //                    solver->Solve(/*parameters=*/{});
  //
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
  std::optional<ModelUpdateProto> ExportModelUpdate(
      UpdateTrackerId update_tracker, bool remove_names = false) const;

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
  struct UpdateTrackerData {
    UpdateTrackerData(
        const VariableStorage& variables, const ObjectiveStorage& objectives,
        const LinearConstraintStorage& linear_constraints,
        const AtomicConstraintStorage<QuadraticConstraintData>&
            quadratic_constraints,
        const AtomicConstraintStorage<SecondOrderConeConstraintData>
            soc_constraints,
        const AtomicConstraintStorage<Sos1ConstraintData>& sos1_constraints,
        const AtomicConstraintStorage<Sos2ConstraintData>& sos2_constraints,
        const AtomicConstraintStorage<IndicatorConstraintData>&
            indicator_constraints)
        : dirty_variables(variables),
          dirty_objective(objectives, variables.next_id()),
          dirty_linear_constraints(linear_constraints,
                                   dirty_variables.checkpoint),
          dirty_quadratic_constraints(quadratic_constraints),
          dirty_soc_constraints(soc_constraints),
          dirty_sos1_constraints(sos1_constraints),
          dirty_sos2_constraints(sos2_constraints),
          dirty_indicator_constraints(indicator_constraints) {}

    // Returns a proto representation of the changes to the model since the most
    // recent call to SharedCheckpoint() or nullopt if no changes happened.
    //
    // Thread-safety: this method is threadsafe.
    std::optional<ModelUpdateProto> ExportModelUpdate(
        const ModelStorage& storage, bool remove_names = false) const;

    // Use the current model state as the starting point to calculate the
    // ModelUpdateProto next time ExportSharedModelUpdate() is called.
    void AdvanceCheckpoint(const ModelStorage& storage);

    // Implementers of new constraint types should provide a specialization that
    // returns the address of the appropriate `UpdateTrackerData` field.
    template <typename ConstraintData>
    static constexpr typename AtomicConstraintStorage<ConstraintData>::Diff
        UpdateTrackerData::*
        AtomicConstraintDirtyFieldPtr();

    // Update information
    //
    // Implicitly, all data for variables and constraints added after the last
    // checkpoint are considered "new" and will NOT be stored in the "dirty"
    // data structures below.

    VariableStorage::Diff dirty_variables;
    ObjectiveStorage::Diff dirty_objective;
    LinearConstraintStorage::Diff dirty_linear_constraints;
    AtomicConstraintStorage<QuadraticConstraintData>::Diff
        dirty_quadratic_constraints;
    AtomicConstraintStorage<SecondOrderConeConstraintData>::Diff
        dirty_soc_constraints;
    AtomicConstraintStorage<Sos1ConstraintData>::Diff dirty_sos1_constraints;
    AtomicConstraintStorage<Sos2ConstraintData>::Diff dirty_sos2_constraints;
    AtomicConstraintStorage<IndicatorConstraintData>::Diff
        dirty_indicator_constraints;
  };

  // All data that is copied (by the C++ default copy constructor) when using
  // Clone().
  struct CopyableData {
    CopyableData(const absl::string_view model_name,
                 const absl::string_view primary_objective_name)
        : name(model_name), objectives(/*name=*/primary_objective_name) {}
    std::string name;

    VariableStorage variables;
    ObjectiveStorage objectives;
    LinearConstraintStorage linear_constraints;

    AtomicConstraintStorage<QuadraticConstraintData> quadratic_constraints;
    AtomicConstraintStorage<SecondOrderConeConstraintData> soc_constraints;
    AtomicConstraintStorage<Sos1ConstraintData> sos1_constraints;
    AtomicConstraintStorage<Sos2ConstraintData> sos2_constraints;
    AtomicConstraintStorage<IndicatorConstraintData> indicator_constraints;
  };

  // Private copy constructor that copies only copyable_data_, not
  // update_trackers_. It is used internally by Clone().
  ModelStorage(const ModelStorage& other)
      : copyable_data_(other.copyable_data_) {}

  auto UpdateAndGetVariableDiffs() {
    return MakeUpdateDataFieldRange<&UpdateTrackerData::dirty_variables>(
        update_trackers_.GetUpdatedTrackers());
  }

  auto UpdateAndGetObjectiveDiffs() {
    return MakeUpdateDataFieldRange<&UpdateTrackerData::dirty_objective>(
        update_trackers_.GetUpdatedTrackers());
  }

  auto UpdateAndGetLinearConstraintDiffs() {
    return MakeUpdateDataFieldRange<
        &UpdateTrackerData::dirty_linear_constraints>(
        update_trackers_.GetUpdatedTrackers());
  }

  // Ids must be greater or equal to `next_variable_id()`.
  void AddVariables(const VariablesProto& variables);

  // Ids must be greater or equal to `next_auxiliary_objective_id()`.
  void AddAuxiliaryObjectives(
      const google::protobuf::Map<int64_t, ObjectiveProto>& objectives);

  // Ids must be greater or equal to `next_linear_constraint_id()`.
  void AddLinearConstraints(const LinearConstraintsProto& linear_constraints);

  void UpdateObjective(ObjectiveId id, const ObjectiveUpdatesProto& update);

  // Updates the objective linear coefficients. The coefficients of variables
  // not in the input are kept as-is.
  void UpdateLinearObjectiveCoefficients(
      ObjectiveId id, const SparseDoubleVectorProto& coefficients);

  // Updates the objective quadratic coefficients. The coefficients of the pairs
  // of variables not in the input are kept as-is.
  void UpdateQuadraticObjectiveCoefficients(
      ObjectiveId id, const SparseDoubleMatrixProto& coefficients);

  // Updates the linear constraints' coefficients. The coefficients of
  // (constraint, variable) pairs not in the input are kept as-is.
  void UpdateLinearConstraintCoefficients(
      const SparseDoubleMatrixProto& coefficients);

  // Implementers of new constraint types should provide a specialization that
  // returns a non-const reference to the appropriate `ModelStorage` field.
  template <typename ConstraintData>
  AtomicConstraintStorage<ConstraintData>& constraint_storage();

  // Implementers of new constraint types should provide a specialization that
  // returns a const reference to the appropriate `ModelStorage` field.
  template <typename ConstraintData>
  const AtomicConstraintStorage<ConstraintData>& constraint_storage() const;

  CopyableData copyable_data_;
  UpdateTrackers<UpdateTrackerData> update_trackers_;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Inlined function implementations
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

ModelStorage::ModelStorage(const absl::string_view model_name,
                           const absl::string_view primary_objective_name)
    : copyable_data_(/*model_name=*/model_name,
                     /*primary_objective_name=*/primary_objective_name) {}

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////

VariableId ModelStorage::AddVariable(absl::string_view name) {
  return AddVariable(-std::numeric_limits<double>::infinity(),
                     std::numeric_limits<double>::infinity(), false, name);
}

double ModelStorage::variable_lower_bound(const VariableId id) const {
  return copyable_data_.variables.lower_bound(id);
}

double ModelStorage::variable_upper_bound(const VariableId id) const {
  return copyable_data_.variables.upper_bound(id);
}

bool ModelStorage::is_variable_integer(VariableId id) const {
  return copyable_data_.variables.is_integer(id);
}

const std::string& ModelStorage::variable_name(const VariableId id) const {
  return copyable_data_.variables.name(id);
}

void ModelStorage::set_variable_lower_bound(const VariableId id,
                                            const double lower_bound) {
  copyable_data_.variables.set_lower_bound(id, lower_bound,
                                           UpdateAndGetVariableDiffs());
}

void ModelStorage::set_variable_upper_bound(const VariableId id,
                                            const double upper_bound) {
  copyable_data_.variables.set_upper_bound(id, upper_bound,
                                           UpdateAndGetVariableDiffs());
}

void ModelStorage::set_variable_is_integer(const VariableId id,
                                           const bool is_integer) {
  copyable_data_.variables.set_integer(id, is_integer,
                                       UpdateAndGetVariableDiffs());
}

void ModelStorage::set_variable_as_integer(VariableId id) {
  set_variable_is_integer(id, true);
}

void ModelStorage::set_variable_as_continuous(VariableId id) {
  set_variable_is_integer(id, false);
}

int ModelStorage::num_variables() const {
  return static_cast<int>(copyable_data_.variables.size());
}

VariableId ModelStorage::next_variable_id() const {
  return copyable_data_.variables.next_id();
}

void ModelStorage::ensure_next_variable_id_at_least(const VariableId id) {
  copyable_data_.variables.ensure_next_id_at_least(id);
}

bool ModelStorage::has_variable(const VariableId id) const {
  return copyable_data_.variables.contains(id);
}

////////////////////////////////////////////////////////////////////////////////
// Linear Constraints
////////////////////////////////////////////////////////////////////////////////

LinearConstraintId ModelStorage::AddLinearConstraint(absl::string_view name) {
  return AddLinearConstraint(-std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::infinity(), name);
}

double ModelStorage::linear_constraint_lower_bound(
    const LinearConstraintId id) const {
  return copyable_data_.linear_constraints.lower_bound(id);
}

double ModelStorage::linear_constraint_upper_bound(
    const LinearConstraintId id) const {
  return copyable_data_.linear_constraints.upper_bound(id);
}

const std::string& ModelStorage::linear_constraint_name(
    const LinearConstraintId id) const {
  return copyable_data_.linear_constraints.name(id);
}

void ModelStorage::set_linear_constraint_lower_bound(
    const LinearConstraintId id, const double lower_bound) {
  copyable_data_.linear_constraints.set_lower_bound(
      id, lower_bound, UpdateAndGetLinearConstraintDiffs());
}

void ModelStorage::set_linear_constraint_upper_bound(
    const LinearConstraintId id, const double upper_bound) {
  copyable_data_.linear_constraints.set_upper_bound(
      id, upper_bound, UpdateAndGetLinearConstraintDiffs());
}

int ModelStorage::num_linear_constraints() const {
  return static_cast<int>(copyable_data_.linear_constraints.size());
}

LinearConstraintId ModelStorage::next_linear_constraint_id() const {
  return copyable_data_.linear_constraints.next_id();
}

void ModelStorage::ensure_next_linear_constraint_id_at_least(
    LinearConstraintId id) {
  copyable_data_.linear_constraints.ensure_next_id_at_least(id);
}

bool ModelStorage::has_linear_constraint(const LinearConstraintId id) const {
  return copyable_data_.linear_constraints.contains(id);
}

////////////////////////////////////////////////////////////////////////////////
// Linear Constraint Matrix
////////////////////////////////////////////////////////////////////////////////

double ModelStorage::linear_constraint_coefficient(
    LinearConstraintId constraint, VariableId variable) const {
  return copyable_data_.linear_constraints.matrix().get(constraint, variable);
}

bool ModelStorage::is_linear_constraint_coefficient_nonzero(
    LinearConstraintId constraint, VariableId variable) const {
  return copyable_data_.linear_constraints.matrix().contains(constraint,
                                                             variable);
}

void ModelStorage::set_linear_constraint_coefficient(
    const LinearConstraintId constraint, const VariableId variable,
    const double value) {
  copyable_data_.linear_constraints.set_term(
      constraint, variable, value, UpdateAndGetLinearConstraintDiffs());
}

std::vector<std::tuple<LinearConstraintId, VariableId, double> >
ModelStorage::linear_constraint_matrix() const {
  return copyable_data_.linear_constraints.matrix().Terms();
}

std::vector<VariableId> ModelStorage::variables_in_linear_constraint(
    LinearConstraintId constraint) const {
  return copyable_data_.linear_constraints.matrix().row(constraint);
}

std::vector<LinearConstraintId> ModelStorage::linear_constraints_with_variable(
    VariableId variable) const {
  return copyable_data_.linear_constraints.matrix().column(variable);
}

////////////////////////////////////////////////////////////////////////////////
// Objectives
////////////////////////////////////////////////////////////////////////////////

bool ModelStorage::is_maximize(const ObjectiveId id) const {
  return copyable_data_.objectives.maximize(id);
}

int64_t ModelStorage::objective_priority(const ObjectiveId id) const {
  return copyable_data_.objectives.priority(id);
}

double ModelStorage::objective_offset(const ObjectiveId id) const {
  return copyable_data_.objectives.offset(id);
}

double ModelStorage::linear_objective_coefficient(
    const ObjectiveId id, const VariableId variable) const {
  return copyable_data_.objectives.linear_term(id, variable);
}

double ModelStorage::quadratic_objective_coefficient(
    const ObjectiveId id, const VariableId first_variable,
    const VariableId second_variable) const {
  return copyable_data_.objectives.quadratic_term(id, first_variable,
                                                  second_variable);
}

bool ModelStorage::is_linear_objective_coefficient_nonzero(
    const ObjectiveId id, const VariableId variable) const {
  return copyable_data_.objectives.linear_terms(id).contains(variable);
}

bool ModelStorage::is_quadratic_objective_coefficient_nonzero(
    const ObjectiveId id, const VariableId first_variable,
    const VariableId second_variable) const {
  return copyable_data_.objectives.quadratic_terms(id).get(
             first_variable, second_variable) != 0.0;
}

const std::string& ModelStorage::objective_name(const ObjectiveId id) const {
  return copyable_data_.objectives.name(id);
}

void ModelStorage::set_is_maximize(const ObjectiveId id,
                                   const bool is_maximize) {
  copyable_data_.objectives.set_maximize(id, is_maximize,
                                         UpdateAndGetObjectiveDiffs());
}

void ModelStorage::set_maximize(const ObjectiveId id) {
  set_is_maximize(id, true);
}

void ModelStorage::set_minimize(const ObjectiveId id) {
  set_is_maximize(id, false);
}

void ModelStorage::set_objective_priority(const ObjectiveId id,
                                          const int64_t value) {
  copyable_data_.objectives.set_priority(id, value,
                                         UpdateAndGetObjectiveDiffs());
}

void ModelStorage::set_objective_offset(const ObjectiveId id,
                                        const double value) {
  copyable_data_.objectives.set_offset(id, value, UpdateAndGetObjectiveDiffs());
}

void ModelStorage::set_linear_objective_coefficient(const ObjectiveId id,
                                                    const VariableId variable,
                                                    const double value) {
  copyable_data_.objectives.set_linear_term(id, variable, value,
                                            UpdateAndGetObjectiveDiffs());
}

void ModelStorage::set_quadratic_objective_coefficient(
    const ObjectiveId id, const VariableId first_variable,
    const VariableId second_variable, const double value) {
  copyable_data_.objectives.set_quadratic_term(
      id, first_variable, second_variable, value, UpdateAndGetObjectiveDiffs());
}

void ModelStorage::clear_objective(const ObjectiveId id) {
  copyable_data_.objectives.Clear(id, UpdateAndGetObjectiveDiffs());
}

const absl::flat_hash_map<VariableId, double>& ModelStorage::linear_objective(
    const ObjectiveId id) const {
  return copyable_data_.objectives.linear_terms(id);
}

int64_t ModelStorage::num_linear_objective_terms(const ObjectiveId id) const {
  return copyable_data_.objectives.linear_terms(id).size();
}

int64_t ModelStorage::num_quadratic_objective_terms(
    const ObjectiveId id) const {
  return copyable_data_.objectives.quadratic_terms(id).nonzeros();
}

std::vector<std::tuple<VariableId, VariableId, double> >
ModelStorage::quadratic_objective_terms(const ObjectiveId id) const {
  return copyable_data_.objectives.quadratic_terms(id).Terms();
}

////////////////////////////////////////////////////////////////////////////////
// Auxiliary objectives
////////////////////////////////////////////////////////////////////////////////

AuxiliaryObjectiveId ModelStorage::AddAuxiliaryObjective(
    const int64_t priority, const absl::string_view name) {
  return copyable_data_.objectives.AddAuxiliaryObjective(priority, name);
}

void ModelStorage::DeleteAuxiliaryObjective(const AuxiliaryObjectiveId id) {
  copyable_data_.objectives.Delete(id, UpdateAndGetObjectiveDiffs());
}

int ModelStorage::num_auxiliary_objectives() const {
  return static_cast<int>(copyable_data_.objectives.num_auxiliary_objectives());
}

AuxiliaryObjectiveId ModelStorage::next_auxiliary_objective_id() const {
  return copyable_data_.objectives.next_id();
}

void ModelStorage::ensure_next_auxiliary_objective_id_at_least(
    const AuxiliaryObjectiveId id) {
  copyable_data_.objectives.ensure_next_id_at_least(id);
}

bool ModelStorage::has_auxiliary_objective(
    const AuxiliaryObjectiveId id) const {
  return copyable_data_.objectives.contains(id);
}

std::vector<AuxiliaryObjectiveId> ModelStorage::AuxiliaryObjectives() const {
  return copyable_data_.objectives.AuxiliaryObjectives();
}

std::vector<AuxiliaryObjectiveId> ModelStorage::SortedAuxiliaryObjectives()
    const {
  return copyable_data_.objectives.SortedAuxiliaryObjectives();
}

////////////////////////////////////////////////////////////////////////////////
// Atomic constraint template inline implementations.
////////////////////////////////////////////////////////////////////////////////

template <typename ConstraintData>
typename ConstraintData::IdType ModelStorage::AddAtomicConstraint(
    ConstraintData data) {
  return constraint_storage<ConstraintData>().AddConstraint(data);
}

template <typename IdType>
void ModelStorage::DeleteAtomicConstraint(const IdType id) {
  using ConstraintData =
      typename AtomicConstraintTraits<IdType>::ConstraintData;
  auto& storage = constraint_storage<ConstraintData>();
  CHECK(storage.contains(id));
  storage.Delete(
      id,
      MakeUpdateDataFieldRange<
          UpdateTrackerData::AtomicConstraintDirtyFieldPtr<ConstraintData>()>(
          update_trackers_.GetUpdatedTrackers()));
}

template <typename IdType>
const typename AtomicConstraintTraits<IdType>::ConstraintData&
ModelStorage::constraint_data(const IdType id) const {
  using ConstraintData =
      typename AtomicConstraintTraits<IdType>::ConstraintData;
  return constraint_storage<ConstraintData>().data(id);
}

template <typename IdType>
int64_t ModelStorage::num_constraints() const {
  using ConstraintData =
      typename AtomicConstraintTraits<IdType>::ConstraintData;
  return constraint_storage<ConstraintData>().size();
}

template <typename IdType>
IdType ModelStorage::next_constraint_id() const {
  using ConstraintData =
      typename AtomicConstraintTraits<IdType>::ConstraintData;
  return constraint_storage<ConstraintData>().next_id();
}

template <typename IdType>
void ModelStorage::ensure_next_constraint_id_at_least(const IdType id) {
  using ConstraintData =
      typename AtomicConstraintTraits<IdType>::ConstraintData;
  return constraint_storage<ConstraintData>().ensure_next_id_at_least(id);
}

template <typename IdType>
bool ModelStorage::has_constraint(const IdType id) const {
  using ConstraintData =
      typename AtomicConstraintTraits<IdType>::ConstraintData;
  return constraint_storage<ConstraintData>().contains(id);
}

template <typename IdType>
std::vector<IdType> ModelStorage::Constraints() const {
  using ConstraintData =
      typename AtomicConstraintTraits<IdType>::ConstraintData;
  return constraint_storage<ConstraintData>().Constraints();
}

template <typename IdType>
std::vector<IdType> ModelStorage::SortedConstraints() const {
  using ConstraintData =
      typename AtomicConstraintTraits<IdType>::ConstraintData;
  return constraint_storage<ConstraintData>().SortedConstraints();
}

template <typename IdType>
std::vector<IdType> ModelStorage::ConstraintsWithVariable(
    const VariableId variable_id) const {
  using ConstraintData =
      typename AtomicConstraintTraits<IdType>::ConstraintData;
  const absl::flat_hash_set<IdType> constraints =
      constraint_storage<ConstraintData>().RelatedConstraints(variable_id);
  return {constraints.begin(), constraints.end()};
}

template <typename IdType>
std::vector<VariableId> ModelStorage::VariablesInConstraint(
    const IdType id) const {
  return constraint_data(id).RelatedVariables();
}

////////////////////////////////////////////////////////////////////////////////
// Atomic constraint template specializations.
////////////////////////////////////////////////////////////////////////////////

// --------------------------- Quadratic constraints ---------------------------

template <>
inline AtomicConstraintStorage<QuadraticConstraintData>&
ModelStorage::constraint_storage() {
  return copyable_data_.quadratic_constraints;
}

template <>
inline const AtomicConstraintStorage<QuadraticConstraintData>&
ModelStorage::constraint_storage() const {
  return copyable_data_.quadratic_constraints;
}

template <>
constexpr typename AtomicConstraintStorage<QuadraticConstraintData>::Diff
    ModelStorage::UpdateTrackerData::*
    ModelStorage::UpdateTrackerData::AtomicConstraintDirtyFieldPtr<
        QuadraticConstraintData>() {
  return &UpdateTrackerData::dirty_quadratic_constraints;
}

// ----------------------- Second-order cone constraints -----------------------

template <>
inline AtomicConstraintStorage<SecondOrderConeConstraintData>&
ModelStorage::constraint_storage() {
  return copyable_data_.soc_constraints;
}

template <>
inline const AtomicConstraintStorage<SecondOrderConeConstraintData>&
ModelStorage::constraint_storage() const {
  return copyable_data_.soc_constraints;
}

template <>
constexpr typename AtomicConstraintStorage<SecondOrderConeConstraintData>::Diff
    ModelStorage::UpdateTrackerData::*
    ModelStorage::UpdateTrackerData::AtomicConstraintDirtyFieldPtr<
        SecondOrderConeConstraintData>() {
  return &UpdateTrackerData::dirty_soc_constraints;
}

// ----------------------------- SOS1 constraints ------------------------------

template <>
inline AtomicConstraintStorage<Sos1ConstraintData>&
ModelStorage::constraint_storage() {
  return copyable_data_.sos1_constraints;
}

template <>
inline const AtomicConstraintStorage<Sos1ConstraintData>&
ModelStorage::constraint_storage() const {
  return copyable_data_.sos1_constraints;
}

template <>
constexpr typename AtomicConstraintStorage<Sos1ConstraintData>::Diff
    ModelStorage::UpdateTrackerData::*
    ModelStorage::UpdateTrackerData::AtomicConstraintDirtyFieldPtr<
        Sos1ConstraintData>() {
  return &UpdateTrackerData::dirty_sos1_constraints;
}

// ----------------------------- SOS2 constraints ------------------------------

template <>
inline AtomicConstraintStorage<Sos2ConstraintData>&
ModelStorage::constraint_storage() {
  return copyable_data_.sos2_constraints;
}

template <>
inline const AtomicConstraintStorage<Sos2ConstraintData>&
ModelStorage::constraint_storage() const {
  return copyable_data_.sos2_constraints;
}

template <>
constexpr typename AtomicConstraintStorage<Sos2ConstraintData>::Diff
    ModelStorage::UpdateTrackerData::*
    ModelStorage::UpdateTrackerData::AtomicConstraintDirtyFieldPtr<
        Sos2ConstraintData>() {
  return &UpdateTrackerData::dirty_sos2_constraints;
}

// --------------------------- Indicator constraints ---------------------------

template <>
inline AtomicConstraintStorage<IndicatorConstraintData>&
ModelStorage::constraint_storage() {
  return copyable_data_.indicator_constraints;
}

template <>
inline const AtomicConstraintStorage<IndicatorConstraintData>&
ModelStorage::constraint_storage() const {
  return copyable_data_.indicator_constraints;
}

template <>
constexpr typename AtomicConstraintStorage<IndicatorConstraintData>::Diff
    ModelStorage::UpdateTrackerData::*
    ModelStorage::UpdateTrackerData::AtomicConstraintDirtyFieldPtr<
        IndicatorConstraintData>() {
  return &UpdateTrackerData::dirty_indicator_constraints;
}

}  // namespace math_opt
}  // namespace operations_research

#endif

namespace operations_research::math_opt {

// Aliases for non-nullable and nullable pointers to a `ModelStorage`.
// We should mostly be using the former, but in some cases we need the latter.
using ModelStoragePtr = absl::Nonnull<ModelStorage*>;
using NullableModelStoragePtr = absl::Nullable<ModelStorage*>;
using ModelStorageCPtr = absl::Nonnull<const ModelStorage*>;
using NullableModelStorageCPtr = absl::Nullable<const ModelStorage*>;

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_STORAGE_MODEL_STORAGE_H_
