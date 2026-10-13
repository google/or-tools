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

#include "ortools/glop/lp_solver.h"

#include <atomic>
#include <cmath>
#include <limits>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/cleanup/cleanup.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/flags.h"
#include "absl/log/log.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/optional_ref.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/log_severity.h"
#include "ortools/glop/glop_testing.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_parser.h"
#include "ortools/lp_data/lp_test_utils.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/lp_types_testing.h"
#include "ortools/lp_data/lp_utils.h"
#include "ortools/lp_data/proto_utils.h"
#include "ortools/lp_data/sparse.h"
#include "ortools/port/scoped_std_stream_capture.h"
#include "ortools/util/file_util.h"
#include "ortools/util/fp_roundtrip_conv.h"
#include "ortools/util/fp_utils_testing.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace glop {
namespace {

using ::testing::_;
using ::testing::AnyOfArray;
using ::testing::DoubleNear;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::EqualsProto;
using ::testing::Ge;
using ::testing::IsNan;
using ::testing::Le;
using ::testing::Optional;

// When evaluating primal/dual ray, this tolerance is used to test that they
// have a significant contribution to the primal/dual objective.
constexpr Fractional kRayObjectiveContributionZeroTolerance = 1e-3;

// Returns a copy of a referenced optional value.
//
// This is used in unit test as there is no printer for
// absl::optional_ref<>. Without we get messages like:
//
//   8-byte object <A0-6E 9B-27 FC-7F 00-00>
//
template <typename T>
std::optional<T> OptionalRefValue(const absl::optional_ref<T>& opt_ref) {
  return opt_ref.has_value() ? std::make_optional(T(*opt_ref)) : std::nullopt;
}

// Note that double is used instead of Fractional to be able to use C-style
// const inline arrays.
template <int NumCols, int NumRows>
struct ExpectedSolution {
  ProblemStatus status;
  double objective_value;
  double solution_value[NumCols > 0 ? NumCols : 1];
  double reduced_cost[NumCols > 0 ? NumCols : 1];
  double dual_value[NumRows > 0 ? NumRows : 1];
  double activity[NumRows > 0 ? NumRows : 1];
  VariableStatus variable_status[NumCols > 0 ? NumCols : 1];
  ConstraintStatus constraint_status[NumRows > 0 ? NumRows : 1];
};

template <int NumCols, int NumRows>
void CheckLPSolver(
    absl::string_view test_problem,
    const ExpectedSolution<NumCols, NumRows>& expected_solution) {
  GlopParameters parameters;
  parameters.set_provide_strong_optimal_guarantee(true);
  CheckLPSolverWithParameters(test_problem, expected_solution, parameters);
  parameters.set_provide_strong_optimal_guarantee(false);
  CheckLPSolverWithParameters(test_problem, expected_solution, parameters);
  parameters.set_num_omp_threads(8);
  CheckLPSolverWithParameters(test_problem, expected_solution, parameters);
}

template <int NumCols, int NumRows>
void CheckLPSolverWithParameters(
    absl::string_view test_problem,
    const ExpectedSolution<NumCols, NumRows>& expected_solution,
    const GlopParameters& parameters) {
  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(test_problem, &linear_program));
  LPSolver lp_solver;
  lp_solver.SetParameters(parameters);

  const SolveStatus status = lp_solver.Solve(linear_program);
  EXPECT_THAT(status, SolveStatusProblemStatusIs(expected_solution.status));
  EXPECT_THAT(lp_solver.GetObjectiveValue(),
              WithinSameAbsoluteOrRelativeTolerance(
                  expected_solution.objective_value, kComparableEpsilon));

  const DenseRow& values = lp_solver.variable_values();
  EXPECT_THAT(values, FractionalVectorComparable(DenseRow(
                          expected_solution.solution_value,
                          expected_solution.solution_value + NumCols)));

  const DenseRow& reduced_costs = lp_solver.reduced_costs();
  EXPECT_THAT(reduced_costs, FractionalVectorComparable(DenseRow(
                                 expected_solution.reduced_cost,
                                 expected_solution.reduced_cost + NumCols)));

  const DenseColumn& dual_values = lp_solver.dual_values();
  EXPECT_THAT(dual_values, FractionalVectorComparable(DenseColumn(
                               expected_solution.dual_value,
                               expected_solution.dual_value + NumRows)));

  DenseColumn activities;
  for (RowIndex ct_id(0); ct_id < NumRows; ++ct_id) {
    const Fractional activity = lp_solver.constraint_activities()[ct_id];
    activities.push_back(activity);
  }
  EXPECT_THAT(activities, FractionalVectorComparable(DenseColumn(
                              expected_solution.activity,
                              expected_solution.activity + NumRows)));

  // Check statuses.
  EXPECT_THAT(lp_solver.variable_statuses(),
              ElementsAreArray(expected_solution.variable_status, NumCols));
  EXPECT_THAT(lp_solver.constraint_statuses(),
              ElementsAreArray(expected_solution.constraint_status, NumRows));
}

// Tests LP solver returns ProblemStatus::OPTIMAL when the problem is empty.
TEST(LPSolverTest, Empty) {
  const int kNumCols = 0;
  const int kNumRows = 0;
  const std::string kLinearProgram = "max: 0";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedSolution = {
      ProblemStatus::OPTIMAL,  // Status
      0,                       // Objective value
      {},                      // Solution values
      {},                      // Reduced costs
      {},                      // Dual values
      {},                      // Constraint activity
      {},                      // Variable status
      {}                       // Constraint status
  };

  CheckLPSolver(kLinearProgram, kExpectedSolution);
}

// Tests LP solver returns ProblemStatus::OPTIMAL when the problem is almost
// empty. Tests that we can query the values.
TEST(LPSolverTest, NoConstraints) {
  const ColIndex kNumCols(10);
  LinearProgram linear_program;
  for (int i = 0; i < kNumCols; ++i) {
    linear_program.CreateNewVariable();
  }
  LPSolver solver;
  EXPECT_THAT(solver.Solve(linear_program),
              SolveStatusWith<SolveStatus::Optimal>(_));
  for (ColIndex col(0); col < kNumCols; ++col) {
    EXPECT_EQ(0.0, solver.variable_values()[col]);
    EXPECT_EQ(0.0, solver.reduced_costs()[col]);

    // The default variable range is [0, infinity).
    EXPECT_EQ(VariableStatus::AT_LOWER_BOUND, solver.variable_statuses()[col]);
  }
  EXPECT_THAT(solver.variable_values(),
              testing::ContainerEq(DenseRow{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}));
}

// Tests that the LP Solver returns ProblemStatus::OPTIMAL when the problem has
// no columns, the left-hand sides of constraints are <= 0 and the right-hand
// sides are >= 0.
// -1 <= 0 <= 2
// -8 <= 0 <= 0  // "Edge" case, still feasible.
TEST(LPSolverTest, NoColumnsOptimal) {
  LinearProgram linear_program;
  linear_program.SetConstraintBounds(RowIndex(0), -1, 2);
  linear_program.SetConstraintBounds(RowIndex(1), -8, 0);
  LPSolver lp_solver;
  EXPECT_THAT(lp_solver.Solve(linear_program),
              SolveStatusWith<SolveStatus::Optimal>(_));
  const RowIndex num_rows = linear_program.num_constraints();
  for (RowIndex row(0); row < num_rows; ++row) {
    EXPECT_EQ(0.0, lp_solver.constraint_activities()[row]);
    EXPECT_EQ(ConstraintStatus::BASIC, lp_solver.constraint_statuses()[row]);
    EXPECT_EQ(0.0, lp_solver.dual_values()[row]);
  }
}

// Tests that the LP Solver returns ProblemStatus::PRIMAL_INFEASIBLE when the
// problem has no columns, the left-hand sides of constraints are > 0 or the
// right-hand sides are < 0.
// -1 <= 0 <= 0
// -8 <= 0 <= -1    // Infeasibility comes from here.
TEST(LPSolverTest, NoColumnsInfeasible) {
  LinearProgram linear_program;
  linear_program.SetConstraintBounds(RowIndex(0), -1, 0);
  linear_program.SetConstraintBounds(RowIndex(1), -8, -1);
  LPSolver lp_solver;
  EXPECT_THAT(lp_solver.Solve(linear_program),
              SolveStatusWith<SolveStatus::PrimalInfeasible>(_));
  const RowIndex num_rows = linear_program.num_constraints();
  for (RowIndex row(0); row < num_rows; ++row) {
    EXPECT_EQ(0.0, lp_solver.constraint_activities()[row]);
    EXPECT_EQ(ConstraintStatus::FREE, lp_solver.constraint_statuses()[row]);
    EXPECT_EQ(0.0, lp_solver.dual_values()[row]);
  }
}

// Tests that the LP Solver returns ProblemStatus::PRIMAL_INFEASIBLE when the
// problem is infeasible. Tests that we can still query the values.
TEST(LPSolverTest, Infeasible) {
  const int kNumCols = 3;
  const int kNumRows = 2;
  const std::string kLinearProgram =
      "min: 0 + x;"
      "0 <= x <= inf;"
      "r1 <= 100;"
      "r2 >= 200;"
      "x - r1 = 0;"
      "x - r2 = 0;";
  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(kLinearProgram, &linear_program));
  LPSolver lp_solver;
  EXPECT_THAT(lp_solver.Solve(linear_program),
              SolveStatusWith<SolveStatus::PrimalInfeasible>(_));

  for (ColIndex col(0); col < kNumCols; ++col) {
    lp_solver.reduced_costs()[col];
    lp_solver.variable_values()[col];
    lp_solver.variable_statuses()[col];
  }

  for (RowIndex row(0); row < kNumRows; ++row) {
    lp_solver.constraint_activities()[row];
    lp_solver.constraint_statuses()[row];
    lp_solver.dual_values()[row];
  }
}

