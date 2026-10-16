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

#include "ortools/math_opt/validators/sparse_vector_validator.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/validators/scalar_validator.h"

namespace operations_research {
namespace math_opt {
namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

using ::absl::StatusCode;
using ::testing::HasSubstr;
using ::testing::status::StatusIs;

TEST(CheckIdsAndValuesSizeTest, Ok) {
  const std::string a = "x3";
  const std::string b = "";
  const std::vector<int64_t> ids = {3, -5};
  const std::vector<double> values = {2.0, 4.0};
  const std::vector<const std::string*> names = {&a, &b};
  EXPECT_OK(CheckIdsAndValuesSize(MakeView(ids, values)));
  EXPECT_OK(CheckIdsAndValuesSize(MakeView(ids, names)));
}

TEST(CheckIdsAndValuesSizeTest, NotOk) {
  const std::string a = "x3";
  const std::string b = "";
  const std::vector<int64_t> ids = {3, 5, 6};
  const std::vector<double> values = {2.0, 4.0};
  const std::vector<const std::string*> names = {&a, &b};
  EXPECT_THAT(
      CheckIdsAndValuesSize(MakeView(ids, values)),
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("Ids size= 3 should be equal to values size= 2")));
  EXPECT_THAT(
      CheckIdsAndValuesSize(MakeView(ids, names), "names"),
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("Ids size= 3 should be equal to names size= 2")));
}

TEST(CheckValuesNotFloatingPointTest, Ok) {
  const std::string a = "x3";
  const std::string b = "";
  const std::vector<int64_t> ids = {3, -5};
  const std::vector<const std::string*> names = {&a, &b};
  const std::vector<int> integers = {1, 0};
  EXPECT_OK(CheckValues(MakeView(ids, names)));
  EXPECT_OK(CheckValues(MakeView(ids, integers)));
}

TEST(CheckValuesNotFloatingPointTest, NotOk) {
  const std::string a = "x3";
  const std::string b = "";
  const std::vector<int64_t> ids = {3, 5, 6};
  const std::vector<const std::string*> names = {&a, &b};
  const std::vector<int> integers = {1, 0};
  EXPECT_THAT(
      CheckValues(MakeView(ids, names), "names"),
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("Ids size= 3 should be equal to names size= 2")));
  EXPECT_THAT(
      CheckValues(MakeView(ids, integers), "integers"),
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("Ids size= 3 should be equal to integers size= 2")));
}

TEST(CheckValuesFloatingPointTest, OkSizeAndNan) {
  const std::vector<int64_t> ids = {3, 5, -6};
  const std::vector<double> values = {2.0, -kInf, kInf};
  EXPECT_OK(CheckValues(MakeView(ids, values), DoubleOptions()));
}

TEST(CheckValuesFloatingPointTest, OkNoInfinity) {
  const std::vector<int64_t> ids = {3, 5, -6};
  const std::vector<double> values = {2.0, 4.0, 1.0};
  EXPECT_OK(CheckValues(
      MakeView(ids, values),
      {.allow_positive_infinity = false, .allow_negative_infinity = false}));
}

TEST(CheckValuesFloatingPointTest, OkNoPositiveInfinity) {
  const std::vector<int64_t> ids = {3, 5, -6};
  const std::vector<double> values = {2.0, -kInf, 1.0};
  EXPECT_OK(
      CheckValues(MakeView(ids, values), {.allow_positive_infinity = false}));
}

TEST(CheckValuesFloatingPointTest, OkNoNegativeInfinity) {
  const std::vector<int64_t> ids = {3, 5, -6};
  const std::vector<double> values = {2.0, 4.0, kInf};
  EXPECT_OK(
      CheckValues(MakeView(ids, values), {.allow_negative_infinity = false}));
}

TEST(CheckValuesFloatingPointTest, NotOkSize) {
  const std::vector<int64_t> ids = {3, 5, 6};
  const std::vector<double> values = {2.0, 4.0};
  EXPECT_THAT(CheckValues(MakeView(ids, values), DoubleOptions()),
              StatusIs(StatusCode::kInvalidArgument, HasSubstr("values size")));
}

