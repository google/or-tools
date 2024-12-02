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

#include "ortools/sat/var_domination.h"

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::ElementsAre;
using ::testing::EqualsProto;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

// X + 2Y + Z = 0
// X + 2Z >= 2
//
// Doing (X--, Z++) is always beneficial if possible.
TEST(VarDominationTest, BasicExample1) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables {
      name: "X"
      domain: [ -10, 10 ]
    }
    variables {
      name: "Y"
      domain: [ -10, 10 ]
    }
    variables {
      name: "Z"
      domain: [ -10, 10 ]
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 2, 1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 2 ]
        coeffs: [ 1, 2 ]
        domain: [ 2, 9223372036854775807 ]
      }
    }
  )pb");
  VarDomination var_dom;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  ScanModelForDominanceDetection(context, &var_dom);

  const IntegerVariable X = VarDomination::RefToIntegerVariable(0);
  const IntegerVariable Y = VarDomination::RefToIntegerVariable(1);
  const IntegerVariable Z = VarDomination::RefToIntegerVariable(2);
  EXPECT_THAT(var_dom.DominatingVariables(X), ElementsAre(Z));
  EXPECT_THAT(var_dom.DominatingVariables(Y), IsEmpty());
  EXPECT_THAT(var_dom.DominatingVariables(Z), IsEmpty());
  EXPECT_THAT(var_dom.DominatingVariables(NegationOf(X)), IsEmpty());
  EXPECT_THAT(var_dom.DominatingVariables(NegationOf(Y)), IsEmpty());
  EXPECT_THAT(var_dom.DominatingVariables(NegationOf(Z)),
              ElementsAre(NegationOf(X)));
}

// X + 2Y + Z = 0
// X + 2Z >= 2
//
// Doing (X--, Z++) is always beneficial if possible.
TEST(VarDominationTest, ExploitDominanceRelation) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables {
      name: "X"
      domain: [ -10, 10 ]
    }
    variables {
      name: "Y"
      domain: [ -10, 10 ]
    }
    variables {
      name: "Z"
      domain: [ -10, 10 ]
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 2, 1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 2 ]
        coeffs: [ 1, 2 ]
        domain: [ 2, 9223372036854775807 ]
      }
    }
  )pb");
  VarDomination var_dom;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  context.UpdateNewConstraintsVariableUsage();
  ScanModelForDominanceDetection(context, &var_dom);
  EXPECT_TRUE(ExploitDominanceRelations(var_dom, &context));

  // Because X--, Z++ is always ok, we can exclude some value from Z using
  // equation X + 2Z >=2 we see that if Z=5, X >= -8, so we can decrease it,
  // but for Z = 6, X might be -10, so we are not sure.
  //
  // Also not that X can be 10 with Z at 10 too, so we cannot reduced the domain
  // of X.
  EXPECT_EQ(context.DomainOf(0).ToString(), "[-10,10]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[-10,10]");
  EXPECT_EQ(context.DomainOf(2).ToString(), "[6,10]");
}

// Same example as before but now Z has holes, which complicate a bit the
// final result.
TEST(VarDominationTest, ExploitDominanceRelationWithHoles) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables {
      name: "X"
      domain: [ -10, 10 ]
    }
    variables {
      name: "Y"
      domain: [ -10, 10 ]
    }
    variables {
      name: "Z"
      domain: [ -10, 0, 7, 10 ]
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 2, 1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 2 ]
        coeffs: [ 1, 2 ]
        domain: [ 2, 9223372036854775807 ]
      }
    }
  )pb");
  VarDomination var_dom;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  context.UpdateNewConstraintsVariableUsage();
  ScanModelForDominanceDetection(context, &var_dom);
  EXPECT_TRUE(ExploitDominanceRelations(var_dom, &context));

  // With hole, if Z is 0, we will not be able to increase it up to 6, so we
  // can't remove 0. If it is lower, we can safely increase it to zero though.
  EXPECT_EQ(context.DomainOf(0).ToString(), "[-10,10]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[-10,10]");
  EXPECT_EQ(context.DomainOf(2).ToString(), "[0][7,10]");
}

