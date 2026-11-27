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

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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

class ViolationLsSolverTestPeer {
 public:
  static LsState* State(const ViolationLsSolver& solver) {
    return solver.state_;
  }
  static bool TaskGenerated(const ViolationLsSolver& solver) {
    return solver.task_generated_.load();
  }
};

namespace {

struct ViolationLsTestContext {
  explicit ViolationLsTestContext(bool interleave = true) {
    params.set_interleave_search(interleave);
    params.set_random_seed(123456);
    params.set_feasibility_jump_restart_factor(1);
    *model.GetOrCreate<SatParameters>() = params;
    proto.add_variables()->add_domain(0);
    proto.mutable_variables(0)->add_domain(1);
    stats = model.GetOrCreate<SharedStatTables>();
    response = model.GetOrCreate<SharedResponseManager>();
    response->SetSynchronizationMode(false);
    time_limit = model.GetOrCreate<ModelSharedTimeLimit>();
    hints = model.GetOrCreate<SharedLsSolutionRepository>();
    states = std::make_shared<SharedLsStates>("test", params, stats);
  }

  std::unique_ptr<ViolationLsSolver> MakeSolver(
      bool improve_solutions = false,
      SharedBoundsManager* shared_bounds = nullptr,
      SharedClausesManager* shared_clauses = nullptr) {
    return std::make_unique<ViolationLsSolver>(
        "test", /*find_first_solution=*/true, improve_solutions, proto, params,
        states, time_limit, response, shared_bounds, shared_clauses, hints,
        stats);
  }

