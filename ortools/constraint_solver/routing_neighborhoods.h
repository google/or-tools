// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_NEIGHBORHOODS_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_NEIGHBORHOODS_H_

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/routing_types.h"
#include "ortools/util/bitset.h"

namespace operations_research {

/// Relocate neighborhood which moves chains of neighbors.
/// The operator starts by relocating a node n after a node m, then continues
/// moving nodes which were after n as long as the "cost" added is less than
/// the "cost" of the arc (m, n). If the new chain doesn't respect the domain of
/// next variables, it will try reordering the nodes.
/// Possible neighbors for path 1 -> A -> B -> C -> D -> E -> 2 (where (1, 2)
/// are first and last nodes of the path and can therefore not be moved, A must
/// be performed before B, and A, D and E are located at the same place):
/// 1 -> A -> C -> [B] -> D -> E -> 2
/// 1 -> A -> C -> D -> [B] -> E -> 2
/// 1 -> A -> C -> D -> E -> [B] -> 2
/// 1 -> A -> B -> D -> [C] -> E -> 2
/// 1 -> A -> B -> D -> E -> [C] -> 2
/// 1 -> A -> [D] -> [E] -> B -> C -> 2
/// 1 -> A -> B -> [D] -> [E] ->  C -> 2
/// 1 -> A -> [E] -> B -> C -> D -> 2
/// 1 -> A -> B -> [E] -> C -> D -> 2
/// 1 -> A -> B -> C -> [E] -> D -> 2
/// This operator is extremely useful to move chains of nodes which are located
/// at the same place (for instance nodes part of a same stop).
// TODO(user): Consider merging with standard Relocate in local_search.cc.
class MakeRelocateNeighborsOperator : public PathOperator {
 public:
  MakeRelocateNeighborsOperator(
      const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& secondary_vars,
      std::function<int(int64_t)> start_empty_path_class,
      std::function<const std::vector<int>&(int, int)> get_neighbors,
      RoutingTransitCallback2 arc_evaluator);
  MakeRelocateNeighborsOperator(
      const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& secondary_vars,
      std::function<int(int64_t)> start_empty_path_class,
      RoutingTransitCallback2 arc_evaluator)
      : MakeRelocateNeighborsOperator(vars, secondary_vars,
                                      std::move(start_empty_path_class),
                                      nullptr, std::move(arc_evaluator)) {}
  ~MakeRelocateNeighborsOperator() override {}

  bool MakeNeighbor() override;
  std::string DebugString() const override { return "RelocateNeighbors"; }

 private:
  /// Moves a chain starting after 'before_chain' and ending at 'chain_end'
  /// after node 'destination'. Tries to repair the resulting solution by
  /// checking if the new arc created after 'destination' is compatible with
  /// NextVar domains, and moves the 'destination' down the path if the solution
  /// is inconsistent. Iterates the process on the new arcs created before
  /// the node 'destination' (if destination was moved).
  bool MoveChainAndRepair(int64_t before_chain, int64_t chain_end,
                          int64_t destination);

  /// Moves node after 'before_to_move' down the path until a position is found
  /// where NextVar domains are not violated, if it exists. Stops when reaching
  /// position after 'up_to'.
  /// If the node was not moved (either because the current position does not
  /// violate any domains or because not such position could be found), returns
  /// -1. If the node was moved to a new position before up_to, returns up_to;
  /// if it was moved just after up_to returns the node which was after up_to.
  int64_t Reposition(int64_t before_to_move, int64_t up_to);

