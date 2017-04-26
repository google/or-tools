// Copyright 2010-2014 Google
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

#ifndef OR_TOOLS_GLOP_LU_FACTORIZATION_H_
#define OR_TOOLS_GLOP_LU_FACTORIZATION_H_

#include "ortools/glop/markowitz.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/status.h"
#include "ortools/lp_data/sparse.h"
#include "ortools/util/stats.h"

namespace operations_research {
namespace glop {

// An LU-Factorization class encapsulating the LU factorization data and
// algorithms. The actual algorithm is in markowitz.h and .cc. This class holds
// all the Solve() functions that deal with the permutations and the L and U
// factors once they are computed.
//
// TODO(user): Add a ScatteredColumn class containing a DenseColumn and
// an EntryRowIndexVector non-zero pattern.
class LuFactorization {
 public:
  LuFactorization();

  // Returns true if the LuFactorization is a factorization of the identity
  // matrix. In this state, all the Solve() functions will work for any
  // vector dimension.
  bool IsIdentityFactorization() { return is_identity_factorization_; }

  // Clears internal data structure and reset this class to the factorization
  // of an identity matrix.
  void Clear();

  // Computes an LU-decomposition for a given matrix B. If for some reason,
  // there was an error, then the factorization is reset to the one of the
  // identity matrix, and an error is reported.
  //
  // Note(user): Since a client must use the result, there is little chance of
  // it being confused by this revert to identity factorization behavior. The
  // reason behind it is that this way, calling any public function of this
  // class will never cause a crash of the program.
  Status ComputeFactorization(const MatrixView& matrix) MUST_USE_RESULT;

  // Returns the column permutation used by the LU factorization.
  const ColumnPermutation& GetColumnPermutation() const { return col_perm_; }

  // Sets the column permutation to the identity permutation. The idea is that
  // the column permutation can be incorporated in the basis RowToColMapping,
  // and once this is done, then a client can call this and effectively remove
  // the need for a column permutation on each solve.
  void SetColumnPermutationToIdentity() {
    col_perm_.clear();
    inverse_col_perm_.clear();
  }

  // Solves 'B.x = b', x initially contains b, and is replaced by 'B^{-1}.b'.
  // Since P.B.Q^{-1} = L.U, we have B = P^{-1}.L.U.Q.
  // 1/ Solve P^{-1}.y = b for y by computing y = P.b,
  // 2/ solve L.z = y for z,
  // 3/ solve U.t = z for t,
  // 4/ finally solve Q.x = t, by computing x = Q^{-1}.t.
  void RightSolve(DenseColumn* x) const;

  // Same as RightSolve(), but takes a SparseColumn b as an input. It also needs
  // the number of rows because if the matrix is the identity matrix, this is
  // not stored in this class or in the given sparse column.
  void SparseRightSolve(const SparseColumn& b, RowIndex num_rows,
                        DenseColumn* x) const;

  // Solves 'y.B = r', y initially contains r, and is replaced by r.B^{-1}.
  // Internally, it takes x = y^T, b = r^T and solves B^T.x = b.
  // We have P.B.Q^{-1} = P.B.Q^T = L.U, thus (L.U)^T = Q.B^T.P^T.
  // Therefore B^T = Q^{-1}.U^T.L^T.P^T.P^{-1} = Q^{-1}.U^T.L^T.P
  // The procedure is thus:
  // 1/ Solve Q^{-1}.y = b for y, by computing y = Q.b,
  // 2/ solve U^T.z = y for z,
  // 3/ solve L^T.t = z for t,
  // 4/ finally, solve P.x = t for x by computing x = P^{-1}.t.
  void LeftSolve(DenseRow* y) const;

  // Same as LeftSolve(), but exploits the given non_zeros of the input.
  // Also returns the non-zeros patern of the result in non_zeros.
  void SparseLeftSolve(DenseRow* y, ColIndexVector* non_zeros) const;

  // More fine-grained right/left solve functions.
  // Note that a solve involving L actually solves P^{-1}.L and a solve
  // involving U actually solves U.Q. To solve a system with the initial matrix
  // B, one needs to call:
  // - RightSolveL() and then RightSolveU() for a right solve (B.x = initial x).
  // - LeftSolveU() and then LeftSolveL() for a left solve (y.B = initial y).
  void RightSolveL(DenseColumn* x) const;
  void RightSolveU(DenseColumn* x) const;
  void LeftSolveU(DenseRow* y) const;
  void LeftSolveL(DenseRow* y) const;

