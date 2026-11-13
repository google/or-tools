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

#include "ortools/sat/feasibility_jump.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/stat_tables.h"
#include "ortools/sat/subsolver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"

namespace operations_research::sat {
namespace {

TEST(JumpTableTest, TestCachesCalls) {
  int num_calls = 0;
  JumpTable jumps;
  jumps.SetComputeFunction(
      [&](int) { return std::make_pair(++num_calls, -1.0); });
  jumps.RecomputeAll(1);

  EXPECT_EQ(jumps.GetJump(0), std::make_pair(int64_t{1}, -1.0));
  EXPECT_EQ(jumps.GetJump(0), std::make_pair(int64_t{1}, -1.0));
  EXPECT_EQ(num_calls, 1);
}

TEST(JumpTableTest, TestNeedsRecomputationOneVar) {
  int num_calls = 0;
  JumpTable jumps;
  jumps.SetComputeFunction(
      [&](int) { return std::make_pair(++num_calls, -1.0); });
  jumps.RecomputeAll(1);

  jumps.GetJump(0);
  jumps.Recompute(0);

  EXPECT_EQ(jumps.GetJump(0), std::make_pair(int64_t{2}, -1.0));
  EXPECT_EQ(num_calls, 2);
}

TEST(JumpTableTest, TestNeedsRecomputationMultiVar) {
  int num_calls = 0;
  JumpTable jumps;
  jumps.SetComputeFunction(
      [&](int v) { return std::make_pair(++num_calls, v); });
  jumps.RecomputeAll(2);

  jumps.GetJump(0);
  jumps.GetJump(1);
  jumps.Recompute(0);

  EXPECT_EQ(jumps.GetJump(0), std::make_pair(int64_t{3}, 0.0));
  EXPECT_EQ(jumps.GetJump(1), std::make_pair(int64_t{2}, 1.0));
  EXPECT_EQ(num_calls, 3);
}

TEST(JumpTableTest, TestVarsNeedingRecomputePossiblyGood) {
  int num_calls = 0;
  JumpTable jumps;
  jumps.SetComputeFunction(
      [&](int) { return std::make_pair(++num_calls, 1.0); });
  jumps.RecomputeAll(1);

  EXPECT_TRUE(jumps.NeedRecomputation(0));
  EXPECT_EQ(num_calls, 0);
}

TEST(JumpTableTest, TestSetJump) {
  int num_calls = 0;
  JumpTable jumps;
  jumps.SetComputeFunction(
      [&](int) { return std::make_pair(++num_calls, -1.0); });
  jumps.RecomputeAll(1);

  jumps.SetJump(0, 1, 1.0);

  EXPECT_FALSE(jumps.NeedRecomputation(0));
  EXPECT_GE(jumps.Score(0), 0);
  EXPECT_EQ(jumps.GetJump(0), std::make_pair(int64_t{1}, 1.0));
  EXPECT_EQ(num_calls, 0);
}

TEST(FeasibilityJumpSolverTest,
     IncrementalUpdatesPreserveWeightsAndAdvanceDeterministicTime) {
  CpModelProto proto;
  for (int i = 0; i < 10; ++i) {
    auto* var = proto.add_variables();
    var->add_domain(0);
    var->add_domain(10);
  }
  // Contradictory constraints on vars 0 and 1 so search never terminates early
  // and keeps updating GLS weights across batches.
  {
    auto* ct = proto.add_constraints()->mutable_linear();
    ct->add_vars(0);
    ct->add_vars(1);
    ct->add_coeffs(1);
    ct->add_coeffs(1);
    ct->add_domain(15);
    ct->add_domain(20);
  }
  {
    auto* ct = proto.add_constraints()->mutable_linear();
    ct->add_vars(0);
    ct->add_vars(1);
    ct->add_coeffs(1);
    ct->add_coeffs(1);
    ct->add_domain(0);
    ct->add_domain(10);
  }
  {
    auto* ct = proto.add_constraints()->mutable_linear();
    for (int i = 2; i < 10; ++i) {
      ct->add_vars(i);
      ct->add_coeffs(1);
    }
    ct->add_domain(0);
    ct->add_domain(80);
  }

  SatParameters params;
  params.set_feasibility_jump_restart_factor(100);
  params.set_feasibility_jump_batch_dtime(1e-4);
  SharedBoundsManager shared_bounds(proto);
  SharedClausesManager shared_clauses(true);
  SharedStatTables stat_tables;
  Model model;
  ModelSharedTimeLimit time_limit(&model);
  SharedResponseManager response(&model);
  SharedLsSolutionRepository ls_hints;

  auto ls_states = std::make_shared<SharedLsStates>("fj", params, &stat_tables);
  FeasibilityJumpSolver fj("fj", SubSolver::FIRST_SOLUTION, proto, params,
                           ls_states, &time_limit, &response, &shared_bounds,
                           &shared_clauses, &ls_hints, &stat_tables);

  // Initialize all 8 states with one batch each.
  double prev_dtime = 0.0;
  for (int t = 0; t < 8; ++t) {
    ASSERT_TRUE(fj.TaskIsAvailable());
    fj.GenerateTask(t)();
    EXPECT_GT(fj.deterministic_time(), prev_dtime);
    prev_dtime = fj.deterministic_time();
  }

  // Peek at state 0's accumulated weights before tightening bounds.
  LsState* state0 = ls_states->GetNextState();
  ASSERT_EQ(state0->bounds_timestamp, -1);
  ASSERT_EQ(state0->solution.size(), 10);
  const double weight_after_first_batch =
      state0->weights[0] + state0->weights[1];
  EXPECT_GT(weight_after_first_batch, 2.0);
  ls_states->Release(state0);

  // Tighten non-fixing bounds on variables 2 and 3; weights on state 0 must not
  // be reset on the next round-robin visit.
  shared_bounds.ReportPotentialNewBounds("sat", {2, 3}, {2, 3}, {8, 9});
  shared_bounds.Synchronize();
  for (int t = 8; t < 16; ++t) {
    ASSERT_TRUE(fj.TaskIsAvailable());
    fj.GenerateTask(t)();
    EXPECT_GT(fj.deterministic_time(), prev_dtime);
    prev_dtime = fj.deterministic_time();
  }
  EXPECT_EQ(state0->bounds_timestamp, -1);
  EXPECT_GT(state0->weights[0] + state0->weights[1], weight_after_first_batch);

  // Fix 20% of variables (vars 2 and 3). Because > 10x rebuild dtime has
  // already been spent in search, the next task rebuilds DenseModelCopy and
  // LsEvaluator without losing monotonic deterministic_time().
  shared_bounds.ReportPotentialNewBounds("sat", {2, 3}, {5, 5}, {5, 5});
  shared_bounds.Synchronize();
  for (int t = 16; t < 24; ++t) {
    ASSERT_TRUE(fj.TaskIsAvailable());
    fj.GenerateTask(t)();
    EXPECT_GT(fj.deterministic_time(), prev_dtime);
    prev_dtime = fj.deterministic_time();
  }
  EXPECT_GE(state0->bounds_timestamp, 0);
  EXPECT_EQ(state0->solution.size(), 8);
  EXPECT_EQ(state0->move->NumVariables(), 8);
}

TEST(FeasibilityJumpSolverTest,
     MultipleWorkersHandleStaggeredRebuildsAndConflictingBounds) {
  CpModelProto proto;
  for (int i = 0; i < 10; ++i) {
    auto* var = proto.add_variables();
    var->add_domain(0);
    var->add_domain(10);
  }
  {
    auto* ct = proto.add_constraints()->mutable_linear();
    ct->add_vars(0);
    ct->add_vars(1);
    ct->add_coeffs(1);
    ct->add_coeffs(1);
    ct->add_domain(15);
    ct->add_domain(20);
  }
  {
    auto* ct = proto.add_constraints()->mutable_linear();
    ct->add_vars(0);
    ct->add_vars(1);
    ct->add_coeffs(1);
    ct->add_coeffs(1);
    ct->add_domain(0);
    ct->add_domain(10);
  }
  {
    auto* ct = proto.add_constraints()->mutable_linear();
    for (int i = 2; i < 10; ++i) {
      ct->add_vars(i);
      ct->add_coeffs(1);
    }
    ct->add_domain(0);
    ct->add_domain(80);
  }

  SatParameters params1;
  params1.set_feasibility_jump_batch_dtime(1e-4);
  params1.set_random_seed(1);
  SatParameters params2 = params1;
  params2.set_feasibility_jump_batch_dtime(1e-7);
  params2.set_random_seed(2);

  SharedBoundsManager shared_bounds(proto);
  SharedClausesManager shared_clauses(true);
  SharedStatTables stat_tables;
  Model model;
  ModelSharedTimeLimit time_limit(&model);
  SharedResponseManager response(&model);
  SharedLsSolutionRepository ls_hints;

  auto ls_states =
      std::make_shared<SharedLsStates>("fj", params1, &stat_tables);
  FeasibilityJumpSolver fj1("fj1", SubSolver::FIRST_SOLUTION, proto, params1,
                            ls_states, &time_limit, &response, &shared_bounds,
                            &shared_clauses, &ls_hints, &stat_tables);
  FeasibilityJumpSolver fj2("fj2", SubSolver::FIRST_SOLUTION, proto, params2,
                            ls_states, &time_limit, &response, &shared_bounds,
                            &shared_clauses, &ls_hints, &stat_tables);

  // Accumulate enough search dtime on fj1 while fj2 has almost none.
  for (int t = 0; t < 4; ++t) {
    fj1.GenerateTask(t)();
  }
  fj2.GenerateTask(4)();

  // Fix 20% of variables: fj1 rebuilds (8 dense vars) while fj2 stays
  // rate-limited (10 dense vars). Interleaving both across SharedLsStates must
  // safely resize moves, solutions, and weights.
  shared_bounds.ReportPotentialNewBounds("sat", {2, 3}, {4, 4}, {4, 4});
  shared_bounds.Synchronize();
  for (int t = 0; t < 10; ++t) {
    ASSERT_TRUE(fj1.TaskIsAvailable());
    fj1.GenerateTask(10 + 2 * t)();
    ASSERT_TRUE(fj2.TaskIsAvailable());
    fj2.GenerateTask(11 + 2 * t)();
  }

  // Conflicting bounds mark the solver done and release task_generated_.
  shared_bounds.ReportPotentialNewBounds("sat", {4}, {8}, {8});
  shared_bounds.Synchronize();
  fj1.GenerateTask(100)();
  shared_bounds.ReportPotentialNewBounds("sat", {4}, {2}, {2});
  shared_bounds.Synchronize();
  fj1.GenerateTask(101)();
  EXPECT_TRUE(fj1.IsDone());
  EXPECT_FALSE(fj1.TaskIsAvailable());
}

TEST(FeasibilityJumpSolverTest,
     LinearizationLevelZeroSolvesWithoutLinearModel) {
  CpModelProto proto;
  // Variables:
  //   0..2: Booleans [0, 1]
  //   3: integer [0, 2]
  //   4: integer [0, 4]
  for (int i = 0; i < 3; ++i) {
    auto* var = proto.add_variables();
    var->add_domain(0);
    var->add_domain(1);
  }
  {
    auto* var = proto.add_variables();
    var->add_domain(0);
    var->add_domain(2);
  }
  {
    auto* var = proto.add_variables();
    var->add_domain(0);
    var->add_domain(4);
  }
  // Constraints that LinearModel would normally rewrite at level > 0:
  // exactly_one(0, 1, 2) + full-encoding literals for var 3.
  {
    auto* ct = proto.add_constraints()->mutable_exactly_one();
    ct->add_literals(0);
    ct->add_literals(1);
    ct->add_literals(2);
  }
  for (int v = 0; v < 3; ++v) {
    auto* eq_ct = proto.add_constraints();
    eq_ct->add_enforcement_literal(v);
    eq_ct->mutable_linear()->add_vars(3);
    eq_ct->mutable_linear()->add_coeffs(1);
    eq_ct->mutable_linear()->add_domain(v);
    eq_ct->mutable_linear()->add_domain(v);

    auto* neq_ct = proto.add_constraints();
    neq_ct->add_enforcement_literal(NegatedRef(v));
    neq_ct->mutable_linear()->add_vars(3);
    neq_ct->mutable_linear()->add_coeffs(1);
    if (v == 0) {
      neq_ct->mutable_linear()->add_domain(1);
      neq_ct->mutable_linear()->add_domain(2);
    } else if (v == 1) {
      neq_ct->mutable_linear()->add_domain(0);
      neq_ct->mutable_linear()->add_domain(0);
      neq_ct->mutable_linear()->add_domain(2);
      neq_ct->mutable_linear()->add_domain(2);
    } else {
      neq_ct->mutable_linear()->add_domain(0);
      neq_ct->mutable_linear()->add_domain(1);
    }
  }
  // Linear constraint: var 3 + var 4 == 4.
  {
    auto* ct = proto.add_constraints()->mutable_linear();
    ct->add_vars(3);
    ct->add_vars(4);
    ct->add_coeffs(1);
    ct->add_coeffs(1);
    ct->add_domain(4);
    ct->add_domain(4);
  }

  SatParameters params;
  params.set_feasibility_jump_linearization_level(0);
  params.set_feasibility_jump_batch_dtime(1e-2);
  SharedBoundsManager shared_bounds(proto);
  SharedClausesManager shared_clauses(true);
  SharedStatTables stat_tables;
  Model model;
  ModelSharedTimeLimit time_limit(&model);
  SharedResponseManager response(&model);
  SharedLsSolutionRepository ls_hints;

  auto ls_states = std::make_shared<SharedLsStates>("fj", params, &stat_tables);
  FeasibilityJumpSolver fj("fj", SubSolver::FIRST_SOLUTION, proto, params,
                           ls_states, &time_limit, &response, &shared_bounds,
                           &shared_clauses, &ls_hints, &stat_tables);

  for (int t = 0; t < 8 && !response.HasFeasibleSolution(); ++t) {
    ASSERT_TRUE(fj.TaskIsAvailable());
    fj.GenerateTask(t)();
  }
  EXPECT_TRUE(response.HasFeasibleSolution());
  EXPECT_GT(fj.deterministic_time(), 0.0);
}

}  // namespace
}  // namespace operations_research::sat
