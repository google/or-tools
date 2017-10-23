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

//
// Storage classes for Linear Programs.
//
// LinearProgram stores the complete data for a Linear Program:
//   - objective coefficients and offset,
//   - cost coefficients,
//   - coefficient matrix,
//   - bounds for each variable,
//   - bounds for each constraint.

#ifndef OR_TOOLS_LP_DATA_LP_DATA_H_
#define OR_TOOLS_LP_DATA_LP_DATA_H_

#include <algorithm>  // for max
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <string>  // for std::string
#include <vector>  // for vector

#include "ortools/base/logging.h"  // for CHECK*
#include "ortools/base/macros.h"   // for DISALLOW_COPY_AND_ASSIGN, NULL
#include "ortools/base/int_type.h"
#include "ortools/base/int_type_indexed_vector.h"
#include "ortools/base/hash.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/matrix_scaler.h"
#include "ortools/lp_data/sparse.h"
#include "ortools/util/fp_utils.h"

namespace operations_research {
namespace glop {

// The LinearProgram class is used to store a linear problem in a form
// accepted by LPSolver.
//
// In addition to the simple setter functions used to create such problems, the
// class also contains a few more advanced modification functions used primarily
// by preprocessors. A client shouldn't need to use them directly.
class LinearProgram {
 public:
  enum class VariableType {
    // The variable can take any value between and including its lower and upper
    // bound.
    CONTINUOUS,
    // The variable must only take integer values.
    INTEGER,
    // The variable is implied integer variable i.e. it was continuous variable
    // in the LP and was detected to take only integer values.
    IMPLIED_INTEGER
  };

  LinearProgram();

  // Clears, i.e. reset the object to its initial value.
  void Clear();

  // Name setter and getter.
  void SetName(const std::string& name) { name_ = name; }
  const std::string& name() const { return name_; }

  // Creates a new variable and returns its index.
  // By default, the column bounds will be [0, infinity).
  ColIndex CreateNewVariable();

  // Creates a new slack variable and returns its index. Do not use this method
  // to create non-slack variables.
  ColIndex CreateNewSlackVariable(bool is_integer_slack_variable,
                                  Fractional lower_bound,
                                  Fractional upper_bound, const std::string& name);

  // Creates a new constraint and returns its index.
  // By default, the constraint bounds will be [0, 0].
  RowIndex CreateNewConstraint();

  // Same as CreateNewVariable() or CreateNewConstraint() but also assign an
  // immutable id to the variable or constraint so they can be retrieved later.
  // By default, the name is also set to this id, but it can be changed later
  // without changing the id.
  //
  // Note that these ids are NOT copied over by the Populate*() functions.
  //
  // TODO(user): Move these and the two corresponding hash_table into a new
  // LinearProgramBuilder class to simplify the code of some functions like
  // DeleteColumns() here and make the behavior on copy clear? or simply remove
  // them as it is almost as easy to maintain a hash_table on the client side.
  ColIndex FindOrCreateVariable(const std::string& variable_id);
  RowIndex FindOrCreateConstraint(const std::string& constraint_id);

  // Functions to set the name of a variable or constraint. Note that you
  // won't be able to find those named variables/constraints with
  // FindOrCreate{Variable|Constraint}().
  // TODO(user): Add PopulateIdsFromNames() so names added via
  // Set{Variable|Constraint}Name() can be found.
  void SetVariableName(ColIndex col, const std::string& name);
  void SetConstraintName(RowIndex row, const std::string& name);

  // Set the type of the variable.
  void SetVariableType(ColIndex col, VariableType type);

  // Returns whether the variable at column col is constrained to be integer.
  bool IsVariableInteger(ColIndex col) const;

  // Returns whether the variable at column col must take binary values or not.
  bool IsVariableBinary(ColIndex col) const;

  // Defines lower and upper bounds for the variable at col. Note that the
  // bounds may be set to +/- infinity. The variable must have been created
  // before or this will crash in non-debug mode.
  void SetVariableBounds(ColIndex col, Fractional lower_bound,
                         Fractional upper_bound);

