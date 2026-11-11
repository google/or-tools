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

#include "ortools/sat/cp_model_utils.h"

#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_format.h"
#include "google/protobuf/descriptor.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/base/stl_util.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/cp_model.pb.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::UnorderedElementsAre;

TEST(LinearExpressionGcdTest, Empty) {
  LinearExpressionProto expr;
  EXPECT_EQ(LinearExpressionGcd(expr), 0);
}

TEST(LinearExpressionGcdTest, BasicCases1) {
  LinearExpressionProto expr;
  expr.set_offset(2);
  expr.add_coeffs(4);
  EXPECT_EQ(LinearExpressionGcd(expr), 2);
}

TEST(LinearExpressionGcdTest, BasicCases2) {
  LinearExpressionProto expr;
  expr.set_offset(5);
  expr.add_coeffs(10);
  EXPECT_EQ(LinearExpressionGcd(expr), 5);
}

TEST(LinearExpressionGcdTest, BasicCases3) {
  LinearExpressionProto expr;
  expr.set_offset(9);
  expr.add_coeffs(12);
  expr.add_coeffs(24);
  EXPECT_EQ(LinearExpressionGcd(expr), 3);
}

TEST(AddReferencesUsedByConstraintTest, Literals) {
  ConstraintProto ct;
  ct.add_enforcement_literal(1);
  auto* arg = ct.mutable_bool_and();
  arg->add_literals(-10);
  arg->add_literals(+10);
  arg->add_literals(-20);
  const IndexReferences references = GetReferencesUsedByConstraint(ct);
  EXPECT_THAT(references.literals, UnorderedElementsAre(-10, +10, -20));
}

TEST(AddReferencesUsedByConstraintTest, IntegerConstraint) {
  ConstraintProto ct;
  auto* arg = ct.mutable_int_prod();
  arg->mutable_target()->add_vars(7);
  arg->mutable_target()->add_coeffs(2);
  auto* term = arg->add_exprs();
  term->add_vars(8);
  term->add_coeffs(2);
  term = arg->add_exprs();
  term->add_vars(8);
  term->add_coeffs(3);
  term = arg->add_exprs();
  term->add_vars(10);
  term->add_coeffs(-2);
  const IndexReferences references = GetReferencesUsedByConstraint(ct);
  EXPECT_THAT(references.variables, UnorderedElementsAre(7, 8, 8, 10));
}

TEST(AddReferencesUsedByConstraintTest, LinearMaxConstraint) {
  ConstraintProto ct;
  auto* arg = ct.mutable_lin_max();
  arg->mutable_target()->add_vars(7);
  arg->add_exprs();
  arg->add_exprs();
  arg->mutable_exprs(0)->add_vars(8);
  arg->mutable_exprs(0)->add_vars(9);
  arg->mutable_exprs(1)->add_vars(9);
  arg->mutable_exprs(1)->add_vars(10);
  const IndexReferences references = GetReferencesUsedByConstraint(ct);
  EXPECT_THAT(references.variables, UnorderedElementsAre(7, 8, 9, 9, 10));
}

TEST(AddReferencesUsedByConstraintTest, LinearConstraint) {
  ConstraintProto ct;
  auto* arg = ct.mutable_linear();
  arg->add_vars(0);
  arg->add_vars(1);
  arg->add_vars(2);
  const IndexReferences references = GetReferencesUsedByConstraint(ct);
  EXPECT_THAT(references.variables, UnorderedElementsAre(0, 1, 2));
}

TEST(UsedIntervalTest, Intervals) {
  ConstraintProto ct;
  auto* arg = ct.mutable_no_overlap();
  arg->add_intervals(0);
  arg->add_intervals(1);
  arg->add_intervals(2);
  EXPECT_THAT(UsedIntervals(ct), UnorderedElementsAre(0, 1, 2));
}

