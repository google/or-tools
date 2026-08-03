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

#include "ortools/sat/constraint_violation.h"

#include <cstdint>
#include <vector>

#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/dump_vars.h"
#include "ortools/base/gmock.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::ElementsAre;

TEST(LinearEvaluatorTest, TestAPI) {
  LinearIncrementalEvaluator evaluator;
  const int c1 = evaluator.NewConstraint({0, 10});
  EXPECT_EQ(c1, 0);
  const int c2 = evaluator.NewConstraint({1, 20});
  EXPECT_EQ(c2, 1);
  evaluator.AddTerm(0, 0, 2);
  EXPECT_TRUE(evaluator.VarIsConsistent(0));
  EXPECT_TRUE(evaluator.VarIsConsistent(1));
  evaluator.AddTerm(0, 0, 3);
  EXPECT_TRUE(evaluator.VarIsConsistent(0));
  evaluator.AddTerm(1, 0, 1);
  EXPECT_TRUE(evaluator.VarIsConsistent(0));
  if (!DEBUG_MODE) {
    evaluator.AddTerm(0, 0, 5);
    EXPECT_FALSE(evaluator.VarIsConsistent(0));
  }
}

TEST(LinearEvaluatorTest, IncrementalScoreComputationForEnforcement) {
  LinearIncrementalEvaluator evaluator;
  const int c = evaluator.NewConstraint({1, 1});
  evaluator.AddEnforcementLiteral(c, PositiveRef(0));
  evaluator.AddEnforcementLiteral(c, NegatedRef(1));
  evaluator.AddEnforcementLiteral(c, PositiveRef(2));
  evaluator.PrecomputeCompactView({1, 1, 1});  // All Booleans.

  std::vector<double> weights{1.0};
  std::vector<int64_t> solution{0, 0, 0};
  std::vector<int64_t> jump_deltas{1, 1, 1};
  std::vector<double> jump_scores(3, 0.0);
  std::vector<int> modified_constraints;

  // For all possible solution, we try all possible move.
  for (int sol = 0; sol < 8; ++sol) {
    for (int move = 0; move < 3; ++move) {
      // Initialize base solution.
      for (int var = 0; var < 3; ++var) {
        solution[var] = ((sol >> var) & 1);
        jump_deltas[var] = (1 ^ solution[var]) - solution[var];
      }
      evaluator.ComputeInitialActivities(solution);
      for (int var = 0; var < 3; ++var) {
        jump_scores[var] =
            evaluator.WeightedViolationDelta(weights, var, jump_deltas[var]);
      }

      // Perform move.
      evaluator.UpdateVariableAndScores(
          move, jump_deltas[move], weights, jump_deltas,
          absl::MakeSpan(jump_scores), &modified_constraints);

      // We never update the score of the flipped variable.
      solution[move] = 1 ^ solution[move];
      jump_deltas[move] = (1 ^ solution[move]) - solution[move];
      jump_scores[move] =
          evaluator.WeightedViolationDelta(weights, move, jump_deltas[move]);

      // Test that the scores are correctly updated.
      for (int test = 0; test < 3; ++test) {
        ASSERT_EQ(jump_scores[test], evaluator.WeightedViolationDelta(
                                         weights, test, jump_deltas[test]))
            << DUMP_VARS(solution) << "\n"
            << DUMP_VARS(move) << "\n"
            << DUMP_VARS(test);
      }
    }
  }
}

TEST(LinearEvaluatorTest, EmptyConstraintDoNotCrash) {
  LinearIncrementalEvaluator evaluator;
  evaluator.NewConstraint({1, 1});
  evaluator.NewConstraint({1, 1});
  evaluator.NewConstraint({1, 1});

  std::vector<int64_t> solution{0, 0, 0};
  std::vector<int64_t> jump_deltas{1, 1, 1};
  std::vector<double> jump_scores(3, 0.0);

  evaluator.PrecomputeCompactView({10, 10, 10});
  evaluator.ComputeInitialActivities(solution);
  evaluator.UpdateScoreOnWeightUpdate(1, jump_deltas,
                                      absl::MakeSpan(jump_scores));
}

TEST(ConstraintViolationTest, BasicExactlyOneExampleNonViolated) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { exactly_one { literals: [ 0, 1, 2, 3 ] } }
  )pb");
  SatParameters params;
  TimeLimit time_limit;
  LsEvaluator ls(model, params, &time_limit);
  ls.ComputeAllViolations({0, 0, 0, 1});
  EXPECT_EQ(0, ls.SumOfViolations());
}

TEST(ConstraintViolationTest, BasicExactlyOneExampleViolated) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { exactly_one { literals: [ 0, 1, 2, 3 ] } }
  )pb");
  SatParameters params;
  TimeLimit time_limit;
  LsEvaluator ls(model, params, &time_limit);
  ls.ComputeAllViolations({0, 0, 1, 1});
  EXPECT_EQ(1, ls.SumOfViolations());
  EXPECT_THAT(ls.ViolatedConstraints(), ElementsAre(0));
  EXPECT_EQ(ls.NumViolatedConstraintsForVarIgnoringObjective(0), 1);
  EXPECT_EQ(ls.NumViolatedConstraintsForVarIgnoringObjective(1), 1);
  EXPECT_EQ(ls.NumViolatedConstraintsForVarIgnoringObjective(2), 1);
  EXPECT_EQ(ls.NumViolatedConstraintsForVarIgnoringObjective(3), 1);
}