// X - 2Y >= -1
// X => Y
//
// Doing (X--, Y--) is always beneficial if possible.
TEST(VarDominationTest, ExploitDominanceOfImplicant) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables {
      name: "X"
      domain: [ 0, 1 ]
    }
    variables {
      name: "Y"
      domain: [ 0, 1 ]
    }
    constraints {
      enforcement_literal: 0
      bool_and { literals: [ 1 ] }
    }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, -2 ]
        domain: [ -1, 10 ]
      }
    }
    solution_hint {
      vars: [ 0, 1 ]
      values: [ 1, 1 ]
    }
  )pb");
  VarDomination var_dom;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  context.UpdateNewConstraintsVariableUsage();
  context.LoadSolutionHint();
  ScanModelForDominanceDetection(context, &var_dom);
  EXPECT_TRUE(ExploitDominanceRelations(var_dom, &context));

  const IntegerVariable X = VarDomination::RefToIntegerVariable(0);
  const IntegerVariable Y = VarDomination::RefToIntegerVariable(1);
  EXPECT_THAT(var_dom.DominatingVariables(X), ElementsAre(NegationOf(Y)));
  EXPECT_EQ(context.DomainOf(0).ToString(), "[0]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[0]");
  EXPECT_EQ(context.SolutionHint(0), 0);
  EXPECT_EQ(context.SolutionHint(1), 0);
}

// 2X - Y >= 0
// X => Y
//
// Doing (X++, Y++) is always beneficial if possible.
TEST(VarDominationTest, ExploitDominanceOfNegatedImplicand) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables {
      name: "X"
      domain: [ 0, 1 ]
    }
    variables {
      name: "Y"
      domain: [ 0, 1 ]
    }
    constraints {
      enforcement_literal: 0
      bool_and { literals: [ 1 ] }
    }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 2, -1 ]
        domain: [ 0, 10 ]
      }
    }
    solution_hint {
      vars: [ 0, 1 ]
      values: [ 0, 0 ]
    }
  )pb");
  VarDomination var_dom;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  context.UpdateNewConstraintsVariableUsage();
  context.LoadSolutionHint();
  ScanModelForDominanceDetection(context, &var_dom);
  EXPECT_TRUE(ExploitDominanceRelations(var_dom, &context));

  const IntegerVariable X = VarDomination::RefToIntegerVariable(0);
  const IntegerVariable Y = VarDomination::RefToIntegerVariable(1);
  EXPECT_THAT(var_dom.DominatingVariables(NegationOf(X)), ElementsAre(Y));
  EXPECT_EQ(context.DomainOf(0).ToString(), "[1]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[1]");
  EXPECT_EQ(context.SolutionHint(0), 1);
  EXPECT_EQ(context.SolutionHint(1), 1);
}

// X + 2Y >= 0
// ExactlyOne(X, Y)
//
// Doing (X--, Y++) is always beneficial if possible.
TEST(VarDominationTest, ExploitDominanceInExactlyOne) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables {
      name: "X"
      domain: [ 0, 1 ]
    }
    variables {
      name: "Y"
      domain: [ 0, 1 ]
    }
    constraints { exactly_one { literals: [ 0, 1 ] } }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 2 ]
        domain: [ 0, 10 ]
      }
    }
    solution_hint {
      vars: [ 0, 1 ]
      values: [ 1, 0 ]
    }
  )pb");
  VarDomination var_dom;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  context.UpdateNewConstraintsVariableUsage();
  context.LoadSolutionHint();
  ScanModelForDominanceDetection(context, &var_dom);
  EXPECT_TRUE(ExploitDominanceRelations(var_dom, &context));

  const IntegerVariable X = VarDomination::RefToIntegerVariable(0);
  const IntegerVariable Y = VarDomination::RefToIntegerVariable(1);
  EXPECT_THAT(var_dom.DominatingVariables(X), ElementsAre(Y));
  EXPECT_EQ(context.DomainOf(0).ToString(), "[0]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[0,1]");
  EXPECT_EQ(context.SolutionHint(0), 0);
  EXPECT_EQ(context.SolutionHint(1), 1);
}

