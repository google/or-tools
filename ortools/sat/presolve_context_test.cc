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

#include "absl/types/span.h"
#include "google/protobuf/arena.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/base/types.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_canonical_checker.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_status.h"
#include "ortools/sat/solution_crush.h"
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

  EXPECT_EQ(context.InsertVarValueEncoding(1, 0, 2), kOk);
  EXPECT_EQ(1, context.GetOrCreateVarValueEncoding(0, 2));
  EXPECT_EQ(1, context.GetOrCreateVarValueEncoding(-1, -2));
}

TEST(PresolveContextTest, InsertVarValueEncodingOnSize2Var) {
  Model model;
  CpModelProto working_model;
  PresolveContext context(&model, &working_model, nullptr);
  context.NewIntVar(Domain::FromValues({1, 4}));
  context.NewBoolVar("test");

  EXPECT_EQ(context.InsertVarValueEncoding(1, 0, 1), kOk);
  EXPECT_EQ(1, context.GetOrCreateVarValueEncoding(0, 1));
  EXPECT_EQ(-2, context.GetOrCreateVarValueEncoding(0, 4));
}

TEST(PresolveContextTest, InsertVarValueEncodingOnSize2VarBis) {
  Model model;
  CpModelProto working_model;
  PresolveContext context(&model, &working_model, nullptr);
  context.NewIntVar(Domain::FromValues({1, 4}));
  context.NewBoolVar("test");

  EXPECT_EQ(context.InsertVarValueEncoding(1, 0, 4), kOk);
  EXPECT_EQ(1, context.GetOrCreateVarValueEncoding(0, 4));
  EXPECT_EQ(-2, context.GetOrCreateVarValueEncoding(0, 1));
}

TEST(PresolveContextTest, InsertVarValueEncodingOnPosLitMinLit) {
  Model model;
  CpModelProto working_model;
  PresolveContext context(&model, &working_model, nullptr);
  const int a = context.NewBoolVar("test");
  const int b = context.NewBoolVar("test");
  EXPECT_EQ(context.InsertVarValueEncoding(a, b, 0), kOk);
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
  EXPECT_EQ(context.InsertVarValueEncoding(a, b, 1), kOk);
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
  EXPECT_EQ(context.InsertVarValueEncoding(NegatedRef(a), b, 0), kOk);
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
  EXPECT_EQ(context.InsertVarValueEncoding(NegatedRef(a), b, 1), kOk);
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
  EXPECT_EQ(context.InsertVarValueEncoding(a, b, 2), kOk);

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
  EXPECT_EQ(context.InsertVarValueEncoding(a, b, 5), kOk);

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
  EXPECT_EQ(context.InsertVarValueEncoding(NegatedRef(a), b, 2), kOk);

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
  EXPECT_EQ(context.InsertVarValueEncoding(NegatedRef(a), b, 5), kOk);

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
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  context.ReadObjectiveFromProto();
  EXPECT_EQ(context.CanonicalizeObjective(), kOk);
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
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  context.ReadObjectiveFromProto();
  EXPECT_EQ(context.CanonicalizeObjective(), kOk);

  // Do not crash if called with empty exactly one. The problem should be UNSAT
  // in this case, but we might call this before reporting it.
  EXPECT_FALSE(context.ExploitExactlyOneInObjective({}));

  EXPECT_TRUE(context.ExploitExactlyOneInObjective({0, 1, 2}));
  EXPECT_EQ(context.CanonicalizeObjective(), kOk);
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
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  context.ReadObjectiveFromProto();
  EXPECT_EQ(context.CanonicalizeObjective(), kOk);
  EXPECT_TRUE(context.ExploitExactlyOneInObjective({0, NegatedRef(1), 2}));
  EXPECT_EQ(context.CanonicalizeObjective(), kOk);
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
      domain: [ 0, 16 ]  # 16 is not reachable, but we don't see that.
      scaling_factor: 1
    }
  )pb");
  EXPECT_THAT(working_model, testing::EqualsProto(expected));
}

TEST(PresolveContextTest,
     ShiftCostInExactlyOneRejectsOverflowAboveHalfInt64Max) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { exactly_one { literals: [ 0, 1, 2, 3 ] } }
    constraints { bool_or { literals: [ 0, 4 ] } }
  )pb");
  const int64_t kQuarterMax = kint64max / 4;
  working_model.mutable_objective()->add_vars(0);
  working_model.mutable_objective()->add_coeffs(kQuarterMax);
  working_model.mutable_objective()->add_vars(4);
  working_model.mutable_objective()->add_coeffs(1);

  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  context.ReadObjectiveFromProto();
  EXPECT_EQ(context.CanonicalizeObjective(), kOk);

  // Shifting {0, 1, 2, 3} by kQuarterMax would produce objective terms
  // -kQuarterMax * x1 - kQuarterMax * x2 - kQuarterMax * x3 + x4, which has
  // sum_min = -3 * kQuarterMax < -kint64max / 2 (failing ValidateObjective's
  // PossibleIntegerOverflow check) even though 3 * kQuarterMax + 1 < kint64max.
  EXPECT_FALSE(context.ShiftCostInExactlyOne({0, 1, 2, 3}, kQuarterMax));
  EXPECT_EQ(context.ObjectiveCoeff(0), kQuarterMax);
  EXPECT_EQ(context.ObjectiveCoeff(1), 0);
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
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  context.ReadObjectiveFromProto();
  EXPECT_EQ(context.CanonicalizeObjective(), kOk);

  const ConstraintProto constraint = ParseTestProto(R"pb(
    linear {
      vars: [ 0, 1, 2 ]
      coeffs: [ -1, 1, 1 ]
      domain: [ 6, 6 ]
    }
  )pb");
  EXPECT_EQ(context.SubstituteVariableInObjective(0, -1, constraint),
            kModified);

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
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  context.ReadObjectiveFromProto();

  const ConstraintProto constraint = ParseTestProto(R"pb(
    linear {
      vars: [ 0, 1, 2 ]
      coeffs: [ -2, 1, 1 ]
      domain: [ 6, 6 ]
    }
  )pb");
  EXPECT_EQ(context.SubstituteVariableInObjective(0, -2, constraint),
            kModified);

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
      domain: [ 12, 36 ]  #  [0, 1000] initially, + 2*6 offset.
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
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  context.modified_domains.ResetAllToFalse();
  EXPECT_EQ(context.StoreLiteralImpliesVarEqValue(0, 2, 4), kModified);
  EXPECT_TRUE(context.modified_domains[2]);
  context.modified_domains.ResetAllToFalse();
  EXPECT_EQ(context.StoreLiteralImpliesVarEqValue(0, 2, 4), kUnchanged);
  EXPECT_FALSE(context.modified_domains[2]);
  EXPECT_FALSE(context.HasVarValueEncoding(2, 4));

  EXPECT_EQ(context.StoreLiteralImpliesVarNeValue(-1, 2, 4), kModified);
  EXPECT_EQ(context.StoreLiteralImpliesVarNeValue(-1, 2, 4), kUnchanged);
  EXPECT_TRUE(context.HasVarValueEncoding(2, 4));

  EXPECT_EQ(context.StoreLiteralImpliesVarNeValue(0, 1, 4), kModified);
  EXPECT_EQ(context.StoreLiteralImpliesVarNeValue(0, 1, 4), kUnchanged);
  EXPECT_FALSE(context.HasVarValueEncoding(1, 4));

  EXPECT_EQ(context.StoreLiteralImpliesVarEqValue(-1, 1, 4), kModified);
  EXPECT_EQ(context.StoreLiteralImpliesVarEqValue(-1, 1, 4), kUnchanged);
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
  EXPECT_EQ(context.InitializeNewDomains(), kOk);

  EXPECT_EQ(context.StoreLiteralImpliesVarEqValue(kLiteral, kVar, kValue),
            kModified);
  EXPECT_EQ(
      context.StoreLiteralImpliesVarNeValue(NegatedRef(kLiteral), kVar, kValue),
      kModified);
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
  EXPECT_EQ(context.InitializeNewDomains(), kOk);

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

  EXPECT_EQ(context.StoreBooleanEqualityRelation(0, 1), kOk);
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

  EXPECT_EQ(context.StoreBooleanEqualityRelation(0, -3), kOk);
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

TEST(PresolveContextTest, GetLiteralRepresentativeWithAffineRelations) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ -1, 0 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ -1, 1 ] }
    variables { domain: [ 0, 2 ] }
  )pb");
  CpModelProto mapping_model;
  PresolveContext context(&model, &working_model, &mapping_model);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);

  // Linking [-1, 0] with [0, 1] via x0 = -x1 chooses the [0, 1] variable x1 as
  // representative and keeps GetLiteralRepresentative valid even after fixing.
  ASSERT_EQ(context.StoreAffineRelation(0, 1, -1, 0), kOk);
  EXPECT_EQ(context.GetLiteralRepresentative(1), 1);
  EXPECT_EQ(context.GetLiteralRepresentative(NegatedRef(1)), NegatedRef(1));
  ASSERT_EQ(context.IntersectDomainWith(1, Domain(0)), kOk);
  EXPECT_EQ(context.GetLiteralRepresentative(1), 1);

  // When x3 = x2 + 1 shrinks into [0, 1] after being linked, canonicalizing its
  // [-1, 0] representative x2 restores a [0, 1] representative for x3.
  ASSERT_EQ(context.StoreAffineRelation(3, 2, 1, 1), kOk);
  ASSERT_EQ(context.IntersectDomainWith(3, Domain(0, 1)), kOk);
  EXPECT_EQ(context.DomainOf(2), Domain(-1, 0));
  EXPECT_EQ(context.CanonicalizeDomainOfSizeTwo(2), kOk);
  const int rep = context.GetAffineRelation(3).representative;
  EXPECT_TRUE(context.CanBeUsedAsLiteral(rep));
  EXPECT_EQ(context.GetLiteralRepresentative(3), rep);
  EXPECT_EQ(context.GetLiteralRepresentative(NegatedRef(3)), NegatedRef(rep));
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
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
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
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  context.LoadAndClampSolutionHint();
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
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
}

