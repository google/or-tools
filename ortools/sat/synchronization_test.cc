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

#include "ortools/sat/synchronization.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/time/time.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/util.h"
#include "ortools/util/random_engine.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::ElementsAre;
using ::testing::EqualsProto;
using ::testing::IsEmpty;

TEST(SharedSolutionRepository, Api) {
  SharedSolutionRepository<int64_t> repository(3);
  EXPECT_EQ(repository.NumSolutions(), 0);
  repository.Add({8, {1, 2}});
  repository.Add({1, {2, 3}});
  EXPECT_EQ(repository.NumSolutions(), 0);
  repository.Synchronize();
  EXPECT_EQ(repository.NumSolutions(), 2);
  EXPECT_EQ(repository.GetSolution(0)->rank, 1);
  EXPECT_EQ(repository.GetSolution(1)->rank, 8);
  EXPECT_EQ(repository.GetVariableValueInSolution(/*var_index=*/1,
                                                  /*solution_index=*/0),
            3);
  EXPECT_EQ(repository.GetVariableValueInSolution(/*var_index=*/0,
                                                  /*solution_index=*/1),
            1);

  repository.Add({3, {1, 2}});
  repository.Add({5, {1, 2}});
  repository.Add({1, {2, 3}});
  repository.Add({9, {1, 2}});
  EXPECT_EQ(repository.NumSolutions(), 2);
  repository.Synchronize();
  EXPECT_EQ(repository.NumSolutions(), 3);
  EXPECT_EQ(repository.GetSolution(0)->rank, 1);
  EXPECT_EQ(repository.GetSolution(1)->rank, 3);
  EXPECT_EQ(repository.GetSolution(2)->rank, 5);
  EXPECT_EQ(repository.GetVariableValueInSolution(/*var_index=*/1,
                                                  /*solution_index=*/0),
            3);
  EXPECT_EQ(repository.GetVariableValueInSolution(/*var_index=*/1,
                                                  /*solution_index=*/1),
            2);
}

TEST(SharedSolutionRepository, DuplicateSolutionAreMerged) {
  SharedSolutionRepository<int64_t> repository(3);
  EXPECT_EQ(repository.NumSolutions(), 0);
  repository.Add({1, {1, 50}});

  // In practice we shouldn't have the same variable values and different
  // objective, but the code don't care about this and just test the perfect
  // equality.
  repository.Add({5, {1, 50}});
  repository.Add({1, {1, 50}});
  EXPECT_EQ(repository.NumSolutions(), 0);
  repository.Synchronize();
  EXPECT_EQ(repository.NumSolutions(), 2);
  EXPECT_EQ(repository.GetSolution(0)->rank, 1);
  EXPECT_EQ(repository.GetSolution(1)->rank, 5);
}

TEST(SharedSolutionRepository, DuplicateSolutionAreMerged2) {
  SharedSolutionRepository<int64_t> repository(3);
  EXPECT_EQ(repository.NumSolutions(), 0);

  // All this should count as 1 solution.
  repository.Add({3, {1, 50}});
  repository.Add({3, {1, 50}});
  repository.Add({3, {1, 50}});
  repository.Add({3, {1, 50}});
  repository.Add({3, {1, 50}});

  // So we should be able to Enqueue worse ones.
  repository.Add({5, {1, 50}});
  repository.Add({6, {1, 50}});

  repository.Synchronize();
  EXPECT_EQ(repository.NumSolutions(), 3);
  EXPECT_EQ(repository.GetSolution(0)->rank, 3);
  EXPECT_EQ(repository.GetSolution(1)->rank, 5);
  EXPECT_EQ(repository.GetSolution(2)->rank, 6);
}

TEST(SharedSolutionRepository, GetRandomBiasedSolution) {
  SharedSolutionRepository<int64_t> repository(5);
  EXPECT_EQ(repository.NumSolutions(), 0);

  repository.Add({3, {1, 50}});
  repository.Add({3, {1, 51}});
  repository.Add({3, {1, 52}});

  // Enqueue worse ones.
  repository.Add({5, {1, 50}});
  repository.Add({6, {1, 50}});

  repository.Synchronize();
  EXPECT_EQ(repository.NumSolutions(), 5);

  // We select one of the solution with best objective.
  random_engine_t random(0);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(repository.GetRandomBiasedSolution(random)->rank, 3);
  }
}

TEST(SharedLPSolutionRepository, NewLPSolution) {
  SharedLPSolutionRepository lp_solutions(1);

  lp_solutions.NewLPSolution({1.0, 2.0, 1.0});

  lp_solutions.Synchronize();
  EXPECT_EQ(lp_solutions.NumSolutions(), 1);
  EXPECT_EQ(lp_solutions.GetSolution(0)->variable_values[0], 1.0);
  EXPECT_EQ(lp_solutions.GetSolution(0)->variable_values[1], 2.0);
  EXPECT_EQ(lp_solutions.GetSolution(0)->variable_values[2], 1.0);

  lp_solutions.NewLPSolution({2.0, 3.0, 0.0});

  lp_solutions.Synchronize();
  EXPECT_EQ(lp_solutions.NumSolutions(), 1);
  EXPECT_EQ(lp_solutions.GetSolution(0)->variable_values[0], 2.0);
  EXPECT_EQ(lp_solutions.GetSolution(0)->variable_values[1], 3.0);
  EXPECT_EQ(lp_solutions.GetSolution(0)->variable_values[2], 0.0);
}

TEST(SharedIncompleteSolutionManager, AddAndRemoveSolutions) {
  SharedIncompleteSolutionManager incomplete_solutions;

  EXPECT_FALSE(incomplete_solutions.HasSolution());
  incomplete_solutions.AddSolution({1.0, 2.0, 1.0});
  EXPECT_TRUE(incomplete_solutions.HasSolution());
  std::vector<double> solution = incomplete_solutions.PopLast();
  EXPECT_THAT(solution, ::testing::ElementsAre(1.0, 2.0, 1.0));

  EXPECT_FALSE(incomplete_solutions.HasSolution());
  incomplete_solutions.AddSolution({2.0, 3.0, 2.0});
  incomplete_solutions.AddSolution({3.0, 4.0, 3.0});
  EXPECT_TRUE(incomplete_solutions.HasSolution());
  solution = incomplete_solutions.PopLast();
  EXPECT_THAT(solution, ::testing::ElementsAre(3.0, 4.0, 3.0));

  EXPECT_TRUE(incomplete_solutions.HasSolution());
  solution = incomplete_solutions.PopLast();
  EXPECT_THAT(solution, ::testing::ElementsAre(2.0, 3.0, 2.0));

  EXPECT_FALSE(incomplete_solutions.HasSolution());
}

