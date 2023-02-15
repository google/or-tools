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

#ifndef OR_TOOLS_GLOP_UPDATE_ROW_H_
#define OR_TOOLS_GLOP_UPDATE_ROW_H_

#include <cstdint>
#include <string>
#include <vector>

#include "ortools/glop/basis_representation.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/variables_info.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/scattered_vector.h"
#include "ortools/util/stats.h"

namespace operations_research {
namespace glop {

// During a simplex iteration, when the basis 'leaving_row' has been
// selected, one of the main quantities needed in the primal or dual simplex
// algorithm is called the update row.
//
// By definition, update_row[col] is the coefficient at position
// 'leaving_row' in the current basis of the column 'col' of the matrix A.
//
// One efficient way to compute it is to compute the left inverse by B of the
// unit vector with a one at the given leaving_row, and then to take the
// scalar product of this left inverse with all the columns of A:
//     update_row[col] = (unit_{leaving_row} . B^{-1}) . A_col
class UpdateRow {
 public:
  // Takes references to the linear program data we need.
  UpdateRow(const CompactSparseMatrix& matrix,
            const CompactSparseMatrix& transposed_matrix,
            const VariablesInfo& variables_info, const RowToColMapping& basis,
            const BasisFactorization& basis_factorization);

  // Invalidates the current update row and unit_row_left_inverse so the next
  // call to ComputeUpdateRow() will recompute everything and not just return
  // right away.
  void Invalidate();

  // Computes the left inverse of the given unit row, and stores it in
  // unit_row_left_inverse_. The result is computed only once if leaving_row do
  // not change, this until the next Invalidate() call.
  void ComputeUnitRowLeftInverse(RowIndex leaving_row);

  // Computes the relevant coefficients (See GetIsRelevantBitRow() in
  // VariablesInfo) of the update row. The result is only computed once
  // if leaving_row do not change, this until the next Invalidate() call.
  void ComputeUpdateRow(RowIndex leaving_row);

  // Returns the left inverse of the unit row as computed by the last call to
  // ComputeUpdateRow().
  const ScatteredRow& GetUnitRowLeftInverse() const;

  // Returns true if ComputeUpdateRow() was called since the last Invalidate().
  const bool IsComputedFor(RowIndex leaving_row) const {
    return update_row_computed_for_ == leaving_row;
  }

  // Returns the update coefficients and non-zero positions corresponding to the
  // last call to ComputeUpdateRow().
  //
  // TODO(user): Consider returning a packed vector of coefficient parallel to
  // GetNonZeroPositions() instead. It should be fast to compute and iteration
  // later should be quicker.
  const DenseRow& GetCoefficients() const;
  const ColIndexVector& GetNonZeroPositions() const;
  const Fractional GetCoefficient(ColIndex col) const {
    return coefficient_[col];
  }

  // Computes the update row including all position and fill output with it.
  // We only use this when ComputeUnitRowLeftInverse() has already been called
  // and we CHECK that.
  void ComputeFullUpdateRow(RowIndex leaving_row, DenseRow* output) const;

  // Sets the algorithm parameters.
  void SetParameters(const GlopParameters& parameters);

  // Returns statistics about this class as a string.
  std::string StatString() const { return stats_.StatString(); }

  // Only used for testing.
  // Computes as the update row the product 'lhs' times the linear program
  // matrix given at construction. Only the relevant columns matter (see
  // VariablesInfo) and 'algorithm' can be one of "column", "row" or
  // "row_hypersparse".
  void ComputeUpdateRowForBenchmark(const DenseRow& lhs,
                                    const std::string& algorithm);

  // Deterministic time used by the scalar product computation of this class.
  double DeterministicTime() const {
    return DeterministicTimeForFpOperations(num_operations_);
  }

  // This returns the asked unit row left inverse. It temporarily invalidate
  // the class state by calling Invalidate().
  const ScatteredRow& ComputeAndGetUnitRowLeftInverse(RowIndex leaving_row);

 private:
  // ComputeUpdateRow() does the common work and calls one of these functions
  // depending on the situation.
  void ComputeUpdatesRowWise();
  void ComputeUpdatesRowWiseHypersparse();
  void ComputeUpdatesColumnWise();
  void ComputeUpdatesForSingleRow(ColIndex row_as_col);

  // Problem data that should be updated from outside.
  const CompactSparseMatrix& matrix_;
  const CompactSparseMatrix& transposed_matrix_;
  const VariablesInfo& variables_info_;
  const RowToColMapping& basis_;
  const BasisFactorization& basis_factorization_;

  // Left inverse by B of a unit row. Its scalar product with a column 'a' of A
  // gives the value of the right inverse of 'a' on the 'leaving_row'.
  ScatteredRow unit_row_left_inverse_;

  // The non-zeros of unit_row_left_inverse_ above the drop tolerance.
  std::vector<ColIndex> unit_row_left_inverse_filtered_non_zeros_;

  // Holds the current update row data.
  // Note that non_zero_position_set_ is not always up to date.
  ColIndexVector non_zero_position_list_;
  DenseBitRow non_zero_position_set_;
  DenseRow coefficient_;

  // Boolean used to avoid recomputing many times the same thing.
  bool compute_update_row_;
  RowIndex left_inverse_computed_for_ = kInvalidRow;
  RowIndex update_row_computed_for_ = kInvalidRow;

  // Statistics about this class.
  struct Stats : public StatsGroup {
    Stats()
        : StatsGroup("UpdateRow"),
          unit_row_left_inverse_density("unit_row_left_inverse_density", this),
          unit_row_left_inverse_accuracy("unit_row_left_inverse_accuracy",
                                         this),
          update_row_density("update_row_density", this) {}
    RatioDistribution unit_row_left_inverse_density;
    DoubleDistribution unit_row_left_inverse_accuracy;
    RatioDistribution update_row_density;
  };

  // Track the number of basic floating point multiplication.
  // Used by DeterministicTime().
  int64_t num_operations_;

  // Glop standard classes.
  GlopParameters parameters_;
  Stats stats_;

  DISALLOW_COPY_AND_ASSIGN(UpdateRow);
};

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_GLOP_UPDATE_ROW_H_
