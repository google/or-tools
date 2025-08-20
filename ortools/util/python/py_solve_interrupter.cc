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

#include "ortools/util/python/py_solve_interrupter.h"

#include <memory>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/log/log.h"
#include "absl/synchronization/mutex.h"

namespace operations_research {

PySolveInterrupter::PySolveInterrupter()
    : callback_(&interrupter_, [this]() { TriggerTargets(); }) {}

std::vector<absl_nonnull std::shared_ptr<PySolveInterrupter>>
PySolveInterrupter::CleanupAndGetTargets(
    const PySolveInterrupter* absl_nullable const to_remove) {
  // First get strong references of non-expired targets. Also filter the
  // to_remove target if found.
  std::vector<absl_nonnull std::shared_ptr<PySolveInterrupter>>
      non_expired_targets;
  non_expired_targets.reserve(targets_.size());
  for (std::weak_ptr<PySolveInterrupter>& weak_target : targets_) {
    std::shared_ptr<PySolveInterrupter> strong_target = weak_target.lock();
    if (strong_target != nullptr && strong_target.get() != to_remove) {
      non_expired_targets.push_back(std::move(strong_target));
    }
  }

  // Then recreates targets weak-references with only the non-expired targets.
  //
  // Note that we could be more efficient by doing the cleanup in-place but here
  // we keep the code simple. In practice the number of targets_ is expected to
  // be very low (less than 10).
  targets_.clear();
  for (std::shared_ptr<PySolveInterrupter>& strong_target :
       non_expired_targets) {
    targets_.push_back(strong_target);
  }

  return non_expired_targets;
}

void PySolveInterrupter::AddTriggerTarget(
    absl_nonnull std::shared_ptr<PySolveInterrupter> target) {
  const absl::MutexLock lock(&mutex_);
  CleanupAndGetTargets();
  // Note that we don't test if targets_ already contain the interrupter as
  // interrupter are triggerable only once. Thus having duplicates won't result
  // in visible changes outside.
  //
  // The way RemoveTriggerTarget() is implemented will remove duplicates as
  // well.
  targets_.push_back(std::move(target));
}

void PySolveInterrupter::RemoveTriggerTarget(
    absl_nonnull std::shared_ptr<PySolveInterrupter> target) {
  const absl::MutexLock lock(&mutex_);
  CleanupAndGetTargets(/*to_remove=*/target.get());
}

void PySolveInterrupter::TriggerTargets() {
  std::vector<absl_nonnull std::shared_ptr<PySolveInterrupter>> targets;
  {  // Limit `lock` scope.
    const absl::MutexLock lock(&mutex_);
    targets = CleanupAndGetTargets();
  }

  // Call targets without holding mutex_.
  for (const absl_nonnull std::shared_ptr<PySolveInterrupter>& target :
       targets) {
    target->Interrupt();
  }
}

}  // namespace operations_research
