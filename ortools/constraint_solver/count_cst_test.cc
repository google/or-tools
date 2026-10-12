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
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solver_test_util.h"

namespace {
class BoundedDistributeTest_TestCardDown_Test;
}
namespace {
class BoundedDistributeTest_TestCardUp_Test;
}
namespace {
class BoundedDistributeTest_TestNoCard_Test;
}
namespace {
class BoundedDistributeTest_TestSetBoundFromCardMin_Test;
}
namespace {
class BoundedDistributeTest_TestWrongCardMinMax_Test;
}
namespace {
class BoundedDistributeTest_TestWrongMinCard_Test;
}
namespace {
class BoundedDistributeTest_TestZeroCard_Test;
}

namespace cp = operations_research;
using cp::Constraint;
using cp::Decision;
using cp::DecisionBuilder;
using cp::IntExpr;
using cp::IntVar;
using cp::Solver;

namespace {
class CountCstTest : public testing::Test {
 protected:
  CountCstTest() : solver_(new Solver("ConstraintTest")) {}
  std::unique_ptr<Solver> solver_;
};

TEST_F(CountCstTest, MagicSequence) {
  const int n = 20;
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(20, 0, n, "var_", &vars);
  for (int i = 0; i < n; ++i) {
    Constraint* ct = solver_->MakeCount(vars, i, vars[i]);
    solver_->AddConstraint(ct);
  }
  IntExpr* sum = solver_->MakeSum(vars);
  Constraint* ct = solver_->MakeEquality(sum, n);
  solver_->AddConstraint(ct);
  DecisionBuilder* db = solver_->MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                                           Solver::ASSIGN_MIN_VALUE);
  solver_->NewSearch(db);
  ASSERT_TRUE(solver_->NextSolution());
  EXPECT_EQ(n - 4, vars[0]->Value());
  EXPECT_EQ(2, vars[1]->Value());
  EXPECT_EQ(1, vars[2]->Value());
  EXPECT_EQ(1, vars[n - 4]->Value());
  EXPECT_EQ(0, vars[3]->Value());
  EXPECT_EQ(0, vars[n - 1]->Value());
  solver_->EndSearch();
}

TEST_F(CountCstTest, MagicSequenceDistribute) {
  const int n = 20;
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(20, 0, n, "var_", &vars);
  std::vector<int64_t> values(20);
  for (int i = 0; i < n; ++i) {
    values[i] = i;
  }
  Constraint* dis = solver_->MakeDistribute(vars, values, vars);
  solver_->AddConstraint(dis);
  IntExpr* sum = solver_->MakeSum(vars);
  Constraint* ct = solver_->MakeEquality(sum, n);
  solver_->AddConstraint(ct);
  DecisionBuilder* db = solver_->MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                                           Solver::ASSIGN_MIN_VALUE);
  solver_->NewSearch(db);
  ASSERT_TRUE(solver_->NextSolution());
  EXPECT_FALSE(dis->DebugString().empty());
  EXPECT_EQ(n - 4, vars[0]->Value());
  EXPECT_EQ(2, vars[1]->Value());
  EXPECT_EQ(1, vars[2]->Value());
  EXPECT_EQ(1, vars[n - 4]->Value());
  EXPECT_EQ(0, vars[3]->Value());
  EXPECT_EQ(0, vars[n - 1]->Value());
  solver_->EndSearch();
}

TEST_F(CountCstTest, MagicSequenceDistributeNoValues) {
  const int n = 20;
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(20, 0, n, "var_", &vars);
  Constraint* dis = solver_->MakeDistribute(vars, vars);
  solver_->AddConstraint(dis);
  IntExpr* sum = solver_->MakeSum(vars);
  Constraint* ct = solver_->MakeEquality(sum, n);
  solver_->AddConstraint(ct);
  DecisionBuilder* db = solver_->MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                                           Solver::ASSIGN_MIN_VALUE);
  solver_->NewSearch(db);
  ASSERT_TRUE(solver_->NextSolution());
  EXPECT_FALSE(dis->DebugString().empty());
  EXPECT_EQ(n - 4, vars[0]->Value());
  EXPECT_EQ(2, vars[1]->Value());
  EXPECT_EQ(1, vars[2]->Value());
  EXPECT_EQ(1, vars[n - 4]->Value());
  EXPECT_EQ(0, vars[3]->Value());
  EXPECT_EQ(0, vars[n - 1]->Value());
  solver_->EndSearch();
}

