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

  // We use the normal SAT search but we will bump the variable activity
  // slightly differently. In addition to the conflicts, we also bump it each
  // time the objective lower bound increase in a sub-node.
  search_heuristic_ =
      SequentialSearch({SatSolverHeuristic(model),
                        model->GetOrCreate<SearchHeuristics>()->fixed_search});
}

bool LbTreeSearch::NodeImprovesLowerBound(const Node& node) {
  return node.objective_lb >
         integer_trail_->LevelZeroLowerBound(objective_var_);
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
    // Each time we are back here, we bump the activities of the variable that
    // are part of the objective lower bound reason.
    //
    // TODO(user): This is slightly different than bumping each time we
    // push a decision that result in an LB increase, I am not sure why.
    if (integer_trail_->LowerBound(objective_var_) >
        integer_trail_->LevelZeroLowerBound(objective_var_)) {
      std::vector<Literal> reason =
          integer_trail_->ReasonFor(IntegerLiteral::GreaterOrEqual(
              objective_var_, integer_trail_->LowerBound(objective_var_)));
      sat_decision_->BumpVariableActivities(reason);
      sat_decision_->UpdateVariableActivityIncrement();

      // Optimization. Record what level is needed for this reason and try to
      // reduce the search tree if this node decision could have been taken
      // earlier.
      const int current_level = sat_solver_->CurrentDecisionLevel();
      if (current_branch_.size() == current_level) {
        // TODO(user): We should probably expand the reason.
        int max_level = 0;
        Node& node = nodes_[current_branch_.back()];
        for (const Literal l : reason) {
          if (l.Variable() == node.literal.Variable()) continue;
          max_level = std::max<int>(
              max_level, sat_solver_->LiteralTrail().Info(l.Variable()).level);
        }
        if (sat_solver_->Assignment().LiteralIsTrue(node.literal)) {
          node.true_level = std::min(node.true_level, max_level);
        } else {
          CHECK(sat_solver_->Assignment().LiteralIsFalse(node.literal));
          node.false_level = std::min(node.false_level, max_level);
        }
        const int level = std::max(node.true_level, node.false_level);
        if (level < current_level - 1) {
          // We Skip a part of the tree and connect directly "ancestor" to
          // "node".
          if (level > 0) {
            Node& ancestor = nodes_[current_branch_[level - 1]];
            if (sat_solver_->Assignment().LiteralIsTrue(ancestor.literal)) {
              ancestor.true_child = current_branch_.back();
              ancestor.UpdateTrueObjective(node.objective_lb);
            } else {
              CHECK(sat_solver_->Assignment().LiteralIsFalse(ancestor.literal));
              ancestor.false_child = current_branch_.back();
              ancestor.UpdateFalseObjective(node.objective_lb);
            }
            current_branch_.resize(level);
          } else {
            const Node copy = node;
            nodes_ = {copy};
            current_branch_ = {0};
          }
          sat_solver_->Backtrack(level);
          if (!sat_solver_->FinishPropagation()) {
            return sat_solver_->UnsatStatus();
          }
        }
      }
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

    if (!search_helper_->BeforeTakingDecision()) {
      return sat_solver_->UnsatStatus();
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
            NodeImprovesLowerBound(nodes_[current_branch_.back()]))) {
      const int child = current_branch_.back();
      current_branch_.pop_back();

      // By construction, we never backtrack over the root node here.
      CHECK(!current_branch_.empty());
      Node& node = nodes_[current_branch_.back()];
      if (node.true_child == child) {
        node.UpdateTrueObjective(nodes_[child].objective_lb);
      } else {
        CHECK_EQ(node.false_child, child);
        node.UpdateFalseObjective(nodes_[child].objective_lb);
      }
    }

    // Backtrack the solver.
    sat_solver_->Backtrack(
        std::max(0, static_cast<int>(current_branch_.size()) - 1));
    if (!sat_solver_->FinishPropagation()) {
      return sat_solver_->UnsatStatus();
    }

    // If we are back to the root node, update the global objective LB.
    if (sat_solver_->CurrentDecisionLevel() == 0 && !current_branch_.empty()) {
      if (NodeImprovesLowerBound(nodes_[current_branch_[0]])) {
        shared_response_->UpdateInnerObjectiveBounds(
            absl::StrCat("lb_tree_search #nodes:", nodes_.size()),
            nodes_[current_branch_[0]].objective_lb,
            integer_trail_->UpperBound(objective_var_));
        if (!integer_trail_->Enqueue(
                IntegerLiteral::GreaterOrEqual(
                    objective_var_, nodes_[current_branch_[0]].objective_lb),
                {}, {})) {
          return SatSolver::INFEASIBLE;
        }
        if (!sat_solver_->FinishPropagation()) {
          return sat_solver_->UnsatStatus();
        }
      }
    }

    // Follow the branch with lowest objective.
    while (current_branch_.size() == sat_solver_->CurrentDecisionLevel() + 1) {
      Node& node = nodes_[current_branch_.back()];
      node.objective_lb = std::max(node.objective_lb,
                                   integer_trail_->LowerBound(objective_var_));
      node.UpdateTrueObjective(node.objective_lb);
      node.UpdateFalseObjective(node.objective_lb);
      if (NodeImprovesLowerBound(node)) break;

      // This will be set to the next index.
      int n;

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
        current_branch_.pop_back();

        // Else we will change the root.
        if (!current_branch_.empty()) {
          const int parent = current_branch_.back();
          if (sat_solver_->Assignment().LiteralIsTrue(nodes_[parent].literal)) {
            nodes_[parent].true_child = n;
            nodes_[parent].UpdateTrueObjective(new_lb);
          } else {
            CHECK(sat_solver_->Assignment().LiteralIsFalse(
                nodes_[parent].literal));
            nodes_[parent].false_child = n;
            nodes_[parent].UpdateFalseObjective(new_lb);
          }
          if (NodeImprovesLowerBound(nodes_[parent])) break;
        }
      } else {
        // If both lower bound are the same, we pick a random sub-branch.
        bool choose_true = node.true_objective < node.false_objective;
        if (node.true_objective == node.false_objective) {
          choose_true = absl::Bernoulli(*random_, 0.5);
        }
        if (choose_true) {
          DCHECK_EQ(node.true_objective,
                    integer_trail_->LevelZeroLowerBound(objective_var_));
          n = node.true_child;
          search_helper_->TakeDecision(node.literal);
        } else {
          DCHECK_EQ(node.false_objective,
                    integer_trail_->LevelZeroLowerBound(objective_var_));
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

        if (choose_true) {
          node.UpdateTrueObjective(integer_trail_->LowerBound(objective_var_));
        } else {
          node.UpdateFalseObjective(integer_trail_->LowerBound(objective_var_));
        }
        if (NodeImprovesLowerBound(node)) break;
        if (integer_trail_->LowerBound(objective_var_) >
            integer_trail_->LevelZeroLowerBound(objective_var_)) {
          break;
        }
      }

      if (n < nodes_.size()) {
        current_branch_.push_back(n);
      } else {
        break;
      }
    }

    // If conflict or the objective lower bound increased, we will backtrack.
    if (current_branch_.size() != sat_solver_->CurrentDecisionLevel() ||
        (!current_branch_.empty() &&
         NodeImprovesLowerBound(nodes_[current_branch_.back()]))) {
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
    if (integer_trail_->LowerBound(objective_var_) >
        integer_trail_->LevelZeroLowerBound(objective_var_)) {
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
    const int n = nodes_.size();
    nodes_.emplace_back(Literal(decision),
                        integer_trail_->LowerBound(objective_var_));
    if (!current_branch_.empty()) {
      const int parent = current_branch_.back();
      if (sat_solver_->Assignment().LiteralIsTrue(nodes_[parent].literal)) {
        nodes_[parent].true_child = n;
        nodes_[parent].UpdateTrueObjective(nodes_.back().objective_lb);
      } else {
        CHECK(sat_solver_->Assignment().LiteralIsFalse(nodes_[parent].literal));
        nodes_[parent].false_child = n;
        nodes_[parent].UpdateFalseObjective(nodes_.back().objective_lb);
      }
    }
    current_branch_.push_back(n);
  }

  return SatSolver::LIMIT_REACHED;
}

}  // namespace sat
}  // namespace operations_research
