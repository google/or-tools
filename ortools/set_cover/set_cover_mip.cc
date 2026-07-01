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

#include <limits>
#include <optional>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/strong_int.h"
#include "ortools/base/strong_vector.h"
#include "ortools/math_opt/cpp/math_opt.h"
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
using StrictVector = util_intops::StrongVector<IndexType, ValueType>;

bool SetCoverMip::OptimizeImpl(absl::Span<const SubsetIndex> focus) {
  inv()->ReportLowerBound(0.0, true);
  StopWatch stop_watch(&run_time_);
  const SubsetIndex num_subsets(model()->num_subsets());
  const ElementIndex num_elements(model()->num_elements());
  math_opt::SolverType solver_type;
  switch (mip_solver_) {
    case SetCoverMipSolver::SCIP:
      solver_type = math_opt::SolverType::kGscip;
      break;
    case SetCoverMipSolver::GUROBI:
      solver_type = math_opt::SolverType::kGurobi;
      break;
    case SetCoverMipSolver::SAT:
      if (!use_integers_) {
        VLOG(1) << "Defaulting to integer variables with SAT";
        use_integers_ = true;
      }
      solver_type = math_opt::SolverType::kCpSat;
      break;
    case SetCoverMipSolver::GLOP:
      VLOG(1) << "Defaulting to linear relaxation with GLOP";
      use_integers_ = false;
      solver_type = math_opt::SolverType::kGlop;
      break;
    case SetCoverMipSolver::PDLP:
      if (use_integers_) {
        VLOG(1) << "Defaulting to linear relaxation with PDLP";
        use_integers_ = false;
      }
      solver_type = math_opt::SolverType::kPdlp;
      break;
    default:
      LOG(WARNING) << "Unknown solver value, defaulting to SCIP";
      solver_type = math_opt::SolverType::kGscip;
  }
  // We use MathOpt now.
  // We construct a restricted MIP, omitting all the parts of the problem
  // that are already fixed in the invariant. The goal is to not spend time
  // sending data, and having the MIP solver re-discover fixed variables.
  math_opt::Model mip_model("set cover mip");
  mip_model.Minimize(0.0);

  StrictVector<ElementIndex, std::optional<math_opt::LinearConstraint>>
      constraints(num_elements, std::nullopt);
  StrictVector<SubsetIndex, std::optional<math_opt::Variable>> vars(
      num_subsets, std::nullopt);
  ElementToIntVector coverage_outside_focus =
      Subtract(inv()->coverage(), inv()->ComputeCoverageInFocus(focus));
  for (const SubsetIndex subset : focus) {
    if (use_integers_) {
      vars[subset] =
          mip_model.AddBinaryVariable(absl::StrCat("subset_", subset.value()));
    } else {
      vars[subset] = mip_model.AddContinuousVariable(
          0.0, 1.0, absl::StrCat("subset_", subset.value()));
    }
    mip_model.AddToObjective(*vars[subset] * model()->subset_costs()[subset]);
    for (const ElementIndex element : model()->columns()[subset]) {
      // The model should only contain elements that are not forcibly covered
      // by subsets outside the focus.
      if (coverage_outside_focus[element] != 0) continue;
      if (!constraints[element].has_value()) {
        constexpr double kInfinity = std::numeric_limits<double>::infinity();
        constraints[element] = mip_model.AddLinearConstraint(
            1.0, kInfinity, absl::StrCat("element_", element.value()));
      }
      mip_model.set_coefficient(*constraints[element], *vars[subset], 1.0);
    }
  }

  math_opt::SolveArguments args;
  args.parameters.time_limit = time_limit();
  // Call the solver.
  const absl::StatusOr<math_opt::SolveResult> result =
      math_opt::Solve(mip_model, solver_type, args);
  CHECK_OK(result.status());
  solve_status_ = result->termination.reason;
  switch (solve_status_) {
    case math_opt::TerminationReason::kOptimal:
    case math_opt::TerminationReason::kFeasible:
      break;
    case math_opt::TerminationReason::kInfeasible:
      LOG(ERROR) << "Did not find solution. Problem is infeasible.";
      break;
    case math_opt::TerminationReason::kUnbounded:
    case math_opt::TerminationReason::kInfeasibleOrUnbounded:
      LOG(ERROR) << "Did not find solution. Problem is unbounded.";
      break;
    default:
      LOG(ERROR) << "Solving resulted in an error. Reason: " << solve_status_;
      return false;
  }
  if (use_integers_) {
    using CL = SetCoverInvariant::ConsistencyLevel;
    for (const SubsetIndex subset : focus) {
      if (result->variable_values().at(*vars[subset]) > 0.9) {
        if (!inv()->is_selected()[subset]) {
          inv()->Select(subset, CL::kCostAndCoverage);
        }
      } else {
        CHECK_LT(result->variable_values().at(*vars[subset]), 0.1)
            << "Solution must be either near 0 or near 1 but got "
            << result->variable_values().at(*vars[subset]);
        if (inv()->is_selected()[subset]) {
          inv()->Deselect(subset, CL::kCostAndCoverage);
        }
      }
    }
  } else {
    solution_weights_.assign(num_subsets.value(), 0.0);
    for (const SubsetIndex subset : focus) {
      solution_weights_[subset] = result->variable_values().at(*vars[subset]);
    }
    // Report the objective value as a lower bound, and mention that the cost is
    // not consistent with the solution.
    inv()->ReportLowerBound(result->objective_value(), false);
  }
  return true;
}

}  // namespace operations_research
