// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_LP_SCHEDULING_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_LP_SCHEDULING_H_

#include "ortools/constraint_solver/routing.h"
#include "ortools/glop/lp_solver.h"

namespace operations_research {

// Classes to solve dimension cumul placement (aka scheduling) problems using
// linear programming.

// Utility class used in RouteDimensionCumulOptimizer to set the LP constraints
// and solve the problem.
class DimensionCumulOptimizerCore {
 public:
  explicit DimensionCumulOptimizerCore(const RoutingDimension* dimension)
      : dimension_(dimension) {}

  bool OptimizeSingleRoute(int vehicle,
                           const std::function<int64(int64)>& next_accessor,
                           glop::LinearProgram* linear_program,
                           glop::LPSolver* lp_solver,
                           std::vector<int64>* cumul_values, int64* cost,
                           int64* transit_cost);

  const RoutingDimension* dimension() const { return dimension_; }

 private:
  void SetRouteCumulConstraints(
      int vehicle, const std::function<int64(int64)>& next_accessor,
      glop::LinearProgram* linear_program, int64* route_transit_cost);

  bool FinalizeAndSolve(glop::LinearProgram* linear_program,
                        glop::LPSolver* lp_solver,
                        std::vector<int64>* cumul_values, int64* cost);
  const RoutingDimension* const dimension_;
  std::vector<glop::ColIndex> current_route_cumul_variables_;
};

// Class used to compute optimal values for dimension cumuls of routes,
// minimizing cumul soft lower and upper bound costs, and vehicle span costs of
// a route.
// In its methods, next_accessor is a callback returning the next node of a
// given node on a route.
class RouteDimensionCumulOptimizer {
 public:
  explicit RouteDimensionCumulOptimizer(const RoutingDimension* dimension);
  // If feasible, computes the optimal cost of the route performed by a vehicle,
  // minimizing cumul soft lower and upper bound costs and vehicle span costs,
  // and stores it in "optimal_cost" (if not null).
  // Returns true iff the route respects all constraints.
  bool ComputeRouteCumulCost(int vehicle,
                             const std::function<int64(int64)>& next_accessor,
                             int64* optimal_cost);
  // Same as ComputeRouteCumulCost, but the cost computed does not contain
  // the part of the vehicle span cost due to fixed transits.
  bool ComputeRouteCumulCostWithoutFixedTransits(
      int vehicle, const std::function<int64(int64)>& next_accessor,
      int64* optimal_cost_without_transits);
  // If feasible, computes the optimal cumul values of the route performed by a
  // vehicle, minimizing cumul soft lower and upper bound costs and vehicle span
  // costs, stores them in "optimal_cumuls" (if not null), and returns true.
  // Returns false if the route is not feasible.
  bool ComputeRouteCumuls(int vehicle,
                          const std::function<int64(int64)>& next_accessor,
                          std::vector<int64>* optimal_cumuls);

  const RoutingDimension* dimension() const {
    return optimizer_core_.dimension();
  }

 private:
  std::vector<std::unique_ptr<glop::LPSolver>> lp_solver_;
  std::vector<std::unique_ptr<glop::LinearProgram>> linear_program_;
  DimensionCumulOptimizerCore optimizer_core_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_LP_SCHEDULING_H_
