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

#include "ortools/sat/presolve_context.h"

#include <cstdint>

#include "absl/container/flat_hash_set.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"
#include "ortools/util/affine_relation.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;

TEST(PresolveContextTest, GetOrCreateEncodingOnIntVar) {
  Model model;
  CpModelProto working_model;
  PresolveContext context(&model, &working_model, nullptr);
  context.NewIntVar({1, 5});

  EXPECT_EQ(1, context.GetOrCreateVarValueEncoding(0, 2));
  EXPECT_EQ(2, context.GetOrCreateVarValueEncoding(0, 4));
  EXPECT_EQ(1, context.GetOrCreateVarValueEncoding(0, 2));
  EXPECT_EQ(1, context.GetOrCreateVarValueEncoding(-1, -2));
}

TEST(PresolveContextTest, GetOrCreateEncodingOnBoolVar) {
  Model model;
  CpModelProto working_model;
  PresolveContext context(&model, &working_model, nullptr);
  context.NewBoolVar("test");

  EXPECT_EQ(0, context.GetOrCreateVarValueEncoding(0, 1));
  EXPECT_EQ(-1, context.GetOrCreateVarValueEncoding(0, 0));
}

TEST(PresolveContextTest, GetOrCreateEncodingOnSize2Var) {
  Model model;
  CpModelProto working_model;
  PresolveContext context(&model, &working_model, nullptr);
  context.NewIntVar(Domain::FromValues({1, 4}));

  EXPECT_EQ(-2, context.GetOrCreateVarValueEncoding(0, 1));
  EXPECT_EQ(1, context.GetOrCreateVarValueEncoding(0, 4));
}

TEST(PresolveContextTest, GetOrCreateEncodingOnSize2VarBis) {
  Model model;
  CpModelProto working_model;
  PresolveContext context(&model, &working_model, nullptr);
  context.NewIntVar(Domain::FromValues({1, 4}));

  EXPECT_EQ(1, context.GetOrCreateVarValueEncoding(0, 4));
  EXPECT_EQ(-2, context.GetOrCreateVarValueEncoding(0, 1));
}

TEST(PresolveContextTest, InsertVarValueEncodingOnIntVar) {
  Model model;
  CpModelProto working_model;
  PresolveContext context(&model, &working_model, nullptr);
  context.NewIntVar({1, 5});
  context.NewBoolVar("test");

  context.InsertVarValueEncoding(1, 0, 2);
  EXPECT_EQ(1, context.GetOrCreateVarValueEncoding(0, 2));
  EXPECT_EQ(1, context.GetOrCreateVarValueEncoding(-1, -2));
}

TEST(PresolveContextTest, InsertVarValueEncodingOnSize2Var) {
  Model model;
  CpModelProto working_model;
  PresolveContext context(&model, &working_model, nullptr);
  context.NewIntVar(Domain::FromValues({1, 4}));
  context.NewBoolVar("test");

  context.InsertVarValueEncoding(1, 0, 1);
  EXPECT_EQ(1, context.GetOrCreateVarValueEncoding(0, 1));
  EXPECT_EQ(-2, context.GetOrCreateVarValueEncoding(0, 4));
}

TEST(PresolveContextTest, InsertVarValueEncodingOnSize2VarBis) {
  Model model;
  CpModelProto working_model;
  PresolveContext context(&model, &working_model, nullptr);
  context.NewIntVar(Domain::FromValues({1, 4}));
  context.NewBoolVar("test");

  context.InsertVarValueEncoding(1, 0, 4);
  EXPECT_EQ(1, context.GetOrCreateVarValueEncoding(0, 4));
  EXPECT_EQ(-2, context.GetOrCreateVarValueEncoding(0, 1));
}

TEST(PresolveContextTest, InsertVarValueEncodingOnPosLitMinLit) {
  Model model;
  CpModelProto working_model;
  PresolveContext context(&model, &working_model, nullptr);
  const int a = context.NewBoolVar("test");
  const int b = context.NewBoolVar("test");
  context.InsertVarValueEncoding(a, b, 0);
  EXPECT_EQ(context.GetLiteralRepresentative(b), NegatedRef(a));
  EXPECT_TRUE(context.VarToConstraints(a).contains(kAffineRelationConstraint));
  EXPECT_TRUE(context.VarToConstraints(b).contains(kAffineRelationConstraint));
}

TEST(PresolveContextTest, InsertVarValueEncodingOnPosLitPosMaxLit) {
  Model model;
  CpModelProto working_model;
  PresolveContext context(&model, &working_model, nullptr);
  const int a = context.NewBoolVar("test");
  const int b = context.NewBoolVar("test");
  context.InsertVarValueEncoding(a, b, 1);
  EXPECT_EQ(context.GetLiteralRepresentative(b), a);
  EXPECT_TRUE(context.VarToConstraints(a).contains(kAffineRelationConstraint));
  EXPECT_TRUE(context.VarToConstraints(b).contains(kAffineRelationConstraint));
}

