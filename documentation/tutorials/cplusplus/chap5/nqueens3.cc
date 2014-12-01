// Copyright 2010-2014 Google
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
//
// n-queens problem
//
// Implementation of a basic model to fetch the first feasible solution.
// Use of cpviz to visualize the search.

#include "base/commandlineflags.h"
#include "base/logging.h"
#include "base/stringprintf.h"

#include "nqueens_utilities.h"

DECLARE_bool(use_symmetry);

namespace operations_research {

void NQueens(int size) {
  CHECK_GE(size, 1);
  Solver s("nqueens");

  // model
  std::vector<IntVar*> queens;
  for (int i = 0; i < size; ++i) {
    queens.push_back(s.MakeIntVar(0, size - 1, StringPrintf("queen%04d", i)));
  }
  s.AddConstraint(s.MakeAllDifferent(queens));

  std::vector<IntVar*> vars(size);
  for (int i = 0; i < size; ++i) {
    vars[i] = s.MakeSum(queens[i], i)->Var();
  }
  s.AddConstraint(s.MakeAllDifferent(vars));
  for (int i = 0; i < size; ++i) {
    vars[i] = s.MakeSum(queens[i], -i)->Var();
  }
  s.AddConstraint(s.MakeAllDifferent(vars));

  vector<SearchMonitor*> monitors;

  SolutionCollector* const collector = s.MakeFirstSolutionCollector();
  collector->Add(queens);
  monitors.push_back(collector);

  SearchMonitor* const cpviz = s.MakeTreeMonitor(queens, "tree.xml",
                                                       "visualization.xml");
  monitors.push_back(cpviz);

  DecisionBuilder* const db = s.MakePhase(queens,
                                          Solver::CHOOSE_FIRST_UNBOUND,
                                          Solver::ASSIGN_MIN_VALUE);

  s.Solve(db, monitors);  // go!

  const int num_solutions = collector->solution_count();
  CHECK_EQ(num_solutions, 1);
  const int64 time = s.wall_time();

  std::cout << "============================" << std::endl;
  std::cout << "size: " << size << std::endl;
  std::cout << "The Solve method took "
                                  << time/1000.0 << " seconds" << std::endl;
  std::cout << "number of solutions: " << num_solutions << std::endl;
  PrintFirstSolution(size, queens, collector);
}
}   // namespace operations_research

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_use_symmetry) {
    LOG(FATAL) << "Symmetries not yet implemented!";
  }
  operations_research::NQueens(4);
  return 0;
}
