// Copyright 2010-2017 Google
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

// LU decomposition algorithm of a sparse matrix B with Markowitz pivot
// selection strategy. The algorithm constructs a lower matrix L, upper matrix
// U, row permutation P and a column permutation Q such that L.U = P.B.Q^{-1}.
//
// The current algorithm is a mix of ideas that can be found in the literature
// and of some optimizations tailored for its use in a revised simplex algorithm
// (like a fast processing of the singleton columns present in B). It constructs
// L and U column by column from left to right.
//
// A key concept is the one of the residual matrix which is the bottom right
// square submatrix that still needs to be factorized during the classical
// Gaussian elimination. The algorithm maintains the non-zero pattern of its
// rows and its row/column degrees.
//
// At each step, a number of columns equal to 'markowitz_zlatev_parameter' are
// chosen as candidates from the residual matrix. They are the ones with minimal
// residual column degree. They can be found easily because the columns of the
// residual matrix are kept in a priority queue.
//
// We compute the numerical value of these residual columns like in a
// left-looking algorithm by solving a sparse lower-triangular system with the
// current L constructed so far. Note that this step is highly optimized for
// sparsity and we reuse the computations done in the previous steps (if the
// candidate column was already considered before). As a by-product, we also
// get the corresponding column of U.
//
// Among the entries of these columns, a pivot is chosen such that the product:
//     (num_column_entries - 1) * (num_row_entries - 1)
// is minimized. Only the pivots with a magnitude greater than
// 'lu_factorization_pivot_threshold' times the maximum magnitude of the
// corresponding residual column are considered for stability reasons.
//
// Once the pivot is chosen, the residual column divided by the pivot becomes a
// column of L, and the non-zero pattern of the new residual submatrix is
// updated by subtracting the outer product of this pivot column times the pivot
// row. The product minimized above is thus an upper bound of the number of
// fill-in created during a step.
//
// References:
//
// J. R. Gilbert and T. Peierls, "Sparse partial pivoting in time proportional
// to arithmetic operations," SIAM J. Sci. Statist. Comput., 9 (1988): 862-874.
//
// I.S. Duff, A.M. Erisman and J.K. Reid, "Direct Methods for Sparse Matrices",
// Clarendon, Oxford, UK, 1987, ISBN 0-19-853421-3,
// http://www.amazon.com/dp/0198534213
//
// T.A. Davis, "Direct methods for Sparse Linear Systems", SIAM, Philadelphia,
// 2006, ISBN-13: 978-0-898716-13, http://www.amazon.com/dp/0898716136
//
// TODO(user): Determine whether any of these would bring any benefit:
// - S.C. Eisenstat and J.W.H. Liu, "The theory of elimination trees for
//   sparse unsymmetric matrices," SIAM J. Matrix Anal. Appl., 26:686-705,
//   January 2005
// - S.C. Eisenstat and J.W.H. Liu. "Algorithmic aspects of elimination trees
//   for sparse unsymmetric matrices," SIAM J. Matrix Anal. Appl.,
//   29:1363-1381, January 2008.
// - http://perso.ens-lyon.fr/~bucar/papers/kauc.pdf

#ifndef OR_TOOLS_GLOP_MARKOWITZ_H_
#define OR_TOOLS_GLOP_MARKOWITZ_H_

#include <queue>

#include "ortools/base/logging.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/status.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/sparse.h"
#include "ortools/util/stats.h"

namespace operations_research {
namespace glop {

// Holds the non-zero positions (by row) and column/row degree of the residual
// matrix during the Gaussian elimination.
//
// During each step of Gaussian elimination, a row and a column will be
// "removed" from the residual matrix. Note however that the row and column
// indices of the non-removed part do not change, so the residual matrix at a
// given step will only correspond to a subset of the initial indices.
class MatrixNonZeroPattern {
 public:
  MatrixNonZeroPattern() {}

  // Releases the memory used by this class.
  void Clear();

  // Resets the pattern to the one of an empty square matrix of the given size.
  void Reset(RowIndex num_rows, ColIndex num_cols);

  // Resets the pattern to the one of the given matrix but only for the
  // rows/columns whose given permutation is kInvalidRow or kInvalidCol.
  void InitializeFromMatrixSubset(const MatrixView& basis_matrix,
                                  const RowPermutation& row_perm,
                                  const ColumnPermutation& col_perm);

  // Adds a non-zero entry to the matrix. There should be no duplicates.
  void AddEntry(RowIndex row, ColIndex col);

