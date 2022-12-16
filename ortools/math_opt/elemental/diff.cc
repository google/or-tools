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

#include "ortools/math_opt/elemental/diff.h"

#include <array>
#include <cstdint>

#include "ortools/math_opt/elemental/elements.h"

namespace operations_research::math_opt {

void Diff::Advance(const std::array<int64_t, kNumElements>& checkpoints) {
  for (int i = 0; i < kNumElements; ++i) {
    element_diffs_[i].Advance(checkpoints[i]);
  }
  attr_diffs_.ForEachAttrValue([](auto& diff) { diff.Advance(); });
}

}  // namespace operations_research::math_opt
