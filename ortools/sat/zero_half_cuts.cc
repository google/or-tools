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

#include "ortools/sat/zero_half_cuts.h"

namespace operations_research {
namespace sat {

void ZeroHalfCutHelper::Reset(int size) {
  rows_.clear();
  shifted_lp_values_.clear();
  bound_parity_.clear();
  col_to_rows_.clear();
  col_to_rows_.resize(size);
  tmp_marked_.resize(size);
}

void ZeroHalfCutHelper::ProcessVariables(
    const std::vector<double>& lp_values,
    const std::vector<IntegerValue>& lower_bounds,
    const std::vector<IntegerValue>& upper_bounds) {
  Reset(lp_values.size());

  // Shift all variables to their closest bound.
  lp_values_ = lp_values;
  for (int i = 0; i < lp_values.size(); ++i) {
    const double lb_dist = lp_values[i] - ToDouble(lower_bounds[i]);
    const double ub_dist = ToDouble(upper_bounds[i]) - lp_values_[i];
    if (lb_dist < ub_dist) {
      shifted_lp_values_.push_back(lb_dist);
      bound_parity_.push_back(lower_bounds[i].value() & 1);
    } else {
      shifted_lp_values_.push_back(ub_dist);
      bound_parity_.push_back(upper_bounds[i].value() & 1);
    }
  }
}

void ZeroHalfCutHelper::AddBinaryRow(const CombinationOfRows& binary_row) {
  // No point pushing an all zero row with a zero rhs.
  if (binary_row.cols.empty() && !binary_row.rhs_parity) return;
  for (const int col : binary_row.cols) {
    col_to_rows_[col].push_back(rows_.size());
  }
  rows_.push_back(binary_row);
}

void ZeroHalfCutHelper::AddOneConstraint(
    const glop::RowIndex row,
    const std::vector<std::pair<glop::ColIndex, IntegerValue>>& terms,
    IntegerValue lb, IntegerValue ub) {
  if (terms.size() > kMaxInputConstraintSize) return;

  double activity = 0.0;
  IntegerValue magnitude(0);
  CombinationOfRows binary_row;
  int rhs_adjust = 0;
  for (const auto& term : terms) {
    const int col = term.first.value();
    activity += ToDouble(term.second) * lp_values_[col];
    magnitude = std::max(magnitude, IntTypeAbs(term.second));

    // Only consider odd coefficient.
    if ((term.second.value() & 1) == 0) continue;

    // Ignore column in the binary matrix if its lp value is almost zero.
    if (shifted_lp_values_[col] > 1e-2) {
      binary_row.cols.push_back(col);
    }

    // Because we work on the shifted variable, the rhs needs to be updated.
    rhs_adjust ^= bound_parity_[col];
  }

  // We ignore constraint with large coefficient, since there is little chance
  // to cancel them and because of that the efficacity of a generated cut will
  // be limited.
  if (magnitude > kMaxInputConstraintMagnitude) return;

  // TODO(user): experiment with the best value. probably only tight rows are
  // best? and we could use the basis status rather than recomputing the
  // activity for that.
  //
  // TODO(user): Avoid adding duplicates and just randomly pick one. Note
  // that we should also remove duplicate in a generic way.
  const double tighteness_threshold = 1e-2;
  if (ToDouble(ub) - activity < tighteness_threshold) {
    binary_row.multipliers = {{row, IntegerValue(1)}};
    binary_row.slack = ToDouble(ub) - activity;
    binary_row.rhs_parity = (ub.value() & 1) ^ rhs_adjust;
    AddBinaryRow(binary_row);
  }
  if (activity - ToDouble(lb) < tighteness_threshold) {
    binary_row.multipliers = {{row, IntegerValue(-1)}};
    binary_row.slack = activity - ToDouble(lb);
    binary_row.rhs_parity = (lb.value() & 1) ^ rhs_adjust;
    AddBinaryRow(binary_row);
  }
}

void ZeroHalfCutHelper::SymmetricDifference(
    std::function<bool(int)> extra_condition, const std::vector<int>& a,
    std::vector<int>* b) {
  for (const int v : *b) tmp_marked_[v] = true;
  for (const int v : a) {
    if (tmp_marked_[v]) {
      tmp_marked_[v] = false;
    } else {
      tmp_marked_[v] = true;

      // TODO(user): optim by doing that at the end?
      b->push_back(v);
    }
  }

  // Remove position that are not marked, and clear tmp_marked_.
  int new_size = 0;
  for (const int v : *b) {
    if (tmp_marked_[v]) {
      if (extra_condition(v)) {
        (*b)[new_size++] = v;
      }
      tmp_marked_[v] = false;
    }
  }
  b->resize(new_size);
}

void ZeroHalfCutHelper::ProcessSingletonColumns() {
  for (const int singleton_col : singleton_cols_) {
    if (col_to_rows_[singleton_col].empty()) continue;
    CHECK_EQ(col_to_rows_[singleton_col].size(), 1);
    const int row = col_to_rows_[singleton_col][0];
    int new_size = 0;
    auto& mutable_cols = rows_[row].cols;
    for (const int col : mutable_cols) {
      if (col == singleton_col) continue;
      mutable_cols[new_size++] = col;
    }
    CHECK_LT(new_size, mutable_cols.size());
    mutable_cols.resize(new_size);
    col_to_rows_[singleton_col].clear();
    rows_[row].slack += shifted_lp_values_[singleton_col];
  }
  singleton_cols_.clear();
}

// This is basically one step of a Gaussian elimination with the given pivot.
void ZeroHalfCutHelper::EliminateVarUsingRow(int eliminated_col,
                                             int eliminated_row) {
  CHECK_LE(rows_[eliminated_row].slack, 1e-6);
  CHECK(!rows_[eliminated_row].cols.empty());

  // First update the row representation of the matrix.
  tmp_marked_.resize(std::max(col_to_rows_.size(), rows_.size()));
  DCHECK(std::all_of(tmp_marked_.begin(), tmp_marked_.end(),
                     [](bool b) { return !b; }));
  int new_size = 0;
  for (const int other_row : col_to_rows_[eliminated_col]) {
    if (other_row == eliminated_row) continue;
    col_to_rows_[eliminated_col][new_size++] = other_row;

    SymmetricDifference([](int i) { return true; }, rows_[eliminated_row].cols,
                        &rows_[other_row].cols);

    // Update slack & parity.
    rows_[other_row].rhs_parity ^= rows_[eliminated_row].rhs_parity;
    rows_[other_row].slack += rows_[eliminated_row].slack;

    // Update the multipliers the same way.
    {
      auto& mutable_multipliers = rows_[other_row].multipliers;
      mutable_multipliers.insert(mutable_multipliers.end(),
                                 rows_[eliminated_row].multipliers.begin(),
                                 rows_[eliminated_row].multipliers.end());
      std::sort(mutable_multipliers.begin(), mutable_multipliers.end());
      int new_size = 0;
      for (const auto& entry : mutable_multipliers) {
        if (new_size > 0 && entry == mutable_multipliers[new_size - 1]) {
          // Cancel both.
          --new_size;
        } else {
          mutable_multipliers[new_size++] = entry;
        }
      }
      mutable_multipliers.resize(new_size);
    }
  }
  col_to_rows_[eliminated_col].resize(new_size);

  // Then update the col representation of the matrix.
  //
  // Note that we remove from the col-wise representation any rows with a large
  // slack.
  {
    int new_size = 0;
    for (const int other_col : rows_[eliminated_row].cols) {
      if (other_col == eliminated_col) continue;
      const int old_size = col_to_rows_[other_col].size();
      rows_[eliminated_row].cols[new_size++] = other_col;
      SymmetricDifference(
          [this](int i) { return rows_[i].slack < kSlackThreshold; },
          col_to_rows_[eliminated_col], &col_to_rows_[other_col]);
      if (old_size != 1 && col_to_rows_[other_col].size() == 1) {
        singleton_cols_.push_back(other_col);
      }
    }
    rows_[eliminated_row].cols.resize(new_size);
  }

  // Clear col.
  col_to_rows_[eliminated_col].clear();
  rows_[eliminated_row].slack += shifted_lp_values_[eliminated_col];
}

std::vector<std::vector<std::pair<glop::RowIndex, IntegerValue>>>
ZeroHalfCutHelper::InterestingCandidates(ModelRandomGenerator* random) {
  std::vector<std::vector<std::pair<glop::RowIndex, IntegerValue>>> result;

  // Initialize singleton_cols_.
  singleton_cols_.clear();
  for (int col = 0; col < col_to_rows_.size(); ++col) {
    if (col_to_rows_[col].size() == 1) singleton_cols_.push_back(col);
  }

  // Process rows by increasing size, but randomize if same size.
  std::vector<int> to_process;
  for (int row = 0; row < rows_.size(); ++row) to_process.push_back(row);
  std::shuffle(to_process.begin(), to_process.end(), *random);
  std::stable_sort(to_process.begin(), to_process.end(), [this](int a, int b) {
    return rows_[a].cols.size() < rows_[b].cols.size();
  });

  for (const int row : to_process) {
    ProcessSingletonColumns();

    if (rows_[row].cols.empty()) continue;
    if (rows_[row].slack > 1e-6) continue;
    if (rows_[row].multipliers.size() > kMaxAggregationSize) continue;

    // Heuristic: eliminate the variable with highest shifted lp value.
    int eliminated_col = -1;
    double max_lp_value = 0.0;
    for (const int col : rows_[row].cols) {
      if (shifted_lp_values_[col] > max_lp_value) {
        max_lp_value = shifted_lp_values_[col];
        eliminated_col = col;
      }
    }
    if (eliminated_col == -1) continue;

    EliminateVarUsingRow(eliminated_col, row);
  }

  // As an heuristic, we just try to add zero rows with an odd rhs and a low
  // enough slack.
  for (const auto& row : rows_) {
    if (row.cols.empty() && row.rhs_parity && row.slack < kSlackThreshold) {
      result.push_back(row.multipliers);
    }
  }
  VLOG(1) << "#candidates: " << result.size() << " / " << rows_.size();
  return result;
}

}  // namespace sat
}  // namespace operations_research
