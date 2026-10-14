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

#include "ortools/constraint_solver/interval.h"

#include <stddef.h>

#include <cstdint>
#include <memory>
#include <string>

#include "gtest/gtest.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solver_test_util.h"

namespace operations_research {
namespace {
class FixedDurationIntervalTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("FixedDurationIntervalTest");
  }
  std::unique_ptr<Solver> solver_;
};

TEST_F(FixedDurationIntervalTest, DebugString) {
  IntervalVar* i = solver_->MakeFixedDurationIntervalVar(0, 10, 3, false, "i");
  EXPECT_FALSE(i->DebugString().empty());
}

TEST_F(FixedDurationIntervalTest, PerformedIntervalVarTestAPI) {
  IntervalVar* i = solver_->MakeFixedDurationIntervalVar(0, 10, 3, false, "i");
  IntervalVar* j = solver_->MakeFixedDurationIntervalVar(0, 10, 3, true, "j");
  EXPECT_EQ(0, i->StartMin());
  EXPECT_EQ(10, i->StartMax());
  EXPECT_EQ(3, i->DurationMin());
  EXPECT_EQ(3, i->DurationMax());
  EXPECT_TRUE(i->MustBePerformed());
  EXPECT_TRUE(i->MayBePerformed());
  EXPECT_TRUE(i->IsPerformedBound());
  EXPECT_FALSE(j->MustBePerformed());
  EXPECT_TRUE(j->MayBePerformed());
  EXPECT_FALSE(j->IsPerformedBound());
}

TEST_F(FixedDurationIntervalTest, SetPerformed) {
  IntervalVar* j = solver_->MakeFixedDurationIntervalVar(0, 10, 3, true, "j");
  j->SetPerformed(true);
  EXPECT_TRUE(j->MustBePerformed());
  EXPECT_TRUE(j->MayBePerformed());
  EXPECT_TRUE(j->IsPerformedBound());
}

TEST_F(FixedDurationIntervalTest, SetUnperformed) {
  IntervalVar* j = solver_->MakeFixedDurationIntervalVar(0, 10, 3, true, "j");
  j->SetPerformed(false);
  EXPECT_FALSE(j->MustBePerformed());
  EXPECT_FALSE(j->MayBePerformed());
  EXPECT_TRUE(j->IsPerformedBound());
}

TEST_IN_PROPAGATION0(FixedDurationIntervalTest, IntervalVarSetters, solver,
                     true) {
  IntervalVar* i = solver->MakeFixedDurationIntervalVar(0, 10, 3, true, "i");
  i->SetStartMin(2);
  EXPECT_EQ(2, i->StartMin());
  i->SetStartMax(8);
  EXPECT_EQ(8, i->StartMax());
  i->SetDurationMin(2);
  EXPECT_EQ(3, i->DurationMin());
  i->SetDurationMax(5);
  EXPECT_EQ(3, i->DurationMax());
  EXPECT_FALSE(i->MustBePerformed());
  EXPECT_TRUE(i->MayBePerformed());
  return nullptr;
};

TEST_IN_PROPAGATION0(FixedDurationIntervalTest, IntervalVarSetFalseStartMin,
                     solver, true) {
  IntervalVar* i = solver->MakeFixedDurationIntervalVar(0, 10, 3, true, "i");
  i->SetStartMin(11);
  EXPECT_FALSE(i->MayBePerformed());
  return nullptr;
};

TEST_IN_PROPAGATION0(FixedDurationIntervalTest, IntervalVarSetFalseStartMax,
                     solver, true) {
  IntervalVar* i = solver->MakeFixedDurationIntervalVar(0, 10, 3, true, "i");
  i->SetStartMax(-1);
  EXPECT_FALSE(i->MayBePerformed());
  return nullptr;
};

TEST_IN_PROPAGATION0(FixedDurationIntervalTest, IntervalVarSetFalseDurationMin,
                     solver, true) {
  IntervalVar* i = solver->MakeFixedDurationIntervalVar(0, 10, 3, true, "i");
  i->SetDurationMin(11);
  EXPECT_FALSE(i->MayBePerformed());
  return nullptr;
};

