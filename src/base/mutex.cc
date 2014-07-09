// Copyright 2010-2014 Google
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
#include "base/mutex.h"
#include "tinythread.cpp"

namespace operations_research {
Mutex::Mutex() : real_mutex_(new tthread::mutex) {}
Mutex::~Mutex() {}
void Mutex::Lock() { real_mutex_->lock(); }
void Mutex::Unlock() { real_mutex_->unlock(); }
bool Mutex::TryLock() { return real_mutex_->try_lock(); }
tthread::mutex* Mutex::RealMutex() const { return real_mutex_.get(); }

CondVar::CondVar() : real_condition_(new tthread::condition_variable) {}
CondVar::~CondVar() {}
void CondVar::Wait(Mutex* const mu) { real_condition_->wait(*mu->RealMutex()); }
void CondVar::Signal() { real_condition_->notify_one(); }
void CondVar::SignalAll() { real_condition_->notify_all(); }
}  // namespace operations_research