TEST(PresolveContextTest, InsertVarValueEncodingOnNegLitMinLit) {
  Model model;
  CpModelProto working_model;
  PresolveContext context(&model, &working_model, nullptr);
  const int a = context.NewBoolVar("test");
  const int b = context.NewBoolVar("test");
  context.InsertVarValueEncoding(NegatedRef(a), b, 0);
  EXPECT_EQ(context.GetLiteralRepresentative(b), a);
  EXPECT_TRUE(context.VarToConstraints(a).contains(kAffineRelationConstraint));
  EXPECT_TRUE(context.VarToConstraints(b).contains(kAffineRelationConstraint));
}

TEST(PresolveContextTest, InsertVarValueEncodingOnNegLitMaxLit) {
  Model model;
  CpModelProto working_model;
  PresolveContext context(&model, &working_model, nullptr);
  const int a = context.NewBoolVar("test");
  const int b = context.NewBoolVar("test");
  context.InsertVarValueEncoding(NegatedRef(a), b, 1);
  EXPECT_EQ(context.GetLiteralRepresentative(b), NegatedRef(a));
  EXPECT_TRUE(context.VarToConstraints(a).contains(kAffineRelationConstraint));
  EXPECT_TRUE(context.VarToConstraints(b).contains(kAffineRelationConstraint));
}

TEST(PresolveContextTest, InsertVarValueEncodingOnPosLitMinVar) {
  Model model;
  CpModelProto working_model;
  PresolveContext context(&model, &working_model, nullptr);
  const int a = context.NewBoolVar("test");
  const int b = context.NewIntVar(Domain::FromValues({2, 5}));
  context.InsertVarValueEncoding(a, b, 2);

  EXPECT_EQ(context.GetAffineRelation(b).representative, a);
  EXPECT_EQ(context.GetAffineRelation(b).coeff, -3);
  EXPECT_EQ(context.GetAffineRelation(b).offset, 5);

  EXPECT_TRUE(context.VarToConstraints(a).contains(kAffineRelationConstraint));
  EXPECT_TRUE(context.VarToConstraints(b).contains(kAffineRelationConstraint));
}

TEST(PresolveContextTest, InsertVarValueEncodingOnPosLitMaxVar) {
  Model model;
  CpModelProto working_model;
  PresolveContext context(&model, &working_model, nullptr);
  const int a = context.NewBoolVar("test");
  const int b = context.NewIntVar(Domain::FromValues({2, 5}));
  context.InsertVarValueEncoding(a, b, 5);

  EXPECT_EQ(context.GetAffineRelation(b).representative, a);
  EXPECT_EQ(context.GetAffineRelation(b).coeff, 3);
  EXPECT_EQ(context.GetAffineRelation(b).offset, 2);

  EXPECT_TRUE(context.VarToConstraints(a).contains(kAffineRelationConstraint));
  EXPECT_TRUE(context.VarToConstraints(b).contains(kAffineRelationConstraint));
}

TEST(PresolveContextTest, InsertVarValueEncodingOnNegLitMinVar) {
  Model model;
  CpModelProto working_model;
  PresolveContext context(&model, &working_model, nullptr);
  const int a = context.NewBoolVar("test");
  const int b = context.NewIntVar(Domain::FromValues({2, 5}));
  context.InsertVarValueEncoding(NegatedRef(a), b, 2);

  EXPECT_EQ(context.GetAffineRelation(b).representative, a);
  EXPECT_EQ(context.GetAffineRelation(b).coeff, 3);
  EXPECT_EQ(context.GetAffineRelation(b).offset, 2);

  EXPECT_TRUE(context.VarToConstraints(a).contains(kAffineRelationConstraint));
  EXPECT_TRUE(context.VarToConstraints(b).contains(kAffineRelationConstraint));
}

TEST(PresolveContextTest, InsertVarValueEncodingOnNegLitMaxVar) {
  Model model;
  CpModelProto working_model;
  PresolveContext context(&model, &working_model, nullptr);
  const int a = context.NewBoolVar("test");
  const int b = context.NewIntVar(Domain::FromValues({2, 5}));
  context.InsertVarValueEncoding(NegatedRef(a), b, 5);

  EXPECT_EQ(context.GetAffineRelation(b).representative, a);
  EXPECT_EQ(context.GetAffineRelation(b).coeff, -3);
  EXPECT_EQ(context.GetAffineRelation(b).offset, 5);

  EXPECT_TRUE(context.VarToConstraints(a).contains(kAffineRelationConstraint));
  EXPECT_TRUE(context.VarToConstraints(b).contains(kAffineRelationConstraint));
}

TEST(PresolveContextTest, DomainContainsExpr) {
  Model model;
  CpModelProto working_model;
  PresolveContext context(&model, &working_model, nullptr);
  const int var = context.NewIntVar({1, 5});

  LinearExpressionProto expr;
  expr.add_vars(var);
  expr.add_coeffs(3);
  expr.set_offset(2);

  EXPECT_FALSE(context.DomainContains(expr, 2));
  EXPECT_FALSE(context.DomainContains(expr, 7));
  EXPECT_TRUE(context.DomainContains(expr, 11));

  LinearExpressionProto fixed;
  fixed.set_offset(-1);
  EXPECT_FALSE(context.DomainContains(fixed, 2));
  EXPECT_TRUE(context.DomainContains(fixed, -1));

  LinearExpressionProto coeff0;
  coeff0.add_vars(var);
  coeff0.add_coeffs(0);
  coeff0.set_offset(5);
  EXPECT_FALSE(context.DomainContains(coeff0, 2));
  EXPECT_TRUE(context.DomainContains(coeff0, 5));
}

