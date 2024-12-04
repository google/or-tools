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

#include "ortools/sat/cuts.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/numeric/int128.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/fp_utils.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {
namespace {

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
              std::min(max_t,
                       GetFactorT(rhs_remainder, divisor, IntegerValue(100))),
              max_scaling);
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

            // TODO(user): Our discretized "mir" function is not always
            // maximal. Try to fix it?
            if (a <= rhs_remainder || max_scaling != 2) continue;
            if (rhs_remainder * max_t < divisor / 2) continue;

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

TEST(GetSuperAdditiveStrengtheningFunction, AllSmallValues) {
  for (const int64_t rhs : {13, 14}) {  // test odd/even
    for (int64_t min_magnitude = 1; min_magnitude <= rhs; ++min_magnitude) {
      const auto f = GetSuperAdditiveStrengtheningFunction(rhs, min_magnitude);

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
      const auto f = GetSuperAdditiveStrengtheningMirFunction(rhs, scaling);

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
  EXPECT_COMPARABLE(cut.ComputeViolation(), 0.2, 1e-10);
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

  // Note(user): the ub of the last variable is not used. But the first two
  // are even though only the second one is required for the validity of the
  // cut.
  std::vector<IntegerValue> ubs = IntegerValueVector({1, 1, 10});

  CutData data;
  data.FillFromParallelVectors(IntegerValue(9), vars, coeffs, lp_values, lbs,
                               ubs);
  data.ComplementForPositiveCoefficients();
  CoverCutHelper helper;
  EXPECT_TRUE(helper.TrySimpleKnapsack(data));
  EXPECT_EQ(GetCutString(helper), "1*X0 1*X1 1*X2 <= 1");
  EXPECT_EQ(helper.Info(), "lift=1");
}

// I tried to reproduce bug 169094958, but if the base constraint is tight,
// the bug was triggered only due to numerical imprecision. A simple way to
// trigger it is like with this test if the given LP value just violate the
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
  CoverCutHelper helper;
  EXPECT_TRUE(helper.TrySimpleKnapsack(data));
  EXPECT_EQ(GetCutString(helper), "1*X0 1*X1 <= 9");
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

  CoverCutHelper helper;
  EXPECT_TRUE(helper.TryWithLetchfordSouliLifting(data));
  EXPECT_EQ(GetCutString(helper),
            "1*X0 1*X1 1*X2 1*X3 3*X4 3*X5 2*X6 1*X7 1*X8 1*X9 <= 3");

  // For now, we only support Booleans in the cover.
  // Note that we don't care for variable not in the cover though.
  data.terms[3].bound_diff = IntegerValue(2);
  EXPECT_FALSE(helper.TryWithLetchfordSouliLifting(data));
}

LinearConstraint IntegerRoundingCutWithBoundsFromTrail(
    const RoundingOptions& options, IntegerValue rhs,
    absl::Span<const IntegerVariable> vars,
    absl::Span<const IntegerValue> coeffs, absl::Span<const double> lp_values,
    const Model& model) {
  std::vector<IntegerValue> lbs;
  std::vector<IntegerValue> ubs;
  auto* integer_trail = model.Get<IntegerTrail>();
  for (int i = 0; i < vars.size(); ++i) {
    lbs.push_back(integer_trail->LowerBound(vars[i]));
    ubs.push_back(integer_trail->UpperBound(vars[i]));
  }

  CutData data;
  data.FillFromParallelVectors(rhs, vars, coeffs, lp_values, lbs, ubs);
  data.ComplementForSmallerLpValues();

  IntegerRoundingCutHelper helper;
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
      options, rhs, vars, coeffs, lp_values, model);
  EXPECT_EQ(constraint.DebugString(), "2*X0 1*X1 <= 2");
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
      RoundingOptions(), rhs, vars, coeffs, lp_values, model);
  EXPECT_EQ(constraint.DebugString(), "1*X0 1*X1 <= 1");
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
      RoundingOptions(), rhs, vars, coeffs, lp_values, model);
  EXPECT_EQ(constraint.DebugString(), "3*X0 2*X1 <= 4");
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
      RoundingOptions(), rhs, vars, coeffs, lp_values, model);

  // We actually do not return like in the example "3*X0 -2*X1 <= 4"
  // But the simpler X0 - X1 <= 2 which has the same violation (0.25) but a
  // better norm.
  EXPECT_EQ(constraint.DebugString(), "1*X0 -1*X1 <= 2");
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
      RoundingOptions(), rhs, vars, coeffs, lp_values, model);

  EXPECT_EQ(constraint.DebugString(), "-2*X0 -1*X1 -2*X2 -2*X3 2*X4 <= -2");
}