TEST(PresolveContextTest, IntersectDomainWithConstant) {
  Model model;
  CpModelProto working_model;
  PresolveContext context(&model, &working_model, nullptr);

  LinearExpressionProto constant;
  constant.set_offset(3);
  EXPECT_EQ(context.IntersectDomainWith(constant, Domain(2, 3)), kOk);
  EXPECT_EQ(context.IntersectDomainWith(constant, Domain(2, 2)), kUnsat);
}

// Most of the logic is already tested by the Domain() manipulation function,
// we just test a simple case here.
TEST(PresolveContextTest, IntersectDomainWithAffineExpression) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 5 }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);

  // -2 x + 3 in [2, 3] so -2x in [-1, 0] and x must be in [0, 1].
  LinearExpressionProto expr;
  expr.add_vars(0);
  expr.add_coeffs(-1);
  expr.set_offset(3);
  EXPECT_EQ(context.IntersectDomainWith(expr, Domain(2, 3)), kOk);
  EXPECT_EQ(context.DomainOf(0), Domain(0, 1));
}

TEST(PresolveContextTest, IntersectDomainWithUpdatesHint) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    solution_hint {
      vars: [ 0 ]
      values: [ 3 ]
    }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  context.LoadAndClampSolutionHint();

  EXPECT_EQ(context.IntersectDomainWith(0, Domain(5, 20)), kOk);

  EXPECT_EQ(context.DomainOf(0), Domain(5, 10));
  EXPECT_EQ(context.solution_crush().GetVarValues()[0], 5);
}

TEST(PresolveContextTest, DomainSuperSetOf) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 1000 }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);

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
  EXPECT_EQ(context.InitializeNewDomains(), kOk);

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
  EXPECT_EQ(context.InitializeNewDomains(), kOk);

  EXPECT_EQ(context.StoreAffineRelation(0, 1, 3, 0), kOk);  // x0 = 3x1
  EXPECT_EQ(context.StoreAffineRelation(2, 3, 5, 0), kOk);  // x2 = 5x3
  EXPECT_EQ(context.StoreAffineRelation(0, 2, 2, 0), kOk);  // x0 = 2x2 !

  // A new variable is created: x4 !
  // x0 = 2x2 gets expanded into 3x1 = 10 x3, so x1 is a multiple of 10.
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
  EXPECT_EQ(context.InitializeNewDomains(), kOk);

  EXPECT_EQ(context.StoreAffineRelation(0, 1, 3, 10),
            kOk);  // x0 = 3x1 + 10
  EXPECT_EQ(context.StoreAffineRelation(2, 3, 1, 30),
            kOk);                                           // x2 = x3 + 30
  EXPECT_EQ(context.StoreAffineRelation(0, 2, 1, 0), kOk);  // x0 = x2 !

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

TEST(PresolveContextTest, AddAffineRelationCommonRepresentativeWithOffsets) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1000 ] }
    variables { domain: [ 0, 1000 ] }
    variables { domain: [ 0, 1000 ] }
    variables { domain: [ 0, 1000 ] }
    solution_hint {
      vars: [ 0, 1, 2, 3 ]
      values: [ 34, 11, 17, 3 ]
    }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  context.LoadAndClampSolutionHint();

  ASSERT_EQ(context.StoreAffineRelation(0, 1, 3, 1),
            kOk);  // x0 = 3 * x1 + 1
  ASSERT_EQ(context.StoreAffineRelation(2, 3, 5, 2),
            kOk);  // x2 = 5 * x3 + 2
  // 3 * x1 + 1 = 2 * (5 * x3 + 2) -> 3 * x1 - 10 * x3 = 3.
  ASSERT_EQ(context.StoreAffineRelation(0, 2, 2, 0), kOk);

  EXPECT_EQ(context.DomainOf(4), Domain(0, 33));
  EXPECT_EQ(context.GetAffineRelation(1),
            (AffineRelation::Relation{/*representative=*/4, /*coeff=*/10,
                                      /*offset=*/1}));
  EXPECT_EQ(context.GetAffineRelation(0),
            (AffineRelation::Relation{/*representative=*/4, /*coeff=*/30,
                                      /*offset=*/4}));
  EXPECT_EQ(context.GetAffineRelation(3),
            (AffineRelation::Relation{/*representative=*/4, /*coeff=*/3,
                                      /*offset=*/0}));
  EXPECT_EQ(context.GetAffineRelation(2),
            (AffineRelation::Relation{/*representative=*/4, /*coeff=*/15,
                                      /*offset=*/2}));
  EXPECT_THAT(context.solution_crush().GetVarValues(),
              testing::ElementsAre(34, 11, 17, 3, 1));
}

TEST(PresolveContextTest, AddAffineRelationCommonRepresentativeWithGcd) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1000 ] }
    variables { domain: [ 0, 1000 ] }
    variables { domain: [ 0, 1000 ] }
    variables { domain: [ 0, 1000 ] }
    solution_hint {
      vars: [ 0, 1, 2, 3 ]
      values: [ 42, 7, 40, 4 ]
    }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  context.LoadAndClampSolutionHint();

  ASSERT_EQ(context.StoreAffineRelation(0, 1, 6, 0), kOk);  // x0 = 6 * x1
  ASSERT_EQ(context.StoreAffineRelation(2, 3, 10, 0),
            kOk);  // x2 = 10 * x3
  // 6 * x1 = 10 * x3 + 2 -> 3 * x1 - 5 * x3 = 1 after dividing by gcd(6, 10).
  ASSERT_EQ(context.StoreAffineRelation(0, 2, 1, 2), kOk);

  EXPECT_EQ(context.DomainOf(4), Domain(0, 32));
  EXPECT_EQ(context.GetAffineRelation(1),
            (AffineRelation::Relation{/*representative=*/4, /*coeff=*/5,
                                      /*offset=*/2}));
  EXPECT_EQ(context.GetAffineRelation(0),
            (AffineRelation::Relation{/*representative=*/4, /*coeff=*/30,
                                      /*offset=*/12}));
  EXPECT_EQ(context.GetAffineRelation(3),
            (AffineRelation::Relation{/*representative=*/4, /*coeff=*/3,
                                      /*offset=*/1}));
  EXPECT_EQ(context.GetAffineRelation(2),
            (AffineRelation::Relation{/*representative=*/4, /*coeff=*/30,
                                      /*offset=*/10}));
  EXPECT_THAT(context.solution_crush().GetVarValues(),
              testing::ElementsAre(42, 7, 40, 4, 1));
}

TEST(PresolveContextTest, AddAffineRelationPreventOverflow) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1000000 ] }
    variables { domain: [ 100000001, 100000004 ] }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);

  // x0 = 10 x2 - 1e9.
  EXPECT_EQ(context.StoreAffineRelation(0, 1, 10, -1000000000), kOk);

  // To avoid "future" overflow a new variable is created.
  // And everything is expressed using that one.
  EXPECT_EQ(context.GetAffineRelation(1).representative, 2);
  EXPECT_EQ(context.GetAffineRelation(1).coeff, 1);
  EXPECT_EQ(context.GetAffineRelation(1).offset, 100000001);
  EXPECT_EQ(context.DomainOf(2).ToString(), "[0,3]");

  // And x0 is in terms of that one.
  EXPECT_EQ(context.GetAffineRelation(0).representative, 2);
  EXPECT_EQ(context.GetAffineRelation(0).coeff, 10);
  EXPECT_EQ(context.DomainOf(0).ToString(), "[10][20][30][40]");
}

TEST(PresolveContextTest,
     StoreAffineRelationPropagatesFixedRepresentativesWhenDomainWasRelaxed) {
  // Subcase 1 (|o| > max(|MinOf(x)|, |MaxOf(x)|)):
  // y (var 0) has Size() = 201 > kDomainComplexityLimit, so var_y = 10 * y
  // (var 1) keeps the continuous interval [1000, 3000]. Adding
  // var_x = var_y - 1000 with var_x in [0, 5] fixes y = 100 and x = 0 during
  // representative propagation while var_y has [1000, 1005]; var_y must also be
  // propagated to [1000, 1000] rather than left unfixed.
  {
    Model model;
    CpModelProto working_model = ParseTestProto(R"pb(
      variables { domain: [ 100, 300 ] }
      variables { domain: [ 1000, 3000 ] }
      variables { domain: [ 0, 5 ] }
      solution_hint {
        vars: [ 0, 1, 2 ]
        values: [ 150, 1500, 4 ]
      }
    )pb");
    PresolveContext context(&model, &working_model, nullptr);
    EXPECT_EQ(context.InitializeNewDomains(), kOk);
    context.LoadAndClampSolutionHint();

    ASSERT_EQ(context.StoreAffineRelation(1, 0, 10, 0), kOk);
    ASSERT_EQ(context.StoreAffineRelation(2, 1, 1, -1000), kOk);

    EXPECT_EQ(context.DomainOf(0), Domain(100));
    EXPECT_EQ(context.DomainOf(1), Domain(1000));
    EXPECT_EQ(context.DomainOf(2), Domain(0));
    EXPECT_EQ(context.GetAffineRelation(2).representative, 2);
    EXPECT_THAT(context.solution_crush().GetVarValues(),
                testing::ElementsAre(100, 1000, 0));
  }

  // Subcase 2 (|o| <= max(|MinOf(x)|, |MaxOf(x)|)):
  // x (var 0) is Boolean [0, 1] with var_x = 2 * x (var 2, domain {0, 2}).
  // y (var 1) has domain [-200, 0] (Size() = 201 > kDomainComplexityLimit) with
  // var_y = -4 * y - 2 (var 3, continuous domain [-2, 798]). Adding
  // var_x = var_y reduces to x = -2 * y - 1 and fixes x = 1 and y = -1 during
  // representative propagation; it must not fall through to AddRelation and
  // assign non-literal y ([-1, -1]) as the representative of Boolean x.
  {
    Model model;
    CpModelProto working_model = ParseTestProto(R"pb(
      variables { domain: [ 0, 1 ] }
      variables { domain: [ -200, 0 ] }
      variables { domain: [ 0, 2 ] }
      variables { domain: [ -10, 800 ] }
    )pb");
    PresolveContext context(&model, &working_model, nullptr);
    EXPECT_EQ(context.InitializeNewDomains(), kOk);

    ASSERT_EQ(context.StoreAffineRelation(2, 0, 2, 0), kOk);
    ASSERT_EQ(context.StoreAffineRelation(3, 1, -4, -2), kOk);
    ASSERT_EQ(context.StoreAffineRelation(2, 3, 1, 0), kOk);

    EXPECT_EQ(context.DomainOf(0), Domain(1));
    EXPECT_EQ(context.DomainOf(1), Domain(-1));
    EXPECT_EQ(context.DomainOf(2), Domain(2));
    EXPECT_EQ(context.DomainOf(3), Domain(2));
    EXPECT_EQ(context.GetAffineRelation(0).representative, 0);
    EXPECT_TRUE(context.CanBeUsedAsLiteral(
        context.GetAffineRelation(0).representative));
    EXPECT_EQ(context.GetLiteralRepresentative(0), 0);
  }
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
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
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
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
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
  EXPECT_EQ(context.InitializeNewDomains(), kOk);

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
  EXPECT_EQ(context.CanonicalizeAffineVariable(x, 3, 6, 9), kOk);

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
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  const auto [min_activity, max_activity] =
      context.ComputeMinMaxActivity(working_model.objective());
  EXPECT_EQ(min_activity, 2 * 0 + 4 * 3 - 2 * 7 - 4 * -2 - 2 * 11);
  EXPECT_EQ(max_activity, 2 * 8 + 4 * 3 - 2 * -2 - 4 * -2 - 2 * -4);

  const PresolveContext::LinearStats stats =
      context.ComputeLinearStats(working_model.objective());
  EXPECT_EQ(stats.min_activity, min_activity);
  EXPECT_EQ(stats.max_activity, max_activity);
  EXPECT_EQ(stats.min_coeff_magnitude, 2);
  EXPECT_EQ(stats.max_coeff_magnitude, 4);
}

