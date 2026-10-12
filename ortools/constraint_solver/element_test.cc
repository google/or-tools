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
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_format.h"
#include "gtest/gtest.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solver_test_util.h"

ABSL_DECLARE_FLAG(bool, cp_disable_expression_optimization);

namespace {
class IntElementConstraintTest_TestMinMaxGetters_Test;
}
namespace {
class IntElementConstraintTest_TestOutOfBound_Test;
}
namespace {
class IntElementConstraintTest_TestOverflow_Test;
}
namespace {
class IntElementConstraintTest_TestSetIndex_Test;
}
namespace {
class IntElementConstraintTest_TestSetMax_Test;
}
namespace {
class IntElementConstraintTest_TestSetMin_Test;
}
namespace {
class IntElementConstraintTest_TestSetRange_Test;
}
namespace {
class IntExprElementInt64TestTestOverflow;
}
namespace {
class IntExprElementIntTestTestOverflow;
}

namespace operations_research {
template <class T>
class SimpleRevFIFO;
}  // namespace operations_research

using operations_research::Decision;
using operations_research::DecisionBuilder;
using operations_research::Demon;
using operations_research::IntExpr;
using operations_research::IntVar;
using operations_research::Solver;

namespace {

class NullDemon : public Demon {
  void Run(Solver* const s) override {}
};

// ----- IntExprElement -----

template <class T>
class IntExprElementTest : public ::testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("IntExprElementTest");
    values_.resize(3);
    values_[0] = 3;
    values_[1] = 1;
    values_[2] = 2;
    index_ = solver_->MakeIntVar(0, 2);
    expr_ = solver_->MakeElement(values_, index_);
  }

  std::unique_ptr<Solver> solver_;
  std::vector<T> values_;
  IntVar* index_;
  IntExpr* expr_;
};

TEST_IN_PROPAGATION(IntExprElementInt64Test, Overflow, s, false) {
  absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
  IntVar* const index = s->MakeIntVar(0, 2);
  IntExpr* expr;
  {
    std::vector<int64_t> values = {3, 1, 2};
    expr = s->MakeElement(values, index);
  }
  index->SetValue(2);
  expr->SetMax(1);
  return nullptr;
}

TEST_IN_PROPAGATION(IntExprElementIntTest, Overflow, s, false) {
  absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
  IntVar* const index = s->MakeIntVar(0, 2);
  IntExpr* expr;
  {
    std::vector<int> values = {3, 1, 2};
    expr = s->MakeElement(values, index);
  }
  index->SetValue(2);
  expr->SetMax(1);
  return nullptr;
}

typedef testing::Types<int, int64_t> IntegerTypes;
TYPED_TEST_SUITE(IntExprElementTest, IntegerTypes);

TYPED_TEST(IntExprElementTest, DebugString) {
  EXPECT_FALSE(this->expr_->DebugString().empty());
}

TYPED_TEST(IntExprElementTest, MinMaxGetters) {
  EXPECT_EQ(1, this->expr_->Min());
  EXPECT_EQ(3, this->expr_->Max());
}

TYPED_TEST(IntExprElementTest, SetMax) {
  this->expr_->SetMax(2);
  EXPECT_EQ(1, this->expr_->Min());
  EXPECT_EQ(2, this->expr_->Max());
  EXPECT_EQ(1, this->index_->Min());
  EXPECT_EQ(2, this->index_->Max());
  this->expr_->SetMax(1);
  EXPECT_EQ(1, this->expr_->Min());
  EXPECT_EQ(1, this->expr_->Max());
  EXPECT_EQ(1, this->index_->Min());
  EXPECT_EQ(1, this->index_->Max());
}

TYPED_TEST(IntExprElementTest, SetMin) {
  this->expr_->SetMin(2);
  EXPECT_EQ(1, this->expr_->Min());
  EXPECT_EQ(3, this->expr_->Max());
  EXPECT_EQ(0, this->index_->Min());
  EXPECT_EQ(2, this->index_->Max());
  this->expr_->SetMin(3);
  EXPECT_EQ(3, this->expr_->Min());
  EXPECT_EQ(3, this->expr_->Max());
  EXPECT_EQ(0, this->index_->Min());
  EXPECT_EQ(0, this->index_->Max());
}

TYPED_TEST(IntExprElementTest, SetRange) {
  this->expr_->SetRange(1, 2);
  EXPECT_EQ(1, this->expr_->Min());
  EXPECT_EQ(2, this->expr_->Max());
  EXPECT_EQ(1, this->index_->Min());
  EXPECT_EQ(2, this->index_->Max());
}

TYPED_TEST(IntExprElementTest, SetIndex) {
  this->index_->SetMax(1);
  EXPECT_EQ(1, this->expr_->Min());
  EXPECT_EQ(3, this->expr_->Max());
  this->index_->SetMin(1);
  EXPECT_EQ(1, this->expr_->Min());
  EXPECT_EQ(1, this->expr_->Max());
}

TYPED_TEST(IntExprElementTest, OutOfBound) {
  IntVar* const v = this->solver_->MakeIntVar(-1, 3, "v");
  IntExpr* const elem = this->solver_->MakeElement(this->values_, v);
  EXPECT_EQ(1, elem->Min());
  EXPECT_EQ(3, elem->Max());
  EXPECT_EQ(-1, v->Min());
  EXPECT_EQ(3, v->Max());
  elem->SetMax(2);
  EXPECT_EQ(1, v->Min());
  EXPECT_EQ(2, v->Max());
}

TYPED_TEST(IntExprElementTest, When) {
  Demon* const d = this->solver_->RevAlloc(new NullDemon());
  this->expr_->WhenRange(d);
}

class IntElementConstraintTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("IntExprElement");
    values_ = {7, -1, 2};
    element_index_ = solver_->MakeIntVar(0, 2);
    element_var_ = solver_->MakeElement(values_, element_index_)->Var();
  }

  std::unique_ptr<Solver> solver_;
  std::vector<int64_t> values_;
  IntVar* element_index_;
  IntVar* element_var_;
};

TEST_F(IntElementConstraintTest, DebugString) {
  EXPECT_FALSE(element_var_->DebugString().empty());
}

TEST_IN_PROPAGATION2(IntElementConstraintTest, Overflow, s, false, IntVar*,
                     element_index_, IntVar*, element_var_) {
  element_index_->SetValue(2);
  element_var_->SetMax(1);
  return nullptr;
}

TEST_IN_PROPAGATION2(IntElementConstraintTest, MinMaxGetters, s, true, IntVar*,
                     element_index_, IntVar*, element_var_) {
  EXPECT_EQ(-1, element_var_->Min());
  EXPECT_EQ(7, element_var_->Max());
  return nullptr;
}

TEST_IN_PROPAGATION2(IntElementConstraintTest, SetMax, s, true, IntVar*,
                     element_index_, IntVar*, element_var_) {
  element_var_->SetMax(2);
  EXPECT_EQ(-1, element_var_->Min());
  EXPECT_EQ(2, element_var_->Max());
  EXPECT_EQ(1, element_index_->Min());
  EXPECT_EQ(2, element_index_->Max());
  element_var_->SetMax(1);
  EXPECT_EQ(-1, element_var_->Min());
  EXPECT_EQ(-1, element_var_->Max());
  EXPECT_EQ(1, element_index_->Min());
  EXPECT_EQ(1, element_index_->Max());
  return nullptr;
}

TEST_IN_PROPAGATION2(IntElementConstraintTest, SetMin, s, true, IntVar*,
                     element_index_, IntVar*, element_var_) {
  element_var_->SetMin(2);
  EXPECT_EQ(2, element_var_->Min());
  EXPECT_EQ(7, element_var_->Max());
  EXPECT_EQ(0, element_index_->Min());
  EXPECT_EQ(2, element_index_->Max());
  EXPECT_EQ(2, element_index_->Size());
  element_var_->SetMin(3);
  EXPECT_EQ(7, element_var_->Min());
  EXPECT_EQ(7, element_var_->Max());
  EXPECT_EQ(0, element_index_->Min());
  EXPECT_EQ(0, element_index_->Max());
  return nullptr;
}

