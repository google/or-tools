// Copyright 2010-2014 Google
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

#ifndef OR_TOOLS_ALGORITHMS_DENSE_DOUBLY_LINKED_LIST_H_
#define OR_TOOLS_ALGORITHMS_DENSE_DOUBLY_LINKED_LIST_H_

#include <vector>
#include "ortools/base/logging.h"

namespace operations_research {

// Specialized doubly-linked list that initially holds [0..n-1] in an arbitrary
// (user-specified) and fixed order.
// It then supports O(1) removal and access to the next and previous element of
// a given (non-removed) element.
//
// It is very fast and compact: it uses exactly 8*n bytes of memory.
class DenseDoublyLinkedList {
 public:
  // You can construct a DenseDoublyLinkedList with any range-iterable class
  // that also has a size() method. The order of the elements is given by the
  // user and will never change (modulo the removal of elements).
  template <class T>
  explicit DenseDoublyLinkedList(const T& sorted_elements);

  int Size() const { return next_.size(); }

  // Next() (resp. Prev()) must be called on elements that haven't yet been
  // removed. They will return -1 if called on the last (resp. first) element.
  int Next(int i) const;
  int Prev(int i) const;

  // You must not call Remove() twice with the same element.
  void Remove(int i);

 private:
  std::vector<int> next_;
  std::vector<int> prev_;
};

// Inline implementations (forced inline for the speed).

inline int DenseDoublyLinkedList::Next(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, Size());
  DCHECK_GE(next_[i], -1);
  return next_[i];
}

inline int DenseDoublyLinkedList::Prev(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, Size());
  DCHECK_GE(prev_[i], -1);
  return prev_[i];
}

inline void DenseDoublyLinkedList::Remove(int i) {
  const int prev = Prev(i);
  const int next = Next(i);
  if (prev >= 0) next_[prev] = next;
  if (next >= 0) prev_[next] = prev;
  if (DEBUG_MODE) next_[i] = prev_[i] = -2;  // To catch bugs.
}

template <class T>
DenseDoublyLinkedList::DenseDoublyLinkedList(const T& elements)
    : next_(elements.size(), -2), prev_(elements.size(), -2) {
  int last = -1;
  for (const int e : elements) {
    DCHECK_GE(e, 0);
    DCHECK_LE(e, Size());
    DCHECK_EQ(-2, prev_[e]) << "Duplicate element: " << e;
    prev_[e] = last;
    if (last >= 0) next_[last] = e;
    last = e;
  }
  if (!elements.empty()) next_[elements.back()] = -1;
  if (DEBUG_MODE) {
    for (int p : prev_) DCHECK_NE(-2, p);
    for (int n : next_) DCHECK_NE(-2, n);
  }
}

}  // namespace operations_research

#endif  // OR_TOOLS_ALGORITHMS_DENSE_DOUBLY_LINKED_LIST_H_