TEST(ConstraintViolationTest, BasicBoolOrViolated) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_or { literals: [ 0, -2, 2, -4 ] } }
  )pb");
  SatParameters params;
  TimeLimit time_limit;
  LsEvaluator ls(model, params, &time_limit);
  ls.ComputeAllViolations({0, 1, 0, 1});
  EXPECT_EQ(1, ls.SumOfViolations());
  ls.ComputeAllViolations({0, 0, 0, 1});
  EXPECT_EQ(0, ls.SumOfViolations());
  ls.ComputeAllViolations({0, 1, 0, 0});
  EXPECT_EQ(0, ls.SumOfViolations());
}

TEST(ConstraintViolationTest, BasicLinearExample) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 5 ] }
    constraints {
      linear {
        vars: [ 0, 1 ],
        coeffs: [ 2, 3 ],
        domain: [ 1, 4 ],
      }
    }
  )pb");
  SatParameters params;
  TimeLimit time_limit;
  LsEvaluator ls(model, params, &time_limit);
  ls.ComputeAllViolations({0, 0});
  EXPECT_EQ(1, ls.SumOfViolations());
  ls.ComputeAllViolations({2, 0});
  EXPECT_EQ(0, ls.SumOfViolations());
  ls.ComputeAllViolations({2, 3});
  EXPECT_EQ(9, ls.SumOfViolations());
  EXPECT_EQ(ls.NumViolatedConstraintsForVarIgnoringObjective(0), 1);
  EXPECT_EQ(ls.NumViolatedConstraintsForVarIgnoringObjective(1), 1);
}

TEST(ConstraintViolationTest, BasicObjectiveExampleWithChange) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    objective {
      vars: [ 0, 1, 2, 3 ],
      coeffs: [ 2, 3, 4, 5 ],
    }
  )pb");
  SatParameters params;
  TimeLimit time_limit;
  LsEvaluator ls(model, params, &time_limit);
  ls.ComputeAllViolations({0, 0, 0, 1});
  EXPECT_EQ(0, ls.SumOfViolations());
  EXPECT_EQ(ls.NumViolatedConstraintsForVarIgnoringObjective(0), 0);
  bool reduced = false;
  ls.ReduceObjectiveBounds(0, 3, reduced);
  EXPECT_TRUE(reduced);
  EXPECT_EQ(2, ls.SumOfViolations());
  EXPECT_THAT(ls.ViolatedConstraints(), ElementsAre(0));
  EXPECT_EQ(ls.NumViolatedConstraintsForVarIgnoringObjective(0), 0);
}

TEST(ConstraintViolationTest, BasicBoolXorExample) {
  const ConstraintProto ct_proto =
      ParseTestProto(R"pb(bool_xor { literals: [ 0, -2, 2 ] })pb");
  CompiledBoolXorConstraint ct(ct_proto);
  EXPECT_EQ(0, ct.ComputeViolation({1, 1, 0}));
  EXPECT_EQ(0, ct.ComputeViolation({0, 0, 0}));
  EXPECT_EQ(1, ct.ComputeViolation({1, 0, 0}));
}

TEST(ConstraintViolationTest, ComputeViolationWithEnforcementLiteral) {
  const ConstraintProto ct_proto =
      ParseTestProto(R"pb(enforcement_literal: 0
                          bool_xor { literals: [ 1, 2 ] })pb");
  CompiledBoolXorConstraint ct(ct_proto);
  EXPECT_EQ(0, ct.ComputeViolation({0, 1, 1}));  // Not enforced.
  EXPECT_EQ(1, ct.ComputeViolation({1, 1, 1}));  // Enforced.
}

TEST(ConstraintViolationTest, ViolationDeltaWithEnforcementLiteral) {
  const ConstraintProto ct_proto =
      ParseTestProto(R"pb(enforcement_literal: 0
                          enforcement_literal: 1
                          bool_xor { literals: [ 2, 3 ] })pb");
  CompiledBoolXorConstraint ct(ct_proto);
  ct.InitializeViolation({0, 0, 0, 0});
  EXPECT_EQ(0, ct.violation());

  // Was not enforced and stays unenforced: no change.
  EXPECT_EQ(0, ct.ViolationDelta(0, 0, {1, 0, 1, 1}));
  ct.PerformMove(0, 0, {1, 0, 1, 1});
  EXPECT_EQ(0, ct.violation());

  // Was not enforced and becomes enforced and violated.
  EXPECT_EQ(1, ct.ViolationDelta(1, 0, {1, 1, 1, 1}));
  ct.PerformMove(1, 0, {1, 1, 1, 1});
  EXPECT_EQ(1, ct.violation());

  // Was enforced and violated, becomes unenforced.
  EXPECT_EQ(-1, ct.ViolationDelta(0, 1, {0, 1, 1, 1}));
  ct.PerformMove(0, 1, {0, 1, 1, 1});
  EXPECT_EQ(0, ct.violation());
}

