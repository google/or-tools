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

#include "ortools/sat/cp_model_expand.h"

#include <cstdint>
#include <string>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/container_logging.h"
#include "ortools/base/gmock.h"
#include "ortools/base/logging.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;

CpSolverResponse SolveAndCheck(
    const CpModelProto& initial_model, absl::string_view extra_parameters = "",
    absl::btree_set<std::vector<int>>* solutions = nullptr) {
  SatParameters params;
  params.set_enumerate_all_solutions(true);
  params.set_keep_all_feasible_solutions_in_presolve(true);
  params.set_debug_crash_if_presolve_breaks_hint(true);
  params.set_log_search_progress(true);
  if (!extra_parameters.empty()) {
    params.MergeFromString(extra_parameters);
  }
  auto observer = [&](const CpSolverResponse& response) {
    VLOG(1) << response;
    EXPECT_TRUE(SolutionIsFeasible(
        initial_model, std::vector<int64_t>(response.solution().begin(),
                                            response.solution().end())));
    if (solutions != nullptr) {
      std::vector<int> solution;
      for (int var = 0; var < initial_model.variables_size(); ++var) {
        solution.push_back(response.solution(var));
      }
      solutions->insert(solution);
    }
  };
  Model model;
  model.Add(NewSatParameters(params));
  model.Add(NewFeasibleSolutionObserver(observer));
  return SolveCpModel(initial_model, &model);
}

TEST(ReservoirExpandTest, NoOptionalAndInitiallyFeasible) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { name: 'x' domain: 0 domain: 2 }
    variables { name: 'y' domain: 0 domain: 2 }
    variables { name: 'z' domain: 0 domain: 2 }
    constraints {
      reservoir {
        time_exprs { vars: 0 coeffs: 1 }
        time_exprs { vars: 1 coeffs: 1 }
        time_exprs { vars: 2 coeffs: 1 }
        level_changes: { offset: 1 }
        level_changes: { offset: 1 }
        level_changes: { offset: 2 }
        min_level: 0
        max_level: 4
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  EXPECT_EQ(27, solutions.size());
}

TEST(ReservoirExpandTest, SimpleSemaphore) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 10 }
    variables { domain: 0 domain: 10 }
    variables { domain: 0 domain: 1 }
    constraints {
      reservoir {
        max_level: 2
        time_exprs { vars: 0 coeffs: 1 }
        time_exprs { vars: 1 coeffs: 1 }
        active_literals: [ 2, 2 ]
        level_changes { offset: -1 }
        level_changes { offset: 1 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  EXPECT_EQ(187, solutions.size());
}

TEST(ReservoirExpandTest, GizaReport) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 10 }
    variables { domain: 0 domain: 10 }
    variables { domain: 0 domain: 10 }
    constraints {
      reservoir {
        time_exprs { vars: 0 coeffs: 1 }
        time_exprs { vars: 1 coeffs: 1 }
        time_exprs { vars: 2 coeffs: 1 }
        level_changes: { offset: 10 }
        level_changes: { offset: 1 }
        level_changes: { offset: -1 }
        min_level: 0
        max_level: 10
      }
    }
    constraints {
      linear {
        vars: 0
        coeffs: 1
        domain: [ 0, 0 ]
      }
    }
    constraints {
      linear {
        vars: 1
        coeffs: 1
        domain: [ 1, 1 ]
      }
    }
    constraints {
      linear {
        vars: 2
        coeffs: 1
        domain: [ 1, 1 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  const CpSolverResponse response = SolveWithParameters(initial_model, params);
  EXPECT_EQ(OPTIMAL, response.status());
}

TEST(ReservoirExpandTest, GizaReportReverse) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 10 }
    variables { domain: 0 domain: 10 }
    variables { domain: 0 domain: 10 }
    constraints {
      reservoir {
        time_exprs { vars: 0 coeffs: 1 }
        time_exprs { vars: 1 coeffs: 1 }
        time_exprs { vars: 2 coeffs: 1 }
        level_changes: { offset: 10 }
        level_changes: { offset: 1 }
        level_changes: { offset: -1 }
        min_level: 0
        max_level: 10
      }
    }
    constraints {
      linear {
        vars: 0
        coeffs: 1
        domain: [ 0, 0 ]
      }
    }
    constraints {
      linear {
        vars: 1
        coeffs: 1
        domain: [ 1, 1 ]
      }
    }
    constraints {
      linear {
        vars: 2
        coeffs: 1
        domain: [ 1, 1 ]
      }
    }
  )pb");
  SatParameters params;
  params.set_cp_model_presolve(false);
  const CpSolverResponse response = SolveWithParameters(initial_model, params);
  EXPECT_EQ(OPTIMAL, response.status());
}

