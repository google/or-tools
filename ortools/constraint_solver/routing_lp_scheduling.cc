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

#include "ortools/constraint_solver/routing_lp_scheduling.h"

#include <numeric>

#include "absl/container/flat_hash_set.h"
#include "absl/time/time.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/map_util.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/glop/lp_solver.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {

namespace {

// The following sets of parameters give the fastest response time without
// impacting solutions found negatively.
glop::GlopParameters GetGlopParametersForLocalLP() {
  glop::GlopParameters parameters;
  parameters.set_use_dual_simplex(true);
  parameters.set_use_preprocessing(false);
  return parameters;
}

glop::GlopParameters GetGlopParametersForGlobalLP() {
  glop::GlopParameters parameters;
  parameters.set_use_dual_simplex(true);
  return parameters;
}

bool SetVariableBounds(glop::LinearProgram* linear_program,
                       const glop::ColIndex index, int64 min, int64 max) {
  // When variable upper bounds are greater than this threshold, precision
  // issues arise in GLOP. In this case we are just going to suppose that these
  // high bound values are infinite and not set the upper bound.
  const int64 kMaxValue = 1e10;
  const double lp_min = min;
  const double lp_max = (max > kMaxValue) ? glop::kInfinity : max;
  if (lp_min <= lp_max) {
    linear_program->SetVariableBounds(index, lp_min, lp_max);
    return true;
  }
  // The linear_program would not be feasible, and it cannot handle the
  // lp_min > lp_max case, so we must detect infeasibility here.
  return false;
}

bool GetCumulBoundsWithOffset(const IntVar& cumul_var, int64 cumul_offset,
                              int64* lower_bound, int64* upper_bound) {
  DCHECK(lower_bound != nullptr);
  DCHECK(upper_bound != nullptr);
  *lower_bound = std::max<int64>(0, CapSub(cumul_var.Min(), cumul_offset));
  *upper_bound = cumul_var.Max();
  if (*upper_bound < kint64max) {
    *upper_bound = CapSub(*upper_bound, cumul_offset);
    if (*upper_bound < *lower_bound) {
      // The cumul's upper bound is less than its lower bound.
      // NOTE: As the lower bound is greater than 0 (using std::max), this
      // includes cases where the upper bound is less than the offset.
      return false;
    }
  }
  return true;
}

// Finds the pickup/delivery pairs of nodes on a given vehicle's route.
// Returns the vector of visited pair indices, and stores the corresponding
// pickup/delivery indices in visited_pickup_delivery_indices_for_pair_.
// NOTE: Supposes that visited_pickup_delivery_indices_for_pair is correctly
// sized and initialized to {-1, -1} for all pairs.
void StoreVisitedPickupDeliveryPairsOnRoute(
    const RoutingDimension& dimension, int vehicle,
    const std::function<int64(int64)>& next_accessor,
    std::vector<int>* visited_pairs,
    std::vector<std::pair<int64, int64>>*
        visited_pickup_delivery_indices_for_pair) {
  // visited_pickup_delivery_indices_for_pair must be all {-1, -1}.
  DCHECK_EQ(visited_pickup_delivery_indices_for_pair->size(),
            dimension.model()->GetPickupAndDeliveryPairs().size());
  DCHECK(std::all_of(visited_pickup_delivery_indices_for_pair->begin(),
                     visited_pickup_delivery_indices_for_pair->end(),
                     [](std::pair<int64, int64> p) {
                       return p.first == -1 && p.second == -1;
                     }));
  visited_pairs->clear();
  if (!dimension.HasPickupToDeliveryLimits()) {
    return;
  }
  const RoutingModel& model = *dimension.model();

  int64 node_index = model.Start(vehicle);
  while (!model.IsEnd(node_index)) {
    const std::vector<std::pair<int, int>>& pickup_index_pairs =
        model.GetPickupIndexPairs(node_index);
    const std::vector<std::pair<int, int>>& delivery_index_pairs =
        model.GetDeliveryIndexPairs(node_index);
    if (!pickup_index_pairs.empty()) {
      // The current node is a pickup. We verify that it belongs to a single
      // pickup index pair and that it's not a delivery, and store the index.
      DCHECK(delivery_index_pairs.empty());
      DCHECK_EQ(pickup_index_pairs.size(), 1);
      (*visited_pickup_delivery_indices_for_pair)[pickup_index_pairs[0].first]
          .first = node_index;
      visited_pairs->push_back(pickup_index_pairs[0].first);
    } else if (!delivery_index_pairs.empty()) {
      // The node is a delivery. We verify that it belongs to a single
      // delivery pair, and set the limit with its pickup if one has been
      // visited for this pair.
      DCHECK_EQ(delivery_index_pairs.size(), 1);
      const int pair_index = delivery_index_pairs[0].first;
      std::pair<int64, int64>& pickup_delivery_index =
          (*visited_pickup_delivery_indices_for_pair)[pair_index];
      if (pickup_delivery_index.first < 0) {
        // This case should not happen, as a delivery must have its pickup
        // on the route, but we ignore it here.
        node_index = next_accessor(node_index);
        continue;
      }
      pickup_delivery_index.second = node_index;
    }
    node_index = next_accessor(node_index);
  }
}

}  // namespace

LocalDimensionCumulOptimizer::LocalDimensionCumulOptimizer(
    const RoutingDimension* dimension)
    : optimizer_core_(dimension, /*use_precedence_propagator=*/false) {
  // Using one solver and linear program per vehicle in the hope that if
  // routes don't change this will be faster.
  const int vehicles = dimension->model()->vehicles();
  lp_solver_.resize(vehicles);
  linear_program_.resize(vehicles);
  const glop::GlopParameters parameters = GetGlopParametersForLocalLP();
  for (int vehicle = 0; vehicle < vehicles; ++vehicle) {
    lp_solver_[vehicle] = absl::make_unique<glop::LPSolver>();
    lp_solver_[vehicle]->SetParameters(parameters);
    linear_program_[vehicle] = absl::make_unique<glop::LinearProgram>();
  }
}

