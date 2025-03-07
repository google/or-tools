// Copyright 2010-2025 Google LLC
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
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/routing_types.h"
#include "ortools/constraint_solver/routing_utils.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {
using NeighborAccessor =
    std::function<const std::vector<int>&(/*node=*/int, /*start_node=*/int)>;

template <bool ignore_path_vars>
MakeRelocateNeighborsOperator<ignore_path_vars>::MakeRelocateNeighborsOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    NeighborAccessor get_incoming_neighbors,
    NeighborAccessor get_outgoing_neighbors,
    RoutingTransitCallback2 arc_evaluator)
    : PathOperator<ignore_path_vars>(
          vars, secondary_vars,
          /*number_of_base_nodes=*/
          get_incoming_neighbors == nullptr && get_outgoing_neighbors == nullptr
              ? 2
              : 1,
          /*skip_locally_optimal_paths=*/true,
          /*accept_path_end_base=*/false, std::move(start_empty_path_class),
          std::move(get_incoming_neighbors), std::move(get_outgoing_neighbors)),
      arc_evaluator_(std::move(arc_evaluator)) {}

template <bool ignore_path_vars>
bool MakeRelocateNeighborsOperator<ignore_path_vars>::MakeNeighbor() {
  const auto do_move = [this](int64_t before_chain, int64_t destination) {
    int64_t chain_end = this->Next(before_chain);
    if (this->IsPathEnd(chain_end)) return false;
    if (chain_end == destination) return false;
    const int64_t max_arc_value = arc_evaluator_(destination, chain_end);
    int64_t next = this->Next(chain_end);
    while (!this->IsPathEnd(next) &&
           arc_evaluator_(chain_end, next) <= max_arc_value) {
      // We return false here to avoid symmetric moves. The rationale is that
      // if destination is part of the same group as the chain, we probably want
      // to extend the chain to contain it, which means finding another
      // destination further down the path.
      // TODO(user): Add a parameter to either return false or break here,
      // depending if we want to permutate nodes within the same chain.
      if (next == destination) return false;
      chain_end = next;
      next = this->Next(chain_end);
    }
    return MoveChainAndRepair(before_chain, chain_end, destination);
  };
  if (this->HasNeighbors()) {
    const auto [neighbor, outgoing] = this->GetNeighborForBaseNode(0);
    if (neighbor < 0 || this->IsInactive(neighbor)) return false;
    if (!outgoing) {
      // TODO(user): Handle incoming neighbors by going backwards on the
      // chain.
      return false;
    }
    return do_move(/*before_chain=*/this->Prev(neighbor),
                   /*destination=*/this->BaseNode(0));
  } else {
    return do_move(/*before_chain=*/this->BaseNode(0),
                   /*destination=*/this->BaseNode(1));
  }
}

template <bool ignore_path_vars>
bool MakeRelocateNeighborsOperator<ignore_path_vars>::MoveChainAndRepair(
    int64_t before_chain, int64_t chain_end, int64_t destination) {
  if (this->MoveChain(before_chain, chain_end, destination)) {
    if (!this->IsPathStart(destination)) {
      int64_t current = this->Prev(destination);
      int64_t last = chain_end;
      if (current == last) {  // chain was just before destination
        current = before_chain;
      }
      while (last >= 0 && !this->IsPathStart(current) && current != last) {
        last = Reposition(current, last);
        current = this->Prev(current);
      }
    }
    return true;
  }
  return false;
}

template <bool ignore_path_vars>
int64_t MakeRelocateNeighborsOperator<ignore_path_vars>::Reposition(
    int64_t before_to_move, int64_t up_to) {
  const int64_t kNoChange = -1;
  const int64_t to_move = this->Next(before_to_move);
  int64_t next = this->Next(to_move);
  if (this->Var(to_move)->Contains(next)) {
    return kNoChange;
  }
  int64_t prev = next;
  next = this->Next(next);
  while (prev != up_to) {
    if (this->Var(prev)->Contains(to_move) &&
        this->Var(to_move)->Contains(next)) {
      this->MoveChain(before_to_move, to_move, prev);
      return up_to;
    }
    prev = next;
    next = this->Next(next);
  }
  if (this->Var(prev)->Contains(to_move)) {
    this->MoveChain(before_to_move, to_move, prev);
    return to_move;
  }
  return kNoChange;
}

LocalSearchOperator* MakeRelocateNeighbors(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    std::function<const std::vector<int>&(int, int)> get_incoming_neighbors,
    std::function<const std::vector<int>&(int, int)> get_outgoing_neighbors,
    RoutingTransitCallback2 arc_evaluator) {
  if (secondary_vars.empty()) {
    return solver->RevAlloc(new MakeRelocateNeighborsOperator<true>(
        vars, secondary_vars, std::move(start_empty_path_class),
        std::move(get_incoming_neighbors), std::move(get_outgoing_neighbors),
        std::move(arc_evaluator)));
  }
  return solver->RevAlloc(new MakeRelocateNeighborsOperator<false>(
      vars, secondary_vars, std::move(start_empty_path_class),
      std::move(get_incoming_neighbors), std::move(get_outgoing_neighbors),
      std::move(arc_evaluator)));
}

LocalSearchOperator* MakeRelocateNeighbors(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    RoutingTransitCallback2 arc_evaluator) {
  return MakeRelocateNeighbors(solver, vars, secondary_vars,
                               std::move(start_empty_path_class), nullptr,
                               nullptr, std::move(arc_evaluator));
}

ShortestPathOnAlternatives::ShortestPathOnAlternatives(
    int num_nodes, std::vector<std::vector<int64_t>> alternative_sets,
    RoutingTransitCallback2 arc_evaluator)
    : arc_evaluator_(std::move(arc_evaluator)),
      alternative_sets_(std::move(alternative_sets)),
      to_alternative_set_(num_nodes, -1),
      path_predecessor_(num_nodes, -1),
      touched_(num_nodes) {
  for (int i = 0; i < alternative_sets_.size(); ++i) {
    for (int j : alternative_sets_[i]) {
      if (j < to_alternative_set_.size()) to_alternative_set_[j] = i;
    }
  }
  for (int i = 0; i < num_nodes; ++i) {
    if (to_alternative_set_[i] == -1) {
      to_alternative_set_[i] = alternative_sets_.size();
      alternative_sets_.push_back({int64_t{i}});
    }
  }
}

bool ShortestPathOnAlternatives::HasAlternatives(int node) const {
  DCHECK_LT(node, to_alternative_set_.size());
  DCHECK_LT(to_alternative_set_[node], alternative_sets_.size());
  return alternative_sets_[to_alternative_set_[node]].size() > 1;
}

absl::Span<const int64_t> ShortestPathOnAlternatives::GetShortestPath(
    int64_t source, int64_t sink, absl::Span<const int64_t> chain) {
  path_.clear();
  if (chain.empty()) return path_;

  const std::vector<int64_t> source_alternatives = {source};
  auto prev_alternative_set = absl::Span<const int64_t>(source_alternatives);
  std::vector<int64_t> prev_values = {0};

  auto get_best_predecessor = [this, &prev_alternative_set, &prev_values](
                                  int64_t node) -> std::pair<int64_t, int64_t> {
    int64_t predecessor = -1;
    int64_t min_value = kint64max;
    for (int prev_alternative = 0;
         prev_alternative < prev_alternative_set.size(); ++prev_alternative) {
      const int64_t new_value =
          CapAdd(prev_values[prev_alternative],
                 arc_evaluator_(prev_alternative_set[prev_alternative], node));
      if (new_value <= min_value) {
        min_value = new_value;
        predecessor = prev_alternative_set[prev_alternative];
      }
    }
    return {predecessor, min_value};
  };

  // Updating values "layer" by "layer" (each one is fully connected to the
  // previous one).
  for (const int64_t node : chain) {
    const std::vector<int64_t>& current_alternative_set =
        alternative_sets_[to_alternative_set_[node]];
    current_values_.clear();
    current_values_.reserve(current_alternative_set.size());
    for (int alternative_node : current_alternative_set) {
      auto [predecessor, min_value] = get_best_predecessor(alternative_node);
      current_values_.push_back(min_value);
      path_predecessor_[alternative_node] = predecessor;
    }
    prev_alternative_set = absl::MakeConstSpan(current_alternative_set);
    prev_values.swap(current_values_);
  }
  // Get the predecessor in the shortest path to sink in the last layer.
  auto [predecessor, min_value] = get_best_predecessor(sink);
  if (predecessor == -1) return path_;
  // Build the path from predecessors on the shortest path.
  path_.resize(chain.size(), predecessor);
  touched_.ResetAllToFalse();
  touched_.Set(predecessor);
  for (int rank = chain.size() - 2; rank >= 0; --rank) {
    path_[rank] = path_predecessor_[path_[rank + 1]];
    if (touched_[path_[rank]]) {
      path_.clear();
      return path_;
    }
    touched_.Set(path_[rank]);
  }
  return path_;
}

