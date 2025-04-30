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

#include "ortools/math_opt/elemental/derived_data.h"

#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/elemental/arrays.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/attributes.h"
#include "ortools/math_opt/elemental/elements.h"
#include "ortools/math_opt/elemental/symmetry.h"
#include "ortools/math_opt/testing/stream.h"

namespace operations_research::math_opt {
namespace {

using ::testing::ElementsAreArray;
using ::testing::IsSupersetOf;

TEST(GetAttrDefaultValueTest, HasRightDefault) {
  EXPECT_EQ(GetAttrDefaultValue<DoubleAttr0::kObjOffset>(), 0.0);
  EXPECT_EQ(GetAttrDefaultValue<BoolAttr1::kVarInteger>(), false);
  EXPECT_EQ(GetAttrDefaultValue<DoubleAttr1::kVarUb>(),
            std::numeric_limits<double>::infinity());
  EXPECT_EQ(GetAttrDefaultValue<DoubleAttr2::kLinConCoef>(), 0.0);
}

TEST(AttrKeyForTest, Works) {
  EXPECT_TRUE((std::is_same_v<AttrKeyFor<BoolAttr0>, AttrKey<0>>));
  EXPECT_TRUE((std::is_same_v<AttrKeyFor<DoubleAttr0>, AttrKey<0>>));
  EXPECT_TRUE((std::is_same_v<AttrKeyFor<DoubleAttr1>, AttrKey<1>>));
  EXPECT_TRUE((std::is_same_v<AttrKeyFor<DoubleAttr2>, AttrKey<2>>));
  EXPECT_TRUE((std::is_same_v<AttrKeyFor<SymmetricDoubleAttr2>,
                              AttrKey<2, ElementSymmetry<0, 1>>>));
}

TEST(ValueTypeForTest, Works) {
  EXPECT_TRUE((std::is_same_v<ValueTypeFor<BoolAttr0>, bool>));
  EXPECT_TRUE((std::is_same_v<ValueTypeFor<DoubleAttr0>, double>));
  EXPECT_TRUE((std::is_same_v<ValueTypeFor<DoubleAttr1>, double>));
  EXPECT_TRUE((std::is_same_v<ValueTypeFor<DoubleAttr2>, double>));
}

TEST(GetAttrKeySizeTest, IsRightSize) {
  EXPECT_EQ(GetAttrKeySize<DoubleAttr0>(), 0);
  EXPECT_EQ(GetAttrKeySize<BoolAttr1>(), 1);
  EXPECT_EQ(GetAttrKeySize<DoubleAttr2>(), 2);
  EXPECT_EQ(GetAttrKeySize<DoubleAttr0::kObjOffset>(), 0);
  EXPECT_EQ(GetAttrKeySize<BoolAttr1::kVarInteger>(), 1);
  EXPECT_EQ(GetAttrKeySize<DoubleAttr2::kLinConCoef>(), 2);
}

TEST(GetElementTypesTest, Attr1HasElement) {
  EXPECT_EQ(GetElementTypes<BoolAttr1::kVarInteger>()[0],
            ElementType::kVariable);
}

TEST(GetElementTypesTest, Attr2HasElements) {
  EXPECT_EQ(GetElementTypes<DoubleAttr2::kLinConCoef>()[0],
            ElementType::kLinearConstraint);
  EXPECT_EQ(GetElementTypes<DoubleAttr2::kLinConCoef>()[1],
            ElementType::kVariable);
}

TEST(AllAttrsTest, Indexing) {
  ForEachIndex<AllAttrs::kNumAttrTypes>(
      // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
      []<int i>() { EXPECT_EQ((AllAttrs::GetIndex<AllAttrs::Type<i>>()), i); });
}

TEST(AllAttrsTest, ForEachAttribute) {
  std::vector<std::string> invoked;
  AllAttrs::ForEachAttr(
      [&invoked](auto attr) { invoked.push_back(StreamToString(attr)); });
  EXPECT_THAT(invoked, IsSupersetOf({"objective_offset", "maximize",
                                     "variable_integer", "variable_lower_bound",
                                     "linear_constraint_coefficient"}));
}

template <int i>
struct Value {
  Value() = default;
  explicit Value(int v) : value(v) {}