  RoutingTransitCallback2 arc_evaluator_;
};

// Swaps active nodes from node alternatives in sequence. Considers chains of
// nodes with alternatives, builds a DAG from the chain, each "layer" of the DAG
// being composed of the set of alternatives of the node at a given rank in the
// chain, fully connected to the next layer. A neighbor is built from the
// shortest path starting from the node before the chain (source), through the
// DAG to the node following the chain. The path is valued with a given
// callback.
// Example:
// Alternative sets: {1,2} and {3,4}
// Current path: 0 -> 1 -> 3 -> 5
// DAG + source and sink:  -> 1 ---> 3 --
//                        |    \ /       v
//                        0     X        5
//                        |    / \       ^
//                         -> 2 ---> 4 --
// Supposing the shortest path from 0 to 5 is 0, 2, 3, 5, the neighbor for the
// chain will be: 0 -> 2 -> 3 -> 5.
// TODO(user): Support vehicle-class-dependent arc_evaluators.
class SwapActiveToShortestPathOperator : public PathOperator {
 public:
  SwapActiveToShortestPathOperator(
      const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& secondary_vars,
      std::function<int(int64_t)> start_empty_path_class,
      std::vector<std::vector<int64_t>> alternative_sets,
      RoutingTransitCallback2 arc_evaluator);
  ~SwapActiveToShortestPathOperator() override {}
  bool MakeNeighbor() override;
  std::string DebugString() const override {
    return "SwapActiveToShortestPath";
  }

 private:
  const std::vector<int64_t>& GetShortestPath(
      int source, int sink, const std::vector<int>& alternative_chain);

  RoutingTransitCallback2 arc_evaluator_;
  const std::vector<std::vector<int64_t>> alternative_sets_;
  std::vector<int> to_alternative_set_;
  std::vector<int64_t> path_predecessor_;
  std::vector<int64_t> path_;
  SparseBitset<int64_t> touched_;
};

/// Pair-based neighborhood operators, designed to move nodes by pairs (pairs
/// are static and given). These neighborhoods are very useful for Pickup and
/// Delivery problems where pickup and delivery nodes must remain on the same
/// route.
// TODO(user): Add option to prune neighbords where the order of node pairs
//                is violated (ie precedence between pickup and delivery nodes).
// TODO(user): Move this to local_search.cc if it's generic enough.
// TODO(user): Detect pairs automatically by parsing the constraint model;
//                we could then get rid of the pair API in the RoutingModel
//                class.

/// Operator which inserts pairs of inactive nodes into a path.
/// Possible neighbors for the path 1 -> 2 -> 3 with pair (A, B) inactive
/// (where 1 and 3 are first and last nodes of the path) are:
///   1 -> [A] -> [B] ->  2  ->  3
///   1 -> [B] ->  2 ->  [A] ->  3
///   1 -> [A] ->  2  -> [B] ->  3
///   1 ->  2  -> [A] -> [B] ->  3
/// Note that this operator does not explicitly insert the nodes of a pair one
/// after the other which forbids the following solutions:
///   1 -> [B] -> [A] ->  2  ->  3
///   1 ->  2  -> [B] -> [A] ->  3
/// which can only be obtained by inserting A after B.
class MakePairActiveOperator : public PathOperator {
 public:
  MakePairActiveOperator(const std::vector<IntVar*>& vars,
                         const std::vector<IntVar*>& secondary_vars,
                         std::function<int(int64_t)> start_empty_path_class,
                         const std::vector<PickupDeliveryPair>& pairs);
  ~MakePairActiveOperator() override {}
  bool MakeNeighbor() override;
  std::string DebugString() const override { return "MakePairActive"; }

 protected:
  bool MakeOneNeighbor() override;
  bool OnSamePathAsPreviousBase(int64_t /*base_index*/) override {
    /// Both base nodes have to be on the same path since they represent the
    /// nodes after which unactive node pairs will be moved.
    return true;
  }

  int64_t GetBaseNodeRestartPosition(int base_index) override;

  /// Required to ensure that after synchronization the operator is in a state
  /// compatible with GetBaseNodeRestartPosition.
  bool RestartAtPathStartOnSynchronize() override { return true; }

 private:
  void OnNodeInitialization() override;
  int FindNextInactivePair(int pair_index) const;
  bool ContainsActiveNodes(const std::vector<int64_t>& nodes) const;

