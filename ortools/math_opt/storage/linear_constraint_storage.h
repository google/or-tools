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

#ifndef OR_TOOLS_MATH_OPT_STORAGE_LINEAR_CONSTRAINT_STORAGE_H_
#define OR_TOOLS_MATH_OPT_STORAGE_LINEAR_CONSTRAINT_STORAGE_H_

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/string_view.h"
#include "absl/log/check.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/model_storage_types.h"
#include "ortools/math_opt/storage/range.h"
#include "ortools/math_opt/storage/sparse_matrix.h"

namespace operations_research::math_opt {

// In memory representation of the linear constraints of an optimization model.
//
// The setter functions all accept a `DiffIter`, which must be an iterator over
// non-const references to LinearConstraintStorage::Diff. These
// functions will modify the LinearConstraintStorage::Diff objects.
class LinearConstraintStorage {
 public:
  // Tracks a "checkpoint" and changes to linear constraints that are before the
  // checkpoint. Advancing the checkpoint throws away tracked changes.
  //
  // An instance of this class is owned by each update tracker of ModelStorage.
  struct Diff {
    // Note: no reference to storage is held.
    Diff(const LinearConstraintStorage& storage,
         VariableId variable_checkpoint);

    LinearConstraintId checkpoint{0};
    VariableId variable_checkpoint{0};
    absl::flat_hash_set<LinearConstraintId> deleted;
    absl::flat_hash_set<LinearConstraintId> lower_bounds;
    absl::flat_hash_set<LinearConstraintId> upper_bounds;
    // Only for pairs where both the variable and constraint are before the
    // checkpoint, i.e.
    //   var_id < variables_checkpoint_  &&
    //   lin_con_id < linear_constraints_checkpoint_
    absl::flat_hash_set<std::pair<LinearConstraintId, VariableId>> matrix_keys;
  };

  struct UpdateResult {
    google::protobuf::RepeatedField<int64_t> deleted;
    LinearConstraintUpdatesProto updates;
    LinearConstraintsProto creates;
    SparseDoubleMatrixProto matrix_updates;
  };

  // Adds a linear constraint to the model and returns its id.
  //
  // The returned ids begin at zero and strictly increase (in particular, if
  // ensure_next_id_at_least() is not used, they will be consecutive). Deleted
  // ids are NOT reused.
  LinearConstraintId Add(double lower_bound, double upper_bound,
                         absl::string_view name);

  inline double lower_bound(LinearConstraintId id) const;
  inline double upper_bound(LinearConstraintId id) const;
  inline const std::string& name(LinearConstraintId id) const;

  template <typename DiffIter>
  void set_lower_bound(LinearConstraintId id, double lower_bound,
                       const iterator_range<DiffIter>& diffs);

  template <typename DiffIter>
  void set_upper_bound(LinearConstraintId id, double upper_bound,
                       const iterator_range<DiffIter>& diffs);

  // Removes a linear constraint from the model.
  //
  // It is an error to use a deleted linear constraint id as input to any
  // subsequent function calls on the model.
  template <typename DiffIter>
  void Delete(LinearConstraintId id, const iterator_range<DiffIter>& diffs);

  // The number of linear constraints in the model.
  //
  // Equal to the number of linear constraints created minus the number of
  // linear constraints deleted.
  inline int64_t size() const;

  // The returned id of the next call to AddLinearConstraint.
  //
  // Equal to the number of linear constraints created.
  inline LinearConstraintId next_id() const;

  // Sets the next variable id to be the maximum of next_id() and `minimum`.
  inline void ensure_next_id_at_least(LinearConstraintId minimum);

  // Returns true if this id has been created and not yet deleted.
  inline bool contains(LinearConstraintId id) const;

  // The LinearConstraintsIds in use (not deleted), order not defined.
  std::vector<LinearConstraintId> LinearConstraints() const;

  // Returns a sorted vector of all existing (not deleted) linear constraints in
  // the model.
  //
  // Runs in O(n log(n)), where n is the number of linear constraints returned.
  std::vector<LinearConstraintId> SortedLinearConstraints() const;

  // Removes all occurrences of var from the constraint matrix.
  template <typename DiffIter>
  void DeleteVariable(VariableId variable,
                      const iterator_range<DiffIter>& diffs);

  // Setting value=0 deletes the key from the matrix.
  template <typename DiffIter>
  void set_term(LinearConstraintId constraint, VariableId variable,
                double value, const iterator_range<DiffIter>& diffs);

  // The matrix of coefficients for the linear terms in the constraints.
  const SparseMatrix<LinearConstraintId, VariableId>& matrix() const {
    return matrix_;
  }

  // Returns an equivalent proto of `this`.
  std::pair<LinearConstraintsProto, SparseDoubleMatrixProto> Proto() const;

  //////////////////////////////////////////////////////////////////////////////
  // Functions for working with Diff
  //////////////////////////////////////////////////////////////////////////////

  // Returns true if there are no changes (tracked changes before the checkpoint
  // or new constraints after the checkpoint).
  //
  // NOTE: when a linear constraint coefficient is modified for a variable past
  // the checkpoint, the Diff object can be empty (and diff_is_empty will return
  // true), but Update() can return a non-empty UpdateResult. This behavior
  // MAY CHANGE in the future, so diff_is_empty is true iff the UpdateResult
  // returned by Update() is empty (a more intuitive API, harder to implement
  // efficiently).
  inline bool diff_is_empty(const Diff& diff) const;