TEST(UsedVariablesTest, BasicTest) {
  ConstraintProto ct;
  ct.add_enforcement_literal(NegatedRef(7));
  auto* arg = ct.mutable_linear();
  arg->add_vars(0);
  arg->add_vars(1);
  arg->add_vars(NegatedRef(2));
  EXPECT_THAT(UsedVariables(ct), testing::ElementsAre(0, 1, 2, 7));
}

TEST(UsedVariablesTest, ConstraintWithMultipleEnforcement) {
  ConstraintProto ct;
  ct.add_enforcement_literal(NegatedRef(7));
  ct.add_enforcement_literal(NegatedRef(18));
  auto* arg = ct.mutable_linear();
  arg->add_vars(0);
  arg->add_vars(1);
  arg->add_vars(NegatedRef(2));
  EXPECT_THAT(UsedVariables(ct), testing::ElementsAre(0, 1, 2, 7, 18));
}

TEST(SetToNegatedLinearExpressionTest, BasicTest) {
  LinearExpressionProto expr;
  expr.set_offset(5);
  expr.add_vars(1);
  expr.add_coeffs(2);
  expr.add_vars(-1);
  expr.add_coeffs(-3);

  LinearExpressionProto negated_expr;
  SetToNegatedLinearExpression(expr, &negated_expr);
  EXPECT_THAT(negated_expr.vars(), testing::ElementsAre(-2, 0));
  EXPECT_THAT(negated_expr.coeffs(), testing::ElementsAre(2, -3));
  EXPECT_EQ(-5, negated_expr.offset());

  LinearExpressionProto expr2;
  expr2.set_offset(3);
  expr2.add_vars(2);
  expr2.add_coeffs(3);
  expr2.add_vars(-2);
  expr2.add_coeffs(-4);

  SetToNegatedLinearExpression(expr2, &negated_expr);
  EXPECT_THAT(negated_expr.vars(), testing::ElementsAre(-3, 1));
  EXPECT_THAT(negated_expr.coeffs(), testing::ElementsAre(3, -4));
  EXPECT_EQ(-3, negated_expr.offset());
}

TEST(ConstraintCaseNameTest, TestFewCases) {
  EXPECT_EQ("kBoolOr",
            ConstraintCaseName(ConstraintProto::ConstraintCase::kBoolOr));
  EXPECT_EQ("kLinear",
            ConstraintCaseName(ConstraintProto::ConstraintCase::kLinear));
  EXPECT_EQ("kEmpty", ConstraintCaseName(
                          ConstraintProto::ConstraintCase::CONSTRAINT_NOT_SET));
}

TEST(ComputeInnerObjectiveTest, SimpleExample) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 5 ] }
    objective {
      vars: [ 0, 1 ]
      coeffs: [ 2, 3 ]
      offset: 3
      scaling_factor: 1.5
    }
  )pb");
  const std::vector<int64_t> solution = {2, 3};
  EXPECT_EQ(ComputeInnerObjective(model_proto.objective(), solution), 13);
  EXPECT_EQ(ScaleObjectiveValue(model_proto.objective(), 13), (13 + 3) * 1.5);
  EXPECT_EQ(UnscaleObjectiveValue(model_proto.objective(), (13 + 3) * 1.5), 13);
}

TEST(GetRefFromExpressionTest, BasicAPI) {
  LinearExpressionProto expr;
  EXPECT_FALSE(ExpressionContainsSingleRef(expr));
  expr.set_offset(1);
  EXPECT_FALSE(ExpressionContainsSingleRef(expr));
  expr.set_offset(0);
  expr.add_vars(2);
  expr.add_coeffs(1);
  EXPECT_TRUE(ExpressionContainsSingleRef(expr));
  EXPECT_EQ(2, GetSingleRefFromExpression(expr));
  expr.set_coeffs(0, -1);
  EXPECT_TRUE(ExpressionContainsSingleRef(expr));
  EXPECT_EQ(-3, GetSingleRefFromExpression(expr));
  expr.set_coeffs(0, 2);
  EXPECT_FALSE(ExpressionContainsSingleRef(expr));
  expr.set_coeffs(0, 1);
  expr.add_vars(3);
  expr.add_coeffs(-1);
  EXPECT_FALSE(ExpressionContainsSingleRef(expr));
}