  // Marks the given pivot row and column as deleted.
  // This is called at each step of the Gaussian elimination on the pivot.
  void DeleteRowAndColumn(RowIndex pivot_row, ColIndex pivot_col);

  // Decreases the degree of a row/column. This is the basic operation used to
  // keep the correct degree after a call to DeleteRowAndColumn(). This is
  // because row_non_zero_[row] is only lazily cleaned.
  int32 DecreaseRowDegree(RowIndex row);
  int32 DecreaseColDegree(ColIndex col);

  // Returns true if the column has been deleted by DeleteRowAndColumn().
  bool IsColumnDeleted(ColIndex col) const;

  // Removes from the corresponding row_non_zero_[row] the columns that have
  // been previously deleted by DeleteRowAndColumn().
  void RemoveDeletedColumnsFromRow(RowIndex row);

  // Returns the first non-deleted column index from this row or kInvalidCol if
  // none can be found.
  ColIndex GetFirstNonDeletedColumnFromRow(RowIndex row) const;

  // Performs a generic Gaussian update of the residual matrix:
  // - DeleteRowAndColumn() must already have been called.
  // - The non-zero pattern is augmented (set union) by the one of the
  //   outer product of the pivot column and row.
  //
  // Important: as a small optimization, this function does not call
  // DecreaseRowDegree() on the row in the pivot column. This has to be done by
  // the client.
  void Update(RowIndex pivot_row, ColIndex pivot_col,
              const SparseColumn& column);

  // Returns the degree (i.e. the number of non-zeros) of the given column.
  // This is only valid for the column indices still in the residual matrix.
  int32 ColDegree(ColIndex col) const {
    DCHECK(!deleted_columns_[col]);
    return col_degree_[col];
  }

  // Returns the degree (i.e. the number of non-zeros) of the given row.
  // This is only valid for the row indices still in the residual matrix.
  int32 RowDegree(RowIndex row) const { return row_degree_[row]; }

  // Returns the set of non-zeros of the given row (unsorted).
  // Call RemoveDeletedColumnsFromRow(row) to clean the row first.
  // This is only valid for the row indices still in the residual matrix.
  const std::vector<ColIndex>& RowNonZero(RowIndex row) const {
    return row_non_zero_[row];
  }

 private:
  // Augments the non-zero pattern of the given row by taking its union with the
  // non-zero pattern of the given pivot_row.
  void MergeInto(RowIndex pivot_row, RowIndex row);

  // Different version of MergeInto() that works only if the non-zeros position
  // of each row are sorted in increasing order. The output will also be sorted.
  //
  // TODO(user): This is currently not used but about the same speed as the
  // non-sorted version. Investigate more.
  void MergeIntoSorted(RowIndex pivot_row, RowIndex row);

  ITIVector<RowIndex, std::vector<ColIndex>> row_non_zero_;
  StrictITIVector<RowIndex, int32> row_degree_;
  StrictITIVector<ColIndex, int32> col_degree_;
  DenseBooleanRow deleted_columns_;
  DenseBooleanRow bool_scratchpad_;
  std::vector<ColIndex> col_scratchpad_;
  ColIndex num_non_deleted_columns_;

  DISALLOW_COPY_AND_ASSIGN(MatrixNonZeroPattern);
};

// Adjustable priority queue of columns. Pop() returns a column with the
// smallest degree first (degree = number of entries in the column).
// Empty columns (i.e. with degree 0) are not stored in the queue.
class ColumnPriorityQueue {
 public:
  ColumnPriorityQueue() {}

  // Releases the memory used by this class.
  void Clear();

  // Clears the queue and prepares it to store up to num_cols column indices
  // with a degree from 1 to max_degree included.
  void Reset(int32 max_degree, ColIndex num_cols);

  // Changes the degree of a column and make sure it is in the queue. The degree
  // must be non-negative (>= 0) and at most equal to the value of num_cols used
  // in Reset(). A degree of zero will remove the column from the queue.
  void PushOrAdjust(ColIndex col, int32 degree);

  // Removes the column index with higher priority from the queue and returns
  // it. Returns kInvalidCol if the queue is empty.
  ColIndex Pop();

 private:
  StrictITIVector<ColIndex, int32> col_index_;
  StrictITIVector<ColIndex, int32> col_degree_;
  std::vector<std::vector<ColIndex>> col_by_degree_;
  int32 min_degree_;
  DISALLOW_COPY_AND_ASSIGN(ColumnPriorityQueue);
};

// Contains a set of columns indexed by ColIndex. This is like a SparseMatrix
// but this class is optimized for the case where only a small subset of columns
// is needed at the same time (like it is the case in our LU algorithm). It
// reuses the memory of the columns that are no longer needed.
class SparseMatrixWithReusableColumnMemory {
 public:
  SparseMatrixWithReusableColumnMemory() {}

