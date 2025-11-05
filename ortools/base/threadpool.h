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

#ifndef ORTOOLS_BASE_THREADPOOL_H_
#define ORTOOLS_BASE_THREADPOOL_H_

#include <cstddef>
#include <deque>
#include <optional>
#include <thread>  // NOLINT
#include <vector>

#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"

namespace operations_research {

class ThreadPool {
 public:
  explicit ThreadPool(int num_threads);
  ThreadPool(absl::string_view prefix, int num_threads);
  ~ThreadPool();

  void Schedule(absl::AnyInvocable<void() &&> callback);

 private:
  // Waiter for a single thread.
  struct Waiter {
    absl::CondVar cv;  // signalled when there is work to do
  };

  // Spawn a single new worker thread.
  //
  // REQUIRES: threads_.size() < max_threads_
  void SpawnThread() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void RunWorker();

  // Removes the oldest element from the queue and returns it. Causes the
  // current thread to wait for producers if the queue is empty. Returns
  // an empty `std::optional` if the thread pool is shutting down.
  std::optional<absl::AnyInvocable<void() &&>> DequeueWork()
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Signals a waiter if there is one, or spawns a thread to try to add a new
  // waiter.
  //
  // REQUIRES: !queue_.empty()
  void SignalWaiter() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable absl::Mutex mutex_;
  absl::CondVar wait_nonfull_ ABSL_GUARDED_BY(mutex_);
  std::vector<Waiter* absl_nonnull> waiters_ ABSL_GUARDED_BY(mutex_);
  const size_t max_threads_;
  std::deque<absl::AnyInvocable<void() &&>> queue_;
  bool stopping_ ABSL_GUARDED_BY(mutex_) = false;
  size_t running_threads_ ABSL_GUARDED_BY(mutex_) = 0;
  std::vector<std::thread> threads_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace operations_research
#endif  // ORTOOLS_BASE_THREADPOOL_H_
