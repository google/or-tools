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

//
// This file contains the presolving code for a LinearProgram.
//
// A classical reference is:
// E. D. Andersen, K. D. Andersen, "Presolving in linear programming.",
// Mathematical Programming 71 (1995) 221-245.

#ifndef OR_TOOLS_GLOP_PREPROCESSOR_H_
#define OR_TOOLS_GLOP_PREPROCESSOR_H_

#include <memory>

#include "glop/parameters.pb.h"
#include "glop/revised_simplex.h"
#include "lp_data/lp_data.h"
#include "lp_data/lp_types.h"
#include "lp_data/matrix_scaler.h"

namespace operations_research {
namespace glop {

// --------------------------------------------------------
// Preprocessor
// --------------------------------------------------------
// This is the base class for preprocessors.
//
// TODO(user): On most preprocessors, calling Run() more than once will not work
// as expected. Fix? or document and crash in debug if this happens.
class Preprocessor {
 public:
  Preprocessor();
  virtual ~Preprocessor();

  // Runs the preprocessor by modifying the given linear program. Returns true
  // if a postsolve step will be needed (i.e. RecoverSolution() is not the
  // identity function). Also updates status_ to something different from
  // ProblemStatus::INIT if the problem was solved (including bad statuses
  // like ProblemStatus::ABNORMAL, ProblemStatus::INFEASIBLE, etc.).
  virtual bool Run(LinearProgram* linear_program, TimeLimit* time_limit) = 0;

  // Stores the optimal solution of the linear program that was passed to
  // Run(). The given solution needs to be set to the optimal solution of the
  // linear program "modified" by Run().
  virtual void RecoverSolution(ProblemSolution* solution) const = 0;

  // Returns the status of the preprocessor.
  // A status different from ProblemStatus::INIT means that the problem is
  // solved and there is not need to call subsequent preprocessors.
  ProblemStatus status() const { return status_; }

  // Stores the parameters for use by the different preprocessors.
  void SetParameters(const GlopParameters& parameters) {
    parameters_ = parameters;
  }

  // Some preprocessors only need minimal changes when used with integer
  // variables in a MIP context. Setting this to true allows to consider integer
  // variables as integer in these preprocessors.
  //
  // Not all preprocessors handle integer variables correctly, calling this
  // function on them will cause a LOG(FATAL).
  virtual void UseInMipContext() { in_mip_context_ = true; }

 protected:
  // Returns true if a is less than b (or slighlty greater than b with a given
  // tolerance).
  bool IsSmallerWithinFeasibilityTolerance(Fractional a, Fractional b) const {
    return ::operations_research::IsSmallerWithinTolerance(
        a, b, parameters_.solution_feasibility_tolerance());
  }
  bool IsSmallerWithinPreprocessorZeroTolerance(Fractional a,
                                                Fractional b) const {
    // TODO(user): use an absolute tolerance here to be even more defensive?
    return ::operations_research::IsSmallerWithinTolerance(
        a, b, parameters_.preprocessor_zero_tolerance());
  }

  ProblemStatus status_;
  GlopParameters parameters_;
  bool in_mip_context_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Preprocessor);
};

// --------------------------------------------------------
// MainLpPreprocessor
// --------------------------------------------------------
// This is the main LP preprocessor responsible for calling all the other
// preprocessors in this file, possibly more than once.
class MainLpPreprocessor : public Preprocessor {
 public:
  MainLpPreprocessor() {}
  ~MainLpPreprocessor() override {}

  bool Run(LinearProgram* linear_program, TimeLimit* time_limit) final;
  void RecoverSolution(ProblemSolution* solution) const override;

 private:
  // Runs the given preprocessor and push it on preprocessors_ for the postsolve
  // step when needed.
  void RunAndPushIfRelevant(std::unique_ptr<Preprocessor> preprocessor,
                            const std::string& name, TimeLimit* time_limit,
                            LinearProgram* lp);

  // Stack of preprocessors currently applied to the lp that needs postsolve.
  //
  // TODO(user): This is mutable so that the preprocessor can be freed as soon
  // as their RecoverSolution() is called. Make RecoverSolution() non-const or
  // remove this optimization?
  mutable std::vector<std::unique_ptr<Preprocessor>> preprocessors_;

  // Initial dimension of the lp given to Run(), for displaying purpose.
  EntryIndex initial_num_entries_;
  RowIndex initial_num_rows_;
  ColIndex initial_num_cols_;

  DISALLOW_COPY_AND_ASSIGN(MainLpPreprocessor);
};

// --------------------------------------------------------
// ColumnDeletionHelper
// --------------------------------------------------------
// Help preprocessors deal with column deletion.
class ColumnDeletionHelper {
 public:
  ColumnDeletionHelper() {}

  // Remember the given column as "deleted" so that it can later be restored
  // by RestoreDeletedColumns(). Optionally, the caller may indicate the
  // value and status of the corresponding variable so that it is automatically
  // restored; if they don't then the restored value and status will be junk
  // and must be set by the caller.
  //
  // The actual deletion is done by LinearProgram::DeleteColumns().
  void MarkColumnForDeletion(ColIndex col);
  void MarkColumnForDeletionWithState(ColIndex col, Fractional value,
                                      VariableStatus status);

