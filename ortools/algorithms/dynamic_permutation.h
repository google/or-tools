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

#ifndef OR_TOOLS_ALGORITHMS_DYNAMIC_PERMUTATION_H_
#define OR_TOOLS_ALGORITHMS_DYNAMIC_PERMUTATION_H_

#include <memory>
#include <set>  // TODO(user): remove when no longer used.
#include <vector>

#include "ortools/base/logging.h"

namespace operations_research {

class SparsePermutation;

// Maintains a 'partial' permutation of [0..n-1] onto itself, with a dynamic
// API allowing it to be built incrementally, and allowing some backtracking.
// This is tuned for a specific usage by ./find_graph_symmetries.cc.
//
// RAM usage: as of 2014-04, this class needs less than:
// 32.125 * (n + 2 * support_size) bytes.
class DynamicPermutation {
 public:
  // Upon construction, every element i in [0..n-1] maps to itself.
  explicit DynamicPermutation(int n);

  int Size() const { return image_.size(); }  // Return the original "n".

  // Declares a set of mappings for this permutation: src[i] will map to dst[i].
  // Requirements that are DCHECKed:
  // - "src" and "dst" must have the same size.
  // - For all i, src[i] must not already be mapped to something.
  // - For all i, dst[i] must not already be the image of something.
  //
  // Complexity: amortized O(src.size()).
  void AddMappings(const std::vector<int>& src, const std::vector<int>& dst);

  // Undoes the last AddMappings() operation, and fills the "undone_mapping_src"
  // vector with the src of that last operation. This works like an undo stack.
  // For example, applying the sequence (Add, Add, Add, Undo, Add, Undo, Undo)
  // has exactly the same effect as applying the first Add() alone.
  // If you call this too may times (i.e. there is nothing left to undo), it is
  // simply a no-op.
  //
  // Complexity: same as the AddMappings() operation being undone.
  void UndoLastMappings(std::vector<int>* undone_mapping_src);

  // Makes the permutation back to the identity (i.e. like right after
  // construction).
  // Complexity: O(support size).
  void Reset();

  int ImageOf(int i) const;  // Complexity: one vector lookup.

  // Returns the union of all "src" ever given to AddMappings().
  const std::vector<int>& AllMappingsSrc() const { return mapping_src_stack_; }

  // While the permutation is partially being built, the orbit of elements will
  // either form unclosed paths, or closed cycles. In the former case,
  // RootOf(i) returns the start of the path where i lies. If i is on a cycle,
  // RootOf(i) will return some element of its cycle (meaning that if i maps to
  // itself, RootOf(i) = i).
  //
  // Complexity: O(log(orbit size)) in average, assuming that the mappings are
  // added in a random order. O(orbit size) in the worst case.
  int RootOf(int i) const;

  // The exhaustive set of the 'loose end' of the incomplete cycles
  // (e.g., paths) built so far.
  // TODO(user): use a faster underlying container like SparseBitSet, and
  // tweak this API accordingly.
  const std::set<int>& LooseEnds() const { return loose_ends_; }

  // Creates a SparsePermutation representing the current permutation.
  // Requirements: the permutation must only have cycles.
  //
  // Complexity: O(support size).
  std::unique_ptr<SparsePermutation> CreateSparsePermutation() const;

  std::string DebugString() const;

 private:
  std::vector<int> image_;
  // ancestor_[i] isn't exactly RootOf(i): it might itself have an ancestor, and
  // so on.
  std::vector<int> ancestor_;

  // The concatenation of all "src" ever given to AddMappings(), and their
  // sizes, to implement the undo stack. Note that "mapping_src_stack_" contains
  // exactly the support of the permutation.
  std::vector<int> mapping_src_stack_;
  std::vector<int> mapping_src_size_stack_;

  // See the homonymous accessor, above.
  std::set<int> loose_ends_;

  // Used transiently by CreateSparsePermutation(). Its resting state is:
  // size=Size(), all elements are false.
  mutable std::vector<bool> tmp_mask_;
};

// Forced-inline for the speed.
inline int DynamicPermutation::ImageOf(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, Size());
  return image_[i];
}

// Forced-inline for the speed.
inline int DynamicPermutation::RootOf(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, Size());
  while (true) {
    const int j = ancestor_[i];
    if (j == i) return i;
    i = j;
  }
}

}  // namespace operations_research

#endif  // OR_TOOLS_ALGORITHMS_DYNAMIC_PERMUTATION_H_