bool LocalDimensionCumulOptimizer::ComputeRouteCumulCost(
    int vehicle, const std::function<int64(int64)>& next_accessor,
    int64* optimal_cost) {
  return optimizer_core_.OptimizeSingleRoute(
      vehicle, next_accessor, linear_program_[vehicle].get(),
      lp_solver_[vehicle].get(), nullptr, optimal_cost, nullptr);
}

bool LocalDimensionCumulOptimizer::ComputeRouteCumulCostWithoutFixedTransits(
    int vehicle, const std::function<int64(int64)>& next_accessor,
    int64* optimal_cost_without_transits) {
  int64 cost = 0;
  int64 transit_cost = 0;
  if (optimizer_core_.OptimizeSingleRoute(
          vehicle, next_accessor, linear_program_[vehicle].get(),
          lp_solver_[vehicle].get(), nullptr, &cost, &transit_cost)) {
    if (optimal_cost_without_transits != nullptr) {
      *optimal_cost_without_transits = CapSub(cost, transit_cost);
    }
    return true;
  }
  return false;
}

bool LocalDimensionCumulOptimizer::ComputeRouteCumuls(
    int vehicle, const std::function<int64(int64)>& next_accessor,
    std::vector<int64>* optimal_cumuls) {
  return optimizer_core_.OptimizeSingleRoute(
      vehicle, next_accessor, linear_program_[vehicle].get(),
      lp_solver_[vehicle].get(), optimal_cumuls, nullptr, nullptr);
}

bool LocalDimensionCumulOptimizer::ComputePackedRouteCumuls(
    int vehicle, const std::function<int64(int64)>& next_accessor,
    std::vector<int64>* packed_cumuls) {
  return optimizer_core_.OptimizeAndPackSingleRoute(
      vehicle, next_accessor, linear_program_[vehicle].get(),
      lp_solver_[vehicle].get(), packed_cumuls);
}

const int CumulBoundsPropagator::kNoParent = -2;
const int CumulBoundsPropagator::kParentToBePropagated = -1;

CumulBoundsPropagator::CumulBoundsPropagator(const RoutingDimension* dimension)
    : dimension_(*dimension), num_nodes_(2 * dimension->cumuls().size()) {
  outgoing_arcs_.resize(num_nodes_);
  node_in_queue_.resize(num_nodes_, false);
  tree_parent_node_of_.resize(num_nodes_, kNoParent);
  propagated_bounds_.resize(num_nodes_);
  visited_pickup_delivery_indices_for_pair_.resize(
      dimension->model()->GetPickupAndDeliveryPairs().size(), {-1, -1});
}

void CumulBoundsPropagator::AddArcs(int first_index, int second_index,
                                    int64 offset) {
  // Add arc first_index + offset <= second_index
  outgoing_arcs_[PositiveNode(first_index)].push_back(
      {PositiveNode(second_index), offset});
  AddNodeToQueue(PositiveNode(first_index));
  // Add arc -second_index + transit <= -first_index
  outgoing_arcs_[NegativeNode(second_index)].push_back(
      {NegativeNode(first_index), offset});
  AddNodeToQueue(NegativeNode(second_index));
}

bool CumulBoundsPropagator::InitializeArcsAndBounds(
    const std::function<int64(int64)>& next_accessor, int64 cumul_offset) {
  propagated_bounds_.assign(num_nodes_, kint64min);

  for (std::vector<ArcInfo>& arcs : outgoing_arcs_) {
    arcs.clear();
  }

  RoutingModel* const model = dimension_.model();
  std::vector<int64>& lower_bounds = propagated_bounds_;

  for (int vehicle = 0; vehicle < model->vehicles(); vehicle++) {
    const std::function<int64(int64, int64)>& transit_accessor =
        dimension_.transit_evaluator(vehicle);

    int node = model->Start(vehicle);
    while (true) {
      int64 cumul_lb, cumul_ub;
      if (!GetCumulBoundsWithOffset(*dimension_.CumulVar(node), cumul_offset,
                                    &cumul_lb, &cumul_ub)) {
        return false;
      }
      lower_bounds[PositiveNode(node)] = cumul_lb;
      if (cumul_ub < kint64max) {
        lower_bounds[NegativeNode(node)] = -cumul_ub;
      }

      if (model->IsEnd(node)) {
        break;
      }

      const int next = next_accessor(node);
      const int64 transit = transit_accessor(node, next);
      const IntVar& slack_var = *dimension_.SlackVar(node);
      // node + transit + slack_var == next
      // Add arcs for node + transit + slack_min <= next
      AddArcs(node, next, CapAdd(transit, slack_var.Min()));
      if (slack_var.Max() < kint64max) {
        // Add arcs for node + transit + slack_max >= next.
        AddArcs(next, node, CapSub(-slack_var.Max(), transit));
      }

      node = next;
    }

    // Add vehicle span upper bound: end - span_ub <= start.
    const int64 span_ub = dimension_.GetSpanUpperBoundForVehicle(vehicle);
    if (span_ub < kint64max) {
      AddArcs(model->End(vehicle), model->Start(vehicle), -span_ub);
    }

    // Set pickup/delivery limits on route.
    std::vector<int> visited_pairs;
    StoreVisitedPickupDeliveryPairsOnRoute(
        dimension_, vehicle, next_accessor, &visited_pairs,
        &visited_pickup_delivery_indices_for_pair_);
    for (int pair_index : visited_pairs) {
      const int64 pickup_index =
          visited_pickup_delivery_indices_for_pair_[pair_index].first;
      const int64 delivery_index =
          visited_pickup_delivery_indices_for_pair_[pair_index].second;
      visited_pickup_delivery_indices_for_pair_[pair_index] = {-1, -1};

      DCHECK_GE(pickup_index, 0);
      if (delivery_index < 0) {
        // We didn't encounter a delivery for this pickup.
        continue;
      }

      const int64 limit = dimension_.GetPickupToDeliveryLimitForPair(
          pair_index, model->GetPickupIndexPairs(pickup_index)[0].second,
          model->GetDeliveryIndexPairs(delivery_index)[0].second);
      if (limit < kint64max) {
        // delivery_cumul - limit  <= pickup_cumul.
        AddArcs(delivery_index, pickup_index, -limit);
      }
    }
  }

  for (const RoutingDimension::NodePrecedence& precedence :
       dimension_.GetNodePrecedences()) {
    const int first_index = precedence.first_node;
    const int second_index = precedence.second_node;
    if (lower_bounds[PositiveNode(first_index)] == kint64min ||
        lower_bounds[PositiveNode(second_index)] == kint64min) {
      // One of the nodes is unperformed, so the precedence rule doesn't apply.
      continue;
    }
    AddArcs(first_index, second_index, precedence.offset);
  }

  return true;
}