template <bool ignore_path_vars>
TwoOptWithShortestPathOperator<ignore_path_vars>::
    TwoOptWithShortestPathOperator(
        const std::vector<IntVar*>& vars,
        const std::vector<IntVar*>& secondary_vars,
        std::function<int(int64_t)> start_empty_path_class,
        std::vector<std::vector<int64_t>> alternative_sets,
        RoutingTransitCallback2 arc_evaluator)
    : PathOperator<ignore_path_vars>(
          vars, secondary_vars, /*number_of_base_nodes=*/2,
          /*skip_locally_optimal_paths=*/true,
          /*accept_path_end_base=*/true, std::move(start_empty_path_class),
          nullptr, nullptr),
      shortest_path_manager_(vars.size(), std::move(alternative_sets),
                             std::move(arc_evaluator)) {}

template <bool ignore_path_vars>
bool TwoOptWithShortestPathOperator<ignore_path_vars>::MakeNeighbor() {
  DCHECK_EQ(this->StartNode(0), this->StartNode(1));
  const int64_t before_chain = this->BaseNode(0);
  if (this->IsPathEnd(before_chain)) {
    ResetChainStatus();
    return false;
  }
  const int64_t after_chain = this->BaseNode(1);
  bool has_alternatives = false;
  if (before_chain != after_chain) {
    const int64_t prev_after_chain = this->Prev(after_chain);
    if (prev_after_chain != before_chain &&
        chain_status_.start == before_chain &&
        chain_status_.end == prev_after_chain) {
      has_alternatives =
          chain_status_.has_alternatives ||
          shortest_path_manager_.HasAlternatives(prev_after_chain);
    } else {
      // Non-incremental computation of alternative presence. The chains are
      // small by definition.
      for (int64_t node = this->Next(before_chain); node != after_chain;
           node = this->Next(node)) {
        has_alternatives |= shortest_path_manager_.HasAlternatives(node);
      }
    }
  }
  chain_status_.start = before_chain;
  chain_status_.end = after_chain;
  chain_status_.has_alternatives = has_alternatives;
  if (!has_alternatives) return false;
  int64_t chain_last;
  if (!this->ReverseChain(before_chain, after_chain, &chain_last)) return false;
  chain_.clear();
  for (int64_t next = this->Next(before_chain); next != after_chain;
       next = this->Next(next)) {
    chain_.push_back(next);
  }
  // The neighbor is accepted if there were actual changes, either we reverted a
  // chain with more than one node, or alternatives were swapped.
  return this->SwapActiveAndInactiveChains(
             chain_, shortest_path_manager_.GetShortestPath(
                         before_chain, after_chain, chain_)) ||
         chain_.size() > 1;
}

LocalSearchOperator* MakeTwoOptWithShortestPath(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    std::vector<std::vector<int64_t>> alternative_sets,
    RoutingTransitCallback2 arc_evaluator) {
  if (secondary_vars.empty()) {
    return solver->RevAlloc(new TwoOptWithShortestPathOperator<true>(
        vars, secondary_vars, std::move(start_empty_path_class),
        std::move(alternative_sets), std::move(arc_evaluator)));
  }
  return solver->RevAlloc(new TwoOptWithShortestPathOperator<false>(
      vars, secondary_vars, std::move(start_empty_path_class),
      std::move(alternative_sets), std::move(arc_evaluator)));
}

template <bool ignore_path_vars>
SwapActiveToShortestPathOperator<ignore_path_vars>::
    SwapActiveToShortestPathOperator(
        const std::vector<IntVar*>& vars,
        const std::vector<IntVar*>& secondary_vars,
        std::function<int(int64_t)> start_empty_path_class,
        std::vector<std::vector<int64_t>> alternative_sets,
        RoutingTransitCallback2 arc_evaluator)
    : PathOperator<ignore_path_vars>(
          vars, secondary_vars, /*number_of_base_nodes=*/1,
          /*skip_locally_optimal_paths=*/true,
          /*accept_path_end_base=*/false, std::move(start_empty_path_class),
          nullptr, nullptr),
      shortest_path_manager_(vars.size(), std::move(alternative_sets),
                             std::move(arc_evaluator)) {}

template <bool ignore_path_vars>
bool SwapActiveToShortestPathOperator<ignore_path_vars>::MakeNeighbor() {
  const int64_t before_chain = this->BaseNode(0);
  if (!this->IsPathStart(before_chain) &&
      shortest_path_manager_.HasAlternatives(before_chain)) {
    return false;
  }
  int64_t next = this->Next(before_chain);
  chain_.clear();
  while (!this->IsPathEnd(next) &&
         shortest_path_manager_.HasAlternatives(next)) {
    chain_.push_back(next);
    next = this->Next(next);
  }
  return this->SwapActiveAndInactiveChains(
      chain_, shortest_path_manager_.GetShortestPath(
                  /*source=*/before_chain, /*sink=*/next, chain_));
}

LocalSearchOperator* MakeSwapActiveToShortestPath(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    std::vector<std::vector<int64_t>> alternative_sets,
    RoutingTransitCallback2 arc_evaluator) {
  if (secondary_vars.empty()) {
    return solver->RevAlloc(new SwapActiveToShortestPathOperator<true>(
        vars, secondary_vars, std::move(start_empty_path_class),
        std::move(alternative_sets), std::move(arc_evaluator)));
  }
  return solver->RevAlloc(new SwapActiveToShortestPathOperator<false>(
      vars, secondary_vars, std::move(start_empty_path_class),
      std::move(alternative_sets), std::move(arc_evaluator)));
}

template <bool ignore_path_vars>
MakePairActiveOperator<ignore_path_vars>::MakePairActiveOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    const std::vector<PickupDeliveryPair>& pairs)
    : PathOperator<ignore_path_vars>(vars, secondary_vars, 2, false, true,
                                     std::move(start_empty_path_class), nullptr,
                                     nullptr),
      inactive_pair_(0),
      inactive_pair_first_index_(0),
      inactive_pair_second_index_(0),
      pairs_(pairs) {}