TEST(SharedBoundsManagerTest, Api) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 20 ] }
  )pb");
  SharedBoundsManager manager(model);
  manager.ReportPotentialNewBounds("auto", {2, 4, 6}, {1, 2, 3}, {11, 12, 13});
  manager.Synchronize();

  std::vector<int> vars;
  std::vector<int64_t> lbs;
  std::vector<int64_t> ubs;

  EXPECT_EQ(manager.RegisterNewId(), 0);
  manager.GetChangedBounds(0, &vars, &lbs, &ubs);
  EXPECT_THAT(vars, ElementsAre(2, 4));
  EXPECT_THAT(lbs, ElementsAre(1, 2));
  EXPECT_THAT(ubs, ElementsAre(11, 12));

  EXPECT_EQ(manager.RegisterNewId(), 1);
  manager.GetChangedBounds(1, &vars, &lbs, &ubs);
  EXPECT_THAT(vars, ElementsAre(2, 4));
  EXPECT_THAT(lbs, ElementsAre(1, 2));
  EXPECT_THAT(ubs, ElementsAre(11, 12));

  EXPECT_EQ(manager.RegisterNewId(), 2);
  manager.GetChangedBounds(2, &vars, &lbs, &ubs);
  EXPECT_THAT(vars, ElementsAre(2, 4));
  EXPECT_THAT(lbs, ElementsAre(1, 2));
  EXPECT_THAT(ubs, ElementsAre(11, 12));

  // Test nilpotence.
  manager.GetChangedBounds(2, &vars, &lbs, &ubs);
  EXPECT_THAT(vars, IsEmpty());
  EXPECT_THAT(lbs, IsEmpty());
  EXPECT_THAT(ubs, IsEmpty());

  // Non improving bounds, and partially improving bounds.
  manager.ReportPotentialNewBounds("fixed", {2, 4}, {0, 5}, {20, 20});
  manager.Synchronize();

  manager.GetChangedBounds(0, &vars, &lbs, &ubs);
  EXPECT_THAT(vars, ElementsAre(4));
  EXPECT_THAT(lbs, ElementsAre(5));
  EXPECT_THAT(ubs, ElementsAre(12));

  manager.GetChangedBounds(1, &vars, &lbs, &ubs);
  EXPECT_THAT(vars, ElementsAre(4));
  EXPECT_THAT(lbs, ElementsAre(5));
  EXPECT_THAT(ubs, ElementsAre(12));

  manager.GetChangedBounds(2, &vars, &lbs, &ubs);
  EXPECT_THAT(vars, ElementsAre(4));
  EXPECT_THAT(lbs, ElementsAre(5));
  EXPECT_THAT(ubs, ElementsAre(12));

  // Matching bounds.
  manager.ReportPotentialNewBounds("fixed", {2}, {1}, {11});
  manager.Synchronize();

  manager.GetChangedBounds(0, &vars, &lbs, &ubs);
  EXPECT_THAT(vars, IsEmpty());
  EXPECT_THAT(lbs, IsEmpty());
  EXPECT_THAT(ubs, IsEmpty());

  manager.GetChangedBounds(1, &vars, &lbs, &ubs);
  EXPECT_THAT(vars, IsEmpty());
  EXPECT_THAT(lbs, IsEmpty());
  EXPECT_THAT(ubs, IsEmpty());

  manager.GetChangedBounds(2, &vars, &lbs, &ubs);
  EXPECT_THAT(vars, IsEmpty());
  EXPECT_THAT(lbs, IsEmpty());
  EXPECT_THAT(ubs, IsEmpty());
}

TEST(SharedBoundsManagerTest, WithSymmetry) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 20 ] }
    symmetry {
      permutations {
        support: [ 0, 1, 2 ]
        cycle_sizes: [ 3 ]
      }
    }
  )pb");
  SharedBoundsManager manager(model);
  manager.ReportPotentialNewBounds("auto", /*variables=*/{2, 1}, {4, 2},
                                   {7, 9});
  manager.Synchronize();

  std::vector<int> vars;
  std::vector<int64_t> lbs;
  std::vector<int64_t> ubs;

  EXPECT_EQ(manager.RegisterNewId(), 0);
  manager.GetChangedBounds(0, &vars, &lbs, &ubs);
  EXPECT_THAT(vars, ElementsAre(0, 1, 2));
  EXPECT_THAT(lbs, ElementsAre(4, 4, 4));
  EXPECT_THAT(ubs, ElementsAre(7, 7, 7));
}

TEST(SharedResponseManagerTest, InitialResponseSAT) {
  Model model;
  auto* shared_response = model.GetOrCreate<SharedResponseManager>();
  EXPECT_EQ(0.0, shared_response->GapIntegral());
  const CpSolverResponse response = ParseTestProto(R"pb(
    status: UNKNOWN,
  )pb");
  EXPECT_THAT(shared_response->GetResponse(), EqualsProto(response));
}

TEST(SharedResponseManagerTest, InitialResponseMinimization) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    objective: { scaling_factor: 1 })pb");

  Model model;
  auto* shared_response = model.GetOrCreate<SharedResponseManager>();
  shared_response->InitializeObjective(model_proto);
  EXPECT_EQ(0.0, shared_response->GapIntegral());
  const CpSolverResponse response = ParseTestProto(R"pb(
    status: UNKNOWN,
    objective_value: inf,
    best_objective_bound: -inf,
    inner_objective_lower_bound: -9223372036854775808,
  )pb");
  EXPECT_THAT(shared_response->GetResponse(), EqualsProto(response));
}

