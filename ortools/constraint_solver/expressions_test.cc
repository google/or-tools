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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/linked_hash_set.h"
#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "gtest/gtest.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solver_test_util.h"
#include "ortools/constraint_solver/reversible_data.h"
#include "ortools/constraint_solver/reversible_engine.h"
#include "ortools/util/piecewise_linear_function.h"
#include "ortools/util/saturated_arithmetic.h"

ABSL_DECLARE_FLAG(bool, cp_disable_expression_optimization);
ABSL_DECLARE_FLAG(bool, cp_share_int_consts);

namespace {
class DivExprExprTest_SimpleTest_Test;
}
namespace {
class TimesIntPosExprTest_TestSetGrowingMin_Test;
}
namespace {
class TimesIntPosExprTest_TestSetMaxFail_Test;
}
namespace {
class TimesIntPosExprTest_TestSetMinFail_Test;
}

namespace operations_research {
namespace {

class NullDemon : public Demon {
  void Run(Solver* const solver) override {}
};

// ----- First Expression Test -----

class ExpressionsTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("ExpressionTest");
  }
  std::unique_ptr<Solver> solver_;
};

TEST_F(ExpressionsTest, Bitset) {
  IntVar* var = solver_->MakeIntVar(2, 18, "var");
  solver_->PushState();
  var->SetMin(5);
  EXPECT_EQ(5, var->Min());
  EXPECT_EQ(14, var->Size());
  solver_->PushState();
  var->RemoveValue(8);
  EXPECT_EQ(13, var->Size());
  EXPECT_FALSE(var->Contains(8));
  solver_->PushState();
  var->RemoveValue(17);
  EXPECT_EQ(12, var->Size());
  EXPECT_FALSE(var->Contains(17));
  solver_->PopState();
  EXPECT_EQ(13, var->Size());
  EXPECT_TRUE(var->Contains(17));
  solver_->PopState();
  EXPECT_EQ(14, var->Size());
  EXPECT_TRUE(var->Contains(8));
  solver_->PopState();
  EXPECT_EQ(2, var->Min());
}

TEST_F(ExpressionsTest, ArrayIntVarAtZero) {
  const std::vector<int> values = {0};
  IntVar* const var = solver_->MakeIntVar(values, "var");
  EXPECT_TRUE(var->Bound());
  EXPECT_EQ(0, var->Min());
}

// ----- test reversibility in SimpleRevFIFO class -----

int SumList(SimpleRevFIFO<int>* l) {
  int res = 0;
  for (SimpleRevFIFO<int>::Iterator it(l); it.ok(); ++it) res += *it;
  return res;
}

TEST_F(ExpressionsTest, SimpleRevFIFO) {
  SimpleRevFIFO<int> list;
  solver_->PushState();
  list.Push(solver_.get(), 3);
  EXPECT_EQ(3, SumList(&list)) << "Wrong initial Sum in rev int RevFIFO";
  solver_->PushState();
  list.PushIfNotTop(solver_.get(), 5);
  list.PushIfNotTop(solver_.get(), 5);
  solver_->PushState();
  EXPECT_EQ(8, SumList(&list)) << "Wrong Sum in rev int RevFIFO";
  list.Push(solver_.get(), 2);
  EXPECT_EQ(10, SumList(&list)) << "Wrong Sum in rev int RevFIFO";
  solver_->PushState();
  EXPECT_EQ(2, list.LastValue()) << "Wrong top in rev int RevFIFO";
  EXPECT_EQ(10, SumList(&list)) << "Wrong Sum in rev int RevFIFO";
  solver_->PopState();
  EXPECT_EQ(10, SumList(&list))
      << "Wrong Sum in rev int RevFIFO after 1 backtrack";
  EXPECT_EQ(2, list.LastValue()) << "Wrong top in rev int RevFIFO";
  solver_->PopState();
  EXPECT_EQ(8, SumList(&list))
      << "Wrong Sum in rev int RevFIFO after 2 backtrack";
  EXPECT_EQ(5, list.LastValue()) << "Wrong top in rev int RevFIFO";
  solver_->PopState();
  EXPECT_EQ(3, SumList(&list))
      << "Wrong Sum in rev int RevFIFO after 3 backtracks";
  EXPECT_EQ(3, list.LastValue()) << "Wrong top in rev int RevFIFO";
  solver_->PopState();
  EXPECT_EQ(nullptr, list.Last()) << "Wrong empty rev int RevFIFO";
}

// test propagation loop, constraint adding...
// The test will involve the following
// A pseudo var with one WatchedRevInt
// An immediate demon containing the lists
// A decision builder increasing the value of the watched rev integer

class WatchedRevInt32 : public BaseObject {
 public:
  WatchedRevInt32(Solver* const solver, int value)
      : solver_(solver), value_(value), demons_() {}

  // This type is neither copyable nor movable.
  WatchedRevInt32(const WatchedRevInt32&) = delete;
  WatchedRevInt32& operator=(const WatchedRevInt32&) = delete;
  ~WatchedRevInt32() override {}
  int Value() const { return value_; }
  void AddDemon(Demon* demon) { demons_.Push(solver_, demon); }
  void SetValue(int value) {
    solver_->SaveAndSetValue(&value_, value);
    for (SimpleRevFIFO<Demon*>::Iterator it(&demons_); it.ok(); ++it) {
      (*it)->Run(solver_);
    }
  }

 private:
  Solver* const solver_;
  int value_;
  SimpleRevFIFO<Demon*> demons_;
};

class SetWatchedRevIntValue : public Decision {
 public:
  SetWatchedRevIntValue(WatchedRevInt32* wri, int value)
      : Decision(), wri_(wri), value_(value) {}
  ~SetWatchedRevIntValue() override {}
  void Apply(Solver* const solver) override { wri_->SetValue(value_); }
  void Refute(Solver* const solver) override {
    LOG(INFO) << "Refuting decision";
  }
  std::string DebugString() const override {
    return absl::StrFormat("SetValue(%d) ", value_);
  }

 private:
  WatchedRevInt32* const wri_;
  int value_;
};

class IncreaseRevInt : public DecisionBuilder {
 public:
  IncreaseRevInt(WatchedRevInt32* wri, int maxval)
      : DecisionBuilder(), wri_(wri), maxval_(maxval) {}
  Decision* Next(Solver* const solver) override {
    if (wri_->Value() < maxval_) {
      return solver->RevAlloc(
          new SetWatchedRevIntValue(wri_, wri_->Value() + 1));
    }
    return nullptr;
  }
  std::string DebugString() const override {
    return absl::StrFormat("IncreaseRevInt(%d) ", maxval_);
  }

 public:
  WatchedRevInt32* const wri_;
  int maxval_;
};

class FollowWatchedRevInt : public Constraint {
 public:
  class MyDemon : public Demon {
   public:
    MyDemon(WatchedRevInt32* wri, int* cnt) : Demon(), wri_(wri), cnt_(cnt) {}
    ~MyDemon() override {}
    void Run(Solver* const solver) override { (*cnt_)++; }

   private:
    WatchedRevInt32* wri_;
    int* cnt_;
  };

  FollowWatchedRevInt(Solver* const solver, WatchedRevInt32* wri, int* cnt,
                      bool post, int* cnt2)
      : Constraint(solver), wri_(wri), cnt_(cnt), post_(post), cnt2_(cnt2) {}
  void Post() override {
    Solver* const s = solver();
    wri_->AddDemon(s->RevAlloc(new MyDemon(wri_, cnt_)));
    if (post_) {
      s->AddConstraint(
          s->RevAlloc(new FollowWatchedRevInt(s, wri_, cnt2_, false, nullptr)));
    }
  }
  void InitialPropagate() override { wri_->SetValue(wri_->Value() + 2); }

  void Accept(ModelVisitor* const visitor) const override {
    const std::string kId = "Dummy";
    visitor->BeginVisitConstraint(kId, this);
    visitor->EndVisitConstraint(kId, this);
  }

 private:
  WatchedRevInt32* const wri_;
  int* cnt_;
  bool post_;
  int* cnt2_;
};

TEST_F(ExpressionsTest, WatchedRevInt) {
  WatchedRevInt32* wri =
      solver_->RevAlloc(new WatchedRevInt32(solver_.get(), 0));
  int count1 = 0;
  int count3 = 0;
  FollowWatchedRevInt* fwri1 = solver_->RevAlloc(
      new FollowWatchedRevInt(solver_.get(), wri, &count1, true, &count3));
  int count2 = 0;
  FollowWatchedRevInt* fwri2 = solver_->RevAlloc(
      new FollowWatchedRevInt(solver_.get(), wri, &count2, false, nullptr));
  solver_->AddConstraint(fwri1);
  solver_->AddConstraint(fwri2);
  IncreaseRevInt* iri = solver_->RevAlloc(new IncreaseRevInt(wri, 10));
  solver_->NewSearch(iri);
  solver_->NextSolution();
  EXPECT_EQ(10, wri->Value());
  EXPECT_EQ(7, count1);
  EXPECT_EQ(6, count2);
  EXPECT_EQ(5, count3);
  solver_->EndSearch();
}

// ----- BaseIntExprTest -----

class BaseIntExprTest : public testing::Test {
 protected:
  explicit BaseIntExprTest(const std::string& name) : name_(name) {}
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>(name_);
    MakeVars(&vars_);
    expr_ = MakeExpr();
  }
  virtual IntExpr* MakeExpr() = 0;
  virtual void MakeVars(std::vector<IntVar*>* vars) = 0;

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  IntExpr* expr_;
  std::string name_;
};

class BaseIntExprTest1 : public BaseIntExprTest {
 protected:
  explicit BaseIntExprTest1(const std::string& name) : BaseIntExprTest(name) {}
  void MakeVars(std::vector<IntVar*>* vars) override {
    for (int i = 0; i < 2; ++i) {
      vars->push_back(solver_->MakeIntVar(i + 2, 3 * i + 3));
    }
  }
};

class BaseNegativeIntExprTest1 : public BaseIntExprTest {
 protected:
  explicit BaseNegativeIntExprTest1(const std::string& name)
      : BaseIntExprTest(name) {}
  void MakeVars(std::vector<IntVar*>* vars) override {
    for (int i = 0; i < 2; ++i) {
      vars->push_back(solver_->MakeIntVar(-3 * i - 3, -i - 2));
    }
  }
};

class BaseOpenIntExprTest1 : public BaseIntExprTest {
 protected:
  explicit BaseOpenIntExprTest1(const std::string& name)
      : BaseIntExprTest(name) {}
  void MakeVars(std::vector<IntVar*>* vars) override {
    for (int i = 0; i < 2; ++i) {
      vars->push_back(solver_->MakeIntVar(kint64min, kint64max));
    }
  }
};

// ----- IntConst tests -----

class IntConstTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_share_int_consts, true);
    solver_ = std::make_unique<Solver>("IntConstTest");
  }

  static const int64_t kZero;
  std::unique_ptr<Solver> solver_;
};

const int64_t IntConstTest::kZero = 0LL;

TEST_F(IntConstTest, Value) {
  IntVar* zero_const = solver_->MakeIntConst(kZero);
  EXPECT_EQ(kZero, zero_const->Value());
}

TEST_F(IntConstTest, DebugString) {
  const char* kOtherName = "Other";
  IntVar* zero_const = solver_->MakeIntConst(kZero);
  EXPECT_EQ(absl::StrFormat("IntConst(%d)", kZero), zero_const->DebugString());
  IntVar* other_zero_const = solver_->MakeIntConst(kZero);
  EXPECT_EQ(other_zero_const, zero_const);
  EXPECT_EQ(absl::StrFormat("IntConst(%d)", kZero), zero_const->DebugString());
  other_zero_const->set_name(kOtherName);
  EXPECT_EQ(absl::StrFormat("%s(%d)", kOtherName, kZero),
            zero_const->DebugString());
  EXPECT_EQ(absl::StrFormat("%s(%d)", kOtherName, kZero),
            other_zero_const->DebugString());
}

TEST_F(IntConstTest, DebugStringNoSharing) {
  const char* kOtherName = "Other";
  absl::SetFlag(&FLAGS_cp_share_int_consts, false);
  IntVar* zero_const = solver_->MakeIntConst(kZero);
  EXPECT_EQ(absl::StrFormat("IntConst(%d)", kZero), zero_const->DebugString());
  IntVar* other_zero_const = solver_->MakeIntConst(kZero);
  EXPECT_NE(other_zero_const, zero_const);
  EXPECT_EQ(absl::StrFormat("IntConst(%d)", kZero), zero_const->DebugString());
  other_zero_const->set_name(kOtherName);
  EXPECT_EQ(absl::StrFormat("IntConst(%d)", kZero), zero_const->DebugString());
  EXPECT_EQ(absl::StrFormat("%s(%d)", kOtherName, kZero),
            other_zero_const->DebugString());
  absl::SetFlag(&FLAGS_cp_share_int_consts, true);
}

TEST_F(IntConstTest, DebugStringNamedConst) {
  const char* kOneName = "One";
  const char* kOtherName = "Other";
  IntVar* zero_const = solver_->MakeIntConst(kZero);
  EXPECT_EQ(absl::StrFormat("IntConst(%d)", kZero), zero_const->DebugString());
  IntVar* other_zero_const = solver_->MakeIntConst(0, kOneName);
  EXPECT_NE(other_zero_const, zero_const);
  EXPECT_EQ(absl::StrFormat("%s(%d)", kOneName, kZero),
            other_zero_const->DebugString());
  other_zero_const->set_name(kOtherName);
  EXPECT_EQ(absl::StrFormat("IntConst(%d)", kZero), zero_const->DebugString());
  EXPECT_EQ(absl::StrFormat("%s(%d)", kOtherName, kZero),
            other_zero_const->DebugString());
}

// ----- PlusIntExpr -----

class PlusIntExprTest : public BaseIntExprTest1 {
 protected:
  PlusIntExprTest() : BaseIntExprTest1("PlusIntExprTest") {}
  IntExpr* MakeExpr() override { return solver_->MakeSum(vars_[0], vars_[1]); }
};

class PlusNegativeIntExprTest : public BaseNegativeIntExprTest1 {
 protected:
  PlusNegativeIntExprTest()
      : BaseNegativeIntExprTest1("PlusNegativeIntExprTest") {}
  IntExpr* MakeExpr() override { return solver_->MakeSum(vars_[0], vars_[1]); }
};

TEST_F(PlusIntExprTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
}

TEST_F(PlusIntExprTest, MinMaxGetters) {
  EXPECT_EQ(5, expr_->Min());
  EXPECT_EQ(9, expr_->Max());
}

TEST_F(PlusIntExprTest, SetMaxToMin) {
  expr_->SetMax(5);
  EXPECT_EQ(5, expr_->Max());
  EXPECT_TRUE(expr_->Bound());
}

TEST_F(PlusIntExprTest, SetLargeMax) {
  expr_->SetMax(kint64max - 2);
  EXPECT_EQ(5, expr_->Min());
  EXPECT_EQ(9, expr_->Max());
}

TEST_F(PlusIntExprTest, SetMinToMax) {
  expr_->SetMin(9);
  EXPECT_EQ(9, expr_->Min());
  EXPECT_TRUE(expr_->Bound());
}

TEST_F(PlusIntExprTest, SetLargeMin) {
  expr_->SetMin(kint64min + 2);
  EXPECT_EQ(5, expr_->Min());
  EXPECT_EQ(9, expr_->Max());
}

TEST_F(PlusIntExprTest, SetLargeRange) {
  expr_->SetRange(kint64min + 2, kint64max - 2);
  EXPECT_EQ(5, expr_->Min());
  EXPECT_EQ(9, expr_->Max());
}

TEST_F(PlusIntExprTest, SetRangeNearMin) {
  expr_->SetRange(6, 7);
  EXPECT_EQ(2, vars_[0]->Min());
  EXPECT_EQ(3, vars_[0]->Max());
  EXPECT_EQ(3, vars_[1]->Min());
  EXPECT_EQ(5, vars_[1]->Max());
}

TEST_F(PlusIntExprTest, SetRangeNearMax) {
  expr_->SetRange(8, 9);
  EXPECT_EQ(2, vars_[0]->Min());
  EXPECT_EQ(3, vars_[0]->Max());
  EXPECT_EQ(5, vars_[1]->Min());
  EXPECT_EQ(6, vars_[1]->Max());
}

TEST_IN_PROPAGATION1(PlusIntExprTest, SetRangeUnderflow, solver, false,
                     IntExpr*, expr_) {
  expr_->SetRange(kint64min, kint64min);
  return nullptr;
}

TEST_IN_PROPAGATION1(PlusNegativeIntExprTest, SetRangeOverflow, solver, false,
                     IntExpr*, expr_) {
  expr_->SetRange(kint64max, kint64max);
  return nullptr;
}

TEST_F(PlusIntExprTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr_->WhenRange(d);
}

TEST_F(PlusIntExprTest, IntermediateOverflow) {
  for (const bool swap : {false, true}) {
    for (int offset = 0; offset < 10; ++offset) {
      IntVar* left = solver_->MakeIntVar(kint64min + offset, 0);
      IntVar* right = solver_->MakeIntVar(0, 5);
      IntVar* tested_var = right;
      if (swap) std::swap(left, right);
      IntExpr* sum = solver_->MakeSum(left, right);
      sum->SetMax(4);  // Should not overflow.
      EXPECT_EQ(0, tested_var->Min());
      EXPECT_EQ(5, tested_var->Max());
    }
  }
}

class PlusOpenIntExprTest : public BaseOpenIntExprTest1 {
 protected:
  PlusOpenIntExprTest() : BaseOpenIntExprTest1("PlusOpenIntExprTest") {}
  IntExpr* MakeExpr() override { return solver_->MakeSum(vars_[0], vars_[1]); }
};

TEST_F(PlusOpenIntExprTest, MinMaxGetters) {
  EXPECT_EQ(kint64min, expr_->Min());
  EXPECT_EQ(kint64max, expr_->Max());
}

TEST_F(PlusOpenIntExprTest, SetMaxToMin) {
  expr_->SetMax(5);
  EXPECT_EQ(kint64max, expr_->Max());
  expr_->SetMax(kint64min);
  EXPECT_EQ(0, expr_->Max());
}

TEST_F(PlusOpenIntExprTest, SetMinToMax) {
  expr_->SetMin(9);
  EXPECT_EQ(kint64min, expr_->Min());
  expr_->SetMin(kint64max);
  EXPECT_EQ(0, expr_->Min());
}

TEST_F(PlusOpenIntExprTest, SetRangeNearMin) {
  expr_->SetRange(kint64min, kint64max);
  EXPECT_EQ(kint64min, vars_[0]->Min());
  EXPECT_EQ(kint64max, vars_[0]->Max());
  EXPECT_EQ(kint64min, vars_[1]->Min());
  EXPECT_EQ(kint64max, vars_[1]->Max());
  const int64_t new_min = 6;
  const int64_t new_max = 7;
  expr_->SetRange(new_min, new_max);
  EXPECT_EQ(kint64min + new_max, vars_[0]->Min());
  EXPECT_EQ(kint64max, vars_[0]->Max());
  EXPECT_EQ(kint64min + new_max, vars_[1]->Min());
  EXPECT_EQ(kint64max, vars_[1]->Max());
}

// ----- PlusIntCstExpr -----

class PlusIntCstExprTest : public BaseIntExprTest1 {
 protected:
  PlusIntCstExprTest() : BaseIntExprTest1("PlusIntCstExprTest") {}
  IntExpr* MakeExpr() override { return solver_->MakeSum(vars_[1], 10); }
};

TEST_F(PlusIntCstExprTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
}

TEST_F(PlusIntCstExprTest, MinMaxGetters) {
  EXPECT_EQ(13, expr_->Min());
  EXPECT_EQ(16, expr_->Max());
}

TEST_F(PlusIntCstExprTest, SetMaxToMin) {
  expr_->SetMax(13);
  EXPECT_EQ(13, expr_->Max());
  EXPECT_TRUE(expr_->Bound());
}

TEST_F(PlusIntCstExprTest, SetMinToMax) {
  expr_->SetMin(16);
  EXPECT_EQ(16, expr_->Min());
  EXPECT_TRUE(expr_->Bound());
}

TEST_F(PlusIntCstExprTest, SetRange) {
  expr_->SetRange(14, 15);
  EXPECT_EQ(4, vars_[1]->Min());
  EXPECT_EQ(5, vars_[1]->Max());
}

TEST_F(PlusIntCstExprTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr_->WhenRange(d);
}

