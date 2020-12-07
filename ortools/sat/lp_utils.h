// Copyright 2010-2018 Google LLC
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
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace sat {

// Returns the smallest factor f such that f * abs(x) is integer modulo the
// given tolerance relative to f (we use f * tolerance). It is only looking
// for f smaller than the given limit. Returns zero if no such factor exist.
//
// The complexity is a lot less than O(limit), but it is possible that we might
// miss the smallest such factor if the tolerance used is too low. This is
// because we only rely on the best rational approximations of x with increasing
// denominator.
int FindRationalFactor(double x, int limit = 1e4, double tolerance = 1e-6);

// Multiplies all continuous variable by the given scaling parameters and change
// the rest of the model accordingly. The returned vector contains the scaling
// of each variable (will always be 1.0 for integers) and can be used to recover
// a solution of the unscaled problem from one of the new scaled problems by
// dividing the variable values.
//
// We usually scale a continuous variable by scaling, but if its domain is going
// to have larger values than max_bound, then we scale to have the max domain
// magnitude equal to max_bound.
//
// Note that it is recommended to call DetectImpliedIntegers() before this
// function so that we do not scale variables that do not need to be scaled.
//
// TODO(user): Also scale the solution hint if any.
std::vector<double> ScaleContinuousVariables(double scaling, double max_bound,
                                             MPModelProto* mp_model);

// To satisfy our scaling requirements, any terms that is almost zero can just
// be set to zero. We need to do that before operations like
// DetectImpliedIntegers(), becauses really low coefficients can cause issues
// and might lead to less detection.
void RemoveNearZeroTerms(const SatParameters& params, MPModelProto* mp_model);

// This will mark implied integer as such. Note that it can also discover
// variable of the form coeff * Integer + offset, and will change the model
// so that these are marked as integer. It is why we return both a scaling and
// an offset to transform the solution back to its original domain.
//
// TODO(user): Actually implement the offset part. This currently only happens
// on the 3 neos-46470* miplib problems where we have a non-integer rhs.
std::vector<double> DetectImpliedIntegers(bool log_info,
                                          MPModelProto* mp_model);

// Converts a MIP problem to a CpModel. Returns false if the coefficients
// couldn't be converted to integers with a good enough precision.
//
// There is a bunch of caveats and you can find more details on the
// SatParameters proto documentation for the mip_* parameters.
bool ConvertMPModelProtoToCpModelProto(const SatParameters& params,
                                       const MPModelProto& mp_model,
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
