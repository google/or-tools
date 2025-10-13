// Copyright 2010-2025 Google LLC
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

#include "ortools/glop/variable_values.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/types/span.h"
#include "ortools/glop/basis_representation.h"
#include "ortools/glop/dual_edge_norms.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/pricing.h"
#include "ortools/glop/variables_info.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/lp_utils.h"
#include "ortools/lp_data/scattered_vector.h"
#include "ortools/lp_data/sparse.h"
#include "ortools/util/stats.h"

namespace operations_research {
namespace glop {

VariableValues::VariableValues(const GlopParameters& parameters,
                               const CompactSparseMatrix& matrix,
                               const RowToColMapping& basis,
                               const VariablesInfo& variables_info,
                               const BasisFactorization& basis_factorization,
                               DualEdgeNorms* dual_edge_norms,
                               DynamicMaximum<RowIndex>* dual_prices)
    : parameters_(parameters),
      matrix_(matrix),
      basis_(basis),
      variables_info_(variables_info),
      basis_factorization_(basis_factorization),
      dual_edge_norms_(dual_edge_norms),
      dual_prices_(dual_prices),
      stats_("VariableValues") {}

void VariableValues::SetNonBasicVariableValueFromStatus(ColIndex col) {
  SCOPED_TIME_STAT(&stats_);
  const DenseRow& lower_bounds = variables_info_.GetVariableLowerBounds();
  const DenseRow& upper_bounds = variables_info_.GetVariableUpperBounds();
  variable_values_.resize(matrix_.num_cols(), 0.0);
  switch (variables_info_.GetStatusRow()[col]) {
    case VariableStatus::FIXED_VALUE:
      DCHECK_NE(-kInfinity, lower_bounds[col]);
      DCHECK_EQ(lower_bounds[col], upper_bounds[col]);
      variable_values_[col] = lower_bounds[col];
      break;
    case VariableStatus::AT_LOWER_BOUND:
      DCHECK_NE(-kInfinity, lower_bounds[col]);
      variable_values_[col] = lower_bounds[col];
      break;
    case VariableStatus::AT_UPPER_BOUND:
      DCHECK_NE(kInfinity, upper_bounds[col]);
      variable_values_[col] = upper_bounds[col];
      break;
    case VariableStatus::FREE:
      LOG(DFATAL) << "SetNonBasicVariableValueFromStatus() shouldn't "
                  << "be called on a FREE variable.";
      break;
    case VariableStatus::BASIC:
      LOG(DFATAL) << "SetNonBasicVariableValueFromStatus() shouldn't "
                  << "be called on a BASIC variable.";
      break;
  }
  // Note that there is no default value in the switch() statement above to
  // get a compile-time error if a value is missing.
}

void VariableValues::ResetAllNonBasicVariableValues(
    const DenseRow& free_initial_value) {
  const DenseRow& lower_bounds = variables_info_.GetVariableLowerBounds();
  const DenseRow& upper_bounds = variables_info_.GetVariableUpperBounds();
  const VariableStatusRow& statuses = variables_info_.GetStatusRow();
  const ColIndex num_cols = matrix_.num_cols();
  variable_values_.resize(num_cols, 0.0);
  for (ColIndex col(0); col < num_cols; ++col) {
    switch (statuses[col]) {
      case VariableStatus::FIXED_VALUE:
        ABSL_FALLTHROUGH_INTENDED;
      case VariableStatus::AT_LOWER_BOUND:
        variable_values_[col] = lower_bounds[col];
        break;
      case VariableStatus::AT_UPPER_BOUND:
        variable_values_[col] = upper_bounds[col];
        break;
      case VariableStatus::FREE:
        variable_values_[col] =
            col < free_initial_value.size() ? free_initial_value[col] : 0.0;
        break;
      case VariableStatus::BASIC:
        break;
    }
  }
}

void VariableValues::RecomputeBasicVariableValues() {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(basis_factorization_.IsRefactorized());
  const RowIndex num_rows = matrix_.num_rows();
  scratchpad_.non_zeros.clear();
  scratchpad_.values.AssignToZero(num_rows);
  for (const ColIndex col : variables_info_.GetNotBasicBitRow()) {
    const Fractional value = variable_values_[col];
    matrix_.ColumnAddMultipleToDenseColumn(col, -value, &scratchpad_.values);
  }
  basis_factorization_.RightSolve(&scratchpad_);
  for (RowIndex row(0); row < num_rows; ++row) {
    variable_values_[basis_[row]] = scratchpad_[row];
  }

  // This makes sure that they will be recomputed if needed.
  dual_prices_->Clear();
}

Fractional VariableValues::ComputeMaximumPrimalResidual() const {
  SCOPED_TIME_STAT(&stats_);
  scratchpad_.non_zeros.clear();
  scratchpad_.values.AssignToZero(matrix_.num_rows());
  const ColIndex num_cols = matrix_.num_cols();
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional value = variable_values_[col];
    matrix_.ColumnAddMultipleToDenseColumn(col, value, &scratchpad_.values);
  }
  return InfinityNorm(scratchpad_.values);
}

Fractional VariableValues::ComputeMaximumPrimalInfeasibility() const {
  SCOPED_TIME_STAT(&stats_);
  Fractional primal_infeasibility = 0.0;
  const ColIndex num_cols = matrix_.num_cols();
  const DenseRow::ConstView values = variable_values_.const_view();
  const DenseRow::ConstView lower_bounds =
      variables_info_.GetVariableLowerBounds().const_view();
  const DenseRow::ConstView upper_bounds =
      variables_info_.GetVariableUpperBounds().const_view();
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional infeasibility =
        GetColInfeasibility(col, values, lower_bounds, upper_bounds);
    primal_infeasibility = std::max(primal_infeasibility, infeasibility);
  }
  return primal_infeasibility;
}

