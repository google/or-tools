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


#include "glop/preprocessor.h"

#include "base/stringprintf.h"
#include "glop/revised_simplex.h"
#include "glop/status.h"
#include "lp_data/lp_utils.h"
#include "lp_data/matrix_utils.h"

namespace operations_research {
namespace glop {

namespace {
// Returns an interval as an human readable std::string for debugging.
std::string IntervalString(Fractional lb, Fractional ub) {
  return StringPrintf("[%g, %g]", lb, ub);
}

#if defined(_MSC_VER)
double trunc(double d) { return d > 0 ? floor(d) : ceil(d); }
#endif
}  // namespace

// --------------------------------------------------------
// Preprocessor
// --------------------------------------------------------
Preprocessor::Preprocessor()
    : status_(ProblemStatus::INIT), parameters_(), in_mip_context_(false) {}
Preprocessor::~Preprocessor() {}

// --------------------------------------------------------
// MainLpPreprocessor
// --------------------------------------------------------

#define RUN_PREPROCESSOR(name)                                           \
  RunAndPushIfRelevant(std::unique_ptr<Preprocessor>(new name()), #name, \
                       time_limit, lp)

bool MainLpPreprocessor::Run(LinearProgram* lp, TimeLimit* time_limit) {
  RETURN_VALUE_IF_NULL(lp, false);
  initial_num_rows_ = lp->num_constraints();
  initial_num_cols_ = lp->num_variables();
  initial_num_entries_ = lp->num_entries();
  if (parameters_.use_preprocessing()) {
    RUN_PREPROCESSOR(ShiftVariableBoundsPreprocessor);

    // We run it a few times because running one preprocessor may allow another
    // one to remove more stuff.
    const int kMaxNumPasses = 20;
    for (int i = 0; i < kMaxNumPasses; ++i) {
      const int old_stack_size = preprocessors_.size();
      RUN_PREPROCESSOR(FixedVariablePreprocessor);
      RUN_PREPROCESSOR(SingletonPreprocessor);
      RUN_PREPROCESSOR(ForcingAndImpliedFreeConstraintPreprocessor);
      RUN_PREPROCESSOR(FreeConstraintPreprocessor);
      RUN_PREPROCESSOR(UnconstrainedVariablePreprocessor);
      RUN_PREPROCESSOR(DoubletonEqualityRowPreprocessor);
      RUN_PREPROCESSOR(ImpliedFreePreprocessor);
      RUN_PREPROCESSOR(DoubletonFreeColumnPreprocessor);

      // Abort early if none of the preprocessors did something. Technically
      // this is true if none of the preprocessors above needs postsolving,
      // which has exactly the same meaning for these particular preprocessors.
      if (preprocessors_.size() == old_stack_size) {
        // We use i here because the last pass did nothing.
        VLOG(1) << "Reached fixed point after presolve pass #" << i;
        break;
      }
    }
    RUN_PREPROCESSOR(EmptyColumnPreprocessor);
    RUN_PREPROCESSOR(EmptyConstraintPreprocessor);

    // TODO(user): Run them in the loop above if the effect on the running time
    // is good. This needs more investigation.
    RUN_PREPROCESSOR(ProportionalColumnPreprocessor);
    RUN_PREPROCESSOR(ProportionalRowPreprocessor);

    // If DualizerPreprocessor was run, we need to do some extra preprocessing.
    // This is because it currently adds a lot of zero-cost singleton columns.
    const int old_stack_size = preprocessors_.size();

    // TODO(user): We probably want to scale the costs before and after this
    // preprocessor so that the rhs/objective of the dual are with a good
    // magnitude.
    RUN_PREPROCESSOR(DualizerPreprocessor);
    RUN_PREPROCESSOR(SolowHalimPreprocessor);
    if (old_stack_size != preprocessors_.size()) {
      RUN_PREPROCESSOR(SingletonPreprocessor);
      RUN_PREPROCESSOR(FreeConstraintPreprocessor);
      RUN_PREPROCESSOR(UnconstrainedVariablePreprocessor);
      RUN_PREPROCESSOR(EmptyColumnPreprocessor);
      RUN_PREPROCESSOR(EmptyConstraintPreprocessor);
    }

    RUN_PREPROCESSOR(SingletonColumnSignPreprocessor);
  }

  // The scaling is controled by use_scaling, not use_preprocessing.
  RUN_PREPROCESSOR(ScalingPreprocessor);

  // This one must always run. It is needed by the revised simplex code.
  RUN_PREPROCESSOR(AddSlackVariablesPreprocessor);
  return !preprocessors_.empty();
}

#undef RUN_PREPROCESSOR

void MainLpPreprocessor::RunAndPushIfRelevant(
    std::unique_ptr<Preprocessor> preprocessor, const std::string& name,
    TimeLimit* time_limit, LinearProgram* lp) {
  RETURN_IF_NULL(preprocessor);
  RETURN_IF_NULL(time_limit);
  if (status_ != ProblemStatus::INIT || time_limit->LimitReached()) return;

  const double start_time = time_limit->GetElapsedTime();
  preprocessor->SetParameters(parameters_);

  // No need to run the preprocessor if the lp is empty.
  // TODO(user): without this test, the code is failing as of 2013-03-18.
  if (lp->num_variables() == 0 && lp->num_constraints() == 0) {
    status_ = ProblemStatus::OPTIMAL;
    return;
  }

  if (preprocessor->Run(lp, time_limit)) {
    const EntryIndex new_num_entries = lp->num_entries();
    const double preprocess_time = time_limit->GetElapsedTime() - start_time;
    VLOG(1) << StringPrintf(
        "%s(%fs): %d(%d) rows, %d(%d) columns, %lld(%lld) entries.",
        name.c_str(), preprocess_time, lp->num_constraints().value(),
        (lp->num_constraints() - initial_num_rows_).value(),
        lp->num_variables().value(),
        (lp->num_variables() - initial_num_cols_).value(),
        // static_cast<int64> is needed because the Android port uses int32.
        static_cast<int64>(new_num_entries.value()),
        static_cast<int64>(new_num_entries.value() -
                           initial_num_entries_.value()));
    status_ = preprocessor->status();
    preprocessors_.push_back(std::move(preprocessor));
    return;
  } else {
    // Even if a preprocessor returns false (i.e. no need for postsolve), it
    // can detect an issue with the problem.
    status_ = preprocessor->status();
    if (status_ != ProblemStatus::INIT) {
      VLOG(1) << name << " detected that the problem is "
              << GetProblemStatusString(status_);
    }
  }
}

void MainLpPreprocessor::RecoverSolution(ProblemSolution* solution) const {
  while (!preprocessors_.empty()) {
    preprocessors_.back()->RecoverSolution(solution);
    preprocessors_.pop_back();
  }
}

// --------------------------------------------------------
// ColumnDeletionHelper
// --------------------------------------------------------

void ColumnDeletionHelper::Clear() {
  is_column_deleted_.clear();
  stored_value_.clear();
}

void ColumnDeletionHelper::MarkColumnForDeletion(ColIndex col) {
  MarkColumnForDeletionWithState(col, 0.0, VariableStatus::FREE);
}

void ColumnDeletionHelper::MarkColumnForDeletionWithState(
    ColIndex col, Fractional fixed_value, VariableStatus status) {
  DCHECK_GE(col, 0);
  if (col >= is_column_deleted_.size()) {
    is_column_deleted_.resize(col + 1, false);
    stored_value_.resize(col + 1, 0.0);
    stored_status_.resize(col + 1, VariableStatus::FREE);
  }
  is_column_deleted_[col] = true;
  stored_value_[col] = fixed_value;
  stored_status_[col] = status;
}

void ColumnDeletionHelper::RestoreDeletedColumns(
    ProblemSolution* solution) const {
  DenseRow new_primal_values;
  VariableStatusRow new_variable_statuses;
  ColIndex old_index(0);
  for (ColIndex col(0); col < is_column_deleted_.size(); ++col) {
    if (is_column_deleted_[col]) {
      new_primal_values.push_back(stored_value_[col]);
      new_variable_statuses.push_back(stored_status_[col]);
    } else {
      new_primal_values.push_back(solution->primal_values[old_index]);
      new_variable_statuses.push_back(solution->variable_statuses[old_index]);
      ++old_index;
    }
  }

  // Copy the end of the vectors and swap them with the ones in solution.
  const ColIndex num_cols = solution->primal_values.size();
  DCHECK_EQ(num_cols, solution->variable_statuses.size());
  for (; old_index < num_cols; ++old_index) {
    new_primal_values.push_back(solution->primal_values[old_index]);
    new_variable_statuses.push_back(solution->variable_statuses[old_index]);
  }
  new_primal_values.swap(solution->primal_values);
  new_variable_statuses.swap(solution->variable_statuses);
}

// --------------------------------------------------------
// RowDeletionHelper
// --------------------------------------------------------

void RowDeletionHelper::Clear() { is_row_deleted_.clear(); }

void RowDeletionHelper::MarkRowForDeletion(RowIndex row) {
  DCHECK_GE(row, 0);
  if (row >= is_row_deleted_.size()) {
    is_row_deleted_.resize(row + 1, false);
  }
  is_row_deleted_[row] = true;
}

void RowDeletionHelper::UnmarkRow(RowIndex row) {
  if (row >= is_row_deleted_.size()) return;
  is_row_deleted_[row] = false;
}

const DenseBooleanColumn& RowDeletionHelper::GetMarkedRows() const {
  return is_row_deleted_;
}

void RowDeletionHelper::RestoreDeletedRows(ProblemSolution* solution) const {
  DenseColumn new_dual_values;
  ConstraintStatusColumn new_constraint_statuses;
  RowIndex old_index(0);
  const RowIndex end = is_row_deleted_.size();
  for (RowIndex row(0); row < end; ++row) {
    if (is_row_deleted_[row]) {
      new_dual_values.push_back(0.0);
      new_constraint_statuses.push_back(ConstraintStatus::BASIC);
    } else {
      new_dual_values.push_back(solution->dual_values[old_index]);
      new_constraint_statuses.push_back(
          solution->constraint_statuses[old_index]);
      ++old_index;
    }
  }

  // Copy the end of the vectors and swap them with the ones in solution.
  const RowIndex num_rows = solution->dual_values.size();
  DCHECK_EQ(num_rows, solution->constraint_statuses.size());
  for (; old_index < num_rows; ++old_index) {
    new_dual_values.push_back(solution->dual_values[old_index]);
    new_constraint_statuses.push_back(solution->constraint_statuses[old_index]);
  }
  new_dual_values.swap(solution->dual_values);
  new_constraint_statuses.swap(solution->constraint_statuses);
}

// --------------------------------------------------------
// EmptyColumnPreprocessor
// --------------------------------------------------------

namespace {

// Computes the status of a variable given its value and bounds. This only works
// with a value exactly at one of the bounds, or a value of 0.0 for free
// variables.
VariableStatus ComputeVariableStatus(Fractional value, Fractional lower_bound,
                                     Fractional upper_bound) {
  if (lower_bound == upper_bound) {
    DCHECK_EQ(value, lower_bound);
    DCHECK(IsFinite(lower_bound));
    return VariableStatus::FIXED_VALUE;
  }
  if (value == lower_bound) {
    DCHECK_NE(lower_bound, -kInfinity);
    return VariableStatus::AT_LOWER_BOUND;
  }
  if (value == upper_bound) {
    DCHECK_NE(upper_bound, kInfinity);
    return VariableStatus::AT_UPPER_BOUND;
  }

  // TODO(user): restrict this to unbounded variables with a value of zero.
  // We can't do that when postsolving infeasible problem. Don't call postsolve
  // on an infeasible problem?
  return VariableStatus::FREE;
}

// Returns the input with the smallest magnitude or zero if both are infinite.
Fractional MinInMagnitudeOrZeroIfInfinite(Fractional a, Fractional b) {
  const Fractional value = fabs(a) < fabs(b) ? a : b;
  return IsFinite(value) ? value : 0.0;
}

Fractional MagnitudeOrZeroIfInfinite(Fractional value) {
  return IsFinite(value) ? fabs(value) : 0.0;
}

// Returns the maximum magnitude of the finite variable bounds of the given
// linear program.
Fractional ComputeMaxVariableBoundsMagnitude(const LinearProgram& lp) {
  Fractional max_bounds_magnitude = 0.0;
  const ColIndex num_cols = lp.num_variables();
  for (ColIndex col(0); col < num_cols; ++col) {
    max_bounds_magnitude = std::max(
        max_bounds_magnitude,
        std::max(MagnitudeOrZeroIfInfinite(lp.variable_lower_bounds()[col]),
                 MagnitudeOrZeroIfInfinite(lp.variable_upper_bounds()[col])));
  }
  return max_bounds_magnitude;
}

}  // namespace

bool EmptyColumnPreprocessor::Run(LinearProgram* linear_program,
                                  TimeLimit* time_limit) {
  RETURN_VALUE_IF_NULL(linear_program, false);
  column_deletion_helper_.Clear();
  const ColIndex num_cols = linear_program->num_variables();
  for (ColIndex col(0); col < num_cols; ++col) {
    if (linear_program->GetSparseColumn(col).IsEmpty()) {
      const Fractional lower_bound =
          linear_program->variable_lower_bounds()[col];
      const Fractional upper_bound =
          linear_program->variable_upper_bounds()[col];
      const Fractional objective_coefficient =
          linear_program->GetObjectiveCoefficientForMinimizationVersion(col);
      Fractional value;
      if (objective_coefficient == 0) {
        // Any feasible value will do.
        if (upper_bound != kInfinity) {
          value = upper_bound;
        } else {
          if (lower_bound != -kInfinity) {
            value = lower_bound;
          } else {
            value = Fractional(0.0);
          }
        }
      } else {
        value = objective_coefficient > 0 ? lower_bound : upper_bound;
        if (!IsFinite(value)) {
          VLOG(1) << "Problem INFEASIBLE_OR_UNBOUNDED, empty column " << col
                  << " has a minimization cost of " << objective_coefficient
                  << " and bounds"
                  << " [" << lower_bound << "," << upper_bound << "]";
          status_ = ProblemStatus::INFEASIBLE_OR_UNBOUNDED;
          return false;
        }
        linear_program->SetObjectiveOffset(
            linear_program->objective_offset() +
            value * linear_program->objective_coefficients()[col]);
      }
      column_deletion_helper_.MarkColumnForDeletionWithState(
          col, value, ComputeVariableStatus(value, lower_bound, upper_bound));
    }
  }
  linear_program->DeleteColumns(column_deletion_helper_.GetMarkedColumns());
  return !column_deletion_helper_.IsEmpty();
}

void EmptyColumnPreprocessor::RecoverSolution(ProblemSolution* solution) const {
  RETURN_IF_NULL(solution);
  column_deletion_helper_.RestoreDeletedColumns(solution);
}

// --------------------------------------------------------
// ProportionalColumnPreprocessor
// --------------------------------------------------------

namespace {

// Subtracts 'multiple' times the column col of the given linear program from
// the constraint bounds. That is, for a non-zero entry of coefficient c,
// c * multiple is substracted from both the constraint upper and lower bound.
void SubtractColumnMultipleFromConstraintBound(ColIndex col,
                                               Fractional multiple,
                                               LinearProgram* lp) {
  for (const SparseColumn::Entry e : lp->GetSparseColumn(col)) {
    const RowIndex row = e.row();
    const Fractional delta = multiple * e.coefficient();
    lp->SetConstraintBounds(row, lp->constraint_lower_bounds()[row] - delta,
                            lp->constraint_upper_bounds()[row] - delta);
  }
  // While not needed for correctness, this allows the presolved problem to
  // have the same objective value as the original one.
  lp->SetObjectiveOffset(lp->objective_offset() +
                         lp->objective_coefficients()[col] * multiple);
}

// Struct used to detect proportional columns with the same cost. For that, a
// vector of such struct will be sorted, and only the columns that end up
// together need to be compared.
struct ColumnWithRepresentativeAndScaledCost {
  ColumnWithRepresentativeAndScaledCost(ColIndex _col, ColIndex _representative,
                                        Fractional _scaled_cost)
      : col(_col), representative(_representative), scaled_cost(_scaled_cost) {}
  ColIndex col;
  ColIndex representative;
  Fractional scaled_cost;

