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

#include "ortools/glop/variable_values.h"

#include "ortools/lp_data/lp_utils.h"
#include "ortools/util/iterators.h"

namespace operations_research {
namespace glop {

VariableValues::VariableValues(const CompactSparseMatrix& matrix,
                               const RowToColMapping& basis,
                               const VariablesInfo& variables_info,
                               const BasisFactorization& basis_factorization)
    : matrix_(matrix),
      basis_(basis),
      variables_info_(variables_info),
      basis_factorization_(basis_factorization),
      stats_("VariableValues") {}

void VariableValues::SetNonBasicVariableValueFromStatus(ColIndex col) {
  SCOPED_TIME_STAT(&stats_);
  const DenseRow& lower_bounds = variables_info_.GetVariableLowerBounds();
  const DenseRow& upper_bounds = variables_info_.GetVariableUpperBounds();
  variable_values_.resize(matrix_.num_cols(), 0.0);
  switch (variables_info_.GetStatusRow()[col]) {
    case VariableStatus::FIXED_VALUE:
      DCHECK_NE(-kInfinity, lower_bounds[col]);
      DCHECK_EQ(lower_bounds[col], upper_bounds[col]);
      variable_values_[col] = lower_bounds[col];
      break;
    case VariableStatus::AT_LOWER_BOUND:
      DCHECK_NE(-kInfinity, lower_bounds[col]);
      variable_values_[col] = lower_bounds[col];
      break;
    case VariableStatus::AT_UPPER_BOUND:
      DCHECK_NE(kInfinity, upper_bounds[col]);
      variable_values_[col] = upper_bounds[col];
      break;
    case VariableStatus::FREE:
      DCHECK_EQ(-kInfinity, lower_bounds[col]);
      DCHECK_EQ(kInfinity, upper_bounds[col]);
      variable_values_[col] = 0.0;
      break;
    case VariableStatus::BASIC:
      LOG(DFATAL) << "SetNonBasicVariableValueFromStatus() shouldn't "
                  << "be called on a BASIC variable.";
      break;
  }
  // Note that there is no default value in the switch() statement above to
  // get a compile-time error if a value is missing.
}

void VariableValues::RecomputeBasicVariableValues() {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(basis_factorization_.IsRefactorized());
  const RowIndex num_rows = matrix_.num_rows();
  scratchpad_.assign(num_rows, 0.0);
  for (const ColIndex col : variables_info_.GetNotBasicBitRow()) {
    const Fractional value = variable_values_[col];
    matrix_.ColumnAddMultipleToDenseColumn(col, -value, &scratchpad_);
  }
  basis_factorization_.RightSolve(&scratchpad_);
  for (RowIndex row(0); row < num_rows; ++row) {
    variable_values_[basis_[row]] = scratchpad_[row];
  }
}

Fractional VariableValues::ComputeMaximumPrimalResidual() const {
  SCOPED_TIME_STAT(&stats_);
  scratchpad_.assign(matrix_.num_rows(), 0.0);
  const ColIndex num_cols = matrix_.num_cols();
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional value = variable_values_[col];
    matrix_.ColumnAddMultipleToDenseColumn(col, value, &scratchpad_);
  }
  return InfinityNorm(scratchpad_);
}

Fractional VariableValues::ComputeMaximumPrimalInfeasibility() const {
  SCOPED_TIME_STAT(&stats_);
  const DenseRow& lower_bounds = variables_info_.GetVariableLowerBounds();
  const DenseRow& upper_bounds = variables_info_.GetVariableUpperBounds();
  Fractional primal_infeasibility = 0.0;
  const ColIndex num_cols = matrix_.num_cols();
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional value = variable_values_[col];
    primal_infeasibility = std::max(
        primal_infeasibility,
        std::max(lower_bounds[col] - value, value - upper_bounds[col]));
  }
  return primal_infeasibility;
}

Fractional VariableValues::ComputeSumOfPrimalInfeasibilities() const {
  SCOPED_TIME_STAT(&stats_);
  const DenseRow& lower_bounds = variables_info_.GetVariableLowerBounds();
  const DenseRow& upper_bounds = variables_info_.GetVariableUpperBounds();
  Fractional sum = 0.0;
  const ColIndex num_cols = matrix_.num_cols();
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional value = variable_values_[col];
    sum += std::max(
        0.0, std::max(lower_bounds[col] - value, value - upper_bounds[col]));
  }
  return sum;
}

