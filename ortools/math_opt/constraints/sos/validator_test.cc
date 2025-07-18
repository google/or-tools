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

#include "ortools/math_opt/constraints/sos/validator.h"

#include <cstdint>
#include <initializer_list>
#include <limits>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/logging.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research::math_opt {
namespace {

using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::status::StatusIs;

IdNameBiMap SimpleVariableUniverse(const std::initializer_list<int64_t> ids) {
  IdNameBiMap universe;
  CHECK_OK(universe.BulkUpdate({}, ids, {}));
  return universe;
}

SosConstraintProto SimpleSosConstraintProto() {
  SosConstraintProto data;
  LinearExpressionProto& expr = *data.add_expressions();
  expr.add_ids(1);
  expr.add_coefficients(3.0);
  expr.set_offset(4.0);
  data.add_weights(2.0);
  return data;
}

TEST(SosDataValidatorTest, EmptyConstraintOk) {
  EXPECT_OK(
      ValidateConstraint(SosConstraintProto(), SimpleVariableUniverse({})));
}

TEST(SosConstraintValidatorTest, SimpleConstraintOk) {
  EXPECT_OK(ValidateConstraint(SimpleSosConstraintProto(),
                               SimpleVariableUniverse({1})));
}

TEST(SosConstraintValidatorTest, WeightsExpressionSizeMismatch) {
  SosConstraintProto data = SimpleSosConstraintProto();
  data.add_weights(3.0);
  EXPECT_THAT(
      ValidateConstraint(data, SimpleVariableUniverse({1})),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("mismatch")));
}

TEST(SosConstraintValidatorTest, InvalidLinearExpression) {
  SosConstraintProto data = SimpleSosConstraintProto();
  data.mutable_expressions(0)->add_ids(2);
  EXPECT_THAT(ValidateConstraint(data, SimpleVariableUniverse({1})),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("linear expression")));
}

TEST(SosConstraintValidatorTest, InfWeight) {
  SosConstraintProto data = SimpleSosConstraintProto();
  data.set_weights(0, std::numeric_limits<double>::infinity());
  EXPECT_THAT(ValidateConstraint(data, SimpleVariableUniverse({1})),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("value: inf"),
                             HasSubstr("Invalid SOS weight"))));
}

TEST(SosConstraintValidatorTest, NanWeight) {
  SosConstraintProto data = SimpleSosConstraintProto();
  data.set_weights(0, std::numeric_limits<double>::quiet_NaN());
  EXPECT_THAT(ValidateConstraint(data, SimpleVariableUniverse({1})),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("value: nan"),
                             HasSubstr("Invalid SOS weight"))));
}

TEST(SosConstraintValidatorTest, RepeatWeight) {
  SosConstraintProto data;
  data.add_expressions();
  data.add_expressions();
  data.add_weights(1.0);
  data.add_weights(1.0);
  EXPECT_THAT(ValidateConstraint(data, SimpleVariableUniverse({1})),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("duplicate weight")));
}

}  // namespace
}  // namespace operations_research::math_opt
