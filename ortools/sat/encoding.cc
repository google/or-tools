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

#include "ortools/sat/encoding.h"

#include <stddef.h>

#include <algorithm>
#include <deque>
#include <functional>
#include <limits>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/stl_util.h"
#include "ortools/sat/boolean_problem.pb.h"
#include "ortools/sat/pb_constraint.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

EncodingNode EncodingNode::ConstantNode(Coefficient weight) {
  EncodingNode node;
  node.lb_ = 1;
  node.ub_ = 1;
  node.weight_lb_ = 0;
  node.weight_ = weight;
  return node;
}

EncodingNode EncodingNode::LiteralNode(Literal l, Coefficient weight) {
  EncodingNode node;
  node.lb_ = 0;
  node.weight_lb_ = 0;
  node.ub_ = 1;
  node.weight_ = weight;
  node.for_sorting_ = l.Variable();
  node.literals_ = {l};
  return node;
}

EncodingNode EncodingNode::GenericNode(int lb, int ub,
                                       std::function<Literal(int x)> create_lit,
                                       Coefficient weight) {
  EncodingNode node;
  node.lb_ = lb;
  node.ub_ = ub;
  node.weight_lb_ = 0;
  node.create_lit_ = std::move(create_lit);
  node.weight_ = weight;

  node.literals_.push_back(node.create_lit_(lb));

  // TODO(user): Not ideal, we should probably just provide index in the
  // original objective for sorting purpose.
  node.for_sorting_ = node.literals_[0].Variable();
  return node;
}

void EncodingNode::InitializeFullNode(int n, EncodingNode* a, EncodingNode* b,
                                      SatSolver* solver) {
  CHECK(literals_.empty()) << "Already initialized";
  CHECK_GT(n, 0);
  const BooleanVariable first_var_index(solver->NumVariables());
  solver->SetNumVariables(solver->NumVariables() + n);
  for (int i = 0; i < n; ++i) {
    literals_.push_back(Literal(first_var_index + i, true));
    if (i > 0) {
      solver->AddBinaryClause(literal(i - 1), literal(i).Negated());
    }
  }
  lb_ = a->lb_ + b->lb_;
  ub_ = lb_ + n;
  depth_ = 1 + std::max(a->depth_, b->depth_);
  child_a_ = a;
  child_b_ = b;
  for_sorting_ = first_var_index;
}

void EncodingNode::InitializeAmoNode(absl::Span<EncodingNode* const> nodes,
                                     SatSolver* solver) {
  CHECK_GE(nodes.size(), 2);
  CHECK(literals_.empty()) << "Already initialized";
  const BooleanVariable var = solver->NewBooleanVariable();
  const Literal new_literal(var, true);
  literals_.push_back(new_literal);
  child_a_ = nullptr;
  child_b_ = nullptr;
  lb_ = 0;
  ub_ = 1;
  depth_ = 0;
  for_sorting_ = var;
  std::vector<Literal> clause{new_literal.Negated()};
  for (const EncodingNode* node : nodes) {
    // node_lit => new_lit.
    clause.push_back(node->literals_[0]);
    solver->AddBinaryClause(node->literals_[0].Negated(), new_literal);
    depth_ = std::max(node->depth_ + 1, depth_);
    for_sorting_ = std::min(for_sorting_, node->for_sorting_);
  }

  // If new_literal is true then one of the lit must be true.
  // Note that this is not needed for correctness though.
  solver->AddProblemClause(clause);
}

void EncodingNode::InitializeLazyNode(EncodingNode* a, EncodingNode* b,
                                      SatSolver* solver) {
  CHECK(literals_.empty()) << "Already initialized";
  const BooleanVariable first_var_index(solver->NumVariables());
  solver->SetNumVariables(solver->NumVariables() + 1);
  literals_.emplace_back(first_var_index, true);
  child_a_ = a;
  child_b_ = b;
  ub_ = a->ub_ + b->ub_;
  lb_ = a->lb_ + b->lb_;
  depth_ = 1 + std::max(a->depth_, b->depth_);

  // Merging the node of the same depth in order seems to help a bit.
  for_sorting_ = std::min(a->for_sorting_, b->for_sorting_);
}

