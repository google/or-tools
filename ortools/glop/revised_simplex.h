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

// Solves a Linear Programing problem using the Revised Simplex algorithm
// as described by G.B. Dantzig.
// The general form is:
// min c.x where c and x are n-vectors,
// subject to Ax = b where A is an mxn-matrix, b an m-vector,
// with l <= x <= u, i.e.
// l_i <= x_i <= u_i for all i in {1 .. m}.
//
// c.x is called the objective function.
// Each row a_i of A is an n-vector, and a_i.x = b_i is a linear constraint.
// A is called the constraint matrix.
// b is called the right hand side (rhs) of the problem.
// The constraints l_i <= x_i <= u_i are called the generalized bounds
// of the problem (most introductory textbooks only deal with x_i >= 0, as
// did the first version of the Simplex algorithm). Note that l_i and u_i
// can be -infinity and +infinity, respectively.
//
// To simplify the entry of data, this code actually handles problems in the
// form:
// min c.x where c and x are n-vectors,
// subject to:
//  A1 x <= b1
//  A2 x >= b2
//  A3 x  = b3
//  l <= x <= u
//
// It transforms the above problem into
// min c.x where c and x are n-vectors,
// subject to:
//  A1 x + s1 = b1
//  A2 x - s2 = b2
//  A3 x      = b3
//  l <= x <= u
//  s1 >= 0, s2 >= 0
//  where xT = (x1, x2, x3),
//  s1 is an m1-vector (m1 being the height of A1),
//  s2 is an m2-vector (m2 being the height of A2).
//
// The following are very good references for terminology, data structures,
// and algorithms. They all contain a wealth of references.
//
// Vasek Chvátal, "Linear Programming," W.H. Freeman, 1983. ISBN 978-0716715870.
// http://www.amazon.com/dp/0716715872
//
// Robert J. Vanderbei, "Linear Programming: Foundations and Extensions,"
// Springer, 2010, ISBN-13: 978-1441944979
// http://www.amazon.com/dp/1441944974
//
// Istvan Maros, "Computational Techniques of the Simplex Method.", Springer,
// 2002, ISBN 978-1402073328
// http://www.amazon.com/dp/1402073321
//
// ===============================================
// Short description of the dual simplex algorithm.
//
// The dual simplex algorithm uses the same data structure as the primal, but
// progresses towards the optimal solution in a different way:
// * It tries to keep the dual values dual-feasible at all time which means that
//   the reduced costs are of the correct sign depending on the bounds of the
//   non-basic variables. As a consequence the values of the basic variable are
//   out of bound until the optimal is reached.
// * A basic leaving variable is selected first (dual pricing) and then a
//   corresponding entering variable is selected. This is done in such a way
//   that the dual objective value increases (lower bound on the optimal
//   solution).
// * Once the basis pivot is chosen, the variable values and the reduced costs
//   are updated the same way as in the primal algorithm.
//
// Good references on the Dual simplex algorithm are:
//
// Robert Fourer, "Notes on the Dual simplex Method", March 14, 1994.
// http://users.iems.northwestern.edu/~4er/WRITINGS/dual.pdf
//
// Achim Koberstein, "The dual simplex method, techniques for a fast and stable
// implementation", PhD, Paderborn, Univ., 2005.
// http://digital.ub.uni-paderborn.de/hs/download/pdf/3885?originalFilename=true

#ifndef OR_TOOLS_GLOP_REVISED_SIMPLEX_H_
#define OR_TOOLS_GLOP_REVISED_SIMPLEX_H_

#include <string>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/macros.h"
#include "ortools/glop/basis_representation.h"
#include "ortools/glop/dual_edge_norms.h"
#include "ortools/glop/entering_variable.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/primal_edge_norms.h"
#include "ortools/glop/reduced_costs.h"
#include "ortools/glop/status.h"
#include "ortools/glop/update_row.h"
#include "ortools/glop/variable_values.h"
#include "ortools/glop/variables_info.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_print_utils.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/scattered_vector.h"
#include "ortools/lp_data/sparse_row.h"
#include "ortools/util/random_engine.h"
#include "ortools/util/time_limit.h"

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
  // TODO(user): A MIP solver will potentially store a lot of BasicStates so
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

