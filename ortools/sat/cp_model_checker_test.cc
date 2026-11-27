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

#include "ortools/sat/cp_model_checker.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/base/types.h"
#include "ortools/sat/cp_model.pb.h"

namespace operations_research {
namespace sat {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::HasSubstr;

// This just checks that the code is at least properly executed.
TEST(SolutionIsFeasibleTest, BasicExample) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { name: 'x' domain: 0 domain: 10 }
    variables { name: 'y' domain: 0 domain: 10 }
    constraints {
      linear { vars: 0 coeffs: 1 vars: 1 coeffs: 1 domain: 0 domain: 10 }
    }
  )pb");
  EXPECT_FALSE(SolutionIsFeasible(model, {8, 8}));
  EXPECT_FALSE(SolutionIsFeasible(model, {11, -1}));
  EXPECT_TRUE(SolutionIsFeasible(model, {5, 5}));
}

TEST(SolutionIsFeasibleTest, LinMax) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { name: 'x' domain: 0 domain: 10 }
    variables { name: 'y' domain: 0 domain: 30 }
    constraints {
      lin_max {
        target { vars: 1 coeffs: 1 offset: 0 }
        exprs { vars: 0 coeffs: 2 offset: 1 }
        exprs { vars: 0 coeffs: 3 offset: -5 }
      }
    }
  )pb");
  EXPECT_FALSE(SolutionIsFeasible(model, {2, 4}));
  EXPECT_FALSE(SolutionIsFeasible(model, {11, -1}));
  EXPECT_TRUE(SolutionIsFeasible(model, {2, 5}));
  EXPECT_TRUE(SolutionIsFeasible(model, {8, 19}));
}

TEST(SolutionIsFeasibleTest, OrToolsIssue3769) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 1, 2 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      no_overlap_2d {
        x_intervals: [ 1, 2 ]
        y_intervals: [ 3, 4 ]
      }
    }
    constraints {
      interval {
        start { offset: 2 }
        end {
          vars: [ 1 ]
          coeffs: [ 1 ]
          offset: 2
        }
        size {
          vars: [ 1 ]
          coeffs: [ 1 ]
        }
      }
    }
    constraints {
      interval {
        start { offset: 1 }
        end { offset: 3 }
        size { offset: 2 }
      }
    }
    constraints {
      interval {
        start { offset: 1 }
        end {
          vars: [ 0 ]
          coeffs: [ 1 ]
          offset: 1
        }
        size {
          vars: [ 0 ]
          coeffs: [ 1 ]
        }
      }
    }
    constraints {
      interval {
        start { offset: 2 }
        end { offset: 2 }
        size {}
      }
    }
  )pb");
  EXPECT_TRUE(SolutionIsFeasible(model, {1, 0}));
  EXPECT_TRUE(SolutionIsFeasible(model, {1, 1}));
  EXPECT_FALSE(SolutionIsFeasible(model, {2, 0}));
}

TEST(SolutionIsFeasibleTest, LegacyInverse) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      inverse {
        f_direct: 0
        f_direct: 1
        f_direct: 2
        f_inverse: 3
        f_inverse: 4
        f_inverse: 5
      }
    }
  )pb");
  EXPECT_TRUE(SolutionIsFeasible(model, {2, 0, 1, 1, 2, 0}));
  EXPECT_FALSE(SolutionIsFeasible(model, {2, 0, 1, 1, 0, 2}));
}

TEST(SolutionIsFeasibleTest, Inverse) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    constraints {
      inverse {
        f_expr_direct: { vars: 0 coeffs: -1 offset: 3 }
        f_expr_direct: { vars: 1 coeffs: 1 offset: -1 }
        f_expr_direct: { vars: 2 coeffs: 3 offset: -2 }
        f_expr_inverse: { vars: 0 coeffs: 1 offset: 0 }
        f_expr_inverse: { vars: 1 coeffs: 1 offset: 1 }
        f_expr_inverse: { vars: 2 coeffs: -2 offset: 2 }
      }
    }
  )pb");
  EXPECT_TRUE(SolutionIsFeasible(model, {1, 1, 1}));
  EXPECT_FALSE(SolutionIsFeasible(model, {1, 0, 2}));
}