  bool operator<(const ColumnWithRepresentativeAndScaledCost& other) const {
    if (representative == other.representative) {
      if (scaled_cost == other.scaled_cost) {
        return col < other.col;
      }
      return scaled_cost < other.scaled_cost;
    }
    return representative < other.representative;
  }
};

}  // namespace

bool ProportionalColumnPreprocessor::Run(LinearProgram* lp,
                                         TimeLimit* time_limit) {
  RETURN_VALUE_IF_NULL(lp, false);
  ColMapping mapping = FindProportionalColumns(
      lp->GetSparseMatrix(), parameters_.preprocessor_zero_tolerance());

  // Compute some statistics and make each class representative point to itself
  // in the mapping. Also store the columns that are proportional to at least
  // another column in proportional_columns to iterate on them more efficiently.
  //
  // TODO(user): Change FindProportionalColumns for this?
  int num_proportionality_classes = 0;
  std::vector<ColIndex> proportional_columns;
  for (ColIndex col(0); col < mapping.size(); ++col) {
    const ColIndex representative = mapping[col];
    if (representative != kInvalidCol) {
      if (mapping[representative] == kInvalidCol) {
        proportional_columns.push_back(representative);
        ++num_proportionality_classes;
        mapping[representative] = representative;
      }
      proportional_columns.push_back(col);
    }
  }
  if (proportional_columns.empty()) return false;
  VLOG(1) << "The problem contains " << proportional_columns.size()
          << " columns which belong to " << num_proportionality_classes
          << " proportionality classes.";

  // Note(user): using the first coefficient may not give the best precision.
  const ColIndex num_cols = lp->num_variables();
  column_factors_.assign(num_cols, 0.0);
  for (const ColIndex col : proportional_columns) {
    const SparseColumn& column = lp->GetSparseColumn(col);
    column_factors_[col] = column.GetFirstCoefficient();
  }

  // This is only meaningful for column representative.
  //
  // The reduced cost of a column is 'cost - dual_values.column' and we know
  // that for all proportional columns, 'dual_values.column /
  // column_factors_[col]' is the same. Here, we bound this quantity which is
  // related to the cost 'slope' of a proportional column:
  // cost / column_factors_[col].
  DenseRow slope_lower_bound(num_cols, -kInfinity);
  DenseRow slope_upper_bound(num_cols, +kInfinity);
  for (const ColIndex col : proportional_columns) {
    const ColIndex representative = mapping[col];

    // We reason in terms of a minimization problem here.
    const bool is_rc_positive_or_zero =
        (lp->variable_upper_bounds()[col] == kInfinity);
    const bool is_rc_negative_or_zero =
        (lp->variable_lower_bounds()[col] == -kInfinity);
    bool is_slope_upper_bounded = is_rc_positive_or_zero;
    bool is_slope_lower_bounded = is_rc_negative_or_zero;
    if (column_factors_[col] < 0.0) {
      std::swap(is_slope_lower_bounded, is_slope_upper_bounded);
    }
    const Fractional slope =
        lp->GetObjectiveCoefficientForMinimizationVersion(col) /
        column_factors_[col];
    if (is_slope_lower_bounded) {
      slope_lower_bound[representative] =
          std::max(slope_lower_bound[representative], slope);
    }
    if (is_slope_upper_bounded) {
      slope_upper_bound[representative] =
          std::min(slope_upper_bound[representative], slope);
    }
  }

  // Deal with empty slope intervals.
  for (const ColIndex col : proportional_columns) {
    const ColIndex representative = mapping[col];

    // This is only needed for class representative columns.
    if (representative == col) {
      if (!IsSmallerWithinFeasibilityTolerance(
              slope_lower_bound[representative],
              slope_upper_bound[representative])) {
        VLOG(1) << "Problem INFEASIBLE_OR_UNBOUNDED, no feasible dual values"
                << " can satisfy the constraints of the proportional columns"
                << " with representative " << representative << "."
                << " the associated quantity must be in ["
                << slope_lower_bound[representative] << ","
                << slope_upper_bound[representative] << "].";
        status_ = ProblemStatus::INFEASIBLE_OR_UNBOUNDED;
        return false;
      }
    }
  }

  // Now, fix the columns that can be fixed to one of their bounds.
  for (const ColIndex col : proportional_columns) {
    const ColIndex representative = mapping[col];
    const Fractional slope =
        lp->GetObjectiveCoefficientForMinimizationVersion(col) /
        column_factors_[col];

    // The scaled reduced cost is slope - quantity.
    bool variable_can_be_fixed = false;
    Fractional target_bound = 0.0;

    const Fractional lower_bound = lp->variable_lower_bounds()[col];
    const Fractional upper_bound = lp->variable_upper_bounds()[col];
    if (!IsSmallerWithinFeasibilityTolerance(slope_lower_bound[representative],
                                             slope)) {
      // The scaled reduced cost is < 0.
      variable_can_be_fixed = true;
      target_bound = (column_factors_[col] >= 0.0) ? upper_bound : lower_bound;
    } else if (!IsSmallerWithinFeasibilityTolerance(
                   slope, slope_upper_bound[representative])) {
      // The scaled reduced cost is > 0.
      variable_can_be_fixed = true;
      target_bound = (column_factors_[col] >= 0.0) ? lower_bound : upper_bound;
    }

    if (variable_can_be_fixed) {
      // Clear mapping[col] so this column will not be considered for the next
      // stage.
      mapping[col] = kInvalidCol;
      if (!IsFinite(target_bound)) {
        VLOG(1) << "Problem INFEASIBLE_OR_UNBOUNDED.";
        status_ = ProblemStatus::INFEASIBLE_OR_UNBOUNDED;
        return false;
      } else {
        SubtractColumnMultipleFromConstraintBound(col, target_bound, lp);
        column_deletion_helper_.MarkColumnForDeletionWithState(
            col, target_bound,
            ComputeVariableStatus(target_bound, lower_bound, upper_bound));
      }
    }
  }

  // Merge the variables with the same scaled cost.
  std::vector<ColumnWithRepresentativeAndScaledCost> sorted_columns;
  for (const ColIndex col : proportional_columns) {
    const ColIndex representative = mapping[col];

    // This test is needed because we already removed some columns.
    if (mapping[col] != kInvalidCol) {
      sorted_columns.push_back(ColumnWithRepresentativeAndScaledCost(
          col, representative,
          lp->objective_coefficients()[col] / column_factors_[col]));
    }
  }
  std::sort(sorted_columns.begin(), sorted_columns.end());

  // All this will be needed during postsolve.
  merged_columns_.assign(num_cols, kInvalidCol);
  lower_bounds_.assign(num_cols, -kInfinity);
  upper_bounds_.assign(num_cols, kInfinity);
  new_lower_bounds_.assign(num_cols, -kInfinity);
  new_upper_bounds_.assign(num_cols, kInfinity);

  for (int i = 0; i < sorted_columns.size();) {
    const ColIndex target_col = sorted_columns[i].col;
    const ColIndex target_representative = sorted_columns[i].representative;
    const Fractional target_scaled_cost = sorted_columns[i].scaled_cost;

    // Save the initial bounds before modifying them.
    lower_bounds_[target_col] = lp->variable_lower_bounds()[target_col];
    upper_bounds_[target_col] = lp->variable_upper_bounds()[target_col];

    int num_merged = 0;
    for (++i; i < sorted_columns.size(); ++i) {
      if (sorted_columns[i].representative != target_representative) break;
      if (fabs(sorted_columns[i].scaled_cost - target_scaled_cost) >=
          parameters_.preprocessor_zero_tolerance()) {
        break;
      }
      ++num_merged;
      const ColIndex col = sorted_columns[i].col;
      const Fractional lower_bound = lp->variable_lower_bounds()[col];
      const Fractional upper_bound = lp->variable_upper_bounds()[col];
      lower_bounds_[col] = lower_bound;
      upper_bounds_[col] = upper_bound;
      merged_columns_[col] = target_col;

      // This is a bit counter intuitive, but when a column is divided by x,
      // the corresponding bounds have to be multiplied by x.
      const Fractional bound_factor =
          column_factors_[col] / column_factors_[target_col];

      // We need to shift the variable so that a basic solution of the new
      // problem can easily be converted to a basic solution of the original
      // problem.

      // A feasible value for the variable must be chosen, and the variable must
      // be shifted by this value. This is done to make sure that it will be
      // possible to recreate a basic solution of the original problem from a
      // basic solution of the pre-solved problem during post-solve.
      const Fractional target_value =
          MinInMagnitudeOrZeroIfInfinite(lower_bound, upper_bound);
      Fractional lower_diff = (lower_bound - target_value) * bound_factor;
      Fractional upper_diff = (upper_bound - target_value) * bound_factor;
      if (bound_factor < 0.0) {
        std::swap(lower_diff, upper_diff);
      }
      lp->SetVariableBounds(
          target_col, lp->variable_lower_bounds()[target_col] + lower_diff,
          lp->variable_upper_bounds()[target_col] + upper_diff);
      SubtractColumnMultipleFromConstraintBound(col, target_value, lp);
      column_deletion_helper_.MarkColumnForDeletionWithState(
          col, target_value,
          ComputeVariableStatus(target_value, lower_bound, upper_bound));
    }

    // If at least one column was merged, the target column must be shifted like
    // the other columns in the same equivalence class for the same reason (see
    // above).
    if (num_merged > 0) {
      merged_columns_[target_col] = target_col;
      const Fractional target_value = MinInMagnitudeOrZeroIfInfinite(
          lower_bounds_[target_col], upper_bounds_[target_col]);
      lp->SetVariableBounds(
          target_col, lp->variable_lower_bounds()[target_col] - target_value,
          lp->variable_upper_bounds()[target_col] - target_value);
      SubtractColumnMultipleFromConstraintBound(target_col, target_value, lp);
      new_lower_bounds_[target_col] = lp->variable_lower_bounds()[target_col];
      new_upper_bounds_[target_col] = lp->variable_upper_bounds()[target_col];
    }
  }

  lp->DeleteColumns(column_deletion_helper_.GetMarkedColumns());
  return !column_deletion_helper_.IsEmpty();
}

void ProportionalColumnPreprocessor::RecoverSolution(
    ProblemSolution* solution) const {
  RETURN_IF_NULL(solution);
  column_deletion_helper_.RestoreDeletedColumns(solution);

  // The rest of this function is to unmerge the columns so that the solution be
  // primal-feasible.
  const ColIndex num_cols = merged_columns_.size();
  DenseBooleanRow is_representative_basic(num_cols, false);
  DenseBooleanRow is_distance_to_upper_bound(num_cols, false);
  DenseRow distance_to_bound(num_cols, 0.0);
  DenseRow wanted_value(num_cols, 0.0);

  // The first pass is a loop over the representatives to compute the current
  // distance to the new bounds.
  for (ColIndex col(0); col < num_cols; ++col) {
    if (merged_columns_[col] == col) {
      const Fractional value = solution->primal_values[col];
      const Fractional distance_to_upper_bound = new_upper_bounds_[col] - value;
      const Fractional distance_to_lower_bound = value - new_lower_bounds_[col];
      if (distance_to_upper_bound < distance_to_lower_bound) {
        distance_to_bound[col] = distance_to_upper_bound;
        is_distance_to_upper_bound[col] = true;
      } else {
        distance_to_bound[col] = distance_to_lower_bound;
        is_distance_to_upper_bound[col] = false;
      }
      is_representative_basic[col] =
          solution->variable_statuses[col] == VariableStatus::BASIC;

      // Restore the representative value to a feasible value of the initial
      // variable. Now all the merged variable are at a feasible value.
      wanted_value[col] = value;
      solution->primal_values[col] = MinInMagnitudeOrZeroIfInfinite(
          lower_bounds_[col], upper_bounds_[col]);
      solution->variable_statuses[col] = ComputeVariableStatus(
          solution->primal_values[col], lower_bounds_[col], upper_bounds_[col]);
    }
  }

  // Second pass to correct the values.
  for (ColIndex col(0); col < num_cols; ++col) {
    const ColIndex representative = merged_columns_[col];
    if (representative != kInvalidCol) {
      if (IsFinite(distance_to_bound[representative])) {
        // If the distance if finite, then each variable is set to its
        // corresponding bound (the one from which the distance is computed) and
        // is then changed by has much as possible until the distance is zero.
        const Fractional bound_factor =
            column_factors_[col] / column_factors_[representative];
        const Fractional scaled_distance =
            distance_to_bound[representative] / fabs(bound_factor);
        const Fractional width = upper_bounds_[col] - lower_bounds_[col];
        const bool to_upper_bound =
            (bound_factor > 0.0) == is_distance_to_upper_bound[representative];
        if (width <= scaled_distance) {
          solution->primal_values[col] =
              to_upper_bound ? lower_bounds_[col] : upper_bounds_[col];
          solution->variable_statuses[col] =
              ComputeVariableStatus(solution->primal_values[col],
                                    lower_bounds_[col], upper_bounds_[col]);
          distance_to_bound[representative] -= width * fabs(bound_factor);
        } else {
          solution->primal_values[col] =
              to_upper_bound ? upper_bounds_[col] - scaled_distance
                             : lower_bounds_[col] + scaled_distance;
          solution->variable_statuses[col] =
              is_representative_basic[representative]
                  ? VariableStatus::BASIC
                  : ComputeVariableStatus(solution->primal_values[col],
                                          lower_bounds_[col],
                                          upper_bounds_[col]);
          distance_to_bound[representative] = 0.0;
          is_representative_basic[representative] = false;
        }
      } else {
        // If the distance is not finite, then only one variable needs to be
        // changed from its current feasible value in order to have a
        // primal-feasible solution.
        const Fractional error = wanted_value[representative];
        if (error == 0.0) {
          if (is_representative_basic[representative]) {
            solution->variable_statuses[col] = VariableStatus::BASIC;
            is_representative_basic[representative] = false;
          }
        } else {
          const Fractional bound_factor =
              column_factors_[col] / column_factors_[representative];
          const bool use_this_variable =
              (error * bound_factor > 0.0) ? (upper_bounds_[col] == kInfinity)
                                           : (lower_bounds_[col] == -kInfinity);
          if (use_this_variable) {
            wanted_value[representative] = 0.0;
            solution->primal_values[col] += error / bound_factor;
            if (is_representative_basic[representative]) {
              solution->variable_statuses[col] = VariableStatus::BASIC;
              is_representative_basic[representative] = false;
            } else {
              // This should not happen on an OPTIMAL or FEASIBLE solution.
              DCHECK(solution->status != ProblemStatus::OPTIMAL &&
                     solution->status != ProblemStatus::PRIMAL_FEASIBLE);
              solution->variable_statuses[col] = VariableStatus::FREE;
            }
          }
        }
      }
    }
  }
}

// --------------------------------------------------------
// ProportionalRowPreprocessor
// --------------------------------------------------------

bool ProportionalRowPreprocessor::Run(LinearProgram* lp,
                                      TimeLimit* time_limit) {
  RETURN_VALUE_IF_NULL(lp, false);
  const RowIndex num_rows = lp->num_constraints();
  const SparseMatrix& transpose = lp->GetTransposeSparseMatrix();

  // Use the first coefficient of each row to compute the proportionality
  // factor. Note that the sign is important.
  //
  // Note(user): using the first coefficient may not give the best precision.
  row_factors_.assign(num_rows, 0.0);
  for (RowIndex row(0); row < num_rows; ++row) {
    const SparseColumn& row_transpose = transpose.column(RowToColIndex(row));
    if (!row_transpose.IsEmpty()) {
      row_factors_[row] = row_transpose.GetFirstCoefficient();
    }
  }

  // The new row bounds (only meaningful for the proportional rows).
  DenseColumn lower_bounds(num_rows, -kInfinity);
  DenseColumn upper_bounds(num_rows, +kInfinity);

  // Where the new bounds are coming from. Only for the constraints that stay
  // in the linear_program and are modified, kInvalidRow otherwise.
  upper_bound_sources_.assign(num_rows, kInvalidRow);
  lower_bound_sources_.assign(num_rows, kInvalidRow);

  // Initialization.
  // We need the first representative of each proportional row class to point to
  // itself for the loop below. TODO(user): Already return such a mapping from
  // FindProportionalColumns()?
  ColMapping mapping = FindProportionalColumns(
      transpose, parameters_.preprocessor_zero_tolerance());
  DenseBooleanColumn is_a_representative(num_rows, false);
  int num_proportional_rows = 0;
  for (RowIndex row(0); row < num_rows; ++row) {
    const ColIndex representative_row_as_col = mapping[RowToColIndex(row)];
    if (representative_row_as_col != kInvalidCol) {
      mapping[representative_row_as_col] = representative_row_as_col;
      is_a_representative[ColToRowIndex(representative_row_as_col)] = true;
      ++num_proportional_rows;
    }
  }

  // Compute the bound of each representative as implied by the rows
  // which are proportional to it. Also keep the source row of each bound.
  for (RowIndex row(0); row < num_rows; ++row) {
    const ColIndex row_as_col = RowToColIndex(row);
    if (mapping[row_as_col] != kInvalidCol) {
      // For now, delete all the rows that are proportional to another one.
      // Note that we will unmark the final representative of this class later.
      row_deletion_helper_.MarkRowForDeletion(row);
      const RowIndex representative_row = ColToRowIndex(mapping[row_as_col]);

      const Fractional factor =
          row_factors_[representative_row] / row_factors_[row];
      Fractional implied_lb = factor * lp->constraint_lower_bounds()[row];
      Fractional implied_ub = factor * lp->constraint_upper_bounds()[row];
      if (factor < 0.0) {
        std::swap(implied_lb, implied_ub);
      }

      // TODO(user): if the bounds are equal, use the largest row in magnitude?
      if (implied_lb >= lower_bounds[representative_row]) {
        lower_bounds[representative_row] = implied_lb;
        lower_bound_sources_[representative_row] = row;
      }
      if (implied_ub <= upper_bounds[representative_row]) {
        upper_bounds[representative_row] = implied_ub;
        upper_bound_sources_[representative_row] = row;
      }
    }
  }

  // For maximum precision, and also to simplify the postsolve, we choose
  // a representative for each class of proportional columns that has at least
  // one of the two tightest bounds.
  for (RowIndex row(0); row < num_rows; ++row) {
    if (!is_a_representative[row]) continue;
    const RowIndex lower_source = lower_bound_sources_[row];
    const RowIndex upper_source = upper_bound_sources_[row];
    lower_bound_sources_[row] = kInvalidRow;
    upper_bound_sources_[row] = kInvalidRow;
    DCHECK_NE(lower_source, kInvalidRow);
    DCHECK_NE(upper_source, kInvalidRow);
    if (lower_source == upper_source) {
      // In this case, a simple change of representative is enough.
      // The constraint bounds of the representative will not change.
      DCHECK_NE(lower_source, kInvalidRow);
      row_deletion_helper_.UnmarkRow(lower_source);
    } else {
      // Report ProblemStatus::PRIMAL_INFEASIBLE if the new lower bound is not
      // lower than the new upper bound modulo the default tolerance.
      if (!IsSmallerWithinFeasibilityTolerance(lower_bounds[row],
                                               upper_bounds[row])) {
        status_ = ProblemStatus::PRIMAL_INFEASIBLE;
        return false;
      }

      // Special case for fixed rows.
      if (lp->constraint_lower_bounds()[lower_source] ==
          lp->constraint_upper_bounds()[lower_source]) {
        row_deletion_helper_.UnmarkRow(lower_source);
        continue;
      }
      if (lp->constraint_lower_bounds()[upper_source] ==
          lp->constraint_upper_bounds()[upper_source]) {
        row_deletion_helper_.UnmarkRow(upper_source);
        continue;
      }

      // This is the only case where a more complex postsolve is needed.
      // To maximize precision, the class representative is changed to either
      // upper_source or lower_source depending of which row has the largest
      // proportionality factor.
      RowIndex new_representative = lower_source;
      RowIndex other = upper_source;
      if (fabs(row_factors_[new_representative]) < fabs(row_factors_[other])) {
        std::swap(new_representative, other);
      }

      // Initialize the new bounds with the implied ones.
      const Fractional factor =
          row_factors_[new_representative] / row_factors_[other];
      Fractional new_lb = factor * lp->constraint_lower_bounds()[other];
      Fractional new_ub = factor * lp->constraint_upper_bounds()[other];
      if (factor < 0.0) {
        std::swap(new_lb, new_ub);
      }

      lower_bound_sources_[new_representative] = new_representative;
      upper_bound_sources_[new_representative] = new_representative;

      if (new_lb > lp->constraint_lower_bounds()[new_representative]) {
        lower_bound_sources_[new_representative] = other;
      } else {
        new_lb = lp->constraint_lower_bounds()[new_representative];
      }
      if (new_ub < lp->constraint_upper_bounds()[new_representative]) {
        upper_bound_sources_[new_representative] = other;
      } else {
        new_ub = lp->constraint_upper_bounds()[new_representative];
      }
      const RowIndex new_lower_source =
          lower_bound_sources_[new_representative];
      if (new_lower_source == upper_bound_sources_[new_representative]) {
        row_deletion_helper_.UnmarkRow(new_lower_source);
        lower_bound_sources_[new_representative] = kInvalidRow;
        upper_bound_sources_[new_representative] = kInvalidRow;
        continue;
      }

      // Take care of small numerical imprecision by making sure that lb <= ub.
      // Note that if the imprecision was greater than the tolerance, the code
      // at the beginning of this block would have reported
      // ProblemStatus::PRIMAL_INFEASIBLE.
      DCHECK(IsSmallerWithinFeasibilityTolerance(new_lb, new_ub));
      if (new_lb > new_ub) {
        if (lower_bound_sources_[new_representative] == new_representative) {
          new_ub = lp->constraint_lower_bounds()[new_representative];
        } else {
          new_lb = lp->constraint_upper_bounds()[new_representative];
        }
      }
      row_deletion_helper_.UnmarkRow(new_representative);
      lp->SetConstraintBounds(new_representative, new_lb, new_ub);
    }
  }

  lp_is_maximization_problem_ = lp->IsMaximizationProblem();
  lp->DeleteRows(row_deletion_helper_.GetMarkedRows());
  return !row_deletion_helper_.IsEmpty();
}

void ProportionalRowPreprocessor::RecoverSolution(
    ProblemSolution* solution) const {
  RETURN_IF_NULL(solution);
  row_deletion_helper_.RestoreDeletedRows(solution);

  // Make sure that all non-zero dual values on the proportional rows are
  // assigned to the correct row with the correct sign and that the statuses
  // are correct.
  const RowIndex num_rows = solution->dual_values.size();
  for (RowIndex row(0); row < num_rows; ++row) {
    const RowIndex lower_source = lower_bound_sources_[row];
    const RowIndex upper_source = upper_bound_sources_[row];
    if (lower_source == kInvalidRow && upper_source == kInvalidRow) continue;
    DCHECK_NE(lower_source, upper_source);
    DCHECK(lower_source == row || upper_source == row);

    // If the representative is ConstraintStatus::BASIC, then all rows in this
    // class will be ConstraintStatus::BASIC and there is nothing to do.
    ConstraintStatus status = solution->constraint_statuses[row];
    if (status == ConstraintStatus::BASIC) continue;

    // If the row is FIXED it will behave as a row
    // ConstraintStatus::AT_UPPER_BOUND or
    // ConstraintStatus::AT_LOWER_BOUND depending on the corresponding dual
    // variable sign.
    if (status == ConstraintStatus::FIXED_VALUE) {
      const Fractional corrected_dual_value = lp_is_maximization_problem_
                                                  ? -solution->dual_values[row]
                                                  : solution->dual_values[row];
      if (corrected_dual_value != 0.0) {
        status = corrected_dual_value > 0.0 ? ConstraintStatus::AT_LOWER_BOUND
                                            : ConstraintStatus::AT_UPPER_BOUND;
      }
    }

    // If one of the two conditions below are true, set the row status to
    // ConstraintStatus::BASIC.
    // Note that the source which is not row can't be FIXED (see presolve).
    if (lower_source != row && status == ConstraintStatus::AT_LOWER_BOUND) {
      DCHECK_EQ(0.0, solution->dual_values[lower_source]);
      const Fractional factor = row_factors_[row] / row_factors_[lower_source];
      solution->dual_values[lower_source] = factor * solution->dual_values[row];
      solution->dual_values[row] = 0.0;
      solution->constraint_statuses[row] = ConstraintStatus::BASIC;
      solution->constraint_statuses[lower_source] =
          factor > 0.0 ? ConstraintStatus::AT_LOWER_BOUND
                       : ConstraintStatus::AT_UPPER_BOUND;
    }
    if (upper_source != row && status == ConstraintStatus::AT_UPPER_BOUND) {
      DCHECK_EQ(0.0, solution->dual_values[upper_source]);
      const Fractional factor = row_factors_[row] / row_factors_[upper_source];
      solution->dual_values[upper_source] = factor * solution->dual_values[row];
      solution->dual_values[row] = 0.0;
      solution->constraint_statuses[row] = ConstraintStatus::BASIC;
      solution->constraint_statuses[upper_source] =
          factor > 0.0 ? ConstraintStatus::AT_UPPER_BOUND
                       : ConstraintStatus::AT_LOWER_BOUND;
    }

    // If the row status is still ConstraintStatus::FIXED_VALUE, we need to
    // relax its status.
    if (solution->constraint_statuses[row] == ConstraintStatus::FIXED_VALUE) {
      solution->constraint_statuses[row] =
          lower_source != row ? ConstraintStatus::AT_UPPER_BOUND
                              : ConstraintStatus::AT_LOWER_BOUND;
    }
  }
}

// --------------------------------------------------------
// FixedVariablePreprocessor
// --------------------------------------------------------

bool FixedVariablePreprocessor::Run(LinearProgram* lp, TimeLimit* time_limit) {
  RETURN_VALUE_IF_NULL(lp, false);
  const ColIndex num_cols = lp->num_variables();
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional lower_bound = lp->variable_lower_bounds()[col];
    const Fractional upper_bound = lp->variable_upper_bounds()[col];
    if (lower_bound == upper_bound) {
      const Fractional fixed_value = lower_bound;
      DCHECK(IsFinite(fixed_value));

      // We need to change the constraint bounds.
      SubtractColumnMultipleFromConstraintBound(col, fixed_value, lp);
      column_deletion_helper_.MarkColumnForDeletionWithState(
          col, fixed_value, VariableStatus::FIXED_VALUE);
    }
  }

