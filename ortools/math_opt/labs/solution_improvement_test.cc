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

#include "ortools/math_opt/labs/solution_improvement.h"

#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/status_builder.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/util/fp_roundtrip_conv.h"
#include "ortools/util/status_macros.h"

namespace operations_research::math_opt {
namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

using ::testing::Each;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

// Optimization direction of the objective.
enum class OptimizationDirection { kMinimize, kMaximize };

// Swaps the constraint coefficients signs and the constraint bounds and signs.
//
// For example:
//   l <= a * x + b * y <= u
// is modified in:
//   -u <= -a * x - b * y <= -l
void SwapConstraintSign(Model& model, const LinearConstraint c) {
  const double lb = c.lower_bound();
  const double ub = c.upper_bound();
  model.set_lower_bound(c, -ub);
  model.set_upper_bound(c, -lb);
  for (const Variable v : model.RowNonzeros(c)) {
    model.set_coefficient(c, v, -model.coefficient(c, v));
  }
}

// Swaps the objective's direction and the sign of every variable coefficient.
void SwapObjectiveDirectionAndSign(Model& model) {
  model.set_is_maximize(!model.is_maximize());
  for (const Variable v : model.Variables()) {
    model.set_objective_coefficient(v, -model.objective_coefficient(v));
  }
}

// Returns the sorted ids of the linear constraints of the model.
std::vector<LinearConstraintId> SortedLinearConstraintIds(const Model& model) {
  std::vector<LinearConstraintId> ret;
  for (const LinearConstraint c : model.SortedLinearConstraints()) {
    ret.push_back(c.typed_id());
  }
  return ret;
}

// Returns all equivalent models by applying all combinations of
// SwapObjectiveDirectionAndSign() and SwapConstraintSign().
//
// The models are sorted in this order:
// * (original objective, original constraint 0, original constraint 1...)
// * (original objective, original constraint 0, swapped constraint 1...)
// * ...
// * (original objective, swapped constraint 0, original constraint 1...)
// * (original objective, swapped constraint 0, swapped constraint 1...)
// * ...
// * (swapped objective, original constraint 0, original constraint 1...)
// * (swapped objective, original constraint 0, swapped constraint 1...)
// * ...
// * (swapped objective, swapped constraint 0, original constraint 1...)
// * (swapped objective, swapped constraint 0, swapped constraint 1...)
// * ...
//
// This is used to tests all combinations of objective direction and coefficient
// signs from only two base models. We need only two base models; one with a
// maximization and a positive objective coefficient. One with a maximization
// and a negative objective coefficient. We can then combine
// SwapConstraintSign() and SwapObjectiveDirectionAndSign() to cover all
// remaining cases.
//
// Here a quick summary table where we use SODAS() for
// SwapObjectiveDirectionAndSign() and SCS() for SwapConstraintSign() with a
// model with a single objective and single constraint illustrating how we cover
// the entire test matrix with only two models a and b:
//
// | case          | dir |      coefficient       |
// |               |     | objective | constraint |
// |---------------+-----+-----------+------------+
// | a             | max |     +     |     +      |
// | SCS(a)        | max |     +     |     -      |
// | b             | max |     -     |     +      |
// | SCS(b)        | max |     -     |     -      |
// | SODAS(b)      | min |     +     |     +      |
// | SCS(SODAS(b)) | min |     +     |     -      |
// | SODAS(a)      | min |     -     |     +      |
// | SCS(SODAS(a)) | min |     -     |     -      |
std::vector<std::unique_ptr<Model>> AllEquivalentModels(const Model& model) {
  // We use LinearConstraintId instead of LinearConstraint as LinearConstraint
  // is model specific and is not compatible with cloned models.
  const std::vector<LinearConstraintId> sorted_constraints =
      SortedLinearConstraintIds(model);

  // The recursion_on_constraints appends to `models` all combinations of the
  // input model with constraints swapped with SwapConstraintSign().
  std::vector<std::unique_ptr<Model>> models;
  std::function<void(std::unique_ptr<Model>, int)> recursion_on_constraints;
  recursion_on_constraints = [&](std::unique_ptr<Model> model,
                                 const int sorted_constraints_index) {
    // We reached a leaf of the decision tree.
    if (sorted_constraints_index == sorted_constraints.size()) {
      models.push_back(std::move(model));
      return;
    }

    // We prepare a clone of the input model with the constraint swapped; we
    // must do that before calling the recursion with the input `model` as it
    // will be moved.
    std::unique_ptr<Model> clone_with_swapped_constraint = model->Clone();
    SwapConstraintSign(*clone_with_swapped_constraint,
                       clone_with_swapped_constraint->linear_constraint(
                           sorted_constraints.at(sorted_constraints_index)));

    recursion_on_constraints(std::move(model), sorted_constraints_index + 1);
    recursion_on_constraints(std::move(clone_with_swapped_constraint),
                             sorted_constraints_index + 1);
  };

  recursion_on_constraints(model.Clone(), 0);
  {  // Limit scope of clone_with_swapped_objective.
    std::unique_ptr<Model> clone_with_swapped_objective = model.Clone();
    SwapObjectiveDirectionAndSign(*clone_with_swapped_objective);
    recursion_on_constraints(std::move(clone_with_swapped_objective), 0);
  }

  return models;
}

// Returns a new map where variables have been converted to ones of `to_model`
// with the same Variable::typed_id().
//
// This function is typically used to convert a map from a given model to a
// clone of this model.
VariableMap<double> SwitchModel(const Model& to_model,
                                const VariableMap<double>& input) {
  VariableMap<double> ret;
  for (const auto& [v, value] : input) {
    CHECK(ret.try_emplace(to_model.variable(v.typed_id()), value).second);
  }
  return ret;
}

// Returns SwitchModel(input.value()) when the input status is OK, else returns
// the failing status.
absl::StatusOr<VariableMap<double>> SwitchModel(
    const Model& to_model, const absl::StatusOr<VariableMap<double>>& input) {
  ABSL_ASSIGN_OR_RETURN(const VariableMap<double> input_values, input);
  return SwitchModel(to_model, input_values);
}

// Returns a new vector where variables have been converted to ones of
// `to_model` with the same Variable::typed_id().
//
// This function is typically used to convert a set of variables from a given
// model to a clone of this model.
std::vector<Variable> SwitchModel(const Model& to_model,
                                  absl::Span<const Variable> variables) {
  std::vector<Variable> ret;
  for (const Variable v : variables) {
    ret.push_back(to_model.variable(v.typed_id()));
  }
  return ret;
}

// Calls MoveVariablesToTheirBestFeasibleValue() with all models returned by
// AllEquivalentModels() and the input values. It returns the result of calling
// the function with one element per model.
//
// The return values are VariableMap<double> that get recreated with the
// Variable objects from the input `model` even though
// MoveVariablesToTheirBestFeasibleValue() is called with cloned of this
// model. Thus the Variable handles of the input `model` can be used in
// matchers.
std::vector<absl::StatusOr<VariableMap<double>>>
AllMoveVariablesToTheirBestFeasibleValue(
    const Model& model, const VariableMap<double>& input_solution,
    absl::Span<const Variable> variables,
    const MoveVariablesToTheirBestFeasibleValueOptions& options = {}) {
  std::vector<absl::StatusOr<VariableMap<double>>> ret;
  for (const std::unique_ptr<Model>& sub_model : AllEquivalentModels(model)) {
    VariableMap<double> sub_model_input_solution;
    for (const auto& [v, value] : input_solution) {
      sub_model_input_solution.try_emplace(sub_model->variable(v.typed_id()),
                                           value);
    }
    ret.push_back(SwitchModel(
        model, MoveVariablesToTheirBestFeasibleValue(
                   *sub_model, SwitchModel(*sub_model, input_solution),
                   SwitchModel(*sub_model, variables), options)));
  }
  return ret;
}

// Returns the value for the given variable in the solution, or an error if the
// value is not found.
absl::StatusOr<double> VariableValue(const VariableMap<double>& solution,
                                     const Variable variable) {
  const auto found = solution.find(variable);
  if (found == solution.end()) {
    return ortools::FailedPreconditionErrorBuilder()
           << "no value for variable " << variable;
  }
  return found->second;
}

// Returns the value of `variable` in `new_solution`.
//
// Returns an error if:
// * initial_solution or new_solution do not have a value for every variables
// * new_solution.at(v) != input_solution.at(v) for all v != variable
absl::StatusOr<double> NewValueOfVariable(
    const Model& model, const Variable variable,
    const VariableMap<double>& initial_solution,
    const VariableMap<double>& new_solution) {
  for (const Variable v : model.SortedVariables()) {
    OR_ASSIGN_OR_RETURN3(const double new_value, VariableValue(new_solution, v),
                         _ << "invalid new_solution");
    OR_ASSIGN_OR_RETURN3(const double initial_value,
                         VariableValue(initial_solution, v),
                         _ << "invalid initial_solution");
    if (v != variable && new_value != initial_value) {
      return ortools::FailedPreconditionErrorBuilder()
             << "variable " << v << " value changed to "
             << RoundTripDoubleFormat(new_value)
             << ", was: " << RoundTripDoubleFormat(initial_value);
    }
  }

  OR_ASSIGN_OR_RETURN3(const double new_value,
                       VariableValue(new_solution, variable),
                       _ << "invalid new_solution");
  return new_value;
}

// Shortcut for AllMoveVariablesToTheirBestFeasibleValue() with a
// single variable for `variables`, thus returning a single `double` value
// instead of a map for each model.
//
// It checks that the result contains indeed all variables and that one the
// input variable values have changed.
std::vector<absl::StatusOr<double>>
AllMoveVariablesToTheirBestFeasibleValueOneVar(
    const Model& model, const VariableMap<double>& input_solution,
    const Variable variable,
    const MoveVariablesToTheirBestFeasibleValueOptions& options = {}) {
  const std::vector<absl::StatusOr<VariableMap<double>>> results =
      AllMoveVariablesToTheirBestFeasibleValue(model, input_solution,
                                               /*variables=*/{variable},
                                               /*options=*/options);
  std::vector<absl::StatusOr<double>> ret;
  for (const absl::StatusOr<VariableMap<double>>& result : results) {
    ret.push_back([&]() -> absl::StatusOr<double> {
      ABSL_RETURN_IF_ERROR(result.status());
      OR_ASSIGN_OR_RETURN3(
          const double new_value,
          NewValueOfVariable(model, variable,
                             /*initial_solution=*/input_solution,
                             /*new_solution=*/result.value()),
          _ << "invalid result of MoveVariablesToTheirBestFeasibleValue()");
      return new_value;
    }());
  }
  return ret;
}

// Returns a matcher for the Status returned by
// MoveVariablesToTheirBestFeasibleValue() when the input model is unbounded for
// one of the variable.
auto StatusIsUnbounded() {
  return StatusIs(absl::StatusCode::kFailedPrecondition,
                  HasSubstr("unbounded"));
}

TEST(MoveVariablesToTheirBestFeasibleValueTest, EmptyModel) {
  const Model empty_model;
  EXPECT_THAT(MoveVariablesToTheirBestFeasibleValue(
                  empty_model, /*input_solution=*/{}, /*variables=*/{}),
              IsOkAndHolds(IsEmpty()));
}

TEST(MoveVariablesToTheirBestFeasibleValueTest, UnsupportedFeature) {
  // Here we don't test all unsupported features as we expect ModelIsSupported()
  // to work.
  Model model;
  const Variable x = model.AddContinuousVariable(
      /*lower_bound=*/-3.0, /*upper_bound=*/5.0, "x");
  // Quadratic objective, which should not be supported.
  model.Maximize(2.0 * x * x);

  EXPECT_THAT(MoveVariablesToTheirBestFeasibleValue(
                  model, /*input_solution=*/{{x, 1.0}}, /*variables=*/{x}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("quadratic objectives")));
}

TEST(MoveVariablesToTheirBestFeasibleValueTest, InvalidIntegralityTolerance) {
  Model model;

  EXPECT_THAT(MoveVariablesToTheirBestFeasibleValue(
                  model, /*input_solution=*/{}, /*variables=*/{}, /*options=*/
                  {
                      .integrality_tolerance = std::nextafter(0.0, -1.0),
                  }),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("integrality_tolerance")));
  EXPECT_THAT(MoveVariablesToTheirBestFeasibleValue(
                  model, /*input_solution=*/{}, /*variables=*/{}, /*options=*/
                  {
                      .integrality_tolerance = std::nextafter(0.25, 1.0),
                  }),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("integrality_tolerance")));
  EXPECT_THAT(MoveVariablesToTheirBestFeasibleValue(
                  model, /*input_solution=*/{}, /*variables=*/{}, /*options=*/
                  {
                      .integrality_tolerance = kNaN,
                  }),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("integrality_tolerance")));
}