TEST(PresolveContextTest, BuildMergedLinearConstraintView) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ -3, 1 ] }
    variables { domain: [ 2, 10 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 7, 10 ] }
    variables { domain: [ 4, 4 ] }
    constraints {
      linear {
        vars: [ 0, 1, 2, 3, 4, 5 ]
        coeffs: [ 10, -10, 3, 7, -3, 5 ]
        domain: [ 0, 100 ]
      }
    }
  )pb");

  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);

  // Ranges are [5, 4, 8, 2, 3, 0] with magnitudes [10, 10, 3, 7, 3, 5]:
  //   magnitude 3  -> 8 + 3 = 11
  //   magnitude 5  -> skipped (range == 0), absorbed into min_activity (+20)
  //   magnitude 7  -> 2
  //   magnitude 10 -> 5 + 4 = 9
  // min_activity = 10*0 - 10*1 + 3*2 + 7*0 - 3*10 + 5*4 = -14.
  const PresolveContext::MergedLinearConstraintView view =
      context.BuildMergedLinearConstraintView(
          working_model.constraints(0).linear());
  EXPECT_THAT(view.terms,
              testing::ElementsAre(
                  PresolveContext::CanonicalTerm{.range = 11, .magnitude = 3},
                  PresolveContext::CanonicalTerm{.range = 2, .magnitude = 7},
                  PresolveContext::CanonicalTerm{.range = 9, .magnitude = 10}));
  EXPECT_EQ(view.lb, 14);
  EXPECT_EQ(view.ub, 114);
  EXPECT_EQ(view.max_variation, 137);

  const LinearConstraintProto empty_proto =
      ParseTestProto(R"pb(domain: [ 0, 0 ])pb");
  const PresolveContext::MergedLinearConstraintView empty =
      context.BuildMergedLinearConstraintView(empty_proto);
  EXPECT_THAT(empty.terms, testing::IsEmpty());
  EXPECT_EQ(empty.lb, 0);
  EXPECT_EQ(empty.ub, 0);
  EXPECT_EQ(empty.max_variation, 0);
}

TEST(PresolveContextTest, BuildMergedLinearConstraintViewClampsBounds) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 4 ] }
    variables { domain: [ -2, 3 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 2, -3 ]
        domain: [ -100, 100 ]
      }
    }
  )pb");

  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);

  // min_activity = 2*1 - 3*3 = -7, max_activity = 2*4 - 3*(-2) = 14.
  // Bounds [-100, 100] are clamped to
  // [0, max_activity - min_activity] = [0, 21].
  const PresolveContext::MergedLinearConstraintView view =
      context.BuildMergedLinearConstraintView(
          working_model.constraints(0).linear());
  EXPECT_THAT(view.terms,
              testing::ElementsAre(
                  PresolveContext::CanonicalTerm{.range = 3, .magnitude = 2},
                  PresolveContext::CanonicalTerm{.range = 5, .magnitude = 3}));
  EXPECT_EQ(view.lb, 0);
  EXPECT_EQ(view.ub, 21);
  EXPECT_EQ(view.max_variation, 21);
}

TEST(PresolveContextTest, BuildMergedLinearConstraintViewInfeasible) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 4 ] }
    variables { domain: [ -2, 3 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 2, -3 ]
        domain: [ 15, 20 ]
      }
    }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 2, -3 ]
        domain: [ -20, -8 ]
      }
    }
  )pb");

  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);

  // min_activity = -7, max_activity = 14.
  // Constraint 0 has ct_lb = 15 > max_activity.
  const PresolveContext::MergedLinearConstraintView above_max =
      context.BuildMergedLinearConstraintView(
          working_model.constraints(0).linear());
  EXPECT_THAT(above_max.terms, testing::IsEmpty());
  EXPECT_EQ(above_max.lb, 1);
  EXPECT_EQ(above_max.ub, 0);
  EXPECT_EQ(above_max.max_variation, 0);

  // Constraint 1 has ct_ub = -8 < min_activity.
  const PresolveContext::MergedLinearConstraintView below_min =
      context.BuildMergedLinearConstraintView(
          working_model.constraints(1).linear());
  EXPECT_THAT(below_min.terms, testing::IsEmpty());
  EXPECT_EQ(below_min.lb, 1);
  EXPECT_EQ(below_min.ub, 0);
  EXPECT_EQ(below_min.max_variation, 0);
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
  EXPECT_EQ(context.InitializeNewDomains(), kOk);

  context.CanonicalizeLinearConstraint(working_model.mutable_constraints(0));

  const ConstraintProto expected = ParseTestProto(R"pb(
    linear {
      vars: [ 0, 1, 2 ]
      coeffs: [ -2, 2, -2 ]
      domain: [ 0, 16 ]
    }
  )pb");
  EXPECT_THAT(working_model.constraints(0), testing::EqualsProto(expected));
}

TEST(PresolveContextTest, LoadAndClampSolutionHint) {
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
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  context.LoadAndClampSolutionHint();

  context.solution_crush().StoreSolutionAsHint(working_model);
  // All hints should be clamped to their respective domains, and new hints
  // should be added for the fixed variables.
  PartialVariableAssignment expected_solution_hint =
      ParseTestProto(R"pb(vars: [ 0, 1, 2 ]
                          values: [ 10, 5, 0 ])pb");
  EXPECT_THAT(working_model.solution_hint(),
              testing::EqualsProto(expected_solution_hint));
}

TEST(PresolveContextTest, IntersectionOfAffineExprsIsNotEmpty_easy) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  LinearExpressionProto expr1;
  expr1.add_vars(0);
  expr1.add_coeffs(1);
  LinearExpressionProto expr2;
  expr2.add_vars(1);
  expr2.add_coeffs(1);
  EXPECT_TRUE(context.IntersectionOfAffineExprsIsNotEmpty(expr1, expr2));
}

TEST(PresolveContextTest, IntersectionOfAffineExprsIsNotEmpty_shifted) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  LinearExpressionProto expr1;
  expr1.add_vars(0);
  expr1.add_coeffs(2);
  LinearExpressionProto expr2;
  expr2.add_vars(1);
  expr2.add_coeffs(2);
  expr2.set_offset(1);
  EXPECT_FALSE(context.IntersectionOfAffineExprsIsNotEmpty(expr1, expr2));
}

TEST(PresolveContextTest, IntersectionOfAffineExprsIsNotEmpty_prime_nosol) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 6 ] }
    variables { domain: [ 1, 4 ] }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  LinearExpressionProto expr1;
  expr1.add_vars(0);
  expr1.add_coeffs(5);
  LinearExpressionProto expr2;
  expr2.add_vars(1);
  expr2.add_coeffs(7);
  EXPECT_FALSE(context.IntersectionOfAffineExprsIsNotEmpty(expr1, expr2));
}

TEST(PresolveContextTest, IntersectionOfAffineExprsIsNotEmpty_prime_nosol2) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 6 ] }
    variables { domain: [ 1, 5 ] }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  LinearExpressionProto expr1;
  expr1.add_vars(0);
  expr1.add_coeffs(5);
  LinearExpressionProto expr2;
  expr2.add_vars(1);
  expr2.add_coeffs(7);
  EXPECT_FALSE(context.IntersectionOfAffineExprsIsNotEmpty(expr1, expr2));
}

TEST(PresolveContextTest, IntersectionOfAffineExprsIsNotEmpty_prime_reach) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 1, 7 ] }
    variables { domain: [ 1, 5 ] }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  LinearExpressionProto expr1;
  expr1.add_vars(0);
  expr1.add_coeffs(5);
  LinearExpressionProto expr2;
  expr2.add_vars(1);
  expr2.add_coeffs(7);
  EXPECT_TRUE(context.IntersectionOfAffineExprsIsNotEmpty(expr1, expr2));
}

