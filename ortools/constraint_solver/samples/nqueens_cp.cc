// Copyright 2011-2021 Google LLC
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
#include <cstdlib>
#include <sstream>
#include <vector>

#include "ortools/constraint_solver/constraint_solver.h"
// [END import]

namespace operations_research {

  void NQueensCp(const int board_size) {
    // Instantiate the solver.
    // [START solver]
    Solver solver("N-Queens");
    // [END solver]

    // [START variables]
    std::vector<IntVar*> queens;
    queens.reserve(board_size);
    for (int i=0; i < board_size; ++i) {
      queens.push_back(solver.MakeIntVar(0, board_size - 1, absl::StrCat("x",i)));
    }
    // [END variables]

    // Define constraints.
    // [START constraints]
    // The following sets the constraint that all queens are in different rows.
    solver.AddConstraint(solver.MakeAllDifferent(queens));

    // All columns must be different because the indices of queens are all different.
    // No two queens can be on the same diagonal.
    std::vector<IntVar*> diag_1;
    diag_1.reserve(board_size);
    std::vector<IntVar*> diag_2;
    diag_2.reserve(board_size);
    for (int i=0; i < board_size; ++i) {
      diag_1.push_back(solver.MakeSum(queens[i], i)->Var());
      diag_2.push_back(solver.MakeSum(queens[i], -i)->Var());
    }
    solver.AddConstraint(solver.MakeAllDifferent(diag_1));
    solver.AddConstraint(solver.MakeAllDifferent(diag_2));
    // [END constraints]

    // [START db]
    DecisionBuilder* const db = solver.MakePhase(
        queens, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
    // [END db]

    // [START solve]
    // Iterates through the solutions, displaying each.
    int num_solutions = 0;

    solver.NewSearch(db);
    while (solver.NextSolution()) {
      // Displays the solution just computed.
      LOG(INFO) << "Solution " << num_solutions;
      for (int i=0; i < board_size; ++i) {
        std::stringstream ss;
        for (int j=0; j < board_size; ++j) {
          if (queens[j]->Value() == i) {
            // There is a queen in column j, row i.
            ss << "Q";
          } else {
            ss << "_";
          }
          if (j != board_size-1) ss << " ";
        }
        LOG(INFO) << ss.str();
      }
      num_solutions++;
    }
    solver.EndSearch();
    // [END solve]

    // Statistics.
    // [START statistics]
    LOG(INFO) << "Statistics";
    LOG(INFO) << "  failures: " << solver.failures();
    LOG(INFO) << "  branches: " << solver.branches();
    LOG(INFO) << "  wall time: " << solver.wall_time() << " ms";
    LOG(INFO) << "  Solutions found: " << num_solutions;
    // [END statistics]
  }

}  // namespace operations_research

int main(int argc, char** argv) {
  int board_size = 8;
  if (argc > 1) {
    board_size = std::atoi(argv[1]);
  }
  operations_research::NQueensCp(board_size);
  return EXIT_SUCCESS;
}
// [END program]