  UpdateResult Update(const Diff& diff,
                      const absl::flat_hash_set<VariableId>& deleted_variables,
                      const std::vector<VariableId>& new_variables) const;

  // Updates the checkpoint and clears all stored changes in diff.
  void AdvanceCheckpointInDiff(VariableId variable_checkpoint,
                               Diff& diff) const;

 private:
  struct Data {
    double lower_bound = -std::numeric_limits<double>::infinity();
    double upper_bound = std::numeric_limits<double>::infinity();
    std::string name;
  };

  std::vector<LinearConstraintId> ConstraintsFrom(
      const LinearConstraintId start) const;

  void AppendConstraint(LinearConstraintId constraint,
                        LinearConstraintsProto* proto) const;

  // Returns a proto representation of the constraints with id in [start, end).
  // (Note: the linear coefficients must be queried separately).
  LinearConstraintsProto Proto(LinearConstraintId start,
                               LinearConstraintId end) const;

  LinearConstraintId next_id_{0};
  absl::flat_hash_map<LinearConstraintId, Data> linear_constraints_;
  SparseMatrix<LinearConstraintId, VariableId> matrix_;
};

////////////////////////////////////////////////////////////////////////////////
// Inline function implementation
////////////////////////////////////////////////////////////////////////////////

double LinearConstraintStorage::lower_bound(const LinearConstraintId id) const {
  return linear_constraints_.at(id).lower_bound;
}

double LinearConstraintStorage::upper_bound(const LinearConstraintId id) const {
  return linear_constraints_.at(id).upper_bound;
}

const std::string& LinearConstraintStorage::name(
    const LinearConstraintId id) const {
  return linear_constraints_.at(id).name;
}

template <typename DiffIter>
void LinearConstraintStorage::set_lower_bound(
    const LinearConstraintId id, const double lower_bound,
    const iterator_range<DiffIter>& diffs) {
  const auto it = linear_constraints_.find(id);
  if (it->second.lower_bound == lower_bound) {
    return;
  }
  it->second.lower_bound = lower_bound;
  for (Diff& diff : diffs) {
    if (id < diff.checkpoint) {
      diff.lower_bounds.insert(id);
    }
  }
}

template <typename DiffIter>
void LinearConstraintStorage::set_upper_bound(
    const LinearConstraintId id, const double upper_bound,
    const iterator_range<DiffIter>& diffs) {
  const auto it = linear_constraints_.find(id);
  if (it->second.upper_bound == upper_bound) {
    return;
  }
  it->second.upper_bound = upper_bound;
  for (Diff& diff : diffs) {
    if (id < diff.checkpoint) {
      diff.upper_bounds.insert(id);
    }
  }
}

template <typename DiffIter>
void LinearConstraintStorage::Delete(const LinearConstraintId id,
                                     const iterator_range<DiffIter>& diffs) {
  for (Diff& diff : diffs) {
    // if the constraint >= checkpoint_, we don't store any info.
    if (id >= diff.checkpoint) {
      continue;
    }
    diff.lower_bounds.erase(id);
    diff.upper_bounds.erase(id);
    diff.deleted.insert(id);
    for (const VariableId row_var : matrix_.row(id)) {
      if (row_var < diff.variable_checkpoint) {
        diff.matrix_keys.erase({id, row_var});
      }
    }
  }
  matrix_.DeleteRow(id);
  linear_constraints_.erase(id);
}

template <typename DiffIter>
void LinearConstraintStorage::DeleteVariable(
    const VariableId variable, const iterator_range<DiffIter>& diffs) {
  for (Diff& diff : diffs) {
    if (variable >= diff.variable_checkpoint) {
      continue;
    }
    for (const LinearConstraintId constraint : matrix_.column(variable)) {
      if (constraint < diff.checkpoint) {
        diff.matrix_keys.erase({constraint, variable});
      }
    }
  }
  matrix_.DeleteColumn(variable);
}

int64_t LinearConstraintStorage::size() const {
  return linear_constraints_.size();
}

LinearConstraintId LinearConstraintStorage::next_id() const { return next_id_; }

void LinearConstraintStorage::ensure_next_id_at_least(
    const LinearConstraintId minimum) {
  next_id_ = std::max(minimum, next_id_);
}

bool LinearConstraintStorage::contains(const LinearConstraintId id) const {
  return linear_constraints_.contains(id);
}

template <typename DiffIter>
void LinearConstraintStorage::set_term(const LinearConstraintId constraint,
                                       const VariableId variable,
                                       const double value,
                                       const iterator_range<DiffIter>& diffs) {
  DCHECK(linear_constraints_.contains(constraint));
  if (!matrix_.set(constraint, variable, value)) {
    return;
  }
  for (Diff& diff : diffs) {
    if (constraint < diff.checkpoint && variable < diff.variable_checkpoint) {
      diff.matrix_keys.insert({constraint, variable});
    }
  }
}

bool LinearConstraintStorage::diff_is_empty(const Diff& diff) const {
  return next_id_ <= diff.checkpoint && diff.deleted.empty() &&
         diff.lower_bounds.empty() && diff.upper_bounds.empty() &&
         diff.matrix_keys.empty();
}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_STORAGE_LINEAR_CONSTRAINT_STORAGE_H_