void EncodingNode::InitializeLazyCoreNode(Coefficient weight, EncodingNode* a,
                                          EncodingNode* b) {
  CHECK(literals_.empty()) << "Already initialized";
  child_a_ = a;
  child_b_ = b;
  ub_ = a->ub_ + b->ub_;
  weight_ = weight;
  weight_lb_ = a->lb_ + b->lb_;
  lb_ = weight_lb_ + 1;
  depth_ = 1 + std::max(a->depth_, b->depth_);

  // Merging the node of the same depth in order seems to help a bit.
  for_sorting_ = std::min(a->for_sorting_, b->for_sorting_);
}

bool EncodingNode::IncreaseCurrentUB(SatSolver* solver) {
  if (current_ub() == ub_) return false;
  if (create_lit_ != nullptr) {
    literals_.emplace_back(create_lit_(current_ub()));
  } else {
    CHECK_NE(solver, nullptr);
    literals_.emplace_back(BooleanVariable(solver->NumVariables()), true);
    solver->SetNumVariables(solver->NumVariables() + 1);
  }
  if (literals_.size() > 1) {
    solver->AddBinaryClause(literals_.back().Negated(),
                            literals_[literals_.size() - 2]);
  }
  return true;
}

Coefficient EncodingNode::Reduce(const SatSolver& solver) {
  if (!literals_.empty()) {
    int i = 0;
    while (i < literals_.size() &&
           solver.Assignment().LiteralIsTrue(literals_[i])) {
      ++i;
      ++lb_;
    }
    literals_.erase(literals_.begin(), literals_.begin() + i);
    while (!literals_.empty() &&
           solver.Assignment().LiteralIsFalse(literals_.back())) {
      literals_.pop_back();
      ub_ = lb_ + literals_.size();
    }
  }

  if (weight_lb_ >= lb_) return Coefficient(0);
  const Coefficient result = Coefficient(lb_ - weight_lb_) * weight_;
  weight_lb_ = lb_;
  return result;
}

void EncodingNode::ApplyWeightUpperBound(Coefficient gap, SatSolver* solver) {
  CHECK_GT(weight_, 0);
  const Coefficient num_allowed = (gap / weight_);
  if (num_allowed > std::numeric_limits<int>::max() / 2) return;
  const int new_size =
      std::max(0, (weight_lb_ - lb_) + static_cast<int>(num_allowed.value()));
  if (size() <= new_size) return;
  for (int i = new_size; i < size(); ++i) {
    if (!solver->AddUnitClause(literal(i).Negated())) return;
  }
  literals_.resize(new_size);
  ub_ = lb_ + new_size;
}

void EncodingNode::TransformToBoolean(SatSolver* solver) {
  if (size() > 1) {
    for (int i = 1; i < size(); ++i) {
      if (!solver->AddUnitClause(literal(i).Negated())) return;
    }
    literals_.resize(1);
    ub_ = lb_ + 1;
    return;
  }

  if (current_ub() == ub_) return;

  // TODO(user): Avoid creating a Boolean just to fix it!
  IncreaseNodeSize(this, solver);
  CHECK_EQ(size(), 2);
  if (!solver->AddUnitClause(literal(1).Negated())) return;
  literals_.resize(1);
  ub_ = lb_ + 1;
}

bool EncodingNode::AssumptionIs(Literal other) const {
  DCHECK(!HasNoWeight());
  const int index = weight_lb_ - lb_;
  return index < literals_.size() && literals_[index].Negated() == other;
}

Literal EncodingNode::GetAssumption(SatSolver* solver) {
  CHECK(!HasNoWeight());
  const int index = weight_lb_ - lb_;
  CHECK_GE(index, 0) << "Not reduced?";
  while (index >= literals_.size()) {
    IncreaseNodeSize(this, solver);
  }
  return literals_[index].Negated();
}

void EncodingNode::IncreaseWeightLb() {
  CHECK_LT(weight_lb_ - lb_, literals_.size());
  weight_lb_++;
}

bool EncodingNode::HasNoWeight() const {
  return weight_ == 0 || weight_lb_ >= ub_;
}

