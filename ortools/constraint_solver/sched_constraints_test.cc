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
#include <vector>

#include "gtest/gtest.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solver_test_util.h"
#include "ortools/constraint_solver/interval.h"

namespace operations_research {
namespace {
class CoverTest : public testing::Test {
 protected:
  void SetUp() override { solver_ = std::make_unique<Solver>("CoverTest"); }
  std::unique_ptr<Solver> solver_;
};

TEST_F(CoverTest, DebugString) {
  const int kSize = 1000;
  IntervalVar* const target =
      solver_->MakeIntervalVar(0, 10, 2, 5, 2, 15, true, "target");
  std::vector<IntervalVar*> intervals;
  solver_->MakeFixedDurationIntervalVarArray(kSize, 0, 10, 2, true, "intervals",
                                             &intervals);
  Constraint* const cover = solver_->MakeCover(intervals, target);
  EXPECT_FALSE(cover->DebugString().empty());
}

TEST_IN_PROPAGATION0(CoverTest, InitialPropagateBoundsUp, s, true) {
  const int kSize = 1000;
  IntervalVar* const target =
      s->MakeIntervalVar(0, 10, 2, 5, 0, 20, false, "target");
  std::vector<IntervalVar*> intervals;
  s->MakeFixedDurationIntervalVarArray(kSize, 2, 10, 2, false, "intervals",
                                       &intervals);
  s->AddConstraint(s->MakeCover(intervals, target));
  EXPECT_EQ(2, target->StartMin());
  EXPECT_EQ(12, target->EndMax());
  return nullptr;
}

TEST_IN_PROPAGATION0(CoverTest, InitialPropagatePerformedUp, s, true) {
  const int kSize = 1000;
  IntervalVar* const target =
      s->MakeIntervalVar(0, 10, 2, 5, 0, 20, true, "target");
  std::vector<IntervalVar*> intervals;
  s->MakeFixedDurationIntervalVarArray(kSize, 2, 10, 2, true, "intervals",
                                       &intervals);
  intervals[0]->SetPerformed(true);
  s->AddConstraint(s->MakeCover(intervals, target));
  EXPECT_TRUE(target->MustBePerformed());
  return nullptr;
}

TEST_IN_PROPAGATION0(CoverTest, PropagatePerformedUp, s, true) {
  const int kSize = 1000;
  IntervalVar* const target =
      s->MakeIntervalVar(0, 10, 2, 5, 0, 20, true, "target");
  std::vector<IntervalVar*> intervals;
  s->MakeFixedDurationIntervalVarArray(kSize, 2, 10, 2, true, "intervals",
                                       &intervals);
  s->AddConstraint(s->MakeCover(intervals, target));
  EXPECT_FALSE(target->MustBePerformed());
  EXPECT_TRUE(target->MayBePerformed());
  intervals[0]->SetPerformed(true);
  EXPECT_TRUE(target->MustBePerformed());
  return nullptr;
}

TEST_IN_PROPAGATION0(CoverTest, InitialPropagateUnperformedUp, s, true) {
  const int kSize = 1000;
  IntervalVar* const target =
      s->MakeIntervalVar(0, 10, 2, 5, 0, 20, true, "target");
  std::vector<IntervalVar*> intervals;
  s->MakeFixedDurationIntervalVarArray(kSize, 2, 10, 2, true, "intervals",
                                       &intervals);
  for (int i = 0; i < kSize; ++i) {
    intervals[i]->SetPerformed(false);
  }
  s->AddConstraint(s->MakeCover(intervals, target));
  EXPECT_FALSE(target->MayBePerformed());
  return nullptr;
}

TEST_IN_PROPAGATION0(CoverTest, PropagateUnperformedUp, s, true) {
  const int kSize = 1000;
  IntervalVar* const target =
      s->MakeIntervalVar(0, 10, 2, 5, 0, 20, true, "target");
  std::vector<IntervalVar*> intervals;
  s->MakeFixedDurationIntervalVarArray(kSize, 2, 10, 2, true, "intervals",
                                       &intervals);
  s->AddConstraint(s->MakeCover(intervals, target));
  for (int i = 0; i < kSize; ++i) {
    intervals[i]->SetPerformed(false);
    if (i != kSize - 1) {
      EXPECT_TRUE(target->MayBePerformed());
    } else {
      EXPECT_FALSE(target->MayBePerformed());
    }
  }
  return nullptr;
}

TEST_IN_PROPAGATION0(CoverTest, InitialPropagateBoundsDown, s, true) {
  const int kSize = 1000;
  IntervalVar* const target =
      s->MakeIntervalVar(2, 10, 2, 5, 4, 15, false, "target");
  std::vector<IntervalVar*> intervals;
  s->MakeFixedDurationIntervalVarArray(kSize, 0, 100, 2, false, "intervals",
                                       &intervals);
  s->AddConstraint(s->MakeCover(intervals, target));
  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(2, intervals[i]->StartMin());
    EXPECT_EQ(15, intervals[i]->EndMax());
  }
  return nullptr;
}

TEST_IN_PROPAGATION0(CoverTest, PropagateBoundsDown, s, true) {
  const int kSize = 1000;
  IntervalVar* const target =
      s->MakeIntervalVar(0, 100, 2, 5, 4, 105, false, "target");
  std::vector<IntervalVar*> intervals;
  s->MakeFixedDurationIntervalVarArray(kSize, 0, 100, 5, false, "intervals",
                                       &intervals);
  s->AddConstraint(s->MakeCover(intervals, target));
  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(0, intervals[i]->StartMin());
    EXPECT_EQ(105, intervals[i]->EndMax());
  }
  target->SetStartMin(2);
  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(2, intervals[i]->StartMin());
    EXPECT_EQ(105, intervals[i]->EndMax());
  }
  target->SetEndMax(15);
  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(2, intervals[i]->StartMin());
    EXPECT_EQ(15, intervals[i]->EndMax());
  }
  return nullptr;
}

