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


#include "lp_data/lp_data.h"

#include <algorithm>
#include <cmath>
#include "base/hash.h"
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/join.h"
#include "lp_data/lp_print_utils.h"
#include "lp_data/lp_utils.h"

namespace operations_research {
namespace glop {

namespace {

// This should be the same as DCHECK(AreBoundsValid()), but the DCHECK() are
// split to give more meaningful information to the user in case of failure.
void DebugCheckBoundsValid(Fractional lower_bound, Fractional upper_bound) {
  DCHECK(!isnan(lower_bound));
  DCHECK(!isnan(upper_bound));
  DCHECK(!(lower_bound == kInfinity && upper_bound == kInfinity));
  DCHECK(!(lower_bound == -kInfinity && upper_bound == -kInfinity));
  DCHECK_LE(lower_bound, upper_bound);
  DCHECK(AreBoundsValid(lower_bound, upper_bound));
}

// Returns true if the bounds are the ones of a free or boxed row. Note that
// a fixed row is not counted as boxed.
bool AreBoundsFreeOrBoxed(Fractional lower_bound, Fractional upper_bound) {
  if (lower_bound == -kInfinity && upper_bound == kInfinity) return true;
  if (lower_bound != -kInfinity && upper_bound != kInfinity &&
      lower_bound != upper_bound)
    return true;
  return false;
}

template <class I, class T>
double Average(const ITIVector<I, T>& v) {
  const size_t size = v.size();
  DCHECK_LT(0, size);
  double sum = 0.0;
  double n = 0.0;  // n is used in a calculation involving doubles.
  for (I i(0); i < size; ++i) {
    if (v[i] == 0.0) continue;
    ++n;
    sum += static_cast<double>(v[i].value());
  }
  return n == 0.0 ? 0.0 : sum / n;
}

template <class I, class T>
double StandardDeviation(const ITIVector<I, T>& v) {
  const size_t size = v.size();
  double n = 0.0;  // n is used in a calculation involving doubles.
  double sigma_square = 0.0;
  double sigma = 0.0;
  for (I i(0); i < size; ++i) {
    double sample = static_cast<double>(v[i].value());
    if (sample == 0.0) continue;
    sigma_square += sample * sample;
    sigma += sample;
    ++n;
  }
  return n == 0.0 ? 0.0 : sqrt((sigma_square - sigma * sigma / n) / n);
}

// Returns 0 when the vector is empty.
template <class I, class T>
T GetMaxElement(const ITIVector<I, T>& v) {
  const size_t size = v.size();
  if (size == 0) {
    return T(0);
  }

  T max_index = v[I(0)];
  for (I i(1); i < size; ++i) {
    if (max_index < v[i]) {
      max_index = v[i];
    }
  }
  return max_index;
}

}  // anonymous namespace

// --------------------------------------------------------
// LinearProgram
// --------------------------------------------------------
LinearProgram::LinearProgram()
    : matrix_(),
      transpose_matrix_(),
      constraint_lower_bounds_(),
      constraint_upper_bounds_(),
      constraint_names_(),
      objective_coefficients_(),
      variable_lower_bounds_(),
      variable_upper_bounds_(),
      variable_names_(),
      is_variable_integer_(),
      integer_variables_list_(),
      variable_table_(),
      constraint_table_(),
      objective_offset_(0.0),
      maximize_(false),
      columns_are_known_to_be_clean_(true),
      transpose_matrix_is_consistent_(true),
      integer_variables_list_is_consistent_(true),
      name_() {}

void LinearProgram::Clear() {
  matrix_.Clear();
  transpose_matrix_.Clear();

  constraint_lower_bounds_.clear();
  constraint_upper_bounds_.clear();
  constraint_names_.clear();

  objective_coefficients_.clear();
  variable_lower_bounds_.clear();
  variable_upper_bounds_.clear();
  is_variable_integer_.clear();
  integer_variables_list_.clear();
  variable_names_.clear();

  constraint_table_.clear();
  variable_table_.clear();

  maximize_ = false;
  objective_offset_ = 0.0;
  columns_are_known_to_be_clean_ = true;
  transpose_matrix_is_consistent_ = true;
  integer_variables_list_is_consistent_ = true;
  name_.clear();
}

ColIndex LinearProgram::CreateNewVariable() {
  objective_coefficients_.push_back(0.0);
  variable_lower_bounds_.push_back(0);
  variable_upper_bounds_.push_back(kInfinity);
  is_variable_integer_.push_back(false);
  variable_names_.push_back("");
  transpose_matrix_is_consistent_ = false;
  return matrix_.AppendEmptyColumn();
}

RowIndex LinearProgram::CreateNewConstraint() {
  const RowIndex row(constraint_names_.size());
  matrix_.SetNumRows(row + 1);
  constraint_lower_bounds_.push_back(Fractional(0.0));
  constraint_upper_bounds_.push_back(Fractional(0.0));
  constraint_names_.push_back("");
  transpose_matrix_is_consistent_ = false;
  return row;
}

ColIndex LinearProgram::FindOrCreateVariable(const std::string& variable_id) {
  const hash_map<std::string, ColIndex>::iterator it =
      variable_table_.find(variable_id);
  if (it != variable_table_.end()) {
    return it->second;
  } else {
    const ColIndex col = CreateNewVariable();
    variable_names_[col] = variable_id;
    variable_table_[variable_id] = col;
    return col;
  }
}

RowIndex LinearProgram::FindOrCreateConstraint(const std::string& constraint_id) {
  const hash_map<std::string, RowIndex>::iterator it =
      constraint_table_.find(constraint_id);
  if (it != constraint_table_.end()) {
    return it->second;
  } else {
    const RowIndex row = CreateNewConstraint();
    constraint_names_[row] = constraint_id;
    constraint_table_[constraint_id] = row;
    return row;
  }
}

void LinearProgram::SetVariableName(ColIndex col, const std::string& name) {
  variable_names_[col] = name;
}

void LinearProgram::SetConstraintName(RowIndex row, const std::string& name) {
  constraint_names_[row] = name;
}

void LinearProgram::SetVariableBounds(ColIndex col, Fractional lower_bound,
                                      Fractional upper_bound) {
  DebugCheckBoundsValid(lower_bound, upper_bound);
  variable_lower_bounds_[col] = lower_bound;
  variable_upper_bounds_[col] = upper_bound;
}

void LinearProgram::SetVariableIntegrality(ColIndex col, bool is_integer) {
  integer_variables_list_is_consistent_ &=
      (is_variable_integer_[col] == is_integer);
  is_variable_integer_[col] = is_integer;
}

void LinearProgram::UpdateAllIntegerVariableLists() const {
  if (integer_variables_list_is_consistent_) return;
  integer_variables_list_.clear();
  binary_variables_list_.clear();
  non_binary_variables_list_.clear();
  const ColIndex num_cols = num_variables();
  for (ColIndex col(0); col < num_cols; ++col) {
    if (is_variable_integer_[col]) {
      integer_variables_list_.push_back(col);
      if (IsVariableBinary(col)) {
        binary_variables_list_.push_back(col);
      } else {
        non_binary_variables_list_.push_back(col);
      }
    }
  }
  integer_variables_list_is_consistent_ = true;
}

const std::vector<ColIndex>& LinearProgram::IntegerVariablesList() const {
  UpdateAllIntegerVariableLists();
  return integer_variables_list_;
}

const std::vector<ColIndex>& LinearProgram::BinaryVariablesList() const {
  UpdateAllIntegerVariableLists();
  return binary_variables_list_;
}

const std::vector<ColIndex>& LinearProgram::NonBinaryVariablesList() const {
  UpdateAllIntegerVariableLists();
  return non_binary_variables_list_;
}


bool LinearProgram::IsVariableBinary(ColIndex col) const {
  // TODO(user, bdb): bounds of binary variables (and of integer ones) should
  // be integer. Add a preprocessor for that.
  return is_variable_integer_[col] &&
         (variable_lower_bounds_[col] < kEpsilon) &&
         (variable_lower_bounds_[col] > Fractional(-1)) &&
         (variable_upper_bounds_[col] > Fractional(1) - kEpsilon) &&
         (variable_upper_bounds_[col] < 2);
}

void LinearProgram::SetConstraintBounds(RowIndex row, Fractional lower_bound,
                                        Fractional upper_bound) {
  DebugCheckBoundsValid(lower_bound, upper_bound);
  ResizeRowsIfNeeded(row);
  constraint_lower_bounds_[row] = lower_bound;
  constraint_upper_bounds_[row] = upper_bound;
}

void LinearProgram::SetCoefficient(RowIndex row, ColIndex col,
                                   Fractional value) {
  DCHECK(IsFinite(value));
  ResizeRowsIfNeeded(row);
  columns_are_known_to_be_clean_ = false;
  transpose_matrix_is_consistent_ = false;
  matrix_.mutable_column(col)->SetCoefficient(row, value);
}

void LinearProgram::SetObjectiveCoefficient(ColIndex col, Fractional value) {
  DCHECK(IsFinite(value));
  objective_coefficients_[col] = value;
}

void LinearProgram::SetObjectiveOffset(Fractional objective_offset) {
  DCHECK(IsFinite(objective_offset));
  objective_offset_ = objective_offset;
}

void LinearProgram::SetMaximizationProblem(bool maximize) {
  maximize_ = maximize;
}

void LinearProgram::CleanUp() {
  if (columns_are_known_to_be_clean_) return;
  matrix_.CleanUp();
  columns_are_known_to_be_clean_ = true;
  transpose_matrix_is_consistent_ = false;
}

bool LinearProgram::IsCleanedUp() const {
  if (columns_are_known_to_be_clean_) return true;
  columns_are_known_to_be_clean_ = matrix_.IsCleanedUp();
  return columns_are_known_to_be_clean_;
}

std::string LinearProgram::GetVariableName(ColIndex col) const {
  return col >= variable_names_.size() || variable_names_[col].empty()
             ? StringPrintf("c%d", col.value())
             : variable_names_[col];
}

std::string LinearProgram::GetConstraintName(RowIndex row) const {
  return row >= constraint_names_.size() || constraint_names_[row].empty()
             ? StringPrintf("r%d", row.value())
             : constraint_names_[row];
}

const SparseMatrix& LinearProgram::GetTransposeSparseMatrix() const {
  if (!transpose_matrix_is_consistent_) {
    transpose_matrix_.PopulateFromTranspose(matrix_);
    transpose_matrix_is_consistent_ = true;
  }
  DCHECK_EQ(transpose_matrix_.num_rows().value(), matrix_.num_cols().value());
  DCHECK_EQ(transpose_matrix_.num_cols().value(), matrix_.num_rows().value());
  return transpose_matrix_;
}

SparseMatrix* LinearProgram::GetMutableTransposeSparseMatrix() {
  if (!transpose_matrix_is_consistent_) {
    transpose_matrix_.PopulateFromTranspose(matrix_);
  }
  // This enables a client to start modifying the matrix and then abort and not
  // call UseTransposeMatrixAsReference(). Then, the other client of
  // GetTransposeSparseMatrix() will still see the correct matrix.
  transpose_matrix_is_consistent_ = false;
  return &transpose_matrix_;
}

void LinearProgram::UseTransposeMatrixAsReference() {
  DCHECK_EQ(transpose_matrix_.num_rows().value(), matrix_.num_cols().value());
  DCHECK_EQ(transpose_matrix_.num_cols().value(), matrix_.num_rows().value());
  matrix_.PopulateFromTranspose(transpose_matrix_);
  transpose_matrix_is_consistent_ = true;
}

void LinearProgram::ClearTransposeMatrix() {
  transpose_matrix_.Clear();
  transpose_matrix_is_consistent_ = false;
}

const SparseColumn& LinearProgram::GetSparseColumn(ColIndex col) const {
  return matrix_.column(col);
}

SparseColumn* LinearProgram::GetMutableSparseColumn(ColIndex col) {
  columns_are_known_to_be_clean_ = false;
  transpose_matrix_is_consistent_ = false;
  return matrix_.mutable_column(col);
}

Fractional LinearProgram::GetObjectiveCoefficientForMinimizationVersion(
    ColIndex col) const {
  return maximize_ ? -objective_coefficients()[col]
                   : objective_coefficients()[col];
}

std::string LinearProgram::GetDimensionString() const {
  return StringPrintf("%d rows, %d columns, %lld entries",
                      num_constraints().value(), num_variables().value(),
                      num_entries().value());
}

bool LinearProgram::IsSolutionFeasible(
    const DenseRow& solution, const Fractional absolute_tolerance) const {
  if (solution.size() != num_variables()) return false;
  if (!IsValid()) return false;
  const ColIndex num_cols = num_variables();
  for (ColIndex col = ColIndex(0); col < num_cols; ++col) {
    const Fractional lb_error = variable_lower_bounds()[col] - solution[col];
    const Fractional ub_error = solution[col] - variable_upper_bounds()[col];
    if (lb_error > absolute_tolerance || ub_error > absolute_tolerance)
      return false;
  }
  const SparseMatrix& transpose = GetTransposeSparseMatrix();
  const RowIndex num_rows = num_constraints();
  for (RowIndex row = RowIndex(0); row < num_rows; ++row) {
    const Fractional sum =
        ScalarProduct(solution, transpose.column(RowToColIndex(row)));
    // In case there are two or more infinite values in the solution with
    // opposite coefficients.
    if (isnan(sum)) return false;
    const Fractional lb_error = constraint_lower_bounds()[row] - sum;
    const Fractional ub_error = sum - constraint_upper_bounds()[row];
    if (lb_error > absolute_tolerance || ub_error > absolute_tolerance)
      return false;
  }
  return true;
}

std::string LinearProgram::Dump() const {
  // Objective line.
  std::string output = maximize_ ? "max:" : "min:";
  if (objective_offset_ != 0.0) {
    output += Stringify(objective_offset_);
  }
  const ColIndex num_cols = num_variables();
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional coeff = objective_coefficients()[col];
    if (coeff != 0.0) {
      output += StringifyMonomial(coeff, GetVariableName(col), false);
    }
  }
  output.append(";\n");