TEST(PresolveContextTest, GetOrCreateEncodingOnAffine) {
  Model model;
  CpModelProto working_model;
  PresolveContext context(&model, &working_model, nullptr);
  const int var = context.NewIntVar({1, 5});

  LinearExpressionProto expr;
  expr.add_vars(var);
  expr.add_coeffs(3);
  expr.set_offset(2);

  const int zero = context.GetFalseLiteral();
  const int one = context.GetTrueLiteral();

  EXPECT_EQ(zero, context.GetOrCreateAffineValueEncoding(expr, 2));
  EXPECT_EQ(zero, context.GetOrCreateAffineValueEncoding(expr, 7));
  EXPECT_EQ(context.GetOrCreateAffineValueEncoding(expr, 11),
            context.GetOrCreateVarValueEncoding(var, 3));

  LinearExpressionProto fixed;
  fixed.set_offset(-1);
  EXPECT_EQ(zero, context.GetOrCreateAffineValueEncoding(fixed, 2));
  EXPECT_EQ(one, context.GetOrCreateAffineValueEncoding(fixed, -1));
}

TEST(PresolveContextTest, LinearExpressionMinMax) {
  Model model;
  CpModelProto working_model;
  PresolveContext context(&model, &working_model, nullptr);
  context.NewIntVar(Domain(0, 1));
  context.NewIntVar(Domain(0, 1));
  const LinearExpressionProto expr = ParseTestProto(R"pb(
    vars: [ 0, 1 ]
    coeffs: [ 2, -3 ]
    offset: 5
  )pb");

  EXPECT_EQ(2, context.MinOf(expr));
  EXPECT_EQ(7, context.MaxOf(expr));
}

TEST(PresolveContextTest, ObjectiveReadCanonicalizeWrite) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 8 ] }
    variables { domain: [ 3, 3 ] }
    variables { domain: [ -2, 7 ] }
    variables { domain: [ -2, -2 ] }
    variables { domain: [ -4, 11 ] }
    objective {
      vars: [ 0, 4, 2, 3, 1 ]
      coeffs: [ 2, 4, -2, -4, -2 ]
      domain: [ 0, 1000 ]
      offset: 3
    }
  )pb");

  PresolveContext context(&model, &working_model, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  EXPECT_TRUE(context.CanonicalizeObjective());
  context.WriteObjectiveToProto();

  const CpModelProto expected = ParseTestProto(R"pb(
    variables { domain: [ 0, 8 ] }
    variables { domain: [ 3, 3 ] }
    variables { domain: [ -2, 7 ] }
    variables { domain: [ -2, -2 ] }
    variables { domain: [ -4, 11 ] }
    objective {
      vars: [ 0, 2, 4 ]
      coeffs: [ 1, -1, 2 ]
      domain: [ -1, 32 ]
      offset: 2.5
      scaling_factor: 2
      integer_before_offset: 1
      integer_scaling_factor: 2
    }
  )pb");
  EXPECT_THAT(working_model, testing::EqualsProto(expected));
}

TEST(PresolveContextTest, ExploitAtMostOneInObjective) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    objective {
      vars: [ 0, 1, 2, 3 ]
      coeffs: [ 2, 3, 7, 4 ]
    }
    constraints { bool_or { literals: [ 0, 1, 2, 3 ] } }
  )pb");

  PresolveContext context(&model, &working_model, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  EXPECT_TRUE(context.CanonicalizeObjective());

  // Do not crash if called with empty exactly one. The problem should be UNSAT
  // in this case, but we might call this before reporting it.
  EXPECT_FALSE(context.ExploitExactlyOneInObjective({}));

  EXPECT_TRUE(context.ExploitExactlyOneInObjective({0, 1, 2}));
  EXPECT_TRUE(context.CanonicalizeObjective());
  context.WriteObjectiveToProto();

  const CpModelProto expected = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    objective {
      vars: [ 1, 2, 3 ]
      coeffs: [ 1, 5, 4 ]
      domain: [ 0, 10 ]
      offset: 2
      scaling_factor: 1
      integer_before_offset: 2
    }
    constraints { bool_or { literals: [ 0, 1, 2, 3 ] } }
  )pb");
  EXPECT_THAT(working_model, testing::EqualsProto(expected));
}

TEST(PresolveContextTest, ExploitAtMostOneInObjectiveNegatedRef) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_or { literals: [ 0, 1, 2, 3 ] } }
    objective {
      vars: [ 0, 1, 2, 3 ]
      coeffs: [ 2, 3, 7, 4 ]
    }
  )pb");

  PresolveContext context(&model, &working_model, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  EXPECT_TRUE(context.CanonicalizeObjective());
  EXPECT_TRUE(context.ExploitExactlyOneInObjective({0, NegatedRef(1), 2}));
  EXPECT_TRUE(context.CanonicalizeObjective());
  context.WriteObjectiveToProto();

  // The objective is 2X + 3(1 - Y) + 7Z  with X + Y + Z = 1
  // So we get 3 + 2 X - 3 Y + 7 Z and when shifted by -3, we get 5X + 10Z.
  const CpModelProto expected = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_or { literals: [ 0, 1, 2, 3 ] } }
    objective {
      vars: [ 0, 2, 3 ]
      coeffs: [ 5, 10, 4 ]
      domain: [ 0, 15 ]  # We get 15 because 16 is not reachable.
      scaling_factor: 1
    }
  )pb");
  EXPECT_THAT(working_model, testing::EqualsProto(expected));
}

