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

#include <stddef.h>

#include <memory>
#include <string>

#include "gtest/gtest.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solver_test_util.h"
#include "ortools/constraint_solver/interval.h"

namespace operations_research {
namespace {

// ----- Unsafe Expr Test -----

void TestStartExpr(IntervalVar* const var) {
  IntExpr* const expr = var->StartExpr();
  EXPECT_EQ(var->StartMin(), expr->Min());
  EXPECT_EQ(var->StartMax(), expr->Max());
  if (var->StartMin() != var->StartMax()) {
    var->SetStartMin(var->StartMin() + 1);
    EXPECT_EQ(var->StartMin(), expr->Min());
    var->SetStartMax(var->StartMax() - 1);
    EXPECT_EQ(var->StartMax(), expr->Max());
    expr->SetMin(expr->Min() + 1);
    EXPECT_EQ(var->StartMin(), expr->Min());
    expr->SetMax(expr->Max() - 1);
    EXPECT_EQ(var->StartMax(), expr->Max());
  }
}

void TestDurationExpr(IntervalVar* const var) {
  IntExpr* const expr = var->DurationExpr();
  EXPECT_EQ(var->DurationMin(), expr->Min());
  EXPECT_EQ(var->DurationMax(), expr->Max());
  if (var->DurationMin() != var->DurationMax()) {
    var->SetDurationMin(var->DurationMin() + 1);
    EXPECT_EQ(var->DurationMin(), expr->Min());
    var->SetDurationMax(var->DurationMax() - 1);
    EXPECT_EQ(var->DurationMax(), expr->Max());
    expr->SetMin(expr->Min() + 1);
    EXPECT_EQ(var->DurationMin(), expr->Min());
    expr->SetMax(expr->Max() - 1);
    EXPECT_EQ(var->DurationMax(), expr->Max());
  }
}

void TestEndExpr(IntervalVar* const var) {
  IntExpr* const expr = var->EndExpr();
  EXPECT_EQ(var->EndMin(), expr->Min());
  EXPECT_EQ(var->EndMax(), expr->Max());
  if (var->EndMin() != var->EndMax()) {
    var->SetEndMin(var->EndMin() + 1);
    EXPECT_EQ(var->EndMin(), expr->Min());
    var->SetEndMax(var->EndMax() - 1);
    EXPECT_EQ(var->EndMax(), expr->Max());
    expr->SetMin(expr->Min() + 1);
    EXPECT_EQ(var->EndMin(), expr->Min());
    expr->SetMax(expr->Max() - 1);
    EXPECT_EQ(var->EndMax(), expr->Max());
  }
}

TEST_IN_PROPAGATION(StartExprTest, FixedIntervalVar, solver, true) {
  IntervalVar* const var = solver->MakeFixedInterval(0, 10, "var");
  TestStartExpr(var);
  return nullptr;
}

TEST_IN_PROPAGATION(DurationExprTest, FixedIntervalVar, solver, true) {
  IntervalVar* const var = solver->MakeFixedInterval(0, 10, "var");
  TestDurationExpr(var);
  return nullptr;
}

TEST_IN_PROPAGATION(EndExprTest, FixedIntervalVar, solver, true) {
  IntervalVar* const var = solver->MakeFixedInterval(0, 10, "var");
  TestEndExpr(var);
  return nullptr;
}

TEST_IN_PROPAGATION(StartExprTest, FixedDurationIntervalVar, solver, true) {
  IntervalVar* const var =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, true, "var");
  TestStartExpr(var);
  return nullptr;
}

TEST_IN_PROPAGATION(DurationExprTest, FixedDurationIntervalVar, solver, true) {
  IntervalVar* const var =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, true, "var");
  TestDurationExpr(var);
  return nullptr;
}

TEST_IN_PROPAGATION(EndExprTest, FixedDurationIntervalVar, solver, true) {
  IntervalVar* const var =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, true, "var");
  TestEndExpr(var);
  return nullptr;
}

TEST_IN_PROPAGATION(StartExprTest, FixedDurationPerformedIntervalVar, solver,
                    true) {
  IntervalVar* const var =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, false, "var");
  TestStartExpr(var);
  return nullptr;
}

