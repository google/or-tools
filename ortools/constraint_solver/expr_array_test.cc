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

#include "absl/strings/str_format.h"
#include "gtest/gtest.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solver_test_util.h"
#include "ortools/constraint_solver/solver_parameters.pb.h"

namespace operations_research {
namespace {
class BooleanScalProdCtCstTest_SetOneVarDown_Test;
}
namespace {
class BooleanScalProdCtCstTest_SetOneVarUp_Test;
}
namespace {
class BooleanScalProdCtCstTest_SetToMax_Test;
}
namespace {
class BooleanScalProdCtCstTest_SetToMin_Test;
}
namespace {
class BooleanScalProdCtTest_SetOneVarDown_Test;
}
namespace {
class BooleanScalProdCtTest_SetOneVarUp_Test;
}
namespace {
class BooleanScalProdCtTest_SetToMax_Test;
}
namespace {
class BooleanScalProdCtTest_SetToMin_Test;
}
namespace {
class BooleanScalProdEqCstTest_Propagate1_Test;
}
namespace {
class BooleanScalProdEqCstTest_Propagate2_Test;
}
namespace {
class BooleanScalProdTest_SetOneNegVarDown_Test;
}
namespace {
class BooleanScalProdTest_SetOneNegVarUp_Test;
}
namespace {
class BooleanScalProdTest_SetOnePosVarDown_Test;
}
namespace {
class BooleanScalProdTest_SetOnePosVarUp_Test;
}
namespace {
class BooleanScalProdTest_SetToMax_Test;
}
namespace {
class BooleanScalProdTest_SetToMin_Test;
}
namespace {
class SumArrayTest_BigReductionMax_Test;
}
namespace {
class SumArrayTest_BigReductionMin_Test;
}
namespace {
class SumArrayTest_FailMax_Test;
}
namespace {
class SumArrayTest_FailMin_Test;
}
namespace {
class SumArrayTest_SetOneVar_Test;
}
namespace {
class SumArrayTest_SetToMax_Test;
}
namespace {
class SumArrayTest_SetToMin_Test;
}
namespace {
class SumArrayTest_SmallReduction_Test;
}

using operations_research::Zero;

namespace {

class NullDemon : public Demon {
  void Run(Solver* const s) override {}
};

// ----- Sum Array Test -----

class SumArrayTest : public testing::Test {
 protected:
  void SetUp() override {
    ConstraintSolverParameters params = Solver::DefaultSolverParameters();
#if defined(SMALL_ARRAY)
    params.set_array_split_size(3);
#endif
    solver_ = std::make_unique<Solver>("SumArrayTest", params);
    vars_.resize(10);
    for (int i = 0; i < 10; ++i) {
      vars_[i] = solver_->MakeIntVar(i, 2 * i);
    }
    sum_ = solver_->MakeSum(vars_);
  }

  void TearDown() override {}

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  IntExpr* sum_;
};

TEST_F(SumArrayTest, EmptyVector) {
  std::vector<IntVar*> vars;
  IntExpr* const sum = solver_->MakeSum(vars);
  EXPECT_BOUND_TO(sum, 0);
}

TEST_F(SumArrayTest, Bounds) {
  EXPECT_EQ(45, sum_->Min()) << "bad computed min in sum_array";
  EXPECT_EQ(90, sum_->Max()) << "bad computed min in sum_array";
}

TEST_IN_PROPAGATION2(SumArrayTest, SetToMin, s, true, IntExpr*, sum_,
                     std::vector<IntVar*>, vars_) {
  sum_->SetMax(45);
  for (int i = 0; i < 10; ++i) {
    EXPECT_BOUND_TO(vars_[i], i);
  }
  return nullptr;
}

TEST_IN_PROPAGATION2(SumArrayTest, SetToMax, s, true, IntExpr*, sum_,
                     std::vector<IntVar*>, vars_) {
  sum_->SetMin(90);
  for (int i = 0; i < 10; ++i) {
    EXPECT_BOUND_TO(vars_[i], 2 * i);
  }
  return nullptr;
}

TEST_IN_PROPAGATION2(SumArrayTest, SetOneVar, s, true, IntExpr*, sum_,
                     std::vector<IntVar*>, vars_) {
  vars_[7]->SetValue(10);
  EXPECT_EQ(48, sum_->Min()) << "bad computed min in sum_array";
  EXPECT_EQ(86, sum_->Max()) << "bad computed max in sum_array";
  return nullptr;
}

TEST_IN_PROPAGATION2(SumArrayTest, SmallReduction, s, true, IntExpr*, sum_,
                     std::vector<IntVar*>, vars_) {
  sum_->SetRange(60, 80);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(i, vars_[i]->Min()) << "min not bound to the correct value";
    EXPECT_EQ(2 * i, vars_[i]->Max()) << "max not bound to the correct value";
  }
  return nullptr;
}

TEST_IN_PROPAGATION2(SumArrayTest, BigReductionMin, s, true, IntExpr*, sum_,
                     std::vector<IntVar*>, vars_) {
  sum_->SetMin(88);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ((i > 1 ? 2 * i - 2 : i), vars_[i]->Min())
        << "min not bound to the correct value";
    EXPECT_EQ(2 * i, vars_[i]->Max()) << "max not bound to the correct value";
  }
  return nullptr;
}

TEST_IN_PROPAGATION2(SumArrayTest, BigReductionMax, s, true, IntExpr*, sum_,
                     std::vector<IntVar*>, vars_) {
  sum_->SetMax(48);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(i, vars_[i]->Min()) << "min not bound to the correct value";
    EXPECT_EQ((i > 2 ? i + 3 : 2 * i), vars_[i]->Max())
        << "max not bound to the correct value";
  }
  return nullptr;
}

TEST_F(SumArrayTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  sum_->WhenRange(d);
}

TEST_IN_PROPAGATION1(SumArrayTest, FailMin, s, false, IntExpr*, sum_) {
  sum_->SetMin(91);
  return nullptr;
}

TEST_IN_PROPAGATION1(SumArrayTest, FailMax, s, false, IntExpr*, sum_) {
  sum_->SetMax(44);
  return nullptr;
}

TEST_F(SumArrayTest, BigSumVector) {
  std::vector<IntVar*> vars;
  solver_->MakeIntVarArray(1001, 1, 10, "x", &vars);
  IntExpr* sum = solver_->MakeSum(vars);
  EXPECT_EQ(10010, sum->Max());
  EXPECT_EQ(1001, sum->Min());
}

TEST_F(SumArrayTest, SmallSumVector) {
  std::vector<IntVar*> vars;
  IntExpr* sum = solver_->MakeSum(vars);
  EXPECT_EQ(0, sum->Min());
  EXPECT_EQ(0, sum->Max());
  vars.push_back(solver_->MakeIntVar(1, 10, "x0"));
  sum = solver_->MakeSum(vars);
  EXPECT_EQ(1, sum->Min());
  EXPECT_EQ(10, sum->Max());
  vars.push_back(solver_->MakeIntVar(1, 10, "x1"));
  sum = solver_->MakeSum(vars);
  EXPECT_EQ(2, sum->Min());
  EXPECT_EQ(20, sum->Max());
  vars.push_back(solver_->MakeIntVar(1, 10, "x2"));
  sum = solver_->MakeSum(vars);
  EXPECT_EQ(3, sum->Min());
  EXPECT_EQ(30, sum->Max());
}

// ----- Safe Sum Array Test -----

class SafeSumArrayTest : public testing::Test {
 protected:
  void SetUp() override {
    ConstraintSolverParameters params = Solver::DefaultSolverParameters();
#if defined(SMALL_ARRAY)
    params.set_array_split_size(3);
#endif
    solver_ = std::make_unique<Solver>("SumArrayTest", params);
    solver_->MakeIntVarArray(20, kint64min, kint64max, &vars_);
    sum_ = solver_->MakeSum(vars_);
  }

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  IntExpr* sum_;
};

TEST_F(SafeSumArrayTest, Bounds) {
  EXPECT_EQ(kint64min, sum_->Min());
  EXPECT_EQ(kint64max, sum_->Max());
}

TEST_IN_PROPAGATION2(SafeSumArrayTest, SetPositive, s, true, IntExpr*, sum_,
                     std::vector<IntVar*>, vars_) {
  sum_->SetMax(kint64max);
  for (int i = 0; i < 20; ++i) {
    EXPECT_EXPR_EQ_RANGE(vars_[i], kint64min, kint64max);
  }
  for (int i = 0; i < 19; ++i) {
    vars_[i]->SetMin(0);
    EXPECT_EXPR_EQ_RANGE(sum_, kint64min, kint64max);
  }
  vars_[19]->SetMin(0);
  EXPECT_EXPR_EQ_RANGE(sum_, 0, kint64max);
  for (int i = 0; i < 20; ++i) {
    vars_[i]->SetMin(1);
    EXPECT_EXPR_EQ_RANGE(sum_, i + 1, kint64max);
  }
  for (int i = 0; i < 19; ++i) {
    vars_[i]->SetMax(100);
    EXPECT_EXPR_EQ_RANGE(sum_, 20, kint64max);
  }
  vars_[19]->SetMax(100);
  EXPECT_EXPR_EQ_RANGE(sum_, 20, 2000);
  return nullptr;
}