TEST(ReservoirExpandTest, RepeatedTimesWithDifferentActivationVariables) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 2 }
    variables { domain: 0 domain: 2 }
    variables { domain: 1 domain: 1 }
    variables { domain: 0 domain: 1 }
    constraints {
      reservoir {
        time_exprs { vars: 0 coeffs: 1 }
        time_exprs { vars: 1 coeffs: 1 }
        time_exprs { vars: 1 coeffs: 1 }
        level_changes: { offset: 1 }
        level_changes: { offset: 1 }
        level_changes: { offset: -10 }
        active_literals: [ 2, 2, 3 ]
        min_level: 0
        max_level: 2
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  // First two time variables should be unconstrained giving us 3x3 solutions.
  EXPECT_EQ(9, solutions.size());
}

TEST(ReservoirExpandTest, NoOptionalAndInitiallyFeasibleWithConsumption) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { name: 'x' domain: 0 domain: 2 }
    variables { name: 'y' domain: 0 domain: 2 }
    variables { name: 'z' domain: 0 domain: 2 }
    constraints {
      reservoir {
        time_exprs { vars: 0 coeffs: 1 }
        time_exprs { vars: 1 coeffs: 1 }
        time_exprs { vars: 2 coeffs: 1 }
        level_changes: { offset: -1 }
        level_changes: { offset: -1 }
        level_changes: { offset: 2 }
        min_level: 0
        max_level: 2
      }
    }
    constraints {
      all_diff {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  EXPECT_EQ(2, solutions.size());
}

TEST(ReservoirExpandTest, NoOptionalAndInitiallyFeasibleAndOverloaded) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { name: 'x' domain: 0 domain: 2 }
    variables { name: 'y' domain: 0 domain: 2 }
    variables { name: 'z' domain: 0 domain: 2 }
    constraints {
      reservoir {
        time_exprs { vars: 0 coeffs: 1 }
        time_exprs { vars: 1 coeffs: 1 }
        time_exprs { vars: 2 coeffs: 1 }
        level_changes: { offset: 1 }
        level_changes: { offset: 1 }
        level_changes: { offset: 2 }
        min_level: 0
        max_level: 2
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(INFEASIBLE, response.status());
  EXPECT_EQ(0, solutions.size());
}

TEST(ReservoirExpandTest, OneUnschedulableOptionalAndInitiallyFeasible) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { name: 'true' domain: 1 domain: 1 }
    variables { name: 'x' domain: 0 domain: 2 }
    variables { name: 'presence_y' domain: 0 domain: 1 }
    variables { name: 'y' domain: 0 domain: 2 }
    constraints {
      reservoir {
        time_exprs { vars: 1 coeffs: 1 }
        time_exprs { vars: 3 coeffs: 1 }
        level_changes: { offset: 1 }
        level_changes: { offset: 2 }
        active_literals: 0
        active_literals: 2
        min_level: 0
        max_level: 2
      }
    }
    constraints {
      enforcement_literal: -3
      linear { vars: 3 coeffs: 1 domain: 0 domain: 0 }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  EXPECT_EQ(3, solutions.size());
}

TEST(ReservoirExpandTest, OptionalWithConsumption) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { name: 'presence_x' domain: 0 domain: 1 }
    variables { name: 'x' domain: 0 domain: 1 }
    variables { name: 'presence_y' domain: 0 domain: 1 }
    variables { name: 'y' domain: 0 domain: 1 }
    constraints {
      reservoir {
        time_exprs { vars: 1 coeffs: 1 }
        time_exprs { vars: 3 coeffs: 1 }
        level_changes: { offset: 1 }
        level_changes: { offset: -1 }
        active_literals: 0
        active_literals: 2
        min_level: 0
        max_level: 2
      }
    }
    constraints {
      enforcement_literal: -1
      linear { vars: 1 coeffs: 1 domain: 0 domain: 0 }
    }
    constraints {
      enforcement_literal: -3
      linear { vars: 3 coeffs: 1 domain: 0 domain: 0 }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  EXPECT_EQ(6, solutions.size());
}

TEST(ReservoirExpandTest, FalseActive) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { name: "x12" domain: 0 domain: 1 }
    variables { name: "start" domain: 0 domain: 0 }
    variables { name: "fill_time_2" domain: 2 domain: 2 }
    variables { name: "empty_time_8" domain: 12 domain: 12 }
    variables { domain: 1 domain: 1 }
    constraints {
      reservoir {
        max_level: 20
        time_exprs { vars: 1 coeffs: 1 }
        time_exprs { vars: 2 coeffs: 1 }
        time_exprs { vars: 3 coeffs: 1 }
        level_changes: { offset: 10 }
        level_changes: { offset: 5 }
        level_changes: { offset: -3 }
        active_literals: 4
        active_literals: 1
        active_literals: 0
      }
    }
  )pb");
  const CpSolverResponse response = Solve(initial_model);
  EXPECT_EQ(OPTIMAL, response.status());
}

TEST(ReservoirExpandTest, ExpandReservoirUsingCircuitPreservesSolutionHint) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      reservoir {
        max_level: 2
        time_exprs { offset: 1 }
        time_exprs { offset: 1 }
        time_exprs { offset: 1 }
        time_exprs { offset: 2 }
        time_exprs { offset: 3 }
        level_changes: { offset: -1 }
        level_changes: { offset: -1 }
        level_changes: { offset: 2 }
        level_changes: { offset: -2 }
        level_changes: { offset: 1 }
        active_literals: 0
        active_literals: 0
        active_literals: 0
        active_literals: 1
        active_literals: 0
      }
    }
    solution_hint {
      vars: [ 0, 1 ]
      values: [ 1, 0 ]
    }
  )pb");

  SatParameters params;
  params.set_expand_reservoir_using_circuit(true);
  params.set_log_search_progress(true);
  params.set_debug_crash_if_presolve_breaks_hint(true);
  CpSolverResponse response = SolveWithParameters(initial_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(IntModExpandTest, FzTest) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { name: 'x' domain: 50 domain: 60 }
    variables { name: 'y' domain: 1 domain: 5 }
    variables { name: 'mod' domain: 5 domain: 5 }
    constraints {
      int_mod {
        target { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  EXPECT_EQ(8, solutions.size());
}

TEST(IntModExpandTest, FzTestVariableMod) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { name: 'x' domain: 50 domain: 60 }
    variables { name: 'y' domain: 1 domain: 5 }
    variables { name: 'mod' domain: 4 domain: 5 }
    constraints {
      int_mod {
        target { vars: 1 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> found_solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &found_solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  absl::btree_set<std::vector<int>> expected{
      {50, 2, 4}, {51, 1, 5}, {51, 3, 4}, {52, 2, 5}, {53, 1, 4}, {53, 3, 5},
      {54, 2, 4}, {54, 4, 5}, {55, 3, 4}, {56, 1, 5}, {57, 1, 4}, {57, 2, 5},
      {58, 2, 4}, {58, 3, 5}, {59, 3, 4}, {59, 4, 5}};
  EXPECT_EQ(found_solutions, expected);
  EXPECT_EQ(16, found_solutions.size());
}

TEST(IntModExpandTest, Issue2420) {
  CpModelProto initial_model = ParseTestProto(R"pb(
    variables { name: "b" domain: 0 domain: 65535 }
    variables { domain: 192 domain: 192 }
    variables { name: "x" domain: 127 domain: 137 }
    constraints {
      int_mod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> found_solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &found_solutions);
  EXPECT_EQ(CpSolverStatus::OPTIMAL, response.status());
  absl::btree_set<std::vector<int>> expected{
      {55, 192, 137}, {56, 192, 136}, {57, 192, 135}, {58, 192, 134},
      {59, 192, 133}, {60, 192, 132}, {61, 192, 131}, {62, 192, 130},
      {63, 192, 129}, {64, 192, 128}, {65, 192, 127}};
  EXPECT_EQ(found_solutions, expected);
  EXPECT_EQ(11, found_solutions.size());
}

TEST(IntModExpandTest, VariableMod) {
  CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 10 }
    variables { domain: 3 domain: 10 }
    variables { domain: 1 domain: 4 }
    constraints {
      int_mod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> found_solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &found_solutions);
  EXPECT_EQ(CpSolverStatus::OPTIMAL, response.status());
  absl::btree_set<std::vector<int>> expected{
      {0, 3, 1}, {0, 3, 3},  {0, 4, 1}, {0, 4, 2},  {0, 4, 4},  {0, 5, 1},
      {0, 6, 1}, {0, 6, 2},  {0, 6, 3}, {0, 7, 1},  {0, 8, 1},  {0, 8, 2},
      {0, 8, 4}, {0, 9, 1},  {0, 9, 3}, {0, 10, 1}, {0, 10, 2}, {1, 3, 2},
      {1, 4, 3}, {1, 5, 2},  {1, 5, 4}, {1, 7, 2},  {1, 7, 3},  {1, 9, 2},
      {1, 9, 4}, {1, 10, 3}, {2, 5, 3}, {2, 6, 4},  {2, 8, 3},  {2, 10, 4},
      {3, 3, 4}, {3, 7, 4}};
  EXPECT_EQ(found_solutions, expected);
  EXPECT_EQ(32, found_solutions.size());
}

TEST(IntModExpansionTest, ExpandIntModPreservesSolutionHint) {
  CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 30 ] }
    variables { domain: [ 1, 10 ] }
    constraints {
      int_mod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
    objective {
      vars: [ 0 ]
      coeffs: [ 1 ]
    }
    solution_hint {
      vars: [ 0, 1, 2 ]
      values: [ 5, 26, 7 ]
    }
  )pb");

  SatParameters params;
  params.set_log_search_progress(true);
  params.set_debug_crash_if_presolve_breaks_hint(true);
  CpSolverResponse response = SolveWithParameters(initial_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(IntProdExpandTest, LeftCase) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { name: 'x' domain: -50 domain: -40 domain: 10 domain: 20 }
    variables { name: 'y' domain: 0 domain: 1 }
    variables { name: 'p' domain: -100 domain: 100 }
    constraints {
      int_prod {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  EXPECT_EQ(44, solutions.size());
}

TEST(IntProdExpandTest, RightCase) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { name: 'x' domain: -50 domain: -40 domain: 10 domain: 20 }
    variables { name: 'y' domain: 0 domain: 1 }
    variables { name: 'p' domain: -100 domain: 100 }
    constraints {
      int_prod {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  EXPECT_EQ(44, solutions.size());
}

TEST(IntProdExpandTest, LeftAcrossZero) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { name: 'x' domain: -6 domain: 6 }
    variables { name: 'y' domain: 2 domain: 4 }
    variables { name: 'p' domain: -30 domain: 30 }
    constraints {
      int_prod {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  absl::btree_set<std::vector<int>> expected{
      {-6, 2, -12}, {-6, 3, -18}, {-6, 4, -24}, {-5, 2, -10}, {-5, 3, -15},
      {-5, 4, -20}, {-4, 2, -8},  {-4, 3, -12}, {-4, 4, -16}, {-3, 2, -6},
      {-3, 3, -9},  {-3, 4, -12}, {-2, 2, -4},  {-2, 3, -6},  {-2, 4, -8},
      {-1, 2, -2},  {-1, 3, -3},  {-1, 4, -4},  {0, 2, 0},    {0, 3, 0},
      {0, 4, 0},    {1, 2, 2},    {1, 3, 3},    {1, 4, 4},    {2, 2, 4},
      {2, 3, 6},    {2, 4, 8},    {3, 2, 6},    {3, 3, 9},    {3, 4, 12},
      {4, 2, 8},    {4, 3, 12},   {4, 4, 16},   {5, 2, 10},   {5, 3, 15},
      {5, 4, 20},   {6, 2, 12},   {6, 3, 18},   {6, 4, 24},
  };
  EXPECT_EQ(solutions.size(), 13 * 3);
  EXPECT_EQ(solutions, expected);
}

TEST(IntProdExpandTest, TestLargerArity) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { name: 'x' domain: -6 domain: 6 }
    variables { name: 'y' domain: 2 domain: 4 }
    variables { name: 'z' domain: 1 domain: 2 }
    variables { name: 'p' domain: -30 domain: 30 }
    constraints {
      int_prod {
        target { vars: 3 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  const Domain dx = ReadDomainFromProto(initial_model.variables(0));
  const Domain dy = ReadDomainFromProto(initial_model.variables(1));
  const Domain dz = ReadDomainFromProto(initial_model.variables(2));
  const Domain dp = ReadDomainFromProto(initial_model.variables(3));

  absl::btree_set<std::vector<int>> expected;
  for (const int vx : dx.Values()) {
    for (const int vy : dy.Values()) {
      for (const int vz : dz.Values()) {
        if (dp.Contains(vx * vy * vz)) {
          expected.insert(std::vector<int>{vx, vy, vz, vx * vy * vz});
        }
      }
    }
  }

  EXPECT_EQ(solutions, expected);
}

TEST(IntProdExpandTest, TestLargerAffineProd) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { name: 'x' domain: -6 domain: 6 }
    variables { name: 'y' domain: 2 domain: 4 }
    variables { name: 'z' domain: 1 domain: 2 }
    variables { name: 'p' domain: -30 domain: 30 }
    constraints {
      int_prod {
        target { vars: 3 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 offset: 2 }
        exprs { vars: 0 coeffs: 3 offset: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 1 coeffs: 2 offset: -1 }
        exprs { vars: 1 coeffs: 3 }
        exprs { vars: 2 coeffs: 1 offset: 1 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());
  const Domain dx = ReadDomainFromProto(initial_model.variables(0));
  const Domain dy = ReadDomainFromProto(initial_model.variables(1));
  const Domain dz = ReadDomainFromProto(initial_model.variables(2));
  const Domain dp = ReadDomainFromProto(initial_model.variables(3));

  absl::btree_set<std::vector<int>> expected;
  for (const int vx : dx.Values()) {
    for (const int vy : dy.Values()) {
      for (const int vz : dz.Values()) {
        const int p = (vx + 2) * (2 * vx + 1) * vy * (2 * vy - 1) * (3 * vy) *
                      (2 * vz + 1);
        if (dp.Contains(p)) {
          expected.insert(std::vector<int>{vx, vy, vz, p});
        }
      }
    }
  }

  EXPECT_EQ(solutions, expected);
}

TEST(IntProdExpansionTest, ExpandNonBinaryIntProdPreservesSolutionHint) {
  CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 500 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      int_prod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        exprs { vars: 3 coeffs: 1 }
        exprs { vars: 4 coeffs: 1 }
      }
    }
    objective {
      vars: [ 0 ]
      coeffs: [ 1 ]
    }
    solution_hint {
      vars: [ 0, 1, 2, 3, 4 ]
      values: [ 360, 3, 4, 5, 6 ]
    }
  )pb");

  SatParameters params;
  params.set_log_search_progress(true);
  params.set_debug_crash_if_presolve_breaks_hint(true);
  CpSolverResponse response = SolveWithParameters(initial_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(ElementExpandTest, ConstantArray) {
  CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ -1, 5 ] }
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 3, 3 ] }
    variables { domain: [ 4, 4 ] }
    variables { domain: [ 5, 5 ] }
    variables { domain: [ 0, 7 ] }
    constraints {
      element {
        index: 0,
        vars: [ 1, 2, 3, 4, 1 ],
        target: 5
      }
    }
  )pb");

  absl::btree_set<std::vector<int>> found_solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &found_solutions);
  absl::btree_set<std::vector<int>> expected{
      {0, 1, 3, 4, 5, 1}, {1, 1, 3, 4, 5, 3}, {2, 1, 3, 4, 5, 4},
      {3, 1, 3, 4, 5, 5}, {4, 1, 3, 4, 5, 1},
  };
  EXPECT_EQ(found_solutions, expected);
}

TEST(AutomatonExpandTest, NonogramRule) {
  // Accept sequences with 3 '1', then 2 '1', then 1 '1', separated by at least
  // one '0'.
  CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      automaton {
        starting_state: 1,
        final_states: [ 9 ],
        transition_tail: [ 1, 1, 2, 3, 4, 5, 5, 6, 7, 8, 8, 9 ],
        transition_head: [ 1, 2, 3, 4, 5, 5, 6, 7, 8, 8, 9, 9 ],
        transition_label: [ 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 0 ],
        vars: [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ],
      }
    }
    solution_hint {
      vars: [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ]
      values: [ 0, 0, 1, 1, 1, 0, 1, 1, 0, 1 ]
    }
  )pb");
  absl::btree_set<std::vector<int>> found_solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &found_solutions);
  absl::btree_set<std::vector<int>> expected{
      {0, 0, 1, 1, 1, 0, 1, 1, 0, 1}, {0, 1, 1, 1, 0, 0, 1, 1, 0, 1},
      {0, 1, 1, 1, 0, 1, 1, 0, 0, 1}, {0, 1, 1, 1, 0, 1, 1, 0, 1, 0},
      {1, 1, 1, 0, 0, 0, 1, 1, 0, 1}, {1, 1, 1, 0, 0, 1, 1, 0, 0, 1},
      {1, 1, 1, 0, 0, 1, 1, 0, 1, 0}, {1, 1, 1, 0, 1, 1, 0, 0, 0, 1},
      {1, 1, 1, 0, 1, 1, 0, 0, 1, 0}, {1, 1, 1, 0, 1, 1, 0, 1, 0, 0}};
  EXPECT_EQ(found_solutions, expected);
}

TEST(AutomatonExpandTest, Bug1753_1) {
  CpModelProto initial_model = ParseTestProto(R"pb(
    variables { name: "0" domain: 0 domain: 2 }
    variables { name: "1" domain: 0 domain: 2 }
    variables { name: "2" domain: 0 domain: 2 }
    constraints {
      automaton {
        starting_state: 1
        final_states: 1
        final_states: 2
        transition_tail: 1
        transition_tail: 2
        transition_head: 2
        transition_head: 1
        transition_label: 1
        transition_label: 2
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> found_solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &found_solutions);
  absl::btree_set<std::vector<int>> expected{{1, 2, 1}};
  EXPECT_EQ(found_solutions, expected);
}

TEST(AutomatonExpandTest, Bug1753_2) {
  CpModelProto initial_model = ParseTestProto(R"pb(
    variables { name: "0" domain: 0 domain: 2 }
    variables { name: "1" domain: 0 domain: 2 }
    variables { name: "2" domain: 0 domain: 2 }
    constraints { linear { vars: 2 coeffs: 1 domain: 1 domain: 1 } }
    constraints {
      automaton {
        starting_state: 1
        final_states: 1
        final_states: 2
        transition_tail: 1
        transition_tail: 0
        transition_tail: 1
        transition_tail: 2
        transition_tail: 0
        transition_tail: 2
        transition_head: 2
        transition_head: 2
        transition_head: 1
        transition_head: 1
        transition_head: 1
        transition_head: 2
        transition_label: 1
        transition_label: 1
        transition_label: 0
        transition_label: 2
        transition_label: 2
        transition_label: 0
        vars: 0
        vars: 1
        vars: 2
      }
    }
    solution_hint {
      vars: [ 0, 1, 2 ]
      values: [ 0, 0, 1 ]
    }
  )pb");
  absl::btree_set<std::vector<int>> found_solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &found_solutions);
  absl::btree_set<std::vector<int>> expected{{0, 0, 1}, {1, 2, 1}};
  EXPECT_EQ(found_solutions, expected);
}

TEST(AutomatonExpandTest, EverythingZero) {
  CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      automaton {
        starting_state: 1,
        final_states: [ 1 ],
        transition_tail: 1,
        transition_head: 1,
        transition_label: 0,
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        exprs { vars: 3 coeffs: 1 }
        exprs { vars: 4 coeffs: 1 }
        exprs { vars: 5 coeffs: 1 }
      }
    }
  )pb");
  Model model;
  PresolveContext context(&model, &initial_model, nullptr);
  ExpandCpModel(&context);

  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 0 }
    variables { domain: 0 domain: 0 }
    variables { domain: 0 domain: 0 }
    variables { domain: 0 domain: 0 }
    variables { domain: 0 domain: 0 }
    variables { domain: 0 domain: 0 }
    constraints {}
  )pb");
  EXPECT_THAT(initial_model, testing::EqualsProto(expected_model));
}

