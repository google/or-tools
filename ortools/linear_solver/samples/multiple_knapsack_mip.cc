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

// [START program]
// Solve a multiple knapsack problem using a MIP solver.
// [START import]
#include <iostream>
#include <numeric>
#include <vector>

#include "absl/strings/str_format.h"
#include "ortools/linear_solver/linear_expr.h"
#include "ortools/linear_solver/linear_solver.h"
// [END import]

namespace operations_research {

void MultipleKnapsackMip() {
  // [START data]
  const std::vector<int> weights = {
      {48, 30, 42, 36, 36, 48, 42, 42, 36, 24, 30, 30, 42, 36, 36}};
  const std::vector<int> values = {
      {10, 30, 25, 50, 35, 30, 15, 40, 30, 35, 45, 10, 20, 30, 25}};
  const int num_items = weights.size();
  std::vector<int> all_items(num_items);
  std::iota(all_items.begin(), all_items.end(), 0);

  const std::vector<int> bin_capacities = {{100, 100, 100, 100, 100}};
  const int num_bins = bin_capacities.size();
  std::vector<int> all_bins(num_bins);
  std::iota(all_bins.begin(), all_bins.end(), 0);
  // [END data]

  // Create the mip solver with the SCIP backend.
  // [START solver]
  std::unique_ptr<MPSolver> solver(MPSolver::CreateSolver("SCIP"));
  if (!solver) {
    LOG(WARNING) << "SCIP solver unavailable.";
    return;
  }
  // [END solver]

  // Variables.
  // [START variables]
  // x[i][b] = 1 if item i is packed in bin b.
  std::vector<std::vector<const MPVariable*>> x(
      num_items, std::vector<const MPVariable*>(num_bins));
  for (int i : all_items) {
    for (int b : all_bins) {
      x[i][b] = solver->MakeBoolVar(absl::StrFormat("x_%d_%d", i, b));
    }
  }
  // [END variables]

  // Constraints.
  // [START constraints]
  // Each item is assigned to at most one bin.
  for (int i : all_items) {
    LinearExpr sum;
    for (int b : all_bins) {
      sum += x[i][b];
    }
    solver->MakeRowConstraint(sum <= 1.0);
  }
  // The amount packed in each bin cannot exceed its capacity.
  for (int b : all_bins) {
    LinearExpr bin_weight;
    for (int i : all_items) {
      bin_weight += LinearExpr(x[i][b]) * weights[i];
    }
    solver->MakeRowConstraint(bin_weight <= bin_capacities[b]);
  }
  // [END constraints]

  // Objective.
  // [START objective]
  // Maximize total value of packed items.
  MPObjective* const objective = solver->MutableObjective();
  LinearExpr objective_value;
  for (int i : all_items) {
    for (int b : all_bins) {
      objective_value += LinearExpr(x[i][b]) * values[i];
    }
  }
  objective->MaximizeLinearExpr(objective_value);
  // [END objective]

  // [START solve]
  const MPSolver::ResultStatus result_status = solver->Solve();
  // [END solve]

  // [START print_solution]
  if (result_status == MPSolver::OPTIMAL) {
    LOG(INFO) << "Total packed value: " << objective->Value();
    double total_weight = 0.0;
    for (int b : all_bins) {
      LOG(INFO) << "Bin " << b;
      double bin_weight = 0.0;
      double bin_value = 0.0;
      for (int i : all_items) {
        if (x[i][b]->solution_value() > 0) {
          LOG(INFO) << "Item " << i << " weight: " << weights[i]
                    << " value: " << values[i];
          bin_weight += weights[i];
          bin_value += values[i];
        }
      }
      LOG(INFO) << "Packed bin weight: " << bin_weight;
      LOG(INFO) << "Packed bin value: " << bin_value;
      total_weight += bin_weight;
    }
    LOG(INFO) << "Total packed weight: " << total_weight;
  } else {
    LOG(INFO) << "The problem does not have an optimal solution.";
  }
  // [END print_solution]
}
}  // namespace operations_research

int main(int argc, char** argv) {
  operations_research::MultipleKnapsackMip();
  return EXIT_SUCCESS;
}
// [END program]
