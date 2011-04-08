// Copyright 2010 Google
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
#include <limits>
#include "base/commandlineflags.h"

DEFINE_int64(min_cost_flow_alpha, 5,
             "Divide factor for epsilon at each refine step.");

namespace operations_research {

MinCostFlow::MinCostFlow(const StarGraph& graph)
    : graph_(graph),
      node_excess_(),
      node_potential_(),
      residual_arc_capacity_(),
      first_admissible_arc_(),
      active_nodes_(),
      epsilon_(0),
      alpha_(FLAGS_min_cost_flow_alpha),
      cost_scaling_factor_(1),
      scaled_arc_unit_cost_() {
  const ArcIndex max_num_arcs = graph_.max_num_arcs();
  CHECK_GE(max_num_arcs, 1);
  const NodeIndex max_num_nodes = graph_.max_num_nodes();
  CHECK_GE(max_num_nodes, 1);
  node_excess_.Reserve(1, max_num_nodes);
  node_excess_.Assign(0);
  node_potential_.Reserve(1, max_num_nodes);
  node_potential_.Assign(0);
  residual_arc_capacity_.Reserve(-max_num_arcs, max_num_arcs);
  residual_arc_capacity_.Assign(0);
  first_admissible_arc_.Reserve(1, max_num_nodes);
  scaled_arc_unit_cost_.Reserve(-max_num_arcs, max_num_arcs);
  scaled_arc_unit_cost_.Assign(0);
}

bool MinCostFlow::CheckInputConsistency() const {
  FlowQuantity total_supply = 0;
  for (StarGraph::NodeIterator node_it(graph_); node_it.Ok(); node_it.Next()) {
    total_supply += node_excess_[node_it.Index()];
  }
  CHECK_EQ(0, total_supply);
  return true;
}

bool MinCostFlow::CheckResult() const {
  for (StarGraph::NodeIterator it(graph_); it.Ok(); it.Next()) {
    const NodeIndex node = it.Index();
    CHECK_EQ(0, node_excess_[node]);
    for (StarGraph::IncidentArcIterator
        arc_it(graph_, node); arc_it.Ok(); arc_it.Next()) {
      const ArcIndex arc = arc_it.Index();
      CHECK_LE(0, residual_arc_capacity_[arc]);
      CHECK(residual_arc_capacity_[arc] == 0 || ReducedCost(arc) >= -epsilon_)
          << residual_arc_capacity_[arc] << " " << ReducedCost(arc);
    }
  }
  return true;
}

bool MinCostFlow::CheckCostRange() const {
  CostValue min_cost_magnitude = std::numeric_limits<CostValue>::max();
  CostValue max_cost_magnitude = 0;
  // Traverse the initial arcs of the graph:
  for (StarGraph::ArcIterator arc_it(graph_); arc_it.Ok(); arc_it.Next()) {
    const ArcIndex arc = arc_it.Index();
    const CostValue cost_magnitude = scaled_arc_unit_cost_[arc] > 0 ?
        scaled_arc_unit_cost_[arc] :
        -scaled_arc_unit_cost_[arc];
    max_cost_magnitude = max(max_cost_magnitude, cost_magnitude);
    if (cost_magnitude != 0.0) {
      min_cost_magnitude = min(min_cost_magnitude, cost_magnitude);
    }
  }
  VLOG(1) << "Min cost magnitude = " << min_cost_magnitude
          << ", Max cost magnitude = " << max_cost_magnitude;
#if !defined(_MSC_VER)
  CHECK_GE(log(std::numeric_limits<CostValue>::max()),
           log(max_cost_magnitude) + log(graph_.num_nodes() + 1))
      << "Maximum cost is too high for the number of nodes. "
      << "Try changing the data.";
#endif
  return true;
}

bool MinCostFlow::CheckRelabelPrecondition(NodeIndex node) const {
  CHECK(IsActive(node));
  for (StarGraph::IncidentArcIterator arc_it(graph_, node);
       arc_it.Ok();
       arc_it.Next()) {
    const ArcIndex arc = arc_it.Index();
    CHECK(!IsAdmissible(arc)) << DebugString("CheckRelabelPrecondition:", arc);
  }
  return true;
}

string MinCostFlow::DebugString(const string& context, ArcIndex arc) const {
  const NodeIndex tail = Tail(arc);
  const NodeIndex head = Head(arc);
  return StringPrintf("%s Arc %lld, from %lld to %lld, "
                      "Capacity = %lld, Residual capacity = %lld, "
                      "Flow = residual capacity for reverse arc = %lld, "
                      "Height(tail) = %lld, Height(head) = %lld, "
                      "Excess(tail) = %lld, Excess(head) = %lld, "
                      "Cost = %lld, Reduced cost = %lld, ",
                      context.c_str(), arc, tail, head, Capacity(arc),
                      residual_arc_capacity_[arc], residual_arc_capacity_[-arc],
                      node_potential_[tail], node_potential_[head],
                      node_excess_[tail], node_excess_[head],
                      scaled_arc_unit_cost_[arc], ReducedCost(arc));
}

CostValue MinCostFlow::ComputeMinCostFlow() {
  DCHECK(CheckInputConsistency());
  DCHECK(CheckCostRange());
  CompleteGraph();
  ResetFirstAdmissibleArcs();
  ScaleCosts();
  Optimize();
  UnscaleCosts();
  CostValue total_flow_cost = 0;
  for (StarGraph::ArcIterator it(graph_); it.Ok(); it.Next()) {
    const ArcIndex arc = it.Index();
    const FlowQuantity flow_on_arc = residual_arc_capacity_[-arc];
    VLOG(1) << "Flow for arc " << arc << " = " << flow_on_arc
            << ", scaled cost = " << scaled_arc_unit_cost_[arc];
    total_flow_cost += scaled_arc_unit_cost_[arc] * flow_on_arc;
  }
  return total_flow_cost;
}

void MinCostFlow::CompleteGraph() {
  // Set the capacities of reverse arcs to zero, and the unit costs of reverse
  // arcs equal to the opposite of the cost for the corresponding direct arc.
  for (StarGraph::ArcIterator arc_it(graph_); arc_it.Ok(); arc_it.Next()) {
    const ArcIndex arc = arc_it.Index();
    residual_arc_capacity_.Set(Opposite(arc), 0);
    scaled_arc_unit_cost_.Set(Opposite(arc), -scaled_arc_unit_cost_[arc]);
  }
}

void MinCostFlow::ResetFirstAdmissibleArcs() {
  for (StarGraph::NodeIterator node_it(graph_); node_it.Ok(); node_it.Next()) {
    const NodeIndex node = node_it.Index();
    first_admissible_arc_.Set(node, GetFirstIncidentArc(node));
  }
}

void MinCostFlow::ScaleCosts() {
  cost_scaling_factor_ = graph_.num_nodes() + 1;
  epsilon_ = 1LL;
  VLOG(1) << "Number of arcs in the graph = " << graph_.num_arcs();
  for (StarGraph::ArcIterator arc_it(graph_); arc_it.Ok(); arc_it.Next()) {
    const ArcIndex arc = arc_it.Index();
    const CostValue cost = scaled_arc_unit_cost_[arc] * cost_scaling_factor_;
    scaled_arc_unit_cost_.Set(arc, cost);
    scaled_arc_unit_cost_.Set(Opposite(arc), -cost);
    epsilon_ = max(epsilon_, cost >= 0 ? cost : -cost);
  }
  VLOG(1) << "Initial epsilon = " << epsilon_;
  VLOG(1) << "Cost scaling factor = " << cost_scaling_factor_;
}

void MinCostFlow::UnscaleCosts() {
  for (StarGraph::ArcIterator arc_it(graph_); arc_it.Ok(); arc_it.Next()) {
    const ArcIndex arc = arc_it.Index();
    const CostValue cost = scaled_arc_unit_cost_[arc] / cost_scaling_factor_;
    scaled_arc_unit_cost_.Set(arc, cost);
    scaled_arc_unit_cost_.Set(Opposite(arc), -cost);
  }
  cost_scaling_factor_ = 1;
}

void MinCostFlow::Optimize() {
  while (epsilon_ > 1) {
    epsilon_ = max(epsilon_ / alpha_, 1LL);  // avoid epsilon_ == 0.
    VLOG(1) << "Epsilon changed to: " << epsilon_;
    Refine();
  }
  DCHECK(CheckResult());
}

void MinCostFlow::SaturateAdmissibleArcs() {
  for (StarGraph::NodeIterator node_it(graph_); node_it.Ok(); node_it.Next()) {
    const NodeIndex node = node_it.Index();
    for (StarGraph::IncidentArcIterator
             arc_it(graph_, node, first_admissible_arc_[node]);
         arc_it.Ok();
         arc_it.Next()) {
      const ArcIndex arc = arc_it.Index();
      if (IsAdmissible(arc)) {
        VLOG(1) << DebugString("SaturateAdmissibleArcs: calling PushFlow", arc);
        PushFlow(residual_arc_capacity_[arc], arc);
      }
    }
  }
}

void MinCostFlow::PushFlow(FlowQuantity flow, ArcIndex arc) {
  DCHECK_GT(residual_arc_capacity_[arc], 0);
  VLOG(1) << "PushFlow: pushing " << flow << " on arc " << arc
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
  VLOG(2) << DebugString("PushFlow: ", arc);
}

void MinCostFlow::InitializeActiveNodeStack() {
  CHECK(active_nodes_.empty());
  for (StarGraph::NodeIterator node_it(graph_); node_it.Ok(); node_it.Next()) {
    const NodeIndex node = node_it.Index();
    if (IsActive(node)) {
      active_nodes_.push(node);
      VLOG(1) << "InitializeActiveNodeStack: node " << node << " added.";
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
      VLOG(1) << "Refine: calling Discharge for node " << node;
      Discharge(node);
    }
  }
}

void MinCostFlow::Discharge(NodeIndex node) {
  DCHECK(IsActive(node));
  VLOG(1) << "Discharging node " << node << ", excess = " << node_excess_[node];
  while (IsActive(node)) {
    for (StarGraph::IncidentArcIterator arc_it(graph_, node,
                                               first_admissible_arc_[node]);
         arc_it.Ok();
         arc_it.Next()) {
      const ArcIndex arc = arc_it.Index();
      VLOG(2) << DebugString("Discharge: considering", arc);
      if (IsAdmissible(arc)) {
        if (node_excess_[node] != 0) {
          VLOG(1) << "Discharge: calling PushFlow.";
          const NodeIndex head = Head(arc);
          const bool head_active_before_push = IsActive(head);
          const FlowQuantity delta = min(node_excess_[node],
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
  }
}

void MinCostFlow::Relabel(NodeIndex node) {
  DCHECK(CheckRelabelPrecondition(node));
  CostValue new_potential = node_potential_[node] - epsilon_;
  VLOG(1) << "Relabel: node " << node << " from " << node_potential_[node]
          << " to " << new_potential;
  node_potential_.Set(node, new_potential);
  first_admissible_arc_.Set(node, GetFirstIncidentArc(node));
}
}  // namespace operations_research
