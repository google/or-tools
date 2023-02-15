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

#ifndef OR_TOOLS_GLOP_PRIMAL_EDGE_NORMS_H_
#define OR_TOOLS_GLOP_PRIMAL_EDGE_NORMS_H_

#include <cstdint>
#include <string>
#include <vector>

#include "ortools/glop/basis_representation.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/update_row.h"
#include "ortools/glop/variables_info.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/scattered_vector.h"
#include "ortools/util/stats.h"

namespace operations_research {
namespace glop {

// This class maintains the primal edge squared norms (and other variants) to be
// used in the primal pricing step. Instead of computing the needed values from
// scractch at each iteration, it is more efficient to update them incrementally
// for each basis pivot applied to the simplex basis matrix B.
//
// Terminology:
// - To each non-basic column 'a' of a matrix A, we can associate an "edge" in
//   the kernel of A equal to 1.0 on the index of 'a' and '-B^{-1}.a' on the
//   basic variables.
// - 'B^{-1}.a' is called the "right inverse" of 'a'.
// - The entering edge is the edge we are following during a simplex step,
//   and we call "direction" the reverse of this edge restricted to the
//   basic variables, i.e. the right inverse of the entering column.
//
// Papers:
// - D. Goldfarb, J.K. Reid, "A practicable steepest-edge simplex algorithm"
//   Mathematical Programming 12 (1977) 361-371, North-Holland.
//   http://www.springerlink.com/content/g8335137n3j16934/
// - J.J. Forrest, D. Goldfarb, "Steepest-edge simplex algorithms for linear
//   programming", Mathematical Programming 57 (1992) 341-374, North-Holland.
//   http://www.springerlink.com/content/q645w3t2q229m248/
// - Ping-Qi Pan "A fast simplex algorithm for linear programming".
//   http://www.optimization-online.org/DB_FILE/2007/10/1805.pdf
// - Ping-Qi Pan, "Efficient nested pricing in the simplex algorithm",
//   http://www.optimization-online.org/DB_FILE/2007/10/1810.pdf
class PrimalEdgeNorms {
 public:
  // Takes references to the linear program data we need. Note that we assume
  // that the matrix will never change in our back, but the other references are
  // supposed to reflect the correct state.
  PrimalEdgeNorms(const CompactSparseMatrix& compact_matrix,
                  const VariablesInfo& variables_info,
                  const BasisFactorization& basis_factorization);

  // Clears, i.e. resets the object to its initial value. This will trigger
  // a recomputation for the next Get*() method call.
  void Clear();

  // If this is true, then the caller must re-factorize the basis before the
  // next call to GetEdgeSquaredNorms(). This is because the latter will
  // recompute the norms from scratch and therefore needs a hightened precision
  // and speed.
  bool NeedsBasisRefactorization() const;

  // Depending on the SetPricingRule(), this returns one of the "norms" vector
  // below. Note that all norms are squared.
  const DenseRow& GetSquaredNorms();

  // Returns the primal edge squared norms. This is only valid if the caller
  // properly called UpdateBeforeBasisPivot() before each basis pivot, or if
  // this is the first call to this function after a Clear(). Note that only the
  // relevant columns are filled.
  const DenseRow& GetEdgeSquaredNorms();

  // Returns an approximation of the edges norms "devex".
  // This is only valid if the caller properly called UpdateBeforeBasisPivot()
  // before each basis pivot, or if this is the first call to this function
  // after a Clear().
  const DenseRow& GetDevexWeights();

  // Returns the L2 norms of all the columns of A.
  // Note that this is currently not cleared by Clear().
  const DenseRow& GetMatrixColumnNorms();

  // Compares the current entering edge norm with its precise version (using the
  // direction that wasn't avaible before) and triggers a full recomputation if
  // the precision is not good enough (see recompute_edges_norm_threshold in
  // GlopParameters). As a side effect, this replace the entering_col edge
  // norm with its precise version.
  //
  // Returns false if the old norm is less that 0.25 the new one. We might want
  // to change the leaving variable if this happens.
  bool TestEnteringEdgeNormPrecision(ColIndex entering_col,
                                     const ScatteredColumn& direction);

  // Updates any internal data BEFORE the given simplex pivot is applied to B.
  // Note that no updates are needed in case of a bound flip.
  // The arguments are in order:
  // - The index of the entering non-basic column of A.
  // - The index in B of the leaving basic variable.
  // - The 'direction', i.e. the right inverse of the entering column.
  // - The update row (see UpdateRow), which will only be computed if needed.
  void UpdateBeforeBasisPivot(ColIndex entering_col, ColIndex leaving_col,
                              RowIndex leaving_row,
                              const ScatteredColumn& direction,
                              UpdateRow* update_row);

