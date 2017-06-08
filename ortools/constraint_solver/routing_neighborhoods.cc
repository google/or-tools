// Copyright 2010-2014 Google
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

#include "ortools/constraint_solver/routing_neighborhoods.h"

namespace operations_research {

MakeRelocateNeighborsOperator::MakeRelocateNeighborsOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    RoutingTransitEvaluator2 arc_evaluator)
    : PathWithPreviousNodesOperator(vars, secondary_vars, 2,
                                    std::move(start_empty_path_class)),
      arc_evaluator_(std::move(arc_evaluator)) {}

bool MakeRelocateNeighborsOperator::MakeNeighbor() {
  const int64 before_chain = BaseNode(0);
  if (IsPathEnd(before_chain)) {
    return false;
  }
  int64 chain_end = Next(before_chain);
  if (IsPathEnd(chain_end)) {
    return false;
  }
  const int64 destination = BaseNode(1);
  const int64 max_arc_value = arc_evaluator_(destination, chain_end);
  int64 next = Next(chain_end);
  while (!IsPathEnd(next) && arc_evaluator_(chain_end, next) <= max_arc_value) {
    chain_end = next;
    next = Next(chain_end);
  }
  return MoveChainAndRepair(before_chain, chain_end, destination);
}

bool MakeRelocateNeighborsOperator::MoveChainAndRepair(int64 before_chain,
                                                       int64 chain_end,
                                                       int64 destination) {
  if (MoveChain(before_chain, chain_end, destination)) {
    if (!IsPathStart(destination)) {
      int64 current = Prev(destination);
      int64 last = chain_end;
      if (current == last) {  // chain was just before destination
        current = before_chain;
      }
      while (last >= 0 && !IsPathStart(current) && current != last) {
        last = Reposition(current, last);
        current = Prev(current);
      }
    }
    return true;
  }
  return false;
}

int64 MakeRelocateNeighborsOperator::Reposition(int64 before_to_move,
                                                int64 up_to) {
  const int64 kNoChange = -1;
  const int64 to_move = Next(before_to_move);
  int64 next = Next(to_move);
  if (Var(to_move)->Contains(next)) {
    return kNoChange;
  }
  int64 prev = next;
  next = Next(next);
  while (prev != up_to) {
    if (Var(prev)->Contains(to_move) && Var(to_move)->Contains(next)) {
      MoveChain(before_to_move, to_move, prev);
      return up_to;
    }
    prev = next;
    next = Next(next);
  }
  if (Var(prev)->Contains(to_move)) {
    MoveChain(before_to_move, to_move, prev);
    return to_move;
  }
  return kNoChange;
}

MakePairActiveOperator::MakePairActiveOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingNodePairs& pairs)
    : PathOperator(vars, secondary_vars, 2, std::move(start_empty_path_class)),
      inactive_pair_(0),
      pairs_(pairs) {}

bool MakePairActiveOperator::MakeNextNeighbor(Assignment* delta,
                                              Assignment* deltadelta) {
  while (inactive_pair_ < pairs_.size()) {
    if (!IsInactive(pairs_[inactive_pair_].first[0]) ||
        !IsInactive(pairs_[inactive_pair_].second[0]) ||
        !PathOperator::MakeNextNeighbor(delta, deltadelta)) {
      ResetPosition();
      ++inactive_pair_;
    } else {
      return true;
    }
  }
  return false;
}

bool MakePairActiveOperator::MakeNeighbor() {
  DCHECK_EQ(StartNode(0), StartNode(1));
  // Inserting the second node of the pair before the first one which ensures
  // that the only solutions where both nodes are next to each other have the
  // first node before the second (the move is not symmetric and doing it this
  // way ensures that a potential precedence constraint between the nodes of the
  // pair is not violated).
  return MakeActive(pairs_[inactive_pair_].second[0], BaseNode(1)) &&
         MakeActive(pairs_[inactive_pair_].first[0], BaseNode(0));
}

int64 MakePairActiveOperator::GetBaseNodeRestartPosition(int base_index) {
  // Base node 1 must be after base node 0 if they are both on the same path.
  if (base_index == 0 || StartNode(base_index) != StartNode(base_index - 1)) {
    return StartNode(base_index);
  } else {
    return BaseNode(base_index - 1);
  }
}

void MakePairActiveOperator::OnNodeInitialization() {
  for (int i = 0; i < pairs_.size(); ++i) {
    if (IsInactive(pairs_[i].first[0]) && IsInactive(pairs_[i].second[0])) {
      inactive_pair_ = i;
      return;
    }
  }
  inactive_pair_ = pairs_.size();
}

MakePairInactiveOperator::MakePairInactiveOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingNodePairs& node_pairs)
    : PathWithPreviousNodesOperator(vars, secondary_vars, 1,
                                    std::move(start_empty_path_class)) {
  int64 max_pair_index = -1;
  for (const auto& node_pair : node_pairs) {
    max_pair_index = std::max(max_pair_index, node_pair.first[0]);
    max_pair_index = std::max(max_pair_index, node_pair.second[0]);
  }
  pairs_.resize(max_pair_index + 1, -1);
  for (const auto& node_pair : node_pairs) {
    pairs_[node_pair.first[0]] = node_pair.second[0];
    pairs_[node_pair.second[0]] = node_pair.first[0];
  }
}

