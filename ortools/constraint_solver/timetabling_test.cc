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

#include <algorithm>
#include <memory>
#include <string>

#include "gtest/gtest.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solver_test_util.h"
#include "ortools/constraint_solver/interval.h"

namespace operations_research {
namespace {

// ----- Unary Relation -----

class UnaryRelationTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("UnaryRelationTest");
  }
  std::unique_ptr<Solver> solver_;
};

TEST_F(UnaryRelationTest, DebugString) {
  IntervalVar* i = solver_->MakeFixedDurationIntervalVar(0, 10, 3, true, "i");
  Constraint* ct = solver_->MakeIntervalVarRelation(i, Solver::ENDS_AFTER, 5);
  EXPECT_FALSE(ct->DebugString().empty());
  ct = solver_->MakeIntervalVarRelation(i, Solver::ENDS_AT, 5);
  EXPECT_FALSE(ct->DebugString().empty());
  ct = solver_->MakeIntervalVarRelation(i, Solver::ENDS_BEFORE, 5);
  EXPECT_FALSE(ct->DebugString().empty());
  ct = solver_->MakeIntervalVarRelation(i, Solver::STARTS_AFTER, 5);
  EXPECT_FALSE(ct->DebugString().empty());
  ct = solver_->MakeIntervalVarRelation(i, Solver::STARTS_AT, 5);
  EXPECT_FALSE(ct->DebugString().empty());
  ct = solver_->MakeIntervalVarRelation(i, Solver::STARTS_BEFORE, 5);
  EXPECT_FALSE(ct->DebugString().empty());
  ct = solver_->MakeIntervalVarRelation(i, Solver::CROSS_DATE, 5);
  EXPECT_FALSE(ct->DebugString().empty());
  ct = solver_->MakeIntervalVarRelation(i, Solver::AVOID_DATE, 5);
  EXPECT_FALSE(ct->DebugString().empty());
}

TEST_IN_PROPAGATION0(UnaryRelationTest, UnaryDirectPropagation, solver, true) {
  IntervalVar* i = solver->MakeFixedDurationIntervalVar(0, 10, 3, true, "i");
  Constraint* ct = solver->MakeIntervalVarRelation(i, Solver::ENDS_AFTER, 5);
  solver->AddConstraint(ct);
  EXPECT_EQ(5, i->EndMin());
  i->SetStartMax(1);
  EXPECT_FALSE(i->MayBePerformed());
  i = solver->MakeFixedDurationIntervalVar(0, 10, 3, true, "i");
  ct = solver->MakeIntervalVarRelation(i, Solver::ENDS_AT, 5);
  solver->AddConstraint(ct);
  EXPECT_EQ(5, i->EndMin());
  EXPECT_EQ(5, i->EndMax());
  i->SetStartMax(1);
  EXPECT_FALSE(i->MayBePerformed());
  i = solver->MakeFixedDurationIntervalVar(0, 10, 3, true, "i");
  ct = solver->MakeIntervalVarRelation(i, Solver::ENDS_BEFORE, 5);
  solver->AddConstraint(ct);
  EXPECT_EQ(3, i->EndMin());
  EXPECT_EQ(5, i->EndMax());
  EXPECT_EQ(0, i->StartMin());
  EXPECT_EQ(2, i->StartMax());
  i = solver->MakeFixedDurationIntervalVar(0, 10, 3, true, "i");
  ct = solver->MakeIntervalVarRelation(i, Solver::STARTS_AFTER, 5);
  solver->AddConstraint(ct);
  EXPECT_EQ(5, i->StartMin());
  EXPECT_EQ(10, i->StartMax());
  i = solver->MakeFixedDurationIntervalVar(0, 10, 3, true, "i");
  ct = solver->MakeIntervalVarRelation(i, Solver::STARTS_AT, 5);
  solver->AddConstraint(ct);
  EXPECT_EQ(5, i->StartMin());
  EXPECT_EQ(5, i->StartMax());
  i = solver->MakeFixedDurationIntervalVar(0, 10, 3, true, "i");
  ct = solver->MakeIntervalVarRelation(i, Solver::STARTS_BEFORE, 5);
  solver->AddConstraint(ct);
  EXPECT_EQ(0, i->StartMin());
  EXPECT_EQ(5, i->StartMax());
  i = solver->MakeFixedDurationIntervalVar(0, 10, 3, true, "i");
  ct = solver->MakeIntervalVarRelation(i, Solver::CROSS_DATE, 5);
  solver->AddConstraint(ct);
  EXPECT_EQ(2, i->StartMin());
  EXPECT_EQ(5, i->StartMax());
  i = solver->MakeFixedDurationIntervalVar(0, 10, 3, true, "i");
  ct = solver->MakeIntervalVarRelation(i, Solver::AVOID_DATE, 5);
  solver->AddConstraint(ct);
  EXPECT_EQ(0, i->StartMin());
  EXPECT_EQ(10, i->StartMax());
  i->SetStartMin(2);
  EXPECT_EQ(2, i->StartMin());
  EXPECT_EQ(10, i->StartMax());
  i->SetStartMin(3);
  EXPECT_EQ(5, i->StartMin());
  EXPECT_EQ(10, i->StartMax());
  i = solver->MakeFixedDurationIntervalVar(0, 10, 3, true, "i");
  ct = solver->MakeIntervalVarRelation(i, Solver::AVOID_DATE, 5);
  solver->AddConstraint(ct);
  EXPECT_EQ(0, i->StartMin());
  EXPECT_EQ(10, i->StartMax());
  i->SetStartMax(5);
  EXPECT_EQ(0, i->StartMin());
  EXPECT_EQ(5, i->StartMax());
  i->SetStartMax(4);
  EXPECT_EQ(0, i->StartMin());
  EXPECT_EQ(2, i->StartMax());
  return nullptr;
};

