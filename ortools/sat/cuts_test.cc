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

#include "ortools/sat/cuts.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/numeric/int128.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_literal.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/fp_utils_testing.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {
namespace {

using ::testing::DoubleNear;
using ::testing::EndsWith;
using ::testing::StartsWith;

std::vector<IntegerValue> IntegerValueVector(absl::Span<const int> values) {
  std::vector<IntegerValue> result;
  for (const int v : values) result.push_back(IntegerValue(v));
  return result;
}

TEST(GetSuperAdditiveRoundingFunctionTest, AllSmallValues) {
  const int max_divisor = 25;
  for (IntegerValue max_t(1); max_t <= 9; ++max_t) {
    for (IntegerValue max_scaling(1); max_scaling <= 9; max_scaling++) {
      for (IntegerValue divisor(1); divisor <= max_divisor; ++divisor) {
        for (IntegerValue rhs_remainder(1); rhs_remainder < divisor;
             ++rhs_remainder) {
          const std::string info = absl::StrCat(
              " rhs_remainder = ", rhs_remainder.value(),
              " divisor = ", divisor.value(), " max_t = ", max_t.value(),
              " max_scaling = ", max_scaling.value());
          const auto f = GetSuperAdditiveRoundingFunction(
              rhs_remainder, divisor,
              std::min(max_t, GetFactorT(rhs_remainder, divisor)), max_scaling);
          ASSERT_EQ(f(IntegerValue(0)), 0) << info;
          ASSERT_GE(f(divisor), 1) << info;
          ASSERT_LE(f(divisor), max_scaling * max_t) << info;
          for (IntegerValue a(0); a < divisor; ++a) {
            IntegerValue min_diff = kMaxIntegerValue;
            for (IntegerValue b(1); b < divisor; ++b) {
              min_diff = std::min(min_diff, f(a + b) - f(a) - f(b));
              ASSERT_GE(min_diff, 0)
                  << info << ", f(" << a << ")=" << f(a) << " + f(" << b
                  << ")=" << f(b) << " <= f(" << a + b << ")=" << f(a + b);
            }

            if (a <= rhs_remainder) continue;
            if (max_scaling == 2 && rhs_remainder * max_t >= divisor / 2) {
              // min_diff > 0 shows that our function is dominated (i.e. not
              // maximal) since f(a) could be increased by 1/2.
              ASSERT_EQ(min_diff, 0)
                  << "Not maximal at " << info << " f(" << a << ") = " << f(a)
                  << " min_diff:" << min_diff;
            }
          }
        }
      }
    }
  }
}

TEST(GetSuperAdditiveRoundingFunctionTest,
     MaximalOnScaledBucketsAndSignedInputs) {
  // With divisor = 14, rhs_remainder = 8 (size = 6) and max_scaling = 4,
  // (max_scaling - 1) = 3 divides size = 6. At remainder = 10 (diff = 2,
  // 2 * diff < size), the maximal bucket is 2 rather than CeilRatio(6, 6) = 1.
  const auto f_concrete = GetSuperAdditiveRoundingFunction(
      IntegerValue(8), IntegerValue(14), IntegerValue(1), IntegerValue(4));
  const std::vector<int64_t> expected = {0, 1, 2, 2, 2, 3, 4};
  for (int i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(f_concrete(IntegerValue(8 + i)), IntegerValue(expected[i])) << i;
  }

  for (IntegerValue divisor(3); divisor <= 20; ++divisor) {
    for (IntegerValue rhs_remainder(1); rhs_remainder < divisor;
         ++rhs_remainder) {
      for (IntegerValue max_scaling(2); max_scaling <= 9; ++max_scaling) {
        const auto f = GetSuperAdditiveRoundingFunction(
            rhs_remainder, divisor, IntegerValue(1), max_scaling);
        ASSERT_EQ(f(IntegerValue(0)), 0);
        ASSERT_EQ(f(rhs_remainder), 0);
        for (IntegerValue a = -2 * divisor; a <= 2 * divisor; ++a) {
          for (IntegerValue b = a; b <= 2 * divisor; ++b) {
            ASSERT_LE(f(a) + f(b), f(a + b))
                << "divisor=" << divisor << " rhs_rem=" << rhs_remainder
                << " max_scaling=" << max_scaling << " a=" << a << " b=" << b;
          }
        }
        if (max_scaling * rhs_remainder > divisor &&
            divisor - rhs_remainder > 1) {
          for (IntegerValue a = rhs_remainder + 1; a < divisor; ++a) {
            IntegerValue max_inc = kMaxIntegerValue;
            for (IntegerValue b(1); b < divisor; ++b) {
              const IntegerValue slack = f(a + b) - f(a) - f(b);
              max_inc = std::min(max_inc, b == a ? slack / 2 : slack);
            }
            ASSERT_EQ(max_inc, 0)
                << "Not maximal: divisor=" << divisor
                << " rhs_rem=" << rhs_remainder
                << " max_scaling=" << max_scaling << " a=" << a;
          }
        }
      }
    }
  }
}

TEST(GetSuperAdditiveStrengtheningFunction, AllSmallValues) {
  for (const int64_t rhs : {13, 14}) {  // test odd/even
    for (int64_t min_magnitude = 1; min_magnitude <= rhs; ++min_magnitude) {
      const IntegerValue min_period =
          3 * min_magnitude <= rhs ? IntegerValue(rhs + 2 * min_magnitude - 2)
                                   : IntegerValue(2 * rhs - 2);
      const auto f = ExtendNegativeFunction(
          GetSuperAdditiveStrengtheningFunction(rhs, min_magnitude),
          std::max(IntegerValue(rhs), min_period));

      // Check super additivity in -[50, 50]
      for (int a = -50; a <= 50; ++a) {
        for (int b = -50; b <= 50; ++b) {
          ASSERT_LE(f(a) + f(b), f(a + b))
              << " a=" << a << " b=" << b << " min=" << min_magnitude
              << " rhs=" << rhs;
        }
      }
    }
  }
}

TEST(GetSuperAdditiveStrengtheningMirFunction, AllSmallValues) {
  for (const int64_t rhs : {13, 14}) {  // test odd/even
    for (int64_t scaling = 1; scaling <= rhs; ++scaling) {
      const IntegerValue min_period =
          (scaling > 1 && scaling < rhs)
              ? IntegerValue(rhs - 1) +
                    CeilRatio(IntegerValue(rhs - 1), IntegerValue(scaling - 1))
              : IntegerValue(rhs);
      const auto f = ExtendNegativeFunction(
          GetSuperAdditiveStrengtheningMirFunction(rhs, scaling), min_period);

      // Check super additivity in -[50, 50]
      for (int a = -50; a <= 50; ++a) {
        for (int b = -50; b <= 50; ++b) {
          ASSERT_LE(f(a) + f(b), f(a + b))
              << " a=" << a << " b=" << b << " scaling=" << scaling
              << " rhs=" << rhs;
        }
      }
    }
  }
}

TEST(CutDataTest, ComputeViolation) {
  CutData cut;
  cut.rhs = 2;
  cut.terms.push_back({.lp_value = 1.2, .coeff = 1});
  cut.terms.push_back({.lp_value = 0.5, .coeff = 2});
  EXPECT_THAT(0.2, WithinSameAbsoluteOrRelativeTolerance(cut.ComputeViolation(),
                                                         1e-10));
}

template <class Helper>
std::string GetCutString(const Helper& helper) {
  LinearConstraint ct;
  CutDataBuilder builder;
  EXPECT_TRUE(builder.ConvertToLinearConstraint(helper.cut(), &ct));
  return ct.DebugString();
}

TEST(CoverCutHelperTest, SimpleExample) {
  // 6x0 + 4x1 + 10x2 <= 9.
  std::vector<IntegerVariable> vars = {IntegerVariable(0), IntegerVariable(2),
                                       IntegerVariable(4)};
  std::vector<IntegerValue> coeffs = IntegerValueVector({6, 4, 10});
  std::vector<IntegerValue> lbs = IntegerValueVector({0, 0, 0});
  std::vector<double> lp_values{1.0, 0.5, 0.1};  // Tight.

  // NOTE(user): the ub of the last variable is not used. But the first two
  // are, even though only the second one is required for the validity of the
  // cut.
  std::vector<IntegerValue> ubs = IntegerValueVector({1, 1, 10});

  CutData data;
  data.FillFromParallelVectors(IntegerValue(9), vars, coeffs, lp_values, lbs,
                               ubs);
  data.ComplementForPositiveCoefficients();
  Model model;
  CoverCutHelper helper(&model);
  EXPECT_TRUE(helper.TrySimpleKnapsack(data));
  EXPECT_EQ(GetCutString(helper), "1*I0 1*I1 1*I2 <= 1");
  EXPECT_EQ(helper.Info(), "lift=1");
}

// I tried to reproduce bug 169094958, but if the base constraint is tight,
// the bug was triggered only due to numerical imprecision. A simple way to
// trigger it is like with this test if the given LP value just violates the
// initial constraint.
TEST(CoverCutHelperTest, WeirdExampleWithViolatedConstraint) {
  // x0 + x1 <= 9.
  std::vector<IntegerVariable> vars = {IntegerVariable(0), IntegerVariable(2)};
  std::vector<IntegerValue> coeffs = IntegerValueVector({1, 1});
  std::vector<IntegerValue> lbs = IntegerValueVector({
      0,
      0,
  });
  std::vector<IntegerValue> ubs = IntegerValueVector({10, 13});
  std::vector<double> lp_values{0.0, 12.6};  // violated.

  CutData data;
  data.FillFromParallelVectors(IntegerValue(9), vars, coeffs, lp_values, lbs,
                               ubs);
  data.ComplementForPositiveCoefficients();
  Model model;
  CoverCutHelper helper(&model);
  EXPECT_TRUE(helper.TrySimpleKnapsack(data));
  EXPECT_EQ(GetCutString(helper), "1*I0 1*I1 <= 9");
  EXPECT_EQ(helper.Info(), "lift=1");
}

TEST(CoverCutHelperTest, LetchfordSouliLifting) {
  const int n = 10;
  const IntegerValue rhs = IntegerValue(16);
  std::vector<IntegerVariable> vars;
  std::vector<IntegerValue> coeffs =
      IntegerValueVector({5, 5, 5, 5, 15, 13, 9, 8, 8, 8});
  for (int i = 0; i < n; ++i) {
    vars.push_back(IntegerVariable(2 * i));
  }
  std::vector<IntegerValue> lbs(n, IntegerValue(0));
  std::vector<IntegerValue> ubs(n, IntegerValue(1));
  std::vector<double> lps(n, 0.0);
  for (int i = 0; i < 4; ++i) {
    lps[i] = 0.9;
  }

  CutData data;
  data.FillFromParallelVectors(rhs, vars, coeffs, lps, lbs, ubs);
  data.ComplementForPositiveCoefficients();

  Model model;
  CoverCutHelper helper(&model);
  EXPECT_TRUE(helper.TryWithLetchfordSouliLifting(data));
  EXPECT_EQ(GetCutString(helper),
            "1*I0 1*I1 1*I2 1*I3 3*I4 3*I5 2*I6 1*I7 1*I8 1*I9 <= 3");

  // For now, we only support Booleans in the cover.
  // Note that we don't care for variables not in the cover though.
  data.terms[3].bound_diff = IntegerValue(2);
  EXPECT_FALSE(helper.TryWithLetchfordSouliLifting(data));
}

LinearConstraint IntegerRoundingCutWithBoundsFromTrail(
    const RoundingOptions& options, IntegerValue rhs,
    absl::Span<const IntegerVariable> vars,
    absl::Span<const IntegerValue> coeffs, absl::Span<const double> lp_values,
    Model* model) {
  std::vector<IntegerValue> lbs;
  std::vector<IntegerValue> ubs;
  auto* integer_trail = model->Get<IntegerTrail>();
  for (int i = 0; i < vars.size(); ++i) {
    lbs.push_back(integer_trail->LowerBound(vars[i]));
    ubs.push_back(integer_trail->UpperBound(vars[i]));
  }

  CutData data;
  data.FillFromParallelVectors(rhs, vars, coeffs, lp_values, lbs, ubs);
  data.ComplementForSmallerLpValues();

  IntegerRoundingCutHelper helper(model);
  EXPECT_TRUE(helper.ComputeCut(options, data, nullptr));

  CutDataBuilder builder;
  LinearConstraint constraint;
  EXPECT_TRUE(builder.ConvertToLinearConstraint(helper.cut(), &constraint));
  return constraint;
}

TEST(IntegerRoundingCutTest, LetchfordLodiExample1) {
  Model model;
  const IntegerVariable x0 = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable x1 = model.Add(NewIntegerVariable(0, 10));

  // 6x0 + 4x1 <= 9.
  const IntegerValue rhs = IntegerValue(9);
  std::vector<IntegerVariable> vars = {x0, x1};
  std::vector<IntegerValue> coeffs = {IntegerValue(6), IntegerValue(4)};

  std::vector<double> lp_values{1.5, 0.0};
  RoundingOptions options;
  options.max_scaling = 2;
  LinearConstraint constraint = IntegerRoundingCutWithBoundsFromTrail(
      options, rhs, vars, coeffs, lp_values, &model);
  EXPECT_EQ(constraint.DebugString(), "2*I0 1*I1 <= 2");
}

TEST(IntegerRoundingCutTest, LetchfordLodiExample1Modified) {
  Model model;
  const IntegerVariable x0 = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable x1 = model.Add(NewIntegerVariable(0, 1));

  // 6x0 + 4x1 <= 9.
  const IntegerValue rhs = IntegerValue(9);

  std::vector<IntegerVariable> vars = {x0, x1};
  std::vector<IntegerValue> coeffs = {IntegerValue(6), IntegerValue(4)};

  // x1 is at its upper bound here.
  std::vector<double> lp_values{5.0 / 6.0, 1.0};

  // Note that the cut is only valid because the bound of x1 is one here.
  LinearConstraint constraint = IntegerRoundingCutWithBoundsFromTrail(
      RoundingOptions(), rhs, vars, coeffs, lp_values, &model);
  EXPECT_EQ(constraint.DebugString(), "1*I0 1*I1 <= 1");
}

TEST(IntegerRoundingCutTest, LetchfordLodiExample2) {
  Model model;
  const IntegerVariable x0 = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable x1 = model.Add(NewIntegerVariable(0, 10));

  // 6x0 + 4x1 <= 9.
  const IntegerValue rhs = IntegerValue(9);
  std::vector<IntegerVariable> vars = {x0, x1};
  std::vector<IntegerValue> coeffs = {IntegerValue(6), IntegerValue(4)};

  std::vector<double> lp_values{0.0, 2.25};
  LinearConstraint constraint = IntegerRoundingCutWithBoundsFromTrail(
      RoundingOptions(), rhs, vars, coeffs, lp_values, &model);
  EXPECT_EQ(constraint.DebugString(), "3*I0 2*I1 <= 4");
}

TEST(IntegerRoundingCutTest, LetchfordLodiExample2WithNegatedCoeff) {
  Model model;
  const IntegerVariable x0 = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable x1 = model.Add(NewIntegerVariable(-3, 0));

  // 6x0 - 4x1 <= 9.
  const IntegerValue rhs = IntegerValue(9);
  std::vector<IntegerVariable> vars = {x0, x1};
  std::vector<IntegerValue> coeffs = {IntegerValue(6), IntegerValue(-4)};

  std::vector<double> lp_values{0.0, -2.25};
  LinearConstraint constraint = IntegerRoundingCutWithBoundsFromTrail(
      RoundingOptions(), rhs, vars, coeffs, lp_values, &model);

  // We actually do not return like in the example "3*I0 -2*I1 <= 4"
  // But the simpler I0 - I1 <= 2 which has the same violation (0.25) but a
  // better norm.
  EXPECT_EQ(constraint.DebugString(), "1*I0 -1*I1 <= 2");
}

// This used to trigger a failure with a wrong implied bound code path.
TEST(IntegerRoundingCutTest, TestCaseUsedForDebugging) {
  Model model;
  // Variable values are in comment.
  const IntegerVariable x0 = model.Add(NewIntegerVariable(0, 3));  // 1
  const IntegerVariable x1 = model.Add(NewIntegerVariable(0, 4));  // 0
  const IntegerVariable x2 = model.Add(NewIntegerVariable(0, 2));  // 1
  const IntegerVariable x3 = model.Add(NewIntegerVariable(0, 1));  // 0
  const IntegerVariable x4 = model.Add(NewIntegerVariable(0, 3));  // 1

  // The constraint is tight with value above (-5 - 4 + 7 == -2).
  const IntegerValue rhs = IntegerValue(-2);
  std::vector<IntegerVariable> vars = {x0, x1, x2, x3, x4};
  std::vector<IntegerValue> coeffs = IntegerValueVector({-5, -1, -4, -7, 7});

  // The constraint is tight under LP (-5 * 0.4 == -2).
  std::vector<double> lp_values{0.4, 0.0, -1e-16, 0.0, 0.0};
  LinearConstraint constraint = IntegerRoundingCutWithBoundsFromTrail(
      RoundingOptions(), rhs, vars, coeffs, lp_values, &model);

  EXPECT_EQ(constraint.DebugString(), "-2*I0 -1*I1 -2*I2 -2*I3 2*I4 <= -2");
}

TEST(IntegerRoundingCutTest, NonRelevantNegativeAdjustPreservesBuckets) {
  Model model;
  const IntegerVariable x0 = model.Add(NewIntegerVariable(0, 2));
  const IntegerVariable x1 = model.Add(NewIntegerVariable(0, 1));
  const IntegerVariable x2 = model.Add(NewIntegerVariable(0, 1));
  const IntegerVariable x3 = model.Add(NewIntegerVariable(0, 1));

  // Divisor 1000, rhs = 500. Dominating f chooses max_scaling = 50, placing
  // x0 (coeff 990) in bucket 49, which drops to 48 as soon as rhs_remainder
  // exceeds 509 (exact slack budget = 9). Thus x1 (-9) is adjusted to 0 while
  // x2 (-10) is kept so x0 loses zero LP violation.
  const IntegerValue rhs(500);
  const std::vector<IntegerVariable> vars = {x0, x1, x2, x3};
  const std::vector<IntegerValue> coeffs =
      IntegerValueVector({990, -9, -10, 1000});
  const std::vector<double> lp_values = {0.8, 0.0, 0.0, 0.0};

  RoundingOptions options;
  options.max_scaling = 60;
  const LinearConstraint constraint = IntegerRoundingCutWithBoundsFromTrail(
      options, rhs, vars, coeffs, lp_values, &model);
  EXPECT_EQ(constraint.DebugString(), "49*I0 -1*I2 50*I3 <= 0");
}

TEST(IntegerRoundingCutTest, NonRelevantNegativeAdjustPreservesFactorT) {
  Model model;
  const IntegerVariable x0 = model.Add(NewIntegerVariable(0, 2));
  const IntegerVariable x1 = model.Add(NewIntegerVariable(0, 1));
  const IntegerVariable x2 = model.Add(NewIntegerVariable(0, 1));
  const IntegerVariable x3 = model.Add(NewIntegerVariable(0, 1));

  // Divisor 1000, rhs = 100 -> factor_t = 5 (r0 = 500) and 5 * 198 = 990.
  // Exact scaled cap is 509, so slack_budget = (509 - 500) / 5 = 1: x1 (-1) is
  // adjusted to 0 while x2 (-40) is not adjusted (preserving factor_t = 5).
  const IntegerValue rhs(100);
  const std::vector<IntegerVariable> vars = {x0, x1, x2, x3};
  const std::vector<IntegerValue> coeffs =
      IntegerValueVector({198, -1, -40, 1000});
  const std::vector<double> lp_values = {0.8, 0.0, 0.0, 0.0};

  RoundingOptions options;
  options.max_scaling = 60;
  const LinearConstraint constraint = IntegerRoundingCutWithBoundsFromTrail(
      options, rhs, vars, coeffs, lp_values, &model);
  EXPECT_EQ(constraint.DebugString(), "49*I0 -20*I2 250*I3 <= 0");
}

TEST(GUBHelperTest, ZeroCoeffComplementedBooleanDoesNotBlockGubLifting) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_at_most_one_max_expansion_size(2);
  auto* integer_encoder = model.GetOrCreate<IntegerEncoder>();
  auto* implications = model.GetOrCreate<BinaryImplicationGraph>();

