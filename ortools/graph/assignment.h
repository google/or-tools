// Copyright 2010-2014 Google
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

// Simple interface to solve the linear sum assignment problem. It
// uses about twice as much memory as directly using the
// LinearSumAssignment class template, but it is as fast and presents
// a simpler interface. This is the class you should use in most
// situations.
//
// The assignment problem: Given N "left" nodes and N "right" nodes,
// and a set of left->right arcs with integer costs, find a perfect
// matching (i.e., each "left" node is assigned to one "right" node)
// that minimizes the overall cost.
//
// Example usage:
//
// #include "ortools/graph/assignment.h"
//
// SimpleLinearSumAssignment assignment;
// for (int arc = 0; arc < num_arcs; ++arc) {
//   assignment.AddArcWithCost(head(arc), tail(arc), cost(arc));
// }
// if (assignment.Solve() == SimpleLinearSumAssignment::OPTIMAL) {
//   printf("A perfect matching exists.\n");
//   printf("The best possible cost is %d.\n", assignment.OptimalCost());
//   printf("An optimal assignment is:\n");
//   for (int node = 0; node < assignment.NumNodes(); ++node) {
//     printf("left node %d assigned to right node %d with cost %d.\n",
//         node,
//         assignment.RightMate(node),
//         assignment.AssignmentCost(node));
//   }
//   printf("Note that it may not be the unique optimal assignment.");
// } else {
//   printf("There is an issue with the input or no perfect matching exists.");
// }

#ifndef OR_TOOLS_GRAPH_ASSIGNMENT_H_
#define OR_TOOLS_GRAPH_ASSIGNMENT_H_

#include <vector>

#include "ortools/graph/ebert_graph.h"

namespace operations_research {

class SimpleLinearSumAssignment {
 public:
  // The constructor takes no size.
  // New node indices will be created lazily by AddArcWithCost().
  SimpleLinearSumAssignment();

  // Adds an arc from a left node to a right node with a given cost.
  // * Node indices must be non-negative (>= 0). For a perfect
  //   matching to exist on n nodes, the values taken by "left_node"
  //   must cover [0, n), same for "right_node".
  // * The arc cost can be any integer, negative, positive or zero.
  // * After the method finishes, NumArcs() == the returned ArcIndex + 1.
  ArcIndex AddArcWithCost(NodeIndex left_node, NodeIndex right_node,
                          CostValue cost);

  // Returns the current number of left nodes which is the same as the
  // number of right nodes. This is one greater than the largest node
  // index seen so far in AddArcWithCost().
  NodeIndex NumNodes() const;

  // Returns the current number of arcs in the graph.
  ArcIndex NumArcs() const;

  // Returns user-provided data.
  // The implementation will crash if "arc" is not in [0, NumArcs()).
  NodeIndex LeftNode(ArcIndex arc) const;
  NodeIndex RightNode(ArcIndex arc) const;
  CostValue Cost(ArcIndex arc) const;

  // Solves the problem (finds the perfect matching that minimizes the
  // cost) and returns the solver status.
  enum Status {
    OPTIMAL,            // The algorithm found a minimum-cost perfect matching.
    INFEASIBLE,         // The given problem admits no perfect matching.
    POSSIBLE_OVERFLOW,  // Some cost magnitude is too large.
  };
  Status Solve();

  // Returns the cost of an assignment with minimal cost.
  // This is 0 if the last Solve() didn't return OPTIMAL.
  CostValue OptimalCost() const { return optimal_cost_; }

  // Returns the right node assigned to the given left node in the
  // last solution computed by Solve(). This works only if Solve()
  // returned OPTIMAL.
  //
  // Note: It is possible that there is more than one optimal
  // solution. The algorithm is deterministic so it will always return
  // the same solution for a given problem. There is no such guarantee
  // from one code version to the next, but the code does not change
  // often.
  NodeIndex RightMate(NodeIndex left_node) const {
    return arc_head_[assignment_arcs_[left_node]];
  }

  // Returns the cost of the arc used for "left_node"'s assignment.
  // This works only if Solve() returned OPTIMAL.
  CostValue AssignmentCost(NodeIndex left_node) const {
    return arc_cost_[assignment_arcs_[left_node]];
  }

 private:
  NodeIndex num_nodes_;
  std::vector<NodeIndex> arc_tail_;
  std::vector<NodeIndex> arc_head_;
  std::vector<CostValue> arc_cost_;
  std::vector<ArcIndex> assignment_arcs_;
  CostValue optimal_cost_;
  DISALLOW_COPY_AND_ASSIGN(SimpleLinearSumAssignment);
};

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_ASSIGNMENT_H_
