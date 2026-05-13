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

#ifndef OR_TOOLS_MATH_OPT_ELEMENTAL_ELEMENT_REF_TRACKER_H_
#define OR_TOOLS_MATH_OPT_ELEMENTAL_ELEMENT_REF_TRACKER_H_

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "ortools/base/map_util.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/elements.h"

namespace operations_research::math_opt {

template <typename ValueType, int n, typename Symmetry>
class ElementRefTracker;

// The `ElementRefTracker` for a given attribute type descriptor.
template <typename Descriptor>
using ElementRefTrackerForAttrTypeDescriptor =
    ElementRefTracker<typename Descriptor::ValueType,
                      Descriptor::kNumKeyElements,
                      typename Descriptor::Symmetry>;

// A tracker for values that reference elements.
//
// This is used to delete attributes when the elements they reference are
// deleted.
template <ElementType element_type, int n, typename Symmetry>
class ElementRefTracker<ElementId<element_type>, n, Symmetry> {
 public:
  using ElemId = ElementId<element_type>;
  using Key = AttrKey<n, Symmetry>;

  // Returns the set of keys that reference element `id`.
  const absl::flat_hash_set<Key>& GetKeysReferencing(const ElemId id) const {
    return gtl::FindWithDefault(element_id_to_attr_keys_, id);
  }

  // Tracks the fact that attribute with key `key` has a value that references
  // elements.
  void Track(const Key key, const ElemId id) {
    element_id_to_attr_keys_[id].insert(key);
  }

  void Untrack(const Key key, const ElemId id) {
    const auto it = element_id_to_attr_keys_.find(id);
    if (it != element_id_to_attr_keys_.end()) {
      it->second.erase(key);
      if (it->second.empty()) {
        element_id_to_attr_keys_.erase(it);
      }
    }
  }

  void Clear() { element_id_to_attr_keys_.clear(); }

 private:
  // A map of element id to the list of attribute keys that have a non-default
  // value for this element.
  absl::flat_hash_map<ElemId, absl::flat_hash_set<Key>>
      element_id_to_attr_keys_;
};

// Other value types do not need tracking.
template <typename ValueType, int n, typename Symmetry>
class ElementRefTracker {};

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_ELEMENTAL_ELEMENT_REF_TRACKER_H_
