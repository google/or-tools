// Copyright 2010-2021 Google LLC
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

#include "ortools/lp_data/lp_data_utils.h"

namespace operations_research {
namespace glop {

void ComputeSlackVariablesValues(const LinearProgram& linear_program,
                                 DenseRow* values) {
  DCHECK(values);
  DCHECK_EQ(linear_program.num_variables(), values->size());

  // If there are no slack variable, we can give up.
  if (linear_program.GetFirstSlackVariable() == kInvalidCol) return;

  const auto& transposed_matrix = linear_program.GetTransposeSparseMatrix();
  for (RowIndex row(0); row < linear_program.num_constraints(); row++) {
    const ColIndex slack_variable = linear_program.GetSlackVariable(row);

    if (slack_variable == kInvalidCol) continue;

    DCHECK_EQ(0.0, linear_program.constraint_lower_bounds()[row]);
    DCHECK_EQ(0.0, linear_program.constraint_upper_bounds()[row]);

    const RowIndex transposed_slack = ColToRowIndex(slack_variable);
    Fractional activation = 0.0;
    // Row in the initial matrix (column in the transposed).
    const SparseColumn& sparse_row =
        transposed_matrix.column(RowToColIndex(row));
    for (const auto& entry : sparse_row) {
      if (transposed_slack == entry.index()) continue;
      activation +=
          (*values)[RowToColIndex(entry.index())] * entry.coefficient();
    }
    (*values)[slack_variable] = -activation;
  }
}

// This is separated from the LinearProgram class because of a cyclic dependency
// when scaling as an LP.
void Scale(LinearProgram* lp, SparseMatrixScaler* scaler) {
  // Create GlopParameters proto to get default scaling algorithm.
  GlopParameters params;
  Scale(lp, scaler, params.scaling_method());
}

// This is separated from LinearProgram class because of a cyclic dependency
// when scaling as an LP.
void Scale(LinearProgram* lp, SparseMatrixScaler* scaler,
           GlopParameters::ScalingAlgorithm scaling_method) {
  scaler->Init(&lp->matrix_);
  scaler->Scale(
      scaling_method);  // Compute R and C, and replace the matrix A by R.A.C
  scaler->ScaleRowVector(false,
                         &lp->objective_coefficients_);  // oc = oc.C
  scaler->ScaleRowVector(true,
                         &lp->variable_upper_bounds_);  // cl = cl.C^-1
  scaler->ScaleRowVector(true,
                         &lp->variable_lower_bounds_);  // cu = cu.C^-1
  scaler->ScaleColumnVector(false, &lp->constraint_upper_bounds_);  // rl = R.rl
  scaler->ScaleColumnVector(false, &lp->constraint_lower_bounds_);  // ru = R.ru
  lp->transpose_matrix_is_consistent_ = false;
}

void LpScalingHelper::Scale(LinearProgram* lp) { Scale(GlopParameters(), lp); }

void LpScalingHelper::Scale(const GlopParameters& params, LinearProgram* lp) {
  scaler_.Clear();
  ::operations_research::glop::Scale(lp, &scaler_, params.scaling_method());
  bound_scaling_factor_ = 1.0 / lp->ScaleBounds();
  objective_scaling_factor_ = 1.0 / lp->ScaleObjective(params.cost_scaling());
}

void LpScalingHelper::Clear() {
  scaler_.Clear();
  bound_scaling_factor_ = 1.0;
  objective_scaling_factor_ = 1.0;
}

Fractional LpScalingHelper::VariableScalingFactor(ColIndex col) const {
  // During scaling a col was multiplied by ColScalingFactor() and the variable
  // bounds divided by it.
  return scaler_.ColUnscalingFactor(col) * bound_scaling_factor_;
}

Fractional LpScalingHelper::ScaleVariableValue(ColIndex col,
                                               Fractional value) const {
  return value * scaler_.ColUnscalingFactor(col) * bound_scaling_factor_;
}

Fractional LpScalingHelper::ScaleReducedCost(ColIndex col,
                                             Fractional value) const {
  // The reduced cost move like the objective and the col scale.
  return value / scaler_.ColUnscalingFactor(col) * objective_scaling_factor_;
}

Fractional LpScalingHelper::ScaleDualValue(RowIndex row,
                                           Fractional value) const {
  // The dual value move like the objective and the inverse of the row scale.
  return value * (scaler_.RowUnscalingFactor(row) * objective_scaling_factor_);
}

Fractional LpScalingHelper::ScaleConstraintActivity(RowIndex row,
                                                    Fractional value) const {
  // The activity move with the row_scale and the bound_scaling_factor.
  return value / scaler_.RowUnscalingFactor(row) * bound_scaling_factor_;
}

Fractional LpScalingHelper::UnscaleVariableValue(ColIndex col,
                                                 Fractional value) const {
  // Just the opposite of ScaleVariableValue().
  return value / (scaler_.ColUnscalingFactor(col) * bound_scaling_factor_);
}

Fractional LpScalingHelper::UnscaleReducedCost(ColIndex col,
                                               Fractional value) const {
  // The reduced cost move like the objective and the col scale.
  return value * scaler_.ColUnscalingFactor(col) / objective_scaling_factor_;
}

Fractional LpScalingHelper::UnscaleDualValue(RowIndex row,
                                             Fractional value) const {
  // The dual value move like the objective and the inverse of the row scale.
  return value / (scaler_.RowUnscalingFactor(row) * objective_scaling_factor_);
}

Fractional LpScalingHelper::UnscaleConstraintActivity(RowIndex row,
                                                      Fractional value) const {
  // The activity move with the row_scale and the bound_scaling_factor.
  return value * scaler_.RowUnscalingFactor(row) / bound_scaling_factor_;
}

void LpScalingHelper::UnscaleUnitRowLeftSolve(
    ColIndex basis_col, ScatteredRow* left_inverse) const {
  const Fractional global_factor = scaler_.ColUnscalingFactor(basis_col);

  // We have left_inverse * [RowScale * B * ColScale] = unit_row.
  if (left_inverse->non_zeros.empty()) {
    const ColIndex num_rows = left_inverse->values.size();
    for (ColIndex col(0); col < num_rows; ++col) {
      left_inverse->values[col] /=
          scaler_.RowUnscalingFactor(ColToRowIndex(col)) * global_factor;
    }
  } else {
    for (const ColIndex col : left_inverse->non_zeros) {
      left_inverse->values[col] /=
          scaler_.RowUnscalingFactor(ColToRowIndex(col)) * global_factor;
    }
  }
}

void LpScalingHelper::UnscaleColumnRightSolve(
    const RowToColMapping& basis, ColIndex col,
    ScatteredColumn* right_inverse) const {
  const Fractional global_factor = scaler_.ColScalingFactor(col);

  // [RowScale * B * BColScale] * inverse = RowScale * column * ColScale.
  // That is B * (BColScale * inverse) = columm * ColScale[col].
  if (right_inverse->non_zeros.empty()) {
    const RowIndex num_rows = right_inverse->values.size();
    for (RowIndex row(0); row < num_rows; ++row) {
      right_inverse->values[row] /=
          scaler_.ColUnscalingFactor(basis[row]) * global_factor;
    }
  } else {
    for (const RowIndex row : right_inverse->non_zeros) {
      right_inverse->values[row] /=
          scaler_.ColUnscalingFactor(basis[row]) * global_factor;
    }
  }
}

}  // namespace glop
}  // namespace operations_research