TEST(ConstraintViolationTest, ViolationDeltaWhenEnforced) {
  const ConstraintProto ct_proto =
      ParseTestProto(R"pb(enforcement_literal: 0
                          enforcement_literal: 1
                          bool_xor { literals: [ 2, 3 ] })pb");
  CompiledBoolXorConstraint ct(ct_proto);
  ct.InitializeViolation({1, 1, 0, 1});
  EXPECT_EQ(0, ct.violation());
  EXPECT_EQ(1, ct.ViolationDelta(2, 0, {1, 1, 1, 1}));
}

TEST(ConstraintViolationTest, BasicLinMaxExampleNoViolation) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      lin_max {
        target { vars: 0 coeffs: 2 }
        exprs { vars: 1 coeffs: 1 offset: 1 }
        exprs { vars: 2 coeffs: 4 offset: 1 }
        exprs { offset: 1 }
      }
    }
  )pb");
  SatParameters params;
  TimeLimit time_limit;
  LsEvaluator ls(model, params, &time_limit);
  ls.ComputeAllViolations({1, 1, 0});
  EXPECT_EQ(0, ls.SumOfViolations());
}

TEST(ConstraintViolationTest, BasicLinMaxExampleExcessViolation) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      lin_max {
        target { vars: 0 coeffs: 2 }
        exprs { vars: 1 coeffs: 1 offset: 1 }
        exprs { vars: 2 coeffs: 4 offset: 1 }
        exprs { offset: 1 }
      }
    }
  )pb");
  SatParameters params;
  TimeLimit time_limit;
  LsEvaluator ls(model, params, &time_limit);
  ls.ComputeAllViolations({0, 0, 0});
  EXPECT_EQ(3, ls.SumOfViolations());
}

TEST(ConstraintViolationTest, BasicLinMaxExampleMissingViolation) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      lin_max {
        target { vars: 0 coeffs: 2 }
        exprs { vars: 1 coeffs: 1 offset: 1 }
        exprs { vars: 2 coeffs: 4 offset: 1 }
        exprs { offset: 1 }
      }
    }
  )pb");
  SatParameters params;
  TimeLimit time_limit;
  LsEvaluator ls(model, params, &time_limit);
  ls.ComputeAllViolations({1, 0, 0});
  EXPECT_EQ(1, ls.SumOfViolations());
}

TEST(ConstraintViolationTest, BasicLinMaxExampleNegativeCoeffs) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 40 ] }
    variables { domain: [ 0, 40 ] }
    variables { domain: [ 0, 40 ] }
    constraints {
      lin_max {
        target { vars: 2 coeffs: -1 }
        exprs { vars: 0 coeffs: -1 offset: -1 }
        exprs { vars: 1 coeffs: -1 offset: -1 }
      }
    }
  )pb");
  SatParameters params;
  TimeLimit time_limit;
  LsEvaluator ls(model, params, &time_limit);
  ls.ComputeAllViolations({33, 33, 33});
  EXPECT_EQ(1, ls.SumOfViolations());
}

TEST(ConstraintViolationTest, BasicIntProdExample) {
  const ConstraintProto ct_proto = ParseTestProto(R"pb(
    int_prod {
      target { vars: 0 coeffs: 1 }
      exprs { vars: 1 coeffs: 1 offset: 1 }
      exprs { vars: 2 coeffs: 2 }
    }
  )pb");

  CompiledIntProdConstraint ct(ct_proto);
  EXPECT_EQ(1, ct.ComputeViolation({1, 0, 0}));
  EXPECT_EQ(3, ct.ComputeViolation({1, 0, 2}));
  EXPECT_EQ(2, ct.ComputeViolation({6, 0, 2}));
}

TEST(ConstraintViolationTest, BasicIntDivExample) {
  const ConstraintProto ct_proto = ParseTestProto(R"pb(
    int_div {
      target { vars: 0 coeffs: 1 }
      exprs { vars: 1 coeffs: 1 offset: 1 }
      exprs { vars: 2 coeffs: 1 }
    }
  )pb");
  CompiledIntDivConstraint ct(ct_proto);
  EXPECT_EQ(1, ct.ComputeViolation({0, 1, 2}));
  EXPECT_EQ(3, ct.ComputeViolation({0, 6, 2}));
}

TEST(ConstraintViolationTest, BasicIntModExample) {
  const ConstraintProto ct_proto = ParseTestProto(R"pb(
    int_mod {
      target { vars: 0 coeffs: 1 }
      exprs { vars: 1 coeffs: 1 }
      exprs { vars: 2 coeffs: 1 }
    }
  )pb");
  CompiledIntModConstraint ct(ct_proto);
  EXPECT_EQ(1, ct.ComputeViolation({1, 2, 3}));
  EXPECT_EQ(0, ct.ComputeViolation({1, 7, 3}));
  // Wrap around.
  EXPECT_EQ(2, ct.ComputeViolation({1, 5, 6}));
  EXPECT_EQ(2, ct.ComputeViolation({5, 1, 6}));
  // Across 0.
  EXPECT_EQ(22, ct.ComputeViolation({18, -4, 6}));
  EXPECT_EQ(22, ct.ComputeViolation({-18, 4, 6}));
}