TEST_IN_PROPAGATION2(SafeSumArrayTest, SetNegative, s, true, IntExpr*, sum_,
                     std::vector<IntVar*>, vars_) {
  sum_->SetMax(kint64max);
  for (int i = 0; i < 20; ++i) {
    EXPECT_EXPR_EQ_RANGE(vars_[i], kint64min, kint64max);
  }
  for (int i = 0; i < 19; ++i) {
    vars_[i]->SetMax(0);
    EXPECT_EXPR_EQ_RANGE(sum_, kint64min, kint64max);
  }
  vars_[19]->SetMax(0);
  EXPECT_EXPR_EQ_RANGE(sum_, kint64min, 0);
  for (int i = 0; i < 20; ++i) {
    vars_[i]->SetMax(-1);
    EXPECT_EXPR_EQ_RANGE(sum_, kint64min, -i - 1);
  }
  for (int i = 0; i < 19; ++i) {
    vars_[i]->SetMin(-100);
    EXPECT_EXPR_EQ_RANGE(sum_, kint64min, -20);
  }
  vars_[19]->SetMin(-100);
  EXPECT_EXPR_EQ_RANGE(sum_, -2000, -20);
  return nullptr;
}

TEST_IN_PROPAGATION0(SafeSumArrayTest, ReduceDomain, solver, true) {
  std::vector<IntVar*> vars(3);
  vars[0] = solver->MakeIntVar(0, 0, "v0");
  vars[1] = solver->MakeIntVar(0, 100, "v1");
  vars[2] = solver->MakeIntVar(kint64min + 1, kint64max - 1, "v2");
  // We are forcing the delta domain to be overflowing while the
  // variable is not stuck on intmin, intmax.
  IntExpr* const sum = solver->MakeSum(vars);
  vars[2]->SetRange(100, 200);
  EXPECT_EXPR_EQ_RANGE(sum, 100, 300);
  return nullptr;
}

// ----- Min Array Ct Test -----

class MinArrayCtTest : public testing::Test {
 protected:
  void SetUp() override {
    ConstraintSolverParameters params = Solver::DefaultSolverParameters();
#if defined(SMALL_ARRAY)
    params.set_array_split_size(3);
#endif
    solver_ = std::make_unique<Solver>("MinArrayCtTest", params);
    vars_.resize(10);
    for (int i = 0; i < 10; ++i) {
      vars_[i] = solver_->MakeIntVar(i, 2 * i + 5);
    }
    min_ = solver_->MakeMin(vars_)->Var();
  }

  void TearDown() override {}

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  IntExpr* min_;
};

TEST_F(MinArrayCtTest, Bounds) {
  EXPECT_EQ(int64_t{0}, min_->Min()) << "bad computed min in min_array";
  EXPECT_EQ(5, min_->Max()) << "bad computed min in min_array";
}

TEST_IN_PROPAGATION2(MinArrayCtTest, SetToMin, s, true, IntExpr*, min_,
                     std::vector<IntVar*>, vars_) {
  min_->SetMax(0);
  EXPECT_TRUE(vars_[0]->Bound()) << "var not bound";
  EXPECT_EQ(0, vars_[0]->Min()) << "var not bound to the correct value";
  return nullptr;
};

TEST_IN_PROPAGATION2(MinArrayCtTest, SetToMax, s, true, IntExpr*, min_,
                     std::vector<IntVar*>, vars_) {
  min_->SetMin(5);
  EXPECT_TRUE(vars_[0]->Bound()) << "var not bound";
  EXPECT_EQ(5, vars_[0]->Min()) << "var not bound to the correct value";
  return nullptr;
};

TEST_IN_PROPAGATION2(MinArrayCtTest, SetOneVar, s, true, IntExpr*, min_,
                     std::vector<IntVar*>, vars_) {
  vars_[0]->SetValue(5);
  EXPECT_EQ(1, min_->Min()) << "bad computed min in min_array";
  EXPECT_EQ(5, min_->Max()) << "bad computed max in min_array";
  return nullptr;
};

TEST_F(MinArrayCtTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  min_->WhenRange(d);
}

TEST_F(MinArrayCtTest, BigMinVector) {
  std::vector<IntVar*> vars;
  for (int i = 0; i < 1001; ++i) {
    vars.push_back(solver_->MakeIntVar(i, 3000 - i, absl::StrFormat("x%d", i)));
  }
  IntExpr* expr = solver_->MakeMin(vars);
  EXPECT_EQ(2000, expr->Max());
  EXPECT_EQ(0, expr->Min());
}

TEST_F(MinArrayCtTest, SmallMinVector) {
  std::vector<IntVar*> vars;
  IntExpr* expr = solver_->MakeMin(vars);
  EXPECT_EQ(kint64max, expr->Min());
  EXPECT_EQ(kint64max, expr->Max());
  vars.push_back(solver_->MakeIntVar(1, 10, "x0"));
  expr = solver_->MakeMin(vars);
  EXPECT_EQ(1, expr->Min());
  EXPECT_EQ(10, expr->Max());
  vars.push_back(solver_->MakeIntVar(2, 9, "x1"));
  expr = solver_->MakeMin(vars);
  EXPECT_EQ(1, expr->Min());
  EXPECT_EQ(9, expr->Max());
  vars.push_back(solver_->MakeIntVar(3, 8, "x2"));
  expr = solver_->MakeMin(vars);
  EXPECT_EQ(1, expr->Min());
  EXPECT_EQ(8, expr->Max());
}

// ----- Max Array Ct Test -----

class MaxArrayCtTest : public testing::Test {
 protected:
  void SetUp() override {
    ConstraintSolverParameters params = Solver::DefaultSolverParameters();
#if defined(SMALL_ARRAY)
    params.set_array_split_size(3);
#endif
    solver_ = std::make_unique<Solver>("MaxArrayCtTest", params);
    vars_.resize(10);
    for (int i = 0; i < 10; ++i) {
      vars_[i] = solver_->MakeIntVar(i, 2 * i + 5);
    }
    max_ = solver_->MakeMax(vars_)->Var();
  }

  void TearDown() override {}

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  IntExpr* max_;
};

TEST_F(MaxArrayCtTest, Bounds) {
  EXPECT_EQ(9, max_->Min()) << "bad computed min in max_array";
  EXPECT_EQ(23, max_->Max()) << "bad computed min in max_array";
}

TEST_IN_PROPAGATION2(MaxArrayCtTest, SetToMin, s, true, IntExpr*, max_,
                     std::vector<IntVar*>, vars_) {
  max_->SetMin(23);
  EXPECT_TRUE(vars_[9]->Bound()) << "var not bound";
  EXPECT_EQ(23, vars_[9]->Min()) << "var not bound to the correct value";
  return nullptr;
};

TEST_IN_PROPAGATION2(MaxArrayCtTest, SetToMax, s, true, IntExpr*, max_,
                     std::vector<IntVar*>, vars_) {
  max_->SetMax(9);
  EXPECT_TRUE(vars_[9]->Bound()) << "var not bound";
  EXPECT_EQ(9, vars_[9]->Min()) << "var not bound to the correct value";
  return nullptr;
};

TEST_IN_PROPAGATION2(MaxArrayCtTest, SetOneVar, s, true, IntExpr*, max_,
                     std::vector<IntVar*>, vars_) {
  vars_[9]->SetValue(18);
  EXPECT_EQ(18, max_->Min()) << "bad computed min in max_array";
  EXPECT_EQ(21, max_->Max()) << "bad computed max in max_array";
  return nullptr;
};

TEST_F(MaxArrayCtTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  max_->WhenRange(d);
}

TEST_F(MaxArrayCtTest, BigMaxVector) {
  std::vector<IntVar*> vars;
  for (int i = 0; i < 1001; ++i) {
    vars.push_back(solver_->MakeIntVar(i, 3000 - i, absl::StrFormat("x%d", i)));
  }
  IntExpr* expr = solver_->MakeMax(vars);
  EXPECT_EQ(3000, expr->Max());
  EXPECT_EQ(1000, expr->Min());
}

TEST_F(MaxArrayCtTest, SmallMaxVector) {
  std::vector<IntVar*> vars;
  IntExpr* expr = solver_->MakeMax(vars);
  EXPECT_EQ(kint64min, expr->Min());
  EXPECT_EQ(kint64min, expr->Max());
  vars.push_back(solver_->MakeIntVar(1, 10, "x0"));
  expr = solver_->MakeMax(vars);
  EXPECT_EQ(1, expr->Min());
  EXPECT_EQ(10, expr->Max());
  vars.push_back(solver_->MakeIntVar(2, 9, "x1"));
  expr = solver_->MakeMax(vars);
  EXPECT_EQ(2, expr->Min());
  EXPECT_EQ(10, expr->Max());
  vars.push_back(solver_->MakeIntVar(3, 8, "x2"));
  expr = solver_->MakeMax(vars);
  EXPECT_EQ(3, expr->Min());
  EXPECT_EQ(10, expr->Max());
}

// ----- SumBooleanGreaterOrEqualToOne -----

class SumBooleanGreaterOrEqualToOneTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("SumBooleanGreaterOrEqualToOneTest");
    solver_->MakeBoolVarArray(10, "vars_", &vars_);
    ct_ = solver_->MakeSumGreaterOrEqual(vars_, 1);
    solver_->AddConstraint(ct_);
  }

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  Constraint* ct_;
};

TEST_IN_PROPAGATION1(SumBooleanGreaterOrEqualToOneTest, SetToMin, s, true,
                     std::vector<IntVar*>, vars_) {
  for (int i = 0; i < vars_.size() - 1; ++i) {
    vars_[i]->SetMax(0);
  }
  EXPECT_EQ(1, vars_.back()->Min()) << "var not bound to the correct value";
  return nullptr;
};

TEST_IN_PROPAGATION1(SumBooleanGreaterOrEqualToOneTest, SetToMax, s, true,
                     std::vector<IntVar*>, vars_) {
  for (int i = 0; i < vars_.size() - 1; ++i) {
    vars_[i]->SetMin(1);
  }
  EXPECT_EQ(0, vars_.back()->Min()) << "var not bound to the correct value";
  EXPECT_EQ(1, vars_.back()->Max()) << "var not bound to the correct value";
  return nullptr;
};

TEST_IN_PROPAGATION1(SumBooleanGreaterOrEqualToOneTest, FailBelowOne, s, false,
                     std::vector<IntVar*>, vars_) {
  for (int i = 0; i < vars_.size(); ++i) {
    vars_[i]->SetMax(0);
  }
  return nullptr;
};

// ----- Big sum -----

class BigSumBooleanGreaterOrEqualToOneTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("BigSumBooleanGreaterOrEqualToOneTest");
    solver_->MakeBoolVarArray(100, "vars_", &vars_);
    ct_ = solver_->MakeSumGreaterOrEqual(vars_, 1);
    solver_->AddConstraint(ct_);
  }

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  Constraint* ct_;
};

TEST_IN_PROPAGATION1(BigSumBooleanGreaterOrEqualToOneTest, SetToMin, s, true,
                     std::vector<IntVar*>, vars_) {
  for (int i = 0; i < vars_.size() - 1; ++i) {
    vars_[i]->SetMax(0);
  }
  EXPECT_EQ(1, vars_.back()->Min()) << "var not bound to the correct value";
  return nullptr;
};

TEST_IN_PROPAGATION1(BigSumBooleanGreaterOrEqualToOneTest, SetToMax, s, true,
                     std::vector<IntVar*>, vars_) {
  for (int i = 0; i < vars_.size() - 1; ++i) {
    vars_[i]->SetMin(1);
  }
  EXPECT_EQ(0, vars_.back()->Min()) << "var not bound to the correct value";
  EXPECT_EQ(1, vars_.back()->Max()) << "var not bound to the correct value";
  return nullptr;
};

TEST_IN_PROPAGATION1(BigSumBooleanGreaterOrEqualToOneTest, FailBelowOne, s,
                     false, std::vector<IntVar*>, vars_) {
  for (int i = 0; i < vars_.size(); ++i) {
    vars_[i]->SetMax(0);
  }
  return nullptr;
}

// ----- SumBooleanLessOrEqualToOne -----

class SumBooleanLessOrEqualToOneTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("SumBooleanLessOrEqualToOneTest");
    solver_->MakeBoolVarArray(10, "vars_", &vars_);
    ct_ = solver_->MakeSumLessOrEqual(vars_, 1);
    solver_->AddConstraint(ct_);
  }

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  Constraint* ct_;
};

TEST_IN_PROPAGATION1(SumBooleanLessOrEqualToOneTest, SetToMin, s, true,
                     std::vector<IntVar*>, vars_) {
  for (int i = 0; i < vars_.size() - 1; ++i) {
    vars_[i]->SetMax(0);
  }
  EXPECT_EQ(0, vars_.back()->Min()) << "var not bound to the correct value";
  EXPECT_EQ(1, vars_.back()->Max()) << "var not bound to the correct value";
  return nullptr;
};

TEST_IN_PROPAGATION1(SumBooleanLessOrEqualToOneTest, SetToMax, s, true,
                     std::vector<IntVar*>, vars_) {
  vars_.back()->SetMin(1);
  for (int i = 0; i < vars_.size() - 1; ++i) {
    EXPECT_EQ(0, vars_[i]->Max()) << "var not bound to the correct value";
  }
  return nullptr;
};

TEST_IN_PROPAGATION1(SumBooleanLessOrEqualToOneTest, FailAboveOne, s, false,
                     std::vector<IntVar*>, vars_) {
  vars_[0]->SetMin(1);
  vars_[1]->SetMin(1);
  return nullptr;
}

// ----- SumBooleanEqualToOne -----

class SumBooleanEqualToOneTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("SumBooleanEqualToOneTest");
    solver_->MakeBoolVarArray(10, "vars_", &vars_);
    ct_ = solver_->MakeSumEquality(vars_, 1);
    solver_->AddConstraint(ct_);
  }

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  Constraint* ct_;
};

TEST_IN_PROPAGATION1(SumBooleanEqualToOneTest, SetToMin, s, true,
                     std::vector<IntVar*>, vars_) {
  for (int i = 0; i < vars_.size() - 1; ++i) {
    vars_[i]->SetMax(0);
  }
  EXPECT_EQ(1, vars_.back()->Min()) << "var not bound to the correct value";
  return nullptr;
};

TEST_IN_PROPAGATION1(SumBooleanEqualToOneTest, SetToMax, s, true,
                     std::vector<IntVar*>, vars_) {
  vars_.back()->SetMin(1);
  for (int i = 0; i < vars_.size() - 1; ++i) {
    EXPECT_EQ(0, vars_[i]->Max()) << "var not bound to the correct value";
  }
  return nullptr;
};

TEST_IN_PROPAGATION1(SumBooleanEqualToOneTest, FailAboveOne, s, false,
                     std::vector<IntVar*>, vars_) {
  vars_[0]->SetMin(1);
  vars_[1]->SetMin(1);
  return nullptr;
}

TEST_IN_PROPAGATION1(SumBooleanEqualToOneTest, FailBelowOne, s, false,
                     std::vector<IntVar*>, vars_) {
  for (int i = 0; i < vars_.size(); ++i) {
    vars_[i]->SetMax(0);
  }
  return nullptr;
}

// ----- SumBooleanEqualToCst -----

namespace {
const int kArraySize = 10;
}  // namespace
class SumBooleanEqualToCstTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("SumBooleanEqualToCstTest");
    solver_->MakeBoolVarArray(kArraySize, "vars_", &vars_);
    ct_ = solver_->MakeSumEquality(vars_, 2);
    solver_->AddConstraint(ct_);
  }

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  Constraint* ct_;
};

TEST_IN_PROPAGATION1(SumBooleanEqualToCstTest, SetToMin, s, true,
                     std::vector<IntVar*>, vars_) {
  for (int i = 0; i < vars_.size() - 2; ++i) {
    vars_[i]->SetMax(0);
  }
  EXPECT_EQ(1, vars_[vars_.size() - 2]->Min());
  EXPECT_EQ(1, vars_[vars_.size() - 1]->Min());
  return nullptr;
};

TEST_IN_PROPAGATION1(SumBooleanEqualToCstTest, SetToMax, s, true,
                     std::vector<IntVar*>, vars_) {
  vars_[vars_.size() - 1]->SetMin(1);
  for (int i = 0; i < vars_.size() - 1; ++i) {
    EXPECT_EQ(1, vars_[i]->Max());
  }
  vars_[vars_.size() - 2]->SetMin(1);
  for (int i = 0; i < vars_.size() - 2; ++i) {
    EXPECT_EQ(0, vars_[i]->Max()) << "var not bound to the correct value";
  }
  return nullptr;
};

TEST_IN_PROPAGATION1(SumBooleanEqualToCstTest, FailAboveTwo, s, false,
                     std::vector<IntVar*>, vars_) {
  vars_[0]->SetMin(1);
  vars_[1]->SetMin(1);
  vars_[2]->SetMin(1);
  return nullptr;
}

TEST_IN_PROPAGATION1(SumBooleanEqualToCstTest, FailBelowTwo, s, false,
                     std::vector<IntVar*>, vars_) {
  for (int i = 0; i < vars_.size() - 1; ++i) {
    vars_[i]->SetMax(0);
  }
  return nullptr;
}

// ----- Max Bool Array Ct Test -----

class MaxBoolArrayCtTest : public testing::Test {
 protected:
  void SetUp() override {
    ConstraintSolverParameters params = Solver::DefaultSolverParameters();
#if defined(SMALL_ARRAY)
    params.set_array_split_size(3);
#endif
    solver_ = std::make_unique<Solver>("MaxBoolArrayCtTest", params);
    solver_->MakeBoolVarArray(kArraySize, "var_", &vars_);
    max_ = solver_->MakeMax(vars_)->Var();
  }

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  IntExpr* max_;
};

TEST_F(MaxBoolArrayCtTest, Bounds) {
  EXPECT_EQ(0, max_->Min()) << "bad computed min in max_array";
  EXPECT_EQ(1, max_->Max()) << "bad computed min in max_array";
}