  const BooleanVariable b0 = model.Add(NewBooleanVariable());
  const BooleanVariable b1 = model.Add(NewBooleanVariable());
  const BooleanVariable b2 = model.Add(NewBooleanVariable());
  const IntegerVariable x0 = model.Add(NewIntegerVariable(0, 1));
  const IntegerVariable x1 = model.Add(NewIntegerVariable(0, 1));
  const IntegerVariable x2 = model.Add(NewIntegerVariable(0, 1));
  const IntegerVariable x3 = model.Add(NewIntegerVariable(0, 2));

  integer_encoder->AssociateToIntegerEqualValue(Literal(b0, true), x0,
                                                IntegerValue(1));
  integer_encoder->AssociateToIntegerEqualValue(Literal(b1, true), x1,
                                                IntegerValue(1));
  integer_encoder->AssociateToIntegerEqualValue(Literal(b2, true), x2,
                                                IntegerValue(1));
  ASSERT_TRUE(implications->AddAtMostOne(
      {Literal(b0, true), Literal(b1, true), Literal(b2, true)}));

  CutData cut;
  cut.rhs = 8;

  // Complemented (1 - x0) with coeff == 0 (e.g. after coefficient adjustment).
  CutTerm t0;
  t0.coeff = 0;
  t0.lp_value = 0.8;
  t0.bound_diff = 1;
  t0.expr_vars[0] = x0;
  t0.expr_coeffs[0] = -1;
  t0.expr_coeffs[1] = 0;
  t0.expr_offset = 1;
  cut.terms.push_back(t0);

  // Complemented (1 - x1) with negative coeff -4 (f(-4) = -2).
  CutTerm t1;
  t1.coeff = -4;
  t1.lp_value = 0.1;
  t1.bound_diff = 1;
  t1.expr_vars[0] = x1;
  t1.expr_coeffs[0] = -1;
  t1.expr_coeffs[1] = 0;
  t1.expr_offset = 1;
  cut.terms.push_back(t1);

  // Uncomplemented x2 in the same AMO with coeff 5: f(5) = 0, but GUB lifting
  // with t1 gives -f(-4) + f(5 - 4) = 2.
  CutTerm t2;
  t2.coeff = 5;
  t2.lp_value = 0.0;
  t2.bound_diff = 1;
  t2.expr_vars[0] = x2;
  t2.expr_coeffs[0] = 1;
  t2.expr_coeffs[1] = 0;
  t2.expr_offset = 0;
  cut.terms.push_back(t2);

  // Violated integer term so ApplyWithPostprocessing runs GUB lifting.
  CutTerm t3;
  t3.coeff = 10;
  t3.lp_value = 0.9;
  t3.bound_diff = 2;
  t3.expr_vars[0] = x3;
  t3.expr_coeffs[0] = 1;
  t3.expr_coeffs[1] = 0;
  t3.expr_offset = 0;
  cut.terms.push_back(t3);

  const auto f = GetSuperAdditiveRoundingFunction(
      /*rhs_remainder=*/IntegerValue(8), /*divisor=*/IntegerValue(10),
      /*t=*/IntegerValue(1), /*max_scaling=*/IntegerValue(2));
  GUBHelper gub_helper(&model);
  EXPECT_TRUE(gub_helper.ApplyWithPostprocessing(
      f, IntegerValue(10), /*ib_processor=*/nullptr, &cut));

  EXPECT_EQ(gub_helper.last_stats().num_gubs, 1);
  ASSERT_EQ(cut.terms.size(), 3);
  EXPECT_EQ(cut.terms[1].coeff, IntegerValue(2));
}

TEST(IntegerRoundingCutTest, ZeroLossAdjustProtectsSmallNonZeroLpTerms) {
  Model model;
  const IntegerVariable x0 = model.Add(NewIntegerVariable(0, 2));
  const IntegerVariable x1 = model.Add(NewIntegerVariable(0, 1));
  const IntegerVariable x2 = model.Add(NewIntegerVariable(0, 1));

  // Adjusting -10*x2 to 0 would increase rhs from 500 to 510 and drop x1's MIR
  // coefficient (for coeff 515) from 2 to 1. Even though x1 has lp_value =
  // 0.005 < 1e-2 (below HasRelevantLpValue()), zero-loss adjustment protects
  // all positive-LP terms (lp_value > 1e-6), so x2 is not adjusted.
  const IntegerValue rhs(500);
  const std::vector<IntegerVariable> vars = {x0, x1, x2};
  const std::vector<IntegerValue> coeffs = {
      IntegerValue(1000), IntegerValue(515), IntegerValue(-10)};
  const std::vector<double> lp_values = {0.495, 0.005, 0.0};

  const LinearConstraint constraint = IntegerRoundingCutWithBoundsFromTrail(
      RoundingOptions(), rhs, vars, coeffs, lp_values, &model);
  EXPECT_EQ(constraint.DebugString(), "30*I0 1*I1 -1*I2 <= 0");
}

TEST(IntegerRoundingCutTest,
     ZeroLossAdjustStrengthensLargeNegativeTermToMultipleOfDivisor) {
  Model model;
  const IntegerVariable x0 = model.Add(NewIntegerVariable(0, 4));
  const IntegerVariable x1 = model.Add(NewIntegerVariable(0, 1));

  // With best_divisor = 1000 and rhs_remainder = 900, x1 has coeff -2005 <
  // -best_divisor. Zero-loss adjustment increases -2005 by +5 to -2000 (an
  // exact multiple of 1000, shifting rhs to 1905), strengthening the cut from
  // 20*I0 - 41*I1 <= 20 to 1*I0 - 2*I1 <= 1.
  const IntegerValue rhs(1900);
  const std::vector<IntegerVariable> vars = {x0, x1};
  const std::vector<IntegerValue> coeffs = {IntegerValue(1000),
                                            IntegerValue(-2005)};
  const std::vector<double> lp_values = {1.9, 0.0};

  const LinearConstraint constraint = IntegerRoundingCutWithBoundsFromTrail(
      RoundingOptions(), rhs, vars, coeffs, lp_values, &model);
  EXPECT_EQ(constraint.DebugString(), "1*I0 -2*I1 <= 1");
}

TEST(IntegerRoundingCutTest, ZeroLossAdjustEliminatesRelevantNegativeTerm) {
  Model model;
  const IntegerVariable x0 = model.Add(NewIntegerVariable(0, 4));
  const IntegerVariable x1 = model.Add(NewIntegerVariable(0, 4));
  const IntegerVariable x2 = model.Add(NewIntegerVariable(0, 1));

  // x2 has lp_value = 0.02 (a relevant entry), is not adjusted inside
  // GetScaledViolation (adjust_threshold = 99 / 1000 = 0), and is not
  // complemented during post-complementation. Zero-loss adjustment after
  // selecting best_scaling = 8 adjusts -12*x2 to 0 (rhs: 700 -> 712) while
  // preserving f(175) = 6, strengthening 8*I0 + 6*I1 - 1*I2 <= 24 to
  // 4*I0 + 3*I1 <= 12.
  const IntegerValue rhs(700);
  const std::vector<IntegerVariable> vars = {x0, x1, x2};
  const std::vector<IntegerValue> coeffs = {
      IntegerValue(200), IntegerValue(175), IntegerValue(-12)};
  const std::vector<double> lp_values = {1.9, 1.8, 0.02};

  const LinearConstraint constraint = IntegerRoundingCutWithBoundsFromTrail(
      RoundingOptions(), rhs, vars, coeffs, lp_values, &model);
  EXPECT_EQ(constraint.DebugString(), "4*I0 3*I1 <= 12");
}

TEST(ZeroLossCoefficientAdjusterTest, ZeroesNegativeTermsWithinBudget) {
  Model model;
  const IntegerVariable x0 = model.Add(NewIntegerVariable(0, 2));
  const IntegerVariable x1 = model.Add(NewIntegerVariable(0, 2));
  const IntegerVariable x2 = model.Add(NewIntegerVariable(0, 1));

  // Pure Chvatal-Gomory f(v) = FloorRatio(v, 100) with rhs_remainder = 50. The
  // rebuilt function stays identical on x0 (remainder 0) for any rhs remainder
  // up to max_rem_cap = 50 + (100 - 1 - 50) / 2 = 74, so slack_budget = 24.
  // Only t2 (prod = 15 * 1 = 15) fits, t1 (prod = 20 * 2 = 40) does not: t2 is
  // zeroed (rhs: 50 -> 65) while t1 keeps f(-20) = -1.
  CutData cut;
  cut.rhs = 50;

  CutTerm t0;
  t0.coeff = 100;
  t0.lp_value = 0.6;
  t0.bound_diff = 2;
  t0.expr_vars[0] = x0;
  t0.expr_coeffs[0] = 1;
  t0.expr_coeffs[1] = 0;
  t0.expr_offset = 0;
  cut.terms.push_back(t0);

  CutTerm t1;
  t1.coeff = -20;
  t1.lp_value = 0.0;
  t1.bound_diff = 2;
  t1.expr_vars[0] = x1;
  t1.expr_coeffs[0] = 1;
  t1.expr_coeffs[1] = 0;
  t1.expr_offset = 0;
  cut.terms.push_back(t1);

  CutTerm t2;
  t2.coeff = -15;
  t2.lp_value = 0.0;
  t2.bound_diff = 1;
  t2.expr_vars[0] = x2;
  t2.expr_coeffs[0] = 1;
  t2.expr_coeffs[1] = 0;
  t2.expr_offset = 0;
  cut.terms.push_back(t2);

  std::function<IntegerValue(IntegerValue)> f =
      GetSuperAdditiveRoundingFunction(
          /*rhs_remainder=*/IntegerValue(50), /*divisor=*/IntegerValue(100),
          /*t=*/IntegerValue(1), /*max_scaling=*/IntegerValue(1));
  ZeroLossCoefficientAdjuster adjuster;
  EXPECT_EQ(adjuster.Adjust(IntegerValue(100), IntegerValue(1), IntegerValue(1),
                            &cut, &f),
            1);
  EXPECT_EQ(cut.rhs, absl::int128(65));
  ASSERT_EQ(cut.terms.size(), 3);
  EXPECT_EQ(cut.terms[1].coeff, IntegerValue(-20));
  EXPECT_EQ(cut.terms[2].coeff, IntegerValue(0));

  GUBHelper gub_helper(&model);
  EXPECT_TRUE(gub_helper.ApplyWithPostprocessing(
      f, IntegerValue(100), /*ib_processor=*/nullptr, &cut));
  EXPECT_EQ(cut.rhs, absl::int128(0));
  ASSERT_EQ(cut.terms.size(), 2);
  EXPECT_EQ(cut.terms[0].expr_vars[0], x0);
  EXPECT_EQ(cut.terms[0].coeff, IntegerValue(1));
  EXPECT_EQ(cut.terms[1].expr_vars[0], x1);
  EXPECT_EQ(cut.terms[1].coeff, IntegerValue(-1));
}

// The algo should find a "divisor" 2 when it leads to a good cut.
//
// TODO(user): Double check that such a divisor will always be found? Of course,
// if the initial constraint coefficients are too high, then it will not, but
// that is okay since such cut efficacy will be bad anyway.
TEST(IntegerRoundingCutTest, ZeroHalfCut) {
  Model model;
  const IntegerVariable x0 = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable x1 = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable x2 = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable x3 = model.Add(NewIntegerVariable(0, 10));

  // 6x0 + 4x1 + 8x2 + 7x3 <= 9.
  const IntegerValue rhs = IntegerValue(9);
  std::vector<IntegerVariable> vars = {x0, x1, x2, x3};
  std::vector<IntegerValue> coeffs = {IntegerValue(6), IntegerValue(4),
                                      IntegerValue(8), IntegerValue(7)};

  std::vector<double> lp_values{0.25, 1.25, 0.3125, 0.0};
  LinearConstraint constraint = IntegerRoundingCutWithBoundsFromTrail(
      RoundingOptions(), rhs, vars, coeffs, lp_values, &model);
  EXPECT_EQ(constraint.DebugString(), "3*I0 2*I1 4*I2 3*I3 <= 4");
}

TEST(IntegerRoundingCutTest, LargeCoeffWithSmallImprecision) {
  Model model;
  const IntegerVariable x0 = model.Add(NewIntegerVariable(0, 5));
  const IntegerVariable x1 = model.Add(NewIntegerVariable(0, 5));

  // 1e6 x0 - x1 <= 1.5e6.
  const IntegerValue rhs = IntegerValue(1.5e6);
  std::vector<IntegerVariable> vars = {x0, x1};
  std::vector<IntegerValue> coeffs = {IntegerValue(1e6), IntegerValue(-1)};

  // Note that without adjustment, this returns 2 * I0 - I1 <= 2.
  // TODO(user): expose parameters so this can be verified other than manually?
  std::vector<double> lp_values{1.5, 0.1};
  LinearConstraint constraint = IntegerRoundingCutWithBoundsFromTrail(
      RoundingOptions(), rhs, vars, coeffs, lp_values, &model);
  EXPECT_EQ(constraint.DebugString(), "1*I0 <= 1");
}

TEST(IntegerRoundingCutTest, LargeCoeffWithSmallImprecision2) {
  Model model;
  const IntegerVariable x0 = model.Add(NewIntegerVariable(0, 5));
  const IntegerVariable x1 = model.Add(NewIntegerVariable(0, 5));

  // 1e6 x0 + 999999 * x1 <= 1.5e6.
  const IntegerValue rhs = IntegerValue(1.5e6);
  std::vector<IntegerVariable> vars = {x0, x1};
  std::vector<IntegerValue> coeffs = {IntegerValue(1e6), IntegerValue(999999)};

  // Note that without adjustment, this returns 2 * I0 + I1 <= 2.
  // TODO(user): expose parameters so this can be verified other than manually?
  std::vector<double> lp_values{1.49, 0.1};
  LinearConstraint constraint = IntegerRoundingCutWithBoundsFromTrail(
      RoundingOptions(), rhs, vars, coeffs, lp_values, &model);
  EXPECT_EQ(constraint.DebugString(), "1*I0 1*I1 <= 1");
}

