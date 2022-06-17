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

#ifndef OR_TOOLS_GLOP_VARIABLES_INFO_H_
#define OR_TOOLS_GLOP_VARIABLES_INFO_H_

#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/sparse.h"

namespace operations_research {
namespace glop {

// Holds the statuses of all the variables, including slack variables. There
// is no point storing constraint statuses since internally all constraints are
// always fixed to zero.
//
// Note that this is the minimal amount of information needed to perform a "warm
// start". Using this information and the original linear program, the basis can
// be refactorized and all the needed quantities derived.
//
// TODO(user): Introduce another state class to store a complete state of the
// solver. Using this state and the original linear program, the solver can be
// restarted with as little time overhead as possible. This is especially useful
// for strong branching in a MIP context.
struct BasisState {
  // TODO(user): A MIP solver will potentially store a lot of BasisStates so
  // memory usage is important. It is possible to use only 2 bits for one
  // VariableStatus enum. To achieve this, the FIXED_VALUE status can be
  // converted to either AT_LOWER_BOUND or AT_UPPER_BOUND and decoded properly
  // later since this will be used with a given linear program. This way we can
  // even encode more information by using the reduced cost sign to choose to
  // which bound the fixed status correspond.
  VariableStatusRow statuses;

  // Returns true if this state is empty.
  bool IsEmpty() const { return statuses.empty(); }
};

// Class responsible for maintaining diverse information for each variable that
// depend on its bounds and status.
//
// Note(user): Not all information is needed at all time, but it is cheap to
// maintain it since it only requires a few calls to Update() per simplex
// iteration.
class VariablesInfo {
 public:
  // Takes references to the linear program data we need.
  explicit VariablesInfo(const CompactSparseMatrix& matrix);

  // Updates the internal bounds and recomputes the variable types from the
  // bounds (this is the only function that changes them).
  //
  // Returns true iff the existing bounds didn't change. Except if the bounds
  // AND underlying matrix didn't change, one will need to call one of the two
  // Initialize*() methods below before using this class.
  bool LoadBoundsAndReturnTrueIfUnchanged(const DenseRow& new_lower_bounds,
                                          const DenseRow& new_upper_bounds);

  // Same for an LP not in equation form.
  bool LoadBoundsAndReturnTrueIfUnchanged(
      const DenseRow& variable_lower_bounds,
      const DenseRow& variable_upper_bounds,
      const DenseColumn& constraint_lower_bounds,
      const DenseColumn& constraint_upper_bounds);

  // Initializes the status according to the given BasisState. Incompatible
  // statuses will be corrected, and we transform the state correctly if new
  // columns / rows were added. Note however that one will need to update the
  // BasisState with deletions to preserve the status of unchanged columns.
  void InitializeFromBasisState(ColIndex first_slack, ColIndex num_new_cols,
                                const BasisState& state);

  // Changes to the FREE status any column with a BASIC status not listed in
  // the basis. Returns their number. Also makes sure all the columns listed in
  // basis are marked as basic. Note that if a variable is fixed, we set its
  // status to FIXED_VALUE not FREE.
  int ChangeUnusedBasicVariablesToFree(const RowToColMapping& basis);

  // Loops over all the free variables, and if such a variable has bounds and
  // its starting value is closer to its closest bound than the given distance,
  // change the status to move this variable to that bound. Returns the number
  // of changes. The variable for which starting_values is not provided are
  // considered at zero.
  //
  // This is mainly useful if non-zero starting values are provided. It allows
  // to move all the variables close to their bounds at once instead of having
  // to move them one by one with simplex pivots later. Of course, by doing that
  // we usually introduce a small primal infeasibility that might need
  // correction.
  //
  // If one uses a large distance, then all such variables will start at their
  // bound if they have one.
  int SnapFreeVariablesToBound(Fractional distance,
                               const DenseRow& starting_values);

  // Sets all variables status to their lowest magnitude bounds. Note that there
  // will be no basic variable after this is called.
  void InitializeToDefaultStatus();

  // Updates the information of the given variable. Note that it is not needed
  // to call this if the status or the bound of a variable didn't change.
  void UpdateToBasicStatus(ColIndex col);
  void UpdateToNonBasicStatus(ColIndex col, VariableStatus status);

  // Various getter, see the corresponding member declaration below for more
  // information.
  const VariableTypeRow& GetTypeRow() const;
  const VariableStatusRow& GetStatusRow() const;
  const DenseBitRow& GetCanIncreaseBitRow() const;
  const DenseBitRow& GetCanDecreaseBitRow() const;
  const DenseBitRow& GetIsRelevantBitRow() const;
  const DenseBitRow& GetIsBasicBitRow() const;
  const DenseBitRow& GetNotBasicBitRow() const;
  const DenseBitRow& GetNonBasicBoxedVariables() const;

