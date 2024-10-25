// Copyright 2010-2024 Google LLC
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

#include "ortools/math_opt/solver_tests/ip_multiple_solutions_tests.h"

#include <cstdint>
#include <optional>
#include <ostream>

#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/port/proto_utils.h"

namespace operations_research::math_opt {

std::ostream& operator<<(std::ostream& out,
                         const IpMultipleSolutionsTestParams& params) {
  out << "{ solver_type: " << params.solver_type << " ensure_hint_in_pool: "
      << ProtobufShortDebugString(params.ensure_hint_in_pool.Proto()) << " }";
  return out;
}

namespace {

TEST_P(IpMultipleSolutionsTest, FindTwoSolutionsUsingHint) {
  ModelSolveParameters model_parameters;

  Model model("Solution Hint MIP");

  Variable x1 = model.AddBinaryVariable("x1");
  Variable x2 = model.AddBinaryVariable("x2");
  model.AddLinearConstraint(x1 + x2 == 1);
  model.Maximize(x1 + 3 * x2);

  // Two feasible solutions [0, 1] and [1, 0]. Hint the worse one.
  ModelSolveParameters::SolutionHint hint;
  hint.variable_values = {{x1, 1.0}, {x2, 0.0}};
  model_parameters.solution_hints.emplace_back(hint);

  const Solution expected1 = {
      .primal_solution =
          PrimalSolution{.variable_values = {{x1, 0.0}, {x2, 1.0}},
                         .objective_value = 3.0,
                         .feasibility_status = SolutionStatus::kFeasible}};

  const Solution expected2 = {
      .primal_solution =
          PrimalSolution{.variable_values = {{x1, 1.0}, {x2, 0.0}},
                         .objective_value = 1.0,
                         .feasibility_status = SolutionStatus::kFeasible}};

  for (int32_t solution_pool_size : {1, 2}) {
    SCOPED_TRACE(absl::StrCat("Solution pool size: ", solution_pool_size));
    SolveArguments args = {.parameters = GetParam().ensure_hint_in_pool,
                           .model_parameters = model_parameters};
    args.parameters.solution_pool_size = solution_pool_size;
    ASSERT_OK_AND_ASSIGN(const SolveResult result,
                         Solve(model, GetParam().solver_type, args));
    EXPECT_THAT(result, IsOptimal(3.0));
    if (solution_pool_size == 1) {
      EXPECT_THAT(result.solutions, ElementsAre(IsNear(expected1)));
    } else {
      // Gurobi does not guarantee that all solution pool entries are feasible,
      // so we also accept undetermined feasibility status.
      EXPECT_THAT(result.solutions,
                  ElementsAre(IsNear(expected1),
                              IsNear(expected2, {.allow_undetermined = true})));
    }
  }
}

}  // namespace

}  // namespace operations_research::math_opt
