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

#include "ortools/math_opt/storage/model_storage_item.h"

#include <cstdint>
#include <ostream>

#include "absl/strings/string_view.h"
#include "ortools/math_opt/elemental/elements.h"

namespace operations_research::math_opt {
namespace internal {

void FormatModelStorageElement(std::ostream& ostr, ElementType element_type,
                               absl::string_view name, int64_t id) {
  // TODO(b/170992529): handle quoting of invalid characters in the name.
  if (name.empty()) {
    ostr << "__" << kShortElementNames[static_cast<int>(element_type)] << "#"
         << id << "__";
  } else {
    ostr << name;
  }
}

}  // namespace internal
}  // namespace operations_research::math_opt
