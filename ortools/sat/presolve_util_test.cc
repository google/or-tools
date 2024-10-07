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

#include "ortools/sat/presolve_util.h"

#include <stdint.h>

#include <array>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/random/random.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/logging.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::ElementsAre;

TEST(DomainDeductionsTest, BasicTest) {
  DomainDeductions deductions;

  deductions.AddDeduction(0, 3, Domain(0, 4));
  deductions.AddDeduction(1, 3, Domain(1, 8));

  EXPECT_TRUE(deductions.ProcessClause({0, 1, 2}).empty());
  EXPECT_THAT(deductions.ProcessClause({0, 1}),
              ElementsAre(std::make_pair(3, Domain(0, 8))));
  EXPECT_THAT(deductions.ProcessClause({0}),
              ElementsAre(std::make_pair(3, Domain(0, 4))));
  EXPECT_THAT(deductions.ProcessClause({1}),
              ElementsAre(std::make_pair(3, Domain(1, 8))));

  deductions.MarkProcessingAsDoneForNow();
  EXPECT_TRUE(deductions.ProcessClause({0}).empty());

  deductions.AddDeduction(0, 3, Domain(4, 4));
  EXPECT_EQ(deductions.ImpliedDomain(0, 3), Domain(4, 4));
  EXPECT_EQ(deductions.ImpliedDomain(7, 3), Domain::AllValues());
  EXPECT_TRUE(deductions.ProcessClause({1}).empty());
  EXPECT_THAT(deductions.ProcessClause({0}),
              ElementsAre(std::make_pair(3, Domain(4, 4))));
  EXPECT_THAT(deductions.ProcessClause({0, 1}),
              ElementsAre(std::make_pair(3, Domain(1, 8))));
}

TEST(AddLinearConstraintMultiple, BasicTestWithPositiveCoeff) {
  ConstraintProto to_modify = ParseTestProto(R"pb(
    linear {
      vars: [ 0, 1, 2, 3 ]
      coeffs: [ 2, 3, 4, 5 ]
      domain: [ 0, 10 ]
    }
  )pb");
  const ConstraintProto to_add = ParseTestProto(R"pb(
    linear {
      vars: [ 0, 1, 2, 3 ]
      coeffs: [ 2, 1, 4, 5 ]
      domain: [ 3, 3 ]
    }
  )pb");

  EXPECT_TRUE(AddLinearConstraintMultiple(3, to_add, &to_modify));
  const ConstraintProto expected = ParseTestProto(R"pb(
    linear {
      vars: [ 0, 1, 2, 3 ]
      coeffs: [ 8, 6, 16, 20 ]
      domain: [ 9, 19 ]
    }
  )pb");
  EXPECT_THAT(to_modify, testing::EqualsProto(expected));
}

TEST(SubstituteVariableTest, BasicTestWithPositiveCoeff) {
  ConstraintProto constraint = ParseTestProto(R"pb(
    linear {
      vars: [ 0, 1, 2, 3 ]
      coeffs: [ 2, 3, 4, 5 ]
      domain: [ 0, 10 ]
    }
  )pb");
  const ConstraintProto definition = ParseTestProto(R"pb(
    linear {
      vars: [ 0, 1, 2, 3 ]
      coeffs: [ 2, 1, 4, 5 ]
      domain: [ 3, 3 ]
    }
  )pb");

  EXPECT_TRUE(SubstituteVariable(1, 1, definition, &constraint));

  // We have X1 = 3 - 2X0 - 4X2 -5X3 and the coeff of X1 in constraint is 3.
  const ConstraintProto expected = ParseTestProto(R"pb(
    linear {
      vars: [ 0, 2, 3 ]
      coeffs: [ -4, -8, -10 ]
      domain: [ -9, 1 ]
    }
  )pb");
  EXPECT_THAT(constraint, testing::EqualsProto(expected));
}

