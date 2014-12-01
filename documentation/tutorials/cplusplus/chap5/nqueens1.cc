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
// Implementation of a basic model to count all the solutions.

#include "base/commandlineflags.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solver.h"

#include "nqueens_utilities.h"

DEFINE_int32(size, 0,
  "Size of the problem. If equal to 0, will test several increasing sizes.");
DECLARE_bool(use_symmetry);
DECLARE_bool(print_all);

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

  //  Collect solutions
  SolutionCollector* const solution_counter = s.MakeAllSolutionCollector(NULL);


  SolutionCollector* const collector = s.MakeFirstSolutionCollector();
  SolutionCollector* const mega_collector = s.MakeAllSolutionCollector();

  if (FLAGS_print_all) {
    mega_collector->Add(queens);
  }

  collector->Add(queens);
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(solution_counter);
  monitors.push_back(collector);
  if (FLAGS_print_all) {
    monitors.push_back(mega_collector);
  }

  //  DecisionBuilder
  DecisionBuilder* const db = s.MakePhase(queens,
                                          Solver::CHOOSE_FIRST_UNBOUND,
                                          Solver::ASSIGN_MIN_VALUE);

  //  Solve
  s.Solve(db, monitors);  // go!

  //  Check solutions
  const int num_solutions = solution_counter->solution_count();
  CheckNumberOfSolutions(size, num_solutions);

  //  Report
  const int64 time = s.wall_time();

  std::cout << "============================" << std::endl;
  std::cout << "size: " << size << std::endl;
  std::cout << "The Solve method took "
                                 << time/1000.0 << " seconds" << std::endl;
  std::cout << "number of solutions: " << num_solutions << std::endl;
  PrintFirstSolution(size, queens, collector);
  PrintAllSolutions(size, queens, mega_collector);
}
}   // namespace operations_research

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_use_symmetry) {
    LOG(FATAL) << "Symmetries not yet implemented!";
  }
  if (FLAGS_size != 0) {
    operations_research::NQueens(FLAGS_size);
  } else {
    for (int n = 1; n < 12; ++n) {
      operations_research::NQueens(n);
    }
  }
  return 0;
}
