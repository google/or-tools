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

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "gtest/gtest.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solver_test_util.h"
#include "ortools/constraint_solver/interval.h"
#include "ortools/constraint_solver/sequence_var.h"

ABSL_DECLARE_FLAG(bool, cp_use_cumulative_edge_finder);
ABSL_DECLARE_FLAG(bool, cp_use_cumulative_time_table);
ABSL_DECLARE_FLAG(bool, cp_use_sequence_high_demand_tasks);
ABSL_DECLARE_FLAG(bool, cp_use_all_possible_disjunctions);

// =====================================================================
// This file contains the unit tests for the resource constraint, both
// disjunctive and cumulative.
//
// Many instances used in the tests are taken from the following references:
//
// * Vilim-CP refers to 'Edge Finding Filtering Algorithm for Discrete
//     Cumulative Resources in O(kn log n)' by Petr Vilim, CP 2009,
//     http://vilim.eu/petr/cp2009.pdf.
// * Mercier-JOC refers to 'Edge Finding for Cumulative Scheduling', by Luc
//     Mercier and Pascal Van Hentenryck, Informs JOC 2008,
//     http://www.box.net/shared/gmg22mqitx.
// * Mercier-DO refers to 'Strong Polynomiality of Resource Constraint
//     Propagation' by Luc Mercier and Pascal Van Hentenryck, Discrete
//     Optimization 2007, http://dx.doi.org/10.1016/j.disopt.2007.01.001.
// * Baptiste-Book refers to 'Constraint Based Scheduling' by
//     Philippe Baptiste, Claude Le Pape, and Wim Nuijten, Kluwer's
//     International Series in Operations Research and Management Science.
//
// =====================================================================

