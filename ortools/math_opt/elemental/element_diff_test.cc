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

#include "ortools/math_opt/elemental/element_diff.h"

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research::math_opt {
namespace {

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

TEST(ElementDiffTest, EmptyDiff) {
  ElementDiff diff;
  EXPECT_EQ(diff.checkpoint(), 0);
  EXPECT_THAT(diff.deleted(), IsEmpty());

  diff.Delete(4);
  EXPECT_THAT(diff.deleted(), IsEmpty());
}

TEST(ElementDiffTest, AddsPointsBelowCheckpoint) {
  ElementDiff diff;
  diff.Advance(4);
  EXPECT_EQ(diff.checkpoint(), 4);

  diff.Delete(1);
  diff.Delete(3);
  diff.Delete(4);
  diff.Delete(5);
  EXPECT_THAT(diff.deleted(), UnorderedElementsAre(1, 3));
}

TEST(ElementDiffTest, AdvanceClearsDiff) {
  ElementDiff diff;
  diff.Advance(4);

  diff.Delete(1);
  diff.Delete(3);

  diff.Advance(5);
  EXPECT_THAT(diff.deleted(), IsEmpty());
  EXPECT_EQ(diff.checkpoint(), 5);
}

}  // namespace
}  // namespace operations_research::math_opt