TEST(LPSolverTest, SingularMatrix) {
  const int kNumCols = 4;
  const int kNumRows = 2;
  const std::string kLinearProgram =
      "max: 0 + 40 x + 70 y;"
      "0 <= x <= 100;"
      "0 <= y <= 100;"
      "0 <= r1 <= 100;"
      "0 <= r2 <= 4000;"
      "   x +  1 y - r1 = 0;"
      "10 x + 50 y - r2 = 0;";
  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(kLinearProgram, &linear_program));
  LPSolver lp_solver;

  // This will cause the basis to be refactorized and the refactorization to
  // fail.
  GlopParameters parameters;
  parameters.set_basis_refactorization_period(0);
  parameters.set_markowitz_singularity_threshold(10);
  lp_solver.SetParameters(parameters);

  // If the solver encounters an error, the status should be
  // SolveStatus::Abnormal.
  EXPECT_THAT(lp_solver.Solve(linear_program),
              SolveStatusWith<SolveStatus::Abnormal>(_));

  // Check that we can still access all the functions of the LPSolver interface.
  for (ColIndex col(0); col < kNumCols; ++col) {
    lp_solver.reduced_costs()[col];
    lp_solver.variable_values()[col];
    lp_solver.variable_statuses()[col];
  }
  for (RowIndex row(0); row < kNumRows; ++row) {
    lp_solver.constraint_activities()[row];
    lp_solver.constraint_statuses()[row];
    lp_solver.dual_values()[row];
  }
}

// Note(user): There are actually many optimal solutions to the problem below!
// One with x=8, t=2 and the other with x=4, t=6. In order to have a unique
// solution, the objective has been perturbed by 1e-8 for x and t.
TEST(LPSolverTest, Maximize) {
  const int kNumCols = 9;
  const int kNumRows = 4;
  const std::string kLinearProgram =
      "max: 9 + 1.00000001 x + y - z + 0.99999999 t;"
      "-5  <= x <= 10;"
      "-2  <= y <=  8;"
      "-3  <= z <=  7;"
      "-5  <= t <= 10;"
      "-10 <= u <=  5;"
      "-1000 <= r1 <= 1000;"
      "   -3 <= r2 <= 1000;"
      "    0 <= r3 <= 20;"
      "-1000 <= r4 <= 10;"
      " x +   t - r1 = 0;"
      " x +   t - r2 = 0;"
      "-x + 4 t - r3 = 0;"
      " x +   t - r4 = 0;";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedSolution = {
      ProblemStatus::OPTIMAL,           // Status
      30,                               // Objective value
      {8, 8, -3, 2, 5, 10, 10, 0, 10},  // Solution values
      {0, 1, -1, 0, 0, 0, 0, 0, 1},     // Reduced costs
      {0, 0, 0, 1},                     // Dual values
      {0, 0, 0, 0},                     // Constraint activity
      {VariableStatus::BASIC, VariableStatus::AT_UPPER_BOUND,
       VariableStatus::AT_LOWER_BOUND, VariableStatus::BASIC,
       VariableStatus::AT_UPPER_BOUND, VariableStatus::BASIC,
       VariableStatus::BASIC, VariableStatus::AT_LOWER_BOUND,
       VariableStatus::AT_UPPER_BOUND},  // Variable status
      {ConstraintStatus::FIXED_VALUE, ConstraintStatus::FIXED_VALUE,
       ConstraintStatus::FIXED_VALUE,
       ConstraintStatus::FIXED_VALUE}  // Constraint status
  };

  CheckLPSolver(kLinearProgram, kExpectedSolution);
}

TEST(LPSolverTest, MaximizeWithEmptyRow) {
  LinearProgram linear_program;
  ColIndex col_x = linear_program.FindOrCreateVariable("x");
  RowIndex row_r1 = linear_program.FindOrCreateConstraint("r1");
  RowIndex row_r2 = linear_program.FindOrCreateConstraint("r2");
  linear_program.SetVariableBounds(col_x, -20, 20);
  linear_program.SetObjectiveCoefficient(col_x, 2);
  linear_program.SetConstraintBounds(row_r1, 0, 20);
  linear_program.SetConstraintBounds(row_r2, 0, 20);
  linear_program.SetCoefficient(row_r1, col_x, 4);
  linear_program.SetMaximizationProblem(true);
  linear_program.CleanUp();

  LPSolver solver;
  EXPECT_THAT(solver.Solve(linear_program),
              SolveStatusWith<SolveStatus::Optimal>(_));
  EXPECT_THAT(solver.GetObjectiveValue(),
              WithinSameAbsoluteOrRelativeTolerance(10.0, kComparableEpsilon));
}

TEST(LPSolverTest, Minimize) {
  const int kNumCols = 9;
  const int kNumRows = 4;
  const std::string kLinearProgram =
      "min: 9 + x + y - z + t;"
      "-5  <= x <= 10;"
      "-2  <= y <=  8;"
      "-3  <= z <=  7;"
      "-5  <= t <= 10;"
      "-10 <= u <=  5;"
      "-1000 <= r1 <= 1000;"
      "   -3 <= r2 <= 1000;"
      "    0 <= r3 <= 20;"
      "-1000 <= r4 <= 10;"
      " x +   t - r1 = 0;"
      " x +   t - r2 = 0;"
      "-x + 4 t - r3 = 0;"
      " x +   t - r4 = 0;";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedSolution = {
      ProblemStatus::OPTIMAL,                 // Status
      -3,                                     // Objective value
      {-2.4, -2, 7, -0.6, 5, -3, -3, 0, -3},  // Solution values
      {0, 1, -1, 0, 0, 0, 1, 0, 0},           // Reduced costs
      {0, 1, 0, 0},                           // Dual values
      {0, 0, 0, 0},                           // Constraint activity
      {VariableStatus::BASIC, VariableStatus::AT_LOWER_BOUND,
       VariableStatus::AT_UPPER_BOUND, VariableStatus::BASIC,
       VariableStatus::AT_UPPER_BOUND, VariableStatus::BASIC,
       VariableStatus::AT_LOWER_BOUND, VariableStatus::AT_LOWER_BOUND,
       VariableStatus::BASIC},  // Variable status
      {ConstraintStatus::FIXED_VALUE, ConstraintStatus::FIXED_VALUE,
       ConstraintStatus::FIXED_VALUE,
       ConstraintStatus::FIXED_VALUE}  // Constraint status
  };

  CheckLPSolver(kLinearProgram, kExpectedSolution);
}

TEST(LPSolverTest, MaximizeEconomicProblem) {
  const int kNumCols = 4;
  const int kNumRows = 2;
  const std::string kLinearProgram =
      "max: 40 x + 70 y;"
      "0 <= x <= 100;"
      "0 <= y <= 100;"
      "0 <= r1 <= 100;"
      "0 <= r2 <= 4000;"
      "x + y - r1 = 0;"
      "10 x + 50 y - r2 = 0;";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedSolution = {
      ProblemStatus::OPTIMAL,  // Status
      6250,                    // Objective value
      {25, 75, 100, 4000},     // Solution values
      {0, 0, 32.5, 0.75},      // Reduced costs
      {32.5, 0.75},            // Dual values
      {0, 0},                  // Constraint activity
      {VariableStatus::BASIC, VariableStatus::BASIC,
       VariableStatus::AT_UPPER_BOUND,
       VariableStatus::AT_UPPER_BOUND},  // Variable status
      {ConstraintStatus::FIXED_VALUE,
       ConstraintStatus::FIXED_VALUE}  // Constraint status
  };

  CheckLPSolver(kLinearProgram, kExpectedSolution);
}

// Same as MaximizeEconomicProblem above,
// but stop after the 1 iteration : FEASIBLE problem.
TEST(LPSolverTest, FeasibleStatus) {
  const int kNumCols = 4;
  const int kNumRows = 2;
  const std::string kLinearProgram =
      "max: 40 x + 70 y;"
      "0 <= x <= 100;"
      "0 <= y <= 100;"
      "0 <= r1 <= 100;"
      "0 <= r2 <= 4000;"
      "x + y - r1 = 0;"
      "10 x + 50 y - r2 = 0;";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedSolution = {
      ProblemStatus::PRIMAL_FEASIBLE,  // Status
      0,                               // Objective value
      {0, 0, 0, 0},                    // Solution values
      {40, 70, 0, 0},                  // Reduced costs
      {0, 0},                          // Dual values
      {0, 0},                          // Constraint activity
      {VariableStatus::AT_LOWER_BOUND, VariableStatus::AT_LOWER_BOUND,
       VariableStatus::AT_LOWER_BOUND, VariableStatus::AT_LOWER_BOUND},
      {ConstraintStatus::BASIC, ConstraintStatus::BASIC}  // Constraint status
  };

  GlopParameters parameters;
  parameters.set_max_number_of_iterations(0);

  // This is needed so the initial basis is the all slack basis.
  parameters.set_initial_basis(GlopParameters::NONE);
  CheckLPSolverWithParameters(kLinearProgram, kExpectedSolution, parameters);
}

