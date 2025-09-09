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

#include "ortools/math_opt/cpp/statistics.h"

#include <limits>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/model.h"

namespace operations_research::math_opt {
namespace {

using ::testing::Optional;
using ::testing::Pair;

constexpr double kInf = std::numeric_limits<double>::infinity();

std::string PrintToString(const ModelRanges& ranges) {
  std::ostringstream oss;
  oss << ranges;
  return oss.str();
}

TEST(ModelRangesTest, Printing) {
  EXPECT_EQ(PrintToString(ModelRanges{}),
            "Objective terms           : no finite values\n"
            "Variable bounds           : no finite values\n"
            "Linear constraints bounds : no finite values\n"
            "Linear constraints coeffs : no finite values");

  EXPECT_EQ(
      PrintToString(ModelRanges{
          .objective_terms = std::make_pair(2.12345e-99, 1.12345e3),
          .variable_bounds = std::make_pair(9.12345e-2, 1.12345e2),
          .linear_constraint_bounds = std::make_pair(2.12345e6, 5.12345e99),
          .linear_constraint_coefficients = std::make_pair(0.0, 0.0),
      }),
      "Objective terms           : [2.12e-99 , 1.12e+03 ]\n"
      "Variable bounds           : [9.12e-02 , 1.12e+02 ]\n"
      "Linear constraints bounds : [2.12e+06 , 5.12e+99 ]\n"
      "Linear constraints coeffs : [0.00e+00 , 0.00e+00 ]");

  EXPECT_EQ(
      PrintToString(ModelRanges{
          .objective_terms = std::make_pair(2.12345e-1, 1.12345e3),
          .variable_bounds = std::make_pair(9.12345e-2, 1.12345e2),
          .linear_constraint_bounds = std::make_pair(2.12345e6, 5.12345e99),
          .linear_constraint_coefficients = std::make_pair(0.0, 1.0e100),
      }),
      "Objective terms           : [2.12e-01 , 1.12e+03 ]\n"
      "Variable bounds           : [9.12e-02 , 1.12e+02 ]\n"
      "Linear constraints bounds : [2.12e+06 , 5.12e+99 ]\n"
      "Linear constraints coeffs : [0.00e+00 , 1.00e+100]");

  EXPECT_EQ(
      PrintToString(ModelRanges{
          .objective_terms = std::make_pair(2.12345e-100, 1.12345e3),
          .variable_bounds = std::make_pair(9.12345e-2, 1.12345e2),
          .linear_constraint_bounds = std::make_pair(2.12345e6, 5.12345e99),
          .linear_constraint_coefficients = std::make_pair(0.0, 0.0),
      }),
      "Objective terms           : [2.12e-100, 1.12e+03 ]\n"
      "Variable bounds           : [9.12e-02 , 1.12e+02 ]\n"
      "Linear constraints bounds : [2.12e+06 , 5.12e+99 ]\n"
      "Linear constraints coeffs : [0.00e+00 , 0.00e+00 ]");

  EXPECT_EQ(
      PrintToString(ModelRanges{
          .objective_terms = std::make_pair(2.12345e-100, 1.12345e3),
          .variable_bounds = std::make_pair(9.12345e-2, 1.12345e2),
          .linear_constraint_bounds = std::make_pair(2.12345e6, 5.12345e99),
          .linear_constraint_coefficients = std::make_pair(0.0, 1.0e100),
      }),
      "Objective terms           : [2.12e-100, 1.12e+03 ]\n"
      "Variable bounds           : [9.12e-02 , 1.12e+02 ]\n"
      "Linear constraints bounds : [2.12e+06 , 5.12e+99 ]\n"
      "Linear constraints coeffs : [0.00e+00 , 1.00e+100]");
}

TEST(ModelRangesTest, PrintingResetFlags) {
  const ModelRanges ranges = {
      .objective_terms = std::make_pair(2.12345e-100, 1.12345e3),
      .variable_bounds = std::make_pair(9.12345e-2, 1.12345e2),
      .linear_constraint_bounds = std::make_pair(2.12345e6, 5.12345e99),
      .linear_constraint_coefficients = std::make_pair(0.0, 1.0e100),
  };

  std::ostringstream oss;
  oss << ranges << '\n' << 1.23456789;

  EXPECT_EQ(oss.str(),
            "Objective terms           : [2.12e-100, 1.12e+03 ]\n"
            "Variable bounds           : [9.12e-02 , 1.12e+02 ]\n"
            "Linear constraints bounds : [2.12e+06 , 5.12e+99 ]\n"
            "Linear constraints coeffs : [0.00e+00 , 1.00e+100]\n"
            "1.23457");
}

TEST(ComputeModelRangesTest, Empty) {
  const ModelRanges ranges = ComputeModelRanges(Model("model"));
  EXPECT_EQ(ranges.objective_terms, std::nullopt);
  EXPECT_EQ(ranges.variable_bounds, std::nullopt);
  EXPECT_EQ(ranges.linear_constraint_bounds, std::nullopt);
  EXPECT_EQ(ranges.linear_constraint_coefficients, std::nullopt);
}

TEST(ComputeModelRangesTest, OnlyZeroAndInfiniteValues) {
  Model model("model");
  model.AddContinuousVariable(/*lower_bound=*/0.0, /*upper_bound=*/kInf);
  model.AddContinuousVariable(/*lower_bound=*/-kInf, /*upper_bound=*/0.0);
  model.AddContinuousVariable(/*lower_bound=*/-kInf, /*upper_bound=*/kInf);
  model.AddLinearConstraint(/*lower_bound=*/0.0, /*upper_bound=*/kInf);
  model.AddLinearConstraint(/*lower_bound=*/-kInf, /*upper_bound=*/0.0);
  model.AddLinearConstraint(/*lower_bound=*/-kInf, /*upper_bound=*/kInf);

  const ModelRanges ranges = ComputeModelRanges(model);
  EXPECT_EQ(ranges.objective_terms, std::nullopt);
  EXPECT_EQ(ranges.variable_bounds, std::nullopt);
  EXPECT_EQ(ranges.linear_constraint_bounds, std::nullopt);
  EXPECT_EQ(ranges.linear_constraint_coefficients, std::nullopt);
}

TEST(ComputeModelRangesTest, MixedValues) {
  Model model("model");
  const Variable x = model.AddContinuousVariable(/*lower_bound=*/0.0,
                                                 /*upper_bound=*/0.0, "x");
  const Variable y = model.AddContinuousVariable(/*lower_bound=*/-kInf,
                                                 /*upper_bound=*/1e-3, "y");
  const Variable z = model.AddContinuousVariable(/*lower_bound=*/-3e2,
                                                 /*upper_bound=*/kInf, "z");
  model.Minimize(-5.0e4 * x + 1.0e-6 * z * x + 3.0 * y);
  model.AddLinearConstraint(0.0 <= 1.25e-3 * y - 4.5e3 * x, "c");
  model.AddLinearConstraint(/*lower_bound=*/-kInf, /*upper_bound=*/3e4);
  model.AddLinearConstraint(-1e-5 <= 2.5e-3 * y <= 0.0);

  const ModelRanges ranges = ComputeModelRanges(model);
  EXPECT_THAT(ranges.objective_terms, Optional(Pair(1.0e-6, 5.0e4)));
  EXPECT_THAT(ranges.variable_bounds, Optional(Pair(1e-3, 3e2)));
  EXPECT_THAT(ranges.linear_constraint_bounds, Optional(Pair(1e-5, 3e4)));
  EXPECT_THAT(ranges.linear_constraint_coefficients,
              Optional(Pair(1.25e-3, 4.5e3)));
}

}  // namespace
}  // namespace operations_research::math_opt
