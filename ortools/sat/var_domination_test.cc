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

#include "ortools/sat/var_domination.h"

#include <cstdint>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
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
  ScanModelForDominanceDetection(context, &var_dom);
  EXPECT_TRUE(ExploitDominanceRelations(var_dom, &context));

  // Because X--, Z++ is always ok, we can exclude some value from Z using
  // equation X + 2Z >= 2 we see that if Z=5, X >= -8, so we can decrease it,
  // but for Z = 6, X might be -10, so we are not sure.
  //
  // Also note that X can be 10 with Z at 10 too, so we cannot reduce the domain
  // of X.
  EXPECT_EQ(context.DomainOf(0).ToString(), "[-10,10]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[-10,10]");
  EXPECT_EQ(context.DomainOf(2).ToString(), "[6,10]");
}

// Same example as before but now Z has holes, which complicates the
// final result a bit.
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
  ScanModelForDominanceDetection(context, &var_dom);
  EXPECT_TRUE(ExploitDominanceRelations(var_dom, &context));

  // With holes, if Z is 0, we will not be able to increase it up to 6, so we
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
  context.LoadAndClampSolutionHint();
  ScanModelForDominanceDetection(context, &var_dom);
  EXPECT_TRUE(ExploitDominanceRelations(var_dom, &context));

  const IntegerVariable X = VarDomination::RefToIntegerVariable(0);
  const IntegerVariable Y = VarDomination::RefToIntegerVariable(1);
  EXPECT_THAT(var_dom.DominatingVariables(X), ElementsAre(NegationOf(Y)));
  EXPECT_EQ(context.DomainOf(0).ToString(), "[0]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[0]");
  EXPECT_EQ(context.solution_crush().GetVarValues()[0], 0);
  EXPECT_EQ(context.solution_crush().GetVarValues()[1], 0);
}

