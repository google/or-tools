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

#ifndef OR_TOOLS_MATH_OPT_ELEMENTAL_DIFF_H_
#define OR_TOOLS_MATH_OPT_ELEMENTAL_DIFF_H_

#include <array>
#include <cstdint>

#include "absl/container/flat_hash_set.h"
#include "absl/types/span.h"
#include "ortools/math_opt/elemental/attr_diff.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/attributes.h"
#include "ortools/math_opt/elemental/derived_data.h"
#include "ortools/math_opt/elemental/element_diff.h"
#include "ortools/math_opt/elemental/elements.h"

namespace operations_research::math_opt {

// Stores the modifications to the model since the previous checkpoint (or since
// creation of the Diff if Advance() has never been called).
//
// Only the following modifications are tracked explicitly:
//  * elements before the checkpoint
//  * attributes with all elements in the key before the checkpoint
// as all changes involving an element after the checkpoint are implied to be in
// the difference.
//
// Note: users of ElementalImpl can only access a const Diff.
//
// When a element is deleted from the model, the creator of the Diff is
// responsible both for:
//   1. Calling Diff::DeleteElement() on the element,
//   2. For each Attr with a key element on the element type, calling
//      Diff::EraseKeysForAttr()
// We cannot do this all at once for the user, as we do not have access to the
// relevant related keys in steps 3/4 above.
class Diff {
 public:
  Diff() = default;

  // Discards all tracked modifications, and in the future, track only
  // modifications where all elements are at most checkpoint.
  //
  // Generally, checkpoints should be component-wise non-decreasing with each
  // invocation of Advance(), but this is not checked here.
  void Advance(const std::array<int64_t, kNumElements>& checkpoints);

  //////////////////////////////////////////////////////////////////////////////
  // Elements
  //////////////////////////////////////////////////////////////////////////////

  // The current checkpoint for the element type `e`.
  //
  // This equals the next element id for the element type `e` when Advance() was
  // last called (or at creation time if advance was never called).
  int64_t checkpoint(const ElementType e) const {
    return element_diff(e).checkpoint();
  }

  // The elements of element type `e` that have been deleted since the last call
  // to Advance() with id less than the checkpoint.
  const absl::flat_hash_set<int64_t>& deleted_elements(
      const ElementType e) const {
    return element_diff(e).deleted();
  }

  // Tracks the element `id` of element type `e` as deleted if it is less than
  // the checkpoint.
  //
  // WARNING: this does not update any related attributes.
  void DeleteElement(const ElementType e, int64_t id) {
    mutable_element_diff(e).Delete(id);
  }

  //////////////////////////////////////////////////////////////////////////////
  // Attributes
  //////////////////////////////////////////////////////////////////////////////

  // Returns the keys with all elements below the checkpoint where the Attr2 `a`
  // was modified since the last call to Advance().
  template <typename AttrType>
  const AttrKeyHashSet<AttrKeyFor<AttrType>>& modified_keys(
      const AttrType a) const {
    return attr_diffs_[a].modified_keys();
  }

  // Marks that the attribute `a` has been modified for `attr_key`.
  template <typename AttrType>
  void SetModified(const AttrType a, const AttrKeyFor<AttrType> attr_key) {
    if (IsBeforeCheckpoint(a, attr_key)) {
      attr_diffs_[a].SetModified(attr_key);
    }
  }

  // Discard any tracked modifications for attribute `a` on `keys`.
  //
  // Typically invoke when the element with id `keys[i]` is deleted from the
  // model, and where `keys` are the keys `k` for all elements `e` where the `e`
  // has a non-default value for `a`.
  template <typename AttrType>
  void EraseKeysForAttr(const AttrType a,
                        absl::Span<const AttrKeyFor<AttrType>> keys) {
    if (!attr_diffs_[a].has_modified_keys()) {
      return;
    }
    for (const auto& attr_key : keys) {
      if (IsBeforeCheckpoint(a, attr_key)) {
        attr_diffs_[a].Erase(attr_key);
      }
    }
  }

 private:
  const ElementDiff& element_diff(const ElementType e) const {
    // TODO(b/365997645): post C++ 23, prefer std::to_underlying(e).
    return element_diffs_[static_cast<int>(e)];
  }

  ElementDiff& mutable_element_diff(const ElementType e) {
    return element_diffs_[static_cast<int>(e)];
  }

  // Returns true if all elements if `key` are before their respective
  // checkpoints.
  template <typename AttrType>
  bool IsBeforeCheckpoint(const AttrType a, const AttrKeyFor<AttrType> key) {
    for (int i = 0; i < GetAttrKeySize<AttrType>(); ++i) {
      if (key[i] >= element_diff(GetElementTypes(a)[i]).checkpoint()) {
        return false;
      }
    }
    return true;
  }

  std::array<ElementDiff, kNumElements> element_diffs_;

  template <int i>
  using DiffForAttr = AttrDiff<AllAttrs::TypeDescriptor<i>::kNumKeyElements,
                               typename AllAttrs::TypeDescriptor<i>::Symmetry>;
  AttrMap<DiffForAttr> attr_diffs_;
};

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_ELEMENTAL_DIFF_H_
