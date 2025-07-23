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

#include "ortools/math_opt/elemental/elemental.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ortools/math_opt/elemental/arrays.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/derived_data.h"
#include "ortools/math_opt/elemental/diff.h"
#include "ortools/math_opt/elemental/elements.h"

namespace operations_research::math_opt {

Elemental::Elemental(std::string model_name, std::string primary_objective_name)
    : model_name_(std::move(model_name)),
      primary_objective_name_(std::move(primary_objective_name)) {
  // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
  ForEachIndex<AllAttrs::kNumAttrTypes>([this]<int attr_type_index>() {
    using Descriptor = AllAttrs::TypeDescriptor<attr_type_index>;
    for (const auto a : Descriptor::Enumerate()) {
      const int index = static_cast<int>(a);
      attrs_[a] = StorageForAttrType<attr_type_index>(
          Descriptor::kAttrDescriptors[index].default_value);
    }
  });
}

std::optional<Elemental::DiffHandle> Elemental::GetDiffHandle(
    const int64_t id) const {
  if (diffs_->Get(id) == nullptr) {
    return std::nullopt;
  }
  return DiffHandle(id, diffs_.get());
}

Elemental::DiffHandle Elemental::AddDiff() {
  auto diff = std::make_unique<Diff>();
  diff->Advance(CurrentCheckpoint());
  const int64_t diff_id = diffs_->Insert(std::move(diff));
  return DiffHandle(diff_id, diffs_.get());
}

bool Elemental::DeleteDiff(const DiffHandle diff) {
  if (&diff.diffs_ != diffs_.get()) {
    return false;
  }
  return diffs_->Erase(diff.diff_id_);
}

bool Elemental::Advance(const DiffHandle diff) {
  if (diffs_.get() != &diff.diffs_) {
    return false;
  }
  Diff* d = diffs_->UpdateAndGet(diff.diff_id_);
  if (d == nullptr) {
    return false;
  }
  d->Advance(CurrentCheckpoint());
  return true;
}

bool Elemental::DeleteElementUntyped(const ElementType e, int64_t id) {
  if (!mutable_element_storage(e).Erase(id)) {
    return false;
  }
  for (auto& [unused, diff] : diffs_->UpdateAndGetAll()) {
    diff->DeleteElement(e, id);
  }
  // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
  AllAttrs::ForEachAttr([this, e, id]<typename AttrType>(AttrType a) {
    ForEachIndex<GetAttrKeySize<AttrType>()>(
        // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
        [&]<int i>() {
          if (GetElementTypes(a)[i] == e) {
            UpdateAttrOnElementDeleted<AttrType, i>(a, id);
          }
        });
    // If `a` is element-valued, we need to remove all keys that refer to the
    // deleted element.
    if constexpr (is_element_id_v<ValueTypeFor<AttrType>>) {
      if (e == ValueTypeFor<AttrType>::type()) {
        const auto keys = element_ref_trackers_[a].GetKeysReferencing(
            ValueTypeFor<AttrType>(id));
        for (const auto key : keys) {
          // Don't use SetAttr here, we do not want to track this change, it is
          // already implied by the deletion of the element. But still clean up
          // the diff trackers for all keys and zero out the value.
          for (auto& [unused, diff] : diffs_->UpdateAndGetAll()) {
            diff->EraseKeysForAttr(a, {key});
          }
          attrs_[a].Erase(key);
        }
      }
    }
  });

  return true;
}

template <typename AttrType, int i>
void Elemental::UpdateAttrOnElementDeleted(const AttrType a, const int64_t id) {
  auto& attr_storage = attrs_[a];
  // We consider the case of n == 1 separately so that we can ensure that
  // for any attribute with a key size of one, the AttrDiff has no deleted
  // elements. (If we did not specialize this code, we would need to check for
  // deleted elements when building our ModelUpdateProto, see
  // README.md#checkpoints-and-model-updates for an explanation.)
  if constexpr (GetAttrKeySize<AttrType>() == 1) {
    for (auto& [unused, diff] : diffs_->UpdateAndGetAll()) {
      diff->EraseKeysForAttr(a, {AttrKey(id)});
    }
    attr_storage.Erase(AttrKey(id));
  } else {
    // NOTE: We explicitly spell out the type here, so that if `Slice` ever
    // returns a reference in `attr_storage` instead of a copy, we are forced
    // to update this code to make a copy of the slice (otherwise the slice
    // would be invalidated by calls to `Erase()` below).
    const std::vector<AttrKeyFor<AttrType>> keys =
        attr_storage.template Slice<i>(id);
    for (auto& [unused, diff] : diffs_->UpdateAndGetAll()) {
      diff->EraseKeysForAttr(a, keys);
    }
    for (const auto& key : keys) {
      attr_storage.Erase(key);
    }
  }
}

std::array<int64_t, kNumElements> Elemental::CurrentCheckpoint() const {
  std::array<int64_t, kNumElements> result;
  for (int i = 0; i < kNumElements; ++i) {
    result[i] = elements_[i].next_id();
  }
  return result;
}

Elemental Elemental::Clone(
    std::optional<absl::string_view> new_model_name) const {
  Elemental result(std::string(new_model_name.value_or(model_name_)),
                   primary_objective_name_);
  result.elements_ = elements_;
  result.attrs_ = attrs_;
  return result;
}

std::ostream& operator<<(std::ostream& ostr, const Elemental& elemental) {
  ostr << elemental.DebugString();
  return ostr;
}

}  // namespace operations_research::math_opt
