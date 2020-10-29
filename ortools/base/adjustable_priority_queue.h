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

#ifndef OR_TOOLS_BASE_ADJUSTABLE_PRIORITY_QUEUE_H_
#define OR_TOOLS_BASE_ADJUSTABLE_PRIORITY_QUEUE_H_

#include <stddef.h>

#include <functional>
#include <list>
#include <vector>

#include "ortools/base/basictypes.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"

template <typename T, typename Comparator>
class LowerPriorityThan {
 public:
  explicit LowerPriorityThan(Comparator* compare) : compare_(compare) {}
  bool operator()(T* a, T* b) const { return (*compare_)(*a, *b); }

 private:
  Comparator* compare_;
};

template <typename T, typename Comp = std::less<T> >
class AdjustablePriorityQueue {
 public:
  // The objects references 'c' and 'm' are not required to be alive for the
  // lifetime of this object.
  AdjustablePriorityQueue() {}
  AdjustablePriorityQueue(const Comp& c) : c_(c) {}
  AdjustablePriorityQueue(const AdjustablePriorityQueue&) = delete;
  AdjustablePriorityQueue& operator=(const AdjustablePriorityQueue&) = delete;
  AdjustablePriorityQueue(AdjustablePriorityQueue&&) = default;
  AdjustablePriorityQueue& operator=(AdjustablePriorityQueue&&) = default;

  void Add(T* val) {
    // Extend the size of the vector by one.  We could just use
    // vector<T>::resize(), but maybe T is not default-constructible.
    elems_.push_back(val);
    AdjustUpwards(elems_.size() - 1);
  }

  void Remove(T* val) {
    int end = elems_.size() - 1;
    int i = val->GetHeapIndex();
    if (i == end) {
      elems_.pop_back();
      return;
    }
    elems_[i] = elems_[end];
    elems_[i]->SetHeapIndex(i);
    elems_.pop_back();
    NoteChangedPriority(elems_[i]);
  }

  bool Contains(const T* val) const {
    int i = val->GetHeapIndex();
    return (i >= 0 && i < elems_.size() && elems_[i] == val);
  }

  void NoteChangedPriority(T* val) {
    LowerPriorityThan<T, Comp> lower_priority(&c_);
    int i = val->GetHeapIndex();
    int parent = (i - 1) / 2;
    if (lower_priority(elems_[parent], val)) {
      AdjustUpwards(i);
    } else {
      AdjustDownwards(i);
    }
  }
  // If val ever changes its priority, you need to call this function
  // to notify the pq so it can move it in the heap accordingly.

  T* Top() { return elems_[0]; }

  const T* Top() const { return elems_[0]; }

  void AllTop(std::vector<T*>* topvec) {
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
        if (!LowerPriorityThan<T, Comp>(&c_)(elems_[leftchild], elems_[ind])) {
          need_to_check_children.push_back(leftchild);
        }
        int rightchild = leftchild + 1;
        if (rightchild < Size() &&
            !LowerPriorityThan<T, Comp>(&c_)(elems_[rightchild], elems_[ind])) {
          need_to_check_children.push_back(rightchild);
        }
      }
    }
  }
  // If there are ties for the top, this returns all of them.

  void Pop() { Remove(Top()); }

  int Size() const { return elems_.size(); }

  // Returns the number of elements for which storage has been allocated.
  int Capacity() const { return elems_.capacity(); }

  // Allocates storage for a given number of elements.
  void SetCapacity(size_t c) { elems_.reserve(c); }

  bool IsEmpty() const { return elems_.empty(); }

  void Clear() { elems_.clear(); }

  // CHECKs that the heap is actually a heap (each "parent" of >=
  // priority than its child).
  void CheckValid() {
    for (int i = 0; i < elems_.size(); ++i) {
      int left_child = 1 + 2 * i;
      if (left_child < elems_.size()) {
        CHECK(
            !(LowerPriorityThan<T, Comp>(&c_))(elems_[i], elems_[left_child]));
      }
      int right_child = left_child + 1;
      if (right_child < elems_.size()) {
        CHECK(
            !(LowerPriorityThan<T, Comp>(&c_))(elems_[i], elems_[right_child]));
      }
    }
  }

  // This is for debugging, e.g. the caller can use it to
  // examine the heap for rationality w.r.t. other parts of the
  // program.
  const std::vector<T*>* Raw() const { return &elems_; }

 private:
  void AdjustUpwards(int i) {
    T* const t = elems_[i];
    while (i > 0) {
      const int parent = (i - 1) / 2;
      if (!c_(*elems_[parent], *t)) {
        break;
      }
      elems_[i] = elems_[parent];
      elems_[i]->SetHeapIndex(i);
      i = parent;
    }
    elems_[i] = t;
    t->SetHeapIndex(i);
  }

  void AdjustDownwards(int i) {
    T* const t = elems_[i];
    while (true) {
      const int left_child = 1 + 2 * i;
      if (left_child >= elems_.size()) {
        break;
      }
      const int right_child = left_child + 1;
      const int next_i = (right_child < elems_.size() &&
                          c_(*elems_[left_child], *elems_[right_child]))
                             ? right_child
                             : left_child;
      if (!c_(*t, *elems_[next_i])) {
        break;
      }
      elems_[i] = elems_[next_i];
      elems_[i]->SetHeapIndex(i);
      i = next_i;
    }
    elems_[i] = t;
    t->SetHeapIndex(i);
  }

  Comp c_;
  std::vector<T*> elems_;
};

#endif  // OR_TOOLS_BASE_ADJUSTABLE_PRIORITY_QUEUE_H_