TEST(AutomatonExpandTest, LoopingAutomatonMultipleFinalStates) {
  // These tuples accept "0*(12)+0*".
  CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      automaton {
        starting_state: 1,
        final_states: [ 3, 4 ],
        transition_tail: [ 1, 1, 2, 3, 3, 4 ],
        transition_head: [ 1, 2, 3, 2, 4, 4 ],
        transition_label: [ 0, 1, 2, 1, 0, 0 ],
        vars: [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ],
      }
    }
    solution_hint {
      vars: [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ]
      values: [ 1, 2, 1, 2, 1, 2, 1, 2, 1, 2 ]
    }
  )pb");
  absl::btree_set<std::vector<int>> found_solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &found_solutions);
  EXPECT_EQ(CpSolverStatus::OPTIMAL, response.status());
  absl::btree_set<std::vector<int>> expected{
      {0, 0, 0, 0, 0, 0, 0, 0, 1, 2}, {0, 0, 0, 0, 0, 0, 0, 1, 2, 0},
      {0, 0, 0, 0, 0, 0, 1, 2, 0, 0}, {0, 0, 0, 0, 0, 0, 1, 2, 1, 2},
      {0, 0, 0, 0, 0, 1, 2, 0, 0, 0}, {0, 0, 0, 0, 0, 1, 2, 1, 2, 0},
      {0, 0, 0, 0, 1, 2, 0, 0, 0, 0}, {0, 0, 0, 0, 1, 2, 1, 2, 0, 0},
      {0, 0, 0, 0, 1, 2, 1, 2, 1, 2}, {0, 0, 0, 1, 2, 0, 0, 0, 0, 0},
      {0, 0, 0, 1, 2, 1, 2, 0, 0, 0}, {0, 0, 0, 1, 2, 1, 2, 1, 2, 0},
      {0, 0, 1, 2, 0, 0, 0, 0, 0, 0}, {0, 0, 1, 2, 1, 2, 0, 0, 0, 0},
      {0, 0, 1, 2, 1, 2, 1, 2, 0, 0}, {0, 0, 1, 2, 1, 2, 1, 2, 1, 2},
      {0, 1, 2, 0, 0, 0, 0, 0, 0, 0}, {0, 1, 2, 1, 2, 0, 0, 0, 0, 0},
      {0, 1, 2, 1, 2, 1, 2, 0, 0, 0}, {0, 1, 2, 1, 2, 1, 2, 1, 2, 0},
      {1, 2, 0, 0, 0, 0, 0, 0, 0, 0}, {1, 2, 1, 2, 0, 0, 0, 0, 0, 0},
      {1, 2, 1, 2, 1, 2, 0, 0, 0, 0}, {1, 2, 1, 2, 1, 2, 1, 2, 0, 0},
      {1, 2, 1, 2, 1, 2, 1, 2, 1, 2}};
  EXPECT_EQ(found_solutions, expected);
  EXPECT_EQ(25, found_solutions.size());
}

