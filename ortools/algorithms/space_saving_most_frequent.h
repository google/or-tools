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

#ifndef ORTOOLS_ALGORITHMS_SPACE_SAVING_MOST_FREQUENT_H_
#define ORTOOLS_ALGORITHMS_SPACE_SAVING_MOST_FREQUENT_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/log/check.h"

namespace operations_research {

namespace ssmf_internal {

template <typename T>
class BoundedAllocator;

template <typename T>
class DoubleLinkedList;

}  // namespace ssmf_internal

// Space-Saving is an approximate algorithm for finding the most frequent items
// in a data stream. It is conceptually very simple: we maintain a list of at
// most `storage_size` elements and the number of times each of them has been
// seen. When a new element is added and the list is full, we remove the least
// frequent item (the one with the lowest count). If there is a tie, we remove
// the oldest one. See space_saving_most_frequent_test.cc for a trivial
// implementation that yield identical results to this class but is much slower.
//
// The implementation is based on [1], which describes a way of storing the
// items so all the operations are O(1). The elements that have the same count
// (a "bucket") are stored in a doubly-linked list, ordered by the time of
// insertion. The buckets are also stored in a doubly-linked list, ordered by
// number of counts. Thus, to increment the count of an element we need to
// remove it from its bucket and add it to the next one, which is a removal and
// an inclusion in linked lists and thus takes O(1) time.
//
// [1] Graham Cormode, Marios Hadjieleftheriou.  Methods for finding frequent
//     items in data streams.  The VLDB Journal (2010) 19: 3.
//     http://dimacs.rutgers.edu/~graham/pubs/papers/freqvldbj.pdf
//
// This class is thread-compatible.
template <typename T, typename Hash = absl::Hash<T>,
          typename Eq = std::equal_to<T>>
class SpaceSavingMostFrequent {
 public:
  // Create a data structure holding at most `storage_size` elements in memory.
  // That means that frequent elements that are added less frequently than
  // `1/storage_size` will be ignored.
  explicit SpaceSavingMostFrequent(int storage_size);

  ~SpaceSavingMostFrequent();

  // Adds `value` to the data structure.
  // Complexity: O(1).
  void Add(T value);

  // Removes all occurrences of `value` from the data structure. Does nothing if
  // the element is not in the data structure.
  // Complexity: O(1).
  void FullyRemove(const T& value);

  // Returns the `num_samples` most frequent elements in the data structure
  // sorted by decreasing count. Note: this does not work with non-copyable
  // types.
  // TODO(user): Replace this by an iterator with a begin() and end().
  std::vector<std::pair<T, int64_t>> GetMostFrequent(int num_samples) const;

  // Equivalent to calling GetMostFrequent(1) and popping the first element.
  T PopMostFrequent();

  // Equivalent of GetMostFrequent(1).second. Returns zero if the data structure
  // is empty.
  int64_t CountOfMostFrequent() const;

 private:
  struct Bucket;

  // The nodes of the doubly-linked list of elements for a given bucket (ie.,
  // sharing the same count).
  struct Item {
    T value;
    Bucket* absl_nonnull bucket;
    Item* absl_nullable next = nullptr;
    Item* absl_nullable prev = nullptr;
  };
  using ItemList = ssmf_internal::DoubleLinkedList<Item>;

  // A bucket of elements with the same count. They are stored in a
  // doubly-linked list ordered by the time of insertion.
  struct Bucket {
    int64_t count;                         // The count of this bucket.
    ItemList items;                        // front (oldest), back (newest).
    Bucket* absl_nullable next = nullptr;  // Bucket with lower count.
    Bucket* absl_nullable prev = nullptr;  // Bucket with higher count.
  };
  using BucketList = ssmf_internal::DoubleLinkedList<Bucket>;

  void RemoveIfEmpty(Bucket* absl_nonnull bucket) {
    if (bucket->items.empty()) {
      bucket_alloc_.Return(buckets_.erase(bucket));
    }
  }

  void RemoveFromLinkedList(Item* absl_nonnull item) {
    Bucket* absl_nonnull bucket = item->bucket;
    item_alloc_.Return(bucket->items.erase(item));
    RemoveIfEmpty(bucket);
  }

  Bucket* absl_nonnull GetBucketForCountOne() {
    if (!buckets_.empty() && buckets_.back()->count == 1) {
      return buckets_.back();
    }
    // We need to create a new empty bucket, which will be the last one.
    Bucket* absl_nonnull bucket = buckets_.insert_back(bucket_alloc_.New());
    bucket->count = 1;
    return bucket;
  }