TEST(SubstituteVariableTest, BasicTestWithNegativeCoeff) {
  ConstraintProto constraint = ParseTestProto(R"pb(
    linear {
      vars: [ 0, 1, 2, 3 ]
      coeffs: [ 2, 3, 4, 5 ]
      domain: [ 0, 10 ]
    }
  )pb");
  const ConstraintProto definition = ParseTestProto(R"pb(
    linear {
      vars: [ 0, 1, 2, 3 ]
      coeffs: [ 2, -1, 4, 5 ]
      domain: [ 3, 3 ]
    }
  )pb");

  EXPECT_TRUE(SubstituteVariable(1, -1, definition, &constraint));

  // We have X1 = 2X0 + 4X2 + 5X3 - 3 and the coeff of X1 in constraint is 3.
  const ConstraintProto expected = ParseTestProto(R"pb(
    linear {
      vars: [ 0, 2, 3 ]
      coeffs: [ 8, 16, 20 ]
      domain: [ 9, 19 ]
    }
  )pb");
  EXPECT_THAT(constraint, testing::EqualsProto(expected));
}

TEST(SubstituteVariableTest, WorkWithDuplicate) {
  ConstraintProto constraint = ParseTestProto(R"pb(
    linear {
      vars: [ 0, 1, 2, 3, 1, 3 ]
      coeffs: [ 2, 3, 4, 5, 5, 5 ]
      domain: [ 0, 10 ]
    }
  )pb");
  const ConstraintProto definition = ParseTestProto(R"pb(
    linear {
      vars: [ 0, 1, 2, 3 ]
      coeffs: [ 2, 1, 4, 5 ]
      domain: [ 3, 3 ]
    }
  )pb");

  EXPECT_TRUE(SubstituteVariable(1, 1, definition, &constraint));

  // Constraint is actually 2X0 + 7X1 + 4X2 + 10X3
  // Which gives  2X0 + 8(3 - 2X0 - 4X2 -5X3) + 4X2 + 10X3
  const ConstraintProto expected = ParseTestProto(R"pb(
    linear {
      vars: [ 0, 2, 3 ]
      coeffs: [ -14, -28, -30 ]
      domain: [ -24, -14 ]
    }
  )pb");
  EXPECT_THAT(constraint, testing::EqualsProto(expected));
}

TEST(SubstituteVariableTest, FalseIfVariableNotThere) {
  ConstraintProto constraint = ParseTestProto(R"pb(
    linear {
      vars: [ 0, 1, 1 ]
      coeffs: [ 2, 3, -3 ]
      domain: [ 0, 10 ]
    }
  )pb");
  const ConstraintProto definition = ParseTestProto(R"pb(
    linear {
      vars: [ 0, 1, 2, 3 ]
      coeffs: [ 2, 1, 4, 5 ]
      domain: [ 3, 3 ]
    }
  )pb");

  EXPECT_FALSE(SubstituteVariable(1, 1, definition, &constraint));
}

TEST(ActivityBoundHelperTest, TrivialMaxBound) {
  ActivityBoundHelper helper;

  // If there are no amo, we get trivial values
  std::vector<std::array<int64_t, 2>> conditional;
  const int64_t result =
      helper.ComputeMaxActivity({{+3, 4}, {-1, -7}, {-3, 5}}, &conditional);
  EXPECT_EQ(result, 9);
  ASSERT_EQ(conditional.size(), 3);
  EXPECT_EQ(conditional[0][0], 5);
  EXPECT_EQ(conditional[0][1], 9);
  EXPECT_EQ(conditional[1][0], 9);
  EXPECT_EQ(conditional[1][1], 2);
  EXPECT_EQ(conditional[2][0], 4);
  EXPECT_EQ(conditional[2][1], 9);
}

TEST(ActivityBoundHelperTest, TrivialMinBound) {
  ActivityBoundHelper helper;

  // If there are no amo, we get trivial values
  std::vector<std::array<int64_t, 2>> conditional;
  const int64_t result =
      helper.ComputeMinActivity({{+3, 4}, {-1, -7}, {-3, 5}}, &conditional);
  EXPECT_EQ(result, -7);
  ASSERT_EQ(conditional.size(), 3);
  EXPECT_EQ(conditional[0][0], -7);
  EXPECT_EQ(conditional[0][1], -3);
  EXPECT_EQ(conditional[1][0], 0);
  EXPECT_EQ(conditional[1][1], -7);
  EXPECT_EQ(conditional[2][0], -7);
  EXPECT_EQ(conditional[2][1], -2);
}