TEST(ConstraintViolationTest, BasicAllDiffExample) {
  const ConstraintProto ct_proto = ParseTestProto(R"pb(
    all_diff {
      exprs { vars: 0 coeffs: 1 }
      exprs { vars: 1 coeffs: 1 }
      exprs { vars: 2 coeffs: 1 }
      exprs { vars: 3 coeffs: 1 }
    }
  )pb");
  CompiledAllDiffConstraint ct(ct_proto);
  EXPECT_EQ(3, ct.ComputeViolation({2, 1, 2, 2}));
  EXPECT_EQ(6, ct.ComputeViolation({2, 2, 2, 2}));
  EXPECT_EQ(1, ct.ComputeViolation({1, 2, 3, 1}));
  EXPECT_EQ(2, ct.ComputeViolation({1, 2, 2, 1}));
}

TEST(ConstraintViolationTest, BasicNoOverlapExample) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 3
      interval {
        start: { vars: 0 coeffs: 1 },
        size: { offset: 4 },
        end: { vars: 0 coeffs: 1 offset: 4 }
      }
    }
    constraints {
      interval {
        start: { vars: 1 coeffs: 1 },
        size: { offset: 4 },
        end: { vars: 1 coeffs: 1 offset: 4 }
      }
    }
    constraints {
      interval {
        start: { vars: 2 coeffs: 1 },
        size: { offset: 4 },
        end: { vars: 2 coeffs: 1 offset: 4 }
      }
    }
    constraints { no_overlap { intervals: [ 0, 1, 2 ] } }
  )pb");

  SatParameters params;
  TimeLimit time_limit;
  LsEvaluator ls(model, params, &time_limit);
  ls.ComputeAllViolations({0, 4, 8, 1});
  EXPECT_EQ(0, ls.SumOfViolations());

  ls.ComputeAllViolations({0, 2, 4, 1});
  EXPECT_EQ(4, ls.SumOfViolations());

  ls.ComputeAllViolations({0, 0, 0, 1});
  EXPECT_EQ(12, ls.SumOfViolations());

  ls.ComputeAllViolations({1, 2, 3, 1});
  EXPECT_EQ(8, ls.SumOfViolations());

  ls.ComputeAllViolations({1, 2, 3, 0});
  EXPECT_EQ(3, ls.SumOfViolations());
}

TEST(ConstraintViolationTest, TwoIntervalsNoOverlapExample) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      enforcement_literal: 2
      interval {
        start: { vars: 0 coeffs: 1 },
        size: { offset: 4 },
        end: { vars: 0 coeffs: 1 offset: 4 }
      }
    }
    constraints {
      interval {
        start: { vars: 1 coeffs: 1 },
        size: { offset: 4 },
        end: { vars: 1 coeffs: 1 offset: 4 }
      }
    }
    constraints { no_overlap { intervals: [ 0, 1 ] } }
  )pb");

  SatParameters params;
  TimeLimit time_limit;
  LsEvaluator ls(model, params, &time_limit);
  ls.ComputeAllViolations({0, 4, 1});
  EXPECT_EQ(0, ls.SumOfViolations());

  ls.ComputeAllViolations({0, 2, 1});
  EXPECT_EQ(2, ls.SumOfViolations());

  ls.ComputeAllViolations({0, 0, 1});
  EXPECT_EQ(4, ls.SumOfViolations());

  ls.ComputeAllViolations({1, 2, 1});
  EXPECT_EQ(3, ls.SumOfViolations());

  ls.ComputeAllViolations({1, 2, 0});
  EXPECT_EQ(0, ls.SumOfViolations());
}

TEST(ConstraintViolationTest, BasicCumulativeExample) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 2, 4 ] }
    constraints {
      enforcement_literal: 3
      interval {
        start: { vars: 0 coeffs: 1 },
        size: { offset: 4 },
        end: { vars: 0 coeffs: 1 offset: 4 }
      }
    }
    constraints {
      interval {
        start: { vars: 1 coeffs: 1 },
        size: { offset: 4 },
        end: { vars: 1 coeffs: 1 offset: 4 }
      }
    }
    constraints {
      interval {
        start: { vars: 2 coeffs: 1 },
        size: { offset: 4 },
        end: { vars: 2 coeffs: 1 offset: 4 }
      }
    }
    constraints {
      cumulative {
        intervals: [ 0, 1, 2 ]
        demands: { offset: 2 }
        demands: { offset: 2 }
        demands: { vars: 4, coeffs: 1 }
        capacity: { vars: 5 coeffs: 1 }
      }
    }
  )pb");

  SatParameters params;
  TimeLimit time_limit;
  LsEvaluator ls(model, params, &time_limit);
  ls.ComputeAllViolations({0, 4, 8, 1, 2, 2});
  EXPECT_EQ(0, ls.SumOfViolations());

  ls.ComputeAllViolations({0, 2, 4, 1, 2, 2});
  EXPECT_EQ(8, ls.SumOfViolations());

  ls.ComputeAllViolations({0, 0, 0, 1, 1, 3});
  EXPECT_EQ(8, ls.SumOfViolations());

  ls.ComputeAllViolations({1, 2, 3, 1, 1, 4});
  EXPECT_EQ(2, ls.SumOfViolations());

  ls.ComputeAllViolations({1, 2, 3, 0, 1, 4});
  EXPECT_EQ(0, ls.SumOfViolations());
}