TEST(SharedResponseManagerTest, InitialResponseMaximization) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    objective: { scaling_factor: -1 })pb");

  Model model;
  auto* shared_response = model.GetOrCreate<SharedResponseManager>();
  shared_response->InitializeObjective(model_proto);

  EXPECT_EQ(0.0, shared_response->GapIntegral());
  const CpSolverResponse response = ParseTestProto(R"pb(
    status: UNKNOWN,
    objective_value: -inf,
    best_objective_bound: inf,
    inner_objective_lower_bound: -9223372036854775808,
  )pb");
  EXPECT_THAT(shared_response->GetResponse(), EqualsProto(response));
}

TEST(SharedResponseManagerTest, UnknownResponseMinimization) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    objective: { scaling_factor: 0 })pb");

  Model model;
  auto* shared_time_limit = model.GetOrCreate<ModelSharedTimeLimit>();
  auto* shared_response = model.GetOrCreate<SharedResponseManager>();
  shared_response->InitializeObjective(model_proto);

  EXPECT_EQ(0.0, shared_response->GapIntegral());
  shared_response->UpdateInnerObjectiveBounds("", IntegerValue(-4),
                                              IntegerValue(7));
  shared_response->UpdateGapIntegral();
  EXPECT_EQ(0.0, shared_response->GapIntegral());
  shared_time_limit->AdvanceDeterministicTime(1.0);
  shared_response->UpdateGapIntegral();
  EXPECT_EQ(1.0 * std::log(1 + 11), shared_response->GapIntegral());

  const std::string response = R"pb(
    objective_value: 7,
    best_objective_bound: -4,
    inner_objective_lower_bound: -4
  )pb";
  auto result = shared_response->GetResponse();
  EXPECT_EQ(result.objective_value(), 7);
  EXPECT_EQ(result.best_objective_bound(), -4);
  EXPECT_EQ(result.inner_objective_lower_bound(), -4);
  EXPECT_EQ(result.status(), UNKNOWN);
}

TEST(SharedResponseManagerTest, UnknownResponseMaximization) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    objective: { scaling_factor: -4 })pb");

  Model model;
  auto* shared_time_limit = model.GetOrCreate<ModelSharedTimeLimit>();
  auto* shared_response = model.GetOrCreate<SharedResponseManager>();
  shared_response->InitializeObjective(model_proto);

  EXPECT_EQ(0.0, shared_response->GapIntegral());
  shared_response->UpdateInnerObjectiveBounds("", IntegerValue(-4),
                                              IntegerValue(7));
  shared_response->UpdateGapIntegral();
  EXPECT_EQ(0.0, shared_response->GapIntegral());
  shared_time_limit->AdvanceDeterministicTime(1.0);
  shared_response->UpdateGapIntegral();
  EXPECT_EQ(1.0 * log(1 + 4.0 * 11), shared_response->GapIntegral());

  const std::string response = R"pb(
    objective_value: -28,
    best_objective_bound: 16,
    inner_objective_lower_bound: -4
  )pb";
  auto result = shared_response->GetResponse();
  EXPECT_EQ(result.objective_value(), -28);
  EXPECT_EQ(result.best_objective_bound(), 16);
  EXPECT_EQ(result.inner_objective_lower_bound(), -4);
  EXPECT_EQ(result.status(), UNKNOWN);
}

TEST(SharedResponseManagerTest, GapIntegralTest) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    objective: { scaling_factor: -4 offset: 100 })pb");

  Model model;
  auto* shared_time_limit = model.GetOrCreate<ModelSharedTimeLimit>();
  auto* shared_response = model.GetOrCreate<SharedResponseManager>();
  shared_response->InitializeObjective(model_proto);

  // At the beginning the primal integral is zero, and will start counting
  // on the first update, ignoring any earlier time. This leave a change to
  // use reasonable bound on the objective.
  EXPECT_EQ(0.0, shared_response->GapIntegral());
  shared_time_limit->AdvanceDeterministicTime(1.0);
  shared_response->UpdateGapIntegral();
  EXPECT_EQ(0.0, shared_response->GapIntegral());

  // Unknown count as max possible difference.
  shared_time_limit->AdvanceDeterministicTime(1.0);
  shared_response->UpdateGapIntegral();
  const double value1 =
      1.0 *
      log(1 + 4 * (static_cast<double>(std::numeric_limits<int64_t>::max()) -
                   static_cast<double>(std::numeric_limits<int64_t>::min())));
  EXPECT_EQ(value1, shared_response->GapIntegral());

  // No time, so still same. But the function height will change.
  shared_response->UpdateInnerObjectiveBounds("", IntegerValue(-4),
                                              IntegerValue(7));
  shared_response->UpdateGapIntegral();
  EXPECT_EQ(value1, shared_response->GapIntegral());

  // Add time, increase the integral.
  shared_time_limit->AdvanceDeterministicTime(3.0);
  shared_response->UpdateGapIntegral();
  EXPECT_EQ(value1 + 3.0 * log(1 + 4.0 * 11), shared_response->GapIntegral());
}

