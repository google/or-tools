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

#ifndef ORTOOLS_MATH_OPT_SOLVERS_GSCIP_GSCIP_TESTING_H_
#define ORTOOLS_MATH_OPT_SOLVERS_GSCIP_GSCIP_TESTING_H_

#include <string>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/solvers/gscip/gscip.h"
#include "ortools/math_opt/solvers/gscip/gscip.pb.h"

namespace operations_research {

// Non-default behavior: don't print search logs to standard out.
GScipParameters TestGScipParameters();

// These are provided to make writing good unit tests easier.
std::string SolutionToString(const GScipSolution& solution);

// The tolerance is term-wise (LInf norm).
bool SolutionsAlmostEqual(const GScipSolution& left, const GScipSolution& right,
                          double tolerance = 1e-5);

testing::Matcher<GScipSolution> GScipSolutionEquals(const GScipSolution& rhs);

// The tolerance is term-wise (LInf norm).
testing::Matcher<GScipSolution> GScipSolutionAlmostEquals(
    const GScipSolution& rhs, double tolerance = 1e-5);

// ASSERTs that actual_result has the expected objective value and first best
// solution, to within tolerance.
void AssertOptimalWithBestSolution(const GScipResult& actual_result,
                                   double expected_objective_value,
                                   const GScipSolution& expected_solution,
                                   double tolerance = 1e-5);

// Like above, but not all variable values must be set in expected_solution.
void AssertOptimalWithPartialBestSolution(
    const GScipResult& actual_result, double expected_objective_value,
    const GScipSolution& expected_solution, double tolerance = 1e-5);

}  // namespace operations_research

#endif  // ORTOOLS_MATH_OPT_SOLVERS_GSCIP_GSCIP_TESTING_H_
