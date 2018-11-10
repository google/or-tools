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

}  // namespace glop
}  // namespace operations_research
