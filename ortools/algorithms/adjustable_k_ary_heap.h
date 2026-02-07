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

#ifndef ORTOOLS_ALGORITHMS_ADJUSTABLE_K_ARY_HEAP_H_
#define ORTOOLS_ALGORITHMS_ADJUSTABLE_K_ARY_HEAP_H_

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include "absl/log/check.h"

// Adjustable k-ary heap for std::pair<Priority, Index> classes containing a
// priority and an index referring to an array where the relevant data is
// stored.
//
// The comparator is the default comparator for pairs, i.e. the index is used as
// a tie-breaker for the priority, thus making the code more repeatable.
//
// Because the class uses indices and vectors, it is much faster than
// AdjustablePriorityQueue, even in the binary heap case.
//
// k-ary heaps are useful when SiftDown() (aka Decrease) is called more often
// than Pop() (aka Extract).
//
// Namely, Pop() has a complexity in O(k * log_k (n)), while SiftDown() is in
// O(log_k(n)), even when k = 2. This explains the small gain.
//
// In the implementation below, k is denoted as Arity.

template <typename Priority, typename Index, int Arity, bool IsMaxHeap>
class AdjustableKAryHeap {
 public:
  using Aggregate = std::pair<Priority, Index>;
  using HeapIndex = Index;
  static_assert(Arity >= 2, "arity must be at least 2");
  static_assert(std::numeric_limits<Index>::is_integer,
                "Index must be an integer");
  static_assert(std::numeric_limits<Priority>::is_specialized,
                "Priority must be an integer or floating-point type");
  AdjustableKAryHeap() { Clear(); }

  // Construct a k-heap from an existing vector, tracking original indices.
  // `universe_size` is the maximum possible index in `elements`.
  explicit AdjustableKAryHeap(const std::vector<Aggregate>& elements,
                              HeapIndex universe_size) {
    Load(elements, universe_size);
  }

  explicit AdjustableKAryHeap(const std::vector<Index>& indices,
                              const std::vector<Priority>& priorities,
                              HeapIndex universe_size) {
    Load(indices, priorities, universe_size);
  }

  void Clear() {
    data_.clear();
    heap_positions_.clear();
    heap_size_ = 0;
  }

  void Load(const std::vector<Aggregate>& elements, HeapIndex universe_size) {
    data_.resize(elements.size());
    heap_size_ = elements.size();
    std::copy(elements.begin(), elements.end(), data_.begin());
    heap_positions_.resize(universe_size, kNonExistent);
    for (HeapIndex i = 0; i < data_.size(); ++i) {
      heap_positions_[index(i)] = i;
    }
    BuildHeap();
  }

  void Load(const std::vector<Index>& indices,
            const std::vector<Priority>& priorities, HeapIndex universe_size) {
    std::copy(indices.begin(), indices.end(), indices_.begin());
    std::copy(priorities.begin(), priorities.end(), priorities_.begin());
    heap_size_ = indices.size();
    heap_positions_.resize(universe_size, kNonExistent);
    for (HeapIndex i = 0; i < data_.size(); ++i) {
      heap_positions_[indices_[i]] = i;
    }
    BuildHeap();
  }

  // Removes the top element from the heap (smallest for min-heap, largest for
  // max-heap), and rearranges the heap.
  // This will CHECK-fail if the heap is empty (through Top()).
  void Pop() {
    CHECK(!IsEmpty());
    CHECK(RemoveAtHeapPosition(0));
  }

  // Returns the index of the top element, without modifying the heap.
  // Note that this does not remove the element from the heap, Pop() must be
  // called explicitly.
  Index TopIndex() const {
    CHECK(!IsEmpty());
    return data_[0].second;
  }

  // Returns the index of the top element, without modifying the heap.
  // Note that this does not remove the element from the heap, Pop() must be
  // called explicitly.

  Priority TopPriority() const {
    CHECK(!IsEmpty());
    return data_[0].first;
  }

  // Returns the number of elements in the heap.
  HeapIndex heap_size() const { return heap_size_; }

  // True iff the heap is empty.
  bool IsEmpty() const { return heap_size() == 0; }

  // Insert an element into the heap.
  void Insert(Aggregate element) {
    const Index index = element.second;
    if (index >= heap_positions_.size()) {
      heap_positions_.resize(index + 1, kNonExistent);
    }
    if (GetHeapPosition(index) == kNonExistent) {
      heap_positions_[index] = heap_size_;
      if (heap_size_ < data_.size()) {
        data_[heap_size_] = element;
      } else {
        data_.push_back(element);
      }
      ++heap_size_;
    }
    Update(element);
  }

  // Removes the element at index. Returns false if the element does not appear
  // in the heap.
  bool Remove(Index index) {
    if (IsEmpty()) return false;
    const HeapIndex heap_position = GetHeapPosition(index);
    return heap_position != kNonExistent ? RemoveAtHeapPosition(heap_position)
                                         : false;
  }

  // Change the value of an element.
  void Update(Aggregate element) {
    DCHECK(!IsEmpty());
    const HeapIndex heap_position = GetHeapPosition(element.second);
    DCHECK_GE(heap_position, 0);
    DCHECK_LT(heap_position, heap_positions_.size());
    data_[heap_position] = element;
    if (HasPriority(heap_position, Parent(heap_position))) {
      SiftUp(heap_position);
    } else {
      SiftDown(heap_position);
    }
  }

