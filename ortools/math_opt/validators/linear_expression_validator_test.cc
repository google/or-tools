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

#include "ortools/math_opt/validators/linear_expression_validator.h"

#include <stdint.h>

#include <initializer_list>
#include <string>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research::math_opt {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::status::StatusIs;

IdNameBiMap SimpleVariableUniverse(std::initializer_list<int64_t> ids) {
  IdNameBiMap universe;
  CHECK_OK(universe.BulkUpdate({}, ids, {}));
  return universe;
}

TEST(LinearExpressionValidatorTest, EmptyExpressionOk) {
  EXPECT_OK(ValidateLinearExpression(LinearExpressionProto(), IdNameBiMap()));
}

TEST(LinearExpressionValidatorTest, SimpleExpressionOk) {
  const LinearExpressionProto expression = ParseTestProto(R"pb(
    ids: [ 1 ]
    coefficients: [ 3.0 ]
    offset: 4.0
  )pb");
  IdNameBiMap variable_universe;
  EXPECT_OK(ValidateLinearExpression(expression, SimpleVariableUniverse({1})));
}

// We rely on CheckIdsAndValues for validating the ids and coefficients fields.
// We test its integration once; here IDs are not in strictly increasing order.
TEST(LinearExpressionValidatorTest, CheckIdsAndValuesPropagates) {
  const LinearExpressionProto expression = ParseTestProto(R"pb(
    ids: [ 2, 1 ]
    coefficients: [ 3.0, 2.0 ]
    offset: 4.0
  )pb");
  EXPECT_THAT(
      ValidateLinearExpression(expression, SimpleVariableUniverse({1, 2})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("ids to be strictly increasing")));
}

TEST(LinearExpressionValidatorTest, TermsPositiveInfCoefficient) {
  const LinearExpressionProto expression = ParseTestProto(R"pb(
    ids: [ 2 ]
    coefficients: [ inf ]
    offset: 4.0
  )pb");
  EXPECT_THAT(ValidateLinearExpression(expression, SimpleVariableUniverse({2})),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("Invalid positive infinite value"),
                             HasSubstr("invalid linear expression terms"))));
}

TEST(LinearExpressionValidatorTest, TermsNegativeCoefficient) {
  const LinearExpressionProto expression = ParseTestProto(R"pb(
    ids: [ 2 ]
    coefficients: [ -inf ]
    offset: 4.0
  )pb");
  EXPECT_THAT(ValidateLinearExpression(expression, SimpleVariableUniverse({2})),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("Invalid negative infinite value"),
                             HasSubstr("invalid linear expression terms"))));
}

TEST(LinearExpressionValidatorTest, TermsInvalidId) {
  const LinearExpressionProto expression = ParseTestProto(R"pb(
    ids: [ 2 ]
    coefficients: [ 3.0 ]
    offset: 4.0
  )pb");
  EXPECT_THAT(
      ValidateLinearExpression(expression, SimpleVariableUniverse({1, 3})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("invalid variable id: 2")));
}

TEST(LinearExpressionValidatorTest, TermsInfOffset) {
  const LinearExpressionProto expression = ParseTestProto(R"pb(
    offset: inf
  )pb");
  EXPECT_THAT(ValidateLinearExpression(expression, SimpleVariableUniverse({2})),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("found value: inf"),
                             HasSubstr("invalid linear expression offset"))));
}

TEST(LinearExpressionValidatorTest, TermsNanOffset) {
  const LinearExpressionProto expression = ParseTestProto(R"pb(
    offset: nan
  )pb");
  EXPECT_THAT(ValidateLinearExpression(expression, SimpleVariableUniverse({2})),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("found value: nan"),
                             HasSubstr("invalid linear expression offset"))));
}

}  // namespace
}  // namespace operations_research::math_opt
