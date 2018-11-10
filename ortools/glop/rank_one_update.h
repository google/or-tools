// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_GLOP_RANK_ONE_UPDATE_H_
#define OR_TOOLS_GLOP_RANK_ONE_UPDATE_H_

#include "ortools/base/logging.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/lp_utils.h"
#include "ortools/lp_data/sparse.h"

namespace operations_research {
namespace glop {

// This class holds a matrix of the form T = I + u.Tr(v) where I is the
// identity matrix and u and v are two column vectors of the same size as I. It
// allows for efficient left and right solves with T. When T is non-singular,
// it is easy to show that T^{-1} = I - 1 / mu * u.Tr(v) where
// mu = 1.0 + Tr(v).u
//
// Note that when v is a unit vector, T is a regular Eta matrix and when u
// is a unit vector, T is a row-wise Eta matrix.
//
// This is based on section 3.1 of:
// Qi Huangfu, J. A. Julian Hall, "Novel update techniques for the revised
// simplex method", 28 january 2013, Technical Report ERGO-13-0001
class RankOneUpdateElementaryMatrix {
 public:
  // Rather than copying the vectors u and v, RankOneUpdateElementaryMatrix
  // takes two columns of a provided CompactSparseMatrix which is used for
  // storage. This has a couple of advantages, especially in the context of the
  // RankOneUpdateFactorization below:
  // - It uses less overall memory (and avoid allocation overhead).
  // - It has a better cache behavior for the RankOneUpdateFactorization solves.
  RankOneUpdateElementaryMatrix(const CompactSparseMatrix* storage,
                                ColIndex u_index, ColIndex v_index,
                                Fractional u_dot_v)
      : storage_(storage),
        u_index_(u_index),
        v_index_(v_index),
        mu_(1.0 + u_dot_v) {}

  // Returns whether or not this matrix is singular.
  // Note that the RightSolve() and LeftSolve() function will fail if this is
  // the case.
  bool IsSingular() const { return mu_ == 0.0; }

  // Solves T.x = rhs with rhs initialy in x (a column vector).
  // The non-zeros version keeps track of the new non-zeros.
  void RightSolve(DenseColumn* x) const {
    DCHECK(!IsSingular());
    const Fractional multiplier =
        -storage_->ColumnScalarProduct(v_index_, Transpose(*x)) / mu_;
    storage_->ColumnAddMultipleToDenseColumn(u_index_, multiplier, x);
  }
  void RightSolveWithNonZeros(ScatteredColumn* x) const {
    DCHECK(!IsSingular());
    const Fractional multiplier =
        -storage_->ColumnScalarProduct(v_index_, Transpose(x->values)) / mu_;
    if (multiplier != 0.0) {
      storage_->ColumnAddMultipleToSparseScatteredColumn(u_index_, multiplier,
                                                         x);
    }
  }

  // Solves y.T = rhs with rhs initialy in y (a row vector).
  // The non-zeros version keeps track of the new non-zeros.
  void LeftSolve(DenseRow* y) const {
    DCHECK(!IsSingular());
    const Fractional multiplier =
        -storage_->ColumnScalarProduct(u_index_, *y) / mu_;
    storage_->ColumnAddMultipleToDenseColumn(v_index_, multiplier,
                                             reinterpret_cast<DenseColumn*>(y));
  }
  void LeftSolveWithNonZeros(ScatteredRow* y) const {
    DCHECK(!IsSingular());
    const Fractional multiplier =
        -storage_->ColumnScalarProduct(u_index_, y->values) / mu_;
    if (multiplier != 0.0) {
      storage_->ColumnAddMultipleToSparseScatteredColumn(
          v_index_, multiplier, reinterpret_cast<ScatteredColumn*>(y));
    }
  }

  // Computes T.x for a given column vector.
  void RightMultiply(DenseColumn* x) const {
    const Fractional multiplier =
        storage_->ColumnScalarProduct(v_index_, Transpose(*x));
    storage_->ColumnAddMultipleToDenseColumn(u_index_, multiplier, x);
  }

  // Computes y.T for a given row vector.
  void LeftMultiply(DenseRow* y) const {
    const Fractional multiplier = storage_->ColumnScalarProduct(u_index_, *y);
    storage_->ColumnAddMultipleToDenseColumn(v_index_, multiplier,
                                             reinterpret_cast<DenseColumn*>(y));
  }

  EntryIndex num_entries() const {
    return storage_->column(u_index_).num_entries() +
           storage_->column(v_index_).num_entries();
  }

 private:
  // This is only used in debug mode.
  Fractional ComputeUScalarV() const {
    DenseColumn dense_u;
    storage_->ColumnCopyToDenseColumn(u_index_, &dense_u);
    return storage_->ColumnScalarProduct(v_index_, Transpose(dense_u));
  }

