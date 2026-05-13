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

#include "ortools/math_opt/core/concurrent_calls_guard.h"

#include "absl/base/thread_annotations.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"

namespace operations_research::math_opt {

absl::StatusOr<ConcurrentCallsGuard> ConcurrentCallsGuard::TryAcquire(
    Tracker& tracker) ABSL_NO_THREAD_SAFETY_ANALYSIS {
  {
    const absl::MutexLock lock(&tracker.mutex_);
    if (tracker.in_a_call_) {
      return absl::InvalidArgumentError("concurrent calls are forbidden");
    }
    tracker.in_a_call_ = true;
  }
  return ConcurrentCallsGuard(&tracker);
}

ConcurrentCallsGuard::~ConcurrentCallsGuard() {
  if (tracker_ == nullptr) {
    return;
  }

  const absl::MutexLock lock(&tracker_->mutex_);
  DCHECK(tracker_->in_a_call_);
  tracker_->in_a_call_ = false;
}

}  // namespace operations_research::math_opt
