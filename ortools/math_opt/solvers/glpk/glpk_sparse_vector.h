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

#ifndef OR_TOOLS_MATH_OPT_SOLVERS_GLPK_GLPK_SPARSE_VECTOR_H_
#define OR_TOOLS_MATH_OPT_SOLVERS_GLPK_GLPK_SPARSE_VECTOR_H_

#include <functional>
#include <limits>
#include <optional>
#include <vector>

#include "ortools/base/logging.h"

namespace operations_research::math_opt {

// Sparse vector in GLPK format.
//
// GLPK represents a sparse vector of size n with two arrays of size n+1, one
// for indices and one for values. The first element of each of these arrays is
// ignored (GLPK uses one-based indices). On top of that, the array of indices
// contains one-based indices (typically rows or columns indices). The entries
// are not necessarily sorted.
//
// For example to store a sparse vector where we have:
//
//   idx | value
//   ----+------
//    1  |  2.5
//    2  |
//    3  | -1.0
//    4  |
//    5  |  0.5
//
// GLPK would use two arrays:
//
//   const int indices[] = { /*ignored*/-1, 3, 1, 5 };
//   const double values[] = { /*ignored*/NAN, -1.0, 2.5, 0.5 };
//
// This class also keeps an additional vector which size is the capacity of the
// sparse vector (i.e. the corresponding size of a dense vector). It associates
// to each index an optional position of the corresponding entry in the indices
// and values arrays. This is used to make Set() and Get() O(1) and this makes
// Clear() O(size()) since indices associated to entries need to be cleared.
//
// This additional vector along with the ones used for indices and values are
// all pre-allocated to fit the capacity. Hence an instance of this class
// allocates:
//
//   capacity * (2 * sizeof(int) + sizeof(double))
//
// It is thus recommended to reuse the same instance multiple times instead of
// reallocating one for it to be efficient.
class GlpkSparseVector {
 public:
  // Builds a sparse vector with the provided capacity (i.e. the size of the
  // vector it was dense).
  //
  // This operation has O(capacity) complexity (see the class documentation for
  // allocated memory).
  explicit GlpkSparseVector(const int capacity);

  // Returns the capacity (the size of the vector if it was dense).
  int capacity() const { return capacity_; }

  // Returns the number of entries in the sparse vector.
  int size() const { return size_; }

  // Returns the indices array of the GLPK sparse vector.
  //
  // Only values in [1, size()] are meaningful.
  const int* indices() const { return indices_.data(); }

  // Returns the values array of the GLPK sparse vector.
  //
  // Only values in [1, size()] are meaningful.
  const double* values() const { return values_.data(); }

  // Clears the sparse vector, removing all entries.
  //
  // This operation has O(size()) complexity.
  void Clear();

  // Returns the value at the given index if there is a corresponding entry or
  // nullopt.
  //
  // It CHECKs that the index is in [1, capacity]. The operation has O(1)
  // complexity.
  inline std::optional<double> Get(int index) const;

  // Changes the value of the given index, adding a new entry if necessary.
  //
  // Note that entries are only removed by Clear() or Load(). Setting a value to
  // 0.0 does not remove the corresponding entry.
  //
  // It CHECKs that the index is in [1, capacity]. The operation has O(1)
  // complexity.
  inline void Set(int index, double value);

  // Replaces the content of the sparse vector by calling a GLPK API.
  //
  // Since GLPK functions have other parameters, here we expect the caller to
  // provide a wrapping lambda expression that passes the indices and values
  // buffers to the GLPK function and returns the number of written elements.
  //
  // It CHECKs that the returned number of elements is not greater than the
  // capacity, that the indices are in [1, capacity] range and that there is no
  // duplicated indices.
  //
  // Example:
  //
  //   GlpkSparseVector row_values(num_cols);
  //   row_values.Set([&](int* const indices, double* const values) {
  //     return glp_get_mat_row(problem, row_index, indices, values);
  //   });
  //
  void Load(std::function<int(int* indices, double* values)> getter);

 private:
  // Guard value used in index_to_entry_ to identify indices not in the sparse
  // vector.
  static constexpr int kNotPresent = std::numeric_limits<int>::max();

  // Capacity.
  int capacity_;

  // Number of entries.
  int size_ = 0;

  // For each dense index in [1, capacity], keeps the index of the corresponding
  // entry in indices_ and values_. If the index i has a value in the sparse
  // vector then indices_[index_to_entry_[i]] == i and
  // values_[index_to_entry_[i]] is the corresponding value. If the index i does
  // not have a value then index_to_entry_[i] == kNotPresent.
  //
  // Note that as for indices_ and values_, index_to_entry_[0] is unused.
  std::vector<int> index_to_entry_;

  // The GLPK one-based vector of entries' indices. Only values in [1, size_]
  // are meaningful.
  std::vector<int> indices_;

  // The GLPK one-based vector of entries' values. Only values in [1, size_] are
  // meaningful.
  std::vector<double> values_;
};

////////////////////////////////////////////////////////////////////////////////
// Inline implementations
////////////////////////////////////////////////////////////////////////////////

std::optional<double> GlpkSparseVector::Get(const int index) const {
  CHECK_GE(index, 1);
  CHECK_LE(index, capacity_);

  const int entry = index_to_entry_[index];
  if (entry == kNotPresent) {
    return std::nullopt;
  }

  DCHECK_GE(entry, 1);
  DCHECK_LE(entry, capacity_);
  DCHECK_EQ(indices_[entry], index);

  return values_[entry];
}

void GlpkSparseVector::Set(const int index, const double value) {
  CHECK_GE(index, 1);
  CHECK_LE(index, capacity_);

  const int entry = index_to_entry_[index];
  if (entry == kNotPresent) {
    DCHECK_LT(size_, capacity_);
    ++size_;
    index_to_entry_[index] = size_;
    indices_[size_] = index;
    values_[size_] = value;

    return;
  }

  DCHECK_GE(entry, 1);
  DCHECK_LE(entry, capacity_);
  DCHECK_EQ(indices_[entry], index);

  values_[entry] = value;
}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_SOLVERS_GLPK_GLPK_SPARSE_VECTOR_H_
