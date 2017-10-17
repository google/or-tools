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

#include "ortools/lp_data/lp_decomposer.h"

#include <vector>

#include "ortools/algorithms/dynamic_partition.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_utils.h"

namespace operations_research {
namespace glop {

//------------------------------------------------------------------------------
// LPDecomposer
//------------------------------------------------------------------------------
LPDecomposer::LPDecomposer()
    : original_problem_(nullptr),
      clusters_(),
      mutex_() {}

void LPDecomposer::Decompose(const LinearProgram* linear_problem) {
  MutexLock mutex_lock(&mutex_);
  original_problem_ = linear_problem;
  clusters_.clear();

  const SparseMatrix& transposed_matrix =
      original_problem_->GetTransposeSparseMatrix();
  MergingPartition partition(original_problem_->num_variables().value());

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
}

int LPDecomposer::GetNumberOfProblems() const {
  MutexLock mutex_lock(&mutex_);
  return clusters_.size();
}

const LinearProgram& LPDecomposer::original_problem() const {
  MutexLock mutex_lock(&mutex_);
  return *original_problem_;
}

void LPDecomposer::ExtractLocalProblem(int problem_index, LinearProgram* lp) {
  CHECK(lp != nullptr);
  CHECK_GE(problem_index, 0);
  CHECK_LT(problem_index, clusters_.size());

  lp->Clear();

  MutexLock mutex_lock(&mutex_);
  const std::vector<ColIndex>& cluster = clusters_[problem_index];
  StrictITIVector<ColIndex, ColIndex> global_to_local(
      original_problem_->num_variables(), kInvalidCol);
  SparseBitset<RowIndex> constraints_to_use(
      original_problem_->num_constraints());
  lp->SetMaximizationProblem(original_problem_->IsMaximizationProblem());

  // Create variables and get all constraints of the cluster.
  const SparseMatrix& original_matrix = original_problem_->GetSparseMatrix();
  const SparseMatrix& transposed_matrix =
      original_problem_->GetTransposeSparseMatrix();
  for (int i = 0; i < cluster.size(); ++i) {
    const ColIndex global_col = cluster[i];
    const ColIndex local_col = lp->CreateNewVariable();
    CHECK_EQ(local_col, ColIndex(i));
    CHECK(global_to_local[global_col] == kInvalidCol ||
          global_to_local[global_col] == local_col)
        << "If the mapping is already assigned it has to be the same.";
    global_to_local[global_col] = local_col;

    lp->SetVariableName(local_col,
                        original_problem_->GetVariableName(global_col));
    lp->SetVariableType(local_col,
                        original_problem_->GetVariableType(global_col));
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
      const ColIndex local_col = global_to_local[global_col];
      lp->SetCoefficient(local_row, local_col, e.coefficient());
    }
  }
}

DenseRow LPDecomposer::AggregateAssignments(
    const std::vector<DenseRow>& assignments) const {
  CHECK_EQ(assignments.size(), clusters_.size());

  MutexLock mutex_lock(&mutex_);
  DenseRow global_assignment(original_problem_->num_variables(),
                             Fractional(0.0));
  for (int problem = 0; problem < assignments.size(); ++problem) {
    const DenseRow& local_assignment = assignments[problem];
    const std::vector<ColIndex>& cluster = clusters_[problem];
    for (int i = 0; i < local_assignment.size(); ++i) {
      const ColIndex global_col = cluster[i];
      global_assignment[global_col] = local_assignment[ColIndex(i)];
    }
  }
  return global_assignment;
}

DenseRow LPDecomposer::ExtractLocalAssignment(int problem_index,
                                              const DenseRow& assignment) {
  CHECK_GE(problem_index, 0);
  CHECK_LT(problem_index, clusters_.size());
  CHECK_EQ(assignment.size(), original_problem_->num_variables());

  MutexLock mutex_lock(&mutex_);
  const std::vector<ColIndex>& cluster = clusters_[problem_index];
  DenseRow local_assignment(ColIndex(cluster.size()), Fractional(0.0));
  for (int i = 0; i < cluster.size(); ++i) {
    const ColIndex global_col = cluster[i];
    local_assignment[ColIndex(i)] = assignment[global_col];
  }
  return local_assignment;
}

}  // namespace glop
}  // namespace operations_research
