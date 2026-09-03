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

#include <cstdint>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/math_opt/elemental/derived_data.h"
#include "ortools/math_opt/elemental/diff.h"
#include "ortools/math_opt/elemental/elemental.h"
#include "ortools/math_opt/elemental/elements.h"

namespace operations_research::math_opt {
namespace {

// Returns a tuple of names of elements corresponding to the element ids in the
// key. For example, if there is a variable named "x" with id 0 and a constraint
// named "c" with id 3, then
//   GetAttrKeyNames(e, DoubleAttr2::kLinConCoef, AttrKey(0, 3))
// returns "(x, c)".
template <typename AttrType>
std::string GetAttrKeyNames(const Elemental& elemental, const AttrType attr,
                            const AttrKeyFor<AttrType> key) {
  std::vector<std::string> element_names;
  for (int i = 0; i < GetAttrKeySize<AttrType>(); ++i) {
    auto n = elemental.GetElementNameUntyped(GetElementTypes(attr)[i], key[i]);
    CHECK_OK(n);
    element_names.push_back(absl::StrCat("\"", absl::CEscape(*n), "\""));
  }
  return absl::StrCat("(", absl::StrJoin(element_names, ", "), ")");
}

std::vector<std::string> ElementalModelDebugString(const Elemental& elemental) {
  std::vector<std::string> lines;
  lines.push_back("Model:");
  if (!elemental.model_name().empty()) {
    lines.push_back(absl::StrCat("model_name: \"",
                                 absl::CEscape(elemental.model_name()), "\""));
  }
  if (!elemental.primary_objective_name().empty()) {
    lines.push_back(
        absl::StrCat("primary_objective_name: \"",
                     absl::CEscape(elemental.primary_objective_name()), "\""));
  }

  for (ElementType e : kElements) {
    if (elemental.NextElementId(e) == 0) {
      continue;
    }
    lines.push_back(absl::StrCat("ElementType: ", e,
                                 " num_elements: ", elemental.NumElements(e),
                                 " next_id: ", elemental.NextElementId(e)));
    std::vector<int64_t> elements = elemental.AllElementsUntyped(e);
    absl::c_sort(elements);
    for (const int64_t element_id : elements) {
      absl::StatusOr<absl::string_view> element_name =
          elemental.GetElementNameUntyped(e, element_id);
      CHECK_OK(element_name);  // CHECK is safe, just got from the model.
      lines.push_back(absl::StrCat("  id: ", element_id, " name: \"",
                                   absl::CEscape(*element_name), "\""));
    }
  }
  AllAttrs::ForEachAttr([&elemental, &lines](const auto a) {
    const int64_t num_non_defaults = elemental.AttrNumNonDefaults(a);
    if (num_non_defaults == 0) {
      return;
    }
    lines.push_back(
        absl::StrCat("Attribute: ", a, " non-defaults: ", num_non_defaults));
    auto keys = elemental.AttrNonDefaults(a);
    absl::c_sort(keys);
    for (const auto key : keys) {
      lines.push_back(absl::StrCat(
          "  key: ", key,
          " value: ", FormatAttrValue(elemental.GetAttr(a, key)),
          " (key names: ", GetAttrKeyNames(elemental, a, key), ")"));
    }
  });
  return lines;
}

std::vector<std::string> DiffDebugString(const Elemental& elemental,
                                         const Diff& diff) {
  std::vector<std::string> lines;
  for (ElementType e : kElements) {
    if (diff.checkpoint(e) == elemental.NextElementId(e) &&
        diff.deleted_elements(e).empty()) {
      continue;
    }
    lines.push_back(absl::StrCat("ElementType: ", e,
                                 " next_id: ", elemental.NextElementId(e),
                                 " checkpoint: ", diff.checkpoint(e)));
    if (diff.deleted_elements(e).empty()) {
      continue;
    }
    std::vector<int64_t> deleted_elements;
    for (const int64_t element_id : diff.deleted_elements(e)) {
      deleted_elements.push_back(element_id);
    }
    absl::c_sort(deleted_elements);
    lines.push_back(absl::StrCat("  deleted: [",
                                 absl::StrJoin(deleted_elements, ", "), "]"));
  }
  AllAttrs::ForEachAttr([&lines, &elemental, &diff]<typename Attr>(Attr attr) {
    if (diff.modified_keys(attr).empty()) {
      return;
    }
    std::vector<AttrKeyFor<Attr>> sorted_keys;
    for (AttrKeyFor<Attr> key : diff.modified_keys(attr)) {
      sorted_keys.push_back(key);
    }
    absl::c_sort(sorted_keys);
    lines.push_back(absl::StrCat("Attribute: ", attr));
    for (const auto key : sorted_keys) {
      lines.push_back(absl::StrCat(
          "  ", key, " (names: ", GetAttrKeyNames(elemental, attr, key), ")"));
    }
  });
  return lines;
}

std::string ElementalDebugString(
    const Elemental& elemental,
    absl::Span<const std::pair<int64_t, const Diff*>> diffs) {
  std::vector<std::string> lines = ElementalModelDebugString(elemental);
  for (const auto& [id, diff] : diffs) {
    lines.push_back(absl::StrCat("Diff: ", id));
    absl::c_move(DiffDebugString(elemental, *diff), std::back_inserter(lines));
  }
  return absl::StrJoin(lines, "\n");
}

}  // namespace

std::string Elemental::DebugString(const bool print_diffs) const {
  std::vector<std::pair<int64_t, const Diff*>> diffs;
  if (print_diffs) {
    for (auto& [id, diff] : diffs_->GetAll()) {
      diffs.push_back({id, diff});
    }
  }
  // It intentional that that this function is implemented without access to the
  // private API of elemental. This allows us to change the implementation
  // elemental without breaking the DebugString() code.
  return ElementalDebugString(*this, absl::MakeConstSpan(diffs));
}

}  // namespace operations_research::math_opt