  // Specialized version of RightSolveL() that takes a SparseColumn (or a
  // ScatteredColumnReference) as input. non_zeros will either be cleared or set
  // to the non zeros of the result. Important: the output x must be of the
  // correct size and all zero.
  void RightSolveLForSparseColumn(const SparseColumn& b, DenseColumn* x,
                                  std::vector<RowIndex>* non_zeros) const;
  void RightSolveLForScatteredColumn(const ScatteredColumnReference& b,
                                     DenseColumn* x,
                                     std::vector<RowIndex>* non_zeros) const;

  // Specialized version of RightSolveL() where x is originaly equal to
  // 'a' permuted by row_perm_. Note that 'a' is only used for DCHECK or when
  // is_identity_factorization_ is true, in which case the assumption of x is
  // relaxed since x is not used at all.
  void RightSolveLWithPermutedInput(const DenseColumn& a, DenseColumn* x) const;

  // Specialized version of LeftSolveU() for an unit right-hand side.
  // non_zeros will either be cleared or set to the non zeros of the results.
  // It also returns the value of col permuted by Q (which is the position
  // of the unit-vector rhs in the solve system: y.U = rhs).
  // Important: the output y must be of the correct size and all zero.
  ColIndex LeftSolveUForUnitRow(ColIndex col, DenseRow* y,
                                std::vector<ColIndex>* non_zeros) const;

  // Specialized version of RightSolveU() and LeftSolveU() that may exploit the
  // initial non-zeros if it is non-empty. In addition,
  // RightSolveUWithNonZeros() always return the non-zeros of the output.
  void RightSolveUWithNonZeros(DenseColumn* x,
                               std::vector<RowIndex>* non_zeros) const;
  void LeftSolveUWithNonZeros(DenseRow* y,
                              std::vector<ColIndex>* non_zeros) const;

  // Specialized version of LeftSolveL() that also computes the non-zero
  // pattern of the output. Moreover, if result_before_permutation is not NULL,
  // it is filled with the result just before row_perm_ is applied to it and
  // true is returned. If result_before_permutation is not filled, then false is
  // returned.
  bool LeftSolveLWithNonZeros(DenseRow* y, ColIndexVector* non_zeros,
                              DenseColumn* result_before_permutation) const;

  // Returns the given column of U.
  // It will only be valid until the next call to GetColumnOfU().
  const SparseColumn& GetColumnOfU(ColIndex col) const;

  // Returns the norm of B^{-1}.a
  Fractional RightSolveSquaredNorm(const SparseColumn& a) const;

  // Returns the norm of (B^T)^{-1}.e_row where e is an unit vector.
  Fractional DualEdgeSquaredNorm(RowIndex row) const;

  // The fill-in of the LU-factorization is defined as the sum of the number
  // of entries of both the lower- and upper-triangular matrices L and U minus
  // the number of entries in the initial matrix B.
  //
  // This returns the number of entries in lower + upper as the percentage of
  // the number of entries in B.
  double GetFillInPercentage(const MatrixView& matrix) const;

  // Returns the number of entries in L + U.
  // If the factorization is the identity, this returns 0.
  EntryIndex NumberOfEntries() const;

  // Computes the determinant of the input matrix B.
  // Since P.B.Q^{-1} = L.U, det(P) * det(B) * det(Q^{-1}) = det(L) * det(U).
  // det(L) = 1 since L is a lower-triangular matrix with 1 on the diagonal.
  // det(P) = +1 or -1 (by definition it is the sign of the permutation P)
  // det(Q^{-1}) = +1 or -1 (the sign of the permutation Q^{-1})
  // Finally det(U) = product of the diagonal elements of U, since U is an
  // upper-triangular matrix.
  // Taking all this into account:
  // det(B) = sign(P) * sign(Q^{-1}) * prod_i u_ii .
  Fractional ComputeDeterminant() const;

  // Computes the 1-norm of the inverse of the input matrix B.
  // For this we iteratively solve B.x = e_j, where e_j is the jth unit vector.
  // The result of this computation is the jth column of B^-1.
  // The 1-norm |B| is defined as max_j sum_i |a_ij|
  // http://en.wikipedia.org/wiki/Matrix_norm
  Fractional ComputeInverseOneNorm() const;

  // Computes the infinity-norm of the inverse of the input matrix B.
  // The infinity-norm |B| is defined as max_i sum_j |a_ij|
  // http://en.wikipedia.org/wiki/Matrix_norm
  Fractional ComputeInverseInfinityNorm() const;

