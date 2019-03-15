// Copyright 2010-2018 Google LLC
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
    RoutingTransitCallback2 arc_evaluator)
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
    const RoutingIndexPairs& pairs)
    : PathOperator(vars, secondary_vars, 2, false,
                   std::move(start_empty_path_class)),
      inactive_pair_(0),
      pairs_(pairs) {}

bool MakePairActiveOperator::MakeNextNeighbor(Assignment* delta,
                                              Assignment* deltadelta) {
  // TODO(user): Support pairs with disjunctions.
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
    const RoutingIndexPairs& index_pairs)
    : PathWithPreviousNodesOperator(vars, secondary_vars, 1,
                                    std::move(start_empty_path_class)) {
  int64 max_pair_index = -1;
  for (const auto& index_pair : index_pairs) {
    max_pair_index = std::max(max_pair_index, index_pair.first[0]);
    max_pair_index = std::max(max_pair_index, index_pair.second[0]);
  }
  pairs_.resize(max_pair_index + 1, -1);
  // TODO(user): Support pairs with disjunctions.
  for (const auto& index_pair : index_pairs) {
    pairs_[index_pair.first[0]] = index_pair.second[0];
    pairs_[index_pair.second[0]] = index_pair.first[0];
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
    const RoutingIndexPairs& index_pairs)
    : PathWithPreviousNodesOperator(vars, secondary_vars, 3,
                                    std::move(start_empty_path_class)) {
  int64 index_max = 0;
  for (const IntVar* const var : vars) {
    index_max = std::max(index_max, var->Max());
  }
  is_first_.resize(index_max + 1, false);
  int64 max_pair_index = -1;
  // TODO(user): Support pairs with disjunctions.
  for (const auto& index_pair : index_pairs) {
    max_pair_index = std::max(max_pair_index, index_pair.first[0]);
    max_pair_index = std::max(max_pair_index, index_pair.second[0]);
  }
  pairs_.resize(max_pair_index + 1, -1);
  for (const auto& index_pair : index_pairs) {
    pairs_[index_pair.first[0]] = index_pair.second[0];
    pairs_[index_pair.second[0]] = index_pair.first[0];
    is_first_[index_pair.first[0]] = true;
  }
}

bool PairRelocateOperator::MakeNeighbor() {
  DCHECK_EQ(StartNode(1), StartNode(2));
  const int64 first_pair_node = BaseNode(kPairFirstNode);
  if (IsPathStart(first_pair_node)) {
    return false;
  }
  int64 first_prev = Prev(first_pair_node);
  const int second_pair_node =
      first_pair_node < pairs_.size() ? pairs_[first_pair_node] : -1;
  if (second_pair_node < 0) {
    return false;
  }
  if (!is_first_[first_pair_node]) {
    return false;
  }
  if (IsPathStart(second_pair_node)) {
    return false;
  }
  const int64 second_prev = Prev(second_pair_node);

  if (BaseNode(kPairFirstNodeDestination) == second_pair_node) {
    // The second_pair_node -> first_pair_node link is forbidden.
    return false;
  }

  if (second_prev == first_pair_node &&
      BaseNode(kPairFirstNodeDestination) == first_prev &&
      BaseNode(kPairSecondNodeDestination) == first_prev) {
    // If the current sequence is first_prev -> first_pair_node ->
    // second_pair_node, and both 1st and 2nd are moved both to prev, the result
    // of the move will be first_prev -> first_pair_node -> second_pair_node,
    // which is no move.
    return false;
  }

  // Relocation is a success if at least one of the nodes moved and all the
  // moves are successful.
  bool moved = false;

  // Do not allow to move second_pair_node to its current prev.
  if (second_prev != BaseNode(kPairSecondNodeDestination)) {
    if (!MoveChain(second_prev, second_pair_node,
                   BaseNode(kPairSecondNodeDestination))) {
      return false;
    }
    if (BaseNode(kPairSecondNodeDestination) == first_prev) {
      first_prev = second_pair_node;
    }
    moved = true;
  }

  // Do not allow to move first_pair_node to its current prev.
  if (first_prev != BaseNode(kPairFirstNodeDestination)) {
    if (!MoveChain(first_prev, first_pair_node,
                   BaseNode(kPairFirstNodeDestination))) {
      return false;
    }
    moved = true;
  }

  return moved;
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

LightPairRelocateOperator::LightPairRelocateOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingIndexPairs& index_pairs)
    : PathWithPreviousNodesOperator(vars, secondary_vars, 2,
                                    std::move(start_empty_path_class)) {
  int64 max_pair_index = -1;
  // TODO(user): Support pairs with disjunctions.
  for (const auto& index_pair : index_pairs) {
    max_pair_index = std::max(max_pair_index, index_pair.first[0]);
    max_pair_index = std::max(max_pair_index, index_pair.second[0]);
  }
  pairs_.resize(max_pair_index + 1, -1);
  for (const auto& index_pair : index_pairs) {
    pairs_[index_pair.first[0]] = index_pair.second[0];
    pairs_[index_pair.second[0]] = index_pair.first[0];
  }
}

bool LightPairRelocateOperator::MakeNeighbor() {
  const int64 prev1 = BaseNode(0);
  if (IsPathEnd(prev1)) return false;
  const int64 node1 = Next(prev1);
  if (IsPathEnd(node1) || node1 >= pairs_.size()) return false;
  const int64 sibling1 = pairs_[node1];
  if (sibling1 == -1) return false;
  const int64 node2 = BaseNode(1);
  if (node2 == sibling1 || IsPathEnd(node2) || node2 >= pairs_.size()) {
    return false;
  }
  const int64 sibling2 = pairs_[node2];
  if (sibling2 == -1) return false;
  int64 prev_sibling1 = Prev(sibling1);
  if (prev_sibling1 == node1) {
    prev_sibling1 = prev1;
  } else if (prev_sibling1 == node2) {
    prev_sibling1 = node1;
  }
  return MoveChain(prev1, node1, node2) &&
         (sibling2 == prev_sibling1 ||
          MoveChain(prev_sibling1, sibling1, sibling2));
}

PairExchangeOperator::PairExchangeOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingIndexPairs& index_pairs)
    : PathWithPreviousNodesOperator(vars, secondary_vars, 2,
                                    std::move(start_empty_path_class)) {
  int64 index_max = 0;
  for (const IntVar* const var : vars) {
    index_max = std::max(index_max, var->Max());
  }
  is_first_.resize(index_max + 1, false);
  int64 max_pair_index = -1;
  // TODO(user): Support pairs with disjunctions.
  for (const auto& index_pair : index_pairs) {
    max_pair_index = std::max(max_pair_index, index_pair.first[0]);
    max_pair_index = std::max(max_pair_index, index_pair.second[0]);
  }
  pairs_.resize(max_pair_index + 1, -1);
  for (const auto& index_pair : index_pairs) {
    pairs_[index_pair.first[0]] = index_pair.second[0];
    pairs_[index_pair.second[0]] = index_pair.first[0];
    is_first_[index_pair.first[0]] = true;
  }
}

