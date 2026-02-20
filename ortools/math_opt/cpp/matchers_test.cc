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

#include "ortools/math_opt/cpp/matchers.h"

#include <cmath>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/cpp/model.h"
#include "ortools/math_opt/cpp/solution.h"
#include "ortools/math_opt/cpp/solve_result.h"

namespace operations_research::math_opt {
namespace {
constexpr double kInf = std::numeric_limits<double>::infinity();

using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::StrEq;
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

TEST(ObjectiveBoundsNear, Is) {
  const ObjectiveBounds bounds = {.primal_bound = 1.0, .dual_bound = 2.0};
  EXPECT_THAT(bounds, ObjectiveBoundsNear(bounds));
}

TEST(ObjectiveBoundsNear, IsNotPrimalDifferent) {
  const ObjectiveBounds expected = {.primal_bound = 1.0, .dual_bound = 2.0};
  const ObjectiveBounds actual = {.primal_bound = 1.1, .dual_bound = 2.0};
  EXPECT_THAT(actual, Not(ObjectiveBoundsNear(expected)));
  EXPECT_THAT(actual, ObjectiveBoundsNear(expected, /*tolerance*/ 0.2));
}

TEST(ObjectiveBoundsNear, IsNotDualDifferent) {
  const ObjectiveBounds expected = {.primal_bound = 1.0, .dual_bound = 2.0};
  const ObjectiveBounds actual = {.primal_bound = 1.0, .dual_bound = 2.1};
  EXPECT_THAT(actual, Not(ObjectiveBoundsNear(expected)));
  EXPECT_THAT(actual, ObjectiveBoundsNear(expected, /*tolerance*/ 0.2));
}

TEST(ProblemStatusIs, IsEqual) {
  const ProblemStatus status = {
      .primal_status = FeasibilityStatus::kUndetermined,
      .dual_status = FeasibilityStatus::kInfeasible,
      .primal_or_dual_infeasible = false};
  EXPECT_THAT(status, ProblemStatusIs(status));
}

TEST(ProblemStatusIs, IsNotPrimalDifferent) {
  const ProblemStatus expected = {
      .primal_status = FeasibilityStatus::kUndetermined,
      .dual_status = FeasibilityStatus::kInfeasible,
      .primal_or_dual_infeasible = false};
  const ProblemStatus actual = {.primal_status = FeasibilityStatus::kFeasible,
                                .dual_status = FeasibilityStatus::kInfeasible,
                                .primal_or_dual_infeasible = false};
  EXPECT_THAT(actual, Not(ProblemStatusIs(expected)));
}

TEST(ProblemStatusIs, IsNotDualDifferent) {
  const ProblemStatus expected = {
      .primal_status = FeasibilityStatus::kFeasible,
      .dual_status = FeasibilityStatus::kUndetermined,
      .primal_or_dual_infeasible = false};
  const ProblemStatus actual = {.primal_status = FeasibilityStatus::kFeasible,
                                .dual_status = FeasibilityStatus::kInfeasible,
                                .primal_or_dual_infeasible = false};
  EXPECT_THAT(actual, Not(ProblemStatusIs(expected)));
}

TEST(ProblemStatusIs, IsNotPrimalOrDualInfeasibleDifferent) {
  const ProblemStatus expected = {
      .primal_status = FeasibilityStatus::kUndetermined,
      .dual_status = FeasibilityStatus::kUndetermined,
      .primal_or_dual_infeasible = true};
  const ProblemStatus actual = {
      .primal_status = FeasibilityStatus::kUndetermined,
      .dual_status = FeasibilityStatus::kUndetermined,
      .primal_or_dual_infeasible = false};
  EXPECT_THAT(actual, Not(ProblemStatusIs(expected)));
}

TEST(ApproximateMapMatcherTest, VariableIsNear) {
  Model model;
  const Variable w = model.AddBinaryVariable("w");
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  const Variable z = model.AddBinaryVariable("z");
  const VariableMap<double> actual = {{x, 2.0}, {y, 4.1}, {z, -2.5}};
  EXPECT_THAT(actual, IsNear(actual));
  EXPECT_THAT(actual, IsNear({{x, 2 + 1e-8}, {y, 4.1}, {z, -2.5}}));
  EXPECT_THAT(actual, Not(IsNear({{x, 2 + 1e-3}, {y, 4.1}, {z, -2.5}})));
  EXPECT_THAT(actual, Not(IsNear({{w, 1}, {z, -2.5}})));
  EXPECT_THAT(actual, Not(IsNear({{z, -2.5}})));
}

TEST(ApproximateMapMatcherTest, VariableIsNearlySupersetOf) {
  Model model;
  const Variable w = model.AddBinaryVariable("w");
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  const Variable z = model.AddBinaryVariable("z");
  const VariableMap<double> actual = {{x, 2.0}, {y, 4.1}, {z, -2.5}};
  EXPECT_THAT(actual, IsNearlySupersetOf(actual));
  EXPECT_THAT(actual, IsNearlySupersetOf({{y, 4.1}, {z, -2.5}}));
  EXPECT_THAT(actual, Not(IsNearlySupersetOf({{w, 1}, {y, 4.1}, {z, -2.5}})));
  EXPECT_THAT(actual, Not(IsNearlySupersetOf({{y, 4.4}, {z, -2.5}})));
}

TEST(ApproximateMapMatcherTest,
     QuadraticConstraintIsNearAndIsNearlySupersetOf) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const QuadraticConstraint c = model.AddQuadraticConstraint(x * x <= 0, "c");
  const QuadraticConstraint d = model.AddQuadraticConstraint(x * x <= 0, "d");
  const QuadraticConstraint e = model.AddQuadraticConstraint(x * x <= 0, "e");

  const absl::flat_hash_map<QuadraticConstraint, double> actual = {{c, 2},
                                                                   {e, 5}};
  EXPECT_THAT(actual, IsNearlySupersetOf(actual));
  EXPECT_THAT(actual, IsNear(actual));
  EXPECT_THAT(actual, IsNear({{c, 2 + 1e-8}, {e, 5}}));
  EXPECT_THAT(actual, Not(IsNear({{e, 5}})));
  EXPECT_THAT(actual, Not(IsNear({{c, 2 + 1e-2}, {e, 5}})));
  EXPECT_THAT(actual, Not(IsNear({{d, 5}})));
  EXPECT_THAT(actual, IsNearlySupersetOf({{e, 5}}));
}

TEST(ApproximateMapMatcherTest, LinearConstraintIsNearAndIsNearlySupersetOf) {
  Model model;
  const LinearConstraint c = model.AddLinearConstraint("c");
  const LinearConstraint d = model.AddLinearConstraint("d");
  const LinearConstraint e = model.AddLinearConstraint("e");

  const LinearConstraintMap<double> actual = {{c, 2}, {e, 5}};
  EXPECT_THAT(actual, IsNearlySupersetOf(actual));
  EXPECT_THAT(actual, IsNear(actual));
  EXPECT_THAT(actual, IsNear({{c, 2 + 1e-8}, {e, 5}}));
  EXPECT_THAT(actual, Not(IsNear({{e, 5}})));
  EXPECT_THAT(actual, Not(IsNear({{c, 2 + 1e-2}, {e, 5}})));
  EXPECT_THAT(actual, Not(IsNear({{d, 5}})));
  EXPECT_THAT(actual, IsNearlySupersetOf({{e, 5}}));
}

TEST(LinearExpressionMatcherTest, IsIdentical) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  const Variable z = model.AddBinaryVariable("z");
  const LinearExpression actual({{x, 1}, {y, 3}}, 4);
  EXPECT_THAT(actual, IsIdentical(LinearExpression({{x, 1}, {y, 3}}, 4)));
  EXPECT_THAT(actual, Not(IsIdentical(LinearExpression({{x, 1}}, 4))));
  EXPECT_THAT(actual, Not(IsIdentical(LinearExpression({{x, 1}, {y, 3}}, 5))));
  EXPECT_THAT(actual,
              Not(IsIdentical(LinearExpression({{x, 1}, {y, 3}, {z, 1}}, 4))));
  EXPECT_THAT(actual, Not(IsIdentical(LinearExpression(
                          {{x, std::nextafter(1, 2)}, {y, 3}}, 4))));

  Model other_model;
  const Variable other_x = Variable(other_model.storage(), x.typed_id());
  EXPECT_THAT(LinearExpression({{x, 1}}, 1),
              Not(IsIdentical(LinearExpression({{other_x, 1}}, 1))));

  // Same as actual, but with a structural zero term.
  const LinearExpression other({{x, 1}, {y, 3}, {z, 0}}, 4);
  EXPECT_THAT(other, Not(IsIdentical(actual)));
  EXPECT_THAT(other,
              IsIdentical(LinearExpression({{x, 1}, {y, 3}, {z, 0}}, 4)));
}

TEST(LinearExpressionMatcherDeathTest, IsIdenticalWithNaNs) {
  Model model;
  const Variable x = model.AddBinaryVariable();

  EXPECT_DEATH(IsIdentical(LinearExpression({}, kNaN)), "Illegal NaN");
  EXPECT_DEATH(IsIdentical(LinearExpression({{x, kNaN}}, 0)), "Illegal NaN");
}

TEST(LinearExpressionMatcherTest, IsIdenticalMatchedAgainstNaNs) {
  Model model;
  const Variable x = model.AddBinaryVariable();

  EXPECT_THAT(LinearExpression(kNaN), Not(IsIdentical(LinearExpression(0))));
  EXPECT_THAT(LinearExpression({{x, kNaN}}, 0),
              Not(IsIdentical(LinearExpression({{x, 1}}, 0))));
}

TEST(LinearExpressionMatcherTest, LinearExpressionIsNear) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  const LinearExpression actual({{x, 1.0}, {y, 3.0}}, 4.0);
  EXPECT_THAT(actual, LinearExpressionIsNear(
                          LinearExpression({{x, 1.0}, {y, 3.0}}, 4.0)));
  EXPECT_THAT(actual, Not(LinearExpressionIsNear(
                          LinearExpression({{x, 1.0}, {y, 3.0}}, 4.2),
                          /*tolerance=*/0.1)));
  EXPECT_THAT(actual, Not(LinearExpressionIsNear(
                          LinearExpression({{x, 1.2}, {y, 3.0}}, 4.0),
                          /*tolerance=*/0.1)));
  EXPECT_THAT(actual, LinearExpressionIsNear(
                          LinearExpression({{x, 1.0}, {y, 3.0}}, 4.2),
                          /*tolerance=*/0.3));
  EXPECT_THAT(actual, LinearExpressionIsNear(
                          LinearExpression({{x, 1.2}, {y, 3.0}}, 4.0),
                          /*tolerance=*/0.3));
}

