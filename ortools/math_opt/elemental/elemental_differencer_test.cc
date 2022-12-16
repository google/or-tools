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

#include "ortools/math_opt/elemental/elemental_differencer.h"

#include "absl/container/flat_hash_set.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/attributes.h"
#include "ortools/math_opt/elemental/elemental.h"
#include "ortools/math_opt/elemental/elements.h"

namespace operations_research::math_opt {
namespace {

using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

TEST(SymmetricDifferenceTest, HasDifference) {
  const absl::flat_hash_set<int> s1 = {4, 2, 9};
  const absl::flat_hash_set<int> s2 = {2, 9, 3, 7};
  const SymmetricDifference<int> d(s1, s2);
  EXPECT_THAT(d.only_in_first, UnorderedElementsAre(4));
  EXPECT_THAT(d.only_in_second, UnorderedElementsAre(3, 7));
}

TEST(ComputeIntersectionTest, HasIntersection) {
  const absl::flat_hash_set<int> s1 = {4, 2, 9};
  const absl::flat_hash_set<int> s2 = {2, 9, 3, 7};
  EXPECT_THAT(IntersectSets(s1, s2), UnorderedElementsAre(2, 9));
}

TEST(ComputeModelDifference, HasIntersection) {
  const absl::flat_hash_set<int> s1 = {4, 2, 9};
  const absl::flat_hash_set<int> s2 = {2, 9, 3, 7};
  EXPECT_THAT(IntersectSets(s1, s2), UnorderedElementsAre(2, 9));
}

TEST(ElementalDifferenceTest, EmptyModelNoDifference) {
  Elemental e1;
  Elemental e2;
  auto diff = ElementalDifference::Create(e1, e2);
  EXPECT_TRUE(diff.Empty());
  EXPECT_EQ(ElementalDifference::Describe(e1, e2, diff), "No difference");
}

TEST(ElementalDifferenceTest, SameNamesNoDifference) {
  Elemental e1("m1", "o1");
  Elemental e2("m1", "o1");
  auto diff = ElementalDifference::Create(e1, e2);
  EXPECT_TRUE(diff.Empty());
  EXPECT_FALSE(diff.model_name_different());
  EXPECT_FALSE(diff.primary_objective_name_different());
  EXPECT_EQ(ElementalDifference::Describe(e1, e2, diff), "No difference");
}

TEST(ElementalDifferenceTest, DifferentModelNames) {
  Elemental e1("m1");
  Elemental e2("m2");
  // Test with checking names
  {
    auto diff = ElementalDifference::Create(e1, e2);
    EXPECT_FALSE(diff.Empty());
    EXPECT_TRUE(diff.model_name_different());
    EXPECT_EQ(ElementalDifference::Describe(e1, e2, diff),
              R"(model name disagrees:
  first_name: "m1"
  second_name: "m2")");
  }
  // Test without checking names
  auto diff = ElementalDifference::Create(e1, e2, {.check_names = false});
  EXPECT_TRUE(diff.Empty());
  EXPECT_FALSE(diff.model_name_different());
  EXPECT_EQ(ElementalDifference::Describe(e1, e2, diff), "No difference");
}

TEST(ElementalDifferenceTest, DifferentPrimaryObjectiveNames) {
  Elemental e1("", "o1");
  Elemental e2("", "o2");
  // Test with checking names
  {
    auto diff = ElementalDifference::Create(e1, e2);
    EXPECT_FALSE(diff.Empty());
    EXPECT_TRUE(diff.primary_objective_name_different());
    EXPECT_EQ(ElementalDifference::Describe(e1, e2, diff),
              R"(primary objective name disagrees:
  first_name: "o1"
  second_name: "o2")");
  }
  // Test without checking names
  auto diff = ElementalDifference::Create(e1, e2, {.check_names = false});
  EXPECT_TRUE(diff.Empty());
  EXPECT_FALSE(diff.primary_objective_name_different());
  EXPECT_EQ(ElementalDifference::Describe(e1, e2, diff), "No difference");
}

TEST(ElementalDifferenceTest, SameElementsNoDifference) {
  Elemental e1;
  e1.AddElement<ElementType::kVariable>("x");
  Elemental e2;
  e2.AddElement<ElementType::kVariable>("x");
  auto diff = ElementalDifference::Create(e1, e2);
  EXPECT_TRUE(diff.Empty());
  EXPECT_EQ(ElementalDifference::Describe(e1, e2, diff), "No difference");
}

TEST(ElementalDifferenceTest, SameElementsWithUpdateTrackerStillNoDifference) {
  Elemental e1;
  e1.AddElement<ElementType::kVariable>("x");
  Elemental e2;
  e2.AddDiff();
  e2.AddElement<ElementType::kVariable>("x");
  auto diff = ElementalDifference::Create(e1, e2);
  EXPECT_TRUE(diff.Empty());
  EXPECT_EQ(ElementalDifference::Describe(e1, e2, diff), "No difference");
}

TEST(ElementalDifferenceTest, DifferentElements) {
  Elemental e1;
  Elemental e2;
  e2.AddElement<ElementType::kVariable>("x");
  e2.AddElement<ElementType::kVariable>("y");
  auto diff = ElementalDifference::Create(e1, e2);
  EXPECT_FALSE(diff.Empty());
  EXPECT_THAT(
      diff.element_difference(ElementType::kVariable).ids.only_in_second,
      UnorderedElementsAre(0, 1));
  EXPECT_EQ(ElementalDifference::Describe(e1, e2, diff),
            R"(variable:
  element ids in second but not first:
    0: (name: "x")
    1: (name: "y")
  next_id does not agree:
    first: 0
    second: 2)");
  EXPECT_THAT(ElementalDifference::DescribeDifference(e2, e1), R"(variable:
  element ids in first but not second:
    0: (name: "x")
    1: (name: "y")
  next_id does not agree:
    first: 2
    second: 0)");
}

TEST(ElementalDifferenceTest, DifferentElementNames) {
  Elemental e1;
  e1.AddElement<ElementType::kVariable>("x");
  Elemental e2;
  e2.AddElement<ElementType::kVariable>("y");
  // Test with checking names
  {
    auto diff = ElementalDifference::Create(e1, e2);
    EXPECT_FALSE(diff.Empty());
    EXPECT_THAT(diff.element_difference(ElementType::kVariable).different_names,
                UnorderedElementsAre(0));
    EXPECT_EQ(ElementalDifference::Describe(e1, e2, diff),
              R"(variable:
  element ids with disagreeing names:
    id: 0 first_name: "x" second_name: "y")");
  }
  // Test without checking names
  auto diff = ElementalDifference::Create(e1, e2, {.check_names = false});
  EXPECT_TRUE(diff.Empty());
  EXPECT_THAT(diff.element_difference(ElementType::kVariable).different_names,
              IsEmpty());
  EXPECT_EQ(ElementalDifference::Describe(e1, e2, diff), "No difference");
}

TEST(ElementalDifferenceTest, DifferentNextId) {
  Elemental e1;
  e1.EnsureNextElementIdAtLeastUntyped(ElementType::kVariable, 1);
  Elemental e2;
  // Test with checking next_id
  {
    auto diff = ElementalDifference::Create(e1, e2);
    EXPECT_FALSE(diff.Empty());
    EXPECT_TRUE(
        diff.element_difference(ElementType::kVariable).next_id_different);
    EXPECT_EQ(ElementalDifference::Describe(e1, e2, diff),
              R"(variable:
  next_id does not agree:
    first: 1
    second: 0)");
  }
  // Test without checking names
  auto diff = ElementalDifference::Create(e1, e2, {.check_next_id = false});
  EXPECT_TRUE(diff.Empty());
  EXPECT_FALSE(
      diff.element_difference(ElementType::kVariable).next_id_different);
  EXPECT_EQ(ElementalDifference::Describe(e1, e2, diff), "No difference");
}

TEST(ElementalDifferenceTest, SameAttributeValues) {
  Elemental e1;
  e1.SetAttr(DoubleAttr0::kObjOffset, {}, 4.0);
  e1.AddElement<ElementType::kVariable>("x");
  e1.SetAttr(DoubleAttr1::kVarUb, AttrKey(0), 5.0);
  Elemental e2;
  e2.SetAttr(DoubleAttr0::kObjOffset, {}, 4.0);
  e2.AddElement<ElementType::kVariable>("x");
  e2.SetAttr(DoubleAttr1::kVarUb, AttrKey(0), 5.0);
  auto diff = ElementalDifference::Create(e1, e2);
  EXPECT_TRUE(diff.Empty());
  EXPECT_TRUE(diff.attr_difference(DoubleAttr0::kObjOffset).Empty());
  EXPECT_TRUE(diff.attr_difference(DoubleAttr1::kVarUb).Empty());
  EXPECT_EQ(ElementalDifference::Describe(e1, e2, diff), "No difference");
}

TEST(ElementalDifferenceTest, Attr0NonDefaultDifferentValue) {
  Elemental e1;
  e1.SetAttr(DoubleAttr0::kObjOffset, {}, 4.0);
  Elemental e2;
  e2.SetAttr(DoubleAttr0::kObjOffset, {}, 5.0);
  auto diff = ElementalDifference::Create(e1, e2);
  EXPECT_FALSE(diff.Empty());
  EXPECT_THAT(diff.attr_difference(DoubleAttr0::kObjOffset).different_values,
              UnorderedElementsAre(AttrKey()));
  EXPECT_EQ(ElementalDifference::Describe(e1, e2, diff),
            R"(For attribute objective_offset errors on the following keys:
  key: AttrKey() (name in first: ()) value in first: 4 (name in second: ()) value in second: 5)");
}

TEST(ElementalDifferenceTest, Attr0OnlySetOnce) {
  Elemental e1;
  Elemental e2;
  e2.SetAttr(DoubleAttr0::kObjOffset, {}, 5.0);
  auto diff = ElementalDifference::Create(e1, e2);
  EXPECT_FALSE(diff.Empty());
  EXPECT_THAT(diff.attr_difference(DoubleAttr0::kObjOffset).different_values,
              IsEmpty());
  EXPECT_THAT(diff.attr_difference(DoubleAttr0::kObjOffset).keys.only_in_first,
              IsEmpty());
  EXPECT_THAT(diff.attr_difference(DoubleAttr0::kObjOffset).keys.only_in_second,
              UnorderedElementsAre(AttrKey()));
  EXPECT_EQ(ElementalDifference::Describe(e1, e2, diff),
            R"(For attribute objective_offset errors on the following keys:
  key: AttrKey() (name in first: ()) value in first: 0 (name in second: ()) value in second: 5)");
}

TEST(ElementalDifferenceTest, Attr1NonDefaultDifferentValue) {
  Elemental e1;
  e1.AddElement<ElementType::kVariable>("x");
  e1.SetAttr(DoubleAttr1::kVarUb, AttrKey(0), 4.0);
  Elemental e2;
  // Make name is different on purpose, ensure we print it correctly.
  e2.AddElement<ElementType::kVariable>("y");
  e2.SetAttr(DoubleAttr1::kVarUb, AttrKey(0), 5.0);

  auto diff = ElementalDifference::Create(e1, e2, {.check_names = false});
  EXPECT_FALSE(diff.Empty());
  EXPECT_THAT(diff.attr_difference(DoubleAttr1::kVarUb).different_values,
              UnorderedElementsAre(AttrKey(0)));
  EXPECT_EQ(ElementalDifference::Describe(e1, e2, diff),
            R"(For attribute variable_upper_bound errors on the following keys:
  key: AttrKey(0) (name in first: ("x")) value in first: 4 (name in second: ("y")) value in second: 5)");
}

TEST(ElementalDifferenceTest, Attr1OnlySetOnce) {
  Elemental e1;
  Elemental e2;
  e2.AddElement<ElementType::kVariable>("x");
  e2.SetAttr(DoubleAttr1::kVarUb, AttrKey(0), 5.0);

  {
    auto diff = ElementalDifference::Create(e1, e2);
    EXPECT_FALSE(diff.Empty());
    EXPECT_THAT(diff.attr_difference(DoubleAttr1::kVarUb).different_values,
                IsEmpty());
    EXPECT_THAT(diff.attr_difference(DoubleAttr1::kVarUb).keys.only_in_first,
                IsEmpty());
    EXPECT_THAT(diff.attr_difference(DoubleAttr1::kVarUb).keys.only_in_second,
                UnorderedElementsAre(AttrKey(0)));
    EXPECT_THAT(
        ElementalDifference::Describe(e1, e2, diff),
        HasSubstr(
            R"(For attribute variable_upper_bound errors on the following keys:
  key: AttrKey(0) (name in first: (__missing__)) value in first: __missing__ (name in second: ("x")) value in second: 5)"));
  }
  // Reverse the order of arguments
  {
    auto diff = ElementalDifference::Create(e2, e1);
    EXPECT_FALSE(diff.Empty());
    EXPECT_THAT(diff.attr_difference(DoubleAttr1::kVarUb).different_values,
                IsEmpty());
    EXPECT_THAT(diff.attr_difference(DoubleAttr1::kVarUb).keys.only_in_second,
                IsEmpty());
    EXPECT_THAT(diff.attr_difference(DoubleAttr1::kVarUb).keys.only_in_first,
                UnorderedElementsAre(AttrKey(0)));
  }
}

}  // namespace
}  // namespace operations_research::math_opt