TEST_IN_PROPAGATION2(IntElementConstraintTest, SetRange, s, true, IntVar*,
                     element_index_, IntVar*, element_var_) {
  element_var_->SetRange(-2, 3);
  EXPECT_EQ(-1, element_var_->Min());
  EXPECT_EQ(2, element_var_->Max());
  EXPECT_EQ(1, element_index_->Min());
  EXPECT_EQ(2, element_index_->Max());
  return nullptr;
}

TEST_IN_PROPAGATION2(IntElementConstraintTest, SetIndex, s, true, IntVar*,
                     element_index_, IntVar*, element_var_) {
  element_index_->SetMax(1);
  EXPECT_EQ(-1, element_var_->Min());
  EXPECT_EQ(7, element_var_->Max());
  element_index_->SetMin(1);
  EXPECT_EQ(-1, element_var_->Min());
  EXPECT_EQ(-1, element_var_->Max());
  return nullptr;
}

TEST_IN_PROPAGATION3(IntElementConstraintTest, OutOfBound, s, true, IntVar*,
                     element_index_, IntVar*, element_var_,
                     const std::vector<int64_t>&, values_) {
  IntVar* v = s->MakeIntVar(-1, 3, "v");
  IntVar* elem = s->MakeElement(values_, v)->Var();
  EXPECT_EQ(-1, elem->Min());
  EXPECT_EQ(7, elem->Max());
  EXPECT_EQ(0, v->Min());
  EXPECT_EQ(2, v->Max());
  elem->SetMax(2);
  EXPECT_EQ(1, v->Min());
  EXPECT_EQ(2, v->Max());
  return nullptr;
}

// ----- ConstantIntExprElement -----
class ConstantIntExprElementTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("ConstantIntExprElement");
    values_.resize(3);
    values_[0] = 2;
    values_[1] = 2;
    values_[2] = 2;
    index_ = solver_->MakeIntVar(0, 2);
    expr_ = solver_->MakeElement(values_, index_);
  }

  std::unique_ptr<Solver> solver_;
  std::vector<int64_t> values_;
  IntVar* index_;
  IntExpr* expr_;
};

TEST_F(ConstantIntExprElementTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
}

TEST_F(ConstantIntExprElementTest, MinMaxGetters) {
  EXPECT_EQ(2, expr_->Min());
  EXPECT_EQ(2, expr_->Max());
}

TEST_F(ConstantIntExprElementTest, SetIndex) {
  index_->SetMax(1);
  EXPECT_EQ(2, expr_->Min());
  EXPECT_EQ(2, expr_->Max());
  index_->SetMin(1);
  EXPECT_EQ(2, expr_->Min());
  EXPECT_EQ(2, expr_->Max());
}

TEST_IN_PROPAGATION2(ConstantIntExprElementTest, OutOfBound, s, true, IntVar*,
                     index_, IntExpr*, expr_) {
  EXPECT_EQ(2, expr_->Min());
  EXPECT_EQ(2, expr_->Max());
  EXPECT_EQ(0, index_->Min());
  EXPECT_EQ(2, index_->Max());
  expr_->SetMax(2);
  EXPECT_EQ(0, index_->Min());
  EXPECT_EQ(2, index_->Max());
  return nullptr;
};

TEST_F(ConstantIntExprElementTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr_->WhenRange(d);
}

// ----- IncreasingIntExprElementTest -----

class IncreasingIntExprElementTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("IncreasingIntExprElement");
    values_.resize(3);
    values_[0] = 1;
    values_[1] = 2;
    values_[2] = 4;
    index_ = solver_->MakeIntVar(0, 2);
    expr_ = solver_->MakeElement(values_, index_);
  }

  std::unique_ptr<Solver> solver_;
  std::vector<int64_t> values_;
  IntVar* index_;
  IntExpr* expr_;
};

TEST_F(IncreasingIntExprElementTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
}

TEST_F(IncreasingIntExprElementTest, MinMaxGetters) {
  EXPECT_EQ(1, expr_->Min());
  EXPECT_EQ(4, expr_->Max());
}

TEST_F(IncreasingIntExprElementTest, SetMax) {
  expr_->SetMax(2);
  EXPECT_EQ(1, expr_->Min());
  EXPECT_EQ(2, expr_->Max());
  EXPECT_EQ(0, index_->Min());
  EXPECT_EQ(1, index_->Max());
  expr_->SetMax(1);
  EXPECT_EQ(1, expr_->Min());
  EXPECT_EQ(1, expr_->Max());
  EXPECT_EQ(0, index_->Min());
  EXPECT_EQ(0, index_->Max());
}

TEST_F(IncreasingIntExprElementTest, SetMin) {
  expr_->SetMin(2);
  EXPECT_EQ(2, expr_->Min());
  EXPECT_EQ(4, expr_->Max());
  EXPECT_EQ(1, index_->Min());
  EXPECT_EQ(2, index_->Max());
  expr_->SetMin(3);
  EXPECT_EQ(4, expr_->Min());
  EXPECT_EQ(4, expr_->Max());
  EXPECT_EQ(2, index_->Min());
  EXPECT_EQ(2, index_->Max());
}

TEST_F(IncreasingIntExprElementTest, SetRange) {
  expr_->SetRange(1, 2);
  EXPECT_EQ(1, expr_->Min());
  EXPECT_EQ(2, expr_->Max());
  EXPECT_EQ(0, index_->Min());
  EXPECT_EQ(1, index_->Max());
}

TEST_F(IncreasingIntExprElementTest, SetIndex) {
  index_->SetMax(1);
  EXPECT_EQ(1, expr_->Min());
  EXPECT_EQ(2, expr_->Max());
  index_->SetMin(1);
  EXPECT_EQ(2, expr_->Min());
  EXPECT_EQ(2, expr_->Max());
}

TEST_F(IncreasingIntExprElementTest, OutOfBound) {
  IntVar* v = solver_->MakeIntVar(-1, 3, "v");
  IntExpr* elem = solver_->MakeElement(values_, v);
  EXPECT_EQ(1, elem->Min());
  EXPECT_EQ(4, elem->Max());
  EXPECT_EQ(-1, v->Min());
  EXPECT_EQ(3, v->Max());
  elem->SetMax(2);
  EXPECT_EQ(0, v->Min());
  EXPECT_EQ(1, v->Max());
}

TEST_F(IncreasingIntExprElementTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr_->WhenRange(d);
}

// ----- OneBoolElement -----

class OneBoolElementTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("OneBoolElement");
    values_.resize(3);
    values_[0] = 0;
    values_[1] = 1;
    values_[2] = 0;
    index_ = solver_->MakeIntVar(0, 2);
    expr_ = solver_->MakeElement(values_, index_);
  }

  std::unique_ptr<Solver> solver_;
  std::vector<int64_t> values_;
  IntVar* index_;
  IntExpr* expr_;
};

TEST_F(OneBoolElementTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
}

TEST_F(OneBoolElementTest, MinMaxGetters) {
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(1, expr_->Max());
}

TEST_IN_PROPAGATION2(OneBoolElementTest, SetMax, s, true, IntVar*, index_,
                     IntExpr*, expr_) {
  expr_->SetMax(0);
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(0, expr_->Max());
  EXPECT_EQ(2, index_->Size());
  EXPECT_FALSE(index_->Contains(1));
  return nullptr;
};

TEST_IN_PROPAGATION2(OneBoolElementTest, SetMin, s, true, IntVar*, index_,
                     IntExpr*, expr_) {
  expr_->SetMin(1);
  EXPECT_EQ(1, expr_->Min());
  EXPECT_EQ(1, expr_->Max());
  EXPECT_EQ(1, index_->Min());
  EXPECT_EQ(1, index_->Max());
  return nullptr;
};

