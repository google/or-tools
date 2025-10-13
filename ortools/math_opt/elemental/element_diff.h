// Copyright 2010-2025 Google LLC
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

#ifndef OR_TOOLS_MATH_OPT_ELEMENTAL_ELEMENT_DIFF_H_
#define OR_TOOLS_MATH_OPT_ELEMENTAL_ELEMENT_DIFF_H_

#include <cstdint>

#include "absl/container/flat_hash_set.h"

namespace operations_research::math_opt {

// Tracks the ids of the elements in a model that:
//   1. Are less than the checkpoint for this element.
//   2. Have been deleted since the most recent time the checkpoint was advanced
//      (or creation of the ElementDiff if advance was never called).
//
// Generally:
//   * Element ids should be nonnegative.
//   * Each element should be deleted at most once.
//   * Sequential calls to Advance() should be called on non-decreasing
//     checkpoints.
// However, these are enforced higher up the stack, not in this class.
class ElementDiff {
 public:
  // The current checkpoint for this element, generally the next_id for this
  // element when Advance() was last called (or at creation time if advance was
  // never called).
  int64_t checkpoint() const { return checkpoint_; }

  // The elements that have been deleted before the checkpoint.
  const absl::flat_hash_set<int64_t>& deleted() const { return deleted_; }

  // Tracks the element `id` as deleted if it is less than the checkpoint.
  void Delete(int64_t id) {
    if (id < checkpoint_) {
      deleted_.insert(id);
    }
  }

  // Update the checkpoint and clears all tracked deletions.
  void Advance(int64_t checkpoint) {
    checkpoint_ = checkpoint;
    deleted_.clear();
  }

 private:
  int64_t checkpoint_ = 0;
  absl::flat_hash_set<int64_t> deleted_;
};

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_ELEMENTAL_ELEMENT_DIFF_H_
