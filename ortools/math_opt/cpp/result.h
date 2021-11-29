// Copyright 2010-2021 Google LLC
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

#ifndef OR_TOOLS_MATH_OPT_CPP_RESULT_H_
#define OR_TOOLS_MATH_OPT_CPP_RESULT_H_

#include <string>
#include <vector>

#include "ortools/base/logging.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "ortools/math_opt/core/indexed_model.h"
#include "ortools/math_opt/cpp/linear_constraint.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/base/protoutil.h"

namespace operations_research {
namespace math_opt {

// The result of solving an optimization problem with MathOpt::Solve.
//
// TODO(b/172211596): there is already a parallel proto named solve result, the
// naming convention should be more consistent.
struct Result {
  // A solution to an optimization problem.
  //
  // E.g. consider a simple linear program:
  //   min c * x
  //   s.t. A * x >= b
  //   x >= 0.
  // A primal solution is assignment values to x. It is feasible if it satisfies
  // A * x >= b and x >= 0 from above. In the class PrimalSolution below,
  // variable_values is x and objective_value is c * x.
  //
  // For the general case of a MathOpt optimization model, see
  // go/mathopt-solutions for details.
  struct PrimalSolution {
    PrimalSolution() = default;
    PrimalSolution(IndexedModel* model, IndexedPrimalSolution indexed_solution);

    VariableMap<double> variable_values;
    double objective_value = 0.0;
  };

  // A direction of unbounded improvement to an optimization problem;
  // equivalently, a certificate of infeasibility for the dual of the
  // optimization problem.
  //
  // E.g. consider a simple linear program:
  //   min c * x
  //   s.t. A * x >= b
  //   x >= 0
  // A primal ray is an x that satisfies:
  //   c * x < 0
  //   A * x >= 0
  //   x >= 0
  // Observe that given a feasible solution, any positive multiple of the primal
  // ray plus that solution is still feasible, and gives a better objective
  // value. A primal ray also proves the dual optimization problem infeasible.
  //
  // In the class PrimalRay below, variable_values is this x.
  //
  // For the general case of a MathOpt optimization model, see
  // go/mathopt-solutions for details.
  struct PrimalRay {
    PrimalRay() = default;
    PrimalRay(IndexedModel* model, IndexedPrimalRay indexed_ray);

    VariableMap<double> variable_values;
  };

  // A solution to the dual of an optimization problem.
  //
  // E.g. consider the primal dual pair linear program pair:
  //   (Primal)             (Dual)
  //   min c * x            max b * y
  //   s.t. A * x >= b      s.t. y * A + r = c
  //   x >= 0               y, r >= 0.
  // The dual solution is the pair (y, r). It is feasible if it satisfies the
  // constraints from (Dual) above.
  //
  // Below, y is dual_values, r is reduced_costs, and b * y is objective value.
  //
  // For the general case, see go/mathopt-solutions and go/mathopt-dual (and
  // note that the dual objective depends on r in the general case).
  struct DualSolution {
    DualSolution() = default;
    DualSolution(IndexedModel* model, IndexedDualSolution indexed_solution);

    LinearConstraintMap<double> dual_values;
    VariableMap<double> reduced_costs;
    double objective_value = 0.0;
  };

  // A direction of unbounded improvement to the dual of an optimization,
  // problem; equivalently, a certificate of primal infeasibility.
  //
  // E.g. consider the primal dual pair linear program pair:
  //    (Primal)              (Dual)
  //    min c * x             max b * y
  //    s.t. A * x >= b       s.t. y * A + r = c
  //    x >= 0                y, r >= 0.
  // The dual ray is the pair (y, r) satisfying:
  //   b * y > 0
  //   y * A + r = 0
  //   y, r >= 0
  // Observe that adding a positive multiple of (y, r) to dual feasible solution
  // maintains dual feasibility and improves the objective (proving the dual is
  // unbounded). The dual ray also proves the primal problem is infeasible.
  //
  // In the class DualRay below, y is dual_values and r is reduced_costs.
  //
  // For the general case, see go/mathopt-solutions and go/mathopt-dual (and
  // note that the dual objective depends on r in the general case).
  struct DualRay {
    DualRay() = default;
    DualRay(IndexedModel* model, IndexedDualRay indexed_ray);

    LinearConstraintMap<double> dual_values;
    VariableMap<double> reduced_costs;
  };

  // A combinatorial characterization for a solution to a linear program.
  //
  // The simplex method for solving linear programs always returns a "basic
  // feasible solution" which can be described combinatorially as a Basis. A
  // basis assigns a BasisStatus for every variable and linear constraint.
  //
  // E.g. consider a standard form LP:
  //   min c * x
  //   s.t. A * x = b
  //   x >= 0
  // that has more variables than constraints and with full row rank A.
  //
  // Let n be the number of variables and m the number of linear constraints. A
  // valid basis for this problem can be constructed as follows:
  //  * All constraints will have basis status FIXED.
  //  * Pick m variables such that the columns of A are linearly independent and
  //    assign the status BASIC.
  //  * Assign the status AT_LOWER for the remaining n - m variables.
  //
  // The basic solution for this basis is the unique solution of A * x = b that
  // has all variables with status AT_LOWER fixed to their lower bounds (all
  // zero). The resulting solution is called a basic feasible solution if it
  // also satisfies x >= 0.
  //
  // See go/mathopt-basis for treatment of the general case and an explanation
  // of how a dual solution is determined for a basis.
  struct Basis {
    Basis() = default;
    Basis(IndexedModel* model, IndexedBasis indexed_basis);

