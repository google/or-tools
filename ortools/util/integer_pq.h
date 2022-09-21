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

// This file contains and adjustable priority queue templated by an Element
// class that must:
//  - Be efficiently copiable and storable in a std::vector<Element>.
//  - Be comparable via the templated Compare class. Top() will returns
//    the element with the largest priority (like std::priority_queue).
//  - Implements the "int Index() const" function that must returns an integer
//    that uniquely identify this particular element. Ideally this index must
//    be dense in [0, max_num_elements).
//
#ifndef OR_TOOLS_UTIL_INTEGER_PQ_H_
#define OR_TOOLS_UTIL_INTEGER_PQ_H_

#include <vector>

#include "ortools/base/logging.h"
#include "ortools/base/macros.h"

namespace operations_research {

// Classic asjustable priority queue implementation. It behaves exactly the same
// as AdjustablePriorityQueue regarding identical elements, but it uses less
// memory and is in general slightly faster.
template <typename Element, class Compare = std::less<Element>>
class IntegerPriorityQueue {
 public:
  // Starts with an empty queue and reserve space for n elements.
  explicit IntegerPriorityQueue(int n = 0, Compare comp = Compare())
      : size_(0), less_(comp) {
    Reserve(n);
  }

  // Increases the space reservation to n elements indices in [0, n). All
  // elements passed to the other functions must have an Index() smaller than
  // this n.
  void Reserve(int n) {
    // The heap_ starts at 1 for faster left/right child indices computation.
    // This also allow us to use position 0 for element not in the queue.
    heap_.resize(n + 1);
    position_.resize(n, 0);
  }

  // Returns the number of elements currently present.
  int Size() const { return size_; }
  bool IsEmpty() const { return size_ == 0; }

  // Removes all elements from the queue.
  // TODO(user): we could make this sparse if it is needed.
  void Clear() {
    size_ = 0;
    position_.assign(position_.size(), 0);
  }

  // Returns true if the element with given index is currently in the queue.
  bool Contains(int index) const { return position_[index] != 0; }

  // Adds the given element to the queue and set its priority.
  // Preconditions: Contains(element) must be false.
  void Add(Element element) {
    DCHECK(!Contains(element.Index()));
    SetAndIncreasePriority(++size_, element);
  }

  // Top() returns the top element and Pop() remove it from the queue.
  // Preconditions: IsEmpty() must be false.
  Element Top() const { return heap_[1]; }
  void Pop() {
    DCHECK(!IsEmpty());
    position_[Top().Index()] = 0;
    const int old_size = size_--;
    if (old_size > 1) SetAndDecreasePriority(1, heap_[old_size]);
  }

  // Removes the element with given index from the queue.
  // Preconditions: Contains(index) must be true.
  void Remove(int index) {
    DCHECK(Contains(index));
    const int to_replace = position_[index];
    position_[index] = 0;
    const int old_size = size_--;
    if (to_replace == old_size) return;
    const Element element = heap_[old_size];
    if (less_(element, heap_[to_replace])) {
      SetAndDecreasePriority(to_replace, element);
    } else {
      SetAndIncreasePriority(to_replace, element);
    }
  }

  // Change the priority of the given element and adjust the queue.
  //
  // Preconditions: Contains(element) must be true.
  void ChangePriority(Element element) {
    DCHECK(Contains(element.Index()));
    const int i = position_[element.Index()];
    if (i > 1 && less_(heap_[i >> 1], element)) {
      SetAndIncreasePriority(i, element);
    } else {
      SetAndDecreasePriority(i, element);
    }
  }

  // Optimized version of ChangePriority() when we know the direction.
  void IncreasePriority(Element element) {
    SetAndIncreasePriority(position_[element.Index()], element);
  }
  void DecreasePriority(Element element) {
    SetAndDecreasePriority(position_[element.Index()], element);
  }

  // Returns the element with given index.
  Element GetElement(int index) const { return heap_[position_[index]]; }

  // For i in [0, Size()) returns an element currently in the queue.
  // This can be used to get a random element from the queue for instance.
  Element QueueElement(int i) const { return heap_[1 + i]; }

 private:
  // Puts the given element at heap index i.
  void Set(int i, Element element) {
    heap_[i] = element;
    position_[element.Index()] = i;
  }

  // Puts the given element at heap index i and update the heap knowing that the
  // element has a priority <= than the priority of the element currently at
  // this position.
  void SetAndDecreasePriority(int i, const Element element) {
    const int size = size_;
    while (true) {
      const int left = i * 2;
      const int right = left + 1;
      if (right > size) {
        if (left > size) break;
        const Element left_element = heap_[left];
        if (!less_(element, left_element)) break;
        Set(i, left_element);
        i = left;
        break;
      }
      const Element left_element = heap_[left];
      const Element right_element = heap_[right];
      if (less_(left_element, right_element)) {
        if (!less_(element, right_element)) break;
        Set(i, right_element);
        i = right;
      } else {
        if (!less_(element, left_element)) break;
        Set(i, left_element);
        i = left;
      }
    }
    Set(i, element);
  }

  // Puts the given element at heap index i and update the heap knowing that the
  // element has a priority >= than the priority of the element currently at
  // this position.
  void SetAndIncreasePriority(int i, const Element element) {
    while (i > 1) {
      const int parent = i >> 1;
      const Element parent_element = heap_[parent];
      if (!less_(parent_element, element)) break;
      Set(i, parent_element);
      i = parent;
    }
    Set(i, element);
  }

  int size_;
  Compare less_;
  std::vector<Element> heap_;
  std::vector<int> position_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_UTIL_INTEGER_PQ_H_