namespace operations_research {
namespace {

// =====================================================================
//      Sequence Constraint
// =====================================================================

class SequenceTest : public testing::Test {
 protected:
  void SetUp() override { solver_ = std::make_unique<Solver>("SequenceTest"); }
  std::unique_ptr<Solver> solver_;
};

// A convenience function to create non-optional tasks using start_min and
// end_max instead of start min/max, which is more common in the literature.
IntervalVar* MakeTask(Solver* const solver, int64_t start_min, int64_t end_max,
                      int64_t duration, bool optional) {
  return solver->MakeFixedDurationIntervalVar(start_min, end_max - duration,
                                              duration, optional, "task");
}

const int kTaskSpecificationSizeNoOptional = 5;
const int kTaskSpecificationSizeWithOptional = 7;

const int kFalse = 0;
const int kTrue = 1;
const int kCantRun = 1;  // Status for an interval task that cannot be performed
const int kMayRun = 2;   // Status for an interval task that may be performed
const int kMustRun = 3;  // Status for an interval task that must be performed
const int kNA = kint32min;

// Verify the propagation of the sequence constraints on an instance specified
// by the given array. Interpreting the 1st dimension as rows, and the second
// as columns, before_and_after is a matrix where rows correspond to tasks, and
// the columns are:
// |--------|---before--------------------|----after------------------|
// |duration|start_min, end_max, optional |start_min, end_max, status |
// size is the number of tasks.
// Should be called during the search, so that it propagates.
void VerifyPropagationWithOptionals(
    Solver* const solver,
    const int before_and_after[][kTaskSpecificationSizeWithOptional],
    int size) {
  // Columns
  const int kDuration = 0;
  const int kStartMinBefore = 1;
  const int kEndMaxBefore = 2;
  const int kOptionalBefore = 3;
  const int kStartMinAfter = 4;
  const int kEndMaxAfter = 5;
  const int kStatusAfter = 6;
  // Construct instance
  std::vector<IntervalVar*> vars(size);
  for (int i = 0; i < size; ++i) {
    const int start_min = before_and_after[i][kStartMinBefore];
    const int duration = before_and_after[i][kDuration];
    const int end_max = before_and_after[i][kEndMaxBefore];
    const bool optional =
        static_cast<bool>(before_and_after[i][kOptionalBefore]);
    vars[i] = MakeTask(solver, start_min, end_max, duration, optional);
  }
  Constraint* const sequence = solver->MakeDisjunctiveConstraint(vars, "seq");
  // Propagate
  solver->AddConstraint(sequence);
  // Verify
  for (int i = 0; i < size; ++i) {
    const int start_min_after = before_and_after[i][kStartMinAfter];
    const int end_max_after = before_and_after[i][kEndMaxAfter];
    const int status_after = before_and_after[i][kStatusAfter];
    switch (status_after) {
      case kCantRun:
        EXPECT_FALSE(vars[i]->MayBePerformed());
        EXPECT_FALSE(vars[i]->MustBePerformed());
        break;
      case kMayRun:
        EXPECT_TRUE(vars[i]->MayBePerformed());
        EXPECT_FALSE(vars[i]->MustBePerformed());
        EXPECT_EQ(start_min_after, vars[i]->StartMin());
        EXPECT_EQ(end_max_after, vars[i]->EndMax());
        break;
      case kMustRun:
        EXPECT_TRUE(vars[i]->MayBePerformed());
        EXPECT_TRUE(vars[i]->MustBePerformed());
        EXPECT_EQ(start_min_after, vars[i]->StartMin());
        EXPECT_EQ(end_max_after, vars[i]->EndMax());
        break;
      default:
        ADD_FAILURE() << "Unexpected status value: " << status_after;
    }
  }
}

// Verify the propagation of the sequence constraints on an instance specified
// by the given array. Interpreting the 1st dimension as rows, and the second
// as columns, before_and_after is a matrix where rows correspond to taks, and
// the columns are:
// |--------|---before----------|----after----------|
// |duration|start_min, end_max |start_min, end_max |
// size is the number of tasks.
// Should be called during the search, so that it propagates
void VerifyPropagation(
    Solver* const solver,
    const int before_and_after[][kTaskSpecificationSizeNoOptional], int size) {
  // Columns
  const int kDuration = 0;
  const int kStartMinBefore = 1;
  const int kEndMaxBefore = 2;
  const int kStartMinAfter = 3;
  const int kEndMaxAfter = 4;
  // Construct instance
  std::vector<IntervalVar*> vars(size);
  for (int i = 0; i < size; ++i) {
    const int start_min = before_and_after[i][kStartMinBefore];
    const int duration = before_and_after[i][kDuration];
    const int end_max = before_and_after[i][kEndMaxBefore];
    vars[i] = MakeTask(solver, start_min, end_max, duration, false);
  }
  Constraint* const sequence = solver->MakeDisjunctiveConstraint(vars, "seq");
  // Propagate
  solver->AddConstraint(sequence);
  // Verify
  for (int i = 0; i < size; ++i) {
    const int start_min_after = before_and_after[i][kStartMinAfter];
    const int end_max_after = before_and_after[i][kEndMaxAfter];
    EXPECT_EQ(start_min_after, vars[i]->StartMin());
    EXPECT_EQ(end_max_after, vars[i]->EndMax());
  }
}

TEST_F(SequenceTest, PostOutsideSearch) {
  IntervalVar* const x = MakeTask(solver_.get(), 0, 1, 1, false);
  IntervalVar* const y = MakeTask(solver_.get(), 0, 1, 1, false);
  std::vector<IntervalVar*> vars = {x, y};
  Constraint* sequence = solver_->MakeDisjunctiveConstraint(vars, "seq");
  sequence->Post();
}

TEST_IN_PROPAGATION0(SequenceTest, BoundOptionalForward, solver_, true) {
  // In this test, an optional interval is made mandatory during search,
  // to check that the propagation will be invoked
  IntervalVar* const x = MakeTask(solver_, 0, 3, 1, true);
  IntervalVar* const y = MakeTask(solver_, 1, 2, 1, true);
  std::vector<IntervalVar*> vars = {x, y};
  SequenceVar* sequence_var =
      solver_->MakeDisjunctiveConstraint(vars, "seq")->MakeSequenceVar();
  std::vector<int> possible_firsts;
  std::vector<int> possible_lasts;
  sequence_var->ComputePossibleFirstsAndLasts(&possible_firsts,
                                              &possible_lasts);
  EXPECT_EQ(2, possible_firsts.size());
  sequence_var->RankFirst(0);
  // This should not propagate anything on x, as y is optional
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(3, x->EndMax());
  EXPECT_EQ(1, y->StartMin());
  EXPECT_EQ(2, y->EndMax());
  // Now bound the optional
  y->SetPerformed(true);
  // Propagation should be triggered
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(1, x->EndMax());
  EXPECT_EQ(1, y->StartMin());
  EXPECT_EQ(2, y->EndMax());
  return nullptr;
}

TEST_IN_PROPAGATION0(SequenceTest, BoundOptionalBackward, solver_, true) {
  // In this test, an optional interval is made mandatory during search,
  // to check that the propagation will be invoked
  IntervalVar* const x = MakeTask(solver_, 0, 3, 1, true);
  IntervalVar* const y = MakeTask(solver_, 1, 2, 1, true);
  std::vector<IntervalVar*> vars = {x, y};
  SequenceVar* sequence_var =
      solver_->MakeDisjunctiveConstraint(vars, "seq")->MakeSequenceVar();
  std::vector<int> possible_firsts;
  std::vector<int> possible_lasts;
  sequence_var->ComputePossibleFirstsAndLasts(&possible_firsts,
                                              &possible_lasts);
  EXPECT_EQ(2, possible_lasts.size());
  sequence_var->RankLast(0);
  // This should not propagate anything on x, as y is optional
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(3, x->EndMax());
  EXPECT_EQ(1, y->StartMin());
  EXPECT_EQ(2, y->EndMax());
  // Now bound the optional
  y->SetPerformed(true);
  // Propagation should be triggered
  EXPECT_EQ(2, x->StartMin());
  EXPECT_EQ(3, x->EndMax());
  EXPECT_EQ(1, y->StartMin());
  EXPECT_EQ(2, y->EndMax());
  return nullptr;
}

TEST_IN_PROPAGATION0(SequenceTest, NoDeduction, solver_, true) {
  const int kSize = 2;
  int instance[kSize][kTaskSpecificationSizeNoOptional] = {
      // |--------|---before----------|----after----------|
      // |duration|start_min, end_max |start_min, end_max |
      {1, 0, 2, 0, 2},
      {1, 0, 2, 0, 2}};
  VerifyPropagation(solver_, instance, kSize);
  return nullptr;
}

TEST_IN_PROPAGATION0(SequenceTest, InferNewStartMin, solver_, true) {
  const int kSize = 2;
  int instance[kSize][kTaskSpecificationSizeNoOptional] = {
      // |--------|---before----------|----after----------|
      // |duration|start_min, end_max |start_min, end_max |
      {2, 0, 4, 2, 4},
      {1, 1, 2, 1, 2}};
  VerifyPropagation(solver_, instance, kSize);
  return nullptr;
}

TEST_IN_PROPAGATION0(SequenceTest, InferNewStartMinOptional, solver_, true) {
  const int kSize = 2;
  int instance[kSize][kTaskSpecificationSizeWithOptional] = {
      // |--------|---before--------------------|----after------------------|
      // |duration|start_min, end_max, optional |start_min, end_max, status |
      {2, 0, 4, kTrue, 2, 4, kMayRun},
      {1, 1, 2, kFalse, 1, 2, kMustRun}};
  VerifyPropagationWithOptionals(solver_, instance, kSize);
  return nullptr;
}

TEST_IN_PROPAGATION0(SequenceTest, InferOptionalCantRun, solver_, true) {
  const int kSize = 2;
  int instance[kSize][kTaskSpecificationSizeWithOptional] = {
      // |--------|---before--------------------|----after------------------|
      // |duration|start_min, end_max, optional |start_min, end_max, status |
      {2, 0, 3, kTrue, kNA, kNA, kCantRun},
      {1, 1, 2, kFalse, 1, 2, kMustRun}};
  VerifyPropagationWithOptionals(solver_, instance, kSize);
  return nullptr;
}

TEST_IN_PROPAGATION0(SequenceTest, InferNewStartMax, solver_, true) {
  const int kSize = 2;
  int instance[kSize][kTaskSpecificationSizeNoOptional] = {
      // |--------|---before----------|----after----------|
      // |duration|start_min, end_max |start_min, end_max |
      {2, 0, 4, 0, 2},
      {1, 2, 3, 2, 3}};
  VerifyPropagation(solver_, instance, kSize);
  return nullptr;
}

TEST_IN_PROPAGATION0(SequenceTest, InferNewStartMaxOptional, solver_, true) {
  const int kSize = 2;
  int instance[kSize][kTaskSpecificationSizeWithOptional] = {
      // |--------|---before--------------------|----after------------------|
      // |duration|start_min, end_max, optional |start_min, end_max, status |
      {2, 0, 4, kTrue, 0, 2, kMayRun},
      {1, 2, 3, kFalse, 2, 3, kMustRun}};
  VerifyPropagationWithOptionals(solver_, instance, kSize);
  return nullptr;
}

TEST_IN_PROPAGATION0(SequenceTest, InferOptionalMaxCantRun, solver_, true) {
  const int kSize = 2;
  int instance[kSize][kTaskSpecificationSizeWithOptional] = {
      // |--------|---before--------------------|----after------------------|
      // |duration|start_min, end_max, optional |start_min, end_max, status |
      {2, 1, 4, kTrue, kNA, kNA, kCantRun},
      {1, 2, 3, kFalse, 2, 3, kMustRun}};
  VerifyPropagationWithOptionals(solver_, instance, kSize);
  return nullptr;
}

TEST_IN_PROPAGATION0(SequenceTest, InferFailure, solver_, false) {
  IntervalVar* const x = MakeTask(solver_, 0, 3, 2, false);
  IntervalVar* const y = MakeTask(solver_, 0, 3, 2, false);
  // Total duration = 4 > interval length = 3
  Constraint* sequence = nullptr;
  {
    std::vector<IntervalVar*> vars = {x, y};
    sequence = solver_->MakeDisjunctiveConstraint(vars, "seq");
  }
  solver_->AddConstraint(sequence);
  return nullptr;
}

TEST_IN_PROPAGATION0(SequenceTest, InferNewStartMin2, solver_, true) {
  // Here we have a bunch of tasks with a space of one in between. A task
  // of length 2 will be thrown all the way to the right
  const int kSize = 4;
  int instance[kSize][kTaskSpecificationSizeNoOptional] = {
      // |--------|---before----------|----after----------|
      // |duration|start_min, end_max |start_min, end_max |
      {2, 0, 10, 6, 10},
      {1, 1, 2, 1, 2},
      {1, 3, 4, 3, 4},
      {1, 5, 6, 5, 6}};
  VerifyPropagation(solver_, instance, kSize);
  return nullptr;
}

TEST_IN_PROPAGATION0(SequenceTest, BaptisteBookE, solver_, true) {
  // This is instance E taken from Baptiste-Book, page 95.
  // On this instance only NFNL makes a deduction: Edge Finding and Detectable
  // Precedences do nothing.
  const int kSize = 3;
  int instance[kSize][kTaskSpecificationSizeNoOptional] = {
      // |--------|---before----------|----after----------|
      // |duration|start_min, end_max |start_min, end_max |
      {2, 0, 5, 0, 5},
      {2, 0, 5, 0, 5},
      {1, 1, 100, 2, 100}};
  VerifyPropagation(solver_, instance, kSize);
  return nullptr;
}

TEST_IN_PROPAGATION0(SequenceTest, BaptisteBookEOptional, solver_, true) {
  // This is instance E taken from Baptiste-Book, page 95.
  // On this instance only NFNL makes a deduction: Edge Finding and Detectable
  // Precedences do nothing.
  const int kSize = 3;
  int instance[kSize][kTaskSpecificationSizeWithOptional] = {
      // |--------|---before--------------------|----after------------------|
      // |duration|start_min, end_max, optional |start_min, end_max, status |
      {2, 0, 5, false, 0, 5, kMustRun},
      {2, 0, 5, false, 0, 5, kMustRun},
      {1, 1, 100, true, 2, 100, kMayRun}};
  VerifyPropagationWithOptionals(solver_, instance, kSize);
  return nullptr;
}

TEST_IN_PROPAGATION0(SequenceTest, BaptisteBookA, solver_, true) {
  // This is instance A taken from Baptiste-Book, page 93.
  // On this instance Edge Finding makes a stronger deduction than other
  // algorithms.
  const int kSize = 4;
  int instance[kSize][kTaskSpecificationSizeNoOptional] = {
      // |--------|---before----------|----after----------|
      // |duration|start_min, end_max |start_min, end_max |
      {2, 0, 101, 3, 101},
      {1, 0, 3, 0, 3},
      {1, 0, 3, 0, 3},
      {1, 0, 3, 0, 3}};
  VerifyPropagation(solver_, instance, kSize);
  return nullptr;
}

TEST_IN_PROPAGATION0(SequenceTest, BaptisteBookAOptional, solver_, true) {
  // This is instance A taken from Baptiste-Book, page 93.
  // On this instance Edge Finding makes a stronger deduction than other
  // algorithms.
  const int kSize = 4;
  int instance[kSize][kTaskSpecificationSizeWithOptional] = {
      // |--------|---before--------------------|----after------------------|
      // |duration|start_min, end_max, optional |start_min, end_max, status |
      {2, 0, 101, true, 3, 101, kMayRun},
      {1, 0, 3, false, 0, 3, kMustRun},
      {1, 0, 3, false, 0, 3, kMustRun},
      {1, 0, 3, false, 0, 3, kMustRun}};
  VerifyPropagationWithOptionals(solver_, instance, kSize);
  return nullptr;
}

TEST_IN_PROPAGATION0(SequenceTest, BaptisteBookABoundOptional, solver_, true) {
  // This is instance A taken from Baptiste-Book, page 93.
  // On this instance Edge Finding makes a stronger deduction than other
  // algorithms.
  // We start with an optional interval that prevents the propagation, then
  // make it mandatory
  IntervalVar* const a = MakeTask(solver_, 2, 100, 2, false);
  IntervalVar* const b = MakeTask(solver_, 0, 3, 1, true);
  IntervalVar* const c = MakeTask(solver_, 0, 3, 1, false);
  IntervalVar* const d = MakeTask(solver_, 0, 3, 1, false);
  std::vector<IntervalVar*> tasks = {a, b, c, d};
  Constraint* const sequence = solver_->MakeDisjunctiveConstraint(tasks, "seq");
  // Propagate
  solver_->AddConstraint(sequence);
  // a should not have been pushed
  EXPECT_EQ(2, a->StartMin());
  // Now bound the optional interval
  b->SetPerformed(true);
  // This should push 'a'
  EXPECT_EQ(3, a->StartMin());
  return nullptr;
}

TEST_IN_PROPAGATION0(SequenceTest, OneInterval, solver_, true) {
  const int kSize = 1;
  int instance[kSize][kTaskSpecificationSizeNoOptional] = {
      // |--------|---before----------|----after----------|
      // |duration|start_min, end_max |start_min, end_max |
      {1, 0, 1, 0, 1}};
  VerifyPropagation(solver_, instance, kSize);
  return nullptr;
}

TEST_IN_PROPAGATION0(SequenceTest, AllDurationZero, solver_, true) {
  const int kSize = 3;
  int instance[kSize][kTaskSpecificationSizeNoOptional] = {
      // |--------|---before----------|----after----------|
      // |duration|start_min, end_max |start_min, end_max |
      {0, 0, 1, 0, 1},
      {0, 0, 1, 0, 1},
      {0, 0, 1, 0, 1}};
  VerifyPropagation(solver_, instance, kSize);
  return nullptr;
}

TEST_IN_PROPAGATION0(SequenceTest, SomeDurationZero, solver_, true) {
  const int kSize = 3;
  int instance[kSize][kTaskSpecificationSizeNoOptional] = {
      // |--------|---before----------|----after----------|
      // |duration|start_min, end_max |start_min, end_max |
      {0, 0, 3, 0, 3},
      {1, 0, 3, 0, 3},
      {2, 0, 3, 0, 3}};
  VerifyPropagation(solver_, instance, kSize);
  return nullptr;
}

TEST_IN_PROPAGATION0(SequenceTest, ZeroFixed, solver_, true) {
  const int kSize = 3;
  int instance[kSize][kTaskSpecificationSizeNoOptional] = {
      // |--------|---before----------|----after----------|
      // |duration|start_min, end_max |start_min, end_max |
      {0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0},
      {0, 0, 0, 0, 0}};
  VerifyPropagation(solver_, instance, kSize);
  return nullptr;
}

TEST_IN_PROPAGATION0(SequenceTest, ThetaLambdaNodeBug, solver_, true) {
  const int kSize = 8;
  // On this instance, there are 6 tasks that consume 14 units of energy
  // between 0 and 14. Therefore the 2 other tasks cannot start before 14.
  int instance[kSize][kTaskSpecificationSizeNoOptional] = {
      // |--------|---before----------|----after----------|
      // |duration|start_min, end_max |start_min, end_max |
      {3, 0, 14, 0, 14},     //
      {3, 0, 14, 0, 14},     //
      {3, 0, 14, 0, 14},     //
      {3, 0, 14, 0, 14},     //
      {1, 2, 100, 14, 100},  //
      {1, 2, 100, 14, 100},  //
      {1, 6, 8, 6, 8},       //
      {1, 6, 8, 6, 8}};
  VerifyPropagation(solver_, instance, kSize);
  return nullptr;
}

TEST_IN_PROPAGATION0(SequenceTest, NoPossibleInterval, solver_, true) {
  IntervalVar* const x = MakeTask(solver_, 0, 1, 1, true);
  x->SetPerformed(false);
  std::vector<IntervalVar*> vars = {x};
  Constraint* sequence = solver_->MakeDisjunctiveConstraint(vars, "seq");
  solver_->AddConstraint(sequence);
  return nullptr;
}

TEST_IN_PROPAGATION0(SequenceTest, NoInterval, solver_, true) {
  std::vector<IntervalVar*> vars;
  Constraint* sequence = solver_->MakeDisjunctiveConstraint(vars, "seq");
  solver_->AddConstraint(sequence);
  return nullptr;
}

TEST_F(SequenceTest, UnperformedIntervals) {
  Solver solver("SequenceTest::UnperformedIntervals");
  IntervalVar* foo = solver.MakeIntervalVar(0, 10, 0, 10, 0, 10, true, "foo");
  IntervalVar* bar = solver.MakeIntervalVar(0, 10, 0, 10, 0, 10, true, "bar");
  foo->SetPerformed(false);
  // Leave bar unspecified.

  std::vector<IntervalVar*> vars = {foo, bar};
  DisjunctiveConstraint* const ct =
      solver.MakeDisjunctiveConstraint(vars, "one at a time");
  EXPECT_FALSE(ct->DebugString().empty());
  solver.AddConstraint(ct);
  SequenceVar* const sequence_var = ct->MakeSequenceVar();

  Assignment* const assignment = solver.MakeAssignment();
  assignment->Add(sequence_var);
  std::vector<int> forward_sequence(1, 1);
  assignment->SetForwardSequence(sequence_var, forward_sequence);
  EXPECT_TRUE(solver.CheckAssignment(assignment));
}

TEST_F(SequenceTest, FlexibleDuration) {
  // A problem with two interval variables and a disjunctive constraints on
  // them. The domains of the intervals easily allow scheduling them, e.g.
  // "first" to (1, 2) and "second" to (2, 5). However, this example does not
  // get past the Theta tree filter in the disjunctive constraint if the Theta
  // tree uses interval->EndMin() for the ETA of the task. Using
  // interval->StartMin() + interval->DurationMin() instead makes it pass.
  const std::vector<IntervalVar*> vars = {
      solver_->MakeIntervalVar(0, 5, 1, 5, 5, 5, false, "second"),
      solver_->MakeIntervalVar(1, 5, 1, 4, 2, 5, false, "first")};
  solver_->AddConstraint(
      solver_->MakeDisjunctiveConstraint(vars, "disjunctive"));
  DecisionBuilder* const db =
      solver_->MakePhase(vars, Solver::INTERVAL_DEFAULT);
  solver_->NewSearch(db);
  EXPECT_TRUE(solver_->NextSolution());
  solver_->EndSearch();
}

// =====================================================================
//  Cumulative Constraint
// =====================================================================

// Test class for the cumulative edge finding.
// It disables all other cumulative propagators.
class CumulativeEFTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_use_cumulative_edge_finder, true);
    absl::SetFlag(&FLAGS_cp_use_cumulative_time_table, false);
    absl::SetFlag(&FLAGS_cp_use_sequence_high_demand_tasks, false);
    absl::SetFlag(&FLAGS_cp_use_all_possible_disjunctions, false);
    solver_ = std::make_unique<Solver>("CumulativeEFTest");
  }
  std::unique_ptr<Solver> solver_;
};