TEST_IN_PROPAGATION(DurationExprTest, FixedDurationPerformedIntervalVar, solver,
                    true) {
  IntervalVar* const var =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, false, "var");
  TestDurationExpr(var);
  return nullptr;
}

TEST_IN_PROPAGATION(EndExprTest, FixedDurationPerformedIntervalVar, solver,
                    true) {
  IntervalVar* const var =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, false, "var");
  TestEndExpr(var);
  return nullptr;
}

TEST_IN_PROPAGATION(StartExprTest, IntervalVar, solver, true) {
  IntervalVar* const var =
      solver->MakeIntervalVar(0, 20, 0, 10, 0, 20, true, "var");
  TestStartExpr(var);
  return nullptr;
}

TEST_IN_PROPAGATION(DurationExprTest, IntervalVar, solver, true) {
  IntervalVar* const var =
      solver->MakeIntervalVar(0, 20, 0, 10, 0, 20, true, "var");
  TestDurationExpr(var);
  return nullptr;
}

TEST_IN_PROPAGATION(EndExprTest, IntervalVar, solver, true) {
  IntervalVar* const var =
      solver->MakeIntervalVar(0, 20, 0, 10, 0, 20, true, "var");
  TestEndExpr(var);
  return nullptr;
}

TEST_IN_PROPAGATION(StartExprTest, MirrorIntervalVar, solver, true) {
  IntervalVar* const inner =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, true, "var");
  IntervalVar* const var = solver->MakeMirrorInterval(inner);
  TestStartExpr(var);
  return nullptr;
}

TEST_IN_PROPAGATION(DurationExprTest, MirrorIntervalVar, solver, true) {
  IntervalVar* const inner =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, true, "var");
  IntervalVar* const var = solver->MakeMirrorInterval(inner);
  TestDurationExpr(var);
  return nullptr;
}

TEST_IN_PROPAGATION(EndExprTest, MirrorIntervalVar, solver, true) {
  IntervalVar* const inner =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, true, "var");
  IntervalVar* const var = solver->MakeMirrorInterval(inner);
  TestEndExpr(var);
  return nullptr;
}

TEST_IN_PROPAGATION(StartExprTest, StartSyncStartIntervalVar, solver, true) {
  IntervalVar* const inner =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, true, "var");
  IntervalVar* const var =
      solver->MakeFixedDurationStartSyncedOnStartIntervalVar(inner, 10, -2);
  TestStartExpr(var);
  return nullptr;
}

TEST_IN_PROPAGATION(DurationExprTest, StartSyncStartIntervalVar, solver, true) {
  IntervalVar* const inner =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, true, "var");
  IntervalVar* const var =
      solver->MakeFixedDurationStartSyncedOnStartIntervalVar(inner, 10, -2);
  TestDurationExpr(var);
  return nullptr;
}

TEST_IN_PROPAGATION(EndExprTest, StartSyncStartIntervalVar, solver, true) {
  IntervalVar* const inner =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, true, "var");
  IntervalVar* const var =
      solver->MakeFixedDurationStartSyncedOnStartIntervalVar(inner, 10, -2);
  TestEndExpr(var);
  return nullptr;
}

TEST_IN_PROPAGATION(StartExprTest, StartSyncEndIntervalVar, solver, true) {
  IntervalVar* const inner =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, true, "var");
  IntervalVar* const var =
      solver->MakeFixedDurationStartSyncedOnEndIntervalVar(inner, 10, -2);
  TestStartExpr(var);
  return nullptr;
}

TEST_IN_PROPAGATION(DurationExprTest, StartSyncEndIntervalVar, solver, true) {
  IntervalVar* const inner =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, true, "var");
  IntervalVar* const var =
      solver->MakeFixedDurationStartSyncedOnEndIntervalVar(inner, 10, -2);
  TestDurationExpr(var);
  return nullptr;
}

TEST_IN_PROPAGATION(EndExprTest, StartSyncEndIntervalVar, solver, true) {
  IntervalVar* const inner =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, true, "var");
  IntervalVar* const var =
      solver->MakeFixedDurationStartSyncedOnEndIntervalVar(inner, 10, -2);
  TestEndExpr(var);
  return nullptr;
}

