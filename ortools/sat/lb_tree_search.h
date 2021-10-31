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

#ifndef OR_TOOLS_SAT_LB_TREE_SEARCH_H_
#define OR_TOOLS_SAT_LB_TREE_SEARCH_H_

#include <limits>
#include <vector>

#include "ortools/sat/integer.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"

namespace operations_research {
namespace sat {

// Implement a "classic" MIP tree search by having an exhaustive list of open
// nodes.
//
// The goal of this subsolver is to improve the objective lower bound. It is
// meant to be used in a multi-thread portfolio, and as such it really do not
// care about finding solution. It is all about improving the lower bound.
//
// TODO(user): What this is doing is really similar to asking a SAT solver if
// the current objective lower bound is reachable by solving a SAT problem.
// However, this code handle on the side all the "conflict" of the form
// objective > current_lb. As a result, when it is UNSAT, we can bump the lower
// bound by a bigger amount than one. We also do not completely loose everything
// learned so far for the next iteration.
class LbTreeSearch {
 public:
  explicit LbTreeSearch(Model* model);

  // Explores the search space.
  SatSolver::Status Search(
      const std::function<void()>& feasible_solution_observer);

 private:
  // Code a binary tree.
  DEFINE_INT_TYPE(NodeIndex, int);
  struct Node {
    Node(Literal l, IntegerValue lb)
        : literal(l), true_objective(lb), false_objective(lb) {}

    // The objective lower bound at this node.
    IntegerValue MinObjective() const {
      return std::min(true_objective, false_objective);
    }

    // Invariant: the objective bounds only increase.
    void UpdateObjective(IntegerValue v) {
      true_objective = std::max(true_objective, v);
      false_objective = std::max(false_objective, v);
    }
    void UpdateTrueObjective(IntegerValue v) {
      true_objective = std::max(true_objective, v);
    }
    void UpdateFalseObjective(IntegerValue v) {
      false_objective = std::max(false_objective, v);
    }

    // The decision for the true and false branch under this node.
    /*const*/ Literal literal;

    // The objective lower bound in both branches.
    IntegerValue true_objective;
    IntegerValue false_objective;

    // Points to adjacent nodes in the tree. Large if no connection.
    NodeIndex true_child = NodeIndex(std::numeric_limits<int32_t>::max());
    NodeIndex false_child = NodeIndex(std::numeric_limits<int32_t>::max());
  };

  // Display the current tree, this is mainly here to investigate ideas to
  // improve the code.
  void DebugDisplayTree(NodeIndex root) const;

  // Updates the objective of the node in the current branch at level n from
  // the one at level n - 1.
  void UpdateObjectiveFromParent(int level);

  // Updates the objective of the node in the current branch at level n - 1 from
  // the one at level n.
  void UpdateParentObjective(int level);

  // Model singleton class used here.
  TimeLimit* time_limit_;
  ModelRandomGenerator* random_;
  SatSolver* sat_solver_;
  IntegerEncoder* integer_encoder_;
  IntegerTrail* integer_trail_;
  SharedResponseManager* shared_response_;
  SatDecisionPolicy* sat_decision_;
  IntegerSearchHelper* search_helper_;
  IntegerVariable objective_var_;

  // This can stay null. Otherwise it will be the lp constraint with
  // objective_var_ as objective.
  LinearProgrammingConstraint* lp_constraint_ = nullptr;

  // We temporarily cache the shared_response_ objective lb here.
  IntegerValue current_objective_lb_;

  // Memory for all the nodes.
  absl::StrongVector<NodeIndex, Node> nodes_;

  // The list of nodes in the current branch, in order from the root.
  std::vector<NodeIndex> current_branch_;

  // Our heuristic used to explore the tree. See code for detail.
  std::function<BooleanOrIntegerLiteral()> search_heuristic_;

  int64_t num_rc_detected_ = 0;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_LB_TREE_SEARCH_H_
