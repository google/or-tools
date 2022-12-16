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

#ifndef OR_TOOLS_MATH_OPT_ELEMENTAL_THREAD_SAFE_ID_MAP_H_
#define OR_TOOLS_MATH_OPT_ELEMENTAL_THREAD_SAFE_ID_MAP_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "ortools/base/stl_util.h"

namespace operations_research::math_opt {

// A map from int64_t ids to V, where the ids are created by this map and handed
// out sequentially.
//
// The underlying storage for this map is vector<pair<int64_t, unique_ptr<V>>>.
// Insertions and deletions from this vector are done lazily whenever any of the
// functions UpdateXXX() are invoked.
//
// At a high level, the purpose of this class is to allow for the thread-safe
// removal of elements from the map, while have as little overhead as possible
// when iterating over the elements of the map (`UpdateAndGetAll()`).
// In particular, in the common case where there is nothing to update,
// `UpdateAndGetAll()` only incurs the cost of a single atomic read with
// std::memory_order_relaxed, which is much faster than acquiring a lock on a
// mutex.
//
// This map has pointer stability for values, users can only insert by providing
// a `unique_ptr<V>`.
//
// The functions of this class are mutually thread-safe. However, the functions:
//  * UpdateAndGetAll()
//  * GetAll()
//  * UpdateAndGet()
//  * Get()
// return pointers that:
//  * can be invalidated other function calls on this class,
//  * are not const and may contain mutable data (if V is mutable).
// Thus there are some limitations on the use of this class in a concurrent
// context. Each of these functions above returning pointers documents their
// invalidation conditions inline. Most importantly, it is safe for a single
// thread to invoke UpdateAndGetAll(), and then modify the returned V*, with an
// arbitrary number of concurrent calls to Erase().
template <typename V>
class ThreadSafeIdMap {
 public:
  ThreadSafeIdMap() = default;

  // Returns all key-value pairs in the map.
  //
  // The returned span is invalidated by an Insert() or Erase() followed by a
  // call to UpdateAndGetAll() or UpdateAndGet().
  //
  // Note: the V values returned are mutable. Any concurrent mutations must be
  // synchronized externally.
  absl::Span<const std::pair<int64_t, std::unique_ptr<V>>> UpdateAndGetAll();

  // Returns all key-value paris in the map.
  //
  // In contrast to UpdateAndGetAll(), this function will always acquire a lock
  // and copy the data before returning. Thus, this function is slower, but the
  // values are harder to invalidate. This function is also const, while
  // UpdateAndGetAll() is not. Last, because this function does not update, it
  // will not invalidate any pointers returned by other functions on this class.
  //
  // For each (id, V*) pair, the V* is
  // invalidated by either:
  //   * ~ThreadSafeIdMap()
  //   * `Erase(id)` followed by any call to UpdateXXX().
  //
  // Note: the V values returned are mutable. Any concurrent mutations must be
  // synchronized externally.
  std::vector<std::pair<int64_t, V*>> GetAll() const;

  // Returns the value for this key, or nullptr if this key is not in the map.
  //
  // The returned pointer is invalidated by either of:
  //   * ~ThreadSafeIdMap()
  //   * `Erase(id)` followed by any call to UpdateXXX().
  //
  // Warning: this does NOT run O(1) time, the complexity is linear in the
  // number of elements in the map plus the number of pending insertions and
  // deletions.
  //
  // Note: the V value returned is mutable. Any concurrent mutations must be
  // synchronized externally.
  V* UpdateAndGet(int64_t id);

  // Returns the value for this key, or nullptr if this key is not in the map.
  //
  // The returned pointer is invalidated by any call to `Erase(id)` or
  // ~ThreadSafeIdMap().
  //
  // This function is similar to UpdateAndGet(), but it is const. It can be
  // slightly slower, but it is also safer to use from a concurrent context, as
  // it will not invalidate any pointers returned by other functions on this
  // class.
  //
  // Warning: this does NOT run O(1) time, the complexity is linear in the
  // number of elements in the map plus the number of pending insertions and
  // deletions.
  //
  // Note: the V value returned is mutable. Any concurrent mutations must be
  // synchronized externally.
  V* Get(int64_t id) const;

  // Inserts value into the map and returns the assigned key.
  int64_t Insert(std::unique_ptr<V> value);

  // Erases key from the map, returning true if the key was found in the map.
  bool Erase(int64_t key);

  // The number of elements in the map.
  int64_t Size() const;