TEST(IntegerRoundingCutTest, MirOnLargerConstraint) {
  Model model;
  std::vector<IntegerVariable> vars(10);
  for (int i = 0; i < 10; ++i) {
    vars[i] = model.Add(NewIntegerVariable(0, 5));
  }

  // sum (i + 1) x_i <= 16.
  const IntegerValue rhs = IntegerValue(16);
  std::vector<IntegerValue> coeffs;
  for (int i = 0; i < vars.size(); ++i) {
    coeffs.push_back(IntegerValue(i + 1));
  }

  std::vector<double> lp_values(vars.size(), 0.0);
  lp_values[9] = 1.6;  // 10 * 1.6 == 16

  RoundingOptions options;
  options.max_scaling = 4;
  LinearConstraint constraint = IntegerRoundingCutWithBoundsFromTrail(
      options, rhs, vars, coeffs, lp_values, &model);
  EXPECT_EQ(constraint.DebugString(), "1*I6 2*I7 3*I8 4*I9 <= 4");
}

TEST(IntegerRoundingCutTest, MirOnLargerConstraint2) {
  Model model;
  std::vector<IntegerVariable> vars(10);
  for (int i = 0; i < 10; ++i) vars[i] = model.Add(NewIntegerVariable(0, 5));

  // sum (i + 1) x_i <= 16.
  const IntegerValue rhs = IntegerValue(16);
  std::vector<IntegerValue> coeffs;
  for (int i = 0; i < vars.size(); ++i) {
    coeffs.push_back(IntegerValue(i + 1));
  }

  std::vector<double> lp_values(vars.size(), 0.0);
  lp_values[4] = 5.5 / 5.0;
  lp_values[9] = 1.05;

  RoundingOptions options;
  options.max_scaling = 4;
  LinearConstraint constraint = IntegerRoundingCutWithBoundsFromTrail(
      options, rhs, vars, coeffs, lp_values, &model);
  EXPECT_EQ(constraint.DebugString(),
            "2*I1 3*I2 4*I3 6*I4 6*I5 8*I6 9*I7 10*I8 12*I9 <= 18");
}

std::vector<IntegerValue> ToIntegerValues(const std::vector<int64_t> input) {
  std::vector<IntegerValue> output;
  for (const int64_t v : input) output.push_back(IntegerValue(v));
  return output;
}

std::vector<IntegerVariable> ToIntegerVariables(
    const std::vector<int64_t> input) {
  std::vector<IntegerVariable> output;
  for (const int64_t v : input) output.push_back(IntegerVariable(v));
  return output;
}

// This used to fail as I was coding the CL when I was trying to force t==1
// in the GetSuperAdditiveRoundingFunction() code.
TEST(IntegerRoundingCutTest, RegressionTest) {
  RoundingOptions options;
  options.max_scaling = 4;

  const IntegerValue rhs = int64_t{7469520585651099083};
  std::vector<IntegerVariable> vars = ToIntegerVariables(
      {0,  2,  4,  6,  8,  10, 12, 14, 16, 18, 20, 22, 24, 26,
       28, 30, 32, 34, 36, 38, 42, 44, 46, 48, 50, 52, 54, 56});
  std::vector<IntegerValue> coeffs = ToIntegerValues(
      {22242929208935956LL,  128795791007031270LL, 64522773588815932LL,
       106805487542181976LL, 136903984044996548LL, 177476314670499137LL,
       364043443034395LL,    28002509947960647LL,  310965596097558939LL,
       103949088324014599LL, 41400520193055115LL,  50111468002532494LL,
       53821870865384327LL,  68690238549704032LL,  75189534851923882LL,
       136250652059774801LL, 169776580612315087LL, 172493907306536826LL,
       13772608007357656LL,  74052819842959090LL,  134400722410234077LL,
       5625133860678171LL,   299572729577293761LL, 81099235700461109LL,
       178989907222373586LL, 16642124499479353LL,  110378717916671350LL,
       41703587448036910LL});
  std::vector<double> lp_values = {
      0,       0,        2.51046,  0.0741114, 0.380072, 5.17238,  0,
      0,       13.2214,  0,        0.635977,  0,        0,        3.39859,
      1.15936, 0.165207, 2.29673,  2.19505,   0,        0,        2.31191,
      0,       0.785149, 0.258119, 2.26978,   0,        0.970046, 0};
  std::vector<IntegerValue> lbs(28, IntegerValue(0));
  std::vector<IntegerValue> ubs(28, IntegerValue(99));
  ubs[8] = 17;
  std::vector<IntegerValue> solution =
      ToIntegerValues({0, 3, 0, 2, 2, 2, 0, 1, 5, 1, 1, 1, 1, 2,
                       0, 2, 1, 3, 1, 1, 4, 1, 6, 2, 3, 0, 1, 1});

  EXPECT_EQ(coeffs.size(), vars.size());
  EXPECT_EQ(lp_values.size(), vars.size());
  EXPECT_EQ(lbs.size(), vars.size());
  EXPECT_EQ(ubs.size(), vars.size());
  EXPECT_EQ(solution.size(), vars.size());

  // The solution is a valid integer solution of the inequality.
  {
    IntegerValue activity(0);
    for (int i = 0; i < vars.size(); ++i) {
      activity += solution[i] * coeffs[i];
    }
    EXPECT_LE(activity, rhs);
  }

  CutData data;
  data.FillFromParallelVectors(rhs, vars, coeffs, lp_values, lbs, ubs);
  Model model;
  IntegerRoundingCutHelper helper(&model);

  // TODO(user): Actually this fails, so we don't compute a cut here.
  EXPECT_FALSE(helper.ComputeCut(options, data, nullptr));
}

void InitializeLpValues(absl::Span<const double> values, Model* model) {
  auto* lp_values = model->GetOrCreate<ModelLpValues>();
  lp_values->resize(2 * values.size());
  for (int i = 0; i < values.size(); ++i) {
    (*lp_values)[IntegerVariable(2 * i)] = values[i];
    (*lp_values)[IntegerVariable(2 * i + 1)] = -values[i];
  }
}

TEST(SquareCutGeneratorTest, TestBelowCut) {
  Model model;
  IntegerVariable x = model.Add(NewIntegerVariable(0, 5));
  IntegerVariable y = model.Add(NewIntegerVariable(0, 25));
  InitializeLpValues({2.0, 12.0}, &model);

  CutGenerator square = CreateSquareCutGenerator(y, x, 1, &model);
  auto* manager = model.GetOrCreate<LinearConstraintManager>();
  square.generate_cuts(manager);
  EXPECT_EQ(1, manager->num_cuts());
  EXPECT_THAT(manager->AllConstraints().front().constraint.DebugString(),
              StartsWith("0 <= 5*I0 -1*I1"));
}

TEST(SquareCutGeneratorTest, TestBelowCutWithOffset) {
  Model model;
  IntegerVariable x = model.Add(NewIntegerVariable(1, 5));
  IntegerVariable y = model.Add(NewIntegerVariable(1, 25));
  InitializeLpValues({2.0, 12.0}, &model);

  CutGenerator square = CreateSquareCutGenerator(y, x, 1, &model);
  auto* manager = model.GetOrCreate<LinearConstraintManager>();
  square.generate_cuts(manager);
  ASSERT_EQ(1, manager->num_cuts());
  EXPECT_THAT(manager->AllConstraints().front().constraint.DebugString(),
              StartsWith("5 <= 6*I0 -1*I1"));
}

TEST(SquareCutGeneratorTest, TestNoBelowCut) {
  Model model;
  IntegerVariable x = model.Add(NewIntegerVariable(1, 5));
  IntegerVariable y = model.Add(NewIntegerVariable(1, 25));
  InitializeLpValues({2.0, 6.0}, &model);

  CutGenerator square = CreateSquareCutGenerator(y, x, 1, &model);
  auto* manager = model.GetOrCreate<LinearConstraintManager>();
  square.generate_cuts(manager);
  ASSERT_EQ(0, manager->num_cuts());
}

TEST(SquareCutGeneratorTest, TestAboveCut) {
  Model model;
  IntegerVariable x = model.Add(NewIntegerVariable(1, 5));
  IntegerVariable y = model.Add(NewIntegerVariable(1, 25));
  InitializeLpValues({2.5, 6.25}, &model);

  CutGenerator square = CreateSquareCutGenerator(y, x, 1, &model);
  auto* manager = model.GetOrCreate<LinearConstraintManager>();
  square.generate_cuts(manager);
  ASSERT_EQ(1, manager->num_cuts());
  EXPECT_THAT(manager->AllConstraints().front().constraint.DebugString(),
              EndsWith("5*I0 -1*I1 <= 6"));
}

TEST(SquareCutGeneratorTest, TestNearlyAboveCut) {
  Model model;
  IntegerVariable x = model.Add(NewIntegerVariable(1, 5));
  IntegerVariable y = model.Add(NewIntegerVariable(1, 25));
  InitializeLpValues({2.4, 5.99999}, &model);

  CutGenerator square = CreateSquareCutGenerator(y, x, 1, &model);
  auto* manager = model.GetOrCreate<LinearConstraintManager>();
  square.generate_cuts(manager);
  ASSERT_EQ(0, manager->num_cuts());
}

TEST(MultiplicationCutGeneratorTest, TestCut1) {
  Model model;
  IntegerVariable x = model.Add(NewIntegerVariable(1, 5));
  IntegerVariable y = model.Add(NewIntegerVariable(2, 3));
  IntegerVariable z = model.Add(NewIntegerVariable(1, 15));
  InitializeLpValues({1.2, 2.1, 2.1}, &model);

  CutGenerator mult =
      CreatePositiveMultiplicationCutGenerator(z, x, y, 1, &model);
  auto* manager = model.GetOrCreate<LinearConstraintManager>();
  mult.generate_cuts(manager);
  ASSERT_EQ(1, manager->num_cuts());
  EXPECT_THAT(manager->AllConstraints().front().constraint.DebugString(),
              EndsWith("2*I0 1*I1 -1*I2 <= 2"));
}

TEST(MultiplicationCutGeneratorTest, TestCut2) {
  Model model;
  IntegerVariable x = model.Add(NewIntegerVariable(1, 5));
  IntegerVariable y = model.Add(NewIntegerVariable(2, 3));
  IntegerVariable z = model.Add(NewIntegerVariable(1, 15));
  InitializeLpValues({4.9, 2.8, 12.0}, &model);

  CutGenerator mult =
      CreatePositiveMultiplicationCutGenerator(z, x, y, 1, &model);
  auto* manager = model.GetOrCreate<LinearConstraintManager>();
  mult.generate_cuts(manager);
  ASSERT_EQ(1, manager->num_cuts());
  EXPECT_THAT(manager->AllConstraints().front().constraint.DebugString(),
              EndsWith("3*I0 5*I1 -1*I2 <= 15"));
}

TEST(MultiplicationCutGeneratorTest, TestCut3) {
  Model model;
  IntegerVariable x = model.Add(NewIntegerVariable(1, 5));
  IntegerVariable y = model.Add(NewIntegerVariable(2, 3));
  IntegerVariable z = model.Add(NewIntegerVariable(1, 15));
  InitializeLpValues({1.2, 2.1, 4.4}, &model);

  CutGenerator mult =
      CreatePositiveMultiplicationCutGenerator(z, x, y, 1, &model);
  auto* manager = model.GetOrCreate<LinearConstraintManager>();
  mult.generate_cuts(manager);
  ASSERT_EQ(2, manager->num_cuts());
  EXPECT_THAT(manager->AllConstraints().front().constraint.DebugString(),
              StartsWith("3 <= 3*I0 1*I1 -1*I2"));
  EXPECT_THAT(manager->AllConstraints().back().constraint.DebugString(),
              StartsWith("10 <= 2*I0 5*I1 -1*I2"));
}

TEST(MultiplicationCutGeneratorTest, TestNoCut1) {
  Model model;
  IntegerVariable x = model.Add(NewIntegerVariable(1, 50));
  IntegerVariable y = model.Add(NewIntegerVariable(2, 30));
  IntegerVariable z = model.Add(NewIntegerVariable(1, 1500));
  InitializeLpValues({40.0, 20.0, 799.0}, &model);

  CutGenerator mult =
      CreatePositiveMultiplicationCutGenerator(z, x, y, 1, &model);
  auto* manager = model.GetOrCreate<LinearConstraintManager>();
  mult.generate_cuts(manager);
  ASSERT_EQ(0, manager->num_cuts());
}

TEST(MultiplicationCutGeneratorTest, TestNoCut2) {
  Model model;
  IntegerVariable x = model.Add(NewIntegerVariable(1, 50));
  IntegerVariable y = model.Add(NewIntegerVariable(2, 30));
  IntegerVariable z = model.Add(NewIntegerVariable(1, 1500));
  InitializeLpValues({40.0, 20.0, 801.0}, &model);

  CutGenerator mult =
      CreatePositiveMultiplicationCutGenerator(z, x, y, 1, &model);
  auto* manager = model.GetOrCreate<LinearConstraintManager>();
  mult.generate_cuts(manager);
  ASSERT_EQ(0, manager->num_cuts());
}

TEST(AllDiffCutGeneratorTest, TestCut) {
  Model model;
  Domain domain(10);
  domain = domain.UnionWith(Domain(15));
  domain = domain.UnionWith(Domain(25));
  IntegerVariable x = model.Add(NewIntegerVariable(domain));
  IntegerVariable y = model.Add(NewIntegerVariable(domain));
  IntegerVariable z = model.Add(NewIntegerVariable(domain));
  InitializeLpValues({15.0, 15.0, 15.0}, &model);

  CutGenerator all_diff = CreateAllDifferentCutGenerator({x, y, z}, &model);
  auto* manager = model.GetOrCreate<LinearConstraintManager>();
  all_diff.generate_cuts(manager);
  ASSERT_EQ(1, manager->num_cuts());
  EXPECT_EQ(manager->AllConstraints().front().constraint.DebugString(),
            "50 <= 1*I0 1*I1 1*I2 <= 50");
}

TEST(AllDiffCutGeneratorTest, TestCut2) {
  Model model;
  Domain domain(10);
  domain = domain.UnionWith(Domain(15));
  domain = domain.UnionWith(Domain(25));
  IntegerVariable x = model.Add(NewIntegerVariable(domain));
  IntegerVariable y = model.Add(NewIntegerVariable(domain));
  IntegerVariable z = model.Add(NewIntegerVariable(domain));
  InitializeLpValues({13.0, 10.0, 12.0}, &model);

  CutGenerator all_diff = CreateAllDifferentCutGenerator({x, y, z}, &model);
  auto* manager = model.GetOrCreate<LinearConstraintManager>();
  all_diff.generate_cuts(manager);
  ASSERT_EQ(2, manager->num_cuts());
  EXPECT_EQ(manager->AllConstraints().front().constraint.DebugString(),
            "25 <= 1*I1 1*I2 <= 40");
  EXPECT_EQ(manager->AllConstraints().back().constraint.DebugString(),
            "50 <= 1*I0 1*I1 1*I2 <= 50");
}

// We model the maximum of 3 affine functions:
//  f0(x) = 1
//  f1(x) = -x0 - 2x1
//  f2(x) = -x0 + x1
// over the box domain -1 <= x0, x1 <= 1. For this data, there are 9 possible
// maximum corner cuts. I denote each by noting which function f^i each input
// variable x_j gets assigned:
//  (1) x0 -> f0, x1 -> f0: y <= 0x0 + 0x1 + 1z_0 + 3z_1 + 2z_2
//  (2) x0 -> f0, x1 -> f1: y <= 0x0 - 2x1 + 3z_0 + 1z_1 + 4z_2
//  (3) x0 -> f0, x1 -> f2: y <= 0x0 +  x1 + 2z_0 + 4z_1 + 1z_2
//  (4) x0 -> f1, x1 -> f0: y <= -x0 + 0x1 + 2z_0 + 2z_1 + 1z_2
//  (5) x0 -> f1, x1 -> f1: y <= -x0 - 2x1 + 4z_0 + 0z_1 + 3z_2
//  (6) x0 -> f1, x1 -> f2: y <= -x0 +  x1 + 3z_0 + 3z_1 + 0z_2
//  (7) x0 -> f2, x1 -> f0: y <= -x0 + 0x1 + 2z_0 + 2z_1 + 1z_2
//  (8) x0 -> f2, x1 -> f1: y <= -x0 - 2x1 + 4z_0 + 0z_1 + 3z_2
//  (9) x0 -> f2, x1 -> f2: y <= -x0 +  x1 + 3z_0 + 3z_1 + 0z_2
TEST(LinMaxCutsTest, BasicCuts1) {
  Model model;
  IntegerVariable x0 = model.Add(NewIntegerVariable(-1, 1));
  IntegerVariable x1 = model.Add(NewIntegerVariable(-1, 1));
  IntegerVariable target = model.Add(NewIntegerVariable(-100, 100));
  LinearExpression f0;
  f0.offset = IntegerValue(1);
  LinearExpression f1;
  f1.vars = {x0, x1};
  f1.coeffs = {IntegerValue(-1), IntegerValue(-2)};
  LinearExpression f2;
  f2.vars = {x0, x1};
  f2.coeffs = {IntegerValue(-1), IntegerValue(1)};

  std::vector<LinearExpression> exprs = {f0, f1, f2};
  std::vector<IntegerVariable> z_vars;
  for (int i = 0; i < exprs.size(); ++i) {
    IntegerVariable z = model.Add(NewIntegerVariable(0, 1));
    z_vars.push_back(z);
  }

  CutGenerator max_cuts =
      CreateLinMaxCutGenerator(target, exprs, z_vars, &model);

  auto* manager = model.GetOrCreate<LinearConstraintManager>();
  InitializeLpValues({-1.0, 1.0, 2.0, 1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0}, &model);

  max_cuts.generate_cuts(manager);
  ASSERT_EQ(1, manager->num_cuts());

  // x vars are I0,I1 respectively, target is I2, z_vars are I3,I4,I5
  // respectively.
  // Most violated inequality is 2.
  EXPECT_THAT(manager->AllConstraints().front().constraint.DebugString(),
              EndsWith("2*I1 1*I2 -3*I3 -1*I4 -4*I5 <= 0"));

  InitializeLpValues({-1.0, -1.0, 2.0, 1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0},
                     &model);
  max_cuts.generate_cuts(manager);
  ASSERT_EQ(2, manager->num_cuts());
  // Most violated inequality is 3.
  EXPECT_THAT(manager->AllConstraints().back().constraint.DebugString(),
              StartsWith("0 <= 1*I1 -1*I2 2*I3 4*I4 1*I5"));
}

