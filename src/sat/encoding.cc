// Copyright 2010-2013 Google
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
#include "sat/encoding.h"

#include <deque>
#include <queue>

namespace operations_research {
namespace sat {

EncodingNode::EncodingNode(Literal l)
    : depth_(0),
      lb_(0),
      ub_(1),
      child_a_(nullptr),
      child_b_(nullptr),
      literals_(1, l),
      for_sorting_(l.Variable()) {}

void EncodingNode::InitializeFullNode(int n, EncodingNode* a, EncodingNode* b,
                                      SatSolver* solver) {
  CHECK(literals_.empty()) << "Already initialized";
  CHECK_GT(n, 0);
  const VariableIndex first_var_index(solver->NumVariables());
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
  const VariableIndex first_var_index(solver->NumVariables());
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
  literals_.emplace_back(VariableIndex(solver->NumVariables()), true);
  solver->SetNumVariables(solver->NumVariables() + 1);
  solver->AddBinaryClause(literals_.back().Negated(),
                          literals_[literals_.size() - 2]);
  return true;
}

int EncodingNode::Reduce(const SatSolver& solver) {
  int i = 0;
  while (i < literals_.size() &&
         solver.Assignment().IsLiteralTrue(literals_[i])) {
    ++i;
    ++lb_;
  }
  literals_.erase(literals_.begin(), literals_.begin() + i);
  while (!literals_.empty() &&
         solver.Assignment().IsLiteralFalse(literals_.back())) {
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

EncodingNode* LazyMerge(EncodingNode* a, EncodingNode* b, SatSolver* solver) {
  EncodingNode* n = new EncodingNode();
  n->InitializeLazyNode(a, b, solver);
  solver->AddBinaryClause(a->literal(0).Negated(), n->literal(0));
  solver->AddBinaryClause(b->literal(0).Negated(), n->literal(0));
  solver->AddTernaryClause(n->literal(0).Negated(), a->literal(0),
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

EncodingNode* FullMerge(Coefficient upper_bound, EncodingNode* a,
                        EncodingNode* b, SatSolver* solver) {
  EncodingNode* n = new EncodingNode();
  const int size = std::min(Coefficient(a->size() + b->size()), upper_bound).value();
  n->InitializeFullNode(size, a, b, solver);
  for (int ia = 0; ia < a->size(); ++ia) {
    if (ia + b->size() < size) {
      solver->AddBinaryClause(n->literal(ia + b->size()).Negated(),
                              a->literal(ia));
    }
    if (ia < size) {
      solver->AddBinaryClause(n->literal(ia), a->literal(ia).Negated());
    } else {
      // Fix the variable to false because of the given upper_bound.
      solver->AddUnitClause(a->literal(ia).Negated());
    }
  }
  for (int ib = 0; ib < b->size(); ++ib) {
    if (ib + a->size() < size) {
      solver->AddBinaryClause(n->literal(ib + a->size()).Negated(),
                              b->literal(ib));
    }
    if (ib < size) {
      solver->AddBinaryClause(n->literal(ib), b->literal(ib).Negated());
    } else {
      // Fix the variable to false because of the given upper_bound.
      solver->AddUnitClause(b->literal(ib).Negated());
    }
  }
  for (int ia = 0; ia < a->size(); ++ia) {
    for (int ib = 0; ib < b->size(); ++ib) {
      if (ia + ib < size) {
        // if x <= ia and y <= ib, then x + y <= ia + ib.
        solver->AddTernaryClause(n->literal(ia + ib).Negated(), a->literal(ia),
                                 b->literal(ib));
      }
      if (ia + ib + 1 < size) {
        // if x > ia and y > ib, then x + y > ia + ib + 1.
        solver->AddTernaryClause(n->literal(ia + ib + 1),
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

EncodingNode* MergeAllNodesWithDeque(
    Coefficient upper_bound, const std::vector<EncodingNode*>& nodes,
    SatSolver* solver, std::vector<std::unique_ptr<EncodingNode>>* repository) {
  std::deque<EncodingNode*> dq(nodes.begin(), nodes.end());
  while (dq.size() > 1) {
    EncodingNode* a = dq.front();
    dq.pop_front();
    EncodingNode* b = dq.front();
    dq.pop_front();
    repository->emplace_back(FullMerge(upper_bound, a, b, solver));
    dq.push_back(repository->back().get());
  }
  return dq.front();
}

namespace {
struct SortEncodingNodePointers {
  bool operator()(EncodingNode* a, EncodingNode* b) const { return *a < *b; }
};
}  // namespace

EncodingNode* LazyMergeAllNodeWithPQ(
    const std::vector<EncodingNode*>& nodes, SatSolver* solver,
    std::vector<std::unique_ptr<EncodingNode>>* repository) {
  std::priority_queue<EncodingNode*, std::vector<EncodingNode*>, SortEncodingNodePointers>
      pq(nodes.begin(), nodes.end());
  while (pq.size() > 1) {
    EncodingNode* a = pq.top();
    pq.pop();
    EncodingNode* b = pq.top();
    pq.pop();
    repository->emplace_back(LazyMerge(a, b, solver));
    pq.push(repository->back().get());
  }
  return pq.top();
}

std::vector<EncodingNode*> CreateInitialEncodingNodes(
    const LinearObjective& objective_proto, Coefficient* offset,
    std::vector<std::unique_ptr<EncodingNode>>* repository) {
  *offset = 0;
  std::vector<EncodingNode*> nodes;
  for (int i = 0; i < objective_proto.literals_size(); ++i) {
    const Literal literal(objective_proto.literals(i));

    // We want to maximize the cost when this literal is true.
    if (objective_proto.coefficients(i) > 0) {
      repository->emplace_back(new EncodingNode(literal));
      nodes.push_back(repository->back().get());
      nodes.back()->set_weight(Coefficient(objective_proto.coefficients(i)));
    } else {
      repository->emplace_back(new EncodingNode(literal.Negated()));
      nodes.push_back(repository->back().get());
      nodes.back()->set_weight(Coefficient(-objective_proto.coefficients(i)));

      // Note that this increase the offset since the coeff is negative.
      *offset -= objective_proto.coefficients(i);
    }
  }
  return nodes;
}

}  // namespace sat
}  // namespace operations_research