// Entry point of the revised simplex algorithm implementation.
class RevisedSimplex {
 public:
  RevisedSimplex();

  // Sets or gets the algorithm parameters to be used on the next Solve().
  void SetParameters(const GlopParameters& parameters);
  const GlopParameters& GetParameters() const { return parameters_; }

  // Solves the given linear program.
  //
  // Expects that the linear program is in the equations form Ax = 0 created by
  // LinearProgram::AddSlackVariablesForAllRows, i.e. the rightmost square
  // submatrix of A is an identity matrix, all its columns have been marked as
  // slack variables, and the bounds of all constraints have been set to [0, 0].
  // Returns ERROR_INVALID_PROBLEM, if these assumptions are violated.
  //
  // By default, the algorithm tries to exploit the computation done during the
  // last Solve() call. It will analyze the difference of the new linear program
  // and try to use the previously computed solution as a warm-start. To disable
  // this behavior or give explicit warm-start data, use one of the State*()
  // functions below.
  ABSL_MUST_USE_RESULT Status Solve(const LinearProgram& lp,
                                    TimeLimit* time_limit);

  // Do not use the current solution as a warm-start for the next Solve(). The
  // next Solve() will behave as if the class just got created.
  void ClearStateForNextSolve();

  // Uses the given state as a warm-start for the next Solve() call.
  void LoadStateForNextSolve(const BasisState& state);

  // Advanced usage. Tells the next Solve() that the matrix inside the linear
  // program will not change compared to the one used the last time Solve() was
  // called. This allows to bypass the somewhat costly check of comparing both
  // matrices. Note that this call will be ignored if Solve() was never called
  // or if ClearStateForNextSolve() was called.
  void NotifyThatMatrixIsUnchangedForNextSolve();

  // Getters to retrieve all the information computed by the last Solve().
  RowIndex GetProblemNumRows() const;
  ColIndex GetProblemNumCols() const;
  ProblemStatus GetProblemStatus() const;
  Fractional GetObjectiveValue() const;
  int64 GetNumberOfIterations() const;
  Fractional GetVariableValue(ColIndex col) const;
  Fractional GetReducedCost(ColIndex col) const;
  const DenseRow& GetReducedCosts() const;
  Fractional GetDualValue(RowIndex row) const;
  Fractional GetConstraintActivity(RowIndex row) const;
  VariableStatus GetVariableStatus(ColIndex col) const;
  ConstraintStatus GetConstraintStatus(RowIndex row) const;
  const BasisState& GetState() const;
  double DeterministicTime() const;
  bool objective_limit_reached() const { return objective_limit_reached_; }

  // If the problem status is PRIMAL_UNBOUNDED (respectively DUAL_UNBOUNDED),
  // then the solver has a corresponding primal (respectively dual) ray to show
  // the unboundness. From a primal (respectively dual) feasible solution any
  // positive multiple of this ray can be added to the solution and keep it
  // feasible. Moreover, by doing so, the objective of the problem will improve
  // and its magnitude will go to infinity.
  //
  // Note that when the problem is DUAL_UNBOUNDED, the dual ray is also known as
  // the Farkas proof of infeasibility of the problem.
  const DenseRow& GetPrimalRay() const;
  const DenseColumn& GetDualRay() const;

  // This is the "dual ray" linear combination of the matrix rows.
  const DenseRow& GetDualRayRowCombination() const;

  // Returns the index of the column in the basis and the basis factorization.
  // Note that the order of the column in the basis is important since it is the
  // one used by the various solve functions provided by the BasisFactorization
  // class.
  ColIndex GetBasis(RowIndex row) const;

  const ScatteredRow& GetUnitRowLeftInverse(RowIndex row) {
    return update_row_.ComputeAndGetUnitRowLeftInverse(row);
  }

  // Returns a copy of basis_ vector for outside applications (like cuts) to
  // have the correspondence between rows and columns of the dictionary.
  RowToColMapping GetBasisVector() const { return basis_; }

  const BasisFactorization& GetBasisFactorization() const;

  // Returns statistics about this class as a string.
  std::string StatString();