TEST_IN_PROPAGATION0(FixedDurationIntervalTest, IntervalVarSetFalseDurationMax,
                     solver, true) {
  IntervalVar* i = solver->MakeFixedDurationIntervalVar(0, 10, 3, true, "i");
  i->SetDurationMax(1);
  EXPECT_FALSE(i->MayBePerformed());
  return nullptr;
};

TEST_IN_PROPAGATION0(FixedDurationIntervalTest, RelaxedMaxIntervalVar, solver_,
                     false) {
  const int64_t kStartMin = 5;
  const int64_t kStartMax = 9;
  const int64_t kDuration = 2;
  IntervalVar* const underlying = solver_->MakeFixedDurationIntervalVar(
      kStartMin, kStartMax, kDuration, true, "underlying");
  IntervalVar* const relaxed = solver_->MakeIntervalRelaxedMax(underlying);
  EXPECT_EQ(kStartMin, relaxed->StartMin());
  EXPECT_EQ(kStartMin + kDuration, relaxed->EndMin());
  EXPECT_EQ(IntervalVar::kMaxValidValue - kDuration, relaxed->StartMax());
  EXPECT_EQ(IntervalVar::kMaxValidValue, relaxed->EndMax());
  EXPECT_TRUE(relaxed->IsPerformedBound());
  EXPECT_TRUE(relaxed->MayBePerformed());
  EXPECT_TRUE(relaxed->MustBePerformed());
  // Now make the solver fail
  relaxed->SetPerformed(false);
  return nullptr;
}

TEST_IN_PROPAGATION0(FixedDurationIntervalTest, RelaxedMinIntervalVar, solver_,
                     false) {
  const int64_t kStartMin = 5;
  const int64_t kStartMax = 9;
  const int64_t kDuration = 2;
  IntervalVar* const underlying = solver_->MakeFixedDurationIntervalVar(
      kStartMin, kStartMax, kDuration, true, "underlying");
  IntervalVar* const relaxed = solver_->MakeIntervalRelaxedMin(underlying);
  EXPECT_EQ(IntervalVar::kMinValidValue, relaxed->StartMin());
  EXPECT_EQ(IntervalVar::kMinValidValue + kDuration, relaxed->EndMin());
  EXPECT_EQ(kStartMax, relaxed->StartMax());
  EXPECT_EQ(kStartMax + kDuration, relaxed->EndMax());
  EXPECT_TRUE(relaxed->IsPerformedBound());
  EXPECT_TRUE(relaxed->MayBePerformed());
  EXPECT_TRUE(relaxed->MustBePerformed());
  // Now make the solver fail
  relaxed->SetPerformed(false);
  return nullptr;
}

// Non fixed duration interval test

class VariableDurationIntervalTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("VariableDurationIntervalTest");
  }
  std::unique_ptr<Solver> solver_;
};

TEST_F(VariableDurationIntervalTest, DebugString) {
  IntervalVar* i = solver_->MakeIntervalVar(0, 10, 2, 3, 2, 13, false, "i");
  EXPECT_FALSE(i->DebugString().empty());
}

TEST_F(VariableDurationIntervalTest, Getters) {
  IntervalVar* const i =
      solver_->MakeIntervalVar(0, 10, 2, 3, 2, 13, false, "i");
  IntervalVar* const j =
      solver_->MakeIntervalVar(0, 10, 2, 3, 2, 13, true, "j");
  EXPECT_EQ(0, i->StartMin());
  EXPECT_EQ(10, i->StartMax());
  EXPECT_EQ(2, i->DurationMin());
  EXPECT_EQ(3, i->DurationMax());
  EXPECT_EQ(2, i->EndMin());
  EXPECT_EQ(13, i->EndMax());
  EXPECT_TRUE(i->MustBePerformed());
  EXPECT_TRUE(i->MayBePerformed());
  EXPECT_TRUE(i->IsPerformedBound());
  EXPECT_FALSE(j->MustBePerformed());
  EXPECT_TRUE(j->MayBePerformed());
  EXPECT_FALSE(j->IsPerformedBound());
}