TEST_F(CountCstTest, MagicSequenceDistributeReversed) {
  const int n = 20;
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(20, 0, n, "var_", &vars);
  std::vector<IntVar*> cards(20);
  std::vector<int64_t> values(20);
  for (int i = 0; i < n; ++i) {
    values[i] = 19 - i;
    cards[i] = vars[19 - i];
  }
  Constraint* dis = solver_->MakeDistribute(vars, values, cards);
  solver_->AddConstraint(dis);
  IntExpr* sum = solver_->MakeSum(vars);
  Constraint* ct = solver_->MakeEquality(sum, n);
  solver_->AddConstraint(ct);
  DecisionBuilder* db = solver_->MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                                           Solver::ASSIGN_MIN_VALUE);
  solver_->NewSearch(db);
  ASSERT_TRUE(solver_->NextSolution());
  EXPECT_FALSE(dis->DebugString().empty());
  EXPECT_EQ(n - 4, vars[0]->Value());
  EXPECT_EQ(2, vars[1]->Value());
  EXPECT_EQ(1, vars[2]->Value());
  EXPECT_EQ(1, vars[n - 4]->Value());
  EXPECT_EQ(0, vars[3]->Value());
  EXPECT_EQ(0, vars[n - 1]->Value());
  solver_->EndSearch();
}

TEST_F(CountCstTest, FastDistributeUnequalArray) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(20, 0, 9, "var_", &vars);
  std::vector<IntVar*> cards;
  solver_->MakeIntVarArray(10, 0, 2, "var_", &cards);
  Constraint* dis = solver_->MakeDistribute(vars, cards);
  solver_->AddConstraint(dis);
  DecisionBuilder* db = solver_->MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                                           Solver::ASSIGN_MIN_VALUE);
  solver_->NewSearch(db);
  ASSERT_TRUE(solver_->NextSolution());
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(2, cards[i]->Value());
  }
  solver_->EndSearch();
}

TEST_F(CountCstTest, DistributeUnequalArray) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(20, 0, 9, "var_", &vars);
  std::vector<IntVar*> cards;
  solver_->MakeIntVarArray(10, 0, 2, "var_", &cards);
  std::vector<int64_t> values(10);
  for (int i = 0; i < 10; ++i) {
    values[i] = 9 - i;
  }
  Constraint* dis = solver_->MakeDistribute(vars, values, cards);
  solver_->AddConstraint(dis);
  DecisionBuilder* db = solver_->MakePhase(vars, Solver::CHOOSE_FIRST_UNBOUND,
                                           Solver::ASSIGN_MIN_VALUE);
  solver_->NewSearch(db);
  ASSERT_TRUE(solver_->NextSolution());
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(2, cards[i]->Value());
  }
  solver_->EndSearch();
}

TEST_IN_PROPAGATION0(CountCstTest, CountValueEqDown, s, true) {
  const int n = 5;
  std::vector<IntVar*> vars;
  s->MakeIntVarArray(5, 0, n, "var_", &vars);
  Constraint* ct = s->MakeCount(vars, 2, 3);
  EXPECT_FALSE(ct->DebugString().empty());
  s->AddConstraint(ct);
  for (int i = 0; i < n; ++i) {
    EXPECT_EQ(n + 1, vars[i]->Size());
  }
  vars[0]->RemoveValue(2);
  for (int i = 1; i < n; ++i) {
    EXPECT_EQ(n + 1, vars[i]->Size());
  }
  EXPECT_EQ(n, vars[0]->Size());
  vars[1]->RemoveValue(2);
  for (int i = 2; i < n; ++i) {
    EXPECT_EQ(1, vars[i]->Size());
    EXPECT_EQ(2, vars[i]->Value());
  }
  EXPECT_EQ(n, vars[0]->Size());
  EXPECT_EQ(n, vars[1]->Size());
  return nullptr;
};