TEST(CheckValuesFloatingPointTest, NotOkNan) {
  const std::vector<int64_t> ids = {3, 5};
  const std::vector<double> values = {std::nan(""), 4.0};
  EXPECT_THAT(CheckValues(MakeView(ids, values), DoubleOptions(), "value_name"),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("NaN"), HasSubstr("value_name"))));
}

TEST(CheckValuesFloatingPointTest, NotOkPositiveInfinity) {
  const std::vector<int64_t> ids = {3, 5};
  const std::vector<double> values = {kInf, 4.0};
  EXPECT_THAT(
      CheckValues(MakeView(ids, values), {.allow_positive_infinity = false},
                  "value_name"),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("positive infinite"), HasSubstr("value_name"))));
}

TEST(CheckValuesFloatingPointTest, NotOkNegativeInfinity) {
  const std::vector<int64_t> ids = {3, 5};
  const std::vector<double> values = {-kInf, 4.0};
  EXPECT_THAT(
      CheckValues(MakeView(ids, values), {.allow_negative_infinity = false},
                  "value_name"),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("negative infinite"), HasSubstr("value_name"))));
}

TEST(CheckIdsAndValuesNotFloatingPointTest, Ok) {
  const std::string a = "x3";
  const std::string b = "";
  const std::vector<int64_t> ids = {3, 5};
  const std::vector<const std::string*> names = {&a, &b};
  const std::vector<int> integers = {1, 0};
  EXPECT_OK(CheckValues(MakeView(ids, names)));
  EXPECT_OK(CheckValues(MakeView(ids, integers)));
}

TEST(CheckIdsAndValuesNotFloatingPointTest, IdsNotOk) {
  const std::string a = "x3";
  const std::string b = "";
  const std::vector<int64_t> ids = {-3, 5};
  const std::vector<const std::string*> names = {&a, &b};
  const std::vector<int> integers = {1, 0};
  EXPECT_THAT(CheckIdsAndValues(MakeView(ids, names)),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Expected ids to be nonnegative")));
  EXPECT_THAT(CheckIdsAndValues(MakeView(ids, integers)),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Expected ids to be nonnegative")));
}

TEST(CheckIdsAndValuesNotFloatingPointTest, ValuesNotOk) {
  const std::string a = "x3";
  const std::string b = "";
  const std::vector<int64_t> ids = {3, 5, 6};
  const std::vector<const std::string*> names = {&a, &b};
  const std::vector<int> integers = {1, 0};
  EXPECT_THAT(CheckIdsAndValues(MakeView(ids, names), "names"),
              StatusIs(StatusCode::kInvalidArgument, HasSubstr("names size")));
  EXPECT_THAT(
      CheckIdsAndValues(MakeView(ids, integers), "integers"),
      StatusIs(StatusCode::kInvalidArgument, HasSubstr("integers size")));
}

TEST(CheckIdsAndValuesFloatingPointTest, Ok) {
  const std::vector<int64_t> ids = {3, 5, 6};
  const std::vector<double> values = {2.0, -kInf, kInf};
  EXPECT_OK(CheckValues(MakeView(ids, values), DoubleOptions()));
}

TEST(CheckIdsAndValuesFloatingPointTest, CheckIdsAndValuesDouble) {
  const std::vector<int64_t> ids = {-3, 5, 6};
  const std::vector<double> values = {2.0, -kInf, kInf};
  EXPECT_THAT(CheckIdsAndValues(MakeView(ids, values), DoubleOptions()),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Expected ids to be nonnegative")));
}

TEST(CheckIdsAndValuesFloatingPointTest, ValuesNotOk) {
  const std::vector<int64_t> ids = {3, 5};
  const std::vector<double> values = {kInf, 4.0};
  EXPECT_THAT(
      CheckIdsAndValues(MakeView(ids, values),
                        {.allow_positive_infinity = false}, "value_name"),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("inf"), HasSubstr("value_name"))));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
