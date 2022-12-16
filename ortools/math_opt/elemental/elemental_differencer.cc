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

#include "ortools/math_opt/elemental/elemental_differencer.h"

#include <cstdint>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "ortools/math_opt/elemental/derived_data.h"
#include "ortools/math_opt/elemental/elemental.h"
#include "ortools/math_opt/elemental/elements.h"

namespace operations_research::math_opt {
namespace {

template <typename T>
absl::flat_hash_set<T> ToSet(const std::vector<T>& vec) {
  return absl::flat_hash_set<T>(vec.begin(), vec.end());
}

template <typename T>
std::vector<T> Sorted(absl::flat_hash_set<T> items) {
  std::vector<T> result(items.begin(), items.end());
  absl::c_sort(result);
  return result;
}

}  // namespace

bool ElementalDifference::ElementDifference::Empty() const {
  return ids.Empty() && different_names.empty() && !next_id_different;
}

bool ElementalDifference::Empty() const {
  if (model_name_different_ || primary_objective_name_different_) {
    return false;
  }
  for (ElementType e : kElements) {
    if (!element_difference(e).Empty()) {
      return false;
    }
  }
  bool empty = true;
  AllAttrs::ForEachAttr([this, &empty](auto attr) {
    empty = empty && this->attr_difference(attr).Empty();
  });
  return empty;
}

ElementalDifference ElementalDifference::Create(
    const Elemental& first, const Elemental& second,
    const ElementalDifferenceOptions& options) {
  ElementalDifference result;
  if (options.check_names) {
    result.model_name_different_ = first.model_name() != second.model_name();
    result.primary_objective_name_different_ =
        first.primary_objective_name() != second.primary_objective_name();
  }
  for (const ElementType e : kElements) {
    ElementDifference& element_diff = result.mutable_element_difference(e);
    const absl::flat_hash_set<int64_t> first_elements =
        ToSet(first.AllElementsUntyped(e));
    const absl::flat_hash_set<int64_t> second_elements =
        ToSet(second.AllElementsUntyped(e));
    element_diff.ids =
        SymmetricDifference<int64_t>(first_elements, second_elements);
    if (options.check_names) {
      for (int64_t element : IntersectSets(first_elements, second_elements)) {
        absl::StatusOr<absl::string_view> first_name =
            first.GetElementNameUntyped(e, element);

        absl::StatusOr<absl::string_view> second_name =
            second.GetElementNameUntyped(e, element);
        // We can CHECK here, we just read the element from the set.
        CHECK_OK(first_name);
        CHECK_OK(second_name);
        if (*first_name != *second_name) {
          element_diff.different_names.insert(element);
        }
      }
    }
    if (options.check_next_id) {
      element_diff.next_id_different =
          first.NextElementId(e) != second.NextElementId(e);
    }
  }
  AllAttrs::ForEachAttr(
      // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
      [&result, &first, &second]<typename AttrType>(AttrType attr) {
        using Key = typename AttributeDifference<AttrType>::Key;
        AttributeDifference<AttrType>& attr_difference =
            result.mutable_attr_difference(attr);
        const absl::flat_hash_set<Key> first_non_defaults =
            ToSet(first.AttrNonDefaults(attr));
        const absl::flat_hash_set<Key> second_non_defaults =
            ToSet(second.AttrNonDefaults(attr));
        attr_difference.keys =
            SymmetricDifference<Key>(first_non_defaults, second_non_defaults);
        for (const Key key :
             IntersectSets(first_non_defaults, second_non_defaults)) {
          if (first.GetAttr(attr, key) != second.GetAttr(attr, key)) {
            attr_difference.different_values.insert(key);
          }
        }
      });
  return result;
}

namespace {

std::string ElementDebugString(const Elemental& elemental, const ElementType e,
                               const int64_t id) {
  const absl::StatusOr<absl::string_view> name =
      elemental.GetElementNameUntyped(e, id);
  CHECK_OK(name);
  return absl::StrCat(id, ": (name: \"", absl::CEscape(*name), "\")");
}

template <typename AttrType>
std::string KeyDebugString(const Elemental& elemental, const AttrType attr,
                           const AttrKeyFor<AttrType> key) {
  std::vector<std::string> element_names;
  for (int i = 0; i < GetAttrKeySize<AttrType>(); ++i) {
    auto name =
        elemental.GetElementNameUntyped(GetElementTypes(attr)[i], key[i]);
    if (name.ok()) {
      element_names.push_back(absl::StrCat("\"", absl::CEscape(*name), "\""));
    } else {
      element_names.push_back("__missing__");
    }
  }
  return absl::StrCat("(", absl::StrJoin(element_names, ", "), ")");
}

}  // namespace

std::string ElementalDifference::Describe(
    const Elemental& first, const Elemental& second,
    const ElementalDifference& difference) {
  if (difference.Empty()) {
    return "No difference";
  }
  std::vector<std::string> lines;
  if (difference.model_name_different_) {
    lines.push_back("model name disagrees:");
    lines.push_back(absl::StrCat("  first_name: \"",
                                 absl::CEscape(first.model_name()), "\""));
    lines.push_back(absl::StrCat("  second_name: \"",
                                 absl::CEscape(second.model_name()), "\""));
  }
  if (difference.primary_objective_name_different_) {
    lines.push_back("primary objective name disagrees:");
    lines.push_back(absl::StrCat("  first_name: \"",
                                 absl::CEscape(first.primary_objective_name()),
                                 "\""));
    lines.push_back(absl::StrCat("  second_name: \"",
                                 absl::CEscape(second.primary_objective_name()),
                                 "\""));
  }
  for (ElementType e : kElements) {
    const ElementDifference& el_diff = difference.element_difference(e);
    if (!el_diff.Empty()) {
      lines.push_back(absl::StrCat(e, ":"));
      if (!el_diff.ids.Empty()) {
        if (!el_diff.ids.only_in_first.empty()) {
          lines.push_back("  element ids in first but not second:");
          for (const int64_t id : Sorted(el_diff.ids.only_in_first)) {
            lines.push_back(
                absl::StrCat("    ", ElementDebugString(first, e, id)));
          }
        }
        if (!el_diff.ids.only_in_second.empty()) {
          lines.push_back("  element ids in second but not first:");
          for (const int64_t id : Sorted(el_diff.ids.only_in_second)) {
            lines.push_back(
                absl::StrCat("    ", ElementDebugString(second, e, id)));
          }
        }
      }
      if (!el_diff.different_names.empty()) {
        lines.push_back("  element ids with disagreeing names:");
        for (const int64_t id : Sorted(el_diff.different_names)) {
          absl::StatusOr<absl::string_view> first_name =
              first.GetElementNameUntyped(e, id);
          absl::StatusOr<absl::string_view> second_name =
              second.GetElementNameUntyped(e, id);
          CHECK_OK(first_name);
          CHECK_OK(second_name);
          lines.push_back(absl::StrCat(
              "    id: ", id, " first_name: \"", absl::CEscape(*first_name),
              "\" second_name: \"", absl::CEscape(*second_name), "\""));
        }
      }
      if (el_diff.next_id_different) {
        lines.push_back("  next_id does not agree:");
        lines.push_back(absl::StrCat("    first: ", first.NextElementId(e)));
        lines.push_back(absl::StrCat("    second: ", second.NextElementId(e)));
      }
    }
  }
  AllAttrs::ForEachAttr(
      // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
      [&lines, &difference, &first, &second]<typename AttrType>(AttrType attr) {
        const auto& attr_diff = difference.attr_difference(attr);
        auto attr_value_str = [attr](const Elemental& elemental,
                                     AttrKeyFor<AttrType> key) {
          auto value = elemental.GetAttr<Elemental::StatusPolicy>(attr, key);
          if (!value.ok()) {
            return std::string("__missing__");
          }
          return FormatAttrValue(*value);
        };
        if (!attr_diff.Empty()) {
          lines.push_back(absl::StrCat("For attribute ", attr,
                                       " errors on the following keys:"));
          for (AttrKeyFor<AttrType> key : attr_diff.AllKeysSorted()) {
            lines.push_back(absl::StrCat(
                "  key: ", key,
                " (name in first: ", KeyDebugString(first, attr, key),
                ") value in first: ", attr_value_str(first, key),
                " (name in second: ", KeyDebugString(second, attr, key),
                ") value in second: ", attr_value_str(second, key)));
          }
        }
      });
  return absl::StrJoin(lines, "\n");
}

std::string ElementalDifference::DescribeDifference(
    const Elemental& first, const Elemental& second,
    const ElementalDifferenceOptions& options) {
  return Describe(first, second, Create(first, second, options));
}

}  // namespace operations_research::math_opt
