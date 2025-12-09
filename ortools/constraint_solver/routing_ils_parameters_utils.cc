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

#include "ortools/constraint_solver/routing_ils_parameters_utils.h"

#include <string>
#include <vector>

#include "ortools/constraint_solver/routing_enums.pb.h"
#include "ortools/constraint_solver/routing_ils.pb.h"

namespace operations_research {

RecreateParameters::ParametersCase GetParameterCaseForRecreateHeuristic(
    FirstSolutionStrategy::Value recreate_heuristic) {
  switch (recreate_heuristic) {
    case FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION:
      return RecreateParameters::kLocalCheapestInsertion;
    case FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION:
      return RecreateParameters::kLocalCheapestInsertion;
    case FirstSolutionStrategy::SAVINGS:
      return RecreateParameters::kSavings;
    case FirstSolutionStrategy::PARALLEL_SAVINGS:
      return RecreateParameters::kSavings;
    case FirstSolutionStrategy::SEQUENTIAL_CHEAPEST_INSERTION:
      return RecreateParameters::kGlobalCheapestInsertion;
    case FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION:
      return RecreateParameters::kGlobalCheapestInsertion;
    default:
      return RecreateParameters::PARAMETERS_NOT_SET;
  }
}

std::vector<RecreateParameters::ParametersCase>
GetSupportedRecreateParametersCases() {
  return {RecreateParameters::kLocalCheapestInsertion,
          RecreateParameters::kSavings,
          RecreateParameters::kGlobalCheapestInsertion};
}

std::string GetRecreateParametersName(
    RecreateParameters::ParametersCase parameters_case) {
  switch (parameters_case) {
    case RecreateParameters::kLocalCheapestInsertion:
      return "local_cheapest_insertion";
    case RecreateParameters::kSavings:
      return "savings";
    case RecreateParameters::kGlobalCheapestInsertion:
      return "global_cheapest_insertion";
    case RecreateParameters::PARAMETERS_NOT_SET:
      return "PARAMETERS_NOT_SET";
  }
}

}  // namespace operations_research