  int inactive_pair_;
  int inactive_pair_first_index_;
  int inactive_pair_second_index_;
  const std::vector<PickupDeliveryPair> pairs_;
};

/// Operator which makes pairs of active nodes inactive.
class MakePairInactiveOperator : public PathOperator {
 public:
  MakePairInactiveOperator(const std::vector<IntVar*>& vars,
                           const std::vector<IntVar*>& secondary_vars,
                           std::function<int(int64_t)> start_empty_path_class,
                           const std::vector<PickupDeliveryPair>& pairs);

  bool MakeNeighbor() override;
  std::string DebugString() const override { return "MakePairInActive"; }
};

/// Operator which moves a pair of nodes to another position where the first
/// node of the pair must be before the second node on the same path.
/// Possible neighbors for the path 1 -> A -> B -> 2 -> 3 (where (1, 3) are
/// first and last nodes of the path and can therefore not be moved, and (A, B)
/// is a pair of nodes):
///   1 -> [A] ->  2  -> [B] -> 3
///   1 ->  2  -> [A] -> [B] -> 3
/// The pair can be moved to another path.
class PairRelocateOperator : public PathOperator {
 public:
  PairRelocateOperator(const std::vector<IntVar*>& vars,
                       const std::vector<IntVar*>& secondary_vars,
                       std::function<int(int64_t)> start_empty_path_class,
                       const std::vector<PickupDeliveryPair>& pairs);
  ~PairRelocateOperator() override {}

  bool MakeNeighbor() override;
  std::string DebugString() const override { return "PairRelocateOperator"; }

 protected:
  bool OnSamePathAsPreviousBase(int64_t base_index) override {
    /// Both destination nodes must be on the same path.
    return base_index == kPairSecondNodeDestination;
  }
  int64_t GetBaseNodeRestartPosition(int base_index) override;

  bool ConsiderAlternatives(int64_t base_index) const override {
    return base_index == kPairFirstNode;
  }

 private:
  bool RestartAtPathStartOnSynchronize() override { return true; }

  static constexpr int kPairFirstNode = 0;
  static constexpr int kPairFirstNodeDestination = 1;
  static constexpr int kPairSecondNodeDestination = 2;
};

/// Operator which moves a pair of nodes to another position where the first
/// node of the pair is directly before the second node.
class GroupPairAndRelocateOperator : public PathOperator {
 public:
  GroupPairAndRelocateOperator(
      const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& secondary_vars,
      std::function<int(int64_t)> start_empty_path_class,
      std::function<const std::vector<int>&(int, int)> get_neighbors,
      const std::vector<PickupDeliveryPair>& pairs);
  GroupPairAndRelocateOperator(
      const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& secondary_vars,
      std::function<int(int64_t)> start_empty_path_class,
      const std::vector<PickupDeliveryPair>& pairs)
      : GroupPairAndRelocateOperator(vars, secondary_vars,
                                     std::move(start_empty_path_class), nullptr,
                                     pairs) {}
  ~GroupPairAndRelocateOperator() override {}

  bool MakeNeighbor() override;
  std::string DebugString() const override { return "GroupPairAndRelocate"; }
};

/// Operator which moves a pair of nodes to another position where the first
/// node of the pair must be before the second node on the same path.
/// The default behavior of the operator is to insert the first node after the
/// first node of another pair, and the second node after the other pair's
/// second node. This results in a FIFO behavior. The behavior can be overridden
/// for each path to enforce a LIFO behavior (the second node gets inserted
/// before the other pair's second node). This is specified using the force_lifo
/// callback which takes the start node of a path as argument; if the callback
/// returns true then the LIFO behavior will be enforced, otherwise it's FIFO.
// TODO(user): Add a version which inserts the first node before the other
// pair's first node; there are many redundant neighbors if done blindly.
class LightPairRelocateOperator : public PathOperator {
 public:
  LightPairRelocateOperator(
      const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& secondary_vars,
      std::function<int(int64_t)> start_empty_path_class,
      std::function<const std::vector<int>&(int, int)> get_neighbors,
      const std::vector<PickupDeliveryPair>& pairs,
      std::function<bool(int64_t)> force_lifo = nullptr);
  LightPairRelocateOperator(const std::vector<IntVar*>& vars,
                            const std::vector<IntVar*>& secondary_vars,
                            std::function<int(int64_t)> start_empty_path_class,
                            const std::vector<PickupDeliveryPair>& pairs,
                            std::function<bool(int64_t)> force_lifo = nullptr);
  ~LightPairRelocateOperator() override {}