TEST(SolutionIsFeasibleTest, Reservoir) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 1, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      reservoir {
        time_exprs: { vars: 0 coeffs: 1 }
        time_exprs: { vars: 1 coeffs: 1 }
        level_changes: { offset: -1 }
        level_changes: { offset: 1 }
        active_literals: [ 2, 3 ]
        min_level: 0
        max_level: 2
      }
    }
  )pb");
  EXPECT_FALSE(SolutionIsFeasible(model, {0, 0, 1, 0}));
  EXPECT_TRUE(SolutionIsFeasible(model, {0, 0, 1, 1}));
  EXPECT_TRUE(SolutionIsFeasible(model, {1, 0, 1, 1}));
  EXPECT_FALSE(SolutionIsFeasible(model, {0, 1, 1, 1}));
  EXPECT_FALSE(SolutionIsFeasible(model, {0, 0, 1, 0}));
}

TEST(SolutionIsFeasibleTest, ReservoirWithNegativeTime) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ -2, 2 ] }
    variables { domain: [ -2, 2 ] }
    constraints {
      reservoir {
        time_exprs: { vars: 0 coeffs: 1 }
        time_exprs: { vars: 1 coeffs: 1 }
        level_changes: { offset: 2 }
        level_changes: { offset: -2 }
        min_level: 0
        max_level: 2
      }
    }
  )pb");
  EXPECT_TRUE(SolutionIsFeasible(model, {1, 1}));
  EXPECT_TRUE(SolutionIsFeasible(model, {0, 0}));
  EXPECT_FALSE(SolutionIsFeasible(model, {1, 0}));
  EXPECT_TRUE(SolutionIsFeasible(model, {0, 1}));
  EXPECT_TRUE(SolutionIsFeasible(model, {-2, 2}));
}

TEST(SolutionIsFeasibleTest, SelfArcAreOk) {
  // The literal -1 is the negation of the first variable.
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      circuit {
        literals: [ -1, 1, 2, 3, 0 ]
        tails: [ 0, 1, 2, 3, 0 ]
        heads: [ 0, 2, 3, 1, 2 ]
      }
    }
  )pb");
  EXPECT_TRUE(SolutionIsFeasible(model, {0, 1, 1, 1}));
  EXPECT_FALSE(SolutionIsFeasible(model, {1, 1, 1, 1}));
}

TEST(SolutionIsFeasibleTest, SparseCircuit) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      circuit {
        literals: [ 0, 1, 2, 3 ]
        tails: [ -10, 10, 9, 1000 ]
        heads: [ 10, 9, 1000, -10 ]
      }
    }
  )pb");
  EXPECT_TRUE(SolutionIsFeasible(model, {1, 1, 1, 1}));
  EXPECT_FALSE(SolutionIsFeasible(model, {1, 0, 1, 1}));
}

TEST(SolutionIsFeasibleTest, BoolXor) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints { bool_xor { literals: [ 0, 1, 2, 3 ] } }
  )pb");
  EXPECT_TRUE(SolutionIsFeasible(model, {1, 0, 0, 0}));
  EXPECT_TRUE(SolutionIsFeasible(model, {1, 1, 1, 0}));
  EXPECT_FALSE(SolutionIsFeasible(model, {1, 1, 1, 1}));
  EXPECT_FALSE(SolutionIsFeasible(model, {1, 0, 1, 0}));
}

TEST(SolutionIsFeasibleTest, WithEnforcement) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { name: 'a' domain: 0 domain: 1 }
    variables { name: 'b' domain: 0 domain: 1 }
    variables { name: 'y' domain: 0 domain: 10 }
    constraints {
      enforcement_literal: [ 0, 1 ]
      linear { vars: 2 coeffs: 1 domain: 7 domain: 7 }
    }
  )pb");
  EXPECT_TRUE(SolutionIsFeasible(model, {0, 0, 5}));
  EXPECT_TRUE(SolutionIsFeasible(model, {0, 1, 5}));
  EXPECT_TRUE(SolutionIsFeasible(model, {1, 0, 5}));
  EXPECT_FALSE(SolutionIsFeasible(model, {1, 1, 5}));
  EXPECT_TRUE(SolutionIsFeasible(model, {1, 1, 7}));
}