TEST(BoundedLinearExpressionMatcherTest, ToleranceEachComponent) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  const BoundedLinearExpression actual(x + 3.0 * y + 4.0, -2.0, 5.0);
  constexpr double esp_error = 0.1;
  const BoundedLinearExpression esp_lb_error(x + 3.0 * y + 4.0,
                                             -2.0 + esp_error, 5.0);
  const BoundedLinearExpression esp_ub_error(x + 3.0 * y + 4.0, -2.0,
                                             5.0 + esp_error);
  const BoundedLinearExpression esp_term_error(x + (3.0 + esp_error) * y + 4.0,
                                               -2.0, 5.0);
  const BoundedLinearExpression esp_offset_error(x + 3.0 * y + 4.0,
                                                 -2.0 + esp_error, 5.0);
  EXPECT_THAT(actual, Not(IsNearlyEquivalent(esp_lb_error, esp_error / 2)));
  EXPECT_THAT(actual, Not(IsNearlyEquivalent(esp_lb_error, esp_error / 2)));
  EXPECT_THAT(actual, Not(IsNearlyEquivalent(esp_lb_error, esp_error / 2)));
  EXPECT_THAT(actual, Not(IsNearlyEquivalent(esp_lb_error, esp_error / 2)));

  EXPECT_THAT(actual, IsNearlyEquivalent(esp_lb_error, esp_error * 2));
  EXPECT_THAT(actual, IsNearlyEquivalent(esp_lb_error, esp_error * 2));
  EXPECT_THAT(actual, IsNearlyEquivalent(esp_lb_error, esp_error * 2));
  EXPECT_THAT(actual, IsNearlyEquivalent(esp_lb_error, esp_error * 2));
}

TEST(BoundedLinearExpressionMatcherTest, IsNearAddAndScale) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  const BoundedLinearExpression actual(x + 3.0 * y + 4.0, -2.0, 5.0);
  const BoundedLinearExpression negated(-x + -3.0 * y - 4.0, -5.0, 2.0);
  const BoundedLinearExpression add_scale(x + 3.0 * y + 5.0, -1.0, 6.0);
  const BoundedLinearExpression negated_add_scale(-x + -3.0 * y - 3.0, -4.0,
                                                  3.0);

  EXPECT_THAT(actual, IsNearlyEquivalent(actual, 0.0));
  EXPECT_THAT(actual, IsNearlyEquivalent(negated, 1e-10));
  EXPECT_THAT(actual, IsNearlyEquivalent(add_scale, 1e-10));
  EXPECT_THAT(actual, IsNearlyEquivalent(negated_add_scale, 1e-10));
}

TEST(QuadraticExpressionMatcherTest, IsIdentical) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  const Variable z = model.AddBinaryVariable("z");
  const QuadraticExpression actual({{x, x, 1}, {x, y, 3}}, {{z, 4}}, 5);
  EXPECT_THAT(actual, IsIdentical(QuadraticExpression({{x, x, 1}, {x, y, 3}},
                                                      {{z, 4}}, 5)));
  EXPECT_THAT(actual,
              Not(IsIdentical(QuadraticExpression({{x, x, 1}}, {{z, 4}}, 5))));
  EXPECT_THAT(
      actual,
      Not(IsIdentical(QuadraticExpression({{x, x, 1}, {x, y, 3}}, {}, 5))));
  EXPECT_THAT(actual, Not(IsIdentical(QuadraticExpression(
                          {{x, x, 1}, {x, y, 3}}, {{z, 4}}, 6))));
  EXPECT_THAT(actual, Not(IsIdentical(QuadraticExpression(
                          {{x, x, 1}, {x, y, 3}, {x, z, 6}}, {{z, 4}}, 5))));
  EXPECT_THAT(actual, Not(IsIdentical(QuadraticExpression(
                          {{x, x, 1}, {x, y, 3}}, {{z, 4}, {x, 6}}, 5))));
  EXPECT_THAT(actual,
              Not(IsIdentical(QuadraticExpression(
                  {{x, x, std::nextafter(1, 2)}, {x, y, 3}}, {{z, 4}}, 5))));

  Model other_model;
  const Variable other_x = Variable(other_model.storage(), x.typed_id());
  EXPECT_THAT(
      QuadraticExpression({{x, x, 1}}, {}, 1),
      Not(IsIdentical(QuadraticExpression({{other_x, other_x, 1}}, {}, 1))));

  // Same as actual, but with structural zero terms.
  const QuadraticExpression other({{x, x, 1}, {x, y, 3}, {x, z, 0}},
                                  {{z, 4}, {y, 0}}, 5);
  EXPECT_THAT(other, Not(IsIdentical(actual)));
  EXPECT_THAT(other,
              IsIdentical(QuadraticExpression({{x, x, 1}, {x, y, 3}, {x, z, 0}},
                                              {{z, 4}, {y, 0}}, 5)));
}

TEST(QuadraticExpressionMatcherDeathTest, IsIdenticalWithNaNs) {
  Model model;
  const Variable x = model.AddBinaryVariable();

  EXPECT_DEATH(IsIdentical(QuadraticExpression({}, {}, kNaN)), "Illegal NaN");
  EXPECT_DEATH(IsIdentical(QuadraticExpression({}, {{x, kNaN}}, 0)),
               "Illegal NaN");
  EXPECT_DEATH(IsIdentical(QuadraticExpression({{x, x, kNaN}}, {}, 0)),
               "Illegal NaN");
}

TEST(QuadraticExpressionMatcherTest, IsIdenticalMatchedAgainstNaNs) {
  Model model;
  const Variable x = model.AddBinaryVariable();

  EXPECT_THAT(QuadraticExpression(kNaN),
              Not(IsIdentical(QuadraticExpression(0))));
  EXPECT_THAT(QuadraticExpression({}, {{x, kNaN}}, 0),
              Not(IsIdentical(QuadraticExpression({}, {{x, 1}}, 0))));
  EXPECT_THAT(QuadraticExpression({{x, x, kNaN}}, {}, 0),
              Not(IsIdentical(QuadraticExpression({{x, x, 1}}, {}, 0))));
}

TEST(PrimalSolutionMatcherTest, IsNear) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  const Variable z = model.AddBinaryVariable("z");
  const PrimalSolution expected = {
      .variable_values = {{x, 2.0}, {y, 4.1}, {z, -2.5}},
      .objective_value = 2.0,
      .feasibility_status = SolutionStatus::kFeasible};
  PrimalSolution expected_no_status = expected;
  expected_no_status.feasibility_status = SolutionStatus::kUndetermined;

  {
    const PrimalSolution actual = expected;
    EXPECT_THAT(actual, IsNear(expected));
  }

  {
    PrimalSolution actual = expected;
    actual.variable_values[x] += 1e-8;
    EXPECT_THAT(actual, IsNear(expected));
  }

  {
    PrimalSolution actual = expected;
    actual.variable_values[x] += 4;
    EXPECT_THAT(actual, Not(IsNear(expected)));
  }

  {
    PrimalSolution actual = expected;
    actual.variable_values.erase(x);
    EXPECT_THAT(actual, Not(IsNear(expected)));
  }

  {
    PrimalSolution actual = expected;
    actual.objective_value += 5.0;
    EXPECT_THAT(actual, Not(IsNear(expected)));
  }

  {
    PrimalSolution actual = expected;
    actual.feasibility_status = SolutionStatus::kInfeasible;
    EXPECT_THAT(actual, Not(IsNear(expected)));
    EXPECT_THAT(actual, Not(IsNear(expected_no_status)));
    actual.feasibility_status = SolutionStatus::kUndetermined;
  }
}

TEST(DualSolutionMatcherTest, IsNear) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");

  const LinearConstraint c = model.AddLinearConstraint("c");
  const LinearConstraint d = model.AddLinearConstraint("d");

  const QuadraticConstraint e = model.AddQuadraticConstraint(x * x <= 0.0, "e");
  const QuadraticConstraint f = model.AddQuadraticConstraint(x * x <= 0.0, "f");

  const DualSolution expected = {
      .dual_values = {{c, 1.0}, {d, 3.1}},
      .quadratic_dual_values = {{e, 5.0}, {f, 6.1}},
      .reduced_costs = {{x, 2.0}, {y, 4.1}},
      .objective_value = 2.0,
      .feasibility_status = SolutionStatus::kFeasible};
  DualSolution expected_no_obj = expected;
  expected_no_obj.objective_value = std::nullopt;
  DualSolution expected_no_status = expected;
  expected_no_status.feasibility_status = SolutionStatus::kUndetermined;

  {
    const DualSolution actual = expected;
    EXPECT_THAT(actual, IsNear(expected));
  }

  {
    DualSolution actual = expected;
    actual.reduced_costs[x] += 1e-8;
    EXPECT_THAT(actual, IsNear(expected));
  }

  {
    DualSolution actual = expected;
    actual.reduced_costs[x] += 4;
    EXPECT_THAT(actual, Not(IsNear(expected)));
  }

  {
    DualSolution actual = expected;
    actual.dual_values.erase(c);
    EXPECT_THAT(actual, Not(IsNear(expected)));
  }

  {
    DualSolution actual = expected;
    actual.quadratic_dual_values.erase(e);
    EXPECT_THAT(actual, Not(IsNear(expected)));
  }

  {
    DualSolution actual = expected;
    *actual.objective_value += 5.0;
    EXPECT_THAT(actual, Not(IsNear(expected)));
    EXPECT_THAT(actual, Not(IsNear(expected_no_obj)));
  }

  {
    DualSolution actual = expected;
    actual.feasibility_status = SolutionStatus::kInfeasible;
    EXPECT_THAT(actual, Not(IsNear(expected)));
    EXPECT_THAT(actual, Not(IsNear(expected_no_status)));
  }
}

TEST(BasisTest, BasisIsOrCompatibleTest) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");

  const LinearConstraint c = model.AddLinearConstraint("c");
  const LinearConstraint d = model.AddLinearConstraint("d");

  Basis b1 = {.constraint_status = {{c, BasisStatus::kBasic},
                                    {d, BasisStatus::kAtUpperBound}},
              .variable_status = {{x, BasisStatus::kAtLowerBound},
                                  {y, BasisStatus::kBasic}},
              .basic_dual_feasibility = SolutionStatus::kFeasible};

  EXPECT_THAT(b1, BasisIs(b1));

  {
    Basis b2 = b1;
    b2.constraint_status[d] = BasisStatus::kAtLowerBound;
    EXPECT_THAT(b1, Not(BasisIs(b2)));
  }

  {
    Basis b3 = b1;
    b3.variable_status[x] = BasisStatus::kBasic;
    EXPECT_THAT(b1, Not(BasisIs(b3)));
  }

  {
    Basis b4 = b1;
    b4.variable_status.clear();
    EXPECT_THAT(b1, Not(BasisIs(b4)));
  }

  {
    Basis b5 = b1;
    b5.basic_dual_feasibility = SolutionStatus::kUndetermined;
    EXPECT_THAT(b1, Not(BasisIs(b5)));
  }
}

