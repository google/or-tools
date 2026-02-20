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

#include "ortools/sat/cp_model.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/container_logging.h"
#include "ortools/base/gmock.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::ElementsAre;
using ::testing::EqualsProto;
using ::testing::HasSubstr;

TEST(BoolVarTest, NullAPI) {
  BoolVar var;
  EXPECT_EQ(var.Name(), "null");
  EXPECT_EQ(var.DebugString(), "null");
  if (!DEBUG_MODE) var.WithName("ignored");  // Do not crash.
}

TEST(BoolVarTest, TestApi) {
  CpModelBuilder cp_model;
  const BoolVar b1 = cp_model.NewBoolVar().WithName("b1");
  const BoolVar b2 = b1.Not();
  const BoolVar b3 = b2.Not();
  EXPECT_EQ(b1.DebugString(), "b1(0, 1)");
  EXPECT_EQ(b2.DebugString(), "Not(b1(0, 1))");
  EXPECT_EQ(b1, b3);
  EXPECT_EQ(b1.Not(), Not(b1));

  EXPECT_EQ(b1.Name(), "b1");
  EXPECT_EQ(b2.Name(), "Not(b1)");

  ASSERT_EQ(Domain(0, 1), IntVar(b1).Domain());

  const BoolVar t = cp_model.TrueVar();
  ASSERT_EQ(Domain(1), IntVar(t).Domain());
  EXPECT_EQ(t.DebugString(), "true");

  const BoolVar f = cp_model.FalseVar();
  ASSERT_EQ(Domain(0), IntVar(f).Domain());
  EXPECT_EQ(f.DebugString(), "false");
}

TEST(IntVarTest, NullAPI) {
  IntVar var;
  EXPECT_EQ(var.Name(), "null");
  EXPECT_EQ(var.DebugString(), "null");
  if (!DEBUG_MODE) var.WithName("ignored");  // Do not crash.
}

TEST(IntVarTest, NullApiFromBoolVar) {
  IntVar var = IntVar(BoolVar());
  EXPECT_EQ(var.Name(), "null");
  EXPECT_EQ(var.DebugString(), "null");
  if (!DEBUG_MODE) var.WithName("ignored");  // Do not crash.
}

TEST(IntVarTest, TestApi) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({2, 8}).WithName("x");
  EXPECT_EQ("x", x.Name());
  ASSERT_EQ(Domain(2, 8), x.Domain());
  EXPECT_EQ(x.DebugString(), "x(2, 8)");
  const IntVar y =
      cp_model.NewIntVar(Domain::FromValues({1, 2, 5, 3})).WithName("y");
  EXPECT_EQ("y", y.Name());
  ASSERT_EQ(Domain::FromValues({1, 2, 3, 5}), y.Domain());
  const IntVar z1 = cp_model.NewConstant(5);
  const IntVar z2 = cp_model.NewConstant(6);
  const IntVar z3 = cp_model.NewConstant(5);
  EXPECT_EQ(z1.DebugString(), "5");
  EXPECT_EQ(z1, z3);
  EXPECT_NE(z1, z2);
}

TEST(CpModelBuilderTest, UsingUninitializedVariableResultInInvalidModel) {
  // This test the non-debug behavior.
  if (DEBUG_MODE) return;

  CpModelBuilder builder;
  IntVar x, y;
  builder.AddEquality(x, y);
  CpModelProto model = builder.Build();
  EXPECT_NE(ValidateCpModel(model), "");
}

TEST(CpModelBuilderTest, FixVariableApi) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({2, 8});
  const BoolVar a = cp_model.NewBoolVar();
  const BoolVar b = a.Not();

  EXPECT_EQ(Domain(2, 8), x.Domain());
  cp_model.FixVariable(x, 10);
  EXPECT_EQ(Domain(10), x.Domain());

  EXPECT_EQ("BoolVar1(0, 1)", a.DebugString());
  cp_model.FixVariable(a, true);
  EXPECT_EQ("true", a.DebugString());
  EXPECT_EQ("Not(true)", b.DebugString());

  cp_model.FixVariable(b, true);
  EXPECT_EQ("false", a.DebugString());
  EXPECT_EQ("Not(false)", b.DebugString());
}

TEST(IntVarTest, TestBoolVarToIntVarCast) {
  CpModelBuilder cp_model;
  const BoolVar b = cp_model.NewBoolVar().WithName("b");
  const IntVar x = IntVar(b);
  ASSERT_EQ(Domain(0, 1), x.Domain());
  EXPECT_EQ(x.Name(), "b");
}

TEST(IntVarTest, TestNotBoolVarToIntVarCast) {
  CpModelBuilder cp_model;
  const BoolVar b = cp_model.NewBoolVar().WithName("b");

  // This cast create an extra variable.
  const IntVar x = IntVar(b.Not());
  EXPECT_EQ(x.Name(), "Not(b)");
  EXPECT_EQ(x.DebugString(), "Not(b)(0, 1)");
  EXPECT_EQ(x.index(), 1);
}

TEST(IntVarTest, TestIntVarToBoolVarCast) {
  CpModelBuilder cp_model;
  const IntVar int_1 = cp_model.NewIntVar({0, 1});
  const BoolVar bool_1 = int_1.ToBoolVar();
  EXPECT_EQ(int_1.index(), bool_1.index());

  const IntVar int_true = cp_model.NewIntVar({1, 1});
  const BoolVar bool_true = int_true.ToBoolVar();
  EXPECT_EQ(int_true.index(), bool_true.index());

  const IntVar int_false = cp_model.NewIntVar({0, 0});
  const BoolVar bool_false = int_false.ToBoolVar();
  EXPECT_EQ(int_false.index(), bool_false.index());
}

TEST(LinearExprTest, TestApi) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({2, 8}).WithName("x");
  const IntVar y = cp_model.NewIntVar({1, 5}).WithName("y");
  const BoolVar b = cp_model.NewBoolVar().WithName("b");
  LinearExpr e1(x);
  EXPECT_THAT(e1.variables(), ElementsAre(x.index()));
  EXPECT_THAT(e1.coefficients(), ElementsAre(1));
  EXPECT_EQ(e1.constant(), 0);
  e1 += b;
  EXPECT_THAT(e1.variables(), ElementsAre(x.index(), b.index()));
  EXPECT_THAT(e1.coefficients(), ElementsAre(1, 1));
  e1 += y * 3;
  EXPECT_THAT(e1.variables(), ElementsAre(x.index(), b.index(), y.index()));
  EXPECT_THAT(e1.coefficients(), ElementsAre(1, 1, 3));
  e1 += 10;
  EXPECT_EQ(10, e1.constant());

  LinearExpr e2(b);
  EXPECT_THAT(e2.variables(), ElementsAre(b.index()));
  EXPECT_THAT(e2.coefficients(), ElementsAre(1));
  EXPECT_EQ(0, e2.constant());

  LinearExpr e3(-5);
  ASSERT_EQ(0, e3.variables().size());
  ASSERT_EQ(0, e3.coefficients().size());
  EXPECT_EQ(-5, e3.constant());
  e3 += b.Not();
  EXPECT_THAT(e3.variables(), ElementsAre(b.index()));
  EXPECT_THAT(e3.coefficients(), ElementsAre(-1));
  EXPECT_EQ(-4, e3.constant());

  LinearExpr e4(b.Not());
  EXPECT_THAT(e4.variables(), ElementsAre(b.index()));
  EXPECT_THAT(e4.coefficients(), ElementsAre(-1));
  EXPECT_EQ(1, e4.constant());

  LinearExpr e5 = x + 22;
  EXPECT_THAT(e5.variables(), ElementsAre(x.index()));
  EXPECT_THAT(e5.coefficients(), ElementsAre(1));
  EXPECT_EQ(22, e5.constant());

  LinearExpr e6 = b + b + 23;
  EXPECT_THAT(e6.variables(), ElementsAre(b.index(), b.index()));
  EXPECT_THAT(e6.coefficients(), ElementsAre(1, 1));
  EXPECT_EQ(23, e6.constant());

  LinearExpr e7 = -5 * x + 24;
  EXPECT_THAT(e7.variables(), ElementsAre(x.index()));
  EXPECT_THAT(e7.coefficients(), ElementsAre(-5));
  EXPECT_EQ(24, e7.constant());

  LinearExpr e8 = LinearExpr::WeightedSum({b}, {17}) + 26;
  EXPECT_THAT(e8.variables(), ElementsAre(b.index()));
  EXPECT_THAT(e8.coefficients(), ElementsAre(17));
  EXPECT_EQ(26, e8.constant());

  LinearExpr e9 = LinearExpr::Sum({x});
  EXPECT_THAT(e9.variables(), ElementsAre(x.index()));
  EXPECT_THAT(e9.coefficients(), ElementsAre(1));
  EXPECT_EQ(0, e9.constant());

  LinearExpr e10 = LinearExpr::Sum({b});
  EXPECT_THAT(e10.variables(), ElementsAre(b.index()));
  EXPECT_THAT(e10.coefficients(), ElementsAre(1));
  EXPECT_EQ(0, e10.constant());

  LinearExpr e11 = LinearExpr::WeightedSum({x}, {-5});
  EXPECT_THAT(e11.variables(), ElementsAre(x.index()));
  EXPECT_THAT(e11.coefficients(), ElementsAre(-5));
  EXPECT_EQ(0, e11.constant());

  LinearExpr e12 = LinearExpr::WeightedSum({b}, {17});
  EXPECT_THAT(e12.variables(), ElementsAre(b.index()));
  EXPECT_THAT(e12.coefficients(), ElementsAre(17));
  EXPECT_EQ(0, e12.constant());

  LinearExpr e13 = LinearExpr::Sum({b});
  EXPECT_THAT(e13.variables(), ElementsAre(b.index()));
  EXPECT_THAT(e13.coefficients(), ElementsAre(1));
  EXPECT_EQ(0, e13.constant());

  LinearExpr e14 = LinearExpr::Sum({b, b}) + 23;
  EXPECT_THAT(e14.variables(), ElementsAre(b.index(), b.index()));
  EXPECT_THAT(e14.coefficients(), ElementsAre(1, 1));
  EXPECT_EQ(23, e14.constant());

  LinearExpr e15 = LinearExpr::WeightedSum({b}, {17});
  EXPECT_THAT(e15.variables(), ElementsAre(b.index()));
  EXPECT_THAT(e15.coefficients(), ElementsAre(17));
  EXPECT_EQ(0, e15.constant());

  LinearExpr e16 = LinearExpr::WeightedSum({b}, {17}) + 26;
  EXPECT_THAT(e16.variables(), ElementsAre(b.index()));
  EXPECT_THAT(e16.coefficients(), ElementsAre(17));
  EXPECT_EQ(26, e16.constant());

  std::vector<BoolVar> bools = {b};
  LinearExpr e17 = LinearExpr::Sum(bools) + 23 - 10;
  EXPECT_THAT(e17.variables(), ElementsAre(b.index()));
  EXPECT_THAT(e17.coefficients(), ElementsAre(1));
  EXPECT_EQ(13, e17.constant());

  LinearExpr e18 = LinearExpr::Term(x, -5);
  EXPECT_THAT(e18.variables(), ElementsAre(x.index()));
  EXPECT_THAT(e18.coefficients(), ElementsAre(-5));
  EXPECT_EQ(0, e18.constant());

  LinearExpr e19 = LinearExpr::Term(b.Not(), 4);
  EXPECT_THAT(e19.variables(), ElementsAre(b.index()));
  EXPECT_THAT(e19.coefficients(), ElementsAre(-4));
  EXPECT_EQ(4, e19.constant());

  LinearExpr e20 = x + 7;
  EXPECT_THAT(e20.variables(), ElementsAre(x.index()));
  EXPECT_THAT(e20.coefficients(), ElementsAre(1));
  EXPECT_EQ(e20.constant(), 7);
}

