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

#ifndef OR_TOOLS_MATH_OPT_STORAGE_OBJECTIVE_STORAGE_H_
#define OR_TOOLS_MATH_OPT_STORAGE_OBJECTIVE_STORAGE_H_

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "google/protobuf/map.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/storage/range.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"
#include "ortools/math_opt/storage/sparse_matrix.h"

namespace operations_research::math_opt {

// In memory representation of the objective of an optimization model.
class ObjectiveStorage {
 public:
  // Tracks the changes to ObjectiveStorage. Advancing the checkpoint throws
  // away tracked changes.
  //
  // An instance of this class is owned by each update tracker of ModelStorage.
  struct Diff {
    struct SingleObjective {
      inline bool empty() const;
      // The quadratic coefficient diff is not indexed symmetrically, so we need
      // additional information to determine which quadratic terms are dirty.
      void DeleteVariable(VariableId deleted_variable,
                          VariableId variable_checkpoint,
                          const SparseSymmetricMatrix& quadratic_terms);

      bool direction = false;
      bool priority = false;
      bool offset = false;
      // Only for terms where the variable is before the variable_checkpoint
      // and, if an auxiliary objective, the objective is before the
      // objective_checkpoint.
      absl::flat_hash_set<VariableId> linear_coefficients;

      // For each entry, first <= second (the matrix is symmetric).
      // Only holds entries with both variables before the variable checkpoint
      // and, if an auxiliary objective, the objective is before the
      // objective_checkpoint.
      absl::flat_hash_set<std::pair<VariableId, VariableId>>
          quadratic_coefficients;
    };

    inline explicit Diff(const ObjectiveStorage& storage,
                         VariableId variable_checkpoint);

    // Returns true if objective `id` is already tracked by the diff. Otherwise,
    // it should be considered a "new" objective.
    inline bool objective_tracked(ObjectiveId id) const;

    AuxiliaryObjectiveId objective_checkpoint{0};
    VariableId variable_checkpoint{0};
    // No guarantees provided on which objectives have corresponding entries,
    // or that values are not `empty()`.
    // TODO(b/259109678): Consider storing primary objective separately (like in
    // `ObjectiveStorage`) if hashing is a noticeable bottleneck.
    absl::flat_hash_map<ObjectiveId, SingleObjective> objective_diffs;
    absl::flat_hash_set<AuxiliaryObjectiveId> deleted;
  };

  inline explicit ObjectiveStorage(absl::string_view name = {});

  // Adds an auxiliary objective to the model and returns its id.
  //
  // The returned ids begin at zero and strictly increase (in particular, if
  // ensure_next_id_at_least() is not used, they will be consecutive). Deleted
  // ids are NOT reused.
  AuxiliaryObjectiveId AddAuxiliaryObjective(int64_t priority,
                                             absl::string_view name);

  inline bool maximize(ObjectiveId id) const;
  inline int64_t priority(ObjectiveId id) const;
  inline double offset(ObjectiveId id) const;
  inline double linear_term(ObjectiveId id, VariableId v) const;
  inline double quadratic_term(ObjectiveId id, VariableId v1,
                               VariableId v2) const;
  inline const std::string& name(ObjectiveId id) const;
  inline const absl::flat_hash_map<VariableId, double>& linear_terms(
      ObjectiveId id) const;
  inline const SparseSymmetricMatrix& quadratic_terms(ObjectiveId id) const;

  template <typename DiffIter>
  void set_maximize(ObjectiveId id, bool maximize,
                    const iterator_range<DiffIter>& diffs);

  template <typename DiffIter>
  void set_priority(ObjectiveId id, int64_t priority,
                    const iterator_range<DiffIter>& diffs);

  template <typename DiffIter>
  void set_offset(ObjectiveId id, double offset,
                  const iterator_range<DiffIter>& diffs);

  template <typename DiffIter>
  void set_linear_term(ObjectiveId id, VariableId variable, double value,
                       const iterator_range<DiffIter>& diffs);

  template <typename DiffIter>
  void set_quadratic_term(ObjectiveId id, VariableId v1, VariableId v2,
                          double val, const iterator_range<DiffIter>& diffs);