TEST_IN_PROPAGATION2(MaxBoolArrayCtTest, SetToZero, s, true,
                     std::vector<IntVar*>, vars_, IntExpr*, max_) {
  max_->SetMax(0);
  for (int i = 0; i < kArraySize; ++i) {
    EXPECT_TRUE(vars_[i]->Bound()) << "var not bound";
    EXPECT_EQ(0, vars_[i]->Max()) << "var not bound to the correct value";
  }
  return nullptr;
};

TEST_IN_PROPAGATION2(MaxBoolArrayCtTest, SetOneVar, s, true,
                     std::vector<IntVar*>, vars_, IntExpr*, max_) {
  s->AddConstraint(s->MakeEquality(max_, int64_t{1}));
  for (int i = 1; i < kArraySize; ++i) {
    EXPECT_FALSE(vars_[i]->Bound());
  }
  for (int i = 1; i < kArraySize; ++i) {
    vars_[i]->SetValue(0);
  }
  EXPECT_TRUE(vars_[0]->Bound()) << "var not bound";
  EXPECT_EQ(1, vars_[0]->Min()) << "var not bound to the correct value";
  return nullptr;
};

class MaxBoolArrayCtTestSetOneVar2 : public DecisionBuilder {
 public:
  MaxBoolArrayCtTestSetOneVar2(IntExpr* const max,
                               const std::vector<IntVar*>& vars, int start,
                               int end, int extra)
      : max_(max), vars_(vars), start_(start), end_(end), extra_(extra) {}
  ~MaxBoolArrayCtTestSetOneVar2() override {}

  Decision* Next(Solver* const s) override {
    for (int i = 1; i < kArraySize; ++i) {
      EXPECT_FALSE(vars_[i]->Bound());
    }
    for (int i = start_; i <= end_; ++i) {
      vars_[i]->SetValue(0);
    }
    s->AddConstraint(s->MakeEquality(max_, int64_t{1}));
    EXPECT_TRUE(vars_[extra_]->Bound()) << "var not bound";
    EXPECT_EQ(1, vars_[extra_]->Min()) << "var not bound to the correct value";
    return nullptr;
  }

 private:
  IntExpr* const max_;
  const std::vector<IntVar*>& vars_;
  const int start_;
  const int end_;
  const int extra_;
};

TEST_F(MaxBoolArrayCtTest, SetOneVar2) {
  solver_->Solve(solver_->RevAlloc(new MaxBoolArrayCtTestSetOneVar2(
      max_, vars_, 0, kArraySize - 2, kArraySize - 1)));
}

TEST_F(MaxBoolArrayCtTest, SetOneVar3) {
  solver_->Solve(solver_->RevAlloc(
      new MaxBoolArrayCtTestSetOneVar2(max_, vars_, 1, kArraySize - 1, 0)));
}

TEST_F(MaxBoolArrayCtTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  max_->WhenRange(d);
}

// ----- Min Bool Array Ct Test -----

class MinBoolArrayCtTest : public testing::Test {
 protected:
  void SetUp() override {
    ConstraintSolverParameters params = Solver::DefaultSolverParameters();
#if defined(SMALL_ARRAY)
    params.set_array_split_size(3);
#endif
    solver_ = std::make_unique<Solver>("MinBoolArrayCtTest", params);
    solver_->MakeBoolVarArray(kArraySize, "var_", &vars_);
    min_ = solver_->MakeMin(vars_)->Var();
  }

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  IntExpr* min_;
};

TEST_F(MinBoolArrayCtTest, Bounds) {
  EXPECT_EQ(0, min_->Min()) << "bad computed min in min_array";
  EXPECT_EQ(1, min_->Max()) << "bad computed min in min_array";
}

TEST_IN_PROPAGATION2(MinBoolArrayCtTest, SetToOne, s, true,
                     std::vector<IntVar*>, vars_, IntExpr*, min_) {
  min_->SetMin(int64_t{1});
  for (int i = 0; i < kArraySize; ++i) {
    EXPECT_TRUE(vars_[i]->Bound()) << "var not bound";
    EXPECT_EQ(int64_t{1}, vars_[i]->Max())
        << "var not bound to the correct value";
  }
  return nullptr;
};

TEST_IN_PROPAGATION2(MinBoolArrayCtTest, SetOneVar, s, true,
                     std::vector<IntVar*>, vars_, IntExpr*, min_) {
  s->AddConstraint(s->MakeEquality(min_, int64_t{0}));
  for (int i = 1; i < kArraySize; ++i) {
    EXPECT_FALSE(vars_[i]->Bound());
  }
  for (int i = 1; i < kArraySize; ++i) {
    vars_[i]->SetValue(int64_t{1});
  }
  EXPECT_TRUE(vars_[0]->Bound()) << "var not bound";
  EXPECT_EQ(0, vars_[0]->Min()) << "var not bound to the correct value";
  return nullptr;
};

class MinBoolArrayCtTestSetOneVar2 : public DecisionBuilder {
 public:
  MinBoolArrayCtTestSetOneVar2(IntExpr* const max,
                               const std::vector<IntVar*>& vars, int start,
                               int end, int extra)
      : min_(max), vars_(vars), start_(start), end_(end), extra_(extra) {}
  ~MinBoolArrayCtTestSetOneVar2() override {}

  Decision* Next(Solver* const s) override {
    for (int i = 1; i < kArraySize; ++i) {
      EXPECT_FALSE(vars_[i]->Bound());
    }
    for (int i = start_; i <= end_; ++i) {
      vars_[i]->SetValue(int64_t{1});
    }
    s->AddConstraint(s->MakeEquality(min_, int64_t{0}));
    EXPECT_TRUE(vars_[extra_]->Bound()) << "var not bound";
    EXPECT_EQ(int64_t{0}, vars_[extra_]->Min());
    return nullptr;
  }

 private:
  IntExpr* const min_;
  const std::vector<IntVar*>& vars_;
  const int start_;
  const int end_;
  const int extra_;
};

TEST_F(MinBoolArrayCtTest, SetOneVar2) {
  solver_->Solve(solver_->RevAlloc(new MinBoolArrayCtTestSetOneVar2(
      min_, vars_, 0, kArraySize - 2, kArraySize - 1)));
}

TEST_F(MinBoolArrayCtTest, SetOneVar3) {
  solver_->Solve(solver_->RevAlloc(
      new MinBoolArrayCtTestSetOneVar2(min_, vars_, 1, kArraySize - 1, 0)));
}

TEST_F(MinBoolArrayCtTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  min_->WhenRange(d);
}

// ----- Null coefficients in ScalProd -----

#define TEST_SCAL_PROD_nullptr_COEFFICIENTS(name, type, fct, build_array) \
  TEST(ScalProdTest, Null##name##type##fct##Coefficients) {               \
    int kSize = 10;                                                       \
    Solver solver("TestNullCoefficients");                                \
    std::vector<IntVar*> vars;                                            \
    build_array;                                                          \
    std::vector<type> coefficients(kSize);                                \
    for (int i = 0; i < kSize; ++i) {                                     \
      coefficients[i] = 0ULL;                                             \
    }

#define TEST_SCAL_PROD_EXP_nullptr_COEFFICIENTS(name, type, build_array)     \
  TEST_SCAL_PROD_nullptr_COEFFICIENTS(name, type, MakeScalProd, build_array) \
      EXPECT_TRUE(solver.MakeScalProd(vars, coefficients) != nullptr);       \
  EXPECT_TRUE(solver.MakeScalProd(vars, coefficients)->Var() != nullptr);    \
  }

TEST_SCAL_PROD_EXP_nullptr_COEFFICIENTS(
    Integer, int,
    solver.MakeIntVarArray(
        kSize, 0, 10,
        &vars)) TEST_SCAL_PROD_EXP_nullptr_COEFFICIENTS(Integer, int64_t,
                                                        solver.MakeIntVarArray(
                                                            kSize, 0, 10,
                                                            &vars))
    TEST_SCAL_PROD_EXP_nullptr_COEFFICIENTS(
        Boolean, int,
        solver.MakeBoolVarArray(
            kSize,
            &vars)) TEST_SCAL_PROD_EXP_nullptr_COEFFICIENTS(Boolean, int64_t,
                                                            solver
                                                                .MakeBoolVarArray(
                                                                    kSize,
                                                                    &vars))
#undef TEST_SCAL_PROD_EXP_nullptr_COEFFICIENTS

#define TEST_SCAL_PROD_LE_nullptr_COEFFICIENTS(name, type, build_array)    \
  TEST_SCAL_PROD_nullptr_COEFFICIENTS(name, type, MakeScalProdLessOrEqual, \
                                      build_array)                         \
      EXPECT_EQ(solver.MakeScalProdLessOrEqual(vars, coefficients, -1),    \
                solver.MakeFalseConstraint());                             \
  EXPECT_EQ(solver.MakeScalProdLessOrEqual(vars, coefficients, 0),         \
            solver.MakeTrueConstraint());                                  \
  }  // NOLINT

        TEST_SCAL_PROD_LE_nullptr_COEFFICIENTS(
            Integer, int,
            solver.MakeIntVarArray(
                kSize, 0, 10,
                &vars)) TEST_SCAL_PROD_LE_nullptr_COEFFICIENTS(Integer, int64_t,
                                                               solver
                                                                   .MakeIntVarArray(
                                                                       kSize, 0,
                                                                       10,
                                                                       &vars))
            TEST_SCAL_PROD_LE_nullptr_COEFFICIENTS(
                Boolean, int, solver.MakeBoolVarArray(kSize, &vars))
                TEST_SCAL_PROD_LE_nullptr_COEFFICIENTS(
                    Boolean, int64_t, solver.MakeBoolVarArray(kSize, &vars))