TEST(AutomatonExpandTest, LoopingAutomatonMultipleFinalStatesNegatedVariables) {
  // These automaton accept "0*(12)+0*".
  CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ -2, 0 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      automaton {
        starting_state: 1,
        final_states: [ 3, 4 ],
        transition_tail: [ 1, 1, 2, 3, 3, 4 ],
        transition_head: [ 1, 2, 3, 2, 4, 4 ],
        transition_label: [ 0, 1, 2, 1, 0, 0 ],
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: -1 }
        exprs { vars: 2 coeffs: 1 }
        exprs { vars: 3 coeffs: 1 }
        exprs { vars: 4 coeffs: 1 }
        exprs { vars: 5 coeffs: 1 }
        exprs { vars: 6 coeffs: 1 }
        exprs { vars: 7 coeffs: 1 }
        exprs { vars: 8 coeffs: 1 }
        exprs { vars: 9 coeffs: 1 }
      }
    }
    solution_hint {
      vars: [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ]
      values: [ 1, -2, 1, 2, 1, 2, 1, 2, 1, 2 ]
    }
  )pb");
  absl::btree_set<std::vector<int>> found_solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &found_solutions);
  EXPECT_EQ(CpSolverStatus::OPTIMAL, response.status());

  absl::btree_set<std::vector<int>> expected{
      {0, 0, 0, 0, 0, 0, 0, 0, 1, 2},  {0, 0, 0, 0, 0, 0, 0, 1, 2, 0},
      {0, 0, 0, 0, 0, 0, 1, 2, 0, 0},  {0, 0, 0, 0, 0, 0, 1, 2, 1, 2},
      {0, 0, 0, 0, 0, 1, 2, 0, 0, 0},  {0, 0, 0, 0, 0, 1, 2, 1, 2, 0},
      {0, 0, 0, 0, 1, 2, 0, 0, 0, 0},  {0, 0, 0, 0, 1, 2, 1, 2, 0, 0},
      {0, 0, 0, 0, 1, 2, 1, 2, 1, 2},  {0, 0, 0, 1, 2, 0, 0, 0, 0, 0},
      {0, 0, 0, 1, 2, 1, 2, 0, 0, 0},  {0, 0, 0, 1, 2, 1, 2, 1, 2, 0},
      {0, 0, 1, 2, 0, 0, 0, 0, 0, 0},  {0, 0, 1, 2, 1, 2, 0, 0, 0, 0},
      {0, 0, 1, 2, 1, 2, 1, 2, 0, 0},  {0, 0, 1, 2, 1, 2, 1, 2, 1, 2},
      {0, -1, 2, 0, 0, 0, 0, 0, 0, 0}, {0, -1, 2, 1, 2, 0, 0, 0, 0, 0},
      {0, -1, 2, 1, 2, 1, 2, 0, 0, 0}, {0, -1, 2, 1, 2, 1, 2, 1, 2, 0},
      {1, -2, 0, 0, 0, 0, 0, 0, 0, 0}, {1, -2, 1, 2, 0, 0, 0, 0, 0, 0},
      {1, -2, 1, 2, 1, 2, 0, 0, 0, 0}, {1, -2, 1, 2, 1, 2, 1, 2, 0, 0},
      {1, -2, 1, 2, 1, 2, 1, 2, 1, 2}};
  EXPECT_EQ(found_solutions, expected);
}

