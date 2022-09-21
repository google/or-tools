// Copyright 2010-2022 Google LLC
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

#include <stdint.h>

#include <algorithm>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_decision.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

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
  DEFINE_STRONG_INDEX_TYPE(NodeIndex);
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

    // Indicates if this nodes was removed from the tree.
    bool is_deleted = false;
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

  // Returns false on conflict.
  bool FullRestart();

  // Mark the given node as deleted. Its literal is assumed to be set. We also
  // delete the subtree that is not longer relevant.
  void MarkAsDeletedNodeAndUnreachableSubtree(Node& node);
  void MarkSubtreeAsDeleted(NodeIndex root);

  // Create a new node at the end of the current branch.
  // This assume the last decision in the branch is assigned.
  void AppendNewNodeToCurrentBranch(Literal decision);

  // Update the bounds on the given nodes by using reduced costs if possible.
  void ExploitReducedCosts(NodeIndex n);

  // Returns a small number of decision needed to reach the same conflict.
  // We basically reduce the number of decision at each level to 1.
  std::vector<Literal> ExtractDecisions(int base_level,
                                        const std::vector<Literal>& conflict);

  // Used in the solve logs.
  std::string SmallProgressString() const;

  // Model singleton class used here.
  TimeLimit* time_limit_;
  ModelRandomGenerator* random_;
  SatSolver* sat_solver_;
  IntegerEncoder* integer_encoder_;
  Trail* trail_;
  IntegerTrail* integer_trail_;
  GenericLiteralWatcher* watcher_;
  SharedResponseManager* shared_response_;
  SatDecisionPolicy* sat_decision_;
  IntegerSearchHelper* search_helper_;
  IntegerVariable objective_var_;
  const SatParameters& parameters_;

  // This can stay null. Otherwise it will be the lp constraint with
  // objective_var_ as objective.
  LinearProgrammingConstraint* lp_constraint_ = nullptr;

  // We temporarily cache the shared_response_ objective lb here.
  IntegerValue current_objective_lb_;

  // Memory for all the nodes.
  int num_nodes_in_tree_ = 0;
  absl::StrongVector<NodeIndex, Node> nodes_;

  // The list of nodes in the current branch, in order from the root.
  std::vector<NodeIndex> current_branch_;

  // Our heuristic used to explore the tree. See code for detail.
  std::function<BooleanOrIntegerLiteral()> search_heuristic_;

  int64_t num_rc_detected_ = 0;

  // Counts the number of decisions we are taking while exploring the search
  // tree.
  int64_t num_decisions_taken_ = 0;

  // Used to trigger the initial restarts and imports.
  int num_full_restarts_ = 0;
  int64_t num_decisions_taken_at_last_restart_ = 0;
  int64_t num_decisions_taken_at_last_level_zero_ = 0;

  // Count the number of time we are back to decision level zero.
  int64_t num_back_to_root_node_ = 0;

  // Used to display periodic info to the log.
  absl::Time last_logging_time_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_LB_TREE_SEARCH_H_