  // Defines lower and upper bounds for the constraint at row. Note that the
  // bounds may be set to +/- infinity. If the constraint wasn't created before,
  // all the rows from the current GetNumberOfRows() to the given row will be
  // created with a range [0,0].
  void SetConstraintBounds(RowIndex row, Fractional lower_bound,
                           Fractional upper_bound);

  // Defines the coefficient for col / row.
  void SetCoefficient(RowIndex row, ColIndex col, Fractional value);

  // Defines the objective coefficient of column col.
  // It is set to 0.0 by default.
  void SetObjectiveCoefficient(ColIndex col, Fractional value);

  // Define the objective offset (0.0 by default) and scaling factor (positive
  // and equal to 1.0 by default). This is mainly used for displaying purpose
  // and the real objective is factor * (objective + offset).
  void SetObjectiveOffset(Fractional objective_offset);
  void SetObjectiveScalingFactor(Fractional objective_scaling_factor);

  // Defines the optimization direction. When maximize is true (resp. false),
  // the objective is maximized (resp. minimized). The default is false.
  void SetMaximizationProblem(bool maximize);

  // Calls CleanUp() on each columns.
  // That is, removes duplicates, zeros, and orders the coefficients by row.
  void CleanUp();

  // Returns true if all the columns are ordered by rows and contain no
  // duplicates or zero entries (i.e. if IsCleanedUp() is true for all columns).
  bool IsCleanedUp() const;

  // Functions that return the name of a variable or constraint. If the name is
  // empty, they return a special name that depends on the index.
  std::string GetVariableName(ColIndex col) const;
  std::string GetConstraintName(RowIndex row) const;

  // Returns the type of variable.
  VariableType GetVariableType(ColIndex col) const;

  // Returns true (resp. false) when the problem is a maximization
  // (resp. minimization) problem.
  bool IsMaximizationProblem() const { return maximize_; }

  // Returns the underlying SparseMatrix or its transpose (which may need to be
  // computed).
  const SparseMatrix& GetSparseMatrix() const { return matrix_; }
  const SparseMatrix& GetTransposeSparseMatrix() const;

  // Some transformations are better done on the transpose representation. These
  // two functions are here for that. Note that calling the first function and
  // modifying the matrix does not change the result of any function in this
  // class until UseTransposeMatrixAsReference() is called. This is because the
  // transpose matrix is only used by GetTransposeSparseMatrix() and this
  // function will recompute the whole tranpose from the matrix. In particular,
  // do not call GetTransposeSparseMatrix() while you modify the matrix returned
  // by GetMutableTransposeSparseMatrix() otherwise all your changes will be
  // lost.
  //
  // IMPORTANT: The matrix dimension cannot change. Otherwise this will cause
  // problems. This is checked in debug mode when calling
  // UseTransposeMatrixAsReference().
  SparseMatrix* GetMutableTransposeSparseMatrix();
  void UseTransposeMatrixAsReference();

  // Release the memory used by the transpose matrix.
  void ClearTransposeMatrix();

  // Gets the underlying SparseColumn with the given index.
  // This is the same as GetSparseMatrix().column(col);
  const SparseColumn& GetSparseColumn(ColIndex col) const;

  // Gets a pointer to the underlying SparseColumn with the given index.
  SparseColumn* GetMutableSparseColumn(ColIndex col);

  // Returns the number of variables.
  ColIndex num_variables() const { return matrix_.num_cols(); }

  // Returns the number of constraints.
  RowIndex num_constraints() const { return matrix_.num_rows(); }

  // Returns the number of entries in the linear program matrix.
  EntryIndex num_entries() const { return matrix_.num_entries(); }

  // Return the lower bounds (resp. upper bounds) of constraints as a column
  // vector. Note that the bound values may be +/- infinity.
  const DenseColumn& constraint_lower_bounds() const {
    return constraint_lower_bounds_;
  }
  const DenseColumn& constraint_upper_bounds() const {
    return constraint_upper_bounds_;
  }