  bool MakeNeighbor() override;
  std::string DebugString() const override {
    return "LightPairRelocateOperator";
  }

 private:
  std::function<bool(int64_t)> force_lifo_;
};

/// Operator which exchanges the position of two pairs; for both pairs the first
/// node of the pair must be before the second node on the same path.
/// Possible neighbors for the paths 1 -> A -> B -> 2 -> 3 and 4 -> C -> D -> 5
/// (where (1, 3) and (4, 5) are first and last nodes of the paths and can
/// therefore not be moved, and (A, B) and (C,D) are pairs of nodes):
///   1 -> [C] ->  [D] -> 2 -> 3, 4 -> [A] -> [B] -> 5
class PairExchangeOperator : public PathOperator {
 public:
  PairExchangeOperator(
      const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& secondary_vars,
      std::function<int(int64_t)> start_empty_path_class,
      std::function<const std::vector<int>&(int, int)> get_neighbors,
      const std::vector<PickupDeliveryPair>& pairs);
  PairExchangeOperator(const std::vector<IntVar*>& vars,
                       const std::vector<IntVar*>& secondary_vars,
                       std::function<int(int64_t)> start_empty_path_class,
                       const std::vector<PickupDeliveryPair>& pairs)
      : PairExchangeOperator(vars, secondary_vars,
                             std::move(start_empty_path_class), nullptr,
                             pairs) {}
  ~PairExchangeOperator() override {}

  bool MakeNeighbor() override;
  std::string DebugString() const override { return "PairExchangeOperator"; }

 private:
  bool RestartAtPathStartOnSynchronize() override { return true; }
  bool ConsiderAlternatives(int64_t /*base_index*/) const override {
    return true;
  }
  bool GetPreviousAndSibling(int64_t node, int64_t* previous, int64_t* sibling,
                             int64_t* sibling_previous) const;
};

/// Operator which exchanges the paths of two pairs (path have to be different).
/// Pairs are inserted in all possible positions in their new path with the
/// constraint that the second node must be placed after the first.
/// Possible neighbors for the path 1 -> A -> B -> 2 -> 3, 4 -> C -> 5 -> D -> 6
/// 1 -> C -> D -> 2 -> 3 4 -> A -> B -> 5 -> 6
/// 1 -> C -> 2 -> D -> 3 4 -> A -> 5 -> B -> 6
/// 1 -> 2 -> C -> D -> 3 4 -> 5 -> A -> B -> 6
/// 1 -> C -> D -> 2 -> 3 4 -> A -> B -> 5 -> 6
/// 1 -> C -> 2 -> D -> 3 4 -> A -> 5 -> B -> 6
/// 1 -> 2 -> C -> D -> 3 4 -> 5 -> A -> B -> 6
/// 1 -> C -> D -> 2 -> 3 4 -> A -> B -> 5 -> 6
/// 1 -> C -> 2 -> D -> 3 4 -> A -> 5 -> B -> 6
/// 1 -> 2 -> C -> D -> 3 4 -> 5 -> A -> B -> 6
class PairExchangeRelocateOperator : public PathOperator {
 public:
  PairExchangeRelocateOperator(
      const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& secondary_vars,
      std::function<int(int64_t)> start_empty_path_class,
      const std::vector<PickupDeliveryPair>& pairs);
  ~PairExchangeRelocateOperator() override {}