class PlusOpenIntCstExprTest : public BaseOpenIntExprTest1 {
 protected:
  PlusOpenIntCstExprTest() : BaseOpenIntExprTest1("PlusOpenIntCstExprTest") {}
  IntExpr* MakeExpr() override { return solver_->MakeSum(vars_[1], 10); }
};

TEST_F(PlusOpenIntCstExprTest, MinMaxGetters) {
  EXPECT_EQ(10 + kint64min, expr_->Min());
  EXPECT_EQ(kint64max, expr_->Max());
}

TEST_F(PlusOpenIntCstExprTest, SetMax) {
  expr_->SetMax(4);
  EXPECT_EQ(4, expr_->Max());
}

TEST_F(PlusOpenIntCstExprTest, SetMin) {
  expr_->SetMin(7);
  EXPECT_EQ(7, expr_->Min());
}

TEST_F(PlusOpenIntCstExprTest, SetRange) {
  expr_->SetRange(5, 6);
  EXPECT_EQ(-5, vars_[1]->Min());
  EXPECT_EQ(-4, vars_[1]->Max());
}

// ----- SubIntExpr -----

class SubIntExprTest : public BaseIntExprTest1 {
 protected:
  SubIntExprTest() : BaseIntExprTest1("SubIntExprTest") {}
  IntExpr* MakeExpr() override {
    return solver_->MakeDifference(vars_[0], vars_[1]);
  }
};

TEST_F(SubIntExprTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
}

TEST_F(SubIntExprTest, MinMaxGetters) {
  EXPECT_EQ(-4, expr_->Min());
  EXPECT_EQ(0, expr_->Max());
}

TEST_F(SubIntExprTest, SetMaxToMin) {
  expr_->SetMax(-4);
  EXPECT_EQ(-4, expr_->Max());
  EXPECT_TRUE(expr_->Bound());
}

TEST_F(SubIntExprTest, SetMinToMax) {
  expr_->SetMin(0);
  EXPECT_EQ(0, expr_->Min());
  EXPECT_TRUE(expr_->Bound());
}

TEST_F(SubIntExprTest, SetRangeNearMin) {
  expr_->SetRange(-3, -2);
  EXPECT_EQ(2, vars_[0]->Min());
  EXPECT_EQ(3, vars_[0]->Max());
  EXPECT_EQ(4, vars_[1]->Min());
  EXPECT_EQ(6, vars_[1]->Max());
}

TEST_F(SubIntExprTest, SetRangeNearMax) {
  expr_->SetRange(-2, -1);
  EXPECT_EQ(2, vars_[0]->Min());
  EXPECT_EQ(3, vars_[0]->Max());
  EXPECT_EQ(3, vars_[1]->Min());
  EXPECT_EQ(5, vars_[1]->Max());
}

TEST_F(SubIntExprTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr_->WhenRange(d);
}

class SubOpenIntExprTest : public BaseOpenIntExprTest1 {
 protected:
  SubOpenIntExprTest() : BaseOpenIntExprTest1("SubOpenIntExprTest") {}
  IntExpr* MakeExpr() override {
    return solver_->MakeDifference(vars_[0], vars_[1]);
  }
};

TEST_F(SubOpenIntExprTest, MinMaxGetters) {
  EXPECT_EQ(kint64min, expr_->Min());
  EXPECT_EQ(kint64max, expr_->Max());
}

TEST_F(SubOpenIntExprTest, SetMaxToMin) {
  expr_->SetMax(5);
  EXPECT_EQ(kint64max, expr_->Max());
  expr_->SetMax(kint64min);
  EXPECT_EQ(-1, expr_->Max());
}

TEST_F(SubOpenIntExprTest, SetMinToMax) {
  expr_->SetMin(9);
  EXPECT_EQ(kint64min, expr_->Min());
  expr_->SetMin(kint64max);
  EXPECT_EQ(-1, expr_->Min());
}

TEST_F(SubOpenIntExprTest, SetRangeNearMin) {
  expr_->SetRange(kint64min, kint64max);
  EXPECT_EQ(kint64min, vars_[0]->Min());
  EXPECT_EQ(kint64max, vars_[0]->Max());
  EXPECT_EQ(kint64min, vars_[1]->Min());
  EXPECT_EQ(kint64max, vars_[1]->Max());
  const int64_t new_min = 6;
  const int64_t new_max = 7;
  expr_->SetRange(new_min, new_max);
  EXPECT_EQ(kint64min + new_min, vars_[0]->Min());
  EXPECT_EQ(kint64max, vars_[0]->Max());
  EXPECT_EQ(kint64min, vars_[1]->Min());
  EXPECT_EQ(kint64max - new_min, vars_[1]->Max());
}

// Creating a difference expression between 2 variables with "open" upper or
// lower bounds bounds but which will create the "unsafe" difference
// implementation (which partially checks for under/overflows).
class UnsafeSubIntExprTest : public BaseIntExprTest {
 protected:
  UnsafeSubIntExprTest() : BaseIntExprTest("UnsafeSubIntExprTest"), count_(0) {}
  IntExpr* MakeExpr() override {
    count_ = 0;
    solver_->set_fail_intercept([this]() { ++count_; });
    return solver_->MakeDifference(vars_[0], vars_[1]);
  }

  int count_;
};

class UnsafeSubIntExprOpenUpperBoundTest : public UnsafeSubIntExprTest {
 protected:
  void MakeVars(std::vector<IntVar*>* vars) override {
    solver_->MakeIntVarArray(2, 0, kint64max, vars);
  }
};

// Calling SetRange on the expression with bounds which will overflow.
TEST_F(UnsafeSubIntExprOpenUpperBoundTest, SetRangeWithPotentialOverflow) {
  expr_->SetRange(kint64min, 10);
  EXPECT_EQ(0, count_);
}

// Calling SetMax on the expression with bounds which will overflow.
TEST_F(UnsafeSubIntExprOpenUpperBoundTest, SetMaxWithPotentialOverflow) {
  expr_->SetMin(kint64min);
  EXPECT_EQ(0, count_);
}

class UnsafeSubIntExprOpenLowerBoundTest : public UnsafeSubIntExprTest {
 protected:
  void MakeVars(std::vector<IntVar*>* vars) override {
    solver_->MakeIntVarArray(2, -kint64max /*> kint64min*/, 0, vars);
  }
};

// Calling SetRange on the expression with bounds which will underflow.
TEST_F(UnsafeSubIntExprOpenLowerBoundTest, SetRangeWithPotentialUnderflow) {
  expr_->SetRange(-10, kint64max);
  EXPECT_EQ(0, count_);
}

// Calling SetMax on the expression with bounds which will underflow.
TEST_F(UnsafeSubIntExprOpenLowerBoundTest, SetMinWithPotentialUnderflow) {
  expr_->SetMax(kint64max);
  EXPECT_EQ(0, count_);
}

class MakeEqAndIneqTestDecisionBuilder : public DecisionBuilder {
 public:
  Decision* Next(Solver* const solver) override {
    // Equality and different tests.
    std::vector<IntVar*> vars;
    for (int i = 0; i < 4; ++i) {
      vars.push_back(solver->MakeIntVar(0, 10, ""));
    }
    std::vector<IntVar*> result_vars;
    for (int i = 0; i < 4; i += 2) {
      result_vars.push_back(solver->MakeIsEqualVar(vars[i], vars[i + 1]));
      result_vars.push_back(solver->MakeIntVar(0, 1, "eq1"));
      solver->AddConstraint(
          solver->MakeIsEqualCt(vars[i], vars[i + 1], result_vars.back()));
      result_vars.push_back(solver->MakeIsDifferentVar(vars[i], vars[i + 1]));
      result_vars.push_back(solver->MakeIntVar(0, 1, "eq1"));
      solver->AddConstraint(
          solver->MakeIsDifferentCt(vars[i], vars[i + 1], result_vars.back()));
    }

    EXPECT_EQ(8, result_vars.size());
    for (int i = 0; i < result_vars.size(); ++i) {
      EXPECT_FALSE(result_vars[i]->Bound()) << i;
    }
    vars[0]->SetValue(3);
    vars[1]->SetValue(3);
    for (int i = 0; i < 4; ++i) {
      EXPECT_TRUE(result_vars[i]->Bound()) << i;
      EXPECT_EQ(i < 2 ? 1 : 0, result_vars[i]->Value()) << i;
    }

    vars[2]->SetValue(2);
    vars[3]->SetValue(3);
    for (int i = 4; i < 8; ++i) {
      EXPECT_TRUE(result_vars[i]->Bound()) << i;
      EXPECT_EQ(i < 6 ? 0 : 1, result_vars[i]->Value()) << i;
    }

    // Inequality tests.
    result_vars.clear();
    vars.clear();
    for (int i = 0; i < 4; ++i) {
      vars.push_back(solver->MakeIntVar(0, 10, ""));
    }

    for (int i = 0; i < 4; i += 2) {
      result_vars.push_back(
          solver->MakeIsGreaterOrEqualVar(vars[i], vars[i + 1]));
      result_vars.push_back(solver->MakeIntVar(0, 1, "eq1"));
      solver->AddConstraint(solver->MakeIsGreaterOrEqualCt(vars[i], vars[i + 1],
                                                           result_vars.back()));

      result_vars.push_back(solver->MakeIsLessOrEqualVar(vars[i], vars[i + 1]));
      result_vars.push_back(solver->MakeIntVar(0, 1, "eq1"));
      solver->AddConstraint(solver->MakeIsLessOrEqualCt(vars[i], vars[i + 1],
                                                        result_vars.back()));

      result_vars.push_back(solver->MakeIsGreaterVar(vars[i], vars[i + 1]));
      result_vars.push_back(solver->MakeIntVar(0, 1, "eq1"));
      solver->AddConstraint(
          solver->MakeIsGreaterCt(vars[i], vars[i + 1], result_vars.back()));

      result_vars.push_back(solver->MakeIsLessVar(vars[i], vars[i + 1]));
      result_vars.push_back(solver->MakeIntVar(0, 1, "eq1"));
      solver->AddConstraint(
          solver->MakeIsLessCt(vars[i], vars[i + 1], result_vars.back()));
    }
    EXPECT_EQ(16, result_vars.size());
    for (int i = 0; i < result_vars.size(); ++i) {
      EXPECT_FALSE(result_vars[i]->Bound()) << i;
    }

    vars[0]->SetValue(3);
    vars[1]->SetValue(3);
    for (int i = 0; i < result_vars.size() / 2; ++i) {
      EXPECT_TRUE(result_vars[i]->Bound()) << i;
      EXPECT_EQ(i < 4 ? 1 : 0, result_vars[i]->Value()) << i;
    }

    vars[2]->SetValue(2);
    vars[3]->SetValue(3);
    for (int i = result_vars.size() / 2; i < result_vars.size(); ++i) {
      EXPECT_TRUE(result_vars[i]->Bound()) << i;
      EXPECT_EQ(i % 4 < 2 ? 0 : 1, result_vars[i]->Value()) << i;
    }

    return nullptr;
  }
};

// ----- MakeIs{Equal,Different,GreaterOrEq,Greater,LessOrEq,Less}{Var,Ct} -----

TEST(IsEqualIsDifferentTest, AllTests) {
  Solver solver("solver");
  DecisionBuilder* db = solver.RevAlloc(new MakeEqAndIneqTestDecisionBuilder);
  ASSERT_TRUE(solver.Solve(db));
}

// Non-regression test for MakeIsEqualCstVar: check large domain doesn't
// lead to an unhandled overflow.

TEST_IN_PROPAGATION(MakeIsEqualCstVarTest, LargeDomain, solver, true) {
  IntVar* var = solver->MakeIntVar(kint64min, kint64max);
  IntVar* eq_var = solver->MakeIsEqualCstVar(var, 0);
  EXPECT_EQ(0, eq_var->Min());
  EXPECT_EQ(1, eq_var->Max());
  var->SetValue(0);
  EXPECT_EQ(1, eq_var->Value());
  return nullptr;
}

// Non-regression test for MakeIsDifferentCstVar: check large domain doesn't
// lead to an unhandled overflow.

TEST_IN_PROPAGATION(MakeIsDifferentCstVarTest, LargeDomain, solver, true) {
  IntVar* var = solver->MakeIntVar(kint64min, kint64max);
  IntVar* eq_var = solver->MakeIsDifferentCstVar(var, 0);
  EXPECT_EQ(0, eq_var->Min());
  EXPECT_EQ(1, eq_var->Max());
  var->SetValue(0);
  EXPECT_EQ(0, eq_var->Value());
  return nullptr;
}

// Non-regression test for MakeIsGreaterOrEqualCstVar: check large domain
// doesn't lead to an unhandled overflow.

TEST_IN_PROPAGATION(MakeIsGreaterOrEqualCstVarTest, LargeDomain, solver, true) {
  IntVar* var = solver->MakeIntVar(kint64min, kint64max);
  IntVar* eq_var = solver->MakeIsGreaterOrEqualCstVar(var, 0);
  EXPECT_EQ(0, eq_var->Min());
  EXPECT_EQ(1, eq_var->Max());
  var->SetValue(10);
  EXPECT_EQ(1, eq_var->Value());
  return nullptr;
}

// ----- SubIntCstExpr -----

class SubIntCstExprTest : public BaseIntExprTest1 {
 protected:
  SubIntCstExprTest() : BaseIntExprTest1("SubIntCstExprTest") {}
  IntExpr* MakeExpr() override { return solver_->MakeDifference(10, vars_[1]); }
};

TEST_F(SubIntCstExprTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
}

TEST_F(SubIntCstExprTest, MinMaxGetters) {
  EXPECT_EQ(4, expr_->Min());
  EXPECT_EQ(7, expr_->Max());
}

TEST_F(SubIntCstExprTest, SetMaxToMin) {
  expr_->SetMax(4);
  EXPECT_EQ(4, expr_->Max());
  EXPECT_TRUE(expr_->Bound());
}

TEST_F(SubIntCstExprTest, SetMinToMax) {
  expr_->SetMin(7);
  EXPECT_EQ(7, expr_->Min());
  EXPECT_TRUE(expr_->Bound());
}

TEST_F(SubIntCstExprTest, SetRange) {
  expr_->SetRange(5, 6);
  EXPECT_EQ(4, vars_[1]->Min());
  EXPECT_EQ(5, vars_[1]->Max());
}

TEST_F(SubIntCstExprTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr_->WhenRange(d);
}

class SubOpenIntCstExprTest : public BaseOpenIntExprTest1 {
 protected:
  SubOpenIntCstExprTest() : BaseOpenIntExprTest1("SubOpenIntCstExprTest") {}
  IntExpr* MakeExpr() override { return solver_->MakeDifference(10, vars_[1]); }
};

TEST_F(SubOpenIntCstExprTest, MinMaxGetters) {
  EXPECT_EQ(10 - kint64max, expr_->Min());
  EXPECT_EQ(kint64max, expr_->Max());
}

TEST_F(SubOpenIntCstExprTest, SetMax) {
  expr_->SetMax(4);
  EXPECT_EQ(4, expr_->Max());
}

TEST_F(SubOpenIntCstExprTest, SetMin) {
  expr_->SetMin(7);
  EXPECT_EQ(7, expr_->Min());
}

TEST_F(SubOpenIntCstExprTest, SetRange) {
  expr_->SetRange(5, 6);
  EXPECT_EQ(4, vars_[1]->Min());
  EXPECT_EQ(5, vars_[1]->Max());
}

// ----- SubIntCstVar -----

class SubIntCstVarTest : public BaseIntExprTest1 {
 protected:
  SubIntCstVarTest() : BaseIntExprTest1("SubIntCstVarTest") {}
  IntExpr* MakeExpr() override {
    return solver_->MakeDifference(10, vars_[1])->Var();
  }
};

TEST_F(SubIntCstVarTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
}

TEST_F(SubIntCstVarTest, MinMaxGetters) {
  EXPECT_EQ(4, expr_->Min());
  EXPECT_EQ(7, expr_->Max());
}

TEST_F(SubIntCstVarTest, SetMaxToMin) {
  expr_->SetMax(4);
  EXPECT_EQ(4, expr_->Max());
  EXPECT_TRUE(expr_->Bound());
}

TEST_F(SubIntCstVarTest, SetMinToMax) {
  expr_->SetMin(7);
  EXPECT_EQ(7, expr_->Min());
  EXPECT_TRUE(expr_->Bound());
}

TEST_F(SubIntCstVarTest, SetRange) {
  expr_->SetRange(5, 6);
  EXPECT_EQ(4, vars_[1]->Min());
  EXPECT_EQ(5, vars_[1]->Max());
}

TEST_F(SubIntCstVarTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr_->WhenRange(d);
}

// ----- OppIntExpr -----

class OppIntExprTest : public BaseIntExprTest1 {
 protected:
  OppIntExprTest() : BaseIntExprTest1("OppIntExprTest") {}
  IntExpr* MakeExpr() override { return solver_->MakeOpposite(vars_[1]); }
};

TEST_F(OppIntExprTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
}

TEST_F(OppIntExprTest, MinMaxGetters) {
  EXPECT_EQ(-6, expr_->Min());
  EXPECT_EQ(-3, expr_->Max());
}

TEST_F(OppIntExprTest, SetMaxToMin) {
  expr_->SetMax(-6);
  EXPECT_EQ(-6, expr_->Max());
  EXPECT_TRUE(expr_->Bound());
}

TEST_F(OppIntExprTest, SetMinToMax) {
  expr_->SetMin(-3);
  EXPECT_EQ(-3, expr_->Min());
  EXPECT_TRUE(expr_->Bound());
}

TEST_F(OppIntExprTest, SetRange) {
  expr_->SetRange(-5, -4);
  EXPECT_EQ(4, vars_[1]->Min());
  EXPECT_EQ(5, vars_[1]->Max());
}

TEST_F(OppIntExprTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr_->WhenRange(d);
}

TEST(OppIntExprOverflowTest, Int64MinOverflow) {
  Solver solver("OppOverflow");
  IntExpr* var = solver.MakeIntConst(kint64min);
  IntExpr* opp_var = solver.MakeOpposite(var);
  ASSERT_TRUE(opp_var->Bound());
  EXPECT_EQ(opp_var->Min(), kint64max);
  IntExpr* opp_opp_var = solver.MakeOpposite(opp_var);
  ASSERT_TRUE(opp_opp_var->Bound());
  EXPECT_EQ(opp_opp_var->Min(), -kint64max);
}

// ----- OppIntVar -----

class OppIntVarTest : public BaseIntExprTest1 {
 protected:
  OppIntVarTest() : BaseIntExprTest1("OppIntVarTest") {}
  IntExpr* MakeExpr() override {
    return solver_->MakeOpposite(vars_[1])->Var();
  }
};

TEST_F(OppIntVarTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
}

TEST_F(OppIntVarTest, MinMaxGetters) {
  EXPECT_EQ(-6, expr_->Min());
  EXPECT_EQ(-3, expr_->Max());
}

TEST_F(OppIntVarTest, SetMaxToMin) {
  expr_->SetMax(-6);
  EXPECT_EQ(-6, expr_->Max());
  EXPECT_TRUE(expr_->Bound());
}

TEST_F(OppIntVarTest, SetMinToMax) {
  expr_->SetMin(-3);
  EXPECT_EQ(-3, expr_->Min());
  EXPECT_TRUE(expr_->Bound());
}

TEST_F(OppIntVarTest, SetRange) {
  expr_->SetRange(-5, -4);
  EXPECT_EQ(4, vars_[1]->Min());
  EXPECT_EQ(5, vars_[1]->Max());
}

TEST_F(OppIntVarTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr_->WhenRange(d);
}

// The unit tests here are slow, because we iterate on a large domain of values
// (x_min, x_max, y_min, y_max). To speed it up, we parallelize the test by
// making (x_min, x_max) a parameter, and using sharding.
class TimesIntExprTest : public ::testing::TestWithParam<int64_t> {
 public:
  // The domain covered by x_min, x_max, y_min, y_max.
  static constexpr int64_t kRadius = 7;

 protected:
  void SetUp() override { x_min = GetParam(); }

  static bool HasSupportValues(int64_t value, int64_t other_min,
                               int64_t other_max, int64_t prod_min,
                               int64_t prod_max) {
    return (value * other_min >= prod_min && value * other_min <= prod_max) ||
           (value * other_max >= prod_min && value * other_max <= prod_max) ||
           (value * other_min <= prod_min && value * other_max >= prod_min) ||
           (value * other_max <= prod_min && value * other_min >= prod_min);
  }

