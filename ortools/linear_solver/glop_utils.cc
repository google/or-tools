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

#include "ortools/linear_solver/glop_utils.h"

namespace operations_research {

MPSolver::ResultStatus GlopToMPSolverResultStatus(glop::ProblemStatus s) {
  switch (s) {
    case glop::ProblemStatus::OPTIMAL:
      return MPSolver::OPTIMAL;
    case glop::ProblemStatus::PRIMAL_FEASIBLE:
      return MPSolver::FEASIBLE;

    // Note(user): MPSolver does not have the equivalent of
    // INFEASIBLE_OR_UNBOUNDED however UNBOUNDED is almost never relevant in
    // applications, so we decided to report this status as INFEASIBLE since
    // it should almost always be the case. Historically, we where reporting
    // ABNORMAL, but that was more confusing than helpful.
    //
    // TODO(user): We could argue that it is infeasible to find the optimal of
    // an unbounded problem. So it might just be simpler to completely get rid
    // of the MpSolver::UNBOUNDED status that seems to never be used
    // programmatically.
    case glop::ProblemStatus::INFEASIBLE_OR_UNBOUNDED:  // PASS_THROUGH_INTENDED
    case glop::ProblemStatus::PRIMAL_INFEASIBLE:        // PASS_THROUGH_INTENDED
    case glop::ProblemStatus::DUAL_UNBOUNDED:
      return MPSolver::INFEASIBLE;

    case glop::ProblemStatus::DUAL_INFEASIBLE:  // PASS_THROUGH_INTENDED
    case glop::ProblemStatus::PRIMAL_UNBOUNDED:
      return MPSolver::UNBOUNDED;

    case glop::ProblemStatus::DUAL_FEASIBLE:  // PASS_THROUGH_INTENDED
    case glop::ProblemStatus::INIT:
      return MPSolver::NOT_SOLVED;

    case glop::ProblemStatus::ABNORMAL:   // PASS_THROUGH_INTENDED
    case glop::ProblemStatus::IMPRECISE:  // PASS_THROUGH_INTENDED
    case glop::ProblemStatus::INVALID_PROBLEM:
      return MPSolver::ABNORMAL;
  }
  LOG(DFATAL) << "Invalid glop::ProblemStatus " << s;
  return MPSolver::ABNORMAL;
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
