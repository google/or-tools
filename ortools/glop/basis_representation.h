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


#ifndef OR_TOOLS_GLOP_BASIS_REPRESENTATION_H_
#define OR_TOOLS_GLOP_BASIS_REPRESENTATION_H_

#include "ortools/base/logging.h"
#include "ortools/glop/lu_factorization.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/rank_one_update.h"
#include "ortools/glop/status.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/sparse.h"
#include "ortools/util/stats.h"

namespace operations_research {
namespace glop {

// An eta matrix E corresponds to the identity matrix except for one column e of
// index j. In particular, B.E is the matrix of the new basis obtained from B by
// replacing the j-th vector of B by B.e, note that this is exactly what happens
// during a "pivot" of the current basis in the simplex algorithm.
//
// E = [  1  ...  0    e_0    0  ...  0
//       ... ... ...   ...   ... ... ...
//        0  ...  1  e_{j-1}  0  ...  0
//        0  ...  0    e_j    0  ...  0
//        0  ...  0  e_{j+1}  1  ...  0
//       ... ... ...   ...   ... ... ...
//        0  ...  0  e_{n-1}  0  ...  1 ]
//
// The inverse of the eta matrix is:
// E^{-1} = [  1  ...  0      -e_0/e_j  0  ...  0
//            ... ... ...     ...      ... ... ...
//             0  ...  1  -e_{j-1}/e_j  0  ...  0
//             0  ...  0         1/e_j  0  ...  0
//             0  ...  0  -e_{j+1}/e_j  1  ...  0
//            ... ... ...     ...      ... ... ...
//             0  ...  0  -e_{n-1}/e_j  0  ...  1 ]
class EtaMatrix {
 public:
  EtaMatrix(ColIndex eta_col, const std::vector<RowIndex>& eta_non_zeros,
            DenseColumn* dense_eta);
  virtual ~EtaMatrix();

  // Solves the system y.E = c, 'c' beeing the initial value of 'y'.
  // Then y = c.E^{-1}, so y is equal to c except for
  //    y_j = (c_j - \sum_{i != j}{c_i * e_i}) / e_j.
  void LeftSolve(DenseRow* y) const;

  // Same as LeftSolve(), but 'pos' contains the non-zero positions of c. The
  // order of the positions is not important, but there must be no duplicates.
  // The values not in 'pos' are not used. If eta_col_ was not already in 'pos',
  // it is added.
  void SparseLeftSolve(DenseRow* y, ColIndexVector* pos) const;

  // Solves the system E.d = a, 'a' beeing the initial value of 'd'.
  // Then d = E^{-1}.a = [ a_0     - e_0   *   a_j / e_j
  //                                 ...
  //                       a_{j-1} - e_{j-1} * a_j / e_j
  //                                           a_j / e_j
  //                       a_{j+1} - e_{j+1} * a_j / e_j
  //                                 ...
  //                       a_{n-1} - e_{n-1} * a_j / e_j ]
  void RightSolve(DenseColumn* d) const;

 private:
  // Internal RightSolve() and LeftSolve() implementations using either the
  // dense or the sparse representation of the eta vector.
  void LeftSolveWithDenseEta(DenseRow* y) const;
  void LeftSolveWithSparseEta(DenseRow* y) const;
  void RightSolveWithDenseEta(DenseColumn* d) const;
  void RightSolveWithSparseEta(DenseColumn* d) const;

  // If an eta vector density is smaller than this threshold, we use the
  // sparse version of the Solve() functions rather than the dense version.
  // TODO(user): Detect automatically a good parameter? 0.5 is a good value on
  // the Netlib (I only did a few experiments though). Note that in the future
  // we may not even keep the dense representation at all.
  static const Fractional kSparseThreshold;

  const ColIndex eta_col_;
  const Fractional eta_col_coefficient_;

  // Note that to optimize solves, the position eta_col_ is set to 0.0 and
  // stored in eta_col_coefficient_ instead.
  DenseColumn eta_coeff_;
  SparseColumn sparse_eta_coeff_;

  DISALLOW_COPY_AND_ASSIGN(EtaMatrix);
};

// An eta factorization corresponds to the product of k eta matrices,
// i.e. E = E_0.E_1. ... .E_{k-1}
// It is used to solve two systems:
//   - E.d = a (where a is usually the entering column).
//   - y.E = c (where c is usually the objective row).
class EtaFactorization {
 public:
  EtaFactorization();
  virtual ~EtaFactorization();

  // Deletes all eta matrices.
  void Clear();

  // Updates the eta factorization, i.e. adds the new eta matrix defined by
  // the leaving variable and the corresponding eta column.
  void Update(ColIndex entering_col, RowIndex leaving_variable_row,
              const std::vector<RowIndex>& eta_non_zeros,
              DenseColumn* dense_eta);

  // Left solves all systems from right to left, i.e. y_i = y_{i+1}.(E_i)^{-1}
  void LeftSolve(DenseRow* y) const;

  // Same as LeftSolve(), but 'pos' contains the non-zero positions of c. The
  // order of the positions is not important, but there must be no duplicates.
  // The values not in 'pos' are not used. If eta_col_ was not already in 'pos',
  // it is added.
  void SparseLeftSolve(DenseRow* y, ColIndexVector* pos) const;

