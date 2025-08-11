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

#include "ortools/math_opt/solvers/gscip/gscip_callback_result.h"

#include "scip/type_result.h"

namespace operations_research {

SCIP_RESULT ConvertGScipCallbackResult(const GScipCallbackResult result) {
  switch (result) {
    case GScipCallbackResult::kDidNotRun:
      return SCIP_DIDNOTRUN;
    case GScipCallbackResult::kDelayed:
      return SCIP_DELAYED;
    case GScipCallbackResult::kDidNotFind:
      return SCIP_DIDNOTFIND;
    case GScipCallbackResult::kFeasible:
      return SCIP_FEASIBLE;
    case GScipCallbackResult::kInfeasible:
      return SCIP_INFEASIBLE;
    case GScipCallbackResult::kUnbounded:
      return SCIP_UNBOUNDED;
    case GScipCallbackResult::kCutOff:
      return SCIP_CUTOFF;
    case GScipCallbackResult::kSeparated:
      return SCIP_SEPARATED;
    case GScipCallbackResult::kNewRound:
      return SCIP_NEWROUND;
    case GScipCallbackResult::kReducedDomain:
      return SCIP_REDUCEDDOM;
    case GScipCallbackResult::kConstraintAdded:
      return SCIP_CONSADDED;
    case GScipCallbackResult::kConstraintChanged:
      return SCIP_CONSCHANGED;
    case GScipCallbackResult::kBranched:
      return SCIP_BRANCHED;
    case GScipCallbackResult::kSolveLp:
      return SCIP_SOLVELP;
    case GScipCallbackResult::kFoundSolution:
      return SCIP_FOUNDSOL;
    case GScipCallbackResult::kSuspend:
      return SCIP_SUSPENDED;
    case GScipCallbackResult::kSuccess:
      return SCIP_SUCCESS;
    case GScipCallbackResult::kDelayNode:
      return SCIP_DELAYNODE;
  }
}

}  // namespace operations_research
