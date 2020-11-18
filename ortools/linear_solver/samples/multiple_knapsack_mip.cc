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

// [START program]
// [START import]
#include <iostream>
#include <numeric>
#include <vector>

#include "ortools/linear_solver/linear_expr.h"
#include "ortools/linear_solver/linear_solver.h"
// [END import]

// [START program_part1]
namespace operations_research {
// [START data_model]
struct DataModel {
  const std::vector<int> weights = {48, 30, 42, 36, 36, 48, 42, 42,
                                    36, 24, 30, 30, 42, 36, 36};
  const std::vector<int> values = {10, 30, 25, 50, 35, 30, 15, 40,
                                   30, 35, 45, 10, 20, 30, 25};
  const int num_items = weights.size();
  const int total_value = std::accumulate(values.begin(), values.end(), 0);
  const std::vector<int> bin_capacities = {{100, 100, 100, 100, 100}};
  const int num_bins = 5;
};
// [END data_model]

void MultipleKnapsackMip() {
  // [START data]
  DataModel data;
  // [END data]
  // [END program_part1]

  // [START solver]
  // Create the mip solver with the CBC backend.
  MPSolver solver("multiple_knapsack_mip",
                  MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING);
  // [END solver]

  // [START program_part2]
  // [START variables]
  std::vector<std::vector<const MPVariable*>> x(
      data.num_items, std::vector<const MPVariable*>(data.num_bins));
  for (int i = 0; i < data.num_items; ++i) {
    for (int j = 0; j < data.num_bins; ++j) {
      x[i][j] = solver.MakeIntVar(0.0, 1.0, "");
    }
  }
  // [END variables]

  // [START constraints]
  // Create the constraints.
  // Each item is in at most one bin.
  for (int i = 0; i < data.num_items; ++i) {
    LinearExpr sum;
    for (int j = 0; j < data.num_bins; ++j) {
      sum += x[i][j];
    }
    solver.MakeRowConstraint(sum <= 1.0);
  }
  // For each bin that is used, the total packed weight can be at most
  // the bin capacity.
  for (int j = 0; j < data.num_bins; ++j) {
    LinearExpr weight;
    for (int i = 0; i < data.num_items; ++i) {
      weight += data.weights[i] * LinearExpr(x[i][j]);
    }
    solver.MakeRowConstraint(weight <= data.bin_capacities[j]);
  }
  // [END constraints]

  // [START objective]
  // Create the objective function.
  MPObjective* const objective = solver.MutableObjective();
  LinearExpr value;
  for (int i = 0; i < data.num_items; ++i) {
    for (int j = 0; j < data.num_bins; ++j) {
      value += data.values[i] * LinearExpr(x[i][j]);
    }
  }
  objective->MaximizeLinearExpr(value);
  // [END objective]

  // [START solve]
  const MPSolver::ResultStatus result_status = solver.Solve();
  // [END solve]

  // [START print_solution]
  // Check that the problem has an optimal solution.
  if (result_status != MPSolver::OPTIMAL) {
    std::cerr << "The problem does not have an optimal solution!";
  }
  std::cout << "Total packed value: " << objective->Value() << "\n\n";
  double total_weight = 0;
  for (int j = 0; j < data.num_bins; ++j) {
    double bin_weight = 0;
    double bin_value = 0;
    std::cout << "Bin " << j << std::endl << std::endl;
    for (int i = 0; i < data.num_items; ++i) {
      if (x[i][j]->solution_value() == 1) {
        std::cout << "Item " << i << " - weight: " << data.weights[i]
                  << "  value: " << data.values[i] << std::endl;
        bin_weight += data.weights[i];
        bin_value += data.values[i];
      }
    }
    std::cout << "Packed bin weight: " << bin_weight << std::endl;
    std::cout << "Packed bin value: " << bin_value << std::endl << std::endl;
    total_weight += bin_weight;
  }
  std::cout << "Total packed weight: " << total_weight << std::endl;
  // [END print_solution]
}
}  // namespace operations_research

int main(int argc, char** argv) {
  operations_research::MultipleKnapsackMip();
  return EXIT_SUCCESS;
}
// [END program_part2]
// [END program]