template <bool ignore_path_vars>
bool MakePairActiveOperator<ignore_path_vars>::MakeOneNeighbor() {
  while (inactive_pair_ < pairs_.size()) {
    if (PathOperator<ignore_path_vars>::MakeOneNeighbor()) return true;
    this->ResetPosition();
    const auto& [pickup_alternatives, delivery_alternatives] =
        pairs_[inactive_pair_];
    if (inactive_pair_first_index_ < pickup_alternatives.size() - 1) {
      ++inactive_pair_first_index_;
    } else if (inactive_pair_second_index_ < delivery_alternatives.size() - 1) {
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

template <bool ignore_path_vars>
bool MakePairActiveOperator<ignore_path_vars>::MakeNeighbor() {
  DCHECK_EQ(this->StartNode(0), this->StartNode(1));
  // Inserting the second node of the pair before the first one which ensures
  // that the only solutions where both nodes are next to each other have the
  // first node before the second (the move is not symmetric and doing it this
  // way ensures that a potential precedence constraint between the nodes of the
  // pair is not violated).
  const auto& [pickup_alternatives, delivery_alternatives] =
      pairs_[inactive_pair_];
  return this->MakeActive(delivery_alternatives[inactive_pair_second_index_],
                          this->BaseNode(1)) &&
         this->MakeActive(pickup_alternatives[inactive_pair_first_index_],
                          this->BaseNode(0));
}

template <bool ignore_path_vars>
int64_t MakePairActiveOperator<ignore_path_vars>::GetBaseNodeRestartPosition(
    int base_index) {
  // Base node 1 must be after base node 0 if they are both on the same path.
  if (base_index == 0 ||
      this->StartNode(base_index) != this->StartNode(base_index - 1)) {
    return this->StartNode(base_index);
  } else {
    return this->BaseNode(base_index - 1);
  }
}

template <bool ignore_path_vars>
void MakePairActiveOperator<ignore_path_vars>::OnNodeInitialization() {
  inactive_pair_ = FindNextInactivePair(0);
  inactive_pair_first_index_ = 0;
  inactive_pair_second_index_ = 0;
}

template <bool ignore_path_vars>
int MakePairActiveOperator<ignore_path_vars>::FindNextInactivePair(
    int pair_index) const {
  for (int index = pair_index; index < pairs_.size(); ++index) {
    if (!ContainsActiveNodes(pairs_[index].pickup_alternatives) &&
        !ContainsActiveNodes(pairs_[index].delivery_alternatives)) {
      return index;
    }
  }
  return pairs_.size();
}

template <bool ignore_path_vars>
bool MakePairActiveOperator<ignore_path_vars>::ContainsActiveNodes(
    absl::Span<const int64_t> nodes) const {
  for (int64_t node : nodes) {
    if (!this->IsInactive(node)) return true;
  }
  return false;
}

LocalSearchOperator* MakePairActive(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    const std::vector<PickupDeliveryPair>& pairs) {
  if (secondary_vars.empty()) {
    return solver->RevAlloc(new MakePairActiveOperator<true>(
        vars, secondary_vars, std::move(start_empty_path_class), pairs));
  }
  return solver->RevAlloc(new MakePairActiveOperator<false>(
      vars, secondary_vars, std::move(start_empty_path_class), pairs));
}

template <bool ignore_path_vars>
MakePairInactiveOperator<ignore_path_vars>::MakePairInactiveOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    const std::vector<PickupDeliveryPair>& pairs)
    : PathOperator<ignore_path_vars>(vars, secondary_vars, 1, true, false,
                                     std::move(start_empty_path_class), nullptr,
                                     nullptr) {
  this->AddPairAlternativeSets(pairs);
}

template <bool ignore_path_vars>
bool MakePairInactiveOperator<ignore_path_vars>::MakeNeighbor() {
  const int64_t base = this->BaseNode(0);
  const int64_t first_index = this->Next(base);
  const int64_t second_index = this->GetActiveAlternativeSibling(first_index);
  if (second_index < 0) {
    return false;
  }
  return this->MakeChainInactive(base, first_index) &&
         this->MakeChainInactive(this->Prev(second_index), second_index);
}

LocalSearchOperator* MakePairInactive(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    const std::vector<PickupDeliveryPair>& pairs) {
  if (secondary_vars.empty()) {
    return solver->RevAlloc(new MakePairInactiveOperator<true>(
        vars, secondary_vars, std::move(start_empty_path_class), pairs));
  }
  return solver->RevAlloc(new MakePairInactiveOperator<false>(
      vars, secondary_vars, std::move(start_empty_path_class), pairs));
}

template <bool ignore_path_vars>
PairRelocateOperator<ignore_path_vars>::PairRelocateOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    const std::vector<PickupDeliveryPair>& pairs)
    : PathOperator<ignore_path_vars>(vars, secondary_vars, 3, true, false,
                                     std::move(start_empty_path_class), nullptr,
                                     nullptr) {
  // TODO(user): Add a version where a (first_node, second_node) pair are
  // added respectively after first_node_neighbor and second_node_neighbor.
  // This requires a complete restructuring of the code, since we would require
  // scanning neighbors for a non-base node (second_node is an active sibling
  // of first_node).
  this->AddPairAlternativeSets(pairs);
}

template <bool ignore_path_vars>
bool PairRelocateOperator<ignore_path_vars>::MakeNeighbor() {
  DCHECK_EQ(this->StartNode(1), this->StartNode(2));
  const int64_t first_pair_node = this->BaseNode(kPairFirstNode);
  if (this->IsPathStart(first_pair_node)) {
    return false;
  }
  int64_t first_prev = this->Prev(first_pair_node);
  const int second_pair_node =
      this->GetActiveAlternativeSibling(first_pair_node);
  if (second_pair_node < 0 || this->IsPathEnd(second_pair_node) ||
      this->IsPathStart(second_pair_node)) {
    return false;
  }
  const int64_t second_prev = this->Prev(second_pair_node);

  const int64_t first_node_destination =
      this->BaseNode(kPairFirstNodeDestination);
  if (first_node_destination == second_pair_node) {
    // The second_pair_node -> first_pair_node link is forbidden.
    return false;
  }

  const int64_t second_node_destination =
      this->BaseNode(kPairSecondNodeDestination);
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
      this->MoveChain(second_prev, second_pair_node, second_node_destination);
  // Explicitly calling Prev as second_pair_node might have been moved before
  // first_pair_node.
  const bool moved_first_pair_node = this->MoveChain(
      this->Prev(first_pair_node), first_pair_node, first_node_destination);
  // Swapping alternatives in.
  this->SwapActiveAndInactive(second_pair_node,
                              this->BaseSiblingAlternativeNode(kPairFirstNode));
  this->SwapActiveAndInactive(first_pair_node,
                              this->BaseAlternativeNode(kPairFirstNode));
  return moved_first_pair_node || moved_second_pair_node;
}

template <bool ignore_path_vars>
int64_t PairRelocateOperator<ignore_path_vars>::GetBaseNodeRestartPosition(
    int base_index) {
  // Destination node of the second node of a pair must be after the
  // destination node of the first node of a pair.
  if (base_index == kPairSecondNodeDestination) {
    return this->BaseNode(kPairFirstNodeDestination);
  } else {
    return this->StartNode(base_index);
  }
}

LocalSearchOperator* MakePairRelocate(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    const std::vector<PickupDeliveryPair>& pairs) {
  if (secondary_vars.empty()) {
    return solver->RevAlloc(new PairRelocateOperator<true>(
        vars, secondary_vars, std::move(start_empty_path_class), pairs));
  }
  return solver->RevAlloc(new PairRelocateOperator<false>(
      vars, secondary_vars, std::move(start_empty_path_class), pairs));
}

template <bool ignore_path_vars>
GroupPairAndRelocateOperator<ignore_path_vars>::GroupPairAndRelocateOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class, NeighborAccessor,
    NeighborAccessor get_outgoing_neighbors,
    const std::vector<PickupDeliveryPair>& pairs)
    : PathOperator<ignore_path_vars>(
          vars, secondary_vars,
          /*number_of_base_nodes=*/
          get_outgoing_neighbors == nullptr ? 2 : 1,
          /*skip_locally_optimal_paths=*/true,
          /*accept_path_end_base=*/false, std::move(start_empty_path_class),
          nullptr,  // We don't use incoming neighbors for this operator.
          std::move(get_outgoing_neighbors)) {
  this->AddPairAlternativeSets(pairs);
}

template <bool ignore_path_vars>
bool GroupPairAndRelocateOperator<ignore_path_vars>::MakeNeighbor() {
  const auto do_move = [this](int64_t node, int64_t destination) {
    if (this->IsPathEnd(node) || this->IsInactive(node)) return false;
    const int64_t sibling = this->GetActiveAlternativeSibling(node);
    if (sibling == -1) return false;
    // Skip redundant cases.
    if (destination == node || destination == sibling) return false;
    const bool ok = this->MoveChain(this->Prev(node), node, destination);
    return this->MoveChain(this->Prev(sibling), sibling, node) || ok;
  };
  if (this->HasNeighbors()) {
    const auto [neighbor, outgoing] = this->GetNeighborForBaseNode(0);
    if (neighbor < 0) return false;
    DCHECK(outgoing);
    return do_move(/*node=*/neighbor, /*destination=*/this->BaseNode(0));
  }
  return do_move(/*node=*/this->Next(this->BaseNode(0)),
                 /*destination=*/this->BaseNode(1));
}

LocalSearchOperator* MakeGroupPairAndRelocate(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    std::function<const std::vector<int>&(int, int)> get_incoming_neighbors,
    std::function<const std::vector<int>&(int, int)> get_outgoing_neighbors,
    const std::vector<PickupDeliveryPair>& pairs) {
  if (secondary_vars.empty()) {
    return solver->RevAlloc(new GroupPairAndRelocateOperator<true>(
        vars, secondary_vars, std::move(start_empty_path_class),
        std::move(get_incoming_neighbors), std::move(get_outgoing_neighbors),
        pairs));
  } else {
    return solver->RevAlloc(new GroupPairAndRelocateOperator<false>(
        vars, secondary_vars, std::move(start_empty_path_class),
        std::move(get_incoming_neighbors), std::move(get_outgoing_neighbors),
        pairs));
  }
}

LocalSearchOperator* MakeGroupPairAndRelocate(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    const std::vector<PickupDeliveryPair>& pairs) {
  return MakeGroupPairAndRelocate(solver, vars, secondary_vars,
                                  std::move(start_empty_path_class), nullptr,
                                  nullptr, pairs);
}

