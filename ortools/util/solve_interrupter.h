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

#ifndef ORTOOLS_UTIL_SOLVE_INTERRUPTER_H_
#define ORTOOLS_UTIL_SOLVE_INTERRUPTER_H_

#include <atomic>
#include <cstdint>
#include <optional>

#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "ortools/base/linked_hash_map.h"
#include "ortools/base/strong_int.h"

namespace operations_research {

// Interrupter used by solvers to know if/when they should interrupt the solve.
//
// Once triggered with Interrupt(), an interrupter can't be reset. It can be
// triggered from any thread.
//
// Thread-safety: APIs on this class are safe to call concurrently from multiple
// threads.
class SolveInterrupter {
 public:
  // Id used to identify a callback.
  DEFINE_STRONG_INT_TYPE(CallbackId, int64_t);

  using Callback = absl::AnyInvocable<void() &&>;

  SolveInterrupter() = default;

  SolveInterrupter(const SolveInterrupter&) = delete;
  SolveInterrupter& operator=(const SolveInterrupter&) = delete;

  // Interrupts the solve as soon as possible.
  //
  // Once requested the interruption can't be reset. The user should use a new
  // SolveInterrupter for later solves.
  //
  // It is safe to call this function multiple times. Only the first call will
  // have visible effects; other calls will be ignored.
  void Interrupt();

  // Returns true if the solve interruption has been requested.
  //
  // This API is fast; it costs the read of an atomic.
  inline bool IsInterrupted() const { return interrupted_.load(); }

  // Registers a callback to be called when the interruption is requested.
  //
  // The callback is immediately called if the interrupter has already been
  // triggered or if it is triggered during the registration. This is typically
  // useful for a solver implementation so that it does not have to test
  // IsInterrupted() to do the same thing it does in the callback. Simply
  // registering the callback is enough.
  //
  // The callback function can't make calls to AddInterruptionCallback(),
  // RemoveInterruptionCallback() and Interrupt(). This would result is a
  // deadlock. Calling IsInterrupted() is fine though.
  //
  // This method is `const` since it does not modify the state of the
  // interrupter (the result of IsInterrupted()). This enables passing a
  // const-ref to solvers, making sure they can't call Interrupt() by mistake.
  CallbackId AddInterruptionCallback(absl_nonnull Callback callback) const;

  // Unregisters a callback previously registered. It fails (with a CHECK) if
  // the callback was already unregistered or unkonwn. After this calls returns,
  // the caller can assume the callback won't be called.
  //
  // This function can't be called from a callback since this would result in a
  // deadlock.
  void RemoveInterruptionCallback(CallbackId id) const;

 private:
  // This atomic must never be reset to false!
  //
  // The mutex_ should be held when setting it to true.
  std::atomic<bool> interrupted_ = false;

  mutable absl::Mutex mutex_;

  // The id to use for the next registered callback.
  mutable CallbackId next_callback_id_ ABSL_GUARDED_BY(mutex_) = {};

  // The list of callbacks. We use a linked_hash_map to make sure the order of
  // calls to callback when the interrupter is triggered is stable.
  //
  // The values are absl_nullable since the value will be nullptr:
  // * either if AddInterruptionCallback() is called after Interrupt(), in which
  //   case the callback is called before AddInterruptionCallback() returns,
  // * or after Interrupt() has been called.
  //
  // This reflects the fact that callback can only be called once.
  mutable gtl::linked_hash_map<CallbackId, absl_nullable Callback> callbacks_
      ABSL_GUARDED_BY(mutex_);
};

// Class implementing RAII for interruption callbacks.
//
// Usage:
//
//   const SolveInterrupter* const interrupter = ...;
//   {
//     const ScopedSolveInterrupterCallback scoped_intr_cb(interrupter, [](){
//       // Do something when/if interrupter is not nullptr and is triggered.
//     }
//     ...
//   }
//   // At this point, the callback will have been removed.
//
// The function RemoveCallbackIfNecessary() can be used to remove the callback
// before the destruction of this object.
class ScopedSolveInterrupterCallback {
 public:
  // Adds a callback to the interrupter if it is not nullptr. Does nothing when
  // interrupter is nullptr.
  ScopedSolveInterrupterCallback(
      const SolveInterrupter* absl_nullable interrupter,
      absl_nonnull SolveInterrupter::Callback callback);

  ScopedSolveInterrupterCallback(const ScopedSolveInterrupterCallback&) =
      delete;
  ScopedSolveInterrupterCallback& operator=(
      const ScopedSolveInterrupterCallback&) = delete;

  // Removes the callback if necessary.
  ~ScopedSolveInterrupterCallback();

  // Removes the callback from the interrupter. If it has already been removed
  // by a previous call or if a null interrupter was passed to the constructor,
  // this function has no effect.
  void RemoveCallbackIfNecessary();

  // Returns the optional interrupter.
  const SolveInterrupter* absl_nullable interrupter() const {
    return interrupter_;
  }

 private:
  // Optional interrupter.
  const SolveInterrupter* absl_nullable const interrupter_;

  // Unset after the callback has been reset.
  std::optional<SolveInterrupter::CallbackId> callback_id_;
};

}  // namespace operations_research

#endif  // ORTOOLS_UTIL_SOLVE_INTERRUPTER_H_
