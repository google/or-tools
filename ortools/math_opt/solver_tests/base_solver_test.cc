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

#include "ortools/math_opt/solver_tests/base_solver_test.h"

#include <optional>

#include "ortools/base/linked_hash_map.h"
#include "ortools/base/logging.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research {
namespace math_opt {

SolverType BaseSolverTest::TestedSolver() const { return GetParam(); }

// TODO(b/200695800): add a common parameter here instead of this hard coded
// value and use a test parameter to choose the solve algorithm for primal and
// dual rays (for some solvers we may actually want to test with multiple
// algorithms).
bool ActivatePrimalRay(const SolverType solver_type, SolveParameters& params) {
  switch (solver_type) {
    case SolverType::kGurobi: {
      params.gurobi.param_values["InfUnbdInfo"] = "1";
      return true;
    }
    case SolverType::kPdlp:
      return true;
    case SolverType::kGscip:
    case SolverType::kGlop:
    case SolverType::kCpSat:
    // TODO(b/260616646): support ECOS
    case SolverType::kEcos:
      return false;
    case SolverType::kGlpk:
      // We have to use PRIMAL_SIMPLEX (the default) for primal rays..
      params.glpk.compute_unbound_rays_if_possible = true;
      return true;
    case SolverType::kScs:
      return false;
    case SolverType::kHighs:
      return false;
    default:
      LOG(FATAL)
          << "Solver " << solver_type
          << " is not known; please update this function for this solver.";
  }
}

// TODO(b/200695800): see the TODO for ActivatePrimalRay().
bool ActivateDualRay(const SolverType solver_type, SolveParameters& params) {
  switch (solver_type) {
    case SolverType::kGurobi: {
      params.gurobi.param_values["InfUnbdInfo"] = "1";
      return true;
    }
    case SolverType::kPdlp:
      return true;
    case SolverType::kGscip:
    case SolverType::kGlop:
    case SolverType::kCpSat:
    // TODO(b/260616646): support ECOS
    case SolverType::kEcos:
      return false;
    case SolverType::kGlpk:
      // We have to use DUAL_SIMPLEX to have dual rays (and PRIMAL_SIMPLEX for
      // primal ones).
      params.lp_algorithm = LPAlgorithm::kDualSimplex;
      params.glpk.compute_unbound_rays_if_possible = true;
      return true;
    case SolverType::kScs:
      return false;
    case SolverType::kHighs:
      return false;
    default:
      LOG(FATAL)
          << "Solver " << solver_type
          << " is not known; please update this function for this solver.";
  }
}

}  // namespace math_opt
}  // namespace operations_research
