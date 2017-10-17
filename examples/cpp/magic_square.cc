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
// Magic square problem
//
// Solves the problem where all numbers in an nxn array have to be different
// while the sums on diagonals, rows, and columns have to be the same.
// The problem is trivial for odd orders, but not for even orders.
// We do not handle odd orders with the trivial method here.

#include "ortools/base/commandlineflags.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/constraint_solver/constraint_solver.h"

DEFINE_int32(size, 0, "Size of the magic square.");
DEFINE_bool(impact, false, "Use impact search.");
DEFINE_int32(restart, -1, "Parameter for constant restart monitor.");
DEFINE_bool(luby, false,
            "Use luby restart monitor instead of constant restart monitor.");
DEFINE_bool(run_all_heuristics, false, "Run all heuristics.");
DEFINE_int32(heuristics_period, 200, "Frequency to run all heuristics.");
DEFINE_int32(choose_var_strategy, 0,
             "Selection strategy for variable: 0 = max sum impact, "
             "1 = max average impact, "
             "2 = max individual impact.");
DEFINE_bool(select_max_impact_value, false,
            "Select the value with max impact instead of min impact.");
DEFINE_double(restart_log_size, -1.0,
              "Threshold for automatic restarting the search in default"
              " phase.");
DEFINE_bool(verbose_impact, false, "Verbose output of impact search.");
DEFINE_bool(use_nogoods, false, "Use no goods in automatic restart.");

namespace operations_research {

void MagicSquare(int grid_size) {
  Solver solver("magicsquare");
  const int total_size = grid_size * grid_size;
  const int sum = grid_size * (total_size + 1) / 2;
  // create the variables
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(total_size, 1, total_size, "", &vars);
  solver.AddConstraint(solver.MakeAllDifferent(vars));

  // create the constraints
  std::vector<IntVar*> diag1(grid_size);
  std::vector<IntVar*> diag2(grid_size);
  for (int n = 0; n < grid_size; ++n) {
    std::vector<IntVar*> sub_set(grid_size);

    for (int m = 0; m < grid_size; ++m) {  // extract row indices
      sub_set[m] = vars[m + n * grid_size];
    }
    solver.AddConstraint(solver.MakeSumEquality(sub_set, sum));

    for (int m = 0; m < grid_size; ++m) {
      sub_set[m] = vars[m * grid_size + n];  // extract column indices
    }
    solver.AddConstraint(solver.MakeSumEquality(sub_set, sum));
    diag1[n] = vars[n + n * grid_size];  // extract first diagonal indices
    diag2[n] = vars[(grid_size - 1 - n) + n * grid_size];  // second diagonal
  }
  solver.AddConstraint(solver.MakeSumEquality(diag1, sum));
  solver.AddConstraint(solver.MakeSumEquality(diag2, sum));

  // To break a simple symmetry: the upper right corner
  // must be less than the lower left corner
  solver.AddConstraint(
      solver.MakeLess(vars[grid_size - 1], vars[(grid_size - 1) * grid_size]));
  // TODO(user) use local search

  DefaultPhaseParameters parameters;
  parameters.run_all_heuristics = FLAGS_run_all_heuristics;
  parameters.heuristic_period = FLAGS_heuristics_period;
  parameters.restart_log_size = FLAGS_restart_log_size;
  parameters.display_level = FLAGS_verbose_impact
                                 ? DefaultPhaseParameters::VERBOSE
                                 : DefaultPhaseParameters::NORMAL;
  parameters.use_no_goods = FLAGS_use_nogoods;
  switch (FLAGS_choose_var_strategy) {
    case 0: {
      parameters.var_selection_schema =
          DefaultPhaseParameters::CHOOSE_MAX_SUM_IMPACT;
      break;
    }
    case 1: {
      parameters.var_selection_schema =
          DefaultPhaseParameters::CHOOSE_MAX_AVERAGE_IMPACT;
      break;
    }
    case 2: {
      parameters.var_selection_schema =
          DefaultPhaseParameters::CHOOSE_MAX_VALUE_IMPACT;
      break;
    }
    default: { LOG(FATAL) << "Should not be here"; }
  }
  parameters.value_selection_schema =
      FLAGS_select_max_impact_value ? DefaultPhaseParameters::SELECT_MAX_IMPACT
                                    : DefaultPhaseParameters::SELECT_MIN_IMPACT;

  DecisionBuilder* const db =
      FLAGS_impact ? solver.MakeDefaultPhase(vars, parameters)
                   : solver.MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                                      Solver::ASSIGN_MIN_VALUE);

  std::vector<SearchMonitor*> monitors;
  SearchMonitor* const log = solver.MakeSearchLog(100000);
  monitors.push_back(log);
  SearchMonitor* const restart =
      FLAGS_restart != -1
          ? (FLAGS_luby ? solver.MakeLubyRestart(FLAGS_restart)
                        : solver.MakeConstantRestart(FLAGS_restart))
          : nullptr;
  if (restart) {
    monitors.push_back(restart);
  }
  solver.NewSearch(db, monitors);
  if (solver.NextSolution()) {
    for (int n = 0; n < grid_size; ++n) {
      std::string output;
      for (int m = 0; m < grid_size; ++m) {  // extract row indices
        int64 v = vars[n * grid_size + m]->Value();
        StringAppendF(&output, "%3lld ", v);
      }
      LOG(INFO) << output;
    }
    LOG(INFO) << "";
  } else {
    LOG(INFO) << "No solution found!";
  }
  solver.EndSearch();
}

}  // namespace operations_research

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags( &argc, &argv, true);
  if (FLAGS_size != 0) {
    operations_research::MagicSquare(FLAGS_size);
  } else {
    for (int n = 3; n < 6; ++n) {
      operations_research::MagicSquare(n);
    }
  }
  return 0;
}