TEST(ConstraintViolationTest, EmptyNoOverlap) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints { no_overlap {} }
  )pb");

  SatParameters params;
  TimeLimit time_limit;
  LsEvaluator ls(model, params, &time_limit);
  ls.ComputeAllViolations({0, 4, 8});
  EXPECT_EQ(0, ls.SumOfViolations());
}

TEST(ConstraintViolationTest, WeightedViolationAndDelta) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 5 ] }
    constraints {
      linear {
        vars: [ 0, 1 ],
        coeffs: [ 2, 3 ],
        domain: [ 1, 4 ],
      }
    }
    constraints {
      linear {
        vars: [ 0, 1 ],
        coeffs: [ 7, 8 ],
        domain: [ 1, 20 ],
      }
    }
  )pb");

  SatParameters params;
  TimeLimit time_limit;
  LsEvaluator ls(model, params, &time_limit);

  std::vector<int64_t> solution{0, 0};
  std::vector<double> weight{0.0, 0.0};
  for (int i = 0; i < 10; ++i) {
    solution[0] = i;
    for (int j = 1; j < 10; ++j) {
      solution[1] = 0;

      ls.ComputeAllViolations(solution);
      const double delta =
          ls.WeightedViolationDelta(/*linear_only=*/false, weight, 1, j,
                                    absl::MakeSpan(solution));  // 0 -> j
      const double expected = ls.WeightedViolation(weight) + delta;

      solution[1] = j;
      ls.ComputeAllViolations(solution);
      EXPECT_EQ(expected, ls.WeightedViolation(weight));
    }
  }
}

TEST(ConstraintViolationTest, Breakpoints) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 5 ] }
    constraints {
      linear {
        vars: [ 0, 1 ],
        coeffs: [ 2, 3 ],
        domain: [ 1, 4 ],
      }
    }
    constraints {
      linear {
        vars: [ 0, 1 ],
        coeffs: [ 7, 8 ],
        domain: [ 1, 20 ],
      }
    }
  )pb");
  SatParameters params;
  TimeLimit time_limit;
  LsEvaluator ls(model, params, &time_limit);
  ls.ComputeAllViolations({0, 0});

  // We don't want the same value as zero, so we should include both values
  // around it to be sure we don't miss the minimum.
  //
  // breakpoints for the first constraint should be at 0,1 and 2.
  // breakpoints for seconds constraints should be at 0,1 and 2,3.
  EXPECT_THAT(
      ls.MutableLinearEvaluator()->SlopeBreakpoints(0, 0, Domain(-5, 8)),
      ::testing::ElementsAre(-5, 0, 1, 2, 3, 8));
}

TEST(ConstraintViolationTest, BasicCircuit) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }  # 0->1
    variables { domain: [ 0, 1 ] }  # 1->2
    variables { domain: [ 0, 1 ] }  # 1->0
    variables { domain: [ 0, 1 ] }  # 2->0
    variables { domain: [ 0, 1 ] }  # 2->2
    variables { domain: [ 0, 1 ] }  # 0->2
    variables { domain: [ 0, 1 ] }  # 0->0
    variables { domain: [ 0, 1 ] }  # 2->1
    constraints {
      circuit {
        tails: [ 0, 1, 1, 2, 2, 0, 0, 2 ]
        heads: [ 1, 2, 0, 0, 2, 2, 0, 1 ]
        literals: [ 0, 1, 2, 3, 4, 5, 6, 7 ]
      }
    }
  )pb");

  SatParameters params;
  TimeLimit time_limit;
  LsEvaluator ls(model, params, &time_limit);
  ls.ComputeAllViolations({0, 0, 0, 0, 0, 0, 0, 0});
  EXPECT_GE(ls.SumOfViolations(), 1);
  ls.ComputeAllViolations({1, 0, 1, 0, 0, 0, 0, 0});
  EXPECT_GE(ls.SumOfViolations(), 1);
  ls.ComputeAllViolations({1, 0, 1, 0, 1, 0, 0, 0});
  EXPECT_EQ(ls.SumOfViolations(), 0);
  ls.ComputeAllViolations({1, 0, 1, 0, 0, 1, 0, 0});
  EXPECT_GE(ls.SumOfViolations(), 1);
  ls.ComputeAllViolations({1, 0, 1, 1, 0, 1, 0, 0});
  EXPECT_GE(ls.SumOfViolations(), 1);
  ls.ComputeAllViolations({1, 1, 0, 1, 0, 0, 0, 0});
  EXPECT_EQ(ls.SumOfViolations(), 0);
  ls.ComputeAllViolations({0, 1, 0, 0, 0, 0, 1, 1});
  EXPECT_EQ(ls.SumOfViolations(), 0);
}

