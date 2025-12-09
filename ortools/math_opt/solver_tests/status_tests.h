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

#ifndef ORTOOLS_MATH_OPT_SOLVER_TESTS_STATUS_TESTS_H_
#define ORTOOLS_MATH_OPT_SOLVER_TESTS_STATUS_TESTS_H_

#include <ostream>
#include <utility>

#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "ortools/base/logging.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {

struct StatusTestParameters {
  // The tested solver.
  SolverType solver_type;
  SolveParameters parameters;

  // If true, then a problem status with primal_or_dual_infeasible = true is
  // never returned.
  bool disallow_primal_or_dual_infeasible;

  bool supports_iteration_limit;

  // True if the tests should be performed with integer variables.
  bool use_integer_variables;

  bool supports_node_limit;

  // True if the solver support SolveInterrupter.
  bool support_interrupter;

  bool supports_one_thread;

  StatusTestParameters(const SolverType solver_type, SolveParameters parameters,
                       const bool disallow_primal_or_dual_infeasible,
                       const bool supports_iteration_limit,
                       const bool use_integer_variables,
                       const bool supports_node_limit,
                       const bool support_interrupter,
                       const bool supports_one_thread)
      : solver_type(solver_type),
        parameters(std::move(parameters)),
        disallow_primal_or_dual_infeasible(disallow_primal_or_dual_infeasible),
        supports_iteration_limit(supports_iteration_limit),
        use_integer_variables(use_integer_variables),
        supports_node_limit(supports_node_limit),
        support_interrupter(support_interrupter),
        supports_one_thread(supports_one_thread) {}

  friend std::ostream& operator<<(std::ostream& out,
                                  const StatusTestParameters& params);
};

class StatusTest : public ::testing::TestWithParam<StatusTestParameters> {
 protected:
  SolverType TestedSolver() const { return GetParam().solver_type; }
  absl::StatusOr<SolveResult> SimpleSolve(const Model& model) {
    return Solve(model, TestedSolver(), {.parameters = GetParam().parameters});
  }
};

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_SOLVER_TESTS_STATUS_TESTS_H_