template <bool ignore_path_vars>
LightPairRelocateOperator<ignore_path_vars>::LightPairRelocateOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class, NeighborAccessor,
    NeighborAccessor get_outgoing_neighbors,
    const std::vector<PickupDeliveryPair>& pairs,
    std::function<bool(int64_t)> force_lifo)
    : PathOperator<ignore_path_vars>(
          vars, secondary_vars,
          /*number_of_base_nodes=*/
          get_outgoing_neighbors == nullptr ? 2 : 1,
          /*skip_locally_optimal_paths=*/true,
          /*accept_path_end_base=*/false, std::move(start_empty_path_class),
          nullptr,  // Incoming neighbors not used as of 09/2024.
          std::move(get_outgoing_neighbors)),
      force_lifo_(std::move(force_lifo)) {
  this->AddPairAlternativeSets(pairs);
}

template <bool ignore_path_vars>
bool LightPairRelocateOperator<ignore_path_vars>::MakeNeighbor() {
  const auto do_move = [this](int64_t node, int64_t destination,
                              bool destination_is_lifo) {
    if (this->IsPathStart(node) || this->IsPathEnd(node) ||
        this->IsInactive(node)) {
      return false;
    }
    const int64_t prev = this->Prev(node);
    if (this->IsPathEnd(node)) return false;
    const int64_t sibling = this->GetActiveAlternativeSibling(node);
    if (sibling == -1 || destination == sibling) return false;

    // Note: MoveChain will return false if it is a no-op (moving the chain to
    // its current position). However we want to accept the move if at least
    // node or sibling gets moved to a new position. Therefore we want to be
    // sure both MoveChains are called and at least one succeeds.

    // Special case handling relocating the first node of a pair "before" the
    // first node of another pair. Limiting this to relocating after the start
    // of the path as other moves will be mostly equivalent to relocating
    // "after".
    // TODO(user): extend to relocating before the start of sub-tours (when
    // all pairs have been matched).
    if (this->IsPathStart(destination)) {
      const bool ok = this->MoveChain(prev, node, destination);
      const int64_t destination_sibling =
          this->GetActiveAlternativeSibling(this->Next(node));
      if (destination_sibling == -1) {
        // Not inserting before a pair node: insert sibling after node.
        return this->MoveChain(this->Prev(sibling), sibling, node) || ok;
      } else {
        // Depending on the lifo status of the path, insert sibling before or
        // after destination_sibling since node is being inserted before
        // next(destination).
        if (!destination_is_lifo) {
          if (this->Prev(destination_sibling) == sibling) return ok;
          return this->MoveChain(this->Prev(sibling), sibling,
                                 this->Prev(destination_sibling)) ||
                 ok;
        } else {
          return this->MoveChain(this->Prev(sibling), sibling,
                                 destination_sibling) ||
                 ok;
        }
      }
    }
    // Relocating the first node of a pair "after" the first node of another
    // pair.
    const int64_t destination_sibling =
        this->GetActiveAlternativeSibling(destination);
    if (destination_sibling == -1) return false;
    const bool ok = this->MoveChain(prev, node, destination);
    if (!destination_is_lifo) {
      return this->MoveChain(this->Prev(sibling), sibling,
                             destination_sibling) ||
             ok;
    } else {
      if (this->Prev(destination_sibling) == sibling) return ok;
      return this->MoveChain(this->Prev(sibling), sibling,
                             this->Prev(destination_sibling)) ||
             ok;
    }
  };
  if (this->HasNeighbors()) {
    const auto [neighbor, outgoing] = this->GetNeighborForBaseNode(0);
    if (neighbor < 0) return false;
    // TODO(user): Add support for incoming neighbors.
    DCHECK(outgoing);
    // TODO(user): Add support for lifo for neighbor-based move.
    return do_move(/*node=*/neighbor, /*destination=*/this->BaseNode(0),
                   /*destination_is_lifo=*/false);
  }
  return do_move(/*node=*/this->Next(this->BaseNode(0)),
                 /*destination=*/this->BaseNode(1),
                 force_lifo_ != nullptr && force_lifo_(this->StartNode(1)));
}

LocalSearchOperator* MakeLightPairRelocate(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    std::function<const std::vector<int>&(int, int)> get_incoming_neighbors,
    std::function<const std::vector<int>&(int, int)> get_outgoing_neighbors,
    const std::vector<PickupDeliveryPair>& pairs,
    std::function<bool(int64_t)> force_lifo) {
  if (secondary_vars.empty()) {
    return solver->RevAlloc(new LightPairRelocateOperator<true>(
        vars, secondary_vars, std::move(start_empty_path_class),
        std::move(get_incoming_neighbors), std::move(get_outgoing_neighbors),
        pairs, std::move(force_lifo)));
  }
  return solver->RevAlloc(new LightPairRelocateOperator<false>(
      vars, secondary_vars, std::move(start_empty_path_class),
      std::move(get_incoming_neighbors), std::move(get_outgoing_neighbors),
      pairs, std::move(force_lifo)));
}

LocalSearchOperator* MakeLightPairRelocate(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    const std::vector<PickupDeliveryPair>& pairs,
    std::function<bool(int64_t)> force_lifo) {
  return MakeLightPairRelocate(solver, vars, secondary_vars,
                               std::move(start_empty_path_class), nullptr,
                               nullptr, pairs, std::move(force_lifo));
}

template <bool ignore_path_vars>
PairExchangeOperator<ignore_path_vars>::PairExchangeOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    NeighborAccessor get_incoming_neighbors,
    NeighborAccessor get_outgoing_neighbors,
    const std::vector<PickupDeliveryPair>& pairs)
    : PathOperator<ignore_path_vars>(
          vars, secondary_vars,
          /*number_of_base_nodes=*/
          get_incoming_neighbors == nullptr && get_outgoing_neighbors == nullptr
              ? 2
              : 1,
          /*skip_locally_optimal_paths=*/true,
          /*accept_path_end_base=*/false, std::move(start_empty_path_class),
          std::move(get_incoming_neighbors),
          std::move(get_outgoing_neighbors)) {
  this->AddPairAlternativeSets(pairs);
}

template <bool ignore_path_vars>
bool PairExchangeOperator<ignore_path_vars>::MakeNeighbor() {
  const int64_t node1 = this->BaseNode(0);
  int64_t prev1, sibling1, sibling_prev1 = -1;
  if (!GetPreviousAndSibling(node1, &prev1, &sibling1, &sibling_prev1)) {
    return false;
  }
  int64_t node2 = -1;
  if (!this->HasNeighbors()) {
    node2 = this->BaseNode(1);
  } else {
    const auto [neighbor, outgoing] = this->GetNeighborForBaseNode(0);
    if (neighbor < 0 || this->IsInactive(neighbor)) return false;
    if (outgoing) {
      if (this->IsPathStart(neighbor)) return false;
    } else if (this->IsPathEnd(neighbor)) {
      return false;
    }
    node2 = outgoing ? this->Prev(neighbor) : this->Next(neighbor);
    if (this->IsPathEnd(node2)) return false;
  }
  int64_t prev2, sibling2, sibling_prev2 = -1;
  if (!GetPreviousAndSibling(node2, &prev2, &sibling2, &sibling_prev2)) {
    return false;
  }
  bool status = true;
  // Exchanging node1 and node2.
  if (node1 == prev2) {
    status = this->MoveChain(prev2, node2, prev1);
    if (sibling_prev1 == node2) sibling_prev1 = node1;
    if (sibling_prev2 == node2) sibling_prev2 = node1;
  } else if (node2 == prev1) {
    status = this->MoveChain(prev1, node1, prev2);
    if (sibling_prev1 == node1) sibling_prev1 = node2;
    if (sibling_prev2 == node1) sibling_prev2 = node2;
  } else {
    status = this->MoveChain(prev1, node1, node2) &&
             this->MoveChain(prev2, node2, prev1);
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
    status = this->MoveChain(sibling_prev2, sibling2, sibling_prev1);
  } else if (sibling2 == sibling_prev1) {
    status = this->MoveChain(sibling_prev1, sibling1, sibling_prev2);
  } else {
    status = this->MoveChain(sibling_prev1, sibling1, sibling2) &&
             this->MoveChain(sibling_prev2, sibling2, sibling_prev1);
  }
  // Swapping alternatives in.
  this->SwapActiveAndInactive(sibling1, this->BaseSiblingAlternativeNode(0));
  this->SwapActiveAndInactive(node1, this->BaseAlternativeNode(0));
  if (!this->HasNeighbors()) {
    // TODO(user): Support alternatives with neighbors.
    this->SwapActiveAndInactive(sibling2, this->BaseSiblingAlternativeNode(1));
    this->SwapActiveAndInactive(node2, this->BaseAlternativeNode(1));
  }
  return status;
}