TEST(SolutionTest, IsNear) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");

  const LinearConstraint c = model.AddLinearConstraint("c");
  const LinearConstraint d = model.AddLinearConstraint("d");

  const Solution expected = {
      .primal_solution =
          PrimalSolution{.variable_values = {{x, 2.0}, {y, 4.1}},
                         .objective_value = 2.0,
                         .feasibility_status = SolutionStatus::kFeasible},
      .dual_solution =
          DualSolution{.dual_values = {{c, 1.0}, {d, 3.1}},
                       .reduced_costs = {{x, 2.0}, {y, 4.1}},
                       .objective_value = 2.0,
                       .feasibility_status = SolutionStatus::kFeasible},
      .basis = Basis{.constraint_status = {{c, BasisStatus::kBasic},
                                           {d, BasisStatus::kAtUpperBound}},
                     .variable_status = {{x, BasisStatus::kAtLowerBound},
                                         {y, BasisStatus::kBasic}},
                     .basic_dual_feasibility = SolutionStatus::kFeasible}};

  {
    const Solution actual = expected;
    EXPECT_THAT(actual, IsNear(expected));
  }

  {
    Solution actual = expected;
    actual.primal_solution->objective_value += 5.0;
    *actual.dual_solution->objective_value += 5.0;
    actual.basis->variable_status[x] = BasisStatus::kBasic;
    EXPECT_THAT(actual, Not(IsNear(expected)));

    const SolutionMatcherOptions check_nothing{
        .check_primal = false, .check_dual = false, .check_basis = false};
    EXPECT_THAT(actual, IsNear(expected, check_nothing));
  }

  {
    Solution actual = expected;
    actual.primal_solution->feasibility_status = SolutionStatus::kUndetermined;
    actual.dual_solution->feasibility_status = SolutionStatus::kUndetermined;
    actual.basis->basic_dual_feasibility = SolutionStatus::kUndetermined;
    EXPECT_THAT(actual, Not(IsNear(expected)));
  }

  {
    const SolutionMatcherOptions skip_primal{.check_primal = false};
    Solution actual = expected;
    actual.primal_solution->objective_value += 5.0;
    EXPECT_THAT(actual, IsNear(expected, skip_primal));
  }

  {
    const SolutionMatcherOptions skip_dual{.check_dual = false};
    Solution actual = expected;
    *actual.dual_solution->objective_value += 5.0;
    EXPECT_THAT(actual, IsNear(expected, skip_dual));
  }

  {
    const SolutionMatcherOptions skip_basis{.check_basis = false};
    Solution actual = expected;
    actual.basis->variable_status[x] = BasisStatus::kBasic;
    EXPECT_THAT(actual, IsNear(expected, skip_basis));
  }
}

TEST(PrimalRayMatcherTest, PrimalRayIsNear) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  const Variable z = model.AddBinaryVariable("z");
  PrimalRay actual = {.variable_values = {{x, 2.0}, {y, 4.1}, {z, -2.5}}};
  EXPECT_THAT(actual, IsNear(actual));
  {
    PrimalRay expected = {
        .variable_values = {{x, 2.0 + 1e-8}, {y, 4.1}, {z, -2.5}}};
    EXPECT_THAT(actual, IsNear(expected));
  }
  {
    PrimalRay expected = {.variable_values = {{x, 4.0}, {y, 8.2}, {z, -5.0}}};
    EXPECT_THAT(actual, IsNear(expected));
  }
  {
    PrimalRay expected = {.variable_values = {{x, 1.0}, {y, 2.05}, {z, -1.25}}};
    EXPECT_THAT(actual, IsNear(expected));
  }
  {
    PrimalRay expected = {
        .variable_values = {{x, 4.0}, {y, 8.2 + 1e-8}, {z, -5.0}}};
    EXPECT_THAT(actual, IsNear(expected));
  }

  {
    PrimalRay expected = {.variable_values = {{x, 2.1}, {y, 4.1}, {z, -2.5}}};
    EXPECT_THAT(actual, Not(IsNear(expected)));
  }
  {
    PrimalRay expected = {.variable_values = {{x, 4.0}, {y, 8.5}, {z, -5.0}}};
    EXPECT_THAT(actual, Not(IsNear(expected)));
  }
  {
    PrimalRay expected = {.variable_values = {{x, 0}, {y, 0}, {z, 0}}};
    EXPECT_THAT(actual, Not(IsNear(expected)));
  }
  {
    PrimalRay expected;
    EXPECT_THAT(actual, Not(IsNear(expected)));
  }
}

TEST(DualRayMatcherTest, DualRayIsNear) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");

  const LinearConstraint c = model.AddLinearConstraint("c");
  const LinearConstraint d = model.AddLinearConstraint("d");

  DualRay actual = {.dual_values = {{c, 1.0}, {d, 3.1}},
                    .reduced_costs = {{x, 2.0}, {y, 4.1}}};

  {
    DualRay expected = actual;
    EXPECT_THAT(actual, IsNear(expected));
  }
  {
    DualRay expected = {.dual_values = {{c, 2.0}, {d, 6.2}},
                        .reduced_costs = {{x, 4.0}, {y, 8.2}}};
    EXPECT_THAT(actual, IsNear(expected));
  }

  {
    DualRay expected = {.dual_values = {{c, 3.0}, {d, 9.3}},
                        .reduced_costs = {{x, 4.0}, {y, 8.2}}};
    EXPECT_THAT(actual, Not(IsNear(expected)));
  }

  {
    DualRay expected = actual;
    expected.reduced_costs[x] += 3.0;
    EXPECT_THAT(actual, Not(IsNear(expected)));
  }

  {
    DualRay expected = actual;
    expected.dual_values.erase(c);
    EXPECT_THAT(actual, Not(IsNear(expected)));
  }
  {
    DualRay expected;
    EXPECT_THAT(actual, Not(IsNear(expected)));
  }
}

TEST(LimitIs, Is) {
  const Termination actual_feasible =
      Termination::Feasible(/*is_maximize=*/false, Limit::kTime,
                            /*finite_primal_objective=*/20.0,
                            /*optional_dual_objective=*/10.0, "full string");
  EXPECT_THAT(actual_feasible, LimitIs(Limit::kTime, HasSubstr("full")));
  EXPECT_THAT(actual_feasible, LimitIs(Limit::kTime));
  const Termination actual_no_solution = Termination::NoSolutionFound(
      /*is_maximize=*/false, Limit::kTime,
      /*optional_dual_objective=*/10.0, "full string");
  EXPECT_THAT(actual_no_solution, LimitIs(Limit::kTime, HasSubstr("full")));
  EXPECT_THAT(actual_no_solution, LimitIs(Limit::kTime));
}

TEST(LimitIs, IsNotLimit) {
  const Termination actual_feasible =
      Termination::Feasible(/*is_maximize=*/false, Limit::kIteration,
                            /*finite_primal_objective=*/20.0);
  EXPECT_THAT(actual_feasible, Not(LimitIs(Limit::kTime)));
  const Termination actual_no_solution =
      Termination::NoSolutionFound(/*is_maximize=*/false, Limit::kIteration);
  EXPECT_THAT(actual_no_solution, Not(LimitIs(Limit::kTime)));
}

TEST(LimitIs, IsNotDetail) {
  const Termination actual_feasible =
      Termination::Feasible(/*is_maximize=*/false, Limit::kIteration,
                            /*finite_primal_objective=*/20.0,
                            /*optional_dual_objective=*/10.0, "string");
  EXPECT_THAT(actual_feasible,
              Not(LimitIs(Limit::kIteration, HasSubstr("full"))));
}

TEST(LimitIs, IsNotReason) {
  const Termination actual = Termination::Infeasible(
      /*is_maximize=*/false,
      /*dual_feasibility_status=*/FeasibilityStatus::kFeasible);
  EXPECT_THAT(actual, Not(LimitIs(Limit::kTime)));
}

TEST(TerminationIsIgnoreDetailTest, NoLimitEqual) {
  const Termination actual = Termination::Optimal(/*objective_value=*/10.0);
  EXPECT_THAT(actual, TerminationIsIgnoreDetail(actual));
}

TEST(TerminationIsIgnoreDetailTest, BadLimitNotEqual) {
  const Termination actual = Termination::Optimal(/*objective_value=*/10.0);
  Termination bad_expected = actual;
  bad_expected.limit = Limit::kTime;
  EXPECT_THAT(actual, Not(TerminationIsIgnoreDetail(bad_expected)));
}

TEST(TerminationIsIgnoreDetailTest, DetailIgnored) {
  const Termination actual =
      Termination::Optimal(/*objective_value=*/10.0, "cat");
  const Termination expected =
      Termination::Optimal(/*objective_value=*/10.0, "dog");
  EXPECT_THAT(actual, TerminationIsIgnoreDetail(expected));
}

TEST(TerminationIsIgnoreDetailTest, ExpectedHasLimit) {
  const Termination actual =
      Termination::Feasible(/*is_maximize=*/false, Limit::kTime,
                            /*finite_primal_objective=*/20.0,
                            /*optional_dual_objective=*/10.0);
  EXPECT_THAT(actual, TerminationIsIgnoreDetail(actual));
}

TEST(TerminationIsIgnoreDetailTest, ExpectedHasWrongLimit) {
  const Termination actual =
      Termination::Feasible(/*is_maximize=*/false, Limit::kTime,
                            /*finite_primal_objective=*/20.0,
                            /*optional_dual_objective=*/10.0);
  EXPECT_THAT(actual, Not(TerminationIsIgnoreDetail(Termination::Feasible(
                          /*is_maximize=*/false, Limit::kIteration,
                          /*finite_primal_objective=*/20.0))));
}

TEST(TerminationIsIgnoreDetailTest, ExpectedHasLimitDetailIgnored) {
  const Termination actual =
      Termination::Feasible(/*is_maximize=*/false, Limit::kTime,
                            /*finite_primal_objective=*/20.0);
  EXPECT_THAT(actual, TerminationIsIgnoreDetail(Termination::Feasible(
                          /*is_maximize=*/false, Limit::kTime,
                          /*finite_primal_objective=*/20.0,
                          /*optional_dual_objective=*/10.0, "mouse")));
}

TEST(ReasonIs, Is) {
  Termination actual = Termination::Infeasible(
      /*is_maximize=*/false,
      /*dual_feasibility_status=*/FeasibilityStatus::kFeasible);
  EXPECT_THAT(actual, ReasonIs(TerminationReason::kInfeasible));
}

TEST(ReasonIs, IsNot) {
  Termination actual = Termination::Infeasible(
      /*is_maximize=*/false,
      /*dual_feasibility_status=*/FeasibilityStatus::kFeasible);
  EXPECT_THAT(actual, Not(ReasonIs(TerminationReason::kUnbounded)));
}

TEST(ReasonIsOptimal, IsOptimal) {
  Termination actual = Termination::Optimal(/*objective_value=*/10.0);
  EXPECT_THAT(actual, ReasonIsOptimal());
}

TEST(ReasonIsOptimal, NotOptimal) {
  Termination actual = Termination::Infeasible(
      /*is_maximize=*/false,
      /*dual_feasibility_status=*/FeasibilityStatus::kFeasible);
  EXPECT_THAT(actual, Not(ReasonIsOptimal()));
}

TEST(TerminationIsOptimal, NotOptimalReason) {
  const double primal_objective_value = 10.0;
  const double dual_objective_value = 20.0;
  Termination actual =
      Termination::Optimal(primal_objective_value, dual_objective_value);
  actual.reason = TerminationReason::kInfeasible;
  EXPECT_THAT(actual, Not(TerminationIsOptimal(primal_objective_value,
                                               dual_objective_value)));
}