TEST_F(VariableDurationIntervalTest, ConsistencyInCtor) {
  // Propagation on min.
  IntervalVar* const i = solver_->MakeIntervalVar(0, 10, 2, 4, 5, 9, true, "i");
  EXPECT_EQ(1, i->StartMin());
  EXPECT_EQ(7, i->StartMax());
  EXPECT_EQ(2, i->DurationMin());
  EXPECT_EQ(4, i->DurationMax());
  EXPECT_EQ(5, i->EndMin());
  EXPECT_EQ(9, i->EndMax());

  // Propagation on max.
  IntervalVar* const j = solver_->MakeIntervalVar(0, 3, 2, 4, 0, 10, true, "i");
  EXPECT_EQ(0, j->StartMin());
  EXPECT_EQ(3, j->StartMax());
  EXPECT_EQ(2, j->DurationMin());
  EXPECT_EQ(4, j->DurationMax());
  EXPECT_EQ(2, j->EndMin());
  EXPECT_EQ(7, j->EndMax());

  // Propagation on duration.
  IntervalVar* const k = solver_->MakeIntervalVar(0, 3, 2, 12, 8, 9, true, "i");
  EXPECT_EQ(0, k->StartMin());
  EXPECT_EQ(3, k->StartMax());
  EXPECT_EQ(5, k->DurationMin());
  EXPECT_EQ(9, k->DurationMax());
  EXPECT_EQ(8, k->EndMin());
  EXPECT_EQ(9, k->EndMax());
}

TEST_IN_PROPAGATION0(VariableDurationIntervalTest, SetStartMin, s, true) {
  IntervalVar* const i = s->MakeIntervalVar(0, 10, 2, 3, 2, 13, true, "i");
  i->SetStartMin(4);
  EXPECT_EQ(4, i->StartMin());
  EXPECT_EQ(10, i->StartMax());
  EXPECT_EQ(2, i->DurationMin());
  EXPECT_EQ(3, i->DurationMax());
  EXPECT_EQ(6, i->EndMin());
  EXPECT_EQ(13, i->EndMax());
  return nullptr;
}

TEST_IN_PROPAGATION0(VariableDurationIntervalTest, SetStartMax, s, true) {
  IntervalVar* const i = s->MakeIntervalVar(0, 10, 2, 3, 2, 13, true, "i");
  i->SetStartMax(7);
  EXPECT_EQ(0, i->StartMin());
  EXPECT_EQ(7, i->StartMax());
  EXPECT_EQ(2, i->DurationMin());
  EXPECT_EQ(3, i->DurationMax());
  EXPECT_EQ(2, i->EndMin());
  EXPECT_EQ(10, i->EndMax());
  return nullptr;
}

TEST_IN_PROPAGATION0(VariableDurationIntervalTest, SetDurationMin, s, true) {
  IntervalVar* const i = s->MakeIntervalVar(0, 10, 2, 3, 2, 13, true, "i");
  i->SetDurationMin(3);
  EXPECT_EQ(0, i->StartMin());
  EXPECT_EQ(10, i->StartMax());
  EXPECT_EQ(3, i->DurationMin());
  EXPECT_EQ(3, i->DurationMax());
  EXPECT_EQ(3, i->EndMin());
  EXPECT_EQ(13, i->EndMax());
  return nullptr;
}

TEST_IN_PROPAGATION0(VariableDurationIntervalTest, SetDurationMax, s, true) {
  IntervalVar* const i = s->MakeIntervalVar(0, 10, 2, 3, 2, 13, true, "i");
  i->SetDurationMax(2);
  EXPECT_EQ(0, i->StartMin());
  EXPECT_EQ(10, i->StartMax());
  EXPECT_EQ(2, i->DurationMin());
  EXPECT_EQ(2, i->DurationMax());
  EXPECT_EQ(2, i->EndMin());
  EXPECT_EQ(12, i->EndMax());
  return nullptr;
}

TEST_IN_PROPAGATION0(VariableDurationIntervalTest, SetEndMin, s, true) {
  IntervalVar* const i = s->MakeIntervalVar(0, 10, 2, 3, 2, 13, true, "i");
  i->SetEndMin(6);
  EXPECT_EQ(3, i->StartMin());
  EXPECT_EQ(10, i->StartMax());
  EXPECT_EQ(2, i->DurationMin());
  EXPECT_EQ(3, i->DurationMax());
  EXPECT_EQ(6, i->EndMin());
  EXPECT_EQ(13, i->EndMax());
  return nullptr;
}

