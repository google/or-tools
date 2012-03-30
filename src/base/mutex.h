// Copyright 2010-2012 Google
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

#include "base/macros.h"
#include "tinythread.h"

namespace operations_research {
class Mutex {
 public:
  Mutex() {}

  ~Mutex() {}

  void Lock() { real_mutex_.lock(); }
  void Unlock() { real_mutex_.unlock(); }
  bool TryLock() { real_mutex_.try_lock(); }

  tthread::mutex* RealMutex() { return &real_mutex_; }

 private:
  tthread::mutex real_mutex_;
  DISALLOW_COPY_AND_ASSIGN(Mutex);
};

class MutexLock {
 public:
  explicit MutexLock(Mutex *mutex) : mutex_(mutex) {
    this->mutex_->Lock();
  }

  ~MutexLock() { this->mutex_->Unlock(); }

 private:
  Mutex* const mutex_;
  DISALLOW_COPY_AND_ASSIGN(MutexLock);
};

class CondVar {
 public:
  CondVar() : real_condition_() {}
  ~CondVar() {}

  void Wait(Mutex* const mu) {
    real_condition_.wait(*mu->RealMutex());
  }

  void Signal() { real_condition_.notify_one(); }

  void SignalAll() { real_condition_.notify_all(); }

 private:
  tthread::condition_variable real_condition_;
  DISALLOW_COPY_AND_ASSIGN(CondVar);
};
}  // namespace operations_research
#endif  // OR_TOOLS_BASE_MUTEX_H_