  lp->DeleteColumns(column_deletion_helper_.GetMarkedColumns());
  return !column_deletion_helper_.IsEmpty();
}

void FixedVariablePreprocessor::RecoverSolution(
    ProblemSolution* solution) const {
  RETURN_IF_NULL(solution);
  column_deletion_helper_.RestoreDeletedColumns(solution);
}

// --------------------------------------------------------
// ForcingAndImpliedFreeConstraintPreprocessor
// --------------------------------------------------------

bool ForcingAndImpliedFreeConstraintPreprocessor::Run(LinearProgram* lp,
                                                      TimeLimit* time_limit) {
  RETURN_VALUE_IF_NULL(lp, false);
  const RowIndex num_rows = lp->num_constraints();

  // Compute the implied constraint bounds from the variable bounds.
  DenseColumn implied_lower_bounds(num_rows, 0);
  DenseColumn implied_upper_bounds(num_rows, 0);
  const ColIndex num_cols = lp->num_variables();
  StrictITIVector<RowIndex, int> row_degree(num_rows, 0);
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional lower = lp->variable_lower_bounds()[col];
    const Fractional upper = lp->variable_upper_bounds()[col];
    for (const SparseColumn::Entry e : lp->GetSparseColumn(col)) {
      const RowIndex row = e.row();
      const Fractional coeff = e.coefficient();
      if (coeff > 0.0) {
        implied_lower_bounds[row] += lower * coeff;
        implied_upper_bounds[row] += upper * coeff;
      } else {
        implied_lower_bounds[row] += upper * coeff;
        implied_upper_bounds[row] += lower * coeff;
      }
      ++row_degree[row];
    }
  }

  // Note that the ScalingPreprocessor is currently executed last, so here the
  // problem has not been scaled yet.
  int num_implied_free_constraints = 0;
  int num_forcing_constraints = 0;
  is_forcing_up_.assign(num_rows, false);
  DenseBooleanColumn is_forcing_down(num_rows, false);
  for (RowIndex row(0); row < num_rows; ++row) {
    if (row_degree[row] == 0) continue;
    Fractional lower = lp->constraint_lower_bounds()[row];
    Fractional upper = lp->constraint_upper_bounds()[row];

    // Check for infeasibility.
    if (!IsSmallerWithinFeasibilityTolerance(lower,
                                             implied_upper_bounds[row]) ||
        !IsSmallerWithinFeasibilityTolerance(implied_lower_bounds[row],
                                             upper)) {
      VLOG(1) << "implied bound " << implied_lower_bounds[row] << " "
              << implied_upper_bounds[row];
      VLOG(1) << "constraint bound " << lower << " " << upper;
      status_ = ProblemStatus::PRIMAL_INFEASIBLE;
      return false;
    }

    // Check if the constraint is forcing. That is, all the variables that
    // appear in it must be at one of their bounds.
    if (IsSmallerWithinPreprocessorZeroTolerance(implied_upper_bounds[row],
                                                 lower)) {
      is_forcing_down[row] = true;
      ++num_forcing_constraints;
      continue;
    }
    if (IsSmallerWithinPreprocessorZeroTolerance(upper,
                                                 implied_lower_bounds[row])) {
      is_forcing_up_[row] = true;
      ++num_forcing_constraints;
      continue;
    }

    // We relax the constraint bounds only if the constraint is implied to be
    // free. Such constraints will later be deleted by the
    // FreeConstraintPreprocessor.
    //
    // Note that we could relax only one of the two bounds, but the impact this
    // would have on the revised simplex algorithm is unclear at this point.
    if (IsSmallerWithinPreprocessorZeroTolerance(lower,
                                                 implied_lower_bounds[row]) &&
        IsSmallerWithinPreprocessorZeroTolerance(implied_upper_bounds[row],
                                                 upper)) {
      lp->SetConstraintBounds(row, -kInfinity, kInfinity);
      ++num_implied_free_constraints;
    }
  }

  if (num_implied_free_constraints > 0) {
    VLOG(1) << num_implied_free_constraints << " implied free constraints.";
  }

  if (num_forcing_constraints > 0) {
    VLOG(1) << num_forcing_constraints << " forcing constraints.";
    lp_is_maximization_problem_ = lp->IsMaximizationProblem();
    deleted_columns_.PopulateFromZero(num_rows, num_cols);
    costs_.resize(num_cols, 0.0);
    for (ColIndex col(0); col < num_cols; ++col) {
      const SparseColumn& column = lp->GetSparseColumn(col);
      const Fractional lower = lp->variable_lower_bounds()[col];
      const Fractional upper = lp->variable_upper_bounds()[col];
      bool is_forced = false;
      Fractional target_bound = 0.0;
      for (const SparseColumn::Entry e : column) {
        if (is_forcing_down[e.row()]) {
          const Fractional candidate = e.coefficient() < 0.0 ? lower : upper;
          if (is_forced && candidate != target_bound) {
            VLOG(1) << "A variable is forced in both directions! bounds: ["
                    << lower << ", " << upper << "]. coeff:" << e.coefficient();
            status_ = ProblemStatus::PRIMAL_INFEASIBLE;
            return false;
          }
          target_bound = candidate;
          is_forced = true;
        }
        if (is_forcing_up_[e.row()]) {
          const Fractional candidate = e.coefficient() < 0.0 ? upper : lower;
          if (is_forced && candidate != target_bound) {
            VLOG(1) << "A variable is forced in both directions! bounds: ["
                    << lower << ", " << upper << "]. coeff:" << e.coefficient();
            status_ = ProblemStatus::PRIMAL_INFEASIBLE;
            return false;
          }
          target_bound = candidate;
          is_forced = true;
        }
      }
      if (is_forced) {
        // Fix the variable, update the constraint bounds and save this column
        // and its cost for the postsolve.
        SubtractColumnMultipleFromConstraintBound(col, target_bound, lp);
        column_deletion_helper_.MarkColumnForDeletionWithState(
            col, target_bound,
            ComputeVariableStatus(target_bound, lower, upper));
        deleted_columns_.mutable_column(col)->PopulateFromSparseVector(column);
        costs_[col] = lp->objective_coefficients()[col];
      }
    }
    for (RowIndex row(0); row < num_rows; ++row) {
      // In theory, an M exists such that for any magnitude >= M, we will be at
      // an optimal solution. However, because of numerical errors, if the value
      // is too large, it causes problem when verifying the solution. So we
      // select the smallest such M (at least a resonably small one) during
      // postsolve. It is the reason why we need to store the columns that were
      // fixed.
      if (is_forcing_down[row] || is_forcing_up_[row]) {
        row_deletion_helper_.MarkRowForDeletion(row);
      }
    }
  }

  lp->DeleteColumns(column_deletion_helper_.GetMarkedColumns());
  lp->DeleteRows(row_deletion_helper_.GetMarkedRows());
  return !column_deletion_helper_.IsEmpty();
}

void ForcingAndImpliedFreeConstraintPreprocessor::RecoverSolution(
    ProblemSolution* solution) const {
  RETURN_IF_NULL(solution);
  column_deletion_helper_.RestoreDeletedColumns(solution);
  row_deletion_helper_.RestoreDeletedRows(solution);

  // Compute for each deleted columns the last deleted row in which it appears.
  const ColIndex num_cols = deleted_columns_.num_cols();
  ColToRowMapping last_deleted_row(num_cols, kInvalidRow);
  for (ColIndex col(0); col < num_cols; ++col) {
    if (!column_deletion_helper_.IsColumnMarked(col)) continue;
    for (const SparseColumn::Entry e : deleted_columns_.column(col)) {
      const RowIndex row = e.row();
      if (row_deletion_helper_.IsRowMarked(row)) {
        last_deleted_row[col] = row;
      }
    }
  }

  // For each deleted row (in order), compute a bound on the dual values so
  // that all the deleted columns for which this row is the last deleted row are
  // dual-feasible. Note that for the other columns, it will always be possible
  // to make them dual-feasible with a later row.
  // There are two possible outcomes:
  //  - The dual value stays 0.0, and nothing changes.
  //  - The bounds enforce a non-zero dual value, and one column will have a
  //    reduced cost of 0.0. This column becomes VariableStatus::BASIC, and the
  //    constraint status is changed to ConstraintStatus::AT_LOWER_BOUND,
  //    ConstraintStatus::AT_UPPER_BOUND or ConstraintStatus::FIXED_VALUE.
  SparseMatrix transpose;
  transpose.PopulateFromTranspose(deleted_columns_);
  const RowIndex num_rows = solution->dual_values.size();
  for (RowIndex row(0); row < num_rows; ++row) {
    if (row_deletion_helper_.IsRowMarked(row)) {
      Fractional new_dual_value = 0.0;
      ColIndex new_basic_column = kInvalidCol;
      for (const SparseColumn::Entry e : transpose.column(RowToColIndex(row))) {
        const ColIndex col = RowToColIndex(e.row());
        if (last_deleted_row[col] != row) continue;
        const Fractional scalar_product =
            ScalarProduct(solution->dual_values, deleted_columns_.column(col));
        const Fractional reduced_cost = costs_[col] - scalar_product;
        const Fractional bound = reduced_cost / e.coefficient();
        if (is_forcing_up_[row] == !lp_is_maximization_problem_) {
          if (bound < new_dual_value) {
            new_dual_value = bound;
            new_basic_column = col;
          }
        } else {
          if (bound > new_dual_value) {
            new_dual_value = bound;
            new_basic_column = col;
          }
        }
      }
      if (new_basic_column != kInvalidCol) {
        solution->dual_values[row] = new_dual_value;
        solution->variable_statuses[new_basic_column] = VariableStatus::BASIC;
        solution->constraint_statuses[row] =
            is_forcing_up_[row] ? ConstraintStatus::AT_UPPER_BOUND
                                : ConstraintStatus::AT_LOWER_BOUND;
      }
    }
  }
}

// --------------------------------------------------------
// ImpliedFreePreprocessor
// --------------------------------------------------------

namespace {
struct ColWithDegree {
  ColIndex col;
  EntryIndex num_entries;
  ColWithDegree(ColIndex c, EntryIndex n) : col(c), num_entries(n) {}
  bool operator<(const ColWithDegree& other) const {
    if (num_entries == other.num_entries) {
      return col < other.col;
    }
    return num_entries < other.num_entries;
  }
};
}  // namespace

bool ImpliedFreePreprocessor::Run(LinearProgram* lp, TimeLimit* time_limit) {
  RETURN_VALUE_IF_NULL(lp, false);
  const RowIndex num_rows = lp->num_constraints();
  const ColIndex num_cols = lp->num_variables();

  // For each constraint with n entries and each of its variable, we want the
  // bounds implied by the (n - 1) other variables and the constraint. We
  // use two handy utility classes that allow us to do that efficiently while
  // dealing properly with infinite bounds.
  const int size = num_rows.value();
  // TODO(user) : Replace SumWithNegativeInfiniteAndOneMissing and
  // SumWithPositiveInfiniteAndOneMissing with IntervalSumWithOneMissing.
  ITIVector<RowIndex, SumWithNegativeInfiniteAndOneMissing> lb_sums(size);
  ITIVector<RowIndex, SumWithPositiveInfiniteAndOneMissing> ub_sums(size);

  // Initialize the sums by adding all the bounds of the variables.
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional lower_bound = lp->variable_lower_bounds()[col];
    const Fractional upper_bound = lp->variable_upper_bounds()[col];
    for (const SparseColumn::Entry e : lp->GetSparseColumn(col)) {
      Fractional entry_lb = e.coefficient() * lower_bound;
      Fractional entry_ub = e.coefficient() * upper_bound;
      if (e.coefficient() < 0.0) std::swap(entry_lb, entry_ub);
      lb_sums[e.row()].Add(entry_lb);
      ub_sums[e.row()].Add(entry_ub);
    }
  }

  // The inequality
  //    constraint_lb <= sum(entries) <= constraint_ub
  // can be rewritten as:
  //    sum(entries) + (-activity) = 0,
  // where (-activity) has bounds [-constraint_ub, -constraint_lb].
  // We use this latter convention to simplify our code.
  for (RowIndex row(0); row < num_rows; ++row) {
    lb_sums[row].Add(-lp->constraint_upper_bounds()[row]);
    ub_sums[row].Add(-lp->constraint_lower_bounds()[row]);
  }

  // Once a variable is freed, none of the rows in which it appears can be
  // used to make another variable free.
  DenseBooleanColumn used_rows(num_rows, false);
  postsolve_status_of_free_variables_.assign(num_cols, VariableStatus::FREE);
  variable_offsets_.assign(num_cols, 0.0);

  // It is better to process columns with a small degree first:
  // - Degree-two columns make it possible to remove a row from the problem.
  // - This way there is more chance to make more free columns.
  // - It is better to have low degree free columns since a free column will
  //   always end up in the simplex basis (except if there is more than the
  //   number of rows in the problem).
  //
  // TODO(user): Only process degree-two so in subsequent passes more degree-two
  // columns could be made free. And only when no other reduction can be
  // applied, process the higher degree column?
  //
  // TODO(user): Be smarter about the order that maximizes the number of free
  // column. For instance if we have 3 doubleton columns that use the rows (1,2)
  // (2,3) and (3,4) then it is better not to make (2,3) free so the two other
  // two can be made free.
  std::vector<ColWithDegree> col_by_degree;
  for (ColIndex col(0); col < num_cols; ++col) {
    col_by_degree.push_back(
        ColWithDegree(col, lp->GetSparseColumn(col).num_entries()));
  }
  std::sort(col_by_degree.begin(), col_by_degree.end());

  // Now loop over the columns in order and make all implied-free columns free.
  int num_already_free_variables = 0;
  int num_implied_free_variables = 0;
  int num_fixed_variables = 0;
  for (ColWithDegree col_with_degree : col_by_degree) {
    const ColIndex col = col_with_degree.col;

    // If the variable is alreay free or fixed, we do nothing.
    const Fractional lower_bound = lp->variable_lower_bounds()[col];
    const Fractional upper_bound = lp->variable_upper_bounds()[col];
    if (!IsFinite(lower_bound) && !IsFinite(upper_bound)) {
      ++num_already_free_variables;
      continue;
    }
    if (lower_bound == upper_bound) continue;

    // Detect if the variable is implied free.
    Fractional overall_implied_lb = -kInfinity;
    Fractional overall_implied_ub = kInfinity;
    for (const SparseColumn::Entry e : lp->GetSparseColumn(col)) {
      // If the row contains another implied free variable, then the bounds
      // implied by it will just be [-kInfinity, kInfinity] so we can skip it.
      if (used_rows[e.row()]) continue;

      // This is the contribution of this column to the sum above.
      const Fractional coeff = e.coefficient();
      Fractional entry_lb = coeff * lower_bound;
      Fractional entry_ub = coeff * upper_bound;
      if (coeff < 0.0) std::swap(entry_lb, entry_ub);

      // If X is the variable with index col and Y the sum of all the other
      // variables and of (-activity), then coeff * X + Y = 0. Since Y's bounds
      // are [lb_sum without X, ub_sum without X], it is easy to derive the
      // implied bounds on X.
      Fractional implied_lb = -ub_sums[e.row()].SumWithout(entry_ub) / coeff;
      Fractional implied_ub = -lb_sums[e.row()].SumWithout(entry_lb) / coeff;
      if (coeff < 0.0) std::swap(implied_lb, implied_ub);
      overall_implied_lb = std::max(overall_implied_lb, implied_lb);
      overall_implied_ub = std::min(overall_implied_ub, implied_ub);
    }

    // Detect infeasible cases.
    if (!IsSmallerWithinFeasibilityTolerance(overall_implied_lb, upper_bound) ||
        !IsSmallerWithinFeasibilityTolerance(lower_bound, overall_implied_ub) ||
        !IsSmallerWithinFeasibilityTolerance(overall_implied_lb,
                                             overall_implied_ub)) {
      status_ = ProblemStatus::PRIMAL_INFEASIBLE;
      return false;
    }

    // Detect fixed variable cases (there are two kinds).
    // Note that currently we don't do anything here except counting them.
    if (IsSmallerWithinPreprocessorZeroTolerance(upper_bound,
                                                 overall_implied_lb) ||
        IsSmallerWithinPreprocessorZeroTolerance(overall_implied_ub,
                                                 lower_bound)) {
      // This case is already dealt with by the
      // ForcingAndImpliedFreeConstraintPreprocessor since it means that (at
      // least) one of the row is forcing.
      ++num_fixed_variables;
      continue;
    } else if (IsSmallerWithinPreprocessorZeroTolerance(overall_implied_ub,
                                                        overall_implied_lb)) {
      // TODO(user): As of July 2013, with our preprocessors this case is never
      // triggered on the Netlib. Note however that if it appears it can have a
      // big impact since by fixing the variable, the two involved constraints
      // are forcing and can be removed too (with all the variables they touch).
      // The postsolve step is quite involved though.
      ++num_fixed_variables;
      continue;
    }

    // Is the variable implied free? Note that for an infinite lower_bound or
    // upper_bound the respective inequality is always true.
    if (IsSmallerWithinPreprocessorZeroTolerance(lower_bound,
                                                 overall_implied_lb) &&
        IsSmallerWithinPreprocessorZeroTolerance(overall_implied_ub,
                                                 upper_bound)) {
      ++num_implied_free_variables;
      lp->SetVariableBounds(col, -kInfinity, kInfinity);
      for (const SparseColumn::Entry e : lp->GetSparseColumn(col)) {
        used_rows[e.row()] = true;
      }

      // This is a tricky part. We're freeing this variable, which means that
      // after solve, the modified variable will have status either
      // VariableStatus::FREE or VariableStatus::BASIC. In the former case
      // (VariableStatus::FREE, value = 0.0), we need to "fix" the
      // status (technically, our variable isn't free!) to either
      // VariableStatus::AT_LOWER_BOUND or VariableStatus::AT_UPPER_BOUND
      // (note that we skipped fixed variables), and "fix" the value to that
      // bound's value as well. We make the decision and the precomputation
      // here: we simply offset the variable by one of its bounds, and store
      // which bound that was. Note that if the modified variable turns out to
      // be VariableStatus::BASIC, we'll simply un-offset its value too;
      // and let the status be VariableStatus::BASIC.
      //
      // TODO(user): This trick is already used in the DualizerPreprocessor,
      // maybe we should just have a preprocessor that shifts all the variables
      // bounds to have at least one of them at 0.0, will that improve precision
      // and speed of the simplex? One advantage is that we can compute the
      // new constraint bounds with better precision using AccurateSum.
      DCHECK_NE(lower_bound, upper_bound);
      const Fractional offset =
          MinInMagnitudeOrZeroIfInfinite(lower_bound, upper_bound);
      if (offset != 0.0) {
        variable_offsets_[col] = offset;
        SubtractColumnMultipleFromConstraintBound(col, offset, lp);
      }
      postsolve_status_of_free_variables_[col] =
          ComputeVariableStatus(offset, lower_bound, upper_bound);
    }
  }
  VLOG(1) << num_already_free_variables << " free variables in the problem.";
  VLOG(1) << num_implied_free_variables << " implied free columns.";
  VLOG(1) << num_fixed_variables << " variables can be fixed.";
  return num_implied_free_variables > 0;
}

