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

//
// The following are very good references for terminology, data structures,
// and algorithms:
//
// I.S. Duff, A.M. Erisman and J.K. Reid, "Direct Methods for Sparse Matrices",
// Clarendon, Oxford, UK, 1987, ISBN 0-19-853421-3,
// http://www.amazon.com/dp/0198534213.
//
//
// T.A. Davis, "Direct methods for Sparse Linear Systems", SIAM, Philadelphia,
// 2006, ISBN-13: 978-0-898716-13, http://www.amazon.com/dp/0898716136.
//
//
// Both books also contain a wealth of references.

#ifndef OR_TOOLS_LP_DATA_SPARSE_H_
#define OR_TOOLS_LP_DATA_SPARSE_H_

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/permutation.h"
#include "ortools/lp_data/scattered_vector.h"
#include "ortools/lp_data/sparse_column.h"
#include "ortools/util/return_macros.h"

namespace operations_research {
namespace glop {

class CompactSparseMatrixView;

// --------------------------------------------------------
// SparseMatrix
// --------------------------------------------------------
// SparseMatrix is a class for sparse matrices suitable for computation.
// Data is represented using the so-called compressed-column storage scheme.
// Entries (row, col, value) are stored by column using a SparseColumn.
//
// Citing [Duff et al, 1987], a matrix is sparse if many of its coefficients are
// zero and if there is an advantage in exploiting its zeros.
// For practical reasons, not all zeros are exploited (for example those that
// result from calculations.) The term entry refers to those coefficients that
// are handled explicitly. All non-zeros are entries while some zero
// coefficients may also be entries.
//
// Note that no special ordering of entries is assumed.
class SparseMatrix {
 public:
  SparseMatrix();

  // Useful for testing. This makes it possible to write:
  // SparseMatrix matrix {
  //    {1, 2, 3},
  //    {4, 5, 6},
  //    {7, 8, 9}};
#if (!defined(_MSC_VER) || _MSC_VER >= 1800)
  SparseMatrix(
      std::initializer_list<std::initializer_list<Fractional>> init_list);
#endif
  // Clears internal data structure, i.e. erases all the columns and set
  // the number of rows to zero.
  void Clear();

  // Returns true if the matrix is empty.
  // That is if num_rows() OR num_cols() are zero.
  bool IsEmpty() const;

  // Cleans the columns, i.e. removes zero-values entries, removes duplicates
  // entries and sorts remaining entries in increasing row order.
  // Call with care: Runs in O(num_cols * column_cleanup), with each column
  // cleanup running in O(num_entries * log(num_entries)).
  void CleanUp();

  // Call CheckNoDuplicates() on all columns, useful for doing a DCHECK.
  bool CheckNoDuplicates() const;

  // Call IsCleanedUp() on all columns, useful for doing a DCHECK.
  bool IsCleanedUp() const;

  // Change the number of row of this matrix.
  void SetNumRows(RowIndex num_rows);

  // Appends an empty column and returns its index.
  ColIndex AppendEmptyColumn();

  // Appends a unit vector defined by the single entry (row, value).
  // Note that the row should be smaller than the number of rows of the matrix.
  void AppendUnitVector(RowIndex row, Fractional value);

  // Swaps the content of this SparseMatrix with the one passed as argument.
  // Works in O(1).
  void Swap(SparseMatrix* matrix);

  // Populates the matrix with num_cols columns of zeros. As the number of rows
  // is specified by num_rows, the matrix is not necessarily square.
  // Previous columns/values are deleted.
  void PopulateFromZero(RowIndex num_rows, ColIndex num_cols);

  // Populates the matrix from the Identity matrix of size num_cols.
  // Previous columns/values are deleted.
  void PopulateFromIdentity(ColIndex num_cols);

  // Populates the matrix from the transposed of the given matrix.
  // Note that this preserve the property of lower/upper triangular matrix
  // to have the diagonal coefficients first/last in each columns. It actually
  // sorts the entries in each columns by their indices.
  template <typename Matrix>
  void PopulateFromTranspose(const Matrix& input);

  // Populates a SparseMatrix from another one (copy), note that this run in
  // O(number of entries in the matrix).
  void PopulateFromSparseMatrix(const SparseMatrix& matrix);

  // Populates a SparseMatrix from the image of a matrix A through the given
  // row_perm and inverse_col_perm. See permutation.h for more details.
  template <typename Matrix>
  void PopulateFromPermutedMatrix(const Matrix& a,
                                  const RowPermutation& row_perm,
                                  const ColumnPermutation& inverse_col_perm);

  // Populates a SparseMatrix from the result of alpha * A + beta * B,
  // where alpha and beta are Fractionals, A and B are sparse matrices.
  void PopulateFromLinearCombination(Fractional alpha, const SparseMatrix& a,
                                     Fractional beta, const SparseMatrix& b);

  // Multiplies SparseMatrix a by SparseMatrix b.
  void PopulateFromProduct(const SparseMatrix& a, const SparseMatrix& b);

  // Removes the marked columns from the matrix and adjust its size.
  // This runs in O(num_cols).
  void DeleteColumns(const DenseBooleanRow& columns_to_delete);

