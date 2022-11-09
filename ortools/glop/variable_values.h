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

#ifndef OR_TOOLS_GLOP_VARIABLE_VALUES_H_
#define OR_TOOLS_GLOP_VARIABLE_VALUES_H_

#include <string>
#include <vector>

#include "ortools/glop/basis_representation.h"
#include "ortools/glop/dual_edge_norms.h"
#include "ortools/glop/pricing.h"
#include "ortools/glop/variables_info.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/scattered_vector.h"
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
  VariableValues(const GlopParameters& parameters,
                 const CompactSparseMatrix& matrix,
                 const RowToColMapping& basis,
                 const VariablesInfo& variables_info,
                 const BasisFactorization& basis_factorization,
                 DualEdgeNorms* dual_edge_norms,
                 DynamicMaximum<RowIndex>* dual_prices);

  // Getters for the variable values.
  const Fractional Get(ColIndex col) const { return variable_values_[col]; }
  const DenseRow& GetDenseRow() const { return variable_values_; }

  // Sets the value of a non-basic variable to the exact value implied by its
  // current status. Note that the basic variable values are NOT updated by this
  // function and it is up to the client to call RecomputeBasicVariableValues().
  void SetNonBasicVariableValueFromStatus(ColIndex col);

  // Calls SetNonBasicVariableValueFromStatus() on all non-basic variables. We
  // accept any size for free_initial_values, for columns col that are valid
  // indices, free_initial_values[col] will be used instead of 0.0 for a free
  // column. If free_initial_values is empty, then we have the default behavior
  // of starting at zero for all FREE variables.
  //
  // Note(user): It is okay to always use the same value to reset a FREE
  // variable because as soon as a FREE variable value is modified, this
  // variable shouldn't be FREE anymore. It will either move to a bound or enter
  // the basis, these are the only options.
  void ResetAllNonBasicVariableValues(const DenseRow& free_initial_values);

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
  // [lower bound, upper bound]. The infeasibility is thus equal to 0.0 if the
  // current value falls within the bounds, to the distance to lower_bound
  // (resp. upper_bound), if the current value is below (resp. above)
  // lower_bound (resp. upper_bound).
  Fractional ComputeMaximumPrimalInfeasibility() const;
  Fractional ComputeSumOfPrimalInfeasibilities() const;

  // Updates the variable during a simplex pivot:
  // - step * direction is substracted from the basic variables value.
  // - step is added to the entering column value.
  void UpdateOnPivoting(const ScatteredColumn& direction, ColIndex entering_col,
                        Fractional step);

  // Batch version of SetNonBasicVariableValueFromStatus(). This function also
  // updates the basic variable values and infeasibility statuses if
  // update_basic_variables is true. The update is done in an incremental way
  // and is thus more efficient than calling afterwards
  // RecomputeBasicVariableValues() and RecomputeDualPrices().
  void UpdateGivenNonBasicVariables(const std::vector<ColIndex>& cols_to_update,
                                    bool update_basic_variables);

  // Functions dealing with the primal-infeasible basic variables. A basic
  // variable is primal-infeasible if its infeasibility is stricly greater than
  // the primal feasibility tolerance. These are exactly the dual "prices" once
  // recalled by the norms. This is only used during the dual simplex.
  //
  // This information is only available after a call to RecomputeDualPrices()
  // and has to be kept in sync by calling UpdateDualPrices() for the rows that
  // changed values.
  //
  // TODO(user): On some problem like stp3d.mps or pds-100.mps, using different
  // price like abs(infeasibility) / squared_norms give better result. Some
  // solver switch according to a criteria like all entry are +1/-1, the column
  // have no more than 24 non-zero and the average column size is no more than
  // 6! Understand and implement some variant of this? I think the gain is
  // mainly because of using sparser vectors?
  void RecomputeDualPrices(bool put_more_importance_on_norm = false);
  void UpdateDualPrices(absl::Span<const RowIndex> row);

  // The primal phase I objective is related to the primal infeasible
  // information above. The cost of a basic column will be 1 if the variable is
  // above its upper bound by strictly more than the primal tolerance, and -1 if
  // it is lower than its lower bound by strictly less than the same tolerance.
  //
  // Returns true iff some cost changed.
  template <typename Rows>
  bool UpdatePrimalPhaseICosts(const Rows& rows, DenseRow* objective);

  // Sets the variable value of a given column.
  void Set(ColIndex col, Fractional value) { variable_values_[col] = value; }

  // Parameters and stats functions.
  std::string StatString() const { return stats_.StatString(); }

 private:
  // It is important that the infeasibility is always computed in the same
  // way. So the code should always use these functions that returns a positive
  // value when the variable is out of bounds.
  Fractional GetUpperBoundInfeasibility(ColIndex col) const {
    return variable_values_[col] -
           variables_info_.GetVariableUpperBounds()[col];
  }
  Fractional GetLowerBoundInfeasibility(ColIndex col) const {
    return variables_info_.GetVariableLowerBounds()[col] -
           variable_values_[col];
  }

  // Input problem data.
  const GlopParameters& parameters_;
  const CompactSparseMatrix& matrix_;
  const RowToColMapping& basis_;
  const VariablesInfo& variables_info_;
  const BasisFactorization& basis_factorization_;

  // This is set by RecomputeDualPrices() so that UpdateDualPrices() use
  // the same formula.
  bool put_more_importance_on_norm_ = false;

  // The dual prices are a normalized version of the primal infeasibility.
  DualEdgeNorms* dual_edge_norms_;
  DynamicMaximum<RowIndex>* dual_prices_;

  // Values of the variables.
  DenseRow variable_values_;

  mutable StatsGroup stats_;
  mutable ScatteredColumn scratchpad_;

  // A temporary scattered column that is always reset to all zero after use.
  ScatteredColumn initially_all_zero_scratchpad_;

  DISALLOW_COPY_AND_ASSIGN(VariableValues);
};

template <typename Rows>
bool VariableValues::UpdatePrimalPhaseICosts(const Rows& rows,
                                             DenseRow* objective) {
  SCOPED_TIME_STAT(&stats_);
  bool changed = false;
  const Fractional tolerance = parameters_.primal_feasibility_tolerance();
  for (const RowIndex row : rows) {
    const ColIndex col = basis_[row];
    Fractional new_cost = 0.0;
    if (GetUpperBoundInfeasibility(col) > tolerance) {
      new_cost = 1.0;
    } else if (GetLowerBoundInfeasibility(col) > tolerance) {
      new_cost = -1.0;
    }
    if (new_cost != (*objective)[col]) {
      changed = true;
      (*objective)[col] = new_cost;
    }
  }
  return changed;
}

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_GLOP_VARIABLE_VALUES_H_
