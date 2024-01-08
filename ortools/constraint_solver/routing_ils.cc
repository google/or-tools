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

#include "ortools/constraint_solver/routing_ils.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/routing_search.h"
#include "ortools/constraint_solver/routing_types.h"

namespace operations_research {

CloseRoutesRemovalRuinProcedure::CloseRoutesRemovalRuinProcedure(
    RoutingModel* model, size_t num_routes)
    : model_(*model),
      neighbors_manager_(model->GetOrCreateNodeNeighborsByCostClass(
          /*TODO(user): use a parameter*/ 100,
          /*add_vehicle_starts_to_neighbors=*/false)),
      num_routes_(num_routes),
      rnd_(/*fixed seed=*/0),
      customer_dist_(0, model->Size() - 1),
      removed_routes_(model->vehicles()) {}

std::function<int64_t(int64_t)> CloseRoutesRemovalRuinProcedure::Ruin(
    const Assignment* assignment) {
  removed_routes_.SparseClearAll();

  if (num_routes_ > 0 && HasPerformedNodes(assignment)) {
    int64_t seed_node;
    int seed_route = -1;
    do {
      seed_node = customer_dist_(rnd_);
      seed_route = assignment->Value(model_.VehicleVar(seed_node));
    } while (model_.IsStart(seed_node) || seed_route == -1);
    DCHECK(!model_.IsEnd(seed_node));

    const RoutingCostClassIndex cost_class_index =
        model_.GetCostClassIndexOfVehicle(seed_route);

    const std::vector<int>& neighbors =
        neighbors_manager_->GetNeighborsOfNodeForCostClass(
            cost_class_index.value(), seed_node);

    for (int neighbor : neighbors) {
      const int64_t route = assignment->Value(model_.VehicleVar(neighbor));
      if (route < 0 || removed_routes_[route]) {
        continue;
      }
      removed_routes_.Set(route);
      if (removed_routes_.NumberOfSetCallsWithDifferentArguments() ==
          num_routes_) {
        break;
      }
    }
  }

  return [this, assignment](int64_t node) {
    // Shortcut removed routes to remove associated customers.
    if (model_.IsStart(node)) {
      const int route = assignment->Value(model_.VehicleVar(node));
      if (removed_routes_[route]) {
        return model_.End(route);
      }
    }
    return assignment->Value(model_.NextVar(node));
  };
}

bool CloseRoutesRemovalRuinProcedure::HasPerformedNodes(
    const Assignment* assignment) {
  for (int v = 0; v < model_.vehicles(); ++v) {
    if (model_.Next(*assignment, model_.Start(v)) != model_.End(v)) {
      return true;
    }
  }
  return false;
}

class RuinAndRecreateDecisionBuilder : public DecisionBuilder {
 public:
  RuinAndRecreateDecisionBuilder(
      const Assignment* assignment, std::unique_ptr<RuinProcedure> ruin,
      std::unique_ptr<RoutingFilteredHeuristic> recreate)
      : assignment_(*assignment),
        ruin_(std::move(ruin)),
        recreate_(std::move(recreate)) {}

  Decision* Next(Solver* const solver) override {
    Assignment* const new_assignment = Recreate(ruin_->Ruin(&assignment_));
    if (new_assignment) {
      new_assignment->Restore();
    } else {
      solver->Fail();
    }
    return nullptr;
  }

 private:
  Assignment* Recreate(const std::function<int64_t(int64_t)>& next_accessor) {
    return recreate_->BuildSolutionFromRoutes(next_accessor);
  }

  const Assignment& assignment_;
  std::unique_ptr<RuinProcedure> ruin_;
  std::unique_ptr<RoutingFilteredHeuristic> recreate_;
};

DecisionBuilder* MakeRuinAndRecreateDecisionBuilder(
    RoutingModel* model, const Assignment* assignment,
    std::unique_ptr<RuinProcedure> ruin,
    std::unique_ptr<RoutingFilteredHeuristic> recreate) {
  return model->solver()->RevAlloc(new RuinAndRecreateDecisionBuilder(
      assignment, std::move(ruin), std::move(recreate)));
}

}  // namespace operations_research