bool PairExchangeOperator::MakeNeighbor() {
  const int64 node1 = BaseNode(0);
  int64 prev1, sibling1, sibling_prev1 = -1;
  if (!GetPreviousAndSibling(node1, &prev1, &sibling1, &sibling_prev1)) {
    return false;
  }
  const int64 node2 = BaseNode(1);
  int64 prev2, sibling2, sibling_prev2 = -1;
  if (!GetPreviousAndSibling(node2, &prev2, &sibling2, &sibling_prev2)) {
    return false;
  }
  bool status = true;
  // Exchanging node1 and node2.
  if (node1 == prev2) {
    status = MoveChain(prev2, node2, prev1);
    if (sibling_prev1 == node2) sibling_prev1 = node1;
    if (sibling_prev2 == node2) sibling_prev2 = node1;
  } else if (node2 == prev1) {
    status = MoveChain(prev1, node1, prev2);
    if (sibling_prev1 == node1) sibling_prev1 = node2;
    if (sibling_prev2 == node1) sibling_prev2 = node2;
  } else {
    status = MoveChain(prev1, node1, node2) && MoveChain(prev2, node2, prev1);
    if (sibling_prev1 == node1) {
      sibling_prev1 = node2;
    } else if (sibling_prev1 == node2) {
      sibling_prev1 = node1;
    }
    if (sibling_prev2 == node1) {
      sibling_prev2 = node2;
    } else if (sibling_prev2 == node2) {
      sibling_prev2 = node1;
    }
  }
  if (!status) return false;
  // Exchanging sibling1 and sibling2.
  if (sibling1 == sibling_prev2) {
    status = MoveChain(sibling_prev2, sibling2, sibling_prev1);
  } else if (sibling2 == sibling_prev1) {
    status = MoveChain(sibling_prev1, sibling1, sibling_prev2);
  } else {
    status = MoveChain(sibling_prev1, sibling1, sibling2) &&
             MoveChain(sibling_prev2, sibling2, sibling_prev1);
  }
  return status;
}

