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

#include "ortools/math_opt/cpp/incremental_solver.h"

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {
namespace {

using ::testing::_;
using ::testing::Return;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

class MockIncrementalSolver final : public IncrementalSolver {
 public:
  MOCK_METHOD(absl::StatusOr<UpdateResult>, Update, (), (override));
  MOCK_METHOD(absl::StatusOr<SolveResult>, SolveWithoutUpdate,
              (const SolveArguments&), (const, override));
  MOCK_METHOD(absl::StatusOr<ComputeInfeasibleSubsystemResult>,
              ComputeInfeasibleSubsystemWithoutUpdate,
              (const ComputeInfeasibleSubsystemArguments&), (const, override));
  MOCK_METHOD(SolverType, solver_type, (), (const, override));
};

TEST(IncrementalSolverTest, SolveWithFailingUpdate) {
  MockIncrementalSolver incremental_solver;
  EXPECT_CALL(incremental_solver, Update())
      .WillOnce(Return(absl::InternalError("oops")));
  EXPECT_THAT(incremental_solver.Solve(),
              StatusIs(absl::StatusCode::kInternal, "oops"));
}

TEST(IncrementalSolverTest, SolveWithFailingSolveWithoutUpdate) {
  MockIncrementalSolver incremental_solver;
  EXPECT_CALL(incremental_solver, Update())
      .WillOnce(Return(UpdateResult(/*did_update=*/true)));
  EXPECT_CALL(incremental_solver, SolveWithoutUpdate(_))
      .WillOnce(Return(absl::InternalError("oops")));
  EXPECT_THAT(incremental_solver.Solve(),
              StatusIs(absl::StatusCode::kInternal, "oops"));
}

TEST(IncrementalSolverTest, SuccessfulSolve) {
  MockIncrementalSolver incremental_solver;
  EXPECT_CALL(incremental_solver, Update())
      .WillOnce(Return(UpdateResult(/*did_update=*/true)));
  constexpr double kObjectiveValue = 3.5;
  constexpr absl::string_view kDetail = "found the optimum!";
  EXPECT_CALL(incremental_solver, SolveWithoutUpdate(_))
      .WillOnce(Return(
          SolveResult(Termination::Optimal(/*objective_value=*/kObjectiveValue,
                                           /*detail=*/std::string(kDetail)))));
  ASSERT_OK_AND_ASSIGN(const SolveResult solve_result,
                       incremental_solver.Solve());
  EXPECT_THAT(solve_result.termination,
              TerminationIsOptimal(/*primal_objective_value=*/kObjectiveValue));
  EXPECT_EQ(solve_result.termination.detail, kDetail);
}

TEST(IncrementalSolverTest, ComputeInfeasibleSubsystemWithFailingUpdate) {
  MockIncrementalSolver incremental_solver;
  EXPECT_CALL(incremental_solver, Update())
      .WillOnce(Return(absl::InternalError("oops")));
  EXPECT_THAT(incremental_solver.ComputeInfeasibleSubsystem(),
              StatusIs(absl::StatusCode::kInternal, "oops"));
}

TEST(IncrementalSolverTest,
     ComputeInfeasibleSubsystemWithFailingComputeWithoutUpdate) {
  MockIncrementalSolver incremental_solver;
  EXPECT_CALL(incremental_solver, Update())
      .WillOnce(Return(UpdateResult(/*did_update=*/true)));
  EXPECT_CALL(incremental_solver, ComputeInfeasibleSubsystemWithoutUpdate(_))
      .WillOnce(Return(absl::InternalError("oops")));
  EXPECT_THAT(incremental_solver.ComputeInfeasibleSubsystem(),
              StatusIs(absl::StatusCode::kInternal, "oops"));
}

TEST(IncrementalSolverTest, SuccessfulComputeInfeasibleSubsystem) {
  MockIncrementalSolver incremental_solver;
  EXPECT_CALL(incremental_solver, Update())
      .WillOnce(Return(UpdateResult(/*did_update=*/true)));
  Model model;
  const Variable v = model.AddBinaryVariable("v");
  const ModelSubset model_subset = {
      .variable_integrality = {v},
  };
  EXPECT_CALL(incremental_solver, ComputeInfeasibleSubsystemWithoutUpdate(_))
      .WillOnce(Return(ComputeInfeasibleSubsystemResult{
          .feasibility = FeasibilityStatus::kInfeasible,
          .infeasible_subsystem = model_subset,
          .is_minimal = false,
      }));
  ASSERT_THAT(incremental_solver.ComputeInfeasibleSubsystem(),
              IsOkAndHolds(IsInfeasible(
                  /*expected_is_minimal=*/false,
                  /*expected_infeasible_subsystem=*/model_subset)));
}

}  // namespace
}  // namespace operations_research::math_opt