  static void CheckProductValidity(int64_t old_x_min, int64_t old_x_max,
                                   int64_t old_y_min, int64_t old_y_max,
                                   int64_t old_prod_min, int64_t old_prod_max,
                                   int64_t range_min, int64_t range_max,
                                   int64_t x_min, int64_t x_max, int64_t y_min,
                                   int64_t y_max, int64_t prod_min,
                                   int64_t prod_max) {
    SCOPED_TRACE(testing::Message()
                 << "[" << old_x_min << " .. " << old_x_max << "] * ["
                 << old_y_min << " .. " << old_y_max << "] in [" << range_min
                 << " .. " << range_max << "] ===> [" << x_min << " .. "
                 << x_max << "] * [ " << y_min << " .. " << y_max << "] == ["
                 << prod_min << " .. " << prod_max << "]");
    for (int64_t value = old_x_min; value < x_min; ++value) {
      EXPECT_FALSE(
          HasSupportValues(value, old_y_min, old_y_max, range_min, range_max));
    }
    EXPECT_TRUE(
        HasSupportValues(x_min, old_y_min, old_y_max, range_min, range_max));
    EXPECT_TRUE(
        HasSupportValues(x_max, old_y_min, old_y_max, range_min, range_max));
    for (int64_t value = x_max + 1; value <= old_x_max; ++value) {
      EXPECT_FALSE(
          HasSupportValues(value, old_y_min, old_y_max, range_min, range_max));
    }
    for (int64_t value = old_y_min; value < y_min; ++value) {
      EXPECT_FALSE(
          HasSupportValues(value, old_x_min, old_x_max, range_min, range_max));
    }
    EXPECT_TRUE(
        HasSupportValues(y_min, old_x_min, old_x_max, range_min, range_max));
    EXPECT_TRUE(
        HasSupportValues(y_max, old_x_min, old_x_max, range_min, range_max));
    for (int64_t value = y_max + 1; value <= old_y_max; ++value) {
      EXPECT_FALSE(
          HasSupportValues(value, old_x_min, old_x_max, range_min, range_max));
    }
    EXPECT_EQ(prod_min, std::min(std::min(x_min * y_min, x_max * y_max),
                                 std::min(x_min * y_max, x_max * y_min)));
    EXPECT_EQ(prod_max, std::max(std::max(x_min * y_min, x_max * y_max),
                                 std::max(x_min * y_max, x_max * y_min)));
  }

  int64_t x_min;
};

TEST_P(TimesIntExprTest, SetMin) {
  absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
  for (int64_t x_max = x_min; x_max <= kRadius; ++x_max) {
    for (int64_t y_min = -kRadius; y_min <= kRadius; ++y_min) {
      for (int64_t y_max = y_min; y_max <= kRadius; ++y_max) {
        const int64_t prod_min =
            std::min(std::min(x_min * y_min, x_max * y_max),
                     std::min(x_min * y_max, x_max * y_min));
        const int64_t prod_max =
            std::max(std::max(x_min * y_min, x_max * y_max),
                     std::max(x_min * y_max, x_max * y_min));
        Solver s("TimesIntExprTest");
        IntVar* const x = s.MakeIntVar(x_min, x_max);
        IntVar* const y = s.MakeIntVar(y_min, y_max);
        IntExpr* const prod = s.MakeProd(x, y);

        EXPECT_EQ(prod_min, prod->Min());
        EXPECT_EQ(prod_max, prod->Max());
        for (int64_t value = prod_min; value <= prod_max; ++value) {
          prod->SetMin(value);
          CheckProductValidity(x_min, x_max, y_min, y_max, prod_min, prod_max,
                               value, prod_max, x->Min(), x->Max(), y->Min(),
                               y->Max(), prod->Min(), prod->Max());
        }
      }
    }
  }
}

TEST_P(TimesIntExprTest, SetMax) {
  absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
  for (int64_t x_max = x_min; x_max <= kRadius; ++x_max) {
    for (int64_t y_min = -kRadius; y_min <= kRadius; ++y_min) {
      for (int64_t y_max = y_min; y_max <= kRadius; ++y_max) {
        const int64_t prod_min =
            std::min(std::min(x_min * y_min, x_max * y_max),
                     std::min(x_min * y_max, x_max * y_min));
        const int64_t prod_max =
            std::max(std::max(x_min * y_min, x_max * y_max),
                     std::max(x_min * y_max, x_max * y_min));
        Solver s("TimesIntExprTest");
        IntVar* const x = s.MakeIntVar(x_min, x_max);
        IntVar* const y = s.MakeIntVar(y_min, y_max);
        IntExpr* const prod = s.MakeProd(x, y);

        EXPECT_EQ(prod_min, prod->Min());
        EXPECT_EQ(prod_max, prod->Max());
        for (int64_t value = prod_max; value >= prod_min; --value) {
          prod->SetMax(value);
          CheckProductValidity(x_min, x_max, y_min, y_max, prod_min, prod_max,
                               prod_min, value, x->Min(), x->Max(), y->Min(),
                               y->Max(), prod->Min(), prod->Max());
        }
      }
    }
  }
}

TEST_P(TimesIntExprTest, SetOffboundNegativeMin) {
  absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
  for (int64_t x_max = x_min; x_max <= kRadius; ++x_max) {
    for (int64_t y_min = -kRadius; y_min <= kRadius; ++y_min) {
      for (int64_t y_max = y_min; y_max <= kRadius; ++y_max) {
        const int64_t prod_min =
            std::min(std::min(x_min * y_min, x_max * y_max),
                     std::min(x_min * y_max, x_max * y_min));
        const int64_t prod_max =
            std::max(std::max(x_min * y_min, x_max * y_max),
                     std::max(x_min * y_max, x_max * y_min));
        Solver s("TimesIntExprTest");
        IntVar* const x = s.MakeIntVar(x_min, x_max);
        IntVar* const y = s.MakeIntVar(y_min, y_max);
        IntExpr* const prod = s.MakeProd(x, y);
        prod->SetMin(prod->Min() - 1);
        EXPECT_EQ(prod_min, prod->Min());
        EXPECT_EQ(prod_max, prod->Max());
        EXPECT_EQ(x_min, x->Min());
        EXPECT_EQ(x_max, x->Max());
        EXPECT_EQ(y_min, y->Min());
        EXPECT_EQ(y_max, y->Max());
      }
    }
  }
}

class SetMin : public DecisionBuilder {
 public:
  SetMin(IntExpr* const expr, int64_t new_min)
      : expr_(expr), new_min_(new_min) {}
  ~SetMin() override {}

  Decision* Next(Solver* const s) override {
    expr_->SetMin(new_min_);
    return nullptr;
  }

 private:
  IntExpr* const expr_;
  const int64_t new_min_;
};

TEST_P(TimesIntExprTest, SetOffboundPositiveMin) {
  absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
  for (int64_t x_max = x_min; x_max <= kRadius; ++x_max) {
    for (int64_t y_min = -kRadius; y_min <= kRadius; ++y_min) {
      for (int64_t y_max = y_min; y_max <= kRadius; ++y_max) {
        Solver s("TimesIntExprTest");
        IntVar* const x = s.MakeIntVar(x_min, x_max);
        IntVar* const y = s.MakeIntVar(y_min, y_max);
        IntExpr* const prod = s.MakeProd(x, y);
        DecisionBuilder* const set_min =
            s.RevAlloc(new SetMin(prod, prod->Max() + 1));
        EXPECT_FALSE(s.Solve(set_min));
      }
    }
  }
}

TEST_P(TimesIntExprTest, SetOffboundPositiveMax) {
  absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
  for (int64_t x_max = x_min; x_max <= kRadius; ++x_max) {
    for (int64_t y_min = -kRadius; y_min <= kRadius; ++y_min) {
      for (int64_t y_max = y_min; y_max <= kRadius; ++y_max) {
        const int64_t prod_min =
            std::min(std::min(x_min * y_min, x_max * y_max),
                     std::min(x_min * y_max, x_max * y_min));
        const int64_t prod_max =
            std::max(std::max(x_min * y_min, x_max * y_max),
                     std::max(x_min * y_max, x_max * y_min));
        Solver s("TimesIntExprTest");
        IntVar* const x = s.MakeIntVar(x_min, x_max);
        IntVar* const y = s.MakeIntVar(y_min, y_max);
        IntExpr* const prod = s.MakeProd(x, y);
        prod->SetMax(prod->Max() + 1);
        EXPECT_EQ(prod_min, prod->Min());
        EXPECT_EQ(prod_max, prod->Max());
        EXPECT_EQ(x_min, x->Min());
        EXPECT_EQ(x_max, x->Max());
        EXPECT_EQ(y_min, y->Min());
        EXPECT_EQ(y_max, y->Max());
      }
    }
  }
}

class SetMax : public DecisionBuilder {
 public:
  SetMax(IntExpr* const expr, int64_t new_max)
      : expr_(expr), new_max_(new_max) {}
  ~SetMax() override {}

  Decision* Next(Solver* const s) override {
    expr_->SetMax(new_max_);
    return nullptr;
  }

 private:
  IntExpr* const expr_;
  const int64_t new_max_;
};

TEST_P(TimesIntExprTest, SetOffboundNegativeMax) {
  absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
  for (int64_t x_max = x_min; x_max <= kRadius; ++x_max) {
    for (int64_t y_min = -kRadius; y_min <= kRadius; ++y_min) {
      for (int64_t y_max = y_min; y_max <= kRadius; ++y_max) {
        Solver s("TimesIntExprTest");
        IntVar* const x = s.MakeIntVar(x_min, x_max);
        IntVar* const y = s.MakeIntVar(y_min, y_max);
        IntExpr* const prod = s.MakeProd(x, y);
        DecisionBuilder* const set_max =
            s.RevAlloc(new SetMax(prod, prod->Min() - 1));
        EXPECT_FALSE(s.Solve(set_max));
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(Domain, TimesIntExprTest,
                         testing::Range(-TimesIntExprTest::kRadius,
                                        TimesIntExprTest::kRadius));

TEST(TimeIntExprOverflowTest, MinOverflow) {
  Solver solver("TimeIntExprOverflowTest");
  IntVar* var1 = solver.MakeIntVar(kint64min, 10);
  IntVar* var2 = solver.MakeIntVar(kint64min, 10);
  IntExpr* prod = solver.MakeProd(var1, var2);
  prod->SetMin(1000);
  EXPECT_EQ(kint64min, var1->Min());
  EXPECT_EQ(kint64min, var2->Min());
  EXPECT_EQ(-1, var1->Max());
  EXPECT_EQ(-1, var2->Max());
}

TEST(TimeIntExprOverflowTest, MaxOverflow) {
  Solver solver("TimeIntExprOverflowTest");
  IntVar* var1 = solver.MakeIntVar(-10, kint64max);
  IntVar* var2 = solver.MakeIntVar(-10, kint64max);
  IntExpr* prod = solver.MakeProd(var1, var2);
  prod->SetMin(-1000);
  EXPECT_EQ(-10, var1->Min());
  EXPECT_EQ(-10, var2->Min());
  EXPECT_EQ(kint64max, var1->Max());
  EXPECT_EQ(kint64max, var2->Max());
}

// ----- TimesIntPosExpr -----

class TimesIntPosExprTest : public BaseIntExprTest {
 protected:
  TimesIntPosExprTest() : BaseIntExprTest("TimesIntPosExprTest") {}
  IntExpr* MakeExpr() override { return solver_->MakeProd(vars_[0], vars_[1]); }
  void MakeVars(std::vector<IntVar*>* vars) override {
    for (int i = 0; i < 2; ++i) {
      vars->push_back(solver_->MakeIntVar(i, 3 * i + 3));
    }
  }
};

TEST_F(TimesIntPosExprTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
}

TEST_F(TimesIntPosExprTest, MinMaxGetters) {
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(18, expr_->Max());
}

TEST_F(TimesIntPosExprTest, SetMaxToMin) {
  expr_->SetMax(0);
  EXPECT_EQ(0, expr_->Max());
  EXPECT_TRUE(expr_->Bound());
}

TEST_F(TimesIntPosExprTest, SetMinToMax) {
  expr_->SetMin(18);
  EXPECT_EQ(18, expr_->Min());
  EXPECT_TRUE(expr_->Bound());
}

TEST_F(TimesIntPosExprTest, SetMaxToZero) {
  expr_->SetMax(0);
  EXPECT_EQ(0, vars_[0]->Min());
  EXPECT_EQ(0, vars_[0]->Max());
  EXPECT_EQ(1, vars_[1]->Min());
  EXPECT_EQ(6, vars_[1]->Max());
}

TEST_F(TimesIntPosExprTest, SetMaxToNonZero) {
  expr_->SetMax(1);
  EXPECT_EQ(0, vars_[0]->Min());
  EXPECT_EQ(1, vars_[0]->Max());
  EXPECT_EQ(1, vars_[1]->Min());
  EXPECT_EQ(6, vars_[1]->Max());
}

TEST_F(TimesIntPosExprTest, SetMinToZero) {
  expr_->SetMin(0);
  EXPECT_EQ(0, vars_[0]->Min());
  EXPECT_EQ(3, vars_[0]->Max());
  EXPECT_EQ(1, vars_[1]->Min());
  EXPECT_EQ(6, vars_[1]->Max());
}

TEST_F(TimesIntPosExprTest, SetMinToNonZero) {
  expr_->SetMin(2);
  EXPECT_EQ(1, vars_[0]->Min());
  EXPECT_EQ(3, vars_[0]->Max());
  EXPECT_EQ(1, vars_[1]->Min());
  EXPECT_EQ(6, vars_[1]->Max());
}

TEST_F(TimesIntPosExprTest, SetRangeNearMin) {
  expr_->SetRange(1, 2);
  EXPECT_EQ(1, vars_[0]->Min());
  EXPECT_EQ(2, vars_[0]->Max());
  EXPECT_EQ(1, vars_[1]->Min());
  EXPECT_EQ(2, vars_[1]->Max());
}

TEST_F(TimesIntPosExprTest, SetRangeNearMax) {
  expr_->SetRange(16, 18);
  EXPECT_EQ(3, vars_[0]->Min());
  EXPECT_EQ(3, vars_[0]->Max());
  EXPECT_EQ(6, vars_[1]->Min());
  EXPECT_EQ(6, vars_[1]->Max());
  EXPECT_EQ(18, expr_->Min());
}

TEST_F(TimesIntPosExprTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr_->WhenRange(d);
}

TEST_F(TimesIntPosExprTest, Overflow) {
  IntVar* const left =
      solver_->MakeIntVar(0, int64_t{2} * kint32max + int64_t{10});
  IntVar* const right =
      solver_->MakeIntVar(0, int64_t{2} * kint32max + int64_t{15});
  IntExpr* const prod = solver_->MakeProd(left, right);
  EXPECT_EQ(0, prod->Min());
  EXPECT_EQ(kint64max, prod->Max());
  prod->SetMax(kint64max);
  EXPECT_EQ(int64_t{2} * kint32max + int64_t{10}, left->Max());
  EXPECT_EQ(int64_t{2} * kint32max + int64_t{15}, right->Max());
  prod->SetMin(1);
  EXPECT_EQ(1, left->Min());
  EXPECT_EQ(1, right->Min());
  EXPECT_EQ(int64_t{2} * kint32max + int64_t{10}, left->Max());
  EXPECT_EQ(int64_t{2} * kint32max + int64_t{15}, right->Max());
  prod->SetMax(kint64max);
  EXPECT_EQ(int64_t{2} * kint32max + int64_t{10}, left->Max());
  EXPECT_EQ(int64_t{2} * kint32max + int64_t{15}, right->Max());
  prod->SetMax(kint32max);
  EXPECT_EQ(kint32max * int64_t{1}, left->Max());
  EXPECT_EQ(kint32max * int64_t{1}, right->Max());
}

TEST_IN_PROPAGATION1(TimesIntPosExprTest, SetMinFail, solver_, false, IntExpr*,
                     expr_) {
  expr_->SetMin(20);
  return nullptr;
}

TEST_IN_PROPAGATION1(TimesIntPosExprTest, SetMaxFail, solver_, false, IntExpr*,
                     expr_) {
  expr_->SetMax(-2);
  return nullptr;
}

TEST_IN_PROPAGATION2(TimesIntPosExprTest, SetGrowingMin, solver_, true,
                     const std::vector<IntVar*>&, vars_, IntExpr*, expr_) {
  expr_->SetMin(4);
  EXPECT_EQ(1, vars_[0]->Min());
  EXPECT_EQ(3, vars_[0]->Max());
  EXPECT_EQ(2, vars_[1]->Min());
  EXPECT_EQ(6, vars_[1]->Max());
  expr_->SetMin(10);
  EXPECT_EQ(2, vars_[0]->Min());
  EXPECT_EQ(3, vars_[0]->Max());
  EXPECT_EQ(4, vars_[1]->Min());
  EXPECT_EQ(6, vars_[1]->Max());

  return nullptr;
}

// ----- TimesIntCstExpr -----

TEST(TimesIntCstExprTest, PositiveBoundVariable) {
  Solver solver("TimesIntCstExprTest");
  IntVar* var = solver.MakeIntVar(2, 2);
  IntExpr* prod = solver.MakeProd(var, 10);
  EXPECT_TRUE(prod->Bound());
  EXPECT_EQ(20, prod->Min());
}

TEST(TimesIntCstExprTest, NegativeBoundVariable) {
  Solver solver("TimesIntCstExprTest");
  IntVar* var = solver.MakeIntVar(-2, -2);
  IntExpr* prod = solver.MakeProd(var, 10);
  EXPECT_TRUE(prod->Bound());
  EXPECT_EQ(-20, prod->Min());
}

TEST(TimesIntCstExprTest, PositiveBoundVariableWithOverflow) {
  Solver solver("TimesIntCstExprTest");
  IntVar* var = solver.MakeIntVar(2, 2);
  IntExpr* prod = solver.MakeProd(var, kint64max);
  EXPECT_TRUE(prod->Bound());
  EXPECT_EQ(kint64max, prod->Min());
}

TEST(TimesIntCstExprTest, NegativeBoundVariableWithOverflow) {
  Solver solver("TimesIntCstExprTest");
  IntVar* var = solver.MakeIntVar(-2, -2);
  IntExpr* prod = solver.MakeProd(var, kint64max);
  EXPECT_TRUE(prod->Bound());
  EXPECT_EQ(kint64min, prod->Min());
}

// ----- TimesIntPosCstExpr -----

class TimesIntPosCstExprTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("TimesIntPosCstExprTest");
    vars_.resize(2);
    vars_[0] = solver_->MakeIntVar(2, 4);
    vars_[1] = solver_->MakeIntVar(-4, -2);
    expr0_ = solver_->MakeProd(vars_[0], 10);
    expr1_ = solver_->MakeProd(vars_[1], 10);
  }

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  IntExpr* expr0_;
  IntExpr* expr1_;
};

TEST_F(TimesIntPosCstExprTest, DebugString) {
  EXPECT_FALSE(expr0_->DebugString().empty());
  EXPECT_FALSE(expr1_->DebugString().empty());
}

TEST_F(TimesIntPosCstExprTest, MinMaxGetters) {
  EXPECT_EQ(20, expr0_->Min());
  EXPECT_EQ(40, expr0_->Max());
  EXPECT_EQ(-40, expr1_->Min());
  EXPECT_EQ(-20, expr1_->Max());
}

TEST_F(TimesIntPosCstExprTest, SetMaxRounding1) {
  expr0_->SetMax(29);
  EXPECT_EQ(20, expr0_->Max());
  expr1_->SetMax(-29);
  EXPECT_EQ(-30, expr1_->Max());
}

TEST_F(TimesIntPosCstExprTest, SetMaxRounding2) {
  expr0_->SetMax(24);
  EXPECT_EQ(20, expr0_->Max());
  expr1_->SetMax(-24);
  EXPECT_EQ(-30, expr1_->Max());
}

TEST_F(TimesIntPosCstExprTest, SetMaxRounding3) {
  expr0_->SetMax(21);
  EXPECT_EQ(20, expr0_->Max());
  expr1_->SetMax(-21);
  EXPECT_EQ(-30, expr1_->Max());
}

TEST_F(TimesIntPosCstExprTest, SetMaxRounding4) {
  expr0_->SetMax(20);
  EXPECT_EQ(20, expr0_->Max());
  expr1_->SetMax(-20);
  EXPECT_EQ(-20, expr1_->Max());
}

TEST_F(TimesIntPosCstExprTest, SetMinRounding1) {
  expr0_->SetMin(29);
  EXPECT_EQ(30, expr0_->Min());
  expr1_->SetMin(-29);
  EXPECT_EQ(-20, expr1_->Min());
}

TEST_F(TimesIntPosCstExprTest, SetMinRounding2) {
  expr0_->SetMin(24);
  EXPECT_EQ(30, expr0_->Min());
  expr1_->SetMin(-24);
  EXPECT_EQ(-20, expr1_->Min());
}

TEST_F(TimesIntPosCstExprTest, SetMinRounding3) {
  expr0_->SetMin(21);
  EXPECT_EQ(30, expr0_->Min());
  expr1_->SetMin(-21);
  EXPECT_EQ(-20, expr1_->Min());
}

TEST_F(TimesIntPosCstExprTest, SetMinRounding4) {
  expr0_->SetMin(20);
  EXPECT_EQ(20, expr0_->Min());
  expr1_->SetMin(-20);
  EXPECT_EQ(-20, expr1_->Min());
}

TEST_F(TimesIntPosCstExprTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr0_->WhenRange(d);
  expr1_->WhenRange(d);
}

TEST_F(TimesIntPosCstExprTest, Overflow) {
  IntVar* const var = solver_->MakeIntVar(kint64min / 2, kint64max / 2);
  IntExpr* const prod = solver_->MakeProd(var, 5);
  EXPECT_EQ(kint64min, prod->Min());
  EXPECT_EQ(kint64max, prod->Max());
  prod->SetRange(kint64min, kint64max);
  EXPECT_EQ(kint64min / 2, var->Min());
  EXPECT_EQ(kint64max / 2, var->Max());
  prod->SetRange(kint32min * int64_t{5}, kint32max * int64_t{5});
  EXPECT_EQ(kint32min, var->Min());
  EXPECT_EQ(kint32max, var->Max());
}

// ----- TimesIntNegCstExpr -----

class TimesIntNegCstExprTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("TimesIntNegCstExprTest");
    vars_.resize(2);
    vars_[0] = solver_->MakeIntVar(-4, -2);
    vars_[1] = solver_->MakeIntVar(2, 4);
    expr0_ = solver_->MakeProd(vars_[0], -10);
    expr1_ = solver_->MakeProd(vars_[1], -10);
  }

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  IntExpr* expr0_;
  IntExpr* expr1_;
};

