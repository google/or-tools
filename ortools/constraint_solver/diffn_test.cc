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

#include <cstdint>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solver_test_util.h"

namespace operations_research {
namespace {
TEST(DiffnTest, BasicApiInt64) {
  Solver s("DiffnTest");
  std::vector<IntVar*> x;
  s.MakeIntVarArray(10, 0, 100, "x", &x);
  std::vector<IntVar*> y;
  s.MakeIntVarArray(10, 0, 100, "y", &y);
  std::vector<int64_t> dx = {1, 3, 5, 7, 2, 4, 6, 8, 6, 3};
  std::vector<int64_t> dy = {3, 5, 7, 2, 4, 6, 8, 6, 3, 10};
  Constraint* const ct = s.MakeNonOverlappingBoxesConstraint(x, y, dx, dy);
  EXPECT_FALSE(ct->DebugString().empty());
}

TEST(DiffnTest, BasicApiInt) {
  Solver s("DiffnTest");
  std::vector<IntVar*> x;
  s.MakeIntVarArray(10, 0, 100, "x", &x);
  std::vector<IntVar*> y;
  s.MakeIntVarArray(10, 0, 100, "y", &y);
  std::vector<int> dx = {1, 3, 5, 7, 2, 4, 6, 8, 6, 3};
  std::vector<int> dy = {3, 5, 7, 2, 4, 6, 8, 6, 3, 10};
  Constraint* const ct = s.MakeNonOverlappingBoxesConstraint(x, y, dx, dy);
  EXPECT_FALSE(ct->DebugString().empty());
}

TEST_IN_PROPAGATION(DiffnTest, NoSpaceY, solver, false) {
  Constraint* ct = nullptr;
  {
    std::vector<IntVar*> x;
    solver->MakeIntVarArray(3, 0, 0, "x", &x);
    std::vector<IntVar*> y;
    solver->MakeIntVarArray(3, 0, 8, "y", &y);
    std::vector<int> dx = {2, 2, 2};
    std::vector<int> dy = {5, 5, 5};
    ct = solver->MakeNonOverlappingBoxesConstraint(x, y, dx, dy);
  }
  solver->AddConstraint(ct);
  return nullptr;
}

TEST_IN_PROPAGATION(DiffnTest, NoSpaceX, solver, false) {
  Constraint* ct = nullptr;
  {
    std::vector<IntVar*> x;
    solver->MakeIntVarArray(3, 0, 8, "x", &x);
    std::vector<IntVar*> y;
    solver->MakeIntVarArray(3, 0, 0, "y", &y);
    std::vector<int> dx = {5, 5, 5};
    std::vector<int> dy = {2, 2, 2};
    ct = solver->MakeNonOverlappingBoxesConstraint(x, y, dx, dy);
  }
  solver->AddConstraint(ct);
  return nullptr;
}

TEST_IN_PROPAGATION(DiffnTest, NoSpaceXDelayedBefore, solver, true) {
  Constraint* ct = nullptr;
  {
    std::vector<IntVar*> x;
    solver->MakeIntVarArray(3, 0, 8, "x", &x);
    std::vector<IntVar*> y;
    solver->MakeIntVarArray(3, 0, 0, "y", &y);
    std::vector<IntVar*> dx;
    solver->MakeIntVarArray(3, 4, 5, "dx", &dx);
    std::vector<IntVar*> dy;
    solver->MakeIntVarArray(3, 2, 2, "dy", &dy);
    ct = solver->MakeNonOverlappingBoxesConstraint(x, y, dx, dy);
  }
  solver->AddConstraint(ct);
  return nullptr;
}

TEST_IN_PROPAGATION(DiffnTest, NoSpaceXDelayedAfter, solver, false) {
  Constraint* ct = nullptr;
  IntVar* dx0 = nullptr;
  IntVar* dx1 = nullptr;
  {
    std::vector<IntVar*> x;
    solver->MakeIntVarArray(3, 0, 8, "x", &x);
    std::vector<IntVar*> y;
    solver->MakeIntVarArray(3, 0, 0, "y", &y);
    std::vector<IntVar*> dx;
    solver->MakeIntVarArray(3, 4, 5, "dx", &dx);
    dx0 = dx[0];
    dx1 = dx[1];
    std::vector<IntVar*> dy;
    solver->MakeIntVarArray(3, 2, 2, "dy", &dy);
    ct = solver->MakeNonOverlappingBoxesConstraint(x, y, dx, dy);
  }
  solver->AddConstraint(ct);
  dx0->SetMin(5);
  dx1->SetMin(5);
  return nullptr;
}

TEST_IN_PROPAGATION(DiffnTest, NoSpaceYDelayedBefore, solver, true) {
  Constraint* ct = nullptr;
  {
    std::vector<IntVar*> x;
    solver->MakeIntVarArray(3, 0, 0, "x", &x);
    std::vector<IntVar*> y;
    solver->MakeIntVarArray(3, 0, 8, "y", &y);
    std::vector<IntVar*> dx;
    solver->MakeIntVarArray(3, 2, 2, "dx", &dx);
    std::vector<IntVar*> dy;
    solver->MakeIntVarArray(3, 4, 5, "dy", &dy);
    ct = solver->MakeNonOverlappingBoxesConstraint(x, y, dx, dy);
  }
  solver->AddConstraint(ct);
  return nullptr;
}

TEST_IN_PROPAGATION(DiffnTest, NoSpaceYDelayedAfter, solver, false) {
  Constraint* ct = nullptr;
  IntVar* dy0 = nullptr;
  IntVar* dy1 = nullptr;
  {
    std::vector<IntVar*> x;
    solver->MakeIntVarArray(3, 0, 0, "x", &x);
    std::vector<IntVar*> y;
    solver->MakeIntVarArray(3, 0, 8, "y", &y);
    std::vector<IntVar*> dx;
    solver->MakeIntVarArray(3, 2, 2, "dx", &dx);
    std::vector<IntVar*> dy;
    solver->MakeIntVarArray(3, 4, 5, "dy", &dy);
    dy0 = dy[0];
    dy1 = dy[1];
    ct = solver->MakeNonOverlappingBoxesConstraint(x, y, dx, dy);
  }
  solver->AddConstraint(ct);
  dy0->SetMin(5);
  dy1->SetMin(5);
  return nullptr;
}

TEST_IN_PROPAGATION(DiffnTest, PushRight, solver, true) {
  std::vector<IntVar*> x;
  solver->MakeIntVarArray(2, 0, 5, "x", &x);
  std::vector<IntVar*> y;
  solver->MakeIntVarArray(2, 0, 3, "y", &y);
  std::vector<int> dx = {4, 4};
  std::vector<int> dy = {5, 5};
  Constraint* const ct =
      solver->MakeNonOverlappingBoxesConstraint(x, y, dx, dy);
  solver->AddConstraint(ct);
  // No modifications
  EXPECT_EQ_RANGE(x[0], 0, 5);
  EXPECT_EQ_RANGE(y[0], 0, 3);
  EXPECT_EQ_RANGE(x[1], 0, 5);
  EXPECT_EQ_RANGE(y[1], 0, 3);
  x[0]->SetMax(2);
  EXPECT_EQ_RANGE(x[0], 0, 1);
  EXPECT_EQ_RANGE(y[0], 0, 3);
  EXPECT_EQ_RANGE(x[1], 4, 5);
  EXPECT_EQ_RANGE(y[1], 0, 3);
  return nullptr;
}

TEST_IN_PROPAGATION(DiffnTest, PushRightDelayed, solver, true) {
  std::vector<IntVar*> x;
  solver->MakeIntVarArray(2, 0, 5, "x", &x);
  std::vector<IntVar*> y;
  solver->MakeIntVarArray(2, 0, 5, "y", &y);
  std::vector<int> dx = {4, 4};
  std::vector<int> dy = {5, 5};
  Constraint* const ct =
      solver->MakeNonOverlappingBoxesConstraint(x, y, dx, dy);
  solver->AddConstraint(ct);
  // No modifications
  EXPECT_EQ_RANGE(x[0], 0, 5);
  EXPECT_EQ_RANGE(y[0], 0, 5);
  EXPECT_EQ_RANGE(x[1], 0, 5);
  EXPECT_EQ_RANGE(y[1], 0, 5);
  x[0]->SetMax(2);
  // No modifications
  EXPECT_EQ_RANGE(x[0], 0, 2);
  EXPECT_EQ_RANGE(y[0], 0, 5);
  EXPECT_EQ_RANGE(x[1], 0, 5);
  EXPECT_EQ_RANGE(y[1], 0, 5);
  // Force overlap.
  y[0]->SetMax(4);
  y[1]->SetMax(1);
  EXPECT_EQ_RANGE(x[0], 0, 1);
  EXPECT_EQ_RANGE(y[0], 0, 4);
  EXPECT_EQ_RANGE(x[1], 4, 5);
  EXPECT_EQ_RANGE(y[1], 0, 1);
  return nullptr;
}

TEST_IN_PROPAGATION(DiffnTest, PushLeft, solver, true) {
  std::vector<IntVar*> x;
  solver->MakeIntVarArray(2, 0, 5, "x", &x);
  std::vector<IntVar*> y;
  solver->MakeIntVarArray(2, 0, 3, "y", &y);
  std::vector<int> dx = {4, 4};
  std::vector<int> dy = {5, 5};
  Constraint* const ct =
      solver->MakeNonOverlappingBoxesConstraint(x, y, dx, dy);
  solver->AddConstraint(ct);
  // No modifications
  EXPECT_EQ_RANGE(x[0], 0, 5);
  EXPECT_EQ_RANGE(x[1], 0, 5);
  x[0]->SetMin(2);
  EXPECT_EQ_RANGE(x[0], 4, 5);
  EXPECT_EQ_RANGE(x[1], 0, 1);
  return nullptr;
}

TEST_IN_PROPAGATION(DiffnTest, PushLeftDelayed, solver, true) {
  std::vector<IntVar*> x;
  solver->MakeIntVarArray(2, 0, 5, "x", &x);
  std::vector<IntVar*> y;
  solver->MakeIntVarArray(2, 0, 5, "y", &y);
  std::vector<int> dx = {4, 4};
  std::vector<int> dy = {5, 5};
  Constraint* const ct =
      solver->MakeNonOverlappingBoxesConstraint(x, y, dx, dy);
  solver->AddConstraint(ct);
  // No modifications
  EXPECT_EQ_RANGE(x[0], 0, 5);
  EXPECT_EQ_RANGE(y[0], 0, 5);
  EXPECT_EQ_RANGE(x[1], 0, 5);
  EXPECT_EQ_RANGE(y[1], 0, 5);
  x[0]->SetMin(2);
  // No modifications
  EXPECT_EQ_RANGE(x[0], 2, 5);
  EXPECT_EQ_RANGE(y[0], 0, 5);
  EXPECT_EQ_RANGE(x[1], 0, 5);
  EXPECT_EQ_RANGE(y[1], 0, 5);
  // Force overlap.
  y[0]->SetMax(4);
  y[1]->SetMax(1);
  EXPECT_EQ_RANGE(x[0], 4, 5);
  EXPECT_EQ_RANGE(y[0], 0, 4);
  EXPECT_EQ_RANGE(x[1], 0, 1);
  EXPECT_EQ_RANGE(y[1], 0, 1);
  return nullptr;
}

TEST_IN_PROPAGATION(DiffnTest, PushUp, solver, true) {
  std::vector<IntVar*> x;
  solver->MakeIntVarArray(2, 0, 0, "x", &x);
  std::vector<IntVar*> y;
  solver->MakeIntVarArray(2, 0, 5, "y", &y);
  std::vector<int> dx = {5, 5};
  std::vector<int> dy = {4, 4};
  Constraint* const ct =
      solver->MakeNonOverlappingBoxesConstraint(y, x, dy, dx);
  solver->AddConstraint(ct);
  // No modifications
  EXPECT_EQ_RANGE(y[0], 0, 5);
  EXPECT_EQ_RANGE(y[1], 0, 5);
  y[0]->SetMax(2);
  EXPECT_EQ_RANGE(y[0], 0, 1);
  EXPECT_EQ_RANGE(y[1], 4, 5);
  return nullptr;
}

TEST_IN_PROPAGATION(DiffnTest, PushUpDelayed, solver, true) {
  std::vector<IntVar*> x;
  solver->MakeIntVarArray(2, 0, 5, "x", &x);
  std::vector<IntVar*> y;
  solver->MakeIntVarArray(2, 0, 5, "y", &y);
  std::vector<int> dx = {5, 5};
  std::vector<int> dy = {4, 4};
  Constraint* const ct =
      solver->MakeNonOverlappingBoxesConstraint(y, x, dy, dx);
  solver->AddConstraint(ct);
  // No modifications
  EXPECT_EQ_RANGE(x[0], 0, 5);
  EXPECT_EQ_RANGE(y[0], 0, 5);
  EXPECT_EQ_RANGE(x[1], 0, 5);
  EXPECT_EQ_RANGE(y[1], 0, 5);
  // No effect.
  y[0]->SetMax(2);
  EXPECT_EQ_RANGE(x[0], 0, 5);
  EXPECT_EQ_RANGE(y[0], 0, 2);
  EXPECT_EQ_RANGE(x[1], 0, 5);
  EXPECT_EQ_RANGE(y[1], 0, 5);
  // Force overlap.
  x[0]->SetMin(2);
  x[1]->SetMin(3);
  EXPECT_EQ_RANGE(x[0], 2, 5);
  EXPECT_EQ_RANGE(y[0], 0, 1);
  EXPECT_EQ_RANGE(x[1], 3, 5);
  EXPECT_EQ_RANGE(y[1], 4, 5);
  return nullptr;
}

TEST_IN_PROPAGATION(DiffnTest, PushDown, solver, true) {
  std::vector<IntVar*> x;
  solver->MakeIntVarArray(2, 0, 0, "x", &x);
  std::vector<IntVar*> y;
  solver->MakeIntVarArray(2, 0, 5, "y", &y);
  std::vector<int> dx = {5, 5};
  std::vector<int> dy = {4, 4};
  Constraint* const ct =
      solver->MakeNonOverlappingBoxesConstraint(y, x, dy, dx);
  solver->AddConstraint(ct);
  // No modifications
  EXPECT_EQ_RANGE(y[0], 0, 5);
  EXPECT_EQ_RANGE(y[1], 0, 5);
  y[0]->SetMin(2);
  EXPECT_EQ_RANGE(y[0], 4, 5);
  EXPECT_EQ_RANGE(y[1], 0, 1);
  return nullptr;
}

TEST_IN_PROPAGATION(DiffnTest, PushDownDelayed, solver, true) {
  std::vector<IntVar*> x;
  solver->MakeIntVarArray(2, 0, 5, "x", &x);
  std::vector<IntVar*> y;
  solver->MakeIntVarArray(2, 0, 5, "y", &y);
  std::vector<int> dx = {5, 5};
  std::vector<int> dy = {4, 4};
  Constraint* const ct =
      solver->MakeNonOverlappingBoxesConstraint(y, x, dy, dx);
  solver->AddConstraint(ct);
  // No modifications
  EXPECT_EQ_RANGE(x[0], 0, 5);
  EXPECT_EQ_RANGE(y[0], 0, 5);
  EXPECT_EQ_RANGE(x[1], 0, 5);
  EXPECT_EQ_RANGE(y[1], 0, 5);
  // No effect.
  y[0]->SetMin(2);
  EXPECT_EQ_RANGE(x[0], 0, 5);
  EXPECT_EQ_RANGE(y[0], 2, 5);
  EXPECT_EQ_RANGE(x[1], 0, 5);
  EXPECT_EQ_RANGE(y[1], 0, 5);
  // Force overlap.
  x[0]->SetMax(2);
  x[1]->SetMax(3);
  EXPECT_EQ_RANGE(x[0], 0, 2);
  EXPECT_EQ_RANGE(y[0], 4, 5);
  EXPECT_EQ_RANGE(x[1], 0, 3);
  EXPECT_EQ_RANGE(y[1], 0, 1);
  return nullptr;
}
}  // namespace
}  // namespace operations_research