TEST(ActivityBoundHelperTest, DisjointAmo) {
  ActivityBoundHelper helper;
  helper.AddAtMostOne({+1, +2, -3});
  helper.AddAtMostOne({-5, -6, -7});

  std::vector<std::array<int64_t, 2>> conditional;
  const int64_t result = helper.ComputeMaxActivity(
      {{+1, 4}, {+2, 7}, {-5, 5}, {-6, 6}, {10, 3}}, &conditional);

  // We have a partition [+1, +2] [-5, -6] [10].
  EXPECT_EQ(result, 16);
  ASSERT_EQ(conditional.size(), 5);
  EXPECT_EQ(conditional[0][0], 16);
  EXPECT_EQ(conditional[0][1], 13);
  EXPECT_EQ(conditional[1][0], 13);
  EXPECT_EQ(conditional[1][1], 16);

  EXPECT_EQ(conditional[2][0], 16);
  EXPECT_EQ(conditional[2][1], 15);
  EXPECT_EQ(conditional[3][0], 15);
  EXPECT_EQ(conditional[3][1], 16);

  EXPECT_EQ(conditional[4][0], 13);
  EXPECT_EQ(conditional[4][1], 16);
}

TEST(ActivityBoundHelperTest, PartitionLiteralsIntoAmo) {
  ActivityBoundHelper helper;
  helper.AddAtMostOne({+1, +2, -3});
  helper.AddAtMostOne({-5, -6, -7});

  // The order is not documented, but it actually follow the original order.
  std::vector<int> literals({+1, -6, +2, 10, -5});
  EXPECT_THAT(
      helper.PartitionLiteralsIntoAmo(literals),
      ElementsAre(ElementsAre(+1, +2), ElementsAre(-6, -5), ElementsAre(10)));
}

TEST(ActivityBoundHelperTest, IsAmo) {
  ActivityBoundHelper helper;
  helper.AddAtMostOne({+1, +2, -3});
  helper.AddAtMostOne({-5, -6, -7});

  EXPECT_FALSE(helper.IsAmo({+1, +2, +3}));
  EXPECT_FALSE(helper.IsAmo({+1, -5, -6}));
  EXPECT_TRUE(helper.IsAmo({+1, -3}));
  EXPECT_TRUE(helper.IsAmo({-5, -7}));
}

// We will compare with CP-SAT on small instances, and make sure bounds are
// correct.
TEST(ActivityBoundHelperTest, RandomTest) {
  for (int num_test = 0; num_test < 10; ++num_test) {
    absl::BitGen random;
    const int num_vars = 10;
    const int num_amos = 5;

    // Generate random sat instances.
    // These are always feasible.
    CpModelBuilder model;
    std::vector<BoolVar> vars;
    for (int i = 0; i < num_vars; ++i) vars.push_back(model.NewBoolVar());
    for (int c = 0; c < num_amos; ++c) {
      std::vector<BoolVar> amo;
      for (int i = 0; i < num_vars; ++i) {
        if (absl::Bernoulli(random, 0.5)) {
          amo.push_back(vars[i]);
        }
      }
      if (!amo.empty()) model.AddAtMostOne(amo);
    }
    LinearExpr obj;
    std::vector<std::pair<int, int64_t>> terms;
    for (int i = 0; i < num_vars; ++i) {
      const int coeff = absl::Uniform(random, -100, 100);
      obj += coeff * vars[i];
      terms.push_back({i, coeff});
    }
    model.Maximize(obj);

    // Get Maximum bound.
    SatParameters params;
    params.set_log_search_progress(false);
    params.set_cp_model_presolve(false);
    const CpModelProto proto = model.Build();
    const CpSolverResponse response = SolveWithParameters(proto, params);
    EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);

    // Same with helper
    ActivityBoundHelper helper;
    helper.AddAllAtMostOnes(proto);
    std::vector<std::array<int64_t, 2>> conditional_max;
    const int64_t max_activity =
        helper.ComputeMaxActivity(terms, &conditional_max);
    EXPECT_GE(max_activity, response.objective_value());
    LOG(INFO) << response.objective_value() << " " << max_activity;
    for (int i = 0; i < conditional_max.size(); ++i) {
      // We also know the exact bound for the returned optimal solution.
      EXPECT_GE(conditional_max[i][response.solution(i)],
                response.objective_value());
    }
  }
}

