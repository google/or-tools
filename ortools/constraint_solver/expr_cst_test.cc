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

#include "absl/log/log.h"
#include "gtest/gtest.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solver_test_util.h"
#include "ortools/util/sorted_interval_list.h"

namespace {
class LargeDiffCstTest_TestSetExactMaxLarge_Test;
}
namespace {
class LargeDiffCstTest_TestSetExactMax_Test;
}
namespace {
class LargeDiffCstTest_TestSetExactMinLarge_Test;
}
namespace {
class LargeDiffCstTest_TestSetExactMin_Test;
}
namespace {
class LargeDiffCstTest_TestSetMaxLarge_Test;
}
namespace {
class LargeDiffCstTest_TestSetMax_Test;
}
namespace {
class LargeDiffCstTest_TestSetMinLarge_Test;
}
namespace {
class LargeDiffCstTest_TestSetMin_Test;
}

namespace operations_research {
namespace {

class ExprCstTest : public testing::Test {
 protected:
  ExprCstTest() : solver_(new Solver("ExprCstTest")) {}
  std::unique_ptr<Solver> solver_;
};

TEST_F(ExprCstTest, Display) {
  const int n = 5;
  std::vector<IntVar*> vars;
  Solver* const s = solver_.get();
  s->MakeIntVarArray(n, 0, n, "var_", &vars);
  IntExpr* sum1 = s->MakeSum(vars);
  LOG(INFO) << "Please ignore: " << s->MakeEquality(sum1, 4);
  LOG(INFO) << "Please ignore: " << s->MakeNonEquality(sum1->Var(), 4);
  LOG(INFO) << "Please ignore: " << s->MakeGreaterOrEqual(sum1, 4);
  LOG(INFO) << "Please ignore: " << s->MakeLessOrEqual(sum1, 4);
  LOG(INFO) << "Please ignore: " << s->MakeIsEqualCstCt(vars[0], 0, vars[1]);
  LOG(INFO) << "Please ignore: "
            << s->MakeIsDifferentCstCt(vars[0], 0, vars[1]);
  LOG(INFO) << "Please ignore: "
            << s->MakeIsGreaterOrEqualCstCt(vars[0], 0, vars[1]);
  LOG(INFO) << "Please ignore: " << s->MakeIsGreaterCstCt(vars[0], 0, vars[1]);
  LOG(INFO) << "Please ignore: "
            << s->MakeIsLessOrEqualCstCt(vars[0], 0, vars[1]);
  LOG(INFO) << "Please ignore: " << s->MakeIsLessCstCt(vars[0], 0, vars[1]);
  LOG(INFO) << "Please ignore: " << s->MakeIsBetweenCt(vars[0], 1, 2, vars[1]);
}

class GreaterEqExprCstTest : public testing::Test {
 protected:
  GreaterEqExprCstTest() : solver_(new Solver("GreaterEqExprCstTest")) {}
  std::unique_ptr<Solver> solver_;
};

TEST_F(GreaterEqExprCstTest, AlternateCtor) {
  IntVar* var = solver_->MakeIntVar(0, 10, "var");
  Constraint* ct = solver_->MakeGreaterOrEqual(var, int64_t{10});
  EXPECT_FALSE(ct->DebugString().empty());
  ct = solver_->MakeGreater(var, 10);
  EXPECT_FALSE(ct->DebugString().empty());
  ct = solver_->MakeGreater(var, int64_t{10});
  EXPECT_FALSE(ct->DebugString().empty());
}

TEST_IN_PROPAGATION0(GreaterEqExprCstTest, ForceMin, s, true) {
  const int n = 5;
  std::vector<IntVar*> vars;
  s->MakeIntVarArray(n, 0, n, "var_", &vars);
  IntExpr* sum1 = s->MakeSum(vars);
  Constraint* ct = s->MakeGreaterOrEqual(sum1, 4);
  s->AddConstraint(ct);
  EXPECT_EQ(0, vars[0]->Min());
  EXPECT_EQ(0, vars[1]->Min());
  EXPECT_EQ(0, vars[2]->Min());
  EXPECT_EQ(0, vars[3]->Min());
  EXPECT_EQ(0, vars[4]->Min());
  vars[0]->SetMax(0);
  EXPECT_EQ(0, vars[1]->Min());
  EXPECT_EQ(0, vars[2]->Min());
  EXPECT_EQ(0, vars[3]->Min());
  EXPECT_EQ(0, vars[4]->Min());
  vars[1]->SetMax(0);
  EXPECT_EQ(0, vars[2]->Min());
  EXPECT_EQ(0, vars[3]->Min());
  EXPECT_EQ(0, vars[4]->Min());
  vars[2]->SetMax(0);
  EXPECT_EQ(0, vars[3]->Min());
  EXPECT_EQ(0, vars[4]->Min());
  vars[3]->SetMax(0);
  EXPECT_EQ(4, vars[4]->Min());
  return nullptr;
};

TEST_IN_PROPAGATION0(GreaterEqExprCstTest, ForceMax, s, true) {
  const int n = 5;
  std::vector<IntVar*> vars;
  s->MakeIntVarArray(n, 0, n, "var_", &vars);
  IntExpr* sum1 = s->MakeSum(vars);
  Constraint* ct = s->MakeGreaterOrEqual(sum1, 21);
  s->AddConstraint(ct);
  EXPECT_EQ(1, vars[0]->Min());
  EXPECT_EQ(1, vars[1]->Min());
  EXPECT_EQ(1, vars[2]->Min());
  EXPECT_EQ(1, vars[3]->Min());
  EXPECT_EQ(1, vars[4]->Min());
  return nullptr;
};

class LessEqExprCstTest : public testing::Test {
 protected:
  LessEqExprCstTest() : solver_(new Solver("LessEqExprCstTest")) {}
  std::unique_ptr<Solver> solver_;
};

TEST_F(LessEqExprCstTest, AlternateCtor) {
  IntVar* var = solver_->MakeIntVar(0, 10, "var");
  Constraint* ct = solver_->MakeLessOrEqual(var, int64_t{10});
  EXPECT_FALSE(ct->DebugString().empty());
  ct = solver_->MakeLess(var, 10);
  EXPECT_FALSE(ct->DebugString().empty());
  ct = solver_->MakeLess(var, int64_t{10});
  EXPECT_FALSE(ct->DebugString().empty());
}

TEST_IN_PROPAGATION0(LessEqExprCstTest, ForceMin, s, true) {
  const int n = 5;
  std::vector<IntVar*> vars;
  s->MakeIntVarArray(n, 0, n, "var_", &vars);
  IntExpr* sum1 = s->MakeSum(vars);
  Constraint* ct = s->MakeLessOrEqual(sum1, 4);
  s->AddConstraint(ct);
  EXPECT_EQ(4, vars[0]->Max());
  EXPECT_EQ(4, vars[1]->Max());
  EXPECT_EQ(4, vars[2]->Max());
  EXPECT_EQ(4, vars[3]->Max());
  EXPECT_EQ(4, vars[4]->Max());
  vars[0]->SetMin(1);
  EXPECT_EQ(3, vars[1]->Max());
  EXPECT_EQ(3, vars[2]->Max());
  EXPECT_EQ(3, vars[3]->Max());
  EXPECT_EQ(3, vars[4]->Max());
  vars[1]->SetMin(1);
  EXPECT_EQ(2, vars[2]->Max());
  EXPECT_EQ(2, vars[3]->Max());
  EXPECT_EQ(2, vars[4]->Max());
  vars[2]->SetMax(0);
  EXPECT_EQ(2, vars[3]->Max());
  EXPECT_EQ(2, vars[4]->Max());
  vars[3]->SetMin(2);
  EXPECT_EQ(0, vars[4]->Max());
  EXPECT_EQ(1, vars[0]->Min());
  EXPECT_EQ(1, vars[1]->Min());
  EXPECT_EQ(0, vars[2]->Min());
  EXPECT_EQ(2, vars[3]->Min());
  EXPECT_EQ(0, vars[4]->Min());
  return nullptr;
};

TEST_IN_PROPAGATION0(LessEqExprCstTest, ForceMax, s, true) {
  const int n = 5;
  std::vector<IntVar*> vars;
  s->MakeIntVarArray(n, 0, n, "var_", &vars);
  IntExpr* sum1 = s->MakeSum(vars);
  Constraint* ct = s->MakeLessOrEqual(sum1, 21);
  s->AddConstraint(ct);
  EXPECT_EQ(5, vars[0]->Max());
  EXPECT_EQ(5, vars[1]->Max());
  EXPECT_EQ(5, vars[2]->Max());
  EXPECT_EQ(5, vars[3]->Max());
  EXPECT_EQ(5, vars[4]->Max());
  vars[0]->SetMin(5);
  EXPECT_EQ(5, vars[1]->Max());
  EXPECT_EQ(5, vars[2]->Max());
  EXPECT_EQ(5, vars[3]->Max());
  EXPECT_EQ(5, vars[4]->Max());
  vars[1]->SetMin(5);
  EXPECT_EQ(5, vars[2]->Max());
  EXPECT_EQ(5, vars[3]->Max());
  EXPECT_EQ(5, vars[4]->Max());
  vars[2]->SetMin(5);
  EXPECT_EQ(5, vars[3]->Max());
  EXPECT_EQ(5, vars[4]->Max());
  vars[3]->SetMin(5);
  EXPECT_EQ(1, vars[4]->Max());
  return nullptr;
};

class IsEqualCstTest : public testing::Test {
 protected:
  IsEqualCstTest() : solver_(new Solver("IsEqualCstTest")) {}
  std::unique_ptr<Solver> solver_;
};

TEST_IN_PROPAGATION0(IsEqualCstTest, SetDomain, s, true) {
  IntVar* v0 = s->MakeIntVar(0, 2, "v0");
  IntVar* v1 = s->MakeIntVar(0, 2, "v1");
  IntVar* v2 = s->MakeIntVar(0, 2, "v2");
  IntVar* v3 = s->MakeIntVar(0, 2, "v3");
  IntVar* b0 = s->MakeIntVar(0, 2, "b0");
  IntVar* b1 = s->MakeIntVar(0, 2, "b1");
  IntVar* b2 = s->MakeIntVar(0, 2, "b2");
  IntVar* b3 = s->MakeIntVar(0, 2, "b3");
  s->AddConstraint(s->MakeIsEqualCstCt(v0, 1, b0));
  s->AddConstraint(s->MakeIsEqualCstCt(v1, 1, b1));
  s->AddConstraint(s->MakeIsEqualCstCt(v2, 1, b2));
  s->AddConstraint(s->MakeIsEqualCstCt(v3, 1, b3));
  EXPECT_EQ(0, b0->Min());
  EXPECT_EQ(0, b1->Min());
  EXPECT_EQ(0, b2->Min());
  EXPECT_EQ(0, b3->Min());
  EXPECT_EQ(1, b0->Max());
  EXPECT_EQ(1, b1->Max());
  EXPECT_EQ(1, b2->Max());
  EXPECT_EQ(1, b3->Max());
  v0->SetValue(0);
  v1->RemoveValue(1);
  v2->RemoveValue(2);
  v3->SetValue(1);
  EXPECT_EQ(0, b0->Min());
  EXPECT_EQ(0, b1->Min());
  EXPECT_EQ(0, b2->Min());
  EXPECT_EQ(1, b3->Min());
  EXPECT_EQ(0, b0->Max());
  EXPECT_EQ(0, b1->Max());
  EXPECT_EQ(1, b2->Max());
  EXPECT_EQ(1, b3->Max());
  return nullptr;
};

TEST_IN_PROPAGATION0(IsEqualCstTest, SetVarDomain, s, true) {
  IntVar* v0 = s->MakeIntVar(0, 2, "v0");
  IntVar* v1 = s->MakeIntVar(0, 2, "v1");
  IntVar* v2 = s->MakeIntVar(0, 2, "v2");
  IntVar* v3 = s->MakeIntVar(0, 2, "v3");
  IntVar* b0 = s->MakeIsEqualCstVar(v0, 1);
  IntVar* b1 = s->MakeIsEqualCstVar(v1, 1);
  IntVar* b2 = s->MakeIsEqualCstVar(v2, 1);
  IntVar* b3 = s->MakeIsEqualCstVar(v3, 1);
  EXPECT_EQ(0, b0->Min());
  EXPECT_EQ(0, b1->Min());
  EXPECT_EQ(0, b2->Min());
  EXPECT_EQ(0, b3->Min());
  EXPECT_EQ(1, b0->Max());
  EXPECT_EQ(1, b1->Max());
  EXPECT_EQ(1, b2->Max());
  EXPECT_EQ(1, b3->Max());
  v0->SetValue(0);
  v1->RemoveValue(1);
  v2->RemoveValue(2);
  v3->SetValue(1);
  EXPECT_EQ(0, b0->Min());
  EXPECT_EQ(0, b1->Min());
  EXPECT_EQ(0, b2->Min());
  EXPECT_EQ(1, b3->Min());
  EXPECT_EQ(0, b0->Max());
  EXPECT_EQ(0, b1->Max());
  EXPECT_EQ(1, b2->Max());
  EXPECT_EQ(1, b3->Max());
  return nullptr;
};

TEST_F(IsEqualCstTest, Cache) {
  IntVar* v0 = solver_->MakeIntVar(0, 2, "v0");
  IntVar* v1 = solver_->MakeIntVar(0, 2, "v1");
  IntVar* b0 = solver_->MakeIsEqualCstVar(v0, 1);
  IntVar* b0bis = solver_->MakeIsEqualCstVar(v0, 1);
  EXPECT_EQ(b0, b0bis);
  IntVar* b1 = solver_->MakeIntVar(0, 2, "b1");
  solver_->MakeIsEqualCstCt(v1, 1, b1);
  IntVar* b1bis = solver_->MakeIsEqualCstVar(v1, 1);
  EXPECT_EQ(b1, b1bis);
}

TEST_IN_PROPAGATION0(IsEqualCstTest, SetBoolean, s, true) {
  IntVar* v0 = s->MakeIntVar(0, 1, "v0");
  IntVar* v1 = s->MakeIntVar(0, 1, "v1");
  IntVar* b0 = s->MakeIntVar(0, 2, "b0");
  IntVar* b1 = s->MakeIntVar(0, 2, "b1");
  s->AddConstraint(s->MakeIsEqualCstCt(v0, 1, b0));
  s->AddConstraint(s->MakeIsEqualCstCt(v1, 1, b1));
  EXPECT_EQ(0, b0->Min());
  EXPECT_EQ(0, b1->Min());
  EXPECT_EQ(1, b0->Max());
  EXPECT_EQ(1, b1->Max());
  EXPECT_EQ(0, v0->Min());
  EXPECT_EQ(0, v1->Min());
  EXPECT_EQ(1, v0->Max());
  EXPECT_EQ(1, v1->Max());
  b0->SetValue(0);
  b1->SetValue(1);
  EXPECT_EQ(0, v0->Value());
  EXPECT_EQ(1, v1->Value());
  return nullptr;
};

class IsDiffCstTest : public testing::Test {
 protected:
  IsDiffCstTest() : solver_(new Solver("IsDiffCstTest")) {}
  std::unique_ptr<Solver> solver_;
};

TEST_IN_PROPAGATION0(IsDiffCstTest, SetDomain, s, true) {
  IntVar* v0 = s->MakeIntVar(0, 2, "v0");
  IntVar* v1 = s->MakeIntVar(0, 2, "v1");
  IntVar* v2 = s->MakeIntVar(0, 2, "v2");
  IntVar* v3 = s->MakeIntVar(0, 2, "v3");
  IntVar* b0 = s->MakeIntVar(0, 2, "b0");
  IntVar* b1 = s->MakeIntVar(0, 2, "b1");
  IntVar* b2 = s->MakeIntVar(0, 2, "b2");
  IntVar* b3 = s->MakeIntVar(0, 2, "b3");
  s->AddConstraint(s->MakeIsDifferentCstCt(v0, 1, b0));
  s->AddConstraint(s->MakeIsDifferentCstCt(v1, 1, b1));
  s->AddConstraint(s->MakeIsDifferentCstCt(v2, 1, b2));
  s->AddConstraint(s->MakeIsDifferentCstCt(v3, 1, b3));
  EXPECT_EQ(0, b0->Min());
  EXPECT_EQ(0, b1->Min());
  EXPECT_EQ(0, b2->Min());
  EXPECT_EQ(0, b3->Min());
  EXPECT_EQ(1, b0->Max());
  EXPECT_EQ(1, b1->Max());
  EXPECT_EQ(1, b2->Max());
  EXPECT_EQ(1, b3->Max());
  v0->SetValue(0);
  v1->RemoveValue(1);
  v2->RemoveValue(2);
  v3->SetValue(1);
  EXPECT_EQ(1, b0->Min());
  EXPECT_EQ(1, b1->Min());
  EXPECT_EQ(0, b2->Min());
  EXPECT_EQ(0, b3->Min());
  EXPECT_EQ(1, b0->Max());
  EXPECT_EQ(1, b1->Max());
  EXPECT_EQ(1, b2->Max());
  EXPECT_EQ(0, b3->Max());
  return nullptr;
};

TEST_IN_PROPAGATION0(IsDiffCstTest, SetVarDomain, s, true) {
  IntVar* v0 = s->MakeIntVar(0, 2, "v0");
  IntVar* v1 = s->MakeIntVar(0, 2, "v1");
  IntVar* v2 = s->MakeIntVar(0, 2, "v2");
  IntVar* v3 = s->MakeIntVar(0, 2, "v3");
  IntVar* b0 = s->MakeIsDifferentCstVar(v0, 1);
  IntVar* b1 = s->MakeIsDifferentCstVar(v1, 1);
  IntVar* b2 = s->MakeIsDifferentCstVar(v2, 1);
  IntVar* b3 = s->MakeIsDifferentCstVar(v3, 1);
  EXPECT_EQ(0, b0->Min());
  EXPECT_EQ(0, b1->Min());
  EXPECT_EQ(0, b2->Min());
  EXPECT_EQ(0, b3->Min());
  EXPECT_EQ(1, b0->Max());
  EXPECT_EQ(1, b1->Max());
  EXPECT_EQ(1, b2->Max());
  EXPECT_EQ(1, b3->Max());
  v0->SetValue(0);
  v1->RemoveValue(1);
  v2->RemoveValue(2);
  v3->SetValue(1);
  EXPECT_EQ(1, b0->Min());
  EXPECT_EQ(1, b1->Min());
  EXPECT_EQ(0, b2->Min());
  EXPECT_EQ(0, b3->Min());
  EXPECT_EQ(1, b0->Max());
  EXPECT_EQ(1, b1->Max());
  EXPECT_EQ(1, b2->Max());
  EXPECT_EQ(0, b3->Max());
  return nullptr;
};

TEST_IN_PROPAGATION0(IsDiffCstTest, SetBoolean, s, true) {
  IntVar* v0 = s->MakeIntVar(0, 1, "v0");
  IntVar* v1 = s->MakeIntVar(0, 1, "v1");
  IntVar* b0 = s->MakeIntVar(0, 2, "b0");
  IntVar* b1 = s->MakeIntVar(0, 2, "b1");
  s->AddConstraint(s->MakeIsDifferentCstCt(v0, 1, b0));
  s->AddConstraint(s->MakeIsDifferentCstCt(v1, 1, b1));
  EXPECT_EQ(0, b0->Min());
  EXPECT_EQ(0, b1->Min());
  EXPECT_EQ(1, b0->Max());
  EXPECT_EQ(1, b1->Max());
  EXPECT_EQ(0, v0->Min());
  EXPECT_EQ(0, v1->Min());
  EXPECT_EQ(1, v0->Max());
  EXPECT_EQ(1, v1->Max());
  b0->SetValue(0);
  b1->SetValue(1);
  EXPECT_EQ(1, v0->Value());
  EXPECT_EQ(0, v1->Value());
  return nullptr;
};

TEST_F(IsDiffCstTest, Cache) {
  IntVar* v0 = solver_->MakeIntVar(0, 2, "v0");
  IntVar* v1 = solver_->MakeIntVar(0, 2, "v1");
  IntVar* b0 = solver_->MakeIsDifferentCstVar(v0, 1);
  IntVar* b0bis = solver_->MakeIsDifferentCstVar(v0, 1);
  EXPECT_EQ(b0, b0bis);
  IntVar* b1 = solver_->MakeIntVar(0, 2, "b1");
  solver_->MakeIsDifferentCstCt(v1, 1, b1);
  IntVar* b1bis = solver_->MakeIsDifferentCstVar(v1, 1);
  EXPECT_EQ(b1, b1bis);
}

class IsGreaterCstTest : public testing::Test {
 protected:
  IsGreaterCstTest() : solver_(new Solver("IsGreaterCstTest")) {}
  std::unique_ptr<Solver> solver_;
};

TEST_IN_PROPAGATION0(IsGreaterCstTest, SetDomain, s, true) {
  IntVar* v0 = s->MakeIntVar(0, 2, "v0");
  IntVar* v1 = s->MakeIntVar(0, 2, "v1");
  IntVar* v2 = s->MakeIntVar(0, 2, "v2");
  IntVar* b0 = s->MakeIntVar(0, 2, "b0");
  IntVar* b1 = s->MakeIntVar(0, 2, "b1");
  IntVar* b2 = s->MakeIntVar(0, 2, "b2");
  s->AddConstraint(s->MakeIsGreaterCstCt(v0, 1, b0));
  s->AddConstraint(s->MakeIsGreaterCstCt(v1, 1, b1));
  s->AddConstraint(s->MakeIsGreaterCstCt(v2, 1, b2));
  EXPECT_EQ(0, b0->Min());
  EXPECT_EQ(0, b1->Min());
  EXPECT_EQ(0, b2->Min());
  EXPECT_EQ(1, b0->Max());
  EXPECT_EQ(1, b1->Max());
  EXPECT_EQ(1, b2->Max());
  v0->SetValue(0);
  v1->SetValue(1);
  v2->SetValue(2);
  EXPECT_EQ(0, b0->Value());
  EXPECT_EQ(0, b1->Value());
  EXPECT_EQ(1, b2->Value());
  return nullptr;
};

TEST_IN_PROPAGATION0(IsGreaterCstTest, SetBoolean, s, true) {
  IntVar* v0 = s->MakeIntVar(0, 2, "v0");
  IntVar* v1 = s->MakeIntVar(0, 2, "v1");
  IntVar* b0 = s->MakeIntVar(0, 2, "b0");
  IntVar* b1 = s->MakeIntVar(0, 2, "b1");
  s->AddConstraint(s->MakeIsGreaterCstCt(v0, 1, b0));
  s->AddConstraint(s->MakeIsGreaterCstCt(v1, 1, b1));
  b0->SetValue(0);
  b1->SetValue(1);
  EXPECT_EQ(1, v0->Max());
  EXPECT_EQ(2, v1->Value());
  return nullptr;
};

TEST_F(IsGreaterCstTest, Cache) {
  IntVar* v0 = solver_->MakeIntVar(0, 2, "v0");
  IntVar* v1 = solver_->MakeIntVar(0, 2, "v1");
  IntVar* b0 = solver_->MakeIsGreaterCstVar(v0, 1);
  IntVar* b0bis = solver_->MakeIsGreaterCstVar(v0, 1);
  EXPECT_EQ(b0, b0bis);
  IntVar* b1 = solver_->MakeIntVar(0, 2, "b1");
  solver_->MakeIsGreaterCstCt(v1, 1, b1);
  IntVar* b1bis = solver_->MakeIsGreaterCstVar(v1, 1);
  EXPECT_EQ(b1, b1bis);
}

class IsGreaterEqualCstTest : public testing::Test {
 protected:
  IsGreaterEqualCstTest() : solver_(new Solver("IsGreaterEqualCstTest")) {}
  std::unique_ptr<Solver> solver_;
};

TEST_IN_PROPAGATION0(IsGreaterEqualCstTest, SetDomain, s, true) {
  IntVar* v0 = s->MakeIntVar(0, 2, "v0");
  IntVar* v1 = s->MakeIntVar(0, 2, "v1");
  IntVar* v2 = s->MakeIntVar(0, 2, "v2");
  IntVar* b0 = s->MakeIntVar(0, 2, "b0");
  IntVar* b1 = s->MakeIntVar(0, 2, "b1");
  IntVar* b2 = s->MakeIntVar(0, 2, "b2");
  s->AddConstraint(s->MakeIsGreaterOrEqualCstCt(v0, 1, b0));
  s->AddConstraint(s->MakeIsGreaterOrEqualCstCt(v1, 1, b1));
  s->AddConstraint(s->MakeIsGreaterOrEqualCstCt(v2, 1, b2));
  EXPECT_EQ(0, b0->Min());
  EXPECT_EQ(0, b1->Min());
  EXPECT_EQ(0, b2->Min());
  EXPECT_EQ(1, b0->Max());
  EXPECT_EQ(1, b1->Max());
  EXPECT_EQ(1, b2->Max());
  v0->SetValue(0);
  v1->SetValue(1);
  v2->SetValue(2);
  EXPECT_EQ(0, b0->Value());
  EXPECT_EQ(1, b1->Value());
  EXPECT_EQ(1, b2->Value());
  return nullptr;
};

TEST_IN_PROPAGATION0(IsGreaterEqualCstTest, SetBoolean, s, true) {
  IntVar* v0 = s->MakeIntVar(0, 2, "v0");
  IntVar* v1 = s->MakeIntVar(0, 2, "v1");
  IntVar* b0 = s->MakeIntVar(0, 2, "b0");
  IntVar* b1 = s->MakeIntVar(0, 2, "b1");
  s->AddConstraint(s->MakeIsGreaterOrEqualCstCt(v0, 1, b0));
  s->AddConstraint(s->MakeIsGreaterOrEqualCstCt(v1, 1, b1));
  b0->SetValue(0);
  b1->SetValue(1);
  EXPECT_EQ(0, v0->Value());
  EXPECT_EQ(1, v1->Min());
  return nullptr;
};

TEST_F(IsGreaterEqualCstTest, Cache) {
  IntVar* v0 = solver_->MakeIntVar(0, 2, "v0");
  IntVar* v1 = solver_->MakeIntVar(0, 2, "v1");
  IntVar* b0 = solver_->MakeIsGreaterOrEqualCstVar(v0, 1);
  IntVar* b0bis = solver_->MakeIsGreaterOrEqualCstVar(v0, 1);
  EXPECT_EQ(b0, b0bis);
  IntVar* b1 = solver_->MakeIntVar(0, 2, "b1");
  solver_->MakeIsGreaterOrEqualCstCt(v1, 1, b1);
  IntVar* b1bis = solver_->MakeIsGreaterOrEqualCstVar(v1, 1);
  EXPECT_EQ(b1, b1bis);
}

class IsLessCstTest : public testing::Test {
 protected:
  IsLessCstTest() : solver_(new Solver("IsLessCstTest")) {}
  std::unique_ptr<Solver> solver_;
};

TEST_IN_PROPAGATION0(IsLessCstTest, SetDomain, s, true) {
  IntVar* v0 = s->MakeIntVar(0, 2, "v0");
  IntVar* v1 = s->MakeIntVar(0, 2, "v1");
  IntVar* v2 = s->MakeIntVar(0, 2, "v2");
  IntVar* b0 = s->MakeIntVar(0, 2, "b0");
  IntVar* b1 = s->MakeIntVar(0, 2, "b1");
  IntVar* b2 = s->MakeIntVar(0, 2, "b2");
  s->AddConstraint(s->MakeIsLessCstCt(v0, 1, b0));
  s->AddConstraint(s->MakeIsLessCstCt(v1, 1, b1));
  s->AddConstraint(s->MakeIsLessCstCt(v2, 1, b2));
  EXPECT_EQ(0, b0->Min());
  EXPECT_EQ(0, b1->Min());
  EXPECT_EQ(0, b2->Min());
  EXPECT_EQ(1, b0->Max());
  EXPECT_EQ(1, b1->Max());
  EXPECT_EQ(1, b2->Max());
  v0->SetValue(0);
  v1->SetValue(1);
  v2->SetValue(2);
  EXPECT_EQ(1, b0->Value());
  EXPECT_EQ(0, b1->Value());
  EXPECT_EQ(0, b2->Value());
  return nullptr;
};

TEST_IN_PROPAGATION0(IsLessCstTest, SetBoolean, s, true) {
  IntVar* v0 = s->MakeIntVar(0, 2, "v0");
  IntVar* v1 = s->MakeIntVar(0, 2, "v1");
  IntVar* v2 = s->MakeIntVar(0, 2, "v2");
  IntVar* b0 = s->MakeIntVar(0, 2, "b0");
  IntVar* b1 = s->MakeIntVar(0, 2, "b1");
  IntVar* b2 = s->MakeIntVar(0, 2, "b2");
  s->AddConstraint(s->MakeIsLessCstCt(v0, 1, b0));
  s->AddConstraint(s->MakeIsLessCstCt(v1, 1, b1));
  s->AddConstraint(s->MakeIsLessCstCt(v2, 1, b2));
  b0->SetValue(0);
  b1->SetValue(1);
  EXPECT_EQ(1, v0->Min());
  EXPECT_EQ(0, v1->Value());
  return nullptr;
};

TEST_F(IsLessCstTest, Cache) {
  IntVar* v0 = solver_->MakeIntVar(0, 2, "v0");
  IntVar* v1 = solver_->MakeIntVar(0, 2, "v1");
  IntVar* b0 = solver_->MakeIsLessCstVar(v0, 1);
  IntVar* b0bis = solver_->MakeIsLessCstVar(v0, 1);
  EXPECT_EQ(b0, b0bis);
  IntVar* b1 = solver_->MakeIntVar(0, 2, "b1");
  solver_->MakeIsLessCstCt(v1, 1, b1);
  IntVar* b1bis = solver_->MakeIsLessCstVar(v1, 1);
  EXPECT_EQ(b1, b1bis);
}

TEST_F(IsLessCstTest, ComplexInteraction) {
  Solver s("ShoppingBasketBug");
  IntVar* const x15 = s.MakeIntVar(0, 2, "x15");
  IntVar* const x18 = s.MakeIntVar(0, 2, "x18");
  IntVar* const is1 = s.MakeIsEqualCstVar(x15, 2);
  IntVar* const is2 = s.MakeIsEqualCstVar(x18, 2);
  IntVar* const is_less = s.MakeIsLessOrEqualCstVar(
      s.MakeSum(s.MakeProd(is1, 2), s.MakeProd(is2, 2)), 1);
  std::vector<int64_t> values1 = {10, 2, 12};
  IntVar* const elem1 = s.MakeElement(values1, x15)->Var();
  std::vector<int64_t> values2 = {2, 10, 5};
  IntVar* const elem2 = s.MakeElement(values2, x18)->Var();
  std::vector<IntVar*> vars = {elem1, is_less, elem2};
  std::vector<int64_t> coefs = {1, 90, 1};
  IntVar* const obj = s.MakeScalProd(vars, coefs)->Var();
  OptimizeVar* const optimize = s.MakeMinimize(obj, 1);
  SolutionCollector* const collector = s.MakeLastSolutionCollector();
  collector->AddObjective(obj);
  collector->Add(x15);
  collector->Add(x18);
  collector->Add(is_less);
  collector->Add(elem1);
  collector->Add(elem2);
  collector->Add(is1);
  collector->Add(is2);
  DecisionBuilder* const db1 =
      s.MakePhase(x15, x18, Solver::CHOOSE_MAX_SIZE, Solver::ASSIGN_MIN_VALUE);
  DecisionBuilder* const db2 =
      s.MakePhase(obj, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  DecisionBuilder* const db = s.Compose(db1, db2);
  EXPECT_TRUE(s.Solve(db, optimize, collector));
  EXPECT_EQ(0, collector->Value(0, is_less));
  EXPECT_EQ(7, collector->objective_value(0));
}

class IsLessEqualCstTest : public testing::Test {
 protected:
  IsLessEqualCstTest() : solver_(new Solver("IsLessEqualCstTest")) {}
  std::unique_ptr<Solver> solver_;
};

TEST_IN_PROPAGATION0(IsLessEqualCstTest, SetDomain, s, true) {
  IntVar* v0 = s->MakeIntVar(0, 2, "v0");
  IntVar* v1 = s->MakeIntVar(0, 2, "v1");
  IntVar* v2 = s->MakeIntVar(0, 2, "v2");
  IntVar* b0 = s->MakeIntVar(0, 2, "b0");
  IntVar* b1 = s->MakeIntVar(0, 2, "b1");
  IntVar* b2 = s->MakeIntVar(0, 2, "b2");
  s->AddConstraint(s->MakeIsLessOrEqualCstCt(v0, 1, b0));
  s->AddConstraint(s->MakeIsLessOrEqualCstCt(v1, 1, b1));
  s->AddConstraint(s->MakeIsLessOrEqualCstCt(v2, 1, b2));
  EXPECT_EQ(0, b0->Min());
  EXPECT_EQ(0, b1->Min());
  EXPECT_EQ(0, b2->Min());
  EXPECT_EQ(1, b0->Max());
  EXPECT_EQ(1, b1->Max());
  EXPECT_EQ(1, b2->Max());
  v0->SetValue(0);
  v1->SetValue(1);
  v2->SetValue(2);
  EXPECT_EQ(1, b0->Value());
  EXPECT_EQ(1, b1->Value());
  EXPECT_EQ(0, b2->Value());
  return nullptr;
};

TEST_IN_PROPAGATION0(IsLessEqualCstTest, SetBoolean, s, true) {
  IntVar* v0 = s->MakeIntVar(0, 2, "v0");
  IntVar* v1 = s->MakeIntVar(0, 2, "v1");
  IntVar* v2 = s->MakeIntVar(0, 2, "v2");
  IntVar* b0 = s->MakeIntVar(0, 2, "b0");
  IntVar* b1 = s->MakeIntVar(0, 2, "b1");
  IntVar* b2 = s->MakeIntVar(0, 2, "b2");
  s->AddConstraint(s->MakeIsLessOrEqualCstCt(v0, 1, b0));
  s->AddConstraint(s->MakeIsLessOrEqualCstCt(v1, 1, b1));
  s->AddConstraint(s->MakeIsLessOrEqualCstCt(v2, 1, b2));
  b0->SetValue(0);
  b1->SetValue(1);
  EXPECT_EQ(2, v0->Value());
  EXPECT_EQ(1, v1->Max());
  return nullptr;
};

TEST_F(IsLessEqualCstTest, Cache) {
  IntVar* v0 = solver_->MakeIntVar(0, 2, "v0");
  IntVar* v1 = solver_->MakeIntVar(0, 2, "v1");
  IntVar* b0 = solver_->MakeIsLessOrEqualCstVar(v0, 1);
  IntVar* b0bis = solver_->MakeIsLessOrEqualCstVar(v0, 1);
  EXPECT_EQ(b0, b0bis);
  IntVar* b1 = solver_->MakeIntVar(0, 2, "b1");
  solver_->MakeIsLessOrEqualCstCt(v1, 1, b1);
  IntVar* b1bis = solver_->MakeIsLessOrEqualCstVar(v1, 1);
  EXPECT_EQ(b1, b1bis);
}

class IsBetweenTest : public testing::Test {
 protected:
  IsBetweenTest() : solver_(new Solver("IsBetweenTest")) {}
  std::unique_ptr<Solver> solver_;
};

TEST_IN_PROPAGATION0(IsBetweenTest, SetDomain, s, true) {
  IntVar* v0 = s->MakeIntVar(0, 4, "v0");
  IntVar* v1 = s->MakeIntVar(0, 4, "v1");
  IntVar* v2 = s->MakeIntVar(0, 4, "v2");
  IntVar* v3 = s->MakeIntVar(0, 4, "v3");
  IntVar* v4 = s->MakeIntVar(0, 4, "v4");
  IntVar* b0 = s->MakeIntVar(0, 2, "b0");
  IntVar* b1 = s->MakeIntVar(0, 2, "b1");
  IntVar* b2 = s->MakeIntVar(0, 2, "b2");
  IntVar* b3 = s->MakeIntVar(0, 2, "b3");
  IntVar* b4 = s->MakeIntVar(0, 2, "b4");
  s->AddConstraint(s->MakeIsBetweenCt(v0, 1, 3, b0));
  s->AddConstraint(s->MakeIsBetweenCt(v1, 1, 3, b1));
  s->AddConstraint(s->MakeIsBetweenCt(v2, 1, 3, b2));
  s->AddConstraint(s->MakeIsBetweenCt(v3, 1, 3, b3));
  s->AddConstraint(s->MakeIsBetweenCt(v4, 1, 3, b4));
  EXPECT_EQ(0, b0->Min());
  EXPECT_EQ(0, b1->Min());
  EXPECT_EQ(0, b2->Min());
  EXPECT_EQ(0, b3->Min());
  EXPECT_EQ(0, b4->Min());
  EXPECT_EQ(1, b0->Max());
  EXPECT_EQ(1, b1->Max());
  EXPECT_EQ(1, b2->Max());
  EXPECT_EQ(1, b3->Max());
  EXPECT_EQ(1, b4->Max());
  v0->SetValue(0);
  v1->SetValue(1);
  v2->SetValue(2);
  v3->SetValue(3);
  v4->SetValue(4);
  EXPECT_EQ(0, b0->Value());
  EXPECT_EQ(1, b1->Value());
  EXPECT_EQ(1, b2->Value());
  EXPECT_EQ(1, b3->Value());
  EXPECT_EQ(0, b4->Value());
  return nullptr;
};

TEST_IN_PROPAGATION0(IsBetweenTest, SetBoolean, s, true) {
  IntVar* v0 = s->MakeIntVar(0, 4, "v0");
  IntVar* v1 = s->MakeIntVar(0, 4, "v1");
  IntVar* b0 = s->MakeIntVar(0, 2, "b0");
  IntVar* b1 = s->MakeIntVar(0, 2, "b1");
  s->AddConstraint(s->MakeIsBetweenCt(v0, 1, 3, b0));
  s->AddConstraint(s->MakeIsBetweenCt(v1, 1, 3, b1));
  b0->SetValue(0);
  b1->SetValue(1);
  EXPECT_TRUE(v0->Contains(0));
  EXPECT_FALSE(v0->Contains(1));
  EXPECT_FALSE(v0->Contains(2));
  EXPECT_FALSE(v0->Contains(3));
  EXPECT_TRUE(v0->Contains(4));
  EXPECT_FALSE(v1->Contains(0));
  EXPECT_TRUE(v1->Contains(1));
  EXPECT_TRUE(v1->Contains(2));
  EXPECT_TRUE(v1->Contains(3));
  EXPECT_FALSE(v1->Contains(4));

  return nullptr;
};

class BetweenTest : public testing::Test {
 protected:
  BetweenTest() : solver_(new Solver("BetweenTest")) {}
  std::unique_ptr<Solver> solver_;
};

TEST_IN_PROPAGATION0(BetweenTest, SetDomain, s, true) {
  IntVar* v0 = s->MakeIntVar(0, 4, "v0");
  s->AddConstraint(s->MakeBetweenCt(v0, 1, 3));
  EXPECT_EQ(1, v0->Min());
  EXPECT_EQ(3, v0->Max());
  return nullptr;
};

TEST_F(IsBetweenTest, EmptyDebugString) {
  IntVar* v0 = solver_->MakeIntVar(0, 4, "v0");
  Constraint* const ct = solver_->MakeBetweenCt(v0, 1, 3);
  EXPECT_FALSE(ct->DebugString().empty());
}

// ----- NotBetweenCtTest -----

TEST(NotBetweenCtTest, Simplification) {
  Solver solver("NotBetweenCtTest::Simplification");
  IntVar* const v = solver.MakeIntVar(0, 100, "v");
  Constraint* const c1 = solver.MakeNotBetweenCt(v, -10, -5);
  Constraint* const c2 = solver.MakeNotBetweenCt(v, 110, 130);
  EXPECT_EQ(c1, solver.MakeTrueConstraint());
  EXPECT_EQ(c2, solver.MakeTrueConstraint());
  Constraint* const c3 = solver.MakeNotBetweenCt(v, -10, 100);
  Constraint* const c4 = solver.MakeNotBetweenCt(v, 0, 110);
  EXPECT_EQ(c3, solver.MakeFalseConstraint());
  EXPECT_EQ(c4, solver.MakeFalseConstraint());
  Constraint* const c5 = solver.MakeNotBetweenCt(v, 10, 0);
  EXPECT_EQ(c5, solver.MakeTrueConstraint());
  Constraint* const c6 = solver.MakeNotBetweenCt(v, 0, 49);
  Constraint* const c7 = solver.MakeNotBetweenCt(v, 51, 100);
  EXPECT_EQ(c6->DebugString(), solver.MakeGreaterOrEqual(v, 50)->DebugString());
  EXPECT_EQ(c7->DebugString(), solver.MakeLessOrEqual(v, 50)->DebugString());
}

TEST_IN_PROPAGATION(NotBetweenCtTest, PropatationOnMin, s, true) {
  IntVar* const v = s->MakeIntVar(0, 100);
  Constraint* const c = s->MakeNotBetweenCt(v, 30, 50);
  s->AddConstraint(c);
  EXPECT_EQ_RANGE(v, 0, 100);
  v->SetMin(10);
  EXPECT_EQ_RANGE(v, 10, 100);
  v->SetMin(29);
  EXPECT_EQ_RANGE(v, 29, 100);
  v->SetMin(30);
  EXPECT_EQ_RANGE(v, 51, 100);
  v->SetMin(60);
  EXPECT_EQ_RANGE(v, 60, 100);
  return nullptr;
}

TEST_IN_PROPAGATION(NotBetweenCtTest, PropatationOnMax, s, true) {
  IntVar* const v = s->MakeIntVar(0, 100);
  Constraint* const c = s->MakeNotBetweenCt(v, 30, 50);
  s->AddConstraint(c);
  EXPECT_EQ_RANGE(v, 0, 100);
  v->SetMax(90);
  EXPECT_EQ_RANGE(v, 0, 90);
  v->SetMax(51);
  EXPECT_EQ_RANGE(v, 0, 51);
  v->SetMax(50);
  EXPECT_EQ_RANGE(v, 0, 29);
  v->SetMax(15);
  EXPECT_EQ_RANGE(v, 0, 15);
  return nullptr;
}

TEST(NotBetweenCtTest, PropagateOnExpression) {
  Solver solver("NotBetweenCtTest::Expression");
  IntVar* const v1 = solver.MakeIntVar(0, 4, "v1");
  IntVar* const v2 = solver.MakeIntVar(0, 4, "v2");
  IntExpr* const e = solver.MakeSum(v1, v2);
  Constraint* const ct = solver.MakeNotBetweenCt(e, 1, 7);
  solver.AddConstraint(ct);
  DecisionBuilder* const db = solver.MakePhase(
      v1, v2, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
  solver.NewSearch(db);
  ASSERT_TRUE(solver.NextSolution());
  EXPECT_EQ(0, v1->Max());
  EXPECT_EQ(0, v2->Max());
  ASSERT_TRUE(solver.NextSolution());
  EXPECT_EQ(4, v1->Min());
  EXPECT_EQ(4, v2->Min());
  EXPECT_FALSE(solver.NextSolution());
  solver.EndSearch();
}

// ----- MemberCtTest -----

class MemberCtTest : public testing::Test {
 protected:
  MemberCtTest() : solver_(new Solver("MemberCtTest")) {}
  std::unique_ptr<Solver> solver_;
};

TEST_F(MemberCtTest, Simplification) {
  Solver solver("MemberCtTest::Simplification");
  // Checks out of domain filtering.
  IntVar* const v = solver.MakeIntVar(0, 10, "v");
  std::vector<int> values1 = {-2, 5, 12};
  Constraint* const c1 = solver.MakeMemberCt(v, values1);
  EXPECT_EQ(c1->DebugString(), solver.MakeEquality(v, 5)->DebugString());
  // Checks complement building.
  std::vector<int> values2 = {0, 2, 3, 5, 6, 7, 9, 10};
  std::vector<int> complement2 = {1, 4, 8};
  Constraint* const c2 = solver.MakeMemberCt(v, values2);
  EXPECT_EQ(c2->DebugString(),
            solver.MakeNotMemberCt(v, complement2)->DebugString());
  // Checks filtering to empty set of values.
  std::vector<int> values3 = {-5, 15};
  Constraint* const c3 = solver.MakeMemberCt(v, values3);
  EXPECT_EQ(c3, solver.MakeFalseConstraint());
  // Checks complete covering by values.
  std::vector<int> values4 = {0, 1, 2, 3, 4, 11, 5, 6, 7, 8, 9, 10, -2};
  Constraint* const c4 = solver.MakeMemberCt(v, values4);
  EXPECT_EQ(c4, solver.MakeTrueConstraint());
  // Checks continuous interval simplification.
  std::vector<int> values5 = {4, 2, 3, 5};
  Constraint* const c5 = solver.MakeMemberCt(v, values5);
  EXPECT_EQ(c5->DebugString(), solver.MakeBetweenCt(v, 2, 5)->DebugString());
  // Checks case when expression is actually a product by a constant.
  IntExpr* const e = solver.MakeProd(v, 3);
  std::vector<int> values6 = {1, 2, 3, 4, 5};
  Constraint* const c6 = solver.MakeMemberCt(e, values6);
  EXPECT_EQ(c6->DebugString(), solver.MakeEquality(v, 1)->DebugString());
}

TEST_IN_PROPAGATION0(MemberCtTest, BoundReduction, s, true) {
  IntVar* const v = s->MakeIntVar(0, 100, "v");
  std::vector<int64_t> values;
  values.push_back(10);
  values.push_back(30);
  values.push_back(30);
  values.push_back(-5);
  values.push_back(110);
  values.push_back(50);
  s->AddConstraint(s->MakeMemberCt(v, values));
  EXPECT_EQ(10, v->Min());
  EXPECT_EQ(50, v->Max());
  v->SetMax(35);
  EXPECT_EQ(10, v->Min());
  EXPECT_EQ(30, v->Max());
  v->SetMin(15);
  EXPECT_TRUE(v->Bound());
  EXPECT_EQ(30, v->Min());
  return nullptr;
}

TEST_IN_PROPAGATION0(MemberCtTest, NoIntersection, s, false) {
  Constraint* ct = nullptr;
  {
    IntVar* const v = s->MakeIntVar(60, 100, "v");
    std::vector<int64_t> values;
    values.push_back(10);
    values.push_back(30);
    values.push_back(30);
    values.push_back(-5);
    values.push_back(110);
    values.push_back(50);
    ct = s->MakeMemberCt(v, values);
  }
  s->AddConstraint(ct);
  return nullptr;
}

// ----- NotMemberCtTest -----

TEST(NotMemberCtTest, Simplification) {
  Solver solver("NotMemberCtTest::Simplification");
  // Checks out of domain filtering.
  IntVar* const v = solver.MakeIntVar(0, 10, "v");
  std::vector<int> values1 = {-2, 5, 12};
  Constraint* const c1 = solver.MakeNotMemberCt(v, values1);
  EXPECT_EQ(c1->DebugString(), solver.MakeNonEquality(v, 5)->DebugString());
  // Checks complement building.
  std::vector<int> values2 = {0, 2, 3, 5, 6, 7, 9, 10};
  std::vector<int> complement2 = {1, 4, 8};
  Constraint* const c2 = solver.MakeNotMemberCt(v, values2);
  EXPECT_EQ(c2->DebugString(),
            solver.MakeMemberCt(v, complement2)->DebugString());
  // Checks filtering to empty set of values.
  std::vector<int> values3 = {-5, 15};
  Constraint* const c3 = solver.MakeNotMemberCt(v, values3);
  EXPECT_EQ(c3, solver.MakeTrueConstraint());
  // Checks complete covering by values.
  std::vector<int> values4 = {0, 1, 2, 3, 4, 11, 5, 6, 7, 8, 9, 10, -2};
  Constraint* const c4 = solver.MakeNotMemberCt(v, values4);
  EXPECT_EQ(c4, solver.MakeFalseConstraint());
  // Checks continuous interval simplification.
  std::vector<int> values5 = {4, 2, 3, 5};
  Constraint* const c5 = solver.MakeNotMemberCt(v, values5);
  EXPECT_EQ(c5->DebugString(), solver.MakeNotBetweenCt(v, 2, 5)->DebugString());
  // Checks case when expression is actually a product by a constant.
  IntExpr* const e = solver.MakeProd(v, 3);
  std::vector<int> values6 = {1, 2, 3, 4, 5};
  Constraint* const c6 = solver.MakeNotMemberCt(e, values6);
  EXPECT_EQ(c6->DebugString(), solver.MakeNonEquality(v, 1)->DebugString());
}

// ----- IsMemberCtTest -----

class IsMemberCtTest : public testing::Test {
 protected:
  IsMemberCtTest() : solver_(new Solver("IsMemberCtTest")) {}
  std::unique_ptr<Solver> solver_;
};

TEST_IN_PROPAGATION0(IsMemberCtTest, BoundReduction, s, true) {
  IntVar* const v = s->MakeIntVar(0, 100, "v");
  std::vector<int64_t> values;
  values.push_back(10);
  values.push_back(30);
  values.push_back(30);
  values.push_back(-5);
  values.push_back(110);
  values.push_back(50);
  IntVar* const b = s->MakeBoolVar("b");
  s->AddConstraint(s->MakeIsMemberCt(v, values, b));
  EXPECT_EQ(int64_t{0}, v->Min());
  EXPECT_EQ(int64_t{100}, v->Max());
  EXPECT_FALSE(b->Bound());
  v->SetMax(35);
  EXPECT_EQ(int64_t{0}, v->Min());
  EXPECT_EQ(int64_t{35}, v->Max());
  EXPECT_FALSE(b->Bound());
  v->SetMin(15);
  EXPECT_FALSE(b->Bound());
  EXPECT_EQ(15, v->Min());
  EXPECT_EQ(35, v->Max());
  v->SetMax(18);
  EXPECT_EQ(15, v->Min());
  EXPECT_EQ(18, v->Max());
  EXPECT_TRUE(b->Bound());
  EXPECT_EQ(0, b->Min());
  return nullptr;
}

TEST_IN_PROPAGATION0(IsMemberCtTest, BoundReductionUsingInt, s, true) {
  IntVar* const v = s->MakeIntVar(0, 100, "v");
  std::vector<int> values;
  values.push_back(10);
  values.push_back(30);
  values.push_back(30);
  values.push_back(-5);
  values.push_back(110);
  values.push_back(50);
  IntVar* const b = s->MakeBoolVar("b");
  s->AddConstraint(s->MakeIsMemberCt(v, values, b));
  EXPECT_EQ(int64_t{0}, v->Min());
  EXPECT_EQ(int64_t{100}, v->Max());
  EXPECT_FALSE(b->Bound());
  v->SetMax(35);
  EXPECT_EQ(int64_t{0}, v->Min());
  EXPECT_EQ(int64_t{35}, v->Max());
  EXPECT_FALSE(b->Bound());
  v->SetMin(15);
  EXPECT_FALSE(b->Bound());
  EXPECT_EQ(15, v->Min());
  EXPECT_EQ(35, v->Max());
  v->SetMax(18);
  EXPECT_EQ(15, v->Min());
  EXPECT_EQ(18, v->Max());
  EXPECT_TRUE(b->Bound());
  EXPECT_EQ(0, b->Min());
  return nullptr;
}

TEST_IN_PROPAGATION0(IsMemberCtTest, BoundReductionTrue, s, true) {
  IntVar* const v = s->MakeIntVar(0, 100, "v");
  std::vector<int64_t> values;
  values.push_back(10);
  values.push_back(30);
  values.push_back(30);
  values.push_back(-5);
  values.push_back(110);
  values.push_back(50);
  IntVar* const b = s->MakeBoolVar("b");
  s->AddConstraint(s->MakeIsMemberCt(v, values, b));
  EXPECT_EQ(int64_t{0}, v->Min());
  EXPECT_EQ(int64_t{100}, v->Max());
  EXPECT_FALSE(b->Bound());
  v->SetMax(35);
  EXPECT_EQ(int64_t{0}, v->Min());
  EXPECT_EQ(int64_t{35}, v->Max());
  EXPECT_FALSE(b->Bound());
  v->SetMin(15);
  EXPECT_FALSE(b->Bound());
  EXPECT_EQ(15, v->Min());
  EXPECT_EQ(35, v->Max());
  v->SetValue(30);
  EXPECT_TRUE(b->Bound());
  EXPECT_EQ(1, b->Min());
  return nullptr;
}

TEST_IN_PROPAGATION0(IsMemberCtTest, BoundReductionTrueUsingInt, s, true) {
  IntVar* const v = s->MakeIntVar(0, 100, "v");
  std::vector<int> values;
  values.push_back(10);
  values.push_back(30);
  values.push_back(30);
  values.push_back(-5);
  values.push_back(110);
  values.push_back(50);
  IntVar* const b = s->MakeBoolVar("b");
  s->AddConstraint(s->MakeIsMemberCt(v, values, b));
  EXPECT_EQ(int64_t{0}, v->Min());
  EXPECT_EQ(int64_t{100}, v->Max());
  EXPECT_FALSE(b->Bound());
  v->SetMax(35);
  EXPECT_EQ(int64_t{0}, v->Min());
  EXPECT_EQ(int64_t{35}, v->Max());
  EXPECT_FALSE(b->Bound());
  v->SetMin(15);
  EXPECT_FALSE(b->Bound());
  EXPECT_EQ(15, v->Min());
  EXPECT_EQ(35, v->Max());
  v->SetValue(30);
  EXPECT_TRUE(b->Bound());
  EXPECT_EQ(1, b->Min());
  return nullptr;
}

TEST_IN_PROPAGATION0(IsMemberCtTest, BoundReductionTrue2, s, true) {
  IntVar* const v = s->MakeIntVar(0, 100, "v");
  std::vector<int64_t> values;
  values.push_back(10);
  values.push_back(30);
  values.push_back(30);
  values.push_back(-5);
  values.push_back(110);
  values.push_back(50);
  IntVar* const b = s->MakeBoolVar("b");
  s->AddConstraint(s->MakeIsMemberCt(v, values, b));
  EXPECT_EQ(int64_t{0}, v->Min());
  EXPECT_EQ(int64_t{100}, v->Max());
  EXPECT_FALSE(b->Bound());
  v->SetMax(int64_t{35});
  EXPECT_EQ(int64_t{0}, v->Min());
  EXPECT_EQ(int64_t{35}, v->Max());
  EXPECT_FALSE(b->Bound());
  v->SetMin(int64_t{15});
  EXPECT_FALSE(b->Bound());
  EXPECT_EQ(int64_t{15}, v->Min());
  EXPECT_EQ(int64_t{35}, v->Max());
  b->SetValue(int64_t{1});
  EXPECT_TRUE(v->Bound());
  EXPECT_EQ(int64_t{30}, v->Min());
  return nullptr;
}

TEST_IN_PROPAGATION0(IsMemberCtTest, BoundReductionTrue2UsingInt, s, true) {
  IntVar* const v = s->MakeIntVar(0, 100, "v");
  std::vector<int> values;
  values.push_back(10);
  values.push_back(30);
  values.push_back(30);
  values.push_back(-5);
  values.push_back(110);
  values.push_back(50);
  IntVar* const b = s->MakeBoolVar("b");
  s->AddConstraint(s->MakeIsMemberCt(v, values, b));
  EXPECT_EQ(int64_t{0}, v->Min());
  EXPECT_EQ(int64_t{100}, v->Max());
  EXPECT_FALSE(b->Bound());
  v->SetMax(int64_t{35});
  EXPECT_EQ(int64_t{0}, v->Min());
  EXPECT_EQ(int64_t{35}, v->Max());
  EXPECT_FALSE(b->Bound());
  v->SetMin(int64_t{15});
  EXPECT_FALSE(b->Bound());
  EXPECT_EQ(int64_t{15}, v->Min());
  EXPECT_EQ(int64_t{35}, v->Max());
  b->SetValue(int64_t{1});
  EXPECT_TRUE(v->Bound());
  EXPECT_EQ(int64_t{30}, v->Min());
  return nullptr;
}

TEST_IN_PROPAGATION0(IsMemberCtTest, BoundReductionFalse, s, true) {
  IntVar* const v = s->MakeIntVar(0, 100, "v");
  std::vector<int64_t> values;
  values.push_back(10);
  values.push_back(30);
  values.push_back(30);
  values.push_back(-5);
  values.push_back(110);
  values.push_back(50);
  IntVar* const b = s->MakeBoolVar("b");
  s->AddConstraint(s->MakeIsMemberCt(v, values, b));
  EXPECT_EQ(int64_t{0}, v->Min());
  EXPECT_EQ(int64_t{100}, v->Max());
  EXPECT_FALSE(b->Bound());
  v->SetMax(35);
  EXPECT_EQ(int64_t{0}, v->Min());
  EXPECT_EQ(int64_t{35}, v->Max());
  EXPECT_FALSE(b->Bound());
  v->SetMin(int64_t{15});
  EXPECT_FALSE(b->Bound());
  EXPECT_EQ(int64_t{15}, v->Min());
  EXPECT_EQ(int64_t{35}, v->Max());
  b->SetValue(int64_t{0});
  EXPECT_EQ(int64_t{20}, v->Size());
  EXPECT_EQ(int64_t{15}, v->Min());
  EXPECT_EQ(int64_t{35}, v->Max());
  EXPECT_FALSE(b->Contains(int64_t{30}));
  return nullptr;
}

TEST_IN_PROPAGATION0(IsMemberCtTest, BoundReductionFalseUsingInt, s, true) {
  IntVar* const v = s->MakeIntVar(0, 100, "v");
  std::vector<int> values;
  values.push_back(10);
  values.push_back(30);
  values.push_back(30);
  values.push_back(-5);
  values.push_back(110);
  values.push_back(50);
  IntVar* const b = s->MakeBoolVar("b");
  s->AddConstraint(s->MakeIsMemberCt(v, values, b));
  EXPECT_EQ(int64_t{0}, v->Min());
  EXPECT_EQ(int64_t{100}, v->Max());
  EXPECT_FALSE(b->Bound());
  v->SetMax(35);
  EXPECT_EQ(int64_t{0}, v->Min());
  EXPECT_EQ(int64_t{35}, v->Max());
  EXPECT_FALSE(b->Bound());
  v->SetMin(int64_t{15});
  EXPECT_FALSE(b->Bound());
  EXPECT_EQ(int64_t{15}, v->Min());
  EXPECT_EQ(int64_t{35}, v->Max());
  b->SetValue(int64_t{0});
  EXPECT_EQ(int64_t{20}, v->Size());
  EXPECT_EQ(int64_t{15}, v->Min());
  EXPECT_EQ(int64_t{35}, v->Max());
  EXPECT_FALSE(b->Contains(int64_t{30}));
  return nullptr;
}

TEST_IN_PROPAGATION0(IsMemberCtTest, NoIntersection, s, true) {
  IntVar* const v = s->MakeIntVar(60, 100, "v");
  IntVar* const b = s->MakeBoolVar("b");
  Constraint* ct = nullptr;
  {
    std::vector<int> values;
    values.push_back(10);
    values.push_back(30);
    values.push_back(30);
    values.push_back(-5);
    values.push_back(110);
    values.push_back(50);
    ct = s->MakeIsMemberCt(v, values, b);
  }
  s->AddConstraint(ct);
  EXPECT_TRUE(b->Bound());
  EXPECT_EQ(int64_t{0}, b->Min());
  return nullptr;
}

TEST_IN_PROPAGATION0(IsMemberCtTest, NoIntersectionUsingInt, s, true) {
  IntVar* const v = s->MakeIntVar(60, 100, "v");
  std::vector<int> values;
  values.push_back(10);
  values.push_back(30);
  values.push_back(30);
  values.push_back(-5);
  values.push_back(110);
  values.push_back(50);
  IntVar* const b = s->MakeBoolVar("b");
  s->AddConstraint(s->MakeIsMemberCt(v, values, b));
  EXPECT_TRUE(b->Bound());
  EXPECT_EQ(int64_t{0}, b->Min());
  return nullptr;
}

TEST_IN_PROPAGATION0(IsMemberCtTest, Bugs, s, true) {
  // Bug where the vmax and vmin where init'ed to v->Min().
  IntVar* const v = s->MakeIntVar(0, 100, "v");
  v->RemoveValue(10);
  std::vector<int64_t> values;
  values.push_back(10);
  values.push_back(30);
  values.push_back(40);
  values.push_back(60);
  values.push_back(110);
  values.push_back(50);
  IntVar* const b = s->MakeBoolVar("b");
  s->AddConstraint(s->MakeIsMemberCt(v, values, b));
  EXPECT_FALSE(b->Bound());
  return nullptr;
}

TEST_IN_PROPAGATION0(IsMemberCtTest, BugsUsingInt, s, true) {
  // Bug where the vmax and vmin where init'ed to v->Min().
  IntVar* const v = s->MakeIntVar(0, 100, "v");
  v->RemoveValue(10);
  std::vector<int> values;
  values.push_back(10);
  values.push_back(30);
  values.push_back(40);
  values.push_back(60);
  values.push_back(110);
  values.push_back(50);
  IntVar* const b = s->MakeBoolVar("b");
  s->AddConstraint(s->MakeIsMemberCt(v, values, b));
  EXPECT_FALSE(b->Bound());
  return nullptr;
}

TEST_F(IsMemberCtTest, Presolve1) {
  IntVar* const x = solver_->MakeIntVar(0, 100, "x");
  std::vector<int64_t> values1 = {-5, 10, 110};
  IntVar* const b = solver_->MakeBoolVar("b");
  Constraint* const base = solver_->MakeIsEqualCstCt(x, 10, b);
  Constraint* const ct1 = solver_->MakeIsMemberCt(x, values1, b);
  EXPECT_EQ(ct1->DebugString(), base->DebugString());
  std::vector<int64_t> values2 = {10, 10, 10, 10};
  Constraint* const ct2 = solver_->MakeIsMemberCt(x, values2, b);
  EXPECT_EQ(ct2->DebugString(), base->DebugString());
}

TEST_F(IsMemberCtTest, Presolve2) {
  IntVar* const x = solver_->MakeIntVar(0, 100, "x");
  std::vector<int64_t> values1 = {3, 5, 7, 2, 4, 6, 125};
  IntVar* const b = solver_->MakeBoolVar("b");
  Constraint* const base = solver_->MakeIsBetweenCt(x, 2, 7, b);
  Constraint* const ct1 = solver_->MakeIsMemberCt(x, values1, b);
  EXPECT_EQ(ct1->DebugString(), base->DebugString());
}

// ----- Simplification Test -----

TEST(SimplificationTest, IsEqualCstOnBounds) {
  Solver s("IsDiffCst");
  IntVar* const v = s.MakeIntVar(0, 10);
  IntVar* const b1 = s.MakeIsEqualCstVar(v, 0);
  IntVar* const b2 = s.MakeIsEqualCstVar(v, 10);
  IntVar* const b3 = s.MakeIsLessOrEqualCstVar(v, 0);
  IntVar* const b4 = s.MakeIsGreaterOrEqualCstVar(v, 10);
  EXPECT_EQ(b1, b3);
  EXPECT_EQ(b2, b4);
}

TEST(SimplificationTest, IsEqualCstOutOfBounds) {
  Solver s("IsDiffCst");
  IntVar* const v = s.MakeIntVar(0, 10);
  IntVar* const false_cst = s.MakeIntConst(0);
  IntVar* const b1 = s.MakeIsEqualCstVar(v, -2);
  EXPECT_EQ(b1, false_cst);
}

TEST(SimplificationTest, IsEqualCstInBounds) {
  Solver s("IsDiffCst");
  IntVar* const v = s.MakeIntVar(10, 10);
  IntVar* const true_cst = s.MakeIntConst(1);
  IntVar* const b1 = s.MakeIsEqualCstVar(v, 10);
  EXPECT_EQ(b1, true_cst);
}

TEST(SimplificationTest, IsDiffCstOnBounds) {
  Solver s("IsDiffCst");
  IntVar* const v = s.MakeIntVar(0, 10);
  IntVar* const b1 = s.MakeIsDifferentCstVar(v, 0);
  IntVar* const b2 = s.MakeIsDifferentCstVar(v, 10);
  IntVar* const b3 = s.MakeIsGreaterOrEqualCstVar(v, 1);
  IntVar* const b4 = s.MakeIsLessOrEqualCstVar(v, 9);
  EXPECT_EQ(b1, b3);
  EXPECT_EQ(b2, b4);
}

TEST(SimplificationTest, IsDiffCstOutOfBounds) {
  Solver s("IsDiffCst");
  IntVar* const v = s.MakeIntVar(0, 10);
  IntVar* const true_cst = s.MakeIntConst(1);
  IntVar* const b1 = s.MakeIsDifferentCstVar(v, -2);
  EXPECT_EQ(b1, true_cst);
}

TEST(SimplificationTest, IsDiffCstInBounds) {
  Solver s("IsDiffCst");
  IntVar* const v = s.MakeIntVar(10, 10);
  IntVar* const false_cst = s.MakeIntConst(0);
  IntVar* const b1 = s.MakeIsDifferentCstVar(v, 10);
  EXPECT_EQ(b1, false_cst);
}

TEST(SimplificationTest, IsGreaterCstInBounds) {
  Solver s("IsDiffCst");
  IntVar* const v = s.MakeIntVar(0, 10);
  IntVar* const true_cst = s.MakeIntConst(1);
  IntVar* const b1 = s.MakeIsGreaterOrEqualCstVar(v, -2);
  EXPECT_EQ(b1, true_cst);
}

TEST(SimplificationTest, IsCreatorOutOfBounds) {
  Solver s("IsDiffCst");
  IntVar* const v = s.MakeIntVar(0, 10);
  IntVar* const false_cst = s.MakeIntConst(0);
  IntVar* const b1 = s.MakeIsGreaterOrEqualCstVar(v, 11);
  EXPECT_EQ(b1, false_cst);
}

TEST(SimplificationTest, IsLessCstInBounds) {
  Solver s("IsDiffCst");
  IntVar* const v = s.MakeIntVar(0, 10);
  IntVar* const true_cst = s.MakeIntConst(1);
  IntVar* const b1 = s.MakeIsLessOrEqualCstVar(v, 12);
  EXPECT_EQ(b1, true_cst);
}

TEST(SimplificationTest, IsLessOutOfBounds) {
  Solver s("IsDiffCst");
  IntVar* const v = s.MakeIntVar(0, 10);
  IntVar* const false_cst = s.MakeIntConst(0);
  IntVar* const b1 = s.MakeIsLessOrEqualCstVar(v, -1);
  EXPECT_EQ(b1, false_cst);
}

static const int64_t kLargeMax = 0x1FFFFFFFFll;  // > 2^32
static const int64_t kSmallRemovedValue = 1000;
static const int64_t kLargeRemovedValue = 0x1FFFFFF00ll;

class LargeDiffCstTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("LargeDiffCstTest");
    var_ = solver_->MakeIntVar(0, kLargeMax);
  }
  std::unique_ptr<Solver> solver_;
  IntVar* var_;
};

TEST_IN_PROPAGATION1(LargeDiffCstTest, SetMin, s, true, IntVar*, var_) {
  s->AddConstraint(s->MakeNonEquality(var_, kSmallRemovedValue));
  EXPECT_EQ(kLargeMax + 1, var_->Size());
  var_->SetMin(4);
  EXPECT_EQ(4, var_->Min());
  var_->SetMin(kSmallRemovedValue + 4);
  EXPECT_EQ(kSmallRemovedValue + 4, var_->Min());
  return nullptr;
}

TEST_IN_PROPAGATION1(LargeDiffCstTest, SetExactMin, s, true, IntVar*, var_) {
  s->AddConstraint(s->MakeNonEquality(var_, kSmallRemovedValue));
  EXPECT_EQ(kLargeMax + 1, var_->Size());
  var_->SetMin(kSmallRemovedValue);
  EXPECT_EQ(kSmallRemovedValue + 1, var_->Min());
  var_->SetMin(kSmallRemovedValue + 4);
  EXPECT_EQ(kSmallRemovedValue + 4, var_->Min());
  return nullptr;
}

TEST_IN_PROPAGATION1(LargeDiffCstTest, SetMax, s, true, IntVar*, var_) {
  s->AddConstraint(s->MakeNonEquality(var_, kSmallRemovedValue));
  EXPECT_EQ(kLargeMax + 1, var_->Size());
  var_->SetMax(kSmallRemovedValue + 4);
  EXPECT_EQ(kSmallRemovedValue + 4, var_->Max());
  EXPECT_FALSE(var_->Contains(kSmallRemovedValue));
  return nullptr;
}

TEST_IN_PROPAGATION1(LargeDiffCstTest, SetExactMax, s, true, IntVar*, var_) {
  s->AddConstraint(s->MakeNonEquality(var_, kSmallRemovedValue));
  EXPECT_EQ(kLargeMax + 1, var_->Size());
  var_->SetMax(kSmallRemovedValue);
  EXPECT_EQ(kSmallRemovedValue - 1, var_->Max());
  var_->SetMax(kSmallRemovedValue - 3);
  EXPECT_EQ(kSmallRemovedValue - 3, var_->Max());
  return nullptr;
}

TEST_IN_PROPAGATION1(LargeDiffCstTest, SetMinLarge, s, true, IntVar*, var_) {
  s->AddConstraint(s->MakeNonEquality(var_, kLargeRemovedValue));
  EXPECT_EQ(kLargeMax + 1, var_->Size());
  var_->SetMin(4);
  EXPECT_EQ(4, var_->Min());
  var_->SetMin(kLargeRemovedValue + 4);
  EXPECT_EQ(kLargeRemovedValue + 4, var_->Min());
  return nullptr;
}

TEST_IN_PROPAGATION1(LargeDiffCstTest, SetExactMinLarge, s, true, IntVar*,
                     var_) {
  s->AddConstraint(s->MakeNonEquality(var_, kLargeRemovedValue));
  EXPECT_EQ(kLargeMax + 1, var_->Size());
  var_->SetMin(kLargeRemovedValue);
  EXPECT_EQ(kLargeRemovedValue + 1, var_->Min());
  var_->SetMin(kLargeRemovedValue + 4);
  EXPECT_EQ(kLargeRemovedValue + 4, var_->Min());
  return nullptr;
}

TEST_IN_PROPAGATION1(LargeDiffCstTest, SetMaxLarge, s, true, IntVar*, var_) {
  s->AddConstraint(s->MakeNonEquality(var_, kLargeRemovedValue));
  EXPECT_EQ(kLargeMax + 1, var_->Size());
  var_->SetMax(kLargeRemovedValue + 4);
  EXPECT_EQ(kLargeRemovedValue + 4, var_->Max());
  EXPECT_TRUE(var_->Contains(kLargeRemovedValue));
  var_->SetMax(kLargeRemovedValue - 4);
  EXPECT_EQ(kLargeRemovedValue - 4, var_->Max());
  return nullptr;
}

TEST_IN_PROPAGATION1(LargeDiffCstTest, SetExactMaxLarge, s, true, IntVar*,
                     var_) {
  s->AddConstraint(s->MakeNonEquality(var_, kLargeRemovedValue));
  EXPECT_EQ(kLargeMax + 1, var_->Size());
  var_->SetMax(kLargeRemovedValue);
  EXPECT_EQ(kLargeRemovedValue - 1, var_->Max());
  var_->SetMax(kLargeRemovedValue - 3);
  EXPECT_EQ(kLargeRemovedValue - 3, var_->Max());
  return nullptr;
}

class LargeMakeNonEqualityTest : public testing::Test {
 protected:
  LargeMakeNonEqualityTest()
      : solver_(new Solver("LargeMakeNonEqualityTest")) {}
  std::unique_ptr<Solver> solver_;
};

TEST_IN_PROPAGATION0(LargeMakeNonEqualityTest, BoundReduction, solver, true) {
  IntVar* const var = solver->MakeIntVar(kint64min, kint64max);
  solver->AddConstraint(solver->MakeNonEquality(var, 10));
  EXPECT_TRUE(var->Contains(10));  // Domain too large, delayed reduction
  var->SetRange(-11, 10);
  EXPECT_EQ(9, var->Max());
  EXPECT_EQ(-11, var->Min());
  return nullptr;
}

TEST(StatusVarTest, Equality) {
  Solver solver("StatusVarTest::TestEquality");
  IntVar* const x = solver.MakeIntVar(0, 10, "x");
  IntVar* const y = solver.MakeIntVar(0, 10, "y");
  Constraint* const c1 = solver.MakeEquality(x, 4);
  EXPECT_EQ(c1->Var(), solver.MakeIsEqualCstVar(x, 4));
  Constraint* const c2 = solver.MakeNonEquality(x, 5);
  EXPECT_EQ(c2->Var(), solver.MakeIsDifferentCstVar(x, 5));
  Constraint* const c3 = solver.MakeGreater(x, 2);
  EXPECT_EQ(c3->Var(), solver.MakeIsGreaterCstVar(x, 2));
  Constraint* const c4 = solver.MakeLess(x, 7);
  EXPECT_EQ(c4->Var(), solver.MakeIsLessCstVar(x, 7));
  Constraint* const c5 = solver.MakeGreaterOrEqual(x, 2);
  EXPECT_EQ(c5->Var(), solver.MakeIsGreaterOrEqualCstVar(x, 2));
  Constraint* const c6 = solver.MakeLessOrEqual(x, 7);
  EXPECT_EQ(c6->Var(), solver.MakeIsLessOrEqualCstVar(x, 7));
  Constraint* const c7 = solver.MakeEquality(x, y);
  EXPECT_EQ(c7->Var(), solver.MakeIsEqualVar(x, y));
  Constraint* const c8 = solver.MakeGreaterOrEqual(x, y);
  EXPECT_EQ(c8->Var(), solver.MakeIsGreaterOrEqualVar(x, y));
  Constraint* const c9 = solver.MakeLessOrEqual(x, y);
  EXPECT_EQ(c9->Var(), solver.MakeIsLessOrEqualVar(x, y));
  Constraint* const c10 = solver.MakeGreater(x, y);
  EXPECT_EQ(c10->Var(), solver.MakeIsGreaterVar(x, y));
  Constraint* const c11 = solver.MakeLess(x, y);
  EXPECT_EQ(c11->Var(), solver.MakeIsLessVar(x, y));
  Constraint* const c12 = solver.MakeNonEquality(x, y);
  EXPECT_EQ(c12->Var(), solver.MakeIsDifferentVar(x, y));
}

// NotMemberCtTest

TEST_IN_PROPAGATION(NotMemberCtIntervalsTest, NoInterval, s, true) {
  IntVar* const x = s->MakeIntVar(0, 100, "x");
  std::vector<int64_t> start;
  std::vector<int64_t> end;
  Constraint* const ct = s->MakeNotMemberCt(x, start, end);
  s->AddConstraint(ct);
  EXPECT_EQ_RANGE(x, 0, 100);
  return nullptr;
}

TEST_IN_PROPAGATION(NotMemberCtIntervalsTest, SingleInterval, s, true) {
  IntVar* const x = s->MakeIntVar(0, 100, "x");
  std::vector<int64_t> start = {99};
  std::vector<int64_t> end = {kint64max};
  Constraint* const ct = s->MakeNotMemberCt(x, start, end);
  s->AddConstraint(ct);
  EXPECT_EQ_RANGE(x, 0, 98);
  return nullptr;
}

TEST_IN_PROPAGATION(NotMemberCtIntervalsTest, PropagateMin, s, true) {
  IntVar* const x = s->MakeIntVar(0, 100, "x");
  std::vector<int64_t> s1 = {0, 20, 40};
  std::vector<int64_t> e1 = {10, 30, 50};
  Constraint* const ct = s->MakeNotMemberCt(x, s1, e1);
  EXPECT_EQ_RANGE(x, 0, 100);
  s->AddConstraint(ct);
  EXPECT_EQ_RANGE(x, 11, 100);
  x->SetMin(15);
  EXPECT_EQ_RANGE(x, 15, 100);
  x->SetMax(60);
  EXPECT_EQ_RANGE(x, 15, 60);
  x->SetMin(22);
  EXPECT_EQ_RANGE(x, 31, 60);
  x->SetMax(48);
  EXPECT_EQ_RANGE(x, 31, 39);
  return nullptr;
}

TEST_IN_PROPAGATION(NotMemberCtIntervalsTest, PropagateMax, s, true) {
  IntVar* const x = s->MakeIntVar(0, 100, "x");
  std::vector<int64_t> s1 = {10, 30, 80};
  std::vector<int64_t> e1 = {20, 40, 100};
  Constraint* const ct = s->MakeNotMemberCt(x, s1, e1);
  EXPECT_EQ_RANGE(x, 0, 100);
  s->AddConstraint(ct);
  EXPECT_EQ_RANGE(x, 0, 79);
  x->SetMax(75);
  EXPECT_EQ_RANGE(x, 0, 75);
  x->SetMin(11);
  EXPECT_EQ_RANGE(x, 21, 75);
  x->SetMax(38);
  EXPECT_EQ_RANGE(x, 21, 29);
  return nullptr;
}

TEST_IN_PROPAGATION(NotMemberCtIntervalsTest, PropagateMaxFromIntervalList, s,
                    true) {
  IntVar* const x = s->MakeIntVar(0, 100, "x");
  std::vector<int64_t> s1 = {10, 30, 80};
  std::vector<int64_t> e1 = {20, 40, 100};
  Constraint* const ct =
      s->MakeNotMemberCt(x, SortedDisjointIntervalList(s1, e1));
  EXPECT_EQ_RANGE(x, 0, 100);
  s->AddConstraint(ct);
  EXPECT_EQ_RANGE(x, 0, 79);
  x->SetMax(75);
  EXPECT_EQ_RANGE(x, 0, 75);
  x->SetMin(11);
  EXPECT_EQ_RANGE(x, 21, 75);
  x->SetMax(38);
  EXPECT_EQ_RANGE(x, 21, 29);
  return nullptr;
}

}  // namespace
}  // namespace operations_research