void ImpliedFreePreprocessor::RecoverSolution(ProblemSolution* solution) const {
  RETURN_IF_NULL(solution);
  const ColIndex num_cols = solution->variable_statuses.size();
  for (ColIndex col(0); col < num_cols; ++col) {
    // Skip variables that the preprocessor didn't change.
    if (postsolve_status_of_free_variables_[col] == VariableStatus::FREE) {
      DCHECK_EQ(0.0, variable_offsets_[col]);
      continue;
    }
    if (solution->variable_statuses[col] == VariableStatus::FREE) {
      solution->variable_statuses[col] =
          postsolve_status_of_free_variables_[col];
    } else {
      DCHECK_EQ(VariableStatus::BASIC, solution->variable_statuses[col]);
    }
    solution->primal_values[col] += variable_offsets_[col];
  }
}

// --------------------------------------------------------
// DoubletonFreeColumnPreprocessor
// --------------------------------------------------------

bool DoubletonFreeColumnPreprocessor::Run(LinearProgram* lp,
                                          TimeLimit* time_limit) {
  RETURN_VALUE_IF_NULL(lp, false);
  // We will modify the matrix transpose and then push the change to the linear
  // program by calling lp->UseTransposeMatrixAsReference(). Note that
  // original_matrix will not change during this preprocessor run.
  const SparseMatrix& original_matrix = lp->GetSparseMatrix();
  SparseMatrix* transpose = lp->GetMutableTransposeSparseMatrix();

  const ColIndex num_cols(lp->num_variables());
  for (ColIndex doubleton_col(0); doubleton_col < num_cols; ++doubleton_col) {
    // Only consider doubleton free columns.
    if (original_matrix.column(doubleton_col).num_entries() != 2) continue;
    if (lp->variable_lower_bounds()[doubleton_col] != -kInfinity) continue;
    if (lp->variable_upper_bounds()[doubleton_col] != kInfinity) continue;

    // Collect the two column items. Note that we skip a column involving a
    // deleted row since it is no longer a doubleton then.
    RestoreInfo r;
    r.col = doubleton_col;
    r.objective_coefficient = lp->objective_coefficients()[r.col];
    int index = 0;
    for (const SparseColumn::Entry e : original_matrix.column(r.col)) {
      if (row_deletion_helper_.IsRowMarked(e.row())) break;
      r.row[index] = e.row();
      r.coeff[index] = e.coefficient();
      DCHECK_NE(0.0, e.coefficient());
      ++index;
    }
    if (index != NUM_ROWS) continue;

    // Since the column didn't touch any previously deleted row, we are sure
    // that the coefficients were left untouched.
    DCHECK_EQ(r.coeff[DELETED], transpose->column(RowToColIndex(r.row[DELETED]))
                                    .LookUpCoefficient(ColToRowIndex(r.col)));
    DCHECK_EQ(r.coeff[MODIFIED],
              transpose->column(RowToColIndex(r.row[MODIFIED]))
                  .LookUpCoefficient(ColToRowIndex(r.col)));

    // We prefer deleting the row with the larger coefficient magnitude because
    // we will divide by this magnitude. TODO(user): Impact?
    if (fabs(r.coeff[DELETED]) < fabs(r.coeff[MODIFIED])) {
      std::swap(r.coeff[DELETED], r.coeff[MODIFIED]);
      std::swap(r.row[DELETED], r.row[MODIFIED]);
    }

    // Save the deleted row for postsolve. Note that we remove it from the
    // transpose at the same time. This last operation is not strictly needed,
    // but it is faster to do it this way (both here and later when we will take
    // the transpose of the final transpose matrix).
    r.deleted_row_as_column.Swap(
        transpose->mutable_column(RowToColIndex(r.row[DELETED])));

    // Move the bound of the deleted constraint to the initially free variable.
    {
      Fractional new_variable_lb =
          lp->constraint_lower_bounds()[r.row[DELETED]];
      Fractional new_variable_ub =
          lp->constraint_upper_bounds()[r.row[DELETED]];
      new_variable_lb /= r.coeff[DELETED];
      new_variable_ub /= r.coeff[DELETED];
      if (r.coeff[DELETED] < 0.0) std::swap(new_variable_lb, new_variable_ub);
      lp->SetVariableBounds(r.col, new_variable_lb, new_variable_ub);
    }

    // Add a multiple of the deleted row to the modified row except on
    // column r.col where the coefficient will be left unchanged.
    r.deleted_row_as_column.AddMultipleToSparseVectorAndIgnoreCommonIndex(
        -r.coeff[MODIFIED] / r.coeff[DELETED], ColToRowIndex(r.col),
        transpose->mutable_column(RowToColIndex(r.row[MODIFIED])));

    // We also need to correct the objective value of the variables involved in
    // the deleted row.
    if (r.objective_coefficient != 0.0) {
      for (const SparseColumn::Entry e : r.deleted_row_as_column) {
        const ColIndex col = RowToColIndex(e.row());
        if (col == r.col) continue;
        const Fractional new_objective =
            lp->objective_coefficients()[col] -
            e.coefficient() * r.objective_coefficient / r.coeff[DELETED];

        // This detects if the objective should actually be zero, but because of
        // the numerical error in the formula above, we have a really low
        // objective instead. The logic is the same as in
        // AddMultipleToSparseVectorAndIgnoreCommonIndex().
        if (fabs(new_objective) > std::numeric_limits<Fractional>::epsilon() *
                                      2.0 *
                                      fabs(lp->objective_coefficients()[col])) {
          lp->SetObjectiveCoefficient(col, new_objective);
        } else {
          lp->SetObjectiveCoefficient(col, 0.0);
        }
      }
    }
    row_deletion_helper_.MarkRowForDeletion(r.row[DELETED]);
    restore_stack_.push_back(r);
  }

  if (!row_deletion_helper_.IsEmpty()) {
    // The order is important.
    lp->UseTransposeMatrixAsReference();
    lp->DeleteRows(row_deletion_helper_.GetMarkedRows());
    return true;
  }
  return false;
}

void DoubletonFreeColumnPreprocessor::RecoverSolution(
    ProblemSolution* solution) const {
  row_deletion_helper_.RestoreDeletedRows(solution);
  for (const RestoreInfo& r : Reverse(restore_stack_)) {
    // Correct the constraint status.
    switch (solution->variable_statuses[r.col]) {
      case VariableStatus::FIXED_VALUE:
        solution->constraint_statuses[r.row[DELETED]] =
            ConstraintStatus::FIXED_VALUE;
        break;
      case VariableStatus::AT_UPPER_BOUND:
        solution->constraint_statuses[r.row[DELETED]] =
            r.coeff[DELETED] > 0.0 ? ConstraintStatus::AT_UPPER_BOUND
                                   : ConstraintStatus::AT_LOWER_BOUND;
        break;
      case VariableStatus::AT_LOWER_BOUND:
        solution->constraint_statuses[r.row[DELETED]] =
            r.coeff[DELETED] > 0.0 ? ConstraintStatus::AT_LOWER_BOUND
                                   : ConstraintStatus::AT_UPPER_BOUND;
        break;
      case VariableStatus::FREE:
        solution->constraint_statuses[r.row[DELETED]] = ConstraintStatus::FREE;
        break;
      case VariableStatus::BASIC:
        // The default is good here:
        DCHECK_EQ(solution->constraint_statuses[r.row[DELETED]],
                  ConstraintStatus::BASIC);
        break;
    }

    // Correct the primal variable value.
    {
      Fractional new_variable_value = solution->primal_values[r.col];
      for (const SparseColumn::Entry e : r.deleted_row_as_column) {
        const ColIndex col = RowToColIndex(e.row());
        if (col == r.col) continue;
        new_variable_value -= (e.coefficient() / r.coeff[DELETED]) *
                              solution->primal_values[RowToColIndex(e.row())];
      }
      solution->primal_values[r.col] = new_variable_value;
    }

    // In all cases, we will make the variable r.col VariableStatus::BASIC, so
    // we need to adjust the dual value of the deleted row so that the variable
    // reduced cost is zero. Note that there is nothing to do if the variable
    // was already basic.
    if (solution->variable_statuses[r.col] != VariableStatus::BASIC) {
      solution->variable_statuses[r.col] = VariableStatus::BASIC;
      Fractional current_reduced_cost =
          r.objective_coefficient -
          r.coeff[MODIFIED] * solution->dual_values[r.row[MODIFIED]];
      // We want current_reduced_cost - dual * coeff = 0, so:
      solution->dual_values[r.row[DELETED]] =
          current_reduced_cost / r.coeff[DELETED];
    } else {
      DCHECK_EQ(solution->dual_values[r.row[DELETED]], 0.0);
    }
  }
}

// --------------------------------------------------------
// UnconstrainedVariablePreprocessor
// --------------------------------------------------------

namespace {

// Does the constraint block the variable to go to infinity in the given
// direction? direction is either positive or negative and row is the index of
// the constraint.
bool IsConstraintBlockingVariable(const LinearProgram& linear_program,
                                  Fractional direction, RowIndex row) {
  return direction > 0.0
             ? linear_program.constraint_upper_bounds()[row] != kInfinity
             : linear_program.constraint_lower_bounds()[row] != -kInfinity;
}

}  // namespace

void UnconstrainedVariablePreprocessor::RemoveZeroCostUnconstrainedVariable(
    ColIndex col, Fractional target_bound, LinearProgram* lp) {
  DCHECK_EQ(0.0, lp->objective_coefficients()[col]);
  if (deleted_rows_as_column_.IsEmpty()) {
    deleted_columns_.PopulateFromZero(lp->num_constraints(),
                                      lp->num_variables());
    deleted_rows_as_column_.PopulateFromZero(
        ColToRowIndex(lp->num_variables()),
        RowToColIndex(lp->num_constraints()));
    rhs_.resize(lp->num_constraints(), 0.0);
    activity_sign_correction_.resize(lp->num_constraints(), 1.0);
    is_unbounded_.resize(lp->num_variables(), false);
  }
  const bool is_unbounded_up = (target_bound == kInfinity);
  const SparseColumn& column = lp->GetSparseColumn(col);
  for (const SparseColumn::Entry e : column) {
    const RowIndex row = e.row();
    if (!row_deletion_helper_.IsRowMarked(row)) {
      row_deletion_helper_.MarkRowForDeletion(row);
      const ColIndex row_as_col = RowToColIndex(row);
      deleted_rows_as_column_.mutable_column(row_as_col)
          ->PopulateFromSparseVector(
              lp->GetTransposeSparseMatrix().column(row_as_col));
    }
    const bool is_constraint_upper_bound_relevant =
        e.coefficient() > 0.0 ? !is_unbounded_up : is_unbounded_up;
    activity_sign_correction_[row] =
        is_constraint_upper_bound_relevant ? 1.0 : -1.0;
    rhs_[row] = is_constraint_upper_bound_relevant
                    ? lp->constraint_upper_bounds()[row]
                    : lp->constraint_lower_bounds()[row];

    // TODO(user): Here, we may render the row free, so subsequent columns
    // processed by the columns loop in Run() have more chance to be removed.
    // However, we need to be more careful during the postsolve() if we do that.
  }
  is_unbounded_[col] = true;
  Fractional initial_feasible_value = MinInMagnitudeOrZeroIfInfinite(
      lp->variable_lower_bounds()[col], lp->variable_upper_bounds()[col]);
  deleted_columns_.mutable_column(col)->PopulateFromSparseVector(column);
  column_deletion_helper_.MarkColumnForDeletionWithState(
      col, initial_feasible_value,
      ComputeVariableStatus(initial_feasible_value,
                            lp->variable_lower_bounds()[col],
                            lp->variable_upper_bounds()[col]));
}

bool UnconstrainedVariablePreprocessor::Run(LinearProgram* lp,
                                            TimeLimit* time_limit) {
  RETURN_VALUE_IF_NULL(lp, false);
  const ColIndex num_cols(lp->num_variables());
  for (ColIndex col(0); col < num_cols; ++col) {
    const SparseColumn& column = lp->GetSparseColumn(col);
    if (column.IsEmpty()) continue;
    const Fractional cost =
        lp->GetObjectiveCoefficientForMinimizationVersion(col);

    bool can_be_removed = false;
    Fractional target_bound;

    // If the variable is not constrained toward +infinity and the cost moves in
    // the correct direction we can fix the variable at its upper bound.
    if (cost <= 0.0) {
      can_be_removed = true;
      target_bound = lp->variable_upper_bounds()[col];
      for (const SparseColumn::Entry e : column) {
        if (IsConstraintBlockingVariable(*lp, e.coefficient(), e.row())) {
          can_be_removed = false;
          break;
        }
      }
    }
    if (!can_be_removed && cost >= 0.0) {
      can_be_removed = true;
      target_bound = lp->variable_lower_bounds()[col];
      for (const SparseColumn::Entry e : column) {
        if (IsConstraintBlockingVariable(*lp, -e.coefficient(), e.row())) {
          can_be_removed = false;
          break;
        }
      }
    }

    if (can_be_removed) {
      // If can_be_removed is true but the target bound is infinite and the cost
      // is non-zero, then the problem is
      // ProblemStatus::INFEASIBLE_OR_UNBOUNDED.
      if (!IsFinite(target_bound)) {
        if (cost != 0.0) {
          VLOG(1) << "Problem INFEASIBLE_OR_UNBOUNDED, variable " << col
                  << " can move to " << target_bound << " and its cost is "
                  << cost << ".";
          status_ = ProblemStatus::INFEASIBLE_OR_UNBOUNDED;
          return false;
        } else {
          // We can remove this column and all its constraints! We just need to
          // choose a variable value during the call to RecoverSolution() that
          // makes all the constraint satisfiable. Test this on bnl2.mps.
          if (!in_mip_context_) {
            // TODO(user): this also works if the variable is integer, but we
            // must choose an integer value during the post-solve. Implement
            // this.
            RemoveZeroCostUnconstrainedVariable(col, target_bound, lp);
          }
          continue;
        }
      }
      column_deletion_helper_.MarkColumnForDeletionWithState(
          col, target_bound,
          ComputeVariableStatus(target_bound, lp->variable_lower_bounds()[col],
                                lp->variable_upper_bounds()[col]));
    }
  }

  // Change the rhs to reflect the fixed variables. Note that is is important to
  // do that after all the calls to RemoveZeroCostUnconstrainedVariable()
  // because RemoveZeroCostUnconstrainedVariable() needs to store the rhs before
  // this modification!
  const ColIndex end = column_deletion_helper_.GetMarkedColumns().size();
  for (ColIndex col(0); col < end; ++col) {
    if (column_deletion_helper_.IsColumnMarked(col)) {
      const Fractional target_bound =
          column_deletion_helper_.GetStoredValue()[col];
      SubtractColumnMultipleFromConstraintBound(col, target_bound, lp);
    }
  }

  lp->DeleteColumns(column_deletion_helper_.GetMarkedColumns());
  lp->DeleteRows(row_deletion_helper_.GetMarkedRows());
  return !column_deletion_helper_.IsEmpty() || !row_deletion_helper_.IsEmpty();
}