TEST_F(CumulativeEFTest, setupWithArray) {
  IntervalVar* i = solver_->MakeFixedDurationIntervalVar(0, 10, 3, true, "i");
  std::vector<IntervalVar*> intervals = {i};
  std::vector<int64_t> demands = {4};
  const int kCapacity = 5;
  Constraint* constraint =
      solver_->MakeCumulative(intervals, demands, kCapacity, "cumulative");
  solver_->AddConstraint(constraint);
}

TEST_F(CumulativeEFTest, setupWithVector) {
  IntervalVar* i = solver_->MakeFixedDurationIntervalVar(0, 10, 3, true, "i");
  std::vector<IntervalVar*> intervals;
  intervals.push_back(i);
  std::vector<int64_t> demands;
  demands.push_back(4);
  const int kCapacity = 5;
  Constraint* constraint =
      solver_->MakeCumulative(intervals, demands, kCapacity, "cumulative");
  solver_->AddConstraint(constraint);
}

// ------- Task specification constants ---------------------

// Number of columns
const int kTaskSpecificationSize = 8;

// Columns indices
const int kDuration = 0;
const int kDemand = 1;
const int kStartMinBefore = 2;
const int kEndMaxBefore = 3;
const int kOptionalBefore = 4;
const int kStartMinAfter = 5;
const int kEndMaxAfter = 6;
const int kStatusAfter = 7;

