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

#ifndef ORTOOLS_MATH_OPT_ELEMENTAL_ELEMENTAL_H_
#define ORTOOLS_MATH_OPT_ELEMENTAL_ELEMENTAL_H_

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/die_if_null.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ortools/base/status_builder.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/attr_storage.h"
#include "ortools/math_opt/elemental/derived_data.h"
#include "ortools/math_opt/elemental/diff.h"
#include "ortools/math_opt/elemental/element_ref_tracker.h"
#include "ortools/math_opt/elemental/element_storage.h"
#include "ortools/math_opt/elemental/elements.h"
#include "ortools/math_opt/elemental/thread_safe_id_map.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"

namespace operations_research::math_opt {

// A MathOpt optimization model and modification trackers.
//
// Holds the elements, the attribute values, and tracks modifications to the
// model by `Diff` objects, and keeps them all in sync. See README.md for
// details.
class Elemental {
 public:
  // An opaque value type for a reference to an underlying `Diff` (change
  // tracker).
  class DiffHandle {
   public:
    int64_t id() const { return diff_id_; }

   private:
    explicit DiffHandle(int64_t diff_id, ThreadSafeIdMap<Diff>* diffs)
        : diff_id_(diff_id), diffs_(*ABSL_DIE_IF_NULL(diffs)) {}

    int64_t diff_id_;
    ThreadSafeIdMap<Diff>& diffs_;

    friend class Elemental;
    friend class ElementalTestPeer;
  };

  explicit Elemental(std::string model_name = "",
                     std::string primary_objective_name = "");

  // The name of this optimization model.
  const std::string& model_name() const { return model_name_; }

  // The name of the primary objective of this optimization model.
  const std::string& primary_objective_name() const {
    return primary_objective_name_;
  }

  //////////////////////////////////////////////////////////////////////////////
  // Elements
  //////////////////////////////////////////////////////////////////////////////

  // Creates and returns the id of a new element for the element type `e`.
  template <ElementType e>
  ElementId<e> AddElement(const absl::string_view name) {
    return ElementId<e>(AddElementUntyped(e, name));
  }

  // Type-erased version of `AddElement`. Prefer the latter.
  int64_t AddElementUntyped(const ElementType e, const absl::string_view name) {
    return mutable_element_storage(e).Add(name);
  }

  // Deletes the element with `id` for element type `e`, returning true on
  // success and false if no element was deleted (it was already deleted or the
  // id was not from any existing element).
  template <ElementType e>
  bool DeleteElement(const ElementId<e> id) {
    return DeleteElementUntyped(e, id.value());
  }

  // Type-erased version of `DeleteElement`. Prefer the latter.
  bool DeleteElementUntyped(ElementType e, int64_t id);

  // Returns true the element with `id` for element type `e` exists (it was
  // created and not yet deleted).
  template <ElementType e>
  bool ElementExists(const ElementId<e> id) const {
    return ElementExistsUntyped(e, id.value());
  }

  // Type-erased version of `ElementExists`. Prefer the latter.
  bool ElementExistsUntyped(const ElementType e, const int64_t id) const {
    return element_storage(e).Exists(id);
  }

  // Returns the name of the element with `id` for element type `e`, or an error
  // if this element does not exist.
  template <ElementType e>
  absl::StatusOr<absl::string_view> GetElementName(
      const ElementId<e> id) const {
    return GetElementNameUntyped(e, id.value());
  }

  // Type-erased version of `GetElementName`. Prefer the latter.
  absl::StatusOr<absl::string_view> GetElementNameUntyped(
      const ElementType e, const int64_t id) const {
    return element_storage(e).GetName(id);
  }

  // Returns the ids of all elements of element type `e` in the model in an
  // unsorted, non-deterministic order.
  template <ElementType e>
  ElementIdsVector<e> AllElements() const {
    return ElementIdsVector<e>(AllElementsUntyped(e));
  }

  // Type-erased version of `AllElements`. Prefer the latter.
  std::vector<int64_t> AllElementsUntyped(const ElementType e) const {
    return element_storage(e).AllIds();
  }

  // Returns the id of next element created for element type `e`.
  //
  // Equal to one plus the number of elements that were created for element type
  // `e`. When no elements have been deleted, this equals num_elements(e).
  int64_t NextElementId(const ElementType e) const {
    return element_storage(e).next_id();
  }