void UnconstrainedVariablePreprocessor::RecoverSolution(
    ProblemSolution* solution) const {
  RETURN_IF_NULL(solution);
  column_deletion_helper_.RestoreDeletedColumns(solution);
  row_deletion_helper_.RestoreDeletedRows(solution);

  // Compute the last deleted column index for each deleted rows.
  const RowIndex num_rows = solution->dual_values.size();
  RowToColMapping last_deleted_column(num_rows, kInvalidCol);
  for (RowIndex row(0); row < num_rows; ++row) {
    if (row_deletion_helper_.IsRowMarked(row)) {
      for (const SparseColumn::Entry e :
           deleted_rows_as_column_.column(RowToColIndex(row))) {
        const ColIndex col = RowToColIndex(e.row());
        if (is_unbounded_[col]) {
          last_deleted_column[row] = col;
        }
      }
    }
  }

  // Note that this will be empty if there were no deleted rows.
  const ColIndex num_cols = is_unbounded_.size();
  for (ColIndex col(0); col < num_cols; ++col) {
    if (!is_unbounded_[col]) continue;
    Fractional primal_value_shift = 0.0;
    RowIndex row_at_bound = kInvalidRow;
    for (const SparseColumn::Entry e : deleted_columns_.column(col)) {
      const RowIndex row = e.row();
      // The second condition is for VariableStatus::FREE rows.
      // TODO(user): In presense of free row, we must move them to 0.
      // Note that currently VariableStatus::FREE rows should be removed before
      // this is called.
      DCHECK(IsFinite(rhs_[row]));
      if (last_deleted_column[row] != col || !IsFinite(rhs_[row])) continue;
      const SparseColumn& row_as_column =
          deleted_rows_as_column_.column(RowToColIndex(row));
      const Fractional activity =
          rhs_[row] - ScalarProduct(solution->primal_values, row_as_column);

      // activity and sign correction must have the same sign or be zero. If
      // not, we find the first unbounded variable and change it accordingly.
      // Note that by construction, the variable value will move towards its
      // unbounded direction.
      if (activity * activity_sign_correction_[row] < 0.0) {
        const Fractional bound = activity / e.coefficient();
        if (fabs(bound) > fabs(primal_value_shift)) {
          primal_value_shift = bound;
          row_at_bound = row;
        }
      }
    }
    solution->primal_values[col] += primal_value_shift;
    if (row_at_bound != kInvalidRow) {
      solution->variable_statuses[col] = VariableStatus::BASIC;
      solution->constraint_statuses[row_at_bound] =
          activity_sign_correction_[row_at_bound] == 1.0
              ? ConstraintStatus::AT_UPPER_BOUND
              : ConstraintStatus::AT_LOWER_BOUND;
    }
  }
}

// --------------------------------------------------------
// FreeConstraintPreprocessor
// --------------------------------------------------------

bool FreeConstraintPreprocessor::Run(LinearProgram* linear_program,
                                     TimeLimit* time_limit) {
  RETURN_VALUE_IF_NULL(linear_program, false);
  const RowIndex num_rows = linear_program->num_constraints();
  for (RowIndex row(0); row < num_rows; ++row) {
    const Fractional lower_bound =
        linear_program->constraint_lower_bounds()[row];
    const Fractional upper_bound =
        linear_program->constraint_upper_bounds()[row];
    if (lower_bound == -kInfinity && upper_bound == kInfinity) {
      row_deletion_helper_.MarkRowForDeletion(row);
    }
  }
  linear_program->DeleteRows(row_deletion_helper_.GetMarkedRows());
  return !row_deletion_helper_.IsEmpty();
}

void FreeConstraintPreprocessor::RecoverSolution(
    ProblemSolution* solution) const {
  RETURN_IF_NULL(solution);
  row_deletion_helper_.RestoreDeletedRows(solution);
}

// --------------------------------------------------------
// EmptyConstraintPreprocessor
// --------------------------------------------------------

bool EmptyConstraintPreprocessor::Run(LinearProgram* lp,
                                      TimeLimit* time_limit) {
  RETURN_VALUE_IF_NULL(lp, false);
  const RowIndex num_rows(lp->num_constraints());
  const ColIndex num_cols(lp->num_variables());

  // Compute degree.
  StrictITIVector<RowIndex, int> degree(num_rows, 0);
  for (ColIndex col(0); col < num_cols; ++col) {
    for (const SparseColumn::Entry e : lp->GetSparseColumn(col)) {
      ++degree[e.row()];
    }
  }

  // Delete degree 0 rows.
  for (RowIndex row(0); row < num_rows; ++row) {
    if (degree[row] == 0) {
      // We need to check that 0.0 is allowed by the constraint bounds,
      // otherwise, the problem is ProblemStatus::PRIMAL_INFEASIBLE.
      if (!IsSmallerWithinFeasibilityTolerance(
              lp->constraint_lower_bounds()[row], 0) ||
          !IsSmallerWithinFeasibilityTolerance(
              0, lp->constraint_upper_bounds()[row])) {
        VLOG(1) << "Problem PRIMAL_INFEASIBLE, constraint " << row
                << " is empty and its range ["
                << lp->constraint_lower_bounds()[row] << ","
                << lp->constraint_upper_bounds()[row] << "] doesn't contain 0.";
        status_ = ProblemStatus::PRIMAL_INFEASIBLE;
        return false;
      }
      row_deletion_helper_.MarkRowForDeletion(row);
    }
  }
  lp->DeleteRows(row_deletion_helper_.GetMarkedRows());
  return !row_deletion_helper_.IsEmpty();
}

void EmptyConstraintPreprocessor::RecoverSolution(
    ProblemSolution* solution) const {
  RETURN_IF_NULL(solution);
  row_deletion_helper_.RestoreDeletedRows(solution);
}

// --------------------------------------------------------
// SingletonPreprocessor
// --------------------------------------------------------

SingletonUndo::SingletonUndo(OperationType type, const LinearProgram& lp,
                             MatrixEntry e, ConstraintStatus status)
    : type_(type),
      is_maximization_(lp.IsMaximizationProblem()),
      e_(e),
      cost_(lp.objective_coefficients()[e.col]),
      variable_lower_bound_(lp.variable_lower_bounds()[e.col]),
      variable_upper_bound_(lp.variable_upper_bounds()[e.col]),
      constraint_lower_bound_(lp.constraint_lower_bounds()[e.row]),
      constraint_upper_bound_(lp.constraint_upper_bounds()[e.row]),
      constraint_status_(status) {}

void SingletonUndo::Undo(const GlopParameters& parameters,
                         const SparseMatrix& deleted_columns,
                         const SparseMatrix& deleted_rows,
                         ProblemSolution* solution) const {
  switch (type_) {
    case SINGLETON_ROW:
      SingletonRowUndo(deleted_columns, solution);
      break;
    case ZERO_COST_SINGLETON_COLUMN:
      ZeroCostSingletonColumnUndo(parameters, deleted_rows, solution);
      break;
    case SINGLETON_COLUMN_IN_EQUALITY:
      SingletonColumnInEqualityUndo(parameters, deleted_rows, solution);
      break;
    case MAKE_CONSTRAINT_AN_EQUALITY:
      MakeConstraintAnEqualityUndo(solution);
      break;
  }
}

void SingletonPreprocessor::DeleteSingletonRow(MatrixEntry e,
                                               LinearProgram* lp) {
  Fractional implied_lower_bound =
      lp->constraint_lower_bounds()[e.row] / e.coeff;
  Fractional implied_upper_bound =
      lp->constraint_upper_bounds()[e.row] / e.coeff;
  if (e.coeff < 0.0) {
    std::swap(implied_lower_bound, implied_upper_bound);
  }
  Fractional new_lower_bound =
      std::max(lp->variable_lower_bounds()[e.col], implied_lower_bound);
  Fractional new_upper_bound =
      std::min(lp->variable_upper_bounds()[e.col], implied_upper_bound);

  if (new_upper_bound < new_lower_bound) {
    if (!IsSmallerWithinFeasibilityTolerance(new_lower_bound,
                                             new_upper_bound)) {
      VLOG(1) << "Problem ProblemStatus::INFEASIBLE_OR_UNBOUNDED, singleton "
                 "row causes the bound of the variable "
              << e.col << " to be infeasible by "
              << new_lower_bound - new_upper_bound;
      status_ = ProblemStatus::PRIMAL_INFEASIBLE;
      return;
    }
    // Otherwise, fix the variable to one of its bounds.
    if (new_lower_bound == lp->variable_lower_bounds()[e.col]) {
      new_upper_bound = new_lower_bound;
    }
    if (new_upper_bound == lp->variable_upper_bounds()[e.col]) {
      new_lower_bound = new_upper_bound;
    }
    DCHECK_EQ(new_lower_bound, new_upper_bound);
  }
  row_deletion_helper_.MarkRowForDeletion(e.row);
  undo_stack_.push_back(SingletonUndo(SingletonUndo::SINGLETON_ROW, *lp, e,
                                      ConstraintStatus::FREE));
  if (deleted_columns_.column(e.col).IsEmpty()) {
    deleted_columns_.mutable_column(e.col)->PopulateFromSparseVector(
        lp->GetSparseColumn(e.col));
  }

  lp->SetVariableBounds(e.col, new_lower_bound, new_upper_bound);
}

// The dual value of the row needs to be corrected to stay at the optimal.
void SingletonUndo::SingletonRowUndo(const SparseMatrix& deleted_columns,
                                     ProblemSolution* solution) const {
  DCHECK_EQ(0, solution->dual_values[e_.row]);

  // If the variable is basic or free, we can just keep the constraint
  // VariableStatus::BASIC and
  // 0.0 as the dual value.
  const VariableStatus status = solution->variable_statuses[e_.col];
  if (status == VariableStatus::BASIC || status == VariableStatus::FREE) return;

  // Compute whether or not the variable bounds changed.
  Fractional implied_lower_bound = constraint_lower_bound_ / e_.coeff;
  Fractional implied_upper_bound = constraint_upper_bound_ / e_.coeff;
  if (e_.coeff < 0.0) {
    std::swap(implied_lower_bound, implied_upper_bound);
  }
  const bool lower_bound_changed = implied_lower_bound > variable_lower_bound_;
  const bool upper_bound_changed = implied_upper_bound < variable_upper_bound_;

  if (!lower_bound_changed && !upper_bound_changed) return;
  if (status == VariableStatus::AT_LOWER_BOUND && !lower_bound_changed) return;
  if (status == VariableStatus::AT_UPPER_BOUND && !upper_bound_changed) return;

  // This is the reduced cost of the variable before the singleton constraint is
  // added back.
  const Fractional reduced_cost =
      cost_ -
      ScalarProduct(solution->dual_values, deleted_columns.column(e_.col));
  const Fractional reduced_cost_for_minimization =
      is_maximization_ ? -reduced_cost : reduced_cost;

  if (status == VariableStatus::FIXED_VALUE) {
    DCHECK(lower_bound_changed || upper_bound_changed);
    if (reduced_cost_for_minimization >= 0.0 && !lower_bound_changed) {
      solution->variable_statuses[e_.col] = VariableStatus::AT_LOWER_BOUND;
      return;
    }
    if (reduced_cost_for_minimization <= 0.0 && !upper_bound_changed) {
      solution->variable_statuses[e_.col] = VariableStatus::AT_UPPER_BOUND;
      return;
    }
  }

  // If one of the variable bounds changes, and the variable is no longer at one
  // of its bounds, then its reduced cost needs to be set to 0.0 and the
  // variable becomes a basic variable. This is what the line below do, since
  // the new reduced cost of the variable will be equal to:
  //     old_reduced_cost - coeff * solution->dual_values[row]
  solution->dual_values[e_.row] = reduced_cost / e_.coeff;
  ConstraintStatus new_constraint_status = VariableToConstraintStatus(status);
  if (status == VariableStatus::FIXED_VALUE &&
      (!lower_bound_changed || !upper_bound_changed)) {
    new_constraint_status = lower_bound_changed
                                ? ConstraintStatus::AT_LOWER_BOUND
                                : ConstraintStatus::AT_UPPER_BOUND;
  }
  if (e_.coeff < 0.0) {
    if (new_constraint_status == ConstraintStatus::AT_LOWER_BOUND) {
      new_constraint_status = ConstraintStatus::AT_UPPER_BOUND;
    } else if (new_constraint_status == ConstraintStatus::AT_UPPER_BOUND) {
      new_constraint_status = ConstraintStatus::AT_LOWER_BOUND;
    }
  }
  solution->variable_statuses[e_.col] = VariableStatus::BASIC;
  solution->constraint_statuses[e_.row] = new_constraint_status;
}

void SingletonPreprocessor::UpdateConstraintBoundsWithVariableBounds(
    MatrixEntry e, LinearProgram* lp) {
  Fractional lower_delta = -e.coeff * lp->variable_upper_bounds()[e.col];
  Fractional upper_delta = -e.coeff * lp->variable_lower_bounds()[e.col];
  if (e.coeff < 0.0) {
    std::swap(lower_delta, upper_delta);
  }
  lp->SetConstraintBounds(e.row,
                          lp->constraint_lower_bounds()[e.row] + lower_delta,
                          lp->constraint_upper_bounds()[e.row] + upper_delta);
}


void SingletonPreprocessor::DeleteZeroCostSingletonColumn(
    const SparseMatrix& transpose, MatrixEntry e, LinearProgram* lp) {
  const ColIndex transpose_col = RowToColIndex(e.row);
  const SparseColumn& column = transpose.column(transpose_col);
  undo_stack_.push_back(SingletonUndo(SingletonUndo::ZERO_COST_SINGLETON_COLUMN,
                                      *lp, e, ConstraintStatus::FREE));
  if (deleted_rows_.column(transpose_col).IsEmpty()) {
    deleted_rows_.mutable_column(transpose_col)
        ->PopulateFromSparseVector(column);
  }
  UpdateConstraintBoundsWithVariableBounds(e, lp);
  column_deletion_helper_.MarkColumnForDeletion(e.col);
}

// We need to restore the variable value in order to satisfy the constraint.
void SingletonUndo::ZeroCostSingletonColumnUndo(
    const GlopParameters& parameters, const SparseMatrix& deleted_rows,
    ProblemSolution* solution) const {
  // If the variable was fixed, this is easy. Note that this is the only
  // possible case if the current constraint status is FIXED.
  if (variable_upper_bound_ == variable_lower_bound_) {
    solution->primal_values[e_.col] = variable_lower_bound_;
    solution->variable_statuses[e_.col] = VariableStatus::FIXED_VALUE;
    return;
  }

  const ConstraintStatus ct_status = solution->constraint_statuses[e_.row];
  DCHECK_NE(ct_status, ConstraintStatus::FIXED_VALUE);
  if (ct_status == ConstraintStatus::AT_LOWER_BOUND ||
      ct_status == ConstraintStatus::AT_UPPER_BOUND) {
    if ((ct_status == ConstraintStatus::AT_UPPER_BOUND && e_.coeff > 0.0) ||
        (ct_status == ConstraintStatus::AT_LOWER_BOUND && e_.coeff < 0.0)) {
      DCHECK(IsFinite(variable_lower_bound_));
      solution->primal_values[e_.col] = variable_lower_bound_;
      solution->variable_statuses[e_.col] = VariableStatus::AT_LOWER_BOUND;
    } else {
      DCHECK(IsFinite(variable_upper_bound_));
      solution->primal_values[e_.col] = variable_upper_bound_;
      solution->variable_statuses[e_.col] = VariableStatus::AT_UPPER_BOUND;
    }
    if (constraint_upper_bound_ == constraint_lower_bound_) {
      solution->constraint_statuses[e_.row] = ConstraintStatus::FIXED_VALUE;
    }
    return;
  }

  // This is the activity of the constraint before the singleton variable is
  // added back to it.
  const ColIndex row_as_col = RowToColIndex(e_.row);
  const Fractional activity =
      ScalarProduct(solution->primal_values, deleted_rows.column(row_as_col));

  // First we try to fix the variable at its lower or upper bound and leave the
  // constraint VariableStatus::BASIC. Note that we use the same logic as in
  // Preprocessor::IsSmallerWithinPreprocessorZeroTolerance() which we can't use
  // here because we are not deriving from the Preprocessor class.
  const Fractional tolerance = parameters.preprocessor_zero_tolerance();
  const auto is_smaller_with_tolerance = [tolerance](Fractional a,
                                                     Fractional b) {
    return ::operations_research::IsSmallerWithinTolerance(a, b, tolerance);
  };
  if (variable_lower_bound_ != -kInfinity) {
    const Fractional activity_at_lb =
        activity + e_.coeff * variable_lower_bound_;
    if (is_smaller_with_tolerance(constraint_lower_bound_, activity_at_lb) &&
        is_smaller_with_tolerance(activity_at_lb, constraint_upper_bound_)) {
      solution->primal_values[e_.col] = variable_lower_bound_;
      solution->variable_statuses[e_.col] = VariableStatus::AT_LOWER_BOUND;
      return;
    }
  }
  if (variable_upper_bound_ != kInfinity) {
    const Fractional actibity_at_ub =
        activity + e_.coeff * variable_upper_bound_;
    if (is_smaller_with_tolerance(constraint_lower_bound_, actibity_at_ub) &&
        is_smaller_with_tolerance(actibity_at_ub, constraint_upper_bound_)) {
      solution->primal_values[e_.col] = variable_upper_bound_;
      solution->variable_statuses[e_.col] = VariableStatus::AT_UPPER_BOUND;
      return;
    }
  }

  // If the current constraint is UNBOUNDED, then the variable is too
  // because of the two cases above. We just set its status to
  // VariableStatus::FREE.
  if (constraint_lower_bound_ == -kInfinity &&
      constraint_upper_bound_ == kInfinity) {
    solution->primal_values[e_.col] = 0.0;
    solution->variable_statuses[e_.col] = VariableStatus::FREE;
    return;
  }

  // If the previous cases didn't apply, the constraint will be fixed to its
  // bounds and the variable will be made VariableStatus::BASIC.
  solution->variable_statuses[e_.col] = VariableStatus::BASIC;
  if (constraint_lower_bound_ == constraint_upper_bound_) {
    solution->primal_values[e_.col] =
        (constraint_lower_bound_ - activity) / e_.coeff;
    solution->constraint_statuses[e_.row] = ConstraintStatus::FIXED_VALUE;
    return;
  }

  bool set_constraint_to_lower_bound;
  if (constraint_lower_bound_ == -kInfinity) {
    set_constraint_to_lower_bound = false;
  } else if (constraint_upper_bound_ == kInfinity) {
    set_constraint_to_lower_bound = true;
  } else {
    // In this case we select the value that is the most inside the variable
    // bound.
    const Fractional to_lb = (constraint_lower_bound_ - activity) / e_.coeff;
    const Fractional to_ub = (constraint_upper_bound_ - activity) / e_.coeff;
    set_constraint_to_lower_bound =
        std::max(variable_lower_bound_ - to_lb, to_lb - variable_upper_bound_) <
        std::max(variable_lower_bound_ - to_ub, to_ub - variable_upper_bound_);
  }

  if (set_constraint_to_lower_bound) {
    solution->primal_values[e_.col] =
        (constraint_lower_bound_ - activity) / e_.coeff;
    solution->constraint_statuses[e_.row] = ConstraintStatus::AT_LOWER_BOUND;
  } else {
    solution->primal_values[e_.col] =
        (constraint_upper_bound_ - activity) / e_.coeff;
    solution->constraint_statuses[e_.row] = ConstraintStatus::AT_UPPER_BOUND;
  }
}