  // Constraints.
  const RowIndex num_rows = num_constraints();
  for (RowIndex row(0); row < num_rows; ++row) {
    const Fractional lower_bound = constraint_lower_bounds()[row];
    const Fractional upper_bound = constraint_upper_bounds()[row];
    output += GetConstraintName(row);
    output += ":";
    if (AreBoundsFreeOrBoxed(lower_bound, upper_bound)) {
      output += " ";
      output += Stringify(lower_bound);
      output += " <=";
    }
    for (ColIndex col(0); col < num_cols; ++col) {
      const Fractional coeff = matrix_.LookUpValue(row, col);
      output += StringifyMonomial(coeff, GetVariableName(col), false);
    }
    if (AreBoundsFreeOrBoxed(lower_bound, upper_bound)) {
      output += " <= ";
      output += Stringify(upper_bound);
    } else if (lower_bound == upper_bound) {
      output += " = ";
      output += Stringify(upper_bound);
    } else if (lower_bound != -kInfinity) {
      output += " >= ";
      output += Stringify(lower_bound);
    } else if (lower_bound != kInfinity) {
      output += " <= ";
      output += Stringify(upper_bound);
    }
    output += ";\n";
  }

  // Variables.
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional lower_bound = variable_lower_bounds()[col];
    const Fractional upper_bound = variable_upper_bounds()[col];
    if (AreBoundsFreeOrBoxed(lower_bound, upper_bound)) {
      output += Stringify(lower_bound);
      output += " <= ";
    }
    output += GetVariableName(col);
    if (AreBoundsFreeOrBoxed(lower_bound, upper_bound)) {
      output += " <= ";
      output += Stringify(upper_bound);
    } else if (lower_bound == upper_bound) {
      output += " = ";
      output += Stringify(upper_bound);
    } else if (lower_bound != -kInfinity) {
      output += " >= ";
      output += Stringify(lower_bound);
    } else if (lower_bound != kInfinity) {
      output += " <= ";
      output += Stringify(upper_bound);
    }
    output += ";\n";
  }