  // From a solution omitting the deleted column, expands it and inserts the
  // deleted columns. If values and statuses for the corresponding variables
  // were saved, they'll be restored.
  void RestoreDeletedColumns(ProblemSolution* solution) const;

  // Returns whether or not the given column is marked for deletion.
  bool IsColumnMarked(ColIndex col) const {
    return col < is_column_deleted_.size() && is_column_deleted_[col];
  }

  // Returns a Boolean vector of the column to be deleted.
  const DenseBooleanRow& GetMarkedColumns() const { return is_column_deleted_; }

  // Returns true if no columns have been marked for deletion.
  bool IsEmpty() const { return is_column_deleted_.empty(); }

  // Restores the class to its initial state.
  void Clear();

  // Returns the value that will be restored by
  // RestoreDeletedColumnInSolution(). Note that only the marked position value
  // make sense.
  const DenseRow& GetStoredValue() const { return stored_value_; }

 private:
  DenseBooleanRow is_column_deleted_;

  // Note that this vector has the same size as is_column_deleted_ and that
  // the value of the variable corresponding to a deleted column col is stored
  // at position col. Values of columns not deleted are not used. We use this
  // data structure so columns can be deleted in any order if needed.
  DenseRow stored_value_;
  VariableStatusRow stored_status_;
  DISALLOW_COPY_AND_ASSIGN(ColumnDeletionHelper);
};

// --------------------------------------------------------
// RowDeletionHelper
// --------------------------------------------------------
// Help preprocessors deal with row deletion.
class RowDeletionHelper {
 public:
  RowDeletionHelper() {}

  // Returns true if no rows have been marked for deletion.
  bool IsEmpty() const { return is_row_deleted_.empty(); }

  // Restores the class to its initial state.
  void Clear();

  // Adds a deleted row to the helper.
  void MarkRowForDeletion(RowIndex row);

  // If the given row was marked for deletion, unmark it.
  void UnmarkRow(RowIndex row);

  // Returns a Boolean vector of the row to be deleted.
  const DenseBooleanColumn& GetMarkedRows() const;

  // Returns whether or not the given row is marked for deletion.
  bool IsRowMarked(RowIndex row) const {
    return row < is_row_deleted_.size() && is_row_deleted_[row];
  }

  // From a solution without the deleted rows, expand it by restoring
  // the deleted rows to a VariableStatus::BASIC status with 0.0 value.
  // This latter value is important, many preprocessors rely on it.
  void RestoreDeletedRows(ProblemSolution* solution) const;

 private:
  DenseBooleanColumn is_row_deleted_;

  DISALLOW_COPY_AND_ASSIGN(RowDeletionHelper);
};

// --------------------------------------------------------
// EmptyColumnPreprocessor
// --------------------------------------------------------
// Removes the empty columns from the problem.
class EmptyColumnPreprocessor : public Preprocessor {
 public:
  EmptyColumnPreprocessor() {}
  ~EmptyColumnPreprocessor() final {}
  bool Run(LinearProgram* linear_program, TimeLimit* time_limit) final;
  void RecoverSolution(ProblemSolution* solution) const final;

 private:
  ColumnDeletionHelper column_deletion_helper_;
  DISALLOW_COPY_AND_ASSIGN(EmptyColumnPreprocessor);
};

// --------------------------------------------------------
// ProportionalColumnPreprocessor
// --------------------------------------------------------
// TODO(user): For now this preprocessor just logs the number of proportional
// columns. Do something with this information.
//
// Removes the proportional columns from the problem when possible. Two columns
// are proportional if one is a non-zero scalar multiple of the other.
//
// Note that in the linear programming literature, two proportional columns are
// usually called duplicates. The notion is the same once the problem has been
// scaled. However, during presolve the columns can't be assumed to be scaled,
// so it makes sense to use the more general notion of proportional columns.
class ProportionalColumnPreprocessor : public Preprocessor {
 public:
  ProportionalColumnPreprocessor() {}
  ~ProportionalColumnPreprocessor() final {}
  bool Run(LinearProgram* linear_program, TimeLimit* time_limit) final;
  void RecoverSolution(ProblemSolution* solution) const final;
  void UseInMipContext() final { LOG(FATAL) << "Not implemented."; }

 private:
  // Postsolve information about proportional columns with the same scaled cost
  // that were merged during presolve.

  // The proportionality factor of each column. If two columns are proportional
  // with factor p1 and p2 then p1 times the first column is the same as p2
  // times the second column.
  DenseRow column_factors_;

  // If merged_columns_[col] != kInvalidCol, then column col has been merged
  // into the column merged_columns_[col].
  ColMapping merged_columns_;

  // The old and new variable bounds.
  DenseRow lower_bounds_;
  DenseRow upper_bounds_;
  DenseRow new_lower_bounds_;
  DenseRow new_upper_bounds_;

