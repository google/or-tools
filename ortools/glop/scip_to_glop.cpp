// This file implements the interface defined in
// scip/src/scip/lpi.h, but we host it in the glop directory because
// it references much of the glop code, which is in active development: the glop
// developers need to easily apply changes to the glop interface to all the
// relevant files; and having those located in the same directory is best.
//
// If SCIP is compiled with this .cpp, then it will use Glop as the underlying
// LP solver. This is done in the target scip:libscip_with_glop.
//
// TODO(fdid): currently a lot of functions are left unimplemented and result
// in a LOG(FATAL) if they are called. It seems that SCIP never uses them when
// called through the MPSolver interface (on July 2013; based on solving a
// couple of MIP problems).
//
// TODO(fdid): The function comments come from the SCIP lpi.h file and are not
// compliant with the Google style guide. We could remove them since they are
// in the .h, but given the file size, they are quite helpful.

#include "scip/src/lpi/lpi.h"
#include "ortools/glop/lp_solver.h"
#include "ortools/glop/revised_simplex.h"
#include "ortools/lp_data/lp_print_utils.h"
#include "ortools/util/stats.h"
#include "ortools/util/time_limit.h"

// We can't be inside the operations_research::glop namespace, because we are
// defining extern methods referenced by the SCIP code.
using operations_research::TimeLimit;
using operations_research::glop::BasisState;
using operations_research::glop::ColIndex;
using operations_research::glop::ColIndexVector;
using operations_research::glop::ConstraintStatus;
using operations_research::glop::DenseBooleanRow;
using operations_research::glop::DenseBooleanColumn;
using operations_research::glop::DenseRow;
using operations_research::glop::DenseColumn;
using operations_research::glop::Fractional;
using operations_research::glop::GetProblemStatusString;
using operations_research::glop::ProblemStatus;
using operations_research::glop::RowIndex;
using operations_research::glop::VariableStatus;

// Struct storing all the state used by the functions in this file.
// This is mapped to SCIP_LPI with a typedef in scip/type_lpi.h
struct SCIP_LPi {
  operations_research::glop::LinearProgram* linear_program;
  operations_research::glop::RevisedSimplex* solver;
  operations_research::glop::GlopParameters* parameters;
  operations_research::StatsGroup* stats;

  // TODO(fdid): Store the parameters not yet supported by this interface.
  // In debug mode, SCIP check that set() and then get() work as expected, so
  // we need to store them.
  bool from_scratch;
  bool fast_mip;
  bool lp_info;
  double rowrepswitch;
  int pricing;

  // This is used by SCIPlpiWasSolved().
  bool lp_modified_since_last_solve;
  bool lp_time_limit_was_reached;
};

// --------------------------------------------------------
// Miscellaneous Methods
// --------------------------------------------------------

// gets name and version of LP solver
const char* SCIPlpiGetSolverName(void) { return "Glop"; }

// gets description of LP solver (developer, webpage, ...)
const char* SCIPlpiGetSolverDesc(void) { return "TODO(fdid): description."; }

// gets pointer for LP solver - use only with great care
//
// The behavior of this function depends on the solver and its use is therefore
// only recommended if you really know what you are doing. In general, it
// returns a pointer to the LP solver object.
void* SCIPlpiGetSolverPointer(SCIP_LPI* lpi) {
  LOG(FATAL) << "calling SCIPlpiGetSolverPointer";
  return NULL;
}

SCIP_RETCODE SCIPlpiSetIntegralityInformation(
    SCIP_LPI* lpi, /**< pointer to an LP interface structure */
    int ncols,     /**< length of integrality array */
    int* intInfo   /**< integrality array (0: continuous, 1: integer) */
    ) {
  LOG(FATAL) << "SCIPlpiSetIntegralityInformation() has not been implemented.";
}

// --------------------------------------------------------
// LPI Creation and Destruction Methods
// --------------------------------------------------------

// creates an LP problem object
SCIP_RETCODE SCIPlpiCreate(
    SCIP_LPI** lpi,  // pointer to an LP interface structure
    SCIP_MESSAGEHDLR* messagehdlr,  // message handler to use for printing
                                    // messages, or NULL
    const char* name,   // problem name
    SCIP_OBJSEN objsen  // objective sense
    ) {
  // Initilialize memory.
  SCIP_ALLOC(BMSallocMemory(lpi));
  (*lpi)->linear_program = new operations_research::glop::LinearProgram();
  (*lpi)->solver = new operations_research::glop::RevisedSimplex();
  (*lpi)->parameters = new operations_research::glop::GlopParameters();
  (*lpi)->stats = new operations_research::StatsGroup("lpi_glop");

  // Set problem name and objective direction.
  (*lpi)->linear_program->SetName(string(name));
  SCIPlpiChgObjsen(*lpi, objsen);
  (*lpi)->lp_modified_since_last_solve = true;
  return SCIP_OKAY;
}

// deletes an LP problem object
SCIP_RETCODE SCIPlpiFree(SCIP_LPI** lpi) {
  VLOG(1) << "calling SCIPlpiFree " << (*lpi)->stats->StatString();
  delete (*lpi)->stats;
  delete (*lpi)->parameters;
  delete (*lpi)->solver;
  delete (*lpi)->linear_program;
  BMSfreeMemory(lpi);
  return SCIP_OKAY;
}

// --------------------------------------------------------
// Modification Methods
// --------------------------------------------------------

// copies LP data with column matrix into LP solver
SCIP_RETCODE SCIPlpiLoadColLP(
    SCIP_LPI* lpi,         // LP interface structure
    SCIP_OBJSEN objsen,    // objective sense
    int ncols,             // number of columns
    const SCIP_Real* obj,  // objective function values of columns
    const SCIP_Real* lb,   // lower bounds of columns
    const SCIP_Real* ub,   // upper bounds of columns
    char** colnames,       // column names, or NULL
    int nrows,             // number of rows
    const SCIP_Real* lhs,  // left hand sides of rows
    const SCIP_Real* rhs,  // right hand sides of rows
    char** rownames,       // row names, or NULL
    int nnonz,            // number of nonzero elements in the constraint matrix
    const int* beg,       // start index of each column in ind- and val-array
    const int* ind,       // row indices of constraint matrix entries
    const SCIP_Real* val  // values of constraint matrix entries
    ) {
  SCOPED_TIME_STAT(lpi->stats);
  LOG(FATAL) << "calling SCIPlpiLoadColLP";
  return SCIP_OKAY;
}

// adds columns to the LP
SCIP_RETCODE SCIPlpiAddCols(
    SCIP_LPI* lpi,         // LP interface structure
    int ncols,             // number of columns to be added
    const SCIP_Real* obj,  // objective function values of new columns
    const SCIP_Real* lb,   // lower bounds of new columns
    const SCIP_Real* ub,   // upper bounds of new columns
    char** colnames,       // column names, or NULL
    int nnonz,             // number of nonzero elements to be added to the
                           // constraint matrix
    const int* beg,        // start index of each column in ind- and
                           // val-array, or NULL if nnonz == 0
    const int* ind,        // row indices of constraint matrix entries, or
                           // NULL if nnonz == 0
    const SCIP_Real* val   // values of constraint matrix entries, or
                           // NULL if nnonz == 0
    ) {
  SCOPED_TIME_STAT(lpi->stats);
  // TODO(fdid): propagate the names?
  VLOG(1) << "calling SCIPlpiAddCols ncols=" << ncols << " nnonz=" << nnonz;
  int nz = 0;
  for (int i = 0; i < ncols; ++i) {
    const ColIndex col = lpi->linear_program->CreateNewVariable();
    lpi->linear_program->SetVariableBounds(col, lb[i], ub[i]);
    lpi->linear_program->SetObjectiveCoefficient(col, obj[i]);
    const int end = (nnonz == 0 || i == ncols - 1) ? nnonz : beg[i + 1];
    while (nz < end) {
      lpi->linear_program->SetCoefficient(RowIndex(ind[nz]), col, val[nz]);
      ++nz;
    }
  }
  lpi->lp_modified_since_last_solve = true;
  return SCIP_OKAY;
}