// Same problem as MaximizeEconomicProblem but do not formulate it with slack.
TEST(LPSolverTest, MaximizeEconomicProblemNoSlack) {
  LinearProgram linear_program;
  ColIndex col_x = linear_program.FindOrCreateVariable("x");
  ColIndex col_y = linear_program.FindOrCreateVariable("y");
  linear_program.SetVariableBounds(col_x, 0, 100);
  linear_program.SetVariableBounds(col_y, 0, 100);
  linear_program.SetObjectiveCoefficient(col_x, 40);
  linear_program.SetObjectiveCoefficient(col_y, 70);
  RowIndex row_r1 = linear_program.FindOrCreateConstraint("r1");
  RowIndex row_r2 = linear_program.FindOrCreateConstraint("r2");
  linear_program.SetConstraintBounds(row_r1, 0, 100);
  linear_program.SetConstraintBounds(row_r2, 0, 4000);
  linear_program.SetMaximizationProblem(true);
  linear_program.SetCoefficient(row_r1, col_x, 1.0);
  linear_program.SetCoefficient(row_r1, col_y, 1.0);
  linear_program.SetCoefficient(row_r2, col_x, 10.0);
  linear_program.SetCoefficient(row_r2, col_y, 50.0);
  linear_program.CleanUp();

  LPSolver solver;
  EXPECT_THAT(solver.Solve(linear_program),
              SolveStatusWith<SolveStatus::Optimal>(_));
  EXPECT_THAT(solver.GetObjectiveValue(), WithinSameAbsoluteOrRelativeTolerance(
                                              6250.0, kComparableEpsilon));

  EXPECT_THAT(solver.variable_values()[col_x],
              WithinSameAbsoluteOrRelativeTolerance(25.0, kComparableEpsilon));
  EXPECT_THAT(solver.variable_values()[col_y],
              WithinSameAbsoluteOrRelativeTolerance(75.0, kComparableEpsilon));
  EXPECT_THAT(solver.dual_values()[row_r1],
              WithinSameAbsoluteOrRelativeTolerance(32.5, kComparableEpsilon));
  EXPECT_THAT(solver.dual_values()[row_r2],
              WithinSameAbsoluteOrRelativeTolerance(0.75, kComparableEpsilon));
  EXPECT_EQ(VariableStatus::BASIC, solver.variable_statuses()[col_x]);
  EXPECT_EQ(VariableStatus::BASIC, solver.variable_statuses()[col_y]);
  EXPECT_THAT(solver.constraint_activities()[row_r1],
              WithinSameAbsoluteOrRelativeTolerance(100.0, kComparableEpsilon));
  EXPECT_THAT(
      solver.constraint_activities()[row_r2],
      WithinSameAbsoluteOrRelativeTolerance(4000.0, kComparableEpsilon));

  EXPECT_EQ(ConstraintStatus::AT_UPPER_BOUND,
            solver.constraint_statuses()[row_r1]);
  EXPECT_EQ(ConstraintStatus::AT_UPPER_BOUND,
            solver.constraint_statuses()[row_r2]);
}

// Same problem as MaximizeEconomicProblem but do not formulate it with slack
// and remove >= 0 on the constraints. Also revert the sign of the second
// constraint so it is ConstraintStatus::AT_LOWER_BOUND and play with the
// variable bounds.
TEST(LPSolverTest, MaximizeEconomicProblemAnotherFormulation) {
  LinearProgram linear_program;
  ColIndex col_x = linear_program.FindOrCreateVariable("x");
  ColIndex col_y = linear_program.FindOrCreateVariable("y");
  ColIndex fixed_dummy_variable = linear_program.FindOrCreateVariable("z");
  linear_program.SetVariableBounds(col_x, -kInfinity, 100);
  linear_program.SetVariableBounds(col_y, 0, kInfinity);
  linear_program.SetVariableBounds(fixed_dummy_variable, 10, 10);
  linear_program.SetObjectiveCoefficient(col_x, 40);
  linear_program.SetObjectiveCoefficient(col_y, 70);
  RowIndex row_r1 = linear_program.FindOrCreateConstraint("r1");
  RowIndex row_r2 = linear_program.FindOrCreateConstraint("r2");
  linear_program.SetConstraintBounds(row_r1, -kInfinity, 100);
  linear_program.SetConstraintBounds(row_r2, -4000, kInfinity);
  linear_program.SetMaximizationProblem(true);
  linear_program.SetCoefficient(row_r1, col_x, 1.0);
  linear_program.SetCoefficient(row_r1, col_y, 1.0);
  linear_program.SetCoefficient(row_r2, col_x, -10.0);
  linear_program.SetCoefficient(row_r2, col_y, -50.0);
  linear_program.CleanUp();

  LPSolver solver;
  EXPECT_THAT(solver.Solve(linear_program),
              SolveStatusWith<SolveStatus::Optimal>(_));
  EXPECT_THAT(solver.GetObjectiveValue(),
              WithinSameAbsoluteOrRelativeTolerance(Fractional(6250.0),
                                                    kComparableEpsilon));

  EXPECT_THAT(solver.variable_values()[col_x],
              WithinSameAbsoluteOrRelativeTolerance(Fractional(25.0),
                                                    kComparableEpsilon));
  EXPECT_THAT(solver.variable_values()[col_y],
              WithinSameAbsoluteOrRelativeTolerance(Fractional(75.0),
                                                    kComparableEpsilon));
  EXPECT_THAT(solver.variable_values()[fixed_dummy_variable],
              WithinSameAbsoluteOrRelativeTolerance(Fractional(10.0),
                                                    kComparableEpsilon));
  EXPECT_THAT(solver.dual_values()[row_r1],
              WithinSameAbsoluteOrRelativeTolerance(Fractional(32.5),
                                                    kComparableEpsilon));
  EXPECT_THAT(solver.dual_values()[row_r2],
              WithinSameAbsoluteOrRelativeTolerance(Fractional(-0.75),
                                                    kComparableEpsilon));
  EXPECT_EQ(VariableStatus::BASIC, solver.variable_statuses()[col_x]);
  EXPECT_EQ(VariableStatus::BASIC, solver.variable_statuses()[col_y]);
  EXPECT_EQ(VariableStatus::FIXED_VALUE,
            solver.variable_statuses()[fixed_dummy_variable]);

  EXPECT_THAT(solver.constraint_activities()[row_r1],
              WithinSameAbsoluteOrRelativeTolerance(Fractional(100.0),
                                                    kComparableEpsilon));
  EXPECT_THAT(solver.constraint_activities()[row_r2],
              WithinSameAbsoluteOrRelativeTolerance(Fractional(-4000.0),
                                                    kComparableEpsilon));

  EXPECT_EQ(ConstraintStatus::AT_UPPER_BOUND,
            solver.constraint_statuses()[row_r1]);
  EXPECT_EQ(ConstraintStatus::AT_LOWER_BOUND,
            solver.constraint_statuses()[row_r2]);
}

TEST(LPSolverTest, Chvatal_p54) {
  const int kNumCols = 7;
  const int kNumRows = 3;
  const std::string kLinearProgram =
      "max: 4 x + y + 5 z + 3 t;"
      "0 <= x <= 1000;"
      "0 <= y <= 1000;"
      "0 <= z <= 1000;"
      "0 <= t <= 1000;"
      "-1000 <= r1 <= 1;"
      "-1000 <= r2 <= 55;"
      "-1000 <= r3 <= 3;"
      "   x -   y -   z + 3 t - r1 = 0;"
      " 5 x +   y + 3 z + 8 t - r2 = 0;"
      "  -x + 2 y + 3 z - 5 t - r3 = 0;";
  const ExpectedSolution<kNumCols, kNumRows> kExpectedSolution = {
      ProblemStatus::OPTIMAL,    // Status
      29,                        // Objective value
      {0, 14, 0, 5, 1, 54, 3},   // Solution values
      {-1, 0, -2, 0, 11, 0, 6},  // Reduced costs
      {11, 0, 6},                // Dual values
      {0, 0, 0},                 // Constraint activity
      {VariableStatus::AT_LOWER_BOUND, VariableStatus::BASIC,
       VariableStatus::AT_LOWER_BOUND, VariableStatus::BASIC,
       VariableStatus::AT_UPPER_BOUND, VariableStatus::BASIC,
       VariableStatus::AT_UPPER_BOUND},  // Variable status
      {ConstraintStatus::FIXED_VALUE, ConstraintStatus::FIXED_VALUE,
       ConstraintStatus::FIXED_VALUE}  // Constraint status
  };

  CheckLPSolver(kLinearProgram, kExpectedSolution);
}