    LinearConstraintMap<BasisStatus> constraint_status;
    VariableMap<BasisStatus> variable_status;
  };

  Result(IndexedModel* model, const SolveResultProto& solve_result);

  // The objective value of the best primal solution. Will CHECK fail if there
  // are no primal solutions.
  double objective_value() const {
    CHECK(has_solution());
    return primal_solutions[0].objective_value;
  }

  absl::Duration solve_time() const {
    return util_time::DecodeGoogleApiProto(solve_stats.solve_time()).value();
  }

  // Indicates if at least one primal feasible solution is available.
  //
  // When termination_reason is TERMINATION_REASON_OPTIMAL, this is guaranteed
  // to be true and need not be checked.
  bool has_solution() const { return !primal_solutions.empty(); }

  // The variable values from the best primal solution. Will CHECK fail if there
  // are no primal solutions.
  const VariableMap<double>& variable_values() const {
    CHECK(has_solution());
    return primal_solutions[0].variable_values;
  }

  // Indicates if at least one primal ray is available.
  //
  // This is NOT guaranteed to be true when termination_reason is
  // UNBOUNDED or DUAL_INFEASIBLE.
  bool has_ray() const { return !primal_rays.empty(); }

  // The variable values from the first primal ray. Will CHECK fail if there
  // are no primal rays.
  const VariableMap<double>& ray_variable_values() const {
    CHECK(has_ray());
    return primal_rays[0].variable_values;
  }

  // Indicates if at least one dual solution is available.
  //
  // This is NOT guaranteed to be true when termination_reason is
  // TERMINATION_REASON_OPTIMAL.
  bool has_dual_solution() const { return !dual_solutions.empty(); }

  // The dual values from the best dual solution. Will CHECK fail if there
  // are no dual solutions.
  const LinearConstraintMap<double>& dual_values() const {
    CHECK(has_dual_solution());
    return dual_solutions[0].dual_values;
  }

  // The reduced from the best dual solution. Will CHECK fail if there
  // are no dual solutions.
  // TODO(b/174564572): if reduced_costs in DualSolution was something like
  // dual_reduced cost it would help prevent people forgetting to call
  // has_dual_solution().
  const VariableMap<double>& reduced_costs() const {
    CHECK(has_dual_solution());
    return dual_solutions[0].reduced_costs;
  }

  // Indicates if at least one dual ray is available.
  //
  // This is NOT guaranteed to be true when termination_reason is
  // INFEASIBLE.
  bool has_dual_ray() const { return !dual_rays.empty(); }

  // The dual values from the first dual ray. Will CHECK fail if there
  // are no dual rays.
  // TODO(b/174564572): note the redunancy of the "double" dual and the
  // inconsistency with `dual_values` in the proto.
  const LinearConstraintMap<double>& ray_dual_values() const {
    CHECK(has_dual_ray());
    return dual_rays[0].dual_values;
  }

  // The reduced from the first dual ray. Will CHECK fail if there
  // are no dual rays.
  const VariableMap<double>& ray_reduced_costs() const {
    CHECK(has_dual_ray());
    return dual_rays[0].reduced_costs;
  }

  // Indicates if at least one basis is available.
  bool has_basis() const { return !basis.empty(); }

  // The constraint basis status for the first primal/dual pair.
  const LinearConstraintMap<BasisStatus>& constraint_status() const {
    CHECK(has_basis());
    return basis[0].constraint_status;
  }

  // The variable basis status for the first primal/dual pair.
  const VariableMap<BasisStatus>& variable_status() const {
    CHECK(has_basis());
    return basis[0].variable_status;
  }

  std::vector<std::string> warnings;
  SolveResultProto::TerminationReason termination_reason =
      SolveResultProto::TERMINATION_REASON_UNSPECIFIED;
  std::string termination_detail;
  SolveStatsProto solve_stats;

  // Primal solutions should be ordered best objective value first.
  std::vector<PrimalSolution> primal_solutions;
  std::vector<PrimalRay> primal_rays;

  // Dual solutions should be ordered best objective value first.
  std::vector<DualSolution> dual_solutions;
  std::vector<DualRay> dual_rays;

  // basis[i] corresponds to the primal dual pair:
  // {primal_solutions[i], dual_solutions[i]}. These fields must have at least
  // as many elements as basis. Basis will only be populated for LPs, and may
  // not be populated.
  std::vector<Basis> basis;

  // Set to true if MathOpt::Solve() has attempted an incremental solve instead
  // of starting from scratch.
  //
  // We have three components involve in Solve(): MathOpt, the solver wrapper
  // (solver.h) and the actual solver (SCIP, ...). For some model modifications,
  // the wrapper can support modifying the actual solver's in-memory model
  // instead of recreating it from scratch. This member is set to true when this
  // happens.
  bool attempted_incremental_solve = false;
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CPP_RESULT_H_
