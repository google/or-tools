// Copyright 2010-2021 Google LLC
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

#include <math.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "ortools/base/int_type.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_lp_scheduling.h"
#include "ortools/constraint_solver/routing_parameters.pb.h"
#include "ortools/graph/ebert_graph.h"
#include "ortools/graph/min_cost_flow.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {

namespace {
// Compute set of disjunctions involved in a pickup and delivery pair.
template <typename Disjunctions>
void AddDisjunctionsFromNodes(const RoutingModel& model,
                              const std::vector<int64_t>& nodes,
                              Disjunctions* disjunctions) {
  for (int64_t node : nodes) {
    for (const auto disjunction : model.GetDisjunctionIndices(node)) {
      disjunctions->insert(disjunction);
    }
  }
}
}  // namespace

bool RoutingModel::IsMatchingModel() const {
  // TODO(user): Support overlapping disjunctions and disjunctions with
  // a cardinality > 1.
  absl::flat_hash_set<int> disjunction_nodes;
  for (DisjunctionIndex i(0); i < GetNumberOfDisjunctions(); ++i) {
    if (GetDisjunctionMaxCardinality(i) > 1) return false;
    for (int64_t node : GetDisjunctionNodeIndices(i)) {
      if (!disjunction_nodes.insert(node).second) return false;
    }
  }
  for (const auto& pd_pairs : GetPickupAndDeliveryPairs()) {
    absl::flat_hash_set<DisjunctionIndex> disjunctions;
    AddDisjunctionsFromNodes(*this, pd_pairs.first, &disjunctions);
    AddDisjunctionsFromNodes(*this, pd_pairs.second, &disjunctions);
    // Pairs involving more than 2 disjunctions are not supported.
    if (disjunctions.size() > 2) return false;
  }
  // Detect if a "unary" dimension prevents from having more than a single
  // non-start/end node (or a single pickup and delivery pair) on a route.
  // Binary dimensions are not considered because they would result in a
  // quadratic check.
  for (const RoutingDimension* const dimension : dimensions_) {
    // TODO(user): Support vehicle-dependent dimension callbacks.
    if (dimension->class_evaluators_.size() != 1) {
      continue;
    }
    const TransitCallback1& transit =
        UnaryTransitCallbackOrNull(dimension->class_evaluators_[0]);
    if (transit == nullptr) {
      continue;
    }
    int64_t max_vehicle_capacity = 0;
    for (int64_t vehicle_capacity : dimension->vehicle_capacities()) {
      max_vehicle_capacity = std::max(max_vehicle_capacity, vehicle_capacity);
    }
    std::vector<int64_t> transits(nexts_.size(),
                                  std::numeric_limits<int64_t>::max());
    for (int i = 0; i < nexts_.size(); ++i) {
      if (!IsStart(i) && !IsEnd(i)) {
        transits[i] = std::min(transits[i], transit(i));
      }
    }
    int64_t min_transit = std::numeric_limits<int64_t>::max();
    // Find the minimal accumulated value resulting from a pickup and delivery
    // pair.
    for (const auto& pd_pairs : GetPickupAndDeliveryPairs()) {
      const auto transit_cmp = [&transits](int i, int j) {
        return transits[i] < transits[j];
      };
      min_transit = std::min(
          min_transit,
          // Min transit from pickup.
          transits[*std::min_element(pd_pairs.first.begin(),
                                     pd_pairs.first.end(), transit_cmp)] +
              // Min transit from delivery.
              transits[*std::min_element(pd_pairs.second.begin(),
                                         pd_pairs.second.end(), transit_cmp)]);
    }
    // Find the minimal accumulated value resulting from a non-pickup/delivery
    // node.
    for (int i = 0; i < transits.size(); ++i) {
      if (GetPickupIndexPairs(i).empty() && GetDeliveryIndexPairs(i).empty()) {
        min_transit = std::min(min_transit, transits[i]);
      }
    }
    // If there cannot be more than one node or pickup and delivery, a matching
    // problem has been detected.
    if (CapProd(min_transit, 2) > max_vehicle_capacity) return true;
  }
  return false;
}

// Solve matching model using a min-cost flow. Here is the underlyihg flow:
//
//                     ---------- Source -------------
//                    | (1,0)                         | (N,0)
//                    V                               V
//                 (vehicles)                     unperformed
//                         | (1,cost)                 |
//                         V                          |
//                         (nodes/pickup/deliveries)  | (1,penalty)
//                                   | (1,0)          |
//                                   V                |
//                              disjunction <---------
//                                   | (1, 0)
//                                   V
//                                  Sink
//
// On arcs, (,) represents (capacity, cost).
// N: number of disjunctions
//