  // Returns the number of elements in the model for element type `e`.
  //
  // Equal to the number of elements that were created minus the number
  // deleted for element type `e`.
  int64_t NumElements(const ElementType e) const {
    return element_storage(e).size();
  }

  // Increases next_element_id(e) to `id` if it is currently less than `id`.
  //
  // Useful for reading a model back from proto, most users should not need to
  // call this directly.
  template <ElementType e>
  void EnsureNextElementIdAtLeast(const ElementId<e> id) {
    EnsureNextElementIdAtLeastUntyped(e, id.value());
  }

  // Type-erased version of `EnsureNextElementIdAtLeast`. Prefer the latter.
  void EnsureNextElementIdAtLeastUntyped(const ElementType e, int64_t id) {
    mutable_element_storage(e).EnsureNextIdAtLeast(id);
  }

  //////////////////////////////////////////////////////////////////////////////
  // Attributes
  //////////////////////////////////////////////////////////////////////////////
  struct AlwaysOk {};

  // In all the following functions, `key` must be a valid key for attribute `a`
  // (i.e. elements must exists for all elements ids of `key`). When this is not
  // the case, the behavior is defined by one of the following policies:
  //  - Checks whether each element of the key exists, and dies if not.
  struct DiePolicy {
    using CheckResultT = AlwaysOk;
    template <typename T>
    using Wrapped = T;
  };
  //  - Checks whether each element of the key exists, and sets `*status` if
  //    not. When `*status` is not OK, the model is not modified and is still
  //    valid.
  struct StatusPolicy {
    using CheckResultT = absl::Status;
    template <typename T>
    using Wrapped = absl::StatusOr<T>;
  };
  //  - Does not check whether key elements exists. UB if the key does not exist
  //    (DCHECK-fails in debug mode). Use if you know that the key exists and
  //    you care about performance.
  struct UBPolicy {
    using CheckResultT = AlwaysOk;
    template <typename T>
    using Wrapped = T;
  };

  // Restores the attribute `a` to its default value for all AttrKeys (or for
  // an Attr0, its only value).
  template <typename AttrType>
  void AttrClear(AttrType a);

  // Return the vector of attribute keys where a is non-default.
  template <typename AttrType>
  std::vector<AttrKeyFor<AttrType>> AttrNonDefaults(const AttrType a) const {
    return attrs_[a].NonDefaults();
  }

  // Returns the number of keys where `a` is non-default.
  template <typename AttrType>
  int64_t AttrNumNonDefaults(const AttrType a) const {
    return attrs_[a].num_non_defaults();
  }

  // Returns the value of the attr `a` for `key`:
  //  - `get_attr(DoubleAttr1::kVarUb, AttrKey(x))` returns a `double` value if
  //     element id `x` exists, and crashes otherwise. The returned value is
  //    the default value if the attribute has not been set for `x`.
  //  - `get_attr<StatusPolicy>(DoubleAttr1::kVarUb, AttrKey(x))` returns a
  //    valid `StatusOr<double>` if element id `x` exists, and an error
  //    otherwise.
  template <typename Policy = DiePolicy, typename AttrType>
  typename Policy::template Wrapped<ValueTypeFor<AttrType>> GetAttr(
      AttrType a, AttrKeyFor<AttrType> key) const;

  // Returns true if the attr `a` for `key` has a value different from its
  // default.
  template <typename Policy = DiePolicy, typename AttrType>
  typename Policy::template Wrapped<bool> AttrIsNonDefault(
      AttrType a, AttrKeyFor<AttrType> key) const;

  // Sets the value of the attr `a` for the element `key` to `value`, and
  // returns true if the value of the attribute has changed.
  template <typename Policy = DiePolicy, typename AttrType>
  typename Policy::CheckResultT SetAttr(AttrType a, AttrKeyFor<AttrType> key,
                                        ValueTypeFor<AttrType> value);

  // Returns the set of all keys `k` such that `k[i] == key_elem` and `k` has a
  // non-default value for the attribute `a`.
  template <int i, typename Policy = DiePolicy, typename AttrType = void>
  typename Policy::template Wrapped<std::vector<AttrKeyFor<AttrType>>> Slice(
      AttrType a, int64_t key_elem) const;

