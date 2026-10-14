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

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "gtest/gtest.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solver_test_util.h"

namespace operations_research {
namespace {
// NoCycle constraint
// TODO(user): add a test for the case with bound variable values taken as
// support.

TEST(NoCycleTest, NoCycleDebugString) {
  Solver solver("NoCycleDebugStringTest");
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(5, 1, 5, "vars", &vars);
  std::vector<IntVar*> active;
  solver.MakeIntVarArray(5, 1, 1, "active", &active);
  EXPECT_FALSE(solver.MakeNoCycle(vars, active)->DebugString().empty());
}

TEST_IN_PROPAGATION(NoCycleTest, EmptyNoCycle, s, true) {
  std::vector<IntVar*> vars;
  std::vector<IntVar*> active;
  s->AddConstraint(s->MakeNoCycle(vars, active));
  return nullptr;
};

TEST_IN_PROPAGATION(NoCycleTest, NoCycle, s, true) {
  std::vector<IntVar*> vars;
  s->MakeIntVarArray(5, 1, 5, "vars", &vars);
  std::vector<IntVar*> active;
  s->MakeIntVarArray(5, 1, 1, "active", &active);
  s->AddConstraint(s->MakeNoCycle(vars, active));
  EXPECT_EQ(1, vars[0]->Min());
  EXPECT_EQ(1, vars[1]->Min());
  EXPECT_EQ(1, vars[2]->Min());
  EXPECT_EQ(1, vars[3]->Min());
  EXPECT_EQ(1, vars[4]->Min());
  EXPECT_EQ(5, vars[0]->Max());
  EXPECT_EQ(5, vars[1]->Max());
  EXPECT_EQ(5, vars[2]->Max());
  EXPECT_EQ(5, vars[3]->Max());
  EXPECT_EQ(5, vars[4]->Max());
  vars[1]->SetValue(2);
  EXPECT_FALSE(vars[2]->Contains(1));
  vars[3]->SetValue(4);
  EXPECT_FALSE(vars[4]->Contains(3));
  vars[2]->SetValue(3);
  EXPECT_FALSE(vars[4]->Contains(1));
  vars[4]->SetValue(5);
  vars[0]->SetValue(1);
  return nullptr;
}

TEST_IN_PROPAGATION(NoCycleWithSinkNotAnEscapeValueTest, NoCycle, s, true) {
  std::vector<IntVar*> vars;
  s->MakeIntVarArray(5, 0, 5, "vars", &vars);
  std::vector<IntVar*> active;
  s->MakeIntVarArray(5, 0, 1, "active", &active);
  s->AddConstraint(
      s->MakeNoCycle(vars, active, [](int64_t x) { return x == 1; }));
  EXPECT_EQ(0, vars[0]->Min());
  EXPECT_EQ(0, vars[1]->Min());
  EXPECT_EQ(0, vars[2]->Min());
  EXPECT_EQ(0, vars[3]->Min());
  EXPECT_EQ(0, vars[4]->Min());
  EXPECT_EQ(4, vars[0]->Max());
  EXPECT_EQ(4, vars[1]->Max());
  EXPECT_EQ(4, vars[2]->Max());
  EXPECT_EQ(4, vars[3]->Max());
  EXPECT_EQ(4, vars[4]->Max());
  vars[3]->SetValue(1);
  vars[1]->SetValue(0);
  vars[0]->SetValue(2);
  vars[4]->SetValue(3);
  vars[2]->SetValue(4);
  return nullptr;
}

TEST_IN_PROPAGATION(NoCycleTest, InactiveNoCycle, s, true) {
  std::vector<IntVar*> vars;
  s->MakeIntVarArray(2, 1, 2, "vars", &vars);
  std::vector<IntVar*> active;
  s->MakeIntVarArray(2, 0, 0, "active", &active);
  s->AddConstraint(s->MakeNoCycle(vars, active));
  s->AddConstraint(s->MakeAllDifferent(vars, false));
  vars[0]->SetValue(2);
  EXPECT_EQ(2, vars[0]->Min());
  EXPECT_EQ(1, vars[1]->Min());
  EXPECT_EQ(2, vars[0]->Max());
  EXPECT_EQ(1, vars[1]->Max());
  return nullptr;
}

TEST_IN_PROPAGATION(NoCycleTest, NoCycleFail, s, false) {
  IntVar* var = nullptr;
  {
    std::vector<IntVar*> vars;
    std::vector<IntVar*> active;
    s->MakeIntVarArray(2, 1, 2, "vars", &vars);
    s->MakeIntVarArray(2, 1, 1, "active", &active);
    s->AddConstraint(s->MakeNoCycle(vars, active));
    s->AddConstraint(s->MakeAllDifferent(vars, false));
    var = vars[0];
  }
  var->SetValue(2);
  return nullptr;
}

// Non-regression test making sure partial path ends are not updated too many
// times, in particular when active and next variables are updated separately.
// Note: this is a tricky test as the issue is only exposed under rare
// circumstances.
TEST_IN_PROPAGATION(NoCycleTest, DetectSubCycle, s, false) {
  IntVar* next0 = s->MakeIntVar(0, 6);
  IntVar* next1 = s->MakeIntVar(0, 6);
  IntVar* next2 = s->MakeIntVar(0, 6);
  IntVar* next3 = s->MakeIntVar(0, 6);
  IntVar* next4 = s->MakeIntVar(0, 6);
  IntVar* next5 = s->MakeIntVar(0, 6);
  IntVar* active0 = s->MakeBoolVar();
  IntVar* active1 = s->MakeBoolVar();
  IntVar* active2 = s->MakeBoolVar();
  IntVar* active3 = s->MakeBoolVar();
  IntVar* active4 = s->MakeBoolVar();
  IntVar* active5 = s->MakeBoolVar();
  s->AddConstraint(
      s->MakeNoCycle({next0, next1, next2, next3, next4, next5},
                     {active0, active1, active2, active3, active4, active5}));
  next1->SetValue(2);  // NO-OP as active1 is not set
  next2->SetValue(3);  // NO-OP as active2 is not set
  // Freezing so that propagation sees the complete partial path, propagation
  // on active and next variables will be called.
  next0->FreezeQueue();
  active1->SetValue(1);  // chain 1-2 detected
  active3->SetValue(1);  // chain 1-4 detected
  active2->SetValue(1);  // chain 1-3 detected
  next3->SetValue(4);    // start of 4 becomes 3 if 3 has not been marked (*bug)
  next0->UnfreezeQueue();
  // Same reason for freezing here while we are building a cycle.
  next0->FreezeQueue();
  active0->SetValue(1);  // chain 0-6 detected
  next0->SetValue(6);    // NO-OPT
  active4->SetValue(1);  // chain 1-5 detected (if *bug, 3-5)
  next4->SetValue(5);    // NO-OPT
  active5->SetValue(1);  // Fail (if *bug, 3-1 detected)
  next5->SetValue(1);    // NO-OPT
  next0->UnfreezeQueue();
  return nullptr;
}

class TestNoCycleGraphFail : public DecisionBuilder {
 public:
  TestNoCycleGraphFail(Solver* const s, int* count) : count_(count) {
    // Array built here to avoid leak when failing in Next()
    s->MakeIntVarArray(4, 1, 4, "vars", &vars_);
    s->MakeIntVarArray(4, 1, 1, "active", &active_);
  }
  Decision* Next(Solver* const s) override {
    s->AddConstraint(s->MakeNoCycle(vars_, active_));
    vars_[1]->SetMax(3);
    ++(*count_);
    vars_[2]->SetMax(3);
    ++(*count_);
    vars_[3]->SetMax(3);
    ++(*count_);
    return nullptr;
  }

