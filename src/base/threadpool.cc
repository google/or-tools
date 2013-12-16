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

#include "tinythread.h"  // NOLINT
#include "base/macros.h"
#include "base/mutex.h"
#include "base/synchronization.h"
#include "base/threadpool.h"

namespace operations_research {
void RunWorker(void* data) {
  ThreadPool* const thread_pool = reinterpret_cast<ThreadPool*>(data);
  Closure* work = thread_pool->GetNextTask();
  while (work != NULL) {
    work->Run();
    work = thread_pool->GetNextTask();
  }
  thread_pool->StopOnFinalBarrier();
}

ThreadPool::ThreadPool(const std::string& prefix, int num_workers)
    : num_workers_(num_workers),
      waiting_to_finish_(false),
      started_(false),
      final_barrier_(new Barrier(num_workers + 1)) {}

ThreadPool::~ThreadPool() {
  if (started_) {
    mutex_.Lock();
    waiting_to_finish_ = true;
    condition_.SignalAll();
    mutex_.Unlock();
    StopOnFinalBarrier();
    for (int i = 0; i < num_workers_; ++i) {
      all_workers_[i]->join();
      delete all_workers_[i];
    }
  }
}

void ThreadPool::StopOnFinalBarrier() {
  if (final_barrier_->Block()) {
    final_barrier_.reset(NULL);
  }
}

void ThreadPool::StartWorkers() {
  started_ = true;
  for (int i = 0; i < num_workers_; ++i) {
    all_workers_.push_back(new tthread::thread(&RunWorker, this));
  }
}

Closure* ThreadPool::GetNextTask() {
  MutexLock lock(&mutex_);
  for (;;) {
    if (!tasks_.empty()) {
      Closure* const task = tasks_.front();
      tasks_.pop_front();
      return task;
    }
    if (waiting_to_finish_) {
      return NULL;
    } else {
      condition_.Wait(&mutex_);
    }
  }
  return NULL;
}

void ThreadPool::Add(Closure* const closure) {
  MutexLock lock(&mutex_);
  tasks_.push_back(closure);
  if (started_) {
    condition_.SignalAll();
  }
}
}  // namespace operations_research
