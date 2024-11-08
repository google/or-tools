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

#include "ortools/math_opt/solver_tests/multi_objective_tests.h"

#include <memory>
#include <ostream>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/port/proto_utils.h"

namespace operations_research::math_opt {

using ::testing::AnyOf;
using ::testing::DoubleNear;
using ::testing::HasSubstr;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

constexpr absl::string_view kNoMultiObjectiveSupportMessage =
    "This test is disabled as the solver does not support multiple objective "
    "models";

constexpr double kTolerance = 1.0e-6;

MultiObjectiveTestParameters::MultiObjectiveTestParameters(
    SolverType solver_type, SolveParameters parameters,
    bool supports_auxiliary_objectives,
    bool supports_incremental_objective_add_and_delete,
    bool supports_incremental_objective_modification)
    : solver_type(solver_type),
      parameters(parameters),
      supports_auxiliary_objectives(supports_auxiliary_objectives),
      supports_incremental_objective_add_and_delete(
          supports_incremental_objective_add_and_delete),
      supports_incremental_objective_modification(
          supports_incremental_objective_modification) {}

std::ostream& operator<<(std::ostream& out,
                         const MultiObjectiveTestParameters& params) {
  out << "{ solver_type: " << params.solver_type
      << ", parameters: " << ProtobufShortDebugString(params.parameters.Proto())
      << ", supports_auxiliary_objectives: "
      << (params.supports_auxiliary_objectives ? "true" : "false")
      << ", supports_incremental_objective_add_and_delete: "
      << (params.supports_incremental_objective_add_and_delete ? "true"
                                                               : "false")
      << ", supports_incremental_objective_modification: "
      << (params.supports_incremental_objective_modification ? "true" : "false")
      << " }";
  return out;
}

namespace {

TEST_P(SimpleMultiObjectiveTest, CanBuildMultiObjectiveModel) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  model.AddMaximizationObjective(x, /*priority=*/2);
  model.AddMinimizationObjective(-3.0 * x + 2.0, /*priority=*/1);

