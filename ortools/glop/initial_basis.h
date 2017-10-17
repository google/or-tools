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

#ifndef OR_TOOLS_GLOP_INITIAL_BASIS_H_
#define OR_TOOLS_GLOP_INITIAL_BASIS_H_

#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/sparse.h"

namespace operations_research {
namespace glop {

// This class implements two initial basis algorithms. The idea is to replace as
// much as possible the columns of B that correspond to fixed slack variables
// with some column of A in order to have more freedom in the values the basic
// variables can take.
//
// The first algorithm is Bixby's initial basis algorithm, described in the
// paper below. It considers the columns of A in a particular order (the ones
// with more freedom first) and adds the current column to the basis if it keeps
// B almost triangular and with coefficients close to 1.0 on the diagonal for
// good numerical stability.
//
// Robert E. Bixby, "Implementing the Simplex Method: The Initial Basis"
// ORSA Jounal on Computing, Vol. 4, No. 3, Summer 1992.
// http://joc.journal.informs.org/content/4/3/267.abstract
//
// The second algorithm is is similar to the "advanced initial basis" that GLPK
// uses by default. It adds columns one by one to the basis B while keeping it
// triangular (not almost triangular as in Bixby's algorithm). The next
// column to add is chosen amongst the set of possible candidates using a
// heuristic similar to the one used by Bixby.
class InitialBasis {
 public:
  // Takes references to the linear program data we need.
  InitialBasis(const MatrixView& matrix, const DenseRow& objective,
               const DenseRow& lower_bound, const DenseRow& upper_bound,
               const VariableTypeRow& variable_type);

  // Completes the entries of the given basis that are equal to kInvalidCol with
  // one of the first num_cols columns of A using Bixby's algorithm.
  //
  // Important: For this function, the matrix must be scaled such that the
  // maximum absolute value in each column is 1.0.
  void CompleteBixbyBasis(ColIndex num_cols, RowToColMapping* basis);

  // Similar to CompleteBixbyBasis() but completes the basis into a triangular
  // one. This function usually produces better initial bases. The dual version
  // just restricts the possible entering columns to the ones with a cost of 0.0
  // in order to always start with the all-zeros vector of dual values.
  //
  // Returns false if an error occurred during the algorithm (numerically
  // instable basis).
  bool CompleteTriangularPrimalBasis(ColIndex num_cols, RowToColMapping* basis);
  bool CompleteTriangularDualBasis(ColIndex num_cols, RowToColMapping* basis);

  // Visible for testing. Computes a list of candidate column indices out of the
  // fist num_candidate_columns of A and sorts them using the
  // bixby_column_comparator_. This also fills max_scaled_abs_cost_.
  void ComputeCandidates(ColIndex num_candidate_columns,
                         std::vector<ColIndex>* candidates);

 private:
  // Internal implementation of the Primal/Dual CompleteTriangularBasis().
  template <bool only_allow_zero_cost_column>
  bool CompleteTriangularBasis(ColIndex num_cols, RowToColMapping* basis);

  // Returns an integer representing the order (the lower the better)
  // between column categories (known as C2, C3 or C4 in the paper).
  // Also returns a greater index for fixed columns.
  int GetColumnCategory(ColIndex col) const;

  // Returns the penalty (the lower the better) of a column. This is 'q_j' for a
  // column 'j' in the paper.
  Fractional GetColumnPenalty(ColIndex col) const;

  // Maximum scaled absolute value of the objective for the columns which are
  // entering candidates. This is used by GetColumnPenalty().
  Fractional max_scaled_abs_cost_;

  // Comparator used to sort column indices according to their penalty.
  // Lower is better.
  struct BixbyColumnComparator {
    explicit BixbyColumnComparator(const InitialBasis& initial_basis)
        : initial_basis_(initial_basis) {}
    bool operator()(ColIndex col_a, ColIndex col_b) const;
    const InitialBasis& initial_basis_;
  } bixby_column_comparator_;

  // Comparator used by CompleteTriangularBasis(). Note that this one is meant
  // to be used by a priority queue, so higher is better.
  struct TriangularColumnComparator {
    explicit TriangularColumnComparator(const InitialBasis& initial_basis)
        : initial_basis_(initial_basis) {}
    bool operator()(ColIndex col_a, ColIndex col_b) const;
    const InitialBasis& initial_basis_;
  } triangular_column_comparator_;

  const MatrixView& matrix_;
  const DenseRow& objective_;
  const DenseRow& lower_bound_;
  const DenseRow& upper_bound_;
  const VariableTypeRow& variable_type_;

  DISALLOW_COPY_AND_ASSIGN(InitialBasis);
};

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_GLOP_INITIAL_BASIS_H_