  // Computes the dictionary B^-1*N on-the-fly row by row. Returns the resulting
  // matrix as a vector of sparse rows so that it is easy to use it on the left
  // side in the matrix multiplication. Runs in O(num_non_zeros_in_matrix).
  // TODO(user): Use row scales as well.
  RowMajorSparseMatrix ComputeDictionary(const DenseRow* column_scales);

  // Initializes the matrix for the given 'linear_program' and 'state' and
  // computes the variable values for basic variables using non-basic variables.
  void ComputeBasicVariablesForState(const LinearProgram& linear_program,
                                     const BasisState& state);

  // This is used in a MIP context to polish the final basis. We assume that the
  // columns for which SetIntegralityScale() has been called correspond to
  // integral variable once multiplied by the given factor.
  void ClearIntegralityScales() { integrality_scale_.clear(); }
  void SetIntegralityScale(ColIndex col, Fractional scale);

 private:
  // Propagates parameters_ to all the other classes that need it.
  //
  // TODO(user): Maybe a better design is for them to have a reference to a
  // unique parameters object? It will clutter a bit more these classes'
  // constructor though.
  void PropagateParameters();

  // Returns a string containing the same information as with GetSolverStats,
  // but in a much more human-readable format. For example:
  //     Problem status                               : Optimal
  //     Solving time                                 : 1.843
  //     Number of iterations                         : 12345
  //     Time for solvability (first phase)           : 1.343
  //     Number of iterations for solvability         : 10000
  //     Time for optimization                        : 0.5
  //     Number of iterations for optimization        : 2345
  //     Maximum time allowed in seconds              : 6000
  //     Maximum number of iterations                 : 1000000
  //     Stop after first basis                       : 0
  std::string GetPrettySolverStats() const;

  // Returns a string containing formatted information about the variable
  // corresponding to column col.
  std::string SimpleVariableInfo(ColIndex col) const;

  // Displays a short string with the current iteration and objective value.
  void DisplayIterationInfo() const;

  // Displays the error bounds of the current solution.
  void DisplayErrors() const;

  // Displays the status of the variables.
  void DisplayInfoOnVariables() const;

  // Displays the bounds of the variables.
  void DisplayVariableBounds();

  // Displays the following information:
  //   * Linear Programming problem as a dictionary, taking into
  //     account the iterations that have been made;
  //   * Variable info;
  //   * Reduced costs;
  //   * Variable bounds.
  // A dictionary is in the form:
  // xB = value + sum_{j in N}  pa_ij x_j
  // z = objective_value + sum_{i in N}  rc_i x_i
  // where the pa's are the coefficients of the matrix after the pivotings
  // and the rc's are the reduced costs, i.e. the coefficients of the objective
  // after the pivotings.
  // Dictionaries are the modern way of presenting the result of an iteration
  // of the Simplex algorithm in the literature.
  void DisplayRevisedSimplexDebugInfo();

  // Displays the Linear Programming problem as it was input.
  void DisplayProblem() const;

  // Returns the current objective value. This is just the sum of the current
  // variable values times their current cost.
  Fractional ComputeObjectiveValue() const;

  // Returns the current objective of the linear program given to Solve() using
  // the initial costs, maximization direction, objective offset and objective
  // scaling factor.
  Fractional ComputeInitialProblemObjectiveValue() const;

  // Assigns names to variables. Variables in the input will be named
  // x1..., slack variables will be s1... .
  void SetVariableNames();

  // Computes the initial variable status from its type. A constrained variable
  // is set to the lowest of its 2 bounds in absolute value.
  VariableStatus ComputeDefaultVariableStatus(ColIndex col) const;

  // Sets the variable status and derives the variable value according to the
  // exact status definition. This can only be called for non-basic variables
  // because the value of a basic variable is computed from the values of the
  // non-basic variables.
  void SetNonBasicVariableStatusAndDeriveValue(ColIndex col,
                                               VariableStatus status);

  // Checks if the basis_ and is_basic_ arrays are well formed. Also checks that
  // the variable statuses are consistent with this basis. Returns true if this
  // is the case. This is meant to be used in debug mode only.
  bool BasisIsConsistent() const;

  // Moves the column entering_col into the basis at position basis_row. Removes
  // the current basis column at position basis_row from the basis and sets its
  // status to leaving_variable_status.
  void UpdateBasis(ColIndex entering_col, RowIndex basis_row,
                   VariableStatus leaving_variable_status);