bool CumulBoundsPropagator::UpdateCurrentLowerBoundOfNode(int node,
                                                          int64 new_lb) {
  propagated_bounds_[node] = new_lb;

  // Test that the lower/upper bounds do not cross each other.
  const int cumul_var_index = node / 2;
  const int64 cumul_lower_bound =
      propagated_bounds_[PositiveNode(cumul_var_index)];

  const int64 negated_cumul_upper_bound =
      propagated_bounds_[NegativeNode(cumul_var_index)];

  return CapAdd(negated_cumul_upper_bound, cumul_lower_bound) <= 0;
}

bool CumulBoundsPropagator::DisassembleSubtree(int source, int target) {
  tmp_dfs_stack_.clear();
  tmp_dfs_stack_.push_back(source);
  while (!tmp_dfs_stack_.empty()) {
    const int tail = tmp_dfs_stack_.back();
    tmp_dfs_stack_.pop_back();
    for (const ArcInfo& arc : outgoing_arcs_[tail]) {
      const int child_node = arc.head;
      if (tree_parent_node_of_[child_node] != tail) continue;
      if (child_node == target) return false;
      tree_parent_node_of_[child_node] = kParentToBePropagated;
      tmp_dfs_stack_.push_back(child_node);
    }
  }
  return true;
}

bool CumulBoundsPropagator::PropagateCumulBounds(
    const std::function<int64(int64)>& next_accessor, int64 cumul_offset) {
  tree_parent_node_of_.assign(num_nodes_, kNoParent);
  DCHECK(std::none_of(node_in_queue_.begin(), node_in_queue_.end(),
                      [](bool b) { return b; }));
  DCHECK(bf_queue_.empty());

  if (!InitializeArcsAndBounds(next_accessor, cumul_offset)) {
    return CleanupAndReturnFalse();
  }

  std::vector<int64>& current_lb = propagated_bounds_;

  // Bellman-Ford-Tarjan algorithm.
  while (!bf_queue_.empty()) {
    const int node = bf_queue_.front();
    bf_queue_.pop_front();
    node_in_queue_[node] = false;

    if (tree_parent_node_of_[node] == kParentToBePropagated) {
      // The parent of this node is still in the queue, so no need to process
      // node now, since it will be re-enqued when its parent is processed.
      continue;
    }

    const int64 lower_bound = current_lb[node];
    for (const ArcInfo& arc : outgoing_arcs_[node]) {
      // NOTE: kint64min as a lower bound means no lower bound at all, so we
      // don't use this value to propagate.
      const int64 induced_lb = (lower_bound == kint64min)
                                   ? kint64min
                                   : CapAdd(lower_bound, arc.offset);

      const int head_node = arc.head;
      if (induced_lb <= current_lb[head_node]) {
        // No update necessary for the head_node, continue to next children of
        // node.
        continue;
      }
      if (!UpdateCurrentLowerBoundOfNode(head_node, induced_lb) ||
          !DisassembleSubtree(head_node, node)) {
        // The new lower bound is infeasible, or a positive cycle was detected
        // in the precedence graph by DisassembleSubtree().
        return CleanupAndReturnFalse();
      }

      tree_parent_node_of_[head_node] = node;
      AddNodeToQueue(head_node);
    }
  }
  return true;
}

bool DimensionCumulOptimizerCore::OptimizeSingleRoute(
    int vehicle, const std::function<int64(int64)>& next_accessor,
    glop::LinearProgram* linear_program, glop::LPSolver* lp_solver,
    std::vector<int64>* cumul_values, int64* cost, int64* transit_cost,
    bool clear_lp) {
  InitOptimizer(linear_program);
  // Make sure SetRouteCumulConstraints will properly set the cumul bounds by
  // looking at this route only.
  DCHECK(propagator_ == nullptr);

  const RoutingModel* const model = dimension()->model();
  const bool optimize_vehicle_costs =
      (cumul_values != nullptr || cost != nullptr) &&
      (!model->IsEnd(next_accessor(model->Start(vehicle))) ||
       model->AreEmptyRouteCostsConsideredForVehicle(vehicle));
  const int64 cumul_offset =
      dimension_->GetLocalOptimizerOffsetForVehicle(vehicle);
  int64 cost_offset = 0;
  if (!SetRouteCumulConstraints(vehicle, next_accessor, cumul_offset,
                                optimize_vehicle_costs, linear_program,
                                transit_cost, &cost_offset) ||
      !FinalizeAndSolve(linear_program, lp_solver)) {
    return false;
  }

  SetCumulValuesFromLP(current_route_cumul_variables_, cumul_offset, *lp_solver,
                       cumul_values);
  if (cost != nullptr) {
    *cost = CapAdd(cost_offset, std::round(lp_solver->GetObjectiveValue()));
  }

  if (clear_lp) {
    linear_program->Clear();
  }
  return true;
}