TEST(DoubleLinearExprTest, TestApi) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({2, 8}).WithName("x");
  const IntVar y = cp_model.NewIntVar({1, 5}).WithName("y");
  const BoolVar b = cp_model.NewBoolVar().WithName("b");
  DoubleLinearExpr e1(x);
  EXPECT_THAT(e1.variables(), ElementsAre(x.index()));
  EXPECT_THAT(e1.coefficients(), ElementsAre(1));
  EXPECT_EQ(e1.constant(), 0);
  e1 += b;
  EXPECT_THAT(e1.variables(), ElementsAre(x.index(), b.index()));
  EXPECT_THAT(e1.coefficients(), ElementsAre(1, 1));
  e1.AddTerm(y, 3);
  EXPECT_THAT(e1.variables(), ElementsAre(x.index(), b.index(), y.index()));
  EXPECT_THAT(e1.coefficients(), ElementsAre(1, 1, 3));
  e1 += 10;
  EXPECT_EQ(10, e1.constant());

  DoubleLinearExpr e2(b);
  EXPECT_THAT(e2.variables(), ElementsAre(b.index()));
  EXPECT_THAT(e2.coefficients(), ElementsAre(1));
  EXPECT_EQ(0, e2.constant());

  DoubleLinearExpr e3(-5);
  ASSERT_EQ(0, e3.variables().size());
  ASSERT_EQ(0, e3.coefficients().size());
  EXPECT_EQ(-5, e3.constant());
  e3 += b.Not();
  EXPECT_THAT(e3.variables(), ElementsAre(b.index()));
  EXPECT_THAT(e3.coefficients(), ElementsAre(-1));
  EXPECT_EQ(-4, e3.constant());

  DoubleLinearExpr e4(b.Not());
  EXPECT_THAT(e4.variables(), ElementsAre(b.index()));
  EXPECT_THAT(e4.coefficients(), ElementsAre(-1));
  EXPECT_EQ(1, e4.constant());

  DoubleLinearExpr e5 = DoubleLinearExpr(x) + 22;
  EXPECT_THAT(e5.variables(), ElementsAre(x.index()));
  EXPECT_THAT(e5.coefficients(), ElementsAre(1));
  EXPECT_EQ(22, e5.constant());

  DoubleLinearExpr e6 = DoubleLinearExpr::Sum({b, b}) + 23;
  EXPECT_THAT(e6.variables(), ElementsAre(b.index(), b.index()));
  EXPECT_THAT(e6.coefficients(), ElementsAre(1, 1));
  EXPECT_EQ(23, e6.constant());

  DoubleLinearExpr e7 = -5 * DoubleLinearExpr(x) + 24;
  EXPECT_THAT(e7.variables(), ElementsAre(x.index()));
  EXPECT_THAT(e7.coefficients(), ElementsAre(-5));
  EXPECT_EQ(24, e7.constant());

  DoubleLinearExpr e8 = DoubleLinearExpr::WeightedSum({b}, {17}) + 26;
  EXPECT_THAT(e8.variables(), ElementsAre(b.index()));
  EXPECT_THAT(e8.coefficients(), ElementsAre(17));
  EXPECT_EQ(26, e8.constant());

  DoubleLinearExpr e10 = DoubleLinearExpr::Sum({b});
  EXPECT_THAT(e10.variables(), ElementsAre(b.index()));
  EXPECT_THAT(e10.coefficients(), ElementsAre(1));
  EXPECT_EQ(0, e10.constant());

  DoubleLinearExpr e11 = -5 * DoubleLinearExpr(x);
  EXPECT_THAT(e11.variables(), ElementsAre(x.index()));
  EXPECT_THAT(e11.coefficients(), ElementsAre(-5));
  EXPECT_EQ(0, e11.constant());

  DoubleLinearExpr e12 = DoubleLinearExpr::WeightedSum({b}, {17});
  EXPECT_THAT(e12.variables(), ElementsAre(b.index()));
  EXPECT_THAT(e12.coefficients(), ElementsAre(17));
  EXPECT_EQ(0, e12.constant());

  DoubleLinearExpr e13 = DoubleLinearExpr::Sum({b});
  EXPECT_THAT(e13.variables(), ElementsAre(b.index()));
  EXPECT_THAT(e13.coefficients(), ElementsAre(1));
  EXPECT_EQ(0, e13.constant());

  DoubleLinearExpr e14 = DoubleLinearExpr::Sum({b, b}) + 23;
  EXPECT_THAT(e14.variables(), ElementsAre(b.index(), b.index()));
  EXPECT_THAT(e14.coefficients(), ElementsAre(1, 1));
  EXPECT_EQ(23, e14.constant());

  DoubleLinearExpr e15 = DoubleLinearExpr::WeightedSum({b}, {17});
  EXPECT_THAT(e15.variables(), ElementsAre(b.index()));
  EXPECT_THAT(e15.coefficients(), ElementsAre(17));
  EXPECT_EQ(0, e15.constant());

  DoubleLinearExpr e16 = DoubleLinearExpr::WeightedSum({b}, {17}) + 26;
  EXPECT_THAT(e16.variables(), ElementsAre(b.index()));
  EXPECT_THAT(e16.coefficients(), ElementsAre(17));
  EXPECT_EQ(26, e16.constant());

  std::vector<BoolVar> bools = {b};
  DoubleLinearExpr e17 = DoubleLinearExpr::Sum(bools) + 23 - 10;
  EXPECT_THAT(e17.variables(), ElementsAre(b.index()));
  EXPECT_THAT(e17.coefficients(), ElementsAre(1));
  EXPECT_EQ(13, e17.constant());

  DoubleLinearExpr e18 = DoubleLinearExpr(x) - 5;
  EXPECT_THAT(e18.variables(), ElementsAre(x.index()));
  EXPECT_THAT(e18.coefficients(), ElementsAre(1));
  EXPECT_EQ(-5, e18.constant());

  DoubleLinearExpr e19 = 4 * DoubleLinearExpr(b.Not());
  EXPECT_THAT(e19.variables(), ElementsAre(b.index()));
  EXPECT_THAT(e19.coefficients(), ElementsAre(-4));
  EXPECT_EQ(4, e19.constant());

  DoubleLinearExpr e20 = DoubleLinearExpr(x) - 7;
  EXPECT_THAT(e20.variables(), ElementsAre(x.index()));
  EXPECT_THAT(e20.coefficients(), ElementsAre(1));
  EXPECT_EQ(e20.constant(), -7);
}

TEST(ConstraintTest, TestAPI) {
  CpModelBuilder cp_model;
  const BoolVar b1 = cp_model.NewBoolVar().WithName("b1");
  const BoolVar b2 = cp_model.NewBoolVar().WithName("b2");
  const BoolVar b3 = cp_model.NewBoolVar().WithName("b3");
  const BoolVar b4 = cp_model.NewBoolVar().WithName("b4");
  Constraint ct = cp_model.AddBoolOr({b1, b2.Not(), b3}).OnlyEnforceIf(Not(b4));
  ct.WithName("bool_or");
  ASSERT_EQ(ct.Name(), "bool_or");
}

TEST(CpModelTest, TestBoolOr) {
  CpModelBuilder cp_model;
  const BoolVar b1 = cp_model.NewBoolVar().WithName("b1");
  const BoolVar b2 = cp_model.NewBoolVar().WithName("b2");
  const BoolVar b3 = cp_model.NewBoolVar().WithName("b3");
  const BoolVar b4 = cp_model.NewBoolVar().WithName("b4");
  cp_model.AddBoolOr({b1, b2.Not(), b3}).OnlyEnforceIf(Not(b4));
  ASSERT_EQ(1, cp_model.Proto().constraints_size());
  EXPECT_EQ(3, cp_model.Proto().constraints(0).bool_or().literals_size());
  ASSERT_EQ(1, cp_model.Proto().constraints(0).enforcement_literal_size());
}

TEST(CpModelTest, TestAtMostOne) {
  CpModelBuilder cp_model;
  const BoolVar b1 = cp_model.NewBoolVar().WithName("b1");
  const BoolVar b2 = cp_model.NewBoolVar().WithName("b2");
  const BoolVar b3 = cp_model.NewBoolVar().WithName("b3");
  cp_model.AddAtMostOne({b1, b2.Not(), b3});
  ASSERT_EQ(1, cp_model.Proto().constraints_size());
  EXPECT_EQ(3, cp_model.Proto().constraints(0).at_most_one().literals_size());
}

TEST(CpModelTest, TestExactlyOne) {
  CpModelBuilder cp_model;
  const BoolVar b1 = cp_model.NewBoolVar().WithName("b1");
  const BoolVar b2 = cp_model.NewBoolVar().WithName("b2");
  const BoolVar b3 = cp_model.NewBoolVar().WithName("b3");
  cp_model.AddExactlyOne({b1, b2.Not(), b3});
  ASSERT_EQ(1, cp_model.Proto().constraints_size());
  EXPECT_EQ(3, cp_model.Proto().constraints(0).exactly_one().literals_size());
}