  Model model;
  CpModelProto proto;
  SatParameters params;
  SharedStatTables* stats;
  SharedResponseManager* response;
  ModelSharedTimeLimit* time_limit;
  SharedLsSolutionRepository* hints;
  std::shared_ptr<SharedLsStates> states;
};

// Observe the state used by the task even when task cleanup clears state_.
LsState* RunTaskAndFindRestartedState(
    const std::function<void()>& task,
    const std::vector<LsState*>& initial_states) {
  std::vector<int64_t> num_restarts;
  for (LsState* state : initial_states) {
    num_restarts.push_back(state->num_restarts);
  }
  task();
  LsState* restarted_state = nullptr;
  for (int i = 0; i < initial_states.size(); ++i) {
    if (initial_states[i]->num_restarts == num_restarts[i]) continue;
    EXPECT_EQ(initial_states[i]->num_restarts, num_restarts[i] + 1);
    EXPECT_EQ(restarted_state, nullptr);
    restarted_state = initial_states[i];
  }
  EXPECT_NE(restarted_state, nullptr);
  return restarted_state;
}

TEST(ViolationLsSolverTest, InterleavedTasksIgnoreExecutionOrder) {
  for (const bool reverse : {false, true}) {
    SCOPED_TRACE(reverse);
    ViolationLsTestContext context;
    std::vector<LsState*> initial_states;
    for (int i = 0; i < 8; ++i) {
      initial_states.push_back(context.states->GetNextState());
    }
    for (LsState* state : initial_states) context.states->Release(state);

    // The next two Luby terms are different: 1 and 2.
    context.states->ConfigureNextLubyRestart(initial_states.back());
    auto first = context.MakeSolver();
    auto second = context.MakeSolver();
    std::array<std::function<void()>, 2> tasks = {first->GenerateTask(0),
                                                  second->GenerateTask(1)};
    std::array<LsState*, 2> assigned_states;
    for (const int index : {reverse ? 1 : 0, reverse ? 0 : 1}) {
      assigned_states[index] =
          RunTaskAndFindRestartedState(tasks[index], initial_states);
    }
    ASSERT_NE(assigned_states[0], nullptr);
    ASSERT_NE(assigned_states[1], nullptr);
    EXPECT_EQ(assigned_states[0], initial_states[0]);
    EXPECT_EQ(assigned_states[1], initial_states[1]);
    EXPECT_EQ(assigned_states[0]->num_batches_before_change, 1);
    EXPECT_EQ(assigned_states[1]->num_batches_before_change, 2);
    first->Synchronize();
    second->Synchronize();
  }
}

TEST(ViolationLsSolverTest, InterleavedStatesStayReservedUntilSynchronize) {
  ViolationLsTestContext context;
  std::vector<LsState*> busy_states;
  for (int i = 0; i < 7; ++i) {
    busy_states.push_back(context.states->GetNextState());
  }
  LsState* only_available = context.states->GetNextState();
  context.states->Release(only_available);

  auto first = context.MakeSolver();
  auto second = context.MakeSolver();
  auto first_task = first->GenerateTask(0);
  auto second_task = second->GenerateTask(1);
  first_task();
  LsState* probe = context.states->GetNextState();
  EXPECT_NE(probe, only_available);
  context.states->Release(probe);
  second_task();
  ASSERT_NE(ViolationLsSolverTestPeer::State(*first), nullptr);
  ASSERT_NE(ViolationLsSolverTestPeer::State(*second), nullptr);
  EXPECT_NE(ViolationLsSolverTestPeer::State(*first),
            ViolationLsSolverTestPeer::State(*second));

  first->Synchronize();
  second->Synchronize();
  for (LsState* state : busy_states) context.states->Release(state);
}

TEST(ViolationLsSolverTest, InterleavedIncumbentResetsIgnoreExecutionOrder) {
  for (const bool reverse : {false, true}) {
    SCOPED_TRACE(reverse);
    ViolationLsTestContext context;
    auto* objective = context.proto.mutable_objective();
    objective->add_vars(0);
    objective->add_coeffs(1);
    objective->add_domain(0);
    objective->add_domain(1);
    context.response->InitializeObjective(context.proto);
    context.response->NewSolution(std::vector<int64_t>{1}, "initial");
    context.response->Synchronize();

    std::vector<LsState*> initial_states;
    for (int i = 0; i < 8; ++i) {
      initial_states.push_back(context.states->GetNextState());
    }
    for (LsState* state : initial_states) context.states->Release(state);
    // Only the second state needs to reset the shared Luby counter.
    initial_states[0]->last_solution_rank = 1;
    initial_states[0]->num_solutions_imported = 1;
    context.states->ConfigureNextLubyRestart(initial_states.back());
    context.states->ConfigureNextLubyRestart(initial_states.back());

    auto first = context.MakeSolver(/*improve_solutions=*/true);
    auto second = context.MakeSolver(/*improve_solutions=*/true);
    std::array<std::function<void()>, 2> tasks = {first->GenerateTask(0),
                                                  second->GenerateTask(1)};
    std::array<LsState*, 2> assigned_states;
    for (const int index : {reverse ? 1 : 0, reverse ? 0 : 1}) {
      assigned_states[index] =
          RunTaskAndFindRestartedState(tasks[index], initial_states);
    }
    LsState* first_state = assigned_states[0];
    LsState* second_state = assigned_states[1];
    ASSERT_NE(first_state, nullptr);
    ASSERT_NE(second_state, nullptr);
    EXPECT_EQ(first_state, initial_states[0]);
    EXPECT_EQ(second_state, initial_states[1]);
    EXPECT_EQ(first_state->num_batches_before_change, 2);
    EXPECT_EQ(second_state->num_batches_before_change, 1);
    EXPECT_EQ(first_state->num_solutions_imported, 2);
    EXPECT_EQ(second_state->num_solutions_imported, 1);
    first->Synchronize();
    second->Synchronize();
  }
}

TEST(ViolationLsSolverTest,
     InterleavedPreIncumbentStatesTransitionDeterministically) {
  for (const bool reverse : {false, true}) {
    SCOPED_TRACE(reverse);
    ViolationLsTestContext context;
    auto* objective = context.proto.mutable_objective();
    objective->add_vars(0);
    objective->add_coeffs(1);
    objective->add_domain(0);
    objective->add_domain(1);
    // Keep pre-incumbent search infeasible until we inject an incumbent.
    auto* y = context.proto.add_variables();
    y->add_domain(0);
    y->add_domain(1);
    auto first = context.MakeSolver(/*improve_solutions=*/true);
    auto second = context.MakeSolver(/*improve_solutions=*/true);

    context.response->InitializeObjective(context.proto);
    std::vector<LsState*> initial_states;
    for (int i = 0; i < 8; ++i) {
      initial_states.push_back(context.states->GetNextState());
    }
    for (LsState* state : initial_states) context.states->Release(state);

    // Simulate initial_states[1] having run pre-incumbent batches with
    // remaining Luby budget, while initial_states[0] already imported the
    // first incumbent and reached the end of its Luby budget.
    initial_states[0]->last_solution_rank = 1;
    initial_states[0]->num_solutions_imported = 1;
    initial_states[0]->num_restarts = 1;
    initial_states[0]->num_batches_before_change = 0;
    initial_states[1]->num_restarts = 1;
    initial_states[1]->num_batches_before_change = 4;
    initial_states[1]->options.use_objective = true;
    context.states->ConfigureNextLubyRestart(initial_states.back());
    context.states->ConfigureNextLubyRestart(initial_states.back());

    context.response->NewSolution(std::vector<int64_t>{1, 0}, "initial");
    context.response->Synchronize();

    std::array<std::function<void()>, 2> tasks = {first->GenerateTask(0),
                                                  second->GenerateTask(1)};
    std::array<LsState*, 2> assigned_states;
    for (const int index : {reverse ? 1 : 0, reverse ? 0 : 1}) {
      assigned_states[index] =
          RunTaskAndFindRestartedState(tasks[index], initial_states);
    }
    ASSERT_EQ(assigned_states[0], initial_states[0]);
    ASSERT_EQ(assigned_states[1], initial_states[1]);
    EXPECT_EQ(assigned_states[0]->num_batches_before_change, 2);
    EXPECT_EQ(assigned_states[1]->num_batches_before_change, 1);
    EXPECT_EQ(assigned_states[0]->num_solutions_imported, 2);
    EXPECT_EQ(assigned_states[1]->num_solutions_imported, 1);
    EXPECT_FALSE(assigned_states[1]->options.use_objective);
    first->Synchronize();
    second->Synchronize();
  }
}

TEST(ViolationLsSolverTest,
     InterleavedStatesSurviveStaggeredRebuildsAndCancelledInitialization) {
  for (const bool reverse : {false, true}) {
    SCOPED_TRACE(reverse);
    ViolationLsTestContext context;
    context.proto.clear_variables();
    for (int i = 0; i < 10; ++i) {
      auto* var = context.proto.add_variables();
      var->add_domain(0);
      var->add_domain(10);
    }
    // Contradictory constraints keep both workers searching across batches.
    for (const auto& [lb, ub] : {std::pair{15, 20}, std::pair{0, 10}}) {
      auto* ct = context.proto.add_constraints()->mutable_linear();
      ct->add_vars(0);
      ct->add_vars(1);
      ct->add_coeffs(1);
      ct->add_coeffs(1);
      ct->add_domain(lb);
      ct->add_domain(ub);
    }
    // Make the model large enough that EstimateCopyDeterministicTime() keeps
    // the second worker rate-limited across its 14 small batches despite the
    // fixed per-batch timer offsets.
    for (int c = 0; c < 60; ++c) {
      auto* ct = context.proto.add_constraints()->mutable_linear();
      for (int i = 2; i < 10; ++i) {
        ct->add_vars(i);
        ct->add_coeffs(1);
      }
      ct->add_domain(0);
      ct->add_domain(80);
    }

    SharedBoundsManager shared_bounds(context.proto);
    SharedClausesManager shared_clauses(true);
    context.params.set_feasibility_jump_restart_factor(100);
    context.params.set_feasibility_jump_batch_dtime(1e-3);
    auto first = context.MakeSolver(/*improve_solutions=*/false, &shared_bounds,
                                    &shared_clauses);
    context.params.set_feasibility_jump_batch_dtime(1e-7);
    context.params.set_random_seed(2);
    auto second = context.MakeSolver(/*improve_solutions=*/false,
                                     &shared_bounds, &shared_clauses);
    const auto run_batch = [&](int batch) {
      std::array<std::function<void()>, 2> tasks = {
          first->GenerateTask(2 * batch), second->GenerateTask(2 * batch + 1)};
      tasks[reverse ? 1 : 0]();
      tasks[reverse ? 0 : 1]();
      std::array<LsState*, 2> states = {
          ViolationLsSolverTestPeer::State(*first),
          ViolationLsSolverTestPeer::State(*second)};
      first->Synchronize();
      second->Synchronize();
      return states;
    };
    for (int batch = 0; batch < 4; ++batch) run_batch(batch);

    // The first worker rebuilds to eight dense variables during its task,
    // after reserving a state using its previous mapping. The second worker
    // stays rate-limited at ten variables and shares the same state pool.
    shared_bounds.ReportPotentialNewBounds("sat", {2, 3}, {4, 4}, {4, 4});
    shared_bounds.Synchronize();
    for (int batch = 4; batch < 14; ++batch) {
      const auto states = run_batch(batch);
      ASSERT_NE(states[0], nullptr);
      ASSERT_NE(states[1], nullptr);
      EXPECT_NE(states[0], states[1]);
      EXPECT_EQ(states[0]->solution.size(), 8);
      ASSERT_NE(states[0]->move, nullptr);
      EXPECT_EQ(states[0]->move->NumVariables(), 8);
      EXPECT_EQ(states[1]->solution.size(), 10);
      ASSERT_NE(states[1]->move, nullptr);
      EXPECT_EQ(states[1]->move->NumVariables(), 10);
      for (const LsState* state : states) {
        ASSERT_EQ(state->input_solution.size(), 10);
        EXPECT_EQ(state->input_solution[2], 4);
        EXPECT_EQ(state->input_solution[3], 4);
      }
    }

    // Leave only a compacted state available to a fresh, uninitialized worker.
    // Cancellation before ImportState() must not reverse-map that state using
    // the fresh worker's ten-variable mapping.
    std::vector<LsState*> held_states;
    LsState* compacted = nullptr;
    for (int i = 0; i < 8; ++i) {
      LsState* state = context.states->GetNextState();
      held_states.push_back(state);
      if (state->solution.size() == 8) compacted = state;
    }
    ASSERT_NE(compacted, nullptr);
    const auto saved_solution = compacted->solution;
    const auto saved_input_solution = compacted->input_solution;
    context.states->Release(compacted);
    auto cancelled = context.MakeSolver(/*improve_solutions=*/false,
                                        &shared_bounds, &shared_clauses);
    auto task = cancelled->GenerateTask(100);
    ASSERT_EQ(ViolationLsSolverTestPeer::State(*cancelled), compacted);
    context.time_limit->Stop();
    task();
    EXPECT_EQ(compacted->solution, saved_solution);
    EXPECT_EQ(compacted->input_solution, saved_input_solution);
    cancelled->Synchronize();
    EXPECT_FALSE(ViolationLsSolverTestPeer::TaskGenerated(*cancelled));
    LsState* next = context.states->GetNextState();
    EXPECT_EQ(next, compacted);
    context.states->Release(next);
    for (LsState* state : held_states) {
      if (state != compacted) context.states->Release(state);
    }
  }
}

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
  ViolationLsSolver fj("ls", /*find_first_solution=*/true,
                       /*improve_solutions=*/false, proto, params, ls_states,
                       &time_limit, &response, &shared_bounds, &shared_clauses,
                       &ls_hints, &stat_tables);

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
  ViolationLsSolver fj1("ls1", /*find_first_solution=*/true,
                        /*improve_solutions=*/false, proto, params1, ls_states,
                        &time_limit, &response, &shared_bounds, &shared_clauses,
                        &ls_hints, &stat_tables);
  ViolationLsSolver fj2("ls2", /*find_first_solution=*/true,
                        /*improve_solutions=*/false, proto, params2, ls_states,
                        &time_limit, &response, &shared_bounds, &shared_clauses,
                        &ls_hints, &stat_tables);

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

  ViolationLsSolver solver("ls", /*find_first_solution=*/true,
                           /*improve_solutions=*/true, model_proto, params,
                           ls_states, shared_time_limit, &shared_response,
                           &shared_bounds, &shared_clauses, &shared_hints,
                           &stat_tables);

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
  ViolationLsSolver fj("ls", /*find_first_solution=*/true,
                       /*improve_solutions=*/false, proto, params, ls_states,
                       &time_limit, &response, &shared_bounds, &shared_clauses,
                       &ls_hints, &stat_tables);
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