  // Initializes matrix-related internal data. Returns true if this data was
  // unchanged. If not, also sets only_change_is_new_rows to true if compared
  // to the current matrix, the only difference is that new rows have been
  // added (with their corresponding extra slack variables).  Similarly, sets
  // only_change_is_new_cols to true if the only difference is that new columns
  // have been added, in which case also sets num_new_cols to the number of
  // new columns.
  bool InitializeMatrixAndTestIfUnchanged(const LinearProgram& lp,
                                          bool* only_change_is_new_rows,
                                          bool* only_change_is_new_cols,
                                          ColIndex* num_new_cols);

  // Initializes bound-related internal data. Returns true if unchanged.
  bool InitializeBoundsAndTestIfUnchanged(const LinearProgram& lp);

  // Checks if the only change to the bounds is the addition of new columns,
  // and that the new columns have at least one bound equal to zero.
  bool OldBoundsAreUnchangedAndNewVariablesHaveOneBoundAtZero(
      const LinearProgram& lp, ColIndex num_new_cols);

  // Initializes objective-related internal data. Returns true if unchanged.
  bool InitializeObjectiveAndTestIfUnchanged(const LinearProgram& lp);

  // Computes the stopping criterion on the problem objective value.
  void InitializeObjectiveLimit(const LinearProgram& lp);

  // Initializes the variable statuses using a warm-start basis.
  void InitializeVariableStatusesForWarmStart(const BasisState& state,
                                              ColIndex num_new_cols);

  // Initializes the starting basis. In most cases it starts by the all slack
  // basis and tries to apply some heuristics to replace fixed variables.
  ABSL_MUST_USE_RESULT Status CreateInitialBasis();

  // Sets the initial basis to the given columns, try to factorize it and
  // recompute the basic variable values.
  ABSL_MUST_USE_RESULT Status
  InitializeFirstBasis(const RowToColMapping& initial_basis);

  // Entry point for the solver initialization.
  ABSL_MUST_USE_RESULT Status Initialize(const LinearProgram& lp);

  // Saves the current variable statuses in solution_state_.
  void SaveState();

  // Displays statistics on what kinds of variables are in the current basis.
  void DisplayBasicVariableStatistics();

  // Tries to reduce the initial infeasibility (stored in error_) by using the
  // singleton columns present in the problem. A singleton column is a column
  // with only one non-zero. This is used by CreateInitialBasis().
  void UseSingletonColumnInInitialBasis(RowToColMapping* basis);

  // Returns the number of empty rows in the matrix, i.e. rows where all
  // the coefficients are zero.
  RowIndex ComputeNumberOfEmptyRows();

  // Returns the number of empty columns in the matrix, i.e. columns where all
  // the coefficients are zero.
  ColIndex ComputeNumberOfEmptyColumns();

  // This method transforms a basis for the first phase, with the optimal
  // value at zero, into a feasible basis for the initial problem, thus
  // preparing the execution of phase-II of the algorithm.
  void CleanUpBasis();

  // If the primal maximum residual is too large, recomputes the basic variable
  // value from the non-basic ones. This function also perturbs the bounds
  // during the primal simplex if too many iterations are degenerate.
  //
  // Only call this on a refactorized basis to have the best precision.
  void CorrectErrorsOnVariableValues();

  // Computes b - A.x in error_
  void ComputeVariableValuesError();

  // Solves the system B.d = a where a is the entering column (given by col).
  // Known as FTRAN (Forward transformation) in FORTRAN codes.
  // See Chvatal's book for more detail (Chapter 7).
  void ComputeDirection(ColIndex col);

  // Computes a - B.d in error_ and return the maximum std::abs() of its coeffs.
  Fractional ComputeDirectionError(ColIndex col);

  // Computes the ratio of the basic variable corresponding to 'row'. A target
  // bound (upper or lower) is chosen depending on the sign of the entering
  // reduced cost and the sign of the direction 'd_[row]'. The ratio is such
  // that adding 'ratio * d_[row]' to the variable value changes it to its
  // target bound.
  template <bool is_entering_reduced_cost_positive>
  Fractional GetRatio(RowIndex row) const;

