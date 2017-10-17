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

#include "ortools/base/threadpool.h"

namespace operations_research {
void RunWorker(void* data) {
  ThreadPool* const thread_pool = reinterpret_cast<ThreadPool*>(data);
  std::function<void()> work = thread_pool->GetNextTask();
  while (work != NULL) {
    work();
    work = thread_pool->GetNextTask();
  }
}

ThreadPool::ThreadPool(const std::string& prefix, int num_workers)
    : num_workers_(num_workers), waiting_to_finish_(false), started_(false) {}

ThreadPool::~ThreadPool() {
  if (started_) {
    std::unique_lock<std::mutex> mutex_lock(mutex_);
    waiting_to_finish_ = true;
    mutex_lock.unlock();
    condition_.notify_all();
    for (int i = 0; i < num_workers_; ++i) {
      all_workers_[i].join();
    }
  }
}

void ThreadPool::StartWorkers() {
  started_ = true;
  for (int i = 0; i < num_workers_; ++i) {
    all_workers_.push_back(std::thread(&RunWorker, this));
  }
}

std::function<void()> ThreadPool::GetNextTask() {
  std::unique_lock<std::mutex> lock(mutex_);
  for (;;) {
    if (!tasks_.empty()) {
      std::function<void()> task = tasks_.front();
      tasks_.pop_front();
      return task;
    }
    if (waiting_to_finish_) {
      return nullptr;
    } else {
      condition_.wait(lock);
    }
  }
  return nullptr;
}

void ThreadPool::Schedule(std::function<void()> closure) {
  std::unique_lock<std::mutex> lock(mutex_);
  tasks_.push_back(closure);
  if (started_) {
    lock.unlock();
    condition_.notify_all();
  }
}
}  // namespace operations_research