bool PairExchangeOperator::GetPreviousAndSibling(
    int64 node, int64* previous, int64* sibling,
    int64* sibling_previous) const {
  if (IsPathStart(node)) return false;
  *previous = Prev(node);
  *sibling = node < pairs_.size() ? pairs_[node] : -1;
  *sibling_previous = *sibling >= 0 ? Prev(*sibling) : -1;
  return *sibling_previous >= 0 && is_first_[node];
}

PairExchangeRelocateOperator::PairExchangeRelocateOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingIndexPairs& index_pairs)
    : PathWithPreviousNodesOperator(vars, secondary_vars, 6,
                                    std::move(start_empty_path_class)) {
  int64 index_max = 0;
  for (const IntVar* const var : vars) {
    index_max = std::max(index_max, var->Max());
  }
  is_first_.resize(index_max + 1, false);
  int64 max_pair_index = -1;
  // TODO(user): Support pairs with disjunctions.
  for (const auto& index_pair : index_pairs) {
    max_pair_index = std::max(max_pair_index, index_pair.first[0]);
    max_pair_index = std::max(max_pair_index, index_pair.second[0]);
  }
  pairs_.resize(max_pair_index + 1, -1);
  for (const auto& index_pair : index_pairs) {
    pairs_[index_pair.first[0]] = index_pair.second[0];
    pairs_[index_pair.second[0]] = index_pair.first[0];
    is_first_[index_pair.first[0]] = true;
  }
}

bool PairExchangeRelocateOperator::MakeNeighbor() {
  DCHECK_EQ(StartNode(kSecondPairFirstNodeDestination),
            StartNode(kSecondPairSecondNodeDestination));
  DCHECK_EQ(StartNode(kSecondPairFirstNode),
            StartNode(kFirstPairFirstNodeDestination));
  DCHECK_EQ(StartNode(kSecondPairFirstNode),
            StartNode(kFirstPairSecondNodeDestination));

  if (StartNode(kFirstPairFirstNode) == StartNode(kSecondPairFirstNode)) {
    SetNextBaseToIncrement(kSecondPairFirstNode);
    return false;
  }
  // Through this method, <base>[X][Y] represent the <base> variable for the
  // node Y of pair X. <base> is in node, prev, dest.
  int64 nodes[2][2];
  int64 prev[2][2];
  int64 dest[2][2];
  nodes[0][0] = BaseNode(kFirstPairFirstNode);
  nodes[1][0] = BaseNode(kSecondPairFirstNode);
  if (nodes[1][0] <= nodes[0][0]) {
    // Exchange is symetric.
    SetNextBaseToIncrement(kSecondPairFirstNode);
    return false;
  }
  if (!GetPreviousAndSibling(nodes[0][0], &prev[0][0], &nodes[0][1],
                             &prev[0][1])) {
    SetNextBaseToIncrement(kFirstPairFirstNode);
    return false;
  }
  if (!GetPreviousAndSibling(nodes[1][0], &prev[1][0], &nodes[1][1],
                             &prev[1][1])) {
    SetNextBaseToIncrement(kSecondPairFirstNode);
    return false;
  }

  if (!LoadAndCheckDest(0, 0, kFirstPairFirstNodeDestination, nodes, dest)) {
    SetNextBaseToIncrement(kFirstPairFirstNodeDestination);
    return false;
  }
  if (!LoadAndCheckDest(0, 1, kFirstPairSecondNodeDestination, nodes, dest)) {
    SetNextBaseToIncrement(kFirstPairSecondNodeDestination);
    return false;
  }
  if (StartNode(kSecondPairFirstNodeDestination) !=
          StartNode(kFirstPairFirstNode) ||
      !LoadAndCheckDest(1, 0, kSecondPairFirstNodeDestination, nodes, dest)) {
    SetNextBaseToIncrement(kSecondPairFirstNodeDestination);
    return false;
  }
  if (!LoadAndCheckDest(1, 1, kSecondPairSecondNodeDestination, nodes, dest)) {
    SetNextBaseToIncrement(kSecondPairSecondNodeDestination);
    return false;
  }

  if (!MoveNode(0, 1, nodes, dest, prev)) {
    SetNextBaseToIncrement(kFirstPairSecondNodeDestination);
    return false;
  }
  if (!MoveNode(0, 0, nodes, dest, prev)) {
    SetNextBaseToIncrement(kFirstPairSecondNodeDestination);
    return false;
  }
  if (!MoveNode(1, 1, nodes, dest, prev)) {
    return false;
  }
  if (!MoveNode(1, 0, nodes, dest, prev)) {
    return false;
  }
  return true;
}