TEST(AddLinearExpressionToLinearConstraintTest, BasicApi) {
  LinearConstraintProto linear;
  linear.add_vars(1);
  linear.add_coeffs(-1);
  linear.add_domain(2);
  linear.add_domain(4);
  LinearExpressionProto expr;
  expr.add_vars(2);
  expr.add_coeffs(2);
  expr.add_vars(3);
  expr.add_coeffs(5);
  expr.set_offset(1);
  AddLinearExpressionToLinearConstraint(expr, -7, &linear);
  const LinearConstraintProto expected_linear_constraint = ParseTestProto(R"pb(
    vars: [ 1, 2, 3 ]
    coeffs: [ -1, -14, -35 ]
    domain: [ 9, 11 ]
  )pb");
  EXPECT_THAT(linear, testing::EqualsProto(expected_linear_constraint));
}

TEST(AddWeightedLiteralToLinearConstraintTest, BasicApi) {
  LinearConstraintProto linear;
  linear.add_vars(1);
  linear.add_coeffs(-1);
  linear.add_domain(2);
  linear.add_domain(4);
  int64_t offset = 0;
  AddWeightedLiteralToLinearConstraint(0, 4, &linear, &offset);
  const LinearConstraintProto expected_linear_constraint_1 = ParseTestProto(
      R"pb(
        vars: [ 1, 0 ]
        coeffs: [ -1, 4 ]
        domain: [ 2, 4 ]
      )pb");
  EXPECT_THAT(linear, testing::EqualsProto(expected_linear_constraint_1));
  EXPECT_EQ(offset, 0);

  AddWeightedLiteralToLinearConstraint(-3, 5, &linear, &offset);
  const LinearConstraintProto expected_linear_constraint_2 =
      ParseTestProto(R"pb(
        vars: [ 1, 0, 2 ]
        coeffs: [ -1, 4, -5 ]
        domain: [ 2, 4 ]
      )pb");
  EXPECT_THAT(linear, testing::EqualsProto(expected_linear_constraint_2));
  EXPECT_EQ(offset, 5);
}

TEST(SafeAddLinearExpressionToLinearConstraintTest, BasicApi) {
  LinearConstraintProto linear;
  linear.add_vars(1);
  linear.add_coeffs(-1);
  linear.add_domain(2);
  linear.add_domain(4);
  LinearExpressionProto expr;
  expr.add_vars(2);
  expr.add_coeffs(2);
  expr.add_vars(3);
  expr.add_coeffs(5);
  expr.set_offset(1);
  EXPECT_TRUE(SafeAddLinearExpressionToLinearConstraint(expr, -7, &linear));
  LinearExpressionProto large_coeff;
  large_coeff.add_vars(0);
  large_coeff.add_coeffs(1e10);
  EXPECT_FALSE(
      SafeAddLinearExpressionToLinearConstraint(large_coeff, 1e10, &linear));
  LinearExpressionProto large_offset;
  large_offset.set_offset(1e10);
  EXPECT_FALSE(
      SafeAddLinearExpressionToLinearConstraint(large_offset, 1e10, &linear));
}

// Fingerprint must be stable. So we can use golden values.
TEST(FingerprintTest, BasicApi) {
  const LinearExpressionProto lin = ParseTestProto(R"pb(
    vars: [ 1, 2, 3 ]
    coeffs: [ -1, -14, -35 ]
    offset: 11
  )pb");
  EXPECT_EQ(uint64_t{0x871AE5CE74BFBE37},
            FingerprintExpression(lin, kDefaultFingerprintSeed));
  EXPECT_EQ(uint64_t{0x3E7E7DEAEF2AB62C},
            FingerprintRepeatedField(lin.vars(), kDefaultFingerprintSeed));
  EXPECT_EQ(uint64_t{0x85715ADBDFD6F8AD},
            FingerprintSingleField(lin.offset(), kDefaultFingerprintSeed));
}

