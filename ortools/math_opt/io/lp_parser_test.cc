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

#include "ortools/math_opt/io/lp_parser.h"

#include <limits>
#include <string>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/io/lp_converter.h"

namespace operations_research::math_opt {
namespace {

using ::testing::EquivToProto;
using ::testing::HasSubstr;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

constexpr double kInf = std::numeric_limits<double>::infinity();

TEST(ModelProtoFromLpTest, SimpleLpRoundTrips) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(-kInf, kInf, "y");
  model.AddLinearConstraint(x + 2 * y <= 1.0, "c");
  model.AddLinearConstraint(x - y == 2.0, "d");
  model.AddLinearConstraint(2 * y >= 3.0, "e");
  model.Minimize(-x + y);
  const ModelProto model_proto = model.ExportModel();

  ASSERT_OK_AND_ASSIGN(const std::string lp_format,
                       ModelProtoToLp(model_proto));
  EXPECT_THAT(ModelProtoFromLp(lp_format),
              IsOkAndHolds(EquivToProto(model_proto)));
}

TEST(ModelProtoFromLpTest, SimpleIpRoundTrips) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  model.Maximize(x);
  const ModelProto model_proto = model.ExportModel();

  ASSERT_OK_AND_ASSIGN(const std::string lp_format,
                       ModelProtoToLp(model_proto));
  EXPECT_THAT(ModelProtoFromLp(lp_format),
              IsOkAndHolds(EquivToProto(model_proto)));
}

// See comment on header file, unfortunately the names of indicator constraints
// are not preserved, so we strip them before making any test assertions.
void RemoveIndicatorNames(ModelProto& model) {
  for (auto& [id, indicator] : *model.mutable_indicator_constraints()) {
    indicator.set_name("");
  }
}

TEST(ModelProtoFromLpTest, IndicatorOnOneRoundTrips) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  const Variable z = model.AddBinaryVariable("z");
  model.AddIndicatorConstraint(x, y + z <= 1, /*activate_on_zero=*/false, "c");

  ASSERT_OK_AND_ASSIGN(const std::string lp_format,
                       ModelProtoToLp(model.ExportModel()));
  ModelProto expected = model.ExportModel();
  RemoveIndicatorNames(expected);
  ASSERT_OK_AND_ASSIGN(ModelProto actual, ModelProtoFromLp(lp_format));
  RemoveIndicatorNames(actual);
  EXPECT_THAT(actual, EquivToProto(expected));
}

TEST(ModelProtoFromLpTest, IndicatorOnZeroRoundTrips) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  const Variable z = model.AddBinaryVariable("z");
  model.AddIndicatorConstraint(x, y + z <= 1, /*activate_on_zero=*/true, "c");

  ASSERT_OK_AND_ASSIGN(const std::string lp_format,
                       ModelProtoToLp(model.ExportModel()));
  ModelProto expected = model.ExportModel();
  RemoveIndicatorNames(expected);
  ASSERT_OK_AND_ASSIGN(ModelProto actual, ModelProtoFromLp(lp_format));
  RemoveIndicatorNames(actual);
  EXPECT_THAT(actual, EquivToProto(expected));
}

TEST(ModelProtoFromLpTest, InvalidLpFileErrors) {
  EXPECT_THAT(ModelProtoFromLp("ST\n x * y * z = x\nEND"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("failed to convert LP file with SCIP")));
}

TEST(ModelProtoFromLpTest, QuadraticConstraintErrors) {
  EXPECT_THAT(ModelProtoFromLp("ST\n [ 4 x * x ] <= 1\nEND"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("failed to parse MPS")));
}

}  // namespace
}  // namespace operations_research::math_opt
