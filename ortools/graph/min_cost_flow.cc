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

#include "ortools/graph/min_cost_flow.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ortools/base/mathutil.h"
#include "ortools/graph/generic_max_flow.h"
#include "ortools/graph_base/graph.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/stats.h"
#include "ortools/util/zvector.h"

// TODO(user): Remove these flags and expose the parameters in the API.
// New clients, please do not use these flags!
ABSL_FLAG(int64_t, min_cost_flow_alpha, 5,
          "Divide factor for epsilon at each refine step.");
ABSL_FLAG(bool, min_cost_flow_check_feasibility, true,
          "Check that the graph has enough capacity to send all supplies "
          "and serve all demands. Also check that the sum of supplies "
          "is equal to the sum of demands.");
ABSL_FLAG(bool, min_cost_flow_check_result, true,
          "Check that the result is valid.");

namespace operations_research {

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::GenericMinCostFlow(
    const Graph* graph)
    : graph_(graph),
      alpha_(absl::GetFlag(FLAGS_min_cost_flow_alpha)),
      stats_("MinCostFlow"),
      check_feasibility_(absl::GetFlag(FLAGS_min_cost_flow_check_feasibility)) {
  // This class assumes we have negative reverse arcs.
  static_assert(Graph::kHasNegativeReverseArcs);

  const NodeIndex max_num_nodes = graph_->node_capacity();
  if (max_num_nodes > 0) {
    first_admissible_arc_ = std::make_unique<ArcIndex[]>(max_num_nodes);
    std::fill(first_admissible_arc_.get(),
              first_admissible_arc_.get() + max_num_nodes, Graph::kNilArc);
    node_potential_ = std::make_unique<CostValue[]>(max_num_nodes);
    node_excess_ = std::make_unique<FlowQuantity[]>(max_num_nodes);
    initial_node_excess_ = std::make_unique<FlowQuantity[]>(max_num_nodes);
  }
  const ArcIndex max_num_arcs = graph_->arc_capacity();
  if (max_num_arcs > 0) {
    residual_arc_capacity_ =
        ZVector<ArcFlowType>(-max_num_arcs, max_num_arcs - 1);
    residual_arc_capacity_.SetAll(0);
    scaled_arc_unit_cost_ =
        ZVector<ArcScaledCostType>(-max_num_arcs, max_num_arcs - 1);
    scaled_arc_unit_cost_.SetAll(0);
  }
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
void GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::SetNodeSupply(
    NodeIndex node, FlowQuantity supply) {
  DCHECK(graph_->IsNodeValid(node));
  node_excess_[node] = supply;
  initial_node_excess_[node] = supply;
  status_ = NOT_SOLVED;
  feasibility_checked_ = false;
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
void GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::SetArcUnitCost(
    ArcIndex arc, ArcScaledCostType unit_cost) {
  DCHECK(IsArcDirect(arc));
  scaled_arc_unit_cost_.Set(arc, unit_cost);
  scaled_arc_unit_cost_.Set(Opposite(arc), -scaled_arc_unit_cost_[arc]);
  status_ = NOT_SOLVED;
  feasibility_checked_ = false;
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
void GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::SetArcCapacity(
    ArcIndex arc, ArcFlowType new_capacity) {
  DCHECK_LE(0, new_capacity);
  DCHECK(IsArcDirect(arc));
  const FlowQuantity free_capacity = residual_arc_capacity_[arc];
  const FlowQuantity capacity_delta = new_capacity - Capacity(arc);
  if (capacity_delta == 0) {
    return;  // Nothing to do.
  }
  status_ = NOT_SOLVED;
  feasibility_checked_ = false;
  const FlowQuantity new_availability = free_capacity + capacity_delta;
  if (new_availability >= 0) {
    // The above condition is true when one of two following holds:
    // 1/ (capacity_delta > 0), meaning we are increasing the capacity
    // 2/ (capacity_delta < 0 && free_capacity + capacity_delta >= 0)
    //    meaning we are reducing the capacity, but that the capacity
    //    reduction is not larger than the free capacity.
    DCHECK((capacity_delta > 0) ||
           (capacity_delta < 0 && new_availability >= 0));
    residual_arc_capacity_.Set(arc, new_availability);
    DCHECK_LE(0, residual_arc_capacity_[arc]);
  } else {
    // We have to reduce the flow on the arc, and update the excesses
    // accordingly.
    const FlowQuantity flow = residual_arc_capacity_[Opposite(arc)];
    const FlowQuantity flow_excess = flow - new_capacity;
    residual_arc_capacity_.Set(arc, 0);
    residual_arc_capacity_.Set(Opposite(arc), new_capacity);
    node_excess_[Tail(arc)] += flow_excess;
    node_excess_[Head(arc)] -= flow_excess;
    DCHECK_LE(0, residual_arc_capacity_[arc]);
    DCHECK_LE(0, residual_arc_capacity_[Opposite(arc)]);
  }
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
bool GenericMinCostFlow<Graph, ArcFlowType,
                        ArcScaledCostType>::CheckInputConsistency() {
  const FlowQuantity kMaxFlow = std::numeric_limits<FlowQuantity>::max();

  // First lets make sure supply == demand and the total supply/demand do not
  // overflow.
  FlowQuantity sum_of_supplies = 0;
  FlowQuantity sum_of_demands = 0;
  for (NodeIndex node = 0; node < graph_->num_nodes(); ++node) {
    const FlowQuantity excess = node_excess_[node];
    if (excess > 0) {
      sum_of_supplies = CapAdd(sum_of_supplies, excess);
      if (sum_of_supplies >= kMaxFlow) {
        status_ = BAD_CAPACITY_RANGE;
        LOG(ERROR) << "Input consistency error: sum of supplies overflow";
        return false;
      }
    } else if (excess < 0) {
      sum_of_demands = CapAdd(sum_of_demands, -excess);
      if (sum_of_demands >= kMaxFlow) {
        status_ = BAD_CAPACITY_RANGE;
        LOG(ERROR) << "Input consistency error: sum of demands overflow";
        return false;
      }
    }
  }
  if (sum_of_supplies != sum_of_demands) {
    status_ = UNBALANCED;
    LOG(ERROR) << "Input consistency error: unbalanced problem";
    return false;
  }

  std::vector<FlowQuantity> max_node_excess(
      node_excess_.get(), node_excess_.get() + graph_->num_nodes());
  std::vector<FlowQuantity> min_node_excess = max_node_excess;
  for (ArcIndex arc = 0; arc < graph_->num_arcs(); ++arc) {
    const FlowQuantity capacity = residual_arc_capacity_[arc];
    CHECK_GE(capacity, 0);
    if (capacity == 0) continue;
    const int tail = graph_->Tail(arc);
    const int head = graph_->Head(arc);
    min_node_excess[tail] = CapSub(min_node_excess[tail], capacity);
    max_node_excess[head] = CapAdd(max_node_excess[head], capacity);
  }

  const int num_nodes = graph_->num_nodes();
  for (NodeIndex node = 0; node < num_nodes; ++node) {
    if (max_node_excess[node] >= kMaxFlow ||
        min_node_excess[node] <= -kMaxFlow) {
      // Try to fix it.
      // Some user just use arc with infinite capacity out of the source/sink.
      // This is why we use CappedCapacity() for a name.
      if (max_node_excess[node] < std::numeric_limits<ArcFlowType>::max()) {
        // There is no point having an outgoing arc with more than this.
        const ArcFlowType upper_bound =
            std::max<ArcFlowType>(0, max_node_excess[node]);

        // Adjust and recompute min_node_excess[node].
        min_node_excess[node] = node_excess_[node];
        for (const ArcIndex arc : graph_->OutgoingArcs(node)) {
          residual_arc_capacity_[arc] =
              std::min(residual_arc_capacity_[arc], upper_bound);
          min_node_excess[node] =
              CapSub(min_node_excess[node], residual_arc_capacity_[arc]);
        }
        if (min_node_excess[node] > -kMaxFlow) continue;
      }
      if (min_node_excess[node] > -std::numeric_limits<ArcFlowType>::max()) {
        // There is no point having an incoming arc with more than this.
        const ArcFlowType upper_bound =
            std::max<ArcFlowType>(0, -min_node_excess[node]);

        // Adjust and recompute max_node_excess[node].
        max_node_excess[node] = node_excess_[node];
        for (const ArcIndex arc : graph_->IncomingArcs(node)) {
          residual_arc_capacity_[arc] =
              std::min(residual_arc_capacity_[arc], upper_bound);
          max_node_excess[node] =
              CapAdd(max_node_excess[node], residual_arc_capacity_[arc]);
        }
        if (max_node_excess[node] < kMaxFlow) continue;
      }

      status_ = BAD_CAPACITY_RANGE;
      LOG(ERROR) << "Maximum in or out flow of node + excess " << node
                 << " overflow the FlowQuantity type (int64_t).";
      return false;
    }
  }
  return true;
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
bool GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::CheckResult()
    const {
  for (NodeIndex node = 0; node < graph_->num_nodes(); ++node) {
    if (node_excess_[node] != 0) {
      LOG(DFATAL) << "node_excess_[" << node << "] != 0";
      return false;
    }
    for (OutgoingOrOppositeIncomingArcIterator it(*graph_, node); it.Ok();
         it.Next()) {
      const ArcIndex arc = it.Index();
      bool ok = true;
      if (residual_arc_capacity_[arc] < 0) {
        LOG(DFATAL) << "residual_arc_capacity_[" << arc << "] < 0";
        ok = false;
      }
      if (residual_arc_capacity_[arc] > 0 && ReducedCost(arc) < -epsilon_) {
        LOG(DFATAL) << "residual_arc_capacity_[" << arc
                    << "] > 0 && ReducedCost(" << arc << ") < " << -epsilon_
                    << ". (epsilon_ = " << epsilon_ << ").";
        ok = false;
      }
      if (!ok) {
        LOG(DFATAL) << DebugString("CheckResult ", arc);
        return false;
      }
    }
  }
  return true;
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
bool GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::
    CheckRelabelPrecondition(NodeIndex node) const {
  // Note that the classical Relabel precondition assumes IsActive(node), i.e.,
  // the node_excess_[node] > 0. However, to implement the Push Look-Ahead
  // heuristic, we can relax this condition as explained in the section 4.3 of
  // the article "An Efficient Implementation of a Scaling Minimum-Cost Flow
  // Algorithm", A.V. Goldberg, Journal of Algorithms 22(1), January 1997, pp.
  // 1-29.
  DCHECK_GE(node_excess_[node], 0);
  for (OutgoingOrOppositeIncomingArcIterator it(*graph_, node); it.Ok();
       it.Next()) {
    const ArcIndex arc = it.Index();
    DCHECK(!IsAdmissible(arc)) << DebugString("CheckRelabelPrecondition:", arc);
  }
  return true;
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
std::string
GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::DebugString(
    absl::string_view context, ArcIndex arc) const {
  const NodeIndex tail = Tail(arc);
  const NodeIndex head = Head(arc);
  // Reduced cost is computed directly without calling ReducedCost to avoid
  // recursive calls between ReducedCost and DebugString in case a DCHECK in
  // ReducedCost fails.
  const CostValue reduced_cost = scaled_arc_unit_cost_[arc] +
                                 node_potential_[tail] - node_potential_[head];
  return absl::StrFormat(
      "%s Arc %d, from %d to %d, "
      "Capacity = %d, Residual capacity = %d, "
      "Flow = residual capacity for reverse arc = %d, "
      "Height(tail) = %d, Height(head) = %d, "
      "Excess(tail) = %d, Excess(head) = %d, "
      "Cost = %d, Reduced cost = %d, ",
      context, arc, tail, head, Capacity(arc),
      static_cast<FlowQuantity>(residual_arc_capacity_[arc]), Flow(arc),
      node_potential_[tail], node_potential_[head], node_excess_[tail],
      node_excess_[head], static_cast<CostValue>(scaled_arc_unit_cost_[arc]),
      reduced_cost);
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
bool GenericMinCostFlow<Graph, ArcFlowType,
                        ArcScaledCostType>::CheckFeasibility() {
  SCOPED_TIME_STAT(&stats_);
  // Create a new graph, which is a copy of graph_, with the following
  // modifications:
  // Two nodes are added: a source and a sink.
  // The source is linked to each supply node (whose supply > 0) by an arc whose
  // capacity is equal to the supply at the supply node.
  // The sink is linked to each demand node (whose supply < 0) by an arc whose
  // capacity is the demand (-supply) at the demand node.
  // There are no supplies or demands or costs in the graph, as we will run
  // max-flow.
  // TODO(user): make it possible to share a graph by MaxFlow and MinCostFlow.
  feasibility_checked_ = false;
  ArcIndex num_extra_arcs = 0;
  for (NodeIndex node = 0; node < graph_->num_nodes(); ++node) {
    if (initial_node_excess_[node] != 0) {
      ++num_extra_arcs;
    }
  }
  const NodeIndex num_nodes_in_max_flow = graph_->num_nodes() + 2;
  const ArcIndex num_arcs_in_max_flow = graph_->num_arcs() + num_extra_arcs;
  const NodeIndex source = num_nodes_in_max_flow - 2;
  const NodeIndex sink = num_nodes_in_max_flow - 1;
  using CheckerGraph = ::util::ReverseArcListGraph<>;
  CheckerGraph checker_graph(num_nodes_in_max_flow, num_arcs_in_max_flow);
  GenericMaxFlow<CheckerGraph> checker(&checker_graph, source, sink);
  // Copy graph_ to checker_graph.
  for (ArcIndex arc = 0; arc < graph_->num_arcs(); ++arc) {
    const ArcIndex new_arc =
        checker_graph.AddArc(graph_->Tail(arc), graph_->Head(arc));
    DCHECK_EQ(arc, new_arc);
    checker.SetArcCapacity(new_arc, Capacity(arc));
  }
  FlowQuantity total_demand = 0;
  FlowQuantity total_supply = 0;
  // Create the source-to-supply node arcs and the demand-node-to-sink arcs.
  for (NodeIndex node = 0; node < graph_->num_nodes(); ++node) {
    const FlowQuantity supply = initial_node_excess_[node];
    if (supply > 0) {
      const ArcIndex new_arc = checker_graph.AddArc(source, node);
      checker.SetArcCapacity(new_arc, supply);
      total_supply += supply;
    } else if (supply < 0) {
      const ArcIndex new_arc = checker_graph.AddArc(node, sink);
      checker.SetArcCapacity(new_arc, -supply);
      total_demand -= supply;
    }
  }
  if (total_supply != total_demand) {
    LOG(DFATAL) << "total_supply(" << total_supply << ") != total_demand("
                << total_demand << ").";
    return false;
  }
  if (!checker.Solve()) {
    LOG(DFATAL) << "Max flow could not be computed.";
    return false;
  }
  const FlowQuantity optimal_max_flow = checker.GetOptimalFlow();
  feasibility_checked_ = true;
  return optimal_max_flow == total_supply;
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
auto GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::Flow(
    ArcIndex arc) const -> FlowQuantity {
  if (IsArcDirect(arc)) {
    return residual_arc_capacity_[Opposite(arc)];
  } else {
    return -residual_arc_capacity_[arc];
  }
}

// We use the equations given in the comment of residual_arc_capacity_.
template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
auto GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::Capacity(
    ArcIndex arc) const -> FlowQuantity {
  if (IsArcDirect(arc)) {
    return residual_arc_capacity_[arc] + residual_arc_capacity_[Opposite(arc)];
  } else {
    return 0;
  }
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
auto GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::UnitCost(
    ArcIndex arc) const -> CostValue {
  DCHECK(IsArcValid(arc));
  DCHECK_EQ(uint64_t{1}, cost_scaling_factor_);
  return scaled_arc_unit_cost_[arc];
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
auto GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::Supply(
    NodeIndex node) const -> FlowQuantity {
  DCHECK(graph_->IsNodeValid(node));
  return node_excess_[node];
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
bool GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::IsAdmissible(
    ArcIndex arc) const {
  return FastIsAdmissible(arc, node_potential_[Tail(arc)]);
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
bool GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::
    FastIsAdmissible(ArcIndex arc, CostValue tail_potential) const {
  DCHECK_EQ(node_potential_[Tail(arc)], tail_potential);
  return residual_arc_capacity_[arc] > 0 &&
         FastReducedCost(arc, tail_potential) < 0;
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
bool GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::IsActive(
    NodeIndex node) const {
  return node_excess_[node] > 0;
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
auto GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::ReducedCost(
    ArcIndex arc) const -> CostValue {
  return FastReducedCost(arc, node_potential_[Tail(arc)]);
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
auto GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::FastReducedCost(
    ArcIndex arc, CostValue tail_potential) const -> CostValue {
  DCHECK_EQ(node_potential_[Tail(arc)], tail_potential);
  DCHECK(graph_->IsNodeValid(Tail(arc)));
  DCHECK(graph_->IsNodeValid(Head(arc)));
  DCHECK_LE(node_potential_[Tail(arc)], 0) << DebugString("ReducedCost:", arc);
  DCHECK_LE(node_potential_[Head(arc)], 0) << DebugString("ReducedCost:", arc);

  // Note that there should be no overflow independently of the order of
  // operations if potentials are in [overflow_threshold_, 0].
  return scaled_arc_unit_cost_[arc] + tail_potential -
         node_potential_[Head(arc)];
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
auto GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::
    GetFirstOutgoingOrOppositeIncomingArc(NodeIndex node) const -> ArcIndex {
  OutgoingOrOppositeIncomingArcIterator arc_it(*graph_, node);
  return arc_it.Index();
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
bool GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::Solve() {
  if (!CheckInputConsistency()) {
    return false;
  }
  if (check_feasibility_ && !CheckFeasibility()) {
    status_ = INFEASIBLE;
    return false;
  }

  status_ = NOT_SOLVED;
  std::fill(node_potential_.get(),
            node_potential_.get() + graph_->node_capacity(), 0);

  ResetFirstAdmissibleArcs();
  if (!ScaleCosts()) return false;
  if (!Optimize()) return false;
  DCHECK_EQ(status_, NOT_SOLVED);
  status_ = OPTIMAL;

  if (absl::GetFlag(FLAGS_min_cost_flow_check_result) && !CheckResult()) {
    status_ = BAD_RESULT;
    UnscaleCosts();
    return false;
  }
  UnscaleCosts();

  IF_STATS_ENABLED(VLOG(1) << stats_.StatString());
  return true;
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
auto GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::GetOptimalCost()
    -> CostValue {
  if (status_ != OPTIMAL) {
    return 0;
  }

  // The total cost of the flow.
  // We cap the result if its overflow.
  CostValue total_flow_cost = 0;
  const CostValue kMaxCost = std::numeric_limits<CostValue>::max();
  const CostValue kMinCost = std::numeric_limits<CostValue>::min();
  for (ArcIndex arc = 0; arc < graph_->num_arcs(); ++arc) {
    const CostValue flow_on_arc = residual_arc_capacity_[Opposite(arc)];
    const CostValue flow_cost =
        CapProd(scaled_arc_unit_cost_[arc], flow_on_arc);
    if (flow_cost == kMaxCost || flow_cost == kMinCost) return kMaxCost;
    total_flow_cost = CapAdd(flow_cost, total_flow_cost);
    if (total_flow_cost == kMaxCost || total_flow_cost == kMinCost) {
      return kMaxCost;
    }
  }
  return total_flow_cost;
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
void GenericMinCostFlow<Graph, ArcFlowType,
                        ArcScaledCostType>::ResetFirstAdmissibleArcs() {
  for (NodeIndex node = 0; node < graph_->num_nodes(); ++node) {
    first_admissible_arc_[node] = GetFirstOutgoingOrOppositeIncomingArc(node);
  }
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
bool GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::ScaleCosts() {
  SCOPED_TIME_STAT(&stats_);
  cost_scaling_factor_ = scale_prices_ ? graph_->num_nodes() + 1 : 1;
  epsilon_ = 1LL;
  VLOG(3) << "Number of nodes in the graph = " << graph_->num_nodes();
  VLOG(3) << "Number of arcs in the graph = " << graph_->num_arcs();
  const CostValue threshold =
      std::numeric_limits<CostValue>::max() / (2 * cost_scaling_factor_);
  for (ArcIndex arc = 0; arc < graph_->num_arcs(); ++arc) {
    if (scaled_arc_unit_cost_[arc] > threshold ||
        scaled_arc_unit_cost_[arc] < -threshold) {
      status_ = BAD_COST_RANGE;
      return false;
    }
    const CostValue cost = scaled_arc_unit_cost_[arc] * cost_scaling_factor_;
    scaled_arc_unit_cost_.Set(arc, cost);
    scaled_arc_unit_cost_.Set(Opposite(arc), -cost);
    epsilon_ = std::max(epsilon_, MathUtil::Abs(cost));
  }

  // Since epsilon_ is currently the largest scaled cost, as long as our
  // node potentials stay above this threshold, we can correctly compute
  // potential - epsilon or cost + potential_diff.
  overflow_threshold_ = std::numeric_limits<CostValue>::min() + epsilon_;

  VLOG(3) << "Initial epsilon = " << epsilon_;
  VLOG(3) << "Cost scaling factor = " << cost_scaling_factor_;
  return true;
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
void GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::UnscaleCosts() {
  SCOPED_TIME_STAT(&stats_);
  for (ArcIndex arc = 0; arc < graph_->num_arcs(); ++arc) {
    const CostValue cost = scaled_arc_unit_cost_[arc] / cost_scaling_factor_;
    scaled_arc_unit_cost_.Set(arc, cost);
    scaled_arc_unit_cost_.Set(Opposite(arc), -cost);
  }
  cost_scaling_factor_ = 1;
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
bool GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::Optimize() {
  const CostValue kEpsilonMin = 1LL;
  num_relabels_since_last_price_update_ = 0;
  do {
    // Avoid epsilon_ == 0.
    epsilon_ = std::max(epsilon_ / alpha_, kEpsilonMin);
    VLOG(3) << "Epsilon changed to: " << epsilon_;
    if (!Refine()) return false;
  } while (epsilon_ != 1LL);
  return true;
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
void GenericMinCostFlow<Graph, ArcFlowType,
                        ArcScaledCostType>::SaturateAdmissibleArcs() {
  SCOPED_TIME_STAT(&stats_);
  for (NodeIndex node = 0; node < graph_->num_nodes(); ++node) {
    const CostValue tail_potential = node_potential_[node];
    for (OutgoingOrOppositeIncomingArcIterator it(*graph_, node,
                                                  first_admissible_arc_[node]);
         it.Ok(); it.Next()) {
      const ArcIndex arc = it.Index();
      if (FastIsAdmissible(arc, tail_potential)) {
        FastPushFlow(residual_arc_capacity_[arc], arc, node);
      }
    }

    // We just saturated all the admissible arcs, so there are no arcs with a
    // positive residual capacity that are incident to the current node.
    // Moreover, during the course of the algorithm, if the residual capacity of
    // such an arc becomes positive again, then the arc is still not admissible
    // until we relabel the node (because the reverse arc was admissible for
    // this to happen). In conclusion, the optimization below is correct.
    first_admissible_arc_[node] = Graph::kNilArc;
  }
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
void GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::PushFlow(
    FlowQuantity flow, ArcIndex arc) {
  SCOPED_TIME_STAT(&stats_);
  FastPushFlow(flow, arc, Tail(arc));
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
void GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::FastPushFlow(
    FlowQuantity flow, ArcIndex arc, NodeIndex tail) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK_EQ(Tail(arc), tail);
  DCHECK_GT(residual_arc_capacity_[arc], 0);
  DCHECK_LE(flow, residual_arc_capacity_[arc]);
  // Reduce the residual capacity on the arc by flow.
  residual_arc_capacity_.Set(arc, residual_arc_capacity_[arc] - flow);
  // Increase the residual capacity on the opposite arc by flow.
  const ArcIndex opposite = Opposite(arc);
  residual_arc_capacity_.Set(opposite, residual_arc_capacity_[opposite] + flow);
  // Update the excesses at the tail and head of the arc.
  node_excess_[tail] -= flow;
  node_excess_[Head(arc)] += flow;
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
void GenericMinCostFlow<Graph, ArcFlowType,
                        ArcScaledCostType>::InitializeActiveNodeStack() {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(active_nodes_.empty());
  for (NodeIndex node = 0; node < graph_->num_nodes(); ++node) {
    if (IsActive(node)) {
      active_nodes_.push(node);
    }
  }
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
bool GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::UpdatePrices() {
  SCOPED_TIME_STAT(&stats_);

  // The algorithm works as follows. Start with a set of nodes S containing all
  // the nodes with negative excess. Expand the set along reverse admissible
  // arcs. If at the end, the complement of S contains at least one node with
  // positive excess, relabel all the nodes in the complement of S by
  // subtracting epsilon from their current potential. See the paper cited in
  // the .h file.
  //
  // After this relabeling is done, the heuristic is reapplied by extending S as
  // much as possible, relabeling the complement of S, and so on until there is
  // no node with positive excess that is not in S. Note that this is not
  // described in the paper.
  //
  // Note(user): The triggering mechanism of this UpdatePrices() is really
  // important; if it is not done properly it may degrade performance!

  // This represents the set S.
  const NodeIndex num_nodes = graph_->num_nodes();
  std::vector<NodeIndex> bfs_queue;
  std::vector<bool> node_in_queue(num_nodes, false);

  // This is used to update the potential of the nodes not in S.
  const CostValue kMinCostValue = std::numeric_limits<CostValue>::min();
  std::vector<CostValue> min_non_admissible_potential(num_nodes, kMinCostValue);
  std::vector<NodeIndex> nodes_to_process;

  // Sum of the positive excesses out of S, used for early exit.
  FlowQuantity remaining_excess = 0;

  // First consider the nodes which have a negative excess.
  for (NodeIndex node = 0; node < num_nodes; ++node) {
    if (node_excess_[node] < 0) {
      bfs_queue.push_back(node);
      node_in_queue[node] = true;

      // This uses the fact that the sum of excesses is always 0.
      remaining_excess -= node_excess_[node];
    }
  }

  // All the nodes not yet in the bfs_queue will have their potential changed by
  // +potential_delta (which becomes more and more negative at each pass). This
  // update is applied when a node is pushed into the queue and at the end of
  // the function for the nodes that are still unprocessed.
  CostValue potential_delta = 0;

  int queue_index = 0;
  while (remaining_excess > 0) {
    // Reverse BFS that expands S as much as possible in the reverse admissible
    // graph. Once S cannot be expanded anymore, perform a relabeling on the
    // nodes not in S but that can reach it in one arc and try to expand S
    // again.
    for (; queue_index < bfs_queue.size(); ++queue_index) {
      DCHECK_GE(num_nodes, bfs_queue.size());
      const NodeIndex node = bfs_queue[queue_index];
      for (OutgoingOrOppositeIncomingArcIterator it(*graph_, node); it.Ok();
           it.Next()) {
        const NodeIndex head = Head(it.Index());
        if (node_in_queue[head]) continue;
        const ArcIndex opposite_arc = Opposite(it.Index());
        if (residual_arc_capacity_[opposite_arc] > 0) {
          node_potential_[head] += potential_delta;
          if (node_potential_[head] < overflow_threshold_) {
            status_ = BAD_COST_RANGE;
            return false;
          }
          if (ReducedCost(opposite_arc) < 0) {
            DCHECK(IsAdmissible(opposite_arc));

            // TODO(user): Try to steal flow if node_excess_[head] > 0.
            // An initial experiment didn't show a big speedup though.

            remaining_excess -= node_excess_[head];
            if (remaining_excess == 0) {
              node_potential_[head] -= potential_delta;
              break;
            }
            bfs_queue.push_back(head);
            node_in_queue[head] = true;
            if (potential_delta < 0) {
              first_admissible_arc_[head] =
                  GetFirstOutgoingOrOppositeIncomingArc(head);
            }
          } else {
            // The opposite_arc is not admissible but is in the residual graph;
            // this updates its min_non_admissible_potential.
            node_potential_[head] -= potential_delta;
            if (min_non_admissible_potential[head] == kMinCostValue) {
              nodes_to_process.push_back(head);
            }
            min_non_admissible_potential[head] = std::max(
                min_non_admissible_potential[head],
                node_potential_[node] - scaled_arc_unit_cost_[opposite_arc]);
          }
        }
      }
      if (remaining_excess == 0) break;
    }
    if (remaining_excess == 0) break;

    // Decrease by as much as possible instead of decreasing by epsilon.
    // TODO(user): Is it worth the extra loop?
    CostValue max_potential_diff = kMinCostValue;
    for (int i = 0; i < nodes_to_process.size(); ++i) {
      const NodeIndex node = nodes_to_process[i];
      if (node_in_queue[node]) continue;
      max_potential_diff =
          std::max(max_potential_diff,
                   min_non_admissible_potential[node] - node_potential_[node]);
      if (max_potential_diff == potential_delta) break;
    }
    DCHECK_LE(max_potential_diff, potential_delta);
    potential_delta = max_potential_diff - epsilon_;

    // Loop over nodes_to_process_ and for each node, apply the first of the
    // rules below that match or leave it in the queue for later iteration:
    // - Remove it if it is already in the queue.
    // - If the node is connected to S by an admissible arc after it is
    //   relabeled by +potential_delta, add it to bfs_queue_ and remove it from
    //   nodes_to_process.
    int index = 0;
    for (int i = 0; i < nodes_to_process.size(); ++i) {
      const NodeIndex node = nodes_to_process[i];
      if (node_in_queue[node]) continue;
      if (node_potential_[node] + potential_delta <
          min_non_admissible_potential[node]) {
        node_potential_[node] += potential_delta;
        if (node_potential_[node] < overflow_threshold_) {
          status_ = BAD_COST_RANGE;
          return false;
        }
        first_admissible_arc_[node] =
            GetFirstOutgoingOrOppositeIncomingArc(node);
        bfs_queue.push_back(node);
        node_in_queue[node] = true;
        remaining_excess -= node_excess_[node];
        continue;
      }

      // Keep the node for later iteration.
      nodes_to_process[index] = node;
      ++index;
    }
    nodes_to_process.resize(index);
  }

  // Update the potentials of the nodes not yet processed.
  if (potential_delta == 0) return true;
  for (NodeIndex node = 0; node < num_nodes; ++node) {
    if (!node_in_queue[node]) {
      node_potential_[node] += potential_delta;
      if (node_potential_[node] < overflow_threshold_) {
        status_ = BAD_COST_RANGE;
        return false;
      }
      first_admissible_arc_[node] = GetFirstOutgoingOrOppositeIncomingArc(node);
    }
  }
  return true;
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
bool GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::Refine() {
  SCOPED_TIME_STAT(&stats_);
  SaturateAdmissibleArcs();
  InitializeActiveNodeStack();

  const NodeIndex num_nodes = graph_->num_nodes();
  while (!active_nodes_.empty()) {
    // TODO(user): Experiment with different factors in front of num_nodes.
    if (num_relabels_since_last_price_update_ >= num_nodes) {
      num_relabels_since_last_price_update_ = 0;
      if (use_price_update_) {
        if (!UpdatePrices()) return false;
      }
    }
    const NodeIndex node = active_nodes_.top();
    active_nodes_.pop();
    DCHECK(IsActive(node));
    if (!Discharge(node)) return false;
  }
  return true;
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
bool GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::Discharge(
    NodeIndex node) {
  SCOPED_TIME_STAT(&stats_);
  while (true) {
    // The node is initially active, and we exit as soon as it becomes
    // inactive.
    DCHECK(IsActive(node));
    const CostValue tail_potential = node_potential_[node];
    for (OutgoingOrOppositeIncomingArcIterator it(*graph_, node,
                                                  first_admissible_arc_[node]);
         it.Ok(); it.Next()) {
      const ArcIndex arc = it.Index();
      if (!FastIsAdmissible(arc, tail_potential)) continue;

      // We look ahead to see if this node can accept the flow or will need
      // to be relabeled later in which case we do it now. Note that this will
      // just be skipped for self-loop so it is fine.
      const NodeIndex head = Head(arc);
      if (node_excess_[head] >= 0 && !NodeHasAdmissibleArc(head)) {
        // Relabel head and skip if the arc is not admissible anymore.
        if (!Relabel(head)) return false;
        if (!FastIsAdmissible(arc, tail_potential)) continue;
      }

      const FlowQuantity delta =
          std::min(node_excess_[node],
                   static_cast<FlowQuantity>(residual_arc_capacity_[arc]));
      const bool head_active_before_push = IsActive(head);
      FastPushFlow(delta, arc, node);
      if (IsActive(head) && !head_active_before_push) {
        active_nodes_.push(head);
      }

      if (node_excess_[node] == 0) {
        // arc may still be admissible.
        first_admissible_arc_[node] = arc;
        return true;
      }
    }
    if (!Relabel(node)) return false;
  }
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
bool GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::
    NodeHasAdmissibleArc(NodeIndex node) {
  SCOPED_TIME_STAT(&stats_);
  const CostValue tail_potential = node_potential_[node];
  for (OutgoingOrOppositeIncomingArcIterator it(*graph_, node,
                                                first_admissible_arc_[node]);
       it.Ok(); it.Next()) {
    const ArcIndex arc = it.Index();
    if (FastIsAdmissible(arc, tail_potential)) {
      first_admissible_arc_[node] = arc;
      return true;
    }
  }
  return false;
}

// Note that we only relabel if there is no leaving admissible arc.
template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
bool GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::Relabel(
    NodeIndex node) {
  SCOPED_TIME_STAT(&stats_);
  DCHECK(CheckRelabelPrecondition(node));
  ++num_relabels_since_last_price_update_;

  // By setting node_potential_[node] to the guaranteed_new_potential we are
  // sure to keep epsilon-optimality of the pseudo-flow. Note that we could
  // return right away with this value, but we prefer to check that this value
  // will lead to at least one admissible arc, and if not, to decrease the
  // potential as much as possible.
  const CostValue guaranteed_new_potential = node_potential_[node] - epsilon_;
  if (guaranteed_new_potential < overflow_threshold_) {
    status_ = BAD_COST_RANGE;
    return false;
  }

  // This will be updated to contain the minimum node potential for which
  // the node has no admissible arc. We know that:
  // - min_non_admissible_potential <= node_potential_[node]
  // - We can set the new node potential to min_non_admissible_potential -
  //   epsilon_ and still keep the epsilon-optimality of the pseudo flow.
  const CostValue kMinCostValue = std::numeric_limits<CostValue>::min();
  CostValue min_non_admissible_potential = kMinCostValue;

  // The following variables help setting the first_admissible_arc_[node] to a
  // value different from GetFirstOutgoingOrOppositeIncomingArc(node) which
  // avoids looking again at some arcs.
  CostValue previous_min_non_admissible_potential = kMinCostValue;
  ArcIndex first_arc = Graph::kNilArc;

  for (OutgoingOrOppositeIncomingArcIterator it(*graph_, node); it.Ok();
       it.Next()) {
    const ArcIndex arc = it.Index();
    if (residual_arc_capacity_[arc] > 0) {
      const CostValue min_non_admissible_potential_for_arc =
          node_potential_[Head(arc)] - scaled_arc_unit_cost_[arc];
      // We test this condition first as it is more probable that the first arc
      // with residual capacity will be admissible if we reduce the node
      // potential by epsilon.
      if (min_non_admissible_potential_for_arc > guaranteed_new_potential) {
        // We found an admissible arc for the guaranteed_new_potential. We
        // stop right now instead of trying to compute the minimum possible
        // new potential that keeps the epsilon-optimality of the pseudo flow.
        node_potential_[node] = guaranteed_new_potential;
        first_admissible_arc_[node] = arc;
        return true;
      }
      if (min_non_admissible_potential_for_arc > min_non_admissible_potential) {
        previous_min_non_admissible_potential = min_non_admissible_potential;
        min_non_admissible_potential = min_non_admissible_potential_for_arc;
        first_arc = arc;
      }
    }
  }

  // No residual arc leaves this node!
  //
  // TODO(user): This can be dealt with before the algorithm start so that we
  // do not need to test it here.
  if (min_non_admissible_potential == kMinCostValue) {
    if (node_excess_[node] != 0) {
      // Note that this infeasibility detection is incomplete.
      // Only max flow can detect that a min-cost flow problem is infeasible.
      status_ = INFEASIBLE;
      return false;
    } else {
      // This source saturates all its arcs, we can actually decrease the
      // potential by as much as we want.
      //
      // TODO(user): Set it to a minimum value? but be careful of overflow.
      node_potential_[node] = guaranteed_new_potential;
      first_admissible_arc_[node] = Graph::kNilArc;
    }
    return true;
  }

  // We decrease the potential as much as possible, but we do not know the first
  // admissible arc (most of the time). Keeping the
  // previous_min_non_admissible_potential makes it faster by a few percent.
  if (min_non_admissible_potential < overflow_threshold_) {
    status_ = BAD_COST_RANGE;
    return false;
  }
  const CostValue new_potential = min_non_admissible_potential - epsilon_;
  if (new_potential < overflow_threshold_) {
    status_ = BAD_COST_RANGE;
    return false;
  }
  node_potential_[node] = new_potential;
  if (previous_min_non_admissible_potential <= new_potential) {
    first_admissible_arc_[node] = first_arc;
  } else {
    // We have no indication of what may be the first admissible arc.
    first_admissible_arc_[node] = GetFirstOutgoingOrOppositeIncomingArc(node);
  }
  return true;
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
typename Graph::ArcIndex
GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::Opposite(
    ArcIndex arc) const {
  return graph_->OppositeArc(arc);
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
bool GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::IsArcValid(
    ArcIndex arc) const {
  return graph_->IsArcValid(arc);
}

template <typename Graph, typename ArcFlowType, typename ArcScaledCostType>
bool GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType>::IsArcDirect(
    ArcIndex arc) const {
  DCHECK(IsArcValid(arc));
  return arc >= 0;
}

// Explicit instantiations that can be used by a client.
//
// TODO(user): Move this code out of a .cc file and include it at the end of
// the header so it can work with any graph implementation?
template class GenericMinCostFlow<::util::ReverseArcListGraph<>>;
template class GenericMinCostFlow<::util::ReverseArcStaticGraph<>>;
template class GenericMinCostFlow<
    ::util::ReverseArcStaticGraph<uint16_t, int32_t>>;
template class GenericMinCostFlow<::util::ReverseArcListGraph<int64_t, int64_t>,
                                  int64_t, int64_t>;

// A more memory-efficient version for large graphs.
template class GenericMinCostFlow<
    ::util::ReverseArcStaticGraph<uint16_t, int32_t>,
    /*ArcFlowType=*/int16_t,
    /*ArcScaledCostType=*/int32_t>;

SimpleMinCostFlow::SimpleMinCostFlow(NodeIndex reserve_num_nodes,
                                     ArcIndex reserve_num_arcs) {
  if (reserve_num_nodes > 0) {
    node_supply_.reserve(reserve_num_nodes);
  }
  if (reserve_num_arcs > 0) {
    arc_tail_.reserve(reserve_num_arcs);
    arc_head_.reserve(reserve_num_arcs);
    arc_capacity_.reserve(reserve_num_arcs);
    arc_cost_.reserve(reserve_num_arcs);
    arc_permutation_.reserve(reserve_num_arcs);
    arc_flow_.reserve(reserve_num_arcs);
  }
}

void SimpleMinCostFlow::SetNodeSupply(NodeIndex node, FlowQuantity supply) {
  ResizeNodeVectors(node);
  node_supply_[node] = supply;
}

SimpleMinCostFlow::ArcIndex SimpleMinCostFlow::AddArcWithCapacityAndUnitCost(
    NodeIndex tail, NodeIndex head, FlowQuantity capacity,
    CostValue unit_cost) {
  ResizeNodeVectors(std::max(tail, head));
  const ArcIndex arc = arc_tail_.size();
  arc_tail_.push_back(tail);
  arc_head_.push_back(head);
  arc_capacity_.push_back(capacity);
  arc_cost_.push_back(unit_cost);
  return arc;
}

void SimpleMinCostFlow::SetArcCapacity(ArcIndex arc, FlowQuantity capacity) {
  arc_capacity_[arc] = capacity;
}

SimpleMinCostFlow::ArcIndex SimpleMinCostFlow::PermutedArc(ArcIndex arc) {
  return arc < arc_permutation_.size() ? arc_permutation_[arc] : arc;
}

SimpleMinCostFlow::Status SimpleMinCostFlow::SolveWithPossibleAdjustment(
    SupplyAdjustment adjustment) {
  optimal_cost_ = 0;
  maximum_flow_ = 0;
  arc_flow_.clear();
  const NodeIndex num_nodes = node_supply_.size();
  const ArcIndex num_arcs = arc_capacity_.size();
  if (num_nodes == 0) return OPTIMAL;

  int supply_node_count = 0, demand_node_count = 0;
  FlowQuantity total_supply = 0, total_demand = 0;
  for (NodeIndex node = 0; node < num_nodes; ++node) {
    if (node_supply_[node] > 0) {
      ++supply_node_count;
      total_supply += node_supply_[node];
    } else if (node_supply_[node] < 0) {
      ++demand_node_count;
      total_demand -= node_supply_[node];
    }
  }
  if (adjustment == DONT_ADJUST && total_supply != total_demand) {
    return UNBALANCED;
  }

  // Feasibility checking, and possible supply/demand adjustment, is done by:
  // 1. Creating a new source and sink node.
  // 2. Taking all nodes that have a non-zero supply or demand and
  //    connecting them to the source or sink respectively. The arc thus
  //    added has a capacity of the supply or demand.
  // 3. Computing the max flow between the new source and sink.
  // 4. If adjustment isn't being done, checking that the max flow is equal
  //    to the total supply/demand (and returning INFEASIBLE if it isn't).
  // 5. Running min-cost max-flow on this augmented graph, using the max
  //    flow computed in step 3 as the supply of the source and demand of
  //    the sink.
  const ArcIndex augmented_num_arcs =
      num_arcs + supply_node_count + demand_node_count;
  const NodeIndex source = num_nodes;
  const NodeIndex sink = num_nodes + 1;
  const NodeIndex augmented_num_nodes = num_nodes + 2;

  Graph graph(augmented_num_nodes, augmented_num_arcs);
  for (ArcIndex arc = 0; arc < num_arcs; ++arc) {
    graph.AddArc(arc_tail_[arc], arc_head_[arc]);
  }

  for (NodeIndex node = 0; node < num_nodes; ++node) {
    if (node_supply_[node] > 0) {
      graph.AddArc(source, node);
    } else if (node_supply_[node] < 0) {
      graph.AddArc(node, sink);
    }
  }

  graph.Build(&arc_permutation_);

  {
    GenericMaxFlow<Graph> max_flow(&graph, source, sink);
    ArcIndex arc;
    for (arc = 0; arc < num_arcs; ++arc) {
      max_flow.SetArcCapacity(PermutedArc(arc), arc_capacity_[arc]);
    }
    for (NodeIndex node = 0; node < num_nodes; ++node) {
      if (node_supply_[node] != 0) {
        max_flow.SetArcCapacity(PermutedArc(arc), std::abs(node_supply_[node]));
        ++arc;
      }
    }
    CHECK_EQ(arc, augmented_num_arcs);
    if (!max_flow.Solve()) {
      LOG(ERROR) << "Max flow could not be computed.";
      switch (max_flow.status()) {
        case MaxFlowStatusClass::NOT_SOLVED:
          return NOT_SOLVED;
        case MaxFlowStatusClass::OPTIMAL:
          LOG(ERROR)
              << "Max flow failed but claimed to have an optimal solution";
          ABSL_FALLTHROUGH_INTENDED;
        default:
          return BAD_RESULT;
      }
    }
    maximum_flow_ = max_flow.GetOptimalFlow();
  }

  if (adjustment == DONT_ADJUST && maximum_flow_ != total_supply) {
    return INFEASIBLE;
  }

  GenericMinCostFlow<Graph> min_cost_flow(&graph);
  ArcIndex arc;
  for (arc = 0; arc < num_arcs; ++arc) {
    ArcIndex permuted_arc = PermutedArc(arc);
    min_cost_flow.SetArcUnitCost(permuted_arc, arc_cost_[arc]);
    min_cost_flow.SetArcCapacity(permuted_arc, arc_capacity_[arc]);
  }
  for (NodeIndex node = 0; node < num_nodes; ++node) {
    if (node_supply_[node] != 0) {
      ArcIndex permuted_arc = PermutedArc(arc);
      min_cost_flow.SetArcCapacity(permuted_arc, std::abs(node_supply_[node]));
      min_cost_flow.SetArcUnitCost(permuted_arc, 0);
      ++arc;
    }
  }
  min_cost_flow.SetNodeSupply(source, maximum_flow_);
  min_cost_flow.SetNodeSupply(sink, -maximum_flow_);
  min_cost_flow.SetCheckFeasibility(false);
  min_cost_flow.SetPriceScaling(scale_prices_);

  arc_flow_.resize(num_arcs);
  if (min_cost_flow.Solve()) {
    optimal_cost_ = min_cost_flow.GetOptimalCost();
    for (arc = 0; arc < num_arcs; ++arc) {
      arc_flow_[arc] = min_cost_flow.Flow(PermutedArc(arc));
    }
  }

  return min_cost_flow.status();
}

SimpleMinCostFlow::CostValue SimpleMinCostFlow::OptimalCost() const {
  return optimal_cost_;
}

SimpleMinCostFlow::FlowQuantity SimpleMinCostFlow::MaximumFlow() const {
  return maximum_flow_;
}

SimpleMinCostFlow::FlowQuantity SimpleMinCostFlow::Flow(ArcIndex arc) const {
  return arc_flow_[arc];
}

SimpleMinCostFlow::SimpleMinCostFlow::NodeIndex SimpleMinCostFlow::NumNodes()
    const {
  return node_supply_.size();
}

SimpleMinCostFlow::ArcIndex SimpleMinCostFlow::NumArcs() const {
  return arc_tail_.size();
}

SimpleMinCostFlow::ArcIndex SimpleMinCostFlow::Tail(ArcIndex arc) const {
  return arc_tail_[arc];
}

SimpleMinCostFlow::ArcIndex SimpleMinCostFlow::Head(ArcIndex arc) const {
  return arc_head_[arc];
}

SimpleMinCostFlow::FlowQuantity SimpleMinCostFlow::Capacity(
    ArcIndex arc) const {
  return arc_capacity_[arc];
}

SimpleMinCostFlow::CostValue SimpleMinCostFlow::UnitCost(ArcIndex arc) const {
  return arc_cost_[arc];
}

SimpleMinCostFlow::FlowQuantity SimpleMinCostFlow::Supply(
    NodeIndex node) const {
  return node_supply_[node];
}

void SimpleMinCostFlow::ResizeNodeVectors(NodeIndex node) {
  if (node < node_supply_.size()) return;
  node_supply_.resize(node + 1);
}

}  // namespace operations_research