  bool MakeNeighbor() override;
  std::string DebugString() const override {
    return "PairExchangeRelocateOperator";
  }

 protected:
  bool OnSamePathAsPreviousBase(int64_t base_index) override;
  int64_t GetBaseNodeRestartPosition(int base_index) override;

 private:
  bool RestartAtPathStartOnSynchronize() override { return true; }
  bool GetPreviousAndSibling(int64_t node, int64_t* previous, int64_t* sibling,
                             int64_t* sibling_previous) const;
  bool MoveNode(int pair, int node, int64_t nodes[2][2], int64_t dest[2][2],
                int64_t prev[2][2]);
  bool LoadAndCheckDest(int pair, int node, int64_t base_node,
                        int64_t nodes[2][2], int64_t dest[2][2]) const;

  static constexpr int kFirstPairFirstNode = 0;
  static constexpr int kSecondPairFirstNode = 1;
  static constexpr int kFirstPairFirstNodeDestination = 2;
  static constexpr int kFirstPairSecondNodeDestination = 3;
  static constexpr int kSecondPairFirstNodeDestination = 4;
  static constexpr int kSecondPairSecondNodeDestination = 5;
};

/// Operator which iterates through each alternative of a set of pairs. If a
/// pair has n and m alternatives, n.m alternatives will be explored.
/// Possible neighbors for the path 1 -> A -> a -> 2 (where (1, 2) are first and
/// last nodes of a path and A has B, C as alternatives and a has b as
/// alternative):
/// 1 -> A -> [b] -> 2
/// 1 -> [B] -> a -> 2
/// 1 -> [B] -> [b] -> 2
/// 1 -> [C] -> a -> 2
/// 1 -> [C] -> [b] -> 2
class SwapIndexPairOperator : public IntVarLocalSearchOperator {
 public:
  SwapIndexPairOperator(const std::vector<IntVar*>& vars,
                        const std::vector<IntVar*>& path_vars,
                        const std::vector<PickupDeliveryPair>& pairs);
  ~SwapIndexPairOperator() override {}

  bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) override;
  void OnStart() override;
  std::string DebugString() const override { return "SwapIndexPairOperator"; }

 private:
  /// Updates first_active_ and second_active_ to make them correspond to the
  /// active nodes of the node pair of index pair_index_.
  bool UpdateActiveNodes();
  /// Sets the to to be the node after from
  void SetNext(int64_t from, int64_t to, int64_t path) {
    DCHECK_LT(from, number_of_nexts_);
    SetValue(from, to);
    if (!ignore_path_vars_) {
      DCHECK_LT(from + number_of_nexts_, Size());
      SetValue(from + number_of_nexts_, path);
    }
  }

  const std::vector<PickupDeliveryPair> pairs_;
  int pair_index_;
  int first_index_;
  int second_index_;
  int64_t first_active_;
  int64_t second_active_;
  std::vector<int64_t> prevs_;
  const int number_of_nexts_;
  const bool ignore_path_vars_;
};

/// Operator which inserts inactive nodes into a path and makes a pair of
/// active nodes inactive.
class IndexPairSwapActiveOperator : public PathOperator {
 public:
  IndexPairSwapActiveOperator(
      const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& secondary_vars,
      std::function<int(int64_t)> start_empty_path_class,
      const std::vector<PickupDeliveryPair>& pairs);
  ~IndexPairSwapActiveOperator() override {}

  bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) override;
  bool MakeNeighbor() override;
  std::string DebugString() const override {
    return "IndexPairSwapActiveOperator";
  }

 private:
  void OnNodeInitialization() override;

  int inactive_node_;
};