// Same problem as Chvatal_p54 defined above with different bounds.
TEST(LPSolverTest, CheckSolutionOptimality) {
  const std::string kLinearProgram =
      "max: 4 x + y + 5 z + 3 t;"
      "x = 0;"
      "z >= 0;"
      "0 <= t <= 1000;"
      "-1000 <= r1 <= 1;"
      "-1000 <= r2 <= 55;"
      "-1000 <= r3 <= 3;"
      "   x -   y -   z + 3 t - r1 = 0;"
      " 5 x +   y + 3 z + 8 t - r2 = 0;"
      "  -x + 2 y + 3 z - 5 t - r3 = 0;";
  LinearProgram lp;
  ASSERT_TRUE(ParseLp(kLinearProgram, &lp));

  ProblemSolution solution(lp.num_constraints(), lp.num_variables());
  solution.status = ProblemStatus::OPTIMAL;
  solution.primal_values = {0, 14, 0, 5, 1, 54, 3};
  solution.variable_statuses = {
      VariableStatus::FIXED_VALUE,    VariableStatus::BASIC,
      VariableStatus::AT_LOWER_BOUND, VariableStatus::BASIC,
      VariableStatus::AT_UPPER_BOUND, VariableStatus::BASIC,
      VariableStatus::AT_UPPER_BOUND};
  solution.dual_values = {11, 0, 6};
  solution.constraint_statuses = {ConstraintStatus::FIXED_VALUE,
                                  ConstraintStatus::FIXED_VALUE,
                                  ConstraintStatus::FIXED_VALUE};
  LPSolver solver;
  EXPECT_EQ(ProblemStatus::OPTIMAL, solver.LoadAndVerifySolution(lp, solution));

  solution.primal_values[ColIndex(3)] = 0;
  EXPECT_EQ(ProblemStatus::IMPRECISE,
            solver.LoadAndVerifySolution(lp, solution));
  solution.primal_values[ColIndex(3)] = 5;

  GlopParameters parameters;
  Fractional epsilon = parameters.solution_feasibility_tolerance();

  solution.dual_values[RowIndex(1)] = epsilon / 8.0;
  EXPECT_EQ(ProblemStatus::OPTIMAL, solver.LoadAndVerifySolution(lp, solution));
  EXPECT_THAT(-epsilon,
              WithinSameAbsoluteOrRelativeTolerance(
                  solver.reduced_costs()[ColIndex(3)], Fractional(1e-6)));

  // The 1e-3 is to compensate the 1000 upper bound.
  solution.dual_values[RowIndex(1)] = -epsilon / 8.0 * 1e-3;
  EXPECT_EQ(ProblemStatus::OPTIMAL, solver.LoadAndVerifySolution(lp, solution));
  EXPECT_THAT(epsilon * 1e-3,
              WithinSameAbsoluteOrRelativeTolerance(
                  solver.reduced_costs()[ColIndex(3)], Fractional(1e-5)));
}

TEST(LPSolverTest, CheckBasicSolverIncrementality) {
  const std::string kLinearProgram =
      "max: 4 x + y + 5 z + 3 t;"
      "x = 0;"
      "z >= 0;"
      "0 <= t <= 1000;"
      "-1000 <= r1 <= 1;"
      "-1000 <= r2 <= 55;"
      "-1000 <= r3 <= 3;"
      "   x -   y -   z + 3 t - r1 = 0;"
      " 5 x +   y + 3 z + 8 t - r2 = 0;"
      "  -x + 2 y + 3 z - 5 t - r3 = 0;";
  LinearProgram lp;
  ASSERT_TRUE(ParseLp(kLinearProgram, &lp));

  LPSolver solver;
  solver.GetMutableParameters()->set_use_preprocessing(false);
  EXPECT_THAT(solver.Solve(lp), SolveStatusWith<SolveStatus::Optimal>(_));
  const int num_fresh_iterations = solver.GetNumberOfSimplexIterations();
  EXPECT_GT(num_fresh_iterations, 0);

  // The actual number of iterations is 3.
  // TODO(user): Remove this expectation if it changes too much when the solver
  // changes.
  EXPECT_EQ(num_fresh_iterations, 3);

  // Solving again will be done with 0 iterations.
  EXPECT_THAT(solver.Solve(lp), SolveStatusWith<SolveStatus::Optimal>(_));
  EXPECT_EQ(solver.GetNumberOfSimplexIterations(), 0);

  // But this is not the case after a Clear().
  solver.Clear();
  EXPECT_THAT(solver.Solve(lp), SolveStatusWith<SolveStatus::Optimal>(_));
  EXPECT_EQ(num_fresh_iterations, solver.GetNumberOfSimplexIterations());

  // More complex case, resuming an interrupted computation.
  solver.GetMutableParameters()->set_max_number_of_iterations(1);
  solver.Clear();
  EXPECT_THAT(solver.Solve(lp),
              SolveStatusWith<SolveStatus::PrimalFeasible>(_));
  EXPECT_EQ(1, solver.GetNumberOfSimplexIterations());

  // Note that this works on this simple example, but there is no guarantee
  // that the path followed by the solver will be exactly the same because
  // interupting it like this triggers extra recomputation and also resets the
  // random seed.
  //
  // TODO(user): Behaving exactly the same with and without interruption should
  // be doable, but is it needed?
  solver.GetMutableParameters()->set_max_number_of_iterations(-1);
  EXPECT_THAT(solver.Solve(lp), SolveStatusWith<SolveStatus::Optimal>(_));
  EXPECT_EQ(num_fresh_iterations - 1, solver.GetNumberOfSimplexIterations());
}

// Note that warm-start is tested more extensively in revised simplex. This just
// tests the wiring of LpSolver by storing the optimal solution and restarting
// solve from there.
TEST(LPSolverTest, UserProvidedBasis) {
  const std::string kLinearProgram =
      "max: 4 x + y + 5 z + 3 t;"
      "x = 0;"
      "z >= 0;"
      "0 <= t <= 1000;"
      "-1000 <=    x -   y -   z + 3 t <= 1;"
      "-1000 <=  5 x +   y + 3 z + 8 t <= 55;"
      "-1000 <=   -x + 2 y + 3 z - 5 t <= 3;";
  LinearProgram lp;
  ASSERT_TRUE(ParseLp(kLinearProgram, &lp));

  GlopParameters parameters;
  parameters.set_use_preprocessing(false);
  LPSolver solver;
  solver.SetParameters(parameters);
  EXPECT_THAT(solver.Solve(lp), SolveStatusWith<SolveStatus::Optimal>(_));
  EXPECT_EQ(solver.GetNumberOfSimplexIterations(), 3);

  // We check that the lower/upper bound swap is exercised.
  EXPECT_EQ(solver.constraint_statuses()[RowIndex(0)],
            ConstraintStatus::AT_UPPER_BOUND);
  EXPECT_EQ(solver.constraint_statuses()[RowIndex(1)], ConstraintStatus::BASIC);
  EXPECT_EQ(solver.constraint_statuses()[RowIndex(2)],
            ConstraintStatus::AT_UPPER_BOUND);

  // Now create another solver and solve directly from the optimal.
  // There should be no iteration needed.
  LPSolver other_solver;
  other_solver.SetParameters(parameters);
  other_solver.SetInitialBasis(solver.variable_statuses(),
                               solver.constraint_statuses());
  EXPECT_THAT(other_solver.Solve(lp), SolveStatusWith<SolveStatus::Optimal>(_));
  EXPECT_EQ(other_solver.GetNumberOfSimplexIterations(), 0);
}

TEST(LPSolverTest, MoveValuesInTheirBounds) {
  LinearProgram lp;
  ColIndex col_x = lp.FindOrCreateVariable("x");
  ColIndex col_y = lp.FindOrCreateVariable("y");
  lp.SetVariableBounds(col_x, -kInfinity, 100.0);
  lp.SetVariableBounds(col_y, 0.0, kInfinity);
  RowIndex row_r1 = lp.FindOrCreateConstraint("r1");
  RowIndex row_r2 = lp.FindOrCreateConstraint("r2");
  lp.SetConstraintBounds(row_r1, 0.0, kInfinity);
  lp.SetConstraintBounds(row_r2, -kInfinity, 0.0);

  LPSolver solver;
  ProblemSolution solution(lp.num_constraints(), lp.num_variables());
  solution.primal_values = {150.0, -10.0};
  solution.variable_statuses = {VariableStatus::BASIC, VariableStatus::BASIC};
  solution.constraint_statuses = {ConstraintStatus::AT_LOWER_BOUND,
                                  ConstraintStatus::AT_UPPER_BOUND};
  solution.dual_values = {10.0, 10.0};
  solution.status = ProblemStatus::OPTIMAL;

  // TODO(user): The solution checker does not check that the basis is
  // factorizable, so here it still returns ProblemStatus::OPTIMAL.
  EXPECT_EQ(ProblemStatus::OPTIMAL, solver.LoadAndVerifySolution(lp, solution));
  EXPECT_EQ(solver.variable_values()[col_x], 100.0);
  EXPECT_EQ(solver.variable_values()[col_y], 0.0);
  EXPECT_EQ(solver.dual_values()[row_r1], 10.0);
  EXPECT_EQ(solver.dual_values()[row_r2], 0.0);

  // Note that we still check the number of VariableStatus::BASIC variables.
  solution.constraint_statuses = {ConstraintStatus::BASIC,
                                  ConstraintStatus::BASIC};
  EXPECT_EQ(ProblemStatus::ABNORMAL,
            solver.LoadAndVerifySolution(lp, solution));
  solution.constraint_statuses = {ConstraintStatus::AT_LOWER_BOUND,
                                  ConstraintStatus::AT_UPPER_BOUND};

  lp.SetMaximizationProblem(true);
  EXPECT_EQ(ProblemStatus::OPTIMAL, solver.LoadAndVerifySolution(lp, solution));
  EXPECT_EQ(solver.dual_values()[row_r1], 0.0);
  EXPECT_EQ(solver.dual_values()[row_r2], 10.0);
}

