// Copyright 2010-2012 Google
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

#include "graph/min_cost_flow.h"

#include <math.h>
#include <algorithm>
#include <limits>

#include "base/commandlineflags.h"
#include "base/stringprintf.h"
#include "base/mathutil.h"
#include "graph/max_flow.h"

DEFINE_int64(min_cost_flow_alpha, 5,
             "Divide factor for epsilon at each refine step.");
DEFINE_bool(min_cost_flow_check_feasibility, true,
            "Check that the graph has enough capacity to send all supplies "
             "and serve all demands. Also check that the sum of supplies "
             "is equal to the sum of demands.");
DEFINE_bool(min_cost_flow_check_balance, true,
            "Check that the sum of supplies is equal to the sum of demands.");
DEFINE_bool(min_cost_flow_check_costs, true,
            "Check that the magnitude of the costs will not exceed the "
            "precision of the machine when scaled (multiplied) by the number "
            "of nodes");
DEFINE_bool(min_cost_flow_fast_potential_update, true,
            "Fast node potential update. Faster when set to true, but does not"
            "detect as many infeasibilities as when set to false.");
DEFINE_bool(min_cost_flow_check_result, true,
            "Check that the result is valid.");

namespace operations_research {

MinCostFlow::MinCostFlow(const StarGraph* graph)
    : graph_(graph),
      node_excess_(),
      node_potential_(),
      residual_arc_capacity_(),
      first_admissible_arc_(),
      active_nodes_(),
      epsilon_(0),
      alpha_(FLAGS_min_cost_flow_alpha),
      cost_scaling_factor_(1),
      scaled_arc_unit_cost_(),
      total_flow_cost_(0),
      status_(NOT_SOLVED),
      initial_node_excess_(),
      feasible_node_excess_(),
      feasibility_checked_(false) {
  const NodeIndex max_num_nodes = graph_->max_num_nodes();
  if (max_num_nodes > 0) {
    node_excess_.Reserve(StarGraph::kFirstNode, max_num_nodes - 1);
    node_excess_.SetAll(0);
    node_potential_.Reserve(StarGraph::kFirstNode, max_num_nodes - 1);
    node_potential_.SetAll(0);
    first_admissible_arc_.Reserve(StarGraph::kFirstNode, max_num_nodes - 1);
    first_admissible_arc_.SetAll(StarGraph::kNilArc);
    initial_node_excess_.Reserve(StarGraph::kFirstNode, max_num_nodes - 1);
    initial_node_excess_.SetAll(0);
    feasible_node_excess_.Reserve(StarGraph::kFirstNode, max_num_nodes - 1);
    feasible_node_excess_.SetAll(0);
  }
  const ArcIndex max_num_arcs = graph_->max_num_arcs();
  if (max_num_arcs > 0) {
    residual_arc_capacity_.Reserve(-max_num_arcs, max_num_arcs - 1);
    residual_arc_capacity_.SetAll(0);
    scaled_arc_unit_cost_.Reserve(-max_num_arcs, max_num_arcs - 1);
    scaled_arc_unit_cost_.SetAll(0);
  }
}

void MinCostFlow::SetArcCapacity(ArcIndex arc, FlowQuantity new_capacity) {
  DCHECK_LE(0, new_capacity);
  DCHECK(graph_->CheckArcValidity(arc));
  const FlowQuantity free_capacity = residual_arc_capacity_[arc];
  const FlowQuantity capacity_delta = new_capacity - Capacity(arc);
  VLOG(3) << "Changing capacity on arc " << arc
          << " from " << Capacity(arc) << " to " << new_capacity
          << ". Current free capacity = " << free_capacity;
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
    VLOG(3) << "Now: capacity = " << Capacity(arc) << " flow = " << Flow(arc);
  } else {
    // We have to reduce the flow on the arc, and update the excesses
    // accordingly.
    const FlowQuantity flow = residual_arc_capacity_[Opposite(arc)];
    const FlowQuantity flow_excess = flow - new_capacity;
    VLOG(3) << "Flow value " << flow << " exceeds new capacity "
            << new_capacity << " by " << flow_excess;
    residual_arc_capacity_.Set(arc, 0);
    residual_arc_capacity_.Set(Opposite(arc), new_capacity);
    const NodeIndex tail = Tail(arc);
    node_excess_.Set(tail, node_excess_[tail] + flow_excess);
    const NodeIndex head = Head(arc);
    node_excess_.Set(head, node_excess_[head] - flow_excess);
    DCHECK_LE(0, residual_arc_capacity_[arc]);
    DCHECK_LE(0, residual_arc_capacity_[Opposite(arc)]);
    VLOG(3) << DebugString("After SetArcCapacity:", arc);
  }
}