TEST(CpModelTest, TestBoolAnd) {
  CpModelBuilder cp_model;
  const BoolVar b1 = cp_model.NewBoolVar().WithName("b1");
  const BoolVar b2 = cp_model.NewBoolVar().WithName("b2");
  const BoolVar b3 = cp_model.NewBoolVar().WithName("b3");
  const BoolVar b4 = cp_model.NewBoolVar().WithName("b4");
  cp_model.AddBoolAnd({b1, b2.Not(), b3, b4});
  ASSERT_EQ(1, cp_model.Proto().constraints_size());
  EXPECT_EQ(4, cp_model.Proto().constraints(0).bool_and().literals_size());
  EXPECT_EQ(0, cp_model.Proto().constraints(0).enforcement_literal_size());
}

TEST(CpModelTest, TestBoolXor) {
  CpModelBuilder cp_model;
  const BoolVar b1 = cp_model.NewBoolVar().WithName("b1");
  const BoolVar b2 = cp_model.NewBoolVar().WithName("b2");
  const BoolVar b3 = cp_model.NewBoolVar().WithName("b3");
  const BoolVar b4 = cp_model.NewBoolVar().WithName("b4");
  cp_model.AddBoolXor({b1, b2.Not(), b3, b4});
  ASSERT_EQ(1, cp_model.Proto().constraints_size());
  EXPECT_EQ(4, cp_model.Proto().constraints(0).bool_xor().literals_size());
  EXPECT_EQ(0, cp_model.Proto().constraints(0).enforcement_literal_size());
}

TEST(CpModelTest, TestLinearizedBoolAndEqual) {
  CpModelBuilder cp_model;
  const BoolVar t = cp_model.NewBoolVar();
  const BoolVar a = cp_model.NewBoolVar();
  const BoolVar b = cp_model.NewBoolVar();
  cp_model.AddBoolAnd({a, b}).OnlyEnforceIf(t);
  cp_model.AddEquality(t, 1).OnlyEnforceIf({a, b});

  Model model;
  SatParameters parameters;
  parameters.set_enumerate_all_solutions(true);
  parameters.set_num_workers(1);
  model.Add(NewSatParameters(parameters));

  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver(
      [&](const CpSolverResponse& /*r*/) { num_solutions++; }));
  const CpSolverResponse response = SolveCpModel(cp_model.Build(), &model);
  EXPECT_EQ(num_solutions, 4);
}

TEST(CpModelTest, TestBoolXorOneFalseVar) {
  CpModelBuilder cp_model;
  const BoolVar b1 = cp_model.FalseVar();
  cp_model.AddBoolXor({b1});
  const CpSolverResponse response = Solve(cp_model.Build());
  ASSERT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(CpModelTest, TestBoolXorTwoTrueVars) {
  CpModelBuilder cp_model;
  const BoolVar b1 = cp_model.NewBoolVar();
  const BoolVar b2 = cp_model.NewBoolVar();
  cp_model.AddBoolXor({b1, b2});
  cp_model.AddEquality(b1, true);
  cp_model.AddEquality(b2, true);
  const CpSolverResponse response = Solve(cp_model.Build());
  ASSERT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(CpModelTest, TestImplication) {
  CpModelBuilder cp_model;
  const BoolVar b1 = cp_model.NewBoolVar().WithName("b1");
  const BoolVar b2 = cp_model.NewBoolVar().WithName("b2");
  cp_model.AddImplication(b1, b2);
  ASSERT_EQ(1, cp_model.Proto().constraints_size());
  EXPECT_EQ(2, cp_model.Proto().constraints(0).bool_or().literals_size());
  EXPECT_EQ(0, cp_model.Proto().constraints(0).enforcement_literal_size());
  EXPECT_EQ(-1, cp_model.Proto().constraints(0).bool_or().literals(0));
  EXPECT_EQ(1, cp_model.Proto().constraints(0).bool_or().literals(1));
}

TEST(CpModelTest, TestEquality) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({0, 20}).WithName("x");
  cp_model.AddEquality(x, 10);
  ASSERT_EQ(1, cp_model.Proto().constraints_size());
  EXPECT_EQ(1, cp_model.Proto().constraints(0).linear().vars_size());
  EXPECT_EQ(0, cp_model.Proto().constraints(0).linear().vars(0));
  EXPECT_EQ(1, cp_model.Proto().constraints(0).linear().coeffs(0));
  EXPECT_EQ(10, cp_model.Proto().constraints(0).linear().domain(0));
  EXPECT_EQ(10, cp_model.Proto().constraints(0).linear().domain(1));
}

TEST(CpModelTest, TestBooleanEquality) {
  CpModelBuilder cp_model;
  const BoolVar x = cp_model.NewBoolVar().WithName("x");
  cp_model.AddEquality(x, 1);
  ASSERT_EQ(1, cp_model.Proto().constraints_size());
  EXPECT_EQ(1, cp_model.Proto().constraints(0).linear().vars_size());
  EXPECT_EQ(0, cp_model.Proto().constraints(0).linear().vars(0));
  EXPECT_EQ(1, cp_model.Proto().constraints(0).linear().coeffs(0));
  EXPECT_EQ(1, cp_model.Proto().constraints(0).linear().domain(0));
  EXPECT_EQ(1, cp_model.Proto().constraints(0).linear().domain(1));
}

TEST(CpModelTest, TestNotBooleanEquality) {
  CpModelBuilder cp_model;
  const BoolVar x = cp_model.NewBoolVar().WithName("x");
  cp_model.AddEquality(Not(x), 0);
  ASSERT_EQ(1, cp_model.Proto().constraints_size());
  EXPECT_EQ(1, cp_model.Proto().constraints(0).linear().vars_size());
  EXPECT_EQ(0, cp_model.Proto().constraints(0).linear().vars(0));
  EXPECT_EQ(-1, cp_model.Proto().constraints(0).linear().coeffs(0));
  EXPECT_EQ(-1, cp_model.Proto().constraints(0).linear().domain(0));
  EXPECT_EQ(-1, cp_model.Proto().constraints(0).linear().domain(1));
}

TEST(CpModelTest, TestGreaterOrEqual) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({0, 20}).WithName("x");
  cp_model.AddGreaterOrEqual(x, 10);
  ASSERT_EQ(1, cp_model.Proto().constraints_size());
  EXPECT_EQ(1, cp_model.Proto().constraints(0).linear().vars_size());
  EXPECT_EQ(0, cp_model.Proto().constraints(0).linear().vars(0));
  EXPECT_EQ(1, cp_model.Proto().constraints(0).linear().coeffs(0));
  EXPECT_EQ(10, cp_model.Proto().constraints(0).linear().domain(0));
  EXPECT_EQ(std::numeric_limits<int64_t>::max(),
            cp_model.Proto().constraints(0).linear().domain(1));
}

TEST(CpModelTest, TestGreater) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({0, 20}).WithName("x");
  cp_model.AddGreaterThan(x, 10);
  ASSERT_EQ(1, cp_model.Proto().constraints_size());
  EXPECT_EQ(1, cp_model.Proto().constraints(0).linear().vars_size());
  EXPECT_EQ(0, cp_model.Proto().constraints(0).linear().vars(0));
  EXPECT_EQ(1, cp_model.Proto().constraints(0).linear().coeffs(0));
  EXPECT_EQ(11, cp_model.Proto().constraints(0).linear().domain(0));
  EXPECT_EQ(std::numeric_limits<int64_t>::max(),
            cp_model.Proto().constraints(0).linear().domain(1));
}

TEST(CpModelTest, TestLessOrEqual) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({0, 20}).WithName("x");
  cp_model.AddLessOrEqual(x, 10);
  ASSERT_EQ(1, cp_model.Proto().constraints_size());
  EXPECT_EQ(1, cp_model.Proto().constraints(0).linear().vars_size());
  EXPECT_EQ(0, cp_model.Proto().constraints(0).linear().vars(0));
  EXPECT_EQ(1, cp_model.Proto().constraints(0).linear().coeffs(0));
  EXPECT_EQ(std::numeric_limits<int64_t>::min(),
            cp_model.Proto().constraints(0).linear().domain(0));
  EXPECT_EQ(10, cp_model.Proto().constraints(0).linear().domain(1));
}

TEST(CpModelTest, TestLess) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({0, 20}).WithName("x");
  cp_model.AddLessThan(x, 10);
  ASSERT_EQ(1, cp_model.Proto().constraints_size());
  EXPECT_EQ(1, cp_model.Proto().constraints(0).linear().vars_size());
  EXPECT_EQ(0, cp_model.Proto().constraints(0).linear().vars(0));
  EXPECT_EQ(1, cp_model.Proto().constraints(0).linear().coeffs(0));
  EXPECT_EQ(std::numeric_limits<int64_t>::min(),
            cp_model.Proto().constraints(0).linear().domain(0));
  EXPECT_EQ(9, cp_model.Proto().constraints(0).linear().domain(1));
}

TEST(CpModelTest, TestLinearConstraint) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({0, 20}).WithName("x");
  const IntVar y = cp_model.NewIntVar({0, 20}).WithName("y");
  cp_model.AddLinearConstraint(LinearExpr::Sum({x, y}), {1, 9});
  ASSERT_EQ(1, cp_model.Proto().constraints_size());
  EXPECT_EQ(2, cp_model.Proto().constraints(0).linear().vars_size());
  EXPECT_EQ(0, cp_model.Proto().constraints(0).linear().vars(0));
  EXPECT_EQ(1, cp_model.Proto().constraints(0).linear().vars(1));
  EXPECT_EQ(1, cp_model.Proto().constraints(0).linear().coeffs(0));
  EXPECT_EQ(1, cp_model.Proto().constraints(0).linear().coeffs(1));
  ASSERT_EQ(2, cp_model.Proto().constraints(0).linear().domain_size());
  EXPECT_EQ(1, cp_model.Proto().constraints(0).linear().domain(0));
  EXPECT_EQ(9, cp_model.Proto().constraints(0).linear().domain(1));
}