TEST_IN_PROPAGATION2(OneBoolElementTest, SetIndex, s, true, IntVar*, index_,
                     IntExpr*, expr_) {
  index_->SetMax(1);
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(1, expr_->Max());
  index_->SetMin(1);
  EXPECT_EQ(1, expr_->Min());
  EXPECT_EQ(1, expr_->Max());
  return nullptr;
};

TEST_F(OneBoolElementTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr_->WhenRange(d);
}

// ----- ContinuousBoolElement -----

class ContinuousBoolElementTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("ContinuousBoolElement");
    values_.resize(5);
    values_[0] = 0;
    values_[1] = 1;
    values_[2] = 1;
    values_[3] = 0;
    values_[4] = 0;
    index_ = solver_->MakeIntVar(0, 5);
    expr_ = solver_->MakeElement(values_, index_);
  }

  std::unique_ptr<Solver> solver_;
  std::vector<int64_t> values_;
  IntVar* index_;
  IntExpr* expr_;
};

TEST_F(ContinuousBoolElementTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
}

TEST_F(ContinuousBoolElementTest, MinMaxGetters) {
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(1, expr_->Max());
}

TEST_IN_PROPAGATION2(ContinuousBoolElementTest, SetMax, s, true, IntVar*,
                     index_, IntExpr*, expr_) {
  expr_->SetMax(0);
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(0, expr_->Max());
  EXPECT_EQ(3, index_->Size());
  EXPECT_TRUE(index_->Contains(0));
  EXPECT_FALSE(index_->Contains(1));
  EXPECT_FALSE(index_->Contains(2));
  EXPECT_TRUE(index_->Contains(3));
  EXPECT_TRUE(index_->Contains(4));
  return nullptr;
};

TEST_IN_PROPAGATION2(ContinuousBoolElementTest, SetMin, s, true, IntVar*,
                     index_, IntExpr*, expr_) {
  expr_->SetMin(1);
  EXPECT_EQ(1, expr_->Min());
  EXPECT_EQ(1, expr_->Max());
  EXPECT_EQ(1, index_->Min());
  EXPECT_EQ(2, index_->Max());
  return nullptr;
};

TEST_IN_PROPAGATION2(ContinuousBoolElementTest, SetIndex, s, true, IntVar*,
                     index_, IntExpr*, expr_) {
  index_->SetMax(1);
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(1, expr_->Max());
  index_->SetMin(1);
  EXPECT_EQ(1, expr_->Min());
  EXPECT_EQ(1, expr_->Max());
  return nullptr;
};

TEST_F(ContinuousBoolElementTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr_->WhenRange(d);
}

// ----- IntExprFunctionElement -----

class IntExprFunctionElementTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("IntExprFunctionElement");
    values_.resize(3);
    values_[0] = 1;
    values_[1] = 2;
    values_[2] = 3;
    index_ = solver_->MakeIntVar(-1, 3);
    expr_ =
        solver_->MakeElement([this](int64_t i) { return Value(i); }, index_);
  }
  int64_t Value(int64_t i) const {
    if (i < 0) {
      return -10;
    } else if (i < 3) {
      return values_[i];
    } else {
      return 10;
    }
  }

  std::unique_ptr<Solver> solver_;
  std::vector<int64_t> values_;
  IntVar* index_;
  IntExpr* expr_;
};

TEST_F(IntExprFunctionElementTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
}

TEST_F(IntExprFunctionElementTest, MinMaxGetters) {
  EXPECT_EQ(-10, expr_->Min());
  EXPECT_EQ(10, expr_->Max());
}

TEST_F(IntExprFunctionElementTest, SetMax) {
  expr_->SetMax(2);
  EXPECT_EQ(-10, expr_->Min());
  EXPECT_EQ(2, expr_->Max());
  EXPECT_EQ(-1, index_->Min());
  EXPECT_EQ(1, index_->Max());
  expr_->SetMax(1);
  EXPECT_EQ(-10, expr_->Min());
  EXPECT_EQ(1, expr_->Max());
  EXPECT_EQ(-1, index_->Min());
  EXPECT_EQ(0, index_->Max());
  expr_->SetMax(0);
  EXPECT_EQ(-10, expr_->Min());
  EXPECT_EQ(-10, expr_->Max());
  EXPECT_EQ(-1, index_->Min());
  EXPECT_EQ(-1, index_->Max());
}

TEST_F(IntExprFunctionElementTest, SetMin) {
  expr_->SetMin(2);
  EXPECT_EQ(2, expr_->Min());
  EXPECT_EQ(10, expr_->Max());
  EXPECT_EQ(1, index_->Min());
  EXPECT_EQ(3, index_->Max());
  expr_->SetMin(3);
  EXPECT_EQ(3, expr_->Min());
  EXPECT_EQ(10, expr_->Max());
  EXPECT_EQ(2, index_->Min());
  EXPECT_EQ(3, index_->Max());
  expr_->SetMin(4);
  EXPECT_EQ(10, expr_->Min());
  EXPECT_EQ(10, expr_->Max());
  EXPECT_EQ(3, index_->Min());
  EXPECT_EQ(3, index_->Max());
}

TEST_F(IntExprFunctionElementTest, SetRange) {
  expr_->SetRange(1, 2);
  EXPECT_EQ(1, expr_->Min());
  EXPECT_EQ(2, expr_->Max());
  EXPECT_EQ(0, index_->Min());
  EXPECT_EQ(1, index_->Max());
}

TEST_F(IntExprFunctionElementTest, SetIndex) {
  index_->SetMax(1);
  EXPECT_EQ(-10, expr_->Min());
  EXPECT_EQ(2, expr_->Max());
  index_->SetMin(1);
  EXPECT_EQ(2, expr_->Min());
  EXPECT_EQ(2, expr_->Max());
}

TEST_F(IntExprFunctionElementTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr_->WhenRange(d);
}

// ----- IntExprFunctionElement -----

class IntExprIncreasingElementTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("IntExprIncreasingElement");
    values_.resize(3);
    values_[0] = 1;
    values_[1] = 2;
    values_[2] = 3;
    index_ = solver_->MakeIntVar(-1, 3);
    expr_ = solver_->MakeMonotonicElement(
        [this](int64_t index) { return Value(index); }, true, index_);
  }

  int64_t Value(int64_t i) const {
    if (i < 0) {
      return -10;
    } else if (i < 3) {
      return values_[i];
    } else {
      return 10;
    }
  }

  std::unique_ptr<Solver> solver_;
  std::vector<int64_t> values_;
  IntVar* index_;
  IntExpr* expr_;
};

TEST_F(IntExprIncreasingElementTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
}

TEST_F(IntExprIncreasingElementTest, MinMaxGetters) {
  EXPECT_EQ(-10, expr_->Min());
  EXPECT_EQ(10, expr_->Max());
}

TEST_F(IntExprIncreasingElementTest, SetMax) {
  expr_->SetMax(2);
  EXPECT_EQ(-10, expr_->Min());
  EXPECT_EQ(2, expr_->Max());
  EXPECT_EQ(-1, index_->Min());
  EXPECT_EQ(1, index_->Max());
  expr_->SetMax(1);
  EXPECT_EQ(-10, expr_->Min());
  EXPECT_EQ(1, expr_->Max());
  EXPECT_EQ(-1, index_->Min());
  EXPECT_EQ(0, index_->Max());
  expr_->SetMax(0);
  EXPECT_EQ(-10, expr_->Min());
  EXPECT_EQ(-10, expr_->Max());
  EXPECT_EQ(-1, index_->Min());
  EXPECT_EQ(-1, index_->Max());
}

TEST_F(IntExprIncreasingElementTest, SetMin) {
  expr_->SetMin(2);
  EXPECT_EQ(2, expr_->Min());
  EXPECT_EQ(10, expr_->Max());
  EXPECT_EQ(1, index_->Min());
  EXPECT_EQ(3, index_->Max());
  expr_->SetMin(3);
  EXPECT_EQ(3, expr_->Min());
  EXPECT_EQ(10, expr_->Max());
  EXPECT_EQ(2, index_->Min());
  EXPECT_EQ(3, index_->Max());
  expr_->SetMin(4);
  EXPECT_EQ(10, expr_->Min());
  EXPECT_EQ(10, expr_->Max());
  EXPECT_EQ(3, index_->Min());
  EXPECT_EQ(3, index_->Max());
}

