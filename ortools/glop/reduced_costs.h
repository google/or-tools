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

#ifndef OR_TOOLS_GLOP_REDUCED_COSTS_H_
#define OR_TOOLS_GLOP_REDUCED_COSTS_H_

#include <string>
#include <vector>

#include "absl/random/bit_gen_ref.h"
#include "ortools/glop/basis_representation.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/pricing.h"
#include "ortools/glop/primal_edge_norms.h"
#include "ortools/glop/status.h"
#include "ortools/glop/update_row.h"
#include "ortools/glop/variables_info.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/scattered_vector.h"
#include "ortools/util/stats.h"

namespace operations_research {
namespace glop {

// Maintains the reduced costs of the non-basic variables and some related
// quantities.
//
// Terminology:
// - To each non-basic column 'a' of A, we can associate an "edge" in the
//   kernel of A equal to 1.0 on the index of 'a' and '-B^{-1}.a' on the basic
//   variables.
// - 'B^{-1}.a' is called the "right inverse" of 'a'.
// - The reduced cost of a column is equal to the scalar product of this
//   column's edge with the cost vector (objective_), and corresponds to the
//   variation in the objective function when we add this edge to the current
//   solution.
// - The dual values are the "left inverse" of the basic objective by B.
//   That is 'basic_objective_.B^{-1}'
// - The reduced cost of a column is also equal to the scalar product of this
//   column with the vector of the dual values.
class ReducedCosts {
 public:
  // Takes references to the linear program data we need.
  ReducedCosts(const CompactSparseMatrix& matrix_, const DenseRow& objective,
               const RowToColMapping& basis,
               const VariablesInfo& variables_info,
               const BasisFactorization& basis_factorization,
               absl::BitGenRef random);

  // If this is true, then the caller must re-factorize the basis before the
  // next call to GetReducedCosts().
  bool NeedsBasisRefactorization() const;

  // Checks the precision of the entering variable choice now that the direction
  // is computed. Returns its precise version. This will also trigger a
  // reduced cost recomputation if it was deemed too imprecise.
  Fractional TestEnteringReducedCostPrecision(ColIndex entering_col,
                                              const ScatteredColumn& direction);

  // Computes the current dual residual and infeasibility. Note that these
  // functions are not really fast (many scalar products will be computed) and
  // shouldn't be called at each iteration.
  //
  // These function will compute the reduced costs if needed.
  // ComputeMaximumDualResidual() also needs ComputeBasicObjectiveLeftInverse()
  // and do not depends on reduced costs.
  Fractional ComputeMaximumDualResidual();
  Fractional ComputeMaximumDualInfeasibility();
  Fractional ComputeSumOfDualInfeasibilities();

  // Same as ComputeMaximumDualInfeasibility() but ignore boxed variables.
  // Because we can always switch bounds of boxed variables, if this is under
  // the dual tolerance, then we can easily have a dual feasible solution and do
  // not need to run a dual phase I algorithm.
  Fractional ComputeMaximumDualInfeasibilityOnNonBoxedVariables();

  // Updates any internal data BEFORE the given simplex pivot is applied to B.
  // Note that no updates are needed in case of a bound flip.
  // The arguments are in order:
  // - The index of the entering non-basic column of A.
  // - The index in B of the leaving basic variable.
  // - The 'direction', i.e. the right inverse of the entering column.
  void UpdateBeforeBasisPivot(ColIndex entering_col, RowIndex leaving_row,
                              const ScatteredColumn& direction,
                              UpdateRow* update_row);

  // Sets the cost of the given non-basic variable to zero and updates its
  // reduced cost. Note that changing the cost of a non-basic variable only
  // impacts its reduced cost and not the one of any other variables.
  // The current_cost pointer must be equal to the address of objective[col]
  // where objective is the DenseRow passed at construction.
  void SetNonBasicVariableCostToZero(ColIndex col, Fractional* current_cost);

  // Sets the pricing parameters. This does not change the pricing rule.
  void SetParameters(const GlopParameters& parameters);

  // Returns true if the current reduced costs are computed with maximum
  // precision.
  bool AreReducedCostsPrecise() { return are_reduced_costs_precise_; }

  // Returns true if the current reduced costs where just recomputed or will be
  // on the next call to GetReducedCosts().
  bool AreReducedCostsRecomputed() {
    return recompute_reduced_costs_ || are_reduced_costs_recomputed_;
  }

  // Makes sure the next time the reduced cost are needed, they will be
  // recomputed with maximum precision (i.e. from scratch with a basis
  // refactorization first).
  void MakeReducedCostsPrecise();

