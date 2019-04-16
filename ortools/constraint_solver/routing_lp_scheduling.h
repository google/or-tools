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

// Utility class used in Local/GlobalDimensionCumulOptimizer to set the LP
// constraints and solve the problem.
class DimensionCumulOptimizerCore {
 public:
  explicit DimensionCumulOptimizerCore(const RoutingDimension* dimension)
      : dimension_(dimension),
        visited_pickup_index_for_pair_(
            dimension->model()->GetPickupAndDeliveryPairs().size(), -1) {}

  // In the OptimizeSingleRoute() and Optimize() methods, if both "cumul_values"
  // and "cost" parameters are null, we don't optimize the cost and stop at the
  // first feasible solution in the LP (since in this case only feasibility is
  // of interest).
  bool OptimizeSingleRoute(int vehicle,
                           const std::function<int64(int64)>& next_accessor,
                           glop::LinearProgram* linear_program,
                           glop::LPSolver* lp_solver,
                           std::vector<int64>* cumul_values, int64* cost,
                           int64* transit_cost);

  bool Optimize(const std::function<int64(int64)>& next_accessor,
                glop::LinearProgram* linear_program, glop::LPSolver* lp_solver,
                std::vector<int64>* cumul_values, int64* cost,
                int64* transit_cost);

  bool OptimizeAndPack(const std::function<int64(int64)>& next_accessor,
                       glop::LinearProgram* linear_program,
                       glop::LPSolver* lp_solver,
                       std::vector<int64>* cumul_values);

  const RoutingDimension* dimension() const { return dimension_; }

 private:
  // Initializes the containers and given linear program. Must be called prior
  // to setting any contraints and solving.
  void InitOptimizer(glop::LinearProgram* linear_program);

  // Sets the constraints for all nodes on "vehicle"'s route according to
  // "next_accessor". If optimize_costs is true, also sets the objective
  // coefficients for the LP.
  // Returns false if some infeasibility was detected, true otherwise.
  bool SetRouteCumulConstraints(
      int vehicle, const std::function<int64(int64)>& next_accessor,
      int64 cumul_offset, bool optimize_costs,
      glop::LinearProgram* linear_program, int64* route_transit_cost,
      int64* route_cost_offset);

  // Sets the global constraints on the dimension, and adds global objective
  // cost coefficients if optimize_costs is true.
  // NOTE: When called, the call to this function MUST come after
  // SetRouteCumulConstraints() has been called on all routes, so that
  // index_to_cumul_variable_ and min_start/max_end_cumul_ are correctly
  // initialized.
  void SetGlobalConstraints(bool optimize_costs,
                            glop::LinearProgram* linear_program);

  bool FinalizeAndSolve(glop::LinearProgram* linear_program,
                        glop::LPSolver* lp_solver);

  void SetCumulValuesFromLP(const std::vector<glop::ColIndex>& cumul_variables,
                            int64 offset, const glop::LPSolver& lp_solver,
                            std::vector<int64>* cumul_values);

  const RoutingDimension* const dimension_;
  std::vector<glop::ColIndex> current_route_cumul_variables_;
  std::vector<glop::ColIndex> index_to_cumul_variable_;
  glop::ColIndex max_end_cumul_;
  glop::ColIndex min_start_cumul_;
  std::vector<int64> visited_pickup_index_for_pair_;
};

// Class used to compute optimal values for dimension cumuls of routes,
// minimizing cumul soft lower and upper bound costs, and vehicle span costs of
// a route.
// In its methods, next_accessor is a callback returning the next node of a
// given node on a route.
class LocalDimensionCumulOptimizer {
 public:
  explicit LocalDimensionCumulOptimizer(const RoutingDimension* dimension);
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

class GlobalDimensionCumulOptimizer {
 public:
  explicit GlobalDimensionCumulOptimizer(const RoutingDimension* dimension);
  // If feasible, computes the optimal cost of the entire model with regards to
  // the optimizer_core_'s dimension costs, minimizing cumul soft lower/upper
  // bound costs and vehicle/global span costs, and stores it in "optimal_cost"
  // (if not null).
  // Returns true iff all the constraints can be respected.
  bool ComputeCumulCostWithoutFixedTransits(
      const std::function<int64(int64)>& next_accessor,
      int64* optimal_cost_without_transits);
  // If feasible, computes the optimal cumul values, minimizing cumul soft
  // lower/upper bound costs and vehicle/global span costs, stores them in
  // "optimal_cumuls" (if not null), and returns true.
  // Returns false if the routes are not feasible.
  bool ComputeCumuls(const std::function<int64(int64)>& next_accessor,
                     std::vector<int64>* optimal_cumuls);

  // Returns true iff the routes resulting from the next_accessor are feasible
  // wrt the constraints on the optimizer_core_.dimension()'s cumuls.
  bool IsFeasible(const std::function<int64(int64)>& next_accessor);

  // Similar to ComputeCumuls, but also tries to pack the cumul values on all
  // routes, such that the cost remains the same, the cumuls of route ends are
  // minimized, and then the cumuls of the starts of the routes are maximized.
  bool ComputePackedCumuls(const std::function<int64(int64)>& next_accessor,
                           std::vector<int64>* packed_cumuls);

  const RoutingDimension* dimension() const {
    return optimizer_core_.dimension();
  }

 private:
  glop::LPSolver lp_solver_;
  glop::LinearProgram linear_program_;
  DimensionCumulOptimizerCore optimizer_core_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_LP_SCHEDULING_H_