bool DimensionCumulOptimizerCore::Optimize(
    const std::function<int64(int64)>& next_accessor,
    glop::LinearProgram* linear_program, glop::LPSolver* lp_solver,
    std::vector<int64>* cumul_values, int64* cost, int64* transit_cost,
    bool clear_lp) {
  InitOptimizer(linear_program);

  // If both "cumul_values" and "cost" parameters are null, we don't try to
  // optimize the cost and stop at the first feasible solution.
  const bool optimize_costs = (cumul_values != nullptr) || (cost != nullptr);
  bool has_vehicles_being_optimized = false;

  const int64 cumul_offset = dimension_->GetGlobalOptimizerOffset();

  if (propagator_ != nullptr &&
      !propagator_->PropagateCumulBounds(next_accessor, cumul_offset)) {
    return false;
  }

  int64 total_transit_cost = 0;
  int64 total_cost_offset = 0;
  const RoutingModel* model = dimension()->model();
  for (int vehicle = 0; vehicle < model->vehicles(); vehicle++) {
    int64 route_transit_cost = 0;
    int64 route_cost_offset = 0;
    const bool optimize_vehicle_costs =
        optimize_costs &&
        (!model->IsEnd(next_accessor(model->Start(vehicle))) ||
         model->AreEmptyRouteCostsConsideredForVehicle(vehicle));
    if (!SetRouteCumulConstraints(vehicle, next_accessor, cumul_offset,
                                  optimize_vehicle_costs, linear_program,
                                  &route_transit_cost, &route_cost_offset)) {
      return false;
    }
    total_transit_cost = CapAdd(total_transit_cost, route_transit_cost);
    total_cost_offset = CapAdd(total_cost_offset, route_cost_offset);
    has_vehicles_being_optimized |= optimize_vehicle_costs;
  }
  if (transit_cost != nullptr) {
    *transit_cost = total_transit_cost;
  }

  SetGlobalConstraints(has_vehicles_being_optimized, linear_program);

  if (!FinalizeAndSolve(linear_program, lp_solver)) {
    return false;
  }

  SetCumulValuesFromLP(index_to_cumul_variable_, cumul_offset, *lp_solver,
                       cumul_values);

  if (cost != nullptr) {
    *cost =
        CapAdd(std::round(lp_solver->GetObjectiveValue()), total_cost_offset);
  }

  if (clear_lp) {
    linear_program->Clear();
  }
  return true;
}

bool DimensionCumulOptimizerCore::OptimizeAndPack(
    const std::function<int64(int64)>& next_accessor,
    glop::LinearProgram* linear_program, glop::LPSolver* lp_solver,
    std::vector<int64>* cumul_values) {
  // Note: We pass a non-nullptr cost to the Optimize() method so the costs are
  // optimized by the LP.
  int64 cost = 0;
  if (!Optimize(next_accessor, linear_program, lp_solver,
                /*cumul_values=*/nullptr, &cost, /*transit_cost=*/nullptr,
                /*clear_lp=*/false)) {
    return false;
  }

  std::vector<int> vehicles(dimension()->model()->vehicles());
  std::iota(vehicles.begin(), vehicles.end(), 0);
  if (!PackRoutes(std::move(vehicles), linear_program, lp_solver)) {
    return false;
  }

  SetCumulValuesFromLP(index_to_cumul_variable_,
                       dimension_->GetGlobalOptimizerOffset(), *lp_solver,
                       cumul_values);
  linear_program->Clear();
  return true;
}

bool DimensionCumulOptimizerCore::OptimizeAndPackSingleRoute(
    int vehicle, const std::function<int64(int64)>& next_accessor,
    glop::LinearProgram* linear_program, glop::LPSolver* lp_solver,
    std::vector<int64>* cumul_values) {
  // Note: We pass a non-nullptr cost to the OptimizeSingleRoute() method so the
  // costs are optimized by the LP.
  int64 cost = 0;
  if (!OptimizeSingleRoute(vehicle, next_accessor, linear_program, lp_solver,
                           /*cumul_values=*/nullptr, &cost,
                           /*transit_cost=*/nullptr,
                           /*clear_lp=*/false) ||
      !PackRoutes({vehicle}, linear_program, lp_solver)) {
    return false;
  }
  SetCumulValuesFromLP(current_route_cumul_variables_,
                       dimension_->GetLocalOptimizerOffsetForVehicle(vehicle),
                       *lp_solver, cumul_values);
  linear_program->Clear();
  return true;
}

bool DimensionCumulOptimizerCore::PackRoutes(
    std::vector<int> vehicles, glop::LinearProgram* linear_program,
    glop::LPSolver* lp_solver) {
  const RoutingModel* model = dimension_->model();

  // Minimize the route end times without increasing the cost.
  glop::RowIndex objective_ct = linear_program->CreateNewConstraint();
  linear_program->SetConstraintBounds(objective_ct, 0,
                                      lp_solver->GetObjectiveValue());

  const glop::DenseRow& objective_coefficients =
      linear_program->objective_coefficients();
  for (glop::ColIndex variable(0); variable < linear_program->num_variables();
       variable++) {
    const double coefficient = objective_coefficients[variable];
    if (coefficient != 0) {
      linear_program->SetCoefficient(objective_ct, variable, coefficient);
      linear_program->SetObjectiveCoefficient(variable, 0);
    }
  }
  for (int vehicle : vehicles) {
    linear_program->SetObjectiveCoefficient(
        index_to_cumul_variable_[model->End(vehicle)], 1);
  }

  if (!FinalizeAndSolve(linear_program, lp_solver)) {
    return false;
  }

  // Maximize the route start times without increasing the cost or the route end
  // times.
  for (int vehicle : vehicles) {
    const glop::ColIndex end_cumul_var =
        index_to_cumul_variable_[model->End(vehicle)];
    // end_cumul_var <= lp_solver.variable_values()[end_cumul_var]
    linear_program->SetVariableBounds(
        end_cumul_var, linear_program->variable_lower_bounds()[end_cumul_var],
        lp_solver->variable_values()[end_cumul_var]);
    linear_program->SetObjectiveCoefficient(end_cumul_var, 0);

    // Maximize the starts of the routes.
    linear_program->SetObjectiveCoefficient(
        index_to_cumul_variable_[model->Start(vehicle)], -1);
  }
  if (!FinalizeAndSolve(linear_program, lp_solver)) {
    return false;
  }

  return true;
}