TEST(MoveVariablesToTheirBestFeasibleValueTest,
     InfiniteVariableValueInSolution) {
  Model model;
  const Variable x = model.AddContinuousVariable(
      /*lower_bound=*/-kInf, /*upper_bound=*/kInf, "x");
  EXPECT_THAT(
      MoveVariablesToTheirBestFeasibleValue(
          model, /*input_solution=*/{{x, kInf}}, /*variables=*/{}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("finite")));
}

TEST(MoveVariablesToTheirBestFeasibleValueTest, NaNVariableValueInSolution) {
  Model model;
  const Variable x = model.AddContinuousVariable(
      /*lower_bound=*/-kInf, /*upper_bound=*/kInf, "x");
  EXPECT_THAT(
      MoveVariablesToTheirBestFeasibleValue(
          model, /*input_solution=*/{{x, kNaN}}, /*variables=*/{}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("finite")));
}

TEST(MoveVariablesToTheirBestFeasibleValueTest, MissingValueInSolution) {
  Model model;
  model.AddContinuousVariable(
      /*lower_bound=*/-kInf, /*upper_bound=*/kInf, "x");
  EXPECT_THAT(
      MoveVariablesToTheirBestFeasibleValue(model, /*input_solution=*/{},
                                            /*variables=*/{}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("solution")));
}

TEST(MoveVariablesToTheirBestFeasibleValueTest, ValueFromOtherModelInSolution) {
  Model model;
  const Variable x = model.AddContinuousVariable(
      /*lower_bound=*/-kInf, /*upper_bound=*/kInf, "x");
  Model other_model;
  const Variable other_x = other_model.AddContinuousVariable(
      /*lower_bound=*/-kInf, /*upper_bound=*/kInf, "x");
  EXPECT_THAT(MoveVariablesToTheirBestFeasibleValue(
                  model, /*input_solution=*/{{x, 2.0}, {other_x, 3.0}},
                  /*variables=*/{}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("different model")));
}

TEST(MoveVariablesToTheirBestFeasibleValueTest,
     ValueFromOtherModelInVariables) {
  Model model;
  const Variable x = model.AddContinuousVariable(
      /*lower_bound=*/-kInf, /*upper_bound=*/kInf, "x");
  Model other_model;
  const Variable other_x = other_model.AddContinuousVariable(
      /*lower_bound=*/-kInf, /*upper_bound=*/kInf, "x");
  EXPECT_THAT(
      MoveVariablesToTheirBestFeasibleValue(
          model, /*input_solution=*/{{x, 2.0}}, /*variables=*/{other_x}),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("different model")));
}

TEST(MoveVariablesToTheirBestFeasibleValueTest, VariableWithNaNLowerBound) {
  Model model;
  const Variable x = model.AddContinuousVariable(
      /*lower_bound=*/kNaN, /*upper_bound=*/kInf, "x");
  EXPECT_THAT(MoveVariablesToTheirBestFeasibleValue(
                  model, /*input_solution=*/{{x, 2.0}}, /*variables=*/{x}),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("NaN")));
}

TEST(MoveVariablesToTheirBestFeasibleValueTest, VariableWithNaNUpperBound) {
  Model model;
  const Variable x = model.AddContinuousVariable(
      /*lower_bound=*/-kInf, /*upper_bound=*/kNaN, "x");
  EXPECT_THAT(MoveVariablesToTheirBestFeasibleValue(
                  model, /*input_solution=*/{{x, 2.0}}, /*variables=*/{x}),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("NaN")));
}

TEST(MoveVariablesToTheirBestFeasibleValueTest, VariableWithCrossingBounds) {
  Model model;
  const Variable x = model.AddContinuousVariable(
      /*lower_bound=*/3.25, /*upper_bound=*/3.0, "x");
  EXPECT_THAT(
      MoveVariablesToTheirBestFeasibleValue(
          model, /*input_solution=*/{{x, 2.0}}, /*variables=*/{x}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("inverted")));
}

