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

#include <string>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/presolve_context.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::ElementsAre;
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
  )pb");
  DualBoundStrengthening dual_bound_strengthening;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  ScanModelForDualBoundStrengthening(context, &dual_bound_strengthening);
  EXPECT_TRUE(dual_bound_strengthening.Strengthen(&context));
  EXPECT_EQ(context.DomainOf(0).ToString(), "[0]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[0]");
  EXPECT_EQ(context.DomainOf(2).ToString(), "[0]");
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

// Here the infered bounds crosses, so we have multiple choices, we will fix
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
  )pb");
  DualBoundStrengthening dual_bound_strengthening;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  ScanModelForDualBoundStrengthening(context, &dual_bound_strengthening);
  EXPECT_TRUE(dual_bound_strengthening.Strengthen(&context));
  EXPECT_EQ(context.DomainOf(0).ToString(), "[0]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[-2]");
  EXPECT_EQ(context.DomainOf(2).ToString(), "[2]");
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
  )pb");
  DualBoundStrengthening dual_bound_strengthening;
  Model model;
  PresolveContext context(&model, &model_proto, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  ScanModelForDualBoundStrengthening(context, &dual_bound_strengthening);
  EXPECT_TRUE(dual_bound_strengthening.Strengthen(&context));
  EXPECT_EQ(context.DomainOf(0).ToString(), "[0,1]");
  EXPECT_EQ(context.DomainOf(1).ToString(), "[0,1]");
  EXPECT_EQ(context.DomainOf(2).ToString(), "[0,1]");

  // Equivalence between a and b.
  EXPECT_EQ(context.GetLiteralRepresentative(1), 0);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