TEST(ConvertCpModelProtoToCnfTest, BasicExample) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_or { literals: [ 0, 1, 2 ] } }
    constraints { bool_or { literals: [ -1, -2 ] } }
    constraints {
      enforcement_literal: [ 2 ]
      bool_or { literals: [ -1, -2 ] }
    }
    constraints {
      enforcement_literal: [ 0, 1 ]
      bool_and { literals: [ -1, -2 ] }
    }
  )pb");
  const std::string expected = R"(p cnf 3 5
1 2 3 0
-1 -2 0
-3 -1 -2 0
-1 -2 -1 0
-1 -2 -2 0
)";
  std::string cnf;
  EXPECT_TRUE(ConvertCpModelProtoToCnf(model_proto, &cnf));
  EXPECT_EQ(expected, cnf);
}

TEST(ConvertCpModelProtoToCnfTest, FixedVariables) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 0 ] }
    variables { domain: [ 1, 1 ] }
    constraints { bool_or { literals: [ 0, 1, 2 ] } }
    constraints { bool_or { literals: [ -1, -2 ] } }
    constraints { bool_or { literals: [ -1, -2 ] } }
  )pb");
  const std::string expected = R"(p cnf 3 5
-2 0
3 0
1 2 3 0
-1 -2 0
-1 -2 0
)";
  std::string cnf;
  EXPECT_TRUE(ConvertCpModelProtoToCnf(model_proto, &cnf));
  EXPECT_EQ(expected, cnf);
}

TEST(ConvertCpModelProtoToCnfTest, BoolAnd) {
  const CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_and { literals: [ 0, 1, 2 ] } }
  )pb");
  const std::string expected = R"(p cnf 3 3
1 0
2 0
3 0
)";
  std::string cnf;
  EXPECT_TRUE(ConvertCpModelProtoToCnf(model_proto, &cnf));
  EXPECT_EQ(expected, cnf);
}

TEST(LargestConstraintTypeTest, IsCorrect) {
  const google::protobuf::Descriptor* message_descriptor =
      ConstraintProto::descriptor();
  ASSERT_NE(message_descriptor, nullptr);

  const google::protobuf::OneofDescriptor* oneof_descriptor =
      message_descriptor->FindOneofByName("constraint");
  ASSERT_NE(oneof_descriptor, nullptr) << "Oneof 'constraint' not found";

  int max_field_number = -1;
  for (int i = 0; i < oneof_descriptor->field_count(); ++i) {
    const google::protobuf::FieldDescriptor* field = oneof_descriptor->field(i);
    ASSERT_NE(field, nullptr);
    max_field_number = std::max(max_field_number, field->number());
  }

  EXPECT_EQ(max_field_number, kLargestConstraintType)
      << "kLargestConstraintType (" << kLargestConstraintType
      << ") does not match the largest field number in the 'constraint' oneof ("
      << max_field_number << "). Please update the constant.";

  EXPECT_LT(kLargestConstraintType, 1 << kConstraintTypeBitSize)
      << "kLargestConstraintType (" << kLargestConstraintType
      << ") is not less than 2^kConstraintTypeBitSize ("
      << (1 << kConstraintTypeBitSize)
      << "). Please update kConstraintTypeBitSize.";
}

