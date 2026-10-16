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

#ifndef ORTOOLS_ROUTING_ROUTING_TEST_UTILS_H_
#define ORTOOLS_ROUTING_ROUTING_TEST_UTILS_H_

#include <cstdint>
#include <vector>

#include "absl/types/span.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/routing/index_manager.h"
#include "ortools/routing/routing.h"

namespace operations_research::routing {

// Builds an assignment from the given solution, such that
// NextVar(i) == solution[i] for all i.
Assignment* BuildAssignmentFromSolution(const Model& model,
                                        absl::Span<const int> solution);

int64_t GetCostFromMatrix(const IndexManager& manager,
                          absl::Span<const std::vector<int64_t>> costs,
                          int64_t from, int64_t to);

// Returns 'demand' for any non-start/end 'from' node and 0 for start/end nodes.
int64_t GetUnaryFixedDemand(Model* model, int64_t demand, int64_t from);

// Same as above, ignoring the 'to' argument.
int64_t GetFixedDemand(Model* model, int64_t demand, int64_t from, int64_t to);

// Returns kint64max if vehicle == big_vehicle and 10 otherwise.
int64_t GetVehicleCapacity(int big_vehicle, int vehicle);

std::vector<int64_t> GetVehicleCapacities(int num_vehicles, int big_vehicle);

void SetConsiderEmptyRouteCostsForAllVehicles(Model* model,
                                              bool consider_vehicle_costs);

}  // namespace operations_research::routing

#endif  // ORTOOLS_ROUTING_ROUTING_TEST_UTILS_H_