TEST(PresolveContextTest, ObjectiveSubstitution) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 9 ] }
    variables { domain: [ 0, 9 ] }
    variables { domain: [ 0, 9 ] }
    variables { domain: [ 0, 9 ] }
    variables { domain: [ 0, 9 ] }
    objective {
      vars: [ 0 ]
      coeffs: [ 1 ]
      domain: [ 0, 1000 ]
      offset: 3
    }
  )pb");

  PresolveContext context(&model, &working_model, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();
  EXPECT_TRUE(context.CanonicalizeObjective());

  const ConstraintProto constraint = ParseTestProto(R"pb(
    linear {
      vars: [ 0, 1, 2 ]
      coeffs: [ -1, 1, 1 ]
      domain: [ 6, 6 ]
    }
  )pb");
  EXPECT_TRUE(context.SubstituteVariableInObjective(0, -1, constraint));

  context.WriteObjectiveToProto();
  const CpModelProto expected = ParseTestProto(R"pb(
    variables { domain: [ 0, 9 ] }
    variables { domain: [ 0, 9 ] }
    variables { domain: [ 0, 9 ] }
    variables { domain: [ 0, 9 ] }
    variables { domain: [ 0, 9 ] }
    objective {
      vars: [ 1, 2 ]
      coeffs: [ 1, 1 ]
      domain: [ 6, 15 ]  #  [0, 9] initially, + 6 offset.
      offset: -3
      integer_before_offset: -6
      scaling_factor: 1
    }
  )pb");
  EXPECT_THAT(working_model, testing::EqualsProto(expected));
}

TEST(PresolveContextTest, ObjectiveSubstitutionWithLargeCoeff) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 9 ] }
    variables { domain: [ 0, 9 ] }
    variables { domain: [ 0, 9 ] }
    variables { domain: [ 0, 9 ] }
    variables { domain: [ 0, 9 ] }
    objective {
      vars: [ 0 ]
      coeffs: [ 4 ]
      domain: [ 0, 1000 ]
      offset: 3
    }
  )pb");

  PresolveContext context(&model, &working_model, nullptr);
  context.InitializeNewDomains();
  context.ReadObjectiveFromProto();

  const ConstraintProto constraint = ParseTestProto(R"pb(
    linear {
      vars: [ 0, 1, 2 ]
      coeffs: [ -2, 1, 1 ]
      domain: [ 6, 6 ]
    }
  )pb");
  EXPECT_TRUE(context.SubstituteVariableInObjective(0, -2, constraint));

  context.WriteObjectiveToProto();
  const CpModelProto expected = ParseTestProto(R"pb(
    variables { domain: [ 0, 9 ] }
    variables { domain: [ 0, 9 ] }
    variables { domain: [ 0, 9 ] }
    variables { domain: [ 0, 9 ] }
    variables { domain: [ 0, 9 ] }
    objective {
      vars: [ 1, 2 ]
      coeffs: [ 2, 2 ]
      domain: [ 12, 1012 ]  #  [0, 1000] initially, + 2*6 offset.
      offset: -9
      integer_before_offset: -12
      scaling_factor: 1
    }
  )pb");
  EXPECT_THAT(working_model, testing::EqualsProto(expected));
}

TEST(PresolveContextTest, VarValueEncoding) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 9 ] }
    variables { domain: [ 0, 9 ] }
  )pb");

  PresolveContext context(&model, &working_model, nullptr);
  context.InitializeNewDomains();
  EXPECT_TRUE(context.StoreLiteralImpliesVarEqValue(0, 2, 4));
  EXPECT_FALSE(context.StoreLiteralImpliesVarEqValue(0, 2, 4));
  EXPECT_FALSE(context.HasVarValueEncoding(2, 4));

  EXPECT_TRUE(context.StoreLiteralImpliesVarNEqValue(-1, 2, 4));
  EXPECT_FALSE(context.StoreLiteralImpliesVarNEqValue(-1, 2, 4));
  EXPECT_TRUE(context.HasVarValueEncoding(2, 4));

  EXPECT_TRUE(context.StoreLiteralImpliesVarNEqValue(0, 1, 4));
  EXPECT_FALSE(context.StoreLiteralImpliesVarNEqValue(0, 1, 4));
  EXPECT_FALSE(context.HasVarValueEncoding(1, 4));

  EXPECT_TRUE(context.StoreLiteralImpliesVarEqValue(-1, 1, 4));
  EXPECT_FALSE(context.StoreLiteralImpliesVarEqValue(-1, 1, 4));
  EXPECT_TRUE(context.HasVarValueEncoding(1, 4));
}

