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

#ifndef OR_TOOLS_SAT_ZERO_HALF_CUTS_H_
#define OR_TOOLS_SAT_ZERO_HALF_CUTS_H_

#include <functional>
#include <utility>
#include <vector>

#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/util.h"

namespace operations_research {
namespace sat {

// Heuristic to find a good sums of rows from the LP (with coeff -1, +1) that
// can lead to a violated zero-half cut (i.e. after integer rounding with a
// divisor 2).
//
// For this, all that matter is the parity of the coefficients and the rhs in
// the linear combination of the original problem constraint. So this class
// maintain a copy of the LP matrix modulo 2 on which simplification and
// heuristic are performed to find good cut candidates(s).
//
// Most of what is done here is described in the paper "Algorithms to Separate
// {0, 1/2}-Chvátal-Gomory Cuts", Arie M. C. A. Koster, Adrian Zymolka, Manuel
// Kutschka.
class ZeroHalfCutHelper {
 public:
  // Public API: ProcessVariables() must be called first and then constraints
  // can be added one by one. Finally GetZeroHalfInterestingCuts() will return a
  // set of good candidates.
  //
  // TODO(user): This is a first implementation, both the heuristic and the
  // code performance can probably be improved uppon.
  void ProcessVariables(const std::vector<double>& lp_values,
                        const std::vector<IntegerValue>& lower_bounds,
                        const std::vector<IntegerValue>& upper_bounds);
  void AddOneConstraint(
      glop::RowIndex,
      const std::vector<std::pair<glop::ColIndex, IntegerValue>>& terms,
      IntegerValue lb, IntegerValue ub);
  std::vector<std::vector<std::pair<glop::RowIndex, IntegerValue>>>
  InterestingCandidates(ModelRandomGenerator* random);

  // Visible for testing.
  void Reset(int size);

  // Visible for testing.
  //
  // Boolean matrix. Each column correspond to one variable (col indices).
  // Each row to a sum of the initial problem constraints. We store the
  // coefficient modulo 2, so only the positions of the ones.
  struct CombinationOfRows {
    // How this row was formed from the initial problem constraints.
    std::vector<std::pair<glop::RowIndex, IntegerValue>> multipliers;

    // The index of the odd coefficient of this combination.
    std::vector<int> cols;

    // The parity of the rhs (1 for odd).
    int rhs_parity;

    // How tight this constraints is under the current LP solution.
    double slack;
  };
  void AddBinaryRow(const CombinationOfRows& binary_row);
  const CombinationOfRows& MatrixRow(int row) const { return rows_[row]; }
  const std::vector<int>& MatrixCol(int col) const { return col_to_rows_[col]; }

  // Visible for testing.
  //
  // Adds the given row to all other rows having an odd cofficient on the given
  // column. This then eliminate the entry (col, row) that is now a singleton by
  // incresing the slack of the given row.
  void EliminateVarUsingRow(int col, int row);

  // Visible for testing.
  //
  // Like std::set_symmetric_difference, but use a vector<bool> instead of sort.
  // This assumes tmp_marked_ to be all false. We don't DCHECK it here for
  // speed, but it DCHECKed on each EliminateVarUsingRow() call. In addition,
  // the result is filtered using the extra_condition function.
  void SymmetricDifference(std::function<bool(int)> extra_condition,
                           const std::vector<int>& a, std::vector<int>* b);

 private:
  void ProcessSingletonColumns();

  // As we combine rows, when the activity of a combination get too far away
  // from its bound, we just discard it. Note that the row will still be there
  // but its index will not appear in the col-wise representation of the matrix.
  const double kSlackThreshold = 0.5;
  const int kMaxAggregationSize = 100;

  // We don't consider long constraint or constraint with high magnitude, since
  // the highest violation we can hope for is 1, and if the magnitude is large
  // then the cut efficacity will not be great.
  const int kMaxInputConstraintSize = 100;
  const double kMaxInputConstraintMagnitude = 1e6;

  // Variable information.
  std::vector<double> lp_values_;
  std::vector<double> shifted_lp_values_;
  std::vector<int> bound_parity_;

  // Binary matrix.
  //
  // Note that as we combine rows, we never move their indices. So after initial
  // creation rows_ will always have the same size.
  std::vector<CombinationOfRows> rows_;
  std::vector<std::vector<int>> col_to_rows_;
  std::vector<int> singleton_cols_;

  // Temporary vector used by SymmetricDifference().
  std::vector<bool> tmp_marked_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_ZERO_HALF_CUTS_H_