template <bool ignore_path_vars>
bool PairExchangeOperator<ignore_path_vars>::GetPreviousAndSibling(
    int64_t node, int64_t* previous, int64_t* sibling,
    int64_t* sibling_previous) const {
  if (this->IsPathStart(node)) return false;
  *previous = this->Prev(node);
  *sibling = this->GetActiveAlternativeSibling(node);
  *sibling_previous = *sibling >= 0 ? this->Prev(*sibling) : -1;
  return *sibling_previous >= 0;
}

LocalSearchOperator* MakePairExchange(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    std::function<const std::vector<int>&(int, int)> get_incoming_neighbors,
    std::function<const std::vector<int>&(int, int)> get_outgoing_neighbors,
    const std::vector<PickupDeliveryPair>& pairs) {
  if (secondary_vars.empty()) {
    return solver->RevAlloc(new PairExchangeOperator<true>(
        vars, secondary_vars, std::move(start_empty_path_class),
        std::move(get_incoming_neighbors), std::move(get_outgoing_neighbors),
        pairs));
  }
  return solver->RevAlloc(new PairExchangeOperator<false>(
      vars, secondary_vars, std::move(start_empty_path_class),
      std::move(get_incoming_neighbors), std::move(get_outgoing_neighbors),
      pairs));
}

LocalSearchOperator* MakePairExchange(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    const std::vector<PickupDeliveryPair>& pairs) {
  return MakePairExchange(solver, vars, secondary_vars,
                          std::move(start_empty_path_class), nullptr, nullptr,
                          pairs);
}

template <bool ignore_path_vars>
PairExchangeRelocateOperator<ignore_path_vars>::PairExchangeRelocateOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    const std::vector<PickupDeliveryPair>& pairs)
    : PathOperator<ignore_path_vars>(vars, secondary_vars, 6, true, false,
                                     std::move(start_empty_path_class), nullptr,
                                     nullptr) {
  this->AddPairAlternativeSets(pairs);
}

template <bool ignore_path_vars>
bool PairExchangeRelocateOperator<ignore_path_vars>::MakeNeighbor() {
  DCHECK_EQ(this->StartNode(kSecondPairFirstNodeDestination),
            this->StartNode(kSecondPairSecondNodeDestination));
  DCHECK_EQ(this->StartNode(kSecondPairFirstNode),
            this->StartNode(kFirstPairFirstNodeDestination));
  DCHECK_EQ(this->StartNode(kSecondPairFirstNode),
            this->StartNode(kFirstPairSecondNodeDestination));

  if (this->StartNode(kFirstPairFirstNode) ==
      this->StartNode(kSecondPairFirstNode)) {
    this->SetNextBaseToIncrement(kSecondPairFirstNode);
    return false;
  }
  // Through this method, <base>[X][Y] represent the <base> variable for the
  // node Y of pair X. <base> is in node, prev, dest.
  int64_t nodes[2][2];
  int64_t prev[2][2];
  int64_t dest[2][2];
  nodes[0][0] = this->BaseNode(kFirstPairFirstNode);
  nodes[1][0] = this->BaseNode(kSecondPairFirstNode);
  if (nodes[1][0] <= nodes[0][0]) {
    // Exchange is symmetric.
    this->SetNextBaseToIncrement(kSecondPairFirstNode);
    return false;
  }
  if (!this->GetPreviousAndSibling(nodes[0][0], &prev[0][0], &nodes[0][1],
                                   &prev[0][1])) {
    this->SetNextBaseToIncrement(kFirstPairFirstNode);
    return false;
  }
  if (!this->GetPreviousAndSibling(nodes[1][0], &prev[1][0], &nodes[1][1],
                                   &prev[1][1])) {
    this->SetNextBaseToIncrement(kSecondPairFirstNode);
    return false;
  }

  if (!this->LoadAndCheckDest(0, 0, kFirstPairFirstNodeDestination, nodes,
                              dest)) {
    this->SetNextBaseToIncrement(kFirstPairFirstNodeDestination);
    return false;
  }
  if (!this->LoadAndCheckDest(0, 1, kFirstPairSecondNodeDestination, nodes,
                              dest)) {
    this->SetNextBaseToIncrement(kFirstPairSecondNodeDestination);
    return false;
  }
  if (this->StartNode(kSecondPairFirstNodeDestination) !=
          this->StartNode(kFirstPairFirstNode) ||
      !this->LoadAndCheckDest(1, 0, kSecondPairFirstNodeDestination, nodes,
                              dest)) {
    this->SetNextBaseToIncrement(kSecondPairFirstNodeDestination);
    return false;
  }
  if (!this->LoadAndCheckDest(1, 1, kSecondPairSecondNodeDestination, nodes,
                              dest)) {
    this->SetNextBaseToIncrement(kSecondPairSecondNodeDestination);
    return false;
  }

  if (!this->MoveNode(0, 1, nodes, dest, prev)) {
    this->SetNextBaseToIncrement(kFirstPairSecondNodeDestination);
    return false;
  }
  if (!this->MoveNode(0, 0, nodes, dest, prev)) {
    this->SetNextBaseToIncrement(kFirstPairSecondNodeDestination);
    return false;
  }
  if (!this->MoveNode(1, 1, nodes, dest, prev)) {
    return false;
  }
  if (!this->MoveNode(1, 0, nodes, dest, prev)) {
    return false;
  }
  return true;
}

