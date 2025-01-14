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

#ifndef OR_TOOLS_MATH_OPT_SOLVER_TESTS_IP_MULTIPLE_SOLUTIONS_TESTS_H_
#define OR_TOOLS_MATH_OPT_SOLVER_TESTS_IP_MULTIPLE_SOLUTIONS_TESTS_H_

#include <ostream>
#include <utility>

#include "gtest/gtest.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {

struct IpMultipleSolutionsTestParams {
  IpMultipleSolutionsTestParams(const SolverType solver_type,
                                SolveParameters ensure_hint_in_pool)
      : solver_type(solver_type),
        ensure_hint_in_pool(std::move(ensure_hint_in_pool)) {}

  // The tested solver.
  SolverType solver_type;

  SolveParameters ensure_hint_in_pool;

  friend std::ostream& operator<<(std::ostream& out,
                                  const IpMultipleSolutionsTestParams& params);
};

class IpMultipleSolutionsTest
    : public ::testing::TestWithParam<IpMultipleSolutionsTestParams> {};

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_SOLVER_TESTS_IP_MULTIPLE_SOLUTIONS_TESTS_H_