void SingletonPreprocessor::DeleteSingletonColumnInEquality(
    const SparseMatrix& transpose, MatrixEntry e, LinearProgram* lp) {
  // Save information for the undo.
  const ColIndex transpose_col = RowToColIndex(e.row);
  const SparseColumn& row_as_column = transpose.column(transpose_col);
  undo_stack_.push_back(
      SingletonUndo(SingletonUndo::SINGLETON_COLUMN_IN_EQUALITY, *lp, e,
                    ConstraintStatus::FREE));
  deleted_rows_.mutable_column(transpose_col)
      ->PopulateFromSparseVector(row_as_column);

  // Update the objective function using the equality constraint. We have
  //     v_col*coeff + expression = rhs,
  // so the contribution of this variable to the cost function (v_col * cost)
  // can be rewrited as:
  //     (rhs * cost - expression * cost) / coeff.
  const Fractional rhs = lp->constraint_upper_bounds()[e.row];
  const Fractional cost = lp->objective_coefficients()[e.col];
  const Fractional multiplier = cost / e.coeff;
  lp->SetObjectiveOffset(lp->objective_offset() + rhs * multiplier);
  for (const SparseColumn::Entry e : row_as_column) {
    const ColIndex col = RowToColIndex(e.row());
    if (!column_deletion_helper_.IsColumnMarked(col)) {
      Fractional new_cost =
          lp->objective_coefficients()[col] - e.coefficient() * multiplier;

      // TODO(user): It is important to avoid having non-zero costs which are
      // the result of numerical error. This is because we still miss some
      // tolerances in a few preprocessors. Like an empty column with a cost of
      // 1e-17 and unbounded towards infinity is currently implying that the
      // problem is unbounded. This will need fixing.
      if (fabs(new_cost) < parameters_.preprocessor_zero_tolerance()) {
        new_cost = 0.0;
      }
      lp->SetObjectiveCoefficient(col, new_cost);
    }
  }

  // Now delete the column like a singleton column without cost.
  UpdateConstraintBoundsWithVariableBounds(e, lp);
  column_deletion_helper_.MarkColumnForDeletion(e.col);
}

void SingletonUndo::SingletonColumnInEqualityUndo(
    const GlopParameters& parameters, const SparseMatrix& deleted_rows,
    ProblemSolution* solution) const {
  // First do the same as a zero-cost singleton column.
  ZeroCostSingletonColumnUndo(parameters, deleted_rows, solution);

  // Then, restore the dual optimal value taking into account the cost
  // modification.
  solution->dual_values[e_.row] += cost_ / e_.coeff;
  if (solution->constraint_statuses[e_.row] == ConstraintStatus::BASIC) {
    solution->variable_statuses[e_.col] = VariableStatus::BASIC;
    solution->constraint_statuses[e_.row] = ConstraintStatus::FIXED_VALUE;
  }
}

void SingletonUndo::MakeConstraintAnEqualityUndo(
    ProblemSolution* solution) const {
  if (solution->constraint_statuses[e_.row] == ConstraintStatus::FIXED_VALUE) {
    solution->constraint_statuses[e_.row] = constraint_status_;
  }
}

bool SingletonPreprocessor::MakeConstraintAnEqualityIfPossible(
    const SparseMatrix& transpose, MatrixEntry e, LinearProgram* lp) {
  // TODO(user): We could skip early if the relevant constraint bound is
  // infinity.
  const Fractional cst_lower_bound = lp->constraint_lower_bounds()[e.row];
  const Fractional cst_upper_bound = lp->constraint_upper_bounds()[e.row];
  if (cst_lower_bound == cst_upper_bound) return true;

  // To be efficient, we only process a row once and cache the domain that an
  // "artificial" extra variable x with coefficient 1.0 could take while still
  // making the constraint feasible. The domain bounds for the constraint e.row
  // will be stored in row_lb_sum_[e.row] and row_ub_sum_[e.row].
  const DenseRow& variable_ubs = lp->variable_upper_bounds();
  const DenseRow& variable_lbs = lp->variable_lower_bounds();
  if (e.row >= row_sum_is_cached_.size() || !row_sum_is_cached_[e.row]) {
    if (e.row >= row_sum_is_cached_.size()) {
      const int new_size = e.row.value() + 1;
      row_sum_is_cached_.resize(new_size);
      row_lb_sum_.resize(new_size);
      row_ub_sum_.resize(new_size);
    }
    row_sum_is_cached_[e.row] = true;
    row_lb_sum_[e.row].Add(cst_lower_bound);
    row_ub_sum_[e.row].Add(cst_upper_bound);
    for (const SparseColumn::Entry entry :
         transpose.column(RowToColIndex(e.row))) {
      const ColIndex row_as_col = RowToColIndex(entry.row());

      // Tricky: Even if later more columns are deleted, these "cached" sums
      // will actually still be valid because we only delete columns in a
      // compatible way.
      //
      // TODO(user): Find a more robust way? it seems easy to add new deletion
      // rules that may break this assumption.
      if (column_deletion_helper_.IsColumnMarked(row_as_col)) continue;
      if (entry.coefficient() > 0.0) {
        row_lb_sum_[e.row].Add(-entry.coefficient() * variable_ubs[row_as_col]);
        row_ub_sum_[e.row].Add(-entry.coefficient() * variable_lbs[row_as_col]);
      } else {
        row_lb_sum_[e.row].Add(-entry.coefficient() * variable_lbs[row_as_col]);
        row_ub_sum_[e.row].Add(-entry.coefficient() * variable_ubs[row_as_col]);
      }

      // TODO(user): Abort early if both sums contain more than 1 infinity?
    }
  }

  // Now that the lb/ub sum for the row is cached, we can use it to compute the
  // implied bounds on the variable from this constraint and the other
  // variables.
  const Fractional c = e.coeff;
  const Fractional lb =
      c > 0.0 ? row_lb_sum_[e.row].SumWithout(-c * variable_ubs[e.col]) / c
              : row_ub_sum_[e.row].SumWithout(-c * variable_ubs[e.col]) / c;
  const Fractional ub =
      c > 0.0 ? row_ub_sum_[e.row].SumWithout(-c * variable_lbs[e.col]) / c
              : row_lb_sum_[e.row].SumWithout(-c * variable_lbs[e.col]) / c;

  // Note that we could do the same for singleton variables with a cost of
  // 0.0, but such variable are already dealt with by
  // DeleteZeroCostSingletonColumn() so there is no point.
  const Fractional cost =
      lp->GetObjectiveCoefficientForMinimizationVersion(e.col);
  DCHECK_NE(cost, 0.0);

  // Note that some of the tests below will be always true if the bounds of
  // the column of index col are infinite. This is the desired behavior.
  ConstraintStatus relaxed_status = ConstraintStatus::FIXED_VALUE;
  if (cost < 0.0 && IsSmallerWithinPreprocessorZeroTolerance(
                        ub, lp->variable_upper_bounds()[e.col])) {
    if (e.coeff > 0) {
      if (cst_upper_bound == kInfinity) {
        status_ = ProblemStatus::INFEASIBLE_OR_UNBOUNDED;
      } else {
        relaxed_status = ConstraintStatus::AT_UPPER_BOUND;
        lp->SetConstraintBounds(e.row, cst_upper_bound, cst_upper_bound);
      }
    } else {
      if (cst_lower_bound == -kInfinity) {
        status_ = ProblemStatus::INFEASIBLE_OR_UNBOUNDED;
      } else {
        relaxed_status = ConstraintStatus::AT_LOWER_BOUND;
        lp->SetConstraintBounds(e.row, cst_lower_bound, cst_lower_bound);
      }
    }

    if (status_ == ProblemStatus::INFEASIBLE_OR_UNBOUNDED) {
      DCHECK_EQ(ub, kInfinity);
      VLOG(1) << "Problem ProblemStatus::INFEASIBLE_OR_UNBOUNDED, singleton "
                 "variable "
              << e.col << " has a cost (for minimization) of " << cost
              << " and is unbounded towards kInfinity.";
      return false;
    }

    // This is important but tricky: The upper bound of the variable needs to
    // be relaxed. This is valid because the implied bound is lower than the
    // original upper bound here. This is needed, so that the optimal
    // primal/dual values of the new problem will also be optimal of the
    // original one.
    //
    // Let's prove the case coeff > 0.0 for a minimization problem. In the new
    // problem, because the variable is unbounded towards +infinity, its
    // reduced cost must satisfy at optimality rc = cost - coeff * dual_v >=
    // 0. But this implies dual_v <= cost / coeff <= 0. This is exactly what
    // is needed for the optimality of the initial problem since the
    // constraint will be at its upper bound, and the corresponding slack
    // condition is that the dual value needs to be <= 0.
    lp->SetVariableBounds(e.col, lp->variable_lower_bounds()[e.col], kInfinity);
  }
  if (cost > 0.0 && IsSmallerWithinPreprocessorZeroTolerance(
                        lp->variable_lower_bounds()[e.col], lb)) {
    if (e.coeff > 0) {
      if (cst_lower_bound == -kInfinity) {
        status_ = ProblemStatus::INFEASIBLE_OR_UNBOUNDED;
      } else {
        relaxed_status = ConstraintStatus::AT_LOWER_BOUND;
        lp->SetConstraintBounds(e.row, cst_lower_bound, cst_lower_bound);
      }
    } else {
      if (cst_upper_bound == kInfinity) {
        status_ = ProblemStatus::INFEASIBLE_OR_UNBOUNDED;
      } else {
        relaxed_status = ConstraintStatus::AT_UPPER_BOUND;
        lp->SetConstraintBounds(e.row, cst_upper_bound, cst_upper_bound);
      }
    }

    if (status_ == ProblemStatus::INFEASIBLE_OR_UNBOUNDED) {
      DCHECK_EQ(lb, -kInfinity);
      VLOG(1) << "Problem ProblemStatus::INFEASIBLE_OR_UNBOUNDED, singleton "
                 "variable "
              << e.col << " has a cost (for minimization) of " << cost
              << " and is unbounded towards -kInfinity.";
      return false;
    }

    // Same remark as above for a lower bounded variable this time.
    lp->SetVariableBounds(e.col, -kInfinity,
                          lp->variable_upper_bounds()[e.col]);
  }

  if (lp->constraint_lower_bounds()[e.row] ==
      lp->constraint_upper_bounds()[e.row]) {
    undo_stack_.push_back(SingletonUndo(
        SingletonUndo::MAKE_CONSTRAINT_AN_EQUALITY, *lp, e, relaxed_status));
    return true;
  }
  return false;
}

bool SingletonPreprocessor::Run(LinearProgram* lp, TimeLimit* time_limit) {
  RETURN_VALUE_IF_NULL(lp, false);
  const SparseMatrix& matrix = lp->GetSparseMatrix();
  const SparseMatrix& transpose = lp->GetTransposeSparseMatrix();

  // Initialize column_to_process with the current singleton columns.
  ColIndex num_cols(matrix.num_cols());
  RowIndex num_rows(matrix.num_rows());
  deleted_columns_.PopulateFromZero(num_rows, num_cols);
  deleted_rows_.PopulateFromZero(ColToRowIndex(num_cols),
                                 RowToColIndex(num_rows));
  StrictITIVector<ColIndex, EntryIndex> column_degree(num_cols, EntryIndex(0));
  std::vector<ColIndex> column_to_process;
  for (ColIndex col(0); col < num_cols; ++col) {
    column_degree[col] = matrix.column(col).num_entries();
    if (column_degree[col] == 1) {
      column_to_process.push_back(col);
    }
  }

  // Initialize row_to_process with the current singleton rows.
  StrictITIVector<RowIndex, EntryIndex> row_degree(num_rows, EntryIndex(0));
  std::vector<RowIndex> row_to_process;
  for (RowIndex row(0); row < num_rows; ++row) {
    row_degree[row] = transpose.column(RowToColIndex(row)).num_entries();
    if (row_degree[row] == 1) {
      row_to_process.push_back(row);
    }
  }

  // Process current singleton rows/columns and enqueue new ones.
  while (status_ == ProblemStatus::INIT &&
         (!column_to_process.empty() || !row_to_process.empty())) {
    while (status_ == ProblemStatus::INIT && !column_to_process.empty()) {
      const ColIndex col = column_to_process.back();
      column_to_process.pop_back();
      if (column_degree[col] <= 0) continue;
      const MatrixEntry e = GetSingletonColumnMatrixEntry(col, matrix);
      // TODO(user): It seems better to process all the singleton columns with
      // a cost of zero first.
      if (lp->objective_coefficients()[col] == 0.0) {
        DeleteZeroCostSingletonColumn(transpose, e, lp);
      } else if (MakeConstraintAnEqualityIfPossible(transpose, e, lp)) {
        DeleteSingletonColumnInEquality(transpose, e, lp);
      } else {
        continue;
      }
      --row_degree[e.row];
      if (row_degree[e.row] == 1) {
        row_to_process.push_back(e.row);
      }
    }
    while (status_ == ProblemStatus::INIT && !row_to_process.empty()) {
      const RowIndex row = row_to_process.back();
      row_to_process.pop_back();
      if (row_degree[row] <= 0) continue;
      const MatrixEntry e = GetSingletonRowMatrixEntry(row, transpose);

      DeleteSingletonRow(e, lp);
      --column_degree[e.col];
      if (column_degree[e.col] == 1) {
        column_to_process.push_back(e.col);
      }
    }
  }

  if (status_ != ProblemStatus::INIT) return false;
  lp->DeleteColumns(column_deletion_helper_.GetMarkedColumns());
  lp->DeleteRows(row_deletion_helper_.GetMarkedRows());
  return !column_deletion_helper_.IsEmpty() || !row_deletion_helper_.IsEmpty();
}

void SingletonPreprocessor::RecoverSolution(ProblemSolution* solution) const {
  RETURN_IF_NULL(solution);

  // Note that the two deletion helpers must restore 0.0 values in the positions
  // that will be used during Undo(). That is, all the calls done by this class
  // to MarkColumnForDeletion() should be done with 0.0 as the value to restore
  // (which is already the case when using MarkRowForDeletion()).
  // This is important because the various Undo() functions assume that a
  // primal/dual variable value which isn't restored yet has the value of 0.0.
  column_deletion_helper_.RestoreDeletedColumns(solution);
  row_deletion_helper_.RestoreDeletedRows(solution);

  // It is important to undo the operations in the correct order, i.e. in the
  // reverse order in which they were done.
  for (int i = undo_stack_.size() - 1; i >= 0; --i) {
    undo_stack_[i].Undo(parameters_, deleted_columns_, deleted_rows_, solution);
  }
}

MatrixEntry SingletonPreprocessor::GetSingletonColumnMatrixEntry(
    ColIndex col, const SparseMatrix& matrix) {
  for (const SparseColumn::Entry e : matrix.column(col)) {
    if (!row_deletion_helper_.IsRowMarked(e.row())) {
      DCHECK_NE(0.0, e.coefficient());
      return MatrixEntry(e.row(), col, e.coefficient());
    }
  }
  // This shouldn't happen.
  LOG(DFATAL) << "No unmarked entry in a column that is supposed to have one.";
  status_ = ProblemStatus::ABNORMAL;
  return MatrixEntry(RowIndex(0), ColIndex(0), 0.0);
}

MatrixEntry SingletonPreprocessor::GetSingletonRowMatrixEntry(
    RowIndex row, const SparseMatrix& transpose) {
  for (const SparseColumn::Entry e : transpose.column(RowToColIndex(row))) {
    const ColIndex col = RowToColIndex(e.row());
    if (!column_deletion_helper_.IsColumnMarked(col)) {
      DCHECK_NE(0.0, e.coefficient());
      return MatrixEntry(row, col, e.coefficient());
    }
  }
  // This shouldn't happen.
  LOG(DFATAL) << "No unmarked entry in a row that is supposed to have one.";
  status_ = ProblemStatus::ABNORMAL;
  return MatrixEntry(RowIndex(0), ColIndex(0), 0.0);
}

// --------------------------------------------------------
// RemoveNearZeroEntriesPreprocessor
// --------------------------------------------------------

bool RemoveNearZeroEntriesPreprocessor::Run(LinearProgram* lp,
                                            TimeLimit* time_limit) {
  RETURN_VALUE_IF_NULL(lp, false);
  const ColIndex num_cols = lp->num_variables();
  if (num_cols == 0) return false;

  // We will use a different threshold for each row depending on its degree.
  // We use Fractionals for convenience since they will be used as such below.
  const RowIndex num_rows = lp->num_constraints();
  DenseColumn row_degree(num_rows, 0.0);
  Fractional num_non_zero_objective_coefficients = 0.0;
  for (ColIndex col(0); col < num_cols; ++col) {
    for (const SparseColumn::Entry e : lp->GetSparseColumn(col)) {
      row_degree[e.row()] += 1.0;
    }
    if (lp->objective_coefficients()[col] != 0.0) {
      num_non_zero_objective_coefficients += 1.0;
    }
  }

  // To not have too many parameters, we use the preprocessor_zero_tolerance.
  const Fractional allowed_impact = parameters_.preprocessor_zero_tolerance();

  // TODO(user): Our criteria ensure that during presolve a primal feasible
  // solution will stay primal feasible. However, we have no guarantee on the
  // dual-feasibility (because the dual variable values range is not taken into
  // account). Fix that? or find a better criteria since it seems that on all
  // our current problems, this preprocessor helps and doesn't introduce errors.
  const EntryIndex initial_num_entries = lp->num_entries();
  int num_zeroed_objective_coefficients = 0;
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional lower_bound = lp->variable_lower_bounds()[col];
    const Fractional upper_bound = lp->variable_upper_bounds()[col];

    // TODO(user): Write a small class that takes a matrix, its transpose, row
    // and column bounds, and "propagate" the bounds as much as possible so we
    // can use this better estimate here and remove more near-zero entries.
    const Fractional max_magnitude =
        std::max(fabs(lower_bound), fabs(upper_bound));
    if (max_magnitude == kInfinity || max_magnitude == 0) continue;
    const Fractional threshold = allowed_impact / max_magnitude;
    lp->GetMutableSparseColumn(col)->RemoveNearZeroEntriesWithWeights(
        threshold, row_degree);

    if (lp->objective_coefficients()[col] != 0.0 &&
        num_non_zero_objective_coefficients *
                fabs(lp->objective_coefficients()[col]) <
            threshold) {
      lp->SetObjectiveCoefficient(col, 0.0);
      ++num_zeroed_objective_coefficients;
    }
  }

  const EntryIndex num_entries = lp->num_entries();
  if (num_entries != initial_num_entries) {
    VLOG(1) << "Removed " << initial_num_entries - num_entries
            << " near-zero entries.";
  }
  if (num_zeroed_objective_coefficients > 0) {
    VLOG(1) << "Removed " << num_zeroed_objective_coefficients
            << " near-zero objective coefficients.";
  }

  // No postsolve is required.
  return false;
}

void RemoveNearZeroEntriesPreprocessor::RecoverSolution(
    ProblemSolution* solution) const {}

// --------------------------------------------------------
// SingletonColumnSignPreprocessor
// --------------------------------------------------------

bool SingletonColumnSignPreprocessor::Run(LinearProgram* linear_program,
                                          TimeLimit* time_limit) {
  RETURN_VALUE_IF_NULL(linear_program, false);
  const ColIndex num_cols = linear_program->num_variables();
  if (num_cols == 0) return false;

  changed_columns_.clear();
  int num_singletons = 0;
  for (ColIndex col(0); col < num_cols; ++col) {
    SparseColumn* sparse_column = linear_program->GetMutableSparseColumn(col);
    const Fractional cost = linear_program->objective_coefficients()[col];
    if (sparse_column->num_entries() == 1) {
      ++num_singletons;
    }
    if (sparse_column->num_entries() == 1 &&
        sparse_column->GetFirstCoefficient() < 0) {
      sparse_column->MultiplyByConstant(-1.0);
      linear_program->SetVariableBounds(
          col, -linear_program->variable_upper_bounds()[col],
          -linear_program->variable_lower_bounds()[col]);
      linear_program->SetObjectiveCoefficient(col, -cost);
      changed_columns_.push_back(col);
    }
  }
  VLOG(1) << "Changed the sign of " << changed_columns_.size() << " columns.";
  VLOG(1) << num_singletons << " singleton columns left.";
  return !changed_columns_.empty();
}

