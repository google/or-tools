// Copyright 2010-2017 Google
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

// Utility functions to interact with an lp solver from the SAT context.

#ifndef OR_TOOLS_SAT_LP_UTILS_H_
#define OR_TOOLS_SAT_LP_UTILS_H_

#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/sat/boolean_problem.pb.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace sat {

// Converts a MIP problem to a CpModel. Returns false if the coefficients
// couldn't be converted to integers with a good enough precision.
//
// Caveats:
//   - We do not support bound larger than or equal to 2^30.
//   - We cap unbounded variable at 2^30.
//   - Non-integer variable must have integer bounds.
//   - We do not scale the variable bounds, so by assuming that a non-integer
//     variable is integer, we may change the problem significantly if the
//     domain is small (like [0.0, 1.0]).
//
// TODO(user): Try to remove some of the restrictions.
bool ConvertMPModelProtoToCpModelProto(const MPModelProto& mp_model,
                                       CpModelProto* cp_model);

// Converts an integer program with only binary variables to a Boolean
// optimization problem. Returns false if the problem didn't contains only
// binary integer variable, or if the coefficients couldn't be converted to
// integer with a good enough precision.
bool ConvertBinaryMPModelProtoToBooleanProblem(const MPModelProto& mp_model,
                                               LinearBooleanProblem* problem);

// Converts a Boolean optimization problem to its lp formulation.
void ConvertBooleanProblemToLinearProgram(const LinearBooleanProblem& problem,
                                          glop::LinearProgram* lp);

// Changes the variable bounds of the lp to reflect the variables that have been
// fixed by the SAT solver (i.e. assigned at decision level 0). Returns the
// number of variables fixed this way.
int FixVariablesFromSat(const SatSolver& solver, glop::LinearProgram* lp);

// Solves the given lp problem and uses the lp solution to drive the SAT solver
// polarity choices. The variable must have the same index in the solved lp
// problem and in SAT for this to make sense.
//
// Returns false if a problem occurred while trying to solve the lp.
bool SolveLpAndUseSolutionForSatAssignmentPreference(
    const glop::LinearProgram& lp, SatSolver* sat_solver,
    double max_time_in_seconds);

// Solves the lp and add constraints to fix the integer variable of the lp in
// the LinearBoolean problem.
bool SolveLpAndUseIntegerVariableToStartLNS(const glop::LinearProgram& lp,
                                            LinearBooleanProblem* problem);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_LP_UTILS_H_