TEST_F(TimesIntNegCstExprTest, DebugString) {
  EXPECT_FALSE(expr0_->DebugString().empty());
  EXPECT_FALSE(expr1_->DebugString().empty());
}

TEST_F(TimesIntNegCstExprTest, MinMaxGetters) {
  EXPECT_EQ(20, expr0_->Min());
  EXPECT_EQ(40, expr0_->Max());
  EXPECT_EQ(-40, expr1_->Min());
  EXPECT_EQ(-20, expr1_->Max());
}

TEST_F(TimesIntNegCstExprTest, SetMaxRounding1) {
  expr0_->SetMax(29);
  EXPECT_EQ(20, expr0_->Max());
  expr1_->SetMax(-29);
  EXPECT_EQ(-30, expr1_->Max());
}

TEST_F(TimesIntNegCstExprTest, SetMaxRounding2) {
  expr0_->SetMax(24);
  EXPECT_EQ(20, expr0_->Max());
  expr1_->SetMax(-24);
  EXPECT_EQ(-30, expr1_->Max());
}

TEST_F(TimesIntNegCstExprTest, SetMaxRounding3) {
  expr0_->SetMax(21);
  EXPECT_EQ(20, expr0_->Max());
  expr1_->SetMax(-21);
  EXPECT_EQ(-30, expr1_->Max());
}

TEST_F(TimesIntNegCstExprTest, SetMaxRounding4) {
  expr0_->SetMax(20);
  EXPECT_EQ(20, expr0_->Max());
  expr1_->SetMax(-20);
  EXPECT_EQ(-20, expr1_->Max());
}

TEST_F(TimesIntNegCstExprTest, SetMinRounding1) {
  expr0_->SetMin(29);
  EXPECT_EQ(30, expr0_->Min());
  expr1_->SetMin(-29);
  EXPECT_EQ(-20, expr1_->Min());
}

TEST_F(TimesIntNegCstExprTest, SetMinRounding2) {
  expr0_->SetMin(24);
  EXPECT_EQ(30, expr0_->Min());
  expr1_->SetMin(-24);
  EXPECT_EQ(-20, expr1_->Min());
}

TEST_F(TimesIntNegCstExprTest, SetMinRounding3) {
  expr0_->SetMin(21);
  EXPECT_EQ(30, expr0_->Min());
  expr1_->SetMin(-21);
  EXPECT_EQ(-20, expr1_->Min());
}

TEST_F(TimesIntNegCstExprTest, SetMinRounding4) {
  expr0_->SetMin(20);
  EXPECT_EQ(20, expr0_->Min());
  expr1_->SetMin(-20);
  EXPECT_EQ(-20, expr1_->Min());
}

TEST_F(TimesIntNegCstExprTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr0_->WhenRange(d);
  expr1_->WhenRange(d);
}

TEST_IN_PROPAGATION0(TimesIntNegCstExprTest, IsLessOrEqualCst1, solver, true) {
  IntVar* const b1 = solver->MakeBoolVar("b1");
  IntVar* const t1 = solver->MakeProd(b1, -4)->Var();
  IntVar* const is = solver->MakeIsLessOrEqualCstVar(t1, -3);
  is->SetValue(1);
  EXPECT_EQ(1, b1->Min());
  return nullptr;
}

TEST_IN_PROPAGATION0(TimesIntNegCstExprTest, IsLessOrEqualCst0, solver, true) {
  IntVar* const b1 = solver->MakeBoolVar("b1");
  IntVar* const t1 = solver->MakeProd(b1, -4)->Var();
  IntVar* const is = solver->MakeIsLessOrEqualCstVar(t1, -3);
  is->SetValue(0);
  EXPECT_EQ(0, b1->Max());
  return nullptr;
}

TEST_IN_PROPAGATION0(TimesIntNegCstExprTest, OverflowProtection, solver, true) {
  const int64_t kBaseValues[] = {
      1, kint32max, static_cast<int64_t>(std::sqrt(kint64max)), kint64max};
  absl::linked_hash_set<int64_t> values;  // to preserve iteration order.
  for (int64_t abs_value : kBaseValues) {
    for (int64_t value : {abs_value, -abs_value}) {
      for (int64_t delta : {0, 1, -1}) {
        values.insert(CapAdd(value, delta));
      }
    }
  }
  for (int64_t ct_value : values) {
    for (int64_t var_min : values) {
      for (int64_t var_max : values) {
        if (var_max < var_min) continue;
        SCOPED_TRACE(absl::StrFormat("ct_value: %d, var_min: %d, var_max: %d",
                                     ct_value, var_min, var_max));
        IntVar* const i1 = solver->MakeIntVar(var_min, var_max);
        IntVar* const t1 = solver->MakeProd(i1, ct_value)->Var();
        int64_t expected_min = CapProd(var_min, ct_value);
        int64_t expected_max = CapProd(var_max, ct_value);
        // If ct_value is negative, the bounds are swapped.
        if (ct_value < 0) std::swap(expected_min, expected_max);
        // Saturated arithmetic isn't done perfectly in the CP Solver. For
        // example, -(-kint64min) != kint64min. Such an off-by-one case does
        // happen here, we give it a pass.
        if (!(expected_min == kint64max && t1->Min() == kint64max - 1)) {
          EXPECT_EQ(t1->Min(), expected_min);
        }
        EXPECT_EQ(t1->Max(), expected_max);
      }
    }
  }
  return nullptr;
}

TEST_IN_PROPAGATION0(TimesIntNegCstExprTest, IsGreaterOrEqualCst0, solver,
                     true) {
  IntVar* const b1 = solver->MakeBoolVar("b1");
  IntVar* const t1 = solver->MakeProd(b1, -4)->Var();
  IntVar* const is = solver->MakeIsGreaterOrEqualCstVar(t1, -3);
  is->SetValue(0);
  EXPECT_EQ(1, b1->Min());
  return nullptr;
}

TEST_IN_PROPAGATION0(TimesIntNegCstExprTest, IsGreaterOrEqualCst1, solver,
                     true) {
  IntVar* const b1 = solver->MakeBoolVar("b1");
  IntVar* const t1 = solver->MakeProd(b1, -4)->Var();
  IntVar* const is = solver->MakeIsGreaterOrEqualCstVar(t1, -3);
  is->SetValue(1);
  EXPECT_EQ(0, b1->Max());
  return nullptr;
}

TEST_F(TimesIntNegCstExprTest, Overflow) {
  IntVar* const var = solver_->MakeIntVar(kint64min / 2, kint64max / 2);
  IntExpr* const prod = solver_->MakeProd(var, -5);
  EXPECT_EQ(kint64min, prod->Min());
  EXPECT_EQ(kint64max, prod->Max());
  prod->SetRange(kint64min, kint64max);
  EXPECT_EQ(kint64min / 2, var->Min());
  EXPECT_EQ(kint64max / 2, var->Max());
  prod->SetRange(kint32min * int64_t{5}, kint32max * int64_t{5});
  EXPECT_EQ(-kint32max, var->Min());
  EXPECT_EQ(kint32min * -1LL, var->Max());
}

class TimesBooleanNegCstExprTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("TimesIntNegCstExprTest");
    var_ = solver_->MakeBoolVar();
    expr_ = solver_->MakeProd(var_, -10);
  }

  std::unique_ptr<Solver> solver_;
  IntVar* var_;
  IntExpr* expr_;
};

TEST_F(TimesBooleanNegCstExprTest, MinMaxGetters) {
  EXPECT_EQ(-10, expr_->Min());
  EXPECT_EQ(0, expr_->Max());
}

TEST_F(TimesBooleanNegCstExprTest, SetMin) {
  expr_->SetMin(-15);
  EXPECT_EQ(0, var_->Min());
  EXPECT_EQ(1, var_->Max());
  expr_->SetMin(-1);
  EXPECT_EQ(0, var_->Max());
}

TEST_F(TimesBooleanNegCstExprTest, SetMax) {
  expr_->SetMax(5);
  EXPECT_EQ(0, var_->Min());
  EXPECT_EQ(1, var_->Max());
  expr_->SetMax(-9);
  EXPECT_EQ(1, var_->Min());
}

TEST_F(TimesBooleanNegCstExprTest, SetRangeMin) {
  expr_->SetRange(-15, 1);
  EXPECT_EQ(0, var_->Min());
  EXPECT_EQ(1, var_->Max());
  expr_->SetRange(-1, 1);
  EXPECT_EQ(0, var_->Max());
}

TEST_F(TimesBooleanNegCstExprTest, SetRangeMax) {
  expr_->SetRange(-11, 5);
  EXPECT_EQ(0, var_->Min());
  EXPECT_EQ(1, var_->Max());
  expr_->SetRange(-11, -9);
  EXPECT_EQ(1, var_->Min());
}

class TimesBooleanNegCstVarTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("TimesIntNegCstVarTest");
    var_ = solver_->MakeBoolVar();
    expr_ = solver_->MakeProd(var_, -10)->Var();
  }

  std::unique_ptr<Solver> solver_;
  IntVar* var_;
  IntExpr* expr_;
};

TEST_F(TimesBooleanNegCstVarTest, MinMaxGetters) {
  EXPECT_EQ(-10, expr_->Min());
  EXPECT_EQ(0, expr_->Max());
}

TEST_F(TimesBooleanNegCstVarTest, SetMin) {
  expr_->SetMin(-15);
  EXPECT_EQ(0, var_->Min());
  EXPECT_EQ(1, var_->Max());
  expr_->SetMin(-1);
  EXPECT_EQ(0, var_->Max());
}

TEST_F(TimesBooleanNegCstVarTest, SetMax) {
  expr_->SetMax(5);
  EXPECT_EQ(0, var_->Min());
  EXPECT_EQ(1, var_->Max());
  expr_->SetMax(-9);
  EXPECT_EQ(1, var_->Min());
}

TEST_F(TimesBooleanNegCstVarTest, SetRangeMin) {
  expr_->SetRange(-15, 1);
  EXPECT_EQ(0, var_->Min());
  EXPECT_EQ(1, var_->Max());
  expr_->SetRange(-1, 1);
  EXPECT_EQ(0, var_->Max());
}

TEST_F(TimesBooleanNegCstVarTest, SetRangeMax) {
  expr_->SetRange(-11, 5);
  EXPECT_EQ(0, var_->Min());
  EXPECT_EQ(1, var_->Max());
  expr_->SetRange(-11, -9);
  EXPECT_EQ(1, var_->Min());
}

// ----- TimesBooleanPosIntExpr -----

class TimesBooleanPosIntExprTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("TimesBooleanPosIntExprTest");
    vars_[0] = solver_->MakeIntVar(1, 12);
    vars_[1] = solver_->MakeIntVar(0, 10);
    boolvars_[0] = solver_->MakeBoolVar();
    boolvars_[1] = solver_->MakeBoolVar();
    exprs_[0] = solver_->MakeProd(vars_[0], boolvars_[0]);
    exprs_[1] = solver_->MakeProd(boolvars_[1], vars_[1]);
  }

  std::unique_ptr<Solver> solver_;
  IntVar* vars_[2];
  IntVar* boolvars_[2];
  IntExpr* exprs_[2];
};

TEST_F(TimesBooleanPosIntExprTest, DebugString) {
  EXPECT_FALSE(exprs_[0]->DebugString().empty());
  EXPECT_FALSE(exprs_[1]->DebugString().empty());
}

TEST_F(TimesBooleanPosIntExprTest, MinMax) {
  EXPECT_EQ(0, exprs_[0]->Min());
  EXPECT_EQ(0, exprs_[1]->Min());
  EXPECT_EQ(12, exprs_[0]->Max());
  EXPECT_EQ(10, exprs_[1]->Max());
}

TEST_F(TimesBooleanPosIntExprTest, SetVarMax) {
  vars_[0]->SetMax(6);
  vars_[1]->SetMax(3);
  EXPECT_EQ(0, exprs_[0]->Min());
  EXPECT_EQ(0, exprs_[1]->Min());
  EXPECT_EQ(6, exprs_[0]->Max());
  EXPECT_EQ(3, exprs_[1]->Max());
}

TEST_F(TimesBooleanPosIntExprTest, SetVarMin) {
  vars_[0]->SetMin(6);
  vars_[1]->SetMin(3);
  EXPECT_EQ(0, exprs_[0]->Min());
  EXPECT_EQ(0, exprs_[1]->Min());
  EXPECT_EQ(12, exprs_[0]->Max());
  EXPECT_EQ(10, exprs_[1]->Max());
}

TEST_F(TimesBooleanPosIntExprTest, SetBoolVarFalse) {
  boolvars_[0]->SetValue(0);
  boolvars_[1]->SetValue(0);
  EXPECT_EQ(0, exprs_[0]->Min());
  EXPECT_EQ(0, exprs_[1]->Min());
  EXPECT_EQ(0, exprs_[0]->Max());
  EXPECT_EQ(0, exprs_[1]->Max());
  vars_[0]->SetMax(8);
  vars_[1]->SetMax(7);
  EXPECT_EQ(0, exprs_[0]->Min());
  EXPECT_EQ(0, exprs_[1]->Min());
  EXPECT_EQ(0, exprs_[0]->Max());
  EXPECT_EQ(0, exprs_[1]->Max());
  vars_[0]->SetMin(6);
  vars_[1]->SetMin(3);
  EXPECT_EQ(0, exprs_[0]->Min());
  EXPECT_EQ(0, exprs_[1]->Min());
  EXPECT_EQ(0, exprs_[0]->Max());
  EXPECT_EQ(0, exprs_[1]->Max());
}

TEST_F(TimesBooleanPosIntExprTest, SetBoolVarTrue) {
  boolvars_[0]->SetValue(1);
  boolvars_[1]->SetValue(1);
  EXPECT_EQ(1, exprs_[0]->Min());
  EXPECT_EQ(0, exprs_[1]->Min());
  EXPECT_EQ(12, exprs_[0]->Max());
  EXPECT_EQ(10, exprs_[1]->Max());
  vars_[0]->SetMax(8);
  vars_[1]->SetMax(7);
  EXPECT_EQ(1, exprs_[0]->Min());
  EXPECT_EQ(0, exprs_[1]->Min());
  EXPECT_EQ(8, exprs_[0]->Max());
  EXPECT_EQ(7, exprs_[1]->Max());
  vars_[0]->SetMin(6);
  vars_[1]->SetMin(3);
  EXPECT_EQ(6, exprs_[0]->Min());
  EXPECT_EQ(3, exprs_[1]->Min());
  EXPECT_EQ(8, exprs_[0]->Max());
  EXPECT_EQ(7, exprs_[1]->Max());
}

TEST_F(TimesBooleanPosIntExprTest, SetProductMin) {
  exprs_[0]->SetMin(-3);
  exprs_[1]->SetMin(-2);
  EXPECT_EQ(0, exprs_[0]->Min());
  EXPECT_EQ(0, exprs_[1]->Min());
  EXPECT_EQ(12, exprs_[0]->Max());
  EXPECT_EQ(10, exprs_[1]->Max());
  exprs_[0]->SetMin(0);
  exprs_[1]->SetMin(0);
  EXPECT_EQ(0, exprs_[0]->Min());  // boolvar = 0 is still an option.
  EXPECT_EQ(0, exprs_[1]->Min());
  EXPECT_EQ(12, exprs_[0]->Max());
  EXPECT_EQ(10, exprs_[1]->Max());
  exprs_[0]->SetMin(2);  // Force boolvar to true.
  exprs_[1]->SetMin(3);
  EXPECT_EQ(2, exprs_[0]->Min());
  EXPECT_EQ(3, exprs_[1]->Min());
  EXPECT_EQ(12, exprs_[0]->Max());
  EXPECT_EQ(10, exprs_[1]->Max());
  EXPECT_EQ(1, boolvars_[0]->Min());
  EXPECT_EQ(1, boolvars_[1]->Min());
}

TEST_F(TimesBooleanPosIntExprTest, SetProductMax) {
  exprs_[0]->SetMax(15);
  exprs_[1]->SetMax(16);
  EXPECT_EQ(0, exprs_[0]->Min());
  EXPECT_EQ(0, exprs_[1]->Min());
  EXPECT_EQ(12, exprs_[0]->Max());
  EXPECT_EQ(10, exprs_[1]->Max());
  exprs_[0]->SetMax(9);  //  boolvar = 0 is still an option
  exprs_[1]->SetMax(8);
  EXPECT_EQ(0, exprs_[0]->Min());
  EXPECT_EQ(0, exprs_[1]->Min());
  EXPECT_EQ(12, exprs_[0]->Max());
  EXPECT_EQ(10, exprs_[1]->Max());
  exprs_[0]->SetMin(2);  // force boolvar to true. But expressions are
  exprs_[1]->SetMin(3);  // non persistent, we have lost previous max.
  EXPECT_EQ(2, exprs_[0]->Min());
  EXPECT_EQ(3, exprs_[1]->Min());
  EXPECT_EQ(12, exprs_[0]->Max());
  EXPECT_EQ(10, exprs_[1]->Max());
  EXPECT_EQ(1, boolvars_[0]->Min());
  EXPECT_EQ(1, boolvars_[1]->Min());
  exprs_[0]->SetMax(9);  // set max again, propagation will occur because
  exprs_[1]->SetMax(8);  // boolvar is now true.
  EXPECT_EQ(9, exprs_[0]->Max());
  EXPECT_EQ(8, exprs_[1]->Max());
}

// ----- TimesBooleanIntExpr -----

class TimesBooleanIntExprTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("TimesBooleanIntExprTest");
    vars_[0] = solver_->MakeIntVar(-5, 12);
    vars_[1] = solver_->MakeIntVar(-3, 10);
    boolvars_[0] = solver_->MakeBoolVar();
    boolvars_[1] = solver_->MakeBoolVar();
    exprs_[0] = solver_->MakeProd(vars_[0], boolvars_[0]);
    exprs_[1] = solver_->MakeProd(boolvars_[1], vars_[1]);
  }

  std::unique_ptr<Solver> solver_;
  IntVar* vars_[2];
  IntVar* boolvars_[2];
  IntExpr* exprs_[2];
};

TEST_F(TimesBooleanIntExprTest, DebugString) {
  EXPECT_FALSE(exprs_[0]->DebugString().empty());
  EXPECT_FALSE(exprs_[1]->DebugString().empty());
}

TEST_F(TimesBooleanIntExprTest, MinMax) {
  EXPECT_EQ(-5, exprs_[0]->Min());
  EXPECT_EQ(-3, exprs_[1]->Min());
  EXPECT_EQ(12, exprs_[0]->Max());
  EXPECT_EQ(10, exprs_[1]->Max());
}

