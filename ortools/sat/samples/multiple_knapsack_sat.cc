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
// Solves a multiple knapsack problem using the CP-SAT solver.
// [START import]
#include <map>
#include <numeric>
#include <tuple>
#include <vector>

#include "absl/strings/str_format.h"
#include "ortools/sat/cp_model.h"
// [END import]

namespace operations_research {
namespace sat {

void MultipleKnapsackSat() {
  // [START data]
  const std::vector<int> weights = {
      {48, 30, 42, 36, 36, 48, 42, 42, 36, 24, 30, 30, 42, 36, 36}};
  const std::vector<int> values = {
      {10, 30, 25, 50, 35, 30, 15, 40, 30, 35, 45, 10, 20, 30, 25}};
  const int num_items = static_cast<int>(weights.size());
  std::vector<int> all_items(num_items);
  std::iota(all_items.begin(), all_items.end(), 0);

  const std::vector<int> bin_capacities = {{100, 100, 100, 100, 100}};
  const int num_bins = static_cast<int>(bin_capacities.size());
  std::vector<int> all_bins(num_bins);
  std::iota(all_bins.begin(), all_bins.end(), 0);
  // [END data]

  // [START model]
  CpModelBuilder cp_model;
  // [END model]

  // Variables.
  // [START variables]
  // x[i, b] = 1 if item i is packed in bin b.
  std::map<std::tuple<int, int>, BoolVar> x;
  for (int i : all_items) {
    for (int b : all_bins) {
      auto key = std::make_tuple(i, b);
      x[key] = cp_model.NewBoolVar().WithName(absl::StrFormat("x_%d_%d", i, b));
    }
  }
  // [END variables]

  // Constraints.
  // [START constraints]
  // Each item is assigned to at most one bin.
  for (int i : all_items) {
    LinearExpr expr;
    for (int b : all_bins) {
      expr += x[std::make_tuple(i, b)];
    }
    cp_model.AddLessOrEqual(expr, 1);
  }

  // The amount packed in each bin cannot exceed its capacity.
  for (int b : all_bins) {
    LinearExpr bin_weight;
    for (int i : all_items) {
      bin_weight += x[std::make_tuple(i, b)] * weights[i];
    }
    cp_model.AddLessOrEqual(bin_weight, bin_capacities[b]);
  }
  // [END constraints]

  // Objective.
  // [START objective]
  // Maximize total value of packed items.
  LinearExpr objective;
  for (int i : all_items) {
    for (int b : all_bins) {
      objective += x[std::make_tuple(i, b)] * values[i];
    }
  }
  cp_model.Maximize(objective);
  // [END objective]

  // [START solve]
  const CpSolverResponse response = Solve(cp_model.Build());
  // [END solve]

  // [START print_solution]
  if (response.status() == CpSolverStatus::OPTIMAL ||
      response.status() == CpSolverStatus::FEASIBLE) {
    LOG(INFO) << "Total packed value: " << response.objective_value();
    double total_weight = 0.0;
    for (int b : all_bins) {
      LOG(INFO) << "Bin " << b;
      double bin_weight = 0.0;
      double bin_value = 0.0;
      for (int i : all_items) {
        auto key = std::make_tuple(i, b);
        if (SolutionIntegerValue(response, x[key]) > 0) {
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

  // Statistics.
  // [START statistics]
  LOG(INFO) << "Statistics";
  LOG(INFO) << CpSolverResponseStats(response);
  // [END statistics]
}
}  // namespace sat
}  // namespace operations_research

int main() {
  operations_research::sat::MultipleKnapsackSat();
  return EXIT_SUCCESS;
}
// [END program]