  if (GetParam().supports_auxiliary_objectives) {
    EXPECT_OK(NewIncrementalSolver(&model, GetParam().solver_type, {}));
  } else {
    EXPECT_THAT(NewIncrementalSolver(&model, GetParam().solver_type, {}),
                StatusIs(AnyOf(absl::StatusCode::kInvalidArgument,
                               absl::StatusCode::kUnimplemented),
                         HasSubstr("multiple objectives")));
  }
}

// We consider the two objective model
// max  {x, x + 3*y + 2}
// s.t. x + y <= 1.5
// 0 <= x, y <= 1
//
// This has the unique optimal solution (x^*, y^*) = (1, 0.5) with objective
// values (1, 4.5).
TEST_P(SimpleMultiObjectiveTest, SolveMultiObjectiveModel) {
  if (!GetParam().supports_auxiliary_objectives) {
    GTEST_SKIP() << kNoMultiObjectiveSupportMessage;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.AddLinearConstraint(x + y <= 1.5);
  model.Maximize(x);
  const Objective o =
      model.AddMaximizationObjective(x + 3.0 * y + 2.0, /*priority=*/1);

  ASSERT_OK_AND_ASSIGN(const SolveResult result, SimpleSolve(model));
  ASSERT_THAT(result, IsOptimalWithSolution(1.0, {{x, 1.0}, {y, 0.5}}));
  EXPECT_EQ(result.objective_value(o), 4.5);
}

// We consider the two objective model
// {min(-x), max(x + 3*y + 2)}
// s.t. x + y <= 1.5
// 0 <= x, y <= 1
//
// This has the unique optimal solution (x^*, y^*) = (1, 0.5) with objective
// values (-1, 4.5).
TEST_P(SimpleMultiObjectiveTest, MultipleObjectivesWithDifferentSenses) {
  if (!GetParam().supports_auxiliary_objectives) {
    GTEST_SKIP() << kNoMultiObjectiveSupportMessage;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.AddLinearConstraint(x + y <= 1.5);
  model.Minimize(-x);
  const Objective o =
      model.AddMaximizationObjective(x + 3.0 * y + 2.0, /*priority=*/1);

  ASSERT_OK_AND_ASSIGN(const SolveResult result, SimpleSolve(model));
  ASSERT_THAT(result, IsOptimalWithSolution(-1.0, {{x, 1.0}, {y, 0.5}}));
  EXPECT_EQ(result.objective_value(o), 4.5);
}

TEST_P(SimpleMultiObjectiveTest, PrimaryAndAuxiliaryObjectiveSharePriority) {
  if (!GetParam().supports_auxiliary_objectives) {
    GTEST_SKIP() << kNoMultiObjectiveSupportMessage;
  }
  Model model;
  model.set_objective_priority(model.primary_objective(), 1);
  model.AddAuxiliaryObjective(1);
  EXPECT_THAT(NewIncrementalSolver(&model, GetParam().solver_type, {}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("repeated objective priority: 1")));
}

TEST_P(SimpleMultiObjectiveTest, AuxiliaryObjectivesSharePriority) {
  if (!GetParam().supports_auxiliary_objectives) {
    GTEST_SKIP() << kNoMultiObjectiveSupportMessage;
  }
  Model model;
  model.AddAuxiliaryObjective(1);
  model.AddAuxiliaryObjective(1);
  EXPECT_THAT(NewIncrementalSolver(&model, GetParam().solver_type, {}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("repeated objective priority: 1")));
}

// For a univariate optimization problem with two objectives.
struct SimpleMultiObjectiveSolveResult {
  TerminationReason termination = TerminationReason::kOtherError;
  double solution = 0.0;
  double priority_0_objective_value = 0.0;
  double priority_1_objective_value = 0.0;
};

enum class ObjectiveType { kPrimary, kAuxiliary };
enum class ToleranceType { kAbsolute, kRelative };

// We consider the two objective model
// priority 0: max(x)
// priority 1: min(x)
// s.t.        0 <= x <= 2
//             x is integer
//
// The optimal solution is x^* = 2 with objective values (2, 2). We test the
// degradation tolerances by setting them on the priority 0 objective such that
// the optimal solution, (up to tolerances), is x^* = 1 with objective values
// (1, 1). We can accomplish this with an absolute tolerance of 1 or a relative
// tolerance of 0.5.
absl::StatusOr<SimpleMultiObjectiveSolveResult> SolveWithObjectiveDegradation(
    const SolverType solver_type, const SolveParameters& parameters,
    const ObjectiveType priority_0_type, const ObjectiveType priority_1_type,
    const ToleranceType tolerance_type) {
  if (priority_0_type == ObjectiveType::kPrimary &&
      priority_1_type == ObjectiveType::kPrimary) {
    return absl::InvalidArgumentError("Both objectives cannot both be primary");
  }
  Model model;
  const Variable x = model.AddIntegerVariable(0.0, 2.0, "x");
  const Objective priority_0 = [&]() {
    switch (priority_0_type) {
      case ObjectiveType::kPrimary:
        model.Maximize(x);
        model.set_objective_priority(model.primary_objective(), 0);
        return model.primary_objective();
      case ObjectiveType::kAuxiliary:
        return model.AddMaximizationObjective(x, /*priority=*/0);
    }
  }();
  const Objective priority_1 = [&]() {
    switch (priority_1_type) {
      case ObjectiveType::kPrimary:
        model.Minimize(x);
        model.set_objective_priority(model.primary_objective(), 1);
        return model.primary_objective();
      case ObjectiveType::kAuxiliary:
        return model.AddMinimizationObjective(x, /*priority=*/1);
    }
  }();
  ModelSolveParameters model_parameters;
  switch (tolerance_type) {
    case ToleranceType::kAbsolute:
      model_parameters.objective_parameters[priority_0]
          .objective_degradation_absolute_tolerance = 1.0;
      break;
    case ToleranceType::kRelative:
      model_parameters.objective_parameters[priority_0]
          .objective_degradation_relative_tolerance = 0.5;
  }
  ASSIGN_OR_RETURN(
      const SolveResult result,
      Solve(model, solver_type,
            {.parameters = parameters, .model_parameters = model_parameters}));
  if (!result.has_primal_feasible_solution()) {
    return absl::InternalError("No feasible solution found");
  }
  return SimpleMultiObjectiveSolveResult{
      .termination = result.termination.reason,
      .solution = result.best_primal_solution().variable_values.at(x),
      .priority_0_objective_value = result.objective_value(priority_0),
      .priority_1_objective_value = result.objective_value(priority_1)};
}

TEST_P(SimpleMultiObjectiveTest, PrimaryObjectiveDegradationAbsoluteTolerance) {
  if (!GetParam().supports_auxiliary_objectives) {
    GTEST_SKIP() << kNoMultiObjectiveSupportMessage;
  }
  ASSERT_OK_AND_ASSIGN(const SimpleMultiObjectiveSolveResult result,
                       SolveWithObjectiveDegradation(
                           GetParam().solver_type, GetParam().parameters,
                           /*priority_0_type=*/ObjectiveType::kPrimary,
                           /*priority_1_type=*/ObjectiveType::kAuxiliary,
                           ToleranceType::kAbsolute));
  EXPECT_EQ(result.termination, TerminationReason::kOptimal);
  EXPECT_THAT(result.solution, DoubleNear(1.0, kTolerance));
  EXPECT_THAT(result.priority_0_objective_value, DoubleNear(1.0, kTolerance));
  EXPECT_THAT(result.priority_1_objective_value, DoubleNear(1.0, kTolerance));
}

TEST_P(SimpleMultiObjectiveTest,
       AuxiliaryObjectiveDegradationAbsoluteTolerance) {
  if (!GetParam().supports_auxiliary_objectives) {
    GTEST_SKIP() << kNoMultiObjectiveSupportMessage;
  }
  ASSERT_OK_AND_ASSIGN(const SimpleMultiObjectiveSolveResult result,
                       SolveWithObjectiveDegradation(
                           GetParam().solver_type, GetParam().parameters,
                           /*priority_0_type=*/ObjectiveType::kAuxiliary,
                           /*priority_1_type=*/ObjectiveType::kPrimary,
                           ToleranceType::kAbsolute));
  EXPECT_EQ(result.termination, TerminationReason::kOptimal);
  EXPECT_THAT(result.solution, DoubleNear(1.0, kTolerance));
  EXPECT_THAT(result.priority_0_objective_value, DoubleNear(1.0, kTolerance));
  EXPECT_THAT(result.priority_1_objective_value, DoubleNear(1.0, kTolerance));
}

TEST_P(SimpleMultiObjectiveTest, PrimaryObjectiveDegradationRelativeTolerance) {
  if (!GetParam().supports_auxiliary_objectives) {
    GTEST_SKIP() << kNoMultiObjectiveSupportMessage;
  }
  ASSERT_OK_AND_ASSIGN(const SimpleMultiObjectiveSolveResult result,
                       SolveWithObjectiveDegradation(
                           GetParam().solver_type, GetParam().parameters,
                           /*priority_0_type=*/ObjectiveType::kPrimary,
                           /*priority_1_type=*/ObjectiveType::kAuxiliary,
                           ToleranceType::kRelative));
  EXPECT_EQ(result.termination, TerminationReason::kOptimal);
  EXPECT_THAT(result.solution, DoubleNear(1.0, kTolerance));
  EXPECT_THAT(result.priority_0_objective_value, DoubleNear(1.0, kTolerance));
  EXPECT_THAT(result.priority_1_objective_value, DoubleNear(1.0, kTolerance));
}

// You should be able to specify this parameter for a single objective model; it
// will be ignored.
TEST_P(SimpleMultiObjectiveTest,
       SingleObjectiveModelWithObjectiveDegradationAbsoluteTolerance) {
  if (!GetParam().supports_auxiliary_objectives) {
    GTEST_SKIP() << kNoMultiObjectiveSupportMessage;
  }
  Model model;
  const Variable x = model.AddIntegerVariable(0.0, 1.0, "x");
  model.Maximize(x);
  ModelSolveParameters model_parameters;
  model_parameters.objective_parameters[model.primary_objective()]
      .objective_degradation_absolute_tolerance = 0.5;
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(model, GetParam().solver_type,
                             {.parameters = GetParam().parameters,
                              .model_parameters = model_parameters}));
  ASSERT_THAT(result, IsOptimalWithSolution(1.0, {{x, 1.0}}));
}

// You should be able to specify this parameter for a single objective model; it
// will be ignored.
TEST_P(SimpleMultiObjectiveTest,
       SingleObjectiveModelWithObjectiveDegradationRelativeTolerance) {
  if (!GetParam().supports_auxiliary_objectives) {
    GTEST_SKIP() << kNoMultiObjectiveSupportMessage;
  }
  Model model;
  const Variable x = model.AddIntegerVariable(0.0, 1.0, "x");
  model.Maximize(x);
  ModelSolveParameters model_parameters;
  model_parameters.objective_parameters[model.primary_objective()]
      .objective_degradation_relative_tolerance = 0.5;
  ASSERT_OK_AND_ASSIGN(const SolveResult result,
                       Solve(model, GetParam().solver_type,
                             {.parameters = GetParam().parameters,
                              .model_parameters = model_parameters}));
  ASSERT_THAT(result, IsOptimalWithSolution(1.0, {{x, 1.0}}));
}

TEST_P(SimpleMultiObjectiveTest,
       AuxiliaryObjectiveDegradationRelativeTolerance) {
  if (!GetParam().supports_auxiliary_objectives) {
    GTEST_SKIP() << kNoMultiObjectiveSupportMessage;
  }
  ASSERT_OK_AND_ASSIGN(const SimpleMultiObjectiveSolveResult result,
                       SolveWithObjectiveDegradation(
                           GetParam().solver_type, GetParam().parameters,
                           /*priority_0_type=*/ObjectiveType::kAuxiliary,
                           /*priority_1_type=*/ObjectiveType::kPrimary,
                           ToleranceType::kRelative));
  EXPECT_EQ(result.termination, TerminationReason::kOptimal);
  EXPECT_THAT(result.solution, DoubleNear(1.0, kTolerance));
  EXPECT_THAT(result.priority_0_objective_value, DoubleNear(1.0, kTolerance));
  EXPECT_THAT(result.priority_1_objective_value, DoubleNear(1.0, kTolerance));
}

// We start with the single objective model:
// max  x
// s.t. x + y <= 1.5
// 0 <= x, y <= 1
//
// The optimal objective value is 1, and (x^*, y^*) = (1, a) is optimal for any
// value a in [0, 1].
//
// We then add the secondary objective
//
// max  {x, x + 3*y + 2}
// s.t. x + y <= 1.5
// 0 <= x, y <= 1
//
// This has the unique optimal solution (x^*, y^*) = (1, 0.5) with objective
// values (1, 4.5).
TEST_P(IncrementalMultiObjectiveTest, SingleToMultiObjectiveModel) {
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.AddLinearConstraint(x + y <= 1.5);
  model.Maximize(x);

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      NewIncrementalSolver(&model, GetParam().solver_type, {}));
  // Since there are multiple optimal solutions we do not match against the
  // solution value.
  ASSERT_THAT(solver->Solve({.parameters = GetParam().parameters}),
              IsOkAndHolds(IsOptimal(1.0)));

  const Objective o =
      model.AddMaximizationObjective(x + 3.0 * y + 2.0, /*priority=*/1);

  if (!GetParam().supports_auxiliary_objectives) {
    // Here we test that solvers that don't support auxiliary objectives return
    // false in SolverInterface::CanUpdate(). Thus they should fail in their
    // factory function instead of failing in their SolverInterface::Update()
    // function. To assert we rely on status annotations added by
    // IncrementalSolver::Update() to the returned status of Solver::Update()
    // and Solver::New().
    EXPECT_THAT(
        solver->Update(),
        StatusIs(AnyOf(absl::StatusCode::kInvalidArgument,
                       absl::StatusCode::kUnimplemented),
                 AllOf(HasSubstr("multiple objective"),
                       // Sub-string expected for Solver::Update() error.
                       Not(HasSubstr("update failed")),
                       // Sub-string expected for Solver::New() error.
                       HasSubstr("solver re-creation failed"))));
    return;
  }

  ASSERT_THAT(
      solver->Update(),
      IsOkAndHolds(GetParam().supports_incremental_objective_add_and_delete
                       ? DidUpdate()
                       : Not(DidUpdate())));
  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      solver->SolveWithoutUpdate({.parameters = GetParam().parameters}));
  ASSERT_THAT(result, IsOptimalWithSolution(1.0, {{x, 1.0}, {y, 0.5}}));
  EXPECT_EQ(result.objective_value(o), 4.5);
}