TEST_IN_PROPAGATION(StartExprTest, EndSyncStartIntervalVar, solver, true) {
  IntervalVar* const inner =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, true, "var");
  IntervalVar* const var =
      solver->MakeFixedDurationEndSyncedOnStartIntervalVar(inner, 10, -2);
  TestStartExpr(var);
  return nullptr;
}

TEST_IN_PROPAGATION(DurationExprTest, EndSyncStartIntervalVar, solver, true) {
  IntervalVar* const inner =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, true, "var");
  IntervalVar* const var =
      solver->MakeFixedDurationEndSyncedOnStartIntervalVar(inner, 10, -2);
  TestDurationExpr(var);
  return nullptr;
}

TEST_IN_PROPAGATION(EndExprTest, EndSyncStartIntervalVar, solver, true) {
  IntervalVar* const inner =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, true, "var");
  IntervalVar* const var =
      solver->MakeFixedDurationEndSyncedOnStartIntervalVar(inner, 10, -2);
  TestEndExpr(var);
  return nullptr;
}

TEST_IN_PROPAGATION(StartExprTest, EndSyncEndIntervalVar, solver, true) {
  IntervalVar* const inner =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, true, "var");
  IntervalVar* const var =
      solver->MakeFixedDurationEndSyncedOnEndIntervalVar(inner, 10, -2);
  TestStartExpr(var);
  return nullptr;
}

TEST_IN_PROPAGATION(DurationExprTest, EndSyncEndIntervalVar, solver, true) {
  IntervalVar* const inner =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, true, "var");
  IntervalVar* const var =
      solver->MakeFixedDurationEndSyncedOnEndIntervalVar(inner, 10, -2);
  TestDurationExpr(var);
  return nullptr;
}

TEST_IN_PROPAGATION(EndExprTest, EndSyncEndIntervalVar, solver, true) {
  IntervalVar* const inner =
      solver->MakeFixedDurationIntervalVar(0, 10, 5, true, "var");
  IntervalVar* const var =
      solver->MakeFixedDurationEndSyncedOnEndIntervalVar(inner, 10, -2);
  TestEndExpr(var);
  return nullptr;
}

// ----- Safe Expr Test -----

class SafeStartTest : public testing::Test {
 protected:
  void SetUp() override { solver_ = std::make_unique<Solver>("CoverTest"); }
  std::unique_ptr<Solver> solver_;
};

TEST_IN_PROPAGATION0(SafeStartTest, DownGetters, s, true) {
  IntervalVar* const interval =
      s->MakeFixedDurationIntervalVar(10, 20, 5, true, "interval");
  IntExpr* const safe_start = interval->SafeStartExpr(0);
  EXPECT_EQ(0, safe_start->Min());
  EXPECT_EQ(20, safe_start->Max());
  interval->SetStartMin(12);
  EXPECT_EQ(0, safe_start->Min());
  EXPECT_EQ(20, safe_start->Max());
  interval->SetStartMax(18);
  EXPECT_EQ(0, safe_start->Min());
  EXPECT_EQ(18, safe_start->Max());
  interval->SetPerformed(true);
  EXPECT_EQ(12, safe_start->Min());
  EXPECT_EQ(18, safe_start->Max());
  return nullptr;
}

TEST_IN_PROPAGATION0(SafeStartTest, DownSetUnperformed, s, true) {
  IntervalVar* const interval =
      s->MakeFixedDurationIntervalVar(10, 20, 5, true, "interval");
  IntExpr* const safe_start = interval->SafeStartExpr(0);
  interval->SetPerformed(false);
  EXPECT_EQ(0, safe_start->Min());
  EXPECT_EQ(0, safe_start->Max());
  return nullptr;
}

TEST_IN_PROPAGATION0(SafeStartTest, DownSetMin, s, true) {
  IntervalVar* const interval =
      s->MakeFixedDurationIntervalVar(10, 20, 5, true, "interval");
  IntExpr* const safe_start = interval->SafeStartExpr(0);
  safe_start->SetMin(-2);
  EXPECT_EQ(0, safe_start->Min());
  EXPECT_EQ(10, interval->StartMin());
  EXPECT_EQ(20, interval->StartMax());
  EXPECT_TRUE(interval->MayBePerformed());
  EXPECT_FALSE(interval->MustBePerformed());
  safe_start->SetMin(2);
  EXPECT_EQ(10, safe_start->Min());
  EXPECT_EQ(10, interval->StartMin());
  EXPECT_EQ(20, interval->StartMax());
  EXPECT_TRUE(interval->MayBePerformed());
  EXPECT_TRUE(interval->MustBePerformed());
  safe_start->SetMin(12);
  EXPECT_EQ(12, interval->StartMin());
  EXPECT_EQ(20, interval->StartMax());
  return nullptr;
}