  const int storage_size_;
  ssmf_internal::BoundedAllocator<Item> item_alloc_;
  ssmf_internal::BoundedAllocator<Bucket> bucket_alloc_;
  BucketList buckets_;  // front with highest count.

  struct HashItemPtr {
    using is_transparent = void;
    size_t operator()(const Item* absl_nonnull value) const {
      return Hash()(value->value);
    }
    size_t operator()(const T& value) const { return Hash()(value); }
  };

  struct EqItemPtr {
    using is_transparent = void;
    bool operator()(const Item* absl_nonnull a,
                    const Item* absl_nonnull b) const {
      return Eq()(a->value, b->value);
    }
    bool operator()(const Item* absl_nonnull a, const T& b) const {
      return Eq()(a->value, b);
    }
    bool operator()(const T& a, const Item* absl_nonnull b) const {
      return Eq()(a, b->value);
    }
  };
  absl::flat_hash_set<Item* absl_nonnull, HashItemPtr, EqItemPtr> item_ptr_set_;
};

template <typename T, typename Hash, typename Eq>
SpaceSavingMostFrequent<T, Hash, Eq>::SpaceSavingMostFrequent(int storage_size)
    : storage_size_(storage_size),
      item_alloc_(storage_size),
      bucket_alloc_(storage_size + 1) {
  CHECK_GT(storage_size, 0);
  item_ptr_set_.reserve(2 * storage_size);
}

// Properly return all buckets and items to their allocators to ensure proper
// destruction.
template <typename T, typename Hash, typename Eq>
SpaceSavingMostFrequent<T, Hash, Eq>::~SpaceSavingMostFrequent() {
#ifdef NDEBUG
  bucket_alloc_.DisposeAll();
  item_alloc_.DisposeAll();
#else
  while (!buckets_.empty()) {
    auto& items = buckets_.front()->items;
    while (!items.empty()) {
      item_alloc_.Return(items.pop_front());
    }
    bucket_alloc_.Return(buckets_.pop_front());
  }
#endif
}

template <typename T, typename Hash, typename Eq>
void SpaceSavingMostFrequent<T, Hash, Eq>::Add(T value) {
  if (buckets_.empty()) {
    // We are adding an element to an empty data structure.
    DCHECK(item_alloc_.empty());
    DCHECK(item_ptr_set_.empty());
    Bucket* absl_nonnull bucket = buckets_.insert_back(bucket_alloc_.New());
    Item* absl_nonnull const item =
        bucket->items.insert_front(item_alloc_.New());
    item->bucket = bucket;
    item->value = std::move(value);
    bucket->count = 1;
    item_ptr_set_.emplace(item);
    return;
  }

  DCHECK(!buckets_.empty());

  auto it = item_ptr_set_.find(value);
  if (it == item_ptr_set_.end()) {
    // We are adding a new element. First, check if we are full, and if so,
    // remove the least frequent element.
    if (item_alloc_.full()) {
      // Remove an entry from the last bucket where the `count` is lowest.
      Bucket* absl_nonnull const last_bucket = buckets_.back();
      // We want to remove the oldest one, with the idea that it is potentially
      // the real least frequent of the bucket since it was unseen for longer.
      Item* absl_nonnull recycled_item = last_bucket->items.front();
      // Reclaim its storage for the newly added element.
      item_ptr_set_.erase(recycled_item);
      item_alloc_.Return(last_bucket->items.pop_front());
      RemoveIfEmpty(last_bucket);
    }
    Bucket* absl_nonnull bucket = GetBucketForCountOne();
    DCHECK_EQ(bucket->count, 1);
    Item* absl_nonnull item = bucket->items.insert_back(item_alloc_.New());
    item->value = std::move(value);
    item->bucket = bucket;
    item_ptr_set_.emplace_hint(it, item);
  } else {
    Item* absl_nonnull item = *it;
    Bucket* absl_nonnull bucket = item->bucket;
    ItemList& current_bucket_items = bucket->items;
    const int64_t new_count = bucket->count + 1;
    const bool no_bucket_for_new_count =
        (bucket->prev == nullptr) || (bucket->prev->count > new_count);
    if (no_bucket_for_new_count && current_bucket_items.single()) {
      // Small optimization for very common elements: if the element is alone in
      // a bucket and there is no bucket for count + 1, we can just increment
      // the count of the bucket.
      bucket->count = new_count;
      return;
    }
    // Extract item from this bucket.
    auto dangling_item = current_bucket_items.erase(item);
    // Fetch the bucket with the correct count.
    Bucket* new_bucket = nullptr;
    if (bucket->prev && bucket->prev->count == new_count) {
      new_bucket = bucket->prev;
    } else {
      // We create a new empty bucket, which will be before the current bucket.
      new_bucket = buckets_.insert_before(bucket, bucket_alloc_.New());
      new_bucket->count = new_count;
    }
    // Insert the item in the new bucket at the end (newest).
    dangling_item->bucket = new_bucket;
    new_bucket->items.insert_back(std::move(dangling_item));

    // Reclaim old bucket if it is empty.
    RemoveIfEmpty(bucket);
  }
}

template <typename T, typename Hash, typename Eq>
void SpaceSavingMostFrequent<T, Hash, Eq>::FullyRemove(const T& value) {
  auto node = item_ptr_set_.extract(value);
  if (node.empty()) return;
  RemoveFromLinkedList(node.value());
}

template <typename T, typename Hash, typename Eq>
std::vector<std::pair<T, int64_t>>
SpaceSavingMostFrequent<T, Hash, Eq>::GetMostFrequent(int num_samples) const {
  std::vector<std::pair<T, int64_t>> result;
  result.reserve(num_samples);
  if (!buckets_.empty()) {
    for (Bucket* bucket = buckets_.front(); bucket; bucket = bucket->next) {
      const int64_t count = bucket->count;
      DCHECK(!bucket->items.empty());
      for (Item* item = bucket->items.back(); item; item = item->prev) {
        if (result.size() == num_samples) return result;
        result.emplace_back(item->value, count);
      }
    }
  }
  return result;
}

template <typename T, typename Hash, typename Eq>
T SpaceSavingMostFrequent<T, Hash, Eq>::PopMostFrequent() {
  CHECK(!buckets_.empty());
  Item* absl_nonnull item = buckets_.front()->items.back();
  DCHECK(item_ptr_set_.contains(item));
  item_ptr_set_.erase(item);
  T value = std::move(item->value);
  RemoveFromLinkedList(item);
  return value;
}

template <typename T, typename Hash, typename Eq>
int64_t SpaceSavingMostFrequent<T, Hash, Eq>::CountOfMostFrequent() const {
  return buckets_.empty() ? 0 : buckets_.front()->count;
}

namespace ssmf_internal {

// This is semantically equivalent to a `std::unique_ptr` except that it does
// not own the object and that only `BoundedAllocator` and `DoubleLinkedList`
// are able to manage the stored pointer.
// Other clients can access the object with the guarantee that it is non null.
template <typename T>
class Ptr {
  explicit Ptr(T* absl_nullable ptr) : ptr_(ptr) { get_nonnull(); }