void DimensionCumulOptimizerCore::InitOptimizer(
    glop::LinearProgram* linear_program) {
  linear_program->Clear();
  linear_program->SetMaximizationProblem(false);
  index_to_cumul_variable_.clear();
  index_to_cumul_variable_.resize(dimension_->cumuls().size(),
                                  glop::ColIndex(-1));
  max_end_cumul_ = linear_program->CreateNewVariable();
  min_start_cumul_ = linear_program->CreateNewVariable();
}

bool DimensionCumulOptimizerCore::ComputeRouteCumulBounds(
    const std::vector<int64>& route, const std::vector<int64>& fixed_transits,
    int64 cumul_offset) {
  const int route_size = route.size();
  current_route_min_cumuls_.resize(route_size);
  current_route_max_cumuls_.resize(route_size);
  if (propagator_ != nullptr) {
    for (int pos = 0; pos < route_size; pos++) {
      const int64 node = route[pos];
      current_route_min_cumuls_[pos] = propagator_->CumulMin(node);
      DCHECK_GE(current_route_min_cumuls_[pos], 0);
      current_route_max_cumuls_[pos] = propagator_->CumulMax(node);
      DCHECK_GE(current_route_max_cumuls_[pos], current_route_min_cumuls_[pos]);
    }
    return true;
  }

  // Extract cumul min/max and fixed transits from CP.
  for (int pos = 0; pos < route_size; ++pos) {
    if (!GetCumulBoundsWithOffset(*dimension_->CumulVar(route[pos]),
                                  cumul_offset, &current_route_min_cumuls_[pos],
                                  &current_route_max_cumuls_[pos])) {
      return false;
    }
  }

  // Refine cumul bounds using
  // cumul[i+1] >= cumul[i] + fixed_transit[i] + slack[i].
  for (int pos = 1; pos < route_size; ++pos) {
    const int64 slack_min = dimension_->SlackVar(route[pos - 1])->Min();
    current_route_min_cumuls_[pos] = std::max(
        current_route_min_cumuls_[pos],
        CapAdd(
            CapAdd(current_route_min_cumuls_[pos - 1], fixed_transits[pos - 1]),
            slack_min));
  }

  for (int pos = route_size - 2; pos >= 0; --pos) {
    // If cumul_max[pos+1] is kint64max, it will be translated to
    // double +infinity, so it must not constrain cumul_max[pos].
    if (current_route_max_cumuls_[pos + 1] < kint64max) {
      const int64 slack_min = dimension_->SlackVar(route[pos])->Min();
      current_route_max_cumuls_[pos] = std::min(
          current_route_max_cumuls_[pos],
          CapSub(
              CapSub(current_route_max_cumuls_[pos + 1], fixed_transits[pos]),
              slack_min));
    }
  }
  return true;
}