TEST(SharedResponseManagerTest, GapIntegralOnEachChangeTest) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    objective: { scaling_factor: -4 offset: 100 })pb");

  Model model;
  auto* shared_time_limit = model.GetOrCreate<ModelSharedTimeLimit>();
  auto* shared_response = model.GetOrCreate<SharedResponseManager>();
  shared_response->InitializeObjective(model_proto);

  // Starts with reasonable bound this time.
  shared_time_limit->AdvanceDeterministicTime(1.0);
  shared_response->UpdateInnerObjectiveBounds("", IntegerValue(0),
                                              IntegerValue(10));
  shared_response->SetUpdateGapIntegralOnEachChange(true);
  shared_response->UpdateGapIntegral();
  EXPECT_EQ(0.0, shared_response->GapIntegral());

  // First Update.
  shared_time_limit->AdvanceDeterministicTime(1.0);
  shared_response->UpdateInnerObjectiveBounds("", IntegerValue(0),
                                              IntegerValue(7));
  shared_response->UpdateGapIntegral();
  double expected = 1.0 * log(1 + 4 * 10);
  EXPECT_EQ(expected, shared_response->GapIntegral());

  // Updating bound with no time, do not do anything.
  shared_response->UpdateInnerObjectiveBounds("", IntegerValue(0),
                                              IntegerValue(3));
  EXPECT_EQ(expected, shared_response->GapIntegral());

  // Add time, and change bound increase the integral.
  shared_time_limit->AdvanceDeterministicTime(3.0);
  shared_response->UpdateInnerObjectiveBounds("", IntegerValue(0),
                                              IntegerValue(2));
  expected += 3.0 * log(1 + 4.0 * 3);
  EXPECT_EQ(expected, shared_response->GapIntegral());

  // Closing the search still increase it. And we deal correcly with bound
  // crossing.
  shared_time_limit->AdvanceDeterministicTime(1.0);
  shared_response->UpdateInnerObjectiveBounds("", IntegerValue(10),
                                              IntegerValue(0));
  expected += 1.0 * log(1 + 4.0 * 2);
  EXPECT_EQ(expected, shared_response->GapIntegral());
}

TEST(SharedResponseManagerDeathTest, UpdateInnerObjectiveBoundsOnSAT) {
  Model model;
  auto* shared_response = model.GetOrCreate<SharedResponseManager>();
  EXPECT_DEATH(shared_response->UpdateInnerObjectiveBounds("", IntegerValue(0),
                                                           IntegerValue(0)),
               "");
}

TEST(SharedResponseManagerTest, Infeasible) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    objective: { scaling_factor: 0 })pb");

  Model model;
  auto* shared_response = model.GetOrCreate<SharedResponseManager>();
  shared_response->InitializeObjective(model_proto);

  shared_response->UpdateInnerObjectiveBounds("", IntegerValue(-4),
                                              IntegerValue(7));
  shared_response->NotifyThatImprovingProblemIsInfeasible("");

  const CpSolverResponse response = ParseTestProto(R"pb(
    status: INFEASIBLE,
  )pb");
  EXPECT_THAT(shared_response->GetResponse(), EqualsProto(response));
  EXPECT_TRUE(shared_response->ProblemIsSolved());
}

TEST(SharedResponseManagerTest, Solution) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    objective: {
      vars: [ 0, 1, 2 ]
      coeffs: [ 2, 3, 4 ]
    })pb");

  Model model;
  auto* shared_response = model.GetOrCreate<SharedResponseManager>();
  shared_response->InitializeObjective(model_proto);
  shared_response->UpdateInnerObjectiveBounds("", IntegerValue(2),
                                              IntegerValue(20));
  const CpSolverResponse solution = ParseTestProto(R"pb(
    solution_info: "test"
    solution: [ 1, 2, 1 ]
  )pb");
  shared_response->NewSolution(solution.solution(), solution.solution_info());

  {
    const CpSolverResponse response = ParseTestProto(R"pb(
      status: FEASIBLE,
      solution_info: "test"
      solution: [ 1, 2, 1 ]
      objective_value: 12
      best_objective_bound: 2
      inner_objective_lower_bound: 2)pb");
    EXPECT_THAT(shared_response->GetResponse(), EqualsProto(response));
    EXPECT_FALSE(shared_response->ProblemIsSolved());
  }

  // Optimal.
  shared_response->NotifyThatImprovingProblemIsInfeasible("");
  {
    const CpSolverResponse response = ParseTestProto(R"pb(
      status: OPTIMAL,
      solution_info: "test"
      solution: [ 1, 2, 1 ]
      objective_value: 12
      best_objective_bound: 12
      inner_objective_lower_bound: 12)pb");
    EXPECT_THAT(shared_response->GetResponse(), EqualsProto(response));
    EXPECT_TRUE(shared_response->ProblemIsSolved());
  }
}

TEST(SharedResponseManagerTest, BestBound) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    objective: {
      vars: [ 0, 1, 2 ]
      coeffs: [ 2, 3, 4 ]
      offset: 0.5
      scaling_factor: 3.0
    })pb");

  Model model;
  auto* shared_response = model.GetOrCreate<SharedResponseManager>();
  shared_response->InitializeObjective(model_proto);
  double result = 0.0;
  auto observer = [&result](double value) { result = value; };
  shared_response->AddBestBoundCallback(observer);
  shared_response->UpdateInnerObjectiveBounds("", 2, 20);
  EXPECT_EQ(result, 7.5);
}

TEST(SharedResponseManagerTest, SolutionToFeasibilityProblem) {
  Model model;
  auto* shared_response = model.GetOrCreate<SharedResponseManager>();
  const CpSolverResponse solution = ParseTestProto(R"pb(
    solution_info: "test"
    solution: [ 1, 2, 1 ]
  )pb");
  shared_response->NewSolution(solution.solution(), solution.solution_info());

  {
    const CpSolverResponse response = ParseTestProto(R"pb(
      status: OPTIMAL,
      solution_info: "test"
      solution: [ 1, 2, 1 ])pb");
    EXPECT_THAT(shared_response->GetResponse(), EqualsProto(response));
    EXPECT_TRUE(shared_response->ProblemIsSolved());
  }
}

TEST(SharedResponseManagerTest, OptimalReachedBecauseOfBounds) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    objective: {
      vars: [ 0, 1, 2 ]
      coeffs: [ 2, 3, 4 ]
    })pb");

  Model model;
  auto* shared_time_limit = model.GetOrCreate<ModelSharedTimeLimit>();
  auto* shared_response = model.GetOrCreate<SharedResponseManager>();
  shared_response->InitializeObjective(model_proto);

  EXPECT_EQ(0.0, shared_response->GapIntegral());
  shared_time_limit->AdvanceDeterministicTime(10.0);

  shared_response->UpdateInnerObjectiveBounds("", IntegerValue(12),
                                              IntegerValue(12));
  const CpSolverResponse solution = ParseTestProto(R"pb(
    solution_info: "test"
    solution: [ 1, 2, 1 ]
  )pb");
  EXPECT_EQ(0.0, shared_response->GapIntegral());

  shared_time_limit->AdvanceDeterministicTime(10.0);
  shared_response->NewSolution(solution.solution(), solution.solution_info());
  EXPECT_EQ(0.0, shared_response->GapIntegral());

  const CpSolverResponse response = ParseTestProto(R"pb(
    status: OPTIMAL,
    solution_info: "test"
    solution: [ 1, 2, 1 ]
    objective_value: 12
    best_objective_bound: 12
    inner_objective_lower_bound: 12)pb");
  EXPECT_THAT(shared_response->GetResponse(), EqualsProto(response));
  EXPECT_TRUE(shared_response->ProblemIsSolved());
}