 private:
  std::vector<IntVar*> vars_;
  std::vector<IntVar*> active_;
  int* count_;
};

TEST(NoCycleTest, NoCycleGraphFail) {
  std::unique_ptr<Solver> solver(new Solver("NoCycleTest"));
  int count = 0;
  DecisionBuilder* const db =
      solver->RevAlloc(new TestNoCycleGraphFail(solver.get(), &count));
  ASSERT_FALSE(solver->Solve(db));
  ASSERT_EQ(2, count);
}

TEST_IN_PROPAGATION(NoCycleTest, NoCycleTree, s, true) {
  std::vector<IntVar*> vars;
  s->MakeIntVarArray(6, 0, 6, "vars", &vars);
  std::vector<IntVar*> active;
  s->MakeIntVarArray(6, 1, 1, "active", &active);
  s->AddConstraint(s->MakeNoCycle(vars, active, nullptr, false));
  vars[0]->SetValue(2);
  EXPECT_FALSE(vars[2]->Contains(0));
  vars[1]->SetValue(2);
  EXPECT_FALSE(vars[2]->Contains(1));
  vars[2]->SetValue(3);
  EXPECT_FALSE(vars[3]->Contains(0));
  EXPECT_FALSE(vars[3]->Contains(1));
  EXPECT_FALSE(vars[3]->Contains(2));
  EXPECT_TRUE(vars[3]->Contains(4));
  EXPECT_TRUE(vars[3]->Contains(5));
  vars[4]->SetValue(1);
  EXPECT_FALSE(vars[3]->Contains(4));
  return nullptr;
}

TEST_IN_PROPAGATION(NoCycleTest, NoCycleTreeOnPath, s, true) {
  std::vector<IntVar*> vars;
  s->MakeIntVarArray(2, 0, 2, "vars", &vars);
  std::vector<IntVar*> active;
  s->MakeIntVarArray(2, 1, 1, "active", &active);
  s->AddConstraint(s->MakeNoCycle(vars, active, nullptr, false));
  for (int i = 0; i < 2; ++i) {
    vars[i]->SetValue(i + 1);
  }
  return nullptr;
}

TEST_IN_PROPAGATION(NoCycleTest, NoCycleTreeFail, s, false) {
  Constraint* ct = nullptr;
  {
    // Array built here to avoid leak when failing in Next()
    std::vector<IntVar*> vars;
    std::vector<IntVar*> active;
    s->MakeIntVarArray(4, 0, 4, "vars", &vars);
    s->MakeIntVarArray(4, 1, 1, "active", &active);
    vars[0]->SetValue(1);
    vars[2]->SetValue(3);
    vars[3]->SetValue(2);
    ct = s->MakeNoCycle(vars, active, nullptr, false);
  }
  s->AddConstraint(ct);
  return nullptr;
}

// PathCumul constraint

int64_t Distance(int64_t i, int64_t j) { return ((i < j) ? j - i : i - j); }

TEST(PathCumulTest, PathCumulDebugStringWithProfiling) {
  ConstraintSolverParameters param = Solver::DefaultSolverParameters();
  std::unique_ptr<Solver> solver(new Solver("PathCumulDebugStringTest", param));
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(5, 1, 5, "vars", &vars);
  std::vector<IntVar*> active;
  solver->MakeIntVarArray(5, 1, 1, "active", &active);
  std::vector<IntVar*> cumuls;
  solver->MakeIntVarArray(6, 0, 10, "cumuls", &cumuls);
  std::vector<IntVar*> transits;
  for (int i = 0; i < 5; ++i) {
    transits.push_back(
        solver->MakeElement([i](int64_t j) { return Distance(i, j); }, vars[i])
            ->Var());
  }
#ifndef DELAYED_PATH_CUMUL
  Constraint* const ct = solver->MakePathCumul(vars, active, cumuls, transits);
#else
  Constraint* const ct =
      solver->MakeDelayedPathCumul(vars, active, cumuls, transits);
#endif
  EXPECT_FALSE(ct->DebugString().empty());
}

TEST_IN_PROPAGATION(PathCumulTest, EmptyPathCumul, s, true) {
  std::vector<IntVar*> vars;
  std::vector<IntVar*> active;
  std::vector<IntVar*> cumuls;
  std::vector<IntVar*> transits;
#ifndef DELAYED_PATH_CUMUL
  s->AddConstraint(s->MakePathCumul(vars, active, cumuls, transits));
#else
  s->AddConstraint(s->MakeDelayedPathCumul(vars, active, cumuls, transits));
#endif
  return nullptr;
}

class BaseTestPathCumul : public DecisionBuilder {
 public:
  Decision* Next(Solver* const s) override {
    std::vector<IntVar*> vars;
    s->MakeIntVarArray(3, 1, 3, "vars", &vars);
    std::vector<IntVar*> active;
    s->MakeIntVarArray(3, 1, 1, "active", &active);
    std::vector<IntVar*> cumuls;
    s->MakeIntVarArray(4, 0, 10, "cumuls", &cumuls);
    s->AddConstraint(BuildPathCumulConstraint(s, vars, active, cumuls));
    EXPECT_EQ(1, vars[0]->Min());
    EXPECT_EQ(1, vars[1]->Min());
    EXPECT_EQ(1, vars[2]->Min());
    EXPECT_EQ(3, vars[0]->Max());
    EXPECT_EQ(3, vars[1]->Max());
    EXPECT_EQ(3, vars[2]->Max());
    EXPECT_EQ(0, cumuls[0]->Min());
    EXPECT_EQ(0, cumuls[1]->Min());
    EXPECT_EQ(0, cumuls[2]->Min());
    EXPECT_EQ(0, cumuls[3]->Min());
    EXPECT_EQ(10, cumuls[0]->Max());
    EXPECT_EQ(10, cumuls[1]->Max());
    EXPECT_EQ(10, cumuls[2]->Max());
    EXPECT_EQ(10, cumuls[3]->Max());
    vars[1]->SetValue(2);
    EXPECT_EQ(1, vars[0]->Min());
    EXPECT_EQ(2, vars[1]->Min());
    EXPECT_EQ(1, vars[2]->Min());
    EXPECT_EQ(3, vars[0]->Max());
    EXPECT_EQ(2, vars[1]->Max());
    EXPECT_EQ(3, vars[2]->Max());
    EXPECT_EQ(0, cumuls[0]->Min());
    EXPECT_EQ(0, cumuls[1]->Min());
    EXPECT_EQ(1, cumuls[2]->Min());
    EXPECT_EQ(0, cumuls[3]->Min());
    EXPECT_EQ(10, cumuls[0]->Max());
    EXPECT_EQ(9, cumuls[1]->Max());
    EXPECT_EQ(10, cumuls[2]->Max());
    EXPECT_EQ(10, cumuls[3]->Max());
    vars[0]->SetValue(1);
    EXPECT_EQ(1, vars[0]->Min());
    EXPECT_EQ(2, vars[1]->Min());
    EXPECT_EQ(1, vars[2]->Min());
    EXPECT_EQ(1, vars[0]->Max());
    EXPECT_EQ(2, vars[1]->Max());
    EXPECT_EQ(3, vars[2]->Max());
    EXPECT_EQ(0, cumuls[0]->Min());
    EXPECT_EQ(1, cumuls[1]->Min());
    EXPECT_EQ(2, cumuls[2]->Min());
    EXPECT_EQ(0, cumuls[3]->Min());
    EXPECT_EQ(8, cumuls[0]->Max());
    EXPECT_EQ(9, cumuls[1]->Max());
    EXPECT_EQ(10, cumuls[2]->Max());
    EXPECT_EQ(10, cumuls[3]->Max());
    cumuls[2]->SetMax(5);
    EXPECT_EQ(1, vars[0]->Min());
    EXPECT_EQ(2, vars[1]->Min());
    EXPECT_EQ(1, vars[2]->Min());
    EXPECT_EQ(1, vars[0]->Max());
    EXPECT_EQ(2, vars[1]->Max());
    EXPECT_EQ(3, vars[2]->Max());
    EXPECT_EQ(0, cumuls[0]->Min());
    EXPECT_EQ(1, cumuls[1]->Min());
    EXPECT_EQ(2, cumuls[2]->Min());
    EXPECT_EQ(0, cumuls[3]->Min());
    EXPECT_EQ(3, cumuls[0]->Max());
    EXPECT_EQ(4, cumuls[1]->Max());
    EXPECT_EQ(5, cumuls[2]->Max());
    EXPECT_EQ(10, cumuls[3]->Max());
    cumuls[0]->SetMin(2);
    EXPECT_EQ(1, vars[0]->Min());
    EXPECT_EQ(2, vars[1]->Min());
    EXPECT_EQ(1, vars[2]->Min());
    EXPECT_EQ(1, vars[0]->Max());
    EXPECT_EQ(2, vars[1]->Max());
    EXPECT_EQ(3, vars[2]->Max());
    EXPECT_EQ(2, cumuls[0]->Min());
    EXPECT_EQ(3, cumuls[1]->Min());
    EXPECT_EQ(4, cumuls[2]->Min());
    EXPECT_EQ(0, cumuls[3]->Min());
    EXPECT_EQ(3, cumuls[0]->Max());
    EXPECT_EQ(4, cumuls[1]->Max());
    EXPECT_EQ(5, cumuls[2]->Max());
    EXPECT_EQ(10, cumuls[3]->Max());
    cumuls[3]->SetValue(5);
    EXPECT_EQ(1, vars[0]->Min());
    EXPECT_EQ(2, vars[1]->Min());
    EXPECT_EQ(1, vars[2]->Min());
    EXPECT_EQ(1, vars[0]->Max());
    EXPECT_EQ(2, vars[1]->Max());
    EXPECT_EQ(3, vars[2]->Max());
    EXPECT_EQ(2, cumuls[0]->Min());
    EXPECT_EQ(3, cumuls[1]->Min());
    EXPECT_EQ(4, cumuls[2]->Min());
    EXPECT_EQ(5, cumuls[3]->Min());
    EXPECT_EQ(3, cumuls[0]->Max());
    EXPECT_EQ(4, cumuls[1]->Max());
    EXPECT_EQ(5, cumuls[2]->Max());
    EXPECT_EQ(5, cumuls[3]->Max());
    vars[2]->SetValue(3);
    EXPECT_EQ(1, vars[0]->Min());
    EXPECT_EQ(2, vars[1]->Min());
    EXPECT_EQ(3, vars[2]->Min());
    EXPECT_EQ(1, vars[0]->Max());
    EXPECT_EQ(2, vars[1]->Max());
    EXPECT_EQ(3, vars[2]->Max());
    EXPECT_EQ(2, cumuls[0]->Min());
    EXPECT_EQ(3, cumuls[1]->Min());
    EXPECT_EQ(4, cumuls[2]->Min());
    EXPECT_EQ(5, cumuls[3]->Min());
    EXPECT_EQ(2, cumuls[0]->Max());
    EXPECT_EQ(3, cumuls[1]->Max());
    EXPECT_EQ(4, cumuls[2]->Max());
    EXPECT_EQ(5, cumuls[3]->Max());
    return nullptr;
  }

