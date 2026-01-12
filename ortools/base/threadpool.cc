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

#include "ortools/base/threadpool.h"

#include <optional>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"

namespace operations_research {

// It is a common error to call ThreadPool(workitems.size()), which
// crashes when workitems is empty. Prevent those crashes by creating at
// least one thread.
ThreadPool::ThreadPool(int num_threads)
    : max_threads_(num_threads == 0 ? 1 : num_threads) {
  CHECK_GT(max_threads_, 0u);
  // Spawn a single thread to handle work by default.
  absl::MutexLock lock(mutex_);
  SpawnThread();
}

ThreadPool::ThreadPool(absl::string_view prefix, int num_threads)
    : ThreadPool(num_threads) {}

ThreadPool::~ThreadPool() {
  // Make threads finish up by setting stopping_. Ensure all threads waiting see
  // this change by signalling their condvar.
  {
    absl::MutexLock l(mutex_);
    stopping_ = true;
    for (Waiter* absl_nonnull waiter : waiters_) {
      waiter->cv.Signal();
    }
    // Wait until the queue is empty. This implies no new threads will be
    // spawned, and all existing threads are exiting.
    auto queue_empty = [this]() ABSL_SHARED_LOCKS_REQUIRED(mutex_) {
      return queue_.empty();
    };
    mutex_.Await(absl::Condition(&queue_empty));
  }
  // Join and delete all threads. Because the queue is empty, we know no new
  // threads will be added to threads_.
  for (auto& worker : threads_) {
    worker.join();
  }
}

void ThreadPool::SpawnThread() {
  CHECK_LE(threads_.size(), max_threads_);
  threads_.emplace_back([this] { RunWorker(); });
}

void ThreadPool::RunWorker() {
  {
    absl::MutexLock lock(mutex_);
    ++running_threads_;
  }
  while (true) {
    std::optional<absl::AnyInvocable<void() &&>> item = DequeueWork();
    if (!item.has_value()) {  // Requesting to stop the worker thread.
      break;
    }
    DCHECK(item);
    std::move (*item)();
  }
}

void ThreadPool::SignalWaiter() {
  DCHECK(!queue_.empty());
  if (waiters_.empty()) {
    // If there are no waiters, try spawning a new thread to pick up work.
    if (running_threads_ == threads_.size() && threads_.size() < max_threads_) {
      SpawnThread();
    }
  } else {
    // If there are waiters we wake the last inserted waiter. Note that we can
    // signal this waiter multiple times. This is not only ok but it is crucial
    // to reduce spurious wakeups.
    waiters_.back()->cv.Signal();
  }
}

std::optional<absl::AnyInvocable<void() &&>> ThreadPool::DequeueWork() {
  // Wait for queue to be not-empty
  absl::MutexLock m(mutex_);
  while (queue_.empty() && !stopping_) {
    Waiter self;
    waiters_.push_back(&self);
    self.cv.Wait(&mutex_);
    waiters_.erase(absl::c_find(waiters_, &self));
  }
  if (queue_.empty()) {
    DCHECK(stopping_);
    return std::nullopt;
  }
  absl::AnyInvocable<void() &&> result = std::move(queue_.front());
  queue_.pop_front();
  if (!queue_.empty()) {
    SignalWaiter();
  }
  return std::move(result);
}

void ThreadPool::Schedule(absl::AnyInvocable<void() &&> callback) {
  // Wait for queue to be not-full
  absl::MutexLock m(mutex_);
  DCHECK(!stopping_) << "Callback added after destructor started";
  if (ABSL_PREDICT_FALSE(stopping_)) return;
  queue_.push_back(std::move(callback));
  SignalWaiter();
}

}  // namespace operations_research