// 2X - Y >= 0
// X => Y
// min -X
//
// Doing (X++, Y++) is always beneficial if possible (without the objective,
// clamping 2X - Y >= 0 to lb_slack = 1 on Booleans also admits (X--, Y--)).
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
    objective {
      vars: [ 0 ]
      coeffs: [ -1 ]
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
  context.LoadAndClampSolutionHint();
  ScanModelForDominanceDetection(context, &var_dom);
  EXPECT_TRUE(ExploitDominanceRelations(var_dom, &context));

  const IntegerVariable X = VarDomination::RefToIntegerVariable(0);
  const IntegerVariable Y = VarDomination::RefToIntegerVariable(1);
  EXPECT_THAT(var_dom.DominatingVariables(NegationOf(X)), ElementsAre(Y));
  EXPECT_EQ(context.DomainOf(0).ToString(), "[1]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[1]");
  EXPECT_EQ(context.solution_crush().GetVarValues()[0], 1);
  EXPECT_EQ(context.solution_crush().GetVarValues()[1], 1);
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
  context.LoadAndClampSolutionHint();
  ScanModelForDominanceDetection(context, &var_dom);
  EXPECT_TRUE(ExploitDominanceRelations(var_dom, &context));

  const IntegerVariable X = VarDomination::RefToIntegerVariable(0);
  const IntegerVariable Y = VarDomination::RefToIntegerVariable(1);
  EXPECT_THAT(var_dom.DominatingVariables(X), ElementsAre(Y));
  EXPECT_EQ(context.DomainOf(0).ToString(), "[0]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[0,1]");
  EXPECT_EQ(context.solution_crush().GetVarValues()[0], 0);
  EXPECT_EQ(context.solution_crush().GetVarValues()[1], 1);
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
  context.LoadAndClampSolutionHint();
  ScanModelForDominanceDetection(context, &var_dom);
  EXPECT_TRUE(ExploitDominanceRelations(var_dom, &context));

  const IntegerVariable X = VarDomination::RefToIntegerVariable(0);
  const IntegerVariable Y = VarDomination::RefToIntegerVariable(1);
  const IntegerVariable Z = VarDomination::RefToIntegerVariable(2);
  EXPECT_THAT(var_dom.DominatingVariables(Z), ElementsAre(X, Y));
  EXPECT_EQ(context.DomainOf(0).ToString(), "[-5]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[0,10]");
  EXPECT_EQ(context.DomainOf(2).ToString(), "[5]");
  EXPECT_EQ(context.solution_crush().GetVarValues()[0], -5);
  EXPECT_EQ(context.solution_crush().GetVarValues()[1], 10);
  EXPECT_EQ(context.solution_crush().GetVarValues()[2], 5);
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
  context.LoadAndClampSolutionHint();
  ScanModelForDominanceDetection(context, &var_dom);
  EXPECT_TRUE(ExploitDominanceRelations(var_dom, &context));

  // Check that an implication between X and Y was added, and that the hint was
  // updated in consequence.
  EXPECT_EQ(context.NumConstraints(), 2);
  const ConstraintProto expected_constraint_proto =
      ParseTestProto(R"pb(enforcement_literal: -1
                          bool_and { literals: -2 })pb");
  EXPECT_THAT(context.Constraint(1), EqualsProto(expected_constraint_proto));
  EXPECT_EQ(context.DomainOf(0).ToString(), "[0,1]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[0,1]");
  EXPECT_EQ(context.solution_crush().GetVarValues()[0], 1);
  EXPECT_EQ(context.solution_crush().GetVarValues()[1], 0);
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
  context.LoadAndClampSolutionHint();
  ScanModelForDominanceDetection(context, &var_dom);
  EXPECT_TRUE(ExploitDominanceRelations(var_dom, &context));

  const IntegerVariable X = VarDomination::RefToIntegerVariable(0);
  const IntegerVariable Y = VarDomination::RefToIntegerVariable(1);
  EXPECT_THAT(var_dom.DominatingVariables(X), ElementsAre(Y));
  EXPECT_EQ(context.DomainOf(0).ToString(), "[-10,-5]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[5,10]");
  EXPECT_EQ(context.DomainOf(2).ToString(), "[5]");
  EXPECT_EQ(context.solution_crush().GetVarValues()[0], -5);
  EXPECT_EQ(context.solution_crush().GetVarValues()[1], 6);
  EXPECT_EQ(context.solution_crush().GetVarValues()[2], 5);
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
  context.LoadAndClampSolutionHint();
  context.ReadObjectiveFromProto();
  ScanModelForDualBoundStrengthening(context, &dual_bound_strengthening);

  EXPECT_TRUE(dual_bound_strengthening.Strengthen(&context));

  EXPECT_EQ(context.DomainOf(0).ToString(), "[-10]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[10]");
  EXPECT_EQ(context.solution_crush().GetVarValues()[0], -10);
  EXPECT_EQ(context.solution_crush().GetVarValues()[1], 10);
}

// Bound propagation sees nothing, but if we can remove feasible solutions,
// from this constraint's point of view, all variables can freely increase or
// decrease until zero (because the constraint is trivial above/below).
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
  context.LoadAndClampSolutionHint();
  context.ReadObjectiveFromProto();
  ScanModelForDualBoundStrengthening(context, &dual_bound_strengthening);

  EXPECT_TRUE(dual_bound_strengthening.Strengthen(&context));

  EXPECT_EQ(context.DomainOf(0).ToString(), "[0]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[0]");
  EXPECT_EQ(context.DomainOf(2).ToString(), "[0]");
  EXPECT_EQ(context.solution_crush().GetVarValues()[0], 0);
  EXPECT_EQ(context.solution_crush().GetVarValues()[1], 0);
  EXPECT_EQ(context.solution_crush().GetVarValues()[2], 0);
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

