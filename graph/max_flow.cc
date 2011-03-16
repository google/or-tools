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

#include <limits>
#include "graph/max_flow.h"

namespace operations_research {


MaxFlow::MaxFlow(const StarGraph& graph,
                 NodeIndex source,
                 NodeIndex sink)
    : graph_(graph),
      node_excess_(),
      node_potential_(),
      residual_arc_capacity_(),
      first_admissible_arc_(),
      active_nodes_(),
      source_(source),
      sink_(sink) {
  const ArcIndex max_num_arcs = graph_.max_num_arcs();
  CHECK_GE(max_num_arcs, 1);
  const NodeIndex max_num_nodes = graph_.max_num_nodes();
  CHECK_GE(max_num_nodes, 1);
  node_excess_.Reserve(1, max_num_nodes);
  node_excess_.Assign(0);
  node_potential_.Reserve(1, max_num_nodes);
  node_potential_.Assign(0);
  residual_arc_capacity_.Reserve(-max_num_arcs, max_num_arcs);
  first_admissible_arc_.Reserve(1, max_num_nodes);
  CHECK(graph_.CheckNodeValidity(source_));
  CHECK(graph_.CheckNodeValidity(sink_));
}

bool MaxFlow::CheckInputConsistency() const {
  for (StarGraph::ArcIterator arc_it(graph_); arc_it.Ok(); arc_it.Next()) {
    const ArcIndex arc = arc_it.Index();
    CHECK_LE(0, residual_arc_capacity_[arc]);
  }
  return true;
}

bool MaxFlow::CheckResult() const {
  for (StarGraph::NodeIterator node_it(graph_); node_it.Ok(); node_it.Next()) {
    const NodeIndex node = node_it.Index();
    if (node != source_ && node != sink_) {
      CHECK_EQ(0, node_excess_[node]) << " node = " << node;
    }
  }
  for (StarGraph::ArcIterator arc_it(graph_); arc_it.Ok(); arc_it.Next()) {
    const ArcIndex arc = arc_it.Index();
    const ArcIndex opposite = Opposite(arc);
    CHECK_GE(residual_arc_capacity_[arc], 0);
    CHECK_GE(residual_arc_capacity_[opposite], 0);
    // The initial capacity of the direct arcs is non-negative.
    CHECK_GE(residual_arc_capacity_[arc] + residual_arc_capacity_[opposite], 0);
  }
  return true;
}

bool MaxFlow::CheckRelabelPrecondition(NodeIndex node) const {
  CHECK(IsActive(node));
  for (StarGraph::IncidentArcIterator arc_it(graph_, node);
       arc_it.Ok();
       arc_it.Next()) {
    const ArcIndex arc = arc_it.Index();
    CHECK(!IsAdmissible(arc)) << DebugString("CheckRelabelPrecondition:", arc);
  }
  return true;
}

string MaxFlow::DebugString(const string& context, ArcIndex arc) const {
  const NodeIndex tail = Tail(arc);
  const NodeIndex head = Head(arc);
  return StringPrintf("%s Arc %lld, from %lld to %lld, "
                      "Capacity = %lld, Residual capacity = %lld, "
                      "Flow = residual capacity for reverse arc = %lld, "
                      "Height(tail) = %lld, Height(head) = %lld, "
                      "Excess(tail) = %lld, Excess(head) = %lld",
                      context.c_str(), arc, tail, head, Capacity(arc),
                      residual_arc_capacity_[arc], Flow(arc),
                      node_potential_[tail], node_potential_[head],
                      node_excess_[tail], node_excess_[head]);
}

FlowQuantity MaxFlow::ComputeMaxFlow() {
  DCHECK(CheckInputConsistency());
  CompleteGraph();
  ResetFirstAdmissibleArcs();
  InitializePreflow();
  Refine();
  DCHECK(CheckResult());
  FlowQuantity total_flow = 0;
  for (StarGraph::OutgoingArcIterator arc_it(graph_, source_);
       arc_it.Ok();
       arc_it.Next()) {
    const ArcIndex arc = arc_it.Index();
    total_flow += Flow(arc);
  }
  return total_flow;
}

void MaxFlow::CompleteGraph() {
  for (StarGraph::ArcIterator arc_it(graph_); arc_it.Ok(); arc_it.Next()) {
    const ArcIndex arc = arc_it.Index();
    // The initial capacity on reverse arcs is set to 0.
    residual_arc_capacity_.Set(Opposite(arc), 0);
  }
}

void MaxFlow::ResetFirstAdmissibleArcs() {
  for (StarGraph::NodeIterator node_it(graph_); node_it.Ok(); node_it.Next()) {
    const NodeIndex node = node_it.Index();
    first_admissible_arc_.Set(node, GetFirstIncidentArc(node));
  }
}

void MaxFlow::InitializePreflow() {
  // The initial height of the source is equal to the number of nodes.
  node_potential_.Set(source_, graph_.num_nodes());
  for (StarGraph::OutgoingArcIterator arc_it(graph_, source_);
       arc_it.Ok();
       arc_it.Next()) {
    const ArcIndex arc = arc_it.Index();
    const FlowQuantity arc_capacity = residual_arc_capacity_[arc];
    // Saturate arcs outgoing from the source. This is not really a PushFlow,
    // since the preconditions for PushFlow are not (yet) met, and we do not
    // need to update the excess at the source.
    residual_arc_capacity_.Set(arc, 0);
    residual_arc_capacity_.Set(Opposite(arc), arc_capacity);
    node_excess_.Set(Head(arc), arc_capacity);
    VLOG(1) << DebugString("InitializePreflow:", arc);
  }
}

void MaxFlow::PushFlow(FlowQuantity flow, ArcIndex arc) {
  DCHECK_GT(residual_arc_capacity_[arc], 0);
  DCHECK_GT(node_excess_[Tail(arc)], 0);
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

void MaxFlow::InitializeActiveNodeStack() {
  CHECK(active_nodes_.empty());
  for (StarGraph::NodeIterator node_it(graph_); node_it.Ok(); node_it.Next()) {
    const NodeIndex node = node_it.Index();
    if (IsActive(node)) {
      active_nodes_.push(node);
      VLOG(1) << "InitializeActiveNodeStack: node " << node << " added.";
    }
  }
}

void MaxFlow::Refine() {
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

void MaxFlow::Discharge(NodeIndex node) {
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

void MaxFlow::Relabel(NodeIndex node) {
  DCHECK(CheckRelabelPrecondition(node));
  CostValue min_height = node_potential_[node];
  for (StarGraph::IncidentArcIterator arc_it(graph_, node);
       arc_it.Ok();
       arc_it.Next()) {
    const ArcIndex arc = arc_it.Index();
    DCHECK_EQ(Tail(arc), node);
    if (residual_arc_capacity_[arc] > 0) {
      // Update min_height only for arcs with available capacity.
      min_height = min(min_height, node_potential_[Head(arc)]);
    }
  }
  VLOG(1) << "Relabel: height(" << node << ") relabeled from "
          << node_potential_[node] << " to " << min_height + 1;
  node_potential_.Set(node, min_height + 1);
  first_admissible_arc_.Set(node, GetFirstIncidentArc(node));
}

}  // namespace operations_research