void SingletonColumnSignPreprocessor::RecoverSolution(
    ProblemSolution* solution) const {
  RETURN_IF_NULL(solution);
  for (int i = 0; i < changed_columns_.size(); ++i) {
    const ColIndex col = changed_columns_[i];
    solution->primal_values[col] = -solution->primal_values[col];
    const VariableStatus status = solution->variable_statuses[col];
    if (status == VariableStatus::AT_UPPER_BOUND) {
      solution->variable_statuses[col] = VariableStatus::AT_LOWER_BOUND;
    } else if (status == VariableStatus::AT_LOWER_BOUND) {
      solution->variable_statuses[col] = VariableStatus::AT_UPPER_BOUND;
    }
  }
}

// --------------------------------------------------------
// DoubletonEqualityRowPreprocessor
// --------------------------------------------------------

bool DoubletonEqualityRowPreprocessor::Run(LinearProgram* lp,
                                           TimeLimit* time_limit) {
  RETURN_VALUE_IF_NULL(lp, false);
  // Note that we don't update the transpose during this preprocessor run.
  const SparseMatrix& original_transpose = lp->GetTransposeSparseMatrix();

  // Iterate over the rows that were already doubletons before this preprocessor
  // run, and whose items don't belong to a column that we deleted during this
  // run. This implies that the rows are only ever touched once per run, because
  // we only modify rows that have an item on a deleted column.
  const RowIndex num_rows(lp->num_constraints());
  for (RowIndex row(0); row < num_rows; ++row) {
    const SparseColumn& original_row =
        original_transpose.column(RowToColIndex(row));
    if (original_row.num_entries() != 2 ||
        lp->constraint_lower_bounds()[row] !=
            lp->constraint_upper_bounds()[row]) {
      continue;
    }

    // Collect the two row items. Skip the ones involving a deleted column.
    // Note: we filled r.col[] and r.coeff[] by item order, and currently we
    // always pick the first column as the to-be-deleted one.
    // TODO(user): make a smarter choice of which column to delete, and
    // swap col[] and coeff[] accordingly.
    RestoreInfo r;  // Use a short name since we're using it everywhere.
    int entry_index = 0;
    for (const SparseColumn::Entry e : original_row) {
      const ColIndex col = RowToColIndex(e.row());
      if (column_deletion_helper_.IsColumnMarked(col)) continue;
      r.col[entry_index] = col;
      r.coeff[entry_index] = e.coefficient();
      DCHECK_NE(0.0, r.coeff[entry_index]);
      ++entry_index;
    }

    // Discard some cases that will be treated by other preprocessors, or by
    // another run of this one.
    // 1) One or two of the items were in a deleted column.
    if (entry_index < 2) continue;

    // Fill the RestoreInfo, even if we end up not using it (because we
    // give up on preprocessing this row): it has a bunch of handy shortcuts.
    r.row = row;
    r.rhs = lp->constraint_lower_bounds()[row];
    for (int col_choice = 0; col_choice < NUM_DOUBLETON_COLS; ++col_choice) {
      const ColIndex col = r.col[col_choice];
      r.lb[col_choice] = lp->variable_lower_bounds()[col];
      r.ub[col_choice] = lp->variable_upper_bounds()[col];
      r.objective_coefficient[col_choice] = lp->objective_coefficients()[col];
      r.column[col_choice].PopulateFromSparseVector(lp->GetSparseColumn(col));
    }

    // 2) One of the columns is fixed: don't bother, it will be treated
    //    by the FixedVariablePreprocessor.
    if (r.lb[DELETED] == r.ub[DELETED] || r.lb[MODIFIED] == r.ub[MODIFIED]) {
      continue;
    }

    // Look at the bounds of both variables and exit early if we can delegate
    // to another pre-processor; otherwise adjust the bounds of the remaining
    // variable as necessary.
    // If the current row is: aX + bY = c, then the bounds of Y must be
    // adjusted to satisfy Y = c/b + (-a/b)X
    //
    // Note: when we compute the coefficients of these equations, we can cause
    // underflows/overflows that could be avoided if we did the computations
    // more carefully; but for now we just treat those cases as
    // ProblemStatus::ABNORMAL.
    // TODO(user): consider skipping the problematic rows in this preprocessor,
    // or trying harder to avoid the under/overflow.
    {
      const Fractional carry_over_offset = r.rhs / r.coeff[MODIFIED];
      const Fractional carry_over_factor =
          -r.coeff[DELETED] / r.coeff[MODIFIED];
      if (!IsFinite(carry_over_offset) || !IsFinite(carry_over_factor) ||
          carry_over_factor == 0.0) {
        status_ = ProblemStatus::ABNORMAL;
        break;
      }
      Fractional lb = r.lb[MODIFIED];
      Fractional ub = r.ub[MODIFIED];
      Fractional carried_over_lb =
          r.lb[DELETED] * carry_over_factor + carry_over_offset;
      Fractional carried_over_ub =
          r.ub[DELETED] * carry_over_factor + carry_over_offset;
      if (carry_over_factor < 0) {
        std::swap(carried_over_lb, carried_over_ub);
      }
      if (carried_over_lb <= lb) {
        // Default (and simplest) case: the lower bound didn't change.
        r.bound_backtracking_at_lower_bound = RestoreInfo::ColChoiceAndStatus(
            MODIFIED, VariableStatus::AT_LOWER_BOUND, lb);
      } else {
        lb = carried_over_lb;
        r.bound_backtracking_at_lower_bound = RestoreInfo::ColChoiceAndStatus(
            DELETED, carry_over_factor > 0 ? VariableStatus::AT_LOWER_BOUND
                                           : VariableStatus::AT_UPPER_BOUND,
            carry_over_factor > 0 ? r.lb[DELETED] : r.ub[DELETED]);
      }
      if (carried_over_ub >= ub) {
        // Default (and simplest) case: the upper bound didn't change.
        r.bound_backtracking_at_upper_bound = RestoreInfo::ColChoiceAndStatus(
            MODIFIED, VariableStatus::AT_UPPER_BOUND, ub);
      } else {
        ub = carried_over_ub;
        r.bound_backtracking_at_upper_bound = RestoreInfo::ColChoiceAndStatus(
            DELETED, carry_over_factor > 0 ? VariableStatus::AT_UPPER_BOUND
                                           : VariableStatus::AT_LOWER_BOUND,
            carry_over_factor > 0 ? r.ub[DELETED] : r.lb[DELETED]);
      }
      // 3) If the new bounds are fixed (the domain is a singleton) or
      //    infeasible, then we let the
      //    ForcingAndImpliedFreeConstraintPreprocessor do the work.
      if (IsSmallerWithinPreprocessorZeroTolerance(ub, lb)) continue;
      lp->SetVariableBounds(r.col[MODIFIED], lb, ub);
    }

    restore_stack_.push_back(r);

    // Now, perform the substitution. If the current row is: aX + bY = c
    // then any other row containing 'X' with coefficient x can remove the
    // entry in X, and instead add an entry on 'Y' with coefficient x(-b/a)
    // and a constant offset x(c/a).
    // Looking at the matrix, this translates into colY += (-b/a) colX.
    DCHECK_NE(r.coeff[DELETED], 0.0);
    const Fractional substitution_factor =
        -r.coeff[MODIFIED] / r.coeff[DELETED];                           // -b/a
    const Fractional constant_offset_factor = r.rhs / r.coeff[DELETED];  // c/a
    // Again we don't bother too much with over/underflows.
    if (!IsFinite(substitution_factor) || substitution_factor == 0.0 ||
        !IsFinite(constant_offset_factor)) {
      status_ = ProblemStatus::ABNORMAL;
      break;
    }
    r.column[DELETED].AddMultipleToSparseVectorAndDeleteCommonIndex(
        substitution_factor, row, lp->GetMutableSparseColumn(r.col[MODIFIED]));

    // Apply similar operations on the objective coefficients.
    // Note that the offset is being updated by
    // SubtractColumnMultipleFromConstraintBound() below.
    lp->SetObjectiveCoefficient(
        r.col[MODIFIED],
        r.objective_coefficient[MODIFIED] +
            substitution_factor * r.objective_coefficient[DELETED]);

    // Carry over the constant factor of the substitution as well.
    // TODO(user): rename that method to reflect the fact that it also updates
    // the objective offset, in the other direction.
    SubtractColumnMultipleFromConstraintBound(r.col[DELETED],
                                              constant_offset_factor, lp);

    // Mark the column and the row for deletion.
    column_deletion_helper_.MarkColumnForDeletion(r.col[DELETED]);
    row_deletion_helper_.MarkRowForDeletion(row);
  }
  if (status_ != ProblemStatus::INIT) return false;
  lp->DeleteColumns(column_deletion_helper_.GetMarkedColumns());
  lp->DeleteRows(row_deletion_helper_.GetMarkedRows());
  return !column_deletion_helper_.IsEmpty();
}

void DoubletonEqualityRowPreprocessor::RecoverSolution(
    ProblemSolution* solution) const {
  RETURN_IF_NULL(solution);
  column_deletion_helper_.RestoreDeletedColumns(solution);
  row_deletion_helper_.RestoreDeletedRows(solution);
  for (const RestoreInfo& r : Reverse(restore_stack_)) {
    switch (solution->variable_statuses[r.col[MODIFIED]]) {
      case VariableStatus::FIXED_VALUE:
        LOG(DFATAL) << "FIXED variable produced by DoubletonPreprocessor!";
        // In non-fastbuild mode, we rely on the rest of the code producing an
        // ProblemStatus::ABNORMAL status here.
        break;
      // When the modified variable is either basic or free, we keep it as is,
      // and simply make the deleted one basic.
      case VariableStatus::FREE:
        FALLTHROUGH_INTENDED;
      case VariableStatus::BASIC:
        // Several code paths set the deleted column as basic. The code that
        // sets its value in that case is below, after the switch() block.
        solution->variable_statuses[r.col[DELETED]] = VariableStatus::BASIC;
        break;
      case VariableStatus::AT_LOWER_BOUND:
        FALLTHROUGH_INTENDED;
      case VariableStatus::AT_UPPER_BOUND: {
        // The bound was induced by a bound of one of the two original
        // variables. Put that original variable at its bound, and make
        // the other one basic.
        const RestoreInfo::ColChoiceAndStatus& bound_backtracking =
            solution->variable_statuses[r.col[MODIFIED]] ==
                    VariableStatus::AT_LOWER_BOUND
                ? r.bound_backtracking_at_lower_bound
                : r.bound_backtracking_at_upper_bound;
        const ColIndex bounded_var = r.col[bound_backtracking.col_choice];
        const ColIndex basic_var =
            r.col[OtherColChoice(bound_backtracking.col_choice)];
        solution->variable_statuses[bounded_var] = bound_backtracking.status;
        solution->primal_values[bounded_var] = bound_backtracking.value;
        solution->variable_statuses[basic_var] = VariableStatus::BASIC;
        // If the modified column is VariableStatus::BASIC, then its value is
        // already set
        // correctly. If it's the deleted column that is basic, its value is
        // set below the switch() block.
      }
    }

    // Restore the value of the deleted column if it is VariableStatus::BASIC.
    if (solution->variable_statuses[r.col[DELETED]] == VariableStatus::BASIC) {
      solution->primal_values[r.col[DELETED]] =
          (r.rhs -
           solution->primal_values[r.col[MODIFIED]] * r.coeff[MODIFIED]) /
          r.coeff[DELETED];
    }

    // Make the deleted constraint status FIXED.
    solution->constraint_statuses[r.row] = ConstraintStatus::FIXED_VALUE;

    // Adjust the dual value of the deleted constraint so that the
    // VariableStatus::BASIC column have a reduced cost of zero (if the two
    // are VariableStatus::BASIC, just pick one).
    // The reduced cost of the other variable will then automatically be
    // correct: zero if it's VariableStatus::BASIC, and with the correct sign if
    // it's bounded.
    const ColChoice col_choice =
        solution->variable_statuses[r.col[DELETED]] == VariableStatus::BASIC
            ? DELETED
            : MODIFIED;
    // To compute the reduced cost properly (i.e. without the restored row).
    solution->dual_values[r.row] = 0.0;
    const Fractional current_reduced_cost =
        r.objective_coefficient[col_choice] -
        PreciseScalarProduct(solution->dual_values, r.column[col_choice]);
    solution->dual_values[r.row] = current_reduced_cost / r.coeff[col_choice];
  }
}

// --------------------------------------------------------
// DualizerPreprocessor
// --------------------------------------------------------

bool DualizerPreprocessor::Run(LinearProgram* lp, TimeLimit* time_limit) {
  RETURN_VALUE_IF_NULL(lp, false);
  if (parameters_.solve_dual_problem() == GlopParameters::NEVER_DO) {
    return false;
  }

  // Store the original problem size and direction.
  primal_num_cols_ = lp->num_variables();
  primal_num_rows_ = lp->num_constraints();
  primal_is_maximization_problem_ = lp->IsMaximizationProblem();

  // If we need to decide wether or not to take the dual, we only take it when
  // the matrix has more rows than columns. The number of rows of a linear
  // program gives the size of the square matrices we need to invert and the
  // order of iterations of the simplex method. So solving a program with less
  // rows is likely a better alternative. Note that the number of row of the
  // dual is the number of column of the primal.
  //
  // Note however that the default is a conservative factor because if the
  // user gives us a primal program, we assume he knows what he is doing and
  // sometimes a problem is a lot faster to solve in a given formulation
  // even if its dimension would say otherwise.
  //
  // Another reason to be conservative, is that the number of columns of the
  // dual is the number of rows of the primal plus up to two times the number of
  // columns of the primal.
  //
  // TODO(user): This effect can be lowered if we use some of the extra
  // variables as slack variable which we are not doing at this point.
  if (parameters_.solve_dual_problem() == GlopParameters::LET_SOLVER_DECIDE) {
    if (1.0 * primal_num_rows_.value() <
        parameters_.dualizer_threshold() * primal_num_cols_.value()) {
      return false;
    }
  }

  // Save the linear program bounds.
  // Also make sure that all the bounded variable have at least one bound set to
  // zero. This will be needed to postsolve a dual-basic solution into a
  // primal-basic one.
  const ColIndex num_cols = lp->num_variables();
  variable_lower_bounds_.assign(num_cols, 0.0);
  variable_upper_bounds_.assign(num_cols, 0.0);
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional lower = lp->variable_lower_bounds()[col];
    const Fractional upper = lp->variable_upper_bounds()[col];

    // We need to shift one of the bound to zero.
    variable_lower_bounds_[col] = lower;
    variable_upper_bounds_[col] = upper;
    const Fractional value = MinInMagnitudeOrZeroIfInfinite(lower, upper);
    if (value != 0.0) {
      lp->SetVariableBounds(col, lower - value, upper - value);
      SubtractColumnMultipleFromConstraintBound(col, value, lp);
    }
  }

  // Fill the information that will be needed during postsolve.
  //
  // TODO(user): This will break if PopulateFromDual() is changed. so document
  // the convention or make the function fill these vectors?
  dual_status_correspondence_.clear();
  for (RowIndex row(0); row < primal_num_rows_; ++row) {
    const Fractional lower_bound = lp->constraint_lower_bounds()[row];
    const Fractional upper_bound = lp->constraint_upper_bounds()[row];
    if (lower_bound == upper_bound) {
      dual_status_correspondence_.push_back(VariableStatus::FIXED_VALUE);
    } else if (upper_bound != kInfinity) {
      dual_status_correspondence_.push_back(VariableStatus::AT_UPPER_BOUND);
    } else if (lower_bound != -kInfinity) {
      dual_status_correspondence_.push_back(VariableStatus::AT_LOWER_BOUND);
    } else {
      LOG(DFATAL) << "There should be no free constraint in this lp.";
    }
  }
  slack_or_surplus_mapping_.clear();
  for (ColIndex col(0); col < primal_num_cols_; ++col) {
    const Fractional lower_bound = lp->variable_lower_bounds()[col];
    const Fractional upper_bound = lp->variable_upper_bounds()[col];
    if (lower_bound != -kInfinity) {
      dual_status_correspondence_.push_back(
          upper_bound == lower_bound ? VariableStatus::FIXED_VALUE
                                     : VariableStatus::AT_LOWER_BOUND);
      slack_or_surplus_mapping_.push_back(col);
    }
  }
  for (ColIndex col(0); col < primal_num_cols_; ++col) {
    const Fractional lower_bound = lp->variable_lower_bounds()[col];
    const Fractional upper_bound = lp->variable_upper_bounds()[col];
    if (upper_bound != kInfinity) {
      dual_status_correspondence_.push_back(
          upper_bound == lower_bound ? VariableStatus::FIXED_VALUE
                                     : VariableStatus::AT_UPPER_BOUND);
      slack_or_surplus_mapping_.push_back(col);
    }
  }

  // TODO(user): There are two different ways to deal with ranged rows when
  // taking the dual. The default way is to duplicate such rows, see
  // PopulateFromDual() for details. Another way is to call
  // lp->AddSlackVariablesForFreeAndBoxedRows() before calling
  // PopulateFromDual(). Adds an option to switch between the two as this may
  // change the running time?
  //
  // Note however that the default algorithm is likely to result in a faster
  // solving time because the dual program will have less rows.
  LinearProgram dual;
  dual.PopulateFromDual(*lp, &duplicated_rows_);
  dual.Swap(lp);
  return true;
}