 protected:
  virtual Constraint* BuildPathCumulConstraint(
      Solver* s, const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& active,
      const std::vector<IntVar*>& cumuls) = 0;
};

class TestPathCumul : public BaseTestPathCumul {
 protected:
  Constraint* BuildPathCumulConstraint(
      Solver* const s, const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& active,
      const std::vector<IntVar*>& cumuls) override {
    std::vector<IntVar*> transits;
    for (int i = 0; i < 3; ++i) {
      transits.push_back(
          s->MakeElement([i](int64_t j) { return Distance(i, j); }, vars[i])
              ->Var());
    }
#ifndef DELAYED_PATH_CUMUL
    return s->MakePathCumul(vars, active, cumuls, transits);
#else
    return s->MakeDelayedPathCumul(vars, active, cumuls, transits);
#endif
  }
};

TEST(PathCumulTest, PathCumul) {
  std::unique_ptr<Solver> solver(new Solver("PathCumulTest"));
  DecisionBuilder* const db = solver->RevAlloc(new TestPathCumul());
  ASSERT_TRUE(solver->Solve(db));
}

class TestResultCallback2PathCumul : public BaseTestPathCumul {
 protected:
  Constraint* BuildPathCumulConstraint(
      Solver* const s, const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& active,
      const std::vector<IntVar*>& cumuls) override {
    return s->MakePathCumul(vars, active, cumuls, Distance);
  }
};

TEST(PathCumulTest, ResultCallback2PathCumul) {
  std::unique_ptr<Solver> solver(new Solver("PathCumulTest"));
  DecisionBuilder* const db =
      solver->RevAlloc(new TestResultCallback2PathCumul());
  ASSERT_TRUE(solver->Solve(db));
}

TEST_IN_PROPAGATION(PathCumulTest, TransitPathCumul, s, true) {
  std::vector<IntVar*> vars;
  s->MakeIntVarArray(2, 1, 3, "vars", &vars);
  std::vector<IntVar*> active;
  s->MakeIntVarArray(2, 1, 1, "active", &active);
  std::vector<IntVar*> cumuls;
  s->MakeIntVarArray(3, 0, 10, "cumuls", &cumuls);
  std::vector<IntVar*> transits;
  s->MakeIntVarArray(2, 0, 10, "transits", &transits);
#ifndef DELAYED_PATH_CUMUL
  s->AddConstraint(s->MakePathCumul(vars, active, cumuls, transits));
#else
  s->AddConstraint(s->MakeDelayedPathCumul(vars, active, cumuls, transits));
#endif
  EXPECT_EQ(1, vars[0]->Min());
  EXPECT_EQ(1, vars[1]->Min());
  EXPECT_EQ(3, vars[0]->Max());
  EXPECT_EQ(3, vars[1]->Max());
  EXPECT_EQ(0, cumuls[0]->Min());
  EXPECT_EQ(0, cumuls[1]->Min());
  EXPECT_EQ(0, cumuls[2]->Min());
  EXPECT_EQ(10, cumuls[0]->Max());
  EXPECT_EQ(10, cumuls[1]->Max());
  EXPECT_EQ(10, cumuls[2]->Max());
  EXPECT_EQ(0, transits[0]->Min());
  EXPECT_EQ(0, transits[1]->Min());
  EXPECT_EQ(10, transits[0]->Max());
  EXPECT_EQ(10, transits[1]->Max());
  vars[0]->SetValue(1);
  vars[1]->SetValue(2);
  EXPECT_EQ(0, cumuls[0]->Min());
  EXPECT_EQ(0, cumuls[1]->Min());
  EXPECT_EQ(0, cumuls[2]->Min());
  EXPECT_EQ(10, cumuls[0]->Max());
  EXPECT_EQ(10, cumuls[1]->Max());
  EXPECT_EQ(10, cumuls[2]->Max());
  EXPECT_EQ(0, transits[0]->Min());
  EXPECT_EQ(0, transits[1]->Min());
  EXPECT_EQ(10, transits[0]->Max());
  EXPECT_EQ(10, transits[1]->Max());
  transits[1]->SetMin(1);
  EXPECT_EQ(0, cumuls[0]->Min());
  EXPECT_EQ(0, cumuls[1]->Min());
  EXPECT_EQ(1, cumuls[2]->Min());
  EXPECT_EQ(9, cumuls[0]->Max());
  EXPECT_EQ(9, cumuls[1]->Max());
  EXPECT_EQ(10, cumuls[2]->Max());
  EXPECT_EQ(0, transits[0]->Min());
  EXPECT_EQ(1, transits[1]->Min());
  EXPECT_EQ(9, transits[0]->Max());
  EXPECT_EQ(10, transits[1]->Max());
  return nullptr;
}

TEST_IN_PROPAGATION(PathCumulTest, TransitSlackPathCumul, s, true) {
  std::vector<IntVar*> vars;
  s->MakeIntVarArray(3, 0, 3, "vars", &vars);
  std::vector<IntVar*> active;
  s->MakeIntVarArray(3, 0, 1, "active", &active);
  std::vector<IntVar*> cumuls;
  s->MakeIntVarArray(4, 0, 20, "cumuls", &cumuls);
  std::vector<IntVar*> slacks;
  s->MakeIntVarArray(3, 3, 5, "slacks", &slacks);
  s->AddConstraint(s->MakePathCumul(vars, active, cumuls, slacks, Distance));
  EXPECT_EQ_RANGE(vars[0], 0, 3);
  EXPECT_EQ_RANGE(vars[1], 0, 3);
  EXPECT_EQ_RANGE(vars[2], 0, 3);
  EXPECT_EQ_RANGE(cumuls[0], 0, 20);
  EXPECT_EQ_RANGE(cumuls[1], 0, 20);
  EXPECT_EQ_RANGE(cumuls[2], 0, 20);
  EXPECT_EQ_RANGE(cumuls[3], 0, 20);
  EXPECT_EQ_RANGE(slacks[0], 3, 5);
  EXPECT_EQ_RANGE(slacks[1], 3, 5);
  EXPECT_EQ_RANGE(slacks[2], 3, 5);
  vars[0]->SetValue(2);
  active[0]->SetValue(1);
  EXPECT_EQ_RANGE(vars[0], 2, 2);
  EXPECT_EQ_RANGE(vars[1], 0, 3);
  EXPECT_EQ_RANGE(vars[2], 0, 3);
  EXPECT_EQ_RANGE(cumuls[0], 0, 15);
  EXPECT_EQ_RANGE(cumuls[1], 0, 20);
  EXPECT_EQ_RANGE(cumuls[2], 5, 20);
  EXPECT_EQ_RANGE(cumuls[3], 0, 20);
  EXPECT_EQ_RANGE(slacks[0], 3, 5);
  EXPECT_EQ_RANGE(slacks[1], 3, 5);
  EXPECT_EQ_RANGE(slacks[2], 3, 5);
  vars[2]->SetValue(3);
  active[2]->SetValue(1);
  EXPECT_EQ_RANGE(vars[0], 2, 2);
  EXPECT_EQ_RANGE(vars[1], 0, 3);
  EXPECT_EQ_RANGE(vars[2], 3, 3);
  EXPECT_EQ_RANGE(cumuls[0], 0, 11);
  EXPECT_EQ_RANGE(cumuls[1], 0, 20);
  EXPECT_EQ_RANGE(cumuls[2], 5, 16);
  EXPECT_EQ_RANGE(cumuls[3], 9, 20);
  EXPECT_EQ_RANGE(slacks[0], 3, 5);
  EXPECT_EQ_RANGE(slacks[1], 3, 5);
  EXPECT_EQ_RANGE(slacks[2], 3, 5);
  active[1]->SetValue(0);
  vars[1]->SetValue(1);
  EXPECT_EQ_RANGE(vars[0], 2, 2);
  EXPECT_EQ_RANGE(vars[1], 1, 1);
  EXPECT_EQ_RANGE(vars[2], 3, 3);
  EXPECT_EQ_RANGE(cumuls[0], 0, 11);
  EXPECT_EQ_RANGE(cumuls[1], 0, 20);
  EXPECT_EQ_RANGE(cumuls[2], 5, 16);
  EXPECT_EQ_RANGE(cumuls[3], 9, 20);
  EXPECT_EQ_RANGE(slacks[0], 3, 5);
  EXPECT_EQ_RANGE(slacks[1], 3, 5);
  EXPECT_EQ_RANGE(slacks[2], 3, 5);
  slacks[0]->SetValue(4);
  EXPECT_EQ_RANGE(vars[0], 2, 2);
  EXPECT_EQ_RANGE(vars[1], 1, 1);
  EXPECT_EQ_RANGE(vars[2], 3, 3);
  EXPECT_EQ_RANGE(cumuls[0], 0, 10);
  EXPECT_EQ_RANGE(cumuls[1], 0, 20);
  EXPECT_EQ_RANGE(cumuls[2], 6, 16);
  EXPECT_EQ_RANGE(cumuls[3], 10, 20);
  EXPECT_EQ_RANGE(slacks[0], 4, 4);
  EXPECT_EQ_RANGE(slacks[1], 3, 5);
  EXPECT_EQ_RANGE(slacks[2], 3, 5);
  return nullptr;
}

// Only one route has energy cost:
// route 1 -> 5, on vehicle 2, with force = 3, distance = 5, unit_cost = 7.
TEST_IN_PROPAGATION(PathEnergyCostTest, EmptyRoute, solver, true) {
  std::vector<IntVar*> nexts, active, force_cumuls, distance_transits, vehicles,
      costs;
  solver->MakeIntVarArray(3, 0, 5, "next", &nexts);
  solver->MakeIntVarArray(6, 0, 100, "force_cumuls", &force_cumuls);
  solver->MakeIntVarArray(3, 0, 100, "distance_transits", &distance_transits);
  solver->MakeIntVarArray(6, -1, 100, "vehicles", &vehicles);
  solver->MakeIntVarArray(3, 0, 1000, "costs", &costs);

  using EnergyCost = Solver::PathEnergyCostConstraintSpecification::EnergyCost;
  const std::vector<EnergyCost> vehicle_energy_costs{
      {.threshold = 0,
       .cost_per_unit_below_threshold = 0,
       .cost_per_unit_above_threshold = 0},
      {0, 0, 1},
      {0, 0, 7}};
  const std::vector<bool> vehicle_used_when_empty{true, false, true};
  const std::vector<int64_t> vehicle_starts{0, 2, 1};
  const std::vector<int64_t> vehicle_ends{3, 4, 5};

  Solver::PathEnergyCostConstraintSpecification specification{
      .nexts = nexts,
      .paths = vehicles,
      .forces = force_cumuls,
      .distances = distance_transits,
      .path_energy_costs = vehicle_energy_costs,
      .path_used_when_empty = vehicle_used_when_empty,
      .path_starts = vehicle_starts,
      .path_ends = vehicle_ends,
      .costs = costs,
  };

  solver->AddConstraint(solver->MakePathEnergyCostConstraint(specification));
  EXPECT_TRUE(costs[0]->Bound());
  EXPECT_EQ(costs[0]->Value(), 0);
  // We constrain variables little by little to ensure that energy_cost[2]
  // only gets bound when all relevant variables are bound.
  for (const auto [var, value] :
       std::vector<std::pair<IntVar*, int64_t>>{{nexts[0], 3},
                                                {vehicles[0], 0},
                                                {vehicles[3], 0},
                                                {distance_transits[0], 5},
                                                {force_cumuls[3], 3},
                                                {nexts[2], 4},
                                                {vehicles[2], 1},
                                                {vehicles[4], 1},
                                                {distance_transits[2], 5},
                                                {force_cumuls[4], 3},
                                                {nexts[1], 5},
                                                {vehicles[1], 2},
                                                {vehicles[5], 2},
                                                {distance_transits[1], 5}}) {
    var->SetValue(value);
    EXPECT_EQ(costs[2]->Min(), 0);
    EXPECT_EQ(costs[2]->Max(), 1000);
  }
  force_cumuls[5]->SetValue(3);
  EXPECT_TRUE(costs[2]->Bound());
  EXPECT_EQ(costs[2]->Value(), 105);
  // Vehicle 1 is not used when empty, its cost should be 0.
  EXPECT_TRUE(costs[1]->Bound());
  EXPECT_EQ(costs[1]->Value(), 0);
  return nullptr;
}

// Two routes: {0, 1, 3, 4, 5, 10} and {2, 6, 8, 7, 9, 11}.
// The first is bound before adding the constraint, the second after.
// We expect initial propagation to compute cost[0] but not cost[1].
// Setting route 1 variables should propagate to cost[1].
TEST_IN_PROPAGATION(PathEnergyCostTest, WaveRoutes, solver, true) {
  std::vector<IntVar*> nexts, active, force_cumuls, distance_transits, vehicles,
      costs;
  solver->MakeIntVarArray(10, 0, 12, "next", &nexts);
  solver->MakeIntVarArray(12, 0, 100, "force_cumuls", &force_cumuls);
  solver->MakeIntVarArray(10, 0, 100, "distance_transits", &distance_transits);
  solver->MakeIntVarArray(12, -1, 2, "vehicles", &vehicles);
  solver->MakeIntVarArray(2, 0, 1000000, "costs", &costs);

  using EnergyLimit = Solver::PathEnergyCostConstraintSpecification::EnergyCost;
  const std::vector<EnergyLimit> vehicle_energy_limits{
      {.threshold = 3,
       .cost_per_unit_below_threshold = 13,
       .cost_per_unit_above_threshold = 5},
      {.threshold = 5,
       .cost_per_unit_below_threshold = 17,
       .cost_per_unit_above_threshold = 9}};
  const std::vector<bool> vehicle_used_when_empty{true, true};
  const std::vector<int64_t> vehicle_starts{0, 2};
  const std::vector<int64_t> vehicle_ends{10, 11};

  const std::vector<int> path0 = {0, 1, 3, 4, 5, 10};
  const std::vector<int> path1 = {2, 6, 8, 7, 9, 11};
  absl::flat_hash_map<int, int> force = {{0, 0}, {1, 2},  {3, 3}, {4, 2},
                                         {5, 4}, {10, 2}, {2, 5}, {6, 7},
                                         {8, 3}, {7, 4},  {9, 2}, {11, 1}};
  absl::flat_hash_map<int, int> distance = {{0, 2}, {1, 3},  {3, 2}, {4, 3},
                                            {5, 2}, {10, 3}, {2, 5}, {6, 7},
                                            {8, 3}, {7, 4},  {9, 2}, {11, 1}};

  // Set route of vehicle 0 before adding the constraint,
  // to test initial propagation.
  for (int i = 0; i + 1 < path0.size(); ++i) {
    const int node = path0[i];
    distance_transits[node]->SetValue(distance[node]);
    vehicles[node]->SetValue(1);
    if (i + 1 == path0.size()) continue;
    const int next = path0[i + 1];
    force_cumuls[next]->SetValue(force[next]);
    nexts[node]->SetValue(next);
  }

  Solver::PathEnergyCostConstraintSpecification specification{
      .nexts = nexts,
      .paths = vehicles,
      .forces = force_cumuls,
      .distances = distance_transits,
      .path_energy_costs = vehicle_energy_limits,
      .path_used_when_empty = vehicle_used_when_empty,
      .path_starts = vehicle_starts,
      .path_ends = vehicle_ends,
      .costs = costs,
  };
  // Initial propagation should bind costs[0].
  Constraint* path_energy = solver->MakePathEnergyCostConstraint(specification);
  solver->AddConstraint(path_energy);
  // cost[0] should be bound to Sum(force * distance) * unit_cost.
  EXPECT_TRUE(costs[0]->Bound());
  EXPECT_EQ(costs[0]->Value(),
            (2 * 2 + 3 * 3 + 2 * 2 + 3 * 3 + 2 * 2) * 13 + (1 * 3) * 5);
  EXPECT_FALSE(costs[1]->Bound());

  // Set route of vehicle 1 after adding the constraint,
  // to test propagation events.
  for (int i = 0; i + 1 < path1.size(); ++i) {
    const int node = path1[i];
    distance_transits[node]->SetValue(distance[node]);
    vehicles[node]->SetValue(1);
    if (i + 1 == path1.size()) continue;
    const int next = path1[i + 1];
    force_cumuls[next]->SetValue(force[next]);
    nexts[node]->SetValue(next);
  }
  // costs[1] should be bound to sum(force * distance) * unit_cost.
  EXPECT_TRUE(costs[1]->Bound());
  EXPECT_EQ(costs[1]->Value(),
            (5 * 5 + 3 * 7 + 4 * 3 + 2 * 4 + 1 * 2) * 17 + (2 * 5) * 9);

  return nullptr;
}

// ----- Circuit -----

TEST(CircuitTest, BasicApi) {
  Solver s("CircuitTest");
  std::vector<IntVar*> vars;
  s.MakeIntVarArray(10, 0, 10, &vars);
  Constraint* const ct = s.MakeCircuit(vars);
  EXPECT_FALSE(ct->DebugString().empty());
}

TEST_IN_PROPAGATION(CircuitTest, InitialPropagation, solver, true) {
  static const int kSize = 10;
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(kSize, 0, kSize, &vars);
  Constraint* const ct = solver->MakeCircuit(vars);
  solver->AddConstraint(ct);
  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(kSize - 1, vars[i]->Size());
    EXPECT_GE(vars[i]->Min(), 0);
    EXPECT_LT(vars[i]->Max(), kSize);
    EXPECT_FALSE(vars[i]->Contains(i));
  }
  return nullptr;
}

TEST_IN_PROPAGATION(CircuitTest, OneBridgeStanding, solver, true) {
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(5, 0, 4, &vars);
  Constraint* const ct = solver->MakeCircuit(vars);
  solver->AddConstraint(ct);
  vars[0]->SetMax(2);
  vars[1]->SetMax(2);
  vars[2]->SetMax(3);
  vars[3]->SetMin(3);
  vars[4]->SetValue(0);
  return nullptr;
}

TEST_IN_PROPAGATION(CircuitTest, OneBridgeCut, solver, false) {
  IntVar* const v0 = solver->MakeIntVar(0, 4);
  IntVar* const v1 = solver->MakeIntVar(0, 4);
  IntVar* const v2 = solver->MakeIntVar(0, 4);
  IntVar* const v3 = solver->MakeIntVar(0, 4);
  IntVar* const v4 = solver->MakeIntVar(0, 4);
  {
    std::vector<IntVar*> vars = {v0, v1, v2, v3, v4};
    Constraint* const ct = solver->MakeCircuit(vars);
    solver->AddConstraint(ct);
  }
  v0->SetMax(2);
  v1->SetMax(2);
  v2->SetMax(3);
  v3->SetMin(3);
  v4->SetValue(0);
  v2->RemoveValue(3);
  return nullptr;
}

// ----- SubCircuit -----

TEST(SubCircuitTest, BasicApi) {
  Solver s("CircuitTest");
  std::vector<IntVar*> vars;
  s.MakeIntVarArray(10, 0, 10, &vars);
  Constraint* const ct = s.MakeSubCircuit(vars);
  EXPECT_FALSE(ct->DebugString().empty());
}

TEST_IN_PROPAGATION(SubCircuitTest, InitialPropagation, solver, true) {
  static const int kSize = 10;
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(kSize, 0, kSize, &vars);
  Constraint* const ct = solver->MakeSubCircuit(vars);
  solver->AddConstraint(ct);
  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ_RANGE(vars[i], 0, kSize - 1);
  }
  return nullptr;
}

