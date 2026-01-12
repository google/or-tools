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

#include "ortools/pdlp/trust_region.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <tuple>
#include <vector>

#include "Eigen/Core"
#include "Eigen/SparseCore"
#include "absl/base/optimization.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/pdlp/quadratic_program.h"
#include "ortools/pdlp/sharded_optimization_utils.h"
#include "ortools/pdlp/sharded_quadratic_program.h"
#include "ortools/pdlp/sharder.h"
#include "ortools/pdlp/test_util.h"

namespace operations_research::pdlp {
namespace {

using ::Eigen::VectorXd;
using ::testing::DoubleNear;
using ::testing::ElementsAre;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

class TrustRegion : public testing::TestWithParam<
                        /*use_diagonal_solver=*/bool> {};

INSTANTIATE_TEST_SUITE_P(
    TrustRegionSolvers, TrustRegion, testing::Bool(),
    [](const testing::TestParamInfo<TrustRegion::ParamType>& info) {
      return (info.param) ? "UseApproximateTRSolver" : "UseLinearTimeTRSolver";
    });

TEST_P(TrustRegion, SolvesWithoutVariableBounds) {
  // min x + y
  // ||(x - 2.0, y - (-5.0))||_2 <= sqrt(2)
  // [x*, y*] = [1.0, -6.0]
  const VectorXd variable_lower_bounds{{-kInfinity, -kInfinity}};
  const VectorXd variable_upper_bounds{{kInfinity, kInfinity}};
  const VectorXd center_point{{2.0, -5.0}};
  const VectorXd objective_vector{{1.0, 1.0}};
  const double target_radius = std::sqrt(2.0);

  Sharder sharder(/*num_elements=*/2, /*num_shards=*/2, nullptr);

  const VectorXd expected_solution{{1.0, -6.0}};
  const double expected_objective_value = -2.0;

  if (GetParam()) {
    TrustRegionResult result = SolveDiagonalTrustRegion(
        objective_vector, /*objective_matrix_diagonal=*/VectorXd::Zero(2),
        variable_lower_bounds, variable_upper_bounds, center_point,
        /*norm_weights=*/VectorXd::Ones(2), target_radius, sharder,
        /*solve_tolerance=*/1.0e-8);
    EXPECT_THAT(result.solution, EigenArrayNear(expected_solution, 1.0e-6));
    EXPECT_NEAR(result.objective_value, expected_objective_value, 1.0e-6);
  } else {
    TrustRegionResult result = SolveTrustRegion(
        objective_vector, variable_lower_bounds, variable_upper_bounds,
        center_point, /*norm_weights=*/VectorXd::Ones(2), target_radius,
        sharder);
    EXPECT_THAT(result.solution, EigenArrayEq(expected_solution));
    EXPECT_DOUBLE_EQ(result.objective_value, expected_objective_value);
  }
}

TEST_P(TrustRegion, SolvesWithVariableBounds) {
  // min x - y + z
  // ||(x - 2.0, y - (-5.0), z - 1.0)||_2 <= sqrt(2.0)
  // x >= 2.0
  // [x*, y*, z*] = [2.0, -4.0, 0.0]
  const VectorXd variable_lower_bounds{{2.0, -kInfinity, -kInfinity}};
  const VectorXd variable_upper_bounds{{kInfinity, kInfinity, kInfinity}};
  const VectorXd center_point{{2.0, -5.0, 1.0}};
  const VectorXd objective_vector{{1.0, -1.0, 1.0}};
  const double target_radius = std::sqrt(2.0);

  Sharder sharder(/*num_elements=*/3, /*num_shards=*/2, nullptr);

  const VectorXd expected_solution{{2.0, -4.0, 0.0}};
  const double expected_objective_value = -2.0;

  if (GetParam()) {
    TrustRegionResult result = SolveDiagonalTrustRegion(
        objective_vector, /*objective_matrix_diagonal=*/VectorXd::Zero(3),
        variable_lower_bounds, variable_upper_bounds, center_point,
        /*norm_weights=*/VectorXd::Ones(3), target_radius, sharder,
        /*solve_tolerance=*/1.0e-6);
    EXPECT_THAT(result.solution, EigenArrayNear(expected_solution, 1.0e-6));
    EXPECT_NEAR(result.objective_value, expected_objective_value, 1.0e-6);
  } else {
    TrustRegionResult result = SolveTrustRegion(
        objective_vector, variable_lower_bounds, variable_upper_bounds,
        center_point, /*norm_weights=*/VectorXd::Ones(3), target_radius,
        sharder);

    EXPECT_THAT(result.solution, EigenArrayEq(expected_solution));
    EXPECT_DOUBLE_EQ(result.objective_value, expected_objective_value);
  }
}

TEST_P(TrustRegion, SolvesAtVariableBounds) {
  // min x - y
  // ||(x - 2.0, y - (-5.0))||_2 <= 1
  // x >= 2.0, y <= -5.0
  // [x*, y*] = [2.0, -5.0]
  // The bound constraints block movement from the center point.
  const VectorXd variable_lower_bounds{{2.0, -kInfinity}};
  const VectorXd variable_upper_bounds{{kInfinity, -5.0}};
  const VectorXd center_point{{2.0, -5.0}};
  const VectorXd objective_vector{{1.0, -1.0}};
  const double target_radius = 1.0;

  Sharder sharder(/*num_elements=*/2, /*num_shards=*/2, nullptr);

  const VectorXd expected_solution{{2.0, -5.0}};
  const double expected_objective_value = 0.0;

  if (GetParam()) {
    TrustRegionResult result = SolveDiagonalTrustRegion(
        objective_vector, /*objective_matrix_diagonal=*/VectorXd::Zero(2),
        variable_lower_bounds, variable_upper_bounds, center_point,
        /*norm_weights=*/VectorXd::Ones(2), target_radius, sharder,
        /*solve_tolerance=*/1.0e-6);

    EXPECT_THAT(result.solution, EigenArrayNear(expected_solution, 1.0e-6));
    EXPECT_NEAR(result.objective_value, expected_objective_value, 1.0e-6);
  } else {
    TrustRegionResult result = SolveTrustRegion(
        objective_vector, variable_lower_bounds, variable_upper_bounds,
        center_point, /*norm_weights=*/VectorXd::Ones(2), target_radius,
        sharder);

    EXPECT_THAT(result.solution, EigenArrayEq(expected_solution));
    EXPECT_DOUBLE_EQ(result.objective_value, expected_objective_value);
  }
}

TEST_P(TrustRegion, SolvesWithInactiveRadius) {
  // min x - y + z
  // ||(x - 2.0, y - (-5.0), z - 1.0)||_2 <= 1
  // x >= 2.0, y <= -5.0, z >= 0.5
  // [x*, y*, z*] = [2.0, -5.0, 0.5]
  // This is a corner case where the radius constraint is not active at the
  // solution.
  const VectorXd variable_lower_bounds{{2.0, -kInfinity, 0.5}};
  const VectorXd variable_upper_bounds{{kInfinity, -5.0, kInfinity}};
  const VectorXd center_point{{2.0, -5.0, 1.0}};
  const VectorXd objective_vector{{1.0, -1.0, 1.0}};
  const double target_radius = 1.0;

  Sharder sharder(/*num_elements=*/3, /*num_shards=*/2, nullptr);

  const VectorXd expected_solution{{2.0, -5.0, 0.5}};
  const double expected_objective_value = -0.5;

  if (GetParam()) {
    TrustRegionResult result = SolveDiagonalTrustRegion(
        objective_vector, /*objective_matrix_diagonal=*/VectorXd::Zero(3),
        variable_lower_bounds, variable_upper_bounds, center_point,
        /*norm_weights=*/VectorXd::Ones(3), target_radius, sharder,
        /*solve_tolerance=*/1.0e-6);

    EXPECT_THAT(result.solution, EigenArrayNear(expected_solution, 1.0e-6));
    EXPECT_NEAR(result.objective_value, expected_objective_value, 1.0e-6);
  } else {
    TrustRegionResult result = SolveTrustRegion(
        objective_vector, variable_lower_bounds, variable_upper_bounds,
        center_point, /*norm_weights=*/VectorXd::Ones(3), target_radius,
        sharder);

    EXPECT_THAT(result.solution, EigenArrayEq(expected_solution));
    EXPECT_DOUBLE_EQ(result.objective_value, expected_objective_value);
  }
}

TEST_P(TrustRegion, SolvesWithZeroRadius) {
  // min x - y + z
  // ||(x - 2.0, y - (-5.0), z - 1.0)||_2 <= 0.0
  // x >= 2.0, y <= -5.0, z >= 0.5
  // [x*, y*, z*] = [2.0, -5.0, 0.5]
  const VectorXd variable_lower_bounds{{2.0, -kInfinity, 0.5}};
  const VectorXd variable_upper_bounds{{kInfinity, -5.0, kInfinity}};
  const VectorXd center_point{{2.0, -5.0, 1.0}};
  const VectorXd objective_vector{{1.0, -1.0, 1.0}};
  const double target_radius = 0.0;

  Sharder sharder(/*num_elements=*/3, /*num_shards=*/2, nullptr);

  const VectorXd expected_solution{{2.0, -5.0, 1.0}};
  const double expected_objective_value = 0.0;

  if (GetParam()) {
    TrustRegionResult result = SolveDiagonalTrustRegion(
        objective_vector, /*objective_matrix_diagonal=*/VectorXd::Zero(3),
        variable_lower_bounds, variable_upper_bounds, center_point,
        /*norm_weights=*/VectorXd::Ones(3), target_radius, sharder,
        /*solve_tolerance=*/1.0e-6);

    EXPECT_THAT(result.solution, EigenArrayNear(expected_solution, 1.0e-6));
    EXPECT_NEAR(result.objective_value, expected_objective_value, 1.0e-6);
  } else {
    TrustRegionResult result = SolveTrustRegion(
        objective_vector, variable_lower_bounds, variable_upper_bounds,
        center_point, /*norm_weights=*/VectorXd::Ones(3), target_radius,
        sharder);

    EXPECT_THAT(result.solution, EigenArrayEq(expected_solution));
    EXPECT_DOUBLE_EQ(result.objective_value, expected_objective_value);
  }
}

TEST_P(TrustRegion, SolvesWithInfiniteRadius) {
  // min x - y + z
  // ||(x - 2.0, y - (-5.0), z - 1.0)||_2 <= Infinity
  // x >= 2.0, y <= -5.0, z >= 0.5
  // [x*, y*, z*] = [2.0, -5.0, 0.5]
  const VectorXd variable_lower_bounds{{2.0, -kInfinity, 0.5}};
  const VectorXd variable_upper_bounds{{kInfinity, -5.0, kInfinity}};
  const VectorXd center_point{{2.0, -5.0, 1.0}};
  const VectorXd objective_vector{{1.0, -1.0, 1.0}};
  const double target_radius = kInfinity;

  Sharder sharder(/*num_elements=*/3, /*num_shards=*/2, nullptr);

  const VectorXd expected_solution{{2.0, -5.0, 0.5}};
  const double expected_objective_value = -0.5;

  if (GetParam()) {
    TrustRegionResult result = SolveDiagonalTrustRegion(
        objective_vector, /*objective_matrix_diagonal=*/VectorXd::Zero(3),
        variable_lower_bounds, variable_upper_bounds, center_point,
        /*norm_weights=*/VectorXd::Ones(3), target_radius, sharder,
        /*solve_tolerance=*/1.0e-6);

    EXPECT_THAT(result.solution, EigenArrayNear(expected_solution, 1.0e-6));
    EXPECT_NEAR(result.objective_value, expected_objective_value, 1.0e-6);
  } else {
    TrustRegionResult result = SolveTrustRegion(
        objective_vector, variable_lower_bounds, variable_upper_bounds,
        center_point, /*norm_weights=*/VectorXd::Ones(3), target_radius,
        sharder);

    EXPECT_THAT(result.solution, EigenArrayEq(expected_solution));
    EXPECT_DOUBLE_EQ(result.objective_value, expected_objective_value);
  }
}

TEST_P(TrustRegion, SolvesWithMixedObjective) {
  // min 2x + y
  // ||(x - 2.0, y - 1.0)||_2 <= sqrt(1.25)
  // x >= 1.0, y >= 0
  // [x*, y*] = [1.0, 0.5]
  // We take a positive step in all coordinates. Only the first coordinate
  // hits its bound.
  const VectorXd variable_lower_bounds{{1.0, 0.0}};
  const VectorXd variable_upper_bounds{{kInfinity, kInfinity}};
  const VectorXd center_point{{2.0, 1.0}};
  const VectorXd objective_vector{{2.0, 1.0}};
  const double target_radius = std::sqrt(1.25);

  Sharder sharder(/*num_elements=*/2, /*num_shards=*/2, nullptr);

  const VectorXd expected_solution{{1.0, 0.5}};
  const double expected_objective_value = -2.5;

  if (GetParam()) {
    TrustRegionResult result = SolveDiagonalTrustRegion(
        objective_vector, /*objective_matrix_diagonal=*/VectorXd::Zero(2),
        variable_lower_bounds, variable_upper_bounds, center_point,
        /*norm_weights=*/VectorXd::Ones(2), target_radius, sharder,
        /*solve_tolerance=*/1.0e-6);
    EXPECT_THAT(result.solution, EigenArrayNear(expected_solution, 1.0e-6));
    EXPECT_NEAR(result.objective_value, expected_objective_value, 2.0e-6);
  } else {
    TrustRegionResult result = SolveTrustRegion(
        objective_vector, variable_lower_bounds, variable_upper_bounds,
        center_point, /*norm_weights=*/VectorXd::Ones(2), target_radius,
        sharder);

    EXPECT_THAT(result.solution, EigenArrayEq(expected_solution));
    EXPECT_DOUBLE_EQ(result.objective_value, expected_objective_value);
  }
}

TEST_P(TrustRegion, SolvesWithZeroObjectiveNoBounds) {
  // min 0*x
  // ||(x - 2.0)||_2 <= 1
  // x* = 2.0
  const VectorXd variable_lower_bounds{{-kInfinity}};
  const VectorXd variable_upper_bounds{{kInfinity}};
  const VectorXd center_point{{2.0}};
  const VectorXd objective_vector{{0.0}};
  const double target_radius = 1.0;

  Sharder sharder(/*num_elements=*/1, /*num_shards=*/1, nullptr);

  const VectorXd expected_solution{{2.0}};
  const double expected_objective_value = 0.0;

  if (GetParam()) {
    TrustRegionResult result = SolveDiagonalTrustRegion(
        objective_vector, /*objective_matrix_diagonal=*/VectorXd::Zero(1),
        variable_lower_bounds, variable_upper_bounds, center_point,
        /*norm_weights=*/VectorXd::Ones(1), target_radius, sharder,
        /*solve_tolerance=*/1.0e-6);

    EXPECT_THAT(result.solution, EigenArrayNear(expected_solution, 1.0e-6));
    EXPECT_NEAR(result.objective_value, expected_objective_value, 1.0e-6);
  } else {
    TrustRegionResult result = SolveTrustRegion(
        objective_vector, variable_lower_bounds, variable_upper_bounds,
        center_point, /*norm_weights=*/VectorXd::Ones(1), target_radius,
        sharder);

    EXPECT_THAT(result.solution, EigenArrayEq(expected_solution));
    EXPECT_DOUBLE_EQ(result.objective_value, expected_objective_value);
  }
}

class TrustRegionWithWeights : public testing::TestWithParam<
                                   /*use_diagonal_solver=*/bool> {};

INSTANTIATE_TEST_SUITE_P(
    TrustRegionSolverWithWeights, TrustRegionWithWeights, testing::Bool(),
    [](const testing::TestParamInfo<TrustRegion::ParamType>& info) {
      return (info.param) ? "UseApproximateTRSolver" : "UseLinearTimeTRSolver";
    });

TEST_P(TrustRegionWithWeights, SolvesWithoutVariableBounds) {
  // min x + 2.0 y
  // ||(x - 2.0, y - (-5.0))||_W <= sqrt(3)
  // norm_weights = [1.0, 2.0]
  // [x*, y*] = [1.0, -6.0]
  const VectorXd variable_lower_bounds{{-kInfinity, -kInfinity}};
  const VectorXd variable_upper_bounds{{kInfinity, kInfinity}};
  const VectorXd center_point{{2.0, -5.0}};
  const VectorXd objective_vector{{1.0, 2.0}};
  const VectorXd norm_weights{{1.0, 2.0}};
  const double target_radius = std::sqrt(3.0);

  Sharder sharder(/*num_elements=*/2, /*num_shards=*/2, nullptr);

  const VectorXd expected_solution{{1.0, -6.0}};
  const double expected_objective_value = -3.0;

  if (GetParam()) {
    TrustRegionResult result = SolveDiagonalTrustRegion(
        objective_vector, /*objective_matrix_diagonal=*/VectorXd::Zero(2),
        variable_lower_bounds, variable_upper_bounds, center_point,
        norm_weights, target_radius, sharder, /*solve_tolerance=*/1.0e-6);

    EXPECT_THAT(result.solution, EigenArrayNear(expected_solution, 1.0e-6));
    EXPECT_NEAR(result.objective_value, expected_objective_value, 1.0e-5);
  } else {
    TrustRegionResult result = SolveTrustRegion(
        objective_vector, variable_lower_bounds, variable_upper_bounds,
        center_point, norm_weights, target_radius, sharder);

    EXPECT_THAT(result.solution, EigenArrayEq(expected_solution));
    EXPECT_DOUBLE_EQ(result.objective_value, expected_objective_value);
  }
}

TEST_P(TrustRegionWithWeights, SolvesWithVariableBounds) {
  // min 0.5 x - 2.0 y + 3.0 z
  // ||(x - 2.0, y - (-5.0), z - 1.0)||_W <= sqrt(5)
  // x >= 2.0
  // norm_weights = [0.5, 2.0, 3.0]
  // [x*, y*, z*] = [2.0, -4.0, 0.0]
  const VectorXd variable_lower_bounds{{2.0, -kInfinity, -kInfinity}};
  const VectorXd variable_upper_bounds{{kInfinity, kInfinity, kInfinity}};
  const VectorXd center_point{{2.0, -5.0, 1.0}};
  const VectorXd objective_vector{{0.5, -2.0, 3.0}};
  const VectorXd norm_weights{{0.5, 2.0, 3.0}};
  const double target_radius = std::sqrt(5.0);

  Sharder sharder(/*num_elements=*/3, /*num_shards=*/2, nullptr);

  const VectorXd expected_solution{{2.0, -4.0, 0.0}};
  const double expected_objective_value = -5.0;

  if (GetParam()) {
    TrustRegionResult result = SolveDiagonalTrustRegion(
        objective_vector, /*objective_matrix_diagonal=*/VectorXd::Zero(3),
        variable_lower_bounds, variable_upper_bounds, center_point,
        norm_weights, target_radius, sharder, /*solve_tolerance=*/1.0e-6);

    EXPECT_THAT(result.solution, EigenArrayNear(expected_solution, 1.0e-6));
    EXPECT_NEAR(result.objective_value, expected_objective_value, 1.0e-5);
  } else {
    TrustRegionResult result = SolveTrustRegion(
        objective_vector, variable_lower_bounds, variable_upper_bounds,
        center_point, norm_weights, target_radius, sharder);

    EXPECT_THAT(result.solution, EigenArrayEq(expected_solution));
    EXPECT_DOUBLE_EQ(result.objective_value, expected_objective_value);
  }
}

TEST_P(TrustRegionWithWeights, SolvesWithVariableThatHitsBounds) {
  // min x + 2y
  // ||(x - 2.0, y - 1.0)||_2 <= 1
  // x >= 1.0, y >= 0
  // [x*, y*] = [1.0, 0.5]
  // norm_weights = [0.5, 2.0]
  // We take a positive step in all coordinates. Only the first coordinate
  // hits its bound.
  const VectorXd variable_lower_bounds{{1.0, 0.0}};
  const VectorXd variable_upper_bounds{{kInfinity, kInfinity}};
  const VectorXd center_point{{2.0, 1.0}};
  const VectorXd objective_vector{{1.0, 2.0}};
  const VectorXd norm_weights{{0.5, 2.0}};
  const double target_radius = 1;

  Sharder sharder(/*num_elements=*/2, /*num_shards=*/2, nullptr);

  const VectorXd expected_solution{{1.0, 0.5}};
  const double expected_objective_value = -2.0;

  if (GetParam()) {
    TrustRegionResult result = SolveDiagonalTrustRegion(
        objective_vector, /*objective_matrix_diagonal=*/VectorXd::Zero(2),
        variable_lower_bounds, variable_upper_bounds, center_point,
        norm_weights, target_radius, sharder,
        /*solve_tolerance=*/1.0e-6);

    EXPECT_THAT(result.solution, EigenArrayNear(expected_solution, 1.0e-6));
    EXPECT_NEAR(result.objective_value, expected_objective_value, 1.0e-6);
  } else {
    TrustRegionResult result = SolveTrustRegion(
        objective_vector, variable_lower_bounds, variable_upper_bounds,
        center_point, norm_weights, target_radius, sharder);

    EXPECT_THAT(result.solution,
                ElementsAre(expected_solution[0],
                            DoubleNear(expected_solution[1], 1.0e-13)));
    EXPECT_DOUBLE_EQ(result.objective_value, expected_objective_value);
  }
}

TEST_P(TrustRegionWithWeights, SolvesWithLargeWeight) {
  // min 1000.0 x + 2y
  // ||(x - 2.0, y - 1.0)||_W <= sqrt(500.5)
  // x >= 1.0, y >= 0
  // [x*, y*] = [1.0, 0.5]
  // norm_weights = [500.0, 2.0]
  // We take a positive step in all coordinates. Only the first coordinate
  // hits its bound. The large norm weight stresses the code.
  const VectorXd variable_lower_bounds{{1.0, 0.0}};
  const VectorXd variable_upper_bounds{{kInfinity, kInfinity}};
  const VectorXd center_point{{2.0, 1.0}};
  const VectorXd objective_vector{{1000.0, 2.0}};
  const VectorXd norm_weights{{500.0, 2.0}};
  const double target_radius = std::sqrt(500.5);

  Sharder sharder(/*num_elements=*/2, /*num_shards=*/2, nullptr);

  const VectorXd expected_solution{{1.0, 0.5}};
  const double expected_objective_value = -1001.0;

  if (GetParam()) {
    TrustRegionResult result = SolveDiagonalTrustRegion(
        objective_vector, /*objective_matrix_diagonal=*/VectorXd::Zero(2),
        variable_lower_bounds, variable_upper_bounds, center_point,
        norm_weights, target_radius, sharder,
        /*solve_tolerance=*/1.0e-6);

    EXPECT_THAT(result.solution, EigenArrayNear(expected_solution, 1.0e-6));
    EXPECT_NEAR(result.objective_value, expected_objective_value, 1.0e-6);
  } else {
    TrustRegionResult result = SolveTrustRegion(
        objective_vector, variable_lower_bounds, variable_upper_bounds,
        center_point, norm_weights, target_radius, sharder);

    EXPECT_THAT(result.solution,
                ElementsAre(expected_solution[0],
                            DoubleNear(expected_solution[1], 1.0e-13)));
    EXPECT_DOUBLE_EQ(result.objective_value, -1001.0);
  }
}

TEST(TrustRegionDeathTest, CheckFailsWithNonPositiveWeights) {
  // min x + y
  // ||(x - 2.0, y - (-5.0))||_2 <= sqrt(2)
  // [x*, y*] = [1.0, -6.0]
  const VectorXd variable_lower_bounds{{-kInfinity, -kInfinity}};
  const VectorXd variable_upper_bounds{{kInfinity, kInfinity}};
  const VectorXd center_point{{2.0, -5.0}};
  const VectorXd objective_vector{{1.0, 1.0}};
  const VectorXd norm_weights{{0.0, 1.0}};
  const double target_radius = std::sqrt(2.0);

  Sharder sharder(/*num_elements=*/2, /*num_shards=*/2, nullptr);

  EXPECT_DEATH(TrustRegionResult result =
                   SolveTrustRegion(objective_vector, variable_lower_bounds,
                                    variable_upper_bounds, center_point,
                                    norm_weights, target_radius, sharder),
               "Check failed: norm_weights_are_positive");
}

TEST(TrustRegionDeathTest, CheckFailsWithNonPositiveWeightsForDiagonalSolver) {
  // min x + y
  // ||(x - 2.0, y - (-5.0))||_2 <= sqrt(2)
  // [x*, y*] = [1.0, -6.0]
  const VectorXd variable_lower_bounds{{-kInfinity, -kInfinity}};
  const VectorXd variable_upper_bounds{{kInfinity, kInfinity}};
  const VectorXd center_point{{2.0, -5.0}};
  const VectorXd objective_vector{{1.0, 1.0}};
  const VectorXd norm_weights{{0.0, 1.0}};
  const double target_radius = std::sqrt(2.0);

  Sharder sharder(/*num_elements=*/2, /*num_shards=*/2, nullptr);

  EXPECT_DEATH(
      TrustRegionResult result = SolveDiagonalTrustRegion(
          objective_vector, /*objective_matrix_diagonal=*/VectorXd::Zero(2),
          variable_lower_bounds, variable_upper_bounds, center_point,
          norm_weights, target_radius, sharder,
          /*solve_tolerance=*/1.0e-6),
      "Check failed: norm_weights_are_positive");
}

TEST(TrustRegionDeathTest, CheckFailsWithNegativeRadius) {
  // min x + y
  // ||(x - 2.0, y - (-5.0))||_2 <= sqrt(2)
  // [x*, y*] = [1.0, -6.0]
  const VectorXd variable_lower_bounds{{-kInfinity, -kInfinity}};
  const VectorXd variable_upper_bounds{{kInfinity, kInfinity}};
  const VectorXd center_point{{2.0, -5.0}};
  const VectorXd objective_vector{{1.0, 1.0}};
  const double target_radius = -std::sqrt(2.0);

  Sharder sharder(/*num_elements=*/2, /*num_shards=*/2, nullptr);

  EXPECT_DEATH(TrustRegionResult result = SolveTrustRegion(
                   objective_vector, variable_lower_bounds,
                   variable_upper_bounds, center_point,
                   /*norm_weights=*/VectorXd::Ones(2), target_radius, sharder),
               "Check failed: target_radius >= 0.0");
}

TEST(TrustRegionDeathTest, CheckFailsWithNegativeRadiusForDiagonalSolver) {
  // min x + y
  // ||(x - 2.0, y - (-5.0))||_2 <= sqrt(2)
  // [x*, y*] = [1.0, -6.0]
  const VectorXd variable_lower_bounds{{-kInfinity, -kInfinity}};
  const VectorXd variable_upper_bounds{{kInfinity, kInfinity}};
  const VectorXd center_point{{2.0, -5.0}};
  const VectorXd objective_vector{{1.0, 1.0}};
  const double target_radius = -std::sqrt(2.0);

  Sharder sharder(/*num_elements=*/2, /*num_shards=*/2, nullptr);

  EXPECT_DEATH(
      TrustRegionResult result = SolveDiagonalTrustRegion(
          objective_vector, /*objective_matrix_diagonal=*/VectorXd::Zero(2),
          variable_lower_bounds, variable_upper_bounds, center_point,
          /*norm_weights=*/VectorXd::Ones(2), target_radius, sharder,
          /*solve_tolerance=*/1.0e-6),
      "Check failed: target_radius >= 0.0");
}

class ComputeLocalizedLagrangianBoundsTest
    : public testing::TestWithParam<std::tuple<PrimalDualNorm, bool>> {
 protected:
  void SetUp() override {
    const auto [primal_dual_norm, use_diagonal_qp_trust_region_solver] =
        GetParam();
    if (use_diagonal_qp_trust_region_solver &&
        (primal_dual_norm == PrimalDualNorm::kMaxNorm)) {
      GTEST_SKIP() << "The diagonal QP trust region solver can only be used "
                   << "when the underlying norms are Euclidean.";
    }
  }
};

INSTANTIATE_TEST_SUITE_P(
    TrustRegionNorm, ComputeLocalizedLagrangianBoundsTest,
    testing::Combine(testing::Values(PrimalDualNorm::kEuclideanNorm,
                                     PrimalDualNorm::kMaxNorm),
                     testing::Bool()),
    [](const testing::TestParamInfo<
        ComputeLocalizedLagrangianBoundsTest::ParamType>& info) {
      const absl::string_view suffix =
          std::get<1>(info.param) ? "DiagonalTRSolver" : "LinearTimeTRSolver";
      switch (std::get<0>(info.param)) {
        case PrimalDualNorm::kEuclideanNorm:
          return absl::StrCat("EuclideanNorm", "_", suffix);
        case PrimalDualNorm::kMaxNorm:
          return absl::StrCat("MaxNorm", "_", suffix);
      }
      ABSL_UNREACHABLE();
    });

TEST_P(ComputeLocalizedLagrangianBoundsTest, ZeroGapAtOptimal) {
  ShardedQuadraticProgram lp(TestLp(), /*num_threads=*/2, /*num_shards=*/2);

  const VectorXd primal_solution{{-1.0, 8.0, 1.0, 2.5}};
  const VectorXd dual_solution{{-2.0, 0.0, 2.375, 2.0 / 3.0}};

  const auto [primal_dual_norm, use_diagonal_qp_solver] = GetParam();

  LocalizedLagrangianBounds bounds = ComputeLocalizedLagrangianBounds(
      lp, primal_solution, dual_solution, primal_dual_norm,
      /*primal_weight=*/1.0, /*radius=*/1.0,
      /*primal_product=*/nullptr,
      /*dual_product=*/nullptr, use_diagonal_qp_solver,
      /*diagonal_qp_trust_region_solver_tolerance=*/1.0e-2);

  EXPECT_DOUBLE_EQ(bounds.radius, 1.0);
  EXPECT_DOUBLE_EQ(bounds.lagrangian_value, -20.0);
  EXPECT_DOUBLE_EQ(bounds.lower_bound, -20.0);
  EXPECT_DOUBLE_EQ(bounds.upper_bound, -20.0);
}

// Sets the radius to the exact distance to optimal and checks that the
// optimal lagrangian value is contained in the computed interval.
TEST_P(ComputeLocalizedLagrangianBoundsTest, OptimalInBoundRange) {
  ShardedQuadraticProgram lp(TestLp(), /*num_threads=*/2, /*num_shards=*/2);

  // x_3 has a lower bound of 2.5.
  const VectorXd primal_solution{{0.0, 0.0, 0.0, 3.0}};
  const VectorXd dual_solution = VectorXd::Zero(4);

  const auto [primal_dual_norm, use_diagonal_qp_solver] = GetParam();

  const double primal_distance_squared_to_optimal =
      0.5 * (1.0 + 8.0 * 8.0 + 1.0 + 0.5 * 0.5);
  const double dual_distance_squared_to_optimal =
      0.5 * (4.0 + 2.375 * 2.375 + 4.0 / 9.0);
  const double distance_to_optimal =
      primal_dual_norm == PrimalDualNorm::kEuclideanNorm
          ? std::sqrt(primal_distance_squared_to_optimal +
                      dual_distance_squared_to_optimal)
          : std::sqrt(std::max(primal_distance_squared_to_optimal,
                               dual_distance_squared_to_optimal));

  LocalizedLagrangianBounds bounds = ComputeLocalizedLagrangianBounds(
      lp, primal_solution, dual_solution, primal_dual_norm,
      /*primal_weight=*/1.0,
      /*radius=*/distance_to_optimal,
      /*primal_product=*/nullptr,
      /*dual_product=*/nullptr, use_diagonal_qp_solver,
      /*diagonal_qp_trust_region_solver_tolerance=*/1.0e-6);

  EXPECT_DOUBLE_EQ(bounds.lagrangian_value, 3.0);
  EXPECT_LE(bounds.lower_bound, -20.0);
  EXPECT_GE(bounds.upper_bound, -20.0);
}

// When the radius is too small, the optimal value will not be contained in
// the computed interval.
TEST_P(ComputeLocalizedLagrangianBoundsTest, OptimalNotInBoundRange) {
  ShardedQuadraticProgram lp(TestLp(), /*num_threads=*/2, /*num_shards=*/2);

  // x_3 has a lower bound of 2.5.
  const VectorXd primal_solution{{0.0, 0.0, 0.0, 3.0}};
  const VectorXd dual_solution = VectorXd::Zero(4);

  const auto [primal_dual_norm, use_diagonal_qp_solver] = GetParam();

  LocalizedLagrangianBounds bounds = ComputeLocalizedLagrangianBounds(
      lp, primal_solution, dual_solution, primal_dual_norm,
      /*primal_weight=*/1.0,
      /*radius=*/0.1,
      /*primal_product=*/nullptr,
      /*dual_product=*/nullptr, use_diagonal_qp_solver,
      /*diagonal_qp_trust_region_solver_tolerance=*/1.0e-6);
  const double expected_lagrangian = 3.0;
  EXPECT_DOUBLE_EQ(bounds.lagrangian_value, expected_lagrangian);

  // Because the dual solution is all zero, the primal gradient is just the
  // objective, [5.5, -2, -1, 1]. The dual gradient is the dual subgradient
  // coefficient minus the primal product. With a zero dual, for one-sided
  // constraints, the dual subgradient coefficient is the bound, and for
  // two-sided constraints it is the violated bound (or zero if feasible). Thus,
  // the dual subgradient coefficients are [12, 7, -4, -1], and the primal
  // product is [6, 0, 0, -3], giving a dual gradient of [6, 7, -4, 2].

  switch (primal_dual_norm) {
    case PrimalDualNorm::kMaxNorm:
      // The target radius r = sqrt(2) * 0.1 ≈ 0.14, and the projected primal
      // direction is d=[-5.5, 2, 1, -1]. The resulting delta is d / ||d|| * r,
      // giving an objective delta of ||d|| * r.
      EXPECT_NEAR(bounds.lower_bound,
                  expected_lagrangian - 0.1 * sqrt(2) * sqrt(36.25), 1.0e-6);
      // The target radius r = sqrt(2) * 0.1 ≈ 0.14, and the projected dual
      // direction is d=[6, 0, 0, 2]. The resulting delta is d / ||d|| * r,
      // giving an objective delta of ||d|| * r.
      EXPECT_NEAR(bounds.upper_bound,
                  expected_lagrangian + 0.1 * sqrt(2) * sqrt(40.0), 1.0e-6);
      break;
    case PrimalDualNorm::kEuclideanNorm:
      // In this case, r = target_radius * sqrt(2) (because the euclidean norm
      // includes a factor of 0.5). The projected combined direction is d=[-5.5,
      // 2, 1, -1; 6, 0, 0, 2]. The resulting primal delta is d[primal] / ||d||
      // * r, and the resulting dual delta is d[dual] / ||d|| * r.
      EXPECT_NEAR(bounds.lower_bound,
                  expected_lagrangian - 0.1 * sqrt(2) * 36.25 / sqrt(76.25),
                  1.0e-6);
      EXPECT_NEAR(bounds.upper_bound,
                  expected_lagrangian + 0.1 * sqrt(2) * 40 / sqrt(76.25),
                  1.0e-6);
      break;
  }
}

// `kEuclideanNorm` isn't covered by this test because the analysis of the
// correct solution is more complex.
TEST(ComputeLocalizedLagrangianBoundsTest, ProcessesPrimalWeight) {
  ShardedQuadraticProgram lp(TestLp(), /*num_threads=*/2, /*num_shards=*/2);

  // x_3 has a lower bound of 2.5.
  const VectorXd primal_solution{{0.0, 0.0, 0.0, 3.0}};
  const VectorXd dual_solution = VectorXd::Zero(4);

  LocalizedLagrangianBounds bounds = ComputeLocalizedLagrangianBounds(
      lp, primal_solution, dual_solution, PrimalDualNorm::kMaxNorm,
      /*primal_weight=*/100.0,
      /*radius=*/0.1,
      /*primal_product=*/nullptr,
      /*dual_product=*/nullptr,
      /*use_diagonal_qp_trust_region_solver=*/false,
      /*diagonal_qp_trust_region_solver_tolerance=*/0.0);
  const double expected_lagrangian = 3.0;
  EXPECT_DOUBLE_EQ(bounds.lagrangian_value, expected_lagrangian);

  // Compared with `OptimalNotInBoundRange`, a primal weight of 100.0 translates
  // to a 10x smaller radius in the primal and 10x larger radius in the dual.
  EXPECT_LE(bounds.lower_bound, expected_lagrangian - 0.028);
  EXPECT_GE(bounds.lower_bound, expected_lagrangian - 0.28);
  EXPECT_GE(bounds.upper_bound, expected_lagrangian + 2.8);
  EXPECT_LE(bounds.upper_bound, expected_lagrangian + 28);
}

// Same as `OptimalInBoundRange` but providing `primal_product` and
// `dual_product`.
TEST_P(ComputeLocalizedLagrangianBoundsTest, AcceptsCachedProducts) {
  ShardedQuadraticProgram lp(TestLp(), /*num_threads=*/2, /*num_shards=*/2);

  // x_3 has a lower bound of 2.5.
  const VectorXd primal_solution{{0.0, 0.0, 0.0, 3.0}};
  const VectorXd dual_solution = VectorXd::Zero(4);

  const VectorXd primal_product{{6.0, 0.0, 0.0, -3.0}};
  const VectorXd dual_product = VectorXd::Zero(4);

  const auto [primal_dual_norm, use_diagonal_qp_solver] = GetParam();

  const double primal_distance_squared_to_optimal =
      0.5 * (1.0 + 8.0 * 8.0 + 1.0 + 0.5 * 0.5);
  const double dual_distance_squared_to_optimal =
      0.5 * (4.0 + 2.375 * 2.375 + 4.0 / 9.0);
  const double distance_to_optimal =
      primal_dual_norm == PrimalDualNorm::kEuclideanNorm
          ? std::sqrt(primal_distance_squared_to_optimal +
                      dual_distance_squared_to_optimal)
          : std::sqrt(std::max(primal_distance_squared_to_optimal,
                               dual_distance_squared_to_optimal));

  LocalizedLagrangianBounds bounds = ComputeLocalizedLagrangianBounds(
      lp, primal_solution, dual_solution, primal_dual_norm,
      /*primal_weight=*/1.0,
      /*radius=*/distance_to_optimal,
      /*primal_product=*/&primal_product,
      /*dual_product=*/&dual_product, use_diagonal_qp_solver,
      /*diagonal_qp_trust_region_solver_tolerance=*/1.0e-6);

  EXPECT_DOUBLE_EQ(bounds.lagrangian_value, 3.0);
  EXPECT_LE(bounds.lower_bound, -20.0);
  EXPECT_GE(bounds.upper_bound, -20.0);
}

// The LP:
// minimize 1.0 x
// s.t. 0 <= x <= 1 (as a constraint, not variable bound).
QuadraticProgram OneDimLp() {
  QuadraticProgram lp(1, 1);
  lp.constraint_lower_bounds = VectorXd{{0}};
  lp.constraint_upper_bounds = VectorXd{{1}};
  lp.variable_lower_bounds = VectorXd{{-kInfinity}};
  lp.variable_upper_bounds = VectorXd{{kInfinity}};
  std::vector<Eigen::Triplet<double, int64_t>> triplets = {{0, 0, 1}};
  lp.constraint_matrix.setFromTriplets(triplets.begin(), triplets.end());
  lp.objective_vector = VectorXd{{1.0}};
  return lp;
}

// The QP:
// minimize 1.0 x + 1.0 * x^2
// s.t. 0 <= x <= 1 (as a constraint, not variable bound).
QuadraticProgram OneDimQp() {
  QuadraticProgram qp(1, 1);
  qp.constraint_lower_bounds = VectorXd{{0}};
  qp.constraint_upper_bounds = VectorXd{{1}};
  qp.variable_lower_bounds = VectorXd{{-kInfinity}};
  qp.variable_upper_bounds = VectorXd{{kInfinity}};
  std::vector<Eigen::Triplet<double, int64_t>> constraint_matrix_triplets = {
      {0, 0, 1}};
  qp.constraint_matrix.setFromTriplets(constraint_matrix_triplets.begin(),
                                       constraint_matrix_triplets.end());
  qp.objective_matrix.emplace();
  qp.objective_matrix->resize(1);
  qp.objective_matrix->diagonal() = VectorXd{{2}};
  qp.objective_vector = VectorXd{{1}};
  return qp;
}

// Helper functions to compute the primal and dual gradient at a given point.
VectorXd GetPrimalGradient(const ShardedQuadraticProgram& sharded_qp,
                           const VectorXd& primal_solution,
                           const VectorXd& dual_solution) {
  const auto dual_product = TransposedMatrixVectorProduct(
      sharded_qp.Qp().constraint_matrix, dual_solution,
      sharded_qp.ConstraintMatrixSharder());
  return ComputePrimalGradient(sharded_qp, primal_solution, dual_product)
      .gradient;
}

VectorXd GetDualGradient(const ShardedQuadraticProgram& sharded_qp,
                         const VectorXd& primal_solution,
                         const VectorXd& dual_solution) {
  const auto primal_product = TransposedMatrixVectorProduct(
      sharded_qp.TransposedConstraintMatrix(), primal_solution,
      sharded_qp.TransposedConstraintMatrixSharder());
  return ComputeDualGradient(sharded_qp, dual_solution, primal_product)
      .gradient;
}

struct TestProblemData {
  VectorXd objective_vector;
  VectorXd objective_matrix_diagonal;
  VectorXd center_point;
  VectorXd variable_lower_bounds;
  VectorXd variable_upper_bounds;
  VectorXd norm_weights;
};

// Generates the problem data corresponding to `OneDimLp()` as raw vectors with
// center point [x, y] = [0, -1].
TestProblemData GenerateTestLpProblemData(const double primal_weight) {
  return {
      .objective_vector = VectorXd{{2, -1}},
      .objective_matrix_diagonal = VectorXd::Zero(2),
      .center_point = VectorXd{{0, -1}},
      .variable_lower_bounds = VectorXd{{-kInfinity, -kInfinity}},
      .variable_upper_bounds = VectorXd{{kInfinity, kInfinity}},
      .norm_weights = VectorXd{{0.5 * primal_weight, 0.5 / primal_weight}},
  };
}

// Generates the problem data corresponding to `OneDimQp()` as raw vectors with
// center point [x, y] = [0, -1].
TestProblemData GenerateTestQpProblemData(const double primal_weight) {
  TestProblemData lp_data = GenerateTestLpProblemData(primal_weight);
  lp_data.objective_matrix_diagonal[0] = 2.0;
  return lp_data;
}

// This is a tiny problem where we can compute the exact solution, checking
// that `kMaxNorm` and `kEuclideanNorm` give different answers.
TEST_P(ComputeLocalizedLagrangianBoundsTest, NormsBehaveDifferently) {
  ShardedQuadraticProgram lp(OneDimLp(), /*num_threads=*/2, /*num_shards=*/2);

  const VectorXd primal_solution = VectorXd::Zero(1);
  const VectorXd dual_solution{{-1}};  // The upper bound is active.

  // The primal gradient is [2], and the dual gradient is [1]. Hence, the norm
  // of the gradient is sqrt(5).

  const auto [primal_dual_norm, use_diagonal_qp_solver] = GetParam();

  LocalizedLagrangianBounds bounds = ComputeLocalizedLagrangianBounds(
      lp, primal_solution, dual_solution, primal_dual_norm,
      /*primal_weight=*/1.0, /*radius=*/1.0 / std::sqrt(2.0),
      /*primal_product=*/nullptr,
      /*dual_product=*/nullptr, use_diagonal_qp_solver,
      /*diagonal_qp_trust_region_solver_tolerance=*/1.0e-6);
  const double expected_lagrangian = -1;
  EXPECT_DOUBLE_EQ(bounds.lagrangian_value, expected_lagrangian);

  switch (primal_dual_norm) {
    case PrimalDualNorm::kMaxNorm:
      EXPECT_DOUBLE_EQ(bounds.lower_bound, expected_lagrangian - 2.0);
      EXPECT_DOUBLE_EQ(bounds.upper_bound, expected_lagrangian + 1.0);
      break;
    case PrimalDualNorm::kEuclideanNorm:
      if (use_diagonal_qp_solver) {
        EXPECT_NEAR(bounds.lower_bound,
                    expected_lagrangian - 4.0 / std::sqrt(5), 1.0e-6);
        EXPECT_NEAR(bounds.upper_bound,
                    expected_lagrangian + 1.0 / std::sqrt(5), 1.0e-6);
      } else {
        EXPECT_DOUBLE_EQ(bounds.lower_bound,
                         expected_lagrangian - 4.0 / std::sqrt(5));
        EXPECT_DOUBLE_EQ(bounds.upper_bound,
                         expected_lagrangian + 1.0 / std::sqrt(5));
      }
      break;
  }
}

// Like `NormsBehaveDifferently` but with a larger primal weight.
TEST_P(ComputeLocalizedLagrangianBoundsTest,
       NormsBehaveDifferentlyWithLargePrimalWeight) {
  ShardedQuadraticProgram lp(OneDimLp(), /*num_threads=*/2, /*num_shards=*/2);

  const VectorXd primal_solution = VectorXd::Zero(1);
  const VectorXd dual_solution{{-1}};  // The upper bound is active.

  // The primal gradient is [2], and the dual gradient is [1].

  const auto [primal_dual_norm, use_diagonal_qp_solver] = GetParam();

  LocalizedLagrangianBounds bounds = ComputeLocalizedLagrangianBounds(
      lp, primal_solution, dual_solution, primal_dual_norm,
      /*primal_weight=*/100.0, /*radius=*/1.0 / std::sqrt(2.0),
      /*primal_product=*/nullptr,
      /*dual_product=*/nullptr, use_diagonal_qp_solver,
      /*diagonal_qp_trust_region_solver_tolerance=*/1.0e-8);
  const double expected_lagrangian = -1;
  EXPECT_DOUBLE_EQ(bounds.lagrangian_value, expected_lagrangian);

  switch (primal_dual_norm) {
    case PrimalDualNorm::kMaxNorm:
      EXPECT_DOUBLE_EQ(bounds.lower_bound, expected_lagrangian - 0.2);
      EXPECT_DOUBLE_EQ(bounds.upper_bound, expected_lagrangian + 10.0);
      break;
    case PrimalDualNorm::kEuclideanNorm:
      // Given c = [2.0, -1], w = [100.0, 0.01], this value is
      // dot(c, (c ./ w) / norm(c ./ sqrt.(w))) (in Julia syntax).
      if (use_diagonal_qp_solver) {
        EXPECT_NEAR(bounds.upper_bound - bounds.lower_bound, 10.00199980003999,
                    10.002 * 1.0e-8);
      } else {
        EXPECT_DOUBLE_EQ(bounds.upper_bound - bounds.lower_bound,
                         10.00199980003999);
      }
      break;
  }
}

TEST(DiagonalTrustRegionSolverTest, JointSolverWorksWithOneDimQpUnitWeight) {
  ShardedQuadraticProgram sharded_qp(OneDimQp(), /*num_threads=*/2,
                                     /*num_shards=*/2);
  const auto problem_data = GenerateTestQpProblemData(/*primal_weight=*/1.0);
  TrustRegionResult result = SolveDiagonalTrustRegion(
      problem_data.objective_vector, problem_data.objective_matrix_diagonal,
      problem_data.variable_lower_bounds, problem_data.variable_upper_bounds,
      problem_data.center_point, problem_data.norm_weights,
      /*target_radius=*/0.5,
      Sharder(/*num_elements=*/2, /*num_shards=*/2, nullptr),
      /*solve_tolerance=*/1.0e-6);
  EXPECT_THAT(result.solution,
              ElementsAre(DoubleNear(-0.5, 1.0e-6), DoubleNear(-0.5, 1.0e-6)));
  EXPECT_NEAR(result.solution_step_size, 4.0, 4.0 * 1.0e-6);
  EXPECT_NEAR(result.objective_value, -1.25, 1.0e-6);
}

TEST(DiagonalTrustRegionSolverTest,
     DiagonalQpSolverWorksWithOneDimQpUnitWeight) {
  ShardedQuadraticProgram sharded_qp(OneDimQp(), /*num_threads=*/2,
                                     /*num_shards=*/2);
  VectorXd primal_solution = VectorXd::Zero(1);
  VectorXd dual_solution = -1.0 * VectorXd::Ones(1);
  VectorXd primal_gradient =
      GetPrimalGradient(sharded_qp, primal_solution, dual_solution);
  VectorXd dual_gradient =
      GetDualGradient(sharded_qp, primal_solution, dual_solution);
  TrustRegionResult result = SolveDiagonalQpTrustRegion(
      sharded_qp, primal_solution, dual_solution, primal_gradient,
      dual_gradient, /*primal_weight=*/1.0, /*target_radius=*/0.5,
      /*solve_tolerance=*/1.0e-6);
  EXPECT_THAT(result.solution,
              ElementsAre(DoubleNear(-0.5, 1.0e-6), DoubleNear(-0.5, 1.0e-6)));
  EXPECT_NEAR(result.solution_step_size, 4.0, 4.0 * 1.0e-6);
  EXPECT_NEAR(result.objective_value, -1.25, 1.0e-6);
}

TEST(DiagonalTrustRegionSolverTest, JointSolverWorksWithOneDimQpLargeWeight) {
  ShardedQuadraticProgram sharded_qp(OneDimQp(), /*num_threads=*/2,
                                     /*num_shards=*/2);
  const auto problem_data = GenerateTestQpProblemData(/*primal_weight=*/100.0);
  TrustRegionResult result = SolveDiagonalTrustRegion(
      problem_data.objective_vector, problem_data.objective_matrix_diagonal,
      problem_data.variable_lower_bounds, problem_data.variable_upper_bounds,
      problem_data.center_point, problem_data.norm_weights,
      /*target_radius=*/std::sqrt(2705.0 / 2) * (5.0 / 13),
      Sharder(/*num_elements=*/2, /*num_shards=*/2, nullptr),
      /*solve_tolerance=*/1.0e-6);
  EXPECT_NEAR(result.solution_step_size, 1.0, 1.0e-6);
}

TEST(DiagonalTrustRegionSolverTest,
     DiagonalQpSolverWorksWithOneDimQpLargeWeight) {
  ShardedQuadraticProgram sharded_qp(OneDimQp(), /*num_threads=*/2,
                                     /*num_shards=*/2);
  VectorXd primal_solution = VectorXd::Zero(1);
  VectorXd dual_solution = -1.0 * VectorXd::Ones(1);
  VectorXd primal_gradient =
      GetPrimalGradient(sharded_qp, primal_solution, dual_solution);
  VectorXd dual_gradient =
      GetDualGradient(sharded_qp, primal_solution, dual_solution);
  TrustRegionResult result = SolveDiagonalQpTrustRegion(
      sharded_qp, primal_solution, dual_solution, primal_gradient,
      dual_gradient, /*primal_weight=*/100.0,
      /*target_radius=*/std::sqrt(2705.0 / 2.0) * (5.0 / 13),
      /*solve_tolerance=*/1.0e-6);
  EXPECT_NEAR(result.solution_step_size, 1.0, 1.0e-6);
}

TEST(DiagonalTrustRegionSolverTest, JointSolverWorksWithOneDimQpSmallWeight) {
  ShardedQuadraticProgram sharded_qp(OneDimQp(), /*num_threads=*/2,
                                     /*num_shards=*/2);
  const auto problem_data = GenerateTestQpProblemData(/*primal_weight=*/0.01);
  TrustRegionResult result = SolveDiagonalTrustRegion(
      problem_data.objective_vector, problem_data.objective_matrix_diagonal,
      problem_data.variable_lower_bounds, problem_data.variable_upper_bounds,
      problem_data.center_point, problem_data.norm_weights,
      /*target_radius=*/0.71063,
      Sharder(/*num_elements=*/2, /*num_shards=*/2, nullptr),
      /*solve_tolerance=*/1.0e-6);
  EXPECT_THAT(result.solution, ElementsAre(DoubleNear(-0.99950025, 1.0e-6),
                                           DoubleNear(-0.9, 1.0e-6)));
  EXPECT_NEAR(result.solution_step_size, 0.2, 1.0e-6);
  EXPECT_NEAR(result.objective_value, -1.0999996, 1.0e-6);
}

TEST(DiagonalTrustRegionSolverTest,
     DiagonalQpSolverWorksWithOneDimQpSmallWeight) {
  ShardedQuadraticProgram sharded_qp(OneDimQp(), /*num_threads=*/2,
                                     /*num_shards=*/2);
  VectorXd primal_solution = VectorXd::Zero(1);
  VectorXd dual_solution = -1.0 * VectorXd::Ones(1);
  VectorXd primal_gradient =
      GetPrimalGradient(sharded_qp, primal_solution, dual_solution);
  VectorXd dual_gradient =
      GetDualGradient(sharded_qp, primal_solution, dual_solution);
  TrustRegionResult result = SolveDiagonalQpTrustRegion(
      sharded_qp, primal_solution, dual_solution, primal_gradient,
      dual_gradient, /*primal_weight=*/0.01,
      /*target_radius=*/0.71063,
      /*solve_tolerance=*/1.0e-6);
  EXPECT_THAT(result.solution, ElementsAre(DoubleNear(-0.99950025, 1.0e-6),
                                           DoubleNear(-0.9, 1.0e-6)));
  EXPECT_NEAR(result.solution_step_size, 0.2, 1.0e-6);
  EXPECT_NEAR(result.objective_value, -1.0999996, 1.0e-6);
}

// This is a tiny QP where we can compute the exact solution.
TEST(ComputeLocalizedLagrangianBoundsTest, SolvesForTestQpUnitWeight) {
  ShardedQuadraticProgram qp(OneDimQp(), /*num_threads=*/2, /*num_shards=*/2);

  const VectorXd primal_solution = VectorXd::Zero(1);
  const VectorXd dual_solution{{-1}};  // The upper bound is active.

  // The primal gradient is [2], and the dual gradient is [1]. Hence, the norm
  // of the gradient is sqrt(5).

  LocalizedLagrangianBounds bounds = ComputeLocalizedLagrangianBounds(
      qp, primal_solution, dual_solution, PrimalDualNorm::kEuclideanNorm,
      /*primal_weight=*/1.0, /*radius=*/0.5,
      /*primal_product=*/nullptr,
      /*dual_product=*/nullptr, /*use_diagonal_qp_trust_region_solver=*/true,
      /*diagonal_qp_trust_region_solver_tolerance=*/1.0e-6);
  const double expected_lagrangian = -1;
  EXPECT_DOUBLE_EQ(bounds.lagrangian_value, expected_lagrangian);
  EXPECT_NEAR(bounds.upper_bound, expected_lagrangian + 0.5, 1.0e-5);
  EXPECT_NEAR(bounds.lower_bound, expected_lagrangian - 0.75, 1.0e-5);
}

}  // namespace
}  // namespace operations_research::pdlp
