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

#ifndef OR_TOOLS_MATH_OPT_STORAGE_ATOMIC_CONSTRAINT_STORAGE_H_
#define OR_TOOLS_MATH_OPT_STORAGE_ATOMIC_CONSTRAINT_STORAGE_H_

#include <algorithm>
#include <cstdint>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "google/protobuf/map.h"
#include "absl/log/check.h"
#include "ortools/base/map_util.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/storage/model_storage_types.h"
#include "ortools/math_opt/storage/range.h"
#include "ortools/math_opt/storage/sorted.h"

namespace operations_research::math_opt {

// Storage for a "mapped" constraint type whose only supported updates are
// constraint addition, and variable or constraint deletion.
//
// The constraints are "atomic" in the sense that they can be added or deleted
// individually, but direct data updates (e.g., to coefficients) are not
// permitted. Note that they are strictly immutable, though, as variable
// deletions may have side effects (e.g., a constraint considers a deleted
// variable as implicitly fixed to zero).
//
// Implementers of new constraint families should provide a conforming
// `ConstraintData` definition, along with a template specialization of
// `AtomicConstraintTraits` (defined below). These should likely be placed in
// `math_opt/constraints/$new_constraint_family/storage.h`.
//
// The `ConstraintData` parameter should refer to a class to represent a single
// constraint in memory. It must satisfy a duck-typed interface:
//  * A `IdType` alias to the (strong int) ID type for the constraint class.
//  * A `ProtoType` alias to the proto message for a single constraint.
//  * A `UpdatesProtoType` alias to the proto message for updates for the given
//    constraint type, as represented by two fields:
//      - repeated int64_t deleted_constraint_ids
//      - Map<int64_t, ProtoType> new_constraints
//  * A member function to return all variables involved in the constraint:
//      std::vector<VariableId> RelatedVariables() const;
//  * A member function to delete a variable from the constraint:
//      void DeleteVariable(VariableId var);
//  * A member function that returns a proto representation of the constraint:
//      ProtoType Proto() const;
//  * A static member function that initializes a constraint from its proto
//    representation:
//      static ConstraintData FromProto(const ProtoType& in_proto);
//
// The setter functions all accept a `DiffIter`, which must be an iterator over
// non-const references to AtomicConstraintStorage<ConstraintData>::Diff. These
// functions will modify the diff objects.
template <typename ConstraintData>
class AtomicConstraintStorage {
 public:
  using IdType = typename ConstraintData::IdType;
  using ProtoType = typename ConstraintData::ProtoType;
  using UpdatesProtoType = typename ConstraintData::UpdatesProtoType;

  // Tracks a "checkpoint" and changes to constraints of a given class that are
  // before the checkpoint. Advancing the checkpoint throws away tracked
  // changes.
  //
  // An instance of this class is owned by each update tracker of ModelStorage.
  struct Diff {
    explicit Diff(const AtomicConstraintStorage<ConstraintData>& storage)
        : checkpoint(storage.next_id()) {}

    IdType checkpoint{0};
    absl::flat_hash_set<IdType> deleted_constraints;
  };

  // Add a single constraint to the storage.
  IdType AddConstraint(ConstraintData constraint);

  // Adds a collection of constraints to the storage, from an "id-to-proto" map.
  // The keys for the input map will be used as the associated IDs in storage.
  //
  // Will CHECK-fail if any ID is less than next_id().
  void AddConstraints(
      const google::protobuf::Map<int64_t, ProtoType>& constraints);

  // Delete a single constraint.
  template <typename DiffIter>
  void Delete(IdType id, const iterator_range<DiffIter>& diffs);

  // Delete a single variable from each constraint in the storage.
  void DeleteVariable(VariableId variable_id);

  // The number of constraints stored (includes everything created and not yet
  // deleted).
  int64_t size() const { return constraint_data_.size(); }

  // The smallest ID which is valid for a new constraint.
  IdType next_id() const { return next_id_; }

  // Sets the next variable id to be the maximum of next_id() and `minimum`.
  void ensure_next_id_at_least(const IdType minimum) {
    next_id_ = std::max(minimum, next_id_);
  }

  // Returns true if this id has been created and not yet deleted.
  bool contains(const IdType id) const { return constraint_data_.contains(id); }

  const absl::flat_hash_set<IdType>& RelatedConstraints(
      const VariableId variable_id) const {
    return gtl::FindWithDefault(constraints_by_variable_, variable_id);
  }

  // The ids in use (not deleted). The order is not defined.
  std::vector<IdType> Constraints() const;

  // Returns a sorted vector of all existing (not deleted) constraints in the
  // model.
  //
  // Runs in O(n log(n)), where n is the number of constraints returned.
  std::vector<IdType> SortedConstraints() const;

  // Returns a proto representation of the constraint class.
  google::protobuf::Map<int64_t, ProtoType> Proto() const {
    google::protobuf::Map<int64_t, ProtoType> constraints;
    for (const auto& [id, data] : constraint_data_) {
      constraints[id.value()] = data.Proto();
    }
    return constraints;
  }

  // Returns the underlying data for constraint `id`.
  // Will crash if `id` is not present (i.e., if `contains(id)` returns false).
  const ConstraintData& data(const IdType id) const {
    return constraint_data_.at(id);
  }