/// RelocateExpensiveChain
///
/// Operator which relocates the most expensive subchains (given a cost
/// callback) in a path to a different position.
///
/// The most expensive chain on a path is the one resulting from cutting the 2
/// most expensive arcs on this path.
class RelocateExpensiveChain : public PathOperator {
 public:
  RelocateExpensiveChain(const std::vector<IntVar*>& vars,
                         const std::vector<IntVar*>& secondary_vars,
                         std::function<int(int64_t)> start_empty_path_class,
                         int num_arcs_to_consider,
                         std::function<int64_t(int64_t, int64_t, int64_t)>
                             arc_cost_for_path_start);
  ~RelocateExpensiveChain() override {}
  bool MakeNeighbor() override;
  bool MakeOneNeighbor() override;

  std::string DebugString() const override { return "RelocateExpensiveChain"; }

 private:
  void OnNodeInitialization() override;
  void IncrementCurrentPath();
  bool IncrementCurrentArcIndices();
  /// Tries to find most expensive chains on remaining paths, starting with the
  /// current one, until succeeding on one of them.
  /// Returns false iff all remaining paths are empty.
  bool FindMostExpensiveChainsOnRemainingPaths();

  int num_arcs_to_consider_;
  int current_path_;
  std::vector<std::pair<int64_t, int>> most_expensive_arc_starts_and_ranks_;
  /// Indices in most_expensive_arc_starts_and_ranks_ corresponding to the first
  /// and second arcs currently being considered for removal.
  std::pair</*first_arc_index*/ int, /*second_arc_index*/ int>
      current_expensive_arc_indices_;
  std::function<int64_t(/*before_node*/ int64_t, /*after_node*/ int64_t,
                        /*path_start*/ int64_t)>
      arc_cost_for_path_start_;
  int end_path_;
  /// The following boolean indicates if there are any non-empty paths left to
  /// explore by the operator.
  bool has_non_empty_paths_to_explore_;
};

/// Operator which inserts pairs of inactive nodes into a path and makes an
/// active node inactive.
/// There are two versions:
/// - one which makes inactive the node being replaced by the first node of the
///   pair (with swap_first true);
/// - one which makes inactive the node being replaced by the second node of the
///   pair (with swap_first false).
template <bool swap_first>
class PairNodeSwapActiveOperator : public PathOperator {
 public:
  PairNodeSwapActiveOperator(const std::vector<IntVar*>& vars,
                             const std::vector<IntVar*>& secondary_vars,
                             std::function<int(int64_t)> start_empty_path_class,
                             const std::vector<PickupDeliveryPair>& pairs);
  ~PairNodeSwapActiveOperator() override {}

  bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) override;
  bool MakeNeighbor() override;
  std::string DebugString() const override {
    return "PairNodeSwapActiveOperator";
  }

 protected:
  bool OnSamePathAsPreviousBase(int64_t /*base_index*/) override {
    /// Both base nodes have to be on the same path since they represent the
    /// nodes after which unactive node pairs will be moved.
    return true;
  }

  int64_t GetBaseNodeRestartPosition(int base_index) override;

  /// Required to ensure that after synchronization the operator is in a state
  /// compatible with GetBaseNodeRestartPosition.
  bool RestartAtPathStartOnSynchronize() override { return true; }

 private:
  void OnNodeInitialization() override;

  int inactive_pair_;
  std::vector<PickupDeliveryPair> pairs_;
};

// ==========================================================================
// Section: Implementations of the template classes declared above.

template <bool swap_first>
PairNodeSwapActiveOperator<swap_first>::PairNodeSwapActiveOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64_t)> start_empty_path_class,
    const std::vector<PickupDeliveryPair>& pairs)
    : PathOperator(vars, secondary_vars, 2, false, false,
                   std::move(start_empty_path_class), nullptr),
      inactive_pair_(0),
      pairs_(pairs) {}

template <bool swap_first>
int64_t PairNodeSwapActiveOperator<swap_first>::GetBaseNodeRestartPosition(
    int base_index) {
  // Base node 1 must be after base node 0 if they are both on the same path.
  if (base_index == 0 || StartNode(base_index) != StartNode(base_index - 1)) {
    return StartNode(base_index);
  } else {
    return BaseNode(base_index - 1);
  }
}

