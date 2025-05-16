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
#include <cstdint>
#include <memory>
#include <numeric>
#include <vector>

#include "absl/base/log_severity.h"
#include "absl/log/globals.h"
#include "absl/strings/str_format.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/linear_solver/linear_solver.h"
// [END import]

namespace operations_research {
void AssignmentTeamsMip() {
  // Data
  // [START data]
  const std::vector<std::vector<int64_t>> costs = {{
      {{90, 76, 75, 70}},
      {{35, 85, 55, 65}},
      {{125, 95, 90, 105}},
      {{45, 110, 95, 115}},
      {{60, 105, 80, 75}},
      {{45, 65, 110, 95}},
  }};
  const int num_workers = costs.size();
  std::vector<int> all_workers(num_workers);
  std::iota(all_workers.begin(), all_workers.end(), 0);

  const int num_tasks = costs[0].size();
  std::vector<int> all_tasks(num_tasks);
  std::iota(all_tasks.begin(), all_tasks.end(), 0);

  const std::vector<int64_t> team1 = {{0, 2, 4}};
  const std::vector<int64_t> team2 = {{1, 3, 5}};
  // Maximum total of tasks for any team
  const int team_max = 2;
  // [END data]

  // Solver
  // [START solver]
  // Create the mip solver with the SCIP backend.
  std::unique_ptr<MPSolver> solver(MPSolver::CreateSolver("SCIP"));
  if (!solver) {
    LOG(WARNING) << "SCIP solver unavailable.";
    return;
  }
  // [END solver]

  // Variables
  // [START variables]
  // x[i][j] is an array of 0-1 variables, which will be 1
  // if worker i is assigned to task j.
  std::vector<std::vector<const MPVariable*>> x(
      num_workers, std::vector<const MPVariable*>(num_tasks));
  for (int worker : all_workers) {
    for (int task : all_tasks) {
      x[worker][task] =
          solver->MakeBoolVar(absl::StrFormat("x[%d,%d]", worker, task));
    }
  }
  // [END variables]

  // Constraints
  // [START constraints]
  // Each worker is assigned to at most one task.
  for (int worker : all_workers) {
    LinearExpr worker_sum;
    for (int task : all_tasks) {
      worker_sum += x[worker][task];
    }
    solver->MakeRowConstraint(worker_sum <= 1.0);
  }
  // Each task is assigned to exactly one worker.
  for (int task : all_tasks) {
    LinearExpr task_sum;
    for (int worker : all_workers) {
      task_sum += x[worker][task];
    }
    solver->MakeRowConstraint(task_sum == 1.0);
  }

  // Each team takes at most two tasks.
  LinearExpr team1_tasks;
  for (int worker : team1) {
    for (int task : all_tasks) {
      team1_tasks += x[worker][task];
    }
  }
  solver->MakeRowConstraint(team1_tasks <= team_max);

  LinearExpr team2_tasks;
  for (int worker : team2) {
    for (int task : all_tasks) {
      team2_tasks += x[worker][task];
    }
  }
  solver->MakeRowConstraint(team2_tasks <= team_max);
  // [END constraints]

  // Objective.
  // [START objective]
  MPObjective* const objective = solver->MutableObjective();
  for (int worker : all_workers) {
    for (int task : all_tasks) {
      objective->SetCoefficient(x[worker][task], costs[worker][task]);
    }
  }
  objective->SetMinimization();
  // [END objective]

  // Solve
  // [START solve]
  const MPSolver::ResultStatus result_status = solver->Solve();
  // [END solve]

  // Print solution.
  // [START print_solution]
  // Check that the problem has a feasible solution.
  if (result_status != MPSolver::OPTIMAL &&
      result_status != MPSolver::FEASIBLE) {
    LOG(FATAL) << "No solution found.";
  }
  LOG(INFO) << "Total cost = " << objective->Value() << "\n\n";
  for (int worker : all_workers) {
    for (int task : all_tasks) {
      // Test if x[i][j] is 0 or 1 (with tolerance for floating point
      // arithmetic).
      if (x[worker][task]->solution_value() > 0.5) {
        LOG(INFO) << "Worker " << worker << " assigned to task " << task
                  << ".  Cost: " << costs[worker][task];
      }
    }
  }
  // [END print_solution]
}
}  // namespace operations_research

int main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, true);
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  operations_research::AssignmentTeamsMip();
  return EXIT_SUCCESS;
}
// [END program]
