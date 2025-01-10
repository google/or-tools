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

#include "ortools/math_opt/constraints/sos/storage.h"

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

// We test with SOS1; there is no difference with SOS2 at the storage level.
using TestConstraintData = Sos1ConstraintData;

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::EquivToProto;
using ::testing::Field;
using ::testing::Matcher;
using ::testing::Property;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

SosConstraintProto SimpleProto(const bool with_weights = true) {
  SosConstraintProto proto;
  proto.set_name("q");
  if (with_weights) {
    proto.add_weights(2.0);
    proto.add_weights(3.0);
  }
  {
    auto& expr = *proto.add_expressions();
    expr.add_ids(0);
    expr.add_coefficients(1.0);
  }
  {
    auto& expr = *proto.add_expressions();
    expr.add_ids(3);
    expr.add_ids(6);
    expr.add_coefficients(2.0);
    expr.add_coefficients(3.0);
    expr.set_offset(4.0);
  }
  return proto;
}

TestConstraintData SimpleData(const bool with_weights = true) {
  std::vector<double> weights;
  if (with_weights) {
    weights.push_back(2.0);
    weights.push_back(3.0);
  }
  return TestConstraintData(
      {{.coeffs = SparseCoefficientMap({{VariableId(0), 1.0}}), .offset = 0.0},
       {.coeffs =
            SparseCoefficientMap({{VariableId(3), 2.0}, {VariableId(6), 3.0}}),
        .offset = 4.0}},
      weights, "q");
}

Matcher<LinearExpressionData> LinearExprEquals(
    const LinearExpressionData& other) {
  return AllOf(
      Field("offset", &LinearExpressionData::offset, Eq(other.offset)),
      Field("coeffs", &LinearExpressionData::coeffs,
            Property("terms", &SparseCoefficientMap::terms,
                     UnorderedElementsAreArray(other.coeffs.terms()))));
}

TEST(SosStorageDataTest, RelatedVariables) {
  EXPECT_THAT(
      SimpleData().RelatedVariables(),
      UnorderedElementsAre(VariableId(0), VariableId(3), VariableId(6)));
}

TEST(SosStorageDataTest, DeleteVariable) {
  TestConstraintData data = SimpleData();
  data.DeleteVariable(VariableId(3));
  ASSERT_THAT(data.num_expressions(), 2);
  EXPECT_EQ(data.weight(0), 2.0);
  EXPECT_THAT(
      data.expression(0),
      LinearExprEquals({.coeffs = SparseCoefficientMap({{VariableId(0), 1.0}}),
                        .offset = 0.0}));
  EXPECT_EQ(data.weight(1), 3.0);
  EXPECT_THAT(
      data.expression(1),
      LinearExprEquals({.coeffs = SparseCoefficientMap({{VariableId(6), 3.0}}),
                        .offset = 4.0}));

  data.DeleteVariable(VariableId(0));
  ASSERT_THAT(data.num_expressions(), 2);
  EXPECT_EQ(data.weight(0), 2.0);
  EXPECT_THAT(data.expression(0),
              LinearExprEquals(LinearExpressionData{.offset = 0.0}));
  EXPECT_EQ(data.weight(1), 3.0);
  EXPECT_THAT(
      data.expression(1),
      LinearExprEquals({.coeffs = SparseCoefficientMap({{VariableId(6), 3.0}}),
                        .offset = 4.0}));
  data.DeleteVariable(VariableId(6));
  ASSERT_THAT(data.num_expressions(), 2);
  EXPECT_EQ(data.weight(0), 2.0);
  EXPECT_THAT(data.expression(0),
              LinearExprEquals(LinearExpressionData{.offset = 0.0}));
  EXPECT_EQ(data.weight(1), 3.0);
  EXPECT_THAT(data.expression(1),
              LinearExprEquals(LinearExpressionData{.offset = 4.0}));
}

TEST(SosStorageDataTest, FromProto) {
  const auto data = TestConstraintData::FromProto(SimpleProto());
  EXPECT_EQ(data.name(), "q");
  ASSERT_THAT(data.num_expressions(), 2);
  EXPECT_EQ(data.weight(0), 2.0);
  EXPECT_THAT(
      data.expression(0),
      LinearExprEquals({.coeffs = SparseCoefficientMap({{VariableId(0), 1.0}}),
                        .offset = 0.0}));
  EXPECT_EQ(data.weight(1), 3.0);
  EXPECT_THAT(
      data.expression(1),
      LinearExprEquals({.coeffs = SparseCoefficientMap(
                            {{VariableId(3), 2.0}, {VariableId(6), 3.0}}),
                        .offset = 4.0}));
}

TEST(SosStorageDataTest, Proto) {
  EXPECT_THAT(SimpleData().Proto(), EquivToProto(SimpleProto()));
}

TEST(SosStorageDataTest, ProtoUnsetWeights) {
  EXPECT_THAT(SimpleData(/*with_weights=*/false).Proto(),
              EquivToProto(SimpleProto(/*with_weights=*/false)));
}

}  // namespace
}  // namespace operations_research::math_opt
