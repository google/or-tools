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

#include "ortools/math_opt/constraints/second_order_cone/storage.h"

#include <vector>

#include "absl/container/flat_hash_map.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/linear_expression_data.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"

namespace operations_research::math_opt {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::EquivToProto;
using ::testing::Field;
using ::testing::Matcher;
using ::testing::Property;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

SecondOrderConeConstraintProto SimpleProto() {
  SecondOrderConeConstraintProto proto;
  proto.set_name("soc");
  {
    auto& expr = *proto.mutable_upper_bound();
    expr.add_ids(0);
    expr.add_coefficients(1.0);
  }
  {
    auto& expr = *proto.add_arguments_to_norm();
    expr.add_ids(3);
    expr.add_ids(6);
    expr.add_coefficients(2.0);
    expr.add_coefficients(3.0);
    expr.set_offset(4.0);
  }
  return proto;
}

SecondOrderConeConstraintData SimpleData() {
  return {
      .upper_bound = {.coeffs = SparseCoefficientMap({{VariableId(0), 1.0}}),
                      .offset = 0.0},
      .arguments_to_norm = {{.coeffs = SparseCoefficientMap(
                                 {{VariableId(3), 2.0}, {VariableId(6), 3.0}}),
                             .offset = 4.0}},
      .name = "soc"};
}

Matcher<LinearExpressionData> LinearExprEquals(
    const LinearExpressionData& other) {
  return AllOf(
      Field("offset", &LinearExpressionData::offset, Eq(other.offset)),
      Field("coeffs", &LinearExpressionData::coeffs,
            Property("terms", &SparseCoefficientMap::terms,
                     UnorderedElementsAreArray(other.coeffs.terms()))));
}

TEST(SecondOrderConeStorageDataTest, RelatedVariables) {
  EXPECT_THAT(
      SimpleData().RelatedVariables(),
      UnorderedElementsAre(VariableId(0), VariableId(3), VariableId(6)));
}

TEST(SecondOrderConeStorageDataTest, DeleteVariable) {
  SecondOrderConeConstraintData data = SimpleData();
  data.DeleteVariable(VariableId(3));
  EXPECT_THAT(
      data.upper_bound,
      LinearExprEquals({.coeffs = SparseCoefficientMap({{VariableId(0), 1.0}}),
                        .offset = 0.0}));
  EXPECT_THAT(data.arguments_to_norm,
              ElementsAre(LinearExprEquals(
                  {.coeffs = SparseCoefficientMap({{VariableId(6), 3.0}}),
                   .offset = 4.0})));

  data.DeleteVariable(VariableId(0));
  EXPECT_THAT(data.upper_bound,
              LinearExprEquals({.coeffs = {}, .offset = 0.0}));
  EXPECT_THAT(data.arguments_to_norm,
              ElementsAre(LinearExprEquals(
                  {.coeffs = SparseCoefficientMap({{VariableId(6), 3.0}}),
                   .offset = 4.0})));

  data.DeleteVariable(VariableId(6));
  EXPECT_THAT(data.upper_bound,
              LinearExprEquals({.coeffs = {}, .offset = 0.0}));
  EXPECT_THAT(data.arguments_to_norm,
              ElementsAre(LinearExprEquals({.coeffs = {}, .offset = 4.0})));
}

TEST(SecondOrderConeStorageDataTest, FromProto) {
  const auto data = SecondOrderConeConstraintData::FromProto(SimpleProto());
  EXPECT_EQ(data.name, "soc");
  EXPECT_THAT(
      data.upper_bound,
      LinearExprEquals({.coeffs = SparseCoefficientMap({{VariableId(0), 1.0}}),
                        .offset = 0.0}));
  EXPECT_THAT(data.arguments_to_norm,
              ElementsAre(LinearExprEquals(
                  {.coeffs = SparseCoefficientMap(
                       {{VariableId(3), 2.0}, {VariableId(6), 3.0}}),
                   .offset = 4.0})));
}

TEST(SosStorageDataTest, Proto) {
  EXPECT_THAT(SimpleData().Proto(), EquivToProto(SimpleProto()));
}

}  // namespace
}  // namespace operations_research::math_opt