TEST(MoveVariablesToTheirBestFeasibleValueTest,
     VariableWithEmptyIntegerBounds) {
  {
    Model model;
    const Variable x = model.AddIntegerVariable(
        /*lower_bound=*/3.25, /*upper_bound=*/3.75, "x");
    EXPECT_THAT(MoveVariablesToTheirBestFeasibleValue(
                    model, /*input_solution=*/{{x, 2.0}}, /*variables=*/{x}),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr("no integer value")));
  }

  // Same test with bounds close to integers.
  {
    Model model;
    const Variable x = model.AddIntegerVariable(
        /*lower_bound=*/3.0001, /*upper_bound=*/3.9999, "x");
    const Variable y = model.AddIntegerVariable(
        /*lower_bound=*/3.0001, /*upper_bound=*/3.9999, "y");
    model.Maximize(x - y);

    // With zero tolerance, we should consider them empty.
    EXPECT_THAT(
        MoveVariablesToTheirBestFeasibleValue(
            model, /*input_solution=*/{{x, 3.5}, {y, 3.5}}, /*variables=*/{x}),
        StatusIs(absl::StatusCode::kInvalidArgument,
                 HasSubstr("no integer value")));

    // With 1e-3 tolerance, we should consider them ok.
    EXPECT_THAT(MoveVariablesToTheirBestFeasibleValue(
                    model, /*input_solution=*/{{x, 3.5}, {y, 3.5}},
                    /*variables=*/{x, y}, {.integrality_tolerance = 1e-3}),
                IsOkAndHolds(IsNear({{x, 4.0}, {y, 3.0}}, /*tolerance=*/0)));
  }
}

// A basic model with a single variable x and no constraints.
struct OneVariable {
  double x_lower_bound = -kInf;
  double x_upper_bound = kInf;
  bool x_is_integer = false;

  OptimizationDirection direction = OptimizationDirection::kMinimize;
  double obj_coeff = 0.0;
};

// Returns the result of MoveVariablesToTheirBestFeasibleValue() with the all
// models returned by AllEquivalentModels() based on the model described by the
// OneVariable and the provided value for the variable x in the input_solution.
std::vector<absl::StatusOr<double>>
AllMoveVariablesToTheirBestFeasibleValueOneVar(
    const OneVariable& data, const double x_value,
    const MoveVariablesToTheirBestFeasibleValueOptions& options = {}) {
  Model model;
  const Variable x = model.AddVariable(
      /*lower_bound=*/data.x_lower_bound, /*upper_bound=*/data.x_upper_bound,
      /*is_integer=*/data.x_is_integer, "x");

  model.SetObjective(
      data.obj_coeff * x,
      /*is_maximize=*/data.direction == OptimizationDirection::kMaximize);

  return AllMoveVariablesToTheirBestFeasibleValueOneVar(
      model, /*input_solution=*/{{x, x_value}}, x, options);
}

TEST(MoveVariablesToTheirBestFeasibleValueTest, OneDualBoundedVariable) {
  // See comment of AllMoveVariablesToTheirBestFeasibleValueOneVar() to
  // understand this test structure.

  // Maximization and positive object coefficient.
  {
    const OneVariable model = {
        .x_lower_bound = -3.0,
        .x_upper_bound = 5.0,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = 3.0,
    };

    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.0),
                Each(IsOkAndHolds(5.0)));
    // Keep out-of-bounds value.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 6.0),
                Each(IsOkAndHolds(6.0)));
    // Below the lower-bound we still saturate to the upper-bound.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -4.0),
                Each(IsOkAndHolds(5.0)));
  }

  // Maximization and negative object coefficient.
  {
    const OneVariable model = {
        .x_lower_bound = -3.0,
        .x_upper_bound = 5.0,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = -3.0,
    };

    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.0),
                Each(IsOkAndHolds(-3.0)));
    // Keep out-of-bounds value.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -4.0),
                Each(IsOkAndHolds(-4.0)));
    // Above the upper-bound we still saturate to the lower-bound.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 10.0),
                Each(IsOkAndHolds(-3.0)));
  }

  // Nothing if the variable's objective coefficient is 0.
  {
    const OneVariable model = {
        .x_lower_bound = -3.0,
        .x_upper_bound = 5.0,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = 0.0,
    };

    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.25),
                Each(IsOkAndHolds(1.25)));
    // Keep out-of-bounds value.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 6.0),
                Each(IsOkAndHolds(6.0)));
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -4.0),
                Each(IsOkAndHolds(-4.0)));
  }
}

TEST(MoveVariablesToTheirBestFeasibleValueTest, OneDualBoundedIntegerVariable) {
  // See comment of AllMoveVariablesToTheirBestFeasibleValueOneVar() to
  // understand this test structure.

  // Maximization and positive object coefficient.
  {
    const OneVariable model = {
        .x_lower_bound = -3.0,
        .x_upper_bound = 4.75,
        .x_is_integer = true,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = 3.0,
    };

    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.0),
                Each(IsOkAndHolds(4.0)));
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 4.0),
                Each(IsOkAndHolds(4.0)));
    // Keep out-of-bounds value; including their fractional part.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 4.5),
                Each(IsOkAndHolds(4.5)));
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 4.75),
                Each(IsOkAndHolds(4.75)));
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 6.25),
                Each(IsOkAndHolds(6.25)));
    // Below the lower-bound we still saturate to the upper-bound.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -4.0),
                Each(IsOkAndHolds(4.0)));
  }

  // Maximization and negative object coefficient.
  {
    const OneVariable model = {
        .x_lower_bound = -3.25,
        .x_upper_bound = 5.0,
        .x_is_integer = true,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = -3.0,
    };

    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.0),
                Each(IsOkAndHolds(-3.0)));
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -3.0),
                Each(IsOkAndHolds(-3.0)));
    // Keep out-of-bounds value; including their fractional part.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -3.125),
                Each(IsOkAndHolds(-3.125)));
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -3.25),
                Each(IsOkAndHolds(-3.25)));
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -4.125),
                Each(IsOkAndHolds(-4.125)));
    // Above the upper-bound we still saturate to the lower-bound.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 10.0),
                Each(IsOkAndHolds(-3.0)));
  }

  // Nothing if the variable's objective coefficient is 0.
  {
    const OneVariable model = {
        .x_lower_bound = -3.0,
        .x_upper_bound = 4.75,
        .x_is_integer = true,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = 0.0,
    };

    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.0),
                Each(IsOkAndHolds(1.0)));
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 3.75),
                Each(IsOkAndHolds(3.75)));
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 4.0),
                Each(IsOkAndHolds(4.0)));
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 4.5),
                Each(IsOkAndHolds(4.5)));
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 4.75),
                Each(IsOkAndHolds(4.75)));
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 6.25),
                Each(IsOkAndHolds(6.25)));
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -4.0),
                Each(IsOkAndHolds(-4.0)));
  }

  // Test with a fractional upper-bound close to an integer value.
  {
    const OneVariable model = {
        .x_lower_bound = -3.0,
        .x_upper_bound = 4.9999,
        .x_is_integer = true,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = 3.0,
    };

    // With default integrality_tolerance of 0.0, we consider the upper-bound to
    // be 4.0
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.0),
                Each(IsOkAndHolds(4.0)));
    // With a tolerance of 1e-3 we should consider the bound to be 5.0
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(
                    model, 1.0, {.integrality_tolerance = 1e-3}),
                Each(IsOkAndHolds(5.0)));
  }

  // Same test with the lower-bound
  {
    const OneVariable model = {
        .x_lower_bound = -3.9999,
        .x_upper_bound = 5.0,
        .x_is_integer = true,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = -3.0,
    };

    // With default integrality_tolerance of 0.0, we consider the upper-bound to
    // be -3.0
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.0),
                Each(IsOkAndHolds(-3.0)));
    // With a tolerance of 1e-3 we should consider the bound to be -4.0
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(
                    model, 1.0, {.integrality_tolerance = 1e-3}),
                Each(IsOkAndHolds(-4.0)));
  }
}