std::string EncodingNode::DebugString(
    const VariablesAssignment& assignment) const {
  std::string result;
  absl::StrAppend(&result, "depth:", depth_);
  absl::StrAppend(&result, " [", lb_, ",", lb_ + literals_.size(), "]");
  absl::StrAppend(&result, " ub:", ub_);
  absl::StrAppend(&result, " weight:", weight_.value());
  absl::StrAppend(&result, " weight_lb:", weight_lb_);
  absl::StrAppend(&result, " values:");
  const size_t limit = 20;
  int value = 0;
  for (int i = 0; i < std::min(literals_.size(), limit); ++i) {
    char c = '?';
    if (assignment.LiteralIsTrue(literals_[i])) {
      c = '1';
      value = i + 1;
    } else if (assignment.LiteralIsFalse(literals_[i])) {
      c = '0';
    }
    result += c;
  }
  absl::StrAppend(&result, " val:", lb_ + value);
  return result;
}

EncodingNode LazyMerge(EncodingNode* a, EncodingNode* b, SatSolver* solver) {
  EncodingNode n;
  n.InitializeLazyNode(a, b, solver);
  solver->AddBinaryClause(a->literal(0).Negated(), n.literal(0));
  solver->AddBinaryClause(b->literal(0).Negated(), n.literal(0));
  solver->AddTernaryClause(n.literal(0).Negated(), a->literal(0),
                           b->literal(0));
  return n;
}

void IncreaseNodeSize(EncodingNode* node, SatSolver* solver) {
  if (!node->IncreaseCurrentUB(solver)) return;
  std::vector<EncodingNode*> to_process;
  to_process.push_back(node);

  // Only one side of the constraint is mandatory (the one propagating the ones
  // to the top of the encoding tree), and it seems more efficient not to encode
  // the other side.
  //
  // TODO(user): Experiment more.
  const bool complete_encoding = false;

  while (!to_process.empty()) {
    EncodingNode* n = to_process.back();
    EncodingNode* a = n->child_a();
    EncodingNode* b = n->child_b();
    to_process.pop_back();

    // Integer leaf node.
    if (a == nullptr) continue;
    CHECK_NE(solver, nullptr);

    // Note that since we were able to increase its size, n must have children.
    // n->GreaterThan(target) is the new literal of n.
    CHECK(a != nullptr);
    CHECK(b != nullptr);
    const int target = n->current_ub() - 1;

    // Add a literal to a if needed.
    // That is, now that the node n can go up to it new current_ub, if we need
    // to increase the current_ub of a.
    if (a->current_ub() != a->ub()) {
      CHECK_GE(a->current_ub() - 1 + b->lb(), target - 1);
      if (a->current_ub() - 1 + b->lb() < target) {
        CHECK(a->IncreaseCurrentUB(solver));
        to_process.push_back(a);
      }
    }

    // Add a literal to b if needed.
    if (b->current_ub() != b->ub()) {
      CHECK_GE(b->current_ub() - 1 + a->lb(), target - 1);
      if (b->current_ub() - 1 + a->lb() < target) {
        CHECK(b->IncreaseCurrentUB(solver));
        to_process.push_back(b);
      }
    }

    // Wire the new literal of n correctly with its two children.
    for (int ia = a->lb(); ia < a->current_ub(); ++ia) {
      const int ib = target - ia;
      if (complete_encoding && ib >= b->lb() && ib < b->current_ub()) {
        // if x <= ia and y <= ib then x + y <= ia + ib.
        solver->AddTernaryClause(n->GreaterThan(target).Negated(),
                                 a->GreaterThan(ia), b->GreaterThan(ib));
      }
      if (complete_encoding && ib == b->ub()) {
        solver->AddBinaryClause(n->GreaterThan(target).Negated(),
                                a->GreaterThan(ia));
      }

      if (ib - 1 == b->lb() - 1) {
        solver->AddBinaryClause(n->GreaterThan(target),
                                a->GreaterThan(ia).Negated());
      }
      if ((ib - 1) >= b->lb() && (ib - 1) < b->current_ub()) {
        // if x > ia and y > ib - 1 then x + y > ia + ib.
        solver->AddTernaryClause(n->GreaterThan(target),
                                 a->GreaterThan(ia).Negated(),
                                 b->GreaterThan(ib - 1).Negated());
      }
    }

    // Case ia = a->lb() - 1; a->GreaterThan(ia) always true.
    {
      const int ib = target - (a->lb() - 1);
      if ((ib - 1) == b->lb() - 1) {
        if (!solver->AddUnitClause(n->GreaterThan(target))) return;
      }
      if ((ib - 1) >= b->lb() && (ib - 1) < b->current_ub()) {
        solver->AddBinaryClause(n->GreaterThan(target),
                                b->GreaterThan(ib - 1).Negated());
      }
    }

    // case ia == a->ub; a->GreaterThan(ia) always false.
    {
      const int ib = target - a->ub();
      if (complete_encoding && ib >= b->lb() && ib < b->current_ub()) {
        solver->AddBinaryClause(n->GreaterThan(target).Negated(),
                                b->GreaterThan(ib));
      }
      if (ib == b->ub()) {
        if (!solver->AddUnitClause(n->GreaterThan(target).Negated())) return;
      }
    }
  }
}