TEST_IN_PROPAGATION0(VariableDurationIntervalTest, SetEndMax, s, true) {
  IntervalVar* const i = s->MakeIntervalVar(0, 10, 2, 3, 2, 13, true, "i");
  i->SetEndMax(9);
  EXPECT_EQ(0, i->StartMin());
  EXPECT_EQ(7, i->StartMax());
  EXPECT_EQ(2, i->DurationMin());
  EXPECT_EQ(3, i->DurationMax());
  EXPECT_EQ(2, i->EndMin());
  EXPECT_EQ(9, i->EndMax());
  return nullptr;
}

TEST_IN_PROPAGATION0(VariableDurationIntervalTest, SetStartMinFalse, s, true) {
  IntervalVar* const i = s->MakeIntervalVar(0, 10, 2, 3, 2, 13, true, "i");
  i->SetStartMin(11);
  EXPECT_FALSE(i->MayBePerformed());
  return nullptr;
}

TEST_IN_PROPAGATION0(VariableDurationIntervalTest, SetStartMaxFalse, s, true) {
  IntervalVar* const i = s->MakeIntervalVar(0, 10, 2, 3, 2, 13, true, "i");
  i->SetStartMax(-1);
  EXPECT_FALSE(i->MayBePerformed());
  return nullptr;
}

TEST_IN_PROPAGATION0(VariableDurationIntervalTest, SetDurationMinFalse, s,
                     true) {
  IntervalVar* const i = s->MakeIntervalVar(0, 10, 2, 3, 2, 13, true, "i");
  i->SetDurationMin(4);
  EXPECT_FALSE(i->MayBePerformed());
  return nullptr;
}

TEST_IN_PROPAGATION0(VariableDurationIntervalTest, SetDurationMaxFalse, s,
                     true) {
  IntervalVar* const i = s->MakeIntervalVar(0, 10, 2, 3, 2, 13, true, "i");
  i->SetDurationMax(1);
  EXPECT_FALSE(i->MayBePerformed());
  return nullptr;
}

TEST_IN_PROPAGATION0(VariableDurationIntervalTest, SetEndMinFalse, s, true) {
  IntervalVar* const i = s->MakeIntervalVar(0, 10, 2, 3, 2, 13, true, "i");
  i->SetEndMin(14);
  EXPECT_FALSE(i->MayBePerformed());
  return nullptr;
}

TEST_IN_PROPAGATION0(VariableDurationIntervalTest, SetEndMaxFalse, s, true) {
  IntervalVar* const i = s->MakeIntervalVar(0, 10, 2, 3, 2, 13, true, "i");
  i->SetEndMax(1);
  EXPECT_FALSE(i->MayBePerformed());
  return nullptr;
}

// Synced interval test

class SyncedIntervalTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("SyncedIntervalTest");
  }
  std::unique_ptr<Solver> solver_;
};

TEST_F(SyncedIntervalTest, DebugString) {
  const int kDuration = 3;
  const int kDelay = -2;
  IntervalVar* const base =
      solver_->MakeFixedDurationIntervalVar(0, 10, 2, false, "i");
  IntervalVar* const start =
      solver_->MakeFixedDurationStartSyncedOnStartIntervalVar(base, kDuration,
                                                              kDelay);
  IntervalVar* const end =
      solver_->MakeFixedDurationStartSyncedOnEndIntervalVar(base, kDuration,
                                                            kDelay);
  EXPECT_FALSE(start->DebugString().empty());
  EXPECT_FALSE(end->DebugString().empty());
}