// We model the maximum of 3 affine functions:
//  f0(x) = 1
//  f1(x) = x
//  f2(x) = -x
//  target = max(f0, f1, f2)
//  x in [-10, 10]
TEST(LinMaxCutsTest, AffineCuts1) {
  Model model;
  const IntegerValue zero(0);
  const IntegerValue one(1);
  IntegerVariable x = model.Add(NewIntegerVariable(-10, 10));
  IntegerVariable target = model.Add(NewIntegerVariable(1, 100));
  LinearExpression target_expr;
  target_expr.vars.push_back(target);
  target_expr.coeffs.push_back(one);

  std::vector<std::pair<IntegerValue, IntegerValue>> affines = {
      {zero, one}, {one, zero}, {-one, zero}};

  LinearConstraintBuilder builder(&model);
  ASSERT_TRUE(
      BuildMaxAffineUpConstraint(target_expr, x, affines, &model, &builder));

  // Note, the cut is not normalized.
  EXPECT_EQ(builder.Build().DebugString(), "20*I1 <= 200");
}

// We model the maximum of 3 affine functions:
//  f0(x) = 1
//  f1(x) = x
//  f2(x) = -x
//  target = max(f0, f1, f2)
//  x in [-1, 10]
TEST(LinMaxCutsTest, AffineCuts2) {
  Model model;
  const IntegerValue zero(0);
  const IntegerValue one(1);
  IntegerVariable x = model.Add(NewIntegerVariable(-1, 10));
  IntegerVariable target = model.Add(NewIntegerVariable(1, 100));
  LinearExpression target_expr;
  target_expr.vars.push_back(target);
  target_expr.coeffs.push_back(one);

  std::vector<std::pair<IntegerValue, IntegerValue>> affines = {
      {zero, one}, {one, zero}, {-one, zero}};

  LinearConstraintBuilder builder(&model);
  ASSERT_TRUE(
      BuildMaxAffineUpConstraint(target_expr, x, affines, &model, &builder));

  EXPECT_EQ(builder.Build().DebugString(), "-9*I0 11*I1 <= 20");
}

// We model the maximum of 3 affine functions:
//  f0(x) = 1
//  f1(x) = x
//  f2(x) = -x
//  target = max(f0, f1, f2)
//  x fixed
TEST(LinMaxCutsTest, AffineCutsFixedVar) {
  Model model;
  const IntegerValue zero(0);
  const IntegerValue one(1);
  IntegerVariable x = model.Add(NewIntegerVariable(2, 2));
  IntegerVariable target = model.Add(NewIntegerVariable(0, 100));
  LinearExpression target_expr;
  target_expr.vars.push_back(target);
  target_expr.coeffs.push_back(one);

  std::vector<std::pair<IntegerValue, IntegerValue>> affines = {
      {zero, one}, {one, zero}, {-one, zero}};

  CutGenerator max_cuts =
      CreateMaxAffineCutGenerator(target_expr, x, affines, "test", &model);

  auto* manager = model.GetOrCreate<LinearConstraintManager>();
  InitializeLpValues({2.0, 8.0}, &model);
  max_cuts.generate_cuts(manager);
  EXPECT_EQ(0, manager->num_cuts());
}

MATCHER_P3(CutDataIs, coeff_matcher, lp_matcher, range_matcher, "") {
  return ExplainMatchResult(coeff_matcher, arg.coeff.value(),
                            result_listener) &&
         ExplainMatchResult(range_matcher, arg.bound_diff.value(),
                            result_listener) &&
         ExplainMatchResult(lp_matcher, arg.lp_value, result_listener);
}

TEST(ImpliedBoundsProcessorTest, PositiveBasicTest) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_use_implied_bounds(true);

  const BooleanVariable b = model.Add(NewBooleanVariable());
  const IntegerVariable b_view = model.Add(NewIntegerVariable(0, 1));
  const IntegerVariable x = model.Add(NewIntegerVariable(2, 9));

  auto* integer_encoder = model.GetOrCreate<IntegerEncoder>();
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* implied_bounds = model.GetOrCreate<ImpliedBounds>();

  integer_encoder->AssociateToIntegerEqualValue(Literal(b, true), b_view,
                                                IntegerValue(1));
  implied_bounds->Add(Literal(b, true),
                      IntegerLiteral::GreaterOrEqual(x, IntegerValue(5)));

  // Lp solution.
  ImpliedBoundsProcessor processor({x, b_view}, integer_trail, implied_bounds);

  util_intops::StrongVector<IntegerVariable, double> lp_values(1000);
  lp_values[x] = 4.0;
  lp_values[b_view] = 2.0 / 3.0;  // 2.0 + b_view_value * (5-2) == 4.0
  processor.RecomputeCacheAndSeparateSomeImpliedBoundCuts(lp_values);

  // Lets look at the term X.
  CutData data;

  CutTerm X;
  X.coeff = 1;
  X.lp_value = 2.0;
  X.bound_diff = 7;
  X.expr_vars[0] = x;
  X.expr_coeffs[0] = 1;
  X.expr_coeffs[1] = 0;
  X.expr_offset = -2;
  data.terms.push_back(X);

  processor.CacheDataForCut(IntegerVariable(100), &data);
  std::vector<CutTerm> new_terms;
  EXPECT_TRUE(processor.TryToExpandWithLowerImpliedbound(
      /*complement=*/false, &data.terms[0], &data.rhs, &new_terms));

  EXPECT_EQ(0, processor.MutableCutBuilder()->AddOrMergeBooleanTerms(
                   absl::MakeSpan(new_terms), &data));

  EXPECT_EQ(data.terms.size(), 2);
  EXPECT_THAT(data.terms[0], CutDataIs(1, DoubleNear(0.0, 1.0e-6), 7));
  EXPECT_THAT(data.terms[1], CutDataIs(3, DoubleNear(0.666667, 1.0e-6), 1));
  EXPECT_EQ(data.terms[1].expr_offset, 0);
}

// Same as above but with b.Negated()
TEST(ImpliedBoundsProcessorTest, NegativeBasicTest) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_use_implied_bounds(true);

  const BooleanVariable b = model.Add(NewBooleanVariable());
  const IntegerVariable b_view = model.Add(NewIntegerVariable(0, 1));
  const IntegerVariable x = model.Add(NewIntegerVariable(2, 9));

  auto* integer_encoder = model.GetOrCreate<IntegerEncoder>();
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* implied_bounds = model.GetOrCreate<ImpliedBounds>();

  integer_encoder->AssociateToIntegerEqualValue(Literal(b, true), b_view,
                                                IntegerValue(1));
  implied_bounds->Add(Literal(b, false),  // False here.
                      IntegerLiteral::GreaterOrEqual(x, IntegerValue(5)));

  // Lp solution.
  ImpliedBoundsProcessor processor({x, b_view}, integer_trail, implied_bounds);

  util_intops::StrongVector<IntegerVariable, double> lp_values(1000);
  lp_values[x] = 4.0;
  lp_values[b_view] = 1.0 - 2.0 / 3.0;  // 1 - value above.
  processor.RecomputeCacheAndSeparateSomeImpliedBoundCuts(lp_values);

  // Lets look at the term X.
  CutData data;

  CutTerm X;
  X.coeff = 1;
  X.lp_value = 2.0;
  X.bound_diff = 7;
  X.expr_vars[0] = x;
  X.expr_coeffs[0] = 1;
  X.expr_coeffs[1] = 0;
  X.expr_offset = -2;
  data.terms.push_back(X);

  processor.CacheDataForCut(IntegerVariable(100), &data);

  std::vector<CutTerm> new_terms;
  EXPECT_TRUE(processor.TryToExpandWithLowerImpliedbound(
      /*complement=*/false, &data.terms[0], &data.rhs, &new_terms));
  EXPECT_EQ(0, processor.MutableCutBuilder()->AddOrMergeBooleanTerms(
                   absl::MakeSpan(new_terms), &data));

  EXPECT_EQ(data.terms.size(), 2);
  EXPECT_THAT(data.terms[0], CutDataIs(1, DoubleNear(0.0, 1.0e-6), 7));
  EXPECT_THAT(data.terms[1], CutDataIs(3, DoubleNear(0.666667, 1.0e-6), 1));

  // This is the only change, we have 1 - bool there actually.
  EXPECT_EQ(data.terms[1].expr_offset, 1);
  EXPECT_EQ(data.terms[1].expr_coeffs[0], -1);
  EXPECT_EQ(data.terms[1].expr_vars[0], b_view);
}

TEST(ImpliedBoundsProcessorTest, DecompositionTest) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_use_implied_bounds(true);

  const BooleanVariable b = model.Add(NewBooleanVariable());
  const IntegerVariable b_view = model.Add(NewIntegerVariable(0, 1));
  const BooleanVariable c = model.Add(NewBooleanVariable());
  const IntegerVariable c_view = model.Add(NewIntegerVariable(0, 1));
  const IntegerVariable x = model.Add(NewIntegerVariable(2, 9));

  auto* integer_encoder = model.GetOrCreate<IntegerEncoder>();
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* implied_bounds = model.GetOrCreate<ImpliedBounds>();

  integer_encoder->AssociateToIntegerEqualValue(Literal(b, true), b_view,
                                                IntegerValue(1));
  integer_encoder->AssociateToIntegerEqualValue(Literal(c, true), c_view,
                                                IntegerValue(1));
  implied_bounds->Add(Literal(b, true),
                      IntegerLiteral::GreaterOrEqual(x, IntegerValue(5)));
  implied_bounds->Add(Literal(c, true),
                      IntegerLiteral::LowerOrEqual(x, IntegerValue(2)));

  // Lp solution.
  ImpliedBoundsProcessor processor({x, b_view, c_view}, integer_trail,
                                   implied_bounds);

  util_intops::StrongVector<IntegerVariable, double> lp_values(1000);
  lp_values[x] = 4.0;
  lp_values[NegationOf(x)] = -4.0;
  lp_values[b_view] = 2.0 / 3.0;  // 2.0 + b_view_value * (5-2) == 4.0
  lp_values[c_view] = 0.5;
  processor.RecomputeCacheAndSeparateSomeImpliedBoundCuts(lp_values);

  // Lets look at the term X.
  CutTerm X;
  X.coeff = 1;
  X.lp_value = 2.0;
  X.bound_diff = 7;
  X.expr_vars[0] = x;
  X.expr_coeffs[0] = 1;
  X.expr_coeffs[1] = 0;
  X.expr_offset = -2;

  CutData data;
  data.terms.push_back(X);
  processor.CacheDataForCut(IntegerVariable(100), &data);
  X = data.terms[0];

  // X - 2 = 3 * B + slack;
  CutTerm bool_term;
  CutTerm slack_term;
  EXPECT_TRUE(
      processor.DecomposeWithImpliedLowerBound(X, bool_term, slack_term));
  EXPECT_THAT(bool_term, CutDataIs(3, DoubleNear(0.666667, 1.0e-6), 1));
  EXPECT_THAT(slack_term, CutDataIs(1, DoubleNear(0.0, 1.0e-6), 7));

  // (9 - X) = 7 * C + slack;
  CutTerm Y = X;
  absl::int128 unused;
  Y.Complement(&unused);
  Y.coeff = -Y.coeff;
  EXPECT_TRUE(
      processor.DecomposeWithImpliedLowerBound(Y, bool_term, slack_term));
  EXPECT_THAT(bool_term, CutDataIs(7, DoubleNear(0.5, 1.0e-6), 1));
  EXPECT_THAT(slack_term, CutDataIs(1, DoubleNear(1.5, 1.0e-6), 7));

  // X - 2 = 7 * (1 - C) - slack;
  EXPECT_TRUE(
      processor.DecomposeWithImpliedUpperBound(X, bool_term, slack_term));
  EXPECT_THAT(bool_term, CutDataIs(7, DoubleNear(0.5, 1.0e-6), 1));
  EXPECT_THAT(slack_term, CutDataIs(-1, DoubleNear(1.5, 1.0e-6), 7));
}

TEST(CoverCutHelperTest, SingleNodeFlowWithSemiContinuousImpliedBounds) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_use_implied_bounds(true);

  // Three discretized semi-continuous arcs 25000 * z_i <= x_i <= 500000 * z_i
  // (U = 500000 >> 6000) plus one small binary x3 in [0, 1].
  // Constraint: x0 + x1 + x2 + x3 <= 800000.
  std::vector<IntegerVariable> x_vars;
  std::vector<IntegerVariable> z_vars;
  auto* integer_encoder = model.GetOrCreate<IntegerEncoder>();
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* implied_bounds = model.GetOrCreate<ImpliedBounds>();
  for (int i = 0; i < 3; ++i) {
    x_vars.push_back(model.Add(NewIntegerVariable(0, 500000)));
  }
  const IntegerVariable x3 = model.Add(NewIntegerVariable(0, 1));
  for (int i = 0; i < 3; ++i) {
    const BooleanVariable b = model.Add(NewBooleanVariable());
    const IntegerVariable z = model.Add(NewIntegerVariable(0, 1));
    z_vars.push_back(z);
    integer_encoder->AssociateToIntegerEqualValue(Literal(b, true), z,
                                                  IntegerValue(1));
    implied_bounds->Add(Literal(b, true), IntegerLiteral::GreaterOrEqual(
                                              x_vars[i], IntegerValue(25000)));
    implied_bounds->Add(Literal(b, false), IntegerLiteral::LowerOrEqual(
                                               x_vars[i], IntegerValue(0)));
  }

  std::vector<IntegerVariable> lp_vars = {x_vars[0], x_vars[1], x_vars[2], x3,
                                          z_vars[0], z_vars[1], z_vars[2]};
  ImpliedBoundsProcessor processor(lp_vars, integer_trail, implied_bounds);

  util_intops::StrongVector<IntegerVariable, double> lp_values(100);
  const std::vector<double> x_lp = {400000.0, 399999.5, 0.0};
  const std::vector<double> z_lp = {0.8, 399999.5 / 500000.0, 0.0};
  for (int i = 0; i < 3; ++i) {
    lp_values[x_vars[i]] = x_lp[i];
    lp_values[NegationOf(x_vars[i])] = -x_lp[i];
    lp_values[z_vars[i]] = z_lp[i];
    lp_values[NegationOf(z_vars[i])] = -z_lp[i];
  }
  lp_values[x3] = 0.5;
  lp_values[NegationOf(x3)] = -0.5;
  processor.RecomputeCacheAndSeparateSomeImpliedBoundCuts(lp_values);

  CutData data;
  data.FillFromParallelVectors(
      IntegerValue(800000), {x_vars[0], x_vars[1], x_vars[2], x3},
      IntegerValueVector({1, 1, 1, 1}), {x_lp[0], x_lp[1], x_lp[2], 0.5},
      IntegerValueVector({0, 0, 0, 0}),
      IntegerValueVector({500000, 500000, 500000, 1}));
  ASSERT_TRUE(processor.CacheDataForCut(IntegerVariable(100), &data));

  CoverCutHelper helper(&model);
  ASSERT_TRUE(helper.TrySingleNodeFlow(data, &processor));
  // Minimal cover {x0, x1} has excess lambda = 1000000 - 800000 = 200000,
  // yielding the facet x0 - 300000*z0 + x1 - 300000*z1 <= 200000.
  EXPECT_EQ(GetCutString(helper), "1*I0 1*I1 -300000*I4 -300000*I5 <= 200000");
}

