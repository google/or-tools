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

  // Returns the variables with any nonzero in the matrix.
  //
  // The return order is deterministic but not defined.
  // TODO(b/233630053): expose an iterator based API to avoid making a copy.
  std::vector<VariableId> Variables() const;

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

  // For testing/debugging only, do not depend on this value, behavior may
  // change based on implementation.
  int64_t impl_detail_matrix_storage_size() const { return values_.size(); }

  // TODO(b/233630053): do not expose values_ directly, instead offer a way to
  // iterate over all the nonzero entries.
  // Warning: this map will contain zeros.
  const absl::flat_hash_map<std::pair<VariableId, VariableId>, double>& values()
      const {
    return values_;
  }

  SparseDoubleMatrixProto Proto() const;

  SparseDoubleMatrixProto Update(
      const absl::flat_hash_set<VariableId>& deleted_variables,
      absl::Span<const VariableId> new_variables,
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
};

// A sparse double valued matrix over int-like rows and columns.
//
// Note that the matrix is sparse in both:
//  * The IDs of the rows/columns, stored as flat_hash_map.
//  * The entries with nonzero value.
//
// Getting/setting/clearing entries are O(1) operations. Getting a row of the
// matrix runs in O(size of the row) if nothing has been deleted, and getting
// all the rows runs in O(number of nonzero entries), even with deletions
// (with deletions, accessing a particular row or columns with many deletions
// may be slow).
//
// Implementation: The entries are stored in a
// flat_hash_map<pair<RowId, ColumnId>, double> `values_`. Additionally, we
// maintain a flat_hash_map<RowId, vector<ColumnId>> `rows_` and a
// flat_hash_map<ColumnId, vector<RowID>> `columns_` that enable efficient
// queries of the nonzeros in any row or column. When a coefficient is set to
// zero or a variable is deleted, we do not immediately delete the data from
// `values_`, `rows_`, or `columns_`, we simply set the coefficient to zero in
// `values_`. We track how many zeros are in `values_`, and when more than some
// constant fraction of all entries are zero (see `internal::kZerosCleanup`), we
// clean up `rows_`, `columns_`, and `values_` to remove all the zeros.
// Iteration over the rows or total entries of the matrix must check for zeros
// in values_ and skip these terms.
//
// Memory use:
//   * 3*8 bytes per nonzero plus hash capacity overhead for values_.
//   * 2*8 bytes per nonzero plus vector capacity overhead for
//     rows_ and columns_.
//   * ~5*8 bytes and one heap allocation per unique row and unique column.
template <typename RowId, typename ColumnId>
class SparseMatrix {
 public:
  // Setting `value` to zero removes the value from the matrix.
  // Returns true if `value` is different from the existing value in the matrix.
  bool set(RowId row, ColumnId column, double value);

  // Zero is returned if the value is not present.
  double get(RowId row, ColumnId column) const;

  // Returns true if the value is present (nonzero).
  bool contains(RowId row, ColumnId column) const;

  // Zeros out all coefficients for this variable.
  void DeleteRow(RowId row);

  // Zeros out all coefficients for this variable.
  void DeleteColumn(ColumnId column);

  // Returns the variables that have nonzero entries with `variable`.
  // TODO(b/233630053): expose an iterator based API to avoid making a copy.
  std::vector<ColumnId> row(RowId row_id) const;

  // Returns the variables that have nonzero entries with `variable`.
  // TODO(b/233630053): expose an iterator based API to avoid making a copy.
  std::vector<RowId> column(ColumnId column_id) const;

  // Returns the variable value pairs (x, c) where `variable` and x have nonzero
  // coefficient c.
  //
  // The return order is deterministic but not defined.
  // TODO(b/233630053): expose an iterator based API to avoid making a copy.
  std::vector<std::pair<ColumnId, double>> RowTerms(RowId row_id) const;

  // Returns the variable value pairs (x, c) where `variable` and x have nonzero
  // coefficient c.
  //
  // The return order is deterministic but not defined.
  // TODO(b/233630053): expose an iterator based API to avoid making a copy.
  std::vector<std::pair<RowId, double>> ColumnTerms(ColumnId col_id) const;