TEST(PresolveContextTest, DetectVarEqValueHalfEncoding) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 9 ] }
    constraints {
      enforcement_literal: 1
      linear {
        vars: [ 2 ]
        coeffs: [ 1 ]
        domain: [ 6, 6 ]
      }
    }
    constraints {
      enforcement_literal: -2
      linear {
        vars: [ 2 ]
        coeffs: [ 1 ]
        domain: [ 0, 5, 7, 9 ]
      }
    }
  )pb");

  const int kLiteral = 1;
  const int kVar = 2;
  const int64_t kValue = 6;

  PresolveContext context(&model, &working_model, nullptr);
  context.InitializeNewDomains();

  context.StoreLiteralImpliesVarEqValue(kLiteral, kVar, kValue);
  context.StoreLiteralImpliesVarNEqValue(NegatedRef(kLiteral), kVar, kValue);
  int encoding_literal = 0;
  EXPECT_TRUE(context.HasVarValueEncoding(kVar, kValue, &encoding_literal));
  EXPECT_EQ(encoding_literal, kLiteral);
}

TEST(PresolveContextTest, GetLiteralRepresentative) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
  )pb");

  PresolveContext context(&model, &working_model, nullptr);
  context.InitializeNewDomains();

  EXPECT_NE(context.GetLiteralRepresentative(0),
            context.GetLiteralRepresentative(1));
  EXPECT_NE(context.GetLiteralRepresentative(0),
            context.GetLiteralRepresentative(2));
  EXPECT_NE(context.GetLiteralRepresentative(1),
            context.GetLiteralRepresentative(2));
  EXPECT_NE(context.GetLiteralRepresentative(-1),
            context.GetLiteralRepresentative(-2));
  EXPECT_NE(context.GetLiteralRepresentative(-1),
            context.GetLiteralRepresentative(-3));
  EXPECT_NE(context.GetLiteralRepresentative(-2),
            context.GetLiteralRepresentative(-3));

  context.StoreBooleanEqualityRelation(0, 1);
  EXPECT_EQ(context.GetLiteralRepresentative(0),
            context.GetLiteralRepresentative(1));
  EXPECT_NE(context.GetLiteralRepresentative(0),
            context.GetLiteralRepresentative(2));
  EXPECT_NE(context.GetLiteralRepresentative(1),
            context.GetLiteralRepresentative(2));
  EXPECT_EQ(context.GetLiteralRepresentative(-1),
            context.GetLiteralRepresentative(-2));
  EXPECT_NE(context.GetLiteralRepresentative(-1),
            context.GetLiteralRepresentative(-3));
  EXPECT_NE(context.GetLiteralRepresentative(-2),
            context.GetLiteralRepresentative(-3));

  context.StoreBooleanEqualityRelation(0, -3);
  EXPECT_EQ(context.GetLiteralRepresentative(0),
            context.GetLiteralRepresentative(1));
  EXPECT_EQ(context.GetLiteralRepresentative(0),
            context.GetLiteralRepresentative(-3));
  EXPECT_EQ(context.GetLiteralRepresentative(1),
            context.GetLiteralRepresentative(-3));
  EXPECT_EQ(context.GetLiteralRepresentative(-1),
            context.GetLiteralRepresentative(-2));
  EXPECT_EQ(context.GetLiteralRepresentative(-1),
            context.GetLiteralRepresentative(2));
  EXPECT_EQ(context.GetLiteralRepresentative(-2),
            context.GetLiteralRepresentative(2));
}

TEST(PresolveContextTest, VarIsOnlyUsedInEncoding) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      enforcement_literal: 0
      linear {
        vars: [ 2 ]
        coeffs: [ 1 ]
        domain: [ 3, 15 ]
      }
    }
    constraints {
      enforcement_literal: 1
      linear {
        vars: [ 2 ]
        coeffs: [ 1 ]
        domain: [ 7, 8 ]
      }
    }
    constraints {
      linear {
        vars: [ 3, 4 ]
        coeffs: [ 1, 1 ]
        domain: [ 5, 5 ]
      }
    }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  context.InitializeNewDomains();
  context.UpdateNewConstraintsVariableUsage();
  EXPECT_FALSE(context.VariableIsOnlyUsedInEncodingAndMaybeInObjective(0));
  EXPECT_FALSE(context.VariableIsOnlyUsedInEncodingAndMaybeInObjective(1));
  EXPECT_TRUE(context.VariableIsOnlyUsedInEncodingAndMaybeInObjective(2));
  EXPECT_FALSE(context.VariableIsOnlyUsedInEncodingAndMaybeInObjective(3));
  EXPECT_FALSE(context.VariableIsOnlyUsedInEncodingAndMaybeInObjective(4));
}

