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

#ifndef OR_TOOLS_MATH_OPT_STORAGE_UPDATE_TRACKERS_H_
#define OR_TOOLS_MATH_OPT_STORAGE_UPDATE_TRACKERS_H_

#include <algorithm>
#include <atomic>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/mutex.h"
#include "absl/log/check.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/storage/model_storage_types.h"

namespace operations_research::math_opt {

// Manager the collection of update trackers for ModelStorage.
//
// The Data type is the type of data associated with trackers.
//
// This class makes sure it is possible to iterate on update trackers for
// ModelStorage modifications without having to hold a mutex. It does that by
// delaying additions & removals so that they are only applied when we need to
// iterate. This enables adding or removing trackers concurrently from multiple
// threads.
template <typename Data>
class UpdateTrackers {
 public:
  // A pair (tracker_id, tracker_data).
  using IdDataPair = std::pair<UpdateTrackerId, std::unique_ptr<Data>>;

  // Adds a new tracker. The args are forwarded to the constructor of the Data
  // type.
  //
  // The actual addition is delayed to the next call of GetUpdatedTrackers().
  //
  // Thread-safety: this method is safe to be called from multiple threads at
  // the same time.
  template <typename... T>
  UpdateTrackerId NewUpdateTracker(T&&... args);

  // Removes an update tracker.
  //
  // The actual removal is delayed to the next call of GetUpdatedTrackers().
  //
  // Thread-safety: this method is safe to be called from multiple threads at
  // the same time. Since the update of vector returned by GetUpdatedTrackers()
  // is delayed it is safe to iterate on it while this method is called.
  //
  // Complexity: O(n), n is the number of trackers. The number of update
  // trackers should be small, if it grows to a point where this is an issue
  // this class can easily be made more efficient (O(lg n) or better).
  void DeleteUpdateTracker(UpdateTrackerId update_tracker);

  // Apply pending additions/deletions and return the trackers.
  //
  // Note that this function returns a const-reference to the vector of
  // trackers, but since it contains (unique-)pointers on the trackers' Data,
  // only the pointers are const not the pointee. It is thus possible (and
  // expected) that callers modify the pointed Data.
  //
  // Thread-safety: this method should not be called from multiple threads as
  // the result is not protected by a mutex and thus could be change by the
  // other call. Note though that concurrent calls to NewUpdateTracker() and
  // DeleteUpdateTracker() are fine since the changes will only be applied on
  // the next call to this function.
  const std::vector<IdDataPair>& GetUpdatedTrackers();

  // Returns the data corresponding to the provided tracker. It CHECKs that the
  // tracker exists.
  //
  // It does not apply the pending actions (so that it can have a const
  // overload), thus the result of GetUpdatedTrackers() is not modified.
  //
  // Thread-safety: this method can be called from multiple threads but the
  // resulting reference can be invalidated if the returned tracker is deleted
  // and GetUpdatedTrackers() is called.
  //
  // Complexity: O(n) where n is the number of trackers. The number of update
  // trackers should be small, if it grows to a point where this is an issue
  // this class can easily be made more efficient (O(lg n) or better).
  Data& GetData(UpdateTrackerId update_tracker);
  const Data& GetData(UpdateTrackerId update_tracker) const;

 private:
  // Returns the iterator pointing to the provided tracker, or v.end() if not
  // found.
  //
  // Complexity: O(n) where n is the number of trackers. Can be optimized to
  // O(lg n) easily if needed.
  static typename std::vector<IdDataPair>::iterator FindTracker(
      std::vector<IdDataPair>& v, UpdateTrackerId update_tracker);
  static typename std::vector<IdDataPair>::const_iterator FindTracker(
      const std::vector<IdDataPair>& v, UpdateTrackerId update_tracker);

  // Updates has_pending_actions_ to be true iff pending_new_trackers_ or
  // pending_removed_trackers_ are not empty.
  void UpdateHasPendingActions() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable absl::Mutex mutex_;

  // Next index to use in NewUpdateTracker().
  UpdateTrackerId next_update_tracker_ ABSL_GUARDED_BY(mutex_) = {};

  // New trackers not yet added to trackers_.
  //
  // Invariants: trackers in this collection are not in trackers_ or in
  // pending_removed_trackers_.
  std::vector<IdDataPair> pending_new_trackers_ ABSL_GUARDED_BY(mutex_);

  // Trackers returned by GetUpdatedTrackers().
  std::vector<IdDataPair> trackers_;

  // Trackers to be removed.
  //
  // Invariants: trackers in this collection must only be in trackers_. When
  // trackers in pending_new_trackers_ are deleted, they are simply removed from
  // pending_new_trackers_.
  absl::flat_hash_set<UpdateTrackerId> pending_removed_trackers_
      ABSL_GUARDED_BY(mutex_);