// Here the inferred bounds cross, so we have multiple choices, we will fix
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
  context.LoadAndClampSolutionHint();
  context.ReadObjectiveFromProto();
  ScanModelForDualBoundStrengthening(context, &dual_bound_strengthening);

  EXPECT_TRUE(dual_bound_strengthening.Strengthen(&context));

  EXPECT_EQ(context.DomainOf(0).ToString(), "[0]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[-2]");
  EXPECT_EQ(context.DomainOf(2).ToString(), "[2]");
  EXPECT_EQ(context.solution_crush().GetVarValues()[0], 0);
  EXPECT_EQ(context.solution_crush().GetVarValues()[1], -2);
  EXPECT_EQ(context.solution_crush().GetVarValues()[2], 2);
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
  context.LoadAndClampSolutionHint();
  context.ReadObjectiveFromProto();
  ScanModelForDualBoundStrengthening(context, &dual_bound_strengthening);

  EXPECT_TRUE(dual_bound_strengthening.Strengthen(&context));

  // not(a) => not(b) and not(a) => not(c) should be added.
  ASSERT_EQ(context.NumConstraints(), 3);
  const ConstraintProto expected_constraint_proto1 =
      ParseTestProto(R"pb(enforcement_literal: -1
                          bool_and { literals: -2 })pb");
  EXPECT_THAT(context.Constraint(1), EqualsProto(expected_constraint_proto1));
  const ConstraintProto expected_constraint_proto2 =
      ParseTestProto(R"pb(enforcement_literal: -1
                          bool_and { literals: -3 })pb");
  EXPECT_THAT(context.Constraint(2), EqualsProto(expected_constraint_proto2));
  EXPECT_EQ(context.DomainOf(0).ToString(), "[0]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[0,1]");
  EXPECT_EQ(context.DomainOf(2).ToString(), "[0,1]");
  EXPECT_EQ(context.solution_crush().GetVarValues()[0], 0);
  EXPECT_EQ(context.solution_crush().GetVarValues()[1], 0);
  EXPECT_EQ(context.solution_crush().GetVarValues()[2], 0);
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
  context.LoadAndClampSolutionHint();
  context.ReadObjectiveFromProto();
  ScanModelForDualBoundStrengthening(context, &dual_bound_strengthening);

  EXPECT_TRUE(dual_bound_strengthening.Strengthen(&context));

  EXPECT_EQ(context.DomainOf(0).ToString(), "[0,1]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[0,1]");
  EXPECT_EQ(context.DomainOf(2).ToString(), "[0,1]");
  // Equivalence between a and b.
  EXPECT_EQ(context.GetLiteralRepresentative(1), 0);
  EXPECT_EQ(context.solution_crush().GetVarValues()[0], 1);
}

// Regression test for duplicate entries in `DominatingVariables(ref)`:
// When a constraint such as `bool_or` contains duplicate literals
// (`literals: [0, 1, 1]`), `InitialDominatingCandidates(X)` contains duplicate
// entries `[X, Y, Y]`. `VarDomination::EndFirstPhase()` deduplicates
// `dominating_vars_[var]` for all variables (not just cropped lists), ensuring
// `var_dom.DominatingVariables(X) == [Y]` so `get_delta` in
// `ExploitDominanceRelations` does not double-count `Y`'s slack.
TEST(
    VarDominationTest,
    Counterexample_DuplicateDominatingVarsDoubleCountsDeltaAndMakesModelUnsat) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables {
      name: "X"
      domain: [ 0, 1 ]
    }
    variables {
      name: "Y"
      domain: [ 0, 1 ]
    }
    variables {
      name: "Z"
      domain: [ 0, 1 ]
    }
    constraints { bool_or { literals: [ 0, 1, 1 ] } }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 1, 2 ]
        domain: [ 0, 2 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 1, 1, 1 ]
        domain: [ 2, 3 ]
      }
    }
    solution_hint {
      vars: [ 0, 1, 2 ]
      values: [ 1, 1, 0 ]
    }
  )pb");

  // 1. Model is valid according to ValidateCpModel (and has no duplicate vars
  //    in any linear constraint).
  ASSERT_EQ(ValidateCpModel(model_proto), "");

  // 2. Before presolve, (X = 1, Y = 1, Z = 0) is the unique feasible solution
  //    of model_proto.
  const std::vector<int64_t> initial_assignment = {1, 1, 0};
  ASSERT_TRUE(SolutionIsFeasible(model_proto, initial_assignment));

  // 3. Run ScanModelForDominanceDetection: EndFirstPhase deduplicates
  //    DominatingVariables(X) to {Y}.
  VarDomination var_dom;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  context.LoadAndClampSolutionHint();
  ScanModelForDominanceDetection(context, &var_dom);

  const IntegerVariable X = VarDomination::RefToIntegerVariable(0);
  const IntegerVariable Y = VarDomination::RefToIntegerVariable(1);
  EXPECT_THAT(var_dom.DominatingVariables(X), ElementsAre(Y));

  // 4. ExploitDominanceRelations preserves satisfiability and keeps the unique
  //    feasible solution (1, 1, 0).
  EXPECT_TRUE(ExploitDominanceRelations(var_dom, &context));
  EXPECT_FALSE(context.ModelIsUnsat());
  context.WriteVariableDomainsToProto();

  EXPECT_THAT(context.solution_crush().GetVarValues(), ElementsAre(1, 1, 0));
  EXPECT_TRUE(
      SolutionIsFeasible(model_proto, context.solution_crush().GetVarValues()));
}

