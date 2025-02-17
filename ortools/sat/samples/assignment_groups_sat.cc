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
// Solve assignment problem for given group of workers.
// [START import]
#include <stdlib.h>

#include <cstdint>
#include <numeric>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"

// [END import]
namespace operations_research {
namespace sat {

void AssignmentGroups() {
  // Data
  // [START data]
  const std::vector<std::vector<int>> costs = {{
      {{90, 76, 75, 70, 50, 74}},
      {{35, 85, 55, 65, 48, 101}},
      {{125, 95, 90, 105, 59, 120}},
      {{45, 110, 95, 115, 104, 83}},
      {{60, 105, 80, 75, 59, 62}},
      {{45, 65, 110, 95, 47, 31}},
      {{38, 51, 107, 41, 69, 99}},
      {{47, 85, 57, 71, 92, 77}},
      {{39, 63, 97, 49, 118, 56}},
      {{47, 101, 71, 60, 88, 109}},
      {{17, 39, 103, 64, 61, 92}},
      {{101, 45, 83, 59, 92, 27}},
  }};
  const int num_workers = static_cast<int>(costs.size());
  std::vector<int> all_workers(num_workers);
  std::iota(all_workers.begin(), all_workers.end(), 0);

  const int num_tasks = static_cast<int>(costs[0].size());
  std::vector<int> all_tasks(num_tasks);
  std::iota(all_tasks.begin(), all_tasks.end(), 0);
  // [END data]

  // Allowed groups of workers:
  // [START allowed_groups]
  const std::vector<std::vector<int64_t>> group1 = {{
      {{0, 0, 1, 1}},  // Workers 2, 3
      {{0, 1, 0, 1}},  // Workers 1, 3
      {{0, 1, 1, 0}},  // Workers 1, 2
      {{1, 1, 0, 0}},  // Workers 0, 1
      {{1, 0, 1, 0}},  // Workers 0, 2
  }};

  const std::vector<std::vector<int64_t>> group2 = {{
      {{0, 0, 1, 1}},  // Workers 6, 7
      {{0, 1, 0, 1}},  // Workers 5, 7
      {{0, 1, 1, 0}},  // Workers 5, 6
      {{1, 1, 0, 0}},  // Workers 4, 5
      {{1, 0, 0, 1}},  // Workers 4, 7
  }};

  const std::vector<std::vector<int64_t>> group3 = {{
      {{0, 0, 1, 1}},  // Workers 10, 11
      {{0, 1, 0, 1}},  // Workers 9, 11
      {{0, 1, 1, 0}},  // Workers 9, 10
      {{1, 0, 1, 0}},  // Workers 8, 10
      {{1, 0, 0, 1}},  // Workers 8, 11
  }};
  // [END allowed_groups]

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
    cp_model.AddAtMostOne(x[worker]);
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

  // [START assignments]
  // Create variables for each worker, indicating whether they work on some
  // task.
  std::vector<IntVar> work(num_workers);
  for (int worker : all_workers) {
    work[worker] = IntVar(
        cp_model.NewBoolVar().WithName(absl::StrFormat("work[%d]", worker)));
  }

  for (int worker : all_workers) {
    LinearExpr task_sum;
    for (int task : all_tasks) {
      task_sum += x[worker][task];
    }
    cp_model.AddEquality(work[worker], task_sum);
  }

  // Define the allowed groups of worders
  auto table1 =
      cp_model.AddAllowedAssignments({work[0], work[1], work[2], work[3]});
  for (const auto& t : group1) {
    table1.AddTuple(t);
  }
  auto table2 =
      cp_model.AddAllowedAssignments({work[4], work[5], work[6], work[7]});
  for (const auto& t : group2) {
    table2.AddTuple(t);
  }
  auto table3 =
      cp_model.AddAllowedAssignments({work[8], work[9], work[10], work[11]});
  for (const auto& t : group3) {
    table3.AddTuple(t);
  }
  // [END assignments]

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

int main(int argc, char** argv) {
  operations_research::sat::AssignmentGroups();
  return EXIT_SUCCESS;
}
// [END program]