TEST_IN_PROPAGATION0(CountCstTest, CountValueEqUp, s, true) {
  const int n = 5;
  std::vector<IntVar*> vars;
  s->MakeIntVarArray(5, 0, n, "var_", &vars);
  Constraint* ct = s->MakeCount(vars, 2, 3);
  s->AddConstraint(ct);
  for (int i = 0; i < n; ++i) {
    EXPECT_EQ(n + 1, vars[i]->Size());
  }
  vars[0]->SetValue(2);
  for (int i = 1; i < n; ++i) {
    EXPECT_EQ(n + 1, vars[i]->Size());
  }
  EXPECT_EQ(1, vars[0]->Size());
  vars[1]->SetValue(2);
  for (int i = 2; i < n; ++i) {
    EXPECT_EQ(n + 1, vars[i]->Size());
  }
  EXPECT_EQ(vars[1]->Size(), 1);
  vars[2]->SetValue(2);
  for (int i = 3; i < n; ++i) {
    EXPECT_EQ(n, vars[i]->Size());
    EXPECT_FALSE(vars[i]->Contains(2));
  }
  return nullptr;
};

// Test Bounded Distribute

static const int kBoundedDistributeVars = 10;

class BoundedDistributeTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("BoundedDistributeTest");
    std::vector<int> values = {-5, 0, 1, 3, 4};
    for (int i = 0; i < kBoundedDistributeVars; ++i) {
      vars_.push_back(solver_->MakeIntVar(values));
    }
    values_ = {-5, 0, 1, 3, 4};
  }

  std::vector<int> values_;
  std::vector<int> mins_;
  std::vector<int> maxes_;
  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
};

TEST_IN_PROPAGATION1(BoundedDistributeTest, NoCard, s, false,
                     const std::vector<IntVar*>&, vars_) {
  Constraint* ct = nullptr;
  {
    std::vector<int> values = {-5, 0, 1, 3, 4};
    std::vector<int> mins(kBoundedDistributeVars / 2, 0);
    std::vector<int> maxes(kBoundedDistributeVars / 2, 0);
    ct = s->MakeDistribute(vars_, values, mins, maxes);
  }
  s->AddConstraint(ct);
  return nullptr;
}

TEST_IN_PROPAGATION1(BoundedDistributeTest, WrongMinCard, s, false,
                     const std::vector<IntVar*>&, vars_) {
  Constraint* ct = nullptr;
  {
    std::vector<int> values = {-5, 0, 1, 3, 4};
    std::vector<int> mins(kBoundedDistributeVars / 2, 3);
    std::vector<int> maxes(kBoundedDistributeVars / 2, 3);
    ct = s->MakeDistribute(vars_, values, mins, maxes);
  }
  s->AddConstraint(ct);
  return nullptr;
}

TEST_IN_PROPAGATION1(BoundedDistributeTest, WrongCardMinMax, s, false,
                     const std::vector<IntVar*>&, vars_) {
  Constraint* ct = nullptr;
  {
    std::vector<int> values = {-5, 0, 1, 3, 4};
    std::vector<int> mins(kBoundedDistributeVars / 2, 2);
    std::vector<int> maxes(kBoundedDistributeVars / 2, 1);
    ct = s->MakeDistribute(vars_, values, mins, maxes);
  }
  s->AddConstraint(ct);
  return nullptr;
}

TEST_IN_PROPAGATION1(BoundedDistributeTest, CardDown, s, true,
                     const std::vector<IntVar*>&, vars_) {
  std::vector<int> values = {-5, 0, 1, 3, 4};
  std::vector<int> mins(kBoundedDistributeVars / 2, 2);
  std::vector<int> maxes(kBoundedDistributeVars / 2, 2);
  s->AddConstraint(s->MakeDistribute(vars_, values, mins, maxes));
  for (int i = 0; i < kBoundedDistributeVars - 2; ++i) {
    vars_[i]->RemoveValue(4);
  }
  EXPECT_EQ(4, vars_[kBoundedDistributeVars - 2]->Min());
  EXPECT_EQ(4, vars_[kBoundedDistributeVars - 1]->Min());
  return nullptr;
}

TEST_IN_PROPAGATION1(BoundedDistributeTest, CardUp, s, true,
                     const std::vector<IntVar*>&, vars_) {
  std::vector<int> values = {-5, 0, 1, 3, 4};
  std::vector<int> mins(kBoundedDistributeVars / 2, 2);
  std::vector<int> maxes(kBoundedDistributeVars / 2, 2);
  s->AddConstraint(s->MakeDistribute(vars_, values, mins, maxes));
  vars_[kBoundedDistributeVars - 2]->SetValue(4);
  vars_[kBoundedDistributeVars - 1]->SetValue(4);
  for (int i = 0; i < kBoundedDistributeVars - 2; ++i) {
    EXPECT_EQ(3, vars_[i]->Max());
  }
  return nullptr;
}