  // Integer variables.
  // TODO(user): if needed provide similar output for binary variables.
  const std::vector<ColIndex>& integer_variables = IntegerVariablesList();
  if (integer_variables.size() > 0) {
    output += "int";
    for (ColIndex col : integer_variables) {
      output += " ";
      output += GetVariableName(col);
    }
    output += ";\n";
  }

  return output;
}

std::string LinearProgram::GetProblemStats() const {
  return ProblemStatFormatter("%d,%d,%lld,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                              "%d,%d,%d,%d");
}

std::string LinearProgram::GetPrettyProblemStats() const {
  return ProblemStatFormatter(
      "Number of rows                               : %d\n"
      "Number of variables in file                  : %d\n"
      "Number of entries (non-zeros)                : %lld\n"
      "Number of entries in the objective           : %d\n"
      "Number of entries in the right-hand side     : %d\n"
      "Number of <= constraints                     : %d\n"
      "Number of >= constraints                     : %d\n"
      "Number of = constraints                      : %d\n"
      "Number of range constraints                  : %d\n"
      "Number of non-negative variables             : %d\n"
      "Number of boxed variables                    : %d\n"
      "Number of free variables                     : %d\n"
      "Number of fixed variables                    : %d\n"
      "Number of other variables                    : %d\n"
      "Number of integer variables                  : %d\n"
      "Number of binary variables                   : %d\n"
      "Number of non-binary integer variables       : %d\n"
      "Number of continuous variables               : %d\n");
}

std::string LinearProgram::GetNonZeroStats() const {
  return NonZeroStatFormatter("%.2f%%,%d,%.2f,%.2f,%d,%.2f,%.2f");
}

std::string LinearProgram::GetPrettyNonZeroStats() const {
  return NonZeroStatFormatter(
      "Fill rate                                    : %.2f%%\n"
      "Entries in row (Max / average / std. dev.)   : %d / %.2f / %.2f\n"
      "Entries in column (Max / average / std. dev.): %d / %.2f / %.2f\n");
}

void LinearProgram::AddSlackVariablesForFreeAndBoxedRows() {
  const RowIndex num_rows = num_constraints();
  for (RowIndex row(0); row < num_rows; ++row) {
    const Fractional lower_bound = constraint_lower_bounds()[row];
    const Fractional upper_bound = constraint_upper_bounds()[row];
    if (AreBoundsFreeOrBoxed(lower_bound, upper_bound)) {
      const ColIndex new_col = CreateNewVariable();
      SetConstraintBounds(row, 0.0, 0.0);
      SetVariableBounds(new_col, -upper_bound, -lower_bound);
      SetCoefficient(row, new_col, Fractional(1.0));
    }
  }
  transpose_matrix_is_consistent_ = false;
}

void LinearProgram::PopulateFromDual(const LinearProgram& dual,
                                     RowToColMapping* duplicated_rows) {
  const ColIndex dual_num_variables = dual.num_variables();
  const RowIndex dual_num_constraints = dual.num_constraints();
  Clear();

  // We always take the dual in its minimization form thanks to the
  // GetObjectiveCoefficientForMinimizationVersion() below, so this will always
  // be a maximization problem.
  SetMaximizationProblem(true);

  // The objective offset is the same.
  SetObjectiveOffset(dual.objective_offset());

  // Create the dual variables y, with bounds depending on the type
  // of constraints in the primal.
  for (RowIndex dual_row(0); dual_row < dual_num_constraints; ++dual_row) {
    CreateNewVariable();
    const ColIndex col = RowToColIndex(dual_row);
    const Fractional lower_bound = dual.constraint_lower_bounds()[dual_row];
    const Fractional upper_bound = dual.constraint_upper_bounds()[dual_row];
    if (lower_bound == upper_bound) {
      SetVariableBounds(col, -kInfinity, kInfinity);
      SetObjectiveCoefficient(col, lower_bound);
    } else if (upper_bound != kInfinity) {
      // Note that for a ranged constraint, the loop will be continued here.
      // This is wanted because we want to deal with the lower_bound afterwards.
      SetVariableBounds(col, -kInfinity, 0.0);
      SetObjectiveCoefficient(col, upper_bound);
    } else if (lower_bound != -kInfinity) {
      SetVariableBounds(col, 0.0, kInfinity);
      SetObjectiveCoefficient(col, lower_bound);
    } else {
      // This code does not support free rows in linear_program.
      LOG(DFATAL) << "PopulateFromDual() was called with a program "
                  << "containing free constraints.";
    }
  }
  // Create the dual slack variables v.
  for (ColIndex dual_col(0); dual_col < dual_num_variables; ++dual_col) {
    const Fractional lower_bound = dual.variable_lower_bounds()[dual_col];
    if (lower_bound != -kInfinity) {
      const ColIndex col = CreateNewVariable();
      SetObjectiveCoefficient(col, lower_bound);
      SetVariableBounds(col, 0.0, kInfinity);
      const RowIndex row = ColToRowIndex(dual_col);
      SetCoefficient(row, col, Fractional(1.0));
    }
  }
  // Create the dual surplus variables w.
  for (ColIndex dual_col(0); dual_col < dual_num_variables; ++dual_col) {
    const Fractional upper_bound = dual.variable_upper_bounds()[dual_col];
    if (upper_bound != kInfinity) {
      const ColIndex col = CreateNewVariable();
      SetObjectiveCoefficient(col, upper_bound);
      SetVariableBounds(col, -kInfinity, 0.0);
      const RowIndex row = ColToRowIndex(dual_col);
      SetCoefficient(row, col, Fractional(1.0));
    }
  }
  // Store the transpose of the matrix.
  for (ColIndex dual_col(0); dual_col < dual_num_variables; ++dual_col) {
    const RowIndex row = ColToRowIndex(dual_col);
    const Fractional row_bound =
        dual.GetObjectiveCoefficientForMinimizationVersion(dual_col);
    SetConstraintBounds(row, row_bound, row_bound);
    for (const SparseColumn::Entry e : dual.GetSparseColumn(dual_col)) {
      const RowIndex dual_row = e.row();
      const ColIndex col = RowToColIndex(dual_row);
      SetCoefficient(row, col, e.coefficient());
    }
  }

  // Take care of ranged constraints.
  duplicated_rows->assign(dual_num_constraints, kInvalidCol);
  for (RowIndex dual_row(0); dual_row < dual_num_constraints; ++dual_row) {
    const Fractional lower_bound = dual.constraint_lower_bounds()[dual_row];
    const Fractional upper_bound = dual.constraint_upper_bounds()[dual_row];
    if (AreBoundsFreeOrBoxed(lower_bound, upper_bound)) {
      DCHECK(upper_bound != kInfinity || lower_bound != -kInfinity);

      // upper_bound was done in a loop above, now do the lower_bound.
      const ColIndex col = CreateNewVariable();
      SetVariableBounds(col, 0.0, kInfinity);
      SetObjectiveCoefficient(col, lower_bound);
      matrix_.mutable_column(col)
          ->PopulateFromSparseVector(matrix_.column(RowToColIndex(dual_row)));
      (*duplicated_rows)[dual_row] = col;
    }
  }

  // We know that the columns are ordered by rows.
  columns_are_known_to_be_clean_ = true;
  transpose_matrix_is_consistent_ = false;
}

void LinearProgram::PopulateFromLinearProgram(
    const LinearProgram& linear_program, bool keep_table_names) {
  matrix_.PopulateFromSparseMatrix(linear_program.matrix_);
  if (linear_program.transpose_matrix_is_consistent_) {
    transpose_matrix_is_consistent_ = true;
    transpose_matrix_.PopulateFromSparseMatrix(
        linear_program.transpose_matrix_);
  } else {
    transpose_matrix_is_consistent_ = false;
    transpose_matrix_.Clear();
  }
  integer_variables_list_is_consistent_ =
      linear_program.integer_variables_list_is_consistent_;

  constraint_lower_bounds_ = linear_program.constraint_lower_bounds_;
  constraint_upper_bounds_ = linear_program.constraint_upper_bounds_;
  constraint_names_ = linear_program.constraint_names_;

  objective_coefficients_ = linear_program.objective_coefficients_;
  variable_lower_bounds_ = linear_program.variable_lower_bounds_;
  variable_upper_bounds_ = linear_program.variable_upper_bounds_;
  variable_names_ = linear_program.variable_names_;
  is_variable_integer_ = linear_program.is_variable_integer_;
  integer_variables_list_ = linear_program.integer_variables_list_;
  binary_variables_list_ = linear_program.binary_variables_list_;
  non_binary_variables_list_ = linear_program.non_binary_variables_list_;

  if (keep_table_names) {
    variable_table_ = linear_program.variable_table_;
    constraint_table_ = linear_program.constraint_table_;
  } else {
    variable_table_.clear();
    constraint_table_.clear();
  }

  maximize_ = linear_program.maximize_;
  objective_offset_ = linear_program.objective_offset_;
  columns_are_known_to_be_clean_ =
      linear_program.columns_are_known_to_be_clean_;
  name_ = linear_program.name_;
}

void LinearProgram::Swap(LinearProgram* linear_program) {
  matrix_.Swap(&linear_program->matrix_);
  transpose_matrix_.Swap(&linear_program->transpose_matrix_);

  constraint_lower_bounds_.swap(linear_program->constraint_lower_bounds_);
  constraint_upper_bounds_.swap(linear_program->constraint_upper_bounds_);
  constraint_names_.swap(linear_program->constraint_names_);

  objective_coefficients_.swap(linear_program->objective_coefficients_);
  variable_lower_bounds_.swap(linear_program->variable_lower_bounds_);
  variable_upper_bounds_.swap(linear_program->variable_upper_bounds_);
  variable_names_.swap(linear_program->variable_names_);
  is_variable_integer_.swap(linear_program->is_variable_integer_);
  integer_variables_list_.swap(linear_program->integer_variables_list_);
  binary_variables_list_.swap(linear_program->binary_variables_list_);
  non_binary_variables_list_.swap(linear_program->non_binary_variables_list_);

  variable_table_.swap(linear_program->variable_table_);
  constraint_table_.swap(linear_program->constraint_table_);

  std::swap(maximize_, linear_program->maximize_);
  std::swap(objective_offset_, linear_program->objective_offset_);
  std::swap(columns_are_known_to_be_clean_,
            linear_program->columns_are_known_to_be_clean_);
  std::swap(transpose_matrix_is_consistent_,
            linear_program->transpose_matrix_is_consistent_);
  std::swap(integer_variables_list_is_consistent_,
            linear_program->integer_variables_list_is_consistent_);
  name_.swap(linear_program->name_);
}

void LinearProgram::DeleteColumns(const DenseBooleanRow& columns_to_delete) {
  if (columns_to_delete.empty()) return;
  integer_variables_list_is_consistent_ = false;
  const ColIndex num_cols = num_variables();
  ColumnPermutation permutation(num_cols);
  ColIndex new_index(0);
  for (ColIndex col(0); col < num_cols; ++col) {
    permutation[col] = new_index;
    if (col >= columns_to_delete.size() || !columns_to_delete[col]) {
      objective_coefficients_[new_index] = objective_coefficients_[col];
      variable_lower_bounds_[new_index] = variable_lower_bounds_[col];
      variable_upper_bounds_[new_index] = variable_upper_bounds_[col];
      is_variable_integer_[new_index] = is_variable_integer_[col];
      variable_names_[new_index] = variable_names_[col];
      ++new_index;
    } else {
      permutation[col] = kInvalidCol;
    }
  }

  matrix_.DeleteColumns(columns_to_delete);
  objective_coefficients_.resize(new_index, 0.0);
  variable_lower_bounds_.resize(new_index, 0.0);
  variable_upper_bounds_.resize(new_index, 0.0);
  is_variable_integer_.resize(new_index, false);
  variable_names_.resize(new_index, "");

  // Remove the id of the deleted columns and adjust the index of the other.
  hash_map<std::string, ColIndex>::iterator it = variable_table_.begin();
  while (it != variable_table_.end()) {
    const ColIndex col = it->second;
    if (col >= columns_to_delete.size() || !columns_to_delete[col]) {
      it->second = permutation[col];
      ++it;
    } else {
      // This safely deletes the entry and moves the iterator one step ahead.
      variable_table_.erase(it++);
    }
  }

  // Eventually update transpose_matrix_.
  if (transpose_matrix_is_consistent_) {
    transpose_matrix_.DeleteRows(
        ColToRowIndex(new_index),
        reinterpret_cast<const RowPermutation&>(permutation));
  }
}

void LinearProgram::Scale(SparseMatrixScaler* scaler) {
  scaler->Init(&matrix_);
  scaler->Scale();  // Compute R and C, and replace the matrix A by R.A.C
  scaler->ScaleRowVector(false, &objective_coefficients_);      // oc = oc.C
  scaler->ScaleRowVector(true, &variable_lower_bounds_);        // cl = cl.C^-1
  scaler->ScaleRowVector(true, &variable_upper_bounds_);        // cu = cu.C^-1
  scaler->ScaleColumnVector(false, &constraint_lower_bounds_);  // rl = R.rl
  scaler->ScaleColumnVector(false, &constraint_upper_bounds_);  // ru = R.ru
  transpose_matrix_is_consistent_ = false;
}

void LinearProgram::DeleteRows(const DenseBooleanColumn& row_to_delete) {
  if (row_to_delete.empty()) return;

  // Deal with row-indexed data and construct the row mapping that will need to
  // be applied to every column entry.
  const RowIndex num_rows = num_constraints();
  RowPermutation permutation(num_rows);
  RowIndex new_index(0);
  for (RowIndex row(0); row < num_rows; ++row) {
    if (row >= row_to_delete.size() || !row_to_delete[row]) {
      constraint_lower_bounds_[new_index] = constraint_lower_bounds_[row];
      constraint_upper_bounds_[new_index] = constraint_upper_bounds_[row];
      constraint_names_[new_index].swap(constraint_names_[row]);
      permutation[row] = new_index;
      ++new_index;
    } else {
      permutation[row] = kInvalidRow;
    }
  }
  constraint_lower_bounds_.resize(new_index, 0.0);
  constraint_upper_bounds_.resize(new_index, 0.0);
  constraint_names_.resize(new_index, "");

  // Remove the rows from the matrix.
  matrix_.DeleteRows(new_index, permutation);

  // Remove the id of the deleted rows and adjust the index of the other.
  hash_map<std::string, RowIndex>::iterator it = constraint_table_.begin();
  while (it != constraint_table_.end()) {
    const RowIndex row = it->second;
    if (permutation[row] != kInvalidRow) {
      it->second = permutation[row];
      ++it;
    } else {
      // This safely deletes the entry and moves the iterator one step ahead.
      constraint_table_.erase(it++);
    }
  }

  // Eventually update transpose_matrix_.
  if (transpose_matrix_is_consistent_) {
    transpose_matrix_.DeleteColumns(
        reinterpret_cast<const DenseBooleanRow&>(row_to_delete));
  }
}

bool LinearProgram::IsValid() const {
  if (!IsFinite(objective_offset_)) return false;
  const ColIndex num_cols = num_variables();
  for (ColIndex col(0); col < num_cols; ++col) {
    if (!AreBoundsValid(variable_lower_bounds()[col],
                        variable_upper_bounds()[col])) {
      return false;
    }
    if (!IsFinite(objective_coefficients()[col])) {
      return false;
    }
    for (const SparseColumn::Entry e : GetSparseColumn(col)) {
      if (!IsFinite(e.coefficient())) return false;
    }
  }
  if (constraint_upper_bounds_.size() != constraint_lower_bounds_.size())
    return false;
  for (RowIndex row(0); row < constraint_lower_bounds_.size(); ++row) {
    if (!AreBoundsValid(constraint_lower_bounds()[row],
                        constraint_upper_bounds()[row])) {
      return false;
    }
  }
  return true;
}

std::string LinearProgram::ProblemStatFormatter(const char* format) const {
  int num_objective_non_zeros = 0;
  int num_non_negative_variables = 0;
  int num_boxed_variables = 0;
  int num_free_variables = 0;
  int num_fixed_variables = 0;
  int num_other_variables = 0;
  const ColIndex num_cols = num_variables();
  for (ColIndex col(0); col < num_cols; ++col) {
    if (objective_coefficients()[col] != 0.0) {
      ++num_objective_non_zeros;
    }

    const Fractional lower_bound = variable_lower_bounds()[col];
    const Fractional upper_bound = variable_upper_bounds()[col];
    const bool lower_bounded = (lower_bound != -kInfinity);
    const bool upper_bounded = (upper_bound != kInfinity);

    if (!lower_bounded && !upper_bounded) {
      ++num_free_variables;
    } else if (lower_bound == 0.0 && !upper_bounded) {
      ++num_non_negative_variables;
    } else if (!upper_bounded || !lower_bounded) {
      ++num_other_variables;
    } else if (lower_bound == upper_bound) {
      ++num_fixed_variables;
    } else {
      ++num_boxed_variables;
    }
  }

  int num_range_constraints = 0;
  int num_less_than_constraints = 0;
  int num_greater_than_constraints = 0;
  int num_equal_constraints = 0;
  int num_rhs_non_zeros = 0;
  const RowIndex num_rows = num_constraints();
  for (RowIndex row(0); row < num_rows; ++row) {
    const Fractional lower_bound = constraint_lower_bounds()[row];
    const Fractional upper_bound = constraint_upper_bounds()[row];
    if (AreBoundsFreeOrBoxed(lower_bound, upper_bound)) {
      // TODO(user): we currently count a free row as a range constraint.
      // Add a new category?
      ++num_range_constraints;
      continue;
    }
    if (lower_bound == upper_bound) {
      ++num_equal_constraints;
      if (lower_bound != 0) {
        ++num_rhs_non_zeros;
      }
      continue;
    }
    if (lower_bound == -kInfinity) {
      ++num_less_than_constraints;
      if (upper_bound != 0) {
        ++num_rhs_non_zeros;
      }
      continue;
    }
    if (upper_bound == kInfinity) {
      ++num_greater_than_constraints;
      if (lower_bound != 0) {
        ++num_rhs_non_zeros;
      }
      continue;
    }
    LOG(DFATAL) << "There is a bug since all possible cases for the row bounds "
                   "should have been accounted for. row=" << row;
  }

  const int num_integer_variables = IntegerVariablesList().size();
  const int num_binary_variables = BinaryVariablesList().size();
  const int num_non_binary_variables = NonBinaryVariablesList().size();
  const int num_continuous_variables = ColToIntIndex(num_variables()) -
                                       num_integer_variables;
  return StringPrintf(
      format, RowToIntIndex(num_constraints()), ColToIntIndex(num_variables()),
      matrix_.num_entries().value(), num_objective_non_zeros, num_rhs_non_zeros,
      num_less_than_constraints, num_greater_than_constraints,
      num_equal_constraints, num_range_constraints, num_non_negative_variables,
      num_boxed_variables, num_free_variables, num_fixed_variables,
      num_other_variables, num_integer_variables, num_binary_variables,
      num_non_binary_variables, num_continuous_variables);
}

std::string LinearProgram::NonZeroStatFormatter(const char* format) const {
  StrictITIVector<RowIndex, EntryIndex> num_entries_in_row(num_constraints(),
                                                           EntryIndex(0));
  StrictITIVector<ColIndex, EntryIndex> num_entries_in_column(num_variables(),
                                                              EntryIndex(0));
  EntryIndex num_entries(0);
  const ColIndex num_cols = num_variables();
  for (ColIndex col(0); col < num_cols; ++col) {
    const SparseColumn& sparse_column = GetSparseColumn(col);
    num_entries += sparse_column.num_entries();
    num_entries_in_column[col] = sparse_column.num_entries();
    for (const SparseColumn::Entry e : sparse_column) {
      ++num_entries_in_row[e.row()];
    }
  }

  // To avoid division by 0 if there are no columns or no rows, we set
  // height and width to be at least one.
  const int64 height = std::max(RowToIntIndex(num_constraints()), 1);
  const int64 width = std::max(ColToIntIndex(num_variables()), 1);
  const double fill_rate = 100.0 * static_cast<double>(num_entries.value()) /
                           static_cast<double>(height * width);

  return StringPrintf(
      format, fill_rate, GetMaxElement(num_entries_in_row).value(),
      Average(num_entries_in_row), StandardDeviation(num_entries_in_row),
      GetMaxElement(num_entries_in_column).value(),
      Average(num_entries_in_column), StandardDeviation(num_entries_in_column));
}

void LinearProgram::ResizeRowsIfNeeded(RowIndex row) {
  DCHECK_GE(row, 0);
  if (row >= num_constraints()) {
    transpose_matrix_is_consistent_ = false;
    matrix_.SetNumRows(row + 1);
    constraint_lower_bounds_.resize(row + 1, Fractional(0.0));
    constraint_upper_bounds_.resize(row + 1, Fractional(0.0));
    constraint_names_.resize(row + 1, "");
  }
}

}  // namespace glop
}  // namespace operations_research