TEST(SharedResponseManagerTest, ProblemCanBeClosedWithJustBoundUpdates) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    objective: {
      vars: [ 0, 1, 2 ]
      coeffs: [ 2, 3, 4 ]
    })pb");

  Model model;
  auto* shared_time_limit = model.GetOrCreate<ModelSharedTimeLimit>();
  auto* shared_response = model.GetOrCreate<SharedResponseManager>();
  shared_response->InitializeObjective(model_proto);
  EXPECT_EQ(0.0, shared_response->GapIntegral());

  shared_time_limit->AdvanceDeterministicTime(10.0);
  shared_response->UpdateInnerObjectiveBounds("", IntegerValue(13),
                                              IntegerValue(12));

  EXPECT_EQ(0.0, shared_response->GapIntegral());
  const CpSolverResponse response = ParseTestProto(R"pb(
    status: INFEASIBLE)pb");
  EXPECT_THAT(shared_response->GetResponse(), EqualsProto(response));
  EXPECT_TRUE(shared_response->ProblemIsSolved());
}

TEST(SharedResponseManagerTest, ProblemCanBeClosedWithJustBoundUpdates2) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    objective: {
      vars: [ 0, 1, 2 ]
      coeffs: [ 2, 3, 4 ]
    })pb");

  Model model;
  auto* shared_response = model.GetOrCreate<SharedResponseManager>();
  shared_response->InitializeObjective(model_proto);

  const CpSolverResponse solution = ParseTestProto(R"pb(
    solution_info: "test"
    solution: [ 1, 2, 1 ]
  )pb");
  shared_response->NewSolution(solution.solution(), solution.solution_info());

  shared_response->UpdateInnerObjectiveBounds("", IntegerValue(13),
                                              IntegerValue(15));

  const CpSolverResponse response = ParseTestProto(R"pb(
    status: OPTIMAL,
    solution_info: "test"
    solution: [ 1, 2, 1 ]
    objective_value: 12
    best_objective_bound: 12,
    inner_objective_lower_bound: 12)pb");
  EXPECT_THAT(shared_response->GetResponse(), EqualsProto(response));
  EXPECT_THAT(shared_response->GetResponse(), EqualsProto(response));
  EXPECT_TRUE(shared_response->ProblemIsSolved());
}

#ifndef NDEBUG

// TODO(user): Having a check sometime fail in multithread. Understand how
// the code can push an invalid lower bound (and still be valid). The likely
// behavior, is that at the end of the search, when the improving problem is
// infeasible, then we might have no guarantee that while incorporating new
// bounds, one thread pushes the lower bound too high ?
TEST(SharedResponseManagerDeathTest, InnerBoundMustBeValid) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    objective: {
      vars: [ 0, 1, 2 ]
      coeffs: [ 2, 3, 4 ]
    })pb");

  Model model;
  auto* shared_response = model.GetOrCreate<SharedResponseManager>();
  shared_response->InitializeObjective(model_proto);
  shared_response->UpdateInnerObjectiveBounds("", IntegerValue(20),
                                              IntegerValue(25));
  const CpSolverResponse solution = ParseTestProto(R"pb(
    solution_info: "test"
    solution: [ 1, 2, 1 ]
  )pb");

  // The lower bound is not globally valid!
  EXPECT_DEATH(shared_response->NewSolution(solution.solution(),
                                            solution.solution_info()),
               "");
}

TEST(SharedResponseManagerDeathTest, OptimalCannotBeImproved) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    objective: {
      vars: [ 0, 1, 2 ]
      coeffs: [ 2, 3, 4 ]
    })pb");

  Model model;
  auto* shared_response = model.GetOrCreate<SharedResponseManager>();
  shared_response->InitializeObjective(model_proto);

  shared_response->NewSolution({1, 2, 1}, "test");
  shared_response->NotifyThatImprovingProblemIsInfeasible("");
  shared_response->NotifyThatImprovingProblemIsInfeasible("");
  {
    const CpSolverResponse response = ParseTestProto(R"pb(
      status: OPTIMAL,
      solution_info: "test"
      solution: [ 1, 2, 1 ]
      objective_value: 12
      best_objective_bound: 12
      inner_objective_lower_bound: 12)pb");
    EXPECT_THAT(shared_response->GetResponse(), EqualsProto(response));
  }

  {
    const CpSolverResponse solution = ParseTestProto(R"pb(
      solution_info: "test"
      solution: [ 1, 1, 1 ]
    )pb");
    EXPECT_DEATH(shared_response->NewSolution(solution.solution(),
                                              solution.solution_info()),
                 "");
  }
}

TEST(SharedResponseManagerDeathTest,
     BetterSolutionMustNotArriveAfterInfeasible) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    objective: {
      vars: [ 0, 1, 2 ]
      coeffs: [ 2, 3, 4 ]
    })pb");

  Model model;
  auto* shared_response = model.GetOrCreate<SharedResponseManager>();
  shared_response->InitializeObjective(model_proto);

  // First solution.
  shared_response->NewSolution({1, 1, 1}, "test");
  shared_response->NotifyThatImprovingProblemIsInfeasible("");
  shared_response->NotifyThatImprovingProblemIsInfeasible("");
  EXPECT_EQ(shared_response->GetResponse().status(), CpSolverStatus::OPTIMAL);

  {
    // Better solution are not possible! otherwise there is a bug.
    EXPECT_DEATH(shared_response->NewSolution({1, 0, 1}, "test2"), "");
  }
}

#endif  // NDEBUG