TEST_F(TimesBooleanIntExprTest, SetVarMax) {
  vars_[0]->SetMax(6);
  vars_[1]->SetMax(3);
  EXPECT_EQ(-5, exprs_[0]->Min());
  EXPECT_EQ(-3, exprs_[1]->Min());
  EXPECT_EQ(6, exprs_[0]->Max());
  EXPECT_EQ(3, exprs_[1]->Max());
  vars_[0]->SetMax(-2);
  vars_[1]->SetMax(-1);
  EXPECT_EQ(-5, exprs_[0]->Min());
  EXPECT_EQ(-3, exprs_[1]->Min());
  EXPECT_EQ(0, exprs_[0]->Max());
  EXPECT_EQ(0, exprs_[1]->Max());
}

TEST_F(TimesBooleanIntExprTest, SetVarMin) {
  vars_[0]->SetMin(-1);
  vars_[1]->SetMin(-2);
  EXPECT_EQ(-1, exprs_[0]->Min());
  EXPECT_EQ(-2, exprs_[1]->Min());
  EXPECT_EQ(12, exprs_[0]->Max());
  EXPECT_EQ(10, exprs_[1]->Max());
  vars_[0]->SetMin(1);
  vars_[1]->SetMin(2);
  EXPECT_EQ(0, exprs_[0]->Min());
  EXPECT_EQ(0, exprs_[1]->Min());
  EXPECT_EQ(12, exprs_[0]->Max());
  EXPECT_EQ(10, exprs_[1]->Max());
}

TEST_F(TimesBooleanIntExprTest, SetBoolVarFalse) {
  boolvars_[0]->SetValue(0);
  boolvars_[1]->SetValue(0);
  EXPECT_EQ(0, exprs_[0]->Min());
  EXPECT_EQ(0, exprs_[1]->Min());
  EXPECT_EQ(0, exprs_[0]->Max());
  EXPECT_EQ(0, exprs_[1]->Max());
  vars_[0]->SetMax(8);
  vars_[1]->SetMax(7);
  EXPECT_EQ(0, exprs_[0]->Min());
  EXPECT_EQ(0, exprs_[1]->Min());
  EXPECT_EQ(0, exprs_[0]->Max());
  EXPECT_EQ(0, exprs_[1]->Max());
  vars_[0]->SetMin(-1);
  vars_[1]->SetMin(-2);
  EXPECT_EQ(0, exprs_[0]->Min());
  EXPECT_EQ(0, exprs_[1]->Min());
  EXPECT_EQ(0, exprs_[0]->Max());
  EXPECT_EQ(0, exprs_[1]->Max());
  vars_[0]->SetMax(-1);
  vars_[1]->SetMax(-2);
  EXPECT_EQ(0, exprs_[0]->Min());
  EXPECT_EQ(0, exprs_[1]->Min());
  EXPECT_EQ(0, exprs_[0]->Max());
  EXPECT_EQ(0, exprs_[1]->Max());
}

TEST_F(TimesBooleanIntExprTest, SetBoolVarTrue) {
  boolvars_[0]->SetValue(1);
  boolvars_[1]->SetValue(1);
  EXPECT_EQ(-5, exprs_[0]->Min());
  EXPECT_EQ(-3, exprs_[1]->Min());
  EXPECT_EQ(12, exprs_[0]->Max());
  EXPECT_EQ(10, exprs_[1]->Max());
  vars_[0]->SetMax(8);
  vars_[1]->SetMax(7);
  EXPECT_EQ(-5, exprs_[0]->Min());
  EXPECT_EQ(-3, exprs_[1]->Min());
  EXPECT_EQ(8, exprs_[0]->Max());
  EXPECT_EQ(7, exprs_[1]->Max());
  vars_[0]->SetMin(-3);
  vars_[1]->SetMin(-2);
  EXPECT_EQ(-3, exprs_[0]->Min());
  EXPECT_EQ(-2, exprs_[1]->Min());
  EXPECT_EQ(8, exprs_[0]->Max());
  EXPECT_EQ(7, exprs_[1]->Max());
  vars_[0]->SetMax(-2);
  vars_[1]->SetMax(-1);
  EXPECT_EQ(-3, exprs_[0]->Min());
  EXPECT_EQ(-2, exprs_[1]->Min());
  EXPECT_EQ(-2, exprs_[0]->Max());
  EXPECT_EQ(-1, exprs_[1]->Max());
}

TEST_F(TimesBooleanIntExprTest, SetProductMin) {
  exprs_[0]->SetMin(-3);
  exprs_[1]->SetMin(-2);
  EXPECT_EQ(-5, exprs_[0]->Min());
  EXPECT_EQ(-3, exprs_[1]->Min());
  EXPECT_EQ(12, exprs_[0]->Max());
  EXPECT_EQ(10, exprs_[1]->Max());
  exprs_[0]->SetMin(0);
  exprs_[1]->SetMin(0);
  EXPECT_EQ(-5, exprs_[0]->Min());  // boolvar = 0 is still an option.
  EXPECT_EQ(-3, exprs_[1]->Min());
  EXPECT_EQ(12, exprs_[0]->Max());
  EXPECT_EQ(10, exprs_[1]->Max());
  exprs_[0]->SetMin(2);  // Force boolvar to true.
  exprs_[1]->SetMin(3);
  EXPECT_EQ(2, exprs_[0]->Min());
  EXPECT_EQ(3, exprs_[1]->Min());
  EXPECT_EQ(12, exprs_[0]->Max());
  EXPECT_EQ(10, exprs_[1]->Max());
  EXPECT_EQ(1, boolvars_[0]->Min());
  EXPECT_EQ(1, boolvars_[1]->Min());
}

TEST_F(TimesBooleanIntExprTest, SetProductMax) {
  exprs_[0]->SetMax(15);
  exprs_[1]->SetMax(16);
  EXPECT_EQ(-5, exprs_[0]->Min());
  EXPECT_EQ(-3, exprs_[1]->Min());
  EXPECT_EQ(12, exprs_[0]->Max());
  EXPECT_EQ(10, exprs_[1]->Max());
  exprs_[0]->SetMax(0);
  exprs_[1]->SetMax(0);
  EXPECT_EQ(-5, exprs_[0]->Min());  // boolvar = 0 is still an option.
  EXPECT_EQ(-3, exprs_[1]->Min());
  EXPECT_EQ(12, exprs_[0]->Max());
  EXPECT_EQ(10, exprs_[1]->Max());
  exprs_[0]->SetMax(-2);  // Force boolvar to true.
  exprs_[1]->SetMax(-1);
  EXPECT_EQ(-5, exprs_[0]->Min());
  EXPECT_EQ(-3, exprs_[1]->Min());
  EXPECT_EQ(-2, exprs_[0]->Max());
  EXPECT_EQ(-1, exprs_[1]->Max());
  EXPECT_EQ(1, boolvars_[0]->Min());
  EXPECT_EQ(1, boolvars_[1]->Min());
}

// ----- DivIntExprTest -----

class DivIntExprTest : public ::testing::TestWithParam<int64_t> {
 public:
  // The domain covered by x_min, x_max, y_min, y_max.
  static constexpr int64_t kRadius = 7;

 protected:
  void SetUp() override { x_min = GetParam(); }

  static bool NumeratorHasSupport(int64_t x, int64_t y_min, int64_t y_max,
                                  int64_t div_min, int64_t div_max) {
    for (int64_t y = y_min; y <= y_max; ++y) {
      if (y != 0) {
        const int64_t d = x / y;
        if (d >= div_min && d <= div_max) {
          return true;
        }
      }
    }
    return false;
  }

  static bool DenominatorHasSupport(int64_t x_min, int64_t x_max, int64_t y,
                                    int64_t div_min, int64_t div_max) {
    for (int64_t x = x_min; x <= x_max; ++x) {
      const int64_t d = x / y;
      if (d >= div_min && d <= div_max) {
        return true;
      }
    }
    return false;
  }

  void CheckDiv(int64_t x_max, int64_t y_min, int64_t y_max, int64_t new_x_min,
                int64_t new_x_max, int64_t new_y_min, int64_t new_y_max,
                int64_t div_min, int64_t div_max) {
    SCOPED_TRACE(absl::StrFormat("%d..%d / %d..%d == %d..%d", new_x_min,
                                 new_x_max, new_y_min, new_y_max, div_min,
                                 div_max));
    for (int64_t x = x_min; x < new_x_min; ++x) {
      SCOPED_TRACE(absl::StrFormat("ExpectNoSupportNum(%d)", x));
      EXPECT_FALSE(NumeratorHasSupport(x, y_min, y_max, div_min, div_max));
    }
    EXPECT_TRUE(NumeratorHasSupport(new_x_min, y_min, y_max, div_min, div_max));
    EXPECT_TRUE(NumeratorHasSupport(new_x_max, y_min, y_max, div_min, div_max));
    for (int64_t x = new_x_max + 1; x <= x_max; ++x) {
      SCOPED_TRACE(absl::StrFormat("ExpectNoSupportNum(%d)", x));
      EXPECT_FALSE(NumeratorHasSupport(x, y_min, y_max, div_min, div_max));
    }

    for (int64_t y = y_min; y < new_y_min; ++y) {
      if (y != 0) {
        SCOPED_TRACE(absl::StrFormat("ExpectNoSupportDenom(%d)", y));
        EXPECT_FALSE(DenominatorHasSupport(x_min, x_max, y, div_min, div_max));
      }
    }
    EXPECT_TRUE(
        DenominatorHasSupport(x_min, x_max, new_y_min, div_min, div_max));
    EXPECT_TRUE(
        DenominatorHasSupport(x_min, x_max, new_y_max, div_min, div_max));
    for (int64_t y = new_y_max + 1; y <= y_max; ++y) {
      if (y != 0) {
        SCOPED_TRACE(absl::StrFormat("ExpectNoSupportDenom(%d)", y));
        EXPECT_FALSE(DenominatorHasSupport(x_min, x_max, y, div_min, div_max));
      }
    }
  }

  static void CheckDivMinMax(int64_t x_min, int64_t x_max, int64_t y_min,
                             int64_t y_max, int64_t div_min, int64_t div_max) {
    int64_t computed_div_min = kint64max;
    int64_t computed_div_max = kint64min;
    for (int64_t n = x_min; n <= x_max; ++n) {
      for (int64_t d = y_min; d <= y_max; ++d) {
        if (d != 0) {
          computed_div_min = std::min(computed_div_min, n / d);
          computed_div_max = std::max(computed_div_max, n / d);
        }
      }
    }
    CHECK_EQ(div_min, computed_div_min);
    CHECK_EQ(div_min, computed_div_min);
  }

  int64_t x_min;
};

TEST_P(DivIntExprTest, MiscMethods) {
  for (int64_t x_max = x_min; x_max <= kRadius; ++x_max) {
    for (int64_t y_min = -kRadius; y_min <= kRadius; ++y_min) {
      if (y_min != 0) {
        for (int64_t y_max = y_min; y_max <= kRadius; ++y_max) {
          if (y_max != 0) {
            Solver s("DivIntExprTest::CheckBounds");
            IntVar* const x = s.MakeIntVar(x_min, x_max);
            IntVar* const y = s.MakeIntVar(y_min, y_max);
            IntExpr* const div = s.MakeDiv(x, y);
            EXPECT_FALSE(div->DebugString().empty());
            Demon* const d = s.RevAlloc(new NullDemon());
            div->WhenRange(d);
          }
        }
      }
    }
  }
}

TEST_P(DivIntExprTest, CheckBounds) {
  for (int64_t x_max = x_min; x_max <= kRadius; ++x_max) {
    for (int64_t y_min = -kRadius; y_min <= kRadius; ++y_min) {
      if (y_min != 0) {
        for (int64_t y_max = y_min; y_max <= kRadius; ++y_max) {
          if (y_max != 0) {
            Solver s("DivIntExprTest::CheckBounds");
            IntVar* const x = s.MakeIntVar(x_min, x_max);
            IntVar* const y = s.MakeIntVar(y_min, y_max);
            IntExpr* const div = s.MakeDiv(x, y);
            SCOPED_TRACE(
                absl::StrFormat("%d..%d / %d..%d", x_min, x_max, y_min, y_max));
            CheckDivMinMax(x_min, x_max, y_min, y_max, div->Min(), div->Max());
          }
        }
      }
    }
  }
}

TEST_P(DivIntExprTest, SafeCheckBounds) {
  for (int64_t x_max = x_min; x_max <= kRadius; ++x_max) {
    for (int64_t y_min = -kRadius; y_min <= kRadius; ++y_min) {
      if (y_min != 0) {
        for (int64_t y_max = y_min; y_max <= kRadius; ++y_max) {
          if (y_max != 0) {
            Solver s("DivIntExprTest::CheckBounds");
            IntVar* const x = s.MakeIntVar(-kRadius, kRadius);
            IntVar* const y = s.MakeIntVar(-kRadius, kRadius);
            IntExpr* const div = s.MakeDiv(x, y);
            x->SetRange(x_min, x_max);
            y->SetRange(y_min, y_max);
            SCOPED_TRACE(
                absl::StrFormat("%d..%d / %d..%d", x_min, x_max, y_min, y_max));
            CheckDivMinMax(x_min, x_max, y_min, y_max, div->Min(), div->Max());
          }
        }
      }
    }
  }
}

TEST_P(DivIntExprTest, CheckMinSetters) {
  for (int64_t x_max = x_min; x_max <= kRadius; ++x_max) {
    for (int64_t y_min = -kRadius; y_min <= kRadius; ++y_min) {
      if (y_min != 0) {
        for (int64_t y_max = y_min; y_max <= kRadius; ++y_max) {
          if (y_max != 0) {
            Solver s("DivIntExprTest::CheckMinSetters");
            IntVar* const x = s.MakeIntVar(x_min, x_max);
            IntVar* const y = s.MakeIntVar(y_min, y_max);
            IntExpr* const div = s.MakeDiv(x, y);
            const int64_t div_min = div->Min();
            const int64_t div_max = div->Max();
            SCOPED_TRACE(absl::StrFormat("%d..%d / %d..%d == [%d..%d]", x_min,
                                         x_max, y_min, y_max, div_min,
                                         div_max));
            for (int64_t new_min = div_min; new_min <= div_max; ++new_min) {
              SCOPED_TRACE(absl::StrFormat("SetMin(%d)", new_min));
              div->SetMin(new_min);
              CheckDiv(x_max, y_min, y_max, x->Min(), x->Max(), y->Min(),
                       y->Max(), new_min, div_max);
            }
          }
        }
      }
    }
  }
}

TEST_P(DivIntExprTest, SafeCheckMinSetters) {
  for (int64_t x_max = x_min; x_max <= kRadius; ++x_max) {
    for (int64_t y_min = -kRadius; y_min <= kRadius; ++y_min) {
      if (y_min != 0) {
        for (int64_t y_max = y_min; y_max <= kRadius; ++y_max) {
          if (y_max != 0) {
            Solver s("DivIntExprTest::CheckMinSetters");
            IntVar* const x = s.MakeIntVar(-kRadius, kRadius);
            IntVar* const y = s.MakeIntVar(-kRadius, kRadius);
            IntExpr* const div = s.MakeDiv(x, y);
            x->SetRange(x_min, x_max);
            y->SetRange(y_min, y_max);
            const int64_t div_min = div->Min();
            const int64_t div_max = div->Max();
            SCOPED_TRACE(absl::StrFormat("%d..%d / %d..%d == [%d..%d]", x_min,
                                         x_max, y_min, y_max, div_min,
                                         div_max));
            for (int64_t new_min = div_min; new_min <= div_max; ++new_min) {
              SCOPED_TRACE(absl::StrFormat("SetMin(%d)", new_min));
              div->SetMin(new_min);
              CheckDiv(x_max, y_min, y_max, x->Min(), x->Max(), y->Min(),
                       y->Max(), new_min, div_max);
            }
          }
        }
      }
    }
  }
}

TEST_P(DivIntExprTest, CheckMaxSetters) {
  for (int64_t x_max = x_min; x_max <= kRadius; ++x_max) {
    for (int64_t y_min = -kRadius; y_min <= kRadius; ++y_min) {
      if (y_min != 0) {
        for (int64_t y_max = y_min; y_max <= kRadius; ++y_max) {
          if (y_max != 0) {
            Solver s("DivIntExprTest::CheckMaxSetters");
            IntVar* const x = s.MakeIntVar(x_min, x_max);
            IntVar* const y = s.MakeIntVar(y_min, y_max);
            IntExpr* const div = s.MakeDiv(x, y);
            const int64_t div_min = div->Min();
            const int64_t div_max = div->Max();
            SCOPED_TRACE(absl::StrFormat("%d..%d / %d..%d == [%d..%d]", x_min,
                                         x_max, y_min, y_max, div_min,
                                         div_max));
            for (int64_t value = div_min; value <= div_max; ++value) {
              const int64_t new_max = div_max - value + div_min;
              SCOPED_TRACE(absl::StrFormat("SetMax(%d)", new_max));
              div->SetMax(new_max);
              CheckDiv(x_max, y_min, y_max, x->Min(), x->Max(), y->Min(),
                       y->Max(), div_min, new_max);
            }
          }
        }
      }
    }
  }
}

