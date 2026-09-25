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

#include "ortools/sat/exact/exact_lp_utils.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/linear_solver/linear_solver.pb.h"

namespace operations_research {
namespace sat {
namespace {

constexpr int64_t kMaxActivity = 1ULL << 38;

// Helper to quickly add an integer variable to a model
void AddIntVar(MPModelProto* model, double lb, double ub) {
  auto* var = model->add_variable();
  var->set_lower_bound(lb);
  var->set_upper_bound(ub);
  var->set_is_integer(true);
}

TEST(ConvertConstraintToIntegerTest, ExactIntegerConstraint) {
  MPModelProto model;
  AddIntVar(&model, 0.0, 10.0);  // x
  AddIntVar(&model, 0.0, 10.0);  // y
  const std::vector<int64_t> lbs = {0, 0};
  const std::vector<int64_t> ubs = {10, 10};

  MPConstraintProto ct;
  ct.add_var_index(0);
  ct.add_coefficient(2.0);
  ct.add_var_index(1);
  ct.add_coefficient(3.0);
  ct.set_lower_bound(-std::numeric_limits<double>::infinity());
  ct.set_upper_bound(10.5);  // 2x + 3y <= 10.5

  auto result =
      ConvertConstraintToIntegerIfPossible(ct, lbs, ubs, kMaxActivity);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->coeffs(0), 2);
  EXPECT_EQ(result->coeffs(1), 3);

  // Upper bound should be floored to 10.
  // Lower bound should be -sum_abs_activity = -(2*10 + 3*10) = -50
  EXPECT_EQ(result->domain(0), -50);
  EXPECT_EQ(result->domain(1), 10);
}

TEST(ConvertConstraintToIntegerTest, DyadicFractionScaling) {
  MPModelProto model;
  AddIntVar(&model, 0.0, 10.0);
  AddIntVar(&model, 0.0, 10.0);
  const std::vector<int64_t> lbs = {0, 0};
  const std::vector<int64_t> ubs = {10, 10};

  MPConstraintProto ct;
  ct.add_var_index(0);
  ct.add_coefficient(0.5);
  ct.add_var_index(1);
  ct.add_coefficient(0.25);
  ct.set_lower_bound(1.5);
  ct.set_upper_bound(3.0);

  // 0.5x + 0.25y in [1.5, 3.0] should perfectly scale by 4 to:
  // 2x + 1y in [6, 12]
  auto result =
      ConvertConstraintToIntegerIfPossible(ct, lbs, ubs, kMaxActivity);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->coeffs(0), 2);
  EXPECT_EQ(result->coeffs(1), 1);
  EXPECT_EQ(result->domain(0), 6);
  EXPECT_EQ(result->domain(1), 12);
}

TEST(ConvertConstraintToIntegerTest, RejectsBoundaryCrossingNoise) {
  MPModelProto model;
  AddIntVar(&model, 0.0, 1000.0);  // Large bounds to amplify noise
  const std::vector<int64_t> lbs = {0};
  const std::vector<int64_t> ubs = {1000};

  MPConstraintProto ct;
  ct.add_var_index(0);
  ct.add_coefficient(1.001);  // Close to 1, but delta is 0.001
  ct.set_lower_bound(0.0);
  ct.set_upper_bound(2.0);

  // max_sum_delta_xi = 0.001 * 1000 = 1.0.
  // ub_min = 2.0 - 0.0 = 2.0 (floor is 2)
  // ub_max = 2.0 - 1.0 = 1.0 (floor is 1)
  // Because 2 != 1, this should be rejected.
  auto result =
      ConvertConstraintToIntegerIfPossible(ct, lbs, ubs, kMaxActivity);
  EXPECT_FALSE(result.has_value());
}

TEST(ConvertConstraintToIntegerTest, AcceptsSafeNoise) {
  MPModelProto model;
  // Variables with bounds [0, 10]
  AddIntVar(&model, 0.0, 10.0);  // x
  AddIntVar(&model, 0.0, 10.0);  // y
  const std::vector<int64_t> lbs = {0, 0};
  const std::vector<int64_t> ubs = {10, 10};

  MPConstraintProto ct;
  ct.add_var_index(0);
  // 2.00001 requires >50 bits to be exact in base-2, so scaling_shift will be
  // 0. The delta will be +0.00001.
  ct.add_coefficient(2.00001);

  ct.add_var_index(1);
  // Delta will be +0.00002.
  ct.add_coefficient(3.00002);

  ct.set_lower_bound(-std::numeric_limits<double>::infinity());
  ct.set_upper_bound(10.001);

  // The max possible delta sum is: (0.00001 * 10) + (0.00002 * 10) = 0.0003
  // ub_min = 10.001 - 0.0 = 10.001      --> floor is 10
  // ub_max = 10.001 - 0.0003 = 10.0097 --> floor is 10
  // Because 10 == 10, the noise is strictly harmless and the constraint is
  // safely converted!

  auto result =
      ConvertConstraintToIntegerIfPossible(ct, lbs, ubs, kMaxActivity);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->coeffs(0), 2);
  EXPECT_EQ(result->coeffs(1), 3);