TEST(AffineExprTest, ToStringAndStringify) {
  EXPECT_EQ((AffineExpr{.var = -1, .coeff = 0, .offset = 42}.ToString()), "42");
  EXPECT_EQ((AffineExpr{.var = 5, .coeff = 0, .offset = 12}.ToString()), "12");
  EXPECT_EQ((AffineExpr{.var = 3, .coeff = 1, .offset = 0}.ToString()), "X3");
  EXPECT_EQ((AffineExpr{.var = 3, .coeff = 1, .offset = 5}.ToString()),
            "X3 + 5");
  EXPECT_EQ((AffineExpr{.var = 3, .coeff = 1, .offset = -5}.ToString()),
            "X3 - 5");
  EXPECT_EQ((AffineExpr{.var = 3, .coeff = -1, .offset = 0}.ToString()), "-X3");
  EXPECT_EQ((AffineExpr{.var = 3, .coeff = -1, .offset = 5}.ToString()),
            "-X3 + 5");
  EXPECT_EQ((AffineExpr{.var = 3, .coeff = -1, .offset = -5}.ToString()),
            "-X3 - 5");
  EXPECT_EQ((AffineExpr{.var = 2, .coeff = 4, .offset = 0}.ToString()),
            "4 * X2");
  EXPECT_EQ((AffineExpr{.var = 2, .coeff = 4, .offset = 10}.ToString()),
            "4 * X2 + 10");
  EXPECT_EQ((AffineExpr{.var = 2, .coeff = 4, .offset = -10}.ToString()),
            "4 * X2 - 10");
  EXPECT_EQ(
      absl::StrFormat("%v", AffineExpr{.var = 1, .coeff = 2, .offset = 3}),
      "2 * X1 + 3");
}

TEST(AffineExprTest, EqualityAndHash) {
  AffineExpr a{.var = 1, .coeff = 2, .offset = 3};
  AffineExpr b{.var = 1, .coeff = 2, .offset = 3};
  AffineExpr c{.var = 1, .coeff = 2, .offset = 4};
  AffineExpr d{.var = 2, .coeff = 2, .offset = 3};
  AffineExpr e{.var = 1, .coeff = 1, .offset = 3};

  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
  EXPECT_NE(a, d);
  EXPECT_NE(a, e);

  absl::flat_hash_set<AffineExpr> expr_set;
  expr_set.insert(a);
  EXPECT_TRUE(expr_set.contains(b));
  EXPECT_FALSE(expr_set.contains(c));
}

TEST(AffineExprTest, MinAndMaxEvaluation) {
  CpModelProto model_proto = ParseTestProto(R"pb(
    variables { domain: [ 2, 10 ] }
    variables { domain: [ -5, 3 ] }
    variables {}
  )pb");

  // Constant expression
  AffineExpr constant_expr{.var = -1, .coeff = 0, .offset = 7};
  EXPECT_EQ(GetAffineExprMin(constant_expr, model_proto), 7);
  EXPECT_EQ(GetAffineExprMax(constant_expr, model_proto), 7);

  // Positive coefficient: var 0 in [2, 10], 3 * X0 + 4 -> [10, 34]
  AffineExpr pos_expr{.var = 0, .coeff = 3, .offset = 4};
  EXPECT_EQ(GetAffineExprMin(pos_expr, model_proto), 10);
  EXPECT_EQ(GetAffineExprMax(pos_expr, model_proto), 34);

  // Negative coefficient: var 1 in [-5, 3], -2 * X1 + 1 -> [-5, 11]
  AffineExpr neg_expr{.var = 1, .coeff = -2, .offset = 1};
  EXPECT_EQ(GetAffineExprMin(neg_expr, model_proto), -5);
  EXPECT_EQ(GetAffineExprMax(neg_expr, model_proto), 11);
}

TEST(ComputeFloatObjectiveValueTest, DirectedRoundingForMinAndMax) {
  // 1.0 + 2^-53 is halfway between 1.0 and nextafter(1.0, inf) and rounds to
  // nearest-even (1.0) in standard double arithmetic.
  FloatObjectiveProto proto;
  proto.add_vars(0);
  proto.add_vars(1);
  proto.add_coeffs(1.0);
  proto.add_coeffs(std::ldexp(1.0, -53));
  const std::vector<int64_t> solution = {1, 1};

  proto.set_maximize(false);
  EXPECT_EQ(ComputeFloatObjectiveValue(proto, solution),
            std::nextafter(1.0, std::numeric_limits<double>::infinity()));

  proto.set_maximize(true);
  EXPECT_EQ(ComputeFloatObjectiveValue(proto, solution), 1.0);
}

