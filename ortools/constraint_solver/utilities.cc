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

#include "ortools/constraint_solver/utilities.h"

#include <cstdint>
#include <vector>

namespace operations_research {

// ----- Vector manipulations -----

std::vector<int64_t> ToInt64Vector(const std::vector<int>& input) {
  std::vector<int64_t> result(input.size());
  for (int i = 0; i < input.size(); ++i) {
    result[i] = input[i];
  }
  return result;
}
}  // namespace operations_research