  // Applies the given row permutation and deletes the rows for which
  // permutation[row] is kInvalidRow. Sets the new number of rows to num_rows.
  // This runs in O(num_entries).
  void DeleteRows(RowIndex num_rows, const RowPermutation& permutation);

  // Appends all rows from the given matrix to the calling object after the last
  // row of the calling object. Both matrices must have the same number of
  // columns. The method returns true if the rows were added successfully and
  // false if it can't add the rows because the number of columns of the
  // matrices are different.
  bool AppendRowsFromSparseMatrix(const SparseMatrix& matrix);

  // Applies the row permutation.
  void ApplyRowPermutation(const RowPermutation& row_perm);

  // Returns the coefficient at position row in column col.
  // Call with care: runs in O(num_entries_in_col) as entries may not be sorted.
  Fractional LookUpValue(RowIndex row, ColIndex col) const;

  // Returns true if the matrix equals a (with a maximum error smaller than
  // given the tolerance).
  bool Equals(const SparseMatrix& a, Fractional tolerance) const;

  // Returns, in min_magnitude and max_magnitude, the minimum and maximum
  // magnitudes of the non-zero coefficients of the calling object.
  void ComputeMinAndMaxMagnitudes(Fractional* min_magnitude,
                                  Fractional* max_magnitude) const;

  // Return the matrix dimension.
  RowIndex num_rows() const { return num_rows_; }
  ColIndex num_cols() const { return ColIndex(columns_.size()); }

  // Access the underlying sparse columns.
  const SparseColumn& column(ColIndex col) const { return columns_[col]; }
  SparseColumn* mutable_column(ColIndex col) { return &(columns_[col]); }

  // Returns the total numbers of entries in the matrix.
  // Runs in O(num_cols).
  EntryIndex num_entries() const;

  // Computes the 1-norm of the matrix.
  // The 1-norm |A| is defined as max_j sum_i |a_ij| or
  // max_col sum_row |a(row,col)|.
  Fractional ComputeOneNorm() const;

  // Computes the oo-norm (infinity-norm) of the matrix.
  // The oo-norm |A| is defined as max_i sum_j |a_ij| or
  // max_row sum_col |a(row,col)|.
  Fractional ComputeInfinityNorm() const;

  // Returns a dense representation of the matrix.
  std::string Dump() const;

 private:
  // Resets the internal data structure and create an empty rectangular
  // matrix of size num_rows x num_cols.
  void Reset(ColIndex num_cols, RowIndex num_rows);

  // Vector of sparse columns.
  StrictITIVector<ColIndex, SparseColumn> columns_;

  // Number of rows. This is needed as sparse columns don't have a maximum
  // number of rows.
  RowIndex num_rows_;

  DISALLOW_COPY_AND_ASSIGN(SparseMatrix);
};

// A matrix constructed from a list of already existing SparseColumn. This class
// does not take ownership of the underlying columns, and thus they must outlive
// this class (and keep the same address in memory).
class MatrixView {
 public:
  MatrixView() {}
  explicit MatrixView(const SparseMatrix& matrix) {
    PopulateFromMatrix(matrix);
  }

  // Takes all the columns of the given matrix.
  void PopulateFromMatrix(const SparseMatrix& matrix) {
    const ColIndex num_cols = matrix.num_cols();
    columns_.resize(num_cols, nullptr);
    for (ColIndex col(0); col < num_cols; ++col) {
      columns_[col] = &matrix.column(col);
    }
    num_rows_ = matrix.num_rows();
  }

  // Takes all the columns of the first matrix followed by the columns of the
  // second matrix.
  void PopulateFromMatrixPair(const SparseMatrix& matrix_a,
                              const SparseMatrix& matrix_b) {
    const ColIndex num_cols = matrix_a.num_cols() + matrix_b.num_cols();
    columns_.resize(num_cols, nullptr);
    for (ColIndex col(0); col < matrix_a.num_cols(); ++col) {
      columns_[col] = &matrix_a.column(col);
    }
    for (ColIndex col(0); col < matrix_b.num_cols(); ++col) {
      columns_[matrix_a.num_cols() + col] = &matrix_b.column(col);
    }
    num_rows_ = std::max(matrix_a.num_rows(), matrix_b.num_rows());
  }

  // Takes only the columns of the given matrix that belongs to the given basis.
  void PopulateFromBasis(const MatrixView& matrix,
                         const RowToColMapping& basis) {
    columns_.resize(RowToColIndex(basis.size()), nullptr);
    for (RowIndex row(0); row < basis.size(); ++row) {
      columns_[RowToColIndex(row)] = &matrix.column(basis[row]);
    }
    num_rows_ = matrix.num_rows();
  }

  // Same behavior as the SparseMatrix functions above.
  bool IsEmpty() const { return columns_.empty(); }
  RowIndex num_rows() const { return num_rows_; }
  ColIndex num_cols() const { return columns_.size(); }
  const SparseColumn& column(ColIndex col) const { return *columns_[col]; }
  EntryIndex num_entries() const;
  Fractional ComputeOneNorm() const;
  Fractional ComputeInfinityNorm() const;