// deletes all columns in the given range from LP
SCIP_RETCODE SCIPlpiDelCols(SCIP_LPI* lpi,  // LP interface structure
                            int firstcol,   // first column to be deleted
                            int lastcol     // last column to be deleted
                            ) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiDelCols firstcol=" << firstcol
          << " lastcol=" << lastcol;
  const ColIndex num_cols = lpi->linear_program->num_variables();
  DenseBooleanRow columns_to_delete(num_cols, false);
  for (int i = firstcol; i <= lastcol; ++i) {
    columns_to_delete[ColIndex(i)] = true;
  }
  lpi->linear_program->DeleteColumns(columns_to_delete);
  lpi->lp_modified_since_last_solve = true;
  return SCIP_OKAY;
}

// deletes columns from SCIP_LPI; the new position of a column must not be
// greater that its old position
SCIP_RETCODE SCIPlpiDelColset(
    SCIP_LPI* lpi,  // LP interface structure
    int* dstat      // deletion status of columns
                    // input:  1 if column should be deleted, 0 if not
                    // output: new position of column, -1 if column was deleted
    ) {
  SCOPED_TIME_STAT(lpi->stats);
  const ColIndex num_cols = lpi->linear_program->num_variables();
  DenseBooleanRow columns_to_delete(num_cols, false);
  int new_index = 0;
  int num_deleted_columns = 0;
  for (ColIndex col(0); col < num_cols; ++col) {
    int i = col.value();
    if (dstat[i] == 1) {
      columns_to_delete[col] = true;
      dstat[i] = -1;
      ++num_deleted_columns;
    } else {
      dstat[i] = new_index;
      ++new_index;
    }
  }
  VLOG(1)
      << "calling SCIPlpiDelColset num_deleted_columns=" << num_deleted_columns;
  lpi->linear_program->DeleteColumns(columns_to_delete);
  lpi->lp_modified_since_last_solve = true;
  return SCIP_OKAY;
}

// adds rows to the LP
SCIP_RETCODE SCIPlpiAddRows(
    SCIP_LPI* lpi,         // LP interface structure
    int nrows,             // number of rows to be added
    const SCIP_Real* lhs,  // left hand sides of new rows
    const SCIP_Real* rhs,  // right hand sides of new rows
    char** rownames,       // row names, or NULL
    int nnonz,             // number of nonzero elements to be added to the
                           // constraint matrix
    const int* beg,        // start index of each row in ind- and val-array,
                           // or NULL if nnonz == 0
    const int* ind,        // column indices of constraint matrix entries,
                           // or NULL if nnonz == 0
    const SCIP_Real* val   // values of constraint matrix entries, or
                           // NULL if nnonz == 0
    ) {
  SCOPED_TIME_STAT(lpi->stats);
  // TODO(fdid): propagate the names?
  VLOG(1) << "calling SCIPlpiAddRows nrows=" << nrows << " nnonz=" << nnonz;
  int nz = 0;
  for (int i = 0; i < nrows; ++i) {
    const RowIndex row = lpi->linear_program->CreateNewConstraint();
    lpi->linear_program->SetConstraintBounds(row, lhs[i], rhs[i]);
    const int end = (nnonz == 0 || i == nrows - 1) ? nnonz : beg[i + 1];
    while (nz < end) {
      lpi->linear_program->SetCoefficient(row, ColIndex(ind[nz]), val[nz]);
      ++nz;
    }
  }
  lpi->lp_modified_since_last_solve = true;
  return SCIP_OKAY;
}

// deletes all rows in the given range from LP
SCIP_RETCODE SCIPlpiDelRows(SCIP_LPI* lpi,  // LP interface structure
                            int firstrow,   // first row to be deleted
                            int lastrow     // last row to be deleted
                            ) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiDelRows firstrow=" << firstrow
          << " lastrow=" << lastrow;
  const RowIndex num_rows = lpi->linear_program->num_constraints();
  DenseBooleanColumn rows_to_delete(num_rows, false);
  for (int i = firstrow; i <= lastrow; ++i) {
    rows_to_delete[RowIndex(i)] = true;
  }
  lpi->linear_program->DeleteRows(rows_to_delete);
  lpi->lp_modified_since_last_solve = true;
  return SCIP_OKAY;
}

// deletes rows from SCIP_LPI; the new position of a row must not be greater
// that its old position
SCIP_RETCODE SCIPlpiDelRowset(
    SCIP_LPI* lpi,  // LP interface structure
    int* dstat      // deletion status of rows
                    //   input: 1 if row should be deleted, 0 if not
                    //   output: new position of row, -1 if row was deleted
    ) {
  SCOPED_TIME_STAT(lpi->stats);
  const RowIndex num_rows = lpi->linear_program->num_constraints();
  DenseBooleanColumn rows_to_delete(num_rows, false);
  int new_index = 0;
  int num_deleted_rows = 0;
  for (RowIndex row(0); row < num_rows; ++row) {
    int i = row.value();
    if (dstat[i] == 1) {
      rows_to_delete[row] = true;
      dstat[i] = -1;
      ++num_deleted_rows;
    } else {
      dstat[i] = new_index;
      ++new_index;
    }
  }
  VLOG(1) << "calling SCIPlpiDelRowset num_deleted_rows=" << num_deleted_rows;
  lpi->linear_program->DeleteRows(rows_to_delete);
  lpi->lp_modified_since_last_solve = true;
  return SCIP_OKAY;
}

// clears the whole LP
SCIP_RETCODE SCIPlpiClear(SCIP_LPI* lpi) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiClear";
  lpi->linear_program->Clear();
  lpi->lp_modified_since_last_solve = true;
  return SCIP_OKAY;
}

// changes lower and upper bounds of columns
SCIP_RETCODE SCIPlpiChgBounds(SCIP_LPI* lpi,  // LP interface structure
                              int ncols,  // number of columns to change bounds
                                          // for
                              const int* ind,       // column indices
                              const SCIP_Real* lb,  // values for the new lower
                                                    // bounds
                              const SCIP_Real* ub   // values for the new upper
                                                    // bounds
                              ) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiChgBounds ncols=" << ncols;
  for (int i = 0; i < ncols; ++i) {
    lpi->linear_program->SetVariableBounds(ColIndex(ind[i]), lb[i], ub[i]);
  }
  lpi->lp_modified_since_last_solve = true;
  return SCIP_OKAY;
}

// changes left and right hand sides of rows
SCIP_RETCODE SCIPlpiChgSides(SCIP_LPI* lpi,  // LP interface structure
                             int nrows,  // number of rows to change sides for
                             const int* ind,        // row indices
                             const SCIP_Real* lhs,  // new values for left hand
                                                    // sides
                             const SCIP_Real* rhs   // new values for right hand
                                                    // sides
                             ) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiChgSides nrows=" << nrows;
  for (int i = 0; i < nrows; ++i) {
    lpi->linear_program->SetConstraintBounds(RowIndex(ind[i]), lhs[i], rhs[i]);
  }
  lpi->lp_modified_since_last_solve = true;
  return SCIP_OKAY;
}

// changes a single coefficient
SCIP_RETCODE SCIPlpiChgCoef(SCIP_LPI* lpi,  // LP interface structure
                            int row,  // row number of coefficient to change
                            int col,  // column number of coefficient to change
                            SCIP_Real newval  // new value of coefficient
                            ) {
  SCOPED_TIME_STAT(lpi->stats);
  LOG(FATAL) << "calling SCIPlpiChgCoef";
  lpi->lp_modified_since_last_solve = true;
  return SCIP_OKAY;
}

// changes the objective sense
SCIP_RETCODE SCIPlpiChgObjsen(SCIP_LPI* lpi,      // LP interface structure
                              SCIP_OBJSEN objsen  // new objective sense
                              ) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiChgObjsen";
  switch (objsen) {
    case SCIP_OBJSEN_MAXIMIZE:
      lpi->linear_program->SetMaximizationProblem(true);
      break;
    case SCIP_OBJSEN_MINIMIZE:
      lpi->linear_program->SetMaximizationProblem(false);
      break;
  }
  lpi->lp_modified_since_last_solve = true;
  return SCIP_OKAY;
}

// changes objective values of columns in the LP
SCIP_RETCODE SCIPlpiChgObj(
    SCIP_LPI* lpi,        // LP interface structure
    int ncols,            // number of columns to change objective value for
    const int* ind,       // column indices to change objective value for
    const SCIP_Real* obj  // new objective values for columns
    ) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiChgObj ncols=" << ncols;
  for (int i = 0; i < ncols; ++i) {
    lpi->linear_program->SetObjectiveCoefficient(ColIndex(ind[i]), obj[i]);
  }
  lpi->lp_modified_since_last_solve = true;
  return SCIP_OKAY;
}

