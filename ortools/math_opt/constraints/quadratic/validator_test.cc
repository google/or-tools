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

#include "ortools/math_opt/constraints/quadratic/validator.h"

#include <cstdint>
#include <initializer_list>
#include <limits>
#include <type_traits>

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

using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::status::StatusIs;

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();

IdNameBiMap SimpleVariableUniverse(const std::initializer_list<int64_t> ids) {
  IdNameBiMap universe;
  CHECK_OK(universe.BulkUpdate({}, ids, {}));
  return universe;
}

QuadraticConstraintProto SimpleQuadraticConstraintProto() {
  QuadraticConstraintProto data;
  data.mutable_linear_terms()->add_ids(2);
  data.mutable_linear_terms()->add_values(2.0);
  data.mutable_quadratic_terms()->add_row_ids(1);
  data.mutable_quadratic_terms()->add_column_ids(1);
  data.mutable_quadratic_terms()->add_coefficients(3.0);
  data.set_lower_bound(4.0);
  data.set_upper_bound(5.0);
  data.set_name("c");
  return data;
}

TEST(QuadraticConstraintDataValidatorTest, EmptyConstraintOk) {
  EXPECT_OK(ValidateConstraint(QuadraticConstraintProto(),
                               SimpleVariableUniverse({})));
}

TEST(QuadraticConstraintDataValidatorTest, SimpleConstraintOk) {
  EXPECT_OK(ValidateConstraint(SimpleQuadraticConstraintProto(),
                               SimpleVariableUniverse({1, 2})));
}

TEST(QuadraticConstraintDataValidatorTest, BadLinearTermIds) {
  QuadraticConstraintProto data = SimpleQuadraticConstraintProto();
  data.mutable_linear_terms()->add_ids(1);
  data.mutable_linear_terms()->add_values(6.0);
  EXPECT_THAT(
      ValidateConstraint(data, SimpleVariableUniverse({1, 2})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("Expected ids to be strictly increasing"),
                     HasSubstr("bad linear term in quadratic constraint"))));
}

TEST(QuadraticConstraintDataValidatorTest, PositiveInfLinearTermValue) {
  QuadraticConstraintProto data = SimpleQuadraticConstraintProto();
  data.mutable_linear_terms()->set_values(0, kInf);
  EXPECT_THAT(
      ValidateConstraint(data, SimpleVariableUniverse({1, 2})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("Invalid positive infinite value"),
                     HasSubstr("bad linear term in quadratic constraint"))));
}

TEST(QuadraticConstraintDataValidatorTest, NegativeInfLinearTermValue) {
  QuadraticConstraintProto data = SimpleQuadraticConstraintProto();
  data.mutable_linear_terms()->set_values(0, -kInf);
  EXPECT_THAT(
      ValidateConstraint(data, SimpleVariableUniverse({1, 2})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("Invalid negative infinite value"),
                     HasSubstr("bad linear term in quadratic constraint"))));
}

TEST(QuadraticConstraintDataValidatorTest, UnknownLinearTermId) {
  EXPECT_THAT(
      ValidateConstraint(SimpleQuadraticConstraintProto(),
                         SimpleVariableUniverse({1})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("id 2 not found"),
                     HasSubstr("bad linear term ID in quadratic constraint"))));
}

TEST(QuadraticConstraintDataValidatorTest, BadQuadraticTermIds) {
  QuadraticConstraintProto data = SimpleQuadraticConstraintProto();
  data.mutable_quadratic_terms()->set_row_ids(0, 2);
  EXPECT_THAT(
      ValidateConstraint(data, SimpleVariableUniverse({1, 2})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("lower triangular entry"),
                     HasSubstr("bad quadratic term in quadratic constraint"))));
}

TEST(QuadraticConstraintDataValidatorTest, PositiveInfQuadraticTermValue) {
  QuadraticConstraintProto data = SimpleQuadraticConstraintProto();
  data.mutable_quadratic_terms()->set_coefficients(0, kInf);
  EXPECT_THAT(
      ValidateConstraint(data, SimpleVariableUniverse({1, 2})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("Expected finite coefficients"),
                     HasSubstr("bad quadratic term in quadratic constraint"))));
}

TEST(QuadraticConstraintDataValidatorTest, NegativeInfQuadraticTermValue) {
  QuadraticConstraintProto data = SimpleQuadraticConstraintProto();
  data.mutable_quadratic_terms()->set_coefficients(0, -kInf);
  EXPECT_THAT(
      ValidateConstraint(data, SimpleVariableUniverse({1, 2})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("Expected finite coefficients"),
                     HasSubstr("bad quadratic term in quadratic constraint"))));
}

TEST(QuadraticConstraintDataValidatorTest, UnknownQuadraticTermId) {
  EXPECT_THAT(
      ValidateConstraint(SimpleQuadraticConstraintProto(),
                         SimpleVariableUniverse({2})),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          AllOf(HasSubstr("Unknown row_id"),
                HasSubstr("bad quadratic term ID in quadratic constraint"))));
}

TEST(QuadraticConstraintDataValidatorTest, PositiveInfLowerBound) {
  QuadraticConstraintProto data = SimpleQuadraticConstraintProto();
  data.set_lower_bound(kInf);
  EXPECT_THAT(
      ValidateConstraint(data, SimpleVariableUniverse({1, 2})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("Invalid positive infinite value"),
                     HasSubstr("bad quadratic constraint lower bound"))));
}

TEST(QuadraticConstraintDataValidatorTest, NanLowerBound) {
  QuadraticConstraintProto data = SimpleQuadraticConstraintProto();
  data.set_lower_bound(kNan);
  EXPECT_THAT(
      ValidateConstraint(data, SimpleVariableUniverse({1, 2})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("Invalid NaN value"),
                     HasSubstr("bad quadratic constraint lower bound"))));
}

TEST(QuadraticConstraintDataValidatorTest, NegativeInfUpperBound) {
  QuadraticConstraintProto data = SimpleQuadraticConstraintProto();
  data.set_upper_bound(-kInf);
  EXPECT_THAT(
      ValidateConstraint(data, SimpleVariableUniverse({1, 2})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("Invalid negative infinite value"),
                     HasSubstr("bad quadratic constraint upper bound"))));
}

TEST(QuadraticConstraintDataValidatorTest, NanUpperBound) {
  QuadraticConstraintProto data = SimpleQuadraticConstraintProto();
  data.set_upper_bound(kNan);
  EXPECT_THAT(
      ValidateConstraint(data, SimpleVariableUniverse({1, 2})),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("Invalid NaN value"),
                     HasSubstr("bad quadratic constraint upper bound"))));
}

TEST(QuadraticConstraintDataValidatorTest, InvertedBounds) {
  QuadraticConstraintProto data = SimpleQuadraticConstraintProto();
  data.set_upper_bound(data.lower_bound() - 1);
  EXPECT_THAT(ValidateConstraint(data, SimpleVariableUniverse({1, 2})),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("Quadratic constraint"),
                             HasSubstr("bounds are inverted"))));
}

}  // namespace
}  // namespace operations_research::math_opt