  // Returns the objective coefficients (or cost) of variables as a row vector.
  const DenseRow& objective_coefficients() const {
    return objective_coefficients_;
  }

  // Return the lower bounds (resp. upper bounds) of variables as a row vector.
  // Note that the bound values may be +/- infinity.
  const DenseRow& variable_lower_bounds() const {
    return variable_lower_bounds_;
  }
  const DenseRow& variable_upper_bounds() const {
    return variable_upper_bounds_;
  }

  // Returns a row vector of VariableType representing types of variables.
  const StrictITIVector<ColIndex, VariableType> variable_types() const {
    return variable_types_;
  }

  // Returns a list (technically a vector) of the ColIndices of the integer
  // variables. This vector is lazily computed.
  const std::vector<ColIndex>& IntegerVariablesList() const;

  // Returns a list (technically a vector) of the ColIndices of the binary
  // integer variables. This vector is lazily computed.
  const std::vector<ColIndex>& BinaryVariablesList() const;

  // Returns a list (technically a vector) of the ColIndices of the non-binary
  // integer variables. This vector is lazily computed.
  const std::vector<ColIndex>& NonBinaryVariablesList() const;

  // Returns the objective coefficient (or cost) of the given variable for the
  // minimization version of the problem. That is, this is the same as
  // GetObjectiveCoefficient() for a minimization problem and the opposite for a
  // maximization problem.
  Fractional GetObjectiveCoefficientForMinimizationVersion(ColIndex col) const;

  // Returns the objective offset and scaling factor.
  Fractional objective_offset() const { return objective_offset_; }
  Fractional objective_scaling_factor() const {
    return objective_scaling_factor_;
  }

  // Tests if the solution is LP-feasible within the given tolerance,
  // i.e., satisfies all linear constraints within the absolute tolerance level.
  // The solution does not need to satisfy the integer constraints.
  bool SolutionIsLPFeasible(const DenseRow& solution,
                            Fractional absolute_tolerance) const;

  // Tests if the solution is integer within the given tolerance, i.e., all
  // integer variables have integer values within the absolute tolerance level.
  // The solution does not need to satisfy the linear constraints.
  bool SolutionIsInteger(const DenseRow& solution,
                         Fractional absolute_tolerance) const;

  // Tests if the solution is both LP-feasible and integer within the tolerance.
  bool SolutionIsMIPFeasible(const DenseRow& solution,
                             Fractional absolute_tolerance) const;

  // Functions to translate the sum(solution * objective_coefficients()) to
  // the real objective of the problem and back. Note that these can also
  // be used to translate bounds of the objective in the same way.
  Fractional ApplyObjectiveScalingAndOffset(Fractional value) const;
  Fractional RemoveObjectiveScalingAndOffset(Fractional value) const;

  // A short std::string with the problem dimension.
  std::string GetDimensionString() const;

  // A short line with some stats on the objective coefficients.
  std::string GetObjectiveStatsString() const;

  // Returns a stringified LinearProgram. We use the LP file format used by
  // lp_solve (see http://lpsolve.sourceforge.net/5.1/index.htm).
  std::string Dump() const;

  // Returns a std::string that contains the provided solution of the LP in the
  // format var1 = X, var2 = Y, var3 = Z, ...
  std::string DumpSolution(const DenseRow& variable_values) const;


  // Returns a comma-separated std::string of integers containing (in that order)
  // num_constraints_, num_variables_in_file_, num_entries_,
  // num_objective_non_zeros_, num_rhs_non_zeros_, num_less_than_constraints_,
  // num_greater_than_constraints_, num_equal_constraints_,
  // num_range_constraints_, num_non_negative_variables_, num_boxed_variables_,
  // num_free_variables_, num_fixed_variables_, num_other_variables_
  // Very useful for reporting in the way used in journal articles.
  std::string GetProblemStats() const;

