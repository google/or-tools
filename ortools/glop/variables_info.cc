// Copyright 2010-2021 Google LLC
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

#include "ortools/glop/variables_info.h"

namespace operations_research {
namespace glop {

VariablesInfo::VariablesInfo(const CompactSparseMatrix& matrix)
    : matrix_(matrix) {}

bool VariablesInfo::LoadBoundsAndReturnTrueIfUnchanged(
    const DenseRow& new_lower_bounds, const DenseRow& new_upper_bounds) {
  const ColIndex num_cols = matrix_.num_cols();
  DCHECK_EQ(num_cols, new_lower_bounds.size());
  DCHECK_EQ(num_cols, new_upper_bounds.size());

  // Optim if nothing changed.
  if (lower_bounds_ == new_lower_bounds && upper_bounds_ == new_upper_bounds) {
    return true;
  }

  lower_bounds_ = new_lower_bounds;
  upper_bounds_ = new_upper_bounds;
  variable_type_.resize(num_cols, VariableType::UNCONSTRAINED);
  for (ColIndex col(0); col < num_cols; ++col) {
    variable_type_[col] = ComputeVariableType(col);
  }
  return false;
}

bool VariablesInfo::LoadBoundsAndReturnTrueIfUnchanged(
    const DenseRow& variable_lower_bounds,
    const DenseRow& variable_upper_bounds,
    const DenseColumn& constraint_lower_bounds,
    const DenseColumn& constraint_upper_bounds) {
  const ColIndex num_cols = matrix_.num_cols();
  const ColIndex num_variables = variable_upper_bounds.size();
  const RowIndex num_rows = constraint_lower_bounds.size();

  bool is_unchanged = (num_cols == lower_bounds_.size());
  DCHECK_EQ(num_cols, num_variables + RowToColIndex(num_rows));
  lower_bounds_.resize(num_cols, 0.0);
  upper_bounds_.resize(num_cols, 0.0);
  variable_type_.resize(num_cols, VariableType::FIXED_VARIABLE);

  // Copy bounds of the variables.
  for (ColIndex col(0); col < num_variables; ++col) {
    if (lower_bounds_[col] != variable_lower_bounds[col] ||
        upper_bounds_[col] != variable_upper_bounds[col]) {
      lower_bounds_[col] = variable_lower_bounds[col];
      upper_bounds_[col] = variable_upper_bounds[col];
      is_unchanged = false;
      variable_type_[col] = ComputeVariableType(col);
    }
  }

  // Copy bounds of the slack.
  for (RowIndex row(0); row < num_rows; ++row) {
    const ColIndex col = num_variables + RowToColIndex(row);
    if (lower_bounds_[col] != -constraint_upper_bounds[row] ||
        upper_bounds_[col] != -constraint_lower_bounds[row]) {
      lower_bounds_[col] = -constraint_upper_bounds[row];
      upper_bounds_[col] = -constraint_lower_bounds[row];
      is_unchanged = false;
      variable_type_[col] = ComputeVariableType(col);
    }
  }

  return is_unchanged;
}

void VariablesInfo::ResetStatusInfo() {
  const ColIndex num_cols = matrix_.num_cols();
  DCHECK_EQ(num_cols, lower_bounds_.size());
  DCHECK_EQ(num_cols, upper_bounds_.size());

  // TODO(user): These could just be Resized() but there is a bug with the
  // iteration and resize it seems. Investigate. I suspect the last bucket
  // is not cleared so you can still iterate on the ones there even if it all
  // positions before num_cols are set to zero.
  variable_status_.resize(num_cols, VariableStatus::FREE);
  can_increase_.ClearAndResize(num_cols);
  can_decrease_.ClearAndResize(num_cols);
  is_basic_.ClearAndResize(num_cols);
  not_basic_.ClearAndResize(num_cols);
  non_basic_boxed_variables_.ClearAndResize(num_cols);

  // This one cannot just be resized.
  boxed_variables_are_relevant_ = true;
  num_entries_in_relevant_columns_ = 0;
  relevance_.ClearAndResize(num_cols);
}

void VariablesInfo::InitializeFromBasisState(ColIndex first_slack_col,
                                             ColIndex num_new_cols,
                                             const BasisState& state) {
  ResetStatusInfo();

  const ColIndex num_cols = lower_bounds_.size();
  DCHECK_LE(num_new_cols, first_slack_col);
  const ColIndex first_new_col(first_slack_col - num_new_cols);

  // Compute the status for all the columns (note that the slack variables are
  // already added at the end of the matrix at this stage).
  for (ColIndex col(0); col < num_cols; ++col) {
    // Start with the given "warm" status from the BasisState if it exists.
    VariableStatus status;
    if (col < first_new_col && col < state.statuses.size()) {
      status = state.statuses[col];
    } else if (col >= first_slack_col &&
               col - num_new_cols < state.statuses.size()) {
      status = state.statuses[col - num_new_cols];
    } else {
      UpdateToNonBasicStatus(col, DefaultVariableStatus(col));
      continue;
    }

    // Remove incompatibilities between the warm status and the current state.
    switch (status) {
      case VariableStatus::BASIC:
        // Because we just called ResetStatusInfo(), we optimize the call to
        // UpdateToNonBasicStatus(col) here. In an incremental setting with
        // almost no work per call, the update of all the DenseBitRow are
        // visible.
        variable_status_[col] = VariableStatus::BASIC;
        is_basic_.Set(col, true);
        break;
      case VariableStatus::AT_LOWER_BOUND:
        if (lower_bounds_[col] == upper_bounds_[col]) {
          UpdateToNonBasicStatus(col, VariableStatus::FIXED_VALUE);
        } else {
          UpdateToNonBasicStatus(col, lower_bounds_[col] == -kInfinity
                                          ? DefaultVariableStatus(col)
                                          : status);
        }
        break;
      case VariableStatus::AT_UPPER_BOUND:
        if (lower_bounds_[col] == upper_bounds_[col]) {
          UpdateToNonBasicStatus(col, VariableStatus::FIXED_VALUE);
        } else {
          UpdateToNonBasicStatus(col, upper_bounds_[col] == kInfinity
                                          ? DefaultVariableStatus(col)
                                          : status);
        }
        break;
      default:
        UpdateToNonBasicStatus(col, DefaultVariableStatus(col));
    }
  }
}

int VariablesInfo::ChangeUnusedBasicVariablesToFree(
    const RowToColMapping& basis) {
  const ColIndex num_cols = lower_bounds_.size();
  is_basic_.ClearAndResize(num_cols);
  for (const ColIndex col : basis) {
    UpdateToBasicStatus(col);
  }
  int num_no_longer_in_basis = 0;
  for (ColIndex col(0); col < num_cols; ++col) {
    if (!is_basic_[col] && variable_status_[col] == VariableStatus::BASIC) {
      ++num_no_longer_in_basis;
      if (variable_type_[col] == VariableType::FIXED_VARIABLE) {
        UpdateToNonBasicStatus(col, VariableStatus::FIXED_VALUE);
      } else {
        UpdateToNonBasicStatus(col, VariableStatus::FREE);
      }
    }
  }
  return num_no_longer_in_basis;
}

int VariablesInfo::SnapFreeVariablesToBound(Fractional distance,
                                            const DenseRow& starting_values) {
  int num_changes = 0;
  const ColIndex num_cols = lower_bounds_.size();
  for (ColIndex col(0); col < num_cols; ++col) {
    if (variable_status_[col] != VariableStatus::FREE) continue;
    if (variable_type_[col] == VariableType::UNCONSTRAINED) continue;
    const Fractional value =
        col < starting_values.size() ? starting_values[col] : 0.0;
    const Fractional diff_ub = upper_bounds_[col] - value;
    const Fractional diff_lb = value - lower_bounds_[col];
    if (diff_lb <= diff_ub) {
      if (diff_lb <= distance) {
        ++num_changes;
        UpdateToNonBasicStatus(col, VariableStatus::AT_LOWER_BOUND);
      }
    } else {
      if (diff_ub <= distance) {
        ++num_changes;
        UpdateToNonBasicStatus(col, VariableStatus::AT_UPPER_BOUND);
      }
    }
  }
  return num_changes;
}

void VariablesInfo::InitializeToDefaultStatus() {
  ResetStatusInfo();
  const ColIndex num_cols = lower_bounds_.size();
  for (ColIndex col(0); col < num_cols; ++col) {
    UpdateToNonBasicStatus(col, DefaultVariableStatus(col));
  }
}

VariableStatus VariablesInfo::DefaultVariableStatus(ColIndex col) const {
  DCHECK_GE(col, 0);
  DCHECK_LT(col, lower_bounds_.size());
  if (lower_bounds_[col] == upper_bounds_[col]) {
    return VariableStatus::FIXED_VALUE;
  }
  if (lower_bounds_[col] == -kInfinity && upper_bounds_[col] == kInfinity) {
    return VariableStatus::FREE;
  }

  // Returns the bound with the lowest magnitude. Note that it must be finite
  // because the VariableStatus::FREE case was tested earlier.
  DCHECK(IsFinite(lower_bounds_[col]) || IsFinite(upper_bounds_[col]));
  return std::abs(lower_bounds_[col]) <= std::abs(upper_bounds_[col])
             ? VariableStatus::AT_LOWER_BOUND
             : VariableStatus::AT_UPPER_BOUND;
}

void VariablesInfo::MakeBoxedVariableRelevant(bool value) {
  if (value == boxed_variables_are_relevant_) return;
  boxed_variables_are_relevant_ = value;
  if (value) {
    for (const ColIndex col : non_basic_boxed_variables_) {
      SetRelevance(col, variable_type_[col] != VariableType::FIXED_VARIABLE);
    }
  } else {
    for (const ColIndex col : non_basic_boxed_variables_) {
      SetRelevance(col, false);
    }
  }
}

void VariablesInfo::UpdateToBasicStatus(ColIndex col) {
  if (in_dual_phase_one_) {
    // TODO(user): A bit annoying that we need to test this even if we
    // don't use the dual. But the cost is minimal.
    if (lower_bounds_[col] != 0.0) lower_bounds_[col] = -kInfinity;
    if (upper_bounds_[col] != 0.0) upper_bounds_[col] = +kInfinity;
    variable_type_[col] = ComputeVariableType(col);
  }
  variable_status_[col] = VariableStatus::BASIC;
  is_basic_.Set(col, true);
  not_basic_.Set(col, false);
  can_increase_.Set(col, false);
  can_decrease_.Set(col, false);
  non_basic_boxed_variables_.Set(col, false);
  SetRelevance(col, false);
}

void VariablesInfo::UpdateToNonBasicStatus(ColIndex col,
                                           VariableStatus status) {
  DCHECK_NE(status, VariableStatus::BASIC);
  variable_status_[col] = status;
  is_basic_.Set(col, false);
  not_basic_.Set(col, true);
  can_increase_.Set(col, status == VariableStatus::AT_LOWER_BOUND ||
                             status == VariableStatus::FREE);
  can_decrease_.Set(col, status == VariableStatus::AT_UPPER_BOUND ||
                             status == VariableStatus::FREE);

  const bool boxed =
      variable_type_[col] == VariableType::UPPER_AND_LOWER_BOUNDED;
  non_basic_boxed_variables_.Set(col, boxed);
  const bool relevance = status != VariableStatus::FIXED_VALUE &&
                         (boxed_variables_are_relevant_ || !boxed);
  SetRelevance(col, relevance);
}

const VariableTypeRow& VariablesInfo::GetTypeRow() const {
  return variable_type_;
}

const VariableStatusRow& VariablesInfo::GetStatusRow() const {
  return variable_status_;
}

const DenseBitRow& VariablesInfo::GetCanIncreaseBitRow() const {
  return can_increase_;
}

const DenseBitRow& VariablesInfo::GetCanDecreaseBitRow() const {
  return can_decrease_;
}

const DenseBitRow& VariablesInfo::GetIsRelevantBitRow() const {
  return relevance_;
}

const DenseBitRow& VariablesInfo::GetIsBasicBitRow() const { return is_basic_; }

const DenseBitRow& VariablesInfo::GetNotBasicBitRow() const {
  return not_basic_;
}

const DenseBitRow& VariablesInfo::GetNonBasicBoxedVariables() const {
  return non_basic_boxed_variables_;
}

EntryIndex VariablesInfo::GetNumEntriesInRelevantColumns() const {
  return num_entries_in_relevant_columns_;
}

VariableType VariablesInfo::ComputeVariableType(ColIndex col) const {
  DCHECK_LE(lower_bounds_[col], upper_bounds_[col]);
  if (lower_bounds_[col] == -kInfinity) {
    if (upper_bounds_[col] == kInfinity) {
      return VariableType::UNCONSTRAINED;
    }
    return VariableType::UPPER_BOUNDED;
  } else if (upper_bounds_[col] == kInfinity) {
    return VariableType::LOWER_BOUNDED;
  } else if (lower_bounds_[col] == upper_bounds_[col]) {
    return VariableType::FIXED_VARIABLE;
  } else {
    return VariableType::UPPER_AND_LOWER_BOUNDED;
  }
}

void VariablesInfo::SetRelevance(ColIndex col, bool relevance) {
  if (relevance_.IsSet(col) == relevance) return;
  if (relevance) {
    relevance_.Set(col);
    num_entries_in_relevant_columns_ += matrix_.ColumnNumEntries(col);
  } else {
    relevance_.Clear(col);
    num_entries_in_relevant_columns_ -= matrix_.ColumnNumEntries(col);
  }
}

// This is really similar to InitializeFromBasisState() but there is less
// cases to consider for TransformToDualPhaseIProblem()/EndDualPhaseI().
void VariablesInfo::UpdateStatusForNewType(ColIndex col) {
  switch (variable_status_[col]) {
    case VariableStatus::BASIC:
      UpdateToBasicStatus(col);
      break;
    case VariableStatus::AT_LOWER_BOUND:
      if (lower_bounds_[col] == upper_bounds_[col]) {
        UpdateToNonBasicStatus(col, VariableStatus::FIXED_VALUE);
      } else if (lower_bounds_[col] == -kInfinity) {
        UpdateToNonBasicStatus(col, DefaultVariableStatus(col));
      } else {
        // TODO(user): This is only needed for boxed variable to update their
        // relevance. It should probably be done with the type and not the
        // status update.
        UpdateToNonBasicStatus(col, variable_status_[col]);
      }
      break;
    case VariableStatus::AT_UPPER_BOUND:
      if (lower_bounds_[col] == upper_bounds_[col]) {
        UpdateToNonBasicStatus(col, VariableStatus::FIXED_VALUE);
      } else if (upper_bounds_[col] == kInfinity) {
        UpdateToNonBasicStatus(col, DefaultVariableStatus(col));
      } else {
        // TODO(user): Same as in the AT_LOWER_BOUND branch above.
        UpdateToNonBasicStatus(col, variable_status_[col]);
      }
      break;
    default:
      // TODO(user): boxed variable that become fixed in
      // TransformToDualPhaseIProblem() will be changed status twice. Once here,
      // and once when we make them dual feasible according to their reduced
      // cost. We should probably just do all at once.
      UpdateToNonBasicStatus(col, DefaultVariableStatus(col));
  }
}

void VariablesInfo::TransformToDualPhaseIProblem(
    Fractional dual_feasibility_tolerance, const DenseRow& reduced_costs) {
  DCHECK(!in_dual_phase_one_);
  in_dual_phase_one_ = true;
  saved_lower_bounds_ = lower_bounds_;
  saved_upper_bounds_ = upper_bounds_;

  // Transform the bound and type to get a new problem. If this problem has an
  // optimal value of 0.0, then the problem is dual feasible. And more
  // importantly, by keeping the same basis, we have a feasible solution of the
  // original problem.
  const ColIndex num_cols = matrix_.num_cols();
  for (ColIndex col(0); col < num_cols; ++col) {
    switch (variable_type_[col]) {
      case VariableType::FIXED_VARIABLE:  // ABSL_FALLTHROUGH_INTENDED
      case VariableType::UPPER_AND_LOWER_BOUNDED:
        lower_bounds_[col] = 0.0;
        upper_bounds_[col] = 0.0;
        variable_type_[col] = VariableType::FIXED_VARIABLE;
        break;
      case VariableType::LOWER_BOUNDED:
        lower_bounds_[col] = 0.0;
        upper_bounds_[col] = 1.0;
        variable_type_[col] = VariableType::UPPER_AND_LOWER_BOUNDED;
        break;
      case VariableType::UPPER_BOUNDED:
        lower_bounds_[col] = -1.0;
        upper_bounds_[col] = 0.0;
        variable_type_[col] = VariableType::UPPER_AND_LOWER_BOUNDED;
        break;
      case VariableType::UNCONSTRAINED:
        lower_bounds_[col] = -1000.0;
        upper_bounds_[col] = 1000.0;
        variable_type_[col] = VariableType::UPPER_AND_LOWER_BOUNDED;
        break;
    }

    // Make sure we start with a feasible dual solution.
    // If the reduced cost is close to zero, we keep the "default" status.
    if (variable_type_[col] == VariableType::UPPER_AND_LOWER_BOUNDED) {
      if (reduced_costs[col] > dual_feasibility_tolerance) {
        variable_status_[col] = VariableStatus::AT_LOWER_BOUND;
      } else if (reduced_costs[col] < -dual_feasibility_tolerance) {
        variable_status_[col] = VariableStatus::AT_UPPER_BOUND;
      }
    }

    UpdateStatusForNewType(col);
  }
}

void VariablesInfo::EndDualPhaseI(Fractional dual_feasibility_tolerance,
                                  const DenseRow& reduced_costs) {
  DCHECK(in_dual_phase_one_);
  in_dual_phase_one_ = false;
  std::swap(saved_lower_bounds_, lower_bounds_);
  std::swap(saved_upper_bounds_, upper_bounds_);

  // This is to clear the memory of the saved bounds since it is no longer
  // needed.
  DenseRow empty1, empty2;
  std::swap(empty1, saved_lower_bounds_);
  std::swap(empty1, saved_upper_bounds_);

  // Restore the type and update all other fields.
  const ColIndex num_cols = matrix_.num_cols();
  for (ColIndex col(0); col < num_cols; ++col) {
    variable_type_[col] = ComputeVariableType(col);

    // We make sure that the old fixed variables that are now boxed are dual
    // feasible.
    //
    // TODO(user): When there is a choice, use the previous status that might
    // have been warm-started ? but then this is not high priority since
    // warm-starting with a non-dual feasible basis seems unfrequent.
    if (variable_type_[col] == VariableType::UPPER_AND_LOWER_BOUNDED) {
      if (reduced_costs[col] > dual_feasibility_tolerance) {
        variable_status_[col] = VariableStatus::AT_LOWER_BOUND;
      } else if (reduced_costs[col] < -dual_feasibility_tolerance) {
        variable_status_[col] = VariableStatus::AT_UPPER_BOUND;
      }
    }

    UpdateStatusForNewType(col);
  }
}

}  // namespace glop
}  // namespace operations_research
