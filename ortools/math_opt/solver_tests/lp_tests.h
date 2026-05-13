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

#ifndef OR_TOOLS_MATH_OPT_SOLVER_TESTS_LP_TESTS_H_
#define OR_TOOLS_MATH_OPT_SOLVER_TESTS_LP_TESTS_H_

#include <memory>
#include <ostream>
#include <utility>

#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/solver_tests/base_solver_test.h"

namespace operations_research {
namespace math_opt {

struct SimpleLpTestParameters {
  // The tested solver.
  SolverType solver_type;
  SolveParameters parameters;

  // True if a dual solution is returned.
  bool supports_duals;

  // True if the solver produces a basis.
  bool supports_basis;

  bool ensures_primal_ray;
  bool ensures_dual_ray;

  // If true, then TERMINATION_REASON_INFEASIBLE_OR_UNBOUNDED is never returned,
  // it is always disambiguated to either TERMINATION_REASON_INFEASIBLE or
  // TERMINATION_REASON_UNBOUNDED.
  bool disallows_infeasible_or_unbounded;

  SimpleLpTestParameters(const SolverType solver_type,
                         SolveParameters parameters, const bool supports_duals,
                         const bool supports_basis,
                         const bool ensures_primal_ray,
                         const bool ensures_dual_ray,
                         const bool disallows_infeasible_or_unbounded)
      : solver_type(solver_type),
        parameters(std::move(parameters)),
        supports_duals(supports_duals),
        supports_basis(supports_basis),
        ensures_primal_ray(ensures_primal_ray),
        ensures_dual_ray(ensures_dual_ray),
        disallows_infeasible_or_unbounded(disallows_infeasible_or_unbounded) {}

  friend std::ostream& operator<<(std::ostream& out,
                                  const SimpleLpTestParameters& params);
};

class SimpleLpTest : public ::testing::TestWithParam<SimpleLpTestParameters> {
 protected:
  SolverType TestedSolver() const { return GetParam().solver_type; }
  absl::StatusOr<SolveResult> SimpleSolve(const Model& model) {
    return Solve(model, TestedSolver(), {.parameters = GetParam().parameters});
  }
};

class IncrementalLpTest : public BaseSolverTest {
 protected:
  IncrementalLpTest();

  Model model_;
  const Variable zero_;
  const Variable x_1_;
  const Variable y_1_;
  const LinearConstraint c_1_;
  const Variable x_2_;
  const Variable y_2_;
  const LinearConstraint c_2_;
  const Variable x_3_;
  const Variable y_3_;
  const LinearConstraint c_3_;

  std::unique_ptr<IncrementalSolver> solver_;
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_SOLVER_TESTS_LP_TESTS_H_
