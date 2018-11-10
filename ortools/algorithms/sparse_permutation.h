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

#ifndef OR_TOOLS_ALGORITHMS_SPARSE_PERMUTATION_H_
#define OR_TOOLS_ALGORITHMS_SPARSE_PERMUTATION_H_

#include <string>
#include <vector>

#include "ortools/base/logging.h"

namespace operations_research {

// A compact representation for permutations of {0..N-1} that displaces few
// elements: it needs only O(K) memory for a permutation that displaces
// K elements.
class SparsePermutation {
 public:
  explicit SparsePermutation(int size) : size_(size) {}  // Identity.

  // TODO(user,user): complete the reader API.
  int Size() const { return size_; }
  int NumCycles() const { return cycle_ends_.size(); }

  // Returns the "support" of this permutation; that is, the set of elements
  // displaced by it.
  const std::vector<int>& Support() const { return cycles_; }

  // The permutation has NumCycles() cycles numbered 0 .. NumCycles()-1.
  // To iterate over cycle #i of the permutation, do this:
  //   for (const int e : permutation.Cycle(i)) { ...
  struct Iterator;
  Iterator Cycle(int i) const;

  // This is useful for iterating over the pair {element, image}
  // of a permutation:
  //
  // for (int c = 0; c < perm.NumCycles(); ++c) {
  //   int element = LastElementInCycle(c);
  //   for (int image : perm.Cycle(c)) {
  //     // The pair is (element, image).
  //     ...
  //     element = image;
  //   }
  // }
  //
  // TODO(user): Provide a full iterator for this? Note that we have more
  // information with the loop above. Not sure it is needed though.
  int LastElementInCycle(int i) const;

  // To add a cycle to the permutation, repeatedly call AddToCurrentCycle()
  // with the cycle's orbit, then call CloseCurrentCycle();
  // This shouldn't be called on trivial cycles (of length 1).
  void AddToCurrentCycle(int x);
  void CloseCurrentCycle();

  // Removes the cycles with given indices from the permutation. This
  // works in O(K) for a permutation displacing K elements.
  void RemoveCycles(const std::vector<int>& cycle_indices);

  // Output all non-identity cycles of the permutation, sorted
  // lexicographically (each cycle is described starting by its smallest
  // element; and all cycles are sorted lexicographically against each other).
  // This isn't efficient; use for debugging only.
  // Example: "(1 4 3) (5 9) (6 8 7)".
  std::string DebugString() const;

 private:
  const int size_;
  std::vector<int> cycles_;
  std::vector<int> cycle_ends_;
};

inline void SparsePermutation::AddToCurrentCycle(int x) {
  DCHECK_GE(x, 0);
  DCHECK_LT(x, size_);
  cycles_.push_back(x);
}

inline void SparsePermutation::CloseCurrentCycle() {
  if (cycle_ends_.empty()) {
    DCHECK_GE(cycles_.size(), 2);
  } else {
    DCHECK_GE(cycles_.size(), cycle_ends_.back() + 2);
  }
  cycle_ends_.push_back(cycles_.size());
}

struct SparsePermutation::Iterator {
  // These typedefs allow this iterator to be used within testing::ElementsAre.
  typedef int value_type;
  typedef std::vector<int>::const_iterator const_iterator;

  Iterator() {}
  Iterator(const std::vector<int>::const_iterator& b,
           const std::vector<int>::const_iterator& e)
      : begin_(b), end_(e) {}

  std::vector<int>::const_iterator begin() const { return begin_; }
  std::vector<int>::const_iterator end() const { return end_; }
  const std::vector<int>::const_iterator begin_;
  const std::vector<int>::const_iterator end_;

  int size() const { return end_ - begin_; }
};

inline SparsePermutation::Iterator SparsePermutation::Cycle(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, NumCycles());
  return Iterator(cycles_.begin() + (i == 0 ? 0 : cycle_ends_[i - 1]),
                  cycles_.begin() + cycle_ends_[i]);
}

inline int SparsePermutation::LastElementInCycle(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, cycle_ends_.size());
  DCHECK_GT(cycle_ends_[i], i == 0 ? 0 : cycle_ends_[i - 1]);
  return cycles_[cycle_ends_[i] - 1];
}

}  // namespace operations_research

#endif  // OR_TOOLS_ALGORITHMS_SPARSE_PERMUTATION_H_
