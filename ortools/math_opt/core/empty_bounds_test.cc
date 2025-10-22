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

#include "ortools/math_opt/core/empty_bounds.h"

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/validators/result_validator.h"

namespace operations_research::math_opt {
namespace {

using ::testing::HasSubstr;

TEST(ResultForIntegerInfeasibleTest, Maximization) {
  const SolveResultProto result =
      ResultForIntegerInfeasible(/*is_maximize=*/true,
                                 /*bad_variable_id=*/3,
                                 /*lb=*/3.5, /*ub=*/3.75);
  EXPECT_EQ(result.termination().reason(), TERMINATION_REASON_INFEASIBLE);
  EXPECT_THAT(result.termination().detail(), HasSubstr("id: 3"));
  EXPECT_THAT(result.termination().detail(), HasSubstr("[3.5, 3.75]"));
  ModelSummary model_summary;
  model_summary.maximize = true;
  EXPECT_OK(ValidateResult(result, /*parameters=*/{}, model_summary));
}

TEST(ResultForIntegerInfeasibleTest, Minimization) {
  const SolveResultProto result =
      ResultForIntegerInfeasible(/*is_maximize=*/false,
                                 /*bad_variable_id=*/0,
                                 /*lb=*/-8.5, /*ub=*/-8.25);
  EXPECT_EQ(result.termination().reason(), TERMINATION_REASON_INFEASIBLE);
  EXPECT_THAT(result.termination().detail(), HasSubstr("id: 0"));
  EXPECT_THAT(result.termination().detail(), HasSubstr("[-8.5, -8.25]"));
  EXPECT_OK(ValidateResult(result, /*parameters=*/{},
                           /*model_summary=*/ModelSummary{}));
}

}  // namespace
}  // namespace operations_research::math_opt
