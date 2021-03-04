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

  RowIndex num_basic_variables(0);
  const ColIndex num_cols = lower_bounds_.size();
  const RowIndex num_rows = ColToRowIndex(num_cols - first_slack_col);
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
        // Do not allow more than num_rows VariableStatus::BASIC variables.
        if (num_basic_variables == num_rows) {
          VLOG(1) << "Too many basic variables in the warm-start basis."
                  << "Only keeping the first ones as VariableStatus::BASIC.";
          UpdateToNonBasicStatus(col, DefaultVariableStatus(col));
        } else {
          ++num_basic_variables;
          UpdateToBasicStatus(col);
        }
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

}  // namespace glop
}  // namespace operations_research