template <bool ignore_path_vars>
bool PairExchangeRelocateOperator<ignore_path_vars>::MoveNode(
    int pair, int node, int64_t nodes[2][2], int64_t dest[2][2],
    int64_t prev[2][2]) {
  if (!this->MoveChain(prev[pair][node], nodes[pair][node], dest[pair][node])) {
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

template <bool ignore_path_vars>
bool PairExchangeRelocateOperator<ignore_path_vars>::LoadAndCheckDest(
    int pair, int node, int64_t base_node, int64_t nodes[2][2],
    int64_t dest[2][2]) const {
  dest[pair][node] = this->BaseNode(base_node);
  // A destination cannot be a node that will be moved.
  return !(nodes[0][0] == dest[pair][node] || nodes[0][1] == dest[pair][node] ||
           nodes[1][0] == dest[pair][node] || nodes[1][1] == dest[pair][node]);
}

template <bool ignore_path_vars>
bool PairExchangeRelocateOperator<ignore_path_vars>::OnSamePathAsPreviousBase(
    int64_t base_index) {
  // Ensuring the destination of the first pair is on the route of the second.
  // pair.
  // Ensuring that destination of both nodes of a pair are on the same route.
  return base_index == kFirstPairFirstNodeDestination ||
         base_index == kFirstPairSecondNodeDestination ||
         base_index == kSecondPairSecondNodeDestination;
}

template <bool ignore_path_vars>
int64_t
PairExchangeRelocateOperator<ignore_path_vars>::GetBaseNodeRestartPosition(
    int base_index) {
  if (base_index == kFirstPairSecondNodeDestination ||
      base_index == kSecondPairSecondNodeDestination) {
    return this->BaseNode(base_index - 1);
  } else {
    return this->StartNode(base_index);
  }
}

template <bool ignore_path_vars>
bool PairExchangeRelocateOperator<ignore_path_vars>::GetPreviousAndSibling(
    int64_t node, int64_t* previous, int64_t* sibling,
    int64_t* sibling_previous) const {
  if (this->IsPathStart(node)) return false;
  *previous = this->Prev(node);
  *sibling = this->GetActiveAlternativeSibling(node);
  *sibling_previous = *sibling >= 0 ? this->Prev(*sibling) : -1;
  return *sibling_previous >= 0;
}

LocalSearchOperator* MakePairExchangeRelocate(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    const std::vector<PickupDeliveryPair>& pairs) {
  if (secondary_vars.empty()) {
    return solver->RevAlloc(new PairExchangeRelocateOperator<true>(
        vars, secondary_vars, std::move(start_empty_path_class), pairs));
  }
  return solver->RevAlloc(new PairExchangeRelocateOperator<false>(
      vars, secondary_vars, std::move(start_empty_path_class), pairs));
}

SwapIndexPairOperator::SwapIndexPairOperator(
    const std::vector<IntVar*>& vars, const std::vector<IntVar*>& path_vars,
    const std::vector<PickupDeliveryPair>& pairs)
    : IntVarLocalSearchOperator(vars),
      pairs_(pairs),
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
  const int64_t kNoPath = -1;
  CHECK(delta != nullptr);
  while (true) {
    RevertChanges(true);

    if (pair_index_ >= pairs_.size()) return false;
    const int64_t path =
        ignore_path_vars_ ? 0LL : Value(first_active_ + number_of_nexts_);
    const int64_t prev_first = prevs_[first_active_];
    const int64_t next_first = Value(first_active_);
    // Making current active "pickup" unperformed.
    SetNext(first_active_, first_active_, kNoPath);
    // Inserting "pickup" alternative at the same position.
    const auto& [pickup_alternatives, delivery_alternatives] =
        pairs_[pair_index_];
    const int64_t insert_first = pickup_alternatives[first_index_];
    SetNext(prev_first, insert_first, path);
    SetNext(insert_first, next_first, path);
    int64_t prev_second = prevs_[second_active_];
    if (prev_second == first_active_) {
      prev_second = insert_first;
    }
    DCHECK_EQ(path, ignore_path_vars_
                        ? int64_t{0}
                        : Value(second_active_ + number_of_nexts_));
    const int64_t next_second = Value(second_active_);
    // Making current active "delivery" unperformed.
    SetNext(second_active_, second_active_, kNoPath);
    // Inserting "delivery" alternative at the same position.
    const int64_t insert_second = delivery_alternatives[second_index_];
    SetNext(prev_second, insert_second, path);
    SetNext(insert_second, next_second, path);
    // Move to next "pickup/delivery" alternative.
    ++second_index_;
    if (second_index_ >= delivery_alternatives.size()) {
      second_index_ = 0;
      ++first_index_;
      if (first_index_ >= pickup_alternatives.size()) {
        first_index_ = 0;
        while (true) {
          ++pair_index_;
          if (!UpdateActiveNodes()) break;
          if (first_active_ != -1 && second_active_ != -1) {
            break;
          }
        }
      }
    }

    if (ApplyChanges(delta, deltadelta)) return true;
  }
  return false;
}

void SwapIndexPairOperator::OnStart() {
  prevs_.resize(number_of_nexts_, -1);
  for (int index = 0; index < number_of_nexts_; ++index) {
    const int64_t next = Value(index);
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
  if (pair_index_ < pairs_.size()) {
    const auto& [pickup_alternatives, delivery_alternatives] =
        pairs_[pair_index_];
    first_active_ = -1;
    second_active_ = -1;
    if (pickup_alternatives.size() == 1 && delivery_alternatives.size() == 1) {
      // When there are no alternatives, the pair should be ignored whether
      // there are active nodes or not.
      return true;
    }
    for (const int64_t first : pickup_alternatives) {
      if (Value(first) != first) {
        first_active_ = first;
        break;
      }
    }
    for (const int64_t second : delivery_alternatives) {
      if (Value(second) != second) {
        second_active_ = second;
        break;
      }
    }
    return true;
  }
  return false;
}

template <bool ignore_path_vars>
IndexPairSwapActiveOperator<ignore_path_vars>::IndexPairSwapActiveOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    const std::vector<PickupDeliveryPair>& pairs)
    : PathOperator<ignore_path_vars>(vars, secondary_vars, 1, true, false,
                                     std::move(start_empty_path_class), nullptr,
                                     nullptr),
      inactive_node_(0) {
  this->AddPairAlternativeSets(pairs);
}

template <bool ignore_path_vars>
bool IndexPairSwapActiveOperator<ignore_path_vars>::MakeNextNeighbor(
    Assignment* delta, Assignment* deltadelta) {
  while (inactive_node_ < this->Size()) {
    if (!this->IsInactive(inactive_node_) ||
        !PathOperator<ignore_path_vars>::MakeNextNeighbor(delta, deltadelta)) {
      this->ResetPosition();
      ++inactive_node_;
    } else {
      return true;
    }
  }
  return false;
}

template <bool ignore_path_vars>
bool IndexPairSwapActiveOperator<ignore_path_vars>::MakeNeighbor() {
  const int64_t base = this->BaseNode(0);
  const int64_t next = this->Next(base);
  const int64_t other = this->GetActiveAlternativeSibling(next);
  if (other != -1) {
    return this->MakeChainInactive(this->Prev(other), other) &&
           this->MakeChainInactive(base, next) &&
           this->MakeActive(inactive_node_, base);
  }
  return false;
}

template <bool ignore_path_vars>
void IndexPairSwapActiveOperator<ignore_path_vars>::OnNodeInitialization() {
  PathOperator<ignore_path_vars>::OnNodeInitialization();
  for (int i = 0; i < this->Size(); ++i) {
    if (this->IsInactive(i)) {
      inactive_node_ = i;
      return;
    }
  }
  inactive_node_ = this->Size();
}

LocalSearchOperator* MakeIndexPairSwapActive(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    const std::vector<PickupDeliveryPair>& pairs) {
  if (secondary_vars.empty()) {
    return solver->RevAlloc(new IndexPairSwapActiveOperator<true>(
        vars, secondary_vars, std::move(start_empty_path_class), pairs));
  }
  return solver->RevAlloc(new IndexPairSwapActiveOperator<false>(
      vars, secondary_vars, std::move(start_empty_path_class), pairs));
}

template <bool ignore_path_vars>
RelocateExpensiveChain<ignore_path_vars>::RelocateExpensiveChain(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    int num_arcs_to_consider,
    std::function<int64_t(int64_t, int64_t, int64_t)> arc_cost_for_path_start)
    : PathOperator<ignore_path_vars>(vars, secondary_vars, 1, false, false,
                                     std::move(start_empty_path_class), nullptr,
                                     nullptr),
      num_arcs_to_consider_(num_arcs_to_consider),
      current_path_(0),
      current_expensive_arc_indices_({-1, -1}),
      arc_cost_for_path_start_(std::move(arc_cost_for_path_start)),
      end_path_(0),
      has_non_empty_paths_to_explore_(false) {
  DCHECK_GE(num_arcs_to_consider_, 2);
}

template <bool ignore_path_vars>
bool RelocateExpensiveChain<ignore_path_vars>::MakeNeighbor() {
  // TODO(user): Consider node neighbors? The operator would no longer be
  // a path operator though, because we would no longer have any base nodes.
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
    return this->CheckChainValidity(first_start_and_rank.first,
                                    second_start_and_rank.first,
                                    this->BaseNode(0)) &&
           this->MoveChain(first_start_and_rank.first,
                           second_start_and_rank.first, this->BaseNode(0));
  }
  return this->CheckChainValidity(second_start_and_rank.first,
                                  first_start_and_rank.first,
                                  this->BaseNode(0)) &&
         this->MoveChain(second_start_and_rank.first,
                         first_start_and_rank.first, this->BaseNode(0));
}