TEST_IN_PROPAGATION(SubCircuitTest, OneBridgeStanding, solver, true) {
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(5, 0, 4, &vars);
  Constraint* const ct = solver->MakeSubCircuit(vars);
  solver->AddConstraint(ct);
  vars[0]->SetMax(2);
  vars[1]->SetMax(2);
  vars[2]->SetMax(3);
  vars[3]->SetMin(3);
  vars[4]->SetValue(0);
  return nullptr;
}

TEST_IN_PROPAGATION(SubCircuitTest, UnreachableFromRoot, solver, true) {
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(5, 0, 4, &vars);
  Constraint* const ct = solver->MakeSubCircuit(vars);
  solver->AddConstraint(ct);
  vars[0]->SetValue(1);
  vars[1]->RemoveValue(2);
  vars[3]->RemoveValue(2);
  vars[4]->RemoveValue(2);
  EXPECT_BOUND_TO(vars[2], 2);
  return nullptr;
}

TEST_IN_PROPAGATION(SubCircuitTest, UnreachableToRoot, solver, true) {
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(5, 0, 4, &vars);
  Constraint* const ct = solver->MakeSubCircuit(vars);
  solver->AddConstraint(ct);
  vars[0]->SetRange(0, 2);
  vars[1]->SetRange(0, 2);
  vars[2]->SetRange(0, 2);
  vars[3]->SetValue(4);
  vars[4]->SetMin(3);
  EXPECT_BOUND_TO(vars[0], 0);
  EXPECT_BOUND_TO(vars[1], 1);
  EXPECT_BOUND_TO(vars[2], 2);
  return nullptr;
}