TEST_IN_PROPAGATION0(CoverTest, InitialPropagateUnperformedDown, s, true) {
  const int kSize = 1000;
  IntervalVar* const target =
      s->MakeIntervalVar(0, 10, 2, 5, 0, 20, true, "target");
  std::vector<IntervalVar*> intervals;
  s->MakeFixedDurationIntervalVarArray(kSize, 2, 10, 2, true, "intervals",
                                       &intervals);
  target->SetPerformed(false);
  s->AddConstraint(s->MakeCover(intervals, target));
  for (int i = 0; i < kSize; ++i) {
    EXPECT_FALSE(intervals[i]->MayBePerformed());
  }
  return nullptr;
}

TEST_IN_PROPAGATION0(CoverTest, PropagateUnperformedDown, s, true) {
  const int kSize = 1000;
  IntervalVar* const target =
      s->MakeIntervalVar(0, 10, 2, 5, 0, 20, true, "target");
  std::vector<IntervalVar*> intervals;
  s->MakeFixedDurationIntervalVarArray(kSize, 2, 10, 2, true, "intervals",
                                       &intervals);
  s->AddConstraint(s->MakeCover(intervals, target));
  for (int i = 0; i < kSize; ++i) {
    EXPECT_TRUE(intervals[i]->MayBePerformed());
  }
  target->SetPerformed(false);
  for (int i = 0; i < kSize; ++i) {
    EXPECT_FALSE(intervals[i]->MayBePerformed());
  }
  return nullptr;
}

TEST_IN_PROPAGATION0(CoverTest, InitialPropagatePerformedDown, s, true) {
  const int kSize = 1000;
  IntervalVar* const target =
      s->MakeIntervalVar(0, 10, 2, 5, 0, 20, false, "target");
  std::vector<IntervalVar*> intervals;
  s->MakeFixedDurationIntervalVarArray(kSize, 2, 10, 2, true, "intervals",
                                       &intervals);
  for (int i = 0; i < kSize - 1; ++i) {
    intervals[i]->SetPerformed(false);
  }
  s->AddConstraint(s->MakeCover(intervals, target));
  EXPECT_TRUE(intervals[kSize - 1]->MustBePerformed());
  return nullptr;
}

TEST_IN_PROPAGATION0(CoverTest, PropagatePerformedDown, s, true) {
  const int kSize = 1000;
  IntervalVar* const target =
      s->MakeIntervalVar(0, 10, 2, 5, 0, 20, false, "target");
  std::vector<IntervalVar*> intervals;
  s->MakeFixedDurationIntervalVarArray(kSize, 2, 10, 2, true, "intervals",
                                       &intervals);
  s->AddConstraint(s->MakeCover(intervals, target));
  for (int i = 0; i < kSize - 1; ++i) {
    EXPECT_FALSE(intervals[kSize - 1]->MustBePerformed());
    intervals[i]->SetPerformed(false);
  }
  EXPECT_TRUE(intervals[kSize - 1]->MustBePerformed());
  return nullptr;
}

TEST_IN_PROPAGATION0(CoverTest, InitialPropagateAllBoundsUp, s, true) {
  IntervalVar* const target =
      s->MakeIntervalVar(0, 48, 2, 50, 2, 50, false, "target");
  IntervalVar* const var1 = s->MakeFixedInterval(0, 5, "var1");
  IntervalVar* const var2 = s->MakeFixedInterval(5, 5, "var2");
  std::vector<IntervalVar*> intervals;
  intervals.push_back(var1);
  intervals.push_back(var2);
  s->AddConstraint(s->MakeCover(intervals, target));
  EXPECT_EQ(0, target->StartMin());
  EXPECT_EQ(0, target->StartMax());
  EXPECT_EQ(10, target->EndMin());
  EXPECT_EQ(10, target->EndMax());
  EXPECT_EQ(10, target->DurationMin());
  EXPECT_EQ(10, target->DurationMax());
  return nullptr;
}

