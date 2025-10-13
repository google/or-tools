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

#ifndef OR_TOOLS_SAT_CP_MODEL_SOLVER_H_
#define OR_TOOLS_SAT_CP_MODEL_SOLVER_H_

#include <functional>
#include <string>

#include "absl/flags/declare.h"
#include "absl/strings/string_view.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"

ABSL_DECLARE_FLAG(bool, cp_model_dump_response);
ABSL_DECLARE_FLAG(bool, cp_model_drat_check);

namespace operations_research {
namespace sat {

/// Returns a string that describes the version of the solver.
std::string CpSatSolverVersion();

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
 * This advanced API accept a Model* which allows to access more advanced
 * features by configuring some classes in the Model before solve.
 *
 * For instance:
 * - StopSearch(&model);
 * - model->Add(NewSatParameters(parameters_as_string_or_proto));
 * - model->Add(NewFeasibleSolutionObserver(observer));
 * - model->Add(NewFeasibleSolutionLogCallback(callback));
 * - model->Add(NewBestBoundCallback(callback));
 */
CpSolverResponse SolveCpModel(const CpModelProto& model_proto, Model* model);

#if !defined(__PORTABLE_PLATFORM__)
/**
 * Solves the given CpModelProto with the given sat parameters as string in JSon
 * format, and returns an instance of CpSolverResponse.
 */
CpSolverResponse SolveWithParameters(const CpModelProto& model_proto,
                                     absl::string_view params);
#endif  // !__PORTABLE_PLATFORM__

/**
 * Creates a solution observer with the model with
 *   model.Add(NewFeasibleSolutionObserver([](response){...}));
 *
 * The given function will be called on each improving feasible solution found
 * during the search. For a non-optimization problem, if the option to find all
 * solution was set, then this will be called on each new solution.
 *
 * WARNING: Except when enumerate_all_solution() is true, one shouldn't rely on
 * this to get a set of "diverse" solutions since any future change to the
 * solver might completely kill any diversity in the set of solutions observed.
 *
 * Valid usage of this includes implementing features like:
 *  - Enumerating all solution via enumerate_all_solution(). If only n solutions
 *    are needed, this can also be used to abort when this number is reached.
 *  - Aborting early if a good enough solution is found.
 *  - Displaying log progress.
 *  - etc...
 */
std::function<void(Model*)> NewFeasibleSolutionObserver(
    const std::function<void(const CpSolverResponse& response)>& callback);

/**
 * Creates a callbacks that will append a string to the search log when
 * reporting a new solution.
 *
 * The given function will be called on each improving feasible solution found
 * during the search. For a non-optimization problem, if the option to find all
 * solution was set, then this will be called on each new solution.
 */
std::function<void(Model*)> NewFeasibleSolutionLogCallback(
    const std::function<std::string(const CpSolverResponse& response)>&
        callback);

/**
 * Creates a callbacks that will be called on each new best objective bound
 * found.
 *
 * Note that this function is called before the update takes place.
 */
std::function<void(Model*)> NewBestBoundCallback(
    const std::function<void(double)>& callback);

/**
 * Creates parameters for the solver, which you can add to the model with
 * \code
    model->Add(NewSatParameters(parameters_as_string_or_proto))
   \endcode
 * before calling \c SolveCpModel().
 */
std::function<SatParameters(Model*)> NewSatParameters(absl::string_view params);
std::function<SatParameters(Model*)> NewSatParameters(
    const SatParameters& parameters);

/// Stops the current search.
void StopSearch(Model* model);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_SOLVER_H_
