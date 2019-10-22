// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_BASE_ADJUSTABLE_PRIORITY_QUEUE_INL_H_
#define OR_TOOLS_BASE_ADJUSTABLE_PRIORITY_QUEUE_INL_H_

#include "ortools/base/adjustable_priority_queue.h"

namespace util {
namespace pq {

// Adjusts a heap so as to move a hole at position i closer to the root,
// until a suitable position is found for element t.  Then, copies t into that
// position.  This function is a slight generalization of std::push_heap() in
// that it takes a pointer to an IndexMaintainer functor.  This functor is
// called each time immediately after modifying a value in the underlying
// container, with the offset of the modified element as its argument.
template <typename RandomAccessIterator, typename Distance, typename T,
          typename Compare, typename IndexMaintainer>
inline void AdjustUpwards(const RandomAccessIterator& first, Distance i, T&& t,
                          const Compare& comp, IndexMaintainer* maintainer) {
  while (i > Distance(0)) {
    const Distance parent = (i - 1) / 2;
    if (!comp(*(first + parent), t)) break;
    *(first + i) = std::move(*(first + parent));
    (*maintainer)(i);
    i = parent;
  }
  *(first + i) = std::forward<T>(t);
  (*maintainer)(i);
}

// Adjusts a heap so as to move a hole at position i farther away from the root,
// until a suitable position is found for element t.  Then, copies t into that
// position.  This function is a generalization of std::pop_heap() in two
// regards:  One, std::pop_heap() can only treat the first (i.e., maximum)
// element of a heap as a hole, which is an artificial restriction.  And two,
// this function takes a pointer to an IndexMaintainer functor, which is
// called each time immediately after modifying a value in the underlying
// conatiner, with the offset of the modified element as its argument.
template <typename RandomAccessIterator, typename Distance, typename T,
          typename Compare, typename IndexMaintainer>
inline void AdjustDownwards(const RandomAccessIterator& first, Distance i,
                            const Distance& length, T&& t, const Compare& comp,
                            IndexMaintainer* maintainer) {
  while (true) {
    const Distance left_child = 1 + 2 * i;
    if (left_child >= length) break;
    const Distance right_child = left_child + 1;
    const Distance& next_i = right_child < length &&
        comp(*(first + left_child), *(first + right_child)) ?
        right_child : left_child;
    if (!comp(t, *(first + next_i)))
      break;
    *(first + i) = std::move(*(first + next_i));
    (*maintainer)(i);
    i = next_i;
  }
  *(first + i) = std::forward<T>(t);
  (*maintainer)(i);
}

}  // namespace pq
}  // namespace util

template <typename T, typename Comparator>
class LowerPriorityThan {
 public:
  explicit LowerPriorityThan(Comparator* compare) : compare_(compare) {}
  bool operator()(T* a, T* b) const { return (*compare_)(*a, *b); }
 private:
  Comparator* compare_;
};

template <class T, class C, class I>
inline T* AdjustablePriorityQueue<T, C, I>::Top() {
  return elems_[0];
}

template <class T, class C, class I>
inline const T* AdjustablePriorityQueue<T, C, I>::Top() const {
  return elems_[0];
}

template <class T, class C, class I>
void AdjustablePriorityQueue<T, C, I>::AllTop(std::vector<T*>* topvec) {
  topvec->clear();
  if (Size() == 0) return;
  std::list<int> need_to_check_children;
  need_to_check_children.push_back(0);
  // Implements breadth-first search down tree, stopping whenever
  // there's an element < top
  while (!need_to_check_children.empty()) {
    int ind = need_to_check_children.front();
    need_to_check_children.pop_front();
    topvec->push_back(elems_[ind]);
    int leftchild = 1 + 2 * ind;
    if (leftchild < Size()) {
      if (!LowerPriorityThan<T, C>(&c_)(elems_[leftchild], elems_[ind])) {
        need_to_check_children.push_back(leftchild);
      }
      int rightchild = leftchild + 1;
      if (rightchild < Size() &&
          !LowerPriorityThan<T, C>(&c_)(elems_[rightchild], elems_[ind])) {
        need_to_check_children.push_back(rightchild);
      }
    }
  }
}

template <class T, class C, class I>
class AQPSetHeapIndex {
 public:
  explicit AQPSetHeapIndex(AdjustablePriorityQueue<T, C, I>* queue)
      : queue_(queue) {}
  void operator()(int offset) const {
    return queue_->imanip_.SetHeapIndex(queue_->elems_[offset], offset);
  }

 private:
  AdjustablePriorityQueue<T, C, I>* queue_;
};

template <class T, class C, class I>
void AdjustablePriorityQueue<T, C, I>::Add(T* val) {
  AQPSetHeapIndex<T, C, I> imanip(this);
  // Extend the size of the vector by one.  We could just use
  // vector<T>::resize(), but maybe T is not default-constructible.
  elems_.push_back(val);
  util::pq::AdjustUpwards(elems_.begin(), Size() - 1, val,
                          LowerPriorityThan<T, C>(&c_), &imanip);
}

template <class T, class C, class I>
void AdjustablePriorityQueue<T, C, I>::NoteChangedPriority(T* val) {
  AQPSetHeapIndex<T, C, I> imanip(this);
  LowerPriorityThan<T, C> lower_priority(&c_);
  int i = imanip_.GetHeapIndex(*val);
  int parent = (i - 1) / 2;
  if (lower_priority(elems_[parent], val)) {
    util::pq::AdjustUpwards(elems_.begin(), i, val, lower_priority, &imanip);
  } else {
    util::pq::AdjustDownwards(elems_.begin(), i, Size(), val, lower_priority,
                              &imanip);
  }
}

template <class T, class C, class I>
void AdjustablePriorityQueue<T, C, I>::Remove(T* val) {
  int end = elems_.size() - 1;
  int i = imanip_.GetHeapIndex(*val);
  if (i == end) {
    elems_.pop_back();
    return;
  }
  elems_[i] = elems_[end];
  imanip_.SetHeapIndex(elems_[i], i);
  elems_.pop_back();
  NoteChangedPriority(elems_[i]);
}

template <class T, class C, class I>
bool AdjustablePriorityQueue<T, C, I>::Contains(const T* val) const {
  int i = imanip_.GetHeapIndex(*val);
  return (i >= 0 && i < elems_.size() && elems_[i] == val);
}

template <class T, class C, class I>
void AdjustablePriorityQueue<T, C, I>::Clear() {
  elems_.clear();
}

template <class T, class C, class I>
void AdjustablePriorityQueue<T, C, I>::CheckValid() {
  for (int i = 0; i < elems_.size(); ++i) {
    int left_child = 1 + 2*i;
    if (left_child < elems_.size()) {
      CHECK(!(LowerPriorityThan<T, C>(&c_))(elems_[i], elems_[left_child]));
    }
    int right_child = left_child + 1;
    if (right_child < elems_.size()) {
      CHECK(!(LowerPriorityThan<T, C>(&c_))(elems_[i], elems_[right_child]));
    }
  }
}

#endif  // OR_TOOLS_BASE_ADJUSTABLE_PRIORITY_QUEUE_INL_H_