IntervalVar* MakeInterval(Solver* solver, int64_t start_min, int64_t end_max,
                          int64_t duration, bool optional) {
  const int start_max = end_max - duration;
  return solver->MakeFixedDurationIntervalVar(start_min, start_max, duration,
                                              optional, "interval");
}

// Verify the propagation of the sequence constraints on an instance specified
// by the given array. Interpreting the 1st dimension as rows, and the second
// as columns, before_and_after is a matrix where rows correspond to tasks, and
// the columns are:
// |----------------|---before--------------------|----after------------------|
// |duration, demand|start_min, end_max, optional |start_min, end_max, status |
// size is the number of tasks.
// Should be called during the search, so that it propagates.
// TODO(user) Find a way to remove all these code duplication, between
// VerifyFailure, VerifyPropagationWithOptionals, and the disjunctive versions
void VerifyFailure(Solver* const solver,
                   const int before_and_after[][kTaskSpecificationSize],
                   int size, int capacity) {
  // Create constraint
  Constraint* constraint = nullptr;
  {
    // A block to prevent leaks in case the solver fails
    std::vector<IntervalVar*> vars;
    std::vector<int64_t> demands;
    vars.reserve(size);
    demands.reserve(size);
    for (int i = 0; i < size; ++i) {
      const int* specification = before_and_after[i];
      const int duration = specification[kDuration];
      const int demand = specification[kDemand];
      const int start_min = specification[kStartMinBefore];
      const int end_max = specification[kEndMaxBefore];
      const bool optional = static_cast<bool>(specification[kOptionalBefore]);
      demands.push_back(demand);
      vars.push_back(
          MakeInterval(solver, start_min, end_max, duration, optional));
      constraint = solver->MakeCumulative(vars, demands, capacity, "cumul");
    }
  }
  ASSERT_TRUE(nullptr != constraint);
  // Add it
  solver->AddConstraint(constraint);
  ADD_FAILURE() << "We expected the solver would fail, but it did not.";
}

// Verify the propagation of the sequence constraints on an instance specified
// by the given array. Interpreting the 1st dimension as rows, and the second
// as columns, before_and_after is a matrix where rows correspond to tasks, and
// the columns are:
// |----------------|---before--------------------|----after------------------|
// |duration, demand|start_min, end_max, optional |start_min, end_max, status |
// size is the number of tasks.
// Should be called during the search, so that it propagates.
void VerifyPropagationWithOptionals(
    Solver* const solver, const int before_and_after[][kTaskSpecificationSize],
    int size, int capacity) {
  // A block to prevent leaks in case the solver fails
  std::vector<IntervalVar*> vars;
  std::vector<int64_t> demands;
  vars.reserve(size);
  demands.reserve(size);
  for (int i = 0; i < size; ++i) {
    const int* specification = before_and_after[i];
    const int duration = specification[kDuration];
    const int demand = specification[kDemand];
    const int start_min = specification[kStartMinBefore];
    const int end_max = specification[kEndMaxBefore];
    const bool optional = static_cast<bool>(specification[kOptionalBefore]);
    demands.push_back(demand);
    vars.push_back(
        MakeInterval(solver, start_min, end_max, duration, optional));
  }
  // Create constraint
  Constraint* constraint =
      solver->MakeCumulative(vars, demands, capacity, "cumulative");
  ASSERT_NE(nullptr, constraint);
  // Add it
  solver->AddConstraint(constraint);
  // Check propagation
  for (int i = 0; i < size; ++i) {
    const int* specification = before_and_after[i];
    const int start_min_after = specification[kStartMinAfter];
    const int end_max_after = specification[kEndMaxAfter];
    const int status_after = specification[kStatusAfter];
    switch (status_after) {
      case kCantRun:
        EXPECT_FALSE(vars[i]->MayBePerformed()) << " i = " << i;
        EXPECT_FALSE(vars[i]->MustBePerformed()) << " i = " << i;
        break;
      case kMayRun:
        EXPECT_TRUE(vars[i]->MayBePerformed()) << " i = " << i;
        EXPECT_FALSE(vars[i]->MustBePerformed()) << " i = " << i;
        EXPECT_EQ(start_min_after, vars[i]->StartMin()) << " i = " << i;
        EXPECT_EQ(end_max_after, vars[i]->EndMax()) << " i = " << i;
        break;
      case kMustRun:
        EXPECT_TRUE(vars[i]->MayBePerformed()) << " i = " << i;
        EXPECT_TRUE(vars[i]->MustBePerformed()) << " i = " << i;
        EXPECT_EQ(start_min_after, vars[i]->StartMin()) << " i = " << i;
        EXPECT_EQ(end_max_after, vars[i]->EndMax()) << " i = " << i;
        break;
      default:
        ADD_FAILURE() << "Unexpected status value: " << status_after;
    }
  }
}

TEST_IN_PROPAGATION0(CumulativeEFTest, Overloaded, solver_, false) {
  const int kSize = 3;
  int instance[kSize][kTaskSpecificationSize] = {
      // duration, demand, then (start_min, end_max, optional) before and after.
      {1, 2, 5, 7, kFalse, kNA, kNA, kNA},
      {1, 2, 5, 7, kFalse, kNA, kNA, kNA},
      {1, 1, 5, 7, kFalse, kNA, kNA, kNA}};
  int kCapacity = 2;
  VerifyFailure(solver_, instance, kSize, kCapacity);
  return nullptr;
}

TEST_IN_PROPAGATION0(CumulativeEFTest, MandatoryTaskTooBig, solver_, false) {
  // In this instance a mandatory tasks exceeds the capacity, causing a failure.
  const int kSize = 1;
  int instance[kSize][kTaskSpecificationSize] = {
      // duration, demand, then (start_min, end_max, optional) before and after.
      {1, 3, 5, 7, kFalse, kNA, kNA, kNA}};
  int kCapacity = 2;
  VerifyFailure(solver_, instance, kSize, kCapacity);
  return nullptr;
}

TEST_IN_PROPAGATION0(CumulativeEFTest, OptionalTaskTooBig, solver_, true) {
  // In this instance an optional task exceeds the capacity, so it cannot run.
  const int kSize = 1;
  int instance[kSize][kTaskSpecificationSize] = {
      // duration, demand, then (start_min, end_max, optional) before and after.
      {1, 3, 5, 7, kTrue, kNA, kNA, kCantRun}};
  int kCapacity = 2;
  VerifyPropagationWithOptionals(solver_, instance, kSize, kCapacity);
  return nullptr;
}

TEST_IN_PROPAGATION0(CumulativeEFTest, NotOverloaded, solver_, true) {
  const int kSize = 3;
  int instance[kSize][kTaskSpecificationSize] = {
      // duration, demand, then (start_min, end_max, optional) before and after.
      {1, 2, 5, 7, kFalse, 5, 7, kMustRun},
      {1, 2, 5, 7, kFalse, 5, 7, kMustRun},
      {1, 1, 5, 8, kFalse, 7, 8, kMustRun}};
  int kCapacity = 2;
  VerifyPropagationWithOptionals(solver_, instance, kSize, kCapacity);
  return nullptr;
}

TEST_IN_PROPAGATION0(CumulativeEFTest, MercierJOCFig1, solver_, true) {
  // This is the instance from Mercier-JOC, fig 1.
  const int kSize = 5;
  int instance[kSize][kTaskSpecificationSize] = {
      // duration, demand, then (start_min, end_max, optional) before and after.
      {4, 1, 0, 69, kFalse, 2, 69, kMustRun},
      {1, 4, 1, 2, kFalse, 1, 2, kMustRun},
      {1, 2, 0, 3, kFalse, 0, 3, kMustRun},
      {1, 2, 0, 3, kFalse, 0, 3, kMustRun},
      {1, 1, 2, 3, kFalse, 2, 3, kMustRun}};
  int kCapacity = 4;
  VerifyPropagationWithOptionals(solver_, instance, kSize, kCapacity);
  return nullptr;
}

TEST_IN_PROPAGATION0(CumulativeEFTest, MercierJOCFig2, solver_, true) {
  // This is the instance from Mercier-JOC, fig 2.
  const int kSize = 3;
  int instance[kSize][kTaskSpecificationSize] = {
      // duration, demand, then (start_min, end_max, optional) before and after.
      {51, 1, 0, 69, kFalse, 2, 69, kMustRun},
      {4, 1, 1, 5, kFalse, 1, 5, kMustRun},
      {2, 1, 4, 6, kFalse, 4, 6, kMustRun}};
  int kCapacity = 2;
  VerifyPropagationWithOptionals(solver_, instance, kSize, kCapacity);
  return nullptr;
}