 private:
  RowIndex num_rows_;
  StrictITIVector<ColIndex, SparseColumn const*> columns_;
};

extern template void SparseMatrix::PopulateFromTranspose<SparseMatrix>(
    const SparseMatrix& input);
extern template void SparseMatrix::PopulateFromPermutedMatrix<SparseMatrix>(
    const SparseMatrix& a, const RowPermutation& row_perm,
    const ColumnPermutation& inverse_col_perm);
extern template void
SparseMatrix::PopulateFromPermutedMatrix<CompactSparseMatrixView>(
    const CompactSparseMatrixView& a, const RowPermutation& row_perm,
    const ColumnPermutation& inverse_col_perm);

// Another matrix representation which is more efficient than a SparseMatrix but
// doesn't allow matrix modification. It is faster to construct, uses less
// memory and provides a better cache locality when iterating over the non-zeros
// of the matrix columns.
class CompactSparseMatrix {
 public:
  // When iteration performance matter, getting a ConstView allows the compiler
  // to do better aliasing analysis and not re-read vectors address all the
  // time.
  class ConstView {
   public:
    explicit ConstView(const CompactSparseMatrix* matrix)
        : coefficients_(matrix->coefficients_.data()),
          rows_(matrix->rows_.data()),
          starts_(matrix->starts_.data()) {}

    // Functions to iterate on the entries of a given column:
    // const auto view = compact_matrix.view();
    // for (const EntryIndex i : view.Column(col)) {
    //   const RowIndex row = view.EntryRow(i);
    //   const Fractional coefficient = view.EntryCoefficient(i);
    // }
    ::util::IntegerRange<EntryIndex> Column(ColIndex col) const {
      return ::util::IntegerRange<EntryIndex>(starts_[col.value()],
                                              starts_[col.value() + 1]);
    }
    Fractional EntryCoefficient(EntryIndex i) const {
      return coefficients_[i.value()];
    }
    RowIndex EntryRow(EntryIndex i) const { return rows_[i.value()]; }

    EntryIndex ColumnNumEntries(ColIndex col) const {
      return starts_[col.value() + 1] - starts_[col.value()];
    }

    // Returns the scalar product of the given row vector with the column of
    // index col of this matrix.
    Fractional ColumnScalarProduct(ColIndex col,
                                   DenseRow::ConstView vector) const;

   private:
    const Fractional* const coefficients_;
    const RowIndex* const rows_;
    const EntryIndex* const starts_;
  };

  CompactSparseMatrix() {}
  ConstView view() const { return ConstView(this); }

  // Convenient constructors for tests.
  // TODO(user): If this is needed in production code, it can be done faster.
  explicit CompactSparseMatrix(const SparseMatrix& matrix) {
    PopulateFromMatrixView(MatrixView(matrix));
  }

  // Creates a CompactSparseMatrix from the given MatrixView. The matrices are
  // the same, only the representation differ. Note that the entry order in
  // each column is preserved.
  void PopulateFromMatrixView(const MatrixView& input);

  // Creates a CompactSparseMatrix by copying the input and adding an identity
  // matrix to the left of it.
  void PopulateFromSparseMatrixAndAddSlacks(const SparseMatrix& input);

  // Creates a CompactSparseMatrix from the transpose of the given
  // CompactSparseMatrix. Note that the entries in each columns will be ordered
  // by row indices.
  void PopulateFromTranspose(const CompactSparseMatrix& input);

  // Clears the matrix and sets its number of rows. If none of the Populate()
  // function has been called, Reset() must be called before calling any of the
  // Add*() functions below.
  void Reset(RowIndex num_rows);

  // Adds a dense column to the CompactSparseMatrix (only the non-zero will be
  // actually stored). This work in O(input.size()) and returns the index of the
  // added column.
  ColIndex AddDenseColumn(const DenseColumn& dense_column);

  // Same as AddDenseColumn(), but only adds the non-zero from the given start.
  ColIndex AddDenseColumnPrefix(const DenseColumn& dense_column,
                                RowIndex start);

  // Same as AddDenseColumn(), but uses the given non_zeros pattern of input.
  // If non_zeros is empty, this actually calls AddDenseColumn().
  ColIndex AddDenseColumnWithNonZeros(const DenseColumn& dense_column,
                                      const std::vector<RowIndex>& non_zeros);

  // Adds a dense column for which we know the non-zero positions and clears it.
  // Note that this function supports duplicate indices in non_zeros. The
  // complexity is in O(non_zeros.size()). Only the indices present in non_zeros
  // will be cleared. Returns the index of the added column.
  ColIndex AddAndClearColumnWithNonZeros(DenseColumn* column,
                                         std::vector<RowIndex>* non_zeros);

  // Returns the number of entries (i.e. degree) of the given column.
  EntryIndex ColumnNumEntries(ColIndex col) const {
    return starts_[col + 1] - starts_[col];
  }