TEST(SolutionIsFeasibleTest, ObjectiveDomain) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { name: 'x' domain: 0 domain: 10 }
    variables { name: 'y' domain: 0 domain: 10 }
    objective {
      vars: [ 0, 1 ]
      coeffs: [ 1, 1 ]
      domain: [ 5, 15 ]
    }
  )pb");
  EXPECT_FALSE(SolutionIsFeasible(model, {8, 8}));
  EXPECT_TRUE(SolutionIsFeasible(model, {5, 5}));
  EXPECT_FALSE(SolutionIsFeasible(model, {0, 0}));
}

TEST(ValidateCpModelTest, BadVariableDomain1) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { name: 'a' domain: 0 domain: 1 domain: 3 }
  )pb");
  EXPECT_THAT(ValidateCpModel(model), HasSubstr("odd domain"));
}

TEST(ValidateCpModelTest, VariableUpperBoundTooLarge) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables {
      name: 'a'
      domain: [ 0, 9223372036854775807 ]
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(model), HasSubstr("does not fall in"));
}

TEST(ValidateCpModelTest, VariableLowerBoundTooLarge1) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables {
      name: 'a'
      domain: [ -9223372036854775807, 0 ]
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(model), HasSubstr("does not fall in"));
}

TEST(ValidateCpModelTest, VariableLowerBoundTooLarge2) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables {
      name: 'a'
      domain: [ -9223372036854775808, 0 ]
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(model), HasSubstr("does not fall in"));
}

TEST(ValidateCpModelTest, VariableDomainOverflow) {
  CHECK_EQ(kint64max / 2, int64_t{4611686018427387903});

  const CpModelProto model_ok = ParseTestProto(R"pb(
    variables {
      name: 'a'
      domain: -2305843009213693951
      domain: 2305843009213693952
    }
  )pb");
  EXPECT_TRUE(ValidateCpModel(model_ok).empty());

  // max(0, UB) - min(0, LB) > kint64max / 2.
  const CpModelProto model_too_large = ParseTestProto(R"pb(
    variables { name: 'a' domain: -1 domain: 4611686018427387903 }
  )pb");
  EXPECT_THAT(ValidateCpModel(model_too_large), HasSubstr("too large"));

  const CpModelProto model_bad0 = ParseTestProto(R"pb(
    variables { name: 'a' domain: 0 domain: 4611686018427387904 }
  )pb");
  EXPECT_THAT(ValidateCpModel(model_bad0), HasSubstr("does not fall in"));

  const CpModelProto model_bad1 = ParseTestProto(R"pb(
    variables { name: 'a' domain: -4611686018427387904 domain: 0 }
  )pb");
  EXPECT_THAT(ValidateCpModel(model_bad1), HasSubstr("does not fall in"));

  CHECK_EQ(kint64min + 2, int64_t{-9223372036854775806});
  const CpModelProto model_bad2 = ParseTestProto(R"pb(
    variables { name: 'a' domain: -9223372036854775806 domain: 2 }
  )pb");
  EXPECT_THAT(ValidateCpModel(model_bad2), HasSubstr("does not fall in"));
}

TEST(ValidateCpModelTest, LinearActivityRangeOverflow) {
  // The activity of x + w is in [-kint64max / 2, 1]. Both bounds are within
  // [-kint64max / 2, kint64max / 2], but their difference is not.
  const CpModelProto model_bad = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ -4611686018427387903, 0 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ -4611686018427387903, 0 ]
      }
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(model_bad), HasSubstr("overflow"));

  // The activity of x + w is in [1 - kint64max / 2, 1].
  const CpModelProto model_ok = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ -4611686018427387902, 0 ] }
    constraints {
      linear {
        vars: [ 0, 1 ]
        coeffs: [ 1, 1 ]
        domain: [ -4611686018427387902, 0 ]
      }
    }
  )pb");
  EXPECT_EQ(ValidateCpModel(model_ok), "");
}