  // Randomly perturb the costs. Both Koberstein and Huangfu recommend doing
  // that before the dual simplex starts in their Phd thesis.
  //
  // The perturbation follows what is explained in Huangfu Q (2013) "High
  // performance simplex solver", Ph.D, dissertation, University of Edinburgh,
  // section 3.2.3, page 58.
  void PerturbCosts();

  // Shifts the cost of the given non-basic column such that its current reduced
  // cost becomes 0.0. Actually, this shifts the cost a bit more according to
  // the positive_direction parameter.
  //
  // This is explained in Koberstein's thesis (section 6.2.2.3) and helps on
  // degenerate problems. As of july 2013, this allowed to pass dano3mip and
  // dbic1 without cycling forever. Note that contrary to what is explained in
  // the thesis, we do not shift any other variable costs. If any becomes
  // infeasible, it will be selected and shifted in subsequent iterations.
  void ShiftCostIfNeeded(bool increasing_rc_is_needed, ColIndex col);

  // Returns true if ShiftCostIfNeeded() was applied since the last
  // ClearAndRemoveCostShifts().
  bool HasCostShift() const { return has_cost_shift_; }

  // Returns true if this step direction make the given column even more
  // infeasible. This is just used for reporting stats.
  bool StepIsDualDegenerate(bool increasing_rc_is_needed, ColIndex col);

  // Removes any cost shift and cost perturbation. This also lazily forces a
  // recomputation of all the derived quantities. This effectively resets the
  // class to its initial state.
  void ClearAndRemoveCostShifts();

  // Invalidates all internal structure that depends on the objective function.
  void ResetForNewObjective();

  // Invalidates the data that depends on the order of the column in basis_.
  void UpdateDataOnBasisPermutation();

  // Returns the current reduced costs. If AreReducedCostsPrecise() is true,
  // then for basic columns, this gives the error between 'c_B' and 'y.B' and
  // for non-basic columns, this is the classic reduced cost. If it is false,
  // then this is defined only for the columns in
  // variables_info_.GetIsRelevantBitRow().
  const DenseRow& GetReducedCosts();

  // Same as GetReducedCosts() but trigger a recomputation if not already done
  // to have access to the reduced costs on all positions, not just the relevant
  // one.
  const DenseRow& GetFullReducedCosts();

  // Returns the dual values associated to the current basis.
  const DenseColumn& GetDualValues();

  // Stats related functions.
  std::string StatString() const { return stats_.StatString(); }

  // Returns the current dual feasibility tolerance.
  Fractional GetDualFeasibilityTolerance() const {
    return dual_feasibility_tolerance_;
  }

  // Does basic checking of an entering candidate.
  bool IsValidPrimalEnteringCandidate(ColIndex col) const;

  // Visible for testing.
  const DenseRow& GetCostPerturbations() const { return cost_perturbations_; }

  // The deterministic time used by this class.
  double DeterministicTime() const { return deterministic_time_; }

  // Registers a boolean that will be set to true each time the reduced costs
  // are or will be recomputed. This allows anyone that depends on this to know
  // that it cannot just assume an incremental changes and needs to updates its
  // data. Important: UpdateBeforeBasisPivot() will not trigger this.
  void AddRecomputationWatcher(bool* watcher) { watchers_.push_back(watcher); }

 private:
  // Statistics about this class.
  struct Stats : public StatsGroup {
    Stats()
        : StatsGroup("ReducedCosts"),
          basic_objective_left_inverse_density(
              "basic_objective_left_inverse_density", this),
          reduced_costs_accuracy("reduced_costs_accuracy", this),
          cost_shift("cost_shift", this) {}
    RatioDistribution basic_objective_left_inverse_density;
    DoubleDistribution reduced_costs_accuracy;
    DoubleDistribution cost_shift;
  };

  // All these Compute() functions fill the corresponding DenseRow using
  // the current problem data.
  void ComputeBasicObjective();
  void ComputeReducedCosts();
  void ComputeBasicObjectiveLeftInverse();

  // Updates reduced_costs_ according to the given pivot. This adds a multiple
  // of the vector equal to 1.0 on the leaving column and given by
  // ComputeUpdateRow() on the non-basic columns. The multiple is such that the
  // new leaving reduced cost is zero.
  void UpdateReducedCosts(ColIndex entering_col, ColIndex leaving_col,
                          RowIndex leaving_row, Fractional pivot,
                          UpdateRow* update_row);

