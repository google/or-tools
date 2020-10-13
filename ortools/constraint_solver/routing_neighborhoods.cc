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

#include <algorithm>
#include <functional>

#include "absl/container/flat_hash_set.h"
#include "ortools/constraint_solver/constraint_solveri.h"

namespace operations_research {

MakeRelocateNeighborsOperator::MakeRelocateNeighborsOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    RoutingTransitCallback2 arc_evaluator)
    : PathOperator(vars, secondary_vars, 2, true, false,
                   std::move(start_empty_path_class)),
      arc_evaluator_(std::move(arc_evaluator)) {}

bool MakeRelocateNeighborsOperator::MakeNeighbor() {
  const int64 before_chain = BaseNode(0);
  int64 chain_end = Next(before_chain);
  if (IsPathEnd(chain_end)) return false;
  const int64 destination = BaseNode(1);
  if (chain_end == destination) return false;
  const int64 max_arc_value = arc_evaluator_(destination, chain_end);
  int64 next = Next(chain_end);
  while (!IsPathEnd(next) && arc_evaluator_(chain_end, next) <= max_arc_value) {
    if (next == destination) return false;
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
    : PathOperator(vars, secondary_vars, 2, false, true,
                   std::move(start_empty_path_class)),
      inactive_pair_(0),
      inactive_pair_first_index_(0),
      inactive_pair_second_index_(0),
      pairs_(pairs) {}

bool MakePairActiveOperator::MakeOneNeighbor() {
  while (inactive_pair_ < pairs_.size()) {
    if (PathOperator::MakeOneNeighbor()) return true;
    ResetPosition();
    if (inactive_pair_first_index_ < pairs_[inactive_pair_].first.size() - 1) {
      ++inactive_pair_first_index_;
    } else if (inactive_pair_second_index_ <
               pairs_[inactive_pair_].second.size() - 1) {
      inactive_pair_first_index_ = 0;
      ++inactive_pair_second_index_;
    } else {
      inactive_pair_ = FindNextInactivePair(inactive_pair_ + 1);
      inactive_pair_first_index_ = 0;
      inactive_pair_second_index_ = 0;
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
  return MakeActive(pairs_[inactive_pair_].second[inactive_pair_second_index_],
                    BaseNode(1)) &&
         MakeActive(pairs_[inactive_pair_].first[inactive_pair_first_index_],
                    BaseNode(0));
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
  inactive_pair_ = FindNextInactivePair(0);
  inactive_pair_first_index_ = 0;
  inactive_pair_second_index_ = 0;
}

int MakePairActiveOperator::FindNextInactivePair(int pair_index) const {
  for (int index = pair_index; index < pairs_.size(); ++index) {
    if (!ContainsActiveNodes(pairs_[index].first) &&
        !ContainsActiveNodes(pairs_[index].second)) {
      return index;
    }
  }
  return pairs_.size();
}

bool MakePairActiveOperator::ContainsActiveNodes(
    const std::vector<int64>& nodes) const {
  for (int64 node : nodes) {
    if (!IsInactive(node)) return true;
  }
  return false;
}

MakePairInactiveOperator::MakePairInactiveOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingIndexPairs& index_pairs)
    : PathOperator(vars, secondary_vars, 1, true, false,
                   std::move(start_empty_path_class)) {
  AddPairAlternativeSets(index_pairs);
}

bool MakePairInactiveOperator::MakeNeighbor() {
  const int64 base = BaseNode(0);
  const int64 first_index = Next(base);
  const int64 second_index = GetActiveAlternativeSibling(first_index);
  if (second_index < 0) {
    return false;
  }
  return MakeChainInactive(base, first_index) &&
         MakeChainInactive(Prev(second_index), second_index);
}

PairRelocateOperator::PairRelocateOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingIndexPairs& index_pairs)
    : PathOperator(vars, secondary_vars, 3, true, false,
                   std::move(start_empty_path_class)) {
  AddPairAlternativeSets(index_pairs);
}

bool PairRelocateOperator::MakeNeighbor() {
  DCHECK_EQ(StartNode(1), StartNode(2));
  const int64 first_pair_node = BaseNode(kPairFirstNode);
  if (IsPathStart(first_pair_node)) {
    return false;
  }
  int64 first_prev = Prev(first_pair_node);
  const int second_pair_node = GetActiveAlternativeSibling(first_pair_node);
  if (second_pair_node < 0 || IsPathEnd(second_pair_node) ||
      IsPathStart(second_pair_node)) {
    return false;
  }
  const int64 second_prev = Prev(second_pair_node);

  const int64 first_node_destination = BaseNode(kPairFirstNodeDestination);
  if (first_node_destination == second_pair_node) {
    // The second_pair_node -> first_pair_node link is forbidden.
    return false;
  }

  const int64 second_node_destination = BaseNode(kPairSecondNodeDestination);
  if (second_prev == first_pair_node && first_node_destination == first_prev &&
      second_node_destination == first_prev) {
    // If the current sequence is first_prev -> first_pair_node ->
    // second_pair_node, and both 1st and 2nd are moved both to prev, the result
    // of the move will be first_prev -> first_pair_node -> second_pair_node,
    // which is no move.
    return false;
  }

  // Relocation is successful if both moves are feasible and at least one of the
  // nodes moves.
  if (second_pair_node == second_node_destination ||
      first_pair_node == first_node_destination) {
    return false;
  }
  const bool moved_second_pair_node =
      MoveChain(second_prev, second_pair_node, second_node_destination);
  // Explictly calling Prev as second_pair_node might have been moved before
  // first_pair_node.
  const bool moved_first_pair_node =
      MoveChain(Prev(first_pair_node), first_pair_node, first_node_destination);
  // Swapping alternatives in.
  SwapActiveAndInactive(second_pair_node,
                        BaseSiblingAlternativeNode(kPairFirstNode));
  SwapActiveAndInactive(first_pair_node, BaseAlternativeNode(kPairFirstNode));
  return moved_first_pair_node || moved_second_pair_node;
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
    : PathOperator(vars, secondary_vars, 2, true, false,
                   std::move(start_empty_path_class)) {
  AddPairAlternativeSets(index_pairs);
}

bool LightPairRelocateOperator::MakeNeighbor() {
  const int64 prev1 = BaseNode(0);
  const int64 node1 = Next(prev1);
  if (IsPathEnd(node1)) return false;
  const int64 sibling1 = GetActiveAlternativeSibling(node1);
  if (sibling1 == -1) return false;
  const int64 node2 = BaseNode(1);
  if (node2 == sibling1) return false;
  const int64 sibling2 = GetActiveAlternativeSibling(node2);
  if (sibling2 == -1) return false;
  // Note: MoveChain will return false if it is a no-op (moving the chain to its
  // current position). However we want to accept the move if at least node1 or
  // sibling1 gets moved to a new position. Therefore we want to be sure both
  // MoveChains are called and at least one succeeds.
  const bool ok = MoveChain(prev1, node1, node2);
  return MoveChain(Prev(sibling1), sibling1, sibling2) || ok;
}

PairExchangeOperator::PairExchangeOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingIndexPairs& index_pairs)
    : PathOperator(vars, secondary_vars, 2, true, true,
                   std::move(start_empty_path_class)) {
  AddPairAlternativeSets(index_pairs);
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
  // Swapping alternatives in.
  SwapActiveAndInactive(sibling1, BaseSiblingAlternativeNode(0));
  SwapActiveAndInactive(node1, BaseAlternativeNode(0));
  SwapActiveAndInactive(sibling2, BaseSiblingAlternativeNode(1));
  SwapActiveAndInactive(node2, BaseAlternativeNode(1));
  return status;
}

bool PairExchangeOperator::GetPreviousAndSibling(
    int64 node, int64* previous, int64* sibling,
    int64* sibling_previous) const {
  if (IsPathStart(node)) return false;
  *previous = Prev(node);
  *sibling = GetActiveAlternativeSibling(node);
  *sibling_previous = *sibling >= 0 ? Prev(*sibling) : -1;
  return *sibling_previous >= 0;
}

PairExchangeRelocateOperator::PairExchangeRelocateOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingIndexPairs& index_pairs)
    : PathOperator(vars, secondary_vars, 6, true, false,
                   std::move(start_empty_path_class)) {
  AddPairAlternativeSets(index_pairs);
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
  *sibling = GetActiveAlternativeSibling(node);
  *sibling_previous = *sibling >= 0 ? Prev(*sibling) : -1;
  return *sibling_previous >= 0;
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
                          ? int64{0}
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
    : PathOperator(vars, secondary_vars, 1, true, false,
                   std::move(start_empty_path_class)),
      inactive_node_(0) {
  AddPairAlternativeSets(index_pairs);
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
  const int64 next = Next(base);
  const int64 other = GetActiveAlternativeSibling(next);
  if (other != -1) {
    return MakeChainInactive(Prev(other), other) &&
           MakeChainInactive(base, next) && MakeActive(inactive_node_, base);
  }
  return false;
}

void IndexPairSwapActiveOperator::OnNodeInitialization() {
  PathOperator::OnNodeInitialization();
  for (int i = 0; i < Size(); ++i) {
    if (IsInactive(i)) {
      inactive_node_ = i;
      return;
    }
  }
  inactive_node_ = Size();
}

// FilteredHeuristicPathLNSOperator

FilteredHeuristicPathLNSOperator::FilteredHeuristicPathLNSOperator(
    std::unique_ptr<RoutingFilteredHeuristic> heuristic)
    : IntVarLocalSearchOperator(heuristic->model()->Nexts()),
      heuristic_(std::move(heuristic)),
      model_(*heuristic_->model()),
      consider_vehicle_vars_(!model_.CostsAreHomogeneousAcrossVehicles()),
      current_route_(0),
      last_route_(0),
      just_started_(false) {
  if (consider_vehicle_vars_) {
    AddVars(model_.VehicleVars());
  }
}

void FilteredHeuristicPathLNSOperator::OnStart() {
  // NOTE: We set last_route_ to current_route_ here to make sure all routes
  // are scanned in IncrementCurrentRouteToNextNonEmpty().
  last_route_ = current_route_;
  if (CurrentRouteIsEmpty()) {
    IncrementCurrentRouteToNextNonEmpty();
  }
  just_started_ = true;
}

bool FilteredHeuristicPathLNSOperator::MakeOneNeighbor() {
  while (IncrementRoute()) {
    // NOTE: No need to call RevertChanges() here as
    // DestroyRouteAndReinsertNodes() will always return true if any change was
    // made.
    if (DestroyRouteAndReinsertNodes()) {
      return true;
    }
  }
  return false;
}

bool FilteredHeuristicPathLNSOperator::IncrementRoute() {
  if (just_started_) {
    just_started_ = false;
    return !CurrentRouteIsEmpty();
  }
  IncrementCurrentRouteToNextNonEmpty();
  return current_route_ != last_route_;
}

bool FilteredHeuristicPathLNSOperator::CurrentRouteIsEmpty() const {
  return model_.IsEnd(OldValue(model_.Start(current_route_)));
}

void FilteredHeuristicPathLNSOperator::IncrementCurrentRouteToNextNonEmpty() {
  const int num_routes = model_.vehicles();
  do {
    ++current_route_ %= num_routes;
    if (current_route_ == last_route_) {
      // All routes have been scanned.
      return;
    }
  } while (CurrentRouteIsEmpty());
}

bool FilteredHeuristicPathLNSOperator::DestroyRouteAndReinsertNodes() {
  const int64 start_node = model_.Start(current_route_);
  const int64 end_node = model_.End(current_route_);

  const Assignment* const result_assignment =
      heuristic_->BuildSolutionFromRoutes(
          [this, start_node, end_node](int64 node) {
            if (node == start_node) return end_node;
            return Value(node);
          });

  if (result_assignment == nullptr) {
    return false;
  }

  bool has_change = false;
  std::vector<bool> node_performed(model_.Size(), false);
  const std::vector<IntVarElement>& elements =
      result_assignment->IntVarContainer().elements();
  for (int vehicle = 0; vehicle < model_.vehicles(); vehicle++) {
    int64 node_index = model_.Start(vehicle);
    while (!model_.IsEnd(node_index)) {
      // NOTE: When building the solution in the heuristic, Next vars are added
      // to the assignment at the position corresponding to their index.
      const IntVarElement& node_element = elements[node_index];
      DCHECK_EQ(node_element.Var(), model_.NextVar(node_index));

      const int64 new_node_value = node_element.Value();
      DCHECK_NE(new_node_value, node_index);
      node_performed[node_index] = true;

      const int64 vehicle_var_index = VehicleVarIndex(node_index);
      if (OldValue(node_index) != new_node_value ||
          (consider_vehicle_vars_ && OldValue(vehicle_var_index) != vehicle)) {
        has_change = true;
        SetValue(node_index, new_node_value);
        if (consider_vehicle_vars_) {
          SetValue(vehicle_var_index, vehicle);
        }
      }

      node_index = new_node_value;
    }
  }
  for (int64 node = 0; node < model_.Size(); node++) {
    if (node_performed[node]) continue;
    const IntVarElement& node_element = elements[node];
    DCHECK_EQ(node_element.Var(), model_.NextVar(node));
    DCHECK_EQ(node_element.Value(), node);
    if (OldValue(node) != node) {
      has_change = true;
      SetValue(node, node);
      if (consider_vehicle_vars_) {
        const int64 vehicle_var_index = VehicleVarIndex(node);
        DCHECK_NE(OldValue(vehicle_var_index), -1);
        SetValue(vehicle_var_index, -1);
      }
    }
  }
  return has_change;
}

// FilteredHeuristicCloseNodesLNSOperator

FilteredHeuristicCloseNodesLNSOperator::FilteredHeuristicCloseNodesLNSOperator(
    std::unique_ptr<RoutingFilteredHeuristic> heuristic, int num_close_nodes)
    : IntVarLocalSearchOperator(heuristic->model()->Nexts(),
                                /*keep_inverse_values*/ true),
      heuristic_(std::move(heuristic)),
      model_(*heuristic_->model()),
      pickup_delivery_pairs_(model_.GetPickupAndDeliveryPairs()),
      consider_vehicle_vars_(!model_.CostsAreHomogeneousAcrossVehicles()),
      current_node_(0),
      last_node_(0),
      close_nodes_(model_.Size()),
      removed_nodes_(model_.Size()),
      new_nexts_(model_.Size()),
      changed_nexts_(model_.Size()),
      new_prevs_(model_.Size()),
      changed_prevs_(model_.Size()) {
  if (consider_vehicle_vars_) {
    AddVars(model_.VehicleVars());
  }
  const int64 size = model_.Size();
  const int64 max_num_neighbors =
      std::max<int64>(0, size - 1 - model_.vehicles());
  const int64 num_closest_neighbors =
      std::min<int64>(num_close_nodes, max_num_neighbors);
  DCHECK_GE(num_closest_neighbors, 0);

  if (num_closest_neighbors == 0) return;

  const int64 num_cost_classes = model_.GetCostClassesCount();

  for (int64 node = 0; node < size; node++) {
    if (model_.IsStart(node) || model_.IsEnd(node)) continue;

    std::vector<std::pair</*cost*/ double, /*node*/ int64>> costed_after_nodes;
    costed_after_nodes.reserve(size);
    for (int64 after_node = 0; after_node < size; after_node++) {
      if (model_.IsStart(after_node) || model_.IsEnd(after_node) ||
          after_node == node) {
        continue;
      }
      double total_cost = 0.0;
      // NOTE: We don't consider the 'always-zero' cost class when searching for
      // closest neighbors.
      for (int cost_class = 1; cost_class < num_cost_classes; cost_class++) {
        total_cost += model_.GetArcCostForClass(node, after_node, cost_class);
      }
      costed_after_nodes.emplace_back(total_cost, after_node);
    }

    std::nth_element(costed_after_nodes.begin(),
                     costed_after_nodes.begin() + num_closest_neighbors - 1,
                     costed_after_nodes.end());
    std::vector<int64>& neighbors = close_nodes_[node];
    neighbors.reserve(num_closest_neighbors);
    for (int index = 0; index < num_closest_neighbors; index++) {
      neighbors.push_back(costed_after_nodes[index].second);
    }
  }
}

void FilteredHeuristicCloseNodesLNSOperator::OnStart() {
  last_node_ = current_node_;
  just_started_ = true;
}

bool FilteredHeuristicCloseNodesLNSOperator::MakeOneNeighbor() {
  while (IncrementNode()) {
    // NOTE: No need to call RevertChanges() here as
    // RemoveCloseNodesAndReinsert() will always return true if any change was
    // made.
    if (RemoveCloseNodesAndReinsert()) {
      return true;
    }
  }
  return false;
}

bool FilteredHeuristicCloseNodesLNSOperator::IncrementNode() {
  if (just_started_) {
    just_started_ = false;
    return true;
  }
  ++current_node_ %= model_.Size();
  return current_node_ != last_node_;
}

void FilteredHeuristicCloseNodesLNSOperator::RemoveNode(int64 node) {
  DCHECK(!model_.IsEnd(node) && !model_.IsStart(node));
  DCHECK_NE(Value(node), node);
  DCHECK(IsActive(node));

  removed_nodes_.Set(node);
  const int64 prev = Prev(node);
  const int64 next = Next(node);
  changed_nexts_.Set(prev);
  new_nexts_[prev] = next;
  if (next < model_.Size()) {
    changed_prevs_.Set(next);
    new_prevs_[next] = prev;
  }
}

void FilteredHeuristicCloseNodesLNSOperator::RemoveNodeAndActiveSibling(
    int64 node) {
  if (!IsActive(node)) return;
  RemoveNode(node);

  for (int64 sibling_node : GetActiveSiblings(node)) {
    if (!model_.IsStart(sibling_node) && !model_.IsEnd(sibling_node)) {
      RemoveNode(sibling_node);
    }
  }
}

std::vector<int64> FilteredHeuristicCloseNodesLNSOperator::GetActiveSiblings(
    int64 node) const {
  // NOTE: In most use-cases, where each node is a pickup or delivery in a
  // single index pair, this function is in O(k) where k is the number of
  // alternative deliveries or pickups for this index pair.
  std::vector<int64> active_siblings;
  for (std::pair<int64, int64> index_pair : model_.GetPickupIndexPairs(node)) {
    for (int64 sibling_delivery :
         pickup_delivery_pairs_[index_pair.first].second) {
      if (IsActive(sibling_delivery)) {
        active_siblings.push_back(sibling_delivery);
        break;
      }
    }
  }
  for (std::pair<int64, int64> index_pair :
       model_.GetDeliveryIndexPairs(node)) {
    for (int64 sibling_pickup :
         pickup_delivery_pairs_[index_pair.first].first) {
      if (IsActive(sibling_pickup)) {
        active_siblings.push_back(sibling_pickup);
        break;
      }
    }
  }
  return active_siblings;
}

bool FilteredHeuristicCloseNodesLNSOperator::RemoveCloseNodesAndReinsert() {
  if (model_.IsStart(current_node_)) {
    return false;
  }
  DCHECK(!model_.IsEnd(current_node_));

  removed_nodes_.SparseClearAll();
  changed_nexts_.SparseClearAll();
  changed_prevs_.SparseClearAll();

  RemoveNodeAndActiveSibling(current_node_);

  for (int64 neighbor : close_nodes_[current_node_]) {
    RemoveNodeAndActiveSibling(neighbor);
  }

  const Assignment* const result_assignment =
      heuristic_->BuildSolutionFromRoutes(
          [this](int64 node) { return Next(node); });

  if (result_assignment == nullptr) {
    return false;
  }

  bool has_change = false;
  const std::vector<IntVarElement>& elements =
      result_assignment->IntVarContainer().elements();
  for (int vehicle = 0; vehicle < model_.vehicles(); vehicle++) {
    int64 node_index = model_.Start(vehicle);
    while (!model_.IsEnd(node_index)) {
      // NOTE: When building the solution in the heuristic, Next vars are added
      // to the assignment at the position corresponding to their index.
      const IntVarElement& node_element = elements[node_index];
      DCHECK_EQ(node_element.Var(), model_.NextVar(node_index));

      const int64 new_node_value = node_element.Value();
      DCHECK_NE(new_node_value, node_index);

      const int64 vehicle_var_index = VehicleVarIndex(node_index);
      if (OldValue(node_index) != new_node_value ||
          (consider_vehicle_vars_ && OldValue(vehicle_var_index) != vehicle)) {
        has_change = true;
        SetValue(node_index, new_node_value);
        if (consider_vehicle_vars_) {
          SetValue(vehicle_var_index, vehicle);
        }
      }
      node_index = new_node_value;
    }
  }
  // Check for newly unperformed nodes among the ones removed for insertion by
  // the heuristic.
  for (int64 node : removed_nodes_.PositionsSetAtLeastOnce()) {
    const IntVarElement& node_element = elements[node];
    DCHECK_EQ(node_element.Var(), model_.NextVar(node));
    if (node_element.Value() == node) {
      DCHECK_NE(OldValue(node), node);
      has_change = true;
      SetValue(node, node);
      if (consider_vehicle_vars_) {
        const int64 vehicle_var_index = VehicleVarIndex(node);
        DCHECK_NE(OldValue(vehicle_var_index), -1);
        SetValue(vehicle_var_index, -1);
      }
    }
  }
  return has_change;
}

// FilteredHeuristicExpensiveChainLNSOperator

FilteredHeuristicExpensiveChainLNSOperator::
    FilteredHeuristicExpensiveChainLNSOperator(
        std::unique_ptr<RoutingFilteredHeuristic> heuristic,
        int num_arcs_to_consider,
        std::function<int64(int64, int64, int64)> arc_cost_for_route_start)
    : IntVarLocalSearchOperator(heuristic->model()->Nexts()),
      heuristic_(std::move(heuristic)),
      model_(*heuristic_->model()),
      consider_vehicle_vars_(!model_.CostsAreHomogeneousAcrossVehicles()),
      current_route_(0),
      last_route_(0),
      num_arcs_to_consider_(num_arcs_to_consider),
      current_expensive_arc_indices_({-1, -1}),
      arc_cost_for_route_start_(std::move(arc_cost_for_route_start)),
      just_started_(false) {
  DCHECK_GE(num_arcs_to_consider_, 2);
  if (consider_vehicle_vars_) {
    AddVars(model_.VehicleVars());
  }
}

void FilteredHeuristicExpensiveChainLNSOperator::OnStart() {
  last_route_ = current_route_;
  just_started_ = true;
}

bool FilteredHeuristicExpensiveChainLNSOperator::MakeOneNeighbor() {
  while (IncrementPosition()) {
    // NOTE: No need to call RevertChanges() here as
    // DestroyChainAndReinsertNodes() will always return true if any change was
    // made.
    if (DestroyChainAndReinsertNodes()) {
      return true;
    }
  }
  return false;
}

bool FilteredHeuristicExpensiveChainLNSOperator::IncrementPosition() {
  if (just_started_) {
    just_started_ = false;
    return FindMostExpensiveChainsOnRemainingRoutes();
  }

  if (IncrementCurrentArcIndices()) return true;

  return IncrementRoute() && FindMostExpensiveChainsOnRemainingRoutes();
}

bool FilteredHeuristicExpensiveChainLNSOperator::
    DestroyChainAndReinsertNodes() {
  const int first_arc_index = current_expensive_arc_indices_.first;
  const int second_arc_index = current_expensive_arc_indices_.second;
  DCHECK_LE(0, first_arc_index);
  DCHECK_LT(first_arc_index, second_arc_index);
  DCHECK_LT(second_arc_index, most_expensive_arc_starts_and_ranks_.size());

  const std::pair<int, int>& first_start_and_rank =
      most_expensive_arc_starts_and_ranks_[first_arc_index];
  const std::pair<int, int>& second_start_and_rank =
      most_expensive_arc_starts_and_ranks_[second_arc_index];
  int64 before_chain, after_chain;
  if (first_start_and_rank.second < second_start_and_rank.second) {
    before_chain = first_start_and_rank.first;
    after_chain = OldValue(second_start_and_rank.first);
  } else {
    before_chain = second_start_and_rank.first;
    after_chain = OldValue(first_start_and_rank.first);
  }

  const Assignment* const result_assignment =
      heuristic_->BuildSolutionFromRoutes(
          [this, before_chain, after_chain](int64 node) {
            if (node == before_chain) return after_chain;
            return OldValue(node);
          });

  if (result_assignment == nullptr) {
    return false;
  }

  bool has_change = false;
  std::vector<bool> node_performed(model_.Size(), false);
  const std::vector<IntVarElement>& elements =
      result_assignment->IntVarContainer().elements();
  for (int vehicle = 0; vehicle < model_.vehicles(); vehicle++) {
    int64 node_index = model_.Start(vehicle);
    while (!model_.IsEnd(node_index)) {
      // NOTE: When building the solution in the heuristic, Next vars are added
      // to the assignment at the position corresponding to their index.
      const IntVarElement& node_element = elements[node_index];
      DCHECK_EQ(node_element.Var(), model_.NextVar(node_index));

      const int64 new_node_value = node_element.Value();
      DCHECK_NE(new_node_value, node_index);
      node_performed[node_index] = true;

      const int64 vehicle_var_index = VehicleVarIndex(node_index);
      if (OldValue(node_index) != new_node_value ||
          (consider_vehicle_vars_ && OldValue(vehicle_var_index) != vehicle)) {
        has_change = true;
        SetValue(node_index, new_node_value);
        if (consider_vehicle_vars_) {
          SetValue(vehicle_var_index, vehicle);
        }
      }

      node_index = new_node_value;
    }
  }
  for (int64 node = 0; node < model_.Size(); node++) {
    if (node_performed[node]) continue;
    const IntVarElement& node_element = elements[node];
    DCHECK_EQ(node_element.Var(), model_.NextVar(node));
    DCHECK_EQ(node_element.Value(), node);
    if (OldValue(node) != node) {
      has_change = true;
      SetValue(node, node);
      if (consider_vehicle_vars_) {
        const int64 vehicle_var_index = VehicleVarIndex(node);
        DCHECK_NE(OldValue(vehicle_var_index), -1);
        SetValue(vehicle_var_index, -1);
      }
    }
  }
  return has_change;
}

bool FilteredHeuristicExpensiveChainLNSOperator::IncrementRoute() {
  ++current_route_ %= model_.vehicles();
  return current_route_ != last_route_;
}

bool FilteredHeuristicExpensiveChainLNSOperator::IncrementCurrentArcIndices() {
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

namespace {

// Returns false if the route starting with 'start' is empty. Otherwise sets
// most_expensive_arc_starts_and_ranks and first_expensive_arc_indices according
// to the most expensive chains on the route, and returns true.
bool FindMostExpensiveArcsOnRoute(
    int num_arcs, int64 start, const std::function<int64(int64)>& next_accessor,
    const std::function<bool(int64)>& is_end,
    const std::function<int64(int64, int64, int64)>& arc_cost_for_route_start,
    std::vector<std::pair<int64, int>>* most_expensive_arc_starts_and_ranks,
    std::pair<int, int>* first_expensive_arc_indices) {
  if (is_end(next_accessor(start))) {
    // Empty route.
    *first_expensive_arc_indices = {-1, -1};
    return false;
  }

  // NOTE: The negative ranks are so that for a given cost, lower ranks are
  // given higher priority.
  using ArcCostNegativeRankStart = std::tuple<int64, int, int64>;
  std::priority_queue<ArcCostNegativeRankStart,
                      std::vector<ArcCostNegativeRankStart>,
                      std::greater<ArcCostNegativeRankStart>>
      arc_info_pq;

  int64 before_node = start;
  int rank = 0;
  while (!is_end(before_node)) {
    const int64 after_node = next_accessor(before_node);
    const int64 arc_cost =
        arc_cost_for_route_start(before_node, after_node, start);
    arc_info_pq.emplace(arc_cost, -rank, before_node);

    before_node = after_node;
    rank++;

    if (rank > num_arcs) {
      arc_info_pq.pop();
    }
  }

  DCHECK_GE(rank, 2);
  DCHECK_EQ(arc_info_pq.size(), std::min(rank, num_arcs));

  most_expensive_arc_starts_and_ranks->resize(arc_info_pq.size());
  int arc_index = arc_info_pq.size() - 1;
  while (!arc_info_pq.empty()) {
    const ArcCostNegativeRankStart& arc_info = arc_info_pq.top();
    (*most_expensive_arc_starts_and_ranks)[arc_index] = {
        std::get<2>(arc_info), -std::get<1>(arc_info)};
    arc_index--;
    arc_info_pq.pop();
  }

  *first_expensive_arc_indices = {0, 1};
  return true;
}

}  // namespace

bool FilteredHeuristicExpensiveChainLNSOperator::
    FindMostExpensiveChainsOnRemainingRoutes() {
  do {
    if (FindMostExpensiveArcsOnRoute(
            num_arcs_to_consider_, model_.Start(current_route_),
            [this](int64 i) { return OldValue(i); },
            [this](int64 node) { return model_.IsEnd(node); },
            arc_cost_for_route_start_, &most_expensive_arc_starts_and_ranks_,
            &current_expensive_arc_indices_)) {
      return true;
    }
  } while (IncrementRoute());

  return false;
}

RelocateExpensiveChain::RelocateExpensiveChain(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class, int num_arcs_to_consider,
    std::function<int64(int64, int64, int64)> arc_cost_for_path_start)
    : PathOperator(vars, secondary_vars, 1, false, false,
                   std::move(start_empty_path_class)),
      num_arcs_to_consider_(num_arcs_to_consider),
      current_path_(0),
      current_expensive_arc_indices_({-1, -1}),
      arc_cost_for_path_start_(std::move(arc_cost_for_path_start)),
      end_path_(0),
      has_non_empty_paths_to_explore_(false) {
  DCHECK_GE(num_arcs_to_consider_, 2);
}

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
    return CheckChainValidity(first_start_and_rank.first,
                              second_start_and_rank.first, BaseNode(0)) &&
           MoveChain(first_start_and_rank.first, second_start_and_rank.first,
                     BaseNode(0));
  }
  return CheckChainValidity(second_start_and_rank.first,
                            first_start_and_rank.first, BaseNode(0)) &&
         MoveChain(second_start_and_rank.first, first_start_and_rank.first,
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
    if (FindMostExpensiveArcsOnRoute(
            num_arcs_to_consider_, path_starts()[current_path_],
            [this](int64 i) { return OldNext(i); },
            [this](int64 node) { return IsPathEnd(node); },
            arc_cost_for_path_start_, &most_expensive_arc_starts_and_ranks_,
            &current_expensive_arc_indices_)) {
      return true;
    }
    IncrementCurrentPath();
  } while (current_path_ != end_path_);
  return false;
}

RelocateSubtrip::RelocateSubtrip(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingIndexPairs& pairs)
    : PathOperator(vars, secondary_vars,
                   /*number_of_base_nodes*/ 2, true, false,
                   std::move(start_empty_path_class)) {
  is_pickup_node_.resize(number_of_nexts_, false);
  is_delivery_node_.resize(number_of_nexts_, false);
  pair_of_node_.resize(number_of_nexts_, -1);
  for (int pair_index = 0; pair_index < pairs.size(); ++pair_index) {
    for (const int node : pairs[pair_index].first) {
      is_pickup_node_[node] = true;
      pair_of_node_[node] = pair_index;
    }
    for (const int node : pairs[pair_index].second) {
      is_delivery_node_[node] = true;
      pair_of_node_[node] = pair_index;
    }
  }
  opened_pairs_bitset_.resize(pairs.size(), false);
}

bool RelocateSubtrip::RelocateSubTripFromPickup(const int64 chain_first_node,
                                                const int64 insertion_node) {
  if (IsPathEnd(insertion_node)) return false;
  if (Prev(chain_first_node) == insertion_node)
    return false;  // Skip null move.

  int num_opened_pairs = 0;
  // Split chain into subtrip and rejected nodes.
  rejected_nodes_ = {Prev(chain_first_node)};
  subtrip_nodes_ = {insertion_node};
  int current = chain_first_node;
  do {
    if (current == insertion_node) {
      // opened_pairs_bitset_ must be all false when we leave this function.
      opened_pairs_bitset_.assign(opened_pairs_bitset_.size(), false);
      return false;
    }
    const int pair = pair_of_node_[current];
    if (is_delivery_node_[current] && !opened_pairs_bitset_[pair]) {
      rejected_nodes_.push_back(current);
    } else {
      subtrip_nodes_.push_back(current);
      if (is_pickup_node_[current]) {
        ++num_opened_pairs;
        opened_pairs_bitset_[pair] = true;
      } else if (is_delivery_node_[current]) {
        --num_opened_pairs;
        opened_pairs_bitset_[pair] = false;
      }
    }
    current = Next(current);
  } while (num_opened_pairs != 0 && !IsPathEnd(current));
  DCHECK_EQ(num_opened_pairs, 0);
  rejected_nodes_.push_back(current);
  subtrip_nodes_.push_back(Next(insertion_node));

  // Set new paths.
  const int64 rejected_path = Path(chain_first_node);
  for (int i = 1; i < rejected_nodes_.size(); ++i) {
    SetNext(rejected_nodes_[i - 1], rejected_nodes_[i], rejected_path);
  }
  const int64 insertion_path = Path(insertion_node);
  for (int i = 1; i < subtrip_nodes_.size(); ++i) {
    SetNext(subtrip_nodes_[i - 1], subtrip_nodes_[i], insertion_path);
  }
  return true;
}

bool RelocateSubtrip::RelocateSubTripFromDelivery(const int64 chain_last_node,
                                                  const int64 insertion_node) {
  if (IsPathEnd(insertion_node)) return false;

  // opened_pairs_bitset_ should be all false.
  DCHECK(std::none_of(opened_pairs_bitset_.begin(), opened_pairs_bitset_.end(),
                      [](bool value) { return value; }));
  int num_opened_pairs = 0;
  // Split chain into subtrip and rejected nodes. Store nodes in reverse order.
  rejected_nodes_ = {Next(chain_last_node)};
  subtrip_nodes_ = {Next(insertion_node)};
  int current = chain_last_node;
  do {
    if (current == insertion_node) {
      opened_pairs_bitset_.assign(opened_pairs_bitset_.size(), false);
      return false;
    }
    const int pair = pair_of_node_[current];
    if (is_pickup_node_[current] && !opened_pairs_bitset_[pair]) {
      rejected_nodes_.push_back(current);
    } else {
      subtrip_nodes_.push_back(current);
      if (is_delivery_node_[current]) {
        ++num_opened_pairs;
        opened_pairs_bitset_[pair] = true;
      } else if (is_pickup_node_[current]) {
        --num_opened_pairs;
        opened_pairs_bitset_[pair] = false;
      }
    }
    current = Prev(current);
  } while (num_opened_pairs != 0 && !IsPathStart(current));
  DCHECK_EQ(num_opened_pairs, 0);
  if (current == insertion_node) return false;  // Skip null move.
  rejected_nodes_.push_back(current);
  subtrip_nodes_.push_back(insertion_node);

  // TODO(user): either remove those std::reverse() and adapt the loops
  // below, or refactor the loops into a function that also DCHECKs the path.
  std::reverse(rejected_nodes_.begin(), rejected_nodes_.end());
  std::reverse(subtrip_nodes_.begin(), subtrip_nodes_.end());

  // Set new paths.
  const int64 rejected_path = Path(chain_last_node);
  for (int i = 1; i < rejected_nodes_.size(); ++i) {
    SetNext(rejected_nodes_[i - 1], rejected_nodes_[i], rejected_path);
  }
  const int64 insertion_path = Path(insertion_node);
  for (int i = 1; i < subtrip_nodes_.size(); ++i) {
    SetNext(subtrip_nodes_[i - 1], subtrip_nodes_[i], insertion_path);
  }
  return true;
}

bool RelocateSubtrip::MakeNeighbor() {
  if (is_pickup_node_[BaseNode(0)]) {
    return RelocateSubTripFromPickup(BaseNode(0), BaseNode(1));
  } else if (is_delivery_node_[BaseNode(0)]) {
    return RelocateSubTripFromDelivery(BaseNode(0), BaseNode(1));
  } else {
    return false;
  }
}

ExchangeSubtrip::ExchangeSubtrip(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingIndexPairs& pairs)
    : PathOperator(vars, secondary_vars, 2, true, false,
                   std::move(start_empty_path_class)) {
  is_pickup_node_.resize(number_of_nexts_, false);
  is_delivery_node_.resize(number_of_nexts_, false);
  pair_of_node_.resize(number_of_nexts_, -1);
  for (int pair_index = 0; pair_index < pairs.size(); ++pair_index) {
    for (const int node : pairs[pair_index].first) {
      is_pickup_node_[node] = true;
      pair_of_node_[node] = pair_index;
    }
    for (const int node : pairs[pair_index].second) {
      is_delivery_node_[node] = true;
      pair_of_node_[node] = pair_index;
    }
  }
  opened_pairs_set_.resize(pairs.size(), false);
}

void ExchangeSubtrip::SetPath(const std::vector<int64>& path, int path_id) {
  for (int i = 1; i < path.size(); ++i) {
    SetNext(path[i - 1], path[i], path_id);
  }
}

namespace {
bool VectorContains(const std::vector<int64>& values, int64 target) {
  return std::find(values.begin(), values.end(), target) != values.end();
}
}  // namespace

bool ExchangeSubtrip::MakeNeighbor() {
  if (pair_of_node_[BaseNode(0)] == -1) return false;
  if (pair_of_node_[BaseNode(1)] == -1) return false;
  // Break symmetry: a move generated from (BaseNode(0), BaseNode(1)) is the
  // same as from (BaseNode(1), BaseNode(1)): no need to do it twice.
  if (BaseNode(0) >= BaseNode(1)) return false;
  rejects0_.clear();
  subtrip0_.clear();
  if (!ExtractChainsAndCheckCanonical(BaseNode(0), &rejects0_, &subtrip0_)) {
    return false;
  }
  rejects1_.clear();
  subtrip1_.clear();
  if (!ExtractChainsAndCheckCanonical(BaseNode(1), &rejects1_, &subtrip1_)) {
    return false;
  }

  // If paths intersect, skip the move.
  if (Path(BaseNode(0)) == Path(BaseNode(1))) {
    if (VectorContains(rejects0_, subtrip1_.front())) return false;
    if (VectorContains(rejects1_, subtrip0_.front())) return false;
    if (VectorContains(subtrip0_, subtrip1_.front())) return false;
    if (VectorContains(subtrip1_, subtrip0_.front())) return false;
  }

  // Assemble the new paths.
  path0_ = {Prev(subtrip0_.front())};
  path1_ = {Prev(subtrip1_.front())};
  const int64 last0 = Next(subtrip0_.back());
  const int64 last1 = Next(subtrip1_.back());
  const bool concatenated01 = last0 == subtrip1_.front();
  const bool concatenated10 = last1 == subtrip0_.front();

  if (is_delivery_node_[BaseNode(0)]) std::swap(subtrip1_, rejects0_);
  path0_.insert(path0_.end(), subtrip1_.begin(), subtrip1_.end());
  path0_.insert(path0_.end(), rejects0_.begin(), rejects0_.end());
  path0_.push_back(last0);

  if (is_delivery_node_[BaseNode(1)]) std::swap(subtrip0_, rejects1_);
  path1_.insert(path1_.end(), subtrip0_.begin(), subtrip0_.end());
  path1_.insert(path1_.end(), rejects1_.begin(), rejects1_.end());
  path1_.push_back(last1);

  // When the trips are concatenated, bypass the regular extremities.
  if (concatenated01) {
    path0_.pop_back();
    path1_.front() = path0_.back();
  } else if (concatenated10) {
    path1_.pop_back();
    path0_.front() = path1_.back();
  }

  // Change the paths. Since SetNext() modifies Path() values,
  // record path_id0 and path_id11 before calling SetPath();
  const int64 path0_id = Path(BaseNode(0));
  const int64 path1_id = Path(BaseNode(1));
  SetPath(path0_, path0_id);
  SetPath(path1_, path1_id);
  return true;
}

bool ExchangeSubtrip::ExtractChainsAndCheckCanonical(
    int64 base_node, std::vector<int64>* rejects, std::vector<int64>* subtrip) {
  const bool extracted =
      is_pickup_node_[base_node]
          ? ExtractChainsFromPickup(base_node, rejects, subtrip)
          : ExtractChainsFromDelivery(base_node, rejects, subtrip);
  if (!extracted) return false;
  // Check canonicality.
  return !is_delivery_node_[base_node] ||
         pair_of_node_[subtrip->front()] != pair_of_node_[subtrip->back()] ||
         !rejects->empty();
}

bool ExchangeSubtrip::ExtractChainsFromPickup(int64 base_node,
                                              std::vector<int64>* rejects,
                                              std::vector<int64>* subtrip) {
  DCHECK(is_pickup_node_[base_node]);
  DCHECK(rejects->empty());
  DCHECK(subtrip->empty());
  // Iterate from base_node forwards while maintaining the set of opened pairs.
  // A pair is opened by a pickup, closed with the corresponding delivery.
  opened_pairs_set_.assign(opened_pairs_set_.size(), false);
  int num_opened_pairs = 0;
  int current = base_node;
  do {
    const int pair = pair_of_node_[current];
    if (is_delivery_node_[current] && !opened_pairs_set_[pair]) {
      rejects->push_back(current);
    } else {
      subtrip->push_back(current);
      if (is_pickup_node_[current]) {
        ++num_opened_pairs;
        opened_pairs_set_[pair] = true;
      } else if (is_delivery_node_[current]) {
        --num_opened_pairs;
        opened_pairs_set_[pair] = false;
      }
    }
    current = Next(current);
  } while (num_opened_pairs != 0 && !IsPathEnd(current));
  return num_opened_pairs == 0;
}

bool ExchangeSubtrip::ExtractChainsFromDelivery(int64 base_node,
                                                std::vector<int64>* rejects,
                                                std::vector<int64>* subtrip) {
  DCHECK(is_delivery_node_[base_node]);
  DCHECK(rejects->empty());
  DCHECK(subtrip->empty());
  // Iterate from base_node backwards while maintaining the set of opened pairs.
  // A pair is opened by a delivery, closed with the corresponding pickup.
  opened_pairs_set_.assign(opened_pairs_set_.size(), false);
  int num_opened_pairs = 0;
  int current = base_node;
  do {
    const int pair = pair_of_node_[current];
    if (is_pickup_node_[current] && !opened_pairs_set_[pair]) {
      rejects->push_back(current);
    } else {
      subtrip->push_back(current);
      if (is_delivery_node_[current]) {
        ++num_opened_pairs;
        opened_pairs_set_[pair] = true;
      } else if (is_pickup_node_[current]) {
        --num_opened_pairs;
        opened_pairs_set_[pair] = false;
      }
    }
    current = Prev(current);
  } while (num_opened_pairs != 0 && !IsPathStart(current));
  if (num_opened_pairs != 0) return false;
  std::reverse(rejects->begin(), rejects->end());
  std::reverse(subtrip->begin(), subtrip->end());
  return true;
}

}  // namespace operations_research