// multiplies a row with a non-zero scalar; for negative scalars, the row's
// sense is switched accordingly
SCIP_RETCODE SCIPlpiScaleRow(SCIP_LPI* lpi,      // LP interface structure
                             int row,            // row number to scale
                             SCIP_Real scaleval  // scaling multiplier
                             ) {
  SCOPED_TIME_STAT(lpi->stats);
  LOG(FATAL) << "calling SCIPlpiScaleRow";
  return SCIP_OKAY;
}

// multiplies a column with a non-zero scalar; the objective value is
// multiplied with the scalar, and the bounds are divided by the scalar; for
// negative scalars, the column's bounds are switched
SCIP_RETCODE SCIPlpiScaleCol(SCIP_LPI* lpi,      // LP interface structure
                             int col,            // column number to scale
                             SCIP_Real scaleval  // scaling multiplier
                             ) {
  SCOPED_TIME_STAT(lpi->stats);
  LOG(FATAL) << "calling SCIPlpiScaleCol";
  return SCIP_OKAY;
}

// --------------------------------------------------------
// Data Accessing Methods
// --------------------------------------------------------

// gets the number of rows in the LP
SCIP_RETCODE SCIPlpiGetNRows(SCIP_LPI* lpi,  // LP interface structure
                             int* nrows  // pointer to store the number of rows
                             ) {
  SCOPED_TIME_STAT(lpi->stats);
  *nrows = lpi->linear_program->num_constraints().value();
  return SCIP_OKAY;
}

// gets the number of columns in the LP
SCIP_RETCODE SCIPlpiGetNCols(SCIP_LPI* lpi,  // LP interface structure
                             int* ncols  // pointer to store the number of cols
                             ) {
  SCOPED_TIME_STAT(lpi->stats);
  *ncols = lpi->linear_program->num_variables().value();
  return SCIP_OKAY;
}

// gets the objective sense of the LP
SCIP_RETCODE SCIPlpiGetObjsen(SCIP_LPI* lpi,       // LP interface structure
                              SCIP_OBJSEN* objsen  // pointer to store objective
                                                   // sense
                              ) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiGetObjsen";
  *objsen = lpi->linear_program->IsMaximizationProblem() ? SCIP_OBJSEN_MAXIMIZE
                                                         : SCIP_OBJSEN_MINIMIZE;
  return SCIP_OKAY;
}

// gets the number of nonzero elements in the LP constraint matrix
SCIP_RETCODE SCIPlpiGetNNonz(SCIP_LPI* lpi,  // LP interface structure
                             int* nnonz      // pointer to store the number of
                                             // nonzeros
                             ) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiGetNNonz";
  *nnonz = lpi->linear_program->num_entries().value();
  return SCIP_OKAY;
}

// gets columns from LP problem object; the arrays have to be large enough to
// store all values; Either both, lb and ub, have to be NULL, or both have to be
// non-NULL, either nnonz, beg, ind, and val have to be NULL, or all of them
// have to be non-NULL.
SCIP_RETCODE SCIPlpiGetCols(
    SCIP_LPI* lpi,  // LP interface structure
    int firstcol,   // first column to get from LP
    int lastcol,    // last column to get from LP
    SCIP_Real* lb,  // buffer to store the lower bound vector, or NULL
    SCIP_Real* ub,  // buffer to store the upper bound vector, or NULL
    int* nnonz,     // pointer to store the number of nonzero elements
                    // returned, or NULL
    int* beg,       // buffer to store start index of each column in ind-
                    // and val-array, or NULL
    int* ind,       // buffer to store column indices of constraint matrix
                    // entries, or NULL
    SCIP_Real* val  // buffer to store values of constraint matrix
                    // entries, or NULL
    ) {
  SCOPED_TIME_STAT(lpi->stats);
  LOG(FATAL) << "calling SCIPlpiGetCols";
  return SCIP_OKAY;
}

// gets rows from LP problem object; the arrays have to be large enough to store
// all values. Either both, lhs and rhs, have to be NULL, or both have to be
// non-NULL, either nnonz, beg, ind, and val have to be NULL, or all of them
// have to be non-NULL.
SCIP_RETCODE SCIPlpiGetRows(
    SCIP_LPI* lpi,   // LP interface structure
    int firstrow,    // first row to get from LP
    int lastrow,     // last row to get from LP
    SCIP_Real* lhs,  // buffer to store left hand side vector, or NULL
    SCIP_Real* rhs,  // buffer to store right hand side vector, or NULL
    int* nnonz,      // pointer to store the number of nonzero elements
                     // returned, or NULL
    int* beg,        // buffer to store start index of each row in ind- and
                     // val-array, or NULL
    int* ind,        // buffer to store row indices of constraint matrix
                     // entries, or NULL
    SCIP_Real* val   // buffer to store values of constraint matrix
                     // entries, or NULL
    ) {
  SCOPED_TIME_STAT(lpi->stats);
  LOG(FATAL) << "calling SCIPlpiGetRows";
  return SCIP_OKAY;
}

// gets column names
SCIP_RETCODE SCIPlpiGetColNames(
    SCIP_LPI* lpi,        // LP interface structure
    int firstcol,         // first column to get name from LP
    int lastcol,          // last column to get name from LP
    char** colnames,      // pointers to column names (of size at
                          // least lastcol-firstcol+1)
    char* namestorage,    // storage for col names
    int namestoragesize,  // size of namestorage (if 0, -storageleft
                          // returns the storage needed)
    int* storageleft      // amount of storage left (if < 0 the
                          // namestorage was not big enough)
    ) {
  SCOPED_TIME_STAT(lpi->stats);
  LOG(FATAL) << "calling SCIPlpiGetColNames";
  return SCIP_OKAY;
}

// gets row names
SCIP_RETCODE SCIPlpiGetRowNames(
    SCIP_LPI* lpi,        // LP interface structure
    int firstrow,         // first row to get name from LP
    int lastrow,          // last row to get name from LP
    char** rownames,      // pointers to row names (of size at least
                          // lastrow - firstrow + 1)
    char* namestorage,    // storage for row names
    int namestoragesize,  // size of namestorage (if 0, -storageleft returns the
                          // storage needed)
    int* storageleft      // amount of storage left (if < 0 the
                          // namestorage was not big enough)
    ) {
  SCOPED_TIME_STAT(lpi->stats);
  LOG(FATAL) << "calling SCIPlpiGetRowNames";
  return SCIP_OKAY;
}

// gets objective coefficients from LP problem object
SCIP_RETCODE SCIPlpiGetObj(SCIP_LPI* lpi,   // LP interface structure
                           int firstcol,    // first column to get objective
                                            // coefficient for
                           int lastcol,     // last column to get objective
                                            // coefficient for
                           SCIP_Real* vals  // array to store objective
                                            // coefficients
                           ) {
  SCOPED_TIME_STAT(lpi->stats);
  int index = 0;
  for (ColIndex col(firstcol); col <= ColIndex(lastcol); ++col) {
    vals[index] = lpi->linear_program->objective_coefficients()[col];
    ++index;
  }
  return SCIP_OKAY;
}

// gets current bounds from LP problem object
SCIP_RETCODE SCIPlpiGetBounds(SCIP_LPI* lpi,   // LP interface structure
                              int firstcol,    // first column to get bounds for
                              int lastcol,     // last column to get bounds for
                              SCIP_Real* lbs,  // array to store lower bound
                                               // values, or NULL
                              SCIP_Real* ubs   // array to store upper bound
                                               // values, or NULL
                              ) {
  SCOPED_TIME_STAT(lpi->stats);
  int index = 0;
  for (ColIndex col(firstcol); col <= ColIndex(lastcol); ++col) {
    if (lbs != NULL) {
      lbs[index] = lpi->linear_program->variable_lower_bounds()[col];
    }
    if (ubs != NULL) {
      ubs[index] = lpi->linear_program->variable_upper_bounds()[col];
    }
    ++index;
  }
  return SCIP_OKAY;
}

