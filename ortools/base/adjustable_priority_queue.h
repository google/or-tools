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

#include <functional>
#include <list>
#include <vector>

#include "ortools/base/basictypes.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"

template <class T> class APQDefaultHeapIndexManip {
 public:
  void SetHeapIndex(T* t, int h) const { t->SetHeapIndex(h); }
  int GetHeapIndex(const T& t) const { return t.GetHeapIndex(); }
};

template <class T, class Comp, class HeapIndexManip>
class AQPSetHeapIndex;

template<typename T, typename Comp = std::less<T>,
         typename HeapIndexManip = APQDefaultHeapIndexManip<T> >
class AdjustablePriorityQueue {
  friend class AQPSetHeapIndex<T, Comp, HeapIndexManip>;
 public:
  // The objects references 'c' and 'm' are not required to be alive for the
  // lifetime of this object.
  AdjustablePriorityQueue() {}
  AdjustablePriorityQueue(const Comp& c) : c_(c) {}
  AdjustablePriorityQueue(const Comp& c, const HeapIndexManip& m)
      : c_(c), imanip_(m) {}
  AdjustablePriorityQueue(const AdjustablePriorityQueue&) = delete;
  AdjustablePriorityQueue& operator=(const AdjustablePriorityQueue&) = delete;
  AdjustablePriorityQueue(AdjustablePriorityQueue&&) = default;
  AdjustablePriorityQueue& operator=(AdjustablePriorityQueue&&) = default;

  void Add(T* val);

  void Remove(T* val);

  bool Contains(const T* val) const;

  void NoteChangedPriority(T* val);
  // If val ever changes its priority, you need to call this function
  // to notify the pq so it can move it in the heap accordingly.

  T* Top();
  const T* Top() const;

  void AllTop(std::vector<T*>* topvec);
  // If there are ties for the top, this returns all of them.

  void Pop() { Remove(Top()); }

  int Size() const { return elems_.size(); }

  // Returns the number of elements for which storage has been allocated.
  int Capacity() const { return elems_.capacity(); }

  // Allocates storage for a given number of elements.
  void SetCapacity(size_t c) { elems_.reserve(c); }

  bool IsEmpty() const { return elems_.empty(); }

  void Clear();

  // CHECKs that the heap is actually a heap (each "parent" of >=
  // priority than its child).
  void CheckValid();

  // This is for debugging, e.g. the caller can use it to
  // examine the heap for rationality w.r.t. other parts of the
  // program.
  const std::vector<T*>* Raw() const { return &elems_; }

 private:
  Comp c_;
  HeapIndexManip imanip_;
  std::vector<T*> elems_;
};

#endif  // OR_TOOLS_BASE_ADJUSTABLE_PRIORITY_QUEUE_H_
