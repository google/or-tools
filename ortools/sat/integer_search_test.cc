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

#include "ortools/sat/integer_search.h"

#include <random>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "gtest/gtest.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/random_engine.h"

namespace operations_research {
namespace sat {
namespace {

// We generate a simple assignment problem for which the LP should give us the
// optimal, thus we expect no conflict.
TEST(ExploitLpSolution, LpIsOptimal) {
  CpModelProto problem;
  const int n = 10;

  // n^2 Boolean arc variables.
  std::vector<std::vector<int>> graph(n, std::vector<int>(n));
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      graph[i][j] = problem.variables_size();
      sat::IntegerVariableProto* var = problem.add_variables();
      var->add_domain(0);
      var->add_domain(1);
    }
  }
  // Exactly one incoming and one outgoing.
  for (int i = 0; i < n; ++i) {
    auto* ct = problem.add_constraints()->mutable_linear();
    ct->add_domain(1);
    ct->add_domain(1);
    for (int j = 0; j < n; ++j) {
      ct->add_coeffs(1);
      ct->add_vars(graph[i][j]);
    }
  }
  for (int i = 0; i < n; ++i) {
    auto* ct = problem.add_constraints()->mutable_linear();
    ct->add_domain(1);
    ct->add_domain(1);
    for (int j = 0; j < n; ++j) {
      ct->add_coeffs(1);
      ct->add_vars(graph[j][i]);
    }
  }
  // Some cost.
  random_engine_t random;
  std::uniform_int_distribution<int> dist(10, 100);
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      problem.mutable_objective()->add_coeffs(dist(random));
      problem.mutable_objective()->add_vars(graph[i][j]);
    }
  }

  for (const bool exploit_integer_lp_solution : {true, false}) {
    Model model;
    SatParameters parameters;
    parameters.set_cp_model_presolve(false);
    parameters.set_exploit_integer_lp_solution(exploit_integer_lp_solution);
    parameters.set_exploit_all_lp_solution(false);
    parameters.set_linearization_level(2);
    parameters.set_num_workers(1);
    model.Add(NewSatParameters(parameters));
    const CpSolverResponse response = SolveCpModel(problem, &model);
    LOG(INFO) << CpSolverResponseStats(response);
    EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
    if (exploit_integer_lp_solution) {
      EXPECT_EQ(response.num_conflicts(), 0);
    } else {
      EXPECT_GT(response.num_conflicts(), 0);
    }
  }
}

TEST(ValueSelectionTest, AtMinValue) {
  Model model;
  const IntegerVariable x = model.Add(NewIntegerVariable(5, 10));
  EXPECT_EQ(IntegerLiteral::LowerOrEqual(x, IntegerValue(5)),
            AtMinValue(x, model.GetOrCreate<IntegerTrail>()));
}

TEST(ValueSelectionTest, GreaterOrEqualToMiddleValue) {
  Model model;
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  const IntegerVariable x = model.Add(NewIntegerVariable(6, 10));
  EXPECT_EQ(IntegerLiteral::GreaterOrEqual(x, IntegerValue(8)),
            GreaterOrEqualToMiddleValue(x, integer_trail));
  EXPECT_EQ(IntegerLiteral::LowerOrEqual(x, IntegerValue(8)),
            GreaterOrEqualToMiddleValue(NegationOf(x), integer_trail));
}

TEST(ValueSelectionTest, SplitAroundGivenValue) {
  Model model;
  const IntegerVariable x = model.Add(NewIntegerVariable(6, 10));

  EXPECT_EQ(IntegerLiteral::GreaterOrEqual(x, IntegerValue(10)),
            SplitAroundGivenValue(x, IntegerValue(10), &model));
  EXPECT_EQ(IntegerLiteral::LowerOrEqual(x, IntegerValue(6)),
            SplitAroundGivenValue(x, IntegerValue(6), &model));
  EXPECT_EQ(IntegerLiteral(),
            SplitAroundGivenValue(x, IntegerValue(5), &model));
}

TEST(ValueSelectionTest, SplitAroundGivenValueObjectiveDirection) {
  Model model;
  const IntegerVariable x = model.Add(NewIntegerVariable(6, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(6, 10));
  model.GetOrCreate<ObjectiveDefinition>()
      ->objective_impacting_variables.insert(x);
  model.GetOrCreate<ObjectiveDefinition>()
      ->objective_impacting_variables.insert(NegationOf(y));

  EXPECT_EQ(IntegerLiteral::LowerOrEqual(x, IntegerValue(8)),
            SplitAroundGivenValue(x, IntegerValue(8), &model));
  EXPECT_EQ(IntegerLiteral::GreaterOrEqual(y, IntegerValue(8)),
            SplitAroundGivenValue(y, IntegerValue(8), &model));
  EXPECT_EQ(IntegerLiteral(),
            SplitAroundGivenValue(x, IntegerValue(5), &model));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