TEST_P(DivIntExprTest, SafeCheckMaxSetters) {
  for (int64_t x_max = x_min; x_max <= kRadius; ++x_max) {
    for (int64_t y_min = -kRadius; y_min <= kRadius; ++y_min) {
      if (y_min != 0) {
        for (int64_t y_max = y_min; y_max <= kRadius; ++y_max) {
          if (y_max != 0) {
            Solver s("DivIntExprTest::CheckMaxSetters");
            IntVar* const x = s.MakeIntVar(-kRadius, kRadius);
            IntVar* const y = s.MakeIntVar(-kRadius, kRadius);
            IntExpr* const div = s.MakeDiv(x, y);
            x->SetRange(x_min, x_max);
            y->SetRange(y_min, y_max);
            const int64_t div_min = div->Min();
            const int64_t div_max = div->Max();
            SCOPED_TRACE(absl::StrFormat("%d..%d / %d..%d == [%d..%d]", x_min,
                                         x_max, y_min, y_max, div_min,
                                         div_max));
            for (int64_t value = div_min; value <= div_max; ++value) {
              const int64_t new_max = div_max - value + div_min;
              SCOPED_TRACE(absl::StrFormat("SetMax(%d)", new_max));
              div->SetMax(new_max);
              CheckDiv(x_max, y_min, y_max, x->Min(), x->Max(), y->Min(),
                       y->Max(), div_min, new_max);
            }
          }
        }
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(Domain, DivIntExprTest,
                         testing::Range(-DivIntExprTest::kRadius,
                                        DivIntExprTest::kRadius));

// ----- IntAbs -----

class IntAbsTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("IntAbsTest");
    vars_.resize(4);
    exprs_.resize(4);
    vars_[0] = solver_->MakeIntVar(-5, -3);
    vars_[1] = solver_->MakeIntVar(-5, 3);
    vars_[2] = solver_->MakeIntVar(-3, 5);
    vars_[3] = solver_->MakeIntVar(3, 5);
    exprs_[0] = solver_->MakeAbs(vars_[0]);
    exprs_[1] = solver_->MakeAbs(vars_[1]);
    exprs_[2] = solver_->MakeAbs(vars_[2]);
    exprs_[3] = solver_->MakeAbs(vars_[3]);
  }
  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  std::vector<IntExpr*> exprs_;
};

TEST_F(IntAbsTest, DebugString) {
  EXPECT_FALSE(exprs_[0]->DebugString().empty());
  EXPECT_FALSE(exprs_[1]->DebugString().empty());
  EXPECT_FALSE(exprs_[2]->DebugString().empty());
  EXPECT_FALSE(exprs_[3]->DebugString().empty());
}

TEST_F(IntAbsTest, MinMaxGetters) {
  EXPECT_EQ(3, exprs_[0]->Min());
  EXPECT_EQ(5, exprs_[0]->Max());
  EXPECT_EQ(0, exprs_[1]->Min());
  EXPECT_EQ(5, exprs_[1]->Max());
  EXPECT_EQ(0, exprs_[2]->Min());
  EXPECT_EQ(5, exprs_[2]->Max());
  EXPECT_EQ(3, exprs_[3]->Min());
  EXPECT_EQ(5, exprs_[3]->Max());
}

TEST_IN_PROPAGATION2(IntAbsTest, SetMax, solver, true, std::vector<IntVar*>,
                     vars_, std::vector<IntExpr*>, exprs_) {
  exprs_[0]->SetMax(4);
  EXPECT_EQ(3, exprs_[0]->Min());
  EXPECT_EQ(4, exprs_[0]->Max());
  EXPECT_EQ(-4, vars_[0]->Min());
  EXPECT_EQ(-3, vars_[0]->Max());
  exprs_[1]->SetMax(4);
  EXPECT_EQ(0, exprs_[1]->Min());
  EXPECT_EQ(4, exprs_[1]->Max());
  EXPECT_EQ(-4, vars_[1]->Min());
  EXPECT_EQ(3, vars_[1]->Max());
  exprs_[2]->SetMax(4);
  EXPECT_EQ(0, exprs_[2]->Min());
  EXPECT_EQ(4, exprs_[2]->Max());
  EXPECT_EQ(-3, vars_[2]->Min());
  EXPECT_EQ(4, vars_[2]->Max());
  exprs_[3]->SetMax(4);
  EXPECT_EQ(3, exprs_[3]->Min());
  EXPECT_EQ(4, exprs_[3]->Max());
  EXPECT_EQ(3, vars_[3]->Min());
  EXPECT_EQ(4, vars_[3]->Max());
  return nullptr;
}

TEST_IN_PROPAGATION2(IntAbsTest, SetMin, solver, true, std::vector<IntVar*>,
                     vars_, std::vector<IntExpr*>, exprs_) {
  exprs_[0]->SetMin(4);
  EXPECT_EQ(4, exprs_[0]->Min());
  EXPECT_EQ(5, exprs_[0]->Max());
  EXPECT_EQ(-5, vars_[0]->Min());
  EXPECT_EQ(-4, vars_[0]->Max());
  exprs_[1]->SetMin(4);
  EXPECT_EQ(4, exprs_[1]->Min());
  EXPECT_EQ(5, exprs_[1]->Max());
  EXPECT_EQ(-5, vars_[1]->Min());
  EXPECT_EQ(-4, vars_[1]->Max());
  exprs_[2]->SetMin(4);
  EXPECT_EQ(4, exprs_[2]->Min());
  EXPECT_EQ(5, exprs_[2]->Max());
  EXPECT_EQ(4, vars_[2]->Min());
  EXPECT_EQ(5, vars_[2]->Max());
  exprs_[3]->SetMin(4);
  EXPECT_EQ(4, exprs_[3]->Min());
  EXPECT_EQ(5, exprs_[3]->Max());
  EXPECT_EQ(4, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
  return nullptr;
}

TEST_IN_PROPAGATION2(IntAbsTest, SetNonZeroMin, solver, true,
                     std::vector<IntVar*>, vars_, std::vector<IntExpr*>,
                     exprs_) {
  exprs_[0]->SetMin(1);
  EXPECT_EQ(3, exprs_[0]->Min());
  EXPECT_EQ(5, exprs_[0]->Max());
  EXPECT_EQ(-5, vars_[0]->Min());
  EXPECT_EQ(-3, vars_[0]->Max());
  exprs_[1]->SetMin(1);
  EXPECT_EQ(0, exprs_[1]->Min());  // An expression.
  EXPECT_EQ(5, exprs_[1]->Max());
  EXPECT_EQ(-5, vars_[1]->Min());
  EXPECT_EQ(3, vars_[1]->Max());
  exprs_[2]->SetMin(1);
  EXPECT_EQ(0, exprs_[2]->Min());  // An expression.
  EXPECT_EQ(5, exprs_[2]->Max());
  EXPECT_EQ(-3, vars_[2]->Min());
  EXPECT_EQ(5, vars_[2]->Max());
  exprs_[3]->SetMin(1);
  EXPECT_EQ(3, exprs_[3]->Min());
  EXPECT_EQ(5, exprs_[3]->Max());
  EXPECT_EQ(3, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
  return nullptr;
}

TEST_F(IntAbsTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  exprs_[0]->WhenRange(d);
  exprs_[1]->WhenRange(d);
  exprs_[2]->WhenRange(d);
  exprs_[3]->WhenRange(d);
}

// ----- IntPosSquare ------

class IntPosSquareTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("IntPosSquareTest");
    var_ = solver_->MakeIntVar(3, 5);
    expr_ = solver_->MakeSquare(var_);
  }

  std::unique_ptr<Solver> solver_;
  IntVar* var_;
  IntExpr* expr_;
};

TEST_F(IntPosSquareTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
}

TEST_F(IntPosSquareTest, MinMaxGetters) {
  EXPECT_EQ(9, expr_->Min());
  EXPECT_EQ(25, expr_->Max());
}

TEST_F(IntPosSquareTest, SetMax) {
  expr_->SetMax(17);
  EXPECT_EQ(9, expr_->Min());
  EXPECT_EQ(16, expr_->Max());
  EXPECT_EQ(3, var_->Min());
  EXPECT_EQ(4, var_->Max());
}

TEST_F(IntPosSquareTest, SetMin) {
  expr_->SetMin(15);
  EXPECT_EQ(16, expr_->Min());
  EXPECT_EQ(25, expr_->Max());
  EXPECT_EQ(4, var_->Min());
  EXPECT_EQ(5, var_->Max());
}

TEST_F(IntPosSquareTest, SetNonZeroMin) {
  expr_->SetMin(1);
  EXPECT_EQ(9, expr_->Min());
  EXPECT_EQ(25, expr_->Max());
  EXPECT_EQ(3, var_->Min());
  EXPECT_EQ(5, var_->Max());
}

// Exercising square root rounding errors using Pell's equation for n=1:
// x^2 - y^2 = 1, x and y positive.
// Note: there are no solutions for n=1.
TEST(GenIntPosSquareTest, InfeasiblePell1) {
  Solver solver("Pell");
  // Reducing range to save time (rounding errors start happening at 67108865).
  const int64_t min_limit = 67108860;
  const int64_t max_limit = 70000000;
  IntVar* x = solver.MakeIntVar(min_limit, max_limit, "x");
  IntVar* y = solver.MakeIntVar(min_limit, max_limit, "y");
  solver.AddConstraint(solver.MakeGreater(x, 0));
  solver.AddConstraint(solver.MakeGreater(y, 0));
  // Pell's equation for n=1:
  solver.AddConstraint(solver.MakeEquality(
      solver.MakeDifference(solver.MakeProd(x, x),
                            solver.MakeProd(solver.MakeProd(y, y), 1)),
      1));
  DecisionBuilder* db = solver.MakePhase(
      x, y, Solver::CHOOSE_MIN_SIZE_LOWEST_MAX, Solver::ASSIGN_MIN_VALUE);
  solver.NewSearch(db);
  EXPECT_FALSE(solver.NextSolution()) << x->Value() << " " << y->Value();
}

// ----- IntSquareTest -----

class IntSquareTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("IntSquareTest");
    vars_.resize(4);
    vars_[0] = solver_->MakeIntVar(-5, -3);
    vars_[1] = solver_->MakeIntVar(-5, 3);
    vars_[2] = solver_->MakeIntVar(-3, 5);
    vars_[3] = solver_->MakeIntVar(3, 5);
    expr0_ = solver_->MakeSquare(vars_[0]);
    expr1_ = solver_->MakeSquare(vars_[1]);
    expr2_ = solver_->MakeSquare(vars_[2]);
    expr3_ = solver_->MakeSquare(vars_[3]);
  }

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  IntExpr* expr0_;
  IntExpr* expr1_;
  IntExpr* expr2_;
  IntExpr* expr3_;
};

TEST_F(IntSquareTest, MinMaxGetters) {
  EXPECT_EQ(9, expr0_->Min());
  EXPECT_EQ(25, expr0_->Max());
  EXPECT_EQ(0, expr1_->Min());
  EXPECT_EQ(25, expr1_->Max());
  EXPECT_EQ(0, expr2_->Min());
  EXPECT_EQ(25, expr2_->Max());
  EXPECT_EQ(9, expr3_->Min());
  EXPECT_EQ(25, expr3_->Max());
}

TEST_F(IntSquareTest, SetMax) {
  expr0_->SetMax(17);
  EXPECT_EQ(9, expr0_->Min());
  EXPECT_EQ(16, expr0_->Max());
  EXPECT_EQ(-4, vars_[0]->Min());
  EXPECT_EQ(-3, vars_[0]->Max());
  expr1_->SetMax(17);
  EXPECT_EQ(0, expr1_->Min());
  EXPECT_EQ(16, expr1_->Max());
  EXPECT_EQ(-4, vars_[1]->Min());
  EXPECT_EQ(3, vars_[1]->Max());
  expr2_->SetMax(17);
  EXPECT_EQ(0, expr2_->Min());
  EXPECT_EQ(16, expr2_->Max());
  EXPECT_EQ(-3, vars_[2]->Min());
  EXPECT_EQ(4, vars_[2]->Max());
  expr3_->SetMax(17);
  EXPECT_EQ(9, expr3_->Min());
  EXPECT_EQ(16, expr3_->Max());
  EXPECT_EQ(3, vars_[3]->Min());
  EXPECT_EQ(4, vars_[3]->Max());
}

TEST_F(IntSquareTest, SetMin) {
  expr0_->SetMin(15);
  EXPECT_EQ(16, expr0_->Min());
  EXPECT_EQ(25, expr0_->Max());
  EXPECT_EQ(-5, vars_[0]->Min());
  EXPECT_EQ(-4, vars_[0]->Max());
  expr1_->SetMin(15);
  EXPECT_EQ(16, expr1_->Min());
  EXPECT_EQ(25, expr1_->Max());
  EXPECT_EQ(-5, vars_[1]->Min());
  EXPECT_EQ(-4, vars_[1]->Max());
  expr2_->SetMin(15);
  EXPECT_EQ(16, expr2_->Min());
  EXPECT_EQ(25, expr2_->Max());
  EXPECT_EQ(4, vars_[2]->Min());
  EXPECT_EQ(5, vars_[2]->Max());
  expr3_->SetMin(15);
  EXPECT_EQ(16, expr3_->Min());
  EXPECT_EQ(25, expr3_->Max());
  EXPECT_EQ(4, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
}

TEST_F(IntSquareTest, SetNonZeroMin) {
  expr0_->SetMin(1);
  EXPECT_EQ(9, expr0_->Min());
  EXPECT_EQ(25, expr0_->Max());
  EXPECT_EQ(-5, vars_[0]->Min());
  EXPECT_EQ(-3, vars_[0]->Max());
  expr1_->SetMin(1);
  EXPECT_EQ(0, expr1_->Min());
  EXPECT_EQ(25, expr1_->Max());
  EXPECT_EQ(-5, vars_[1]->Min());
  EXPECT_EQ(3, vars_[1]->Max());
  expr2_->SetMin(1);
  EXPECT_EQ(0, expr2_->Min());
  EXPECT_EQ(25, expr2_->Max());
  EXPECT_EQ(-3, vars_[2]->Min());
  EXPECT_EQ(5, vars_[2]->Max());
  expr3_->SetMin(1);
  EXPECT_EQ(9, expr3_->Min());
  EXPECT_EQ(25, expr3_->Max());
  EXPECT_EQ(3, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
}

TEST_F(IntSquareTest, NonOverflow) {
  // kint32max * kint32max is far from overflowing int64 (≈kint64max/2).
  IntVar* const var1 =
      solver_->MakeIntVar(kint32min - int64_t{10}, kint32max + int64_t{10});
  IntExpr* const sq1 = solver_->MakeSquare(var1);
  EXPECT_EQ(sq1->Min(), 0);
  EXPECT_EQ(sq1->Max(), (kint32min - int64_t{10}) * (kint32min - int64_t{10}));
  sq1->SetMax(kint64max);
  EXPECT_EQ(kint32min - int64_t{10}, var1->Min());
  EXPECT_EQ(kint32max + int64_t{10}, var1->Max());
}

TEST_F(IntSquareTest, Overflow) {
  const int64_t sqrt_kint64max = static_cast<int64_t>(sqrt(kint64max));
  IntVar* const var1 =
      solver_->MakeIntVar(-sqrt_kint64max - 11, sqrt_kint64max + 10);
  IntExpr* const sq1 = solver_->MakeSquare(var1);
  EXPECT_EQ(0, sq1->Min());
  EXPECT_EQ(kint64max, sq1->Max());
  sq1->SetMax(kint64max);
  EXPECT_EQ(-sqrt_kint64max - 11, var1->Min());
  EXPECT_EQ(sqrt_kint64max + 10, var1->Max());

  IntVar* const var2 = solver_->MakeIntVar(-sqrt_kint64max - 11, 5);
  IntExpr* const sq2 = solver_->MakeSquare(var2);
  EXPECT_EQ(0, sq2->Min());
  EXPECT_EQ(kint64max, sq2->Max());
  sq2->SetMax(kint64max);
  EXPECT_EQ(-sqrt_kint64max - 11, var2->Min());
  EXPECT_EQ(5, var2->Max());

  IntVar* const var3 = solver_->MakeIntVar(-5, sqrt_kint64max + 7);
  IntExpr* const sq3 = solver_->MakeSquare(var3);
  EXPECT_EQ(0, sq3->Min());
  EXPECT_EQ(kint64max, sq3->Max());
  sq3->SetMax(kint64max);
  EXPECT_EQ(-5, var3->Min());
  EXPECT_EQ(sqrt_kint64max + 7, var3->Max());

  IntVar* const var4 =
      solver_->MakeIntVar(-sqrt_kint64max - 13, -sqrt_kint64max - 3);
  IntExpr* const sq4 = solver_->MakeSquare(var4);
  EXPECT_EQ(kint64max, sq4->Min());
  EXPECT_EQ(kint64max, sq4->Max());
  sq4->SetMax(kint64max);
  EXPECT_EQ(-sqrt_kint64max - 13, var4->Min());
  EXPECT_EQ(-sqrt_kint64max - 3, var4->Max());

  IntVar* const var5 =
      solver_->MakeIntVar(sqrt_kint64max + 6, sqrt_kint64max + 9);
  IntExpr* const sq5 = solver_->MakeSquare(var5);
  EXPECT_EQ(kint64max, sq5->Min());
  EXPECT_EQ(kint64max, sq5->Max());
  sq5->SetMax(kint64max);
  EXPECT_EQ(sqrt_kint64max + 6, var5->Min());
  EXPECT_EQ(sqrt_kint64max + 9, var5->Max());

  IntVar* const var6 =
      solver_->MakeIntVar(-sqrt_kint64max - 10, -sqrt_kint64max);
  IntExpr* const sq6 = solver_->MakeSquare(var6);
  EXPECT_EQ(sqrt_kint64max * sqrt_kint64max, sq6->Min());
  EXPECT_EQ(kint64max, sq6->Max());
  sq6->SetMax(kint64max);
  EXPECT_EQ(-sqrt_kint64max - 10, var6->Min());
  EXPECT_EQ(-sqrt_kint64max, var6->Max());

  IntVar* const var7 = solver_->MakeIntVar(sqrt_kint64max, sqrt_kint64max + 5);
  IntExpr* const sq7 = solver_->MakeSquare(var7);
  EXPECT_EQ(sqrt_kint64max * sqrt_kint64max, sq7->Min());
  EXPECT_EQ(kint64max, sq7->Max());
  sq7->SetMax(kint64max);
  EXPECT_EQ(sqrt_kint64max, var7->Min());
  EXPECT_EQ(sqrt_kint64max + 5, var7->Max());
}

// ----- IntPowerTest -----

class IntPowerEvenTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("IntPowerEvenTest");
    vars_.resize(4);
    vars_[0] = solver_->MakeIntVar(-5, -3);
    vars_[1] = solver_->MakeIntVar(-5, 3);
    vars_[2] = solver_->MakeIntVar(-3, 5);
    vars_[3] = solver_->MakeIntVar(3, 5);
    expr0_ = solver_->MakePower(vars_[0], 4);
    expr1_ = solver_->MakePower(vars_[1], 4);
    expr2_ = solver_->MakePower(vars_[2], 4);
    expr3_ = solver_->MakePower(vars_[3], 4);
  }

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  IntExpr* expr0_;
  IntExpr* expr1_;
  IntExpr* expr2_;
  IntExpr* expr3_;
};

TEST_F(IntPowerEvenTest, MinMaxGetters) {
  EXPECT_EQ(81, expr0_->Min());
  EXPECT_EQ(625, expr0_->Max());
  EXPECT_EQ(0, expr1_->Min());
  EXPECT_EQ(625, expr1_->Max());
  EXPECT_EQ(0, expr2_->Min());
  EXPECT_EQ(625, expr2_->Max());
  EXPECT_EQ(81, expr3_->Min());
  EXPECT_EQ(625, expr3_->Max());
}

TEST_F(IntPowerEvenTest, SetMax) {
  expr0_->SetMax(289);
  EXPECT_EQ(81, expr0_->Min());
  EXPECT_EQ(256, expr0_->Max());
  EXPECT_EQ(-4, vars_[0]->Min());
  EXPECT_EQ(-3, vars_[0]->Max());
  expr1_->SetMax(289);
  EXPECT_EQ(0, expr1_->Min());
  EXPECT_EQ(256, expr1_->Max());
  EXPECT_EQ(-4, vars_[1]->Min());
  EXPECT_EQ(3, vars_[1]->Max());
  expr2_->SetMax(289);
  EXPECT_EQ(0, expr2_->Min());
  EXPECT_EQ(256, expr2_->Max());
  EXPECT_EQ(-3, vars_[2]->Min());
  EXPECT_EQ(4, vars_[2]->Max());
  expr3_->SetMax(289);
  EXPECT_EQ(81, expr3_->Min());
  EXPECT_EQ(256, expr3_->Max());
  EXPECT_EQ(3, vars_[3]->Min());
  EXPECT_EQ(4, vars_[3]->Max());
}

TEST_F(IntPowerEvenTest, SetMin) {
  expr0_->SetMin(225);
  EXPECT_EQ(256, expr0_->Min());
  EXPECT_EQ(625, expr0_->Max());
  EXPECT_EQ(-5, vars_[0]->Min());
  EXPECT_EQ(-4, vars_[0]->Max());
  expr1_->SetMin(225);
  EXPECT_EQ(256, expr1_->Min());
  EXPECT_EQ(625, expr1_->Max());
  EXPECT_EQ(-5, vars_[1]->Min());
  EXPECT_EQ(-4, vars_[1]->Max());
  expr2_->SetMin(225);
  EXPECT_EQ(256, expr2_->Min());
  EXPECT_EQ(625, expr2_->Max());
  EXPECT_EQ(4, vars_[2]->Min());
  EXPECT_EQ(5, vars_[2]->Max());
  expr3_->SetMin(225);
  EXPECT_EQ(256, expr3_->Min());
  EXPECT_EQ(625, expr3_->Max());
  EXPECT_EQ(4, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
}

TEST_F(IntPowerEvenTest, SetNonZeroMin) {
  expr0_->SetMin(1);
  EXPECT_EQ(81, expr0_->Min());
  EXPECT_EQ(625, expr0_->Max());
  EXPECT_EQ(-5, vars_[0]->Min());
  EXPECT_EQ(-3, vars_[0]->Max());
  expr1_->SetMin(1);
  EXPECT_EQ(0, expr1_->Min());
  EXPECT_EQ(625, expr1_->Max());
  EXPECT_EQ(-5, vars_[1]->Min());
  EXPECT_EQ(3, vars_[1]->Max());
  expr2_->SetMin(1);
  EXPECT_EQ(0, expr2_->Min());
  EXPECT_EQ(625, expr2_->Max());
  EXPECT_EQ(-3, vars_[2]->Min());
  EXPECT_EQ(5, vars_[2]->Max());
  expr3_->SetMin(1);
  EXPECT_EQ(81, expr3_->Min());
  EXPECT_EQ(625, expr3_->Max());
  EXPECT_EQ(3, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
}

TEST_F(IntPowerEvenTest, Overflow) {
  IntVar* const var1 =
      solver_->MakeIntVar(kint32min - int64_t{10}, kint32max + int64_t{10});
  IntExpr* const sq1 = solver_->MakePower(var1, 4);
  EXPECT_EQ(0, sq1->Min());
  EXPECT_EQ(kint64max, sq1->Max());
  sq1->SetMax(kint64max);
  EXPECT_EQ(kint32min - int64_t{10}, var1->Min());
  EXPECT_EQ(kint32max + int64_t{10}, var1->Max());

  IntVar* const var2 = solver_->MakeIntVar(kint32min - int64_t{10}, 5);
  IntExpr* const sq2 = solver_->MakePower(var2, 4);
  EXPECT_EQ(0, sq2->Min());
  EXPECT_EQ(kint64max, sq2->Max());
  sq2->SetMax(kint64max);
  EXPECT_EQ(kint32min - int64_t{10}, var2->Min());
  EXPECT_EQ(5, var2->Max());

  IntVar* const var3 = solver_->MakeIntVar(-5, kint32max + int64_t{10});
  IntExpr* const sq3 = solver_->MakePower(var3, 4);
  EXPECT_EQ(0, sq3->Min());
  EXPECT_EQ(kint64max, sq3->Max());
  sq3->SetMax(kint64max);
  EXPECT_EQ(-5, var3->Min());
  EXPECT_EQ(kint32max + int64_t{10}, var3->Max());

  IntVar* const var4 =
      solver_->MakeIntVar(kint32min - int64_t{15}, kint32min - int64_t{5});
  IntExpr* const sq4 = solver_->MakePower(var4, 4);
  EXPECT_EQ(kint64max, sq4->Min());
  EXPECT_EQ(kint64max, sq4->Max());
  sq4->SetMax(kint64max);
  EXPECT_EQ(kint32min - int64_t{15}, var4->Min());
  EXPECT_EQ(kint32min - int64_t{5}, var4->Max());

  IntVar* const var5 =
      solver_->MakeIntVar(kint32max + int64_t{5}, kint32max + int64_t{15});
  IntExpr* const sq5 = solver_->MakePower(var5, 4);
  EXPECT_EQ(kint64max, sq5->Min());
  EXPECT_EQ(kint64max, sq5->Max());
  sq5->SetMax(kint64max);
  EXPECT_EQ(kint32max + int64_t{5}, var5->Min());
  EXPECT_EQ(kint32max + int64_t{15}, var5->Max());
}

TEST_F(IntPowerEvenTest, Rounding) {
  IntVar* const var1 = solver_->MakeIntVar(-20, 20);
  IntExpr* const sq1 = solver_->MakePower(var1, 4);
  sq1->SetMax(65536);
  EXPECT_EQ(-16, var1->Min());
  EXPECT_EQ(16, var1->Max());

  IntVar* const var2 = solver_->MakeIntVar(10, 20);
  IntExpr* const sq2 = solver_->MakePower(var2, 4);
  sq2->SetMin(65536);
  EXPECT_EQ(16, var2->Min());

  IntVar* const var3 = solver_->MakeIntVar(-20, -10);
  IntExpr* const sq3 = solver_->MakePower(var3, 4);
  sq3->SetMin(65536);
  EXPECT_EQ(-16, var3->Max());
}

class IntPowerOddTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("IntPowerOddTest");
    vars_.resize(4);
    vars_[0] = solver_->MakeIntVar(-5, -3);
    vars_[1] = solver_->MakeIntVar(-5, 3);
    vars_[2] = solver_->MakeIntVar(-3, 5);
    vars_[3] = solver_->MakeIntVar(3, 5);
    expr0_ = solver_->MakePower(vars_[0], 3);
    expr1_ = solver_->MakePower(vars_[1], 3);
    expr2_ = solver_->MakePower(vars_[2], 3);
    expr3_ = solver_->MakePower(vars_[3], 3);
  }

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  IntExpr* expr0_;
  IntExpr* expr1_;
  IntExpr* expr2_;
  IntExpr* expr3_;
};