bool DimensionCumulOptimizerCore::SetRouteCumulConstraints(
    int vehicle, const std::function<int64(int64)>& next_accessor,
    int64 cumul_offset, bool optimize_costs,
    glop::LinearProgram* linear_program, int64* route_transit_cost,
    int64* route_cost_offset) {
  RoutingModel* const model = dimension_->model();
  // Extract the vehicle's path from next_accessor.
  std::vector<int64> path;
  {
    int node = model->Start(vehicle);
    path.push_back(node);
    while (!model->IsEnd(node)) {
      node = next_accessor(node);
      path.push_back(node);
    }
    DCHECK_GE(path.size(), 2);
  }
  const int path_size = path.size();

  std::vector<int64> fixed_transit(path_size - 1);
  {
    const std::function<int64(int64, int64)>& transit_accessor =
        dimension_->transit_evaluator(vehicle);
    for (int pos = 1; pos < path_size; ++pos) {
      fixed_transit[pos - 1] = transit_accessor(path[pos - 1], path[pos]);
    }
  }

  if (!ComputeRouteCumulBounds(path, fixed_transit, cumul_offset)) {
    return false;
  }

  // LP Model variables, current_route_cumul_variables_ and lp_slacks.
  // Create LP variables for cumuls.
  std::vector<glop::ColIndex>& lp_cumuls = current_route_cumul_variables_;
  lp_cumuls.assign(path_size, glop::kInvalidCol);
  for (int pos = 0; pos < path_size; ++pos) {
    const glop::ColIndex lp_cumul = linear_program->CreateNewVariable();
    index_to_cumul_variable_[path[pos]] = lp_cumul;
    lp_cumuls[pos] = lp_cumul;
    if (!SetVariableBounds(linear_program, lp_cumul,
                           current_route_min_cumuls_[pos],
                           current_route_max_cumuls_[pos])) {
      return false;
    }
  }
  // Create LP variables for slacks.
  std::vector<glop::ColIndex> lp_slacks(path_size - 1, glop::kInvalidCol);
  for (int pos = 0; pos < path_size - 1; ++pos) {
    const IntVar* cp_slack = dimension_->SlackVar(path[pos]);
    lp_slacks[pos] = linear_program->CreateNewVariable();
    if (!SetVariableBounds(linear_program, lp_slacks[pos], cp_slack->Min(),
                           cp_slack->Max())) {
      return false;
    }
  }

  // LP Model constraints and costs.
  // Add all path constraints to LP:
  // cumul[i] + fixed_transit[i] + slack[i] == cumul[i+1]
  // <=> fixed_transit[i] == cumul[i+1] - cumul[i] - slack[i].
  for (int pos = 0; pos < path_size - 1; ++pos) {
    const glop::RowIndex ct = linear_program->CreateNewConstraint();
    linear_program->SetConstraintBounds(ct, fixed_transit[pos],
                                        fixed_transit[pos]);
    linear_program->SetCoefficient(ct, lp_cumuls[pos + 1], 1);
    linear_program->SetCoefficient(ct, lp_cumuls[pos], -1);
    linear_program->SetCoefficient(ct, lp_slacks[pos], -1);
  }
  if (route_cost_offset != nullptr) *route_cost_offset = 0;
  if (optimize_costs) {
    // Add soft upper bounds.
    for (int pos = 0; pos < path_size; ++pos) {
      if (!dimension_->HasCumulVarSoftUpperBound(path[pos])) continue;
      const int64 coef =
          dimension_->GetCumulVarSoftUpperBoundCoefficient(path[pos]);
      if (coef == 0) continue;
      int64 bound = dimension_->GetCumulVarSoftUpperBound(path[pos]);
      if (bound < cumul_offset && route_cost_offset != nullptr) {
        // Add coef * (cumul_offset - bound) to the cost offset.
        *route_cost_offset = CapAdd(*route_cost_offset,
                                    CapProd(CapSub(cumul_offset, bound), coef));
      }
      bound = std::max<int64>(0, CapSub(bound, cumul_offset));
      if (current_route_max_cumuls_[pos] <= bound) {
        // constraint is never violated.
        continue;
      }
      const glop::ColIndex soft_ub_diff = linear_program->CreateNewVariable();
      linear_program->SetObjectiveCoefficient(soft_ub_diff, coef);
      // cumul - soft_ub_diff <= bound.
      const glop::RowIndex ct = linear_program->CreateNewConstraint();
      linear_program->SetConstraintBounds(ct, -glop::kInfinity, bound);
      linear_program->SetCoefficient(ct, lp_cumuls[pos], 1);
      linear_program->SetCoefficient(ct, soft_ub_diff, -1);
    }
    // Add soft lower bounds.
    for (int pos = 0; pos < path_size; ++pos) {
      if (!dimension_->HasCumulVarSoftLowerBound(path[pos])) continue;
      const int64 coef =
          dimension_->GetCumulVarSoftLowerBoundCoefficient(path[pos]);
      if (coef == 0) continue;
      const int64 bound = std::max<int64>(
          0, CapSub(dimension_->GetCumulVarSoftLowerBound(path[pos]),
                    cumul_offset));
      if (current_route_min_cumuls_[pos] >= bound) {
        // constraint is never violated.
        continue;
      }
      const glop::ColIndex soft_lb_diff = linear_program->CreateNewVariable();
      linear_program->SetObjectiveCoefficient(soft_lb_diff, coef);
      // bound - cumul <= soft_lb_diff
      const glop::RowIndex ct = linear_program->CreateNewConstraint();
      linear_program->SetConstraintBounds(ct, bound, glop::kInfinity);
      linear_program->SetCoefficient(ct, lp_cumuls[pos], 1);
      linear_program->SetCoefficient(ct, soft_lb_diff, 1);
    }
  }
  // Add pickup and delivery limits.
  std::vector<int> visited_pairs;
  StoreVisitedPickupDeliveryPairsOnRoute(
      *dimension_, vehicle, next_accessor, &visited_pairs,
      &visited_pickup_delivery_indices_for_pair_);
  for (int pair_index : visited_pairs) {
    const int64 pickup_index =
        visited_pickup_delivery_indices_for_pair_[pair_index].first;
    const int64 delivery_index =
        visited_pickup_delivery_indices_for_pair_[pair_index].second;
    visited_pickup_delivery_indices_for_pair_[pair_index] = {-1, -1};

    DCHECK_GE(pickup_index, 0);
    if (delivery_index < 0) {
      // We didn't encounter a delivery for this pickup.
      continue;
    }

    const int64 limit = dimension_->GetPickupToDeliveryLimitForPair(
        pair_index, model->GetPickupIndexPairs(pickup_index)[0].second,
        model->GetDeliveryIndexPairs(delivery_index)[0].second);
    if (limit < kint64max) {
      // delivery_cumul - pickup_cumul <= limit.
      glop::RowIndex ct = linear_program->CreateNewConstraint();
      linear_program->SetConstraintBounds(ct, -glop::kInfinity, limit);
      linear_program->SetCoefficient(
          ct, index_to_cumul_variable_[delivery_index], 1);
      linear_program->SetCoefficient(ct, index_to_cumul_variable_[pickup_index],
                                     -1);
    }
  }

  // Add span bound constraint.
  const int64 span_bound = dimension_->GetSpanUpperBoundForVehicle(vehicle);
  if (span_bound < kint64max) {
    // end_cumul - start_cumul <= bound
    glop::RowIndex ct = linear_program->CreateNewConstraint();
    linear_program->SetConstraintBounds(ct, -glop::kInfinity, span_bound);
    linear_program->SetCoefficient(ct, lp_cumuls.back(), 1);
    linear_program->SetCoefficient(ct, lp_cumuls.front(), -1);
  }
  // Add span cost.
  const int64 span_cost_coef =
      dimension_->GetSpanCostCoefficientForVehicle(vehicle);
  if (optimize_costs && span_cost_coef > 0) {
    linear_program->SetObjectiveCoefficient(lp_cumuls.back(), span_cost_coef);
    linear_program->SetObjectiveCoefficient(lp_cumuls.front(), -span_cost_coef);
  }
  // Add soft span cost.
  if (optimize_costs && dimension_->HasSoftSpanUpperBounds()) {
    SimpleBoundCosts::BoundCost bound_cost =
        dimension_->GetSoftSpanUpperBoundForVehicle(vehicle);
    if (bound_cost.bound < kint64max && bound_cost.cost > 0) {
      glop::ColIndex span_violation = linear_program->CreateNewVariable();
      linear_program->SetVariableBounds(span_violation, 0.0, glop::kInfinity);
      // end - start <= bound + span_violation
      glop::RowIndex violation = linear_program->CreateNewConstraint();
      linear_program->SetConstraintBounds(violation, -glop::kInfinity,
                                          bound_cost.bound);
      linear_program->SetCoefficient(violation, lp_cumuls.back(), 1.0);
      linear_program->SetCoefficient(violation, lp_cumuls.front(), -1.0);
      linear_program->SetCoefficient(violation, span_violation, -1.0);
      // Add span_violation * cost to objective.
      linear_program->SetObjectiveCoefficient(span_violation, bound_cost.cost);
    }
  }
  // Add global span constraint.
  if (optimize_costs && dimension_->global_span_cost_coefficient() > 0) {
    // min_start_cumul_ <= cumuls[start]
    glop::RowIndex ct = linear_program->CreateNewConstraint();
    linear_program->SetConstraintBounds(ct, -glop::kInfinity, 0);
    linear_program->SetCoefficient(ct, min_start_cumul_, 1);
    linear_program->SetCoefficient(ct, lp_cumuls.front(), -1);
    // max_end_cumul_ >= cumuls[end]
    ct = linear_program->CreateNewConstraint();
    linear_program->SetConstraintBounds(ct, 0, glop::kInfinity);
    linear_program->SetCoefficient(ct, max_end_cumul_, 1);
    linear_program->SetCoefficient(ct, lp_cumuls.back(), -1);
  }
  // Fill transit cost if specified.
  if (route_transit_cost != nullptr) {
    if (optimize_costs && span_cost_coef > 0) {
      const int64 total_fixed_transit = std::accumulate(
          fixed_transit.begin(), fixed_transit.end(), 0, CapAdd);
      *route_transit_cost = CapProd(total_fixed_transit, span_cost_coef);
    } else {
      *route_transit_cost = 0;
    }
  }
  // For every break that must be inside the route, the duration of that break
  // must be flowed in the slacks of arcs that can intersect the break.
  // This LP modelization is correct but not complete:
  // can miss some cases where the breaks cannot fit.
  const int num_breaks =
      dimension_->HasBreakConstraints()
          ? dimension_->GetBreakIntervalsOfVehicle(vehicle).size()
          : 0;
  if (num_breaks == 0) return true;
  std::vector<glop::RowIndex> break_constraints(num_breaks, glop::kInvalidRow);
  std::vector<glop::RowIndex> slack_constraints(path_size - 1,
                                                glop::kInvalidRow);
  const std::vector<IntervalVar*>& breaks =
      dimension_->GetBreakIntervalsOfVehicle(vehicle);
  // Gather visit information: the visit of node i has [start, end) =
  // [cumul[i] - post_travel[i-1], cumul[i] + pre_travel[i]).
  std::vector<int64> pre_travel(path_size - 1, 0);
  std::vector<int64> post_travel(path_size - 1, 0);
  {
    const int pre_travel_index =
        dimension_->GetPreTravelEvaluatorOfVehicle(vehicle);
    if (pre_travel_index != -1) {
      FillPathEvaluation(path, model->TransitCallback(pre_travel_index),
                         &pre_travel);
    }
    const int post_travel_index =
        dimension_->GetPostTravelEvaluatorOfVehicle(vehicle);
    if (post_travel_index != -1) {
      FillPathEvaluation(path, model->TransitCallback(post_travel_index),
                         &post_travel);
    }
  }
  const int64 vehicle_start_max = current_route_max_cumuls_.front();
  const int64 vehicle_end_min = current_route_min_cumuls_.back();
  for (int br = 0; br < num_breaks; ++br) {
    // Create a constraint for every break that must be in the path:
    // sum_i break_to_slack_i  == breaks[br].DurationMin().
    if (!breaks[br]->MustBePerformed()) continue;
    const int64 break_end_min = CapSub(breaks[br]->EndMin(), cumul_offset);
    if (break_end_min <= vehicle_start_max) continue;
    const int64 break_start_max = CapSub(breaks[br]->StartMax(), cumul_offset);
    if (vehicle_end_min <= break_start_max) continue;
    break_constraints[br] = linear_program->CreateNewConstraint();
    linear_program->SetConstraintBounds(break_constraints[br],
                                        breaks[br]->DurationMin(),
                                        breaks[br]->DurationMin());
    for (int pos = 0; pos < path_size - 1; ++pos) {
      // Pass on slacks that cannot start before, cannot end after,
      // or are not long enough to contain the break.
      const int64 slack_start_min =
          CapAdd(current_route_min_cumuls_[pos], pre_travel[pos]);
      if (slack_start_min > break_start_max) continue;
      const int64 slack_end_max =
          CapSub(current_route_max_cumuls_[pos + 1], post_travel[pos]);
      if (break_end_min > slack_end_max) continue;
      const int64 slack_duration_max =
          std::min(CapSub(CapSub(current_route_max_cumuls_[pos + 1],
                                 current_route_min_cumuls_[pos]),
                          fixed_transit[pos]),
                   dimension_->SlackVar(path[pos])->Max());
      if (slack_duration_max < breaks[br]->DurationMin()) continue;
      // Break can fit into slack: make LP variable, add to break and slack
      // constraints.
      glop::ColIndex break_to_slack = linear_program->CreateNewVariable();
      linear_program->SetVariableBounds(break_to_slack, 0,
                                        breaks[br]->DurationMin());
      linear_program->SetCoefficient(break_constraints[br], break_to_slack, 1);
      // Make a slack constraint (lazily), that will represent
      // sum_break break_to_slack_i <= lp_slacks[i].
      if (slack_constraints[pos] == glop::kInvalidRow) {
        slack_constraints[pos] = linear_program->CreateNewConstraint();
        linear_program->SetConstraintBounds(slack_constraints[pos],
                                            -glop::kInfinity, 0);
        linear_program->SetCoefficient(slack_constraints[pos], lp_slacks[pos],
                                       -1);
      }
      linear_program->SetCoefficient(slack_constraints[pos], break_to_slack, 1);
    }
  }
  return true;
}

