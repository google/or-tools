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

#include "ortools/algorithms/dynamic_permutation.h"

#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

TEST(DynamicPermutationTest, EndToEnd) {
  DynamicPermutation perm(10);
  EXPECT_EQ("", perm.DebugString());

  // Incrementally enter the following permutation:
  // 5->3->6(->5); 1->2(->1); 8->9->7(->8).
  perm.AddMappings({3, 5}, {6, 3});
  EXPECT_THAT(perm.LooseEnds(), UnorderedElementsAre(6));
  perm.AddMappings({1, 0}, {2, 0});
  EXPECT_THAT(perm.LooseEnds(), UnorderedElementsAre(2, 6));
  perm.AddMappings({6}, {5});
  EXPECT_THAT(perm.LooseEnds(), UnorderedElementsAre(2));

  // Temporarily add two mappings and undo them.
  perm.AddMappings({2, 4, 7}, {4, 9, 8});
  EXPECT_THAT(perm.LooseEnds(), UnorderedElementsAre(9, 8));
  perm.AddMappings({8}, {7});
  EXPECT_THAT(perm.LooseEnds(), UnorderedElementsAre(9));
  std::vector<int> last_mapping_src;
  perm.UndoLastMappings(&last_mapping_src);
  EXPECT_THAT(last_mapping_src, ElementsAre(8));
  perm.UndoLastMappings(&last_mapping_src);
  EXPECT_THAT(last_mapping_src, ElementsAre(2, 4, 7));
  EXPECT_THAT(perm.LooseEnds(), UnorderedElementsAre(2));

  // Finish entering the permutation described above.
  perm.AddMappings({2, 8, 7}, {1, 9, 8});
  perm.AddMappings({9}, {7});
  EXPECT_EQ("(1 2) (3 6 5) (7 8 9)", perm.DebugString());
  EXPECT_THAT(perm.AllMappingsSrc(), ElementsAre(3, 5, 1, 0, 6, 2, 8, 7, 9));
  EXPECT_THAT(perm.LooseEnds(), IsEmpty());

  perm.Reset();
  EXPECT_EQ("", perm.DebugString());
  EXPECT_THAT(perm.AllMappingsSrc(), IsEmpty());
  EXPECT_THAT(perm.LooseEnds(), IsEmpty());
  perm.UndoLastMappings(&last_mapping_src);
  EXPECT_THAT(last_mapping_src, IsEmpty());
  EXPECT_EQ("", perm.DebugString());
}

// TODO(user): better, more focused and modular tests that cover all the logic
// and the corner cases.

// TODO(user): verify the average complexity claim of RootOf().

// TODO(user): Add debug-only death tests that verify that any misuse of the
// API triggers a DCHECK fail.

}  // namespace
}  // namespace operations_research
