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

#include "ortools/routing/routing.h"

#include <limits.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/functional/any_invocable.h"
#include "absl/functional/bind_front.h"
#include "absl/hash/hash.h"
#include "absl/log/check.h"
#include "absl/log/die_if_null.h"
#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "google/protobuf/util/message_differencer.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/strong_vector.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/solver_parameters.pb.h"
#include "ortools/graph/connected_components.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/linear_assignment.h"
#include "ortools/routing/constraints.h"
#include "ortools/routing/decision_builders.h"
#include "ortools/routing/enums.pb.h"
#include "ortools/routing/filters.h"
#include "ortools/routing/ils.h"
#include "ortools/routing/ils.pb.h"
#include "ortools/routing/index_manager.h"
#include "ortools/routing/insertion_lns.h"
#include "ortools/routing/lp_scheduling.h"
#include "ortools/routing/neighborhoods.h"
#include "ortools/routing/parameters.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/routing/search.h"
#include "ortools/routing/types.h"
#include "ortools/routing/utils.h"
#include "ortools/util/bitset.h"
#include "ortools/util/optional_boolean.pb.h"
#include "ortools/util/piecewise_linear_function.h"
#include "ortools/util/range_query_function.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/stats.h"

namespace {
using GraphNodeIndex = int32_t;
using GraphArcIndex = int32_t;
using Graph = ::util::ListGraph<GraphNodeIndex, GraphArcIndex>;
using CostValue = int64_t;
}  // namespace

// Trace settings

namespace operations_research::routing {

std::string RoutingModel::RouteDimensionTravelInfo::DebugString(
    std::string line_prefix) const {
  std::string s = absl::StrFormat("%stravel_cost_coefficient: %ld", line_prefix,
                                  travel_cost_coefficient);
  for (int i = 0; i < transition_info.size(); ++i) {
    absl::StrAppendFormat(&s, "\ntransition[%d] {\n%s\n}\n", i,
                          transition_info[i].DebugString(line_prefix + "\t"));
  }
  return s;
}

std::string RoutingModel::RouteDimensionTravelInfo::TransitionInfo::DebugString(
    std::string line_prefix) const {
  return absl::StrFormat(
      R"({
%spre: %ld
%spost: %ld
%slower_bound: %ld
%supper_bound: %ld
%stravel_value: %s
%scost: %s
})",
      line_prefix, pre_travel_transit_value, line_prefix,
      post_travel_transit_value, line_prefix,
      compressed_travel_value_lower_bound, line_prefix,
      travel_value_upper_bound, line_prefix,
      travel_start_dependent_travel.DebugString(line_prefix + "\t"),
      line_prefix, travel_compression_cost.DebugString(line_prefix + "\t"));
}

const Assignment* RoutingModel::PackCumulsOfOptimizerDimensionsFromAssignment(
    const Assignment* original_assignment, absl::Duration duration_limit,
    bool* time_limit_was_reached) {
  CHECK(closed_);
  if (original_assignment == nullptr) return nullptr;
  if (duration_limit <= absl::ZeroDuration()) {
    if (time_limit_was_reached) *time_limit_was_reached = true;
    return original_assignment;
  }
  if (global_dimension_optimizers_.empty() &&
      local_dimension_optimizers_.empty()) {
    return original_assignment;
  }
  RegularLimit* const limit = GetOrCreateLimit();
  limit->UpdateLimits(duration_limit, std::numeric_limits<int64_t>::max(),
                      std::numeric_limits<int64_t>::max(),
                      std::numeric_limits<int64_t>::max());

  RegularLimit* const cumulative_limit = GetOrCreateCumulativeLimit();
  cumulative_limit->UpdateLimits(
      duration_limit, std::numeric_limits<int64_t>::max(),
      std::numeric_limits<int64_t>::max(), std::numeric_limits<int64_t>::max());

  // Initialize the packed_assignment with the Next values in the
  // original_assignment.
  Assignment* packed_assignment = solver_->MakeAssignment();
  packed_assignment->Add(Nexts());
  // Also keep the Resource values to avoid unnecessary re-optimizations.
  for (const RoutingDimension* const dimension : dimensions_) {
    for (int rg_index : GetDimensionResourceGroupIndices(dimension)) {
      DCHECK(HasLocalCumulOptimizer(*dimension));
      packed_assignment->Add(resource_vars_[rg_index]);
    }
  }
  packed_assignment->CopyIntersection(original_assignment);

  std::vector<DecisionBuilder*> decision_builders;
  decision_builders.push_back(solver_->MakeRestoreAssignment(preassignment_));
  decision_builders.push_back(
      solver_->MakeRestoreAssignment(packed_assignment));
  for (auto& [lp_optimizer, mp_optimizer] : local_dimension_optimizers_) {
    if (HasGlobalCumulOptimizer(*lp_optimizer->dimension())) {
      // Don't set cumuls of dimensions with a global optimizer.
      continue;
    }
    decision_builders.push_back(MakeSetCumulsFromLocalDimensionCosts(
        solver_.get(), lp_optimizer.get(), mp_optimizer.get(),
        /*optimize_and_pack=*/true));
  }
  for (auto& [lp_optimizer, mp_optimizer] : global_dimension_optimizers_) {
    decision_builders.push_back(MakeSetCumulsFromGlobalDimensionCosts(
        solver_.get(), lp_optimizer.get(), mp_optimizer.get(), cumulative_limit,
        /*optimize_and_pack=*/true));
  }
  decision_builders.push_back(finalizer_variables_->CreateFinalizer());

  DecisionBuilder* restore_pack_and_finalize =
      solver_->Compose(decision_builders);
  solver_->Solve(restore_pack_and_finalize,
                 optimized_dimensions_assignment_collector_, limit);
  const bool limit_was_reached = limit->Check();
  if (time_limit_was_reached) *time_limit_was_reached = limit_was_reached;
  if (optimized_dimensions_assignment_collector_->solution_count() != 1) {
    if (limit_was_reached) {
      VLOG(1) << "The packing reached the time limit.";
    } else {
      // TODO(user): Upgrade this to a LOG(DFATAL) when it no longer happens
      // in the stress test.
      LOG(ERROR) << "The given assignment is not valid for this model, or"
                    " cannot be packed.";
    }
    return nullptr;
  }

  packed_assignment->Copy(original_assignment);
  packed_assignment->CopyIntersection(
      optimized_dimensions_assignment_collector_->solution(0));

  return packed_assignment;
}

void RoutingModel::SetSweepArranger(SweepArranger* sweep_arranger) {
  sweep_arranger_.reset(sweep_arranger);
}

SweepArranger* RoutingModel::sweep_arranger() const {
  return sweep_arranger_.get();
}

void RoutingModel::NodeNeighborsByCostClass::ComputeNeighbors(
    const NodeNeighborsParameters& params) {
  auto [num_neighbors, add_vehicle_starts_to_neighbors,
        add_vehicle_ends_to_neighbors,
        only_sort_neighbors_for_partial_neighborhoods] = params;
  DCHECK_GE(num_neighbors, 0);
  // TODO(user): consider checking search limits.
  const int size = routing_model_.Size();
  const int num_non_start_end_nodes = size - routing_model_.vehicles();
  const int size_with_vehicle_nodes = size + routing_model_.vehicles();

  const int max_num_neighbors = std::max(num_non_start_end_nodes - 1, 0);
  num_neighbors = std::min<int>(max_num_neighbors, num_neighbors);
  node_index_to_incoming_neighbors_by_cost_class_.clear();
  node_index_to_outgoing_neighbors_by_cost_class_.clear();
  node_index_to_outgoing_neighbor_indicator_by_cost_class_.clear();
  all_incoming_nodes_.clear();
  all_outgoing_nodes_.clear();
  full_neighborhood_ = num_neighbors == max_num_neighbors;
  if (full_neighborhood_ && only_sort_neighbors_for_partial_neighborhoods) {
    all_incoming_nodes_.reserve(size);
    all_outgoing_nodes_.reserve(size);
    for (int node = 0; node < size_with_vehicle_nodes; node++) {
      const bool not_start = !routing_model_.IsStart(node);
      const bool not_end = !routing_model_.IsEnd(node);
      if (not_start && (not_end || add_vehicle_ends_to_neighbors)) {
        all_outgoing_nodes_.push_back(node);
      }
      if (not_end && (not_start || add_vehicle_starts_to_neighbors)) {
        all_incoming_nodes_.push_back(node);
      }
    }
    return;
  }

  const int num_cost_classes = routing_model_.GetCostClassesCount();
  node_index_to_incoming_neighbors_by_cost_class_.resize(num_cost_classes);
  node_index_to_outgoing_neighbors_by_cost_class_.resize(num_cost_classes);
  node_index_to_outgoing_neighbor_indicator_by_cost_class_.resize(
      num_cost_classes);
  std::vector<std::vector<std::vector<int64_t>>>
      node_index_to_outgoing_costs_by_cost_class(num_cost_classes);
  for (int cc = 0; cc < num_cost_classes; cc++) {
    if (!routing_model_.HasVehicleWithCostClassIndex(
            RoutingCostClassIndex(cc))) {
      continue;
    }
    node_index_to_incoming_neighbors_by_cost_class_[cc].resize(
        size_with_vehicle_nodes);
    node_index_to_outgoing_neighbors_by_cost_class_[cc].resize(size);
    node_index_to_outgoing_neighbor_indicator_by_cost_class_[cc].resize(size);
    node_index_to_outgoing_costs_by_cost_class[cc].resize(size);
    for (int node = 0; node < size_with_vehicle_nodes; node++) {
      node_index_to_incoming_neighbors_by_cost_class_[cc][node].reserve(
          num_neighbors + routing_model_.vehicles());
      if (node < size) {
        node_index_to_outgoing_neighbors_by_cost_class_[cc][node].reserve(
            num_neighbors + routing_model_.vehicles());
        node_index_to_outgoing_neighbor_indicator_by_cost_class_[cc][node]
            .resize(size_with_vehicle_nodes, false);
        node_index_to_outgoing_costs_by_cost_class[cc][node].resize(
            size_with_vehicle_nodes, -1);
      }
    }
  }

  std::vector<int> outgoing_neighbors;
  for (int cost_class = 0; cost_class < num_cost_classes; cost_class++) {
    if (!routing_model_.HasVehicleWithCostClassIndex(
            RoutingCostClassIndex(cost_class))) {
      // No vehicle with this cost class, avoid unnecessary computations.
      continue;
    }
    std::vector<std::vector<int>>& node_index_to_incoming_neighbors =
        node_index_to_incoming_neighbors_by_cost_class_[cost_class];
    std::vector<std::vector<int>>& node_index_to_outgoing_neighbors =
        node_index_to_outgoing_neighbors_by_cost_class_[cost_class];
    std::vector<std::vector<bool>>& node_index_to_outgoing_neighbor_indicator =
        node_index_to_outgoing_neighbor_indicator_by_cost_class_[cost_class];
    std::vector<std::vector<int64_t>>& node_index_to_outgoing_costs =
        node_index_to_outgoing_costs_by_cost_class[cost_class];
    for (int node_index = 0; node_index < size; ++node_index) {
      if (routing_model_.IsStart(node_index)) {
        // For vehicle start/ends, we consider all nodes (see below).
        continue;
      }

      // TODO(user): Use the model's IndexNeighborFinder when available.
      outgoing_neighbors.clear();
      outgoing_neighbors.reserve(num_non_start_end_nodes);
      if (num_neighbors > 0) {
        std::vector<int64_t>& costs = node_index_to_outgoing_costs[node_index];
        for (int after_node = 0; after_node < size; ++after_node) {
          if (after_node != node_index && !routing_model_.IsStart(after_node)) {
            costs[after_node] = routing_model_.GetArcCostForClass(
                node_index, after_node, cost_class);
            outgoing_neighbors.push_back(after_node);
          }
        }
        // Get the 'num_neighbors' closest neighbors.
        DCHECK_GE(outgoing_neighbors.size(), num_neighbors);
        std::nth_element(outgoing_neighbors.begin(),
                         outgoing_neighbors.begin() + num_neighbors - 1,
                         outgoing_neighbors.end(), [&costs](int n1, int n2) {
                           return std::tie(costs[n1], n1) <
                                  std::tie(costs[n2], n2);
                         });
        outgoing_neighbors.resize(num_neighbors);
      }

      // Add neighborhoods.
      for (int outgoing_neighbor : outgoing_neighbors) {
        DCHECK(!routing_model_.IsEnd(outgoing_neighbor) &&
               !routing_model_.IsStart(outgoing_neighbor));
        DCHECK(!node_index_to_outgoing_neighbor_indicator[node_index]
                                                         [outgoing_neighbor]);
        node_index_to_outgoing_neighbor_indicator[node_index]
                                                 [outgoing_neighbor] = true;
        node_index_to_outgoing_neighbors[node_index].push_back(
            outgoing_neighbor);
        // node_index is an incoming neighbor of outgoing_neighbor.
        node_index_to_incoming_neighbors[outgoing_neighbor].push_back(
            node_index);
      }
    }
  }

  // Add all vehicle start/ends as incoming/outgoing neighbors for all nodes.
  for (int cost_class = 0; cost_class < num_cost_classes; cost_class++) {
    if (!routing_model_.HasVehicleWithCostClassIndex(
            RoutingCostClassIndex(cost_class))) {
      // No vehicle with this cost class, avoid unnecessary computations.
      continue;
    }
    std::vector<std::vector<int>>& node_index_to_incoming_neighbors =
        node_index_to_incoming_neighbors_by_cost_class_[cost_class];
    std::vector<std::vector<int>>& node_index_to_outgoing_neighbors =
        node_index_to_outgoing_neighbors_by_cost_class_[cost_class];
    std::vector<std::vector<bool>>& node_index_to_outgoing_neighbor_indicator =
        node_index_to_outgoing_neighbor_indicator_by_cost_class_[cost_class];
    std::vector<std::vector<int64_t>>& node_index_to_outgoing_costs =
        node_index_to_outgoing_costs_by_cost_class[cost_class];
    for (int vehicle = 0; vehicle < routing_model_.vehicles(); vehicle++) {
      const int vehicle_start = routing_model_.Start(vehicle);
      const int vehicle_end = routing_model_.End(vehicle);

      // Mark vehicle_start -> vehicle_end as a neighborhood arc.
      DCHECK(!node_index_to_outgoing_neighbor_indicator[vehicle_start]
                                                       [vehicle_end]);
      node_index_to_outgoing_neighbor_indicator[vehicle_start][vehicle_end] =
          true;
      if (add_vehicle_starts_to_neighbors) {
        node_index_to_incoming_neighbors[vehicle_end].push_back(vehicle_start);
      }
      if (add_vehicle_ends_to_neighbors) {
        node_index_to_outgoing_neighbors[vehicle_start].push_back(vehicle_end);
      }
      node_index_to_outgoing_costs[vehicle_start][vehicle_end] =
          routing_model_.GetArcCostForClass(vehicle_start, vehicle_end,
                                            cost_class);

      for (int node_index = 0; node_index < size; ++node_index) {
        if (routing_model_.IsStart(node_index)) continue;

        // Mark vehicle_start -> node_index as a neighborhood arc.
        DCHECK(!node_index_to_outgoing_neighbor_indicator[node_index]
                                                         [vehicle_start]);
        DCHECK(!node_index_to_outgoing_neighbor_indicator[vehicle_start]
                                                         [node_index]);
        node_index_to_outgoing_neighbor_indicator[vehicle_start][node_index] =
            true;
        if (add_vehicle_starts_to_neighbors) {
          node_index_to_incoming_neighbors[node_index].push_back(vehicle_start);
        }
        node_index_to_outgoing_neighbors[vehicle_start].push_back(node_index);
        node_index_to_outgoing_costs[vehicle_start][node_index] =
            routing_model_.GetArcCostForClass(vehicle_start, node_index,
                                              cost_class);

        // Mark node_index -> vehicle_end as a neighborhood arc.
        DCHECK(!node_index_to_outgoing_neighbor_indicator[node_index]
                                                         [vehicle_end]);
        node_index_to_outgoing_neighbor_indicator[node_index][vehicle_end] =
            true;
        node_index_to_incoming_neighbors[vehicle_end].push_back(node_index);
        if (add_vehicle_ends_to_neighbors) {
          node_index_to_outgoing_neighbors[node_index].push_back(vehicle_end);
        }
        node_index_to_outgoing_costs[node_index][vehicle_end] =
            routing_model_.GetArcCostForClass(node_index, vehicle_end,
                                              cost_class);
      }
    }
  }

  // Sort the neighbors into
  // node_index_to_{incoming,outgoing}_neighbors_by_cost_class_ by cost.
  for (int cost_class = 0; cost_class < num_cost_classes; cost_class++) {
    if (!routing_model_.HasVehicleWithCostClassIndex(
            RoutingCostClassIndex(cost_class))) {
      // No vehicle with this cost class.
      continue;
    }
    const std::vector<std::vector<int64_t>>& node_index_to_outgoing_costs =
        node_index_to_outgoing_costs_by_cost_class[cost_class];
    for (int node_index = 0; node_index < size_with_vehicle_nodes;
         ++node_index) {
      std::vector<int>& incoming_node_neighbors =
          node_index_to_incoming_neighbors_by_cost_class_[cost_class]
                                                         [node_index];
      absl::c_sort(
          incoming_node_neighbors,
          [node_index, size, &node_index_to_outgoing_costs](int n1, int n2) {
            DCHECK_GE(node_index_to_outgoing_costs[n1][node_index], 0);
            DCHECK_GE(node_index_to_outgoing_costs[n2][node_index], 0);
            DCHECK_LT(n1, size);
            DCHECK_LT(n2, size);
            return std::tie(node_index_to_outgoing_costs[n1][node_index], n1) <
                   std::tie(node_index_to_outgoing_costs[n2][node_index], n2);
          });
      // Check that there are no duplicate elements.
      DCHECK(absl::c_adjacent_find(incoming_node_neighbors) ==
             incoming_node_neighbors.end());

      if (node_index < size) {
        std::vector<int>& outgoing_node_neighbors =
            node_index_to_outgoing_neighbors_by_cost_class_[cost_class]
                                                           [node_index];
        const std::vector<int64_t>& outgoing_costs =
            node_index_to_outgoing_costs[node_index];
        absl::c_sort(outgoing_node_neighbors,
                     [&outgoing_costs](int n1, int n2) {
                       DCHECK_GE(outgoing_costs[n1], 0);
                       DCHECK_GE(outgoing_costs[n2], 0);
                       return std::tie(outgoing_costs[n1], n1) <
                              std::tie(outgoing_costs[n2], n2);
                     });

        // Check that there are no duplicate elements.
        DCHECK(absl::c_adjacent_find(outgoing_node_neighbors) ==
               outgoing_node_neighbors.end());
      }
    }
  }
}

const RoutingModel::NodeNeighborsByCostClass*
RoutingModel::GetOrCreateNodeNeighborsByCostClass(
    double neighbors_ratio, int64_t min_neighbors, double& neighbors_ratio_used,
    bool add_vehicle_starts_to_neighbors, bool add_vehicle_ends_to_neighbors,
    bool only_sort_neighbors_for_partial_neighborhoods) {
  const int64_t num_non_start_end_nodes = Size() - vehicles();
  neighbors_ratio_used = neighbors_ratio;
  int num_neighbors = std::max(
      min_neighbors,
      MathUtil::SafeRound<int64_t>(neighbors_ratio * num_non_start_end_nodes));
  if (neighbors_ratio == 1 || num_neighbors >= num_non_start_end_nodes - 1) {
    neighbors_ratio_used = 1;
    num_neighbors = Size();
  }
  return GetOrCreateNodeNeighborsByCostClass(
      {num_neighbors, add_vehicle_starts_to_neighbors,
       add_vehicle_ends_to_neighbors,
       only_sort_neighbors_for_partial_neighborhoods});
}

const RoutingModel::NodeNeighborsByCostClass*
RoutingModel::GetOrCreateNodeNeighborsByCostClass(
    const NodeNeighborsParameters& params) {
  std::unique_ptr<NodeNeighborsByCostClass>& node_neighbors_by_cost_class =
      node_neighbors_by_cost_class_per_size_[params];
  if (node_neighbors_by_cost_class != nullptr) {
    return node_neighbors_by_cost_class.get();
  }
  node_neighbors_by_cost_class =
      std::make_unique<NodeNeighborsByCostClass>(this);
  node_neighbors_by_cost_class->ComputeNeighbors(params);
  return node_neighbors_by_cost_class.get();
}

namespace {

// Evaluators
template <class A, class B>
static int64_t ReturnZero(A, B) {
  return 0;
}

}  // namespace

// ----- Routing model -----

static const int kUnassigned = -1;
const int64_t RoutingModel::kNoPenalty = -1;

const RoutingModel::DisjunctionIndex RoutingModel::kNoDisjunction(-1);

const RoutingModel::DimensionIndex RoutingModel::kNoDimension(-1);

RoutingModel::RoutingModel(const RoutingIndexManager& index_manager)
    : RoutingModel(index_manager, DefaultRoutingModelParameters()) {}

namespace {
std::unique_ptr<Solver> CreateSolverFromParameters(
    const RoutingModelParameters& parameters) {
  VLOG(1) << "Model parameters:\n" << parameters;
  ConstraintSolverParameters solver_parameters =
      parameters.has_solver_parameters() ? parameters.solver_parameters()
                                         : Solver::DefaultSolverParameters();
  return std::make_unique<Solver>("Routing", solver_parameters);
}
}  // namespace

RoutingModel::RoutingModel(const RoutingIndexManager& index_manager,
                           const RoutingModelParameters& parameters)
    : solver_(CreateSolverFromParameters(parameters)),
      nodes_(index_manager.num_nodes()),
      vehicles_(index_manager.num_vehicles()),
      max_active_vehicles_(vehicles_),
      fixed_cost_of_vehicle_(vehicles_, 0),
      cost_class_index_of_vehicle_(vehicles_, CostClassIndex(-1)),
      linear_cost_factor_of_vehicle_(vehicles_, 0),
      quadratic_cost_factor_of_vehicle_(vehicles_, 0),
      vehicle_amortized_cost_factors_set_(false),
      vehicle_used_when_empty_(vehicles_, false),
      cost_classes_(),
      costs_are_homogeneous_across_vehicles_(
          parameters.reduce_vehicle_cost_model()),
      cache_callbacks_(false),
      vehicle_class_index_of_vehicle_(vehicles_, VehicleClassIndex(-1)),
      vehicle_pickup_delivery_policy_(vehicles_, PICKUP_AND_DELIVERY_NO_ORDER),
      num_visit_types_(0),
      paths_metadata_(index_manager),
      manager_(index_manager),
      search_parameters_(DefaultRoutingSearchParameters()),
      finalizer_variables_(std::make_unique<FinalizerVariables>(solver_.get())),
      interrupt_cp_sat_(false),
      interrupt_cp_(false) {
  // Initialize vehicle costs to the zero evaluator.
  vehicle_to_transit_cost_.assign(
      vehicles_, RegisterTransitCallback(
                     ReturnZero<int64_t, int64_t>,
                     RoutingModel::kTransitEvaluatorSignPositiveOrZero));
  // Active caching after initializing vehicle_to_transit_cost_ to avoid
  // uselessly caching ReturnZero.
  cache_callbacks_ = (nodes_ <= parameters.max_callback_cache_size());

  // TODO(user): Remove when removal of NodeIndex is complete.
  start_end_count_ = index_manager.num_unique_depots();
  Initialize();

  const int64_t size = Size();
  index_to_pickup_position_.resize(size);
  index_to_delivery_position_.resize(size);
  index_to_visit_type_.resize(index_manager.num_indices(), kUnassigned);
  index_to_type_policy_.resize(index_manager.num_indices());

  const std::vector<RoutingIndexManager::NodeIndex>& index_to_node =
      index_manager.GetIndexToNodeMap();
  index_to_equivalence_class_.resize(index_manager.num_indices());
  for (int i = 0; i < index_to_node.size(); ++i) {
    index_to_equivalence_class_[i] = index_to_node[i].value();
  }
  allowed_vehicles_.resize(Size() + vehicles_);
}

void RoutingModel::Initialize() {
  const int size = Size();
  // Next variables
  solver_->MakeIntVarArray(size, 0, size + vehicles_ - 1, "Nexts", &nexts_);
  solver_->AddConstraint(solver_->MakeAllDifferent(nexts_, false));
  index_to_disjunctions_.resize(size + vehicles_);
  // Vehicle variables. In case that node i is not active, vehicle_vars_[i] is
  // bound to -1.
  solver_->MakeIntVarArray(size + vehicles_, -1, vehicles_ - 1, "Vehicles",
                           &vehicle_vars_);
  // Active variables
  solver_->MakeBoolVarArray(size, "Active", &active_);
  // Active vehicle variables
  solver_->MakeBoolVarArray(vehicles_, "ActiveVehicle", &vehicle_active_);
  // Variables representing vehicles contributing to cost.
  solver_->MakeBoolVarArray(vehicles_, "VehicleCostsConsidered",
                            &vehicle_route_considered_);
  // Is-bound-to-end variables.
  solver_->MakeBoolVarArray(size + vehicles_, "IsBoundToEnd",
                            &is_bound_to_end_);
  // Cost cache
  cost_cache_.clear();
  cost_cache_.resize(size + vehicles_, {kUnassigned, CostClassIndex(-1), 0});
  preassignment_ = solver_->MakeAssignment();
}

RoutingModel::~RoutingModel() {
  gtl::STLDeleteElements(&dimensions_);

  // State dependent transit callbacks.
  absl::flat_hash_set<RangeIntToIntFunction*> value_functions_delete;
  absl::flat_hash_set<RangeMinMaxIndexFunction*> index_functions_delete;
  for (const auto& cache_line : state_dependent_transit_evaluators_cache_) {
    for (const auto& key_transit : *cache_line) {
      value_functions_delete.insert(key_transit.second.transit);
      index_functions_delete.insert(key_transit.second.transit_plus_identity);
    }
  }
  gtl::STLDeleteElements(&value_functions_delete);
  gtl::STLDeleteElements(&index_functions_delete);
}

int RoutingModel::RegisterUnaryTransitVector(std::vector<int64_t> values) {
  TransitEvaluatorSign sign = kTransitEvaluatorSignUnknown;
  if (std::all_of(std::cbegin(values), std::cend(values),
                  [](int64_t transit) { return transit >= 0; })) {
    sign = kTransitEvaluatorSignPositiveOrZero;
  } else if (std::all_of(std::cbegin(values), std::cend(values),
                         [](int64_t transit) { return transit <= 0; })) {
    sign = kTransitEvaluatorSignNegativeOrZero;
  }
  return RegisterUnaryTransitCallback(
      [this, values = std::move(values)](int64_t i) {
        return values[manager_.IndexToNode(i).value()];
      },
      sign);
}

int RoutingModel::RegisterUnaryTransitCallback(TransitCallback1 callback,
                                               TransitEvaluatorSign sign) {
  const int index = unary_transit_evaluators_.size();
  unary_transit_evaluators_.push_back(std::move(callback));
  return RegisterTransitCallback(
      [this, index](int i, int /*j*/) {
        return unary_transit_evaluators_[index](i);
      },
      sign);
}

int RoutingModel::RegisterTransitMatrix(
    std::vector<std::vector<int64_t> /*needed_for_swig*/> values) {
  // TODO(user): when we move away from std::functions, use a (potentially
  // vectorized) helper to compute the sign of a range.
  bool all_transits_geq_zero = true;
  bool all_transits_leq_zero = true;
  for (const std::vector<int64_t>& transit_values : values) {
    for (const int64_t value : transit_values) {
      all_transits_leq_zero &= value <= 0;
      all_transits_geq_zero &= value >= 0;
    }
    if (!all_transits_geq_zero && !all_transits_leq_zero) break;
  }
  const TransitEvaluatorSign sign =
      all_transits_geq_zero
          ? kTransitEvaluatorSignPositiveOrZero
          : (all_transits_leq_zero ? kTransitEvaluatorSignNegativeOrZero
                                   : kTransitEvaluatorSignUnknown);
  return RegisterTransitCallback(
      [this, values = std::move(values)](int64_t i, int64_t j) {
        return values[manager_.IndexToNode(i).value()]
                     [manager_.IndexToNode(j).value()];
      },
      sign);
}

int RoutingModel::RegisterTransitCallback(TransitCallback2 callback,
                                          TransitEvaluatorSign sign) {
  if (cache_callbacks_) {
    TransitEvaluatorSign actual_sign = sign;
    const int size = Size() + vehicles();
    std::vector<int64_t> cache(size * size, 0);
    bool all_transits_geq_zero = true;
    bool all_transits_leq_zero = true;
    for (int i = 0; i < size; ++i) {
      for (int j = 0; j < size; ++j) {
        const int64_t value = callback(i, j);
        cache[i * size + j] = value;
        all_transits_geq_zero &= value >= 0;
        all_transits_leq_zero &= value <= 0;
      }
    }
    actual_sign =
        all_transits_geq_zero
            ? kTransitEvaluatorSignPositiveOrZero
            : (all_transits_leq_zero ? kTransitEvaluatorSignNegativeOrZero
                                     : kTransitEvaluatorSignUnknown);
    transit_evaluators_.push_back(
        [cache = std::move(cache), size](int64_t i, int64_t j) {
          return cache[i * size + j];
        });
    DCHECK(sign == kTransitEvaluatorSignUnknown || actual_sign == sign);
  } else {
    transit_evaluators_.push_back(std::move(callback));
  }
  if (transit_evaluators_.size() != unary_transit_evaluators_.size()) {
    DCHECK_EQ(transit_evaluators_.size(), unary_transit_evaluators_.size() + 1);
    unary_transit_evaluators_.push_back(nullptr);
  }
  transit_evaluator_sign_.push_back(sign);
  return transit_evaluators_.size() - 1;
}

int RoutingModel::RegisterStateDependentTransitCallback(
    VariableIndexEvaluator2 callback) {
  state_dependent_transit_evaluators_cache_.push_back(
      std::make_unique<StateDependentTransitCallbackCache>());
  StateDependentTransitCallbackCache* const cache =
      state_dependent_transit_evaluators_cache_.back().get();
  state_dependent_transit_evaluators_.push_back(
      [cache, callback = std::move(callback)](int64_t i, int64_t j) {
        StateDependentTransit value;
        if (gtl::FindCopy(*cache, CacheKey(i, j), &value)) return value;
        value = callback(i, j);
        cache->insert({CacheKey(i, j), value});
        return value;
      });
  return state_dependent_transit_evaluators_.size() - 1;
}

int RoutingModel::RegisterCumulDependentTransitCallback(
    CumulDependentTransitCallback2 callback) {
  cumul_dependent_transit_evaluators_.push_back(std::move(callback));
  return cumul_dependent_transit_evaluators_.size() - 1;
}

void RoutingModel::AddNoCycleConstraintInternal() {
  if (no_cycle_constraint_ == nullptr) {
    no_cycle_constraint_ = solver_->MakeNoCycle(nexts_, active_);
    solver_->AddConstraint(no_cycle_constraint_);
  }
}

bool RoutingModel::AddDimension(int evaluator_index, int64_t slack_max,
                                int64_t capacity, bool fix_start_cumul_to_zero,
                                const std::string& name) {
  const std::vector<int> evaluator_indices(vehicles_, evaluator_index);
  std::vector<int64_t> capacities(vehicles_, capacity);
  return AddDimensionWithCapacityInternal(evaluator_indices, {}, slack_max,
                                          std::move(capacities),
                                          fix_start_cumul_to_zero, name);
}

bool RoutingModel::AddDimensionWithVehicleTransits(
    const std::vector<int>& evaluator_indices, int64_t slack_max,
    int64_t capacity, bool fix_start_cumul_to_zero, const std::string& name) {
  std::vector<int64_t> capacities(vehicles_, capacity);
  return AddDimensionWithCapacityInternal(evaluator_indices, {}, slack_max,
                                          std::move(capacities),
                                          fix_start_cumul_to_zero, name);
}

bool RoutingModel::AddDimensionWithVehicleCapacity(
    int evaluator_index, int64_t slack_max,
    std::vector<int64_t> vehicle_capacities, bool fix_start_cumul_to_zero,
    const std::string& name) {
  const std::vector<int> evaluator_indices(vehicles_, evaluator_index);
  return AddDimensionWithCapacityInternal(evaluator_indices, {}, slack_max,
                                          std::move(vehicle_capacities),
                                          fix_start_cumul_to_zero, name);
}

bool RoutingModel::AddDimensionWithVehicleTransitAndCapacity(
    const std::vector<int>& evaluator_indices, int64_t slack_max,
    std::vector<int64_t> vehicle_capacities, bool fix_start_cumul_to_zero,
    const std::string& name) {
  return AddDimensionWithCapacityInternal(evaluator_indices, {}, slack_max,
                                          std::move(vehicle_capacities),
                                          fix_start_cumul_to_zero, name);
}

bool RoutingModel::AddDimensionWithCumulDependentVehicleTransitAndCapacity(
    const std::vector<int>& fixed_evaluator_indices,
    const std::vector<int>& cumul_dependent_evaluator_indices,
    int64_t slack_max, std::vector<int64_t> vehicle_capacities,
    bool fix_start_cumul_to_zero, const std::string& name) {
  return AddDimensionWithCapacityInternal(
      fixed_evaluator_indices, cumul_dependent_evaluator_indices, slack_max,
      std::move(vehicle_capacities), fix_start_cumul_to_zero, name);
}

bool RoutingModel::AddDimensionWithCapacityInternal(
    const std::vector<int>& evaluator_indices,
    const std::vector<int>& cumul_dependent_evaluator_indices,
    int64_t slack_max, std::vector<int64_t> vehicle_capacities,
    bool fix_start_cumul_to_zero, const std::string& name) {
  CHECK_EQ(vehicles_, vehicle_capacities.size());
  return InitializeDimensionInternal(
      evaluator_indices, cumul_dependent_evaluator_indices,
      /*state_dependent_evaluator_indices=*/{}, slack_max,
      fix_start_cumul_to_zero,
      new RoutingDimension(this, std::move(vehicle_capacities), name, nullptr));
}

bool RoutingModel::InitializeDimensionInternal(
    const std::vector<int>& evaluator_indices,
    const std::vector<int>& cumul_dependent_evaluator_indices,
    const std::vector<int>& state_dependent_evaluator_indices,
    int64_t slack_max, bool fix_start_cumul_to_zero,
    RoutingDimension* dimension) {
  DCHECK(dimension != nullptr);
  DCHECK_EQ(vehicles_, evaluator_indices.size());
  DCHECK((dimension->base_dimension_ == nullptr &&
          state_dependent_evaluator_indices.empty()) ||
         vehicles_ == state_dependent_evaluator_indices.size());
  if (!HasDimension(dimension->name())) {
    DCHECK_EQ(dimensions_.size(), dimension->index().value());
    dimension_name_to_index_[dimension->name()] = dimension->index();
    dimensions_.push_back(dimension);
    dimension->Initialize(evaluator_indices, cumul_dependent_evaluator_indices,
                          state_dependent_evaluator_indices, slack_max);
    solver_->AddConstraint(solver_->MakeDelayedPathCumul(
        nexts_, active_, dimension->cumuls(), dimension->transits()));
    if (fix_start_cumul_to_zero) {
      for (int i = 0; i < vehicles_; ++i) {
        IntVar* const start_cumul = dimension->CumulVar(Start(i));
        CHECK_EQ(0, start_cumul->Min());
        start_cumul->SetValue(0);
      }
    }
    return true;
  }
  delete dimension;
  return false;
}

std::pair<int, bool> RoutingModel::AddConstantDimensionWithSlack(
    int64_t value, int64_t capacity, int64_t slack_max,
    bool fix_start_cumul_to_zero, const std::string& dimension_name) {
  const TransitEvaluatorSign sign = value < 0
                                        ? kTransitEvaluatorSignNegativeOrZero
                                        : kTransitEvaluatorSignPositiveOrZero;
  const int evaluator_index =
      RegisterUnaryTransitCallback([value](int64_t) { return value; }, sign);
  return std::make_pair(evaluator_index,
                        AddDimension(evaluator_index, slack_max, capacity,
                                     fix_start_cumul_to_zero, dimension_name));
}

std::pair<int, bool> RoutingModel::AddVectorDimension(
    std::vector<int64_t> values, int64_t capacity, bool fix_start_cumul_to_zero,
    const std::string& dimension_name) {
  const int evaluator_index = RegisterUnaryTransitVector(std::move(values));
  return std::make_pair(evaluator_index,
                        AddDimension(evaluator_index, 0, capacity,
                                     fix_start_cumul_to_zero, dimension_name));
}

std::pair<int, bool> RoutingModel::AddMatrixDimension(
    std::vector<std::vector<int64_t>> values, int64_t capacity,
    bool fix_start_cumul_to_zero, const std::string& dimension_name) {
  const int evaluator_index = RegisterTransitMatrix(std::move(values));
  return std::make_pair(evaluator_index,
                        AddDimension(evaluator_index, 0, capacity,
                                     fix_start_cumul_to_zero, dimension_name));
}

namespace {
// RangeMakeElementExpr is an IntExpr that corresponds to a
// RangeIntToIntFunction indexed by an IntVar.
// Do not create this class dicretly, but rather use MakeRangeMakeElementExpr.
class RangeMakeElementExpr : public BaseIntExpr {
 public:
  RangeMakeElementExpr(const RangeIntToIntFunction* callback, IntVar* index,
                       Solver* s)
      : BaseIntExpr(s), callback_(ABSL_DIE_IF_NULL(callback)), index_(index) {
    CHECK(callback_ != nullptr);
    CHECK(index != nullptr);
  }

  int64_t Min() const override {
    // Converting [index_->Min(), index_->Max()] to [idx_min, idx_max).
    const int idx_min = index_->Min();
    const int idx_max = index_->Max() + 1;
    return (idx_min < idx_max) ? callback_->RangeMin(idx_min, idx_max)
                               : std::numeric_limits<int64_t>::max();
  }
  void SetMin(int64_t new_min) override {
    const int64_t old_min = Min();
    const int64_t old_max = Max();
    if (old_min < new_min && new_min <= old_max) {
      const int64_t old_idx_min = index_->Min();
      const int64_t old_idx_max = index_->Max() + 1;
      if (old_idx_min < old_idx_max) {
        const int64_t new_idx_min = callback_->RangeFirstInsideInterval(
            old_idx_min, old_idx_max, new_min, old_max + 1);
        index_->SetMin(new_idx_min);
        if (new_idx_min < old_idx_max) {
          const int64_t new_idx_max = callback_->RangeLastInsideInterval(
              new_idx_min, old_idx_max, new_min, old_max + 1);
          index_->SetMax(new_idx_max);
        }
      }
    }
  }
  int64_t Max() const override {
    // Converting [index_->Min(), index_->Max()] to [idx_min, idx_max).
    const int idx_min = index_->Min();
    const int idx_max = index_->Max() + 1;
    return (idx_min < idx_max) ? callback_->RangeMax(idx_min, idx_max)
                               : std::numeric_limits<int64_t>::min();
  }
  void SetMax(int64_t new_max) override {
    const int64_t old_min = Min();
    const int64_t old_max = Max();
    if (old_min <= new_max && new_max < old_max) {
      const int64_t old_idx_min = index_->Min();
      const int64_t old_idx_max = index_->Max() + 1;
      if (old_idx_min < old_idx_max) {
        const int64_t new_idx_min = callback_->RangeFirstInsideInterval(
            old_idx_min, old_idx_max, old_min, new_max + 1);
        index_->SetMin(new_idx_min);
        if (new_idx_min < old_idx_max) {
          const int64_t new_idx_max = callback_->RangeLastInsideInterval(
              new_idx_min, old_idx_max, old_min, new_max + 1);
          index_->SetMax(new_idx_max);
        }
      }
    }
  }
  void WhenRange(Demon* d) override { index_->WhenRange(d); }

 private:
  const RangeIntToIntFunction* const callback_;
  IntVar* const index_;
};

IntExpr* MakeRangeMakeElementExpr(const RangeIntToIntFunction* callback,
                                  IntVar* index, Solver* s) {
  return s->RegisterIntExpr(
      s->RevAlloc(new RangeMakeElementExpr(callback, index, s)));
}
}  // namespace

bool RoutingModel::AddDimensionDependentDimensionWithVehicleCapacity(
    const std::vector<int>& dependent_transits,
    const RoutingDimension* base_dimension, int64_t slack_max,
    std::vector<int64_t> vehicle_capacities, bool fix_start_cumul_to_zero,
    const std::string& name) {
  const std::vector<int> pure_transits(vehicles_, /*zero_evaluator*/ 0);
  return AddDimensionDependentDimensionWithVehicleCapacity(
      pure_transits, dependent_transits, base_dimension, slack_max,
      std::move(vehicle_capacities), fix_start_cumul_to_zero, name);
}

bool RoutingModel::AddDimensionDependentDimensionWithVehicleCapacity(
    int transit, const RoutingDimension* dimension, int64_t slack_max,
    int64_t vehicle_capacity, bool fix_start_cumul_to_zero,
    const std::string& name) {
  return AddDimensionDependentDimensionWithVehicleCapacity(
      /*zero_evaluator*/ 0, transit, dimension, slack_max, vehicle_capacity,
      fix_start_cumul_to_zero, name);
}

bool RoutingModel::AddDimensionDependentDimensionWithVehicleCapacityInternal(
    const std::vector<int>& pure_transits,
    const std::vector<int>& dependent_transits,
    const RoutingDimension* base_dimension, int64_t slack_max,
    std::vector<int64_t> vehicle_capacities, bool fix_start_cumul_to_zero,
    const std::string& name) {
  CHECK_EQ(vehicles_, vehicle_capacities.size());
  RoutingDimension* new_dimension = nullptr;
  if (base_dimension == nullptr) {
    new_dimension = new RoutingDimension(this, std::move(vehicle_capacities),
                                         name, RoutingDimension::SelfBased());
  } else {
    new_dimension = new RoutingDimension(this, std::move(vehicle_capacities),
                                         name, base_dimension);
  }
  return InitializeDimensionInternal(pure_transits,
                                     /*cumul_dependent_evaluator_indices=*/{},
                                     dependent_transits, slack_max,
                                     fix_start_cumul_to_zero, new_dimension);
}

bool RoutingModel::AddDimensionDependentDimensionWithVehicleCapacity(
    int pure_transit, int dependent_transit,
    const RoutingDimension* base_dimension, int64_t slack_max,
    int64_t vehicle_capacity, bool fix_start_cumul_to_zero,
    const std::string& name) {
  std::vector<int> pure_transits(vehicles_, pure_transit);
  std::vector<int> dependent_transits(vehicles_, dependent_transit);
  std::vector<int64_t> vehicle_capacities(vehicles_, vehicle_capacity);
  return AddDimensionDependentDimensionWithVehicleCapacityInternal(
      pure_transits, dependent_transits, base_dimension, slack_max,
      std::move(vehicle_capacities), fix_start_cumul_to_zero, name);
}

RoutingModel::StateDependentTransit RoutingModel::MakeStateDependentTransit(
    const std::function<int64_t(int64_t)>& f, int64_t domain_start,
    int64_t domain_end) {
  const std::function<int64_t(int64_t)> g = [&f](int64_t x) {
    return f(x) + x;
  };
  // The next line is safe, because MakeCachedIntToIntFunction does not count
  // on keeping the closure of its first argument alive.
  return {MakeCachedIntToIntFunction(f, domain_start, domain_end),
          MakeCachedRangeMinMaxIndexFunction(g, domain_start, domain_end)};
}

std::vector<std::string> RoutingModel::GetAllDimensionNames() const {
  std::vector<std::string> dimension_names;
  for (const auto& dimension_name_index : dimension_name_to_index_) {
    dimension_names.push_back(dimension_name_index.first);
  }
  std::sort(dimension_names.begin(), dimension_names.end());
  return dimension_names;
}

GlobalDimensionCumulOptimizer* RoutingModel::GetMutableGlobalCumulLPOptimizer(
    const RoutingDimension& dimension) const {
  const int optimizer_index = GetGlobalCumulOptimizerIndex(dimension);
  return optimizer_index < 0
             ? nullptr
             : global_dimension_optimizers_[optimizer_index].lp_optimizer.get();
}

GlobalDimensionCumulOptimizer* RoutingModel::GetMutableGlobalCumulMPOptimizer(
    const RoutingDimension& dimension) const {
  const int optimizer_index = GetGlobalCumulOptimizerIndex(dimension);
  return optimizer_index < 0
             ? nullptr
             : global_dimension_optimizers_[optimizer_index].mp_optimizer.get();
}

int RoutingModel::GetGlobalCumulOptimizerIndex(
    const RoutingDimension& dimension) const {
  DCHECK(closed_);
  const DimensionIndex dim_index = dimension.index();
  if (dim_index < 0 || dim_index >= global_optimizer_index_.size() ||
      global_optimizer_index_[dim_index] < 0) {
    return -1;
  }
  const int optimizer_index = global_optimizer_index_[dim_index];
  DCHECK_LT(optimizer_index, global_dimension_optimizers_.size());
  return optimizer_index;
}

LocalDimensionCumulOptimizer* RoutingModel::GetMutableLocalCumulLPOptimizer(
    const RoutingDimension& dimension) const {
  const int optimizer_index = GetLocalCumulOptimizerIndex(dimension);
  return optimizer_index < 0
             ? nullptr
             : local_dimension_optimizers_[optimizer_index].lp_optimizer.get();
}

LocalDimensionCumulOptimizer* RoutingModel::GetMutableLocalCumulMPOptimizer(
    const RoutingDimension& dimension) const {
  const int optimizer_index = GetLocalCumulOptimizerIndex(dimension);
  return optimizer_index < 0
             ? nullptr
             : local_dimension_optimizers_[optimizer_index].mp_optimizer.get();
}

int RoutingModel::GetLocalCumulOptimizerIndex(
    const RoutingDimension& dimension) const {
  DCHECK(closed_);
  const DimensionIndex dim_index = dimension.index();
  if (dim_index < 0 || dim_index >= local_optimizer_index_.size() ||
      local_optimizer_index_[dim_index] < 0) {
    return -1;
  }
  const int optimizer_index = local_optimizer_index_[dim_index];
  DCHECK_LT(optimizer_index, local_dimension_optimizers_.size());
  return optimizer_index;
}

bool RoutingModel::HasDimension(absl::string_view dimension_name) const {
  return dimension_name_to_index_.contains(dimension_name);
}

RoutingModel::DimensionIndex RoutingModel::GetDimensionIndex(
    absl::string_view dimension_name) const {
  return gtl::FindWithDefault(dimension_name_to_index_, dimension_name,
                              kNoDimension);
}

const RoutingDimension& RoutingModel::GetDimensionOrDie(
    const std::string& dimension_name) const {
  return *dimensions_[gtl::FindOrDie(dimension_name_to_index_, dimension_name)];
}

RoutingDimension* RoutingModel::GetMutableDimension(
    const std::string& dimension_name) const {
  const DimensionIndex index = GetDimensionIndex(dimension_name);
  if (index != kNoDimension) {
    return dimensions_[index];
  }
  return nullptr;
}

// ResourceGroup
namespace {
using ResourceGroup = RoutingModel::ResourceGroup;
}  // namespace

ResourceGroup::Attributes::Attributes()
    : start_domain_(Domain::AllValues()), end_domain_(Domain::AllValues()) {
  /// The default attributes have unconstrained start/end domains.
}

ResourceGroup::Attributes::Attributes(Domain start_domain, Domain end_domain)
    : start_domain_(std::move(start_domain)),
      end_domain_(std::move(end_domain)) {}

void ResourceGroup::Resource::SetDimensionAttributes(
    Attributes attributes, const RoutingDimension* dimension) {
  DCHECK(attributes_.empty())
      << "As of 2021/07, each resource can only constrain a single dimension.";

  const DimensionIndex dimension_index = dimension->index();
  DCHECK(!dimension_attributes_.contains(dimension_index));
  const int attribute_index = attributes_.size();
  dimension_attributes_[dimension_index] = attribute_index;
  if (dimension_index >= dimension_attributes_per_index_.size()) {
    dimension_attributes_per_index_.resize(dimension_index.value() + 1);
  }
  dimension_attributes_per_index_[dimension_index] = attribute_index;
  attributes_.push_back(std::move(attributes));
}

const ResourceGroup::Attributes& ResourceGroup::Resource::GetDefaultAttributes()
    const {
  static const Attributes* const kAttributes = new Attributes();
  return *kAttributes;
}

ResourceGroup* RoutingModel::AddResourceGroup() {
  DCHECK_EQ(resource_groups_.size(), resource_vars_.size());
  // Create and add the resource group.
  // Using 'new' to access private constructor.
  resource_groups_.push_back(absl::WrapUnique(new ResourceGroup(this)));
  const int rg_index = resource_groups_.back()->Index();
  DCHECK_EQ(rg_index, resource_groups_.size() - 1);

  // Create and add the resource vars (the proper variable bounds and
  // constraints are set up when closing the model).
  resource_vars_.push_back({});
  solver_->MakeIntVarArray(vehicles(), -1, std::numeric_limits<int64_t>::max(),
                           absl::StrCat("Resources[", rg_index, "]"),
                           &resource_vars_.back());

  return resource_groups_[rg_index].get();
}

int ResourceGroup::AddResource(Attributes attributes,
                               const RoutingDimension* dimension) {
  resources_.push_back(Resource(model_));
  resources_.back().SetDimensionAttributes(std::move(attributes), dimension);

  affected_dimension_indices_.insert(dimension->index());

  DCHECK_EQ(affected_dimension_indices_.size(), 1)
      << "As of 2021/07, each ResourceGroup can only affect a single "
         "RoutingDimension at a time.";

  return resources_.size() - 1;
}

void ResourceGroup::NotifyVehicleRequiresAResource(int vehicle) {
  DCHECK_LT(vehicle, vehicle_requires_resource_.size());
  if (vehicle_requires_resource_[vehicle]) return;
  vehicle_requires_resource_[vehicle] = true;
  vehicles_requiring_resource_.push_back(vehicle);
}

namespace {
struct ResourceClass {
  using DimensionIndex = RoutingModel::DimensionIndex;
  /// The attributes for each dimension.
  util_intops::StrongVector<DimensionIndex, ResourceGroup::Attributes>
      dimension_attributes;
  /// Assignability of vehicles.
  std::vector<bool> assignable_to_vehicle;

  // Make ResourceClass absl::Hash-able.
  friend bool operator==(const ResourceClass& c1, const ResourceClass& c2) {
    return c1.dimension_attributes == c2.dimension_attributes &&
           c1.assignable_to_vehicle == c2.assignable_to_vehicle;
  }
  template <typename H>
  friend H AbslHashValue(H h, const ResourceClass& c) {
    return H::combine(std::move(h), c.dimension_attributes,
                      c.assignable_to_vehicle);
  }
};
}  // namespace

void ResourceGroup::ComputeResourceClasses() {
  resource_class_indices_.assign(resources_.size(), ResourceClassIndex(-1));
  resource_indices_per_class_.clear();

  absl::flat_hash_map<ResourceClass, ResourceClassIndex> resource_class_map;
  for (int r = 0; r < resources_.size(); ++r) {
    ResourceClass resource_class;

    util_intops::StrongVector<DimensionIndex, Attributes>& dim_attributes =
        resource_class.dimension_attributes;
    dim_attributes.resize(model_->dimensions_.size(), Attributes());
    for (const auto& [dim_index, attributes] :
         resources_[r].dimension_attributes_) {
      dim_attributes[dim_index] = resources_[r].attributes_[attributes];
    }

    std::vector<bool>& assignable_to_v = resource_class.assignable_to_vehicle;
    assignable_to_v.resize(model_->vehicles_, false);
    for (int v : vehicles_requiring_resource_) {
      assignable_to_v[v] = IsResourceAllowedForVehicle(r, v) &&
                           model_->ResourceVar(v, index_)->Contains(r);
    }

    DCHECK_EQ(resource_indices_per_class_.size(), resource_class_map.size());
    const ResourceClassIndex num_resource_classes(resource_class_map.size());
    ResourceClassIndex& resource_class_index = resource_class_indices_[r];
    resource_class_index = gtl::LookupOrInsert(
        &resource_class_map, resource_class, num_resource_classes);
    if (resource_class_index == num_resource_classes) {
      // New resource class.
      resource_indices_per_class_.push_back({});
    }
    resource_indices_per_class_[resource_class_index].push_back(r);
  }
}

const std::vector<int>& RoutingModel::GetDimensionResourceGroupIndices(
    const RoutingDimension* dimension) const {
  DCHECK(closed_);
  return dimension_resource_group_indices_[dimension->index()];
}

RoutingModel::SecondaryOptimizer::SecondaryOptimizer(
    RoutingModel* model, RoutingSearchParameters* search_parameters,
    int64_t solve_period)
    : model_(model),
      search_parameters_(search_parameters),
      solve_period_(solve_period) {
  DCHECK(model_ != nullptr);
  state_ = model_->solver()->MakeAssignment();
  Assignment::IntContainer* container = state_->MutableIntVarContainer();
  const std::vector<IntVar*> nexts = model_->Nexts();
  container->Resize(nexts.size());
  for (int i = 0; i < nexts.size(); ++i) {
    IntVar* next_var = nexts[i];
    container->AddAtPosition(next_var, i)->SetValue(i);
    var_to_index_[next_var] = i;
  }
  IntVar* cost = (model_->CostVar() != nullptr)
                     ? model_->CostVar()
                     : model_->solver()->MakeIntConst(0);
  state_->AddObjective(cost);
}

bool RoutingModel::SecondaryOptimizer::Solve(
    const std::vector<RoutingModel::VariableValuePair>& in_state,
    std::vector<RoutingModel::VariableValuePair>* out_state) {
  if (solve_period_ <= 0) return false;
  if (call_count_ == solve_period_) {
    call_count_ = 0;
  } else {
    call_count_++;
  }
  out_state->clear();
  Assignment::IntContainer* container = state_->MutableIntVarContainer();
  for (const auto [var, value] : in_state) {
    container->MutableElement(var)->SetValue(value);
  }
  if (call_count_ != 0) return false;
  absl::flat_hash_set<IntVar*> touched;
  const Assignment* solution = model_->FastSolveFromAssignmentWithParameters(
      state_, *search_parameters_,
      /*check_solution_in_cp=*/false, &touched);
  if (solution == nullptr || touched.empty()) return false;
  for (IntVar* var : touched) {
    const int index = var_to_index_[var];
    const int64_t value = solution->Value(var);
    out_state->push_back({index, value});
    container->MutableElement(index)->SetValue(value);
  }
  return true;
}

void RoutingModel::SetArcCostEvaluatorOfAllVehicles(int evaluator_index) {
  CHECK_LT(0, vehicles_);
  for (int i = 0; i < vehicles_; ++i) {
    SetArcCostEvaluatorOfVehicle(evaluator_index, i);
  }
}

void RoutingModel::SetArcCostEvaluatorOfVehicle(int evaluator_index,
                                                int vehicle) {
  CHECK_LT(vehicle, vehicles_);
  CHECK_LT(evaluator_index, transit_evaluators_.size());
  vehicle_to_transit_cost_[vehicle] = evaluator_index;
}

void RoutingModel::SetFixedCostOfAllVehicles(int64_t cost) {
  for (int i = 0; i < vehicles_; ++i) {
    SetFixedCostOfVehicle(cost, i);
  }
}

int64_t RoutingModel::GetFixedCostOfVehicle(int vehicle) const {
  CHECK_LT(vehicle, vehicles_);
  return fixed_cost_of_vehicle_[vehicle];
}

void RoutingModel::SetFixedCostOfVehicle(int64_t cost, int vehicle) {
  CHECK_LT(vehicle, vehicles_);
  DCHECK_GE(cost, 0);
  fixed_cost_of_vehicle_[vehicle] = cost;
}

void RoutingModel::SetPathEnergyCostOfVehicle(const std::string& force,
                                              const std::string& distance,
                                              int64_t cost_per_unit,
                                              int vehicle) {
  SetPathEnergyCostsOfVehicle(force, distance, /*threshold=*/0,
                              /*cost_per_unit_below_threshold=*/0,
                              /*cost_per_unit_above_threshold=*/cost_per_unit,
                              vehicle);
}

void RoutingModel::SetPathEnergyCostsOfVehicle(
    const std::string& force, const std::string& distance, int64_t threshold,
    int64_t cost_per_unit_below_threshold,
    int64_t cost_per_unit_above_threshold, int vehicle) {
  DCHECK_LE(0, vehicle);
  DCHECK_LT(vehicle, vehicles_);
  DCHECK_LE(0, threshold);
  DCHECK_LE(0, cost_per_unit_below_threshold);
  DCHECK_LE(0, cost_per_unit_above_threshold);
  // When costs are 0, we can avoid useless computations.
  if (cost_per_unit_below_threshold == 0 && cost_per_unit_above_threshold == 0)
    return;
  using Limit = Solver::PathEnergyCostConstraintSpecification::EnergyCost;
  std::vector<Limit>& energy_costs =
      force_distance_to_energy_costs_[std::make_pair(force, distance)];
  if (energy_costs.size() < vehicles_) {
    energy_costs.resize(vehicles_, {0, 0, 0});
  }
  energy_costs[vehicle] = {
      .threshold = threshold,
      .cost_per_unit_below_threshold = cost_per_unit_below_threshold,
      .cost_per_unit_above_threshold = cost_per_unit_above_threshold};
}

void RoutingModel::SetAmortizedCostFactorsOfAllVehicles(
    int64_t linear_cost_factor, int64_t quadratic_cost_factor) {
  for (int v = 0; v < vehicles_; v++) {
    SetAmortizedCostFactorsOfVehicle(linear_cost_factor, quadratic_cost_factor,
                                     v);
  }
}

void RoutingModel::SetAmortizedCostFactorsOfVehicle(
    int64_t linear_cost_factor, int64_t quadratic_cost_factor, int vehicle) {
  CHECK_LT(vehicle, vehicles_);
  DCHECK_GE(linear_cost_factor, 0);
  DCHECK_GE(quadratic_cost_factor, 0);
  if (linear_cost_factor + quadratic_cost_factor > 0) {
    vehicle_amortized_cost_factors_set_ = true;
  }
  linear_cost_factor_of_vehicle_[vehicle] = linear_cost_factor;
  quadratic_cost_factor_of_vehicle_[vehicle] = quadratic_cost_factor;
}

void RoutingModel::AddRouteConstraint(
    absl::AnyInvocable<std::optional<int64_t>(const std::vector<int64_t>&)>
        route_evaluator,
    bool costs_are_homogeneous_across_vehicles) {
  costs_are_homogeneous_across_vehicles_ &=
      costs_are_homogeneous_across_vehicles;
  route_evaluators_.push_back(std::move(route_evaluator));
}

void RoutingModel::FinalizeAllowedVehicles() {
  const std::vector<RoutingDimension*> unary_dimensions = GetUnaryDimensions();

  // For each dimension, find the range of possible total transits.
  // This is precomputed to heuristically avoid a linear test on all vehicles.
  struct TransitBounds {
    int64_t min = std::numeric_limits<int64_t>::max();
    int64_t max = std::numeric_limits<int64_t>::min();
  };
  std::vector<TransitBounds> dimension_bounds(unary_dimensions.size());
  for (int d = 0; d < unary_dimensions.size(); ++d) {
    TransitBounds transit_bounds;
    const RoutingDimension* dimension = unary_dimensions[d];
    for (const int e : dimension->class_evaluators_) {
      const TransitCallback1& evaluator = UnaryTransitCallbackOrNull(e);
      DCHECK(evaluator != nullptr);
      for (int node = 0; node < Size(); ++node) {
        if (IsStart(node)) continue;
        const int64_t transit = evaluator(node);
        const IntVar* slack = dimension->SlackVar(node);
        transit_bounds = {
            .min = std::min(transit_bounds.min, CapAdd(transit, slack->Min())),
            .max = std::max(transit_bounds.max, CapAdd(transit, slack->Max()))};
      }
    }
    dimension_bounds[d] = transit_bounds;
  }

  // For each vehicle-node pair, find whether a dimension constraint forbids
  // assigning the pair.
  for (int vehicle = 0; vehicle < vehicles_; ++vehicle) {
    if (CheckLimit()) return;
    for (int d = 0; d < unary_dimensions.size(); ++d) {
      const RoutingDimension* const dim = unary_dimensions[d];
      const TransitCallback1& transit_evaluator =
          dim->GetUnaryTransitEvaluator(vehicle);
      DCHECK(transit_evaluator != nullptr);
      const TransitBounds allowed_transits = {
          .min = CapOpp(dim->vehicle_capacities()[vehicle]),
          .max = dim->vehicle_capacities()[vehicle],
      };
      // If the transit range over all nodes is within the vehicle's max
      // allowed variation, no need to scan all nodes: always keep the vehicle.
      if (allowed_transits.min <= dimension_bounds[d].min &&
          dimension_bounds[d].max <= allowed_transits.max) {
        continue;
      }
      for (int node = 0; node < Size(); ++node) {
        if (IsStart(node)) continue;
        absl::flat_hash_set<int>& allowed_vehicles = allowed_vehicles_[node];
        // NOTE: An empty set of "allowed_vehicles" actually means all
        // vehicles are allowed for this node, so we lazily fill
        // "allowed_vehicles" to [-1, num_vehicles) when removing a vehicle.

        // The vehicle is already forbidden for this node.
        if (!allowed_vehicles.empty() && !allowed_vehicles.contains(vehicle)) {
          continue;
        }
        // If the transit is within the allowed range, we can keep the vehicle.
        const int64_t transit = transit_evaluator(node);
        const IntVar* slack_var = dim->SlackVar(node);
        if (allowed_transits.min <= CapAdd(transit, slack_var->Max()) &&
            CapAdd(transit, slack_var->Min()) <= allowed_transits.max) {
          continue;
        }
        // We will remove the vehicle, lazy fill.
        if (allowed_vehicles.empty()) {
          allowed_vehicles.reserve(vehicles_);
          for (int v = 0; v < vehicles_; ++v) allowed_vehicles.insert(v);
        }
        allowed_vehicles.erase(vehicle);
        if (allowed_vehicles.empty()) {
          // If after erasing 'vehicle', allowed_vehicles becomes empty, it
          // means no vehicle is allowed for this node, so we insert the value
          // -1 in allowed_vehicles to distinguish with an empty
          // allowed_vehicles which actually means all vehicles allowed.
          allowed_vehicles.insert(-1);
        }
      }
    }
  }
}

// static
const RoutingModel::CostClassIndex RoutingModel::kCostClassIndexOfZeroCost =
    CostClassIndex(0);

void RoutingModel::ComputeCostClasses(
    const RoutingSearchParameters& /*parameters*/) {
  // Create and reduce the cost classes.
  cost_classes_.reserve(vehicles_);
  cost_classes_.clear();
  cost_class_index_of_vehicle_.assign(vehicles_, CostClassIndex(-1));
  absl::flat_hash_map<CostClass, CostClassIndex> cost_class_map;
  // Pre-insert the built-in cost class 'zero cost' with index 0.
  const CostClass zero_cost_class(0);
  cost_classes_.push_back(zero_cost_class);
  DCHECK_EQ(cost_classes_[kCostClassIndexOfZeroCost].evaluator_index, 0);
  cost_class_map[zero_cost_class] = kCostClassIndexOfZeroCost;

  // Determine the canonicalized cost class for each vehicle, and insert it as
  // a new cost class if it doesn't exist already. Building cached evaluators
  // on the way.
  has_vehicle_with_zero_cost_class_ = false;
  for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
    CostClass cost_class(vehicle_to_transit_cost_[vehicle]);

    // Insert the dimension data in a canonical way.
    for (const RoutingDimension* const dimension : dimensions_) {
      const int64_t span_coeff =
          dimension->vehicle_span_cost_coefficients()[vehicle];
      const int64_t slack_coeff =
          dimension->vehicle_slack_cost_coefficients()[vehicle];
      if (span_coeff == 0 && slack_coeff == 0) continue;
      cost_class.dimension_transit_evaluator_class_and_cost_coefficient
          .push_back({dimension->vehicle_to_class(vehicle), span_coeff,
                      slack_coeff, dimension});
    }
    absl::c_sort(
        cost_class.dimension_transit_evaluator_class_and_cost_coefficient);
    // Try inserting the CostClass, if it's not already present.
    const CostClassIndex num_cost_classes(cost_classes_.size());
    const CostClassIndex cost_class_index =
        gtl::LookupOrInsert(&cost_class_map, cost_class, num_cost_classes);
    if (cost_class_index == kCostClassIndexOfZeroCost) {
      has_vehicle_with_zero_cost_class_ = true;
    } else if (cost_class_index == num_cost_classes) {  // New cost class.
      cost_classes_.push_back(std::move(cost_class));
    }
    cost_class_index_of_vehicle_[vehicle] = cost_class_index;
  }

  // TRICKY:
  // If some vehicle had the "zero" cost class, then we'll have homogeneous
  // vehicles iff they all have that cost class (i.e. cost class count = 1).
  // If none of them have it, then we have homogeneous costs iff there are two
  // cost classes: the unused "zero" cost class and the one used by all
  // vehicles.
  // Note that we always need the zero cost class, even if no vehicle uses it,
  // because we use it in the vehicle_var = -1 scenario (i.e. unperformed).
  //
  // Fixed costs are simply ignored for computing these cost classes. They are
  // attached to start nodes directly.
  costs_are_homogeneous_across_vehicles_ &= has_vehicle_with_zero_cost_class_
                                                ? GetCostClassesCount() == 1
                                                : GetCostClassesCount() <= 2;
}

namespace {

struct VehicleClass {
  using DimensionIndex = RoutingModel::DimensionIndex;
  /// The cost class of the vehicle.
  RoutingModel::CostClassIndex cost_class_index;
  /// Contrarily to CostClass, here we need strict equivalence.
  int64_t fixed_cost;
  /// Whether or not the vehicle is used when empty.
  bool used_when_empty;
  /// Vehicle start and end equivalence classes. Currently if two vehicles
  /// have different start/end nodes which are "physically" located at the
  /// same place, these two vehicles will be considered as non-equivalent
  /// unless the two indices are in the same class.
  // TODO(user): Find equivalent start/end nodes wrt dimensions and
  // callbacks.
  int start_equivalence_class;
  int end_equivalence_class;
  /// Bounds of cumul variables at start and end vehicle nodes.
  /// dimension_{start,end}_cumuls_{min,max}[d] is the bound for dimension d.
  util_intops::StrongVector<DimensionIndex, int64_t> dimension_start_cumuls_min;
  util_intops::StrongVector<DimensionIndex, int64_t> dimension_start_cumuls_max;
  util_intops::StrongVector<DimensionIndex, int64_t> dimension_end_cumuls_min;
  util_intops::StrongVector<DimensionIndex, int64_t> dimension_end_cumuls_max;
  util_intops::StrongVector<DimensionIndex, int64_t> dimension_capacities;
  /// dimension_evaluators[d]->Run(from, to) is the fixed transit value of arc
  /// from->to for a dimension d.
  util_intops::StrongVector<DimensionIndex, int64_t>
      dimension_evaluator_classes;
  /// Same as above but for the cumul-dependent transit evaluators, if the
  /// dimension has any.
  util_intops::StrongVector<DimensionIndex, int64_t>
      cumul_dependent_dimension_evaluator_classes;
  /// Hash of the visitability of (non-start/end) nodes.
  uint64_t visitable_nodes_hash;
  /// Hash of allowed resources for each resource group, or -1 if a given
  /// resource group isn't required by the vehicle.
  std::vector<int64_t> group_allowed_resources_hash;

  friend bool operator==(const VehicleClass& c1, const VehicleClass& c2) {
    return c1.cost_class_index == c2.cost_class_index &&
           c1.fixed_cost == c2.fixed_cost &&
           c1.used_when_empty == c2.used_when_empty &&
           c1.start_equivalence_class == c2.start_equivalence_class &&
           c1.end_equivalence_class == c2.end_equivalence_class &&
           c1.dimension_start_cumuls_min == c2.dimension_start_cumuls_min &&
           c1.dimension_start_cumuls_max == c2.dimension_start_cumuls_max &&
           c1.dimension_end_cumuls_min == c2.dimension_end_cumuls_min &&
           c1.dimension_end_cumuls_max == c2.dimension_end_cumuls_max &&
           c1.dimension_capacities == c2.dimension_capacities &&
           c1.dimension_evaluator_classes == c2.dimension_evaluator_classes &&
           c1.cumul_dependent_dimension_evaluator_classes ==
               c2.cumul_dependent_dimension_evaluator_classes &&
           c1.visitable_nodes_hash == c2.visitable_nodes_hash &&
           c1.group_allowed_resources_hash == c2.group_allowed_resources_hash;
  }
  template <typename H>
  friend H AbslHashValue(H h, const VehicleClass& c) {
    return H::combine(std::move(h), c.cost_class_index, c.fixed_cost,
                      c.used_when_empty, c.start_equivalence_class,
                      c.end_equivalence_class, c.dimension_start_cumuls_min,
                      c.dimension_start_cumuls_max, c.dimension_end_cumuls_min,
                      c.dimension_end_cumuls_max, c.dimension_capacities,
                      c.dimension_evaluator_classes,
                      c.cumul_dependent_dimension_evaluator_classes,
                      c.visitable_nodes_hash, c.group_allowed_resources_hash);
  }
};

}  // namespace

void RoutingModel::ComputeVehicleClasses() {
  vehicle_class_index_of_vehicle_.assign(vehicles_, VehicleClassIndex(-1));
  absl::flat_hash_map<VehicleClass, VehicleClassIndex> vehicle_class_map;
  std::vector<bool> node_is_visitable(Size(), true);
  const auto bool_vec_hash = absl::Hash<std::vector<bool>>();
  for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
    VehicleClass vehicle_class;
    vehicle_class.cost_class_index = cost_class_index_of_vehicle_[vehicle];
    vehicle_class.fixed_cost = fixed_cost_of_vehicle_[vehicle];
    vehicle_class.used_when_empty = vehicle_used_when_empty_[vehicle];
    vehicle_class.start_equivalence_class =
        index_to_equivalence_class_[Start(vehicle)];
    vehicle_class.end_equivalence_class =
        index_to_equivalence_class_[End(vehicle)];
    for (const RoutingDimension* const dimension : dimensions_) {
      IntVar* const start_cumul_var = dimension->cumuls()[Start(vehicle)];
      vehicle_class.dimension_start_cumuls_min.push_back(
          start_cumul_var->Min());
      vehicle_class.dimension_start_cumuls_max.push_back(
          start_cumul_var->Max());
      IntVar* const end_cumul_var = dimension->cumuls()[End(vehicle)];
      vehicle_class.dimension_end_cumuls_min.push_back(end_cumul_var->Min());
      vehicle_class.dimension_end_cumuls_max.push_back(end_cumul_var->Max());
      vehicle_class.dimension_capacities.push_back(
          dimension->vehicle_capacities()[vehicle]);
      vehicle_class.dimension_evaluator_classes.push_back(
          dimension->vehicle_to_class(vehicle));
      vehicle_class.cumul_dependent_dimension_evaluator_classes.push_back(
          dimension->vehicle_to_cumul_dependent_class(vehicle));
    }
    node_is_visitable.assign(Size(), true);
    for (int index = 0; index < Size(); ++index) {
      DCHECK(!IsEnd(index));
      if (IsStart(index)) continue;
      if (!vehicle_vars_[index]->Contains(vehicle) ||
          !IsVehicleAllowedForIndex(vehicle, index)) {
        node_is_visitable[index] = false;
      }
    }
    vehicle_class.visitable_nodes_hash = bool_vec_hash(node_is_visitable);

    std::vector<int64_t>& allowed_resources_hash =
        vehicle_class.group_allowed_resources_hash;
    allowed_resources_hash.reserve(resource_groups_.size());
    for (int rg_index = 0; rg_index < resource_groups_.size(); rg_index++) {
      const ResourceGroup& resource_group = *resource_groups_[rg_index];
      if (!resource_group.VehicleRequiresAResource(vehicle)) {
        allowed_resources_hash.push_back(-1);
        continue;
      }
      const std::vector<IntVar*>& resource_vars = resource_vars_[rg_index];
      std::vector<bool> resource_allowed_for_vehicle(resource_group.Size(),
                                                     true);
      for (int resource = 0; resource < resource_group.Size(); resource++) {
        if (!resource_vars[vehicle]->Contains(resource) ||
            !resource_group.IsResourceAllowedForVehicle(resource, vehicle)) {
          resource_allowed_for_vehicle[resource] = false;
        }
      }
      allowed_resources_hash.push_back(
          bool_vec_hash(resource_allowed_for_vehicle));
    }
    DCHECK_EQ(allowed_resources_hash.size(), resource_groups_.size());

    const VehicleClassIndex num_vehicle_classes(vehicle_class_map.size());
    vehicle_class_index_of_vehicle_[vehicle] = gtl::LookupOrInsert(
        &vehicle_class_map, vehicle_class, num_vehicle_classes);
  }
  num_vehicle_classes_ = vehicle_class_map.size();
}

void RoutingModel::ComputeVehicleTypes() {
  const int nodes_squared = nodes_ * nodes_;
  std::vector<int>& type_index_of_vehicle =
      vehicle_type_container_.type_index_of_vehicle;
  std::vector<std::set<VehicleTypeContainer::VehicleClassEntry>>&
      sorted_vehicle_classes_per_type =
          vehicle_type_container_.sorted_vehicle_classes_per_type;
  std::vector<std::deque<int>>& vehicles_per_vehicle_class =
      vehicle_type_container_.vehicles_per_vehicle_class;

  type_index_of_vehicle.resize(vehicles_);
  sorted_vehicle_classes_per_type.clear();
  sorted_vehicle_classes_per_type.reserve(vehicles_);
  vehicles_per_vehicle_class.clear();
  vehicles_per_vehicle_class.resize(GetVehicleClassesCount());

  absl::flat_hash_map<int64_t, int> type_to_type_index;

  for (int v = 0; v < vehicles_; v++) {
    const int start = manager_.IndexToNode(Start(v)).value();
    const int end = manager_.IndexToNode(End(v)).value();
    const int cost_class = GetCostClassIndexOfVehicle(v).value();
    const int64_t type = cost_class * nodes_squared + start * nodes_ + end;

    const auto& vehicle_type_added = type_to_type_index.insert(
        std::make_pair(type, type_to_type_index.size()));

    const int index = vehicle_type_added.first->second;

    const int vehicle_class = GetVehicleClassIndexOfVehicle(v).value();
    const VehicleTypeContainer::VehicleClassEntry class_entry = {
        vehicle_class, GetFixedCostOfVehicle(v)};

    if (vehicle_type_added.second) {
      // Type was not indexed yet.
      DCHECK_EQ(sorted_vehicle_classes_per_type.size(), index);
      sorted_vehicle_classes_per_type.push_back({class_entry});
    } else {
      // Type already indexed.
      DCHECK_LT(index, sorted_vehicle_classes_per_type.size());
      sorted_vehicle_classes_per_type[index].insert(class_entry);
    }
    vehicles_per_vehicle_class[vehicle_class].push_back(v);
    type_index_of_vehicle[v] = index;
  }
}

void RoutingModel::ComputeResourceClasses() {
  for (auto& resource_group : resource_groups_) {
    resource_group->ComputeResourceClasses();
  }
}

void RoutingModel::FinalizeVisitTypes() {
  single_nodes_of_type_.clear();
  single_nodes_of_type_.resize(num_visit_types_);
  pair_indices_of_type_.clear();
  pair_indices_of_type_.resize(num_visit_types_);
  std::vector<absl::flat_hash_set<int>> pair_indices_added_for_type(
      num_visit_types_);

  const auto& store_pair_index_type = [this, &pair_indices_added_for_type](
                                          int pair_index, int visit_type) {
    if (pair_index != kUnassigned &&
        pair_indices_added_for_type[visit_type].insert(pair_index).second) {
      pair_indices_of_type_[visit_type].push_back(pair_index);
    }
  };

  for (int index = 0; index < index_to_visit_type_.size(); index++) {
    const int visit_type = GetVisitType(index);
    if (visit_type < 0) {
      continue;
    }
    if (!IsPickup(index) && !IsDelivery(index)) {
      single_nodes_of_type_[visit_type].push_back(index);
    } else {
      store_pair_index_type(index_to_pickup_position_[index].pd_pair_index,
                            visit_type);
      store_pair_index_type(index_to_delivery_position_[index].pd_pair_index,
                            visit_type);
    }
  }

  TopologicallySortVisitTypes();
  ComputeVisitTypesConnectedComponents();
}

namespace {
template <typename C>
std::vector<std::vector<int>> GetTopologicallySortedNodes(
    const SparseBitset<>& active_nodes, std::vector<int> node_in_degree,
    const std::vector<absl::flat_hash_set<int>>& children,
    const C& comparator) {
  std::vector<int> current_nodes_with_zero_indegree;
  for (int node : active_nodes.PositionsSetAtLeastOnce()) {
    if (node_in_degree[node] == 0) {
      current_nodes_with_zero_indegree.push_back(node);
    }
  }
  std::vector<std::vector<int>> topologically_sorted_nodes;
  int num_nodes_added = 0;
  while (!current_nodes_with_zero_indegree.empty()) {
    // Add all zero-degree nodes to the same topological order group, while
    // also marking their dependent nodes that become part of the next group.
    topologically_sorted_nodes.push_back({});
    std::vector<int>& topological_group = topologically_sorted_nodes.back();
    std::vector<int> next_nodes_with_zero_indegree;
    for (int node : current_nodes_with_zero_indegree) {
      topological_group.push_back(node);
      num_nodes_added++;
      for (int dependent_node : children[node]) {
        DCHECK_GT(node_in_degree[dependent_node], 0);
        if (--node_in_degree[dependent_node] == 0) {
          next_nodes_with_zero_indegree.push_back(dependent_node);
        }
      }
    }
    absl::c_sort(topological_group, comparator);
    // Swap the current nodes with zero in-degree with the next ones.
    current_nodes_with_zero_indegree.swap(next_nodes_with_zero_indegree);
  }

  const int num_active_nodes =
      active_nodes.NumberOfSetCallsWithDifferentArguments();
  DCHECK_LE(num_nodes_added, num_active_nodes);
  if (num_nodes_added < num_active_nodes) {
    // Graph is cyclic, no topological order.
    topologically_sorted_nodes.clear();
  }
  return topologically_sorted_nodes;
}
}  // namespace

void RoutingModel::ComputeVisitTypesConnectedComponents() {
  if (!HasSameVehicleTypeRequirements() && !HasTemporalTypeRequirements()) {
    return;
  }
  std::vector<std::vector<int>> graph(num_visit_types_);
  for (int type = 0; type < num_visit_types_; type++) {
    for (const std::vector<absl::flat_hash_set<int>>*
             required_type_alternatives :
         {&GetRequiredTypeAlternativesWhenAddingType(type),
          &GetRequiredTypeAlternativesWhenRemovingType(type),
          &GetSameVehicleRequiredTypeAlternativesOfType(type)}) {
      for (const absl::flat_hash_set<int>& alternatives :
           *required_type_alternatives) {
        for (int required_type : alternatives) {
          graph[required_type].push_back(type);
          graph[type].push_back(required_type);
        }
      }
    }
  }
  const std::vector<int> connected_components =
      util::GetConnectedComponents(num_visit_types_, graph);
  visit_type_components_.clear();
  visit_type_components_.resize(connected_components.size());
  for (int type = 0; type < num_visit_types_; type++) {
    visit_type_components_[connected_components[type]].push_back(type);
  }
}

void RoutingModel::TopologicallySortVisitTypes() {
  if (!HasSameVehicleTypeRequirements() && !HasTemporalTypeRequirements()) {
    return;
  }
  std::vector<std::pair<double, double>> type_requirement_tightness(
      num_visit_types_, {0, 0});
  std::vector<absl::flat_hash_set<int>> type_to_dependent_types(
      num_visit_types_);
  SparseBitset<> types_in_requirement_graph(num_visit_types_);
  std::vector<int> in_degree(num_visit_types_, 0);
  for (int type = 0; type < num_visit_types_; type++) {
    int num_alternative_required_types = 0;
    int num_required_sets = 0;
    for (const std::vector<absl::flat_hash_set<int>>*
             required_type_alternatives :
         {&GetRequiredTypeAlternativesWhenAddingType(type),
          &GetRequiredTypeAlternativesWhenRemovingType(type),
          &GetSameVehicleRequiredTypeAlternativesOfType(type)}) {
      for (const absl::flat_hash_set<int>& alternatives :
           *required_type_alternatives) {
        types_in_requirement_graph.Set(type);
        num_required_sets++;
        for (int required_type : alternatives) {
          type_requirement_tightness[required_type].second +=
              1.0 / alternatives.size();
          types_in_requirement_graph.Set(required_type);
          num_alternative_required_types++;
          if (type_to_dependent_types[required_type].insert(type).second) {
            in_degree[type]++;
          }
        }
      }
    }
    if (num_alternative_required_types > 0) {
      type_requirement_tightness[type].first += 1.0 * num_required_sets *
                                                num_required_sets /
                                                num_alternative_required_types;
    }
  }

  topologically_sorted_visit_types_ = GetTopologicallySortedNodes(
      types_in_requirement_graph, std::move(in_degree), type_to_dependent_types,
      // Sort the types in the current topological group based on their
      // requirement tightness.
      // NOTE: For a deterministic order, types with equal tightness are sorted
      // by increasing type.
      // TODO(user): Put types of the same topological order and same
      // requirement tightness in a single group (so that they all get inserted
      // simultaneously by the GlobalCheapestInsertion heuristic, for instance).
      [&type_requirement_tightness](int type1, int type2) {
        const auto& tightness1 = type_requirement_tightness[type1];
        const auto& tightness2 = type_requirement_tightness[type2];
        return tightness1 > tightness2 ||
               (tightness1 == tightness2 && type1 < type2);
      });
}

void RoutingModel::FinalizePrecedences() {
  for (const RoutingDimension* dimension : dimensions_) {
    if (dimension->GetNodePrecedences().empty()) continue;
    std::vector<int> in_degree(Size(), 0);
    SparseBitset<> nodes_in_precedences(Size());
    std::vector<absl::flat_hash_set<int>> successors(Size());
    std::vector<int64_t> node_max_offset(Size(),
                                         std::numeric_limits<int64_t>::min());
    // Note: A precedence constraint between first_node and second_node with an
    // offset enforces cumuls(second_node) >= cumuls(first_node) + offset.
    for (const auto [first_node, second_node, offset, unused] :
         dimension->GetNodePrecedences()) {
      in_degree[second_node]++;
      nodes_in_precedences.Set(first_node);
      nodes_in_precedences.Set(second_node);
      successors[first_node].insert(second_node);
      node_max_offset[first_node] =
          std::max(node_max_offset[first_node], offset);
      node_max_offset[second_node] =
          std::max(node_max_offset[second_node], offset);
    }
    topologically_sorted_node_precedences_.push_back(
        GetTopologicallySortedNodes(
            nodes_in_precedences, std::move(in_degree), successors,
            // Sort the nodes in the current topological group based on their
            // precedence offset.
            // NOTE: For a deterministic order, nodes with equal offset are
            // sorted by increasing node.
            [&node_max_offset](int node1, int node2) {
              const int64_t offset1 = node_max_offset[node1];
              const int64_t offset2 = node_max_offset[node2];
              return offset1 > offset2 || (offset1 == offset2 && node1 < node2);
            }));
  }
}

RoutingModel::DisjunctionIndex RoutingModel::AddDisjunction(
    const std::vector<int64_t>& indices, int64_t penalty,
    int64_t max_cardinality, PenaltyCostBehavior penalty_cost_behavior) {
  CHECK_GE(max_cardinality, 1);
  for (int i = 0; i < indices.size(); ++i) {
    CHECK_NE(kUnassigned, indices[i]);
  }

  const DisjunctionIndex disjunction_index(disjunctions_.size());
  disjunctions_.push_back(
      {indices, {penalty, max_cardinality, penalty_cost_behavior}});
  for (const int64_t index : indices) {
    index_to_disjunctions_[index].push_back(disjunction_index);
  }
  return disjunction_index;
}

bool RoutingModel::HasMandatoryDisjunctions() const {
  for (const auto& [indices, value] : disjunctions_) {
    if (value.penalty == kNoPenalty) return true;
  }
  return false;
}

bool RoutingModel::HasMaxCardinalityConstrainedDisjunctions() const {
  for (const auto& [indices, value] : disjunctions_) {
    if (indices.size() > value.max_cardinality) return true;
  }
  return false;
}

std::vector<std::pair<int64_t, int64_t>>
RoutingModel::GetPerfectBinaryDisjunctions() const {
  std::vector<std::pair<int64_t, int64_t>> var_index_pairs;
  for (const Disjunction& disjunction : disjunctions_) {
    const std::vector<int64_t>& var_indices = disjunction.indices;
    if (var_indices.size() != 2) continue;
    const int64_t v0 = var_indices[0];
    const int64_t v1 = var_indices[1];
    if (index_to_disjunctions_[v0].size() == 1 &&
        index_to_disjunctions_[v1].size() == 1) {
      // We output sorted pairs.
      var_index_pairs.push_back({std::min(v0, v1), std::max(v0, v1)});
    }
  }
  std::sort(var_index_pairs.begin(), var_index_pairs.end());
  return var_index_pairs;
}

void RoutingModel::IgnoreDisjunctionsAlreadyForcedToZero() {
  CHECK(!closed_);
  for (Disjunction& disjunction : disjunctions_) {
    bool has_one_potentially_active_var = false;
    for (const int64_t var_index : disjunction.indices) {
      if (ActiveVar(var_index)->Max() > 0) {
        has_one_potentially_active_var = true;
        break;
      }
    }
    if (!has_one_potentially_active_var) {
      disjunction.value.max_cardinality = 0;
    }
  }
}

IntVar* RoutingModel::CreateDisjunction(DisjunctionIndex disjunction) {
  const std::vector<int64_t>& indices = disjunctions_[disjunction].indices;
  const int indices_size = indices.size();
  std::vector<IntVar*> disjunction_vars(indices_size);
  for (int i = 0; i < indices_size; ++i) {
    const int64_t index = indices[i];
    CHECK_LT(index, Size());
    disjunction_vars[i] = ActiveVar(index);
  }
  const int64_t max_cardinality =
      disjunctions_[disjunction].value.max_cardinality;

  IntVar* number_active_vars = solver_->MakeIntVar(0, max_cardinality);
  solver_->AddConstraint(
      solver_->MakeSumEquality(disjunction_vars, number_active_vars));

  const int64_t penalty = disjunctions_[disjunction].value.penalty;
  // If penalty is negative, then disjunction is mandatory
  // i.e. number of active vars must be equal to max cardinality.
  if (penalty < 0) {
    solver_->AddConstraint(
        solver_->MakeEquality(number_active_vars, max_cardinality));
    return nullptr;
  }

  const PenaltyCostBehavior penalty_cost_behavior =
      disjunctions_[disjunction].value.penalty_cost_behavior;
  if (max_cardinality == 1 ||
      penalty_cost_behavior == PenaltyCostBehavior::PENALIZE_ONCE) {
    IntVar* penalize_var = solver_->MakeBoolVar();
    solver_->AddConstraint(solver_->MakeIsDifferentCstCt(
        number_active_vars, max_cardinality, penalize_var));
    return solver_->MakeProd(penalize_var, penalty)->Var();
  } else {
    IntVar* number_no_active_vars = solver_->MakeIntVar(0, max_cardinality);
    solver_->AddConstraint(solver_->MakeEquality(
        number_no_active_vars,
        solver_->MakeDifference(max_cardinality, number_active_vars)));
    return solver_->MakeProd(number_no_active_vars, penalty)->Var();
  }
}

void RoutingModel::AddSoftSameVehicleConstraint(std::vector<int64_t> indices,
                                                int64_t cost) {
  if (!indices.empty()) {
    same_vehicle_costs_.push_back(
        {.indices = std::move(indices), .value = cost});
  }
}

void RoutingModel::SetAllowedVehiclesForIndex(absl::Span<const int> vehicles,
                                              int64_t index) {
  DCHECK(!closed_);
  auto& allowed_vehicles = allowed_vehicles_[index];
  allowed_vehicles.clear();
  for (int vehicle : vehicles) {
    allowed_vehicles.insert(vehicle);
  }
}

void RoutingModel::AddPickupAndDelivery(int64_t pickup, int64_t delivery) {
  AddPickupAndDeliverySetsInternal({pickup}, {delivery});
  pickup_delivery_disjunctions_.push_back({kNoDisjunction, kNoDisjunction});
}

void RoutingModel::AddPickupAndDeliverySets(
    DisjunctionIndex pickup_disjunction,
    DisjunctionIndex delivery_disjunction) {
  AddPickupAndDeliverySetsInternal(
      GetDisjunctionNodeIndices(pickup_disjunction),
      GetDisjunctionNodeIndices(delivery_disjunction));
  pickup_delivery_disjunctions_.push_back(
      {pickup_disjunction, delivery_disjunction});
}

// TODO(user): Return an error (boolean?) when any node in the pickup or
// deliveries is already registered as pickup or delivery instead of DCHECK-ing.
void RoutingModel::AddPickupAndDeliverySetsInternal(
    const std::vector<int64_t>& pickups,
    const std::vector<int64_t>& deliveries) {
  if (pickups.empty() || deliveries.empty()) {
    return;
  }
  const int64_t size = Size();
  const int pair_index = pickup_delivery_pairs_.size();
  for (int pickup_index = 0; pickup_index < pickups.size(); pickup_index++) {
    const int64_t pickup = pickups[pickup_index];
    CHECK_LT(pickup, size);
    DCHECK(!IsPickup(pickup));
    DCHECK(!IsDelivery(pickup));
    index_to_pickup_position_[pickup] = {pair_index, pickup_index};
  }
  for (int delivery_index = 0; delivery_index < deliveries.size();
       delivery_index++) {
    const int64_t delivery = deliveries[delivery_index];
    CHECK_LT(delivery, size);
    DCHECK(!IsPickup(delivery));
    DCHECK(!IsDelivery(delivery));
    index_to_delivery_position_[delivery] = {pair_index, delivery_index};
  }
  pickup_delivery_pairs_.push_back({pickups, deliveries});
}

std::optional<RoutingModel::PickupDeliveryPosition>
RoutingModel::GetPickupPosition(int64_t node_index) const {
  CHECK_LT(node_index, index_to_pickup_position_.size());
  if (IsPickup(node_index)) return index_to_pickup_position_[node_index];
  return std::nullopt;
}

std::optional<RoutingModel::PickupDeliveryPosition>
RoutingModel::GetDeliveryPosition(int64_t node_index) const {
  CHECK_LT(node_index, index_to_delivery_position_.size());
  if (IsDelivery(node_index)) return index_to_delivery_position_[node_index];
  return std::nullopt;
}

void RoutingModel::SetPickupAndDeliveryPolicyOfVehicle(
    PickupAndDeliveryPolicy policy, int vehicle) {
  CHECK_LT(vehicle, vehicles_);
  vehicle_pickup_delivery_policy_[vehicle] = policy;
}

void RoutingModel::SetPickupAndDeliveryPolicyOfAllVehicles(
    PickupAndDeliveryPolicy policy) {
  CHECK_LT(0, vehicles_);
  for (int i = 0; i < vehicles_; ++i) {
    SetPickupAndDeliveryPolicyOfVehicle(policy, i);
  }
}

RoutingModel::PickupAndDeliveryPolicy
RoutingModel::GetPickupAndDeliveryPolicyOfVehicle(int vehicle) const {
  CHECK_LT(vehicle, vehicles_);
  return vehicle_pickup_delivery_policy_[vehicle];
}

std::optional<int64_t> RoutingModel::GetFirstMatchingPickupDeliverySibling(
    int64_t node, const std::function<bool(int64_t)>& is_match) const {
  // NOTE: In most use-cases, where each node is a pickup or delivery in a
  // single index pair, this function is in O(k) where k is the number of
  // alternative deliveries or pickups for this index pair.

  // A node can't be a pickup and a delivery at the same time.
  DCHECK(!IsPickup(node) || !IsDelivery(node));

  const auto& pickup_and_delivery_pairs = GetPickupAndDeliveryPairs();

  if (const auto pickup_position = GetPickupPosition(node);
      pickup_position.has_value()) {
    const int pair_index = pickup_position->pd_pair_index;
    for (int64_t delivery_sibling :
         pickup_and_delivery_pairs[pair_index].delivery_alternatives) {
      if (is_match(delivery_sibling)) {
        return delivery_sibling;
      }
    }
  }

  if (const auto delivery_position = GetDeliveryPosition(node);
      delivery_position.has_value()) {
    const int pair_index = delivery_position->pd_pair_index;
    for (int64_t pickup_sibling :
         pickup_and_delivery_pairs[pair_index].pickup_alternatives) {
      if (is_match(pickup_sibling)) {
        return pickup_sibling;
      }
    }
  }

  return std::nullopt;
}

int RoutingModel::GetNumOfSingletonNodes() const {
  int count = 0;
  for (int i = 0; i < Nexts().size(); ++i) {
    // End nodes have no next variables.
    if (!IsStart(i) && !IsPickup(i) && !IsDelivery(i)) {
      ++count;
    }
  }
  return count;
}

IntVar* RoutingModel::CreateSameVehicleCost(int vehicle_index) {
  const std::vector<int64_t>& indices =
      same_vehicle_costs_[vehicle_index].indices;
  CHECK(!indices.empty());
  std::vector<IntVar*> vehicle_counts;
  solver_->MakeIntVarArray(vehicle_vars_.size() + 1, 0, indices.size() + 1,
                           &vehicle_counts);
  std::vector<int64_t> vehicle_values(vehicle_vars_.size() + 1);
  for (int i = 0; i < vehicle_vars_.size(); ++i) {
    vehicle_values[i] = i;
  }
  vehicle_values[vehicle_vars_.size()] = -1;
  std::vector<IntVar*> vehicle_vars;
  vehicle_vars.reserve(indices.size());
  for (const int64_t index : indices) {
    vehicle_vars.push_back(vehicle_vars_[index]);
  }
  solver_->AddConstraint(solver_->MakeDistribute(vehicle_vars, vehicle_counts));
  std::vector<IntVar*> vehicle_used;
  for (int i = 0; i < vehicle_vars_.size() + 1; ++i) {
    vehicle_used.push_back(
        solver_->MakeIsGreaterOrEqualCstVar(vehicle_counts[i], 1));
  }
  vehicle_used.push_back(solver_->MakeIntConst(-1));
  return solver_
      ->MakeProd(solver_->MakeMax(solver_->MakeSum(vehicle_used), 0),
                 same_vehicle_costs_[vehicle_index].value)
      ->Var();
}

void RoutingModel::AddLocalSearchOperator(LocalSearchOperator* ls_operator) {
  extra_operators_.push_back(ls_operator);
}

int64_t RoutingModel::GetDepot() const {
  return vehicles() > 0 ? Start(0) : -1;
}

// TODO(user): Remove the need for the homogeneous version once the
// vehicle var to cost class element constraint is fast enough.
void RoutingModel::AppendHomogeneousArcCosts(
    const RoutingSearchParameters& parameters, int node_index,
    std::vector<IntVar*>* cost_elements) {
  CHECK(cost_elements != nullptr);
  const auto arc_cost_evaluator = [this, node_index](int64_t next_index) {
    return GetHomogeneousCost(node_index, next_index);
  };
  if (UsesLightPropagation(parameters)) {
    // Only supporting positive costs.
    // TODO(user): Detect why changing lower bound to kint64min stalls
    // the search in GLS in some cases (Solomon instances for instance).
    IntVar* const base_cost_var =
        solver_->MakeIntVar(0, std::numeric_limits<int64_t>::max());
    solver_->AddConstraint(solver_->MakeLightElement(
        arc_cost_evaluator, base_cost_var, nexts_[node_index],
        [this]() { return enable_deep_serialization_; }));
    IntVar* const var =
        solver_->MakeProd(base_cost_var, active_[node_index])->Var();
    cost_elements->push_back(var);
  } else {
    IntExpr* const expr =
        solver_->MakeElement(arc_cost_evaluator, nexts_[node_index]);
    IntVar* const var = solver_->MakeProd(expr, active_[node_index])->Var();
    cost_elements->push_back(var);
  }
}

void RoutingModel::AppendArcCosts(const RoutingSearchParameters& parameters,
                                  int node_index,
                                  std::vector<IntVar*>* cost_elements) {
  CHECK(cost_elements != nullptr);
  DCHECK_GT(vehicles_, 0);
  if (UsesLightPropagation(parameters)) {
    // Only supporting positive costs.
    // TODO(user): Detect why changing lower bound to kint64min stalls
    // the search in GLS in some cases (Solomon instances for instance).
    IntVar* const base_cost_var =
        solver_->MakeIntVar(0, std::numeric_limits<int64_t>::max());
    solver_->AddConstraint(solver_->MakeLightElement(
        [this, node_index](int64_t to, int64_t vehicle) {
          return GetArcCostForVehicle(node_index, to, vehicle);
        },
        base_cost_var, nexts_[node_index], vehicle_vars_[node_index],
        [this]() { return enable_deep_serialization_; }));
    IntVar* const var =
        solver_->MakeProd(base_cost_var, active_[node_index])->Var();
    cost_elements->push_back(var);
  } else {
    IntVar* const vehicle_class_var =
        solver_
            ->MakeElement(
                [this](int64_t index) {
                  return SafeGetCostClassInt64OfVehicle(index);
                },
                vehicle_vars_[node_index])
            ->Var();
    IntExpr* const expr = solver_->MakeElement(
        [this, node_index](int64_t next, int64_t vehicle_class) {
          return GetArcCostForClass(node_index, next, vehicle_class);
        },
        nexts_[node_index], vehicle_class_var);
    IntVar* const var = solver_->MakeProd(expr, active_[node_index])->Var();
    cost_elements->push_back(var);
  }
}

int RoutingModel::GetVehicleStartClass(int64_t start_index) const {
  const int vehicle = VehicleIndex(start_index);
  if (vehicle != kUnassigned) {
    return GetVehicleClassIndexOfVehicle(vehicle).value();
  }
  return kUnassigned;
}

const std::deque<int>& RoutingModel::GetVehiclesOfSameClass(
    int64_t start_end_index) const {
  const int vehicle = VehicleIndex(start_end_index);
  DCHECK_NE(vehicle, kUnassigned);
  return GetVehicleTypeContainer().vehicles_per_vehicle_class
      [GetVehicleClassIndexOfVehicle(vehicle).value()];
}

std::vector<std::pair<int64_t, int64_t>> RoutingModel::GetSameVehicleClassArcs(
    int64_t from_index, int64_t to_index) const {
  std::vector<std::pair<int64_t, int64_t>> arcs;
  if (IsStart(from_index)) {
    for (int vehicle : GetVehiclesOfSameClass(from_index)) {
      const int64_t start = Start(vehicle);
      if (!IsEnd(to_index)) {
        arcs.push_back({start, to_index});
      } else {
        arcs.push_back({start, End(vehicle)});
      }
    }
  } else if (IsEnd(to_index)) {
    for (int vehicle : GetVehiclesOfSameClass(to_index)) {
      arcs.push_back({from_index, End(vehicle)});
    }
  } else {
    arcs.push_back({from_index, to_index});
  }
  return arcs;
}

std::string RoutingModel::FindErrorInSearchParametersForModel(
    const RoutingSearchParameters& search_parameters) const {
  const FirstSolutionStrategy::Value first_solution_strategy =
      search_parameters.first_solution_strategy();
  if (GetFirstSolutionDecisionBuilder(search_parameters) == nullptr) {
    return absl::StrCat(
        "Undefined first solution strategy: ",
        FirstSolutionStrategy::Value_Name(first_solution_strategy),
        " (int value: ", first_solution_strategy, ")");
  }
  if (search_parameters.first_solution_strategy() ==
          FirstSolutionStrategy::SWEEP &&
      sweep_arranger() == nullptr) {
    return "Undefined sweep arranger for ROUTING_SWEEP strategy.";
  }
  return "";
}

void RoutingModel::QuietCloseModel() {
  QuietCloseModelWithParameters(DefaultRoutingSearchParameters());
}

void RoutingModel::CloseModel() {
  CloseModelWithParameters(DefaultRoutingSearchParameters());
}

class RoutingModelInspector : public ModelVisitor {
 public:
  explicit RoutingModelInspector(RoutingModel* model) : model_(model) {
    same_vehicle_components_.SetNumberOfNodes(model->Size());
    same_active_var_components_.SetNumberOfNodes(model->Size());
    for (const std::string& name : model->GetAllDimensionNames()) {
      RoutingDimension* const dimension = model->GetMutableDimension(name);
      const std::vector<IntVar*>& cumuls = dimension->cumuls();
      for (int i = 0; i < cumuls.size(); ++i) {
        cumul_to_dim_indices_[cumuls[i]] = {dimension, i};
      }
    }
    const std::vector<IntVar*>& vehicle_vars = model->VehicleVars();
    for (int i = 0; i < vehicle_vars.size(); ++i) {
      vehicle_var_to_indices_[vehicle_vars[i]] = i;
    }
    for (int i = 0; i < model->Size(); ++i) {
      active_var_to_indices_[model->ActiveVar(i)] = i;
    }
    RegisterInspectors();
  }
  ~RoutingModelInspector() override = default;
  void EndVisitModel(const std::string& /*solver_name*/) override {
    const std::vector<int> node_to_same_vehicle_component_id =
        same_vehicle_components_.GetComponentIds();
    model_->InitSameVehicleGroups(
        same_vehicle_components_.GetNumberOfComponents());
    for (int node = 0; node < model_->Size(); ++node) {
      model_->SetSameVehicleGroup(node,
                                  node_to_same_vehicle_component_id[node]);
    }
    const std::vector<int> node_to_same_active_var_component_id =
        same_active_var_components_.GetComponentIds();
    model_->InitSameActiveVarGroups(
        same_active_var_components_.GetNumberOfComponents());
    for (int node = 0; node < model_->Size(); ++node) {
      model_->SetSameActiveVarGroup(node,
                                    node_to_same_active_var_component_id[node]);
    }
    // TODO(user): Perform transitive closure of dimension precedence graphs.
    // TODO(user): Have a single annotated precedence graph.
  }
  void EndVisitConstraint(const std::string& type_name,
                          const Constraint* const /*constraint*/) override {
    gtl::FindWithDefault(constraint_inspectors_, type_name, []() {})();
  }
  void VisitIntegerExpressionArgument(const std::string& type_name,
                                      IntExpr* const expr) override {
    gtl::FindWithDefault(expr_inspectors_, type_name,
                         [](const IntExpr*) {})(expr);
  }
  void VisitIntegerArrayArgument(const std::string& arg_name,
                                 const std::vector<int64_t>& values) override {
    gtl::FindWithDefault(array_inspectors_, arg_name,
                         [](absl::Span<const int64_t>) {})(values);
  }

 private:
  using ExprInspector = std::function<void(const IntExpr*)>;
  using ArrayInspector = std::function<void(const std::vector<int64_t>&)>;
  using ConstraintInspector = std::function<void()>;

  void RegisterInspectors() {
    expr_inspectors_[kExpressionArgument] = [this](const IntExpr* expr) {
      expr_ = expr;
    };
    expr_inspectors_[kLeftArgument] = [this](const IntExpr* expr) {
      left_ = expr;
    };
    expr_inspectors_[kRightArgument] = [this](const IntExpr* expr) {
      right_ = expr;
    };
    array_inspectors_[kStartsArgument] =
        [this](const std::vector<int64_t>& int_array) {
          starts_argument_ = int_array;
        };
    array_inspectors_[kEndsArgument] =
        [this](const std::vector<int64_t>& int_array) {
          ends_argument_ = int_array;
        };
    constraint_inspectors_[kNotMember] = [this]() {
      std::pair<RoutingDimension*, int> dim_index;
      if (gtl::FindCopy(cumul_to_dim_indices_, expr_, &dim_index)) {
        RoutingDimension* const dimension = dim_index.first;
        const int index = dim_index.second;
        dimension->forbidden_intervals_[index].InsertIntervals(starts_argument_,
                                                               ends_argument_);
        VLOG(2) << dimension->name() << " " << index << ": "
                << dimension->forbidden_intervals_[index].DebugString();
      }
      expr_ = nullptr;
      starts_argument_.clear();
      ends_argument_.clear();
    };
    constraint_inspectors_[kEquality] = [this]() {
      int left_index = 0;
      int right_index = 0;
      if (gtl::FindCopy(vehicle_var_to_indices_, left_, &left_index) &&
          gtl::FindCopy(vehicle_var_to_indices_, right_, &right_index)) {
        VLOG(2) << "Vehicle variables for " << left_index << " and "
                << right_index << " are equal.";
        same_vehicle_components_.AddEdge(left_index, right_index);
      }
      if (gtl::FindCopy(active_var_to_indices_, left_, &left_index) &&
          gtl::FindCopy(active_var_to_indices_, right_, &right_index)) {
        VLOG(2) << "Active variables for " << left_index << " and "
                << right_index << " are equal.";
        same_active_var_components_.AddEdge(left_index, right_index);
      }

      left_ = nullptr;
      right_ = nullptr;
    };
    constraint_inspectors_[kLessOrEqual] = [this]() {
      std::pair<RoutingDimension*, int> left_index;
      std::pair<RoutingDimension*, int> right_index;
      if (gtl::FindCopy(cumul_to_dim_indices_, left_, &left_index) &&
          gtl::FindCopy(cumul_to_dim_indices_, right_, &right_index)) {
        RoutingDimension* const dimension = left_index.first;
        if (dimension == right_index.first) {
          VLOG(2) << "For dimension " << dimension->name() << ", cumul for "
                  << left_index.second << " is less than " << right_index.second
                  << ".";
          dimension->path_precedence_graph_.AddArc(left_index.second,
                                                   right_index.second);
        }
      }
      left_ = nullptr;
      right_ = nullptr;
    };
  }

  RoutingModel* const model_;
  DenseConnectedComponentsFinder same_vehicle_components_;
  DenseConnectedComponentsFinder same_active_var_components_;
  absl::flat_hash_map<const IntExpr*, std::pair<RoutingDimension*, int>>
      cumul_to_dim_indices_;
  absl::flat_hash_map<const IntExpr*, int> vehicle_var_to_indices_;
  absl::flat_hash_map<const IntExpr*, int> active_var_to_indices_;
  absl::flat_hash_map<std::string, ExprInspector> expr_inspectors_;
  absl::flat_hash_map<std::string, ArrayInspector> array_inspectors_;
  absl::flat_hash_map<std::string, ConstraintInspector> constraint_inspectors_;
  const IntExpr* expr_ = nullptr;
  const IntExpr* left_ = nullptr;
  const IntExpr* right_ = nullptr;
  std::vector<int64_t> starts_argument_;
  std::vector<int64_t> ends_argument_;
};

void RoutingModel::DetectImplicitPickupAndDeliveries() {
  std::vector<int> non_pickup_delivery_nodes;
  for (int node = 0; node < Size(); ++node) {
    if (!IsStart(node) && !IsPickup(node) && !IsDelivery(node)) {
      non_pickup_delivery_nodes.push_back(node);
    }
  }
  // Needs to be sorted for stability.
  std::set<std::pair<int64_t, int64_t>> implicit_pickup_deliveries;
  for (const RoutingDimension* const dimension : dimensions_) {
    if (dimension->class_evaluators_.size() != 1) {
      continue;
    }
    const TransitCallback1& transit =
        UnaryTransitCallbackOrNull(dimension->class_evaluators_[0]);
    if (transit == nullptr) continue;
    absl::flat_hash_map<int64_t, std::vector<int64_t>> nodes_by_positive_demand;
    absl::flat_hash_map<int64_t, std::vector<int64_t>> nodes_by_negative_demand;
    for (int node : non_pickup_delivery_nodes) {
      const int64_t demand = transit(node);
      if (demand > 0) {
        nodes_by_positive_demand[demand].push_back(node);
      } else if (demand < 0) {
        nodes_by_negative_demand[-demand].push_back(node);
      }
    }
    for (const auto& [demand, positive_nodes] : nodes_by_positive_demand) {
      const std::vector<int64_t>* const negative_nodes =
          gtl::FindOrNull(nodes_by_negative_demand, demand);
      if (negative_nodes != nullptr) {
        for (int64_t positive_node : positive_nodes) {
          for (int64_t negative_node : *negative_nodes) {
            implicit_pickup_deliveries.insert({positive_node, negative_node});
          }
        }
      }
    }
  }
  implicit_pickup_delivery_pairs_without_alternatives_.clear();
  for (auto [pickup, delivery] : implicit_pickup_deliveries) {
    implicit_pickup_delivery_pairs_without_alternatives_.push_back(
        {{pickup}, {delivery}});
  }
}

namespace {
absl::Duration GetTimeLimit(const RoutingSearchParameters& parameters) {
  if (!parameters.has_time_limit()) return absl::InfiniteDuration();
  return util_time::DecodeGoogleApiProto(parameters.time_limit()).value();
}
}  // namespace

void RoutingModel::CloseModelWithParameters(
    const RoutingSearchParameters& parameters) {
  status_ = RoutingSearchStatus::ROUTING_NOT_SOLVED;
  const std::string error = FindErrorInRoutingSearchParameters(parameters);
  if (!error.empty()) {
    status_ = RoutingSearchStatus::ROUTING_INVALID;
    LOG(ERROR) << "Invalid RoutingSearchParameters: " << error;
    return;
  }
  if (closed_) {
    LOG(WARNING) << "Model already closed";
    return;
  }
  closed_ = true;

  // Setup the time limit to be able to check it while closing the model.
  GetOrCreateLimit()->UpdateLimits(
      GetTimeLimit(parameters), std::numeric_limits<int64_t>::max(),
      std::numeric_limits<int64_t>::max(), parameters.solution_limit());

  for (RoutingDimension* const dimension : dimensions_) {
    dimension->CloseModel(UsesLightPropagation(parameters));
  }

  dimension_resource_group_indices_.resize(dimensions_.size());
  for (int rg_index = 0; rg_index < resource_groups_.size(); rg_index++) {
    const ResourceGroup& resource_group = *resource_groups_[rg_index];
    if (resource_group.GetVehiclesRequiringAResource().empty()) continue;
    for (DimensionIndex dim_index :
         resource_group.GetAffectedDimensionIndices()) {
      dimension_resource_group_indices_[dim_index].push_back(rg_index);
    }
  }

  // NOTE: FinalizeAllowedVehicles() must be called *after* calling
  // CloseModel() on dimensions and *before* ComputeVehicleClasses().
  FinalizeAllowedVehicles();
  ComputeCostClasses(parameters);
  ComputeVehicleClasses();
  ComputeVehicleTypes();
  ComputeResourceClasses();
  FinalizeVisitTypes();
  FinalizePrecedences();
  vehicle_start_class_callback_ = [this](int64_t start) {
    return GetVehicleStartClass(start);
  };

  AddNoCycleConstraintInternal();

  const int size = Size();

  // Vehicle variable constraints.
  for (int i = 0; i < vehicles_; ++i) {
    const int64_t start = Start(i);
    const int64_t end = End(i);
    solver_->AddConstraint(
        solver_->MakeEquality(vehicle_vars_[start], solver_->MakeIntConst(i)));
    solver_->AddConstraint(
        solver_->MakeEquality(vehicle_vars_[end], solver_->MakeIntConst(i)));
    solver_->AddConstraint(
        solver_->MakeIsDifferentCstCt(nexts_[start], end, vehicle_active_[i]));
    if (vehicle_used_when_empty_[i]) {
      vehicle_route_considered_[i]->SetMin(1);
    } else {
      solver_->AddConstraint(solver_->MakeEquality(
          vehicle_active_[i], vehicle_route_considered_[i]));
    }
  }
  // Reduce domains of vehicle variables.
  for (int i = 0; i < allowed_vehicles_.size(); ++i) {
    const auto& allowed_vehicles = allowed_vehicles_[i];
    if (!allowed_vehicles.empty()) {
      std::vector<int64_t> vehicles;
      vehicles.reserve(allowed_vehicles.size() + 1);
      vehicles.push_back(-1);
      for (int vehicle : allowed_vehicles) {
        vehicles.push_back(vehicle);
      }
      solver_->AddConstraint(solver_->MakeMemberCt(VehicleVar(i), vehicles));
    }
  }

  // Limit the number of vehicles with non-empty routes.
  if (vehicles_ > max_active_vehicles_) {
    solver_->AddConstraint(
        solver_->MakeSumLessOrEqual(vehicle_active_, max_active_vehicles_));
    for (const RoutingDimension* dimension : dimensions_) {
      solver_->AddConstraint(MakeNumActiveVehiclesCapacityConstraint(
          solver_.get(), dimension->fixed_transits_, active_, vehicle_active_,
          dimension->vehicle_capacities_, max_active_vehicles_));
    }
  }

  // If there is only one vehicle in the model the vehicle variables will have
  // a maximum domain of [-1, 0]. If a node is performed/active then its vehicle
  // variable will be reduced to [0] making the path-cumul constraint below
  // useless. If the node is unperformed/unactive then its vehicle variable will
  // be reduced to [-1] in any case.
  if (vehicles_ > 1) {
    std::vector<IntVar*> zero_transit(size, solver_->MakeIntConst(0));
    solver_->AddConstraint(solver_->MakeDelayedPathCumul(
        nexts_, active_, vehicle_vars_, zero_transit));
  }

  // Nodes which are not in a disjunction are mandatory, and those with a
  // trivially infeasible type are necessarily unperformed
  for (int i = 0; i < size; ++i) {
    const std::vector<DisjunctionIndex>& disjunctions =
        GetDisjunctionIndices(i);
    bool is_mandatory = disjunctions.empty();
    for (const DisjunctionIndex& disjunction : disjunctions) {
      if (GetDisjunctionNodeIndices(disjunction).size() == 1 &&
          GetDisjunctionPenalty(disjunction) == kNoPenalty) {
        is_mandatory = true;
        break;
      }
    }
    if (is_mandatory && active_[i]->Max() != 0) {
      active_[i]->SetValue(1);
    }
    const int type = GetVisitType(i);
    if (type == kUnassigned) {
      continue;
    }
    const absl::flat_hash_set<VisitTypePolicy>* const infeasible_policies =
        gtl::FindOrNull(trivially_infeasible_visit_types_to_policies_, type);
    if (infeasible_policies != nullptr &&
        infeasible_policies->contains(index_to_type_policy_[i])) {
      active_[i]->SetValue(0);
    }
  }

  // Reduce domain of next variables.
  for (int i = 0; i < size; ++i) {
    // No variable can point back to a start.
    solver_->AddConstraint(MakeDifferentFromValues(solver_.get(), nexts_[i],
                                                   paths_metadata_.Starts()));
    // Extra constraint to state an active node can't point to itself.
    solver_->AddConstraint(
        solver_->MakeIsDifferentCstCt(nexts_[i], i, active_[i]));
  }

  // Add constraints to bind vehicle_vars_[i] to -1 in case that node i is not
  // active.
  for (int i = 0; i < size; ++i) {
    solver_->AddConstraint(
        solver_->MakeIsDifferentCstCt(vehicle_vars_[i], -1, active_[i]));
  }

  if (HasTypeRegulations()) {
    solver_->AddConstraint(
        solver_->RevAlloc(new TypeRegulationsConstraint(*this)));
  }

  // Associate first and "logical" last nodes
  for (int i = 0; i < vehicles_; ++i) {
    std::vector<int64_t> forbidden_ends;
    forbidden_ends.reserve(vehicles_ - 1);
    for (int j = 0; j < vehicles_; ++j) {
      if (i != j) {
        forbidden_ends.push_back(End(j));
      }
    }
    solver_->AddConstraint(MakeDifferentFromValues(
        solver_.get(), nexts_[Start(i)], std::move(forbidden_ends)));
  }

  // Constraining is_bound_to_end_ variables.
  for (const int64_t end : paths_metadata_.Ends()) {
    is_bound_to_end_[end]->SetValue(1);
  }

  // Adding route constraint.
  std::vector<IntVar*> route_cost_vars;
  if (!route_evaluators_.empty()) {
    solver()->MakeIntVarArray(vehicles(), 0, kint64max, &route_cost_vars);
    solver()->AddConstraint(MakeRouteConstraint(
        this, route_cost_vars,
        absl::bind_front(&RoutingModel::GetRouteCost, this)));
  }

  std::vector<IntVar*> cost_elements;
  // Arc and dimension costs.
  if (vehicles_ > 0) {
    for (int node_index = 0; node_index < size; ++node_index) {
      if (CostsAreHomogeneousAcrossVehicles()) {
        AppendHomogeneousArcCosts(parameters, node_index, &cost_elements);
      } else {
        AppendArcCosts(parameters, node_index, &cost_elements);
      }
    }
    if (vehicle_amortized_cost_factors_set_) {
      std::vector<IntVar*> route_lengths;
      solver_->MakeIntVarArray(vehicles_, 0, size, &route_lengths);
      solver_->AddConstraint(
          solver_->MakeDistribute(vehicle_vars_, route_lengths));
      std::vector<IntVar*> vehicle_used;
      for (int i = 0; i < vehicles_; i++) {
        // The start/end of the vehicle are always on the route.
        vehicle_used.push_back(
            solver_->MakeIsGreaterCstVar(route_lengths[i], 2));
        IntVar* const var =
            solver_
                ->MakeProd(solver_->MakeOpposite(solver_->MakeSquare(
                               solver_->MakeSum(route_lengths[i], -2))),
                           quadratic_cost_factor_of_vehicle_[i])
                ->Var();
        cost_elements.push_back(var);
      }
      IntVar* const vehicle_usage_cost =
          solver_->MakeScalProd(vehicle_used, linear_cost_factor_of_vehicle_)
              ->Var();
      cost_elements.push_back(vehicle_usage_cost);
    }
  }
  // Dimension span constraints: cost and limits.
  for (const RoutingDimension* dimension : dimensions_) {
    dimension->SetupGlobalSpanCost(&cost_elements);
    dimension->SetupSlackAndDependentTransitCosts();
    const std::vector<int64_t>& span_costs =
        dimension->vehicle_span_cost_coefficients();
    const std::vector<int64_t>& slack_costs =
        dimension->vehicle_slack_cost_coefficients();
    const std::vector<int64_t>& span_ubs =
        dimension->vehicle_span_upper_bounds();
    const bool has_span_constraint =
        std::any_of(span_costs.begin(), span_costs.end(),
                    [](int64_t coeff) { return coeff != 0; }) ||
        std::any_of(slack_costs.begin(), slack_costs.end(),
                    [](int64_t coeff) { return coeff != 0; }) ||
        std::any_of(span_ubs.begin(), span_ubs.end(),
                    [](int64_t value) {
                      return value < std::numeric_limits<int64_t>::max();
                    }) ||
        dimension->HasSoftSpanUpperBounds() ||
        dimension->HasQuadraticCostSoftSpanUpperBounds();
    if (has_span_constraint) {
      std::vector<IntVar*> spans(vehicles(), nullptr);
      std::vector<IntVar*> total_slacks(vehicles(), nullptr);
      // Generate variables only where needed.
      for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
        if (span_ubs[vehicle] < std::numeric_limits<int64_t>::max()) {
          spans[vehicle] = solver_->MakeIntVar(0, span_ubs[vehicle], "");
        }
        if (span_costs[vehicle] != 0 || slack_costs[vehicle] != 0) {
          total_slacks[vehicle] = solver_->MakeIntVar(0, span_ubs[vehicle], "");
        }
      }
      if (dimension->HasSoftSpanUpperBounds()) {
        for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
          if (spans[vehicle]) continue;
          const BoundCost bound_cost =
              dimension->GetSoftSpanUpperBoundForVehicle(vehicle);
          if (bound_cost.cost == 0) continue;
          spans[vehicle] = solver_->MakeIntVar(0, span_ubs[vehicle]);
        }
      }
      if (dimension->HasQuadraticCostSoftSpanUpperBounds()) {
        for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
          if (spans[vehicle]) continue;
          const BoundCost bound_cost =
              dimension->GetQuadraticCostSoftSpanUpperBoundForVehicle(vehicle);
          if (bound_cost.cost == 0) continue;
          spans[vehicle] = solver_->MakeIntVar(0, span_ubs[vehicle]);
        }
      }
      solver_->AddConstraint(
          MakePathSpansAndTotalSlacks(dimension, spans, total_slacks));
      // If a vehicle's span is constrained, its start/end cumuls must be
      // instantiated.
      for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
        if (!spans[vehicle] && !total_slacks[vehicle]) continue;
        if (spans[vehicle]) {
          AddVariableTargetToFinalizer(spans[vehicle],
                                       std::numeric_limits<int64_t>::min());
        }
        AddVariableTargetToFinalizer(dimension->CumulVar(End(vehicle)),
                                     std::numeric_limits<int64_t>::min());
        AddVariableTargetToFinalizer(dimension->CumulVar(Start(vehicle)),
                                     std::numeric_limits<int64_t>::max());
      }
      // Add costs of variables.
      for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
        if (span_costs[vehicle] == 0 && slack_costs[vehicle] == 0) continue;
        DCHECK(total_slacks[vehicle] != nullptr);
        IntVar* const slack_amount =
            solver_
                ->MakeProd(vehicle_route_considered_[vehicle],
                           total_slacks[vehicle])
                ->Var();
        const int64_t slack_cost_coefficient =
            CapAdd(slack_costs[vehicle], span_costs[vehicle]);
        IntVar* const slack_cost =
            solver_->MakeProd(slack_amount, slack_cost_coefficient)->Var();
        cost_elements.push_back(slack_cost);
        AddWeightedVariableMinimizedByFinalizer(slack_amount,
                                                slack_cost_coefficient);
      }
      if (dimension->HasSoftSpanUpperBounds()) {
        for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
          const auto bound_cost =
              dimension->GetSoftSpanUpperBoundForVehicle(vehicle);
          if (bound_cost.cost == 0 ||
              bound_cost.bound == std::numeric_limits<int64_t>::max())
            continue;
          DCHECK(spans[vehicle] != nullptr);
          // Additional cost is vehicle_cost_considered_[vehicle] *
          // max(0, spans[vehicle] - bound_cost.bound) * bound_cost.cost.
          IntVar* const span_violation_amount =
              solver_
                  ->MakeProd(
                      vehicle_route_considered_[vehicle],
                      solver_->MakeMax(
                          solver_->MakeSum(spans[vehicle], -bound_cost.bound),
                          0))
                  ->Var();
          IntVar* const span_violation_cost =
              solver_->MakeProd(span_violation_amount, bound_cost.cost)->Var();
          cost_elements.push_back(span_violation_cost);
          AddWeightedVariableMinimizedByFinalizer(span_violation_amount,
                                                  bound_cost.cost);
        }
      }
      if (dimension->HasQuadraticCostSoftSpanUpperBounds()) {
        for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
          const auto bound_cost =
              dimension->GetQuadraticCostSoftSpanUpperBoundForVehicle(vehicle);
          if (bound_cost.cost == 0 ||
              bound_cost.bound == std::numeric_limits<int64_t>::max())
            continue;
          DCHECK(spans[vehicle] != nullptr);
          // Additional cost is vehicle_cost_considered_[vehicle] *
          // max(0, spans[vehicle] - bound_cost.bound)^2 * bound_cost.cost.
          IntExpr* max0 = solver_->MakeMax(
              solver_->MakeSum(spans[vehicle], -bound_cost.bound), 0);
          IntVar* const squared_span_violation_amount =
              solver_
                  ->MakeProd(vehicle_route_considered_[vehicle],
                             solver_->MakeSquare(max0))
                  ->Var();
          IntVar* const span_violation_cost =
              solver_->MakeProd(squared_span_violation_amount, bound_cost.cost)
                  ->Var();
          cost_elements.push_back(span_violation_cost);
          AddWeightedVariableMinimizedByFinalizer(squared_span_violation_amount,
                                                  bound_cost.cost);
        }
      }
    }
  }
  // Penalty costs
  for (DisjunctionIndex i(0); i < disjunctions_.size(); ++i) {
    IntVar* penalty_var = CreateDisjunction(i);
    if (penalty_var != nullptr) {
      cost_elements.push_back(penalty_var);
    }
  }
  // Soft cumul lower/upper bound costs
  for (const RoutingDimension* dimension : dimensions_) {
    dimension->SetupCumulVarSoftLowerBoundCosts(&cost_elements);
    dimension->SetupCumulVarSoftUpperBoundCosts(&cost_elements);
    dimension->SetupCumulVarPiecewiseLinearCosts(&cost_elements);
  }
  // Same vehicle costs
  for (int i = 0; i < same_vehicle_costs_.size(); ++i) {
    cost_elements.push_back(CreateSameVehicleCost(i));
  }
  // Energy costs
  for (const auto& [force_distance, costs] : force_distance_to_energy_costs_) {
    std::vector<IntVar*> energy_costs(vehicles_, nullptr);

    for (int v = 0; v < vehicles_; ++v) {
      const int64_t cost_ub = costs[v].IsNull() ? 0 : kint64max;
      energy_costs[v] = solver_->MakeIntVar(0, cost_ub);
      cost_elements.push_back(energy_costs[v]);
      AddWeightedVariableMinimizedByFinalizer(
          energy_costs[v], std::max(costs[v].cost_per_unit_below_threshold,
                                    costs[v].cost_per_unit_above_threshold));
    }

    const RoutingDimension* force_dimension =
        GetMutableDimension(force_distance.first);
    DCHECK_NE(force_dimension, nullptr);
    const RoutingDimension* distance_dimension =
        GetMutableDimension(force_distance.second);
    DCHECK_NE(distance_dimension, nullptr);
    if (force_dimension == nullptr || distance_dimension == nullptr) continue;

    Solver::PathEnergyCostConstraintSpecification specification{
        .nexts = Nexts(),
        .paths = VehicleVars(),
        .forces = force_dimension->cumuls(),
        .distances = distance_dimension->transits(),
        .path_energy_costs = costs,
        .path_used_when_empty = vehicle_used_when_empty_,
        .path_starts = paths_metadata_.Starts(),
        .path_ends = paths_metadata_.Ends(),
        .costs = std::move(energy_costs),
    };

    solver_->AddConstraint(
        solver_->MakePathEnergyCostConstraint(std::move(specification)));
  }
  for (IntVar* route_cost_var : route_cost_vars) {
    cost_elements.push_back(route_cost_var);
  }
  // cost_ is the sum of cost_elements.
  cost_ = solver_->MakeSum(cost_elements)->Var();
  cost_->set_name("Cost");

  // Pickup-delivery precedences
  std::vector<std::pair<int, int>> pickup_delivery_precedences;
  for (const auto& [pickups, deliveries] : pickup_delivery_pairs_) {
    DCHECK(!pickups.empty() && !deliveries.empty());
    for (int pickup : pickups) {
      for (int delivery : deliveries) {
        pickup_delivery_precedences.emplace_back(pickup, delivery);
      }
    }
  }
  std::vector<int> lifo_vehicles;
  std::vector<int> fifo_vehicles;
  for (int i = 0; i < vehicles_; ++i) {
    switch (vehicle_pickup_delivery_policy_[i]) {
      case PICKUP_AND_DELIVERY_NO_ORDER:
        break;
      case PICKUP_AND_DELIVERY_LIFO:
        lifo_vehicles.push_back(Start(i));
        break;
      case PICKUP_AND_DELIVERY_FIFO:
        fifo_vehicles.push_back(Start(i));
        break;
    }
  }
  solver_->AddConstraint(solver_->MakePathPrecedenceConstraint(
      nexts_, pickup_delivery_precedences, lifo_vehicles, fifo_vehicles));

  // Add ordered activity group constraints.
  for (const auto& disjunctions : ordered_activity_groups_) {
    if (disjunctions.size() <= 1) continue;
    IntVar* prev_active_var = nullptr;
    for (const DisjunctionIndex disjunction_index : disjunctions) {
      const std::vector<int64_t>& node_indices =
          GetDisjunctionNodeIndices(disjunction_index);
      std::vector<IntVar*> active_vars;
      active_vars.reserve(node_indices.size());
      for (const int node : node_indices) {
        active_vars.push_back(ActiveVar(node));
      }
      IntVar* const sum = solver_->MakeSum(active_vars)->Var();
      IntVar* active_var = solver_->MakeBoolVar();
      solver_->AddConstraint(solver_->MakeIsGreaterOrEqualCstCt(
          sum, GetDisjunctionMaxCardinality(disjunction_index), active_var));
      if (prev_active_var != nullptr) {
        solver_->AddConstraint(
            solver_->MakeLessOrEqual(active_var, prev_active_var));
      }
      prev_active_var = active_var;
    }
  }

  // Detect constraints
  enable_deep_serialization_ = false;
  std::unique_ptr<RoutingModelInspector> inspector(
      new RoutingModelInspector(this));
  solver_->Accept(inspector.get());
  enable_deep_serialization_ = true;

  for (const RoutingDimension* const dimension : dimensions_) {
    // Dimension path precedences, discovered by model inspection (which must be
    // performed before adding path transit precedences).
    const ReverseArcListGraph<int, int>& graph =
        dimension->GetPathPrecedenceGraph();
    std::vector<std::pair<int, int>> path_precedences;
    for (const auto tail : graph.AllNodes()) {
      for (const auto head : graph[tail]) {
        path_precedences.emplace_back(tail, head);
      }
    }
    if (!path_precedences.empty()) {
      solver_->AddConstraint(solver_->MakePathTransitPrecedenceConstraint(
          nexts_, dimension->transits(), path_precedences));
    }

    // Dimension node precedences.
    for (const auto [first_node, second_node, offset, performed_constraint] :
         dimension->GetNodePrecedences()) {
      IntExpr* const nodes_are_selected =
          solver_->MakeMin(active_[first_node], active_[second_node]);
      IntExpr* const cumul_difference = solver_->MakeDifference(
          dimension->CumulVar(second_node), dimension->CumulVar(first_node));
      IntVar* const cumul_difference_is_ge_offset =
          solver_->MakeIsGreaterOrEqualCstVar(cumul_difference, offset);
      // Forces the implication: both nodes are active => cumul difference
      // constraint is active.
      solver_->AddConstraint(solver_->MakeLessOrEqual(
          nodes_are_selected->Var(), cumul_difference_is_ge_offset));
      using PerformedConstraint =
          RoutingDimension::NodePrecedence::PerformedConstraint;
      switch (performed_constraint) {
        case PerformedConstraint::kFirstAndSecondIndependent:
          break;
        case PerformedConstraint::kSecondImpliesFirst:
          solver_->AddConstraint(solver_->MakeGreaterOrEqual(
              active_[first_node], active_[second_node]));
          break;
        case PerformedConstraint::kFirstImpliesSecond:
          solver_->AddConstraint(solver_->MakeGreaterOrEqual(
              active_[second_node], active_[first_node]));
          break;
        case PerformedConstraint::kFirstAndSecondEqual:
          solver_->AddConstraint(
              solver_->MakeEquality(active_[first_node], active_[second_node]));
      }
    }
  }

  if (!resource_groups_.empty()) {
    DCHECK_EQ(resource_vars_.size(), resource_groups_.size());
    for (int rg = 0; rg < resource_groups_.size(); ++rg) {
      const auto& resource_group = resource_groups_[rg];
      const int max_resource_index = resource_group->Size() - 1;
      std::vector<IntVar*>& vehicle_res_vars = resource_vars_[rg];
      for (IntVar* res_var : vehicle_res_vars) {
        res_var->SetMax(max_resource_index);
      }
      solver_->AddConstraint(MakeResourceConstraint(resource_group.get(),
                                                    &vehicle_res_vars, this));
    }
  }

  DetectImplicitPickupAndDeliveries();

  // Store the local/global cumul optimizers, along with their offsets.
  StoreDimensionCumulOptimizers(parameters);

  // Keep this out of SetupSearch as this contains static search objects.
  // This will allow calling SetupSearch multiple times with different search
  // parameters.
  CreateNeighborhoodOperators(parameters);
  CreateFirstSolutionDecisionBuilders(parameters);
  monitors_before_setup_ = monitors_.size();
  // This must set here as SetupSearch needs to be aware of previously existing
  // monitors.
  monitors_after_setup_ = monitors_.size();
  SetupSearch(parameters);
}

void RoutingModel::AddSearchMonitor(SearchMonitor* const monitor) {
  monitors_.push_back(monitor);
  secondary_ls_monitors_.push_back(monitor);
}

void RoutingModel::AddRestoreDimensionValuesResetCallback(
    std::function<void()> callback) {
  if (callback) {
    if (restore_dimension_values_reset_callbacks_.empty()) {
      AddEnterSearchCallback([this]() {
        for (const auto& callback : restore_dimension_values_reset_callbacks_) {
          callback();
        }
      });
    }
    restore_dimension_values_reset_callbacks_.push_back(std::move(callback));
  }
}

namespace {
class EnterSearchMonitor : public SearchMonitor {
 public:
  EnterSearchMonitor(Solver* solver, std::function<void()> callback)
      : SearchMonitor(solver), callback_(std::move(callback)) {}
  void EnterSearch() override { callback_(); }
  void Install() override { ListenToEvent(Solver::MonitorEvent::kEnterSearch); }

 private:
  std::function<void()> callback_;
};

class AtSolutionCallbackMonitor : public SearchMonitor {
 public:
  AtSolutionCallbackMonitor(Solver* solver, std::function<void()> callback,
                            bool track_unchecked_neighbors)
      : SearchMonitor(solver),
        callback_(std::move(callback)),
        track_unchecked_neighbors_(track_unchecked_neighbors) {}
  bool AtSolution() override {
    callback_();
    return false;
  }
  void AcceptUncheckedNeighbor() override { AtSolution(); }
  void Install() override {
    ListenToEvent(Solver::MonitorEvent::kAtSolution);
    if (track_unchecked_neighbors_) {
      ListenToEvent(Solver::MonitorEvent::kAcceptUncheckedNeighbor);
    }
  }

 private:
  std::function<void()> callback_;
  const bool track_unchecked_neighbors_;
};
}  // namespace

void RoutingModel::AddEnterSearchCallback(std::function<void()> callback) {
  EnterSearchMonitor* const monitor = solver_->RevAlloc(
      new EnterSearchMonitor(solver_.get(), std::move(callback)));
  AddSearchMonitor(monitor);
}

void RoutingModel::AddAtSolutionCallback(std::function<void()> callback,
                                         bool track_unchecked_neighbors) {
  AtSolutionCallbackMonitor* const monitor =
      solver_->RevAlloc(new AtSolutionCallbackMonitor(
          solver_.get(), std::move(callback), track_unchecked_neighbors));
  at_solution_monitors_.push_back(monitor);
  AddSearchMonitor(monitor);
}

const Assignment* RoutingModel::Solve(const Assignment* assignment) {
  return SolveFromAssignmentWithParameters(assignment,
                                           DefaultRoutingSearchParameters());
}

const Assignment* RoutingModel::SolveWithParameters(
    const RoutingSearchParameters& parameters,
    std::vector<const Assignment*>* solutions) {
  return SolveFromAssignmentWithParameters(nullptr, parameters, solutions);
}

namespace {
absl::Duration GetLnsTimeLimit(const RoutingSearchParameters& parameters) {
  if (!parameters.has_lns_time_limit()) return absl::InfiniteDuration();
  return util_time::DecodeGoogleApiProto(parameters.lns_time_limit()).value();
}
}  // namespace

namespace {
void MakeAllUnperformedInAssignment(const RoutingModel* model,
                                    Assignment* assignment) {
  assignment->Clear();
  for (int i = 0; i < model->Nexts().size(); ++i) {
    if (!model->IsStart(i)) {
      assignment->Add(model->NextVar(i))->SetValue(i);
    }
  }
  for (int vehicle = 0; vehicle < model->vehicles(); ++vehicle) {
    assignment->Add(model->NextVar(model->Start(vehicle)))
        ->SetValue(model->End(vehicle));
  }
}
}  //  namespace

bool RoutingModel::CheckIfAssignmentIsFeasible(const Assignment& assignment,
                                               bool call_at_solution_monitors) {
  tmp_assignment_->CopyIntersection(&assignment);
  std::vector<SearchMonitor*> monitors = call_at_solution_monitors
                                             ? at_solution_monitors_
                                             : std::vector<SearchMonitor*>();
  monitors.push_back(collect_one_assignment_);
  monitors.push_back(GetOrCreateLimit());
  solver_->Solve(restore_tmp_assignment_, monitors);
  return collect_one_assignment_->solution_count() == 1;
}

bool RoutingModel::AppendAssignmentIfFeasible(
    const Assignment& assignment,
    std::vector<std::unique_ptr<Assignment>>* assignments,
    bool call_at_solution_monitors) {
  if (CheckIfAssignmentIsFeasible(assignment, call_at_solution_monitors)) {
    assignments->push_back(std::make_unique<Assignment>(solver_.get()));
    assignments->back()->Copy(collect_one_assignment_->solution(0));
    return true;
  }
  return false;
}

void RoutingModel::LogSolution(const RoutingSearchParameters& parameters,
                               absl::string_view description,
                               int64_t solution_cost, int64_t start_time_ms) {
  const std::string memory_str = MemoryUsage();
  const double cost_scaling_factor = parameters.log_cost_scaling_factor();
  const double cost_offset = parameters.log_cost_offset();
  const std::string cost_string =
      cost_scaling_factor == 1.0 && cost_offset == 0.0
          ? absl::StrCat(solution_cost)
          : absl::StrFormat(
                "%d (%.8lf)", solution_cost,
                cost_scaling_factor * (solution_cost + cost_offset));
  LOG(INFO) << absl::StrFormat(
      "%s (%s, time = %d ms, memory used = %s)", description, cost_string,
      solver_->wall_time() - start_time_ms, memory_str);
}

const Assignment* RoutingModel::SolveFromAssignmentWithParameters(
    const Assignment* assignment, const RoutingSearchParameters& parameters,
    std::vector<const Assignment*>* solutions) {
  return SolveFromAssignmentsWithParameters({assignment}, parameters,
                                            solutions);
}

const Assignment* RoutingModel::FastSolveFromAssignmentWithParameters(
    const Assignment* assignment,
    const RoutingSearchParameters& search_parameters, bool check_solution_in_cp,
    absl::flat_hash_set<IntVar*>* touched) {
  if (search_parameters.local_search_metaheuristic() !=
          LocalSearchMetaheuristic::GREEDY_DESCENT &&
      search_parameters.local_search_metaheuristic() !=
          LocalSearchMetaheuristic::AUTOMATIC) {
    LOG(DFATAL) << "local_search_metaheuristic value unsupported: "
                << search_parameters.local_search_metaheuristic();
    return nullptr;
  }
  const int64_t start_time_ms = solver_->wall_time();
  QuietCloseModelWithParameters(search_parameters);
  UpdateSearchFromParametersIfNeeded(search_parameters);

  if (status_ == RoutingSearchStatus::ROUTING_INVALID) return nullptr;
  status_ = RoutingSearchStatus::ROUTING_NOT_SOLVED;
  if (assignment == nullptr) return nullptr;
  limit_->UpdateLimits(
      GetTimeLimit(search_parameters), std::numeric_limits<int64_t>::max(),
      std::numeric_limits<int64_t>::max(), search_parameters.solution_limit());
  std::vector<SearchMonitor*> monitors = {metaheuristic_};
  if (search_log_ != nullptr) monitors.push_back(search_log_);
  Assignment* solution = solver()->RunUncheckedLocalSearch(
      assignment,
      GetOrCreateLocalSearchFilterManager(search_parameters,
                                          {/*filter_objective=*/true,
                                           /*filter_with_cp_solver=*/false}),
      primary_ls_operator_, monitors, limit_, touched);
  const absl::Duration elapsed_time =
      absl::Milliseconds(solver_->wall_time() - start_time_ms);
  if (solution != nullptr) {
    if (!check_solution_in_cp ||
        CheckIfAssignmentIsFeasible(*solution,
                                    /*call_at_solution_monitors=*/false)) {
      status_ = RoutingSearchStatus::ROUTING_SUCCESS;
    }
  }
  if (status_ != RoutingSearchStatus::ROUTING_SUCCESS) {
    if (elapsed_time >= GetTimeLimit(search_parameters)) {
      status_ = RoutingSearchStatus::ROUTING_FAIL_TIMEOUT;
    } else {
      status_ = RoutingSearchStatus::ROUTING_FAIL;
    }
  }
  return solution;
}

const Assignment* RoutingModel::SolveFromAssignmentsWithParameters(
    const std::vector<const Assignment*>& assignments,
    const RoutingSearchParameters& parameters,
    std::vector<const Assignment*>* solutions) {
  const int64_t start_time_ms = solver_->wall_time();
  QuietCloseModelWithParameters(parameters);
  UpdateSearchFromParametersIfNeeded(parameters);
  if (solutions != nullptr) solutions->clear();
  if (status_ == RoutingSearchStatus::ROUTING_INVALID) {
    return nullptr;
  }
  status_ = RoutingSearchStatus::ROUTING_NOT_SOLVED;

  // Detect infeasibilities at the root of the search tree.
  if (!solver_->CheckConstraint(solver_->MakeTrueConstraint())) {
    status_ = RoutingSearchStatus::ROUTING_INFEASIBLE;
    return nullptr;
  }

  const bool perform_secondary_ls =
      GetTimeLimit(parameters) != absl::InfiniteDuration() &&
      parameters.secondary_ls_time_limit_ratio() > 0;
  const auto update_time_limits =
      [this, start_time_ms, perform_secondary_ls,
       &parameters](bool leave_secondary_solve_buffer = true) {
        const absl::Duration elapsed_time =
            absl::Milliseconds(solver_->wall_time() - start_time_ms);
        const absl::Duration time_left =
            GetTimeLimit(parameters) - elapsed_time;

        if (time_left < absl::ZeroDuration()) return false;

        const absl::Duration secondary_solve_buffer =
            !leave_secondary_solve_buffer || !perform_secondary_ls
                ? absl::ZeroDuration()
                : parameters.secondary_ls_time_limit_ratio() * time_left;
        const absl::Duration time_limit = time_left - secondary_solve_buffer;
        limit_->UpdateLimits(time_limit, std::numeric_limits<int64_t>::max(),
                             std::numeric_limits<int64_t>::max(),
                             parameters.solution_limit());
        DCHECK_NE(ls_limit_, nullptr);
        ls_limit_->UpdateLimits(time_limit, std::numeric_limits<int64_t>::max(),
                                std::numeric_limits<int64_t>::max(), 1);
        // TODO(user): Come up with a better formula. Ideally this should be
        // calibrated in the first solution strategies.
        time_buffer_ = std::min(absl::Seconds(1), time_limit * 0.05);
        return true;
      };
  if (!update_time_limits()) {
    status_ = RoutingSearchStatus::ROUTING_FAIL_TIMEOUT;
    return nullptr;
  }
  lns_limit_->UpdateLimits(
      GetLnsTimeLimit(parameters), std::numeric_limits<int64_t>::max(),
      std::numeric_limits<int64_t>::max(), std::numeric_limits<int64_t>::max());
  // NOTE: Allow more time for the first solution's scheduling, since if it
  // fails, we won't have anything to build upon.
  // We set this time limit based on whether local/global dimension optimizers
  // are used in the finalizer to avoid going over the general time limit.
  // TODO(user): Adapt this when absolute timeouts are given to the model.
  const int time_limit_shares = 1 + !global_dimension_optimizers_.empty() +
                                !local_dimension_optimizers_.empty();
  const absl::Duration first_solution_lns_time_limit =
      std::max(GetTimeLimit(parameters) / time_limit_shares,
               GetLnsTimeLimit(parameters));
  first_solution_lns_limit_->UpdateLimits(
      first_solution_lns_time_limit, std::numeric_limits<int64_t>::max(),
      std::numeric_limits<int64_t>::max(), std::numeric_limits<int64_t>::max());

  std::vector<std::unique_ptr<Assignment>> solution_pool;
  std::vector<const Assignment*> first_solution_assignments;
  for (const Assignment* assignment : assignments) {
    if (assignment != nullptr) first_solution_assignments.push_back(assignment);
  }
  local_optimum_reached_ = false;
  objective_lower_bound_ = kint64min;
  if (parameters.use_cp() == BOOL_TRUE) {
    const auto run_secondary_ls = [this, perform_secondary_ls,
                                   &update_time_limits]() {
      if (collect_assignments_->has_solution() && perform_secondary_ls &&
          update_time_limits(/*leave_secondary_solve_buffer=*/false)) {
        assignment_->CopyIntersection(
            collect_assignments_->last_solution_or_null());
        solver_->Solve(secondary_ls_db_, secondary_ls_monitors_);
      }
    };
    if (first_solution_assignments.empty()) {
      bool solution_found = false;
      if (IsMatchingModel()) {
        Assignment matching(solver_.get());
        // TODO(user): Pass time limits to the flow.
        if (SolveMatchingModel(&matching, parameters) &&
            update_time_limits(/*leave_secondary_solve_buffer=*/false) &&
            AppendAssignmentIfFeasible(matching, &solution_pool)) {
          if (parameters.log_search()) {
            LogSolution(parameters, "Min-Cost Flow Solution",
                        solution_pool.back()->ObjectiveValue(), start_time_ms);
          }
          solution_found = true;
          local_optimum_reached_ = true;
        }
      }
      if (!solution_found) {
        // Build trivial solutions to which we can come back too in case the
        // solver does not manage to build something better.
        Assignment unperformed(solver_.get());
        MakeAllUnperformedInAssignment(this, &unperformed);
        if (update_time_limits(/*leave_secondary_solve_buffer=*/false) &&
            AppendAssignmentIfFeasible(unperformed, &solution_pool, false) &&
            parameters.log_search()) {
          LogSolution(parameters, "All Unperformed Solution",
                      solution_pool.back()->ObjectiveValue(), start_time_ms);
        }
        local_optimum_reached_ = false;
        if (update_time_limits()) {
          solver_->Solve(solve_db_, monitors_);
          run_secondary_ls();
        }
      }
    } else {
      for (const Assignment* assignment : first_solution_assignments) {
        assignment_->CopyIntersection(assignment);
        solver_->Solve(improve_db_, monitors_);
        run_secondary_ls();
        if (collect_assignments_->solution_count() >= 1 ||
            !update_time_limits()) {
          break;
        }
      }
      if (collect_assignments_->solution_count() == 0 && update_time_limits() &&
          hint_ != nullptr) {
        solver_->Solve(solve_db_, monitors_);
        run_secondary_ls();
      }
    }
  }

  const SolutionCollector* const solution_collector =
      collect_secondary_ls_assignments_->has_solution()
          ? collect_secondary_ls_assignments_
          : collect_assignments_;

  if (update_time_limits(/*leave_secondary_solve_buffer=*/false) &&
      (parameters.use_cp_sat() == BOOL_TRUE ||
       parameters.use_generalized_cp_sat() == BOOL_TRUE ||
       (parameters.fallback_to_cp_sat_size_threshold() >= Size() &&
        !solution_collector->has_solution() && solution_pool.empty()))) {
    VLOG(1) << "Solving with CP-SAT";
    Assignment* const cp_solution = solution_collector->last_solution_or_null();
    Assignment sat_solution(solver_.get());
    if (SolveModelWithSat(this, &search_stats_, parameters, cp_solution,
                          &sat_solution) &&
        update_time_limits(/*leave_secondary_solve_buffer=*/false) &&
        AppendAssignmentIfFeasible(sat_solution, &solution_pool)) {
      if (parameters.log_search()) {
        LogSolution(parameters, "SAT", solution_pool.back()->ObjectiveValue(),
                    start_time_ms);
      }
      local_optimum_reached_ = true;
      if (sat_solution.HasObjective()) {
        objective_lower_bound_ =
            std::max(objective_lower_bound_, sat_solution.ObjectiveValue());
      }
    }
  }
  VLOG(1) << "Objective lower bound: " << objective_lower_bound_;
  const absl::Duration elapsed_time =
      absl::Milliseconds(solver_->wall_time() - start_time_ms);

  if (solution_collector->has_solution() || !solution_pool.empty()) {
    status_ = local_optimum_reached_
                  ? RoutingSearchStatus::ROUTING_SUCCESS
                  : RoutingSearchStatus::
                        ROUTING_PARTIAL_SUCCESS_LOCAL_OPTIMUM_NOT_REACHED;
    if (solutions != nullptr) {
      std::vector<Assignment*> temp_solutions;
      for (int i = 0; i < solution_collector->solution_count(); ++i) {
        temp_solutions.push_back(
            solver_->MakeAssignment(solution_collector->solution(i)));
      }
      for (const auto& solution : solution_pool) {
        if (temp_solutions.empty() ||
            solution->ObjectiveValue() <
                temp_solutions.back()->ObjectiveValue()) {
          temp_solutions.push_back(solver_->MakeAssignment(solution.get()));
        }
      }
      // By construction, the last assignment in 'temp_solutions' necessarily
      // has the best objective value.
      DCHECK(!temp_solutions.empty());
      const int64_t min_objective_value =
          temp_solutions.back()->ObjectiveValue();

      if (temp_solutions.size() < parameters.number_of_solutions_to_collect() &&
          solution_collector != collect_assignments_ &&
          collect_assignments_->has_solution()) {
        // Since the secondary LS is run starting from the primary LS's last
        // assignment, and that it will be the first solution collected in the
        // secondary search, we already have it in the results.
        DCHECK_EQ(*collect_assignments_->last_solution_or_null(),
                  *temp_solutions[0]);
        // Add the remaining solutions from the original assignment collector.
        const size_t num_solutions = collect_assignments_->solution_count();
        const int num_solutions_to_add = std::min(
            parameters.number_of_solutions_to_collect() - solutions->size(),
            num_solutions - 1);
        for (int i = num_solutions_to_add; i > 0; --i) {
          solutions->push_back(solver_->MakeAssignment(
              collect_assignments_->solution(num_solutions - 1 - i)));
          DCHECK_GE(solutions->back()->ObjectiveValue(), min_objective_value);
        }
      }
      // Keep 'solutions' sorted from worst to best solution by appending
      // temp_solutions in the end.
      solutions->insert(solutions->end(), temp_solutions.begin(),
                        temp_solutions.end());
      if (min_objective_value <= objective_lower_bound_) {
        status_ = RoutingSearchStatus::ROUTING_OPTIMAL;
      }
      return solutions->back();
    }
    Assignment* best_assignment = solution_collector->last_solution_or_null();
    for (const auto& solution : solution_pool) {
      if (best_assignment == nullptr ||
          solution->ObjectiveValue() < best_assignment->ObjectiveValue()) {
        best_assignment = solution.get();
      }
    }
    if (best_assignment->ObjectiveValue() <= objective_lower_bound_) {
      status_ = RoutingSearchStatus::ROUTING_OPTIMAL;
    }
    return solver_->MakeAssignment(best_assignment);
  } else {
    if (elapsed_time >= GetTimeLimit(parameters)) {
      status_ = RoutingSearchStatus::ROUTING_FAIL_TIMEOUT;
    } else {
      status_ = RoutingSearchStatus::ROUTING_FAIL;
    }
    return nullptr;
  }
}

const Assignment* RoutingModel::SolveWithIteratedLocalSearch(
    const RoutingSearchParameters& parameters) {
  DCHECK(parameters.use_iterated_local_search());

  if (nodes() == 0) {
    return nullptr;
  }

  const int64_t start_time_ms = solver_->wall_time();
  QuietCloseModelWithParameters(parameters);
  UpdateSearchFromParametersIfNeeded(parameters);
  if (status_ == RoutingSearchStatus::ROUTING_INVALID) {
    return nullptr;
  }

  // Build an initial solution.
  solver_->Solve(solve_db_, monitors_);
  int64_t explored_solutions = solver_->solutions();

  Assignment* best_solution = collect_assignments_->last_solution_or_null();
  if (!best_solution) {
    return nullptr;
  }

  // The solution that tracks the search trajectory.
  Assignment* last_accepted_solution = solver_->MakeAssignment(best_solution);

  LocalSearchFilterManager* filter_manager =
      GetOrCreateLocalSearchFilterManager(parameters,
                                          {/*filter_objective=*/false,
                                           /*filter_with_cp_solver=*/false});

  std::mt19937 rnd(/*seed=*/0);

  DecisionBuilder* perturbation_db = MakePerturbationDecisionBuilder(
      parameters, this, &rnd, last_accepted_solution,
      [this]() { return CheckLimit(time_buffer_); }, filter_manager);

  // TODO(user): This lambda can probably be refactored into a function as a
  // similar version in used in another place.
  const auto update_time_limits = [this, start_time_ms, &parameters]() {
    const absl::Duration elapsed_time =
        absl::Milliseconds(solver_->wall_time() - start_time_ms);
    const absl::Duration time_left = GetTimeLimit(parameters) - elapsed_time;
    if (time_left < absl::ZeroDuration()) {
      return false;
    }
    limit_->UpdateLimits(time_left, std::numeric_limits<int64_t>::max(),
                         std::numeric_limits<int64_t>::max(),
                         parameters.solution_limit());
    DCHECK_NE(ls_limit_, nullptr);
    ls_limit_->UpdateLimits(time_left, std::numeric_limits<int64_t>::max(),
                            std::numeric_limits<int64_t>::max(), 1);
    // TODO(user): Come up with a better formula. Ideally this should be
    // calibrated in the first solution strategies.
    time_buffer_ = std::min(absl::Seconds(1), time_left * 0.05);
    return true;
  };

  const IteratedLocalSearchParameters& ils_parameters =
      parameters.iterated_local_search_parameters();

  const absl::Duration final_duration =
      !parameters.has_time_limit()
          ? absl::InfiniteDuration()
          : util_time::DecodeGoogleApiProto(parameters.time_limit()).value();

  NeighborAcceptanceCriterion::SearchState final_search_state = {
      final_duration, parameters.solution_limit()};

  std::unique_ptr<NeighborAcceptanceCriterion> reference_acceptance_criterion =
      MakeNeighborAcceptanceCriterion(
          *this, ils_parameters.reference_solution_acceptance_strategy(),
          final_search_state, &rnd);

  std::unique_ptr<NeighborAcceptanceCriterion> best_acceptance_criterion =
      MakeNeighborAcceptanceCriterion(
          *this, ils_parameters.best_solution_acceptance_strategy(),
          final_search_state, &rnd);

  const bool improve_perturbed_solution =
      ils_parameters.improve_perturbed_solution();

  while (update_time_limits() &&
         explored_solutions < parameters.solution_limit()) {
    solver_->Solve(perturbation_db, monitors_);
    explored_solutions += solver_->solutions();

    Assignment* neighbor_solution =
        collect_assignments_->last_solution_or_null();
    if (!neighbor_solution) {
      continue;
    }

    if (improve_perturbed_solution && update_time_limits()) {
      assignment_->CopyIntersection(neighbor_solution);

      solver_->Solve(improve_db_, monitors_);
      explored_solutions += solver_->solutions();

      neighbor_solution = collect_assignments_->last_solution_or_null();
      if (!neighbor_solution) {
        continue;
      }
    }

    absl::Duration elapsed_time =
        absl::Milliseconds(solver_->wall_time() - start_time_ms);

    if (best_acceptance_criterion->Accept({elapsed_time, explored_solutions},
                                          neighbor_solution, best_solution)) {
      best_solution->CopyIntersection(neighbor_solution);
    }

    if (reference_acceptance_criterion->Accept(
            {elapsed_time, explored_solutions}, neighbor_solution,
            last_accepted_solution)) {
      // Note that the perturbation_db is using last_accepted_solution as
      // reference assignment. By updating last_accepted_solution here we thus
      // also keep the perturbation_db reference assignment up to date.
      last_accepted_solution->CopyIntersection(neighbor_solution);
    }
  }

  return best_solution;
}

void RoutingModel::SetAssignmentFromOtherModelAssignment(
    Assignment* target_assignment, const RoutingModel* source_model,
    const Assignment* source_assignment) {
  const int size = Size();
  DCHECK_EQ(size, source_model->Size());
  CHECK_EQ(target_assignment->solver(), solver_.get());

  if (CostsAreHomogeneousAcrossVehicles()) {
    SetAssignmentFromAssignment(target_assignment, Nexts(), source_assignment,
                                source_model->Nexts());
  } else {
    std::vector<IntVar*> source_vars(size + size + vehicles_);
    std::vector<IntVar*> target_vars(size + size + vehicles_);
    for (int index = 0; index < size; index++) {
      source_vars[index] = source_model->NextVar(index);
      target_vars[index] = NextVar(index);
    }
    for (int index = 0; index < size + vehicles_; index++) {
      source_vars[size + index] = source_model->VehicleVar(index);
      target_vars[size + index] = VehicleVar(index);
    }
    SetAssignmentFromAssignment(target_assignment, target_vars,
                                source_assignment, source_vars);
  }

  target_assignment->AddObjective(cost_);
}

SubSolverStatistics RoutingModel::GetSubSolverStatistics() const {
  SubSolverStatistics stats;
  stats.set_num_glop_calls_in_lp_scheduling(
      search_stats_.num_glop_calls_in_lp_scheduling);
  stats.set_num_cp_sat_calls_in_lp_scheduling(
      search_stats_.num_cp_sat_calls_in_lp_scheduling);
  stats.set_num_min_cost_flow_calls(search_stats_.num_min_cost_flow_calls);
  return stats;
}

// Computing a lower bound to the cost of a vehicle routing problem solving a
// a linear assignment problem (minimum-cost perfect bipartite matching).
// A bipartite graph is created with left nodes representing the nodes of the
// routing problem and right nodes representing possible node successors; an
// arc between a left node l and a right node r is created if r can be the
// node following l in a route (Next(l) = r); the cost of the arc is the transit
// cost between l and r in the routing problem.
// This is a lower bound given the solution to assignment problem does not
// necessarily produce a (set of) closed route(s) from a starting node to an
// ending node.
int64_t RoutingModel::ComputeLowerBound() {
  if (!closed_) {
    LOG(WARNING) << "Non-closed model not supported.";
    return 0;
  }
  if (!CostsAreHomogeneousAcrossVehicles()) {
    LOG(WARNING) << "Non-homogeneous vehicle costs not supported";
    return 0;
  }
  if (!disjunctions_.empty()) {
    LOG(WARNING)
        << "Node disjunction constraints or optional nodes not supported.";
    return 0;
  }
  const int num_nodes = Size() + vehicles_;
  Graph graph(2 * num_nodes, num_nodes * num_nodes);
  LinearSumAssignment<Graph> linear_sum_assignment(graph, num_nodes);
  // Adding arcs for non-end nodes, based on possible values of next variables.
  // Left nodes in the bipartite are indexed from 0 to num_nodes - 1; right
  // nodes are indexed from num_nodes to 2 * num_nodes - 1.
  for (int tail = 0; tail < Size(); ++tail) {
    std::unique_ptr<IntVarIterator> iterator(
        nexts_[tail]->MakeDomainIterator(false));
    for (const int64_t head : InitAndGetValues(iterator.get())) {
      // Given there are no disjunction constraints, a node cannot point to
      // itself. Doing this explicitly given that outside the search,
      // propagation hasn't removed this value from next variables yet.
      if (head == tail) {
        continue;
      }
      // The index of a right node in the bipartite graph is the index
      // of the successor offset by the number of nodes.
      const GraphArcIndex arc = graph.AddArc(tail, num_nodes + head);
      const ::CostValue cost = GetHomogeneousCost(tail, head);
      linear_sum_assignment.SetArcCost(arc, cost);
    }
  }
  // The linear assignment library requires having as many left and right nodes.
  // Therefore we are creating fake assignments for end nodes, forced to point
  // to the equivalent start node with a cost of 0.
  for (int tail = Size(); tail < num_nodes; ++tail) {
    const GraphArcIndex arc =
        graph.AddArc(tail, num_nodes + Start(tail - Size()));
    linear_sum_assignment.SetArcCost(arc, 0);
  }
  if (linear_sum_assignment.ComputeAssignment()) {
    return linear_sum_assignment.GetCost();
  }
  return 0;
}

bool RoutingModel::RouteCanBeUsedByVehicle(const Assignment& assignment,
                                           int start_index, int vehicle) const {
  int current_index =
      IsStart(start_index) ? Next(assignment, start_index) : start_index;
  while (!IsEnd(current_index)) {
    const IntVar* const vehicle_var = VehicleVar(current_index);
    if (!vehicle_var->Contains(vehicle)) {
      return false;
    }
    const int next_index = Next(assignment, current_index);
    CHECK_NE(next_index, current_index) << "Inactive node inside a route";
    current_index = next_index;
  }
  return true;
}

bool RoutingModel::ReplaceUnusedVehicle(
    int unused_vehicle, int active_vehicle,
    Assignment* const compact_assignment) const {
  CHECK(compact_assignment != nullptr);
  CHECK(!IsVehicleUsed(*compact_assignment, unused_vehicle));
  CHECK(IsVehicleUsed(*compact_assignment, active_vehicle));
  // Swap NextVars at start nodes.
  const int unused_vehicle_start = Start(unused_vehicle);
  IntVar* const unused_vehicle_start_var = NextVar(unused_vehicle_start);
  const int unused_vehicle_end = End(unused_vehicle);
  const int active_vehicle_start = Start(active_vehicle);
  const int active_vehicle_end = End(active_vehicle);
  IntVar* const active_vehicle_start_var = NextVar(active_vehicle_start);
  const int active_vehicle_next =
      compact_assignment->Value(active_vehicle_start_var);
  compact_assignment->SetValue(unused_vehicle_start_var, active_vehicle_next);
  compact_assignment->SetValue(active_vehicle_start_var, End(active_vehicle));

  // Update VehicleVars along the route, update the last NextVar.
  int current_index = active_vehicle_next;
  while (!IsEnd(current_index)) {
    IntVar* const vehicle_var = VehicleVar(current_index);
    compact_assignment->SetValue(vehicle_var, unused_vehicle);
    const int next_index = Next(*compact_assignment, current_index);
    if (IsEnd(next_index)) {
      IntVar* const last_next_var = NextVar(current_index);
      compact_assignment->SetValue(last_next_var, End(unused_vehicle));
    }
    current_index = next_index;
  }

  // Update dimensions: update transits at the start.
  for (const RoutingDimension* const dimension : dimensions_) {
    const std::vector<IntVar*>& transit_variables = dimension->transits();
    IntVar* const unused_vehicle_transit_var =
        transit_variables[unused_vehicle_start];
    IntVar* const active_vehicle_transit_var =
        transit_variables[active_vehicle_start];
    const bool contains_unused_vehicle_transit_var =
        compact_assignment->Contains(unused_vehicle_transit_var);
    const bool contains_active_vehicle_transit_var =
        compact_assignment->Contains(active_vehicle_transit_var);
    if (contains_unused_vehicle_transit_var !=
        contains_active_vehicle_transit_var) {
      // TODO(user): clarify the expected trigger rate of this LOG.
      LOG(INFO) << "The assignment contains transit variable for dimension '"
                << dimension->name() << "' for some vehicles, but not for all";
      return false;
    }
    if (contains_unused_vehicle_transit_var) {
      const int64_t old_unused_vehicle_transit =
          compact_assignment->Value(unused_vehicle_transit_var);
      const int64_t old_active_vehicle_transit =
          compact_assignment->Value(active_vehicle_transit_var);
      compact_assignment->SetValue(unused_vehicle_transit_var,
                                   old_active_vehicle_transit);
      compact_assignment->SetValue(active_vehicle_transit_var,
                                   old_unused_vehicle_transit);
    }

    // Update dimensions: update cumuls at the end.
    const std::vector<IntVar*>& cumul_variables = dimension->cumuls();
    IntVar* const unused_vehicle_cumul_var =
        cumul_variables[unused_vehicle_end];
    IntVar* const active_vehicle_cumul_var =
        cumul_variables[active_vehicle_end];
    const int64_t old_unused_vehicle_cumul =
        compact_assignment->Value(unused_vehicle_cumul_var);
    const int64_t old_active_vehicle_cumul =
        compact_assignment->Value(active_vehicle_cumul_var);
    compact_assignment->SetValue(unused_vehicle_cumul_var,
                                 old_active_vehicle_cumul);
    compact_assignment->SetValue(active_vehicle_cumul_var,
                                 old_unused_vehicle_cumul);
  }
  return true;
}

Assignment* RoutingModel::CompactAssignment(
    const Assignment& assignment) const {
  return CompactAssignmentInternal(assignment, false);
}

Assignment* RoutingModel::CompactAndCheckAssignment(
    const Assignment& assignment) const {
  return CompactAssignmentInternal(assignment, true);
}

Assignment* RoutingModel::CompactAssignmentInternal(
    const Assignment& assignment, bool check_compact_assignment) const {
  CHECK_EQ(assignment.solver(), solver_.get());
  if (!CostsAreHomogeneousAcrossVehicles()) {
    LOG(WARNING)
        << "The costs are not homogeneous, routes cannot be rearranged";
    return nullptr;
  }

  std::unique_ptr<Assignment> compact_assignment(new Assignment(&assignment));
  for (int vehicle = 0; vehicle < vehicles_ - 1; ++vehicle) {
    if (IsVehicleUsed(*compact_assignment, vehicle)) {
      continue;
    }
    const int vehicle_start = Start(vehicle);
    const int vehicle_end = End(vehicle);
    // Find the last vehicle, that can swap routes with this one.
    int swap_vehicle = vehicles_ - 1;
    bool has_more_vehicles_with_route = false;
    for (; swap_vehicle > vehicle; --swap_vehicle) {
      // If a vehicle was already swapped, it will appear in compact_assignment
      // as unused.
      if (!IsVehicleUsed(*compact_assignment, swap_vehicle) ||
          !IsVehicleUsed(*compact_assignment, swap_vehicle)) {
        continue;
      }
      has_more_vehicles_with_route = true;
      const int swap_vehicle_start = Start(swap_vehicle);
      const int swap_vehicle_end = End(swap_vehicle);
      if (manager_.IndexToNode(vehicle_start) !=
              manager_.IndexToNode(swap_vehicle_start) ||
          manager_.IndexToNode(vehicle_end) !=
              manager_.IndexToNode(swap_vehicle_end)) {
        continue;
      }

      // Check that updating VehicleVars is OK.
      if (RouteCanBeUsedByVehicle(*compact_assignment, swap_vehicle_start,
                                  vehicle)) {
        break;
      }
    }

    if (swap_vehicle == vehicle) {
      if (has_more_vehicles_with_route) {
        // No route can be assigned to this vehicle, but there are more vehicles
        // with a route left. This would leave a gap in the indices.
        // TODO(user): clarify the expected trigger rate of this LOG.
        LOG(INFO) << "No vehicle that can be swapped with " << vehicle
                  << " was found";
        return nullptr;
      } else {
        break;
      }
    } else {
      if (!ReplaceUnusedVehicle(vehicle, swap_vehicle,
                                compact_assignment.get())) {
        return nullptr;
      }
    }
  }
  if (check_compact_assignment &&
      !solver_->CheckAssignment(compact_assignment.get())) {
    // TODO(user): clarify the expected trigger rate of this LOG.
    LOG(WARNING) << "The compacted assignment is not a valid solution";
    return nullptr;
  }
  return compact_assignment.release();
}

int RoutingModel::FindNextActive(int index,
                                 absl::Span<const int64_t> indices) const {
  ++index;
  CHECK_LE(0, index);
  const int size = indices.size();
  while (index < size && ActiveVar(indices[index])->Max() == 0) {
    ++index;
  }
  return index;
}

IntVar* RoutingModel::ApplyLocks(absl::Span<const int64_t> locks) {
  // TODO(user): Replace calls to this method with calls to
  // ApplyLocksToAllVehicles and remove this method?
  CHECK_EQ(vehicles_, 1);
  preassignment_->Clear();
  IntVar* next_var = nullptr;
  int lock_index = FindNextActive(-1, locks);
  const int size = locks.size();
  if (lock_index < size) {
    next_var = NextVar(locks[lock_index]);
    preassignment_->Add(next_var);
    for (lock_index = FindNextActive(lock_index, locks); lock_index < size;
         lock_index = FindNextActive(lock_index, locks)) {
      preassignment_->SetValue(next_var, locks[lock_index]);
      next_var = NextVar(locks[lock_index]);
      preassignment_->Add(next_var);
    }
  }
  return next_var;
}

bool RoutingModel::ApplyLocksToAllVehicles(
    const std::vector<std::vector<int64_t>>& locks, bool close_routes) {
  preassignment_->Clear();
  return RoutesToAssignment(locks, true, close_routes, preassignment_);
}

int64_t RoutingModel::GetNumberOfDecisionsInFirstSolution(
    const RoutingSearchParameters& parameters) const {
  IntVarFilteredDecisionBuilder* const decision_builder =
      GetFilteredFirstSolutionDecisionBuilderOrNull(parameters);
  return decision_builder != nullptr ? decision_builder->number_of_decisions()
                                     : 0;
}

int64_t RoutingModel::GetNumberOfRejectsInFirstSolution(
    const RoutingSearchParameters& parameters) const {
  IntVarFilteredDecisionBuilder* const decision_builder =
      GetFilteredFirstSolutionDecisionBuilderOrNull(parameters);
  return decision_builder != nullptr ? decision_builder->number_of_rejects()
                                     : 0;
}

bool RoutingModel::WriteAssignment(const std::string& file_name) const {
  if (collect_assignments_->solution_count() == 1 && assignment_ != nullptr) {
    assignment_->CopyIntersection(collect_assignments_->solution(0));
    return assignment_->Save(file_name);
  } else {
    return false;
  }
}

Assignment* RoutingModel::ReadAssignment(const std::string& file_name) {
  QuietCloseModel();
  CHECK(assignment_ != nullptr);
  if (assignment_->Load(file_name)) {
    return DoRestoreAssignment();
  }
  return nullptr;
}

Assignment* RoutingModel::RestoreAssignment(const Assignment& solution) {
  QuietCloseModel();
  CHECK(assignment_ != nullptr);
  assignment_->CopyIntersection(&solution);
  return DoRestoreAssignment();
}

Assignment* RoutingModel::DoRestoreAssignment() {
  if (status_ == RoutingSearchStatus::ROUTING_INVALID) {
    return nullptr;
  }
  solver_->Solve(restore_assignment_, monitors_);
  if (collect_assignments_->solution_count() == 1) {
    status_ = RoutingSearchStatus::ROUTING_SUCCESS;
    return collect_assignments_->solution(0);
  } else {
    status_ = RoutingSearchStatus::ROUTING_FAIL;
    return nullptr;
  }
  return nullptr;
}

bool RoutingModel::RoutesToAssignment(
    const std::vector<std::vector<int64_t>>& routes,
    bool ignore_inactive_indices, bool close_routes,
    Assignment* const assignment) const {
  CHECK(assignment != nullptr);
  if (!closed_) {
    LOG(ERROR) << "The model is not closed yet";
    return false;
  }
  const int num_routes = routes.size();
  if (num_routes > vehicles_) {
    LOG(ERROR) << "The number of vehicles in the assignment (" << routes.size()
               << ") is greater than the number of vehicles in the model ("
               << vehicles_ << ")";
    return false;
  }

  absl::flat_hash_set<int> visited_indices;
  // Set value to NextVars based on the routes.
  for (int vehicle = 0; vehicle < num_routes; ++vehicle) {
    const std::vector<int64_t>& route = routes[vehicle];
    int from_index = Start(vehicle);
    std::pair<absl::flat_hash_set<int>::iterator, bool> insert_result =
        visited_indices.insert(from_index);
    if (!insert_result.second) {
      LOG(ERROR) << "Index " << from_index << " (start node for vehicle "
                 << vehicle << ") was already used";
      return false;
    }

    for (const int64_t to_index : route) {
      if (to_index < 0 || to_index >= Size()) {
        LOG(ERROR) << "Invalid index: " << to_index;
        return false;
      }

      IntVar* const active_var = ActiveVar(to_index);
      if (active_var->Max() == 0) {
        if (ignore_inactive_indices) {
          continue;
        } else {
          LOG(ERROR) << "Index " << to_index << " is not active";
          return false;
        }
      }

      insert_result = visited_indices.insert(to_index);
      if (!insert_result.second) {
        LOG(ERROR) << "Index " << to_index << " is used multiple times";
        return false;
      }

      const IntVar* const vehicle_var = VehicleVar(to_index);
      if (!vehicle_var->Contains(vehicle)) {
        LOG(ERROR) << "Vehicle " << vehicle << " is not allowed at index "
                   << to_index;
        return false;
      }

      IntVar* const from_var = NextVar(from_index);
      if (!assignment->Contains(from_var)) {
        assignment->Add(from_var);
      }
      assignment->SetValue(from_var, to_index);

      from_index = to_index;
    }

    if (close_routes) {
      IntVar* const last_var = NextVar(from_index);
      if (!assignment->Contains(last_var)) {
        assignment->Add(last_var);
      }
      assignment->SetValue(last_var, End(vehicle));
    }
  }

  // Do not use the remaining vehicles.
  for (int vehicle = num_routes; vehicle < vehicles_; ++vehicle) {
    const int start_index = Start(vehicle);
    // Even if close_routes is false, we still need to add the start index to
    // visited_indices so that deactivating other nodes works correctly.
    std::pair<absl::flat_hash_set<int>::iterator, bool> insert_result =
        visited_indices.insert(start_index);
    if (!insert_result.second) {
      LOG(ERROR) << "Index " << start_index << " is used multiple times";
      return false;
    }
    if (close_routes) {
      IntVar* const start_var = NextVar(start_index);
      if (!assignment->Contains(start_var)) {
        assignment->Add(start_var);
      }
      assignment->SetValue(start_var, End(vehicle));
    }
  }

  // Deactivate other nodes (by pointing them to themselves).
  if (close_routes) {
    for (int index = 0; index < Size(); ++index) {
      if (!visited_indices.contains(index)) {
        IntVar* const next_var = NextVar(index);
        if (!assignment->Contains(next_var)) {
          assignment->Add(next_var);
        }
        assignment->SetValue(next_var, index);
      }
    }
  }

  return true;
}

Assignment* RoutingModel::ReadAssignmentFromRoutes(
    const std::vector<std::vector<int64_t>>& routes,
    bool ignore_inactive_indices) {
  QuietCloseModel();
  if (!RoutesToAssignment(routes, ignore_inactive_indices, true, assignment_)) {
    return nullptr;
  }
  // DoRestoreAssignment() might still fail when checking constraints (most
  // constraints are not verified by RoutesToAssignment) or when filling in
  // dimension variables.
  return DoRestoreAssignment();
}

void RoutingModel::AssignmentToRoutes(
    const Assignment& assignment,
    std::vector<std::vector<int64_t>>* const routes) const {
  CHECK(closed_);
  CHECK(routes != nullptr);

  const int model_size = Size();
  routes->resize(vehicles_);
  for (int vehicle = 0; vehicle < vehicles_; ++vehicle) {
    std::vector<int64_t>* const vehicle_route = &routes->at(vehicle);
    vehicle_route->clear();

    int num_visited_indices = 0;
    const int first_index = Start(vehicle);
    const IntVar* const first_var = NextVar(first_index);
    CHECK(assignment.Contains(first_var));
    CHECK(assignment.Bound(first_var));
    int current_index = assignment.Value(first_var);
    while (!IsEnd(current_index)) {
      vehicle_route->push_back(current_index);

      const IntVar* const next_var = NextVar(current_index);
      CHECK(assignment.Contains(next_var));
      CHECK(assignment.Bound(next_var));
      current_index = assignment.Value(next_var);

      ++num_visited_indices;
      CHECK_LE(num_visited_indices, model_size)
          << "The assignment contains a cycle";
    }
  }
}

#ifndef SWIG
std::vector<std::vector<int64_t>> RoutingModel::GetRoutesFromAssignment(
    const Assignment& assignment) {
  std::vector<std::vector<int64_t>> route_indices(vehicles());
  for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
    if (!assignment.Bound(NextVar(vehicle))) {
      LOG(DFATAL) << "GetRoutesFromAssignment() called on incomplete solution:"
                  << " NextVar(" << vehicle << ") is unbound.";
    }
  }
  for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
    int64_t index = Start(vehicle);
    route_indices[vehicle].push_back(index);
    while (!IsEnd(index)) {
      index = assignment.Value(NextVar(index));
      route_indices[vehicle].push_back(index);
    }
  }
  return route_indices;
}
#endif

int64_t RoutingModel::GetArcCostForClassInternal(
    int64_t from_index, int64_t to_index,
    CostClassIndex cost_class_index) const {
  DCHECK(closed_);
  DCHECK_GE(cost_class_index, 0);
  DCHECK_LT(cost_class_index, cost_classes_.size());
  CostCacheElement* const cache = &cost_cache_[from_index];
  // See the comment in CostCacheElement in the .h for the int64_t->int cast.
  if (cache->index == static_cast<int>(to_index) &&
      cache->cost_class_index == cost_class_index) {
    return cache->cost;
  }
  int64_t cost = 0;
  const CostClass& cost_class = cost_classes_[cost_class_index];
  const auto& evaluator = transit_evaluators_[cost_class.evaluator_index];
  if (!IsStart(from_index)) {
    cost = CapAdd(evaluator(from_index, to_index),
                  GetDimensionTransitCostSum(from_index, to_index, cost_class));
  } else if (!IsEnd(to_index)) {
    // Apply route fixed cost on first non-first/last node, in other words on
    // the arc from the first node to its next node if it's not the last node.
    cost = CapAdd(
        evaluator(from_index, to_index),
        CapAdd(GetDimensionTransitCostSum(from_index, to_index, cost_class),
               fixed_cost_of_vehicle_[VehicleIndex(from_index)]));
  } else {
    // If there's only the first and last nodes on the route, it is considered
    // as an empty route.
    if (vehicle_used_when_empty_[VehicleIndex(from_index)]) {
      cost =
          CapAdd(evaluator(from_index, to_index),
                 GetDimensionTransitCostSum(from_index, to_index, cost_class));
    } else {
      cost = 0;
    }
  }
  *cache = {static_cast<int>(to_index), cost_class_index, cost};
  return cost;
}

std::function<int64_t(int64_t, int64_t, int64_t)>
RoutingModel::GetLocalSearchArcCostCallback(
    const RoutingSearchParameters& parameters) const {
  return parameters
                 .use_guided_local_search_penalties_in_local_search_operators()
             ? absl::bind_front(
                   &RoutingModel::GetArcCostWithGuidedLocalSearchPenalties,
                   this)
             : absl::bind_front(&RoutingModel::GetArcCostForVehicle, this);
}

std::function<int64_t(int64_t, int64_t)>
RoutingModel::GetLocalSearchHomogeneousArcCostCallback(
    const RoutingSearchParameters& parameters) const {
  return parameters
                 .use_guided_local_search_penalties_in_local_search_operators()
             ? absl::bind_front(
                   &RoutingModel::
                       GetHomogeneousArcCostWithGuidedLocalSearchPenalties,
                   this)
             : absl::bind_front(&RoutingModel::GetHomogeneousCost, this);
}

bool RoutingModel::IsVehicleUsed(const Assignment& assignment,
                                 int vehicle) const {
  CHECK_GE(vehicle, 0);
  CHECK_LT(vehicle, vehicles_);
  CHECK_EQ(solver_.get(), assignment.solver());
  IntVar* const start_var = NextVar(Start(vehicle));
  CHECK(assignment.Contains(start_var));
  return !IsEnd(assignment.Value(start_var));
}

int64_t RoutingModel::Next(const Assignment& assignment, int64_t index) const {
  CHECK_EQ(solver_.get(), assignment.solver());
  IntVar* const next_var = NextVar(index);
  CHECK(assignment.Contains(next_var));
  CHECK(assignment.Bound(next_var));
  return assignment.Value(next_var);
}

int64_t RoutingModel::GetArcCostForVehicle(int64_t from_index, int64_t to_index,
                                           int64_t vehicle) const {
  if (from_index != to_index && vehicle >= 0) {
    return GetArcCostForClassInternal(from_index, to_index,
                                      GetCostClassIndexOfVehicle(vehicle));
  } else {
    return 0;
  }
}

int64_t RoutingModel::GetArcCostForClass(
    int64_t from_index, int64_t to_index,
    int64_t /*CostClassIndex*/ cost_class_index) const {
  if (from_index != to_index) {
    return GetArcCostForClassInternal(from_index, to_index,
                                      CostClassIndex(cost_class_index));
  } else {
    return 0;
  }
}

int64_t RoutingModel::GetArcCostForFirstSolution(int64_t from_index,
                                                 int64_t to_index) const {
  // Return high cost if connecting to an end (or bound-to-end) node;
  // this is used in the cost-based first solution strategies to avoid closing
  // routes too soon.
  if (!is_bound_to_end_ct_added_.Switched()) {
    // Lazily adding path-cumul constraint propagating connection to route end,
    // as it can be pretty costly in the general case.
    std::vector<IntVar*> zero_transit(Size(), solver_->MakeIntConst(0));
    solver_->AddConstraint(solver_->MakeDelayedPathCumul(
        nexts_, active_, is_bound_to_end_, zero_transit));
    is_bound_to_end_ct_added_.Switch(solver_.get());
  }
  if (is_bound_to_end_[to_index]->Min() == 1)
    return std::numeric_limits<int64_t>::max();
  // TODO(user): Take vehicle into account.
  return GetHomogeneousCost(from_index, to_index);
}

int64_t RoutingModel::GetDimensionTransitCostSum(
    int64_t i, int64_t j, const CostClass& cost_class) const {
  int64_t cost = 0;
  for ([[maybe_unused]] const auto [transit_evaluator_class,
                                    span_cost_coefficient,
                                    unused_slack_cost_coefficient, dimension] :
       cost_class.dimension_transit_evaluator_class_and_cost_coefficient) {
    DCHECK_GE(span_cost_coefficient, 0);
    if (span_cost_coefficient == 0) continue;
    CapAddTo(CapProd(span_cost_coefficient, dimension->GetTransitValueFromClass(
                                                i, j, transit_evaluator_class)),
             &cost);
  }
  return cost;
}

bool RoutingModel::ArcIsMoreConstrainedThanArc(int64_t from, int64_t to1,
                                               int64_t to2) {
  // Deal with end nodes: never pick an end node over a non-end node.
  if (IsEnd(to1) || IsEnd(to2)) {
    if (IsEnd(to1) != IsEnd(to2)) return IsEnd(to2);
    // If both are end nodes, we don't care; the right end node will be picked
    // by constraint propagation. Break the tie by index.
    return to1 < to2;
  }

  // Look whether they are mandatory (must be performed) or optional.
  const bool mandatory1 = active_[to1]->Min() == 1;
  const bool mandatory2 = active_[to2]->Min() == 1;
  // Always pick a mandatory node over a non-mandatory one.
  if (mandatory1 != mandatory2) return mandatory1;

  // Look at the vehicle variables.
  IntVar* const src_vehicle_var = VehicleVar(from);
  // In case the source vehicle is bound, "src_vehicle" will be it.
  // Otherwise, it'll be set to some possible source vehicle that
  // isn't -1 (if possible).
  const int64_t src_vehicle = src_vehicle_var->Max();
  if (src_vehicle_var->Bound()) {
    IntVar* const to1_vehicle_var = VehicleVar(to1);
    IntVar* const to2_vehicle_var = VehicleVar(to2);
    // Subtle: non-mandatory node have kNoVehicle as possible value for
    // their vehicle variable. So they're effectively "bound" when their domain
    // size is 2.
    const bool bound1 =
        mandatory1 ? to1_vehicle_var->Bound() : (to1_vehicle_var->Size() <= 2);
    const bool bound2 =
        mandatory2 ? to2_vehicle_var->Bound() : (to2_vehicle_var->Size() <= 2);
    // Prefer a destination bound to a given vehicle, even if it's not
    // bound to the right one (the propagation will quickly rule it out).
    if (bound1 != bound2) return bound1;
    if (bound1) {  // same as bound1 && bound2.
      // Min() will return kNoVehicle for optional nodes. Thus we use Max().
      const int64_t vehicle1 = to1_vehicle_var->Max();
      const int64_t vehicle2 = to2_vehicle_var->Max();
      // Prefer a destination bound to the right vehicle.
      // TODO(user): cover this clause in a unit test.
      if ((vehicle1 == src_vehicle) != (vehicle2 == src_vehicle)) {
        return vehicle1 == src_vehicle;
      }
      // If no destination is bound to the right vehicle, whatever we
      // return doesn't matter: both are infeasible. To be consistent, we
      // just break the tie.
      if (vehicle1 != src_vehicle) return to1 < to2;
    }
  }
  // At this point, either both destinations are bound to the source vehicle,
  // or none of them is bound, or the source vehicle isn't bound.
  // We don't bother inspecting the domains of the vehicle variables further.

  // Inspect the primary constrained dimension, if any.
  // TODO(user): try looking at all the dimensions, not just the primary one,
  // and reconsider the need for a "primary" dimension.
  if (!GetPrimaryConstrainedDimension().empty()) {
    const std::vector<IntVar*>& cumul_vars =
        GetDimensionOrDie(GetPrimaryConstrainedDimension()).cumuls();
    IntVar* const dim1 = cumul_vars[to1];
    IntVar* const dim2 = cumul_vars[to2];
    // Prefer the destination that has a lower upper bound for the constrained
    // dimension.
    if (dim1->Max() != dim2->Max()) return dim1->Max() < dim2->Max();
    // TODO(user): evaluate the *actual* Min() of each cumul variable in the
    // scenario where the corresponding arc from->to is performed, and pick
    // the destination with the lowest value.
  }

  // Break ties on equally constrained nodes with the (cost - unperformed
  // penalty).
  {
    const /*CostClassIndex*/ int64_t cost_class_index =
        SafeGetCostClassInt64OfVehicle(src_vehicle);
    const int64_t cost1 =
        CapSub(GetArcCostForClass(from, to1, cost_class_index),
               UnperformedPenalty(to1));
    const int64_t cost2 =
        CapSub(GetArcCostForClass(from, to2, cost_class_index),
               UnperformedPenalty(to2));
    if (cost1 != cost2) return cost1 < cost2;
  }

  // Further break ties by looking at the size of the VehicleVar.
  {
    const int64_t num_vehicles1 = VehicleVar(to1)->Size();
    const int64_t num_vehicles2 = VehicleVar(to2)->Size();
    if (num_vehicles1 != num_vehicles2) return num_vehicles1 < num_vehicles2;
  }

  // Break perfect ties by value.
  return to1 < to2;
}

void RoutingModel::SetVisitType(int64_t index, int type,
                                VisitTypePolicy policy) {
  CHECK_LT(index, index_to_visit_type_.size());
  DCHECK_EQ(index_to_visit_type_.size(), index_to_type_policy_.size());
  index_to_visit_type_[index] = type;
  index_to_type_policy_[index] = policy;
  num_visit_types_ = std::max(num_visit_types_, type + 1);
}

int RoutingModel::GetVisitType(int64_t index) const {
  CHECK_LT(index, index_to_visit_type_.size());
  return index_to_visit_type_[index];
}

const std::vector<int>& RoutingModel::GetSingleNodesOfType(int type) const {
  DCHECK_LT(type, single_nodes_of_type_.size());
  return single_nodes_of_type_[type];
}

const std::vector<int>& RoutingModel::GetPairIndicesOfType(int type) const {
  DCHECK_LT(type, pair_indices_of_type_.size());
  return pair_indices_of_type_[type];
}

RoutingModel::VisitTypePolicy RoutingModel::GetVisitTypePolicy(
    int64_t index) const {
  CHECK_LT(index, index_to_type_policy_.size());
  return index_to_type_policy_[index];
}

void RoutingModel::AddHardTypeIncompatibility(int type1, int type2) {
  DCHECK_LT(std::max(type1, type2), num_visit_types_);
  if (hard_incompatible_types_per_type_index_.size() < num_visit_types_) {
    hard_incompatible_types_per_type_index_.resize(num_visit_types_);
  }

  hard_incompatible_types_per_type_index_[type1].insert(type2);
  hard_incompatible_types_per_type_index_[type2].insert(type1);
}

void RoutingModel::AddTemporalTypeIncompatibility(int type1, int type2) {
  DCHECK_LT(std::max(type1, type2), num_visit_types_);
  if (temporal_incompatible_types_per_type_index_.size() < num_visit_types_) {
    temporal_incompatible_types_per_type_index_.resize(num_visit_types_);
  }

  temporal_incompatible_types_per_type_index_[type1].insert(type2);
  temporal_incompatible_types_per_type_index_[type2].insert(type1);
}

const absl::flat_hash_set<int>&
RoutingModel::GetHardTypeIncompatibilitiesOfType(int type) const {
  DCHECK(closed_);
  DCHECK_GE(type, 0);
  DCHECK_LT(type, num_visit_types_);
  return type < hard_incompatible_types_per_type_index_.size()
             ? hard_incompatible_types_per_type_index_[type]
             : empty_incompatibility_set_;
}

const absl::flat_hash_set<int>&
RoutingModel::GetTemporalTypeIncompatibilitiesOfType(int type) const {
  DCHECK(closed_);
  DCHECK_GE(type, 0);
  DCHECK_LT(type, num_visit_types_);
  return type < temporal_incompatible_types_per_type_index_.size()
             ? temporal_incompatible_types_per_type_index_[type]
             : empty_incompatibility_set_;
}

// TODO(user): Consider if an empty "required_type_alternatives" should mean
// trivially feasible requirement, as there are no required type alternatives?
void RoutingModel::AddSameVehicleRequiredTypeAlternatives(
    int dependent_type, absl::flat_hash_set<int> required_type_alternatives) {
  DCHECK_LT(dependent_type, num_visit_types_);

  if (required_type_alternatives.empty()) {
    // The dependent_type requires an infeasible (empty) set of types.
    // Nodes of this type and all policies except
    // ADDED_TYPE_REMOVED_FROM_VEHICLE are trivially infeasible.
    absl::flat_hash_set<VisitTypePolicy>& infeasible_policies =
        trivially_infeasible_visit_types_to_policies_[dependent_type];
    infeasible_policies.insert(TYPE_ADDED_TO_VEHICLE);
    infeasible_policies.insert(TYPE_ON_VEHICLE_UP_TO_VISIT);
    infeasible_policies.insert(TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED);
    return;
  }

  if (same_vehicle_required_type_alternatives_per_type_index_.size() <
      num_visit_types_) {
    same_vehicle_required_type_alternatives_per_type_index_.resize(
        num_visit_types_);
  }
  same_vehicle_required_type_alternatives_per_type_index_[dependent_type]
      .push_back(std::move(required_type_alternatives));
}

void RoutingModel::AddRequiredTypeAlternativesWhenAddingType(
    int dependent_type, absl::flat_hash_set<int> required_type_alternatives) {
  DCHECK_LT(dependent_type, num_visit_types_);

  if (required_type_alternatives.empty()) {
    // The dependent_type requires an infeasible (empty) set of types.
    // Nodes of this type and policy TYPE_ADDED_TO_VEHICLE or
    // TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED are trivially infeasible.
    absl::flat_hash_set<VisitTypePolicy>& infeasible_policies =
        trivially_infeasible_visit_types_to_policies_[dependent_type];
    infeasible_policies.insert(TYPE_ADDED_TO_VEHICLE);
    infeasible_policies.insert(TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED);
    return;
  }

  if (required_type_alternatives_when_adding_type_index_.size() <
      num_visit_types_) {
    required_type_alternatives_when_adding_type_index_.resize(num_visit_types_);
  }
  required_type_alternatives_when_adding_type_index_[dependent_type].push_back(
      std::move(required_type_alternatives));
}

void RoutingModel::AddRequiredTypeAlternativesWhenRemovingType(
    int dependent_type, absl::flat_hash_set<int> required_type_alternatives) {
  DCHECK_LT(dependent_type, num_visit_types_);

  if (required_type_alternatives.empty()) {
    // The dependent_type requires an infeasible (empty) set of types.
    // Nodes of this type and all policies except TYPE_ADDED_TO_VEHICLE are
    // trivially infeasible.
    absl::flat_hash_set<VisitTypePolicy>& infeasible_policies =
        trivially_infeasible_visit_types_to_policies_[dependent_type];
    infeasible_policies.insert(ADDED_TYPE_REMOVED_FROM_VEHICLE);
    infeasible_policies.insert(TYPE_ON_VEHICLE_UP_TO_VISIT);
    infeasible_policies.insert(TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED);
    return;
  }

  if (required_type_alternatives_when_removing_type_index_.size() <
      num_visit_types_) {
    required_type_alternatives_when_removing_type_index_.resize(
        num_visit_types_);
  }
  required_type_alternatives_when_removing_type_index_[dependent_type]
      .push_back(std::move(required_type_alternatives));
}

const std::vector<absl::flat_hash_set<int>>&
RoutingModel::GetSameVehicleRequiredTypeAlternativesOfType(int type) const {
  DCHECK(closed_);
  DCHECK_GE(type, 0);
  DCHECK_LT(type, num_visit_types_);
  return type < same_vehicle_required_type_alternatives_per_type_index_.size()
             ? same_vehicle_required_type_alternatives_per_type_index_[type]
             : empty_required_type_alternatives_;
}

const std::vector<absl::flat_hash_set<int>>&
RoutingModel::GetRequiredTypeAlternativesWhenAddingType(int type) const {
  DCHECK(closed_);
  DCHECK_GE(type, 0);
  DCHECK_LT(type, num_visit_types_);
  return type < required_type_alternatives_when_adding_type_index_.size()
             ? required_type_alternatives_when_adding_type_index_[type]
             : empty_required_type_alternatives_;
}

const std::vector<absl::flat_hash_set<int>>&
RoutingModel::GetRequiredTypeAlternativesWhenRemovingType(int type) const {
  DCHECK(closed_);
  DCHECK_GE(type, 0);
  DCHECK_LT(type, num_visit_types_);
  return type < required_type_alternatives_when_removing_type_index_.size()
             ? required_type_alternatives_when_removing_type_index_[type]
             : empty_required_type_alternatives_;
}

int64_t RoutingModel::UnperformedPenalty(int64_t var_index) const {
  return UnperformedPenaltyOrValue(0, var_index);
}

int64_t RoutingModel::UnperformedPenaltyOrValue(int64_t default_value,
                                                int64_t var_index) const {
  if (active_[var_index]->Min() == 1)
    return std::numeric_limits<int64_t>::max();  // Forced active.
  const std::vector<DisjunctionIndex>& disjunction_indices =
      GetDisjunctionIndices(var_index);
  if (disjunction_indices.size() != 1) return default_value;
  const DisjunctionIndex disjunction_index = disjunction_indices[0];
  // The disjunction penalty can be kNoPenalty iff there is more than one node
  // in the disjunction; otherwise we would have caught it earlier (the node
  // would be forced active).
  return std::max(int64_t{0}, disjunctions_[disjunction_index].value.penalty);
}

std::string RoutingModel::DebugOutputAssignment(
    const Assignment& solution_assignment,
    const std::string& dimension_to_print) const {
  for (int i = 0; i < Size(); ++i) {
    if (!solution_assignment.Bound(NextVar(i))) {
      LOG(DFATAL)
          << "DebugOutputVehicleSchedules() called on incomplete solution:"
          << " NextVar(" << i << ") is unbound.";
      return "";
    }
  }
  std::string output;
  absl::flat_hash_set<std::string> dimension_names;
  if (dimension_to_print.empty()) {
    const std::vector<std::string> all_dimension_names = GetAllDimensionNames();
    dimension_names.insert(all_dimension_names.begin(),
                           all_dimension_names.end());
  } else {
    dimension_names.insert(dimension_to_print);
  }
  for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
    int empty_vehicle_range_start = vehicle;
    while (vehicle < vehicles() &&
           IsEnd(solution_assignment.Value(NextVar(Start(vehicle))))) {
      vehicle++;
    }
    if (empty_vehicle_range_start != vehicle) {
      if (empty_vehicle_range_start == vehicle - 1) {
        absl::StrAppendFormat(&output, "Vehicle %d: empty",
                              empty_vehicle_range_start);
      } else {
        absl::StrAppendFormat(&output, "Vehicles %d-%d: empty",
                              empty_vehicle_range_start, vehicle - 1);
      }
      output.append("\n");
    }
    if (vehicle < vehicles()) {
      absl::StrAppendFormat(&output, "Vehicle %d:", vehicle);
      int64_t index = Start(vehicle);
      for (;;) {
        const IntVar* vehicle_var = VehicleVar(index);
        absl::StrAppendFormat(&output, "%d Vehicle(%d) ", index,
                              solution_assignment.Value(vehicle_var));
        for (const RoutingDimension* const dimension : dimensions_) {
          if (dimension_names.contains(dimension->name())) {
            const IntVar* const var = dimension->CumulVar(index);
            absl::StrAppendFormat(&output, "%s(%d..%d) ", dimension->name(),
                                  solution_assignment.Min(var),
                                  solution_assignment.Max(var));
          }
        }
        if (IsEnd(index)) break;
        index = solution_assignment.Value(NextVar(index));
        if (IsEnd(index)) output.append("Route end ");
      }
      output.append("\n");
    }
  }
  output.append("Unperformed nodes: ");
  bool has_unperformed = false;
  for (int i = 0; i < Size(); ++i) {
    if (!IsEnd(i) && !IsStart(i) &&
        solution_assignment.Value(NextVar(i)) == i) {
      absl::StrAppendFormat(&output, "%d ", i);
      has_unperformed = true;
    }
  }
  if (!has_unperformed) output.append("None");
  output.append("\n");
  return output;
}

#ifndef SWIG
std::vector<std::vector<std::pair<int64_t, int64_t>>>
RoutingModel::GetCumulBounds(const Assignment& solution_assignment,
                             const RoutingDimension& dimension) {
  std::vector<std::vector<std::pair<int64_t, int64_t>>> cumul_bounds(
      vehicles());
  for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
    if (!solution_assignment.Bound(NextVar(vehicle))) {
      LOG(DFATAL) << "GetCumulBounds() called on incomplete solution:"
                  << " NextVar(" << vehicle << ") is unbound.";
    }
  }

  for (int vehicle_id = 0; vehicle_id < vehicles(); ++vehicle_id) {
    int64_t index = Start(vehicle_id);
    IntVar* dim_var = dimension.CumulVar(index);
    cumul_bounds[vehicle_id].emplace_back(solution_assignment.Min(dim_var),
                                          solution_assignment.Max(dim_var));
    while (!IsEnd(index)) {
      index = solution_assignment.Value(NextVar(index));
      IntVar* dim_var = dimension.CumulVar(index);
      cumul_bounds[vehicle_id].emplace_back(solution_assignment.Min(dim_var),
                                            solution_assignment.Max(dim_var));
    }
  }
  return cumul_bounds;
}
#endif

Assignment* RoutingModel::GetOrCreateAssignment() {
  if (assignment_ == nullptr) {
    assignment_ = solver_->MakeAssignment();
    assignment_->Add(nexts_);
    if (!CostsAreHomogeneousAcrossVehicles()) {
      assignment_->Add(vehicle_vars_);
    }
    assignment_->AddObjective(cost_);
  }
  return assignment_;
}

Assignment* RoutingModel::GetOrCreateTmpAssignment() {
  if (tmp_assignment_ == nullptr) {
    tmp_assignment_ = solver_->MakeAssignment();
    tmp_assignment_->Add(nexts_);
  }
  return tmp_assignment_;
}

RegularLimit* RoutingModel::GetOrCreateLimit() {
  if (limit_ == nullptr) {
    limit_ = solver_->MakeLimit(
        absl::InfiniteDuration(), std::numeric_limits<int64_t>::max(),
        std::numeric_limits<int64_t>::max(),
        std::numeric_limits<int64_t>::max(), /*smart_time_check=*/true);
  }
  return limit_;
}

RegularLimit* RoutingModel::GetOrCreateCumulativeLimit() {
  if (cumulative_limit_ == nullptr) {
    cumulative_limit_ = solver_->MakeLimit(
        absl::InfiniteDuration(), std::numeric_limits<int64_t>::max(),
        std::numeric_limits<int64_t>::max(),
        std::numeric_limits<int64_t>::max(), /*smart_time_check=*/true,
        /*cumulative=*/true);
  }
  return cumulative_limit_;
}

RegularLimit* RoutingModel::GetOrCreateLocalSearchLimit() {
  if (ls_limit_ == nullptr) {
    ls_limit_ = solver_->MakeLimit(absl::InfiniteDuration(),
                                   std::numeric_limits<int64_t>::max(),
                                   std::numeric_limits<int64_t>::max(),
                                   /*solutions=*/1, /*smart_time_check=*/true);
  }
  return ls_limit_;
}

RegularLimit* RoutingModel::GetOrCreateLargeNeighborhoodSearchLimit() {
  if (lns_limit_ == nullptr) {
    lns_limit_ = solver_->MakeLimit(
        absl::InfiniteDuration(), std::numeric_limits<int64_t>::max(),
        std::numeric_limits<int64_t>::max(),
        std::numeric_limits<int64_t>::max(), /*smart_time_check=*/false);
  }
  return lns_limit_;
}

RegularLimit*
RoutingModel::GetOrCreateFirstSolutionLargeNeighborhoodSearchLimit() {
  if (first_solution_lns_limit_ == nullptr) {
    first_solution_lns_limit_ = solver_->MakeLimit(
        absl::InfiniteDuration(), std::numeric_limits<int64_t>::max(),
        std::numeric_limits<int64_t>::max(),
        std::numeric_limits<int64_t>::max(), /*smart_time_check=*/false);
  }
  return first_solution_lns_limit_;
}

LocalSearchOperator* RoutingModel::CreateInsertionOperator() {
  auto get_vehicle_vars = [this]() {
    return CostsAreHomogeneousAcrossVehicles() ? std::vector<IntVar*>()
                                               : vehicle_vars_;
  };
  LocalSearchOperator* insertion_operator = MakeActive(
      solver_.get(), nexts_, get_vehicle_vars(), vehicle_start_class_callback_);
  if (!pickup_delivery_pairs_.empty()) {
    insertion_operator = solver_->ConcatenateOperators(
        {MakePairActive(solver_.get(), nexts_, get_vehicle_vars(),
                        vehicle_start_class_callback_, pickup_delivery_pairs_),
         insertion_operator});
  }
  if (!implicit_pickup_delivery_pairs_without_alternatives_.empty()) {
    insertion_operator = solver_->ConcatenateOperators(
        {MakePairActive(solver_.get(), nexts_, get_vehicle_vars(),
                        vehicle_start_class_callback_,
                        implicit_pickup_delivery_pairs_without_alternatives_),
         insertion_operator});
  }
  return insertion_operator;
}

LocalSearchOperator* RoutingModel::CreateMakeInactiveOperator() {
  auto get_vehicle_vars = [this]() {
    return CostsAreHomogeneousAcrossVehicles() ? std::vector<IntVar*>()
                                               : vehicle_vars_;
  };
  LocalSearchOperator* make_inactive_operator = MakeInactive(
      solver_.get(), nexts_, get_vehicle_vars(), vehicle_start_class_callback_);
  if (!pickup_delivery_pairs_.empty()) {
    make_inactive_operator = solver_->ConcatenateOperators(
        {MakePairInactive(solver_.get(), nexts_, get_vehicle_vars(),
                          vehicle_start_class_callback_,
                          pickup_delivery_pairs_),
         make_inactive_operator});
  }
  return make_inactive_operator;
}

void RoutingModel::CreateNeighborhoodOperators(
    const RoutingSearchParameters& parameters) {
  // TODO(user): Consider setting
  // 'only_sort_neighbors_for_partial_neighborhoods' to false in
  // GetOrCreateNodeNeighborsByCostClass(), and use neighbors regardless of
  // the "used" ratio when parameters.ls_operator_neighbors_ratio() < 1.
  // This would allow the operators to iterate on the neighbors by increasing
  // distance, even if all nodes are considered as neighbors.
  double neighbors_ratio_used = 1;
  const NodeNeighborsByCostClass* neighbors_by_cost_class =
      GetOrCreateNodeNeighborsByCostClass(
          parameters.ls_operator_neighbors_ratio(),
          parameters.ls_operator_min_neighbors(), neighbors_ratio_used,
          /*add_vehicle_starts_to_neighbors=*/false,
          /*add_vehicle_ends_to_neighbors=*/false);
  std::function<const std::vector<int>&(int, int)> get_incoming_neighbors;
  std::function<const std::vector<int>&(int, int)> get_outgoing_neighbors;
  if (neighbors_ratio_used != 1) {
    get_incoming_neighbors = [neighbors_by_cost_class, this](
                                 int64_t node,
                                 int64_t start) -> const std::vector<int>& {
      DCHECK(!IsStart(node));
      return neighbors_by_cost_class->GetIncomingNeighborsOfNodeForCostClass(
          GetCostClassIndexOfVehicle(VehicleIndex(start)).value(), node);
    };
    get_outgoing_neighbors = [neighbors_by_cost_class, this](
                                 int64_t node,
                                 int64_t start) -> const std::vector<int>& {
      DCHECK(!IsEnd(node));
      return neighbors_by_cost_class->GetOutgoingNeighborsOfNodeForCostClass(
          GetCostClassIndexOfVehicle(VehicleIndex(start)).value(), node);
    };
  }

  local_search_operators_.clear();
  local_search_operators_.resize(LOCAL_SEARCH_OPERATOR_COUNTER, nullptr);
  {
    // Operators defined by Solver::LocalSearchOperators.
    const std::vector<
        std::pair<RoutingLocalSearchOperator, Solver::LocalSearchOperators>>
        operator_by_type = {{OR_OPT, Solver::OROPT},
                            {PATH_LNS, Solver::PATHLNS},
                            {FULL_PATH_LNS, Solver::FULLPATHLNS},
                            {INACTIVE_LNS, Solver::UNACTIVELNS}};
    for (const auto [type, op] : operator_by_type) {
      local_search_operators_[type] =
          CostsAreHomogeneousAcrossVehicles()
              ? solver_->MakeOperator(nexts_, op)
              : solver_->MakeOperator(nexts_, vehicle_vars_, op);
    }
  }
  {
    // Operators defined by Solver::EvaluatorLocalSearchOperators.
    const std::vector<std::pair<RoutingLocalSearchOperator,
                                Solver::EvaluatorLocalSearchOperators>>
        operator_by_type = {{LIN_KERNIGHAN, Solver::LK},
                            {TSP_OPT, Solver::TSPOPT},
                            {TSP_LNS, Solver::TSPLNS}};
    for (const auto [type, op] : operator_by_type) {
      auto arc_cost = GetLocalSearchArcCostCallback(parameters);
      local_search_operators_[type] =
          CostsAreHomogeneousAcrossVehicles()
              ? solver_->MakeOperator(nexts_, std::move(arc_cost), op)
              : solver_->MakeOperator(nexts_, vehicle_vars_,
                                      std::move(arc_cost), op);
    }
  }

  auto get_vehicle_vars = [this]() {
    return CostsAreHomogeneousAcrossVehicles() ? std::vector<IntVar*>()
                                               : vehicle_vars_;
  };

  // Other operators defined in the CP solver.
  local_search_operators_[RELOCATE] = MakeRelocate(
      solver_.get(), nexts_, get_vehicle_vars(), vehicle_start_class_callback_,
      get_incoming_neighbors, get_outgoing_neighbors);
  local_search_operators_[EXCHANGE] = MakeExchange(
      solver_.get(), nexts_, get_vehicle_vars(), vehicle_start_class_callback_,
      get_incoming_neighbors, get_outgoing_neighbors);
  local_search_operators_[CROSS] = MakeCross(
      solver_.get(), nexts_, get_vehicle_vars(), vehicle_start_class_callback_,
      get_incoming_neighbors, get_outgoing_neighbors);
  local_search_operators_[TWO_OPT] = MakeTwoOpt(
      solver_.get(), nexts_, get_vehicle_vars(), vehicle_start_class_callback_,
      get_incoming_neighbors, get_outgoing_neighbors);
  local_search_operators_[RELOCATE_AND_MAKE_ACTIVE] = RelocateAndMakeActive(
      solver_.get(), nexts_, get_vehicle_vars(), vehicle_start_class_callback_);
  local_search_operators_[MAKE_ACTIVE_AND_RELOCATE] = MakeActiveAndRelocate(
      solver_.get(), nexts_, get_vehicle_vars(), vehicle_start_class_callback_);
  local_search_operators_[EXCHANGE_AND_MAKE_ACTIVE] = ExchangeAndMakeActive(
      solver_.get(), nexts_, get_vehicle_vars(), vehicle_start_class_callback_);
  local_search_operators_[EXCHANGE_PATH_START_ENDS_AND_MAKE_ACTIVE] =
      ExchangePathStartEndsAndMakeActive(solver_.get(), nexts_,
                                         get_vehicle_vars(),
                                         vehicle_start_class_callback_);
  local_search_operators_[MAKE_CHAIN_INACTIVE] = MakeChainInactive(
      solver_.get(), nexts_, get_vehicle_vars(), vehicle_start_class_callback_);
  local_search_operators_[SWAP_ACTIVE] = MakeSwapActive(
      solver_.get(), nexts_, get_vehicle_vars(), vehicle_start_class_callback_);
  local_search_operators_[SWAP_ACTIVE_CHAIN] = MakeSwapActiveChain(
      solver_.get(), nexts_, get_vehicle_vars(), vehicle_start_class_callback_,
      parameters.max_swap_active_chain_size());
  local_search_operators_[EXTENDED_SWAP_ACTIVE] = MakeExtendedSwapActive(
      solver_.get(), nexts_, get_vehicle_vars(), vehicle_start_class_callback_);
  std::vector<std::vector<int64_t>> alternative_sets(disjunctions_.size());
  for (const RoutingModel::Disjunction& disjunction : disjunctions_) {
    // Only add disjunctions of cardinality 1 and of size > 1, as
    // SwapActiveToShortestPathOperator and TwoOptWithShortestPathOperator only
    // support DAGs, and don't care about chain-DAGS.
    if (disjunction.value.max_cardinality == 1 &&
        disjunction.indices.size() > 1) {
      alternative_sets.push_back(disjunction.indices);
    }
  }
  local_search_operators_[SHORTEST_PATH_SWAP_ACTIVE] =
      MakeSwapActiveToShortestPath(
          solver_.get(), nexts_, get_vehicle_vars(),
          vehicle_start_class_callback_, alternative_sets,
          GetLocalSearchHomogeneousArcCostCallback(parameters));
  // TODO(user): Consider having only one variant of 2Opt active.
  local_search_operators_[SHORTEST_PATH_TWO_OPT] = MakeTwoOptWithShortestPath(
      solver_.get(), nexts_, get_vehicle_vars(), vehicle_start_class_callback_,
      alternative_sets, GetLocalSearchHomogeneousArcCostCallback(parameters));

  // Routing-specific operators.
  local_search_operators_[MAKE_ACTIVE] = CreateInsertionOperator();
  local_search_operators_[MAKE_INACTIVE] = CreateMakeInactiveOperator();
  local_search_operators_[RELOCATE_PAIR] =
      MakePairRelocate(solver_.get(), nexts_, get_vehicle_vars(),
                       vehicle_start_class_callback_, pickup_delivery_pairs_);
  std::vector<LocalSearchOperator*> light_relocate_pair_operators;
  light_relocate_pair_operators.push_back(MakeLightPairRelocate(
      solver_.get(), nexts_, get_vehicle_vars(), vehicle_start_class_callback_,
      get_incoming_neighbors, get_outgoing_neighbors, pickup_delivery_pairs_,
      [this](int64_t start) {
        return vehicle_pickup_delivery_policy_[VehicleIndex(start)] ==
               RoutingModel::PICKUP_AND_DELIVERY_LIFO;
      }));
  light_relocate_pair_operators.push_back(MakeGroupPairAndRelocate(
      solver_.get(), nexts_, get_vehicle_vars(), vehicle_start_class_callback_,
      get_incoming_neighbors, get_outgoing_neighbors, pickup_delivery_pairs_));
  local_search_operators_[LIGHT_RELOCATE_PAIR] =
      solver_->ConcatenateOperators(light_relocate_pair_operators);
  local_search_operators_[EXCHANGE_PAIR] = solver_->ConcatenateOperators(
      {MakePairExchange(solver_.get(), nexts_, get_vehicle_vars(),
                        vehicle_start_class_callback_, get_incoming_neighbors,
                        get_outgoing_neighbors, pickup_delivery_pairs_),
       solver_->RevAlloc(new SwapIndexPairOperator(nexts_, get_vehicle_vars(),
                                                   pickup_delivery_pairs_))});
  local_search_operators_[EXCHANGE_RELOCATE_PAIR] = MakePairExchangeRelocate(
      solver_.get(), nexts_, get_vehicle_vars(), vehicle_start_class_callback_,
      pickup_delivery_pairs_);
  local_search_operators_[RELOCATE_NEIGHBORS] = MakeRelocateNeighbors(
      solver_.get(), nexts_, get_vehicle_vars(), vehicle_start_class_callback_,
      get_incoming_neighbors, get_outgoing_neighbors,
      GetLocalSearchHomogeneousArcCostCallback(parameters));
  local_search_operators_[NODE_PAIR_SWAP] = solver_->ConcatenateOperators(
      {MakeIndexPairSwapActive(solver_.get(), nexts_, get_vehicle_vars(),
                               vehicle_start_class_callback_,
                               pickup_delivery_pairs_),
       MakePairNodeSwapActive<true>(solver_.get(), nexts_, get_vehicle_vars(),
                                    vehicle_start_class_callback_,
                                    pickup_delivery_pairs_),
       MakePairNodeSwapActive<false>(solver_.get(), nexts_, get_vehicle_vars(),
                                     vehicle_start_class_callback_,
                                     pickup_delivery_pairs_)});
  local_search_operators_[RELOCATE_SUBTRIP] = MakeRelocateSubtrip(
      solver_.get(), nexts_, get_vehicle_vars(), vehicle_start_class_callback_,
      get_incoming_neighbors, get_outgoing_neighbors, pickup_delivery_pairs_);
  local_search_operators_[EXCHANGE_SUBTRIP] = MakeExchangeSubtrip(
      solver_.get(), nexts_, get_vehicle_vars(), vehicle_start_class_callback_,
      get_incoming_neighbors, get_outgoing_neighbors, pickup_delivery_pairs_);

  const auto arc_cost_for_path_start =
      [this, arc_cost_getter = GetLocalSearchArcCostCallback(parameters)](
          int64_t before_node, int64_t after_node, int64_t start_index) {
        const int vehicle = VehicleIndex(start_index);
        const int64_t arc_cost =
            arc_cost_getter(before_node, after_node, vehicle);
        return (before_node != start_index || IsEnd(after_node))
                   ? arc_cost
                   : CapSub(arc_cost, GetFixedCostOfVehicle(vehicle));
      };
  local_search_operators_[RELOCATE_EXPENSIVE_CHAIN] =
      MakeRelocateExpensiveChain(
          solver_.get(), nexts_, get_vehicle_vars(),
          vehicle_start_class_callback_,
          parameters.relocate_expensive_chain_num_arcs_to_consider(),
          arc_cost_for_path_start);

  // Insertion-based LNS neighborhoods.
  const auto make_global_cheapest_insertion_filtered_heuristic =
      [this, &parameters]() {
        return std::make_unique<GlobalCheapestInsertionFilteredHeuristic>(
            this, [this]() { return CheckLimit(time_buffer_); },
            GetLocalSearchArcCostCallback(parameters),
            absl::bind_front(&RoutingModel::UnperformedPenaltyOrValue, this, 0),
            GetOrCreateLocalSearchFilterManager(
                parameters,
                {/*filter_objective=*/false, /*filter_with_cp_solver=*/false}),
            parameters.global_cheapest_insertion_ls_operator_parameters(),
            /*is_sequential=*/false);
      };
  const auto make_local_cheapest_insertion_filtered_heuristic =
      [this, &parameters]() {
        const LocalCheapestInsertionParameters& lci_params =
            parameters.local_cheapest_insertion_parameters();
        return std::make_unique<LocalCheapestInsertionFilteredHeuristic>(
            this, [this]() { return CheckLimit(time_buffer_); },
            GetLocalSearchArcCostCallback(parameters), lci_params,
            GetOrCreateLocalSearchFilterManager(
                parameters,
                {/*filter_objective=*/false, /*filter_with_cp_solver=*/false}),
            /*use_first_solution_hint=*/false, bin_capacities_.get());
      };
  local_search_operators_[GLOBAL_CHEAPEST_INSERTION_VISIT_TYPES_LNS] =
      solver_->RevAlloc(new RelocateVisitTypeOperator(
          make_global_cheapest_insertion_filtered_heuristic()));
  local_search_operators_[LOCAL_CHEAPEST_INSERTION_VISIT_TYPES_LNS] =
      solver_->RevAlloc(new RelocateVisitTypeOperator(
          make_local_cheapest_insertion_filtered_heuristic()));
  local_search_operators_[GLOBAL_CHEAPEST_INSERTION_CLOSE_NODES_LNS] =
      solver_->RevAlloc(new FilteredHeuristicCloseNodesLNSOperator(
          make_global_cheapest_insertion_filtered_heuristic(),
          parameters.heuristic_close_nodes_lns_num_nodes()));

  local_search_operators_[LOCAL_CHEAPEST_INSERTION_CLOSE_NODES_LNS] =
      solver_->RevAlloc(new FilteredHeuristicCloseNodesLNSOperator(
          make_local_cheapest_insertion_filtered_heuristic(),
          parameters.heuristic_close_nodes_lns_num_nodes()));

  local_search_operators_[GLOBAL_CHEAPEST_INSERTION_PATH_LNS] =
      solver_->RevAlloc(new FilteredHeuristicPathLNSOperator(
          make_global_cheapest_insertion_filtered_heuristic()));

  local_search_operators_[LOCAL_CHEAPEST_INSERTION_PATH_LNS] =
      solver_->RevAlloc(new FilteredHeuristicPathLNSOperator(
          make_local_cheapest_insertion_filtered_heuristic()));

  local_search_operators_
      [RELOCATE_PATH_GLOBAL_CHEAPEST_INSERTION_INSERT_UNPERFORMED] =
          solver_->RevAlloc(
              new RelocatePathAndHeuristicInsertUnperformedOperator(
                  make_global_cheapest_insertion_filtered_heuristic()));

  local_search_operators_[GLOBAL_CHEAPEST_INSERTION_EXPENSIVE_CHAIN_LNS] =
      solver_->RevAlloc(new FilteredHeuristicExpensiveChainLNSOperator(
          make_global_cheapest_insertion_filtered_heuristic(),
          parameters.heuristic_expensive_chain_lns_num_arcs_to_consider(),
          arc_cost_for_path_start));

  local_search_operators_[LOCAL_CHEAPEST_INSERTION_EXPENSIVE_CHAIN_LNS] =
      solver_->RevAlloc(new FilteredHeuristicExpensiveChainLNSOperator(
          make_local_cheapest_insertion_filtered_heuristic(),
          parameters.heuristic_expensive_chain_lns_num_arcs_to_consider(),
          arc_cost_for_path_start));
}

#define CP_ROUTING_PUSH_OPERATOR(operator_type, operator_method)            \
  if (operators_to_consider.contains(operator_type) &&                      \
      search_parameters.local_search_operators().use_##operator_method() == \
          BOOL_TRUE) {                                                      \
    operators.push_back(local_search_operators_[operator_type]);            \
  }

LocalSearchOperator* RoutingModel::ConcatenateOperators(
    const RoutingSearchParameters& search_parameters,
    const std::vector<LocalSearchOperator*>& operators) const {
  if (search_parameters.use_multi_armed_bandit_concatenate_operators()) {
    return solver_->MultiArmedBanditConcatenateOperators(
        operators,
        search_parameters
            .multi_armed_bandit_compound_operator_memory_coefficient(),
        search_parameters
            .multi_armed_bandit_compound_operator_exploration_coefficient(),
        /*maximize=*/false);
  }
  return solver_->ConcatenateOperators(operators);
}

LocalSearchOperator* RoutingModel::GetNeighborhoodOperators(
    const RoutingSearchParameters& search_parameters,
    const absl::flat_hash_set<RoutingLocalSearchOperator>&
        operators_to_consider) const {
  std::vector<LocalSearchOperator*> operator_groups;
  std::vector<LocalSearchOperator*> operators = extra_operators_;
  if (!pickup_delivery_pairs_.empty()) {
    CP_ROUTING_PUSH_OPERATOR(RELOCATE_PAIR, relocate_pair);
    // Only add the light version of relocate pair if the normal version has not
    // already been added as it covers a subset of its neighborhood.
    if (search_parameters.local_search_operators().use_relocate_pair() ==
        BOOL_FALSE) {
      CP_ROUTING_PUSH_OPERATOR(LIGHT_RELOCATE_PAIR, light_relocate_pair);
    }
    CP_ROUTING_PUSH_OPERATOR(EXCHANGE_PAIR, exchange_pair);
    CP_ROUTING_PUSH_OPERATOR(NODE_PAIR_SWAP, node_pair_swap_active);
    CP_ROUTING_PUSH_OPERATOR(RELOCATE_SUBTRIP, relocate_subtrip);
    CP_ROUTING_PUSH_OPERATOR(EXCHANGE_SUBTRIP, exchange_subtrip);
  }
  if (vehicles_ > 1) {
    if (GetNumOfSingletonNodes() > 0) {
      // If there are only pairs in the model the only case where Relocate will
      // work is for intra-route moves, already covered by OrOpt.
      // We are not disabling Exchange and Cross because there are no
      // intra-route equivalents.
      CP_ROUTING_PUSH_OPERATOR(RELOCATE, relocate);
    }
    CP_ROUTING_PUSH_OPERATOR(EXCHANGE, exchange);
    CP_ROUTING_PUSH_OPERATOR(CROSS, cross);
  }
  if (!pickup_delivery_pairs_.empty() ||
      search_parameters.local_search_operators().use_relocate_neighbors() ==
          BOOL_TRUE) {
    operators.push_back(local_search_operators_[RELOCATE_NEIGHBORS]);
  }
  const LocalSearchMetaheuristic::Value local_search_metaheuristic =
      search_parameters.local_search_metaheuristic();
  if (local_search_metaheuristic != LocalSearchMetaheuristic::TABU_SEARCH &&
      local_search_metaheuristic !=
          LocalSearchMetaheuristic::GENERIC_TABU_SEARCH &&
      local_search_metaheuristic !=
          LocalSearchMetaheuristic::SIMULATED_ANNEALING) {
    CP_ROUTING_PUSH_OPERATOR(LIN_KERNIGHAN, lin_kernighan);
  }
  CP_ROUTING_PUSH_OPERATOR(TWO_OPT, two_opt);
  CP_ROUTING_PUSH_OPERATOR(OR_OPT, or_opt);
  CP_ROUTING_PUSH_OPERATOR(RELOCATE_EXPENSIVE_CHAIN, relocate_expensive_chain);
  size_t max_alternative_set_size = 0;
  for (const RoutingModel::Disjunction& disjunction : disjunctions_) {
    max_alternative_set_size =
        std::max(max_alternative_set_size, disjunction.indices.size());
  }
  if (!disjunctions_.empty()) {
    CP_ROUTING_PUSH_OPERATOR(MAKE_INACTIVE, make_inactive);
    CP_ROUTING_PUSH_OPERATOR(MAKE_CHAIN_INACTIVE, make_chain_inactive);
    CP_ROUTING_PUSH_OPERATOR(MAKE_ACTIVE, make_active);

    // The relocate_and_make_active parameter activates all neighborhoods
    // relocating a node together with making another active.
    CP_ROUTING_PUSH_OPERATOR(RELOCATE_AND_MAKE_ACTIVE,
                             relocate_and_make_active);
    CP_ROUTING_PUSH_OPERATOR(MAKE_ACTIVE_AND_RELOCATE,
                             relocate_and_make_active);

    CP_ROUTING_PUSH_OPERATOR(EXCHANGE_AND_MAKE_ACTIVE,
                             exchange_and_make_active);
    CP_ROUTING_PUSH_OPERATOR(EXCHANGE_PATH_START_ENDS_AND_MAKE_ACTIVE,
                             exchange_path_start_ends_and_make_active);

    CP_ROUTING_PUSH_OPERATOR(SWAP_ACTIVE, swap_active);
    CP_ROUTING_PUSH_OPERATOR(SWAP_ACTIVE_CHAIN, swap_active_chain);
    CP_ROUTING_PUSH_OPERATOR(EXTENDED_SWAP_ACTIVE, extended_swap_active);
    if (max_alternative_set_size > 1) {
      CP_ROUTING_PUSH_OPERATOR(SHORTEST_PATH_SWAP_ACTIVE,
                               shortest_path_swap_active);
      CP_ROUTING_PUSH_OPERATOR(SHORTEST_PATH_TWO_OPT, shortest_path_two_opt);
    }
  }
  LocalSearchOperator* main_operator_group =
      ConcatenateOperators(search_parameters, operators);

  // We concatenate heuristic LNS operators consecutively with the main group,
  // (by increasing complexity of the operators), replacing the main group with
  // this concatenation at each step.
  // These successive concatenations guarantee that adding the more complex
  // heuristic-LNS operators will always improve (or at least not degrade) the
  // quality of the local minimum solution, though they will increase the time
  // to reach it.
  operators.clear();
  if (vehicles() > 1) {
    // NOTE: The following heuristic path LNS with a single vehicle are
    // equivalent to using the heuristic as first solution strategy, so we
    // only add these moves if we have at least 2 vehicles in the model.
    CP_ROUTING_PUSH_OPERATOR(GLOBAL_CHEAPEST_INSERTION_PATH_LNS,
                             global_cheapest_insertion_path_lns);
    CP_ROUTING_PUSH_OPERATOR(LOCAL_CHEAPEST_INSERTION_PATH_LNS,
                             local_cheapest_insertion_path_lns);
    CP_ROUTING_PUSH_OPERATOR(
        RELOCATE_PATH_GLOBAL_CHEAPEST_INSERTION_INSERT_UNPERFORMED,
        relocate_path_global_cheapest_insertion_insert_unperformed);

    // NOTE: A subtlety here is that the path-LNS operators are concatenated
    // into one single group before concatenating it with the main group. This
    // is because the path-LNS operators are considerably faster than the arc
    // and node-based versions and are very effective at reducing the number of
    // routes, so we put them in a separate group to iterate on them as much as
    // possible before moving on to other operators (going back to the faster
    // main operators).
    LocalSearchOperator* path_lns_operator_group =
        ConcatenateOperators(search_parameters, operators);
    operators.assign({main_operator_group, path_lns_operator_group});
    main_operator_group = ConcatenateOperators(search_parameters, operators);
  }

  operators.assign({main_operator_group});
  CP_ROUTING_PUSH_OPERATOR(GLOBAL_CHEAPEST_INSERTION_EXPENSIVE_CHAIN_LNS,
                           global_cheapest_insertion_expensive_chain_lns);
  CP_ROUTING_PUSH_OPERATOR(LOCAL_CHEAPEST_INSERTION_EXPENSIVE_CHAIN_LNS,
                           local_cheapest_insertion_expensive_chain_lns);
  main_operator_group = ConcatenateOperators(search_parameters, operators);

  operators.assign({main_operator_group});
  CP_ROUTING_PUSH_OPERATOR(GLOBAL_CHEAPEST_INSERTION_CLOSE_NODES_LNS,
                           global_cheapest_insertion_close_nodes_lns);
  CP_ROUTING_PUSH_OPERATOR(LOCAL_CHEAPEST_INSERTION_CLOSE_NODES_LNS,
                           local_cheapest_insertion_close_nodes_lns);
  operator_groups.push_back(ConcatenateOperators(search_parameters, operators));

  operators.assign({main_operator_group});
  CP_ROUTING_PUSH_OPERATOR(GLOBAL_CHEAPEST_INSERTION_VISIT_TYPES_LNS,
                           global_cheapest_insertion_visit_types_lns);
  CP_ROUTING_PUSH_OPERATOR(LOCAL_CHEAPEST_INSERTION_VISIT_TYPES_LNS,
                           local_cheapest_insertion_visit_types_lns);
  operator_groups.push_back(ConcatenateOperators(search_parameters, operators));

  // Third local search loop: Expensive LNS operators.
  operators.clear();
  if (local_search_metaheuristic != LocalSearchMetaheuristic::TABU_SEARCH &&
      local_search_metaheuristic !=
          LocalSearchMetaheuristic::GENERIC_TABU_SEARCH &&
      local_search_metaheuristic !=
          LocalSearchMetaheuristic::SIMULATED_ANNEALING) {
    CP_ROUTING_PUSH_OPERATOR(TSP_OPT, tsp_opt);
  }
  if (local_search_metaheuristic != LocalSearchMetaheuristic::TABU_SEARCH &&
      local_search_metaheuristic !=
          LocalSearchMetaheuristic::GENERIC_TABU_SEARCH &&
      local_search_metaheuristic !=
          LocalSearchMetaheuristic::SIMULATED_ANNEALING) {
    CP_ROUTING_PUSH_OPERATOR(TSP_LNS, tsp_lns);
  }
  CP_ROUTING_PUSH_OPERATOR(FULL_PATH_LNS, full_path_lns);
  CP_ROUTING_PUSH_OPERATOR(PATH_LNS, path_lns);
  if (!disjunctions_.empty()) {
    CP_ROUTING_PUSH_OPERATOR(INACTIVE_LNS, inactive_lns);
  }
  operator_groups.push_back(ConcatenateOperators(search_parameters, operators));

  return solver_->ConcatenateOperators(operator_groups);
}

#undef CP_ROUTING_PUSH_OPERATOR

namespace {

void ConvertVectorInt64ToVectorInt(absl::Span<const int64_t> input,
                                   std::vector<int>* output) {
  const int n = input.size();
  output->resize(n);
  int* data = output->data();
  for (int i = 0; i < n; ++i) {
    const int element = static_cast<int>(input[i]);
    DCHECK_EQ(input[i], static_cast<int64_t>(element));
    data[i] = element;
  }
}

}  // namespace

std::vector<LocalSearchFilterManager::FilterEvent>
RoutingModel::CreateLocalSearchFilters(
    const RoutingSearchParameters& parameters, const FilterOptions& options) {
  const auto kAccept = LocalSearchFilterManager::FilterEventType::kAccept;
  const auto kRelax = LocalSearchFilterManager::FilterEventType::kRelax;
  // As of 2013/01, three filters evaluate sub-parts of the objective
  // function:
  // - NodeDisjunctionFilter: takes disjunction penalty costs into account,
  // - PathCumulFilter: takes dimension span costs into account,
  // - ObjectiveFilter:
  //     - VehicleAmortizedCostFilter, which considers the part of the cost
  //       related to amortized linear and quadratic vehicle cost factors.
  //     - LocalSearchObjectiveFilter, which takes dimension "arc" costs into
  //       account.
  std::vector<LocalSearchFilterManager::FilterEvent> filter_events;

  // VehicleAmortizedCostFilter can have a negative value, so it must be first.
  int priority = 0;
  if (options.filter_objective && vehicle_amortized_cost_factors_set_) {
    filter_events.push_back(
        {MakeVehicleAmortizedCostFilter(*this), kAccept, priority});
  }

  // The SumObjectiveFilter has the best reject/second ratio in practice,
  // so it is the earliest.
  ++priority;
  if (options.filter_objective) {
    if (CostsAreHomogeneousAcrossVehicles()) {
      LocalSearchFilter* sum = solver_->MakeSumObjectiveFilter(
          nexts_,
          [this](int64_t i, int64_t j) { return GetHomogeneousCost(i, j); },
          Solver::LE);
      filter_events.push_back({sum, kAccept, priority});
    } else {
      LocalSearchFilter* sum = solver_->MakeSumObjectiveFilter(
          nexts_, vehicle_vars_,
          [this](int64_t i, int64_t j, int64_t k) {
            return GetArcCostForVehicle(i, j, k);
          },
          Solver::LE);
      filter_events.push_back({sum, kAccept, priority});
    }
  }
  const PathState* path_state_reference = nullptr;
  {
    std::vector<int> path_starts;
    std::vector<int> path_ends;
    ConvertVectorInt64ToVectorInt(paths_metadata_.Starts(), &path_starts);
    ConvertVectorInt64ToVectorInt(paths_metadata_.Ends(), &path_ends);
    auto path_state = std::make_unique<PathState>(
        Size() + vehicles(), std::move(path_starts), std::move(path_ends));
    path_state_reference = path_state.get();
    filter_events.push_back(
        {MakePathStateFilter(solver_.get(), std::move(path_state), Nexts()),
         kRelax, priority});
  }

  {
    ++priority;
    filter_events.push_back(
        {solver_->MakeVariableDomainFilter(), kAccept, priority});

    if (vehicles_ > max_active_vehicles_) {
      filter_events.push_back(
          {MakeMaxActiveVehiclesFilter(*this), kAccept, priority});
    }

    bool has_same_activity_constraints = false;
    for (int node = 0; node < Size(); ++node) {
      if (GetSameVehicleIndicesOfIndex(node).size() > 1) {
        has_same_activity_constraints = true;
        break;
      }
    }
    if (has_same_activity_constraints) {
      filter_events.push_back(
          {MakeActiveNodeGroupFilter(*this), kAccept, priority});
    }
    if (!GetOrderedActivityGroups().empty()) {
      filter_events.push_back(
          {MakeOrderedActivityGroupFilter(*this), kAccept, priority});
    }

    if (!disjunctions_.empty()) {
      if (options.filter_objective || HasMandatoryDisjunctions() ||
          HasMaxCardinalityConstrainedDisjunctions()) {
        filter_events.push_back(
            {MakeNodeDisjunctionFilter(*this, options.filter_objective),
             kAccept, priority});
      }
    }
    if (!same_vehicle_costs_.empty()) {
      if (options.filter_objective) {
        filter_events.push_back(
            {MakeSameVehicleCostFilter(*this), kAccept, priority});
      }
    }

    // If vehicle costs are not homogeneous, vehicle variables will be added to
    // local search deltas and their domain will be checked by
    // VariableDomainFilter.
    if (CostsAreHomogeneousAcrossVehicles()) {
      filter_events.push_back(
          {MakeVehicleVarFilter(*this, path_state_reference), kAccept,
           priority});
    }

    // Append filters, then overwrite preset priority to current priority.
    // TODO(user): Merge Append*DimensionFilters in one procedure, needs
    // to revisit priorities so they reflect complexity less arbitrarily.
    const int first_lightweight_index = filter_events.size();
    AppendLightWeightDimensionFilters(path_state_reference, GetDimensions(),
                                      &filter_events);
    for (int e = first_lightweight_index; e < filter_events.size(); ++e) {
      filter_events[e].priority = priority;
    }
  }

  // As of 10/2021, TypeRegulationsFilter assumes pickup and delivery
  // constraints are enforced, therefore PickupDeliveryFilter must be
  // called first.
  ++priority;
  if (!pickup_delivery_pairs_.empty()) {
    LocalSearchFilter* filter = MakePickupDeliveryFilter(
        *this, path_state_reference, pickup_delivery_pairs_,
        vehicle_pickup_delivery_policy_);
    filter_events.push_back({filter, kRelax, priority});
    filter_events.push_back({filter, kAccept, priority});
  }
  if (options.filter_objective) {
    const int num_vehicles = vehicles();
    for (const auto& [force_distance, energy_costs] :
         force_distance_to_energy_costs_) {
      const auto& [force, distance] = force_distance;
      const RoutingDimension* force_dimension = GetMutableDimension(force);
      DCHECK_NE(force_dimension, nullptr);
      if (force_dimension == nullptr) continue;
      std::vector<int64_t> force_start_min(num_vehicles);
      std::vector<int64_t> force_end_min(num_vehicles);
      std::vector<int> force_class(num_vehicles);
      std::vector<const std::function<int64_t(int64_t)>*> force_evaluators;
      for (int v = 0; v < num_vehicles; ++v) {
        force_start_min[v] = force_dimension->GetCumulVarMin(Start(v));
        force_end_min[v] = force_dimension->GetCumulVarMin(End(v));
        const int c = force_dimension->vehicle_to_class(v);
        force_class[v] = c;
        if (c >= force_evaluators.size()) {
          force_evaluators.resize(c + 1, nullptr);
        }
        if (force_evaluators[c] == nullptr) {
          force_evaluators[c] = &(force_dimension->GetUnaryTransitEvaluator(v));
          DCHECK(force_evaluators[c] != nullptr);
          if (force_evaluators[c] == nullptr) continue;
        }
      }
      const RoutingDimension* distance_dimension =
          GetMutableDimension(distance);
      DCHECK_NE(distance_dimension, nullptr);
      if (distance_dimension == nullptr) continue;
      std::vector<int> distance_class(num_vehicles);
      std::vector<const std::function<int64_t(int64_t, int64_t)>*>
          distance_evaluators;
      for (int v = 0; v < num_vehicles; ++v) {
        const int c = distance_dimension->vehicle_to_class(v);
        distance_class[v] = c;
        if (c >= distance_evaluators.size())
          distance_evaluators.resize(c + 1, nullptr);
        if (distance_evaluators[c] == nullptr) {
          distance_evaluators[c] =
              &(distance_dimension->GetBinaryTransitEvaluator(v));
        }
      }
      std::vector<PathEnergyCostChecker::EnergyCost> path_energy_costs;
      for (const auto& limit : energy_costs) {
        path_energy_costs.push_back({
            .threshold = limit.threshold,
            .cost_per_unit_below_threshold =
                limit.cost_per_unit_below_threshold,
            .cost_per_unit_above_threshold =
                limit.cost_per_unit_above_threshold,
        });
      }
      auto checker = std::make_unique<PathEnergyCostChecker>(
          path_state_reference, std::move(force_start_min),
          std::move(force_end_min), std::move(force_class),
          std::move(force_evaluators), std::move(distance_class),
          std::move(distance_evaluators), std::move(path_energy_costs),
          vehicle_used_when_empty_);
      filter_events.push_back(
          {MakePathEnergyCostFilter(solver(), std::move(checker),
                                    absl::StrCat(force_dimension->name(),
                                                 distance_dimension->name())),
           kAccept, priority});
    }
  }

  if (HasTypeRegulations()) {
    ++priority;
    filter_events.push_back(
        {MakeTypeRegulationsFilter(*this), kAccept, priority});
  }

  {
    ++priority;
    const int first_dimension_filter_index = filter_events.size();
    AppendDimensionCumulFilters(
        GetDimensions(), parameters, options.filter_objective,
        /* filter_light_weight_dimensions */ false, &filter_events);
    int max_priority = priority;
    for (int e = first_dimension_filter_index; e < filter_events.size(); ++e) {
      filter_events[e].priority += priority;
      max_priority = std::max(max_priority, filter_events[e].priority);
    }
    priority = max_priority;
  }

  if (!route_evaluators_.empty()) {
    ++priority;
    filter_events.push_back(
        {MakeRouteConstraintFilter(*this), kAccept, priority});
  }

  if (!extra_filters_.empty()) {
    ++priority;
    for (const auto& event : extra_filters_) {
      filter_events.push_back({event.filter, event.event_type, priority});
    }
  }

  if (options.filter_with_cp_solver) {
    ++priority;
    filter_events.push_back({MakeCPFeasibilityFilter(this), kAccept, priority});
  }
  return filter_events;
}

LocalSearchFilterManager* RoutingModel::GetOrCreateLocalSearchFilterManager(
    const RoutingSearchParameters& parameters, const FilterOptions& options) {
  LocalSearchFilterManager* local_search_filter_manager =
      gtl::FindPtrOrNull(local_search_filter_managers_, options);
  if (local_search_filter_manager == nullptr) {
    local_search_filter_manager =
        solver_->RevAlloc(new LocalSearchFilterManager(
            CreateLocalSearchFilters(parameters, options)));
    local_search_filter_managers_[options] = local_search_filter_manager;
  }
  return local_search_filter_manager;
}

std::unique_ptr<BinCapacities> MakeBinCapacities(
    const std::vector<RoutingDimension*>& dimensions,
    const PathsMetadata& paths_metadata) {
  const int num_vehicles = paths_metadata.NumPaths();
  auto bin_capacities = std::make_unique<BinCapacities>(num_vehicles);
  std::vector<BinCapacities::LoadLimit> load_limits;
  for (const RoutingDimension* dimension : dimensions) {
    // If the dimension is not unary, skip.
    if (dimension->GetUnaryTransitEvaluator(0) == nullptr) continue;
    // If the dimension has no constant-signed transit evaluator, skip.
    if (dimension->AllTransitEvaluatorSignsAreUnknown()) continue;
    // For each vehicle, if the sign of its evaluator is constant,
    // set a transit evaluator to pass to BinCapacities.
    load_limits.assign(num_vehicles, {.max_load = kint64max,
                                      .soft_max_load = 0,
                                      .cost_above_soft_max_load = 0});
    for (int vehicle = 0; vehicle < num_vehicles; ++vehicle) {
      const RoutingModel::TransitEvaluatorSign sign =
          dimension->GetTransitEvaluatorSign(vehicle);
      if (sign == RoutingModel::kTransitEvaluatorSignUnknown) continue;
      // Vehicle load changes monotonically along the route.
      // If transit signs are >= 0, the min load is at start, the max at end.
      // If transit signs are <= 0, the max load is at start, the min at end.
      // The encoding into BinCapacities associates a bin dimension with this
      // routing dimension, with bin capacity = vehicle capacity - min load,
      // and bin item size = abs(transit(node)).
      int64_t min_node = paths_metadata.Starts()[vehicle];
      int64_t max_node = paths_metadata.Ends()[vehicle];
      if (sign == RoutingModel::kTransitEvaluatorSignNegativeOrZero) {
        std::swap(min_node, max_node);
      }
      const int64_t load_min =
          std::max<int64_t>(0, dimension->CumulVar(min_node)->Min());
      const int64_t load_max =
          std::min(dimension->vehicle_capacities()[vehicle],
                   dimension->CumulVar(max_node)->Max());
      load_limits[vehicle].max_load = CapSub(load_max, load_min);
      if (dimension->HasCumulVarSoftUpperBound(max_node)) {
        load_limits[vehicle].soft_max_load =
            CapSub(dimension->GetCumulVarSoftUpperBound(max_node), load_min);
        load_limits[vehicle].cost_above_soft_max_load =
            dimension->GetCumulVarSoftUpperBoundCoefficient(max_node);
      }
    }
    bin_capacities->AddDimension(
        [dimension](int node, int vehicle) {
          return CapAbs(dimension->GetUnaryTransitEvaluator(vehicle)(node));
        },
        load_limits);
  }
  if (bin_capacities->NumDimensions() == 0) bin_capacities.reset(nullptr);
  return bin_capacities;
}

namespace {
bool AllTransitsPositive(const RoutingDimension& dimension) {
  for (int vehicle = 0; vehicle < dimension.model()->vehicles(); vehicle++) {
    if (!dimension.AreVehicleTransitsPositive(vehicle)) {
      return false;
    }
  }
  return true;
}
}  // namespace

void RoutingModel::StoreDimensionCumulOptimizers(
    const RoutingSearchParameters& parameters) {
  Assignment* optimized_dimensions_collector_assignment =
      solver_->MakeAssignment();
  optimized_dimensions_collector_assignment->AddObjective(CostVar());
  const int num_dimensions = dimensions_.size();
  local_optimizer_index_.resize(num_dimensions, -1);
  global_optimizer_index_.resize(num_dimensions, -1);
  if (parameters.disable_scheduling_beware_this_may_degrade_performance()) {
    return;
  }
  for (DimensionIndex dim = DimensionIndex(0); dim < num_dimensions; dim++) {
    RoutingDimension* dimension = dimensions_[dim];
    DCHECK_EQ(dimension->model(), this);
    const int num_resource_groups =
        GetDimensionResourceGroupIndices(dimension).size();
    bool needs_optimizer = false;
    if (dimension->global_span_cost_coefficient() > 0 ||
        !dimension->GetNodePrecedences().empty() || num_resource_groups > 1) {
      // Use global optimizer.
      needs_optimizer = true;
      global_optimizer_index_[dim] = global_dimension_optimizers_.size();
      global_dimension_optimizers_.push_back(
          {std::make_unique<GlobalDimensionCumulOptimizer>(
               dimension, parameters.continuous_scheduling_solver(),
               &search_stats_),
           std::make_unique<GlobalDimensionCumulOptimizer>(
               dimension, parameters.mixed_integer_scheduling_solver(),
               &search_stats_)});
      if (!AllTransitsPositive(*dimension)) {
        dimension->SetOffsetForGlobalOptimizer(0);
      } else {
        int64_t offset =
            vehicles() == 0 ? 0 : std::numeric_limits<int64_t>::max();
        for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
          DCHECK_GE(dimension->CumulVar(Start(vehicle))->Min(), 0);
          offset =
              std::min(offset, dimension->CumulVar(Start(vehicle))->Min() - 1);
        }
        if (dimension->HasBreakConstraints()) {
          for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
            for (const IntervalVar* br :
                 dimension->GetBreakIntervalsOfVehicle(vehicle)) {
              offset = std::min(offset, CapSub(br->StartMin(), 1));
            }
          }
        }

        dimension->SetOffsetForGlobalOptimizer(std::max(Zero(), offset));
      }
    }
    // Check if we need the local optimizer.
    bool has_span_cost = false;
    bool has_span_limit = false;
    std::vector<int64_t> vehicle_offsets(vehicles());
    for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
      if (dimension->GetSpanCostCoefficientForVehicle(vehicle) > 0) {
        has_span_cost = true;
      }
      if (dimension->GetSpanUpperBoundForVehicle(vehicle) <
          std::numeric_limits<int64_t>::max()) {
        has_span_limit = true;
      }
      DCHECK_GE(dimension->CumulVar(Start(vehicle))->Min(), 0);
      int64_t offset = 0;
      if (dimension->AreVehicleTransitsPositive(vehicle)) {
        offset = CapSub(dimension->CumulVar(Start(vehicle))->Min(), 1);
        if (dimension->HasBreakConstraints()) {
          for (const IntervalVar* br :
               dimension->GetBreakIntervalsOfVehicle(vehicle)) {
            offset = std::min(offset, CapSub(br->StartMin(), 1));
          }
        }
      }
      vehicle_offsets[vehicle] = std::max<int64_t>(0, offset);
    }
    bool has_soft_lower_bound = false;
    bool has_soft_upper_bound = false;
    for (int i = 0; i < dimension->cumuls().size(); ++i) {
      if (dimension->HasCumulVarSoftLowerBound(i)) {
        has_soft_lower_bound = true;
      }
      if (dimension->HasCumulVarSoftUpperBound(i)) {
        has_soft_upper_bound = true;
      }
    }
    int num_linear_constraints = 0;
    if (has_span_cost) ++num_linear_constraints;
    if (has_span_limit) ++num_linear_constraints;
    if (dimension->HasSoftSpanUpperBounds()) ++num_linear_constraints;
    if (dimension->HasQuadraticCostSoftSpanUpperBounds()) {
      ++num_linear_constraints;
    }
    if (has_soft_lower_bound) ++num_linear_constraints;
    if (has_soft_upper_bound) ++num_linear_constraints;
    if (dimension->HasBreakConstraints()) ++num_linear_constraints;
    if (num_resource_groups > 0 || num_linear_constraints >= 2) {
      needs_optimizer = true;
      dimension->SetVehicleOffsetsForLocalOptimizer(std::move(vehicle_offsets));
      local_optimizer_index_[dim] = local_dimension_optimizers_.size();
      local_dimension_optimizers_.push_back(
          {std::make_unique<LocalDimensionCumulOptimizer>(
               dimension, parameters.continuous_scheduling_solver(),
               &search_stats_),
           std::make_unique<LocalDimensionCumulOptimizer>(
               dimension, parameters.mixed_integer_scheduling_solver(),
               &search_stats_)});
    }
    if (needs_optimizer) {
      optimized_dimensions_collector_assignment->Add(dimension->cumuls());
    }
  }

  // NOTE(b/129252839): We also add all other extra variables to the
  // optimized_dimensions_collector_assignment to make sure the necessary
  // propagations on these variables after packing/optimizing are correctly
  // stored.
  for (IntVar* const extra_var : extra_vars_) {
    optimized_dimensions_collector_assignment->Add(extra_var);
  }
  for (IntervalVar* const extra_interval : extra_intervals_) {
    optimized_dimensions_collector_assignment->Add(extra_interval);
  }

  optimized_dimensions_assignment_collector_ =
      solver_->MakeFirstSolutionCollector(
          optimized_dimensions_collector_assignment);
}

std::vector<RoutingDimension*> RoutingModel::GetDimensionsWithSoftOrSpanCosts()
    const {
  std::vector<RoutingDimension*> dimensions;
  for (RoutingDimension* dimension : dimensions_) {
    bool has_soft_or_span_cost = false;
    for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
      if (dimension->GetSpanCostCoefficientForVehicle(vehicle) > 0) {
        has_soft_or_span_cost = true;
        break;
      }
    }
    if (!has_soft_or_span_cost) {
      for (int i = 0; i < dimension->cumuls().size(); ++i) {
        if (dimension->HasCumulVarSoftUpperBound(i) ||
            dimension->HasCumulVarSoftLowerBound(i)) {
          has_soft_or_span_cost = true;
          break;
        }
      }
    }
    if (has_soft_or_span_cost) dimensions.push_back(dimension);
  }
  return dimensions;
}

std::vector<RoutingDimension*> RoutingModel::GetUnaryDimensions() const {
  std::vector<RoutingDimension*> unary_dimensions;
  for (RoutingDimension* dim : dimensions_) {
    if (dim->IsUnary()) {
      unary_dimensions.push_back(dim);
    }
  }
  return unary_dimensions;
}

std::vector<const RoutingDimension*>
RoutingModel::GetDimensionsWithGlobalCumulOptimizers() const {
  DCHECK(closed_);
  std::vector<const RoutingDimension*> global_optimizer_dimensions;
  for (auto& [lp_optimizer, mp_optimizer] : global_dimension_optimizers_) {
    DCHECK_NE(lp_optimizer.get(), nullptr);
    DCHECK_NE(mp_optimizer.get(), nullptr);
    global_optimizer_dimensions.push_back(lp_optimizer->dimension());
  }
  return global_optimizer_dimensions;
}

std::vector<const RoutingDimension*>
RoutingModel::GetDimensionsWithLocalCumulOptimizers() const {
  DCHECK(closed_);
  std::vector<const RoutingDimension*> local_optimizer_dimensions;
  for (auto& [lp_optimizer, mp_optimizer] : local_dimension_optimizers_) {
    DCHECK_NE(lp_optimizer.get(), nullptr);
    DCHECK_NE(mp_optimizer.get(), nullptr);
    local_optimizer_dimensions.push_back(lp_optimizer->dimension());
  }
  return local_optimizer_dimensions;
}

bool RoutingModel::AreRoutesInterdependent(
    const RoutingSearchParameters& parameters) const {
  //  By default, GENERIC_TABU_SEARCH applies tabu search on the cost variable.
  //  This can potentially modify variables appearing in the cost function which
  //  do not belong to modified routes, creating a dependency between routes.
  //  Similarly, the plateau avoidance criteria of TABU_SEARCH can constrain the
  //  cost variable, with the same consequences.
  if (parameters.local_search_metaheuristic() ==
          LocalSearchMetaheuristic::GENERIC_TABU_SEARCH ||
      parameters.local_search_metaheuristic() ==
          LocalSearchMetaheuristic::TABU_SEARCH) {
    return true;
  }
  for (RoutingDimension* const dim : dimensions_) {
    if (!GetDimensionResourceGroupIndices(dim).empty() ||
        HasGlobalCumulOptimizer(*dim)) {
      return true;
    }
  }
  return false;
}

DecisionBuilder* RoutingModel::CreateSolutionFinalizer(
    const RoutingSearchParameters& parameters, SearchLimit* lns_limit) {
  std::vector<DecisionBuilder*> decision_builders;
  decision_builders.push_back(solver_->MakePhase(
      nexts_, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE));
  if (!AreRoutesInterdependent(parameters)) {
    // When routes are interdependent, optimal dimension values of unchanged
    // routes might be affected by changes on other routes, so we only add the
    // RestoreDimensionValuesForUnchangedRoutes decision builder when routes
    // aren't interdependent.
    decision_builders.push_back(
        MakeRestoreDimensionValuesForUnchangedRoutes(this));
  }
  const bool can_use_dimension_cumul_optimizers =
      !parameters.disable_scheduling_beware_this_may_degrade_performance();
  DCHECK(local_dimension_optimizers_.empty() ||
         can_use_dimension_cumul_optimizers);
  for (auto& [lp_optimizer, mp_optimizer] : local_dimension_optimizers_) {
    const RoutingDimension* const dim = lp_optimizer->dimension();
    if (HasGlobalCumulOptimizer(*dim)) {
      // Don't set cumuls of dimensions having a global optimizer.
      continue;
    }
    DCHECK_LE(GetDimensionResourceGroupIndices(dim).size(), 1);
    decision_builders.push_back(MakeSetCumulsFromLocalDimensionCosts(
        solver_.get(), lp_optimizer.get(), mp_optimizer.get()));
  }

  DCHECK(global_dimension_optimizers_.empty() ||
         can_use_dimension_cumul_optimizers);
  for (auto& [lp_optimizer, mp_optimizer] : global_dimension_optimizers_) {
    decision_builders.push_back(MakeSetCumulsFromGlobalDimensionCosts(
        solver_.get(), lp_optimizer.get(), mp_optimizer.get(), lns_limit));
  }
  decision_builders.push_back(finalizer_variables_->CreateFinalizer());

  return solver_->Compose(decision_builders);
}

void RoutingModel::CreateFirstSolutionDecisionBuilders(
    const RoutingSearchParameters& search_parameters) {
  first_solution_decision_builders_.resize(
      FirstSolutionStrategy_Value_Value_ARRAYSIZE, nullptr);
  first_solution_filtered_decision_builders_.resize(
      FirstSolutionStrategy_Value_Value_ARRAYSIZE, nullptr);
  DecisionBuilder* const finalize_solution = CreateSolutionFinalizer(
      search_parameters, GetOrCreateLargeNeighborhoodSearchLimit());
  // Default heuristic
  first_solution_decision_builders_
      [FirstSolutionStrategy::FIRST_UNBOUND_MIN_VALUE] = finalize_solution;
  // Global cheapest addition heuristic.
  first_solution_decision_builders_
      [FirstSolutionStrategy::GLOBAL_CHEAPEST_ARC] = solver_->MakePhase(
          nexts_,
          [this](int64_t i, int64_t j) {
            return GetArcCostForFirstSolution(i, j);
          },
          Solver::CHOOSE_STATIC_GLOBAL_BEST);
  // Cheapest addition heuristic.
  Solver::IndexEvaluator2 eval = [this](int64_t i, int64_t j) {
    return GetArcCostForFirstSolution(i, j);
  };
  first_solution_decision_builders_[FirstSolutionStrategy::LOCAL_CHEAPEST_ARC] =
      solver_->MakePhase(nexts_, Solver::CHOOSE_FIRST_UNBOUND, eval);
  // Path-based cheapest addition heuristic.
  first_solution_decision_builders_[FirstSolutionStrategy::PATH_CHEAPEST_ARC] =
      solver_->MakePhase(nexts_, Solver::CHOOSE_PATH, eval);
  if (!search_parameters.use_unfiltered_first_solution_strategy()) {
    first_solution_filtered_decision_builders_
        [FirstSolutionStrategy::PATH_CHEAPEST_ARC] =
            CreateIntVarFilteredDecisionBuilder<
                EvaluatorCheapestAdditionFilteredHeuristic>(
                eval,
                GetOrCreateLocalSearchFilterManager(
                    search_parameters, {/*filter_objective=*/false,
                                        /*filter_with_cp_solver=*/false}));
    first_solution_decision_builders_
        [FirstSolutionStrategy::PATH_CHEAPEST_ARC] =
            solver_->Try(first_solution_filtered_decision_builders_
                             [FirstSolutionStrategy::PATH_CHEAPEST_ARC],
                         first_solution_decision_builders_
                             [FirstSolutionStrategy::PATH_CHEAPEST_ARC]);
  }
  // Path-based most constrained arc addition heuristic.
  Solver::VariableValueComparator comp = [this](int64_t i, int64_t j,
                                                int64_t k) {
    return ArcIsMoreConstrainedThanArc(i, j, k);
  };

  first_solution_decision_builders_
      [FirstSolutionStrategy::PATH_MOST_CONSTRAINED_ARC] =
          solver_->MakePhase(nexts_, Solver::CHOOSE_PATH, comp);
  if (!search_parameters.use_unfiltered_first_solution_strategy()) {
    first_solution_filtered_decision_builders_
        [FirstSolutionStrategy::PATH_MOST_CONSTRAINED_ARC] =
            CreateIntVarFilteredDecisionBuilder<
                ComparatorCheapestAdditionFilteredHeuristic>(
                comp,
                GetOrCreateLocalSearchFilterManager(
                    search_parameters, {/*filter_objective=*/false,
                                        /*filter_with_cp_solver=*/false}));
    first_solution_decision_builders_
        [FirstSolutionStrategy::PATH_MOST_CONSTRAINED_ARC] = solver_->Try(
            first_solution_filtered_decision_builders_
                [FirstSolutionStrategy::PATH_MOST_CONSTRAINED_ARC],
            first_solution_decision_builders_
                [FirstSolutionStrategy::PATH_MOST_CONSTRAINED_ARC]);
  }
  // Evaluator-based path heuristic.
  if (first_solution_evaluator_ != nullptr) {
    first_solution_decision_builders_
        [FirstSolutionStrategy::EVALUATOR_STRATEGY] = solver_->MakePhase(
            nexts_, Solver::CHOOSE_PATH, first_solution_evaluator_);
  } else {
    first_solution_decision_builders_
        [FirstSolutionStrategy::EVALUATOR_STRATEGY] = nullptr;
  }
  // All unperformed heuristic.
  first_solution_decision_builders_[FirstSolutionStrategy::ALL_UNPERFORMED] =
      MakeAllUnperformed(this);
  // Best insertion heuristic.
  RegularLimit* const ls_limit = solver_->MakeLimit(
      GetTimeLimit(search_parameters), std::numeric_limits<int64_t>::max(),
      std::numeric_limits<int64_t>::max(), std::numeric_limits<int64_t>::max(),
      /*smart_time_check=*/true);
  DecisionBuilder* const finalize = solver_->MakeSolveOnce(
      finalize_solution, GetOrCreateLargeNeighborhoodSearchLimit());
  LocalSearchPhaseParameters* const insertion_parameters =
      solver_->MakeLocalSearchPhaseParameters(
          nullptr, CreateInsertionOperator(), finalize, ls_limit,
          GetOrCreateLocalSearchFilterManager(
              search_parameters,
              {/*filter_objective=*/true, /*filter_with_cp_solver=*/false}));
  std::vector<IntVar*> decision_vars = nexts_;
  if (!CostsAreHomogeneousAcrossVehicles()) {
    decision_vars.insert(decision_vars.end(), vehicle_vars_.begin(),
                         vehicle_vars_.end());
  }
  const int64_t optimization_step = std::max(
      MathUtil::SafeRound<int64_t>(search_parameters.optimization_step()),
      One());
  first_solution_decision_builders_[FirstSolutionStrategy::BEST_INSERTION] =
      solver_->MakeNestedOptimize(
          solver_->MakeLocalSearchPhase(decision_vars, MakeAllUnperformed(this),
                                        insertion_parameters),
          GetOrCreateAssignment(), false, optimization_step);
  first_solution_decision_builders_[FirstSolutionStrategy::BEST_INSERTION] =
      solver_->Compose(first_solution_decision_builders_
                           [FirstSolutionStrategy::BEST_INSERTION],
                       finalize);

  // Parallel/Sequential Global cheapest insertion
  for (bool is_sequential : {false, true}) {
    FirstSolutionStrategy::Value first_solution_strategy =
        is_sequential ? FirstSolutionStrategy::SEQUENTIAL_CHEAPEST_INSERTION
                      : FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION;

    first_solution_filtered_decision_builders_[first_solution_strategy] =
        CreateIntVarFilteredDecisionBuilder<
            GlobalCheapestInsertionFilteredHeuristic>(
            [this](int64_t i, int64_t j, int64_t vehicle) {
              return GetArcCostForVehicle(i, j, vehicle);
            },
            [this](int64_t i) { return UnperformedPenaltyOrValue(0, i); },
            GetOrCreateLocalSearchFilterManager(
                search_parameters, {/*filter_objective=*/false,
                                    /*filter_with_cp_solver=*/false}),
            search_parameters
                .global_cheapest_insertion_first_solution_parameters(),
            is_sequential);
    IntVarFilteredDecisionBuilder* const strong_gci =
        CreateIntVarFilteredDecisionBuilder<
            GlobalCheapestInsertionFilteredHeuristic>(
            [this](int64_t i, int64_t j, int64_t vehicle) {
              return GetArcCostForVehicle(i, j, vehicle);
            },
            [this](int64_t i) { return UnperformedPenaltyOrValue(0, i); },
            GetOrCreateLocalSearchFilterManager(
                search_parameters, {/*filter_objective=*/false,
                                    /*filter_with_cp_solver=*/true}),
            search_parameters
                .global_cheapest_insertion_first_solution_parameters(),
            is_sequential);
    first_solution_decision_builders_[first_solution_strategy] = solver_->Try(
        first_solution_filtered_decision_builders_[first_solution_strategy],
        solver_->Try(strong_gci, first_solution_decision_builders_
                                     [FirstSolutionStrategy::BEST_INSERTION]));
  }

  // Local cheapest insertion
  std::function<bool(const std::vector<VariableValuePair>&,
                     std::vector<VariableValuePair>*)>
      optimize_on_insertion;
  if (secondary_model_ != nullptr) {
    secondary_model_->QuietCloseModelWithParameters(secondary_parameters_);
    secondary_optimizer_ = std::make_unique<SecondaryOptimizer>(
        secondary_model_, &secondary_parameters_,
        search_parameters.first_solution_optimization_period());
    optimize_on_insertion = absl::bind_front(&SecondaryOptimizer::Solve,
                                             secondary_optimizer_.get());
  }
  const LocalCheapestInsertionParameters& lci_params =
      search_parameters.local_cheapest_insertion_parameters();
  first_solution_filtered_decision_builders_
      [FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION] =
          CreateIntVarFilteredDecisionBuilder<
              LocalCheapestInsertionFilteredHeuristic>(
              [this](int64_t i, int64_t j, int64_t vehicle) {
                return GetArcCostForVehicle(i, j, vehicle);
              },
              lci_params,
              GetOrCreateLocalSearchFilterManager(
                  search_parameters, {/*filter_objective=*/false,
                                      /*filter_with_cp_solver=*/false}),
              /*use_first_solution_hint=*/true, bin_capacities_.get(),
              optimize_on_insertion);
  IntVarFilteredDecisionBuilder* const strong_lci =
      CreateIntVarFilteredDecisionBuilder<
          LocalCheapestInsertionFilteredHeuristic>(
          [this](int64_t i, int64_t j, int64_t vehicle) {
            return GetArcCostForVehicle(i, j, vehicle);
          },
          lci_params,
          GetOrCreateLocalSearchFilterManager(search_parameters,
                                              {/*filter_objective=*/false,
                                               /*filter_with_cp_solver=*/true}),
          /*use_first_solution_hint=*/true, bin_capacities_.get(),
          optimize_on_insertion);
  first_solution_decision_builders_
      [FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION] = solver_->Try(
          first_solution_filtered_decision_builders_
              [FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION],
          solver_->Try(strong_lci,
                       first_solution_decision_builders_
                           [FirstSolutionStrategy::BEST_INSERTION]));

  // Local cheapest cost insertion
  const LocalCheapestInsertionParameters& lcci_params =
      search_parameters.local_cheapest_cost_insertion_parameters();
  first_solution_filtered_decision_builders_
      [FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION] =
          CreateIntVarFilteredDecisionBuilder<
              LocalCheapestInsertionFilteredHeuristic>(
              /*evaluator=*/nullptr, lcci_params,
              GetOrCreateLocalSearchFilterManager(
                  search_parameters, {/*filter_objective=*/true,
                                      /*filter_with_cp_solver=*/false}),
              /*use_first_solution_hint=*/true, bin_capacities_.get(),
              optimize_on_insertion);
  IntVarFilteredDecisionBuilder* const strong_lcci =
      CreateIntVarFilteredDecisionBuilder<
          LocalCheapestInsertionFilteredHeuristic>(
          /*evaluator=*/nullptr, lcci_params,
          GetOrCreateLocalSearchFilterManager(search_parameters,
                                              {/*filter_objective=*/true,
                                               /*filter_with_cp_solver=*/true}),
          /*use_first_solution_hint=*/true, bin_capacities_.get(),
          optimize_on_insertion);
  first_solution_decision_builders_
      [FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION] = solver_->Try(
          first_solution_filtered_decision_builders_
              [FirstSolutionStrategy::LOCAL_CHEAPEST_COST_INSERTION],
          solver_->Try(strong_lcci,
                       first_solution_decision_builders_
                           [FirstSolutionStrategy::BEST_INSERTION]));

  // Savings
  LocalSearchFilterManager* filter_manager = nullptr;
  if (!search_parameters.use_unfiltered_first_solution_strategy()) {
    filter_manager = GetOrCreateLocalSearchFilterManager(
        search_parameters,
        {/*filter_objective=*/false, /*filter_with_cp_solver=*/false});
  }

  IntVarFilteredDecisionBuilder* parallel_savings_db =
      CreateIntVarFilteredDecisionBuilder<ParallelSavingsFilteredHeuristic>(
          search_parameters.savings_parameters(), filter_manager);
  if (!search_parameters.use_unfiltered_first_solution_strategy()) {
    first_solution_filtered_decision_builders_
        [FirstSolutionStrategy::PARALLEL_SAVINGS] = parallel_savings_db;
  }

  first_solution_decision_builders_[FirstSolutionStrategy::PARALLEL_SAVINGS] =
      solver_->Try(
          parallel_savings_db,
          CreateIntVarFilteredDecisionBuilder<ParallelSavingsFilteredHeuristic>(
              search_parameters.savings_parameters(),
              GetOrCreateLocalSearchFilterManager(
                  search_parameters, {/*filter_objective=*/false,
                                      /*filter_with_cp_solver=*/true})));

  IntVarFilteredDecisionBuilder* sequential_savings_db =
      CreateIntVarFilteredDecisionBuilder<SequentialSavingsFilteredHeuristic>(
          search_parameters.savings_parameters(), filter_manager);
  if (!search_parameters.use_unfiltered_first_solution_strategy()) {
    first_solution_filtered_decision_builders_[FirstSolutionStrategy::SAVINGS] =
        sequential_savings_db;
  }

  first_solution_decision_builders_[FirstSolutionStrategy::SAVINGS] =
      solver_->Try(
          sequential_savings_db,
          CreateIntVarFilteredDecisionBuilder<
              SequentialSavingsFilteredHeuristic>(
              search_parameters.savings_parameters(),
              GetOrCreateLocalSearchFilterManager(
                  search_parameters, {/*filter_objective=*/false,
                                      /*filter_with_cp_solver=*/true})));

  // Sweep
  first_solution_decision_builders_[FirstSolutionStrategy::SWEEP] =
      MakeSweepDecisionBuilder(this, true);
  DecisionBuilder* sweep_builder = MakeSweepDecisionBuilder(this, false);
  first_solution_decision_builders_[FirstSolutionStrategy::SWEEP] =
      solver_->Try(
          sweep_builder,
          first_solution_decision_builders_[FirstSolutionStrategy::SWEEP]);
  // Christofides
  first_solution_decision_builders_[FirstSolutionStrategy::CHRISTOFIDES] =
      CreateIntVarFilteredDecisionBuilder<ChristofidesFilteredHeuristic>(
          GetOrCreateLocalSearchFilterManager(
              search_parameters, {/*filter_objective=*/false,
                                  /*filter_with_cp_solver=*/false}),
          search_parameters.christofides_use_minimum_matching());
  // Automatic
  const bool has_precedences = std::any_of(
      dimensions_.begin(), dimensions_.end(),
      [](RoutingDimension* dim) { return !dim->GetNodePrecedences().empty(); });
  bool has_single_vehicle_node = false;
  for (int node = 0; node < Size(); node++) {
    if (!IsStart(node) && !IsEnd(node) && allowed_vehicles_[node].size() == 1) {
      has_single_vehicle_node = true;
      break;
    }
  }
  automatic_first_solution_strategy_ =
      AutomaticFirstSolutionStrategy(!pickup_delivery_pairs_.empty(),
                                     has_precedences, has_single_vehicle_node);
  first_solution_decision_builders_[FirstSolutionStrategy::AUTOMATIC] =
      first_solution_decision_builders_[automatic_first_solution_strategy_];
  first_solution_decision_builders_[FirstSolutionStrategy::UNSET] =
      first_solution_decision_builders_[FirstSolutionStrategy::AUTOMATIC];

  // Naming decision builders to clarify profiling.
  for (FirstSolutionStrategy_Value strategy =
           FirstSolutionStrategy_Value_Value_MIN;
       strategy <= FirstSolutionStrategy_Value_Value_MAX;
       strategy = FirstSolutionStrategy_Value(strategy + 1)) {
    if (first_solution_decision_builders_[strategy] == nullptr ||
        strategy == FirstSolutionStrategy::AUTOMATIC) {
      continue;
    }
    const std::string strategy_name =
        FirstSolutionStrategy_Value_Name(strategy);
    const std::string& log_tag = search_parameters.log_tag();
    if (!log_tag.empty() && log_tag != strategy_name) {
      first_solution_decision_builders_[strategy]->set_name(absl::StrFormat(
          "%s / %s", strategy_name, search_parameters.log_tag()));
    } else {
      first_solution_decision_builders_[strategy]->set_name(strategy_name);
    }
  }
}

DecisionBuilder* RoutingModel::GetFirstSolutionDecisionBuilder(
    const RoutingSearchParameters& search_parameters) const {
  const FirstSolutionStrategy::Value first_solution_strategy =
      search_parameters.first_solution_strategy();
  if (first_solution_strategy < first_solution_decision_builders_.size()) {
    return first_solution_decision_builders_[first_solution_strategy];
  } else {
    return nullptr;
  }
}

IntVarFilteredDecisionBuilder*
RoutingModel::GetFilteredFirstSolutionDecisionBuilderOrNull(
    const RoutingSearchParameters& search_parameters) const {
  const FirstSolutionStrategy::Value first_solution_strategy =
      search_parameters.first_solution_strategy();
  return first_solution_filtered_decision_builders_[first_solution_strategy];
}

template <typename Heuristic, typename... Args>
IntVarFilteredDecisionBuilder*
RoutingModel::CreateIntVarFilteredDecisionBuilder(const Args&... args) {
  return solver_->RevAlloc(
      new IntVarFilteredDecisionBuilder(std::make_unique<Heuristic>(
          this, [this]() { return CheckLimit(time_buffer_); }, args...)));
}

LocalSearchPhaseParameters* RoutingModel::CreateLocalSearchParameters(
    const RoutingSearchParameters& search_parameters, bool secondary_ls) {
  SearchLimit* lns_limit = GetOrCreateLargeNeighborhoodSearchLimit();
  absl::flat_hash_set<RoutingLocalSearchOperator> operators_to_consider;
  LocalSearchOperator* ls_operator = nullptr;
  if (secondary_ls) {
    if (secondary_ls_operator_ == nullptr) {
      operators_to_consider = {TWO_OPT,
                               OR_OPT,
                               LIN_KERNIGHAN,
                               MAKE_INACTIVE,
                               MAKE_CHAIN_INACTIVE,
                               SHORTEST_PATH_SWAP_ACTIVE,
                               SHORTEST_PATH_TWO_OPT};
      secondary_ls_operator_ =
          GetNeighborhoodOperators(search_parameters, operators_to_consider);
    }
    ls_operator = secondary_ls_operator_;
  } else {
    if (primary_ls_operator_ == nullptr) {
      // Consider all operators for the primary LS phase.
      for (int op = 0; op < LOCAL_SEARCH_OPERATOR_COUNTER; ++op) {
        operators_to_consider.insert(RoutingLocalSearchOperator(op));
      }
      primary_ls_operator_ =
          GetNeighborhoodOperators(search_parameters, operators_to_consider);
    }
    ls_operator = primary_ls_operator_;
  }
  return solver_->MakeLocalSearchPhaseParameters(
      CostVar(), ls_operator,
      solver_->MakeSolveOnce(
          CreateSolutionFinalizer(search_parameters, lns_limit), lns_limit),
      GetOrCreateLocalSearchLimit(),
      GetOrCreateLocalSearchFilterManager(
          search_parameters,
          {/*filter_objective=*/true, /*filter_with_cp_solver=*/false}));
}

DecisionBuilder* RoutingModel::CreatePrimaryLocalSearchDecisionBuilder(
    const RoutingSearchParameters& search_parameters) {
  const int size = Size();
  DecisionBuilder* first_solution =
      GetFirstSolutionDecisionBuilder(search_parameters);
  LocalSearchPhaseParameters* const parameters =
      CreateLocalSearchParameters(search_parameters, /*secondary_ls=*/false);
  SearchLimit* first_solution_lns_limit =
      GetOrCreateFirstSolutionLargeNeighborhoodSearchLimit();
  DecisionBuilder* const first_solution_sub_decision_builder =
      solver_->MakeSolveOnce(
          CreateSolutionFinalizer(search_parameters, first_solution_lns_limit),
          first_solution_lns_limit);
  if (CostsAreHomogeneousAcrossVehicles()) {
    return solver_->MakeLocalSearchPhase(nexts_, first_solution,
                                         first_solution_sub_decision_builder,
                                         parameters);
  }
  const int all_size = size + size + vehicles_;
  std::vector<IntVar*> all_vars(all_size);
  for (int i = 0; i < size; ++i) {
    all_vars[i] = nexts_[i];
  }
  for (int i = size; i < all_size; ++i) {
    all_vars[i] = vehicle_vars_[i - size];
  }
  return solver_->MakeLocalSearchPhase(all_vars, first_solution,
                                       first_solution_sub_decision_builder,
                                       parameters);
}

void RoutingModel::SetupDecisionBuilders(
    const RoutingSearchParameters& search_parameters) {
  if (search_parameters.use_depth_first_search()) {
    SearchLimit* first_lns_limit =
        GetOrCreateFirstSolutionLargeNeighborhoodSearchLimit();
    solve_db_ = solver_->Compose(
        GetFirstSolutionDecisionBuilder(search_parameters),
        solver_->MakeSolveOnce(
            CreateSolutionFinalizer(search_parameters, first_lns_limit),
            first_lns_limit));
  } else {
    solve_db_ = CreatePrimaryLocalSearchDecisionBuilder(search_parameters);
  }
  CHECK(preassignment_ != nullptr);
  DecisionBuilder* restore_preassignment =
      solver_->MakeRestoreAssignment(preassignment_);
  solve_db_ = solver_->Compose(restore_preassignment, solve_db_);

  improve_db_ =
      solver_->Compose(restore_preassignment,
                       solver_->MakeLocalSearchPhase(
                           GetOrCreateAssignment(),
                           CreateLocalSearchParameters(
                               search_parameters, /*secondary_ls=*/false)));

  secondary_ls_db_ = solver_->MakeLocalSearchPhase(
      GetOrCreateAssignment(),
      CreateLocalSearchParameters(search_parameters, /*secondary_ls=*/true));
  secondary_ls_db_ = solver_->Compose(restore_preassignment, secondary_ls_db_);

  restore_assignment_ = solver_->Compose(
      solver_->MakeRestoreAssignment(GetOrCreateAssignment()),
      CreateSolutionFinalizer(search_parameters,
                              GetOrCreateLargeNeighborhoodSearchLimit()));
  restore_tmp_assignment_ = solver_->Compose(
      restore_preassignment,
      solver_->MakeRestoreAssignment(GetOrCreateTmpAssignment()),
      CreateSolutionFinalizer(search_parameters,
                              GetOrCreateLargeNeighborhoodSearchLimit()));
}

void RoutingModel::SetupMetaheuristics(
    const RoutingSearchParameters& search_parameters) {
  BaseObjectiveMonitor* optimize = nullptr;
  const auto build_metaheuristic = [this, &search_parameters](
                                       LocalSearchMetaheuristic::Value
                                           metaheuristic) {
    BaseObjectiveMonitor* optimize = nullptr;
    // Some metaheuristics will effectively never terminate; warn
    // user if they fail to set a time limit.
    bool limit_too_long = !search_parameters.has_time_limit() &&
                          search_parameters.solution_limit() ==
                              std::numeric_limits<int64_t>::max();
    const int64_t optimization_step = std::max(
        MathUtil::SafeRound<int64_t>(search_parameters.optimization_step()),
        One());
    switch (metaheuristic) {
      case LocalSearchMetaheuristic::GUIDED_LOCAL_SEARCH: {
        std::function<std::vector<std::pair<int64_t, int64_t>>(int64_t,
                                                               int64_t)>
            same_class_arc_getter;
        if (search_parameters
                .guided_local_search_penalize_with_vehicle_classes()) {
          same_class_arc_getter =
              absl::bind_front(&RoutingModel::GetSameVehicleClassArcs, this);
        }
        if (CostsAreHomogeneousAcrossVehicles()) {
          optimize = solver_->MakeGuidedLocalSearch(
              false, cost_,
              [this](int64_t i, int64_t j) { return GetHomogeneousCost(i, j); },
              optimization_step, nexts_,
              search_parameters.guided_local_search_lambda_coefficient(),
              std::move(same_class_arc_getter),
              search_parameters
                  .guided_local_search_reset_penalties_on_new_best_solution());
        } else {
          optimize = solver_->MakeGuidedLocalSearch(
              false, cost_,
              [this](int64_t i, int64_t j, int64_t k) {
                return GetArcCostForVehicle(i, j, k);
              },
              optimization_step, nexts_, vehicle_vars_,
              search_parameters.guided_local_search_lambda_coefficient(),
              std::move(same_class_arc_getter),
              search_parameters
                  .guided_local_search_reset_penalties_on_new_best_solution());
        }
        break;
      }
      case LocalSearchMetaheuristic::SIMULATED_ANNEALING:
        optimize = solver_->MakeSimulatedAnnealing(false, cost_,
                                                   optimization_step, 100);
        break;
      case LocalSearchMetaheuristic::TABU_SEARCH:
        optimize = solver_->MakeTabuSearch(false, cost_, optimization_step,
                                           nexts_, 10, 10, .8);
        break;
      case LocalSearchMetaheuristic::GENERIC_TABU_SEARCH: {
        std::vector<operations_research::IntVar*> tabu_vars;
        if (tabu_var_callback_) {
          tabu_vars = tabu_var_callback_(this);
        } else {
          tabu_vars.push_back(cost_);
        }
        optimize = solver_->MakeGenericTabuSearch(
            false, cost_, optimization_step, tabu_vars, 100);
        break;
      }
      default:
        limit_too_long = false;
        OptimizeVar* const minimize =
            solver_->MakeMinimize(cost_, optimization_step);
        optimize = minimize;
        minimize->SetOnOptimalFoundcallback([this](int64_t value) {
          objective_lower_bound_ = std::max(objective_lower_bound_, value);
        });
    }
    if (limit_too_long) {
      LOG(WARNING) << LocalSearchMetaheuristic::Value_Name(metaheuristic)
                   << " specified without sane timeout: solve may run forever.";
    }
    return optimize;
  };
  if (!search_parameters.local_search_metaheuristics().empty()) {
    std::vector<BaseObjectiveMonitor*> metaheuristics;
    for (int i = 0; i < search_parameters.local_search_metaheuristics_size();
         ++i) {
      metaheuristics.push_back(build_metaheuristic(
          search_parameters.local_search_metaheuristics(i)));
    }
    optimize = solver_->MakeRoundRobinCompoundObjectiveMonitor(
        metaheuristics,
        search_parameters.num_max_local_optima_before_metaheuristic_switch());
  } else {
    optimize =
        build_metaheuristic(search_parameters.local_search_metaheuristic());
  }
  metaheuristic_ = optimize;
  monitors_.push_back(optimize);
  secondary_ls_monitors_.push_back(optimize);
}

void RoutingModel::SetTabuVarsCallback(GetTabuVarsCallback tabu_var_callback) {
  tabu_var_callback_ = std::move(tabu_var_callback);
}

void RoutingModel::SetupAssignmentCollector(
    const RoutingSearchParameters& search_parameters) {
  Assignment* full_assignment = solver_->MakeAssignment();
  for (const RoutingDimension* const dimension : dimensions_) {
    full_assignment->Add(dimension->cumuls());
  }
  for (IntVar* const extra_var : extra_vars_) {
    full_assignment->Add(extra_var);
  }
  for (IntervalVar* const extra_interval : extra_intervals_) {
    full_assignment->Add(extra_interval);
  }
  full_assignment->Add(nexts_);
  full_assignment->Add(active_);
  full_assignment->Add(vehicle_vars_);
  full_assignment->AddObjective(cost_);

  collect_assignments_ = solver_->MakeNBestValueSolutionCollector(
      full_assignment, search_parameters.number_of_solutions_to_collect(),
      false);
  collect_secondary_ls_assignments_ = solver_->MakeNBestValueSolutionCollector(
      full_assignment, search_parameters.number_of_solutions_to_collect(),
      false);
  collect_one_assignment_ =
      solver_->MakeFirstSolutionCollector(full_assignment);
  monitors_.push_back(collect_assignments_);
  secondary_ls_monitors_.push_back(collect_secondary_ls_assignments_);
}

void RoutingModel::SetupTrace(
    const RoutingSearchParameters& search_parameters) {
  if (search_parameters.log_search()) {
    Solver::SearchLogParameters search_log_parameters;
    search_log_parameters.branch_period = 10000;
    search_log_parameters.objective = nullptr;
    search_log_parameters.variables = {cost_};
    search_log_parameters.scaling_factors = {
        search_parameters.log_cost_scaling_factor()};
    search_log_parameters.offsets = {search_parameters.log_cost_offset()};
    if (!search_parameters.log_tag().empty()) {
      const std::string& tag = search_parameters.log_tag();
      search_log_parameters.display_callback = [tag]() { return tag; };
    } else {
      search_log_parameters.display_callback = nullptr;
    }
    search_log_parameters.display_on_new_solutions_only = false;
    search_log_ = solver_->MakeSearchLog(search_log_parameters);
    monitors_.push_back(search_log_);
    secondary_ls_monitors_.push_back(
        solver_->MakeSearchLog(std::move(search_log_parameters)));
  }
}

void RoutingModel::SetupImprovementLimit(
    const RoutingSearchParameters& search_parameters) {
  if (!search_parameters.has_improvement_limit_parameters()) return;

  SearchMonitor* const improvement_limit = solver_->MakeImprovementLimit(
      cost_, /*maximize=*/false, search_parameters.log_cost_scaling_factor(),
      search_parameters.log_cost_offset(),
      search_parameters.improvement_limit_parameters()
          .improvement_rate_coefficient(),
      search_parameters.improvement_limit_parameters()
          .improvement_rate_solutions_distance());
  monitors_.push_back(improvement_limit);
  secondary_ls_monitors_.push_back(improvement_limit);
}

namespace {

template <typename EndInitialPropagationCallback, typename LocalOptimumCallback>
class LocalOptimumWatcher : public SearchMonitor {
 public:
  LocalOptimumWatcher(
      Solver* solver,
      EndInitialPropagationCallback end_initial_propagation_callback,
      LocalOptimumCallback local_optimum_callback)
      : SearchMonitor(solver),
        end_initial_propagation_callback_(
            std::move(end_initial_propagation_callback)),
        local_optimum_callback_(std::move(local_optimum_callback)) {}
  void Install() override {
    ListenToEvent(Solver::MonitorEvent::kEndInitialPropagation);
    ListenToEvent(Solver::MonitorEvent::kLocalOptimum);
  }
  void EndInitialPropagation() override { end_initial_propagation_callback_(); }
  bool AtLocalOptimum() override {
    local_optimum_callback_();
    return SearchMonitor::AtLocalOptimum();
  }

 private:
  EndInitialPropagationCallback end_initial_propagation_callback_;
  LocalOptimumCallback local_optimum_callback_;
};

template <typename EndInitialPropagationCallback, typename LocalOptimumCallback>
SearchMonitor* MakeLocalOptimumWatcher(
    Solver* solver,
    EndInitialPropagationCallback end_initial_propagation_callback,
    LocalOptimumCallback local_optimum_callback) {
  return solver->RevAlloc(new LocalOptimumWatcher<EndInitialPropagationCallback,
                                                  LocalOptimumCallback>(
      solver, std::move(end_initial_propagation_callback),
      std::move(local_optimum_callback)));
}

}  // namespace

void RoutingModel::SetupSearchMonitors(
    const RoutingSearchParameters& search_parameters) {
  std::vector<SearchMonitor*> old_monitors = monitors_;
  monitors_.clear();
  for (int i = 0; i < monitors_before_setup_; ++i) {
    monitors_.push_back(old_monitors[i]);
  }
  monitors_.push_back(GetOrCreateLimit());
  monitors_.push_back(MakeLocalOptimumWatcher(
      solver(),
      [this]() {
        objective_lower_bound_ =
            std::max(objective_lower_bound_, CostVar()->Min());
      },
      [this]() { local_optimum_reached_ = true; }));
  monitors_.push_back(
      solver_->MakeCustomLimit([this]() -> bool { return interrupt_cp_; }));

  secondary_ls_monitors_ = monitors_;

  SetupImprovementLimit(search_parameters);
  SetupMetaheuristics(search_parameters);
  SetupAssignmentCollector(search_parameters);
  SetupTrace(search_parameters);
  int new_monitors_after_setup = monitors_.size();
  for (int i = monitors_after_setup_; i < old_monitors.size(); ++i) {
    monitors_.push_back(old_monitors[i]);
  }
  monitors_after_setup_ = new_monitors_after_setup;
}

bool RoutingModel::UsesLightPropagation(
    const RoutingSearchParameters& search_parameters) const {
  return !search_parameters.use_full_propagation() &&
         !search_parameters.use_depth_first_search() &&
         search_parameters.first_solution_strategy() !=
             FirstSolutionStrategy::FIRST_UNBOUND_MIN_VALUE;
}

void RoutingModel::AddWeightedVariableTargetToFinalizer(IntVar* var,
                                                        int64_t target,
                                                        int64_t cost) {
  finalizer_variables_->AddWeightedVariableTarget(var, target, cost);
}

void RoutingModel::AddWeightedVariableMinimizedByFinalizer(IntVar* var,
                                                           int64_t cost) {
  finalizer_variables_->AddWeightedVariableTarget(var, kint64min, cost);
}

void RoutingModel::AddWeightedVariableMaximizedByFinalizer(IntVar* var,
                                                           int64_t cost) {
  finalizer_variables_->AddWeightedVariableTarget(var, kint64max, cost);
}

void RoutingModel::AddVariableTargetToFinalizer(IntVar* var, int64_t target) {
  finalizer_variables_->AddVariableTarget(var, target);
}

void RoutingModel::AddVariableMaximizedByFinalizer(IntVar* var) {
  finalizer_variables_->AddVariableTarget(var, kint64max);
}

void RoutingModel::AddVariableMinimizedByFinalizer(IntVar* var) {
  finalizer_variables_->AddVariableTarget(var, kint64min);
}

void RoutingModel::SetupSearch(
    const RoutingSearchParameters& search_parameters) {
  const std::string error =
      FindErrorInSearchParametersForModel(search_parameters);
  if (!error.empty()) {
    status_ = RoutingSearchStatus::ROUTING_INVALID;
    LOG(ERROR) << "Invalid RoutingSearchParameters for this model: " << error;
    return;
  }
  SetupDecisionBuilders(search_parameters);
  SetupSearchMonitors(search_parameters);
  search_parameters_ = search_parameters;
}

void RoutingModel::UpdateSearchFromParametersIfNeeded(
    const RoutingSearchParameters& search_parameters) {
  // TODO(user): Cache old configs instead of overwriting them. This will
  // avoid consuming extra memory for configs that were already considered.
  if (!google::protobuf::util::MessageDifferencer::Equivalent(
          search_parameters_, search_parameters)) {
    status_ = RoutingSearchStatus::ROUTING_NOT_SOLVED;
    std::string error = FindErrorInRoutingSearchParameters(search_parameters);
    if (!error.empty()) {
      status_ = RoutingSearchStatus::ROUTING_INVALID;
      LOG(ERROR) << "Invalid RoutingSearchParameters: " << error;
    } else {
      SetupSearch(search_parameters);
    }
  }
  VLOG(1) << "Search parameters:\n" << search_parameters;
}

void RoutingModel::AddToAssignment(IntVar* const var) {
  extra_vars_.push_back(var);
}

void RoutingModel::AddIntervalToAssignment(IntervalVar* const interval) {
  extra_intervals_.push_back(interval);
}

const char RoutingModelVisitor::kLightElement[] = "LightElement";
const char RoutingModelVisitor::kLightElement2[] = "LightElement2";
const char RoutingModelVisitor::kRemoveValues[] = "RemoveValues";

RoutingDimension::RoutingDimension(RoutingModel* model,
                                   std::vector<int64_t> vehicle_capacities,
                                   const std::string& name,
                                   const RoutingDimension* base_dimension)
    : vehicle_capacities_(std::move(vehicle_capacities)),
      base_dimension_(base_dimension),
      global_span_cost_coefficient_(0),
      model_(model),
      index_(model->dimensions_.size()),
      name_(name),
      global_optimizer_offset_(0) {
  CHECK(model != nullptr);
  vehicle_span_upper_bounds_.assign(model->vehicles(),
                                    std::numeric_limits<int64_t>::max());
  vehicle_span_cost_coefficients_.assign(model->vehicles(), 0);
  vehicle_slack_cost_coefficients_.assign(model->vehicles(), 0);
}

RoutingDimension::RoutingDimension(RoutingModel* model,
                                   std::vector<int64_t> vehicle_capacities,
                                   const std::string& name, SelfBased)
    : RoutingDimension(model, std::move(vehicle_capacities), name, this) {}

RoutingDimension::~RoutingDimension() {
  cumul_var_piecewise_linear_cost_.clear();
}

void RoutingDimension::Initialize(
    absl::Span<const int> transit_evaluators,
    absl::Span<const int> cumul_dependent_transit_evaluators,
    absl::Span<const int> state_dependent_transit_evaluators,
    int64_t slack_max) {
  InitializeCumuls();
  InitializeTransits(transit_evaluators, cumul_dependent_transit_evaluators,
                     state_dependent_transit_evaluators, slack_max);
}

void RoutingDimension::InitializeCumuls() {
  Solver* const solver = model_->solver();
  const int size = model_->Size() + model_->vehicles();
  const auto capacity_range = std::minmax_element(vehicle_capacities_.begin(),
                                                  vehicle_capacities_.end());
  const int64_t min_capacity = *capacity_range.first;
  CHECK_GE(min_capacity, 0);
  const int64_t max_capacity = *capacity_range.second;
  solver->MakeIntVarArray(size, 0, max_capacity, name_, &cumuls_);
  // Refine the min/max for vehicle start/ends based on vehicle capacities.
  for (int v = 0; v < model_->vehicles(); v++) {
    const int64_t vehicle_capacity = vehicle_capacities_[v];
    cumuls_[model_->Start(v)]->SetMax(vehicle_capacity);
    cumuls_[model_->End(v)]->SetMax(vehicle_capacity);
  }

  forbidden_intervals_.resize(size);
  capacity_vars_.clear();
  if (min_capacity != max_capacity) {
    solver->MakeIntVarArray(size, 0, std::numeric_limits<int64_t>::max(),
                            &capacity_vars_);
    for (int i = 0; i < size; ++i) {
      IntVar* const capacity_var = capacity_vars_[i];
      if (i < model_->Size()) {
        IntVar* const capacity_active = solver->MakeBoolVar();
        solver->AddConstraint(
            solver->MakeLessOrEqual(model_->ActiveVar(i), capacity_active));
        solver->AddConstraint(solver->MakeIsLessOrEqualCt(
            cumuls_[i], capacity_var, capacity_active));
      } else {
        solver->AddConstraint(
            solver->MakeLessOrEqual(cumuls_[i], capacity_var));
      }
    }
  }
}

namespace {
void ComputeTransitClasses(absl::Span<const int> evaluator_indices,
                           std::vector<int>* class_evaluators,
                           std::vector<int>* vehicle_to_class) {
  CHECK(class_evaluators != nullptr);
  CHECK(vehicle_to_class != nullptr);
  class_evaluators->clear();
  vehicle_to_class->resize(evaluator_indices.size(), -1);
  absl::flat_hash_map<int, int64_t> evaluator_to_class;
  for (int i = 0; i < evaluator_indices.size(); ++i) {
    const int evaluator_index = evaluator_indices[i];
    const int new_class = class_evaluators->size();
    const int evaluator_class =
        gtl::LookupOrInsert(&evaluator_to_class, evaluator_index, new_class);
    if (evaluator_class == new_class) {
      class_evaluators->push_back(evaluator_index);
    }
    (*vehicle_to_class)[i] = evaluator_class;
  }
}
}  // namespace

void RoutingDimension::InitializeTransitVariables(int64_t slack_max) {
  CHECK(!class_evaluators_.empty());
  CHECK(base_dimension_ == nullptr ||
        !state_dependent_class_evaluators_.empty());

  Solver* const solver = model_->solver();
  const int size = model_->Size();
  const Solver::IndexEvaluator1 dependent_vehicle_class_function =
      [this](int index) {
        return (0 <= index && index < state_dependent_vehicle_to_class_.size())
                   ? state_dependent_vehicle_to_class_[index]
                   : state_dependent_class_evaluators_.size();
      };
  const std::string slack_name = name_ + " slack";
  const std::string transit_name = name_ + " fixed transit";

  bool are_all_evaluators_positive = true;
  for (int class_evaluator : class_evaluators_) {
    if (model()->transit_evaluator_sign_[class_evaluator] !=
        RoutingModel::kTransitEvaluatorSignPositiveOrZero) {
      are_all_evaluators_positive = false;
      break;
    }
  }
  const bool is_unary = IsUnary();
  for (int64_t i = 0; i < size; ++i) {
    int64_t min_fixed_transit = std::numeric_limits<int64_t>::max();
    if (is_unary) {
      for (int evaluator_index : class_evaluators_) {
        const auto& unary_transit_callback =
            model_->UnaryTransitCallbackOrNull(evaluator_index);
        DCHECK(unary_transit_callback != nullptr);
        min_fixed_transit =
            std::min(min_fixed_transit, unary_transit_callback(i));
      }
    }
    fixed_transits_[i] = solver->MakeIntVar(
        is_unary                      ? min_fixed_transit
        : are_all_evaluators_positive ? int64_t{0}
                                      : std::numeric_limits<int64_t>::min(),
        std::numeric_limits<int64_t>::max(), absl::StrCat(transit_name, i));
    // Setting dependent_transits_[i].
    if (base_dimension_ != nullptr) {
      if (state_dependent_class_evaluators_.size() == 1) {
        std::vector<IntVar*> transition_variables(cumuls_.size(), nullptr);
        for (int64_t j = 0; j < cumuls_.size(); ++j) {
          transition_variables[j] =
              MakeRangeMakeElementExpr(
                  model_
                      ->StateDependentTransitCallback(
                          state_dependent_class_evaluators_[0])(i, j)
                      .transit,
                  base_dimension_->CumulVar(i), solver)
                  ->Var();
        }
        dependent_transits_[i] =
            solver->MakeElement(transition_variables, model_->NextVar(i))
                ->Var();
      } else {
        IntVar* const vehicle_class_var =
            solver
                ->MakeElement(dependent_vehicle_class_function,
                              model_->VehicleVar(i))
                ->Var();
        std::vector<IntVar*> transit_for_vehicle;
        transit_for_vehicle.reserve(state_dependent_class_evaluators_.size() +
                                    1);
        for (int evaluator : state_dependent_class_evaluators_) {
          std::vector<IntVar*> transition_variables(cumuls_.size(), nullptr);
          for (int64_t j = 0; j < cumuls_.size(); ++j) {
            transition_variables[j] =
                MakeRangeMakeElementExpr(
                    model_->StateDependentTransitCallback(evaluator)(i, j)
                        .transit,
                    base_dimension_->CumulVar(i), solver)
                    ->Var();
          }
          transit_for_vehicle.push_back(
              solver->MakeElement(transition_variables, model_->NextVar(i))
                  ->Var());
        }
        transit_for_vehicle.push_back(solver->MakeIntConst(0));
        dependent_transits_[i] =
            solver->MakeElement(transit_for_vehicle, vehicle_class_var)->Var();
      }
    } else {
      dependent_transits_[i] = solver->MakeIntConst(0);
    }

    // Summing fixed transits, dependent transits and the slack.
    IntExpr* transit_expr = fixed_transits_[i];
    if (dependent_transits_[i]->Min() != 0 ||
        dependent_transits_[i]->Max() != 0) {
      transit_expr = solver->MakeSum(transit_expr, dependent_transits_[i]);
    }

    if (slack_max == 0) {
      slacks_[i] = solver->MakeIntConst(0);
    } else {
      slacks_[i] =
          solver->MakeIntVar(0, slack_max, absl::StrCat(slack_name, i));
      transit_expr = solver->MakeSum(slacks_[i], transit_expr);
    }
    transits_[i] = transit_expr->Var();
  }
}

void RoutingDimension::InitializeTransits(
    absl::Span<const int> transit_evaluators,
    absl::Span<const int> cumul_dependent_transit_evaluators,
    absl::Span<const int> state_dependent_transit_evaluators,
    int64_t slack_max) {
  CHECK_EQ(model_->vehicles(), transit_evaluators.size());
  CHECK(base_dimension_ == nullptr ||
        model_->vehicles() == state_dependent_transit_evaluators.size());
  const int size = model_->Size();
  transits_.resize(size, nullptr);
  fixed_transits_.resize(size, nullptr);
  slacks_.resize(size, nullptr);
  dependent_transits_.resize(size, nullptr);
  ComputeTransitClasses(transit_evaluators, &class_evaluators_,
                        &vehicle_to_class_);
  ComputeTransitClasses(cumul_dependent_transit_evaluators,
                        &cumul_dependent_class_evaluators_,
                        &vehicle_to_cumul_dependent_class_);
  if (base_dimension_ != nullptr) {
    ComputeTransitClasses(state_dependent_transit_evaluators,
                          &state_dependent_class_evaluators_,
                          &state_dependent_vehicle_to_class_);
  }

  InitializeTransitVariables(slack_max);
}

// TODO(user): Apply http://go/minimize-pointer-following.
void FillPathEvaluation(absl::Span<const int64_t> path,
                        const RoutingModel::TransitCallback2& evaluator,
                        std::vector<int64_t>* values) {
  const int num_nodes = path.size();
  values->resize(num_nodes - 1);
  for (int i = 0; i < num_nodes - 1; ++i) {
    (*values)[i] = evaluator(path[i], path[i + 1]);
  }
}

TypeRegulationsChecker::TypeRegulationsChecker(const RoutingModel& model)
    : model_(model), occurrences_of_type_(model.GetNumberOfVisitTypes()) {}

bool TypeRegulationsChecker::CheckVehicle(
    int vehicle, const std::function<int64_t(int64_t)>& next_accessor) {
  if (!HasRegulationsToCheck()) {
    return true;
  }

  InitializeCheck(vehicle, next_accessor);

  for (int pos = 0; pos < current_route_visits_.size(); pos++) {
    const int64_t current_visit = current_route_visits_[pos];
    const int type = model_.GetVisitType(current_visit);
    if (type < 0) {
      continue;
    }
    const VisitTypePolicy policy = model_.GetVisitTypePolicy(current_visit);

    DCHECK_LT(type, occurrences_of_type_.size());
    int& num_type_added = occurrences_of_type_[type].num_type_added_to_vehicle;
    int& num_type_removed =
        occurrences_of_type_[type].num_type_removed_from_vehicle;
    DCHECK_LE(num_type_removed, num_type_added);
    if (policy == RoutingModel::ADDED_TYPE_REMOVED_FROM_VEHICLE &&
        num_type_removed == num_type_added) {
      // The type is not actually being removed as all added types have already
      // been removed.
      continue;
    }

    if (!CheckTypeRegulations(type, policy, pos)) {
      return false;
    }
    // Update count of type based on the visit policy.
    if (policy == VisitTypePolicy::TYPE_ADDED_TO_VEHICLE ||
        policy == VisitTypePolicy::TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED) {
      num_type_added++;
    }
    if (policy == VisitTypePolicy::TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED ||
        policy == VisitTypePolicy::ADDED_TYPE_REMOVED_FROM_VEHICLE) {
      num_type_removed++;
    }
  }
  return FinalizeCheck();
}

void TypeRegulationsChecker::InitializeCheck(
    int vehicle, const std::function<int64_t(int64_t)>& next_accessor) {
  // Accumulates the count of types before the current node.
  // {0, 0, -1} does not compile on or-tools.
  std::fill(occurrences_of_type_.begin(), occurrences_of_type_.end(),
            TypeRegulationsChecker::TypePolicyOccurrence());

  // TODO(user): Optimize the filter to avoid scanning the route an extra
  // time when there are no TYPE_ON_VEHICLE_UP_TO_VISIT policies on the route,
  // by passing a boolean to CheckVehicle() passed to InitializeCheck().
  current_route_visits_.clear();
  for (int64_t current = model_.Start(vehicle); !model_.IsEnd(current);
       current = next_accessor(current)) {
    const int type = model_.GetVisitType(current);
    if (type >= 0 && model_.GetVisitTypePolicy(current) ==
                         VisitTypePolicy::TYPE_ON_VEHICLE_UP_TO_VISIT) {
      occurrences_of_type_[type].position_of_last_type_on_vehicle_up_to_visit =
          current_route_visits_.size();
    }
    current_route_visits_.push_back(current);
  }

  OnInitializeCheck();
}

bool TypeRegulationsChecker::TypeOccursOnRoute(int type) const {
  const TypePolicyOccurrence& occurrences = occurrences_of_type_[type];
  return occurrences.num_type_added_to_vehicle > 0 ||
         occurrences.position_of_last_type_on_vehicle_up_to_visit >= 0;
}

bool TypeRegulationsChecker::TypeCurrentlyOnRoute(int type, int pos) const {
  const TypePolicyOccurrence& occurrences = occurrences_of_type_[type];
  return occurrences.num_type_removed_from_vehicle <
             occurrences.num_type_added_to_vehicle ||
         occurrences.position_of_last_type_on_vehicle_up_to_visit >= pos;
}

TypeIncompatibilityChecker::TypeIncompatibilityChecker(
    const RoutingModel& model, bool check_hard_incompatibilities)
    : TypeRegulationsChecker(model),
      check_hard_incompatibilities_(check_hard_incompatibilities) {}

bool TypeIncompatibilityChecker::HasRegulationsToCheck() const {
  return model_.HasTemporalTypeIncompatibilities() ||
         (check_hard_incompatibilities_ &&
          model_.HasHardTypeIncompatibilities());
}

// TODO(user): Remove the check_hard_incompatibilities_ boolean and always
// check both incompatibilities to simplify the code?
// TODO(user): Improve algorithm by only checking a given type if necessary?
// - For temporal incompatibilities, only check if NonDeliveredType(count) == 1.
// - For hard incompatibilities, only if NonDeliveryType(type) == 1.
bool TypeIncompatibilityChecker::CheckTypeRegulations(int type,
                                                      VisitTypePolicy policy,
                                                      int pos) {
  if (policy == VisitTypePolicy::ADDED_TYPE_REMOVED_FROM_VEHICLE) {
    // NOTE: We don't need to check incompatibilities when the type is being
    // removed from the route.
    return true;
  }
  for (int incompatible_type :
       model_.GetTemporalTypeIncompatibilitiesOfType(type)) {
    if (TypeCurrentlyOnRoute(incompatible_type, pos)) {
      return false;
    }
  }
  if (check_hard_incompatibilities_) {
    for (int incompatible_type :
         model_.GetHardTypeIncompatibilitiesOfType(type)) {
      if (TypeOccursOnRoute(incompatible_type)) {
        return false;
      }
    }
  }
  return true;
}

bool TypeRequirementChecker::HasRegulationsToCheck() const {
  return model_.HasTemporalTypeRequirements() ||
         model_.HasSameVehicleTypeRequirements();
}

bool TypeRequirementChecker::CheckRequiredTypesCurrentlyOnRoute(
    absl::Span<const absl::flat_hash_set<int>> required_type_alternatives,
    int pos) {
  for (const absl::flat_hash_set<int>& requirement_alternatives :
       required_type_alternatives) {
    bool has_one_of_alternatives = false;
    for (int type_alternative : requirement_alternatives) {
      if (TypeCurrentlyOnRoute(type_alternative, pos)) {
        has_one_of_alternatives = true;
        break;
      }
    }
    if (!has_one_of_alternatives) {
      return false;
    }
  }
  return true;
}

bool TypeRequirementChecker::CheckTypeRegulations(int type,
                                                  VisitTypePolicy policy,
                                                  int pos) {
  if (policy == RoutingModel::TYPE_ADDED_TO_VEHICLE ||
      policy == RoutingModel::TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED) {
    if (!CheckRequiredTypesCurrentlyOnRoute(
            model_.GetRequiredTypeAlternativesWhenAddingType(type), pos)) {
      return false;
    }
  }
  if (policy != RoutingModel::TYPE_ADDED_TO_VEHICLE) {
    if (!CheckRequiredTypesCurrentlyOnRoute(
            model_.GetRequiredTypeAlternativesWhenRemovingType(type), pos)) {
      return false;
    }
  }
  if (policy != RoutingModel::ADDED_TYPE_REMOVED_FROM_VEHICLE &&
      !model_.GetSameVehicleRequiredTypeAlternativesOfType(type).empty()) {
    types_with_same_vehicle_requirements_on_route_.insert(type);
  }
  return true;
}

bool TypeRequirementChecker::FinalizeCheck() const {
  for (int type : types_with_same_vehicle_requirements_on_route_) {
    for (const absl::flat_hash_set<int>& requirement_alternatives :
         model_.GetSameVehicleRequiredTypeAlternativesOfType(type)) {
      bool has_one_of_alternatives = false;
      for (const int type_alternative : requirement_alternatives) {
        if (TypeOccursOnRoute(type_alternative)) {
          has_one_of_alternatives = true;
          break;
        }
      }
      if (!has_one_of_alternatives) {
        return false;
      }
    }
  }
  return true;
}

TypeRegulationsConstraint::TypeRegulationsConstraint(const RoutingModel& model)
    : Constraint(model.solver()),
      model_(model),
      incompatibility_checker_(model, /*check_hard_incompatibilities*/ true),
      requirement_checker_(model),
      vehicle_demons_(model.vehicles()) {}

void TypeRegulationsConstraint::PropagateNodeRegulations(int node) {
  DCHECK_LT(node, model_.Size());
  if (!model_.VehicleVar(node)->Bound() || !model_.NextVar(node)->Bound()) {
    // Vehicle var or Next var not bound.
    return;
  }
  const int vehicle = model_.VehicleVar(node)->Min();
  if (vehicle < 0) return;
  DCHECK(vehicle_demons_[vehicle] != nullptr);
  EnqueueDelayedDemon(vehicle_demons_[vehicle]);
}

void TypeRegulationsConstraint::CheckRegulationsOnVehicle(int vehicle) {
  const auto next_accessor = [this, vehicle](int64_t node) {
    if (model_.NextVar(node)->Bound()) {
      return model_.NextVar(node)->Value();
    }
    // Node not bound, skip to the end of the vehicle.
    return model_.End(vehicle);
  };
  if (!incompatibility_checker_.CheckVehicle(vehicle, next_accessor) ||
      !requirement_checker_.CheckVehicle(vehicle, next_accessor)) {
    model_.solver()->Fail();
  }
}

void TypeRegulationsConstraint::Post() {
  for (int vehicle = 0; vehicle < model_.vehicles(); vehicle++) {
    vehicle_demons_[vehicle] = MakeDelayedConstraintDemon1(
        solver(), this, &TypeRegulationsConstraint::CheckRegulationsOnVehicle,
        "CheckRegulationsOnVehicle", vehicle);
  }
  for (int node = 0; node < model_.Size(); node++) {
    Demon* node_demon = MakeConstraintDemon1(
        solver(), this, &TypeRegulationsConstraint::PropagateNodeRegulations,
        "PropagateNodeRegulations", node);
    model_.NextVar(node)->WhenBound(node_demon);
    model_.VehicleVar(node)->WhenBound(node_demon);
  }
}

void TypeRegulationsConstraint::InitialPropagate() {
  for (int vehicle = 0; vehicle < model_.vehicles(); vehicle++) {
    CheckRegulationsOnVehicle(vehicle);
  }
}

void RoutingDimension::CloseModel(bool use_light_propagation) {
  Solver* const solver = model_->solver();
  const auto capacity_lambda = [this](int64_t vehicle) {
    return vehicle >= 0 ? vehicle_capacities_[vehicle]
                        : std::numeric_limits<int64_t>::max();
  };
  for (int i = 0; i < capacity_vars_.size(); ++i) {
    IntVar* const vehicle_var = model_->VehicleVar(i);
    IntVar* const capacity_var = capacity_vars_[i];
    if (use_light_propagation) {
      solver->AddConstraint(solver->MakeLightElement(
          capacity_lambda, capacity_var, vehicle_var,
          [this]() { return model_->enable_deep_serialization_; }));
    } else {
      solver->AddConstraint(solver->MakeEquality(
          capacity_var,
          solver->MakeElement(capacity_lambda, vehicle_var)->Var()));
    }
  }
  for (int i = 0; i < fixed_transits_.size(); ++i) {
    IntVar* const next_var = model_->NextVar(i);
    IntVar* const fixed_transit = fixed_transits_[i];
    const auto transit_vehicle_evaluator = [this, i](int64_t to,
                                                     int64_t eval_index) {
      return eval_index >= 0 ? transit_evaluator(eval_index)(i, to) : 0;
    };
    if (use_light_propagation) {
      if (class_evaluators_.size() == 1) {
        const int class_evaluator_index = class_evaluators_[0];
        const auto& unary_callback =
            model_->UnaryTransitCallbackOrNull(class_evaluator_index);
        if (unary_callback == nullptr) {
          solver->AddConstraint(solver->MakeLightElement(
              [this, i](int64_t to) {
                return model_->TransitCallback(class_evaluators_[0])(i, to);
              },
              fixed_transit, next_var,
              [this]() { return model_->enable_deep_serialization_; }));
        } else {
          fixed_transit->SetValue(unary_callback(i));
        }
      } else {
        solver->AddConstraint(solver->MakeLightElement(
            transit_vehicle_evaluator, fixed_transit, next_var,
            model_->VehicleVar(i),
            [this]() { return model_->enable_deep_serialization_; }));
      }
    } else {
      if (class_evaluators_.size() == 1) {
        const int class_evaluator_index = class_evaluators_[0];
        const auto& unary_callback =
            model_->UnaryTransitCallbackOrNull(class_evaluator_index);
        if (unary_callback == nullptr) {
          solver->AddConstraint(solver->MakeEquality(
              fixed_transit, solver
                                 ->MakeElement(
                                     [this, i](int64_t to) {
                                       return model_->TransitCallback(
                                           class_evaluators_[0])(i, to);
                                     },
                                     model_->NextVar(i))
                                 ->Var()));
        } else {
          fixed_transit->SetValue(unary_callback(i));
        }
      } else {
        solver->AddConstraint(solver->MakeEquality(
            fixed_transit, solver
                               ->MakeElement(transit_vehicle_evaluator,
                                             next_var, model_->VehicleVar(i))
                               ->Var()));
      }
    }
  }
  if (HasBreakConstraints()) {
    solver->AddConstraint(
        MakeGlobalVehicleBreaksConstraint(model_->solver(), this));
    // If a vehicle has a duration-distance (max interbreak) constraint,
    // its breaks must be ordered.
    for (int v = 0; v < model_->vehicles(); ++v) {
      const std::vector<IntervalVar*>& breaks = GetBreakIntervalsOfVehicle(v);
      const int num_breaks = breaks.size();
      if (num_breaks <= 1 || GetBreakDistanceDurationOfVehicle(v).empty()) {
        continue;
      }
      for (int b = 1; b < num_breaks; ++b) {
        Constraint* precedence = solver->MakeIntervalVarRelation(
            breaks[b], Solver::STARTS_AFTER_END, breaks[b - 1]);
        solver->AddConstraint(precedence);
      }
    }
    // Add all cumuls to the finalizer.
    for (IntVar* cumul : cumuls_) {
      model_->AddVariableMinimizedByFinalizer(cumul);
    }
  }
}

int64_t RoutingDimension::GetTransitValue(int64_t from_index, int64_t to_index,
                                          int64_t vehicle) const {
  DCHECK(transit_evaluator(vehicle) != nullptr);
  return transit_evaluator(vehicle)(from_index, to_index);
}

bool RoutingDimension::AllTransitEvaluatorSignsAreUnknown() const {
  for (const int evaluator_index : class_evaluators_) {
    if (model()->transit_evaluator_sign_[evaluator_index] !=
        RoutingModel::kTransitEvaluatorSignUnknown) {
      return false;
    }
  }
  return true;
}

SortedDisjointIntervalList RoutingDimension::GetAllowedIntervalsInRange(
    int64_t index, int64_t min_value, int64_t max_value) const {
  SortedDisjointIntervalList allowed;
  const SortedDisjointIntervalList& forbidden = forbidden_intervals_[index];
  IntVar* const cumul_var = cumuls_[index];
  const int64_t min = std::max(min_value, cumul_var->Min());
  const int64_t max = std::min(max_value, cumul_var->Max());
  int64_t next_start = min;
  for (SortedDisjointIntervalList::Iterator interval =
           forbidden.FirstIntervalGreaterOrEqual(min);
       interval != forbidden.end(); ++interval) {
    if (next_start > max) break;
    if (next_start < interval->start) {
      allowed.InsertInterval(next_start, CapSub(interval->start, 1));
    }
    next_start = CapAdd(interval->end, 1);
  }
  if (next_start <= max) {
    allowed.InsertInterval(next_start, max);
  }
  return allowed;
}

void RoutingDimension::SetSpanUpperBoundForVehicle(int64_t upper_bound,
                                                   int vehicle) {
  CHECK_GE(vehicle, 0);
  CHECK_LT(vehicle, vehicle_span_upper_bounds_.size());
  CHECK_GE(upper_bound, 0);
  vehicle_span_upper_bounds_[vehicle] = upper_bound;
}

void RoutingDimension::SetSpanCostCoefficientForVehicle(int64_t coefficient,
                                                        int vehicle) {
  CHECK_GE(vehicle, 0);
  CHECK_LT(vehicle, vehicle_span_cost_coefficients_.size());
  CHECK_GE(coefficient, 0);
  vehicle_span_cost_coefficients_[vehicle] = coefficient;
}

void RoutingDimension::SetSpanCostCoefficientForAllVehicles(
    int64_t coefficient) {
  CHECK_GE(coefficient, 0);
  vehicle_span_cost_coefficients_.assign(model_->vehicles(), coefficient);
}

void RoutingDimension::SetGlobalSpanCostCoefficient(int64_t coefficient) {
  CHECK_GE(coefficient, 0);
  global_span_cost_coefficient_ = coefficient;
}

void RoutingDimension::SetSlackCostCoefficientForVehicle(int64_t coefficient,
                                                         int vehicle) {
  CHECK_GE(vehicle, 0);
  CHECK_LT(vehicle, vehicle_slack_cost_coefficients_.size());
  CHECK_GE(coefficient, 0);
  vehicle_slack_cost_coefficients_[vehicle] = coefficient;
}
void RoutingDimension::SetSlackCostCoefficientForAllVehicles(
    int64_t coefficient) {
  CHECK_GE(coefficient, 0);
  vehicle_slack_cost_coefficients_.assign(model_->vehicles(), coefficient);
}

void RoutingDimension::SetCumulVarPiecewiseLinearCost(
    int64_t index, const PiecewiseLinearFunction& cost) {
  if (!cost.IsNonDecreasing()) {
    LOG(WARNING) << "Only non-decreasing cost functions are supported.";
    return;
  }
  if (cost.Value(0) < 0) {
    LOG(WARNING) << "Only positive cost functions are supported.";
    return;
  }
  if (index >= cumul_var_piecewise_linear_cost_.size()) {
    cumul_var_piecewise_linear_cost_.resize(index + 1);
  }
  PiecewiseLinearCost& piecewise_linear_cost =
      cumul_var_piecewise_linear_cost_[index];
  piecewise_linear_cost.var = cumuls_[index];
  piecewise_linear_cost.cost = std::make_unique<PiecewiseLinearFunction>(cost);
}

bool RoutingDimension::HasCumulVarPiecewiseLinearCost(int64_t index) const {
  return (index < cumul_var_piecewise_linear_cost_.size() &&
          cumul_var_piecewise_linear_cost_[index].var != nullptr);
}

const PiecewiseLinearFunction* RoutingDimension::GetCumulVarPiecewiseLinearCost(
    int64_t index) const {
  if (index < cumul_var_piecewise_linear_cost_.size() &&
      cumul_var_piecewise_linear_cost_[index].var != nullptr) {
    return cumul_var_piecewise_linear_cost_[index].cost.get();
  }
  return nullptr;
}

namespace {
IntVar* BuildVarFromExprAndIndexActiveState(const RoutingModel* model,
                                            IntExpr* expr, int index) {
  Solver* const solver = model->solver();
  if (model->IsStart(index) || model->IsEnd(index)) {
    const int vehicle = model->VehicleIndex(index);
    DCHECK_GE(vehicle, 0);
    return solver->MakeProd(expr, model->VehicleRouteConsideredVar(vehicle))
        ->Var();
  }
  return solver->MakeProd(expr, model->ActiveVar(index))->Var();
}
}  // namespace

void RoutingDimension::SetupCumulVarPiecewiseLinearCosts(
    std::vector<IntVar*>* cost_elements) const {
  CHECK(cost_elements != nullptr);
  Solver* const solver = model_->solver();
  for (int i = 0; i < cumul_var_piecewise_linear_cost_.size(); ++i) {
    const PiecewiseLinearCost& piecewise_linear_cost =
        cumul_var_piecewise_linear_cost_[i];
    if (piecewise_linear_cost.var != nullptr) {
      IntExpr* const expr = solver->MakePiecewiseLinearExpr(
          piecewise_linear_cost.var, *piecewise_linear_cost.cost);
      IntVar* cost_var = BuildVarFromExprAndIndexActiveState(model_, expr, i);
      cost_elements->push_back(cost_var);
      // TODO(user): Check if it wouldn't be better to minimize
      // piecewise_linear_cost.var here.
      model_->AddWeightedVariableMinimizedByFinalizer(cost_var, 0);
    }
  }
}

void RoutingDimension::SetCumulVarSoftUpperBound(int64_t index,
                                                 int64_t upper_bound,
                                                 int64_t coefficient) {
  if (index >= cumul_var_soft_upper_bound_.size()) {
    cumul_var_soft_upper_bound_.resize(index + 1, {nullptr, 0, 0});
  }
  cumul_var_soft_upper_bound_[index] = {cumuls_[index], upper_bound,
                                        coefficient};
}

bool RoutingDimension::HasCumulVarSoftUpperBound(int64_t index) const {
  return (index < cumul_var_soft_upper_bound_.size() &&
          cumul_var_soft_upper_bound_[index].var != nullptr);
}

int64_t RoutingDimension::GetCumulVarSoftUpperBound(int64_t index) const {
  if (index < cumul_var_soft_upper_bound_.size() &&
      cumul_var_soft_upper_bound_[index].var != nullptr) {
    return cumul_var_soft_upper_bound_[index].bound;
  }
  return cumuls_[index]->Max();
}

int64_t RoutingDimension::GetCumulVarSoftUpperBoundCoefficient(
    int64_t index) const {
  if (index < cumul_var_soft_upper_bound_.size() &&
      cumul_var_soft_upper_bound_[index].var != nullptr) {
    return cumul_var_soft_upper_bound_[index].coefficient;
  }
  return 0;
}

void RoutingDimension::SetupCumulVarSoftUpperBoundCosts(
    std::vector<IntVar*>* cost_elements) const {
  CHECK(cost_elements != nullptr);
  Solver* const solver = model_->solver();
  for (int i = 0; i < cumul_var_soft_upper_bound_.size(); ++i) {
    const SoftBound& soft_bound = cumul_var_soft_upper_bound_[i];
    if (soft_bound.var != nullptr) {
      IntExpr* const expr = solver->MakeSemiContinuousExpr(
          solver->MakeSum(soft_bound.var, -soft_bound.bound), 0,
          soft_bound.coefficient);
      IntVar* cost_var = BuildVarFromExprAndIndexActiveState(model_, expr, i);
      cost_elements->push_back(cost_var);
      // NOTE: We minimize the cost here instead of minimizing the cumul
      // variable, to avoid setting the cumul to earlier than necessary.
      model_->AddWeightedVariableMinimizedByFinalizer(cost_var,
                                                      soft_bound.coefficient);
    }
  }
}

void RoutingDimension::SetCumulVarSoftLowerBound(int64_t index,
                                                 int64_t lower_bound,
                                                 int64_t coefficient) {
  if (index >= cumul_var_soft_lower_bound_.size()) {
    cumul_var_soft_lower_bound_.resize(index + 1, {nullptr, 0, 0});
  }
  cumul_var_soft_lower_bound_[index] = {cumuls_[index], lower_bound,
                                        coefficient};
}

bool RoutingDimension::HasCumulVarSoftLowerBound(int64_t index) const {
  return (index < cumul_var_soft_lower_bound_.size() &&
          cumul_var_soft_lower_bound_[index].var != nullptr);
}

int64_t RoutingDimension::GetCumulVarSoftLowerBound(int64_t index) const {
  if (index < cumul_var_soft_lower_bound_.size() &&
      cumul_var_soft_lower_bound_[index].var != nullptr) {
    return cumul_var_soft_lower_bound_[index].bound;
  }
  return cumuls_[index]->Min();
}

int64_t RoutingDimension::GetCumulVarSoftLowerBoundCoefficient(
    int64_t index) const {
  if (index < cumul_var_soft_lower_bound_.size() &&
      cumul_var_soft_lower_bound_[index].var != nullptr) {
    return cumul_var_soft_lower_bound_[index].coefficient;
  }
  return 0;
}

void RoutingDimension::SetupCumulVarSoftLowerBoundCosts(
    std::vector<IntVar*>* cost_elements) const {
  CHECK(cost_elements != nullptr);
  Solver* const solver = model_->solver();
  for (int i = 0; i < cumul_var_soft_lower_bound_.size(); ++i) {
    const SoftBound& soft_bound = cumul_var_soft_lower_bound_[i];
    if (soft_bound.var != nullptr) {
      IntExpr* const expr = solver->MakeSemiContinuousExpr(
          solver->MakeDifference(soft_bound.bound, soft_bound.var), 0,
          soft_bound.coefficient);
      IntVar* cost_var = BuildVarFromExprAndIndexActiveState(model_, expr, i);
      cost_elements->push_back(cost_var);
      // NOTE: We minimize the cost here instead of maximizing the cumul
      // variable, to avoid setting the cumul to later than necessary.
      model_->AddWeightedVariableMinimizedByFinalizer(cost_var,
                                                      soft_bound.coefficient);
    }
  }
}

void RoutingDimension::SetupGlobalSpanCost(
    std::vector<IntVar*>* cost_elements) const {
  CHECK(cost_elements != nullptr);
  Solver* const solver = model_->solver();
  if (global_span_cost_coefficient_ != 0) {
    std::vector<IntVar*> end_cumuls;
    for (int i = 0; i < model_->vehicles(); ++i) {
      end_cumuls.push_back(solver
                               ->MakeProd(model_->vehicle_route_considered_[i],
                                          cumuls_[model_->End(i)])
                               ->Var());
    }
    IntVar* const max_end_cumul = solver->MakeMax(end_cumuls)->Var();
    model_->AddWeightedVariableMinimizedByFinalizer(
        max_end_cumul, global_span_cost_coefficient_);
    std::vector<IntVar*> start_cumuls;
    for (int i = 0; i < model_->vehicles(); ++i) {
      IntVar* global_span_cost_start_cumul =
          solver->MakeIntVar(0, std::numeric_limits<int64_t>::max());
      solver->AddConstraint(solver->MakeIfThenElseCt(
          model_->vehicle_route_considered_[i], cumuls_[model_->Start(i)],
          max_end_cumul, global_span_cost_start_cumul));
      start_cumuls.push_back(global_span_cost_start_cumul);
    }
    IntVar* const min_start_cumul = solver->MakeMin(start_cumuls)->Var();
    model_->AddWeightedVariableMaximizedByFinalizer(
        min_start_cumul, global_span_cost_coefficient_);
    // If there is a single vehicle, model the cost as the sum of its transits
    // to avoid slow (infinite) propagation loops.
    // TODO(user): Avoid slow propagation in the path constraints.
    if (model_->vehicles() == 1) {
      for (int var_index = 0; var_index < model_->Size(); ++var_index) {
        model_->AddWeightedVariableMinimizedByFinalizer(
            slacks_[var_index], global_span_cost_coefficient_);
        cost_elements->push_back(
            solver
                ->MakeProd(
                    model_->vehicle_route_considered_[0],
                    solver->MakeProd(
                        solver->MakeProd(
                            solver->MakeSum(transits_[var_index],
                                            dependent_transits_[var_index]),
                            global_span_cost_coefficient_),
                        model_->ActiveVar(var_index)))
                ->Var());
      }
    } else {
      IntVar* const end_range =
          solver->MakeDifference(max_end_cumul, min_start_cumul)->Var();
      end_range->SetMin(0);
      cost_elements->push_back(
          solver->MakeProd(end_range, global_span_cost_coefficient_)->Var());
    }
  }
}

void RoutingDimension::SetBreakIntervalsOfVehicle(
    std::vector<IntervalVar*> breaks, int vehicle,
    std::vector<int64_t> node_visit_transits) {
  if (breaks.empty()) return;
  const int visit_evaluator = model()->RegisterTransitCallback(
      [node_visit_transits = std::move(node_visit_transits)](
          int64_t from, int64_t /*to*/) { return node_visit_transits[from]; },
      RoutingModel::kTransitEvaluatorSignPositiveOrZero);
  SetBreakIntervalsOfVehicle(std::move(breaks), vehicle, visit_evaluator, -1);
}

void RoutingDimension::SetBreakIntervalsOfVehicle(
    std::vector<IntervalVar*> breaks, int vehicle,
    std::vector<int64_t> node_visit_transits,
    std::function<int64_t(int64_t, int64_t)> delays) {
  if (breaks.empty()) return;
  const int visit_evaluator = model()->RegisterTransitCallback(
      [node_visit_transits = std::move(node_visit_transits)](
          int64_t from, int64_t /*to*/) { return node_visit_transits[from]; },
      RoutingModel::kTransitEvaluatorSignPositiveOrZero);
  const int delay_evaluator = model()->RegisterTransitCallback(
      std::move(delays), RoutingModel::kTransitEvaluatorSignPositiveOrZero);
  SetBreakIntervalsOfVehicle(std::move(breaks), vehicle, visit_evaluator,
                             delay_evaluator);
}

void RoutingDimension::SetBreakIntervalsOfVehicle(
    std::vector<IntervalVar*> breaks, int vehicle, int pre_travel_evaluator,
    int post_travel_evaluator) {
  DCHECK_LE(0, vehicle);
  DCHECK_LT(vehicle, model_->vehicles());
  if (breaks.empty()) return;
  if (!break_constraints_are_initialized_) InitializeBreaks();
  vehicle_break_intervals_[vehicle] = std::move(breaks);
  vehicle_pre_travel_evaluators_[vehicle] = pre_travel_evaluator;
  vehicle_post_travel_evaluators_[vehicle] = post_travel_evaluator;
  // Breaks intervals must be fixed by search.
  for (IntervalVar* const interval : vehicle_break_intervals_[vehicle]) {
    model_->AddIntervalToAssignment(interval);
    if (interval->MayBePerformed() && !interval->MustBePerformed()) {
      model_->AddVariableTargetToFinalizer(interval->PerformedExpr()->Var(), 0);
    }
    model_->AddVariableTargetToFinalizer(interval->SafeStartExpr(0)->Var(),
                                         std::numeric_limits<int64_t>::min());
    model_->AddVariableTargetToFinalizer(interval->SafeDurationExpr(0)->Var(),
                                         std::numeric_limits<int64_t>::min());
  }
  // When a vehicle has breaks, if its start and end are fixed,
  // then propagation keeps the cumuls min and max on its path feasible.
  model_->AddVariableTargetToFinalizer(CumulVar(model_->End(vehicle)),
                                       std::numeric_limits<int64_t>::min());
  model_->AddVariableTargetToFinalizer(CumulVar(model_->Start(vehicle)),
                                       std::numeric_limits<int64_t>::max());
}

void RoutingDimension::InitializeBreaks() {
  DCHECK(!break_constraints_are_initialized_);
  const int num_vehicles = model_->vehicles();
  vehicle_break_intervals_.resize(num_vehicles);
  vehicle_pre_travel_evaluators_.resize(num_vehicles, -1);
  vehicle_post_travel_evaluators_.resize(num_vehicles, -1);
  vehicle_break_distance_duration_.resize(num_vehicles);
  break_constraints_are_initialized_ = true;
}

bool RoutingDimension::HasBreakConstraints() const {
  return break_constraints_are_initialized_;
}

const std::vector<IntervalVar*>& RoutingDimension::GetBreakIntervalsOfVehicle(
    int vehicle) const {
  DCHECK_LE(0, vehicle);
  DCHECK_LT(vehicle, vehicle_break_intervals_.size());
  return vehicle_break_intervals_[vehicle];
}

int RoutingDimension::GetPreTravelEvaluatorOfVehicle(int vehicle) const {
  DCHECK_LE(0, vehicle);
  DCHECK_LT(vehicle, vehicle_pre_travel_evaluators_.size());
  return vehicle_pre_travel_evaluators_[vehicle];
}

int RoutingDimension::GetPostTravelEvaluatorOfVehicle(int vehicle) const {
  DCHECK_LE(0, vehicle);
  DCHECK_LT(vehicle, vehicle_post_travel_evaluators_.size());
  return vehicle_post_travel_evaluators_[vehicle];
}

void RoutingDimension::SetBreakDistanceDurationOfVehicle(int64_t distance,
                                                         int64_t duration,
                                                         int vehicle) {
  DCHECK_LE(0, vehicle);
  DCHECK_LT(vehicle, model_->vehicles());
  if (!break_constraints_are_initialized_) InitializeBreaks();
  vehicle_break_distance_duration_[vehicle].emplace_back(distance, duration);
  // When a vehicle has breaks, if its start and end are fixed,
  // then propagation keeps the cumuls min and max on its path feasible.
  model_->AddVariableTargetToFinalizer(CumulVar(model_->End(vehicle)),
                                       std::numeric_limits<int64_t>::min());
  model_->AddVariableTargetToFinalizer(CumulVar(model_->Start(vehicle)),
                                       std::numeric_limits<int64_t>::max());
}

const std::vector<std::pair<int64_t, int64_t>>&
RoutingDimension::GetBreakDistanceDurationOfVehicle(int vehicle) const {
  DCHECK_LE(0, vehicle);
  DCHECK_LT(vehicle, vehicle_break_distance_duration_.size());
  return vehicle_break_distance_duration_[vehicle];
}

void RoutingDimension::SetPickupToDeliveryLimitFunctionForPair(
    PickupToDeliveryLimitFunction limit_function, int pair_index) {
  CHECK_GE(pair_index, 0);
  if (pair_index >= pickup_to_delivery_limits_per_pair_index_.size()) {
    pickup_to_delivery_limits_per_pair_index_.resize(pair_index + 1);
  }
  pickup_to_delivery_limits_per_pair_index_[pair_index] =
      std::move(limit_function);
}

bool RoutingDimension::HasPickupToDeliveryLimits() const {
  return !pickup_to_delivery_limits_per_pair_index_.empty();
}

int64_t RoutingDimension::GetPickupToDeliveryLimitForPair(
    int pair_index, int pickup_alternative_index,
    int delivery_alternative_index) const {
  DCHECK_GE(pair_index, 0);

  if (pair_index >= pickup_to_delivery_limits_per_pair_index_.size()) {
    return std::numeric_limits<int64_t>::max();
  }
  const PickupToDeliveryLimitFunction& pickup_to_delivery_limit_function =
      pickup_to_delivery_limits_per_pair_index_[pair_index];
  if (!pickup_to_delivery_limit_function) {
    // No limit function set for this pair.
    return std::numeric_limits<int64_t>::max();
  }
  DCHECK_GE(pickup_alternative_index, 0);
  DCHECK_GE(delivery_alternative_index, 0);
  return pickup_to_delivery_limit_function(pickup_alternative_index,
                                           delivery_alternative_index);
}

void RoutingDimension::SetupSlackAndDependentTransitCosts() const {
  if (model_->vehicles() == 0) return;
  // Figure out whether all vehicles have the same span cost coefficient or not.
  if (absl::c_all_of(vehicle_span_cost_coefficients_,
                     [](int64_t c) { return c == 0; }) &&
      absl::c_all_of(vehicle_slack_cost_coefficients_,
                     [](int64_t c) { return c == 0; })) {
    return;  // No vehicle span/slack costs.
  }

  // Make sure that the vehicle's start cumul will be maximized in the end;
  // and that the vehicle's end cumul and the node's slacks will be minimized.
  // Note that we don't do that if there was no span cost (see the return
  // clause above), because in that case we want the dimension cumul to
  // remain unconstrained. Since transitions depend on base dimensions, we
  // have to make sure the slacks of base dimensions are taken care of.
  // Also, it makes more sense to make decisions from the root of the tree
  // towards to leaves, and hence the slacks are pushed in reverse order.
  std::vector<const RoutingDimension*> dimensions_with_relevant_slacks = {this};
  while (true) {
    const RoutingDimension* next =
        dimensions_with_relevant_slacks.back()->base_dimension_;
    if (next == nullptr || next == dimensions_with_relevant_slacks.back()) {
      break;
    }
    dimensions_with_relevant_slacks.push_back(next);
  }

  for (auto it = dimensions_with_relevant_slacks.rbegin();
       it != dimensions_with_relevant_slacks.rend(); ++it) {
    for (int i = 0; i < model_->vehicles(); ++i) {
      model_->AddVariableTargetToFinalizer((*it)->cumuls_[model_->End(i)],
                                           std::numeric_limits<int64_t>::min());
      model_->AddVariableTargetToFinalizer((*it)->cumuls_[model_->Start(i)],
                                           std::numeric_limits<int64_t>::max());
    }
    for (IntVar* const slack : (*it)->slacks_) {
      model_->AddVariableTargetToFinalizer(slack,
                                           std::numeric_limits<int64_t>::min());
    }
  }
}

}  // namespace operations_research::routing