TEST(CpModelTest, TestNotEqual) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({0, 20}).WithName("x");
  const IntVar y = cp_model.NewIntVar({0, 20}).WithName("y");
  cp_model.AddNotEqual(x, y);
  ASSERT_EQ(1, cp_model.Proto().constraints_size());
  EXPECT_EQ(2, cp_model.Proto().constraints(0).linear().vars_size());
  EXPECT_EQ(0, cp_model.Proto().constraints(0).linear().vars(0));
  EXPECT_EQ(1, cp_model.Proto().constraints(0).linear().vars(1));
  EXPECT_EQ(1, cp_model.Proto().constraints(0).linear().coeffs(0));
  EXPECT_EQ(-1, cp_model.Proto().constraints(0).linear().coeffs(1));
  EXPECT_EQ(std::numeric_limits<int64_t>::min(),
            cp_model.Proto().constraints(0).linear().domain(0));
  EXPECT_EQ(-1, cp_model.Proto().constraints(0).linear().domain(1));
  EXPECT_EQ(1, cp_model.Proto().constraints(0).linear().domain(2));
  EXPECT_EQ(std::numeric_limits<int64_t>::max(),
            cp_model.Proto().constraints(0).linear().domain(3));
}

TEST(CpModelTest, TestAllDifferent) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({0, 20}).WithName("x");
  const IntVar y = cp_model.NewIntVar({0, 20}).WithName("y");
  const IntVar z = cp_model.NewIntVar({0, 20}).WithName("z");
  cp_model.AddAllDifferent({x, z, y});

  ASSERT_EQ(1, cp_model.Proto().constraints_size());
  EXPECT_EQ(3, cp_model.Proto().constraints(0).all_diff().exprs_size());
  EXPECT_EQ(0, cp_model.Proto().constraints(0).all_diff().exprs(0).vars(0));
  EXPECT_EQ(2, cp_model.Proto().constraints(0).all_diff().exprs(1).vars(0));
  EXPECT_EQ(1, cp_model.Proto().constraints(0).all_diff().exprs(2).vars(0));
}

TEST(CpModelTest, TestVariableElement) {
  CpModelBuilder cp_model;
  const IntVar index = cp_model.NewIntVar({0, 2}).WithName("index");
  const IntVar x = cp_model.NewIntVar({0, 20}).WithName("x");
  const IntVar y = cp_model.NewIntVar({0, 20}).WithName("y");
  const IntVar z = cp_model.NewIntVar({0, 20}).WithName("z");
  const IntVar target = cp_model.NewIntVar({5, 15}).WithName("target");
  cp_model.AddVariableElement(index, {x, z, y}, target);

  ASSERT_EQ(1, cp_model.Proto().constraints_size());
  const ElementConstraintProto& element =
      cp_model.Proto().constraints(0).element();
  EXPECT_EQ(3, element.exprs_size());
  EXPECT_EQ(0, element.linear_index().vars(0));
  EXPECT_EQ(1, element.exprs(0).vars(0));
  EXPECT_EQ(3, element.exprs(1).vars(0));
  EXPECT_EQ(2, element.exprs(2).vars(0));
  EXPECT_EQ(4, element.linear_target().vars(0));
}

TEST(CpModelTest, TestExprElement) {
  CpModelBuilder cp_model;
  const IntVar index = cp_model.NewIntVar({0, 2}).WithName("index");
  const IntVar x = cp_model.NewIntVar({0, 20}).WithName("x");
  const IntVar y = cp_model.NewIntVar({0, 20}).WithName("y");
  const IntVar z = cp_model.NewIntVar({0, 20}).WithName("z");
  const IntVar target = cp_model.NewIntVar({5, 15}).WithName("target");
  cp_model.AddElement(2 * index - 1, {-x, 2 * z, y + 2, 11}, 5 - target);

  ASSERT_EQ(1, cp_model.Proto().constraints_size());
  const ElementConstraintProto& element =
      cp_model.Proto().constraints(0).element();
  EXPECT_EQ(4, element.exprs_size());
  EXPECT_EQ(0, element.linear_index().vars(0));
  EXPECT_EQ(2, element.linear_index().coeffs(0));
  EXPECT_EQ(-1, element.linear_index().offset());
  EXPECT_EQ(1, element.exprs(0).vars(0));
  EXPECT_EQ(-1, element.exprs(0).coeffs(0));
  EXPECT_EQ(0, element.exprs(0).offset());
  EXPECT_EQ(3, element.exprs(1).vars(0));
  EXPECT_EQ(2, element.exprs(1).coeffs(0));
  EXPECT_EQ(0, element.exprs(1).offset());
  EXPECT_EQ(2, element.exprs(2).vars(0));
  EXPECT_EQ(1, element.exprs(2).coeffs(0));
  EXPECT_EQ(2, element.exprs(2).offset());
  EXPECT_EQ(11, element.exprs(3).offset());
  EXPECT_EQ(4, element.linear_target().vars(0));
  EXPECT_EQ(-1, element.linear_target().coeffs(0));
  EXPECT_EQ(5, element.linear_target().offset());
}

TEST(CpModelTest, TestExprElementWithOnlyVarAndConstants) {
  CpModelBuilder cp_model;
  const IntVar index = cp_model.NewIntVar({0, 2}).WithName("index");
  const IntVar x = cp_model.NewIntVar({0, 20}).WithName("x");
  const IntVar y = cp_model.NewIntVar({0, 20}).WithName("y");
  const IntVar z = cp_model.NewIntVar({0, 20}).WithName("z");
  const IntVar target = cp_model.NewIntVar({5, 15}).WithName("target");
  cp_model.AddElement(2 * index - 1, {x, z, y, 11}, 5 - target);

  ASSERT_EQ(1, cp_model.Proto().constraints_size());
  const ElementConstraintProto& element =
      cp_model.Proto().constraints(0).element();
  EXPECT_EQ(4, element.exprs_size());
  EXPECT_EQ(0, element.linear_index().vars(0));
  EXPECT_EQ(2, element.linear_index().coeffs(0));
  EXPECT_EQ(-1, element.linear_index().offset());
  EXPECT_EQ(1, element.exprs(0).vars(0));
  EXPECT_EQ(1, element.exprs(0).coeffs(0));
  EXPECT_EQ(0, element.exprs(0).offset());
  EXPECT_EQ(3, element.exprs(1).vars(0));
  EXPECT_EQ(1, element.exprs(1).coeffs(0));
  EXPECT_EQ(0, element.exprs(1).offset());
  EXPECT_EQ(2, element.exprs(2).vars(0));
  EXPECT_EQ(1, element.exprs(2).coeffs(0));
  EXPECT_EQ(0, element.exprs(2).offset());
  EXPECT_EQ(11, element.exprs(3).offset());
  EXPECT_EQ(4, element.linear_target().vars(0));
  EXPECT_EQ(-1, element.linear_target().coeffs(0));
  EXPECT_EQ(5, element.linear_target().offset());
}

TEST(CpModelTest, TestElement) {
  CpModelBuilder cp_model;
  const IntVar index = cp_model.NewIntVar({0, 2}).WithName("index");
  const IntVar target = cp_model.NewIntVar({5, 15}).WithName("target");
  cp_model.AddElement(index, {1, 12, 5}, target);

  ASSERT_EQ(1, cp_model.Proto().constraints_size());
  const ElementConstraintProto& element =
      cp_model.Proto().constraints(0).element();
  EXPECT_EQ(3, element.exprs_size());
  EXPECT_EQ(1, element.exprs(0).offset());
  EXPECT_EQ(12, element.exprs(1).offset());
  EXPECT_EQ(5, element.exprs(2).offset());
  EXPECT_EQ(0, element.linear_index().vars(0));
  EXPECT_EQ(1, element.linear_target().vars(0));
}

TEST(CpModelTest, TestElementWithBooleanVar) {
  CpModelBuilder cp_model;
  const IntVar index = cp_model.NewIntVar({0, 2}).WithName("index");
  const BoolVar target = cp_model.NewBoolVar().WithName("target");
  cp_model.AddElement(index, {1, 0, 1}, IntVar(Not(target)));
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables {
      name: "index"
      domain: [ 0, 2 ]
    }
    variables {
      name: "target"
      domain: [ 0, 1 ]
    }
    variables {
      name: "Not(target)"
      domain: [ 0, 1 ]
    }
    constraints {
      linear {
        vars: [ 2, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 1, 1 ]
      }
    }
    constraints {
      element {
        linear_index: { vars: 0 coeffs: 1 }
        linear_target { vars: 2 coeffs: 1 }
        exprs { offset: 1 }
        exprs {}
        exprs { offset: 1 }
      }
    }
  )pb");
  EXPECT_THAT(cp_model.Proto(), EqualsProto(expected_model));
}

TEST(CpModelTest, TestElementValuesFromInt64Vector) {
  CpModelBuilder cp_model;
  const IntVar index = cp_model.NewIntVar({0, 2}).WithName("index");
  const IntVar target = cp_model.NewIntVar({5, 15}).WithName("target");
  const std::vector<int64_t> values = {1, 12, 5};
  cp_model.AddElement(index, values, target);

  ASSERT_EQ(1, cp_model.Proto().constraints_size());
  const ElementConstraintProto& element =
      cp_model.Proto().constraints(0).element();
  EXPECT_EQ(3, element.exprs_size());
  EXPECT_EQ(1, element.exprs(0).offset());
  EXPECT_EQ(12, element.exprs(1).offset());
  EXPECT_EQ(5, element.exprs(2).offset());
  EXPECT_EQ(0, element.linear_index().vars(0));
  EXPECT_EQ(1, element.linear_target().vars(0));
}

TEST(CpModelTest, TestCircuit) {
  CpModelBuilder cp_model;
  std::vector<BoolVar> vars;
  for (int i = 0; i < 3; ++i) {
    vars.push_back(cp_model.NewBoolVar());
  }

  CircuitConstraint circuit = cp_model.AddCircuitConstraint();
  circuit.AddArc(0, 0, vars[0]);
  circuit.AddArc(0, 1, vars[1]);
  circuit.AddArc(1, 0, vars[2]);

  const CpSolverResponse response = Solve(cp_model.Build());
  ASSERT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_FALSE(SolutionBooleanValue(response, vars[0]));
  EXPECT_TRUE(SolutionBooleanValue(response, vars[1]));
  EXPECT_TRUE(SolutionBooleanValue(response, vars[2]));
}