TEST(PresolveContextTest, CanonicalizeLinMaxSortsTermsAndExpressions) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      lin_max {
        target {
          vars: [ 2, 1, 2 ]
          coeffs: [ 1, 3, 2 ]
          offset: 4
        }
        exprs {
          vars: [ 1, 0 ]
          coeffs: [ 2, 1 ]
          offset: 5
        }
        exprs {
          vars: [ 0, 1 ]
          coeffs: [ 1, -1 ]
          offset: 10
        }
        exprs {
          vars: [ 0, 1 ]
          coeffs: [ 1, -1 ]
          offset: 2
        }
      }
    }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  EXPECT_FALSE(context.ConstraintIsCanonical(0));
  EXPECT_EQ(context.CanonicalizeConstraintsLazily(), kOk);
  EXPECT_TRUE(context.ModelProtoIsCanonical());

  const ConstraintProto expected = ParseTestProto(R"pb(
    lin_max {
      target {
        vars: [ 1, 2 ]
        coeffs: [ 3, 3 ]
        offset: 4
      }
      exprs {
        vars: [ 0, 1 ]
        coeffs: [ 1, -1 ]
        offset: 2
      }
      exprs {
        vars: [ 0, 1 ]
        coeffs: [ 1, -1 ]
        offset: 10
      }
      exprs {
        vars: [ 0, 1 ]
        coeffs: [ 1, 2 ]
        offset: 5
      }
    }
  )pb");
  EXPECT_THAT(context.Constraint(0), testing::EqualsProto(expected));
}

TEST(PresolveContextTest, CanonicalizeLinMaxCancelsZeroCoefficientTerms) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      lin_max {
        target {
          vars: [ 0, 1, 0 ]
          coeffs: [ 2, 0, -2 ]
          offset: 3
        }
        exprs {
          vars: [ 1, 1 ]
          coeffs: [ 4, -4 ]
          offset: 7
        }
        exprs { offset: 1 }
      }
    }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  EXPECT_FALSE(context.ConstraintIsCanonical(0));
  EXPECT_EQ(context.CanonicalizeConstraintsLazily(), kOk);
  EXPECT_TRUE(context.ModelProtoIsCanonical());

  const ConstraintProto expected = ParseTestProto(R"pb(
    lin_max {
      target { offset: 3 }
      exprs { offset: 1 }
      exprs { offset: 7 }
    }
  )pb");
  EXPECT_THAT(context.Constraint(0), testing::EqualsProto(expected));
}

TEST(PresolveContextTest, CanonicalizeLinMaxEnforcementLiterals) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 20 ] }
    constraints {
      enforcement_literal: [ 1, -1, 1 ]
      lin_max {
        target {
          vars: [ 0, 1, 2 ]
          coeffs: [ 3, 4, 5 ]
          offset: 1
        }
        exprs {
          vars: [ 1, 2 ]
          coeffs: [ 2, 1 ]
          offset: 10
        }
        exprs {
          vars: [ 0, 2 ]
          coeffs: [ -2, 1 ]
          offset: 5
        }
      }
    }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  EXPECT_FALSE(context.ConstraintIsCanonical(0));
  EXPECT_EQ(context.CanonicalizeConstraintsLazily(), kOk);
  EXPECT_TRUE(context.ModelProtoIsCanonical());

  const ConstraintProto expected = ParseTestProto(R"pb(
    enforcement_literal: [ -1, 1 ]
    lin_max {
      target {
        vars: [ 2 ]
        coeffs: [ 5 ]
        offset: 5
      }
      exprs {
        vars: [ 2 ]
        coeffs: [ 1 ]
        offset: 5
      }
      exprs {
        vars: [ 2 ]
        coeffs: [ 1 ]
        offset: 12
      }
    }
  )pb");
  EXPECT_THAT(context.Constraint(0), testing::EqualsProto(expected));
}

TEST(PresolveContextTest, CanonicalizeLinearConstraintWithEnforcements) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      enforcement_literal: [ 0, -2 ]
      linear {
        vars: [ 2, 0, 2, 1 ]
        coeffs: [ 3, 4, -1, 5 ]
        domain: [ 10, 20 ]
      }
    }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  EXPECT_EQ(context.CanonicalizeLinearConstraint(context.MutableConstraint(0)),
            kModified);
  const ConstraintProto expected = ParseTestProto(R"pb(
    enforcement_literal: [ 0, -2 ]
    linear {
      vars: [ 2 ]
      coeffs: [ 2 ]
      domain: [ 6, 16 ]
    }
  )pb");
  EXPECT_THAT(context.Constraint(0), testing::EqualsProto(expected));
}

TEST(PresolveContextTest, CanonicalizeLinMaxLiteralAndNegationInInitialProto) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      enforcement_literal: [ 0, -1 ]
      lin_max {
        target {
          vars: [ 1 ]
          coeffs: [ 1 ]
        }
        exprs {
          vars: [ 2 ]
          coeffs: [ 1 ]
        }
      }
    }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  EXPECT_EQ(kModified, context.CanonicalizeConstraint(0));
  EXPECT_EQ(context.Constraint(0).constraint_case(),
            ConstraintProto::CONSTRAINT_NOT_SET);
}

TEST(PresolveContextTest,
     CanonicalizeLinMaxLiteralAndNegationAfterSubstitution) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      enforcement_literal: [ 0, 1 ]
      lin_max {
        target {
          vars: [ 2 ]
          coeffs: [ 1 ]
        }
        exprs {
          vars: [ 3 ]
          coeffs: [ 1 ]
        }
      }
    }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  // Substitute literal 1 with not(0) (-1).
  EXPECT_EQ(context.StoreBooleanEqualityRelation(0, -2), kOk);
  EXPECT_EQ(kModified, context.CanonicalizeConstraint(0));
  EXPECT_EQ(context.Constraint(0).constraint_case(),
            ConstraintProto::CONSTRAINT_NOT_SET);
}

TEST(PresolveContextTest, CanonicalizeConstraintsLazilyAcrossFamilies) {
  Model model;
  // Variables:
  // 0..5: Booleans [0, 1]
  // 6..9: Integers [0, 10]
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    # c0: kLinear with unsorted vars, negative leading coeff, GCD 2, and fixed
    # var 9.
    constraints {
      linear {
        vars: [ 8, 7, 9 ]
        coeffs: [ 4, -2, 2 ]
        domain: [ 0, 20 ]
      }
    }
    # c1: kBoolOr with unsorted/duplicate literals and a false literal.
    constraints { bool_or { literals: [ 3, 1, 0, 1, 4 ] } }
    # c2: enforced kBoolAnd with redundant enforcement literal in body.
    constraints {
      enforcement_literal: [ 1 ]
      bool_and { literals: [ 0, 1 ] }
    }
    # c3: kAtMostOne (3 == 2 will create duplicate 2 -> fixes 2 = 0, leaving [0,
    # 1]).
    constraints { at_most_one { literals: [ 3, 2, 0, 1, 4 ] } }
    # c4: kExactlyOne (with 2 = 0 and 4 = 0, leaves [0, 1]).
    constraints { exactly_one { literals: [ 2, 0, 1, 4 ] } }
    # c5: kBoolXor with duplicate pair cancellation and parity normalization.
    constraints { bool_xor { literals: [ 3, -1, 3, 1, 5, 4 ] } }
    # c6: kLinMax with unsorted exprs.
    constraints {
      lin_max {
        target {
          vars: [ 8 ]
          coeffs: [ 1 ]
        }
        exprs {
          vars: [ 7, 9 ]
          coeffs: [ 2, 1 ]
          offset: 1
        }
        exprs {
          vars: [ 6 ]
          coeffs: [ 1 ]
          offset: 5
        }
      }
    }
    # c7: optional kInterval whose enforcement literal 4 will be fixed false.
    constraints {
      enforcement_literal: [ 4 ]
      interval {
        start {
          vars: [ 6 ]
          coeffs: [ 1 ]
        }
        size { offset: 3 }
        end {
          vars: [ 6 ]
          coeffs: [ 1 ]
          offset: 3
        }
      }
    }
    # c8: performed kInterval referencing affine variable 7.
    constraints {
      interval {
        start {
          vars: [ 7 ]
          coeffs: [ 1 ]
        }
        size {
          vars: [ 9 ]
          coeffs: [ 1 ]
          offset: 2
        }
        end {
          vars: [ 8 ]
          coeffs: [ 1 ]
        }
      }
    }
    # c9: kCumulative referencing interval 7 and 8.
    constraints {
      cumulative {
        intervals: [ 7, 8 ]
        demands {
          vars: [ 9 ]
          coeffs: [ 1 ]
          offset: 1
        }
        demands {
          vars: [ 7 ]
          coeffs: [ 2 ]
        }
        capacity {
          vars: [ 8 ]
          coeffs: [ 1 ]
          offset: 5
        }
      }
    }
    # c10: kTable with modern exprs.
    constraints {
      table {
        exprs {
          vars: [ 7, 9 ]
          coeffs: [ 1, 1 ]
        }
        exprs {
          vars: [ 8 ]
          coeffs: [ 1 ]
        }
        values: [ 5, 5, 6, 6 ]
      }
    }
    # c11: kRoutes whose dimension expression uses var 7 (not in arc literals!).
    constraints {
      routes {
        tails: [ 0, 1 ]
        heads: [ 1, 0 ]
        literals: [ 0, 1 ]
        dimensions {
          exprs {
            vars: [ 7, 9 ]
            coeffs: [ 1, 2 ]
            offset: 1
          }
          exprs {
            vars: [ 8 ]
            coeffs: [ 1 ]
          }
        }
      }
    }
  )pb");

  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  EXPECT_TRUE(context.NeedsCanonicalization());

  // Initial sync canonicalizes unsorted inputs.
  EXPECT_EQ(context.CanonicalizeConstraintsLazily(), kOk);
  EXPECT_FALSE(context.NeedsCanonicalization());
  EXPECT_TRUE(context.ModelProtoIsCanonical());
  EXPECT_TRUE(context.ConstraintVariableUsageIsConsistent());

  // Now mutate variable state:
  // - Fix var 4 = 0 (makes c7 unperformed interval stub, drops 4 from
  // c1/c3/c4).
  // - Fix var 5 = 1 (flips parity in c5).
  // - Fix var 9 = 3 (folds constant into c0, c6, c8, c9, c10, c11).
  // - Link var 7 = 2 * var 6 + 1 (wakes up c0, c6, c8, c9, c10, and c11!).
  // - Link literal 3 == literal 2 (wakes up c1, c2, c3, c4, c5).
  ASSERT_EQ(context.SetLiteralToFalse(4), kOk);
  ASSERT_EQ(context.SetLiteralToTrue(5), kOk);
  ASSERT_EQ(context.IntersectDomainWith(9, Domain(3)), kOk);
  ASSERT_EQ(context.StoreAffineRelation(7, 6, 2, 1), kOk);
  ASSERT_EQ(context.StoreBooleanEqualityRelation(3, 2), kOk);

  EXPECT_TRUE(context.NeedsCanonicalization());
  EXPECT_EQ(context.CanonicalizeConstraintsLazily(), kOk);
  EXPECT_FALSE(context.NeedsCanonicalization());
  EXPECT_TRUE(context.ModelProtoIsCanonical());

  // Verify unperformed kInterval stub (c7) kept kInterval active with cleared
  // start/size/end and a single false enforcement literal.
  const ConstraintProto& itv_stub = context.Constraint(7);
  EXPECT_EQ(itv_stub.constraint_case(), ConstraintProto::kInterval);
  ASSERT_EQ(itv_stub.enforcement_literal_size(), 1);
  EXPECT_TRUE(context.LiteralIsFalse(itv_stub.enforcement_literal(0)));
  EXPECT_FALSE(itv_stub.interval().has_start());
  EXPECT_FALSE(itv_stub.interval().has_size());
  EXPECT_FALSE(itv_stub.interval().has_end());

  // Verify kRoutes dimension expression (c11) substituted var 7 -> 2*var 6 + 1
  // and folded var 9 = 3 (offset = 1 + 1 + 2*3 = 8).
  const LinearExpressionProto& route_expr0 =
      context.Constraint(11).routes().dimensions(0).exprs(0);
  EXPECT_THAT(route_expr0.vars(), testing::ElementsAre(6));
  EXPECT_THAT(route_expr0.coeffs(), testing::ElementsAre(2));
  EXPECT_EQ(route_expr0.offset(), 8);

  // Verify kLinear (c0): originally 4*x8 - 2*x7 + 2*x9 in [0, 20].
  // With x7 = 2*x6 + 1 (and x6 <= 4 from domain propagation) and x9 = 3:
  // -4*x6 + 4*x8 + 4 in [0, 20] -> -4*x6 + 4*x8 in [-4, 16]
  // -> divide by GCD 4: -x6 + x8 in [-1, 4]
  // -> positive leading coeff: x6 - x8 in [-4, 1].
  const LinearConstraintProto& lin0 = context.Constraint(0).linear();
  EXPECT_THAT(lin0.vars(), testing::ElementsAre(6, 8));
  EXPECT_THAT(lin0.coeffs(), testing::ElementsAre(1, -1));
  EXPECT_THAT(lin0.domain(), testing::ElementsAre(-4, 1));

  // Graph consistency holds after lazy sync.
  EXPECT_TRUE(context.ConstraintVariableUsageIsConsistent());
}