// Gets current row sides (i.e. constraint bounds) from LP problem object.
SCIP_RETCODE SCIPlpiGetSides(SCIP_LPI* lpi,    // LP interface structure
                             int firstrow,     // first row to get sides for
                             int lastrow,      // last row to get sides for
                             SCIP_Real* lhss,  // array to store left hand side
                                               // values, or NULL
                             SCIP_Real* rhss   // array to store right hand side
                                               // values, or NULL
                             ) {
  SCOPED_TIME_STAT(lpi->stats);
  int index = 0;
  for (RowIndex row(firstrow); row <= RowIndex(lastrow); ++row) {
    if (lhss != NULL) {
      lhss[index] = lpi->linear_program->constraint_lower_bounds()[row];
    }
    if (rhss != NULL) {
      rhss[index] = lpi->linear_program->constraint_upper_bounds()[row];
    }
    ++index;
  }
  return SCIP_OKAY;
}

// Gets a single coefficient.
SCIP_RETCODE SCIPlpiGetCoef(SCIP_LPI* lpi,  // LP interface structure
                            int row,        // row number of coefficient
                            int col,        // column number of coefficient
                            SCIP_Real* val  // pointer to store the value of the
                                            // coefficient
                            ) {
  SCOPED_TIME_STAT(lpi->stats);
  LOG(FATAL) << "calling SCIPlpiGetCoef";
  return SCIP_OKAY;
}

// --------------------------------------------------------
// Solving Methods
// --------------------------------------------------------

// Common function between the two LPI Solve() functions.
SCIP_RETCODE SolveInternal(SCIP_LPI* lpi) {
  lpi->solver->SetParameters(*(lpi->parameters));
  lpi->lp_time_limit_was_reached = false;
  std::unique_ptr<TimeLimit> time_limit =
      TimeLimit::FromParameters(*lpi->parameters);
  lpi->linear_program->AddSlackVariablesWhereNecessary(false);
  if (!lpi->solver->Solve(*(lpi->linear_program), time_limit.get()).ok()) {
    lpi->linear_program->DeleteSlackVariables();
    return SCIP_LPERROR;
  }
  lpi->lp_time_limit_was_reached = time_limit->LimitReached();
  lpi->linear_program->DeleteSlackVariables();
  VLOG(1)
      << "---> "
      << " status=" << GetProblemStatusString(lpi->solver->GetProblemStatus())
      << " obj=" << lpi->solver->GetObjectiveValue()
      << " iter=" << lpi->solver->GetNumberOfIterations();
  lpi->lp_modified_since_last_solve = false;
  return SCIP_OKAY;
}

// calls primal simplex to solve the LP
SCIP_RETCODE SCIPlpiSolvePrimal(SCIP_LPI* lpi) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiSolvePrimal "
          << lpi->linear_program->num_constraints() << " x "
          << lpi->linear_program->num_variables();
  lpi->parameters->set_use_dual_simplex(false);
  return SolveInternal(lpi);
}

// calls dual simplex to solve the LP
SCIP_RETCODE SCIPlpiSolveDual(SCIP_LPI* lpi) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiSolveDual "
          << lpi->linear_program->num_constraints()<< " x "
          << lpi->linear_program->num_variables();
  lpi->parameters->set_use_dual_simplex(true);
  return SolveInternal(lpi);
}

// calls barrier or interior point algorithm to solve the LP with crossover to
// simplex basis
SCIP_RETCODE SCIPlpiSolveBarrier(SCIP_LPI* lpi,       // LP interface structure
                                 SCIP_Bool crossover  // perform crossover
                                 ) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiSolveBarrier - Not supported";
  return SCIPlpiSolveDual(lpi);
}

// start strong branching - call before any strong branching
SCIP_RETCODE SCIPlpiStartStrongbranch(SCIP_LPI* lpi) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiStartStrongbranch";
  // TODO(fdid): Save state and do all the branching from there.
  return SCIP_OKAY;
}

// end strong branching - call after any strong branching
SCIP_RETCODE SCIPlpiEndStrongbranch(SCIP_LPI* lpi) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiEndStrongbranch";
  // TODO(fdid): Restore the saved state in SCIPlpiStartStrongbranch().
  return SCIP_OKAY;
}

namespace {
bool IsDualBoundValid(ProblemStatus status) {
  return status == ProblemStatus::OPTIMAL ||
      status == ProblemStatus::DUAL_FEASIBLE ||
          status == ProblemStatus::DUAL_UNBOUNDED;
}
}  // namespace

// performs strong branching iterations on one @b fractional candidate
SCIP_RETCODE SCIPlpiStrongbranchFrac(
    SCIP_LPI* lpi,         // LP interface structure
    int col_index,         // column to apply strong branching on
    SCIP_Real psol,        // fractional current primal solution value of column
    int itlim,             // iteration limit for strong branchings
    SCIP_Real* down,       // stores dual bound after branching column down
    SCIP_Real* up,         // stores dual bound after branching column up
    SCIP_Bool* downvalid,  // stores whether the returned down value is a
                           // valid dual bound; otherwise, it can only
                           // be used as an estimate value
    SCIP_Bool* upvalid,    // stores whether the returned up value is
                           // a valid dual bound; otherwise, it can only
                           // be used as an estimate value
    int* iter              // stores total number of strong branching
                           // iterations, or -1; may be NULL
    ) {
  SCOPED_TIME_STAT(lpi->stats);
  CHECK_NOTNULL(down);
  CHECK_NOTNULL(up);
  CHECK_NOTNULL(downvalid);
  CHECK_NOTNULL(upvalid);

  const ColIndex col(col_index);
  const Fractional lb = lpi->linear_program->variable_lower_bounds()[col];
  const Fractional ub = lpi->linear_program->variable_upper_bounds()[col];

  // Configure solver.
  // TODO(fdid): use the iteration limit once glop support incrementality.
  int num_iterations = 0;
  lpi->parameters->set_use_dual_simplex(true);
  lpi->solver->SetParameters(*(lpi->parameters));

  // Down branch.
  const Fractional eps = lpi->parameters->primal_feasibility_tolerance();
  lpi->linear_program->SetVariableBounds(col, lb, EPSCEIL(psol - 1.0, eps));
  std::unique_ptr<TimeLimit> time_limit =
      TimeLimit::FromParameters(*lpi->parameters);
  if (lpi->solver->Solve(*(lpi->linear_program), time_limit.get()).ok()) {
    num_iterations += lpi->solver->GetNumberOfIterations();
    *down = lpi->solver->GetObjectiveValue();
    *downvalid = IsDualBoundValid(lpi->solver->GetProblemStatus());
    VLOG(1) << " down:"
            << " itlim=" << itlim << " col=" << col << " [" << lb << ","
            << EPSCEIL(psol - 1.0, eps) << "]"
            << " obj=" << lpi->solver->GetObjectiveValue() << " status="
            << GetProblemStatusString(lpi->solver->GetProblemStatus())
            << " iter=" << lpi->solver->GetNumberOfIterations();
  } else {
    LOG(WARNING) << "error during solve";
    *down = 0.0;
    *downvalid = false;
  }

  // Up branch.
  lpi->linear_program->SetVariableBounds(col, EPSFLOOR(psol + 1.0, eps), ub);
  if (lpi->solver->Solve(*(lpi->linear_program), time_limit.get()).ok()) {
    num_iterations += lpi->solver->GetNumberOfIterations();
    *up = lpi->solver->GetObjectiveValue();
    *upvalid = IsDualBoundValid(lpi->solver->GetProblemStatus());
    VLOG(1) << " up:  "
            << " itlim=" << itlim << " col=" << col << " ["
            << EPSFLOOR(psol + 1.0, eps) << "," << ub << "]"
            << " obj=" << lpi->solver->GetObjectiveValue() << " status="
            << GetProblemStatusString(lpi->solver->GetProblemStatus())
            << " iter=" << lpi->solver->GetNumberOfIterations();
  } else {
    LOG(WARNING) << "error during solve";
    *up = 0.0;
    *upvalid = false;
  }

  // Restore bound.
  lpi->linear_program->SetVariableBounds(col, lb, ub);
  if (iter != NULL) {
    *iter = num_iterations;
  }
  return SCIP_OKAY;
}