// Objective: min(X + Y + 2Z)
// Constraint: X + Y + Z <= 10
// X, Y in [-10, 10], Z in [5, 10]
//
// Doing (X++, Z--) or (Y++, Z--) is always beneficial if possible.
TEST(VarDominationTest, ExploitDominanceWithIntegerVariables) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables {
      name: "X"
      domain: [ -10, 10 ]
    }
    variables {
      name: "Y"
      domain: [ -10, 10 ]
    }
    variables {
      name: "Z"
      domain: [ 5, 10 ]
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 1, 1 ]
        domain: [ 0, 10 ]
      }
    }
    objective {
      vars: [ 0, 1, 2 ]
      coeffs: [ 1, 1, 2 ]
    }
    solution_hint {
      vars: [ 0, 1, 2 ]
      values: [ 1, 1, 8 ]
    }
  )pb");
  VarDomination var_dom;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  context.UpdateNewConstraintsVariableUsage();
  context.LoadSolutionHint();
  ScanModelForDominanceDetection(context, &var_dom);
  EXPECT_TRUE(ExploitDominanceRelations(var_dom, &context));

  const IntegerVariable X = VarDomination::RefToIntegerVariable(0);
  const IntegerVariable Y = VarDomination::RefToIntegerVariable(1);
  const IntegerVariable Z = VarDomination::RefToIntegerVariable(2);
  EXPECT_THAT(var_dom.DominatingVariables(Z), ElementsAre(X, Y));
  EXPECT_EQ(context.DomainOf(0).ToString(), "[-5]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[0,10]");
  EXPECT_EQ(context.DomainOf(2).ToString(), "[5]");
  EXPECT_EQ(context.SolutionHint(0), -5);
  EXPECT_EQ(context.SolutionHint(1), 10);
  EXPECT_EQ(context.SolutionHint(2), 5);
}

// Objective: min(X + 2Y)
// Constraint: BoolOr(X, Y)
//
// Doing (X++, Y--) is always beneficial if possible.
TEST(VarDominationTest, ExploitRemainingDominance) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables {
      name: "X"
      domain: [ 0, 1 ]
    }
    variables {
      name: "Y"
      domain: [ 0, 1 ]
    }
    constraints { bool_or { literals: [ 0, 1 ] } }
    objective {
      vars: [ 0, 1 ]
      coeffs: [ 1, 2 ]
    }
    solution_hint {
      vars: [ 0, 1 ]
      values: [ 0, 1 ]
    }
  )pb");
  VarDomination var_dom;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  context.UpdateNewConstraintsVariableUsage();
  context.LoadSolutionHint();
  ScanModelForDominanceDetection(context, &var_dom);
  EXPECT_TRUE(ExploitDominanceRelations(var_dom, &context));

  // Check that an implication between X and Y was added, and that the hint was
  // updated in consequence.
  EXPECT_EQ(context.working_model->constraints_size(), 2);
  const ConstraintProto expected_constraint_proto =
      ParseTestProto(R"pb(enforcement_literal: -1
                          bool_and { literals: -2 })pb");
  EXPECT_THAT(context.working_model->constraints(1),
              EqualsProto(expected_constraint_proto));
  EXPECT_EQ(context.DomainOf(0).ToString(), "[0,1]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[0,1]");
  EXPECT_EQ(context.SolutionHint(0), 1);
  EXPECT_EQ(context.SolutionHint(1), 0);
}

// Objective: min(X)
// Constraint: -5 <= X + Y <= 5
// Constraint: -15 <= Y + Z <= 15
// X,Y in [-10, 10], Z in [-5, 5]
//
// Doing (X--, Y++) is always beneficial if possible.
TEST(VarDominationTest, ExploitRemainingDominanceWithIntegerVariables) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables {
      name: "X"
      domain: [ -10, 10 ]
    }
    variables {
      name: "Y"
      domain: [ -10, 10 ]
    }
    variables {
      name: "Z"
      domain: [ -5, 5 ]
    }
    objective {
      vars: [ 0 ]
      coeffs: [ 1 ]
    }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ -5, 5 ]
      }
    }
    constraints {
      linear {
        vars: [ 1, 2 ]
        coeffs: [ 1, 1 ]
        domain: [ -15, 15 ]
      }
    }
    solution_hint {
      vars: [ 0, 1, 2 ]
      values: [ 0, 1, 2 ]
    }
  )pb");
  VarDomination var_dom;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  context.UpdateNewConstraintsVariableUsage();
  context.LoadSolutionHint();
  ScanModelForDominanceDetection(context, &var_dom);
  EXPECT_TRUE(ExploitDominanceRelations(var_dom, &context));

  const IntegerVariable X = VarDomination::RefToIntegerVariable(0);
  const IntegerVariable Y = VarDomination::RefToIntegerVariable(1);
  EXPECT_THAT(var_dom.DominatingVariables(X), ElementsAre(Y));
  EXPECT_EQ(context.DomainOf(0).ToString(), "[-10,-5]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[5,10]");
  EXPECT_EQ(context.DomainOf(2).ToString(), "[5]");
  EXPECT_EQ(context.SolutionHint(0), -5);
  EXPECT_EQ(context.SolutionHint(1), 6);
  EXPECT_EQ(context.SolutionHint(2), 5);
}