  // Returns (x, y, c) tuples where variables x and y have nonzero coefficient
  // c, and x <= y.
  //
  // The return order is non-deterministic and not defined.
  // TODO(b/233630053): expose an iterator based API to avoid making a copy.
  std::vector<std::tuple<RowId, ColumnId, double>> Terms() const;

  // Removes all terms from the matrix.
  void Clear();

  // The number of (row, column) keys with nonzero value.
  int64_t nonzeros() const;

  // For testing/debugging only, do not depend on this value, behavior may
  // change based on implementation.
  int64_t impl_detail_matrix_storage_size() const { return values_.size(); }

  SparseDoubleMatrixProto Proto() const;

  SparseDoubleMatrixProto Update(
      const absl::flat_hash_set<RowId>& deleted_rows,
      absl::Span<const RowId> new_rows,
      const absl::flat_hash_set<ColumnId>& deleted_columns,
      absl::Span<const ColumnId> new_columns,
      const absl::flat_hash_set<std::pair<RowId, ColumnId>>& dirty) const;

 private:
  void CompactIfNeeded();

  // The values of the map can include zero.
  absl::flat_hash_map<std::pair<RowId, ColumnId>, double> values_;
  absl::flat_hash_map<RowId, std::vector<ColumnId>> rows_;
  absl::flat_hash_map<ColumnId, std::vector<RowId>> columns_;

  // The number of nonzero elements in values_.
  int64_t nonzeros_ = 0;
};

namespace internal {

// When the fraction of entries in values_ with value 0.0 is larger than
// kZerosCleanup, we compact the data structure and remove all zero entries.
constexpr double kZerosCleanup = 1.0 / 3.0;

// entries must have unique (row, column) values but can be in any order.
template <typename RowId, typename ColumnId>
SparseDoubleMatrixProto EntriesToMatrixProto(
    std::vector<std::tuple<RowId, ColumnId, double>> entries);

}  // namespace internal

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Inlined functions
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

namespace internal {

template <typename RowId, typename ColumnId>
SparseDoubleMatrixProto EntriesToMatrixProto(
    std::vector<std::tuple<RowId, ColumnId, double>> entries) {
  absl::c_sort(entries);
  const int num_entries = static_cast<int>(entries.size());
  SparseDoubleMatrixProto result;
  result.mutable_row_ids()->Reserve(num_entries);
  result.mutable_column_ids()->Reserve(num_entries);
  result.mutable_coefficients()->Reserve(num_entries);
  for (const auto [lin_con, var, coef] : entries) {
    result.add_row_ids(lin_con.value());
    result.add_column_ids(var.value());
    result.add_coefficients(coef);
  }
  return result;
}

}  // namespace internal

////////////////////////////////////////////////////////////////////////////////
// SparseSymmetricMatrix
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

////////////////////////////////////////////////////////////////////////////////
// SparseMatrix
////////////////////////////////////////////////////////////////////////////////

template <typename RowId, typename ColumnId>
bool SparseMatrix<RowId, ColumnId>::set(const RowId row, const ColumnId column,
                                        const double value) {
  const std::pair<RowId, ColumnId> key = {row, column};
  const auto it = values_.find(key);
  if (it == values_.end()) {
    if (value == 0.0) {
      return false;
    }
    rows_[row].push_back(column);
    columns_[column].push_back(row);
    values_[key] = value;
    ++nonzeros_;
    return true;
  } else {
    if (it->second == value) {
      return false;
    }
    const double old_value = it->second;
    it->second = value;
    if (value == 0.0) {
      --nonzeros_;
      CompactIfNeeded();
    } else if (old_value == 0.0) {
      ++nonzeros_;
    }
    return true;
  }
}

