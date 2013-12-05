// Copyright 2010-2013 Google
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

#include "base/unique_ptr.h"

#include "base/callback.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "base/scoped_ptr.h"
#include "base/synchronization.h"
#include "base/threadpool.h"
#include "constraint_solver/assignment.pb.h"
#include "constraint_solver/constraint_solver.h"

DEFINE_int32(cp_parallel_update_frequency, 16,
             "frequency to update the local solution with the foreign "
             "one stored on the master.");

namespace operations_research {
namespace {

// ----- Headers -----

// This class acts as a glue between the master and different workers
// in a multi thread environment. It offers synchronization services
// and help creates the different objects needed by the searches.
class MtSolveSupport : public ParallelSolveSupport {
 public:
  MtSolveSupport(int workers, bool maximize, ModelBuilder* const run_model);

  virtual ~MtSolveSupport();

  // This method is used by slaves to wait for the initial solution to
  // be found by the master. If the return value is false, then no
  // solution has been found and the slave should exit gracefully.
  virtual bool WaitForInitialSolution(Assignment* const to_fill, int worker);

  // This method is used be the master to signal the initial solution
  // to workers.
  virtual void RegisterInitialSolution(Assignment* const to_save);

  // This method is used by the master to signal that no initial
  // solution has been found.
  virtual void RegisterNoInitialSolution();

  // Creates a decision builder for the master. This decision builder
  // will print out each solutions found by the slaves.
  virtual DecisionBuilder* MakeReplayDecisionBuilder(
      Solver* const s, const Assignment* const solution);

  // A simple shortcut to create the search log only on the master.
  virtual SearchMonitor* MakeSearchLog(Solver* const s, bool master, int64 freq,
                                       IntVar* const objective);

  // A simple shortcut to create the limit only on the workers and not
  // on the master.
  virtual SearchMonitor* MakeLimit(Solver* const s, bool master,
                                   int64 time_limit, int64 branch_limit,
                                   int64 fail_limit, int64 solution_limit);

  // Creates a search monitor that communicates solutions found by the
  // slaves to the master. Both master and slaves should use this.
  virtual SearchMonitor* MakeCommunicationMonitor(
      Solver* const s, const Assignment* const solution, bool master,
      int worker);

  // Creates a solution pool to be used in the Local Search for each
  // worker. This solution pool is responsible for pulling improved
  // solution from the master.
  virtual SolutionPool* MakeSolutionPool(Solver* const s, int worker);

  virtual void Run() {
    ThreadPool pool("Parallel_LNS", workers_ + 1);
    pool.StartWorkers();
    // Start master.
    pool.Add(NewCallback(run_model_.get(), &ModelBuilder::Run,
                         reinterpret_cast<ParallelSolveSupport*>(this), true,
                         -1));

    for (int index = 0; index < workers_; ++index) {
      pool.Add(NewCallback(run_model_.get(), &ModelBuilder::Run,
                           reinterpret_cast<ParallelSolveSupport*>(this), false,
                           index));
    }
  }

  // Returns the number of workers (master + slaves).
  int workers() const { return workers_; }

  // Internal
  void MasterApplyReplayer(Solver* const s, Assignment* const solution);
  void MasterRefuteReplayer(Solver* const s, Assignment* const solution);
  Decision* MasterNextDecision(Solver* const s, Assignment* const solution);

  void MasterEnterSearch();
  void MasterExitSearch();

  void SlaveEnterSearch(int worker);
  void SlaveExitSearch(int worker);
  void SlaveNotifySolution(int worker, Assignment* const solution);

  // Compare local solution with the shared one.
  bool IsSharedSolutionBetter(int64 current_value) const;
  bool IsSharedSolutionWorse(int64 current_value) const;

  // Locks the internal mutex.
  void LockMutex();
  // Unlocks the internal mutex.
  void UnlockMutex();