// Note(user): This assumes that LinearProgram.PopulateFromDual() uses
// the first ColIndex and RowIndex for the rows and columns of the given
// problem.
void DualizerPreprocessor::RecoverSolution(ProblemSolution* solution) const {
  RETURN_IF_NULL(solution);

  DenseRow new_primal_values(primal_num_cols_, 0.0);
  VariableStatusRow new_variable_statuses(primal_num_cols_,
                                          VariableStatus::FREE);
  DCHECK_LE(primal_num_cols_, RowToColIndex(solution->dual_values.size()));
  for (ColIndex col(0); col < primal_num_cols_; ++col) {
    RowIndex row = ColToRowIndex(col);
    const Fractional lower = variable_lower_bounds_[col];
    const Fractional upper = variable_upper_bounds_[col];

    // The new variable value corresponds to the dual value of the dual.
    // The shift applied during presolve needs to be removed.
    const Fractional shift = MinInMagnitudeOrZeroIfInfinite(lower, upper);
    new_primal_values[col] = solution->dual_values[row] + shift;

    // A variable will be VariableStatus::BASIC if the dual constraint is not.
    if (solution->constraint_statuses[row] != ConstraintStatus::BASIC) {
      new_variable_statuses[col] = VariableStatus::BASIC;
    } else {
      // Otherwise, the dual value must be zero, and the variable is at an exact
      // bound or zero if it is VariableStatus::FREE. Note that this works
      // because the bounds
      // are shifted to 0.0 in the presolve!
      DCHECK_EQ(solution->dual_values[row], 0.0);
      new_variable_statuses[col] = ComputeVariableStatus(shift, lower, upper);
    }
  }

  // A basic variable that corresponds to slack/surplus variable is the same as
  // a basic row. The new variable status (that was just set to
  // VariableStatus::BASIC above)
  // needs to be corrected and depends on the variable type (slack/surplus).
  const ColIndex begin = RowToColIndex(primal_num_rows_);
  const ColIndex end = dual_status_correspondence_.size();
  DCHECK_GE(solution->variable_statuses.size(), end);
  DCHECK_EQ(end - begin, slack_or_surplus_mapping_.size());
  for (ColIndex index(begin); index < end; ++index) {
    if (solution->variable_statuses[index] == VariableStatus::BASIC) {
      const ColIndex col = slack_or_surplus_mapping_[index - begin];
      const VariableStatus status = dual_status_correspondence_[index];

      // The new variable value is set to its exact bound because the dual
      // variable value can be imprecise.
      new_variable_statuses[col] = status;
      if (status == VariableStatus::AT_UPPER_BOUND ||
          status == VariableStatus::FIXED_VALUE) {
        new_primal_values[col] = variable_upper_bounds_[col];
      } else {
        DCHECK_EQ(status, VariableStatus::AT_LOWER_BOUND);
        new_primal_values[col] = variable_lower_bounds_[col];
      }
    }
  }

  // Note the <= in the DCHECK, since we may need to add variables when taking
  // the dual.
  DCHECK_LE(primal_num_rows_, ColToRowIndex(solution->primal_values.size()));
  DenseColumn new_dual_values(primal_num_rows_, 0.0);
  ConstraintStatusColumn new_constraint_statuses(primal_num_rows_,
                                                 ConstraintStatus::FREE);

  // Note that the sign need to be corrected because of the special behavior of
  // PopulateFromDual() on a maximization problem, see the comment in the
  // declaration of PopulateFromDual().
  Fractional sign = primal_is_maximization_problem_ ? -1 : 1;
  for (RowIndex row(0); row < primal_num_rows_; ++row) {
    const ColIndex col = RowToColIndex(row);
    new_dual_values[row] = sign * solution->primal_values[col];

    // A constraint will be ConstraintStatus::BASIC if the dual variable is not.
    if (solution->variable_statuses[col] != VariableStatus::BASIC) {
      new_constraint_statuses[row] = ConstraintStatus::BASIC;
      if (duplicated_rows_[row] != kInvalidCol) {
        if (solution->variable_statuses[duplicated_rows_[row]] ==
            VariableStatus::BASIC) {
          // The duplicated row is always about the lower bound.
          new_constraint_statuses[row] = ConstraintStatus::AT_LOWER_BOUND;
        }
      }
    } else {
      // ConstraintStatus::AT_LOWER_BOUND/ConstraintStatus::AT_UPPER_BOUND/
      // ConstraintStatus::FIXED depend on the type of the constraint at this
      // position.
      new_constraint_statuses[row] =
          VariableToConstraintStatus(dual_status_correspondence_[col]);
    }

    // If the original row was duplicated, we need to take into account the
    // value of the corresponding dual column.
    if (duplicated_rows_[row] != kInvalidCol) {
      new_dual_values[row] +=
          sign * solution->primal_values[duplicated_rows_[row]];
    }

    // Because non-basic variable values are exactly at one of their bounds, a
    // new basic constraint will have a dual value exactly equal to zero.
    DCHECK(new_dual_values[row] == 0 ||
           new_constraint_statuses[row] != ConstraintStatus::BASIC);
  }

  solution->status = ChangeStatusToDualStatus(solution->status);
  new_primal_values.swap(solution->primal_values);
  new_dual_values.swap(solution->dual_values);
  new_variable_statuses.swap(solution->variable_statuses);
  new_constraint_statuses.swap(solution->constraint_statuses);
}

ProblemStatus DualizerPreprocessor::ChangeStatusToDualStatus(
    ProblemStatus status) const {
  switch (status) {
    case ProblemStatus::PRIMAL_INFEASIBLE:
      return ProblemStatus::DUAL_INFEASIBLE;
    case ProblemStatus::DUAL_INFEASIBLE:
      return ProblemStatus::PRIMAL_INFEASIBLE;
    case ProblemStatus::PRIMAL_UNBOUNDED:
      return ProblemStatus::DUAL_UNBOUNDED;
    case ProblemStatus::DUAL_UNBOUNDED:
      return ProblemStatus::PRIMAL_UNBOUNDED;
    case ProblemStatus::PRIMAL_FEASIBLE:
      return ProblemStatus::DUAL_FEASIBLE;
    case ProblemStatus::DUAL_FEASIBLE:
      return ProblemStatus::PRIMAL_FEASIBLE;
    default:
      return status;
  }
}

// --------------------------------------------------------
// ShiftVariableBoundsPreprocessor
// --------------------------------------------------------

bool ShiftVariableBoundsPreprocessor::Run(LinearProgram* lp,
                                          TimeLimit* time_limit) {
  RETURN_VALUE_IF_NULL(lp, false);

  // Save the linear program bounds before shifting them.
  bool all_variable_domains_contain_zero = true;
  const ColIndex num_cols = lp->num_variables();
  variable_initial_lbs_.assign(num_cols, 0.0);
  variable_initial_ubs_.assign(num_cols, 0.0);
  for (ColIndex col(0); col < num_cols; ++col) {
    variable_initial_lbs_[col] = lp->variable_lower_bounds()[col];
    variable_initial_ubs_[col] = lp->variable_upper_bounds()[col];
    if (0.0 < variable_initial_lbs_[col] || 0.0 > variable_initial_ubs_[col]) {
      all_variable_domains_contain_zero = false;
    }
  }
  VLOG(1) << "Maximum variable bounds magnitude (before shift): "
          << ComputeMaxVariableBoundsMagnitude(*lp);

  // Abort early if there is nothing to do.
  if (all_variable_domains_contain_zero) return false;

  // Shift the variable bounds and compute the changes to the constraint bounds
  // and objective offset in a precise way.
  int num_bound_shifts = 0;
  const RowIndex num_rows = lp->num_constraints();
  KahanSum objective_offset;
  ITIVector<RowIndex, KahanSum> row_offsets(num_rows.value());
  offsets_.assign(num_cols, 0.0);
  for (ColIndex col(0); col < num_cols; ++col) {
    if (0.0 < variable_initial_lbs_[col] || 0.0 > variable_initial_ubs_[col]) {
      Fractional offset = MinInMagnitudeOrZeroIfInfinite(
          variable_initial_lbs_[col], variable_initial_ubs_[col]);
      if (in_mip_context_ && lp->is_variable_integer()[col]) {
        // In the integer case, we truncate the number because if for instance
        // the lower bound is a positive integer + epsilon, we only want to
        // shift by the integer and leave the lower bound at epsilon.
        //
        // TODO(user): This would not be needed, if we always make the bound
        // of an integer variable integer before applying this preprocessor.
        offset = trunc(offset);
      } else {
        DCHECK_NE(offset, 0.0);
      }
      offsets_[col] = offset;
      lp->SetVariableBounds(col, variable_initial_lbs_[col] - offset,
                            variable_initial_ubs_[col] - offset);
      const SparseColumn& sparse_column = lp->GetSparseColumn(col);
      for (const SparseColumn::Entry e : sparse_column) {
        row_offsets[e.row()].Add(e.coefficient() * offset);
      }
      objective_offset.Add(lp->objective_coefficients()[col] * offset);
      ++num_bound_shifts;
    }
  }
  VLOG(1) << "Maximum variable bounds magnitude (after " << num_bound_shifts
          << " shifts): " << ComputeMaxVariableBoundsMagnitude(*lp);

  // Apply the changes to the constraint bound and objective offset.
  for (RowIndex row(0); row < num_rows; ++row) {
    lp->SetConstraintBounds(
        row, lp->constraint_lower_bounds()[row] - row_offsets[row].Value(),
        lp->constraint_upper_bounds()[row] - row_offsets[row].Value());
  }
  lp->SetObjectiveOffset(lp->objective_offset() + objective_offset.Value());
  return true;
}

void ShiftVariableBoundsPreprocessor::RecoverSolution(
    ProblemSolution* solution) const {
  RETURN_IF_NULL(solution);
  const ColIndex num_cols = solution->variable_statuses.size();
  for (ColIndex col(0); col < num_cols; ++col) {
    if (in_mip_context_) {
      solution->primal_values[col] += offsets_[col];
    } else {
      switch (solution->variable_statuses[col]) {
        case VariableStatus::FIXED_VALUE:
          FALLTHROUGH_INTENDED;
        case VariableStatus::AT_LOWER_BOUND:
          solution->primal_values[col] = variable_initial_lbs_[col];
          break;
        case VariableStatus::AT_UPPER_BOUND:
          solution->primal_values[col] = variable_initial_ubs_[col];
          break;
        case VariableStatus::BASIC:
          solution->primal_values[col] += offsets_[col];
          break;
        case VariableStatus::FREE:
          break;
      }
    }
  }
}

// --------------------------------------------------------
// ScalingPreprocessor
// --------------------------------------------------------

bool ScalingPreprocessor::Run(LinearProgram* lp, TimeLimit* time_limit) {
  RETURN_VALUE_IF_NULL(lp, false);
  if (!parameters_.use_scaling()) return false;

  // Save the linear program bounds before scaling them.
  const ColIndex num_cols = lp->num_variables();
  variable_lower_bounds_.assign(num_cols, 0.0);
  variable_upper_bounds_.assign(num_cols, 0.0);
  for (ColIndex col(0); col < num_cols; ++col) {
    variable_lower_bounds_[col] = lp->variable_lower_bounds()[col];
    variable_upper_bounds_[col] = lp->variable_upper_bounds()[col];
  }

  lp->Scale(&scaler_);

  // We scale the costs to always have a maximum cost magnitude of 1.0. Note
  // that this makes a lot of sense since internally we use absolute tolerances.
  // We don't want to have a completely different behavior just because the user
  // changed the units in the objective for instance.
  cost_scaling_factor_ = lp->ScaleObjective();
  return true;
}

void ScalingPreprocessor::RecoverSolution(ProblemSolution* solution) const {
  RETURN_IF_NULL(solution);
  scaler_.ScaleRowVector(false, &(solution->primal_values));
  scaler_.ScaleColumnVector(false, &(solution->dual_values));
  for (RowIndex row(0); row < solution->dual_values.size(); ++row) {
    solution->dual_values[row] *= cost_scaling_factor_;
  }

  // Make sure the variable are at they exact bounds according to their status.
  // This just remove a really low error (about 1e-15) but allows to keep the
  // variables at their exact bounds.
  const ColIndex num_cols = solution->primal_values.size();
  for (ColIndex col(0); col < num_cols; ++col) {
    switch (solution->variable_statuses[col]) {
      case VariableStatus::AT_UPPER_BOUND:
        FALLTHROUGH_INTENDED;
      case VariableStatus::FIXED_VALUE:
        solution->primal_values[col] = variable_upper_bounds_[col];
        break;
      case VariableStatus::AT_LOWER_BOUND:
        solution->primal_values[col] = variable_lower_bounds_[col];
        break;
      case VariableStatus::FREE:
        FALLTHROUGH_INTENDED;
      case VariableStatus::BASIC:
        break;
    }
  }
}

// --------------------------------------------------------
// ToMinimizationPreprocessor
// --------------------------------------------------------

bool ToMinimizationPreprocessor::Run(LinearProgram* lp, TimeLimit* time_limit) {
  RETURN_VALUE_IF_NULL(lp, false);
  if (lp->IsMaximizationProblem()) {
    for (ColIndex col(0); col < lp->num_variables(); ++col) {
      const Fractional coeff = lp->objective_coefficients()[col];
      if (coeff != 0.0) {
        lp->SetObjectiveCoefficient(col, -coeff);
      }
    }
    lp->SetMaximizationProblem(false);
    lp->SetObjectiveOffset(-lp->objective_offset());
    lp->SetObjectiveScalingFactor(-lp->objective_scaling_factor());
  }
  return false;
}

void ToMinimizationPreprocessor::RecoverSolution(
    ProblemSolution* solution) const {}

// --------------------------------------------------------
// AddSlackVariablesPreprocessor
// --------------------------------------------------------

bool AddSlackVariablesPreprocessor::Run(LinearProgram* lp,
                                        TimeLimit* time_limit) {
  RETURN_VALUE_IF_NULL(lp, false);
  lp->AddSlackVariablesWhereNecessary(/*detect_integer_constraints=*/true);
  first_slack_col_ = lp->GetFirstSlackVariable();
  return true;
}

void AddSlackVariablesPreprocessor::RecoverSolution(
    ProblemSolution* solution) const {
  RETURN_IF_NULL(solution);

  // Compute constraint statuses from statuses of slack variables.
  const RowIndex num_rows = solution->dual_values.size();
  for (RowIndex row(0); row < num_rows; ++row) {
    const ColIndex slack_col = first_slack_col_ + RowToColIndex(row);
    const VariableStatus variable_status =
        solution->variable_statuses[slack_col];
    ConstraintStatus constraint_status = ConstraintStatus::FREE;
    // The slack variables have reversed bounds - if the value of the variable
    // is at one bound, the value of the constraint is at the opposite bound.
    switch (variable_status) {
      case VariableStatus::AT_LOWER_BOUND:
        constraint_status = ConstraintStatus::AT_UPPER_BOUND;
        break;
      case VariableStatus::AT_UPPER_BOUND:
        constraint_status = ConstraintStatus::AT_LOWER_BOUND;
        break;
      default:
        constraint_status = VariableToConstraintStatus(variable_status);
        break;
    }
    solution->constraint_statuses[row] = constraint_status;
  }

  // Drop the primal values and variable statuses for slack variables.
  solution->primal_values.resize(first_slack_col_, 0.0);
  solution->variable_statuses.resize(first_slack_col_, VariableStatus::FREE);
}

// --------------------------------------------------------
// SolowHalimPreprocessor
// --------------------------------------------------------
bool SolowHalimPreprocessor::Run(LinearProgram* lp, TimeLimit* time_limit) {
  RETURN_VALUE_IF_NULL(lp, false);
  if (!parameters_.use_solowhalim()){
    return false;
  }

  // mip context not implemented
  // TODO : in order to manage mip context we must take care
  // of truncated offsets
  if (in_mip_context_){
    return false;
  }

  const bool lp_is_maximization_problem = lp->IsMaximizationProblem();
  const ColIndex num_cols = lp->num_variables();
  const RowIndex num_rows = lp->num_constraints();
  ColIndex num_shifted_cols(0);
  ColIndex num_shifted_opposite_cols(0);

  variable_initial_lbs_.assign(num_cols, 0.0);
  variable_initial_ubs_.assign(num_cols, 0.0);
  for (ColIndex col(0); col < num_cols; ++col) {
    variable_initial_lbs_[col] = lp->variable_lower_bounds()[col];
    variable_initial_ubs_[col] = lp->variable_upper_bounds()[col];
  }

  KahanSum objective_offset;
  ITIVector<RowIndex, KahanSum> row_offsets(num_rows.value());
  column_transform_.resize(num_cols.value(), NOT_MODIFIED);
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional coeff = lp->objective_coefficients()[col];
    if (coeff != 0.0){
      bool coeff_opposite_direction =
          ( (coeff < 0.0 && !lp_is_maximization_problem) ||
            (coeff > 0.0 &&  lp_is_maximization_problem));

      const Fractional ub = lp->variable_upper_bounds()[col];
      const Fractional lb = lp->variable_lower_bounds()[col];
      if (IsFinite(ub) && IsFinite(lb)){
        ColumnTransformType column_transform = NOT_MODIFIED;
        double shift_value = 0.0;

        if (coeff_opposite_direction) {
          SparseColumn* mutable_sparse_column = lp->GetMutableSparseColumn(col);
          for (const SparseColumn::Entry e : (*mutable_sparse_column) ) {
            row_offsets[e.row()].Add(e.coefficient() * ub);
          }
          mutable_sparse_column->MultiplyByConstant(-1);
          lp->SetObjectiveCoefficient(col, -coeff);
          shift_value = ub;
          column_transform = SHIFTED_OPPOSITE_DIRECTION;
          num_shifted_opposite_cols++;
        } else {
          const SparseColumn& sparse_column = lp->GetSparseColumn(col);
          for (const SparseColumn::Entry e : sparse_column) {
            row_offsets[e.row()].Add(e.coefficient() * lb);
          }
          shift_value = lb;
          column_transform = SHIFTED;
          num_shifted_cols++;
        }

        if (column_transform != NOT_MODIFIED){
          column_transform_[col] = column_transform;
          objective_offset.Add(coeff * shift_value);
          lp->SetVariableBounds(col, 0, ub - lb);
        }
      }
    }
  }

  for (RowIndex row(0); row < num_rows; ++row) {
    lp->SetConstraintBounds(
        row, lp->constraint_lower_bounds()[row] - row_offsets[row].Value(),
        lp->constraint_upper_bounds()[row] - row_offsets[row].Value());
  }
  lp->SetObjectiveOffset(lp->objective_offset() + objective_offset.Value());

  VLOG(1) << "Shifted " << num_shifted_cols << " variables.";
  VLOG(1) << "Shifted opposite " << num_shifted_opposite_cols << " variables.";
  VLOG(1) << "Objective offset : " << objective_offset.Value();

  return true;
}

void SolowHalimPreprocessor::RecoverSolution(ProblemSolution* solution) const {
  RETURN_IF_NULL(solution);
  const ColIndex num_cols = solution->variable_statuses.size();
  for (ColIndex col(0); col < num_cols; ++col) {

    VLOG(2) << "col = " << col << "\t" << column_transform_[col];
    VLOG(2) << "\tinitial range : \t [" <<  variable_initial_lbs_[col]
            << " ; " << variable_initial_ubs_[col] << "]";
    VLOG(2) << "\tstatus : " <<  solution->variable_statuses[col]
            << "\t raw value : " << solution->primal_values[col];

    switch (column_transform_[col]){
      case NOT_MODIFIED:
        break;
      case SHIFTED:
        switch (solution->variable_statuses[col]) {
          case VariableStatus::AT_LOWER_BOUND:
            solution->primal_values[col] = variable_initial_lbs_[col];
            break;
          case VariableStatus::AT_UPPER_BOUND:
            solution->primal_values[col] = variable_initial_ubs_[col];
            break;
          case VariableStatus::BASIC:
            solution->primal_values[col] =
              variable_initial_lbs_[col] + solution->primal_values[col];
            break;
          case VariableStatus::FIXED_VALUE:
            FALLTHROUGH_INTENDED;
          case VariableStatus::FREE:
            break;
        }
        break;
      case SHIFTED_OPPOSITE_DIRECTION:
        switch (solution->variable_statuses[col]) {
          case VariableStatus::AT_LOWER_BOUND:
            solution->primal_values[col] = variable_initial_ubs_[col];
            solution->variable_statuses[col] = VariableStatus::AT_UPPER_BOUND;
            break;
          case VariableStatus::AT_UPPER_BOUND:
            solution->primal_values[col] = variable_initial_lbs_[col];
            solution->variable_statuses[col] = VariableStatus::AT_LOWER_BOUND;
            break;
          case VariableStatus::BASIC:
            solution->primal_values[col] =
              variable_initial_ubs_[col] - solution->primal_values[col];
            break;
          case VariableStatus::FIXED_VALUE:
            FALLTHROUGH_INTENDED;
          case VariableStatus::FREE:
            break;
        }
        break;
    }
    VLOG(2) << " recover value : " << solution->primal_values[col];

  }
}

}  // namespace glop
}  // namespace operations_research