  // Right solves all systems from left to right, i.e. E_i.d_{i+1} = d_i
  void RightSolve(DenseColumn* d) const;

 private:
  std::vector<EtaMatrix*> eta_matrix_;

  DISALLOW_COPY_AND_ASSIGN(EtaFactorization);
};

// A basis factorization is the product of an eta factorization and
// a L.U decomposition, i.e. B = L.U.E_0.E_1. ... .E_{k-1}
// It is used to solve two systems:
//   - B.d = a where a is the entering column.
//   - y.B = c where c is the objective row.
//
// To speed-up and improve stability the factorization is refactorized at least
// every 'refactorization_period' updates.
class BasisFactorization {
 public:
  BasisFactorization(const MatrixView& matrix, const RowToColMapping& basis);
  virtual ~BasisFactorization();

  // Sets the parameters for this component.
  void SetParameters(const GlopParameters& parameters) {
    max_num_updates_ = parameters.basis_refactorization_period();
    use_middle_product_form_update_ =
        parameters.use_middle_product_form_update();
    parameters_ = parameters;
    lu_factorization_.SetParameters(parameters);
  }

  // Returns the column permutation used by the LU factorization.
  // This call only makes sense if the basis was just refactorized.
  const ColumnPermutation& GetColumnPermutation() const {
    DCHECK(IsRefactorized());
    return lu_factorization_.GetColumnPermutation();
  }

  // Sets the column permutation used by the LU factorization to the identity.
  // Hense the Solve() results will be computed without this permutation.
  // This call only makes sense if the basis was just refactorized.
  void SetColumnPermutationToIdentity() {
    DCHECK(IsRefactorized());
    lu_factorization_.SetColumnPermutationToIdentity();
  }

  // Clears the factorization and resets it to an identity matrix of size given
  // by matrix_.num_rows().
  void Clear();

  // Clears the factorization and initializes the class using the current
  // matrix_ and basis_. This is fast if IsIdentityBasis() is true, otherwise
  // it will trigger a refactorization and will return an error if the matrix
  // could not be factorized.
  Status Initialize() MUST_USE_RESULT;

  // Return the number of rows in the basis.
  RowIndex GetNumberOfRows() const { return matrix_.num_rows(); }

  // Clears eta factorization and refactorizes LU.
  // Nothing happens if this is called on an already refactorized basis.
  // Returns an error if the matrix could not be factorized: i.e. not a basis.
  Status Refactorize() MUST_USE_RESULT;

  // Like Refactorize(), but do it even if IsRefactorized() is true.
  // Call this if the underlying basis_ changed and Update() wasn't called.
  Status ForceRefactorization() MUST_USE_RESULT;

  // Returns true if the factorization was just recomputed.
  bool IsRefactorized() const;

  // Updates the factorization. The 'eta' column will be modified with a swap to
  // avoid a copy (only if the standard eta update is used). Returns an error if
  // the matrix could not be factorized: i.e. not a basis.
  Status Update(ColIndex entering_col, RowIndex leaving_variable_row,
                const std::vector<RowIndex>& eta_non_zeros,
                DenseColumn* dense_eta) MUST_USE_RESULT;

  // Left solves the system y.B = rhs, where y initialy contains rhs.
  // The second version also computes the non-zero positions of the result.
  void LeftSolve(DenseRow* y) const;
  void LeftSolveWithNonZeros(DenseRow* y, ColIndexVector* non_zeros) const;

  // Left solves the system y.B = e_j, where e_j has only 1 non-zero
  // coefficient of value 1.0 at position 'j'.
  void LeftSolveForUnitRow(ColIndex j, DenseRow* y,
                           ColIndexVector* non_zeros) const;

  // Same as RightSolve() for matrix.column(col).
  // This also exploits its sparsity.
  void RightSolveForProblemColumn(ColIndex col, DenseColumn* d,
                                  std::vector<RowIndex>* non_zeros) const;

  // Right solves the system B.d = a where the input is the initial value of d.
  void RightSolve(DenseColumn* d) const;
  void RightSolveWithNonZeros(DenseColumn* d,
                              std::vector<RowIndex>* non_zeros) const;

  // Specialized version for ComputeTau() in DualEdgeNorms. This reuses an
  // intermediate result of the last LeftSolveForUnitRow() in order to save a
  // permutation if it is available. Note that the input 'a' should always be
  // equal to the last result of LeftSolveForUnitRow() and will be used for a
  // DCHECK() or if the intermediate result wasn't kept.
  DenseColumn* RightSolveForTau(ScatteredColumnReference a) const;

  // Returns the norm of B^{-1}.a, this is a specific function because
  // it is a bit faster and it avoids polluting the stats of RightSolve().
  // It can be called only when IsRefactorized() is true.
  Fractional RightSolveSquaredNorm(const SparseColumn& a) const;

  // Returns the norm of (B^T)^{-1}.e_row where e is an unit vector.
  // This is a bit faster and avoids polluting the stats of LeftSolve().
  // It can be called only when IsRefactorized() is true.
  Fractional DualEdgeSquaredNorm(RowIndex row) const;

