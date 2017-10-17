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

#ifndef OR_TOOLS_GLOP_VARIABLE_VALUES_H_
#define OR_TOOLS_GLOP_VARIABLE_VALUES_H_

#include "ortools/glop/basis_representation.h"
#include "ortools/glop/variables_info.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/util/stats.h"

namespace operations_research {
namespace glop {

// Class holding all the variable values and responsible for updating them. The
// variable values 'x' are such that 'A.x = 0' where A is the linear program
// matrix. This is because slack variables with bounds corresponding to the
// constraints bounds were added to the linear program matrix A.
//
// Some remarks:
// - For convenience, the variable values are stored in a DenseRow and indexed
//   by ColIndex, like the variables and the columns of A.
// - During the dual-simplex, all non-basic variable values are at their exact
//   bounds or exactly at 0.0 for a free variable.
// - During the primal-simplex, the non-basic variable values may not be exactly
//   at their bounds because of bound-shifting during degenerate simplex
//   pivoting which is implemented by not setting the variable values exactly at
//   their bounds to have a lower primal residual error.
class VariableValues {
 public:
  VariableValues(const CompactSparseMatrix& matrix,
                 const RowToColMapping& basis,
                 const VariablesInfo& variables_info,
                 const BasisFactorization& basis_factorization);

  // Getters for the variable values.
  const Fractional Get(ColIndex col) const { return variable_values_[col]; }
  const DenseRow& GetDenseRow() const { return variable_values_; }

  // Sets the value of a non-basic variable to the exact value implied by its
  // current status. Note that the basic variable values are NOT updated by this
  // function and it is up to the client to call RecomputeBasicVariableValues().
  void SetNonBasicVariableValueFromStatus(ColIndex col);

  // Recomputes the value of the basic variables from the non-basic ones knowing
  // that the linear program matrix A times the variable values vector must be
  // zero. It is better to call this when the basis is refactorized. This
  // is checked in debug mode.
  void RecomputeBasicVariableValues();

  // Computes the infinity norm of A.x where A is the linear_program matrix and
  // x is the variable values column.
  Fractional ComputeMaximumPrimalResidual() const;

  // Computes the maximum bound error for all the variables, defined as the
  // distance of the current value of the variable to its interval
  // [lower bound, upper bound]. The residual is thus equal to 0.0 if the
  // current value falls within the bounds, to the distance to lower_bound
  // (resp. upper_bound), if the current value is below (resp. above)
  // lower_bound (resp. upper_bound).
  Fractional ComputeMaximumPrimalInfeasibility() const;
  Fractional ComputeSumOfPrimalInfeasibilities() const;

  // Updates the variable during a simplex pivot:
  // - step * direction is substracted from the basic variables value.
  // - step is added to the entering column value.
  void UpdateOnPivoting(ScatteredColumnReference direction,
                        ColIndex entering_col, Fractional step);

  // Batch version of SetNonBasicVariableValueFromStatus(). This function also
  // updates the basic variable values and infeasibility statuses if
  // update_basic_variables is true. The update is done in an incremental way
  // and is thus more efficient than calling afterwards
  // RecomputeBasicVariableValues() and ResetPrimalInfeasibilityInformation().
  void UpdateGivenNonBasicVariables(const std::vector<ColIndex>& cols_to_update,
                                    bool update_basic_variables);

  // Functions dealing with the primal-infeasible basic variables. A basic
  // variable is primal-infeasible if its value is outside its bounds
  // (modulo the absolute tolerance set by SetBoundTolerance()). This
  // information is only available after a call to
  // ResetPrimalInfeasibilityInformation() and has to be kept in sync by calling
  // UpdatePrimalInfeasibilityInformation() for the rows that changed values.
  void SetBoundTolerance(Fractional tolerance) { tolerance_ = tolerance; }
  const DenseBitColumn& GetPrimalInfeasiblePositions() const;
  const DenseColumn& GetPrimalSquaredInfeasibilities() const;
  void ResetPrimalInfeasibilityInformation();
  void UpdatePrimalInfeasibilityInformation(const std::vector<RowIndex>& rows);

  // Sets the variable value of a given column.
  void Set(ColIndex col, Fractional value) { variable_values_[col] = value; }

  // Parameters and stats functions.
  std::string StatString() const { return stats_.StatString(); }

 private:
  // Internal version of UpdatePrimalInfeasibilityInformation().
  template <typename Rows>
  void UpdatePrimalInfeasibilities(const Rows& rows);

  // Input problem data.
  const CompactSparseMatrix& matrix_;
  const RowToColMapping& basis_;
  const VariablesInfo& variables_info_;
  const BasisFactorization& basis_factorization_;

  // Values of the variables.
  DenseRow variable_values_;

  // Members used for the basic primal-infeasible variables.
  Fractional tolerance_;
  DenseColumn primal_squared_infeasibilities_;
  DenseBitColumn primal_infeasible_positions_;

  mutable StatsGroup stats_;
  mutable DenseColumn scratchpad_;

  // A temporary dense column that is always reset to all zero after use.
  DenseColumn initially_all_zero_scratchpad_;

  // A temporary RowIndexVector used to hold the non-zero positions of a column.
  RowIndexVector row_index_vector_scratchpad_;

  DISALLOW_COPY_AND_ASSIGN(VariableValues);
};

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_GLOP_VARIABLE_VALUES_H_