namespace {
struct FlowArc {
  int64_t tail;
  int64_t head;
  int64_t capacity;
  int64_t cost;
};
}  // namespace

bool RoutingModel::SolveMatchingModel(
    Assignment* assignment, const RoutingSearchParameters& parameters) {
  VLOG(2) << "Solving with flow";
  assignment->Clear();

  // Collect dimensions with costs.
  // TODO(user): If the costs are soft cumul upper (resp. lower) bounds only,
  // do not use the LP model.
  const std::vector<RoutingDimension*> dimensions =
      GetDimensionsWithSoftOrSpanCosts();
  std::vector<LocalDimensionCumulOptimizer> optimizers;
  optimizers.reserve(dimensions.size());
  for (RoutingDimension* dimension : dimensions) {
    optimizers.emplace_back(dimension,
                            parameters.continuous_scheduling_solver());
  }

  int num_flow_nodes = 0;
  std::vector<std::vector<int64_t>> disjunction_to_flow_nodes;
  std::vector<int64_t> disjunction_penalties;
  std::vector<bool> in_disjunction(Size(), false);
  // Create pickup and delivery pair flow nodes.
  // TODO(user): Check pair alternatives correspond exactly to at most two
  // disjunctions.
  absl::flat_hash_map<int, std::pair<int64_t, int64_t>> flow_to_pd;
  for (const auto& pd_pairs : GetPickupAndDeliveryPairs()) {
    disjunction_to_flow_nodes.push_back({});
    absl::flat_hash_set<DisjunctionIndex> disjunctions;
    AddDisjunctionsFromNodes(*this, pd_pairs.first, &disjunctions);
    AddDisjunctionsFromNodes(*this, pd_pairs.second, &disjunctions);
    for (int64_t pickup : pd_pairs.first) {
      in_disjunction[pickup] = true;
      for (int64_t delivery : pd_pairs.second) {
        in_disjunction[delivery] = true;
        flow_to_pd[num_flow_nodes] = {pickup, delivery};
        disjunction_to_flow_nodes.back().push_back(num_flow_nodes);
        num_flow_nodes++;
      }
    }
    DCHECK_LE(disjunctions.size(), 2);
    int64_t penalty = 0;
    if (disjunctions.size() < 2) {
      penalty = kNoPenalty;
    } else {
      for (DisjunctionIndex index : disjunctions) {
        const int64_t d_penalty = GetDisjunctionPenalty(index);
        if (d_penalty == kNoPenalty) {
          penalty = kNoPenalty;
          break;
        }
        penalty = CapAdd(penalty, d_penalty);
      }
    }
    disjunction_penalties.push_back(penalty);
  }
  // Create non-pickup and delivery flow nodes.
  absl::flat_hash_map<int, int64_t> flow_to_non_pd;
  for (int node = 0; node < Size(); ++node) {
    if (IsStart(node) || in_disjunction[node]) continue;
    const std::vector<DisjunctionIndex>& disjunctions =
        GetDisjunctionIndices(node);
    DCHECK_LE(disjunctions.size(), 1);
    disjunction_to_flow_nodes.push_back({});
    disjunction_penalties.push_back(
        disjunctions.empty() ? kNoPenalty
                             : GetDisjunctionPenalty(disjunctions.back()));
    if (disjunctions.empty()) {
      in_disjunction[node] = true;
      flow_to_non_pd[num_flow_nodes] = node;
      disjunction_to_flow_nodes.back().push_back(num_flow_nodes);
      num_flow_nodes++;
    } else {
      for (int n : GetDisjunctionNodeIndices(disjunctions.back())) {
        in_disjunction[n] = true;
        flow_to_non_pd[num_flow_nodes] = n;
        disjunction_to_flow_nodes.back().push_back(num_flow_nodes);
        num_flow_nodes++;
      }
    }
  }

  std::vector<FlowArc> arcs;

  // Build a flow node for each disjunction and corresponding arcs.
  // Each node exits to the sink through a node, for which the outgoing
  // capacity is one (only one of the nodes in the disjunction is performed).
  absl::flat_hash_map<int, int> flow_to_disjunction;
  for (int i = 0; i < disjunction_to_flow_nodes.size(); ++i) {
    const std::vector<int64_t>& flow_nodes = disjunction_to_flow_nodes[i];
    if (flow_nodes.size() == 1) {
      flow_to_disjunction[flow_nodes.back()] = i;
    } else {
      flow_to_disjunction[num_flow_nodes] = i;
      for (int64_t flow_node : flow_nodes) {
        arcs.push_back({flow_node, num_flow_nodes, 1, 0});
      }
      num_flow_nodes++;
    }
  }

  // Build arcs from each vehicle to each non-vehicle flow node; the cost of
  // each arc corresponds to:
  // start(vehicle) -> pickup -> delivery -> end(vehicle)
  // or
  // start(vehicle) -> node -> end(vehicle)
  std::vector<int> vehicle_to_flow;
  absl::flat_hash_map<int, int> flow_to_vehicle;
  for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
    const int64_t start = Start(vehicle);
    const int64_t end = End(vehicle);
    for (const std::vector<int64_t>& flow_nodes : disjunction_to_flow_nodes) {
      for (int64_t flow_node : flow_nodes) {
        std::pair<int64_t, int64_t> pd_pair;
        int64_t node = -1;
        int64_t cost = 0;
        bool add_arc = false;
        if (gtl::FindCopy(flow_to_pd, flow_node, &pd_pair)) {
          const int64_t pickup = pd_pair.first;
          const int64_t delivery = pd_pair.second;
          if (IsVehicleAllowedForIndex(vehicle, pickup) &&
              IsVehicleAllowedForIndex(vehicle, delivery)) {
            add_arc = true;
            cost =
                CapAdd(GetArcCostForVehicle(start, pickup, vehicle),
                       CapAdd(GetArcCostForVehicle(pickup, delivery, vehicle),
                              GetArcCostForVehicle(delivery, end, vehicle)));
            const absl::flat_hash_map<int64_t, int64_t> nexts = {
                {start, pickup}, {pickup, delivery}, {delivery, end}};
            for (LocalDimensionCumulOptimizer& optimizer : optimizers) {
              int64_t cumul_cost_value = 0;
              // TODO(user): if the result is RELAXED_OPTIMAL_ONLY, do a
              // second pass with an MP solver.
              if (optimizer.ComputeRouteCumulCostWithoutFixedTransits(
                      vehicle,
                      [&nexts](int64_t node) {
                        return nexts.find(node)->second;
                      },
                      &cumul_cost_value) !=
                  DimensionSchedulingStatus::INFEASIBLE) {
                cost = CapAdd(cost, cumul_cost_value);
              } else {
                add_arc = false;
                break;
              }
            }
          }
        } else if (gtl::FindCopy(flow_to_non_pd, flow_node, &node)) {
          if (IsVehicleAllowedForIndex(vehicle, node)) {
            add_arc = true;
            cost = CapAdd(GetArcCostForVehicle(start, node, vehicle),
                          GetArcCostForVehicle(node, end, vehicle));
            const absl::flat_hash_map<int64_t, int64_t> nexts = {{start, node},
                                                                 {node, end}};
            for (LocalDimensionCumulOptimizer& optimizer : optimizers) {
              int64_t cumul_cost_value = 0;
              // TODO(user): if the result is RELAXED_OPTIMAL_ONLY, do a
              // second pass with an MP solver.
              if (optimizer.ComputeRouteCumulCostWithoutFixedTransits(
                      vehicle,
                      [&nexts](int64_t node) {
                        return nexts.find(node)->second;
                      },
                      &cumul_cost_value) !=
                  DimensionSchedulingStatus::INFEASIBLE) {
                cost = CapAdd(cost, cumul_cost_value);
              } else {
                add_arc = false;
                break;
              }
            }
          }
        } else {
          DCHECK(false);
        }
        if (add_arc) {
          arcs.push_back({num_flow_nodes, flow_node, 1, cost});
        }
      }
    }
    flow_to_vehicle[num_flow_nodes] = vehicle;
    vehicle_to_flow.push_back(num_flow_nodes);
    num_flow_nodes++;
  }
  // Create flow source and sink nodes.
  const int source = num_flow_nodes + 1;
  const int sink = source + 1;
  // Source connected to vehicle nodes.
  for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
    arcs.push_back({source, vehicle_to_flow[vehicle], 1, 0});
  }
  // Handle unperformed nodes.
  // Create a node to catch unperformed nodes and connect it to source.
  const int unperformed = num_flow_nodes;
  const int64_t flow_supply = disjunction_to_flow_nodes.size();
  arcs.push_back({source, unperformed, flow_supply, 0});
  for (const auto& flow_disjunction_element : flow_to_disjunction) {
    const int flow_node = flow_disjunction_element.first;
    const int64_t penalty =
        disjunction_penalties[flow_disjunction_element.second];
    if (penalty != kNoPenalty) {
      arcs.push_back({unperformed, flow_node, 1, penalty});
    }
    // Connect non-vehicle flow nodes to sinks.
    arcs.push_back({flow_node, sink, 1, 0});
  }

  // Rescale costs for min-cost flow; assuming max cost resulting from the
  // push-relabel flow algorithm is max_arc_cost * (num_nodes+1) * (num_nodes+1)
  // (cost-scaling multiplies arc costs by num_nodes+1 and the flow itself can
  // accumulate num_nodes+1 such arcs (with capacity being 1 for costed arcs)).
  int64_t scale_factor = 1;
  const FlowArc& arc_with_max_cost = *std::max_element(
      arcs.begin(), arcs.end(),
      [](const FlowArc& a, const FlowArc& b) { return a.cost < b.cost; });
  // SimpleMinCostFlow adds a source and a sink node, so actual number of
  // nodes to consider is num_flow_nodes + 3.
  const int actual_flow_num_nodes = num_flow_nodes + 3;
  if (log(static_cast<double>(arc_with_max_cost.cost) + 1) +
          2 * log(actual_flow_num_nodes) >
      log(std::numeric_limits<int64_t>::max())) {
    scale_factor = CapProd(actual_flow_num_nodes, actual_flow_num_nodes);
  }

  SimpleMinCostFlow flow;
  // Add arcs to flow.
  for (const FlowArc& arc : arcs) {
    flow.AddArcWithCapacityAndUnitCost(arc.tail, arc.head, arc.capacity,
                                       arc.cost / scale_factor);
  }

  // Set flow supply (number of non-vehicle nodes or pairs).
  flow.SetNodeSupply(source, flow_supply);
  flow.SetNodeSupply(sink, -flow_supply);

  // TODO(user): Take time limit into account.
  if (flow.Solve() != SimpleMinCostFlow::OPTIMAL) {
    return false;
  }

  // Map the flow result to assignment, only setting next variables.
  std::vector<bool> used_vehicles(vehicles(), false);
  absl::flat_hash_set<int> used_nodes;
  for (int i = 0; i < flow.NumArcs(); ++i) {
    if (flow.Flow(i) > 0 && flow.Tail(i) != source && flow.Head(i) != sink) {
      std::vector<int> nodes;
      std::pair<int64_t, int64_t> pd_pair;
      int node = -1;
      int index = -1;
      if (gtl::FindCopy(flow_to_pd, flow.Head(i), &pd_pair)) {
        nodes.push_back(pd_pair.first);
        nodes.push_back(pd_pair.second);
      } else if (gtl::FindCopy(flow_to_non_pd, flow.Head(i), &node)) {
        nodes.push_back(node);
      } else if (gtl::FindCopy(flow_to_disjunction, flow.Head(i), &index)) {
        for (int64_t flow_node : disjunction_to_flow_nodes[index]) {
          if (gtl::FindCopy(flow_to_pd, flow_node, &pd_pair)) {
            nodes.push_back(pd_pair.first);
            nodes.push_back(pd_pair.second);
          } else if (gtl::FindCopy(flow_to_non_pd, flow_node, &node)) {
            nodes.push_back(node);
          }
        }
      }
      int vehicle = -1;
      if (flow.Tail(i) == unperformed) {
        // Head is unperformed.
        for (int node : nodes) {
          assignment->Add(NextVar(node))->SetValue(node);
          used_nodes.insert(node);
        }
      } else if (gtl::FindCopy(flow_to_vehicle, flow.Tail(i), &vehicle)) {
        // Head is performed on a vehicle.
        used_vehicles[vehicle] = true;
        int current = Start(vehicle);
        for (int node : nodes) {
          assignment->Add(NextVar(current))->SetValue(node);
          used_nodes.insert(node);
          current = node;
        }
        assignment->Add(NextVar(current))->SetValue(End(vehicle));
      }
    }
  }
  // Adding unused nodes.
  for (int node = 0; node < Size(); ++node) {
    if (!IsStart(node) && used_nodes.count(node) == 0) {
      assignment->Add(NextVar(node))->SetValue(node);
    }
  }
  // Adding unused vehicles.
  for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
    if (!used_vehicles[vehicle]) {
      assignment->Add(NextVar(Start(vehicle)))->SetValue(End(vehicle));
    }
  }
  return true;
}

}  // namespace operations_research
