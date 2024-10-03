// Copyright 2010-2024 Google LLC
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

#ifndef OR_TOOLS_ROUTING_PARAMETERS_UTILS_H_
#define OR_TOOLS_ROUTING_PARAMETERS_UTILS_H_

#include <vector>

#include "absl/types/span.h"
#include "ortools/routing/parameters.pb.h"

namespace operations_research::routing {

// Takes RoutingSearchParameters::local_cheapest_insertion_sorting_properties in
// input and returns the ordered list of properties that is used to sort nodes
// when performing a local cheapest insertion first heuristic.
std::vector<RoutingSearchParameters::InsertionSortingProperty>
GetLocalCheapestInsertionSortingProperties(
    absl::Span<const int> lci_insertion_sorting_properties);

}  // namespace operations_research::routing

#endif  // OR_TOOLS_ROUTING_PARAMETERS_UTILS_H_
