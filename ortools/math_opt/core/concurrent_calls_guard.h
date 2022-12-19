// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_MATH_OPT_CORE_CONCURRENT_CALLS_GUARD_H_
#define OR_TOOLS_MATH_OPT_CORE_CONCURRENT_CALLS_GUARD_H_

#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"

namespace operations_research::math_opt {

// RAII class that is used to return an error when concurrent calls to some
// functions are made.
//
// Usage:
//
//   // Calling f() and/or g() concurrently will return an error.
//   class A {
//    public:
//     absl::StatusOr<...> f() {
//       ASSIGN_OR_RETURN(const auto guard,
//                        ConcurrentCallsGuard::TryAcquire(tracker_));
//       ...
//     }
//
//     absl::StatusOr<...> g() {
//       ASSIGN_OR_RETURN(const auto guard,
//                        ConcurrentCallsGuard::TryAcquire(tracker_));
//       ...
//     }
//
//    private:
//     ConcurrentCallsGuard::Tracker tracker_;
//   };
//
class ConcurrentCallsGuard {
 public:
  // Tracker for pending calls.
  class Tracker {
   public:
    Tracker() = default;

    Tracker(const Tracker&) = delete;
    Tracker& operator=(const Tracker&) = delete;

   private:
    friend class ConcurrentCallsGuard;

    absl::Mutex mutex_;
    bool in_a_call_ ABSL_GUARDED_BY(mutex_) = false;
  };

  // Returns an errors status when concurrent calls are made, or a guard that
  // must only be kept on stack during the execution of the call.
  static absl::StatusOr<ConcurrentCallsGuard> TryAcquire(Tracker& tracker);

  ConcurrentCallsGuard(const ConcurrentCallsGuard&) = delete;
  ConcurrentCallsGuard& operator=(const ConcurrentCallsGuard&) = delete;
  ConcurrentCallsGuard& operator=(ConcurrentCallsGuard&&) = delete;

  ConcurrentCallsGuard(ConcurrentCallsGuard&& other)
      : tracker_(std::exchange(other.tracker_, nullptr)) {}

  // Release the guard.
  ~ConcurrentCallsGuard();

 private:
  // Asserts that the mutex is held by the current thread.
  explicit ConcurrentCallsGuard(Tracker* const tracker) : tracker_(tracker) {}

  // Reset to nullptr when the class is moved by the move constructor.
  Tracker* tracker_;
};

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_CORE_CONCURRENT_CALLS_GUARD_H_