TEST(AutomatonExpandTest, AnotherAutomaton) {
  // This accept everything that does not contain 4 consecutives 1 or 4
  // consecutives 2.
  CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      automaton {
        starting_state: 1,
        final_states: [ 1, 2, 3, 4, 5, 6, 7 ],
        transition_tail: [ 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7 ],
        transition_head: [ 2, 5, 3, 5, 4, 5, 0, 5, 2, 6, 2, 7, 2, 0 ],
        transition_label: [ 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2 ],
        vars: [ 0, 1, 2, 3, 4, 5, 6 ],
      }
    }
    solution_hint {
      vars: [ 0, 1, 2, 3, 4, 5, 6 ]
      values: [ 1, 1, 1, 2, 2, 2, 1 ]
    }
  )pb");
  absl::btree_set<std::vector<int>> found_solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &found_solutions);

  EXPECT_EQ(CpSolverStatus::OPTIMAL, response.status());

  // Out of the 2**7 tuples, the one that contains 4 consecutive 1 are:
  // - 1111??? (8)
  // - 21111?? (4)
  // - ?21111? (4)
  // - ??21111 (4)
  EXPECT_EQ(128 - 2 * 20, found_solutions.size());
}

TEST(ExpandTableTest, EnumerationAndEncoding) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 4 ] }
    constraints {
      table {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        values: 0
        values: 1
      }
    }
    constraints {
      table {
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 3 coeffs: 1 }
        values: 4
        values: 0
      }
    }
    constraints {
      table {
        exprs { vars: 2 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        values: 1
        values: 4
      }
    }
  )pb");

  Model model;
  model.Add(NewSatParameters("enumerate_all_solutions:true"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << gtl::LogContainer(response.solution());
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);

  // There should be just one solution [0, 4, 1, 0], but the solver used to
  // report more because of extra "free" variable used in the encoding.
  EXPECT_EQ(count, 1);
}

TEST(ExpandTableTest, EnumerationAndEncodingTwoVars) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables {
      name: "X1"
      domain: [ 0, 4 ]
    }
    variables {
      name: "X3"
      domain: [ 0, 4 ]
    }
    constraints {
      table {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        values: [ 0, 0, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4 ]
      }
    }
  )pb");

  Model model;
  model.Add(NewSatParameters("enumerate_all_solutions:true"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << gtl::LogContainer(response.solution());
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(count, 7);
}

TEST(ExpandTableTest, EnumerationAndEncodingFullPrefix) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      table {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        values: [
          0, 0, 0, 0, 1, 1, 0, 2, 2, 1, 0, 1, 1, 1,
          2, 1, 2, 0, 2, 0, 2, 2, 1, 0, 2, 2, 1
        ]
      }
    }
  )pb");

  Model model;
  model.Add(NewSatParameters("enumerate_all_solutions:true"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << gtl::LogContainer(response.solution());
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);

  EXPECT_EQ(count, 9);
}

