// Copyright 2010-2024 Google LLC
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

// Utility helpers for manipulating LinearProgram and other types defined in
// lp_data.

#ifndef OR_TOOLS_LP_DATA_LP_DATA_UTILS_H_
#define OR_TOOLS_LP_DATA_LP_DATA_UTILS_H_

#include "ortools/glop/parameters.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/matrix_scaler.h"
#include "ortools/lp_data/scattered_vector.h"

namespace operations_research {
namespace glop {

// For all constraints in linear_program, if the constraint has a slack
// variable, change its value in *values so that the constraints itself is
// satisfied.
// Note that this obviously won't always imply that the bounds of the slack
// variable itself will be satisfied.
// The code assumes (and DCHECKs) that all constraints with a slack variable
// have their upper and lower bounds both set to 0. This is ensured by
// LinearProgram::AddSlackVariablesWhereNecessary().
void ComputeSlackVariablesValues(const LinearProgram& linear_program,
                                 DenseRow* values);

// This is separated from LinearProgram class because of a cyclic dependency
// when scaling as an LP.
void Scale(LinearProgram* lp, SparseMatrixScaler* scaler,
           GlopParameters::ScalingAlgorithm scaling_method);

// A convenience method for above providing a default algorithm for callers that
// don't specify one.
void Scale(LinearProgram* lp, SparseMatrixScaler* scaler);

// Class to facilitate the conversion between an original "unscaled" LP problem
// and its scaled version. It is easy to get the direction wrong, so it make
// sense to have a single place where all the scaling formulas are kept.
class LpScalingHelper {
 public:
  // Clear all scaling coefficients.
  void Clear();

  // Scale the given LP.
  void Scale(LinearProgram* lp);
  void Scale(const GlopParameters& params, LinearProgram* lp);
  void ConfigureFromFactors(absl::Span<const double> row_factors,
                            absl::Span<const double> col_factors);

  // Transforms value from unscaled domain to the scaled one.
  Fractional ScaleVariableValue(ColIndex col, Fractional value) const;
  Fractional ScaleReducedCost(ColIndex col, Fractional value) const;
  Fractional ScaleDualValue(RowIndex row, Fractional value) const;
  Fractional ScaleConstraintActivity(RowIndex row, Fractional value) const;

  // Transforms corresponding value from the scaled domain to the original one.
  Fractional UnscaleVariableValue(ColIndex col, Fractional value) const;
  Fractional UnscaleReducedCost(ColIndex col, Fractional value) const;
  Fractional UnscaleDualValue(RowIndex row, Fractional value) const;
  Fractional UnscaleLeftSolveValue(RowIndex row, Fractional value) const;
  Fractional UnscaleConstraintActivity(RowIndex row, Fractional value) const;

  // Unscale a row vector v such that v.B = unit_row. When basis_col is the
  // index of the Column that correspond to the unit position in matrix B.
  void UnscaleUnitRowLeftSolve(ColIndex basis_col,
                               ScatteredRow* left_inverse) const;

  // Unscale a col vector v such that B.c = matrix_column_col.
  void UnscaleColumnRightSolve(const RowToColMapping& basis, ColIndex col,
                               ScatteredColumn* right_inverse) const;

  // A variable value in the original domain must be multiplied by this factor
  // to be in the scaled domain.
  Fractional VariableScalingFactor(ColIndex col) const;

  // Same as VariableScalingFactor() except that ColIndex greater than the
  // number of columns will be interpreted as "slack" variable whose scaling
  // factor depends on the row.
  Fractional VariableScalingFactorWithSlack(ColIndex col) const;

  // Extra scaling function, to scale objective/bounds.
  void AverageCostScaling(DenseRow* objective);
  void ContainOneBoundScaling(DenseRow* upper_bounds, DenseRow* lower_bounds);

  // Visible for testing. All variable/constraint bounds of the original LP
  // where multiplied by this factor. Nothing else changed.
  Fractional BoundsScalingFactor() const { return bound_scaling_factor_; }

  // Visible for testing. All objective coefficients of the original LP where
  // multiplied by this factor. Nothing else changed.
  Fractional ObjectiveScalingFactor() const {
    return objective_scaling_factor_;
  }

 private:
  Fractional RowUnscalingFactor(RowIndex row) const {
    return matrix_is_scaled_ ? row_unscaling_factors_[row] : 1.0;
  }
  Fractional ColUnscalingFactor(ColIndex col) const {
    return matrix_is_scaled_ ? col_unscaling_factors_[col] : 1.0;
  }

  bool matrix_is_scaled_ = false;
  DenseColumn row_unscaling_factors_;
  DenseRow col_unscaling_factors_;

  Fractional bound_scaling_factor_ = 1.0;
  Fractional objective_scaling_factor_ = 1.0;
};

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_LP_DATA_LP_DATA_UTILS_H_
