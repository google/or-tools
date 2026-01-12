// Copyright 2010-2025 Google LLC
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

#include "ortools/sat/subsolver.h"

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/timer.h"
#include "ortools/sat/util.h"
#if !defined(__PORTABLE_PLATFORM__)
#include "ortools/base/threadpool.h"
#endif  // __PORTABLE_PLATFORM__

namespace operations_research {
namespace sat {

namespace {

// Returns the next SubSolver index from which to call GenerateTask(). Note that
// only SubSolvers for which TaskIsAvailable() is true are considered. Return -1
// if no SubSolver can generate a new task.
//
// For now we use a really basic logic that tries to equilibrate the walltime or
// deterministic time spent in each subsolver.
int NextSubsolverToSchedule(std::vector<std::unique_ptr<SubSolver>>& subsolvers,
                            bool deterministic = true) {
  int best = -1;
  double best_score = std::numeric_limits<double>::infinity();
  for (int i = 0; i < subsolvers.size(); ++i) {
    if (subsolvers[i] == nullptr) continue;
    if (subsolvers[i]->TaskIsAvailable()) {
      const double score = subsolvers[i]->GetSelectionScore(deterministic);
      if (best == -1 || score < best_score) {
        best_score = score;
        best = i;
      }
    }
  }

  if (best != -1) VLOG(1) << "Scheduling " << subsolvers[best]->name();
  return best;
}

void ClearSubsolversThatAreDone(
    absl::Span<const int> num_in_flight_per_subsolvers,
    std::vector<std::unique_ptr<SubSolver>>& subsolvers) {
  for (int i = 0; i < subsolvers.size(); ++i) {
    if (subsolvers[i] == nullptr) continue;
    if (num_in_flight_per_subsolvers[i] > 0) continue;
    if (subsolvers[i]->IsDone()) {
      // We can free the memory used by this solver for good.
      VLOG(1) << "Deleting " << subsolvers[i]->name();
      subsolvers[i].reset();
      continue;
    }
  }
}

void SynchronizeAll(absl::Span<const std::unique_ptr<SubSolver>> subsolvers) {
  for (const auto& subsolver : subsolvers) {
    if (subsolver == nullptr) continue;
    subsolver->Synchronize();
  }
}

}  // namespace

void SequentialLoop(std::vector<std::unique_ptr<SubSolver>>& subsolvers) {
  int64_t task_id = 0;
  std::vector<int> num_in_flight_per_subsolvers(subsolvers.size(), 0);
  while (true) {
    SynchronizeAll(subsolvers);
    ClearSubsolversThatAreDone(num_in_flight_per_subsolvers, subsolvers);
    const int best = NextSubsolverToSchedule(subsolvers);
    if (best == -1) break;
    subsolvers[best]->NotifySelection();

    WallTimer timer;
    timer.Start();
    subsolvers[best]->GenerateTask(task_id++)();
    subsolvers[best]->AddTaskDuration(timer.Get());
  }
}

#if defined(__PORTABLE_PLATFORM__)

// On portable platform, we don't support multi-threading for now.

void NonDeterministicLoop(std::vector<std::unique_ptr<SubSolver>>& subsolvers,
                          int num_threads, ModelSharedTimeLimit* time_limit) {
  SequentialLoop(subsolvers);
}

void DeterministicLoop(std::vector<std::unique_ptr<SubSolver>>& subsolvers,
                       int num_threads, int batch_size, int max_num_batches) {
  SequentialLoop(subsolvers);
}

#else  // __PORTABLE_PLATFORM__

void DeterministicLoop(std::vector<std::unique_ptr<SubSolver>>& subsolvers,
                       int num_threads, int batch_size, int max_num_batches) {
  CHECK_GT(num_threads, 0);
  CHECK_GT(batch_size, 0);
  if (batch_size == 1) {
    return SequentialLoop(subsolvers);
  }

  int64_t task_id = 0;
  std::vector<int> num_in_flight_per_subsolvers(subsolvers.size(), 0);
  std::vector<std::function<void()>> to_run;
  std::vector<int> indices;
  std::vector<double> timing;
  to_run.reserve(batch_size);
  ThreadPool pool(num_threads);
  for (int batch_index = 0;; ++batch_index) {
    VLOG(2) << "Starting deterministic batch of size " << batch_size;
    SynchronizeAll(subsolvers);
    ClearSubsolversThatAreDone(num_in_flight_per_subsolvers, subsolvers);

    // We abort the loop after the last synchronize to properly reports final
    // status in case max_num_batches is used.
    if (max_num_batches > 0 && batch_index >= max_num_batches) break;

    // We first generate all task to run in this batch.
    // Note that we can't start the task right away since if a task finish
    // before we schedule everything, we will not be deterministic.
    to_run.clear();
    indices.clear();
    for (int t = 0; t < batch_size; ++t) {
      const int best = NextSubsolverToSchedule(subsolvers);
      if (best == -1) break;
      num_in_flight_per_subsolvers[best]++;
      subsolvers[best]->NotifySelection();
      to_run.push_back(subsolvers[best]->GenerateTask(task_id++));
      indices.push_back(best);
    }
    if (to_run.empty()) break;

    // Schedule each task.
    timing.resize(to_run.size());
    absl::BlockingCounter blocking_counter(static_cast<int>(to_run.size()));
    for (int i = 0; i < to_run.size(); ++i) {
      pool.Schedule(
          [i, f = std::move(to_run[i]), &timing, &blocking_counter]() {
            WallTimer timer;
            timer.Start();
            f();
            timing[i] = timer.Get();
            blocking_counter.DecrementCount();
          });
    }

    // Wait for all tasks of this batch to be done before scheduling another
    // batch.
    blocking_counter.Wait();

    // Update times.
    num_in_flight_per_subsolvers.assign(subsolvers.size(), 0);
    for (int i = 0; i < to_run.size(); ++i) {
      subsolvers[indices[i]]->AddTaskDuration(timing[i]);
    }
  }
}

void NonDeterministicLoop(std::vector<std::unique_ptr<SubSolver>>& subsolvers,
                          const int num_threads,
                          ModelSharedTimeLimit* time_limit) {
  CHECK_GT(num_threads, 0);
  if (num_threads == 1) {
    return SequentialLoop(subsolvers);
  }

  // The mutex guards num_in_flight and num_in_flight_per_subsolvers.
  // This is used to detect when the search is done.
  absl::Mutex mutex;
  int num_in_flight = 0;  // Guarded by `mutex`.
  std::vector<int> num_in_flight_per_subsolvers(subsolvers.size(), 0);

  // Predicate to be used with absl::Condition to detect that num_in_flight <
  // num_threads. Must only be called while locking `mutex`.
  const auto num_in_flight_lt_num_threads = [&num_in_flight, num_threads]() {
    return num_in_flight < num_threads;
  };

  ThreadPool pool(num_threads);

  // The lambda below are using little space, but there is no reason
  // to create millions of them, so we use the blocking nature of
  // pool.Schedule() when the queue capacity is set.
  int64_t task_id = 0;
  while (true) {
    // Set to true if no task is pending right now.
    bool all_done = false;
    {
      // Wait if num_in_flight == num_threads.
      const bool condition = mutex.LockWhenWithTimeout(
          absl::Condition(&num_in_flight_lt_num_threads),
          absl::Milliseconds(100));

      // To support some "advanced" cancelation of subsolve, we still call
      // synchronize every 0.1 seconds even if there is no worker available.
      //
      // TODO(user): We could also directly register callback to set stopping
      // Boolean to false in a few places.
      if (!condition) {
        mutex.unlock();
        SynchronizeAll(subsolvers);
        continue;
      }

      // The stopping condition is that we do not have anything else to generate
      // once all the task are done and synchronized.
      if (num_in_flight == 0) all_done = true;
      mutex.unlock();
    }

    SynchronizeAll(subsolvers);
    int best = -1;
    {
      // We need to do that while holding the lock since substask below might
      // be currently updating the time via AddTaskDuration().
      const absl::MutexLock mutex_lock(mutex);
      ClearSubsolversThatAreDone(num_in_flight_per_subsolvers, subsolvers);
      best = NextSubsolverToSchedule(subsolvers, /*deterministic=*/false);
      if (VLOG_IS_ON(1) && time_limit->LimitReached()) {
        std::vector<std::string> debug;
        for (int i = 0; i < subsolvers.size(); ++i) {
          if (subsolvers[i] != nullptr && num_in_flight_per_subsolvers[i] > 0) {
            debug.push_back(absl::StrCat(subsolvers[i]->name(), ":",
                                         num_in_flight_per_subsolvers[i]));
          }
        }
        if (!debug.empty()) {
          VLOG_EVERY_N_SEC(1, 1)
              << "Subsolvers still running after time limit: "
              << absl::StrJoin(debug, ",");
        }
      }
    }
    if (best == -1) {
      if (all_done) break;

      // It is hard to know when new info will allows for more task to be
      // scheduled, so for now we just sleep for a bit. Note that in practice We
      // will never reach here except at the end of the search because we can
      // always schedule LNS threads.
      absl::SleepFor(absl::Milliseconds(1));
      continue;
    }

    // Schedule next task.
    subsolvers[best]->NotifySelection();
    {
      absl::MutexLock mutex_lock(mutex);
      num_in_flight++;
      num_in_flight_per_subsolvers[best]++;
    }
    std::function<void()> task = subsolvers[best]->GenerateTask(task_id++);
    const std::string name = subsolvers[best]->name();
    pool.Schedule([task = std::move(task), name, best, &subsolvers, &mutex,
                   &num_in_flight, &num_in_flight_per_subsolvers]() {
      WallTimer timer;
      timer.Start();
      task();

      const absl::MutexLock mutex_lock(mutex);
      DCHECK(subsolvers[best] != nullptr);
      DCHECK_GT(num_in_flight_per_subsolvers[best], 0);
      num_in_flight_per_subsolvers[best]--;
      VLOG(1) << name << " done in " << timer.Get() << "s.";
      subsolvers[best]->AddTaskDuration(timer.Get());
      num_in_flight--;
    });
  }
}

#endif  // __PORTABLE_PLATFORM__

}  // namespace sat
}  // namespace operations_research
