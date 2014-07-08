//
// Storage classes for Linear Programs.
// LinearProgram stores the complete data for a Linear Program:
//   - objective coefficients and offset,
//   - cost coefficients,
//   - coefficient matrix,
//   - bounds for each variable,
//   - bounds for each constraint.

#ifndef OR_TOOLS_GLOP_LP_DATA_H_
#define OR_TOOLS_GLOP_LP_DATA_H_

#include <algorithm>  // for max
#include <map>
#include "base/hash.h"
#include <string>  // for std::string
#include <vector>  // for vector

#include "base/logging.h"  // for CHECK*
#include "base/macros.h"   // for DISALLOW_COPY_AND_ASSIGN, NULL
#include "base/int_type.h"
#include "base/int_type_indexed_vector.h"
#include "base/hash.h"
#include "glop/lp_types.h"
#include "glop/matrix_scaler.h"
#include "glop/sparse.h"
#include "util/fp_utils.h"

namespace operations_research {
namespace glop {

// --------------------------------------------------------
// LinearProgram
// --------------------------------------------------------
// This class is used to store a linear problem in a form accepted by LPSolver.
//
// In addition to the simple setters functions used to create such problems, the
// class also contains a few more advanced modification functions used primarly
// by preprocessors. A client shouldn't need to use them directly.
class LinearProgram {
 public:
  LinearProgram();

  // Clears, i.e. reset the object to its initial value.
  void Clear();

  // Name setter and getter.
  void SetName(const std::string& name) { name_ = name; }
  const std::string& name() const { return name_; }

  // Creates a new variable and returns its index.
  // By default, the column bounds will be [0, infinity).
  ColIndex CreateNewVariable();

  // Creates a new constraint and returns its index.
  // By default, the constraint bounds will be [0, 0].
  RowIndex CreateNewConstraint();

  // Same as CreateNewVariable() or CreateNewConstraint() but also assign an
  // immutable id to the variable or constraint so they can be retrieved later.
  // By default, the name is also set to this id, but it can be changed later
  // without changing the id.
  ColIndex FindOrCreateVariable(const std::string& variable_id);
  RowIndex FindOrCreateConstraint(const std::string& constraint_id);

  // Functions to set the name of a variable or constraint.
  void SetVariableName(ColIndex col, const std::string& name);
  void SetConstraintName(RowIndex row, const std::string& name);

  // Records the fact that the variable at column col must only take integer
  // values.
  // Note(user): For the time being, this is not handled. The continuous
  // relaxation of the problem (with integrality constraints removed) is solved
  // instead.
  // TODO(user): Improve the support of integer variables.
  void SetVariableIntegrality(ColIndex col, bool is_integer);

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

  // Defines the objective offset. It is 0.0 by default.
  void SetObjectiveOffset(Fractional objective_offset);

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

  // Returns a row vector of Booleans representing whether each variable is
  // constrained to be integer.
  const DenseBooleanRow& is_variable_integer() const {
    return is_variable_integer_;
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

  // Returns the objective offset, i.e. value of the objective when all
  // variables are set to zero.
  Fractional objective_offset() const { return objective_offset_; }

  // Tests if the solution is LP-feasible within the given tolerance,
  // i.e., satisfies all linear constraints within the absolute tolerance level.
  // The solution does not need to satisfy the integer constraints.
  // The solution must contain exactly GetNumberOfColumns() values and IsValid()
  // must evaluate to true (otherwise, false is returned).
  bool IsSolutionFeasible(const DenseRow& solution,
                          const Fractional absolute_tolerance) const;

  // A short std::string with the problem dimension.
  std::string GetDimensionString() const;

  // Returns a stringified LinearProgram. We use the LP file format used by
  // lp_solve (see http://lpsolve.sourceforge.net/5.1/index.htm).
  std::string Dump() const;

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

  // Adds slack variables to the problem for rows that are either free or are
  // lower- and upper- bounded by finite bounds that are not equal (range
  // constraint). This is done in such a way that in the modified problem, these
  // constraints will be equality constraints with both bounds set to 0.0.
  void AddSlackVariablesForFreeAndBoxedRows();

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

  // Populates the calling object with the given LinearProgram. If
  // keep_table_names is false, we do not keep the column/row name mappings
  // which are mainly useful when constructing a program (this is a bit faster).
  void PopulateFromLinearProgram(const LinearProgram& linear_program,
                                 bool keep_table_names);

  // Swaps the content of this LinearProgram with the one passed as argument.
  // Works in O(1).
  void Swap(LinearProgram* linear_program);

  // Removes the given column indices from the LinearProgram.
  // This needs to allocate O(num_variables) memory to update variable_table_.
  void DeleteColumns(const DenseBooleanRow& columns_to_delete);

  // Scales the problem using the given scaler.
  void Scale(SparseMatrixScaler* scaler);

  // Removes the given row indices from the LinearProgram.
  // This needs to allocate O(num_variables) memory.
  void DeleteRows(const DenseBooleanColumn& rows_to_delete);

  // Does basic checking on the linear program:
  // - returns false if some coefficient are NaNs.
  // - returns false if some coefficient other than the bounds are +/- infinity.
  // Note that these conditions are also guarded by DCHECK on each of the
  // SetXXX() function above.
  bool IsValid() const;

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
  DenseBooleanRow is_variable_integer_;

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
  hash_map<std::string, ColIndex> variable_table_;

  // Map used to find the index of a constraint based on its id.
  hash_map<std::string, RowIndex> constraint_table_;

  // Offset of the objective, i.e. value of the objective when all variables
  // are set to zero.
  Fractional objective_offset_;

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

  DISALLOW_COPY_AND_ASSIGN(LinearProgram);
};

// Helper function to check the bounds of the SetVariableBounds() and
// SetConstraintBounds() functions.
inline bool AreBoundsValid(Fractional lower_bound, Fractional upper_bound) {
  if (isnan(lower_bound)) return false;
  if (isnan(upper_bound)) return false;
  if (lower_bound == kInfinity && upper_bound == kInfinity) return false;
  if (lower_bound == -kInfinity && upper_bound == -kInfinity) return false;
  if (lower_bound > upper_bound) return false;
  return true;
}

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_GLOP_LP_DATA_H_
