// Copyright 2010-2014 Google
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

namespace Google.OrTools.ConstraintSolver {
using System;
using System.Collections.Generic;

// Add a reference to callbacks on the routing model such that they
// are not garbaged out.
public partial class RoutingModel {
  public bool AddDimension(NodeEvaluator2 evaluator, long slack_max,
                           long capacity, bool fix_start_cumul_to_zero,
                           string name) {
    pinned_node_evaluator2_.Add(evaluator);
    return AddDimensionAux(evaluator, slack_max, capacity,
                           fix_start_cumul_to_zero, name);
  }

  public bool AddDimensionWithVehicleCapacity(
      NodeEvaluator2 evaluator, long slack_max,
      long[] vehicle_capacity, bool fix_start_cumul_to_zero,
      string name) {
    pinned_node_evaluator2_.Add(evaluator);
    return AddDimensionWithVehicleCapacityAux(
        evaluator, slack_max, vehicle_capacity, fix_start_cumul_to_zero, name);
  }

  public void SetArcCostEvaluatorOfAllVehicles(NodeEvaluator2 evaluator) {
    pinned_node_evaluator2_.Add(evaluator);
    SetArcCostEvaluatorOfAllVehiclesAux(evaluator);
  }

  public void SetArcCostEvaluatorOfVehicle(NodeEvaluator2 evaluator,
                                           int vehicle) {
    pinned_node_evaluator2_.Add(evaluator);
    SetArcCostEvaluatorOfVehicleAux(evaluator, vehicle);
  }

  private System.Collections.Generic.List<NodeEvaluator2>
      pinned_node_evaluator2_ =
          new System.Collections.Generic.List<NodeEvaluator2>();
}

}  // namespace Google.OrTools.ConstraintSolver
