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
#include <cstdlib>

#include "ortools/base/logging.h"
#include "ortools/set_cover/set_cover_heuristics.h"
#include "ortools/set_cover/set_cover_invariant.h"
#include "ortools/set_cover/set_cover_model.h"
// [END import]

namespace operations_research {

void SimpleSetCoverProgram() {
  // [START data]
  SetCoverModel model;
  model.AddEmptySubset(2.0);
  model.AddElementToLastSubset(0);
  model.AddEmptySubset(2.0);
  model.AddElementToLastSubset(1);
  model.AddEmptySubset(1.0);
  model.AddElementToLastSubset(0);
  model.AddElementToLastSubset(1);
  // [END data]

  // [START solve]
  SetCoverInvariant inv(&model);
  GreedySolutionGenerator greedy(&inv);
  bool found_solution = greedy.NextSolution();
  if (!found_solution) {
    LOG(INFO) << "No solution found by the greedy heuristic.";
    return;
  }
  SetCoverSolutionResponse solution = inv.ExportSolutionAsProto();
  // [END solve]

  // [START print_solution]
  LOG(INFO) << "Total cost: " << solution.cost();  // == inv.cost()
  LOG(INFO) << "Total number of selected subsets: " << solution.num_subsets();
  LOG(INFO) << "Chosen subsets:";
  for (int i = 0; i < solution.subset_size(); ++i) {
    LOG(INFO) << "  " << solution.subset(i);
  }
  // [END print_solution]
}

}  // namespace operations_research

int main() {
  operations_research::SimpleSetCoverProgram();
  return EXIT_SUCCESS;
}
// [END program]