bool MakePairInactiveOperator::MakeNeighbor() {
  const int64 base = BaseNode(0);
  if (IsPathEnd(base)) {
    return false;
  }
  const int64 next = Next(base);
  if (next < pairs_.size() && pairs_[next] != -1) {
    return MakeChainInactive(Prev(pairs_[next]), pairs_[next]) &&
           MakeChainInactive(base, next);
  }
  return false;
}

PairRelocateOperator::PairRelocateOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingNodePairs& node_pairs)
    : PathOperator(vars, secondary_vars, 3, std::move(start_empty_path_class)) {
  int64 index_max = 0;
  for (const IntVar* const var : vars) {
    index_max = std::max(index_max, var->Max());
  }
  prevs_.resize(index_max + 1, -1);
  is_first_.resize(index_max + 1, false);
  int64 max_pair_index = -1;
  for (const auto& node_pair : node_pairs) {
    max_pair_index = std::max(max_pair_index, node_pair.first[0]);
    max_pair_index = std::max(max_pair_index, node_pair.second[0]);
  }
  pairs_.resize(max_pair_index + 1, -1);
  for (const auto& node_pair : node_pairs) {
    pairs_[node_pair.first[0]] = node_pair.second[0];
    pairs_[node_pair.second[0]] = node_pair.first[0];
    is_first_[node_pair.first[0]] = true;
  }
}

bool PairRelocateOperator::MakeNeighbor() {
  DCHECK_EQ(StartNode(1), StartNode(2));
  const int64 first_pair_node = BaseNode(kPairFirstNode);
  const int64 prev = prevs_[first_pair_node];
  if (prev < 0) {
    return false;
  }
  const int sibling =
      first_pair_node < pairs_.size() ? pairs_[first_pair_node] : -1;
  if (sibling < 0) {
    return false;
  }
  if (!is_first_[first_pair_node]) {
    return false;
  }
  const int64 prev_sibling = prevs_[sibling];
  if (prev_sibling < 0) {
    return false;
  }
  return MoveChain(prev_sibling, sibling,
                   BaseNode(kPairSecondNodeDestination)) &&
         MoveChain(prev, first_pair_node, BaseNode(kPairFirstNodeDestination));
}

int64 PairRelocateOperator::GetBaseNodeRestartPosition(int base_index) {
  // Destination node of the second node of a pair must be after the
  // destination node of the first node of a pair.
  if (base_index == kPairSecondNodeDestination) {
    return BaseNode(kPairFirstNodeDestination);
  } else {
    return StartNode(base_index);
  }
}

void PairRelocateOperator::OnNodeInitialization() {
  for (int i = 0; i < number_of_nexts(); ++i) {
    prevs_[Next(i)] = i;
  }
}

NodePairSwapActiveOperator::NodePairSwapActiveOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingNodePairs& node_pairs)
    : PathWithPreviousNodesOperator(vars, secondary_vars, 1,
                                    std::move(start_empty_path_class)),
      inactive_node_(0) {
  int64 max_pair_index = -1;
  for (const auto& node_pair : node_pairs) {
    max_pair_index = std::max(max_pair_index, node_pair.first[0]);
    max_pair_index = std::max(max_pair_index, node_pair.second[0]);
  }
  pairs_.resize(max_pair_index + 1, -1);
  for (const auto& node_pair : node_pairs) {
    pairs_[node_pair.first[0]] = node_pair.second[0];
    pairs_[node_pair.second[0]] = node_pair.first[0];
  }
}

bool NodePairSwapActiveOperator::MakeNextNeighbor(Assignment* delta,
                                                  Assignment* deltadelta) {
  while (inactive_node_ < Size()) {
    if (!IsInactive(inactive_node_) ||
        !PathOperator::MakeNextNeighbor(delta, deltadelta)) {
      ResetPosition();
      ++inactive_node_;
    } else {
      return true;
    }
  }
  return false;
}

bool NodePairSwapActiveOperator::MakeNeighbor() {
  const int64 base = BaseNode(0);
  if (IsPathEnd(base)) {
    return false;
  }
  const int64 next = Next(base);
  if (next < pairs_.size() && pairs_[next] != -1) {
    return MakeChainInactive(Prev(pairs_[next]), pairs_[next]) &&
           MakeChainInactive(base, next) && MakeActive(inactive_node_, base);
  }
  return false;
}

void NodePairSwapActiveOperator::OnNodeInitialization() {
  PathWithPreviousNodesOperator::OnNodeInitialization();
  for (int i = 0; i < Size(); ++i) {
    if (IsInactive(i) && i < pairs_.size() && pairs_[i] == -1) {
      inactive_node_ = i;
      return;
    }
  }
  inactive_node_ = Size();
}

}  // namespace operations_research
