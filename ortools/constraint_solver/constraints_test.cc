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
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solver_test_util.h"

namespace operations_research {
namespace {
class ConstraintsTest : public testing::Test {
 protected:
  ConstraintsTest() : solver_(new Solver("ConstraintTest")) {}
  std::unique_ptr<Solver> solver_;
};

TEST_F(ConstraintsTest, Pheasant) {
  IntVar* const p = solver_->MakeIntVar(0, 100, "pheasant");
  IntVar* const r = solver_->MakeIntVar(0, 100, "rabbit");
  IntExpr* const sum1 =
      solver_->MakeSum(solver_->MakeProd(p, 2), solver_->MakeProd(r, 4));
  IntExpr* const sum2 = solver_->MakeSum(p, r);
  IntVar* const v1 = sum1->Var();
  IntVar* const v2 = sum2->Var();
  Constraint* const ct1 = solver_->MakeEquality(v1, 56);
  Constraint* const ct2 = solver_->MakeEquality(v2, 20);
  solver_->AddConstraint(ct1);
  solver_->AddConstraint(ct2);
  DecisionBuilder* const db = solver_->MakePhase(
      p, r, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  solver_->NewSearch(db);
  ASSERT_TRUE(solver_->NextSolution());
  EXPECT_EQ(8, r->Value());
  EXPECT_EQ(12, p->Value());
  solver_->EndSearch();
}

TEST_F(ConstraintsTest, PheasantExpr) {
  IntVar* const p = solver_->MakeIntVar(0, 100, "pheasant");
  IntVar* const r = solver_->MakeIntVar(0, 100, "rabbit");
  IntExpr* const sum1 =
      solver_->MakeSum(solver_->MakeProd(p, 2), solver_->MakeProd(r, 4));
  IntExpr* const sum2 = solver_->MakeSum(p, r);
  Constraint* const ct1 = solver_->MakeEquality(sum1, 56);
  Constraint* const ct2 = solver_->MakeEquality(sum2, 20);
  solver_->AddConstraint(ct1);
  solver_->AddConstraint(ct2);
  DecisionBuilder* const db = solver_->MakePhase(
      p, r, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  solver_->NewSearch(db);
  ASSERT_TRUE(solver_->NextSolution());
  EXPECT_EQ(8, r->Value());
  EXPECT_EQ(12, p->Value());
  solver_->EndSearch();
}

TEST_F(ConstraintsTest, SendMoreMoney) {
  IntVar* const s = solver_->MakeIntVar(0, 9, "s");
  IntVar* const e = solver_->MakeIntVar(0, 9, "e");
  IntVar* const n = solver_->MakeIntVar(0, 9, "n");
  IntVar* const d = solver_->MakeIntVar(0, 9, "d");
  IntVar* const m = solver_->MakeIntVar(0, 9, "m");
  IntVar* const o = solver_->MakeIntVar(0, 9, "o");
  IntVar* const r = solver_->MakeIntVar(0, 9, "r");
  IntVar* const y = solver_->MakeIntVar(0, 9, "y");
  IntExpr* const send = solver_->MakeSum(
      solver_->MakeSum(solver_->MakeProd(s, 1000), solver_->MakeProd(e, 100)),
      solver_->MakeSum(solver_->MakeProd(n, 10), d));
  IntExpr* const more = solver_->MakeSum(
      solver_->MakeSum(solver_->MakeProd(m, 1000), solver_->MakeProd(o, 100)),
      solver_->MakeSum(solver_->MakeProd(r, 10), e));
  IntExpr* const money = solver_->MakeSum(
      solver_->MakeProd(m, 10000),
      solver_->MakeSum(solver_->MakeSum(solver_->MakeProd(o, 1000),
                                        solver_->MakeProd(n, 100)),
                       solver_->MakeSum(solver_->MakeProd(e, 10), y)));
  Constraint* const ct1 =
      solver_->MakeEquality(solver_->MakeSum(send, more)->Var(), money->Var());
  solver_->AddConstraint(ct1);
  std::vector<IntVar*> vars;
  vars.push_back(s);
  vars.push_back(e);
  vars.push_back(n);
  vars.push_back(d);
  vars.push_back(m);
  vars.push_back(o);
  vars.push_back(r);
  vars.push_back(y);
  Constraint* const ct2 = solver_->MakeAllDifferent(vars, false);
  solver_->AddConstraint(ct2);
  Constraint* const ct3 = solver_->MakeNonEquality(m, int64_t{0});
  solver_->AddConstraint(ct3);
  Constraint* const ct4 = solver_->MakeNonEquality(s, int64_t{0});
  solver_->AddConstraint(ct4);
  DecisionBuilder* const db = solver_->MakePhase(
      vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  solver_->NewSearch(db);
  ASSERT_TRUE(solver_->NextSolution());
  EXPECT_EQ(9, s->Value());
  EXPECT_EQ(5, e->Value());
  EXPECT_EQ(6, n->Value());
  EXPECT_EQ(7, d->Value());
  EXPECT_EQ(1, m->Value());
  EXPECT_EQ(0, o->Value());
  EXPECT_EQ(8, r->Value());
  EXPECT_EQ(2, y->Value());
  solver_->EndSearch();
}

TEST_F(ConstraintsTest, Golomb_8) {
  const int n = 8;
  const int64_t max_val = n * n;
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(n, 0, max_val, "var", &vars);
  std::vector<IntVar*> diffs;
  int i = 0, j = 0;
  for (i = 0; i < n; ++i) {
    for (j = i + 1; j < n; ++j) {
      IntExpr* const e = solver_->MakeDifference(vars[j], vars[i]);
      diffs.push_back(e->Var());
    }
  }
  for (i = 0; i < n - 1; ++i) {
    solver_->AddConstraint(solver_->MakeLess(vars[i], vars[i + 1]));
  }
  solver_->AddConstraint(solver_->MakeEquality(vars[0], Zero()));
  solver_->AddConstraint(solver_->MakeAllDifferent(diffs, false));
  Assignment* const soln = solver_->MakeAssignment();
  soln->Add(vars);
  soln->AddObjective(vars[n - 1]);
  SolutionCollector* const solns = solver_->MakeAllSolutionCollector(soln);
  OptimizeVar* const minim = solver_->MakeMinimize(vars[n - 1], 1);
  DecisionBuilder* const db = solver_->MakePhase(
      vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  ASSERT_TRUE(solver_->Solve(db, solns, minim));
  ASSERT_EQ(7, solns->solution_count());
  EXPECT_EQ(34, solns->solution(6)->ObjectiveValue());
}

TEST_F(ConstraintsTest, Golomb_8_NewSearch) {
  const int n = 8;
  const int64_t max_val = n * n;
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(n, 0, max_val, "var", &vars);
  std::vector<IntVar*> diffs;
  int i = 0, j = 0;
  for (i = 0; i < n; ++i) {
    for (j = i + 1; j < n; ++j) {
      IntExpr* const e = solver_->MakeDifference(vars[j], vars[i]);
      diffs.push_back(e->Var());
    }
  }
  for (i = 0; i < n - 1; ++i) {
    solver_->AddConstraint(solver_->MakeLess(vars[i], vars[i + 1]));
  }
  solver_->AddConstraint(solver_->MakeEquality(vars[0], Zero()));
  solver_->AddConstraint(solver_->MakeAllDifferent(diffs, false));
  OptimizeVar* const minim = solver_->MakeMinimize(vars[n - 1], 1);
  DecisionBuilder* const db = solver_->MakePhase(
      vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  solver_->NewSearch(db, minim);
  int64_t best = kint64max;
  int64_t count = 0;
  while (solver_->NextSolution()) {
    count++;
    const int64_t objval = vars[n - 1]->Value();
    EXPECT_LT(objval, best);
    best = objval;
  }
  solver_->EndSearch();
  ASSERT_EQ(7, count);
  EXPECT_EQ(34, best);
}

// MapDomain constraint

TEST_IN_PROPAGATION(MapDomainTest, MapDomainFromVar, s, true) {
  IntVar* var = s->MakeIntVar(0, 3, "var");
  std::vector<IntVar*> actives;
  s->MakeIntVarArray(6, 0, 2, "active", &actives);
  s->AddConstraint(s->MakeMapDomain(var, actives));
  EXPECT_EQ(0, actives[0]->Min());
  EXPECT_EQ(0, actives[1]->Min());
  EXPECT_EQ(0, actives[2]->Min());
  EXPECT_EQ(0, actives[3]->Min());
  EXPECT_EQ(0, actives[4]->Min());
  EXPECT_EQ(0, actives[5]->Min());
  EXPECT_EQ(1, actives[0]->Max());
  EXPECT_EQ(1, actives[1]->Max());
  EXPECT_EQ(1, actives[2]->Max());
  EXPECT_EQ(1, actives[3]->Max());
  EXPECT_EQ(0, actives[4]->Max());
  EXPECT_EQ(0, actives[5]->Max());
  var->RemoveValue(2);
  EXPECT_EQ(0, actives[2]->Max());
  var->SetMin(1);
  EXPECT_EQ(0, actives[0]->Max());
  EXPECT_EQ(1, actives[1]->Max());
  var->SetValue(3);
  EXPECT_EQ(0, actives[1]->Max());
  EXPECT_EQ(1, actives[3]->Min());

  actives.clear();
  var = s->MakeIntVar(0, 3, "var");
  s->MakeIntVarArray(2, 0, 2, "active", &actives);
  s->AddConstraint(s->MakeMapDomain(var, actives));
  EXPECT_EQ(3, var->Max());
  return nullptr;
}

TEST_IN_PROPAGATION(MapDomainTest, MapDomainFromActive, s, true) {
  IntVar* const var = s->MakeIntVar(0, 3, "var");
  std::vector<IntVar*> actives;
  s->MakeIntVarArray(6, 0, 2, "active", &actives);
  s->AddConstraint(s->MakeMapDomain(var, actives));
  EXPECT_EQ(0, actives[0]->Min());
  EXPECT_EQ(0, actives[1]->Min());
  EXPECT_EQ(0, actives[2]->Min());
  EXPECT_EQ(0, actives[3]->Min());
  EXPECT_EQ(0, actives[4]->Min());
  EXPECT_EQ(0, actives[5]->Min());
  EXPECT_EQ(1, actives[0]->Max());
  EXPECT_EQ(1, actives[1]->Max());
  EXPECT_EQ(1, actives[2]->Max());
  EXPECT_EQ(1, actives[3]->Max());
  EXPECT_EQ(0, actives[4]->Max());
  EXPECT_EQ(0, actives[5]->Max());
  actives[2]->SetValue(0);
  EXPECT_FALSE(var->Contains(2));
  actives[3]->SetValue(1);
  EXPECT_TRUE(var->Bound());
  EXPECT_EQ(3, var->Value());
  EXPECT_EQ(0, actives[0]->Max());
  EXPECT_EQ(0, actives[1]->Max());
  return nullptr;
}

// ----- Test Lexical Less Constraint -----

TEST(LexicalLessTest, Ctor) {
  Solver solver("LexLessEqTest::Ctor");
  std::vector<IntVar*> left;
  solver.MakeIntVarArray(2, 0, 10, "left", &left);
  std::vector<IntVar*> right;
  solver.MakeIntVarArray(2, 0, 10, "right", &right);
  EXPECT_EQ(solver.MakeLexicalLess(left, right)->DebugString(),
            "LexicalLessOrEqual([left0(0..10), left1(0..10), IntConst(1)], "
            "[right0(0..10), right1(0..10), IntConst(0)], [1, 1, 1])");
  EXPECT_EQ(solver.MakeLexicalLessOrEqual(left, right)->DebugString(),
            "LexicalLessOrEqual([left0(0..10), left1(0..10)], "
            "[right0(0..10), right1(0..10)], [1, 1])");
  EXPECT_EQ(solver.MakeLexicalLessOrEqualWithOffsets(left, right, {10, 2})
                ->DebugString(),
            "LexicalLessOrEqual([left0(0..10), left1(0..10)], "
            "[right0(0..10), right1(0..10)], [10, 2])");
}

TEST_IN_PROPAGATION(LexicalLessTest, NonStrict, solver, true) {
  std::vector<IntVar*> left;
  solver->MakeIntVarArray(3, 0, 10, "left", &left);
  std::vector<IntVar*> right;
  solver->MakeIntVarArray(3, 0, 10, "right", &right);
  Constraint* const ct = solver->MakeLexicalLessOrEqual(left, right);
  solver->AddConstraint(ct);
  EXPECT_EQ_RANGE(left[0], 0, 10);
  EXPECT_EQ_RANGE(left[1], 0, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 0, 10);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  left[1]->SetMin(5);  // No propagation.
  EXPECT_EQ_RANGE(left[0], 0, 10);
  EXPECT_EQ_RANGE(left[1], 5, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 0, 10);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  left[0]->SetMin(3);  // Propagate to right[0].
  EXPECT_EQ_RANGE(left[0], 3, 10);
  EXPECT_EQ_RANGE(left[1], 5, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 3, 10);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  right[0]->SetMax(8);  // Propagate to left[0].
  EXPECT_EQ_RANGE(left[0], 3, 8);
  EXPECT_EQ_RANGE(left[1], 5, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 3, 8);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  right[0]->SetValue(3);  // Triggers propagation to right[1].
  EXPECT_EQ_RANGE(left[0], 3, 3);
  EXPECT_EQ_RANGE(left[1], 5, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 3, 3);
  EXPECT_EQ_RANGE(right[1], 5, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  return nullptr;
}

TEST_IN_PROPAGATION(LexicalLessTest, Strict, solver, true) {
  std::vector<IntVar*> left;
  solver->MakeIntVarArray(3, 0, 10, "left", &left);
  std::vector<IntVar*> right;
  solver->MakeIntVarArray(3, 0, 10, "right", &right);
  Constraint* const ct = solver->MakeLexicalLess(left, right);
  solver->AddConstraint(ct);
  EXPECT_EQ_RANGE(left[0], 0, 10);
  EXPECT_EQ_RANGE(left[1], 0, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 0, 10);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  left[1]->SetMin(5);  // No propagation.
  EXPECT_EQ_RANGE(left[0], 0, 10);
  EXPECT_EQ_RANGE(left[1], 5, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 0, 10);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  left[0]->SetMin(3);  // Propagates to right[0].
  EXPECT_EQ_RANGE(left[0], 3, 10);
  EXPECT_EQ_RANGE(left[1], 5, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 3, 10);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  right[0]->SetMax(8);  // Propagates to left[0].
  EXPECT_EQ_RANGE(left[0], 3, 8);
  EXPECT_EQ_RANGE(left[1], 5, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 3, 8);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  right[0]->SetValue(3);  // Triggers propagation to right[1].
  EXPECT_EQ_RANGE(left[0], 3, 3);
  EXPECT_EQ_RANGE(left[1], 5, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 3, 3);
  EXPECT_EQ_RANGE(right[1], 5, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  left[1]->SetValue(10);  // Triggers propagation to left/right[2].
  EXPECT_EQ_RANGE(left[0], 3, 3);
  EXPECT_EQ_RANGE(left[1], 10, 10);
  EXPECT_EQ_RANGE(left[2], 0, 9);
  EXPECT_EQ_RANGE(right[0], 3, 3);
  EXPECT_EQ_RANGE(right[1], 10, 10);
  EXPECT_EQ_RANGE(right[2], 1, 10);
  return nullptr;
}

TEST_IN_PROPAGATION(LexicalLessTest, StrictWithHardLimit, solver, true) {
  std::vector<IntVar*> left;
  solver->MakeIntVarArray(3, 0, 10, "left", &left);
  std::vector<IntVar*> right;
  solver->MakeIntVarArray(3, 0, 10, "right", &right);
  Constraint* const ct = solver->MakeLexicalLess(left, right);
  solver->AddConstraint(ct);
  EXPECT_EQ_RANGE(left[0], 0, 10);
  EXPECT_EQ_RANGE(left[1], 0, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 0, 10);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  left[2]->SetMin(5);  // No propagation.
  right[2]->SetMax(4);
  EXPECT_EQ_RANGE(left[0], 0, 10);
  EXPECT_EQ_RANGE(left[1], 0, 10);
  EXPECT_EQ_RANGE(left[2], 5, 10);
  EXPECT_EQ_RANGE(right[0], 0, 10);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 4);
  right[0]->SetMax(0);  // Propagates to left/right[1] as right[2] < left[2].
  EXPECT_EQ_RANGE(left[0], 0, 0);
  EXPECT_EQ_RANGE(left[1], 0, 9);
  EXPECT_EQ_RANGE(left[2], 5, 10);
  EXPECT_EQ_RANGE(right[0], 0, 0);
  EXPECT_EQ_RANGE(right[1], 1, 10);
  EXPECT_EQ_RANGE(right[2], 0, 4);
  return nullptr;
}

TEST_IN_PROPAGATION(LexicalLessTest, WithOffsets, solver, true) {
  std::vector<IntVar*> left;
  solver->MakeIntVarArray(4, 0, 10, "left", &left);
  std::vector<IntVar*> right;
  solver->MakeIntVarArray(4, 0, 10, "right", &right);
  std::vector<int64_t> offsets{2, 1, 4, 1};
  // Forcing strict less.
  left.back()->SetMin(1);
  right.back()->SetMax(0);
  Constraint* const ct =
      solver->MakeLexicalLessOrEqualWithOffsets(left, right, offsets);
  solver->AddConstraint(ct);
  EXPECT_EQ_RANGE(left[0], 0, 10);
  EXPECT_EQ_RANGE(left[1], 0, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 0, 10);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  left[1]->SetMin(5);  // No propagation.
  EXPECT_EQ_RANGE(left[0], 0, 10);
  EXPECT_EQ_RANGE(left[1], 5, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 0, 10);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  left[0]->SetMin(3);  // Propagates to right[0].
  EXPECT_EQ_RANGE(left[0], 3, 10);
  EXPECT_EQ_RANGE(left[1], 5, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 3, 10);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  right[0]->SetMax(8);  // Propagates to left[0].
  EXPECT_EQ_RANGE(left[0], 3, 8);
  EXPECT_EQ_RANGE(left[1], 5, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 3, 8);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  right[0]->SetValue(4);  // Triggers propagation to right[1] (left[0] and
                          // right[0] are close enough).
  EXPECT_EQ_RANGE(left[0], 3, 4);
  EXPECT_EQ_RANGE(left[1], 5, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 4, 4);
  EXPECT_EQ_RANGE(right[1], 5, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  left[1]->SetValue(10);  // Triggers propagation to left/right[2].
  EXPECT_EQ_RANGE(left[0], 3, 4);
  EXPECT_EQ_RANGE(left[1], 10, 10);
  EXPECT_EQ_RANGE(left[2], 0, 6);
  EXPECT_EQ_RANGE(right[0], 4, 4);
  EXPECT_EQ_RANGE(right[1], 10, 10);
  EXPECT_EQ_RANGE(right[2], 4, 10);
  return nullptr;
}

TEST_IN_PROPAGATION(LexicalLessTest, WithOffsetsWithHardLimit, solver, true) {
  std::vector<IntVar*> left;
  solver->MakeIntVarArray(3, 0, 10, "left", &left);
  std::vector<IntVar*> right;
  solver->MakeIntVarArray(3, 0, 10, "right", &right);
  std::vector<int64_t> offsets{2, 1, 4};
  Constraint* const ct =
      solver->MakeLexicalLessOrEqualWithOffsets(left, right, offsets);
  solver->AddConstraint(ct);
  EXPECT_EQ_RANGE(left[0], 0, 10);
  EXPECT_EQ_RANGE(left[1], 0, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 0, 10);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  left[2]->SetMin(5);  // No propagation.
  right[2]->SetMax(8);
  EXPECT_EQ_RANGE(left[0], 0, 10);
  EXPECT_EQ_RANGE(left[1], 0, 10);
  EXPECT_EQ_RANGE(left[2], 5, 10);
  EXPECT_EQ_RANGE(right[0], 0, 10);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 8);
  right[0]->SetMax(0);  // No propagation as right[2] and left[2] can still be
                        // equal.
  EXPECT_EQ_RANGE(left[0], 0, 0);
  EXPECT_EQ_RANGE(left[1], 0, 10);
  EXPECT_EQ_RANGE(left[2], 5, 10);
  EXPECT_EQ_RANGE(right[0], 0, 0);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 8);
  right[2]->SetMax(4);  // Propagates to left/right[1] as right[2] > left[2].
  EXPECT_EQ_RANGE(left[0], 0, 0);
  EXPECT_EQ_RANGE(left[1], 0, 9);
  EXPECT_EQ_RANGE(left[2], 5, 10);
  EXPECT_EQ_RANGE(right[0], 0, 0);
  EXPECT_EQ_RANGE(right[1], 1, 10);
  EXPECT_EQ_RANGE(right[2], 0, 4);
  return nullptr;
}

TEST_IN_PROPAGATION(IsLexicalLessTest, CtFalseImpliesFalse, solver, true) {
  std::vector<IntVar*> left;
  solver->MakeIntVarArray(3, 0, 10, "left", &left);
  std::vector<IntVar*> right;
  solver->MakeIntVarArray(3, 0, 10, "right", &right);
  std::vector<int64_t> offsets{2, 1, 4};
  IntVar* status = solver->MakeBoolVar("bool");
  Constraint* const ct = solver->MakeIsLexicalLessOrEqualWithOffsetsCt(
      left, right, offsets, status);
  solver->AddConstraint(ct);
  EXPECT_EQ_RANGE(left[0], 0, 10);
  EXPECT_EQ_RANGE(left[1], 0, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 0, 10);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  EXPECT_EQ_RANGE(status, 0, 1);
  left[0]->SetValue(2);
  right[0]->SetValue(0);  // forcing status to 0
  EXPECT_EQ_RANGE(left[0], 2, 2);
  EXPECT_EQ_RANGE(left[1], 0, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 0, 0);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  EXPECT_EQ_RANGE(status, 0, 0);
  return nullptr;
}

TEST_IN_PROPAGATION(IsLexicalLessTest, TrueImpliesCtTrue, solver, true) {
  std::vector<IntVar*> left;
  solver->MakeIntVarArray(3, 0, 10, "left", &left);
  std::vector<IntVar*> right;
  solver->MakeIntVarArray(3, 0, 10, "right", &right);
  std::vector<int64_t> offsets{2, 1, 4};
  IntVar* status = solver->MakeBoolVar("bool");
  Constraint* const ct = solver->MakeIsLexicalLessOrEqualWithOffsetsCt(
      left, right, offsets, status);
  solver->AddConstraint(ct);
  EXPECT_EQ_RANGE(left[0], 0, 10);
  EXPECT_EQ_RANGE(left[1], 0, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 0, 10);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  EXPECT_EQ_RANGE(status, 0, 1);
  left[0]->SetMin(1);  // no propagation since status is unknown
  EXPECT_EQ_RANGE(left[0], 1, 10);
  EXPECT_EQ_RANGE(left[1], 0, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 0, 10);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  EXPECT_EQ_RANGE(status, 0, 1);
  status->SetValue(1);  // enforce lexLE
  EXPECT_EQ_RANGE(left[0], 1, 10);
  EXPECT_EQ_RANGE(left[1], 0, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 1, 10);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  EXPECT_EQ_RANGE(status, 1, 1);
  return nullptr;
}

TEST_IN_PROPAGATION(IsLexicalLessTest, FalseDoesNotImplyCtFalse, solver, true) {
  std::vector<IntVar*> left;
  solver->MakeIntVarArray(3, 0, 10, "left", &left);
  std::vector<IntVar*> right;
  solver->MakeIntVarArray(3, 0, 10, "right", &right);
  std::vector<int64_t> offsets{2, 1, 4};
  IntVar* status = solver->MakeBoolVar("bool");
  Constraint* const ct = solver->MakeIsLexicalLessOrEqualWithOffsetsCt(
      left, right, offsets, status);
  solver->AddConstraint(ct);
  EXPECT_EQ_RANGE(left[0], 0, 10);
  EXPECT_EQ_RANGE(left[1], 0, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 0, 10);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  EXPECT_EQ_RANGE(status, 0, 1);
  left[0]->SetMin(1);  // no propagation since status is unknown
  EXPECT_EQ_RANGE(left[0], 1, 10);
  EXPECT_EQ_RANGE(left[1], 0, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 0, 10);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  EXPECT_EQ_RANGE(status, 0, 1);
  status->SetValue(0);  // enforces nothing on lexLE
  EXPECT_EQ_RANGE(left[0], 1, 10);
  EXPECT_EQ_RANGE(left[1], 0, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 0, 10);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  EXPECT_EQ_RANGE(status, 0, 0);
  return nullptr;
}

TEST_IN_PROPAGATION(IsLexicalLessTest, CtTrueDoesNotImplyTrue, solver, true) {
  std::vector<IntVar*> left;
  solver->MakeIntVarArray(3, 0, 10, "left", &left);
  std::vector<IntVar*> right;
  solver->MakeIntVarArray(3, 0, 10, "right", &right);
  std::vector<int64_t> offsets{2, 1, 4};
  IntVar* status = solver->MakeBoolVar("bool");
  Constraint* const ct = solver->MakeIsLexicalLessOrEqualWithOffsetsCt(
      left, right, offsets, status);
  solver->AddConstraint(ct);
  EXPECT_EQ_RANGE(left[0], 0, 10);
  EXPECT_EQ_RANGE(left[1], 0, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 0, 10);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  EXPECT_EQ_RANGE(status, 0, 1);
  left[0]->SetValue(0);
  right[0]->SetValue(4);  // left < right enforces nothing on status
  EXPECT_EQ_RANGE(left[0], 0, 0);
  EXPECT_EQ_RANGE(left[1], 0, 10);
  EXPECT_EQ_RANGE(left[2], 0, 10);
  EXPECT_EQ_RANGE(right[0], 4, 4);
  EXPECT_EQ_RANGE(right[1], 0, 10);
  EXPECT_EQ_RANGE(right[2], 0, 10);
  EXPECT_EQ_RANGE(status, 0, 1);
  return nullptr;
}

// Test inverse.

TEST(InversePermutation, Ctor) {
  const int kSize = 10;
  Solver solver("InversePermutation::Ctor");
  std::vector<IntVar*> left;
  solver.MakeIntVarArray(kSize, 0, kSize - 1, "left", &left);
  std::vector<IntVar*> right;
  solver.MakeIntVarArray(kSize, 0, kSize - 1, "right", &right);
  Constraint* const ct = solver.MakeInversePermutationConstraint(left, right);
  EXPECT_FALSE(ct->DebugString().empty());
}

TEST_IN_PROPAGATION(InversePermutation, Propagation, solver, true) {
  const int kSize = 10;
  std::vector<IntVar*> left;
  solver->MakeIntVarArray(kSize, 0, kSize, "left", &left);
  std::vector<IntVar*> right;
  solver->MakeIntVarArray(kSize, 0, kSize, "right", &right);
  Constraint* const ct = solver->MakeInversePermutationConstraint(left, right);
  solver->AddConstraint(ct);
  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ_RANGE(left[i], 0, kSize - 1);
    EXPECT_EQ_RANGE(right[i], 0, kSize - 1);
  }
  right[3]->RemoveValue(2);
  EXPECT_FALSE(left[2]->Contains(3));
  left[5]->RemoveValue(4);
  EXPECT_FALSE(right[4]->Contains(5));
  return nullptr;
}

TEST_IN_PROPAGATION(InversePermutation, OverlapLeft, solver, false) {
  const int kSize = 10;
  IntVar* var1 = nullptr;
  IntVar* var2 = nullptr;
  {  // As it will fail, we need to make sure we do not leak vectors.
    std::vector<IntVar*> left;
    std::vector<IntVar*> right;
    solver->MakeIntVarArray(kSize, 0, kSize, "left", &left);
    solver->MakeIntVarArray(kSize, 0, kSize, "right", &right);
    Constraint* const ct =
        solver->MakeInversePermutationConstraint(left, right);
    solver->AddConstraint(ct);
    var1 = left[3];
    var2 = left[6];
  }
  var1->SetValue(5);
  var2->SetValue(5);
  return nullptr;
}

TEST_IN_PROPAGATION(InversePermutation, OverlapRight, solver, false) {
  const int kSize = 10;
  IntVar* var1 = nullptr;
  IntVar* var2 = nullptr;
  {  // As it will fail, we need to make sure we do not leak vectors.
    std::vector<IntVar*> left;
    std::vector<IntVar*> right;
    solver->MakeIntVarArray(kSize, 0, kSize, "left", &left);
    solver->MakeIntVarArray(kSize, 0, kSize, "right", &right);
    Constraint* const ct =
        solver->MakeInversePermutationConstraint(left, right);
    solver->AddConstraint(ct);
    var1 = right[3];
    var2 = right[6];
  }
  var1->SetValue(5);
  var2->SetValue(5);
  return nullptr;
}

TEST_IN_PROPAGATION(InversePermutation, TriangularMatrix, solver, true) {
  const int kSize = 100;
  std::vector<IntVar*> left(kSize);
  for (int i = 0; i < kSize / 2; ++i) {
    left[i] = solver->MakeIntVar(kSize - 1 - i, kSize - 1);
  }
  for (int i = kSize / 2; i < kSize; ++i) {
    left[i] = solver->MakeIntVar(0, kSize - 1);
  }
  std::vector<IntVar*> right(kSize);
  for (int i = 0; i < kSize / 2; ++i) {
    right[i] = solver->MakeIntVar(kSize - 1 - i, kSize - 1);
  }
  for (int i = kSize / 2; i < kSize; ++i) {
    right[i] = solver->MakeIntVar(0, kSize - 1);
  }
  Constraint* const ct = solver->MakeInversePermutationConstraint(left, right);
  solver->AddConstraint(ct);
  for (int i = 0; i < kSize; ++i) {
    EXPECT_BOUND_TO(left[i], kSize - 1 - i);
    EXPECT_BOUND_TO(right[i], kSize - 1 - i);
  }
  return nullptr;
}

TEST_IN_PROPAGATION(IndexOfMaxValue, Propagate, solver, true) {
  const int kSize = 100;
  const int kMax = 20;
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(kSize, 0, kMax, "vars", &vars);
  IntVar* const index = solver->MakeIntVar(0, kSize, "index");
  Constraint* const ct =
      solver->MakeIndexOfFirstMaxValueConstraint(index, vars);
  solver->AddConstraint(ct);
  // Adding the constraint should just set a max on the index.
  EXPECT_EQ_RANGE(index, 0, kSize - 1);
  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ_RANGE(vars[i], 0, kMax);
  }

  // This would propagate before the min of the index.
  const int kOffset = 10;
  index->SetRange(kOffset, kSize - kOffset);
  for (int i = 0; i < kOffset; ++i) {
    EXPECT_EQ_RANGE(vars[i], 0, kMax - 1);
  }
  for (int i = kOffset; i < kSize; ++i) {
    EXPECT_EQ_RANGE(vars[i], 0, kMax);
  }

  // Changing the min of a variable should have no effect.
  const int kPivot = kSize / 2;
  vars[kPivot]->SetMin(kMax / 2);

  EXPECT_EQ_RANGE(index, kOffset, kSize - kOffset);
  for (int i = 0; i < kOffset; ++i) {
    EXPECT_EQ_RANGE(vars[i], 0, kMax - 1);
  }
  for (int i = kOffset; i < kSize; ++i) {
    if (i != kPivot) {
      EXPECT_EQ_RANGE(vars[i], 0, kMax);
    }
  }

  // Excluding the variable corresponding to the min of the index should
  // remove it from the possible index values.
  vars[kOffset]->SetMax(kMax / 2 - 1);
  EXPECT_EQ_RANGE(index, kOffset + 1, kSize - kOffset);

  // Excluding a variable 1 below the max of the index should do nothing.
  vars[kSize - kOffset - 1]->SetMax(kMax / 2 - 1);
  EXPECT_EQ_RANGE(index, kOffset + 1, kSize - kOffset);

  // Until the max of the index is decreased by one, then it should move one
  // more step down.
  index->SetMax(kSize - kOffset - 1);
  EXPECT_EQ_RANGE(index, kOffset + 1, kSize - kOffset - 2);

  // Move the max of all 'active' variables down should move all the rest down
  // too.
  for (int i = kOffset + 1; i <= kSize - kOffset - 2; ++i) {
    vars[i]->SetRange(0, kMax - 2);
  }
  for (int i = 0; i < kOffset; ++i) {
    EXPECT_EQ_RANGE(vars[i], 0, kMax - 3);
  }
  EXPECT_EQ_RANGE(vars[kOffset], 0, kMax / 2 - 1);
  EXPECT_EQ_RANGE(vars[kSize - kOffset - 1], 0, kMax / 2 - 1);
  for (int i = kSize - kOffset; i < kSize; ++i) {
    EXPECT_EQ_RANGE(vars[i], 0, kMax - 2);
  }

  return nullptr;
}

TEST_IN_PROPAGATION(IndexOfMinValue, Propagate, solver, true) {
  const int kSize = 100;
  const int kMax = 20;
  std::vector<IntVar*> vars;
  solver->MakeIntVarArray(kSize, 0, kMax, "vars", &vars);
  IntVar* const index = solver->MakeIntVar(0, kSize, "index");
  Constraint* const ct =
      solver->MakeIndexOfFirstMinValueConstraint(index, vars);
  solver->AddConstraint(ct);
  // Adding the constraint should just set a max on the index.
  EXPECT_EQ_RANGE(index, 0, kSize - 1);
  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ_RANGE(vars[i], 0, kMax);
  }

  // This would propagate before the min of the index.
  const int kOffset = 10;
  index->SetRange(kOffset, kSize - kOffset);
  for (int i = 0; i < kOffset; ++i) {
    EXPECT_EQ_RANGE(vars[i], 1, kMax);
  }
  for (int i = kOffset; i < kSize; ++i) {
    EXPECT_EQ_RANGE(vars[i], 0, kMax);
  }

  // Changing the max of a variable should have no effect.
  const int kPivot = kSize / 2;
  vars[kPivot]->SetMax(kMax / 2);

  EXPECT_EQ_RANGE(index, kOffset, kSize - kOffset);
  for (int i = 0; i < kOffset; ++i) {
    EXPECT_EQ_RANGE(vars[i], 1, kMax);
  }
  for (int i = kOffset; i < kSize; ++i) {
    if (i != kPivot) {
      EXPECT_EQ_RANGE(vars[i], 0, kMax);
    }
  }

  // Excluding the variable corresponding to the min of the index should
  // remove it from the possible index values.
  vars[kOffset]->SetMin(kMax / 2 + 1);
  EXPECT_EQ_RANGE(index, kOffset + 1, kSize - kOffset);

  // Excluding a variable 1 below the max of the index should do nothing.
  vars[kSize - kOffset - 1]->SetMin(kMax / 2 + 1);
  EXPECT_EQ_RANGE(index, kOffset + 1, kSize - kOffset);

  // Until the max of the index is decreased by one, then it should move one
  // more step down.
  index->SetMax(kSize - kOffset - 1);
  EXPECT_EQ_RANGE(index, kOffset + 1, kSize - kOffset - 2);

  // Move the min of all 'active' variables down should move all the rest down
  // too.
  for (int i = kOffset + 1; i <= kSize - kOffset - 2; ++i) {
    vars[i]->SetRange(2, kMax);
  }
  for (int i = 0; i < kOffset; ++i) {
    EXPECT_EQ_RANGE(vars[i], 3, kMax);
  }
  EXPECT_EQ_RANGE(vars[kOffset], kMax / 2 + 1, kMax);
  EXPECT_EQ_RANGE(vars[kSize - kOffset - 1], kMax / 2 + 1, kMax);
  for (int i = kSize - kOffset; i < kSize; ++i) {
    EXPECT_EQ_RANGE(vars[i], 2, kMax);
  }

  return nullptr;
}
}  // namespace
}  // namespace operations_research
