// Copyright 2010-2014 Google
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

#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"

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
// easy to set custom parameters or time limit on the model with calls like:
//  - model->SetSingleton<TimeLimit>(std::move(time_limit));
//  - model->Add(NewSatParameters(parameters_as_string_or_proto));
CpSolverResponse SolveCpModel(const CpModelProto& model_proto, Model* model);

// Allows to register a solution "observer" with the model with
//   model.Add(NewFeasibleSolutionObserver([](values){...}));
// The given function will be called on each feasible solution found during the
// search. The values will be in one to one correspondence with the variables
// in the model_proto.
//
// TODO(user): Change the API to take the full CpSolverResponse() so we have
// solve statistics and the current objective value.
std::function<void(Model*)> NewFeasibleSolutionObserver(
    const std::function<void(const std::vector<int64>& values)>& observer);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_SOLVER_H_