TEST(SharedResponseManagerTest, Callback) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    objective: {
      vars: [ 0, 1, 2 ]
      coeffs: [ 2, 3, 4 ]
    })pb");

  Model model;
  auto* shared_response = model.GetOrCreate<SharedResponseManager>();
  shared_response->InitializeObjective(model_proto);

  int num_solutions = 0;
  const int callback_id = shared_response->AddSolutionCallback(
      [&num_solutions](const CpSolverResponse& /*response*/) {
        ++num_solutions;
      });

  {
    const CpSolverResponse solution = ParseTestProto(R"pb(
      solution_info: "test"
      solution: [ 1, 1, 1 ]
    )pb");
    shared_response->NewSolution(solution.solution(), solution.solution_info());
    EXPECT_EQ(num_solutions, 1);
  }

  // Not improving, so not called.
  {
    const CpSolverResponse solution = ParseTestProto(R"pb(
      solution_info: "test"
      solution: [ 1, 1, 1 ]
    )pb");
    shared_response->NewSolution(solution.solution(), solution.solution_info());
    EXPECT_EQ(num_solutions, 1);
  }

  // Improving, so called.
  {
    const CpSolverResponse solution = ParseTestProto(R"pb(
      solution_info: "test"
      solution: [ 0, 1, 1 ]
    )pb");
    shared_response->NewSolution(solution.solution(), solution.solution_info());
    EXPECT_EQ(num_solutions, 2);
  }

  // Improving, but unregistered, so not called.
  {
    const CpSolverResponse solution = ParseTestProto(R"pb(
      solution_info: "test"
      solution: [ 0, 1, 0 ]
    )pb");

    shared_response->UnregisterCallback(callback_id);
    shared_response->NewSolution(solution.solution(), solution.solution_info());
    EXPECT_EQ(num_solutions, 2);
  }
}

TEST(SharedClausesManagerTest, SyncApi) {
  SharedClausesManager manager(/*always_synchronize=*/true,
                               /*share_frequency=*/absl::ZeroDuration());
  EXPECT_EQ(0, manager.RegisterNewId());
  EXPECT_EQ(1, manager.RegisterNewId());

  manager.AddBinaryClause(/*id=*/0, 1, 2);
  std::vector<std::pair<int, int>> new_clauses;
  manager.GetUnseenBinaryClauses(/*id=*/0, &new_clauses);
  EXPECT_TRUE(new_clauses.empty());
  manager.GetUnseenBinaryClauses(/*id=*/1, &new_clauses);
  EXPECT_EQ(1, new_clauses.size());
  EXPECT_THAT(new_clauses, ::testing::ElementsAre(std::make_pair(1, 2)));
  manager.AddBinaryClause(/*id=*/1, 2, 3);
  manager.AddBinaryClause(/*id=*/1, 3, 2);
  manager.AddBinaryClause(/*id=*/0, 0, 1);
  manager.GetUnseenBinaryClauses(/*id=*/0, &new_clauses);
  EXPECT_THAT(new_clauses, ::testing::ElementsAre(std::make_pair(2, 3),
                                                  std::make_pair(0, 1)));
  manager.GetUnseenBinaryClauses(/*id=*/1, &new_clauses);
  EXPECT_THAT(new_clauses, ::testing::ElementsAre(std::make_pair(0, 1)));

  manager.GetUnseenBinaryClauses(/*id=*/0, &new_clauses);
  EXPECT_TRUE(new_clauses.empty());
  manager.GetUnseenBinaryClauses(/*id=*/1, &new_clauses);
  EXPECT_TRUE(new_clauses.empty());
}

TEST(UniqueClauseStreamTest, AddIgnoresDuplicates) {
  UniqueClauseStream stream;

  EXPECT_TRUE(stream.Add({1, 2, 3}));
  EXPECT_FALSE(stream.Add({3, 2, 1}));
  EXPECT_EQ(stream.NumBufferedLiterals(), 3);
}

TEST(UniqueClauseStreamTest, Delete) {
  UniqueClauseStream stream;

  EXPECT_TRUE(stream.Add({3, 2, 1}));
  EXPECT_TRUE(stream.Delete({1, 2, 3}));
  EXPECT_FALSE(stream.Delete({1, 2, 3, 4}));
  EXPECT_THAT(stream.NextBatch(), ::testing::IsEmpty());
  EXPECT_TRUE(stream.Add({2, 3, 1}));
  EXPECT_EQ(stream.NumBufferedLiterals(), 3);
  stream.NextBatch();
  EXPECT_TRUE(stream.Delete({1, 2, 3}));
}

TEST(UniqueClauseStreamTest, AddIgnoresInvalidSizeClauses) {
  UniqueClauseStream stream;
  std::vector<int> long_clause;
  long_clause.resize(UniqueClauseStream::kMaxClauseSize + 1);
  for (int i = 0; i < long_clause.size(); ++i) long_clause[i] = i;

  EXPECT_FALSE(stream.Add({2, 1}));
  EXPECT_FALSE(stream.Add(long_clause));
  EXPECT_EQ(stream.NumBufferedLiterals(), 0);
}

TEST(UniqueClauseStreamTest, ExportsShortestClauses) {
  UniqueClauseStream stream;
  for (int i = 0; i < 1024 / 4; ++i) {
    stream.Add({i + 1, i + 256, i + 512, -4});
  }
  for (int i = 0; i < 1024 / 3; ++i) {
    stream.Add({i + 1, i + 256, i + 512});
  }
  for (int i = 0; i < 1024 / 5; ++i) {
    stream.Add({i + 1, i + 256, i + 512, i + 1024, -2048});
  }

  // Batch 1 should be filled with size 3 clauses.
  EXPECT_EQ(stream.NextBatch().size(), 1024 / 3);
  // Batch 2 should be filled with size 4 clauses.
  EXPECT_EQ(stream.NextBatch().size(), 1024 / 4);
  // Batch 3 should be filled with size 5 clauses.
  EXPECT_EQ(stream.NextBatch().size(), 1024 / 5);
}

