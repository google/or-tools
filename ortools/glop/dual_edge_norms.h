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

#ifndef OR_TOOLS_GLOP_DUAL_EDGE_NORMS_H_
#define OR_TOOLS_GLOP_DUAL_EDGE_NORMS_H_

#include "ortools/glop/basis_representation.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/permutation.h"
#include "ortools/lp_data/scattered_vector.h"
#include "ortools/util/stats.h"

namespace operations_research {
namespace glop {

// This class maintains the dual edge squared norms to be used in the
// dual steepest edge pricing. The dual edge u_i associated with a basic
// variable of row index i is such that u_i.B = e_i where e_i is the unit row
// vector with a 1.0 at position i and B the current basis. We call such vector
// u_i an unit row left inverse, and it can be computed by
//
//    basis_factorization.LeftSolveForUnitRow(i, &u_i);
//
// Instead of computing each ||u_i|| at every iteration, it is more efficient to
// update them incrementally for each basis pivot applied to B. See the code or
// the papers below for details:
//
// J.J. Forrest, D. Goldfarb, "Steepest-edge simplex algorithms for linear
// programming", Mathematical Programming 57 (1992) 341-374, North-Holland.
// http://www.springerlink.com/content/q645w3t2q229m248/
//
// Achim Koberstein, "The dual simplex method, techniques for a fast and stable
// implementation", PhD, Paderborn, Univ., 2005.
// http://digital.ub.uni-paderborn.de/hs/download/pdf/3885?originalFilename=true
class DualEdgeNorms {
 public:
  // Takes references to the linear program data we need.
  explicit DualEdgeNorms(const BasisFactorization& basis_factorization);

  // Clears, i.e. reset the object to its initial value. This will trigger a
  // full norm recomputation on the next GetEdgeSquaredNorms().
  void Clear();

  // When we just add new constraints to the matrix and use an incremental
  // solve, we do not need to recompute the norm of the old rows, and the norm
  // of the new ones can be just set to 1 as long as we use identity columns for
  // these.
  void ResizeOnNewRows(RowIndex new_size);

  // If this is true, then the caller must re-factorize the basis before the
  // next call to GetEdgeSquaredNorms(). This is because the latter will
  // recompute the norms from scratch and therefore needs a hightened precision
  // and speed.
  bool NeedsBasisRefactorization();

  // Returns the dual edge squared norms. This is only valid if the caller
  // properly called UpdateBeforeBasisPivot() before each basis pivot, or just
  // called Clear().
  const DenseColumn& GetEdgeSquaredNorms();

  // Updates the norms if the columns of the basis where permuted.
  void UpdateDataOnBasisPermutation(const ColumnPermutation& col_perm);

  // Updates the norms just before a basis pivot is applied:
  // - The column at leaving_row will leave the basis and the column at
  //   entering_col will enter it.
  // - direction is the right inverse of the entering column.
  // - unit_row_left_inverse is the left inverse of the unit row with index
  //   given by the leaving_row. This is also the leaving dual edge.
  void UpdateBeforeBasisPivot(ColIndex entering_col, RowIndex leaving_row,
                              const ScatteredColumn& direction,
                              const ScatteredRow& unit_row_left_inverse);

  // Sets the algorithm parameters.
  void SetParameters(const GlopParameters& parameters) {
    parameters_ = parameters;
  }

  // Stats related functions.
  std::string StatString() const { return stats_.StatString(); }

 private:
  // Recomputes the dual edge squared norms from scratch with maximum precision.
  // The matrix must have been refactorized before because we will do a lot of
  // inversions. See NeedsBasisRefactorization(). This is checked in debug mode.
  void ComputeEdgeSquaredNorms();

  // Computes the vector tau needed to update the norms using a right solve:
  //     B.tau = (u_i)^T, u_i.B = e_i for i = leaving_row.
  const DenseColumn& ComputeTau(const ScatteredColumn& unit_row_left_inverse);

  // Statistics.
  struct Stats : public StatsGroup {
    Stats()
        : StatsGroup("DualEdgeNorms"),
          tau_density("tau_density", this),
          edge_norms_accuracy("edge_norms_accuracy", this),
          lower_bounded_norms("lower_bounded_norms", this) {}
    RatioDistribution tau_density;
    DoubleDistribution edge_norms_accuracy;
    IntegerDistribution lower_bounded_norms;
  };
  Stats stats_;

  // Parameters.
  GlopParameters parameters_;

  // Problem data that should be updated from outside.
  const BasisFactorization& basis_factorization_;

  // The dual edge norms.
  DenseColumn edge_squared_norms_;

  // Whether we should recompute the norm from scratch.
  bool recompute_edge_squared_norms_;

  DISALLOW_COPY_AND_ASSIGN(DualEdgeNorms);
};

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_GLOP_DUAL_EDGE_NORMS_H_
