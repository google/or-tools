// Copyright 2010-2021 Google LLC
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

#include "ortools/math_opt/core/model_summary.h"

#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"

namespace operations_research {
namespace math_opt {

IdNameBiMap::IdNameBiMap(
    std::initializer_list<std::pair<int64_t, absl::string_view>> ids) {
  for (const auto& pair : ids) {
    Insert(pair.first, std::string(pair.second));
  }
}

}  // namespace math_opt
}  // namespace operations_research