// We start with the two objective model:
// max  {x, 3}
// s.t. x + y <= 1.5
// 0 <= x, y <= 1
//
// The optimal objective values are (1, 3), and (x^*, y^*) = (1, a) is optimal
// for any value a in [0, 1].
//
// We then add the tertiary objective
//
// max  {x, 3, x + 3*y + 2}
// s.t. x + y <= 1.5
// 0 <= x, y <= 1
//
// This has the unique optimal solution (x^*, y^*) = (1, 0.5) with objective
// values (1, 3, 4.5).
TEST_P(IncrementalMultiObjectiveTest, AddObjectiveToMultiObjectiveModel) {
  if (!GetParam().supports_auxiliary_objectives) {
    GTEST_SKIP() << kNoMultiObjectiveSupportMessage;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.AddLinearConstraint(x + y <= 1.5);
  model.Maximize(x);
  const Objective o = model.AddMaximizationObjective(3.0, /*priority=*/2);

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      NewIncrementalSolver(&model, GetParam().solver_type, {}));
  {
    ASSERT_OK_AND_ASSIGN(const SolveResult result,
                         solver->Solve({.parameters = GetParam().parameters}));
    ASSERT_THAT(result, IsOptimal(1.0));
    EXPECT_EQ(result.objective_value(o), 3.0);
  }

  const Objective o2 =
      model.AddMaximizationObjective(x + 3.0 * y + 2.0, /*priority=*/5);

  ASSERT_THAT(
      solver->Update(),
      IsOkAndHolds(GetParam().supports_incremental_objective_add_and_delete
                       ? DidUpdate()
                       : Not(DidUpdate())));
  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      solver->SolveWithoutUpdate({.parameters = GetParam().parameters}));
  ASSERT_THAT(result, IsOptimalWithSolution(1.0, {{x, 1.0}, {y, 0.5}}));
  EXPECT_EQ(result.objective_value(o), 3.0);
  EXPECT_EQ(result.objective_value(o2), 4.5);
}

