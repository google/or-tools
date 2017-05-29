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
//  - model->Add(NewSatParameters(parameters_as_string));
CpSolverResponse SolveCpModel(const CpModelProto& model_proto, Model* model);

// Same as above, but do not run the CpModelProto presolver. This is exposed for
// internal usage, all client should use the version above.
//
// TODO(user): add a parameters to enable/disable the presolve.
CpSolverResponse SolveCpModelWithoutPresolve(const CpModelProto& model_proto,
                                             Model* model);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_SOLVER_H_