TEST(DualBoundReductionTest, MultipleEquivalenceDetection) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      # a1 => b
      enforcement_literal: 0
      bool_and { literals: [ 2 ] }
    }
    constraints {
      # a2 => b
      enforcement_literal: 1
      bool_and { literals: [ 2 ] }
    }
    constraints {
      linear {
        # b == c (we just want b and c not to freely vary)
        vars: [ 2, 3 ]
        coeffs: [ 1, -1 ]
        domain: [ 0, 0 ]
      }
    }
    constraints {
      bool_or {
        literals: [ 0, 3 ]  # a1 + c >= 1
      }
    }
    constraints {
      bool_or {
        literals: [ 1, 3 ]  # a2 + c >= 1
      }
    }
    solution_hint {
      vars: [ 0, 1, 2, 3 ]
      values: [ 0, 0, 1, 1 ]
    }
  )pb");
  DualBoundStrengthening dual_bound_strengthening;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.LoadAndClampSolutionHint();
  context.ReadObjectiveFromProto();
  ScanModelForDualBoundStrengthening(context, &dual_bound_strengthening);

  EXPECT_TRUE(dual_bound_strengthening.Strengthen(&context));

  // Both a1 and a2 should be made equivalent to b in a single pass.
  EXPECT_EQ(context.GetLiteralRepresentative(1),
            context.GetLiteralRepresentative(0));
  EXPECT_EQ(context.GetLiteralRepresentative(2),
            context.GetLiteralRepresentative(0));
  EXPECT_EQ(context.solution_crush().GetVarValues()[0], 1);
  EXPECT_EQ(context.solution_crush().GetVarValues()[1], 1);
  EXPECT_EQ(context.solution_crush().GetVarValues()[2], 1);
}

// On the boxed constraint 2 <= 2X + 4Y + 2Z + 2W <= 6 over Booleans:
// - Lower-bound slack is 2, so clamping coefficients to 2 shows (X++, Y--)
//   never violates >= 2.
// - Upper-bound slack is 4, and 2 <= 4 shows (X++, Y--) never violates <= 6.
// Together with AtMostOne(X, Y) and objective X + 2Y + 3Z + 4W, X uniquely
// dominates Y and fixes Y = 0.
TEST(VarDominationTest, DominanceAcrossBoxedLinearConstraint) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables {
      name: "X"
      domain: [ 0, 1 ]
    }
    variables {
      name: "Y"
      domain: [ 0, 1 ]
    }
    variables {
      name: "Z"
      domain: [ 0, 1 ]
    }
    variables {
      name: "W"
      domain: [ 0, 1 ]
    }
    constraints { at_most_one { literals: [ 0, 1 ] } }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3 ]
        coeffs: [ 2, 4, 2, 2 ]
        domain: [ 2, 6 ]
      }
    }
    objective {
      vars: [ 0, 1, 2, 3 ]
      coeffs: [ 1, 2, 3, 4 ]
    }
    solution_hint {
      vars: [ 0, 1, 2, 3 ]
      values: [ 0, 1, 1, 0 ]
    }
  )pb");
  ASSERT_TRUE(SolutionIsFeasible(model_proto, {0, 1, 1, 0}));

  VarDomination var_dom;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  context.LoadAndClampSolutionHint();
  ScanModelForDominanceDetection(context, &var_dom);

  const IntegerVariable X = VarDomination::RefToIntegerVariable(0);
  const IntegerVariable Y = VarDomination::RefToIntegerVariable(1);
  EXPECT_THAT(var_dom.DominatingVariables(Y), ElementsAre(X));

  EXPECT_TRUE(ExploitDominanceRelations(var_dom, &context));
  EXPECT_EQ(context.DomainOf(1).ToString(), "[0]");
  EXPECT_THAT(context.solution_crush().GetVarValues(), ElementsAre(1, 0, 1, 0));
  EXPECT_TRUE(
      SolutionIsFeasible(model_proto, context.solution_crush().GetVarValues()));
}

