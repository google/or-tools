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

#include "ortools/set_cover/set_cover_mip.h"

#include <cstdint>
#include <limits>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_invariant.h"
#include "ortools/set_cover/set_cover_model.h"

namespace operations_research {

namespace {
// Returns the vector a - b.
ElementToIntVector Subtract(const ElementToIntVector& a,
                            const ElementToIntVector& b) {
  ElementToIntVector delta(a.size());
  DCHECK_EQ(a.size(), b.size());
  for (const ElementIndex i : a.index_range()) {
    delta[i] = a[i] - b[i];
  }
  return delta;
}
}  // namespace

template <typename IndexType, typename ValueType>
using StrictVector = glop::StrictITIVector<IndexType, ValueType>;

bool SetCoverMip::NextSolution(absl::Span<const SubsetIndex> focus) {
  inv()->ReportLowerBound(0.0, true);
  StopWatch stop_watch(&run_time_);
  const SubsetIndex num_subsets(model()->num_subsets());
  const ElementIndex num_elements(model()->num_elements());
  SubsetBoolVector choices = inv()->is_selected();
  MPSolver::OptimizationProblemType problem_type;
  switch (mip_solver_) {
    case SetCoverMipSolver::SCIP:
      problem_type = MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING;
      break;
    case SetCoverMipSolver::GUROBI:
      if (use_integers_) {
        problem_type = MPSolver::GUROBI_MIXED_INTEGER_PROGRAMMING;
      } else {
        problem_type = MPSolver::GUROBI_LINEAR_PROGRAMMING;
      }
      break;
    case SetCoverMipSolver::SAT:
      if (!use_integers_) {
        VLOG(1) << "Defaulting to integer variables with SAT";
        use_integers_ = true;
      }
      problem_type = MPSolver::SAT_INTEGER_PROGRAMMING;
      break;
    case SetCoverMipSolver::GLOP:
      VLOG(1) << "Defaulting to linear relaxation with GLOP";
      use_integers_ = false;
      problem_type = MPSolver::GLOP_LINEAR_PROGRAMMING;
      break;
    case SetCoverMipSolver::PDLP:
      if (use_integers_) {
        VLOG(1) << "Defaulting to linear relaxation with PDLP";
        use_integers_ = false;
      }
      problem_type = MPSolver::PDLP_LINEAR_PROGRAMMING;
      break;
    default:
      LOG(WARNING) << "Unknown solver value, defaulting to SCIP";
      problem_type = MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING;
  }
  // We are using MPSolver, which is deprecated, because MathOpt does not
  // provide an interface without using protobufs.
  // We construct a restricted MIP, omitting all the parts of the problem
  // that are already fixed in the invariant. The goal is to not spend time
  // sending data, and having the MIP solver re-discover fixed variables.
  MPSolver solver("set cover mip", problem_type);
  solver.SuppressOutput();
  MPObjective* const objective = solver.MutableObjective();
  objective->SetMinimization();

  StrictVector<ElementIndex, MPConstraint*> constraints(num_elements, nullptr);
  StrictVector<SubsetIndex, MPVariable*> vars(num_subsets, nullptr);
  ElementToIntVector coverage_outside_focus =
      Subtract(inv()->coverage(), inv()->ComputeCoverageInFocus(focus));
  for (const SubsetIndex subset : focus) {
    vars[subset] = solver.MakeVar(0, 1, use_integers_, "");
    objective->SetCoefficient(vars[subset], model()->subset_costs()[subset]);
    for (const ElementIndex element : model()->columns()[subset]) {
      // The model should only contain elements that are not forcibly covered
      // by subsets outside the focus.
      if (coverage_outside_focus[element] != 0) continue;
      if (constraints[element] == nullptr) {
        constexpr double kInfinity = std::numeric_limits<double>::infinity();
        constraints[element] = solver.MakeRowConstraint(1.0, kInfinity);
      }
      constraints[element]->SetCoefficient(vars[subset], 1);
    }
  }
  // set_time_limit takes milliseconds as a unit.
  solver.set_time_limit(static_cast<int64_t>(time_limit_in_seconds() * 1000));

  // Call the solver.
  solve_status_ = solver.Solve();
  switch (solve_status_) {
    case MPSolver::OPTIMAL:
      break;
    case MPSolver::FEASIBLE:
      break;
    case MPSolver::INFEASIBLE:
      LOG(ERROR) << "Did not find solution. Problem is infeasible.";
      break;
    case MPSolver::UNBOUNDED:
      LOG(ERROR) << "Did not find solution. Problem is unbounded.";
      break;
    default:
      LOG(ERROR) << "Solving resulted in an error.";
      return false;
  }
  if (use_integers_) {
    using CL = SetCoverInvariant::ConsistencyLevel;
    for (const SubsetIndex subset : focus) {
      if (vars[subset]->solution_value() > 0.9) {
        if (!inv()->is_selected()[subset]) {
          inv()->Select(subset, CL::kCostAndCoverage);
        }
      } else {
        CHECK_LT(vars[subset]->solution_value(), 0.1)
            << "Solution must be either near 0 or near 1 but got "
            << vars[subset]->solution_value();
        if (inv()->is_selected()[subset]) {
          inv()->Deselect(subset, CL::kCostAndCoverage);
        }
      }
    }
  } else {
    solution_weights_.assign(num_subsets.value(), 0.0);
    for (const SubsetIndex subset : focus) {
      solution_weights_[subset] = vars[subset]->solution_value();
    }
    // Report the objective value as a lower bound, and mention that the cost is
    // not consistent with the solution.
    inv()->ReportLowerBound(solver.Objective().Value(), false);
  }
  return true;
}

}  // namespace operations_research