// We start with the three objective model:
// max  {x, 3, x + 3*y + 2}
// s.t. x + y <= 1.5
// 0 <= x, y <= 1
//
// This has the unique optimal solution (x^*, y^*) = (1, 0.5) with objective
// values (1, 3, 4.5).
//
// We then delete the second objective, leaving
//
// max  {x, x + 3*y + 2}
// s.t. x + y <= 1.5
// 0 <= x, y <= 1
//
// This has the unique optimal solution (x^*, y^*) = (1, 0.5) with objective
// values (1, 4.5).
TEST_P(IncrementalMultiObjectiveTest, DeleteObjectiveFromMultiObjectiveModel) {
  if (!GetParam().supports_auxiliary_objectives) {
    GTEST_SKIP() << kNoMultiObjectiveSupportMessage;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.AddLinearConstraint(x + y <= 1.5);
  model.Maximize(x);
  const Objective o = model.AddMaximizationObjective(3.0, /*priority=*/2);
  const Objective o2 =
      model.AddMaximizationObjective(x + 3.0 * y + 2.0, /*priority=*/3);

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      NewIncrementalSolver(&model, GetParam().solver_type, {}));
  {
    ASSERT_OK_AND_ASSIGN(const SolveResult result,
                         solver->Solve({.parameters = GetParam().parameters}));
    ASSERT_THAT(result, IsOptimalWithSolution(1.0, {{x, 1.0}, {y, 0.5}}));
    EXPECT_EQ(result.objective_value(o), 3.0);
    EXPECT_EQ(result.objective_value(o2), 4.5);
  }

  model.DeleteAuxiliaryObjective(o);

  ASSERT_THAT(
      solver->Update(),
      IsOkAndHolds(GetParam().supports_incremental_objective_add_and_delete
                       ? DidUpdate()
                       : Not(DidUpdate())));
  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      solver->SolveWithoutUpdate({.parameters = GetParam().parameters}));
  ASSERT_THAT(result, IsOptimalWithSolution(1.0, {{x, 1.0}, {y, 0.5}}));
  EXPECT_EQ(result.objective_value(o2), 4.5);
}