  // First pass of the Harris ratio test. Returns the harris ratio value which
  // is an upper bound on the ratio value that the leaving variable can take.
  // Fills leaving_candidates with the ratio and row index of a super-set of the
  // columns with a ratio <= harris_ratio.
  template <bool is_entering_reduced_cost_positive>
  Fractional ComputeHarrisRatioAndLeavingCandidates(
      Fractional bound_flip_ratio, SparseColumn* leaving_candidates) const;

  // Chooses the leaving variable, considering the entering column and its
  // associated reduced cost. If there was a precision issue and the basis is
  // not refactorized, set refactorize to true. Otherwise, the row number of the
  // leaving variable is written in *leaving_row, and the step length
  // is written in *step_length.
  Status ChooseLeavingVariableRow(ColIndex entering_col,
                                  Fractional reduced_cost, bool* refactorize,
                                  RowIndex* leaving_row,
                                  Fractional* step_length,
                                  Fractional* target_bound);

  // Chooses the leaving variable for the primal phase-I algorithm. The
  // algorithm follows more or less what is described in Istvan Maros's book in
  // chapter 9.6 and what is done for the dual phase-I algorithm which was
  // derived from Koberstein's PhD. Both references can be found at the top of
  // this file.
  void PrimalPhaseIChooseLeavingVariableRow(ColIndex entering_col,
                                            Fractional reduced_cost,
                                            bool* refactorize,
                                            RowIndex* leaving_row,
                                            Fractional* step_length,
                                            Fractional* target_bound) const;

  // Chooses an infeasible basic variable. The returned values are:
  // - leaving_row: the basic index of the infeasible leaving variable
  //   or kNoLeavingVariable if no such row exists: the dual simplex algorithm
  //   has terminated and the optimal has been reached.
  // - cost_variation: how much do we improve the objective by moving one unit
  //   along this dual edge.
  // - target_bound: the bound at which the leaving variable should go when
  //   leaving the basis.
  ABSL_MUST_USE_RESULT Status DualChooseLeavingVariableRow(
      RowIndex* leaving_row, Fractional* cost_variation,
      Fractional* target_bound);

  // Updates the prices used by DualChooseLeavingVariableRow() after a simplex
  // iteration by using direction_. The prices are stored in
  // dual_pricing_vector_. Note that this function only takes care of the
  // entering and leaving column dual feasibility status change and that other
  // changes will be dealt with by DualPhaseIUpdatePriceOnReducedCostsChange().
  void DualPhaseIUpdatePrice(RowIndex leaving_row, ColIndex entering_col);

  // Updates the prices used by DualChooseLeavingVariableRow() when the reduced
  // costs of the given columns have changed.
  template <typename Cols>
  void DualPhaseIUpdatePriceOnReducedCostChange(const Cols& cols);

  // Same as DualChooseLeavingVariableRow() but for the phase I of the dual
  // simplex. Here the objective is not to minimize the primal infeasibility,
  // but the dual one, so the variable is not chosen in the same way. See
  // "Notes on the Dual simplex Method" or Istvan Maros, "A Piecewise Linear
  // Dual Phase-1 Algorithm for the Simplex Method", Computational Optimization
  // and Applications, October 2003, Volume 26, Issue 1, pp 63-81.
  // http://rd.springer.com/article/10.1023%2FA%3A1025102305440
  ABSL_MUST_USE_RESULT Status DualPhaseIChooseLeavingVariableRow(
      RowIndex* leaving_row, Fractional* cost_variation,
      Fractional* target_bound);

  // Makes sure the boxed variable are dual-feasible by setting them to the
  // correct bound according to their reduced costs. This is called
  // Dual feasibility correction in the literature.
  //
  // Note that this function is also used as a part of the bound flipping ratio
  // test by flipping the boxed dual-infeasible variables at each iteration.
  //
  // If update_basic_values is true, the basic variable values are updated.
  template <typename BoxedVariableCols>
  void MakeBoxedVariableDualFeasible(const BoxedVariableCols& cols,
                                     bool update_basic_values);

  // Computes the step needed to move the leaving_row basic variable to the
  // given target bound.
  Fractional ComputeStepToMoveBasicVariableToBound(RowIndex leaving_row,
                                                   Fractional target_bound);