TEST(CpModelTest, TestAllowedAssignment) {
  CpModelBuilder cp_model;
  std::vector<IntVar> vars;
  for (int i = 0; i < 3; ++i) {
    vars.push_back(cp_model.NewIntVar({0, 3}));
  }
  TableConstraint ct = cp_model.AddAllowedAssignments(vars);
  ct.AddTuple({1, 1, 2});
  ct.AddTuple({0, 1, 2});
  ct.AddTuple({4, 1, 2});  // Tuple is invalid.
  ct.AddTuple({2, 1, 0});

  Model model;

  // Tell the solver to enumerate all solutions.
  SatParameters parameters;
  parameters.set_enumerate_all_solutions(true);
  parameters.set_num_workers(1);
  model.Add(NewSatParameters(parameters));

  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver(
      [&](const CpSolverResponse& /*r*/) { num_solutions++; }));
  const CpSolverResponse response = SolveCpModel(cp_model.Build(), &model);
  EXPECT_EQ(num_solutions, 3);
}

TEST(CpModelTest, TestForbiddenAssignments) {
  CpModelBuilder cp_model;
  std::vector<IntVar> vars;
  for (int i = 0; i < 3; ++i) {
    vars.push_back(cp_model.NewIntVar({0, 3}));
  }
  TableConstraint ct = cp_model.AddForbiddenAssignments(vars);
  ct.AddTuple({1, 1, 2});
  ct.AddTuple({0, 1, 2});
  ct.AddTuple({4, 1, 2});  // Tuple is invalid.
  ct.AddTuple({2, 1, 0});

  Model model;

  // Tell the solver to enumerate all solutions.
  SatParameters parameters;
  parameters.set_enumerate_all_solutions(true);
  parameters.set_num_workers(1);
  model.Add(NewSatParameters(parameters));

  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver(
      [&](const CpSolverResponse&) { num_solutions++; }));
  const CpSolverResponse response = SolveCpModel(cp_model.Build(), &model);
  EXPECT_EQ(num_solutions, 4 * 4 * 4 - 3);
}

TEST(CpModelTest, TestInverseConstraint) {
  const int kNumVars = 4;
  CpModelBuilder cp_model;
  std::vector<IntVar> vars;
  std::vector<IntVar> i_vars;
  for (int i = 0; i < kNumVars; ++i) {
    vars.push_back(cp_model.NewIntVar({0, kNumVars - 1}));
    i_vars.push_back(cp_model.NewIntVar({0, kNumVars - 1}));
  }
  cp_model.AddInverseConstraint(vars, i_vars);
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    variables { domain: [ 0, 3 ] }
    constraints {
      inverse {
        f_direct: [ 0, 2, 4, 6 ],
        f_inverse: [ 1, 3, 5, 7 ]
      }
    }
  )pb");
  EXPECT_THAT(cp_model.Proto(), EqualsProto(expected_model));
}

TEST(CpModelTest, TestReservoirConstraint) {
  CpModelBuilder cp_model;
  ReservoirConstraint reservoir = cp_model.AddReservoirConstraint(1, 5);
  reservoir.AddEvent(cp_model.NewIntVar({0, 10}), 1);
  reservoir.AddEvent(cp_model.NewIntVar({0, 10}), 2);
  const BoolVar is_active = cp_model.NewBoolVar();
  reservoir.AddOptionalEvent(cp_model.NewIntVar({0, 10}), -3, is_active);

  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      reservoir {
        time_exprs: { vars: 0 coeffs: 1 }
        time_exprs: { vars: 2 coeffs: 1 }
        time_exprs: { vars: 4 coeffs: 1 }
        level_changes: { offset: 1 }
        level_changes: { offset: 2 }
        level_changes: { offset: -3 }
        active_literals: [ 1, 1, 3 ],
        min_level: 1,
        max_level: 5
      }
    }
  )pb");
  EXPECT_THAT(cp_model.Proto(), EqualsProto(expected_model));
}

TEST(CpModelTest, TestIntMax) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({0, 20});
  const IntVar y = cp_model.NewIntVar({0, 20});
  const IntVar z = cp_model.NewIntVar({0, 20});
  cp_model.AddMaxEquality(x, {y, z});
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 20 ] }
    constraints {
      lin_max {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");
  EXPECT_THAT(cp_model.Proto(), EqualsProto(expected_model));
}

TEST(CpModelTest, TestIntMin) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({0, 20});
  const IntVar y = cp_model.NewIntVar({0, 20});
  const IntVar z = cp_model.NewIntVar({0, 20});
  cp_model.AddMinEquality(x, {y, z});
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 20 ] }
    constraints {
      lin_max {
        target { vars: 0 coeffs: -1 }
        exprs { vars: 1 coeffs: -1 }
        exprs { vars: 2 coeffs: -1 }
      }
    }
  )pb");
  EXPECT_THAT(cp_model.Proto(), EqualsProto(expected_model));
}

TEST(CpModelTest, TestIntDiv) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({0, 20});
  const IntVar y = cp_model.NewIntVar({0, 20});
  const IntVar z = cp_model.NewIntVar({0, 20});
  cp_model.AddDivisionEquality(x, y, z);
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 20 ] }
    constraints {
      int_div {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");
  EXPECT_THAT(cp_model.Proto(), EqualsProto(expected_model));
}

TEST(CpModelTest, TestIntAbs) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({0, 20});
  const IntVar y = cp_model.NewIntVar({-20, 20});
  cp_model.AddAbsEquality(x, y);
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 20 ] }
    variables { domain: [ -20, 20 ] }
    constraints {
      lin_max {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 1 coeffs: -1 }
      }
    }
  )pb");
  EXPECT_THAT(cp_model.Proto(), EqualsProto(expected_model));
}

TEST(CpModelTest, TestIntModulo) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({0, 20});
  const IntVar y = cp_model.NewIntVar({0, 20});
  const IntVar z = cp_model.NewIntVar({0, 20});
  cp_model.AddModuloEquality(x, y, z);
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 20 ] }
    constraints {
      int_mod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");
  EXPECT_THAT(cp_model.Proto(), EqualsProto(expected_model));
}

TEST(CpModelTest, TestIntProd) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({0, 20});
  const IntVar y = cp_model.NewIntVar({0, 20});
  const IntVar z = cp_model.NewIntVar({0, 20});
  cp_model.AddMultiplicationEquality(x, {y, z});
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 20 ] }
    constraints {
      int_prod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");
  EXPECT_THAT(cp_model.Proto(), EqualsProto(expected_model));
}

TEST(CpModelTest, TestIntProdLeftRight) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({0, 20});
  const IntVar y = cp_model.NewIntVar({0, 20});
  const IntVar z = cp_model.NewIntVar({0, 20});
  cp_model.AddMultiplicationEquality(x, y, z);
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 20 ] }
    constraints {
      int_prod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");
  EXPECT_THAT(cp_model.Proto(), EqualsProto(expected_model));
}

TEST(CpModelTest, TestIntProdVar) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({0, 20});
  const IntVar y = cp_model.NewIntVar({0, 20});
  const IntVar z = cp_model.NewIntVar({0, 20});
  cp_model.AddMultiplicationEquality(x, {y, z});
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 0, 20 ] }
    constraints {
      int_prod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");
  EXPECT_THAT(cp_model.Proto(), EqualsProto(expected_model));
}

TEST(IntervalVarTest, NullAPI) {
  IntervalVar var;
  EXPECT_EQ(var.Name(), "null");
  EXPECT_EQ(var.DebugString(), "null");
  if (!DEBUG_MODE) {
    // We don't crash, but we only return expression at zero.
    EXPECT_TRUE(var.StartExpr().IsConstant());
    EXPECT_TRUE(var.EndExpr().IsConstant());
    EXPECT_TRUE(var.SizeExpr().IsConstant());

    // And the presence is the undefined BoolVar().
    EXPECT_EQ(var.PresenceBoolVar(), BoolVar());
  }
}

TEST(CpModelTest, TestIntervalVar) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({0, 20}).WithName("x");
  const IntVar y = cp_model.NewIntVar({0, 20}).WithName("y");
  const IntVar z = cp_model.NewIntVar({0, 20}).WithName("z");
  const IntervalVar interval =
      cp_model.NewIntervalVar(x, y, z).WithName("interval");
  EXPECT_EQ(interval.StartExpr().variables()[0], x.index());
  EXPECT_EQ(interval.SizeExpr().variables()[0], y.index());
  EXPECT_EQ(interval.EndExpr().variables()[0], z.index());
  EXPECT_EQ(interval.PresenceBoolVar(), cp_model.TrueVar());
  EXPECT_EQ("interval", interval.Name());
  EXPECT_EQ(interval.DebugString(),
            "interval(x(0, 20), y(0, 20), z(0, 20), true)");
  const IntVar t = cp_model.NewIntVar({0, 20}).WithName("t");
  const IntervalVar other = cp_model.NewIntervalVar(x, 5, t);
  EXPECT_EQ(other.DebugString(), "IntervalVar1(x(0, 20), 5, t(0, 20), true)");
  EXPECT_NE(interval, other);
  EXPECT_EQ(interval, interval);
}

TEST(CpModelTest, TestBooleanIntervalVar) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({0, 20}).WithName("x");
  const BoolVar y = cp_model.NewBoolVar().WithName("y");
  const IntVar z = cp_model.NewIntVar({0, 20}).WithName("z");
  const IntervalVar interval =
      cp_model.NewIntervalVar(x, Not(y), z).WithName("interval");
  EXPECT_THAT(interval.StartExpr().variables(), ElementsAre(x.index()));
  EXPECT_EQ(interval.SizeExpr().variables()[0], y.index());
  EXPECT_EQ(interval.SizeExpr().coefficients()[0], -1);
  EXPECT_EQ(interval.SizeExpr().constant(), 1);
  EXPECT_THAT(interval.EndExpr().variables(), ElementsAre(z.index()));
  EXPECT_EQ(interval.PresenceBoolVar(), cp_model.TrueVar());
  EXPECT_EQ(interval.DebugString(),
            "interval(x(0, 20), -y(0, 1) + 1, z(0, 20), true)");
}

TEST(CpModelTest, TestOptionalIntervalVar) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({0, 20}).WithName("x");
  const IntVar y = cp_model.NewIntVar({0, 20}).WithName("y");
  const IntVar z = cp_model.NewIntVar({0, 20}).WithName("z");
  const BoolVar b = cp_model.NewBoolVar().WithName("b");
  const IntervalVar interval =
      cp_model.NewOptionalIntervalVar(x, y, z, b).WithName("interval");
  EXPECT_THAT(interval.StartExpr().variables(), ElementsAre(x.index()));
  EXPECT_THAT(interval.SizeExpr().variables(), ElementsAre(y.index()));
  EXPECT_THAT(interval.EndExpr().variables(), ElementsAre(z.index()));
  EXPECT_EQ(interval.PresenceBoolVar(), b);
  EXPECT_EQ("interval", interval.Name());
}

