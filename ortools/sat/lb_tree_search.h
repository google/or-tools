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

#ifndef OR_TOOLS_SAT_LB_TREE_SEARCH_H_
#define OR_TOOLS_SAT_LB_TREE_SEARCH_H_

#include <stdint.h>

#include <algorithm>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/glop/variables_info.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/pseudo_costs.h"
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
    explicit Node(IntegerValue lb) : true_objective(lb), false_objective(lb) {}

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

    // Should be called only once.
    void SetDecision(Literal l) {
      DCHECK(!is_deleted);
      DCHECK_EQ(literal_index, kNoLiteralIndex);
      literal_index = l.Index();
    }

    Literal Decision() const {
      DCHECK(!is_deleted);
      DCHECK_NE(literal_index, kNoLiteralIndex);
      return sat::Literal(literal_index);
    }

    bool IsLeaf() const { return literal_index == kNoLiteralIndex; }

    // The decision for the true and false branch under this node.
    // Initially this is kNoLiteralIndex until SetDecision() is called.
    LiteralIndex literal_index = kNoLiteralIndex;

    // The objective lower bound in both branches.
    IntegerValue true_objective;
    IntegerValue false_objective;

    // Points to adjacent nodes in the tree. Large if no connection.
    NodeIndex true_child = NodeIndex(std::numeric_limits<int32_t>::max());
    NodeIndex false_child = NodeIndex(std::numeric_limits<int32_t>::max());

    // Indicates if this nodes was removed from the tree.
    bool is_deleted = false;

    // Experimental. Store the optimal basis at each node.
    int64_t basis_timestamp;
    glop::BasisState basis;
  };

  // Regroup some logic done when we are back at level zero in Search().
  // Returns false if UNSAT.
  bool LevelZeroLogic();

  // Returns true if we save/load LP basis.
  // Note that when this is true we also do not solve the LP as often.
  bool SaveLpBasisOption() const {
    return lp_constraint_ != nullptr &&
           parameters_.save_lp_basis_in_lb_tree_search();
  }

  // Display the current tree, this is mainly here to investigate ideas to
  // improve the code.
  std::string NodeDebugString(NodeIndex node) const;
  void DebugDisplayTree(NodeIndex root) const;

  // Updates the objective of the node in the current branch at level n from
  // the one at level n - 1.
  void UpdateObjectiveFromParent(int level);

  // Updates the objective of the node in the current branch at level n - 1 from
  // the one at level n.
  void UpdateParentObjective(int level);

  // Returns false on conflict.
  bool FullRestart();

  // Loads any known basis that is the closest to the current branch.
  void EnableLpAndLoadBestBasis();
  void SaveLpBasisInto(Node& node);
  bool NodeHasUpToDateBasis(const Node& node) const;
  bool NodeHasBasis(const Node& node) const;

  // Mark the given node as deleted. Its literal is assumed to be set. We also
  // delete the subtree that is not longer relevant.
  void MarkAsDeletedNodeAndUnreachableSubtree(Node& node);
  void MarkBranchAsInfeasible(Node& node, bool true_branch);
  void MarkSubtreeAsDeleted(NodeIndex root);

  // Create a new node at the end of the current branch.
  // This assume the last decision in the branch is assigned.
  NodeIndex CreateNewEmptyNodeIfNeeded();
  void AppendNewNodeToCurrentBranch(Literal decision);

  // Update the bounds on the given nodes by using reduced costs if possible.
  void ExploitReducedCosts(NodeIndex n);

  // Returns a small number of decision needed to reach the same conflict.
  // We basically reduce the number of decision at each level to 1.
  std::vector<Literal> ExtractDecisions(int base_level,
                                        absl::Span<const Literal> conflict);

  // Used in the solve logs.
  std::string SmallProgressString() const;

  // Save the current number of iterations on creation and add the difference
  // to the counter when the returned function is called. This is meant to
  // be used with:
  // const auto cleanup = absl::MakeCleanup(UpdateLpIters(&counter));
  std::function<void()> UpdateLpIters(int64_t* counter);

  // Model singleton class used here.
  const std::string name_;
  TimeLimit* time_limit_;
  ModelRandomGenerator* random_;
  SatSolver* sat_solver_;
  IntegerEncoder* integer_encoder_;
  Trail* trail_;
  const VariablesAssignment& assignment_;
  IntegerTrail* integer_trail_;
  GenericLiteralWatcher* watcher_;
  SharedResponseManager* shared_response_;
  PseudoCosts* pseudo_costs_;
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
  util_intops::StrongVector<NodeIndex, Node> nodes_;

  // The list of nodes in the current branch, in order from the root.
  std::vector<NodeIndex> current_branch_;

  // Our heuristic used to explore the tree. See code for detail.
  std::function<BooleanOrIntegerLiteral()> search_heuristic_;

  int64_t num_rc_detected_ = 0;

  // Counts the number of decisions we are taking while exploring the search
  // tree.
  int64_t num_decisions_taken_ = 0;

  // Counts number of lp iterations at various places.
  int64_t num_lp_iters_at_level_zero_ = 0;
  int64_t num_lp_iters_save_basis_ = 0;
  int64_t num_lp_iters_first_branch_ = 0;
  int64_t num_lp_iters_dive_ = 0;

  // Used to trigger the initial restarts and imports.
  int num_full_restarts_ = 0;
  int64_t num_decisions_taken_at_last_restart_ = 0;
  int64_t num_decisions_taken_at_last_level_zero_ = 0;

  // Count the number of time we are back to decision level zero.
  int64_t num_back_to_root_node_ = 0;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_LB_TREE_SEARCH_H_
