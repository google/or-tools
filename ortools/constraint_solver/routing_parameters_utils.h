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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_ROUTING_PARAMETERS_UTILS_H_
#define ORTOOLS_CONSTRAINT_SOLVER_ROUTING_PARAMETERS_UTILS_H_

#include <vector>

#include "absl/types/span.h"
#include "ortools/constraint_solver/routing_heuristic_parameters.pb.h"
#include "ortools/constraint_solver/routing_parameters.pb.h"

namespace operations_research {

// Takes LocalCheapestInsertionParameters::insertion_sorting_properties
// in input and returns the ordered list of properties that is used to sort
// nodes when performing a local cheapest insertion first heuristic.
std::vector<LocalCheapestInsertionParameters::InsertionSortingProperty>
GetLocalCheapestInsertionSortingProperties(
    absl::Span<const int> lci_insertion_sorting_properties);

void DisableAllLocalSearchOperators(
    RoutingSearchParameters::LocalSearchNeighborhoodOperators* operators);

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_ROUTING_PARAMETERS_UTILS_H_
