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

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_DECISION_BUILDERS_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_DECISION_BUILDERS_H_

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_lp_scheduling.h"

namespace operations_research {

/// A decision builder which tries to assign values to variables as close as
/// possible to target values first.
DecisionBuilder* MakeSetValuesFromTargets(Solver* solver,
                                          std::vector<IntVar*> variables,
                                          std::vector<int64_t> targets);

/// Functions returning decision builders which try to instantiate dimension
/// cumul variables using scheduling optimizers.

/// Variant based on local optimizers, for which each route is handled
/// separately.
DecisionBuilder* MakeSetCumulsFromLocalDimensionCosts(
    Solver* solver, LocalDimensionCumulOptimizer* local_optimizer,
    LocalDimensionCumulOptimizer* local_mp_optimizer, SearchMonitor* monitor,
    bool optimize_and_pack = false,
    std::vector<RoutingModel::RouteDimensionTravelInfo>
        dimension_travel_info_per_route = {});

/// Variant based on global optimizers, handling all routes together.
DecisionBuilder* MakeSetCumulsFromGlobalDimensionCosts(
    Solver* solver, GlobalDimensionCumulOptimizer* global_optimizer,
    GlobalDimensionCumulOptimizer* global_mp_optimizer, SearchMonitor* monitor,
    bool optimize_and_pack = false,
    std::vector<RoutingModel::RouteDimensionTravelInfo>
        dimension_travel_info_per_route = {});

/// Variant taking into account resources.
DecisionBuilder* MakeSetCumulsFromResourceAssignmentCosts(
    Solver* solver, LocalDimensionCumulOptimizer* lp_optimizer,
    LocalDimensionCumulOptimizer* mp_optimizer, SearchMonitor* monitor);

/// A decision builder that monitors solutions, and tries to fix dimension
/// variables whose route did not change in the candidate solution.
/// Dimension variables are Cumul, Slack and break variables of all dimensions.
/// The user must make sure that those variables will be always be fixed at
/// solution, typically by composing another DecisionBuilder after this one.
/// If this DecisionBuilder returns a non-nullptr value at some node of the
/// search tree, it will always return nullptr in the subtree of that node.
/// Moreover, the decision will be a simultaneous assignment of the dimension
/// variables of unchanged routes on the left branch, and an empty decision on
/// the right branch.
DecisionBuilder* MakeRestoreDimensionValuesForUnchangedRoutes(
    RoutingModel* model);

/// A container that allows to accumulate variables and weights to generate a
/// static DecisionBuilder that uses weights to prioritize the branching
/// decisions (by decreasing weight).
class FinalizerVariables {
 public:
  explicit FinalizerVariables(Solver* solver) : solver_(solver) {}
  /// Adds a variable to minimize in the solution finalizer. The solution
  /// finalizer is called each time a solution is found during the search and
  /// allows to instantiate secondary variables (such as dimension cumul
  /// variables).
  void AddVariableToMinimize(IntVar* var);
  /// Adds a variable to maximize in the solution finalizer (see above for
  /// information on the solution finalizer).
  void AddVariableToMaximize(IntVar* var);
  /// Adds a variable to minimize in the solution finalizer, with a weighted
  /// priority: the higher the more priority it has.
  void AddWeightedVariableToMinimize(IntVar* var, int64_t cost);
  /// Adds a variable to maximize in the solution finalizer, with a weighted
  /// priority: the higher the more priority it has.
  void AddWeightedVariableToMaximize(IntVar* var, int64_t cost);
  /// Add a variable to set the closest possible to the target value in the
  /// solution finalizer.
  void AddVariableTarget(IntVar* var, int64_t target);
  /// Same as above with a weighted priority: the higher the cost, the more
  /// priority it has to be set close to the target value.
  void AddWeightedVariableTarget(IntVar* var, int64_t target, int64_t cost);
  ///
  DecisionBuilder* CreateFinalizer();

 private:
  Solver* const solver_;
#ifndef SWIG
  struct VarTarget {
    IntVar* var;
    int64_t target;
  };
  std::vector<std::pair<VarTarget, int64_t>>
      weighted_finalizer_variable_targets_;
  std::vector<VarTarget> finalizer_variable_targets_;
  absl::flat_hash_map<IntVar*, int> weighted_finalizer_variable_index_;
  absl::flat_hash_set<IntVar*> finalizer_variable_target_set_;
#endif
};

}  // namespace operations_research
#endif  // OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_DECISION_BUILDERS_H_