  ColumnDeletionHelper column_deletion_helper_;

  DISALLOW_COPY_AND_ASSIGN(ProportionalColumnPreprocessor);
};

// --------------------------------------------------------
// ProportionalRowPreprocessor
// --------------------------------------------------------
// Removes the proportional rows from the problem.
// The linear programming literature also calls such rows duplicates, see the
// same remark above for columns in ProportionalColumnPreprocessor.
class ProportionalRowPreprocessor : public Preprocessor {
 public:
  ProportionalRowPreprocessor() {}
  ~ProportionalRowPreprocessor() final {}
  bool Run(LinearProgram* linear_program, TimeLimit* time_limit) final;
  void RecoverSolution(ProblemSolution* solution) const final;

 private:
  // Informations about proportional rows, only filled for such rows.
  DenseColumn row_factors_;
  RowMapping upper_bound_sources_;
  RowMapping lower_bound_sources_;

  bool lp_is_maximization_problem_;
  RowDeletionHelper row_deletion_helper_;
  DISALLOW_COPY_AND_ASSIGN(ProportionalRowPreprocessor);
};

// --------------------------------------------------------
// SingletonPreprocessor
// --------------------------------------------------------
// Removes as many singleton rows and singleton columns as possible from the
// problem. Note that not all types of singleton columns can be removed. See the
// comments below on the SingletonPreprocessor functions for more details.
//
// TODO(user): Generalize the design used in this preprocessor to a general
// "propagation" framework in order to apply as many reductions as possible in
// an efficient manner.

// Holds a triplet (row, col, coefficient).
struct MatrixEntry {
  MatrixEntry(RowIndex _row, ColIndex _col, Fractional _coeff)
      : row(_row), col(_col), coeff(_coeff) {}
  RowIndex row;
  ColIndex col;
  Fractional coeff;
};

// Stores the information needed to undo a singleton row/column deletion.
class SingletonUndo {
 public:
  // The type of a given operation.
  typedef enum {
    ZERO_COST_SINGLETON_COLUMN,
    SINGLETON_ROW,
    SINGLETON_COLUMN_IN_EQUALITY,
    MAKE_CONSTRAINT_AN_EQUALITY,
  } OperationType;

  // Stores the information, which together with the field deleted_columns_ and
  // deleted_rows_ of SingletonPreprocessor, are needed to undo an operation
  // with the given type. Note that all the arguments must refer to the linear
  // program BEFORE the operation is applied.
  SingletonUndo(OperationType type, const LinearProgram& linear_program,
                MatrixEntry e, ConstraintStatus status);

  // Undo the operation saved in this class, taking into account the deleted
  // columns and rows passed by the calling instance of SingletonPreprocessor.
  // Note that the operations must be undone in the reverse order of the one
  // in which they were applied.
  void Undo(const GlopParameters& parameters,
            const SparseMatrix& deleted_columns,
            const SparseMatrix& deleted_rows, ProblemSolution* solution) const;

 private:
  // Actual undo functions for each OperationType.
  // Undo() just calls the correct one.
  void SingletonRowUndo(const SparseMatrix& deleted_columns,
                        ProblemSolution* solution) const;
  void ZeroCostSingletonColumnUndo(const GlopParameters& parameters,
                                   const SparseMatrix& deleted_rows,
                                   ProblemSolution* solution) const;
  void SingletonColumnInEqualityUndo(const GlopParameters& parameters,
                                     const SparseMatrix& deleted_rows,
                                     ProblemSolution* solution) const;
  void MakeConstraintAnEqualityUndo(ProblemSolution* solution) const;

  // All the information needed during undo.
  OperationType type_;
  bool is_maximization_;
  MatrixEntry e_;
  Fractional cost_;

  // TODO(user): regroup the pair (lower bound, upper bound) in a bound class?
  Fractional variable_lower_bound_;
  Fractional variable_upper_bound_;
  Fractional constraint_lower_bound_;
  Fractional constraint_upper_bound_;

  // This in only used with MAKE_CONSTRAINT_AN_EQUALITY undo.
  // TODO(user): Clean that up using many Undo classes and virtual functions.
  ConstraintStatus constraint_status_;
};

// Deletes as many singleton rows or singleton columns as possible. Note that
// each time we delete a row or a column, new singletons may be created.
class SingletonPreprocessor : public Preprocessor {
 public:
  SingletonPreprocessor() {}
  ~SingletonPreprocessor() final {}
  bool Run(LinearProgram* linear_program, TimeLimit* time_limit) final;
  void RecoverSolution(ProblemSolution* solution) const final;
  void UseInMipContext() final { LOG(FATAL) << "Not implemented."; }

 private:
  // Returns the MatrixEntry of the given singleton row or column, taking into
  // account the rows and columns that were already deleted.
  MatrixEntry GetSingletonColumnMatrixEntry(ColIndex col,
                                            const SparseMatrix& matrix);
  MatrixEntry GetSingletonRowMatrixEntry(RowIndex row,
                                         const SparseMatrix& matrix_transpose);