// ----- PathConnected -----

TEST(PathConnectedTest, BasicApi) {
  Solver solver("PathConnectedTest");
  std::vector<IntVar*> nexts;
  solver.MakeIntVarArray(4, 0, 10, &nexts);
  std::vector<IntVar*> status;
  solver.MakeBoolVarArray(1, &status);
  Constraint* const ct = solver.MakePathConnected(nexts, {0}, {2}, status);
  EXPECT_FALSE(ct->DebugString().empty());
}

TEST_IN_PROPAGATION(PathConnectedTest, InitialPropagation, solver, true) {
  std::vector<IntVar*> nexts;
  solver->MakeIntVarArray(5, 0, 10, &nexts);
  // Closed path
  nexts[0]->SetValue(2);
  // Unclosable path
  nexts[4]->SetValue(10);
  std::vector<IntVar*> status;
  solver->MakeBoolVarArray(3, &status);
  solver->AddConstraint(
      solver->MakePathConnected(nexts, {0, 1, 4}, {2, 3, 2}, status));
  EXPECT_EQ(1, status[0]->Value());
  EXPECT_FALSE(status[1]->Bound());
  EXPECT_EQ(0, status[2]->Value());
  return nullptr;
}

TEST_IN_PROPAGATION(PathConnectedTest, ClosingOnePath, solver, true) {
  std::vector<IntVar*> nexts;
  solver->MakeIntVarArray(4, 0, 10, &nexts);
  std::vector<IntVar*> status;
  solver->MakeBoolVarArray(1, &status);
  solver->AddConstraint(solver->MakePathConnected(nexts, {0}, {2}, status));
  EXPECT_FALSE(status[0]->Bound());
  nexts[0]->SetValue(1);
  EXPECT_FALSE(status[0]->Bound());
  nexts[3]->SetValue(2);
  EXPECT_FALSE(status[0]->Bound());
  nexts[1]->SetValue(3);
  EXPECT_EQ(1, status[0]->Value());
  return nullptr;
}

TEST_IN_PROPAGATION(PathConnectedTest, NoPath, solver, true) {
  std::vector<IntVar*> nexts;
  solver->MakeIntVarArray(3, 0, 10, &nexts);
  std::vector<IntVar*> status;
  solver->MakeBoolVarArray(1, &status);
  solver->AddConstraint(solver->MakePathConnected(nexts, {0}, {2}, status));
  EXPECT_FALSE(status[0]->Bound());
  nexts[0]->SetValue(1);
  EXPECT_FALSE(status[0]->Bound());
  nexts[1]->SetValue(4);
  EXPECT_EQ(0, status[0]->Value());
  return nullptr;
}

TEST_IN_PROPAGATION(PathConnectedTest, Cycle, solver, true) {
  std::vector<IntVar*> nexts;
  solver->MakeIntVarArray(3, 0, 10, &nexts);
  std::vector<IntVar*> status;
  solver->MakeBoolVarArray(1, &status);
  solver->AddConstraint(solver->MakePathConnected(nexts, {0}, {2}, status));
  EXPECT_FALSE(status[0]->Bound());
  nexts[0]->SetValue(1);
  EXPECT_FALSE(status[0]->Bound());
  nexts[1]->SetValue(0);
  EXPECT_EQ(0, status[0]->Value());
  return nullptr;
}

TEST_IN_PROPAGATION(PathConnectedTest, SingleNodePath, solver, true) {
  std::vector<IntVar*> nexts;
  solver->MakeIntVarArray(3, 0, 10, &nexts);
  std::vector<IntVar*> status;
  solver->MakeBoolVarArray(1, &status);
  solver->AddConstraint(solver->MakePathConnected(nexts, {0}, {0}, status));
  EXPECT_EQ(1, status[0]->Value());
  return nullptr;
}

// PathPrecedence

TEST(SimplePathPrecedenceTest, BasicApi) {
  Solver solver("PathPrecedenceTest");
  std::vector<IntVar*> nexts;
  solver.MakeIntVarArray(4, 0, 10, &nexts);
  Constraint* const ct = solver.MakePathPrecedenceConstraint(nexts, {{1, 2}});
  EXPECT_EQ(
      "PathPrecedence(DomainIntVar(0..10),DomainIntVar(0..10),DomainIntVar(0.."
      "10),DomainIntVar(0..10),(1, 2))",
      ct->DebugString());
}

