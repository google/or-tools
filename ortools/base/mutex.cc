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

#include <thread>  // NOLINT

#include "ortools/base/mutex.h"

namespace operations_research {
Mutex::Mutex() {}
Mutex::~Mutex() {}
void Mutex::Lock() { real_mutex_.lock(); }
void Mutex::Unlock() { real_mutex_.unlock(); }
bool Mutex::TryLock() { return real_mutex_.try_lock(); }

CondVar::CondVar() {}
CondVar::~CondVar() {}
void CondVar::Wait(Mutex* const mu) {
  std::unique_lock<std::mutex> mutex_lock(mu->real_mutex_);
  real_condition_.wait(mutex_lock);
}
void CondVar::Signal() { real_condition_.notify_one(); }
void CondVar::SignalAll() { real_condition_.notify_all(); }
}  // namespace operations_research
