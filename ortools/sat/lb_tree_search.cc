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

#include "ortools/sat/lb_tree_search.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/integer_expr.h"
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

LbTreeSearch::LbTreeSearch(Model* model)
    : name_(model->Name()),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      random_(model->GetOrCreate<ModelRandomGenerator>()),
      sat_solver_(model->GetOrCreate<SatSolver>()),
      integer_encoder_(model->GetOrCreate<IntegerEncoder>()),
      trail_(model->GetOrCreate<Trail>()),
      assignment_(trail_->Assignment()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      watcher_(model->GetOrCreate<GenericLiteralWatcher>()),
      shared_response_(model->GetOrCreate<SharedResponseManager>()),
      pseudo_costs_(model->GetOrCreate<PseudoCosts>()),
      sat_decision_(model->GetOrCreate<SatDecisionPolicy>()),
      search_helper_(model->GetOrCreate<IntegerSearchHelper>()),
      parameters_(*model->GetOrCreate<SatParameters>()) {
  // We should create this class only in the presence of an objective.
  //
  // TODO(user): Starts with an initial variable score for all variable in
  // the objective at their minimum value? this should emulate the first step of
  // the core approach and gives a similar bound.
  const ObjectiveDefinition* objective = model->Get<ObjectiveDefinition>();
  DCHECK(objective != nullptr);
  objective_var_ = objective->objective_var;

  // Identify an LP with the same objective variable.
  //
  // TODO(user): if we have many independent LP, this will find nothing.
  for (LinearProgrammingConstraint* lp :
       *model->GetOrCreate<LinearProgrammingConstraintCollection>()) {
    if (lp->ObjectiveVariable() == objective_var_) {
      lp_constraint_ = lp;
    }
  }

  // We use the normal SAT search but we will bump the variable activity
  // slightly differently. In addition to the conflicts, we also bump it each
  // time the objective lower bound increase in a sub-node.
  std::vector<std::function<BooleanOrIntegerLiteral()>> heuristics;
  if (SaveLpBasisOption()) {
    heuristics.emplace_back(LpPseudoCostHeuristic(model));
  }
  heuristics.emplace_back(SatSolverHeuristic(model));
  heuristics.emplace_back(MostFractionalHeuristic(model));
  heuristics.emplace_back(IntegerValueSelectionHeuristic(
      model->GetOrCreate<SearchHeuristics>()->fixed_search, model));
  search_heuristic_ = SequentialSearch(std::move(heuristics));
}

bool LbTreeSearch::NodeHasBasis(const Node& node) const {
  return !node.basis.IsEmpty();
}

bool LbTreeSearch::NodeHasUpToDateBasis(const Node& node) const {
  if (node.basis.IsEmpty()) return false;

  // TODO(user): Do something smarter. We can at least reuse the variable
  // statuses maybe?
  if (node.basis_timestamp != lp_constraint_->num_lp_changes()) {
    return false;
  }
  return true;
}

void LbTreeSearch::EnableLpAndLoadBestBasis() {
  DCHECK(lp_constraint_ != nullptr);
  lp_constraint_->EnablePropagation(true);

  const int level = trail_->CurrentDecisionLevel();
  if (current_branch_.empty()) return;

  NodeIndex n = current_branch_[0];  // Root.
  int basis_level = -1;
  NodeIndex last_node_with_basis(-1);
  for (int i = 0; i < level; ++i) {
    if (n >= nodes_.size()) break;
    if (NodeHasBasis(nodes_[n])) {
      basis_level = i;
      last_node_with_basis = n;
    }
    const Literal decision = trail_->Decisions()[i].literal;
    if (nodes_[n].literal_index == decision.Index()) {
      n = nodes_[n].true_child;
    } else {
      DCHECK_EQ(nodes_[n].literal_index, decision.NegatedIndex());
      n = nodes_[n].false_child;
    }
  }
  if (n < nodes_.size()) {
    if (NodeHasBasis(nodes_[n])) {
      basis_level = level;
      last_node_with_basis = n;
    }
  }

  if (last_node_with_basis == -1) {
    VLOG(1) << "no basis?";
    return;
  }
  VLOG(1) << "load " << basis_level << " / " << level;

  if (!NodeHasUpToDateBasis(nodes_[last_node_with_basis])) {
    // The basis is no longer up to date, for now we do not load it.
    // TODO(user): try to do something about it.
    VLOG(1) << "Skipping potentially bad basis.";
    return;
  }

  lp_constraint_->LoadBasisState(nodes_[last_node_with_basis].basis);
}

void LbTreeSearch::SaveLpBasisInto(Node& node) {
  node.basis_timestamp = lp_constraint_->num_lp_changes();
  node.basis = lp_constraint_->GetBasisState();
}

void LbTreeSearch::UpdateParentObjective(int level) {
  DCHECK_GE(level, 0);
  DCHECK_LT(level, current_branch_.size());
  if (level == 0) return;
  const NodeIndex parent_index = current_branch_[level - 1];
  Node& parent = nodes_[parent_index];
  const NodeIndex child_index = current_branch_[level];
  const Node& child = nodes_[child_index];
  if (parent.true_child == child_index) {
    parent.UpdateTrueObjective(child.MinObjective());
  } else {
    DCHECK_EQ(parent.false_child, child_index);
    parent.UpdateFalseObjective(child.MinObjective());
  }
}

void LbTreeSearch::UpdateObjectiveFromParent(int level) {
  DCHECK_GE(level, 0);
  DCHECK_LT(level, current_branch_.size());
  if (level == 0) return;
  const NodeIndex parent_index = current_branch_[level - 1];
  const Node& parent = nodes_[parent_index];
  DCHECK_GE(parent.MinObjective(), current_objective_lb_);
  const NodeIndex child_index = current_branch_[level];
  Node& child = nodes_[child_index];
  if (parent.true_child == child_index) {
    child.UpdateObjective(parent.true_objective);
  } else {
    DCHECK_EQ(parent.false_child, child_index);
    child.UpdateObjective(parent.false_objective);
  }
}

std::string LbTreeSearch::NodeDebugString(NodeIndex n) const {
  const IntegerValue root_lb = current_objective_lb_;
  const auto shifted_lb = [root_lb](IntegerValue lb) {
    return std::max<int64_t>(0, (lb - root_lb).value());
  };

  std::string s;
  absl::StrAppend(&s, "#", n.value());

  const Node& node = nodes_[n];
  std::string true_letter = "t";
  std::string false_letter = "f";
  if (node.literal_index != kNoLiteralIndex && !node.is_deleted) {
    const Literal decision = node.Decision();
    if (assignment_.LiteralIsTrue(decision)) {
      true_letter = "T";
    }
    if (assignment_.LiteralIsFalse(decision)) {
      false_letter = "F";
    }
  }

  if (node.true_child < nodes_.size()) {
    absl::StrAppend(&s, " [", true_letter, ":#", node.true_child.value(), " ",
                    shifted_lb(node.true_objective), "]");
  } else {
    absl::StrAppend(&s, " [", true_letter, ":## ",
                    shifted_lb(node.true_objective), "]");
  }
  if (node.false_child < nodes_.size()) {
    absl::StrAppend(&s, " [", false_letter, ":#", node.false_child.value(), " ",
                    shifted_lb(node.false_objective), "]");
  } else {
    absl::StrAppend(&s, " [", false_letter, ":## ",
                    shifted_lb(node.false_objective), "]");
  }

  if (node.is_deleted) {
    absl::StrAppend(&s, " <D>");
  }
  if (NodeHasBasis(node)) {
    absl::StrAppend(&s, " <B>");
  }

  return s;
}

void LbTreeSearch::DebugDisplayTree(NodeIndex root) const {
  int num_nodes = 0;

  util_intops::StrongVector<NodeIndex, int> level(nodes_.size(), 0);
  std::vector<NodeIndex> to_explore = {root};
  while (!to_explore.empty()) {
    NodeIndex n = to_explore.back();
    to_explore.pop_back();

    ++num_nodes;
    const Node& node = nodes_[n];

    std::string s(level[n], ' ');
    absl::StrAppend(&s, NodeDebugString(n));
    LOG(INFO) << s;

    if (node.true_child < nodes_.size()) {
      to_explore.push_back(node.true_child);
      level[node.true_child] = level[n] + 1;
    }
    if (node.false_child < nodes_.size()) {
      to_explore.push_back(node.false_child);
      level[node.false_child] = level[n] + 1;
    }
  }
  LOG(INFO) << "num_nodes: " << num_nodes;
}

// Here we forgot the whole search tree and restart.
//
// The idea is that the heuristic has now more information so it will likely
// take better decision which will result in a smaller overall tree.
bool LbTreeSearch::FullRestart() {
  ++num_full_restarts_;
  num_decisions_taken_at_last_restart_ = num_decisions_taken_;
  num_nodes_in_tree_ = 0;
  nodes_.clear();
  current_branch_.clear();
  return sat_solver_->ResetToLevelZero();
}

void LbTreeSearch::MarkAsDeletedNodeAndUnreachableSubtree(Node& node) {
  --num_nodes_in_tree_;
  DCHECK(!node.is_deleted);
  node.is_deleted = true;
  DCHECK_NE(node.literal_index, kNoLiteralIndex);
  if (assignment_.LiteralIsTrue(Literal(node.literal_index))) {
    MarkSubtreeAsDeleted(node.false_child);
  } else {
    MarkSubtreeAsDeleted(node.true_child);
  }
}

void LbTreeSearch::MarkBranchAsInfeasible(Node& node, bool true_branch) {
  if (true_branch) {
    node.UpdateTrueObjective(kMaxIntegerValue);
    MarkSubtreeAsDeleted(node.true_child);
    node.true_child = NodeIndex(std::numeric_limits<int32_t>::max());
  } else {
    node.UpdateFalseObjective(kMaxIntegerValue);
    MarkSubtreeAsDeleted(node.false_child);
    node.false_child = NodeIndex(std::numeric_limits<int32_t>::max());
  }
}

void LbTreeSearch::MarkSubtreeAsDeleted(NodeIndex root) {
  std::vector<NodeIndex> to_delete{root};
  for (int i = 0; i < to_delete.size(); ++i) {
    const NodeIndex n = to_delete[i];
    if (n >= nodes_.size()) continue;
    if (nodes_[n].is_deleted) continue;

    --num_nodes_in_tree_;
    nodes_[n].is_deleted = true;

    to_delete.push_back(nodes_[n].true_child);
    to_delete.push_back(nodes_[n].false_child);
  }
}

std::string LbTreeSearch::SmallProgressString() const {
  return absl::StrCat(
      "nodes=", num_nodes_in_tree_, "/", nodes_.size(),
      " rc=", num_rc_detected_, " decisions=", num_decisions_taken_,
      " @root=", num_back_to_root_node_, " restarts=", num_full_restarts_,
      " lp_iters=[", FormatCounter(num_lp_iters_at_level_zero_), ", ",
      FormatCounter(num_lp_iters_save_basis_), ", ",
      FormatCounter(num_lp_iters_first_branch_), ", ",
      FormatCounter(num_lp_iters_dive_), "]");
}

std::function<void()> LbTreeSearch::UpdateLpIters(int64_t* counter) {
  if (lp_constraint_ == nullptr) return []() {};
  const int64_t old_num = lp_constraint_->total_num_simplex_iterations();
  return [old_num, counter, this]() {
    const int64_t new_num = lp_constraint_->total_num_simplex_iterations();
    *counter += new_num - old_num;
  };
}

bool LbTreeSearch::LevelZeroLogic() {
  ++num_back_to_root_node_;
  num_decisions_taken_at_last_level_zero_ = num_decisions_taken_;

  // Always run the LP when we are back at level zero.
  if (SaveLpBasisOption() && !current_branch_.empty()) {
    const auto cleanup =
        absl::MakeCleanup(UpdateLpIters(&num_lp_iters_at_level_zero_));
    EnableLpAndLoadBestBasis();
    if (!sat_solver_->FinishPropagation()) {
      return false;
    }
    SaveLpBasisInto(nodes_[current_branch_[0]]);
    lp_constraint_->EnablePropagation(false);
  }

  // Import the objective upper-bound.
  // We do that manually because we disabled objective import to not "pollute"
  // the objective lower_bound and still have local reason for objective
  // improvement.
  {
    const IntegerValue ub = shared_response_->GetInnerObjectiveUpperBound();
    if (integer_trail_->UpperBound(objective_var_) > ub) {
      if (!integer_trail_->Enqueue(
              IntegerLiteral::LowerOrEqual(objective_var_, ub), {}, {})) {
        sat_solver_->NotifyThatModelIsUnsat();
        return false;
      }
      if (!sat_solver_->FinishPropagation()) {
        return false;
      }
    }
  }

  // If the search has not just been restarted (in which case nodes_ would be
  // empty), and if we are at level zero (either naturally, or if the
  // backtrack level was set to zero in the above code), let's run a different
  // heuristic to decide whether to restart the search from scratch or not.
  //
  // We ignore small search trees.
  if (num_nodes_in_tree_ > 50) {
    // Let's count how many nodes have worse objective bounds than the best
    // known external objective lower bound.
    const IntegerValue latest_lb =
        shared_response_->GetInnerObjectiveLowerBound();
    int num_nodes = 0;
    int num_nodes_with_lower_objective = 0;
    for (const Node& node : nodes_) {
      if (node.is_deleted) continue;
      ++num_nodes;
      if (node.MinObjective() < latest_lb) num_nodes_with_lower_objective++;
    }
    DCHECK_EQ(num_nodes_in_tree_, num_nodes);
    if (num_nodes_with_lower_objective * 2 > num_nodes) {
      VLOG(2) << "lb_tree_search restart nodes: "
              << num_nodes_with_lower_objective << "/" << num_nodes << " : "
              << 100.0 * num_nodes_with_lower_objective / num_nodes << "%"
              << ", decisions:" << num_decisions_taken_;
      if (!FullRestart()) return false;
    }
  }

  return true;
}

SatSolver::Status LbTreeSearch::Search(
    const std::function<void()>& feasible_solution_observer) {
  if (!sat_solver_->ResetToLevelZero()) {
    return sat_solver_->UnsatStatus();
  }

  // We currently restart the search tree from scratch from time to times:
  // - Initially, every kNumDecisionsBeforeInitialRestarts, for at most
  //   kMaxNumInitialRestarts times.
  // - Every time we backtrack to level zero, we count how many nodes are worse
  //   than the best known objective lower bound. If this is true for more than
  //   half of the existing nodes, we restart and clear all nodes. If if this
  //   happens during the initial restarts phase, it reset the above counter and
  //   uses 1 of the available initial restarts.
  //
  // This has 2 advantages:
  //   - It allows our "pseudo-cost" to kick in and experimentally result in
  //     smaller trees down the road.
  //   - It removes large inefficient search trees.
  //
  // TODO(user): a strong branching initial start, or allowing a few decision
  // per nodes might be a better approach.
  //
  // TODO(user): It would also be cool to exploit the reason for the LB increase
  // even more.
  const int kMaxNumInitialRestarts = 10;
  const int64_t kNumDecisionsBeforeInitialRestarts = 1000;

  // If some branches already have a good lower bound, no need to call the LP
  // on those.
  watcher_->SetStopPropagationCallback([this] {
    return integer_trail_->LowerBound(objective_var_) > current_objective_lb_;
  });

  while (!time_limit_->LimitReached() && !shared_response_->ProblemIsSolved()) {
    VLOG(2) << "LOOP " << sat_solver_->CurrentDecisionLevel();

    // Each time we are back here, we bump the activities of the variable that
    // are part of the objective lower bound reason.
    //
    // Note that this is why we prefer not to increase the lower zero lower
    // bound of objective_var_ with the tree root lower bound, so we can exploit
    // more reasons.
    //
    // TODO(user): This is slightly different than bumping each time we
    // push a decision that result in an LB increase. This is also called on
    // backjump for instance.
    const IntegerValue obj_diff =
        integer_trail_->LowerBound(objective_var_) -
        integer_trail_->LevelZeroLowerBound(objective_var_);
    if (obj_diff > 0) {
      std::vector<Literal> reason =
          integer_trail_->ReasonFor(IntegerLiteral::GreaterOrEqual(
              objective_var_, integer_trail_->LowerBound(objective_var_)));

      // TODO(user): We also need to update pseudo cost on conflict.
      pseudo_costs_->UpdateBoolPseudoCosts(reason, obj_diff);
      sat_decision_->BumpVariableActivities(reason);
      sat_decision_->UpdateVariableActivityIncrement();
    }

    // Propagate upward in the tree the new objective lb.
    if (!current_branch_.empty()) {
      // Our branch is always greater or equal to the level.
      // We increase the objective_lb of the current node if needed.
      {
        const int current_level = sat_solver_->CurrentDecisionLevel();
        const IntegerValue current_objective_lb =
            integer_trail_->LowerBound(objective_var_);
        if (DEBUG_MODE) {
          CHECK_LE(current_level, current_branch_.size());
          for (int i = 0; i < current_level; ++i) {
            CHECK(!nodes_[current_branch_[i]].is_deleted);
            CHECK(assignment_.LiteralIsAssigned(
                nodes_[current_branch_[i]].Decision()));
          }
        }
        if (current_level < current_branch_.size()) {
          nodes_[current_branch_[current_level]].UpdateObjective(
              current_objective_lb);
        }

        // Minor optim: sometimes, because of the LP and cuts, the reason for
        // objective_var_ only contains lower level literals, so we can exploit
        // that.
        //
        // TODO(user): No point checking that if the objective lb wasn't
        // assigned at this level.
        //
        // TODO(user): Exploit the reasons further.
        if (current_objective_lb >
            integer_trail_->LevelZeroLowerBound(objective_var_)) {
          const std::vector<Literal> reason =
              integer_trail_->ReasonFor(IntegerLiteral::GreaterOrEqual(
                  objective_var_, current_objective_lb));
          int max_level = 0;
          for (const Literal l : reason) {
            max_level = std::max<int>(
                max_level,
                sat_solver_->LiteralTrail().Info(l.Variable()).level);
          }
          if (max_level < current_level) {
            nodes_[current_branch_[max_level]].UpdateObjective(
                current_objective_lb);
          }
        }
      }

      // Propagate upward any new bounds.
      for (int level = current_branch_.size(); --level > 0;) {
        UpdateParentObjective(level);
      }
    }

    if (SaveLpBasisOption()) {
      // We disable LP automatic propagation and only enable it:
      // - at root node
      // - when we go to a new branch.
      lp_constraint_->EnablePropagation(false);
    }
    if (sat_solver_->ModelIsUnsat()) return sat_solver_->UnsatStatus();

    // This will import other workers bound if we are back to level zero.
    // It might also decide to restart.
    if (!search_helper_->BeforeTakingDecision()) {
      return sat_solver_->UnsatStatus();
    }

    // This is the current bound we try to improve. We cache it here to avoid
    // getting the lock many times and it is also easier to follow the code if
    // this is assumed constant for one iteration.
    current_objective_lb_ = shared_response_->GetInnerObjectiveLowerBound();
    if (!current_branch_.empty()) {
      nodes_[current_branch_[0]].UpdateObjective(current_objective_lb_);
      for (int i = 1; i < current_branch_.size(); ++i) {
        UpdateObjectiveFromParent(i);
      }

      // If the root lb increased, update global shared objective lb.
      const IntegerValue bound = nodes_[current_branch_[0]].MinObjective();
      if (bound > current_objective_lb_) {
        shared_response_->UpdateInnerObjectiveBounds(
            absl::StrCat(name_, " (", SmallProgressString(), ") "), bound,
            integer_trail_->LevelZeroUpperBound(objective_var_));
        current_objective_lb_ = bound;
        if (VLOG_IS_ON(3)) DebugDisplayTree(current_branch_[0]);
      }
    }

    // Forget the whole tree and restart.
    // We will do it periodically at the beginning of the search each time we
    // cross the kNumDecisionsBeforeInitialRestarts decision since the last
    // restart. This will happen at most kMaxNumInitialRestarts times.
    if (num_decisions_taken_ >= num_decisions_taken_at_last_restart_ +
                                    kNumDecisionsBeforeInitialRestarts &&
        num_full_restarts_ < kMaxNumInitialRestarts) {
      VLOG(2) << "lb_tree_search (initial_restart " << SmallProgressString()
              << ")";
      if (!FullRestart()) return sat_solver_->UnsatStatus();
      continue;
    }

    // Periodic backtrack to level zero so we can import bounds.
    if (num_decisions_taken_ >=
        num_decisions_taken_at_last_level_zero_ + 10000) {
      if (!sat_solver_->ResetToLevelZero()) {
        return sat_solver_->UnsatStatus();
      }
    }

    // Backtrack if needed.
    //
    // Our algorithm stop exploring a branch as soon as its objective lower
    // bound is greater than the root lower bound. We then backtrack to the
    // first node in the branch that is not yet closed under this bound.
    //
    // TODO(user): If we remember how far we can backjump for both true/false
    // branch, we could be more efficient.
    while (current_branch_.size() > sat_solver_->CurrentDecisionLevel() + 1 ||
           (current_branch_.size() > 1 &&
            nodes_[current_branch_.back()].MinObjective() >
                current_objective_lb_)) {
      current_branch_.pop_back();
    }

    // Backtrack the solver to be in sync with current_branch_.
    {
      const int backtrack_level =
          std::max(0, static_cast<int>(current_branch_.size()) - 1);
      sat_solver_->Backtrack(backtrack_level);
      if (!sat_solver_->FinishPropagation()) {
        return sat_solver_->UnsatStatus();
      }
      if (sat_solver_->CurrentDecisionLevel() < backtrack_level) {
        continue;
      }
    }

    if (sat_solver_->CurrentDecisionLevel() == 0) {
      if (!LevelZeroLogic()) {
        return sat_solver_->UnsatStatus();
      }
    }

    // Dive: Follow the branch with lowest objective.
    // Note that we do not creates new nodes here.
    //
    // TODO(user): If we have new information and our current objective bound
    // is higher than any bound in a whole subtree, we might want to just
    // restart this subtree exploration?
    while (true) {
      const int size = current_branch_.size();
      const int level = sat_solver_->CurrentDecisionLevel();

      // Invariant are tricky:
      // current_branch_ contains one entry per decision taken + the last one
      // which we are about to take. If we don't have the last entry, it means
      // we are about to take a new decision.
      DCHECK(size == level || size == level + 1);
      if (size == level) break;  // Take new decision.

      const NodeIndex node_index = current_branch_[level];
      Node& node = nodes_[node_index];
      DCHECK_GT(node.true_child, node_index);
      DCHECK_GT(node.false_child, node_index);

      // If the bound of this node is high, restart the main loop..
      node.UpdateObjective(std::max(
          current_objective_lb_, integer_trail_->LowerBound(objective_var_)));
      if (node.MinObjective() > current_objective_lb_) break;
      DCHECK_EQ(node.MinObjective(), current_objective_lb_) << level;

      // This will be set to the next node index.
      NodeIndex n;
      DCHECK(!node.is_deleted);
      const Literal node_literal = node.Decision();

      // If the variable is already fixed, we bypass the node and connect
      // its parent directly to the relevant child.
      if (assignment_.LiteralIsAssigned(node_literal)) {
        IntegerValue new_lb;
        if (assignment_.LiteralIsTrue(node_literal)) {
          n = node.true_child;
          new_lb = node.true_objective;
        } else {
          n = node.false_child;
          new_lb = node.false_objective;
        }
        MarkAsDeletedNodeAndUnreachableSubtree(node);

        // We jump directly to the subnode.
        // Else we will change the root.
        current_branch_.pop_back();
        if (!current_branch_.empty()) {
          const NodeIndex parent = current_branch_.back();
          DCHECK(!nodes_[parent].is_deleted);
          const Literal parent_literal = nodes_[parent].Decision();
          if (assignment_.LiteralIsTrue(parent_literal)) {
            nodes_[parent].true_child = n;
            nodes_[parent].UpdateTrueObjective(new_lb);
          } else {
            DCHECK(assignment_.LiteralIsFalse(parent_literal));
            nodes_[parent].false_child = n;
            nodes_[parent].UpdateFalseObjective(new_lb);
          }
          if (new_lb > current_objective_lb_) {
            // This is probably not needed.
            if (n < nodes_.size() && !nodes_[n].IsLeaf()) {
              current_branch_.push_back(n);
              nodes_[n].UpdateObjective(new_lb);
            }
            break;
          }
        } else {
          if (n >= nodes_.size()) {
            // We never explored the other branch, so we can just clear all
            // nodes.
            num_nodes_in_tree_ = 0;
            nodes_.clear();
          } else if (nodes_[n].IsLeaf()) {
            // Keep the saved basis.
            num_nodes_in_tree_ = 1;
            Node root = std::move(nodes_[n]);
            nodes_.clear();
            nodes_.push_back(std::move(root));
            n = NodeIndex(0);
          } else {
            // We always make sure the root is at zero.
            // The root is no longer at zero, that might cause issue.
            // Cleanup.
          }
        }
      } else {
        // See if we have better bounds using the current LP state.
        ExploitReducedCosts(current_branch_[level]);
        if (node.MinObjective() > current_objective_lb_) break;

        // If both lower bound are the same, we pick the literal branch. We do
        // that because this is the polarity that was chosen by the SAT
        // heuristic in the first place. We tried random, it doesn't seems to
        // work as well.
        num_decisions_taken_++;
        const bool choose_true = node.true_objective <= node.false_objective;
        Literal next_decision;
        if (choose_true) {
          n = node.true_child;
          next_decision = node_literal;
        } else {
          n = node.false_child;
          next_decision = node_literal.Negated();
        }

        // If we are taking this branch for the first time, we enable the LP and
        // make sure we solve it before taking the decision. This allow to have
        // proper pseudo-costs, and also be incremental for the decision we are
        // about to take.
        //
        // We also enable the LP if we have no basis info for this node.
        if (SaveLpBasisOption() &&
            (n >= nodes_.size() || !NodeHasBasis(node))) {
          const auto cleanup =
              absl::MakeCleanup(UpdateLpIters(&num_lp_iters_save_basis_));

          VLOG(1) << "~~~~";
          EnableLpAndLoadBestBasis();
          const int level = sat_solver_->CurrentDecisionLevel();
          if (!sat_solver_->FinishPropagation()) {
            return sat_solver_->UnsatStatus();
          }
          if (sat_solver_->CurrentDecisionLevel() < level) {
            node.UpdateObjective(kMaxIntegerValue);
            break;
          }

          // The decision might have become assigned, in which case we loop.
          if (assignment_.LiteralIsAssigned(next_decision)) {
            continue;
          }

          SaveLpBasisInto(node);

          // If we are not at the end, disable the LP propagation.
          if (n < nodes_.size()) {
            lp_constraint_->EnablePropagation(false);
          }
        }

        // Take the decision.
        const auto cleanup =
            absl::MakeCleanup(UpdateLpIters(&num_lp_iters_first_branch_));
        DCHECK(!assignment_.LiteralIsAssigned(next_decision));
        search_helper_->TakeDecision(next_decision);

        // Conflict?
        if (current_branch_.size() != sat_solver_->CurrentDecisionLevel()) {
          MarkBranchAsInfeasible(node, choose_true);
          break;
        }

        // Update the proper field and abort the dive if we crossed the
        // threshold.
        const IntegerValue lb = integer_trail_->LowerBound(objective_var_);
        if (choose_true) {
          node.UpdateTrueObjective(lb);
        } else {
          node.UpdateFalseObjective(lb);
        }

        if (n < nodes_.size()) {
          nodes_[n].UpdateObjective(lb);
        } else if (SaveLpBasisOption()) {
          SaveLpBasisInto(nodes_[CreateNewEmptyNodeIfNeeded()]);
        }

        if (lb > current_objective_lb_) break;
      }

      if (VLOG_IS_ON(1)) {
        shared_response_->LogMessageWithThrottling(
            "TreeS", absl::StrCat(" (", SmallProgressString(), ")"));
      }

      if (n < nodes_.size() && !nodes_[n].IsLeaf()) {
        current_branch_.push_back(n);
      } else {
        break;
      }
    }

    // If a conflict occurred, we will backtrack.
    if (current_branch_.size() != sat_solver_->CurrentDecisionLevel()) {
      continue;
    }

    // TODO(user): The code is hard to follow. Fix and merge that with test
    // below.
    if (!current_branch_.empty()) {
      const Node& final_node = nodes_[current_branch_.back()];
      if (assignment_.LiteralIsTrue(final_node.Decision())) {
        if (final_node.true_objective > current_objective_lb_) {
          continue;
        }
      } else {
        DCHECK(assignment_.LiteralIsFalse(final_node.Decision()));
        if (final_node.false_objective > current_objective_lb_) {
          continue;
        }
      }
    }

    // This test allow to not take a decision when the branch is already closed
    // (i.e. the true branch or false branch lb is high enough). Adding it
    // basically changes if we take the decision later when we explore the
    // branch or right now.
    //
    // I feel taking it later is better. It also avoid creating unneeded nodes.
    // It does change the behavior on a few problem though. For instance on
    // irp.mps.gz, the search works better without this, whatever the random
    // seed. Not sure why, maybe it creates more diversity?
    //
    // Another difference is that if the search is done and we have a feasible
    // solution, we will not report it because of this test (except if we are
    // at the optimal).
    if (integer_trail_->LowerBound(objective_var_) > current_objective_lb_) {
      continue;
    }

    const auto cleanup = absl::MakeCleanup(UpdateLpIters(&num_lp_iters_dive_));

    if (current_branch_.empty()) {
      VLOG(2) << "DIVE from empty tree";
    } else {
      VLOG(2) << "DIVE from " << NodeDebugString(current_branch_.back());
    }

    if (SaveLpBasisOption() && !lp_constraint_->PropagationIsEnabled()) {
      // This reuse or create a node to store the basis.
      const NodeIndex index = CreateNewEmptyNodeIfNeeded();

      EnableLpAndLoadBestBasis();
      const int level = sat_solver_->CurrentDecisionLevel();
      if (!sat_solver_->FinishPropagation()) {
        return sat_solver_->UnsatStatus();
      }

      // Loop on backtrack or bound improvement.
      if (sat_solver_->CurrentDecisionLevel() < level) {
        Node& node = nodes_[index];
        node.UpdateObjective(kMaxIntegerValue);
        continue;
      }

      SaveLpBasisInto(nodes_[index]);

      const IntegerValue obj_lb = integer_trail_->LowerBound(objective_var_);
      if (obj_lb > current_objective_lb_) {
        nodes_[index].UpdateObjective(obj_lb);
        if (!current_branch_.empty()) {
          Node& parent_node = nodes_[current_branch_.back()];
          const Literal node_literal = parent_node.Decision();
          DCHECK(assignment_.LiteralIsAssigned(node_literal));
          if (assignment_.LiteralIsTrue(node_literal)) {
            parent_node.UpdateTrueObjective(obj_lb);
          } else {
            parent_node.UpdateFalseObjective(obj_lb);
          }
        }
        continue;
      }
    }

    // Invariant: The current branch is fully assigned, and the solver is in
    // sync. And we are not on a "bad" path.
    const int base_level = sat_solver_->CurrentDecisionLevel();
    if (DEBUG_MODE) {
      CHECK_EQ(base_level, current_branch_.size());
      for (const NodeIndex index : current_branch_) {
        CHECK(!nodes_[index].is_deleted);
        const Literal decision = nodes_[index].Decision();
        if (assignment_.LiteralIsTrue(decision)) {
          CHECK_EQ(nodes_[index].true_objective, current_objective_lb_);
        } else {
          CHECK(assignment_.LiteralIsFalse(decision));
          CHECK_EQ(nodes_[index].false_objective, current_objective_lb_);
        }
      }
    }

    // We are about to take a new decision, what we will do is dive until
    // the objective lower bound increase. we will then create a bunch of new
    // nodes in the tree.
    //
    // By analyzing the reason for the increase, we can create less nodes than
    // if we just followed the initial heuristic.
    //
    // TODO(user): In multithread, this change the behavior a lot since we
    // dive until we beat the best shared bound. Maybe we shouldn't do that.
    while (true) {
      // TODO(user): We sometimes branch on the objective variable, this should
      // probably be avoided.
      if (sat_solver_->ModelIsUnsat()) return sat_solver_->UnsatStatus();
      LiteralIndex decision;
      if (!search_helper_->GetDecision(search_heuristic_, &decision)) {
        continue;
      }

      // No new decision: search done.
      if (time_limit_->LimitReached()) return SatSolver::LIMIT_REACHED;
      if (decision == kNoLiteralIndex) {
        feasible_solution_observer();
        break;
      }

      num_decisions_taken_++;
      if (!search_helper_->TakeDecision(Literal(decision))) {
        return sat_solver_->UnsatStatus();
      }
      if (trail_->CurrentDecisionLevel() < base_level) {
        // TODO(user): it would be nice to mark some node as infeasible if
        // this is the case. However this could happen after many decision and
        // we realize with the lp that one of them should have been fixed
        // earlier, without any infeasibility in the current branch.
        break;
      }
      if (integer_trail_->LowerBound(objective_var_) > current_objective_lb_) {
        break;
      }
    }

    if (trail_->CurrentDecisionLevel() <= base_level) {
      continue;
    }

    // Analyse the reason for objective increase. Deduce a set of new nodes to
    // append to the tree.
    //
    // TODO(user): Try to minimize the number of decisions?
    const std::vector<Literal> reason =
        integer_trail_->ReasonFor(IntegerLiteral::GreaterOrEqual(
            objective_var_, integer_trail_->LowerBound(objective_var_)));
    std::vector<Literal> decisions = ExtractDecisions(base_level, reason);

    // Bump activities.
    sat_decision_->BumpVariableActivities(reason);
    sat_decision_->BumpVariableActivities(decisions);
    sat_decision_->UpdateVariableActivityIncrement();

    // Create one node per new decisions.
    DCHECK_EQ(current_branch_.size(), base_level);
    for (const Literal d : decisions) {
      AppendNewNodeToCurrentBranch(d);
    }

    // TODO(user): We should probably save the basis in more cases.
    if (SaveLpBasisOption() && decisions.size() == 1) {
      SaveLpBasisInto(nodes_[CreateNewEmptyNodeIfNeeded()]);
    }

    // Update the objective of the last node in the branch since we just
    // improved that.
    if (!current_branch_.empty()) {
      Node& n = nodes_[current_branch_.back()];
      if (assignment_.LiteralIsTrue(n.Decision())) {
        n.UpdateTrueObjective(integer_trail_->LowerBound(objective_var_));
      } else {
        n.UpdateFalseObjective(integer_trail_->LowerBound(objective_var_));
      }
    }

    // Reset the solver to a correct state since we have a subset of the
    // current propagation. We backtrack as little as possible.
    //
    // The decision level is the number of decision taken.
    // Decision()[level] is the decision at that level.
    int backtrack_level = base_level;
    DCHECK_LE(current_branch_.size(), trail_->CurrentDecisionLevel());
    while (backtrack_level < current_branch_.size() &&
           trail_->Decisions()[backtrack_level].literal.Index() ==
               nodes_[current_branch_[backtrack_level]].literal_index) {
      ++backtrack_level;
    }
    sat_solver_->BacktrackAndPropagateReimplications(backtrack_level);

    // Update bounds with reduced costs info.
    //
    // TODO(user): Uses old optimal constraint that we just potentially
    // backtracked over?
    //
    // TODO(user): We could do all at once rather than in O(#decision * #size).
    for (int i = backtrack_level; i < current_branch_.size(); ++i) {
      ExploitReducedCosts(current_branch_[i]);
    }
  }

  return SatSolver::LIMIT_REACHED;
}

std::vector<Literal> LbTreeSearch::ExtractDecisions(
    int base_level, absl::Span<const Literal> conflict) {
  std::vector<int> num_per_level(sat_solver_->CurrentDecisionLevel() + 1, 0);
  std::vector<bool> is_marked;
  for (const Literal l : conflict) {
    const AssignmentInfo& info = trail_->Info(l.Variable());
    if (info.level <= base_level) continue;
    num_per_level[info.level]++;
    if (info.trail_index >= is_marked.size()) {
      is_marked.resize(info.trail_index + 1);
    }
    is_marked[info.trail_index] = true;
  }

  std::vector<Literal> result;
  if (is_marked.empty()) return result;
  for (int i = is_marked.size() - 1; i >= 0; --i) {
    if (!is_marked[i]) continue;

    const Literal l = (*trail_)[i];
    const AssignmentInfo& info = trail_->Info(l.Variable());
    if (info.level <= base_level) break;
    if (num_per_level[info.level] == 1) {
      result.push_back(l);
      continue;
    }

    // Expand.
    num_per_level[info.level]--;
    for (const Literal new_l : trail_->Reason(l.Variable())) {
      const AssignmentInfo& new_info = trail_->Info(new_l.Variable());
      if (new_info.level <= base_level) continue;
      if (is_marked[new_info.trail_index]) continue;
      is_marked[new_info.trail_index] = true;
      num_per_level[new_info.level]++;
    }
  }

  // We prefer to keep the same order.
  std::reverse(result.begin(), result.end());
  return result;
}

LbTreeSearch::NodeIndex LbTreeSearch::CreateNewEmptyNodeIfNeeded() {
  NodeIndex n(0);
  if (current_branch_.empty()) {
    if (nodes_.empty()) {
      ++num_nodes_in_tree_;
      nodes_.emplace_back(current_objective_lb_);
    } else {
      DCHECK_EQ(nodes_.size(), 1);
    }
  } else {
    const NodeIndex parent = current_branch_.back();
    DCHECK(!nodes_[parent].is_deleted);
    const Literal parent_literal = nodes_[parent].Decision();
    if (assignment_.LiteralIsTrue(parent_literal)) {
      if (nodes_[parent].true_child >= nodes_.size()) {
        n = NodeIndex(nodes_.size());
        ++num_nodes_in_tree_;
        nodes_[parent].true_child = NodeIndex(nodes_.size());
        nodes_.emplace_back(current_objective_lb_);
      } else {
        n = nodes_[parent].true_child;
      }
      nodes_[parent].UpdateTrueObjective(current_objective_lb_);
    } else {
      DCHECK(assignment_.LiteralIsFalse(parent_literal));
      if (nodes_[parent].false_child >= nodes_.size()) {
        n = NodeIndex(nodes_.size());
        ++num_nodes_in_tree_;
        nodes_[parent].false_child = NodeIndex(nodes_.size());
        nodes_.emplace_back(current_objective_lb_);
      } else {
        n = nodes_[parent].false_child;
      }
      nodes_[parent].UpdateFalseObjective(current_objective_lb_);
    }
  }
  DCHECK(!nodes_[n].is_deleted);
  DCHECK_EQ(nodes_[n].literal_index, kNoLiteralIndex);
  return n;
}

void LbTreeSearch::AppendNewNodeToCurrentBranch(Literal decision) {
  NodeIndex n(nodes_.size());
  if (current_branch_.empty()) {
    if (nodes_.empty()) {
      ++num_nodes_in_tree_;
      nodes_.emplace_back(current_objective_lb_);
    } else {
      DCHECK_EQ(nodes_.size(), 1);
      n = 0;
    }
  } else {
    const NodeIndex parent = current_branch_.back();
    DCHECK(!nodes_[parent].is_deleted);
    const Literal parent_literal = nodes_[parent].Decision();
    if (assignment_.LiteralIsTrue(parent_literal)) {
      if (nodes_[parent].true_child < nodes_.size()) {
        n = nodes_[parent].true_child;
      } else {
        ++num_nodes_in_tree_;
        nodes_.emplace_back(current_objective_lb_);
        nodes_[parent].true_child = n;
      }
      nodes_[parent].UpdateTrueObjective(current_objective_lb_);
    } else {
      DCHECK(assignment_.LiteralIsFalse(parent_literal));
      if (nodes_[parent].false_child < nodes_.size()) {
        n = nodes_[parent].false_child;
      } else {
        ++num_nodes_in_tree_;
        nodes_.emplace_back(current_objective_lb_);
        nodes_[parent].false_child = n;
      }
      nodes_[parent].UpdateFalseObjective(current_objective_lb_);
    }
  }
  DCHECK_LT(n, nodes_.size());
  DCHECK_EQ(nodes_[n].literal_index, kNoLiteralIndex) << " issue " << n;
  nodes_[n].SetDecision(decision);
  nodes_[n].UpdateObjective(current_objective_lb_);
  current_branch_.push_back(n);
}

// Looking at the reduced costs, we can already have a bound for one of the
// branch. Increasing the corresponding objective can save some branches,
// and also allow for a more incremental LP solving since we do less back
// and forth.
//
// TODO(user): The code to recover that is a bit convoluted. Alternatively
// Maybe we should do a "fast" propagation without the LP in each branch.
// That will work as long as we keep these optimal LP constraints around
// and propagate them.
//
// TODO(user): Incorporate this in the heuristic so we choose more Booleans
// inside these LP explanations?
void LbTreeSearch::ExploitReducedCosts(NodeIndex n) {
  if (lp_constraint_ == nullptr) return;

  // TODO(user): we could consider earlier constraints instead of just
  // looking at the last one, but experiments didn't really show a big
  // gain.
  const auto& cts = lp_constraint_->OptimalConstraints();
  if (cts.empty()) return;
  const std::unique_ptr<IntegerSumLE128>& rc = cts.back();

  // Note that this return literal EQUIVALENT to the node.literal, not just
  // implied by it. We need that for correctness.
  int num_tests = 0;
  Node& node = nodes_[n];
  DCHECK(!node.is_deleted);
  const Literal node_literal = node.Decision();

  // This can happen if we have re-implication and propagation...
  if (assignment_.LiteralIsAssigned(node_literal)) return;

  for (const IntegerLiteral integer_literal :
       integer_encoder_->GetIntegerLiterals(node_literal)) {
    // To avoid bad corner case. Not sure it ever triggers.
    if (++num_tests > 10) break;

    const std::pair<IntegerValue, IntegerValue> bounds =
        rc->ConditionalLb(integer_literal, objective_var_);
    if (bounds.first > node.false_objective) {
      ++num_rc_detected_;
      node.UpdateFalseObjective(bounds.first);
    }
    if (bounds.second > node.true_objective) {
      ++num_rc_detected_;
      node.UpdateTrueObjective(bounds.second);
    }
  }
}

}  // namespace sat
}  // namespace operations_research
