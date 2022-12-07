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

#ifndef OR_TOOLS_MATH_OPT_CORE_SOLVE_INTERRUPTER_H_
#define OR_TOOLS_MATH_OPT_CORE_SOLVE_INTERRUPTER_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "ortools/base/linked_hash_map.h"
#include "ortools/base/strong_int.h"

namespace operations_research {
namespace math_opt {

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

  using Callback = std::function<void()>;

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
  CallbackId AddInterruptionCallback(Callback callback);

  // Unregisters a callback previously registered. It fails (with a CHECK) if
  // the callback was already unregistered or unkonwn. After this calls returns,
  // the caller can assume the callback won't be called.
  //
  // This function can't be called from a callback since this would result in a
  // deadlock.
  void RemoveInterruptionCallback(CallbackId id);

 private:
  // This atomic must never be reset to false!
  //
  // The mutex_ should be held when setting it to true.
  std::atomic<bool> interrupted_ = false;

  absl::Mutex mutex_;

  // The id to use for the next registered callback.
  CallbackId next_callback_id_ ABSL_GUARDED_BY(mutex_) = {};

  // The list of callbacks. We use a linked_hash_map to make sure the order of
  // calls to callback when the interrupter is triggered is stable.
  gtl::linked_hash_map<CallbackId, Callback> callbacks_ ABSL_GUARDED_BY(mutex_);
};

// Class implementing RAII for interruption callbacks.
//
// Usage:
//
//   SolveInterrupter* const interrupter = ...;
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
  ScopedSolveInterrupterCallback(SolveInterrupter* interrupter,
                                 SolveInterrupter::Callback callback);

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
  SolveInterrupter* interrupter() const { return interrupter_; }

 private:
  // Optional interrupter.
  SolveInterrupter* const interrupter_;

  // Unset after the callback has been reset.
  std::optional<SolveInterrupter::CallbackId> callback_id_;
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CORE_SOLVE_INTERRUPTER_H_