TEST_IN_PROPAGATION0(CoverTest, PropagateAllBoundsUp, s, true) {
  IntervalVar* const target =
      s->MakeIntervalVar(0, 48, 2, 50, 2, 50, false, "target");
  IntervalVar* const var1 =
      s->MakeFixedDurationIntervalVar(0, 5, 5, true, "var1");
  IntervalVar* const var2 =
      s->MakeFixedDurationIntervalVar(10, 15, 5, true, "var2");
  std::vector<IntervalVar*> intervals;
  intervals.push_back(var1);
  intervals.push_back(var2);
  s->AddConstraint(s->MakeCover(intervals, target));
  EXPECT_EQ(0, target->StartMin());
  EXPECT_EQ(18, target->StartMax());
  EXPECT_EQ(2, target->EndMin());
  EXPECT_EQ(20, target->EndMax());
  EXPECT_EQ(2, target->DurationMin());
  EXPECT_EQ(20, target->DurationMax());
  var1->SetPerformed(true);
  EXPECT_EQ(0, target->StartMin());
  EXPECT_EQ(5, target->StartMax());
  EXPECT_EQ(5, target->EndMin());
  EXPECT_EQ(20, target->EndMax());
  EXPECT_EQ(2, target->DurationMin());
  EXPECT_EQ(20, target->DurationMax());
  var2->SetPerformed(true);
  EXPECT_EQ(0, target->StartMin());
  EXPECT_EQ(5, target->StartMax());
  EXPECT_EQ(15, target->EndMin());
  EXPECT_EQ(20, target->EndMax());
  EXPECT_EQ(10, target->DurationMin());
  EXPECT_EQ(20, target->DurationMax());
  var1->SetStartRange(2, 4);
  EXPECT_EQ(2, target->StartMin());
  EXPECT_EQ(4, target->StartMax());
  EXPECT_EQ(15, target->EndMin());
  EXPECT_EQ(20, target->EndMax());
  EXPECT_EQ(11, target->DurationMin());
  EXPECT_EQ(18, target->DurationMax());
  var2->SetEndRange(16, 19);
  EXPECT_EQ(2, target->StartMin());
  EXPECT_EQ(4, target->StartMax());
  EXPECT_EQ(16, target->EndMin());
  EXPECT_EQ(19, target->EndMax());
  EXPECT_EQ(12, target->DurationMin());
  EXPECT_EQ(17, target->DurationMax());
  return nullptr;
}

TEST_IN_PROPAGATION0(CoverTest, IntegrationTest, s, true) {
  const int kStart = 0;
  const int kEnd = 100;
  const int kDuration = 5;
  const int kSyncDuration = 3;
  IntervalVar* const cover1 = s->MakeIntervalVar(kStart, kEnd, 1, kEnd - kStart,
                                                 kStart, kEnd, false, "cover1");
  IntervalVar* const claim1a = s->MakeFixedDurationIntervalVar(
      kStart, kEnd, kDuration, false, "claim1a");
  IntervalVar* const claim1b = s->MakeFixedDurationIntervalVar(
      kStart, kEnd, kDuration, false, "claim1b");
  IntervalVar* const sync1 = s->MakeFixedDurationStartSyncedOnStartIntervalVar(
      cover1, kSyncDuration, 0);
  IntervalVar* const cover2 = s->MakeIntervalVar(kStart, kEnd, 1, kEnd - kStart,
                                                 kStart, kEnd, false, "cover2");
  IntervalVar* const claim2a = s->MakeFixedDurationIntervalVar(
      kStart, kEnd, kDuration, false, "claim2a");
  IntervalVar* const claim2b = s->MakeFixedDurationIntervalVar(
      kStart, kEnd, kDuration, false, "claim2b");
  IntervalVar* const sync2 = s->MakeFixedDurationStartSyncedOnStartIntervalVar(
      cover2, kSyncDuration, 0);
  std::vector<IntervalVar*> i1 = {claim1a, claim1b, sync1};
  std::vector<IntervalVar*> i2 = {claim2a, claim2b, sync2};
  std::vector<IntervalVar*> c1 = {claim1a, claim1b};
  std::vector<IntervalVar*> c2 = {claim2a, claim2b};
  std::vector<IntervalVar*> covers = {cover1, cover2};
  s->AddConstraint(s->MakeCover(i1, cover1));
  s->AddConstraint(s->MakeCover(i2, cover2));
  s->AddConstraint(s->MakeDisjunctiveConstraint(c1, "c1"));
  s->AddConstraint(s->MakeDisjunctiveConstraint(c2, "c2"));
  s->AddConstraint(s->MakeDisjunctiveConstraint(covers, "covers"));

  claim1a->SetStartMax(claim1a->StartMin());
  EXPECT_EQ(kStart, claim1a->StartMax());
  EXPECT_EQ(kDuration, claim1b->StartMin());
  EXPECT_EQ(kStart, cover1->StartMax());
  EXPECT_EQ(kStart + kDuration + kDuration, cover2->StartMin());
  return nullptr;
}