void DimensionCumulOptimizerCore::SetGlobalConstraints(
    bool optimize_costs, glop::LinearProgram* linear_program) {
  // Global span cost =
  //     global_span_cost_coefficient * (max_end_cumul - min_start_cumul).
  const int64 global_span_coeff = dimension_->global_span_cost_coefficient();
  if (optimize_costs && global_span_coeff > 0) {
    linear_program->SetObjectiveCoefficient(max_end_cumul_, global_span_coeff);
    linear_program->SetObjectiveCoefficient(min_start_cumul_,
                                            -global_span_coeff);
  }

  // Node precedence constraints, set when both nodes are visited.
  for (const RoutingDimension::NodePrecedence& precedence :
       dimension_->GetNodePrecedences()) {
    const glop::ColIndex first_cumul_var =
        index_to_cumul_variable_[precedence.first_node];
    const glop::ColIndex second_cumul_var =
        index_to_cumul_variable_[precedence.second_node];
    if (first_cumul_var < 0 || second_cumul_var < 0) {
      // At least one of the nodes is not on any route, skip this precedence
      // constraint.
      continue;
    }
    DCHECK_NE(first_cumul_var, second_cumul_var)
        << "Dimension " << dimension_->name()
        << " has a self-precedence on node " << precedence.first_node << ".";

    // cumul[second_node] - cumul[first_node] >= offset.
    const glop::RowIndex ct = linear_program->CreateNewConstraint();
    linear_program->SetConstraintBounds(ct, precedence.offset, glop::kInfinity);
    linear_program->SetCoefficient(ct, second_cumul_var, 1);
    linear_program->SetCoefficient(ct, first_cumul_var, -1);
  }
}