TEST(CpModelTest, TestNoOverlap) {
  CpModelBuilder cp_model;
  IntVar x_start = cp_model.NewIntVar({0, 20});
  IntVar x_end = cp_model.NewIntVar({0, 20});
  const IntervalVar x = cp_model.NewIntervalVar(x_start, 5, x_end);
  IntVar y_start = cp_model.NewIntVar({0, 20});
  IntVar y_end = cp_model.NewIntVar({0, 20});
  const IntervalVar y = cp_model.NewIntervalVar(y_start, 5, y_end);
  IntVar z_start = cp_model.NewIntVar({0, 20});
  IntVar z_end = cp_model.NewIntVar({0, 20});
  const IntervalVar z = cp_model.NewIntervalVar(z_start, 5, z_end);
  NoOverlapConstraint ct = cp_model.AddNoOverlap({x, y});
  ct.AddInterval(z);
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 20 }
    variables { domain: 0 domain: 20 }
    variables { domain: 0 domain: 20 }
    variables { domain: 0 domain: 20 }
    variables { domain: 0 domain: 20 }
    variables { domain: 0 domain: 20 }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 1 coeffs: 1 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        end { vars: 3 coeffs: 1 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 4 coeffs: 1 }
        end { vars: 5 coeffs: 1 }
        size { offset: 5 }
      }
    }
    constraints { no_overlap { intervals: 0 intervals: 1 intervals: 2 } })pb");
  EXPECT_THAT(cp_model.Proto(), EqualsProto(expected_model));
}

TEST(CpModelTest, TestNoOverlap2D) {
  CpModelBuilder cp_model;
  IntVar x_start = cp_model.NewIntVar({0, 20});
  IntVar x_end = cp_model.NewIntVar({0, 20});
  const IntervalVar x =
      cp_model.NewIntervalVar(x_start, cp_model.NewConstant(5), x_end);
  IntVar y_start = cp_model.NewIntVar({0, 20});
  IntVar y_end = cp_model.NewIntVar({0, 20});
  const IntervalVar y =
      cp_model.NewIntervalVar(y_start, cp_model.NewConstant(5), y_end);
  IntVar z_start = cp_model.NewIntVar({0, 20});
  IntVar z_end = cp_model.NewIntVar({0, 20});
  const IntervalVar z =
      cp_model.NewIntervalVar(z_start, cp_model.NewConstant(5), z_end);
  IntVar t_start = cp_model.NewIntVar({0, 20});
  IntVar t_end = cp_model.NewIntVar({0, 20});
  const IntervalVar t =
      cp_model.NewIntervalVar(t_start, cp_model.NewConstant(5), t_end);

  NoOverlap2DConstraint ct = cp_model.AddNoOverlap2D();
  ct.AddRectangle(x, y);
  ct.AddRectangle(z, t);

  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 20 }
    variables { domain: 0 domain: 20 }
    variables { domain: 5 domain: 5 }
    variables { domain: 0 domain: 20 }
    variables { domain: 0 domain: 20 }
    variables { domain: 0 domain: 20 }
    variables { domain: 0 domain: 20 }
    variables { domain: 0 domain: 20 }
    variables { domain: 0 domain: 20 }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 1 coeffs: 1 }
        size { vars: 2 coeffs: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 3 coeffs: 1 }
        end { vars: 4 coeffs: 1 }
        size { vars: 2 coeffs: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 5 coeffs: 1 }
        end { vars: 6 coeffs: 1 }
        size { vars: 2 coeffs: 1 }
      }
    }
    constraints {
      interval {
        start { vars: 7 coeffs: 1 }
        end { vars: 8 coeffs: 1 }
        size { vars: 2 coeffs: 1 }
      }
    }
    constraints {
      no_overlap_2d {
        x_intervals: 0
        x_intervals: 2
        y_intervals: 1
        y_intervals: 3
      }
    })pb");
  EXPECT_THAT(cp_model.Proto(), EqualsProto(expected_model));
}

TEST(CpModelTest, TestCumulative) {
  CpModelBuilder cp_model;
  IntVar x_start = cp_model.NewIntVar({0, 20});
  IntVar x_end = cp_model.NewIntVar({0, 20});
  const IntervalVar x = cp_model.NewIntervalVar(x_start, 5, x_end);
  IntVar y_start = cp_model.NewIntVar({0, 20});
  IntVar y_end = cp_model.NewIntVar({0, 20});
  const IntervalVar y = cp_model.NewIntervalVar(y_start, 5, y_end);
  const IntVar a = cp_model.NewIntVar({5, 10});
  const IntVar b = cp_model.NewIntVar({5, 10});

  CumulativeConstraint cumul = cp_model.AddCumulative(a);
  cumul.AddDemand(x, b);
  cumul.AddDemand(y, 8);

  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 20 }
    variables { domain: 0 domain: 20 }
    variables { domain: 0 domain: 20 }
    variables { domain: 0 domain: 20 }
    variables { domain: 5 domain: 10 }
    variables { domain: 5 domain: 10 }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 1 coeffs: 1 }
        size { offset: 5 }
      }
    }
    constraints {
      interval {
        start { vars: 2 coeffs: 1 }
        end { vars: 3 coeffs: 1 }
        size { offset: 5 }
      }
    }
    constraints {
      cumulative {
        capacity { vars: 4 coeffs: 1 }
        intervals: 0
        intervals: 1
        demands { vars: 5 coeffs: 1 }
        demands { offset: 8 }
      }
    })pb");
  EXPECT_THAT(cp_model.Proto(), EqualsProto(expected_model));
}

TEST(CpModelTest, RabbitsAndPheasants) {
  CpModelBuilder cp_model;
  Domain all_animals(0, 20);
  const IntVar rabbits = cp_model.NewIntVar(all_animals).WithName("rabbits");
  const IntVar pheasants =
      cp_model.NewIntVar(all_animals).WithName("pheasants");

  cp_model.AddEquality(LinearExpr::Sum({rabbits, pheasants}), 20);
  cp_model.AddEquality(LinearExpr::WeightedSum({rabbits, pheasants}, {4, 2}),
                       56);

  const CpSolverResponse response = Solve(cp_model.Build());
  ASSERT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(SolutionIntegerValue(response, rabbits), 8);
  EXPECT_EQ(SolutionIntegerValue(response, pheasants), 12);
}

