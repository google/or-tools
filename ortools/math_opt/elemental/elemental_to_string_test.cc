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

#include <string>

#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/attributes.h"
#include "ortools/math_opt/elemental/elemental.h"
#include "ortools/math_opt/elemental/elements.h"
#include "ortools/math_opt/testing/stream.h"

namespace operations_research::math_opt {
namespace {

using ::testing::HasSubstr;

TEST(ElementalDebugStringTest, EmptyModel) {
  Elemental elemental;
  const std::string expected = "Model:";
  EXPECT_EQ(elemental.DebugString(/*print_diffs=*/true), expected);
  EXPECT_EQ(elemental.DebugString(/*print_diffs=*/false), expected);
  EXPECT_EQ(absl::StrCat(elemental), expected);    // Tests absl::Stringify.
  EXPECT_EQ(StreamToString(elemental), expected);  // Tests << operator.
}

TEST(ElementalDebugStringTest, NamedModel) {
  Elemental elemental("mod_name", "obj_name");
  const std::string expected =
      "Model:\nmodel_name: \"mod_name\"\nprimary_objective_name: \"obj_name\"";
  EXPECT_EQ(elemental.DebugString(/*print_diffs=*/true), expected);
  EXPECT_EQ(elemental.DebugString(/*print_diffs=*/false), expected);
}

TEST(ElementalDebugStringTest, HasElements) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("x");
  elemental.AddElement<ElementType::kVariable>("");
  std::string expected =
      "Model:\nElementType: variable num_elements: 2 next_id: 2\n  id: 0 name: "
      "\"x\"\n  id: 1 name: \"\"";
  EXPECT_EQ(elemental.DebugString(/*print_diffs=*/true), expected);
  EXPECT_EQ(elemental.DebugString(/*print_diffs=*/false), expected);
}

TEST(ElementalDebugStringTest, HasAttr0) {
  Elemental elemental;
  elemental.SetAttr(DoubleAttr0::kObjOffset, {}, 12.5);
  std::string expected =
      "Model:\nAttribute: objective_offset non-defaults: 1\n  key: AttrKey() "
      "value: 12.5 (key names: ())";
  EXPECT_EQ(elemental.DebugString(/*print_diffs=*/true), expected);
  EXPECT_EQ(elemental.DebugString(/*print_diffs=*/false), expected);
}

TEST(ElementalDebugStringTest, HasAttr1) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("x");
  elemental.AddElement<ElementType::kVariable>("");
  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(0), 5.0);
  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(1), 3.0);
  std::string expected_substr =
      "Attribute: variable_upper_bound non-defaults: 2\n  key: AttrKey(0) "
      "value: 5 (key names: (\"x\"))\n  key: AttrKey(1) value: 3 (key "
      "names: (\"\"))";
  EXPECT_THAT(elemental.DebugString(/*print_diffs=*/true),
              HasSubstr(expected_substr));
  EXPECT_THAT(elemental.DebugString(/*print_diffs=*/false),
              HasSubstr(expected_substr));
}

TEST(ElementalDebugStringTest, HasAttr2) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("");
  elemental.AddElement<ElementType::kVariable>("x");
  elemental.AddElement<ElementType::kLinearConstraint>("c");
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(0, 1), 5.0);
  std::string expected_substr =
      "Attribute: linear_constraint_coefficient non-defaults: 1\n  key: "
      "AttrKey(0, 1) "
      "value: 5 (key names: (\"c\", \"x\"))";
  EXPECT_THAT(elemental.DebugString(/*print_diffs=*/true),
              HasSubstr(expected_substr));
  EXPECT_THAT(elemental.DebugString(/*print_diffs=*/false),
              HasSubstr(expected_substr));
}

TEST(ElementalDebugStringTest, EmptyDiffs) {
  Elemental elemental;
  elemental.AddDiff();
  elemental.AddDiff();
  EXPECT_EQ(elemental.DebugString(/*print_diffs=*/true),
            "Model:\nDiff: 0\nDiff: 1");
  EXPECT_EQ(elemental.DebugString(/*print_diffs=*/false), "Model:");
}

TEST(ElementalDebugStringTest, CheckpointLessThanVariablesInDiff) {
  Elemental elemental;
  elemental.AddDiff();
  elemental.AddElement<ElementType::kVariable>("x");
  std::string expected = R"(Model:
ElementType: variable num_elements: 1 next_id: 1
  id: 0 name: "x"
Diff: 0
ElementType: variable next_id: 1 checkpoint: 0)";
  EXPECT_EQ(elemental.DebugString(/*print_diffs=*/true), expected);
}

TEST(ElementalDebugStringTest, DeletedElementsInDiff) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  elemental.AddElement<ElementType::kVariable>("y");
  const VariableId z = elemental.AddElement<ElementType::kVariable>("z");
  elemental.AddDiff();
  elemental.DeleteElement(z);
  elemental.DeleteElement(x);
  std::string expected = R"(Model:
ElementType: variable num_elements: 1 next_id: 3
  id: 1 name: "y"
Diff: 0
ElementType: variable next_id: 3 checkpoint: 3
  deleted: [0, 2])";
  EXPECT_EQ(elemental.DebugString(/*print_diffs=*/true), expected);
}

TEST(ElementalDebugStringTest, ModifiedAttrInDiff) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("");
  elemental.AddElement<ElementType::kVariable>("y");
  elemental.AddDiff();
  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(0), 1.0);
  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(1), 1.0);
  std::string expected_substr = R"(Diff: 0
Attribute: variable_upper_bound
  AttrKey(0) (names: (""))
  AttrKey(1) (names: ("y")))";
  EXPECT_THAT(elemental.DebugString(/*print_diffs=*/true),
              HasSubstr(expected_substr));
}

}  // namespace
}  // namespace operations_research::math_opt
