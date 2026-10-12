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

#include "ortools/linear_solver/glop_utils.h"

#include "gtest/gtest.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/lp_data/lp_types.h"

namespace operations_research {
namespace {

TEST(VariableStatusConversionTest, GlopToMPSolverAndViceVersa) {
  for (const auto s :
       {MPSolver::FREE, MPSolver::AT_LOWER_BOUND, MPSolver::AT_UPPER_BOUND,
        MPSolver::FIXED_VALUE, MPSolver::BASIC}) {
    EXPECT_EQ(s, GlopToMPSolverVariableStatus(MPSolverToGlopVariableStatus(s)));
  }
  for (const auto s :
       {glop::VariableStatus::FREE, glop::VariableStatus::AT_LOWER_BOUND,
        glop::VariableStatus::AT_UPPER_BOUND, glop::VariableStatus::FIXED_VALUE,
        glop::VariableStatus::BASIC}) {
    EXPECT_EQ(s, MPSolverToGlopVariableStatus(GlopToMPSolverVariableStatus(s)));
  }
}

TEST(ConstraintStatusConversionTest, GlopToMPSolverAndViceVersa) {
  for (const auto s :
       {MPSolver::FREE, MPSolver::AT_LOWER_BOUND, MPSolver::AT_UPPER_BOUND,
        MPSolver::FIXED_VALUE, MPSolver::BASIC}) {
    EXPECT_EQ(
        s, GlopToMPSolverConstraintStatus(MPSolverToGlopConstraintStatus(s)));
  }
  for (const auto s :
       {glop::ConstraintStatus::FREE, glop::ConstraintStatus::AT_LOWER_BOUND,
        glop::ConstraintStatus::AT_UPPER_BOUND,
        glop::ConstraintStatus::FIXED_VALUE, glop::ConstraintStatus::BASIC}) {
    EXPECT_EQ(
        s, MPSolverToGlopConstraintStatus(GlopToMPSolverConstraintStatus(s)));
  }
}

}  // namespace
}  // namespace operations_research
