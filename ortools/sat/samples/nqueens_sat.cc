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
// OR-Tools solution to the N-queens problem.
// [START import]
#include <cstdint>
#include <vector>

#include "absl/strings/numbers.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
// [END import]

namespace operations_research {
namespace sat {

void NQueensSat(const int board_size) {
  // Instantiate the solver.
  // [START model]
  CpModelBuilder cp_model;
  // [END model]

  // [START variables]
  std::vector<IntVar> queens;
  queens.reserve(board_size);
  Domain range(0, board_size - 1);
  for (int i = 0; i < board_size; ++i) {
    queens.push_back(
        cp_model.NewIntVar(range).WithName("x" + std::to_string(i)));
  }
  // [END variables]

  // Define constraints.
  // [START constraints]
  // The following sets the constraint that all queens are in different rows.
  cp_model.AddAllDifferent(queens);

  // All columns must be different because the indices of queens are all
  // different. No two queens can be on the same diagonal.
  std::vector<LinearExpr> diag_1;
  diag_1.reserve(board_size);
  std::vector<LinearExpr> diag_2;
  diag_2.reserve(board_size);
  for (int i = 0; i < board_size; ++i) {
    diag_1.push_back(queens[i] + i);
    diag_2.push_back(queens[i] - i);
  }
  cp_model.AddAllDifferent(diag_1);
  cp_model.AddAllDifferent(diag_2);
  // [END constraints]

  // [START solution_printer]
  int num_solutions = 0;
  Model model;
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& response) {
    LOG(INFO) << "Solution " << num_solutions;
    for (int i = 0; i < board_size; ++i) {
      std::stringstream ss;
      for (int j = 0; j < board_size; ++j) {
        if (SolutionIntegerValue(response, queens[j]) == i) {
          // There is a queen in column j, row i.
          ss << "Q";
        } else {
          ss << "_";
        }
        if (j != board_size - 1) ss << " ";
      }
      LOG(INFO) << ss.str();
    }
    num_solutions++;
  }));
  // [END solution_printer]

  // [START solve]
  // Tell the solver to enumerate all solutions.
  SatParameters parameters;
  parameters.set_enumerate_all_solutions(true);
  model.Add(NewSatParameters(parameters));

  const CpSolverResponse response = SolveCpModel(cp_model.Build(), &model);
  LOG(INFO) << "Number of solutions found: " << num_solutions;
  // [END solve]

  // Statistics.
  // [START statistics]
  LOG(INFO) << "Statistics";
  LOG(INFO) << CpSolverResponseStats(response);
  // [END statistics]
}

}  // namespace sat
}  // namespace operations_research

int main(int argc, char** argv) {
  int board_size = 8;
  if (argc > 1) {
    CHECK(absl::SimpleAtoi(argv[1], &board_size));
  }
  operations_research::sat::NQueensSat(board_size);
  return EXIT_SUCCESS;
}
// [END program]