  // Set to true iff pending_new_trackers_ or pending_removed_trackers_ are not
  // empty. This must be set only while holding mutex_ but will be loaded
  // without it. This atomic enables not having the performance penalty of
  // acquiring a mutex in each call to GetUpdatedTrackers().
  //
  // This should not be set directly, instead UpdateHasPendingActions() should
  // be called.
  std::atomic<bool> has_pending_actions_ = false;
};

////////////////////////////////////////////////////////////////////////////////
// Inline implementations.
////////////////////////////////////////////////////////////////////////////////

template <typename Data>
void UpdateTrackers<Data>::UpdateHasPendingActions() {
  has_pending_actions_ =
      !pending_new_trackers_.empty() || !pending_removed_trackers_.empty();
}

template <typename Data>
template <typename... Args>
UpdateTrackerId UpdateTrackers<Data>::NewUpdateTracker(Args&&... args) {
  const absl::MutexLock lock(&mutex_);

  const UpdateTrackerId update_tracker = next_update_tracker_;
  ++next_update_tracker_;

  pending_new_trackers_.push_back(std::make_pair(
      update_tracker, std::make_unique<Data>(std::forward<Args>(args)...)));
  UpdateHasPendingActions();

  return update_tracker;
}

template <typename Data>
typename std::vector<typename UpdateTrackers<Data>::IdDataPair>::iterator
UpdateTrackers<Data>::FindTracker(std::vector<IdDataPair>& v,
                                  const UpdateTrackerId update_tracker) {
  return std::find_if(v.begin(), v.end(), [&](const IdDataPair& pair) {
    return pair.first == update_tracker;
  });
}

template <typename Data>
typename std::vector<typename UpdateTrackers<Data>::IdDataPair>::const_iterator
UpdateTrackers<Data>::FindTracker(const std::vector<IdDataPair>& v,
                                  const UpdateTrackerId update_tracker) {
  return std::find_if(v.begin(), v.end(), [&](const IdDataPair& pair) {
    return pair.first == update_tracker;
  });
}

template <typename Data>
void UpdateTrackers<Data>::DeleteUpdateTracker(
    const UpdateTrackerId update_tracker) {
  const absl::MutexLock lock(&mutex_);

  // The delete trackers may still be in pending_new_trackers_. We have to
  // remove it from there.
  {
    const auto found_new = FindTracker(pending_new_trackers_, update_tracker);
    if (found_new != pending_new_trackers_.end()) {
      pending_new_trackers_.erase(found_new);
      UpdateHasPendingActions();
      return;
    }
  }

  // The deleted trackers could already be in pending_removed_trackers_, which
  // would be an issue since trackers can't be removed multiple times.
  CHECK(pending_removed_trackers_.find(update_tracker) ==
        pending_removed_trackers_.end())
      << "Update tracker " << update_tracker << " does not exist";

  // Test that the tracker actually exists.
  const auto found_existing = FindTracker(trackers_, update_tracker);
  CHECK(found_existing != trackers_.end())
      << "Update tracker " << update_tracker << " does not exist";

  pending_removed_trackers_.insert(update_tracker);
  UpdateHasPendingActions();
}

template <typename Data>
const std::vector<typename UpdateTrackers<Data>::IdDataPair>&
UpdateTrackers<Data>::GetUpdatedTrackers() {
  // Here we use the "relaxed" memory order since we don't want or need to
  // ensure proper ordering of memory accesses. We only want to make sure it is
  // safe to delete a tracker from another thread while we read it
  // here. Temporarily modifying a deleted tracker has no downside effect; we
  // only care that the tracker will eventually be deleted, not that it is
  // deleted as soon as possible.
  //
  // Using a non-relaxed memory order has significant impact on performances
  // here.
  if (has_pending_actions_.load(std::memory_order_relaxed)) {
    const absl::MutexLock lock(&mutex_);

    // Flush removed trackers.
    gtl::STLEraseAllFromSequenceIf(
        &trackers_,
        [&](const IdDataPair& pair) ABSL_SHARED_LOCKS_REQUIRED(mutex_) {
          const auto found = pending_removed_trackers_.find(pair.first);
          if (found == pending_removed_trackers_.end()) {
            return false;
          }
          pending_removed_trackers_.erase(found);
          return true;
        });
    DCHECK(pending_removed_trackers_.empty());
    pending_removed_trackers_.clear();  // Safeguard.

    // Move new trackers.
    trackers_.insert(trackers_.end(),
                     std::make_move_iterator(pending_new_trackers_.begin()),
                     std::make_move_iterator(pending_new_trackers_.end()));
    pending_new_trackers_.clear();

    UpdateHasPendingActions();
  }
  return trackers_;
}

template <typename Data>
Data& UpdateTrackers<Data>::GetData(UpdateTrackerId update_tracker) {
  const absl::MutexLock lock(&mutex_);
  // Note that here we could apply the pending actions. We don't do so to keep
  // the symmetry with the const overload below.

  // The tracker may still be in pending_new_trackers_.
  {
    const auto found_new = FindTracker(pending_new_trackers_, update_tracker);
    if (found_new != pending_new_trackers_.end()) {
      return *found_new->second;
    }
  }

  // The tracker could be in pending_removed_trackers_.
  CHECK(pending_removed_trackers_.find(update_tracker) ==
        pending_removed_trackers_.end())
      << "Update tracker " << update_tracker << " does not exist";

  // The tracker must be in trackers_.
  const auto found_existing = FindTracker(trackers_, update_tracker);
  CHECK(found_existing != trackers_.end())
      << "Update tracker " << update_tracker << " does not exist";

  return *found_existing->second;
}

template <typename Data>
const Data& UpdateTrackers<Data>::GetData(
    UpdateTrackerId update_tracker) const {
  const absl::MutexLock lock(&mutex_);
  // The tracker may still be in pending_new_trackers_.
  {
    const auto found_new = FindTracker(pending_new_trackers_, update_tracker);
    if (found_new != pending_new_trackers_.end()) {
      return *found_new->second;
    }
  }

  // The tracker could be in pending_removed_trackers_.
  CHECK(pending_removed_trackers_.find(update_tracker) ==
        pending_removed_trackers_.end())
      << "Update tracker " << update_tracker << " does not exist";

  // The tracker must be in trackers_.
  const auto found_existing = FindTracker(trackers_, update_tracker);
  CHECK(found_existing != trackers_.end())
      << "Update tracker " << update_tracker << " does not exist";

  return *found_existing->second;
}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_STORAGE_UPDATE_TRACKERS_H_