TEST_F(IntExprIncreasingElementTest, SetRange) {
  expr_->SetRange(1, 2);
  EXPECT_EQ(1, expr_->Min());
  EXPECT_EQ(2, expr_->Max());
  EXPECT_EQ(0, index_->Min());
  EXPECT_EQ(1, index_->Max());
}

TEST_F(IntExprIncreasingElementTest, SetIndex) {
  index_->SetMax(1);
  EXPECT_EQ(-10, expr_->Min());
  EXPECT_EQ(2, expr_->Max());
  index_->SetMin(1);
  EXPECT_EQ(2, expr_->Min());
  EXPECT_EQ(2, expr_->Max());
}

TEST_F(IntExprIncreasingElementTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr_->WhenRange(d);
}

class IntExprDecreasingElementTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("IntExprDecreasingElement");
    values_.resize(3);
    values_[0] = -1;
    values_[1] = -2;
    values_[2] = -3;
    index_ = solver_->MakeIntVar(-1, 3);
    expr_ = solver_->MakeMonotonicElement(
        [this](int64_t index) { return Value(index); }, false, index_);
  }

  int64_t Value(int64_t i) const {
    if (i < 0) {
      return 10;
    } else if (i < 3) {
      return values_[i];
    } else {
      return -10;
    }
  }

  std::unique_ptr<Solver> solver_;
  std::vector<int64_t> values_;
  IntVar* index_;
  IntExpr* expr_;
};

TEST_F(IntExprDecreasingElementTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
}

TEST_F(IntExprDecreasingElementTest, MinMaxGetters) {
  EXPECT_EQ(-10, expr_->Min());
  EXPECT_EQ(10, expr_->Max());
}

TEST_F(IntExprDecreasingElementTest, SetMax) {
  expr_->SetMax(0);
  EXPECT_EQ(-10, expr_->Min());
  EXPECT_EQ(-1, expr_->Max());
  EXPECT_EQ(0, index_->Min());
  EXPECT_EQ(3, index_->Max());
  expr_->SetMax(1);
  EXPECT_EQ(-10, expr_->Min());
  EXPECT_EQ(-1, expr_->Max());
  EXPECT_EQ(0, index_->Min());
  EXPECT_EQ(3, index_->Max());
  expr_->SetMax(-2);
  EXPECT_EQ(-10, expr_->Min());
  EXPECT_EQ(-2, expr_->Max());
  EXPECT_EQ(1, index_->Min());
  EXPECT_EQ(3, index_->Max());
}

TEST_F(IntExprDecreasingElementTest, SetMin) {
  expr_->SetMin(-2);
  EXPECT_EQ(-2, expr_->Min());
  EXPECT_EQ(10, expr_->Max());
  EXPECT_EQ(-1, index_->Min());
  EXPECT_EQ(1, index_->Max());
  expr_->SetMin(-1);
  EXPECT_EQ(-1, expr_->Min());
  EXPECT_EQ(10, expr_->Max());
  EXPECT_EQ(-1, index_->Min());
  EXPECT_EQ(0, index_->Max());
  expr_->SetMin(1);
  EXPECT_EQ(10, expr_->Min());
  EXPECT_EQ(10, expr_->Max());
  EXPECT_EQ(-1, index_->Min());
  EXPECT_EQ(-1, index_->Max());
}

TEST_F(IntExprDecreasingElementTest, SetRange) {
  expr_->SetRange(-2, -1);
  EXPECT_EQ(-2, expr_->Min());
  EXPECT_EQ(-1, expr_->Max());
  EXPECT_EQ(0, index_->Min());
  EXPECT_EQ(1, index_->Max());
}

TEST_F(IntExprDecreasingElementTest, SetIndex) {
  index_->SetMax(1);
  EXPECT_EQ(-2, expr_->Min());
  EXPECT_EQ(10, expr_->Max());
  index_->SetMin(1);
  EXPECT_EQ(-2, expr_->Min());
  EXPECT_EQ(-2, expr_->Max());
}

TEST_F(IntExprDecreasingElementTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr_->WhenRange(d);
}

// ----- IntIntExprFunctionElement -----

const int kSize = 3;
const int64_t kTestMatrix[kSize][kSize] = {{1, 2, 3}, {1, 1, 2}, {3, 2, 3}};
const int64_t kUnderFlowValue = -10;
const int64_t kOverFlowValue = 10;
const int64_t kMin = -1;
const int64_t kMax = 3;

class IntIntExprFunctionElementTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("IntIntExprFunctionElement");
    index1_ = solver_->MakeIntVar(kMin, kMax);
    index2_ = solver_->MakeIntVar(kMin, kMax);
    expr_ = solver_->MakeElement(
        [this](int64_t i, int64_t j) { return Value(i, j); }, index1_, index2_);
  }
  int64_t Value(int64_t i, int64_t j) const {
    if (i < 0 || j < 0) {
      return kUnderFlowValue;
    } else if (i < kSize && j < kSize) {
      return kTestMatrix[i][j];
    } else {
      return kOverFlowValue;
    }
  }
  int64_t Index1Min(int64_t expr_min, int64_t expr_max, int64_t index1_min,
                    int64_t index1_max, int64_t index2_min,
                    int64_t index2_max) const {
    for (int i = index1_min; i <= index1_max; ++i) {
      for (int j = index2_min; j <= index2_max; ++j) {
        if (expr_min <= Value(i, j) && Value(i, j) <= expr_max) {
          return i;
        }
      }
    }
    return kint64max;
  }
  int64_t Index2Min(int64_t expr_min, int64_t expr_max, int64_t index1_min,
                    int64_t index1_max, int64_t index2_min,
                    int64_t index2_max) const {
    for (int i = index2_min; i <= index2_max; ++i) {
      for (int j = index1_min; j <= index1_max; ++j) {
        if (expr_min <= Value(j, i) && Value(j, i) <= expr_max) {
          return i;
        }
      }
    }
    return kint64max;
  }
  int64_t ExprMin(int64_t index1_min, int64_t index1_max, int64_t index2_min,
                  int64_t index2_max) const {
    int64_t expr_min = kint64max;
    for (int i = index2_min; i <= index2_max; ++i) {
      for (int j = index1_min; j <= index1_max; ++j) {
        expr_min = std::min(expr_min, Value(i, j));
      }
    }
    return expr_min;
  }
  int64_t Index1Max(int64_t expr_min, int64_t expr_max, int64_t index1_min,
                    int64_t index1_max, int64_t index2_min,
                    int64_t index2_max) const {
    for (int i = index1_max; i >= index1_min; --i) {
      for (int j = index2_min; j <= index2_max; ++j) {
        if (expr_min <= Value(i, j) && Value(i, j) <= expr_max) {
          return i;
        }
      }
    }
    return kint64max;
  }
  int64_t Index2Max(int64_t expr_min, int64_t expr_max, int64_t index1_min,
                    int64_t index1_max, int64_t index2_min,
                    int64_t index2_max) const {
    for (int i = index2_max; i >= index2_min; --i) {
      for (int j = index1_min; j <= index1_max; ++j) {
        if (expr_min <= Value(j, i) && Value(j, i) <= expr_max) {
          return i;
        }
      }
    }
    return kint64max;
  }
  int64_t ExprMax(int64_t index1_min, int64_t index1_max, int64_t index2_min,
                  int64_t index2_max) const {
    int64_t expr_max = kint64min;
    for (int i = index2_min; i <= index2_max; ++i) {
      for (int j = index1_min; j <= index1_max; ++j) {
        expr_max = std::max(expr_max, Value(i, j));
      }
    }
    return expr_max;
  }

  std::unique_ptr<Solver> solver_;
  IntVar* index1_;
  IntVar* index2_;
  IntExpr* expr_;
};

TEST_F(IntIntExprFunctionElementTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
}