// On -2 <= 3X + 2Y - 2Z <= 8 with X, Y, Z in [0, 2]:
// - min_activity = -4 (lb_slack = 2) and max_activity = 10 (ub_slack = 2).
// - Clamping coefficients on the upper bound to ub_slack = 2 shows (X++, Y--)
//   never violates <= 8 (since Y-- forces Y' <= 1, keeping 3X' + 2Y' - 2Z <= 8)
//   even though 3 > 2 increases the linear expression.
TEST(VarDominationTest, DominanceAcrossBoxedLinearConstraintUpperSlackClamp) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables {
      name: "X"
      domain: [ 0, 2 ]
    }
    variables {
      name: "Y"
      domain: [ 0, 2 ]
    }
    variables {
      name: "Z"
      domain: [ 0, 2 ]
    }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ 0, 2 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 3, 2, -2 ]
        domain: [ -2, 8 ]
      }
    }
    objective {
      vars: [ 0, 1 ]
      coeffs: [ 1, 2 ]
    }
    solution_hint {
      vars: [ 0, 1, 2 ]
      values: [ 0, 2, 0 ]
    }
  )pb");
  ASSERT_TRUE(SolutionIsFeasible(model_proto, {0, 2, 0}));

  VarDomination var_dom;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  context.LoadAndClampSolutionHint();
  ScanModelForDominanceDetection(context, &var_dom);

  const IntegerVariable X = VarDomination::RefToIntegerVariable(0);
  const IntegerVariable Y = VarDomination::RefToIntegerVariable(1);
  const IntegerVariable Z = VarDomination::RefToIntegerVariable(2);
  EXPECT_THAT(var_dom.DominatingVariables(Y), ElementsAre(X, NegationOf(Z)));

  EXPECT_TRUE(ExploitDominanceRelations(var_dom, &context));
  EXPECT_EQ(context.DomainOf(1).ToString(), "[0]");
  EXPECT_THAT(context.solution_crush().GetVarValues(), ElementsAre(2, 0, 0));
  EXPECT_TRUE(
      SolutionIsFeasible(model_proto, context.solution_crush().GetVarValues()));
}

CpModelProto MakeUnclampedBoxedPartitionModel(int num_z_vars) {
  CpModelProto model_proto;
  for (int i = 0; i < num_z_vars + 2; ++i) {
    IntegerVariableProto* var = model_proto.add_variables();
    var->add_domain(0);
    var->add_domain(1);
  }
  const int y0 = num_z_vars;
  const int y1 = num_z_vars + 1;

  // bool_or over all variables: without partition refinement on the unclamped
  // boxed constraint below, y1's initial candidate list starts with z_0..z_54
  // and crops out y0 at kMaxInitialSize = 50.
  BoolArgumentProto* bool_or = model_proto.add_constraints()->mutable_bool_or();
  for (int i = 0; i < num_z_vars + 2; ++i) {
    bool_or->add_literals(i);
  }

  BoolArgumentProto* amo = model_proto.add_constraints()->mutable_at_most_one();
  amo->add_literals(y0);
  amo->add_literals(y1);

  // 4 <= 2 * sum(z_i) + y0 + y1 <= 100:
  // lb_slack = 4, ub_slack = 2 * num_z_vars + 2 - 100 = 12, and
  // max_coeff_magnitude = 2 <= min(4, 12), so neither side clamps any
  // coefficient and ActivityShouldNotChange refines {y0, y1} into its own part.
  LinearConstraintProto* linear =
      model_proto.add_constraints()->mutable_linear();
  for (int i = 0; i < num_z_vars; ++i) {
    linear->add_vars(i);
    linear->add_coeffs(2);
  }
  linear->add_vars(y0);
  linear->add_coeffs(1);
  linear->add_vars(y1);
  linear->add_coeffs(1);
  linear->add_domain(4);
  linear->add_domain(100);

  CpObjectiveProto* obj = model_proto.mutable_objective();
  for (int i = 0; i < num_z_vars; ++i) {
    obj->add_vars(i);
    obj->add_coeffs(1);
  }
  obj->add_vars(y0);
  obj->add_coeffs(1);
  obj->add_vars(y1);
  obj->add_coeffs(2);

  PartialVariableAssignment* hint = model_proto.mutable_solution_hint();
  for (int i = 0; i < num_z_vars + 2; ++i) {
    hint->add_vars(i);
    hint->add_values((i == 0 || i == 1 || i == y1) ? 1 : 0);
  }
  return model_proto;
}

