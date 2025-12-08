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

#ifndef OR_TOOLS_MATH_OPT_SOLVER_TESTS_LOGICAL_CONSTRAINT_TESTS_H_
#define OR_TOOLS_MATH_OPT_SOLVER_TESTS_LOGICAL_CONSTRAINT_TESTS_H_

#include <ostream>

#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {

struct LogicalConstraintTestParameters {
  explicit LogicalConstraintTestParameters(
      SolverType solver_type, SolveParameters parameters,
      bool supports_integer_variables, bool supports_sos1, bool supports_sos2,
      bool supports_indicator_constraints,
      bool supports_incremental_add_and_deletes,
      bool supports_incremental_variable_deletions,
      bool supports_deleting_indicator_variables,
      bool supports_updating_binary_variables,
      bool supports_sos_on_expressions = true);

  // The tested solver.
  SolverType solver_type;

  SolveParameters parameters;

  // True if the solver supports integer variables.
  bool supports_integer_variables;

  // True if the solver supports SOS1 constraints.
  bool supports_sos1;

  // True if the solver supports SOS2 constraints.
  bool supports_sos2;

  // True if the solver supports indicator constraints.
  bool supports_indicator_constraints;

  // True if the solver supports incremental updates that add and/or delete
  // any of the logical constraint types it supports.
  bool supports_incremental_add_and_deletes;

  // True if the solver supports updates that delete (non-indicator) variables.
  bool supports_incremental_variable_deletions;

  // True if the solver supports updates that delete indicator variables.
  bool supports_deleting_indicator_variables;

  // True if the solver supports updates (changing bounds or vartype) to binary
  // variables.
  bool supports_updating_binary_variables;

  // True if the solver supports SOS constraints on expressions. False if
  // SOS constraints are only supported on singleton variables.
  bool supports_sos_on_expressions;
};

std::ostream& operator<<(std::ostream& out,
                         const LogicalConstraintTestParameters& params);

// A suite of unit tests for logical constraints.
//
// To use these tests, in file <solver>_test.cc, write:
//   INSTANTIATE_TEST_SUITE_P(
//       <Solver>SimpleLogicalConstraintTest, SimpleLogicalConstraint,
//       testing::Values(LogicalConstraintTestParameters(
//                       SolverType::k<Solver>, parameters,
//                       /*supports_integer_variables=*/false,
//                       /*supports_sos1=*/false, /*supports_sos2=*/false,
//                       /*supports_indicator=*/false,
//                       /*supports_incremental_add_and_deletes=*/false,
//                       /*supports_incremental_variable_deletions=*/false,
//                       /*supports_deleting_indicator_variables=*/false,
//                       /*supports_updating_binary_variables=*/false)));
class SimpleLogicalConstraintTest
    : public testing::TestWithParam<LogicalConstraintTestParameters> {
 protected:
  absl::StatusOr<SolveResult> SimpleSolve(const Model& model) {
    return Solve(model, GetParam().solver_type,
                 {.parameters = GetParam().parameters});
  }
};

// A suite of unit tests for logical constraints.
//
// To use these tests, in file <solver>_test.cc, write:
//   INSTANTIATE_TEST_SUITE_P(
//       <Solver>IncrementalLogicalConstraintTest, IncrementalLogicalConstraint,
//       testing::Values(LogicalConstraintTestParameters(
//                       SolverType::k<Solver>, parameters,
//                       /*supports_integer_variables=*/false,
//                       /*supports_sos1=*/false, /*supports_sos2=*/false,
//                       /*supports_indicator=*/false,
//                       /*supports_incremental_add_and_deletes=*/false,
//                       /*supports_incremental_variable_deletions=*/false,
//                       /*supports_deleting_indicator_variables=*/false,
//                       /*supports_updating_binary_variables=*/false)));
class IncrementalLogicalConstraintTest
    : public testing::TestWithParam<LogicalConstraintTestParameters> {};

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_SOLVER_TESTS_LOGICAL_CONSTRAINT_TESTS_H_