EncodingNode FullMerge(Coefficient upper_bound, EncodingNode* a,
                       EncodingNode* b, SatSolver* solver) {
  EncodingNode n;
  const int size =
      std::min(Coefficient(a->size() + b->size()), upper_bound).value();
  n.InitializeFullNode(size, a, b, solver);
  for (int ia = 0; ia < a->size(); ++ia) {
    if (ia + b->size() < size) {
      solver->AddBinaryClause(n.literal(ia + b->size()).Negated(),
                              a->literal(ia));
    }
    if (ia < size) {
      solver->AddBinaryClause(n.literal(ia), a->literal(ia).Negated());
    } else {
      // Fix the variable to false because of the given upper_bound.
      if (!solver->AddUnitClause(a->literal(ia).Negated())) return n;
    }
  }
  for (int ib = 0; ib < b->size(); ++ib) {
    if (ib + a->size() < size) {
      solver->AddBinaryClause(n.literal(ib + a->size()).Negated(),
                              b->literal(ib));
    }
    if (ib < size) {
      solver->AddBinaryClause(n.literal(ib), b->literal(ib).Negated());
    } else {
      // Fix the variable to false because of the given upper_bound.
      if (!solver->AddUnitClause(b->literal(ib).Negated())) return n;
    }
  }
  for (int ia = 0; ia < a->size(); ++ia) {
    for (int ib = 0; ib < b->size(); ++ib) {
      if (ia + ib < size) {
        // if x <= ia and y <= ib, then x + y <= ia + ib.
        solver->AddTernaryClause(n.literal(ia + ib).Negated(), a->literal(ia),
                                 b->literal(ib));
      }
      if (ia + ib + 1 < size) {
        // if x > ia and y > ib, then x + y > ia + ib + 1.
        solver->AddTernaryClause(n.literal(ia + ib + 1),
                                 a->literal(ia).Negated(),
                                 b->literal(ib).Negated());
      } else {
        solver->AddBinaryClause(a->literal(ia).Negated(),
                                b->literal(ib).Negated());
      }
    }
  }
  return n;
}

EncodingNode* MergeAllNodesWithDeque(Coefficient upper_bound,
                                     const std::vector<EncodingNode*>& nodes,
                                     SatSolver* solver,
                                     std::deque<EncodingNode>* repository) {
  std::deque<EncodingNode*> dq(nodes.begin(), nodes.end());
  while (dq.size() > 1) {
    EncodingNode* a = dq.front();
    dq.pop_front();
    EncodingNode* b = dq.front();
    dq.pop_front();
    repository->push_back(FullMerge(upper_bound, a, b, solver));
    dq.push_back(&repository->back());
  }
  return dq.front();
}

namespace {
struct SortEncodingNodePointers {
  bool operator()(EncodingNode* a, EncodingNode* b) const { return *a < *b; }
};
}  // namespace