  // A singleton row can always be removed by changing the corresponding
  // variable bounds to take into account the bounds on this singleton row.
  void DeleteSingletonRow(MatrixEntry e, LinearProgram* lp);

  // Internal operation when removing a zero-cost singleton column corresponding
  // to the given entry. This modifies the constraint bounds to take into acount
  // the bounds of the corresponding variable.
  void UpdateConstraintBoundsWithVariableBounds(MatrixEntry e,
                                                LinearProgram* lp);


  // A singleton column with a cost of zero can always be removed by changing
  // the corresponding constraint bounds to take into acount the bound of this
  // singleton column.
  void DeleteZeroCostSingletonColumn(const SparseMatrix& matrix_transpose,
                                     MatrixEntry e, LinearProgram* lp);

  // Returns true if the constraint associated to the given singleton column was
  // an equality or could be made one:
  // If a singleton variable is free in a direction that improves the cost, then
  // we can always move it as much as possible in this direction. Only the
  // constraint will stop us, making it an equality. If the constraint doesn't
  // stop us, then the program is unbounded (provided that there is a feasible
  // solution).
  //
  // Note that this operation does not need any "undo" during the post-solve. At
  // optimality, the dual value on the constraint row will be of the correct
  // sign, and relaxing the constraint bound will not impact the dual
  // feasibility of the solution.
  //
  // TODO(user): this operation can be generalized to columns with just one
  // blocking constraint. Investigate how to use this. The 'reverse' can
  // probably also be done, relaxing a constraint that is blocking a
  // unconstrained variable.
  bool MakeConstraintAnEqualityIfPossible(const SparseMatrix& matrix_transpose,
                                          MatrixEntry e, LinearProgram* lp);

  // If a singleton column appears in an equality, we can remove its cost by
  // changing the other variables cost using the constraint. We can then delete
  // the column like in DeleteZeroCostSingletonColumn().
  void DeleteSingletonColumnInEquality(const SparseMatrix& matrix_transpose,
                                       MatrixEntry e, LinearProgram* lp);

  ColumnDeletionHelper column_deletion_helper_;
  RowDeletionHelper row_deletion_helper_;
  std::vector<SingletonUndo> undo_stack_;

  // This is used as a "cache" by MakeConstraintAnEqualityIfPossible() to avoid
  // scanning more than once each row. See the code to see how this is used.
  ITIVector<RowIndex, bool> row_sum_is_cached_;
  ITIVector<RowIndex, SumWithNegativeInfiniteAndOneMissing> row_lb_sum_;
  ITIVector<RowIndex, SumWithPositiveInfiniteAndOneMissing> row_ub_sum_;

  // The columns that are deleted by this preprocessor.
  SparseMatrix deleted_columns_;
  // The transpose of the rows that are deleted by this preprocessor.
  // TODO(user): implement a RowMajorSparseMatrix class to simplify the code.
  SparseMatrix deleted_rows_;

  DISALLOW_COPY_AND_ASSIGN(SingletonPreprocessor);
};

// --------------------------------------------------------
// FixedVariablePreprocessor
// --------------------------------------------------------
// Removes the fixed variables from the problem.
class FixedVariablePreprocessor : public Preprocessor {
 public:
  FixedVariablePreprocessor() {}
  ~FixedVariablePreprocessor() final {}
  bool Run(LinearProgram* linear_program, TimeLimit* time_limit) final;
  void RecoverSolution(ProblemSolution* solution) const final;

 private:
  ColumnDeletionHelper column_deletion_helper_;
  DISALLOW_COPY_AND_ASSIGN(FixedVariablePreprocessor);
};

// --------------------------------------------------------
// ForcingAndImpliedFreeConstraintPreprocessor
// --------------------------------------------------------
// This preprocessor computes for each constraint row the bounds that are
// implied by the variable bounds and applies one of the following reductions:
//
// * If the intersection of the implied bounds and the current constraint bounds
// is empty (modulo some tolerance), the problem is INFEASIBLE.
//
// * If the intersection of the implied bounds and the current constraint bounds
// is a singleton (modulo some tolerance), then the constraint is said to be
// forcing and all the variables that appear in it can be fixed to one of their
// bounds. All these columns and the constraint row is removed.
//
// * If the implied bounds are included inside the current constraint bounds
// (modulo some tolerance) then the constraint is said to be redundant or
// implied free. Its bounds are relaxed and the constraint will be removed
// later by the FreeConstraintPreprocessor.
//
// * Otherwise, wo do nothing.
class ForcingAndImpliedFreeConstraintPreprocessor : public Preprocessor {
 public:
  ForcingAndImpliedFreeConstraintPreprocessor() {}
  ~ForcingAndImpliedFreeConstraintPreprocessor() final {}
  bool Run(LinearProgram* linear_program, TimeLimit* time_limit) final;
  void RecoverSolution(ProblemSolution* solution) const final;

