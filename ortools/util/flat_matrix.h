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

#ifndef OR_TOOLS_UTIL_FLAT_MATRIX_H_
#define OR_TOOLS_UTIL_FLAT_MATRIX_H_

// A very simple flattened 2D array of fixed size. It's movable, copyable.
// It can also be assigned.
// This was originally made to replace uses of vector<vector<...>> where each
// vector had a fixed size: vector<vector<>> has much worse performance in a
// highly concurrent setting, because it does a lot of memory allocations.

#include <vector>

#include "absl/types/span.h"

namespace operations_research {

// NOTE(user): T=bool is not yet supported (the [] operator doesn't work).
template <typename T>
class FlatMatrix {
 public:
  FlatMatrix() : num_rows_(0), num_cols_(0) {}
  FlatMatrix(size_t num_rows, size_t num_cols)
      : num_rows_(num_rows),
        num_cols_(num_cols),
        array_(num_rows_ * num_cols_) {}
  FlatMatrix(size_t num_rows, size_t num_cols, const T& elem)
      : num_rows_(num_rows),
        num_cols_(num_cols),
        array_(num_rows_ * num_cols_, elem) {}

  size_t num_rows() const { return num_rows_; }
  size_t num_cols() const { return num_cols_; }

  absl::Span<T> operator[](size_t row) {
    return absl::Span<T>(array_.data() + row * num_cols_, num_cols_);
  }
  absl::Span<const T> operator[](size_t row) const {
    return {array_.data() + row * num_cols_, num_cols_};
  }

  // All the elements of the FlatMatrix.
  absl::Span<T> all_elements() { return absl::MakeSpan(array_); }
  absl::Span<const T> all_elements() const {
    return absl::MakeConstSpan(array_);
  }

  // To iterate over the rows of the FlatMatrix. Example:
  //   FlatMatrix<double> matrix(23, 45);
  //   for (absl::Span<const double> row : matrix.rows()) {
  //     LOG(INFO) << DUMP_VARS(row);
  //   }
  class ConstRowsIterator {
   public:
    typedef T value_type;
    ConstRowsIterator(const T* ptr, size_t row, size_t row_size);
    absl::Span<const T> operator*() const { return {ptr_, row_size_}; }
    ConstRowsIterator& operator++();
    bool operator!=(const ConstRowsIterator& rhs) const;

   private:
    const T* ptr_;
    size_t row_;
    const size_t row_size_;
  };
  struct ConstRows {
    typedef absl::Span<const T> value_type;
    typedef ConstRowsIterator const_iterator;
    ConstRowsIterator begin() const;
    ConstRowsIterator end() const;
    FlatMatrix* matrix = nullptr;
  };
  ConstRows rows() { return {.matrix = this}; }

 private:
  // Those are non-const only to support the assignment operators.
  size_t num_rows_;
  size_t num_cols_;
  // NOTE(user): We could use a simpler unique_ptr<T[]> or even a self-managed
  // memory block, but we'd need to define the copy constructor.
  std::vector<T> array_;
};

// Implementation of the templates.
template <typename T>
FlatMatrix<T>::ConstRowsIterator::ConstRowsIterator(const T* ptr, size_t row,
                                                    size_t row_size)
    : ptr_(ptr), row_(row), row_size_(row_size) {}

template <typename T>
typename FlatMatrix<T>::ConstRowsIterator&
FlatMatrix<T>::ConstRowsIterator::operator++() {
  ptr_ += row_size_;
  ++row_;
  return *this;
}

template <typename T>
bool FlatMatrix<T>::ConstRowsIterator::operator!=(
    const ConstRowsIterator& rhs) const {
  return std::tie(ptr_, row_, row_size_) !=
         std::tie(rhs.ptr_, rhs.row_, rhs.row_size_);
}

template <typename T>
typename FlatMatrix<T>::ConstRowsIterator FlatMatrix<T>::ConstRows::begin()
    const {
  return ConstRowsIterator(matrix->array_.data(), /*row=*/0,
                           /*row_size=*/matrix->num_cols_);
}

template <typename T>
typename FlatMatrix<T>::ConstRowsIterator FlatMatrix<T>::ConstRows::end()
    const {
  return ConstRowsIterator(matrix->array_.data() + matrix->array_.size(),
                           /*row=*/matrix->num_rows_,
                           /*row_size=*/matrix->num_cols_);
}

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_FLAT_MATRIX_H_