// We start with the two objective model:
// {max(x), max(x + 3*y + 2)}
// s.t. x + y <= 1.5
// 0 <= x, y <= 1
//
// This has the unique optimal solution (x^*, y^*) = (1, 0.5) with objective
// values (1, 4.5).
//
// We then flip the sign of the first objective, leaving
//
// {min(x), max(x + 3*y + 2)}
// s.t. x + y <= 1.5
// 0 <= x, y <= 1
//
// This has the unique optimal solution (x^*, y^*) = (0, 1) with objective
// values (0, 5).
TEST_P(IncrementalMultiObjectiveTest,
       ModifyPrimaryObjectiveSenseInMultiObjectiveModel) {
  if (!GetParam().supports_auxiliary_objectives) {
    GTEST_SKIP() << kNoMultiObjectiveSupportMessage;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.AddLinearConstraint(x + y <= 1.5);
  model.Maximize(x);
  const Objective o =
      model.AddMaximizationObjective(x + 3.0 * y + 2.0, /*priority=*/2);

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      NewIncrementalSolver(&model, GetParam().solver_type, {}));
  {
    ASSERT_OK_AND_ASSIGN(const SolveResult result,
                         solver->Solve({.parameters = GetParam().parameters}));
    ASSERT_THAT(result, IsOptimalWithSolution(1.0, {{x, 1.0}, {y, 0.5}}));
    ASSERT_OK_AND_ASSIGN(const SolveResultProto result_proto, result.Proto());
    EXPECT_EQ(result.objective_value(o), 4.5);
  }

  model.set_minimize();

  ASSERT_THAT(
      solver->Update(),
      IsOkAndHolds(GetParam().supports_incremental_objective_modification
                       ? DidUpdate()
                       : Not(DidUpdate())));
  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      solver->SolveWithoutUpdate({.parameters = GetParam().parameters}));
  ASSERT_THAT(result, IsOptimalWithSolution(0.0, {{x, 0.0}, {y, 1.0}}));
  EXPECT_EQ(result.objective_value(o), 5.0);
}

