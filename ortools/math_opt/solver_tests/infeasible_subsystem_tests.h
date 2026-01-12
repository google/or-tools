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

#ifndef ORTOOLS_MATH_OPT_SOLVER_TESTS_INFEASIBLE_SUBSYSTEM_TESTS_H_
#define ORTOOLS_MATH_OPT_SOLVER_TESTS_INFEASIBLE_SUBSYSTEM_TESTS_H_

#include <ostream>

#include "gtest/gtest.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {

// Relevant functionality for infeasible subsystem computation that the solver
// supports.
struct InfeasibleSubsystemSupport {
  bool supports_infeasible_subsystems = false;
};

std::ostream& operator<<(std::ostream& ostr,
                         const InfeasibleSubsystemSupport& support_menu);

struct InfeasibleSubsystemTestParameters {
  // The tested solver.
  SolverType solver_type;

  InfeasibleSubsystemSupport support_menu;
};

std::ostream& operator<<(std::ostream& ostr,
                         const InfeasibleSubsystemTestParameters& params);

// A suite of unit tests to show that a solver correctly handles requests for
// infeasible subsystems.
//
// To use these tests, in file <solver>_test.cc write:
// INSTANTIATE_TEST_SUITE_P(<Solver>InfeasibleSubsystemTest,
//                          InfeasibleSubsystemTest,
//                          testing::Values(InfeasibleSubsystemTestParameters(
//                              {.solver_type = SolverType::k<Solver>,
//                               .support_menu = {
//                                   .supports_infeasible_subsystems =
//                                   false}})));
class InfeasibleSubsystemTest
    : public ::testing::TestWithParam<InfeasibleSubsystemTestParameters> {};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_SOLVER_TESTS_INFEASIBLE_SUBSYSTEM_TESTS_H_