TEST(TerminationIsOptimal, NotPrimalFeasible) {
  const double primal_objective_value = 10.0;
  const double dual_objective_value = 20.0;
  Termination actual =
      Termination::Optimal(primal_objective_value, dual_objective_value);
  actual.problem_status.primal_status = FeasibilityStatus::kInfeasible;
  EXPECT_THAT(actual, Not(TerminationIsOptimal(primal_objective_value,
                                               dual_objective_value)));
}

TEST(TerminationIsOptimal, NotDualFeasible) {
  const double primal_objective_value = 10.0;
  const double dual_objective_value = 20.0;
  Termination actual =
      Termination::Optimal(primal_objective_value, dual_objective_value);
  actual.problem_status.dual_status = FeasibilityStatus::kInfeasible;
  EXPECT_THAT(actual, Not(TerminationIsOptimal(primal_objective_value,
                                               dual_objective_value)));
}

TEST(TerminationIsOptimal, NotFalsePrimalOrDualInfeasible) {
  const double primal_objective_value = 10.0;
  const double dual_objective_value = 20.0;
  Termination actual =
      Termination::Optimal(primal_objective_value, dual_objective_value);
  actual.problem_status.primal_or_dual_infeasible = true;
  EXPECT_THAT(actual, Not(TerminationIsOptimal(primal_objective_value,
                                               dual_objective_value)));
}

TEST(TerminationIsOptimal, WrongPrimalBound) {
  const double primal_objective_value = 10.0;
  const double dual_objective_value = 20.0;
  const Termination actual = Termination::Optimal(
      /*primal_objective_value=*/10.1, dual_objective_value);
  EXPECT_THAT(actual, Not(TerminationIsOptimal(primal_objective_value,
                                               dual_objective_value)));
  EXPECT_THAT(actual,
              TerminationIsOptimal(primal_objective_value, dual_objective_value,
                                   /*tolerance=*/0.2));
}

TEST(TerminationIsOptimal, WrongDualBound) {
  const double primal_objective_value = 10.0;
  const double dual_objective_value = 20.0;
  const Termination actual = Termination::Optimal(
      primal_objective_value, /*dual_objective_value=*/20.1);
  EXPECT_THAT(actual, Not(TerminationIsOptimal(primal_objective_value,
                                               dual_objective_value)));
  EXPECT_THAT(actual,
              TerminationIsOptimal(primal_objective_value, dual_objective_value,
                                   /*tolerance=*/0.2));
}

TEST(TerminationIsOptimal, Optimal) {
  const double primal_objective_value = 10.0;
  const Termination actual =
      Termination::Optimal(primal_objective_value,
                           /*dual_objective_value=*/primal_objective_value +
                               kMatcherDefaultTolerance / 2.0);
  EXPECT_THAT(actual, TerminationIsOptimal(primal_objective_value));
}

TEST(IsOptimalTest, IsOptimal) {
  // Assuming maximization.
  // TODO(b/309658404): consider changing to finite dual bound.
  SolveResult actual{Termination::Optimal(/*primal_objective_value=*/20.0,
                                          /*dual_objective_value=*/kInf)};
  actual.solutions.push_back(
      Solution{.primal_solution = PrimalSolution{
                   .objective_value = 10.0,
                   .feasibility_status = SolutionStatus::kFeasible}});
  EXPECT_THAT(actual, IsOptimal());
}

TEST(IsOptimalTest, NotOptimal) {
  SolveResult actual{Termination::Infeasible(
      /*is_maximize=*/false,
      /*dual_feasibility_status=*/FeasibilityStatus::kFeasible)};
  EXPECT_THAT(actual, Not(IsOptimal()));
}

TEST(IsOptimalTest, CheckObjective) {
  // Assuming maximization.
  // TODO(b/309658404): consider changing to finite dual bound.
  SolveResult actual{Termination::Optimal(/*primal_objective_value=*/50.0,
                                          /*dual_objective_value=*/kInf)};
  actual.solutions.push_back(
      Solution{.primal_solution = PrimalSolution{
                   .objective_value = 42.0,
                   .feasibility_status = SolutionStatus::kFeasible}});
  EXPECT_THAT(actual, IsOptimal(42.0));
  EXPECT_THAT(actual, Not(IsOptimal(35)));
}

TEST(IsOptimalTest, CheckObjectiveMissingSolution) {
  // Assuming maximization.
  // TODO(b/309658404): consider changing to finite dual bound.
  SolveResult actual{Termination::Optimal(/*primal_objective_value=*/50.0,
                                          /*dual_objective_value=*/kInf)};
  EXPECT_THAT(actual, Not(IsOptimal(42.0)));
}

TEST(IsOptimalTest, CheckObjectiveWrongObjectiveForSolution) {
  // Assuming maximization.
  // TODO(b/309658404): consider changing to finite dual bound.
  SolveResult actual{Termination::Optimal(/*primal_objective_value=*/42.0,
                                          /*dual_objective_value=*/kInf)};
  actual.solutions.push_back(
      Solution{.primal_solution = PrimalSolution{
                   .objective_value = 35.0,
                   .feasibility_status = SolutionStatus::kFeasible}});
  EXPECT_THAT(actual, Not(IsOptimal(42.0)));
}

TEST(TerminatesWithTest, Expected) {
  SolveResult actual{Termination::Infeasible(
      /*is_maximize=*/false,
      /*dual_feasibility_status=*/FeasibilityStatus::kFeasible)};
  EXPECT_THAT(actual, TerminatesWith(TerminationReason::kInfeasible));
}

TEST(TerminatesWithTest, WrongReason) {
  SolveResult actual{Termination::Infeasible(
      /*is_maximize=*/false,
      /*dual_feasibility_status=*/FeasibilityStatus::kFeasible)};
  EXPECT_THAT(actual, Not(TerminatesWith(TerminationReason::kUnbounded)));
}

TEST(TerminatesWithOneOfTest, ExpectedInList) {
  SolveResult actual{Termination::Infeasible(
      /*is_maximize=*/false,
      /*dual_feasibility_status=*/FeasibilityStatus::kFeasible)};
  EXPECT_THAT(actual, TerminatesWithOneOf({TerminationReason::kUnbounded,
                                           TerminationReason::kInfeasible}));
}

TEST(TerminatesWithOneOfTest, ExpectedNotInList) {
  SolveResult actual{Termination::Infeasible(
      /*is_maximize=*/false,
      /*dual_feasibility_status=*/FeasibilityStatus::kFeasible)};
  EXPECT_THAT(actual, Not(TerminatesWithOneOf({TerminationReason::kUnbounded,
                                               TerminationReason::kOptimal})));
}

TEST(TerminatesWithLimitTest, Expected) {
  SolveResult feasible{Termination::Feasible(/*is_maximize=*/false,
                                             Limit::kTime,
                                             /*finite_primal_objective=*/20.0)};
  EXPECT_THAT(feasible, TerminatesWithLimit(Limit::kTime));
  EXPECT_THAT(feasible, Not(TerminatesWithLimit(Limit::kIteration)));

  SolveResult no_solution_found{
      Termination::NoSolutionFound(/*is_maximize=*/false, Limit::kTime)};
  EXPECT_THAT(no_solution_found, TerminatesWithLimit(Limit::kTime));
  EXPECT_THAT(no_solution_found, Not(TerminatesWithLimit(Limit::kIteration)));
}

TEST(TerminatesWithLimitTest, AllowUndetermined) {
  SolveResult feasible{Termination::Feasible(/*is_maximize=*/false,
                                             Limit::kUndetermined,
                                             /*finite_primal_objective=*/20.0)};
  EXPECT_THAT(feasible, TerminatesWithLimit(Limit::kTime,
                                            /*allow_limit_undetermined=*/true));
  EXPECT_THAT(feasible, Not(TerminatesWithLimit(Limit::kTime)));

  SolveResult no_solution_found{Termination::NoSolutionFound(
      /*is_maximize=*/false, Limit::kUndetermined)};
  EXPECT_THAT(no_solution_found,
              TerminatesWithLimit(Limit::kTime,
                                  /*allow_limit_undetermined=*/true));
  EXPECT_THAT(no_solution_found, Not(TerminatesWithLimit(Limit::kTime)));
}

TEST(TerminatesWithReasonFeasibleTest, Expected) {
  SolveResult feasible{Termination::Feasible(/*is_maximize=*/false,
                                             Limit::kTime,
                                             /*finite_primal_objective=*/20.0)};
  EXPECT_THAT(feasible, TerminatesWithReasonFeasible(Limit::kTime));
  EXPECT_THAT(feasible, Not(TerminatesWithReasonFeasible(Limit::kIteration)));

  SolveResult no_solution_found{
      Termination::NoSolutionFound(/*is_maximize=*/false, Limit::kTime)};
  EXPECT_THAT(no_solution_found,
              Not(TerminatesWithReasonFeasible(Limit::kTime)));
}

TEST(TerminatesWithReasonFeasibleTest, AllowUndetermined) {
  SolveResult feasible{Termination::Feasible(/*is_maximize=*/false,
                                             Limit::kUndetermined,
                                             /*finite_primal_objective=*/20.0)};
  EXPECT_THAT(feasible,
              TerminatesWithReasonFeasible(Limit::kTime,
                                           /*allow_limit_undetermined=*/true));
  EXPECT_THAT(feasible, Not(TerminatesWithReasonFeasible(Limit::kTime)));

  SolveResult no_solution_found{Termination::NoSolutionFound(
      /*is_maximize=*/false, Limit::kUndetermined)};
  EXPECT_THAT(no_solution_found, Not(TerminatesWithReasonFeasible(
                                     Limit::kTime,
                                     /*allow_limit_undetermined=*/true)));
}

TEST(TerminatesWithReasonNoSolutionFoundTest, Expected) {
  SolveResult feasible{Termination::Feasible(/*is_maximize=*/false,
                                             Limit::kTime,
                                             /*finite_primal_objective=*/20.0)};
  EXPECT_THAT(feasible, Not(TerminatesWithReasonNoSolutionFound(Limit::kTime)));

  SolveResult no_solution_found{
      Termination::NoSolutionFound(/*is_maximize=*/false, Limit::kTime)};
  EXPECT_THAT(no_solution_found,
              TerminatesWithReasonNoSolutionFound(Limit::kTime));
  EXPECT_THAT(no_solution_found,
              Not(TerminatesWithReasonNoSolutionFound(Limit::kIteration)));
}

TEST(TTerminatesWithReasonNoSolutionFoundTest, AllowUndetermined) {
  SolveResult feasible{Termination::Feasible(/*is_maximize=*/false,
                                             Limit::kUndetermined,
                                             /*finite_primal_objective=*/20.0)};
  EXPECT_THAT(feasible, Not(TerminatesWithReasonNoSolutionFound(
                            Limit::kTime,
                            /*allow_limit_undetermined=*/true)));

  SolveResult no_solution_found{Termination::NoSolutionFound(
      /*is_maximize=*/false, Limit::kUndetermined)};
  EXPECT_THAT(no_solution_found, TerminatesWithReasonNoSolutionFound(
                                     Limit::kTime,
                                     /*allow_limit_undetermined=*/true));
  EXPECT_THAT(no_solution_found,
              Not(TerminatesWithReasonNoSolutionFound(Limit::kTime)));
}

