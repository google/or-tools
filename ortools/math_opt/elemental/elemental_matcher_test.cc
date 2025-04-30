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

#include "ortools/math_opt/elemental/elemental_matcher.h"

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/elemental/elemental.h"
#include "ortools/math_opt/elemental/elemental_differencer.h"
#include "ortools/math_opt/elemental/elements.h"

namespace operations_research::math_opt {

using ::testing::Not;

TEST(ElementalMatcherTest, SuccessOnActualMatch) {
  Elemental e1;
  e1.AddElement<ElementType::kVariable>("x");

  Elemental e2;
  e2.AddElement<ElementType::kVariable>("x");

  EXPECT_THAT(e1, EquivToElemental(e2));
}

TEST(ElementalMatcherTest, FailsOnErrorAndSupportsElementalDifferenceOptions) {
  Elemental e1;
  e1.AddElement<ElementType::kVariable>("x");

  Elemental e2;
  e2.AddElement<ElementType::kVariable>("y");

  EXPECT_THAT(e1, Not(EquivToElemental(e2)));
  EXPECT_THAT(e1, EquivToElemental(
                      e2, ElementalDifferenceOptions{.check_names = false}));
}

}  // namespace operations_research::math_opt