TEST(CpModelTest, BoolAnd) {
  CpModelBuilder cp_model;
  BoolVar a = cp_model.NewBoolVar();
  BoolVar b = cp_model.NewBoolVar();
  cp_model.AddBoolAnd({a, b});
  const CpSolverResponse response = Solve(cp_model.Build());
  ASSERT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(CpModelTest, Min) {
  CpModelBuilder cp_model;
  IntVar x0 = cp_model.NewIntVar(Domain(1, 2));
  IntVar x1 = cp_model.NewIntVar(Domain(0, 1));
  IntVar x2 = cp_model.NewIntVar(Domain(-2, -1));
  IntVar target = cp_model.NewIntVar(Domain(-3, 0));

  LinearExpr expr1;
  expr1 += x0 * 2;
  expr1 += x1 * 3;
  expr1 += -5;

  LinearExpr expr2;
  expr2 += x1 * 2;
  expr2 += x2 * -5;
  expr2 += 6;

  LinearExpr expr3;
  expr3 += x0 * 2;
  expr3 += x2 * 3;
  expr3 += 0;

  std::vector<LinearExpr> exprs = {expr1, expr2, expr3};
  cp_model.AddMinEquality(target, exprs);

  Model model;

  // Tell the solver to enumerate all solutions.
  SatParameters parameters;
  parameters.set_enumerate_all_solutions(true);
  parameters.set_num_workers(1);
  parameters.set_linearization_level(2);
  model.Add(NewSatParameters(parameters));

  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver(
      [&](const CpSolverResponse&) { num_solutions++; }));

  const CpSolverResponse& response = SolveCpModel(cp_model.Build(), &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  int expected_num_solutions = 0;
  for (int x0 = 1; x0 <= 2; ++x0) {
    for (int x1 = 0; x1 <= 1; ++x1) {
      for (int x2 = -2; x2 <= -1; ++x2) {
        for (int target = -3; target <= 0; ++target) {
          if (target == std::min({2 * x0 + 3 * x1 - 5, 2 * x1 - 5 * x2 + 6,
                                  2 * x0 + 3 * x2})) {
            expected_num_solutions++;
          }
        }
      }
    }
  }
  EXPECT_EQ(expected_num_solutions, num_solutions);
}

TEST(CpModelTest, Min2) {
  CpModelBuilder cp_model;
  IntVar x0 = cp_model.NewIntVar(Domain(0, 6));
  IntVar x1 = cp_model.NewIntVar(Domain(0, 6));
  IntVar x2 = cp_model.NewIntVar(Domain(0, 6));
  IntVar target = cp_model.NewIntVar(Domain(5, 5));

  LinearExpr expr1;
  expr1 += x0 * 1;
  expr1 += x1 * 1;
  expr1 += x2 * 1;
  expr1 += 0;

  LinearExpr expr2;
  expr2 += 100;

  std::vector<LinearExpr> exprs = {expr1, expr2};
  cp_model.AddMinEquality(target, exprs);

  Model model;

  // Tell the solver to enumerate all solutions.
  SatParameters parameters;
  parameters.set_cp_model_presolve(false);
  parameters.set_enumerate_all_solutions(true);
  parameters.set_num_workers(1);
  model.Add(NewSatParameters(parameters));

  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver(
      [&](const CpSolverResponse&) { num_solutions++; }));

  const CpSolverResponse& response = SolveCpModel(cp_model.Build(), &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  int expected_num_solutions = 0;
  for (int x0 = 0; x0 <= 6; ++x0) {
    for (int x1 = 0; x1 <= 6; ++x1) {
      for (int x2 = 0; x2 <= 6; ++x2) {
        for (int target = 5; target <= 5; ++target) {
          if (target == std::min({x0 + x1 + x2, 100})) {
            expected_num_solutions++;
          }
        }
      }
    }
  }
  EXPECT_EQ(expected_num_solutions, num_solutions);
}

TEST(CpModelTest, Max) {
  CpModelBuilder cp_model;
  IntVar x0 = cp_model.NewIntVar(Domain(1, 2));
  IntVar x1 = cp_model.NewIntVar(Domain(0, 1));
  IntVar x2 = cp_model.NewIntVar(Domain(-2, -1));
  IntVar target = cp_model.NewIntVar(Domain(-3, 0));

  LinearExpr expr1;
  expr1 += x0 * 2;
  expr1 += x1 * 3;
  expr1 += -5;

  LinearExpr expr2;
  expr2 += x1 * 2;
  expr2 += x2 * 5;
  expr2 += 6;

  LinearExpr expr3;
  expr3 += x0 * 2;
  expr3 += x2 * 3;
  expr3 += 0;

  std::vector<LinearExpr> exprs = {expr1, expr2, expr3};
  cp_model.AddMaxEquality(target, exprs);

  Model model;

  // Tell the solver to enumerate all solutions.
  SatParameters parameters;
  parameters.set_enumerate_all_solutions(true);
  parameters.set_num_workers(1);
  model.Add(NewSatParameters(parameters));

  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver(
      [&](const CpSolverResponse&) { num_solutions++; }));

  const CpSolverResponse& response = SolveCpModel(cp_model.Build(), &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  int expected_num_solutions = 0;
  for (int x0 = 1; x0 <= 2; ++x0) {
    for (int x1 = 0; x1 <= 1; ++x1) {
      for (int x2 = -2; x2 <= -1; ++x2) {
        for (int target = -3; target <= 0; ++target) {
          if (target == std::max({2 * x0 + 3 * x1 - 5, 2 * x1 + 5 * x2 + 6,
                                  2 * x0 + 3 * x2})) {
            expected_num_solutions++;
          }
        }
      }
    }
  }
  EXPECT_EQ(expected_num_solutions, num_solutions);
}

TEST(CpModelTest, MinExpression) {
  CpModelBuilder cp_model;
  IntVar x0 = cp_model.NewIntVar(Domain(1, 2));
  IntVar x1 = cp_model.NewIntVar(Domain(0, 1));
  IntVar y0 = cp_model.NewIntVar(Domain(1, 2));
  IntVar y1 = cp_model.NewIntVar(Domain(-1, 0));

  LinearExpr target;
  target -= y0 * 2;
  target += y1 * 1;
  target += 2;

  LinearExpr expr1;
  expr1 += x0 * 2;
  expr1 += x1 * 3;
  expr1 -= 5;

  LinearExpr expr2;
  expr2 += x0 * 2;
  expr2 += x1 * 1;
  expr2 -= 4;

  std::vector<LinearExpr> exprs = {expr1, expr2};
  cp_model.AddMinEquality(target, exprs);

  Model model;

  // Tell the solver to enumerate all solutions.
  SatParameters parameters;
  parameters.set_enumerate_all_solutions(true);
  parameters.set_num_workers(1);
  model.Add(NewSatParameters(parameters));

  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver(
      [&](const CpSolverResponse&) { num_solutions++; }));

  const CpSolverResponse& response = SolveCpModel(cp_model.Build(), &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  int expected_num_solutions = 0;
  for (int x0 = 1; x0 <= 2; ++x0) {
    for (int x1 = 0; x1 <= 1; ++x1) {
      for (int y0 = 1; y0 <= 2; ++y0) {
        for (int y1 = -1; y1 <= 0; ++y1) {
          if ((-2 * y0 + y1 + 2) ==
              std::min(2 * x0 + 3 * x1 - 5, 2 * x0 + x1 - 4)) {
            expected_num_solutions++;
          }
        }
      }
    }
  }
  EXPECT_EQ(expected_num_solutions, num_solutions);
}

TEST(CpModelTest, MaxExpression) {
  CpModelBuilder cp_model;
  IntVar x0 = cp_model.NewIntVar(Domain(1, 2));
  IntVar x1 = cp_model.NewIntVar(Domain(0, 1));
  IntVar y0 = cp_model.NewIntVar(Domain(1, 2));
  IntVar y1 = cp_model.NewIntVar(Domain(-1, 0));

  LinearExpr target;
  target -= y0 * 2;
  target += y1 * 1;
  target += 2;

  LinearExpr expr1;
  expr1 += x0 * 2;
  expr1 += x1 * 3;
  expr1 -= 5;

  LinearExpr expr2;
  expr2 += x0 * 2;
  expr2 += x1 * 1;
  expr2 -= 4;

  std::vector<LinearExpr> exprs = {expr1, expr2};
  cp_model.AddMaxEquality(target, exprs);

  Model model;

  // Tell the solver to enumerate all solutions.
  SatParameters parameters;
  parameters.set_enumerate_all_solutions(true);
  parameters.set_num_workers(1);
  model.Add(NewSatParameters(parameters));

  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver(
      [&](const CpSolverResponse&) { num_solutions++; }));

  const CpSolverResponse& response = SolveCpModel(cp_model.Build(), &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  int expected_num_solutions = 0;
  for (int x0 = 1; x0 <= 2; ++x0) {
    for (int x1 = 0; x1 <= 1; ++x1) {
      for (int y0 = 1; y0 <= 2; ++y0) {
        for (int y1 = -1; y1 <= 0; ++y1) {
          if ((-2 * y0 + y1 + 2) ==
              std::max(2 * x0 + 3 * x1 - 5, 2 * x0 + x1 - 4)) {
            expected_num_solutions++;
          }
        }
      }
    }
  }
  EXPECT_EQ(expected_num_solutions, num_solutions);
}

TEST(CpModelTest, MinExpressionInfeasible) {
  CpModelBuilder cp_model;
  IntVar x0 = cp_model.NewIntVar(Domain(1, 2));
  IntVar x1 = cp_model.NewIntVar(Domain(0, 1));
  IntVar y0 = cp_model.NewIntVar(Domain(1, 2));
  IntVar y1 = cp_model.NewIntVar(Domain(0, 0));

  LinearExpr target;
  target -= y0 * 2;
  target += y1 * 1;
  target += 2;

  LinearExpr expr1;
  expr1 += x0 * 2;
  expr1 += x1 * 3;
  expr1 -= 5;

  LinearExpr expr2;
  expr2 += x0 * 2;
  expr2 += x1 * 1;
  expr2 -= 4;

  std::vector<LinearExpr> exprs = {expr1, expr2};
  cp_model.AddMinEquality(target, exprs);

  const CpSolverResponse& response = Solve(cp_model.Build());
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(CpModelTest, MaxExpressionInfeasible) {
  CpModelBuilder cp_model;
  IntVar x0 = cp_model.NewIntVar(Domain(1, 2));
  IntVar x1 = cp_model.NewIntVar(Domain(0, 1));
  IntVar y0 = cp_model.NewIntVar(Domain(1, 2));
  IntVar y1 = cp_model.NewIntVar(Domain(-1, -1));

  LinearExpr target;
  target -= y0 * 2;
  target += y1 * 1;
  target += 2;

  LinearExpr expr1;
  expr1 += x0 * 2;
  expr1 += x1 * 3;
  expr1 -= 5;

  LinearExpr expr2;
  expr2 += x0 * 2;
  expr2 += x1 * 1;
  expr2 -= 4;

  std::vector<LinearExpr> exprs = {expr1, expr2};
  cp_model.AddMaxEquality(target, exprs);

  const CpSolverResponse& response = Solve(cp_model.Build());
  EXPECT_EQ(response.status(), CpSolverStatus::INFEASIBLE);
}

TEST(CpModelTest, Hinting) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({0, 20}).WithName("x");
  const IntVar y = cp_model.NewIntVar({0, 20}).WithName("y");
  cp_model.AddLinearConstraint(LinearExpr::Sum({x, y}), {1, 9});
  cp_model.AddHint(x, 4);
  cp_model.AddHint(y, 3);
  const CpSolverResponse response =
      SolveWithParameters(cp_model.Build(), "cp_model_presolve:false");
  ASSERT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(4, SolutionIntegerValue(response, x));
  EXPECT_EQ(3, SolutionIntegerValue(response, y));
}

TEST(CpModelTest, PositiveTable) {
  CpModelBuilder cp_model;

  std::vector<IntVar> vars;
  for (int i = 0; i < 3; ++i) {
    vars.push_back(cp_model.NewIntVar(Domain(0, 1)));
  }
  vars.push_back(cp_model.NewIntVar(Domain(0, 3)));

  TableConstraint ct = cp_model.AddAllowedAssignments(vars);
  const std::vector<std::vector<int64_t>> tuples = {
      {0, 0, 0, 0},
      {1, 1, 0, 2},
      {0, 0, 1, 3},
      {0, 1, 1, 3},
  };
  for (const std::vector<int64_t>& tuple : tuples) {
    ct.AddTuple(tuple);
  }

  Model model;

  // Tell the solver to enumerate all solutions.
  SatParameters parameters;
  parameters.set_enumerate_all_solutions(true);
  parameters.set_num_workers(1);
  parameters.set_cp_model_presolve(false);
  model.Add(NewSatParameters(parameters));

  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    LOG(INFO) << gtl::LogContainer(r.solution());
    num_solutions++;
  }));

  SolveCpModel(cp_model.Build(), &model);
  EXPECT_EQ(num_solutions, tuples.size());
}

TEST(CpModelTest, NegativeTable) {
  CpModelBuilder cp_model;

  std::vector<IntVar> vars;
  for (int i = 0; i < 3; ++i) {
    vars.push_back(cp_model.NewIntVar(Domain(0, 1)));
  }
  vars.push_back(cp_model.NewIntVar(Domain(0, 3)));

  TableConstraint ct = cp_model.AddForbiddenAssignments(vars);
  const std::vector<std::vector<int64_t>> tuples = {
      {0, 0, 0, 0},
      {1, 1, 0, 2},
      {0, 0, 1, 3},
      {0, 1, 1, 3},
  };
  for (const std::vector<int64_t>& tuple : tuples) {
    ct.AddTuple(tuple);
  }

  Model model;

  // Tell the solver to enumerate all solutions.
  SatParameters parameters;
  parameters.set_enumerate_all_solutions(true);
  parameters.set_num_workers(1);
  parameters.set_cp_model_presolve(false);
  model.Add(NewSatParameters(parameters));

  int num_solutions = 0;
  model.Add(NewFeasibleSolutionObserver([&](const CpSolverResponse& r) {
    LOG(INFO) << gtl::LogContainer(r.solution());
    num_solutions++;
  }));

  SolveCpModel(cp_model.Build(), &model);
  EXPECT_EQ(num_solutions, 32 - tuples.size());
}