template <bool swap_first>
void PairNodeSwapActiveOperator<swap_first>::OnNodeInitialization() {
  for (int i = 0; i < pairs_.size(); ++i) {
    if (IsInactive(pairs_[i].pickup_alternatives[0]) &&
        IsInactive(pairs_[i].delivery_alternatives[0])) {
      inactive_pair_ = i;
      return;
    }
  }
  inactive_pair_ = pairs_.size();
}

template <bool swap_first>
bool PairNodeSwapActiveOperator<swap_first>::MakeNextNeighbor(
    Assignment* delta, Assignment* deltadelta) {
  while (inactive_pair_ < pairs_.size()) {
    if (!IsInactive(pairs_[inactive_pair_].pickup_alternatives[0]) ||
        !IsInactive(pairs_[inactive_pair_].delivery_alternatives[0]) ||
        !PathOperator::MakeNextNeighbor(delta, deltadelta)) {
      ResetPosition();
      ++inactive_pair_;
    } else {
      return true;
    }
  }
  return false;
}

template <bool swap_first>
bool PairNodeSwapActiveOperator<swap_first>::MakeNeighbor() {
  const int64_t base = BaseNode(0);
  if (IsPathEnd(base)) {
    return false;
  }
  const int64_t pair_first = pairs_[inactive_pair_].pickup_alternatives[0];
  const int64_t pair_second = pairs_[inactive_pair_].delivery_alternatives[0];
  if (swap_first) {
    return MakeActive(pair_second, BaseNode(1)) &&
           MakeActive(pair_first, base) &&
           MakeChainInactive(pair_first, Next(pair_first));
  } else {
    return MakeActive(pair_second, BaseNode(1)) &&
           MakeActive(pair_first, base) &&
           MakeChainInactive(pair_second, Next(pair_second));
  }
}

/// A utility class to maintain pickup and delivery information of nodes.
class PickupAndDeliveryData {
 public:
  PickupAndDeliveryData(int num_nodes,
                        const std::vector<PickupDeliveryPair>& pairs);
  bool IsPickupNode(int64_t node) const {
    DCHECK_LT(node, is_pickup_node_.size());
    return is_pickup_node_[node];
  }
  bool IsDeliveryNode(int64_t node) const {
    DCHECK_LT(node, is_delivery_node_.size());
    return is_delivery_node_[node];
  }
  int GetPairOfNode(int64_t node) const {
    DCHECK_LT(node, pair_of_node_.size());
    return pair_of_node_[node];
  }

 private:
  std::vector<bool> is_pickup_node_;
  std::vector<bool> is_delivery_node_;
  std::vector<int> pair_of_node_;
};

/// Tries to move subtrips after an insertion node.
/// A subtrip is a subsequence that contains only matched pickup and delivery
/// nodes, or pickup-only nodes, i.e. it cannot contain a pickup without a
/// corresponding delivery or vice-versa.
///
/// For a given subtrip given by path indices i_1 ... i_k, we call 'rejected'
/// the nodes with indices i_1 < j < i_k that are not in the subtrip. If the
/// base_node is a pickup, this operator selects the smallest subtrip starting
/// at base_node such that rejected nodes are only deliveries. If the base_node
/// is a delivery, it selects the smallest subtrip ending at base_node such that
/// rejected nodes are only pickups.
class RelocateSubtrip : public PathOperator {
 public:
  RelocateSubtrip(
      const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& secondary_vars,
      std::function<int(int64_t)> start_empty_path_class,
      std::function<const std::vector<int>&(int, int)> get_neighbors,
      const std::vector<PickupDeliveryPair>& pairs);
  RelocateSubtrip(const std::vector<IntVar*>& vars,
                  const std::vector<IntVar*>& secondary_vars,
                  std::function<int(int64_t)> start_empty_path_class,
                  const std::vector<PickupDeliveryPair>& pairs)
      : RelocateSubtrip(vars, secondary_vars, std::move(start_empty_path_class),
                        nullptr, pairs) {}