bool PairExchangeRelocateOperator::MoveNode(int pair, int node,
                                            int64 nodes[2][2], int64 dest[2][2],
                                            int64 prev[2][2]) {
  if (!MoveChain(prev[pair][node], nodes[pair][node], dest[pair][node])) {
    return false;
  }
  // Update the other pair if needed.
  if (prev[1 - pair][0] == dest[pair][node]) {
    prev[1 - pair][0] = nodes[pair][node];
  }
  if (prev[1 - pair][1] == dest[pair][node]) {
    prev[1 - pair][1] = nodes[pair][node];
  }
  return true;
}

bool PairExchangeRelocateOperator::LoadAndCheckDest(int pair, int node,
                                                    int64 base_node,
                                                    int64 nodes[2][2],
                                                    int64 dest[2][2]) const {
  dest[pair][node] = BaseNode(base_node);
  // A destination cannot be a node that will be moved.
  return !(nodes[0][0] == dest[pair][node] || nodes[0][1] == dest[pair][node] ||
           nodes[1][0] == dest[pair][node] || nodes[1][1] == dest[pair][node]);
}

bool PairExchangeRelocateOperator::OnSamePathAsPreviousBase(int64 base_index) {
  // Ensuring the destination of the first pair is on the route of the second.
  // pair.
  // Ensuring that destination of both nodes of a pair are on the same route.
  return base_index == kFirstPairFirstNodeDestination ||
         base_index == kFirstPairSecondNodeDestination ||
         base_index == kSecondPairSecondNodeDestination;
}

int64 PairExchangeRelocateOperator::GetBaseNodeRestartPosition(int base_index) {
  if (base_index == kFirstPairSecondNodeDestination ||
      base_index == kSecondPairSecondNodeDestination) {
    return BaseNode(base_index - 1);
  } else {
    return StartNode(base_index);
  }
}

bool PairExchangeRelocateOperator::GetPreviousAndSibling(
    int64 node, int64* previous, int64* sibling,
    int64* sibling_previous) const {
  if (IsPathStart(node)) return false;
  *previous = Prev(node);
  *sibling = node < pairs_.size() ? pairs_[node] : -1;
  *sibling_previous = *sibling >= 0 ? Prev(*sibling) : -1;
  return *sibling_previous >= 0 && is_first_[node];
}

SwapIndexPairOperator::SwapIndexPairOperator(
    const std::vector<IntVar*>& vars, const std::vector<IntVar*>& path_vars,
    const RoutingIndexPairs& index_pairs)
    : IntVarLocalSearchOperator(vars),
      index_pairs_(index_pairs),
      pair_index_(0),
      first_index_(0),
      second_index_(0),
      number_of_nexts_(vars.size()),
      ignore_path_vars_(path_vars.empty()) {
  if (!ignore_path_vars_) {
    AddVars(path_vars);
  }
}