TEST(MoveVariablesToTheirBestFeasibleValueTest, OneUnboundedVariable) {
  // See comment of AllMoveVariablesToTheirBestFeasibleValueOneVar() to
  // understand this test structure.

  // Maximization and positive object coefficient.
  {
    const OneVariable model = {
        .x_lower_bound = -3.0,
        .x_upper_bound = kInf,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = 3.0,
    };

    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.0),
                Each(StatusIsUnbounded()));
    // Below the lower-bound we still report unbounded.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -4.0),
                Each(StatusIsUnbounded()));
  }

  // Maximization and negative object coefficient.
  {
    const OneVariable model = {
        .x_lower_bound = -kInf,
        .x_upper_bound = 5.0,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = -3.0,
    };

    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.0),
                Each(StatusIsUnbounded()));
    // Above the upper-bound we still report unbounded.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 10.0),
                Each(StatusIsUnbounded()));
  }

  // Nothing if the variable's objective coefficient is 0.
  {
    const OneVariable model = {
        .x_lower_bound = -3.0,
        .x_upper_bound = kInf,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = 0.0,
    };

    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.0),
                Each(IsOkAndHolds(1.0)));
    // Out of bounds are kept as well.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -4.0),
                Each(IsOkAndHolds(-4.0)));
  }
}

TEST(MoveVariablesToTheirBestFeasibleValueTest, OneUnboundedIntegerVariable) {
  // See comment of AllMoveVariablesToTheirBestFeasibleValueOneVar() to
  // understand this test structure.

  // Maximization and positive object coefficient.
  {
    const OneVariable model = {
        .x_lower_bound = -3.0,
        .x_upper_bound = kInf,
        .x_is_integer = true,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = 3.0,
    };

    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.0),
                Each(StatusIsUnbounded()));
    // Below the lower-bound we still report unbounded.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -4.0),
                Each(StatusIsUnbounded()));
  }

  // Maximization and negative object coefficient.
  {
    const OneVariable model = {
        .x_lower_bound = -kInf,
        .x_upper_bound = 5.0,
        .x_is_integer = true,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = -3.0,
    };

    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.0),
                Each(StatusIsUnbounded()));
    // Above the upper-bound we still report unbounded.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 10.0),
                Each(StatusIsUnbounded()));
  }

  // Nothing if the variable's objective coefficient is 0.
  {
    const OneVariable model = {
        .x_lower_bound = -3.0,
        .x_upper_bound = kInf,
        .x_is_integer = true,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = 0.0,
    };

    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.25),
                Each(IsOkAndHolds(1.25)));
    // Below the lower-bound we still report unbounded.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -4.0),
                Each(IsOkAndHolds(-4.0)));
  }
}

// A basic model with a single variable x and a single constraint c.
struct OneVariableOneConstraint {
  double x_lower_bound = -kInf;
  double x_upper_bound = kInf;
  bool x_is_integer = false;

  double c_lower_bound = -kInf;
  double c_upper_bound = kInf;
  double c_coeff = 0.0;

  OptimizationDirection direction = OptimizationDirection::kMinimize;
  double obj_coeff = 0.0;
};

// Returns the result of MoveVariablesToTheirBestFeasibleValue() with the all
// models returned by AllEquivalentModels() based on the model described by the
// input OneVariableOneConstraint and the provided value for the variable x in
// the input_solution.
std::vector<absl::StatusOr<double>>
AllMoveVariablesToTheirBestFeasibleValueOneVar(
    const OneVariableOneConstraint& data, const double x_value) {
  Model model;
  const Variable x = model.AddVariable(
      /*lower_bound=*/data.x_lower_bound, /*upper_bound=*/data.x_upper_bound,
      /*is_integer=*/data.x_is_integer, "x");
  model.AddLinearConstraint(
      data.c_lower_bound <= data.c_coeff * x <= data.c_upper_bound, "c");

  model.SetObjective(
      data.obj_coeff * x,
      /*is_maximize=*/data.direction == OptimizationDirection::kMaximize);

  return AllMoveVariablesToTheirBestFeasibleValueOneVar(
      model, /*input_solution=*/{{x, x_value}}, x);
}

// This test validate cases where a variable growth is more limited by a
// constraint with a finite bound.
TEST(MoveVariablesToTheirBestFeasibleValueTest,
     OneVariableOneLimitingConstraint) {
  // See comment of AllMoveVariablesToTheirBestFeasibleValueOneVar() to
  // understand this test structure.

  // Maximization with positive objective coefficient.
  {
    const OneVariableOneConstraint model = {
        .x_lower_bound = -3.0,
        .x_upper_bound = 5.0,
        .c_lower_bound = 3.0 * -1.0,
        .c_upper_bound = 3.0 * 2.0,
        .c_coeff = 3.0,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = 3.0,
    };
    // The variable can be improved.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.0),
                Each(IsOkAndHolds(2.0)));
    // If the constraint's bound in the direction of the objective's improvement
    // is not feasible, keep the variable's initial value.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 3.25),
                Each(IsOkAndHolds(3.25)));
    // If this is the other bound we still will change the variable value (here
    // -2.0 < -1.0 which breaks the constraint).
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -2.0),
                Each(IsOkAndHolds(2.0)));
    // Same but we also break the variable's lower-bound.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -5.0),
                Each(IsOkAndHolds(2.0)));
  }

  // Maximization with negative objective coefficient.
  {
    const OneVariableOneConstraint model = {
        .x_lower_bound = -3.0,
        .x_upper_bound = 5.0,
        .c_lower_bound = 3.0 * -1.0,
        .c_upper_bound = 3.0 * 2.0,
        .c_coeff = 3.0,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = -2.5,
    };
    // The variable can be improved.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.0),
                Each(IsOkAndHolds(-1.0)));
    // If the constraint's bound in the direction of the objective's improvement
    // is not feasible, keep the variable's initial value.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -2.25),
                Each(IsOkAndHolds(-2.25)));
    // If this is the other bound we still will change the variable value (here
    // 3.0 > 2.0 which breaks the constraint).
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 3.0),
                Each(IsOkAndHolds(-1.0)));
    // Same but we also break the variable's upper-bound.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 8.0),
                Each(IsOkAndHolds(-1.0)));
  }
}

// This test validate cases where an integer variable growth is more limited by
// a constraint with a finite bound (with non-zero fractional part).
TEST(MoveVariablesToTheirBestFeasibleValueTest,
     OneIntegerVariableOneLimitingConstraint) {
  // See comment of AllMoveVariablesToTheirBestFeasibleValueOneVar() to
  // understand this test structure.

  // Maximization with positive objective coefficient.
  {
    const OneVariableOneConstraint model = {
        .x_lower_bound = -3.0,
        .x_upper_bound = 5.0,
        .x_is_integer = true,
        .c_lower_bound = 3.0 * -1.5,
        .c_upper_bound = 3.0 * 2.5,
        .c_coeff = 3.0,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = 3.0,
    };
    // The variable can be improved.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.0),
                Each(IsOkAndHolds(2.0)));
    // If the constraint's bound in the direction of the objective's improvement
    // is not feasible, keep the variable's initial value.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 3.25),
                Each(IsOkAndHolds(3.25)));
    // If this is the other bound we still will change the variable value (here
    // -2.0 < -1.0 which breaks the constraint).
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -2.0),
                Each(IsOkAndHolds(2.0)));
    // Same but we also break the variable's lower-bound.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -5.0),
                Each(IsOkAndHolds(2.0)));
  }

  // Maximization with negative objective coefficient.
  {
    const OneVariableOneConstraint model = {
        .x_lower_bound = -3.0,
        .x_upper_bound = 5.0,
        .x_is_integer = true,
        .c_lower_bound = 3.0 * -1.5,
        .c_upper_bound = 3.0 * 2.5,
        .c_coeff = 3.0,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = -2.5,
    };
    // The variable can be improved.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.0),
                Each(IsOkAndHolds(-1.0)));
    // If the constraint's bound in the direction of the objective's improvement
    // is not feasible, keep the variable's initial value.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -2.25),
                Each(IsOkAndHolds(-2.25)));
    // If this is the other bound we still will change the variable value (here
    // 3.0 > 2.0 which breaks the constraint).
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 3.0),
                Each(IsOkAndHolds(-1.0)));
    // Same but we also break the variable's upper-bound.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 8.0),
                Each(IsOkAndHolds(-1.0)));
  }
}