// ----- EqualityTest -----

class EqualityTest : public testing::Test {
 protected:
  void SetUp() override { solver_ = std::make_unique<Solver>("EqualityTest"); }
  std::unique_ptr<Solver> solver_;
};

TEST_F(EqualityTest, DebugString) {
  IntervalVar* const left =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "left");
  IntervalVar* const right =
      solver_->MakeFixedDurationIntervalVar(0, 10, 5, true, "right");
  Constraint* const ct = solver_->MakeEquality(left, right);
  EXPECT_FALSE(ct->DebugString().empty());
}

void ExpectEquality(IntervalVar* const left, IntervalVar* const right) {
  if (!left->MayBePerformed() || !right->MayBePerformed()) {
    EXPECT_EQ(left->MayBePerformed(), right->MayBePerformed());
  } else {
    EXPECT_EQ(left->StartMin(), right->StartMin());
    EXPECT_EQ(left->StartMax(), right->StartMax());
    EXPECT_EQ(left->DurationMin(), right->DurationMin());
    EXPECT_EQ(left->DurationMax(), right->DurationMax());
    EXPECT_EQ(left->EndMin(), right->EndMin());
    EXPECT_EQ(left->EndMax(), right->EndMax());
    EXPECT_EQ(left->MustBePerformed(), right->MustBePerformed());
    EXPECT_EQ(left->MayBePerformed(), right->MayBePerformed());
  }
}

TEST_IN_PROPAGATION0(EqualityTest, PropagateLeft, s, true) {
  IntervalVar* const left =
      s->MakeIntervalVar(0, 10, 2, 5, 4, 13, true, "left");
  IntervalVar* const right =
      s->MakeIntervalVar(1, 9, 2, 5, 4, 13, true, "right");
  Constraint* const ct = s->MakeEquality(left, right);
  s->AddConstraint(ct);
  ExpectEquality(left, right);
  left->SetStartRange(2, 8);
  ExpectEquality(left, right);
  left->SetDurationRange(3, 4);
  ExpectEquality(left, right);
  left->SetEndRange(7, 9);
  ExpectEquality(left, right);
  left->SetPerformed(true);
  ExpectEquality(left, right);
  return nullptr;
}

TEST_IN_PROPAGATION0(EqualityTest, PropagateRight, s, true) {
  IntervalVar* const right =
      s->MakeIntervalVar(0, 10, 2, 5, 4, 13, true, "right");
  IntervalVar* const left = s->MakeIntervalVar(1, 9, 2, 5, 4, 13, true, "left");
  Constraint* const ct = s->MakeEquality(right, left);
  s->AddConstraint(ct);
  ExpectEquality(right, left);
  right->SetStartRange(2, 8);
  ExpectEquality(right, left);
  right->SetDurationRange(3, 4);
  ExpectEquality(right, left);
  right->SetEndRange(7, 9);
  ExpectEquality(right, left);
  right->SetPerformed(true);
  ExpectEquality(right, left);
  return nullptr;
}

TEST_IN_PROPAGATION0(EqualityTest, PropagateLeftUnperformed, s, true) {
  IntervalVar* const left =
      s->MakeIntervalVar(0, 10, 2, 5, 4, 13, true, "left");
  IntervalVar* const right =
      s->MakeIntervalVar(1, 9, 2, 5, 4, 13, true, "right");
  Constraint* const ct = s->MakeEquality(left, right);
  s->AddConstraint(ct);
  ExpectEquality(left, right);
  left->SetPerformed(false);
  ExpectEquality(left, right);
  return nullptr;
}

TEST_IN_PROPAGATION0(EqualityTest, PropagateRightUnperformed, s, true) {
  IntervalVar* const left =
      s->MakeIntervalVar(0, 10, 2, 5, 4, 13, true, "left");
  IntervalVar* const right =
      s->MakeIntervalVar(1, 9, 2, 5, 4, 13, true, "right");
  Constraint* const ct = s->MakeEquality(left, right);
  s->AddConstraint(ct);
  ExpectEquality(left, right);
  right->SetPerformed(false);
  ExpectEquality(left, right);
  return nullptr;
}
}  // namespace
}  // namespace operations_research