TEST(ConstraintViolationTest, BasicMultiCircuit) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }  # 0->1
    variables { domain: [ 0, 1 ] }  # 1->2
    variables { domain: [ 0, 1 ] }  # 1->0
    variables { domain: [ 0, 1 ] }  # 2->0
    variables { domain: [ 0, 1 ] }  # 2->2
    variables { domain: [ 0, 1 ] }  # 0->2
    variables { domain: [ 0, 1 ] }  # 2->1
    constraints {
      routes {
        tails: [ 0, 1, 1, 2, 2, 0, 2 ]
        heads: [ 1, 2, 0, 0, 2, 2, 1 ]
        literals: [ 0, 1, 2, 3, 4, 5, 6 ]
      }
    }
  )pb");

  SatParameters params;
  TimeLimit time_limit;
  LsEvaluator ls(model, params, &time_limit);
  ls.ComputeAllViolations({0, 0, 0, 0, 0, 0, 0});
  EXPECT_GE(ls.SumOfViolations(), 1) << "arcs: None";
  ls.ComputeAllViolations({1, 0, 1, 0, 0, 0, 0});
  EXPECT_GE(ls.SumOfViolations(), 1) << "arcs: 0->1;1->0";
  ls.ComputeAllViolations({1, 0, 1, 0, 0, 1, 0});
  EXPECT_GE(ls.SumOfViolations(), 1) << "arcs: 0->1;1->0;0->2";
  ls.ComputeAllViolations({1, 0, 1, 1, 0, 0, 0});
  EXPECT_GE(ls.SumOfViolations(), 1) << "arcs: 0->1;1->0;2->0";
  ls.ComputeAllViolations({1, 0, 1, 0, 1, 0, 0});
  EXPECT_EQ(ls.SumOfViolations(), 0) << "arcs: 0->1;1->0;2->2";
  ls.ComputeAllViolations({1, 0, 1, 1, 0, 1, 0});
  EXPECT_EQ(ls.SumOfViolations(), 0) << "arcs: 0->1;1->0;0->2;2->0";
  ls.ComputeAllViolations({1, 0, 1, 1, 0, 1, 0});
  EXPECT_EQ(ls.SumOfViolations(), 0) << "arcs: 0->1;1->0;0->2;2->0";
  ls.ComputeAllViolations({0, 1, 0, 0, 0, 0, 1});
  EXPECT_GE(ls.SumOfViolations(), 1) << "arcs: 1->2;2->1";
}

TEST(ConstraintViolationTest, LastUpdateViolationChanges) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 4 ] }
    variables { domain: [ 0, 5 ] }
    variables { domain: [ 0, 20 ] }
    constraints {
      linear {
        vars: [ 0, 1 ],
        coeffs: [ 2, 3 ],
        domain: [ 1, 4 ],
      }
    }
    constraints {
      int_prod {
        target { vars: 2 coeffs: 1 }
        exprs { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  SatParameters params;
  TimeLimit time_limit;
  LsEvaluator ls(model, params, &time_limit);
  std::vector<double> unused_jump_scores = {0.0, 0.0, 0.0};

  std::vector<int64_t> solution = {2, 1, 3};
  ls.ComputeAllViolations(solution);

  solution[0] = 3;
  ls.UpdateLinearScores(0, 2, 3, /*weights=*/{1.0, 1.0},
                        /*jump_deltas=*/{-2, -1, -3},
                        absl::MakeSpan(unused_jump_scores));
  ls.UpdateNonLinearViolations(0, 2, solution);
  EXPECT_THAT(ls.last_update_violation_changes(), ElementsAre(0, 1));

  solution[2] = 2;
  ls.UpdateLinearScores(2, 3, 2, /*weights=*/{1.0, 1.0},
                        /*jump_deltas=*/{-2, -1, 3},
                        absl::MakeSpan(unused_jump_scores));
  ls.UpdateNonLinearViolations(2, 3, solution);
  EXPECT_THAT(ls.last_update_violation_changes(), ElementsAre(1));
}

// Simple test to check that empty active field and all-1 active fields are
// encoded correctly, because they are special cases.
TEST(ConstraintViolationTest, ReservoirEmptyOrAllOneActive) {
  for (const bool empty_active_field : {true, false}) {
    CpModelProto model = ParseTestProto(R"pb(
      variables { domain: [ 0, 10 ] }
      variables { domain: [ 0, 10 ] }
      variables { domain: [ 1, 1 ] }
      constraints {
        reservoir {
          min_level: 0
          max_level: 1
          time_exprs {
            vars: [ 0 ]
            coeffs: [ 1 ]
            offset: 0
          }
          time_exprs {
            vars: [ 1 ]
            coeffs: [ 1 ]
            offset: 0
          }
          level_changes { offset: 2 }
          level_changes { offset: -1 }
          active_literals: [ 2, 2 ]
        }
      }
    )pb");
    SatParameters params;
    TimeLimit time_limit;
    if (empty_active_field) {
      model.mutable_constraints(0)
          ->mutable_reservoir()
          ->clear_active_literals();
    }
    LsEvaluator ls(model, params, &time_limit);

    // If t0 = 1, t1 = 2:
    // level after event 0 is 2 -> exceeds max_level=1 -> violated!
    ls.ComputeAllViolations({1, 2});
    EXPECT_EQ(ls.SumOfViolations(), 1);

    // If t0 = 2, t1 = 1:
    // t1 first: level is -1 -> violates min_level=0 -> violated!
    ls.ComputeAllViolations({2, 1});
    EXPECT_EQ(ls.SumOfViolations(), 1);
  }
}

TEST(ConstraintViolationTest, ReservoirFixedChanges) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }    # t0: var 0
    variables { domain: [ 0, 10 ] }    # t1: var 1
    variables { domain: [ -10, 10 ] }  # d0: var 2
    variables { domain: [ -10, 10 ] }  # d1: var 3
    variables { domain: [ 0, 1 ] }     # a0: var 4
    variables { domain: [ 0, 1 ] }     # a1: var 5
    constraints {
      reservoir {
        min_level: -2
        max_level: 5
        time_exprs {
          vars: [ 0 ]
          coeffs: [ 1 ]
          offset: 0
        }
        time_exprs {
          vars: [ 1 ]
          coeffs: [ 1 ]
          offset: 0
        }
        level_changes {
          vars: [ 2 ]
          coeffs: [ 1 ]
          offset: 0
        }
        level_changes {
          vars: [ 3 ]
          coeffs: [ 1 ]
          offset: 0
        }
        active_literals: [ 4, 5 ]
      }
    }
  )pb");
  SatParameters params;
  TimeLimit time_limit;
  LsEvaluator ls(model, params, &time_limit);

  // 1. Feasible case: t0 = 1, t1 = 3, d0 = 3, d1 = -4, a0 = 1, a1 = 1.
  // Profile: 3@1, -1@3.
  ls.ComputeAllViolations({1, 3, 3, -4, 1, 1});
  EXPECT_EQ(ls.SumOfViolations(), 0);

  // 2. Max level violation: t0 = 1, t1 = 3, d0 = 7, d1 = -4, a0 = 1, a1 = 1.
  // Profile: 7@1, 3@3.
  // Duration: 3 - 1 = 2.
  // Violation: 2 (overload) * 2 (duration) = 4.
  ls.ComputeAllViolations({1, 3, 7, -4, 1, 1});
  EXPECT_EQ(ls.SumOfViolations(), 4);

  // 3. Min level violation: t0 = 1, t1 = 3, d0 = -4, d1 = 3, a0 = 1, a1 = 1.
  // Profile: -4@1, -1@3.
  // Duration: 3 - 1 = 2.
  // Violation: 2 (underload) * 2 (duration) = 4.
  ls.ComputeAllViolations({1, 3, -4, 3, 1, 1});
  EXPECT_EQ(ls.SumOfViolations(), 4);

  // 4. Inactive events: t0 = 1, t1 = 3, d0 = 7, d1 = -4, a0 = 0, a1 = 1.
  // Profile: -4@3.
  // Event 0 is inactive, so its level change is ignored.
  // Level at t=3: -4 (below min_level=-2 by 2).
  // Time profile check ignores after t=3 -> 0 violation.
  // Final level check -> 2 violation.
  // Total violation: 2.
  ls.ComputeAllViolations({1, 3, 7, -4, 0, 1});
  EXPECT_EQ(ls.SumOfViolations(), 2);
}