TEST_F(IntPowerOddTest, MinMaxGetters) {
  EXPECT_EQ(-125, expr0_->Min());
  EXPECT_EQ(-27, expr0_->Max());
  EXPECT_EQ(-125, expr1_->Min());
  EXPECT_EQ(27, expr1_->Max());
  EXPECT_EQ(-27, expr2_->Min());
  EXPECT_EQ(125, expr2_->Max());
  EXPECT_EQ(27, expr3_->Min());
  EXPECT_EQ(125, expr3_->Max());
}

TEST_F(IntPowerOddTest, SetMax) {
  expr0_->SetMax(-60);
  EXPECT_EQ(-125, expr0_->Min());
  EXPECT_EQ(-64, expr0_->Max());
  EXPECT_EQ(-5, vars_[0]->Min());
  EXPECT_EQ(-4, vars_[0]->Max());
  expr1_->SetMax(-64);
  EXPECT_EQ(-125, expr1_->Min());
  EXPECT_EQ(-64, expr1_->Max());
  EXPECT_EQ(-5, vars_[1]->Min());
  EXPECT_EQ(-4, vars_[1]->Max());
  expr2_->SetMax(64);
  EXPECT_EQ(-27, expr2_->Min());
  EXPECT_EQ(64, expr2_->Max());
  EXPECT_EQ(-3, vars_[2]->Min());
  EXPECT_EQ(4, vars_[2]->Max());
  expr3_->SetMax(64);
  EXPECT_EQ(27, expr3_->Min());
  EXPECT_EQ(64, expr3_->Max());
  EXPECT_EQ(3, vars_[3]->Min());
  EXPECT_EQ(4, vars_[3]->Max());
}

TEST_F(IntPowerOddTest, SetMin) {
  expr0_->SetMin(-70);
  EXPECT_EQ(-64, expr0_->Min());
  EXPECT_EQ(-27, expr0_->Max());
  EXPECT_EQ(-4, vars_[0]->Min());
  EXPECT_EQ(-3, vars_[0]->Max());
  expr1_->SetMin(-70);
  EXPECT_EQ(-64, expr1_->Min());
  EXPECT_EQ(27, expr1_->Max());
  EXPECT_EQ(-4, vars_[1]->Min());
  EXPECT_EQ(3, vars_[1]->Max());
  expr2_->SetMin(60);
  EXPECT_EQ(64, expr2_->Min());
  EXPECT_EQ(125, expr2_->Max());
  EXPECT_EQ(4, vars_[2]->Min());
  EXPECT_EQ(5, vars_[2]->Max());
  expr3_->SetMin(60);
  EXPECT_EQ(64, expr3_->Min());
  EXPECT_EQ(125, expr3_->Max());
  EXPECT_EQ(4, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
}

TEST_F(IntPowerOddTest, Overflow) {
  IntVar* const var1 =
      solver_->MakeIntVar(kint32min - int64_t{10}, kint32max + int64_t{10});
  IntExpr* const sq1 = solver_->MakePower(var1, 3);
  EXPECT_EQ(kint64min, sq1->Min());
  EXPECT_EQ(kint64max, sq1->Max());
  sq1->SetMax(kint64max);
  sq1->SetMin(kint64min);
  EXPECT_EQ(kint32min - int64_t{10}, var1->Min());
  EXPECT_EQ(kint32max + int64_t{10}, var1->Max());

  IntVar* const var2 = solver_->MakeIntVar(kint32min - int64_t{10}, 5);
  IntExpr* const sq2 = solver_->MakePower(var2, 3);
  EXPECT_EQ(kint64min, sq2->Min());
  EXPECT_EQ(125, sq2->Max());
  sq2->SetMin(kint64min);
  EXPECT_EQ(kint32min - int64_t{10}, var2->Min());
  EXPECT_EQ(5, var2->Max());

  IntVar* const var3 = solver_->MakeIntVar(-5, kint32max + int64_t{10});
  IntExpr* const sq3 = solver_->MakePower(var3, 3);
  EXPECT_EQ(-125, sq3->Min());
  EXPECT_EQ(kint64max, sq3->Max());
  sq3->SetMax(kint64max);
  EXPECT_EQ(-5, var3->Min());
  EXPECT_EQ(kint32max + int64_t{10}, var3->Max());
}

TEST_F(IntPowerOddTest, Rounding) {
  IntVar* const var1 = solver_->MakeIntVar(-20, 20);
  IntExpr* const sq1 = solver_->MakePower(var1, 3);
  sq1->SetMin(-4096);
  sq1->SetMax(4096);
  EXPECT_EQ(-16, var1->Min());
  EXPECT_EQ(16, var1->Max());

  IntVar* const var2 = solver_->MakeIntVar(10, 40);
  IntExpr* const sq2 = solver_->MakePower(var2, 3);
  sq2->SetMin(4096);
  EXPECT_EQ(16, var2->Min());

  IntVar* const var3 = solver_->MakeIntVar(-30, -10);
  IntExpr* const sq3 = solver_->MakePower(var3, 3);
  sq3->SetMax(-4096);
  EXPECT_EQ(-16, var3->Max());
}

// ----------

class BaseIntExprTest2 : public BaseIntExprTest {
 protected:
  explicit BaseIntExprTest2(const std::string& name) : BaseIntExprTest(name) {}
  void MakeVars(std::vector<IntVar*>* vars) override {
    for (int i = 0; i < 2; ++i) {
      vars->push_back(solver_->MakeIntVar(i * 5 + 5, i * 5 + 15));
    }
  }
};

// ----- MinIntExpr -----

class MinIntExprTest : public BaseIntExprTest2 {
 protected:
  MinIntExprTest() : BaseIntExprTest2("MinIntExprTest") {}
  IntExpr* MakeExpr() override { return solver_->MakeMin(vars_[0], vars_[1]); }
};

TEST_F(MinIntExprTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
}

TEST_F(MinIntExprTest, MinMaxGetters) {
  EXPECT_EQ(5, expr_->Min());
  EXPECT_EQ(15, expr_->Max());
}

TEST_F(MinIntExprTest, SetMin) {
  expr_->SetMin(9);
  EXPECT_EQ(9, vars_[0]->Min());
  EXPECT_EQ(10, vars_[1]->Min());
  expr_->SetMin(12);
  EXPECT_EQ(12, vars_[0]->Min());
  EXPECT_EQ(12, vars_[1]->Min());
}

TEST_F(MinIntExprTest, SetMax) {
  expr_->SetMax(19);
  EXPECT_EQ(15, vars_[0]->Max());
  EXPECT_EQ(20, vars_[1]->Max());
  expr_->SetMax(9);
  EXPECT_EQ(9, vars_[0]->Max());
  EXPECT_EQ(20, vars_[1]->Max());
}

TEST_F(MinIntExprTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr_->WhenRange(d);
}

// ----- MinCstIntExpr -----

class MinIntCstExprTest : public BaseIntExprTest2 {
 protected:
  MinIntCstExprTest() : BaseIntExprTest2("MinIntCstExprTest") {}
  IntExpr* MakeExpr() override { return solver_->MakeMin(vars_[0], 10); }
};

TEST_F(MinIntCstExprTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
}

TEST_F(MinIntCstExprTest, MinMaxGetters) {
  EXPECT_EQ(5, expr_->Min());
  EXPECT_EQ(10, expr_->Max());
}

TEST_F(MinIntCstExprTest, SetMin) {
  expr_->SetMin(9);
  EXPECT_EQ(9, vars_[0]->Min());
}

TEST_IN_PROPAGATION0(MinIntCstExprTest, SetMinFail, s, false) {
  IntVar* var = s->MakeIntVar(0, 20);
  IntExpr* expr = s->MakeMin(var, 10);
  expr->SetMin(12);
  return nullptr;
}

TEST_F(MinIntCstExprTest, SetMax) {
  expr_->SetMax(13);
  EXPECT_EQ(15, vars_[0]->Max());
  expr_->SetMax(9);
  EXPECT_EQ(9, vars_[0]->Max());
}

TEST_F(MinIntCstExprTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr_->WhenRange(d);
}

// ----- MaxIntExpr -----

class MaxIntExprTest : public BaseIntExprTest2 {
 protected:
  MaxIntExprTest() : BaseIntExprTest2("MaxIntExprTest") {}
  IntExpr* MakeExpr() override { return solver_->MakeMax(vars_[0], vars_[1]); }
};

TEST_F(MaxIntExprTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
}

TEST_F(MaxIntExprTest, MinMaxGetters) {
  EXPECT_EQ(10, expr_->Min());
  EXPECT_EQ(20, expr_->Max());
}

TEST_F(MaxIntExprTest, SetMin) {
  expr_->SetMin(9);
  EXPECT_EQ(5, vars_[0]->Min());
  EXPECT_EQ(10, vars_[1]->Min());
  expr_->SetMin(17);
  EXPECT_EQ(5, vars_[0]->Min());
  EXPECT_EQ(17, vars_[1]->Min());
}

TEST_F(MaxIntExprTest, SetMax) {
  expr_->SetMax(19);
  EXPECT_EQ(15, vars_[0]->Max());
  EXPECT_EQ(19, vars_[1]->Max());
  expr_->SetMax(11);
  EXPECT_EQ(11, vars_[0]->Max());
  EXPECT_EQ(11, vars_[1]->Max());
}

TEST_F(MaxIntExprTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr_->WhenRange(d);
}

// ----- MaxCstIntExpr -----

class MaxIntCstExprTest : public BaseIntExprTest2 {
 protected:
  MaxIntCstExprTest() : BaseIntExprTest2("MaxIntCstExprTest") {}
  IntExpr* MakeExpr() override { return solver_->MakeMax(vars_[0], 10); }
};

TEST_F(MaxIntCstExprTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
}

TEST_F(MaxIntCstExprTest, MaxMaxGetters) {
  EXPECT_EQ(10, expr_->Min());
  EXPECT_EQ(15, expr_->Max());
}

TEST_F(MaxIntCstExprTest, SetMin) {
  expr_->SetMin(9);
  EXPECT_EQ(5, vars_[0]->Min());
  expr_->SetMin(12);
  EXPECT_EQ(12, vars_[0]->Min());
}

TEST_F(MaxIntCstExprTest, SetMax) {
  expr_->SetMax(13);
  EXPECT_EQ(13, vars_[0]->Max());
}

TEST_IN_PROPAGATION0(MaxIntCstExprTest, SetMaxFail, s, false) {
  IntVar* var = s->MakeIntVar(0, 20);
  IntExpr* expr = s->MakeMax(var, 10);
  expr->SetMax(9);
  return nullptr;
}

TEST_F(MaxIntCstExprTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr_->WhenRange(d);
}

// ----- SimpleConvexPiecewiseExprTest -----

class SimpleConvexPiecewiseExprTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("SimpleConvexPiecewiseExprTest");
    var_ = solver_->MakeIntVar(-10, 10, "var");
    expr_ = solver_->MakeConvexPiecewiseExpr(var_, 5, -2, 2, 2);
  }

  std::unique_ptr<Solver> solver_;
  IntVar* var_;
  IntExpr* expr_;
};

TEST_F(SimpleConvexPiecewiseExprTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
}

TEST_F(SimpleConvexPiecewiseExprTest, Getters) {
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(40, expr_->Max());
}

TEST_F(SimpleConvexPiecewiseExprTest, ExprSetMin) {
  expr_->SetMin(-2);
  EXPECT_EQ(-10, var_->Min());
  EXPECT_EQ(10, var_->Max());
  expr_->SetMin(7);
  EXPECT_EQ(-10, var_->Min());
  EXPECT_EQ(10, var_->Max());
  EXPECT_FALSE(var_->Contains(-3));
  EXPECT_TRUE(var_->Contains(-4));
  EXPECT_FALSE(var_->Contains(5));
  EXPECT_TRUE(var_->Contains(6));
}

TEST_F(SimpleConvexPiecewiseExprTest, ExprSetMax) {
  expr_->SetMax(7);
  EXPECT_EQ(-3, var_->Min());
  EXPECT_EQ(5, var_->Max());
}

TEST_F(SimpleConvexPiecewiseExprTest, ExprSetValueRight) {
  expr_->SetValue(6);
  EXPECT_EQ(5, var_->Value());
}

TEST_F(SimpleConvexPiecewiseExprTest, ExprSetValueLeft) {
  expr_->SetValue(15);
  EXPECT_EQ(-5, var_->Value());
}

TEST_F(SimpleConvexPiecewiseExprTest, VarSetMin) {
  var_->SetMin(0);
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(16, expr_->Max());
  var_->SetMin(4);
  EXPECT_EQ(4, expr_->Min());
  EXPECT_EQ(16, expr_->Max());
}

TEST_F(SimpleConvexPiecewiseExprTest, VarSetMax) {
  var_->SetMax(0);
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(40, expr_->Max());
  var_->SetMax(-4);
  EXPECT_EQ(10, expr_->Min());
  EXPECT_EQ(40, expr_->Max());
}

TEST_F(SimpleConvexPiecewiseExprTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr_->WhenRange(d);
}

// ----- SemiContinuousExprTest -----

class SemiContinuousExprTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("SemiContinuousExprTest");
    var_ = solver_->MakeIntVar(-10, 10, "var");
    expr_ = solver_->MakeSemiContinuousExpr(var_, 10, 3);
    open_var_ = solver_->MakeIntVar(kint64min, kint64max, "var");
    open_expr_ = solver_->MakeSemiContinuousExpr(open_var_, 10, 3);
    infinite_expr1_ = solver_->MakeSemiContinuousExpr(var_, kint64max, 3);
    infinite_expr2_ = solver_->MakeSemiContinuousExpr(var_, 10, kint64max);
    infinite_expr3_ = solver_->MakeSemiContinuousExpr(open_var_, kint64max, 3);
    infinite_expr4_ = solver_->MakeSemiContinuousExpr(open_var_, 10, kint64max);
  }
  std::unique_ptr<Solver> solver_;
  IntVar* var_;
  IntExpr* expr_;
  IntVar* open_var_;
  IntExpr* open_expr_;
  IntExpr* infinite_expr1_;
  IntExpr* infinite_expr2_;
  IntExpr* infinite_expr3_;
  IntExpr* infinite_expr4_;
};

TEST_F(SemiContinuousExprTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
  EXPECT_FALSE(open_expr_->DebugString().empty());
  EXPECT_FALSE(infinite_expr1_->DebugString().empty());
  EXPECT_FALSE(infinite_expr2_->DebugString().empty());
  EXPECT_FALSE(infinite_expr3_->DebugString().empty());
  EXPECT_FALSE(infinite_expr4_->DebugString().empty());
}

TEST_F(SemiContinuousExprTest, Getters) {
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(40, expr_->Max());
  EXPECT_EQ(0, open_expr_->Min());
  EXPECT_EQ(kint64max, open_expr_->Max());
  EXPECT_EQ(0, infinite_expr1_->Min());
  EXPECT_EQ(kint64max, infinite_expr1_->Max());
  EXPECT_EQ(0, infinite_expr2_->Min());
  EXPECT_EQ(kint64max, infinite_expr2_->Max());
  EXPECT_EQ(0, infinite_expr3_->Min());
  EXPECT_EQ(kint64max, infinite_expr3_->Max());
  EXPECT_EQ(0, infinite_expr4_->Min());
  EXPECT_EQ(kint64max, infinite_expr4_->Max());
}

TEST_F(SemiContinuousExprTest, ExprSetMin) {
  expr_->SetMin(-2);
  EXPECT_EQ(-10, var_->Min());
  EXPECT_EQ(10, var_->Max());
  expr_->SetMin(7);
  EXPECT_EQ(1, var_->Min());
  expr_->SetMin(14);
  EXPECT_EQ(2, var_->Min());
  open_expr_->SetMin(-2);
  EXPECT_EQ(kint64min, open_var_->Min());
  EXPECT_EQ(kint64max, open_var_->Max());
  open_expr_->SetMin(7);
  EXPECT_EQ(1, open_var_->Min());
  open_expr_->SetMin(14);
  EXPECT_EQ(2, open_var_->Min());
}

TEST_F(SemiContinuousExprTest, ExprSetMax) {
  expr_->SetMax(14);
  EXPECT_EQ(-10, var_->Min());
  EXPECT_EQ(1, var_->Max());
  expr_->SetMax(7);
  EXPECT_EQ(-10, var_->Min());
  EXPECT_EQ(0, var_->Max());
  expr_->SetMax(0);
  EXPECT_EQ(-10, var_->Min());
  EXPECT_EQ(0, var_->Max());
  open_expr_->SetMax(14);
  EXPECT_EQ(kint64min, open_var_->Min());
  EXPECT_EQ(1, open_var_->Max());
  open_expr_->SetMax(7);
  EXPECT_EQ(kint64min, open_var_->Min());
  EXPECT_EQ(0, open_var_->Max());
  expr_->SetMax(0);
  EXPECT_EQ(kint64min, open_var_->Min());
  EXPECT_EQ(0, open_var_->Max());
}

