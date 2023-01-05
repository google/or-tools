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

// Utility functions to interact with an lp solver from the SAT context.

#ifndef OR_TOOLS_SAT_LP_UTILS_H_
#define OR_TOOLS_SAT_LP_UTILS_H_

#include <stdint.h>

#include <utility>
#include <vector>

#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/sat/boolean_problem.pb.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/logging.h"

namespace operations_research {
namespace sat {

// Returns the smallest factor f such that f * abs(x) is integer modulo the
// given tolerance relative to f (we use f * tolerance). It is only looking
// for f smaller than the given limit. Returns zero if no such factor exist
// below the limit.
//
// The complexity is a lot less than O(limit), but it is possible that we might
// miss the smallest such factor if the tolerance used is too low. This is
// because we only rely on the best rational approximations of x with increasing
// denominator.
int64_t FindRationalFactor(double x, int64_t limit = 1e4,
                           double tolerance = 1e-6);

// Given a linear expression Sum_i c_i * X_i with each X_i in [lb_i, ub_i],
// this returns a scaling factor f such that
// 1/ the rounded expression cannot overflow given the domains of the X_i:
//    Sum |std::round(f * c_i) * X_i| <= max_absolute_activity
// 2/ the error is bounded:
//    | Sum_i (std::round(f * c_i) - f * c_i) |
//         < f * wanted_absolute_activity_precision
//
// This also fills the exact errors made by using the returned scaling factor.
// The heuristics try to minimize the magnitude of the scaled expression while
// satisfying the requested precision.
//
// Returns 0.0 if no scaling factor can be found under the condition 1/. Note
// that we try really hard to satisfy 2/ but we still return our best shot even
// when 2/ is not satisfied. One can check this by comparing the returned
// scaled_sum_error / f with wanted_absolute_activity_precision.
//
// TODO(user): unit test this and move to fp_utils.
// TODO(user): Ideally the lower/upper should be int64_t so that we can have
// an exact definition for the max_absolute_activity allowed.
double FindBestScalingAndComputeErrors(
    const std::vector<double>& coefficients,
    const std::vector<double>& lower_bounds,
    const std::vector<double>& upper_bounds, int64_t max_absolute_activity,
    double wanted_absolute_activity_precision, double* relative_coeff_error,
    double* scaled_sum_error);

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

// This simple step helps and should be done first. Returns false if the model
// is trivially infeasible because of crossing bounds.
bool MakeBoundsOfIntegerVariablesInteger(const SatParameters& params,
                                         MPModelProto* mp_model,
                                         SolverLogger* logger);

// Performs some extra tests on the given MPModelProto and returns false if one
// is not satisfied. These are needed before trying to convert it to the native
// CP-SAT format.
bool MPModelProtoValidationBeforeConversion(const SatParameters& params,
                                            const MPModelProto& mp_model,
                                            SolverLogger* logger);

// To satisfy our scaling requirements, any terms that is almost zero can just
// be set to zero. We need to do that before operations like
// DetectImpliedIntegers(), becauses really low coefficients can cause issues
// and might lead to less detection.
void RemoveNearZeroTerms(const SatParameters& params, MPModelProto* mp_model,
                         SolverLogger* logger);

// This will mark implied integer as such. Note that it can also discover
// variable of the form coeff * Integer + offset, and will change the model
// so that these are marked as integer. It is why we return both a scaling and
// an offset to transform the solution back to its original domain.
//
// TODO(user): Actually implement the offset part. This currently only happens
// on the 3 neos-46470* miplib problems where we have a non-integer rhs.
std::vector<double> DetectImpliedIntegers(MPModelProto* mp_model,
                                          SolverLogger* logger);

// Converts a MIP problem to a CpModel. Returns false if the coefficients
// couldn't be converted to integers with a good enough precision.
//
// There is a bunch of caveats and you can find more details on the
// SatParameters proto documentation for the mip_* parameters.
bool ConvertMPModelProtoToCpModelProto(const SatParameters& params,
                                       const MPModelProto& mp_model,
                                       CpModelProto* cp_model,
                                       SolverLogger* logger);

// Converts a CP-SAT model to a MPModelProto one.
// This only works for pure linear model (otherwise it returns false). This is
// mainly useful for debugging or using CP-SAT presolve and then trying other
// MIP solvers.
//
// TODO(user): This first version do not even handle basic Boolean constraint.
// Support more constraints as needed.
bool ConvertCpModelProtoToMPModelProto(const CpModelProto& input,
                                       MPModelProto* output);

// Scales a double objective to its integer version and fills it in the proto.
// The variable listed in the objective must be already defined in the cp_model
// proto as this uses the variables bounds to compute a proper scaling.
//
// This uses params.mip_wanted_tolerance() and
// params.mip_max_activity_exponent() to compute the scaling. Note however that
// if the wanted tolerance is not satisfied this still scale with best effort.
// You can see in the log the tolerance guaranteed by this automatic scaling.
//
// This will almost always returns true except for really bad cases like having
// infinity in the objective.
bool ScaleAndSetObjective(const SatParameters& params,
                          const std::vector<std::pair<int, double>>& objective,
                          double objective_offset, bool maximize,
                          CpModelProto* cp_model, SolverLogger* logger);

// Given a CpModelProto with a floating point objective, and its scaled integer
// version with a known lower bound, this uses the variable bounds to derive a
// correct lower bound on the original objective.
//
// Note that the integer version can be way different, but then the bound is
// likely to be bad. For now, we solve this with a simple LP with one
// constraint.
//
// TODO(user): Code a custom algo with more precision guarantee?
double ComputeTrueObjectiveLowerBound(
    const CpModelProto& model_proto_with_floating_point_objective,
    const CpObjectiveProto& integer_objective,
    const int64_t inner_integer_objective_lower_bound);

// Converts an integer program with only binary variables to a Boolean
// optimization problem. Returns false if the problem didn't contains only
// binary integer variable, or if the coefficients couldn't be converted to
// integer with a good enough precision.
bool ConvertBinaryMPModelProtoToBooleanProblem(const MPModelProto& mp_model,
                                               LinearBooleanProblem* problem);

// Converts a Boolean optimization problem to its lp formulation.
void ConvertBooleanProblemToLinearProgram(const LinearBooleanProblem& problem,
                                          glop::LinearProgram* lp);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_LP_UTILS_H_