// X + Y + Z = 0
// X + 2 Z >= 2
TEST(VarDominationTest, BasicExample1Variation) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables {
      name: "X"
      domain: [ -10, 10 ]
    }
    variables {
      name: "Y"
      domain: [ -10, 10 ]
    }
    variables {
      name: "Z"
      domain: [ -10, 10 ]
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 1, 1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 2 ]
        coeffs: [ 1, 2 ]
        domain: [ 2, 9223372036854775807 ]
      }
    }
  )pb");
  VarDomination var_dom;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  ScanModelForDominanceDetection(context, &var_dom);

  const IntegerVariable X = VarDomination::RefToIntegerVariable(0);
  const IntegerVariable Y = VarDomination::RefToIntegerVariable(1);
  const IntegerVariable Z = VarDomination::RefToIntegerVariable(2);
  EXPECT_THAT(var_dom.DominatingVariables(X), ElementsAre(Z));
  EXPECT_THAT(var_dom.DominatingVariables(Y), UnorderedElementsAre(X, Z));
  EXPECT_THAT(var_dom.DominatingVariables(Z), IsEmpty());

  // TODO(user): Transpose is broken.
  EXPECT_THAT(var_dom.DominatingVariables(NegationOf(X)),
              ElementsAre(NegationOf(Y)));
  EXPECT_THAT(var_dom.DominatingVariables(NegationOf(Y)), IsEmpty());
  EXPECT_THAT(var_dom.DominatingVariables(NegationOf(Z)),
              UnorderedElementsAre(NegationOf(X), NegationOf(Y)));
}

// X + Y + Z >= 0
// Y + Z <= 0
TEST(VarDominationTest, BasicExample2) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables {
      name: "X"
      domain: [ -10, 10 ]
    }
    variables {
      name: "Y"
      domain: [ -10, 10 ]
    }
    variables {
      name: "Z"
      domain: [ -10, 10 ]
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 1, 1 ]
        domain: [ 0, 9223372036854775807 ]
      }
    }
    constraints {
      linear {
        vars: [ 1, 2 ]
        coeffs: [ 1, 1 ]
        domain: [ -9223372036854775808, 0 ]
      }
    }
  )pb");
  VarDomination var_dom;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  ScanModelForDominanceDetection(context, &var_dom);

  const IntegerVariable X = VarDomination::RefToIntegerVariable(0);
  const IntegerVariable Y = VarDomination::RefToIntegerVariable(1);
  const IntegerVariable Z = VarDomination::RefToIntegerVariable(2);

  EXPECT_FALSE(var_dom.CanFreelyDecrease(X));
  EXPECT_THAT(var_dom.DominatingVariables(X), IsEmpty());
  EXPECT_TRUE(var_dom.CanFreelyDecrease(NegationOf(X)));

  // We do not include X in these lists, because X++ can always happen.
  EXPECT_THAT(var_dom.DominatingVariables(Y), UnorderedElementsAre(Z));
  EXPECT_THAT(var_dom.DominatingVariables(Z), UnorderedElementsAre(Y));
  EXPECT_THAT(var_dom.DominatingVariables(NegationOf(Y)),
              ElementsAre(NegationOf(Z)));
  EXPECT_THAT(var_dom.DominatingVariables(NegationOf(Z)),
              ElementsAre(NegationOf(Y)));
}

