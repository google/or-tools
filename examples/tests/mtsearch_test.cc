// Copyright 2009 Google Inc. All Rights Reserved.
// Author: lperron@google.com (Laurent Perron)

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

namespace operations_research {
class UpVar : public BaseLNS {
 public:
  UpVar(const IntVar* const* vars, int size, int worker)
      : BaseLNS(vars, size),  worker_(worker) {}

  virtual ~UpVar() {}

  virtual bool NextFragment(vector<int>* fragment) {
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
    ThreadSafeIncrement(active, mutex, 1);
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
    ThreadSafeIncrement(active, mutex, 1);
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
  Solver s(StringPrintf("Worker_%i", worker));
  VLOG(1) << "Worker " << worker  << " started";
  vector<IntVar*> vars;
  s.MakeBoolVarArray(workers, "vars", &vars);
  Assignment* const solution = s.MakeAssignment();
  solution->Add(vars);
  vector<int> coefficients;
  int obj_max = 0;
  for (int i = 0; i < workers; ++i) {
    const int value = (i + 1) * (i + 1);
    coefficients.push_back(value);
    obj_max += value;
  }
  IntVar* const objective = s.MakeScalProd(vars, coefficients)->Var();
  solution->AddObjective(objective);

  SolutionCollector* const collector = master ?
      s.MakeLastSolutionCollector(solution) :
      NULL;

  vector<SearchMonitor*> monitors;

  if (master) {
    monitors.push_back(collector);

    for (int i = 0; i < workers; ++i) {
      solution->SetValue(vars[i], 0);
    }
    solution->SetObjectiveValue(0);
    support->RegisterInitialSolution(solution);
  } else {
    CHECK(support->WaitForInitialSolution(solution, worker));
    CHECK_EQ(0, solution->ObjectiveValue());
  }

  monitors.push_back(s.MakeMaximize(objective, 1));

  UpVar* local_search_operator =
      s.RevAlloc(new UpVar(vars.data(), workers, worker));

  DecisionBuilder* db = s.MakePhase(vars,
                                    Solver::CHOOSE_FIRST_UNBOUND,
                                    Solver::ASSIGN_MAX_VALUE);

  SolutionPool* const pool = !master ?
      support->MakeSolutionPool(&s, worker) :
      s.MakeDefaultSolutionPool();

  LocalSearchPhaseParameters* parameters =
      s.MakeLocalSearchPhaseParameters(pool, local_search_operator, db);

  DecisionBuilder* const final_db = master ?
      support->MakeReplayDecisionBuilder(&s, solution) :
      s.MakeLocalSearchPhase(solution, parameters);

  monitors.push_back(
      support->MakeCommunicationMonitor(&s, solution, master, worker));

  s.Solve(final_db, monitors);
  if (master && collector->solution_count()) {
    CHECK_EQ(1, collector->solution_count());
    Assignment* const best_solution = collector->solution(0);
    CHECK_EQ(obj_max, best_solution->ObjectiveValue());
  }
}

void TestInitialSolution() {
  LOG(INFO) << "TestInitialSolution";
  const int kWorkers = 4;
  int work_done = 0;
  Mutex mutex;
  scoped_ptr<ParallelSolveSupport> support(
      MakeMtSolveSupport(kWorkers,
                         false,
                         NewPermanentCallback(
                             &BuildModelWithSolution,
                             &work_done,
                             &mutex)));

  support->Run();
  CHECK_EQ(kWorkers + 1, work_done);
}

void TestNoInitialSolution() {
  LOG(INFO) << "TestNoInitialSolution";
  const int kWorkers = 4;
  int work_done = 0;
  Mutex mutex;
  scoped_ptr<ParallelSolveSupport> support(
      MakeMtSolveSupport(kWorkers,
                         false,
                         NewPermanentCallback(
                             &BuildModelWithoutSolution,
                             &work_done,
                             &mutex)));
  support->Run();
  CHECK_EQ(kWorkers + 1, work_done);
}

void TestModelWithSearch() {
  LOG(INFO) << "TestModelWithSearch";
  const int kSlaves = 4;
  scoped_ptr<ParallelSolveSupport> support(
      MakeMtSolveSupport(kSlaves,
                         true,
                         NewPermanentCallback(
                             &BuildModelWithSearch,
                             kSlaves)));
  support->Run();
}
}  // namespace  operations_research

int main(int argc, char **argv) {
  operations_research::TestInitialSolution();
  operations_research::TestNoInitialSolution();
  operations_research::TestModelWithSearch();
  return 0;
}