  // Updates basic_objective_ according to the given pivot.
  void UpdateBasicObjective(ColIndex entering_col, RowIndex leaving_row);

  // All places that do 'recompute_reduced_costs_ = true' must go through here.
  void SetRecomputeReducedCostsAndNotifyWatchers();

  // Problem data that should be updated from outside.
  const CompactSparseMatrix& matrix_;
  const DenseRow& objective_;
  const RowToColMapping& basis_;
  const VariablesInfo& variables_info_;
  const BasisFactorization& basis_factorization_;
  absl::BitGenRef random_;

  // Internal data.
  GlopParameters parameters_;
  mutable Stats stats_;

  // Booleans to control what happens on the next ChooseEnteringColumn() call.
  bool must_refactorize_basis_;
  bool recompute_basic_objective_left_inverse_;
  bool recompute_basic_objective_;
  bool recompute_reduced_costs_;

  // Indicates if we have computed the reduced costs with a good precision.
  bool are_reduced_costs_precise_;
  bool are_reduced_costs_recomputed_;

  bool has_cost_shift_ = false;

  // Values of the objective on the columns of the basis. The order is given by
  // the basis_ mapping. It is usually denoted as 'c_B' in the literature .
  DenseRow basic_objective_;

  // Perturbations to the objective function. This may be introduced to
  // counter degenerecency. It will be removed at the end of the algorithm.
  DenseRow cost_perturbations_;

  // Reduced costs of the relevant columns of A.
  DenseRow reduced_costs_;

  // Left inverse by B of the basic_objective_. This is known as 'y' or 'pi' in
  // the literature. Its scalar product with a column 'a' of A gives the value
  // of the scalar product of the basic objective with the right inverse of 'a'.
  //
  // TODO(user): using the unit_row_left_inverse_, we can update the
  // basic_objective_left_inverse_ at each iteration, this is not needed for the
  // algorithm, but may gives us a good idea of the current precision of our
  // estimates. It is also faster to compute the unit_row_left_inverse_ because
  // of sparsity.
  ScatteredRow basic_objective_left_inverse_;

  // This is usually parameters_.dual_feasibility_tolerance() except when the
  // dual residual error |y.B - c_B| is higher than it and we have to increase
  // the tolerance.
  Fractional dual_feasibility_tolerance_;

  // Boolean(s) to set to false when the reduced cost are changed outside of the
  // UpdateBeforeBasisPivot() function.
  std::vector<bool*> watchers_;

  double deterministic_time_ = 0.0;

  DISALLOW_COPY_AND_ASSIGN(ReducedCosts);
};

// Maintains the list of dual infeasible positions and their associated prices.
//
// TODO(user): Not high priority but should probably be moved to its own file.
class PrimalPrices {
 public:
  // Takes references to what we need.
  // TODO(user): Switch to a model based API like in CP-SAT.
  PrimalPrices(absl::BitGenRef random, const VariablesInfo& variables_info,
               PrimalEdgeNorms* primal_edge_norms, ReducedCosts* reduced_costs);

  // Returns the best candidate out of the dual infeasible positions to enter
  // the basis during a primal simplex iterations.
  ColIndex GetBestEnteringColumn();

  // Similar to the other UpdateBeforeBasisPivot() functions.
  //
  // Important: Both the primal norms and reduced costs must have been updated
  // before this is called.
  void UpdateBeforeBasisPivot(ColIndex entering_col, UpdateRow* update_row);

  // Triggers a recomputation of the price at the given column only.
  void RecomputePriceAt(ColIndex col);

  // Same than RecomputePriceAt() for the case where we know the position is
  // dual feasible.
  void SetAndDebugCheckThatColumnIsDualFeasible(ColIndex col);

  // If the incremental updates are not properly called for a while, then it is
  // important to make sure that the prices will be recomputed the next time
  // GetBestEnteringColumn() is called.
  void ForceRecomputation() { recompute_ = true; }

 private:
  // Recomputes the primal prices but only for the given column indices. If
  // from_clean_state is true, then we assume that there is currently no
  // candidates in prices_.
  template <bool from_clean_state, typename ColumnsToUpdate>
  void UpdateEnteringCandidates(const ColumnsToUpdate& cols);

  bool recompute_ = true;
  DynamicMaximum<ColIndex> prices_;

  const VariablesInfo& variables_info_;
  PrimalEdgeNorms* primal_edge_norms_;
  ReducedCosts* reduced_costs_;
};

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_GLOP_REDUCED_COSTS_H_
