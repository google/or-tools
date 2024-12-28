// Copyright 2010-2024 Google LLC
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
#include "ortools/graph/assignment.h"

#include <cstdint>
#include <numeric>
#include <string>
#include <vector>

#include "ortools/base/logging.h"
// [END import]

namespace operations_research {
// Simple Linear Sum Assignment Problem (LSAP).
void AssignmentLinearSumAssignment() {
  // [START solver]
  SimpleLinearSumAssignment assignment;
  // [END solver]

  // [START data]
  const int num_workers = 4;
  std::vector<int> all_workers(num_workers);
  std::iota(all_workers.begin(), all_workers.end(), 0);

  const int num_tasks = 4;
  std::vector<int> all_tasks(num_tasks);
  std::iota(all_tasks.begin(), all_tasks.end(), 0);

  const std::vector<std::vector<int>> costs = {{
      {{90, 76, 75, 70}},    // Worker 0
      {{35, 85, 55, 65}},    // Worker 1
      {{125, 95, 90, 105}},  // Worker 2
      {{45, 110, 95, 115}},  // Worker 3
  }};
  // [END data]

  // [START constraints]
  for (int w : all_workers) {
    for (int t : all_tasks) {
      if (costs[w][t]) {
        assignment.AddArcWithCost(w, t, costs[w][t]);
      }
    }
  }
  // [END constraints]

  // [START solve]
  SimpleLinearSumAssignment::Status status = assignment.Solve();
  // [END solve]

  // [START print_solution]
  if (status == SimpleLinearSumAssignment::OPTIMAL) {
    LOG(INFO) << "Total cost: " << assignment.OptimalCost();
    for (int worker : all_workers) {
      LOG(INFO) << "Worker " << std::to_string(worker) << " assigned to task "
                << std::to_string(assignment.RightMate(worker)) << ". Cost: "
                << std::to_string(assignment.AssignmentCost(worker)) << ".";
    }
  } else {
    LOG(INFO) << "Solving the linear assignment problem failed.";
  }
  // [END print_solution]
}

}  // namespace operations_research

int main() {
  operations_research::AssignmentLinearSumAssignment();
  return EXIT_SUCCESS;
}
// [END program]