TEST(CoverCutHelperTest, SingleNodeFlowDefersActiveIndicatorPreDecomposition) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_use_implied_bounds(true);

  // x0 in [0, 600000] with 25000*z0 <= x0 <= 600000*z0 (active: z0* = 1.0),
  // x1 in [0, 350000] with 25000*z1 <= x1 <= 350000*z1 (fractional: z1* = 0.5),
  // and binary y in [0, 1] on x0 + x1 + 250000*y <= 800000.
  // Deferring x0's implied-bound decomposition (since 1 - z0* == 0) keeps
  // period = 350000 instead of 600000, lifting +250000*y to +50000*y while
  // PostprocessWithImpliedBound() still lifts z0.
  auto* integer_encoder = model.GetOrCreate<IntegerEncoder>();
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* implied_bounds = model.GetOrCreate<ImpliedBounds>();
  const IntegerVariable x0 = model.Add(NewIntegerVariable(0, 600000));
  const IntegerVariable x1 = model.Add(NewIntegerVariable(0, 350000));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 1));
  std::vector<IntegerVariable> z_vars;
  for (const IntegerVariable x : {x0, x1}) {
    const BooleanVariable b = model.Add(NewBooleanVariable());
    const IntegerVariable z = model.Add(NewIntegerVariable(0, 1));
    z_vars.push_back(z);
    integer_encoder->AssociateToIntegerEqualValue(Literal(b, true), z,
                                                  IntegerValue(1));
    implied_bounds->Add(Literal(b, true),
                        IntegerLiteral::GreaterOrEqual(x, IntegerValue(25000)));
    implied_bounds->Add(Literal(b, false),
                        IntegerLiteral::LowerOrEqual(x, IntegerValue(0)));
  }

  std::vector<IntegerVariable> lp_vars = {x0, x1, y, z_vars[0], z_vars[1]};
  ImpliedBoundsProcessor processor(lp_vars, integer_trail, implied_bounds);

  util_intops::StrongVector<IntegerVariable, double> lp_values(100);
  const std::vector<double> vals = {525000.0, 175000.0, 0.4, 1.0, 0.5};
  for (int i = 0; i < lp_vars.size(); ++i) {
    lp_values[lp_vars[i]] = vals[i];
    lp_values[NegationOf(lp_vars[i])] = -vals[i];
  }
  processor.RecomputeCacheAndSeparateSomeImpliedBoundCuts(lp_values);

  CutData data;
  data.FillFromParallelVectors(
      IntegerValue(800000), {x0, x1, y}, IntegerValueVector({1, 1, 250000}),
      {525000.0, 175000.0, 0.4}, IntegerValueVector({0, 0, 0}),
      IntegerValueVector({600000, 350000, 1}));
  ASSERT_TRUE(processor.CacheDataForCut(IntegerVariable(100), &data));

  CoverCutHelper helper(&model);
  ASSERT_TRUE(helper.TrySingleNodeFlow(data, &processor));
  EXPECT_EQ(GetCutString(helper),
            "1*I0 1*I1 50000*I2 -300000*I3 -200000*I4 <= 300000");
}

TEST(CoverCutHelperTest, MinimizeCoverPreservesGreedyRemovalOrder) {
  Model model;
  std::vector<IntegerVariable> vars;
  for (int i = 0; i < 4; ++i) {
    vars.push_back(model.Add(NewIntegerVariable(0, 1)));
  }

  // On 10*x0 + 2*x1 + 2*x2 + 2*x3 <= 11 with x* = (0.4, 0.92, 0.94, 1.0), all
  // four variables enter the initial cover (slack = 5). Sorted for removal as
  // [x0, x1, x2, x3], greedy minimization keeps x0 (10 >= 5), drops x1 and x2
  // (slack 5 -> 3 -> 1), and keeps the tight variable x3* = 1.0 to yield
  // x0 + x3 <= 1 rather than swapping x3 into x1's slot and dropping it.
  CutData data;
  data.FillFromParallelVectors(
      IntegerValue(11), vars, IntegerValueVector({10, 2, 2, 2}),
      {0.4, 0.92, 0.94, 1.0}, IntegerValueVector({0, 0, 0, 0}),
      IntegerValueVector({1, 1, 1, 1}));

  CoverCutHelper helper(&model);
  ASSERT_TRUE(helper.TrySimpleKnapsack(data));
  EXPECT_EQ(GetCutString(helper), "1*I0 1*I3 <= 1");
  ASSERT_TRUE(helper.TryWithLetchfordSouliLifting(data));
  EXPECT_EQ(GetCutString(helper), "1*I0 1*I3 <= 1");
  ASSERT_TRUE(helper.TrySingleNodeFlow(data));
  EXPECT_EQ(GetCutString(helper), "1*I0 1*I3 <= 1");
}

TEST(CoverCutHelperTest, SimpleKnapsackWithTightUpperImpliedBoundsOption) {
  auto run_with_option = [](bool use_tight_upper_ib, std::string* cut_str) {
    Model model;
    auto* params = model.GetOrCreate<SatParameters>();
    params->set_use_implied_bounds(true);
    params->set_use_tight_upper_implied_bounds_in_cuts(use_tight_upper_ib);

    std::vector<IntegerVariable> x_vars;
    std::vector<IntegerVariable> z_vars;
    auto* integer_encoder = model.GetOrCreate<IntegerEncoder>();
    auto* integer_trail = model.GetOrCreate<IntegerTrail>();
    auto* implied_bounds = model.GetOrCreate<ImpliedBounds>();
    for (int i = 0; i < 3; ++i) {
      x_vars.push_back(model.Add(NewIntegerVariable(0, 500000)));
    }
    for (int i = 0; i < 3; ++i) {
      const BooleanVariable b = model.Add(NewBooleanVariable());
      const IntegerVariable z = model.Add(NewIntegerVariable(0, 1));
      z_vars.push_back(z);
      integer_encoder->AssociateToIntegerEqualValue(Literal(b, true), z,
                                                    IntegerValue(1));
      implied_bounds->Add(
          Literal(b, true),
          IntegerLiteral::GreaterOrEqual(x_vars[i], IntegerValue(25000)));
      implied_bounds->Add(Literal(b, false), IntegerLiteral::LowerOrEqual(
                                                 x_vars[i], IntegerValue(0)));
    }

    std::vector<IntegerVariable> lp_vars = {x_vars[0], x_vars[1], x_vars[2],
                                            z_vars[0], z_vars[1], z_vars[2]};
    ImpliedBoundsProcessor processor(lp_vars, integer_trail, implied_bounds);

    util_intops::StrongVector<IntegerVariable, double> lp_values(100);
    const std::vector<double> x_lp = {400000.0, 400000.0, 0.0};
    const std::vector<double> z_lp = {0.8, 0.8, 0.0};
    for (int i = 0; i < 3; ++i) {
      lp_values[x_vars[i]] = x_lp[i];
      lp_values[NegationOf(x_vars[i])] = -x_lp[i];
      lp_values[z_vars[i]] = z_lp[i];
      lp_values[NegationOf(z_vars[i])] = -z_lp[i];
    }
    processor.RecomputeCacheAndSeparateSomeImpliedBoundCuts(lp_values);

    CutData data;
    data.FillFromParallelVectors(IntegerValue(800000), x_vars,
                                 IntegerValueVector({1, 1, 1}), x_lp,
                                 IntegerValueVector({0, 0, 0}),
                                 IntegerValueVector({500000, 500000, 500000}));
    CHECK(processor.CacheDataForCut(IntegerVariable(100), &data));

    CoverCutHelper helper(&model);
    if (!helper.TrySimpleKnapsack(data, &processor)) return false;
    *cut_str = GetCutString(helper);
    return true;
  };

  std::string cut_without_option;
  EXPECT_FALSE(run_with_option(false, &cut_without_option));

  std::string cut_with_option;
  ASSERT_TRUE(run_with_option(true, &cut_with_option));
  EXPECT_EQ(cut_with_option, "1*I0 1*I1 -300000*I3 -300000*I4 <= 200000");
}

TEST(CoverCutHelperTest, LargeBoundDiffDoesNotExceedBoundOrTriggerZeroSlack) {
  // For bound_diff = 2^53 + 3 = 9007199254740995, IEEE-754 round-to-even rounds
  // static_cast<double>(bound_diff) UP to 9007199254740996.0 (> bound_diff);
  // for kMaxIntegerValue it rounds UP to 2^63 (outside signed int64_t). Verify
  // ImpliedRoundedUpValue() and CoverCutHelper clamp safely to [0, bound_diff]
  // and never call MinimizeCover() with slack == 0 when rhs == coeff *
  // bound_diff.
  ASSERT_GT(static_cast<int64_t>(static_cast<double>((int64_t{1} << 53) + 3)),
            (int64_t{1} << 53) + 3);
  for (const int64_t bound_diff :
       {(int64_t{1} << 53) + 1, (int64_t{1} << 53) + 3,
        kMaxIntegerValue.value()}) {
    Model model;
    model.GetOrCreate<SatParameters>()->set_use_implied_bounds(true);
    auto* integer_trail = model.GetOrCreate<IntegerTrail>();
    auto* implied_bounds = model.GetOrCreate<ImpliedBounds>();
    const IntegerVariable x = model.Add(NewIntegerVariable(0, bound_diff));
    const double lp_val = static_cast<double>(bound_diff);

    ImpliedBoundsProcessor processor({x}, integer_trail, implied_bounds);
    util_intops::StrongVector<IntegerVariable, double> lp_values(10);
    lp_values[x] = lp_val;
    lp_values[NegationOf(x)] = -lp_val;
    processor.RecomputeCacheAndSeparateSomeImpliedBoundCuts(lp_values);

    CutData data;
    data.FillFromParallelVectors(IntegerValue(bound_diff), {x},
                                 {IntegerValue(1)}, {lp_val}, {IntegerValue(0)},
                                 {IntegerValue(bound_diff)});
    processor.CacheDataForCut(IntegerVariable(10), &data);
    ASSERT_EQ(data.terms.size(), 1);
    const IntegerValue rounded = processor.ImpliedRoundedUpValue(data.terms[0]);
    EXPECT_GE(rounded, IntegerValue(0));
    EXPECT_LE(rounded, IntegerValue(bound_diff));
    if (bound_diff != (int64_t{1} << 53) + 1) {
      EXPECT_EQ(rounded, IntegerValue(bound_diff));
    }

    CoverCutHelper helper(&model);
    EXPECT_FALSE(helper.TrySingleNodeFlow(data, &processor));
    EXPECT_FALSE(helper.TrySingleNodeFlow(data, nullptr));
    EXPECT_FALSE(helper.TrySimpleKnapsack(data, &processor));
    EXPECT_FALSE(helper.TrySimpleKnapsack(data, nullptr));
  }
}

TEST(IntegerRoundingCutTest, SemiContinuousTightUpperImpliedBounds) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_use_implied_bounds(true);

  // Four semi-continuous variables 25000 * z_i <= x_i <= 500000 * z_i on
  // x0 + x1 + x2 - x3 <= 700000, with z* = (0.4, 0.4, 0.8, 0.2) and
  // x* = 500000 * z* = (200000, 200000, 400000, 100000).
  std::vector<IntegerVariable> x_vars;
  std::vector<IntegerVariable> z_vars;
  auto* integer_encoder = model.GetOrCreate<IntegerEncoder>();
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* implied_bounds = model.GetOrCreate<ImpliedBounds>();
  for (int i = 0; i < 4; ++i) {
    x_vars.push_back(model.Add(NewIntegerVariable(0, 500000)));
  }
  for (int i = 0; i < 4; ++i) {
    const BooleanVariable b = model.Add(NewBooleanVariable());
    const IntegerVariable z = model.Add(NewIntegerVariable(0, 1));
    z_vars.push_back(z);
    integer_encoder->AssociateToIntegerEqualValue(Literal(b, true), z,
                                                  IntegerValue(1));
    implied_bounds->Add(Literal(b, true), IntegerLiteral::GreaterOrEqual(
                                              x_vars[i], IntegerValue(25000)));
    implied_bounds->Add(Literal(b, false), IntegerLiteral::LowerOrEqual(
                                               x_vars[i], IntegerValue(0)));
  }

  std::vector<IntegerVariable> lp_vars = {x_vars[0], x_vars[1], x_vars[2],
                                          x_vars[3], z_vars[0], z_vars[1],
                                          z_vars[2], z_vars[3]};
  ImpliedBoundsProcessor processor(lp_vars, integer_trail, implied_bounds);

  util_intops::StrongVector<IntegerVariable, double> lp_values(100);
  const std::vector<double> x_lp = {200000.0, 200000.0, 400000.0, 100000.0};
  const std::vector<double> z_lp = {0.4, 0.4, 0.8, 0.2};
  for (int i = 0; i < 4; ++i) {
    lp_values[x_vars[i]] = x_lp[i];
    lp_values[NegationOf(x_vars[i])] = -x_lp[i];
    lp_values[z_vars[i]] = z_lp[i];
    lp_values[NegationOf(z_vars[i])] = -z_lp[i];
  }
  processor.RecomputeCacheAndSeparateSomeImpliedBoundCuts(lp_values);

  CutData data;
  data.FillFromParallelVectors(
      IntegerValue(700000), x_vars, IntegerValueVector({1, 1, 1, -1}), x_lp,
      IntegerValueVector({0, 0, 0, 0}),
      IntegerValueVector({500000, 500000, 500000, 500000}));
  ASSERT_TRUE(processor.CacheDataForCut(IntegerVariable(100), &data));
  data.ComplementForSmallerLpValues();

  RoundingOptions options;
  options.max_scaling = IntegerValue(600);
  options.use_ib_before_heuristic = true;
  options.prefer_positive_ib = true;
  options.use_tight_upper_ib = true;

  IntegerRoundingCutHelper helper(&model);
  ASSERT_TRUE(helper.ComputeCut(options, data, &processor));
  EXPECT_EQ(GetCutString(helper),
            "1*I0 1*I1 1*I2 -200000*I4 -200000*I5 -200000*I6 -300000*I7 <= "
            "300000");
}

TEST(IntegerRoundingCutTest, HighPrecisionScaledSemiContinuousImpliedBounds) {
  Model model;
  auto* params = model.GetOrCreate<SatParameters>();
  params->set_use_implied_bounds(true);
  params->set_use_tight_upper_implied_bounds_in_cuts(true);

  // Same semi-continuous structure as SemiContinuousTightUpperImpliedBounds,
  // but with mip_scale_for_precision-level row coefficients c ~ 1e8 and
  // U = 500000 so that c * U ~ 5e13 > sqrt(kint64max) and row GCD == 1.
  std::vector<IntegerVariable> x_vars;
  std::vector<IntegerVariable> z_vars;
  auto* integer_encoder = model.GetOrCreate<IntegerEncoder>();
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* implied_bounds = model.GetOrCreate<ImpliedBounds>();
  for (int i = 0; i < 4; ++i) {
    x_vars.push_back(model.Add(NewIntegerVariable(0, 500000)));
  }
  for (int i = 0; i < 4; ++i) {
    const BooleanVariable b = model.Add(NewBooleanVariable());
    const IntegerVariable z = model.Add(NewIntegerVariable(0, 1));
    z_vars.push_back(z);
    integer_encoder->AssociateToIntegerEqualValue(Literal(b, true), z,
                                                  IntegerValue(1));
    implied_bounds->Add(Literal(b, true), IntegerLiteral::GreaterOrEqual(
                                              x_vars[i], IntegerValue(25000)));
    implied_bounds->Add(Literal(b, false), IntegerLiteral::LowerOrEqual(
                                               x_vars[i], IntegerValue(0)));
  }

  std::vector<IntegerVariable> lp_vars = {x_vars[0], x_vars[1], x_vars[2],
                                          x_vars[3], z_vars[0], z_vars[1],
                                          z_vars[2], z_vars[3]};
  ImpliedBoundsProcessor processor(lp_vars, integer_trail, implied_bounds);

  util_intops::StrongVector<IntegerVariable, double> lp_values(100);
  const std::vector<double> x_lp = {225000.0, 225000.0, 400000.0, 100000.0};
  const std::vector<double> z_lp = {0.45, 0.45, 0.8, 0.2};
  for (int i = 0; i < 4; ++i) {
    lp_values[x_vars[i]] = x_lp[i];
    lp_values[NegationOf(x_vars[i])] = -x_lp[i];
    lp_values[z_vars[i]] = z_lp[i];
    lp_values[NegationOf(z_vars[i])] = -z_lp[i];
  }
  processor.RecomputeCacheAndSeparateSomeImpliedBoundCuts(lp_values);

  constexpr int64_t kC0 = 100000007LL;
  constexpr int64_t kC1 = 100000009LL;
  const int64_t rhs = 225000LL * kC0 + 225000LL * kC1 + 300000LL * kC0;

  CutData data;
  data.FillFromParallelVectors(
      IntegerValue(rhs), x_vars, IntegerValueVector({kC0, kC1, kC0, -kC0}),
      x_lp, IntegerValueVector({0, 0, 0, 0}),
      IntegerValueVector({500000, 500000, 500000, 500000}));
  ASSERT_TRUE(processor.CacheDataForCut(IntegerVariable(100), &data));

  CoverCutHelper cover_helper(&model);
  ASSERT_TRUE(cover_helper.TrySingleNodeFlow(data, &processor));
  LinearConstraint flow_ct;
  CutDataBuilder builder;
  ASSERT_TRUE(builder.ConvertToLinearConstraint(cover_helper.cut(), &flow_ct));
  EXPECT_FALSE(PossibleOverflow(*integer_trail, flow_ct));
  EXPECT_GT((ComputeActivity(flow_ct, lp_values) - ToDouble(flow_ct.ub)) /
                ComputeL2Norm(flow_ct),
            0.1);

  ASSERT_TRUE(cover_helper.TrySimpleKnapsack(data, &processor));
  LinearConstraint knapsack_ct;
  ASSERT_TRUE(
      builder.ConvertToLinearConstraint(cover_helper.cut(), &knapsack_ct));
  EXPECT_FALSE(PossibleOverflow(*integer_trail, knapsack_ct));
  EXPECT_GT(
      (ComputeActivity(knapsack_ct, lp_values) - ToDouble(knapsack_ct.ub)) /
          ComputeL2Norm(knapsack_ct),
      0.1);

  data.ComplementForSmallerLpValues();
  RoundingOptions options;
  options.max_scaling = IntegerValue(600);
  options.use_ib_before_heuristic = true;
  options.prefer_positive_ib = true;
  options.use_tight_upper_ib = true;

  IntegerRoundingCutHelper rounding_helper(&model);
  ASSERT_TRUE(rounding_helper.ComputeCut(options, data, &processor));
  LinearConstraint mir_ct;
  ASSERT_TRUE(
      builder.ConvertToLinearConstraint(rounding_helper.cut(), &mir_ct));
  EXPECT_FALSE(PossibleOverflow(*integer_trail, mir_ct));
  EXPECT_GT((ComputeActivity(mir_ct, lp_values) - ToDouble(mir_ct.ub)) /
                ComputeL2Norm(mir_ct),
            0.1);
}

