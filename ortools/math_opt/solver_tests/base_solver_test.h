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

#ifndef OR_TOOLS_MATH_OPT_SOLVER_TESTS_BASE_SOLVER_TEST_H_
#define OR_TOOLS_MATH_OPT_SOLVER_TESTS_BASE_SOLVER_TEST_H_

#include "gtest/gtest.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research {
namespace math_opt {

class BaseSolverTest : public ::testing::TestWithParam<SolverType> {
 protected:
  SolverType TestedSolver() const;
};

// Updates the input parameters so that the solver produces a primal ray for
// unbounded problems for infeasible ones. Return true if the solver supports
// producing primal rays, else returns false.
bool ActivatePrimalRay(SolverType solver_type, SolveParameters& params);

// Updates the input parameters so that the solver produces a dual ray for
// infeasible ones. Return true if the solver supports producing dual rays, else
// returns false.
bool ActivateDualRay(SolverType solver_type, SolveParameters& params);

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_SOLVER_TESTS_BASE_SOLVER_TEST_H_