TEST(ExpandTableTest, EnumerationAndEncodingPartialPrefix) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      table {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        values: [
          0, 0, 0, 0, 2, 2, 1, 0, 1, 1, 1, 2, 1, 2, 0, 2, 0, 2, 2, 1, 0
        ]
      }
    }
  )pb");

  Model model;
  model.Add(NewSatParameters("enumerate_all_solutions:true"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << gtl::LogContainer(response.solution());
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);

  EXPECT_EQ(count, 7);
}

TEST(ExpandTableTest, EnumerationAndEncodingInvalidTuples) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      table {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        values: [
          0, 0, 4, 0, 2, 2, 1, 0, 1, 1, 1, 2, 1, 2, 0, 2, 0, 2, 2, 1, 4
        ]
      }
    }
  )pb");

  Model model;
  model.Add(
      NewSatParameters("enumerate_all_solutions:true,cp_model_presolve:false"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << gtl::LogContainer(response.solution());
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);

  // There should be exactly one solution per valid tuple.
  EXPECT_EQ(count, 5);
}

TEST(ExpandTableTest, EnumerationAndEncodingOneTupleWithAny) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    constraints {
      table {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        values: [ 1, 0, 2, 1, 1, 2, 1, 2, 2 ]
      }
    }
  )pb");

  Model model;
  model.Add(
      NewSatParameters("enumerate_all_solutions:true,cp_model_presolve:false"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << gtl::LogContainer(response.solution());
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);

  EXPECT_EQ(count, 3);
}

TEST(ExpandTableTest, EnumerationAndEncodingPrefixWithLargeNegatedPart) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 5 ] }
    constraints {
      table {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        values: [ 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5 ]
      }
    }
  )pb");

  Model model;
  model.Add(
      NewSatParameters("enumerate_all_solutions:true,cp_model_presolve:false"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << gtl::LogContainer(response.solution());
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);

  EXPECT_EQ(count, 6);
}

TEST(ExpandTableTest, EnforcedPositiveTable) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 1, 3 ] }
    variables { domain: [ 1, 3 ] }
    variables { domain: [ 1, 3 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 3 ]
      table {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        values: [ 1, 2, 3, 2, 2, 2, 3, 2, 1 ]
      }
    }
  )pb");

  Model model;
  model.Add(
      NewSatParameters("enumerate_all_solutions:true,cp_model_presolve:false"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << gtl::LogContainer(response.solution());
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);

  EXPECT_EQ(count, 30);
}

TEST(ExpandTableTest, EnforcedPositiveEmptyTable) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 1, 3 ] }
    variables { domain: [ 1, 3 ] }
    variables { domain: [ 1, 3 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 3 ]
      table {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        values: []
      }
    }
  )pb");

  Model model;
  model.Add(
      NewSatParameters("enumerate_all_solutions:true,cp_model_presolve:false"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << gtl::LogContainer(response.solution());
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);

  EXPECT_EQ(count, 27);
}

TEST(ExpandTableTest, DualEnforcedPositiveTable) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 1, 3 ] }
    variables { domain: [ 1, 3 ] }
    variables { domain: [ 1, 3 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 3, 4 ]
      table {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        values: [ 1, 2, 3, 2, 2, 2, 3, 2, 1 ]
      }
    }
  )pb");

  Model model;
  model.Add(NewSatParameters("enumerate_all_solutions:true"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << gtl::LogContainer(response.solution());
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);

  EXPECT_EQ(count, 84);
}

TEST(ExpandTableTest, EnforcedNegativeTable) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 1, 3 ] }
    variables { domain: [ 1, 3 ] }
    variables { domain: [ 1, 3 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 3 ]
      table {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        values: [ 1, 2, 3, 2, 2, 2, 3, 2, 1 ]
        negated: true
      }
    }
  )pb");

  Model model;
  model.Add(
      NewSatParameters("enumerate_all_solutions:true,cp_model_presolve:false"));
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << gtl::LogContainer(response.solution());
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);

  EXPECT_EQ(count, 51);
}

TEST(ExpandTableTest, UnsatTable) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 5, 9 ] }
    constraints { table { vars: 0 vars: 1 values: 3 values: 3 } }
  )pb");

  Model model;
  model.Add(NewSatParameters("cp_model_presolve:false"));
  const CpSolverResponse response = SolveCpModel(model_proto, &model);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(ExpandTableTest, UnsatNegatedTable) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    constraints {
      table {
        exprs { vars: 0 coeffs: 1 }
        values: [ 0, 1 ]
        negated: true
      }
    }
  )pb");

  const CpSolverResponse response = Solve(model_proto);
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(ExpandAllDiffTest, Permutation) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      all_diff {
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");

  absl::btree_set<std::vector<int>> found_solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "presolve_cp_model:false", &found_solutions);
  absl::btree_set<std::vector<int>> expected{
      {0, 2, 1}, {2, 0, 1}, {1, 2, 0}, {2, 1, 0}};
  EXPECT_EQ(found_solutions, expected);
}

TEST(ExpandInverseTest, CountInvolution) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      inverse {
        f_direct: [ 0, 1, 2 ]
        f_inverse: [ 0, 1, 2 ]
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());

  // On 3 elements, we either have the identity or one of the 3 two cycle.
  EXPECT_EQ(4, solutions.size());
}

TEST(ExpandInverseTest, DuplicateAtDifferentPosition) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      inverse {
        f_direct: [ 0, 1, 2, 3 ]
        f_inverse: [ 4, 5, 6, 0 ]
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> solutions;
  const CpSolverResponse response =
      SolveAndCheck(initial_model, "", &solutions);
  EXPECT_EQ(OPTIMAL, response.status());

  // f(0) = 1 has 2 solutions, same with f(0) = 2.
  EXPECT_EQ(4, solutions.size());
}