void VariableValues::UpdateOnPivoting(ScatteredColumnReference direction,
                                      ColIndex entering_col, Fractional step) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(IsFinite(step));

  // Note(user): Some positions are ignored during the primal ratio test:
  // - The rows for which direction_[row] < tolerance.
  // - The non-zeros of direction_ignored_position_ in case of degeneracy.
  // Such positions may result in basic variables going out of their bounds by
  // more than the allowed tolerance. We could choose not to update theses
  // variables or not make them take out-of-bound values, but this would
  // introduce artificial errors.

  // Note that there is no need to call variables_info_.Update() on basic
  // variables when they change values. Note also that the status of
  // entering_col will be updated later.
  for (const RowIndex row : direction.non_zero_rows) {
    const ColIndex col = basis_[row];
    variable_values_[col] -= direction[row] * step;
  }
  variable_values_[entering_col] += step;
}

void VariableValues::UpdateGivenNonBasicVariables(
    const std::vector<ColIndex>& cols_to_update, bool update_basic_variables) {
  SCOPED_TIME_STAT(&stats_);
  if (update_basic_variables) {
    initially_all_zero_scratchpad_.resize(matrix_.num_rows(), 0.0);
    DCHECK(IsAllZero(initially_all_zero_scratchpad_));
    for (ColIndex col : cols_to_update) {
      const Fractional old_value = variable_values_[col];
      SetNonBasicVariableValueFromStatus(col);
      matrix_.ColumnAddMultipleToDenseColumn(col,
                                             variable_values_[col] - old_value,
                                             &initially_all_zero_scratchpad_);
    }
    basis_factorization_.RightSolveWithNonZeros(&initially_all_zero_scratchpad_,
                                                &row_index_vector_scratchpad_);
    for (const RowIndex row : row_index_vector_scratchpad_) {
      variable_values_[basis_[row]] -= initially_all_zero_scratchpad_[row];
      initially_all_zero_scratchpad_[row] = 0.0;
    }
    UpdatePrimalInfeasibilityInformation(row_index_vector_scratchpad_);
  } else {
    for (ColIndex col : cols_to_update) {
      SetNonBasicVariableValueFromStatus(col);
    }
  }
}

const DenseColumn& VariableValues::GetPrimalSquaredInfeasibilities() const {
  return primal_squared_infeasibilities_;
}

const DenseBitColumn& VariableValues::GetPrimalInfeasiblePositions() const {
  return primal_infeasible_positions_;
}

void VariableValues::ResetPrimalInfeasibilityInformation() {
  SCOPED_TIME_STAT(&stats_);
  const RowIndex num_rows = matrix_.num_rows();
  primal_squared_infeasibilities_.resize(num_rows, 0.0);
  primal_infeasible_positions_.ClearAndResize(num_rows);
  UpdatePrimalInfeasibilities(IntegerRange<RowIndex>(RowIndex(0), num_rows));
}

void VariableValues::UpdatePrimalInfeasibilityInformation(
    const std::vector<RowIndex>& rows) {
  if (primal_squared_infeasibilities_.size() != matrix_.num_rows()) {
    ResetPrimalInfeasibilityInformation();
  } else {
    SCOPED_TIME_STAT(&stats_);
    UpdatePrimalInfeasibilities(rows);
  }
}

template <typename Rows>
void VariableValues::UpdatePrimalInfeasibilities(const Rows& rows) {
  const DenseRow& lower_bounds = variables_info_.GetVariableLowerBounds();
  const DenseRow& upper_bounds = variables_info_.GetVariableUpperBounds();
  for (const RowIndex row : rows) {
    const ColIndex col = basis_[row];
    const Fractional value = variable_values_[col];
    const Fractional magnitude =
        std::max(value - upper_bounds[col], lower_bounds[col] - value);
    if (magnitude > tolerance_) {
      primal_squared_infeasibilities_[row] = Square(magnitude);
      primal_infeasible_positions_.Set(row);
    } else {
      primal_infeasible_positions_.Clear(row);
    }
  }
}

}  // namespace glop
}  // namespace operations_research