  // Note that we allow copy and assignment so we can store a
  // RankOneUpdateElementaryMatrix in an STL container.
  const CompactSparseMatrix* storage_;
  ColIndex u_index_;
  ColIndex v_index_;
  Fractional mu_;
};

// A rank one update factorization corresponds to the product of k rank one
// update elementary matrices, i.e. T = T_0.T_1. ... .T_{k-1}
class RankOneUpdateFactorization {
 public:
  // TODO(user): make the 5% a parameter and share it between all the places
  // that switch between a sparse/dense version.
  RankOneUpdateFactorization() : hypersparse_ratio_(0.05) {}

  // This is currently only visible for testing.
  void set_hypersparse_ratio(double value) { hypersparse_ratio_ = value; }

  // Deletes all elementary matrices of this factorization.
  void Clear() {
    elementary_matrices_.clear();
    num_entries_ = 0;
  }

  // Updates the factorization.
  void Update(const RankOneUpdateElementaryMatrix& update_matrix) {
    elementary_matrices_.push_back(update_matrix);
    num_entries_ += update_matrix.num_entries();
  }

  // Left-solves all systems from right to left, i.e. y_i = y_{i+1}.(T_i)^{-1}
  void LeftSolve(DenseRow* y) const {
    RETURN_IF_NULL(y);
    for (int i = elementary_matrices_.size() - 1; i >= 0; --i) {
      elementary_matrices_[i].LeftSolve(y);
    }
  }

  // Same as LeftSolve(), but if the given non_zeros are not empty, then all
  // the new non-zeros in the result are happended to it.
  void LeftSolveWithNonZeros(ScatteredRow* y) const {
    RETURN_IF_NULL(y);
    if (y->non_zeros.empty()) {
      LeftSolve(&y->values);
      return;
    }

    // tmp_row_is_non_zero_ is always all false before and after this code.
    y->is_non_zero.resize(y->values.size(), false);
    DCHECK(IsAllFalse(y->is_non_zero));
    for (const ColIndex col : y->non_zeros) y->is_non_zero[col] = true;
    const int hypersparse_threshold = static_cast<int>(
        hypersparse_ratio_ * static_cast<double>(y->values.size().value()));
    for (int i = elementary_matrices_.size() - 1; i >= 0; --i) {
      if (y->non_zeros.size() < hypersparse_threshold) {
        elementary_matrices_[i].LeftSolveWithNonZeros(y);
      } else {
        elementary_matrices_[i].LeftSolve(&y->values);
      }
    }
    for (const ColIndex col : y->non_zeros) y->is_non_zero[col] = false;
    if (y->non_zeros.size() >= hypersparse_threshold) y->non_zeros.clear();
  }

  // Right-solves all systems from left to right, i.e. T_i.d_{i+1} = d_i
  void RightSolve(DenseColumn* d) const {
    RETURN_IF_NULL(d);
    const size_t end = elementary_matrices_.size();
    for (int i = 0; i < end; ++i) {
      elementary_matrices_[i].RightSolve(d);
    }
  }

  // Same as RightSolve(), but if the given non_zeros are not empty, then all
  // the new non-zeros in the result are happended to it.
  void RightSolveWithNonZeros(ScatteredColumn* d) const {
    RETURN_IF_NULL(d);
    if (d->non_zeros.empty()) {
      RightSolve(&d->values);
      return;
    }

    // d->is_non_zero is always all false before and after this code.
    d->is_non_zero.resize(d->values.size(), false);
    DCHECK(IsAllFalse(d->is_non_zero));
    for (const RowIndex row : d->non_zeros) d->is_non_zero[row] = true;
    const size_t end = elementary_matrices_.size();
    const int hypersparse_threshold = static_cast<int>(
        hypersparse_ratio_ * static_cast<double>(d->values.size().value()));
    for (int i = 0; i < end; ++i) {
      if (d->non_zeros.size() < hypersparse_threshold) {
        elementary_matrices_[i].RightSolveWithNonZeros(d);
      } else {
        elementary_matrices_[i].RightSolve(&d->values);
      }
    }
    for (const RowIndex row : d->non_zeros) d->is_non_zero[row] = false;
    if (d->non_zeros.size() >= hypersparse_threshold) d->non_zeros.clear();
  }

  EntryIndex num_entries() const { return num_entries_; }

 private:
  double hypersparse_ratio_;
  EntryIndex num_entries_;
  std::vector<RankOneUpdateElementaryMatrix> elementary_matrices_;
  DISALLOW_COPY_AND_ASSIGN(RankOneUpdateFactorization);
};

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_GLOP_RANK_ONE_UPDATE_H_