  // Returns the matrix dimensions. See same functions in SparseMatrix.
  EntryIndex num_entries() const {
    DCHECK_EQ(coefficients_.size(), rows_.size());
    return coefficients_.size();
  }
  RowIndex num_rows() const { return num_rows_; }
  ColIndex num_cols() const { return num_cols_; }

  // Returns whether or not this matrix contains any non-zero entries.
  bool IsEmpty() const {
    DCHECK_EQ(coefficients_.size(), rows_.size());
    return coefficients_.empty();
  }

  // Alternative iteration API compatible with the one from SparseMatrix.
  // The ConstView alternative should be faster.
  ColumnView column(ColIndex col) const {
    DCHECK_LT(col, num_cols_);

    // Note that the start may be equal to row.size() if the last columns
    // are empty, it is why we don't use &row[start].
    const EntryIndex start = starts_[col];
    return ColumnView(starts_[col + 1] - start, rows_.data() + start.value(),
                      coefficients_.data() + start.value());
  }

  // Returns true if the given column is empty. Note that for triangular matrix
  // this does not include the diagonal coefficient (see below).
  bool ColumnIsEmpty(ColIndex col) const {
    return starts_[col + 1] == starts_[col];
  }

  // Returns the scalar product of the given row vector with the column of index
  // col of this matrix.
  Fractional ColumnScalarProduct(ColIndex col, const DenseRow& vector) const {
    return view().ColumnScalarProduct(col, vector.const_view());
  }

  // Adds a multiple of the given column of this matrix to the given
  // dense_column. If multiplier is 0.0, this function does nothing. This
  // function is declared in the .h for efficiency.
  void ColumnAddMultipleToDenseColumn(ColIndex col, Fractional multiplier,
                                      DenseColumn* dense_column) const {
    RETURN_IF_NULL(dense_column);
    if (multiplier == 0.0) return;
    const auto entry_rows = rows_.view();
    const auto entry_coeffs = coefficients_.view();
    for (const EntryIndex i : Column(col)) {
      (*dense_column)[entry_rows[i]] += multiplier * entry_coeffs[i];
    }
  }

  // Same as ColumnAddMultipleToDenseColumn() but also adds the new non-zeros to
  // the non_zeros vector. A non-zero is "new" if is_non_zero[row] was false,
  // and we update dense_column[row]. This function also updates is_non_zero.
  void ColumnAddMultipleToSparseScatteredColumn(ColIndex col,
                                                Fractional multiplier,
                                                ScatteredColumn* column) const {
    RETURN_IF_NULL(column);
    if (multiplier == 0.0) return;
    const auto entry_rows = rows_.view();
    const auto entry_coeffs = coefficients_.view();
    for (const EntryIndex i : Column(col)) {
      column->Add(entry_rows[i], multiplier * entry_coeffs[i]);
    }
  }

  // Copies the given column of this matrix into the given dense_column.
  // This function is declared in the .h for efficiency.
  void ColumnCopyToDenseColumn(ColIndex col, DenseColumn* dense_column) const {
    RETURN_IF_NULL(dense_column);
    dense_column->AssignToZero(num_rows_);
    ColumnCopyToClearedDenseColumn(col, dense_column);
  }

  // Same as ColumnCopyToDenseColumn() but assumes the column to be initially
  // all zero.
  void ColumnCopyToClearedDenseColumn(ColIndex col,
                                      DenseColumn* dense_column) const {
    RETURN_IF_NULL(dense_column);
    dense_column->resize(num_rows_, 0.0);
    const auto entry_rows = rows_.view();
    const auto entry_coeffs = coefficients_.view();
    for (const EntryIndex i : Column(col)) {
      (*dense_column)[entry_rows[i]] = entry_coeffs[i];
    }
  }

  // Same as ColumnCopyToClearedDenseColumn() but also fills non_zeros.
  void ColumnCopyToClearedDenseColumnWithNonZeros(
      ColIndex col, DenseColumn* dense_column,
      RowIndexVector* non_zeros) const {
    RETURN_IF_NULL(dense_column);
    dense_column->resize(num_rows_, 0.0);
    non_zeros->clear();
    const auto entry_rows = rows_.view();
    const auto entry_coeffs = coefficients_.view();
    for (const EntryIndex i : Column(col)) {
      const RowIndex row = entry_rows[i];
      (*dense_column)[row] = entry_coeffs[i];
      non_zeros->push_back(row);
    }
  }

  void Swap(CompactSparseMatrix* other);

 protected:
  // Functions to iterate on the entries of a given column.
  ::util::IntegerRange<EntryIndex> Column(ColIndex col) const {
    return ::util::IntegerRange<EntryIndex>(starts_[col], starts_[col + 1]);
  }

  // The matrix dimensions, properly updated by full and incremental builders.
  RowIndex num_rows_;
  ColIndex num_cols_;