 private:
  bool CheckTermination();
  void Reset();
  void BlockBarrier(std::unique_ptr<Barrier>* barrier_ptr) {
    if ((*barrier_ptr)->Block()) {
      barrier_ptr->reset(new Barrier(workers_ + 1));
    }
  }

  // Total number of workers.
  const int workers_;
  // Global mutex.
  Mutex mutex_;
  // Condition var to awaken master after a new solution.
  CondVar cond_var_;
  // Cost of best solution reported by the master search.
  int64 best_exported_cost_;
  // Is the master blocked awaiting a better solution.
  bool master_blocked_;
  // Initial barrier after the search for the first solution.
  std::unique_ptr<Barrier> solution_barrier_;
  // Initial barrier when all slaves enter search.
  std::unique_ptr<Barrier> enter_search_barrier_;
  // Initial barrier when all slaves exit search.
  std::unique_ptr<Barrier> exit_search_barrier_;
  // Fail stamp of the last reported solution in the master search.
  uint64 fail_stamp_;
  // How many slaves have started.
  int started_slaves_;
  // How many slaves have stopped.
  int ended_slaves_;
};

// This class is used in the master search to replay the best solution
// so far in the apply branch.
class MtReplaySolution : public Decision {
 public:
  MtReplaySolution(MtSolveSupport* const support, Assignment* const solution)
      : support_(support), solution_(solution) {}
  virtual ~MtReplaySolution() {}

  virtual void Apply(Solver* const solver) {
    support_->MasterApplyReplayer(solver, solution_);
  }

  virtual void Refute(Solver* const solver) {
    support_->MasterRefuteReplayer(solver, solution_);
  }

 private:
  MtSolveSupport* const support_;
  Assignment* const solution_;
};

// This decision builder replays the solutions found by the slaves.
class MtReplayer : public DecisionBuilder {
 public:
  MtReplayer(Solver* const solver, MtSolveSupport* const support,
             const Assignment* const solution)
      : support_(support), solution_(solver->MakeAssignment(solution)) {}
  virtual ~MtReplayer() {}

  virtual Decision* Next(Solver* const solver) {
    return support_->MasterNextDecision(solver, solution_);
  }

 private:
  MtSolveSupport* const support_;
  Assignment* const solution_;
};

// This search monitor is used to synchronize the master and the
// slaves at the beginning of the search.
class MtSolutionReceiver : public SearchMonitor {
 public:
  MtSolutionReceiver(Solver* const solver, MtSolveSupport* const support)
      : SearchMonitor(solver), support_(support) {}

  virtual ~MtSolutionReceiver() {}

  virtual void EnterSearch() { support_->MasterEnterSearch(); }
  virtual void ExitSearch() { support_->MasterExitSearch(); }

  virtual string DebugString() const { return "MtSolutionReceiver"; }

 private:
  MtSolveSupport* const support_;
};

// This class is used to to synchronize the master and the search at
// the beginning of the search. It is also used to report solutions
// from the slaves to the master.
class MtSolutionDispatcher : public SearchMonitor {
 public:
  MtSolutionDispatcher(Solver* const solver, MtSolveSupport* const support,
                       const Assignment* const assignment, int worker)
      : SearchMonitor(solver),
        support_(support),
        assignment_(solver->MakeAssignment(assignment)),
        worker_(worker) {}

  virtual ~MtSolutionDispatcher() {}

  virtual void EnterSearch() { support_->SlaveEnterSearch(worker_); }

  virtual void ExitSearch() { support_->SlaveExitSearch(worker_); }

  virtual bool AtSolution() {
    assignment_->Store();
    support_->SlaveNotifySolution(worker_, assignment_);
    return false;
  }

  virtual string DebugString() const { return "MtSolutionDispatcher"; }

