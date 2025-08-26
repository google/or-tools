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

#ifndef OR_TOOLS_UTIL_PYTHON_PY_SOLVE_INTERRUPTER_H_
#define OR_TOOLS_UTIL_PYTHON_PY_SOLVE_INTERRUPTER_H_

#include <memory>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "ortools/util/solve_interrupter.h"

namespace operations_research {

// Simple class that wraps a SolveInterrupter for Clif or pybind11.
//
class PySolveInterrupter {
 public:
  PySolveInterrupter();

  PySolveInterrupter(const PySolveInterrupter&) = delete;
  PySolveInterrupter& operator=(const PySolveInterrupter&) = delete;

  // Interrupts the solve as soon as possible.
  void Interrupt() { interrupter_.Interrupt(); }

  // Returns true if the solve interruption has been requested.
  inline bool IsInterrupted() const { return interrupter_.IsInterrupted(); }

  // Triggers the target when this interrupter is triggered.
  //
  // A std::weak_ptr is kept on the target. Expired std::weak_ptr are cleaned up
  // on calls to AddTriggerTarget() and RemoveTriggerTarget().
  //
  // Complexity: O(num_targets).
  void AddTriggerTarget(absl_nonnull std::shared_ptr<PySolveInterrupter> target)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Removes the target if not null and present, else do nothing.
  //
  // Complexity: O(num_targets).
  void RemoveTriggerTarget(
      absl_nonnull std::shared_ptr<PySolveInterrupter> target)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Add a callback on the interrupter and returns an id to use to remove it.
  //
  // See SolveInterrupter::AddInterruptionCallback().
  int64_t AddInterruptionCallback(std::function<void()> callback) const {
    return interrupter_.AddInterruptionCallback(std::move(callback)).value();
  }

  // Remove a callback previously registered by AddInterruptionCallback().
  //
  // See SolveInterrupter::RemoveInterruptionCallback().
  void RemoveInterruptionCallback(int64_t callback_id) const {
    interrupter_.RemoveInterruptionCallback(
        SolveInterrupter::CallbackId{callback_id});
  }

  // Return the underlying interrupter. This method is not exposed in Python and
  // only available for C++ code.
  //
  // The lifetime of the PySolveInterrupter is controlled by Python. To prevent
  // issues where the underlying SolveInterrupter would be destroyed while still
  // being pointed to by C++ code, C++ Clif/pybind11 consumer code should take
  // references to PySolveInterrupter in an std::shared_ptr that outlives any
  // reference.
  const SolveInterrupter* absl_nonnull interrupter() const {
    return &interrupter_;
  }

 private:
  // Remove expired targets_, the optional to_remove target and return strong
  // references on non-expired targets.
  //
  // The caller must have acquired mutex_.
  //
  // Complexity: O(num_targets).
  std::vector<absl_nonnull std::shared_ptr<PySolveInterrupter>>
  CleanupAndGetTargets(const PySolveInterrupter* absl_nullable to_remove =
                           nullptr) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Triggers all non-expired targets_ interrupters.
  void TriggerTargets() ABSL_LOCKS_EXCLUDED(mutex_);

  SolveInterrupter interrupter_;

  absl::Mutex mutex_;

  // Interrupters to trigger when interrupter_ is triggered.
  std::vector<std::weak_ptr<PySolveInterrupter>> targets_
      ABSL_GUARDED_BY(mutex_);

  // Callback that will trigger all interrupters in target_.
  //
  // It MUST appear after targets_ and interrupter_ as we want to make sure it
  // is destroyed first!
  ScopedSolveInterrupterCallback callback_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_PYTHON_PY_SOLVE_INTERRUPTER_H_
