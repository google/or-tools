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

#include "ortools/sat/encoding.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <queue>
#include <string>
#include <vector>

#include "ortools/base/logging.h"
#include "ortools/sat/boolean_problem.pb.h"
#include "ortools/sat/pb_constraint.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

EncodingNode::EncodingNode(Literal l)
    : for_sorting_(l.Variable()), literals_(1, l) {}

EncodingNode::EncodingNode(int lb, int ub,
                           std::function<Literal(int x)> create_lit)
    : lb_(lb), ub_(ub), create_lit_(create_lit) {
  CHECK_LT(lb, ub);
  literals_.push_back(create_lit(lb));

  // TODO(user): Not ideal, we should probably just provide index in the
  // original objective for sorting purpose.
  for_sorting_ = literals_[0].Variable();
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

  if (weight_lb_ >= lb_) return Coefficient(0);
  const Coefficient result = Coefficient(lb_ - weight_lb_) * weight_;
  weight_lb_ = lb_;
  return result;
}

void EncodingNode::ApplyWeightUpperBound(Coefficient gap, SatSolver* solver) {
  CHECK_GT(weight_, 0);
  const Coefficient num_allowed = (gap / weight_);
  const Coefficient new_size =
      std::max(Coefficient(0), Coefficient(weight_lb_ - lb_) + num_allowed);
  if (size() <= new_size) return;
  for (int i = new_size.value(); i < size(); ++i) {
    solver->AddUnitClause(literal(i).Negated());
  }
  literals_.resize(new_size.value());
  ub_ = lb_ + literals_.size();
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
        solver->AddUnitClause(n->GreaterThan(target));
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
        solver->AddUnitClause(n->GreaterThan(target).Negated());
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
      solver->AddUnitClause(a->literal(ia).Negated());
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
      solver->AddUnitClause(b->literal(ib).Negated());
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

std::vector<EncodingNode*> CreateInitialEncodingNodes(
    const std::vector<Literal>& literals,
    const std::vector<Coefficient>& coeffs, Coefficient* offset,
    std::deque<EncodingNode>* repository) {
  CHECK_EQ(literals.size(), coeffs.size());
  *offset = 0;
  std::vector<EncodingNode*> nodes;
  for (int i = 0; i < literals.size(); ++i) {
    // We want to maximize the cost when this literal is true.
    if (coeffs[i] > 0) {
      repository->emplace_back(literals[i]);
      nodes.push_back(&repository->back());
      nodes.back()->set_weight(coeffs[i]);
    } else {
      repository->emplace_back(literals[i].Negated());
      nodes.push_back(&repository->back());
      nodes.back()->set_weight(-coeffs[i]);

      // Note that this increase the offset since the coeff is negative.
      *offset -= coeffs[i];
    }
  }
  return nodes;
}

std::vector<EncodingNode*> CreateInitialEncodingNodes(
    const LinearObjective& objective_proto, Coefficient* offset,
    std::deque<EncodingNode>* repository) {
  *offset = 0;
  std::vector<EncodingNode*> nodes;
  for (int i = 0; i < objective_proto.literals_size(); ++i) {
    const Literal literal(objective_proto.literals(i));

    // We want to maximize the cost when this literal is true.
    if (objective_proto.coefficients(i) > 0) {
      repository->emplace_back(literal);
      nodes.push_back(&repository->back());
      nodes.back()->set_weight(Coefficient(objective_proto.coefficients(i)));
    } else {
      repository->emplace_back(literal.Negated());
      nodes.push_back(&repository->back());
      nodes.back()->set_weight(Coefficient(-objective_proto.coefficients(i)));

      // Note that this increase the offset since the coeff is negative.
      *offset -= objective_proto.coefficients(i);
    }
  }
  return nodes;
}

namespace {

bool EncodingNodeByWeight(const EncodingNode* a, const EncodingNode* b) {
  return a->weight() < b->weight();
}

bool EncodingNodeByDepth(const EncodingNode* a, const EncodingNode* b) {
  return a->depth() < b->depth();
}

}  // namespace

std::vector<Literal> ReduceNodesAndExtractAssumptions(
    Coefficient upper_bound, Coefficient stratified_lower_bound,
    Coefficient* lower_bound, std::vector<EncodingNode*>* nodes,
    SatSolver* solver) {
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
    if (gap < 0) return {};
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

  // Extract the assumptions from the nodes.
  std::vector<Literal> assumptions;
  for (EncodingNode* n : *nodes) {
    if (n->weight() >= stratified_lower_bound) {
      assumptions.push_back(n->GetAssumption(solver));
    }
  }
  return assumptions;
}

Coefficient ComputeCoreMinWeight(const std::vector<EncodingNode*>& nodes,
                                 const std::vector<Literal>& core) {
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

bool ProcessCore(const std::vector<Literal>& core, Coefficient min_weight,
                 std::deque<EncodingNode>* repository,
                 std::vector<EncodingNode*>* nodes, SatSolver* solver) {
  // Backtrack to be able to add new constraints.
  solver->ResetToLevelZero();
  if (core.size() == 1) {
    return solver->AddUnitClause(core[0].Negated());
  }

  // Remove from nodes the EncodingNode in the core, merge them, and add the
  // resulting EncodingNode at the back.
  int index = 0;
  int new_node_index = 0;
  std::vector<EncodingNode*> to_merge;
  for (int i = 0; i < core.size(); ++i) {
    // Since the nodes appear in order in the core, we can find the
    // relevant "objective" variable efficiently with a simple linear scan
    // in the nodes vector (done with index).
    for (; !(*nodes)[index]->AssumptionIs(core[i]); ++index) {
      CHECK_LT(index, nodes->size());
      (*nodes)[new_node_index] = (*nodes)[index];
      ++new_node_index;
    }
    CHECK_LT(index, nodes->size());
    to_merge.push_back((*nodes)[index]);

    // Special case if the weight > min_weight. we keep it, but reduce its
    // cost. This is the same "trick" as in WPM1 used to deal with weight.
    // We basically split a clause with a larger weight in two identical
    // clauses, one with weight min_weight that will be merged and one with
    // the remaining weight.
    if ((*nodes)[index]->weight() > min_weight) {
      (*nodes)[index]->set_weight((*nodes)[index]->weight() - min_weight);
      (*nodes)[new_node_index] = (*nodes)[index];
      ++new_node_index;
    }
    ++index;
  }
  for (; index < nodes->size(); ++index) {
    (*nodes)[new_node_index] = (*nodes)[index];
    ++new_node_index;
  }
  nodes->resize(new_node_index);
  nodes->push_back(LazyMergeAllNodeWithPQAndIncreaseLb(min_weight, to_merge,
                                                       solver, repository));
  return !solver->IsModelUnsat();
}

bool ProcessCoreWithAlternativeEncoding(const std::vector<Literal>& core,
                                        Coefficient min_weight,
                                        std::deque<EncodingNode>* repository,
                                        std::vector<EncodingNode*>* nodes,
                                        SatSolver* solver) {
  // Backtrack to be able to add new constraints.
  solver->ResetToLevelZero();

  if (core.size() == 1) {
    return solver->AddUnitClause(core[0].Negated());
  }

  std::vector<EncodingNode*> new_nodes;
  std::vector<EncodingNode*> to_merge;

  // Preconditions.
  for (EncodingNode* n : *nodes) {
    CHECK_GT(n->size(), 0);
  }

  // Remove from nodes the EncodingNode in the core, merge them, and add the
  // resulting EncodingNode at the back.
  int index = 0;
  for (int i = 0; i < core.size(); ++i) {
    // Since the nodes appear in order in the core, we can find the
    // relevant "objective" variable efficiently with a simple linear scan
    // in the nodes vector (done with index).
    CHECK_LT(index, nodes->size());
    for (; !(*nodes)[index]->AssumptionIs(core[i]); ++index) {
      CHECK_LT(index, nodes->size());
      new_nodes.push_back((*nodes)[index]);
    }
    CHECK_LT(index, nodes->size());

    // We have a node from the core.
    // We will distinguish its first literal.
    EncodingNode* n = (*nodes)[index];
    const Literal lit = core[i].Negated();
    n->IncreaseWeightLb();
    ++index;
    CHECK_GT(n->size(), 0);

    // TODO(user): For node with same depth, the sorting order is not the same
    // if we create a new node or reuse one. Experiment what is the best order.
    repository->emplace_back(lit);
    EncodingNode* new_bool_node = &repository->back();
    new_bool_node->set_depth(n->depth());
    CHECK_GT(new_bool_node->size(), 0);
    to_merge.push_back(new_bool_node);
    if (n->weight() > min_weight) {
      new_bool_node->set_weight(n->weight() - min_weight);
      new_nodes.push_back(new_bool_node);
    }

    if (!n->HasNoWeight()) {
      new_nodes.push_back(n);
    }
  }

  for (; index < nodes->size(); ++index) {
    new_nodes.push_back((*nodes)[index]);
  }

  new_nodes.push_back(LazyMergeAllNodeWithPQAndIncreaseLb(min_weight, to_merge,
                                                          solver, repository));
  *nodes = new_nodes;
  return !solver->IsModelUnsat();
}

}  // namespace sat
}  // namespace operations_research
