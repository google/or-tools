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

// Returns a std::string with some statistics on the given CpModelProto.
std::string CpModelStats(const CpModelProto& model);

// Returns a std::string with some statistics on the solver response.
std::string CpSolverResponseStats(const CpSolverResponse& response);

// Solves the given CpModelProto.
//
// Note that the API takes a Model* that will be filled with the in-memory
// representation of the given CpModelProto. It is done this way so that it is
// easy to set custom parameters or interrupt the solver will calls like:
//  - model->Add(NewSatParameters(parameters_as_string_or_proto));
//  - model->GetOrCreate<TimeLimit>()->RegisterExternalBooleanAsLimit(&stop);
//  - model->GetOrCreate<SigintHandler>()->Register([&stop]() { stop = true; });
CpSolverResponse SolveCpModel(const CpModelProto& model_proto, Model* model);

// Allows to register a solution "observer" with the model with
//   model.Add(NewFeasibleSolutionObserver([](response){...}));
// The given function will be called on each "improving" feasible solution found
// during the search. For a non-optimization problem, if the option to find all
// solution was set, then this will be called on each new solution.
std::function<void(Model*)> NewFeasibleSolutionObserver(
    const std::function<void(const CpSolverResponse& response)>& observer);

// If set, the underlying solver will call this function "regularly" in a
// deterministic way. It will then wait until this function returns with the
// current "best" information about the current problem.
//
// This is meant to be used in a multi-threaded environment with many parallel
// solving process. If the returned current "best" response only uses
// informations derived at a lower deterministic time (possibly with offset)
// than the deterministic time of the current thread, then the whole process can
// be made deterministic.
void SetSynchronizationFunction(std::function<CpSolverResponse()> f,
                                Model* model);

// Allows to change the default parameters with
//   model->Add(NewSatParameters(parameters_as_string_or_proto))
// before calling SolveCpModel().
#if !defined(__PORTABLE_PLATFORM__)
std::function<SatParameters(Model*)> NewSatParameters(
    const std::string& params);
#endif  // !__PORTABLE_PLATFORM__
std::function<SatParameters(Model*)> NewSatParameters(
    const SatParameters& parameters);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_SOLVER_H_