  // Returns true if the basis obtained after the given pivot can be factorized.
  bool TestPivot(ColIndex entering_col, RowIndex leaving_row);

  // Gets the current LU column permutation from basis_representation,
  // applies it to basis_ and then sets it to the identity permutation since
  // it will no longer be needed during solves. This function also updates all
  // the data that depends on the column order in basis_.
  void PermuteBasis();

  // Updates the system state according to the given basis pivot.
  // Returns an error if the update could not be done because of some precision
  // issue.
  ABSL_MUST_USE_RESULT Status UpdateAndPivot(ColIndex entering_col,
                                             RowIndex leaving_row,
                                             Fractional target_bound);

  // Displays all the timing stats related to the calling object.
  void DisplayAllStats();

  // Returns whether or not a basis refactorization is needed at the beginning
  // of the main loop in Minimize() or DualMinimize(). The idea is that if a
  // refactorization is going to be needed by one of the components, it is
  // better to do that as soon as possible so that every component can take
  // advantage of it.
  bool NeedsBasisRefactorization(bool refactorize);

  // Calls basis_factorization_.Refactorize() depending on the result of
  // NeedsBasisRefactorization(). Invalidates any data structure that depends
  // on the current factorization. Sets refactorize to false.
  Status RefactorizeBasisIfNeeded(bool* refactorize);

  // Minimize the objective function, be it for satisfiability or for
  // optimization. Used by Solve().
  ABSL_MUST_USE_RESULT Status Minimize(TimeLimit* time_limit);

  // Same as Minimize() for the dual simplex algorithm.
  // TODO(user): remove duplicate code between the two functions.
  ABSL_MUST_USE_RESULT Status DualMinimize(TimeLimit* time_limit);

  // Experimental. This is useful in a MIP context. It performs a few degenerate
  // pivot to try to mimize the fractionality of the optimal basis.
  //
  // We assume that the columns for which SetIntegralityScale() has been called
  // correspond to integral variable once scaled by the given factor.
  //
  // I could only find slides for the reference of this "LP Solution Polishing
  // to improve MIP Performance", Matthias Miltenberger, Zuse Institute Berlin.
  ABSL_MUST_USE_RESULT Status Polish(TimeLimit* time_limit);

  // Utility functions to return the current ColIndex of the slack column with
  // given number. Note that currently, such columns are always present in the
  // internal representation of a linear program.
  ColIndex SlackColIndex(RowIndex row) const;

  // Advances the deterministic time in time_limit with the difference between
  // the current internal deterministic time and the internal deterministic time
  // during the last call to this method.
  // TODO(user): Update the internals of revised simplex so that the time
  // limit is updated at the source and remove this method.
  void AdvanceDeterministicTime(TimeLimit* time_limit);

  // Problem status
  ProblemStatus problem_status_;

  // Current number of rows in the problem.
  RowIndex num_rows_;

  // Current number of columns in the problem.
  ColIndex num_cols_;

  // Index of the first slack variable in the input problem. We assume that all
  // variables with index greater or equal to first_slack_col_ are slack
  // variables.
  ColIndex first_slack_col_;

  // We're using vectors after profiling and looking at the generated assembly
  // it's as fast as std::unique_ptr as long as the size is properly reserved
  // beforehand.

  // Compact version of the matrix given to Solve().
  CompactSparseMatrix compact_matrix_;

  // The transpose of compact_matrix_, it may be empty if it is not needed.
  CompactSparseMatrix transposed_matrix_;

  // Stop the algorithm and report feasibility if:
  // - The primal simplex is used, the problem is primal-feasible and the
  //   current objective value is strictly lower than primal_objective_limit_.
  // - The dual simplex is used, the problem is dual-feasible and the current
  //   objective value is strictly greater than dual_objective_limit_.
  Fractional primal_objective_limit_;
  Fractional dual_objective_limit_;

  // Current objective (feasibility for Phase-I, user-provided for Phase-II).
  DenseRow current_objective_;

  // Array of coefficients for the user-defined objective.
  // Indexed by column number. Used in Phase-II.
  DenseRow objective_;

  // Objective offset and scaling factor of the linear program given to Solve().
  // This is used to display the correct objective values in the logs with
  // ComputeInitialProblemObjectiveValue().
  Fractional objective_offset_;
  Fractional objective_scaling_factor_;