// This test validate cases where a variable growth is more limited its own
// bound but a constraint has an higher a finite bound.
TEST(MoveVariablesToTheirBestFeasibleValueTest,
     OneVariableOneNonLimitingFiniteConstraint) {
  // See comment of AllMoveVariablesToTheirBestFeasibleValueOneVar() to
  // understand this test structure.

  // Maximization with positive objective coefficient.
  {
    const OneVariableOneConstraint model = {
        .x_lower_bound = -1.0,
        .x_upper_bound = 2.0,
        .c_lower_bound = 3.0 * -3.0,
        .c_upper_bound = 3.0 * 5.0,
        .c_coeff = 3.0,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = 3.0,
    };
    // The variable can be improved.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.0),
                Each(IsOkAndHolds(2.0)));
    // If the variable's bound in the direction of the objective's improvement
    // is not feasible, keep the variable's initial value.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 3.25),
                Each(IsOkAndHolds(3.25)));
    // If this is the other bound we still will change the variable value (here
    // -2.0 < -1.0 which breaks the constraint).
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -2.0),
                Each(IsOkAndHolds(2.0)));
    // Same but we also break the variable's lower-bound.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -5.0),
                Each(IsOkAndHolds(2.0)));
  }

  // Maximization with negative objective coefficient.
  {
    const OneVariableOneConstraint model = {
        .x_lower_bound = -1.0,
        .x_upper_bound = 2.0,
        .c_lower_bound = 3.0 * -3.0,
        .c_upper_bound = 3.0 * 5.0,
        .c_coeff = 3.0,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = -2.5,
    };
    // The variable can be improved.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.0),
                Each(IsOkAndHolds(-1.0)));
    // If the variable's bound in the direction of the objective's improvement
    // is not feasible, keep the variable's initial value.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -2.25),
                Each(IsOkAndHolds(-2.25)));
    // If this is the other bound we still will change the variable value (here
    // 3.0 > 2.0 which breaks the constraint).
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 3.0),
                Each(IsOkAndHolds(-1.0)));
    // Same but we also break the variable's upper-bound.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 8.0),
                Each(IsOkAndHolds(-1.0)));
  }
}

// This test validate cases where an integer variable growth is more limited its
// own bound (with non-zero fractional part) but a constraint has an higher a
// finite bound.
TEST(MoveVariablesToTheirBestFeasibleValueTest,
     OneIntegerVariableOneNonLimitingFiniteConstraint) {
  // See comment of AllMoveVariablesToTheirBestFeasibleValueOneVar() to
  // understand this test structure.

  // Maximization with positive objective coefficient.
  {
    const OneVariableOneConstraint model = {
        .x_lower_bound = -1.5,
        .x_upper_bound = 2.25,
        .x_is_integer = true,
        .c_lower_bound = 3.0 * -3.0,
        .c_upper_bound = 3.0 * 5.0,
        .c_coeff = 3.0,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = 3.0,
    };
    // The variable can be improved.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.0),
                Each(IsOkAndHolds(2.0)));
    // If the variable's bound in the direction of the objective's improvement
    // is not feasible, keep the variable's initial value.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 3.25),
                Each(IsOkAndHolds(3.25)));
    // If this is the other bound we still will change the variable value (here
    // -2.0 < -1.5 which breaks the constraint).
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -2.0),
                Each(IsOkAndHolds(2.0)));
    // Same but we also break the variable's lower-bound.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -5.0),
                Each(IsOkAndHolds(2.0)));
  }

  // Maximization with negative objective coefficient.
  {
    const OneVariableOneConstraint model = {
        .x_lower_bound = -1.5,
        .x_upper_bound = 2.25,
        .x_is_integer = true,
        .c_lower_bound = 3.0 * -3.0,
        .c_upper_bound = 3.0 * 5.0,
        .c_coeff = 3.0,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = -2.5,
    };
    // The variable can be improved.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.0),
                Each(IsOkAndHolds(-1.0)));
    // If the variable's bound in the direction of the objective's improvement
    // is not feasible, keep the variable's initial value.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -2.25),
                Each(IsOkAndHolds(-2.25)));
    // If this is the other bound we still will change the variable value (here
    // 3.0 > 2.25 which breaks the constraint).
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 3.0),
                Each(IsOkAndHolds(-1.0)));
    // Same but we also break the variable's upper-bound.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 8.0),
                Each(IsOkAndHolds(-1.0)));
  }
}

// This test validate cases where a variable growth is more limited its own
// bound but a constraint has an infinite corresponding bound.
TEST(MoveVariablesToTheirBestFeasibleValueTest,
     OneVariableOneNonLimitingInfiniteConstraint) {
  // See comment of AllMoveVariablesToTheirBestFeasibleValueOneVar() to
  // understand this test structure.

  // Maximization with positive objective coefficient.
  {
    const OneVariableOneConstraint model = {
        .x_lower_bound = -1.0,
        .x_upper_bound = 2.0,
        .c_lower_bound = 3.0 * -3.0,
        .c_upper_bound = kInf,
        .c_coeff = 3.0,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = 3.0,
    };
    // The variable can be improved.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.0),
                Each(IsOkAndHolds(2.0)));
    // If the variable's bound in the direction of the objective's improvement
    // is not feasible, keep the variable's initial value.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 3.25),
                Each(IsOkAndHolds(3.25)));
    // If this is the other bound we still will change the variable value (here
    // -2.0 < -1.0 which breaks the constraint).
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -2.0),
                Each(IsOkAndHolds(2.0)));
    // Same but we also break the variable's lower-bound.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -5.0),
                Each(IsOkAndHolds(2.0)));
  }

  // Maximization with negative objective coefficient.
  {
    const OneVariableOneConstraint model = {
        .x_lower_bound = -1.0,
        .x_upper_bound = 2.0,
        .c_lower_bound = -kInf,
        .c_upper_bound = 5.0,
        .c_coeff = 3.0,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = -2.5,
    };
    // The variable can be improved.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.0),
                Each(IsOkAndHolds(-1.0)));
    // If the variable's bound in the direction of the objective's improvement
    // is not feasible, keep the variable's initial value.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -2.25),
                Each(IsOkAndHolds(-2.25)));
    // If this is the other bound we still will change the variable value (here
    // 3.0 > 2.0 which breaks the constraint).
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 3.0),
                Each(IsOkAndHolds(-1.0)));
    // Same but we also break the variable's upper-bound.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 8.0),
                Each(IsOkAndHolds(-1.0)));
  }
}

// This test validate cases where a variable growth is neither limited by its
// own bound nor a constraint.
TEST(MoveVariablesToTheirBestFeasibleValueTest,
     UnboundedOneVariableOneConstraint) {
  // See comment of AllMoveVariablesToTheirBestFeasibleValueOneVar() to
  // understand this test structure.

  // Maximization with positive objective coefficient.
  {
    const OneVariableOneConstraint model = {
        .x_lower_bound = -1.0,
        .x_upper_bound = kInf,
        .c_lower_bound = 3.0 * -3.0,
        .c_upper_bound = kInf,
        .c_coeff = 3.0,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = 3.0,
    };
    // The variable can be improved.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.0),
                Each(StatusIsUnbounded()));
    // If this is the other bound we still will change the variable value (here
    // -2.0 < -1.0 which breaks the constraint).
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -2.0),
                Each(StatusIsUnbounded()));
    // Same but we also break the variable's lower-bound.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, -5.0),
                Each(StatusIsUnbounded()));
  }

  // Maximization with negative objective coefficient.
  {
    const OneVariableOneConstraint model = {
        .x_lower_bound = -kInf,
        .x_upper_bound = 2.0,
        .c_lower_bound = -kInf,
        .c_upper_bound = 3.0 * 8.0,
        .c_coeff = 3.0,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = -2.5,
    };
    // The variable can be infinitely improved.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.0),
                Each(StatusIsUnbounded()));
    // If this is the other bound we still will change the variable value (here
    // 3.0 > 2.0 which breaks the constraint).
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 3.0),
                Each(StatusIsUnbounded()));
    // Same but we also break the variable's upper-bound.
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 8.0),
                Each(StatusIsUnbounded()));
  }
}

TEST(MoveVariablesToTheirBestFeasibleValueTest, InfiniteConstraintCoefficient) {
  // Here we don't use OneVariableOneConstraint as kInf generates an error with
  // Model::Clone().
  Model model;
  const Variable x = model.AddContinuousVariable(
      /*lower_bound=*/-3.0, /*upper_bound=*/5.0, "x");
  model.AddLinearConstraint(-3.0 <= kInf * x <= 6.0, "c");

  model.Maximize(2.0 * x);

  EXPECT_THAT(
      MoveVariablesToTheirBestFeasibleValue(
          model, /*input_solution=*/{{x, 1.0}}, /*variables=*/{x}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("finite")));
}

TEST(MoveVariablesToTheirBestFeasibleValueTest, NaNConstraintCoefficient) {
  // Here we don't use OneVariableOneConstraint as kNaN generates an error with
  // Model::Clone().
  Model model;
  const Variable x = model.AddContinuousVariable(
      /*lower_bound=*/-3.0, /*upper_bound=*/5.0, "x");
  model.AddLinearConstraint(-3.0 <= kNaN * x <= 6.0, "c");

  model.Maximize(2.0 * x);

  EXPECT_THAT(
      MoveVariablesToTheirBestFeasibleValue(
          model, /*input_solution=*/{{x, 1.0}}, /*variables=*/{x}),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("finite")));
}