 private:
  bool lp_is_maximization_problem_;
  SparseMatrix deleted_columns_;
  DenseRow costs_;
  DenseBooleanColumn is_forcing_up_;
  ColumnDeletionHelper column_deletion_helper_;
  RowDeletionHelper row_deletion_helper_;

  DISALLOW_COPY_AND_ASSIGN(ForcingAndImpliedFreeConstraintPreprocessor);
};

// --------------------------------------------------------
// ImpliedFreePreprocessor
// --------------------------------------------------------
// It is possible to compute "implied" bounds on a variable from the bounds of
// all the other variables and the constraints in which this variable take
// place. If such "implied" bounds are inside the variable bounds, then the
// variable bounds can be relaxed and the variable is said to be "implied free".
//
// This preprocessor detects the implied free variables and make as many as
// possible free with a priority towards low-degree columns. This transformation
// will make the simplex algorithm more efficient later, but will also make it
// possible to reduce the problem by applying subsequent transformations:
//
// * The SingletonPreprocessor already deals with implied free singleton
// variables and removes the columns and the rows in which they appear.
//
// * Any multiple of the column of a free variable can be added to any other
// column without changing the linear program solution. This is the dual
// counterpart of the fact that any multiple of an equality row can be added to
// any row.
//
// TODO(user): Only process doubleton columns so we have more chance in the
// later passes to create more doubleton columns? Such columns lead to a smaller
// problem thanks to the DoubletonFreeColumnPreprocessor.
class ImpliedFreePreprocessor : public Preprocessor {
 public:
  ImpliedFreePreprocessor() {}
  ~ImpliedFreePreprocessor() final {}
  bool Run(LinearProgram* linear_program, TimeLimit* time_limit) final;
  void RecoverSolution(ProblemSolution* solution) const final;
  void UseInMipContext() final { LOG(FATAL) << "Not implemented."; }

 private:
  // This preprocessor adds fixed offsets to some variables. We remember those
  // here to un-offset them in RecoverSolution().
  DenseRow variable_offsets_;

  // This preprocessor causes some variables who would normally be
  // AT_{LOWER,UPPER}_BOUND to be VariableStatus::FREE. We store the restore
  // value of these variables; which will only be used (eg. restored) if the
  // variable actually turns out to be VariableStatus::FREE.
  VariableStatusRow postsolve_status_of_free_variables_;

  DISALLOW_COPY_AND_ASSIGN(ImpliedFreePreprocessor);
};

// --------------------------------------------------------
// DoubletonFreeColumnPreprocessor
// --------------------------------------------------------
// This preprocessor removes one of the two rows in which a doubleton column of
// a free variable appears. Since we can add any multiple of such a column to
// any other column, the way this works is that we can always remove all the
// entries on one row.
//
// Actually, we can remove all the entries except the one of the free column.
// But we will be left with a singleton row that we can delete in the same way
// as what is done in SingletonPreprocessor. That is by reporting the constraint
// bounds into the one of the originally free variable. After this operation,
// the doubleton free column will become a singleton and may or may not be
// removed later by the SingletonPreprocessor.
//
// Note that this preprocessor can be seen as the dual of the
// DoubletonEqualityRowPreprocessor since when taking the dual, an equality row
// becomes a free variable and vice versa.
//
// Note(user): As far as I know, this doubleton free column procedure is more
// general than what can be found in the research papers or in any of the linear
// solver open source codes as of July 2013. All of them only process such
// columns if one of the two rows is also an equality which is not actually
// required. Most probably, commercial solvers do use it though.
class DoubletonFreeColumnPreprocessor : public Preprocessor {
 public:
  DoubletonFreeColumnPreprocessor() {}
  ~DoubletonFreeColumnPreprocessor() final {}
  bool Run(LinearProgram* linear_program, TimeLimit* time_limit) final;
  void RecoverSolution(ProblemSolution* solution) const final;
  void UseInMipContext() final { LOG(FATAL) << "Not implemented."; }

 private:
  enum RowChoice {
    DELETED = 0,
    MODIFIED = 1,
    // This is just a constant for the number of rows in a doubleton column.
    // That is 2, one will be DELETED, the other MODIFIED.
    NUM_ROWS = 2,
  };
  struct RestoreInfo {
    // The index of the original free doubleton column and its objective.
    ColIndex col;
    Fractional objective_coefficient;

    // The row indices of the two involved rows and their coefficients on
    // column col.
    RowIndex row[NUM_ROWS];
    Fractional coeff[NUM_ROWS];

    // The deleted row as a column.
    SparseColumn deleted_row_as_column;
  };
  std::vector<RestoreInfo> restore_stack_;
  RowDeletionHelper row_deletion_helper_;
  DISALLOW_COPY_AND_ASSIGN(DoubletonFreeColumnPreprocessor);
};

// --------------------------------------------------------
// UnconstrainedVariablePreprocessor
// --------------------------------------------------------
// If for a given variable, none of the constraints block it in one direction
// and this direction improves the objective, then this variable can be fixed to
// its bound in this direction. If this bound is infinite and the variable cost
// is non-zero, then the problem is unbounded.
//
// TODO(user): This is similar to a preprocessor infering bounds on the reduced
// costs to fix a variable. It deals with less cases, however there are no
// numerical errors here.
class UnconstrainedVariablePreprocessor : public Preprocessor {
 public:
  UnconstrainedVariablePreprocessor() {}
  ~UnconstrainedVariablePreprocessor() final {}
  bool Run(LinearProgram* linear_program, TimeLimit* time_limit) final;
  void RecoverSolution(ProblemSolution* solution) const final;

