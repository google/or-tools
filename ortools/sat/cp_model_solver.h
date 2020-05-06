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

#ifndef OR_TOOLS_SAT_CP_MODEL_SOLVER_H_
#define OR_TOOLS_SAT_CP_MODEL_SOLVER_H_

#include <functional>
#include <string>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {

/// Solves the given CpModelProto and returns an instance of CpSolverResponse.
CpSolverResponse Solve(const CpModelProto& model_proto);

/// Solves the given CpModelProto with the given parameters.
CpSolverResponse SolveWithParameters(const CpModelProto& model_proto,
                                     const SatParameters& params);

/// Returns a string with some statistics on the given CpModelProto.
std::string CpModelStats(const CpModelProto& model);

/** Returns a string with some statistics on the solver response.
 *
 * If the second argument is false, we will just display NA for the objective
 * value instead of zero. It is not really needed but it makes things a bit
 * clearer to see that there is no objective.
 */
std::string CpSolverResponseStats(const CpSolverResponse& response,
                                  bool has_objective = true);

/**
 * Solves the given CpModelProto.
 *
 * This advanced API accept a Model* which allows to access more adavanced
 * features by configuring some classes in the Model before solve.
 *
 * For instance:
 * - model->Add(NewSatParameters(parameters_as_string_or_proto));
 * - model->GetOrCreate<TimeLimit>()->RegisterExternalBooleanAsLimit(&stop);
 * - model->Add(NewFeasibleSolutionObserver(observer));
 */
CpSolverResponse SolveCpModel(const CpModelProto& model_proto, Model* model);

#if !defined(__PORTABLE_PLATFORM__)
/**
 * Solves the given CpModelProto with the given sat parameters as string in JSon
 * format, and returns an instance of CpSolverResponse.
 */
CpSolverResponse SolveWithParameters(const CpModelProto& model_proto,
                                     const std::string& params);
#endif  // !__PORTABLE_PLATFORM__

/**
 * Creates a solution observer with the model with
 *   model.Add(NewFeasibleSolutionObserver([](response){...}));
 *
 * The given function will be called on each improving feasible solution found
 * during the search. For a non-optimization problem, if the option to find all
 * solution was set, then this will be called on each new solution.
 */
std::function<void(Model*)> NewFeasibleSolutionObserver(
    const std::function<void(const CpSolverResponse& response)>& observer);

/**
 * If set, the underlying solver will call this function regularly in a
 * deterministic way. It will then wait until this function returns with the
 * current best information about the current problem.
 *
 * This is meant to be used in a multi-threaded environment with many parallel
 * solving process. If the returned current "best" response only uses
 * information derived at a lower deterministic time (possibly with offset)
 * than the deterministic time of the current thread, the whole process can
 * be made deterministic.
 */
void SetSynchronizationFunction(std::function<CpSolverResponse()> f,
                                Model* model);

/**
 * Creates parameters for the solver, which you can add to the model with
 * \code
    model->Add(NewSatParameters(parameters_as_string_or_proto))
   \endcode
 * before calling \c SolveCpModel().
 */
#if !defined(__PORTABLE_PLATFORM__)
std::function<SatParameters(Model*)> NewSatParameters(
    const std::string& params);
#endif  // !__PORTABLE_PLATFORM__
std::function<SatParameters(Model*)> NewSatParameters(
    const SatParameters& parameters);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_SOLVER_H_