  // Returns a std::string containing the same information as with GetProblemStats(),
  // but in a much more human-readable form, for example:
  //      Number of rows                               : 27
  //      Number of variables in file                  : 32
  //      Number of entries (non-zeros)                : 83
  //      Number of entries in the objective           : 5
  //      Number of entries in the right-hand side     : 7
  //      Number of <= constraints                     : 19
  //      Number of >= constraints                     : 0
  //      Number of = constraints                      : 8
  //      Number of range constraints                  : 0
  //      Number of non-negative variables             : 32
  //      Number of boxed variables                    : 0
  //      Number of free variables                     : 0
  //      Number of fixed variables                    : 0
  //      Number of other variables                    : 0
  std::string GetPrettyProblemStats() const;

  // Returns a comma-separated std::string of numbers containing (in that order)
  // fill rate, max number of entries (length) in a row, average row length,
  // standard deviation of row length, max column length, average column length,
  // standard deviation of column length
  // Useful for profiling algorithms.
  //
  // TODO(user): Theses are statistics about the underlying matrix and should be
  // moved to SparseMatrix.
  std::string GetNonZeroStats() const;

  // Returns a std::string containing the same information as with GetNonZeroStats(),
  // but in a much more human-readable form, for example:
  //      Fill rate                                    : 9.61%
  //      Entries in row (Max / average / std, dev.)   : 9 / 3.07 / 1.94
  //      Entries in column (Max / average / std, dev.): 4 / 2.59 / 0.96
  std::string GetPrettyNonZeroStats() const;

  // Adds slack variables to the problem for all rows which don't have slack
  // variables. The new slack variables have bounds set to opposite of the
  // bounds of the corresponding constraint, and changes all constraints to
  // equality constraints with both bounds set to 0.0. If a constraint uses only
  // integer variables and all their coefficients are integer, it will mark the
  // slack variable as integer too.
  //
  // It is an error to call CreateNewVariable() or CreateNewConstraint() on a
  // linear program on which this method was called.
  //
  // Note that many of the slack variables may not be useful at all, but in
  // order not to recompute the matrix from one Solve() to the next, we always
  // include all of them for a given lp matrix.
  //
  // TODO(user): investigate the impact on the running time. It seems low
  // because we almost never iterate on fixed variables.
  void AddSlackVariablesWhereNecessary(bool detect_integer_constraints);

  // Returns the index of the first slack variable in the linear program.
  // Returns kInvalidCol if slack variables were not injected into the problem
  // yet.
  ColIndex GetFirstSlackVariable() const;

  // Returns the index of the slack variable corresponding to the given
  // constraint. Returns kInvalidCol if slack variables were not injected into
  // the problem yet.
  ColIndex GetSlackVariable(RowIndex row) const;

  // Populates the calling object with the dual of the LinearProgram passed as
  // parameter.
  // For the general form that we solve,
  // min c.x
  // s.t.  A_1 x = b_1
  //       A_2 x <= b_2
  //       A_3 x >= b_3
  //       l <= x <= u
  // With x: n-column of unknowns
  // l,u: n-columns of bound coefficients
  // c: n-row of cost coefficients
  // A_i: m_i×n-matrix of coefficients
  // b_i: m_i-column of right-hand side coefficients
  //
  // The dual is
  //
  // max b_1.y_1 + b_2.y_2 + b_3.y_3 + l.v + u.w
  // s.t. y_1 A_1 + y_2 A_2 + y_3 A_3 + v + w = c
  //       y_1 free, y_2 <= 0, y_3 >= 0, v >= 0, w <= 0
  // With:
  // y_i: m_i-row of unknowns
  // v,w: n-rows of unknowns
  //
  // If range constraints are present, each of the corresponding row is
  // duplicated (with one becoming lower bounded and the other upper bounded).
  // For such ranged row in the primal, duplicated_rows[row] is set to the
  // column index in the dual of the corresponding column duplicate. For
  // non-ranged row, duplicated_rows[row] is set to kInvalidCol.
  //
  // IMPORTANT: The linear_program argument must not have any free constraints.
  //
  // IMPORTANT: This function always interprets the argument in its minimization
  // form. So the dual solution of the dual needs to be negated if we want to
  // compute the solution of a maximization problem given as an argument.
  //
  // TODO(user): Do not interpret as a minimization problem?
  void PopulateFromDual(const LinearProgram& linear_program,
                        RowToColMapping* duplicated_rows);