TEST(PresolveContextTest, ReifiedConstraintCache) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    solution_hint {
      vars: [ 0, 1, 2, 3 ]
      values: [ 1, 1, 5, 7 ]
    }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  context.InitializeNewDomains();
  context.UpdateNewConstraintsVariableUsage();
  context.LoadSolutionHint();
  LinearExpressionProto expr1;
  expr1.add_vars(2);
  expr1.add_coeffs(1);
  LinearExpressionProto expr2;
  expr2.add_vars(3);
  expr2.add_coeffs(1);

  const int var2_before_var3 =
      context.GetOrCreateReifiedPrecedenceLiteral(expr1, expr2, 0, 1);
  EXPECT_EQ(var2_before_var3,
            context.GetOrCreateReifiedPrecedenceLiteral(expr1, expr2, 0, 1));
  EXPECT_EQ(var2_before_var3,
            context.GetOrCreateReifiedPrecedenceLiteral(expr1, expr2, 1, 0));
  EXPECT_NE(var2_before_var3,
            context.GetOrCreateReifiedPrecedenceLiteral(expr2, expr1, 1, 0));
  ConstraintProto bool_or = ParseTestProto(R"pb(
    bool_or { literals: [ 5, 4, -2, -1 ] })pb");
  // 2 x (2 implications , 2 enforced linear) + bool_or.
  ASSERT_EQ(9, working_model.constraints_size());
  EXPECT_THAT(working_model.constraints(8), ::testing::EqualsProto(bool_or));
  EXPECT_TRUE(context.DebugTestHintFeasibility());
}

TEST(PresolveContextTest, ExploitFixedDomainOverflow) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 0 }
    variables { domain: 34359738368 domain: 34359738368 }
    constraints { dummy_constraint { vars: 0 vars: 1 } }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  context.InitializeNewDomains();
  context.UpdateNewConstraintsVariableUsage();
}

TEST(PresolveContextTest, IntersectDomainWithConstant) {
  Model model;
  CpModelProto working_model;
  PresolveContext context(&model, &working_model, nullptr);

  LinearExpressionProto constant;
  constant.set_offset(3);
  EXPECT_TRUE(context.IntersectDomainWith(constant, Domain(2, 3)));
  EXPECT_FALSE(context.IntersectDomainWith(constant, Domain(2, 2)));
}

// Most of the logic is already tested by the Domain() manipulation function,
// we just test a simple case here.
TEST(PresolveContextTest, IntersectDomainWithAffineExpression) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 5 }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  context.InitializeNewDomains();

  // -2 x + 3 in [2, 3] so -2x in [-1, 0] and x must be in [0, 1].
  LinearExpressionProto expr;
  expr.add_vars(0);
  expr.add_coeffs(-1);
  expr.set_offset(3);
  EXPECT_TRUE(context.IntersectDomainWith(expr, Domain(2, 3)));
  EXPECT_EQ(context.DomainOf(0), Domain(0, 1));
}

TEST(PresolveContextTest, IntersectDomainAndUpdateHint) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    solution_hint {
      vars: [ 0 ]
      values: [ 3 ]
    }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  context.InitializeNewDomains();
  context.LoadSolutionHint();

  EXPECT_TRUE(context.IntersectDomainWithAndUpdateHint(0, Domain(5, 20)));

  EXPECT_EQ(context.DomainOf(0), Domain(5, 10));
  EXPECT_EQ(context.SolutionHint(0), 5);
}

TEST(PresolveContextTest, DomainSuperSetOf) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 1000 }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  context.InitializeNewDomains();

  const LinearExpressionProto expr1 =
      ParseTestProto(R"pb(vars: 0 coeffs: 1 offset: 4)pb");
  EXPECT_EQ(context.DomainSuperSetOf(expr1), Domain(4, 1004));

  const LinearExpressionProto expr2 =
      ParseTestProto(R"pb(vars: 0 coeffs: 2 offset: 4)pb");
  EXPECT_EQ(context.DomainSuperSetOf(expr2), Domain(4, 2004));
}

TEST(PresolveContextTest, DomainSuperSetOfDiscrete) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 1 }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  context.InitializeNewDomains();

  const LinearExpressionProto expr1 =
      ParseTestProto(R"pb(vars: 0 coeffs: -2 offset: 4)pb");
  EXPECT_EQ(context.DomainSuperSetOf(expr1), Domain::FromValues({2, 4}));
}

TEST(PresolveContextTest, AddAffineRelation) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1000 ] }
    variables { domain: [ 0, 1000 ] }
    variables { domain: [ 0, 1000 ] }
    variables { domain: [ 0, 1000 ] }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  context.InitializeNewDomains();
  context.UpdateNewConstraintsVariableUsage();

  EXPECT_TRUE(context.StoreAffineRelation(0, 1, 3, 0));  // x0 = 3x1
  EXPECT_TRUE(context.StoreAffineRelation(2, 3, 5, 0));  // x2 = 5x3
  EXPECT_TRUE(context.StoreAffineRelation(0, 2, 2, 0));  // x0 = 2x2 !

  // A new variable is created: x4 !
  // x0 = 2x2 get expanded into 3x1 = 10 x3, so x1 is a multiple of 10.
  EXPECT_EQ(context.GetAffineRelation(1).representative, 4);
  EXPECT_EQ(context.GetAffineRelation(1).coeff, 10);
  EXPECT_EQ(context.DomainOf(4).ToString(), "[0,33]");

  // x0 = 3x1 multiple of 30.
  EXPECT_EQ(context.GetAffineRelation(0).representative, 4);
  EXPECT_EQ(context.GetAffineRelation(0).coeff, 30);

  // x3 is a multiple of 3.
  EXPECT_EQ(context.GetAffineRelation(3).representative, 4);
  EXPECT_EQ(context.GetAffineRelation(3).coeff, 3);

  // x2 = 5x3 is a multiple of 15.
  EXPECT_EQ(context.GetAffineRelation(2).representative, 4);
  EXPECT_EQ(context.GetAffineRelation(2).coeff, 15);
}

