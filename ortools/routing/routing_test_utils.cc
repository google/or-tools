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

#include "ortools/routing/routing_test_utils.h"

#include <cstdint>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/routing/index_manager.h"
#include "ortools/routing/routing.h"

namespace operations_research::routing {

Assignment* BuildAssignmentFromSolution(const Model& model,
                                        absl::Span<const int> solution) {
  DCHECK_EQ(solution.size(), model.Nexts().size());
  Assignment* const assignment = model.solver()->MakeAssignment();
  for (int i = 0; i < solution.size(); ++i) {
    assignment->Add(model.NextVar(i))->SetValue(solution[i]);
  }
  return assignment;
}

int64_t GetCostFromMatrix(const IndexManager& manager,
                          absl::Span<const std::vector<int64_t>> costs,
                          int64_t from, int64_t to) {
  return costs[manager.IndexToNode(from).value()]
              [manager.IndexToNode(to).value()];
}

int64_t GetUnaryFixedDemand(Model* model, int64_t demand, int64_t from) {
  if (!model->IsEnd(from) && !model->IsStart(from)) {
    return demand;
  } else {
    return 0;
  }
}

int64_t GetFixedDemand(Model* model, int64_t demand, int64_t from,
                       int64_t /*to*/) {
  return GetUnaryFixedDemand(model, demand, from);
}

int64_t GetVehicleCapacity(int big_vehicle, int vehicle) {
  static constexpr int64_t kLargeCapacity = kint64max;
  static constexpr int64_t kSmallCapacity = 10;
  return vehicle == big_vehicle ? kLargeCapacity : kSmallCapacity;
}

std::vector<int64_t> GetVehicleCapacities(int num_vehicles, int big_vehicle) {
  std::vector<int64_t> capacities(num_vehicles, 0);
  for (int i = 0; i < num_vehicles; ++i) {
    capacities[i] = GetVehicleCapacity(big_vehicle, i);
  }
  return capacities;
}

void SetConsiderEmptyRouteCostsForAllVehicles(Model* model,
                                              bool consider_vehicle_costs) {
  for (int v = 0; v < model->vehicles(); ++v) {
    model->SetVehicleUsedWhenEmpty(consider_vehicle_costs, v);
  }
}

}  // namespace operations_research::routing
