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
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
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
    explicit Diff(const VariableId variable_checkpoint)
        : variable_checkpoint(variable_checkpoint) {}

    VariableId variable_checkpoint{0};
    bool direction = false;
    bool offset = false;
    // Only holds variables before the variable checkpoint.
    absl::flat_hash_set<VariableId> linear_coefficients;

    // For each entry, first <= second (the matrix is symmetric).
    // Only holds entries with both variables before the variable checkpoint.
    absl::flat_hash_set<std::pair<VariableId, VariableId>>
        quadratic_coefficients;
  };

  bool maximize() const { return maximize_; }
  double offset() const { return offset_; }
  inline double linear_term(VariableId v) const;

  const absl::flat_hash_map<VariableId, double>& linear_terms() const {
    return linear_terms_.terms();
  }
  double quadratic_term(const VariableId v1, const VariableId v2) const {
    return quadratic_terms_.get(v1, v2);
  }
  const SparseSymmetricMatrix& quadratic_terms() const {
    return quadratic_terms_;
  }

  template <typename DiffIter>
  void set_maximize(bool maximize, const iterator_range<DiffIter>& diffs);

  template <typename DiffIter>
  void set_offset(double offset, const iterator_range<DiffIter>& diffs);

  template <typename DiffIter>
  void set_linear_term(VariableId variable, double value,
                       const iterator_range<DiffIter>& diffs);

  template <typename DiffIter>
  void set_quadratic_term(VariableId v1, VariableId v2, double val,
                          const iterator_range<DiffIter>& diffs);

  template <typename DiffIter>
  void Clear(const iterator_range<DiffIter>& diffs);

  // Removes all occurrences of var from the objective.
  template <typename DiffIter>
  void DeleteVariable(VariableId variable,
                      const iterator_range<DiffIter>& diffs);

  ObjectiveProto Proto() const;

  //////////////////////////////////////////////////////////////////////////////
  // Functions for working with Diff
  //////////////////////////////////////////////////////////////////////////////

  // Returns true if there are no changes (tracked changes before the
  // checkpoint).
  //
  // NOTE: when there are new variables with nonzero objective coefficient, the
  // Diff object can be empty (and diff_is_empty will return true), but Update()
  // can return a non-empty ObjectiveUpdatesProto. This behavior MAY CHANGE in
  // the future, so diff_is_empty is true iff Update() returns an empty
  // ObjectiveUpdatesProto (a more intuitive API, harder to implement
  // efficiently).
  inline bool diff_is_empty(const Diff& diff) const;

  ObjectiveUpdatesProto Update(
      const Diff& diff,
      const absl::flat_hash_set<VariableId>& deleted_variables,
      const std::vector<VariableId>& new_variables) const;

  // Updates the checkpoint and clears all stored changes in diff.
  void AdvanceCheckpointInDiff(VariableId variable_checkpoint,
                               Diff& diff) const;

 private:
  bool maximize_ = false;
  double offset_ = 0.0;
  SparseCoefficientMap linear_terms_;
  SparseSymmetricMatrix quadratic_terms_;
};

////////////////////////////////////////////////////////////////////////////////
// Inline function implementation
////////////////////////////////////////////////////////////////////////////////

double ObjectiveStorage::linear_term(const VariableId v) const {
  return linear_terms_.get(v);
}

template <typename DiffIter>
void ObjectiveStorage::set_maximize(const bool maximize,
                                    const iterator_range<DiffIter>& diffs) {
  if (maximize_ == maximize) {
    return;
  }
  maximize_ = maximize;
  for (ObjectiveStorage::Diff& diff : diffs) {
    diff.direction = true;
  }
}

template <typename DiffIter>
void ObjectiveStorage::set_offset(const double offset,
                                  const iterator_range<DiffIter>& diffs) {
  if (offset_ == offset) {
    return;
  }
  offset_ = offset;
  for (ObjectiveStorage::Diff& diff : diffs) {
    diff.offset = true;
  }
}

template <typename DiffIter>
void ObjectiveStorage::set_linear_term(const VariableId variable,
                                       const double value,
                                       const iterator_range<DiffIter>& diffs) {
  if (linear_terms_.set(variable, value)) {
    for (ObjectiveStorage::Diff& diff : diffs) {
      if (variable < diff.variable_checkpoint) {
        diff.linear_coefficients.insert(variable);
      }
    }
  }
}

template <typename DiffIter>
void ObjectiveStorage::set_quadratic_term(
    const VariableId v1, const VariableId v2, const double val,
    const iterator_range<DiffIter>& diffs) {
  if (quadratic_terms_.set(v1, v2, val)) {
    for (ObjectiveStorage::Diff& diff : diffs) {
      if (v1 < diff.variable_checkpoint && v2 < diff.variable_checkpoint) {
        diff.quadratic_coefficients.insert(
            {std::min(v1, v2), std::max(v1, v2)});
      }
    }
  }
}

template <typename DiffIter>
void ObjectiveStorage::Clear(const iterator_range<DiffIter>& diffs) {
  set_offset(0.0, diffs);
  for (ObjectiveStorage::Diff& diff : diffs) {
    for (const auto [var, _] : linear_terms_.terms()) {
      if (var < diff.variable_checkpoint) {
        diff.linear_coefficients.insert(var);
      }
    }
    for (const auto [v1, v2, _] : quadratic_terms_.Terms()) {
      if (v2 < diff.variable_checkpoint) {  // v1 <= v2 is implied
        diff.quadratic_coefficients.insert({v1, v2});
      }
    }
  }
  linear_terms_.clear();
  quadratic_terms_.Clear();
}

template <typename DiffIter>
void ObjectiveStorage::DeleteVariable(const VariableId variable,
                                      const iterator_range<DiffIter>& diffs) {
  for (ObjectiveStorage::Diff& diff : diffs) {
    if (variable >= diff.variable_checkpoint) {
      continue;
    }
    diff.linear_coefficients.erase(variable);
    for (const VariableId v2 : quadratic_terms_.RelatedVariables(variable)) {
      if (v2 < diff.variable_checkpoint) {
        diff.quadratic_coefficients.erase(
            {std::min(variable, v2), std::max(variable, v2)});
      }
    }
  }
  linear_terms_.erase(variable);
  quadratic_terms_.Delete(variable);
}

bool ObjectiveStorage::diff_is_empty(const Diff& diff) const {
  return !diff.offset && !diff.direction && diff.linear_coefficients.empty() &&
         diff.quadratic_coefficients.empty();
}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_STORAGE_OBJECTIVE_STORAGE_H_
