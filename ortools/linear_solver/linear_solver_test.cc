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

#include "ortools/linear_solver/linear_solver.h"

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/linear_solver/linear_solver.pb.h"

namespace operations_research {
namespace {

TEST(LinearSolverTest, MPSolverResponseStatusToResultStatusMapping) {
  EXPECT_EQ(MPSolverResponseStatusToResultStatus(MPSOLVER_OPTIMAL),
            MPSolver::OPTIMAL);
  EXPECT_EQ(MPSolverResponseStatusToResultStatus(MPSOLVER_FEASIBLE),
            MPSolver::FEASIBLE);
  EXPECT_EQ(MPSolverResponseStatusToResultStatus(MPSOLVER_INFEASIBLE),
            MPSolver::INFEASIBLE);
  EXPECT_EQ(MPSolverResponseStatusToResultStatus(MPSOLVER_UNBOUNDED),
            MPSolver::UNBOUNDED);
  EXPECT_EQ(MPSolverResponseStatusToResultStatus(MPSOLVER_ABNORMAL),
            MPSolver::ABNORMAL);
  EXPECT_EQ(MPSolverResponseStatusToResultStatus(MPSOLVER_MODEL_INVALID),
            MPSolver::MODEL_INVALID);
  EXPECT_EQ(MPSolverResponseStatusToResultStatus(
                MPSOLVER_MODEL_INVALID_SOLUTION_HINT),
            MPSolver::MODEL_INVALID);
  EXPECT_EQ(MPSolverResponseStatusToResultStatus(
                MPSOLVER_MODEL_INVALID_SOLVER_PARAMETERS),
            MPSolver::MODEL_INVALID);
  EXPECT_EQ(MPSolverResponseStatusToResultStatus(
                MPSOLVER_SOLVER_TYPE_UNAVAILABLE),
            MPSolver::MODEL_INVALID);
  EXPECT_EQ(MPSolverResponseStatusToResultStatus(
                MPSOLVER_INCOMPATIBLE_OPTIONS),
            MPSolver::MODEL_INVALID);
  EXPECT_EQ(MPSolverResponseStatusToResultStatus(MPSOLVER_NOT_SOLVED),
            MPSolver::NOT_SOLVED);
  EXPECT_EQ(MPSolverResponseStatusToResultStatus(MPSOLVER_MODEL_IS_VALID),
            MPSolver::NOT_SOLVED);

  // Verifies that unknown or unmapped response statuses (such as
  // MPSOLVER_UNKNOWN_STATUS = 99 or MPSOLVER_CANCELLED_BY_USER = 98) safely map
  // to ABNORMAL rather than leaking raw integer values into client wrappers.
  EXPECT_EQ(MPSolverResponseStatusToResultStatus(MPSOLVER_CANCELLED_BY_USER),
            MPSolver::ABNORMAL);
  EXPECT_EQ(MPSolverResponseStatusToResultStatus(MPSOLVER_UNKNOWN_STATUS),
            MPSolver::ABNORMAL);
  EXPECT_EQ(MPSolverResponseStatusToResultStatus(
                static_cast<MPSolverResponseStatus>(999)),
            MPSolver::ABNORMAL);
}

TEST(LinearSolverTest, ResultStatusToMPSolverResponseStatusMapping) {
  EXPECT_EQ(ResultStatusToMPSolverResponseStatus(MPSolver::OPTIMAL),
            MPSOLVER_OPTIMAL);
  EXPECT_EQ(ResultStatusToMPSolverResponseStatus(MPSolver::FEASIBLE),
            MPSOLVER_FEASIBLE);
  EXPECT_EQ(ResultStatusToMPSolverResponseStatus(MPSolver::INFEASIBLE),
            MPSOLVER_INFEASIBLE);
  EXPECT_EQ(ResultStatusToMPSolverResponseStatus(MPSolver::UNBOUNDED),
            MPSOLVER_UNBOUNDED);
  EXPECT_EQ(ResultStatusToMPSolverResponseStatus(MPSolver::ABNORMAL),
            MPSOLVER_ABNORMAL);
  EXPECT_EQ(ResultStatusToMPSolverResponseStatus(MPSolver::MODEL_INVALID),
            MPSOLVER_MODEL_INVALID);
  EXPECT_EQ(ResultStatusToMPSolverResponseStatus(MPSolver::NOT_SOLVED),
            MPSOLVER_NOT_SOLVED);
}

TEST(LinearSolverTest, LoadSolutionFromProtoWithUnknownStatus) {
  MPSolver solver("test", MPSolver::GLOP_LINEAR_PROGRAMMING);
  MPSolutionResponse response;
  response.set_status(MPSOLVER_UNKNOWN_STATUS);

  const absl::Status status = solver.LoadSolutionFromProto(response);
  EXPECT_FALSE(status.ok());

  MPSolutionResponse out_response;
  solver.FillSolutionResponseProto(&out_response);
  EXPECT_EQ(out_response.status(), MPSOLVER_ABNORMAL);
}

}  // namespace
}  // namespace operations_research