  //////////////////////////////////////////////////////////////////////////////
  // Functions for working with Diff
  //////////////////////////////////////////////////////////////////////////////

  // Returns true if there are no changes (tracked changes before the checkpoint
  // or new constraints after the checkpoint).
  inline bool diff_is_empty(const Diff& diff) const;

  // Return a proto representation of the current update.
  typename ConstraintData::UpdatesProtoType Update(const Diff& diff) const;

  // Updates the checkpoint and clears all stored changes in diff.
  void AdvanceCheckpointInDiff(Diff& diff) const;

 private:
  IdType next_id_{0};
  absl::flat_hash_map<IdType, ConstraintData> constraint_data_;
  // TODO(b/232619901): Use std::vector<IdType> as values and lazily compact it.
  absl::flat_hash_map<VariableId, absl::flat_hash_set<IdType>>
      constraints_by_variable_;
};

// A placeholder templated definition for traits-based parameter inference when
// working with atomic constraint families.
//
// For template specializations on `IdType`: the `ConstraintData` typedef must
// satisfy IdType == AtomicConstraintStorage<ConstraintData>::IdType.
template <typename IdType>
struct AtomicConstraintTraits {
  using ConstraintData = void;
};

////////////////////////////////////////////////////////////////////////////////
// Inlined implementations
////////////////////////////////////////////////////////////////////////////////

template <typename ConstraintData>
typename AtomicConstraintStorage<ConstraintData>::IdType
AtomicConstraintStorage<ConstraintData>::AddConstraint(
    ConstraintData constraint) {
  const std::vector<VariableId> vars = constraint.RelatedVariables();
  const IdType id = next_id_++;
  CHECK(constraint_data_.insert({id, std::move(constraint)}).second);
  for (const VariableId v : vars) {
    constraints_by_variable_[v].insert(id);
  }
  return id;
}

template <typename ConstraintData>
void AtomicConstraintStorage<ConstraintData>::AddConstraints(
    const google::protobuf::Map<int64_t, ProtoType>& constraints) {
  for (const int64_t raw_id : SortedMapKeys(constraints)) {
    const IdType id(raw_id);
    CHECK_GE(id, next_id())
        << "constraint ID in map: " << raw_id
        << " is smaller than next_id(): " << next_id().value();
    ensure_next_id_at_least(id);
    AddConstraint(ConstraintData::FromProto(constraints.at(raw_id)));
  }
}

template <typename ConstraintData>
template <typename DiffIter>
void AtomicConstraintStorage<ConstraintData>::Delete(
    IdType id, const iterator_range<DiffIter>& diffs) {
  for (Diff& diff : diffs) {
    // if the constraint >= checkpoint_, we don't store any info.
    if (id >= diff.checkpoint) {
      continue;
    }
    diff.deleted_constraints.insert(id);
  }
  const auto data = constraint_data_.find(id);
  CHECK(data != constraint_data_.end());
  for (const VariableId v : data->second.RelatedVariables()) {
    constraints_by_variable_[v].erase(id);
  }
  constraint_data_.erase(id);
}

template <typename ConstraintData>
void AtomicConstraintStorage<ConstraintData>::DeleteVariable(
    VariableId variable_id) {
  const auto it = constraints_by_variable_.find(variable_id);
  if (it == constraints_by_variable_.end()) {
    return;
  }
  for (const IdType constraint_id : it->second) {
    constraint_data_.at(constraint_id).DeleteVariable(variable_id);
  }
  constraints_by_variable_.erase(it);
}

template <typename ConstraintData>
std::vector<typename AtomicConstraintStorage<ConstraintData>::IdType>
AtomicConstraintStorage<ConstraintData>::Constraints() const {
  std::vector<IdType> result;
  for (const auto& [id, _] : constraint_data_) {
    result.push_back(id);
  }
  return result;
}

template <typename ConstraintData>
std::vector<typename AtomicConstraintStorage<ConstraintData>::IdType>
AtomicConstraintStorage<ConstraintData>::SortedConstraints() const {
  std::vector<IdType> result = Constraints();
  absl::c_sort(result);
  return result;
}

template <typename ConstraintData>
bool AtomicConstraintStorage<ConstraintData>::diff_is_empty(
    const Diff& diff) const {
  return next_id_ <= diff.checkpoint && diff.deleted_constraints.empty();
}

template <typename ConstraintData>
void AtomicConstraintStorage<ConstraintData>::AdvanceCheckpointInDiff(
    Diff& diff) const {
  diff.checkpoint = std::max(diff.checkpoint, next_id_);
  diff.deleted_constraints.clear();
}

template <typename ConstraintData>
typename ConstraintData::UpdatesProtoType
AtomicConstraintStorage<ConstraintData>::Update(const Diff& diff) const {
  UpdatesProtoType update;
  for (const IdType deleted_id : diff.deleted_constraints) {
    update.mutable_deleted_constraint_ids()->Add(deleted_id.value());
  }
  absl::c_sort(*update.mutable_deleted_constraint_ids());
  for (const IdType id :
       util_intops::MakeStrongIntRange(diff.checkpoint, next_id_)) {
    if (contains(id)) {
      (*update.mutable_new_constraints())[id.value()] = data(id).Proto();
    }
  }
  return update;
}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_STORAGE_ATOMIC_CONSTRAINT_STORAGE_H_
