// Copyright 2010-2017 Google
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

#include <functional>
#include <list>
#include <vector>
#include "ortools/base/basictypes.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"

#ifndef OR_TOOLS_BASE_ADJUSTABLE_PRIORITY_QUEUE_H_
#define OR_TOOLS_BASE_ADJUSTABLE_PRIORITY_QUEUE_H_

namespace operations_research {

template <typename T>
class AdjustablePriorityQueue {
 public:
  AdjustablePriorityQueue() {}

  void Add(T* const val) {
    elems_.push_back(val);
    AdjustUpwards(elems_.size() - 1);
  }

  void Remove(T* const val) {
    const int i = val->GetHeapIndex();
    if (i == elems_.size() - 1) {
      elems_.pop_back();
      return;
    }
    elems_[i] = elems_.back();
    elems_[i]->SetHeapIndex(i);
    elems_.pop_back();
    NoteChangedPriority(elems_[i]);
  }

  bool Contains(const T* const val) const {
    const int i = val->GetHeapIndex();
    if (i < 0 || i >= elems_.size() || elems_[i] != val) {
      return false;
    }
    return true;
  }

  void NoteChangedPriority(T* val) {
    const int i = val->GetHeapIndex();
    const int parent = (i - 1) / 2;
    if (*elems_[parent] < *val) {
      AdjustUpwards(i);
    } else {
      AdjustDownwards(i);
    }
  }

  T* Top() { return elems_[0]; }

  const T* Top() const { return elems_[0]; }

  void Pop() { Remove(Top()); }

  int Size() const { return elems_.size(); }

  bool IsEmpty() const { return elems_.empty(); }

  void Clear() { elems_.clear(); }

  void CheckValid() const {
    for (int i = 0; i < elems_.size(); ++i) {
      const int left_child = 1 + 2 * i;
      if (left_child < elems_.size()) {
        CHECK_GE(elems_[i], elems_[left_child]);
      }
      const int right_child = left_child + 1;
      if (right_child < elems_.size()) {
        CHECK_GE(elems_[i], elems_[right_child]);
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
      if (!(*elems_[parent] < *t)) {
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
                          *elems_[left_child] < *elems_[right_child])
                             ? right_child
                             : left_child;
      if (!(*t < *elems_[next_i])) {
        break;
      }
      elems_[i] = elems_[next_i];
      elems_[i]->SetHeapIndex(i);
      i = next_i;
    }
    elems_[i] = t;
    t->SetHeapIndex(i);
  }

  std::vector<T*> elems_;
  DISALLOW_COPY_AND_ASSIGN(AdjustablePriorityQueue);
};
}  // namespace operations_research

#endif  // OR_TOOLS_BASE_ADJUSTABLE_PRIORITY_QUEUE_H_