TEST_F(IntIntExprFunctionElementTest, MinMaxGetters) {
  EXPECT_EQ(kUnderFlowValue, expr_->Min());
  EXPECT_EQ(kOverFlowValue, expr_->Max());
}

TEST_F(IntIntExprFunctionElementTest, SetMax1) {
  for (int i = 2; i >= 0; --i) {
    SCOPED_TRACE(absl::StrFormat("expr_->SetMax(%d)", i));
    expr_->SetMax(i);
    int64_t index1_min = Index1Min(kUnderFlowValue, i, kMin, kMax, kMin, kMax);
    int64_t index1_max = Index1Max(kUnderFlowValue, i, kMin, kMax, kMin, kMax);
    int64_t index2_min = Index2Min(kUnderFlowValue, i, kMin, kMax, kMin, kMax);
    int64_t index2_max = Index2Max(kUnderFlowValue, i, kMin, kMax, kMin, kMax);
    EXPECT_EQ(ExprMin(index1_min, index1_max, index2_min, index2_max),
              expr_->Min());
    EXPECT_EQ(ExprMax(index1_min, index1_max, index2_min, index2_max),
              expr_->Max());
    EXPECT_EQ(index1_min, index1_->Min());
    EXPECT_EQ(index2_min, index2_->Min());
    EXPECT_EQ(index1_max, index1_->Max());
    EXPECT_EQ(index2_max, index2_->Max());
  }
}

TEST_F(IntIntExprFunctionElementTest, SetMax2) {
  index1_->SetRange(0, kSize - 1);
  index2_->SetRange(0, kSize - 1);
  for (int i = 2; i > 0; --i) {
    SCOPED_TRACE(absl::StrFormat("expr_->SetMax(%d)", i));
    expr_->SetMax(i);
    int64_t index1_min =
        Index1Min(kUnderFlowValue, i, 0, kSize - 1, 0, kSize - 1);
    int64_t index1_max =
        Index1Max(kUnderFlowValue, i, 0, kSize - 1, 0, kSize - 1);
    int64_t index2_min =
        Index2Min(kUnderFlowValue, i, 0, kSize - 1, 0, kSize - 1);
    int64_t index2_max =
        Index2Max(kUnderFlowValue, i, 0, kSize - 1, 0, kSize - 1);
    EXPECT_EQ(ExprMin(index1_min, index1_max, index2_min, index2_max),
              expr_->Min());
    EXPECT_EQ(ExprMax(index1_min, index1_max, index2_min, index2_max),
              expr_->Max());
    EXPECT_EQ(index1_min, index1_->Min());
    EXPECT_EQ(index2_min, index2_->Min());
    EXPECT_EQ(index1_max, index1_->Max());
    EXPECT_EQ(index2_max, index2_->Max());
  }
}

TEST_F(IntIntExprFunctionElementTest, SetMin1) {
  for (int i = 2; i <= 4; ++i) {
    SCOPED_TRACE(absl::StrFormat("expr_->SetMin(%d)", i));
    expr_->SetMin(i);
    int64_t index1_min = Index1Min(i, kOverFlowValue, kMin, kMax, kMin, kMax);
    int64_t index1_max = Index1Max(i, kOverFlowValue, kMin, kMax, kMin, kMax);
    int64_t index2_min = Index2Min(i, kOverFlowValue, kMin, kMax, kMin, kMax);
    int64_t index2_max = Index2Max(i, kOverFlowValue, kMin, kMax, kMin, kMax);
    EXPECT_EQ(ExprMin(index1_min, index1_max, index2_min, index2_max),
              expr_->Min());
    EXPECT_EQ(ExprMax(index1_min, index1_max, index2_min, index2_max),
              expr_->Max());
    EXPECT_EQ(index1_min, index1_->Min());
    EXPECT_EQ(index2_min, index2_->Min());
    EXPECT_EQ(index1_max, index1_->Max());
    EXPECT_EQ(index2_max, index2_->Max());
  }
}

TEST_F(IntIntExprFunctionElementTest, SetMin2) {
  index1_->SetRange(0, kSize - 1);
  index2_->SetRange(0, kSize - 1);
  for (int i = 2; i < 4; ++i) {
    SCOPED_TRACE(absl::StrFormat("expr_->SetMin(%d)", i));
    expr_->SetMin(i);
    int64_t index1_min =
        Index1Min(i, kOverFlowValue, 0, kSize - 1, 0, kSize - 1);
    int64_t index1_max =
        Index1Max(i, kOverFlowValue, 0, kSize - 1, 0, kSize - 1);
    int64_t index2_min =
        Index2Min(i, kOverFlowValue, 0, kSize - 1, 0, kSize - 1);
    int64_t index2_max =
        Index2Max(i, kOverFlowValue, 0, kSize - 1, 0, kSize - 1);
    EXPECT_EQ(ExprMin(index1_min, index1_max, index2_min, index2_max),
              expr_->Min());
    EXPECT_EQ(ExprMax(index1_min, index1_max, index2_min, index2_max),
              expr_->Max());
    EXPECT_EQ(index1_min, index1_->Min());
    EXPECT_EQ(index2_min, index2_->Min());
    EXPECT_EQ(index1_max, index1_->Max());
    EXPECT_EQ(index2_max, index2_->Max());
  }
}

TEST_F(IntIntExprFunctionElementTest, SetRange) {
  expr_->SetRange(1, 2);
  int64_t index1_min = Index1Min(1, 2, kMin, kMax, kMin, kMax);
  int64_t index1_max = Index1Max(1, 2, kMin, kMax, kMin, kMax);
  int64_t index2_min = Index2Min(1, 2, kMin, kMax, kMin, kMax);
  int64_t index2_max = Index2Max(1, 2, kMin, kMax, kMin, kMax);
  EXPECT_EQ(ExprMin(index1_min, index1_max, index2_min, index2_max),
            expr_->Min());
  EXPECT_EQ(ExprMax(index1_min, index1_max, index2_min, index2_max),
            expr_->Max());
  EXPECT_EQ(index1_min, index1_->Min());
  EXPECT_EQ(index2_min, index2_->Min());
  EXPECT_EQ(index1_max, index1_->Max());
  EXPECT_EQ(index2_max, index2_->Max());
}

TEST_F(IntIntExprFunctionElementTest, SetIndex) {
  const int64_t kIndex1 = 1;
  const int64_t kIndex2 = 2;
  index1_->SetMax(kIndex1);
  EXPECT_EQ(ExprMin(kMin, kIndex1, kMin, kMax), expr_->Min());
  EXPECT_EQ(ExprMax(kMin, kIndex1, kMin, kMax), expr_->Max());
  index2_->SetMax(kIndex2);
  EXPECT_EQ(ExprMin(kMin, kIndex1, kMin, kIndex2), expr_->Min());
  EXPECT_EQ(ExprMax(kMin, kIndex1, kMin, kIndex2), expr_->Max());
  index1_->SetMin(kIndex1);
  EXPECT_EQ(ExprMin(kIndex1, kIndex1, kMin, kIndex2), expr_->Min());
  EXPECT_EQ(ExprMax(kIndex1, kIndex1, kMin, kIndex2), expr_->Max());
  index2_->SetMin(kIndex2);
  EXPECT_EQ(ExprMin(kIndex1, kIndex1, kIndex2, kIndex2), expr_->Min());
  EXPECT_EQ(ExprMax(kIndex1, kIndex1, kIndex2, kIndex2), expr_->Max());
}

TEST_F(IntIntExprFunctionElementTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr_->WhenRange(d);
}

// ----- IntExprArrayElement -----

class IntExprArrayElementTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("IntExprArrayElement");
    vars_.resize(4);
    vars_[0] = solver_->MakeIntVar(0, 0);
    vars_[1] = solver_->MakeIntVar(1, 2);
    vars_[2] = solver_->MakeIntVar(1, 1);
    vars_[3] = solver_->MakeIntVar(4, 5);
    index_ = solver_->MakeIntVar(0, 3);
    expr_ = solver_->MakeElement(vars_, index_);
  }

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  IntVar* index_;
  IntExpr* expr_;
};