EncodingNode* LazyMergeAllNodeWithPQAndIncreaseLb(
    Coefficient weight, const std::vector<EncodingNode*>& nodes,
    SatSolver* solver, std::deque<EncodingNode>* repository) {
  std::priority_queue<EncodingNode*, std::vector<EncodingNode*>,
                      SortEncodingNodePointers>
      pq(nodes.begin(), nodes.end());
  while (pq.size() > 2) {
    EncodingNode* a = pq.top();
    pq.pop();
    EncodingNode* b = pq.top();
    pq.pop();
    repository->push_back(LazyMerge(a, b, solver));
    pq.push(&repository->back());
  }

  CHECK_EQ(pq.size(), 2);
  EncodingNode* a = pq.top();
  pq.pop();
  EncodingNode* b = pq.top();
  pq.pop();

  repository->push_back(EncodingNode());
  EncodingNode* n = &repository->back();
  n->InitializeLazyCoreNode(weight, a, b);
  solver->AddBinaryClause(a->literal(0), b->literal(0));
  return n;
}

namespace {

bool EncodingNodeByWeight(const EncodingNode* a, const EncodingNode* b) {
  return a->weight() < b->weight();
}

bool EncodingNodeByDepth(const EncodingNode* a, const EncodingNode* b) {
  return a->depth() < b->depth();
}

}  // namespace

void ReduceNodes(Coefficient upper_bound, Coefficient* lower_bound,
                 std::vector<EncodingNode*>* nodes, SatSolver* solver) {
  // Remove the left-most variables fixed to one from each node.
  // Also update the lower_bound. Note that Reduce() needs the solver to be
  // at the root node in order to work.
  solver->Backtrack(0);
  for (EncodingNode* n : *nodes) {
    *lower_bound += n->Reduce(*solver);
  }

  // Fix the nodes right-most variables that are above the gap.
  // If we closed the problem, we abort and return and empty vector.
  if (upper_bound != kCoefficientMax) {
    const Coefficient gap = upper_bound - *lower_bound;
    if (gap < 0) {
      nodes->clear();
      return;
    }
    for (EncodingNode* n : *nodes) {
      n->ApplyWeightUpperBound(gap, solver);
    }
  }

  // Remove the empty nodes.
  nodes->erase(std::remove_if(nodes->begin(), nodes->end(),
                              [](EncodingNode* a) { return a->HasNoWeight(); }),
               nodes->end());

  // Sort the nodes.
  switch (solver->parameters().max_sat_assumption_order()) {
    case SatParameters::DEFAULT_ASSUMPTION_ORDER:
      break;
    case SatParameters::ORDER_ASSUMPTION_BY_DEPTH:
      std::sort(nodes->begin(), nodes->end(), EncodingNodeByDepth);
      break;
    case SatParameters::ORDER_ASSUMPTION_BY_WEIGHT:
      std::sort(nodes->begin(), nodes->end(), EncodingNodeByWeight);
      break;
  }
  if (solver->parameters().max_sat_reverse_assumption_order()) {
    // TODO(user): with DEFAULT_ASSUMPTION_ORDER, this will lead to a somewhat
    // weird behavior, since we will reverse the nodes at each iteration...
    std::reverse(nodes->begin(), nodes->end());
  }
}

std::vector<Literal> ExtractAssumptions(Coefficient stratified_lower_bound,
                                        const std::vector<EncodingNode*>& nodes,
                                        SatSolver* solver) {
  // Extract the assumptions from the nodes.
  std::vector<Literal> assumptions;
  for (EncodingNode* n : nodes) {
    if (n->weight() >= stratified_lower_bound) {
      assumptions.push_back(n->GetAssumption(solver));
    }
  }
  return assumptions;
}

Coefficient ComputeCoreMinWeight(const std::vector<EncodingNode*>& nodes,
                                 absl::Span<const Literal> core) {
  Coefficient min_weight = kCoefficientMax;
  int index = 0;
  for (int i = 0; i < core.size(); ++i) {
    for (; index < nodes.size() && !nodes[index]->AssumptionIs(core[i]);
         ++index) {
    }
    CHECK_LT(index, nodes.size());
    min_weight = std::min(min_weight, nodes[index]->weight());
  }
  return min_weight;
}

Coefficient MaxNodeWeightSmallerThan(const std::vector<EncodingNode*>& nodes,
                                     Coefficient upper_bound) {
  Coefficient result(0);
  for (EncodingNode* n : nodes) {
    CHECK_GT(n->weight(), 0);
    if (n->weight() < upper_bound) {
      result = std::max(result, n->weight());
    }
  }
  return result;
}