bool DimensionCumulOptimizerCore::FinalizeAndSolve(
    glop::LinearProgram* linear_program, glop::LPSolver* lp_solver) {
  // Set the time limit of the LP solver based on the model's remaining time.
  const absl::Duration duration_limit = dimension()->model()->RemainingTime();
  lp_solver->GetMutableParameters()->set_max_time_in_seconds(
      absl::ToDoubleSeconds(duration_limit));

  // Because we construct the lp one constraint at a time and we never call
  // SetCoefficient() on the same variable twice for a constraint, we know that
  // the columns do not contain duplicates and are already ordered by constraint
  // so we do not need to call linear_program->CleanUp() which can be costly.
  // Note that the assumptions are DCHECKed() in the call below.
  linear_program->NotifyThatColumnsAreClean();
  VLOG(2) << linear_program->Dump();
  const glop::ProblemStatus status = lp_solver->Solve(*linear_program);
  if (status != glop::ProblemStatus::OPTIMAL &&
      status != glop::ProblemStatus::IMPRECISE) {
    linear_program->Clear();
    return false;
  }
  return true;
}

void DimensionCumulOptimizerCore::SetCumulValuesFromLP(
    const std::vector<glop::ColIndex>& cumul_variables, int64 offset,
    const glop::LPSolver& lp_solver, std::vector<int64>* cumul_values) {
  if (cumul_values == nullptr) return;
  cumul_values->clear();
  cumul_values->resize(cumul_variables.size());

  for (int index = 0; index < cumul_variables.size(); index++) {
    const glop::ColIndex cumul_var = cumul_variables[index];
    if (cumul_var < 0) {
      // Node indices that do not appear on any route (i.e. unperformed nodes)
      // have a cumul_var of -1 when SetCumulValuesFromLP() is called with
      // cumul_variables == index_to_cumul_variable_.
      (*cumul_values)[index] = dimension_->CumulVar(index)->Min();
    } else {
      const double lp_value_double = lp_solver.variable_values()[cumul_var];
      const int64 lp_value_int64 =
          (lp_value_double >= kint64max)
              ? kint64max
              : static_cast<int64>(std::round(lp_value_double));
      (*cumul_values)[index] = CapAdd(lp_value_int64, offset);
    }
  }
}

GlobalDimensionCumulOptimizer::GlobalDimensionCumulOptimizer(
    const RoutingDimension* dimension)
    : optimizer_core_(dimension,
                      /*use_precedence_propagator=*/
                      !dimension->GetNodePrecedences().empty()) {
  lp_solver_.SetParameters(GetGlopParametersForGlobalLP());
}

bool GlobalDimensionCumulOptimizer::ComputeCumulCostWithoutFixedTransits(
    const std::function<int64(int64)>& next_accessor,
    int64* optimal_cost_without_transits) {
  int64 cost = 0;
  int64 transit_cost = 0;
  if (optimizer_core_.Optimize(next_accessor, &linear_program_, &lp_solver_,
                               nullptr, &cost, &transit_cost)) {
    if (optimal_cost_without_transits != nullptr) {
      *optimal_cost_without_transits = CapSub(cost, transit_cost);
    }
    return true;
  }
  return false;
}

bool GlobalDimensionCumulOptimizer::ComputeCumuls(
    const std::function<int64(int64)>& next_accessor,
    std::vector<int64>* optimal_cumuls) {
  return optimizer_core_.Optimize(next_accessor, &linear_program_, &lp_solver_,
                                  optimal_cumuls, nullptr, nullptr);
}

bool GlobalDimensionCumulOptimizer::IsFeasible(
    const std::function<int64(int64)>& next_accessor) {
  return optimizer_core_.Optimize(next_accessor, &linear_program_, &lp_solver_,
                                  nullptr, nullptr, nullptr);
}

bool GlobalDimensionCumulOptimizer::ComputePackedCumuls(
    const std::function<int64(int64)>& next_accessor,
    std::vector<int64>* packed_cumuls) {
  return optimizer_core_.OptimizeAndPack(next_accessor, &linear_program_,
                                         &lp_solver_, packed_cumuls);
}
}  // namespace operations_research