// X + Y <= 0
// Y + Z <= 0
TEST(VarDominationTest, BasicExample3) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables {
      name: "X"
      domain: [ -10, 10 ]
    }
    variables {
      name: "Y"
      domain: [ -10, 10 ]
    }
    variables {
      name: "Z"
      domain: [ -10, 10 ]
    }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ -9223372036854775808, 0 ]
      }
    }
    constraints {
      linear {
        vars: [ 1, 2 ]
        coeffs: [ 1, 1 ]
        domain: [ -9223372036854775808, 0 ]
      }
    }
  )pb");
  VarDomination var_dom;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  ScanModelForDominanceDetection(context, &var_dom);

  const IntegerVariable X = VarDomination::RefToIntegerVariable(0);
  const IntegerVariable Y = VarDomination::RefToIntegerVariable(1);
  const IntegerVariable Z = VarDomination::RefToIntegerVariable(2);

  EXPECT_TRUE(var_dom.CanFreelyDecrease(X));
  EXPECT_TRUE(var_dom.CanFreelyDecrease(Y));
  EXPECT_TRUE(var_dom.CanFreelyDecrease(Z));
  EXPECT_FALSE(var_dom.CanFreelyDecrease(NegationOf(X)));
  EXPECT_FALSE(var_dom.CanFreelyDecrease(NegationOf(Y)));
  EXPECT_FALSE(var_dom.CanFreelyDecrease(NegationOf(Z)));

  // No domination here, because all the dominator can just freely move in
  // one direction.
  EXPECT_THAT(var_dom.DominatingVariables(NegationOf(X)), IsEmpty());
  EXPECT_THAT(var_dom.DominatingVariables(NegationOf(Y)), IsEmpty());
  EXPECT_THAT(var_dom.DominatingVariables(NegationOf(Z)), IsEmpty());
}

// X + Y >= 0
// Y + Z >= 0
TEST(VarDominationTest, BasicExample4) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables {
      name: "X"
      domain: [ -10, 10 ]
    }
    variables {
      name: "Y"
      domain: [ -10, 10 ]
    }
    variables {
      name: "Z"
      domain: [ -10, 10 ]
    }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 9223372036854775807 ]
      }
    }
    constraints {
      linear {
        vars: [ 1, 2 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 9223372036854775807 ]
      }
    }
  )pb");
  VarDomination var_dom;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  ScanModelForDominanceDetection(context, &var_dom);

  const IntegerVariable X = VarDomination::RefToIntegerVariable(0);
  const IntegerVariable Y = VarDomination::RefToIntegerVariable(1);
  const IntegerVariable Z = VarDomination::RefToIntegerVariable(2);

  EXPECT_FALSE(var_dom.CanFreelyDecrease(X));
  EXPECT_FALSE(var_dom.CanFreelyDecrease(Y));
  EXPECT_FALSE(var_dom.CanFreelyDecrease(Z));
  EXPECT_TRUE(var_dom.CanFreelyDecrease(NegationOf(X)));
  EXPECT_TRUE(var_dom.CanFreelyDecrease(NegationOf(Y)));
  EXPECT_TRUE(var_dom.CanFreelyDecrease(NegationOf(Z)));

  EXPECT_THAT(var_dom.DominatingVariables(X), IsEmpty());
  EXPECT_THAT(var_dom.DominatingVariables(Y), IsEmpty());
  EXPECT_THAT(var_dom.DominatingVariables(Z), IsEmpty());
}

// X + Y + Z = 0
TEST(VarDominationTest, AllEquivalent) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables {
      name: "X"
      domain: [ -10, 10 ]
    }
    variables {
      name: "Y"
      domain: [ -10, 10 ]
    }
    variables {
      name: "Z"
      domain: [ -10, 10 ]
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 1, 1 ]
        domain: [ 0, 0 ]
      }
    }
  )pb");
  VarDomination var_dom;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  ScanModelForDominanceDetection(context, &var_dom);

  // Domination is slightly related to symmetry and duplicate columns.
  const IntegerVariable X = VarDomination::RefToIntegerVariable(0);
  const IntegerVariable Y = VarDomination::RefToIntegerVariable(1);
  const IntegerVariable Z = VarDomination::RefToIntegerVariable(2);
  EXPECT_THAT(var_dom.DominatingVariables(X), UnorderedElementsAre(Y, Z));
  EXPECT_THAT(var_dom.DominatingVariables(Y), UnorderedElementsAre(X, Z));
  EXPECT_THAT(var_dom.DominatingVariables(Z), UnorderedElementsAre(X, Y));
}