TEST(ConstraintViolationTest, ReservoirExhaustiveThreeEvents) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }    # t0
    variables { domain: [ 0, 10 ] }    # t1
    variables { domain: [ 0, 10 ] }    # t2
    variables { domain: [ -20, 20 ] }  # d0
    variables { domain: [ -20, 20 ] }  # d1
    variables { domain: [ -20, 20 ] }  # d2
    variables { domain: [ 0, 1 ] }     # a0
    variables { domain: [ 0, 1 ] }     # a1
    variables { domain: [ 0, 1 ] }     # a2
    constraints {
      reservoir {
        min_level: 0
        max_level: 10
        time_exprs {
          vars: [ 0 ]
          coeffs: [ 1 ]
          offset: 0
        }
        time_exprs {
          vars: [ 1 ]
          coeffs: [ 1 ]
          offset: 0
        }
        time_exprs {
          vars: [ 2 ]
          coeffs: [ 1 ]
          offset: 0
        }
        level_changes {
          vars: [ 3 ]
          coeffs: [ 1 ]
          offset: 0
        }
        level_changes {
          vars: [ 4 ]
          coeffs: [ 1 ]
          offset: 0
        }
        level_changes {
          vars: [ 5 ]
          coeffs: [ 1 ]
          offset: 0
        }
        active_literals: [ 6, 7, 8 ]
      }
    }
  )pb");
  SatParameters params;
  TimeLimit time_limit;
  LsEvaluator ls(model, params, &time_limit);

  // t0 = 1, t1 = 3, t2 = 6, d0 = 12, d1 = -4, d2 = -3, a0 = 1, a1 = 1, a2 = 1.
  // Profile: 12@1, 8@3, 5@6.
  // Interval [1, 3): level=12, overload=2, duration=2. Violation = 4.
  // Interval [3, 6): level=8, no overload.
  ls.ComputeAllViolations({1, 3, 6, 12, -4, -3, 1, 1, 1});
  EXPECT_EQ(ls.SumOfViolations(), 4);

  // If a1 = 0 (event 1 inactive):
  // Profile: 12@1, 9@6.
  // Active events at t=1, t=6.
  // Interval [1, 6): level=12, overload=2, duration=5. Violation = 10.
  ls.ComputeAllViolations({1, 3, 6, 12, -4, -3, 1, 0, 1});
  EXPECT_EQ(ls.SumOfViolations(), 10);
}

