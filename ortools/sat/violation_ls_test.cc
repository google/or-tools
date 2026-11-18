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

#include "ortools/sat/violation_ls.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/base/threadpool.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_test_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/stat_tables.h"
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

TEST(ViolationLsSolverTest,
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

  auto ls_states = std::make_shared<SharedLsStates>("ls", params, &stat_tables);
  ViolationLsSolver fj("ls", /*fj_enabled=*/true, /*vls_enabled=*/false, proto,
                       params, ls_states, &time_limit, &response,
                       &shared_bounds, &shared_clauses, &ls_hints,
                       &stat_tables);

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

TEST(ViolationLsSolverTest,
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
      std::make_shared<SharedLsStates>("ls", params1, &stat_tables);
  ViolationLsSolver fj1("ls1", /*fj_enabled=*/true, /*vls_enabled=*/false,
                        proto, params1, ls_states, &time_limit, &response,
                        &shared_bounds, &shared_clauses, &ls_hints,
                        &stat_tables);
  ViolationLsSolver fj2("ls2", /*fj_enabled=*/true, /*vls_enabled=*/false,
                        proto, params2, ls_states, &time_limit, &response,
                        &shared_bounds, &shared_clauses, &ls_hints,
                        &stat_tables);

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

TEST(ViolationLsSolverTest, ReinitializeAbortedByTimeLimitDoesNotCrash) {
  CpModelProto model_proto;
  auto* x = model_proto.add_variables();
  x->add_domain(0);
  x->add_domain(10);
  auto* y = model_proto.add_variables();
  y->add_domain(0);
  y->add_domain(10);
  auto* ct = model_proto.add_constraints()->mutable_linear();
  ct->add_vars(0);
  ct->add_coeffs(1);
  ct->add_vars(1);
  ct->add_coeffs(1);
  ct->add_domain(5);
  ct->add_domain(20);
  auto* obj = model_proto.mutable_objective();
  obj->add_vars(0);
  obj->add_coeffs(1);
  obj->add_vars(1);
  obj->add_coeffs(1);

  Model model;
  SatParameters params;
  params.set_feasibility_jump_batch_dtime(1e-4);
  *model.GetOrCreate<SatParameters>() = params;

  auto* shared_time_limit = model.GetOrCreate<ModelSharedTimeLimit>();
  SharedResponseManager shared_response(&model);
  shared_response.InitializeObjective(model_proto);
  SharedBoundsManager shared_bounds(model_proto);
  SharedClausesManager shared_clauses(/*always_synchronize=*/true);
  SharedLsSolutionRepository shared_hints;
  SharedStatTables stat_tables;
  auto ls_states = std::make_shared<SharedLsStates>("ls", params, &stat_tables);

  ViolationLsSolver solver("ls", /*fj_enabled=*/true, /*vls_enabled=*/true,
                           model_proto, params, ls_states, shared_time_limit,
                           &shared_response, &shared_bounds, &shared_clauses,
                           &shared_hints, &stat_tables);

  // First task initializes evaluator_ and runs a batch.
  solver.GenerateTask(0)();

  // Trigger a model rebuild on the next task while stopping the shared limit so
  // Initialize() aborts and resets evaluator_ to nullptr.
  shared_bounds.ReportPotentialNewBounds("test", {0}, {2}, {2});
  shared_bounds.Synchronize();
  shared_time_limit->Stop();
  solver.GenerateTask(1)();

  // A subsequent task sees updated == false and evaluator_ == nullptr; it must
  // re-attempt initialization cleanly instead of dereferencing evaluator_.
  solver.GenerateTask(2)();
}

TEST(ViolationLsSolverTest, StopsMidBatchWhenTimeLimitIsStopped) {
  const CpModelProto proto = PigeonholeProblem(30);
  SatParameters params;
  params.set_feasibility_jump_batch_dtime(10.0);
  params.set_feasibility_jump_decay(1.0);
  params.set_violation_ls_compound_move_probability(0.0);
  SharedBoundsManager shared_bounds(proto);
  SharedClausesManager shared_clauses(true);
  SharedStatTables stat_tables;
  Model model;
  ModelSharedTimeLimit time_limit(&model);
  SharedResponseManager response(&model);
  SharedLsSolutionRepository ls_hints;
  auto ls_states = std::make_shared<SharedLsStates>("ls", params, &stat_tables);
  ViolationLsSolver fj("ls", /*fj_enabled=*/true, /*vls_enabled=*/false, proto,
                       params, ls_states, &time_limit, &response,
                       &shared_bounds, &shared_clauses, &ls_hints,
                       &stat_tables);
  absl::Notification started;
  absl::Notification done;
  ThreadPool pool(1);

  pool.Schedule([&, task = fj.GenerateTask(0)] {
    started.Notify();
    task();
    done.Notify();
  });
  started.WaitForNotification();
  absl::SleepFor(absl::Milliseconds(20));
  time_limit.Stop();

  EXPECT_TRUE(done.WaitForNotificationWithTimeout(absl::Milliseconds(100)));
}
}  // namespace
}  // namespace operations_research::sat
