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
#include "ortools/math_opt/cpp/linear_constraint.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/indexed_model.h"
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
  struct PrimalSolution {
    PrimalSolution() = default;
    PrimalSolution(IndexedModel* model, IndexedPrimalSolution indexed_solution);

    VariableMap<double> variable_values;
    double objective_value = 0.0;
  };
  struct PrimalRay {
    PrimalRay() = default;
    PrimalRay(IndexedModel* model, IndexedPrimalRay indexed_ray);

    VariableMap<double> variable_values;
  };
  struct DualSolution {
    DualSolution() = default;
    DualSolution(IndexedModel* model, IndexedDualSolution indexed_solution);

    LinearConstraintMap<double> dual_values;
    VariableMap<double> reduced_costs;
    double objective_value = 0.0;
  };
  struct DualRay {
    DualRay() = default;
    DualRay(IndexedModel* model, IndexedDualRay indexed_ray);

    LinearConstraintMap<double> dual_values;
    VariableMap<double> reduced_costs;
  };
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
  // (solver.h) and the actual solver (SCIP, ...). For some model modifications,
  // the wrapper can support modifying the actual solver's in-memory model
  // instead of recreating it from scratch. This member is set to true when this
  // happens.
  bool attempted_incremental_solve = false;
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CPP_RESULT_H_
