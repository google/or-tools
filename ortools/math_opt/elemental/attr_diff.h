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

#ifndef OR_TOOLS_MATH_OPT_ELEMENTAL_ATTR_DIFF_H_
#define OR_TOOLS_MATH_OPT_ELEMENTAL_ATTR_DIFF_H_

#include <cstdint>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "ortools/math_opt/elemental/attr_key.h"

namespace operations_research::math_opt {

// Tracks modifications to an Attribute with a key size of n (e.g., variable
// lower bound has a key size of 1).
template <int n, typename Symmetry>
class AttrDiff {
 public:
  using Key = AttrKey<n, Symmetry>;

  // On creation, the attribute is not modified for any key.
  AttrDiff() = default;

  // Clear all tracked modifications.
  void Advance() { modified_keys_.clear(); }

  // Mark the attribute as modified for `key`.
  void SetModified(const Key key) { modified_keys_.insert(key); }

  // Returns the attribute keys that have been modified for this attribute (the
  // elements where set_modified() was called without a subsequent call to
  // Advance()).
  const AttrKeyHashSet<Key>& modified_keys() const { return modified_keys_; }

  bool has_modified_keys() const { return !modified_keys_.empty(); }

  // Stop tracking modifications for this attribute key. (Typically invoked when
  // an element in the key was deleted from the model.)
  void Erase(const Key key) { modified_keys_.erase(key); }

 private:
  AttrKeyHashSet<Key> modified_keys_;
};

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_ELEMENTAL_ATTR_DIFF_H_