TEST_F(SyncedIntervalTest, Getters) {
  const int kDuration = 5;
  const int kDelay = -2;
  const int kStartMin = 0;
  const int kStartMax = 10;
  const int kSyncedDuration = 3;
  IntervalVar* const base = solver_->MakeFixedDurationIntervalVar(
      kStartMin, kStartMax, kDuration, false, "i");
  IntervalVar* const start =
      solver_->MakeFixedDurationStartSyncedOnStartIntervalVar(
          base, kSyncedDuration, kDelay);
  IntervalVar* const end =
      solver_->MakeFixedDurationStartSyncedOnEndIntervalVar(
          base, kSyncedDuration, kDelay);
  IntervalVar* const start2 =
      solver_->MakeFixedDurationEndSyncedOnStartIntervalVar(
          base, kSyncedDuration, kDelay);
  IntervalVar* const end2 = solver_->MakeFixedDurationEndSyncedOnEndIntervalVar(
      base, kSyncedDuration, kDelay);
  EXPECT_EQ(kStartMin + kDelay, start->StartMin());
  EXPECT_EQ(kStartMax + kDelay, start->StartMax());
  EXPECT_EQ(kSyncedDuration, start->DurationMin());
  EXPECT_EQ(kSyncedDuration, start->DurationMax());
  EXPECT_EQ(kStartMin + kSyncedDuration + kDelay, start->EndMin());
  EXPECT_EQ(kStartMax + kSyncedDuration + kDelay, start->EndMax());
  EXPECT_TRUE(start->MustBePerformed());
  EXPECT_TRUE(start->MayBePerformed());
  EXPECT_TRUE(start->IsPerformedBound());

  EXPECT_EQ(kStartMin + kDuration + kDelay, end->StartMin());
  EXPECT_EQ(kStartMax + kDuration + kDelay, end->StartMax());
  EXPECT_EQ(kSyncedDuration, end->DurationMin());
  EXPECT_EQ(kSyncedDuration, end->DurationMax());
  EXPECT_EQ(kStartMin + kSyncedDuration + kDuration + kDelay, end->EndMin());
  EXPECT_EQ(kStartMax + kSyncedDuration + kDuration + kDelay, end->EndMax());
  EXPECT_TRUE(end->MustBePerformed());
  EXPECT_TRUE(end->MayBePerformed());
  EXPECT_TRUE(end->IsPerformedBound());

  EXPECT_EQ(kStartMin + kDelay - kSyncedDuration, start2->StartMin());
  EXPECT_EQ(kStartMax + kDelay - kSyncedDuration, start2->StartMax());
  EXPECT_EQ(kSyncedDuration, start2->DurationMin());
  EXPECT_EQ(kSyncedDuration, start2->DurationMax());
  EXPECT_EQ(kStartMin + kDelay, start2->EndMin());
  EXPECT_EQ(kStartMax + kDelay, start2->EndMax());
  EXPECT_TRUE(start2->MustBePerformed());
  EXPECT_TRUE(start2->MayBePerformed());
  EXPECT_TRUE(start2->IsPerformedBound());

  EXPECT_EQ(kStartMin + kDuration + kDelay - kSyncedDuration, end2->StartMin());
  EXPECT_EQ(kStartMax + kDuration + kDelay - kSyncedDuration, end2->StartMax());
  EXPECT_EQ(kSyncedDuration, end2->DurationMin());
  EXPECT_EQ(kSyncedDuration, end2->DurationMax());
  EXPECT_EQ(kStartMin + kDuration + kDelay, end2->EndMin());
  EXPECT_EQ(kStartMax + kDuration + kDelay, end2->EndMax());
  EXPECT_TRUE(end2->MustBePerformed());
  EXPECT_TRUE(end2->MayBePerformed());
  EXPECT_TRUE(end2->IsPerformedBound());
}

TEST_F(SyncedIntervalTest, StartSettersOnStart) {
  const int kDuration = 5;
  const int kDelay = -2;
  const int kStartMin = 0;
  const int kStartMax = 10;
  const int kSyncedDuration = 3;
  const int kDelta = 1;
  IntervalVar* const base = solver_->MakeFixedDurationIntervalVar(
      kStartMin, kStartMax, kDuration, true, "i");
  IntervalVar* const start =
      solver_->MakeFixedDurationStartSyncedOnStartIntervalVar(
          base, kSyncedDuration, kDelay);
  start->SetStartMin(kStartMin + kDelay + kDelta);
  EXPECT_EQ(kStartMin + kDelta, base->StartMin());
  start->SetStartMax(kStartMax + kDelay - kDelta);
  EXPECT_EQ(kStartMax - kDelta, base->StartMax());
  start->SetStartRange(kStartMin + kDelay + kDelta * 2,
                       kStartMax + kDelay - kDelta * 2);
  EXPECT_EQ(kStartMin + kDelta * 2, base->StartMin());
  EXPECT_EQ(kStartMax - kDelta * 2, base->StartMax());
  start->SetPerformed(false);
  EXPECT_FALSE(base->MayBePerformed());
}