#undef TEST_SCAL_PROD_LE_nullptr_COEFFICIENTS

#define TEST_SCAL_PROD_GE_nullptr_COEFFICIENTS(name, type, build_array)       \
  TEST_SCAL_PROD_nullptr_COEFFICIENTS(name, type, MakeScalProdGreaterOrEqual, \
                                      build_array)                            \
      EXPECT_EQ(solver.MakeScalProdGreaterOrEqual(vars, coefficients, 0),     \
                solver.MakeTrueConstraint());                                 \
  EXPECT_EQ(solver.MakeScalProdGreaterOrEqual(vars, coefficients, 1),         \
            solver.MakeFalseConstraint());                                    \
  }  // NOLINT

                    TEST_SCAL_PROD_GE_nullptr_COEFFICIENTS(
                        Integer, int,
                        solver.MakeIntVarArray(kSize, 0, 10, &vars))
                        TEST_SCAL_PROD_GE_nullptr_COEFFICIENTS(
                            Integer, int64_t,
                            solver.MakeIntVarArray(kSize, 0, 10, &vars))
                            TEST_SCAL_PROD_GE_nullptr_COEFFICIENTS(
                                Boolean, int,
                                solver.MakeBoolVarArray(kSize, &vars))
                                TEST_SCAL_PROD_GE_nullptr_COEFFICIENTS(
                                    Boolean, int64_t,
                                    solver.MakeBoolVarArray(kSize, &vars))

#undef TEST_SCAL_PROD_GE_nullptr_COEFFICIENTS

#define TEST_SCAL_PROD_EQ_nullptr_COEFFICIENTS(name, type, build_array)  \
  TEST_SCAL_PROD_nullptr_COEFFICIENTS(name, type, MakeScalProdEquality,  \
                                      build_array)                       \
      EXPECT_EQ(solver.MakeScalProdEquality(vars, coefficients, Zero()), \
                solver.MakeTrueConstraint());                            \
  EXPECT_EQ(solver.MakeScalProdEquality(vars, coefficients, 1),          \
            solver.MakeFalseConstraint());                               \
  }  // NOLINT

                                    TEST_SCAL_PROD_EQ_nullptr_COEFFICIENTS(
                                        Integer, int,
                                        solver.MakeIntVarArray(kSize, 0, 10,
                                                               &vars))
                                        TEST_SCAL_PROD_EQ_nullptr_COEFFICIENTS(
                                            Integer, int64_t,
                                            solver.MakeIntVarArray(kSize, 0, 10,
                                                                   &vars))
                                            TEST_SCAL_PROD_EQ_nullptr_COEFFICIENTS(
                                                Boolean, int,
                                                solver.MakeBoolVarArray(kSize,
                                                                        &vars))
                                                TEST_SCAL_PROD_EQ_nullptr_COEFFICIENTS(
                                                    Boolean, int64_t,
                                                    solver.MakeBoolVarArray(
                                                        kSize, &vars))

#undef TEST_SCAL_PROD_EQ_nullptr_COEFFICIENTS

#undef TEST_SCAL_PROD_nullptr_COEFFICIENTS

    // ----- BooleanScalProdTest -----

    class BaseBooleanScalProdTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("BooleanScalProdTest");
    solver_->MakeBoolVarArray(10, &vars_);
    coefficients_.resize(10);
    for (int i = 0; i < 10; ++i) {
      coefficients_[i] = Coefficient(i);
    }
    sum_ = solver_->MakeScalProd(vars_, coefficients_);
  }

  void TearDown() override {}
  virtual int64_t Coefficient(int index) const = 0;

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  std::vector<int64_t> coefficients_;
  IntExpr* sum_;
};

class PositiveBooleanScalProdTest : public BaseBooleanScalProdTest {
 protected:
  int64_t Coefficient(int index) const override { return index + 1; }
};

TEST_F(PositiveBooleanScalProdTest, Bounds) {
  EXPECT_EQ(0, sum_->Min()) << "bad computed min in sum_array";
  EXPECT_EQ(55, sum_->Max()) << "bad computed min in sum_array";
}

TEST_IN_PROPAGATION2(PositiveBooleanScalProdTest, SetToMin, solver, true,
                     IntExpr*, sum_, const std::vector<IntVar*>&, vars_) {
  sum_->SetMax(0);
  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(vars_[i]->Bound()) << "var not bound";
    EXPECT_EQ(int64_t{0}, vars_[i]->Min())
        << "var not bound to the correct value";
  }
  return nullptr;
}

TEST_IN_PROPAGATION2(PositiveBooleanScalProdTest, SetToMax, solver, true,
                     IntExpr*, sum_, const std::vector<IntVar*>&, vars_) {
  sum_->SetMin(55);
  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(vars_[i]->Bound()) << "var not bound";
    EXPECT_EQ(int64_t{1}, vars_[i]->Min())
        << "var not bound to the correct value";
  }
  return nullptr;
}

TEST_IN_PROPAGATION2(PositiveBooleanScalProdTest, SetOneVarDown, solver, true,
                     IntExpr*, sum_, const std::vector<IntVar*>&, vars_) {
  vars_[7]->SetValue(0);
  EXPECT_EQ(0, sum_->Min()) << "bad computed min in sum_array";
  EXPECT_EQ(47, sum_->Max()) << "bad computed max in sum_array";
  return nullptr;
}

TEST_IN_PROPAGATION2(PositiveBooleanScalProdTest, SetOneVarUp, solver, true,
                     IntExpr*, sum_, const std::vector<IntVar*>&, vars_) {
  vars_[7]->SetValue(1);
  EXPECT_EQ(8, sum_->Min()) << "bad computed min in sum_array";
  EXPECT_EQ(55, sum_->Max()) << "bad computed max in sum_array";
  return nullptr;
}

TEST_IN_PROPAGATION0(PositiveBooleanScalProdTest, ReductionDown, solver, true) {
  std::vector<IntVar*> vars;
  solver->MakeBoolVarArray(4, &vars);
  std::vector<int64_t> coefficients;
  coefficients.push_back(100);
  coefficients.push_back(10);
  coefficients.push_back(2);
  coefficients.push_back(1);
  IntExpr* const scal_prod = solver->MakeScalProd(vars, coefficients);
  scal_prod->SetMax(101);
  EXPECT_FALSE(vars[0]->Bound());
  EXPECT_FALSE(vars[1]->Bound());
  EXPECT_FALSE(vars[2]->Bound());
  EXPECT_FALSE(vars[3]->Bound());
  scal_prod->SetMax(99);
  EXPECT_TRUE(vars[0]->Bound());
  EXPECT_EQ(int64_t{0}, vars[0]->Min());
  EXPECT_FALSE(vars[1]->Bound());
  EXPECT_FALSE(vars[2]->Bound());
  EXPECT_FALSE(vars[3]->Bound());
  scal_prod->SetMax(9);
  EXPECT_TRUE(vars[0]->Bound());
  EXPECT_TRUE(vars[1]->Bound());
  EXPECT_EQ(int64_t{0}, vars[1]->Min());
  EXPECT_FALSE(vars[2]->Bound());
  EXPECT_FALSE(vars[3]->Bound());
  return nullptr;
}

TEST_IN_PROPAGATION0(PositiveBooleanScalProdTest, ReductionUp, solver, true) {
  std::vector<IntVar*> vars;
  solver->MakeBoolVarArray(4, &vars);
  std::vector<int64_t> coefficients;
  coefficients.push_back(100);
  coefficients.push_back(10);
  coefficients.push_back(2);
  coefficients.push_back(1);
  IntExpr* const scal_prod = solver->MakeScalProd(vars, coefficients);
  scal_prod->SetMin(9);
  EXPECT_FALSE(vars[0]->Bound());
  EXPECT_FALSE(vars[1]->Bound());
  EXPECT_FALSE(vars[2]->Bound());
  EXPECT_FALSE(vars[3]->Bound());
  scal_prod->SetMin(15);
  EXPECT_TRUE(vars[0]->Bound());
  EXPECT_EQ(int64_t{1}, vars[0]->Min());
  EXPECT_FALSE(vars[1]->Bound());
  EXPECT_FALSE(vars[2]->Bound());
  EXPECT_FALSE(vars[3]->Bound());
  scal_prod->SetMin(111);
  EXPECT_TRUE(vars[0]->Bound());
  EXPECT_TRUE(vars[1]->Bound());
  EXPECT_EQ(int64_t{1}, vars[1]->Min());
  EXPECT_FALSE(vars[2]->Bound());
  EXPECT_FALSE(vars[3]->Bound());
  return nullptr;
}