TEST(HasSolution, NoSolution) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  SolveResult actual{Termination::Optimal(/*objective_value=*/10.0)};
  EXPECT_THAT(actual, Not(HasSolution(PrimalSolution{
                          .variable_values = {{x, 1.0}},
                          .objective_value = 42,
                          .feasibility_status = SolutionStatus::kFeasible})));
}

TEST(HasSolution, HasSolution) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  SolveResult actual{Termination::Optimal(/*objective_value=*/10.0)};
  actual.solutions.push_back(
      Solution{.primal_solution = PrimalSolution{
                   .variable_values = {{x, 1.0}},
                   .objective_value = 42,
                   .feasibility_status = SolutionStatus::kFeasible}});
  actual.solutions.push_back(
      Solution{.primal_solution = PrimalSolution{
                   .variable_values = {{x, 2.0}},
                   .objective_value = 42,
                   .feasibility_status = SolutionStatus::kFeasible}});
  actual.solutions.push_back(
      Solution{.primal_solution = PrimalSolution{
                   .variable_values = {{x, 0.0}},
                   .objective_value = 10,
                   .feasibility_status = SolutionStatus::kFeasible}});
  actual.solutions.push_back(
      Solution{.primal_solution = PrimalSolution{
                   .variable_values = {{x, -1.0}},
                   .objective_value = 42,
                   .feasibility_status = SolutionStatus::kInfeasible}});
  EXPECT_THAT(actual, HasSolution(PrimalSolution{
                          .variable_values = {{x, 1.0}},
                          .objective_value = 42,
                          .feasibility_status = SolutionStatus::kFeasible}));
  EXPECT_THAT(actual, Not(HasSolution(PrimalSolution{
                          .variable_values = {{x, 1.0}},
                          .objective_value = 32,
                          .feasibility_status = SolutionStatus::kFeasible})));
}

TEST(HasDualSolution, NoSolution) {
  Model model;
  const Variable x = model.AddVariable("x");
  const LinearConstraint c = model.AddLinearConstraint("c");
  SolveResult actual{Termination::Optimal(/*objective_value=*/10.0)};
  DualSolution expected;
  expected.dual_values[c] = 1;
  expected.reduced_costs[x] = 3;
  expected.objective_value = 5;
  EXPECT_THAT(actual, Not(HasDualSolution(DualSolution{
                          .dual_values = {{c, 3.0}},
                          .reduced_costs = {{x, 5}},
                          .objective_value = 42,
                          .feasibility_status = SolutionStatus::kFeasible})));
}

TEST(HasBestDualSolutionTest, HasSolution) {
  Model model;
  const Variable x = model.AddVariable("x");
  const LinearConstraint c = model.AddLinearConstraint("c");
  SolveResult actual{Termination::Optimal(/*objective_value=*/10.0)};
  actual.solutions.push_back(
      Solution{.dual_solution = DualSolution{
                   .dual_values = {{c, 3.0}},
                   .reduced_costs = {{x, 5}},
                   .objective_value = 42,
                   .feasibility_status = SolutionStatus::kFeasible}});
  actual.solutions.push_back(
      Solution{.dual_solution = DualSolution{
                   .dual_values = {{c, 2.0}},
                   .reduced_costs = {{x, 1}},
                   .objective_value = 12,
                   .feasibility_status = SolutionStatus::kFeasible}});
  EXPECT_THAT(actual, HasDualSolution(*actual.solutions[0].dual_solution));
  EXPECT_THAT(actual, HasDualSolution(*actual.solutions[1].dual_solution));
  {
    DualSolution expected = *actual.solutions[0].dual_solution;
    expected.feasibility_status = SolutionStatus::kInfeasible;
    EXPECT_THAT(actual, Not(HasDualSolution(expected)));
  }

  {
    DualSolution expected = *actual.solutions[0].dual_solution;
    expected.feasibility_status = SolutionStatus::kUndetermined;
    EXPECT_THAT(actual, Not(HasDualSolution(expected)));
  }
}

TEST(IsOptimalWithSolution, IsOptimalCorrectlyCalled) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  SolveResult actual{Termination::Optimal(/*objective_value=*/42.0)};
  actual.solutions.push_back(
      Solution{.primal_solution = PrimalSolution{
                   .variable_values = {{x, 1.0}},
                   .objective_value = 42,
                   .feasibility_status = SolutionStatus::kFeasible}});
  EXPECT_THAT(actual, Not(IsOptimalWithSolution(43, {{x, 1.0}},
                                                /*tolerance=*/0.1)));
  EXPECT_THAT(actual, IsOptimalWithSolution(43, {{x, 1.0}},
                                            /*tolerance=*/10));
  EXPECT_THAT(actual, IsOptimalWithSolution(42, {{x, 1.0}}));
}

TEST(IsOptimalWithSolution, HasSolutionCorrectlyCalled) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");
  SolveResult actual{Termination::Optimal(/*objective_value=*/42.0)};
  actual.solutions.push_back(
      Solution{.primal_solution = PrimalSolution{
                   .variable_values = {{x, 1.0}, {y, 0.0}},
                   .objective_value = 42,
                   .feasibility_status = SolutionStatus::kFeasible}});
  actual.solutions.push_back(
      Solution{.primal_solution = PrimalSolution{
                   .variable_values = {{x, 0.0}, {y, 1.0}},
                   .objective_value = 42,
                   .feasibility_status = SolutionStatus::kFeasible}});
  EXPECT_THAT(actual, IsOptimalWithSolution(42, {{x, 1.0}, {y, 0.0}}));
  EXPECT_THAT(actual, IsOptimalWithSolution(42, {{x, 0.0}, {y, 1.0}}));
  EXPECT_THAT(actual, Not(IsOptimalWithSolution(42, {{x, 0.0}, {y, 2.0}},
                                                /*tolerance=*/0.1)));
  EXPECT_THAT(actual, IsOptimalWithSolution(42, {{x, 0.0}, {y, 2.0}},
                                            /*tolerance=*/10));
  actual.solutions[1].primal_solution->feasibility_status =
      SolutionStatus::kInfeasible;
  EXPECT_THAT(actual, Not(IsOptimalWithSolution(42, {{x, 0.0}, {y, 1.0}})));
}

TEST(IsOptimalWithDualSolution, IsOptimalCorrectlyCalled) {
  Model model;
  const Variable x = model.AddVariable("x");
  const LinearConstraint c = model.AddLinearConstraint("c");
  SolveResult actual{Termination::Optimal(/*objective_value=*/42.0)};
  actual.solutions.push_back(Solution{
      .primal_solution =
          PrimalSolution{.variable_values = {{x, 1.0}},
                         .objective_value = 42,
                         .feasibility_status = SolutionStatus::kFeasible},
      .dual_solution =
          DualSolution{.dual_values = {{c, 3.0}},
                       .reduced_costs = {{x, 5}},
                       .objective_value = 42,
                       .feasibility_status = SolutionStatus::kFeasible}});
  EXPECT_THAT(actual, Not(IsOptimalWithDualSolution(43, {{c, 3.0}}, {{x, 5}},

                                                    /*tolerance=*/0.1)));
  EXPECT_THAT(actual, IsOptimalWithDualSolution(43, {{c, 3.0}}, {{x, 5}},
                                                /*tolerance=*/10));
  EXPECT_THAT(actual, IsOptimalWithDualSolution(42, {{c, 3.0}}, {{x, 5}}));
}

TEST(IsOptimalWithDualSolution, HasDualSolutionCorrectlyCalled) {
  Model model;
  const Variable x = model.AddVariable("x");
  const LinearConstraint c = model.AddLinearConstraint("c");
  const LinearConstraint d = model.AddLinearConstraint("d");
  SolveResult actual{Termination::Optimal(/*objective_value=*/42.0)};
  actual.solutions.push_back(Solution{
      .primal_solution =
          PrimalSolution{.variable_values = {{x, 1.0}},
                         .objective_value = 42,
                         .feasibility_status = SolutionStatus::kFeasible},
      .dual_solution =
          DualSolution{.dual_values = {{c, 1.0}, {d, 0.0}},
                       .reduced_costs = {{x, 5}},
                       .objective_value = 42,
                       .feasibility_status = SolutionStatus::kFeasible}});
  actual.solutions.push_back(Solution{
      .primal_solution =
          PrimalSolution{.variable_values = {{x, 1.0}},
                         .objective_value = 42,
                         .feasibility_status = SolutionStatus::kFeasible},
      .dual_solution =
          DualSolution{.dual_values = {{c, 0.0}, {d, 1.0}},
                       .reduced_costs = {{x, 5}},
                       .objective_value = 42,
                       .feasibility_status = SolutionStatus::kFeasible}});
  EXPECT_THAT(actual,
              IsOptimalWithDualSolution(42, {{c, 1.0}, {d, 0.0}}, {{x, 5}}));
  EXPECT_THAT(actual,
              IsOptimalWithDualSolution(42, {{c, 0.0}, {d, 1.0}}, {{x, 5}}));
  EXPECT_THAT(actual,
              Not(IsOptimalWithDualSolution(42, {{c, 1.0}, {d, 0.0}}, {{x, 6}},
                                            /*tolerance=*/0.1)));
  EXPECT_THAT(actual, IsOptimalWithDualSolution(42, {{c, 1.0}, {d, 0.0}},
                                                {{x, 6}}, /*tolerance=*/10));
}

TEST(IsOptimalWithDualSolutionQc, IsOptimalCorrectlyCalled) {
  Model model;
  const Variable x = model.AddVariable("x");
  const LinearConstraint c = model.AddLinearConstraint("c");
  const QuadraticConstraint d = model.AddQuadraticConstraint(x * x <= 1, "c");
  SolveResult actual{Termination::Optimal(/*objective_value=*/42.0)};
  actual.solutions.push_back(Solution{
      .primal_solution =
          PrimalSolution{.variable_values = {{x, 1.0}},
                         .objective_value = 42,
                         .feasibility_status = SolutionStatus::kFeasible},
      .dual_solution =
          DualSolution{.dual_values = {{c, 3.0}},
                       .quadratic_dual_values = {{d, 3.0}},
                       .reduced_costs = {{x, 5}},
                       .objective_value = 42,
                       .feasibility_status = SolutionStatus::kFeasible}});
  EXPECT_THAT(actual, Not(IsOptimalWithDualSolution(43, {{c, 3.0}}, {{d, 3.0}},
                                                    {{x, 5}},

                                                    /*tolerance=*/0.1)));
  EXPECT_THAT(actual,
              IsOptimalWithDualSolution(43, {{c, 3.0}}, {{d, 3.0}}, {{x, 5}},
                                        /*tolerance=*/10));
  EXPECT_THAT(actual,
              IsOptimalWithDualSolution(42, {{c, 3.0}}, {{d, 3.0}}, {{x, 5}}));
}