TEST(UniqueClauseStreamTest, RemoveWorstClauses) {
  UniqueClauseStream stream;
  // Fill the buffer
  for (int i = 0; i < UniqueClauseStream::kMaxBufferedLiterals / 6; ++i) {
    stream.Add({i + 1, i + 256, i + 512, -4, -3, -2});
  }
  for (int i = 0; i < UniqueClauseStream::kMaxLiteralsPerBatch / 2 / 3; ++i) {
    stream.Add({i + 1, i + 256, i + 512});
  }

  stream.RemoveWorstClauses();

  EXPECT_GE(stream.NumBufferedLiterals(),
            UniqueClauseStream::kMaxBufferedLiterals);
  EXPECT_LT(stream.NumBufferedLiterals(),
            UniqueClauseStream::kMaxBufferedLiterals + 6);
  EXPECT_TRUE(stream.CanAccept(3, /*lbd=*/2));
  EXPECT_FALSE(stream.CanAccept(6, /*lbd=*/2));
  // Make sure none of the size 3 clauses were removed.
  EXPECT_EQ(stream.NextBatch().size(),
            UniqueClauseStream::kMaxLiteralsPerBatch / 2 / 3 +
                UniqueClauseStream::kMaxBufferedLiterals / 2 / 6);
}

TEST(UniqueClauseStreamTest, DropsClauses) {
  UniqueClauseStream stream;
  // We shouldn't drop any clause where Add returns true.
  int literals_successfully_added = 0;
  for (int i = 0; i < 256 / 4; ++i) {
    literals_successfully_added +=
        4 * stream.Add({i + 1, i + 256, i + 512, -4});
  }
  for (int i = 0; i < 256 / 3; ++i) {
    literals_successfully_added += 3 * stream.Add({i + 1, i + 256, i + 512});
  }
  for (int i = 0; i < 1024 * 1024 / 5; ++i) {
    literals_successfully_added +=
        5 * stream.Add({i + 1, i + 256, i + 512, i + 1024, -2048});
  }

  EXPECT_FALSE(stream.CanAccept(3, /*lbd=*/3));
  EXPECT_TRUE(stream.CanAccept(3, /*lbd=*/2));
  EXPECT_TRUE(stream.CanAccept(4, /*lbd=*/2));
  EXPECT_FALSE(stream.CanAccept(5, /*lbd=*/2));
  EXPECT_EQ(stream.NumBufferedLiterals(), literals_successfully_added);
  EXPECT_EQ(
      literals_successfully_added,
      256 - 256 % 3 +      // size 3 clauses
          256 - 256 % 4 +  // size 4 clauses
          UniqueClauseStream::kMaxBufferedLiterals -
          UniqueClauseStream::kMaxBufferedLiterals % 5);  // size 5 clauses
  // Batch 1 should be filled with size 3 clauses.
  EXPECT_EQ(stream.NextBatch().size(), 256 / 3 + 256 / 4 + 512 / 5);
}

TEST(SharedClausesManagerTest, NonSyncApi) {
  SharedClausesManager manager(/*always_synchronize=*/false,
                               /*share_frequency=*/absl::ZeroDuration());
  EXPECT_EQ(0, manager.RegisterNewId());
  EXPECT_EQ(1, manager.RegisterNewId());

  manager.AddBinaryClause(/*id=*/0, 1, 2);
  std::vector<std::pair<int, int>> new_clauses;
  manager.GetUnseenBinaryClauses(/*id=*/0, &new_clauses);
  EXPECT_TRUE(new_clauses.empty());
  manager.GetUnseenBinaryClauses(/*id=*/1, &new_clauses);
  EXPECT_TRUE(new_clauses.empty());

  manager.Synchronize();
  manager.GetUnseenBinaryClauses(/*id=*/1, &new_clauses);
  EXPECT_EQ(1, new_clauses.size());
  EXPECT_THAT(new_clauses, ::testing::ElementsAre(std::make_pair(1, 2)));

  manager.AddBinaryClause(/*id=*/1, 2, 3);
  manager.AddBinaryClause(/*id=*/1, 3, 2);
  manager.AddBinaryClause(/*id=*/0, 0, 1);

  // Not synced.
  manager.GetUnseenBinaryClauses(/*id=*/0, &new_clauses);
  EXPECT_TRUE(new_clauses.empty());
  manager.GetUnseenBinaryClauses(/*id=*/1, &new_clauses);
  EXPECT_TRUE(new_clauses.empty());

  // After sync.
  manager.Synchronize();
  manager.GetUnseenBinaryClauses(/*id=*/0, &new_clauses);
  EXPECT_THAT(new_clauses, ::testing::ElementsAre(std::make_pair(2, 3),
                                                  std::make_pair(0, 1)));
  manager.GetUnseenBinaryClauses(/*id=*/1, &new_clauses);
  EXPECT_THAT(new_clauses, ::testing::ElementsAre(std::make_pair(0, 1)));

  // Not synced.
  manager.GetUnseenBinaryClauses(/*id=*/0, &new_clauses);
  EXPECT_TRUE(new_clauses.empty());
  manager.GetUnseenBinaryClauses(/*id=*/1, &new_clauses);
  EXPECT_TRUE(new_clauses.empty());

  // After sync.
  manager.Synchronize();
  manager.GetUnseenBinaryClauses(/*id=*/0, &new_clauses);
  EXPECT_TRUE(new_clauses.empty());
  manager.GetUnseenBinaryClauses(/*id=*/1, &new_clauses);
  EXPECT_TRUE(new_clauses.empty());
}