// performs strong branching iterations on given @b fractional candidates
SCIP_RETCODE SCIPlpiStrongbranchesFrac(
    SCIP_LPI* lpi,     // LP interface structure
    int* cols,         // columns to apply strong branching on
    int ncols,         // number of columns
    SCIP_Real* psols,  // fractional current primal solution values of columns
    int itlim,         // iteration limit for strong branchings
    SCIP_Real* down,   // stores dual bounds after branching columns down
    SCIP_Real* up,     // stores dual bounds after branching columns up
    SCIP_Bool* downvalid, SCIP_Bool* upvalid, int* iter) {
  SCOPED_TIME_STAT(lpi->stats);
  LOG(FATAL) << "calling SCIPlpiStrongbranchesFrac";
  return SCIP_OKAY;
}

// performs strong branching iterations on one candidate with @b integral value
SCIP_RETCODE SCIPlpiStrongbranchInt(
    SCIP_LPI* lpi,    // LP interface structure
    int col,          // column to apply strong branching on
    SCIP_Real psol,   // current integral primal solution value of column
    int itlim,        // iteration limit for strong branchings
    SCIP_Real* down,  // stores dual bound after branching column down
    SCIP_Real* up,    // stores dual bound after branching column up
    SCIP_Bool* downvalid, SCIP_Bool* upvalid, int* iter) {
  SCOPED_TIME_STAT(lpi->stats);
  LOG(FATAL) << "calling SCIPlpiStrongbranchInt";
  return SCIP_OKAY;
}

// performs strong branching iterations on given candidates with @b integral
// values
SCIP_RETCODE SCIPlpiStrongbranchesInt(
    SCIP_LPI* lpi,     // LP interface structure
    int* cols,         // columns to apply strong branching on
    int ncols,         // number of columns
    SCIP_Real* psols,  // current integral primal solution values of columns
    int itlim,         // iteration limit for strong branchings
    SCIP_Real* down,   // stores dual bounds after branching columns down
    SCIP_Real* up,     // stores dual bounds after branching columns up
    SCIP_Bool* downvalid, SCIP_Bool* upvalid, int* iter) {
  SCOPED_TIME_STAT(lpi->stats);
  LOG(FATAL) << "calling SCIPlpiStrongbranchesInt";
  return SCIP_OKAY;
}

// --------------------------------------------------------
// Solution Information Methods
// --------------------------------------------------------

// returns whether a solve method was called after the last modification of the
// LP
SCIP_Bool SCIPlpiWasSolved(SCIP_LPI* lpi) {
  SCOPED_TIME_STAT(lpi->stats);

  // TODO(fdid): track this to avoid uneeded resolving.
  return !(lpi->lp_modified_since_last_solve);
}

// gets information about primal and dual feasibility of the current LP
// solution
SCIP_RETCODE SCIPlpiGetSolFeasibility(
    SCIP_LPI* lpi,              // LP interface structure
    SCIP_Bool* primalfeasible,  // stores primal feasibility status
    SCIP_Bool* dualfeasible     // stores dual feasibility status
    ) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiGetSolFeasibility";
  const ProblemStatus status = lpi->solver->GetProblemStatus();
  *primalfeasible = (status == ProblemStatus::OPTIMAL ||
                     status == ProblemStatus::PRIMAL_FEASIBLE);
  *dualfeasible = (status == ProblemStatus::OPTIMAL ||
                   status == ProblemStatus::DUAL_FEASIBLE);
  return SCIP_OKAY;
}

// returns TRUE iff LP is proven to have a primal unbounded ray (but not
// necessary a primal feasible point) this does not necessarily mean, that the
// solver knows and can return the primal ray
SCIP_Bool SCIPlpiExistsPrimalRay(SCIP_LPI* lpi) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiExistsPrimalRay";
  return lpi->solver->GetProblemStatus() == ProblemStatus::PRIMAL_UNBOUNDED;
}

// returns TRUE iff LP is proven to have a primal unbounded ray (but not
// necessary a primal feasible point), and the solver knows and can return the
// primal ray
SCIP_Bool SCIPlpiHasPrimalRay(SCIP_LPI* lpi) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiHasPrimalRay";
  return lpi->solver->GetProblemStatus() == ProblemStatus::PRIMAL_UNBOUNDED;
}

// returns TRUE iff LP is proven to be primal unbounded
SCIP_Bool SCIPlpiIsPrimalUnbounded(SCIP_LPI* lpi) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiIsPrimalUnbounded";
  return lpi->solver->GetProblemStatus() == ProblemStatus::PRIMAL_UNBOUNDED;
}

// returns TRUE iff LP is proven to be primal infeasible
SCIP_Bool SCIPlpiIsPrimalInfeasible(SCIP_LPI* lpi) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiIsPrimalInfeasible";
  const ProblemStatus status = lpi->solver->GetProblemStatus();
  return status == ProblemStatus::DUAL_UNBOUNDED ||
         status == ProblemStatus::PRIMAL_INFEASIBLE;
}

// returns TRUE iff LP is proven to be primal feasible
SCIP_Bool SCIPlpiIsPrimalFeasible(SCIP_LPI* lpi) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiIsPrimalFeasible";
  const ProblemStatus status = lpi->solver->GetProblemStatus();
  return status == ProblemStatus::PRIMAL_FEASIBLE ||
         status == ProblemStatus::OPTIMAL;
}

// returns TRUE iff LP is proven to have a dual unbounded ray (but not
// necessary a dual feasible point); this does not necessarily mean, that the
// solver knows and can return the dual ray
SCIP_Bool SCIPlpiExistsDualRay(SCIP_LPI* lpi) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiExistsDualRay";
  const ProblemStatus status = lpi->solver->GetProblemStatus();
  return status == ProblemStatus::DUAL_UNBOUNDED;
}

// returns TRUE iff LP is proven to have a dual unbounded ray (but not necessary
// a dual feasible point), and the solver knows and can return the dual ray
SCIP_Bool SCIPlpiHasDualRay(SCIP_LPI* lpi) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiHasDualRay";
  const ProblemStatus status = lpi->solver->GetProblemStatus();
  // TODO(fdid): check the sign of SCIPlpiGetDualfarkas()
  return status == ProblemStatus::DUAL_UNBOUNDED;
}

// returns TRUE iff LP is proven to be dual unbounded
SCIP_Bool SCIPlpiIsDualUnbounded(SCIP_LPI* lpi) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiIsDualUnbounded";
  const ProblemStatus status = lpi->solver->GetProblemStatus();
  return status == ProblemStatus::DUAL_UNBOUNDED;
}

// returns TRUE iff LP is proven to be dual infeasible
SCIP_Bool SCIPlpiIsDualInfeasible(SCIP_LPI* lpi) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiIsDualInfeasible";
  const ProblemStatus status = lpi->solver->GetProblemStatus();
  return status == ProblemStatus::PRIMAL_UNBOUNDED ||
         status == ProblemStatus::DUAL_INFEASIBLE;
}

// returns TRUE iff LP is proven to be dual feasible
SCIP_Bool SCIPlpiIsDualFeasible(SCIP_LPI* lpi) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiIsDualFeasible";
  const ProblemStatus status = lpi->solver->GetProblemStatus();
  return status == ProblemStatus::DUAL_FEASIBLE ||
         status == ProblemStatus::OPTIMAL;
}

// returns TRUE iff LP was solved to optimality
SCIP_Bool SCIPlpiIsOptimal(SCIP_LPI* lpi) {
  SCOPED_TIME_STAT(lpi->stats);
  return lpi->solver->GetProblemStatus() == ProblemStatus::OPTIMAL;
}

// returns TRUE iff current LP basis is stable
SCIP_Bool SCIPlpiIsStable(SCIP_LPI* lpi) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiIsStable";
  return lpi->solver->GetProblemStatus() != ProblemStatus::ABNORMAL &&
         lpi->solver->GetProblemStatus() != ProblemStatus::INVALID_PROBLEM;
}

// returns TRUE iff the objective limit was reached
SCIP_Bool SCIPlpiIsObjlimExc(SCIP_LPI* lpi) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiIsObjlimExc";
  return lpi->solver->objective_limit_reached();
}

// returns TRUE iff the iteration limit was reached
SCIP_Bool SCIPlpiIsIterlimExc(SCIP_LPI* lpi) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiIsIterlimExc";
  return lpi->solver->GetNumberOfIterations() >=
         lpi->parameters->max_number_of_iterations();
}

// Returns TRUE iff the time limit was reached.
SCIP_Bool SCIPlpiIsTimelimExc(SCIP_LPI* lpi) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiIsTimelimExc";
  return lpi->lp_time_limit_was_reached;
}

