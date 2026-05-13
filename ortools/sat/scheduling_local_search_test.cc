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

#include "ortools/sat/scheduling_local_search.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/scheduling_model.h"
#include "ortools/sat/util.h"

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::UnorderedElementsAre;

namespace operations_research {
namespace sat {
namespace {

class JSSPSolverTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // 1. Setup the Problem
    problem_.tasks = {
        {0, 10, {}},  // Task 0: M0, Dur 10
        {0, 10, {}},  // Task 1: M0, Dur 10
        {0, 10, {}},  // Task 2: M0, Dur 10
        {1, 10, {2}}  // Task 3: M1, Dur 10. Depends on Task 2
    };

    // 2. Setup the Initial Sequence (M0: 0->1->2, M1: 3)
    machine_tasks_.ResetFromPairs(
        std::vector<std::pair<int, int>>({{0, 0}, {0, 1}, {0, 2}, {1, 3}}));
  }

  struct SchedLSTest : public SchedulingLocalSearch {
   public:
    explicit SchedLSTest(const SchedulingProblem& problem)
        : SchedulingLocalSearch(problem) {}

    using SchedulingLocalSearch::AnalyzeSchedule;
    using SchedulingLocalSearch::BuildInitialMachineSequences;
    using SchedulingLocalSearch::ComputeDynamicState;
    using SchedulingLocalSearch::EstimateMakespanForInsert;
    using SchedulingLocalSearch::GenerateN8Moves;
    using SchedulingLocalSearch::InsertMove;
    using SchedulingLocalSearch::MoveEvaluationScratch;
    using SchedulingLocalSearch::ScheduleAnalysis;
    using SchedulingLocalSearch::SolverState;
  };