TEST(PresolveContextTest, AddAffineRelationWithOffset) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1000 ] }
    variables { domain: [ 0, 1000 ] }
    variables { domain: [ 0, 1000 ] }
    variables { domain: [ 0, 1000 ] }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  context.InitializeNewDomains();
  context.UpdateNewConstraintsVariableUsage();

  EXPECT_TRUE(context.StoreAffineRelation(0, 1, 3, 10));  // x0 = 3x1 + 10
  EXPECT_TRUE(context.StoreAffineRelation(2, 3, 1, 30));  // x2 = x3 + 30
  EXPECT_TRUE(context.StoreAffineRelation(0, 2, 1, 0));   // x0 = x2 !

  // x0 = 3x1 + 10
  EXPECT_EQ(context.GetAffineRelation(0).representative, 1);
  EXPECT_EQ(context.GetAffineRelation(0).coeff, 3);
  EXPECT_EQ(context.GetAffineRelation(0).offset, 10);

  // x3 = x2 - 30 = 3x1 - 20
  EXPECT_EQ(context.GetAffineRelation(3).representative, 1);
  EXPECT_EQ(context.GetAffineRelation(3).coeff, 3);
  EXPECT_EQ(context.GetAffineRelation(3).offset, -20);

  // x2 same as x0
  EXPECT_EQ(context.GetAffineRelation(2).representative, 1);
  EXPECT_EQ(context.GetAffineRelation(2).coeff, 3);
  EXPECT_EQ(context.GetAffineRelation(2).offset, 10);
}

TEST(PresolveContextTest, AddAffineRelationPreventOverflow) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1000000 ] }
    variables { domain: [ 100000001, 100000004 ] }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  context.InitializeNewDomains();
  context.UpdateNewConstraintsVariableUsage();

  // x0 = 10 x2 - 1e9.
  EXPECT_TRUE(context.StoreAffineRelation(0, 1, 10, -1000000000));

  // To avoid "future" overflow a new variable is created.
  // And everything is expressed using that one.
  EXPECT_EQ(context.GetAffineRelation(1).representative, 2);
  EXPECT_EQ(context.GetAffineRelation(1).coeff, 1);
  EXPECT_EQ(context.GetAffineRelation(1).offset, 100000001);
  EXPECT_EQ(context.DomainOf(2).ToString(), "[0,3]");

  // And x0 is in term of that one.
  EXPECT_EQ(context.GetAffineRelation(0).representative, 2);
  EXPECT_EQ(context.GetAffineRelation(0).coeff, 10);
  EXPECT_EQ(context.DomainOf(0).ToString(), "[10][20][30][40]");
}

TEST(PresolveContextTest, ObjectiveScalingMinimize) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 10, 30 ] }
    floating_point_objective {
      vars: [ 0, 1 ],
      coeffs: [ 3.5, -1.3333333333 ],
      maximize: false,
      offset: 1.0
    }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  context.InitializeNewDomains();
  ASSERT_TRUE(ScaleFloatingPointObjective(context.params(), context.logger(),
                                          &working_model));
  ASSERT_TRUE(working_model.has_objective());
  ASSERT_FALSE(working_model.has_floating_point_objective());
  const CpObjectiveProto& obj = working_model.objective();
  EXPECT_EQ(2, obj.vars_size());
  EXPECT_FLOAT_EQ(obj.scaling_factor() * obj.coeffs(0), 3.5);
  EXPECT_NEAR(obj.scaling_factor() * obj.coeffs(1), -4.0 / 3.0, 1e-5);
  EXPECT_FLOAT_EQ(obj.scaling_factor() * obj.offset(), 1.0);
}

TEST(PresolveContextTest, ObjectiveScalingMaximize) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 20 ] }
    variables { domain: [ 10, 30 ] }
    floating_point_objective {
      vars: [ 0, 1 ],
      coeffs: [ 3.5, -1.3333333333 ],
      maximize: true,
      offset: 1.0
    }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  context.InitializeNewDomains();
  ASSERT_TRUE(ScaleFloatingPointObjective(context.params(), context.logger(),
                                          &working_model));
  ASSERT_TRUE(working_model.has_objective());
  ASSERT_FALSE(working_model.has_floating_point_objective());
  const CpObjectiveProto& obj = working_model.objective();
  EXPECT_EQ(2, obj.vars_size());
  EXPECT_FLOAT_EQ(obj.scaling_factor() * obj.coeffs(0), 3.5);
  EXPECT_NEAR(obj.scaling_factor() * obj.coeffs(1), -4.0 / 3.0, 1e-5);
  EXPECT_FLOAT_EQ(obj.scaling_factor() * obj.offset(), 1.0);
}