  // Removes an auxiliary objective from the model.
  //
  // It is an error to use a deleted auxiliary objective id as input to any
  // subsequent function calls on the model.
  template <typename DiffIter>
  void Delete(AuxiliaryObjectiveId id, const iterator_range<DiffIter>& diffs);

  // The number of auxiliary objectives in the model.
  //
  // Equal to the number of auxiliary objectives created minus the number of
  // auxiliary objectives deleted.
  inline int64_t num_auxiliary_objectives() const;

  // The returned id of the next call to AddAuxiliaryObjective.
  //
  // Equal to the number of auxiliary objectives created.
  inline AuxiliaryObjectiveId next_id() const;

  // Sets the next auxiliary objective id to be the maximum of next_id() and
  // `minimum`.
  inline void ensure_next_id_at_least(AuxiliaryObjectiveId minimum);

  // Returns true if this id has been created and not yet deleted.
  inline bool contains(AuxiliaryObjectiveId id) const;

  // The AuxiliaryObjectivesIds in use (not deleted), order not defined.
  std::vector<AuxiliaryObjectiveId> AuxiliaryObjectives() const;

  // Returns a sorted vector of all existing (not deleted) auxiliary objectives
  // in the model.
  //
  // Runs in O(n log(n)), where n is the number of auxiliary objectives
  // returned.
  std::vector<AuxiliaryObjectiveId> SortedAuxiliaryObjectives() const;

  // Clears the objective function (coefficients and offset), but not the
  // sense or priority.
  template <typename DiffIter>
  void Clear(ObjectiveId id, const iterator_range<DiffIter>& diffs);

  // Removes all occurrences of var from the objective. Runs in O(# objectives)
  // time (though this can potentially be improved to O(1) if the need arises).
  template <typename DiffIter>
  void DeleteVariable(VariableId variable,
                      const iterator_range<DiffIter>& diffs);

  // Returns a proto description for the primary objective (.first) and all
  // auxiliary objectives (.second).
  std::pair<ObjectiveProto, google::protobuf::Map<int64_t, ObjectiveProto>>
  Proto() const;

  //////////////////////////////////////////////////////////////////////////////
  // Functions for working with Diff
  //////////////////////////////////////////////////////////////////////////////

  // Returns true if there are no changes (tracked changes before the
  // checkpoint).
  //
  // NOTE: when there are new variables with nonzero objective coefficient, the
  // Diff object can be empty (and diff_is_empty will return true), but Update()
  // can return a non-empty ObjectiveUpdatesProto. This behavior MAY CHANGE in
  // the future (this new behavior would be more intuitive, though it is harder
  // to implement efficiently).
  inline bool diff_is_empty(const Diff& diff) const;

  std::pair<ObjectiveUpdatesProto, AuxiliaryObjectivesUpdatesProto> Update(
      const Diff& diff,
      const absl::flat_hash_set<VariableId>& deleted_variables,
      const std::vector<VariableId>& new_variables) const;

  // Updates the checkpoint and clears all stored changes in diff.
  void AdvanceCheckpointInDiff(VariableId variable_checkpoint,
                               Diff& diff) const;

 private:
  struct ObjectiveData {
    ObjectiveProto Proto() const;
    // Returns a proto representing the objective changes with respect to the
    // `diff_data`. If there is no change, returns nullopt.
    std::optional<ObjectiveUpdatesProto> Update(
        const Diff::SingleObjective& diff_data,
        const absl::flat_hash_set<VariableId>& deleted_variables,
        const std::vector<VariableId>& new_variables) const;

    inline void DeleteVariable(VariableId variable);

    bool maximize = false;
    int64_t priority = 0;
    double offset = 0.0;
    SparseCoefficientMap linear_terms;
    SparseSymmetricMatrix quadratic_terms;
    std::string name = "";
  };

  inline const ObjectiveData& objective(ObjectiveId id) const;
  inline ObjectiveData& objective(ObjectiveId id);