// The algo should find a "divisor" 2 when it lead to a good cut.
//
// TODO(user): Double check that such divisor will always be found? Of course,
// if the initial constraint coefficient are too high, then it will not, but
// that is okay since such cut efficacity will be bad anyway.
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
      RoundingOptions(), rhs, vars, coeffs, lp_values, model);
  EXPECT_EQ(constraint.DebugString(), "3*X0 2*X1 4*X2 3*X3 <= 4");
}

TEST(IntegerRoundingCutTest, LargeCoeffWithSmallImprecision) {
  Model model;
  const IntegerVariable x0 = model.Add(NewIntegerVariable(0, 5));
  const IntegerVariable x1 = model.Add(NewIntegerVariable(0, 5));

  // 1e6 x0 - x1 <= 1.5e6.
  const IntegerValue rhs = IntegerValue(1.5e6);
  std::vector<IntegerVariable> vars = {x0, x1};
  std::vector<IntegerValue> coeffs = {IntegerValue(1e6), IntegerValue(-1)};

  // Note thate without adjustement, this returns 2 * X0 - X1 <= 2.
  // TODO(user): expose parameters so this can be verified other than manually?
  std::vector<double> lp_values{1.5, 0.1};
  LinearConstraint constraint = IntegerRoundingCutWithBoundsFromTrail(
      RoundingOptions(), rhs, vars, coeffs, lp_values, model);
  EXPECT_EQ(constraint.DebugString(), "1*X0 <= 1");
}

