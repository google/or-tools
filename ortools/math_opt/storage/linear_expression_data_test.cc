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

#include "ortools/math_opt/storage/linear_expression_data.h"

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"

namespace operations_research::math_opt {
namespace {

using ::testing::EqualsProto;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(LinearExpressionDataTest, Empty) {
  LinearExpressionData data;
  EXPECT_EQ(data.offset, 0.0);
  EXPECT_THAT(data.coeffs.terms(), IsEmpty());
}

TEST(LinearExpressionDataTest, Proto) {
  LinearExpressionData data{.offset = 3.0};
  data.coeffs.set(VariableId(1), -1.0);
  data.coeffs.set(VariableId(4), 1.0);
  data.coeffs.set(VariableId(12), -4.0);
  LinearExpressionProto expected;
  expected.set_offset(3.0);
  expected.add_ids(1);
  expected.add_ids(4);
  expected.add_ids(12);
  expected.add_coefficients(-1.0);
  expected.add_coefficients(1.0);
  expected.add_coefficients(-4.0);
  EXPECT_THAT(data.Proto(), EqualsProto(expected));
}

TEST(LinearExpressionDataTest, FromProto) {
  LinearExpressionProto proto;
  proto.set_offset(3.0);
  proto.add_ids(1);
  proto.add_ids(4);
  proto.add_ids(12);
  proto.add_coefficients(-1.0);
  proto.add_coefficients(1.0);
  proto.add_coefficients(-4.0);

  const LinearExpressionData expr_data = LinearExpressionData::FromProto(proto);
  EXPECT_EQ(expr_data.offset, 3.0);
  EXPECT_THAT(
      expr_data.coeffs.terms(),
      UnorderedElementsAre(Pair(VariableId(1), -1.0), Pair(VariableId(4), 1.0),
                           Pair(VariableId(12), -4.0)));
}

}  // namespace
}  // namespace operations_research::math_opt