TEST_IN_PROPAGATION0(SafeStartTest, DownSetMax, s, true) {
  IntervalVar* const interval =
      s->MakeFixedDurationIntervalVar(10, 20, 5, true, "interval");
  IntExpr* const safe_start = interval->SafeStartExpr(0);
  safe_start->SetMax(22);
  EXPECT_EQ(20, safe_start->Max());
  EXPECT_EQ(10, interval->StartMin());
  EXPECT_EQ(20, interval->StartMax());
  EXPECT_TRUE(interval->MayBePerformed());
  EXPECT_FALSE(interval->MustBePerformed());
  safe_start->SetMax(18);
  EXPECT_EQ(20, safe_start->Max());
  EXPECT_EQ(10, interval->StartMin());
  EXPECT_EQ(20, interval->StartMax());
  EXPECT_TRUE(interval->MayBePerformed());
  EXPECT_FALSE(interval->MustBePerformed());
  safe_start->SetMax(8);
  EXPECT_EQ(0, safe_start->Min());
  EXPECT_EQ(0, safe_start->Max());
  EXPECT_FALSE(interval->MayBePerformed());
  return nullptr;
}

TEST_IN_PROPAGATION0(SafeStartTest, UpGetters, s, true) {
  IntervalVar* const interval =
      s->MakeFixedDurationIntervalVar(10, 20, 5, true, "interval");
  IntExpr* const safe_start = interval->SafeStartExpr(30);
  EXPECT_EQ(10, safe_start->Min());
  EXPECT_EQ(30, safe_start->Max());
  interval->SetStartMin(12);
  EXPECT_EQ(12, safe_start->Min());
  EXPECT_EQ(30, safe_start->Max());
  interval->SetStartMax(18);
  EXPECT_EQ(12, safe_start->Min());
  EXPECT_EQ(30, safe_start->Max());
  interval->SetPerformed(true);
  EXPECT_EQ(12, safe_start->Min());
  EXPECT_EQ(18, safe_start->Max());
  return nullptr;
}

TEST_IN_PROPAGATION0(SafeStartTest, UpSetUnperformed, s, true) {
  IntervalVar* const interval =
      s->MakeFixedDurationIntervalVar(10, 20, 5, true, "interval");
  IntExpr* const safe_start = interval->SafeStartExpr(30);
  interval->SetPerformed(false);
  EXPECT_EQ(30, safe_start->Min());
  EXPECT_EQ(30, safe_start->Max());
  return nullptr;
}

TEST_IN_PROPAGATION0(SafeStartTest, UpSetMin, s, true) {
  IntervalVar* const interval =
      s->MakeFixedDurationIntervalVar(10, 20, 5, true, "interval");
  IntExpr* const safe_start = interval->SafeStartExpr(30);
  safe_start->SetMin(2);
  EXPECT_EQ(10, safe_start->Min());
  EXPECT_EQ(30, safe_start->Max());
  EXPECT_EQ(10, interval->StartMin());
  EXPECT_EQ(20, interval->StartMax());
  EXPECT_TRUE(interval->MayBePerformed());
  EXPECT_FALSE(interval->MustBePerformed());
  safe_start->SetMin(12);
  EXPECT_EQ(10, safe_start->Min());
  EXPECT_EQ(10, interval->StartMin());
  EXPECT_EQ(20, interval->StartMax());
  EXPECT_TRUE(interval->MayBePerformed());
  EXPECT_FALSE(interval->MustBePerformed());
  safe_start->SetMin(22);
  EXPECT_EQ(30, safe_start->Min());
  EXPECT_EQ(30, safe_start->Max());
  EXPECT_FALSE(interval->MayBePerformed());
  return nullptr;
}

