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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_ROUTING_ILS_PARAMETERS_UTILS_H_
#define ORTOOLS_CONSTRAINT_SOLVER_ROUTING_ILS_PARAMETERS_UTILS_H_

#include <string>
#include <vector>

#include "ortools/constraint_solver/routing_enums.pb.h"
#include "ortools/constraint_solver/routing_ils.pb.h"

namespace operations_research {

// Returns the appropriate parameters case for the given recreate heuristic.
// Returns PARAMETERS_NOT_SET if the heuristic is not supported.
RecreateParameters::ParametersCase GetParameterCaseForRecreateHeuristic(
    FirstSolutionStrategy::Value recreate_heuristic);

// Returns the list of supported recreate parameters cases.
std::vector<RecreateParameters::ParametersCase>
GetSupportedRecreateParametersCases();

// Returns the name of the given recreate parameter.
std::string GetRecreateParametersName(
    RecreateParameters::ParametersCase parameters_case);

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_ROUTING_ILS_PARAMETERS_UTILS_H_
