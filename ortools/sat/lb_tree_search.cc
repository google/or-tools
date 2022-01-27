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

#include "ortools/sat/lb_tree_search.h"

#include <cstdint>

#include "ortools/sat/cp_model_mapping.h"

namespace operations_research {
namespace sat {

LbTreeSearch::LbTreeSearch(Model* model)
    : time_limit_(model->GetOrCreate<TimeLimit>()),
      random_(model->GetOrCreate<ModelRandomGenerator>()),
      sat_solver_(model->GetOrCreate<SatSolver>()),
      integer_encoder_(model->GetOrCreate<IntegerEncoder>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      shared_response_(model->GetOrCreate<SharedResponseManager>()),
      sat_decision_(model->GetOrCreate<SatDecisionPolicy>()),
      search_helper_(model->GetOrCreate<IntegerSearchHelper>()) {
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
  search_heuristic_ =
      SequentialSearch({SatSolverHeuristic(model),
                        model->GetOrCreate<SearchHeuristics>()->fixed_search});
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

SatSolver::Status LbTreeSearch::Search(
    const std::function<void()>& feasible_solution_observer) {
  if (!sat_solver_->RestoreSolverToAssumptionLevel()) {
    return sat_solver_->UnsatStatus();
  }

  // We currently restart the search tree from scratch a few time. This is to
  // allow our "pseudo-cost" to kick in and experimentally result in smaller
  // trees down the road.
  //
  // TODO(user): a strong branching initial start, or allowing a few decision
  // per nodes might be a better approach.
  //
  // TODO(user): It would also be cool to exploit the reason for the LB increase
  // even more.
  int64_t restart = 100;
  int64_t num_restart = 1;
  const int kNumRestart = 10;

  while (!time_limit_->LimitReached() && !shared_response_->ProblemIsSolved()) {
    // This is the current bound we try to improve. We cache it here to avoid
    // getting the lock many times and it is also easier to follow the code if
    // this is assumed constant for one iteration.
    current_objective_lb_ = shared_response_->GetInnerObjectiveLowerBound();

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
            absl::StrCat("lb_tree_search #nodes:", nodes_.size(),
                         " #rc:", num_rc_detected_),
            bound, integer_trail_->LevelZeroUpperBound(objective_var_));
        current_objective_lb_ = bound;
        if (VLOG_IS_ON(2)) DebugDisplayTree(current_branch_[0]);
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

    // Forget the whole tree and restart?
    if (nodes_.size() > num_restart * restart && num_restart < kNumRestart) {
      nodes_.clear();
      current_branch_.clear();
      if (!sat_solver_->RestoreSolverToAssumptionLevel()) {
        return sat_solver_->UnsatStatus();
      }
      ++num_restart;
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
    sat_solver_->Backtrack(
        std::max(0, static_cast<int>(current_branch_.size()) - 1));
    if (!sat_solver_->FinishPropagation()) {
      return sat_solver_->UnsatStatus();
    }

    // This will import other workers bound if we are back to level zero.
    if (!search_helper_->BeforeTakingDecision()) {
      return sat_solver_->UnsatStatus();
    }

    // Dive: Follow the branch with lowest objective.
    // Note that we do not creates new nodes here.
    while (current_branch_.size() == sat_solver_->CurrentDecisionLevel() + 1) {
      const int level = current_branch_.size() - 1;
      CHECK_EQ(level, sat_solver_->CurrentDecisionLevel());
      Node& node = nodes_[current_branch_[level]];
      node.UpdateObjective(std::max(
          current_objective_lb_, integer_trail_->LowerBound(objective_var_)));
      if (node.MinObjective() > current_objective_lb_) {
        break;
      }
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

        // We jump directly to the subnode.
        // Else we will change the root.
        current_branch_.pop_back();
        if (!current_branch_.empty()) {
          const NodeIndex parent = current_branch_.back();
          if (sat_solver_->Assignment().LiteralIsTrue(nodes_[parent].literal)) {
            nodes_[parent].true_child = n;
            nodes_[parent].UpdateTrueObjective(new_lb);
          } else {
            CHECK(sat_solver_->Assignment().LiteralIsFalse(
                nodes_[parent].literal));
            nodes_[parent].false_child = n;
            nodes_[parent].UpdateFalseObjective(new_lb);
          }
          if (nodes_[parent].MinObjective() > current_objective_lb_) break;
        }
      } else {
        // If both lower bound are the same, we pick a random sub-branch.
        bool choose_true = node.true_objective < node.false_objective;
        if (node.true_objective == node.false_objective) {
          choose_true = absl::Bernoulli(*random_, 0.5);
        }
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

    // Increase the size of the tree by exploring a new decision.
    const LiteralIndex decision =
        search_helper_->GetDecision(search_heuristic_);

    // No new decision: search done.
    if (time_limit_->LimitReached()) return SatSolver::LIMIT_REACHED;
    if (decision == kNoLiteralIndex) {
      feasible_solution_observer();
      continue;
    }

    // Create a new node.
    // Note that the decision will be pushed to the solver on the next loop.
    const NodeIndex n(nodes_.size());
    nodes_.emplace_back(Literal(decision),
                        std::max(current_objective_lb_,
                                 integer_trail_->LowerBound(objective_var_)));
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
    // TODO(user): Incorporate this in the heuristic so we choose more Boolean
    // inside these LP explanations?
    if (lp_constraint_ != nullptr) {
      // Note that this return literal EQUIVALENT to the decision, not just
      // implied by it. We need that for correctness.
      int num_tests = 0;
      for (const IntegerLiteral integer_literal :
           integer_encoder_->GetIntegerLiterals(Literal(decision))) {
        if (integer_trail_->IsCurrentlyIgnored(integer_literal.var)) continue;

        // To avoid bad corner case. Not sure it ever triggers.
        if (++num_tests > 10) break;

        // TODO(user): we could consider earlier constraint instead of just
        // looking at the last one, but experiments didn't really show a big
        // gain.
        const auto& cts = lp_constraint_->OptimalConstraints();
        if (cts.empty()) continue;

        const std::unique_ptr<IntegerSumLE>& rc = cts.back();
        const std::pair<IntegerValue, IntegerValue> bounds =
            rc->ConditionalLb(integer_literal, objective_var_);
        Node& node = nodes_[n];
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
  }

  return SatSolver::LIMIT_REACHED;
}

}  // namespace sat
}  // namespace operations_research