TEST_IN_PROPAGATION(SimplePathPrecedenceTest, EmptyGraph, solver, true) {
  std::vector<IntVar*> nexts;
  solver->AddConstraint(solver->MakePathPrecedenceConstraint(nexts, {{1, 2}}));
  return nullptr;
}

TEST_IN_PROPAGATION(SimplePathPrecedenceTest, EmptyGraphAndPrecedence, solver,
                    true) {
  std::vector<IntVar*> nexts;
  solver->AddConstraint(solver->MakePathPrecedenceConstraint(nexts, {{}}));
  return nullptr;
}

class PathPrecedenceTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("PathPrecedenceTest");
    solver_->MakeIntVarArray(7, 0, 10, &nexts_);
  }

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> nexts_;
};

TEST_IN_PROPAGATION1(PathPrecedenceTest, EmptyPrecedence, solver, true,
                     const std::vector<IntVar*>&, nexts_) {
  solver->AddConstraint(solver->MakePathPrecedenceConstraint(nexts_, {{}}));
  nexts_[0]->SetValue(1);
  nexts_[1]->SetValue(2);
  nexts_[2]->SetValue(3);
  return nullptr;
}

TEST_IN_PROPAGATION1(PathPrecedenceTest, InactivePrecedence, solver, true,
                     const std::vector<IntVar*>&, nexts_) {
  solver->AddConstraint(
      solver->MakePathPrecedenceConstraint(nexts_, {{8, 9}, {4, 6}}));
  nexts_[0]->SetValue(1);
  nexts_[1]->SetValue(2);
  nexts_[2]->SetValue(3);
  return nullptr;
}

TEST_IN_PROPAGATION1(PathPrecedenceTest, DirectConnect, solver, true,
                     const std::vector<IntVar*>&, nexts_) {
  solver->AddConstraint(solver->MakePathPrecedenceConstraint(nexts_, {{1, 2}}));
  nexts_[0]->SetValue(1);
  nexts_[1]->SetValue(2);
  nexts_[2]->SetValue(4);
  return nullptr;
}

TEST_IN_PROPAGATION1(PathPrecedenceTest, DirectConnectFail, solver, false,
                     const std::vector<IntVar*>&, nexts_) {
  solver->AddConstraint(solver->MakePathPrecedenceConstraint(nexts_, {{1, 2}}));
  nexts_[2]->SetValue(1);
  return nullptr;
}

TEST_IN_PROPAGATION1(PathPrecedenceTest, DirectConnectPrecedenceLast, solver,
                     true, const std::vector<IntVar*>&, nexts_) {
  solver->AddConstraint(solver->MakePathPrecedenceConstraint(nexts_, {{1, 2}}));
  nexts_[0]->SetValue(1);
  nexts_[2]->SetValue(4);
  nexts_[1]->SetValue(2);
  return nullptr;
}

TEST_IN_PROPAGATION1(PathPrecedenceTest, InDirectConnect, solver, true,
                     const std::vector<IntVar*>&, nexts_) {
  solver->AddConstraint(solver->MakePathPrecedenceConstraint(nexts_, {{1, 2}}));
  nexts_[0]->SetValue(1);
  nexts_[1]->SetValue(3);
  nexts_[3]->SetValue(2);
  nexts_[2]->SetValue(4);
  return nullptr;
}

TEST_IN_PROPAGATION1(PathPrecedenceTest, InDirectConnectPrecedenceLast, solver,
                     true, const std::vector<IntVar*>&, nexts_) {
  solver->AddConstraint(solver->MakePathPrecedenceConstraint(nexts_, {{1, 2}}));
  nexts_[0]->SetValue(1);
  nexts_[2]->SetValue(4);
  nexts_[1]->SetValue(3);
  nexts_[3]->SetValue(2);
  return nullptr;
}

TEST_IN_PROPAGATION1(PathPrecedenceTest, InDirectConnectFail, solver, false,
                     const std::vector<IntVar*>&, nexts_) {
  solver->AddConstraint(solver->MakePathPrecedenceConstraint(nexts_, {{1, 2}}));
  nexts_[2]->SetValue(3);
  nexts_[3]->SetValue(1);
  return nullptr;
}

void TestWithUninterleavedPrecedences(Solver* solver,
                                      const std::vector<IntVar*>& nexts,
                                      Constraint* ct) {
  solver->AddConstraint(ct);
  nexts[0]->SetValue(1);
  nexts[1]->SetValue(2);
  nexts[2]->SetValue(3);
  nexts[3]->SetValue(4);
  nexts[4]->SetValue(5);
}

TEST_IN_PROPAGATION1(PathPrecedenceTest, UninterleavedPrecedences, solver, true,
                     const std::vector<IntVar*>&, nexts_) {
  TestWithUninterleavedPrecedences(
      solver, nexts_,
      solver->MakePathPrecedenceConstraint(nexts_, {{1, 2}, {3, 4}}));
  return nullptr;
}

TEST_IN_PROPAGATION1(PathPrecedenceTest, UninterleavedPrecedencesWithLIFO,
                     solver, true, const std::vector<IntVar*>&, nexts_) {
  TestWithUninterleavedPrecedences(
      solver, nexts_,
      solver->MakePathPrecedenceConstraint(nexts_, {{1, 2}, {3, 4}}, {0}, {}));
  return nullptr;
}

TEST_IN_PROPAGATION1(PathPrecedenceTest, UninterleavedPrecedencesWithFIFO,
                     solver, true, const std::vector<IntVar*>&, nexts_) {
  TestWithUninterleavedPrecedences(
      solver, nexts_,
      solver->MakePathPrecedenceConstraint(nexts_, {{1, 2}, {3, 4}}, {}, {0}));
  return nullptr;
}

void TestWithLIFOPrecedences(Solver* solver, const std::vector<IntVar*>& nexts,
                             Constraint* ct) {
  solver->AddConstraint(ct);
  nexts[0]->SetValue(1);
  nexts[1]->SetValue(3);
  nexts[3]->SetValue(4);
  nexts[4]->SetValue(2);
  nexts[2]->SetValue(5);
}

void TestWithLIFOPrecedencesWithSingleton(Solver* solver,
                                          const std::vector<IntVar*>& nexts,
                                          Constraint* ct) {
  solver->AddConstraint(ct);
  nexts[0]->SetValue(1);
  nexts[1]->SetValue(3);
  nexts[3]->SetValue(6);
  nexts[6]->SetValue(4);
  nexts[4]->SetValue(2);
  nexts[2]->SetValue(5);
}

TEST_IN_PROPAGATION1(PathPrecedenceTest, LIFOPrecedences, solver, true,
                     const std::vector<IntVar*>&, nexts_) {
  Constraint* ct =
      solver->MakePathPrecedenceConstraint(nexts_, {{1, 2}, {3, 4}});
  TestWithLIFOPrecedences(solver, nexts_, ct);
  return nullptr;
}

TEST_IN_PROPAGATION1(PathPrecedenceTest, LIFOPrecedencesWithSingleton, solver,
                     true, const std::vector<IntVar*>&, nexts_) {
  Constraint* ct =
      solver->MakePathPrecedenceConstraint(nexts_, {{1, 2}, {3, 4}});
  TestWithLIFOPrecedencesWithSingleton(solver, nexts_, ct);
  return nullptr;
}

TEST_IN_PROPAGATION1(PathPrecedenceTest, LIFOPrecedencesWithLIFO, solver, true,
                     const std::vector<IntVar*>&, nexts_) {
  Constraint* ct =
      solver->MakePathPrecedenceConstraint(nexts_, {{1, 2}, {3, 4}}, {0}, {});
  TestWithLIFOPrecedences(solver, nexts_, ct);
  return nullptr;
}

TEST_IN_PROPAGATION1(PathPrecedenceTest, LIFOPrecedencesWithLIFOAndSingleton,
                     solver, true, const std::vector<IntVar*>&, nexts_) {
  Constraint* ct =
      solver->MakePathPrecedenceConstraint(nexts_, {{1, 2}, {3, 4}}, {0}, {});
  TestWithLIFOPrecedencesWithSingleton(solver, nexts_, ct);
  return nullptr;
}

TEST_IN_PROPAGATION1(PathPrecedenceTest, LIFOPrecedencesWithFIFO, solver, false,
                     const std::vector<IntVar*>&, nexts_) {
  Constraint* ct =
      solver->MakePathPrecedenceConstraint(nexts_, {{1, 2}, {3, 4}}, {}, {0});
  TestWithLIFOPrecedences(solver, nexts_, ct);
  return nullptr;
}