  // Computes the condition number of B.
  // For a given norm, this is the matrix norm times the norm of its inverse.
  // A condition number greater than 1E7 will lead to precision problems.
  Fractional ComputeOneNormConditionNumber() const;
  Fractional ComputeInfinityNormConditionNumber() const;

  // Computes the 1-norm of B.
  // The 1-norm |A| is defined as max_j sum_i |a_ij|
  // http://en.wikipedia.org/wiki/Matrix_norm
  Fractional ComputeOneNorm() const;

  // Computes the infinity-norm of B.
  // The infinity-norm |A| is defined as max_i sum_j |a_ij|
  // http://en.wikipedia.org/wiki/Matrix_norm
  Fractional ComputeInfinityNorm() const;

  // Computes the 1-norm of the inverse of B.
  // For this we iteratively solve B.x = e_j, where e_j is the jth unit vector.
  // The result of this computation is the jth column of B^-1.
  Fractional ComputeInverseOneNorm() const;

  // Computes the infinity-norm of the inverse of B.
  Fractional ComputeInverseInfinityNorm() const;

  // Stats related function.
  // Note that ResetStats() could be const, but until needed it is not to
  // prevent anyone holding a const BasisFactorization& to call it.
  std::string StatString() const {
    return stats_.StatString() + lu_factorization_.StatString();
  }
  void ResetStats() { stats_.Reset(); }

  // The deterministic time used by this class. It is incremented for each
  // solve and each factorization.
  double DeterministicTime() const;

 private:
  // Return true if the submatrix of matrix_ given by basis_ is exactly the
  // identity (without permutation).
  bool IsIdentityBasis() const;

  // Updates the factorization using the middle product form update.
  // Qi Huangfu, J. A. Julian Hall, "Novel update techniques for the revised
  // simplex method", 28 january 2013, Technical Report ERGO-13-0001
  Status MiddleProductFormUpdate(ColIndex entering_col,
                                 RowIndex leaving_variable_row) MUST_USE_RESULT;

  // Increases the deterministic time for a solve operation with a vector having
  // this number of non-zero entries (it can be an approximation).
  void BumpDeterministicTimeForSolve(int num_entries) const;

  // Stats about this class.
  struct Stats : public StatsGroup {
    Stats()
        : StatsGroup("BasisFactorization"),
          refactorization_interval("refactorization_interval", this) {}
    IntegerDistribution refactorization_interval;
  };

  // Mutable because we track the running time of const method like
  // RightSolve() and LeftSolve().
  mutable Stats stats_;
  GlopParameters parameters_;

  // References to the basis subpart of the linear program matrix.
  const MatrixView& matrix_;
  const RowToColMapping& basis_;

  // Middle form product update factorization and scratchpad_ used to construct
  // new rank one matrices.
  RankOneUpdateFactorization rank_one_factorization_;
  mutable DenseColumn scratchpad_;
  mutable std::vector<RowIndex> scratchpad_non_zeros_;

  // This is used by RightSolveForTau(). It holds an intermediate result from
  // the last LeftSolveForUnitRow() and also the final result of
  // RightSolveForTau().
  mutable DenseColumn tau_;
  mutable std::vector<RowIndex> tau_non_zeros_;

  // Booleans controlling the interaction between LeftSolveForUnitRow() that may
  // or may not keep its intermediate results for the optimized
  // RightSolveForTau().
  //
  // tau_computation_can_be_optimized_ will be true iff LeftSolveForUnitRow()
  // kept its intermediate result when it was called and the factorization
  // didn't change since then. If it is true, then RightSolveForTau() can use
  // this result for a faster computation.
  //
  // tau_is_computed_ is used as an heuristic by LeftSolveForUnitRow() to decide
  // if it is worth keeping its intermediate result (which is sligthly slower).
  // It is simply set to true by RightSolveForTau() and to false by
  // LeftSolveForUnitRow(), this way the optimization will automatically switch
  // itself on when switching from the primal simplex (where RightSolveForTau()
  // is never called) to the dual where it is called after each
  // LeftSolveForUnitRow(), and back off again in the other direction.
  mutable bool tau_computation_can_be_optimized_;
  mutable bool tau_is_computed_;

  // Data structure to store partial solve results for the middle form product
  // update. See LeftSolveForUnitRow() and RightSolveForProblemColumn(). We use
  // two CompactSparseMatrix to have a better cache behavior when solving with
  // the rank_one_factorization_.
  mutable CompactSparseMatrix storage_;
  mutable CompactSparseMatrix right_storage_;
  mutable ColMapping left_pool_mapping_;
  mutable ColMapping right_pool_mapping_;

  bool use_middle_product_form_update_;
  int max_num_updates_;
  int num_updates_;
  EtaFactorization eta_factorization_;
  LuFactorization lu_factorization_;

  // mutable because the Solve() functions are const but need to update this.
  mutable double deterministic_time_;

  DISALLOW_COPY_AND_ASSIGN(BasisFactorization);
};

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_GLOP_BASIS_REPRESENTATION_H_
