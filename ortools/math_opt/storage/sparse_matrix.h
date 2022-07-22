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

// Classes for modeling sparse matrices.
#ifndef OR_TOOLS_MATH_OPT_STORAGE_SPARSE_MATRIX_H_
#define OR_TOOLS_MATH_OPT_STORAGE_SPARSE_MATRIX_H_

#include <algorithm>
#include <cstdint>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/meta/type_traits.h"
#include "absl/types/span.h"
#include "ortools/base/map_util.h"
#include "ortools/base/strong_int.h"
#include "ortools/base/strong_vector.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/model_storage_types.h"

namespace operations_research::math_opt {

// A sparse symmetric double valued matrix over VariableIds.
//
// Note that the matrix is sparse in both:
//  * The IDs of the rows/columns (both VariableIds), stored as flat_hash_map.
//  * The entries with nonzero value.
//
// Getting/setting/clearing entries are O(1) operations. Getting a row of the
// matrix runs in O(size of the row) if nothing has been deleted, and getting
// all the rows runs in O(number of nonzero entries), even with deletions
// (with deletions, accessing a particular row with many deletions may be slow).
//
// Implementation: The entries are stored in a
// flat_hash_map<pair<VariableId, VariableId>, double> `values_` where for each
// key, key.first <= key.second. Additionally, we maintain a
// flat_hash_map<VariableId, vector<VariableId>> `related_variables_` that says
// for each variable, which variables they have a nonzero term with. When a
// coefficient is set to zero or a variable is deleted, we do not immediately
// delete the data from values_ or related_variables_, we simply set the
// coefficient to zero in values_. We track how many zeros are in values_, and
// when more than some constant fraction of all entries are zero (see
// kZerosCleanup in cc file), we clean up related_variables_ and values_ to
// remove all the zeros. Iteration over the rows or total entries of the matrix
// must check for zeros in values_ and skip these terms.
//
// Memory use:
//   * 3*8 bytes per nonzero plus hash capacity overhead for values_.
//   * 2*8 bytes per nonzero plus vector capacity overhead for
//     related_variables_.
//   * ~5*8 bytes per variable participating in any quadratic term; one heap
//     allocation per such variable.
class SparseSymmetricMatrix {
 public:
  // Setting `value` to zero removes the value from the matrix.
  // Returns true if `value` is different from the existing value in the matrix.
  inline bool set(VariableId first, VariableId second, double value);

  // Zero is returned if the value is not present.
  inline double get(VariableId first, VariableId second) const;

  // Zeros out all coefficients for this variable.
  void Delete(VariableId variable);

  // Returns the variables that have nonzero entries with `variable`.
  //
  // The return order is deterministic but not defined.
  // TODO(b/233630053): expose an iterator based API to avoid making a copy.
  std::vector<VariableId> RelatedVariables(VariableId variable) const;

  // Returns the variable value pairs (x, c) where `variable` and x have nonzero
  // coefficient c.
  //
  // The return order is deterministic but not defined.
  // TODO(b/233630053): expose an iterator based API to avoid making a copy.
  std::vector<std::pair<VariableId, double>> Terms(VariableId variable) const;

  // Returns (x, y, c) tuples where variables x and y have nonzero coefficient
  // c, and x <= y.
  //
  // The return order is non-deterministic and not defined.
  // TODO(b/233630053): expose an iterator based API to avoid making a copy.
  std::vector<std::tuple<VariableId, VariableId, double>> Terms() const;

  // Removes all terms from the matrix.
  void Clear();

  // The number of (var, var) keys with nonzero value. Note that (x, y) and
  // (y, x) are the same key.
  int64_t nonzeros() const { return nonzeros_; }

  // Visible for testing, do not depend on this.
  int64_t compactions() const { return compactions_; }

  // TODO(b/233630053): do not expose values_ directly, instead offer a way to
  // iterate over all the nonzero entries.
  // Warning: this map will contain zeros.
  const absl::flat_hash_map<std::pair<VariableId, VariableId>, double>& values()
      const {
    return values_;
  }

  SparseDoubleMatrixProto Proto() const;

  template <typename VarContainer>
  inline SparseDoubleMatrixProto Update(
      const VarContainer& variables, VariableId checkpoint, VariableId next_var,
      const absl::flat_hash_set<std::pair<VariableId, VariableId>>& dirty)
      const;

 private:
  inline std::pair<VariableId, VariableId> make_key(VariableId first,
                                                    VariableId second) const;
  void CompactIfNeeded();

  // The keys of values_ have key.first <= key.second.
  absl::flat_hash_map<std::pair<VariableId, VariableId>, double> values_;
  absl::flat_hash_map<VariableId, std::vector<VariableId>> related_variables_;

  // The number of nonzero elements in values_.
  int64_t nonzeros_ = 0;
  int64_t compactions_ = 0;
};

////////////////////////////////////////////////////////////////////////////////
// Inlined functions
////////////////////////////////////////////////////////////////////////////////

std::pair<VariableId, VariableId> SparseSymmetricMatrix::make_key(
    const VariableId first, const VariableId second) const {
  return {std::min(first, second), std::max(first, second)};
}

bool SparseSymmetricMatrix::set(const VariableId first, const VariableId second,
                                const double value) {
  const std::pair<VariableId, VariableId> key = make_key(first, second);
  auto map_iter = values_.find(key);

  if (map_iter == values_.end()) {
    if (value == 0.0) {
      return false;
    }
    related_variables_[first].push_back(second);
    if (first != second) {
      related_variables_[second].push_back(first);
    }
    values_[key] = value;
    nonzeros_++;
    return true;
  } else {
    if (map_iter->second == value) {
      return false;
    }
    const double old_value = map_iter->second;
    map_iter->second = value;
    if (value == 0.0) {
      nonzeros_--;
      CompactIfNeeded();
    } else if (old_value == 0.0) {
      nonzeros_++;
    }
    return true;
  }
}

double SparseSymmetricMatrix::get(const VariableId first,
                                  const VariableId second) const {
  return gtl::FindWithDefault(values_, make_key(first, second));
}

template <typename VarContainer>
SparseDoubleMatrixProto SparseSymmetricMatrix::Update(
    const VarContainer& variables, const VariableId checkpoint,
    const VariableId next_var,
    const absl::flat_hash_set<std::pair<VariableId, VariableId>>& dirty) const {
  std::vector<std::pair<VariableId, VariableId>> updates;
  for (const std::pair<VariableId, VariableId> pair : dirty) {
    // If either variable has been deleted, don't add it.
    if (variables.contains(pair.first) && variables.contains(pair.second)) {
      updates.push_back(pair);
    }
  }

  for (const VariableId v : MakeStrongIntRange(checkpoint, next_var)) {
    if (related_variables_.contains(v)) {
      // TODO(b/233630053): do not allocate here.
      for (const VariableId other : RelatedVariables(v)) {
        if (v <= other) {
          updates.push_back({v, other});
        } else if (other < checkpoint) {
          updates.push_back({other, v});
        }
      }
    }
  }
  absl::c_sort(updates);
  SparseDoubleMatrixProto result;
  for (const auto [row, col] : updates) {
    result.add_row_ids(row.value());
    result.add_column_ids(col.value());
    result.add_coefficients(get(row, col));
  }
  return result;
}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_STORAGE_SPARSE_MATRIX_H_
