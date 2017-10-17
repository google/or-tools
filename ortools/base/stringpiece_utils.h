// Copyright 2010-2017 Google
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

#ifndef OR_TOOLS_BASE_STRINGPIECE_UTILS_H_
#define OR_TOOLS_BASE_STRINGPIECE_UTILS_H_

#include <string.h>

#include "ortools/base/string_view.h"

namespace strings {
// Returns whether s begins with x.
inline bool StartsWith(operations_research::string_view s,
                       operations_research::string_view x) {
  return s.size() >= x.size() && memcmp(s.data(), x.data(), x.size()) == 0;
}

// Returns whether s ends with x.
inline bool EndsWith(operations_research::string_view s,
                     operations_research::string_view x) {
  return s.size() >= x.size() &&
         memcmp(s.data() + (s.size() - x.size()), x.data(), x.size()) == 0;
}
}  // namespace strings
#endif  // OR_TOOLS_BASE_STRINGPIECE_UTILS_H_