TEST(PresolveContextTest, CanonicalizeConstraintsLazilyCascadingPropagation) {
  Model model;
  // Variables:
  // 0: a in [0, 1]
  // 1: b in [0, 1]
  // 2: x in [0, 10]
  // 3: y in [0, 10]
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    # c0: ~a \/ b (so fixing a = 1 forces b = 1).
    constraints { bool_or { literals: [ -1, 1 ] } }
    # c1: b => x in [4, 4] (once b = 1, un-enforced unary linear fixes x = 4!).
    constraints {
      enforcement_literal: [ 1 ]
      linear {
        vars: [ 2 ]
        coeffs: [ 1 ]
        domain: [ 4, 4 ]
      }
    }
    # c2: interval with start = x + y (once x = 4, start becomes y + 4).
    constraints {
      interval {
        start {
          vars: [ 2, 3 ]
          coeffs: [ 1, 1 ]
        }
        size { offset: 2 }
        end {
          vars: [ 3 ]
          coeffs: [ 1 ]
          offset: 6
        }
      }
    }
  )pb");

  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  EXPECT_EQ(context.CanonicalizeConstraintsLazily(), kOk);
  EXPECT_TRUE(context.ModelProtoIsCanonical());

  // Fix a = 1; a single CanonicalizeConstraintsLazily() call must cascade
  // a=1 -> b=1 -> x=4 -> canonicalize interval c2.
  ASSERT_EQ(context.SetLiteralToTrue(0), kOk);
  EXPECT_EQ(context.CanonicalizeConstraintsLazily(), kOk);

  EXPECT_TRUE(context.LiteralIsTrue(1));
  EXPECT_TRUE(context.IsFixed(2));
  EXPECT_EQ(context.FixedValue(2), 4);
  EXPECT_EQ(context.Constraint(0).constraint_case(),
            ConstraintProto::CONSTRAINT_NOT_SET);
  EXPECT_EQ(context.Constraint(1).constraint_case(),
            ConstraintProto::CONSTRAINT_NOT_SET);
  EXPECT_THAT(context.Constraint(2).interval().start().vars(),
              testing::ElementsAre(3));
  EXPECT_EQ(context.Constraint(2).interval().start().offset(), 4);
  EXPECT_TRUE(context.ModelProtoIsCanonical());
}

TEST(PresolveContextTest, CanonicalizeConstraintsLazilyArenaAndIdempotence) {
  google::protobuf::Arena arena;
  CpModelProto* working_model =
      google::protobuf::Arena::Create<CpModelProto>(&arena);
  *working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints { bool_or { literals: [ 1, 0 ] } }
    constraints {
      linear {
        vars: [ 3, 2 ]
        coeffs: [ 2, 2 ]
        domain: [ 2, 12 ]
      }
    }
  )pb");

  Model model;
  PresolveContext context(&model, working_model, nullptr);

  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  EXPECT_EQ(context.CanonicalizeConstraintsLazily(), kOk);
  EXPECT_FALSE(context.NeedsCanonicalization());
  EXPECT_TRUE(context.ModelProtoIsCanonical());

  const uint64_t space_after_first_sync = arena.SpaceUsed();

  // Re-running CanonicalizeConstraint on already-canonical constraints is a
  // pure read-only no-op returning false.
  for (int c = 0; c < context.NumConstraints(); ++c) {
    EXPECT_EQ(kUnchanged, context.CanonicalizeConstraint(c));
  }

  for (int iter = 0; iter < 10; ++iter) {
    EXPECT_EQ(context.CanonicalizeConstraintsLazily(), kOk);
  }
  EXPECT_EQ(arena.SpaceUsed(), space_after_first_sync);

  // Force a full detach -> CanonicalizeConstraint -> reattach Swap cycle on
  // both constraints and verify zero Arena allocation (O(1) pointer swap).
  context.MarkConstraintForCanonicalization(0);
  context.MarkConstraintForCanonicalization(1);
  EXPECT_TRUE(context.NeedsCanonicalization());
  EXPECT_EQ(context.CanonicalizeConstraintsLazily(), kOk);
  EXPECT_FALSE(context.NeedsCanonicalization());
  EXPECT_EQ(arena.SpaceUsed(), space_after_first_sync);

  // Test AddConstraint + RemoveLastConstraint watermark handling.
  ConstraintProto* temp = context.AddConstraint();
  temp->mutable_bool_or()->add_literals(0);
  EXPECT_TRUE(context.NeedsCanonicalization());
  context.RemoveLastConstraint();
  EXPECT_FALSE(context.NeedsCanonicalization());

  // Non-fixing bound tightening does not invalidate lazy canonicality.
  ASSERT_EQ(context.IntersectDomainWith(2, Domain(0, 2)), kOk);
  ASSERT_EQ(context.IntersectDomainWith(3, Domain(0, 2)), kOk);
  EXPECT_FALSE(context.NeedsCanonicalization());
  EXPECT_TRUE(context.ModelProtoIsCanonical());
}

TEST(PresolveContextTest,
     CanonicalizeConstraintsLazilyRemovesSubstitutedVarWithFixedRep) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      linear {
        vars: [ 1, 2, 3, 4 ]
        coeffs: [ 1, 1, 1, 1 ]
        domain: [ 1, 3 ]
      }
    }
    constraints { at_most_one { literals: [ 1, 2, 3 ] } }
  )pb");
  CpModelProto mapping_model;
  PresolveContext context(&model, &working_model, &mapping_model);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  EXPECT_EQ(context.CanonicalizeConstraintsLazily(), kOk);
  ASSERT_TRUE(context.ModelProtoIsCanonical());

  // Map var 4 -> var 0 while both are unfixed, then fix var 0 (the
  // representative) before CanonicalizeConstraintsLazily() runs. Note that
  // var 4's own domain in domains_ remains [0, 1] because
  // IntersectDomainWith(0, ...) does not eagerly push down to
  // non-representative variables.
  ASSERT_EQ(context.StoreBooleanEqualityRelation(4, 0), kOk);
  ASSERT_EQ(context.SetLiteralToTrue(0), kOk);
  EXPECT_FALSE(context.IsFixed(4));
  EXPECT_TRUE(context.IsFixed(0));

  EXPECT_EQ(context.CanonicalizeConstraintsLazily(), kOk);
  EXPECT_TRUE(context.ModelProtoIsCanonical());
  EXPECT_THAT(working_model.constraints(0).linear(), testing::EqualsProto(R"pb(
                vars: [ 1, 2, 3 ]
                coeffs: [ 1, 1, 1 ]
                domain: [ 0, 2 ]
              )pb"));
}