TEST_IN_PROPAGATION0(CumulativeEFTest, VilimInstanceFig1, solver_, true) {
  // This is the instance from Vilim-CP, fig 1.
  const int kSize = 4;
  int instance[kSize][kTaskSpecificationSize] = {
      // duration, demand, then (start_min, end_max, optional) before and after.
      {1, 3, 0, 5, kFalse, 0, 2, kMustRun},
      {3, 1, 2, 5, kFalse, 2, 5, kMustRun},
      {2, 2, 2, 5, kFalse, 2, 5, kMustRun},
      {3, 2, 0, 100, kFalse, 4, 100, kMustRun}};
  int kCapacity = 3;
  VerifyPropagationWithOptionals(solver_, instance, kSize, kCapacity);
  return nullptr;
}

TEST_IN_PROPAGATION0(CumulativeEFTest, VilimInstanceFig4, solver_, true) {
  // This is the instance from Vilim-CP, fig. 4.
  const int kSize = 3;
  int instance[kSize][kTaskSpecificationSize] = {
      // duration, demand, then (start_min, end_max, optional) before and after.
      {1, 3, 2, 5, kFalse, 2, 5, kMustRun},
      {2, 3, 2, 5, kFalse, 2, 5, kMustRun},
      {5, 1, 0, 10, kFalse, 5, 10, kMustRun}};
  int kCapacity = 3;
  VerifyPropagationWithOptionals(solver_, instance, kSize, kCapacity);
  return nullptr;
}

TEST_IN_PROPAGATION0(CumulativeEFTest, VilimInstanceFig5, solver_, true) {
  // This is the instance from Vilim-CP, fig. 5.
  const int kSize = 4;
  int instance[kSize][kTaskSpecificationSize] = {
      // duration, demand, then (start_min, end_max, optional) before and after.
      {2, 1, 0, 7, kFalse, 0, 7, kMustRun},
      {6, 1, 0, 7, kFalse, 0, 7, kMustRun},
      {1, 1, 6, 7, kFalse, 6, 7, kMustRun},
      {6, 1, 0, 100, kFalse, 2, 100, kMustRun}};
  int kCapacity = 2;
  VerifyPropagationWithOptionals(solver_, instance, kSize, kCapacity);
  return nullptr;
}

TEST_IN_PROPAGATION0(CumulativeEFTest, MercierDOPingPong, solver_, true) {
  // This is the 'Ping-Pong' instance from Mercier-DO, in the proof of
  // theorem 8. On this instance the edge-finder converges slowly to a fixed
  // point.
  const int p = 5;  // The parameter p in the instance description
  const int kSize = 8;
  int instance[kSize][kTaskSpecificationSize] = {
      // duration, demand, then (start_min, end_max, optional) before and after.
      {2 * p + 1, 1, -2 * p, p, kFalse, -2 * p, 2, kMustRun},
      {2 * p + 1, 1, -p, 2 * p, kFalse, -1, 2 * p, kMustRun},
      {1, 1, -2, 2, kFalse, -2, 2, kMustRun},
      {p - 1, 1, -2 * p, 0, kFalse, -2 * p, 0, kMustRun},
      {p - 1, 1, -2 * p, 0, kFalse, -2 * p, 0, kMustRun},
      {p - 1, 1, 0, 2 * p, kFalse, 0, 2 * p, kMustRun},
      {p - 1, 1, 0, 2 * p, kFalse, 0, 2 * p, kMustRun},
      {1, 1, -2 * p, 0, kFalse, -2 * p, 0, kMustRun}};
  int kCapacity = 2;
  VerifyPropagationWithOptionals(solver_, instance, kSize, kCapacity);
  return nullptr;
}

TEST_IN_PROPAGATION0(CumulativeEFTest, BaptisteBookI, solver_, false) {
  // This is instance I from Baptiste-Book, page 96.
  const int kSize = 3;
  int instance[kSize][kTaskSpecificationSize] = {
      // duration, demand, then (start_min, end_max, optional) before and after.
      {1, 1, 0, 1, kFalse, kNA, kNA, kNA},
      {1, 1, 0, 1, kFalse, kNA, kNA, kNA},
      {1, 1, 0, 1, kFalse, kNA, kNA, kNA}};
  int kCapacity = 2;
  VerifyFailure(solver_, instance, kSize, kCapacity);
  return nullptr;
}

TEST_IN_PROPAGATION0(CumulativeEFTest, BaptisteBookJ, solver_, true) {
  // This is instance J from Baptiste-Book, page 97.
  const int kSize = 3;
  int instance[kSize][kTaskSpecificationSize] = {
      // duration, demand, then (start_min, end_max, optional) before and after.
      {2, 2, 3, 5, kFalse, 3, 5, kMustRun},
      {2, 2, 3, 5, kFalse, 3, 5, kMustRun},
      {8, 2, 0, 8, kFalse, 0, 8, kMustRun}};
  int kCapacity = 5;
  VerifyPropagationWithOptionals(solver_, instance, kSize, kCapacity);
  return nullptr;
}

TEST_IN_PROPAGATION0(CumulativeEFTest, BaptisteBookL, solver_, true) {
  // This is instance L from Baptiste-Book, page 98.
  const int kSize = 3;
  int instance[kSize][kTaskSpecificationSize] = {
      // duration, demand, then (start_min, end_max, optional) before and after.
      {22, 2, 0, 100, kFalse, 9, 100, kMustRun},
      {24, 2, 0, 26, kFalse, 0, 26, kMustRun},
      {24, 2, 0, 26, kFalse, 0, 26, kMustRun}};
  int kCapacity = 5;
  VerifyPropagationWithOptionals(solver_, instance, kSize, kCapacity);
  return nullptr;
}

TEST_IN_PROPAGATION0(CumulativeEFTest, IntegerDivisionBug, solver_, true) {
  // This is a regression test for a past bug due to integer division
  const int kSize = 2;
  int instance[kSize][kTaskSpecificationSize] = {
      // duration, demand, then (start_min, end_max, optional) before and after.
      {15, 3, -60, -45, kFalse, -60, -45, kMustRun},
      {15, 2, -45, -30, kFalse, -45, -30, kMustRun}};
  int kCapacity = 3;
  VerifyPropagationWithOptionals(solver_, instance, kSize, kCapacity);
  return nullptr;
}

TEST_IN_PROPAGATION0(CumulativeEFTest, ManyDifferentDemands, solver_, true) {
  // In this test, there are many demands, so the small_map will use the
  // structure for non-small maps.
  const int kSize = 10;
  int instance[kSize][kTaskSpecificationSize] = {
      // duration, demand, then (start_min, end_max, optional) before and after.
      {1, 1, 0, 1, kFalse, 0, 1, kMustRun},
      {1, 4, 0, 1, kFalse, 0, 1, kMustRun},
      {1, 7, 0, 1, kFalse, 0, 1, kMustRun},
      {1, 9, 0, 1, kFalse, 0, 1, kMustRun},
      {1, 17, 0, 1, kFalse, 0, 1, kMustRun},
      {1, 35, 0, 1, kFalse, 0, 1, kMustRun},
      {1, 41, 0, 1, kFalse, 0, 1, kMustRun},
      {1, 42, 0, 1, kFalse, 0, 1, kMustRun},
      {1, 60, 0, 1, kFalse, 0, 1, kMustRun},
      {1, 99, 0, 1, kFalse, 0, 1, kMustRun}};
  int kCapacity = 1000;
  VerifyPropagationWithOptionals(solver_, instance, kSize, kCapacity);
  return nullptr;
}

TEST_IN_PROPAGATION0(CumulativeEFTest, OptionalTaskCapacity5, solver_, true) {
  // In this test, there is one optional task and one resource of capacity 5.
  // No overflow should appear.

  const int kSize = 1;
  int instance[kSize][kTaskSpecificationSize] = {
      // duration, demand, then (start_min, end_max, optional) before and after.
      {1, 1, 0, 1, kTrue, 0, 1, kMayRun}};
  int kCapacity = 5;
  VerifyPropagationWithOptionals(solver_, instance, kSize, kCapacity);
  return nullptr;
}

// Test class for the cumulative time table.
// It disables all other cumulative propagators.
class CumulativeTimeTableTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_use_cumulative_edge_finder, false);
    absl::SetFlag(&FLAGS_cp_use_cumulative_time_table, true);
    absl::SetFlag(&FLAGS_cp_use_sequence_high_demand_tasks, false);
    absl::SetFlag(&FLAGS_cp_use_all_possible_disjunctions, false);
    solver_ = std::make_unique<Solver>("CumulativeTimeTableTest");
  }
  std::unique_ptr<Solver> solver_;
};

