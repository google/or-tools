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

#ifndef OR_TOOLS_ALGORITHMS_SET_COVER_UTILS_H_
#define OR_TOOLS_ALGORITHMS_SET_COVER_UTILS_H_

#include <limits>
#include <vector>
#if defined(_MSC_VER)
#include <basetsd.h>
#define ssize_t SSIZE_T
#endif

#include "ortools/algorithms/set_cover_ledger.h"
#include "ortools/algorithms/set_cover_model.h"
#include "ortools/base/adjustable_priority_queue.h"

namespace operations_research {

// Element used for AdjustablePriorityQueue. It's an implementation detail.
class SubsetPriority {
 public:
  SubsetPriority()
      : heap_index_(-1),
        subset_(0),
        priority_(std::numeric_limits<Cost>::infinity()) {}

  SubsetPriority(SubsetIndex s, Cost cost)
      : heap_index_(s.value()), subset_(s), priority_(cost) {}

  void SetHeapIndex(int h) { heap_index_ = h; }
  int GetHeapIndex() const { return heap_index_; }

  // The priority queue maintains the max element. This comparator breaks ties
  // between subsets using their cardinalities.
  bool operator<(const SubsetPriority& other) const {
    return priority_ < other.priority_ ||
           (priority_ == other.priority_ && subset_ < other.subset_);
  }

  SubsetIndex GetSubset() const { return subset_; }
  void SetPriority(Cost priority) { priority_ = priority; }
  Cost GetPriority() const { return priority_; }

 private:
  int heap_index_;
  SubsetIndex subset_;
  Cost priority_;
};

using SubsetPriorityVector = glop::StrictITIVector<SubsetIndex, SubsetPriority>;

// Also an implementation detail.
class SubsetPriorityQueue {
 public:
  explicit SubsetPriorityQueue(SetCoverLedger* ledger) : ledger_(ledger) {
    Initialize();
  }

  // Adds subset to the priority queue.
  void Add(SubsetIndex subset, Cost priority);

  // Changes the priority of subset in the queue.
  void ChangePriority(SubsetIndex subset, Cost priority);

  // Removes subset from the queue, if it is in the queue.
  void Remove(SubsetIndex subset);

  // Returns true if the subset is in the queue.
  bool Contains(SubsetIndex subset) {
    return max_pq_.Contains(&pq_elements_[subset]);
  }

  // Returns true if the queue is empty.
  bool IsEmpty() const { return max_pq_.IsEmpty(); }

  // Returns the top subset in the queue.
  SubsetIndex TopSubset() const;

  // Returns the priority of the subset in the queue.
  Cost Priority(SubsetIndex subset) {
    return pq_elements_[subset].GetPriority();
  }

  // Returns the size of the queue.
  ssize_t Size() const { return max_pq_.Size(); }

 private:
  // Initializes the priority queue.
  void Initialize();

  // The ledger on which the priority queue applies.
  SetCoverLedger* ledger_;

  // The adjustable priority queue per se.
  AdjustablePriorityQueue<SubsetPriority> max_pq_;

  // The elements of the priority queue.
  SubsetPriorityVector pq_elements_;
};

// A Tabu list is a fixed-sized circular array of small size, usually a few
// dozens of elements.
template <typename T>
class TabuList {
 public:
  explicit TabuList(T size) : array_(0), fill_(0), index_(0) {
    array_.resize(size.value(), T(-1));
  }

  // Returns the size of the array.
  int size() const { return array_.size(); }

  // Initializes the array of the Tabu list.
  void Init(int size) {
    array_.resize(size, T(-1));
    fill_ = 0;
    index_ = 0;
  }

  // Adds t to the array. When the end of the array is reached, re-start at 0.
  void Add(T t) {
    const int size = array_.size();
    array_[index_] = t;
    ++index_;
    if (index_ >= size) {
      index_ = 0;
    }
    if (fill_ < size) {
      ++fill_;
    }
  }

  // Returns true if t is in the array. This is O(size), but small.
  bool Contains(T t) const {
    for (int i = 0; i < fill_; ++i) {
      if (t == array_[i]) {
        return true;
      }
    }
    return false;
  }

 private:
  std::vector<T> array_;
  int fill_;
  int index_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_ALGORITHMS_SET_COVER_UTILS_H_