  int value = i;
};

TEST(AttrMapTest, GetSet) {
  AttrMap<Value> attr_map;

  constexpr int kBoolAttr0Index = AllAttrs::GetIndex<BoolAttr0>();
  constexpr int kBoolAttr1Index = AllAttrs::GetIndex<BoolAttr1>();
  constexpr int kDoubleAttr1Index = AllAttrs::GetIndex<DoubleAttr1>();
  constexpr int kDoubleAttr2Index = AllAttrs::GetIndex<DoubleAttr2>();

  // Default initialization.
  EXPECT_EQ(attr_map[BoolAttr0::kMaximize].value, kBoolAttr0Index);
  EXPECT_EQ(attr_map[BoolAttr1::kVarInteger].value, kBoolAttr1Index);
  EXPECT_EQ(attr_map[DoubleAttr1::kVarLb].value, kDoubleAttr1Index);
  EXPECT_EQ(attr_map[DoubleAttr1::kVarUb].value, kDoubleAttr1Index);
  EXPECT_EQ(attr_map[DoubleAttr2::kLinConCoef].value, kDoubleAttr2Index);

  // Mutation (typed).
  attr_map[BoolAttr0::kMaximize] = Value<kBoolAttr0Index>(42);
  attr_map[BoolAttr1::kVarInteger] = Value<kBoolAttr1Index>(43);
  attr_map[DoubleAttr1::kVarLb] = Value<kDoubleAttr1Index>(44);
  attr_map[DoubleAttr1::kVarUb] = Value<kDoubleAttr1Index>(45);
  attr_map[DoubleAttr2::kLinConCoef] = Value<kDoubleAttr2Index>(46);
  EXPECT_EQ(attr_map[BoolAttr0::kMaximize].value, 42);
  EXPECT_EQ(attr_map[BoolAttr1::kVarInteger].value, 43);
  EXPECT_EQ(attr_map[DoubleAttr1::kVarLb].value, 44);
  EXPECT_EQ(attr_map[DoubleAttr1::kVarUb].value, 45);
  EXPECT_EQ(attr_map[DoubleAttr2::kLinConCoef].value, 46);
}

TEST(AttrMapTest, Iteration) {
  AttrMap<Value> attr_map;

  // Collect all values in the default-initialized map.s
  std::vector<int> values;
  attr_map.ForEachAttrValue(
      [&values](const auto& v) { values.emplace_back(v.value); });

  // We should have `NumAttrs()` values `i` per attribute.
  std::vector<int> expected_values;
  // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
  ForEachIndex<AllAttrs::kNumAttrTypes>([&expected_values]<int i>() {
    for (int k = 0; k < AllAttrs::TypeDescriptor<i>::NumAttrs(); ++k) {
      expected_values.push_back(i);
    }
  });
  EXPECT_THAT(values, ElementsAreArray(expected_values));
}

TEST(CallForAttrTest, Works) {
  EXPECT_EQ(CallForAttr(DoubleAttr1::kVarUb,
                        // NOLINTNEXTLINE(clang-diagnostic-pre-c++20-compat)
                        []<DoubleAttr1 a>() { return static_cast<int>(a); }),
            static_cast<int>(DoubleAttr1::kVarUb));
}

TEST(FormatAttrValueTest, FormatsBool) {
  EXPECT_EQ(FormatAttrValue(true), "true");
}

TEST(FormatAttrValueTest, FormatsInt64) {
  EXPECT_EQ(FormatAttrValue(int64_t{12}), "12");
}

TEST(FormatAttrValueTest, FormatsDouble) {
  // need a double with an exact binary representation
  EXPECT_EQ(FormatAttrValue(4.5), "4.5");
}

}  // namespace
}  // namespace operations_research::math_opt
