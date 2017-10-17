// Copyright 2010-2017 Google
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
#include <deque>
#include <memory>
#include <queue>

#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {

EncodingNode::EncodingNode(Literal l)
    : depth_(0),
      lb_(0),
      ub_(1),
      for_sorting_(l.Variable()),
      child_a_(nullptr),
      child_b_(nullptr),
      literals_(1, l) {}

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

bool EncodingNode::IncreaseCurrentUB(SatSolver* solver) {
  CHECK(!literals_.empty());
  if (current_ub() == ub_) return false;
  literals_.emplace_back(BooleanVariable(solver->NumVariables()), true);
  solver->SetNumVariables(solver->NumVariables() + 1);
  solver->AddBinaryClause(literals_.back().Negated(),
                          literals_[literals_.size() - 2]);
  return true;
}

int EncodingNode::Reduce(const SatSolver& solver) {
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
  return i;
}

void EncodingNode::ApplyUpperBound(int64 upper_bound, SatSolver* solver) {
  if (size() <= upper_bound) return;
  for (int i = upper_bound; i < size(); ++i) {
    solver->AddUnitClause(literal(i).Negated());
  }
  literals_.resize(upper_bound);
  ub_ = lb_ + literals_.size();
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

    // Note that since we were able to increase its size, n must have children.
    // n->GreaterThan(target) is the new literal of n.
    CHECK(a != nullptr);
    CHECK(b != nullptr);
    CHECK_GE(n->size(), 2);
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

EncodingNode* LazyMergeAllNodeWithPQ(const std::vector<EncodingNode*>& nodes,
                                     SatSolver* solver,
                                     std::deque<EncodingNode>* repository) {
  std::priority_queue<EncodingNode*, std::vector<EncodingNode*>,
                      SortEncodingNodePointers>
      pq(nodes.begin(), nodes.end());
  while (pq.size() > 1) {
    EncodingNode* a = pq.top();
    pq.pop();
    EncodingNode* b = pq.top();
    pq.pop();
    repository->push_back(LazyMerge(a, b, solver));
    pq.push(&repository->back());
  }
  return pq.top();
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

bool EmptyEncodingNode(const EncodingNode* a) { return a->size() == 0; }

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
    *lower_bound += n->Reduce(*solver) * n->weight();
  }

  // Fix the nodes right-most variables that are above the gap.
  if (upper_bound != kCoefficientMax) {
    const Coefficient gap = upper_bound - *lower_bound;
    if (gap <= 0) return {};
    for (EncodingNode* n : *nodes) {
      n->ApplyUpperBound((gap / n->weight()).value(), solver);
    }
  }

  // Remove the empty nodes.
  nodes->erase(std::remove_if(nodes->begin(), nodes->end(), EmptyEncodingNode),
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
      assumptions.push_back(n->literal(0).Negated());
    }
  }
  return assumptions;
}

Coefficient ComputeCoreMinWeight(const std::vector<EncodingNode*>& nodes,
                                 const std::vector<Literal>& core) {
  Coefficient min_weight = kCoefficientMax;
  int index = 0;
  for (int i = 0; i < core.size(); ++i) {
    for (;
         index < nodes.size() && nodes[index]->literal(0).Negated() != core[i];
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

void ProcessCore(const std::vector<Literal>& core, Coefficient min_weight,
                 std::deque<EncodingNode>* repository,
                 std::vector<EncodingNode*>* nodes, SatSolver* solver) {
  // Backtrack to be able to add new constraints.
  solver->Backtrack(0);

  if (core.size() == 1) {
    // The core will be reduced at the beginning of the next loop.
    // Find the associated node, and call IncreaseNodeSize() on it.
    CHECK(solver->Assignment().LiteralIsFalse(core[0]));
    for (EncodingNode* n : *nodes) {
      if (n->literal(0).Negated() == core[0]) {
        IncreaseNodeSize(n, solver);
        return;
      }
    }
    LOG(FATAL) << "Node with literal " << core[0] << " not found!";
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
    for (; (*nodes)[index]->literal(0).Negated() != core[i]; ++index) {
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
  nodes->push_back(LazyMergeAllNodeWithPQ(to_merge, solver, repository));
  IncreaseNodeSize(nodes->back(), solver);
  nodes->back()->set_weight(min_weight);
  CHECK(solver->AddUnitClause(nodes->back()->literal(0)));
}

}  // namespace sat
}  // namespace operations_research