  AuxiliaryObjectiveId next_id_{0};
  ObjectiveData primary_objective_;
  absl::flat_hash_map<AuxiliaryObjectiveId, ObjectiveData>
      auxiliary_objectives_;
};

////////////////////////////////////////////////////////////////////////////////
// Inline function implementation
////////////////////////////////////////////////////////////////////////////////

bool ObjectiveStorage::Diff::SingleObjective::empty() const {
  return !direction && !priority && !offset && linear_coefficients.empty() &&
         quadratic_coefficients.empty();
}

ObjectiveStorage::Diff::Diff(const ObjectiveStorage& storage,
                             const VariableId variable_checkpoint)
    : objective_checkpoint(storage.next_id()),
      variable_checkpoint(variable_checkpoint) {}

ObjectiveStorage::ObjectiveStorage(const absl::string_view name) {
  primary_objective_.name = std::string(name);
}

bool ObjectiveStorage::maximize(const ObjectiveId id) const {
  return objective(id).maximize;
}

int64_t ObjectiveStorage::priority(const ObjectiveId id) const {
  return objective(id).priority;
}

double ObjectiveStorage::offset(const ObjectiveId id) const {
  return objective(id).offset;
}

double ObjectiveStorage::linear_term(const ObjectiveId id,
                                     const VariableId v) const {
  return objective(id).linear_terms.get(v);
}

double ObjectiveStorage::quadratic_term(const ObjectiveId id,
                                        const VariableId v1,
                                        const VariableId v2) const {
  return objective(id).quadratic_terms.get(v1, v2);
}

const std::string& ObjectiveStorage::name(const ObjectiveId id) const {
  return objective(id).name;
}

const absl::flat_hash_map<VariableId, double>& ObjectiveStorage::linear_terms(
    const ObjectiveId id) const {
  return objective(id).linear_terms.terms();
}

const SparseSymmetricMatrix& ObjectiveStorage::quadratic_terms(
    const ObjectiveId id) const {
  return objective(id).quadratic_terms;
}

template <typename DiffIter>
void ObjectiveStorage::set_maximize(const ObjectiveId id, const bool maximize,
                                    const iterator_range<DiffIter>& diffs) {
  if (objective(id).maximize == maximize) {
    return;
  }
  objective(id).maximize = maximize;
  for (Diff& diff : diffs) {
    if (diff.objective_tracked(id)) {
      diff.objective_diffs[id].direction = true;
    }
  }
}

template <typename DiffIter>
void ObjectiveStorage::set_priority(const ObjectiveId id,
                                    const int64_t priority,
                                    const iterator_range<DiffIter>& diffs) {
  if (objective(id).priority == priority) {
    return;
  }
  objective(id).priority = priority;
  for (Diff& diff : diffs) {
    if (diff.objective_tracked(id)) {
      diff.objective_diffs[id].priority = true;
    }
  }
}

template <typename DiffIter>
void ObjectiveStorage::set_offset(const ObjectiveId id, const double offset,
                                  const iterator_range<DiffIter>& diffs) {
  if (objective(id).offset == offset) {
    return;
  }
  objective(id).offset = offset;
  for (Diff& diff : diffs) {
    if (diff.objective_tracked(id)) {
      diff.objective_diffs[id].offset = true;
    }
  }
}

template <typename DiffIter>
void ObjectiveStorage::set_linear_term(const ObjectiveId id,
                                       const VariableId variable,
                                       const double value,
                                       const iterator_range<DiffIter>& diffs) {
  if (objective(id).linear_terms.set(variable, value)) {
    for (Diff& diff : diffs) {
      if (diff.objective_tracked(id) && variable < diff.variable_checkpoint) {
        diff.objective_diffs[id].linear_coefficients.insert(variable);
      }
    }
  }
}

template <typename DiffIter>
void ObjectiveStorage::set_quadratic_term(
    const ObjectiveId id, const VariableId v1, const VariableId v2,
    const double val, const iterator_range<DiffIter>& diffs) {
  if (objective(id).quadratic_terms.set(v1, v2, val)) {
    for (Diff& diff : diffs) {
      if (diff.objective_tracked(id) && v1 < diff.variable_checkpoint &&
          v2 < diff.variable_checkpoint) {
        diff.objective_diffs[id].quadratic_coefficients.insert(
            {std::min(v1, v2), std::max(v1, v2)});
      }
    }
  }
}

template <typename DiffIter>
void ObjectiveStorage::Delete(const AuxiliaryObjectiveId id,
                              const iterator_range<DiffIter>& diffs) {
  CHECK(auxiliary_objectives_.contains(id));
  for (Diff& diff : diffs) {
    if (diff.objective_tracked(id)) {
      diff.deleted.insert(id);
      diff.objective_diffs.erase(id);
    }
  }
  auxiliary_objectives_.erase(id);
}

int64_t ObjectiveStorage::num_auxiliary_objectives() const {
  return auxiliary_objectives_.size();
}

inline AuxiliaryObjectiveId ObjectiveStorage::next_id() const {
  return next_id_;
}

void ObjectiveStorage::ensure_next_id_at_least(
    const AuxiliaryObjectiveId minimum) {
  next_id_ = std::max(minimum, next_id_);
}

bool ObjectiveStorage::contains(const AuxiliaryObjectiveId id) const {
  return auxiliary_objectives_.contains(id);
}

template <typename DiffIter>
void ObjectiveStorage::Clear(const ObjectiveId id,
                             const iterator_range<DiffIter>& diffs) {
  ObjectiveData& data = objective(id);
  set_offset(id, 0.0, diffs);
  for (ObjectiveStorage::Diff& diff : diffs) {
    if (!diff.objective_tracked(id)) {
      continue;
    }
    for (const auto [var, _] : data.linear_terms.terms()) {
      if (var < diff.variable_checkpoint) {
        diff.objective_diffs[id].linear_coefficients.insert(var);
      }
    }
    for (const auto [v1, v2, _] : data.quadratic_terms.Terms()) {
      if (v2 < diff.variable_checkpoint) {  // v1 <= v2 is implied
        diff.objective_diffs[id].quadratic_coefficients.insert({v1, v2});
      }
    }
  }
  data.linear_terms.clear();
  data.quadratic_terms.Clear();
}

template <typename DiffIter>
void ObjectiveStorage::DeleteVariable(const VariableId variable,
                                      const iterator_range<DiffIter>& diffs) {
  for (Diff& diff : diffs) {
    for (auto& [id, obj_diff_data] : diff.objective_diffs) {
      obj_diff_data.DeleteVariable(variable, diff.variable_checkpoint,
                                   quadratic_terms(id));
    }
  }
  primary_objective_.DeleteVariable(variable);
  for (auto& [unused, aux_obj] : auxiliary_objectives_) {
    aux_obj.DeleteVariable(variable);
  }
}

bool ObjectiveStorage::diff_is_empty(const Diff& diff) const {
  if (next_id_ > diff.objective_checkpoint) {
    // There is a new auxiliary objective that needs extracting.
    return false;
  }
  for (const auto& [unused, diff_data] : diff.objective_diffs) {
    if (!diff_data.empty()) {
      // We must apply an objective modification.
      return false;
    }
  }
  // If nonempty we need to delete some auxiliary objectives.
  return diff.deleted.empty();
}

bool ObjectiveStorage::Diff::objective_tracked(const ObjectiveId id) const {
  // The primary objective is always present, so updates are always exported.
  if (id == kPrimaryObjectiveId) {
    return true;
  }
  return id < objective_checkpoint;
}

const ObjectiveStorage::ObjectiveData& ObjectiveStorage::objective(
    ObjectiveId id) const {
  if (id == kPrimaryObjectiveId) {
    return primary_objective_;
  }
  const auto it = auxiliary_objectives_.find(*id);
  CHECK(it != auxiliary_objectives_.end());
  return it->second;
}

ObjectiveStorage::ObjectiveData& ObjectiveStorage::objective(ObjectiveId id) {
  if (id == kPrimaryObjectiveId) {
    return primary_objective_;
  }
  const auto it = auxiliary_objectives_.find(*id);
  CHECK(it != auxiliary_objectives_.end());
  return it->second;
}

void ObjectiveStorage::ObjectiveData::DeleteVariable(VariableId variable) {
  linear_terms.erase(variable);
  quadratic_terms.Delete(variable);
}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_STORAGE_OBJECTIVE_STORAGE_H_