  T* absl_nonnull get_nonnull() const {
    DCHECK(ptr_ != nullptr);
    return ptr_;
  };

  T* absl_nonnull release() {
    T* absl_nonnull ptr = get_nonnull();
    ptr_ = nullptr;
    return ptr;
  }

  T* absl_nullable ptr_ = nullptr;

 public:
  Ptr(const Ptr&) = delete;
  Ptr(Ptr&& other) : ptr_(other.release()) {}
  ~Ptr() { DCHECK(ptr_ == nullptr); }
  T* absl_nonnull operator->() const { return get_nonnull(); };
  T& operator*() const { return *get_nonnull(); };
  friend class BoundedAllocator<T>;
  friend class DoubleLinkedList<T>;
};

// Allocator that allows creating up to `max_size` objects.
// Storage is allocated at once contiguously which helps with cache locality.
// Objects that are returned to the allocator are stored in a freelist for later
// use and are not destroyed right away. Objects that are extracted from the
// freelist are default initialized for correctness.
// The allocator makes sure that all created objects are returned to the pool
// upon destruction, this allows to catch logic errors. It is possible to bypass
// this behavior when it is safe to destroy all object at once by calling the
// `DisposeAll` method. Once this method is called the allocator cannot be used
// anymore.
template <typename T>
class BoundedAllocator {
 public:
  explicit BoundedAllocator(size_t max_size) : data_(max_size) {
    freelist_.reserve(max_size);
    for (auto& data : data_) {
      freelist_.push_back(&data);
    }
  }

  ~BoundedAllocator() {
    CHECK(empty()) << "some elements are not returned and won't be destroyed.";
  }