TEST(IsOptimalWithDualSolutionQc, HasDualSolutionCorrectlyCalled) {
  Model model;
  const Variable x = model.AddVariable("x");
  const LinearConstraint c = model.AddLinearConstraint("c");
  const LinearConstraint d = model.AddLinearConstraint("d");
  const QuadraticConstraint e = model.AddQuadraticConstraint(x * x <= 1, "e");
  SolveResult actual{Termination::Optimal(/*objective_value=*/42.0)};
  actual.solutions.push_back(Solution{
      .primal_solution =
          PrimalSolution{.variable_values = {{x, 1.0}},
                         .objective_value = 42,
                         .feasibility_status = SolutionStatus::kFeasible},
      .dual_solution =
          DualSolution{.dual_values = {{c, 1.0}, {d, 0.0}},
                       .quadratic_dual_values = {{e, 3.0}},
                       .reduced_costs = {{x, 5}},
                       .objective_value = 42,
                       .feasibility_status = SolutionStatus::kFeasible}});
  actual.solutions.push_back(Solution{
      .primal_solution =
          PrimalSolution{.variable_values = {{x, 1.0}},
                         .objective_value = 42,
                         .feasibility_status = SolutionStatus::kFeasible},
      .dual_solution =
          DualSolution{.dual_values = {{c, 0.0}, {d, 1.0}},
                       .quadratic_dual_values = {{e, 3.0}},
                       .reduced_costs = {{x, 5}},
                       .objective_value = 42,
                       .feasibility_status = SolutionStatus::kFeasible}});
  EXPECT_THAT(actual, IsOptimalWithDualSolution(42, {{c, 1.0}, {d, 0.0}},
                                                {{e, 3.0}}, {{x, 5}}));
  EXPECT_THAT(actual, IsOptimalWithDualSolution(42, {{c, 0.0}, {d, 1.0}},
                                                {{e, 3.0}}, {{x, 5}}));
  EXPECT_THAT(actual, Not(IsOptimalWithDualSolution(42, {{c, 1.0}, {d, 0.0}},
                                                    {{e, 4.0}}, {{x, 5}},
                                                    /*tolerance=*/0.1)));
  EXPECT_THAT(actual,
              IsOptimalWithDualSolution(42, {{c, 1.0}, {d, 0.0}}, {{e, 4.0}},
                                        {{x, 5}}, /*tolerance=*/10));
}

TEST(HasPrimalRayTest, NoRay) {
  Model model;
  const Variable x = model.AddVariable("x");
  SolveResult actual{Termination::Optimal(/*objective_value=*/10.0)};
  EXPECT_THAT(actual, Not(HasPrimalRay({{x, 1.0}})));
  PrimalRay expected = {.variable_values = {{x, 1.0}}};
  EXPECT_THAT(actual, Not(HasPrimalRay(expected)));
}

TEST(HasPrimalRayTest, HasRay) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");
  SolveResult actual{Termination::Optimal(/*objective_value=*/10.0)};
  PrimalRay& first_ray = actual.primal_rays.emplace_back();
  first_ray.variable_values = {{x, 1}, {y, 0}};
  PrimalRay& second_ray = actual.primal_rays.emplace_back();
  second_ray.variable_values = {{x, 1}, {y, 2}};
  EXPECT_THAT(actual, HasPrimalRay({{x, 1}, {y, 0}}));
  EXPECT_THAT(actual, HasPrimalRay({{x, 2}, {y, 0}}));
  EXPECT_THAT(actual, HasPrimalRay({{x, 1}, {y, 2}}));
  EXPECT_THAT(actual, HasPrimalRay(actual.primal_rays[0]));
  EXPECT_THAT(actual, HasPrimalRay(actual.primal_rays[1]));
  EXPECT_THAT(actual, Not(HasPrimalRay({{x, 0}, {y, 1}})));
  PrimalRay bad_ray;
  bad_ray.variable_values = {{x, 0}, {y, 1}};
  EXPECT_THAT(actual, Not(HasPrimalRay(bad_ray)));
}

TEST(HasDualRayTest, NoRay) {
  Model model;
  const Variable x = model.AddVariable("x");
  const LinearConstraint c = model.AddLinearConstraint("c");
  SolveResult actual{Termination::Optimal(/*objective_value=*/10.0)};
  DualRay expected = {.dual_values = {{c, 2}}, .reduced_costs = {{x, 1}}};
  EXPECT_THAT(actual, Not(HasDualRay(expected)));
}

TEST(HasDualRayTest, HasRay) {
  Model model;
  const Variable x = model.AddVariable("x");
  const LinearConstraint c = model.AddLinearConstraint("c");
  SolveResult actual{Termination::Optimal(/*objective_value=*/10.0)};
  DualRay& first_ray = actual.dual_rays.emplace_back();
  first_ray.dual_values[c] = 1;
  first_ray.reduced_costs[x] = 2;

  DualRay& second_ray = actual.dual_rays.emplace_back();
  second_ray.dual_values[c] = 3;
  second_ray.reduced_costs[x] = 1;

  DualRay bad_ray;
  second_ray.dual_values[c] = -3;
  second_ray.reduced_costs[x] = -3;

  EXPECT_THAT(actual, HasDualRay(actual.dual_rays[0]));
  EXPECT_THAT(actual, HasDualRay(actual.dual_rays[1]));
  EXPECT_THAT(actual, Not(HasDualRay(bad_ray)));
}

TEST(ResultsConsistentTest, SimpleOptimal) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");

  SolveResult expected{Termination::Optimal(/*objective_value=*/10.0)};
  expected.solutions.push_back(
      Solution{.primal_solution = PrimalSolution{
                   .variable_values = {{x, 1}, {y, 0}}, .objective_value = 3}});

  {
    SolveResult actual = expected;
    EXPECT_THAT(actual, IsConsistentWith(expected));
  }

  {
    SolveResult small_error = expected;
    small_error.solutions[0].primal_solution->variable_values[x] += 1e-7;
    EXPECT_THAT(small_error, IsConsistentWith(expected, {.tolerance = 1e-6}));
    EXPECT_THAT(small_error,
                Not(IsConsistentWith(expected, {.tolerance = 1e-8})));
  }

  {
    SolveResult extra_solution = expected;
    PrimalSolution solution2;
    solution2.variable_values = {{x, 0}, {y, 1}};
    solution2.objective_value = 2;
    extra_solution.solutions.push_back(Solution{.primal_solution = solution2});
    EXPECT_THAT(extra_solution,
                IsConsistentWith(expected, {.first_solution_only = true}));
    EXPECT_THAT(extra_solution, Not(IsConsistentWith(
                                    expected, {.first_solution_only = false})));
  }
}

TEST(ResultsConsistentTest, MultipleSolutions) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddBinaryVariable("y");

  SolveResult expected{Termination::Optimal(/*objective_value=*/10.0)};
  expected.solutions.push_back(
      Solution{.primal_solution = PrimalSolution{
                   .variable_values = {{x, 1}, {y, 0}}, .objective_value = 3}});
  expected.solutions.push_back(
      Solution{.primal_solution = PrimalSolution{
                   .variable_values = {{x, 0}, {y, 1}}, .objective_value = 2}});
  expected.solutions.push_back(
      Solution{.primal_solution = PrimalSolution{
                   .variable_values = {{x, 0}, {y, 0}}, .objective_value = 0}});
  {
    SolveResult actual = expected;
    EXPECT_THAT(actual,
                IsConsistentWith(expected, {.first_solution_only = false}));
    EXPECT_THAT(actual,
                IsConsistentWith(expected, {.first_solution_only = true}));
  }

  {
    SolveResult alternate{Termination::Optimal(/*objective_value=*/10.0)};
    alternate.solutions.push_back(Solution{
        .primal_solution = PrimalSolution{.variable_values = {{x, 1}, {y, 0}},
                                          .objective_value = 3}});
    alternate.solutions.push_back(Solution{
        .primal_solution = PrimalSolution{.variable_values = {{x, 1}, {y, 1}},
                                          .objective_value = -1}});
    EXPECT_THAT(alternate, Not(IsConsistentWith(
                               expected, {.first_solution_only = false})));
    EXPECT_THAT(alternate,
                IsConsistentWith(expected, {.first_solution_only = true}));
  }
}

TEST(ResultsConsistentTest, DualSolutionAndBasis) {
  Model model;
  const Variable x = model.AddContinuousVariable(0, 1, "x");
  const Variable y = model.AddContinuousVariable(0, 1, "y");
  const LinearConstraint c = model.AddLinearConstraint("c");

  SolveResult expected{Termination::Optimal(/*objective_value=*/10.0)};
  expected.solutions.push_back(Solution{
      .primal_solution = PrimalSolution{.variable_values = {{x, 1}, {y, 0}},
                                        .objective_value = 3},
      .dual_solution = DualSolution{.dual_values = {{c, 1.0}},
                                    .reduced_costs = {{x, 0.0}, {y, 1.0}},
                                    .objective_value = 3},
      .basis = Basis{.constraint_status = {{c, BasisStatus::kBasic}},
                     .variable_status = {{x, BasisStatus::kAtUpperBound},
                                         {y, BasisStatus::kAtLowerBound}}}});

  {
    SolveResult actual = expected;
    EXPECT_THAT(actual, IsConsistentWith(expected));
    EXPECT_THAT(actual, IsConsistentWith(expected, {.check_dual = false}));
    EXPECT_THAT(actual, IsConsistentWith(expected, {.check_basis = true}));
  }

  {
    SolveResult dual_missing = expected;
    dual_missing.solutions[0].dual_solution.reset();
    EXPECT_THAT(dual_missing, Not(IsConsistentWith(expected)));
    EXPECT_THAT(dual_missing,
                IsConsistentWith(expected, {.check_dual = false}));
    // Flip the roles of actual and expected, should still hold.
    EXPECT_THAT(expected, Not(IsConsistentWith(dual_missing)));
    EXPECT_THAT(expected,
                IsConsistentWith(dual_missing, {.check_dual = false}));
  }

  {
    SolveResult basis_missing = expected;
    basis_missing.solutions[0].basis.reset();
    EXPECT_THAT(basis_missing,
                Not(IsConsistentWith(expected, {.check_basis = true})));
    EXPECT_THAT(basis_missing, IsConsistentWith(expected));
    // Flip the roles of actual and expected, should still hold.
    EXPECT_THAT(expected,
                Not(IsConsistentWith(basis_missing, {.check_basis = true})));
    EXPECT_THAT(expected, IsConsistentWith(basis_missing));
  }

  {
    SolveResult extra_solution = expected;
    extra_solution.solutions.push_back(Solution{
        .dual_solution = DualSolution{.dual_values = {{c, 1.0}},
                                      .reduced_costs = {{x, 1.0}, {y, 0.0}},
                                      .objective_value = 4}});
    EXPECT_THAT(extra_solution, IsConsistentWith(expected));
    EXPECT_THAT(extra_solution, Not(IsConsistentWith(
                                    expected, {.first_solution_only = false})));
    // Flip the roles of actual and expected, should still hold.
    EXPECT_THAT(expected, IsConsistentWith(extra_solution));
    EXPECT_THAT(expected, Not(IsConsistentWith(
                              extra_solution, {.first_solution_only = false})));
  }
}