 private:
  void ApplyPendingModifications() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void UpdateHasPendingModifications() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable absl::Mutex mutex_;
  std::atomic<bool> has_pending_modifications_ = false;
  int64_t next_id_ ABSL_GUARDED_BY(mutex_) = 0;

  std::vector<std::pair<int64_t, std::unique_ptr<V>>> elements_;
  std::vector<std::pair<int64_t, std::unique_ptr<V>>> pending_inserts_
      ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_set<int64_t> pending_deletes_ ABSL_GUARDED_BY(mutex_);
};

////////////////////////////////////////////////////////////////////////////////
// Template function implementations.
////////////////////////////////////////////////////////////////////////////////

template <typename V>
absl::Span<const std::pair<int64_t, std::unique_ptr<V>>>
ThreadSafeIdMap<V>::UpdateAndGetAll() {
  if (has_pending_modifications_.load(std::memory_order_relaxed)) {
    absl::MutexLock lock(&mutex_);
    ApplyPendingModifications();
  }
  return absl::MakeConstSpan(elements_);
}

template <typename V>
int64_t ThreadSafeIdMap<V>::Size() const {
  absl::MutexLock lock(&mutex_);
  return static_cast<int64_t>(elements_.size() + pending_inserts_.size()) -
         static_cast<int64_t>(pending_deletes_.size());
}

template <typename V>
V* ThreadSafeIdMap<V>::Get(const int64_t id) const {
  absl::MutexLock lock(&mutex_);
  if (pending_deletes_.contains(id)) {
    return nullptr;
  }
  for (const auto& [key, value] : pending_inserts_) {
    if (id == key) {
      return value.get();
    }
  }
  for (const auto& [key, value] : elements_) {
    if (id == key) {
      return value.get();
    }
  }
  return nullptr;
}

template <typename V>
std::vector<std::pair<int64_t, V*>> ThreadSafeIdMap<V>::GetAll() const {
  absl::MutexLock lock(&mutex_);
  std::vector<std::pair<int64_t, V*>> result;
  for (const auto& [id, diff] : elements_) {
    if (!pending_deletes_.contains(id)) {
      result.push_back({id, diff.get()});
    }
  }
  for (const auto& [id, diff] : pending_inserts_) {
    if (!pending_deletes_.contains(id)) {
      result.push_back({id, diff.get()});
    }
  }
  return result;
}

template <typename V>
V* ThreadSafeIdMap<V>::UpdateAndGet(const int64_t id) {
  absl::MutexLock lock(&mutex_);
  if (has_pending_modifications_.load()) {
    ApplyPendingModifications();
  }
  for (const auto& [key, value] : elements_) {
    if (id == key) {
      return value.get();
    }
  }
  return nullptr;
}

template <typename V>
int64_t ThreadSafeIdMap<V>::Insert(std::unique_ptr<V> value) {
  absl::MutexLock lock(&mutex_);
  const int64_t result = next_id_;
  ++next_id_;
  pending_inserts_.push_back(std::make_pair(result, std::move(value)));
  has_pending_modifications_ = true;
  return result;
}

template <typename V>
bool ThreadSafeIdMap<V>::Erase(const int64_t key) {
  absl::MutexLock lock(&mutex_);
  for (auto it = pending_inserts_.begin(); it != pending_inserts_.end(); ++it) {
    if (it->first == key) {
      pending_inserts_.erase(it);
      UpdateHasPendingModifications();
      return true;
    }
  }
  for (const auto& [k, v] : elements_) {
    if (k == key) {
      auto [unused, inserted] = pending_deletes_.insert(k);
      if (inserted) {
        UpdateHasPendingModifications();
      }
      return inserted;
    }
  }
  return false;
}

template <typename V>
void ThreadSafeIdMap<V>::ApplyPendingModifications() {
  if (!pending_deletes_.empty()) {
    gtl::STLEraseAllFromSequenceIf(
        &elements_, [this](const std::pair<int64_t, std::unique_ptr<V>>& entry)
                        ABSL_SHARED_LOCKS_REQUIRED(mutex_) {
                          return pending_deletes_.contains(entry.first);
                        });
    pending_deletes_.clear();
  }
  for (auto& kv : pending_inserts_) {
    elements_.push_back(std::move(kv));
  }
  pending_inserts_.clear();
  has_pending_modifications_ = false;
}

template <typename V>
void ThreadSafeIdMap<V>::UpdateHasPendingModifications() {
  has_pending_modifications_ =
      !pending_inserts_.empty() || !pending_deletes_.empty();
}

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_ELEMENTAL_THREAD_SAFE_ID_MAP_H_