  std::string DebugString() const override { return "RelocateSubtrip"; }
  bool MakeNeighbor() override;

 private:
  // Relocates the subtrip starting at chain_first_node. It must be a pickup.
  bool RelocateSubTripFromPickup(int64_t chain_first_node,
                                 int64_t insertion_node);
  /// Relocates the subtrip ending at chain_first_node. It must be a delivery.
  bool RelocateSubTripFromDelivery(int64_t chain_last_node,
                                   int64_t insertion_node);
  void SetPath(const std::vector<int64_t>& path, int path_id);

  const PickupAndDeliveryData pd_data_;
  // Represents the set of pairs that have been opened during a call to
  // MakeNeighbor(). This vector must be all false before and after calling
  // RelocateSubTripFromPickup() and RelocateSubTripFromDelivery().
  std::vector<bool> opened_pairs_set_;

  std::vector<int64_t> rejected_nodes_;
  std::vector<int64_t> subtrip_nodes_;
};

class ExchangeSubtrip : public PathOperator {
 public:
  ExchangeSubtrip(
      const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& secondary_vars,
      std::function<int(int64_t)> start_empty_path_class,
      std::function<const std::vector<int>&(int, int)> get_neighbors,
      const std::vector<PickupDeliveryPair>& pairs);
  ExchangeSubtrip(const std::vector<IntVar*>& vars,
                  const std::vector<IntVar*>& secondary_vars,
                  std::function<int(int64_t)> start_empty_path_class,
                  const std::vector<PickupDeliveryPair>& pairs)
      : ExchangeSubtrip(vars, secondary_vars, std::move(start_empty_path_class),
                        nullptr, pairs) {}

  std::string DebugString() const override { return "ExchangeSubtrip"; }
  bool MakeNeighbor() override;

 private:
  // Try to extract a subtrip from base_node (see below) and check that the move
  // will be canonical.
  // Given a pickup/delivery pair, this operator could generate the same move
  // twice, the first time with base_node == pickup, the second time with
  // base_node == delivery. This happens only when no nodes in the subtrip
  // remain in the original path, i.e. when rejects is empty after
  // chain extraction. In that case, we keep only a canonical move out of the
  // two possibilities, the move where base_node is a pickup.
  bool ExtractChainsAndCheckCanonical(int64_t base_node,
                                      std::vector<int64_t>* rejects,
                                      std::vector<int64_t>* subtrip);
  // Reads the path from base_node forward, collecting subtrip nodes in
  // subtrip and non-subtrip nodes in rejects.
  // Non-subtrip nodes will be unmatched delivery nodes.
  // base_node must be a pickup, and remaining/extracted_nodes must be empty.
  // Returns true if such chains could be extracted.
  bool ExtractChainsFromPickup(int64_t base_node, std::vector<int64_t>* rejects,
                               std::vector<int64_t>* subtrip);
  // Reads the path from base_node backward, collecting subtrip nodes in
  // subtrip and non-subtrip nodes in rejects.
  // Non-subtrip nodes will be unmatched pickup nodes.
  // base_node must be a delivery, and remaining/extracted_nodes must be empty.
  // Returns true if such chains could be extracted.
  bool ExtractChainsFromDelivery(int64_t base_node,
                                 std::vector<int64_t>* rejects,
                                 std::vector<int64_t>* subtrip);
  void SetPath(const std::vector<int64_t>& path, int path_id);

  const PickupAndDeliveryData pd_data_;
  // Represents the set of opened pairs during ExtractChainsFromXXX().
  std::vector<bool> opened_pairs_set_;
  // Keep internal structures under hand to avoid reallocation.
  std::vector<int64_t> rejects0_;
  std::vector<int64_t> subtrip0_;
  std::vector<int64_t> rejects1_;
  std::vector<int64_t> subtrip1_;
  std::vector<int64_t> path0_;
  std::vector<int64_t> path1_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_NEIGHBORHOODS_H_
