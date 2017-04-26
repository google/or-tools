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


#include "ortools/glop/initial_basis.h"
#include <queue>

#include "ortools/glop/markowitz.h"
#include "ortools/lp_data/lp_utils.h"

namespace operations_research {
namespace glop {

InitialBasis::InitialBasis(const MatrixView& matrix, const DenseRow& objective,
                           const DenseRow& lower_bound,
                           const DenseRow& upper_bound,
                           const VariableTypeRow& variable_type)
    : max_scaled_abs_cost_(0.0),
      bixby_column_comparator_(*this),
      triangular_column_comparator_(*this),
      matrix_(matrix),
      objective_(objective),
      lower_bound_(lower_bound),
      upper_bound_(upper_bound),
      variable_type_(variable_type) {}

void InitialBasis::CompleteBixbyBasis(ColIndex num_cols,
                                      RowToColMapping* basis) {
  // Initialize can_be_replaced ('I' in Bixby's paper) and has_zero_coefficient
  // ('r' in Bixby's paper).
  const RowIndex num_rows = matrix_.num_rows();
  DenseBooleanColumn can_be_replaced(num_rows, false);
  DenseBooleanColumn has_zero_coefficient(num_rows, false);
  DCHECK_EQ(num_rows, basis->size());
  basis->resize(num_rows, kInvalidCol);
  for (RowIndex row(0); row < num_rows; ++row) {
    if ((*basis)[row] == kInvalidCol) {
      can_be_replaced[row] = true;
      has_zero_coefficient[row] = true;
    }
  }

  // This is 'v' in Bixby's paper.
  DenseColumn scaled_diagonal_abs(matrix_.num_rows(), kInfinity);

  // Compute a list of candidate indices and sort them using the heuristic
  // described in Bixby's paper.
  std::vector<ColIndex> candidates;
  ComputeCandidates(num_cols, &candidates);

  // Loop over the candidate columns, and add them to the basis if the
  // heuristics are satisfied.
  for (int i = 0; i < candidates.size(); ++i) {
    bool enter_basis = false;
    const ColIndex candidate_col_index = candidates[i];
    const SparseColumn& candidate_col = matrix_.column(candidate_col_index);

    // Bixby's heuristic only works with scaled columns. This should be the
    // case by default since we only use this when the matrix is scaled, but
    // it is not the case for our tests... The overhead for computing the
    // infinity norm for each column should be minimal.
    if (InfinityNorm(candidate_col) != 1.0) continue;

    RowIndex candidate_row;
    Fractional candidate_coeff = RestrictedInfinityNorm(
        candidate_col, has_zero_coefficient, &candidate_row);
    const Fractional kBixbyHighThreshold = 0.99;
    if (candidate_coeff > kBixbyHighThreshold) {
      enter_basis = true;
    } else if (IsDominated(candidate_col, scaled_diagonal_abs)) {
      candidate_coeff = RestrictedInfinityNorm(candidate_col, can_be_replaced,
                                               &candidate_row);
      if (candidate_coeff != 0.0) {
        enter_basis = true;
      }
    }

    if (enter_basis) {
      can_be_replaced[candidate_row] = false;
      SetSupportToFalse(candidate_col, &has_zero_coefficient);
      const Fractional kBixbyLowThreshold = 0.01;
      scaled_diagonal_abs[candidate_row] =
          kBixbyLowThreshold * std::abs(candidate_coeff);
      (*basis)[candidate_row] = candidate_col_index;
    }
  }
}

bool InitialBasis::CompleteTriangularPrimalBasis(ColIndex num_cols,
                                                 RowToColMapping* basis) {
  return CompleteTriangularBasis<false>(num_cols, basis);
}

bool InitialBasis::CompleteTriangularDualBasis(ColIndex num_cols,
                                               RowToColMapping* basis) {
  return CompleteTriangularBasis<true>(num_cols, basis);
}

template <bool only_allow_zero_cost_column>
bool InitialBasis::CompleteTriangularBasis(ColIndex num_cols,
                                           RowToColMapping* basis) {
  // Initialize can_be_replaced.
  const RowIndex num_rows = matrix_.num_rows();
  DenseBooleanColumn can_be_replaced(num_rows, false);
  DCHECK_EQ(num_rows, basis->size());
  basis->resize(num_rows, kInvalidCol);
  for (RowIndex row(0); row < num_rows; ++row) {
    if ((*basis)[row] == kInvalidCol) {
      can_be_replaced[row] = true;
    }
  }

  // Initialize the residual non-zero pattern for the rows that can be replaced.
  MatrixNonZeroPattern residual_pattern;
  residual_pattern.Reset(num_rows, num_cols);
  for (ColIndex col(0); col < num_cols; ++col) {
    if (only_allow_zero_cost_column && objective_[col] != 0.0) continue;
    for (const SparseColumn::Entry e : matrix_.column(col)) {
      if (can_be_replaced[e.row()]) {
        residual_pattern.AddEntry(e.row(), col);
      }
    }
  }

  // Initialize a priority queue of residual singleton columns.
  // Also compute max_scaled_abs_cost_ for GetColumnPenalty().
  std::vector<ColIndex> residual_singleton_column;
  max_scaled_abs_cost_ = 0.0;
  for (ColIndex col(0); col < num_cols; ++col) {
    max_scaled_abs_cost_ =
        std::max(max_scaled_abs_cost_, std::abs(objective_[col]));
    if (residual_pattern.ColDegree(col) == 1) {
      residual_singleton_column.push_back(col);
    }
  }
  const Fractional kBixbyWeight = 1000.0;
  max_scaled_abs_cost_ =
      (max_scaled_abs_cost_ == 0.0) ? 1.0 : kBixbyWeight * max_scaled_abs_cost_;
  std::priority_queue<ColIndex, std::vector<ColIndex>,
                 InitialBasis::TriangularColumnComparator>
      queue(residual_singleton_column.begin(), residual_singleton_column.end(),
            triangular_column_comparator_);

  // If the the product magnitude of the diagonal coefficients become smaller
  // than a given threshold, we will assume that this method returns an instable
  // first basis. The threshold is somewhat arbitrary and is mainly here to
  // avoid an infinite inverse product which will trigger floating point
  // exceptions in other part of the code.
  const double kMinimumProductMagnitude = 1e-100;
  double partial_diagonal_product = 1.0;

  // Process the residual singleton columns by priority and add them to the
  // basis if their "diagonal" coefficient is not too small.
  while (!queue.empty()) {
    const ColIndex candidate = queue.top();
    queue.pop();
    if (residual_pattern.ColDegree(candidate) != 1) continue;

    // Find the position of the singleton and compute the infinity norm of
    // the column (note that this is always 1.0 if the problem was scaled).
    RowIndex row(kInvalidRow);
    Fractional coeff = 0.0;
    Fractional max_magnitude = 0.0;
    for (const SparseColumn::Entry e : matrix_.column(candidate)) {
      max_magnitude = std::max(max_magnitude, std::abs(e.coefficient()));
      if (can_be_replaced[e.row()]) {
        row = e.row();
        coeff = e.coefficient();
        break;
      }
    }
    const Fractional kStabilityThreshold = 0.01;
    if (std::abs(coeff) < kStabilityThreshold * max_magnitude) continue;
    DCHECK_NE(kInvalidRow, row);

    partial_diagonal_product *= coeff;
    if (std::abs(partial_diagonal_product) < kMinimumProductMagnitude) {
      LOG(INFO) << "Numerical difficulties detected. The product of the "
                << "diagonal coefficients is currently equal to "
                << partial_diagonal_product;
      break;
    }

    // Use this candidate column in the basis.
    (*basis)[row] = candidate;
    can_be_replaced[row] = false;
    residual_pattern.DeleteRowAndColumn(row, candidate);
    for (const ColIndex col : residual_pattern.RowNonZero(row)) {
      if (col == candidate) continue;
      residual_pattern.DecreaseColDegree(col);
      if (residual_pattern.ColDegree(col) == 1) {
        queue.push(col);
      }
    }
  }

  return std::abs(partial_diagonal_product) >= kMinimumProductMagnitude;
}

void InitialBasis::ComputeCandidates(ColIndex num_cols,
                                     std::vector<ColIndex>* candidates) {
  candidates->clear();
  max_scaled_abs_cost_ = 0.0;
  for (ColIndex col(0); col < num_cols; ++col) {
    if (variable_type_[col] != VariableType::FIXED_VARIABLE &&
        matrix_.column(col).num_entries() > 0) {
      candidates->push_back(col);
      max_scaled_abs_cost_ =
          std::max(max_scaled_abs_cost_, std::abs(objective_[col]));
    }
  }
  const Fractional kBixbyWeight = 1000.0;
  max_scaled_abs_cost_ =
      (max_scaled_abs_cost_ == 0.0) ? 1.0 : kBixbyWeight * max_scaled_abs_cost_;
  std::sort(candidates->begin(), candidates->end(), bixby_column_comparator_);
}

int InitialBasis::GetColumnCategory(ColIndex col) const {
  // Only the relative position of the returned number is important, so we use
  // 2 for the category C2 in Bixby's paper and so on.
  switch (variable_type_[col]) {
    case VariableType::UNCONSTRAINED:
      return 2;
    case VariableType::LOWER_BOUNDED:
      return 3;
    case VariableType::UPPER_BOUNDED:
      return 3;
    case VariableType::UPPER_AND_LOWER_BOUNDED:
      return 4;
    case VariableType::FIXED_VARIABLE:
      return 5;
    default:
      LOG(DFATAL) << "Column " << col
                  << " has no meaningful type.";
      return 6;
  }
}

Fractional InitialBasis::GetColumnPenalty(ColIndex col) const {
  const VariableType type = variable_type_[col];
  Fractional penalty = 0.0;
  if (type == VariableType::LOWER_BOUNDED) {
    penalty = lower_bound_[col];
  }
  if (type == VariableType::UPPER_BOUNDED) {
    penalty = -upper_bound_[col];
  }
  if (type == VariableType::UPPER_AND_LOWER_BOUNDED) {
    penalty = lower_bound_[col] - upper_bound_[col];
  }
  return penalty + std::abs(objective_[col]) / max_scaled_abs_cost_;
}

bool InitialBasis::BixbyColumnComparator::operator()(ColIndex col_a,
                                                     ColIndex col_b) const {
  if (col_a == col_b) return false;
  const int category_a = initial_basis_.GetColumnCategory(col_a);
  const int category_b = initial_basis_.GetColumnCategory(col_b);
  if (category_a != category_b) {
    return category_a < category_b;
  } else {
    return initial_basis_.GetColumnPenalty(col_a) <
           initial_basis_.GetColumnPenalty(col_b);
  }
}

bool InitialBasis::TriangularColumnComparator::operator()(
    ColIndex col_a, ColIndex col_b) const {
  if (col_a == col_b) return false;
  const int category_a = initial_basis_.GetColumnCategory(col_a);
  const int category_b = initial_basis_.GetColumnCategory(col_b);
  if (category_a != category_b) {
    return category_a > category_b;
  }

  // The nonzero is not in the original Bixby paper, but experiment shows it is
  // important. It leads to sparser solves, but also sparser direction, which
  // mean potentially less blocking variables on each pivot...
  //
  // TODO(user): Experiments more with this comparator or the
  // BixbyColumnComparator.
  if (initial_basis_.matrix_.column(col_a).num_entries() !=
      initial_basis_.matrix_.column(col_b).num_entries()) {
    return initial_basis_.matrix_.column(col_a).num_entries() >
           initial_basis_.matrix_.column(col_b).num_entries();
  }
  return initial_basis_.GetColumnPenalty(col_a) >
         initial_basis_.GetColumnPenalty(col_b);
}

}  // namespace glop
}  // namespace operations_research
