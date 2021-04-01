// Copyright 2010-2021 Google LLC
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

#include "ortools/base/logging.h"

#if !defined(__PORTABLE_PLATFORM__)
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#endif  // __PORTABLE_PLATFORM__

namespace operations_research {
namespace sat {

namespace {

// Returns the next SubSolver index from which to call GenerateTask(). Note that
// only SubSolvers for which TaskIsAvailable() is true are considered. Return -1
// if no SubSolver can generate a new task.
//
// For now we use a really basic logic: call the least frequently called.
int NextSubsolverToSchedule(
    const std::vector<std::unique_ptr<SubSolver>>& subsolvers,
    const std::vector<int64_t>& num_generated_tasks) {
  int best = -1;
  for (int i = 0; i < subsolvers.size(); ++i) {
    if (subsolvers[i]->TaskIsAvailable()) {
      if (best == -1 || num_generated_tasks[i] < num_generated_tasks[best]) {
        best = i;
      }
    }
  }
  if (best != -1) VLOG(1) << "Scheduling " << subsolvers[best]->name();
  return best;
}

void SynchronizeAll(const std::vector<std::unique_ptr<SubSolver>>& subsolvers) {
  for (const auto& subsolver : subsolvers) subsolver->Synchronize();
}

}  // namespace

void SequentialLoop(const std::vector<std::unique_ptr<SubSolver>>& subsolvers) {
  int64_t task_id = 0;
  std::vector<int64_t> num_generated_tasks(subsolvers.size(), 0);
  while (true) {
    SynchronizeAll(subsolvers);
    const int best = NextSubsolverToSchedule(subsolvers, num_generated_tasks);
    if (best == -1) break;
    num_generated_tasks[best]++;
    subsolvers[best]->GenerateTask(task_id++)();
  }
}

#if defined(__PORTABLE_PLATFORM__)

// On portable platform, we don't support multi-threading for now.

void NonDeterministicLoop(
    const std::vector<std::unique_ptr<SubSolver>>& subsolvers,
    int num_threads) {
  SequentialLoop(subsolvers);
}

void DeterministicLoop(
    const std::vector<std::unique_ptr<SubSolver>>& subsolvers, int num_threads,
    int batch_size) {
  SequentialLoop(subsolvers);
}

#else  // __PORTABLE_PLATFORM__

void DeterministicLoop(
    const std::vector<std::unique_ptr<SubSolver>>& subsolvers, int num_threads,
    int batch_size) {
  CHECK_GT(num_threads, 0);
  CHECK_GT(batch_size, 0);
  if (batch_size == 1) {
    return SequentialLoop(subsolvers);
  }

  int64_t task_id = 0;
  std::vector<int64_t> num_generated_tasks(subsolvers.size(), 0);
  while (true) {
    SynchronizeAll(subsolvers);

    // TODO(user): We could reuse the same ThreadPool as long as we wait for all
    // the task in a batch to finish before scheduling new ones. Not sure how
    // to easily do that, so for now we just recreate the pool for each batch.
    ThreadPool pool("DeterministicLoop", num_threads);
    pool.StartWorkers();

    int num_in_batch = 0;
    for (int t = 0; t < batch_size; ++t) {
      const int best = NextSubsolverToSchedule(subsolvers, num_generated_tasks);
      if (best == -1) break;
      ++num_in_batch;
      num_generated_tasks[best]++;
      pool.Schedule(subsolvers[best]->GenerateTask(task_id++));
    }
    if (num_in_batch == 0) break;
  }
}

void NonDeterministicLoop(
    const std::vector<std::unique_ptr<SubSolver>>& subsolvers,
    int num_threads) {
  CHECK_GT(num_threads, 0);
  if (num_threads == 1) {
    return SequentialLoop(subsolvers);
  }

  // The mutex will protect these two fields. This is used to only keep
  // num_threads task in-flight and detect when the search is done.
  absl::Mutex mutex;
  absl::CondVar thread_available_condition;
  int num_scheduled_and_not_done = 0;

  ThreadPool pool("NonDeterministicLoop", num_threads);
  pool.StartWorkers();

  // The lambda below are using little space, but there is no reason
  // to create millions of them, so we use the blocking nature of
  // pool.Schedule() when the queue capacity is set.
  int64_t task_id = 0;
  std::vector<int64_t> num_generated_tasks(subsolvers.size(), 0);
  while (true) {
    bool all_done = false;
    {
      absl::MutexLock mutex_lock(&mutex);

      // The stopping condition is that we do not have anything else to generate
      // once all the task are done and synchronized.
      if (num_scheduled_and_not_done == 0) all_done = true;

      // Wait if num_scheduled_and_not_done == num_threads.
      if (num_scheduled_and_not_done == num_threads) {
        thread_available_condition.Wait(&mutex);
      }
    }

    SynchronizeAll(subsolvers);
    const int best = NextSubsolverToSchedule(subsolvers, num_generated_tasks);
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
    num_generated_tasks[best]++;
    {
      absl::MutexLock mutex_lock(&mutex);
      num_scheduled_and_not_done++;
    }
    std::function<void()> task = subsolvers[best]->GenerateTask(task_id++);
    const std::string name = subsolvers[best]->name();
    pool.Schedule([task, num_threads, name, &mutex, &num_scheduled_and_not_done,
                   &thread_available_condition]() {
      task();

      absl::MutexLock mutex_lock(&mutex);
      VLOG(1) << name << " done.";
      num_scheduled_and_not_done--;
      if (num_scheduled_and_not_done == num_threads - 1) {
        thread_available_condition.SignalAll();
      }
    });
  }
}

#endif  // __PORTABLE_PLATFORM__

}  // namespace sat
}  // namespace operations_research