TEST(ValidateCpModelTest, LinearExpressionOffsetOverflow) {
  // The partial sums of y - 2e18 with y in [0, 2.7e18] are in [-2e18, 2.7e18],
  // which is included in [-kint64max / 2, kint64max / 2] (although its width
  // 4.7e18 is larger than kint64max / 2).
  const CpModelProto model_ok = ParseTestProto(R"pb(
    variables { domain: [ 0, 2700000000000000000 ] }
    constraints {
      all_diff {
        exprs {
          vars: [ 0 ]
          coeffs: [ 1 ]
          offset: -2000000000000000000
        }
      }
    }
  )pb");
  EXPECT_EQ(ValidateCpModel(model_ok), "");

  // y - 6e18 with y in [2e18, 4e18] has values in [-4e18, -2e18], but its
  // offset is not in [-kint64max / 2, kint64max / 2].
  const CpModelProto model_bad_offset = ParseTestProto(R"pb(
    variables { domain: [ 2000000000000000000, 4000000000000000000 ] }
    constraints {
      all_diff {
        exprs {
          vars: [ 0 ]
          coeffs: [ 1 ]
          offset: -6000000000000000000
        }
      }
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(model_bad_offset), HasSubstr("overflow"));

  // y + 3e18 with y in [0, 2e18] has values in [3e18, 5e18].
  const CpModelProto model_bad_max = ParseTestProto(R"pb(
    variables { domain: [ 0, 2000000000000000000 ] }
    constraints {
      all_diff {
        exprs {
          vars: [ 0 ]
          coeffs: [ 1 ]
          offset: 3000000000000000000
        }
      }
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(model_bad_max), HasSubstr("overflow"));

  // In an interval, start + size - end combines three offsets: 3e18 + 3e18
  // exceeds kint64max / 2, but subtracting end's 3e18 offset brings the
  // combined offset back to 3e18 <= kint64max / 2.
  const CpModelProto interval_ok = ParseTestProto(R"pb(
    variables { domain: [ -2000000000000000000, 0 ] }
    variables { domain: [ -2000000000000000000, 0 ] }
    constraints {
      interval {
        start {
          vars: [ 0 ]
          coeffs: [ 1 ]
          offset: 3000000000000000000
        }
        size {
          vars: [ 1 ]
          coeffs: [ 1 ]
          offset: 3000000000000000000
        }
        end { offset: 3000000000000000000 }
      }
    }
  )pb");
  EXPECT_EQ(ValidateCpModel(interval_ok), "");

  // When start.offset + size.offset - end.offset overflows int64_t (4e18 +
  // 4e18 - (-4e18) = 12e18), the interval must be rejected rather than wrapping
  // around.
  const CpModelProto interval_bad = ParseTestProto(R"pb(
    constraints {
      interval {
        start { offset: 4000000000000000000 }
        size { offset: 4000000000000000000 }
        end { offset: -4000000000000000000 }
      }
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(interval_bad), HasSubstr("overflow"));
}

TEST(LinearOverflowCheckerTest, TermContributions) {
  using Pair = std::pair<int64_t, int64_t>;
  EXPECT_EQ(LinearOverflowChecker::TermContributions(3, 2, 5), Pair(0, 15));
  EXPECT_EQ(LinearOverflowChecker::TermContributions(-3, 2, 5), Pair(-15, 0));
  EXPECT_EQ(LinearOverflowChecker::TermContributions(2, -4, 5), Pair(-8, 10));
  EXPECT_EQ(LinearOverflowChecker::TermContributions(-2, -4, 5), Pair(-10, 8));
  EXPECT_EQ(LinearOverflowChecker::TermContributions(0, -4, 5), Pair(0, 0));
  EXPECT_EQ(LinearOverflowChecker::TermContributions(kint64min, 0, 1),
            std::nullopt);
  EXPECT_EQ(LinearOverflowChecker::TermContributions(2, 0, kint64max / 2 + 1),
            std::nullopt);
}

TEST(LinearOverflowCheckerTest, SumsAreValid) {
  const int64_t kHalfMax = kint64max / 2;
  EXPECT_TRUE(LinearOverflowChecker::SumsAreValid(0, kHalfMax));
  EXPECT_TRUE(LinearOverflowChecker::SumsAreValid(-kHalfMax, 0));
  EXPECT_TRUE(LinearOverflowChecker::SumsAreValid(-1, kHalfMax - 1));
  EXPECT_FALSE(LinearOverflowChecker::SumsAreValid(0, kHalfMax + 1));
  EXPECT_FALSE(LinearOverflowChecker::SumsAreValid(-kHalfMax - 1, 0));
  EXPECT_FALSE(LinearOverflowChecker::SumsAreValid(-1, kHalfMax));
}

TEST(ValidateCpModelTest, ObjectiveOverflow) {
  CHECK_EQ(kint64max / 4, int64_t{2305843009213693951});
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ -2305843009213693951, 2305843009213693951 ] }
    variables { domain: [ -2305843009213693951, 2305843009213693951 ] }
    variables { domain: [ -2305843009213693951, 2305843009213693951 ] }
    objective {
      vars: [ 0, 1, 2 ]
      coeffs: [ 1, 1, 1 ]
    }
  )pb");

  // The min/max sum does not overflow, but their difference does.
  EXPECT_THAT(ValidateCpModel(model), HasSubstr("overflow"));
}