TEST_IN_PROPAGATION1(PathPrecedenceTest, LIFOPrecedencesWithFIFOAndSingleton,
                     solver, false, const std::vector<IntVar*>&, nexts_) {
  Constraint* ct =
      solver->MakePathPrecedenceConstraint(nexts_, {{1, 2}, {3, 4}}, {}, {0});
  TestWithLIFOPrecedencesWithSingleton(solver, nexts_, ct);
  return nullptr;
}

void TestWithFIFOPrecedences(Solver* solver, const std::vector<IntVar*>& nexts,
                             Constraint* ct) {
  solver->AddConstraint(ct);
  nexts[0]->SetValue(1);
  nexts[1]->SetValue(3);
  nexts[3]->SetValue(2);
  nexts[2]->SetValue(4);
  nexts[4]->SetValue(5);
}

void TestWithFIFOPrecedencesWithSingleton(Solver* solver,
                                          const std::vector<IntVar*>& nexts,
                                          Constraint* ct) {
  solver->AddConstraint(ct);
  nexts[0]->SetValue(1);
  nexts[1]->SetValue(3);
  nexts[3]->SetValue(6);
  nexts[6]->SetValue(2);
  nexts[2]->SetValue(4);
  nexts[4]->SetValue(5);
}

TEST_IN_PROPAGATION1(PathPrecedenceTest, FIFOPrecedences, solver, true,
                     const std::vector<IntVar*>&, nexts_) {
  Constraint* ct =
      solver->MakePathPrecedenceConstraint(nexts_, {{1, 2}, {3, 4}});
  TestWithFIFOPrecedences(solver, nexts_, ct);
  return nullptr;
}

TEST_IN_PROPAGATION1(PathPrecedenceTest, FIFOPrecedencesWithSingleton, solver,
                     true, const std::vector<IntVar*>&, nexts_) {
  Constraint* ct =
      solver->MakePathPrecedenceConstraint(nexts_, {{1, 2}, {3, 4}});
  TestWithFIFOPrecedencesWithSingleton(solver, nexts_, ct);
  return nullptr;
}

TEST_IN_PROPAGATION1(PathPrecedenceTest, FIFOPrecedencesWithLIFO, solver, false,
                     const std::vector<IntVar*>&, nexts_) {
  Constraint* ct =
      solver->MakePathPrecedenceConstraint(nexts_, {{1, 2}, {3, 4}}, {0}, {});
  TestWithFIFOPrecedences(solver, nexts_, ct);
  return nullptr;
}

TEST_IN_PROPAGATION1(PathPrecedenceTest, FIFOPrecedencesWithLIFOAndSingletong,
                     solver, false, const std::vector<IntVar*>&, nexts_) {
  Constraint* ct =
      solver->MakePathPrecedenceConstraint(nexts_, {{1, 2}, {3, 4}}, {0}, {});
  TestWithFIFOPrecedencesWithSingleton(solver, nexts_, ct);
  return nullptr;
}

TEST_IN_PROPAGATION1(PathPrecedenceTest, FIFOPrecedencesWithFIFO, solver, true,
                     const std::vector<IntVar*>&, nexts_) {
  Constraint* ct =
      solver->MakePathPrecedenceConstraint(nexts_, {{1, 2}, {3, 4}}, {}, {0});
  TestWithFIFOPrecedences(solver, nexts_, ct);
  return nullptr;
}

TEST_IN_PROPAGATION1(PathPrecedenceTest, FIFOPrecedencesWithFIFOAndSingleton,
                     solver, true, const std::vector<IntVar*>&, nexts_) {
  Constraint* ct =
      solver->MakePathPrecedenceConstraint(nexts_, {{1, 2}, {3, 4}}, {}, {0});
  TestWithFIFOPrecedencesWithSingleton(solver, nexts_, ct);
  return nullptr;
}

TEST_IN_PROPAGATION1(PathPrecedenceTest, MultiplePrecedencesFail, solver, false,
                     const std::vector<IntVar*>&, nexts_) {
  solver->AddConstraint(
      solver->MakePathPrecedenceConstraint(nexts_, {{1, 2}, {3, 4}}));
  nexts_[0]->SetValue(1);
  nexts_[1]->SetValue(2);
  nexts_[2]->SetValue(4);
  nexts_[4]->SetValue(3);
  return nullptr;
}

TEST_IN_PROPAGATION1(PathPrecedenceTest, MultipleInterleavedPrecedencesFail,
                     solver, false, const std::vector<IntVar*>&, nexts_) {
  solver->AddConstraint(
      solver->MakePathPrecedenceConstraint(nexts_, {{1, 2}, {3, 4}}));
  nexts_[0]->SetValue(4);
  nexts_[4]->SetValue(1);
  nexts_[1]->SetValue(2);
  nexts_[2]->SetValue(3);
  return nullptr;
}

TEST_IN_PROPAGATION1(PathPrecedenceTest, MultiplePrecedencesOnSingleNodeFail,
                     solver, false, const std::vector<IntVar*>&, nexts_) {
  solver->AddConstraint(
      solver->MakePathPrecedenceConstraint(nexts_, {{1, 2}, {3, 2}}));
  nexts_[0]->SetValue(3);
  nexts_[3]->SetValue(2);
  nexts_[2]->SetValue(1);
  nexts_[1]->SetValue(4);
  nexts_[4]->SetValue(5);
  return nullptr;
}

TEST_IN_PROPAGATION1(PathPrecedenceTest, MultiplePrecedencesOnMultipleNodesFail,
                     solver, false, const std::vector<IntVar*>&, nexts_) {
  solver->AddConstraint(solver->MakePathPrecedenceConstraint(nexts_, {{1, 4},
                                                                      {1, 5},
                                                                      {1, 6},
                                                                      {2, 4},
                                                                      {2, 5},
                                                                      {2, 6},
                                                                      {3, 4},
                                                                      {3, 5},
                                                                      {3, 6}}));
  nexts_[0]->SetValue(3);
  nexts_[3]->SetValue(4);
  nexts_[6]->SetValue(1);
  return nullptr;
}

// PathTransitPrecedence

TEST(SimplePathTransitPrecedenceTest, BasicApi) {
  Solver solver("PathTransitPrecedenceTest");
  std::vector<IntVar*> nexts;
  solver.MakeIntVarArray(4, 0, 10, &nexts);
  std::vector<IntVar*> transits;
  solver.MakeIntVarArray(3, 0, 20, &transits);
  Constraint* const ct =
      solver.MakePathTransitPrecedenceConstraint(nexts, transits, {{1, 2}});
  EXPECT_EQ(
      "PathPrecedence(DomainIntVar(0..10),DomainIntVar(0..10),DomainIntVar(0.."
      "10),DomainIntVar(0..10),DomainIntVar(0..20),DomainIntVar(0..20),"
      "DomainIntVar(0..20),(1, 2))",
      ct->DebugString());
}

TEST_IN_PROPAGATION(SimplePathTransitPrecedenceTest, EmptyGraph, solver, true) {
  std::vector<IntVar*> nexts;
  solver->AddConstraint(
      solver->MakePathTransitPrecedenceConstraint(nexts, {}, {{1, 2}}));
  return nullptr;
}

TEST_IN_PROPAGATION(SimplePathTransitPrecedenceTest, EmptyGraphAndPrecedence,
                    solver, true) {
  std::vector<IntVar*> nexts;
  solver->AddConstraint(
      solver->MakePathTransitPrecedenceConstraint(nexts, {}, {{}}));
  return nullptr;
}

class PathTransitPrecedenceTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("PathTransitPrecedenceTest");
    solver_->MakeIntVarArray(5, 0, 10, &nexts_);
    solver_->MakeIntVarArray(5, -20, 20, &transits_);
  }

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> nexts_;
  std::vector<IntVar*> transits_;
};

TEST_IN_PROPAGATION2(PathTransitPrecedenceTest, EmptyPrecedence, solver, true,
                     const std::vector<IntVar*>&, nexts_,
                     const std::vector<IntVar*>, transits_) {
  solver->AddConstraint(
      solver->MakePathTransitPrecedenceConstraint(nexts_, transits_, {{}}));
  nexts_[0]->SetValue(1);
  nexts_[1]->SetValue(2);
  nexts_[2]->SetValue(3);
  return nullptr;
}

TEST_IN_PROPAGATION2(PathTransitPrecedenceTest, InactivePrecedence, solver,
                     true, const std::vector<IntVar*>&, nexts_,
                     const std::vector<IntVar*>, transits_) {
  solver->AddConstraint(solver->MakePathTransitPrecedenceConstraint(
      nexts_, transits_, {{8, 9}, {4, 6}}));
  for (IntVar* const transit : transits_) transit->SetMin(1);
  nexts_[0]->SetValue(1);
  nexts_[1]->SetValue(2);
  nexts_[2]->SetValue(3);
  return nullptr;
}

