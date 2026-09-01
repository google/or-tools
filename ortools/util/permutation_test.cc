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

#include "ortools/util/permutation.h"

#include <array>

#include "gtest/gtest.h"

namespace operations_research {

TEST(PermutationTest, BasicOperation) {
  auto data = std::to_array<int>({1005, 1000, 1004, 1001, 1003, 1002});
  auto permutation = std::to_array<int>({1, 3, 5, 4, 2, 0});

  ArrayIndexCycleHandler<int, int> handler(data.data());
  PermutationApplier<int> permuter(&handler);
  static_assert(data.size() == permutation.size());
  permuter.Apply(permutation.data(), 0, data.size());
  for (int i = 0; i < data.size(); ++i) {
    EXPECT_EQ(data[i], 1000 + i);
  }
}

}  // namespace operations_research