TEST_F(SemiContinuousExprTest, VarSetMin) {
  var_->SetMin(0);
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(40, expr_->Max());
  EXPECT_EQ(0, infinite_expr1_->Min());
  EXPECT_EQ(kint64max, infinite_expr1_->Max());
  EXPECT_EQ(0, infinite_expr2_->Min());
  EXPECT_EQ(kint64max, infinite_expr2_->Max());
  var_->SetMin(4);
  EXPECT_EQ(22, expr_->Min());
  EXPECT_EQ(40, expr_->Max());
  EXPECT_EQ(kint64max, infinite_expr1_->Min());
  EXPECT_EQ(kint64max, infinite_expr1_->Max());
  EXPECT_EQ(kint64max, infinite_expr2_->Min());
  EXPECT_EQ(kint64max, infinite_expr2_->Max());
  open_var_->SetMin(0);
  EXPECT_EQ(0, open_expr_->Min());
  EXPECT_EQ(kint64max, open_expr_->Max());
  EXPECT_EQ(0, infinite_expr3_->Min());
  EXPECT_EQ(kint64max, infinite_expr3_->Max());
  EXPECT_EQ(0, infinite_expr4_->Min());
  EXPECT_EQ(kint64max, infinite_expr4_->Max());
  open_var_->SetMin(4);
  EXPECT_EQ(22, open_expr_->Min());
  EXPECT_EQ(kint64max, open_expr_->Max());
  EXPECT_EQ(kint64max, infinite_expr3_->Min());
  EXPECT_EQ(kint64max, infinite_expr3_->Max());
  EXPECT_EQ(kint64max, infinite_expr4_->Min());
  EXPECT_EQ(kint64max, infinite_expr4_->Max());
}

TEST_F(SemiContinuousExprTest, VarSetMax) {
  var_->SetMax(4);
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(22, expr_->Max());
  EXPECT_EQ(0, infinite_expr1_->Min());
  EXPECT_EQ(kint64max, infinite_expr1_->Max());
  EXPECT_EQ(0, infinite_expr2_->Min());
  EXPECT_EQ(kint64max, infinite_expr2_->Max());
  var_->SetMax(0);
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(0, expr_->Max());
  EXPECT_EQ(0, infinite_expr1_->Min());
  EXPECT_EQ(0, infinite_expr1_->Max());
  EXPECT_EQ(0, infinite_expr2_->Min());
  EXPECT_EQ(0, infinite_expr2_->Max());
  var_->SetMax(-4);
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(0, expr_->Max());
  EXPECT_EQ(0, infinite_expr1_->Min());
  EXPECT_EQ(0, infinite_expr1_->Max());
  EXPECT_EQ(0, infinite_expr2_->Min());
  EXPECT_EQ(0, infinite_expr2_->Max());
  open_var_->SetMax(4);
  EXPECT_EQ(0, open_expr_->Min());
  EXPECT_EQ(22, open_expr_->Max());
  EXPECT_EQ(0, infinite_expr3_->Min());
  EXPECT_EQ(kint64max, infinite_expr3_->Max());
  EXPECT_EQ(0, infinite_expr4_->Min());
  EXPECT_EQ(kint64max, infinite_expr4_->Max());
  open_var_->SetMax(0);
  EXPECT_EQ(0, open_expr_->Min());
  EXPECT_EQ(0, open_expr_->Max());
  EXPECT_EQ(0, infinite_expr3_->Min());
  EXPECT_EQ(0, infinite_expr3_->Max());
  EXPECT_EQ(0, infinite_expr4_->Min());
  EXPECT_EQ(0, infinite_expr4_->Max());
  open_var_->SetMax(-4);
  EXPECT_EQ(0, open_expr_->Min());
  EXPECT_EQ(0, open_expr_->Max());
  EXPECT_EQ(0, infinite_expr3_->Min());
  EXPECT_EQ(0, infinite_expr3_->Max());
  EXPECT_EQ(0, infinite_expr4_->Min());
  EXPECT_EQ(0, infinite_expr4_->Max());
}

TEST_F(SemiContinuousExprTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr_->WhenRange(d);
}

TEST_F(SemiContinuousExprTest, NullFunction) {
  IntExpr* const expr =
      solver_->MakeSemiContinuousExpr(var_, int64_t{0}, int64_t{0});
  EXPECT_EQ(expr, solver_->MakeIntConst(int64_t{0}));
}

// ----- SemiContinuousStepOneExprTest -----

class SemiContinuousStepOneExprTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("SemiContinuousStepOneExprTest");
    var_ = solver_->MakeIntVar(-10, 10, "var");
    expr_ = solver_->MakeSemiContinuousExpr(var_, 10, 1);
  }

  std::unique_ptr<Solver> solver_;
  IntVar* var_;
  IntExpr* expr_;
};

TEST_F(SemiContinuousStepOneExprTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
}

TEST_F(SemiContinuousStepOneExprTest, Getters) {
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(20, expr_->Max());
}

TEST_F(SemiContinuousStepOneExprTest, ExprSetMin) {
  expr_->SetMin(-2);
  EXPECT_EQ(-10, var_->Min());
  EXPECT_EQ(10, var_->Max());
  expr_->SetMin(7);
  EXPECT_EQ(1, var_->Min());
  expr_->SetMin(14);
  EXPECT_EQ(4, var_->Min());
}

TEST_F(SemiContinuousStepOneExprTest, ExprSetMax) {
  expr_->SetMax(14);
  EXPECT_EQ(-10, var_->Min());
  EXPECT_EQ(4, var_->Max());
  expr_->SetMax(7);
  EXPECT_EQ(-10, var_->Min());
  EXPECT_EQ(0, var_->Max());
  expr_->SetMax(0);
  EXPECT_EQ(-10, var_->Min());
  EXPECT_EQ(0, var_->Max());
}

TEST_F(SemiContinuousStepOneExprTest, VarSetMin) {
  var_->SetMin(0);
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(20, expr_->Max());
  var_->SetMin(4);
  EXPECT_EQ(14, expr_->Min());
  EXPECT_EQ(20, expr_->Max());
}

TEST_F(SemiContinuousStepOneExprTest, VarSetMax) {
  var_->SetMax(4);
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(14, expr_->Max());
  var_->SetMax(0);
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(0, expr_->Max());
  var_->SetMax(-4);
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(0, expr_->Max());
}

TEST_F(SemiContinuousStepOneExprTest, NullFixedCharge) {
  IntExpr* const expr =
      solver_->MakeSemiContinuousExpr(var_, int64_t{0}, int64_t{1});
  var_->SetMax(4);
  EXPECT_EQ(0, expr->Min());
  EXPECT_EQ(4, expr->Max());
  expr->SetMax(3);
  EXPECT_EQ(-10, var_->Min());
  EXPECT_EQ(3, var_->Max());
  var_->SetMax(1);
  EXPECT_EQ(0, expr->Min());
  EXPECT_EQ(1, expr->Max());
  expr->SetMax(0);
  EXPECT_EQ(-10, var_->Min());
  EXPECT_EQ(0, var_->Max());
  var_->SetMax(-2);
  EXPECT_EQ(0, expr->Min());
  EXPECT_EQ(0, expr->Max());
}

TEST_F(SemiContinuousStepOneExprTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr_->WhenRange(d);
}

// ----- SemiContinuousStepOneExprTest -----

class SemiContinuousStepZeroExprTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("SemiContinuousStepZeroExprTest");
    var_ = solver_->MakeIntVar(-10, 10, "var");
    expr_ = solver_->MakeSemiContinuousExpr(var_, 10, 0);
  }

  std::unique_ptr<Solver> solver_;
  IntVar* var_;
  IntExpr* expr_;
};

TEST_F(SemiContinuousStepZeroExprTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
}

TEST_F(SemiContinuousStepZeroExprTest, Getters) {
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(10, expr_->Max());
}

TEST_F(SemiContinuousStepZeroExprTest, ExprSetMin) {
  expr_->SetMin(-2);
  EXPECT_EQ(-10, var_->Min());
  EXPECT_EQ(10, var_->Max());
  expr_->SetMin(7);
  EXPECT_EQ(1, var_->Min());
}

TEST_F(SemiContinuousStepZeroExprTest, ExprSetMax) {
  expr_->SetMax(14);
  EXPECT_EQ(-10, var_->Min());
  EXPECT_EQ(10, var_->Max());
  expr_->SetMax(7);
  EXPECT_EQ(-10, var_->Min());
  EXPECT_EQ(0, var_->Max());
  expr_->SetMax(0);
  EXPECT_EQ(-10, var_->Min());
  EXPECT_EQ(0, var_->Max());
}

TEST_F(SemiContinuousStepZeroExprTest, VarSetMin) {
  var_->SetMin(0);
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(10, expr_->Max());
  var_->SetMin(4);
  EXPECT_EQ(10, expr_->Min());
  EXPECT_EQ(10, expr_->Max());
}

TEST_F(SemiContinuousStepZeroExprTest, VarSetMax) {
  var_->SetMax(4);
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(10, expr_->Max());
  var_->SetMax(0);
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(0, expr_->Max());
  var_->SetMax(-4);
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(0, expr_->Max());
}

TEST_F(SemiContinuousStepZeroExprTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr_->WhenRange(d);
}

// ----- PiecewiseLinearExprTest -----

class PiecewiseLinearExprTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("PiecewiseLinearExprTest");
    var_ = solver_->MakeIntVar(-10, 10, "var");
    f_.reset(PiecewiseLinearFunction::CreateFullDomainFunction(0, {-2, 2},
                                                               {-5, 0, 2}));
    expr_ = solver_->MakePiecewiseLinearExpr(var_, *f_);
  }

  std::unique_ptr<Solver> solver_;
  IntVar* var_;
  IntExpr* expr_;
  std::unique_ptr<PiecewiseLinearFunction> f_;
};

TEST_F(PiecewiseLinearExprTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
}

TEST_F(PiecewiseLinearExprTest, Getters) {
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(40, expr_->Max());
}

TEST_F(PiecewiseLinearExprTest, ExprSetMin) {
  expr_->SetMin(-2);
  EXPECT_EQ(-10, var_->Min());
  EXPECT_EQ(10, var_->Max());
  expr_->SetMin(7);
  EXPECT_EQ(-10, var_->Min());
  EXPECT_EQ(10, var_->Max());
}

TEST_F(PiecewiseLinearExprTest, ExprSetMax) {
  expr_->SetMax(7);
  EXPECT_EQ(-3, var_->Min());
  EXPECT_EQ(5, var_->Max());
}

TEST_F(PiecewiseLinearExprTest, ExprSetValueRight) {
  expr_->SetValue(6);
  EXPECT_EQ(5, var_->Value());
}

TEST_F(PiecewiseLinearExprTest, ExprSetValueLeft) {
  expr_->SetValue(15);
  EXPECT_EQ(-5, var_->Value());
}

TEST_F(PiecewiseLinearExprTest, VarSetMin) {
  var_->SetMin(0);
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(16, expr_->Max());
  var_->SetMin(4);
  EXPECT_EQ(4, expr_->Min());
  EXPECT_EQ(16, expr_->Max());
}

TEST_F(PiecewiseLinearExprTest, VarSetMax) {
  var_->SetMax(0);
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(40, expr_->Max());
  var_->SetMax(-4);
  EXPECT_EQ(10, expr_->Min());
  EXPECT_EQ(40, expr_->Max());
}

TEST_F(PiecewiseLinearExprTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr_->WhenRange(d);
}

// ----- Conditional Expression -----

TEST(ConditionalExprTest, BoundCondition) {
  Solver s("ConditionalExprTest::BoundCondition");
  IntVar* const expression = s.MakeIntVar(0, 100);
  const int64_t kEscape = -10;
  IntExpr* const cond_true =
      s.MakeConditionalExpression(s.MakeIntConst(1), expression, kEscape);
  IntExpr* const cond_false =
      s.MakeConditionalExpression(s.MakeIntConst(0), expression, kEscape);
  EXPECT_EXPR_EQ_RANGE(cond_true, 0, 100);
  EXPECT_EXPR_EQ_RANGE(cond_false, kEscape, kEscape);
}

TEST_IN_PROPAGATION(ConditionalExprTest, ChangeExpression, s, true) {
  IntVar* const condition = s->MakeBoolVar();
  IntVar* const expression = s->MakeIntVar(0, 100);
  const int64_t kEscape = -10;
  IntExpr* const cond_expr =
      s->MakeConditionalExpression(condition, expression, kEscape);
  EXPECT_EXPR_EQ_RANGE(cond_expr, -10, 100);
  expression->SetRange(20, 80);
  EXPECT_EXPR_EQ_RANGE(cond_expr, -10, 80);
  cond_expr->SetMin(0);
  EXPECT_BOUND_TO(condition, 1);
  EXPECT_EXPR_EQ_RANGE(cond_expr, 20, 80);
  return nullptr;
}

TEST_IN_PROPAGATION(ConditionalExprTest, ChangeConditionToTrue, s, true) {
  IntVar* const condition = s->MakeBoolVar();
  IntVar* const expression = s->MakeIntVar(0, 100);
  const int64_t kEscape = -10;
  IntExpr* const cond_expr =
      s->MakeConditionalExpression(condition, expression, kEscape);
  EXPECT_EXPR_EQ_RANGE(cond_expr, -10, 100);
  condition->SetValue(1);
  EXPECT_EXPR_EQ_RANGE(cond_expr, 0, 100);
  return nullptr;
}

TEST_IN_PROPAGATION(ConditionalExprTest, ChangeConditionToFalse, s, true) {
  IntVar* const condition = s->MakeBoolVar();
  IntVar* const expression = s->MakeIntVar(0, 100);
  const int64_t kEscape = -10;
  IntExpr* const cond_expr =
      s->MakeConditionalExpression(condition, expression, kEscape);
  EXPECT_EXPR_EQ_RANGE(cond_expr, -10, 100);
  condition->SetValue(0);
  EXPECT_EXPR_EQ_RANGE(cond_expr, -10, -10);
  return nullptr;
}

TEST_IN_PROPAGATION(ConditionalExprTest, ForceConditionToTrue, s, true) {
  IntVar* const condition = s->MakeBoolVar();
  IntVar* const expression = s->MakeIntVar(0, 100);
  const int64_t kEscape = -10;
  IntExpr* const cond_expr =
      s->MakeConditionalExpression(condition, expression, kEscape);
  EXPECT_EXPR_EQ_RANGE(cond_expr, -10, 100);
  cond_expr->SetRange(20, 80);
  EXPECT_BOUND_TO(condition, 1);
  EXPECT_EXPR_EQ_RANGE(expression, 20, 80);
  return nullptr;
}

TEST_IN_PROPAGATION(ConditionalExprTest, ForceConditionToFalse, s, true) {
  IntVar* const condition = s->MakeBoolVar();
  IntVar* const expression = s->MakeIntVar(0, 100);
  const int64_t kEscape = -10;
  IntExpr* const cond_expr =
      s->MakeConditionalExpression(condition, expression, kEscape);
  EXPECT_EXPR_EQ_RANGE(cond_expr, -10, 100);
  cond_expr->SetRange(-30, -5);
  EXPECT_BOUND_TO(condition, 0);
  return nullptr;
}

// ----- Modulo -----

int64_t PythonMod(int64_t x, int64_t mod) { return x - (x / mod) * mod; }

int CountCp(int64_t xmin, int64_t xmax, int64_t modmin, int64_t modmax,
            int64_t ymin, int64_t ymax) {
  Solver solver("TestCP");
  IntVar* const x = solver.MakeIntVar(xmin, xmax, "x");
  IntVar* const mod = solver.MakeIntVar(modmin, modmax, "mod");
  IntVar* const y = solver.MakeModulo(x, mod)->Var();
  y->SetRange(ymin, ymax);
  int counter = 0;
  solver.NewSearch(solver.MakePhase(x, mod, Solver::CHOOSE_FIRST_UNBOUND,
                                    Solver::ASSIGN_MIN_VALUE));
  while (solver.NextSolution()) {
    EXPECT_EQ(PythonMod(x->Value(), mod->Value()), y->Value())
        << x->Value() << " % " << mod->Value() << " == " << y->Value();
    counter++;
  }
  solver.EndSearch();
  return counter;
}

int CountBruteForce(int64_t xmin, int64_t xmax, int64_t modmin, int64_t modmax,
                    int64_t ymin, int64_t ymax) {
  int counter = 0;
  for (int x = xmin; x <= xmax; ++x) {
    for (int mod = modmin; mod <= modmax; ++mod) {
      if (mod != 0) {
        for (int y = ymin; y <= ymax; ++y) {
          counter += (PythonMod(x, mod) == y);
        }
      }
    }
  }
  return counter;
}

TEST(ModuloTest, PositivePositivePositive) {
  EXPECT_EQ(CountCp(0, 10, 0, 10, 0, 10), CountBruteForce(0, 10, 0, 10, 0, 10));
}

TEST(ModuloTest, PositiveBoundNull) {
  EXPECT_EQ(CountCp(0, 200, 7, 7, 0, 0), CountBruteForce(0, 200, 7, 7, 0, 0));
}

TEST(ModuloTest, PositivePositiveNull) {
  EXPECT_EQ(CountCp(0, 200, 0, 20, 0, 0), CountBruteForce(0, 200, 0, 20, 0, 0));
}

TEST(ModuloTest, GeneralPositiveNull) {
  EXPECT_EQ(CountCp(-50, 50, 0, 20, 0, 0),
            CountBruteForce(-50, 50, 0, 20, 0, 0));
}

TEST(ModuloTest, GeneralGeneralNull) {
  EXPECT_EQ(CountCp(-50, 50, -12, 12, 0, 0),
            CountBruteForce(-50, 50, -12, 12, 0, 0));
}

TEST(ModuloTest, GeneralNegativeNull) {
  EXPECT_EQ(CountCp(-50, 50, -12, -1, 0, 0),
            CountBruteForce(-50, 50, -12, -1, 0, 0));
}

TEST(ModuloTest, PositiveBoundBound) {
  EXPECT_EQ(CountCp(0, 200, 7, 7, 3, 3), CountBruteForce(0, 200, 7, 7, 3, 3));
}

TEST(ModuloTest, PositiveBoundPositive) {
  EXPECT_EQ(CountCp(0, 200, 7, 7, 0, 10), CountBruteForce(0, 200, 7, 7, 0, 10));
}

TEST(ModuloTest, PositivePositiveBound) {
  EXPECT_EQ(CountCp(0, 200, 0, 20, 5, 5), CountBruteForce(0, 200, 0, 20, 5, 5));
}

TEST(ModuloTest, GeneralPositiveBound) {
  EXPECT_EQ(CountCp(-50, 50, 0, 20, 5, 5),
            CountBruteForce(-50, 50, 0, 20, 5, 5));
}

TEST(ModuloTest, GeneralGeneralBound) {
  EXPECT_EQ(CountCp(-50, 50, -12, 12, 5, 5),
            CountBruteForce(-50, 50, -12, 12, 5, 5));
}

TEST(ModuloTest, GeneralNegativeBound) {
  EXPECT_EQ(CountCp(-50, 50, -12, -1, 5, 5),
            CountBruteForce(-50, 50, -12, -1, 5, 5));
}

TEST(ModuloTest, GeneralGeneralGeneral) {
  EXPECT_EQ(CountCp(-50, 50, -12, 12, -8, 8),
            CountBruteForce(-50, 50, -12, 12, -8, 8));
}

// Variable indexing.

TEST(IntVarIndexingTest, Indexing) {
  // Testing with various bounds to cover IntConst, BoolVar and IntDomainVar.
  const int kBounds[][2] = {{1, 3}, {1, 2}, {1, 1}, {0, 1},
                            {1, 1}, {1, 3}, {0, 0}};
  const int kExpectedIndices[] = {17,
                                  /* two vars created */ 19,
                                  /* cached constant created internally */ 9,
                                  20,
                                  /* cached constant created internally */ 9,
                                  21,
                                  /* cached constant created internally */ 8};
  Solver solver("");
  std::vector<IntVar*> vars;
  for (int i = 0; i < std::size(kBounds); ++i) {
    vars.push_back(solver.MakeIntVar(kBounds[i][0], kBounds[i][1]));
  }
  for (int i = 0; i < std::size(kBounds); ++i) {
    EXPECT_EQ(kExpectedIndices[i], vars[i]->index());
  }
}

// Overflow tests

TEST(OverflowIntVarTest, LargeSize) {
  Solver solver("");
  IntVar* var1 = solver.MakeIntVar(1, kint64max);
  EXPECT_EQ(kint64max, var1->Size());
  IntVar* var2 = solver.MakeIntVar(0, kint64max);
  EXPECT_EQ(static_cast<uint64_t>(kint64max) + uint64_t{1}, var2->Size());
  IntVar* var3 = solver.MakeIntVar(kint64min, -1);
  EXPECT_EQ(kint64min, var3->Size());
  IntVar* var4 = solver.MakeIntVar(kint64min, 0);
  EXPECT_EQ(static_cast<uint64_t>(kint64min) + uint64_t{1}, var4->Size());
  IntVar* var5 = solver.MakeIntVar(-kint64max, kint64max);
  EXPECT_EQ(kuint64max, var5->Size());
}

}  // namespace
}  // namespace operations_research
