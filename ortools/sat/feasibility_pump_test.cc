// Copyright 2010-2024 Google LLC
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

#include "ortools/sat/feasibility_pump.h"

#include "gtest/gtest.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_loader.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace sat {
namespace {

constexpr double kTolerance = 1e-6;

int AddVariable(int lb, int ub, CpModelProto* model) {
  const int index = model->variables_size();
  sat::IntegerVariableProto* var = model->add_variables();
  var->add_domain(lb);
  var->add_domain(ub);
  return index;
}

TEST(FPTest, SimpleTest) {
  Model model;
  CpModelProto model_proto;
  AddVariable(0, 50, &model_proto);
  AddVariable(0, 20, &model_proto);
  auto* mapping = model.GetOrCreate<CpModelMapping>();
  LoadVariables(model_proto, false, &model);
  IntegerVariable x = mapping->Integer(0);
  IntegerVariable y = mapping->Integer(1);
  FeasibilityPump* fp = model.Create<FeasibilityPump>();
  fp->SetMaxFPIterations(3);

  LinearConstraintBuilder ct(IntegerValue(4), IntegerValue(8));
  ct.AddTerm(x, IntegerValue(2));
  ct.AddTerm(y, IntegerValue(1));
  fp->AddLinearConstraint(ct.Build());

  fp->SetObjectiveCoefficient(x, IntegerValue(6));
  fp->SetObjectiveCoefficient(y, IntegerValue(3));

  EXPECT_TRUE(fp->Solve());

  EXPECT_TRUE(fp->HasLPSolution());
  EXPECT_NEAR(12.0, fp->LPSolutionObjectiveValue(), kTolerance);
  EXPECT_TRUE(fp->LPSolutionIsInteger());
}

TEST(FPTest, InfeasibilityTest) {
  Model model;
  CpModelProto model_proto;
  model.GetOrCreate<SatParameters>()->set_fp_rounding(
      SatParameters::PROPAGATION_ASSISTED);
  AddVariable(0, 2, &model_proto);
  auto* mapping = model.GetOrCreate<CpModelMapping>();
  LoadVariables(model_proto, false, &model);
  IntegerVariable x = mapping->Integer(0);
  FeasibilityPump* fp = model.Create<FeasibilityPump>();
  fp->SetMaxFPIterations(3);

  // Note(user): We don't rely on the imprecise LP to report infeasibility.
  model.GetOrCreate<SatSolver>()->NotifyThatModelIsUnsat();
  // x = 1/4
  LinearConstraintBuilder ct(IntegerValue(1), IntegerValue(1));
  ct.AddTerm(x, IntegerValue(4));
  fp->AddLinearConstraint(ct.Build());

  fp->SetObjectiveCoefficient(x, IntegerValue(4));

  EXPECT_FALSE(fp->Solve());
}

// int x,y in [0,50]
// min -x -2y
// 2x + 3y <= 12
// 3x + 2y <= 12
// -x + y = 0
//
// LP solution: -7.2 (x = y = 2.4).
// Integer feasible solution: -6 (x = y = 2).
class FeasibilityPumpTest : public testing::Test {
 public:
  FeasibilityPumpTest() {
    AddVariable(0, 50, &model_proto_);
    AddVariable(0, 50, &model_proto_);
    auto* mapping = model_.GetOrCreate<CpModelMapping>();
    LoadVariables(model_proto_, false, &model_);
    IntegerVariable x_ = mapping->Integer(0);
    IntegerVariable y_ = mapping->Integer(1);
    fp_ = model_.Create<FeasibilityPump>();

    LinearConstraintBuilder ct(kMinIntegerValue, IntegerValue(12));
    ct.AddTerm(x_, IntegerValue(3));
    ct.AddTerm(y_, IntegerValue(2));
    fp_->AddLinearConstraint(ct.Build());

    LinearConstraintBuilder ct2(kMinIntegerValue, IntegerValue(12));
    ct2.AddTerm(x_, IntegerValue(2));
    ct2.AddTerm(y_, IntegerValue(3));
    fp_->AddLinearConstraint(ct2.Build());

    LinearConstraintBuilder ct3(IntegerValue(0), IntegerValue(0));
    ct3.AddTerm(x_, IntegerValue(-1));
    ct3.AddTerm(y_, IntegerValue(1));
    fp_->AddLinearConstraint(ct3.Build());

    fp_->SetObjectiveCoefficient(x_, IntegerValue(-1));
    fp_->SetObjectiveCoefficient(y_, IntegerValue(-2));
  }

  FeasibilityPump* fp_;
  Model model_;
  CpModelProto model_proto_;
  IntegerVariable x_;
  IntegerVariable y_;
};

TEST_F(FeasibilityPumpTest, SimpleRounding) {
  fp_->SetMaxFPIterations(1);
  EXPECT_TRUE(fp_->Solve());

  EXPECT_TRUE(fp_->HasLPSolution());
  EXPECT_NEAR(-7.2, fp_->LPSolutionObjectiveValue(), kTolerance);
  EXPECT_FALSE(fp_->LPSolutionIsInteger());
  EXPECT_NEAR(0.4, fp_->LPSolutionFractionality(), kTolerance);

  EXPECT_TRUE(fp_->HasIntegerSolution());
  EXPECT_EQ(-6, fp_->IntegerSolutionObjectiveValue());
  EXPECT_TRUE(fp_->IntegerSolutionIsFeasible());
}

TEST_F(FeasibilityPumpTest, MultipleIterations) {
  fp_->SetMaxFPIterations(5);
  EXPECT_TRUE(fp_->Solve());

  EXPECT_TRUE(fp_->HasLPSolution());
  EXPECT_NEAR(-6, fp_->LPSolutionObjectiveValue(), kTolerance);
  EXPECT_TRUE(fp_->LPSolutionIsInteger());

  EXPECT_TRUE(fp_->HasIntegerSolution());
  EXPECT_EQ(-6, fp_->IntegerSolutionObjectiveValue());
  EXPECT_TRUE(fp_->IntegerSolutionIsFeasible());
}

TEST_F(FeasibilityPumpTest, MultipleCalls) {
  EXPECT_TRUE(fp_->Solve());

  EXPECT_TRUE(fp_->HasLPSolution());
  EXPECT_NEAR(-6, fp_->LPSolutionObjectiveValue(), kTolerance);
  EXPECT_TRUE(fp_->LPSolutionIsInteger());

  EXPECT_TRUE(fp_->HasIntegerSolution());
  EXPECT_EQ(-6, fp_->IntegerSolutionObjectiveValue());
  EXPECT_TRUE(fp_->IntegerSolutionIsFeasible());

  // Change bounds.
  auto* integer_trail = model_.GetOrCreate<IntegerTrail>();
  ASSERT_TRUE(integer_trail->Enqueue(
      IntegerLiteral::LowerOrEqual(x_, IntegerValue(1)), {}, {}));
  EXPECT_TRUE(fp_->Solve());

  EXPECT_TRUE(fp_->HasLPSolution());
  EXPECT_NEAR(-3, fp_->LPSolutionObjectiveValue(), kTolerance);
  EXPECT_TRUE(fp_->LPSolutionIsInteger());
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