  // Sets the algorithm parameters.
  void SetParameters(const GlopParameters& parameters) {
    parameters_ = parameters;
  }

  // This changes what GetSquaredNorms() returns.
  void SetPricingRule(GlopParameters::PricingRule rule) {
    pricing_rule_ = rule;
  }

  // Registers a boolean that will be set to true each time the norms are or
  // will be recomputed. This allows anyone that depends on this to know that it
  // cannot just assume an incremental changes and needs to updates its data.
  // Important: UpdateBeforeBasisPivot() will not trigger this.
  void AddRecomputationWatcher(bool* watcher) { watchers_.push_back(watcher); }

  // Returns a string with statistics about this class.
  std::string StatString() const { return stats_.StatString(); }

  // Deterministic time used by the scalar product computation of this class.
  double DeterministicTime() const {
    return DeterministicTimeForFpOperations(num_operations_);
  }

 private:
  // Statistics about this class.
  struct Stats : public StatsGroup {
    Stats()
        : StatsGroup("PrimalEdgeNorms"),
          direction_left_inverse_density("direction_left_inverse_density",
                                         this),
          direction_left_inverse_accuracy("direction_left_inverse_accuracy",
                                          this),
          edges_norm_accuracy("edges_norm_accuracy", this),
          lower_bounded_norms("lower_bounded_norms", this) {}
    RatioDistribution direction_left_inverse_density;
    DoubleDistribution direction_left_inverse_accuracy;
    DoubleDistribution edges_norm_accuracy;
    IntegerDistribution lower_bounded_norms;
  };

  // Recompute the matrix column L2 norms from scratch.
  void ComputeMatrixColumnNorms();

  // Recompute the edge squared L2 norms from scratch.
  void ComputeEdgeSquaredNorms();

  // Compute the left inverse of the direction.
  // The first argument is there for checking precision.
  void ComputeDirectionLeftInverse(ColIndex entering_col,
                                   const ScatteredColumn& direction);

  // Updates edges_squared_norm_ according to the given pivot.
  void UpdateEdgeSquaredNorms(ColIndex entering_col, ColIndex leaving_col,
                              RowIndex leaving_row,
                              const DenseColumn& direction,
                              const UpdateRow& update_row);

  // Resets all devex weights to 1.0 .
  void ResetDevexWeights();

  // Updates devex_weights_ according to the given pivot.
  void UpdateDevexWeights(ColIndex entering_col, ColIndex leaving_col,
                          RowIndex leaving_row, const DenseColumn& direction,
                          const UpdateRow& update_row);

  // Problem data that should be updated from outside.
  const CompactSparseMatrix& compact_matrix_;
  const VariablesInfo& variables_info_;
  const BasisFactorization& basis_factorization_;

  // Internal data.
  GlopParameters parameters_;
  GlopParameters::PricingRule pricing_rule_ = GlopParameters::DANTZIG;
  Stats stats_;

  // Booleans to control what happens on the next ChooseEnteringColumn() call.
  bool must_refactorize_basis_;
  bool recompute_edge_squared_norms_;
  bool reset_devex_weights_;

  // Norm^2 of the edges of the relevant columns of A.
  DenseRow edge_squared_norms_;

  // Norm of all the columns of A.
  DenseRow matrix_column_norms_;

  // Approximation of edges norms "devex".
  // Denoted by vector 'w' in Pin Qi Pan (1810.pdf section 1.1.4)
  // At any time, devex_weights_ >= 1.0.
  DenseRow devex_weights_;

  // Tracks number of updates of the devex weights since we have to reset
  // them to 1.0 every now and then.
  int num_devex_updates_since_reset_;

  // Left inverse by B of the 'direction'. This is the transpose of 'v' in the
  // steepest edge paper. Its scalar product with a column 'a' of A gives the
  // value of the scalar product of the 'direction' with the right inverse of
  // 'a'.
  ScatteredRow direction_left_inverse_;

  // Used by DeterministicTime().
  int64_t num_operations_;

  // Boolean(s) to set to false when the norms are changed outside of the
  // UpdateBeforeBasisPivot() function.
  std::vector<bool*> watchers_;

  DISALLOW_COPY_AND_ASSIGN(PrimalEdgeNorms);
};

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_GLOP_PRIMAL_EDGE_NORMS_H_
