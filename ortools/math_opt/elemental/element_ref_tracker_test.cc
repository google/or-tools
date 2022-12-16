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

#include "ortools/math_opt/elemental/element_ref_tracker.h"

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/elements.h"
#include "ortools/math_opt/elemental/symmetry.h"

namespace operations_research::math_opt {

namespace {
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

TEST(ElementRefTrackerTest, ElementValued) {
  const VariableId x(0);
  const VariableId y(1);

  ElementRefTracker<VariableId, 1, NoSymmetry> tracker;
  tracker.Track(AttrKey(1), x);
  tracker.Track(AttrKey(2), x);
  tracker.Track(AttrKey(3), y);
  EXPECT_THAT(tracker.GetKeysReferencing(x),
              UnorderedElementsAre(AttrKey(1), AttrKey(2)));
  EXPECT_THAT(tracker.GetKeysReferencing(y), UnorderedElementsAre(AttrKey(3)));

  tracker.Untrack(AttrKey(1), x);
  EXPECT_THAT(tracker.GetKeysReferencing(x), UnorderedElementsAre(AttrKey(2)));
  EXPECT_THAT(tracker.GetKeysReferencing(y), UnorderedElementsAre(AttrKey(3)));

  tracker.Untrack(AttrKey(2), x);
  EXPECT_THAT(tracker.GetKeysReferencing(x), IsEmpty());
  EXPECT_THAT(tracker.GetKeysReferencing(y), UnorderedElementsAre(AttrKey(3)));

  tracker.Untrack(AttrKey(3), y);
  EXPECT_THAT(tracker.GetKeysReferencing(x), IsEmpty());
  EXPECT_THAT(tracker.GetKeysReferencing(y), IsEmpty());
}

}  // namespace
}  // namespace operations_research::math_opt
