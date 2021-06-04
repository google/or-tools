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
  struct Node {
    Node(Literal l, IntegerValue lb)
        : literal(l),
          objective_lb(lb),
          true_objective(lb),
          false_objective(lb) {}

    // Invariant: the objective bounds only increase.
    void UpdateObjective() {
      objective_lb =
          std::max(objective_lb, std::min(true_objective, false_objective));
    }
    void UpdateTrueObjective(IntegerValue v) {
      true_objective = std::max(true_objective, v);
      UpdateObjective();
    }
    void UpdateFalseObjective(IntegerValue v) {
      false_objective = std::max(false_objective, v);
      UpdateObjective();
    }

    // The decision for the true and false branch under this node.
    /*const*/ Literal literal;

    // The objective lower bound at this node.
    IntegerValue objective_lb;

    // The objective lower bound in both branches.
    // This is only updated when we backtrack over this node.
    IntegerValue true_objective;
    IntegerValue false_objective;

    // Points to adjacent nodes in the tree. Large if no connection.
    int true_child = std::numeric_limits<int32_t>::max();
    int false_child = std::numeric_limits<int32_t>::max();

    // Instead of storing the full reason for an objective LB increase in one
    // the branches (which can lead to a quadratic memory usage), we stores the
    // level of the highest decision needed, not counting this node literal.
    // Basically, the reason for true_objective without {literal} only includes
    // literal with levels in [0, true_level].
    //
    // This allows us to slighlty reduce the size of the overall tree. If both
    // branches have a low enough level, then we can backjump in the search tree
    // and skip all the nodes in-between by connecting directly the correct
    // ancestor to this node. Note that when we do that, the level of the nodes
    // in the sub-branch change, but this still work.
    int true_level = std::numeric_limits<int32_t>::max();
    int false_level = std::numeric_limits<int32_t>::max();
  };

  // Returns true if this node objective lb is greater than the root level
  // objective lower bound.
  bool NodeImprovesLowerBound(const LbTreeSearch::Node& node);

  // Model singleton class used here.
  TimeLimit* time_limit_;
  ModelRandomGenerator* random_;
  SatSolver* sat_solver_;
  IntegerTrail* integer_trail_;
  SharedResponseManager* shared_response_;
  SatDecisionPolicy* sat_decision_;
  IntegerSearchHelper* search_helper_;
  IntegerVariable objective_var_;

  // Memory for all the nodes.
  std::vector<Node> nodes_;

  // The list of nodes in the current branch, in order from the root.
  std::vector<int> current_branch_;

  // Our heuristic used to explore the tree. See code for detail.
  std::function<BooleanOrIntegerLiteral()> search_heuristic_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_LB_TREE_SEARCH_H_
