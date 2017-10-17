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

#ifndef OR_TOOLS_BASE_MUTEX_H_
#define OR_TOOLS_BASE_MUTEX_H_

#include <condition_variable>  // NOLINT
#include <mutex>  // NOLINT

#include "ortools/base/macros.h"

namespace operations_research {
class Mutex {
 public:
  Mutex();
  ~Mutex();
  void Lock();
  void Unlock();
  bool TryLock();

  friend class CondVar;

 private:
  std::mutex real_mutex_;
  DISALLOW_COPY_AND_ASSIGN(Mutex);
};

class MutexLock {
 public:
  explicit MutexLock(Mutex* const mutex) : mutex_(mutex) {
    this->mutex_->Lock();
  }

  ~MutexLock() { this->mutex_->Unlock(); }

 private:
  Mutex* const mutex_;
  DISALLOW_COPY_AND_ASSIGN(MutexLock);
};

class CondVar {
 public:
  CondVar();
  ~CondVar();
  void Wait(Mutex* const mu);
  void Signal();
  void SignalAll();

 private:
  std::condition_variable real_condition_;
  DISALLOW_COPY_AND_ASSIGN(CondVar);
};

// Checking macros.
#define EXCLUSIVE_LOCK_FUNCTION(x)
#define UNLOCK_FUNCTION(x)
#define LOCKS_EXCLUDED(x)
#define EXCLUSIVE_LOCKS_REQUIRED(x)
#define NO_THREAD_SAFETY_ANALYSIS
#define GUARDED_BY(x)
}  // namespace operations_research
#endif  // OR_TOOLS_BASE_MUTEX_H_