  SchedulingProblem problem_;
  CompactVectorVector<int> machine_tasks_;
};

TEST_F(JSSPSolverTest, AnalyzeScheduleComputesCorrectGraph) {
  const SchedLSTest solver(problem_);
  SchedLSTest::ScheduleAnalysis analysis(problem_);
  solver.AnalyzeSchedule(machine_tasks_, &analysis);

  // Check Makespan
  EXPECT_EQ(analysis.makespan, 40);

  // Check Heads (Earliest Start Times)
  EXPECT_THAT(analysis.start_mins, ElementsAre(0, 10, 20, 30));

  // Check Critical Path
  EXPECT_THAT(analysis.critical_path, ElementsAre(0, 1, 2, 3));

  // Check Topo Order
  EXPECT_THAT(analysis.topo_order[3], Eq(3));
  EXPECT_THAT(absl::MakeSpan(analysis.topo_order).subspan(0, 3),
              UnorderedElementsAre(0, 1, 2));
}

TEST_F(JSSPSolverTest, ComputeDynamicStateBuildsCorrectTailsAndLookups) {
  const SchedLSTest solver(problem_);
  SchedLSTest::ScheduleAnalysis analysis(problem_);
  solver.AnalyzeSchedule(machine_tasks_, &analysis);
  const SchedLSTest::SolverState state =
      solver.ComputeDynamicState(machine_tasks_, analysis.topo_order);

  // Check Tails
  EXPECT_THAT(state.tails, ElementsAre(30, 20, 10, 0));

  // Check Machine Lookups
  EXPECT_THAT(state.prev_on_machine, ElementsAre(-1, 0, 1, -1));
  EXPECT_THAT(state.next_on_machine, ElementsAre(1, 2, -1, -1));

  // Check Position Lookup
  EXPECT_THAT(state.position_in_machine, ElementsAre(0, 1, 2, 0));
}

TEST_F(JSSPSolverTest, GenerateN8MovesExtractsValidInsertions) {
  const SchedLSTest solver(problem_);
  SchedLSTest::ScheduleAnalysis analysis(problem_);
  solver.AnalyzeSchedule(machine_tasks_, &analysis);
  const SchedLSTest::SolverState state =
      solver.ComputeDynamicState(machine_tasks_, analysis.topo_order);

  const std::vector<SchedLSTest::InsertMove> moves = solver.GenerateN8Moves(
      analysis.critical_path, state.prev_on_machine, state.next_on_machine,
      analysis.start_mins, state.tails);

  // The critical path is [0, 1, 2, 3].
  // Blocks: Block 0 is [0, 1, 2] on M0. Block 1 is [3] on M1.
  // Block 0 is the FIRST block, so it tests Last-Op / Rightward rules.
  // Valid unique moves generated for Block 0:
  // 1. Task 0 moving right after last_op (2).
  // 2. Task 1 moving right after last_op (2).
  // Note: Moving 2 before 1 is functionally identical to moving 1 after 2,
  // and the loop bounds (end - 2) intentionally prevent this duplicate.
  ASSERT_EQ(moves.size(), 2);

  // Task 0 placed after Task 2
  EXPECT_EQ(moves[0].task, 0);
  EXPECT_EQ(moves[0].target_task, 2);
  EXPECT_EQ(moves[0].place_after, true);

  // Task 1 placed after Task 2
  EXPECT_EQ(moves[1].task, 1);
  EXPECT_EQ(moves[1].target_task, 2);
  EXPECT_EQ(moves[1].place_after, true);
}

TEST_F(JSSPSolverTest, EstimateMakespanForInsertAccuratelyPredictsImprovement) {
  const SchedLSTest solver(problem_);
  SchedLSTest::ScheduleAnalysis analysis(problem_);
  solver.AnalyzeSchedule(machine_tasks_, &analysis);
  const SchedLSTest::SolverState state =
      solver.ComputeDynamicState(machine_tasks_, analysis.topo_order);

  // We propose taking Task 1 and inserting it AFTER Task 2.
  // Old M0 sequence: 0 -> 1 -> 2
  // New M0 sequence: 0 -> 2 -> 1
  const SchedLSTest::InsertMove insert_move{1, 2, true};
  SchedLSTest::MoveEvaluationScratch scratch;

  const IntegerValue estimated_makespan = solver.EstimateMakespanForInsert(
      insert_move, analysis.start_mins, state.tails, state.prev_on_machine,
      state.next_on_machine, &scratch);

  // Moving T1 after T2 allows T2 to execute earlier at t=10.
  // Since T3 depends on T2, T3 can now execute at t=20 and finish at t=30.
  // T1 is pushed to t=20 (after T2) and finishes at t=30.
  // Both T1 and T3 paths terminate at 30, meaning the new makespan is
  // exactly 30.
  EXPECT_EQ(estimated_makespan, 30);
}

TEST_F(JSSPSolverTest, BuildInitialMachineSequencesHandlesDirtyHints) {
  // We use the same 4-task problem defined in the SetUp:
  // T0(M0), T1(M0), T2(M0), T3(M1)

  // Let's provide a completely non-canonical hint:
  // - It doesn't start at 0.
  // - It has massive, arbitrary idle gaps.
  // - We intentionally schedule them out of the standard 0->1->2 order.
  // We want Machine 0 to process in the order: Task 2 -> Task 0 -> Task 1.

  const std::vector<int64_t> dirty_hint = {
      500,   // Task 0 starts at 500
      1000,  // Task 1 starts at 1000
      50,    // Task 2 starts at 50
      200    // Task 3 starts at 200 (Machine 1)
  };

  const SchedLSTest solver(problem_);
  const CompactVectorVector<int> generated_sequences =
      solver.BuildInitialMachineSequences(dirty_hint);

  // We expect exactly 2 machines to be populated
  ASSERT_EQ(generated_sequences.size(), 2);

  // Machine 0 should extract the sequence purely based on the sorted start
  // times: T2 (50) comes first, then T0 (500), then T1 (1000).
  EXPECT_THAT(generated_sequences[0], ElementsAre(2, 0, 1));

  // Machine 1 only has Task 3.
  EXPECT_THAT(generated_sequences[1], ElementsAre(3));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
