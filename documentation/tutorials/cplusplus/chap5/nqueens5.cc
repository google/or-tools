// Copyright 2011-2014 Google
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
//
// n-Queens Problem
//
// Use of customized search strategies with the help of callbacks to select variables
// and assign a value to it.

#include <math.h>  // abs

#include "base/join.h"
#include "base/commandlineflags.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solver.h"

#include "nqueens_utilities.h"

DEFINE_int32(size, 4, "Size of the problem.");
DECLARE_bool(use_symmetry);

namespace operations_research {

typedef ResultCallback1<int64, int64> IndexEvaluator1;

class MiddleVariableIndexSelector : public IndexEvaluator1 {
public:
  MiddleVariableIndexSelector(const int64 n): n_(n), middle_var_index_((n-1)/2) {}
  ~MiddleVariableIndexSelector() {}
  int64 Run(int64 index) {
    return abs(middle_var_index_ - index);
  }

private:
  const int64 n_;
  const int64 middle_var_index_;
};

void NQueens(int size) {
  CHECK_GE(size, 1);
  Solver s("nqueens");

  // model
  std::vector<IntVar*> queens;
  for (int i = 0; i < size; ++i) {
    queens.push_back(s.MakeIntVar(0, size - 1, StringPrintf("x%04d", i)));
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

  SolutionCollector* const solution_counter = s.MakeAllSolutionCollector(NULL);
  monitors.push_back(solution_counter);
  SolutionCollector* const collector = s.MakeFirstSolutionCollector();
  collector->Add(queens);
  monitors.push_back(collector);

  MiddleVariableIndexSelector * index_evaluator = new MiddleVariableIndexSelector(size);

  DecisionBuilder* const db = s.MakePhase(queens,
                                          index_evaluator,
                                          Solver::ASSIGN_CENTER_VALUE); //  ASSIGN_CENTER_VALUE, ASSIGN_MIN_VALUE

  s.Solve(db, monitors);  // go!
  CheckNumberOfSolutions(size, solution_counter->solution_count());

  const int num_solutions = solution_counter->solution_count();
  const int64 time = s.wall_time();

  std::cout << "============================" << std::endl;
  std::cout << "size: " << size << std::endl;
  std::cout << "The Solve method took "
                                  << time/1000.0 << " seconds" << std::endl;
  std::cout << "Number of solutions: " << num_solutions << std::endl;
  std::cout << "Failures: " << s.failures() << std::endl;
  std::cout << "Branches: " << s.branches() << std::endl;
  std::cout << "Backtracks: " << s.fail_stamp() << std::endl;
  std::cout << "Stamps: " << s.stamp() << std::endl;
  PrintFirstSolution(size, queens, collector);
}
}   // namespace operations_research


int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_use_symmetry) {
    LOG(FATAL) << "Symmetries not yet implemented!";
  }
  operations_research::NQueens(FLAGS_size);
  return 0;
}