  // Holds the columns non-zero coefficients and row positions.
  // The entries for the column of index col are stored in the entries
  // [starts_[col], starts_[col + 1]).
  StrictITIVector<EntryIndex, Fractional> coefficients_;
  StrictITIVector<EntryIndex, RowIndex> rows_;
  StrictITIVector<ColIndex, EntryIndex> starts_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CompactSparseMatrix);
};

inline Fractional CompactSparseMatrix::ConstView::ColumnScalarProduct(
    ColIndex col, DenseRow::ConstView vector) const {
  // We expand ourselves since we don't really care about the floating
  // point order of operation and this seems faster.
  int i = starts_[col.value()].value();
  const int end = starts_[col.value() + 1].value();
  const int shifted_end = end - 3;
  Fractional result1 = 0.0;
  Fractional result2 = 0.0;
  Fractional result3 = 0.0;
  Fractional result4 = 0.0;
  for (; i < shifted_end; i += 4) {
    result1 += coefficients_[i] * vector[RowToColIndex(rows_[i])];
    result2 += coefficients_[i + 1] * vector[RowToColIndex(rows_[i + 1])];
    result3 += coefficients_[i + 2] * vector[RowToColIndex(rows_[i + 2])];
    result4 += coefficients_[i + 3] * vector[RowToColIndex(rows_[i + 3])];
  }
  Fractional result = result1 + result2 + result3 + result4;
  if (i < end) {
    result += coefficients_[i] * vector[RowToColIndex(rows_[i])];
    if (i + 1 < end) {
      result += coefficients_[i + 1] * vector[RowToColIndex(rows_[i + 1])];
      if (i + 2 < end) {
        result += coefficients_[i + 2] * vector[RowToColIndex(rows_[i + 2])];
      }
    }
  }
  return result;
}

// A matrix view of the basis columns of a CompactSparseMatrix, with basis
// specified as a RowToColMapping.  This class does not take ownership of the
// underlying matrix or basis, and thus they must outlive this class (and keep
// the same address in memory).
class CompactSparseMatrixView {
 public:
  CompactSparseMatrixView(const CompactSparseMatrix* compact_matrix,
                          const RowToColMapping* basis)
      : compact_matrix_(*compact_matrix),
        columns_(basis->data(), basis->size().value()) {}
  CompactSparseMatrixView(const CompactSparseMatrix* compact_matrix,
                          const std::vector<ColIndex>* columns)
      : compact_matrix_(*compact_matrix), columns_(*columns) {}

  // Same behavior as the SparseMatrix functions above.
  bool IsEmpty() const { return compact_matrix_.IsEmpty(); }
  RowIndex num_rows() const { return compact_matrix_.num_rows(); }
  ColIndex num_cols() const { return ColIndex(columns_.size()); }
  const ColumnView column(ColIndex col) const {
    return compact_matrix_.column(columns_[col.value()]);
  }
  EntryIndex num_entries() const;
  Fractional ComputeOneNorm() const;
  Fractional ComputeInfinityNorm() const;

 private:
  // We require that the underlying CompactSparseMatrix and RowToColMapping
  // continue to own the (potentially large) data accessed via this view.
  const CompactSparseMatrix& compact_matrix_;
  const absl::Span<const ColIndex> columns_;
};

// Specialization of a CompactSparseMatrix used for triangular matrices.
// To be able to solve triangular systems as efficiently as possible, the
// diagonal entries are stored in a separate vector and not in the underlying
// CompactSparseMatrix.
//
// Advanced usage: this class also support matrices that can be permuted into a
// triangular matrix and some functions work directly on such matrices.
class TriangularMatrix : private CompactSparseMatrix {
 public:
  TriangularMatrix() : all_diagonal_coefficients_are_one_(true) {}

  // Only a subset of the functions from CompactSparseMatrix are exposed (note
  // the private inheritance). They are extended to deal with diagonal
  // coefficients properly.
  void PopulateFromTranspose(const TriangularMatrix& input);
  void Swap(TriangularMatrix* other);
  bool IsEmpty() const { return diagonal_coefficients_.empty(); }
  RowIndex num_rows() const { return num_rows_; }
  ColIndex num_cols() const { return num_cols_; }
  EntryIndex num_entries() const {
    return EntryIndex(num_cols_.value()) + coefficients_.size();
  }

  // On top of the CompactSparseMatrix functionality, TriangularMatrix::Reset()
  // also pre-allocates space of size col_size for a number of internal vectors.
  // This helps reduce costly push_back operations for large problems.
  //
  // WARNING: Reset() must be called with a sufficiently large col_capacity
  // prior to any Add* calls (e.g., AddTriangularColumn).
  void Reset(RowIndex num_rows, ColIndex col_capacity);

  // Constructs a triangular matrix from the given SparseMatrix. The input is
  // assumed to be lower or upper triangular without any permutations. This is
  // checked in debug mode.
  void PopulateFromTriangularSparseMatrix(const SparseMatrix& input);

  // Functions to create a triangular matrix incrementally, column by column.
  // A client needs to call Reset(num_rows) first, and then each column must be
  // added by calling one of the 3 functions below.
  //
  // Note that the row indices of the columns are allowed to be permuted: the
  // diagonal entry of the column #col not being necessarily on the row #col.
  // This is why these functions require the 'diagonal_row' parameter. The
  // permutation can be fixed at the end by a call to
  // ApplyRowPermutationToNonDiagonalEntries() or accounted directly in the case
  // of PermutedLowerSparseSolve().
  void AddTriangularColumn(const ColumnView& column, RowIndex diagonal_row);
  void AddTriangularColumnWithGivenDiagonalEntry(const SparseColumn& column,
                                                 RowIndex diagonal_row,
                                                 Fractional diagonal_value);
  void AddDiagonalOnlyColumn(Fractional diagonal_value);