bool MinCostFlow::CheckInputConsistency() const {
  FlowQuantity total_supply = 0;
  uint64 max_capacity = 0;  // uint64 because it is positive and will be used
                            // to check against FlowQuantity overflows.
  for (StarGraph::ArcIterator arc_it(*graph_); arc_it.Ok(); arc_it.Next()) {
    const ArcIndex arc = arc_it.Index();
    const uint64 capacity = static_cast<uint64>(residual_arc_capacity_[arc]);
    max_capacity = std::max(capacity, max_capacity);
  }
  uint64 total_flow = 0;   // uint64 for the same reason as max_capacity.
  for (StarGraph::NodeIterator node_it(*graph_); node_it.Ok(); node_it.Next()) {
    const NodeIndex node = node_it.Index();
    const FlowQuantity excess = node_excess_[node];
    total_supply += excess;
    if (excess > 0) {
      total_flow += excess;
      if (std::numeric_limits<FlowQuantity>::max() <
          max_capacity + total_flow) {
        LOG(DFATAL) << "Input consistency error: max capacity + flow exceed "
                    << "precision";
      }
    }
  }
  if (total_supply != 0) {
    LOG(DFATAL) << "Input consistency error: unbalanced problem";
  }
  return true;
}

bool MinCostFlow::CheckResult() const {
  for (StarGraph::NodeIterator it(*graph_); it.Ok(); it.Next()) {
    const NodeIndex node = it.Index();
    if (node_excess_[node] != 0) {
      LOG(DFATAL) << "node_excess_[" << node << "] != 0";
      return false;
    }
    for (StarGraph::IncidentArcIterator
        arc_it(*graph_, node); arc_it.Ok(); arc_it.Next()) {
      const ArcIndex arc = arc_it.Index();
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
      }
    }
  }
  return true;
}

bool MinCostFlow::CheckCostRange() const {
  CostValue min_cost_magnitude = std::numeric_limits<CostValue>::max();
  CostValue max_cost_magnitude = 0;
  // Traverse the initial arcs of the graph:
  for (StarGraph::ArcIterator arc_it(*graph_); arc_it.Ok(); arc_it.Next()) {
    const ArcIndex arc = arc_it.Index();
    const CostValue cost_magnitude = MathUtil::Abs(scaled_arc_unit_cost_[arc]);
    max_cost_magnitude = std::max(max_cost_magnitude, cost_magnitude);
    if (cost_magnitude != 0.0) {
      min_cost_magnitude = std::min(min_cost_magnitude, cost_magnitude);
    }
  }
  VLOG(3) << "Min cost magnitude = " << min_cost_magnitude
          << ", Max cost magnitude = " << max_cost_magnitude;
#if !defined(_MSC_VER)
  if (log(std::numeric_limits<CostValue>::max()) <
      log(max_cost_magnitude + 1) + log(graph_->num_nodes() + 1)) {
    LOG(DFATAL) << "Maximum cost magnitude " << max_cost_magnitude << " is too "
                << "high for the number of nodes. Try changing the data.";
    return false;
  }
#endif
  return true;
}

bool MinCostFlow::CheckRelabelPrecondition(NodeIndex node) const {
  DCHECK(IsActive(node));
  for (StarGraph::IncidentArcIterator arc_it(*graph_, node);
       arc_it.Ok();
       arc_it.Next()) {
    const ArcIndex arc = arc_it.Index();
    DCHECK(!IsAdmissible(arc));
  }
  return true;
}