TEST(ValidateCpModelTest, ValidSolutionHint) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    solution_hint {
      vars: [ 0, 1 ]
      values: [ 1, 2 ]
    }
  )pb");
  EXPECT_TRUE(ValidateCpModel(model).empty());
}

TEST(ValidateCpModelTest, SolutionHint1) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    solution_hint {
      vars: [ 0, 1, 2 ]
      values: [ 1, 2, 3, 4 ]
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(model), HasSubstr("same size"));
}

TEST(ValidateCpModelTest, SolutionHint2) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    solution_hint {
      vars: [ 0, 10, 2 ]
      values: [ 1, 2, 3 ]
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(model), HasSubstr("Invalid variable"));
}

TEST(ValidateCpModelTest, SolutionHint3) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    solution_hint {
      vars: [ 0, 2, 0 ]
      values: [ 1, 2, 3 ]
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(model), HasSubstr("duplicate"));
}

TEST(ValidateCpModelTest, Assumptions) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    assumptions: [ 0, 1, 4 ]
  )pb");
  EXPECT_THAT(ValidateCpModel(model),
              "Invalid literal reference 4 in the 'assumptions' field.");
}

TEST(ValidateCpModelTest, NegativeValueInIntervalSizeDomain) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 0 ] }
    variables { domain: [ -7, -7, 0, 0 ] }
    constraints {
      interval {
        start { vars: 0 coeffs: 1 }
        end { vars: 1 coeffs: 1 }
        size { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(model),
              HasSubstr("The size of a performed interval must be >= 0"));
}

TEST(ValidateCpModelTest, ParallelVectorMustHaveTheSameSize) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 4503599627370529 }
    constraints {
      interval {
        start { offset: 1 }
        size { offset: 2 }
        end { offset: 3 }
      }
    }
    constraints {
      no_overlap_2d { x_intervals: 0 y_intervals: 0 y_intervals: 0 }
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(model), HasSubstr("must have the same size"));
}

TEST(ValidateCpModelTest, InvalidDomainInLinear) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: -288230376151711744 domain: 262144 }
    variables { domain: 0 domain: 5 }
    constraints {
      linear {
        vars: [ 1, 0 ]
        coeffs: [ 1, 2 ]
        domain: [ 1, 3, 5 ]
      }
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(model), HasSubstr("Invalid domain"));
}

TEST(ValidateCpModelTest, InvalidDomainInLinear2) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: -288230376151711744 domain: 262144 }
    variables { domain: 0 domain: 5 }
    constraints {
      name: "T"
      linear {
        vars: [ 1, 0 ]
        coeffs: [ 1, 2 ]
        domain: [ 3, 0 ]
      }
    }
  )pb");

  EXPECT_THAT(ValidateCpModel(model), HasSubstr("Invalid domain"));
}

TEST(ValidateCpModelTest, NegatedReferenceInLinear) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { name: "c" domain: 1 domain: 1 }
    variables { domain: 0 domain: 1 }
    constraints {
      int_div {
        target {}
        exprs {}
        exprs { vars: -2 coeffs: 792633495762501632 }
      }
    }
  )pb");

  EXPECT_THAT(ValidateCpModel(model), HasSubstr("Invalid negated variable"));
}

TEST(ValidateCpModelTest, ArityOneInIntProd) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      int_prod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  EXPECT_TRUE(ValidateCpModel(model).empty());
}

TEST(ValidateCpModelTest, ArityThreeInIntProd) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      int_prod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  EXPECT_TRUE(ValidateCpModel(model).empty());
}

TEST(ValidateCpModelTest, WrongArityInIntDiv) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      int_div {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(model), HasSubstr("have exactly 2 terms"));
}

TEST(ValidateCpModelTest, DivisorDomainContainsZero) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ -3, 3 ] }
    constraints {
      int_div {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(model),
              HasSubstr("The domain of the divisor cannot contain 0"));
}