// X + Y + Z <= 0  (to prevent freely moving variables).
// -X + -2Y + -3Z <= 0
TEST(VarDominationTest, NegativeCoefficients) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables {
      name: "X"
      domain: [ -10, 10 ]
    }
    variables {
      name: "Y"
      domain: [ -10, 10 ]
    }
    variables {
      name: "Z"
      domain: [ -10, 10 ]
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 1, 1 ]
        domain: [ -9223372036854775808, 0 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ -1, -2, -3 ]
        domain: [ -9223372036854775808, 0 ]
      }
    }
  )pb");
  VarDomination var_dom;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  ScanModelForDominanceDetection(context, &var_dom);

  const IntegerVariable X = VarDomination::RefToIntegerVariable(0);
  const IntegerVariable Y = VarDomination::RefToIntegerVariable(1);
  const IntegerVariable Z = VarDomination::RefToIntegerVariable(2);
  EXPECT_THAT(var_dom.DominatingVariables(X), UnorderedElementsAre(Y, Z));
  EXPECT_THAT(var_dom.DominatingVariables(Y), UnorderedElementsAre(Z));
  EXPECT_THAT(var_dom.DominatingVariables(Z), IsEmpty());
}

TEST(DualBoundReductionTest, FixVariableToDomainBound) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -10, 10 ] }
    constraints {
      linear {
        vars: [ 0 ]
        coeffs: [ 1 ]
        domain: [ -15, 0 ]
      }
    }
    constraints {
      linear {
        vars: [ 1 ]
        coeffs: [ 1 ]
        domain: [ 0, 15 ]
      }
    }
    solution_hint {
      vars: [ 0, 1 ]
      values: [ -5, 5 ]
    }
  )pb");
  DualBoundStrengthening dual_bound_strengthening;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.LoadSolutionHint();
  context.ReadObjectiveFromProto();
  ScanModelForDualBoundStrengthening(context, &dual_bound_strengthening);

  EXPECT_TRUE(dual_bound_strengthening.Strengthen(&context));

  EXPECT_EQ(context.DomainOf(0).ToString(), "[-10]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[10]");
  EXPECT_EQ(context.SolutionHint(0), -10);
  EXPECT_EQ(context.SolutionHint(1), 10);
}

// Bound propagation see nothing, but if we can remove feasible solution, from
// this constraint point of view, all variables can freely increase or decrease
// until zero (because the constraint is trivial above/below).
//
// -20 <= X + Y + Z <= 20
TEST(DualBoundReductionTest, BasicTest) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -10, 10 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 1, 1 ]
        domain: [ -20, 20 ]
      }
    }
    solution_hint {
      vars: [ 0, 1, 2 ]
      values: [ 3, 4, 5 ]
    }
  )pb");
  DualBoundStrengthening dual_bound_strengthening;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.LoadSolutionHint();
  context.ReadObjectiveFromProto();
  ScanModelForDualBoundStrengthening(context, &dual_bound_strengthening);

  EXPECT_TRUE(dual_bound_strengthening.Strengthen(&context));

  EXPECT_EQ(context.DomainOf(0).ToString(), "[0]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[0]");
  EXPECT_EQ(context.DomainOf(2).ToString(), "[0]");
  EXPECT_EQ(context.SolutionHint(0), 0);
  EXPECT_EQ(context.SolutionHint(1), 0);
  EXPECT_EQ(context.SolutionHint(2), 0);
}

TEST(DualBoundReductionTest, CarefulWithHoles) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -10, 0, 7, 10 ] }
    variables { domain: [ -10, -6, 3, 10 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 1, 1 ]
        domain: [ -15, 15 ]
      }
    }
  )pb");
  DualBoundStrengthening dual_bound_strengthening;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  ScanModelForDualBoundStrengthening(context, &dual_bound_strengthening);

  EXPECT_TRUE(dual_bound_strengthening.Strengthen(&context));

  EXPECT_EQ(context.DomainOf(0).ToString(), "[-5,5]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[-5,0][7]");
  EXPECT_EQ(context.DomainOf(2).ToString(), "[-6][3,5]");
}