TEST_F(IntExprArrayElementTest, DebugString) {
  EXPECT_FALSE(expr_->DebugString().empty());
}

TEST_F(IntExprArrayElementTest, MinMaxGetters) {
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(5, expr_->Max());
}

TEST_IN_PROPAGATION3(IntExprArrayElementTest, SetMax, solver, true, IntExpr*,
                     expr_, IntVar*, index_, std::vector<IntVar*>, vars_) {
  expr_->SetMax(3);
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(2, expr_->Max());
  EXPECT_EQ(0, index_->Min());
  EXPECT_EQ(2, index_->Max());
  EXPECT_EQ(0, vars_[0]->Min());
  EXPECT_EQ(0, vars_[0]->Max());
  EXPECT_EQ(1, vars_[1]->Min());
  EXPECT_EQ(2, vars_[1]->Max());
  EXPECT_EQ(1, vars_[2]->Min());
  EXPECT_EQ(1, vars_[2]->Max());
  EXPECT_EQ(4, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
  expr_->SetMax(2);
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(2, expr_->Max());
  EXPECT_EQ(0, index_->Min());
  EXPECT_EQ(2, index_->Max());
  EXPECT_EQ(0, vars_[0]->Min());
  EXPECT_EQ(0, vars_[0]->Max());
  EXPECT_EQ(1, vars_[1]->Min());
  EXPECT_EQ(2, vars_[1]->Max());
  EXPECT_EQ(1, vars_[2]->Min());
  EXPECT_EQ(1, vars_[2]->Max());
  EXPECT_EQ(4, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
  expr_->SetMax(1);
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(1, expr_->Max());
  EXPECT_EQ(0, index_->Min());
  EXPECT_EQ(2, index_->Max());
  EXPECT_EQ(0, vars_[0]->Min());
  EXPECT_EQ(0, vars_[0]->Max());
  EXPECT_EQ(1, vars_[1]->Min());
  EXPECT_EQ(2, vars_[1]->Max());
  EXPECT_EQ(1, vars_[2]->Min());
  EXPECT_EQ(1, vars_[2]->Max());
  EXPECT_EQ(4, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
  expr_->SetMax(0);
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(0, expr_->Max());
  EXPECT_EQ(0, index_->Min());
  EXPECT_EQ(0, index_->Max());
  EXPECT_EQ(0, vars_[0]->Min());
  EXPECT_EQ(0, vars_[0]->Max());
  EXPECT_EQ(1, vars_[1]->Min());
  EXPECT_EQ(2, vars_[1]->Max());
  EXPECT_EQ(1, vars_[2]->Min());
  EXPECT_EQ(1, vars_[2]->Max());
  EXPECT_EQ(4, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
  return nullptr;
}

TEST_IN_PROPAGATION3(IntExprArrayElementTest, SetMin, solver, true, IntExpr*,
                     expr_, IntVar*, index_, std::vector<IntVar*>, vars_) {
  expr_->SetMin(1);
  EXPECT_EQ(1, expr_->Min());
  EXPECT_EQ(5, expr_->Max());
  EXPECT_EQ(1, index_->Min());
  EXPECT_EQ(3, index_->Max());
  EXPECT_EQ(0, vars_[0]->Min());
  EXPECT_EQ(0, vars_[0]->Max());
  EXPECT_EQ(1, vars_[1]->Min());
  EXPECT_EQ(2, vars_[1]->Max());
  EXPECT_EQ(1, vars_[2]->Min());
  EXPECT_EQ(1, vars_[2]->Max());
  EXPECT_EQ(4, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
  expr_->SetMin(2);
  EXPECT_EQ(2, expr_->Min());
  EXPECT_EQ(5, expr_->Max());
  EXPECT_EQ(1, index_->Min());
  EXPECT_EQ(3, index_->Max());
  EXPECT_EQ(0, vars_[0]->Min());
  EXPECT_EQ(0, vars_[0]->Max());
  EXPECT_EQ(1, vars_[1]->Min());
  EXPECT_EQ(2, vars_[1]->Max());
  EXPECT_EQ(1, vars_[2]->Min());
  EXPECT_EQ(1, vars_[2]->Max());
  EXPECT_EQ(4, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
  expr_->SetMin(3);
  EXPECT_EQ(4, expr_->Min());
  EXPECT_EQ(5, expr_->Max());
  EXPECT_EQ(3, index_->Min());
  EXPECT_EQ(3, index_->Max());
  EXPECT_EQ(0, vars_[0]->Min());
  EXPECT_EQ(0, vars_[0]->Max());
  EXPECT_EQ(1, vars_[1]->Min());
  EXPECT_EQ(2, vars_[1]->Max());
  EXPECT_EQ(1, vars_[2]->Min());
  EXPECT_EQ(1, vars_[2]->Max());
  EXPECT_EQ(4, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
  expr_->SetMin(4);
  EXPECT_EQ(4, expr_->Min());
  EXPECT_EQ(5, expr_->Max());
  EXPECT_EQ(3, index_->Min());
  EXPECT_EQ(3, index_->Max());
  EXPECT_EQ(0, vars_[0]->Min());
  EXPECT_EQ(0, vars_[0]->Max());
  EXPECT_EQ(1, vars_[1]->Min());
  EXPECT_EQ(2, vars_[1]->Max());
  EXPECT_EQ(1, vars_[2]->Min());
  EXPECT_EQ(1, vars_[2]->Max());
  EXPECT_EQ(4, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
  expr_->SetMin(5);
  EXPECT_EQ(5, expr_->Min());
  EXPECT_EQ(5, expr_->Max());
  EXPECT_EQ(3, index_->Min());
  EXPECT_EQ(3, index_->Max());
  EXPECT_EQ(0, vars_[0]->Min());
  EXPECT_EQ(0, vars_[0]->Max());
  EXPECT_EQ(1, vars_[1]->Min());
  EXPECT_EQ(2, vars_[1]->Max());
  EXPECT_EQ(1, vars_[2]->Min());
  EXPECT_EQ(1, vars_[2]->Max());
  EXPECT_EQ(5, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
  return nullptr;
}

TEST_IN_PROPAGATION3(IntExprArrayElementTest, SetRange, solver, true, IntExpr*,
                     expr_, IntVar*, index_, std::vector<IntVar*>, vars_) {
  expr_->SetRange(1, 4);
  EXPECT_EQ(1, expr_->Min());
  EXPECT_EQ(4, expr_->Max());
  EXPECT_EQ(1, index_->Min());
  EXPECT_EQ(3, index_->Max());
  EXPECT_EQ(0, vars_[0]->Min());
  EXPECT_EQ(0, vars_[0]->Max());
  EXPECT_EQ(1, vars_[1]->Min());
  EXPECT_EQ(2, vars_[1]->Max());
  EXPECT_EQ(1, vars_[2]->Min());
  EXPECT_EQ(1, vars_[2]->Max());
  EXPECT_EQ(4, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
  expr_->SetRange(2, 3);
  EXPECT_EQ(2, expr_->Min());
  EXPECT_EQ(2, expr_->Max());
  EXPECT_EQ(1, index_->Min());
  EXPECT_EQ(1, index_->Max());
  EXPECT_EQ(0, vars_[0]->Min());
  EXPECT_EQ(0, vars_[0]->Max());
  EXPECT_EQ(2, vars_[1]->Min());
  EXPECT_EQ(2, vars_[1]->Max());
  EXPECT_EQ(1, vars_[2]->Min());
  EXPECT_EQ(1, vars_[2]->Max());
  EXPECT_EQ(4, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
  return nullptr;
}

TEST_IN_PROPAGATION3(IntExprArrayElementTest, SetIndex, solver, true, IntExpr*,
                     expr_, IntVar*, index_, std::vector<IntVar*>, vars_) {
  index_->SetMax(2);
  EXPECT_EQ(0, expr_->Min());
  EXPECT_EQ(2, expr_->Max());
  index_->SetMin(1);
  EXPECT_EQ(1, expr_->Min());
  EXPECT_EQ(2, expr_->Max());
  index_->SetValue(1);
  EXPECT_EQ(1, expr_->Min());
  EXPECT_EQ(2, expr_->Max());
  return nullptr;
}

TEST_IN_PROPAGATION3(IntExprArrayElementTest, SetMaxIndexBound, solver, true,
                     IntExpr*, expr_, IntVar*, index_, std::vector<IntVar*>,
                     vars_) {
  index_->SetValue(1);
  expr_->SetMax(1);
  EXPECT_EQ(1, vars_[1]->Max());
  return nullptr;
}

TEST_IN_PROPAGATION3(IntExprArrayElementTest, SetMinIndexBound, solver, true,
                     IntExpr*, expr_, IntVar*, index_, std::vector<IntVar*>,
                     vars_) {
  index_->SetValue(1);
  expr_->SetMin(2);
  EXPECT_EQ(2, vars_[1]->Min());
  return nullptr;
}

TEST_IN_PROPAGATION3(IntExprArrayElementTest, SetRangeIndexBound, solver, true,
                     IntExpr*, expr_, IntVar*, index_, std::vector<IntVar*>,
                     vars_) {
  index_->SetValue(1);
  expr_->SetRange(1, 1);
  EXPECT_EQ(1, vars_[1]->Max());
  return nullptr;
}

TEST_F(IntExprArrayElementTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  expr_->WhenRange(d);
}

// ----- IntExprArrayElementCt -----

class IntExprArrayElementCtTest : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("IntExprArrayElementCt");
    vars_.resize(4);
    vars_[0] = solver_->MakeIntVar(0, 0);
    vars_[1] = solver_->MakeIntVar(1, 2);
    vars_[2] = solver_->MakeIntVar(1, 1);
    vars_[3] = solver_->MakeIntVar(4, 5);
    index_ = solver_->MakeIntVar(0, 3);
    var_ = solver_->MakeElement(vars_, index_)->Var();
  }

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  IntVar* index_;
  IntVar* var_;
};

TEST_F(IntExprArrayElementCtTest, MinMaxGetters) {
  EXPECT_EQ(0, var_->Min());
  EXPECT_EQ(5, var_->Max());
}

TEST_IN_PROPAGATION3(IntExprArrayElementCtTest, SetMax, s, true,
                     std::vector<IntVar*>, vars_, IntVar*, index_, IntVar*,
                     var_) {
  var_->SetMax(3);
  EXPECT_EQ(0, var_->Min());
  EXPECT_EQ(2, var_->Max());
  EXPECT_EQ(0, index_->Min());
  EXPECT_EQ(2, index_->Max());
  EXPECT_EQ(0, vars_[0]->Min());
  EXPECT_EQ(0, vars_[0]->Max());
  EXPECT_EQ(1, vars_[1]->Min());
  EXPECT_EQ(2, vars_[1]->Max());
  EXPECT_EQ(1, vars_[2]->Min());
  EXPECT_EQ(1, vars_[2]->Max());
  EXPECT_EQ(4, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
  var_->SetMax(2);
  EXPECT_EQ(0, var_->Min());
  EXPECT_EQ(2, var_->Max());
  EXPECT_EQ(0, index_->Min());
  EXPECT_EQ(2, index_->Max());
  EXPECT_EQ(0, vars_[0]->Min());
  EXPECT_EQ(0, vars_[0]->Max());
  EXPECT_EQ(1, vars_[1]->Min());
  EXPECT_EQ(2, vars_[1]->Max());
  EXPECT_EQ(1, vars_[2]->Min());
  EXPECT_EQ(1, vars_[2]->Max());
  EXPECT_EQ(4, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
  var_->SetMax(1);
  EXPECT_EQ(0, var_->Min());
  EXPECT_EQ(1, var_->Max());
  EXPECT_EQ(0, index_->Min());
  EXPECT_EQ(2, index_->Max());
  EXPECT_EQ(0, vars_[0]->Min());
  EXPECT_EQ(0, vars_[0]->Max());
  EXPECT_EQ(1, vars_[1]->Min());
  EXPECT_EQ(2, vars_[1]->Max());
  EXPECT_EQ(1, vars_[2]->Min());
  EXPECT_EQ(1, vars_[2]->Max());
  EXPECT_EQ(4, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
  var_->SetMax(0);
  EXPECT_EQ(0, var_->Min());
  EXPECT_EQ(0, var_->Max());
  EXPECT_EQ(0, index_->Min());
  EXPECT_EQ(0, index_->Max());
  EXPECT_EQ(0, vars_[0]->Min());
  EXPECT_EQ(0, vars_[0]->Max());
  EXPECT_EQ(1, vars_[1]->Min());
  EXPECT_EQ(2, vars_[1]->Max());
  EXPECT_EQ(1, vars_[2]->Min());
  EXPECT_EQ(1, vars_[2]->Max());
  EXPECT_EQ(4, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
  return nullptr;
};

TEST_IN_PROPAGATION3(IntExprArrayElementCtTest, SetMin, s, true,
                     std::vector<IntVar*>, vars_, IntVar*, index_, IntVar*,
                     var_) {
  var_->SetMin(1);
  EXPECT_EQ(1, var_->Min());
  EXPECT_EQ(5, var_->Max());
  EXPECT_EQ(1, index_->Min());
  EXPECT_EQ(3, index_->Max());
  EXPECT_EQ(0, vars_[0]->Min());
  EXPECT_EQ(0, vars_[0]->Max());
  EXPECT_EQ(1, vars_[1]->Min());
  EXPECT_EQ(2, vars_[1]->Max());
  EXPECT_EQ(1, vars_[2]->Min());
  EXPECT_EQ(1, vars_[2]->Max());
  EXPECT_EQ(4, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
  var_->SetMin(2);
  EXPECT_EQ(2, var_->Min());
  EXPECT_EQ(5, var_->Max());
  EXPECT_EQ(1, index_->Min());
  EXPECT_EQ(3, index_->Max());
  EXPECT_EQ(0, vars_[0]->Min());
  EXPECT_EQ(0, vars_[0]->Max());
  EXPECT_EQ(1, vars_[1]->Min());
  EXPECT_EQ(2, vars_[1]->Max());
  EXPECT_EQ(1, vars_[2]->Min());
  EXPECT_EQ(1, vars_[2]->Max());
  EXPECT_EQ(4, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
  var_->SetMin(3);
  EXPECT_EQ(4, var_->Min());
  EXPECT_EQ(5, var_->Max());
  EXPECT_EQ(3, index_->Min());
  EXPECT_EQ(3, index_->Max());
  EXPECT_EQ(0, vars_[0]->Min());
  EXPECT_EQ(0, vars_[0]->Max());
  EXPECT_EQ(1, vars_[1]->Min());
  EXPECT_EQ(2, vars_[1]->Max());
  EXPECT_EQ(1, vars_[2]->Min());
  EXPECT_EQ(1, vars_[2]->Max());
  EXPECT_EQ(4, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
  var_->SetMin(4);
  EXPECT_EQ(4, var_->Min());
  EXPECT_EQ(5, var_->Max());
  EXPECT_EQ(3, index_->Min());
  EXPECT_EQ(3, index_->Max());
  EXPECT_EQ(0, vars_[0]->Min());
  EXPECT_EQ(0, vars_[0]->Max());
  EXPECT_EQ(1, vars_[1]->Min());
  EXPECT_EQ(2, vars_[1]->Max());
  EXPECT_EQ(1, vars_[2]->Min());
  EXPECT_EQ(1, vars_[2]->Max());
  EXPECT_EQ(4, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
  var_->SetMin(5);
  EXPECT_EQ(5, var_->Min());
  EXPECT_EQ(5, var_->Max());
  EXPECT_EQ(3, index_->Min());
  EXPECT_EQ(3, index_->Max());
  EXPECT_EQ(0, vars_[0]->Min());
  EXPECT_EQ(0, vars_[0]->Max());
  EXPECT_EQ(1, vars_[1]->Min());
  EXPECT_EQ(2, vars_[1]->Max());
  EXPECT_EQ(1, vars_[2]->Min());
  EXPECT_EQ(1, vars_[2]->Max());
  EXPECT_EQ(5, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
  return nullptr;
};

TEST_IN_PROPAGATION3(IntExprArrayElementCtTest, SetRange, s, true,
                     std::vector<IntVar*>, vars_, IntVar*, index_, IntVar*,
                     var_) {
  var_->SetRange(1, 4);
  EXPECT_EQ(1, var_->Min());
  EXPECT_EQ(4, var_->Max());
  EXPECT_EQ(1, index_->Min());
  EXPECT_EQ(3, index_->Max());
  EXPECT_EQ(0, vars_[0]->Min());
  EXPECT_EQ(0, vars_[0]->Max());
  EXPECT_EQ(1, vars_[1]->Min());
  EXPECT_EQ(2, vars_[1]->Max());
  EXPECT_EQ(1, vars_[2]->Min());
  EXPECT_EQ(1, vars_[2]->Max());
  EXPECT_EQ(4, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
  var_->SetRange(2, 3);
  EXPECT_EQ(2, var_->Min());
  EXPECT_EQ(2, var_->Max());
  EXPECT_EQ(1, index_->Min());
  EXPECT_EQ(1, index_->Max());
  EXPECT_EQ(0, vars_[0]->Min());
  EXPECT_EQ(0, vars_[0]->Max());
  EXPECT_EQ(2, vars_[1]->Min());
  EXPECT_EQ(2, vars_[1]->Max());
  EXPECT_EQ(1, vars_[2]->Min());
  EXPECT_EQ(1, vars_[2]->Max());
  EXPECT_EQ(4, vars_[3]->Min());
  EXPECT_EQ(5, vars_[3]->Max());
  return nullptr;
};

TEST_IN_PROPAGATION3(IntExprArrayElementCtTest, SetIndex, s, true,
                     std::vector<IntVar*>, vars_, IntVar*, index_, IntVar*,
                     var_) {
  index_->SetMax(2);
  EXPECT_EQ(0, var_->Min());
  EXPECT_EQ(2, var_->Max());
  index_->SetMin(1);
  EXPECT_EQ(1, var_->Min());
  EXPECT_EQ(2, var_->Max());
  index_->SetValue(1);
  EXPECT_EQ(1, var_->Min());
  EXPECT_EQ(2, var_->Max());
  return nullptr;
};

TEST_IN_PROPAGATION3(IntExprArrayElementCtTest, SetMaxIndexBound, s, true,
                     std::vector<IntVar*>, vars_, IntVar*, index_, IntVar*,
                     var_) {
  index_->SetValue(1);
  var_->SetMax(1);
  EXPECT_EQ(1, vars_[1]->Max());
  return nullptr;
};

TEST_IN_PROPAGATION3(IntExprArrayElementCtTest, SetMinIndexBound, s, true,
                     std::vector<IntVar*>, vars_, IntVar*, index_, IntVar*,
                     var_) {
  index_->SetValue(1);
  var_->SetMin(2);
  EXPECT_EQ(2, vars_[1]->Min());
  return nullptr;
};

TEST_IN_PROPAGATION3(IntExprArrayElementCtTest, SetRangeIndexBound, s, true,
                     std::vector<IntVar*>, vars_, IntVar*, index_, IntVar*,
                     var_) {
  index_->SetValue(1);
  var_->SetRange(1, 1);
  EXPECT_EQ(1, vars_[1]->Max());
  return nullptr;
};

// ----- IntExprEvaluatorElementCt -----

static const int64_t kFuncBegin_ = -5;
static const int64_t kFuncEnd_ = 5;
static const int64_t kIdxBegin_ = -8;
static const int64_t kIdxEnd_ = 3;
static const int64_t kDomainJump_ = 5;

class IntExprEvaluatorElementCt : public testing::Test {
 protected:
  void SetUp() override {
    absl::SetFlag(&FLAGS_cp_disable_expression_optimization, true);
    solver_ = std::make_unique<Solver>("IntExprArrayElementCt");
    for (int i = kFuncBegin_; i < kFuncEnd_; ++i) {
      const int64_t domain_start = i * kDomainJump_;
      const int64_t domain_end = domain_start + 2 * kDomainJump_ - 1;
      vars_.push_back(solver_->MakeIntVar(domain_start, domain_end));
    }
    evaluator_ = [this](int64_t idx) { return vars_[idx - kFuncBegin_]; };
    index_ = solver_->MakeIntVar(kIdxBegin_, kIdxEnd_);
    var_ =
        solver_->MakeElement(evaluator_, kFuncBegin_, kFuncEnd_, index_)->Var();
  }

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  IntVar* index_;
  IntVar* var_;
  Solver::Int64ToIntVar evaluator_;
};

TEST_F(IntExprEvaluatorElementCt, TestMin) {
  int64_t first_idx = std::max(kFuncBegin_, kIdxBegin_);
  EXPECT_EQ(first_idx * kDomainJump_, var_->Min());
}

TEST_F(IntExprEvaluatorElementCt, TestMax) {
  int64_t last_idx = std::min(kFuncEnd_, kIdxEnd_ + 1);
  EXPECT_EQ((last_idx + 1) * kDomainJump_ - 1, var_->Max());
}

TEST_IN_PROPAGATION2(IntExprEvaluatorElementCt, SetMin, s, true,
                     const Solver::Int64ToIntVar&, evaluator_, IntVar*, var_) {
  int64_t first_idx = std::max(kFuncBegin_, kIdxBegin_);
  IntVar* var_with_min_domain = evaluator_(first_idx);
  int64_t global_min = var_with_min_domain->Min();
  EXPECT_EQ(var_->Min(), global_min);
  LOG(INFO) << "Log: 1";
  var_with_min_domain->SetMin(var_with_min_domain->Max());
  LOG(INFO) << "Log: 2";
  EXPECT_GT(var_->Min(), global_min);
  LOG(INFO) << "Log: 3";
  EXPECT_EQ(var_->Min(), evaluator_(kFuncBegin_ + 1)->Min());
  return nullptr;
};

TEST_IN_PROPAGATION2(IntExprEvaluatorElementCt, SetMax, s, true,
                     const Solver::Int64ToIntVar&, evaluator_, IntVar*, var_) {
  int64_t last_idx = std::min(kFuncEnd_, kIdxEnd_ + 1);
  IntVar* var_with_max_domain = evaluator_(last_idx - 1);
  int64_t global_max = var_with_max_domain->Max();
  EXPECT_EQ(var_->Max(), global_max);
  var_with_max_domain->SetMax(var_with_max_domain->Min());
  EXPECT_LT(var_->Max(), global_max);
  EXPECT_EQ(var_->Max(), evaluator_(last_idx - 2)->Max());
  return nullptr;
};

TEST_IN_PROPAGATION3(IntExprEvaluatorElementCt, ChangeRange, s, true,
                     const Solver::Int64ToIntVar&, evaluator_, IntVar*, index_,
                     IntVar*, var_) {
  int64_t first_idx = std::max(kFuncBegin_, kIdxBegin_);
  int64_t last_idx = std::min(kFuncEnd_, kIdxEnd_ + 1);
  EXPECT_EQ(evaluator_(first_idx)->Min(), var_->Min());
  EXPECT_EQ(evaluator_(last_idx - 1)->Max(), var_->Max());
  index_->SetMin(first_idx + 1);
  index_->SetMax(last_idx - 2);
  EXPECT_LT(evaluator_(first_idx)->Min(), var_->Min());
  EXPECT_GT(evaluator_(last_idx - 1)->Max(), var_->Max());
  EXPECT_EQ(evaluator_(first_idx + 1)->Min(), var_->Min());
  EXPECT_EQ(evaluator_(last_idx - 2)->Max(), var_->Max());
  return nullptr;
};

}  // namespace
