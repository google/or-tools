// Copyright 2010-2017 Google
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
// N-queens problem
//
//  unique solutions: http://www.research.att.com/~njas/sequences/A000170
//  distinct solutions: http://www.research.att.com/~njas/sequences/A002562


#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/constraint_solver/constraint_solver.h"

DEFINE_int32(
    size, 88,
    "Size of the problem. If equal to 0, will test several increasing sizes.");

namespace operations_research {
void NQueens(int size) {
  CHECK_GE(size, 1);
  Solver s("nqueens");

  // model
  std::vector<IntVar*> queens;
  for (int i = 0; i < size; ++i) {
    queens.push_back(s.MakeIntVar(0, size - 1, StringPrintf("queen%04d", i)));
  }
  for (int i = 0; i < size - 1; ++i) {
    for (int j = i + 1; j < size; ++j) {
      s.AddConstraint(s.MakeNonEquality(queens[i], queens[j]));
      s.AddConstraint(s.MakeNonEquality(s.MakeSum(queens[i], i), queens[j]));
      s.AddConstraint(s.MakeNonEquality(s.MakeSum(queens[i], -i), queens[j]));
    }
  }
  std::vector<SearchMonitor*> monitors;
  DecisionBuilder* const db = s.MakePhase(queens, Solver::CHOOSE_FIRST_UNBOUND,
                                          Solver::ASSIGN_MIN_VALUE);
  monitors.push_back(s.MakeSearchLog(1000000));
  s.Solve(db, monitors);  // go!
}
}  // namespace operations_research

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags( &argc, &argv, true);
  operations_research::NQueens(FLAGS_size);
  return 0;
}