TEST_F(SyncedIntervalTest, EndSettersOnStart) {
  const int kDuration = 5;
  const int kDelay = -2;
  const int kStartMin = 0;
  const int kStartMax = 10;
  const int kSyncedDuration = 3;
  const int kDelta = 1;
  IntervalVar* const base = solver_->MakeFixedDurationIntervalVar(
      kStartMin, kStartMax, kDuration, true, "i");
  IntervalVar* const start =
      solver_->MakeFixedDurationStartSyncedOnStartIntervalVar(
          base, kSyncedDuration, kDelay);
  start->SetEndMin(kStartMin + kDelay + kDelta + kSyncedDuration);
  EXPECT_EQ(kStartMin + kDelta, base->StartMin());
  start->SetEndMax(kStartMax + kDelay - kDelta + kSyncedDuration);
  EXPECT_EQ(kStartMax - kDelta, base->StartMax());
  start->SetEndRange(kStartMin + kDelay + kDelta * 2 + kSyncedDuration,
                     kStartMax + kDelay - kDelta * 2 + kSyncedDuration);
  EXPECT_EQ(kStartMin + kDelta * 2, base->StartMin());
  EXPECT_EQ(kStartMax - kDelta * 2, base->StartMax());
  start->SetPerformed(true);
  EXPECT_TRUE(base->MustBePerformed());
}

TEST_F(SyncedIntervalTest, StartSettersOnEnd) {
  const int kDuration = 5;
  const int kDelay = -2;
  const int kStartMin = 0;
  const int kStartMax = 10;
  const int kSyncedDuration = 3;
  const int kDelta = 1;
  IntervalVar* const base = solver_->MakeFixedDurationIntervalVar(
      kStartMin, kStartMax, kDuration, true, "i");
  IntervalVar* const end =
      solver_->MakeFixedDurationStartSyncedOnEndIntervalVar(
          base, kSyncedDuration, kDelay);
  end->SetStartMin(kStartMin + kDuration + kDelay + kDelta);
  EXPECT_EQ(kStartMin + kDelta, base->StartMin());
  end->SetStartMax(kStartMax + kDuration + kDelay - kDelta);
  EXPECT_EQ(kStartMax - kDelta, base->StartMax());
  end->SetStartRange(kStartMin + kDuration + kDelay + kDelta * 2,
                     kStartMax + kDuration + kDelay - kDelta * 2);
  EXPECT_EQ(kStartMin + kDelta * 2, base->StartMin());
  EXPECT_EQ(kStartMax - kDelta * 2, base->StartMax());
  end->SetPerformed(false);
  EXPECT_FALSE(base->MayBePerformed());
}

TEST_F(SyncedIntervalTest, EndSettersOnEnd) {
  const int kDuration = 5;
  const int kDelay = -2;
  const int kStartMin = 0;
  const int kStartMax = 10;
  const int kSyncedDuration = 3;
  const int kDelta = 1;
  IntervalVar* const base = solver_->MakeFixedDurationIntervalVar(
      kStartMin, kStartMax, kDuration, true, "i");
  IntervalVar* const end =
      solver_->MakeFixedDurationStartSyncedOnEndIntervalVar(
          base, kSyncedDuration, kDelay);
  end->SetEndMin(kStartMin + kDuration + kDelay + kDelta + kSyncedDuration);
  EXPECT_EQ(kStartMin + kDelta, base->StartMin());
  end->SetEndMax(kStartMax + kDuration + kDelay - kDelta + kSyncedDuration);
  EXPECT_EQ(kStartMax - kDelta, base->StartMax());
  end->SetEndRange(
      kStartMin + kDuration + kDelay + kDelta * 2 + kSyncedDuration,
      kStartMax + kDuration + kDelay - kDelta * 2 + kSyncedDuration);
  EXPECT_EQ(kStartMin + kDelta * 2, base->StartMin());
  EXPECT_EQ(kStartMax - kDelta * 2, base->StartMax());
  end->SetPerformed(true);
  EXPECT_TRUE(base->MustBePerformed());
}