// returns the internal solution status of the solver
int SCIPlpiGetInternalStatus(SCIP_LPI* lpi) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiGetInternalStatus";
  return static_cast<int>(lpi->solver->GetProblemStatus());
}

// tries to reset the internal status of the LP solver in order to ignore an
// instability of the last solving call
SCIP_RETCODE SCIPlpiIgnoreInstability(SCIP_LPI* lpi,  // LP interface structure
                                      SCIP_Bool* success  // pointer to store,
                                                          // whether the
                                                          // instability could
                                                          // be ignored
                                      ) {
  SCOPED_TIME_STAT(lpi->stats);
  LOG(FATAL) << "calling SCIPlpiIgnoreInstability";
  return SCIP_OKAY;
}

// gets objective value of solution
SCIP_RETCODE SCIPlpiGetObjval(SCIP_LPI* lpi,     // LP interface structure
                              SCIP_Real* objval  // stores the objective value
                              ) {
  SCOPED_TIME_STAT(lpi->stats);
  *objval = lpi->solver->GetObjectiveValue();
  return SCIP_OKAY;
}

// gets primal and dual solution vectors for feasible LPs
SCIP_RETCODE SCIPlpiGetSol(
    SCIP_LPI* lpi,        // LP interface structure
    SCIP_Real* objval,    // objective value, may be NULL if not needed
    SCIP_Real* primsol,   // primal solution vector, may be NULL if not needed
    SCIP_Real* dualsol,   // dual solution vector, may be NULL if not needed
    SCIP_Real* activity,  // row activity vector, may be NULL if not needed
    SCIP_Real* redcost    // reduced cost vector, may be NULL if not needed
    ) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiGetSol";
  if (objval != NULL) *objval = lpi->solver->GetObjectiveValue();

  const ColIndex num_cols = lpi->linear_program->num_variables();
  for (ColIndex col(0); col < num_cols; ++col) {
    int i = col.value();
    if (primsol != NULL) primsol[i] = lpi->solver->GetVariableValue(col);
    if (redcost != NULL) redcost[i] = lpi->solver->GetReducedCost(col);
  }

  const RowIndex num_rows = lpi->linear_program->num_constraints();
  for (RowIndex row(0); row < num_rows; ++row) {
    int j = row.value();
    if (dualsol != NULL) dualsol[j] = lpi->solver->GetDualValue(row);
    if (activity != NULL) activity[j] = lpi->solver->GetConstraintActivity(row);
  }
  return SCIP_OKAY;
}

// gets primal ray for unbounded LPs
SCIP_RETCODE SCIPlpiGetPrimalRay(SCIP_LPI* lpi,  // LP interface structure
                                 SCIP_Real* ray  // primal ray
                                 ) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiGetPrimalRay";
  CHECK_NOTNULL(ray);
  const ColIndex num_cols = lpi->linear_program->num_variables();
  const DenseRow& primal_ray = lpi->solver->GetPrimalRay();
  for (ColIndex col(0); col < num_cols; ++col) {
    ray[col.value()] = primal_ray[col];
  }
  return SCIP_OKAY;
}

// gets dual Farkas proof for infeasibility
SCIP_RETCODE SCIPlpiGetDualfarkas(SCIP_LPI* lpi,  // LP interface structure
                                  SCIP_Real* dualfarkas  // dual Farkas row
                                                         // multipliers
                                  ) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiGetDualfarkas";
  CHECK_NOTNULL(dualfarkas);
  const RowIndex num_rows = lpi->linear_program->num_constraints();
  const DenseColumn& dual_ray = lpi->solver->GetDualRay();
  for (RowIndex row(0); row < num_rows; ++row) {
    dualfarkas[row.value()] = dual_ray[row];
  }
  return SCIP_OKAY;
}

// gets the number of LP iterations of the last solve call
SCIP_RETCODE SCIPlpiGetIterations(SCIP_LPI* lpi,   // LP interface structure
                                  int* iterations  // pointer to store the
                                                   // number of iterations of
                                                   // the last solve call
                                  ) {
  SCOPED_TIME_STAT(lpi->stats);
  *iterations = lpi->solver->GetNumberOfIterations();
  return SCIP_OKAY;
}

// gets information about the quality of an LP solution
//
// Such information is usually only available, if also a (maybe not optimal)
// solution is available. The LPI should return SCIP_INVALID for *quality, if
// the requested quantity is not available.
SCIP_RETCODE SCIPlpiGetRealSolQuality(
    SCIP_LPI* lpi,                       // LP interface structure
    SCIP_LPSOLQUALITY qualityindicator,  // indicates which quality should
                                         // be returned
    SCIP_Real* quality                   // pointer to store quality number
    ) {
  SCOPED_TIME_STAT(lpi->stats);
  LOG(FATAL) << "calling SCIPlpiGetRealSolQuality";
  *quality = SCIP_INVALID;
  return SCIP_OKAY;
}

// --------------------------------------------------------
// LP Basis Methods
// --------------------------------------------------------

namespace {

int ConvertGlopVariableStatus(VariableStatus status, Fractional rc) {
  switch (status) {
    case VariableStatus::BASIC:
      return SCIP_BASESTAT_BASIC;
    case VariableStatus::AT_UPPER_BOUND:
      return SCIP_BASESTAT_UPPER;
    case VariableStatus::AT_LOWER_BOUND:
      return SCIP_BASESTAT_LOWER;
    case VariableStatus::FREE:
      return SCIP_BASESTAT_ZERO;
    case VariableStatus::FIXED_VALUE:
      return rc > 0.0 ? SCIP_BASESTAT_LOWER : SCIP_BASESTAT_UPPER;
  }
}

int ConvertGlopConstraintStatus(ConstraintStatus status, Fractional rc) {
  switch (status) {
    case ConstraintStatus::BASIC:
      return SCIP_BASESTAT_BASIC;
    case ConstraintStatus::AT_UPPER_BOUND:
      return SCIP_BASESTAT_UPPER;
    case ConstraintStatus::AT_LOWER_BOUND:
      return SCIP_BASESTAT_LOWER;
    case ConstraintStatus::FREE:
      return SCIP_BASESTAT_ZERO;
    case ConstraintStatus::FIXED_VALUE:
      return rc > 0.0 ? SCIP_BASESTAT_LOWER : SCIP_BASESTAT_UPPER;
  }
}

}  // namespace

// gets current basis status for columns and rows; arrays must be large enough
// to store the basis status
SCIP_RETCODE SCIPlpiGetBase(SCIP_LPI* lpi,  // LP interface structure
                            int* cstat,  // array to store column basis status,
                                         // or NULL
                            int* rstat   // array to store row basis status, or
                                         // NULL
                            ) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "calling SCIPlpiGetBase";
  CHECK_EQ(lpi->solver->GetProblemStatus(), ProblemStatus::OPTIMAL);
  if (cstat != NULL) {
    const ColIndex num_cols = lpi->linear_program->num_variables();
    for (ColIndex col(0); col < num_cols; ++col) {
      int i = col.value();
      cstat[i] = ConvertGlopVariableStatus(lpi->solver->GetVariableStatus(col),
                                           lpi->solver->GetReducedCost(col));
    }
  }
  if (rstat != NULL) {
    const RowIndex num_rows = lpi->linear_program->num_constraints();
    for (RowIndex row(0); row < num_rows; ++row) {
      int i = row.value();
      rstat[i] =
          ConvertGlopConstraintStatus(lpi->solver->GetConstraintStatus(row),
                                      lpi->solver->GetDualValue(row));
    }
  }
  return SCIP_OKAY;
}

// sets current basis status for columns and rows
SCIP_RETCODE SCIPlpiSetBase(SCIP_LPI* lpi,     // LP interface structure
                            const int* cstat,  // array with column basis status
                            const int* rstat   // array with row basis status
                            ) {
  SCOPED_TIME_STAT(lpi->stats);
  LOG(FATAL) << "calling SCIPlpiSetBase";
  return SCIP_OKAY;
}