  // Adds the given sparse column divided by diagonal_coefficient.
  // The diagonal_row is assumed to be present and its value should be the
  // same as the one given in diagonal_coefficient. Note that this function
  // tests for zero coefficients in the input column and removes them.
  void AddAndNormalizeTriangularColumn(const SparseColumn& column,
                                       RowIndex diagonal_row,
                                       Fractional diagonal_coefficient);

  // Applies the given row permutation to all entries except the diagonal ones.
  void ApplyRowPermutationToNonDiagonalEntries(const RowPermutation& row_perm);

  // Copy a triangular column with its diagonal entry to the given SparseColumn.
  void CopyColumnToSparseColumn(ColIndex col, SparseColumn* output) const;

  // Copy a triangular matrix to the given SparseMatrix.
  void CopyToSparseMatrix(SparseMatrix* output) const;

  // Returns the index of the first column which is not an identity column (i.e.
  // a column j with only one entry of value 1 at the j-th row). This is always
  // zero if the matrix is not triangular.
  ColIndex GetFirstNonIdentityColumn() const {
    return first_non_identity_column_;
  }

  // Returns the diagonal coefficient of the given column.
  Fractional GetDiagonalCoefficient(ColIndex col) const {
    return diagonal_coefficients_[col];
  }

  // Returns true iff the column contains no non-diagonal entries.
  bool ColumnIsDiagonalOnly(ColIndex col) const {
    return CompactSparseMatrix::ColumnIsEmpty(col);
  }

  // --------------------------------------------------------------------------
  // Triangular solve functions.
  //
  // All the functions containing the word Lower (resp. Upper) require the
  // matrix to be lower (resp. upper_) triangular without any permutation.
  // --------------------------------------------------------------------------

  // Solve the system L.x = rhs for a lower triangular matrix.
  // The result overwrite rhs.
  void LowerSolve(DenseColumn* rhs) const;

  // Solves the system U.x = rhs for an upper triangular matrix.
  void UpperSolve(DenseColumn* rhs) const;

  // Solves the system Transpose(U).x = rhs where U is upper triangular.
  // This can be used to do a left-solve for a row vector (i.e. y.Y = rhs).
  void TransposeUpperSolve(DenseColumn* rhs) const;

  // This assumes that the rhs is all zero before the given position.
  void LowerSolveStartingAt(ColIndex start, DenseColumn* rhs) const;

  // Solves the system Transpose(L).x = rhs, where L is lower triangular.
  // This can be used to do a left-solve for a row vector (i.e., y.Y = rhs).
  void TransposeLowerSolve(DenseColumn* rhs) const;

  // Hyper-sparse version of the triangular solve functions. The passed
  // non_zero_rows should contain the positions of the symbolic non-zeros of the
  // result in the order in which they need to be accessed (or in the reverse
  // order for the Reverse*() versions).
  //
  // The non-zero vector is mutable so that the symbolic non-zeros that are
  // actually zero because of numerical cancellations can be removed.
  //
  // The non-zeros can be computed by one of these two methods:
  // - ComputeRowsToConsiderWithDfs() which will give them in the reverse order
  //   of the one they need to be accessed in. This is only a topological order,
  //   and it will not necessarily be "sorted".
  // - ComputeRowsToConsiderInSortedOrder() which will always give them in
  //   increasing order.
  //
  // Note that if the non-zeros are given in a sorted order, then the
  // hyper-sparse functions will return EXACTLY the same results as the non
  // hyper-sparse version above.
  //
  // For a given solve, here is the required order:
  // - For a lower solve, increasing non-zeros order.
  // - For an upper solve, decreasing non-zeros order.
  // - for a transpose lower solve, decreasing non-zeros order.
  // - for a transpose upper solve, increasing non_zeros order.
  //
  // For a general discussion of hyper-sparsity in LP, see:
  // J.A.J. Hall, K.I.M. McKinnon, "Exploiting hyper-sparsity in the revised
  // simplex method", December 1999, MS 99-014.
  // http://www.maths.ed.ac.uk/hall/MS-99/MS9914.pdf
  void HyperSparseSolve(DenseColumn* rhs, RowIndexVector* non_zero_rows) const;
  void HyperSparseSolveWithReversedNonZeros(
      DenseColumn* rhs, RowIndexVector* non_zero_rows) const;
  void TransposeHyperSparseSolve(DenseColumn* rhs,
                                 RowIndexVector* non_zero_rows) const;
  void TransposeHyperSparseSolveWithReversedNonZeros(
      DenseColumn* rhs, RowIndexVector* non_zero_rows) const;

