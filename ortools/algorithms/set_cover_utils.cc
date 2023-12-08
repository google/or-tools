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

#include "ortools/algorithms/set_cover_utils.h"

#include "ortools/base/adjustable_priority_queue-inl.h"  // IWYU pragma: keep

namespace operations_research {

void SubsetPriorityQueue::Initialize() {
  max_pq_.Clear();
  pq_elements_.assign(ledger_->model()->num_subsets(), SubsetPriority());
}

void SubsetPriorityQueue::Add(SubsetIndex subset, Cost priority) {
  pq_elements_[subset] = SubsetPriority(subset, priority);
  max_pq_.Add(&pq_elements_[subset]);
}

void SubsetPriorityQueue::ChangePriority(SubsetIndex subset, Cost priority) {
  // TODO(user): see if the reference to ledger_ can be removed.
  if (ledger_->marginal_impacts()[subset] != 0) {
    pq_elements_[subset].SetPriority(priority);
    max_pq_.NoteChangedPriority(&pq_elements_[subset]);
    DVLOG(1) << "Priority of subset " << subset << " is now "
             << pq_elements_[subset].GetPriority();
  }
}

void SubsetPriorityQueue::Remove(SubsetIndex subset) {
  if (max_pq_.Contains(&pq_elements_[subset])) {
    DVLOG(1) << "Removing subset " << subset << " from priority queue";
    max_pq_.Remove(&pq_elements_[subset]);
  }
}

SubsetIndex SubsetPriorityQueue::TopSubset() const {
  return max_pq_.Top()->GetSubset();
}

}  // namespace operations_research
