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

#include "ortools/math_opt/constraints/indicator/storage.h"

#include <optional>
#include <string>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"

namespace operations_research::math_opt {
namespace {

using ::testing::EquivToProto;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

IndicatorConstraintProto SimpleProto() {
  IndicatorConstraintProto proto;
  proto.set_lower_bound(-1.0);
  proto.set_upper_bound(1.0);
  proto.set_name("indicator");
  proto.set_indicator_id(2);
  proto.set_activate_on_zero(true);
  proto.mutable_expression()->add_ids(1);
  proto.mutable_expression()->add_values(2.0);
  proto.mutable_expression()->add_ids(3);
  proto.mutable_expression()->add_values(4.0);
  proto.mutable_expression()->add_ids(5);
  proto.mutable_expression()->add_values(6.0);
  return proto;
}

IndicatorConstraintData SimpleData() {
  IndicatorConstraintData data;
  data.lower_bound = -1.0;
  data.upper_bound = 1.0;
  data.name = "indicator";
  data.indicator = VariableId(2);
  data.activate_on_zero = true;
  data.linear_terms.set(VariableId(1), 2.0);
  data.linear_terms.set(VariableId(3), 4.0);
  data.linear_terms.set(VariableId(5), 6.0);
  return data;
}

TEST(QuadraticStorageDataTest, RelatedVariables) {
  EXPECT_THAT(SimpleData().RelatedVariables(),
              UnorderedElementsAre(VariableId(1), VariableId(2), VariableId(3),
                                   VariableId(5)));
}

TEST(IndicatorConstraintDataTest, DeleteVariable) {
  IndicatorConstraintData data = SimpleData();
  data.DeleteVariable(VariableId(1));
  EXPECT_THAT(data.indicator, Optional(VariableId(2)));
  EXPECT_THAT(
      data.linear_terms.terms(),
      UnorderedElementsAre(Pair(VariableId(3), 4.0), Pair(VariableId(5), 6.0)));

  data.DeleteVariable(VariableId(2));
  EXPECT_EQ(data.indicator, std::nullopt);
  EXPECT_THAT(
      data.linear_terms.terms(),
      UnorderedElementsAre(Pair(VariableId(3), 4.0), Pair(VariableId(5), 6.0)));
}

TEST(IndicatorConstraintDataTest, FromProto) {
  const auto data = IndicatorConstraintData::FromProto(SimpleProto());
  EXPECT_EQ(data.lower_bound, -1.0);
  EXPECT_EQ(data.upper_bound, 1.0);
  EXPECT_EQ(data.name, "indicator");
  EXPECT_THAT(data.indicator, Optional(VariableId(2)));
  EXPECT_TRUE(data.activate_on_zero);
  EXPECT_THAT(
      data.linear_terms.terms(),
      UnorderedElementsAre(Pair(VariableId(1), 2.0), Pair(VariableId(3), 4.0),
                           Pair(VariableId(5), 6.0)));
}

TEST(IndicatorConstraintDataTest, FromProtoUnsetIndicator) {
  IndicatorConstraintProto proto = SimpleProto();
  proto.clear_indicator_id();
  const auto data = IndicatorConstraintData::FromProto(proto);
  EXPECT_EQ(data.lower_bound, -1.0);
  EXPECT_EQ(data.upper_bound, 1.0);
  EXPECT_EQ(data.name, "indicator");
  EXPECT_EQ(data.indicator, std::nullopt);
  EXPECT_TRUE(data.activate_on_zero);
  EXPECT_THAT(
      data.linear_terms.terms(),
      UnorderedElementsAre(Pair(VariableId(1), 2.0), Pair(VariableId(3), 4.0),
                           Pair(VariableId(5), 6.0)));
}

TEST(IndicatorConstraintDataTest, Proto) {
  EXPECT_THAT(SimpleData().Proto(), EquivToProto(SimpleProto()));
}

TEST(IndicatorConstraintDataTest, ProtoUnsetIndicator) {
  IndicatorConstraintData data = SimpleData();
  data.indicator.reset();
  IndicatorConstraintProto expected = SimpleProto();
  expected.clear_indicator_id();
  EXPECT_THAT(data.Proto(), EquivToProto(expected));
}

}  // namespace
}  // namespace operations_research::math_opt