// Test StartVarIntervalVar (non performed version)

TEST(StartVarIntervalVarTest, Ctor) {
  Solver solver("StartVarIntervalVarTest.Ctor");
  IntVar* const start = solver.MakeIntVar(0, 10, "start");
  IntVar* const performed = solver.MakeBoolVar("performed");
  const int kDuration = 5;
  IntervalVar* const interval = solver.MakeFixedDurationIntervalVar(
      start, kDuration, performed, "interval");
  EXPECT_EQ(0, interval->StartMin());
  EXPECT_EQ(10, interval->StartMax());
  EXPECT_EQ(5, interval->EndMin());
  EXPECT_EQ(15, interval->EndMax());
  EXPECT_EQ(5, interval->DurationMin());
  EXPECT_EQ(5, interval->DurationMax());
  EXPECT_TRUE(interval->MayBePerformed());
  EXPECT_FALSE(interval->MustBePerformed());
}

TEST_IN_PROPAGATION(StartVarIntervalVarTest, SetStartRange1, solver, true) {
  IntVar* const start = solver->MakeIntVar(0, 10, "start");
  IntVar* const performed = solver->MakeBoolVar("performed");
  const int kDuration = 5;
  IntervalVar* const interval = solver->MakeFixedDurationIntervalVar(
      start, kDuration, performed, "interval");
  start->SetMin(2);
  EXPECT_EQ(2, interval->StartMin());
  EXPECT_EQ(10, interval->StartMax());
  start->SetMax(8);
  EXPECT_EQ(7, interval->EndMin());
  EXPECT_EQ(13, interval->EndMax());

  // No propagation if not performed.
  interval->SetStartRange(3, 7);
  EXPECT_EQ(2, start->Min());
  EXPECT_EQ(8, start->Max());
  EXPECT_EQ(3, interval->StartMin());
  EXPECT_EQ(7, interval->StartMax());

  // Force propagation by setting the performed var.
  performed->SetValue(1);
  EXPECT_EQ(3, start->Min());
  EXPECT_EQ(7, start->Max());
  EXPECT_EQ(3, interval->StartMin());
  EXPECT_EQ(7, interval->StartMax());

  return nullptr;
}

TEST_IN_PROPAGATION(StartVarIntervalVarTest, SetStartRange2, solver, true) {
  IntVar* const start = solver->MakeIntVar(0, 10, "start");
  IntVar* const performed = solver->MakeBoolVar("performed");
  const int kDuration = 5;
  IntervalVar* const interval = solver->MakeFixedDurationIntervalVar(
      start, kDuration, performed, "interval");
  start->SetMin(2);
  EXPECT_EQ(2, interval->StartMin());
  EXPECT_EQ(10, interval->StartMax());
  start->SetMax(8);
  EXPECT_EQ(7, interval->EndMin());
  EXPECT_EQ(13, interval->EndMax());

  // No propagation if not performed.
  interval->SetStartRange(3, 7);
  EXPECT_EQ(2, start->Min());
  EXPECT_EQ(8, start->Max());
  EXPECT_EQ(3, interval->StartMin());
  EXPECT_EQ(7, interval->StartMax());

  // Force propagation by setting the performed status on the interval.
  interval->SetPerformed(true);
  EXPECT_EQ(3, start->Min());
  EXPECT_EQ(7, start->Max());
  EXPECT_EQ(3, interval->StartMin());
  EXPECT_EQ(7, interval->StartMax());

  return nullptr;
}

TEST_IN_PROPAGATION(StartVarIntervalVarTest, SetStartRange3, solver, true) {
  IntVar* const start = solver->MakeIntVar(0, 10, "start");
  IntVar* const performed = solver->MakeBoolVar("performed");
  const int kDuration = 5;
  IntervalVar* const interval = solver->MakeFixedDurationIntervalVar(
      start, kDuration, performed, "interval");
  start->SetMin(2);
  EXPECT_EQ(2, interval->StartMin());
  EXPECT_EQ(10, interval->StartMax());
  start->SetMax(8);
  EXPECT_EQ(7, interval->EndMin());
  EXPECT_EQ(13, interval->EndMax());

  interval->SetPerformed(true);
  interval->SetStartRange(3, 7);
  EXPECT_EQ(3, start->Min());
  EXPECT_EQ(7, start->Max());
  EXPECT_EQ(3, interval->StartMin());
  EXPECT_EQ(7, interval->StartMax());

  return nullptr;
}

