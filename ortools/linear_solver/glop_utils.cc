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

#include "ortools/linear_solver/glop_utils.h"

#include <variant>

#include "absl/functional/overload.h"
#include "absl/log/log.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/lp_data/lp_types.h"

namespace operations_research {

MPSolver::ResultStatus GlopToMPSolverResultStatus(const glop::SolveStatus s) {
  return std::visit(
      absl::Overload{
          [&](const glop::SolveStatus::Optimal&) { return MPSolver::OPTIMAL; },
          [&](const glop::SolveStatus::PrimalInfeasible&) {
            return MPSolver::INFEASIBLE;
          },
          [&](const glop::SolveStatus::DualInfeasible&) {
            return MPSolver::UNBOUNDED;
          },
          [&](const glop::SolveStatus::InfeasibleOrUnbounded&) {
            // Note(user): MPSolver does not have the equivalent of
            // INFEASIBLE_OR_UNBOUNDED however UNBOUNDED is almost never
            // relevant in applications, so we decided to report this status as
            // INFEASIBLE since it should almost always be the
            // case. Historically, we where reporting ABNORMAL, but that was
            // more confusing than helpful.
            //
            // TODO(user): We could argue that it is infeasible to find the
            // optimal of an unbounded problem. So it might just be simpler to
            // completely get rid of the MpSolver::UNBOUNDED status that seems
            // to never be used programmatically.
            return MPSolver::INFEASIBLE;
          },
          [&](const glop::SolveStatus::PrimalUnbounded&) {
            return MPSolver::UNBOUNDED;
          },
          [&](const glop::SolveStatus::DualUnbounded&) {
            return MPSolver::INFEASIBLE;
          },
          [&](const glop::SolveStatus::Init&) { return MPSolver::NOT_SOLVED; },
          [&](const glop::SolveStatus::PrimalFeasible&) {
            return MPSolver::FEASIBLE;
          },
          [&](const glop::SolveStatus::DualFeasible&) {
            return MPSolver::NOT_SOLVED;
          },
          [&](const glop::SolveStatus::Imprecise&) {
            return MPSolver::ABNORMAL;
          },
          [&](const glop::SolveStatus::Abnormal&) {
            return MPSolver::ABNORMAL;
          },
          [&](const glop::SolveStatus::InvalidProblem&) {
            return MPSolver::ABNORMAL;
          },
      },
      s.value);
}

MPSolver::BasisStatus GlopToMPSolverVariableStatus(glop::VariableStatus s) {
  switch (s) {
    case glop::VariableStatus::FREE:
      return MPSolver::FREE;
    case glop::VariableStatus::AT_LOWER_BOUND:
      return MPSolver::AT_LOWER_BOUND;
    case glop::VariableStatus::AT_UPPER_BOUND:
      return MPSolver::AT_UPPER_BOUND;
    case glop::VariableStatus::FIXED_VALUE:
      return MPSolver::FIXED_VALUE;
    case glop::VariableStatus::BASIC:
      return MPSolver::BASIC;
  }
  LOG(DFATAL) << "Unknown variable status: " << s;
  return MPSolver::FREE;
}

glop::VariableStatus MPSolverToGlopVariableStatus(MPSolver::BasisStatus s) {
  switch (s) {
    case MPSolver::FREE:
      return glop::VariableStatus::FREE;
    case MPSolver::AT_LOWER_BOUND:
      return glop::VariableStatus::AT_LOWER_BOUND;
    case MPSolver::AT_UPPER_BOUND:
      return glop::VariableStatus::AT_UPPER_BOUND;
    case MPSolver::FIXED_VALUE:
      return glop::VariableStatus::FIXED_VALUE;
    case MPSolver::BASIC:
      return glop::VariableStatus::BASIC;
  }
  LOG(DFATAL) << "Unknown variable status: " << s;
  return glop::VariableStatus::FREE;
}

MPSolver::BasisStatus GlopToMPSolverConstraintStatus(glop::ConstraintStatus s) {
  switch (s) {
    case glop::ConstraintStatus::FREE:
      return MPSolver::FREE;
    case glop::ConstraintStatus::AT_LOWER_BOUND:
      return MPSolver::AT_LOWER_BOUND;
    case glop::ConstraintStatus::AT_UPPER_BOUND:
      return MPSolver::AT_UPPER_BOUND;
    case glop::ConstraintStatus::FIXED_VALUE:
      return MPSolver::FIXED_VALUE;
    case glop::ConstraintStatus::BASIC:
      return MPSolver::BASIC;
  }
  LOG(DFATAL) << "Unknown constraint status: " << s;
  return MPSolver::FREE;
}

glop::ConstraintStatus MPSolverToGlopConstraintStatus(MPSolver::BasisStatus s) {
  switch (s) {
    case MPSolver::FREE:
      return glop::ConstraintStatus::FREE;
    case MPSolver::AT_LOWER_BOUND:
      return glop::ConstraintStatus::AT_LOWER_BOUND;
    case MPSolver::AT_UPPER_BOUND:
      return glop::ConstraintStatus::AT_UPPER_BOUND;
    case MPSolver::FIXED_VALUE:
      return glop::ConstraintStatus::FIXED_VALUE;
    case MPSolver::BASIC:
      return glop::ConstraintStatus::BASIC;
  }
  LOG(DFATAL) << "Unknown constraint status: " << s;
  return glop::ConstraintStatus::FREE;
}

}  // namespace operations_research