bool ObjectiveEncoder::ProcessCore(absl::Span<const Literal> core,
                                   Coefficient min_weight, Coefficient gap,
                                   std::string* info) {
  // Backtrack to be able to add new constraints.
  if (!sat_solver_->ResetToLevelZero()) return false;
  if (core.size() == 1) {
    return sat_solver_->AddUnitClause(core[0].Negated());
  }

  // Remove from nodes the EncodingNode in the core and put them in to_merge.
  std::vector<EncodingNode*> to_merge;
  {
    int index = 0;
    std::vector<EncodingNode*> new_nodes;
    for (int i = 0; i < core.size(); ++i) {
      // Since the nodes appear in order in the core, we can find the
      // relevant "objective" variable efficiently with a simple linear scan
      // in the nodes vector (done with index).
      for (; !nodes_[index]->AssumptionIs(core[i]); ++index) {
        CHECK_LT(index, nodes_.size());
        new_nodes.push_back(nodes_[index]);
      }
      CHECK_LT(index, nodes_.size());
      EncodingNode* node = nodes_[index];

      // TODO(user): propagate proper ub first.
      if (alternative_encoding_ && node->ub() > node->lb() + 1) {
        // We can distinguish the first literal of the node.
        // By not counting its weight in node, and creating a new node for it.
        node->IncreaseWeightLb();
        new_nodes.push_back(node);

        // Now keep processing with this new node instead.
        repository_.push_back(
            EncodingNode::LiteralNode(core[i].Negated(), node->weight()));
        repository_.back().set_depth(node->depth());
        node = &repository_.back();
      }

      to_merge.push_back(node);

      // Special case if the weight > min_weight. we keep it, but reduce its
      // cost. This is the same "trick" as in WPM1 used to deal with weight.
      // We basically split a clause with a larger weight in two identical
      // clauses, one with weight min_weight that will be merged and one with
      // the remaining weight.
      if (node->weight() > min_weight) {
        node->set_weight(node->weight() - min_weight);
        new_nodes.push_back(node);
      }
      ++index;
    }
    for (; index < nodes_.size(); ++index) {
      new_nodes.push_back(nodes_[index]);
    }
    nodes_ = new_nodes;
  }

  // Are the literal in amo relationship?
  // - If min_weight is large enough, we can infer that.
  // - If the size is small we can infer this via propagation.
  bool in_exactly_one = (2 * min_weight) > gap;

  // Amongst the node to merge, if many are boolean nodes in an "at most one"
  // relationship, it is super advantageous to exploit it during merging as we
  // can regroup all nodes from an at most one in a single new node with a depth
  // of 1.
  if (!in_exactly_one) {
    // Collect "boolean nodes".
    std::vector<Literal> bool_nodes;
    absl::flat_hash_map<LiteralIndex, int> node_indices;
    for (int i = 0; i < to_merge.size(); ++i) {
      const EncodingNode& node = *to_merge[i];

      if (node.size() != 1) continue;
      if (node.ub() != node.lb() + 1) continue;
      if (node.weight_lb() != node.lb()) continue;
      if (node_indices.contains(node.literal(0).Index())) continue;
      node_indices[node.literal(0).Index()] = i;
      bool_nodes.push_back(node.literal(0));
    }

    // For "small" core, with O(n) full propagation, we can discover possible
    // at most ones. This is a bit costly but can significantly reduce the
    // number of Booleans needed and has a good positive impact.
    std::vector<int> buffer;
    std::vector<absl::Span<const Literal>> decomposition;
    if (params_.core_minimization_level() > 1 && bool_nodes.size() < 300 &&
        bool_nodes.size() > 1) {
      const auto& assignment = sat_solver_->Assignment();
      const int size = bool_nodes.size();
      std::vector<std::vector<int>> graph(size);
      for (int i = 0; i < size; ++i) {
        if (!sat_solver_->ResetToLevelZero()) return false;
        if (!sat_solver_->EnqueueDecisionIfNotConflicting(bool_nodes[i])) {
          // TODO(user): this node is closed and can be removed from the core.
          continue;
        }
        for (int j = 0; j < size; ++j) {
          if (i == j) continue;
          if (assignment.LiteralIsFalse(bool_nodes[j])) {
            graph[i].push_back(j);

            // Unit propagation is not always symmetric.
            graph[j].push_back(i);
          }

          // TODO(user): If assignment.LiteralIsTrue(bool_nodes[j]) We can
          // minimize the core here by removing bool_nodes[i] from it. Note
          // however that since we already minimized the core, this is
          // unlikely to happen.
        }
      }
      if (!sat_solver_->ResetToLevelZero()) return false;

      for (std::vector<int>& adj : graph) {
        gtl::STLSortAndRemoveDuplicates(&adj);
      }
      const std::vector<absl::Span<int>> index_decompo =
          AtMostOneDecomposition(graph, *random_, &buffer);

      // Convert.
      std::vector<Literal> new_order;
      for (const int i : buffer) new_order.push_back(bool_nodes[i]);
      bool_nodes = new_order;
      for (const auto span : index_decompo) {
        if (span.size() == 1) continue;
        decomposition.push_back(absl::MakeSpan(
            bool_nodes.data() + (span.data() - buffer.data()), span.size()));
      }
    } else {
      decomposition = implications_->HeuristicAmoPartition(&bool_nodes);
    }

    // Same case as above, all the nodes in the core are in a exactly_one.
    if (decomposition.size() == 1 && decomposition[0].size() == core.size()) {
      in_exactly_one = true;
    }

    int num_in_decompo = 0;
    if (!in_exactly_one) {
      for (const auto amo : decomposition) {
        num_in_decompo += amo.size();

        // Extract Amo nodes and set them to nullptr in to_merge.
        std::vector<EncodingNode*> amo_nodes;
        for (const Literal l : amo) {
          const int index = node_indices.at(l.Index());
          amo_nodes.push_back(to_merge[index]);
          to_merge[index] = nullptr;
        }

        // Create the new node with proper constraints and weight.
        repository_.push_back(EncodingNode());
        EncodingNode* n = &repository_.back();
        n->InitializeAmoNode(amo_nodes, sat_solver_);
        n->set_weight(min_weight);
        to_merge.push_back(n);
      }
      if (num_in_decompo > 0) {
        absl::StrAppend(info, " amo:", decomposition.size(),
                        " lit:", num_in_decompo);
      }

      // Clean-up to_merge.
      int new_size = 0;
      for (EncodingNode* node : to_merge) {
        if (node == nullptr) continue;
        to_merge[new_size++] = node;
      }
      to_merge.resize(new_size);
    }
  }

  // If all the literal of the core are in at_most_one, the core is actually an
  // exactly_one. We just subtracted the min_cost above. We just have to enqueue
  // a constant node with min_weight for the rest of the code to work.
  if (in_exactly_one) {
    // Tricky: We need to enforce an upper bound of 1 on the nodes.
    for (EncodingNode* node : to_merge) {
      node->TransformToBoolean(sat_solver_);
    }

    absl::StrAppend(info, " exo");
    repository_.push_back(EncodingNode::ConstantNode(min_weight));
    nodes_.push_back(&repository_.back());

    // The negation of the literal in the core are in exactly one.
    // TODO(user): If we infered the exactly one from the binary implication
    // graph, there is no need to add the amo since it is already there.
    std::vector<LiteralWithCoeff> cst;
    cst.reserve(core.size());
    for (const Literal l : core) {
      cst.emplace_back(l.Negated(), Coefficient(1));
    }
    sat_solver_->AddLinearConstraint(
        /*use_lower_bound=*/true, Coefficient(1),
        /*use_upper_bound=*/true, Coefficient(1), &cst);
    return !sat_solver_->ModelIsUnsat();
  }

  nodes_.push_back(LazyMergeAllNodeWithPQAndIncreaseLb(
      min_weight, to_merge, sat_solver_, &repository_));
  absl::StrAppend(info, " d:", nodes_.back()->depth());
  return !sat_solver_->ModelIsUnsat();
}

}  // namespace sat
}  // namespace operations_research