TEST_F(PositiveBooleanScalProdTest, When) {
  Demon* d = solver_->RevAlloc(new NullDemon());
  sum_->WhenRange(d);
}

class BooleanScalProdTest : public BaseBooleanScalProdTest {
 protected:
  int64_t Coefficient(int index) const override {
    if (index % 2 == 0) {
      return index + 1;
    } else {
      return -index;
    }
  }
};

TEST_F(BooleanScalProdTest, Bounds) {
  EXPECT_EQ(-25, sum_->Min());
  EXPECT_EQ(25, sum_->Max());
}

TEST_IN_PROPAGATION3(BooleanScalProdTest, SetToMin, s, true,
                     const std::vector<IntVar*>&, vars_,
                     const std::vector<int64_t>&, coefficients_, IntExpr*,
                     sum_) {
  sum_->SetMax(-25);
  for (int i = 0; i < 10; ++i) {
    if (coefficients_[i] > 0) {
      EXPECT_EQ(int64_t{0}, vars_[i]->Value());
    } else {
      EXPECT_EQ(int64_t{1}, vars_[i]->Value());
    }
  }
  return nullptr;
}

TEST_IN_PROPAGATION3(BooleanScalProdTest, SetToMax, s, true,
                     const std::vector<IntVar*>&, vars_,
                     const std::vector<int64_t>&, coefficients_, IntExpr*,
                     sum_) {
  sum_->SetMin(25);
  for (int i = 0; i < 10; ++i) {
    if (coefficients_[i] > 0) {
      EXPECT_EQ(int64_t{1}, vars_[i]->Value());
    } else {
      EXPECT_EQ(int64_t{0}, vars_[i]->Value());
    }
  }
  return nullptr;
}

TEST_IN_PROPAGATION2(BooleanScalProdTest, SetOneNegVarDown, s, true,
                     const std::vector<IntVar*>&, vars_, IntExpr*, sum_) {
  vars_[7]->SetValue(0);
  EXPECT_EQ(-18, sum_->Min());
  EXPECT_EQ(25, sum_->Max());
  return nullptr;
}

TEST_IN_PROPAGATION2(BooleanScalProdTest, SetOnePosVarDown, s, true,
                     const std::vector<IntVar*>&, vars_, IntExpr*, sum_) {
  vars_[6]->SetValue(0);
  EXPECT_EQ(-25, sum_->Min());
  EXPECT_EQ(18, sum_->Max());
  return nullptr;
}

TEST_IN_PROPAGATION2(BooleanScalProdTest, SetOneNegVarUp, s, true,
                     const std::vector<IntVar*>&, vars_, IntExpr*, sum_) {
  vars_[7]->SetValue(1);
  EXPECT_EQ(-25, sum_->Min());
  EXPECT_EQ(18, sum_->Max());
  return nullptr;
}

TEST_IN_PROPAGATION2(BooleanScalProdTest, SetOnePosVarUp, s, true,
                     const std::vector<IntVar*>&, vars_, IntExpr*, sum_) {
  vars_[6]->SetValue(1);
  EXPECT_EQ(-18, sum_->Min());
  EXPECT_EQ(25, sum_->Max());
  return nullptr;
}

class NegativeBooleanScalProdTest : public BaseBooleanScalProdTest {
 protected:
  int64_t Coefficient(int index) const override { return -(index + 1); }
};

TEST_F(NegativeBooleanScalProdTest, Bounds) {
  EXPECT_EQ(-55, sum_->Min());
  EXPECT_EQ(0, sum_->Max());
}

TEST_IN_PROPAGATION2(NegativeBooleanScalProdTest, SetToMin, s, true,
                     const std::vector<IntVar*>&, vars_, IntExpr*, sum_) {
  sum_->SetMax(-55);
  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(vars_[i]->Bound());
    EXPECT_EQ(int64_t{1}, vars_[i]->Min());
  }
  return nullptr;
}

TEST_IN_PROPAGATION2(NegativeBooleanScalProdTest, SetToMax, s, true,
                     const std::vector<IntVar*>&, vars_, IntExpr*, sum_) {
  sum_->SetMin(0);
  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(vars_[i]->Bound());
    EXPECT_EQ(int64_t{0}, vars_[i]->Min());
  }
  return nullptr;
}

TEST_IN_PROPAGATION2(NegativeBooleanScalProdTest, SetOneVarDown, s, true,
                     const std::vector<IntVar*>&, vars_, IntExpr*, sum_) {
  vars_[7]->SetValue(0);
  EXPECT_EQ(-47, sum_->Min());
  EXPECT_EQ(0, sum_->Max());
  return nullptr;
}

TEST_IN_PROPAGATION2(NegativeBooleanScalProdTest, SetOneVarUp, s, true,
                     const std::vector<IntVar*>&, vars_, IntExpr*, sum_) {
  vars_[7]->SetValue(1);
  EXPECT_EQ(-55, sum_->Min());
  EXPECT_EQ(-8, sum_->Max());
  return nullptr;
}

class BooleanScalProdCstTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("BooleanScalProdCstTest");
    solver_->MakeBoolVarArray(10, &vars_);
    vars_.push_back(solver_->MakeIntConst(int64_t{1}));
    std::vector<int64_t> coefficients(11);
    for (int i = 0; i < 11; ++i) {
      coefficients[i] = i + 1;
    }
    sum_ = solver_->MakeScalProd(vars_, coefficients);
  }

  void TearDown() override {}

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  IntExpr* sum_;
};

TEST_F(BooleanScalProdCstTest, Bounds) {
  EXPECT_EQ(11, sum_->Min()) << "bad computed min in sum_array";
  EXPECT_EQ(66, sum_->Max()) << "bad computed min in sum_array";
}

TEST_IN_PROPAGATION2(BooleanScalProdCstTest, SetToMin, s, true,
                     const std::vector<IntVar*>&, vars_, IntExpr*, sum_) {
  sum_->SetMax(11);
  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(vars_[i]->Bound()) << "var not bound";
    EXPECT_EQ(int64_t{0}, vars_[i]->Min())
        << "var not bound to the correct value";
  }
  return nullptr;
}

TEST_IN_PROPAGATION2(BooleanScalProdCstTest, SetToMax, s, true,
                     const std::vector<IntVar*>&, vars_, IntExpr*, sum_) {
  sum_->SetMin(66);
  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(vars_[i]->Bound()) << "var not bound";
    EXPECT_EQ(int64_t{1}, vars_[i]->Min())
        << "var not bound to the correct value";
  }
  return nullptr;
}

TEST_IN_PROPAGATION2(BooleanScalProdCstTest, SetOneVarDown, s, true,
                     const std::vector<IntVar*>&, vars_, IntExpr*, sum_) {
  vars_[7]->SetValue(0);
  EXPECT_EQ(11, sum_->Min()) << "bad computed min in sum_array";
  EXPECT_EQ(58, sum_->Max()) << "bad computed max in sum_array";
  return nullptr;
}

TEST_IN_PROPAGATION2(BooleanScalProdCstTest, SetOneVarUp, s, true,
                     const std::vector<IntVar*>&, vars_, IntExpr*, sum_) {
  vars_[7]->SetValue(1);
  EXPECT_EQ(19, sum_->Min()) << "bad computed min in sum_array";
  EXPECT_EQ(66, sum_->Max()) << "bad computed max in sum_array";
  return nullptr;
}

// ----- BooleanScalProdCtTest -----

class BooleanScalProdCtTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("BooleanScalProdCtTest");
    solver_->MakeBoolVarArray(10, &vars_);
    std::vector<int64_t> coefficients(10);
    for (int i = 0; i < 10; ++i) {
      coefficients[i] = i + 1;
    }
    sum_ = solver_->MakeScalProd(vars_, coefficients)->Var();
  }

  void TearDown() override {}

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  IntExpr* sum_;
};

TEST_F(BooleanScalProdCtTest, Bounds) {
  EXPECT_EQ(0, sum_->Min()) << "bad computed min in sum_array";
  EXPECT_EQ(55, sum_->Max()) << "bad computed min in sum_array";
}

TEST_IN_PROPAGATION2(BooleanScalProdCtTest, SetToMin, s, true,
                     const std::vector<IntVar*>&, vars_, IntExpr*, sum_) {
  sum_->SetMax(0);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(int64_t{0}, vars_[i]->Max());
  }
  return nullptr;
}

TEST_IN_PROPAGATION2(BooleanScalProdCtTest, SetToMax, s, true,
                     const std::vector<IntVar*>&, vars_, IntExpr*, sum_) {
  sum_->SetMin(55);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(int64_t{1}, vars_[i]->Min());
  }
  return nullptr;
}

TEST_IN_PROPAGATION2(BooleanScalProdCtTest, SetOneVarDown, s, true,
                     const std::vector<IntVar*>&, vars_, IntExpr*, sum_) {
  vars_[7]->SetValue(0);
  EXPECT_EQ(0, sum_->Min()) << "bad computed min in sum_array";
  EXPECT_EQ(47, sum_->Max()) << "bad computed max in sum_array";
  return nullptr;
}