// ----- Binary reations ------

class BinaryRelationTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("BinaryRelationTest");
  }
  std::unique_ptr<Solver> solver_;
};

struct Bounds {
  const int start_min_x;
  const int start_max_x;
  const int duration_x;
  const int start_min_y;
  const int start_max_y;
  const int duration_y;
  const int delay;
  Bounds(int x1, int x2, int dx, int y1, int y2, int dy, int d)
      : start_min_x(x1),
        start_max_x(x2),
        duration_x(dx),
        start_min_y(y1),
        start_max_y(y2),
        duration_y(dy),
        delay(d) {}
};

void TestAfterRelation(Solver* const solver, Solver::BinaryIntervalRelation rel,
                       int x_to_y, struct Bounds bounds, bool x_first) {
  IntervalVar* const x = solver->MakeFixedDurationIntervalVar(
      bounds.start_min_x, bounds.start_max_x, bounds.duration_x, true, "x");
  IntervalVar* const y = solver->MakeFixedDurationIntervalVar(
      bounds.start_min_y, bounds.start_max_y, bounds.duration_y, true, "y");
  Constraint* const constraint =
      solver->MakeIntervalVarRelationWithDelay(y, rel, x, bounds.delay);
  solver->AddConstraint(constraint);
  // No Propagation
  EXPECT_EQ(bounds.start_min_x, x->StartMin());
  EXPECT_EQ(bounds.start_max_x, x->StartMax());
  EXPECT_EQ(bounds.start_min_y, y->StartMin());
  EXPECT_EQ(bounds.start_max_y, y->StartMax());
  if (x_first) {
    // Propagate from x to y.
    x->SetPerformed(true);
    EXPECT_EQ(bounds.start_min_x, x->StartMin());
    EXPECT_EQ(bounds.start_max_x, x->StartMax());
    EXPECT_EQ(std::max(bounds.start_min_y,
                       bounds.start_min_x + bounds.delay + x_to_y),
              y->StartMin());
    EXPECT_EQ(bounds.start_max_y, y->StartMax());
    // Propagate both ways, thus from y to x here.
    y->SetPerformed(true);
  } else {
    // Propagate from y to x.
    y->SetPerformed(true);
    EXPECT_EQ(bounds.start_min_x, x->StartMin());
    EXPECT_EQ(std::min(bounds.start_max_x,
                       bounds.start_max_y - bounds.delay - x_to_y),
              x->StartMax());
    EXPECT_EQ(bounds.start_min_y, y->StartMin());
    EXPECT_EQ(bounds.start_max_y, y->StartMax());

    // Propagate both ways, thus from x to y here.
    x->SetPerformed(true);
  }
  EXPECT_EQ(bounds.start_min_x, x->StartMin());
  EXPECT_EQ(
      std::min(bounds.start_max_x, bounds.start_max_y - bounds.delay - x_to_y),
      x->StartMax());
  EXPECT_EQ(
      std::max(bounds.start_min_y, bounds.start_min_x + bounds.delay + x_to_y),
      y->StartMin());
  EXPECT_EQ(bounds.start_max_y, y->StartMax());
}