Fractional VariableValues::ComputeSumOfPrimalInfeasibilities() const {
  SCOPED_TIME_STAT(&stats_);
  Fractional sum = 0.0;
  const ColIndex num_cols = matrix_.num_cols();
  const DenseRow::ConstView values = variable_values_.const_view();
  const DenseRow::ConstView lower_bounds =
      variables_info_.GetVariableLowerBounds().const_view();
  const DenseRow::ConstView upper_bounds =
      variables_info_.GetVariableUpperBounds().const_view();
  for (ColIndex col(0); col < num_cols; ++col) {
    const Fractional infeasibility =
        GetColInfeasibility(col, values, lower_bounds, upper_bounds);
    sum += std::max(Fractional{0.0}, infeasibility);
  }
  return sum;
}

void VariableValues::UpdateOnPivoting(const ScatteredColumn& direction,
                                      ColIndex entering_col, Fractional step) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(IsFinite(step));

  // Note(user): Some positions are ignored during the primal ratio test:
  // - The rows for which direction_[row] < tolerance.
  // - The non-zeros of direction_ignored_position_ in case of degeneracy.
  // Such positions may result in basic variables going out of their bounds by
  // more than the allowed tolerance. We could choose not to update these
  // variables or not make them take out-of-bound values, but this would
  // introduce artificial errors.

  // Note that there is no need to call variables_info_.Update() on basic
  // variables when they change values. Note also that the status of
  // entering_col will be updated later.
  auto basis = basis_.const_view();
  auto values = variable_values_.view();
  for (const auto e : direction) {
    const ColIndex col = basis[e.row()];
    values[col] -= e.coefficient() * step;
  }
  values[entering_col] += step;
}

void VariableValues::UpdateGivenNonBasicVariables(
    absl::Span<const ColIndex> cols_to_update, bool update_basic_variables) {
  SCOPED_TIME_STAT(&stats_);
  if (!update_basic_variables) {
    for (ColIndex col : cols_to_update) {
      SetNonBasicVariableValueFromStatus(col);
    }
    return;
  }

  const RowIndex num_rows = matrix_.num_rows();
  initially_all_zero_scratchpad_.values.resize(num_rows, 0.0);
  DCHECK(IsAllZero(initially_all_zero_scratchpad_.values));
  initially_all_zero_scratchpad_.ClearSparseMask();
  bool use_dense = false;
  for (ColIndex col : cols_to_update) {
    const Fractional old_value = variable_values_[col];
    SetNonBasicVariableValueFromStatus(col);
    if (use_dense) {
      matrix_.ColumnAddMultipleToDenseColumn(
          col, variable_values_[col] - old_value,
          &initially_all_zero_scratchpad_.values);
    } else {
      matrix_.ColumnAddMultipleToSparseScatteredColumn(
          col, variable_values_[col] - old_value,
          &initially_all_zero_scratchpad_);
      use_dense = initially_all_zero_scratchpad_.ShouldUseDenseIteration();
    }
  }
  initially_all_zero_scratchpad_.ClearSparseMask();
  initially_all_zero_scratchpad_.ClearNonZerosIfTooDense();

  basis_factorization_.RightSolve(&initially_all_zero_scratchpad_);
  if (initially_all_zero_scratchpad_.non_zeros.empty()) {
    for (RowIndex row(0); row < num_rows; ++row) {
      variable_values_[basis_[row]] -= initially_all_zero_scratchpad_[row];
    }
    initially_all_zero_scratchpad_.values.AssignToZero(num_rows);
    RecomputeDualPrices();
    return;
  }

  for (const auto e : initially_all_zero_scratchpad_) {
    variable_values_[basis_[e.row()]] -= e.coefficient();
    initially_all_zero_scratchpad_[e.row()] = 0.0;
  }
  UpdateDualPrices(initially_all_zero_scratchpad_.non_zeros);
  initially_all_zero_scratchpad_.non_zeros.clear();
}