  // Given the positions of the non-zeros of a vector, computes the non-zero
  // positions of the vector after a solve by this triangular matrix. The order
  // of the returned non-zero positions will be in the REVERSE elimination
  // order. If the function detects that there are too many non-zeros, then it
  // aborts early and non_zero_rows is cleared.
  void ComputeRowsToConsiderWithDfs(RowIndexVector* non_zero_rows) const;

  // Same as TriangularComputeRowsToConsider() but always returns the non-zeros
  // sorted by rows. It is up to the client to call the direct or reverse
  // hyper-sparse solve function depending if the matrix is upper or lower
  // triangular.
  void ComputeRowsToConsiderInSortedOrder(RowIndexVector* non_zero_rows,
                                          Fractional sparsity_ratio,
                                          Fractional num_ops_ratio) const;
  void ComputeRowsToConsiderInSortedOrder(RowIndexVector* non_zero_rows) const;
  // This is currently only used for testing. It achieves the same result as
  // PermutedLowerSparseSolve() below, but the latter exploits the sparsity of
  // rhs and is thus faster for our use case.
  //
  // Note that partial_inverse_row_perm only permutes the first k rows, where k
  // is the same as partial_inverse_row_perm.size(). It is the inverse
  // permutation of row_perm which only permutes k rows into is [0, k), the
  // other row images beeing kInvalidRow. The other arguments are the same as
  // for PermutedLowerSparseSolve() and described there.
  //
  // IMPORTANT: lower will contain all the "symbolic" non-zero entries.
  // A "symbolic" zero entry is one that will be zero whatever the coefficients
  // of the rhs entries. That is it only depends on the position of its
  // entries, not on their values. Thus, some of its coefficients may be zero.
  // This fact is exploited by the LU factorization code. The zero coefficients
  // of upper will be cleaned, however.
  void PermutedLowerSolve(const SparseColumn& rhs,
                          const RowPermutation& row_perm,
                          const RowMapping& partial_inverse_row_perm,
                          SparseColumn* lower, SparseColumn* upper) const;

  // This solves a lower triangular system with only ones on the diagonal where
  // the matrix and the input rhs are permuted by the inverse of row_perm. Note
  // that the output will also be permuted by the inverse of row_perm. The
  // function also supports partial permutation. That is if row_perm[i] < 0 then
  // column row_perm[i] is assumed to be an identity column.
  //
  // The output is given as follow:
  // - lower is cleared, and receives the rows for which row_perm[row] < 0
  //   meaning not yet examined as a pivot (see markowitz.cc).
  // - upper is NOT cleared, and the other rows (row_perm[row] >= 0) are
  //   appended to it.
  // - Note that lower and upper can point to the same SparseColumn.
  //
  // Note: This function is non-const because ComputeRowsToConsider() also
  // prunes the underlying dependency graph of the lower matrix while doing a
  // solve. See marked_ and pruned_ends_ below.
  void PermutedLowerSparseSolve(const ColumnView& rhs,
                                const RowPermutation& row_perm,
                                SparseColumn* lower, SparseColumn* upper);

  // This is used to compute the deterministic time of a matrix factorization.
  int64_t NumFpOperationsInLastPermutedLowerSparseSolve() const {
    return num_fp_operations_;
  }

  // To be used in DEBUG mode by the client code. This check that the matrix is
  // lower- (resp. upper-) triangular without any permutation and that there is
  // no zero on the diagonal. We can't do that on each Solve() that require so,
  // otherwise it will be too slow in debug.
  bool IsLowerTriangular() const;
  bool IsUpperTriangular() const;

  // Visible for testing. This is used by PermutedLowerSparseSolve() to compute
  // the non-zero indices of the result. The output is as follow:
  // - lower_column_rows will contains the rows for which row_perm[row] < 0.
  // - upper_column_rows will contains the other rows in the reverse topological
  //   order in which they should be considered in PermutedLowerSparseSolve().
  //
  // This function is non-const because it prunes the underlying dependency
  // graph of the lower matrix while doing a solve. See marked_ and pruned_ends_
  // below.
  //
  // Pruning the graph at the same time is slower but not by too much (< 2x) and
  // seems worth doing. Note that when the lower matrix is dense, most of the
  // graph will likely be pruned. As a result, the symbolic phase will be
  // negligible compared to the numerical phase so we don't really need a dense
  // version of PermutedLowerSparseSolve().
  void PermutedComputeRowsToConsider(const ColumnView& rhs,
                                     const RowPermutation& row_perm,
                                     RowIndexVector* lower_column_rows,
                                     RowIndexVector* upper_column_rows);

  // The upper bound is computed using one of the algorithm presented in
  // "A Survey of Condition Number Estimation for Triangular Matrices"
  // https:epubs.siam.org/doi/pdf/10.1137/1029112/
  Fractional ComputeInverseInfinityNormUpperBound() const;
  Fractional ComputeInverseInfinityNorm() const;

