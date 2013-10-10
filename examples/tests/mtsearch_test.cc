// Copyright 2010-2012 Google
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

#include <stddef.h>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/synchronization.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"

DEFINE_int32(workers, 4, "Number of workers for tests");

namespace operations_research {
class UpVar : public BaseLNS {
 public:
  UpVar(const std::vector<IntVar*>& vars, int worker)
      : BaseLNS(vars),  worker_(worker) {}

  virtual ~UpVar() {}

  virtual bool NextFragment(std::vector<int>* fragment) {
    bool all_done = true;
    for (int i = 0; i < Size(); ++i) {
      if (Value(i) == 0) {
        all_done = false;
        break;
      }
    }
    if (all_done) {
      VLOG(1) << "worker " << worker_ << " thinks search should terminate";
      return false;
    }
    fragment->push_back(worker_);
    return true;
  }
 private:
  const int worker_;
};

void BuildModelWithSolution(int* const active,
                            Mutex* const mutex,
                            ParallelSolveSupport* const support,
                            bool master,
                            int worker) {
  Solver s(StringPrintf("Worker_%i", worker));
  IntVar* const x = s.MakeIntVar(0, 10, "x");
  IntVar* const y = s.MakeIntVar(0, 10, "y");
  Assignment* const solution = s.MakeAssignment();
  solution->Add(x);
  solution->Add(y);
  if (master) {
    VLOG(1) << "Master run";
    solution->SetValue(x, 2);
    solution->SetValue(y, 4);
    support->RegisterInitialSolution(solution);
    ThreadSafeIncrement(active, mutex, 2);
    VLOG(1) << "Master initial solution sent";
  } else {
    VLOG(1) << "Slave " << worker;
    CHECK(support->WaitForInitialSolution(solution, worker));
    VLOG(1) << "Worker solution received";
    CHECK_EQ(2, solution->Value(x));
    CHECK_EQ(4, solution->Value(y));
    ThreadSafeIncrement(active, mutex, 1);
  }
}

void BuildModelWithoutSolution(int* const active,
                               Mutex* const mutex,
                               ParallelSolveSupport* const support,
                               bool master,
                               int worker) {
  Solver s(StringPrintf("Worker_%i", worker));
  if (master) {
    support->RegisterNoInitialSolution();
    ThreadSafeIncrement(active, mutex, 2);
  } else {
    Assignment* const solution = s.MakeAssignment();
    CHECK(!support->WaitForInitialSolution(solution, worker));
    ThreadSafeIncrement(active, mutex, 1);
  }
}

void BuildModelWithSearch(int workers,
                          ParallelSolveSupport* const support,
                          bool master,
                          int worker) {
  // Standard model building.
  Solver s(StringPrintf("Worker_%i", worker));
  VLOG(1) << "Worker " << worker  << " started";
  std::vector<IntVar*> vars;
  s.MakeBoolVarArray(workers, "vars", &vars);
  Assignment* const solution = s.MakeAssignment();
  solution->Add(vars);
  std::vector<int> coefficients;
  int obj_max = 0;
  for (int i = 0; i < workers; ++i) {
    const int value = (i + 1) * (i + 1);
    coefficients.push_back(value);
    obj_max += value;
  }
  IntVar* const objective = s.MakeScalProd(vars, coefficients)->Var();
  solution->AddObjective(objective);

  SolutionCollector* const collector =
      master ? s.MakeLastSolutionCollector(solution) : NULL;

  std::vector<SearchMonitor*> monitors;

  // Create or wait for initial solution.
  if (master) {
    // Only the master needs to store solutions.
    monitors.push_back(collector);

    // Creates the initial solution.
    for (int i = 0; i < workers; ++i) {
      solution->SetValue(vars[i], 0);
    }
    solution->SetObjectiveValue(0);
    support->RegisterInitialSolution(solution);
  } else {
    // Workers waits for the initial solution.
    CHECK(support->WaitForInitialSolution(solution, worker));
    CHECK_EQ(0, solution->ObjectiveValue());
  }

  monitors.push_back(s.MakeMaximize(objective, 1));

  UpVar* const local_search_operator =
      s.RevAlloc(new UpVar(vars, worker));

  DecisionBuilder* db = s.MakePhase(vars,
                                    Solver::CHOOSE_FIRST_UNBOUND,
                                    Solver::ASSIGN_MAX_VALUE);

  // The master must run a dedicated decision builder to report
  // solutions found by the workers. The workers run a LNS operator
  // with a customized solution pool.
  DecisionBuilder* const final_db = master ?
      support->MakeReplayDecisionBuilder(&s, solution) :  // Master.
      s.MakeLocalSearchPhase(                             // Worker.
          solution,
          s.MakeLocalSearchPhaseParameters(
              support->MakeSolutionPool(&s, worker),
              local_search_operator,
              db));

  // Everybody needs this communication monitor.
  monitors.push_back(
      support->MakeCommunicationMonitor(&s, solution, master, worker));

  s.Solve(final_db, monitors);  // go!

  // The master will report solutions.
  if (master && collector->solution_count()) {
    CHECK_EQ(1, collector->solution_count());
    Assignment* const best_solution = collector->solution(0);
    CHECK_EQ(obj_max, best_solution->ObjectiveValue());
  }
}

void TestInitialSolution() {
  LOG(INFO) << "TestInitialSolution";
  int work_done = 0;
  Mutex mutex;
  scoped_ptr<ParallelSolveSupport> support(
      MakeMtSolveSupport(FLAGS_workers,
                         false,
                         NewPermanentCallback(
                             &BuildModelWithSolution,
                             &work_done,
                             &mutex)));

  support->Run();
  CHECK_EQ(FLAGS_workers + 2, work_done);
}

void TestNoInitialSolution() {
  LOG(INFO) << "TestNoInitialSolution";
  int work_done = 0;
  Mutex mutex;
  scoped_ptr<ParallelSolveSupport> support(
      MakeMtSolveSupport(FLAGS_workers,
                         false,
                         NewPermanentCallback(
                             &BuildModelWithoutSolution,
                             &work_done,
                             &mutex)));
  support->Run();
  CHECK_EQ(FLAGS_workers + 2, work_done);
}

void TestModelWithSearch() {
  LOG(INFO) << "TestModelWithSearch";
  scoped_ptr<ParallelSolveSupport> support(
      MakeMtSolveSupport(FLAGS_workers,
                         true,
                         NewPermanentCallback(
                             &BuildModelWithSearch,
                             FLAGS_workers)));
  support->Run();
}
}  // namespace  operations_research

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  operations_research::TestInitialSolution();
  operations_research::TestNoInitialSolution();
  operations_research::TestModelWithSearch();
  return 0;
}
