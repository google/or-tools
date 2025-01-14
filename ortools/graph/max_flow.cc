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

#include "ortools/graph/max_flow.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "ortools/graph/generic_max_flow.h"
#include "ortools/graph/graph.h"

namespace operations_research {

SimpleMaxFlow::SimpleMaxFlow() : num_nodes_(0) {}

SimpleMaxFlow::ArcIndex SimpleMaxFlow::AddArcWithCapacity(
    NodeIndex tail, NodeIndex head, FlowQuantity capacity) {
  const ArcIndex num_arcs = arc_tail_.size();
  num_nodes_ = std::max(num_nodes_, tail + 1);
  num_nodes_ = std::max(num_nodes_, head + 1);
  arc_tail_.push_back(tail);
  arc_head_.push_back(head);
  arc_capacity_.push_back(capacity);
  return num_arcs;
}

SimpleMaxFlow::NodeIndex SimpleMaxFlow::NumNodes() const { return num_nodes_; }

SimpleMaxFlow::ArcIndex SimpleMaxFlow::NumArcs() const {
  return arc_tail_.size();
}

SimpleMaxFlow::NodeIndex SimpleMaxFlow::Tail(ArcIndex arc) const {
  return arc_tail_[arc];
}

SimpleMaxFlow::NodeIndex SimpleMaxFlow::Head(ArcIndex arc) const {
  return arc_head_[arc];
}

SimpleMaxFlow::FlowQuantity SimpleMaxFlow::Capacity(ArcIndex arc) const {
  return arc_capacity_[arc];
}

void SimpleMaxFlow::SetArcCapacity(ArcIndex arc, FlowQuantity capacity) {
  arc_capacity_[arc] = capacity;
}

SimpleMaxFlow::Status SimpleMaxFlow::Solve(NodeIndex source, NodeIndex sink) {
  const ArcIndex num_arcs = arc_capacity_.size();
  arc_flow_.assign(num_arcs, 0);
  underlying_max_flow_.reset();
  underlying_graph_.reset();
  optimal_flow_ = 0;
  if (source == sink || source < 0 || sink < 0) {
    return BAD_INPUT;
  }
  if (source >= num_nodes_ || sink >= num_nodes_) {
    return OPTIMAL;
  }
  underlying_graph_ = std::make_unique<Graph>(num_nodes_, num_arcs);
  underlying_graph_->AddNode(source);
  underlying_graph_->AddNode(sink);
  for (int arc = 0; arc < num_arcs; ++arc) {
    underlying_graph_->AddArc(arc_tail_[arc], arc_head_[arc]);
  }
  underlying_graph_->Build(&arc_permutation_);
  underlying_max_flow_ = std::make_unique<GenericMaxFlow<Graph>>(
      underlying_graph_.get(), source, sink);
  for (ArcIndex arc = 0; arc < num_arcs; ++arc) {
    ArcIndex permuted_arc =
        arc < arc_permutation_.size() ? arc_permutation_[arc] : arc;
    underlying_max_flow_->SetArcCapacity(permuted_arc, arc_capacity_[arc]);
  }
  if (underlying_max_flow_->Solve()) {
    optimal_flow_ = underlying_max_flow_->GetOptimalFlow();
    for (ArcIndex arc = 0; arc < num_arcs; ++arc) {
      ArcIndex permuted_arc =
          arc < arc_permutation_.size() ? arc_permutation_[arc] : arc;
      arc_flow_[arc] = underlying_max_flow_->Flow(permuted_arc);
    }
  }
  // Translate the GenericMaxFlow::Status. It is different because NOT_SOLVED
  // does not make sense in the simple api.
  switch (underlying_max_flow_->status()) {
    case GenericMaxFlow<Graph>::NOT_SOLVED:
      return BAD_RESULT;
    case GenericMaxFlow<Graph>::OPTIMAL:
      return OPTIMAL;
    case GenericMaxFlow<Graph>::INT_OVERFLOW:
      return POSSIBLE_OVERFLOW;
    case GenericMaxFlow<Graph>::BAD_INPUT:
      return BAD_INPUT;
    case GenericMaxFlow<Graph>::BAD_RESULT:
      return BAD_RESULT;
  }
  return BAD_RESULT;
}

SimpleMaxFlow::FlowQuantity SimpleMaxFlow::OptimalFlow() const {
  return optimal_flow_;
}

SimpleMaxFlow::FlowQuantity SimpleMaxFlow::Flow(ArcIndex arc) const {
  return arc_flow_[arc];
}

void SimpleMaxFlow::GetSourceSideMinCut(std::vector<NodeIndex>* result) {
  if (underlying_max_flow_ == nullptr) return;
  underlying_max_flow_->GetSourceSideMinCut(result);
}

void SimpleMaxFlow::GetSinkSideMinCut(std::vector<NodeIndex>* result) {
  if (underlying_max_flow_ == nullptr) return;
  underlying_max_flow_->GetSinkSideMinCut(result);
}

FlowModelProto SimpleMaxFlow::CreateFlowModelProto(NodeIndex source,
                                                   NodeIndex sink) const {
  FlowModelProto model;
  model.set_problem_type(FlowModelProto::MAX_FLOW);
  for (int n = 0; n < num_nodes_; ++n) {
    FlowNodeProto* node = model.add_nodes();
    node->set_id(n);
    if (n == source) node->set_supply(1);
    if (n == sink) node->set_supply(-1);
  }
  for (int a = 0; a < arc_tail_.size(); ++a) {
    FlowArcProto* arc = model.add_arcs();
    arc->set_tail(Tail(a));
    arc->set_head(Head(a));
    arc->set_capacity(Capacity(a));
  }
  return model;
}

}  // namespace operations_research
