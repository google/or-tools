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

#include "ortools/math_opt/constraints/indicator/validator.h"

#include <cstdint>
#include <initializer_list>
#include <limits>
#include <string>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research::math_opt {
namespace {

using ::testing::HasSubstr;
using ::testing::status::StatusIs;

constexpr double kInf = std::numeric_limits<double>::infinity();

IdNameBiMap SimpleVariableUniverse(const std::initializer_list<int64_t> ids) {
  IdNameBiMap universe;
  CHECK_OK(universe.BulkUpdate({}, ids, {}));
  return universe;
}

IndicatorConstraintProto SimpleIndicatorConstraintProto() {
  IndicatorConstraintProto data;
  data.set_indicator_id(1);
  data.mutable_expression()->add_ids(2);
  data.mutable_expression()->add_values(3.0);
  data.set_lower_bound(-1.0);
  data.set_upper_bound(1.0);
  return data;
}

TEST(IndicatorConstraintValidatorTest, SimpleConstraintOk) {
  EXPECT_OK(ValidateConstraint(SimpleIndicatorConstraintProto(),
                               SimpleVariableUniverse({1, 2})));
}

TEST(IndicatorConstraintValidatorTest, UnsetIndicatorId) {
  IndicatorConstraintProto constraint = SimpleIndicatorConstraintProto();
  constraint.clear_indicator_id();
  EXPECT_OK(ValidateConstraint(constraint, SimpleVariableUniverse({2})));
}

TEST(IndicatorConstraintValidatorTest, InvalidIndicatorId) {
  EXPECT_THAT(ValidateConstraint(SimpleIndicatorConstraintProto(),
                                 SimpleVariableUniverse({2})),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("indicator variable id")));
}

TEST(IndicatorConstraintValidatorTest, InvalidTermInExpression) {
  IndicatorConstraintProto constraint = SimpleIndicatorConstraintProto();
  constraint.mutable_expression()->set_values(0, kInf);
  EXPECT_THAT(ValidateConstraint(constraint, SimpleVariableUniverse({1, 2})),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("infinite value"),
                             HasSubstr("implied constraint"))));
}

TEST(IndicatorConstraintValidatorTest, InvalidIdInExpression) {
  EXPECT_THAT(ValidateConstraint(SimpleIndicatorConstraintProto(),
                                 SimpleVariableUniverse({1})),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("variable id"),
                             HasSubstr("implied constraint"))));
}

TEST(IndicatorConstraintValidatorTest, InvalidLowerBound) {
  IndicatorConstraintProto constraint = SimpleIndicatorConstraintProto();
  constraint.set_lower_bound(kInf);
  EXPECT_THAT(ValidateConstraint(constraint, SimpleVariableUniverse({1, 2})),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid lower bound")));
  constraint.set_lower_bound(std::numeric_limits<double>::quiet_NaN());
  EXPECT_THAT(ValidateConstraint(constraint, SimpleVariableUniverse({1, 2})),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid lower bound")));
}

TEST(IndicatorConstraintValidatorTest, InvalidUpperBound) {
  IndicatorConstraintProto constraint = SimpleIndicatorConstraintProto();
  constraint.set_upper_bound(-kInf);
  EXPECT_THAT(ValidateConstraint(constraint, SimpleVariableUniverse({1, 2})),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid upper bound")));
  constraint.set_upper_bound(std::numeric_limits<double>::quiet_NaN());
  EXPECT_THAT(ValidateConstraint(constraint, SimpleVariableUniverse({1, 2})),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid upper bound")));
}

}  // namespace
}  // namespace operations_research::math_opt