TEST_IN_PROPAGATION0(CumulativeTimeTableTest, TimeTable, solver_, false) {
  // This is an infeasible instance on which the edge finder finds nothing.
  // Cumulative Time Table finds the contradiction.
  const int kSize = 2;
  int instance[kSize][kTaskSpecificationSize] = {
      // duration, demand, then (start_min, end_max, optional) before and after.
      {3, 2, 0, 4, kFalse, kNA, kNA, kNA},
      {3, 2, 1, 5, kFalse, kNA, kNA, kNA}};
  int kCapacity = 3;
  VerifyFailure(solver_, instance, kSize, kCapacity);
  return nullptr;
}

TEST_IN_PROPAGATION0(CumulativeTimeTableTest, TimeTablePush, solver_, true) {
  // This is an instance on Time Table pushes a task.
  const int kSize = 2;
  int instance[kSize][kTaskSpecificationSize] = {
      // duration, demand, then (start_min, end_max, optional) before and after.
      {1, 2, 1, 2, kFalse, 1, 2, kMustRun},
      {3, 2, 0, 10, kFalse, 2, 10, kMustRun}};
  int kCapacity = 3;
  VerifyPropagationWithOptionals(solver_, instance, kSize, kCapacity);
  return nullptr;
}

TEST_IN_PROPAGATION0(CumulativeTimeTableTest, TimeTablePush2, solver_, true) {
  // This is an instance on Time Table pushes a task.
  const int kSize = 3;
  int instance[kSize][kTaskSpecificationSize] = {
      // duration, demand, then (start_min, end_max, optional) before and after.
      {1, 2, 1, 2, kFalse, 1, 2, kMustRun},
      {1, 2, 3, 4, kFalse, 3, 4, kMustRun},
      {3, 2, 0, 10, kFalse, 4, 10, kMustRun}};
  int kCapacity = 3;
  VerifyPropagationWithOptionals(solver_, instance, kSize, kCapacity);
  return nullptr;
}

TEST_IN_PROPAGATION0(CumulativeTimeTableTest, TimeTablePush3, solver_, true) {
  // This is an instance on which Time Table pushes a task.
  // Here several tasks make up the bump.
  const int kSize = 3;
  int instance[kSize][kTaskSpecificationSize] = {
      // duration, demand, then (start_min, end_max, optional) before and after.
      {3, 1, 0, 4, kFalse, 0, 4, kMustRun},
      {3, 1, 1, 5, kFalse, 1, 5, kMustRun},
      {3, 2, 0, 10, kFalse, 3, 10, kMustRun}};
  // Here the two first tasks have the following profile:
  // usage ^
  //     2 |    **
  //     1 |  **--**
  //     0 |**------******************> time
  //        0 1 2 3 4 5 6
  // The interval [2, 3] has a profile too high to accommodate the third task.
  int kCapacity = 3;
  VerifyPropagationWithOptionals(solver_, instance, kSize, kCapacity);
  return nullptr;
}

TEST_IN_PROPAGATION0(CumulativeTimeTableTest, TimeTablePush4, solver_, true) {
  // This is an instance on which Time Table pushes a task.
  // Similar to TimeTablePush3, but the two small tasks have the same profile.
  const int kSize = 3;
  int instance[kSize][kTaskSpecificationSize] = {
      // duration, demand, then (start_min, end_max, optional) before and after.
      {4, 1, 0, 5, kFalse, 0, 5, kMustRun},
      {3, 1, 1, 4, kFalse, 1, 4, kMustRun},
      {3, 2, 0, 10, kFalse, 4, 10, kMustRun}};
  int kCapacity = 3;
  VerifyPropagationWithOptionals(solver_, instance, kSize, kCapacity);
  return nullptr;
}

TEST_IN_PROPAGATION0(CumulativeTimeTableTest, TimeTablePushRev, solver_, true) {
  // This is an instance on which Time Table pushes the end max of
  // a task down, whereas previous example pushed the start min up.
  // Here several tasks make up the bump.
  const int kSize = 3;
  int instance[kSize][kTaskSpecificationSize] = {
      // duration, demand, then (start_min, end_max, optional) before and after.
      {3, 1, 6, 10, kFalse, 6, 10, kMustRun},
      {3, 1, 5, 9, kFalse, 5, 9, kMustRun},
      {3, 2, 0, 10, kFalse, 0, 7, kMustRun}};
  int kCapacity = 3;
  VerifyPropagationWithOptionals(solver_, instance, kSize, kCapacity);
  return nullptr;
}

TEST_IN_PROPAGATION0(CumulativeTimeTableTest, RegressionTest, solver_, true) {
  // Regression test: there used to be a bug when no profile delta corresponded
  // to the start time of a task.
  const int kSize = 2;
  int instance[kSize][kTaskSpecificationSize] = {
      // duration, demand, then (start_min, end_max, optional) before and after.
      {3, 1, 0, 3, kFalse, 0, 3, kMustRun},
      {2, 1, 2, 5, kFalse, 3, 5, kMustRun}};
  int kCapacity = 1;
  VerifyPropagationWithOptionals(solver_, instance, kSize, kCapacity);
  return nullptr;
}

// Regression test: there used to be a bug when two back to back
// tasks where exceeding the capacity in the partial sum.
TEST_IN_PROPAGATION0(CumulativeTimeTableTest, PartialSumBug, solver_, true) {
  const int kSize = 7;
  int instance[kSize][kTaskSpecificationSize] = {
      {510, 142, 0, 510, kFalse, 0, 510, kMustRun},
      {268, 130, 242, 510, kFalse, 242, 510, kMustRun},
      {74, 147, 510, 584, kFalse, 510, 584, kMustRun},
      {197, 204, 584, 781, kFalse, 584, 781, kMustRun},
      {72, 138, 781, 853, kFalse, 781, 853, kMustRun},
      {170, 231, 853, 1023, kFalse, 853, 1023, kMustRun},
      {181, 131, 1023, 1204, kFalse, 1023, 1204, kMustRun},
  };
  int kCapacity = 315;
  VerifyPropagationWithOptionals(solver_, instance, kSize, kCapacity);
  return nullptr;
}

// Test class for the cumulative high-demand sequence.
// It disables all other cumulative propagators.
class CumulativeHighDemSeqTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_use_cumulative_edge_finder, false);
    absl::SetFlag(&FLAGS_cp_use_cumulative_time_table, false);
    absl::SetFlag(&FLAGS_cp_use_sequence_high_demand_tasks, true);
    absl::SetFlag(&FLAGS_cp_use_all_possible_disjunctions, false);
    solver_ = std::make_unique<Solver>("CumulativeTimeTableTest");
  }
  std::unique_ptr<Solver> solver_;
};

TEST_IN_PROPAGATION0(CumulativeHighDemSeqTest, BaptisteBookA, solver_, true) {
  // This is a variant of instance A taken Baptiste-Book, page 93.
  // Instance A is a disjunctive instance. Here it is made cumulative, but
  // with all tasks having a high demand (> 1/2 capacity).
  // On this instance, the strongest deduction is made by the disjunctive
  // Edge Finding on the high-demand tasks.
  const int kSize = 4;
  int instance[kSize][kTaskSpecificationSize] = {
      // duration, demand, then (start_min, end_max, optional) before and after.
      {2, 4, 0, 101, false, 3, 101, kMustRun},
      {1, 4, 0, 3, false, 0, 3, kMustRun},
      {1, 4, 0, 3, false, 0, 3, kMustRun},
      {1, 4, 0, 3, false, 0, 3, kMustRun}};
  int kCapacity = 7;
  VerifyPropagationWithOptionals(solver_, instance, kSize, kCapacity);
  return nullptr;
}

TEST_IN_PROPAGATION0(CumulativeHighDemSeqTest, Infeasible, solver_, false) {
  // This instance is infeasible, but the cumulative edge-finding and
  // cumulative time table are not able to make any deduction.
  // This is because the total energy of the tasks is 3 * 3 * 3 = 27,
  // while the available space on the only relevant interval is 5 * 8 = 40.
  const int kSize = 3;
  int instance[kSize][kTaskSpecificationSize] = {
      // duration, demand, then (start_min, end_max, optional) before and after.
      {3, 3, 0, 8, kFalse, kNA, kNA, kNA},
      {3, 3, 0, 8, kFalse, kNA, kNA, kNA},
      {3, 3, 0, 8, kFalse, kNA, kNA, kNA}};
  int kCapacity = 5;
  VerifyFailure(solver_, instance, kSize, kCapacity);
  return nullptr;
}

