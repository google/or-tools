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

#ifndef OR_TOOLS_BASE_NOTIFICATION_H_
#define OR_TOOLS_BASE_NOTIFICATION_H_

#include <atomic>
#include <condition_variable>  // NOLINT
#include <mutex>               // NOLINT

namespace absl {

// -----------------------------------------------------------------------------
// Notification
// -----------------------------------------------------------------------------
class Notification {
 public:
  // Initializes the "notified" state to unnotified.
  Notification() : notified_yet_(false) {}
  Notification(const Notification&) = delete;
  Notification& operator=(const Notification&) = delete;
  ~Notification();

  // Notification::HasBeenNotified()
  //
  // Returns the value of the notification's internal "notified" state.
  bool HasBeenNotified() const;

  // Notification::WaitForNotification()
  //
  // Blocks the calling thread until the notification's "notified" state is
  // `true`. Note that if `Notify()` has been previously called on this
  // notification, this function will immediately return.
  void WaitForNotification();

  // Notification::Notify()
  //
  // Sets the "notified" state of this notification to `true` and wakes waiting
  // threads. Note: do not call `Notify()` multiple times on the same
  // `Notification`; calling `Notify()` more than once on the same notification
  // results in undefined behavior.
  void Notify();

 private:
  std::mutex mutex_;
  std::condition_variable condition_;
  std::atomic<bool> notified_yet_;  // written under mutex_
};

}  // namespace absl

#endif  // OR_TOOLS_BASE_NOTIFICATION_H_
