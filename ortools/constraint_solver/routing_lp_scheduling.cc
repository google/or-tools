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

bool GetCumulBoundsWithOffset(const RoutingDimension& dimension,
                              int64 node_index, int64 cumul_offset,
                              int64* lower_bound, int64* upper_bound) {
  DCHECK(lower_bound != nullptr);
  DCHECK(upper_bound != nullptr);

  const IntVar& cumul_var = *dimension.CumulVar(node_index);
  *upper_bound = cumul_var.Max();
  if (*upper_bound < cumul_offset) {
    return false;
  }

  const int64 first_after_offset =
      std::max(dimension.GetFirstPossibleGreaterOrEqualValueForNode(
                   node_index, cumul_offset),
               cumul_var.Min());
  DCHECK_LT(first_after_offset, kint64max);
  *lower_bound = CapSub(first_after_offset, cumul_offset);
  DCHECK_GE(*lower_bound, 0);

  if (*upper_bound == kint64max) {
    return true;
  }
  *upper_bound = CapSub(*upper_bound, cumul_offset);
  DCHECK_GE(*upper_bound, *lower_bound);
  return true;
}

int64 GetFirstPossibleValueForCumulWithOffset(const RoutingDimension& dimension,
                                              int64 node_index,
                                              int64 lower_bound_without_offset,
                                              int64 cumul_offset) {
  return CapSub(
      dimension.GetFirstPossibleGreaterOrEqualValueForNode(
          node_index, CapAdd(lower_bound_without_offset, cumul_offset)),
      cumul_offset);
}

int64 GetLastPossibleValueForCumulWithOffset(const RoutingDimension& dimension,
                                             int64 node_index,
                                             int64 upper_bound_without_offset,
                                             int64 cumul_offset) {
  return CapSub(
      dimension.GetLastPossibleLessOrEqualValueForNode(
          node_index, CapAdd(upper_bound_without_offset, cumul_offset)),
      cumul_offset);
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
    const RoutingDimension* dimension,
    RoutingSearchParameters::SchedulingSolver solver_type)
    : optimizer_core_(dimension, /*use_precedence_propagator=*/false) {
  // Using one solver per vehicle in the hope that if routes don't change this
  // will be faster.
  const int vehicles = dimension->model()->vehicles();
  solver_.resize(vehicles);
  switch (solver_type) {
    case RoutingSearchParameters::GLOP: {
      const glop::GlopParameters parameters = GetGlopParametersForLocalLP();
      for (int vehicle = 0; vehicle < vehicles; ++vehicle) {
        solver_[vehicle] = absl::make_unique<RoutingGlopWrapper>(parameters);
      }
      break;
    }
    case RoutingSearchParameters::CP_SAT: {
      for (int vehicle = 0; vehicle < vehicles; ++vehicle) {
        solver_[vehicle] = absl::make_unique<RoutingCPSatWrapper>();
      }
      break;
    }
    default:
      LOG(DFATAL) << "Unrecognized solver type: " << solver_type;
  }
}

DimensionSchedulingStatus LocalDimensionCumulOptimizer::ComputeRouteCumulCost(
    int vehicle, const std::function<int64(int64)>& next_accessor,
    int64* optimal_cost) {
  return optimizer_core_.OptimizeSingleRoute(vehicle, next_accessor,
                                             solver_[vehicle].get(), nullptr,
                                             nullptr, optimal_cost, nullptr);
}

DimensionSchedulingStatus
LocalDimensionCumulOptimizer::ComputeRouteCumulCostWithoutFixedTransits(
    int vehicle, const std::function<int64(int64)>& next_accessor,
    int64* optimal_cost_without_transits) {
  int64 cost = 0;
  int64 transit_cost = 0;
  const DimensionSchedulingStatus status = optimizer_core_.OptimizeSingleRoute(
      vehicle, next_accessor, solver_[vehicle].get(), nullptr, nullptr, &cost,
      &transit_cost);
  if (status != DimensionSchedulingStatus::INFEASIBLE &&
      optimal_cost_without_transits != nullptr) {
    *optimal_cost_without_transits = CapSub(cost, transit_cost);
  }
  return status;
}

DimensionSchedulingStatus LocalDimensionCumulOptimizer::ComputeRouteCumuls(
    int vehicle, const std::function<int64(int64)>& next_accessor,
    std::vector<int64>* optimal_cumuls, std::vector<int64>* optimal_breaks) {
  return optimizer_core_.OptimizeSingleRoute(
      vehicle, next_accessor, solver_[vehicle].get(), optimal_cumuls,
      optimal_breaks, nullptr, nullptr);
}

