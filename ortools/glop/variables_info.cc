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

#include "ortools/glop/variables_info.h"

namespace operations_research {
namespace glop {

VariablesInfo::VariablesInfo(const CompactSparseMatrix& matrix,
                             const DenseRow& lower_bound,
                             const DenseRow& upper_bound)
    : matrix_(matrix),
      lower_bound_(lower_bound),
      upper_bound_(upper_bound),
      boxed_variables_are_relevant_(true) {}

void VariablesInfo::InitializeAndComputeType() {
  const ColIndex num_cols = matrix_.num_cols();
  can_increase_.ClearAndResize(num_cols);
  can_decrease_.ClearAndResize(num_cols);
  is_basic_.ClearAndResize(num_cols);
  not_basic_.ClearAndResize(num_cols);
  non_basic_boxed_variables_.ClearAndResize(num_cols);

  num_entries_in_relevant_columns_ = 0;
  boxed_variables_are_relevant_ = true;
  relevance_.ClearAndResize(num_cols);

  variable_status_.resize(num_cols, VariableStatus::FREE);
  variable_type_.resize(num_cols, VariableType::UNCONSTRAINED);
  for (ColIndex col(0); col < num_cols; ++col) {
    variable_type_[col] = ComputeVariableType(col);
  }
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

void VariablesInfo::Update(ColIndex col, VariableStatus status) {
  if (status == VariableStatus::BASIC) {
    UpdateToBasicStatus(col);
  } else {
    UpdateToNonBasicStatus(col, status);
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
  DCHECK_LE(lower_bound_[col], upper_bound_[col]);
  if (lower_bound_[col] == -kInfinity) {
    if (upper_bound_[col] == kInfinity) {
      return VariableType::UNCONSTRAINED;
    }
    return VariableType::UPPER_BOUNDED;
  } else if (upper_bound_[col] == kInfinity) {
    return VariableType::LOWER_BOUNDED;
  } else if (lower_bound_[col] == upper_bound_[col]) {
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
