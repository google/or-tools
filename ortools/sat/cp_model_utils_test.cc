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

#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
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

void Random(ConstraintProto ct) {
  // The behavior of both functions differ on the enforcement_literal, so
  // we clear it. TODO(user): make the behavior identical.
  ct.clear_enforcement_literal();

  absl::flat_hash_set<int> expected;
  {
    const IndexReferences references = GetReferencesUsedByConstraint(ct);
    expected.insert(references.variables.begin(), references.variables.end());
    expected.insert(references.literals.begin(), references.literals.end());
  }

  absl::flat_hash_set<int> var_and_literals;
  ApplyToAllVariableIndices(
      [&var_and_literals](int* ref) { var_and_literals.insert(*ref); }, &ct);
  ApplyToAllLiteralIndices(
      [&var_and_literals](int* ref) { var_and_literals.insert(*ref); }, &ct);
  EXPECT_EQ(var_and_literals, expected) << ProtobufDebugString(ct);

  std::vector<int> intervals;
  ApplyToAllIntervalIndices(
      [&intervals](int* ref) { intervals.push_back(*ref); }, &ct);
  gtl::STLSortAndRemoveDuplicates(&intervals);
  EXPECT_EQ(intervals, UsedIntervals(ct)) << ProtobufDebugString(ct);
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
    constraints { bool_or { literals: [ -1, -2 ] } }
    constraints {
      enforcement_literal: [ 0, 1 ]
      bool_and { literals: [ -1, -2 ] }
    }
  )pb");
  const std::string expected = R"(p cnf 3 5
1 2 3 0
-1 -2 0
-1 -2 0
-1 -2 -1 0
-1 -2 -2 0
)";
  std::string cnf;
  EXPECT_TRUE(ConvertCpModelProtoToCnf(model_proto, &cnf));
  EXPECT_EQ(expected, cnf);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