  // Returns the size of the given slice: This is equivalent to `Slice(a,
  // key_elem).size()`, but `O(1)`.
  template <int i, typename Policy = DiePolicy, typename AttrType = void>
  typename Policy::template Wrapped<int64_t> GetSliceSize(
      AttrType a, int64_t key_elem) const;

  // Returns a copy of this, but with no diffs. The name of the model can
  // optionally be replaced by `new_model_name`.
  Elemental Clone(
      std::optional<absl::string_view> new_model_name = std::nullopt) const;

  //////////////////////////////////////////////////////////////////////////////
  // Working with proto
  //////////////////////////////////////////////////////////////////////////////

  // Returns an equivalent protocol buffer. Fails if the model is too big
  // to fit in the in-memory representation of the proto (it has more than
  // 2**31-1 elements of a type or non-defaults for an attribute).
  absl::StatusOr<ModelProto> ExportModel(bool remove_names = false) const;

  // Creates an equivalent Elemental to `proto`.
  static absl::StatusOr<Elemental> FromModelProto(const ModelProto& proto);

  // Applies the changes to the model in `update_proto`.
  absl::Status ApplyUpdateProto(const ModelUpdateProto& update_proto);

  //////////////////////////////////////////////////////////////////////////////
  // Diffs
  //////////////////////////////////////////////////////////////////////////////

  // Returns the DiffHandle for `id`, if one exists, or nullopt otherwise.
  std::optional<DiffHandle> GetDiffHandle(int64_t id) const;

  // Returned handle is valid until passed `DeleteDiff` or `*this` is
  // destructed.
  DiffHandle AddDiff();

  // Deletes `diff` & invalidates it. Returns false if the handle was invalid or
  // from the wrong elemental). On success, invalidates `diff`.
  bool DeleteDiff(DiffHandle diff);

  // The number of diffs currently tracking this.
  int64_t NumDiffs() const { return diffs_->Size(); }

  // Returns true on success (fails if diff was null, deleted or from the wrong
  // elemental). Warning: diff is modified (owned by this).
  bool Advance(DiffHandle diff);

  // Internal use only (users of Elemental cannot access Diff directly), but
  // prefer to invoking Diff::modified_keys() directly..
  //
  // Returns the modified keys in a Diff for an attribute, filtering out the
  // keys referring to an element that has been deleted.
  //
  // This is needed because in some situations where a variable is deleted
  // we cannot clean up the diff, see README.md.
  template <typename AttrType>
  std::vector<AttrKeyFor<AttrType>> ModifiedKeysThatExist(
      AttrType attr, const Diff& diff) const;

  // Returns a proto describing all changes to the model for `diff` since the
  // most recent call to `Advance(diff)` (or the creation of `diff` if
  // `Advance()` was never called).
  //
  // Returns std::nullopt the resulting ModelUpdateProto would be the empty
  // message (there have been no changes to the model to report).
  //
  // Fails if the update is too big to fit in the in-memory representation of
  // the proto (it has more than 2**31-1 elements in a RepeatedField).
  absl::StatusOr<std::optional<ModelUpdateProto>> ExportModelUpdate(
      DiffHandle diff, bool remove_names = false) const;

  // Prints out the model by element and attribute. If print_diffs is true, also
  // prints out the deleted elements and modified keys for each attribute for
  // each DiffHandle tracked.
  //
  // This is a debug format. Do not assume the output is consistent across CLs
  // and do not parse this format.
  std::string DebugString(bool print_diffs = true) const;

 private:
  DiePolicy::CheckResultT CheckElementExists(ElementType elem_type,
                                             int64_t elem_id, DiePolicy) const;
  StatusPolicy::CheckResultT CheckElementExists(ElementType elem_type,
                                                int64_t elem_id,
                                                StatusPolicy) const;
  UBPolicy::CheckResultT CheckElementExists(ElementType elem_type,
                                            int64_t elem_id, UBPolicy) const;

  template <typename AttrType>
  bool AttrKeyExists(AttrType attr, AttrKeyFor<AttrType> key) const;

  template <typename Policy, typename AttrType>
  typename Policy::CheckResultT CheckAttrKeyExists(
      AttrType a, AttrKeyFor<AttrType> key) const;

