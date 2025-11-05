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

#ifndef ORTOOLS_ROUTING_PARAMETERS_H_
#define ORTOOLS_ROUTING_PARAMETERS_H_

#include <string>
#include <vector>

#include "ortools/routing/parameters.pb.h"

namespace operations_research::routing {

RoutingModelParameters DefaultRoutingModelParameters();
RoutingSearchParameters DefaultRoutingSearchParameters();
RoutingSearchParameters DefaultSecondaryRoutingSearchParameters();

/// Returns an empty std::string if the routing search parameters are valid, and
/// a non-empty, human readable error description if they're not.
std::string FindErrorInRoutingSearchParameters(
    const RoutingSearchParameters& search_parameters);

/// Returns a list of std::string describing the errors in the routing search
/// parameters. Returns an empty vector if the parameters are valid.
std::vector<std::string> FindErrorsInRoutingSearchParameters(
    const RoutingSearchParameters& search_parameters);

}  // namespace operations_research::routing

#endif  // ORTOOLS_ROUTING_PARAMETERS_H_
