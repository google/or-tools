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

#include "ortools/util/solve_interrupter.h"

#include <atomic>
#include <optional>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "ortools/base/linked_hash_map.h"
#include "ortools/base/logging.h"

namespace operations_research {

void SolveInterrupter::Interrupt() {
  const absl::MutexLock lock(mutex_);

  // Here we don't use compare_exchange_strong since we need to hold the lock
  // before changing the value of interrupted_ anyway. So there is no need to
  // use this complex function.
  if (interrupted_.load()) {
    // We must not call the callbacks more than once.
    return;
  }

  // We need to change this value while holding the lock since in
  // AddInterruptionCallback() we must know if we need to call the new callback
  // of if this function has called it.
  interrupted_ = true;

  // We are holding the lock while calling callbacks. This makes it impossible
  // to call Interrupt(), AddInterruptionCallback(), or
  // RemoveInterruptionCallback() from a callback but it ensures that external
  // code that can modify callbacks_ will wait until the end of Interrupt.
  for (auto& [callback_id, callback] : callbacks_) {
    // We can only have nullptr if:
    // * interrupted_ was true when AddInterruptionCallback() was called,
    // * or a previous Interrupt() has set the pointer to null.
    // In these two cases we should not reach this code.
    CHECK(callback != nullptr);
    std::move(callback)();
    callback = nullptr;
  }
}

SolveInterrupter::CallbackId SolveInterrupter::AddInterruptionCallback(
    absl_nonnull Callback callback) const {
  const absl::MutexLock lock(mutex_);

  const CallbackId id = next_callback_id_;
  ++next_callback_id_;

  const auto [it, inserted] = callbacks_.try_emplace(id, std::move(callback));
  CHECK(inserted);

  // We must make this call while holding the lock since we want to be sure that
  // the calls to the callbacks_ won't occur before we registered the new
  // one. If we were not holding the lock, this could return false and before we
  // could add the new callback to callbacks_, the Interrupt() function may
  // still have called them.
  if (interrupted_.load()) {
    std::move(it->second)();
    it->second = nullptr;
  }

  return id;
}

void SolveInterrupter::RemoveInterruptionCallback(CallbackId id) const {
  const absl::MutexLock lock(mutex_);
  CHECK_EQ(callbacks_.erase(id), 1) << "unregistered callback id: " << id;
}

ScopedSolveInterrupterCallback::ScopedSolveInterrupterCallback(
    const SolveInterrupter* absl_nullable const interrupter,
    absl_nonnull SolveInterrupter::Callback callback)
    : interrupter_(interrupter),
      callback_id_(
          interrupter != nullptr
              ? std::make_optional(
                    interrupter->AddInterruptionCallback(std::move(callback)))
              : std::nullopt) {}

ScopedSolveInterrupterCallback::~ScopedSolveInterrupterCallback() {
  RemoveCallbackIfNecessary();
}

void ScopedSolveInterrupterCallback::RemoveCallbackIfNecessary() {
  if (callback_id_) {
    CHECK_NE(interrupter_, nullptr);
    interrupter_->RemoveInterruptionCallback(*callback_id_);
    callback_id_.reset();
  }
}

}  // namespace operations_research