// Test class for the cumulative temporal disjunctions.
// It disables all other cumulative propagators.
class CumulativeDisjunctionsTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_use_cumulative_edge_finder, false);
    absl::SetFlag(&FLAGS_cp_use_cumulative_time_table, false);
    absl::SetFlag(&FLAGS_cp_use_sequence_high_demand_tasks, false);
    absl::SetFlag(&FLAGS_cp_use_all_possible_disjunctions, true);
    solver_ = std::make_unique<Solver>("CumulativeDisjunctionsTest");
  }
  std::unique_ptr<Solver> solver_;
};

TEST_IN_PROPAGATION0(CumulativeDisjunctionsTest, BaptisteBookK, solver_, true) {
  // This is instance K taken from Baptiste-Book, page 97.
  // On this instance, the strongest deduction is made by the temporal
  // disjunctions of tasks that cannot overlap.
  const int kSize = 2;
  int instance[kSize][kTaskSpecificationSize] = {
      // duration, demand, then (start_min, end_max, optional) before and after.
      {6, 2, 2, 100, false, 4, 100, kMustRun},
      {4, 5, 0, 9, false, 0, 9, kMustRun}};
  int kCapacity = 6;
  VerifyPropagationWithOptionals(solver_, instance, kSize, kCapacity);
  return nullptr;
}

// ----- Ranked Propagator Test -----

TEST_IN_PROPAGATION(RankedPropagatorTest, BugIssue33, solver, true) {
  static const int kHorizon = 20;
  static const int kDuration = 2;
  std::vector<IntervalVar*> intervals;
  solver->MakeFixedDurationIntervalVarArray(4, 0, kHorizon, kDuration, true,
                                            "interval", &intervals);
  DisjunctiveConstraint* const disj =
      solver->MakeDisjunctiveConstraint(intervals, "disjunctive");
  solver->AddConstraint(disj);
  SequenceVar* const sequence = disj->MakeSequenceVar();
  sequence->RankLast(2);
  sequence->RankLast(3);
  intervals[1]->SetPerformed(false);
  return nullptr;
}

TEST_IN_PROPAGATION(RankedPropagatorTest, TestSymmetricBugIssue33, solver,
                    true) {
  static const int kHorizon = 20;
  static const int kDuration = 2;
  std::vector<IntervalVar*> intervals;
  solver->MakeFixedDurationIntervalVarArray(4, 0, kHorizon, kDuration, true,
                                            "interval", &intervals);
  DisjunctiveConstraint* const disj =
      solver->MakeDisjunctiveConstraint(intervals, "disjunctive");
  solver->AddConstraint(disj);
  SequenceVar* const sequence = disj->MakeSequenceVar();
  sequence->RankFirst(1);
  sequence->RankFirst(0);
  intervals[3]->SetPerformed(false);
  return nullptr;
}

// ----- TransitionTimeTest -----

TEST_IN_PROPAGATION(TransitionTimeTest, PushWhenRankedFirst, solver, true) {
  static const int kHorizon = 20;
  static const int kDuration = 2;
  std::vector<IntervalVar*> intervals;
  solver->MakeFixedDurationIntervalVarArray(3, 0, kHorizon, kDuration, false,
                                            "interval", &intervals);
  // Distance is an anonymous code.
  std::function<int64_t(int64_t, int64_t)> distance = [](int64_t x, int64_t y) {
    return std::abs(x - y);
  };
  DisjunctiveConstraint* const disj =
      solver->MakeDisjunctiveConstraint(intervals, "disjunctive");
  disj->SetTransitionTime(distance);
  solver->AddConstraint(disj);
  EXPECT_EQ(0, intervals[0]->StartMin());
  EXPECT_EQ(kHorizon, intervals[0]->StartMax());
  EXPECT_EQ(0, intervals[1]->StartMin());
  EXPECT_EQ(kHorizon, intervals[1]->StartMax());
  EXPECT_EQ(0, intervals[2]->StartMin());
  EXPECT_EQ(kHorizon, intervals[2]->StartMax());
  SequenceVar* const sequence = disj->MakeSequenceVar();
  EXPECT_EQ(0, intervals[0]->StartMin());
  EXPECT_EQ(kHorizon, intervals[0]->StartMax());
  EXPECT_EQ(0, intervals[1]->StartMin());
  EXPECT_EQ(kHorizon, intervals[1]->StartMax());
  EXPECT_EQ(0, intervals[2]->StartMin());
  EXPECT_EQ(kHorizon, intervals[2]->StartMax());
  sequence->RankFirst(0);
  EXPECT_EQ(0, intervals[0]->StartMin());
  // TODO(user): Missing propagation, should be H - 2 * duration - (2 + 1).
  EXPECT_EQ(kHorizon - kDuration - 2, intervals[0]->StartMax());
  EXPECT_EQ(kDuration + 1, intervals[1]->StartMin());
  EXPECT_EQ(kHorizon, intervals[1]->StartMax());
  EXPECT_EQ(kDuration + 2, intervals[2]->StartMin());
  EXPECT_EQ(kHorizon, intervals[2]->StartMax());
  static const int kDelta = 3;
  intervals[0]->SetStartMin(kDelta);
  EXPECT_EQ(kDelta, intervals[0]->StartMin());
  EXPECT_EQ(kHorizon - kDuration - 2, intervals[0]->StartMax());
  EXPECT_EQ(kDuration + 1 + kDelta, intervals[1]->StartMin());
  EXPECT_EQ(kHorizon, intervals[1]->StartMax());
  EXPECT_EQ(kDuration + 2 + kDelta, intervals[2]->StartMin());
  EXPECT_EQ(kHorizon, intervals[2]->StartMax());
  return nullptr;
}

int64_t AbsDistance(int64_t i, int64_t j) { return std::abs(i - j); }

TEST_IN_PROPAGATION(TransitionTimeTest, PushWhenRankedLast, solver, true) {
  static const int kHorizon = 20;
  static const int kDuration = 2;
  std::vector<IntervalVar*> intervals;
  solver->MakeFixedDurationIntervalVarArray(3, 0, kHorizon, kDuration, false,
                                            "interval", &intervals);
  // Distance is a function.
  std::function<int64_t(int64_t, int64_t)> distance = AbsDistance;
  DisjunctiveConstraint* const disj =
      solver->MakeDisjunctiveConstraint(intervals, "disjunctive");
  disj->SetTransitionTime(distance);
  solver->AddConstraint(disj);
  EXPECT_EQ(0, intervals[0]->StartMin());
  EXPECT_EQ(kHorizon, intervals[0]->StartMax());
  EXPECT_EQ(0, intervals[1]->StartMin());
  EXPECT_EQ(kHorizon, intervals[1]->StartMax());
  EXPECT_EQ(0, intervals[2]->StartMin());
  EXPECT_EQ(kHorizon, intervals[2]->StartMax());
  SequenceVar* const sequence = disj->MakeSequenceVar();
  EXPECT_EQ(0, intervals[0]->StartMin());
  EXPECT_EQ(kHorizon, intervals[0]->StartMax());
  EXPECT_EQ(0, intervals[1]->StartMin());
  EXPECT_EQ(kHorizon, intervals[1]->StartMax());
  EXPECT_EQ(0, intervals[2]->StartMin());
  EXPECT_EQ(kHorizon, intervals[2]->StartMax());
  sequence->RankLast(0);
  // TODO(user): Missing propagation, should be H - 2 * duration - (2 + 1).
  EXPECT_EQ(kDuration + 2, intervals[0]->StartMin());
  EXPECT_EQ(kHorizon, intervals[0]->StartMax());
  EXPECT_EQ(0, intervals[1]->StartMin());
  EXPECT_EQ(kHorizon - kDuration - 1, intervals[1]->StartMax());
  EXPECT_EQ(0, intervals[2]->StartMin());
  EXPECT_EQ(kHorizon - kDuration - 2, intervals[2]->StartMax());
  static const int kDelta = 3;
  intervals[0]->SetStartMax(kHorizon - kDelta);
  EXPECT_EQ(kDuration + 2, intervals[0]->StartMin());
  EXPECT_EQ(kHorizon - kDelta, intervals[0]->StartMax());
  EXPECT_EQ(0, intervals[1]->StartMin());
  EXPECT_EQ(kHorizon - kDuration - 1 - kDelta, intervals[1]->StartMax());
  EXPECT_EQ(0, intervals[2]->StartMin());
  EXPECT_EQ(kHorizon - kDuration - 2 - kDelta, intervals[2]->StartMax());
  return nullptr;
}