template <bool ignore_path_vars>
bool RelocateExpensiveChain<ignore_path_vars>::MakeOneNeighbor() {
  while (has_non_empty_paths_to_explore_) {
    if (!PathOperator<ignore_path_vars>::MakeOneNeighbor()) {
      this->ResetPosition();
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

template <bool ignore_path_vars>
void RelocateExpensiveChain<ignore_path_vars>::OnNodeInitialization() {
  if (current_path_ >= this->path_starts().size()) {
    // current_path_ was made empty by last move (and it was the last non-empty
    // path), restart from 0.
    current_path_ = 0;
  }
  end_path_ = current_path_;
  has_non_empty_paths_to_explore_ = FindMostExpensiveChainsOnRemainingPaths();
}

template <bool ignore_path_vars>
void RelocateExpensiveChain<ignore_path_vars>::IncrementCurrentPath() {
  const int num_paths = this->path_starts().size();
  if (++current_path_ == num_paths) {
    current_path_ = 0;
  }
}

template <bool ignore_path_vars>
bool RelocateExpensiveChain<ignore_path_vars>::IncrementCurrentArcIndices() {
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

template <bool ignore_path_vars>
bool RelocateExpensiveChain<
    ignore_path_vars>::FindMostExpensiveChainsOnRemainingPaths() {
  do {
    if (FindMostExpensiveArcsOnRoute(
            num_arcs_to_consider_, this->path_starts()[current_path_],
            [this](int64_t i) { return this->OldNext(i); },
            [this](int64_t node) { return this->IsPathEnd(node); },
            arc_cost_for_path_start_, &most_expensive_arc_starts_and_ranks_,
            &current_expensive_arc_indices_)) {
      return true;
    }
    IncrementCurrentPath();
  } while (current_path_ != end_path_);
  return false;
}

LocalSearchOperator* MakeRelocateExpensiveChain(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    int num_arcs_to_consider,
    std::function<int64_t(int64_t, int64_t, int64_t)> arc_cost_for_path_start) {
  if (secondary_vars.empty()) {
    return solver->RevAlloc(new RelocateExpensiveChain<true>(
        vars, secondary_vars, std::move(start_empty_path_class),
        num_arcs_to_consider, std::move(arc_cost_for_path_start)));
  }
  return solver->RevAlloc(new RelocateExpensiveChain<false>(
      vars, secondary_vars, std::move(start_empty_path_class),
      num_arcs_to_consider, std::move(arc_cost_for_path_start)));
}

PickupAndDeliveryData::PickupAndDeliveryData(
    int num_nodes, absl::Span<const PickupDeliveryPair> pairs)
    : is_pickup_node_(num_nodes, false),
      is_delivery_node_(num_nodes, false),
      pair_of_node_(num_nodes, -1) {
  for (int pair_index = 0; pair_index < pairs.size(); ++pair_index) {
    for (const int node : pairs[pair_index].pickup_alternatives) {
      is_pickup_node_[node] = true;
      pair_of_node_[node] = pair_index;
    }
    for (const int node : pairs[pair_index].delivery_alternatives) {
      is_delivery_node_[node] = true;
      pair_of_node_[node] = pair_index;
    }
  }
}

template <bool ignore_path_vars>
RelocateSubtrip<ignore_path_vars>::RelocateSubtrip(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class, NeighborAccessor,
    NeighborAccessor get_outgoing_neighbors,
    absl::Span<const PickupDeliveryPair> pairs)
    : PathOperator<ignore_path_vars>(
          vars, secondary_vars,
          /*number_of_base_nodes=*/
          get_outgoing_neighbors == nullptr ? 2 : 1,
          /*skip_locally_optimal_paths=*/true,
          /*accept_path_end_base=*/false, std::move(start_empty_path_class),
          nullptr,  // Incoming neighbors aren't supported as of 09/2024.
          std::move(get_outgoing_neighbors)),
      pd_data_(this->number_of_nexts_, pairs) {
  opened_pairs_set_.resize(pairs.size(), false);
}

template <bool ignore_path_vars>
void RelocateSubtrip<ignore_path_vars>::SetPath(absl::Span<const int64_t> path,
                                                int path_id) {
  for (int i = 1; i < path.size(); ++i) {
    this->SetNext(path[i - 1], path[i], path_id);
  }
}

template <bool ignore_path_vars>
bool RelocateSubtrip<ignore_path_vars>::RelocateSubTripFromPickup(
    const int64_t chain_first_node, const int64_t insertion_node) {
  if (this->IsPathEnd(insertion_node)) return false;
  if (this->Prev(chain_first_node) == insertion_node)
    return false;  // Skip null move.

  int num_opened_pairs = 0;
  // Split chain into subtrip and rejected nodes.
  rejected_nodes_ = {this->Prev(chain_first_node)};
  subtrip_nodes_ = {insertion_node};
  int current = chain_first_node;
  do {
    if (current == insertion_node) {
      // opened_pairs_set_ must be all false when we leave this function.
      opened_pairs_set_.assign(opened_pairs_set_.size(), false);
      return false;
    }
    const int pair = pd_data_.GetPairOfNode(current);
    if (pd_data_.IsDeliveryNode(current) && !opened_pairs_set_[pair]) {
      rejected_nodes_.push_back(current);
    } else {
      subtrip_nodes_.push_back(current);
      if (pd_data_.IsPickupNode(current)) {
        ++num_opened_pairs;
        opened_pairs_set_[pair] = true;
      } else if (pd_data_.IsDeliveryNode(current)) {
        --num_opened_pairs;
        opened_pairs_set_[pair] = false;
      }
    }
    current = this->Next(current);
  } while (num_opened_pairs != 0 && !this->IsPathEnd(current));
  DCHECK_EQ(num_opened_pairs, 0);
  rejected_nodes_.push_back(current);
  subtrip_nodes_.push_back(this->Next(insertion_node));

  // Set new paths.
  SetPath(rejected_nodes_, this->Path(chain_first_node));
  SetPath(subtrip_nodes_, this->Path(insertion_node));
  return true;
}

template <bool ignore_path_vars>
bool RelocateSubtrip<ignore_path_vars>::RelocateSubTripFromDelivery(
    const int64_t chain_last_node, const int64_t insertion_node) {
  if (this->IsPathEnd(insertion_node)) return false;

  // opened_pairs_set_ should be all false.
  DCHECK(std::none_of(opened_pairs_set_.begin(), opened_pairs_set_.end(),
                      [](bool value) { return value; }));
  int num_opened_pairs = 0;
  // Split chain into subtrip and rejected nodes. Store nodes in reverse order.
  rejected_nodes_ = {this->Next(chain_last_node)};
  subtrip_nodes_ = {this->Next(insertion_node)};
  int current = chain_last_node;
  do {
    if (current == insertion_node) {
      opened_pairs_set_.assign(opened_pairs_set_.size(), false);
      return false;
    }
    const int pair = pd_data_.GetPairOfNode(current);
    if (pd_data_.IsPickupNode(current) && !opened_pairs_set_[pair]) {
      rejected_nodes_.push_back(current);
    } else {
      subtrip_nodes_.push_back(current);
      if (pd_data_.IsDeliveryNode(current)) {
        ++num_opened_pairs;
        opened_pairs_set_[pair] = true;
      } else if (pd_data_.IsPickupNode(current)) {
        --num_opened_pairs;
        opened_pairs_set_[pair] = false;
      }
    }
    current = this->Prev(current);
  } while (num_opened_pairs != 0 && !this->IsPathStart(current));
  DCHECK_EQ(num_opened_pairs, 0);
  if (current == insertion_node) return false;  // Skip null move.
  rejected_nodes_.push_back(current);
  subtrip_nodes_.push_back(insertion_node);

  // TODO(user): either remove those std::reverse() and adapt the loops
  // below, or refactor the loops into a function that also DCHECKs the path.
  std::reverse(rejected_nodes_.begin(), rejected_nodes_.end());
  std::reverse(subtrip_nodes_.begin(), subtrip_nodes_.end());

  // Set new paths.
  SetPath(rejected_nodes_, this->Path(chain_last_node));
  SetPath(subtrip_nodes_, this->Path(insertion_node));
  return true;
}

template <bool ignore_path_vars>
bool RelocateSubtrip<ignore_path_vars>::MakeNeighbor() {
  const auto do_move = [this](int64_t node, int64_t insertion_node) {
    if (this->IsInactive(node)) return false;
    if (pd_data_.IsPickupNode(node)) {
      return RelocateSubTripFromPickup(node, insertion_node);
    } else if (pd_data_.IsDeliveryNode(node)) {
      return RelocateSubTripFromDelivery(node, insertion_node);
    } else {
      return false;
    }
  };
  if (this->HasNeighbors()) {
    const auto [neighbor, outgoing] = this->GetNeighborForBaseNode(0);
    if (neighbor < 0) return false;
    DCHECK(outgoing);
    if (this->IsInactive(neighbor)) return false;
    return do_move(/*node=*/neighbor, /*insertion_node=*/this->BaseNode(0));
  }
  return do_move(/*node=*/this->BaseNode(0),
                 /*insertion_node=*/this->BaseNode(1));
}

LocalSearchOperator* MakeRelocateSubtrip(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    std::function<const std::vector<int>&(int, int)> get_incoming_neighbors,
    std::function<const std::vector<int>&(int, int)> get_outgoing_neighbors,
    absl::Span<const PickupDeliveryPair> pairs) {
  if (secondary_vars.empty()) {
    return solver->RevAlloc(new RelocateSubtrip<true>(
        vars, secondary_vars, std::move(start_empty_path_class),
        std::move(get_incoming_neighbors), std::move(get_outgoing_neighbors),
        pairs));
  }
  return solver->RevAlloc(new RelocateSubtrip<false>(
      vars, secondary_vars, std::move(start_empty_path_class),
      std::move(get_incoming_neighbors), std::move(get_outgoing_neighbors),
      pairs));
}

LocalSearchOperator* MakeRelocateSubtrip(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    absl::Span<const PickupDeliveryPair> pairs) {
  return MakeRelocateSubtrip(solver, vars, secondary_vars,
                             std::move(start_empty_path_class), nullptr,
                             nullptr, pairs);
}

template <bool ignore_path_vars>
ExchangeSubtrip<ignore_path_vars>::ExchangeSubtrip(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class, NeighborAccessor,
    NeighborAccessor get_outgoing_neighbors,
    absl::Span<const PickupDeliveryPair> pairs)
    : PathOperator<ignore_path_vars>(
          vars, secondary_vars,
          /*number_of_base_nodes=*/
          get_outgoing_neighbors == nullptr ? 2 : 1,
          /*skip_locally_optimal_paths=*/true,
          /*accept_path_end_base=*/false, std::move(start_empty_path_class),
          nullptr,  // Incoming neighbors aren't supported as of 09/2024.
          std::move(get_outgoing_neighbors)),
      pd_data_(this->number_of_nexts_, pairs) {
  opened_pairs_set_.resize(pairs.size(), false);
}

template <bool ignore_path_vars>
void ExchangeSubtrip<ignore_path_vars>::SetPath(absl::Span<const int64_t> path,
                                                int path_id) {
  for (int i = 1; i < path.size(); ++i) {
    this->SetNext(path[i - 1], path[i], path_id);
  }
}

namespace {
bool VectorContains(absl::Span<const int64_t> values, int64_t target) {
  return std::find(values.begin(), values.end(), target) != values.end();
}
}  // namespace

template <bool ignore_path_vars>
bool ExchangeSubtrip<ignore_path_vars>::MakeNeighbor() {
  int64_t node0 = -1;
  int64_t node1 = -1;
  if (this->HasNeighbors()) {
    const int64_t node = this->BaseNode(0);
    const auto [neighbor, outgoing] = this->GetNeighborForBaseNode(0);
    if (neighbor < 0) return false;
    DCHECK(outgoing);
    if (this->IsInactive(neighbor)) return false;
    if (pd_data_.IsDeliveryNode(node) &&
        pd_data_.IsDeliveryNode(this->Prev(neighbor))) {
      node0 = node;
      node1 = this->Prev(neighbor);
    } else if (pd_data_.IsPickupNode(neighbor) &&
               !this->IsPathEnd(this->Next(node)) &&
               pd_data_.IsPickupNode(this->Next(node))) {
      node0 = this->Next(node);
      node1 = neighbor;
    } else {
      return false;
    }
  } else {
    node0 = this->BaseNode(0);
    node1 = this->BaseNode(1);
  }

  if (pd_data_.GetPairOfNode(node0) == -1) return false;
  if (pd_data_.GetPairOfNode(node1) == -1) return false;
  // Break symmetry: a move generated from (node0, node1) is the
  // same as from (node1, node0): no need to do it twice.
  if (node0 >= node1) return false;
  rejects0_.clear();
  subtrip0_.clear();
  if (!ExtractChainsAndCheckCanonical(node0, &rejects0_, &subtrip0_)) {
    return false;
  }
  rejects1_.clear();
  subtrip1_.clear();
  if (!ExtractChainsAndCheckCanonical(node1, &rejects1_, &subtrip1_)) {
    return false;
  }

  // If paths intersect, skip the move.
  if (this->HasNeighbors() || this->StartNode(0) == this->StartNode(1)) {
    if (VectorContains(rejects0_, subtrip1_.front())) return false;
    if (VectorContains(rejects1_, subtrip0_.front())) return false;
    if (VectorContains(subtrip0_, subtrip1_.front())) return false;
    if (VectorContains(subtrip1_, subtrip0_.front())) return false;
  }

  // Assemble the new paths.
  path0_ = {this->Prev(subtrip0_.front())};
  path1_ = {this->Prev(subtrip1_.front())};
  const int64_t last0 = this->Next(subtrip0_.back());
  const int64_t last1 = this->Next(subtrip1_.back());
  const bool concatenated01 = last0 == subtrip1_.front();
  const bool concatenated10 = last1 == subtrip0_.front();

  if (pd_data_.IsDeliveryNode(node0)) std::swap(subtrip1_, rejects0_);
  path0_.insert(path0_.end(), subtrip1_.begin(), subtrip1_.end());
  path0_.insert(path0_.end(), rejects0_.begin(), rejects0_.end());
  path0_.push_back(last0);

  if (pd_data_.IsDeliveryNode(node1)) std::swap(subtrip0_, rejects1_);
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
  const int64_t path0_id = this->Path(node0);
  const int64_t path1_id = this->Path(node1);
  SetPath(path0_, path0_id);
  SetPath(path1_, path1_id);
  return true;
}

template <bool ignore_path_vars>
bool ExchangeSubtrip<ignore_path_vars>::ExtractChainsAndCheckCanonical(
    int64_t base_node, std::vector<int64_t>* rejects,
    std::vector<int64_t>* subtrip) {
  const bool extracted =
      pd_data_.IsPickupNode(base_node)
          ? ExtractChainsFromPickup(base_node, rejects, subtrip)
          : ExtractChainsFromDelivery(base_node, rejects, subtrip);
  if (!extracted) return false;
  // Check canonicality.
  return !pd_data_.IsDeliveryNode(base_node) ||
         pd_data_.GetPairOfNode(subtrip->front()) !=
             pd_data_.GetPairOfNode(subtrip->back()) ||
         !rejects->empty();
}

template <bool ignore_path_vars>
bool ExchangeSubtrip<ignore_path_vars>::ExtractChainsFromPickup(
    int64_t base_node, std::vector<int64_t>* rejects,
    std::vector<int64_t>* subtrip) {
  DCHECK(pd_data_.IsPickupNode(base_node));
  DCHECK(rejects->empty());
  DCHECK(subtrip->empty());
  // Iterate from base_node forwards while maintaining the set of opened pairs.
  // A pair is opened by a pickup, closed with the corresponding delivery.
  opened_pairs_set_.assign(opened_pairs_set_.size(), false);
  int num_opened_pairs = 0;
  int current = base_node;
  do {
    const int pair = pd_data_.GetPairOfNode(current);
    if (pd_data_.IsDeliveryNode(current) && !opened_pairs_set_[pair]) {
      rejects->push_back(current);
    } else {
      subtrip->push_back(current);
      if (pd_data_.IsPickupNode(current)) {
        ++num_opened_pairs;
        opened_pairs_set_[pair] = true;
      } else if (pd_data_.IsDeliveryNode(current)) {
        --num_opened_pairs;
        opened_pairs_set_[pair] = false;
      }
    }
    current = this->Next(current);
  } while (num_opened_pairs != 0 && !this->IsPathEnd(current));
  return num_opened_pairs == 0;
}

template <bool ignore_path_vars>
bool ExchangeSubtrip<ignore_path_vars>::ExtractChainsFromDelivery(
    int64_t base_node, std::vector<int64_t>* rejects,
    std::vector<int64_t>* subtrip) {
  DCHECK(pd_data_.IsDeliveryNode(base_node));
  DCHECK(rejects->empty());
  DCHECK(subtrip->empty());
  // Iterate from base_node backwards while maintaining the set of opened pairs.
  // A pair is opened by a delivery, closed with the corresponding pickup.
  opened_pairs_set_.assign(opened_pairs_set_.size(), false);
  int num_opened_pairs = 0;
  int current = base_node;
  do {
    const int pair = pd_data_.GetPairOfNode(current);
    if (pd_data_.IsPickupNode(current) && !opened_pairs_set_[pair]) {
      rejects->push_back(current);
    } else {
      subtrip->push_back(current);
      if (pd_data_.IsDeliveryNode(current)) {
        ++num_opened_pairs;
        opened_pairs_set_[pair] = true;
      } else if (pd_data_.IsPickupNode(current)) {
        --num_opened_pairs;
        opened_pairs_set_[pair] = false;
      }
    }
    current = this->Prev(current);
  } while (num_opened_pairs != 0 && !this->IsPathStart(current));
  if (num_opened_pairs != 0) return false;
  std::reverse(rejects->begin(), rejects->end());
  std::reverse(subtrip->begin(), subtrip->end());
  return true;
}

LocalSearchOperator* MakeExchangeSubtrip(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    std::function<const std::vector<int>&(int, int)> get_incoming_neighbors,
    std::function<const std::vector<int>&(int, int)> get_outgoing_neighbors,
    absl::Span<const PickupDeliveryPair> pairs) {
  if (secondary_vars.empty()) {
    return solver->RevAlloc(new ExchangeSubtrip<true>(
        vars, secondary_vars, std::move(start_empty_path_class),
        std::move(get_incoming_neighbors), std::move(get_outgoing_neighbors),
        pairs));
  }
  return solver->RevAlloc(new ExchangeSubtrip<false>(
      vars, secondary_vars, std::move(start_empty_path_class),
      std::move(get_incoming_neighbors), std::move(get_outgoing_neighbors),
      pairs));
}

LocalSearchOperator* MakeExchangeSubtrip(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    absl::Span<const PickupDeliveryPair> pairs) {
  return MakeExchangeSubtrip(solver, vars, secondary_vars,
                             std::move(start_empty_path_class), nullptr,
                             nullptr, pairs);
}

}  // namespace operations_research