// returns the indices of the basic columns and rows
SCIP_RETCODE SCIPlpiGetBasisInd(SCIP_LPI* lpi,  // LP interface structure
                                int* bind       // basic column n gives value n,
                                                // basic row m gives value -1-m
                                ) {
  SCOPED_TIME_STAT(lpi->stats);
  CHECK_NOTNULL(bind);
  VLOG(1) << "calling SCIPlpiGetBasisInd";

  // The order is important!
  const ColIndex num_cols = lpi->linear_program->num_variables();
  const RowIndex num_rows = lpi->linear_program->num_constraints();
  for (RowIndex row(0); row < num_rows; ++row) {
    const ColIndex col = lpi->solver->GetBasis(row);
    if (col < num_cols) {
      bind[row.value()] = col.value();
    } else {
      CHECK_LT(col, num_cols.value() + num_rows.value());
      bind[row.value()] = -1 - (col - num_cols).value();
    }
  }
  return SCIP_OKAY;
}

// get dense row of inverse basis matrix B^-1
SCIP_RETCODE SCIPlpiGetBInvRow(SCIP_LPI* lpi,    // LP interface structure
                               int r,            // row number
                               SCIP_Real* coef,  // ptr to save row coeffs
                               int* inds,        // ptr to save nonzero indices
                               int* ninds        // ptr to save len(inds) or -1
                               ) {
  SCOPED_TIME_STAT(lpi->stats);
  CHECK_NOTNULL(coef);
  DenseRow solution;
  ColIndexVector non_zero_positions;
  lpi->solver->GetBasisFactorization()
      .LeftSolveForUnitRow(ColIndex(r), &solution, &non_zero_positions);
  const ColIndex num_cols = solution.size();
  for (ColIndex col(0); col < num_cols; ++col) {
    coef[col.value()] = solution[col];
  }

  // Only returns a dense vector, so set ninds to -1.
  if (ninds != NULL) {
    *ninds = -1;
  }

  return SCIP_OKAY;
}

// get dense column of inverse basis matrix B^-1
SCIP_RETCODE SCIPlpiGetBInvCol(
    SCIP_LPI* lpi,  // LP interface structure
    int c,          // column number of B^-1; this is NOT the number of the
                    // column in the LP; you have to call SCIPlpiGetBasisInd()
                    // to get the array which links the B^-1 column numbers to
                    // the row and column numbers of the LP! c must be between 0
                    // and nrows-1, since the basis has the size nrows * nrows
    SCIP_Real* coef,  // pointer to store the coefficients of the column
    int* inds,        // ptr to save nonzero indices
    int* ninds        // ptr to save len(inds) or -1
    ) {
  SCOPED_TIME_STAT(lpi->stats);
  LOG(FATAL) << "calling SCIPlpiGetBInvCol";
  return SCIP_OKAY;
}

// get dense row of inverse basis matrix times constraint matrix B^-1 * A
SCIP_RETCODE SCIPlpiGetBInvARow(SCIP_LPI* lpi,  // LP interface structure
                                int r,          // row number
                                const SCIP_Real* binvrow,  // row in (A_B)^-1
                                                           // from prior call to
                                // SCIPlpiGetBInvRow(), or NULL
                                SCIP_Real* coef,  // ptr to save coeffs
                                int* inds,        // ptr to save nonzero indices
                                int* ninds        // ptr to save len(inds) or -1
                                ) {
  SCOPED_TIME_STAT(lpi->stats);
  LOG(FATAL) << "calling SCIPlpiGetBInvARow";
  return SCIP_OKAY;
}

// get dense column of inverse basis matrix times constraint matrix B^-1 * A
SCIP_RETCODE SCIPlpiGetBInvACol(SCIP_LPI* lpi,    // LP interface structure
                                int c,            // column number
                                SCIP_Real* coef,  // ptr to save coeffs
                                int* inds,        // ptr to save nonzero indices
                                int* ninds        // ptr to save len(inds) or -1
                                ) {
  SCOPED_TIME_STAT(lpi->stats);
  LOG(FATAL) << "calling SCIPlpiGetBInvACol";
  return SCIP_OKAY;
}

// --------------------------------------------------------
// LPi State Methods
// --------------------------------------------------------

// SCIP_LPiState stores basis information and is implemented by the glop
// BasisState class. However, because in type_lpi.h there is
//     typedef struct SCIP_LPiState SCIP_LPISTATE;
// We cannot just use a typedef here and SCIP_LPiState needs to be a struct.
struct SCIP_LPiState : BasisState {};

// stores LPi state (like basis information) into lpistate object
SCIP_RETCODE SCIPlpiGetState(SCIP_LPI* lpi,            // LP interface structure
                             BMS_BLKMEM* blkmem,       // block memory
                             SCIP_LPISTATE** lpistate  // pointer to LPi state
                                                       // information
                             ) {
  SCOPED_TIME_STAT(lpi->stats);
  *lpistate =
      static_cast<SCIP_LPISTATE*>(new BasisState(lpi->solver->GetState()));
  return SCIP_OKAY;
}

// loads LPi state (like basis information) into solver; note that the LP might
// have been extended with additional columns and rows since the state was
// stored with SCIPlpiGetState()
SCIP_RETCODE SCIPlpiSetState(
    SCIP_LPI* lpi,                 // LP interface structure
    BMS_BLKMEM* blkmem,            // block memory
    const SCIP_LPISTATE* lpistate  // LPi state information (like basis
                                   // information)
    ) {
  SCOPED_TIME_STAT(lpi->stats);
  CHECK_NOTNULL(lpistate);
  lpi->solver->LoadStateForNextSolve(*lpistate);
  return SCIP_OKAY;
}

// clears current LPi state (like basis information) of the solver
SCIP_RETCODE SCIPlpiClearState(SCIP_LPI* lpi) {
  SCOPED_TIME_STAT(lpi->stats);
  lpi->solver->ClearStateForNextSolve();
  return SCIP_OKAY;
}

// frees LPi state information
SCIP_RETCODE SCIPlpiFreeState(SCIP_LPI* lpi,       // LP interface structure
                              BMS_BLKMEM* blkmem,  // block memory
                              SCIP_LPISTATE** lpistate  // pointer to LPi state
                                                        // information
                              ) {
  SCOPED_TIME_STAT(lpi->stats);
  CHECK_NOTNULL(lpistate);
  CHECK_NOTNULL(*lpistate);
  delete *lpistate;
  *lpistate = NULL;
  return SCIP_OKAY;
}

// checks, whether the given LPi state contains simplex basis information
SCIP_Bool SCIPlpiHasStateBasis(SCIP_LPI* lpi,  // LP interface structure
                               SCIP_LPISTATE* lpistate  // LPi state information
                                                        // (like basis
                                                        // information)
                               ) {
  SCOPED_TIME_STAT(lpi->stats);
  return lpistate != NULL;
}

// reads LPi state (like basis information from a file
SCIP_RETCODE SCIPlpiReadState(SCIP_LPI* lpi,     // LP interface structure
                              const char* fname  // file name
                              ) {
  SCOPED_TIME_STAT(lpi->stats);
  LOG(FATAL) << "Calling SCIPlpiReadState";
  return SCIP_OKAY;
}

// writes LPi state (like basis information) to a file
SCIP_RETCODE SCIPlpiWriteState(SCIP_LPI* lpi,     // LP interface structure
                               const char* fname  // file name
                               ) {
  SCOPED_TIME_STAT(lpi->stats);
  LOG(FATAL) << "Calling SCIPlpiWriteState";
  return SCIP_OKAY;
}

// --------------------------------------------------------
// LP Pricing Norms Methods
// --------------------------------------------------------

// SCIP_LPiNorms stores norm information so they are not recomputed from one
// state to the next.
//
// TODO(fdid): Implement this.
struct SCIP_LPiNorms {};

// stores LPi pricing norms information
// @todo store primal norms as well?
SCIP_RETCODE SCIPlpiGetNorms(
    SCIP_LPI* lpi,            // LP interface structure
    BMS_BLKMEM* blkmem,       // block memory
    SCIP_LPINORMS** lpinorms  // pointer to LPi pricing norms information
    ) {
  SCOPED_TIME_STAT(lpi->stats);
  return SCIP_OKAY;
}

// loads LPi pricing norms into solver; note that the LP might have been
// extended with additional columns and rows since the state was stored with
// SCIPlpiGetNorms()
SCIP_RETCODE SCIPlpiSetNorms(
    SCIP_LPI* lpi,                 // LP interface structure
    BMS_BLKMEM* blkmem,            // block memory
    const SCIP_LPINORMS* lpinorms  // LPi pricing norms information
    ) {
  SCOPED_TIME_STAT(lpi->stats);
  return SCIP_OKAY;
}