string MinCostFlow::DebugString(const string& context, ArcIndex arc) const {
  const NodeIndex tail = Tail(arc);
  const NodeIndex head = Head(arc);
  // Reduced cost is computed directly without calling ReducedCost to avoid
  // recursive calls between ReducedCost and DebugString in case a DCHECK in
  // ReducedCost fails.
  const CostValue reduced_cost = scaled_arc_unit_cost_[arc]
                               + node_potential_[tail]
                               - node_potential_[head];
  return StringPrintf("%s Arc %d, from %d to %d, "
                      "Capacity = %lld, Residual capacity = %lld, "
                      "Flow = residual capacity for reverse arc = %lld, "
                      "Height(tail) = %lld, Height(head) = %lld, "
                      "Excess(tail) = %lld, Excess(head) = %lld, "
                      "Cost = %lld, Reduced cost = %lld, ",
                      context.c_str(), arc, tail, head, Capacity(arc),
                      residual_arc_capacity_[arc], Flow(arc),
                      node_potential_[tail], node_potential_[head],
                      node_excess_[tail], node_excess_[head],
                      scaled_arc_unit_cost_[arc], reduced_cost);
}

bool MinCostFlow::CheckFeasibility(
  std::vector<NodeIndex>* const infeasible_supply_node,
  std::vector<NodeIndex>* const infeasible_demand_node) {
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
  // For this it is necessary to make StarGraph resizable.
  feasibility_checked_ = false;
  ArcIndex num_extra_arcs = 0;
  for (StarGraph::NodeIterator it(*graph_); it.Ok(); it.Next()) {
    const NodeIndex node = it.Index();
    if (initial_node_excess_[node] != 0) {
      ++num_extra_arcs;
    }
  }
  const NodeIndex num_nodes_in_max_flow = graph_->num_nodes() + 2;
  const ArcIndex num_arcs_in_max_flow = graph_->num_arcs() + num_extra_arcs;
  const NodeIndex source = num_nodes_in_max_flow - 2;
  const NodeIndex sink = num_nodes_in_max_flow - 1;
  StarGraph checker_graph(num_nodes_in_max_flow, num_arcs_in_max_flow);
  MaxFlow checker(&checker_graph, source, sink);
  // Copy graph_ to checker_graph.
  for (StarGraph::ArcIterator it(*graph_); it.Ok(); it.Next()) {
    const ArcIndex arc = it.Index();
    const ArcIndex new_arc = checker_graph.AddArc(graph_->Tail(arc),
                                                  graph_->Head(arc));
    DCHECK_EQ(arc, new_arc);
    checker.SetArcCapacity(new_arc, Capacity(arc));
  }
  FlowQuantity total_demand = 0;
  FlowQuantity total_supply = 0;
  // Create the source-to-supply node arcs and the demand-node-to-sink arcs.
  for (StarGraph::NodeIterator it(*graph_); it.Ok(); it.Next()) {
    const NodeIndex node = it.Index();
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
  feasible_node_excess_.SetAll(0);
  for (StarGraph::OutgoingArcIterator it(checker_graph, source);
       it.Ok(); it.Next()) {
    const ArcIndex arc = it.Index();
    const NodeIndex node = checker_graph.Head(arc);
    const FlowQuantity flow = checker.Flow(arc);
    feasible_node_excess_.Set(node, flow);
    if (infeasible_supply_node != NULL) {
      infeasible_supply_node->push_back(node);
    }
  }
  for (StarGraph::IncomingArcIterator it(checker_graph, sink);
       it.Ok(); it.Next()) {
    const ArcIndex arc = checker_graph.DirectArc(it.Index());
    const NodeIndex node = checker_graph.Tail(arc);
    const FlowQuantity flow = checker.Flow(arc);
    feasible_node_excess_.Set(node, -flow);
    if (infeasible_demand_node != NULL) {
      infeasible_demand_node->push_back(node);
    }
  }
  feasibility_checked_ = true;
  return optimal_max_flow == total_supply;
}

bool MinCostFlow::MakeFeasible() {
  if (!feasibility_checked_) {
    return false;
  }
  for (StarGraph::NodeIterator it(*graph_); it.Ok(); it.Next()) {
    const NodeIndex node = it.Index();
    const FlowQuantity excess = feasible_node_excess_[node];
    node_excess_.Set(node, excess);
    initial_node_excess_.Set(node, excess);
  }
  return true;
}

bool MinCostFlow::Solve() {
  status_ = NOT_SOLVED;
  if (FLAGS_min_cost_flow_check_balance && !CheckInputConsistency()) {
    status_ = UNBALANCED;
    return false;
  }
  if (FLAGS_min_cost_flow_check_costs && !CheckCostRange()) {
    status_ = BAD_COST_RANGE;
    return false;
  }
  if (FLAGS_min_cost_flow_check_feasibility && !CheckFeasibility(NULL, NULL)) {
    status_ = INFEASIBLE;
    return false;
  }
  node_potential_.SetAll(0);
  ResetFirstAdmissibleArcs();
  ScaleCosts();
  Optimize();
  if (FLAGS_min_cost_flow_check_result && !CheckResult()) {
    status_ = BAD_RESULT;
    UnscaleCosts();
    return false;
  }
  UnscaleCosts();
  if (status_ != OPTIMAL) {
    LOG(DFATAL) << "Status != OPTIMAL";
    total_flow_cost_ = 0;
    return false;
  }
  total_flow_cost_ = 0;
  for (StarGraph::ArcIterator it(*graph_); it.Ok(); it.Next()) {
    const ArcIndex arc = it.Index();
    const FlowQuantity flow_on_arc = residual_arc_capacity_[Opposite(arc)];
    VLOG(3) << "Flow for arc " << arc << " = " << flow_on_arc
            << ", scaled cost = " << scaled_arc_unit_cost_[arc];
    total_flow_cost_ += scaled_arc_unit_cost_[arc] * flow_on_arc;
  }
  status_ = OPTIMAL;
  return true;
}

void MinCostFlow::ResetFirstAdmissibleArcs() {
  for (StarGraph::NodeIterator node_it(*graph_); node_it.Ok(); node_it.Next()) {
    const NodeIndex node = node_it.Index();
    first_admissible_arc_.Set(node, GetFirstIncidentArc(node));
  }
}

void MinCostFlow::ScaleCosts() {
  cost_scaling_factor_ = graph_->num_nodes() + 1;
  epsilon_ = 1LL;
  VLOG(3) << "Number of arcs in the graph = " << graph_->num_arcs();
  for (StarGraph::ArcIterator arc_it(*graph_); arc_it.Ok(); arc_it.Next()) {
    const ArcIndex arc = arc_it.Index();
    const CostValue cost = scaled_arc_unit_cost_[arc] * cost_scaling_factor_;
    scaled_arc_unit_cost_.Set(arc, cost);
    scaled_arc_unit_cost_.Set(Opposite(arc), -cost);
    epsilon_ = std::max(epsilon_, MathUtil::Abs(cost));
  }
  VLOG(3) << "Initial epsilon = " << epsilon_;
  VLOG(3) << "Cost scaling factor = " << cost_scaling_factor_;
}

void MinCostFlow::UnscaleCosts() {
  for (StarGraph::ArcIterator arc_it(*graph_); arc_it.Ok(); arc_it.Next()) {
    const ArcIndex arc = arc_it.Index();
    const CostValue cost = scaled_arc_unit_cost_[arc] / cost_scaling_factor_;
    scaled_arc_unit_cost_.Set(arc, cost);
    scaled_arc_unit_cost_.Set(Opposite(arc), -cost);
  }
  cost_scaling_factor_ = 1;
}

void MinCostFlow::Optimize() {
  const CostValue kEpsilonMin = 1LL;
  do {
    // Avoid epsilon_ == 0.
    epsilon_ = std::max(epsilon_ / alpha_, kEpsilonMin);
    VLOG(3) << "Epsilon changed to: " << epsilon_;
    Refine();
  } while (epsilon_ != 1LL && status_ != INFEASIBLE);
  if (status_ == NOT_SOLVED) {
    status_ = OPTIMAL;
  }
}

void MinCostFlow::SaturateAdmissibleArcs() {
  for (StarGraph::NodeIterator node_it(*graph_); node_it.Ok(); node_it.Next()) {
    const NodeIndex node = node_it.Index();
    for (StarGraph::IncidentArcIterator
             arc_it(*graph_, node, first_admissible_arc_[node]);
         arc_it.Ok();
         arc_it.Next()) {
      const ArcIndex arc = arc_it.Index();
      if (IsAdmissible(arc)) {
        VLOG(3) << DebugString("SaturateAdmissibleArcs: calling PushFlow", arc);
        PushFlow(residual_arc_capacity_[arc], arc);
      }
    }
  }
}

void MinCostFlow::PushFlow(FlowQuantity flow, ArcIndex arc) {
  DCHECK_GT(residual_arc_capacity_[arc], 0);
  VLOG(3) << "PushFlow: pushing " << flow << " on arc " << arc
          << " from node " << Tail(arc) << " to node " << Head(arc);
  // Reduce the residual capacity on arc by flow.
  residual_arc_capacity_.Set(arc, residual_arc_capacity_[arc] - flow);
  // Increase the residual capacity on opposite arc by flow.
  const ArcIndex opposite = Opposite(arc);
  residual_arc_capacity_.Set(opposite, residual_arc_capacity_[opposite] + flow);
  // Update the excesses at the tail and head of the arc.
  const NodeIndex tail = Tail(arc);
  node_excess_.Set(tail, node_excess_[tail] - flow);
  const NodeIndex head = Head(arc);
  node_excess_.Set(head, node_excess_[head] + flow);
  VLOG(4) << DebugString("PushFlow: ", arc);
}

void MinCostFlow::InitializeActiveNodeStack() {
  DCHECK(active_nodes_.empty());
  for (StarGraph::NodeIterator node_it(*graph_); node_it.Ok(); node_it.Next()) {
    const NodeIndex node = node_it.Index();
    if (IsActive(node)) {
      active_nodes_.push(node);
      VLOG(3) << "InitializeActiveNodeStack: node " << node << " added.";
    }
  }
}

void MinCostFlow::Refine() {
  SaturateAdmissibleArcs();
  InitializeActiveNodeStack();
  while (!active_nodes_.empty()) {
    const NodeIndex node = active_nodes_.top();
    active_nodes_.pop();
    if (IsActive(node)) {
      VLOG(3) << "Refine: calling Discharge for node " << node;
      Discharge(node);
      if (status_ == INFEASIBLE) {
        return;
      }
    }
  }
}

void MinCostFlow::Discharge(NodeIndex node) {
  DCHECK(IsActive(node));
  VLOG(3) << "Discharging node " << node << ", excess = " << node_excess_[node];
  while (IsActive(node)) {
    for (StarGraph::IncidentArcIterator arc_it(*graph_, node,
                                               first_admissible_arc_[node]);
         arc_it.Ok();
         arc_it.Next()) {
      const ArcIndex arc = arc_it.Index();
      VLOG(4) << DebugString("Discharge: considering", arc);
      if (IsAdmissible(arc)) {
        if (node_excess_[node] != 0) {
          VLOG(3) << "Discharge: calling PushFlow.";
          const NodeIndex head = Head(arc);
          const bool head_active_before_push = IsActive(head);
          const FlowQuantity delta = std::min(node_excess_[node],
                                              residual_arc_capacity_[arc]);
          PushFlow(delta, arc);
          if (IsActive(head) && !head_active_before_push) {
            active_nodes_.push(Head(arc));
          }
        }
        if (node_excess_[node] == 0) {
          first_admissible_arc_.Set(node, arc);  // arc may still be admissible.
          return;
        }
      }
    }
    Relabel(node);
    if (status_ == INFEASIBLE) {
      return;
    }
  }
}

void MinCostFlow::Relabel(NodeIndex node) {
  DCHECK(CheckRelabelPrecondition(node));
  CostValue new_potential;
  if (FLAGS_min_cost_flow_fast_potential_update) {
    new_potential = node_potential_[node] - epsilon_;
  } else {
    new_potential = kint64min;
    for (StarGraph::IncidentArcIterator arc_it(*graph_, node);
         arc_it.Ok();
         arc_it.Next()) {
      ArcIndex arc = arc_it.Index();
      DCHECK_EQ(Tail(arc), node);
      if (residual_arc_capacity_[arc] > 0) {
        const NodeIndex head = Head(arc);
        new_potential = std::max(new_potential, node_potential_[head]
                                              - scaled_arc_unit_cost_[arc]
                                              - epsilon_);
        DCHECK_GE(0, new_potential);
      }
    }
    if (new_potential == kint64min) {
      // Note that this infeasibility detection is incomplete.
      // Only max flow can detect that a min-cost flow problem is infeasible.
      status_ = INFEASIBLE;
      return;
    }
  }
  VLOG(3) << "Relabel: node " << node << " from " << node_potential_[node]
          << " to " << new_potential;
  node_potential_.Set(node, new_potential);
  first_admissible_arc_.Set(node, GetFirstIncidentArc(node));
}
}  // namespace operations_research
