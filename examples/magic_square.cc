// Copyright 2010 Google
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
// Magic square problem
//
// Solves the problem where all numbers in an nxn array have to be different
// while the sums on diagonals, rows, and columns have to be the same.
// The problem is trivial for odd orders, but not for even orders.
// We do not handle odd orders with the trivial method here.

#include "base/commandlineflags.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solver.h"

DEFINE_int32(size, 0, "Size of the magic square");
DEFINE_bool(impact, false, "Use impact search");
DEFINE_int32(impact_size, 30, "Default size of impact search");
DEFINE_int32(restart, -1, "parameter for constant restart monitor");
DEFINE_bool(luby, false, "Use luby sequence instead of constant restart");

namespace operations_research {

void MagicSquare(int grid_size) {
  Solver solver("magicsquare");
  const int total_size = grid_size * grid_size;
  const int sum = grid_size * (total_size + 1) / 2;
  // create the variables
  vector<IntVar*> vars;
  solver.MakeIntVarArray(total_size, 1, total_size, "", &vars);
  solver.AddConstraint(solver.MakeAllDifferent(vars, true));

  // create the constraints
  vector<IntVar*> diag1(grid_size);
  vector<IntVar*> diag2(grid_size);
  for (int n = 0; n < grid_size; ++n) {
    vector<IntVar *> sub_set(grid_size);

    for (int m = 0; m < grid_size; ++m) {    // extract row indices
      sub_set[m] = vars[m + n * grid_size];
    }
    solver.AddConstraint(solver.MakeSumEquality(sub_set, sum));

    for (int m = 0; m < grid_size; ++m) {
      sub_set[m] = vars[m * grid_size + n];  // extract column indices
    }
    solver.AddConstraint(solver.MakeSumEquality(sub_set, sum));
    diag1[n] = vars[n + n * grid_size];      // extract first diagonal indices
    diag2[n] = vars[(grid_size - 1 - n) + n * grid_size];  // second diagonal
  }
  solver.AddConstraint(solver.MakeSumEquality(diag1, sum));
  solver.AddConstraint(solver.MakeSumEquality(diag2, sum));

  // To break a simple symmetry: the upper right corner
  // must be less than the lower left corner
  solver.AddConstraint(solver.MakeLess(vars[grid_size - 1],
                                       vars[(grid_size - 1) * grid_size]));
  // TODO(user) use local search

  DecisionBuilder* const db = FLAGS_impact?
      solver.MakeImpactPhase(vars, FLAGS_impact_size):
      solver.MakePhase(vars,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE);

  SearchMonitor* const log = solver.MakeSearchLog(100000);
  SearchMonitor* const restart = FLAGS_restart != -1?
      (FLAGS_luby?
       solver.MakeLubyRestart(FLAGS_restart):
       solver.MakeConstantRestart(FLAGS_restart)):
      NULL;
  solver.NewSearch(db, log, restart);
  if (solver.NextSolution()) {
    for (int n = 0; n < grid_size; ++n) {
      string output;
      for (int m = 0; m < grid_size; ++m) {   // extract row indices
        int64 v = vars[n * grid_size + m]->Value();
        StringAppendF(&output, "%3lld ", v);
      }
      LG << output;
    }
    LG << "";
  } else {
    LG << "No solution found!";
  }
  solver.EndSearch();
}

}  // namespace operations_research

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_size != 0) {
    operations_research::MagicSquare(FLAGS_size);
  } else {
    for (int n = 3; n < 6; ++n) {
      operations_research::MagicSquare(n);
    }
  }
  return 0;
}