TEST(CpModelTest, WrongPresolve) {
  CpModelBuilder cp_model;
  const IntVar word_var = cp_model.NewIntVar(Domain::FromValues({0, 1}));
  const std::vector<int64_t> weights{2, 1};
  const IntVar weight_var = cp_model.NewIntVar(Domain::FromValues({1, 2}));
  cp_model.AddElement(word_var, weights, weight_var);
  cp_model.Maximize(weight_var);
  SatParameters parameters;
  parameters.set_cp_model_presolve(false);
  parameters.set_log_search_progress(true);
  const CpSolverResponse response =
      SolveWithParameters(cp_model.Build(), parameters);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

TEST(CpModelTest, TestSearchStrategy) {
  CpModelBuilder cp_model;
  const IntVar i1 = cp_model.NewIntVar({0, 2}).WithName("i1");
  const IntVar i2 = cp_model.NewIntVar({0, 2}).WithName("i2");
  const IntVar i3 = cp_model.NewIntVar({0, 2}).WithName("i3");
  const BoolVar b1 = cp_model.NewBoolVar().WithName("b1");
  const BoolVar b2 = cp_model.NewBoolVar().WithName("b2");
  cp_model.AddDecisionStrategy({i2, i1, i3},
                               DecisionStrategyProto::CHOOSE_FIRST,
                               DecisionStrategyProto::SELECT_MIN_VALUE);
  cp_model.AddDecisionStrategy({b1, b2.Not()},
                               DecisionStrategyProto::CHOOSE_FIRST,
                               DecisionStrategyProto::SELECT_MAX_VALUE);
  const CpModelProto expected_model = ParseTestProto(R"pb(
    variables { name: "i1" domain: 0 domain: 2 }
    variables { name: "i2" domain: 0 domain: 2 }
    variables { name: "i3" domain: 0 domain: 2 }
    variables { name: "b1" domain: 0 domain: 1 }
    variables { name: "b2" domain: 0 domain: 1 }
    search_strategy {
      exprs { vars: 1 coeffs: 1 }
      exprs { vars: 0 coeffs: 1 }
      exprs { vars: 2 coeffs: 1 }
    }
    search_strategy {
      domain_reduction_strategy: SELECT_MAX_VALUE
      exprs { vars: 3 coeffs: 1 }
      exprs { vars: 4 coeffs: -1 offset: 1 }
    }
  )pb");
  EXPECT_THAT(cp_model.Proto(), EqualsProto(expected_model));
}

TEST(CpModelTest, DeepCopy) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({0, 20}).WithName("x");
  const BoolVar b = cp_model.NewBoolVar();
  cp_model.AddEquality(x, 10).OnlyEnforceIf(b);

  CpModelBuilder copy = cp_model.Clone();
  const IntVar copy_x = copy.GetIntVarFromProtoIndex(x.index());
  const BoolVar copy_b = copy.GetBoolVarFromProtoIndex(b.index());

  EXPECT_EQ(x.index(), copy_x.index());
  EXPECT_EQ(b.index(), copy_b.index());

  EXPECT_THAT(cp_model.Proto(), EqualsProto(copy.Proto()));
}

TEST(LinearExpr, NaturalApi) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({0, 20});
  const IntVar y = cp_model.NewIntVar({0, 20});
  const BoolVar b = cp_model.NewBoolVar();

  EXPECT_EQ(LinearExpr(3 * x + 4).DebugString(), "3 * V0 + 4");
  EXPECT_EQ(LinearExpr(-x).DebugString(), "-V0");
  EXPECT_EQ(LinearExpr(-(x + y)).DebugString(), "-V0 - V1");
  EXPECT_EQ(LinearExpr(-2 * (x + y)).DebugString(), "-2 * V0 - 2 * V1");
  EXPECT_EQ(LinearExpr(3 * x + b + 4).DebugString(), "3 * V0 + V2 + 4");
  EXPECT_EQ(LinearExpr(b * 2 + 3 * x).DebugString(), "2 * V2 + 3 * V0");
  EXPECT_EQ(LinearExpr(b + b + b.Not()).DebugString(), "V2 + V2 - V2 + 1");
}

TEST(LinearExpr, NaturalApiNegation) {
  CpModelBuilder cp_model;
  const IntVar x = cp_model.NewIntVar({0, 20});
  const IntVar y = cp_model.NewIntVar({0, 20});
  const IntVar z = cp_model.NewIntVar({0, 20});
  {
    LinearExpr a = x + y;
    LinearExpr b = x + y + z;
    EXPECT_EQ((std::move(a) - std::move(b)).DebugString(),
              "V0 + V1 - V0 - V1 - V2");
  }
  {
    LinearExpr a = x + y;
    LinearExpr b = x + y + z;
    EXPECT_EQ((std::move(a) - b).DebugString(), "V0 + V1 - V0 - V1 - V2");
  }
  {
    // Note that we re-order this one to optimize memory.
    LinearExpr a = x + y;
    LinearExpr b = x + y + z;
    EXPECT_EQ((a - std::move(b)).DebugString(), "-V0 - V1 - V2 + V0 + V1");
  }
  {
    LinearExpr a = x + y;
    LinearExpr b = x + y + z;
    EXPECT_EQ((std::move(b) - std::move(a)).DebugString(),
              "V0 + V1 + V2 - V0 - V1");
  }
}

TEST(LinearExpr, ComplexityIsOk) {
  // We rely on the move semantics to not be in O(n^2).
  // Note that this is not a code style to follow!
  CpModelBuilder cp_model;
  LinearExpr expr;
  for (int i = 0; i < 1e6; ++i) {
    expr = cp_model.NewBoolVar() + std::move(expr) +
           i * cp_model.NewIntVar({0, 20});
  }
}

TEST(Hints, HintIsComplete) {
  // Build model.
  CpModelBuilder model;
  IntVar start1 = model.NewIntVar(Domain(0, 10)).WithName("start1");
  IntVar length1 = model.NewIntVar(Domain(0, 10)).WithName("length1");
  IntVar end1 = model.NewIntVar(Domain(0, 10)).WithName("end1");
  IntVar start2 = model.NewIntVar(Domain(0, 10)).WithName("start2");
  IntVar length2 = model.NewIntVar(Domain(0, 10)).WithName("length2");
  IntVar end2 = model.NewIntVar(Domain(0, 10)).WithName("end2");
  model.NewIntervalVar(start1, length1, end1);
  model.NewIntervalVar(start2, length2, end2);
  // Add hint.
  model.AddHint(start1, 0);
  model.AddHint(length1, 4);
  model.AddHint(end1, 4);
  model.AddHint(start2, 4);
  model.AddHint(length2, 6);
  model.AddHint(end2, 10);
  // Solve model.
  SatParameters parameters;
  parameters.set_log_search_progress(true);
  parameters.set_log_to_response(true);
  parameters.set_num_workers(1);
  Model sat_model;
  const CpSolverResponse response =
      SolveWithParameters(model.Build(), parameters);
  EXPECT_THAT(response.solve_log(),
              HasSubstr("The solution hint is complete and is feasible."));
}

TEST(Hints, HintObjectiveValue) {
  // Build model.
  CpModelBuilder model;
  IntVar start1 = model.NewIntVar(Domain(0, 10)).WithName("start1");
  IntVar length1 = model.NewIntVar(Domain(0, 10)).WithName("length1");
  IntVar end1 = model.NewIntVar(Domain(0, 10)).WithName("end1");
  IntVar start2 = model.NewIntVar(Domain(0, 10)).WithName("start2");
  IntVar length2 = model.NewIntVar(Domain(0, 10)).WithName("length2");
  IntVar end2 = model.NewIntVar(Domain(0, 10)).WithName("end2");
  model.NewIntervalVar(start1, length1, end1);
  model.NewIntervalVar(start2, length2, end2);
  model.Minimize(start1 + length1 + end1 + start2 + length2 + end2);
  // Add hint.
  model.AddHint(start1, 0);
  model.AddHint(length1, 4);
  model.AddHint(end1, 4);
  model.AddHint(start2, 4);
  model.AddHint(length2, 6);
  model.AddHint(end2, 10);
  // Solve model.
  SatParameters parameters;
  parameters.set_log_search_progress(true);
  parameters.set_log_to_response(true);
  parameters.set_num_workers(1);
  Model sat_model;
  const CpSolverResponse response =
      SolveWithParameters(model.Build(), parameters);
  EXPECT_THAT(response.solve_log(),
              HasSubstr("The solution hint is complete and is feasible. Its "
                        "objective value is 28."));
}

TEST(CpModelTest, TestChaining) {
  CpModelBuilder cp_model;
  const BoolVar bool_var = cp_model.NewBoolVar();
  const IntVar int_var = cp_model.NewIntVar({0, 10});
  const LinearExpr expr = int_var;
  const IntervalVar interval_var = cp_model.NewIntervalVar(0, 10, 10);

  // Circuit
  cp_model.AddCircuitConstraint().AddArc(0, 1, bool_var).AddArc(1, 0, bool_var);

  // MultipleCircuit
  cp_model.AddMultipleCircuitConstraint()
      .AddArc(0, 1, bool_var)
      .AddArc(1, 0, bool_var);

  // Table
  cp_model.AddAllowedAssignments({int_var}).AddTuple({0}).AddTuple({1});

  // Reservoir
  cp_model.AddReservoirConstraint(0, 10).AddEvent(expr, 5).AddOptionalEvent(
      expr, -5, bool_var);

  // Automaton
  cp_model.AddAutomaton({int_var}, 0, {1})
      .AddTransition(0, 1, 0)
      .AddTransition(1, 0, 1);

  // NoOverlap
  cp_model.AddNoOverlap().AddInterval(interval_var).AddInterval(interval_var);

  // NoOverlap2D
  cp_model.AddNoOverlap2D()
      .AddRectangle(interval_var, interval_var)
      .AddRectangle(interval_var, interval_var);

  // Cumulative
  cp_model.AddCumulative(10)
      .AddDemand(interval_var, 5)
      .AddDemand(interval_var, 5);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
