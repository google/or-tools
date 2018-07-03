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

#include "ortools/base/notification.h"

#include <atomic>
#include <condition_variable>  // NOLINT
#include <mutex>               // NOLINT

namespace absl {
void Notification::Notify() {
  std::unique_lock<std::mutex> mutex_lock(mutex_);
  notified_yet_.store(true, std::memory_order_release);
  condition_.notify_all();
}

Notification::~Notification() {
  // Make sure that the thread running Notify() exits before the object is
  // destructed.
  std::unique_lock<std::mutex> mutex_lock(mutex_);
}

static inline bool HasBeenNotifiedInternal(
    const std::atomic<bool> *notified_yet) {
  return notified_yet->load(std::memory_order_acquire);
}

bool Notification::HasBeenNotified() const {
  return HasBeenNotifiedInternal(&this->notified_yet_);
}

void Notification::WaitForNotification() {
  if (!HasBeenNotifiedInternal(&this->notified_yet_)) {
    std::unique_lock<std::mutex> mutex_lock(mutex_);
    condition_.wait(mutex_lock);
  }
}

}  // namespace absl
