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

#include "ortools/math_opt/elemental/elements.h"

#include <iterator>
#include <ostream>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"

namespace operations_research::math_opt {

absl::string_view ToString(const ElementType element_type) {
  const int element_type_id = static_cast<int>(element_type);
  CHECK_LT(element_type_id, std::size(kElementNames));
  return kElementNames[element_type_id];
}

std::ostream& operator<<(std::ostream& ostr, const ElementType element_type) {
  ostr << ToString(element_type);
  return ostr;
}

}  // namespace operations_research::math_opt