  // Resets the repository to num_cols empty columns.
  void Reset(ColIndex num_cols);

  // Returns the column with given index.
  const SparseColumn& column(ColIndex col) const;

  // Gets the mutable column with given column index. The returned vector
  // address is only valid until the next call to mutable_column().
  SparseColumn* mutable_column(ColIndex col);

  // Clears the column with given index and releases its memory to the common
  // memory pool that is used to create new mutable_column() on demand.
  void ClearAndReleaseColumn(ColIndex col);

  // Reverts this class to its initial state. This releases the memory of the
  // columns that were used but not the memory of this class member (this should
  // be fine).
  void Clear();

 private:
  // mutable_column(col) is stored in columns_[mapping_[col]].
  // The columns_ that can be reused have their index stored in free_columns_.
  const SparseColumn empty_column_;
  ITIVector<ColIndex, int> mapping_;
  std::vector<int> free_columns_;
  std::vector<SparseColumn> columns_;
  DISALLOW_COPY_AND_ASSIGN(SparseMatrixWithReusableColumnMemory);
};

// The class that computes either the actual L.U decomposition, or the
// permutation P and Q such that P.B.Q^{-1} will have a sparse L.U
// decomposition.
class Markowitz {
 public:
  Markowitz() {}

  // Computes the full factorization with P, Q, L and U.
  //
  // If the matrix is singular, the returned status will indicate it and the
  // permutation (col_perm) will contain a maximum non-singular set of columns
  // of the matrix. Moreover, by adding singleton columns with a one at the rows
  // such that 'row_perm[row] == kInvalidRow', then the matrix will be
  // non-singular.
  Status ComputeLU(const MatrixView& basis_matrix, RowPermutation* row_perm,
                   ColumnPermutation* col_perm, TriangularMatrix* lower,
                   TriangularMatrix* upper) MUST_USE_RESULT;

  // Only computes P and Q^{-1}, L and U can be computed later from these
  // permutations using another algorithm (for instance left-looking L.U). This
  // may be faster than computing the full L and U at the same time but the
  // current implementation is not optimized for this.
  //
  // It behaves the same as ComputeLU() for singular matrices.
  //
  // This function also works with a non-square matrix. It will return a set of
  // independent columns of maximum size. If all the given columns are
  // independent, the returned Status will be OK.
  Status ComputeRowAndColumnPermutation(
      const MatrixView& basis_matrix, RowPermutation* row_perm,
      ColumnPermutation* col_perm) MUST_USE_RESULT;

  // Releases the memory used by this class.
  void Clear();

  // Returns a std::string containing the statistics for this class.
  std::string StatString() const { return stats_.StatString(); }

  // Sets the current parameters.
  void SetParameters(const GlopParameters& parameters) {
    parameters_ = parameters;
  }

 private:
  // Statistics about this class.
  struct Stats : public StatsGroup {
    Stats()
        : StatsGroup("Markowitz"),
          basis_singleton_column_ratio("basis_singleton_column_ratio", this),
          basis_residual_singleton_column_ratio(
              "basis_residual_singleton_column_ratio", this),
          pivots_without_fill_in_ratio("pivots_without_fill_in_ratio", this),
          degree_two_pivot_columns("degree_two_pivot_columns", this) {}
    RatioDistribution basis_singleton_column_ratio;
    RatioDistribution basis_residual_singleton_column_ratio;
    RatioDistribution pivots_without_fill_in_ratio;
    RatioDistribution degree_two_pivot_columns;
  };
  Stats stats_;

  // Initializes residual_matrix_non_zero_, singleton_column_ and
  // singleton_row_.
  void InitializeResidualMatrix(const MatrixView& basis_matrix,
                                const RowPermutation& row_perm,
                                const ColumnPermutation& col_perm);

  // Fast track for singleton columns of the matrix. Fills a part of the row and
  // column permutation that move these columns in order to form an identity
  // sub-matrix on the upper left.
  //
  // Note(user): Linear programming bases usually have a resonable percentage of
  // slack columns in them, so this gives a big speedup.
  void ExtractSingletonColumns(const MatrixView& basis_matrix,
                               RowPermutation* row_perm,
                               ColumnPermutation* col_perm, int* index);

