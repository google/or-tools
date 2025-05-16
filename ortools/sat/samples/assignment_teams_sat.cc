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
// Solve a simple assignment problem.
// [START import]
#include <stdlib.h>

#include <numeric>
#include <vector>

#include "absl/base/log_severity.h"
#include "absl/log/globals.h"
#include "absl/strings/str_format.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"

// [END import]
namespace operations_research {
namespace sat {

void AssignmentTeamsSat() {
  // Data
  // [START data]
  const std::vector<std::vector<int>> costs = {{
      {{90, 76, 75, 70}},
      {{35, 85, 55, 65}},
      {{125, 95, 90, 105}},
      {{45, 110, 95, 115}},
      {{60, 105, 80, 75}},
      {{45, 65, 110, 95}},
  }};
  const int num_workers = static_cast<int>(costs.size());
  std::vector<int> all_workers(num_workers);
  std::iota(all_workers.begin(), all_workers.end(), 0);

  const int num_tasks = static_cast<int>(costs[0].size());
  std::vector<int> all_tasks(num_tasks);
  std::iota(all_tasks.begin(), all_tasks.end(), 0);

  const std::vector<int> team1 = {{0, 2, 4}};
  const std::vector<int> team2 = {{1, 3, 5}};
  // Maximum total of tasks for any team
  const int team_max = 2;
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

  // Each team takes at most two tasks.
  LinearExpr team1_tasks;
  for (int worker : team1) {
    for (int task : all_tasks) {
      team1_tasks += x[worker][task];
    }
  }
  cp_model.AddLessOrEqual(team1_tasks, team_max);

  LinearExpr team2_tasks;
  for (int worker : team2) {
    for (int task : all_tasks) {
      team2_tasks += x[worker][task];
    }
  }
  cp_model.AddLessOrEqual(team2_tasks, team_max);
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
  operations_research::sat::AssignmentTeamsSat();
  return EXIT_SUCCESS;
}
// [END program]