  // Removes the given variable and all the rows in which it appears: If a
  // variable is unconstrained with a zero cost, then all the constraints in
  // which it appears can be made free! More precisely, during postsolve, if
  // such a variable is unconstrained towards +kInfinity, for any activity value
  // of the involved constraints, an M exists such that for each value of the
  // variable >= M the problem will be feasible.
  //
  // The algorithm during postsolve is to find a feasible value for all such
  // variables while trying to keep their magnitudes small (for better numerical
  // behavior). target_bound should take only two possible values: +/-kInfinity.
  void RemoveZeroCostUnconstrainedVariable(ColIndex col,
                                           Fractional target_bound,
                                           LinearProgram* lp);

 private:
  ColumnDeletionHelper column_deletion_helper_;
  RowDeletionHelper row_deletion_helper_;
  DenseColumn rhs_;
  DenseColumn activity_sign_correction_;
  DenseBooleanRow is_unbounded_;

  SparseMatrix deleted_columns_;
  SparseMatrix deleted_rows_as_column_;

  DISALLOW_COPY_AND_ASSIGN(UnconstrainedVariablePreprocessor);
};

// --------------------------------------------------------
// FreeConstraintPreprocessor
// --------------------------------------------------------
// Removes the constraints with no bounds from the problem.
class FreeConstraintPreprocessor : public Preprocessor {
 public:
  FreeConstraintPreprocessor() {}
  ~FreeConstraintPreprocessor() final {}
  bool Run(LinearProgram* linear_program, TimeLimit* time_limit) final;
  void RecoverSolution(ProblemSolution* solution) const final;

 private:
  RowDeletionHelper row_deletion_helper_;
  DISALLOW_COPY_AND_ASSIGN(FreeConstraintPreprocessor);
};

// --------------------------------------------------------
// EmptyConstraintPreprocessor
// --------------------------------------------------------
// Removes the constraints with no coefficients from the problem.
class EmptyConstraintPreprocessor : public Preprocessor {
 public:
  EmptyConstraintPreprocessor() {}
  ~EmptyConstraintPreprocessor() final {}
  bool Run(LinearProgram* linear_program, TimeLimit* time_limit) final;
  void RecoverSolution(ProblemSolution* solution) const final;

 private:
  RowDeletionHelper row_deletion_helper_;
  DISALLOW_COPY_AND_ASSIGN(EmptyConstraintPreprocessor);
};

// --------------------------------------------------------
// RemoveNearZeroEntriesPreprocessor
// --------------------------------------------------------
// Removes matrix entries that have only a negligible impact on the solution.
// Using the variable bounds, we derive a maximum possible impact, and remove
// the entries whose impact is under a given tolerance.
//
// TODO(user): This preprocessor doesn't work well on badly scaled problems. In
// particular, it will set the objective to zero if all the objective
// coefficients are small! Run it after ScalingPreprocessor or fix the code.
class RemoveNearZeroEntriesPreprocessor : public Preprocessor {
 public:
  RemoveNearZeroEntriesPreprocessor() {}
  ~RemoveNearZeroEntriesPreprocessor() final {}
  bool Run(LinearProgram* linear_program, TimeLimit* time_limit) final;
  void RecoverSolution(ProblemSolution* solution) const final;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoveNearZeroEntriesPreprocessor);
};

// --------------------------------------------------------
// SingletonColumnSignPreprocessor
// --------------------------------------------------------
// Make sure that the only coefficient of all singleton columns (i.e. column
// with only one entry) is positive. This is because this way the column will
// be transformed in an identity column by the scaling. This will lead to more
// efficient solve when this column is involved.
class SingletonColumnSignPreprocessor : public Preprocessor {
 public:
  SingletonColumnSignPreprocessor() {}
  ~SingletonColumnSignPreprocessor() final {}
  bool Run(LinearProgram* linear_program, TimeLimit* time_limit) final;
  void RecoverSolution(ProblemSolution* solution) const final;

 private:
  std::vector<ColIndex> changed_columns_;
  DISALLOW_COPY_AND_ASSIGN(SingletonColumnSignPreprocessor);
};

// --------------------------------------------------------
// DoubletonEqualityRowPreprocessor
// --------------------------------------------------------
// Reduce equality constraints involving two variables (i.e. aX + bY = c),
// by substitution (and thus removal) of one of the variables by the other
// in all the constraints that it is involved in.
class DoubletonEqualityRowPreprocessor : public Preprocessor {
 public:
  DoubletonEqualityRowPreprocessor() {}
  ~DoubletonEqualityRowPreprocessor() final {}
  bool Run(LinearProgram* linear_program, TimeLimit* time_limit) final;
  void RecoverSolution(ProblemSolution* solution) const final;
  void UseInMipContext() final { LOG(FATAL) << "Not implemented."; }