TEST(VarDominationTest, UnclampedBoxedLinearConstraintRefinesPartition) {
  constexpr int kNumZVars = 55;
  constexpr int kY0 = kNumZVars;
  constexpr int kY1 = kNumZVars + 1;
  CpModelProto model_proto = MakeUnclampedBoxedPartitionModel(kNumZVars);
  ASSERT_EQ(ValidateCpModel(model_proto), "");

  VarDomination var_dom;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  context.LoadAndClampSolutionHint();
  ASSERT_TRUE(
      SolutionIsFeasible(model_proto, context.solution_crush().GetVarValues()));

  ScanModelForDominanceDetection(context, &var_dom);

  EXPECT_THAT(
      var_dom.DominatingVariables(VarDomination::RefToIntegerVariable(kY1)),
      ElementsAre(VarDomination::RefToIntegerVariable(kY0)));

  EXPECT_TRUE(ExploitDominanceRelations(var_dom, &context));
  EXPECT_EQ(context.DomainOf(kY1).ToString(), "[0]");
  EXPECT_EQ(context.solution_crush().GetVarValues()[kY0], 1);
  EXPECT_EQ(context.solution_crush().GetVarValues()[kY1], 0);
  EXPECT_TRUE(
      SolutionIsFeasible(model_proto, context.solution_crush().GetVarValues()));
}

// On the unpropagated equality 3X + Y + Z == 1 over Booleans:
// - lb_slack = 1 and ub_slack = 4 (max_coeff_magnitude = 3 > min(1, 4)).
// - Clamping >= 1 to [1, 1, 1] while keeping <= 1 at [3, 1, 1] detects that Y
//   dominates X, fixing X = 0 via AtMostOne(X, Y).
TEST(VarDominationTest, DominanceAcrossUnpropagatedEqualityConstraint) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables {
      name: "X"
      domain: [ 0, 1 ]
    }
    variables {
      name: "Y"
      domain: [ 0, 1 ]
    }
    variables {
      name: "Z"
      domain: [ 0, 1 ]
    }
    constraints { at_most_one { literals: [ 0, 1 ] } }
    constraints {
      linear {
        vars: [ 0, 1, 2 ]
        coeffs: [ 3, 1, 1 ]
        domain: [ 1, 1 ]
      }
    }
    objective {
      vars: [ 0, 1, 2 ]
      coeffs: [ 2, 1, 3 ]
    }
    solution_hint {
      vars: [ 0, 1, 2 ]
      values: [ 0, 0, 1 ]
    }
  )pb");
  ASSERT_EQ(ValidateCpModel(model_proto), "");
  ASSERT_TRUE(SolutionIsFeasible(model_proto, {0, 0, 1}));

  VarDomination var_dom;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  context.LoadAndClampSolutionHint();
  ScanModelForDominanceDetection(context, &var_dom);

  const IntegerVariable X = VarDomination::RefToIntegerVariable(0);
  const IntegerVariable Y = VarDomination::RefToIntegerVariable(1);
  EXPECT_THAT(var_dom.DominatingVariables(X), ElementsAre(Y));

  EXPECT_TRUE(ExploitDominanceRelations(var_dom, &context));
  EXPECT_EQ(context.DomainOf(0).ToString(), "[0]");
  EXPECT_THAT(context.solution_crush().GetVarValues(), ElementsAre(0, 0, 1));
  EXPECT_TRUE(
      SolutionIsFeasible(model_proto, context.solution_crush().GetVarValues()));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
