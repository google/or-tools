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

#ifndef OR_TOOLS_GLOP_REDUCED_COSTS_H_
#define OR_TOOLS_GLOP_REDUCED_COSTS_H_

#include "ortools/glop/basis_representation.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/primal_edge_norms.h"
#include "ortools/glop/status.h"
#include "ortools/glop/update_row.h"
#include "ortools/glop/variables_info.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/util/random_engine.h"
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
               random_engine_t* random);

  // If this is true, then the caller must re-factorize the basis before the
  // next call to GetReducedCosts().
  bool NeedsBasisRefactorization() const;

  // Checks the precision of the entering variable choice now that the direction
  // is computed, and return true if we can continue with this entering column,
  // or false if this column is actually not good and ChooseEnteringColumn()
  // need to be called again.
  bool TestEnteringReducedCostPrecision(ColIndex entering_col,
                                        ScatteredColumnReference direction,
                                        Fractional* reduced_cost);

  // Computes the current dual residual and infeasibility. Note that these
  // functions are not really fast (many scalar products will be computed) and
  // shouldn't be called at each iteration. They will return 0.0 if the reduced
  // costs need to be recomputed first and fail in debug mode.
  Fractional ComputeMaximumDualResidual() const;
  Fractional ComputeMaximumDualInfeasibility() const;
  Fractional ComputeSumOfDualInfeasibilities() const;

  // Updates any internal data BEFORE the given simplex pivot is applied to B.
  // Note that no updates are needed in case of a bound flip.
  // The arguments are in order:
  // - The index of the entering non-basic column of A.
  // - The index in B of the leaving basic variable.
  // - The 'direction', i.e. the right inverse of the entering column.
  void UpdateBeforeBasisPivot(ColIndex entering_col, RowIndex leaving_row,
                              const DenseColumn& direction,
                              UpdateRow* update_row);

  // Once a pivot has been done, this need to be called on the column that just
  // left the basis before the next ChooseEnteringColumn(). On a bound flip,
  // this must also be called with the variable that flipped.
  //
  // In both cases, the variable should be dual-feasible by construction. This
  // sets is_dual_infeasible_[col] to false and checks in debug mode that the
  // variable is indeed not an entering candidate.
  void SetAndDebugCheckThatColumnIsDualFeasible(ColIndex col);

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
  // cost becomes 0.0. Actually, this shifts the cost a bit more, so that the
  // reduced cost becomes slightly of the other sign.
  //
  // This is explained in Koberstein's thesis (section 6.2.2.3) and helps on
  // degenerate problems. As of july 2013, this allowed to pass dano3mip and
  // dbic1 without cycling forever. Note that contrary to what is explained in
  // the thesis, we do not shift any other variable costs. If any becomes
  // infeasible, it will be selected and shifted in subsequent iterations.
  void ShiftCost(ColIndex col);

  // Removes any cost shift and cost perturbation. This also lazily forces a
  // recomputation of all the derived quantities. This effectively resets the
  // class to its initial state.
  void ClearAndRemoveCostShifts();

  // Invalidates all internal structure that depends on the objective function.
  void ResetForNewObjective();

  // Sets whether or not the bitset of the dual infeasible positions is updated.
  void MaintainDualInfeasiblePositions(bool maintain);

  // Invalidates the data that depends on the order of the column in basis_.
  void UpdateDataOnBasisPermutation();

  // Returns the current reduced costs. If AreReducedCostsPrecise() is true,
  // then for basic columns, this gives the error between 'c_B' and 'y.B' and
  // for non-basic columns, this is the classic reduced cost. If it is false,
  // then this is defined only for the columns in
  // variables_info_.GetIsRelevantBitRow().
  const DenseRow& GetReducedCosts();

  // Returns the non-basic columns that are dual-infeasible. These are also
  // the primal simplex possible entering columns.
  const DenseBitRow& GetDualInfeasiblePositions() const {
    // TODO(user): recompute if needed?
    DCHECK(are_dual_infeasible_positions_maintained_);
    return is_dual_infeasible_;
  }

  // Returns the dual values associated to the current basis.
  const DenseColumn& GetDualValues();

  // Stats related functions.
  std::string StatString() const { return stats_.StatString(); }

  // Returns the current dual feasibility tolerance.
  Fractional GetDualFeasibilityTolerance() const {
    return dual_feasibility_tolerance_;
  }

  // Does basic checking of an entering candidate. To be used in DCHECK().
  bool IsValidPrimalEnteringCandidate(ColIndex col) const;

  // Visible for testing.
  const DenseRow& GetCostPerturbations() const {
    return objective_perturbation_;
  }

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

  // Small utility function to be called before reduced_costs_ and/or
  // is_dual_infeasible_ are accessed.
  void RecomputeReducedCostsAndPrimalEnteringCandidatesIfNeeded();

  // All these Compute() functions fill the corresponding DenseRow using
  // the current problem data.
  void ComputeBasicObjective();
  void ComputeReducedCosts();
  void ComputeBasicObjectiveLeftInverse();

  // Updates reduced_costs_ according to the given pivot. This adds a multiple
  // of the vector equal to 1.0 on the leaving column and given by
  // ComputeUpdateRow() on the non-basic columns. The multiple is such that the
  // new leaving reduced cost is zero. If update_column_status is false, then
  // is_dual_infeasible_ will not be updated.
  void UpdateReducedCosts(ColIndex entering_col, ColIndex leaving_col,
                          RowIndex leaving_row, Fractional pivot,
                          UpdateRow* update_row);

  // Recomputes from scratch the is_dual_infeasible_ bit row. Note that an
  // entering candidate is by definition a dual-infeasible variable.
  void ResetDualInfeasibilityBitSet();

  // Recomputes is_dual_infeasible_ but only for the given column indices.
  template <typename ColumnsToUpdate>
  void UpdateEnteringCandidates(const ColumnsToUpdate& cols);

  // Updates basic_objective_ according to the given pivot.
  void UpdateBasicObjective(ColIndex entering_col, RowIndex leaving_row);

  // Problem data that should be updated from outside.
  const CompactSparseMatrix& matrix_;
  const DenseRow& objective_;
  const RowToColMapping& basis_;
  const VariablesInfo& variables_info_;
  const BasisFactorization& basis_factorization_;
  random_engine_t* random_;

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

  // Values of the objective on the columns of the basis. The order is given by
  // the basis_ mapping. It is usually denoted as 'c_B' in the literature .
  DenseRow basic_objective_;

  // Perturbations to the objective function. This may be introduced to
  // counter degenerecency. It will be removed at the end of the algorithm.
  //
  // TODO(user): rename this cost_perturbations_ to be more consistent with
  // the literature.
  DenseRow objective_perturbation_;

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
  DenseRow basic_objective_left_inverse_;

  // This is usually parameters_.dual_feasibility_tolerance() except when the
  // dual residual error |y.B - c_B| is higher than it and we have to increase
  // the tolerance.
  Fractional dual_feasibility_tolerance_;

  // True for the columns that can enter the basis.
  // TODO(user): Investigate using a list of ColIndex instead (but we need
  // dynamic update and may lose the ordering of the indices).
  DenseBitRow is_dual_infeasible_;

  // Indicates if the dual-infeasible positions are maintained or not.
  bool are_dual_infeasible_positions_maintained_;

  DISALLOW_COPY_AND_ASSIGN(ReducedCosts);
};

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_GLOP_REDUCED_COSTS_H_