  template <typename AttrType, int i>
  void UpdateAttrOnElementDeleted(AttrType a, int64_t id);

  std::array<int64_t, kNumElements> CurrentCheckpoint() const;

  const ElementStorage& element_storage(const ElementType e) const {
    // TODO(b/365997645): post C++ 23, prefer std::to_underlying(e).
    return elements_[static_cast<int>(e)];
  }

  ElementStorage& mutable_element_storage(const ElementType e) {
    return elements_[static_cast<int>(e)];
  }

  std::string model_name_;
  std::string primary_objective_name_;
  std::array<ElementStorage, kNumElements> elements_;
  template <int i>
  using StorageForAttrType =
      AttrStorage<typename AllAttrs::TypeDescriptor<i>::ValueType,
                  AllAttrs::TypeDescriptor<i>::kNumKeyElements,
                  typename AllAttrs::TypeDescriptor<i>::Symmetry>;
  AttrMap<StorageForAttrType> attrs_;
  // For each attribute whose value is an element, we need to keep a map of
  // element to the set of keys whose value refers to that element. This
  // is used to erase the attribute when the element is deleted.
  // This is kept outside of `attrs_` so that we can update the diffs when
  // element deletions trigger attribute deletions.
  template <int i>
  using ElementRefTrackerForAttrType =
      ElementRefTrackerForAttrTypeDescriptor<AllAttrs::TypeDescriptor<i>>;
  AttrMap<ElementRefTrackerForAttrType> element_ref_trackers_;
  // Note: it is important that this is a unique_ptr for two reasons:
  //  1. We need a stable memory address for diffs_ to refer to in DiffHandle,
  //     and the Elemental type is moveable.
  //  2. We want Elemental to be moveable, but ThreadSafeIdMap<Diff> is not.
  std::unique_ptr<ThreadSafeIdMap<Diff>> diffs_ =
      std::make_unique<ThreadSafeIdMap<Diff>>();
};

template <typename Sink>
void AbslStringify(Sink& sink, const Elemental& elemental) {
  sink.Append(elemental.DebugString());
}

std::ostream& operator<<(std::ostream& ostr, const Elemental& elemental);

///////////////////////////////////////////////////////////////////////////////
// Inline and template implementation

template <typename AttrType>
void Elemental::AttrClear(const AttrType a) {
  // Note: this is slightly faster than setting each non-default back to the
  // default value.
  const std::vector<AttrKeyFor<AttrType>> non_defaults = AttrNonDefaults(a);
  if (!non_defaults.empty()) {
    for (auto& [unused, diff] : diffs_->UpdateAndGetAll()) {
      for (const auto key : non_defaults) {
        diff->SetModified(a, key);
      }
    }
  }
  attrs_[a].Clear();
  if constexpr (is_element_id_v<ValueTypeFor<AttrType>>) {
    element_ref_trackers_[a].Clear();
  }
}

#define ELEMENTAL_RETURN_IF_ERROR(expr)                         \
  {                                                             \
    auto error = (expr);                                        \
    if constexpr (!std::is_same_v<decltype(error), AlwaysOk>) { \
      if (!error.ok()) {                                        \
        return error;                                           \
      }                                                         \
    }                                                           \
  }

template <typename Policy, typename AttrType>
typename Policy::template Wrapped<ValueTypeFor<AttrType>> Elemental::GetAttr(
    const AttrType a, const AttrKeyFor<AttrType> key) const {
  ELEMENTAL_RETURN_IF_ERROR(CheckAttrKeyExists<Policy>(a, key));
  return attrs_[a].Get(key);
}

template <typename Policy, typename AttrType>
typename Policy::template Wrapped<bool> Elemental::AttrIsNonDefault(
    const AttrType a, const AttrKeyFor<AttrType> key) const {
  ELEMENTAL_RETURN_IF_ERROR(CheckAttrKeyExists<Policy>(a, key));
  return attrs_[a].IsNonDefault(key);
}

template <typename Policy, typename AttrType>
typename Policy::CheckResultT Elemental::SetAttr(
    const AttrType a, const AttrKeyFor<AttrType> key,
    const ValueTypeFor<AttrType> value) {
  ELEMENTAL_RETURN_IF_ERROR(CheckAttrKeyExists<Policy>(a, key));
  const std::optional<ValueTypeFor<AttrType>> prev_value =
      attrs_[a].Set(key, value);
  if (prev_value.has_value()) {
    if constexpr (is_element_id_v<ValueTypeFor<AttrType>>) {
      element_ref_trackers_[a].Untrack(key, *prev_value);
    }
    for (auto& [unused, diff] : diffs_->UpdateAndGetAll()) {
      diff->SetModified(a, key);
    }
    if constexpr (is_element_id_v<ValueTypeFor<AttrType>>) {
      element_ref_trackers_[a].Track(key, value);
    }
  }
  return {};
}

template <int i, typename Policy, typename AttrType>
typename Policy::template Wrapped<std::vector<AttrKeyFor<AttrType>>>
Elemental::Slice(const AttrType a, const int64_t key_elem) const {
  ELEMENTAL_RETURN_IF_ERROR(
      CheckElementExists(GetElementTypes<AttrType>(a)[i], key_elem, Policy{}));
  return attrs_[a].template Slice<i>(key_elem);
}

template <int i, typename Policy, typename AttrType>
typename Policy::template Wrapped<int64_t> Elemental::GetSliceSize(
    const AttrType a, const int64_t key_elem) const {
  ELEMENTAL_RETURN_IF_ERROR(
      CheckElementExists(GetElementTypes<AttrType>(a)[i], key_elem, Policy{}));
  return attrs_[a].template GetSliceSize<i>(key_elem);
}

inline Elemental::DiePolicy::CheckResultT Elemental::CheckElementExists(
    const ElementType elem_type, const int64_t elem_id, DiePolicy) const {
  // This is ~30% faster than:
  // CHECK_OK(CheckElementExistsUntyped(elem_type, elem_id, StatusPolicy{}));
  CHECK(ElementExistsUntyped(elem_type, elem_id))
      << "no element with id " << elem_id << " for element type " << elem_type;
  return {};
}

inline Elemental::StatusPolicy::CheckResultT Elemental::CheckElementExists(
    const ElementType elem_type, const int64_t elem_id, StatusPolicy) const {
  if (!ElementExistsUntyped(elem_type, elem_id)) {
    return util::InvalidArgumentErrorBuilder()
           << "no element with id " << elem_id << " for element type "
           << elem_type;
  }
  return absl::OkStatus();
}

inline Elemental::UBPolicy::CheckResultT Elemental::CheckElementExists(
    const ElementType elem_type, const int64_t elem_id, UBPolicy) const {
  // Try to be useful in debug mode.
  DCHECK_OK(CheckElementExists(elem_type, elem_id, StatusPolicy{}));
  return {};
}

template <typename AttrType>
bool Elemental::AttrKeyExists(const AttrType attr,
                              const AttrKeyFor<AttrType> key) const {
  for (int i = 0; i < key.size(); ++i) {
    if (!ElementExistsUntyped(GetElementTypes<AttrType>(attr)[i], key[i])) {
      return false;
    }
  }
  return true;
}

template <typename Policy, typename AttrType>
typename Policy::CheckResultT Elemental::CheckAttrKeyExists(
    const AttrType a, const AttrKeyFor<AttrType> key) const {
  for (int i = 0; i < key.size(); ++i) {
    ELEMENTAL_RETURN_IF_ERROR(
        CheckElementExists(GetElementTypes<AttrType>(a)[i], key[i], Policy{}));
  }
  return {};
}

template <typename AttrType>
std::vector<AttrKeyFor<AttrType>> Elemental::ModifiedKeysThatExist(
    AttrType attr, const Diff& diff) const {
  using Key = AttrKeyFor<AttrType>;
  std::vector<Key> keys;
  // Can be a slight overestimate.
  keys.reserve(diff.modified_keys(attr).size());
  for (const Key key : diff.modified_keys(attr)) {
    if constexpr (Key::size() > 1) {
      if (AttrKeyExists(attr, key)) {
        keys.push_back(key);
      }
    } else {
      keys.push_back(key);
    }
  }
  return keys;
}

#undef ELEMENTAL_RETURN_IF_ERROR

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_ELEMENTAL_ELEMENTAL_H_