TEST(IntegerRoundingCutTest, LargeCoeffWithSmallImprecision2) {
  Model model;
  const IntegerVariable x0 = model.Add(NewIntegerVariable(0, 5));
  const IntegerVariable x1 = model.Add(NewIntegerVariable(0, 5));

  // 1e6 x0 + 999999 * x1 <= 1.5e6.
  const IntegerValue rhs = IntegerValue(1.5e6);
  std::vector<IntegerVariable> vars = {x0, x1};
  std::vector<IntegerValue> coeffs = {IntegerValue(1e6), IntegerValue(999999)};

  // Note thate without adjustement, this returns 2 * X0 + X1 <= 2.
  // TODO(user): expose parameters so this can be verified other than manually?
  std::vector<double> lp_values{1.49, 0.1};
  LinearConstraint constraint = IntegerRoundingCutWithBoundsFromTrail(
      RoundingOptions(), rhs, vars, coeffs, lp_values, model);
  EXPECT_EQ(constraint.DebugString(), "1*X0 1*X1 <= 1");
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
      options, rhs, vars, coeffs, lp_values, model);
  EXPECT_EQ(constraint.DebugString(), "1*X6 2*X7 3*X8 4*X9 <= 4");
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
      options, rhs, vars, coeffs, lp_values, model);
  EXPECT_EQ(constraint.DebugString(),
            "2*X1 3*X2 4*X3 6*X4 6*X5 8*X6 9*X7 10*X8 12*X9 <= 18");
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
  IntegerRoundingCutHelper helper;

  // TODO(user): Actually this fail, so we don't compute a cut here.
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
              EndsWith("-5*X0 1*X1 <= 0"));
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
              EndsWith("-6*X0 1*X1 <= -5"));
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
              StartsWith("-6 <= -5*X0 1*X1"));
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
              EndsWith("2*X0 1*X1 -1*X2 <= 2"));
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
              EndsWith("3*X0 5*X1 -1*X2 <= 15"));
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
              StartsWith("3 <= 3*X0 1*X1 -1*X2"));
  EXPECT_THAT(manager->AllConstraints().back().constraint.DebugString(),
              StartsWith("10 <= 2*X0 5*X1 -1*X2"));
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
            "50 <= 1*X0 1*X1 1*X2 <= 50");
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
            "25 <= 1*X1 1*X2 <= 40");
  EXPECT_EQ(manager->AllConstraints().back().constraint.DebugString(),
            "50 <= 1*X0 1*X1 1*X2 <= 50");
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

  // x vars are X0,X1 respectively, target is X2, z_vars are X3,X4,X5
  // respectively.
  // Most violated inequality is 2.
  EXPECT_THAT(manager->AllConstraints().front().constraint.DebugString(),
              StartsWith("0 <= -2*X1 -1*X2 3*X3 1*X4 4*X5"));

  InitializeLpValues({-1.0, -1.0, 2.0, 1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0},
                     &model);
  max_cuts.generate_cuts(manager);
  ASSERT_EQ(2, manager->num_cuts());
  // Most violated inequality is 3.
  EXPECT_THAT(manager->AllConstraints().back().constraint.DebugString(),
              StartsWith("0 <= 1*X1 -1*X2 2*X3 4*X4 1*X5"));
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
  EXPECT_EQ(builder.Build().DebugString(), "20*X1 <= 200");
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

  EXPECT_EQ(builder.Build().DebugString(), "-9*X0 11*X1 <= 20");
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
  const IntegerValue t(1);
  std::vector<CutTerm> new_terms;
  EXPECT_TRUE(processor.TryToExpandWithLowerImpliedbound(
      t, /*complement=*/false, &data.terms[0], &data.rhs, &new_terms));

  EXPECT_EQ(0, processor.MutableCutBuilder()->AddOrMergeBooleanTerms(
                   absl::MakeSpan(new_terms), t, &data));

  EXPECT_EQ(data.terms.size(), 2);
  EXPECT_THAT(data.terms[0].DebugString(),
              ::testing::StartsWith("coeff=1 lp=0 range=7"));
  EXPECT_THAT(data.terms[1].DebugString(),
              ::testing::StartsWith("coeff=3 lp=0.666667 range=1"));
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

  const IntegerValue t(1);
  std::vector<CutTerm> new_terms;
  EXPECT_TRUE(processor.TryToExpandWithLowerImpliedbound(
      t, /*complement=*/false, &data.terms[0], &data.rhs, &new_terms));
  EXPECT_EQ(0, processor.MutableCutBuilder()->AddOrMergeBooleanTerms(
                   absl::MakeSpan(new_terms), t, &data));

  EXPECT_EQ(data.terms.size(), 2);
  EXPECT_THAT(data.terms[0].DebugString(),
              ::testing::StartsWith("coeff=1 lp=0 range=7"));
  EXPECT_THAT(data.terms[1].DebugString(),
              ::testing::StartsWith("coeff=3 lp=0.666667 range=1"));

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
  EXPECT_TRUE(processor.DecomposeWithImpliedLowerBound(X, IntegerValue(1),
                                                       bool_term, slack_term));
  EXPECT_THAT(bool_term.DebugString(),
              ::testing::StartsWith("coeff=3 lp=0.666667 range=1"));
  EXPECT_THAT(slack_term.DebugString(),
              ::testing::StartsWith("coeff=1 lp=0 range=7"));

  // (9 - X) = 7 * C + slack;
  CutTerm Y = X;
  absl::int128 unused;
  Y.Complement(&unused);
  Y.coeff = -Y.coeff;
  EXPECT_TRUE(processor.DecomposeWithImpliedLowerBound(Y, IntegerValue(1),
                                                       bool_term, slack_term));
  EXPECT_THAT(bool_term.DebugString(),
              ::testing::StartsWith("coeff=7 lp=0.5 range=1"));
  EXPECT_THAT(slack_term.DebugString(),
              ::testing::StartsWith("coeff=1 lp=1.5 range=7"));

  // X - 2 = 7 * (1 - C) - slack;
  EXPECT_TRUE(processor.DecomposeWithImpliedUpperBound(X, IntegerValue(1),
                                                       bool_term, slack_term));
  EXPECT_THAT(bool_term.DebugString(),
              ::testing::StartsWith("coeff=7 lp=0.5 range=1"));
  EXPECT_THAT(slack_term.DebugString(),
              ::testing::StartsWith("coeff=-1 lp=1.5 range=7"));
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

  // 6 (X0' + 7) - 4 (X1' - 3) <= 9
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

}  // namespace
}  // namespace sat
}  // namespace operations_research