TEST_IN_PROPAGATION0(SafeStartTest, UpSetMax, s, true) {
  IntervalVar* const interval =
      s->MakeFixedDurationIntervalVar(10, 20, 5, true, "interval");
  IntExpr* const safe_start = interval->SafeStartExpr(30);
  safe_start->SetMax(32);
  EXPECT_EQ(30, safe_start->Max());
  EXPECT_EQ(10, interval->StartMin());
  EXPECT_EQ(20, interval->StartMax());
  EXPECT_TRUE(interval->MayBePerformed());
  EXPECT_FALSE(interval->MustBePerformed());
  safe_start->SetMax(28);
  EXPECT_EQ(20, safe_start->Max());
  EXPECT_EQ(10, interval->StartMin());
  EXPECT_EQ(20, interval->StartMax());
  EXPECT_TRUE(interval->MustBePerformed());
  safe_start->SetMax(18);
  EXPECT_EQ(10, safe_start->Min());
  EXPECT_EQ(18, safe_start->Max());
  EXPECT_EQ(10, interval->StartMin());
  EXPECT_EQ(18, interval->StartMax());
  EXPECT_TRUE(interval->MustBePerformed());
  return nullptr;
}

class SafeEndTest : public testing::Test {
 protected:
  void SetUp() override { solver_ = std::make_unique<Solver>("CoverTest"); }
  std::unique_ptr<Solver> solver_;
};

TEST_IN_PROPAGATION0(SafeEndTest, DownGetters, s, true) {
  IntervalVar* const interval =
      s->MakeFixedDurationIntervalVar(5, 15, 5, true, "interval");
  IntExpr* const safe_end = interval->SafeEndExpr(0);
  EXPECT_EQ(0, safe_end->Min());
  EXPECT_EQ(20, safe_end->Max());
  interval->SetEndMin(12);
  EXPECT_EQ(0, safe_end->Min());
  EXPECT_EQ(20, safe_end->Max());
  interval->SetEndMax(18);
  EXPECT_EQ(0, safe_end->Min());
  EXPECT_EQ(18, safe_end->Max());
  interval->SetPerformed(true);
  EXPECT_EQ(12, safe_end->Min());
  EXPECT_EQ(18, safe_end->Max());
  return nullptr;
}

TEST_IN_PROPAGATION0(SafeEndTest, DownSetUnperformed, s, true) {
  IntervalVar* const interval =
      s->MakeFixedDurationIntervalVar(5, 15, 5, true, "interval");
  IntExpr* const safe_end = interval->SafeEndExpr(0);
  interval->SetPerformed(false);
  EXPECT_EQ(0, safe_end->Min());
  EXPECT_EQ(0, safe_end->Max());
  return nullptr;
}

TEST_IN_PROPAGATION0(SafeEndTest, DownSetMin, s, true) {
  IntervalVar* const interval =
      s->MakeFixedDurationIntervalVar(5, 15, 5, true, "interval");
  IntExpr* const safe_end = interval->SafeEndExpr(0);
  safe_end->SetMin(-2);
  EXPECT_EQ(0, safe_end->Min());
  EXPECT_EQ(10, interval->EndMin());
  EXPECT_EQ(20, interval->EndMax());
  EXPECT_TRUE(interval->MayBePerformed());
  EXPECT_FALSE(interval->MustBePerformed());
  safe_end->SetMin(2);
  EXPECT_EQ(10, safe_end->Min());
  EXPECT_EQ(10, interval->EndMin());
  EXPECT_EQ(20, interval->EndMax());
  EXPECT_TRUE(interval->MayBePerformed());
  EXPECT_TRUE(interval->MustBePerformed());
  safe_end->SetMin(12);
  EXPECT_EQ(12, interval->EndMin());
  EXPECT_EQ(20, interval->EndMax());
  return nullptr;
}

TEST_IN_PROPAGATION0(SafeEndTest, DownSetMax, s, true) {
  IntervalVar* const interval =
      s->MakeFixedDurationIntervalVar(5, 15, 5, true, "interval");
  IntExpr* const safe_end = interval->SafeEndExpr(0);
  safe_end->SetMax(22);
  EXPECT_EQ(20, safe_end->Max());
  EXPECT_EQ(10, interval->EndMin());
  EXPECT_EQ(20, interval->EndMax());
  EXPECT_TRUE(interval->MayBePerformed());
  EXPECT_FALSE(interval->MustBePerformed());
  safe_end->SetMax(18);
  EXPECT_EQ(20, safe_end->Max());
  EXPECT_EQ(10, interval->EndMin());
  EXPECT_EQ(20, interval->EndMax());
  EXPECT_TRUE(interval->MayBePerformed());
  EXPECT_FALSE(interval->MustBePerformed());
  safe_end->SetMax(8);
  EXPECT_EQ(0, safe_end->Min());
  EXPECT_EQ(0, safe_end->Max());
  EXPECT_FALSE(interval->MayBePerformed());
  return nullptr;
}

