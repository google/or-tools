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

#include "ortools/math_opt/elemental/elements.h"

#include <array>
#include <cstdint>
#include <utility>

#include "absl/hash/hash_testing.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/testing/stream.h"

namespace operations_research::math_opt {
namespace {

using ::testing::ElementsAre;

TEST(ToStringTests, EachTypeCanConvert) {
  EXPECT_EQ(ToString(ElementType::kVariable), "variable");
  // Now check that absl::Stringify wraps ToString()
  EXPECT_EQ(absl::StrCat(ElementType::kVariable), "variable");
  // Now check that << wraps ToString()
  EXPECT_EQ(StreamToString(ElementType::kVariable), "variable");
}

TEST(ElementIdTest, Valid) {
  VariableId var(42);
  EXPECT_TRUE(var.IsValid());
  EXPECT_EQ(var.value(), 42);
  EXPECT_EQ(var.tag_value(), ElementType::kVariable);
  EXPECT_EQ(absl::StrCat(var), "variable{42}");
  EXPECT_EQ(StreamToString(var), "variable{42}");
}

TEST(ElementIdTest, Invalid) {
  VariableId var;
  EXPECT_FALSE(var.IsValid());
  EXPECT_EQ(var.tag_value(), ElementType::kVariable);
  EXPECT_EQ(absl::StrCat(var), "variable{invalid}");
}

TEST(ElementIdTest, Hashing) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      VariableId(1),
      VariableId(2),
      VariableId(),
  }));
}

TEST(ElementIdsVectorTest, Works) {
  ElementIdsVector<ElementType::kVariable> ids({1, 2, 3});
  EXPECT_EQ(ids.size(), 3);
  EXPECT_EQ(ids[0], VariableId(1));
  EXPECT_EQ(ids[1], VariableId(2));
  EXPECT_EQ(ids[2], VariableId(3));
  EXPECT_THAT(ids, ElementsAre(VariableId(1), VariableId(2), VariableId(3)));

  // Test move ctor.
  ElementIdsVector<ElementType::kVariable> ids2 = std::move(ids);
  EXPECT_THAT(ids2, ElementsAre(VariableId(1), VariableId(2), VariableId(3)));
}

TEST(ElementIdsSpanTest, Works) {
  std::array<int64_t, 3> values = {1, 2, 3};
  ElementIdsConstView<ElementType::kVariable, std::array<int64_t, 3>> ids(
      &values);
  EXPECT_EQ(ids.size(), 3);
  EXPECT_EQ(ids[0], VariableId(1));
  EXPECT_EQ(ids[1], VariableId(2));
  EXPECT_EQ(ids[2], VariableId(3));
  EXPECT_THAT(ids, ElementsAre(VariableId(1), VariableId(2), VariableId(3)));
}
}  // namespace

}  // namespace operations_research::math_opt