// In non-debug mode, it is possible to construct an invalid program.
// In this case, we test that the LPSolver behaves correctly.
TEST(LPSolverTest, InvalidInput) {
  if constexpr (DEBUG_MODE) GTEST_SKIP() << "Runs only in opt mode";
  LinearProgram linear_program;
  EXPECT_TRUE(linear_program.IsValid());
  const Fractional nan = std::numeric_limits<double>::quiet_NaN();
  const ColIndex col = linear_program.CreateNewVariable();
  linear_program.SetVariableBounds(col, 0.0, kInfinity);
  linear_program.SetVariableBounds(col, 0.0, nan);
  EXPECT_FALSE(linear_program.IsValid());

  // Calling Solve() multiple times should give the same result.
  LPSolver solver;
  EXPECT_THAT(solver.Solve(linear_program),
              SolveStatusWith<SolveStatus::InvalidProblem>(_));
  EXPECT_THAT(solver.Solve(linear_program),
              SolveStatusWith<SolveStatus::InvalidProblem>(_));
}

TEST(LPSolverTest, WriteToProtoFile) {
  const std::string kLinearProgram =
      "max: 4 x + y + 5 z + 3 t;"
      "x = 0;"
      "z >= 0;"
      "0 <= t <= 1000;"
      "-1000 <= r1 <= 1;"
      "-1000 <= r2 <= 55;"
      "-1000 <= r3 <= 3;"
      "   x -   y -   z + 3 t - r1 = 0;"
      " 5 x +   y + 3 z + 8 t - r2 = 0;"
      "  -x + 2 y + 3 z - 5 t - r3 = 0;";
  LinearProgram linear_program;
  ASSERT_TRUE(ParseLp(kLinearProgram, &linear_program));

  const bool saved_flag = absl::GetFlag(FLAGS_lp_dump_to_proto_file);
  absl::Cleanup flag_closer = [saved_flag] {
    absl::SetFlag(&FLAGS_lp_dump_to_proto_file, saved_flag);
  };
  absl::SetFlag(&FLAGS_lp_dump_to_proto_file, true);
  absl::SetFlag(&FLAGS_lp_dump_dir, ::testing::TempDir());
  const std::string kName = "Test";
  const std::string kFileName =
      absl::GetFlag(FLAGS_lp_dump_dir) + "/" + kName + "-000001.pb.gz";
  linear_program.SetName(kName);

  LPSolver lp_solver;
  EXPECT_THAT(lp_solver.Solve(linear_program),
              SolveStatusWith<SolveStatus::Optimal>(_));
  MPModelProto memory_proto;
  LinearProgramToMPModelProto(linear_program, &memory_proto);
  MPModelProto disk_proto;
  ASSERT_OK(ReadFileToProto(kFileName, &disk_proto));
  EXPECT_THAT(memory_proto, EqualsProto(disk_proto));
}

TEST(LPSolverTest, TestObjectiveScaling) {
  const std::string kLinearProgram =
      "max: +5.382735153879039E-19 z1;"
      "-1 <= z0 <= 1;"
      "-1 <= z1 <= 1;"
      "-1 <= z2 <= 1;"
      "z0 - z1  <= 0;"
      "z0 - z2  <= 0;"
      "z1 - z2  <= 0;";
  LinearProgram lp;
  ASSERT_TRUE(ParseLp(kLinearProgram, &lp));

  LPSolver solver;
  EXPECT_THAT(solver.Solve(lp), SolveStatusWith<SolveStatus::Optimal>(_));
}

TEST(LpSolverTest, InvalidPreconditionsInOptMode) {
  if constexpr (DEBUG_MODE) GTEST_SKIP() << "Runs only in opt mode";
  LinearProgram lp;
  LPSolver solver;

  // LP must be cleaned up.
  lp.SetCoefficient(lp.CreateNewConstraint(), lp.CreateNewVariable(), 0.0);
  EXPECT_THAT(solver.Solve(lp),
              SolveStatusWith<SolveStatus::InvalidProblem>(_));
  lp.CleanUp();
  EXPECT_THAT(solver.Solve(lp), SolveStatusWith<SolveStatus::Optimal>(_));
}

TEST(LpSolverTest, BoundCrossingResultInInvalidModel) {
  LinearProgram lp;
  lp.SetDcheckBounds(false);
  lp.SetVariableBounds(lp.CreateNewVariable(), 1.0, 0.0);

  LPSolver solver;
  EXPECT_THAT(solver.Solve(lp),
              SolveStatusWith<SolveStatus::InvalidProblem>(_));
}

// This test used to fail when it was added.
TEST(LPSolverTest, ProblematicPresolve) {
  const std::string kLinearProgram =
      "min: 0.2 * t2 + 0.8 * t3;"
      "0.0 <= t2 <= 1000.0;"
      "0.0 <= t3 <= 1000.0;"
      "0.2 * t2 - 0.19999999999999996 * t3 >= 0;"
      "-0.2 * t2 + 0.20000000000000007 * t3 >= 0;";
  LinearProgram lp;
  ASSERT_TRUE(ParseLp(kLinearProgram, &lp));

  LPSolver solver;
  EXPECT_THAT(solver.Solve(lp), SolveStatusWith<SolveStatus::Optimal>(_));
}

// After scaling, the equation should be x == 1 / coeff so the constraint will
// have a FIXED status, but our validation used to fail since the unscaled
// problem do not have an exact equality constraint.
TEST(LPSolverTest, ScalingAndFixedStatus) {
  LinearProgram lp;
  const ColIndex c = lp.CreateNewVariable();
  const RowIndex r = lp.CreateNewConstraint();
  lp.SetConstraintBounds(r, 3.6979988349292639e-05, 3.6979988349292646e-05);
  const double coeff = 1.1851775554051365;
  lp.SetCoefficient(r, c, coeff);
  lp.SetObjectiveCoefficient(c, 1.0);

  // The value are such that we loose the fact that the two bounds are different
  // uppon division due to numerical errors!
  EXPECT_NE(lp.constraint_lower_bounds()[r], lp.constraint_upper_bounds()[r]);
  EXPECT_EQ(lp.constraint_lower_bounds()[r] / coeff,
            lp.constraint_upper_bounds()[r] / coeff);

  // Currently, the status of the unique row is "FIXED" even if the two bounds
  // are not exactly equal. This should not matter in the futre when we remove
  // the FIXED status completely.
  LPSolver solver;
  solver.GetMutableParameters()->set_use_preprocessing(false);
  EXPECT_THAT(solver.Solve(lp), SolveStatusWith<SolveStatus::Optimal>(_));
  EXPECT_EQ(solver.constraint_statuses()[r], ConstraintStatus::FIXED_VALUE);
}

// Test that the solver do not log anything to stdout by default, even with --v.
TEST(LPSolverTest, NoLogsToStdoutInVerbose) {
  LPSolver solver;
  ScopedStdStreamCapture stdout_capture(CapturedStream::kStdout);
  EXPECT_THAT(solver.Solve({}), SolveStatusWith<SolveStatus::Optimal>(_));
  if (ScopedStdStreamCapture::kIsSupported) {
    EXPECT_EQ(std::move(stdout_capture).StopCaptureAndReturnContents(), "");
  }
}

// A test model for LPSolverPrimalRayTest parametric test.
struct LPSolverPrimalRayTestModel {
  // Name of the model. It is used as a part of the name of the test, thus it
  // must only contains characters, numbers and underscores (NO spaces, commas,
  // ...).
  std::string name ABSL_REQUIRE_EXPLICIT_INIT;

  // The model that will parsed by ParseLp().
  //
  // The objective direction is overwritten by the test, as a convention it
  // should be written as a minimization to align with the rays in
  // expected_minimization_primal_rays.
  std::string lp_model ABSL_REQUIRE_EXPLICIT_INIT;

  // An exhaustive list of primal rays that can be produced by:
  // * either the phase II of the primal simplex,
  // * or the phase I of the dual simplex,
  // when solving a minimization of lp_model (rays for maximization are the
  // opposite).
  //
  // See RevisedSimplexPrimalRayTestModel documentation in
  // revised_simplex_test.cc for details on how to produce those for non trivial
  // models.
  std::vector<DenseRow> expected_minimization_primal_rays
      ABSL_REQUIRE_EXPLICIT_INIT;
};

std::ostream& operator<<(std::ostream& out,
                         const LPSolverPrimalRayTestModel& model) {
  out << "{ name: " << model.name << ", lp_model: '"
      << absl::CEscape(model.lp_model)
      << "', expected_minimization_primal_rays: [";
  bool first_ray = true;
  for (const auto& primal_ray : model.expected_minimization_primal_rays) {
    if (first_ray) {
      first_ray = false;
    } else {
      out << ", ";
    }
    out << '[';
    bool first_value = true;
    for (const Fractional value : primal_ray) {
      if (first_value) {
        first_value = false;
      } else {
        out << ", ";
      }
      out << RoundTripDoubleFormat(value);
    }
    out << ']';
  }
  out << "] }";
  return out;
}

struct LPSolverPrimalRayTest
    : ::testing::TestWithParam<
          std::tuple<LPSolverPrimalRayTestModel, TestAlgorithm, TestDirection,
                     TestUseScaling>> {
 protected:
  // Shortcuts to get parameters.
  TestDirection direction() const {
    return std::get<TestDirection>(GetParam());
  }
  TestAlgorithm algorithm() const {
    return std::get<TestAlgorithm>(GetParam());
  }
  TestUseScaling use_scaling() const {
    return std::get<TestUseScaling>(GetParam());
  }
};