TEST(IntegerRoundingCutTest, DeferredImpliedBoundUsesUnscaledNegativeSlack) {
  auto check_cut = [](double x0_lp, double z0_lp, double y_lp) {
    Model model;
    auto* params = model.GetOrCreate<SatParameters>();
    params->set_use_implied_bounds(true);
    params->set_use_tight_upper_implied_bounds_in_cuts(true);

    auto* integer_encoder = model.GetOrCreate<IntegerEncoder>();
    auto* integer_trail = model.GetOrCreate<IntegerTrail>();
    auto* implied_bounds = model.GetOrCreate<ImpliedBounds>();
    const IntegerVariable x0 = model.Add(NewIntegerVariable(0, 500000));
    const IntegerVariable y1 = model.Add(NewIntegerVariable(0, 1));
    const IntegerVariable y2 = model.Add(NewIntegerVariable(0, 1));
    const BooleanVariable b0 = model.Add(NewBooleanVariable());
    const IntegerVariable z0 = model.Add(NewIntegerVariable(0, 1));
    integer_encoder->AssociateToIntegerEqualValue(Literal(b0, true), z0,
                                                  IntegerValue(1));
    implied_bounds->Add(Literal(b0, true), IntegerLiteral::GreaterOrEqual(
                                               x0, IntegerValue(25000)));
    implied_bounds->Add(Literal(b0, false),
                        IntegerLiteral::LowerOrEqual(x0, IntegerValue(0)));

    std::vector<IntegerVariable> lp_vars = {x0, y1, y2, z0};
    ImpliedBoundsProcessor processor(lp_vars, integer_trail, implied_bounds);

    util_intops::StrongVector<IntegerVariable, double> lp_values(100);
    const std::vector<double> vals = {x0_lp, y_lp, y_lp, z0_lp};
    for (int i = 0; i < lp_vars.size(); ++i) {
      lp_values[lp_vars[i]] = vals[i];
      lp_values[NegationOf(lp_vars[i])] = -vals[i];
    }
    processor.RecomputeCacheAndSeparateSomeImpliedBoundCuts(lp_values);

    // On x0 + 500000*y1 + 500000*y2 <= 800000, complementing x0 yields
    // -X0' + 500000*y1 + 500000*y2 <= 300000 with X0' >= 500000*(1 - z0)
    // deferred to PostprocessWithImpliedBound() when use_ib_before_heuristic is
    // false. WillHaveNegativeIbSlack(cut_, ib_processor) must keep unscaled MIR
    // so the cut is not diluted by bucket scaling.
    CutData data;
    data.FillFromParallelVectors(
        IntegerValue(800000), {x0, y1, y2},
        IntegerValueVector({1, 500000, 500000}), {x0_lp, y_lp, y_lp},
        IntegerValueVector({0, 0, 0}), IntegerValueVector({500000, 1, 1}));
    ASSERT_TRUE(processor.CacheDataForCut(IntegerVariable(100), &data));
    data.ComplementForSmallerLpValues();

    RoundingOptions options;
    options.max_scaling = IntegerValue(60);
    options.use_ib_before_heuristic = false;
    options.use_tight_upper_ib = true;

    IntegerRoundingCutHelper helper(&model);
    ASSERT_TRUE(helper.ComputeCut(options, data, &processor));
    EXPECT_EQ(GetCutString(helper),
              "1*I0 200000*I1 200000*I2 -300000*I3 <= 200000");
  };

  check_cut(/*x0_lp=*/400000.0, /*z0_lp=*/0.8, /*y_lp=*/0.4);
  check_cut(/*x0_lp=*/500000.0, /*z0_lp=*/1.0, /*y_lp=*/0.3);
}

TEST(IntegerRoundingCutTest, SmallDivisorSearchUsesBaseDivisor) {
  Model model;
  const IntegerVariable x0 = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable x1 = model.Add(NewIntegerVariable(0, 10));

  // On 60*x0 + 100*x1 <= 359 at x* = (3.65, 1.4), the initial candidate
  // divisors [100, 60] select base_divisor = 60 (x0 + x1 <= 5). Dividing by
  // div = 2 (divisor = 30, 2*x0 + 3*x1 <= 11) improves scaled violation, and
  // div = 3 must still test base_divisor / 3 = 20 (rather than 30 / 3 = 10) to
  // recover the stronger cut 3*x0 + 5*x1 <= 17.
  CutData data;
  data.FillFromParallelVectors(
      IntegerValue(359), {x0, x1}, IntegerValueVector({60, 100}), {3.65, 1.4},
      IntegerValueVector({0, 0}), IntegerValueVector({10, 10}));

  RoundingOptions options;
  options.max_scaling = IntegerValue(60);

  IntegerRoundingCutHelper helper(&model);
  ASSERT_TRUE(helper.ComputeCut(options, data, /*ib_processor=*/nullptr));
  EXPECT_EQ(GetCutString(helper), "3*I0 5*I1 <= 17");
}

TEST(IntegerRoundingCutTest, LotSizingKeepsLargeDomainInventorySlackUnscaled) {
  Model model;
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable inventory = model.Add(NewIntegerVariable(0, 1000));

  // Single-period lot-sizing 50*y + I >= 45 (i.e. -50*y - I <= -45) with
  // integer batches y and a large-domain inventory slack I, at the LP point
  // y* = 0.8, I* = 5. With divisor 50 we have rhs_remainder = 5 and
  // GetFactorT() = 5; multiplying by t = 5 maps -I to -5 and yields
  // 25*y + I >= 25 which is not violated, whereas the exact t = 1 MIR cut
  // 45*y + I >= 45 is violated by 4.
  CutData data;
  data.FillFromParallelVectors(
      IntegerValue(-45), {y, inventory}, IntegerValueVector({-50, -1}),
      {0.8, 5.0}, IntegerValueVector({0, 0}), IntegerValueVector({10, 1000}));
  data.ComplementForSmallerLpValues();

  RoundingOptions options;
  options.max_scaling = IntegerValue(60);

  IntegerRoundingCutHelper helper(&model);
  ASSERT_TRUE(helper.ComputeCut(options, data, /*ib_processor=*/nullptr));
  EXPECT_EQ(GetCutString(helper), "-45*I0 -1*I1 <= -45");
}

// Discretized continuous network lot-sizing row (mip_scale_for_precision scales
// the continuous flow variables by K = 2^23): the incoming flow X_in, the
// integer batch variables Y_i with capacities 7, 31 and 255 and the outgoing
// flow X_out must cover a demand of 100 units:
//   -7K*Y1 - 31K*Y2 - 255K*Y3 - X_in + X_out <= -100K.
constexpr int64_t kFlowScale = int64_t{1} << 23;
constexpr int64_t kFlowUpperBound = 294000 * kFlowScale;

std::vector<IntegerVariable> NewScaledLotSizingVariables(Model* model) {
  std::vector<IntegerVariable> vars;
  for (int i = 0; i < 3; ++i) {
    vars.push_back(model->Add(NewIntegerVariable(0, 1000)));
  }
  for (int i = 0; i < 2; ++i) {
    vars.push_back(model->Add(NewIntegerVariable(0, kFlowUpperBound)));
  }
  return vars;
}

CutData ScaledLotSizingRow(absl::Span<const IntegerVariable> vars,
                           absl::Span<const double> lp_values) {
  CutData data;
  data.FillFromParallelVectors(
      IntegerValue(-100 * kFlowScale), vars,
      {IntegerValue(-7 * kFlowScale), IntegerValue(-31 * kFlowScale),
       IntegerValue(-255 * kFlowScale), IntegerValue(-1), IntegerValue(1)},
      lp_values, IntegerValueVector({0, 0, 0, 0, 0}),
      {IntegerValue(1000), IntegerValue(1000), IntegerValue(1000),
       IntegerValue(kFlowUpperBound), IntegerValue(kFlowUpperBound)});
  return data;
}

TEST(IntegerRoundingCutTest, ScaledContinuousLotSizingRowIsRoundedUnscaled) {
  Model model;
  const std::vector<IntegerVariable> vars = NewScaledLotSizingVariables(&model);

  // At Y1* = 100 / 7 (all flows at zero), rounding with divisor 7K gives
  // rhs_remainder = 5K and size = 2K >> max_scaling. Bucketing into 60 buckets
  // would map -X_in to -1 while mapping -7K*Y1 to -60, i.e. a cut where
  // 60 * 2^-23 units of incoming flow replace a whole batch. The exact t = 1
  // MIR cut keeps the flow at its natural scale:
  //   2K*Y1 + 10K*Y2 + 74K*Y3 + X_in >= 30K.
  CutData data = ScaledLotSizingRow(vars, {100.0 / 7.0, 0.0, 0.0, 0.0, 0.0});
  data.ComplementForSmallerLpValues();

  RoundingOptions options;
  options.max_scaling = IntegerValue(60);

  IntegerRoundingCutHelper helper(&model);
  ASSERT_TRUE(helper.ComputeCut(options, data, /*ib_processor=*/nullptr));
  EXPECT_EQ(GetCutString(helper),
            "-16777216*I0 -83886080*I1 -620756992*I2 -1*I3 <= -251658240");
}

TEST(CoverCutHelperTest,
     ScaledContinuousLotSizingFlowCoverIsStrengthenedUnscaled) {
  Model model;
  const std::vector<IntegerVariable> vars = NewScaledLotSizingVariables(&model);

  // At Y3* = 0.3 and X_in* = 23.5K, the flow cover {Y1, Y2, Y3, X_in} has
  // positive_rhs = 100K >> 6000. Capping the strengthening function at 6000
  // buckets would map -X_in to -1 against a rhs of -6000 (so 6000 * 2^-23
  // units of incoming flow would replace the whole demand) and the cut would
  // not even be violated. The exact strengthening gives the flow cover
  //   7K*Y1 + 31K*Y2 + 100K*Y3 + X_in >= 100K.
  CutData data = ScaledLotSizingRow(
      vars, {0.0, 0.0, 0.3, 23.5 * static_cast<double>(kFlowScale), 0.0});
  data.ComplementForPositiveCoefficients();

  CoverCutHelper helper(&model);
  ASSERT_TRUE(helper.TrySingleNodeFlow(data, /*ib_processor=*/nullptr));
  EXPECT_EQ(GetCutString(helper),
            "-58720256*I0 -260046848*I1 -838860800*I2 -1*I3 <= -838860800");
}

TEST(CoverCutHelperTest, SingleNodeFlowDoesNotCoverHugeDomainSlackNearZero) {
  Model model;
  const IntegerVariable x0 = model.Add(NewIntegerVariable(0, 1));
  const IntegerVariable x1 = model.Add(NewIntegerVariable(0, 1));
  const IntegerVariable s = model.Add(NewIntegerVariable(0, 1'000'000'000));

  // On 10*x0 + 10*x1 + s <= 16 at x* = (0.75, 0.75), s* = 1, the unlinked
  // huge-domain slack s has a tiny (lp_dist / bound_diff^2) score and would
  // be selected alone as the "cover" (which is not a cover since s* rounds up
  // to 1), leaving no cut. Scoring it by lp_dist / (coeff * bound_diff) puts
  // it last and yields the cover {x0, x1}.
  CutData data;
  data.FillFromParallelVectors(IntegerValue(16), {x0, x1, s},
                               IntegerValueVector({10, 10, 1}),
                               {0.75, 0.75, 1.0}, IntegerValueVector({0, 0, 0}),
                               IntegerValueVector({1, 1, 1'000'000'000}));

  CoverCutHelper helper(&model);
  ASSERT_TRUE(helper.TrySingleNodeFlow(data, /*ib_processor=*/nullptr));
  EXPECT_EQ(GetCutString(helper), "1*I0 1*I1 <= 1");
}

TEST(IntegerRoundingCutTest, ManyContinuousTermsDoNotCrowdOutIntegerDivisor) {
  Model model;
  std::vector<IntegerVariable> vars;
  std::vector<IntegerValue> coeffs;
  std::vector<double> lp_values;
  std::vector<IntegerValue> lbs;
  std::vector<IntegerValue> ubs;
  constexpr int kNumContinuous = 55;
  for (int j = 0; j < kNumContinuous; ++j) {
    vars.push_back(model.Add(NewIntegerVariable(0, 1000)));
    coeffs.push_back(IntegerValue(8 * (2 + j)));
    lp_values.push_back(5.0);
    lbs.push_back(IntegerValue(0));
    ubs.push_back(IntegerValue(1000));
  }
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 10));
  vars.push_back(y);
  coeffs.push_back(IntegerValue(8));
  lp_values.push_back(0.5);
  lbs.push_back(IntegerValue(0));
  ubs.push_back(IntegerValue(10));

  // 55 large-domain terms with distinct coefficients 8 * (2 + j) and LP value
  // 5.0 precede the fractional integer y (coefficient 8, y* = 0.5) once sorted
  // by LP value. Keeping only the first 50 magnitudes would never try divisor
  // 8, which is the only one giving a violated cut here:
  //   sum_j (2 + j) * c_j + y <= 7975.
  const int64_t rhs = 5 * 8 * (2 * kNumContinuous + 1485) + 4;
  CutData data;
  data.FillFromParallelVectors(IntegerValue(rhs), vars, coeffs, lp_values, lbs,
                               ubs);
  data.ComplementForSmallerLpValues();

  RoundingOptions options;
  options.max_scaling = IntegerValue(60);

  IntegerRoundingCutHelper helper(&model);
  ASSERT_TRUE(helper.ComputeCut(options, data, /*ib_processor=*/nullptr));
  const std::string cut = GetCutString(helper);
  EXPECT_THAT(cut, StartsWith("2*I0 3*I1 4*I2 "));
  EXPECT_THAT(cut, EndsWith(" 55*I53 56*I54 1*I55 <= 7975"));
}

TEST(IntegerRoundingCutTest, TrialComplementationIsRestoredExactly) {
  Model model;
  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, int64_t{1} << 50));

  // On 6x + 4y <= 9 at (x*, y*) = (1, 0.02), the rounded cut 2x + y <= 2 is
  // only violated thanks to y (by 0.02 >= kMinCutViolation). The heuristic
  // tries to complement y (which has a relevant LP value): 2^50 - 0.02 rounds
  // to 2^50 in double precision, so complementing back would leave y with an
  // LP value of exactly 0 and the final violation check would reject the cut.
  // We must restore the saved term instead.
  CutData data;
  data.FillFromParallelVectors(
      IntegerValue(9), {x, y}, IntegerValueVector({6, 4}), {1.0, 0.02},
      IntegerValueVector({0, 0}),
      {IntegerValue(10), IntegerValue(int64_t{1} << 50)});

  RoundingOptions options;
  options.max_scaling = IntegerValue(60);

  IntegerRoundingCutHelper helper(&model);
  ASSERT_TRUE(helper.ComputeCut(options, data, /*ib_processor=*/nullptr));
  EXPECT_EQ(GetCutString(helper), "2*I0 1*I1 <= 2");
  ASSERT_EQ(helper.cut().terms.size(), 2);
  EXPECT_EQ(helper.cut().terms[1].lp_value, 0.02);
}

TEST(IntegerRoundingCutTest, LpSlackInfinityNormBoundsTheUnscaledScaling) {
  Model model;
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 1000));
  const IntegerVariable slack =
      model.Add(NewIntegerVariable(0, int64_t{1} << 50));

  // On 2^40 * y - S <= 2^39 at y* = 0.5 with the large-domain slack S at zero,
  // bucket scaling would dilute -S, so we round with t = 1 and as many buckets
  // as is safe. Without any information on S, the exact unscaled MIR cut
  // 2^39 * y - S <= 0 is used. When S is the slack of an LP row with infinity
  // norm 2^56 (so that substituting it back multiplies its coefficient by up to
  // 2^56), the scaling must be capped so that the final coefficients stay
  // below 2^52 (about 2^36 buckets). The bucketed rounding function further
  // caps the scaling at kint64max / divisor = 2^23 - 1 to stay in int64_t, so
  // we get 2^23 - 1 buckets instead of either 2^39 or just max_scaling = 60.
  auto compute_cut = [&](IntegerValue slack_infinity_norm) {
    CutData data;
    data.FillFromParallelVectors(
        IntegerValue(int64_t{1} << 39), {y, slack},
        {IntegerValue(int64_t{1} << 40), IntegerValue(-1)}, {0.5, 0.0},
        IntegerValueVector({0, 0}),
        {IntegerValue(1000), IntegerValue(int64_t{1} << 50)});
    if (slack_infinity_norm > 0) {
      data.first_slack = slack;
      data.slack_infinity_norms.push_back(slack_infinity_norm);
    }
    data.ComplementForSmallerLpValues();

    RoundingOptions options;
    options.max_scaling = IntegerValue(60);
    IntegerRoundingCutHelper helper(&model);
    EXPECT_TRUE(helper.ComputeCut(options, data, /*ib_processor=*/nullptr));
    return GetCutString(helper);
  };

  EXPECT_EQ(compute_cut(IntegerValue(0)), "549755813888*I0 -1*I1 <= 0");
  EXPECT_EQ(compute_cut(IntegerValue(1)), "549755813888*I0 -1*I1 <= 0");
  EXPECT_EQ(compute_cut(IntegerValue(int64_t{1} << 56)),
            "8388607*I0 -1*I1 <= 0");
}