TEST_IN_PROPAGATION(StartVarIntervalVarTest, SetStartRange4, solver, true) {
  IntVar* const start = solver->MakeIntVar(0, 10, "start");
  IntVar* const performed = solver->MakeBoolVar("performed");
  const int kDuration = 5;
  IntervalVar* const interval = solver->MakeFixedDurationIntervalVar(
      start, kDuration, performed, "interval");
  start->SetMin(2);
  EXPECT_EQ(2, interval->StartMin());
  EXPECT_EQ(10, interval->StartMax());
  start->SetMax(8);
  EXPECT_EQ(7, interval->EndMin());
  EXPECT_EQ(13, interval->EndMax());

  performed->SetValue(1);
  interval->SetStartRange(3, 7);
  EXPECT_EQ(3, start->Min());
  EXPECT_EQ(7, start->Max());
  EXPECT_EQ(3, interval->StartMin());
  EXPECT_EQ(7, interval->StartMax());

  return nullptr;
}

#define DEF_ALL                                                       \
  IntVar* const start = solver->MakeIntVar(0, 10, "start");           \
  IntVar* const performed = solver->MakeBoolVar("performed");         \
  const int kDuration = 5;                                            \
  IntervalVar* const interval = solver->MakeFixedDurationIntervalVar( \
      start, kDuration, performed, "interval")

TEST_IN_PROPAGATION(StartVarIntervalVarTest, SetStartMinFail, solver, true) {
  DEF_ALL;
  interval->SetStartMin(12);
  EXPECT_FALSE(interval->MayBePerformed());
  EXPECT_EQ(0, performed->Max());
  return nullptr;
}

TEST_IN_PROPAGATION(StartVarIntervalVarTest, SetStartMaxFail, solver, true) {
  DEF_ALL;
  interval->SetStartMax(-1);
  EXPECT_FALSE(interval->MayBePerformed());
  EXPECT_EQ(0, performed->Max());
  return nullptr;
}

TEST_IN_PROPAGATION(StartVarIntervalVarTest, SetDurationMinFail, solver, true) {
  DEF_ALL;
  interval->SetDurationMin(6);
  EXPECT_FALSE(interval->MayBePerformed());
  EXPECT_EQ(0, performed->Max());
  return nullptr;
}

TEST_IN_PROPAGATION(StartVarIntervalVarTest, SetDurationMaxFail, solver, true) {
  DEF_ALL;
  interval->SetDurationMax(4);
  EXPECT_FALSE(interval->MayBePerformed());
  EXPECT_EQ(0, performed->Max());
  return nullptr;
}

TEST_IN_PROPAGATION(StartVarIntervalVarTest, SetEndMinFail, solver, true) {
  DEF_ALL;
  interval->SetEndMin(16);
  EXPECT_FALSE(interval->MayBePerformed());
  EXPECT_EQ(0, performed->Max());
  return nullptr;
}
#undef DEF_ALL

TEST_IN_PROPAGATION(StartVarIntervalVarTest, SetEndMaxFail, solver, true) {
  IntVar* const start = solver->MakeIntVar(0, 10, "start");
  IntVar* const performed = solver->MakeBoolVar("performed");
  const int kDuration = 5;
  IntervalVar* const interval = solver->MakeFixedDurationIntervalVar(
      start, kDuration, performed, "interval");
  interval->SetEndMax(4);
  EXPECT_FALSE(interval->MayBePerformed());
  EXPECT_EQ(0, performed->Max());
  return nullptr;
}

TEST_IN_PROPAGATION(IntervalTest, Overflow, solver, true) {
  IntervalVar* const interval = solver->MakeIntervalVar(
      0, kint64max, 0, kint64max, 0, kint64max, true, "interval");
  interval->SetPerformed(true);
  EXPECT_EQ(kint64max, interval->EndExpr()->Max());
  return nullptr;
}
}  // namespace
}  // namespace operations_research