TEST_IN_PROPAGATION2(PathTransitPrecedenceTest, DirectConnect, solver, true,
                     const std::vector<IntVar*>&, nexts_,
                     const std::vector<IntVar*>, transits_) {
  solver->AddConstraint(
      solver->MakePathTransitPrecedenceConstraint(nexts_, transits_, {{1, 2}}));
  for (IntVar* const transit : transits_) transit->SetMin(1);
  nexts_[0]->SetValue(1);
  nexts_[1]->SetValue(2);
  nexts_[2]->SetValue(4);
  return nullptr;
}

TEST_IN_PROPAGATION2(PathTransitPrecedenceTest, DirectConnectFail, solver,
                     false, const std::vector<IntVar*>&, nexts_,
                     const std::vector<IntVar*>, transits_) {
  solver->AddConstraint(
      solver->MakePathTransitPrecedenceConstraint(nexts_, transits_, {{1, 2}}));
  for (IntVar* const transit : transits_) transit->SetMin(1);
  nexts_[2]->SetValue(1);
  return nullptr;
}

TEST_IN_PROPAGATION2(PathTransitPrecedenceTest, DirectConnectNegativeTransit,
                     solver, true, const std::vector<IntVar*>&, nexts_,
                     const std::vector<IntVar*>, transits_) {
  solver->AddConstraint(
      solver->MakePathTransitPrecedenceConstraint(nexts_, transits_, {{1, 2}}));
  nexts_[2]->SetValue(1);
  return nullptr;
}

TEST_IN_PROPAGATION2(PathTransitPrecedenceTest, DirectConnectPrecedenceLast,
                     solver, true, const std::vector<IntVar*>&, nexts_,
                     const std::vector<IntVar*>, transits_) {
  for (IntVar* const transit : transits_) transit->SetMin(1);
  solver->AddConstraint(
      solver->MakePathTransitPrecedenceConstraint(nexts_, transits_, {{1, 2}}));
  nexts_[0]->SetValue(1);
  nexts_[2]->SetValue(4);
  nexts_[1]->SetValue(2);
  return nullptr;
}

TEST_IN_PROPAGATION2(PathTransitPrecedenceTest, InDirectConnect, solver, true,
                     const std::vector<IntVar*>&, nexts_,
                     const std::vector<IntVar*>, transits_) {
  solver->AddConstraint(
      solver->MakePathTransitPrecedenceConstraint(nexts_, transits_, {{1, 2}}));
  for (IntVar* const transit : transits_) transit->SetMin(1);
  nexts_[0]->SetValue(1);
  nexts_[1]->SetValue(3);
  nexts_[3]->SetValue(2);
  nexts_[2]->SetValue(4);
  return nullptr;
}

TEST_IN_PROPAGATION2(PathTransitPrecedenceTest, InDirectConnectPrecedenceLast,
                     solver, true, const std::vector<IntVar*>&, nexts_,
                     const std::vector<IntVar*>, transits_) {
  solver->AddConstraint(
      solver->MakePathTransitPrecedenceConstraint(nexts_, transits_, {{1, 2}}));
  for (IntVar* const transit : transits_) transit->SetMin(1);
  nexts_[0]->SetValue(1);
  nexts_[2]->SetValue(4);
  nexts_[1]->SetValue(3);
  nexts_[3]->SetValue(2);
  return nullptr;
}

TEST_IN_PROPAGATION2(PathTransitPrecedenceTest, InDirectConnectFail, solver,
                     false, const std::vector<IntVar*>&, nexts_,
                     const std::vector<IntVar*>, transits_) {
  solver->AddConstraint(
      solver->MakePathTransitPrecedenceConstraint(nexts_, transits_, {{1, 2}}));
  for (IntVar* const transit : transits_) transit->SetMin(1);
  nexts_[2]->SetValue(3);
  nexts_[3]->SetValue(1);
  return nullptr;
}

TEST_IN_PROPAGATION2(PathTransitPrecedenceTest, InDirectConnectNegativeTransits,
                     solver, true, const std::vector<IntVar*>&, nexts_,
                     const std::vector<IntVar*>, transits_) {
  solver->AddConstraint(
      solver->MakePathTransitPrecedenceConstraint(nexts_, transits_, {{1, 2}}));
  nexts_[2]->SetValue(3);
  nexts_[3]->SetValue(1);
  return nullptr;
}

TEST_IN_PROPAGATION2(PathTransitPrecedenceTest, MultiplePrecedences, solver,
                     true, const std::vector<IntVar*>&, nexts_,
                     const std::vector<IntVar*>, transits_) {
  solver->AddConstraint(solver->MakePathTransitPrecedenceConstraint(
      nexts_, transits_, {{1, 2}, {3, 4}}));
  for (IntVar* const transit : transits_) transit->SetMin(1);
  nexts_[0]->SetValue(1);
  nexts_[1]->SetValue(2);
  nexts_[2]->SetValue(3);
  nexts_[3]->SetValue(4);
  nexts_[4]->SetValue(5);
  return nullptr;
}

TEST_IN_PROPAGATION2(PathTransitPrecedenceTest, MultiplePrecedencesFail, solver,
                     false, const std::vector<IntVar*>&, nexts_,
                     const std::vector<IntVar*>, transits_) {
  solver->AddConstraint(solver->MakePathTransitPrecedenceConstraint(
      nexts_, transits_, {{1, 2}, {3, 4}}));
  for (IntVar* const transit : transits_) transit->SetMin(1);
  nexts_[0]->SetValue(1);
  nexts_[1]->SetValue(2);
  nexts_[2]->SetValue(4);
  nexts_[4]->SetValue(3);
  return nullptr;
}

TEST_IN_PROPAGATION2(PathTransitPrecedenceTest,
                     MultiplePrecedencesNegativeTransits, solver, true,
                     const std::vector<IntVar*>&, nexts_,
                     const std::vector<IntVar*>, transits_) {
  solver->AddConstraint(solver->MakePathTransitPrecedenceConstraint(
      nexts_, transits_, {{1, 2}, {3, 4}}));
  nexts_[0]->SetValue(1);
  nexts_[1]->SetValue(2);
  nexts_[2]->SetValue(4);
  nexts_[4]->SetValue(3);
  return nullptr;
}

TEST_IN_PROPAGATION2(PathTransitPrecedenceTest,
                     MultipleInterleavedPrecedencesFail, solver, false,
                     const std::vector<IntVar*>&, nexts_,
                     const std::vector<IntVar*>, transits_) {
  solver->AddConstraint(solver->MakePathTransitPrecedenceConstraint(
      nexts_, transits_, {{1, 2}, {3, 4}}));
  for (IntVar* const transit : transits_) transit->SetMin(1);
  nexts_[0]->SetValue(4);
  nexts_[4]->SetValue(1);
  nexts_[1]->SetValue(2);
  nexts_[2]->SetValue(3);
  return nullptr;
}

TEST_IN_PROPAGATION2(PathTransitPrecedenceTest,
                     MultipleInterleavedPrecedencesNegativeTransit, solver,
                     true, const std::vector<IntVar*>&, nexts_,
                     const std::vector<IntVar*>, transits_) {
  solver->AddConstraint(solver->MakePathTransitPrecedenceConstraint(
      nexts_, transits_, {{1, 2}, {3, 4}}));
  nexts_[0]->SetValue(4);
  nexts_[4]->SetValue(1);
  nexts_[1]->SetValue(2);
  nexts_[2]->SetValue(3);
  return nullptr;
}

TEST_IN_PROPAGATION2(PathTransitPrecedenceTest,
                     MultiplePrecedencesOnSingleNodeFail, solver, false,
                     const std::vector<IntVar*>&, nexts_,
                     const std::vector<IntVar*>, transits_) {
  solver->AddConstraint(solver->MakePathTransitPrecedenceConstraint(
      nexts_, transits_, {{1, 2}, {3, 2}}));
  for (IntVar* const transit : transits_) transit->SetMin(1);
  nexts_[0]->SetValue(3);
  nexts_[3]->SetValue(2);
  nexts_[2]->SetValue(1);
  nexts_[1]->SetValue(4);
  nexts_[4]->SetValue(5);
  return nullptr;
}

TEST_IN_PROPAGATION2(PathTransitPrecedenceTest,
                     MultiplePrecedencesOnSingleNodeNegativeTransits, solver,
                     true, const std::vector<IntVar*>&, nexts_,
                     const std::vector<IntVar*>, transits_) {
  solver->AddConstraint(solver->MakePathTransitPrecedenceConstraint(
      nexts_, transits_, {{1, 2}, {3, 2}}));
  nexts_[0]->SetValue(3);
  nexts_[3]->SetValue(2);
  nexts_[2]->SetValue(1);
  nexts_[1]->SetValue(4);
  nexts_[4]->SetValue(5);
  return nullptr;
}
}  // namespace
}  // namespace operations_research
