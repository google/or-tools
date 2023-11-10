// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_GSCIP_GSCIP_CALLBACK_RESULT_H_
#define OR_TOOLS_GSCIP_GSCIP_CALLBACK_RESULT_H_

#include "scip/type_result.h"
#include "scip/type_retcode.h"

namespace operations_research {

// Equivalent to type_result.h in SCIP.
enum class GScipCallbackResult {
  kDidNotRun = 1,  // The method was not executed.
  kDelayed = 2,    // The method was not executed, should be called again later.
  kDidNotFind = 3,  // The method was executed, but failed finding anything.
  kFeasible = 4,    // No infeasibility could be found.
  kInfeasible = 5,  // An infeasibility was detected.
  kUnbounded = 6,   // An unboundedness was detected.
  kCutOff = 7,      // The current node is infeasible and can be cut off.
  kSeparated = 8,   // The method added a cutting plane.
  // The method added a cutting plane and a new separation round should
  // immediately start.
  kNewRound = 9,
  kReducedDomain = 10,      // The method reduced the domain of a variable.
  kConstraintAdded = 11,    // The method added a constraint.
  kConstraintChanged = 12,  // The method changed a constraint.
  kBranched = 13,           // The method created a branching.
  kSolveLp = 14,            // The current node's LP must be solved.
  kFoundSolution = 15,      // The method found a feasible primal solution.
  // The method interrupted its execution, but can continue if needed.
  kSuspend = 16,
  kSuccess = 17,  // The method was successfully executed.
  // The processing of the branch-and-bound node should stopped and continued
  // later.
  kDelayNode = 18
};

SCIP_RESULT ConvertGScipCallbackResult(GScipCallbackResult result);

}  // namespace operations_research

#endif  // OR_TOOLS_GSCIP_GSCIP_CALLBACK_RESULT_H_