// frees pricing norms information
SCIP_RETCODE SCIPlpiFreeNorms(
    SCIP_LPI* lpi,            // LP interface structure
    BMS_BLKMEM* blkmem,       // block memory
    SCIP_LPINORMS** lpinorms  // pointer to LPi pricing norms information
    ) {
  SCOPED_TIME_STAT(lpi->stats);
  return SCIP_OKAY;
}

// --------------------------------------------------------
// Parameter Methods
// --------------------------------------------------------

// gets integer parameter of LP
SCIP_RETCODE SCIPlpiGetIntpar(SCIP_LPI* lpi,      // LP interface structure
                              SCIP_LPPARAM type,  // parameter number
                              int* ival  // buffer to store the parameter value
                              ) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "Calling SCIPlpiGetIntpar type=" << type;
  switch (type) {
    case SCIP_LPPAR_FROMSCRATCH:
      *ival = lpi->from_scratch;
      break;
    case SCIP_LPPAR_FASTMIP:
      *ival = lpi->fast_mip;
      break;
    case SCIP_LPPAR_LPINFO:
      *ival = lpi->lp_info;
      break;
    case SCIP_LPPAR_LPITLIM:
      *ival = lpi->parameters->max_number_of_iterations();
      break;
    case SCIP_LPPAR_PRESOLVING:
      *ival = lpi->parameters->use_preprocessing();
      break;
    case SCIP_LPPAR_PRICING:
      *ival = lpi->pricing;
      break;
    case SCIP_LPPAR_SCALING:
      *ival = lpi->parameters->use_scaling();
      break;
    default:
      return SCIP_PARAMETERUNKNOWN;
  }
  return SCIP_OKAY;
}

// sets integer parameter of LP
SCIP_RETCODE SCIPlpiSetIntpar(SCIP_LPI* lpi,      // LP interface structure
                              SCIP_LPPARAM type,  // parameter number
                              int ival            // parameter value
                              ) {
  SCOPED_TIME_STAT(lpi->stats);
  switch (type) {
    case SCIP_LPPAR_FROMSCRATCH:
      lpi->from_scratch = ival;
      VLOG(1) << "type=" << type << " SCIP_LPPAR_FROMSCRATCH " << ival;
      break;
    case SCIP_LPPAR_FASTMIP:
      lpi->fast_mip = ival;
      VLOG(1) << "type=" << type << " SCIP_LPPAR_FASTMIP " << ival;
      break;
    case SCIP_LPPAR_LPINFO:
      lpi->lp_info = ival;
      VLOG(1) << "type=" << type << " SCIP_LPPAR_LPINFO " << ival;
      break;
    case SCIP_LPPAR_LPITLIM:
      lpi->parameters->set_max_number_of_iterations(ival);
      VLOG(1) << "type=" << type << " SCIP_LPPAR_LPITLIM " << ival;
      break;
    case SCIP_LPPAR_PRESOLVING:
      lpi->parameters->set_use_preprocessing(ival);
      VLOG(1) << "type=" << type << " SCIP_LPPAR_PRESOLVING " << ival;
      break;
    case SCIP_LPPAR_PRICING:
      lpi->pricing = ival;
      VLOG(1) << "type=" << type << " SCIP_LPPAR_PRICING " << ival;
      break;
    case SCIP_LPPAR_SCALING:
      lpi->parameters->set_use_scaling(ival);
      VLOG(1) << "type=" << type << " SCIP_LPPAR_SCALING " << ival;
      break;
    default:
      VLOG(1) << "Unknown parameter " << type << " val=" << ival;
      return SCIP_PARAMETERUNKNOWN;
  }
  return SCIP_OKAY;
}

// gets floating point parameter of LP
SCIP_RETCODE SCIPlpiGetRealpar(SCIP_LPI* lpi,      // LP interface structure
                               SCIP_LPPARAM type,  // parameter number
                               SCIP_Real* dval  // buffer to store the parameter
                                                // value
                               ) {
  SCOPED_TIME_STAT(lpi->stats);
  VLOG(1) << "Calling SCIPlpiGetRealpar type=" << type;
  switch (type) {
    case SCIP_LPPAR_FEASTOL:
      *dval = lpi->parameters->primal_feasibility_tolerance();
      break;
    case SCIP_LPPAR_DUALFEASTOL:
      *dval = lpi->parameters->dual_feasibility_tolerance();
      break;
    case SCIP_LPPAR_LOBJLIM:
      *dval = lpi->parameters->objective_lower_limit();
      break;
    case SCIP_LPPAR_UOBJLIM:
      *dval = lpi->parameters->objective_upper_limit();
      break;
    case SCIP_LPPAR_LPTILIM:
      *dval = lpi->parameters->max_time_in_seconds();
      break;
    case SCIP_LPPAR_ROWREPSWITCH:
      *dval = lpi->rowrepswitch;
      break;
    default:
      return SCIP_PARAMETERUNKNOWN;
  }
  return SCIP_OKAY;
}

// sets floating point parameter of LP
SCIP_RETCODE SCIPlpiSetRealpar(SCIP_LPI* lpi,      // LP interface structure
                               SCIP_LPPARAM type,  // parameter number
                               SCIP_Real dval      // parameter value
                               ) {
  SCOPED_TIME_STAT(lpi->stats);
  switch (type) {
    case SCIP_LPPAR_FEASTOL:
      VLOG(1) << "type=" << type << " SCIP_LPPAR_FEASTOL " << dval;
      lpi->parameters->set_primal_feasibility_tolerance(dval);
      break;
    case SCIP_LPPAR_DUALFEASTOL:
      lpi->parameters->set_dual_feasibility_tolerance(dval);
      VLOG(1) << "type=" << type << " SCIP_LPPAR_DUALFEASTOL " << dval;
      break;
    case SCIP_LPPAR_LOBJLIM:
      lpi->parameters->set_objective_lower_limit(dval);
      VLOG(1) << "type=" << type << " SCIP_LPPAR_LOBJLIM " << dval;
      break;
    case SCIP_LPPAR_UOBJLIM:
      lpi->parameters->set_objective_upper_limit(dval);
      VLOG(1) << "type=" << type << " SCIP_LPPAR_UOBJLIM " << dval;
      break;
    case SCIP_LPPAR_LPTILIM:
      VLOG(1) << "type=" << type << " SCIP_LPPAR_LPTILIM " << dval << "(s)";
      lpi->parameters->set_max_time_in_seconds(dval);
      break;
    case SCIP_LPPAR_ROWREPSWITCH:
      lpi->rowrepswitch = dval;
      VLOG(1) << "type=" << type << " SCIP_LPPAR_ROWREPSWITCH " << dval;
      break;
    default:
      VLOG(1) << "Unknown parameter " << type << " val=" << dval;
      return SCIP_PARAMETERUNKNOWN;
  }
  return SCIP_OKAY;
}

// --------------------------------------------------------
// Numerical Methods
// --------------------------------------------------------

// returns value treated as infinity in the LP solver
SCIP_Real SCIPlpiInfinity(SCIP_LPI* lpi) {
  SCOPED_TIME_STAT(lpi->stats);
  return std::numeric_limits<SCIP_Real>::infinity();
}

// checks if given value is treated as infinity in the LP solver
SCIP_Bool SCIPlpiIsInfinity(SCIP_LPI* lpi,  // LP interface structure
                            SCIP_Real val   // value to be checked for infinity
                            ) {
  SCOPED_TIME_STAT(lpi->stats);
  return val == std::numeric_limits<SCIP_Real>::infinity();
}

// --------------------------------------------------------
// File Interface Methods
// --------------------------------------------------------

// reads LP from a file
SCIP_RETCODE SCIPlpiReadLP(SCIP_LPI* lpi,     // LP interface structure
                           const char* fname  // file name
                           ) {
  SCOPED_TIME_STAT(lpi->stats);
  LOG(FATAL) << "calling SCIPlpiReadLP";
  return SCIP_OKAY;
}

// writes LP to a file
SCIP_RETCODE SCIPlpiWriteLP(SCIP_LPI* lpi,     // LP interface structure
                            const char* fname  // file name
                            ) {
  SCOPED_TIME_STAT(lpi->stats);
  LOG(FATAL) << "calling SCIPlpiWriteLP";
  return SCIP_OKAY;
}
