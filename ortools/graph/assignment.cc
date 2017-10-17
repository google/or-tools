// Copyright 2010-2017 Google
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

#include "ortools/graph/assignment.h"

#include "ortools/base/commandlineflags.h"
#include "ortools/graph/ebert_graph.h"
#include "ortools/graph/linear_assignment.h"

namespace operations_research {

SimpleLinearSumAssignment::SimpleLinearSumAssignment() : num_nodes_(0) {}

ArcIndex SimpleLinearSumAssignment::AddArcWithCost(NodeIndex left_node,
                                                   NodeIndex righ_node,
                                                   CostValue cost) {
  const ArcIndex num_arcs = arc_cost_.size();
  num_nodes_ = std::max(num_nodes_, left_node + 1);
  num_nodes_ = std::max(num_nodes_, righ_node + 1);
  arc_tail_.push_back(left_node);
  arc_head_.push_back(righ_node);
  arc_cost_.push_back(cost);
  return num_arcs;
}

NodeIndex SimpleLinearSumAssignment::NumNodes() const { return num_nodes_; }

ArcIndex SimpleLinearSumAssignment::NumArcs() const { return arc_cost_.size(); }

NodeIndex SimpleLinearSumAssignment::LeftNode(ArcIndex arc) const {
  return arc_tail_[arc];
}

NodeIndex SimpleLinearSumAssignment::RightNode(ArcIndex arc) const {
  return arc_head_[arc];
}

CostValue SimpleLinearSumAssignment::Cost(ArcIndex arc) const {
  return arc_cost_[arc];
}

SimpleLinearSumAssignment::Status SimpleLinearSumAssignment::Solve() {
  optimal_cost_ = 0;
  assignment_arcs_.clear();
  if (NumNodes() == 0) return OPTIMAL;
  // HACK(user): Detect overflows early. In ./linear_assignment.h, the cost of
  // each arc is internally multiplied by cost_scaling_factor_ (which is equal
  // to (num_nodes + 1)) without overflow checking.
  const CostValue max_supported_arc_cost =
      std::numeric_limits<CostValue>::max() / (NumNodes() + 1);
  for (const CostValue unscaled_arc_cost : arc_cost_) {
    if (unscaled_arc_cost > max_supported_arc_cost) return POSSIBLE_OVERFLOW;
  }

  const ArcIndex num_arcs = arc_cost_.size();
  ForwardStarGraph graph(2 * num_nodes_, num_arcs);
  LinearSumAssignment<ForwardStarGraph> assignment(graph, num_nodes_);
  for (ArcIndex arc = 0; arc < num_arcs; ++arc) {
    graph.AddArc(arc_tail_[arc], num_nodes_ + arc_head_[arc]);
    assignment.SetArcCost(arc, arc_cost_[arc]);
  }
  // TODO(user): Improve the LinearSumAssignment api to clearly define
  // the error cases.
  if (!assignment.FinalizeSetup()) return POSSIBLE_OVERFLOW;
  if (!assignment.ComputeAssignment()) return INFEASIBLE;
  optimal_cost_ = assignment.GetCost();
  for (NodeIndex node = 0; node < num_nodes_; ++node) {
    assignment_arcs_.push_back(assignment.GetAssignmentArc(node));
  }
  return OPTIMAL;
}

}  // namespace operations_research