TEST(ActivityBoundHelperTest, PresolveEnforcement) {
  ActivityBoundHelper helper;
  helper.AddAtMostOne({+1, +2, +3});
  helper.AddAtMostOne({+4, +5, +6, +7});

  ConstraintProto ct;
  ct.add_enforcement_literal(+1);
  ct.add_enforcement_literal(NegatedRef(+2));
  ct.add_enforcement_literal(+6);

  absl::flat_hash_set<int> at_true;
  EXPECT_TRUE(helper.PresolveEnforcement({1, 2, 3, 4, 5}, &ct, &at_true));

  // NegatedRef(+2) is a consequence of +1 (we process in order), so removed.
  EXPECT_THAT(ct.enforcement_literal(), ElementsAre(+1, +6));
  EXPECT_TRUE(at_true.contains(+1));
  EXPECT_TRUE(at_true.contains(NegatedRef(+2)));
  EXPECT_TRUE(at_true.contains(NegatedRef(+3)));
  EXPECT_TRUE(at_true.contains(NegatedRef(+4)));
  EXPECT_TRUE(at_true.contains(NegatedRef(+5)));

  // Not in the list, so not contained.
  EXPECT_FALSE(at_true.contains(+7));
  EXPECT_FALSE(at_true.contains(NegatedRef(+7)));
}

// This used to fail because of the degenerate AMO with x and not(x).
TEST(ActivityBoundHelperTest, PresolveEnforcementCornerCase) {
  ActivityBoundHelper helper;
  helper.AddAtMostOne({+1, -2});

  ConstraintProto ct;
  ct.add_enforcement_literal(+1);

  absl::flat_hash_set<int> at_true;
  EXPECT_TRUE(helper.PresolveEnforcement({}, &ct, &at_true));
  EXPECT_THAT(ct.enforcement_literal(), ElementsAre(+1));
}

TEST(ClauseWithOneMissingHasherTest, BasicTest) {
  absl::BitGen random;
  ClauseWithOneMissingHasher hasher(random);

  hasher.RegisterClause(0, {+1, -5, +6, +7});
  hasher.RegisterClause(2, {+1, +7, +6, -4});
  EXPECT_EQ(hasher.HashWithout(0, -5), hasher.HashWithout(2, -4));
  EXPECT_NE(hasher.HashWithout(0, +6), hasher.HashWithout(2, +6));
}

// !X1 =>  X2 + X3 <= 1
// X1 + X2 <= 1
//
// when X1 is true, we can see that X2 + X3 <= 1 still stand, so we don't need
// the enforcement.
TEST(ActivityBoundHelper, RemoveEnforcementThatCouldBeLifted) {
  ActivityBoundHelper helper;
  helper.AddAtMostOne({+1, +2});

  ConstraintProto ct;
  ct.add_enforcement_literal(NegatedRef(1));
  std::vector<std::pair<int, int64_t>> terms{{+2, 1}, {+3, 1}};

  const int num_removed = helper.RemoveEnforcementThatMakesConstraintTrivial(
      terms, Domain(0), Domain(0, 1), &ct);
  EXPECT_EQ(num_removed, 1);
  EXPECT_TRUE(ct.enforcement_literal().empty());
}

// !X1 => 2 * X2 + X3 + X4 <= 2 and X1 + X2 + X3 <= 1
// Note that in this case, if X1 is 1, we have some slack, so we could lift it
// into X1 + 2 * X2 + X3 + X4 <= 2.
//
// But here, we could just extract X2 as an enforcement too, and just have
// X2 => X4 <= 0. This should just be a stronger relaxation.
TEST(ActivityBoundHelper, RemoveEnforcementThatCouldBeLiftedCase2) {
  ActivityBoundHelper helper;
  helper.AddAtMostOne({+1, +2, +3});

  ConstraintProto ct;
  ct.add_enforcement_literal(NegatedRef(1));
  std::vector<std::pair<int, int64_t>> terms{{+2, 2}, {+3, 1}, {+4, 1}};

  const int num_removed = helper.RemoveEnforcementThatMakesConstraintTrivial(
      terms, Domain(0), Domain(0, 2), &ct);
  EXPECT_EQ(num_removed, 1);
  EXPECT_TRUE(ct.enforcement_literal().empty());
}