  // Populates the calling object with the given LinearProgram.
  void PopulateFromLinearProgram(const LinearProgram& linear_program);

  // Populates the calling object with the given LinearProgram while permuting
  // variables and constraints. This is useful mainly for testing to generate
  // a model with the same optimal objective value.
  void PopulateFromPermutedLinearProgram(
      const LinearProgram& lp, const RowPermutation& row_permutation,
      const ColumnPermutation& col_permutation);

  // Populates the calling object with the variables of the given LinearProgram.
  // The function preserves the bounds, the integrality, the names of the
  // variables and their objective coefficients. No constraints are copied (the
  // matrix in the destination has 0 rows).
  void PopulateFromLinearProgramVariables(const LinearProgram& linear_program);

  // Adds constraints to the linear program. The constraints are specified using
  // a sparse matrix of the coefficients, and vectors that represent the
  // left-hand side and the right-hand side of the constraints, i.e.
  // left_hand_sides <= coefficients * variables <= right_hand_sides.
  // The sizes of the columns and the names must be the same as the number of
  // rows of the sparse matrix; the number of columns of the matrix must be
  // equal to the number of variables of the linear program.
  void AddConstraints(const SparseMatrix& coefficients,
                      const DenseColumn& left_hand_sides,
                      const DenseColumn& right_hand_sides,
                      const StrictITIVector<RowIndex, std::string>& names);

  // Calls the AddConstraints method. After adding the constraints it adds slack
  // variables to the constraints.
  void AddConstraintsWithSlackVariables(
      const SparseMatrix& coefficients, const DenseColumn& left_hand_sides,
      const DenseColumn& right_hand_sides,
      const StrictITIVector<RowIndex, std::string>& names,
      bool detect_integer_constraints_for_slack);

  // Swaps the content of this LinearProgram with the one passed as argument.
  // Works in O(1).
  void Swap(LinearProgram* linear_program);

  // Removes the given column indices from the LinearProgram.
  // This needs to allocate O(num_variables) memory to update variable_table_.
  void DeleteColumns(const DenseBooleanRow& columns_to_delete);

  // Removes slack variables from the linear program. The method restores the
  // bounds on constraints from the bounds of the slack variables, resets the
  // index of the first slack variable, and removes the relevant columns from
  // the matrix.
  void DeleteSlackVariables();

  // Scales the problem using the given scaler.
  void Scale(SparseMatrixScaler* scaler);

  // While Scale() makes sure the coefficients inside the linear program matrix
  // are in [-1, 1], the objective coefficients, variable bounds and constraint
  // bounds can still take large values (originally or due to the matrix
  // scaling).
  //
  // It makes a lot of sense to also scale them given that internally we use
  // absolute tolerances, and that it is nice to have the same behavior if users
  // scale their problems. For instance one could change the unit of ALL the
  // variables from Bytes to MBytes if they denote memory quantities. Or express
  // a cost in dollars instead of thousands of dollars.
  //
  // Here, we are quite prudent and just make sure that the range of the
  // non-zeros magnitudes contains one. So for instance if all non-zeros costs
  // are in [1e4, 1e6], we will divide them by 1e4 so that the new range is
  // [1, 1e2].
  //
  // TODO(user): Another more aggressive idea is to set the median/mean/geomean
  // of the magnitudes to one. Investigate if this leads to better results. It
  // does look more robust.
  //
  // Both functions update objective_scaling_factor()/objective_offset() and
  // return the scaling coefficient so that:
  // - For ScaleObjective(), the old coefficients can be retrieved by
  //   multiplying the new ones by the returned factor.
  // - For ScaleBounds(), the old variable and constraint bounds can be
  //   retrieved by multiplying the new ones by the returned factor.
  Fractional ScaleObjective();
  Fractional ScaleBounds();