 private:
  enum ColChoice {
    DELETED = 0,
    MODIFIED = 1,
    // For for() loops iterating over the ColChoice values, and/or arrays.
    NUM_DOUBLETON_COLS = 2,
  };
  static ColChoice OtherColChoice(ColChoice x) {
    return x == DELETED ? MODIFIED : DELETED;
  }

  ColumnDeletionHelper column_deletion_helper_;
  RowDeletionHelper row_deletion_helper_;

  struct RestoreInfo {
    // The row index of the doubleton equality constraint, and its constant.
    RowIndex row;
    Fractional rhs;  // The constant c in the equality aX + bY = c.

    // The indices and the data of the two columns that we touched, exactly
    // as they were beforehand.
    ColIndex col[NUM_DOUBLETON_COLS];
    Fractional coeff[NUM_DOUBLETON_COLS];
    Fractional lb[NUM_DOUBLETON_COLS];
    Fractional ub[NUM_DOUBLETON_COLS];
    SparseColumn column[NUM_DOUBLETON_COLS];
    Fractional objective_coefficient[NUM_DOUBLETON_COLS];

    // If the modified variable has status AT_[LOWER,UPPER]_BOUND, then we'll
    // set one of the two original variables to one of its bounds, and set the
    // other to VariableStatus::BASIC. We store this information (which variable
    // will be set to one of its bounds, and which bound) for each possible
    // outcome.
    struct ColChoiceAndStatus {
      ColChoice col_choice;
      VariableStatus status;
      Fractional value;
      ColChoiceAndStatus() : col_choice(), status(), value(0.0) {}
      ColChoiceAndStatus(ColChoice c, VariableStatus s, Fractional v)
          : col_choice(c), status(s), value(v) {}
    };
    ColChoiceAndStatus bound_backtracking_at_lower_bound;
    ColChoiceAndStatus bound_backtracking_at_upper_bound;
  };
  std::vector<RestoreInfo> restore_stack_;

  DISALLOW_COPY_AND_ASSIGN(DoubletonEqualityRowPreprocessor);
};

// --------------------------------------------------------
// DualizerPreprocessor
// --------------------------------------------------------
// DualizerPreprocessor may change the given program to its dual depending
// on the value of the parameter solve_dual_problem.
//
// IMPORTANT: FreeConstraintPreprocessor() must be called first since this
// preprocessor does not deal correctly with free constraints.
class DualizerPreprocessor : public Preprocessor {
 public:
  DualizerPreprocessor() {}
  ~DualizerPreprocessor() final {}
  bool Run(LinearProgram* linear_program, TimeLimit* time_limit) final;
  void RecoverSolution(ProblemSolution* solution) const final;
  void UseInMipContext() final {
    LOG(FATAL) << "In the presence of integer variables, "
               << "there is no notion of a dual problem.";
  }

  // Convert the given problem status to the one of its dual.
  ProblemStatus ChangeStatusToDualStatus(ProblemStatus status) const;

 private:
  DenseRow variable_lower_bounds_;
  DenseRow variable_upper_bounds_;

  RowIndex primal_num_rows_;
  ColIndex primal_num_cols_;
  bool primal_is_maximization_problem_;
  RowToColMapping duplicated_rows_;

  // For postsolving the variable/constraint statuses.
  VariableStatusRow dual_status_correspondence_;
  ColMapping slack_or_surplus_mapping_;

  DISALLOW_COPY_AND_ASSIGN(DualizerPreprocessor);
};

// --------------------------------------------------------
// ShiftVariableBoundsPreprocessor
// --------------------------------------------------------
// For each variable, inspects its bounds and "shift" them if necessary, so that
// its domain contains zero. A variable that was shifted will always have at
// least one of its bounds to zero. Doing it all at once allows to have a better
// precision when modifying the constraint bounds by using an accurate summation
// algorithm.
//
// Example:
// - A variable with bound [1e10, infinity] will be shifted to [0, infinity].
// - A variable with domain [-1e10, 1e10] will not be shifted. Note that
//   compared to the first case, doing so here may introduce unecessary
//   numerical errors if the variable value in the final solution is close to
//   zero.
//
// The expected impact of this is:
// - Better behavior of the scaling.
// - Better precision and numerical accuracy of the simplex method.
// - Slightly improved speed (because adding a column with a variable value of
//   zero takes no work later).
//
// TODO(user): Having for each variable one of their bounds at zero is a
// requirement for the DualizerPreprocessor and for the implied free column in
// the ImpliedFreePreprocessor. However, shifting a variable with a domain like
// [-1e10, 1e10] may introduce numerical issues. Relax the definition of
// a free variable so that only having a domain containing 0.0 is enough?
class ShiftVariableBoundsPreprocessor : public Preprocessor {
 public:
  ShiftVariableBoundsPreprocessor() {}
  ~ShiftVariableBoundsPreprocessor() final {}
  bool Run(LinearProgram* linear_program, TimeLimit* time_limit) final;
  void RecoverSolution(ProblemSolution* solution) const final;