TEST_IN_PROPAGATION0(SafeEndTest, UpGetters, s, true) {
  IntervalVar* const interval =
      s->MakeFixedDurationIntervalVar(5, 15, 5, true, "interval");
  IntExpr* const safe_end = interval->SafeEndExpr(30);
  EXPECT_EQ(10, safe_end->Min());
  EXPECT_EQ(30, safe_end->Max());
  interval->SetEndMin(12);
  EXPECT_EQ(12, safe_end->Min());
  EXPECT_EQ(30, safe_end->Max());
  interval->SetEndMax(18);
  EXPECT_EQ(12, safe_end->Min());
  EXPECT_EQ(30, safe_end->Max());
  interval->SetPerformed(true);
  EXPECT_EQ(12, safe_end->Min());
  EXPECT_EQ(18, safe_end->Max());
  return nullptr;
}

TEST_IN_PROPAGATION0(SafeEndTest, UpSetUnperformed, s, true) {
  IntervalVar* const interval =
      s->MakeFixedDurationIntervalVar(5, 15, 5, true, "interval");
  IntExpr* const safe_end = interval->SafeEndExpr(30);
  interval->SetPerformed(false);
  EXPECT_EQ(30, safe_end->Min());
  EXPECT_EQ(30, safe_end->Max());
  return nullptr;
}

TEST_IN_PROPAGATION0(SafeEndTest, UpSetMin, s, true) {
  IntervalVar* const interval =
      s->MakeFixedDurationIntervalVar(5, 15, 5, true, "interval");
  IntExpr* const safe_end = interval->SafeEndExpr(30);
  safe_end->SetMin(2);
  EXPECT_EQ(10, safe_end->Min());
  EXPECT_EQ(30, safe_end->Max());
  EXPECT_EQ(10, interval->EndMin());
  EXPECT_EQ(20, interval->EndMax());
  EXPECT_TRUE(interval->MayBePerformed());
  EXPECT_FALSE(interval->MustBePerformed());
  safe_end->SetMin(12);
  EXPECT_EQ(10, safe_end->Min());
  EXPECT_EQ(10, interval->EndMin());
  EXPECT_EQ(20, interval->EndMax());
  EXPECT_TRUE(interval->MayBePerformed());
  EXPECT_FALSE(interval->MustBePerformed());
  safe_end->SetMin(22);
  EXPECT_EQ(30, safe_end->Min());
  EXPECT_EQ(30, safe_end->Max());
  EXPECT_FALSE(interval->MayBePerformed());
  return nullptr;
}

TEST_IN_PROPAGATION0(SafeEndTest, UpSetMax, s, true) {
  IntervalVar* const interval =
      s->MakeFixedDurationIntervalVar(5, 15, 5, true, "interval");
  IntExpr* const safe_end = interval->SafeEndExpr(30);
  safe_end->SetMax(32);
  EXPECT_EQ(30, safe_end->Max());
  EXPECT_EQ(10, interval->EndMin());
  EXPECT_EQ(20, interval->EndMax());
  EXPECT_TRUE(interval->MayBePerformed());
  EXPECT_FALSE(interval->MustBePerformed());
  safe_end->SetMax(28);
  EXPECT_EQ(20, safe_end->Max());
  EXPECT_EQ(10, interval->EndMin());
  EXPECT_EQ(20, interval->EndMax());
  EXPECT_TRUE(interval->MustBePerformed());
  safe_end->SetMax(18);
  EXPECT_EQ(10, safe_end->Min());
  EXPECT_EQ(18, safe_end->Max());
  EXPECT_EQ(10, interval->EndMin());
  EXPECT_EQ(18, interval->EndMax());
  EXPECT_TRUE(interval->MustBePerformed());
  return nullptr;
}

}  // namespace
}  // namespace operations_research