void VariableValues::RecomputeDualPrices(bool put_more_importance_on_norm) {
  SCOPED_TIME_STAT(&stats_);
  const RowIndex num_rows = matrix_.num_rows();
  dual_prices_->ClearAndResize(num_rows);
  dual_prices_->StartDenseUpdates();

  put_more_importance_on_norm_ = put_more_importance_on_norm;
  const Fractional tolerance = parameters_.primal_feasibility_tolerance();
  const DenseColumn::ConstView squared_norms =
      dual_edge_norms_->GetEdgeSquaredNorms();
  const RowToColMapping::ConstView basis = basis_.const_view();
  const DenseRow::ConstView values = variable_values_.const_view();
  const DenseRow::ConstView lower_bounds =
      variables_info_.GetVariableLowerBounds().const_view();
  const DenseRow::ConstView upper_bounds =
      variables_info_.GetVariableUpperBounds().const_view();
  if (put_more_importance_on_norm) {
    for (RowIndex row(0); row < num_rows; ++row) {
      const ColIndex col = basis[row];
      const Fractional infeasibility =
          GetColInfeasibility(col, values, lower_bounds, upper_bounds);
      if (infeasibility > tolerance) {
        dual_prices_->DenseAddOrUpdate(
            row, std::abs(infeasibility) / squared_norms[row]);
      }
    }
  } else {
    for (RowIndex row(0); row < num_rows; ++row) {
      const ColIndex col = basis[row];
      const Fractional infeasibility =
          GetColInfeasibility(col, values, lower_bounds, upper_bounds);
      if (infeasibility > tolerance) {
        dual_prices_->DenseAddOrUpdate(
            row, Square(infeasibility) / squared_norms[row]);
      }
    }
  }
}

void VariableValues::UpdateDualPrices(absl::Span<const RowIndex> rows) {
  if (dual_prices_->Size() != matrix_.num_rows()) {
    RecomputeDualPrices(put_more_importance_on_norm_);
    return;
  }

  // Note(user): this is the same as the code in RecomputeDualPrices(), but we
  // do need the clear part.
  SCOPED_TIME_STAT(&stats_);
  const Fractional tolerance = parameters_.primal_feasibility_tolerance();
  const RowToColMapping::ConstView basis = basis_.const_view();
  const DenseColumn::ConstView squared_norms =
      dual_edge_norms_->GetEdgeSquaredNorms();
  const DenseRow::ConstView values = variable_values_.const_view();
  const DenseRow::ConstView lower_bounds =
      variables_info_.GetVariableLowerBounds().const_view();
  const DenseRow::ConstView upper_bounds =
      variables_info_.GetVariableUpperBounds().const_view();
  if (put_more_importance_on_norm_) {
    for (const RowIndex row : rows) {
      const ColIndex col = basis[row];
      const Fractional infeasibility =
          GetColInfeasibility(col, values, lower_bounds, upper_bounds);
      if (infeasibility > tolerance) {
        dual_prices_->AddOrUpdate(row,
                                  std::abs(infeasibility) / squared_norms[row]);
      } else {
        dual_prices_->Remove(row);
      }
    }
  } else {
    for (const RowIndex row : rows) {
      const ColIndex col = basis[row];
      const Fractional infeasibility =
          GetColInfeasibility(col, values, lower_bounds, upper_bounds);
      if (infeasibility > tolerance) {
        dual_prices_->AddOrUpdate(row,
                                  Square(infeasibility) / squared_norms[row]);
      } else {
        dual_prices_->Remove(row);
      }
    }
  }
}

}  // namespace glop
}  // namespace operations_research