TEST_IN_PROPAGATION1(BoundedDistributeTest, ZeroCard, s, true,
                     const std::vector<IntVar*>&, vars_) {
  std::vector<int> values = {1};
  std::vector<int> mins(1, 0);
  std::vector<int> maxes(1, 0);
  s->AddConstraint(s->MakeDistribute(vars_, values, mins, maxes));
  for (int i = 0; i < kBoundedDistributeVars; ++i) {
    EXPECT_FALSE(vars_[i]->Contains(1));
  }
  return nullptr;
}

TEST_IN_PROPAGATION1(BoundedDistributeTest, SetBoundFromCardMin, s, true,
                     const std::vector<IntVar*>&, vars_) {
  std::vector<int> values = {3, 4};
  std::vector<int> mins(2, 5);
  std::vector<int> maxes(2, 5);

  s->AddConstraint(s->MakeDistribute(vars_, values, mins, maxes));
  for (int i = 0; i < kBoundedDistributeVars; ++i) {
    EXPECT_EQ(3, vars_[i]->Min());
  }
  return nullptr;
}

TEST_F(BoundedDistributeTest, SpecialCtor) {
  std::vector<int64_t> values = {3, 4};
  std::vector<int64_t> mins(2, 5);
  std::vector<int64_t> maxes(2, 5);

  Solver s("BoundedDistributeTest::TestSpecialCtor");
  Constraint* const ct = s.MakeDistribute(vars_, values, mins, maxes);
  EXPECT_FALSE(ct->DebugString().empty());
}

// Test Fast Bounded Distribute

static const int kFastBoundedDistributeVars = 10;
static const int kFastBoundedDistributeVarMax = 4;

class FastBoundedDistributeTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("FastBoundedDistributeTest");
    solver_->MakeIntVarArray(kFastBoundedDistributeVars, 0,
                             kFastBoundedDistributeVarMax, &vars_);
  }
  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
};

TEST_IN_PROPAGATION1(FastBoundedDistributeTest, NoCard, s, false,
                     const std::vector<IntVar*>&, vars_) {
  s->AddConstraint(
      s->MakeDistribute(vars_, 0, 0, kFastBoundedDistributeVars / 2));
  return nullptr;
}

TEST_IN_PROPAGATION1(FastBoundedDistributeTest, WrongMinCard, s, false,
                     const std::vector<IntVar*>&, vars_) {
  s->AddConstraint(
      s->MakeDistribute(vars_, 3, 3, kFastBoundedDistributeVars / 2));
  return nullptr;
}

TEST_IN_PROPAGATION1(FastBoundedDistributeTest, WrongCardMinMax, s, false,
                     const std::vector<IntVar*>&, vars_) {
  s->AddConstraint(
      s->MakeDistribute(vars_, 2, 1, kFastBoundedDistributeVars / 2));
  return nullptr;
}

TEST_IN_PROPAGATION1(FastBoundedDistributeTest, CardDown, s, true,
                     const std::vector<IntVar*>&, vars_) {
  s->AddConstraint(
      s->MakeDistribute(vars_, 2, 2, kFastBoundedDistributeVars / 2));
  for (int i = 0; i < kFastBoundedDistributeVars - 2; ++i) {
    vars_[i]->RemoveValue(kFastBoundedDistributeVarMax);
  }
  EXPECT_EQ(kFastBoundedDistributeVarMax,
            vars_[kFastBoundedDistributeVars - 2]->Min());
  EXPECT_EQ(kFastBoundedDistributeVarMax,
            vars_[kFastBoundedDistributeVars - 1]->Min());
  return nullptr;
}

TEST_IN_PROPAGATION1(FastBoundedDistributeTest, CardUp, s, true,
                     const std::vector<IntVar*>&, vars_) {
  s->AddConstraint(
      s->MakeDistribute(vars_, 2, 2, kFastBoundedDistributeVars / 2));
  vars_[kFastBoundedDistributeVars - 2]->SetValue(kFastBoundedDistributeVarMax);
  vars_[kFastBoundedDistributeVars - 1]->SetValue(kFastBoundedDistributeVarMax);
  for (int i = 0; i < kFastBoundedDistributeVars - 2; ++i) {
    EXPECT_EQ(kFastBoundedDistributeVarMax - 1, vars_[i]->Max());
  }
  return nullptr;
}