 private:
  // Contains for each variable by how much its bounds where shifted during
  // presolve. Note that the shift was negative (new bound = initial bound -
  // offset).
  DenseRow offsets_;
  // Contains the initial problem bounds. They are needed to get the perfect
  // numerical accuracy for variables at their bound after postsolve.
  DenseRow variable_initial_lbs_;
  DenseRow variable_initial_ubs_;
  DISALLOW_COPY_AND_ASSIGN(ShiftVariableBoundsPreprocessor);
};

// --------------------------------------------------------
// ScalingPreprocessor
// --------------------------------------------------------
// Scales the SparseMatrix of the linear program using a SparseMatrixScaler.
// This is only applied if the parameter use_scaling is true.
class ScalingPreprocessor : public Preprocessor {
 public:
  ScalingPreprocessor() {}
  ~ScalingPreprocessor() final {}
  bool Run(LinearProgram* linear_program, TimeLimit* time_limit) final;
  void RecoverSolution(ProblemSolution* solution) const final;
  void UseInMipContext() final { LOG(FATAL) << "Not implemented."; }

 private:
  DenseRow variable_lower_bounds_;
  DenseRow variable_upper_bounds_;
  Fractional cost_scaling_factor_;
  SparseMatrixScaler scaler_;

  DISALLOW_COPY_AND_ASSIGN(ScalingPreprocessor);
};

// --------------------------------------------------------
// ToMinimizationPreprocessor
// --------------------------------------------------------
// Changes the problem from maximization to minimization (if applicable).
// As of 2015/09/03 this is not used by Glop, but will be used by Glip.
// The preprocessor is kept here, because it could be used by Glop too.
class ToMinimizationPreprocessor : public Preprocessor {
 public:
  ToMinimizationPreprocessor() {}
  ~ToMinimizationPreprocessor() final {}
  bool Run(LinearProgram* linear_program, TimeLimit* time_limit) final;
  void RecoverSolution(ProblemSolution* solution) const final;

 private:
  DISALLOW_COPY_AND_ASSIGN(ToMinimizationPreprocessor);
};

// --------------------------------------------------------
// AddSlackVariablesPreprocessor
// --------------------------------------------------------
// Transforms the linear program to the equation form
// min c.x, s.t. A.x = 0. This is done by:
// 1. Introducing slack variables for all constraints; all these variables are
//    introduced with coefficient 1.0, and their bounds are set to be negative
//    bounds of the corresponding constraint.
// 2. Changing the bounds of all constraints to (0, 0) to make them an equality.
//
// As a consequence, the matrix of the linear program always has full row rank
// after this preprocessor. Note that the slack variables are always added last,
// so that the rightmost square sub-matrix is always the identity matrix.
class AddSlackVariablesPreprocessor : public Preprocessor {
 public:
  AddSlackVariablesPreprocessor() {}
  ~AddSlackVariablesPreprocessor() final {}
  bool Run(LinearProgram* linear_program, TimeLimit* time_limit) final;
  void RecoverSolution(ProblemSolution* solution) const final;

 private:
  ColIndex first_slack_col_;

  DISALLOW_COPY_AND_ASSIGN(AddSlackVariablesPreprocessor);
};

// --------------------------------------------------------
// SolowHalimPreprocessor
// --------------------------------------------------------
// Modifies the problem by changing variables to begin
// geometrically near to the optimal solution according to the suggestion of
// Solow & Halim.
// Variable changes have a very simple form :
// x_j' =  upper_bound(j) - x_j if cost(j) >  0
// x_j' = -lower_bound(j) + x_j if cost(j) <  0
// for a maximization problem
//
// reference:
// Improving the Efficiency of the Simplex Algorithm Based on a
// Geometric Explanation of Phase 1
// https://weatherhead.case.edu/faculty/research/library/detail?id=12128593921
// DOI: 10.1504/IJOR.2009.025701
class SolowHalimPreprocessor : public Preprocessor {
 public:
  SolowHalimPreprocessor() {}
  ~SolowHalimPreprocessor() final {}
  bool Run(LinearProgram* linear_program, TimeLimit* time_limit) final;
  void RecoverSolution(ProblemSolution* solution) const final;

 private:
  typedef enum {
    NOT_MODIFIED = 0,
    SHIFTED = 1,
    SHIFTED_OPPOSITE_DIRECTION = 2
  } ColumnTransformType;

  // Contains the coordinate change information for each column
  ITIVector<ColIndex, ColumnTransformType> column_transform_;

  // Contains the initial problem bounds.
  DenseRow variable_initial_lbs_;
  DenseRow variable_initial_ubs_;
  DISALLOW_COPY_AND_ASSIGN(SolowHalimPreprocessor);
};

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_GLOP_PREPROCESSOR_H_
