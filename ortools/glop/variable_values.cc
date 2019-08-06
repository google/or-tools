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

#include "ortools/glop/variable_values.h"

#include "ortools/graph/iterators.h"
#include "ortools/lp_data/lp_utils.h"

namespace operations_research {
namespace glop {

VariableValues::VariableValues(const GlopParameters& parameters,
                               const CompactSparseMatrix& matrix,
                               const RowToColMapping& basis,
                               const VariablesInfo& variables_info,
                               const BasisFactorization& basis_factorization)
    : parameters_(parameters),
      matrix_(matrix),
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

void VariableValues::ResetAllNonBasicVariableValues() {
  const DenseRow& lower_bounds = variables_info_.GetVariableLowerBounds();
  const DenseRow& upper_bounds = variables_info_.GetVariableUpperBounds();
  const VariableStatusRow& statuses = variables_info_.GetStatusRow();
  const ColIndex num_cols = matrix_.num_cols();
  variable_values_.resize(num_cols, 0.0);
  for (ColIndex col(0); col < num_cols; ++col) {
    switch (statuses[col]) {
      case VariableStatus::FIXED_VALUE:
        ABSL_FALLTHROUGH_INTENDED;
      case VariableStatus::AT_LOWER_BOUND:
        variable_values_[col] = lower_bounds[col];
        break;
      case VariableStatus::AT_UPPER_BOUND:
        variable_values_[col] = upper_bounds[col];
        break;
      case VariableStatus::FREE:
        variable_values_[col] = 0.0;
        break;
      case VariableStatus::BASIC:
        break;
    }
  }
}

void VariableValues::RecomputeBasicVariableValues() {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(basis_factorization_.IsRefactorized());
  const RowIndex num_rows = matrix_.num_rows();
  scratchpad_.non_zeros.clear();
  scratchpad_.values.AssignToZero(num_rows);
  for (const ColIndex col : variables_info_.GetNotBasicBitRow()) {
    const Fractional value = variable_values_[col];
    matrix_.ColumnAddMultipleToDenseColumn(col, -value, &scratchpad_.values);
  }
  basis_factorization_.RightSolve(&scratchpad_);
  for (RowIndex row(0); row < num_rows; ++row) {
    variable_values_[basis_[row]] = scratchpad_[row];
  }
}

Fractional VariableValues::ComputeMaximumPrimalResidual() const {
  SCOPED_TIME_STAT(&stats_);
  scratchpad_.non_zeros.clear();
  scratchpad_.values.AssignToZero(matrix_.num_rows());
  const ColIndex num_cols = matrix_.num_cols();
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional value = variable_values_[col];
    matrix_.ColumnAddMultipleToDenseColumn(col, value, &scratchpad_.values);
  }
  return InfinityNorm(scratchpad_.values);
}

Fractional VariableValues::ComputeMaximumPrimalInfeasibility() const {
  SCOPED_TIME_STAT(&stats_);
  Fractional primal_infeasibility = 0.0;
  const ColIndex num_cols = matrix_.num_cols();
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional col_infeasibility = std::max(
        GetUpperBoundInfeasibility(col), GetLowerBoundInfeasibility(col));
    primal_infeasibility = std::max(primal_infeasibility, col_infeasibility);
  }
  return primal_infeasibility;
}

Fractional VariableValues::ComputeSumOfPrimalInfeasibilities() const {
  SCOPED_TIME_STAT(&stats_);
  Fractional sum = 0.0;
  const ColIndex num_cols = matrix_.num_cols();
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional col_infeasibility = std::max(
        GetUpperBoundInfeasibility(col), GetLowerBoundInfeasibility(col));
    sum += std::max(0.0, col_infeasibility);
  }
  return sum;
}

void VariableValues::UpdateOnPivoting(const ScatteredColumn& direction,
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
  for (const auto e : direction) {
    const ColIndex col = basis_[e.row()];
    variable_values_[col] -= e.coefficient() * step;
  }
  variable_values_[entering_col] += step;
}