 private:
  MtSolveSupport* const support_;
  Assignment* const assignment_;
  const int worker_;
};

// ---------- Implementation ----------

// ----- MtSolveSupport -----

MtSolveSupport::MtSolveSupport(
    int workers, bool maximize,
    ParallelSolveSupport::ModelBuilder* const run_model)
    : ParallelSolveSupport(maximize, run_model),
      workers_(workers),
      best_exported_cost_(maximize_ ? kint64min : kint64max),
      master_blocked_(false),
      solution_barrier_(new Barrier(workers_ + 1)),
      enter_search_barrier_(new Barrier(workers_ + 1)),
      exit_search_barrier_(new Barrier(workers_ + 1)),
      fail_stamp_(0ULL),
      started_slaves_(0),
      ended_slaves_(0) {
  Reset();
}

MtSolveSupport::~MtSolveSupport() {}

void MtSolveSupport::Reset() {
  VLOG(1) << "Calling reset on MtSolveSupport";
  started_slaves_ = 0;
  ended_slaves_ = 0;
  master_blocked_ = false;
  best_exported_cost_ = maximize() ? kint64min : kint64max;
  fail_stamp_ = 0ULL;
  local_solution_->mutable_worker_info()->set_worker_id(-1);
  local_solution_->set_is_valid(false);
}

void MtSolveSupport::LockMutex() EXCLUSIVE_LOCK_FUNCTION(mutex_) {
  mutex_.Lock();
}

void MtSolveSupport::UnlockMutex() UNLOCK_FUNCTION(mutex_) { mutex_.Unlock(); }

bool MtSolveSupport::WaitForInitialSolution(Assignment* const to_fill,
                                            int worker) {
  VLOG(1) << "worker " << worker << " waiting for initial solution";
  BlockBarrier(&solution_barrier_);
  if (local_solution_->is_valid()) {
    to_fill->Load(*local_solution_);
    VLOG(1) << "worker " << worker << " receiving initial solution with value "
            << to_fill->ObjectiveValue();
    return true;
  } else {
    VLOG(1) << "worker " << worker << " has not received a solution";
    return false;
  }
}

void MtSolveSupport::RegisterInitialSolution(Assignment* const to_save) {
  CHECK(to_save);
  to_save->Save(local_solution_.get());
  local_solution_->mutable_worker_info()->set_worker_id(0);
  local_solution_->set_is_valid(true);
  VLOG(1) << "Importing initial solution with value "
          << to_save->ObjectiveValue();
  BlockBarrier(&solution_barrier_);
}

void MtSolveSupport::RegisterNoInitialSolution() {
  VLOG(1) << "No initial solution found";
  local_solution_->clear_int_var_assignment();
  local_solution_->clear_interval_var_assignment();
  local_solution_->clear_objective();
  local_solution_->mutable_worker_info()->set_worker_id(0);
  local_solution_->set_is_valid(false);
  BlockBarrier(&solution_barrier_);
}

bool MtSolveSupport::CheckTermination() {
  return (ended_slaves_ == workers_ &&
          best_exported_cost_ == local_solution_->objective().min());
}

void MtSolveSupport::MasterApplyReplayer(Solver* const s,
                                         Assignment* const solution) {
  mutex_.Lock();
  while (best_exported_cost_ == local_solution_->objective().min() &&
         !CheckTermination()) {
    master_blocked_ = true;
    VLOG(1) << "master going into sleep";
    cond_var_.Wait(&mutex_);
  }
  master_blocked_ = false;
  if (CheckTermination()) {
    VLOG(1) << "Master failing after detecting termination";
    mutex_.Unlock();
    s->Fail();
    // Solver::Fail() will end up invoking Search::JumpBack() which does a
    // longjmp, so the following code should never be executed.  We're careful
    // to unlock the mutex before the call, since otherwise the mutex would be
    // in an inconsistent state (i.e. it would be bad to have the mutex locked
    // when we longjmp out of this context).  The call to lock the mutex is
    // solely to appease annotalysis, and is correct in the event we do execute
    // the following code.
    mutex_.Lock();
  }
  VLOG(1) << "Master has received solution with objective value "
          << local_solution_->objective().min() << " from worker "
          << local_solution_->worker_info().worker_id();
  fail_stamp_ = s->fail_stamp();
  best_exported_cost_ = local_solution_->objective().min();
  solution->Load(*local_solution_);
  mutex_.Unlock();
  solution->Restore();
  VLOG(1) << "Master has successfully restored solution";
}

void MtSolveSupport::MasterRefuteReplayer(Solver* const s,
                                          Assignment* const solution) {
  if (CheckTermination()) {
    VLOG(1) << "Master killing right branch after detecting termination";
    s->Fail();
  }
}

Decision* MtSolveSupport::MasterNextDecision(Solver* const s,
                                             Assignment* const solution) {
  if (CheckTermination()) {
    VLOG(1) << "Master not creating decision after detecting termination";
    s->Fail();
  }
  if (s->fail_stamp() == fail_stamp_) {
    return nullptr;
  } else {
    return s->RevAlloc(new MtReplaySolution(this, solution));
  }
}

void MtSolveSupport::SlaveExitSearch(int worker) {
  {
    MutexLock lock(&mutex_);
    ended_slaves_++;
    VLOG(1) << "Slave " << worker << " exiting!";
    if (ended_slaves_ == started_slaves_ && master_blocked_) {
      VLOG(1) << "Slave " << worker << " awaking master";
      cond_var_.SignalAll();
    }
  }
  BlockBarrier(&exit_search_barrier_);
  VLOG(1) << "Slave " << worker << " after exit barrier";
}

void MtSolveSupport::MasterEnterSearch() {
  VLOG(1) << "Master before enter barrier";
  BlockBarrier(&enter_search_barrier_);
  VLOG(1) << "Master after enter barrier";
}

void MtSolveSupport::MasterExitSearch() {
  VLOG(1) << "Master before exit barrier";
  Reset();
  BlockBarrier(&exit_search_barrier_);
  VLOG(1) << "Master after exit barrier";
}

void MtSolveSupport::SlaveEnterSearch(int worker) {
  VLOG(1) << "Slave " << worker << " before enter barrier";
  BlockBarrier(&enter_search_barrier_);
  VLOG(1) << "Slave " << worker << " after enter barrier";
  {
    MutexLock lock(&mutex_);
    VLOG(1) << "Slave " << worker << " starting!";
    started_slaves_++;
  }
}

bool MtSolveSupport::IsSharedSolutionBetter(int64 current_value) const {
  const int64 best_value = local_solution_->objective().min();
  return (!maximize_ && current_value > best_value) ||
         (maximize_ && current_value < best_value);
}

bool MtSolveSupport::IsSharedSolutionWorse(int64 current_value) const {
  const int64 best_value = local_solution_->objective().min();
  return (!maximize_ && current_value < best_value) ||
         (maximize_ && current_value > best_value);
}

void MtSolveSupport::SlaveNotifySolution(int worker,
                                         Assignment* const solution) {
  MutexLock lock(&mutex_);
  const int64 objective_value = solution->ObjectiveValue();
  VLOG(1) << "worker " << worker
          << " has found a solution with objective value " << objective_value;
  if (IsSharedSolutionWorse(objective_value)) {
    VLOG(1) << "  - solution accepted against "
            << local_solution_->objective().min();
    solution->Save(local_solution_.get());
    local_solution_->mutable_worker_info()->set_worker_id(worker);
    local_solution_->set_is_valid(true);
    if (master_blocked_) {
      VLOG(1) << "Slave " << worker << " awakening master after solution";
      cond_var_.SignalAll();
    }
  } else {
    VLOG(1) << "  - solution rejected against shared version";
  }
}

DecisionBuilder* MtSolveSupport::MakeReplayDecisionBuilder(
    Solver* const s, const Assignment* const solution) {
  return s->RevAlloc(new MtReplayer(s, this, solution));
}

SearchMonitor* MtSolveSupport::MakeSearchLog(Solver* const s, bool master,
                                             int64 freq,
                                             IntVar* const objective) {
  if (master) {
    return s->MakeSearchLog(freq, objective);
  } else {
    return nullptr;
  }
}

SearchMonitor* MtSolveSupport::MakeCommunicationMonitor(
    Solver* const s, const Assignment* const solution, bool master,
    int worker) {
  if (master) {
    return s->RevAlloc(new MtSolutionReceiver(s, this));
  } else {
    return s->RevAlloc(new MtSolutionDispatcher(s, this, solution, worker));
  }
}

SearchMonitor* MtSolveSupport::MakeLimit(Solver* const s, bool master,
                                         int64 time_limit, int64 branch_limit,
                                         int64 fail_limit,
                                         int64 solution_limit) {
  if (master) {
    return nullptr;
  } else {
    return s->MakeLimit(time_limit, branch_limit, fail_limit, solution_limit);
  }
}

// ----- Sharing Solution Pool -----

class MTSharingSolutionPool : public SolutionPool {
 public:
  MTSharingSolutionPool(MtSolveSupport* const support, int worker)
      : support_(support), worker_(worker), count_(0) {}