TEST(ClauseIsEnforcementImpliesLiteralTest, BasicTest) {
  EXPECT_TRUE(ClauseIsEnforcementImpliesLiteral(
      {+1, -5, +7, -9}, {NegatedRef(+1), NegatedRef(-5), NegatedRef(-9)}, +7));
}

LinearConstraintProto GetLinear(std::vector<std::pair<int, int64_t>> terms) {
  LinearConstraintProto result;
  for (const auto [var, coeff] : terms) {
    result.add_vars(var);
    result.add_coeffs(coeff);
  }
  return result;
}

TEST(FindSingleLinearDifferenceTest, TwoDiff1) {
  LinearConstraintProto lin1 = GetLinear({{0, 1}, {1, 1}, {2, 1}});
  LinearConstraintProto lin2 = GetLinear({{0, 2}, {1, 1}, {2, 2}});
  int var1, var2;
  int64_t coeff1, coeff2;
  EXPECT_FALSE(
      FindSingleLinearDifference(lin1, lin2, &var1, &coeff1, &var2, &coeff2));
  EXPECT_FALSE(
      FindSingleLinearDifference(lin2, lin1, &var1, &coeff1, &var2, &coeff2));
}

TEST(FindSingleLinearDifferenceTest, TwoDiff2) {
  LinearConstraintProto lin1 = GetLinear({{0, 1}, {1, 1}, {3, 1}});
  LinearConstraintProto lin2 = GetLinear({{0, 2}, {1, 1}, {2, 1}});
  int var1, var2;
  int64_t coeff1, coeff2;
  EXPECT_FALSE(
      FindSingleLinearDifference(lin1, lin2, &var1, &coeff1, &var2, &coeff2));
  EXPECT_FALSE(
      FindSingleLinearDifference(lin2, lin1, &var1, &coeff1, &var2, &coeff2));
}

TEST(FindSingleLinearDifferenceTest, OkNotSameVariable) {
  LinearConstraintProto lin1 = GetLinear({{0, 1}, {1, 1}, {3, 1}});
  LinearConstraintProto lin2 = GetLinear({{0, 1}, {2, 1}, {3, 1}});
  int var1, var2;
  int64_t coeff1, coeff2;
  EXPECT_TRUE(
      FindSingleLinearDifference(lin2, lin1, &var1, &coeff1, &var2, &coeff2));
  EXPECT_TRUE(
      FindSingleLinearDifference(lin1, lin2, &var1, &coeff1, &var2, &coeff2));
  EXPECT_EQ(var1, 1);
  EXPECT_EQ(coeff1, 1);
  EXPECT_EQ(var2, 2);
  EXPECT_EQ(coeff2, 1);
}

TEST(FindSingleLinearDifferenceTest, OkNotSameCoeff) {
  LinearConstraintProto lin1 = GetLinear({{0, 1}, {1, 1}, {3, 1}});
  LinearConstraintProto lin2 = GetLinear({{0, 1}, {1, 3}, {3, 1}});
  int var1, var2;
  int64_t coeff1, coeff2;
  EXPECT_TRUE(
      FindSingleLinearDifference(lin2, lin1, &var1, &coeff1, &var2, &coeff2));
  EXPECT_TRUE(
      FindSingleLinearDifference(lin1, lin2, &var1, &coeff1, &var2, &coeff2));
  EXPECT_EQ(var1, 1);
  EXPECT_EQ(coeff1, 1);
  EXPECT_EQ(var2, 1);
  EXPECT_EQ(coeff2, 3);
}

TEST(FindSingleLinearDifferenceTest, OkNotSamePosition) {
  LinearConstraintProto lin1 = GetLinear({{0, 1}, {3, 1}, {5, 1}});
  LinearConstraintProto lin2 = GetLinear({{0, 1}, {1, 3}, {3, 1}});
  int var1, var2;
  int64_t coeff1, coeff2;
  EXPECT_TRUE(
      FindSingleLinearDifference(lin2, lin1, &var1, &coeff1, &var2, &coeff2));
  EXPECT_TRUE(
      FindSingleLinearDifference(lin1, lin2, &var1, &coeff1, &var2, &coeff2));
  EXPECT_EQ(var1, 5);
  EXPECT_EQ(coeff1, 1);
  EXPECT_EQ(var2, 1);
  EXPECT_EQ(coeff2, 3);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
