// Copyright 2010-2011 Google
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
//This model implements a simple jobshop named ft06.
//
// A jobshop is a standard scheduling problem when you must sequence a
// series of tasks on a set of machines. Each job contains one task per
// machine. The order of execution and the length of each job on each
// machine is task dependent.
//
// The objective is to minimize the maximum completion time of all
// jobs. This is called the makespan.


#include <cstdio>
#include <map>

#include "base/commandlineflags.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "constraint_solver/constraint_solver.h"

namespace operations_research {
void Jobshop() {
  Solver solver("jobshop");
  const int kMachineCount = 6;
  const int kJobCount = 6;
  const int durations[kJobCount][kMachineCount] = {{1, 3, 6, 7, 3, 6},
                                                   {8, 5, 10, 10, 10, 4},
                                                   {5, 4, 8, 9, 1, 7},
                                                   {5, 5, 5, 3, 8, 9},
                                                   {9, 3, 5, 4, 3, 1},
                                                   {3, 3, 9, 10, 4, 1}};

  const int machines[kJobCount][kMachineCount] = {{2, 0, 1, 3, 5, 4},
                                                  {1, 2, 4, 5, 0, 3},
                                                  {2, 3, 5, 0, 1, 4},
                                                  {1, 0, 2, 3, 4, 5},
                                                  {2, 1, 4, 5, 0, 3},
                                                  {1, 3, 5, 0, 4, 2}};

  const int kHorizon = 200;
  std::vector<std::vector<IntervalVar*> > all_tasks(kJobCount);
  for (int i = 0; i < kJobCount; ++i) {
    for (int j = 0; j < kMachineCount; ++j) {
      all_tasks[i].push_back(
          solver.MakeFixedDurationIntervalVar(0,
                                              kHorizon,
                                              durations[i][j],
                                              false,
                                              StringPrintf("Job_%i_%i", i, j)));
    }
  }
  std::vector<Sequence*> all_sequences;
  for (int i = 0; i < kMachineCount; ++i) {
    std::vector<IntervalVar*> machine_jobs;
    for (int j = 0; j < kJobCount; ++j) {
      for(int k = 0; k < kMachineCount; ++k) {
        if (machines[j][k] == i) {
          machine_jobs.push_back(all_tasks[j][k]);
        }
      }
    }
    all_sequences.push_back(
        solver.MakeSequence(machine_jobs, StringPrintf("machine %i", i)));
  }
  std::vector<IntVar*> all_ends;
  for (int i = 0; i < kJobCount; ++i) {
    all_ends.push_back(all_tasks[i][kMachineCount - 1]->EndExpr()->Var());
  }
  IntVar* const obj_var = solver.MakeMax(all_ends)->Var();
  OptimizeVar* const objective = solver.MakeMinimize(obj_var, 1);

  for (int i = 0; i < kJobCount; ++i) {
    for (int j = 0; j < kMachineCount - 1; ++j) {
      solver.AddConstraint(
          solver.MakeIntervalVarRelation(all_tasks[i][j + 1],
                                         Solver::STARTS_AFTER_END,
                                         all_tasks[i][j]));
    }
  }

  for (int i = 0; i < kMachineCount; ++i) {
    solver.AddConstraint(all_sequences[i]);
    LOG(INFO) << all_sequences[i]->name();
  }

  DecisionBuilder* const vars_phase =
      solver.MakePhase(obj_var,
                       Solver::CHOOSE_FIRST_UNBOUND,
                       Solver::ASSIGN_MIN_VALUE);
  DecisionBuilder* const sequence_phase =
      solver.MakePhase(all_sequences, Solver::SEQUENCE_DEFAULT);
  DecisionBuilder* const main_phase =
      solver.Compose(sequence_phase, vars_phase);

  SearchMonitor* const search_log = solver.MakeSearchLog(100, objective);
  solver.Solve(main_phase, search_log, objective);
}
}  // namespace operations_research

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  operations_research::Jobshop();
  return 0;
}