TEST(CoverCutHelperTest, SingleNodeFlowRejectsCoverNotStrengthenedByF) {
  Model model;
  std::vector<IntegerVariable> vars;
  for (int i = 0; i < 3; ++i) {
    vars.push_back(model.Add(NewIntegerVariable(0, 10)));
  }

  // On x0 + x1 + x2 <= 25 with x_i in [0, 10], the cover {x0, x1, x2} has
  // positive_rhs = 30 - 25 = 5 and the strengthening function is the identity
  // on all the (complemented) cover coefficients -1: the "cut" is just the
  // base constraint. At x* = 8.3334 (slightly violating the base constraint,
  // like LP values do by floating point drift), it would pass the violation
  // check and later be rejected as empty. We must detect this early.
  CutData data;
  data.FillFromParallelVectors(
      IntegerValue(25), vars, IntegerValueVector({1, 1, 1}),
      {8.3334, 8.3334, 8.3334}, IntegerValueVector({0, 0, 0}),
      IntegerValueVector({10, 10, 10}));
  CoverCutHelper helper(&model);
  EXPECT_FALSE(helper.TrySingleNodeFlow(data, /*ib_processor=*/nullptr));

  // On 10*x3 + 10*x4 + x0 <= 16 at x3* = x4* = 0.75 and x0* = 1, the cover
  // {x3, x4} has positive_rhs = 4 and f() truncates -10 to -4, so we do get
  // the cut x3 + x4 <= 1.
  const IntegerVariable x3 = model.Add(NewIntegerVariable(0, 1));
  const IntegerVariable x4 = model.Add(NewIntegerVariable(0, 1));
  data.FillFromParallelVectors(IntegerValue(16), {x3, x4, vars[0]},
                               IntegerValueVector({10, 10, 1}),
                               {0.75, 0.75, 1.0}, IntegerValueVector({0, 0, 0}),
                               IntegerValueVector({1, 1, 10}));
  ASSERT_TRUE(helper.TrySingleNodeFlow(data, /*ib_processor=*/nullptr));
  EXPECT_EQ(GetCutString(helper), "1*I3 1*I4 <= 1");
}

// Returns a string describing the first integer point of the box [lbs, ubs]
// that satisfies all the `bases` constraints but violates `cut`, or an empty
// string if `cut` is valid for all of them. Variables are IntegerVariable(2 *
// i) and their negations.
std::string FirstPointCutOff(absl::Span<const LinearConstraint* const> bases,
                             const LinearConstraint& cut,
                             absl::Span<const IntegerValue> lbs,
                             absl::Span<const IntegerValue> ubs) {
  const int n = lbs.size();
  std::vector<int64_t> point(n);
  for (int i = 0; i < n; ++i) point[i] = lbs[i].value();
  const auto activity = [&point](const LinearConstraint& ct) {
    absl::int128 result = 0;
    for (int i = 0; i < ct.num_terms; ++i) {
      const int index = PositiveVariable(ct.vars[i]).value() / 2;
      const int64_t value =
          VariableIsPositive(ct.vars[i]) ? point[index] : -point[index];
      result += absl::int128(ct.coeffs[i].value()) * absl::int128(value);
    }
    return result;
  };
  while (true) {
    bool feasible = true;
    for (const LinearConstraint* base : bases) {
      const absl::int128 base_activity = activity(*base);
      if (base_activity > absl::int128(base->ub.value()) ||
          base_activity < absl::int128(base->lb.value())) {
        feasible = false;
        break;
      }
    }
    if (feasible) {
      const absl::int128 cut_activity = activity(cut);
      if (cut_activity > absl::int128(cut.ub.value()) ||
          cut_activity < absl::int128(cut.lb.value())) {
        return absl::StrCat("point=[", absl::StrJoin(point, ","),
                            "] cut_activity=", cut_activity);
      }
    }
    int i = 0;
    while (i < n && point[i] == ubs[i].value()) {
      point[i] = lbs[i].value();
      ++i;
    }
    if (i == n) return "";
    ++point[i];
  }
}

// Brute-force validity check of the cover, flow and integer rounding cut
// heuristics on random small-domain constraints with coefficients of various
// magnitudes (including the large ones produced by mip_scale_for_precision).
TEST(CutValidityFuzzTest, RandomConstraintsWithSmallDomains) {
  std::mt19937 random(12345);
  const auto uniform = [&random](int64_t lo, int64_t hi) {
    return std::uniform_int_distribution<int64_t>(lo, hi)(random);
  };
  // Includes the very large magnitudes produced by mip_scale_for_precision, for
  // which kint64max / positive_rhs is small in the flow cover strengthening.
  const int64_t kScales[] = {1,
                             1,
                             10,
                             1000,
                             1000000,
                             1000000000,
                             1000000000000,
                             100000000000000,
                             1000000000000000};
  Model model;
  CoverCutHelper cover_helper(&model);
  IntegerRoundingCutHelper rounding_helper(&model);
  CutDataBuilder builder;
  int num_cuts = 0;
  for (int iter = 0; iter < 20000; ++iter) {
    const int n = uniform(2, 5);
    std::vector<IntegerVariable> vars;
    std::vector<IntegerValue> lbs;
    std::vector<IntegerValue> ubs;
    std::vector<IntegerValue> coeffs;
    std::vector<double> lp_values;
    LinearConstraint base;
    base.lb = kMinIntegerValue;
    double activity = 0.0;
    for (int i = 0; i < n; ++i) {
      vars.push_back(IntegerVariable(2 * i));
      const int64_t lb = uniform(-2, 2);
      const int64_t ub = lb + uniform(1, 3);
      lbs.push_back(IntegerValue(lb));
      ubs.push_back(IntegerValue(ub));
      const int64_t scale = kScales[uniform(0, 8)];
      int64_t coeff = uniform(1, 50) * scale + uniform(0, 6);
      if (uniform(0, 3) == 0) coeff = -coeff;
      coeffs.push_back(IntegerValue(coeff));
      const double lp =
          static_cast<double>(lb) + static_cast<double>(uniform(0, 1000)) /
                                        1000.0 * static_cast<double>(ub - lb);
      lp_values.push_back(lp);
      activity += static_cast<double>(coeff) * lp;
    }
    // Make the LP point (nearly) tight.
    const IntegerValue rhs(static_cast<int64_t>(std::floor(activity)) +
                           uniform(-2, 2));
    base.ub = rhs;
    base.resize(n);
    for (int i = 0; i < n; ++i) {
      base.vars[i] = vars[i];
      base.coeffs[i] = coeffs[i];
    }

    CutData data;
    if (!data.FillFromParallelVectors(rhs, vars, coeffs, lp_values, lbs, ubs)) {
      continue;
    }
    data.ComplementForPositiveCoefficients();
    // The cover heuristics require a non-negative rhs once all coefficients
    // are positive (an infeasible constraint otherwise).
    if (data.rhs < 0) continue;

    const auto check = [&](absl::string_view name, const CutData& cut) {
      ++num_cuts;
      LinearConstraint converted;
      ASSERT_TRUE(builder.ConvertToLinearConstraint(cut, &converted));
      const std::string cut_off =
          FirstPointCutOff({&base}, converted, lbs, ubs);
      const auto fmt = [](std::string* out, IntegerValue v) {
        absl::StrAppend(out, v.value());
      };
      ASSERT_EQ(cut_off, "")
          << name << " cut " << converted.DebugString() << " from "
          << base.DebugString() << " lbs=[" << absl::StrJoin(lbs, ",", fmt)
          << "] ubs=[" << absl::StrJoin(ubs, ",", fmt) << "] lp=["
          << absl::StrJoin(lp_values, ",") << "] " << cut_off;
    };

    cover_helper.ClearCache();
    if (cover_helper.TrySingleNodeFlow(data)) {
      check("flow", cover_helper.cut());
    }
    if (cover_helper.TrySimpleKnapsack(data)) {
      check("knapsack", cover_helper.cut());
    }
    if (cover_helper.TryWithLetchfordSouliLifting(data)) {
      check("letchford_souli", cover_helper.cut());
    }

    data.ComplementForSmallerLpValues();
    for (const int64_t max_scaling : {2, 60, 600}) {
      RoundingOptions options;
      options.max_scaling = IntegerValue(max_scaling);
      if (rounding_helper.ComputeCut(options, data)) {
        check(absl::StrCat("rounding/", max_scaling), rounding_helper.cut());
      }
    }
  }
  EXPECT_GT(num_cuts, 1000);
}

