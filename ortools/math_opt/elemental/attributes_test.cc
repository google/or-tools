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

#include "ortools/math_opt/elemental/attributes.h"

#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/math_opt/elemental/arrays.h"
#include "ortools/math_opt/testing/stream.h"

namespace operations_research::math_opt {
namespace {

TEST(ToStringTests, EachTypeCanConvert) {
  EXPECT_EQ(ToString(BoolAttr0::kMaximize), "maximize");
  EXPECT_EQ(ToString(BoolAttr1::kVarInteger), "variable_integer");
  EXPECT_EQ(ToString(IntAttr0::kObjPriority), "objective_priority");
  EXPECT_EQ(ToString(IntAttr1::kAuxObjPriority),
            "auxiliary_objective_priority");
  EXPECT_EQ(ToString(DoubleAttr0::kObjOffset), "objective_offset");
  EXPECT_EQ(ToString(DoubleAttr1::kVarLb), "variable_lower_bound");
  EXPECT_EQ(ToString(DoubleAttr2::kLinConCoef),
            "linear_constraint_coefficient");
  EXPECT_EQ(ToString(SymmetricDoubleAttr2::kObjQuadCoef),
            "objective_quadratic_coefficient");
  EXPECT_EQ(ToString(SymmetricDoubleAttr3::kQuadConQuadCoef),
            "quadratic_constraint_quadratic_coefficient");
  // Now check that absl::Stringify wraps ToString()
  EXPECT_EQ(absl::StrCat(BoolAttr0::kMaximize), "maximize");
  // Now check that << wraps ToString()
  EXPECT_EQ(StreamToString(BoolAttr0::kMaximize), "maximize");
}

// Validate that for all symmetric attribute types, the symmetry is consistent
// with element types.
TEST(SymmetryTest, AllSymmetricTypesAreCorrect) {
  ForEach(
      []<typename Descriptor>(const Descriptor&) {
        for (const auto& attr : Descriptor::kAttrDescriptors) {
          Descriptor::Symmetry::CheckElementTypes(attr.key_types);
        }
      },
      AllAttrTypeDescriptors{});
}

}  // namespace
}  // namespace operations_research::math_opt