TEST(MoveVariablesToTheirBestFeasibleValueTest, ZeroConstraintCoefficient) {
  const OneVariableOneConstraint model = {
      .x_lower_bound = -3.0,
      .x_upper_bound = 5.0,
      .c_lower_bound = 3.0 * -1.0,
      .c_upper_bound = 3.0 * 2.0,
      .c_coeff = 0.0,
      .direction = OptimizationDirection::kMaximize,
      .obj_coeff = 3.0,
  };
  EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.0),
              Each(IsOkAndHolds(5.0)));
}

// This test validates the specific cases where the computation could lead to
// infinite or NaN values due to overflows in intermediate sums.
TEST(MoveVariablesToTheirBestFeasibleValueTest, NumericallyUnbounded) {
  // Use a finite starting constraint value.
  {
    const OneVariableOneConstraint model = {
        .x_lower_bound = 0.0,
        .x_upper_bound = kInf,
        .c_lower_bound = -kInf,
        .c_upper_bound = 1e308,
        .c_coeff = 1e-30,
        .direction = OptimizationDirection::kMaximize,
        .obj_coeff = 3.0,
    };
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValueOneVar(model, 1.0),
                Each(IsOkAndHolds(1.0)));
  }

  // Use an infinite starting constraint value. To do so we make sure we have a
  // term in the constraint that evaluates to inf.
  {
    Model model;
    const Variable x = model.AddContinuousVariable(
        /*lower_bound=*/-kInf, /*upper_bound=*/kInf, "x");
    const Variable y = model.AddContinuousVariable(
        /*lower_bound=*/-kInf, /*upper_bound=*/kInf, "y");
    model.AddLinearConstraint(1e300 * x + 2.0 * y <= 1e308, "c");

    model.Maximize(y);

    EXPECT_THAT(
        AllMoveVariablesToTheirBestFeasibleValue(
            model, /*input_solution=*/{{x, 1e300}, {y, 1.0}},
            /*variables=*/{y}),
        Each(IsOkAndHolds(IsNear({{x, 1e300}, {y, 1.0}}, /*tolerance=*/0.0))));
  }

  // Use a NaN starting constraint value. To do so we make sure we have a
  // two terms in the constraint that evaluates to +inf and -inf.
  {
    Model model;
    const Variable x = model.AddContinuousVariable(
        /*lower_bound=*/-kInf, /*upper_bound=*/kInf, "x");
    const Variable y = model.AddContinuousVariable(
        /*lower_bound=*/-kInf, /*upper_bound=*/kInf, "y");
    const Variable z = model.AddContinuousVariable(
        /*lower_bound=*/-kInf, /*upper_bound=*/kInf, "z");
    model.AddLinearConstraint(1e300 * x + 2.0 * y + 1e300 * z <= 1e308, "c");

    model.Maximize(y);

    EXPECT_THAT(
        AllMoveVariablesToTheirBestFeasibleValue(
            model, /*input_solution=*/{{x, 1e300}, {y, 1.0}, {z, -1e300}},
            /*variables=*/{y}),
        Each(IsOkAndHolds(
            IsNear({{x, 1e300}, {y, 1.0}, {z, -1e300}}, /*tolerance=*/0.0))));
  }
}

TEST(MoveVariablesToTheirBestFeasibleValueTest,
     MultipleVariablesAndConstraints) {
  // For this test we don't test again all tests we done with
  // OneVariableOneConstraint. We only focus on testing that multiple
  // constraints are indeed taken into account properly.
  {
    Model model;
    const Variable x = model.AddContinuousVariable(
        /*lower_bound=*/0.0, /*upper_bound=*/5.25, "x");
    const Variable y = model.AddContinuousVariable(
        /*lower_bound=*/-kInf, /*upper_bound=*/kInf, "y");
    // Limits x growth to (8.25 - 2 * y) / 3.0.
    model.AddLinearConstraint(3.0 * x + 2.0 * y <= 8.25, "c");
    // Does not limit x.
    model.AddLinearConstraint(3.0 <= 5.0 * x, "d");
    // Limits x growth to -4.5 / -2.0 = 2.25.
    model.AddLinearConstraint(-4.5 <= -2.0 * x, "e");

    model.Maximize(x);

    // With y = 0, the first constraint is x <= 2.75 and thus only the third
    // constraint limits x to 2.25.
    EXPECT_THAT(
        AllMoveVariablesToTheirBestFeasibleValueOneVar(
            model, /*input_solution=*/{{x, 1.0}, {y, 0}}, /*variable=*/x),
        Each(IsOkAndHolds(2.25)));
    // With y = 1.5, the first constraint is x <= 1.75 and thus it is the
    // limiting constraint.
    EXPECT_THAT(
        AllMoveVariablesToTheirBestFeasibleValueOneVar(
            model, /*input_solution=*/{{x, 1.0}, {y, 1.5}}, /*variable=*/x),
        Each(IsOkAndHolds(1.75)));
  }

  {
    Model model;
    const Variable x = model.AddIntegerVariable(
        /*lower_bound=*/0.0, /*upper_bound=*/8.25, "x");
    const Variable y = model.AddContinuousVariable(
        /*lower_bound=*/-5.25, /*upper_bound=*/12.0, "y");
    model.AddLinearConstraint(3.0 * x + 2.0 * y <= 13.5, "c");
    model.AddLinearConstraint(5.0 * x - 3.0 * y <= 6.375, "d");

    model.Maximize(x - y);

    // Optimizing x first, then y.
    //
    // We start with constraints values:
    //
    //  c = 8.046875, d = -2.421875
    //
    // thus we have slack:
    //
    //  13.5 - c = 5.453125, 6.375 - d = 8.796875
    //
    // dividing by the coefficient of x:
    //
    //  (13.5 - c) / 3 = 1.81770833333, (6.375 - d) / 5 = 1.759375
    //
    // We see that the increment is at most 1 (since x is integer) and thus the
    // new value of x is 2.
    //
    // With this value for x we repeat the computation for y:
    //
    //  c = 11., d = 2.5
    //  13.5 - c = 2.5, 6.375 - d = 3.875
    //  (13.5 - c) / 2.0 = 1.25, (6.375 - d) / -3.0 = -1.29166666667
    //
    // As y has a negative objective coefficient, we thus use -1.29166666667:
    //
    //  y = 2.5 - 1.29166 = 1.20833
    //
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValue(
                    model, /*input_solution=*/{{x, 1.015625}, {y, 2.5}},
                    /*variables=*/{x, y}),
                Each(IsOkAndHolds(
                    IsNear({{x, 2.0}, {y, 1.20833}}, /*tolerance=*/1e-3))));

    // Optimizing y first, then x. We saturate the second constraint with y and
    // thus x can't move (and we keep its fractional part).
    EXPECT_THAT(AllMoveVariablesToTheirBestFeasibleValue(
                    model, /*input_solution=*/{{x, 2.015625}, {y, 2.5}},
                    /*variables=*/{y, x}),
                Each(IsOkAndHolds(IsNear({{x, 2.015625}, {y, 1.234375}},
                                         /*tolerance=*/1e-3))));
  }
}

// Parameters for RoundedLowerBoundTest and RoundedUpperBoundTest.
struct RoundedBoundTestParam {
  // The bound value.
  double bound = 0.0;

  // Tolerance parameter.
  double tolerance = 0.0;

  // Expected value when the variable is integral. When the variable is
  // continuous we expect `bound` to be returned unchanged.
  double expected_integer = 0.0;
};

std::ostream& operator<<(std::ostream& out,
                         const RoundedBoundTestParam& param) {
  out << "{ bound: " << RoundTripDoubleFormat(param.bound)
      << " tolerance: " << RoundTripDoubleFormat(param.tolerance)
      << " expected_integer: " << RoundTripDoubleFormat(param.expected_integer)
      << " }";
  return out;
}

class RoundedLowerBoundTest
    : public ::testing::TestWithParam<RoundedBoundTestParam> {};