TEST(SharedClausesManagerTest, ShareGlueClauses) {
  SharedClausesManager manager(/*always_synchronize=*/true,
                               absl::ZeroDuration());
  ASSERT_EQ(0, manager.RegisterNewId());
  ASSERT_EQ(1, manager.RegisterNewId());
  auto* stream0 = manager.GetClauseStream(0);
  auto* stream1 = manager.GetClauseStream(1);
  // Add a bunch of clauses that will be skipped in the first batch.
  for (int i = 0; i < 1024 / 8; ++i) {
    EXPECT_TRUE(stream0->Add({1, 2, 3, 4, 5, 6, 7, i + 8}));
  }
  EXPECT_EQ(stream0->NumBufferedLiterals(), 1024);
  // Fill 1 batch of shorter clauses.
  for (int i = 0; i < 1024 / 4; ++i) {
    stream1->Add({1, 2, 3, i + 4});
  }
  EXPECT_EQ(stream1->NumBufferedLiterals(), 1024);

  EXPECT_THAT(manager.GetUnseenClauses(0), ::testing::IsEmpty());
  EXPECT_THAT(manager.GetUnseenClauses(1), ::testing::IsEmpty());
  manager.Synchronize();
  EXPECT_THAT(manager.GetUnseenClauses(0), ::testing::SizeIs(1024 / 4));
  EXPECT_THAT(manager.GetUnseenClauses(1), ::testing::SizeIs(1024 / 4));
  EXPECT_THAT(manager.GetUnseenClauses(0), ::testing::IsEmpty());
  EXPECT_THAT(manager.GetUnseenClauses(1), ::testing::IsEmpty());
  manager.Synchronize();
  EXPECT_THAT(manager.GetUnseenClauses(0), ::testing::SizeIs(1024 / 8));
  EXPECT_THAT(manager.GetUnseenClauses(1), ::testing::SizeIs(1024 / 8));
  EXPECT_THAT(manager.GetUnseenClauses(0), ::testing::IsEmpty());
  EXPECT_THAT(manager.GetUnseenClauses(1), ::testing::IsEmpty());
  manager.Synchronize();
  EXPECT_THAT(manager.GetUnseenClauses(0), ::testing::IsEmpty());
  EXPECT_THAT(manager.GetUnseenClauses(1), ::testing::IsEmpty());
}

TEST(SharedClausesManagerTest, ShareFrequency) {
  SharedClausesManager manager(/*always_synchronize=*/true,
                               absl::InfiniteDuration());
  ASSERT_EQ(0, manager.RegisterNewId());
  ASSERT_EQ(1, manager.RegisterNewId());
  auto* stream0 = manager.GetClauseStream(0);
  auto* stream1 = manager.GetClauseStream(1);
  for (int i = 0; i < 1024 / 5; ++i) {
    stream0->Add({i + 1, i + 513, 2048, 2049, -10});
    stream1->Add({i + 1, i + 513, 2048, 2049, -10});
  }

  EXPECT_THAT(manager.GetUnseenClauses(0), ::testing::IsEmpty());
  EXPECT_THAT(manager.GetUnseenClauses(1), ::testing::IsEmpty());
  manager.Synchronize();
  EXPECT_THAT(manager.GetUnseenClauses(0), ::testing::IsEmpty());
  EXPECT_THAT(manager.GetUnseenClauses(1), ::testing::IsEmpty());
}

TEST(SharedClausesManagerTest, LbdThresholdIncrease) {
  SharedClausesManager manager(/*always_synchronize=*/true,
                               absl::ZeroDuration());
  ASSERT_EQ(0, manager.RegisterNewId());
  ASSERT_EQ(1, manager.RegisterNewId());
  auto* stream0 = manager.GetClauseStream(0);
  auto* stream1 = manager.GetClauseStream(1);
  for (int i = 0; i < 1024 / 5; ++i) {
    stream0->Add({i + 1, i + 513, 2048, 2049, -10});
    stream1->Add({i + 1, i + 513, 2048, 2049, -10});
  }

  EXPECT_THAT(manager.GetUnseenClauses(0), ::testing::IsEmpty());
  EXPECT_THAT(manager.GetUnseenClauses(1), ::testing::IsEmpty());
  manager.Synchronize();
  EXPECT_THAT(manager.GetUnseenClauses(0), ::testing::SizeIs(1024 / 5));
  EXPECT_THAT(manager.GetUnseenClauses(1), ::testing::SizeIs(1024 / 5));
  EXPECT_EQ(stream0->lbd_threshold(), 2);
  EXPECT_EQ(stream1->lbd_threshold(), 2);
  manager.Synchronize();
  EXPECT_THAT(manager.GetUnseenClauses(0), ::testing::IsEmpty());
  EXPECT_THAT(manager.GetUnseenClauses(1), ::testing::IsEmpty());
  EXPECT_EQ(stream0->lbd_threshold(), 3);
  EXPECT_EQ(stream1->lbd_threshold(), 3);
}

TEST(SharedClausesManagerTest, LbdThresholdDecrease) {
  SharedClausesManager manager(/*always_synchronize=*/true,
                               absl::ZeroDuration());
  ASSERT_EQ(0, manager.RegisterNewId());
  ASSERT_EQ(1, manager.RegisterNewId());
  ASSERT_EQ(2, manager.RegisterNewId());
  auto* stream0 = manager.GetClauseStream(0);
  auto* stream1 = manager.GetClauseStream(1);

  // Should increase LBD Threshold.
  manager.Synchronize();
  // Then add 1/2 batch of clauses to each worker.
  for (int i = 0; i < 1024 / 4 / 2; ++i) {
    stream0->Add({i + 1, i + 512, 2048, 2049});
    stream1->Add({i + 1, i + 513, 2048, 2049});
  }
  // Than add loads of longer clauses to just stream0.
  for (int i = 1024 / 5 / 2; i < 3 * 1024 / 5; ++i) {
    stream0->Add({i + 1, 2, 3, -10});
  }

  EXPECT_THAT(manager.GetUnseenClauses(0), ::testing::IsEmpty());
  EXPECT_THAT(manager.GetUnseenClauses(1), ::testing::IsEmpty());
  EXPECT_EQ(stream0->lbd_threshold(), 3);
  EXPECT_EQ(stream1->lbd_threshold(), 3);
  manager.Synchronize();
  EXPECT_THAT(manager.GetUnseenClauses(0), ::testing::SizeIs(1024 / 4));
  EXPECT_THAT(manager.GetUnseenClauses(1), ::testing::SizeIs(1024 / 4));
  EXPECT_EQ(stream0->lbd_threshold(), 2);
  EXPECT_EQ(stream1->lbd_threshold(), 3);
}
}  // namespace
}  // namespace sat
}  // namespace operations_research