template <typename RowId, typename ColumnId>
double SparseMatrix<RowId, ColumnId>::get(const RowId row,
                                          const ColumnId column) const {
  const auto it = values_.find({row, column});
  if (it != values_.end()) {
    return it->second;
  }
  return 0.0;
}
template <typename RowId, typename ColumnId>
bool SparseMatrix<RowId, ColumnId>::contains(const RowId row,
                                             const ColumnId column) const {
  const auto it = values_.find({row, column});
  return it != values_.end() && it->second != 0.0;
}

template <typename RowId, typename ColumnId>
void SparseMatrix<RowId, ColumnId>::DeleteRow(const RowId row) {
  const auto row_it = rows_.find(row);
  if (row_it == rows_.end()) {
    return;
  }
  for (const ColumnId col : row_it->second) {
    auto val_it = values_.find({row_it->first, col});
    if (val_it != values_.end() && val_it->second != 0.0) {
      val_it->second = 0.0;
      nonzeros_--;
    }
  }
  CompactIfNeeded();
}

template <typename RowId, typename ColumnId>
void SparseMatrix<RowId, ColumnId>::DeleteColumn(const ColumnId column) {
  const auto col_it = columns_.find(column);
  if (col_it == columns_.end()) {
    return;
  }
  for (const RowId row : col_it->second) {
    auto val_it = values_.find({row, col_it->first});
    if (val_it != values_.end() && val_it->second != 0.0) {
      val_it->second = 0.0;
      nonzeros_--;
    }
  }
  CompactIfNeeded();
}

template <typename RowId, typename ColumnId>
std::vector<ColumnId> SparseMatrix<RowId, ColumnId>::row(
    const RowId row_id) const {
  std::vector<ColumnId> result;
  const auto it = rows_.find(row_id);
  if (it != rows_.end()) {
    for (const ColumnId col : it->second) {
      if (contains(row_id, col)) {
        result.push_back(col);
      }
    }
  }
  return result;
}

template <typename RowId, typename ColumnId>
std::vector<RowId> SparseMatrix<RowId, ColumnId>::column(
    const ColumnId column_id) const {
  std::vector<RowId> result;
  const auto it = columns_.find(column_id);
  if (it != columns_.end()) {
    for (const RowId row_id : it->second) {
      if (contains(row_id, column_id)) {
        result.push_back(row_id);
      }
    }
  }
  return result;
}

template <typename RowId, typename ColumnId>
std::vector<std::pair<ColumnId, double>>
SparseMatrix<RowId, ColumnId>::RowTerms(const RowId row_id) const {
  std::vector<std::pair<ColumnId, double>> result;
  const auto it = rows_.find(row_id);
  if (it != rows_.end()) {
    for (const ColumnId col : it->second) {
      const double val = get(row_id, col);
      if (val != 0.0) {
        result.push_back({col, val});
      }
    }
  }
  return result;
}

template <typename RowId, typename ColumnId>
std::vector<std::pair<RowId, double>>
SparseMatrix<RowId, ColumnId>::ColumnTerms(const ColumnId col_id) const {
  std::vector<std::pair<RowId, double>> result;
  const auto it = columns_.find(col_id);
  if (it != columns_.end()) {
    for (const RowId row_id : it->second) {
      const double val = get(row_id, col_id);
      if (val != 0.0) {
        result.push_back({row_id, val});
      }
    }
  }
  return result;
}

template <typename RowId, typename ColumnId>
std::vector<std::tuple<RowId, ColumnId, double>>
SparseMatrix<RowId, ColumnId>::Terms() const {
  std::vector<std::tuple<RowId, ColumnId, double>> result;
  result.reserve(nonzeros_);
  for (const auto& [k, v] : values_) {
    if (v != 0.0) {
      result.push_back({k.first, k.second, v});
    }
  }
  return result;
}

template <typename RowId, typename ColumnId>
void SparseMatrix<RowId, ColumnId>::Clear() {
  rows_.clear();
  columns_.clear();
  values_.clear();
}

template <typename RowId, typename ColumnId>
int64_t SparseMatrix<RowId, ColumnId>::nonzeros() const {
  return nonzeros_;
}