// Same problem as ModifyPrimaryObjectiveSenseInMultiObjectiveModel, except we
// switch which objective is primary and which is auxiliary.
TEST_P(IncrementalMultiObjectiveTest,
       ModifyAuxiliaryObjectiveSenseInMultiObjectiveModel) {
  if (!GetParam().supports_auxiliary_objectives) {
    GTEST_SKIP() << kNoMultiObjectiveSupportMessage;
  }
  Model model;
  const Variable x = model.AddContinuousVariable(0.0, 1.0, "x");
  const Variable y = model.AddContinuousVariable(0.0, 1.0, "y");
  model.AddLinearConstraint(x + y <= 1.5);
  model.Maximize(x + 3.0 * y + 2.0);
  model.set_objective_priority(model.primary_objective(), 2);
  const Objective o = model.AddMaximizationObjective(x, /*priority=*/0);

  ASSERT_OK_AND_ASSIGN(
      const auto solver,
      NewIncrementalSolver(&model, GetParam().solver_type, {}));
  {
    ASSERT_OK_AND_ASSIGN(const SolveResult result,
                         solver->Solve({.parameters = GetParam().parameters}));
    ASSERT_THAT(result, IsOptimalWithSolution(4.5, {{x, 1.0}, {y, 0.5}}));
    EXPECT_EQ(result.objective_value(o), 1.0);
  }

  model.set_minimize(o);

  ASSERT_THAT(
      solver->Update(),
      IsOkAndHolds(GetParam().supports_incremental_objective_modification
                       ? DidUpdate()
                       : Not(DidUpdate())));
  ASSERT_OK_AND_ASSIGN(
      const SolveResult result,
      solver->SolveWithoutUpdate({.parameters = GetParam().parameters}));
  ASSERT_THAT(result, IsOptimalWithSolution(5.0, {{x, 0.0}, {y, 1.0}}));
  EXPECT_EQ(result.objective_value(o), 0.0);
}

// TODO(b/214027410): Add these to IncrementalMultiObjectiveTest when needed:
//  * ModifyPrimaryObjectiveOffsetInMultiObjectiveModel
//  * ModifyAuxiliaryObjectiveOffsetInMultiObjectiveModel
//  * ModifyPrimaryObjectiveLinearCoefficientInMultiObjectiveModel
//  * ModifyAuxiliaryObjectiveLinearCoefficientInMultiObjectiveModel
//  * AfterUpdatePrimaryAndAuxiliaryObjectiveSharePriority
//  * AfterUpdateAuxiliaryObjectivesSharePriority

}  // namespace
}  // namespace operations_research::math_opt
