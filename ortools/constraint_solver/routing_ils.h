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

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_ILS_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_ILS_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>

#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_parameters.pb.h"
#include "ortools/util/bitset.h"

namespace operations_research {

// Ruin interface.
class RuinProcedure {
 public:
  virtual ~RuinProcedure() = default;
  virtual std::function<int64_t(int64_t)> Ruin(
      const Assignment* assignment) = 0;
};

// Remove a number of routes that are spatially close together.
class CloseRoutesRemovalRuinProcedure : public RuinProcedure {
 public:
  CloseRoutesRemovalRuinProcedure(RoutingModel* model, size_t num_routes);
  // Returns next accessors where at most num_routes routes have been shortcut,
  // i.e., next(shortcut route begin) = shortcut route end.
  // Next accessors for customers belonging to shortcut routes are still set to
  // their original value and should not be used.
  std::function<int64_t(int64_t)> Ruin(const Assignment* assignment) override;

 private:
  // Returns whether the assignment as at least one performed node.
  bool HasPerformedNodes(const Assignment* assignment);

  const RoutingModel& model_;
  const RoutingModel::NodeNeighborsByCostClass* const neighbors_manager_;
  const size_t num_routes_;
  std::mt19937 rnd_;
  std::uniform_int_distribution<int64_t> customer_dist_;
  SparseBitset<int64_t> removed_routes_;
};

// Returns a DecisionBuilder implementing a perturbation step of an Iterated
// Local Search approach.
DecisionBuilder* MakePerturbationDecisionBuilder(
    const RoutingSearchParameters& parameters, RoutingModel* model,
    const Assignment* assignment, std::function<bool()> stop_search,
    LocalSearchFilterManager* filter_manager);

// Neighbor acceptance criterion interface.
class NeighborAcceptanceCriterion {
 public:
  virtual ~NeighborAcceptanceCriterion() = default;
  // Returns whether `candidate` should replace `reference`.
  virtual bool Accept(const Assignment* candidate,
                      const Assignment* reference) const = 0;
};

// Returns a neighbor acceptance criterion based on the given parameters.
std::unique_ptr<NeighborAcceptanceCriterion> MakeNeighborAcceptanceCriterion(
    const RoutingSearchParameters& parameters);

}  // namespace operations_research

#endif  // OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_ILS_H_