TEST_IN_PROPAGATION1(FastBoundedDistributeTest, ZeroCard, s, true,
                     const std::vector<IntVar*>&, vars_) {
  s->AddConstraint(
      s->MakeDistribute(vars_, 0, 0, kFastBoundedDistributeVarMax));
  for (int i = 0; i < kFastBoundedDistributeVars; ++i) {
    EXPECT_EQ(kFastBoundedDistributeVarMax, vars_[i]->Min());
  }
  return nullptr;
}

TEST_IN_PROPAGATION1(FastBoundedDistributeTest, SetBoundFromCardMin, s, true,
                     const std::vector<IntVar*>&, vars_) {
  s->AddConstraint(
      s->MakeDistribute(vars_, 5, 5, kFastBoundedDistributeVars / 5));
  for (int i = 0; i < kFastBoundedDistributeVars; ++i) {
    EXPECT_EQ(1, vars_[i]->Max());
  }
  return nullptr;
}

TEST_F(FastBoundedDistributeTest, SpecialCtor) {
  EXPECT_EQ(solver_->MakeTrueConstraint(),
            solver_->MakeDistribute(vars_, 0, kFastBoundedDistributeVars, 4));
  EXPECT_EQ(solver_->MakeFalseConstraint(),
            solver_->MakeDistribute(vars_, 0, -1, 4));
  EXPECT_EQ(solver_->MakeFalseConstraint(),
            solver_->MakeDistribute(vars_, 2, 1, 4));
  EXPECT_EQ(solver_->MakeFalseConstraint(),
            solver_->MakeDistribute(vars_, kFastBoundedDistributeVars + 1,
                                    kFastBoundedDistributeVars + 2, 4));
}

TEST_F(FastBoundedDistributeTest, SpecialCtorArray) {
  std::vector<int> zeros(5, 0);
  std::vector<int> ones(5, 1);
  std::vector<int> twos(5, 2);
  std::vector<int> big_plus_1(5, kFastBoundedDistributeVars + 1);
  std::vector<int> big_plus_2(5, kFastBoundedDistributeVars + 2);
  std::vector<int> minus_ones(5, -1);

  EXPECT_EQ(solver_->MakeTrueConstraint(),
            solver_->MakeDistribute(vars_, zeros, big_plus_1));
  EXPECT_EQ(solver_->MakeFalseConstraint(),
            solver_->MakeDistribute(vars_, zeros, minus_ones));
  EXPECT_EQ(solver_->MakeFalseConstraint(),
            solver_->MakeDistribute(vars_, big_plus_1, big_plus_2));
}

TEST(AtMostTest, Ctor) {
  Solver solver("AtMostTest::Ctor");
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(10, 0, 10, "vars", &vars);
  Constraint* at_most = solver.MakeAtMost(vars, 2, 5);
  EXPECT_FALSE(at_most->DebugString().empty());
}

TEST_IN_PROPAGATION(AtMostTest, CardUp, s, true) {
  std::vector<IntVar*> vars;
  const int kValue = 5;
  const int kOtherValue = 3;
  const int kMaxCount = 2;
  s->MakeIntVarArray(10, 0, 10, "vars", &vars);
  Constraint* const at_most = s->MakeAtMost(vars, kValue, kMaxCount);
  s->AddConstraint(at_most);
  vars[0]->SetValue(kValue);
  vars[1]->SetValue(kOtherValue);
  for (int i = 2; i < 10; ++i) {
    EXPECT_TRUE(vars[i]->Contains(kValue));
  }
  vars[9]->SetValue(kValue);
  for (int i = 2; i < 9; ++i) {
    EXPECT_FALSE(vars[i]->Contains(kValue));
  }
  return nullptr;
}

class AtMostFailingTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("AtMostFailingTest");
    solver_->MakeIntVarArray(10, 0, 10, &vars_);
  }
  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
};

TEST_IN_PROPAGATION1(AtMostFailingTest, CardOver, s, false,
                     const std::vector<IntVar*>&, vars_) {
  std::vector<IntVar*> vars;
  const int kValue = 5;
  const int kMaxCount = 2;
  Constraint* const at_most = s->MakeAtMost(vars_, kValue, kMaxCount);
  s->AddConstraint(at_most);
  vars_[0]->SetValue(kValue);
  vars_[1]->SetValue(kValue);
  vars_[9]->SetValue(kValue);
  return nullptr;
}
}  // namespace