// Returns a matcher for each column/row based on the expected sign of values of
// x or A⋅x based on the bounds of columns/rows.
//
// Index template parameter is either ColIndex or RowIndex.
//
// The zero_tolerance parameter is the absolute tolerance used to compare to
// zero (x = 0 is tested with |x| ≤ zero_tolerance).
//
// See LPSolverPrimalRayTest.Optimize for details.
template <typename Index>
std::vector<testing::Matcher<Fractional>> PrimalRaySignsMatchers(
    const StrictITIVector<Index, Fractional>& lower_bounds,
    const StrictITIVector<Index, Fractional>& upper_bounds,
    const Fractional zero_tolerance) {
  CHECK_EQ(lower_bounds.size(), upper_bounds.size());
  const Index num_elts = lower_bounds.size();

  std::vector<testing::Matcher<Fractional>> matchers;
  matchers.reserve(num_elts.value());
  for (Index i(0); i < num_elts; ++i) {
    const bool lb_is_finite = std::isfinite(lower_bounds[i]);
    const bool ub_is_finite = std::isfinite(upper_bounds[i]);
    if (lb_is_finite && ub_is_finite) {
      matchers.push_back(DoubleNear(0.0, zero_tolerance));
    } else if (lb_is_finite) {  // ub == +∞
      matchers.push_back(Ge(-zero_tolerance));
    } else if (ub_is_finite) {  // lb == -∞
      matchers.push_back(Le(zero_tolerance));
    } else {
      // Both bounds are infinite; only test that it is a number.
      matchers.push_back(Not(IsNan()));
    }
  }
  return matchers;
}