TEST(ExpressionIsALiteralTest, BasicApi) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 1 ] }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  context.InitializeNewDomains();
  context.UpdateNewConstraintsVariableUsage();

  int ref;
  const LinearExpressionProto expr1 = ParseTestProto(R"pb(
    vars: 0 coeffs: 1
  )pb");
  EXPECT_FALSE(context.ExpressionIsALiteral(expr1));

  const LinearExpressionProto expr2 = ParseTestProto(R"pb(
    vars: 1 coeffs: 1
  )pb");
  EXPECT_TRUE(context.ExpressionIsALiteral(expr2, &ref));
  EXPECT_EQ(1, ref);

  const LinearExpressionProto expr3 =
      ParseTestProto(R"pb(
        vars: 1 coeffs: -1 offset: 1
      )pb");
  EXPECT_TRUE(context.ExpressionIsALiteral(expr3, &ref));
  EXPECT_EQ(-2, ref);

  const LinearExpressionProto expr4 =
      ParseTestProto(R"pb(
        vars: 1 coeffs: -1 offset: 2
      )pb");
  EXPECT_FALSE(context.ExpressionIsALiteral(expr4));

  const LinearExpressionProto expr5 =
      ParseTestProto(R"pb(
        vars: -2 coeffs: 1 offset: 1
      )pb");
  EXPECT_TRUE(context.ExpressionIsALiteral(expr5, &ref));
  EXPECT_EQ(-2, ref);
}

TEST(PresolveContextTest, CanonicalizeAffineVariable) {
  Model model;
  CpModelProto working_model;
  PresolveContext context(&model, &working_model, nullptr);
  const int x = context.NewIntVar(Domain(0, 15));

  // 3 * x + 9  is a multiple of 6.
  // This is the same as x + 3 is a multiple of 2.
  EXPECT_TRUE(context.CanonicalizeAffineVariable(x, 3, 6, 9));

  const AffineRelation::Relation r = context.GetAffineRelation(x);
  EXPECT_EQ(r.coeff, 2);
}

TEST(PresolveContextTest, ComputeMinMaxActivity) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 8 ] }
    variables { domain: [ 3, 3 ] }
    variables { domain: [ -2, 7 ] }
    variables { domain: [ -2, -2 ] }
    variables { domain: [ -4, 11 ] }
    objective {
      vars: [ 0, 1, 2, 3, 4 ]
      coeffs: [ 2, 4, -2, -4, -2 ]
      domain: [ 0, 1000 ]
      offset: 3
    }
  )pb");

  PresolveContext context(&model, &working_model, nullptr);
  context.InitializeNewDomains();
  const auto [min_activity, max_activity] =
      context.ComputeMinMaxActivity(working_model.objective());
  EXPECT_EQ(min_activity, 2 * 0 + 4 * 3 - 2 * 7 - 4 * -2 - 2 * 11);
  EXPECT_EQ(max_activity, 2 * 8 + 4 * 3 - 2 * -2 - 4 * -2 - 2 * -4);
}

TEST(PresolveContextTest, CanonicalizeLinearConstraint) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 8 ] }
    variables { domain: [ 0, 8 ] }
    variables { domain: [ 0, 8 ] }
    variables { domain: [ 0, 8 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 0, 1 ]
        coeffs: [ 2, 4, -2, -4, -2 ]
        domain: [ 0, 1000 ]
      }
    }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  context.InitializeNewDomains();

  context.CanonicalizeLinearConstraint(working_model.mutable_constraints(0));

  const ConstraintProto expected = ParseTestProto(R"pb(
    linear {
      vars: [ 0, 1, 2 ]
      coeffs: [ -2, 2, -2 ]
      domain: [ 0, 1000 ]
    }
  )pb");
  EXPECT_THAT(working_model.constraints(0), testing::EqualsProto(expected));
}

TEST(PresolveContextTest, LoadSolutionHint) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 5, 5 ] }
    variables { domain: [ 0, 1 ] }
    solution_hint {
      vars: [ 0, 2 ]
      values: [ 12, 0 ]
    }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  context.InitializeNewDomains();
  context.LoadSolutionHint();

  EXPECT_TRUE(context.HintIsLoaded());
  EXPECT_TRUE(context.VarHasSolutionHint(0));
  EXPECT_TRUE(context.VarHasSolutionHint(1));  // From the fixed domain.
  EXPECT_TRUE(context.VarHasSolutionHint(2));
  EXPECT_EQ(context.SolutionHint(0), 10);  // Clamped to the domain.
  EXPECT_EQ(context.SolutionHint(1), 5);   // From the fixed domain.
  EXPECT_EQ(context.SolutionHint(2), 0);
  EXPECT_EQ(context.GetRefSolutionHint(0), 10);
  EXPECT_EQ(context.GetRefSolutionHint(NegatedRef(0)), -10);
  EXPECT_FALSE(context.LiteralSolutionHint(2));
  EXPECT_TRUE(context.LiteralSolutionHint(NegatedRef(2)));
  EXPECT_TRUE(context.LiteralSolutionHintIs(2, false));
  EXPECT_TRUE(context.LiteralSolutionHintIs(NegatedRef(2), true));
  EXPECT_THAT(context.SolutionHint(), ::testing::ElementsAre(10, 5, 0));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