DimensionSchedulingStatus
LocalDimensionCumulOptimizer::ComputePackedRouteCumuls(
    int vehicle, const std::function<int64(int64)>& next_accessor,
    std::vector<int64>* packed_cumuls, std::vector<int64>* packed_breaks) {
  return optimizer_core_.OptimizeAndPackSingleRoute(
      vehicle, next_accessor, solver_[vehicle].get(), packed_cumuls,
      packed_breaks);
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
      if (!GetCumulBoundsWithOffset(dimension_, node, cumul_offset, &cumul_lb,
                                    &cumul_ub)) {
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
                                                          int64 new_lb,
                                                          int64 offset) {
  const int cumul_var_index = node / 2;

  if (node == PositiveNode(cumul_var_index)) {
    // new_lb is a lower bound of the cumul of variable 'cumul_var_index'.
    propagated_bounds_[node] = GetFirstPossibleValueForCumulWithOffset(
        dimension_, cumul_var_index, new_lb, offset);
  } else {
    // -new_lb is an upper bound of the cumul of variable 'cumul_var_index'.
    const int64 new_ub = CapSub(0, new_lb);
    propagated_bounds_[node] =
        CapSub(0, GetLastPossibleValueForCumulWithOffset(
                      dimension_, cumul_var_index, new_ub, offset));
  }

  // Test that the lower/upper bounds do not cross each other.
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
      if (!UpdateCurrentLowerBoundOfNode(head_node, induced_lb, cumul_offset) ||
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

DimensionCumulOptimizerCore::DimensionCumulOptimizerCore(
    const RoutingDimension* dimension, bool use_precedence_propagator)
    : dimension_(dimension),
      visited_pickup_delivery_indices_for_pair_(
          dimension->model()->GetPickupAndDeliveryPairs().size(), {-1, -1}) {
  if (use_precedence_propagator) {
    propagator_ = absl::make_unique<CumulBoundsPropagator>(dimension);
  }
  if (dimension_->HasBreakConstraints()) {
    // Initialize vehicle_to_first_index_ so the variables of the breaks of
    // vehicle v are stored from vehicle_to_first_index_[v] to
    // vehicle_to_first_index_[v+1] - 1.
    const int num_vehicles = dimension_->model()->vehicles();
    vehicle_to_all_break_variables_offset_.reserve(num_vehicles);
    int num_break_vars = 0;
    for (int vehicle = 0; vehicle < num_vehicles; ++vehicle) {
      vehicle_to_all_break_variables_offset_.push_back(num_break_vars);
      const auto& intervals = dimension_->GetBreakIntervalsOfVehicle(vehicle);
      num_break_vars += 2 * intervals.size();  // 2 variables per break.
    }
    all_break_variables_.resize(num_break_vars, -1);
  }
}

DimensionSchedulingStatus DimensionCumulOptimizerCore::OptimizeSingleRoute(
    int vehicle, const std::function<int64(int64)>& next_accessor,
    RoutingLinearSolverWrapper* solver, std::vector<int64>* cumul_values,
    std::vector<int64>* break_values, int64* cost, int64* transit_cost,
    bool clear_lp) {
  InitOptimizer(solver);
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
                                optimize_vehicle_costs, solver, transit_cost,
                                &cost_offset)) {
    return DimensionSchedulingStatus::INFEASIBLE;
  }
  const DimensionSchedulingStatus status =
      solver->Solve(model->RemainingTime());
  if (status == DimensionSchedulingStatus::INFEASIBLE) {
    return status;
  }

  SetValuesFromLP(current_route_cumul_variables_, cumul_offset, solver,
                  cumul_values);
  SetValuesFromLP(current_route_break_variables_, cumul_offset, solver,
                  break_values);
  if (cost != nullptr) {
    *cost = CapAdd(cost_offset, solver->GetObjectiveValue());
  }

  if (clear_lp) {
    solver->Clear();
  }
  return status;
}

bool DimensionCumulOptimizerCore::Optimize(
    const std::function<int64(int64)>& next_accessor,
    RoutingLinearSolverWrapper* solver, std::vector<int64>* cumul_values,
    std::vector<int64>* break_values, int64* cost, int64* transit_cost,
    bool clear_lp) {
  InitOptimizer(solver);

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
                                  optimize_vehicle_costs, solver,
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

  SetGlobalConstraints(has_vehicles_being_optimized, solver);

  if (solver->Solve(model->RemainingTime()) ==
      DimensionSchedulingStatus::INFEASIBLE) {
    return false;
  }

  SetValuesFromLP(index_to_cumul_variable_, cumul_offset, solver, cumul_values);
  SetValuesFromLP(all_break_variables_, cumul_offset, solver, break_values);

  if (cost != nullptr) {
    *cost = CapAdd(solver->GetObjectiveValue(), total_cost_offset);
  }

  if (clear_lp) {
    solver->Clear();
  }
  return true;
}

bool DimensionCumulOptimizerCore::OptimizeAndPack(
    const std::function<int64(int64)>& next_accessor,
    RoutingLinearSolverWrapper* solver, std::vector<int64>* cumul_values,
    std::vector<int64>* break_values) {
  // Note: We pass a non-nullptr cost to the Optimize() method so the costs are
  // optimized by the LP.
  int64 cost = 0;
  if (!Optimize(next_accessor, solver,
                /*cumul_values=*/nullptr, /*break_values=*/nullptr, &cost,
                /*transit_cost=*/nullptr, /*clear_lp=*/false)) {
    return false;
  }

  std::vector<int> vehicles(dimension()->model()->vehicles());
  std::iota(vehicles.begin(), vehicles.end(), 0);
  if (PackRoutes(std::move(vehicles), solver) ==
      DimensionSchedulingStatus::INFEASIBLE) {
    return false;
  }
  const int64 global_offset = dimension_->GetGlobalOptimizerOffset();
  SetValuesFromLP(index_to_cumul_variable_, global_offset, solver,
                  cumul_values);
  SetValuesFromLP(all_break_variables_, global_offset, solver, break_values);
  solver->Clear();
  return true;
}

DimensionSchedulingStatus
DimensionCumulOptimizerCore::OptimizeAndPackSingleRoute(
    int vehicle, const std::function<int64(int64)>& next_accessor,
    RoutingLinearSolverWrapper* solver, std::vector<int64>* cumul_values,
    std::vector<int64>* break_values) {
  // Note: We pass a non-nullptr cost to the OptimizeSingleRoute() method so the
  // costs are optimized by the LP.
  int64 cost = 0;
  if (OptimizeSingleRoute(vehicle, next_accessor, solver,
                          /*cumul_values=*/nullptr, /*break_values=*/nullptr,
                          &cost, /*transit_cost=*/nullptr,
                          /*clear_lp=*/false) ==
      DimensionSchedulingStatus::INFEASIBLE) {
    return DimensionSchedulingStatus::INFEASIBLE;
  }
  const DimensionSchedulingStatus status = PackRoutes({vehicle}, solver);
  if (status == DimensionSchedulingStatus::INFEASIBLE) {
    return DimensionSchedulingStatus::INFEASIBLE;
  }
  const int64 local_offset =
      dimension_->GetLocalOptimizerOffsetForVehicle(vehicle);
  SetValuesFromLP(current_route_cumul_variables_, local_offset, solver,
                  cumul_values);
  SetValuesFromLP(current_route_break_variables_, local_offset, solver,
                  break_values);
  solver->Clear();
  return status;
}

DimensionSchedulingStatus DimensionCumulOptimizerCore::PackRoutes(
    std::vector<int> vehicles, RoutingLinearSolverWrapper* solver) {
  const RoutingModel* model = dimension_->model();

  // NOTE(user): Given our constraint matrix, our problem *should* always
  // have an integer optimal solution, in which case we can round to the nearest
  // integer both for the objective constraint bound (returned by
  // GetObjectiveValue()) and the end cumul variable bound after minimizing
  // (see b/154381899 showcasing an example where std::ceil leads to an
  // "imperfect" packing due to rounding precision errors).
  // If this DCHECK ever fails, it can be removed but the code below should be
  // adapted to have a 2-phase approach, solving once with the rounded value as
  // bound and if this fails, solve again using std::ceil.
  DCHECK(solver->SolutionIsInteger());

  // Minimize the route end times without increasing the cost.
  const int objective_ct =
      solver->CreateNewConstraint(0, solver->GetObjectiveValue());

  for (int variable = 0; variable < solver->NumVariables(); variable++) {
    const double coefficient = solver->GetObjectiveCoefficient(variable);
    if (coefficient != 0) {
      solver->SetCoefficient(objective_ct, variable, coefficient);
    }
  }
  solver->ClearObjective();
  for (int vehicle : vehicles) {
    solver->SetObjectiveCoefficient(
        index_to_cumul_variable_[model->End(vehicle)], 1);
  }

  if (solver->Solve(model->RemainingTime()) ==
      DimensionSchedulingStatus::INFEASIBLE) {
    return DimensionSchedulingStatus::INFEASIBLE;
  }

  // Maximize the route start times without increasing the cost or the route end
  // times.
  solver->ClearObjective();
  for (int vehicle : vehicles) {
    const int end_cumul_var = index_to_cumul_variable_[model->End(vehicle)];
    // end_cumul_var <= solver.GetValue(end_cumul_var)
    solver->SetVariableBounds(
        end_cumul_var, solver->GetVariableLowerBound(end_cumul_var),
        MathUtil::FastInt64Round(solver->GetValue(end_cumul_var)));

    // Maximize the starts of the routes.
    solver->SetObjectiveCoefficient(
        index_to_cumul_variable_[model->Start(vehicle)], -1);
  }
  return solver->Solve(model->RemainingTime());
}

void DimensionCumulOptimizerCore::InitOptimizer(
    RoutingLinearSolverWrapper* solver) {
  solver->Clear();
  index_to_cumul_variable_.assign(dimension_->cumuls().size(), -1);
  max_end_cumul_ = solver->CreateNewPositiveVariable();
  min_start_cumul_ = solver->CreateNewPositiveVariable();
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
    if (!GetCumulBoundsWithOffset(*dimension_, route[pos], cumul_offset,
                                  &current_route_min_cumuls_[pos],
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
    current_route_min_cumuls_[pos] = GetFirstPossibleValueForCumulWithOffset(
        *dimension_, route[pos], current_route_min_cumuls_[pos], cumul_offset);
    if (current_route_min_cumuls_[pos] > current_route_max_cumuls_[pos]) {
      return false;
    }
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
      current_route_max_cumuls_[pos] = GetLastPossibleValueForCumulWithOffset(
          *dimension_, route[pos], current_route_max_cumuls_[pos],
          cumul_offset);
      if (current_route_max_cumuls_[pos] < current_route_min_cumuls_[pos]) {
        return false;
      }
    }
  }
  return true;
}

bool DimensionCumulOptimizerCore::SetRouteCumulConstraints(
    int vehicle, const std::function<int64(int64)>& next_accessor,
    int64 cumul_offset, bool optimize_costs, RoutingLinearSolverWrapper* solver,
    int64* route_transit_cost, int64* route_cost_offset) {
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
  std::vector<int>& lp_cumuls = current_route_cumul_variables_;
  lp_cumuls.assign(path_size, -1);
  for (int pos = 0; pos < path_size; ++pos) {
    const int lp_cumul = solver->CreateNewPositiveVariable();
    index_to_cumul_variable_[path[pos]] = lp_cumul;
    lp_cumuls[pos] = lp_cumul;
    if (!solver->SetVariableBounds(lp_cumul, current_route_min_cumuls_[pos],
                                   current_route_max_cumuls_[pos])) {
      return false;
    }
    const SortedDisjointIntervalList& forbidden =
        dimension_->forbidden_intervals()[path[pos]];
    if (forbidden.NumIntervals() > 0) {
      std::vector<int64> starts;
      std::vector<int64> ends;
      for (const ClosedInterval interval :
           dimension_->GetAllowedIntervalsInRange(
               path[pos], CapAdd(current_route_min_cumuls_[pos], cumul_offset),
               CapAdd(current_route_max_cumuls_[pos], cumul_offset))) {
        starts.push_back(CapSub(interval.start, cumul_offset));
        ends.push_back(CapSub(interval.end, cumul_offset));
      }
      solver->SetVariableDisjointBounds(lp_cumul, starts, ends);
    }
  }
  // Create LP variables for slacks.
  std::vector<int> lp_slacks(path_size - 1, -1);
  for (int pos = 0; pos < path_size - 1; ++pos) {
    const IntVar* cp_slack = dimension_->SlackVar(path[pos]);
    lp_slacks[pos] = solver->CreateNewPositiveVariable();
    if (!solver->SetVariableBounds(lp_slacks[pos], cp_slack->Min(),
                                   cp_slack->Max())) {
      return false;
    }
  }

  // LP Model constraints and costs.
  // Add all path constraints to LP:
  // cumul[i] + fixed_transit[i] + slack[i] == cumul[i+1]
  // <=> fixed_transit[i] == cumul[i+1] - cumul[i] - slack[i].
  for (int pos = 0; pos < path_size - 1; ++pos) {
    const int ct =
        solver->CreateNewConstraint(fixed_transit[pos], fixed_transit[pos]);
    solver->SetCoefficient(ct, lp_cumuls[pos + 1], 1);
    solver->SetCoefficient(ct, lp_cumuls[pos], -1);
    solver->SetCoefficient(ct, lp_slacks[pos], -1);
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
      const int soft_ub_diff = solver->CreateNewPositiveVariable();
      solver->SetObjectiveCoefficient(soft_ub_diff, coef);
      // cumul - soft_ub_diff <= bound.
      const int ct = solver->CreateNewConstraint(kint64min, bound);
      solver->SetCoefficient(ct, lp_cumuls[pos], 1);
      solver->SetCoefficient(ct, soft_ub_diff, -1);
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
      const int soft_lb_diff = solver->CreateNewPositiveVariable();
      solver->SetObjectiveCoefficient(soft_lb_diff, coef);
      // bound - cumul <= soft_lb_diff
      const int ct = solver->CreateNewConstraint(bound, kint64max);
      solver->SetCoefficient(ct, lp_cumuls[pos], 1);
      solver->SetCoefficient(ct, soft_lb_diff, 1);
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
      const int ct = solver->CreateNewConstraint(kint64min, limit);
      solver->SetCoefficient(ct, index_to_cumul_variable_[delivery_index], 1);
      solver->SetCoefficient(ct, index_to_cumul_variable_[pickup_index], -1);
    }
  }

  // Add span bound constraint.
  const int64 span_bound = dimension_->GetSpanUpperBoundForVehicle(vehicle);
  if (span_bound < kint64max) {
    // end_cumul - start_cumul <= bound
    const int ct = solver->CreateNewConstraint(kint64min, span_bound);
    solver->SetCoefficient(ct, lp_cumuls.back(), 1);
    solver->SetCoefficient(ct, lp_cumuls.front(), -1);
  }
  // Add span cost.
  const int64 span_cost_coef =
      dimension_->GetSpanCostCoefficientForVehicle(vehicle);
  if (optimize_costs && span_cost_coef > 0) {
    solver->SetObjectiveCoefficient(lp_cumuls.back(), span_cost_coef);
    solver->SetObjectiveCoefficient(lp_cumuls.front(), -span_cost_coef);
  }
  // Add soft span cost.
  if (optimize_costs && dimension_->HasSoftSpanUpperBounds()) {
    SimpleBoundCosts::BoundCost bound_cost =
        dimension_->GetSoftSpanUpperBoundForVehicle(vehicle);
    if (bound_cost.bound < kint64max && bound_cost.cost > 0) {
      const int span_violation = solver->CreateNewPositiveVariable();
      // end - start <= bound + span_violation
      const int violation =
          solver->CreateNewConstraint(kint64min, bound_cost.bound);
      solver->SetCoefficient(violation, lp_cumuls.back(), 1.0);
      solver->SetCoefficient(violation, lp_cumuls.front(), -1.0);
      solver->SetCoefficient(violation, span_violation, -1.0);
      // Add span_violation * cost to objective.
      solver->SetObjectiveCoefficient(span_violation, bound_cost.cost);
    }
  }
  // Add global span constraint.
  if (optimize_costs && dimension_->global_span_cost_coefficient() > 0) {
    // min_start_cumul_ <= cumuls[start]
    int ct = solver->CreateNewConstraint(kint64min, 0);
    solver->SetCoefficient(ct, min_start_cumul_, 1);
    solver->SetCoefficient(ct, lp_cumuls.front(), -1);
    // max_end_cumul_ >= cumuls[end]
    ct = solver->CreateNewConstraint(0, kint64max);
    solver->SetCoefficient(ct, max_end_cumul_, 1);
    solver->SetCoefficient(ct, lp_cumuls.back(), -1);
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
  // TODO(user): remove the need for returns in the code below.
  current_route_break_variables_.clear();
  if (!dimension_->HasBreakConstraints()) return true;
  const std::vector<IntervalVar*>& breaks =
      dimension_->GetBreakIntervalsOfVehicle(vehicle);
  const int num_breaks = breaks.size();
  // When there are no breaks, only break distance needs to be modeled,
  // and it reduces to a span maximum.
  // TODO(user): Also add the case where no breaks can intersect the route.
  if (num_breaks == 0) {
    int64 maximum_route_span = kint64max;
    for (const auto& distance_duration :
         dimension_->GetBreakDistanceDurationOfVehicle(vehicle)) {
      maximum_route_span =
          std::min(maximum_route_span, distance_duration.first);
    }
    if (maximum_route_span < kint64max) {
      const int ct = solver->CreateNewConstraint(kint64min, maximum_route_span);
      solver->SetCoefficient(ct, lp_cumuls.back(), 1);
      solver->SetCoefficient(ct, lp_cumuls.front(), -1);
    }
    return true;
  }
  // Gather visit information: the visit of node i has [start, end) =
  // [cumul[i] - post_travel[i-1], cumul[i] + pre_travel[i]).
  // Breaks cannot overlap those visit intervals.
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
  // If the solver is CPSAT, it will need to represent the times at which
  // breaks are scheduled, those variables are used both in the pure breaks
  // part and in the break distance part of the model.
  // Otherwise, it doesn't need the variables and they are not created.
  std::vector<int> lp_break_start;
  std::vector<int> lp_break_duration;
  std::vector<int> lp_break_end;
  if (solver->IsCPSATSolver()) {
    lp_break_start.resize(num_breaks, -1);
    lp_break_duration.resize(num_breaks, -1);
    lp_break_end.resize(num_breaks, -1);
  }

  std::vector<int> slack_exact_lower_bound_ct(path_size - 1, -1);
  std::vector<int> slack_linear_lower_bound_ct(path_size - 1, -1);

  const int64 vehicle_start_min = current_route_min_cumuls_.front();
  const int64 vehicle_start_max = current_route_max_cumuls_.front();
  const int64 vehicle_end_min = current_route_min_cumuls_.back();
  const int64 vehicle_end_max = current_route_max_cumuls_.back();
  const int all_break_variables_offset =
      vehicle_to_all_break_variables_offset_[vehicle];
  for (int br = 0; br < num_breaks; ++br) {
    const IntervalVar& break_var = *breaks[br];
    if (!break_var.MustBePerformed()) continue;
    const int64 break_start_min = CapSub(break_var.StartMin(), cumul_offset);
    const int64 break_start_max = CapSub(break_var.StartMax(), cumul_offset);
    const int64 break_end_min = CapSub(break_var.EndMin(), cumul_offset);
    const int64 break_end_max = CapSub(break_var.EndMax(), cumul_offset);
    const int64 break_duration_min = break_var.DurationMin();
    const int64 break_duration_max = break_var.DurationMax();
    // The CPSAT solver encodes all breaks that can intersect the route,
    // the LP solver only encodes the breaks that must intersect the route.
    if (solver->IsCPSATSolver()) {
      if (break_end_max <= vehicle_start_min ||
          vehicle_end_max <= break_start_min) {
        all_break_variables_[all_break_variables_offset + 2 * br] = -1;
        all_break_variables_[all_break_variables_offset + 2 * br + 1] = -1;
        current_route_break_variables_.push_back(-1);
        current_route_break_variables_.push_back(-1);
        continue;
      }
      lp_break_start[br] =
          solver->AddVariable(break_start_min, break_start_max);
      lp_break_end[br] = solver->AddVariable(break_end_min, break_end_max);
      lp_break_duration[br] =
          solver->AddVariable(break_duration_min, break_duration_max);
      // start + duration = end.
      solver->AddLinearConstraint(0, 0,
                                  {{lp_break_end[br], 1},
                                   {lp_break_start[br], -1},
                                   {lp_break_duration[br], -1}});
      // Record index of variables
      all_break_variables_[all_break_variables_offset + 2 * br] =
          lp_break_start[br];
      all_break_variables_[all_break_variables_offset + 2 * br + 1] =
          lp_break_end[br];
      current_route_break_variables_.push_back(lp_break_start[br]);
      current_route_break_variables_.push_back(lp_break_end[br]);
    } else {
      if (break_end_min <= vehicle_start_max ||
          vehicle_end_min <= break_start_max) {
        continue;
      }
    }

    // Create a constraint for every break, that forces it to be scheduled
    // in exactly one place, i.e. one slack or before/after the route.
    // sum_i break_in_slack_i  == 1.
    const int break_in_one_slack_ct = solver->CreateNewConstraint(1, 1);

    if (solver->IsCPSATSolver()) {
      // Break can be before route.
      if (break_end_min <= vehicle_start_max) {
        const int ct = solver->AddLinearConstraint(
            0, kint64max, {{lp_cumuls.front(), 1}, {lp_break_end[br], -1}});
        const int break_is_before_route = solver->AddVariable(0, 1);
        solver->SetEnforcementLiteral(ct, break_is_before_route);
        solver->SetCoefficient(break_in_one_slack_ct, break_is_before_route, 1);
      }
      // Break can be after route.
      if (vehicle_end_min <= break_start_max) {
        const int ct = solver->AddLinearConstraint(
            0, kint64max, {{lp_break_start[br], 1}, {lp_cumuls.back(), -1}});
        const int break_is_after_route = solver->AddVariable(0, 1);
        solver->SetEnforcementLiteral(ct, break_is_after_route);
        solver->SetCoefficient(break_in_one_slack_ct, break_is_after_route, 1);
      }
    }

    // Add the possibility of fitting the break during each slack where it can.
    for (int pos = 0; pos < path_size - 1; ++pos) {
      // Pass on slacks that cannot start before, cannot end after,
      // or are not long enough to contain the break.
      const int64 slack_start_min =
          CapAdd(current_route_min_cumuls_[pos], pre_travel[pos]);
      if (slack_start_min > break_start_max) break;
      const int64 slack_end_max =
          CapSub(current_route_max_cumuls_[pos + 1], post_travel[pos]);
      if (break_end_min > slack_end_max) continue;
      const int64 slack_duration_max =
          std::min(CapSub(CapSub(current_route_max_cumuls_[pos + 1],
                                 current_route_min_cumuls_[pos]),
                          fixed_transit[pos]),
                   dimension_->SlackVar(path[pos])->Max());
      if (slack_duration_max < break_duration_min) continue;

      // Break can fit into slack: make LP variable, add to break and slack
      // constraints.
      // Make a linearized slack lower bound (lazily), that represents
      // sum_br break_duration_min(br) * break_in_slack(br, pos) <=
      // lp_slacks(pos).
      const int break_in_slack = solver->AddVariable(0, 1);
      solver->SetCoefficient(break_in_one_slack_ct, break_in_slack, 1);
      if (slack_linear_lower_bound_ct[pos] == -1) {
        slack_linear_lower_bound_ct[pos] =
            solver->AddLinearConstraint(kint64min, 0, {{lp_slacks[pos], -1}});
      }
      solver->SetCoefficient(slack_linear_lower_bound_ct[pos], break_in_slack,
                             break_duration_min);
      if (solver->IsCPSATSolver()) {
        // Exact relation between breaks, slacks and cumul variables.
        // Make an exact slack lower bound (lazily), that represents
        // sum_br break_duration(br) * break_in_slack(br, pos) <=
        // lp_slacks(pos).
        const int break_duration_in_slack =
            solver->AddVariable(0, slack_duration_max);
        solver->AddProductConstraint(break_duration_in_slack,
                                     {break_in_slack, lp_break_duration[br]});
        if (slack_exact_lower_bound_ct[pos] == -1) {
          slack_exact_lower_bound_ct[pos] =
              solver->AddLinearConstraint(kint64min, 0, {{lp_slacks[pos], -1}});
        }
        solver->SetCoefficient(slack_exact_lower_bound_ct[pos],
                               break_duration_in_slack, 1);
        // If break_in_slack_i == 1, then
        // 1) break_start >= cumul[pos] + pre_travel[pos]
        const int break_start_after_current_ct = solver->AddLinearConstraint(
            pre_travel[pos], kint64max,
            {{lp_break_start[br], 1}, {lp_cumuls[pos], -1}});
        solver->SetEnforcementLiteral(break_start_after_current_ct,
                                      break_in_slack);
        // 2) break_end <= cumul[pos+1] - post_travel[pos]
        const int break_ends_before_next_ct = solver->AddLinearConstraint(
            post_travel[pos], kint64max,
            {{lp_cumuls[pos + 1], 1}, {lp_break_end[br], -1}});
        solver->SetEnforcementLiteral(break_ends_before_next_ct,
                                      break_in_slack);
      }
    }
  }

  if (!solver->IsCPSATSolver()) return true;
  if (!dimension_->GetBreakDistanceDurationOfVehicle(vehicle).empty()) {
    // If there is an optional interval, the following model would be wrong.
    // TODO(user): support optional intervals.
    for (const IntervalVar* interval :
         dimension_->GetBreakIntervalsOfVehicle(vehicle)) {
      if (!interval->MustBePerformed()) return true;
    }
    // When this feature is used, breaks are in sorted order.
    for (int br = 1; br < num_breaks; ++br) {
      solver->AddLinearConstraint(
          0, kint64max, {{lp_break_end[br - 1], -1}, {lp_break_start[br], 1}});
    }
  }
  for (const auto& distance_duration :
       dimension_->GetBreakDistanceDurationOfVehicle(vehicle)) {
    const int64 limit = distance_duration.first;
    const int64 min_break_duration = distance_duration.second;
    // Interbreak limit constraint: breaks are interpreted as being in sorted
    // order, and the maximum duration between two consecutive
    // breaks of duration more than 'min_break_duration' is 'limit'. This
    // considers the time until start of route and after end of route to be
    // infinite breaks.
    // The model for this constraint adds some 'cover_i' variables, such that
    // the breaks up to i and the start of route allows to go without a break.
    // With s_i the start of break i and e_i its end:
    // - the route start covers time from start to start + limit:
    //   cover_0 = route_start + limit
    // - the coverage up to a given break is the largest of the coverage of the
    //   previous break and if the break is long enough, break end + limit:
    //   cover_{i+1} = max(cover_i,
    //                     e_i - s_i >= min_break_duration ? e_i + limit : -inf)
    // - the coverage of the last break must be at least the route end,
    //   to ensure the time point route_end-1 is covered:
    //   cover_{num_breaks} >= route_end
    // - similarly, time point s_i-1 must be covered by breaks up to i-1,
    //   but only if the cover has not reached the route end.
    //   For instance, a vehicle could have a choice between two days,
    //   with a potential break on day 1 and a potential break on day 2,
    //   but the break of day 1 does not have to cover that of day 2!
    //   cover_{i-1} < route_end => s_i <= cover_{i-1}
    // This is sufficient to ensure that the union of the intervals
    // (-infinity, route_start], [route_end, +infinity) and all
    // [s_i, e_i+limit) where e_i - s_i >= min_break_duration is
    // the whole timeline (-infinity, +infinity).
    int previous_cover = solver->AddVariable(CapAdd(vehicle_start_min, limit),
                                             CapAdd(vehicle_start_max, limit));
    solver->AddLinearConstraint(limit, limit,
                                {{previous_cover, 1}, {lp_cumuls.front(), -1}});
    for (int br = 0; br < num_breaks; ++br) {
      if (lp_break_start[br] == -1) continue;
      const int64 break_end_min = CapSub(breaks[br]->EndMin(), cumul_offset);
      const int64 break_end_max = CapSub(breaks[br]->EndMax(), cumul_offset);
      // break_is_eligible <=>
      // break_end - break_start >= break_minimum_duration.
      const int break_is_eligible = solver->AddVariable(0, 1);
      const int break_is_not_eligible = solver->AddVariable(0, 1);
      {
        solver->AddLinearConstraint(
            1, 1, {{break_is_eligible, 1}, {break_is_not_eligible, 1}});
        const int positive_ct = solver->AddLinearConstraint(
            min_break_duration, kint64max,
            {{lp_break_end[br], 1}, {lp_break_start[br], -1}});
        solver->SetEnforcementLiteral(positive_ct, break_is_eligible);
        const int negative_ct = solver->AddLinearConstraint(
            kint64min, min_break_duration - 1,
            {{lp_break_end[br], 1}, {lp_break_start[br], -1}});
        solver->SetEnforcementLiteral(negative_ct, break_is_not_eligible);
      }
      // break_is_eligible => break_cover == break_end + limit.
      // break_is_not_eligible => break_cover == vehicle_start_min + limit.
      // break_cover's initial domain is the smallest interval that contains the
      // union of sets {vehicle_start_min+limit} and
      // [break_end_min+limit, break_end_max+limit).
      const int break_cover = solver->AddVariable(
          CapAdd(std::min(vehicle_start_min, break_end_min), limit),
          CapAdd(std::max(vehicle_start_min, break_end_max), limit));
      const int limit_cover_ct = solver->AddLinearConstraint(
          limit, limit, {{break_cover, 1}, {lp_break_end[br], -1}});
      solver->SetEnforcementLiteral(limit_cover_ct, break_is_eligible);
      const int empty_cover_ct = solver->AddLinearConstraint(
          CapAdd(vehicle_start_min, limit), CapAdd(vehicle_start_min, limit),
          {{break_cover, 1}});
      solver->SetEnforcementLiteral(empty_cover_ct, break_is_not_eligible);

      const int cover =
          solver->AddVariable(CapAdd(vehicle_start_min, limit), kint64max);
      solver->AddMaximumConstraint(cover, {previous_cover, break_cover});
      // Cover chaining. If route end is not covered, break start must be:
      // cover_{i-1} < route_end => s_i <= cover_{i-1}
      const int route_end_is_not_covered = solver->AddReifiedLinearConstraint(
          1, kint64max, {{lp_cumuls.back(), 1}, {previous_cover, -1}});
      const int break_start_cover_ct = solver->AddLinearConstraint(
          0, kint64max, {{previous_cover, 1}, {lp_break_start[br], -1}});
      solver->SetEnforcementLiteral(break_start_cover_ct,
                                    route_end_is_not_covered);

      previous_cover = cover;
    }
    solver->AddLinearConstraint(0, kint64max,
                                {{previous_cover, 1}, {lp_cumuls.back(), -1}});
  }

  return true;
}

void DimensionCumulOptimizerCore::SetGlobalConstraints(
    bool optimize_costs, RoutingLinearSolverWrapper* solver) {
  // Global span cost =
  //     global_span_cost_coefficient * (max_end_cumul - min_start_cumul).
  const int64 global_span_coeff = dimension_->global_span_cost_coefficient();
  if (optimize_costs && global_span_coeff > 0) {
    solver->SetObjectiveCoefficient(max_end_cumul_, global_span_coeff);
    solver->SetObjectiveCoefficient(min_start_cumul_, -global_span_coeff);
  }

  // Node precedence constraints, set when both nodes are visited.
  for (const RoutingDimension::NodePrecedence& precedence :
       dimension_->GetNodePrecedences()) {
    const int first_cumul_var = index_to_cumul_variable_[precedence.first_node];
    const int second_cumul_var =
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
    const int ct = solver->CreateNewConstraint(precedence.offset, kint64max);
    solver->SetCoefficient(ct, second_cumul_var, 1);
    solver->SetCoefficient(ct, first_cumul_var, -1);
  }
}

void DimensionCumulOptimizerCore::SetValuesFromLP(
    const std::vector<int>& lp_variables, int64 offset,
    RoutingLinearSolverWrapper* solver, std::vector<int64>* lp_values) {
  if (lp_values == nullptr) return;
  lp_values->assign(lp_variables.size(), kint64min);
  for (int i = 0; i < lp_variables.size(); i++) {
    const int cumul_var = lp_variables[i];
    if (cumul_var < 0) continue;  // Keep default value, kint64min.
    const double lp_value_double = solver->GetValue(cumul_var);
    const int64 lp_value_int64 =
        (lp_value_double >= kint64max)
            ? kint64max
            : MathUtil::FastInt64Round(lp_value_double);
    (*lp_values)[i] = CapAdd(lp_value_int64, offset);
  }
}

GlobalDimensionCumulOptimizer::GlobalDimensionCumulOptimizer(
    const RoutingDimension* dimension)
    : optimizer_core_(dimension,
                      /*use_precedence_propagator=*/
                      !dimension->GetNodePrecedences().empty()) {
  solver_ =
      absl::make_unique<RoutingGlopWrapper>(GetGlopParametersForGlobalLP());
}

bool GlobalDimensionCumulOptimizer::ComputeCumulCostWithoutFixedTransits(
    const std::function<int64(int64)>& next_accessor,
    int64* optimal_cost_without_transits) {
  int64 cost = 0;
  int64 transit_cost = 0;
  if (optimizer_core_.Optimize(next_accessor, solver_.get(), nullptr, nullptr,
                               &cost, &transit_cost)) {
    if (optimal_cost_without_transits != nullptr) {
      *optimal_cost_without_transits = CapSub(cost, transit_cost);
    }
    return true;
  }
  return false;
}

bool GlobalDimensionCumulOptimizer::ComputeCumuls(
    const std::function<int64(int64)>& next_accessor,
    std::vector<int64>* optimal_cumuls, std::vector<int64>* optimal_breaks) {
  return optimizer_core_.Optimize(next_accessor, solver_.get(), optimal_cumuls,
                                  optimal_breaks, nullptr, nullptr);
}

bool GlobalDimensionCumulOptimizer::IsFeasible(
    const std::function<int64(int64)>& next_accessor) {
  return optimizer_core_.Optimize(next_accessor, solver_.get(), nullptr,
                                  nullptr, nullptr, nullptr);
}

bool GlobalDimensionCumulOptimizer::ComputePackedCumuls(
    const std::function<int64(int64)>& next_accessor,
    std::vector<int64>* packed_cumuls, std::vector<int64>* packed_breaks) {
  return optimizer_core_.OptimizeAndPack(next_accessor, solver_.get(),
                                         packed_cumuls, packed_breaks);
}
}  // namespace operations_research