TEST_IN_PROPAGATION2(BooleanScalProdCtTest, SetOneVarUp, s, true,
                     const std::vector<IntVar*>&, vars_, IntExpr*, sum_) {
  vars_[7]->SetValue(int64_t{1});
  EXPECT_EQ(8, sum_->Min()) << "bad computed min in sum_array";
  EXPECT_EQ(55, sum_->Max()) << "bad computed max in sum_array";
  return nullptr;
}

TEST_IN_PROPAGATION0(BooleanScalProdCtTest, ReductionDown, s, true) {
  std::vector<IntVar*> vars;
  s->MakeBoolVarArray(4, &vars);
  std::vector<int64_t> coefficients;
  coefficients.push_back(100);
  coefficients.push_back(10);
  coefficients.push_back(2);
  coefficients.push_back(1);
  IntExpr* const scal_prod = s->MakeScalProd(vars, coefficients);
  scal_prod->SetMax(101);
  EXPECT_FALSE(vars[0]->Bound());
  EXPECT_FALSE(vars[1]->Bound());
  EXPECT_FALSE(vars[2]->Bound());
  EXPECT_FALSE(vars[3]->Bound());
  scal_prod->SetMax(99);
  EXPECT_TRUE(vars[0]->Bound());
  EXPECT_EQ(int64_t{0}, vars[0]->Min());
  EXPECT_FALSE(vars[1]->Bound());
  EXPECT_FALSE(vars[2]->Bound());
  EXPECT_FALSE(vars[3]->Bound());
  scal_prod->SetMax(9);
  EXPECT_TRUE(vars[0]->Bound());
  EXPECT_TRUE(vars[1]->Bound());
  EXPECT_EQ(int64_t{0}, vars[1]->Min());
  EXPECT_FALSE(vars[2]->Bound());
  EXPECT_FALSE(vars[3]->Bound());
  return nullptr;
}

TEST_IN_PROPAGATION0(BooleanScalProdCtTest, ReductionUp, s, true) {
  std::vector<IntVar*> vars;
  s->MakeBoolVarArray(4, &vars);
  std::vector<int64_t> coefficients;
  coefficients.push_back(100);
  coefficients.push_back(10);
  coefficients.push_back(2);
  coefficients.push_back(1);
  IntExpr* const scal_prod = s->MakeScalProd(vars, coefficients);
  scal_prod->SetMin(9);
  EXPECT_FALSE(vars[0]->Bound());
  EXPECT_FALSE(vars[1]->Bound());
  EXPECT_FALSE(vars[2]->Bound());
  EXPECT_FALSE(vars[3]->Bound());
  scal_prod->SetMin(15);
  EXPECT_TRUE(vars[0]->Bound());
  EXPECT_EQ(int64_t{1}, vars[0]->Min());
  EXPECT_FALSE(vars[1]->Bound());
  EXPECT_FALSE(vars[2]->Bound());
  EXPECT_FALSE(vars[3]->Bound());
  scal_prod->SetMin(111);
  EXPECT_TRUE(vars[0]->Bound());
  EXPECT_TRUE(vars[1]->Bound());
  EXPECT_EQ(int64_t{1}, vars[1]->Min());
  EXPECT_FALSE(vars[2]->Bound());
  EXPECT_FALSE(vars[3]->Bound());
  return nullptr;
}

class BooleanScalProdCtCstTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("BooleanScalProdCtCstTest");
    solver_->MakeBoolVarArray(10, &vars_);
    vars_.push_back(solver_->MakeIntConst(int64_t{1}));
    std::vector<int64_t> coefficients(11);
    for (int i = 0; i < 11; ++i) {
      coefficients[i] = i + 1;
    }
    sum_ = solver_->MakeScalProd(vars_, coefficients)->Var();
  }

  void TearDown() override {}

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
  IntExpr* sum_;
};

TEST_F(BooleanScalProdCtCstTest, Bounds) {
  EXPECT_EQ(11, sum_->Min()) << "bad computed min in sum_array";
  EXPECT_EQ(66, sum_->Max()) << "bad computed min in sum_array";
}

TEST_IN_PROPAGATION2(BooleanScalProdCtCstTest, SetToMin, s, true,
                     const std::vector<IntVar*>&, vars_, IntExpr*, sum_) {
  sum_->SetMax(11);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(int64_t{0}, vars_[i]->Max());
  }
  return nullptr;
}

TEST_IN_PROPAGATION2(BooleanScalProdCtCstTest, SetToMax, s, true,
                     const std::vector<IntVar*>&, vars_, IntExpr*, sum_) {
  sum_->SetMin(66);
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(int64_t{1}, vars_[i]->Min());
  }
  return nullptr;
}

TEST_IN_PROPAGATION2(BooleanScalProdCtCstTest, SetOneVarDown, s, true,
                     const std::vector<IntVar*>&, vars_, IntExpr*, sum_) {
  vars_[7]->SetValue(0);
  EXPECT_EQ(11, sum_->Min()) << "bad computed min in sum_array";
  EXPECT_EQ(58, sum_->Max()) << "bad computed max in sum_array";
  return nullptr;
}

TEST_IN_PROPAGATION2(BooleanScalProdCtCstTest, SetOneVarUp, s, true,
                     const std::vector<IntVar*>&, vars_, IntExpr*, sum_) {
  vars_[7]->SetValue(int64_t{1});
  EXPECT_EQ(19, sum_->Min()) << "bad computed min in sum_array";
  EXPECT_EQ(66, sum_->Max()) << "bad computed max in sum_array";
  return nullptr;
}

class EmptyScalProdTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("EmptyScalProdTest");
  }
  std::unique_ptr<Solver> solver_;
};

TEST_F(EmptyScalProdTest, ScalProd) {
  std::vector<IntVar*> vars;
  std::vector<int64_t> coefs_64;
  std::vector<int> coefs;
  EXPECT_EQ(solver_->MakeScalProd(vars, coefs_64), solver_->MakeIntConst(0));
  EXPECT_EQ(solver_->MakeScalProd(vars, coefs), solver_->MakeIntConst(0));
}

TEST_F(EmptyScalProdTest, ScalProdEquality) {
  std::vector<IntVar*> vars;
  std::vector<int64_t> coefs_64;
  std::vector<int> coefs;
  EXPECT_EQ(solver_->MakeScalProdEquality(vars, coefs_64, Zero()),
            solver_->MakeTrueConstraint());
  EXPECT_EQ(solver_->MakeScalProdEquality(vars, coefs, Zero()),
            solver_->MakeTrueConstraint());
  EXPECT_EQ(solver_->MakeScalProdEquality(vars, coefs_64, 1),
            solver_->MakeFalseConstraint());
  EXPECT_EQ(solver_->MakeScalProdEquality(vars, coefs, 1),
            solver_->MakeFalseConstraint());
}

TEST_F(EmptyScalProdTest, ScalProdLessOrEqual) {
  std::vector<IntVar*> vars;
  std::vector<int64_t> coefs_64;
  std::vector<int> coefs;
  EXPECT_EQ(solver_->MakeScalProdLessOrEqual(vars, coefs_64, 0),
            solver_->MakeTrueConstraint());
  EXPECT_EQ(solver_->MakeScalProdLessOrEqual(vars, coefs, 0),
            solver_->MakeTrueConstraint());
  EXPECT_EQ(solver_->MakeScalProdLessOrEqual(vars, coefs_64, 1),
            solver_->MakeTrueConstraint());
  EXPECT_EQ(solver_->MakeScalProdLessOrEqual(vars, coefs, 1),
            solver_->MakeTrueConstraint());
  EXPECT_EQ(solver_->MakeScalProdLessOrEqual(vars, coefs_64, -1),
            solver_->MakeFalseConstraint());
  EXPECT_EQ(solver_->MakeScalProdLessOrEqual(vars, coefs, -1),
            solver_->MakeFalseConstraint());
}

TEST_F(EmptyScalProdTest, ScalProdGreaterOrEqual) {
  std::vector<IntVar*> vars;
  std::vector<int64_t> coefs_64;
  std::vector<int> coefs;
  EXPECT_EQ(solver_->MakeScalProdGreaterOrEqual(vars, coefs_64, 0),
            solver_->MakeTrueConstraint());
  EXPECT_EQ(solver_->MakeScalProdGreaterOrEqual(vars, coefs, 0),
            solver_->MakeTrueConstraint());
  EXPECT_EQ(solver_->MakeScalProdGreaterOrEqual(vars, coefs_64, 1),
            solver_->MakeFalseConstraint());
  EXPECT_EQ(solver_->MakeScalProdGreaterOrEqual(vars, coefs, 1),
            solver_->MakeFalseConstraint());
  EXPECT_EQ(solver_->MakeScalProdGreaterOrEqual(vars, coefs_64, -1),
            solver_->MakeTrueConstraint());
  EXPECT_EQ(solver_->MakeScalProdGreaterOrEqual(vars, coefs, -1),
            solver_->MakeTrueConstraint());
}

class BooleanScalProdEqCstTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("BooleanScalProdEqCstTest");
    solver_->MakeBoolVarArray(10, &vars_);
    std::vector<int64_t> coefficients(10);
    for (int i = 0; i < 10; ++i) {
      coefficients[i] = i + 1;
    }
    solver_->AddConstraint(
        solver_->MakeScalProdEquality(vars_, coefficients, 20));
  }

  void TearDown() override {}

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
};