TEST_IN_PROPAGATION0(BinaryRelationTest, StartsAfterStartWithDelay1, solver_,
                     true) {
  struct Bounds b(0, 50, 2, 20, 80, 3, 5);
  TestAfterRelation(solver_, Solver::STARTS_AFTER_START, 0, b, true);
  TestAfterRelation(solver_, Solver::STARTS_AFTER_START, 0, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, StartsAfterStartWithDelay2, solver_,
                     true) {
  struct Bounds b(0, 100, 2, 20, 80, 3, 5);
  TestAfterRelation(solver_, Solver::STARTS_AFTER_START, 0, b, true);
  TestAfterRelation(solver_, Solver::STARTS_AFTER_START, 0, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, StartsAfterStartWithDelay3, solver_,
                     true) {
  struct Bounds b(20, 80, 2, 0, 100, 3, 5);
  TestAfterRelation(solver_, Solver::STARTS_AFTER_START, 0, b, true);
  TestAfterRelation(solver_, Solver::STARTS_AFTER_START, 0, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, StartsAfterStartWithDelay4, solver_,
                     true) {
  struct Bounds b(20, 80, 2, 0, 50, 3, 5);
  TestAfterRelation(solver_, Solver::STARTS_AFTER_START, 0, b, true);
  TestAfterRelation(solver_, Solver::STARTS_AFTER_START, 0, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, StartsAfterEndWithDelay1, solver_,
                     true) {
  struct Bounds b(0, 50, 2, 20, 80, 3, 5);
  TestAfterRelation(solver_, Solver::STARTS_AFTER_END, 2, b, true);
  TestAfterRelation(solver_, Solver::STARTS_AFTER_END, 2, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, StartsAfterEndWithDelay2, solver_,
                     true) {
  struct Bounds b(0, 100, 2, 20, 80, 3, 5);
  TestAfterRelation(solver_, Solver::STARTS_AFTER_END, 2, b, true);
  TestAfterRelation(solver_, Solver::STARTS_AFTER_END, 2, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, StartsAfterEndWithDelay3, solver_,
                     true) {
  struct Bounds b(20, 80, 2, 0, 100, 3, 5);
  TestAfterRelation(solver_, Solver::STARTS_AFTER_END, 2, b, true);
  TestAfterRelation(solver_, Solver::STARTS_AFTER_END, 2, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, StartsAfterEndWithDelay4, solver_,
                     true) {
  struct Bounds b(20, 80, 2, 0, 50, 3, 5);
  TestAfterRelation(solver_, Solver::STARTS_AFTER_END, 2, b, true);
  TestAfterRelation(solver_, Solver::STARTS_AFTER_END, 2, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, EndsAfterStartWithDelay1, solver_,
                     true) {
  struct Bounds b(0, 50, 2, 20, 80, 3, 5);
  TestAfterRelation(solver_, Solver::ENDS_AFTER_START, -3, b, true);
  TestAfterRelation(solver_, Solver::ENDS_AFTER_START, -3, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, EndsAfterStartWithDelay2, solver_,
                     true) {
  struct Bounds b(0, 100, 2, 20, 80, 3, 5);
  TestAfterRelation(solver_, Solver::ENDS_AFTER_START, -3, b, true);
  TestAfterRelation(solver_, Solver::ENDS_AFTER_START, -3, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, EndsAfterStartWithDelay3, solver_,
                     true) {
  struct Bounds b(20, 80, 2, 0, 100, 3, 5);
  TestAfterRelation(solver_, Solver::ENDS_AFTER_START, -3, b, true);
  TestAfterRelation(solver_, Solver::ENDS_AFTER_START, -3, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, EndsAfterStartWithDelay4, solver_,
                     true) {
  struct Bounds b(20, 80, 2, 0, 50, 3, 5);
  TestAfterRelation(solver_, Solver::ENDS_AFTER_START, -3, b, true);
  TestAfterRelation(solver_, Solver::ENDS_AFTER_START, -3, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, EndsAfterEndWithDelay1, solver_,
                     true) {
  struct Bounds b(0, 50, 2, 20, 80, 3, 5);
  TestAfterRelation(solver_, Solver::ENDS_AFTER_END, -1, b, true);
  TestAfterRelation(solver_, Solver::ENDS_AFTER_END, -1, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, EndsAfterEndWithDelay2, solver_,
                     true) {
  struct Bounds b(0, 100, 2, 20, 80, 3, 5);
  TestAfterRelation(solver_, Solver::ENDS_AFTER_END, -1, b, true);
  TestAfterRelation(solver_, Solver::ENDS_AFTER_END, -1, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, EndsAfterEndWithDelay3, solver_,
                     true) {
  struct Bounds b(20, 80, 2, 0, 100, 3, 5);
  TestAfterRelation(solver_, Solver::ENDS_AFTER_END, -1, b, true);
  TestAfterRelation(solver_, Solver::ENDS_AFTER_END, -1, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, EndsAfterEndWithDelay4, solver_,
                     true) {
  struct Bounds b(20, 80, 2, 0, 50, 3, 5);
  TestAfterRelation(solver_, Solver::ENDS_AFTER_END, -1, b, true);
  TestAfterRelation(solver_, Solver::ENDS_AFTER_END, -1, b, false);
  return nullptr;
}

void TestAtRelation(Solver* const solver, Solver::BinaryIntervalRelation rel,
                    int x_to_y, struct Bounds bounds, bool x_first) {
  IntervalVar* const x = solver->MakeFixedDurationIntervalVar(
      bounds.start_min_x, bounds.start_max_x, bounds.duration_x, true, "x");
  IntervalVar* const y = solver->MakeFixedDurationIntervalVar(
      bounds.start_min_y, bounds.start_max_y, bounds.duration_y, true, "y");
  Constraint* const constraint =
      solver->MakeIntervalVarRelationWithDelay(y, rel, x, bounds.delay);
  solver->AddConstraint(constraint);
  // No Propagation
  EXPECT_EQ(bounds.start_min_x, x->StartMin());
  EXPECT_EQ(bounds.start_max_x, x->StartMax());
  EXPECT_EQ(bounds.start_min_y, y->StartMin());
  EXPECT_EQ(bounds.start_max_y, y->StartMax());
  if (x_first) {
    // Propagate from x to y.
    x->SetPerformed(true);
    EXPECT_EQ(bounds.start_min_x, x->StartMin());
    EXPECT_EQ(bounds.start_max_x, x->StartMax());
    EXPECT_EQ(std::max(bounds.start_min_y,
                       bounds.start_min_x + bounds.delay + x_to_y),
              y->StartMin());
    EXPECT_EQ(std::min(bounds.start_max_y,
                       bounds.start_max_x + bounds.delay + x_to_y),
              y->StartMax());
    // Propagate both ways, thus from y to x here.
    y->SetPerformed(true);
  } else {
    // Propagate from y to x.
    y->SetPerformed(true);
    EXPECT_EQ(std::max(bounds.start_min_x,
                       bounds.start_min_y - bounds.delay - x_to_y),
              x->StartMin());
    EXPECT_EQ(std::min(bounds.start_max_x,
                       bounds.start_max_y - bounds.delay - x_to_y),
              x->StartMax());
    EXPECT_EQ(bounds.start_min_y, y->StartMin());
    EXPECT_EQ(bounds.start_max_y, y->StartMax());

    // Propagate both ways, thus from x to y here.
    x->SetPerformed(true);
  }
  EXPECT_EQ(
      std::max(bounds.start_min_x, bounds.start_min_y - bounds.delay - x_to_y),
      x->StartMin());
  EXPECT_EQ(
      std::min(bounds.start_max_x, bounds.start_max_y - bounds.delay - x_to_y),
      x->StartMax());
  EXPECT_EQ(
      std::max(bounds.start_min_y, bounds.start_min_x + bounds.delay + x_to_y),
      y->StartMin());
  EXPECT_EQ(
      std::min(bounds.start_max_y, bounds.start_max_x + bounds.delay + x_to_y),
      y->StartMax());
}

TEST_IN_PROPAGATION0(BinaryRelationTest, StartsAtStartWithDelay1, solver_,
                     true) {
  struct Bounds b(0, 50, 2, 20, 80, 3, 5);
  TestAtRelation(solver_, Solver::STARTS_AT_START, 0, b, true);
  TestAtRelation(solver_, Solver::STARTS_AT_START, 0, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, StartsAtStartWithDelay2, solver_,
                     true) {
  struct Bounds b(0, 100, 2, 20, 80, 3, 5);
  TestAtRelation(solver_, Solver::STARTS_AT_START, 0, b, true);
  TestAtRelation(solver_, Solver::STARTS_AT_START, 0, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, StartsAtStartWithDelay3, solver_,
                     true) {
  struct Bounds b(20, 80, 2, 0, 100, 3, 5);
  TestAtRelation(solver_, Solver::STARTS_AT_START, 0, b, true);
  TestAtRelation(solver_, Solver::STARTS_AT_START, 0, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, StartsAtStartWithDelay4, solver_,
                     true) {
  struct Bounds b(20, 80, 2, 0, 50, 3, 5);
  TestAtRelation(solver_, Solver::STARTS_AT_START, 0, b, true);
  TestAtRelation(solver_, Solver::STARTS_AT_START, 0, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, StartsAtEndWithDelay1, solver_, true) {
  struct Bounds b(0, 50, 2, 20, 80, 3, 5);
  TestAtRelation(solver_, Solver::STARTS_AT_END, 2, b, true);
  TestAtRelation(solver_, Solver::STARTS_AT_END, 2, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, StartsAtEndWithDelay2, solver_, true) {
  struct Bounds b(0, 100, 2, 20, 80, 3, 5);
  TestAtRelation(solver_, Solver::STARTS_AT_END, 2, b, true);
  TestAtRelation(solver_, Solver::STARTS_AT_END, 2, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, StartsAtEndWithDelay3, solver_, true) {
  struct Bounds b(20, 80, 2, 0, 100, 3, 5);
  TestAtRelation(solver_, Solver::STARTS_AT_END, 2, b, true);
  TestAtRelation(solver_, Solver::STARTS_AT_END, 2, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, StartsAtEndWithDelay4, solver_, true) {
  struct Bounds b(20, 80, 2, 0, 50, 3, 5);
  TestAtRelation(solver_, Solver::STARTS_AT_END, 2, b, true);
  TestAtRelation(solver_, Solver::STARTS_AT_END, 2, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, EndsAtStartWithDelay1, solver_, true) {
  struct Bounds b(0, 50, 2, 20, 80, 3, 5);
  TestAtRelation(solver_, Solver::ENDS_AT_START, -3, b, true);
  TestAtRelation(solver_, Solver::ENDS_AT_START, -3, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, EndsAtStartWithDelay2, solver_, true) {
  struct Bounds b(0, 100, 2, 20, 80, 3, 5);
  TestAtRelation(solver_, Solver::ENDS_AT_START, -3, b, true);
  TestAtRelation(solver_, Solver::ENDS_AT_START, -3, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, EndsAtStartWithDelay3, solver_, true) {
  struct Bounds b(20, 80, 2, 0, 100, 3, 5);
  TestAtRelation(solver_, Solver::ENDS_AT_START, -3, b, true);
  TestAtRelation(solver_, Solver::ENDS_AT_START, -3, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, EndsAtStartWithDelay4, solver_, true) {
  struct Bounds b(20, 80, 2, 0, 50, 3, 5);
  TestAtRelation(solver_, Solver::ENDS_AT_START, -3, b, true);
  TestAtRelation(solver_, Solver::ENDS_AT_START, -3, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, EndsAtEndWithDelay1, solver_, true) {
  struct Bounds b(0, 50, 2, 20, 80, 3, 5);
  TestAtRelation(solver_, Solver::ENDS_AT_END, -1, b, true);
  TestAtRelation(solver_, Solver::ENDS_AT_END, -1, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, EndsAtEndWithDelay2, solver_, true) {
  struct Bounds b(0, 100, 2, 20, 80, 3, 5);
  TestAtRelation(solver_, Solver::ENDS_AT_END, -1, b, true);
  TestAtRelation(solver_, Solver::ENDS_AT_END, -1, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, EndsAtEndWithDelay3, solver_, true) {
  struct Bounds b(20, 80, 2, 0, 100, 3, 5);
  TestAtRelation(solver_, Solver::ENDS_AT_END, -1, b, true);
  TestAtRelation(solver_, Solver::ENDS_AT_END, -1, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, EndsAtEndWithDelay4, solver_, true) {
  struct Bounds b(20, 80, 2, 0, 50, 3, 5);
  TestAtRelation(solver_, Solver::ENDS_AT_END, -1, b, true);
  TestAtRelation(solver_, Solver::ENDS_AT_END, -1, b, false);
  return nullptr;
}

void TestSyncRelation(Solver* const solver, struct Bounds bounds,
                      bool x_first) {
  IntervalVar* const x = solver->MakeFixedDurationIntervalVar(
      bounds.start_min_x, bounds.start_max_x, bounds.duration_x, true, "x");
  IntervalVar* const y = solver->MakeFixedDurationIntervalVar(
      bounds.start_min_y, bounds.start_max_y, bounds.duration_y, true, "y");
  Constraint* const constraint = solver->MakeIntervalVarRelationWithDelay(
      y, Solver::STAYS_IN_SYNC, x, bounds.delay);
  solver->AddConstraint(constraint);
  // No Propagation
  EXPECT_EQ(bounds.start_min_x, x->StartMin());
  EXPECT_EQ(bounds.start_max_x, x->StartMax());
  EXPECT_EQ(bounds.start_min_y, y->StartMin());
  EXPECT_EQ(bounds.start_max_y, y->StartMax());
  if (x_first) {
    // Propagate from x to y.
    x->SetPerformed(true);
    EXPECT_EQ(bounds.start_min_x, x->StartMin());
    EXPECT_EQ(bounds.start_max_x, x->StartMax());
    EXPECT_EQ(std::max(bounds.start_min_y, bounds.start_min_x + bounds.delay),
              y->StartMin());
    EXPECT_EQ(std::min(bounds.start_max_y, bounds.start_max_x + bounds.delay),
              y->StartMax());
    // Propagate both ways, thus from y to x here.
    y->SetPerformed(true);
  } else {
    // Propagate from y to x.
    y->SetPerformed(true);
    EXPECT_EQ(std::max(bounds.start_min_x, bounds.start_min_y - bounds.delay),
              x->StartMin());
    EXPECT_EQ(std::min(bounds.start_max_x, bounds.start_max_y - bounds.delay),
              x->StartMax());
    EXPECT_EQ(bounds.start_min_y, y->StartMin());
    EXPECT_EQ(bounds.start_max_y, y->StartMax());

    // Propagate both ways, thus from x to y here.
    x->SetPerformed(true);
  }
  EXPECT_EQ(std::max(bounds.start_min_x, bounds.start_min_y - bounds.delay),
            x->StartMin());
  EXPECT_EQ(std::min(bounds.start_max_x, bounds.start_max_y - bounds.delay),
            x->StartMax());
  EXPECT_EQ(std::max(bounds.start_min_y, bounds.start_min_x + bounds.delay),
            y->StartMin());
  EXPECT_EQ(std::min(bounds.start_max_y, bounds.start_max_x + bounds.delay),
            y->StartMax());
}

TEST_IN_PROPAGATION0(BinaryRelationTest, InSyncWithDelay1, solver_, true) {
  // Durations must be equals.
  struct Bounds b(0, 50, 3, 20, 80, 3, 5);
  TestSyncRelation(solver_, b, true);
  TestSyncRelation(solver_, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, InSyncWithDelay2, solver_, true) {
  // Durations must be equals.
  struct Bounds b(0, 100, 3, 20, 80, 3, 5);
  TestSyncRelation(solver_, b, true);
  TestSyncRelation(solver_, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, InSyncWithDelay3, solver_, true) {
  // Durations must be equals.
  struct Bounds b(20, 80, 3, 0, 100, 3, 5);
  TestSyncRelation(solver_, b, true);
  TestSyncRelation(solver_, b, false);
  return nullptr;
}

TEST_IN_PROPAGATION0(BinaryRelationTest, InSyncWithDelay4, solver_, true) {
  // Durations must be equals.
  struct Bounds b(20, 80, 3, 0, 50, 3, 5);
  TestSyncRelation(solver_, b, true);
  TestSyncRelation(solver_, b, false);
  return nullptr;
}

// ----- Temporal Disjunction -----

class TemporalDisjunctionTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("TemporalDisjunctionTest");
  }
  std::unique_ptr<Solver> solver_;
};

TEST_F(TemporalDisjunctionTest, DebugString) {
  IntervalVar* x = solver_->MakeFixedDurationIntervalVar(0, 10, 3, false, "x");
  IntervalVar* y = solver_->MakeFixedDurationIntervalVar(0, 10, 2, false, "y");
  IntVar* b = solver_->MakeBoolVar("b");
  Constraint* ct = solver_->MakeTemporalDisjunction(x, y, b);
  ASSERT_FALSE(ct->DebugString().empty());
}

TEST_IN_PROPAGATION0(TemporalDisjunctionTest, AssignAltFalse, solver, true) {
  IntervalVar* x = solver->MakeFixedDurationIntervalVar(0, 10, 3, false, "x");
  IntervalVar* y = solver->MakeFixedDurationIntervalVar(0, 10, 2, false, "y");
  IntVar* b = solver->MakeBoolVar("b");
  solver->AddConstraint(solver->MakeTemporalDisjunction(x, y, b));
  EXPECT_EQ(0, b->Min());
  EXPECT_EQ(1, b->Max());
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(10, x->StartMax());
  EXPECT_EQ(0, y->StartMin());
  EXPECT_EQ(10, y->StartMax());
  b->SetValue(0);
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(7, x->StartMax());
  EXPECT_EQ(3, y->StartMin());
  EXPECT_EQ(10, y->StartMax());
  y->SetStartMax(9);
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(6, x->StartMax());
  EXPECT_EQ(3, y->StartMin());
  EXPECT_EQ(9, y->StartMax());
  x->SetStartMin(2);
  EXPECT_EQ(2, x->StartMin());
  EXPECT_EQ(6, x->StartMax());
  EXPECT_EQ(5, y->StartMin());
  EXPECT_EQ(9, y->StartMax());
  return nullptr;
};

TEST_IN_PROPAGATION0(TemporalDisjunctionTest, OptionalBound, solver_, true) {
  IntervalVar* x = solver_->MakeFixedDurationIntervalVar(0, 3, 2, false, "x");
  IntervalVar* y = solver_->MakeFixedDurationIntervalVar(3, 3, 1, true, "y");
  solver_->AddConstraint(solver_->MakeTemporalDisjunction(x, y));
  // There should have been no propagation
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(3, x->StartMax());
  EXPECT_EQ(3, y->StartMin());
  EXPECT_EQ(3, y->StartMax());
  // Make y mandatory
  y->SetPerformed(true);
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(1, x->StartMax());
  EXPECT_EQ(3, y->StartMin());
  EXPECT_EQ(3, y->StartMax());
  return nullptr;
}

TEST_IN_PROPAGATION0(TemporalDisjunctionTest, AssignAltTrue, solver, true) {
  IntervalVar* x = solver->MakeFixedDurationIntervalVar(0, 10, 3, false, "x");
  IntervalVar* y = solver->MakeFixedDurationIntervalVar(0, 10, 2, false, "y");
  IntVar* b = solver->MakeBoolVar("b");
  solver->AddConstraint(solver->MakeTemporalDisjunction(x, y, b));
  EXPECT_EQ(0, b->Min());
  EXPECT_EQ(1, b->Max());
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(10, x->StartMax());
  EXPECT_EQ(0, y->StartMin());
  EXPECT_EQ(10, y->StartMax());
  b->SetValue(1);
  EXPECT_EQ(2, x->StartMin());
  EXPECT_EQ(10, x->StartMax());
  EXPECT_EQ(0, y->StartMin());
  EXPECT_EQ(8, y->StartMax());
  x->SetStartMax(9);
  EXPECT_EQ(2, x->StartMin());
  EXPECT_EQ(9, x->StartMax());
  EXPECT_EQ(0, y->StartMin());
  EXPECT_EQ(7, y->StartMax());
  y->SetStartMin(2);
  EXPECT_EQ(4, x->StartMin());
  EXPECT_EQ(9, x->StartMax());
  EXPECT_EQ(2, y->StartMin());
  EXPECT_EQ(7, y->StartMax());
  return nullptr;
};

TEST_IN_PROPAGATION0(TemporalDisjunctionTest, ForceOneBeforeTwoFromOne, solver,
                     true) {
  IntervalVar* x = solver->MakeFixedDurationIntervalVar(0, 10, 4, false, "x");
  IntervalVar* y = solver->MakeFixedDurationIntervalVar(0, 10, 5, false, "y");
  solver->AddConstraint(solver->MakeTemporalDisjunction(x, y));
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(10, x->StartMax());
  EXPECT_EQ(0, y->StartMin());
  EXPECT_EQ(10, y->StartMax());
  x->SetStartMax(5);
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(5, x->StartMax());
  EXPECT_EQ(0, y->StartMin());
  EXPECT_EQ(10, y->StartMax());
  x->SetStartMax(4);
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(4, x->StartMax());
  EXPECT_EQ(4, y->StartMin());
  EXPECT_EQ(10, y->StartMax());
  return nullptr;
};

TEST_IN_PROPAGATION0(TemporalDisjunctionTest, ForceOneBeforeTwoFromTwo, solver,
                     true) {
  IntervalVar* x = solver->MakeFixedDurationIntervalVar(0, 10, 4, false, "x");
  IntervalVar* y = solver->MakeFixedDurationIntervalVar(0, 10, 5, false, "y");
  solver->AddConstraint(solver->MakeTemporalDisjunction(x, y));
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(10, x->StartMax());
  EXPECT_EQ(0, y->StartMin());
  EXPECT_EQ(10, y->StartMax());
  y->SetStartMin(5);
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(10, x->StartMax());
  EXPECT_EQ(5, y->StartMin());
  EXPECT_EQ(10, y->StartMax());
  y->SetStartMin(8);
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(6, x->StartMax());
  EXPECT_EQ(8, y->StartMin());
  EXPECT_EQ(10, y->StartMax());
  return nullptr;
};

TEST_IN_PROPAGATION0(TemporalDisjunctionTest, ForceOneBeforeTwoFromOneAlt,
                     solver, true) {
  IntervalVar* x = solver->MakeFixedDurationIntervalVar(0, 10, 4, false, "x");
  IntervalVar* y = solver->MakeFixedDurationIntervalVar(0, 10, 5, false, "y");
  IntVar* b = solver->MakeBoolVar("b");
  solver->AddConstraint(solver->MakeTemporalDisjunction(x, y, b));
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(10, x->StartMax());
  EXPECT_EQ(0, y->StartMin());
  EXPECT_EQ(10, y->StartMax());
  x->SetStartMax(5);
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(5, x->StartMax());
  EXPECT_EQ(0, y->StartMin());
  EXPECT_EQ(10, y->StartMax());
  EXPECT_EQ(0, b->Min());
  EXPECT_EQ(1, b->Max());
  x->SetStartMax(4);
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(4, x->StartMax());
  EXPECT_EQ(4, y->StartMin());
  EXPECT_EQ(10, y->StartMax());
  EXPECT_EQ(0, b->Min());
  EXPECT_EQ(0, b->Max());
  return nullptr;
};

TEST_IN_PROPAGATION0(TemporalDisjunctionTest, ForceOneBeforeTwoFromTwoAlt,
                     solver, true) {
  IntervalVar* x = solver->MakeFixedDurationIntervalVar(0, 10, 4, false, "x");
  IntervalVar* y = solver->MakeFixedDurationIntervalVar(0, 10, 5, false, "y");
  IntVar* b = solver->MakeBoolVar("b");
  solver->AddConstraint(solver->MakeTemporalDisjunction(x, y, b));
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(10, x->StartMax());
  EXPECT_EQ(0, y->StartMin());
  EXPECT_EQ(10, y->StartMax());
  y->SetStartMin(5);
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(10, x->StartMax());
  EXPECT_EQ(5, y->StartMin());
  EXPECT_EQ(10, y->StartMax());
  EXPECT_EQ(0, b->Min());
  EXPECT_EQ(1, b->Max());
  y->SetStartMin(8);
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(6, x->StartMax());
  EXPECT_EQ(8, y->StartMin());
  EXPECT_EQ(10, y->StartMax());
  EXPECT_EQ(0, b->Min());
  EXPECT_EQ(0, b->Max());
  return nullptr;
};

TEST_IN_PROPAGATION0(TemporalDisjunctionTest, ForceTwoBeforeOneFromOne, solver,
                     true) {
  IntervalVar* x = solver->MakeFixedDurationIntervalVar(0, 10, 4, false, "x");
  IntervalVar* y = solver->MakeFixedDurationIntervalVar(0, 10, 5, false, "y");
  solver->AddConstraint(solver->MakeTemporalDisjunction(x, y));
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(10, x->StartMax());
  EXPECT_EQ(0, y->StartMin());
  EXPECT_EQ(10, y->StartMax());
  y->SetStartMax(4);
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(10, x->StartMax());
  EXPECT_EQ(0, y->StartMin());
  EXPECT_EQ(4, y->StartMax());
  y->SetStartMax(3);
  EXPECT_EQ(5, x->StartMin());
  EXPECT_EQ(10, x->StartMax());
  EXPECT_EQ(0, y->StartMin());
  EXPECT_EQ(3, y->StartMax());
  return nullptr;
};

TEST_IN_PROPAGATION0(TemporalDisjunctionTest, ForceTwoBeforeOneFromTwo, solver,
                     true) {
  IntervalVar* x = solver->MakeFixedDurationIntervalVar(0, 10, 4, false, "x");
  IntervalVar* y = solver->MakeFixedDurationIntervalVar(0, 10, 5, false, "y");
  solver->AddConstraint(solver->MakeTemporalDisjunction(x, y));
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(10, x->StartMax());
  EXPECT_EQ(0, y->StartMin());
  EXPECT_EQ(10, y->StartMax());
  x->SetStartMin(6);
  EXPECT_EQ(6, x->StartMin());
  EXPECT_EQ(10, x->StartMax());
  EXPECT_EQ(0, y->StartMin());
  EXPECT_EQ(10, y->StartMax());
  x->SetStartMin(8);
  EXPECT_EQ(8, x->StartMin());
  EXPECT_EQ(10, x->StartMax());
  EXPECT_EQ(0, y->StartMin());
  EXPECT_EQ(5, y->StartMax());
  return nullptr;
};

TEST_IN_PROPAGATION0(TemporalDisjunctionTest, ForceTwoBeforeOneFromOneAlt,
                     solver, true) {
  IntervalVar* x = solver->MakeFixedDurationIntervalVar(0, 10, 4, false, "x");
  IntervalVar* y = solver->MakeFixedDurationIntervalVar(0, 10, 5, false, "y");
  IntVar* b = solver->MakeBoolVar("b");
  solver->AddConstraint(solver->MakeTemporalDisjunction(x, y, b));
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(10, x->StartMax());
  EXPECT_EQ(0, y->StartMin());
  EXPECT_EQ(10, y->StartMax());
  y->SetStartMax(4);
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(10, x->StartMax());
  EXPECT_EQ(0, y->StartMin());
  EXPECT_EQ(4, y->StartMax());
  EXPECT_EQ(0, b->Min());
  EXPECT_EQ(1, b->Max());
  y->SetStartMax(3);
  EXPECT_EQ(5, x->StartMin());
  EXPECT_EQ(10, x->StartMax());
  EXPECT_EQ(0, y->StartMin());
  EXPECT_EQ(3, y->StartMax());
  EXPECT_EQ(1, b->Min());
  EXPECT_EQ(1, b->Max());
  return nullptr;
};

TEST_IN_PROPAGATION0(TemporalDisjunctionTest, ForceTwoBeforeOneFromTwoAlt,
                     solver, true) {
  IntervalVar* x = solver->MakeFixedDurationIntervalVar(0, 10, 4, false, "x");
  IntervalVar* y = solver->MakeFixedDurationIntervalVar(0, 10, 5, false, "y");
  IntVar* b = solver->MakeBoolVar("b");
  solver->AddConstraint(solver->MakeTemporalDisjunction(x, y, b));
  EXPECT_EQ(0, x->StartMin());
  EXPECT_EQ(10, x->StartMax());
  EXPECT_EQ(0, y->StartMin());
  EXPECT_EQ(10, y->StartMax());
  x->SetStartMin(6);
  EXPECT_EQ(6, x->StartMin());
  EXPECT_EQ(10, x->StartMax());
  EXPECT_EQ(0, y->StartMin());
  EXPECT_EQ(10, y->StartMax());
  EXPECT_EQ(0, b->Min());
  EXPECT_EQ(1, b->Max());
  x->SetStartMin(8);
  EXPECT_EQ(8, x->StartMin());
  EXPECT_EQ(10, x->StartMax());
  EXPECT_EQ(0, y->StartMin());
  EXPECT_EQ(5, y->StartMax());
  EXPECT_EQ(1, b->Min());
  EXPECT_EQ(1, b->Max());
  return nullptr;
};

}  // namespace
}  // namespace operations_research
