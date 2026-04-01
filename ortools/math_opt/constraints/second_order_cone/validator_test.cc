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

#include "ortools/math_opt/constraints/second_order_cone/validator.h"

#include <cstdint>
#include <initializer_list>

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

IdNameBiMap SimpleVariableUniverse(const std::initializer_list<int64_t> ids) {
  IdNameBiMap universe;
  CHECK_OK(universe.BulkUpdate({}, ids, {}));
  return universe;
}

SecondOrderConeConstraintProto SimpleSecondOrderConeConstraint() {
  SecondOrderConeConstraintProto data;
  {
    LinearExpressionProto& expr = *data.mutable_upper_bound();
    expr.add_ids(1);
    expr.add_coefficients(2.0);
    expr.set_offset(3.0);
  }
  {
    LinearExpressionProto& expr = *data.add_arguments_to_norm();
    expr.add_ids(2);
    expr.add_coefficients(3.0);
    expr.set_offset(4.0);
  }
  return data;
}

TEST(SecondOrderConeValidatorTest, EmptyConstraintOk) {
  EXPECT_OK(ValidateConstraint(SecondOrderConeConstraintProto(),
                               SimpleVariableUniverse({})));
}

TEST(SecondOrderConeValidatorTest, SimpleConstraintOk) {
  EXPECT_OK(ValidateConstraint(SimpleSecondOrderConeConstraint(),
                               SimpleVariableUniverse({1, 2})));
}

TEST(SosConstraintValidatorTest, InvalidUpperBound) {
  SecondOrderConeConstraintProto data = SimpleSecondOrderConeConstraint();
  data.mutable_upper_bound()->add_ids(2);
  EXPECT_THAT(
      ValidateConstraint(data, SimpleVariableUniverse({1, 2})),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("upper_bound")));
}

TEST(SosConstraintValidatorTest, InvalidArgumentsToNorm) {
  SecondOrderConeConstraintProto data = SimpleSecondOrderConeConstraint();
  data.mutable_arguments_to_norm(0)->add_ids(2);
  EXPECT_THAT(ValidateConstraint(data, SimpleVariableUniverse({1})),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("arguments_to_norm")));
}

}  // namespace
}  // namespace operations_research::math_opt
