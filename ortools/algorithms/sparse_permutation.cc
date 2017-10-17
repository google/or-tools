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

#include "ortools/algorithms/sparse_permutation.h"

#include <algorithm>
#include "ortools/base/logging.h"
#include "ortools/base/join.h"

namespace operations_research {

void SparsePermutation::RemoveCycles(const std::vector<int>& cycle_indices) {
  // TODO(user): make this a class member to avoid allocation if the complexity
  // becomes an issue. In this case, also optimize the loop below by not copying
  // the first cycles.
  std::vector<bool> should_be_deleted(NumCycles(), false);
  for (int i : cycle_indices) {
    DCHECK_GE(i, 0);
    DCHECK_LT(i, NumCycles());
    DCHECK(!should_be_deleted[i])
        << "Duplicate index given to RemoveCycles(): " << i;
    should_be_deleted[i] = true;
  }
  int new_cycles_size = 0;      // new index in cycles_
  int new_cycle_ends_size = 0;  // new index in cycle_ends_
  int start = 0;
  for (int i = 0; i < NumCycles(); ++i) {
    const int end = cycle_ends_[i];
    if (!should_be_deleted[i]) {
      for (int j = start; j < end; ++j) {
        cycles_[new_cycles_size++] = cycles_[j];
      }
      cycle_ends_[new_cycle_ends_size++] = new_cycles_size;
    }
    start = end;
  }
  cycles_.resize(new_cycles_size);
  cycle_ends_.resize(new_cycle_ends_size);
}

std::string SparsePermutation::DebugString() const {
  DCHECK_EQ(cycles_.empty(), cycle_ends_.empty());
  if (!cycles_.empty()) DCHECK_EQ(cycles_.size(), cycle_ends_.back());
  std::vector<std::vector<int>> cycles;
  int start = 0;
  for (const int end : cycle_ends_) {
    // Find the minimum.
    int min_pos = start;
    for (int i = start + 1; i < end; ++i) {
      if (cycles_[i] < cycles_[min_pos]) min_pos = i;
    }
    std::vector<int> cycle;
    for (int i = min_pos; i < end; ++i) cycle.push_back(cycles_[i]);
    for (int i = start; i < min_pos; ++i) cycle.push_back(cycles_[i]);
    cycles.push_back(cycle);
    start = end;
  }
  std::sort(cycles.begin(), cycles.end());
  std::string out;
  for (const std::vector<int>& cycle : cycles) {
    if (!out.empty()) out += " ";
    out += "(";
    out += strings::Join(cycle, " ");
    out += ")";
  }
  return out;
}

}  // namespace operations_research
