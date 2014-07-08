#include "glop/variables_info.h"

namespace operations_research {
namespace glop {

VariablesInfo::VariablesInfo(const CompactSparseMatrix& matrix,
                             const DenseRow& lower_bound,
                             const DenseRow& upper_bound)
    : matrix_(matrix),
      lower_bound_(lower_bound),
      upper_bound_(upper_bound),
      boxed_variables_are_relevant_(true) {}

void VariablesInfo::Initialize() {
  boxed_variables_are_relevant_ = true;
  const ColIndex num_cols = matrix_.num_cols();
  num_entries_in_relevant_columns_ = 0;
  variable_type_.resize(num_cols, VariableType::UNCONSTRAINED);
  variable_status_.resize(num_cols, VariableStatus::FREE);
  can_increase_.ClearAndResize(num_cols);
  can_decrease_.ClearAndResize(num_cols);
  is_relevant_.ClearAndResize(num_cols);
  is_basic_.ClearAndResize(num_cols);
  not_basic_.ClearAndResize(num_cols);
  non_basic_boxed_variables_.ClearAndResize(num_cols);
}

void VariablesInfo::MakeBoxedVariableRelevant(bool value) {
  if (value == boxed_variables_are_relevant_) return;
  boxed_variables_are_relevant_ = value;
  for (ColIndex col(0); col < is_relevant_.size(); ++col) {
    SetRelevance(col, variable_status_[col]);
  }
}

void VariablesInfo::Update(ColIndex col, VariableStatus status) {
  variable_type_[col] = ComputeVariableType(col);
  variable_status_[col] = status;
  is_basic_.Set(col, status == VariableStatus::BASIC);
  not_basic_.Set(col, status != VariableStatus::BASIC);
  can_increase_.Set(col, status == VariableStatus::AT_LOWER_BOUND ||
                         status == VariableStatus::FREE);
  can_decrease_.Set(col, status == VariableStatus::AT_UPPER_BOUND ||
                         status == VariableStatus::FREE);
  SetRelevance(col, status);
  non_basic_boxed_variables_.Set(col, status != VariableStatus::BASIC &&
      variable_type_[col] == VariableType::UPPER_AND_LOWER_BOUNDED);
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
  return is_relevant_;
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
  if (lower_bound_[col] == -kInfinity && upper_bound_[col] == kInfinity) {
    return VariableType::UNCONSTRAINED;
  } else if (lower_bound_[col] == -kInfinity) {
    return VariableType::UPPER_BOUNDED;
  } else if (upper_bound_[col] == kInfinity) {
    return VariableType::LOWER_BOUNDED;
  } else if (lower_bound_[col] == upper_bound_[col]) {
    return VariableType::FIXED_VARIABLE;
  } else {
    return VariableType::UPPER_AND_LOWER_BOUNDED;
  }
}

void VariablesInfo::SetRelevance(ColIndex col, VariableStatus status) {
  const bool is_relevant = (status != VariableStatus::BASIC &&
      status != VariableStatus::FIXED_VALUE &&
      (boxed_variables_are_relevant_ ||
      variable_type_[col] != VariableType::UPPER_AND_LOWER_BOUNDED));
  if (is_relevant_.IsSet(col) == is_relevant) return;
  if (is_relevant) {
    is_relevant_.Set(col);
    num_entries_in_relevant_columns_ += matrix_.ColumnNumEntries(col);
  } else {
    is_relevant_.Clear(col);
    num_entries_in_relevant_columns_ -= matrix_.ColumnNumEntries(col);
  }
}

}  // namespace glop
}  // namespace operations_research