  // Fast track for columns that form a triangular matrix. This does not find
  // all of them, but because the column are ordered in the same way they were
  // ordered at the end of the previous factorization, this is likely to find
  // quite a few.
  //
  // The main gain here is that it avoids taking these columns into account in
  // InitializeResidualMatrix() and later in RemoveRowFromResidualMatrix().
  void ExtractResidualSingletonColumns(const MatrixView& basis_matrix,
                                       RowPermutation* row_perm,
                                       ColumnPermutation* col_perm, int* index);

  // Returns the column of the current residual matrix with an index 'col' in
  // the initial matrix. We compute it by solving a linear system with the
  // current lower_ and the last computed column 'col' of a previous residual
  // matrix. This uses the same algorithm as a left-looking factorization (see
  // lu_factorization.h for more details).
  const SparseColumn& ComputeColumn(const RowPermutation& row_perm,
                                    ColIndex col);

  // Finds an entry in the residual matrix with a low Markowitz score and a high
  // enough magnitude. Returns its Markowitz score and updates the given
  // pointers.
  //
  // We use the strategy of Zlatev, "On some pivotal strategies in Gaussian
  // elimination by sparse technique" (1980). SIAM J. Numer. Anal. 17 18-30. It
  // consists of looking for the best pivot in only a few columns (usually 3
  // or 4) amongst the ones which have the lowest number of entries.
  //
  // Amongst the pivots with a minimum Markowitz number, we choose the one
  // with highest magnitude. This doesn't apply to pivots with a 0 Markowitz
  // number because all such pivots will have to be taken at some point anyway.
  int64 FindPivot(const RowPermutation& row_perm,
                  const ColumnPermutation& col_perm, RowIndex* pivot_row,
                  ColIndex* pivot_col, Fractional* pivot_coefficient);

  // Updates the degree of a given column in the internal structure of the
  // class.
  void UpdateDegree(ColIndex col, int degree);

  // Removes all the coefficients in the residual matrix that are on the given
  // row or column. In both cases, the pivot row or column is ignored.
  void RemoveRowFromResidualMatrix(RowIndex pivot_row, ColIndex pivot_col);
  void RemoveColumnFromResidualMatrix(RowIndex pivot_row, ColIndex pivot_col);

  // Updates the residual matrix given the pivot position. This is needed if the
  // pivot row and pivot column both have more than one entry. Otherwise, the
  // residual matrix can be updated more efficiently by calling one of the
  // Remove...() functions above.
  void UpdateResidualMatrix(RowIndex pivot_row, ColIndex pivot_col);

  // Pointer to the matrix to factorize.
  MatrixView const* basis_matrix_;

  // These matrices are transformed during the algorithm into the final L and U
  // matrices modulo some row and column permutations. Note that the columns of
  // these matrices stay in the initial order.
  SparseMatrixWithReusableColumnMemory permuted_lower_;
  SparseMatrixWithReusableColumnMemory permuted_upper_;

  // These matrices will hold the final L and U. The are created columns by
  // columns from left to right, and at the end, their rows are permuted by
  // ComputeLU() to become triangular.
  TriangularMatrix lower_;
  TriangularMatrix upper_;

  // The columns of permuted_lower_ for which we do need a call to
  // PermutedLowerSparseSolve(). This speeds up ComputeColumn().
  DenseBooleanRow permuted_lower_column_needs_solve_;

  // Contains the non-zero positions of the current residual matrix (the
  // lower-right square matrix that gets smaller by one row and column at each
  // Gaussian elimination step).
  MatrixNonZeroPattern residual_matrix_non_zero_;

  // Data structure to access the columns by increasing degree.
  ColumnPriorityQueue col_by_degree_;

  // True as long as only singleton columns of the residual matrix are used.
  bool contains_only_singleton_columns_;

  // Boolean used to know when col_by_degree_ become useful.
  bool is_col_by_degree_initialized_;

  // FindPivot() needs to look at the first entries of col_by_degree_, it
  // temporary put them here before pushing them back to col_by_degree_.
  std::vector<ColIndex> examined_col_;

  // Singleton column indices are kept here rather than in col_by_degree_ to
  // optimize the algorithm: as long as this or singleton_row_ are not empty,
  // col_by_degree_ do not need to be initialized nor updated.
  std::vector<ColIndex> singleton_column_;

  // List of singleton row indices.
  std::vector<RowIndex> singleton_row_;

  // Proto holding all the parameters of this algorithm.
  GlopParameters parameters_;

  DISALLOW_COPY_AND_ASSIGN(Markowitz);
};

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_GLOP_MARKOWITZ_H_