  // The upper bound cleanly tightens down to 10
  EXPECT_EQ(result->domain(1), 10);

  // The lower bound implicitly clamps to -sum_abs_activity = -((2*10) + (3*10))
  // = -50
  EXPECT_EQ(result->domain(0), -50);
}

// -----------------------------------------------------------------------------
// Randomized Property-Based Equivalence Test
// -----------------------------------------------------------------------------
TEST(ConvertConstraintToIntegerTest, FuzzTestEquivalenceWithSafeNoise) {
  absl::BitGen gen;  // Non-deterministic: catches flakiness over time.

  const int kNumIterations = 300000;
  const int kVarsPerCt = 5;
  const int kChecksPerConvert = 50;

  int successful_conversions = 0;

  for (int iter = 0; iter < kNumIterations; ++iter) {
    MPModelProto model;
    MPConstraintProto ct;
    std::vector<int64_t> lbs(kVarsPerCt);
    std::vector<int64_t> ubs(kVarsPerCt);

    for (int i = 0; i < kVarsPerCt; ++i) {
      // Must use IntervalClosedClosed so random assignments can hit bounds.
      int64_t lb = absl::Uniform(absl::IntervalClosedClosed, gen, -100, 100);
      int64_t ub = absl::Uniform(absl::IntervalClosedClosed, gen, -100, 100);
      if (lb > ub) std::swap(lb, ub);
      AddIntVar(&model, lb, ub);
      lbs[i] = lb;
      ubs[i] = ub;

      ct.add_var_index(i);

      // Generate an integer coefficient + tiny non-dyadic noise.
      // Because absl::Uniform generates a float with a full 53-bit mantissa,
      // this reliably exceeds the 20-bit scaling limit and forces the
      // conversion to use your delta-rounding tolerance logic!
      double base_coeff =
          absl::Uniform(absl::IntervalClosedClosed, gen, -10, 10);
      double noise = absl::Uniform(gen, -1e-6, 1e-6);
      ct.add_coefficient(base_coeff + noise);
    }

    // Generate fractional bounds near .5 (e.g., 14.5).
    // This ensures they are far away from integers, so the delta from the
    // noisy coefficients will easily pass the floor/ceil equivalence checks.
    double ct_lb =
        absl::Uniform(absl::IntervalClosedClosed, gen, -250, 250) + 0.5;
    double ct_ub =
        absl::Uniform(absl::IntervalClosedClosed, gen, -250, 250) + 0.5;
    if (ct_lb > ct_ub) std::swap(ct_lb, ct_ub);

    ct.set_lower_bound(ct_lb);
    ct.set_upper_bound(ct_ub);

    auto result =
        ConvertConstraintToIntegerIfPossible(ct, lbs, ubs, kMaxActivity);

    // If the random noise happened to be unsafe (crossed an integer boundary),
    // the algorithm safely rejects it. We only test successful conversions.
    if (!result.has_value()) continue;
    successful_conversions++;

    // Property Test: Check strict equivalence
    for (int check = 0; check < kChecksPerConvert; ++check) {
      double original_activity = 0.0;
      int64_t new_activity = 0;

      std::vector<int64_t> x(kVarsPerCt);
      for (int i = 0; i < kVarsPerCt; ++i) {
        const int64_t var_lb = model.variable(i).lower_bound();
        const int64_t var_ub = model.variable(i).upper_bound();
        x[i] = absl::Uniform(absl::IntervalClosedClosed, gen, var_lb, var_ub);

        original_activity += ct.coefficient(i) * x[i];
      }
      for (int j = 0; j < result->vars_size(); ++j) {
        new_activity += result->coeffs(j) * x[result->vars(j)];
      }

      // STRICT EXACT MATCH: Zero epsilon.
      // We can do this safely because your floor/ceil logic proved that
      // original_activity is bounded away from the threshold by at least
      // the margin of error.
      const bool original_is_sat = (original_activity >= ct.lower_bound() &&
                                    original_activity <= ct.upper_bound());

      const bool new_is_sat =
          !result->domain().empty() && (new_activity >= result->domain(0) &&
                                        new_activity <= result->domain(1));

      ASSERT_EQ(original_is_sat, new_is_sat)
          << "Mismatch on noisy fuzz iteration " << iter << "\n"
          << "Original float activity: " << original_activity << " in ["
          << ct.lower_bound() << ", " << ct.upper_bound() << "]\n"
          << "New integer activity: " << new_activity << " in "
          << (result->domain().empty()
                  ? "[]"
                  : absl::StrCat("[", result->domain(0), ", ",
                                 result->domain(1), "]"));
    }
  }

  // Because the fractional bound gap is ~0.5 and our max noise is tiny,
  // virtually all 1000 of these should safely convert.
  EXPECT_GT(successful_conversions, 0);
}