 private:
  // Internal versions of some Solve() functions to avoid code duplication.
  template <bool diagonal_of_ones>
  void LowerSolveStartingAtInternal(ColIndex start,
                                    DenseColumn::View rhs) const;
  template <bool diagonal_of_ones>
  void UpperSolveInternal(DenseColumn::View rhs) const;
  template <bool diagonal_of_ones>
  void TransposeLowerSolveInternal(DenseColumn::View rhs) const;
  template <bool diagonal_of_ones>
  void TransposeUpperSolveInternal(DenseColumn::View rhs) const;
  template <bool diagonal_of_ones>
  void HyperSparseSolveInternal(DenseColumn::View rhs,
                                RowIndexVector* non_zero_rows) const;
  template <bool diagonal_of_ones>
  void HyperSparseSolveWithReversedNonZerosInternal(
      DenseColumn::View rhs, RowIndexVector* non_zero_rows) const;
  template <bool diagonal_of_ones>
  void TransposeHyperSparseSolveInternal(DenseColumn::View rhs,
                                         RowIndexVector* non_zero_rows) const;
  template <bool diagonal_of_ones>
  void TransposeHyperSparseSolveWithReversedNonZerosInternal(
      DenseColumn::View rhs, RowIndexVector* non_zero_rows) const;

  // Internal function used by the Add*() functions to finish adding
  // a new column to a triangular matrix.
  void CloseCurrentColumn(Fractional diagonal_value);

  // Extra data for "triangular" matrices. The diagonal coefficients are
  // stored in a separate vector instead of beeing stored in each column.
  StrictITIVector<ColIndex, Fractional> diagonal_coefficients_;

  // Index of the first column which is not a diagonal only column with a
  // coefficient of 1. This is used to optimize the solves.
  ColIndex first_non_identity_column_;

  // This common case allows for more efficient Solve() functions.
  // TODO(user): Do not even construct diagonal_coefficients_ in this case?
  bool all_diagonal_coefficients_are_one_;

  // For the hyper-sparse version. These are used to implement a DFS, see
  // TriangularComputeRowsToConsider() for more details.
  mutable DenseBooleanColumn stored_;
  mutable std::vector<RowIndex> nodes_to_explore_;

  // For PermutedLowerSparseSolve().
  int64_t num_fp_operations_;
  mutable std::vector<RowIndex> lower_column_rows_;
  mutable std::vector<RowIndex> upper_column_rows_;
  mutable DenseColumn initially_all_zero_scratchpad_;

  // This boolean vector is used to detect entries that can be pruned during
  // the DFS used for the symbolic phase of ComputeRowsToConsider().
  //
  // Problem: We have a DAG where each node has outgoing arcs towards other
  // nodes (this adjacency list is NOT sorted by any order). We want to compute
  // the reachability of a set of nodes S and its topological order. While doing
  // this, we also want to prune the adjacency lists to exploit the simple fact
  // that if a -> (b, c) and b -> (c) then c can be removed from the adjacency
  // list of a since it will be implied through b. Note that this doesn't change
  // the reachability of any set nor a valid topological ordering of such a set.
  //
  // The concept is known as the transitive reduction of a DAG, see
  // http://en.wikipedia.org/wiki/Transitive_reduction.
  //
  // Heuristic algorithm: While doing the DFS to compute Reach(S) and its
  // topological order, each time we process a node, we mark all its adjacent
  // node while going down in the DFS, and then we unmark all of them when we go
  // back up. During the un-marking, if a node is already un-marked, it means
  // that it was implied by some other path starting at the current node and we
  // can prune it and remove it from the adjacency list of the current node.
  //
  // Note(user): I couldn't find any reference for this algorithm, even though
  // I suspect I am not the first one to need something similar.
  mutable DenseBooleanColumn marked_;

  // This is used to represent a pruned sub-matrix of the current matrix that
  // corresponds to the pruned DAG as described in the comment above for
  // marked_. This vector is used to encode the sub-matrix as follow:
  // - Both the rows and the coefficients of the pruned matrix are still stored
  //   in rows_ and coefficients_.
  // - The data of column 'col' is still stored starting at starts_[col].
  // - But, its end is given by pruned_ends_[col] instead of starts_[col + 1].
  //
  // The idea of using a smaller graph for the symbolic phase is well known in
  // sparse linear algebra. See:
  // - John R. Gilbert and Joseph W. H. Liu, "Elimination structures for
  //   unsymmetric sparse LU factors", Tech. Report CS-90-11. Departement of
  //   Computer Science, York University, North York. Ontario, Canada, 1990.
  // - Stanley C. Eisenstat and Joseph W. H. Liu, "Exploiting structural
  //   symmetry in a sparse partial pivoting code". SIAM J. Sci. Comput. Vol
  //   14, No 1, pp. 253-257, January 1993.
  //
  // Note that we use an original algorithm and prune the graph while performing
  // the symbolic phase. Hence the pruning will only benefit the next symbolic
  // phase. This is different from Eisenstat-Liu's symmetric pruning. It is
  // still a heuristic and will not necessarily find the minimal graph that
  // has the same result for the symbolic phase though.
  //
  // TODO(user): Use this during the "normal" hyper-sparse solves so that
  // we can benefit from the pruned lower matrix there?
  StrictITIVector<ColIndex, EntryIndex> pruned_ends_;

  DISALLOW_COPY_AND_ASSIGN(TriangularMatrix);
};

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_LP_DATA_SPARSE_H_
