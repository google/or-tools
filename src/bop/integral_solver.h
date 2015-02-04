// Copyright 2010-2014 Google
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

#ifndef OR_TOOLS_BOP_INTEGRAL_SOLVER_H_
#define OR_TOOLS_BOP_INTEGRAL_SOLVER_H_

#include <map>
#include <vector>

#include "bop/bop_base.h"
#include "bop/bop_parameters.pb.h"
#include "bop/bop_solution.h"
#include "bop/bop_types.h"
#include "lp_data/lp_data.h"

namespace operations_research {
namespace bop {
// This class implements an Integer Programming solver, i.e. the solver solves
// problems with both integral and boolean variables, linear constraint and
// linear objective function.
// Note that the current implementation is limited to integral coefficients.
// TODO(user): Add scaling to deal with any coefficient.
class IntegralSolver {
 public:
  IntegralSolver();

  // Sets the solver parameters.
  // See the proto for an extensive documentation.
  void SetParameters(const BopParameters& parameters) {
    parameters_ = parameters;
  }
  BopParameters parameters() const { return parameters_; }

  // Solves the given linear program and returns the solve status.
  BopSolveStatus Solve(const glop::LinearProgram& linear_problem)
      MUST_USE_RESULT;

  // Returns the objective value of the solution with its offset.
  glop::Fractional objective_value() const { return objective_value_; }

  // Returns the best bound found so far.
  glop::Fractional best_bound() const { return best_bound_; }

  // Returns the solution values. Note that the values only make sense when a
  // solution is found.
  const glop::DenseRow& variable_values() const { return variable_values_; }

 private:
  // Solves the given linear program and returns the solve status.
  BopSolveStatus InternalSolve(const glop::LinearProgram& linear_problem,
                               const BopParameters& parameters,
                               glop::DenseRow* variable_values,
                               glop::Fractional* objective_value,
                               glop::Fractional* best_bound) MUST_USE_RESULT;

  BopParameters parameters_;
  glop::DenseRow variable_values_;
  glop::Fractional objective_value_;
  glop::Fractional best_bound_;
};
}  // namespace bop
}  // namespace operations_research
#endif  // OR_TOOLS_BOP_INTEGRAL_SOLVER_H_
