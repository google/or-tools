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

// [START program]
// [START import]
#include <stdlib.h>

#include <vector>

#include "absl/base/log_severity.h"
#include "absl/log/globals.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"

// [END import]
namespace operations_research {
namespace sat {

void IntegerProgrammingExample() {
  // Data
  // [START data_model]
  const std::vector<std::vector<int>> costs{
      {90, 80, 75, 70},   {35, 85, 55, 65},   {125, 95, 90, 95},
      {45, 110, 95, 115}, {50, 100, 90, 100},
  };
  const int num_workers = static_cast<int>(costs.size());
  const int num_tasks = static_cast<int>(costs[0].size());
  // [END data_model]

  // Model
  // [START model]
  CpModelBuilder cp_model;
  // [END model]

  // Variables
  // [START variables]
  // x[i][j] is an array of Boolean variables. x[i][j] is true
  // if worker i is assigned to task j.
  std::vector<std::vector<BoolVar>> x(num_workers,
                                      std::vector<BoolVar>(num_tasks));
  for (int i = 0; i < num_workers; ++i) {
    for (int j = 0; j < num_tasks; ++j) {
      x[i][j] = cp_model.NewBoolVar();
    }
  }
  // [END variables]

  // Constraints
  // [START constraints]
  // Each worker is assigned to at most one task.
  for (int i = 0; i < num_workers; ++i) {
    cp_model.AddAtMostOne(x[i]);
  }
  // Each task is assigned to exactly one worker.
  for (int j = 0; j < num_tasks; ++j) {
    std::vector<BoolVar> tasks;
    for (int i = 0; i < num_workers; ++i) {
      tasks.push_back(x[i][j]);
    }
    cp_model.AddExactlyOne(tasks);
  }
  // [END constraints]

  // Objective
  // [START objective]
  LinearExpr total_cost;
  for (int i = 0; i < num_workers; ++i) {
    for (int j = 0; j < num_tasks; ++j) {
      total_cost += x[i][j] * costs[i][j];
    }
  }
  cp_model.Minimize(total_cost);
  // [END objective]

  // Solve
  // [START solve]
  const CpSolverResponse response = Solve(cp_model.Build());
  // [END solve]

  // Print solution.
  // [START print_solution]
  if (response.status() == CpSolverStatus::INFEASIBLE) {
    LOG(FATAL) << "No solution found.";
  }

  LOG(INFO) << "Total cost: " << response.objective_value();
  LOG(INFO);
  for (int i = 0; i < num_workers; ++i) {
    for (int j = 0; j < num_tasks; ++j) {
      if (SolutionBooleanValue(response, x[i][j])) {
        LOG(INFO) << "Task " << i << " assigned to worker " << j
                  << ".  Cost: " << costs[i][j];
      }
    }
  }
  // [END print_solution]
}
}  // namespace sat
}  // namespace operations_research

int main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, true);
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  operations_research::sat::IntegerProgrammingExample();
  return EXIT_SUCCESS;
}
// [END program]