TEST(ConstraintViolationTest, ReservoirFinalLevelViolation) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }  # t0: var 0
    variables { domain: [ 0, 10 ] }  # t1: var 1
    constraints {
      reservoir {
        min_level: 0
        max_level: 10
        time_exprs {
          vars: [ 0 ]
          coeffs: [ 1 ]
          offset: 0
        }
        time_exprs {
          vars: [ 1 ]
          coeffs: [ 1 ]
          offset: 0
        }
        level_changes { offset: 4 }
        level_changes { offset: 7 }
      }
    }
  )pb");
  SatParameters params;
  TimeLimit time_limit;
  LsEvaluator ls(model, params, &time_limit);

  // t0 = 1, t1 = 3.
  // Profile: 4@1, 11@3.
  // Final level lies outside max_level bounds (11 > 10).
  // Time-profile violation: 0 (since t=3 is the last event).
  // Final level violation: 1 (overload).
  // Total violation: 1.
  ls.ComputeAllViolations({1, 3});
  EXPECT_EQ(ls.SumOfViolations(), 1);

  // If min_level is violated at the end:
  // d0 = -4, d1 = 1, min_level = -2.
  // Profile: -4@1, -3@3.
  // Final level is -3 which is below min_level=-2 by 1.
  // Time-profile violation: 4 (overload on interval [1, 3) is 2 * 2 = 4).
  // Final level violation: 1 (underload).
  // Total violation: 5.
  const CpModelProto model_min = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      reservoir {
        min_level: -2
        max_level: 10
        time_exprs {
          vars: [ 0 ]
          coeffs: [ 1 ]
          offset: 0
        }
        time_exprs {
          vars: [ 1 ]
          coeffs: [ 1 ]
          offset: 0
        }
        level_changes { offset: -4 }
        level_changes { offset: 1 }
      }
    }
  )pb");
  LsEvaluator ls_min(model_min, params, &time_limit);
  ls_min.ComputeAllViolations({1, 3});
  EXPECT_EQ(ls_min.SumOfViolations(), 5);
}

TEST(ConstraintViolationTest, ScalarProductConstraint) {
  // We store the x and y expression lists inside a single CpModelProto (in two
  // lin_max constraints) so that we can parse everything at once and extract
  // them to test CompiledScalarProductConstraint directly.
  // Scalar product:
  // x0 * y0 + x1 * y1 = solution[0] * 5 + solution[1] * solution[2]
  // min_value = 10, max_value = 20
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      lin_max {
        exprs {
          vars: [ 0 ]
          coeffs: [ 1 ]
        }
        exprs {
          vars: [ 1 ]
          coeffs: [ 1 ]
        }
      }
    }
    constraints {
      lin_max {
        exprs { offset: 5 }
        exprs {
          vars: [ 2 ]
          coeffs: [ 1 ]
        }
      }
    }
  )pb");
  std::vector<int> enforcement_literals = {3};
  const auto& x_exprs = model.constraints(0).lin_max().exprs();
  const auto& y_exprs = model.constraints(1).lin_max().exprs();
  std::vector<LinearExpressionProto> x(x_exprs.begin(), x_exprs.end());
  std::vector<LinearExpressionProto> y(y_exprs.begin(), y_exprs.end());

  // Constraint: var_3 -> (10 <= var_0 * 5 + var_1 * var_2 <= 20).
  CompiledScalarProductConstraint ct(enforcement_literals, x, y, 10, 20);

  // Scalar product: 2 * 5 + 3 * 4 = 22. Violation is 22 - 20 = 2.
  EXPECT_EQ(ct.ComputeViolation({2, 3, 4, 1}), 2);

  // Same, not enforced. No violation.
  EXPECT_EQ(ct.ComputeViolation({2, 3, 4, 0}), 0);

  // Scalar product: 1 * 5 + 1 * 3 = 8. Violation is 10 - 8 = 2.
  EXPECT_EQ(ct.ComputeViolation({1, 1, 3, 1}), 2);

  // Scalar product: 2 * 5 + 2 * 3 = 16. No violation.
  EXPECT_EQ(ct.ComputeViolation({2, 2, 3, 1}), 0);

  // Test incremental updates.
  // Start with solution {2, 2, 3, 1}. Scalar product = 16. No violation.
  ct.InitializeViolation({2, 2, 3, 1});
  EXPECT_EQ(ct.violation(), 0);

  // Move var 0 from 2 to 3, new solution is {3, 2, 3, 1}.
  // New scalar product: 3 * 5 + 2 * 3 = 21. New violation: 21 - 20 = 1.
  EXPECT_EQ(ct.ViolationDelta(0, 2, {3, 2, 3, 1}), 1);
  ct.PerformMove(0, 2, {3, 2, 3, 1});
  EXPECT_EQ(ct.violation(), 1);

  // Move var 1 from 2 to 1, new solution is {3, 1, 3, 1}.
  // New scalar product: 18. New violation: 0.
  EXPECT_EQ(ct.ViolationDelta(1, 2, {3, 1, 3, 1}), -1);
  ct.PerformMove(1, 2, {3, 1, 3, 1});
  EXPECT_EQ(ct.violation(), 0);

  // Move var 0 from 3 to 1, new solution is {1, 1, 3, 1}.
  // New scalar product: 8. New violation: 10 - 8 = 2.
  EXPECT_EQ(ct.ViolationDelta(0, 3, {1, 1, 3, 1}), 2);
  ct.PerformMove(0, 3, {1, 1, 3, 1});
  EXPECT_EQ(ct.violation(), 2);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
