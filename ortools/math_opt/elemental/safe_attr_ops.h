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

// Common code between python & C bindings.
#ifndef OR_TOOLS_MATH_OPT_ELEMENTAL_SAFE_ATTR_OPS_H_
#define OR_TOOLS_MATH_OPT_ELEMENTAL_SAFE_ATTR_OPS_H_

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/elemental/derived_data.h"
#include "ortools/math_opt/elemental/elemental.h"

namespace operations_research::math_opt {

template <typename AttrType>
struct AttrOp {
  using Descriptor = AttrTypeDescriptorT<AttrType>;
  using Key = AttrKeyFor<AttrType>;
  using ValueType = typename Descriptor::ValueType;

  static absl::StatusOr<AttrType> SafeCastAttr(int attr) {
    if (attr < 0 || attr >= Descriptor::NumAttrs()) {
      return absl::InvalidArgumentError(
          absl::StrCat("invalid attribute ", attr));
    }
    return static_cast<AttrType>(attr);
  }

  static absl::StatusOr<ValueType> Get(Elemental* e, int attr, Key key) {
    ASSIGN_OR_RETURN(const auto typed_attr, SafeCastAttr(attr));
    return e->GetAttr<Elemental::StatusPolicy>(typed_attr, key);
  }

  static absl::Status Set(Elemental* e, int attr, Key key, ValueType value) {
    ASSIGN_OR_RETURN(const auto typed_attr, SafeCastAttr(attr));
    return e->SetAttr<Elemental::StatusPolicy>(typed_attr, key, value);
  }

  static absl::StatusOr<bool> IsNonDefault(Elemental* e, int attr, Key key) {
    ASSIGN_OR_RETURN(const auto typed_attr, SafeCastAttr(attr));
    return e->AttrIsNonDefault<Elemental::StatusPolicy>(typed_attr, key);
  }

  static absl::StatusOr<int64_t> NumNonDefaults(Elemental* e, int attr) {
    ASSIGN_OR_RETURN(const auto typed_attr, SafeCastAttr(attr));
    return e->AttrNumNonDefaults(typed_attr);
  }

  static absl::StatusOr<std::vector<Key>> GetNonDefaults(Elemental* e,
                                                         int attr) {
    ASSIGN_OR_RETURN(const auto typed_attr, SafeCastAttr(attr));
    return e->AttrNonDefaults(typed_attr);
  }
};

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_ELEMENTAL_SAFE_ATTR_OPS_H_