  // Returns the variable bounds.
  const DenseRow& GetVariableLowerBounds() const { return lower_bounds_; }
  const DenseRow& GetVariableUpperBounds() const { return upper_bounds_; }

  const ColIndex GetNumberOfColumns() const { return matrix_.num_cols(); }

  // Changes whether or not a non-basic boxed variable is 'relevant' and will be
  // returned as such by GetIsRelevantBitRow().
  void MakeBoxedVariableRelevant(bool value);

  // This is used in UpdateRow to decide whether to compute it using the
  // row-wise or column-wise representation.
  EntryIndex GetNumEntriesInRelevantColumns() const;

  // Returns the distance between the upper and lower bound of the given column.
  Fractional GetBoundDifference(ColIndex col) const {
    return upper_bounds_[col] - lower_bounds_[col];
  }

  // This is used for the (SP) method of "Progress in the dual simplex method
  // for large scale LP problems: practical dual phase I algorithms". Achim
  // Koberstein & Uwe H. Suhl.
  //
  // This just set the bounds according to the variable types:
  // - Boxed variables get fixed at [0,0].
  // - Upper bounded variables get [-1, 0] bounds
  // - Lower bounded variables get [0, 1] bounds
  // - Free variables get [-1000, 1000] to heuristically move them to the basis.
  //   I.e. they cost in the dual infeasibility minimization problem is
  //   multiplied by 1000.
  //
  // It then update the status to get an initial dual feasible solution, and
  // then one just have to apply the phase II algo on this problem to try to
  // find a feasible solution to the original problem.
  //
  // Optimization: When a variable become basic, its non-zero bounds are
  // relaxed. This is a bit hacky as it requires that the status is updated
  // before the bounds are read (which is the case). It is however an important
  // optimization.
  //
  // TODO(user): Shall we re-add the bound when the variable is moved out of
  // the base? it is not needed, but might allow for more bound flips?
  void TransformToDualPhaseIProblem(Fractional dual_feasibility_tolerance,
                                    const DenseRow& reduced_costs);
  void EndDualPhaseI(Fractional dual_feasibility_tolerance,
                     const DenseRow& reduced_costs);

 private:
  // Computes the initial/default variable status from its type. A constrained
  // variable is set to the lowest of its 2 bounds in absolute value.
  VariableStatus DefaultVariableStatus(ColIndex col) const;

  // Resizes all status related vectors.
  void ResetStatusInfo();

  // Computes the variable type from its lower and upper bound.
  VariableType ComputeVariableType(ColIndex col) const;

  // Sets the column relevance and updates num_entries_in_relevant_columns_.
  void SetRelevance(ColIndex col, bool relevance);

  // Used by TransformToDualPhaseIProblem()/EndDualPhaseI().
  void UpdateStatusForNewType(ColIndex col);

  // Problem data that should be updated from outside.
  const CompactSparseMatrix& matrix_;

  // The variables bounds of the current problem. Like everything here, it
  // include the slacks.
  DenseRow lower_bounds_;
  DenseRow upper_bounds_;

  // This is just used temporarily by the dual phase I algo to save the original
  // bounds.
  DenseRow saved_lower_bounds_;
  DenseRow saved_upper_bounds_;

  // Array of variable statuses, indexed by column index.
  VariableStatusRow variable_status_;

  // Array of variable types, indexed by column index.
  VariableTypeRow variable_type_;

  // Indicates if a non-basic variable can move up or down while not increasing
  // the primal infeasibility. Note that all combinaisons are possible for a
  // variable according to its status: fixed, free, upper or lower bounded. This
  // is always false for basic variable.
  DenseBitRow can_increase_;
  DenseBitRow can_decrease_;

  // Indicates if we should consider this variable for entering the basis during
  // the simplex algorithm. We never consider fixed variables and in the dual
  // feasibility phase, we don't consider boxed variable.
  DenseBitRow relevance_;

  // Indicates if a variable is BASIC or not. There are currently two members
  // because the DenseBitRow class only supports a nice range-based iteration on
  // the non-zero positions and not on the others.
  DenseBitRow is_basic_;
  DenseBitRow not_basic_;

  // Set of boxed variables that are non-basic.
  DenseBitRow non_basic_boxed_variables_;

  // Number of entries for the relevant matrix columns (see relevance_).
  EntryIndex num_entries_in_relevant_columns_;

  // Whether or not a boxed variable should be considered relevant.
  bool boxed_variables_are_relevant_ = true;

  // Whether we are between the calls TransformToDualPhaseIProblem() and
  // EndDualPhaseI().
  bool in_dual_phase_one_ = false;

  DISALLOW_COPY_AND_ASSIGN(VariablesInfo);
};

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_GLOP_VARIABLES_INFO_H_