TEST_P(LPSolverPrimalRayTest, Optimize) {
  if (algorithm() == TestAlgorithm::kDualSimplex) {
    // TODO: b/553095297 - Add support of primal rays on dual infeasibility.
    GTEST_SKIP() << "Currently only primal simplex returns primal rays.";
  }

  LinearProgram lp;
  ASSERT_TRUE(
      ParseLp(std::get<LPSolverPrimalRayTestModel>(GetParam()).lp_model, &lp));
  lp.SetMaximizationProblem(direction() == TestDirection::kMaximize);

  LPSolver solver;
  // As of today (2026-09) rays can only be returned without pre-processing.
  solver.GetMutableParameters()->set_use_preprocessing(false);
  solver.GetMutableParameters()->set_use_scaling(use_scaling() ==
                                                 TestUseScaling::kYes);

  solver.GetMutableParameters()->set_use_dual_simplex(
      algorithm() == TestAlgorithm::kDualSimplex);

  const SolveStatus solve_status = solver.Solve(lp);
  switch (algorithm()) {
    case TestAlgorithm::kPrimalSimplex:
      EXPECT_THAT(solve_status,
                  SolveStatusWith<SolveStatus::PrimalUnbounded>(_));
      break;
    case TestAlgorithm::kDualSimplex:
      EXPECT_THAT(solve_status,
                  SolveStatusWith<SolveStatus::DualInfeasible>(_));
      break;
  }

  // We check the ray is a primal unbound ray.
  //
  // Using the notation in LPSolver::RunRevisedSimplex(), a primal unbound ray
  // is a vector x such:
  // * it is primal feasible (i.e. they have the proper sign):
  //   - ∀ i / L_i ≠ -∞, 0 ≤ A_i⋅x
  //   - ∀ i / U_i ≠ +∞,     A_i⋅x ≤ 0
  //   - ∀ j / l_j ≠ -∞, 0 ≤ x_j
  //   - ∀ j / u_j ≠ +∞,     x_j ≤ 0
  // * it improves the objective:
  //   - primal min: c^T⋅x < 0
  //   - primal max: c^T⋅x > 0

  // Check x signs.
  EXPECT_THAT(
      solver.primal_ray(),
      Optional(ElementsAreArray(PrimalRaySignsMatchers(
          lp.variable_lower_bounds(), lp.variable_upper_bounds(), 1e-7))));
  // Check A.x signs.
  {
    ASSERT_THAT(OptionalRefValue(solver.primal_ray()), Optional(_));
    const SparseMatrix& transposed_matrix = lp.GetTransposeSparseMatrix();
    const RowIndex num_rows = lp.num_constraints();
    DenseColumn columns_combination(num_rows, 0.0);
    for (RowIndex row(0); row < num_rows; ++row) {
      columns_combination[row] = ScalarProduct(
          *solver.primal_ray(), transposed_matrix.column(RowToColIndex(row)));
    }
    EXPECT_THAT(columns_combination,
                ElementsAreArray(PrimalRaySignsMatchers(
                    /*lower_bounds=*/lp.constraint_lower_bounds(),
                    /*upper_bounds=*/lp.constraint_upper_bounds(),
                    /*zero_tolerance=*/
                    solver.GetParameters().primal_feasibility_tolerance())));
  }
  // Then test the objective contribution sign.
  {
    ASSERT_THAT(OptionalRefValue(solver.primal_ray()), Optional(_));
    const Fractional objective_contribution =
        ScalarProduct(*solver.primal_ray(), lp.objective_coefficients());

    // We expect a number significatively not close to zero.
    if (direction() == TestDirection::kMaximize) {
      EXPECT_GE(objective_contribution, kRayObjectiveContributionZeroTolerance);
    } else {
      EXPECT_LE(objective_contribution,
                -kRayObjectiveContributionZeroTolerance);
    }
  }

  // Only test specific values when not using scaling.
  //
  // With scaling, if the problem contains independent subproblems, we can end
  // up a ray that is not proportional to any of the rays RevisedSimplex is
  // expected to produce.
  if (use_scaling() == TestUseScaling::kNo) {
    std::vector<DenseRow> expected_primal_rays;
    for (const auto& expected_minimization_primal_ray :
         std::get<LPSolverPrimalRayTestModel>(GetParam())
             .expected_minimization_primal_rays) {
      auto expected_primal_ray = expected_minimization_primal_ray;
      if (direction() == TestDirection::kMaximize) {
        ChangeSign(&expected_primal_ray);
      }
      expected_primal_rays.push_back(std::move(expected_primal_ray));
    }
    EXPECT_THAT(OptionalRefValue(solver.primal_ray()),
                AnyOfArray(expected_primal_rays));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All, LPSolverPrimalRayTest,
    testing::Combine(
        testing::Values(
            LPSolverPrimalRayTestModel{
                .name = "basic_ray",
                // We solve:
                //
                //   min x_1
                //
                //     x_1 + x_2 == 0  (c1)
                //
                // The unique (up to positive scaling) cost improving directions
                // for the min and max problems are x = (-1, 1) and x = (1, -1)
                // respectively.
                .lp_model = ("min: 1 x1 + 0 x2;"
                             "c1: 1 x1 + 1 x2 = 0;"),
                .expected_minimization_primal_rays =
                    {
                        {-1.0, 1.0},
                    },
            },
            // Test that when using scaling we properly produce a ray. In this
            // test the RevisedSimplex will received a scaled problem for which
            // the dual ray is {1.0, 1.0} but this ray is not an unbound ray of
            // the original problem.
            LPSolverPrimalRayTestModel{
                .name = "primal_scaling",
                .lp_model = ("min: 1 x1 + 0.125 x2;"
                             "c1: -2 <= 1 x1 - 0.125 x2 <= 2;"),
                .expected_minimization_primal_rays =
                    {
                        {-1.0, -8.0},
                        {-1.0 / 8.0, -1.0},
                    },
            }),
        testing::Values(TestAlgorithm::kPrimalSimplex,
                        TestAlgorithm::kDualSimplex),
        testing::Values(TestDirection::kMinimize, TestDirection::kMaximize),
        testing::Values(TestUseScaling::kYes, TestUseScaling::kNo)),
    [](const testing::TestParamInfo<LPSolverPrimalRayTest::ParamType>& info) {
      std::ostringstream oss;
      oss << std::get<LPSolverPrimalRayTestModel>(info.param).name << "__"
          << std::get<TestAlgorithm>(info.param) << "__"
          << std::get<TestDirection>(info.param) << "__"
          << std::get<TestUseScaling>(info.param);
      return oss.str();
    });

TEST(LPSolverTest, PrimalRayClearedWithIncrementalSolve) {
  LinearProgram lp;
  ASSERT_TRUE(
      ParseLp("min: 1 x1 + 0 x2;"
              "c1: 1 x1 + 1 x2 = 0;",
              &lp));

  GlopParameters parameters;
  parameters.set_use_preprocessing(false);
  parameters.set_use_dual_simplex(false);

  // Generate a primal ray.
  LPSolver solver;
  solver.SetParameters(parameters);
  EXPECT_THAT(solver.Solve(lp),
              SolveStatusWith<SolveStatus::PrimalUnbounded>(_));
  EXPECT_THAT(OptionalRefValue(solver.primal_ray()),
              Optional(ElementsAre(-1.0, 1.0)));

  // Check that primal ray is cleared between incremental solves.
  lp.SetVariableBounds(ColIndex(0), -1, 1);
  EXPECT_THAT(solver.Solve(lp), SolveStatusWith<SolveStatus::Optimal>(_));
  EXPECT_EQ(OptionalRefValue(solver.primal_ray()), std::nullopt);
}

// A test model for LPSolverDualRayTest parametric test.
struct LPSolverDualRayTestModel {
  // An expected dual ray.
  struct DualRay {
    // Dual values of the constraints.
    DenseColumn constraints ABSL_REQUIRE_EXPLICIT_INIT;

    // Dual values of the constraints.
    DenseRow bounds ABSL_REQUIRE_EXPLICIT_INIT;
  };

  // Name of the model. It is used as a part of the name of the test, thus it
  // must only contains characters, numbers and underscores (NO spaces, commas,
  // ...).
  std::string name ABSL_REQUIRE_EXPLICIT_INIT;

  // The model that will parsed by ParseLp().
  //
  // The objective direction is overwritten by the test, as a convention it
  // should be written as a minimization to align with the rays in
  // expected_minimization_dual_rays.
  std::string lp_model ABSL_REQUIRE_EXPLICIT_INIT;

  // An exhaustive list of dual rays that can be produced by:
  // * either the phase II of the dual simplex,
  // * or the phase I of the primal simplex,
  // when solving a minimization of lp_model (rays for maximization are the
  // opposite).
  //
  // See RevisedSimplexDualRayTestModel documentation in revised_simplex_test.cc
  // for details on how to produce those for non trivial models.
  std::vector<DualRay> expected_minimization_dual_rays
      ABSL_REQUIRE_EXPLICIT_INIT;
};

std::ostream& operator<<(std::ostream& out,
                         const LPSolverDualRayTestModel::DualRay& dual_ray) {
  const auto write_vector = [&](const auto& vec) {
    out << '[';
    bool first = true;
    for (const Fractional value : vec) {
      if (first) {
        first = false;
      } else {
        out << ", ";
      }
      out << RoundTripDoubleFormat(value);
    }
    out << ']';
  };

  out << "{ constraints: ";
  write_vector(dual_ray.constraints);
  out << ", bounds: ";
  write_vector(dual_ray.bounds);
  out << " }";
  return out;
}

std::ostream& operator<<(std::ostream& out,
                         const LPSolverDualRayTestModel& model) {
  out << "{ name: " << model.name << ", lp_model: '"
      << absl::CEscape(model.lp_model)
      << "', expected_minimization_dual_rays: [";
  bool first = true;
  for (const auto& dual_ray : model.expected_minimization_dual_rays) {
    if (first) {
      first = false;
    } else {
      out << ", ";
    }
    out << dual_ray;
  }
  out << "] }";
  return out;
}

struct LPSolverDualRayTest
    : ::testing::TestWithParam<
          std::tuple<LPSolverDualRayTestModel, TestAlgorithm, TestDirection,
                     TestUseScaling>> {
 protected:
  // Shortcuts to get parameters.
  TestDirection direction() const {
    return std::get<TestDirection>(GetParam());
  }
  TestAlgorithm algorithm() const {
    return std::get<TestAlgorithm>(GetParam());
  }
  TestUseScaling use_scaling() const {
    return std::get<TestUseScaling>(GetParam());
  }
};

// Returns the objective contribution of the dual values from the bounds of
// columns/rows.
//
// When negate_dual_values is true, swap the signs of dual values.
//
// Index template parameter is either ColIndex or RowIndex.
//
// See LPSolverDualRayTest.Optimize for details.
template <typename Index>
Fractional DualValuesObjectiveContribution(
    const bool negate_dual_values,
    const StrictITIVector<Index, Fractional>& dual_values,
    const StrictITIVector<Index, Fractional>& lower_bounds,
    const StrictITIVector<Index, Fractional>& upper_bounds) {
  CHECK_EQ(lower_bounds.size(), upper_bounds.size());
  CHECK_EQ(dual_values.size(), lower_bounds.size());
  const Index num_elts = lower_bounds.size();

  Fractional objective_contribution = 0.0;
  const Fractional sign_factor = negate_dual_values ? -1.0 : 1.0;
  for (Index i(0); i < num_elts; ++i) {
    const Fractional dual_value = sign_factor * dual_values[i];
    CHECK(!std::isnan(dual_value));
    if (dual_value > 0) {
      objective_contribution += dual_value * lower_bounds[i];
    } else if (dual_value < 0) {
      objective_contribution += dual_value * upper_bounds[i];
    }
  }
  return objective_contribution;
}

TEST_P(LPSolverDualRayTest, Optimize) {
  LinearProgram lp;
  ASSERT_TRUE(
      ParseLp(std::get<LPSolverDualRayTestModel>(GetParam()).lp_model, &lp));
  lp.SetMaximizationProblem(direction() == TestDirection::kMaximize);

  LPSolver solver;
  // As of today (2026-09) rays can only be returned without pre-processing.
  solver.GetMutableParameters()->set_use_preprocessing(false);
  solver.GetMutableParameters()->set_use_scaling(use_scaling() ==
                                                 TestUseScaling::kYes);

  solver.GetMutableParameters()->set_use_dual_simplex(
      algorithm() == TestAlgorithm::kDualSimplex);

  const SolveStatus solve_status = solver.Solve(lp);
  switch (algorithm()) {
    case TestAlgorithm::kPrimalSimplex:
      EXPECT_THAT(solve_status,
                  SolveStatusWith<SolveStatus::PrimalInfeasible>(_));
      break;
    case TestAlgorithm::kDualSimplex:
      EXPECT_THAT(solve_status, SolveStatusWith<SolveStatus::DualUnbounded>(_));
      break;
  }

  // We check the ray is a dual unbound ray.
  //
  // Using the notation in LPSolver::RunRevisedSimplex(), a dual unbound ray is
  // a set of (y_l, y_u, v, w) such:
  // * they are dual feasible:
  //   - primal min: y_l ≥ 0, y_u ≤ 0, v ≥ 0, w ≤ 0,
  //   - primal max: y_l ≤ 0, y_u ≥ 0, v ≤ 0, w ≥ 0,
  // * they satisfy:
  //   A^T⋅(y_l + y_u) + v + w = 0
  // * their improve the objective:
  //   - primal min: L^T⋅y_l + U^T⋅y_u + l^T⋅v + u^T⋅w > 0
  //   - primal max: L^T⋅y_l + U^T⋅y_u + l^T⋅v + u^T⋅w < 0
  //
  // In practice we consider the sum of the dual values (as one if zero when the
  // other is not):
  // * y = y_l + y_u (LPSolver::constraints_dual_ray())
  // * vw = v + w     (LPSolver::variable_bounds_dual_ray())
  //
  // When we maximize we can simply consider the opposite of the dual values:
  // * y' = -y,
  // * vw' = -vw
  // and we end up with similar conditions as for minimization.
  //
  // Rewriting the conditions for minimization (the dual feasiblity conditions
  // are included in testing that the selected bound based on the dual sign is
  // finite):
  // * A^T⋅y + vw = 0
  // * sum y_i⋅(L_i if y_i > 0 else U_i) + sum d_j⋅(l_j if d_j > 0 else u_j) > 0
  //
  // First check A^T⋅y + vw = 0.
  {
    ASSERT_THAT(OptionalRefValue(solver.constraints_dual_ray()), Optional(_));
    ASSERT_THAT(OptionalRefValue(solver.variable_bounds_dual_ray()),
                Optional(_));
    const ColIndex num_cols = lp.num_variables();
    DenseRow sum(num_cols, 0.0);
    for (ColIndex col(0); col < num_cols; ++col) {
      sum[col] = ScalarProduct(*solver.constraints_dual_ray(),
                               lp.GetSparseColumn(col)) +
                 (*solver.variable_bounds_dual_ray())[col];
    }
    EXPECT_THAT(sum,
                Each(DoubleNear(
                    0.0, solver.GetParameters().dual_feasibility_tolerance())));
  }
  // Then test the objective contribution:
  //
  // sum y_i⋅(L_i if y_i > 0 else U_i) + sum d_j⋅(l_j if d_j > 0 else u_j) > 0
  {
    ASSERT_THAT(OptionalRefValue(solver.constraints_dual_ray()), Optional(_));
    ASSERT_THAT(OptionalRefValue(solver.variable_bounds_dual_ray()),
                Optional(_));
    EXPECT_GE(DualValuesObjectiveContribution(
                  /*negate_dual_values=*/lp.IsMaximizationProblem(),
                  /*dual_values=*/*solver.constraints_dual_ray(),
                  /*lower_bounds=*/lp.constraint_lower_bounds(),
                  /*upper_bounds=*/lp.constraint_upper_bounds()) +
                  DualValuesObjectiveContribution(
                      /*negate_dual_values=*/lp.IsMaximizationProblem(),
                      /*dual_values=*/*solver.variable_bounds_dual_ray(),
                      /*lower_bounds=*/lp.variable_lower_bounds(),
                      /*upper_bounds=*/lp.variable_upper_bounds()),
              kRayObjectiveContributionZeroTolerance);
  }

  // Only test specific values when not using scaling.
  //
  // With scaling, if the problem contains independent subproblems, we can end
  // up a ray that is not proportional to any of the rays RevisedSimplex is
  // expected to produce (this happens with
  // random_with_different_primal_dual_simplex_rays model for example).
  if (use_scaling() == TestUseScaling::kNo) {
    std::vector<std::pair<DenseColumn, DenseRow>> expected_dual_rays;
    for (const auto& expected_minimization_dual_ray :
         std::get<LPSolverDualRayTestModel>(GetParam())
             .expected_minimization_dual_rays) {
      auto expected_dual_ray =
          std::make_pair(expected_minimization_dual_ray.constraints,
                         expected_minimization_dual_ray.bounds);
      if (direction() == TestDirection::kMaximize) {
        ChangeSign(&expected_dual_ray.first);
        ChangeSign(&expected_dual_ray.second);
      }
      expected_dual_rays.push_back(std::move(expected_dual_ray));
    }
    EXPECT_THAT(
        std::make_pair(OptionalRefValue(solver.constraints_dual_ray()),
                       OptionalRefValue(solver.variable_bounds_dual_ray())),
        AnyOfArray(expected_dual_rays));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All, LPSolverDualRayTest,
    testing::Combine(
        testing::Values(
            LPSolverDualRayTestModel{
                .name = "bounds_on_constraints_only",
                .lp_model = ("min: 1 x1 + 1 x2;"
                             "c1:  1 <= 1 x1 + 1 x2 <= 1;"
                             "c2:  0 <= 1 x1 + 1 x2 <= 0;"),
                // Dual:
                //
                //   max y_1
                //
                //      y_1 + y_2 == 1
                //      y_1 + y_2 == 1
                //
                // Any unbounded direction for the dual problems satisfies y1 =
                // - y2. For the max dual the unique (up to positive scaling)
                // ray that improves the objective is y = (1, -1) and for the
                // min dual it is y = (-1, 1).
                //
                .expected_minimization_dual_rays =
                    {
                        {.constraints = {1.0, -1.0}, .bounds = {0.0, 0.0}},
                    },
            },
            LPSolverDualRayTestModel{
                .name = "bounds_variables_and_constraints",
                .lp_model = ("min: 1 x1 + 1 x2;"
                             "x1 >= 0;"
                             "x2 >= 0;"
                             "c1: 1 x1 + 1 x2 <= -1;"),
                // Dual:
                //
                //   max  -y_1
                //
                //      y_1 + r_1 == 1
                //      y_1 + r_2 == 1
                //            y_1 <= 0
                //       r_1, r_2 >= 0
                //
                // where r_1 and r_2 are the dual multipliers associated to the
                // variable bounds.  The unique (up to positive scaling)
                // cost-improving unboundedness direction for the dual is y_1 =
                // -1 and r = (1, 1). If we change the primal to max (and the
                // dual to min) the cost-improving unboundedness direction is
                // multiplied by -1.
                .expected_minimization_dual_rays =
                    {
                        {.constraints = {-1.0}, .bounds = {1.0, 1.0}},
                    },
            },
            // Test that when using scaling we properly produce a ray. In this
            // test the RevisedSimplex will received a scaled problem for which
            // the dual ray is {1.0, 1.0} but this ray is not an unbound ray of
            // the original problem.
            LPSolverDualRayTestModel{
                .name = "dual_scaling",
                // Here the two rows are clearly not compatible (c2 expression
                // is -16 times c1 but the bounds are incompatible).
                .lp_model = ("min: 0.125 x1 + 0.5 x2;"
                             "x1 >= 0;"
                             "x2 >= 0;"
                             "c1: 0.125 <= 0.125 x1 + 0.125 x2 <= 2;"
                             "c2: -1 <= -2 x1 - 2 x2 <= 2;"),
                .expected_minimization_dual_rays =
                    {
                        {.constraints = {1.0, 1.0 / 16.0},
                         .bounds = {0.0, 0.0}},
                        {.constraints = {16.0, 1.0}, .bounds = {0.0, 0.0}},
                    },
            },
            LPSolverDualRayTestModel{
                .name = "random_with_different_primal_dual_simplex_rays",
                // A randomly generated problem that has:
                // * multiple rays candidates,
                // * different depending on the simplex algorithm,
                // * with power-of-two values.
                //
                // Note that the constraint "c1" is always broken, but the dual
                // simplex may not include it as "c2" is enough to proof
                // infeasibility. Taking the dual ray where "c1" coeff is 0 and
                // combining the constraint and bounds leads to:
                //
                //   8 *  6 <=   x2
                //  -1 *                x3 <= 9
                //  -1 *       8 x2 - 1 x3 <= -4
                //
                //       48 <=  8 x2
                //       -9 <=       - x3
                //        4 <= -8 x2 + x3
                //
                //       43 <= 0 (obvious infeasibility)
                //
                // Here the primal simplex will not do that; instead it will
                // always return a combination of both constraints.
                .lp_model = ("min: 0 x1 + 4 x2 + 4 x3;"
                             "6 <= x2 <= 8;"
                             "x3 <= 9;"
                             "c1: 0 x1 <= -1;"
                             "c2: -6 <= 8 x2 - 1 x3 <= -4;"),
                .expected_minimization_dual_rays =
                    {
                        // Dual-simplex rays (only on constraint).
                        {.constraints = {-1.0, 0.0}, .bounds = {0.0, 0.0, 0.0}},
                        {.constraints = {0.0, -1.0},
                         .bounds = {0.0, 8.0, -1.0}},
                        // Primal-simplex rays (always both constraints).
                        {.constraints = {-1.0, -1.0 / 8.0},
                         .bounds = {0.0, 1.0, -1.0 / 8.0}},
                        {.constraints = {-1.0, -1.0},
                         .bounds = {0.0, 8.0, -1.0}},
                    },
            }),
        testing::Values(TestAlgorithm::kPrimalSimplex,
                        TestAlgorithm::kDualSimplex),
        testing::Values(TestDirection::kMinimize, TestDirection::kMaximize),
        testing::Values(TestUseScaling::kYes, TestUseScaling::kNo)),
    [](const testing::TestParamInfo<LPSolverDualRayTest::ParamType>& info) {
      std::ostringstream oss;
      oss << std::get<LPSolverDualRayTestModel>(info.param).name << "__"
          << std::get<TestAlgorithm>(info.param) << "__"
          << std::get<TestDirection>(info.param) << "__"
          << std::get<TestUseScaling>(info.param);
      return oss.str();
    });

TEST(LPSolverTest, DualRayClearedWithIncrementalSolve) {
  LinearProgram lp;

  ASSERT_TRUE(
      ParseLp("min: 1 x1 + 1 x2;"
              "x1 >= 0;"
              "x2 >= 0;"
              "c1: 1 x1 + 1 x2 <= -1;",
              &lp));

  // Generate a dual ray.
  LPSolver solver;
  solver.GetMutableParameters()->set_use_preprocessing(false);
  solver.GetMutableParameters()->set_use_dual_simplex(true);

  EXPECT_THAT(solver.Solve(lp), SolveStatusWith<SolveStatus::DualUnbounded>(_));
  EXPECT_THAT(OptionalRefValue(solver.constraints_dual_ray()),
              Optional(ElementsAre(-1.0)));
  EXPECT_THAT(OptionalRefValue(solver.variable_bounds_dual_ray()),
              Optional(ElementsAre(1.0, 1.0)));

  // Make the problem feasible and resolve.
  lp.SetConstraintBounds(RowIndex{0}, 0, 1);
  EXPECT_THAT(solver.Solve(lp), SolveStatusWith<SolveStatus::Optimal>(_));
  EXPECT_EQ(OptionalRefValue(solver.constraints_dual_ray()), std::nullopt);
  EXPECT_EQ(OptionalRefValue(solver.variable_bounds_dual_ray()), std::nullopt);
}

TEST(LPSolverTest, TimeLimit) {
  LinearProgram linear_program;
  const ColIndex x = linear_program.CreateNewVariable();
  linear_program.SetVariableBounds(x, 0.5, 3.5);
  linear_program.SetObjectiveCoefficient(x, 2.0);
  linear_program.CleanUp();

  // The logging callback will wait for this duration; making sure the solve is
  // delayed and will be interrupted if we use a shorter time limit.
  constexpr absl::Duration kCallbackSleepDuration = absl::Milliseconds(200);

  LPSolver solver;
  auto& solver_parameters = *solver.GetMutableParameters();
  // Use half of the sleep time as time-limit.
  solver_parameters.set_max_time_in_seconds(
      absl::ToDoubleSeconds(kCallbackSleepDuration / 2));
  solver_parameters.set_log_search_progress(true);
  solver_parameters.set_log_to_stdout(false);
  bool first_call = true;
  solver.GetSolverLogger().AddInfoLoggingCallback(
      [&](const std::string& message) {
        if (first_call) {
          first_call = false;
          LOG(INFO) << "Sleeping for " << kCallbackSleepDuration << "...";
          absl::SleepFor(kCallbackSleepDuration);
          LOG(INFO) << "Waking up!";
        }
        LOG(INFO) << "Glop: " << message;
      });
  constexpr auto kExpectedCause = InterruptionCause::kTimeLimit;
  EXPECT_THAT(solver.Solve(linear_program),
              SolveStatusWithCause<SolveStatus::Init>(kExpectedCause));
}

TEST(LPSolverTest, ExternalInterruption) {
  LinearProgram linear_program;
  const ColIndex x = linear_program.CreateNewVariable();
  linear_program.SetVariableBounds(x, 0.5, 3.5);
  linear_program.SetObjectiveCoefficient(x, 2.0);
  linear_program.CleanUp();

  LPSolver solver;
  auto& solver_parameters = *solver.GetMutableParameters();
  solver_parameters.set_log_search_progress(true);
  solver_parameters.set_log_to_stdout(false);
  solver.GetSolverLogger().AddInfoLoggingCallback(
      [&](const std::string& message) { LOG(INFO) << "Glop: " << message; });

  TimeLimit time_limit;
  std::atomic<bool> interrupt = true;
  time_limit.RegisterExternalBooleanAsLimit(&interrupt);
  constexpr auto kExpectedCause = InterruptionCause::kExternal;
  EXPECT_THAT(solver.Solve(linear_program, time_limit),
              SolveStatusWithCause<SolveStatus::Init>(kExpectedCause));
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
