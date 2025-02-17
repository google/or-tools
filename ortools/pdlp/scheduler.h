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

#ifndef PDLP_SCHEDULER_H_
#define PDLP_SCHEDULER_H_

// Eigen defaults to using TensorFlow's scheduler, unless we add this line.
#ifndef EIGEN_USE_CUSTOM_THREAD_POOL
#define EIGEN_USE_CUSTOM_THREAD_POOL
#endif

#include <memory>
#include <string>

#include "absl/functional/any_invocable.h"
#include "absl/synchronization/blocking_counter.h"
#include "ortools/base/threadpool.h"
#include "ortools/pdlp/solvers.pb.h"
#include "unsupported/Eigen/CXX11/ThreadPool"

namespace operations_research::pdlp {

// Thread scheduling interface.
class Scheduler {
 public:
  virtual ~Scheduler() = default;
  virtual int num_threads() const = 0;
  virtual std::string info_string() const = 0;

  // Calls `do_func(i)` in parallel for `i` from `start` to `end-1`.
  virtual void ParallelFor(int start, int end,
                           absl::AnyInvocable<void(int)> do_func) = 0;
};

// Google3 ThreadPool scheduler with barrier synchronization.
class GoogleThreadPoolScheduler : public Scheduler {
 public:
  GoogleThreadPoolScheduler(int num_threads)
      : num_threads_(num_threads),
        threadpool_(std::make_unique<ThreadPool>("pdlp", num_threads)) {
    threadpool_->StartWorkers();
  }
  int num_threads() const override { return num_threads_; };
  std::string info_string() const override { return "google_threadpool"; };

  void ParallelFor(int start, int end,
                   absl::AnyInvocable<void(int)> do_func) override {
    absl::BlockingCounter counter(end - start);
    for (int i = start; i < end; ++i) {
      threadpool_->Schedule([&, i]() {
        do_func(i);
        counter.DecrementCount();
      });
    }
    counter.Wait();
  }

 private:
  const int num_threads_;
  std::unique_ptr<ThreadPool> threadpool_ = nullptr;
};

// Eigen ThreadPool scheduler with barrier synchronization.
class EigenThreadPoolScheduler : public Scheduler {
 public:
  EigenThreadPoolScheduler(int num_threads)
      : num_threads_(num_threads),
        eigen_threadpool_(std::make_unique<Eigen::ThreadPool>(num_threads)) {}
  int num_threads() const override { return num_threads_; };
  std::string info_string() const override { return "eigen_threadpool"; };

  void ParallelFor(int start, int end,
                   absl::AnyInvocable<void(int)> do_func) override {
    Eigen::Barrier eigen_barrier(end - start);
    for (int i = start; i < end; ++i) {
      eigen_threadpool_->Schedule([&, i]() {
        do_func(i);
        eigen_barrier.Notify();
      });
    }
    eigen_barrier.Wait();
  }

 private:
  const int num_threads_;
  std::unique_ptr<Eigen::ThreadPool> eigen_threadpool_ = nullptr;
};

// Makes a scheduler of a given type.
std::unique_ptr<Scheduler> MakeScheduler(SchedulerType type, int num_threads);

}  // namespace operations_research::pdlp

#endif  // PDLP_SCHEDULER_H_
