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
// Solve assignment problem.
// [START import]
#include <stdlib.h>

#include <cstdint>
#include <numeric>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/strings/str_format.h"
#include "ortools/base/init_google.h"
#include "ortools/base/log_severity.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"

// [END import]
namespace operations_research {
namespace sat {

void AssignmentTaskSizes() {
  // Data
  // [START data]
  const std::vector<std::vector<int>> costs = {{
      {{90, 76, 75, 70, 50, 74, 12, 68}},
      {{35, 85, 55, 65, 48, 101, 70, 83}},
      {{125, 95, 90, 105, 59, 120, 36, 73}},
      {{45, 110, 95, 115, 104, 83, 37, 71}},
      {{60, 105, 80, 75, 59, 62, 93, 88}},
      {{45, 65, 110, 95, 47, 31, 81, 34}},
      {{38, 51, 107, 41, 69, 99, 115, 48}},
      {{47, 85, 57, 71, 92, 77, 109, 36}},
      {{39, 63, 97, 49, 118, 56, 92, 61}},
      {{47, 101, 71, 60, 88, 109, 52, 90}},
  }};
  const int num_workers = static_cast<int>(costs.size());
  std::vector<int> all_workers(num_workers);
  std::iota(all_workers.begin(), all_workers.end(), 0);

  const int num_tasks = static_cast<int>(costs[0].size());
  std::vector<int> all_tasks(num_tasks);
  std::iota(all_tasks.begin(), all_tasks.end(), 0);

  const std::vector<int64_t> task_sizes = {{10, 7, 3, 12, 15, 4, 11, 5}};
  // Maximum total of task sizes for any worker
  const int total_size_max = 15;
  // [END data]

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
  for (int worker : all_workers) {
    for (int task : all_tasks) {
      x[worker][task] = cp_model.NewBoolVar().WithName(
          absl::StrFormat("x[%d,%d]", worker, task));
    }
  }
  // [END variables]

  // Constraints
  // [START constraints]
  // Each worker is assigned to at most one task.
  for (int worker : all_workers) {
    LinearExpr task_sum;
    for (int task : all_tasks) {
      task_sum += x[worker][task] * task_sizes[task];
    }
    cp_model.AddLessOrEqual(task_sum, total_size_max);
  }
  // Each task is assigned to exactly one worker.
  for (int task : all_tasks) {
    std::vector<BoolVar> tasks;
    for (int worker : all_workers) {
      tasks.push_back(x[worker][task]);
    }
    cp_model.AddExactlyOne(tasks);
  }
  // [END constraints]

  // Objective
  // [START objective]
  LinearExpr total_cost;
  for (int worker : all_workers) {
    for (int task : all_tasks) {
      total_cost += x[worker][task] * costs[worker][task];
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
  for (int worker : all_workers) {
    for (int task : all_tasks) {
      if (SolutionBooleanValue(response, x[worker][task])) {
        LOG(INFO) << "Worker " << worker << " assigned to task " << task
                  << ".  Cost: " << costs[worker][task];
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
  operations_research::sat::AssignmentTaskSizes();
  return EXIT_SUCCESS;
}
// [END program]