  // Checks if the element with index is in the heap.
  bool Contains(Index index) const {
    return GetHeapPosition(index) != kNonExistent;
  }

  // Checks that the heap is well-formed.
  bool CheckHeapProperty() const {
    for (HeapIndex i = heap_size() - 1; i >= Arity; --i) {
      CHECK(HasPriority(Parent(i), i))
          << "Parent " << Parent(i) << " with priority " << priority(Parent(i))
          << " does not have priority over " << i << " with priority "
          << priority(i) << " , heap_size = " << heap_size()
          << ", priority difference = " << priority(i) - priority(Parent(i));
    }
    CHECK_LE(heap_size(), heap_positions_.size());
    CHECK_LE(heap_size(), data_.size());
    return true;
  }

 private:
  // Gets the current position of element with index i in the heap.
  HeapIndex GetHeapPosition(Index i) const {
    DCHECK_GE(i, 0);
    DCHECK_LT(i, heap_positions_.size());
    return heap_positions_[i];
  }

  // Removes an element at a given heap position.
  bool RemoveAtHeapPosition(HeapIndex heap_index) {
    DCHECK(!IsEmpty());
    DCHECK_GE(heap_index, 0);
    if (heap_index >= heap_size()) return false;
    PerformSwap(heap_index, heap_size() - 1);
    --heap_size_;
    if (HasPriority(heap_index, Parent(heap_index))) {
      SiftUp(heap_index);
    } else {
      SiftDown(heap_index);
    }
    heap_positions_[index(heap_size_)] = kNonExistent;
    return true;
  }

  // Maintains heap property by sifting down starting from the end,
  void BuildHeap() {
    for (HeapIndex i = Parent(heap_size()); i >= 0; --i) {
      SiftDown(i);
    }
    DCHECK(CheckHeapProperty());
  }

  // Maintains heap property by sifting up an element.
  void SiftUp(HeapIndex index) {
    while (index > 0 && HasPriority(index, Parent(index))) {
      PerformSwap(index, Parent(index));
      index = Parent(index);
    }
  }

  // Maintains heap property by sifting down an element.
  void SiftDown(HeapIndex index) {
    while (true) {
      const HeapIndex highest_priority_child = GetHighestPriorityChild(index);
      if (highest_priority_child == index) return;
      PerformSwap(index, highest_priority_child);
      index = highest_priority_child;
    }
  }

  // Finds the child with the highest priority, i.e. the child with the
  // smallest (resp. largest) key for a min- (resp. max-) heap.
  // Returns index if there are no such children.
  HeapIndex GetHighestPriorityChild(HeapIndex index) const {
    const HeapIndex right_bound = std::min(RightChild(index) + 1, heap_size());
    HeapIndex highest_priority_child = index;
    for (HeapIndex i = LeftChild(index); i < right_bound; ++i) {
      if (HasPriority(i, highest_priority_child)) {
        highest_priority_child = i;
      }
    }
    return highest_priority_child;
  }

  // Swaps two elements of data_, while also making sure heap_positions_ is
  // properly maintained.
  void PerformSwap(HeapIndex i, HeapIndex j) {
    std::swap(data_[i], data_[j]);
    std::swap(heap_positions_[index(i)], heap_positions_[index(j)]);
  }

  // Compares two elements based on whether we are dealing with a min- or a
  // max-heap. Returns true if (data indexed by) i has more priority
  // than j. Note that we only use operator::<.
  bool HasPriority(HeapIndex i, HeapIndex j) const {
    return IsMaxHeap ? data_[j] < data_[i] : data_[i] < data_[j];
  }

  // Since Arity is a (small) constant, we expect compilers to avoid
  // multiplication instructions and use LEA instructions or a combination
  // of shifts and arithmetic operations.
  // Powers of 2 are guaranteed to be quick thanks to simple shifts.

  // Gets the leftmost child index of a given node
  HeapIndex LeftChild(HeapIndex index) const { return Arity * index + 1; }

  // Gets the rightmost child index of a given node
  HeapIndex RightChild(HeapIndex index) const { return Arity * (index + 1); }

  // For division, the optimization is more uncertain, although a simple
  // multiplication and a shift might be used by the compiler.
  // Of course, powers of 2 are guaranteed to be quick thanks to simple shifts.

  // Gets the parent index of a given index.
  HeapIndex Parent(HeapIndex index) const { return (index - 1) / Arity; }

  // Returns the index of the element at position i in the heap.
  Index index(HeapIndex i) const { return data_[i].second; }

  // Returns the index of the element at position i in the heap.
  Priority priority(HeapIndex i) const { return data_[i].first; }

  // The heap is stored as a vector.
  std::vector<Aggregate> data_;

  // The heap is stored as two vectors.
  // indices_ is such that heap_positions_[indices_[i]] == i
  // and indices_[heap_positions_[i]] == i, at all times unless
  // indices_[i] is not in the heap, and therefore
  // heap_positions_[indices[i]] == -1.
  std::vector<Index> indices_;
  std::vector<Priority> priorities_;

  // Maps original index to current heap position.
  std::vector<Index> heap_positions_;

  // The number of elements currently in the heap. This may be updated
  // either when removing an element (which is not removed from data_), or
  // adding a new one.
  HeapIndex heap_size_ = 0;

  // The index for Aggregates not in the heap.
  const Index kNonExistent = -1;
};

#endif  // ORTOOLS_ALGORITHMS_ADJUSTABLE_K_ARY_HEAP_H_