// Regression test for a wrong flow cover cut found on timtab1CUTS: when the
// largest safe unscaled scaling is above kint64max / positive_rhs,
// GetSuperAdditiveStrengtheningMirFunction() silently caps its scaling but the
// period of ExtendNegativeFunction() was computed from the uncapped value, so
// the extension was not super-additive and the lifted positive coefficients
// cut off integer solutions.
TEST(CoverCutHelperTest, SingleNodeFlowPeriodUsesTheCappedScaling) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_use_implied_bounds(true);

  // (3e15 + 1e5) x0 + 1e11 x2 <= 3e15 + 2e5 with x0 in [0, 2], x2 in
  // [0, 30000] and the implied bound b => x0 <= 1.
  const IntegerVariable x0 = model.Add(NewIntegerVariable(0, 2));
  const IntegerVariable x2 = model.Add(NewIntegerVariable(0, 30000));
  const BooleanVariable b = model.Add(NewBooleanVariable());
  const IntegerVariable z = model.Add(NewIntegerVariable(0, 1));
  auto* integer_encoder = model.GetOrCreate<IntegerEncoder>();
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* implied_bounds = model.GetOrCreate<ImpliedBounds>();
  integer_encoder->AssociateToIntegerEqualValue(Literal(b, true), z,
                                                IntegerValue(1));
  implied_bounds->Add(Literal(b, true),
                      IntegerLiteral::LowerOrEqual(x0, IntegerValue(1)));
  ImpliedBoundsProcessor processor({x0, x2, z}, integer_trail, implied_bounds);

  util_intops::StrongVector<IntegerVariable, double> lp_values(100);
  lp_values[x0] = 1.5;
  lp_values[NegationOf(x0)] = -1.5;
  lp_values[x2] = 15000.0;
  lp_values[NegationOf(x2)] = -15000.0;
  lp_values[z] = 0.4;
  lp_values[NegationOf(z)] = -0.4;
  processor.RecomputeCacheAndSeparateSomeImpliedBoundCuts(lp_values);

  // The cover {x0} has positive_rhs = 3e15 and the complemented x0 is
  // decomposed with the implied bound into a non-simple slack, so the unscaled
  // strengthening is requested. The scaled strengthening function can have at
  // most kint64max / 3e15 = 3074 buckets while ComputeMaxSafeUnscaledScaling()
  // allows ~1.5e15: with the latter, the period would be the base period
  // 3e15 + 1e5, far below the 3e15 + 9.8e11 needed by 3074 buckets, and x2
  // would be lifted with coefficient 1 instead of 0, giving the invalid cut
  // 3074 x0 + x2 <= 3074 (x = (0, 30000) is feasible). The valid cut is
  // x0 <= 1.
  const IntegerValue rhs(3'000'000'000'200'000);
  std::vector<IntegerVariable> vars = {x0, x2};
  std::vector<IntegerValue> coeffs = {IntegerValue(3'000'000'000'100'000),
                                      IntegerValue(100'000'000'000)};
  CutData data;
  ASSERT_TRUE(data.FillFromParallelVectors(rhs, vars, coeffs, {1.5, 15000.0},
                                           IntegerValueVector({0, 0}),
                                           IntegerValueVector({2, 30000})));
  ASSERT_TRUE(processor.CacheDataForCut(IntegerVariable(100), &data));
  CoverCutHelper helper(&model);
  ASSERT_TRUE(helper.TrySingleNodeFlow(data, &processor));
  EXPECT_EQ(GetCutString(helper), "1*I0 <= 1");

  CutDataBuilder builder;
  LinearConstraint cut;
  ASSERT_TRUE(builder.ConvertToLinearConstraint(helper.cut(), &cut));
  LinearConstraint base(kMinIntegerValue, rhs);
  base.resize(2);
  base.vars[0] = x0;
  base.coeffs[0] = coeffs[0];
  base.vars[1] = x2;
  base.coeffs[1] = coeffs[1];
  LinearConstraint implied(kMinIntegerValue, IntegerValue(2));  // x0 + z <= 2.
  implied.resize(2);
  implied.vars[0] = x0;
  implied.coeffs[0] = IntegerValue(1);
  implied.vars[1] = z;
  implied.coeffs[1] = IntegerValue(1);
  EXPECT_EQ(
      FirstPointCutOff({&base, &implied}, cut, IntegerValueVector({0, 0, 0}),
                       IntegerValueVector({2, 30000, 1})),
      "")
      << cut.DebugString();
}

TEST(CutDataTest, SimpleExample) {
  Model model;
  const IntegerVariable x0 = model.Add(NewIntegerVariable(7, 10));
  const IntegerVariable x1 = model.Add(NewIntegerVariable(-3, 20));

  // 6x0 - 4x1 <= 9.
  const IntegerValue rhs = IntegerValue(9);
  std::vector<IntegerVariable> vars = {x0, x1};
  std::vector<IntegerValue> coeffs = {IntegerValue(6), IntegerValue(-4)};
  std::vector<double> lp_values = {7.5, 4.5};

  CutData cut;
  std::vector<IntegerValue> lbs;
  std::vector<IntegerValue> ubs;
  auto* integer_trail = model.Get<IntegerTrail>();
  for (int i = 0; i < vars.size(); ++i) {
    lbs.push_back(integer_trail->LowerBound(vars[i]));
    ubs.push_back(integer_trail->UpperBound(vars[i]));
  }
  cut.FillFromParallelVectors(rhs, vars, coeffs, lp_values, lbs, ubs);
  cut.ComplementForSmallerLpValues();

  // 6 (I0' + 7) - 4 (I1' - 3) <= 9
  ASSERT_EQ(cut.terms.size(), 2);
  EXPECT_EQ(cut.rhs, 9 - 4 * 3 - 6 * 7);
  EXPECT_EQ(cut.terms[0].coeff, 6);
  EXPECT_EQ(cut.terms[0].lp_value, 0.5);
  EXPECT_EQ(cut.terms[0].bound_diff, 3);
  EXPECT_EQ(cut.terms[1].coeff, -4);
  EXPECT_EQ(cut.terms[1].lp_value, 7.5);
  EXPECT_EQ(cut.terms[1].bound_diff, 23);

  // Lets complement.
  const absl::int128 old_rhs = cut.rhs;
  cut.terms[0].Complement(&cut.rhs);
  EXPECT_EQ(cut.rhs, old_rhs - 3 * 6);
  EXPECT_EQ(cut.terms[0].coeff, -6);
  EXPECT_EQ(cut.terms[0].lp_value, 3 - 0.5);
  EXPECT_EQ(cut.terms[0].bound_diff, 3);

  // Encode back.
  LinearConstraint new_constraint;
  CutDataBuilder builder;
  EXPECT_TRUE(builder.ConvertToLinearConstraint(cut, &new_constraint));

  // We have a division by GCD in there.
  const IntegerValue gcd = 2;
  EXPECT_EQ(vars.size(), new_constraint.num_terms);
  for (int i = 0; i < new_constraint.num_terms; ++i) {
    EXPECT_EQ(vars[i], new_constraint.vars[i]);
    EXPECT_EQ(coeffs[i] / gcd, new_constraint.coeffs[i]);
  }
}

TEST(SumOfAllDiffLowerBounderTest, ContinuousVariables) {
  Model model;
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  IntegerVariable x1 = model.Add(NewIntegerVariable(1, 10));
  IntegerVariable x2 = model.Add(NewIntegerVariable(1, 10));
  IntegerVariable x3 = model.Add(NewIntegerVariable(1, 10));

  SumOfAllDiffLowerBounder helper;
  helper.Add(x1, 3, *integer_trail);
  helper.Add(x2, 3, *integer_trail);
  helper.Add(x3, 3, *integer_trail);
  EXPECT_EQ(3, helper.size());
  EXPECT_EQ(6, helper.SumOfMinDomainValues());
  EXPECT_EQ(6, helper.SumOfDifferentMins());
  std::string suffix;
  EXPECT_EQ(6, helper.GetBestLowerBound(suffix));
  EXPECT_EQ("e", suffix);
  helper.Clear();
  EXPECT_EQ(0, helper.size());
}

TEST(SumOfAllDiffLowerBounderTest, DisjointVariables) {
  Model model;
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  IntegerVariable x1 = model.Add(NewIntegerVariable(1, 10));
  IntegerVariable x2 = model.Add(NewIntegerVariable(1, 10));
  IntegerVariable x3 = model.Add(NewIntegerVariable(1, 10));

  SumOfAllDiffLowerBounder helper;
  helper.Add(x1, 3, *integer_trail);
  helper.Add(x2, 3, *integer_trail);
  helper.Add(AffineExpression(x3, 1, 10), 3, *integer_trail);
  EXPECT_EQ(3, helper.size());
  EXPECT_EQ(6, helper.SumOfMinDomainValues());
  EXPECT_EQ(14, helper.SumOfDifferentMins());
  std::string suffix;
  EXPECT_EQ(14, helper.GetBestLowerBound(suffix));
  EXPECT_EQ("a", suffix);
}

TEST(SumOfAllDiffLowerBounderTest, DiscreteDomains) {
  Model model;
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  IntegerVariable x1 = model.Add(NewIntegerVariable(1, 10));
  IntegerVariable x2 = model.Add(NewIntegerVariable(1, 10));
  IntegerVariable x3 = model.Add(NewIntegerVariable(1, 10));

  SumOfAllDiffLowerBounder helper;
  helper.Add(AffineExpression(x1, 3, 0), 3, *integer_trail);
  helper.Add(AffineExpression(x2, 3, 0), 3, *integer_trail);
  helper.Add(AffineExpression(x3, 3, 0), 3, *integer_trail);
  EXPECT_EQ(3, helper.size());
  EXPECT_EQ(18, helper.SumOfMinDomainValues());
  EXPECT_EQ(12, helper.SumOfDifferentMins());
  std::string suffix;
  EXPECT_EQ(18, helper.GetBestLowerBound(suffix));
  EXPECT_EQ("d", suffix);
}

TEST(ComputeHyperPlaneBelowSquare, Exhaustive) {
  const IntegerVariable x(0);
  const IntegerVariable s(2);
  for (int lb = -10; lb <= 10; ++lb) {
    for (int ub = lb; ub <= 10; ++ub) {
      for (IntegerValue location(lb); location < ub; ++location) {
        const LinearConstraint cut = ComputeHyperplanBelowSquare(
            AffineExpression(x), AffineExpression(s), location);
        for (IntegerValue x_value = lb; x_value <= ub; ++x_value) {
          const IntegerValue s_value = x_value * x_value;
          IntegerValue line_value(0);
          for (int i = 0; i < cut.num_terms; ++i) {
            CHECK(cut.vars[i] == x || cut.vars[i] == s);
            const IntegerValue point = cut.vars[i] == x ? x_value : s_value;
            line_value += point * cut.coeffs[i];
          }
          EXPECT_GE(line_value, cut.lb);
          EXPECT_LE(line_value, cut.ub);

          // The line should be tight at these points.
          if (x_value == location || x_value == location + 1) {
            EXPECT_TRUE(line_value == cut.lb || line_value == cut.ub);
          }
        }
      }
    }
  }
}

TEST(ComputeHyperPlaneAboveSquare, Exhaustive) {
  const IntegerVariable x(0);
  const IntegerVariable s(2);
  for (int lb = -10; lb <= 10; ++lb) {
    for (int ub = lb; ub <= 10; ++ub) {
      const LinearConstraint cut = ComputeHyperplanAboveSquare(
          AffineExpression(x), AffineExpression(s), lb, ub);
      for (IntegerValue x_value = lb; x_value <= ub; ++x_value) {
        const IntegerValue s_value = x_value * x_value;
        IntegerValue line_value(0);
        for (int i = 0; i < cut.num_terms; ++i) {
          CHECK(cut.vars[i] == x || cut.vars[i] == s);
          const IntegerValue point = cut.vars[i] == x ? x_value : s_value;
          line_value += point * cut.coeffs[i];
        }
        EXPECT_GE(line_value, cut.lb);
        EXPECT_LE(line_value, cut.ub);

        // The line should be tight at these points.
        if (x_value == lb || x_value == ub) {
          EXPECT_TRUE(line_value == cut.lb || line_value == cut.ub);
        }
      }
    }
  }
}

TEST(CutsOverflowTest, SquareCutGeneratorSkipsLargeConstants) {
  Model model;
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 10000));
  const IntegerVariable v =
      model.Add(NewIntegerVariable(1LL << 60, (1LL << 60) + 10));
  InitializeLpValues({0.0, static_cast<double>(1LL << 60)}, &model);

  // Shifted expression with small [0, 10] bounds but huge constant offset must
  // be skipped without overflowing in LinearConstraintBuilder.
  auto* manager = model.GetOrCreate<LinearConstraintManager>();
  const AffineExpression shifted_x(v, IntegerValue(1),
                                   IntegerValue(-(1LL << 60)));
  CutGenerator square_shifted =
      CreateSquareCutGenerator(y, shifted_x, 2, &model);
  EXPECT_TRUE(square_shifted.generate_cuts(manager));
  EXPECT_EQ(manager->num_cuts(), 0);
}

TEST(CutsOverflowTest, AllDiffCutGeneratorStickyOverflowDoesNotAddBogusBound) {
  Model model;
  const int64_t big = 1LL << 61;
  const IntegerVariable x1 = model.Add(NewIntegerVariable(-big, big));
  const IntegerVariable x2 = model.Add(NewIntegerVariable(-big, big));
  const IntegerVariable x3 = model.Add(NewIntegerVariable(-big, big));
  const IntegerVariable x4 = model.Add(NewIntegerVariable(-big, big));
  const IntegerVariable x5 = model.Add(NewIntegerVariable(-big, 100));
  InitializeLpValues({1e18, 1e18, 1e18, 1e18, 50.0}, &model);

  CutGenerator all_diff =
      CreateAllDifferentCutGenerator({x1, x2, x3, x4, x5}, &model);
  auto* manager = model.GetOrCreate<LinearConstraintManager>();
  EXPECT_TRUE(all_diff.generate_cuts(manager));
  EXPECT_EQ(0, manager->num_cuts());
}

TEST(CutsOverflowTest, LinMaxCutGeneratorSkipsOverflowingCornerCoefficients) {
  Model model;
  const int64_t big = 1LL << 61;
  const IntegerVariable x1 = model.Add(NewIntegerVariable(0, 1));
  const IntegerVariable x2 = model.Add(NewIntegerVariable(0, 1));
  const IntegerVariable x3 = model.Add(NewIntegerVariable(0, 1));
  const IntegerVariable x4 = model.Add(NewIntegerVariable(0, 1));
  const IntegerVariable target = model.Add(NewIntegerVariable(-big, big));
  const IntegerVariable z0 = model.Add(NewIntegerVariable(0, 1));
  const IntegerVariable z1 = model.Add(NewIntegerVariable(0, 1));
  const IntegerVariable z2 = model.Add(NewIntegerVariable(0, 1));
  const IntegerVariable z3 = model.Add(NewIntegerVariable(0, 1));
  const IntegerVariable z4 = model.Add(NewIntegerVariable(0, 1));

  LinearExpression f0;
  LinearExpression f1;
  f1.vars = {x1};
  f1.coeffs = {IntegerValue(-big)};
  LinearExpression f2;
  f2.vars = {x2};
  f2.coeffs = {IntegerValue(-big)};
  LinearExpression f3;
  f3.vars = {x3};
  f3.coeffs = {IntegerValue(-big)};
  LinearExpression f4;
  f4.vars = {x4};
  f4.coeffs = {IntegerValue(-big)};

  CutGenerator max_cuts = CreateLinMaxCutGenerator(
      target, {f0, f1, f2, f3, f4}, {z0, z1, z2, z3, z4}, &model);
  auto* manager = model.GetOrCreate<LinearConstraintManager>();
  InitializeLpValues({1.0, 1.0, 1.0, 1.0, 1e18, 0.5, 0.0, 0.0, 0.0, 0.0},
                     &model);
  EXPECT_TRUE(max_cuts.generate_cuts(manager));
  EXPECT_EQ(0, manager->num_cuts());
}

TEST(CutsOverflowTest, MaxAffineUpConstraintRejectsOverflowingRhs) {
  Model model;
  const IntegerVariable x = model.Add(NewIntegerVariable(2, 6));
  const IntegerVariable target = model.Add(NewIntegerVariable(0, 15LL << 58));
  LinearExpression target_expr;
  target_expr.vars = {target};
  target_expr.coeffs = {IntegerValue(1)};

  // Valid affine expression (max magnitude 15 * 2^58 < kint64max / 2) where
  // delta_x * y_at_min (28 * 2^58) and -delta_y * x_min (8 * 2^58) each fit in
  // int64_t, but their sum rhs = 36 * 2^58 overflows int64_t.
  const std::vector<std::pair<IntegerValue, IntegerValue>> affines = {
      {IntegerValue(-(1LL << 58)), IntegerValue(9LL << 58)}};
  LinearConstraintBuilder builder(&model);
  EXPECT_FALSE(
      BuildMaxAffineUpConstraint(target_expr, x, affines, &model, &builder));
}

TEST(CutsOverflowTest,
     DecomposeWithImpliedUpperBoundRejectsNonTightAtInt64Max) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_use_implied_bounds(true);

  const BooleanVariable c = model.Add(NewBooleanVariable());
  const IntegerVariable c_view = model.Add(NewIntegerVariable(0, 1));
  constexpr int64_t kMax = std::numeric_limits<int64_t>::max();
  constexpr int64_t kImpliedDiff = kMax / 7;
  static_assert(kImpliedDiff * 7 == kMax);
  const IntegerVariable x = model.Add(NewIntegerVariable(-100, kImpliedDiff));

  auto* integer_encoder = model.GetOrCreate<IntegerEncoder>();
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* implied_bounds = model.GetOrCreate<ImpliedBounds>();

  integer_encoder->AssociateToIntegerEqualValue(Literal(c, true), c_view,
                                                IntegerValue(1));
  // c => x <= 0 gives implied bound diff = kImpliedDiff on (kImpliedDiff - x),
  // which is strictly smaller than X.bound_diff = kImpliedDiff + 100.
  implied_bounds->Add(Literal(c, true),
                      IntegerLiteral::LowerOrEqual(x, IntegerValue(0)));

  ImpliedBoundsProcessor processor({x, c_view}, integer_trail, implied_bounds);
  util_intops::StrongVector<IntegerVariable, double> lp_values(100);
  lp_values[x] = 0.0;
  lp_values[NegationOf(x)] = 0.0;
  lp_values[c_view] = 0.5;
  processor.RecomputeCacheAndSeparateSomeImpliedBoundCuts(lp_values);

  CutTerm term;
  term.coeff = 7;
  term.lp_value = 100.0;
  term.bound_diff = kImpliedDiff + 100;
  term.expr_vars[0] = x;
  term.expr_coeffs[0] = 1;
  term.expr_coeffs[1] = 0;
  term.expr_offset = 100;

  CutData data;
  data.terms.push_back(term);
  ASSERT_TRUE(processor.CacheDataForCut(IntegerVariable(100), &data));
  term = data.terms[0];

  CutTerm bool_term;
  CutTerm slack_term;
  // Must return false (rather than saturating CapProdI to kint64max and hitting
  // CHECK_EQ(unused, 0)) because the implied bound does not span bound_diff.
  EXPECT_FALSE(
      processor.DecomposeWithImpliedUpperBound(term, bool_term, slack_term));
}

TEST(ImpliedBoundsProcessorTest,
     SharesSingletonAcrossComponentsWithoutCrossContamination) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_use_implied_bounds(true);

  const BooleanVariable b1 = model.Add(NewBooleanVariable());
  const IntegerVariable b1_view = model.Add(NewIntegerVariable(0, 1));
  const BooleanVariable b2 = model.Add(NewBooleanVariable());
  const IntegerVariable b2_view = model.Add(NewIntegerVariable(0, 1));
  const BooleanVariable b3 = model.Add(NewBooleanVariable());
  const IntegerVariable b3_view = model.Add(NewIntegerVariable(0, 1));
  const IntegerVariable x = model.Add(NewIntegerVariable(0, 100));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 100));
  const IntegerVariable z = model.Add(NewIntegerVariable(0, 100));

  auto* integer_encoder = model.GetOrCreate<IntegerEncoder>();
  auto* implied_bounds = model.GetOrCreate<ImpliedBounds>();
  auto* processor = model.GetOrCreate<ImpliedBoundsProcessor>();
  EXPECT_EQ(model.GetOrCreate<ImpliedBoundsProcessor>(), processor);

  integer_encoder->AssociateToIntegerEqualValue(Literal(b1, true), b1_view,
                                                IntegerValue(1));
  integer_encoder->AssociateToIntegerEqualValue(Literal(b2, true), b2_view,
                                                IntegerValue(1));
  integer_encoder->AssociateToIntegerEqualValue(Literal(b3, true), b3_view,
                                                IntegerValue(1));
  implied_bounds->Add(Literal(b1, true),
                      IntegerLiteral::GreaterOrEqual(x, IntegerValue(50)));
  implied_bounds->Add(Literal(b2, true),
                      IntegerLiteral::GreaterOrEqual(y, IntegerValue(60)));
  // Violated on component 1 (25.0 < 80 * 0.5) and adds a cut to IbCutPool().
  implied_bounds->Add(Literal(b3, true),
                      IntegerLiteral::GreaterOrEqual(x, IntegerValue(80)));
  implied_bounds->Add(Literal(b1, true),
                      IntegerLiteral::GreaterOrEqual(z, IntegerValue(40)));
  implied_bounds->Add(Literal(b1, true),
                      IntegerLiteral::LowerOrEqual(z, IntegerValue(60)));

  util_intops::StrongVector<IntegerVariable, double> lp_values(30, 0.0);
  lp_values[b1_view] = 0.5;
  lp_values[b2_view] = 0.5;
  lp_values[b3_view] = 0.5;
  lp_values[x] = 25.0;
  lp_values[y] = 30.0;

  // Component 1 includes x, b1_view, and b3_view (dense scan branch).
  processor->RecomputeCacheAndSeparateSomeImpliedBoundCuts(
      {x, b1_view, b3_view}, lp_values);
  EXPECT_EQ(processor->GetCachedImpliedBoundInfo(x).bool_var, b1_view);
  EXPECT_EQ(processor->GetCachedImpliedBoundInfo(x).implied_bound,
            IntegerValue(50));
  EXPECT_EQ(processor->GetCachedImpliedBoundInfo(y).bool_var,
            kNoIntegerVariable);

  // Component 2 includes only {y} (sparse component scan branch, without
  // b2_view), and simulates Component 1 returning early before transferring
  // IbCutPool().
  processor->RecomputeCacheAndSeparateSomeImpliedBoundCuts({y}, lp_values);
  EXPECT_EQ(processor->GetCachedImpliedBoundInfo(x).bool_var,
            kNoIntegerVariable);
  EXPECT_EQ(processor->GetCachedImpliedBoundInfo(y).bool_var,
            kNoIntegerVariable);
  LinearConstraintManager manager(&model);
  processor->IbCutPool().TransferToManager(&manager);
  EXPECT_EQ(manager.num_cuts(), 0);

  // Component 2 with {y, b2_view} activates y's tight implied bound while
  // keeping x uncached.
  processor->RecomputeCacheAndSeparateSomeImpliedBoundCuts({y, b2_view},
                                                           lp_values);
  EXPECT_EQ(processor->GetCachedImpliedBoundInfo(x).bool_var,
            kNoIntegerVariable);
  const ImpliedBoundsProcessor::BestImpliedBoundInfo info =
      processor->GetCachedImpliedBoundInfo(y);
  EXPECT_EQ(info.bool_var, b2_view);
  EXPECT_EQ(info.implied_bound, IntegerValue(60));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
