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

#include "lp_data/lp_decomposer.h"

#include <vector>

#include "algorithms/dynamic_partition.h"
#include "lp_data/lp_data.h"
#include "lp_data/lp_utils.h"

namespace operations_research {
namespace glop {

//------------------------------------------------------------------------------
// LPDecomposer
//------------------------------------------------------------------------------
LPDecomposer::LPDecomposer()
    : original_problem_(nullptr),
      clusters_(),
      local_to_global_vars_(),
      mutex_() {}

void LPDecomposer::Decompose(const LinearProgram* linear_problem) {
  MutexLock mutex_lock(&mutex_);
  original_problem_ = linear_problem;
  clusters_.clear();
  local_to_global_vars_.clear();

  const SparseMatrix& transposed_matrix =
      original_problem_->GetTransposeSparseMatrix();
  MergingPartition partition;
  partition.Reset(original_problem_->num_variables().value());

  // Iterate on all constraints, and merge all variables of each constraint.
  const ColIndex num_ct = RowToColIndex(original_problem_->num_constraints());
  for (ColIndex ct(0); ct < num_ct; ++ct) {
    const SparseColumn& sparse_constraint = transposed_matrix.column(ct);
    if (sparse_constraint.num_entries() > 1) {
      const RowIndex first_row = sparse_constraint.GetFirstRow();
      for (EntryIndex e(1); e < sparse_constraint.num_entries(); ++e) {
        partition.MergePartsOf(first_row.value(),
                               sparse_constraint.EntryRow(e).value());
      }
    }
  }

  std::vector<int> classes;
  const int num_classes = partition.FillEquivalenceClasses(&classes);
  clusters_.resize(num_classes);
  for (int i = 0; i < classes.size(); ++i) {
    clusters_[classes[i]].push_back(ColIndex(i));
  }
  for (int i = 0; i < num_classes; ++i) {
    std::sort(clusters_[i].begin(), clusters_[i].end());
  }
  local_to_global_vars_.resize(clusters_.size());
}

int LPDecomposer::GetNumberOfProblems() const {
  MutexLock mutex_lock(&mutex_);
  return clusters_.size();
}

const LinearProgram& LPDecomposer::original_problem() const {
  MutexLock mutex_lock(&mutex_);
  return *original_problem_;
}

void LPDecomposer::BuildProblem(int problem_index, LinearProgram* lp) {
  CHECK(lp != nullptr);
  CHECK_GE(problem_index, 0);
  CHECK_LT(problem_index, clusters_.size());

  lp->Clear();

  const std::vector<ColIndex>& cluster = clusters_[problem_index];
  StrictITIVector<ColIndex, ColIndex> global_to_local_vars(
      original_problem_->num_variables(), kInvalidCol);
  SparseBitset<RowIndex> constraints_to_use(
      original_problem_->num_constraints());
  StrictITIVector<ColIndex, ColIndex> local_to_global(ColIndex(cluster.size()),
                                                      kInvalidCol);
  lp->SetMaximizationProblem(original_problem_->IsMaximizationProblem());

  // Create variables and get all constraints of the cluster.
  const SparseMatrix& original_matrix = original_problem_->GetSparseMatrix();
  const SparseMatrix& transposed_matrix =
      original_problem_->GetTransposeSparseMatrix();
  for (int i = 0; i < cluster.size(); ++i) {
    const ColIndex global_col = cluster[i];
    const ColIndex local_col = lp->CreateNewVariable();
    CHECK(global_to_local_vars[global_col] == kInvalidCol ||
          global_to_local_vars[global_col] == local_col)
        << "If the mapping is already assigned it has to be the same.";
    global_to_local_vars[global_col] = local_col;
    CHECK_EQ(kInvalidCol, local_to_global[local_col]);
    local_to_global[local_col] = global_col;

    lp->SetVariableName(local_col,
                        original_problem_->GetVariableName(global_col));
    lp->SetVariableIntegrality(
        local_col, original_problem_->is_variable_integer()[global_col]);
    lp->SetVariableBounds(
        local_col, original_problem_->variable_lower_bounds()[global_col],
        original_problem_->variable_upper_bounds()[global_col]);
    lp->SetObjectiveCoefficient(
        local_col, original_problem_->objective_coefficients()[global_col]);

    for (const SparseColumn::Entry e : original_matrix.column(global_col)) {
      constraints_to_use.Set(e.row());
    }
  }
  // Create the constraints.
  for (const RowIndex global_row :
       constraints_to_use.PositionsSetAtLeastOnce()) {
    const RowIndex local_row = lp->CreateNewConstraint();
    lp->SetConstraintName(local_row,
                          original_problem_->GetConstraintName(global_row));
    lp->SetConstraintBounds(
        local_row, original_problem_->constraint_lower_bounds()[global_row],
        original_problem_->constraint_upper_bounds()[global_row]);

    for (const SparseColumn::Entry e :
         transposed_matrix.column(RowToColIndex(global_row))) {
      const ColIndex global_col = RowToColIndex(e.row());
      const ColIndex local_col = global_to_local_vars[global_col];
      lp->SetCoefficient(local_row, local_col, e.coefficient());
    }
  }

  MutexLock mutex_lock(&mutex_);
  local_to_global_vars_[problem_index] = local_to_global;
}

DenseRow LPDecomposer::AggregateAssignments(
    const std::vector<DenseRow>& assignments) const {
  CHECK_EQ(assignments.size(), local_to_global_vars_.size());

  MutexLock mutex_lock(&mutex_);
  DenseRow values(original_problem_->num_variables(), Fractional(0.0));
  for (int problem = 0; problem < assignments.size(); ++problem) {
    const DenseRow& assignment = assignments[problem];
    const StrictITIVector<ColIndex, ColIndex>& local_to_global =
        local_to_global_vars_[problem];
    for (ColIndex local_col(0); local_col < assignment.size(); ++local_col) {
      const ColIndex global_col = local_to_global[local_col];
      const Fractional value = assignment[local_col];
      values[global_col] = value;
    }
  }
  return values;
}

}  // namespace glop
}  // namespace operations_research
