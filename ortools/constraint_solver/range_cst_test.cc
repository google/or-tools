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

#include "absl/log/log.h"
#include "gtest/gtest.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solver_test_util.h"

namespace operations_research {
namespace {

class RangeCstTest : public testing::Test {
 protected:
  RangeCstTest() : solver_(new Solver("RangeCstTest")) {}
  std::unique_ptr<Solver> solver_;
};

TEST_F(RangeCstTest, Display) {
  Solver* const s = solver_.get();
  IntVar* v0 = s->MakeIntVar(0, 2, "v0");
  IntVar* v1 = s->MakeIntVar(0, 2, "v1");
  LOG(INFO) << "Please ignore: " << s->MakeEquality(v0, v1);
  LOG(INFO) << "Please ignore: " << s->MakeNonEquality(v0, v1);
  LOG(INFO) << "Please ignore: " << s->MakeGreaterOrEqual(v0, v1);
  LOG(INFO) << "Please ignore: " << s->MakeGreater(v0, v1);
  LOG(INFO) << "Please ignore: " << s->MakeLessOrEqual(v0, v1);
  LOG(INFO) << "Please ignore: " << s->MakeLess(v0, v1);
}

class RangeEqualityTest : public testing::Test {
 protected:
  RangeEqualityTest() : solver_(new Solver("RangeEqualityTest")) {}
  std::unique_ptr<Solver> solver_;
};

TEST_IN_PROPAGATION0(RangeEqualityTest, SetMinLeft, solver, true) {
  IntVar* v0 = solver->MakeIntVar(0, 2, "v0");
  IntVar* v1 = solver->MakeIntVar(0, 2, "v1");
  Constraint* ct = solver->MakeEquality(v0, v1);
  solver->AddConstraint(ct);
  v0->SetMin(1);
  EXPECT_EQ(1, v1->Min());
  return nullptr;
};

TEST_IN_PROPAGATION0(RangeEqualityTest, SetMinRight, solver, true) {
  IntVar* v0 = solver->MakeIntVar(0, 2, "v0");
  IntVar* v1 = solver->MakeIntVar(0, 2, "v1");
  Constraint* ct = solver->MakeEquality(v0, v1);
  solver->AddConstraint(ct);
  v1->SetMin(1);
  EXPECT_EQ(1, v0->Min());
  return nullptr;
};

TEST_IN_PROPAGATION0(RangeEqualityTest, SetMaxLeft, solver, true) {
  IntVar* v0 = solver->MakeIntVar(0, 2, "v0");
  IntVar* v1 = solver->MakeIntVar(0, 2, "v1");
  Constraint* ct = solver->MakeEquality(v0, v1);
  solver->AddConstraint(ct);
  v0->SetMax(1);
  EXPECT_EQ(1, v1->Max());
  return nullptr;
};

TEST_IN_PROPAGATION0(RangeEqualityTest, SetMaxRight, solver, true) {
  IntVar* v0 = solver->MakeIntVar(0, 2, "v0");
  IntVar* v1 = solver->MakeIntVar(0, 2, "v1");
  Constraint* ct = solver->MakeEquality(v0, v1);
  solver->AddConstraint(ct);
  v1->SetMax(1);
  EXPECT_EQ(1, v0->Max());
  return nullptr;
};

class RangeLessOrEqualTest : public testing::Test {
 protected:
  RangeLessOrEqualTest() : solver_(new Solver("RangeLessOrEqualTest")) {}
  std::unique_ptr<Solver> solver_;
};

TEST_IN_PROPAGATION0(RangeLessOrEqualTest, SetMinLeft, solver, true) {
  IntVar* v0 = solver->MakeIntVar(0, 2, "v0");
  IntVar* v1 = solver->MakeIntVar(0, 2, "v1");
  Constraint* ct = solver->MakeLessOrEqual(v0, v1);
  solver->AddConstraint(ct);
  v0->SetMin(1);
  EXPECT_EQ(1, v1->Min());
  return nullptr;
};

TEST_IN_PROPAGATION0(RangeLessOrEqualTest, SetMinRight, solver, true) {
  IntVar* v0 = solver->MakeIntVar(0, 2, "v0");
  IntVar* v1 = solver->MakeIntVar(0, 2, "v1");
  Constraint* ct = solver->MakeLessOrEqual(v0, v1);
  solver->AddConstraint(ct);
  v1->SetMin(1);
  EXPECT_EQ(0, v0->Min());
  return nullptr;
};

TEST_IN_PROPAGATION0(RangeLessOrEqualTest, SetMaxLeft, solver, true) {
  IntVar* v0 = solver->MakeIntVar(0, 2, "v0");
  IntVar* v1 = solver->MakeIntVar(0, 2, "v1");
  Constraint* ct = solver->MakeLessOrEqual(v0, v1);
  solver->AddConstraint(ct);
  v0->SetMax(1);
  EXPECT_EQ(2, v1->Max());
  return nullptr;
};

TEST_IN_PROPAGATION0(RangeLessOrEqualTest, SetMaxRight, solver, true) {
  IntVar* v0 = solver->MakeIntVar(0, 2, "v0");
  IntVar* v1 = solver->MakeIntVar(0, 2, "v1");
  Constraint* ct = solver->MakeLessOrEqual(v0, v1);
  solver->AddConstraint(ct);
  v1->SetMax(1);
  EXPECT_EQ(1, v0->Max());
  return nullptr;
};

class RangeLessTest : public testing::Test {
 protected:
  RangeLessTest() : solver_(new Solver("RangeLessTest")) {}
  std::unique_ptr<Solver> solver_;
};

TEST_IN_PROPAGATION0(RangeLessTest, SetMinLeft, solver, true) {
  IntVar* v0 = solver->MakeIntVar(0, 2, "v0");
  IntVar* v1 = solver->MakeIntVar(0, 2, "v1");
  Constraint* ct = solver->MakeLess(v0, v1);
  solver->AddConstraint(ct);
  v0->SetMin(1);
  EXPECT_EQ(2, v1->Min());
  return nullptr;
};

TEST_IN_PROPAGATION0(RangeLessTest, SetMinRight, solver, true) {
  IntVar* v0 = solver->MakeIntVar(0, 2, "v0");
  IntVar* v1 = solver->MakeIntVar(0, 2, "v1");
  Constraint* ct = solver->MakeLess(v0, v1);
  solver->AddConstraint(ct);
  v1->SetMin(2);
  EXPECT_EQ(0, v0->Min());
  return nullptr;
};

TEST_IN_PROPAGATION0(RangeLessTest, SetMaxLeft, solver, true) {
  IntVar* v0 = solver->MakeIntVar(0, 2, "v0");
  IntVar* v1 = solver->MakeIntVar(0, 2, "v1");
  Constraint* ct = solver->MakeLess(v0, v1);
  solver->AddConstraint(ct);
  v0->SetMax(1);
  EXPECT_EQ(2, v1->Max());
  return nullptr;
};

TEST_IN_PROPAGATION0(RangeLessTest, SetMaxRight, solver, true) {
  IntVar* v0 = solver->MakeIntVar(0, 2, "v0");
  IntVar* v1 = solver->MakeIntVar(0, 2, "v1");
  Constraint* ct = solver->MakeLess(v0, v1);
  solver->AddConstraint(ct);
  v1->SetMax(1);
  EXPECT_EQ(0, v0->Value());
  return nullptr;
};

TEST_IN_PROPAGATION0(RangeLessTest, RightAtMin, solver, false) {
  IntVar* v0 = solver->MakeIntVar(kint64min, kint64max, "v0");
  IntVar* v1 = solver->MakeIntVar(kint64min, kint64min, "v1");
  Constraint* ct = solver->MakeLess(v0, v1);
  solver->AddConstraint(ct);
  return nullptr;
};

TEST_IN_PROPAGATION0(RangeLessTest, LeftAtMax, solver, false) {
  IntVar* v0 = solver->MakeIntVar(kint64max, kint64max, "v0");
  IntVar* v1 = solver->MakeIntVar(kint64min, kint64max, "v1");
  Constraint* ct = solver->MakeLess(v0, v1);
  solver->AddConstraint(ct);
  return nullptr;
};

// Greater

class RangeGreaterOrEqualTest : public testing::Test {
 protected:
  RangeGreaterOrEqualTest() : solver_(new Solver("RangeGreaterOrEqualTest")) {}
  std::unique_ptr<Solver> solver_;
};

TEST_IN_PROPAGATION0(RangeGreaterOrEqualTest, SetMinLeft, solver, true) {
  IntVar* v0 = solver->MakeIntVar(0, 2, "v0");
  IntVar* v1 = solver->MakeIntVar(0, 2, "v1");
  Constraint* ct = solver->MakeGreaterOrEqual(v0, v1);
  solver->AddConstraint(ct);
  v0->SetMin(1);
  EXPECT_EQ(0, v1->Min());
  return nullptr;
};

TEST_IN_PROPAGATION0(RangeGreaterOrEqualTest, SetMinRight, solver, true) {
  IntVar* v0 = solver->MakeIntVar(0, 2, "v0");
  IntVar* v1 = solver->MakeIntVar(0, 2, "v1");
  Constraint* ct = solver->MakeGreaterOrEqual(v0, v1);
  solver->AddConstraint(ct);
  v1->SetMin(1);
  EXPECT_EQ(1, v0->Min());
  return nullptr;
};

TEST_IN_PROPAGATION0(RangeGreaterOrEqualTest, SetMaxLeft, solver, true) {
  IntVar* v0 = solver->MakeIntVar(0, 2, "v0");
  IntVar* v1 = solver->MakeIntVar(0, 2, "v1");
  Constraint* ct = solver->MakeGreaterOrEqual(v0, v1);
  solver->AddConstraint(ct);
  v0->SetMax(1);
  EXPECT_EQ(1, v1->Max());
  return nullptr;
};

TEST_IN_PROPAGATION0(RangeGreaterOrEqualTest, SetMaxRight, solver, true) {
  IntVar* v0 = solver->MakeIntVar(0, 2, "v0");
  IntVar* v1 = solver->MakeIntVar(0, 2, "v1");
  Constraint* ct = solver->MakeGreaterOrEqual(v0, v1);
  solver->AddConstraint(ct);
  v1->SetMax(1);
  EXPECT_EQ(2, v0->Max());
  return nullptr;
};

class RangeGreaterTest : public testing::Test {
 protected:
  RangeGreaterTest() : solver_(new Solver("RangeGreaterTest")) {}
  std::unique_ptr<Solver> solver_;
};

TEST_IN_PROPAGATION0(RangeGreaterTest, SetMinLeft, solver, true) {
  IntVar* v0 = solver->MakeIntVar(0, 2, "v0");
  IntVar* v1 = solver->MakeIntVar(0, 2, "v1");
  Constraint* ct = solver->MakeGreater(v0, v1);
  solver->AddConstraint(ct);
  v0->SetMin(1);
  EXPECT_EQ(0, v1->Min());
  return nullptr;
};

TEST_IN_PROPAGATION0(RangeGreaterTest, SetMinRight, solver, true) {
  IntVar* v0 = solver->MakeIntVar(0, 2, "v0");
  IntVar* v1 = solver->MakeIntVar(0, 2, "v1");
  Constraint* ct = solver->MakeGreater(v0, v1);
  solver->AddConstraint(ct);
  v1->SetMin(1);
  EXPECT_EQ(2, v0->Value());
  return nullptr;
};

TEST_IN_PROPAGATION0(RangeGreaterTest, SetMaxLeft, solver, true) {
  IntVar* v0 = solver->MakeIntVar(0, 2, "v0");
  IntVar* v1 = solver->MakeIntVar(0, 2, "v1");
  Constraint* ct = solver->MakeGreater(v0, v1);
  solver->AddConstraint(ct);
  v0->SetMax(1);
  EXPECT_EQ(0, v1->Max());
  return nullptr;
};

TEST_IN_PROPAGATION0(RangeGreaterTest, SetMaxRight, solver, true) {
  IntVar* v0 = solver->MakeIntVar(0, 2, "v0");
  IntVar* v1 = solver->MakeIntVar(0, 2, "v1");
  Constraint* ct = solver->MakeGreater(v0, v1);
  solver->AddConstraint(ct);
  v1->SetMax(1);
  EXPECT_EQ(2, v0->Max());
  return nullptr;
};

}  // namespace
}  // namespace operations_research