bool SwapIndexPairOperator::MakeNextNeighbor(Assignment* delta,
                                             Assignment* deltadelta) {
  const int64 kNoPath = -1;
  CHECK(delta != nullptr);
  while (true) {
    RevertChanges(true);

    if (pair_index_ < index_pairs_.size()) {
      const int64 path =
          ignore_path_vars_ ? 0LL : Value(first_active_ + number_of_nexts_);
      const int64 prev_first = prevs_[first_active_];
      const int64 next_first = Value(first_active_);
      // Making current active "pickup" unperformed.
      SetNext(first_active_, first_active_, kNoPath);
      // Inserting "pickup" alternative at the same position.
      const int64 insert_first = index_pairs_[pair_index_].first[first_index_];
      SetNext(prev_first, insert_first, path);
      SetNext(insert_first, next_first, path);
      int64 prev_second = prevs_[second_active_];
      if (prev_second == first_active_) {
        prev_second = insert_first;
      }
      DCHECK_EQ(path, ignore_path_vars_
                          ? 0LL
                          : Value(second_active_ + number_of_nexts_));
      const int64 next_second = Value(second_active_);
      // Making current active "delivery" unperformed.
      SetNext(second_active_, second_active_, kNoPath);
      // Inserting "delivery" alternative at the same position.
      const int64 insert_second =
          index_pairs_[pair_index_].second[second_index_];
      SetNext(prev_second, insert_second, path);
      SetNext(insert_second, next_second, path);
      // Move to next "pickup/delivery" alternative.
      ++second_index_;
      if (second_index_ >= index_pairs_[pair_index_].second.size()) {
        second_index_ = 0;
        ++first_index_;
        if (first_index_ >= index_pairs_[pair_index_].first.size()) {
          first_index_ = 0;
          ++pair_index_;
          UpdateActiveNodes();
        }
      }
    } else {
      return false;
    }

    if (ApplyChanges(delta, deltadelta)) {
      VLOG(2) << "Delta (" << DebugString() << ") = " << delta->DebugString();
      return true;
    }
  }
  return false;
}

void SwapIndexPairOperator::OnStart() {
  prevs_.resize(number_of_nexts_, -1);
  for (int index = 0; index < number_of_nexts_; ++index) {
    const int64 next = Value(index);
    if (next >= prevs_.size()) prevs_.resize(next + 1, -1);
    prevs_[next] = index;
  }
  pair_index_ = 0;
  first_index_ = 0;
  second_index_ = 0;
  first_active_ = -1;
  second_active_ = -1;
  while (true) {
    if (!UpdateActiveNodes()) break;
    if (first_active_ != -1 && second_active_ != -1) {
      break;
    }
    ++pair_index_;
  }
}

bool SwapIndexPairOperator::UpdateActiveNodes() {
  if (pair_index_ < index_pairs_.size()) {
    for (const int64 first : index_pairs_[pair_index_].first) {
      if (Value(first) != first) {
        first_active_ = first;
        break;
      }
    }
    for (const int64 second : index_pairs_[pair_index_].second) {
      if (Value(second) != second) {
        second_active_ = second;
        break;
      }
    }
    return true;
  }
  return false;
}

IndexPairSwapActiveOperator::IndexPairSwapActiveOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingIndexPairs& index_pairs)
    : PathWithPreviousNodesOperator(vars, secondary_vars, 1,
                                    std::move(start_empty_path_class)),
      inactive_node_(0) {
  int64 max_pair_index = -1;
  // TODO(user): Support pairs with disjunctions.
  for (const auto& index_pair : index_pairs) {
    max_pair_index = std::max(max_pair_index, index_pair.first[0]);
    max_pair_index = std::max(max_pair_index, index_pair.second[0]);
  }
  pairs_.resize(max_pair_index + 1, -1);
  for (const auto& index_pair : index_pairs) {
    pairs_[index_pair.first[0]] = index_pair.second[0];
    pairs_[index_pair.second[0]] = index_pair.first[0];
  }
}