TEST(ConvertConstraintToIntegerTest, InfeasibleConstraintEmitsEmptyDomain) {
  MPModelProto model;
  AddIntVar(&model, 0.0, 10.0);  // max_act = 10
  const std::vector<int64_t> lbs = {0};
  const std::vector<int64_t> ubs = {10};

  // Case 1: ub < -max_act (-15.0 < -10) -> integer_ub = -11 < integer_lb = -10
  {
    MPConstraintProto ct;
    ct.add_var_index(0);
    ct.add_coefficient(1.0);
    ct.set_lower_bound(-std::numeric_limits<double>::infinity());
    ct.set_upper_bound(-15.0);

    auto result =
        ConvertConstraintToIntegerIfPossible(ct, lbs, ubs, kMaxActivity);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->domain().empty());
  }
}

TEST(ConvertConstraintToIntegerTest, BoundaryShiftAtMinusMaxActIsExact) {
  MPModelProto model;
  // x in [-10, -5], so max_act = 10.
  AddIntVar(&model, -10.0, -5.0);
  const std::vector<int64_t> lbs = {-10};
  const std::vector<int64_t> ubs = {-5};

  MPConstraintProto ct;
  ct.add_var_index(0);
  // coeff = 1.02 => round = 1, delta = +0.02.
  // For x in [-10, -5], delta * x is in [-0.20, -0.10].
  ct.add_coefficient(1.02);
  ct.set_lower_bound(-std::numeric_limits<double>::infinity());
  // At x = -10, 1.02 * (-10) = -10.20 <= -10.05 (FEASIBLE).
  // At x = -9,  1.02 * (-9)  = -9.18  >  -10.05 (INFEASIBLE).
  // Note that floor(ub) = floor(-10.05) = -11 < -max_act (-10),
  // but floor(ub_frac - delta*x) = floor(0.95 - (-0.10)) = +1,
  // so exact_ub = -11 + 1 = -10 = -max_act.
  ct.set_upper_bound(-10.05);

  auto result =
      ConvertConstraintToIntegerIfPossible(ct, lbs, ubs, kMaxActivity);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->domain_size(), 2);
  EXPECT_EQ(result->domain(0), -10);
  EXPECT_EQ(result->domain(1), -10);
}

TEST(ConvertConstraintToIntegerTest,
     VacuousConstraintConvertsToZeroEqualsZero) {
  MPModelProto model;
  AddIntVar(&model, 0.0, 10.0);
  const std::vector<int64_t> lbs = {0};
  const std::vector<int64_t> ubs = {10};

  MPConstraintProto ct;
  ct.add_var_index(0);
  ct.add_coefficient(1.234567);
  ct.set_lower_bound(-std::numeric_limits<double>::infinity());
  ct.set_upper_bound(std::numeric_limits<double>::infinity());

  auto result =
      ConvertConstraintToIntegerIfPossible(ct, lbs, ubs, kMaxActivity);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->vars_size(), 0);
  EXPECT_EQ(result->coeffs_size(), 0);
  ASSERT_EQ(result->domain_size(), 2);
  EXPECT_EQ(result->domain(0), 0);
  EXPECT_EQ(result->domain(1), 0);
}

TEST(ConvertConstraintToIntegerTest,
     FixedZeroVariableDoesNotOverflowScaledCoefficient) {
  // Variable 0 is fixed to [0, 0] with a large coefficient (e.g., 1e15), while
  // variable 1 in [0, 1] has a small dyadic coefficient (2^-30) that induces
  // exact_shift = 30. Scaling 1e15 by 2^30 exceeds INT64_MAX (~9.22e18).
  // ConvertConstraintToIntegerIfPossible must skip variable 0 and cleanly
  // convert the constraint over variable 1 without overflowing int64_t.
  const std::vector<int64_t> lbs = {0, 0};
  const std::vector<int64_t> ubs = {0, 1};

  MPConstraintProto ct;
  ct.add_var_index(0);
  ct.add_coefficient(1e15);
  ct.add_var_index(1);
  ct.add_coefficient(std::ldexp(1.0, -30));
  ct.set_lower_bound(0.0);
  ct.set_upper_bound(1.0);

  auto result =
      ConvertConstraintToIntegerIfPossible(ct, lbs, ubs, kMaxActivity);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->vars_size(), 1);
  EXPECT_EQ(result->vars(0), 1);
  EXPECT_EQ(result->coeffs(0), 1);
  ASSERT_EQ(result->domain_size(), 2);
  EXPECT_EQ(result->domain(0), 0);
  EXPECT_EQ(result->domain(1), 1);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
