// Copyright 2010-2024 Google LLC
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
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_expr.h"
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

LbTreeSearch::LbTreeSearch(Model* model)
    : name_(model->Name()),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      random_(model->GetOrCreate<ModelRandomGenerator>()),
      sat_solver_(model->GetOrCreate<SatSolver>()),
      integer_encoder_(model->GetOrCreate<IntegerEncoder>()),
      trail_(model->GetOrCreate<Trail>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      watcher_(model->GetOrCreate<GenericLiteralWatcher>()),
      shared_response_(model->GetOrCreate<SharedResponseManager>()),
      sat_decision_(model->GetOrCreate<SatDecisionPolicy>()),
      search_helper_(model->GetOrCreate<IntegerSearchHelper>()),
      parameters_(*model->GetOrCreate<SatParameters>()) {
  // We should create this class only in the presence of an objective.
  //
  // TODO(user): Starts with an initial variable score for all variable in
  // the objective at their minimum value? this should emulate the first step of
  // the core approach and gives a similar bound.
  const ObjectiveDefinition* objective = model->Get<ObjectiveDefinition>();
  CHECK(objective != nullptr);
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
  search_heuristic_ = SequentialSearch(
      {SatSolverHeuristic(model), MostFractionalHeuristic(model),
       IntegerValueSelectionHeuristic(
           model->GetOrCreate<SearchHeuristics>()->fixed_search, model)});
}

void LbTreeSearch::UpdateParentObjective(int level) {
  CHECK_GE(level, 0);
  CHECK_LT(level, current_branch_.size());
  if (level == 0) return;
  const NodeIndex parent_index = current_branch_[level - 1];
  Node& parent = nodes_[parent_index];
  const NodeIndex child_index = current_branch_[level];
  const Node& child = nodes_[child_index];
  if (parent.true_child == child_index) {
    parent.UpdateTrueObjective(child.MinObjective());
  } else {
    CHECK_EQ(parent.false_child, child_index);
    parent.UpdateFalseObjective(child.MinObjective());
  }
}

void LbTreeSearch::UpdateObjectiveFromParent(int level) {
  CHECK_GE(level, 0);
  CHECK_LT(level, current_branch_.size());
  if (level == 0) return;
  const NodeIndex parent_index = current_branch_[level - 1];
  const Node& parent = nodes_[parent_index];
  CHECK_GE(parent.MinObjective(), current_objective_lb_);
  const NodeIndex child_index = current_branch_[level];
  Node& child = nodes_[child_index];
  if (parent.true_child == child_index) {
    child.UpdateObjective(parent.true_objective);
  } else {
    CHECK_EQ(parent.false_child, child_index);
    child.UpdateObjective(parent.false_objective);
  }
}

void LbTreeSearch::DebugDisplayTree(NodeIndex root) const {
  int num_nodes = 0;
  const IntegerValue root_lb = nodes_[root].MinObjective();
  const auto shifted_lb = [root_lb](IntegerValue lb) {
    return std::max<int64_t>(0, (lb - root_lb).value());
  };

  absl::StrongVector<NodeIndex, int> level(nodes_.size(), 0);
  std::vector<NodeIndex> to_explore = {root};
  while (!to_explore.empty()) {
    NodeIndex n = to_explore.back();
    to_explore.pop_back();

    ++num_nodes;
    const Node& node = nodes_[n];

    std::string s(level[n], ' ');
    absl::StrAppend(&s, "#", n.value());

    if (node.true_child < nodes_.size()) {
      absl::StrAppend(&s, " [t:#", node.true_child.value(), " ",
                      shifted_lb(node.true_objective), "]");
      to_explore.push_back(node.true_child);
      level[node.true_child] = level[n] + 1;
    } else {
      absl::StrAppend(&s, " [t:## ", shifted_lb(node.true_objective), "]");
    }
    if (node.false_child < nodes_.size()) {
      absl::StrAppend(&s, " [f:#", node.false_child.value(), " ",
                      shifted_lb(node.false_objective), "]");
      to_explore.push_back(node.false_child);
      level[node.false_child] = level[n] + 1;
    } else {
      absl::StrAppend(&s, " [f:## ", shifted_lb(node.false_objective), "]");
    }
    LOG(INFO) << s;
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
  return sat_solver_->RestoreSolverToAssumptionLevel();
}

void LbTreeSearch::MarkAsDeletedNodeAndUnreachableSubtree(Node& node) {
  --num_nodes_in_tree_;
  node.is_deleted = true;
  if (sat_solver_->Assignment().LiteralIsTrue(node.literal)) {
    MarkSubtreeAsDeleted(node.false_child);
  } else {
    MarkSubtreeAsDeleted(node.true_child);
  }
}

void LbTreeSearch::MarkSubtreeAsDeleted(NodeIndex root) {
  std::vector<NodeIndex> to_delete{root};
  for (int i = 0; i < to_delete.size(); ++i) {
    const NodeIndex n = to_delete[i];
    if (n >= nodes_.size()) continue;

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
      " @root=", num_back_to_root_node_, " restarts=", num_full_restarts_);
}

SatSolver::Status LbTreeSearch::Search(
    const std::function<void()>& feasible_solution_observer) {
  if (!sat_solver_->RestoreSolverToAssumptionLevel()) {
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

  while (!time_limit_->LimitReached() && !shared_response_->ProblemIsSolved()) {
    // This is the current bound we try to improve. We cache it here to avoid
    // getting the lock many times and it is also easier to follow the code if
    // this is assumed constant for one iteration.
    current_objective_lb_ = shared_response_->GetInnerObjectiveLowerBound();

    // If some branches already have a good lower bound, no need to call the LP
    // on those.
    watcher_->SetStopPropagationCallback([this] {
      return integer_trail_->LowerBound(objective_var_) > current_objective_lb_;
    });

    // Propagate upward in the tree the new objective lb.
    if (!current_branch_.empty()) {
      // Our branch is always greater or equal to the level.
      // We increase the objective_lb of the current node if needed.
      {
        const int current_level = sat_solver_->CurrentDecisionLevel();
        CHECK_GE(current_branch_.size(), current_level);
        for (int i = 0; i < current_level; ++i) {
          CHECK(sat_solver_->Assignment().LiteralIsAssigned(
              nodes_[current_branch_[i]].literal));
        }
        if (current_level < current_branch_.size()) {
          nodes_[current_branch_[current_level]].UpdateObjective(
              integer_trail_->LowerBound(objective_var_));
        }

        // Minor optim: sometimes, because of the LP and cuts, the reason for
        // objective_var_ only contains lower level literals, so we can exploit
        // that.
        //
        // TODO(user): No point checking that if the objective lb wasn't
        // assigned at this level.
        //
        // TODO(user): Exploit the reasons further.
        if (integer_trail_->LowerBound(objective_var_) >
            integer_trail_->LevelZeroLowerBound(objective_var_)) {
          const std::vector<Literal> reason =
              integer_trail_->ReasonFor(IntegerLiteral::GreaterOrEqual(
                  objective_var_, integer_trail_->LowerBound(objective_var_)));
          int max_level = 0;
          for (const Literal l : reason) {
            max_level = std::max<int>(
                max_level,
                sat_solver_->LiteralTrail().Info(l.Variable()).level);
          }
          if (max_level < current_level) {
            nodes_[current_branch_[max_level]].UpdateObjective(
                integer_trail_->LowerBound(objective_var_));
          }
        }
      }

      // Propagate upward and then forward any new bounds.
      for (int level = current_branch_.size(); --level > 0;) {
        UpdateParentObjective(level);
      }
      nodes_[current_branch_[0]].UpdateObjective(current_objective_lb_);
      for (int level = 1; level < current_branch_.size(); ++level) {
        UpdateObjectiveFromParent(level);
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
    if (integer_trail_->LowerBound(objective_var_) >
        integer_trail_->LevelZeroLowerBound(objective_var_)) {
      std::vector<Literal> reason =
          integer_trail_->ReasonFor(IntegerLiteral::GreaterOrEqual(
              objective_var_, integer_trail_->LowerBound(objective_var_)));
      sat_decision_->BumpVariableActivities(reason);
      sat_decision_->UpdateVariableActivityIncrement();
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

    // Backtrack the solver.
    {
      int backtrack_level =
          std::max(0, static_cast<int>(current_branch_.size()) - 1);

      // Periodic backtrack to level zero so we can import bounds.
      if (num_decisions_taken_ >=
          num_decisions_taken_at_last_level_zero_ + 10000) {
        backtrack_level = 0;
      }

      sat_solver_->Backtrack(backtrack_level);
      if (!sat_solver_->FinishPropagation()) {
        return sat_solver_->UnsatStatus();
      }
    }

    if (sat_solver_->CurrentDecisionLevel() == 0) {
      ++num_back_to_root_node_;
      num_decisions_taken_at_last_level_zero_ = num_decisions_taken_;
    }

    // This will import other workers bound if we are back to level zero.
    if (!search_helper_->BeforeTakingDecision()) {
      return sat_solver_->UnsatStatus();
    }

    // If the search has not just been restarted (in which case nodes_ would be
    // empty), and if we are at level zero (either naturally, or if the
    // backtrack level was set to zero in the above code), let's run a different
    // heuristic to decide whether to restart the search from scratch or not.
    //
    // We ignore small search trees.
    if (sat_solver_->CurrentDecisionLevel() == 0 && num_nodes_in_tree_ > 50) {
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
        if (!FullRestart()) return sat_solver_->UnsatStatus();
      }
    }

    // Dive: Follow the branch with lowest objective.
    // Note that we do not creates new nodes here.
    //
    // TODO(user): If we have new information and our current objective bound
    // is higher than any bound in a whole subtree, we might want to just
    // restart this subtree exploration?
    while (current_branch_.size() == sat_solver_->CurrentDecisionLevel() + 1) {
      const int level = current_branch_.size() - 1;
      CHECK_EQ(level, sat_solver_->CurrentDecisionLevel());
      Node& node = nodes_[current_branch_[level]];
      node.UpdateObjective(std::max(
          current_objective_lb_, integer_trail_->LowerBound(objective_var_)));
      if (node.MinObjective() > current_objective_lb_) break;
      CHECK_EQ(node.MinObjective(), current_objective_lb_) << level;

      // This will be set to the next node index.
      NodeIndex n;

      // If the variable is already fixed, we bypass the node and connect
      // its parent directly to the relevant child.
      if (sat_solver_->Assignment().LiteralIsAssigned(node.literal)) {
        IntegerValue new_lb;
        if (sat_solver_->Assignment().LiteralIsTrue(node.literal)) {
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
          if (sat_solver_->Assignment().LiteralIsTrue(nodes_[parent].literal)) {
            nodes_[parent].true_child = n;
            nodes_[parent].UpdateTrueObjective(new_lb);
          } else {
            DCHECK(sat_solver_->Assignment().LiteralIsFalse(
                nodes_[parent].literal));
            nodes_[parent].false_child = n;
            nodes_[parent].UpdateFalseObjective(new_lb);
          }
          if (nodes_[parent].MinObjective() > current_objective_lb_) break;
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
        if (choose_true) {
          n = node.true_child;
          search_helper_->TakeDecision(node.literal);
        } else {
          n = node.false_child;
          search_helper_->TakeDecision(node.literal.Negated());
        }

        // Conflict?
        if (current_branch_.size() != sat_solver_->CurrentDecisionLevel()) {
          if (choose_true) {
            node.UpdateTrueObjective(kMaxIntegerValue);
          } else {
            node.UpdateFalseObjective(kMaxIntegerValue);
          }
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
        if (lb > current_objective_lb_) break;
      }

      if (VLOG_IS_ON(1)) {
        shared_response_->LogMessageWithThrottling(
            "TreeS", absl::StrCat(" (", SmallProgressString(), ")"));
      }

      if (n < nodes_.size()) {
        current_branch_.push_back(n);
      } else {
        break;
      }
    }

    // If a conflict occurred, we will backtrack.
    if (current_branch_.size() != sat_solver_->CurrentDecisionLevel()) {
      continue;
    }

    // This test allow to not take a decision when the branch is already closed
    // (i.e. the true branch or false branch lb is high enough). Adding it
    // basically changes if we take the decision later when we explore the
    // branch or right now.
    //
    // I feel taking it later is better. It also avoid creating uneeded nodes.
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

    // We are about to take a new decision, what we will do is dive until
    // the objective lower bound increase. we will then create a bunch of new
    // nodes in the tree.
    //
    // By analyzing the reason for the increase, we can create less nodes than
    // if we just followed the initial heuristic.
    //
    // TODO(user): In multithread, this change the behavior a lot since we
    // dive until we beat the best shared bound. Maybe we shouldn't do that.
    const int base_level = sat_solver_->CurrentDecisionLevel();
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
      if (sat_solver_->CurrentDecisionLevel() < base_level) break;
      if (integer_trail_->LowerBound(objective_var_) > current_objective_lb_) {
        break;
      }
    }
    if (sat_solver_->CurrentDecisionLevel() <= base_level) continue;

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
    CHECK_EQ(current_branch_.size(), base_level);
    for (const Literal d : decisions) {
      AppendNewNodeToCurrentBranch(d);
    }

    // Update the objective of the last node in the branch since we just
    // improved that.
    if (!current_branch_.empty()) {
      Node& n = nodes_[current_branch_.back()];
      if (sat_solver_->Assignment().LiteralIsTrue(n.literal)) {
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
    CHECK_LE(current_branch_.size(), sat_solver_->CurrentDecisionLevel());
    while (backtrack_level < current_branch_.size() &&
           sat_solver_->Decisions()[backtrack_level].literal ==
               nodes_[current_branch_[backtrack_level]].literal) {
      ++backtrack_level;
    }
    sat_solver_->Backtrack(backtrack_level);

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

void LbTreeSearch::AppendNewNodeToCurrentBranch(Literal decision) {
  const NodeIndex n(nodes_.size());
  ++num_nodes_in_tree_;
  nodes_.emplace_back(Literal(decision), current_objective_lb_);
  if (!current_branch_.empty()) {
    const NodeIndex parent = current_branch_.back();
    if (sat_solver_->Assignment().LiteralIsTrue(nodes_[parent].literal)) {
      nodes_[parent].true_child = n;
      nodes_[parent].UpdateTrueObjective(nodes_.back().MinObjective());
    } else {
      CHECK(sat_solver_->Assignment().LiteralIsFalse(nodes_[parent].literal));
      nodes_[parent].false_child = n;
      nodes_[parent].UpdateFalseObjective(nodes_.back().MinObjective());
    }
  }
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
  CHECK(!sat_solver_->Assignment().LiteralIsAssigned(node.literal));
  for (const IntegerLiteral integer_literal :
       integer_encoder_->GetIntegerLiterals(node.literal)) {
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