bool IndexPairSwapActiveOperator::MakeNextNeighbor(Assignment* delta,
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

bool IndexPairSwapActiveOperator::MakeNeighbor() {
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

void IndexPairSwapActiveOperator::OnNodeInitialization() {
  PathWithPreviousNodesOperator::OnNodeInitialization();
  for (int i = 0; i < Size(); ++i) {
    if (IsInactive(i) && i < pairs_.size() && pairs_[i] == -1) {
      inactive_node_ = i;
      return;
    }
  }
  inactive_node_ = Size();
}

RelocateExpensiveChain::RelocateExpensiveChain(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class, int num_arcs_to_consider,
    std::function<int64(int64, int64, int64)> arc_cost_for_path_start)
    : PathOperator(vars, secondary_vars, 1, false,
                   std::move(start_empty_path_class)),
      num_arcs_to_consider_(num_arcs_to_consider),
      current_path_(0),
      current_expensive_arc_indices_({-1, -1}),
      arc_cost_for_path_start_(std::move(arc_cost_for_path_start)),
      end_path_(0),
      has_non_empty_paths_to_explore_(false) {}

bool RelocateExpensiveChain::MakeNeighbor() {
  const int first_arc_index = current_expensive_arc_indices_.first;
  const int second_arc_index = current_expensive_arc_indices_.second;
  DCHECK_LE(0, first_arc_index);
  DCHECK_LT(first_arc_index, second_arc_index);
  DCHECK_LT(second_arc_index, most_expensive_arc_starts_and_ranks_.size());

  const std::pair<int, int>& first_start_and_rank =
      most_expensive_arc_starts_and_ranks_[first_arc_index];
  const std::pair<int, int>& second_start_and_rank =
      most_expensive_arc_starts_and_ranks_[second_arc_index];
  if (first_start_and_rank.second < second_start_and_rank.second) {
    return MoveChain(first_start_and_rank.first, second_start_and_rank.first,
                     BaseNode(0));
  }
  return MoveChain(second_start_and_rank.first, first_start_and_rank.first,
                   BaseNode(0));
}

bool RelocateExpensiveChain::MakeOneNeighbor() {
  while (has_non_empty_paths_to_explore_) {
    if (!PathOperator::MakeOneNeighbor()) {
      ResetPosition();
      // Move on to the next expensive arcs on the same path.
      if (IncrementCurrentArcIndices()) {
        continue;
      }
      // Move on to the next non_empty path.
      IncrementCurrentPath();
      has_non_empty_paths_to_explore_ =
          current_path_ != end_path_ &&
          FindMostExpensiveChainsOnRemainingPaths();
    } else {
      return true;
    }
  }
  return false;
}

void RelocateExpensiveChain::OnNodeInitialization() {
  if (current_path_ >= path_starts().size()) {
    // current_path_ was made empty by last move (and it was the last non-empty
    // path), restart from 0.
    current_path_ = 0;
  }
  end_path_ = current_path_;
  has_non_empty_paths_to_explore_ = FindMostExpensiveChainsOnRemainingPaths();
}

void RelocateExpensiveChain::IncrementCurrentPath() {
  const int num_paths = path_starts().size();
  if (++current_path_ == num_paths) {
    current_path_ = 0;
  }
}

bool RelocateExpensiveChain::IncrementCurrentArcIndices() {
  int& second_index = current_expensive_arc_indices_.second;
  if (++second_index < most_expensive_arc_starts_and_ranks_.size()) {
    return true;
  }
  int& first_index = current_expensive_arc_indices_.first;
  if (first_index + 2 < most_expensive_arc_starts_and_ranks_.size()) {
    first_index++;
    second_index = first_index + 1;
    return true;
  }
  return false;
}

bool RelocateExpensiveChain::FindMostExpensiveChainsOnRemainingPaths() {
  do {
    if (FindMostExpensiveChainsOnCurrentPath()) {
      return true;
    }
    IncrementCurrentPath();
  } while (current_path_ != end_path_);
  return false;
}

bool RelocateExpensiveChain::FindMostExpensiveChainsOnCurrentPath() {
  const int64 current_path_start = path_starts()[current_path_];

  if (IsPathEnd(OldNext(current_path_start))) {
    // Empty path.
    current_expensive_arc_indices_.first =
        current_expensive_arc_indices_.second = -1;
    return false;
  }

  // TODO(user): Investigate the impact of using a limited size priority
  // queue instead of vectors on performance.
  std::vector<int64> most_expensive_arc_costs(num_arcs_to_consider_, -1);
  most_expensive_arc_starts_and_ranks_.assign(num_arcs_to_consider_, {-1, -1});

  int64 before_node = current_path_start;
  int rank = 0;
  while (!IsPathEnd(before_node)) {
    const int64 after_node = OldNext(before_node);
    const int64 arc_cost =
        arc_cost_for_path_start_(before_node, after_node, current_path_start);
    if (most_expensive_arc_costs.back() < arc_cost) {
      // Insert this arc in most_expensive_* vectors.
      most_expensive_arc_costs.back() = arc_cost;
      most_expensive_arc_starts_and_ranks_.back().first = before_node;
      most_expensive_arc_starts_and_ranks_.back().second = rank;
      // Move the newly added element in the vectors to keep
      // most_expensive_arc_costs sorted decreasingly.
      int index_before_added_arc = num_arcs_to_consider_ - 2;
      while (index_before_added_arc >= 0 &&
             most_expensive_arc_costs[index_before_added_arc] < arc_cost) {
        std::swap(most_expensive_arc_costs[index_before_added_arc + 1],
                  most_expensive_arc_costs[index_before_added_arc]);
        std::swap(
            most_expensive_arc_starts_and_ranks_[index_before_added_arc + 1],
            most_expensive_arc_starts_and_ranks_[index_before_added_arc]);
        index_before_added_arc--;
      }
    }
    before_node = after_node;
    rank++;
  }
  // If there are less than num_arcs_to_consider_ arcs on the path, resize the
  // vector of arc starts.
  if (rank < num_arcs_to_consider_) {
    most_expensive_arc_starts_and_ranks_.resize(rank);
  }

  current_expensive_arc_indices_.first = 0;
  current_expensive_arc_indices_.second = 1;

  return true;
}

RelocateSubtrip::RelocateSubtrip(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingIndexPairs& pairs)
    : PathOperator(vars, secondary_vars, 2, false,
                   std::move(start_empty_path_class)) {
  is_first_node_.resize(number_of_nexts_, false);
  is_second_node_.resize(number_of_nexts_, false);
  pair_of_node_.resize(number_of_nexts_, -1);
  for (int pair_index = 0; pair_index < pairs.size(); ++pair_index) {
    for (const int node : pairs[pair_index].first) {
      is_first_node_[node] = true;
      pair_of_node_[node] = pair_index;
    }
    for (const int node : pairs[pair_index].second) {
      is_second_node_[node] = true;
      pair_of_node_[node] = pair_index;
    }
  }
  opened_pairs_set_.resize(pairs.size(), false);
}

bool RelocateSubtrip::MakeNeighbor() {
  // We iterate on two nodes, one is the beginning of the subtrip,
  // the other is the position after which to insert.
  // If the first node cannot be the beginning of a subtrip, do not move.
  const int prev_chain = BaseNode(0);
  if (IsPathEnd(prev_chain)) return false;
  const int insertion_node = BaseNode(1);
  if (IsPathEnd(insertion_node)) return false;
  if (prev_chain == insertion_node) return false;

  // We are looking for the shortest chain such that all pickup/delivery pairs
  // appearing in the chain have both pickup and delivery inside the chain.
  // If there is none (other that the empty chain), do not move.
  // Obviously, the chain should not contain the insertion node.
  int current = Next(prev_chain);
  if (IsPathEnd(current)) return false;
  if (!is_first_node_[current]) return false;
  int chain_end = current;
  opened_pairs_set_.assign(opened_pairs_set_.size(), false);
  int num_opened_pairs = 0;
  do {
    if (current == insertion_node) return false;
    if (is_first_node_[current] || is_second_node_[current]) {
      const int pair = pair_of_node_[current];
      if (opened_pairs_set_[pair]) {
        --num_opened_pairs;
        opened_pairs_set_[pair] = false;
      } else {
        ++num_opened_pairs;
        opened_pairs_set_[pair] = true;
      }
    }
    chain_end = current;
    current = Next(current);
  } while (num_opened_pairs != 0 && !IsPathEnd(current));
  if (num_opened_pairs != 0) return false;

  return MoveChain(prev_chain, chain_end, insertion_node);
}

}  // namespace operations_research