// Here the inferred bounds crosses, so we have multiple choices, we will fix
// to the lowest magnitude.
TEST(DualBoundReductionTest, Choices) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ -10, 10 ] }
    variables { domain: [ -10, -2, 3, 10 ] }
    variables { domain: [ -10, -3, 2, 10 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 1, 1 ]
        domain: [ -25, 25 ]
      }
    }
    solution_hint {
      vars: [ 0, 1, 2 ]
      values: [ 3, 4, 5 ]
    }
  )pb");
  DualBoundStrengthening dual_bound_strengthening;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.LoadSolutionHint();
  context.ReadObjectiveFromProto();
  ScanModelForDualBoundStrengthening(context, &dual_bound_strengthening);

  EXPECT_TRUE(dual_bound_strengthening.Strengthen(&context));

  EXPECT_EQ(context.DomainOf(0).ToString(), "[0]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[-2]");
  EXPECT_EQ(context.DomainOf(2).ToString(), "[2]");
  EXPECT_EQ(context.SolutionHint(0), 0);
  EXPECT_EQ(context.SolutionHint(1), -2);
  EXPECT_EQ(context.SolutionHint(2), 2);
}

TEST(DualBoundReductionTest, AddImplication) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      # a => (b == c)
      enforcement_literal: 0
      linear {
        vars: [ 1, 2 ]
        coeffs: [ -1, 1 ]
        domain: [ 0, 0 ]
      }
    }
    solution_hint {
      vars: [ 0, 1, 1 ]
      values: [ 0, 1, 1 ]
    }
  )pb");
  DualBoundStrengthening dual_bound_strengthening;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.LoadSolutionHint();
  context.ReadObjectiveFromProto();
  ScanModelForDualBoundStrengthening(context, &dual_bound_strengthening);

  EXPECT_TRUE(dual_bound_strengthening.Strengthen(&context));

  // not(a) => not(b) and not(a) => not(c) should be added.
  ASSERT_EQ(context.working_model->constraints_size(), 3);
  const ConstraintProto expected_constraint_proto1 =
      ParseTestProto(R"pb(enforcement_literal: -1
                          bool_and { literals: -2 })pb");
  EXPECT_THAT(context.working_model->constraints(1),
              EqualsProto(expected_constraint_proto1));
  const ConstraintProto expected_constraint_proto2 =
      ParseTestProto(R"pb(enforcement_literal: -1
                          bool_and { literals: -3 })pb");
  EXPECT_THAT(context.working_model->constraints(2),
              EqualsProto(expected_constraint_proto2));
  EXPECT_EQ(context.DomainOf(0).ToString(), "[0]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[0,1]");
  EXPECT_EQ(context.DomainOf(2).ToString(), "[0,1]");
  EXPECT_EQ(context.SolutionHint(0), 0);
  EXPECT_EQ(context.SolutionHint(1), 0);
  EXPECT_EQ(context.SolutionHint(2), 0);
}

TEST(DualBoundReductionTest, EquivalenceDetection) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      # a => b
      enforcement_literal: 0
      bool_and { literals: [ 1 ] }
    }
    constraints {
      linear {
        # b == c (we just want b and c not to freely vary)
        vars: [ 1, 2 ]
        coeffs: [ 1, -1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      bool_or {
        literals: [ 0, 2 ]  # a + c >= 0
      }
    }
    solution_hint {
      vars: [ 0, 1, 2 ]
      values: [ 0, 1, 1 ]
    }
  )pb");
  DualBoundStrengthening dual_bound_strengthening;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.LoadSolutionHint();
  context.ReadObjectiveFromProto();
  ScanModelForDualBoundStrengthening(context, &dual_bound_strengthening);

  EXPECT_TRUE(dual_bound_strengthening.Strengthen(&context));

  EXPECT_EQ(context.DomainOf(0).ToString(), "[0,1]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[0,1]");
  EXPECT_EQ(context.DomainOf(2).ToString(), "[0,1]");
  // Equivalence between a and b.
  EXPECT_EQ(context.GetLiteralRepresentative(1), 0);
  EXPECT_TRUE(context.LiteralSolutionHint(0));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