  virtual ~MTSharingSolutionPool() {}

  virtual void Initialize(Assignment* const assignment) {
    reference_assignment_.reset(new Assignment(assignment));
  }

  virtual void RegisterNewSolution(Assignment* const assignment) {
    reference_assignment_->Copy(assignment);
  }

  virtual void GetNextSolution(Assignment* const assignment) {
    const int64 local_best = reference_assignment_->ObjectiveValue();
    support_->LockMutex();
    AssignmentProto* const best_solution = support_->solution();
    if (support_->IsSharedSolutionBetter(local_best)) {
      VLOG(1) << "slave " << worker_ << " import solution with value "
              << best_solution->objective().min() << " from "
              << best_solution->worker_info().worker_id();
      reference_assignment_->Load(*best_solution);
    }
    support_->UnlockMutex();
    assignment->Copy(reference_assignment_.get());
  }

  virtual bool SyncNeeded(Assignment* const local_assignment) {
    if (++count_ >= FLAGS_cp_parallel_update_frequency) {
      count_ = 0;
      const int64 current_value = local_assignment->ObjectiveValue();
      support_->LockMutex();
      bool result = support_->IsSharedSolutionBetter(current_value);
      if (result) {
        VLOG(1) << "Synchronizing current solution with value " << current_value
                << " with foreign solution with value "
                << support_->solution()->objective().min() << " for worker "
                << worker_;
      }
      support_->UnlockMutex();
      return result;
    } else {
      return false;
    }
  }

  virtual string DebugString() const { return "MTSharingSolutionPool"; }

 private:
  std::unique_ptr<Assignment> reference_assignment_;
  MtSolveSupport* const support_;
  const int worker_;
  int count_;
};

SolutionPool* MtSolveSupport::MakeSolutionPool(Solver* const s, int worker) {
  return s->RevAlloc(new MTSharingSolutionPool(this, worker));
}
}  // namespace

// ----- ParallelSolveSupport -----

ParallelSolveSupport::ParallelSolveSupport(bool maximize,
                                           ModelBuilder* const run_model)
    : local_solution_(new AssignmentProto()),
      maximize_(maximize),
      run_model_(run_model) {
  run_model->CheckIsRepeatable();
  local_solution_->mutable_worker_info()->set_worker_id(-1);
}

ParallelSolveSupport::~ParallelSolveSupport() {}

// ----- API -----

ParallelSolveSupport* MakeMtSolveSupport(
    int workers, bool maximize,
    ParallelSolveSupport::ModelBuilder* const model_builder) {
  return new MtSolveSupport(workers, maximize, model_builder);
}

}  // namespace operations_research