  // Array of values representing variable bounds. Indexed by column number.
  DenseRow lower_bound_;
  DenseRow upper_bound_;

  // The bound perturbation to be used for basic variable that are slightly
  // outside their bounds. This contains small values that are non-zero only if
  // the primal simplex ran into many degenerate iterations.
  DenseRow bound_perturbation_;

  // Used in dual phase I to keep track of the non-basic dual infeasible
  // columns and their sign of infeasibility (+1 or -1).
  DenseRow dual_infeasibility_improvement_direction_;
  int num_dual_infeasible_positions_;

  // Used in dual phase I to hold the price of each possible leaving choices
  // and the bitset of the possible leaving candidates.
  DenseColumn dual_pricing_vector_;
  DenseBitColumn is_dual_entering_candidate_;

  // A temporary scattered column that is always reset to all zero after use.
  ScatteredColumn initially_all_zero_scratchpad_;

  // Array of column index, giving the column number corresponding
  // to a given basis row.
  RowToColMapping basis_;

  // Vector of strings containing the names of variables.
  // Indexed by column number.
  StrictITIVector<ColIndex, std::string> variable_name_;

  // Information about the solution computed by the last Solve().
  Fractional solution_objective_value_;
  DenseColumn solution_dual_values_;
  DenseRow solution_reduced_costs_;
  DenseRow solution_primal_ray_;
  DenseColumn solution_dual_ray_;
  DenseRow solution_dual_ray_row_combination_;
  BasisState solution_state_;
  bool solution_state_has_been_set_externally_;

  // Flag used by NotifyThatMatrixIsUnchangedForNextSolve() and changing
  // the behavior of Initialize().
  bool notify_that_matrix_is_unchanged_ = false;

  // This is known as 'd' in the literature and is set during each pivot to the
  // right inverse of the basic entering column of A by ComputeDirection().
  // ComputeDirection() also fills direction_.non_zeros with the position of the
  // non-zero.
  ScatteredColumn direction_;
  Fractional direction_infinity_norm_;

  // Used to compute the error 'b - A.x' or 'a - B.d'.
  DenseColumn error_;

  // Representation of matrix B using eta matrices and LU decomposition.
  BasisFactorization basis_factorization_;

  // Classes responsible for maintaining the data of the corresponding names.
  VariablesInfo variables_info_;
  VariableValues variable_values_;
  DualEdgeNorms dual_edge_norms_;
  PrimalEdgeNorms primal_edge_norms_;
  UpdateRow update_row_;
  ReducedCosts reduced_costs_;

  // Class holding the algorithms to choose the entering column during a simplex
  // pivot.
  EnteringVariable entering_variable_;

  // Temporary memory used by DualMinimize().
  std::vector<ColIndex> bound_flip_candidates_;
  std::vector<std::pair<RowIndex, ColIndex>> pair_to_ignore_;

  // Total number of iterations performed.
  uint64 num_iterations_;

  // Number of iterations performed during the first (feasibility) phase.
  uint64 num_feasibility_iterations_;

  // Number of iterations performed during the second (optimization) phase.
  uint64 num_optimization_iterations_;

  // Total time spent in Solve().
  double total_time_;

  // Time spent in the first (feasibility) phase.
  double feasibility_time_;

  // Time spent in the second (optimization) phase.
  double optimization_time_;

  // The internal deterministic time during the most recent call to
  // RevisedSimplex::AdvanceDeterministicTime.
  double last_deterministic_time_update_;

  // Statistics about the iterations done by Minimize().
  struct IterationStats : public StatsGroup {
    IterationStats()
        : StatsGroup("IterationStats"),
          total("total", this),
          normal("normal", this),
          bound_flip("bound_flip", this),
          degenerate("degenerate", this),
          degenerate_run_size("degenerate_run_size", this) {}
    TimeDistribution total;
    TimeDistribution normal;
    TimeDistribution bound_flip;
    TimeDistribution degenerate;
    IntegerDistribution degenerate_run_size;
  };
  IterationStats iteration_stats_;