  bool full() const { return freelist_.empty(); }

  bool empty() const { return data_.size() == freelist_.size(); }

  Ptr<T> New() {
    CHECK(!freelist_.empty());
    T* absl_nonnull t = freelist_.back();
    freelist_.pop_back();
    return Ptr<T>(t);
  }

  void Return(Ptr<T> ptr) {
    T* absl_nonnull t = ptr.release();
    DCHECK(t != nullptr);
    DCHECK_GE(t, &data_.front());
    DCHECK_LE(t, &data_.back());
    *t = T();
    freelist_.push_back(t);
  }

  // Destroys all allocated objects.
  // Once called, it is no more possible to allocate new objects.
  void DisposeAll() {
    freelist_.clear();
    data_.clear();
  }

 private:
  std::vector<T> data_;
  std::vector<T* absl_nonnull> freelist_;
};

// A simple doubly linked list with ownership transfer.
// All elements added or extracted from the list are done though the `Ptr`
// abstraction. This guarantees that there is always only one owner.
template <typename T>
class DoubleLinkedList {
 public:
  DoubleLinkedList() = default;
  DoubleLinkedList(const DoubleLinkedList&) = delete;
  ~DoubleLinkedList() {
    DCHECK_EQ(front_, nullptr);
    DCHECK_EQ(front_, nullptr);
  }

  bool empty() const {
    DCHECK_EQ(front_ == nullptr, back_ == nullptr);
    return front_ == nullptr;
  }

  bool single() const {
    DCHECK_EQ(front_ == nullptr, back_ == nullptr);
    return front_ != nullptr && front_ == back_;
  }

  T* absl_nonnull front() const {
    DCHECK_NE(front_, nullptr);
    return front_;
  }

  T* absl_nonnull back() const {
    DCHECK_NE(back_, nullptr);
    return back_;
  }

  T* absl_nonnull insert_after(T* absl_nonnull node, Ptr<T> new_node) {
    T* absl_nonnull new_node_ptr = new_node.release();
    new_node_ptr->prev = node;
    if (node->next == nullptr) {
      DCHECK_EQ(new_node_ptr->next, nullptr);
      back_ = new_node_ptr;
    } else {
      new_node_ptr->next = node->next;
      node->next->prev = new_node_ptr;
    }
    node->next = new_node_ptr;
    return new_node_ptr;
  }

  T* absl_nonnull insert_before(T* absl_nonnull node, Ptr<T> new_node) {
    T* absl_nonnull new_node_ptr = new_node.release();
    new_node_ptr->next = node;
    if (node->prev == nullptr) {
      DCHECK_EQ(new_node_ptr->prev, nullptr);
      front_ = new_node_ptr;
    } else {
      new_node_ptr->prev = node->prev;
      node->prev->next = new_node_ptr;
    }
    node->prev = new_node_ptr;
    return new_node_ptr;
  }

  T* absl_nonnull insert_front(Ptr<T> new_node) {
    if (front_ != nullptr) {
      return insert_before(front_, std::move(new_node));
    }
    T* absl_nonnull new_node_ptr = new_node.release();
    front_ = new_node_ptr;
    back_ = new_node_ptr;
    new_node_ptr->prev = nullptr;
    new_node_ptr->next = nullptr;
    return new_node_ptr;
  }

  T* absl_nonnull insert_back(Ptr<T> new_node) {
    if (back_ != nullptr) {
      return insert_after(back_, std::move(new_node));
    } else {
      return insert_front(std::move(new_node));
    }
  }

  ABSL_MUST_USE_RESULT Ptr<T> erase(T* absl_nonnull node) {
    if (node->prev) {
      node->prev->next = node->next;
    } else {
      front_ = node->next;
    }
    if (node->next) {
      node->next->prev = node->prev;
    } else {
      back_ = node->prev;
    }
    node->next = nullptr;
    node->prev = nullptr;
    return Ptr<T>(node);
  }

  ABSL_MUST_USE_RESULT Ptr<T> pop_front() { return erase(front()); }

  ABSL_MUST_USE_RESULT Ptr<T> pop_back() { return erase(back()); }

 private:
  T* absl_nullable front_ = nullptr;
  T* absl_nullable back_ = nullptr;
};
}  // namespace ssmf_internal

}  // namespace operations_research

#endif  // ORTOOLS_ALGORITHMS_SPACE_SAVING_MOST_FREQUENT_H_