TEST(ExpandSmallLinearTest, ReplaceNonEqual) {
  CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 4, 6, 10 ]
      }
    }
  )pb");
  Model model;
  PresolveContext context(&model, &initial_model, nullptr);
  context.InitializeNewDomains();
  context.InsertVarValueEncoding(2, 0, 0);
  context.InsertVarValueEncoding(3, 0, 1);
  context.InsertVarValueEncoding(4, 0, 2);
  context.InsertVarValueEncoding(5, 0, 3);
  context.InsertVarValueEncoding(6, 0, 4);
  context.InsertVarValueEncoding(7, 0, 5);
  context.InsertVarValueEncoding(8, 1, 0);
  context.InsertVarValueEncoding(9, 1, 1);
  context.InsertVarValueEncoding(10, 1, 2);
  context.InsertVarValueEncoding(11, 1, 3);
  context.InsertVarValueEncoding(12, 1, 4);
  context.InsertVarValueEncoding(13, 1, 5);
  ExpandCpModel(&context);

  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 5 }
    variables { domain: 0 domain: 5 }
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    variables { domain: 0 domain: 1 }
    constraints {}
    constraints {
      enforcement_literal: 2
      linear { vars: 0 coeffs: 1 domain: 0 domain: 0 }
    }
    constraints {
      enforcement_literal: -3
      linear {
        vars: 0
        coeffs: 1
        domain: -9223372036854775808
        domain: -1
        domain: 1
        domain: 9223372036854775807
      }
    }
    constraints {
      enforcement_literal: 3
      linear { vars: 0 coeffs: 1 domain: 1 domain: 1 }
    }
    constraints {
      enforcement_literal: -4
      linear {
        vars: 0
        coeffs: 1
        domain: -9223372036854775808
        domain: 0
        domain: 2
        domain: 9223372036854775807
      }
    }
    constraints {
      enforcement_literal: 4
      linear { vars: 0 coeffs: 1 domain: 2 domain: 2 }
    }
    constraints {
      enforcement_literal: -5
      linear {
        vars: 0
        coeffs: 1
        domain: -9223372036854775808
        domain: 1
        domain: 3
        domain: 9223372036854775807
      }
    }
    constraints {
      enforcement_literal: 5
      linear { vars: 0 coeffs: 1 domain: 3 domain: 3 }
    }
    constraints {
      enforcement_literal: -6
      linear {
        vars: 0
        coeffs: 1
        domain: -9223372036854775808
        domain: 2
        domain: 4
        domain: 9223372036854775807
      }
    }
    constraints {
      enforcement_literal: 6
      linear { vars: 0 coeffs: 1 domain: 4 domain: 4 }
    }
    constraints {
      enforcement_literal: -7
      linear {
        vars: 0
        coeffs: 1
        domain: -9223372036854775808
        domain: 3
        domain: 5
        domain: 9223372036854775807
      }
    }
    constraints {
      enforcement_literal: 7
      linear { vars: 0 coeffs: 1 domain: 5 domain: 5 }
    }
    constraints {
      enforcement_literal: -8
      linear {
        vars: 0
        coeffs: 1
        domain: -9223372036854775808
        domain: 4
        domain: 6
        domain: 9223372036854775807
      }
    }
    constraints {
      enforcement_literal: 8
      linear { vars: 1 coeffs: 1 domain: 0 domain: 0 }
    }
    constraints {
      enforcement_literal: -9
      linear {
        vars: 1
        coeffs: 1
        domain: -9223372036854775808
        domain: -1
        domain: 1
        domain: 9223372036854775807
      }
    }
    constraints {
      enforcement_literal: 9
      linear { vars: 1 coeffs: 1 domain: 1 domain: 1 }
    }
    constraints {
      enforcement_literal: -10
      linear {
        vars: 1
        coeffs: 1
        domain: -9223372036854775808
        domain: 0
        domain: 2
        domain: 9223372036854775807
      }
    }
    constraints {
      enforcement_literal: 10
      linear { vars: 1 coeffs: 1 domain: 2 domain: 2 }
    }
    constraints {
      enforcement_literal: -11
      linear {
        vars: 1
        coeffs: 1
        domain: -9223372036854775808
        domain: 1
        domain: 3
        domain: 9223372036854775807
      }
    }
    constraints {
      enforcement_literal: 11
      linear { vars: 1 coeffs: 1 domain: 3 domain: 3 }
    }
    constraints {
      enforcement_literal: -12
      linear {
        vars: 1
        coeffs: 1
        domain: -9223372036854775808
        domain: 2
        domain: 4
        domain: 9223372036854775807
      }
    }
    constraints {
      enforcement_literal: 12
      linear { vars: 1 coeffs: 1 domain: 4 domain: 4 }
    }
    constraints {
      enforcement_literal: -13
      linear {
        vars: 1
        coeffs: 1
        domain: -9223372036854775808
        domain: 3
        domain: 5
        domain: 9223372036854775807
      }
    }
    constraints {
      enforcement_literal: 13
      linear { vars: 1 coeffs: 1 domain: 5 domain: 5 }
    }
    constraints {
      enforcement_literal: -14
      linear {
        vars: 1
        coeffs: 1
        domain: -9223372036854775808
        domain: 4
        domain: 6
        domain: 9223372036854775807
      }
    }
    constraints { bool_or { literals: -3 literals: -14 } }
    constraints { bool_or { literals: -4 literals: -13 } }
    constraints { bool_or { literals: -5 literals: -12 } }
    constraints { bool_or { literals: -6 literals: -11 } }
    constraints { bool_or { literals: -7 literals: -10 } }
    constraints { bool_or { literals: -8 literals: -9 } }
  )pb");
  EXPECT_THAT(initial_model, testing::EqualsProto(expected_model));
}

TEST(TableExpandTest, UsedToFail) {
  const CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    constraints {
      table {
        vars: [ 0, 4, 1, 5 ]
        values: [ 0, 0, 2, 2 ]
        values: [ 1, 0, 3, 0 ]
        values: [ 2, 2, 0, 0 ]
        values: [ 3, 0, 1, 0 ]
      }
    }
    constraints {
      table {
        vars: [ 1, 5, 3, 6 ]
        values: [ 0, 0, 2, 2 ]
        values: [ 1, 0, 3, 0 ]
        values: [ 2, 2, 0, 0 ]
        values: [ 3, 0, 1, 0 ]
      }
    }
    constraints {
      table {
        vars: [ 2, 6, 3, 7 ]
        values: [ 0, 0, 2, 2 ]
        values: [ 1, 0, 3, 0 ]
        values: [ 2, 2, 0, 0 ]
        values: [ 3, 0, 1, 0 ]
      }
    }
    constraints {
      table {
        vars: [ 3, 7, 0, 4 ]
        values: [ 0, 0, 2, 2 ]
        values: [ 1, 0, 3, 0 ]
        values: [ 2, 2, 0, 0 ]
        values: [ 3, 0, 1, 0 ]
      }
    }
  )pb");

  SatParameters params;
  params.set_cp_model_presolve(false);
  const CpSolverResponse response = SolveWithParameters(initial_model, params);
  EXPECT_EQ(INFEASIBLE, response.status());
}

TEST(LinMaxExpansionTest, SimpleEnumeration) {
  CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 6 ] }
    constraints {
      lin_max {
        target { vars: 0 coeffs: 1 offset: 1 }
        exprs { vars: 1 coeffs: 2 }
        exprs: { vars: 2 coeffs: 1 offset: -3 }
      }
    }
  )pb");
  absl::btree_set<std::vector<int>> found_solutions;
  const CpSolverResponse response = SolveAndCheck(
      initial_model, "max_lin_max_size_for_expansion:4", &found_solutions);
  absl::btree_set<std::vector<int>> expected{
      {0, 0, 4}, {1, 0, 5}, {1, 1, 0}, {1, 1, 1}, {1, 1, 2}, {1, 1, 3},
      {1, 1, 4}, {1, 1, 5}, {2, 0, 6}, {2, 1, 6}, {3, 2, 0}, {3, 2, 1},
      {3, 2, 2}, {3, 2, 3}, {3, 2, 4}, {3, 2, 5}, {3, 2, 6}, {5, 3, 0},
      {5, 3, 1}, {5, 3, 2}, {5, 3, 3}, {5, 3, 4}, {5, 3, 5}, {5, 3, 6}};
  EXPECT_EQ(found_solutions, expected);
}