TEST(ResultsConsistentTest, Unbounded) {
  Model model;
  const Variable x = model.AddContinuousVariable(0, 1, "x");
  const Variable y = model.AddContinuousVariable(0, 1, "y");

  SolveResult expected{Termination::Unbounded(/*is_maximize=*/false)};
  expected.primal_rays.push_back({.variable_values = {{x, 1}, {y, 0}}});

  {
    SolveResult actual = expected;
    EXPECT_THAT(actual, IsConsistentWith(expected));
    EXPECT_THAT(actual, IsConsistentWith(expected, {.check_rays = false}));
    EXPECT_THAT(actual,
                IsConsistentWith(expected, {.inf_or_unb_soft_match = false}));
  }

  {
    SolveResult dual_infeasible = expected;
    dual_infeasible.termination.reason =
        TerminationReason::kInfeasibleOrUnbounded;
    EXPECT_THAT(dual_infeasible, IsConsistentWith(expected));
    EXPECT_THAT(
        dual_infeasible,
        Not(IsConsistentWith(expected, {.inf_or_unb_soft_match = false})));
    // Flip the roles of actual and expected, should still hold.
    EXPECT_THAT(expected, IsConsistentWith(dual_infeasible));
    EXPECT_THAT(expected,
                Not(IsConsistentWith(dual_infeasible,
                                     {.inf_or_unb_soft_match = false})));
  }

  {
    SolveResult with_primal = expected;
    with_primal.solutions.push_back(Solution{
        .primal_solution = PrimalSolution{.variable_values = {{x, 1}, {y, 0}},
                                          .objective_value = 3.0}});
    EXPECT_THAT(with_primal,
                Not(IsConsistentWith(
                    expected, {.check_solutions_if_inf_or_unbounded = true})));
    EXPECT_THAT(with_primal, IsConsistentWith(expected));
    // Flip the roles of actual and expected, should still hold.
    EXPECT_THAT(expected, Not(IsConsistentWith(
                              with_primal,
                              {.check_solutions_if_inf_or_unbounded = true})));
    EXPECT_THAT(expected, IsConsistentWith(with_primal));
  }

  {
    SolveResult ray_missing = expected;
    ray_missing.primal_rays.clear();
    EXPECT_THAT(ray_missing, Not(IsConsistentWith(expected)));
    EXPECT_THAT(ray_missing, IsConsistentWith(expected, {.check_rays = false}));
    // Flip the roles of actual and expected, should still hold.
    EXPECT_THAT(expected, Not(IsConsistentWith(ray_missing)));
    EXPECT_THAT(expected, IsConsistentWith(ray_missing, {.check_rays = false}));
  }

  {
    SolveResult wrong_ray = expected;
    wrong_ray.primal_rays[0].variable_values[y] = 1;
    EXPECT_THAT(wrong_ray, Not(IsConsistentWith(expected)));
    EXPECT_THAT(wrong_ray, IsConsistentWith(expected, {.check_rays = false}));
    // Flip the roles of actual and expected, should still hold.
    EXPECT_THAT(expected, Not(IsConsistentWith(wrong_ray)));
    EXPECT_THAT(expected, IsConsistentWith(wrong_ray, {.check_rays = false}));
  }
}

TEST(ResultsConsistentTest, UnboundedMultipleRays) {
  Model model;
  std::vector<Variable> xs;
  for (int i = 0; i < 4; ++i) {
    xs.push_back(model.AddContinuousVariable(0, 1, absl::StrCat("x_", i)));
  }

  SolveResult first{Termination::Unbounded(/*is_maximize=*/false)};
  SolveResult second{Termination::Unbounded(/*is_maximize=*/false)};

  for (int i = 0; i < 4; ++i) {
    PrimalRay ray;
    for (int j = 0; j < 4; ++j) {
      ray.variable_values[xs[j]] = j == i ? 1 : 0;
    }
    if (i < 3) {
      first.primal_rays.push_back(ray);
    }
    if (i > 0) {
      second.primal_rays.push_back(ray);
    }
  }

  EXPECT_THAT(first, IsConsistentWith(second));
  EXPECT_THAT(first,
              Not(IsConsistentWith(second, {.first_solution_only = false})));
  // Reverse first and second, the result should match
  EXPECT_THAT(second, IsConsistentWith(first));
  EXPECT_THAT(second,
              Not(IsConsistentWith(first, {.first_solution_only = false})));

  EXPECT_THAT(first, IsConsistentWith(first, {.first_solution_only = false}));
  EXPECT_THAT(second, IsConsistentWith(second, {.first_solution_only = false}));
}

TEST(ResultsConsistentTest, Infeasible) {
  Model model;
  const Variable x = model.AddContinuousVariable(0, 1, "x");
  const LinearConstraint c = model.AddLinearConstraint("c");

  SolveResult expected{Termination::Infeasible(
      /*is_maximize=*/false,
      /*dual_feasibility_status=*/FeasibilityStatus::kFeasible)};
  expected.dual_rays.push_back(
      {.dual_values = {{c, 1}}, .reduced_costs = {{x, 0}}});

  {
    SolveResult actual = expected;
    EXPECT_THAT(actual, IsConsistentWith(expected));
    EXPECT_THAT(actual, IsConsistentWith(expected, {.check_rays = false}));
    EXPECT_THAT(actual,
                IsConsistentWith(expected, {.inf_or_unb_soft_match = false}));
  }

  {
    SolveResult dual_infeasible = expected;
    dual_infeasible.termination.reason =
        TerminationReason::kInfeasibleOrUnbounded;
    EXPECT_THAT(dual_infeasible, IsConsistentWith(expected));
    EXPECT_THAT(
        dual_infeasible,
        Not(IsConsistentWith(expected, {.inf_or_unb_soft_match = false})));
    // Flip the roles of actual and expected, should still hold.
    EXPECT_THAT(expected, IsConsistentWith(dual_infeasible));
    EXPECT_THAT(expected,
                Not(IsConsistentWith(dual_infeasible,
                                     {.inf_or_unb_soft_match = false})));
  }

  {
    SolveResult with_dual = expected;
    with_dual.solutions.push_back(
        Solution{.dual_solution = DualSolution{.dual_values = {{c, 1}},
                                               .reduced_costs = {{x, 1}},
                                               .objective_value = 3.0}});
    EXPECT_THAT(with_dual,
                Not(IsConsistentWith(
                    expected, {.check_solutions_if_inf_or_unbounded = true})));
    EXPECT_THAT(with_dual, IsConsistentWith(expected));
    // Flip the roles of actual and expected, should still hold.
    EXPECT_THAT(expected,
                Not(IsConsistentWith(
                    with_dual, {.check_solutions_if_inf_or_unbounded = true})));
    EXPECT_THAT(expected, IsConsistentWith(with_dual));
  }

  {
    SolveResult ray_missing = expected;
    ray_missing.dual_rays.clear();
    EXPECT_THAT(ray_missing, Not(IsConsistentWith(expected)));
    EXPECT_THAT(ray_missing, IsConsistentWith(expected, {.check_rays = false}));
    // Flip the roles of actual and expected, should still hold.
    EXPECT_THAT(expected, Not(IsConsistentWith(ray_missing)));
    EXPECT_THAT(expected, IsConsistentWith(ray_missing, {.check_rays = false}));
  }

  {
    SolveResult wrong_ray = expected;
    wrong_ray.dual_rays[0].reduced_costs[x] = 1;
    EXPECT_THAT(wrong_ray, Not(IsConsistentWith(expected)));
    EXPECT_THAT(wrong_ray, IsConsistentWith(expected, {.check_rays = false}));
    // Flip the roles of actual and expected, should still hold.
    EXPECT_THAT(expected, Not(IsConsistentWith(wrong_ray)));
    EXPECT_THAT(expected, IsConsistentWith(wrong_ray, {.check_rays = false}));
  }
}

TEST(ResultsConsistentTest, InfeasibleMultipleRays) {
  Model model;
  std::vector<Variable> xs;
  for (int i = 0; i < 4; ++i) {
    xs.push_back(model.AddContinuousVariable(0, 1, absl::StrCat("x_", i)));
  }

  SolveResult first{Termination::Infeasible(
      /*is_maximize=*/false,
      /*dual_feasibility_status=*/FeasibilityStatus::kFeasible)};
  SolveResult second{Termination::Infeasible(
      /*is_maximize=*/false,
      /*dual_feasibility_status=*/FeasibilityStatus::kFeasible)};

  for (int i = 0; i < 4; ++i) {
    DualRay ray;
    for (int j = 0; j < 4; ++j) {
      ray.reduced_costs[xs[j]] = j == i ? 1 : 0;
    }
    if (i < 3) {
      first.dual_rays.push_back(ray);
    }
    if (i > 0) {
      second.dual_rays.push_back(ray);
    }
  }

  EXPECT_THAT(first, IsConsistentWith(second));
  EXPECT_THAT(first,
              Not(IsConsistentWith(second, {.first_solution_only = false})));
  // Reverse first and second, the result should match
  EXPECT_THAT(second, IsConsistentWith(first));
  EXPECT_THAT(second,
              Not(IsConsistentWith(first, {.first_solution_only = false})));

  EXPECT_THAT(first, IsConsistentWith(first, {.first_solution_only = false}));
  EXPECT_THAT(second, IsConsistentWith(second, {.first_solution_only = false}));
}

TEST(PrintToTest, SmallIdMap) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");
  VariableMap<double> vars = {{x, 1.0}, {y, 0.0}};
  std::ostringstream oss;
  PrintTo(vars, &oss);
  EXPECT_EQ(oss.str(), "{{x, 1}, {y, 0}}");
}

TEST(PrintToTest, LargeIdMap) {
  Model model;
  std::vector<Variable> xs;
  VariableMap<double> vars;
  for (int i = 0; i < 100; ++i) {
    xs.push_back(model.AddVariable(absl::StrCat("x", i)));
    vars[xs.back()] = i;
  }
  std::ostringstream oss;
  PrintTo(vars, &oss);
  EXPECT_EQ(oss.str(),
            "{{x0, 0}, {x1, 1}, {x2, 2}, {x3, 3}, {x4, 4}, {x5, 5}, {x6, 6}, "
            "{x7, 7}, {x8, 8}, {x9, 9}, ...(size=100)}");
}

TEST(PrintToTest, BasisIdMap) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");
  VariableMap<BasisStatus> vars = {{x, BasisStatus::kAtLowerBound},
                                   {y, BasisStatus::kAtUpperBound}};
  std::ostringstream oss;
  PrintTo(vars, &oss);
  EXPECT_EQ(oss.str(), "{{x, at_lower_bound}, {y, at_upper_bound}}");
}

TEST(PrintToTest, PrimalSolution) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");
  PrimalSolution solution;
  solution.variable_values = {{x, 1.0}, {y, 0.0}};
  solution.objective_value = 12;
  solution.feasibility_status = SolutionStatus::kFeasible;
  std::ostringstream oss;
  PrintTo(solution, &oss);
  EXPECT_EQ(oss.str(),
            "{variable_values: {{x, 1}, {y, 0}}, objective_value: 12, "
            "feasibility_status: feasible}");
}