  // Computes the condition number of the input matrix B.
  // For a given norm, this is the matrix norm times the norm of its inverse.
  //
  // Note that because the LuFactorization class does not keep the
  // non-factorized matrix in memory, it needs to be passed to these functions.
  // It is up to the client to pass exactly the same matrix as the one used
  // for ComputeFactorization().
  //
  // TODO(user): separate this from LuFactorization.
  Fractional ComputeOneNormConditionNumber(const MatrixView& matrix) const;
  Fractional ComputeInfinityNormConditionNumber(const MatrixView& matrix) const;

  // Sets the current parameters.
  void SetParameters(const GlopParameters& parameters) {
    parameters_ = parameters;
    markowitz_.SetParameters(parameters);
  }

  // Returns a std::string containing the statistics for this class.
  std::string StatString() const {
    return stats_.StatString() + markowitz_.StatString();
  }

  // This is only used for testing and in debug mode.
  // TODO(user): avoid the matrix conversion by multiplying TriangularMatrix
  // directly.
  void ComputeLowerTimesUpper(SparseMatrix* product) const {
    SparseMatrix temp_lower, temp_upper;
    lower_.CopyToSparseMatrix(&temp_lower);
    upper_.CopyToSparseMatrix(&temp_upper);
    product->PopulateFromProduct(temp_lower, temp_upper);
  }

  // Visible for testing.
  const RowPermutation& row_perm() const { return row_perm_; }
  const ColumnPermutation& inverse_col_perm() const {
    return inverse_col_perm_;
  }

 private:
  // Statistics about this class.
  struct Stats : public StatsGroup {
    Stats()
        : StatsGroup("LuFactorization"),
          basis_num_entries("basis_num_entries", this),
          lu_fill_in("lu_fill_in", this) {}
    IntegerDistribution basis_num_entries;
    RatioDistribution lu_fill_in;
  };

  // Internal function used in the left solve functions.
  void LeftSolveScratchpad() const;

  // Fills transpose_upper_ from upper_.
  void ComputeTransposeUpper();

  // transpose_lower_ is only needed when we compute dual norms.
  void ComputeTransposeLower() const;

  // Computes R = P.B.Q^{-1} - L.U and returns false if the largest magnitude of
  // the coefficients of P.B.Q^{-1} - L.U is greater than tolerance.
  bool CheckFactorization(const MatrixView& matrix, Fractional tolerance) const;

  // Special case where we have nothing to do. This happens at the beginning
  // when we start the problem with an all-slack basis and gives a good speedup
  // on really easy problems. It is initially true and set to true each time we
  // call Clear(). We set it to false if a call to ComputeFactorization()
  // succeeds.
  bool is_identity_factorization_;

  // The triangular factor L and U (and its transpose).
  TriangularMatrix lower_;
  TriangularMatrix upper_;
  TriangularMatrix transpose_upper_;

  // The transpose of lower_. It is just used by DualEdgeSquaredNorm()
  // and mutable so it can be lazily initialized.
  mutable TriangularMatrix transpose_lower_;

  // The column permutation Q and its inverse Q^{-1} in P.B.Q^{-1} = L.U.
  ColumnPermutation col_perm_;
  ColumnPermutation inverse_col_perm_;

  // The row permutation P and its inverse P^{-1} in P.B.Q^{-1} = L.U.
  RowPermutation row_perm_;
  RowPermutation inverse_row_perm_;

  // Temporary storage used by LeftSolve()/RightSolve().
  mutable DenseColumn dense_column_scratchpad_;

  // Temporary storage used by GetColumnOfU().
  mutable SparseColumn column_of_upper_;

  // Same as dense_column_scratchpad_ but this vector is always reset to zero by
  // the functions that use it. non_zero_rows_ is used to track the
  // non_zero_rows_ position of dense_column_scratchpad_.
  mutable DenseColumn dense_zero_scratchpad_;
  mutable std::vector<RowIndex> non_zero_rows_;

  // Statistics, mutable so const functions can still update it.
  mutable Stats stats_;

  // Proto holding all the parameters of this algorithm.
  GlopParameters parameters_;

  // The class doing the Markowitz LU factorization.
  Markowitz markowitz_;

  DISALLOW_COPY_AND_ASSIGN(LuFactorization);
};

}  // namespace glop
}  // namespace operations_research
#endif  // OR_TOOLS_GLOP_LU_FACTORIZATION_H_