TEST(PresolveContextTest,
     CanonicalizeLinearOfSizeOneSimplifiesHolesUsingVarDomain) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 0, 2, 2, 4, 4, 6, 6, 8, 8 ] }
    constraints {
      enforcement_literal: [ 0 ]
      linear {
        vars: [ 1 ]
        coeffs: [ 1 ]
        domain: [ 0, 0, 4, 4, 6, 6, 8, 8 ]
      }
    }
  )pb");
  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);

  EXPECT_EQ(context.CanonicalizeConstraintsLazily(), kOk);
  EXPECT_TRUE(context.ModelProtoIsCanonical());
  EXPECT_THAT(working_model.constraints(0).linear(), testing::EqualsProto(R"pb(
                vars: [ 1 ]
                coeffs: [ 1 ]
                domain: [ 0, 0, 4, 8 ]
              )pb"));

  // Re-canonicalizing is a no-op and preserves the simplified 2-interval RHS.
  EXPECT_EQ(kUnchanged, context.CanonicalizeConstraint(0));
  EXPECT_THAT(working_model.constraints(0).linear().domain(),
              testing::ElementsAre(0, 0, 4, 8));
}

TEST(PresolveContextTest,
     CanonicalizeConstraintsLazilyEnforcedExprsAndInverse) {
  Model model;
  // Variables:
  // 0: b in [0, 1] (enforcement literal)
  // 1: x in [0, 10] (affine representative)
  // 2: y in [0, 25] (will get affine relation y = 2 * x + 1)
  // 3: z in [0, 10] (will get fixed z = 3)
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 25 ] }
    variables { domain: [ 0, 10 ] }
    # c0: fixed interval [0, 2] referenced by c3.
    constraints {
      interval {
        start { offset: 0 }
        size { offset: 2 }
        end { offset: 2 }
      }
    }
    # c1: enforced kAllDiff (b => all_diff(y + z + b, x)).
    constraints {
      enforcement_literal: [ 0 ]
      all_diff {
        exprs {
          vars: [ 2, 3, 0 ]
          coeffs: [ 1, 1, 1 ]
        }
        exprs {
          vars: [ 1 ]
          coeffs: [ 1 ]
        }
      }
    }
    # c2: enforced kAutomaton (~b => automaton(y - z + 2*b)).
    constraints {
      enforcement_literal: [ -1 ]
      automaton {
        starting_state: 0
        final_states: [ 1 ]
        transition_tail: [ 0 ]
        transition_head: [ 1 ]
        transition_label: [ 0 ]
        exprs {
          vars: [ 2, 3, 0 ]
          coeffs: [ 1, -1, 2 ]
        }
      }
    }
    # c3: enforced kCumulative (b => cumulative).
    constraints {
      enforcement_literal: [ 0 ]
      cumulative {
        intervals: [ 0 ]
        demands {
          vars: [ 2, 0 ]
          coeffs: [ 1, 3 ]
        }
        capacity {
          vars: [ 3, 1 ]
          coeffs: [ 2, 1 ]
        }
      }
    }
    # c4: enforced kReservoir (b => reservoir).
    constraints {
      enforcement_literal: [ 0 ]
      reservoir {
        min_level: 0
        max_level: 20
        time_exprs {
          vars: [ 2, 3 ]
          coeffs: [ 1, 1 ]
        }
        level_changes {
          vars: [ 0, 1 ]
          coeffs: [ 2, 1 ]
        }
        active_literals: [ 0 ]
      }
    }
    # c5: kInverse with affine and fixed expressions.
    constraints {
      inverse {
        f_expr_direct {
          vars: [ 2 ]
          coeffs: [ 1 ]
        }
        f_expr_direct {
          vars: [ 1 ]
          coeffs: [ 1 ]
        }
        f_expr_inverse {
          vars: [ 3 ]
          coeffs: [ 1 ]
        }
        f_expr_inverse {
          vars: [ 1 ]
          coeffs: [ 1 ]
        }
      }
    }
  )pb");

  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  EXPECT_EQ(context.CanonicalizeConstraintsLazily(), kOk);
  EXPECT_TRUE(context.ModelProtoIsCanonical());

  // Fix z = 3 and link y = 2 * x + 1.
  ASSERT_EQ(context.IntersectDomainWith(3, Domain(3)), kOk);
  ASSERT_EQ(context.StoreAffineRelation(2, 1, 2, 1), kOk);
  EXPECT_TRUE(context.NeedsCanonicalization());

  EXPECT_EQ(context.CanonicalizeConstraintsLazily(), kOk);
  EXPECT_FALSE(context.NeedsCanonicalization());
  EXPECT_TRUE(context.ModelProtoIsCanonical());

  // c1 (b = 1): y + z + b = (2*x + 1) + 3 + 1 = 2*x + 5, sorted after x.
  EXPECT_THAT(context.Constraint(1).all_diff().exprs(0),
              testing::EqualsProto(R"pb(
                vars: [ 1 ]
                coeffs: [ 1 ]
              )pb"));
  EXPECT_THAT(context.Constraint(1).all_diff().exprs(1),
              testing::EqualsProto(R"pb(
                vars: [ 1 ]
                coeffs: [ 2 ]
                offset: 5
              )pb"));
  // c2 (b = 0): y - z + 2*b = (2*x + 1) - 3 + 0 = 2*x - 2.
  EXPECT_THAT(context.Constraint(2).automaton().exprs(0),
              testing::EqualsProto(R"pb(
                vars: [ 1 ]
                coeffs: [ 2 ]
                offset: -2
              )pb"));
  // c3 (b = 1): demand = (2*x + 1) + 3 = 2*x + 4; capacity = 2*3 + x = x + 6.
  EXPECT_THAT(context.Constraint(3).cumulative().demands(0),
              testing::EqualsProto(R"pb(
                vars: [ 1 ]
                coeffs: [ 2 ]
                offset: 4
              )pb"));
  EXPECT_THAT(context.Constraint(3).cumulative().capacity(),
              testing::EqualsProto(R"pb(
                vars: [ 1 ]
                coeffs: [ 1 ]
                offset: 6
              )pb"));
  // c4 (b = 1): time = (2*x + 1) + 3 = 2*x + 4; level_change = 2 + x = x + 2;
  // active_literals[0] = b = true.
  EXPECT_THAT(context.Constraint(4).reservoir().time_exprs(0),
              testing::EqualsProto(R"pb(
                vars: [ 1 ]
                coeffs: [ 2 ]
                offset: 4
              )pb"));
  EXPECT_THAT(context.Constraint(4).reservoir().level_changes(0),
              testing::EqualsProto(R"pb(
                vars: [ 1 ]
                coeffs: [ 1 ]
                offset: 2
              )pb"));
  EXPECT_THAT(context.Constraint(4).reservoir().active_literals(),
              testing::ElementsAre(context.GetTrueLiteral()));
  // c5: f_expr_direct[0] = 2*x + 1; f_expr_inverse[0] = 3.
  EXPECT_THAT(context.Constraint(5).inverse().f_expr_direct(0),
              testing::EqualsProto(R"pb(
                vars: [ 1 ]
                coeffs: [ 2 ]
                offset: 1
              )pb"));
  EXPECT_THAT(context.Constraint(5).inverse().f_expr_inverse(0),
              testing::EqualsProto(R"pb(offset: 3)pb"));

  EXPECT_TRUE(context.ConstraintVariableUsageIsConsistent());
}

TEST(PresolveContextTest,
     CanonicalizesSortedLiteralsDuplicatesAndOppositePairsWithoutHashSet) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    # c0: enforcement [2, 0, -3] contains 2 and ~2 -> never enforced (removed).
    constraints {
      enforcement_literal: [ 2, 0, -3 ]
      linear {
        vars: [ 5 ]
        coeffs: [ 1 ]
        domain: [ 0, 5 ]
      }
    }
    # c1: enforcement [2, 0, 2] -> deduplicated and sorted to [0, 2].
    constraints {
      enforcement_literal: [ 2, 0, 2 ]
      linear {
        vars: [ 5 ]
        coeffs: [ 1 ]
        domain: [ 1, 5 ]
      }
    }
    # c2: kBoolOr [1, 2, -2, 1] contains 1 and ~1 -> tautology (removed).
    constraints { bool_or { literals: [ 1, 2, -2, 1 ] } }
    # c3: kBoolAnd [2, 0] => [3, -4, 3] contains 3 and ~3 -> negated
    # enforcement clause [-3, -1].
    constraints {
      enforcement_literal: [ 2, 0 ]
      bool_and { literals: [ 3, -4, 3 ] }
    }
    # c4: kBoolAnd [2, 1] => [3, -2] has ~1 vs enforcement 1 ("x => not x") ->
    # negated enforcement clause [-3, -2].
    constraints {
      enforcement_literal: [ 2, 1 ]
      bool_and { literals: [ 3, -2 ] }
    }
    # c5: kBoolAnd [2, 0] => [3, 2, 3, 1] drops redundant 2 ("x => x") and
    # duplicate 3, sorting RHS to [1, 3].
    constraints {
      enforcement_literal: [ 2, 0 ]
      bool_and { literals: [ 3, 2, 3, 1 ] }
    }
    # c6: kAtMostOne [4, 3, -4, -4] has 3 and ~3 with duplicate ~3 -> fixes
    # var 4 = 0 and var 3 = 1, then removes c6 (which also wakes c3, c4, c5!).
    constraints { at_most_one { literals: [ 4, 3, -4, -4 ] } }
  )pb");

  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  EXPECT_EQ(context.CanonicalizeConstraintsLazily(), kOk);

  EXPECT_FALSE(context.NeedsCanonicalization());
  EXPECT_TRUE(context.ModelProtoIsCanonical());
  EXPECT_TRUE(context.ConstraintVariableUsageIsConsistent());

  EXPECT_EQ(context.Constraint(0).constraint_case(),
            ConstraintProto::CONSTRAINT_NOT_SET);
  EXPECT_THAT(context.Constraint(1).enforcement_literal(),
              testing::ElementsAre(0, 2));
  EXPECT_EQ(context.Constraint(2).constraint_case(),
            ConstraintProto::CONSTRAINT_NOT_SET);
  EXPECT_THAT(context.Constraint(3), testing::EqualsProto(R"pb(
                bool_or { literals: [ -3, -1 ] }
              )pb"));
  EXPECT_THAT(context.Constraint(4), testing::EqualsProto(R"pb(
                bool_or { literals: [ -3, -2 ] }
              )pb"));
  // In c5, var 3 was fixed to 1 by c6, leaving [0, 2] => [1].
  EXPECT_THAT(context.Constraint(5), testing::EqualsProto(R"pb(
                enforcement_literal: [ 0, 2 ]
                bool_and { literals: [ 1 ] }
              )pb"));
  EXPECT_EQ(context.Constraint(6).constraint_case(),
            ConstraintProto::CONSTRAINT_NOT_SET);
  EXPECT_TRUE(context.LiteralIsTrue(3));
  EXPECT_TRUE(context.LiteralIsFalse(4));
}

TEST(PresolveContextTest,
     CanonicalizeBoolXorRemovesEnforcementLiteralsAndRewritesUnaryToBoolOr) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    # c0: [2] => xor(2, -1, 1, 3): 2 is true (flips parity) and -1 (~v0) flips
    # parity back, leaving [2] => xor(0, 1, 3).
    constraints {
      enforcement_literal: [ 2 ]
      bool_xor { literals: [ 2, -1, 1, 3 ] }
    }
    # c1: [2, -5] => xor(-3, -5, 0, 1): -3 (~v2) is false under [2], -5 (~v4)
    # is true under [-5] (flips parity), leaving [-5, 2] => xor(-1, 1).
    constraints {
      enforcement_literal: [ 2, -5 ]
      bool_xor { literals: [ -3, -5, 0, 1 ] }
    }
    # c2: [2] => xor(0) (unary enforced) -> bool_or([-3, 0]).
    constraints {
      enforcement_literal: [ 2 ]
      bool_xor { literals: [ 0 ] }
    }
    # c3: [2, 3] => xor(2, 1) -> 2 is true (flips parity), leaving unary
    # [2, 3] => xor(-2) -> bool_or([-4, -3, -2]).
    constraints {
      enforcement_literal: [ 2, 3 ]
      bool_xor { literals: [ 2, 1 ] }
    }
    # c4: [2] => xor(2) -> 2 is true, leaving [2] => (0 == 0) (always true) ->
    # removed.
    constraints {
      enforcement_literal: [ 2 ]
      bool_xor { literals: [ 2 ] }
    }
    # c5: [2, 3] => xor(-3): -3 (~v2) is false under [2], leaving [2, 3] =>
    # (0 == 1) (always false) -> bool_or([-4, -3]).
    constraints {
      enforcement_literal: [ 2, 3 ]
      bool_xor { literals: [ -3 ] }
    }
  )pb");

  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);

  // Before canonicalization, unary enforced kBoolXor and kBoolXor containing
  // enforcement variables are non-canonical.
  EXPECT_FALSE(context.ConstraintIsCanonical(0));
  EXPECT_FALSE(context.ConstraintIsCanonical(2));

  EXPECT_EQ(context.CanonicalizeConstraintsLazily(), kOk);

  EXPECT_FALSE(context.NeedsCanonicalization());
  EXPECT_TRUE(context.ModelProtoIsCanonical());
  EXPECT_TRUE(context.ConstraintVariableUsageIsConsistent());

  EXPECT_THAT(context.Constraint(0), testing::EqualsProto(R"pb(
                enforcement_literal: [ 2 ]
                bool_xor { literals: [ 0, 1, 3 ] }
              )pb"));
  EXPECT_THAT(context.Constraint(1), testing::EqualsProto(R"pb(
                enforcement_literal: [ -5, 2 ]
                bool_xor { literals: [ -1, 1 ] }
              )pb"));
  EXPECT_THAT(context.Constraint(2), testing::EqualsProto(R"pb(
                bool_or { literals: [ -3, 0 ] }
              )pb"));
  EXPECT_THAT(context.Constraint(3), testing::EqualsProto(R"pb(
                bool_or { literals: [ -4, -3, -2 ] }
              )pb"));
  EXPECT_EQ(context.Constraint(4).constraint_case(),
            ConstraintProto::CONSTRAINT_NOT_SET);
  EXPECT_THAT(context.Constraint(5), testing::EqualsProto(R"pb(
                bool_or { literals: [ -4, -3 ] }
              )pb"));
}

TEST(PresolveContextTest, CheckSortedParameterInConstraintIsCanonical) {
  CpModelProto permuted_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      enforcement_literal: [ 2, -1 ]
      linear {
        vars: [ 4, 3 ]
        coeffs: [ 1, -2 ]
        domain: [ -10, 5 ]
      }
    }
    constraints { bool_or { literals: [ 2, -1, 1 ] } }
    constraints { bool_xor { literals: [ -3, 0, 1 ] } }
    constraints {
      lin_max {
        target {
          vars: [ 3 ]
          coeffs: [ 1 ]
        }
        exprs {
          vars: [ 4, 3 ]
          coeffs: [ 1, 1 ]
          offset: 2
        }
        exprs {
          vars: [ 3 ]
          coeffs: [ 1 ]
          offset: 1
        }
      }
    }
  )pb");
  Model model;
  PresolveContext context(&model, &permuted_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  for (int c = 0; c < context.NumConstraints(); ++c) {
    EXPECT_FALSE(context.ConstraintIsCanonical(c, /*check_sorted=*/true));
  }
  EXPECT_FALSE(context.ModelProtoIsCanonical(/*check_sorted=*/true));
  EXPECT_TRUE(context.ModelProtoIsCanonical(/*check_sorted=*/false));
  EXPECT_TRUE(ModelProtoIsCanonical(permuted_model));
}

TEST(PresolveContextTest,
     CanonicalizeEnforcedCircuitRoutesAndReservoirLiterals) {
  Model model;
  // Variables:
  // 0, 1: enforcement booleans e0, e1 in [0, 1]
  // 2: arc/active boolean a in [0, 1] (will later be linked a == ~e1)
  // 3: uninvolved boolean b in [0, 1]
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    # c0: single-enforced sparse kCircuit (~e0 => circuit on nodes -10 and 5).
    # Arc -10->5 has literal ~e0 (true), 5->-10 has literal b, and self-loop
    # -10->-10 has literal e0 (false -> removed).
    constraints {
      enforcement_literal: [ -1 ]
      circuit {
        tails: [ -10, -10, 5 ]
        heads: [ 5, -10, -10 ]
        literals: [ -1, 0, 3 ]
      }
    }
    # c1: multi-enforced kRoutes ((e0 /\ e1) => routes).
    # Arc 0->1 has literal e0 (true), 1->0 has literal b, and self-loops 1->1
    # have literal ~e1 (false -> removed) on node 1? No: keep 1->1 as `a` and a
    # parallel/extra arc 0->1 with `~e1` (false -> removed). When `a` later
    # maps to `~e1`, 1->1 is also removed while node 1 still has 0->1 and 1->0.
    constraints {
      enforcement_literal: [ 0, 1 ]
      routes {
        tails: [ 0, 0, 1, 1 ]
        heads: [ 1, 1, 0, 1 ]
        literals: [ 0, -2, 3, 2 ]
      }
    }
    # c2: multi-enforced kReservoir ((e0 /\ e1) => reservoir).
    # Event 0 has active `e1` (replaced by true_lit), event 1 has `~e0`
    # (unperformed -> removed), event 2 has `a`, event 3 has `b`.
    constraints {
      enforcement_literal: [ 0, 1 ]
      reservoir {
        min_level: -5
        max_level: 10
        time_exprs { offset: 0 }
        time_exprs { offset: 1 }
        time_exprs { offset: 2 }
        time_exprs { offset: 3 }
        level_changes { offset: 1 }
        level_changes { offset: 9 }
        level_changes { offset: 2 }
        level_changes { offset: -3 }
        active_literals: [ 1, -1, 2, 3 ]
      }
    }
  )pb");

  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  EXPECT_FALSE(context.ConstraintIsCanonical(0));
  EXPECT_FALSE(context.ConstraintIsCanonical(1));
  EXPECT_FALSE(context.ConstraintIsCanonical(2));

  EXPECT_EQ(context.CanonicalizeConstraintsLazily(), kOk);
  EXPECT_TRUE(context.ModelProtoIsCanonical());
  EXPECT_TRUE(context.ConstraintVariableUsageIsConsistent());

  const int true_lit = context.GetTrueLiteral();
  EXPECT_THAT(context.Constraint(0).circuit().tails(),
              testing::ElementsAre(-10, 5));
  EXPECT_THAT(context.Constraint(0).circuit().heads(),
              testing::ElementsAre(5, -10));
  EXPECT_THAT(context.Constraint(0).circuit().literals(),
              testing::ElementsAre(true_lit, 3));

  EXPECT_THAT(context.Constraint(1).routes().tails(),
              testing::ElementsAre(0, 1, 1));
  EXPECT_THAT(context.Constraint(1).routes().heads(),
              testing::ElementsAre(1, 0, 1));
  EXPECT_THAT(context.Constraint(1).routes().literals(),
              testing::ElementsAre(true_lit, 3, 2));

  EXPECT_THAT(context.Constraint(2).reservoir().active_literals(),
              testing::ElementsAre(true_lit, 2, 3));
  ASSERT_EQ(context.Constraint(2).reservoir().level_changes_size(), 3);
  EXPECT_EQ(context.Constraint(2).reservoir().level_changes(0).offset(), 1);
  EXPECT_EQ(context.Constraint(2).reservoir().level_changes(1).offset(), 2);
  EXPECT_EQ(context.Constraint(2).reservoir().level_changes(2).offset(), -3);

  // Link `var 2 == ~var 1` (`2 -> -2`), mapping literal `2` onto `~e1` in c1
  // and c2; lazy re-canonicalization must remove the newly false arc/event.
  ASSERT_EQ(context.StoreBooleanEqualityRelation(2, -2), kOk);
  EXPECT_TRUE(context.NeedsCanonicalization());
  EXPECT_EQ(context.CanonicalizeConstraintsLazily(), kOk);
  EXPECT_TRUE(context.ModelProtoIsCanonical());
  EXPECT_TRUE(context.ConstraintVariableUsageIsConsistent());

  EXPECT_THAT(context.Constraint(1).routes().tails(),
              testing::ElementsAre(0, 1));
  EXPECT_THAT(context.Constraint(1).routes().heads(),
              testing::ElementsAre(1, 0));
  EXPECT_THAT(context.Constraint(1).routes().literals(),
              testing::ElementsAre(true_lit, 3));

  EXPECT_THAT(context.Constraint(2).reservoir().active_literals(),
              testing::ElementsAre(true_lit, 3));
  ASSERT_EQ(context.Constraint(2).reservoir().level_changes_size(), 2);
  EXPECT_EQ(context.Constraint(2).reservoir().level_changes(0).offset(), 1);
  EXPECT_EQ(context.Constraint(2).reservoir().level_changes(1).offset(), -3);
}

TEST(PresolveContextTest,
     CanonicalizeCircuitRoutesAndReservoirInfeasibleAndEmptyDepotCases) {
  Model model;
  // Variables:
  // 0: e0 in [0, 1]
  // 1: e1 in [0, 1]
  // 2: e2 in [0, 1]
  // 3: a in [0, 1]
  // 4: b in [0, 1]
  // 5: false_var in [0, 0]
  // 6: true_var in [1, 1]
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 0 ] }
    variables { domain: [ 1, 1 ] }
    # c0: enforced kCircuit where removing false arc 1->0 leaves node 0 with
    # in_degree == 0 -> forces e0 = 0 and clears c0.
    constraints {
      enforcement_literal: [ 0 ]
      circuit {
        tails: [ 0, 1 ]
        heads: [ 1, 0 ]
        literals: [ 3, 5 ]
      }
    }
    # c1: enforced kRoutes with two true outgoing arcs from non-depot node 1
    # -> forces e1 = 0 and clears c1.
    constraints {
      enforcement_literal: [ 1 ]
      routes {
        tails: [ 0, 1, 1 ]
        heads: [ 1, 0, 1 ]
        literals: [ 3, 6, 1 ]
      }
    }
    # c2: enforced kRoutes where depot incoming arc 1->0 is false (`5`), while
    # 0->1 has literal `a` (`3`) and self-loop 1->1 has literal `b` (`4`)
    # -> rewritten into `e2 => (~a /\ b)` (`kBoolAnd` with `[-4, 4]`).
    constraints {
      enforcement_literal: [ 2 ]
      routes {
        tails: [ 0, 1, 1 ]
        heads: [ 1, 0, 1 ]
        literals: [ 3, 5, 4 ]
      }
    }
    # c3: kReservoir whose events are all inactive (`5`) -> trivially feasible
    # and removed.
    constraints {
      reservoir {
        min_level: 0
        max_level: 5
        time_exprs { offset: 0 }
        level_changes { offset: 10 }
        active_literals: [ 5 ]
      }
    }
  )pb");

  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  EXPECT_EQ(context.CanonicalizeConstraintsLazily(), kOk);
  EXPECT_TRUE(context.ModelProtoIsCanonical());
  EXPECT_TRUE(context.ConstraintVariableUsageIsConsistent());

  EXPECT_TRUE(context.LiteralIsFalse(0));
  EXPECT_TRUE(context.LiteralIsFalse(1));
  EXPECT_EQ(context.Constraint(0).constraint_case(),
            ConstraintProto::CONSTRAINT_NOT_SET);
  EXPECT_EQ(context.Constraint(1).constraint_case(),
            ConstraintProto::CONSTRAINT_NOT_SET);
  EXPECT_THAT(context.Constraint(2), testing::EqualsProto(R"pb(
                enforcement_literal: [ 2 ]
                bool_and { literals: [ -4, 4 ] }
              )pb"));
  EXPECT_EQ(context.Constraint(3).constraint_case(),
            ConstraintProto::CONSTRAINT_NOT_SET);
}

TEST(PresolveContextTest,
     CanonicalizeByIndexAndClearUpdateGraphWithoutDirtyingCanonicalization) {
  Model model;
  CpModelProto working_model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_or { literals: [ 0, 1, 2 ] } }
    constraints { at_most_one { literals: [ 1, 2, 3 ] } }
  )pb");

  PresolveContext context(&model, &working_model, nullptr);
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  EXPECT_EQ(context.CanonicalizeConstraintsLazily(), kOk);
  ASSERT_FALSE(context.NeedsCanonicalization());

  // Mutating c0 into a smaller already-canonical clause and calling
  // CanonicalizeModifiedConstraint(0) updates VarToConstraints without marking
  // c0 for re-canonicalization.
  context.MutableConstraint(0)->mutable_bool_or()->mutable_literals()->Truncate(
      2);
  EXPECT_EQ(context.CanonicalizeModifiedConstraint(0), kOk);
  EXPECT_FALSE(context.NeedsCanonicalization());
  EXPECT_THAT(context.VarToConstraints(2), testing::ElementsAre(1));

  // Clearing c1 updates VarToConstraints without marking c1 for
  // canonicalization (even if MarkConstraintForCanonicalization is also called
  // on the cleared constraint).
  context.ClearConstraint(1);
  context.MarkConstraintForCanonicalization(1);
  EXPECT_FALSE(context.NeedsCanonicalization());
  EXPECT_THAT(context.VarToConstraints(2), testing::IsEmpty());
  EXPECT_TRUE(context.ConstraintVariableUsageIsConsistent());

  // MarkConstraintForCanonicalization on a non-empty mutated constraint marks
  // BOTH the variable graph and lazy canonicalization.
  context.MutableConstraint(0)->mutable_bool_or()->add_literals(3);
  context.MutableConstraint(0)->mutable_bool_or()->add_literals(2);
  context.MarkConstraintForCanonicalization(0);
  EXPECT_TRUE(context.NeedsCanonicalization());
  EXPECT_THAT(context.VarToConstraints(3), testing::ElementsAre(0));

  // CanonicalizeConstraint(int c) normalizes c0 in-place and updates the graph
  // without leaving NeedsCanonicalization() set when no other dirty state
  // exists. Flush first, then test AddConstraint() + CanonicalizeConstraint.
  EXPECT_EQ(context.CanonicalizeConstraintsLazily(), kOk);
  ASSERT_FALSE(context.NeedsCanonicalization());

  // Appending a single constraint and immediately calling
  // CanonicalizeConstraint advances the watermark without scheduling redundant
  // lazy canonicalization.
  ConstraintProto* c2 = context.AddConstraint();
  c2->mutable_bool_or()->add_literals(3);
  c2->mutable_bool_or()->add_literals(1);
  c2->mutable_bool_or()->add_literals(1);
  EXPECT_TRUE(context.NeedsCanonicalization());
  EXPECT_EQ(kModified, context.CanonicalizeConstraint(2));
  EXPECT_FALSE(context.NeedsCanonicalization());
  EXPECT_THAT(context.Constraint(2), testing::EqualsProto(R"pb(
                bool_or { literals: [ 1, 3 ] }
              )pb"));
  EXPECT_THAT(context.VarToConstraints(3), testing::UnorderedElementsAre(0, 2));

  // Appending an uncanonicalized c3 followed by an eagerly canonicalized c4
  // still leaves c3 marked for lazy canonicalization while c4 is canonical, and
  // RemoveLastConstraint() on a speculative c5 keeps state consistent.
  ConstraintProto* c3 = context.AddConstraint();
  c3->mutable_bool_or()->add_literals(2);
  c3->mutable_bool_or()->add_literals(0);
  ConstraintProto* c4 = context.AddConstraint();
  c4->mutable_at_most_one()->add_literals(3);
  c4->mutable_at_most_one()->add_literals(1);
  EXPECT_EQ(kModified, context.CanonicalizeConstraint(4));
  ConstraintProto* c5 = context.AddConstraint();
  c5->mutable_bool_or()->add_literals(0);
  c5->mutable_bool_or()->add_literals(1);
  EXPECT_EQ(context.CanonicalizeConstraint(5), kUnchanged);
  context.RemoveLastConstraint();

  EXPECT_TRUE(context.NeedsCanonicalization());
  EXPECT_EQ(context.CanonicalizeConstraintsLazily(), kOk);
  EXPECT_FALSE(context.NeedsCanonicalization());
  EXPECT_THAT(context.Constraint(3), testing::EqualsProto(R"pb(
                bool_or { literals: [ 0, 2 ] }
              )pb"));
  EXPECT_TRUE(context.ModelProtoIsCanonical());
  EXPECT_TRUE(context.ConstraintVariableUsageIsConsistent());
}

TEST(PresolveContextTest, AddNeverSeenBeforeImplication) {
  Model model;
  CpModelProto working_model;
  PresolveContext context(&model, &working_model, nullptr);
  const int a = context.NewBoolVar("a");
  const int b = context.NewBoolVar("b");

  // First time seeing a => b adds the constraint and returns true.
  EXPECT_TRUE(context.AddNeverSeenBeforeImplication(a, b));
  ASSERT_EQ(context.NumConstraints(), 1);
  EXPECT_THAT(context.Constraint(0), testing::EqualsProto(R"pb(
                enforcement_literal: 0
                bool_and { literals: 1 }
              )pb"));

  // Exact repeat and contrapositive (~b => ~a) are both rejected.
  EXPECT_FALSE(context.AddNeverSeenBeforeImplication(a, b));
  EXPECT_FALSE(
      context.AddNeverSeenBeforeImplication(NegatedRef(b), NegatedRef(a)));
  EXPECT_EQ(context.NumConstraints(), 1);

  // Converse implication b => a is distinct and accepted once.
  EXPECT_TRUE(context.AddNeverSeenBeforeImplication(b, a));
  EXPECT_FALSE(
      context.AddNeverSeenBeforeImplication(NegatedRef(a), NegatedRef(b)));
  EXPECT_EQ(context.NumConstraints(), 2);

  // ResetAfterCopy() clears the seen set so remapped models start fresh.
  context.ResetAfterCopy();
  EXPECT_EQ(context.InitializeNewDomains(), kOk);
  EXPECT_TRUE(context.AddNeverSeenBeforeImplication(a, b));
  EXPECT_EQ(context.NumConstraints(), 3);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