TEST(PrintToTest, PrimalRay) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");
  PrimalRay ray;
  ray.variable_values = {{x, 1.0}, {y, 0.0}};
  std::ostringstream oss;
  PrintTo(ray, &oss);
  EXPECT_EQ(oss.str(), "{variable_values: {{x, 1}, {y, 0}}}");
}

TEST(PrintToTest, DualSolution) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");
  const LinearConstraint c = model.AddLinearConstraint("c");
  const QuadraticConstraint d = model.AddQuadraticConstraint(x * x <= 0.0, "d");
  DualSolution solution;
  solution.reduced_costs = {{x, 1.0}, {y, 0.0}};
  solution.dual_values = {{c, 2.0}};
  solution.quadratic_dual_values = {{d, 3.0}};
  solution.feasibility_status = SolutionStatus::kInfeasible;
  std::ostringstream oss;
  PrintTo(solution, &oss);
  EXPECT_EQ(oss.str(),
            "{dual_values: {{c, 2}}, quadratic_dual_values: {{d, 3}}, "
            "reduced_costs: {{x, 1}, {y, 0}}, objective_value: (nullopt), "
            "feasibility_status: infeasible}");
}

TEST(PrintToTest, DualRay) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");
  const LinearConstraint c = model.AddLinearConstraint("c");
  DualRay ray;
  ray.reduced_costs = {{x, 1.0}, {y, 0.0}};
  ray.dual_values = {{c, 2.0}};
  std::ostringstream oss;
  PrintTo(ray, &oss);
  EXPECT_EQ(oss.str(),
            "{dual_values: {{c, 2}}, reduced_costs: {{x, 1}, {y, 0}}}");
}

TEST(PrintToTest, Basis) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");
  const LinearConstraint c = model.AddLinearConstraint("c");
  Basis basis;
  basis.variable_status = {{x, BasisStatus::kAtUpperBound},
                           {y, BasisStatus::kAtLowerBound}};
  basis.constraint_status = {{c, BasisStatus::kAtLowerBound}};
  basis.basic_dual_feasibility = SolutionStatus::kUndetermined;
  std::ostringstream oss;
  PrintTo(basis, &oss);
  EXPECT_EQ(oss.str(),
            "{variable_status: {{x, at_upper_bound}, {y, "
            "at_lower_bound}}, "
            "constraint_status: {{c, at_lower_bound}}, "
            "basic_dual_feasibility: (undetermined)}");
}

TEST(PrintToTest, SolveResult) {
  Model model;
  const Variable x = model.AddVariable("x");
  const LinearConstraint c = model.AddLinearConstraint("c");
  SolveResult result{Termination::Feasible(
      /*is_maximize=*/false, Limit::kTime, /*finite_primal_objective=*/20.0,
      /*optional_dual_objective=*/10.0, "hit \"3\" seconds")};
  result.solve_stats.node_count = 2;
  result.solve_stats.simplex_iterations = 0;
  result.solve_stats.barrier_iterations = 0;
  result.solve_stats.first_order_iterations = 0;

  PrimalSolution primal;
  primal.objective_value = 20.0;
  primal.variable_values = {{x, 1}};
  primal.feasibility_status = SolutionStatus::kFeasible;
  DualSolution dual;
  dual.reduced_costs = {{x, 1.0}};
  dual.dual_values = {{c, 2.0}};
  dual.objective_value = 10.0;
  dual.feasibility_status = SolutionStatus::kFeasible;
  Basis basis;
  basis.variable_status = {{x, BasisStatus::kAtUpperBound}};
  basis.constraint_status = {{c, BasisStatus::kAtLowerBound}};
  basis.basic_dual_feasibility = SolutionStatus::kFeasible;
  result.solutions.push_back(Solution{.primal_solution = std::move(primal),
                                      .dual_solution = std::move(dual),
                                      .basis = std::move(basis)});

  PrimalRay primal_ray;
  primal_ray.variable_values = {{x, 2}};
  result.primal_rays.push_back(std::move(primal_ray));

  DualRay dual_ray;
  dual_ray.reduced_costs = {{x, 4.0}};
  dual_ray.dual_values = {{c, 5.0}};
  result.dual_rays.push_back(std::move(dual_ray));

  std::ostringstream oss;
  PrintTo(result, &oss);
}

TEST(IsFeasibleTest, Feasible) {
  EXPECT_THAT((ComputeInfeasibleSubsystemResult{
                  .feasibility = FeasibilityStatus::kFeasible,
                  .is_minimal = false,
              }),
              IsFeasible());

  // True .is_minimal should not match.
  EXPECT_THAT((ComputeInfeasibleSubsystemResult{
                  .feasibility = FeasibilityStatus::kFeasible,
                  .is_minimal = true,
              }),
              Not(IsFeasible()));

  // A non-empty .infeasible_subsystem should not match.
  Model model;
  const Variable x = model.AddIntegerVariable(
      /*lower_bound=*/0.0, /*upper_bound=*/5.0, "x");
  EXPECT_THAT((ComputeInfeasibleSubsystemResult{
                  .feasibility = FeasibilityStatus::kFeasible,
                  .infeasible_subsystem =
                      {
                          .variable_integrality = {x},
                      },
                  .is_minimal = true,
              }),
              Not(IsFeasible()));
}

TEST(IsFeasibleTest, Undetermined) {
  EXPECT_THAT((ComputeInfeasibleSubsystemResult{
                  .feasibility = FeasibilityStatus::kUndetermined,
                  .is_minimal = false,
              }),
              Not(IsFeasible()));
}

TEST(IsFeasibleTest, Infeasible) {
  Model model;
  const Variable x = model.AddIntegerVariable(
      /*lower_bound=*/0.0, /*upper_bound=*/5.0, "x");
  EXPECT_THAT((ComputeInfeasibleSubsystemResult{
                  .feasibility = FeasibilityStatus::kInfeasible,
                  .infeasible_subsystem =
                      {
                          .variable_integrality = {x},
                      },
                  .is_minimal = true,
              }),
              Not(IsFeasible()));
}

TEST(IsUndeterminedTest, Undetermined) {
  // The value of .is_minimal should be ignored.
  EXPECT_THAT((ComputeInfeasibleSubsystemResult{
                  .feasibility = FeasibilityStatus::kUndetermined,
                  .is_minimal = false,
              }),
              IsUndetermined());

  // True .is_minimal should not match.
  EXPECT_THAT((ComputeInfeasibleSubsystemResult{
                  .feasibility = FeasibilityStatus::kUndetermined,
                  .is_minimal = true,
              }),
              Not(IsUndetermined()));

  // A non-empty .infeasible_subsystem should not match.
  Model model;
  const Variable x = model.AddIntegerVariable(
      /*lower_bound=*/0.0, /*upper_bound=*/5.0, "x");
  EXPECT_THAT((ComputeInfeasibleSubsystemResult{
                  .feasibility = FeasibilityStatus::kUndetermined,
                  .infeasible_subsystem =
                      {
                          .variable_integrality = {x},
                      },
                  .is_minimal = true,
              }),
              Not(IsUndetermined()));
}

TEST(IsUndeterminedTest, Feasible) {
  EXPECT_THAT((ComputeInfeasibleSubsystemResult{
                  .feasibility = FeasibilityStatus::kFeasible,
                  .is_minimal = false,
              }),
              Not(IsUndetermined()));
}

TEST(IsUndeterminedTest, Infeasible) {
  Model model;
  const Variable x = model.AddIntegerVariable(
      /*lower_bound=*/0.0, /*upper_bound=*/5.0, "x");
  EXPECT_THAT((ComputeInfeasibleSubsystemResult{
                  .feasibility = FeasibilityStatus::kInfeasible,
                  .infeasible_subsystem =
                      {
                          .variable_integrality = {x},
                      },
                  .is_minimal = true,
              }),
              Not(IsUndetermined()));
}

TEST(IsInfeasibleTest, Infeasible) {
  Model model;
  const Variable x = model.AddIntegerVariable(
      /*lower_bound=*/0.0, /*upper_bound=*/5.0, "x");

  EXPECT_THAT((ComputeInfeasibleSubsystemResult{
                  .feasibility = FeasibilityStatus::kInfeasible,
                  .infeasible_subsystem =
                      {
                          .variable_integrality = {x},
                      },
                  .is_minimal = true,
              }),
              IsInfeasible());

  // Same with .is_minimal = false.
  EXPECT_THAT((ComputeInfeasibleSubsystemResult{
                  .feasibility = FeasibilityStatus::kInfeasible,
                  .infeasible_subsystem =
                      {
                          .variable_integrality = {x},
                      },
                  .is_minimal = false,
              }),
              IsInfeasible());

  // Empty .infeasible_subsystem should not match.
  EXPECT_THAT((ComputeInfeasibleSubsystemResult{
                  .feasibility = FeasibilityStatus::kInfeasible,
                  .is_minimal = true,
              }),
              Not(IsInfeasible()));

  // Testing .expected_is_minimal.
  EXPECT_THAT((ComputeInfeasibleSubsystemResult{
                  .feasibility = FeasibilityStatus::kInfeasible,
                  .infeasible_subsystem = {.variable_integrality = {x}},
                  .is_minimal = false,
              }),
              IsInfeasible(/*expected_is_minimal=*/false));
  EXPECT_THAT((ComputeInfeasibleSubsystemResult{
                  .feasibility = FeasibilityStatus::kInfeasible,
                  .infeasible_subsystem = {.variable_integrality = {x}},
                  .is_minimal = false,
              }),
              Not(IsInfeasible(/*expected_is_minimal=*/true)));
  EXPECT_THAT((ComputeInfeasibleSubsystemResult{
                  .feasibility = FeasibilityStatus::kInfeasible,
                  .infeasible_subsystem = {.variable_integrality = {x}},
                  .is_minimal = true,
              }),
              Not(IsInfeasible(/*expected_is_minimal=*/false)));
  EXPECT_THAT((ComputeInfeasibleSubsystemResult{
                  .feasibility = FeasibilityStatus::kInfeasible,
                  .infeasible_subsystem = {.variable_integrality = {x}},
                  .is_minimal = true,
              }),
              IsInfeasible(/*expected_is_minimal=*/true));

  // Testing .expected_infeasible_subsystem.
  EXPECT_THAT((ComputeInfeasibleSubsystemResult{
                  .feasibility = FeasibilityStatus::kInfeasible,
                  .infeasible_subsystem =
                      {
                          .variable_integrality = {x},
                      },
                  .is_minimal = false,
              }),
              IsInfeasible(/*expected_is_minimal=*/std::nullopt,
                           /*expected_infeasible_subsystem=*/ModelSubset{
                               .variable_integrality = {x},
                           }));
}

TEST(IsInfeasibleTest, Feasible) {
  EXPECT_THAT((ComputeInfeasibleSubsystemResult{
                  .feasibility = FeasibilityStatus::kFeasible,
                  .is_minimal = false,
              }),
              Not(IsInfeasible()));
}

TEST(IsInfeasibleTest, Undetermined) {
  EXPECT_THAT((ComputeInfeasibleSubsystemResult{
                  .feasibility = FeasibilityStatus::kUndetermined,
                  .is_minimal = false,
              }),
              Not(IsInfeasible()));
}

}  // namespace
}  // namespace operations_research::math_opt