TEST(ValidateCpModelTest, DivisorSpanningAcrossZero) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ -3, 3 ] }
    constraints {
      int_div {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 2 offset: -3 }
      }
    }
  )pb");
  EXPECT_TRUE(ValidateCpModel(model).empty());
}

TEST(ValidateCpModelTest, TwoTermDivisorNeverZero) {
  // a + b with a in {-2, 2} and b in {-1, 1} is never 0, even though 0 lies
  // between its min and max.
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ -2, -2, 2, 2 ] }
    variables { domain: [ -1, -1, 1, 1 ] }
    constraints {
      int_div {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 vars: 3 coeffs: 1 coeffs: 1 }
      }
    }
  )pb");
  EXPECT_TRUE(ValidateCpModel(model).empty());
}

TEST(ValidateCpModelTest, TwoTermDivisorCanBeZero) {
  // 2 * a + b - 1 is 0 for a = 0 and b = 1.
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 2 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      int_div {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 vars: 3 coeffs: 2 coeffs: 1 offset: -1 }
      }
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(model),
              HasSubstr("The domain of the divisor cannot contain 0"));
}

TEST(ValidateCpModelTest, DivisorIsZero) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      int_div {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs {}
      }
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(model), HasSubstr("Division by 0"));
}

TEST(ValidateCpModelTest, WrongArityInIntMod) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      int_mod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
      }
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(model), HasSubstr("have exactly 2 terms"));
}

TEST(ValidateCpModelTest, NegativeModulo) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ -3, 3 ] }
    constraints {
      int_mod {
        target { vars: 0 coeffs: 1 }
        exprs { vars: 1 coeffs: 1 }
        exprs { vars: 2 coeffs: 1 }
      }
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(model),
              HasSubstr("strictly positive modulo argument"));
}

TEST(ValidateCpModelTest, InvalidLegacyInverseConstraint) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 1 }
    constraints { inverse { f_direct: [ 0 ] } }
  )pb");
  EXPECT_THAT(ValidateCpModel(model),
              HasSubstr("Non-matching fields size in inverse"));
}

TEST(ValidateCpModelTest, InvalidInverseConstraint) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 1 }
    constraints {
      inverse {
        f_expr_direct: {
          vars: [ 0 ]
          coeffs: [ 1 ]
        }
      }
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(model),
              HasSubstr("Non-matching fields size in inverse"));
}

TEST(ValidateCpModelTest, InconsistentInverseConstraint) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 1 }
    constraints {
      inverse {
        f_direct: [ 0 ]
        f_inverse: [ 0 ]
        f_expr_direct: {
          vars: [ 0 ]
          coeffs: [ 1 ]
        }
        f_expr_inverse: {
          vars: [ 0 ]
          coeffs: [ 1 ]
        }
      }
    }
  )pb");
  EXPECT_THAT(
      ValidateCpModel(model),
      HasSubstr(
          "Inconsistent inverse with both legacy and new format defined"));
}

TEST(ValidateCpModelTest, IncompatibleAutomatonTransitions) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 1 }
    constraints {
      automaton {
        final_states: 0
        transition_tail: 0
        transition_tail: 0
        transition_head: 0
        transition_head: 1
        transition_label: 0
        transition_label: 0
        vars: 0
      }
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(model),
              HasSubstr("automaton: incompatible transitions"));
}

TEST(ValidateCpModelTest, DuplicateAutomatonTransitions) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: 0 domain: 1 }
    constraints {
      automaton {
        final_states: 0
        transition_tail: 0
        transition_tail: 0
        transition_head: 0
        transition_head: 0
        transition_label: 0
        transition_label: 0
        vars: 0
      }
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(model),
              HasSubstr("automaton: duplicate transition"));
}

TEST(ValidateCpModelTest, IntervalMustAppearBeforeTheyAreUsed) {
  const CpModelProto model = ParseTestProto(R"pb(
    constraints { no_overlap { intervals: [ 1, 2 ] } }
    constraints {
      interval {
        start { offset: 0 }
        end { offset: 4 }
        size { offset: 4 }
      }
    }
    constraints {
      interval {
        start { offset: 4 }
        end { offset: 5 }
        size { offset: 1 }
      }
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(model, /*after_presolve=*/true),
              HasSubstr("must appear before"));
}

TEST(ValidateCpModelTest, ValidNodeExpressions) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      routes {
        tails: [ 0, 1 ]
        heads: [ 1, 0 ]
        literals: [ 0, 1 ]
        dimensions {
          exprs {
            vars: [ 2 ]
            coeffs: [ 1 ]
          }
          exprs {
            vars: [ 3 ]
            coeffs: [ 2 ]
          }
        }
        dimensions {
          exprs {}
          exprs {}
        }
      }
    }
  )pb");
  EXPECT_TRUE(ValidateCpModel(model).empty());
}

