// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_CONSTRAINTS_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_CONSTRAINTS_H_

#include <cstdint>
#include <vector>

#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/routing.h"

namespace operations_research {

Constraint* MakeDifferentFromValues(Solver* solver, IntVar* var,
                                    std::vector<int64_t> values);

Constraint* MakeResourceConstraint(
    const RoutingModel::ResourceGroup* resource_group,
    const std::vector<IntVar*>* vehicle_resource_vars, RoutingModel* model);

/// For every vehicle of the routing model:
/// - if total_slacks[vehicle] is not nullptr, constrains it to be the sum of
///   slacks on that vehicle, that is,
///   dimension->CumulVar(end) - dimension->CumulVar(start) -
///   sum_{node in path of vehicle} dimension->FixedTransitVar(node).
/// - if spans[vehicle] is not nullptr, constrains it to be
///   dimension->CumulVar(end) - dimension->CumulVar(start)
/// This does stronger propagation than a decomposition, and takes breaks into
/// account.
Constraint* MakePathSpansAndTotalSlacks(const RoutingDimension* dimension,
                                        std::vector<IntVar*> spans,
                                        std::vector<IntVar*> total_slacks);

}  // namespace operations_research

#endif  // OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_CONSTRAINTS_H_