  // Removes the given row indices from the LinearProgram.
  // This needs to allocate O(num_variables) memory.
  void DeleteRows(const DenseBooleanColumn& rows_to_delete);

  // Does basic checking on the linear program:
  // - returns false if some coefficient are NaNs.
  // - returns false if some coefficient other than the bounds are +/- infinity.
  // Note that these conditions are also guarded by DCHECK on each of the
  // SetXXX() function above.
  bool IsValid() const;

  // Updates the bounds of the variables to the intersection of their original
  // bounds and the bounds specified by variable_lower_bounds and
  // variable_upper_bounds. If the new bounds of all variables are non-empty,
  // returns true; otherwise, returns false.
  bool UpdateVariableBoundsToIntersection(
      const DenseRow& variable_lower_bounds,
      const DenseRow& variable_upper_bounds);

  // Returns true if the linear program is in equation form Ax = 0 and all slack
  // variables have been added. This is also called "computational form" in some
  // of the literature.
  bool IsInEquationForm() const;

  // Returns true if all integer variables in the linear program have strictly
  // integer bounds.
  bool BoundsOfIntegerVariablesAreInteger(Fractional tolerance) const;

  // Returns true if all integer constraints in the linear program have strictly
  // integer bounds.
  bool BoundsOfIntegerConstraintsAreInteger(Fractional tolerance) const;

  // Advanced usage. Bypass the costly call to CleanUp() when we known that the
  // change we made kept the matrix columns "clean" (see the comment of
  // CleanUp()). This is unsafe but can save a big chunk of the running time
  // when one does a small amount of incremental changes to the problem (like
  // adding a new row with no duplicates or zero entries).
  void NotifyThatColumnsAreClean() {
    DCHECK(matrix_.IsCleanedUp());
    columns_are_known_to_be_clean_ = true;
  }

 private:
  // A helper function that updates the vectors integer_variables_list_,
  // binary_variables_list_, and non_binary_variables_list_.
  void UpdateAllIntegerVariableLists() const;

  // A helper function to format problem statistics. Used by GetProblemStats()
  // and GetPrettyProblemStats().
  std::string ProblemStatFormatter(const char* format) const;

  // A helper function to format non-zero statistics. Used by GetNonZeroStats()
  // and GetPrettyNonZeroStats().
  std::string NonZeroStatFormatter(const char* format) const;

  // Resizes all row vectors to include index 'row'.
  void ResizeRowsIfNeeded(RowIndex row);

  // Populates the definitions of variables, name and objective in the calling
  // linear program with the data from the given linear program. The method does
  // not touch the data structures for storing constraints.
  void PopulateNameObjectiveAndVariablesFromLinearProgram(
      const LinearProgram& linear_program);

  // Stores the linear program coefficients.
  SparseMatrix matrix_;

  // The transpose of matrix_. This will be lazily recomputed by
  // GetTransposeSparseMatrix() if tranpose_matrix_is_consistent_ is false.
  mutable SparseMatrix transpose_matrix_;

  // Constraint related quantities.
  DenseColumn constraint_lower_bounds_;
  DenseColumn constraint_upper_bounds_;
  StrictITIVector<RowIndex, std::string> constraint_names_;

  // Variable related quantities.
  DenseRow objective_coefficients_;
  DenseRow variable_lower_bounds_;
  DenseRow variable_upper_bounds_;
  StrictITIVector<ColIndex, std::string> variable_names_;
  StrictITIVector<ColIndex, VariableType> variable_types_;