TEST(ValidateCpModelTest, InvalidNodeExpressionsCount) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      routes {
        tails: [ 0, 1 ]
        heads: [ 1, 0 ]
        literals: [ 0, 1 ]
        dimensions {
          exprs {
            vars: [ 2 ]
            coeffs: [ 1 ]
          }
          exprs {
            vars: [ 3 ]
            coeffs: [ 1 ]
          }
          exprs {
            vars: [ 2 ]
            coeffs: [ 1 ]
          }
        }
      }
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(model), HasSubstr("must be of size num_nodes:2"));
}

TEST(ValidateCpModelTest, InvalidNodeExpressionInRoutesConstraint) {
  const CpModelProto model = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 10 ] }
    constraints {
      routes {
        tails: [ 0, 1 ]
        heads: [ 1, 0 ]
        literals: [ 0, 1 ]
        dimensions {
          exprs {
            vars: [ 2 ]
            coeffs: [ 1 ]
          }
          exprs {
            vars: [ 3 ]
            coeffs: [ 1 ]
          }
        }
      }
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(model),
              HasSubstr("Out of bound integer variable 3 in route constraint"));
}

TEST(ValidateCpModelTest, ReservoirLevelChangesOverflow) {
  CHECK_EQ(kint64max / 2, int64_t{4611686018427387903});
  const CpModelProto model_ok = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      reservoir {
        min_level: -1
        max_level: 5
        time_exprs { offset: 0 }
        time_exprs { offset: 1 }
        active_literals: [ 0, -2 ]
        level_changes { offset: 1 }
        level_changes { offset: -4611686018427387902 }
      }
    }
  )pb");
  EXPECT_EQ(ValidateCpModel(model_ok), "");

  const CpModelProto model_overflow = ParseTestProto(R"pb(
    variables { domain: [ 0, 1 ] }
    variables { domain: [ 0, 1 ] }
    constraints {
      reservoir {
        min_level: -1
        max_level: 5
        time_exprs { offset: 0 }
        time_exprs { offset: 1 }
        active_literals: [ 0, -2 ]
        level_changes { offset: 1 }
        level_changes { offset: -4611686018427387903 }
      }
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(model_overflow),
              HasSubstr("Possible integer overflow in constraint"));
}

TEST(ValidateCpModelTest, ReservoirTimeExprsOverflow) {
  CHECK_EQ(kint64max / 4, int64_t{2305843009213693951});
  // The range of each time expression is 2e18 < kint64max / 4, so the range
  // of time_exprs[i] - time_exprs[j] is less than kint64max / 2.
  const CpModelProto model_ok = ParseTestProto(R"pb(
    variables { domain: [ -1000000000000000000, 1000000000000000000 ] }
    variables { domain: [ -1000000000000000000, 1000000000000000000 ] }
    constraints {
      reservoir {
        min_level: 0
        max_level: 1
        time_exprs { vars: 0 coeffs: 1 }
        time_exprs { vars: 1 coeffs: 1 }
        level_changes { offset: 1 }
        level_changes { offset: -1 }
      }
    }
  )pb");
  EXPECT_EQ(ValidateCpModel(model_ok), "");

  // The values of each time expression are in (-kint64max / 4,
  // kint64max / 4), but its range is 4e18 >= kint64max / 4.
  const CpModelProto model_overflow = ParseTestProto(R"pb(
    variables { domain: [ -2000000000000000000, 2000000000000000000 ] }
    variables { domain: [ -2000000000000000000, 2000000000000000000 ] }
    constraints {
      reservoir {
        min_level: 0
        max_level: 1
        time_exprs { vars: 0 coeffs: 1 }
        time_exprs { vars: 1 coeffs: 1 }
        level_changes { offset: 1 }
        level_changes { offset: -1 }
      }
    }
  )pb");
  EXPECT_THAT(ValidateCpModel(model_overflow),
              HasSubstr("Potential integer overflow on time_expr"));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