TEST(ComputeFloatObjectiveValueTest, DirectedRoundingForZeroPointOnePlusTwo) {
  // In infinite precision, double(0.1) + double(0.2) lies strictly between
  // nextafter(0.1 + 0.2, -inf) and (0.1 + 0.2).
  FloatObjectiveProto proto;
  proto.add_vars(0);
  proto.add_vars(1);
  proto.add_coeffs(0.1);
  proto.add_coeffs(0.2);
  const std::vector<int64_t> solution = {1, 1};

  const double rounded_up = 0.1 + 0.2;
  const double rounded_down =
      std::nextafter(rounded_up, -std::numeric_limits<double>::infinity());

  proto.set_maximize(false);
  EXPECT_EQ(ComputeFloatObjectiveValue(proto, solution), rounded_up);

  proto.set_maximize(true);
  EXPECT_EQ(ComputeFloatObjectiveValue(proto, solution), rounded_down);
}

TEST(ComputeFloatObjectiveValueTest, ExactWithLargeInt64Solution) {
  // (2^53 + 1) cannot be represented in double without losing the +1 bit.
  FloatObjectiveProto proto;
  proto.add_vars(0);
  proto.add_vars(1);
  proto.add_coeffs(1.0);
  proto.add_coeffs(1.0);
  const std::vector<int64_t> solution = {(1LL << 53) + 1, -(1LL << 53)};

  proto.set_maximize(false);
  EXPECT_EQ(ComputeFloatObjectiveValue(proto, solution), 1.0);

  proto.set_maximize(true);
  EXPECT_EQ(ComputeFloatObjectiveValue(proto, solution), 1.0);
}

TEST(CanonicalizeLinMaxTest, SortsTermsAndExpressions) {
  ConstraintProto ct = ParseTestProto(R"pb(
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
  )pb");
  EXPECT_FALSE(LinMaxIsCanonical(ct));
  CanonicalizeLinMax(&ct);
  EXPECT_TRUE(LinMaxIsCanonical(ct));

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
  EXPECT_THAT(ct, testing::EqualsProto(expected));
}

TEST(CanonicalizeLinMaxTest, CancelsZeroCoefficientTerms) {
  ConstraintProto ct = ParseTestProto(R"pb(
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
  )pb");
  EXPECT_FALSE(LinMaxIsCanonical(ct));
  CanonicalizeLinMax(&ct);
  EXPECT_TRUE(LinMaxIsCanonical(ct));

  const ConstraintProto expected = ParseTestProto(R"pb(
    lin_max {
      target { offset: 3 }
      exprs { offset: 1 }
      exprs { offset: 7 }
    }
  )pb");
  EXPECT_THAT(ct, testing::EqualsProto(expected));
}

TEST(CanonicalizeLinMaxTest, CanonicalizesEnforcementLiterals) {
  ConstraintProto ct = ParseTestProto(R"pb(
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
  )pb");
  EXPECT_FALSE(LinMaxIsCanonical(ct));
  CanonicalizeLinMax(&ct);
  EXPECT_TRUE(LinMaxIsCanonical(ct));

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
  EXPECT_THAT(ct, testing::EqualsProto(expected));

  // Contradictory enforcement literals (x and not(x)) are not canonical and
  // clear the constraint when canonicalized.
  ConstraintProto contradictory_ct = ParseTestProto(R"pb(
    enforcement_literal: [ -1, 0 ]
    lin_max {
      target {
        vars: [ 2 ]
        coeffs: [ 1 ]
      }
      exprs {
        vars: [ 2 ]
        coeffs: [ 1 ]
      }
    }
  )pb");
  EXPECT_FALSE(LinMaxIsCanonical(contradictory_ct));
  CanonicalizeLinMax(&contradictory_ct);
  EXPECT_EQ(contradictory_ct.constraint_case(),
            ConstraintProto::CONSTRAINT_NOT_SET);
}