  // The vector of the indices of variables constrained to be integer.
  // Note(user): the set of indices in integer_variables_list_ is the union
  // of the set of indices in binary_variables_list_ and of the set of indices
  // in non_binary_variables_list_ below.
  mutable std::vector<ColIndex> integer_variables_list_;

  // The vector of the indices of variables constrained to be binary.
  mutable std::vector<ColIndex> binary_variables_list_;

  // The vector of the indices of variables constrained to be integer, but not
  // binary.
  mutable std::vector<ColIndex> non_binary_variables_list_;

  // Map used to find the index of a variable based on its id.
  std::unordered_map<std::string, ColIndex> variable_table_;

  // Map used to find the index of a constraint based on its id.
  std::unordered_map<std::string, RowIndex> constraint_table_;

  // Offset of the objective, i.e. value of the objective when all variables
  // are set to zero.
  Fractional objective_offset_;
  Fractional objective_scaling_factor_;

  // Boolean true (resp. false) when the problem is a maximization
  // (resp. minimization) problem.
  bool maximize_;

  // Boolean to speed-up multiple calls to IsCleanedUp() or
  // CleanUp(). Mutable so IsCleanedUp() can be const.
  mutable bool columns_are_known_to_be_clean_;

  // Whether transpose_matrix_ is guaranteed to be the transpose of matrix_.
  mutable bool transpose_matrix_is_consistent_;

  // Whether integer_variables_list_ is consistent with the current
  // LinearProgram.
  mutable bool integer_variables_list_is_consistent_;

  // The name of the LinearProgram.
  std::string name_;

  // The index of the first slack variable added to the linear program by
  // LinearProgram::AddSlackVariablesForAllRows().
  ColIndex first_slack_variable_;

  DISALLOW_COPY_AND_ASSIGN(LinearProgram);
};

// --------------------------------------------------------
// ProblemSolution
// --------------------------------------------------------
// Contains the solution of a LinearProgram as returned by a preprocessor.
struct ProblemSolution {
  ProblemSolution(RowIndex num_rows, ColIndex num_cols)
      : status(ProblemStatus::OPTIMAL),
        primal_values(num_cols, 0.0),
        dual_values(num_rows, 0.0),
        variable_statuses(num_cols, VariableStatus::FREE),
        constraint_statuses(num_rows, ConstraintStatus::FREE) {}
  // The solution status.
  ProblemStatus status;

  // The actual primal/dual solution values. This is what most clients will
  // need, and this is enough for LPSolver to easily check the optimality.
  DenseRow primal_values;
  DenseColumn dual_values;

  // The status of the variables and constraints which is difficult to
  // reconstruct from the solution values alone. Some remarks:
  //  - From this information alone, by factorizing the basis, it is easy to
  //    reconstruct the primal and dual values.
  //  - The main difficulty to construct this from the solution values is to
  //    reconstruct the optimal basis if some basic variables are exactly at
  //    one of their bounds (and their reduced costs are close to zero).
  //  - The non-basic information (VariableStatus::FIXED_VALUE,
  //    VariableStatus::AT_LOWER_BOUND, VariableStatus::AT_UPPER_BOUND,
  //    VariableStatus::FREE) is easy to construct for variables (because
  //    they are at their exact bounds). They can be guessed for constraints
  //    (here a small precision error is unavoidable). However, it is useful to
  //    carry this exact information during post-solve.
  VariableStatusRow variable_statuses;
  ConstraintStatusColumn constraint_statuses;

  std::string DebugString() const;
};

// Helper function to check the bounds of the SetVariableBounds() and
// SetConstraintBounds() functions.
inline bool AreBoundsValid(Fractional lower_bound, Fractional upper_bound) {
  if (std::isnan(lower_bound)) return false;
  if (std::isnan(upper_bound)) return false;
  if (lower_bound == kInfinity && upper_bound == kInfinity) return false;
  if (lower_bound == -kInfinity && upper_bound == -kInfinity) return false;
  if (lower_bound > upper_bound) return false;
  return true;
}

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_LP_DATA_LP_DATA_H_