TEST_IN_PROPAGATION(TransitionTimeTest, RankedWithOptional, solver, true) {
  static const int kHorizon = 20;
  static const int kDuration = 2;
  std::vector<IntervalVar*> intervals;
  solver->MakeFixedDurationIntervalVarArray(3, 0, kHorizon, kDuration, true,
                                            "interval", &intervals);
  // The function is passed directly to the function.
  DisjunctiveConstraint* const disj =
      solver->MakeDisjunctiveConstraint(intervals, "disjunctive");
  disj->SetTransitionTime(AbsDistance);
  solver->AddConstraint(disj);
  // No propagation
  EXPECT_EQ(0, intervals[0]->StartMin());
  EXPECT_EQ(kHorizon, intervals[0]->StartMax());
  EXPECT_EQ(0, intervals[1]->StartMin());
  EXPECT_EQ(kHorizon, intervals[1]->StartMax());
  EXPECT_EQ(0, intervals[2]->StartMin());
  EXPECT_EQ(kHorizon, intervals[2]->StartMax());
  SequenceVar* const sequence = disj->MakeSequenceVar();
  EXPECT_EQ(0, intervals[0]->StartMin());
  EXPECT_EQ(kHorizon, intervals[0]->StartMax());
  EXPECT_EQ(0, intervals[1]->StartMin());
  EXPECT_EQ(kHorizon, intervals[1]->StartMax());
  EXPECT_EQ(0, intervals[2]->StartMin());
  EXPECT_EQ(kHorizon, intervals[2]->StartMax());
  sequence->RankFirst(0);
  // RankFirst pushes the others.
  EXPECT_EQ(0, intervals[0]->StartMin());
  EXPECT_EQ(kHorizon, intervals[0]->StartMax());
  EXPECT_EQ(kDuration + 1, intervals[1]->StartMin());
  EXPECT_EQ(kHorizon, intervals[1]->StartMax());
  EXPECT_EQ(kDuration + 2, intervals[2]->StartMin());
  EXPECT_EQ(kHorizon, intervals[2]->StartMax());
  sequence->RankLast(1);
  // RankLast pushes the others back, but the optional does not push any
  // interval.
  EXPECT_EQ(0, intervals[0]->StartMin());
  EXPECT_EQ(kHorizon - kDuration - 1, intervals[0]->StartMax());
  EXPECT_EQ(kDuration + 1, intervals[1]->StartMin());
  EXPECT_EQ(kHorizon, intervals[1]->StartMax());
  EXPECT_EQ(kDuration + 2, intervals[2]->StartMin());
  EXPECT_EQ(kHorizon - kDuration - 1, intervals[2]->StartMax());
  intervals[2]->SetPerformed(true);
  // Now that is is performed, it pushes everything correctly.
  EXPECT_EQ(0, intervals[0]->StartMin());
  EXPECT_EQ(kHorizon - 2 * kDuration - 3, intervals[0]->StartMax());
  EXPECT_EQ(kDuration * 2 + 3, intervals[1]->StartMin());
  EXPECT_EQ(kHorizon, intervals[1]->StartMax());
  EXPECT_EQ(kDuration + 2, intervals[2]->StartMin());
  EXPECT_EQ(kHorizon - kDuration - 1, intervals[2]->StartMax());
  return nullptr;
}

// Test cumulative with variable demands and variable capacity.

TEST_IN_PROPAGATION(VariableCumulativeTest, VariableDemand, solver, true) {
  IntervalVar* const i1 =
      solver->MakeFixedDurationIntervalVar(0, 0, 5, false, "i1");
  IntervalVar* const i2 =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, false, "i2");
  std::vector<IntervalVar*> intervals = {i1, i2};
  IntVar* const d1 = solver->MakeIntVar(0, 3, "d1");
  IntVar* const d2 = solver->MakeIntVar(0, 3, "d2");
  std::vector<IntVar*> demands = {d1, d2};
  const int capacity = 4;
  Constraint* const constraint =
      solver->MakeCumulative(intervals, demands, capacity, "cumulative");
  // Add it
  solver->AddConstraint(constraint);
  // Check propagation
  CHECK_EQ(0, i2->StartMin());
  CHECK_EQ(10, i2->StartMax());
  d1->SetMin(3);
  CHECK_EQ(0, i2->StartMin());
  CHECK_EQ(10, i2->StartMax());
  d2->SetMin(1);
  CHECK_EQ(0, i2->StartMin());
  CHECK_EQ(10, i2->StartMax());
  // Force propagation.
  d2->SetMin(2);
  CHECK_EQ(5, i2->StartMin());
  CHECK_EQ(10, i2->StartMax());
  return nullptr;
}

TEST_IN_PROPAGATION(VariableCumulativeTest, VariableCapacity, solver, true) {
  IntervalVar* const i1 =
      solver->MakeFixedDurationIntervalVar(0, 0, 5, false, "i1");
  IntervalVar* const i2 =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, false, "i2");
  std::vector<IntervalVar*> intervals = {i1, i2};
  IntVar* const d1 = solver->MakeIntVar(2, 3, "d1");
  IntVar* const d2 = solver->MakeIntVar(2, 3, "d2");
  std::vector<IntVar*> demands = {d1, d2};
  IntVar* const capacity = solver->MakeIntVar(2, 4);
  Constraint* const constraint =
      solver->MakeCumulative(intervals, demands, capacity, "cumulative");
  // Add it
  solver->AddConstraint(constraint);
  // Check propagation
  CHECK_EQ(0, i2->StartMin());
  CHECK_EQ(10, i2->StartMax());
  // Force propagation.
  capacity->SetMax(3);
  CHECK_EQ(5, i2->StartMin());
  CHECK_EQ(10, i2->StartMax());
  return nullptr;
}

TEST(VariableCumulativeTest, BooleanDemand) {
  Solver solver("VariableCumulativeTest::BooleanDemand");
  IntervalVar* const task1 =
      solver.MakeFixedDurationIntervalVar(0, 15, 10, false, "task1");
  IntervalVar* const task2 =
      solver.MakeFixedDurationIntervalVar(3, 15, 10, false, "task2");
  const std::vector<IntervalVar*> tasks = {task1, task2};

  IntVar* const demand1 = solver.MakeIntVar(0, 1, "demand1");
  IntVar* const demand2 = solver.MakeIntVar(0, 1, "demand2");

  const std::vector<IntVar*> demands = {demand1, demand2};

  // Each resource cannot be used simultaneously by more than one post task
  solver.AddConstraint(solver.MakeCumulative(tasks, demands, 1, "cumul"));

  solver.AddConstraint(solver.MakeEquality(demand1, 1));
  solver.AddConstraint(solver.MakeEquality(demand2, 0));
  solver.AddConstraint(solver.MakeEquality(task1->StartExpr(), 0));

  // Search setup and solving
  DecisionBuilder* const dbInterval =
      solver.MakePhase(tasks, Solver::INTERVAL_DEFAULT);
  EXPECT_TRUE(solver.Solve(dbInterval));
}

TEST(VariableCumulativeTest, ZeroDemandAtFirst) {
  Solver solver("VariableCumulativeTest::ZeroDemandAtFirst");
  IntervalVar* const task1 =
      solver.MakeFixedDurationIntervalVar(0, 5, 5, false, "task1");
  IntervalVar* const task2 =
      solver.MakeFixedDurationIntervalVar(0, 5, 5, false, "task2");
  IntervalVar* const task3 =
      solver.MakeFixedDurationIntervalVar(0, 5, 5, false, "task3");
  IntervalVar* const task4 =
      solver.MakeFixedDurationIntervalVar(0, 5, 5, false, "task4");
  const std::vector<IntervalVar*> tasks = {task1, task2, task3, task4};
  IntVar* const demand1 = solver.MakeIntVar(0, 1, "demand1");
  IntVar* const demand2 = solver.MakeIntVar(0, 1, "demand2");
  IntVar* const demand3 = solver.MakeIntVar(0, 1, "demand3");
  IntVar* const demand4 = solver.MakeIntVar(0, 1, "demand4");
  // Each resource cannot be used simultaneously by more than one post task
  const std::vector<IntVar*> demands = {demand1, demand2, demand3, demand4};
  solver.AddConstraint(solver.MakeCumulative(tasks, demands, 1, "cumul"));
  // One task per group must be active.
  const std::vector<IntVar*> group1 = {demand1, demand2};
  const std::vector<IntVar*> group2 = {demand3, demand4};
  solver.AddConstraint(solver.MakeSumGreaterOrEqual(group1, 1));
  solver.AddConstraint(solver.MakeSumGreaterOrEqual(group2, 1));
  // Search setup and solving
  DecisionBuilder* const db_demand = solver.MakePhase(
      demands, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  DecisionBuilder* const db_interval =
      solver.MakePhase(tasks, Solver::INTERVAL_DEFAULT);
  DecisionBuilder* const db = solver.Compose(db_demand, db_interval);
  int count = 0;
  solver.NewSearch(db);
  while (solver.NextSolution()) {
    count++;
  }
  solver.EndSearch();
  EXPECT_EQ(8, count);
}
}  // namespace
}  // namespace operations_research