TEST(AffineExprTest, ConstAndVarHelpers) {
  constexpr AffineExpr c = AffineExpr::Const(42);
  EXPECT_EQ(c.var, -1);
  EXPECT_EQ(c.coeff, 0);
  EXPECT_EQ(c.offset, 42);
  EXPECT_TRUE(c.is_constant());
  EXPECT_EQ(c.constant_value(), 42);

  constexpr AffineExpr v = AffineExpr::Var(7);
  EXPECT_EQ(v.var, 7);
  EXPECT_EQ(v.coeff, 1);
  EXPECT_EQ(v.offset, 0);
  EXPECT_FALSE(v.is_constant());

  AffineExpr scaled{.var = 5, .coeff = 0, .offset = 10};
  EXPECT_TRUE(scaled.is_constant());
  EXPECT_EQ(scaled.constant_value(), 10);
}

TEST(CpModelUtilsTest, ApplyToAllLinearExpressions) {
  // Unperformed optional interval stub: mutable visitor must not materialize
  // start/size/end submessages.
  ConstraintProto unperformed_interval = ParseTestProto(R"pb(
    enforcement_literal: [ 0 ]
    interval {}
  )pb");
  int visited = 0;
  ApplyToAllLinearExpressions(
      [&](LinearExpressionProto* /*expr*/) { ++visited; },
      &unperformed_interval);
  EXPECT_EQ(visited, 0);
  EXPECT_FALSE(unperformed_interval.interval().has_start());
  EXPECT_FALSE(unperformed_interval.interval().has_size());
  EXPECT_FALSE(unperformed_interval.interval().has_end());

  // Routes node expressions are visited by both mutable and const overloads.
  ConstraintProto routes_ct = ParseTestProto(R"pb(
    routes {
      tails: [ 0 ]
      heads: [ 1 ]
      literals: [ 0 ]
      dimensions {
        exprs {
          vars: [ 1 ]
          coeffs: [ 2 ]
          offset: 3
        }
      }
    }
  )pb");
  int routes_visited = 0;
  ApplyToAllLinearExpressions(
      [&](LinearExpressionProto* expr) {
        expr->set_offset(expr->offset() + 10);
        ++routes_visited;
      },
      &routes_ct);
  EXPECT_EQ(routes_visited, 1);
  EXPECT_EQ(routes_ct.routes().dimensions(0).exprs(0).offset(), 13);

  const ConstraintProto& const_routes = routes_ct;
  int const_visited = 0;
  ApplyToAllLinearExpressions(
      [&](const LinearExpressionProto& expr) {
        EXPECT_EQ(expr.offset(), 13);
        ++const_visited;
      },
      const_routes);
  EXPECT_EQ(const_visited, 1);

  // Legacy table with vars populated should not visit exprs.
  ConstraintProto legacy_table = ParseTestProto(R"pb(
    table {
      vars: [ 0, 1 ]
      values: [ 0, 1 ]
    }
  )pb");
  visited = 0;
  ApplyToAllLinearExpressions(
      [&](const LinearExpressionProto& /*expr*/) { ++visited; },
      static_cast<const ConstraintProto&>(legacy_table));
  EXPECT_EQ(visited, 0);
}

TEST(CpModelUtilsTest, CanonicalizeLinearConstraintProto) {
  LinearConstraintProto lin = ParseTestProto(R"pb(
    vars: [ 2, 0, 2, 1 ]
    coeffs: [ 3, 4, -1, 5 ]
    domain: [ 10, 20 ]
  )pb");
  // Enforcement: var 0 == 1 (shifts domain by -4), var 1 == 0 (dropped).
  CanonicalizeLinearExpression({0, -2}, &lin);
  const LinearConstraintProto expected = ParseTestProto(R"pb(
    vars: [ 2 ]
    coeffs: [ 2 ]
    domain: [ 6, 16 ]
  )pb");
  EXPECT_THAT(lin, testing::EqualsProto(expected));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