TEST_IN_PROPAGATION1(BooleanScalProdEqCstTest, Propagate1, s, true,
                     const std::vector<IntVar*>&, vars_) {
  vars_[9]->SetValue(1);
  for (int i = 0; i < 9; ++i) {
    EXPECT_FALSE(vars_[i]->Bound());
  }
  vars_[8]->SetValue(1);
  for (int i = 0; i < 9; ++i) {
    EXPECT_TRUE(vars_[i]->Bound());
  }
  EXPECT_EQ(1, vars_[0]->Min());
  for (int i = 1; i < 8; ++i) {
    EXPECT_EQ(0, vars_[i]->Min());
  }
  return nullptr;
}

TEST_IN_PROPAGATION1(BooleanScalProdEqCstTest, Propagate2, s, true,
                     const std::vector<IntVar*>&, vars_) {
  vars_[0]->SetValue(0);
  for (int i = 1; i < 10; ++i) {
    EXPECT_FALSE(vars_[i]->Bound());
  }
  vars_[9]->SetValue(0);
  vars_[8]->SetValue(0);
  vars_[7]->SetValue(0);
  vars_[6]->SetValue(0);
  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(vars_[i]->Bound());
  }
  for (int i = 1; i < 6; ++i) {
    EXPECT_EQ(1, vars_[i]->Min());
  }
  return nullptr;
}

// BooleanScalProdLessCst

class BooleanScalProdLessCstTest : public testing::Test {
 protected:
  void SetUp() override {
    solver_ = std::make_unique<Solver>("BooleanScalProdLessCstTest");
    solver_->MakeBoolVarArray(10, &vars_);
    std::vector<int64_t> coefficients(10);
    for (int i = 0; i < 10; ++i) {
      coefficients[i] = i + 1;
    }
    solver_->AddConstraint(
        solver_->MakeScalProdLessOrEqual(vars_, coefficients, 20));
  }

  std::unique_ptr<Solver> solver_;
  std::vector<IntVar*> vars_;
};

TEST_IN_PROPAGATION1(BooleanScalProdLessCstTest, Propagate, s, true,
                     const std::vector<IntVar*>&, vars_) {
  vars_[9]->SetValue(1);
  for (int i = 0; i < 9; ++i) {
    EXPECT_FALSE(vars_[i]->Bound());
  }
  vars_[8]->SetValue(1);
  for (int i = 1; i < 9; ++i) {
    EXPECT_TRUE(vars_[i]->Bound());
  }
  EXPECT_FALSE(vars_[0]->Bound());
  for (int i = 1; i < 8; ++i) {
    EXPECT_EQ(0, vars_[i]->Min());
  }
  return nullptr;
}

// ----- Simplification Test -----

TEST_IN_PROPAGATION(LinearizerTest, SimplifySum, s, true) {
  IntVar* const x = s->MakeIntVar(0, 10, "x");
  std::vector<IntVar*> vars(3, x);
  IntExpr* const sum = s->MakeSum(vars);
  // Should be simplified to 3 * x, one way to test it is to force min
  // to 1, resulting mint should be 3.
  EXPECT_EQ(0, sum->Min());
  EXPECT_EQ(30, sum->Max());
  sum->SetMin(1);
  EXPECT_EQ(3, sum->Min());
  return nullptr;
}

TEST_IN_PROPAGATION(LinearizerTest, SimplifySumWithProd, s, true) {
  IntVar* const x = s->MakeIntVar(0, 10, "x");
  std::vector<IntVar*> vars(3);
  vars[0] = s->MakeProd(x, 1)->Var();
  vars[1] = s->MakeProd(x, 2)->Var();
  vars[2] = s->MakeProd(x, 3)->Var();
  IntExpr* const sum = s->MakeSum(vars);
  // Should be simplified to 6 * x, one way to test it is to force min
  // to 1, resulting mint should be 6.
  EXPECT_EQ(0, sum->Min());
  EXPECT_EQ(60, sum->Max());
  sum->SetMin(1);
  EXPECT_EQ(6, sum->Min());
  return nullptr;
}

TEST_IN_PROPAGATION(LinearizerTest, SimplifySumWithOpposite, s, true) {
  IntVar* const x = s->MakeIntVar(0, 10, "x");
  IntVar* const y = s->MakeIntVar(0, 10, "y");
  std::vector<IntVar*> vars(3);
  vars[0] = s->MakeProd(x, 2)->Var();
  vars[1] = y;
  vars[2] = s->MakeOpposite(y)->Var();
  IntExpr* const sum = s->MakeSum(vars);
  // Should be simplified to 2 * x, one way to test it is to force min
  // to 1, resulting mint should be 2.
  EXPECT_EQ(0, sum->Min());
  EXPECT_EQ(20, sum->Max());
  sum->SetMin(1);
  EXPECT_EQ(2, sum->Min());
  return nullptr;
}

TEST(LinearizerTest, SimplifySumWithDifference) {
  Solver s("LinearizerTest::SimplifySumWithDifference");
  IntVar* const x = s.MakeIntVar(0, 10, "x");
  IntVar* const y = s.MakeIntVar(0, 10, "y");
  std::vector<IntVar*> vars(3);
  vars[0] = s.MakeProd(x, 2)->Var();
  vars[1] = y;
  vars[2] = s.MakeDifference(x, y)->Var();
  IntExpr* const sum = s.MakeSum(vars);
  // Should be simplified to 3 * x, one way to test it is to force min
  // to 1, resulting min should be 3.
  EXPECT_EQ(0, sum->Min());
  EXPECT_EQ(30, sum->Max());
  sum->SetMin(1);
  EXPECT_EQ(3, sum->Min());
}

TEST(LinearizerTest, SimplifySumWithDifferenceAsExpr) {
  Solver s("LinearizerTest::SimplifySumWithDifference");
  IntVar* const x = s.MakeIntVar(0, 10, "x");
  IntVar* const y = s.MakeIntVar(0, 10, "y");
  std::vector<IntVar*> vars(3);
  vars[0] = s.MakeProd(x, 2)->Var();
  vars[1] = y;
  vars[2] = s.MakeOpposite(s.MakeDifference(y, x))->Var();
  IntExpr* const sum = s.MakeSum(vars);
  // Should be simplified to 3 * x, one way to test it is to force min
  // to 1, resulting min should be 3.
  EXPECT_EQ(0, sum->Min());
  EXPECT_EQ(30, sum->Max());
  sum->SetMin(1);
  EXPECT_EQ(3, sum->Min());
}

TEST(LinearizerTest, SimplifySumWithSum) {
  Solver s("LinearizerTest::SimplifySumWithDifference");
  IntVar* const x = s.MakeIntVar(0, 10, "x");
  IntVar* const y = s.MakeIntVar(0, 10, "y");
  std::vector<IntVar*> vars(3);
  vars[0] = s.MakeProd(x, 2)->Var();
  vars[1] = s.MakeOpposite(y)->Var();
  vars[2] = s.MakeSum(x, y)->Var();
  IntExpr* const sum = s.MakeSum(vars);
  // Should be simplified to 3 * x, one way to test it is to force min
  // to 1, resulting min should be 3.
  EXPECT_EQ(0, sum->Min());
  EXPECT_EQ(30, sum->Max());
  sum->SetMin(1);
  EXPECT_EQ(3, sum->Min());
}

TEST(LinearizerTest, SimplifySumWithSum2) {
  Solver s("LinearizerTest::SimplifySumWithDifference");
  IntVar* const x = s.MakeIntVar(0, 10, "x");
  std::vector<IntVar*> vars(3);
  vars[0] = s.MakeSum(x, 2)->Var();
  vars[1] = x;
  vars[2] = x;
  IntExpr* const sum = s.MakeSum(vars);
  // Should be simplified to 3 * x + 2, one way to test it is to force min
  // to 3, resulting min should be 5.
  EXPECT_EQ(2, sum->Min());
  EXPECT_EQ(32, sum->Max());
  sum->SetMin(3);
  EXPECT_EQ(5, sum->Min());
}

TEST(LinearizerTest, SimplifyScalProd) {
  Solver s("LinearizerTest::SimplifySumWithDifference");
  IntVar* const x = s.MakeIntVar(0, 10, "x");
  IntVar* const y = s.MakeIntVar(0, 10, "y");
  std::vector<IntVar*> vars(3);
  vars[0] = x;
  vars[1] = y;
  vars[2] = s.MakeDifference(x, y)->Var();
  std::vector<int64_t> coefficients = {1, 2, -1};
  IntExpr* const sum = s.MakeScalProd(vars, coefficients);
  // Should be simplified to 3 * y, one way to test it is to force min
  // to 1, resulting min should be 3.
  EXPECT_EQ(0, sum->Min());
  EXPECT_EQ(30, sum->Max());
  sum->SetMin(1);
  EXPECT_EQ(3, sum->Min());
  EXPECT_EQ(0, x->Min());
  EXPECT_EQ(1, y->Min());
}
}  // namespace
}  // namespace operations_research