void VariableValues::UpdateGivenNonBasicVariables(
    const std::vector<ColIndex>& cols_to_update, bool update_basic_variables) {
  SCOPED_TIME_STAT(&stats_);
  if (update_basic_variables) {
    const RowIndex num_rows = matrix_.num_rows();
    initially_all_zero_scratchpad_.values.resize(num_rows, 0.0);
    initially_all_zero_scratchpad_.is_non_zero.resize(num_rows, false);
    DCHECK(IsAllZero(initially_all_zero_scratchpad_.values));
    DCHECK(IsAllFalse(initially_all_zero_scratchpad_.is_non_zero));

    // TODO(user): Abort the non-zeros computation earlier if dense.
    for (ColIndex col : cols_to_update) {
      const Fractional old_value = variable_values_[col];
      SetNonBasicVariableValueFromStatus(col);
      matrix_.ColumnAddMultipleToSparseScatteredColumn(
          col, variable_values_[col] - old_value,
          &initially_all_zero_scratchpad_);
    }
    if (initially_all_zero_scratchpad_.ShouldUseDenseIteration()) {
      initially_all_zero_scratchpad_.non_zeros.clear();
      initially_all_zero_scratchpad_.is_non_zero.assign(num_rows, false);
    } else {
      for (const RowIndex row : initially_all_zero_scratchpad_.non_zeros) {
        initially_all_zero_scratchpad_.is_non_zero[row] = false;
      }
    }

    basis_factorization_.RightSolve(&initially_all_zero_scratchpad_);
    if (initially_all_zero_scratchpad_.non_zeros.empty()) {
      for (RowIndex row(0); row < num_rows; ++row) {
        variable_values_[basis_[row]] -= initially_all_zero_scratchpad_[row];
      }
      initially_all_zero_scratchpad_.values.AssignToZero(num_rows);
      ResetPrimalInfeasibilityInformation();
    } else {
      for (const auto e : initially_all_zero_scratchpad_) {
        variable_values_[basis_[e.row()]] -= e.coefficient();
        initially_all_zero_scratchpad_[e.row()] = 0.0;
      }
      UpdatePrimalInfeasibilityInformation(
          initially_all_zero_scratchpad_.non_zeros);
      initially_all_zero_scratchpad_.non_zeros.clear();
    }
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

  const Fractional tolerance = parameters_.primal_feasibility_tolerance();
  for (RowIndex row(0); row < num_rows; ++row) {
    const ColIndex col = basis_[row];
    const Fractional infeasibility = std::max(GetUpperBoundInfeasibility(col),
                                              GetLowerBoundInfeasibility(col));
    if (infeasibility > tolerance) {
      primal_squared_infeasibilities_[row] = Square(infeasibility);
      primal_infeasible_positions_.Set(row);
    }
  }
}

void VariableValues::UpdatePrimalInfeasibilityInformation(
    const std::vector<RowIndex>& rows) {
  if (primal_squared_infeasibilities_.size() != matrix_.num_rows()) {
    ResetPrimalInfeasibilityInformation();
    return;
  }

  // Note(user): this is the same as the code in
  // ResetPrimalInfeasibilityInformation(), but we do need the clear part.
  SCOPED_TIME_STAT(&stats_);
  const Fractional tolerance = parameters_.primal_feasibility_tolerance();
  for (const RowIndex row : rows) {
    const ColIndex col = basis_[row];
    const Fractional infeasibility = std::max(GetUpperBoundInfeasibility(col),
                                              GetLowerBoundInfeasibility(col));
    if (infeasibility > tolerance) {
      primal_squared_infeasibilities_[row] = Square(infeasibility);
      primal_infeasible_positions_.Set(row);
    } else {
      primal_infeasible_positions_.Clear(row);
    }
  }
}

}  // namespace glop
}  // namespace operations_research