TEST_P(RoundedLowerBoundTest, IntegerVariable) {
  Model model;
  const Variable v = model.AddIntegerVariable(
      /*lower_bound=*/GetParam().bound, /*upper_bound=*/kInf, "x");
  const double result = RoundedLowerBound(v, GetParam().tolerance);
  if (std::isnan(GetParam().expected_integer)) {
    EXPECT_TRUE(std::isnan(result))
        << "RoundedLowerBound(v) = " << RoundTripDoubleFormat(result);
  } else {
    EXPECT_EQ(result, GetParam().expected_integer)
        << "RoundedLowerBound(v) = " << RoundTripDoubleFormat(result)
        << " expected_integer = "
        << RoundTripDoubleFormat(GetParam().expected_integer);
  }
}

TEST_P(RoundedLowerBoundTest, ContinuousVariable) {
  Model model;
  const Variable v = model.AddContinuousVariable(
      /*lower_bound=*/GetParam().bound, /*upper_bound=*/kInf, "x");
  const double result = RoundedLowerBound(v, GetParam().tolerance);
  if (std::isnan(GetParam().bound)) {
    EXPECT_TRUE(std::isnan(result))
        << "RoundedLowerBound(v) = " << RoundTripDoubleFormat(result);
  } else {
    EXPECT_EQ(result, GetParam().bound)
        << "RoundedLowerBound(v) = " << RoundTripDoubleFormat(result)
        << " expected = " << RoundTripDoubleFormat(GetParam().bound);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Values, RoundedLowerBoundTest,
    testing::ValuesIn(std::vector<RoundedBoundTestParam>{
        // Tolerance 0.0.
        {.bound = -kInf, .tolerance = 0.0, .expected_integer = -kInf},
        {.bound = -std::numeric_limits<double>::max(),
         .tolerance = 0.0,
         .expected_integer = -std::numeric_limits<double>::max()},
        {.bound = std::nextafter(-1564.0, -kInf),
         .tolerance = 0.0,
         .expected_integer = -1564.0},
        {.bound = -1564.0, .tolerance = 0.0, .expected_integer = -1564.0},
        {.bound = std::nextafter(-1564.0, kInf),
         .tolerance = 0.0,
         .expected_integer = -1563.0},
        {.bound = std::nextafter(-1.0, -kInf),
         .tolerance = 0.0,
         .expected_integer = -1.0},
        {.bound = -1.0, .tolerance = 0.0, .expected_integer = -1.0},
        {.bound = std::nextafter(-1.0, kInf),
         .tolerance = 0.0,
         .expected_integer = 0.0},
        {.bound = std::nextafter(0.0, -kInf),
         .tolerance = 0.0,
         .expected_integer = 0.0},
        {.bound = 0.0, .tolerance = 0.0, .expected_integer = 0.0},
        {.bound = std::nextafter(0.0, kInf),
         .tolerance = 0.0,
         .expected_integer = 1.0},
        {.bound = std::nextafter(1.0, -kInf),
         .tolerance = 0.0,
         .expected_integer = 1.0},
        {.bound = 1.0, .tolerance = 0.0, .expected_integer = 1.0},
        {.bound = std::nextafter(1.0, kInf),
         .tolerance = 0.0,
         .expected_integer = 2.0},
        {.bound = 1.5, .tolerance = 0.0, .expected_integer = 2.0},
        {.bound = std::nextafter(1564.0, -kInf),
         .tolerance = 0.0,
         .expected_integer = 1564.0},
        {.bound = 1564.0, .tolerance = 0.0, .expected_integer = 1564.0},
        {.bound = std::nextafter(1564.0, kInf),
         .tolerance = 0.0,
         .expected_integer = 1565.0},
        {.bound = std::ldexp(1.0, 53) - 2,
         .tolerance = 0.0,
         .expected_integer = std::ldexp(1.0, 53) - 2},
        {.bound = std::ldexp(1.0, 53) - 1,
         .tolerance = 0.0,
         .expected_integer = std::ldexp(1.0, 53) - 1},
        {.bound = std::ldexp(1.0, 53),
         .tolerance = 0.0,
         .expected_integer = std::ldexp(1.0, 53)},
        {.bound = std::numeric_limits<double>::max(),
         .tolerance = 0.0,
         .expected_integer = std::numeric_limits<double>::max()},
        {.bound = kInf, .tolerance = 0.0, .expected_integer = kInf},
        {.bound = kNaN, .tolerance = 0.0, .expected_integer = kNaN},
        // Tolerance NaN (should return the same thing as 0.0).
        {.bound = std::nextafter(1564.0, kInf),
         .tolerance = kNaN,
         .expected_integer = 1565.0},
        // Tolerance 1-e3.
        {.bound = -1564.9999, .tolerance = 1e-3, .expected_integer = -1565.0},
        {.bound = -1564.999, .tolerance = 1e-3, .expected_integer = -1565.0},
        {.bound = -1564.99, .tolerance = 1e-3, .expected_integer = -1564.0},
        {.bound = -1564.9, .tolerance = 1e-3, .expected_integer = -1564.0},
        {.bound = -1564.0, .tolerance = 1e-3, .expected_integer = -1564.0},
        {.bound = -1563.9999, .tolerance = 1e-3, .expected_integer = -1564.0},
        {.bound = -1563.999, .tolerance = 1e-3, .expected_integer = -1564.0},
        {.bound = -1563.99, .tolerance = 1e-3, .expected_integer = -1563.0},
        {.bound = 1563.0009, .tolerance = 1e-3, .expected_integer = 1563.0},
        {.bound = 1563.002, .tolerance = 1e-3, .expected_integer = 1564.0},
        {.bound = 1563.99, .tolerance = 1e-3, .expected_integer = 1564.0},
        {.bound = 1563.9999, .tolerance = 1e-3, .expected_integer = 1564.0},
        {.bound = 1564.0, .tolerance = 1e-3, .expected_integer = 1564.0},
        {.bound = 1564.0001, .tolerance = 1e-3, .expected_integer = 1564.0},
        {.bound = 1564.002, .tolerance = 1e-3, .expected_integer = 1565.0},
        {.bound = 1564.01, .tolerance = 1e-3, .expected_integer = 1565.0},
        // Tolerance 1.0 (should consider 0.25).
        {.bound = -std::ldexp(1.0, 53),
         .tolerance = 1.0,
         .expected_integer = -std::ldexp(1.0, 53)},
        {.bound = -std::ldexp(1.0, 53) + 1,
         .tolerance = 1.0,
         .expected_integer = -std::ldexp(1.0, 53) + 1},
        {.bound = -std::ldexp(1.0, 53) + 2,
         .tolerance = 1.0,
         .expected_integer = -std::ldexp(1.0, 53) + 2},
        {.bound = -2.8, .tolerance = 1.0, .expected_integer = -3.0},
        {.bound = -2.5, .tolerance = 1.0, .expected_integer = -2.0},
        {.bound = -2.1, .tolerance = 1.0, .expected_integer = -2.0},
        {.bound = -1.9, .tolerance = 1.0, .expected_integer = -2.0},
        {.bound = -1.3, .tolerance = 1.0, .expected_integer = -1.0},
        {.bound = -1.1, .tolerance = 1.0, .expected_integer = -1.0},
        {.bound = -0.9, .tolerance = 1.0, .expected_integer = -1.0},
        {.bound = -0.5, .tolerance = 1.0, .expected_integer = 0.0},
        {.bound = -0.1, .tolerance = 1.0, .expected_integer = 0.0},
        {.bound = 0.1, .tolerance = 1.0, .expected_integer = 0.0},
        {.bound = 0.5, .tolerance = 1.0, .expected_integer = 1.0},
        {.bound = 0.9, .tolerance = 1.0, .expected_integer = 1.0},
        {.bound = 1.1, .tolerance = 1.0, .expected_integer = 1.0},
        {.bound = 1.3, .tolerance = 1.0, .expected_integer = 2.0},
        {.bound = std::ldexp(1.0, 53) - 2,
         .tolerance = 1.0,
         .expected_integer = std::ldexp(1.0, 53) - 2},
        {.bound = std::ldexp(1.0, 53) - 1,
         .tolerance = 1.0,
         .expected_integer = std::ldexp(1.0, 53) - 1},
        {.bound = std::ldexp(1.0, 53),
         .tolerance = 1.0,
         .expected_integer = std::ldexp(1.0, 53)},
        {.bound = std::numeric_limits<double>::max(),
         .tolerance = 1.0,
         .expected_integer = std::numeric_limits<double>::max()},
    }));

class RoundedUpperBoundTest
    : public ::testing::TestWithParam<RoundedBoundTestParam> {};

TEST_P(RoundedUpperBoundTest, IntegerVariable) {
  Model model;
  const Variable v = model.AddIntegerVariable(
      /*lower_bound=*/-kInf, /*upper_bound=*/GetParam().bound, "x");
  const double result = RoundedUpperBound(v, GetParam().tolerance);
  if (std::isnan(GetParam().expected_integer)) {
    EXPECT_TRUE(std::isnan(result))
        << "RoundedUpperBound(v) = " << RoundTripDoubleFormat(result);
  } else {
    EXPECT_EQ(result, GetParam().expected_integer)
        << "RoundedUpperBound(v) = " << RoundTripDoubleFormat(result)
        << " expected_integer = "
        << RoundTripDoubleFormat(GetParam().expected_integer);
  }
}

TEST_P(RoundedUpperBoundTest, ContinuousVariable) {
  Model model;
  const Variable v = model.AddContinuousVariable(
      /*lower_bound=*/-kInf, /*upper_bound=*/GetParam().bound, "x");
  const double result = RoundedUpperBound(v, GetParam().tolerance);
  if (std::isnan(GetParam().bound)) {
    EXPECT_TRUE(std::isnan(result))
        << "RoundedUpperBound(v) = " << RoundTripDoubleFormat(result);
  } else {
    EXPECT_EQ(result, GetParam().bound)
        << "RoundedUpperBound(v) = " << RoundTripDoubleFormat(result)
        << " expected = " << RoundTripDoubleFormat(GetParam().bound);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Values, RoundedUpperBoundTest,
    testing::ValuesIn(std::vector<RoundedBoundTestParam>{
        // Tolerance 0.0.
        {.bound = -kInf, .tolerance = 0.0, .expected_integer = -kInf},
        {.bound = -std::numeric_limits<double>::max(),
         .tolerance = 0.0,
         .expected_integer = -std::numeric_limits<double>::max()},
        {.bound = std::nextafter(-1564.0, -kInf),
         .tolerance = 0.0,
         .expected_integer = -1565.0},
        {.bound = -1564.0, .tolerance = 0.0, .expected_integer = -1564.0},
        {.bound = std::nextafter(-1564.0, kInf),
         .tolerance = 0.0,
         .expected_integer = -1564.0},
        {.bound = std::nextafter(-1.0, -kInf),
         .tolerance = 0.0,
         .expected_integer = -2.0},
        {.bound = -1.0, .tolerance = 0.0, .expected_integer = -1.0},
        {.bound = std::nextafter(-1.0, kInf),
         .tolerance = 0.0,
         .expected_integer = -1.0},
        {.bound = std::nextafter(0.0, -kInf),
         .tolerance = 0.0,
         .expected_integer = -1.0},
        {.bound = 0.0, .tolerance = 0.0, .expected_integer = 0.0},
        {.bound = std::nextafter(0.0, kInf),
         .tolerance = 0.0,
         .expected_integer = 0.0},
        {.bound = std::nextafter(1.0, -kInf),
         .tolerance = 0.0,
         .expected_integer = 0.0},
        {.bound = 1.0, .tolerance = 0.0, .expected_integer = 1.0},
        {.bound = std::nextafter(1.0, kInf),
         .tolerance = 0.0,
         .expected_integer = 1.0},
        {.bound = 1.5, .tolerance = 0.0, .expected_integer = 1.0},
        {.bound = std::nextafter(1564.0, -kInf),
         .tolerance = 0.0,
         .expected_integer = 1563.0},
        {.bound = 1564.0, .tolerance = 0.0, .expected_integer = 1564.0},
        {.bound = std::nextafter(1564.0, kInf),
         .tolerance = 0.0,
         .expected_integer = 1564.0},
        {.bound = std::ldexp(1.0, 53) - 2,
         .tolerance = 0.0,
         .expected_integer = std::ldexp(1.0, 53) - 2},
        {.bound = std::ldexp(1.0, 53) - 1,
         .tolerance = 0.0,
         .expected_integer = std::ldexp(1.0, 53) - 1},
        {.bound = std::ldexp(1.0, 53),
         .tolerance = 0.0,
         .expected_integer = std::ldexp(1.0, 53)},
        {.bound = std::numeric_limits<double>::max(),
         .tolerance = 0.0,
         .expected_integer = std::numeric_limits<double>::max()},
        {.bound = kInf, .tolerance = 0.0, .expected_integer = kInf},
        {.bound = kNaN, .tolerance = 0.0, .expected_integer = kNaN},
        // Tolerance NaN (should return the same thing as 0.0).
        {.bound = std::nextafter(1564.0, -kInf),
         .tolerance = kNaN,
         .expected_integer = 1563.0},
        // Tolerance 1-e3.
        {.bound = -1564.1, .tolerance = 1e-3, .expected_integer = -1565.0},
        {.bound = -1564.01, .tolerance = 1e-3, .expected_integer = -1565.0},
        {.bound = -1564.001, .tolerance = 1e-3, .expected_integer = -1564.0},
        {.bound = -1564.0001, .tolerance = 1e-3, .expected_integer = -1564.0},
        {.bound = -1564.0, .tolerance = 1e-3, .expected_integer = -1564.0},
        {.bound = -1563.9999, .tolerance = 1e-3, .expected_integer = -1564.0},
        {.bound = -1563.999, .tolerance = 1e-3, .expected_integer = -1564.0},
        {.bound = 1563.99, .tolerance = 1e-3, .expected_integer = 1563.0},
        {.bound = 1563.998, .tolerance = 1e-3, .expected_integer = 1563.0},
        {.bound = 1563.9999, .tolerance = 1e-3, .expected_integer = 1564.0},
        {.bound = 1564.0, .tolerance = 1e-3, .expected_integer = 1564.0},
        {.bound = 1564.0001, .tolerance = 1e-3, .expected_integer = 1564.0},
        {.bound = 1564.001, .tolerance = 1e-3, .expected_integer = 1564.0},
        {.bound = 1564.01, .tolerance = 1e-3, .expected_integer = 1564.0},
        // Tolerance 1.0 (should consider 0.25).
        {.bound = -std::ldexp(1.0, 53),
         .tolerance = 1.0,
         .expected_integer = -std::ldexp(1.0, 53)},
        {.bound = -std::ldexp(1.0, 53) + 1,
         .tolerance = 1.0,
         .expected_integer = -std::ldexp(1.0, 53) + 1},
        {.bound = -std::ldexp(1.0, 53) + 2,
         .tolerance = 1.0,
         .expected_integer = -std::ldexp(1.0, 53) + 2},
        {.bound = -2.8, .tolerance = 1.0, .expected_integer = -3.0},
        {.bound = -2.5, .tolerance = 1.0, .expected_integer = -3.0},
        {.bound = -2.1, .tolerance = 1.0, .expected_integer = -2.0},
        {.bound = -1.9, .tolerance = 1.0, .expected_integer = -2.0},
        {.bound = -1.3, .tolerance = 1.0, .expected_integer = -2.0},
        {.bound = -1.1, .tolerance = 1.0, .expected_integer = -1.0},
        {.bound = -0.9, .tolerance = 1.0, .expected_integer = -1.0},
        {.bound = -0.5, .tolerance = 1.0, .expected_integer = -1.0},
        {.bound = -0.1, .tolerance = 1.0, .expected_integer = 0.0},
        {.bound = 0.1, .tolerance = 1.0, .expected_integer = 0.0},
        {.bound = 0.5, .tolerance = 1.0, .expected_integer = 0.0},
        {.bound = 0.9, .tolerance = 1.0, .expected_integer = 1.0},
        {.bound = 1.1, .tolerance = 1.0, .expected_integer = 1.0},
        {.bound = 1.3, .tolerance = 1.0, .expected_integer = 1.0},
        {.bound = 1.9, .tolerance = 1.0, .expected_integer = 2.0},
        {.bound = std::ldexp(1.0, 53) - 2,
         .tolerance = 1.0,
         .expected_integer = std::ldexp(1.0, 53) - 2},
        {.bound = std::ldexp(1.0, 53) - 1,
         .tolerance = 1.0,
         .expected_integer = std::ldexp(1.0, 53) - 1},
        {.bound = std::ldexp(1.0, 53),
         .tolerance = 1.0,
         .expected_integer = std::ldexp(1.0, 53)},
        {.bound = std::numeric_limits<double>::max(),
         .tolerance = 1.0,
         .expected_integer = std::numeric_limits<double>::max()},
    }));

}  // namespace
}  // namespace operations_research::math_opt