template <typename RowId, typename ColumnId>
SparseDoubleMatrixProto SparseMatrix<RowId, ColumnId>::Proto() const {
  SparseDoubleMatrixProto result;
  std::vector<std::tuple<RowId, ColumnId, double>> terms = Terms();
  absl::c_sort(terms);
  for (const auto [r, c, v] : terms) {
    result.add_row_ids(r.value());
    result.add_column_ids(c.value());
    result.add_coefficients(v);
  }
  return result;
}

template <typename RowId, typename ColumnId>
SparseDoubleMatrixProto SparseMatrix<RowId, ColumnId>::Update(
    const absl::flat_hash_set<RowId>& deleted_rows,
    const absl::Span<const RowId> new_rows,
    const absl::flat_hash_set<ColumnId>& deleted_columns,
    const absl::Span<const ColumnId> new_columns,
    const absl::flat_hash_set<std::pair<RowId, ColumnId>>& dirty) const {
  // Extract changes to the matrix of linear constraint coefficients
  std::vector<std::tuple<LinearConstraintId, VariableId, double>>
      matrix_updates;
  for (const auto [row, column] : dirty) {
    // Note: it is important that we check for deleted constraints and variables
    // here. While we generally try to remove elements from matrix_keys_ when
    // either their variable or constraint is deleted, if a coefficient is set
    // to zero and then the variable/constraint is deleted, we will miss it.
    if (deleted_rows.contains(row) || deleted_columns.contains(column)) {
      continue;
    }
    matrix_updates.push_back({row, column, get(row, column)});
  }

  for (const ColumnId new_col : new_columns) {
    // TODO(b/233630053): use iterator API.
    for (const auto [row, coef] : ColumnTerms(new_col)) {
      matrix_updates.push_back({row, new_col, coef});
    }
  }
  for (const RowId new_row : new_rows) {
    // TODO(b/233630053) use iterator API.
    for (const auto [col, coef] : RowTerms(new_row)) {
      // NOTE(user): we already have the columns above the checkpoint from
      // the loop above, but we will do at most twice as much as needed here.
      if (new_columns.empty() || col < new_columns[0]) {
        matrix_updates.push_back({new_row, col, coef});
      }
    }
  }
  return internal::EntriesToMatrixProto(std::move(matrix_updates));
}

template <typename RowId, typename ColumnId>
void SparseMatrix<RowId, ColumnId>::CompactIfNeeded() {
  const int64_t zeros = values_.size() - nonzeros_;
  if (values_.empty() ||
      static_cast<double>(zeros) / values_.size() <= internal::kZerosCleanup) {
    return;
  }
  // Traverse the rows and remove elements where value_ is zero. Delete the row
  // if it has no entries.
  for (auto row_it = rows_.begin(); row_it != rows_.end();) {
    const RowId row = row_it->first;
    std::vector<ColumnId>& row_entries = row_it->second;
    int64_t write = 0;
    for (int64_t read = 0; read < row_entries.size(); ++read) {
      const ColumnId col = row_entries[read];
      const auto val_it = values_.find({row, col});
      if (val_it != values_.end() && val_it->second != 0.0) {
        row_entries[write] = col;
        ++write;
      }
    }
    if (write == 0) {
      rows_.erase(row_it++);
    } else {
      row_entries.resize(write);
      ++row_it;
    }
  }

  // Like above, but now over the columns. Additionally, delete elements from
  // values_ that are zero in this second pass.
  for (auto col_it = columns_.begin(); col_it != columns_.end();) {
    const ColumnId col = col_it->first;
    std::vector<RowId>& col_entries = col_it->second;
    int64_t write = 0;
    for (int64_t read = 0; read < col_entries.size(); ++read) {
      const RowId row = col_entries[read];
      const auto val_it = values_.find({row, col});
      if (val_it != values_.end()) {
        if (val_it->second != 0.0) {
          col_entries[write] = row;
          ++write;
        } else {
          values_.erase(val_it);
        }
      }
    }
    if (write == 0) {
      columns_.erase(col_it++);
    } else {
      col_entries.resize(write);
      ++col_it;
    }
  }
}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_STORAGE_SPARSE_MATRIX_H_