TEST(LinMaxExpansionTest, GoldenTest) {
  CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 6 ] }
    constraints {
      lin_max {
        target { vars: 0 coeffs: 1 offset: 1 }
        exprs { vars: 1 coeffs: 2 }
        exprs: { vars: 2 coeffs: 1 offset: -3 }
      }
    }
  )pb");
  Model model;
  model.GetOrCreate<SatParameters>()->set_max_lin_max_size_for_expansion(4);
  PresolveContext context(&model, &initial_model, nullptr);
  ExpandCpModel(&context);

  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 5 }
    variables { domain: 0 domain: 5 }
    variables { domain: 0 domain: 6 }
    variables { domain: 0 domain: 1 }
    constraints {}
    constraints {
      linear {
        vars: 0
        vars: 1
        coeffs: 1
        coeffs: -2
        domain: -1
        domain: 9223372036854775806
      }
    }
    constraints {
      linear {
        vars: 0
        vars: 2
        coeffs: 1
        coeffs: -1
        domain: -4
        domain: 9223372036854775803
      }
    }
    constraints {
      enforcement_literal: 3
      linear {
        vars: 0
        vars: 1
        coeffs: 1
        coeffs: -2
        domain: -9223372036854775808
        domain: -1
      }
    }
    constraints {
      enforcement_literal: -4
      linear {
        vars: 0
        vars: 2
        coeffs: 1
        coeffs: -1
        domain: -9223372036854775808
        domain: -4
      }
    }
  )pb");
  EXPECT_THAT(initial_model, testing::EqualsProto(expected_model));
}

TEST(LinMaxExpansionTest, ExpandLinMaxPreservesSolutionHint) {
  CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 4 ] }
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 5 ] }
    constraints {
      lin_max {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
        exprs { vars: 3 coeffs: 1 }
      }
    }
    objective {
      vars: [ 0 ]
      coeffs: [ 1 ]
    }
    solution_hint {
      vars: [ 0, 1, 2, 3 ]
      values: [ 3, 2, 3, 3 ]
    }
  )pb");

  SatParameters params;
  params.set_log_search_progress(true);
  params.set_max_lin_max_size_for_expansion(10);
  params.set_debug_crash_if_presolve_breaks_hint(true);
  CpSolverResponse response = SolveWithParameters(initial_model, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(FinalExpansionForLinearConstraintTest, ComplexLinearExpansion) {
  CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 2, 4, 6, 8, 10 ]
      }
    }
    solution_hint {
      vars: [ 0, 1 ]
      values: [ 1, 5 ]
    }
  )pb");
  Model model;
  PresolveContext context(&model, &initial_model, nullptr);

  context.InitializeNewDomains();
  context.LoadSolutionHint();

  FinalExpansionForLinearConstraint(&context);

  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {}
    constraints { bool_or { literals: [ 2, 3, 4 ] } }
    constraints {
      enforcement_literal: 2
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 2 ]
      }
    }
    constraints {
      enforcement_literal: 3
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 4, 6 ]
      }
    }
    constraints {
      enforcement_literal: 4
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 8, 10 ]
      }
    }
    solution_hint {
      vars: [ 0, 1 ]
      values: [ 1, 5 ]
    }
  )pb");
  EXPECT_THAT(initial_model, testing::EqualsProto(expected_model));

  // We should properly complete the hint and choose the bucket [4, 6].
  EXPECT_THAT(context.solution_crush().GetVarValues(),
              ::testing::ElementsAre(1, 5, 0, 1, 0));
  EXPECT_TRUE(context.DebugTestHintFeasibility());
}

TEST(FinalExpansionForLinearConstraintTest, ComplexLinearExpansionWithInteger) {
  CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 2, 4, 6, 8, 10 ]
      }
    }
    solution_hint {
      vars: [ 0, 1 ]
      values: [ 1, 5 ]
    }
  )pb");
  Model model;
  model.GetOrCreate<SatParameters>()
      ->set_encode_complex_linear_constraint_with_integer(true);
  PresolveContext context(&model, &initial_model, nullptr);

  context.InitializeNewDomains();
  context.LoadSolutionHint();

  FinalExpansionForLinearConstraint(&context);

  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 2, 4, 6, 8, 10 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 1, -1 ]
        domain: [ 0, 0 ]
      }
    }
    solution_hint {
      vars: [ 0, 1 ]
      values: [ 1, 5 ]
    }
  )pb");
  EXPECT_THAT(initial_model, testing::EqualsProto(expected_model));

  // We should properly complete the hint with the new slack variable.
  EXPECT_THAT(context.solution_crush().GetVarValues(),
              ::testing::ElementsAre(1, 5, 6));
  EXPECT_TRUE(context.DebugTestHintFeasibility());
}

TEST(FinalExpansionForLinearConstraintTest,
     ComplexLinearExpansionWithEnforcementLiterals) {
  CpModelProto initial_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 6 ] }
    variables { domain: [ 0, 6 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: [ 2, -4 ]
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 2, 4, 6 ]
      }
    }
    solution_hint {
      vars: [ 0, 1, 2, 3 ]
      values: [ 1, 5, 1, 0 ]
    }
  )pb");
  Model model;
  model.GetOrCreate<SatParameters>()->set_enumerate_all_solutions(true);
  PresolveContext context(&model, &initial_model, nullptr);

  context.InitializeNewDomains();
  context.LoadSolutionHint();

  FinalExpansionForLinearConstraint(&context);

  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 6 ] }
    variables { domain: [ 0, 6 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {}
    constraints { bool_or { literals: [ -3, 3, 4, 5 ] } }
    constraints {
      enforcement_literal: 4
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 2 ]
      }
    }
    constraints {
      enforcement_literal: 5
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 4, 6 ]
      }
    }
    constraints { bool_or { literals: -3 literals: 3 literals: 6 } }
    constraints {
      enforcement_literal: -3
      bool_and { literals: -7 }
    }
    constraints {
      enforcement_literal: 3
      bool_and { literals: -7 }
    }
    constraints {
      enforcement_literal: -7
      bool_and { literals: -5 }
    }
    constraints {
      enforcement_literal: -7
      bool_and { literals: -6 }
    }
    solution_hint {
      vars: [ 0, 1, 2, 3 ]
      values: [ 1, 5, 1, 0 ]
    }
  )pb");
  EXPECT_THAT(initial_model, testing::EqualsProto(expected_model));

  // We should properly complete the hint and choose the bucket [4, 6], as well
  // as set the new linear_is_enforced hint to true.
  EXPECT_THAT(context.solution_crush().GetVarValues(),
              ::testing::ElementsAre(1, 5, 1, 0, 0, 1, 1));
  EXPECT_TRUE(context.DebugTestHintFeasibility());
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