  struct RatioTestStats : public StatsGroup {
    RatioTestStats()
        : StatsGroup("RatioTestStats"),
          bound_shift("bound_shift", this),
          abs_used_pivot("abs_used_pivot", this),
          abs_tested_pivot("abs_tested_pivot", this),
          abs_skipped_pivot("abs_skipped_pivot", this),
          direction_density("direction_density", this),
          leaving_choices("leaving_choices", this),
          num_perfect_ties("num_perfect_ties", this) {}
    DoubleDistribution bound_shift;
    DoubleDistribution abs_used_pivot;
    DoubleDistribution abs_tested_pivot;
    DoubleDistribution abs_skipped_pivot;
    RatioDistribution direction_density;
    IntegerDistribution leaving_choices;
    IntegerDistribution num_perfect_ties;
  };
  mutable RatioTestStats ratio_test_stats_;

  // Placeholder for all the function timing stats.
  // Mutable because we time const functions like ChooseLeavingVariableRow().
  mutable StatsGroup function_stats_;

  // Proto holding all the parameters of this algorithm.
  //
  // Note that parameters_ may actually change during a solve as the solver may
  // dynamically adapt some values. It is why we store the argument of the last
  // SetParameters() call in initial_parameters_ so the next Solve() can reset
  // it correctly.
  GlopParameters parameters_;
  GlopParameters initial_parameters_;

  // LuFactorization used to test if a pivot will cause the new basis to
  // not be factorizable.
  LuFactorization test_lu_;

  // Number of degenerate iterations made just before the current iteration.
  int num_consecutive_degenerate_iterations_;

  // Indicate if we are in the feasibility_phase (1st phase) or not.
  bool feasibility_phase_;

  // Indicates whether simplex ended due to the objective limit being reached.
  // Note that it's not enough to compare the final objective value with the
  // limit due to numerical issues (i.e., the limit which is reached within
  // given tolerance on the internal objective may no longer be reached when the
  // objective scaling and offset are taken into account).
  bool objective_limit_reached_;

  // Temporary SparseColumn used by ChooseLeavingVariableRow().
  SparseColumn leaving_candidates_;

  // Temporary vector used to hold the best leaving column candidates that are
  // tied using the current choosing criteria. We actually only store the tied
  // candidate #2, #3, ...; because the first tied candidate is remembered
  // anyway.
  std::vector<RowIndex> equivalent_leaving_choices_;

  // A random number generator.
  random_engine_t random_;

  // This is used by Polish().
  DenseRow integrality_scale_;

  DISALLOW_COPY_AND_ASSIGN(RevisedSimplex);
};

// Hides the details of the dictionary matrix implementation. In the future,
// GLOP will support generating the dictionary one row at a time without having
// to store the whole matrix in memory.
class RevisedSimplexDictionary {
 public:
  typedef RowMajorSparseMatrix::const_iterator ConstIterator;

  // RevisedSimplex cannot be passed const because we have to call a non-const
  // method ComputeDictionary.
  // TODO(user): Overload this to take RevisedSimplex* alone when the
  // caller would normally pass a nullptr for col_scales so this and
  // ComputeDictionary can take a const& argument.
  RevisedSimplexDictionary(const DenseRow* col_scales,
                           RevisedSimplex* revised_simplex)
      : dictionary_(
            ABSL_DIE_IF_NULL(revised_simplex)->ComputeDictionary(col_scales)),
        basis_vars_(ABSL_DIE_IF_NULL(revised_simplex)->GetBasisVector()) {}

  ConstIterator begin() const { return dictionary_.begin(); }
  ConstIterator end() const { return dictionary_.end(); }

  size_t NumRows() const { return dictionary_.size(); }

  // TODO(user): This function is a better fit for the future custom iterator.
  ColIndex GetBasicColumnForRow(RowIndex r) const { return basis_vars_[r]; }
  SparseRow GetRow(RowIndex r) const { return dictionary_[r]; }

 private:
  const RowMajorSparseMatrix dictionary_;
  const RowToColMapping basis_vars_;
  DISALLOW_COPY_AND_ASSIGN(RevisedSimplexDictionary);
};

// TODO(user): When a row-by-row generation of the dictionary is supported,
// implement DictionaryIterator class that would call it inside operator*().

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_GLOP_REVISED_SIMPLEX_H_
