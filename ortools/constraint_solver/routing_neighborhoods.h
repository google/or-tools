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

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_NEIGHBORHOODS_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_NEIGHBORHOODS_H_

#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/routing_types.h"

namespace operations_research {

// Relocate neighborhood which moves chains of neighbors.
// The operator starts by relocating a node n after a node m, then continues
// moving nodes which were after n as long as the "cost" added is less than
// the "cost" of the arc (m, n). If the new chain doesn't respect the domain of
// next variables, it will try reordering the nodes.
// Possible neighbors for path 1 -> A -> B -> C -> D -> E -> 2 (where (1, 2) are
// first and last nodes of the path and can therefore not be moved, A must
// be performed before B, and A, D and E are located at the same place):
// 1 -> A -> C -> [B] -> D -> E -> 2
// 1 -> A -> C -> D -> [B] -> E -> 2
// 1 -> A -> C -> D -> E -> [B] -> 2
// 1 -> A -> B -> D -> [C] -> E -> 2
// 1 -> A -> B -> D -> E -> [C] -> 2
// 1 -> A -> [D] -> [E] -> B -> C -> 2
// 1 -> A -> B -> [D] -> [E] ->  C -> 2
// 1 -> A -> [E] -> B -> C -> D -> 2
// 1 -> A -> B -> [E] -> C -> D -> 2
// 1 -> A -> B -> C -> [E] -> D -> 2
// This operator is extremely useful to move chains of nodes which are located
// at the same place (for instance nodes part of a same stop).
// TODO(user): Consider merging with standard Relocate in local_search.cc.
class MakeRelocateNeighborsOperator : public PathWithPreviousNodesOperator {
 public:
  MakeRelocateNeighborsOperator(
      const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& secondary_vars,
      std::function<int(int64)> start_empty_path_class,
      RoutingTransitEvaluator2 arc_evaluator);
  ~MakeRelocateNeighborsOperator() override {}

  bool MakeNeighbor() override;
  std::string DebugString() const override { return "RelocateNeighbors"; }

 private:
  // Moves a chain starting after 'before_chain' and ending at 'chain_end'
  // after node 'destination'. Tries to repair the resulting solution by
  // checking if the new arc created after 'destination' is compatible with
  // NextVar domains, and moves the 'destination' down the path if the solution
  // is inconsistent. Iterates the process on the new arcs created before
  // the node 'destination' (if destination was moved).
  bool MoveChainAndRepair(int64 before_chain, int64 chain_end,
                          int64 destination);

  // Moves node after 'before_to_move' down the path until a position is found
  // where NextVar domains are not violated, it it exists. Stops when reaching
  // position after 'up_to'.
  int64 Reposition(int64 before_to_move, int64 up_to);

  RoutingTransitEvaluator2 arc_evaluator_;
};

// Pair-based neighborhood operators, designed to move nodes by pairs (pairs
// are static and given). These neighborhoods are very useful for Pickup and
// Delivery problems where pickup and delivery nodes must remain on the same
// route.
// TODO(user): Add option to prune neighbords where the order of node pairs
//                is violated (ie precedence between pickup and delivery nodes).
// TODO(user): Move this to local_search.cc if it's generic enough.
// TODO(user): Detect pairs automatically by parsing the constraint model;
//                we could then get rid of the pair API in the RoutingModel
//                class.

// Operator which inserts pairs of inactive nodes into a path.
// Possible neighbors for the path 1 -> 2 -> 3 with pair (A, B) inactive
// (where 1 and 3 are first and last nodes of the path) are:
//   1 -> [A] -> [B] ->  2  ->  3
//   1 -> [B] ->  2 ->  [A] ->  3
//   1 -> [A] ->  2  -> [B] ->  3
//   1 ->  2  -> [A] -> [B] ->  3
// Note that this operator does not expicitely insert the nodes of a pair one
// after the other which forbids the following solutions:
//   1 -> [B] -> [A] ->  2  ->  3
//   1 ->  2  -> [B] -> [A] ->  3
// which can only be obtained by inserting A after B.
class MakePairActiveOperator : public PathOperator {
 public:
  MakePairActiveOperator(const std::vector<IntVar*>& vars,
                         const std::vector<IntVar*>& secondary_vars,
                         std::function<int(int64)> start_empty_path_class,
                         const RoutingNodePairs& pairs);
  ~MakePairActiveOperator() override {}
  bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) override;
  bool MakeNeighbor() override;
  std::string DebugString() const override { return "MakePairActive"; }

 protected:
  bool OnSamePathAsPreviousBase(int64 base_index) override {
    // Both base nodes have to be on the same path since they represent the
    // nodes after which unactive node pairs will be moved.
    return true;
  }

  int64 GetBaseNodeRestartPosition(int base_index) override;

  // Required to ensure that after synchronization the operator is in a state
  // compatible with GetBaseNodeRestartPosition.
  bool RestartAtPathStartOnSynchronize() override { return true; }

 private:
  void OnNodeInitialization() override;

  int inactive_pair_;
  RoutingNodePairs pairs_;
};

// Operator which makes pairs of active nodes inactive.
class MakePairInactiveOperator : public PathWithPreviousNodesOperator {
 public:
  MakePairInactiveOperator(const std::vector<IntVar*>& vars,
                           const std::vector<IntVar*>& secondary_vars,
                           std::function<int(int64)> start_empty_path_class,
                           const RoutingNodePairs& node_pairs);

  bool MakeNeighbor() override;
  std::string DebugString() const override { return "MakePairInActive"; }

 private:
  std::vector<int> pairs_;
};

// Operator which moves a pair of nodes to another position where the first
// node of the pair must be before the second node on the same path.
// Possible neighbors for the path 1 -> A -> B -> 2 -> 3 (where (1, 3) are
// first and last nodes of the path and can therefore not be moved, and (A, B)
// is a pair of nodes):
//   1 -> [A] ->  2  -> [B] -> 3
//   1 ->  2  -> [A] -> [B] -> 3
class PairRelocateOperator : public PathOperator {
 public:
  PairRelocateOperator(const std::vector<IntVar*>& vars,
                       const std::vector<IntVar*>& secondary_vars,
                       std::function<int(int64)> start_empty_path_class,
                       const RoutingNodePairs& node_pairs);
  ~PairRelocateOperator() override {}

  bool MakeNeighbor() override;
  std::string DebugString() const override { return "PairRelocateOperator"; }

 protected:
  bool OnSamePathAsPreviousBase(int64 base_index) override {
    // Both destination nodes must be on the same path.
    return base_index == kPairSecondNodeDestination;
  }
  int64 GetBaseNodeRestartPosition(int base_index) override;

 private:
  void OnNodeInitialization() override;
  bool RestartAtPathStartOnSynchronize() override { return true; }

  std::vector<int> pairs_;
  std::vector<int> prevs_;
  std::vector<bool> is_first_;
  static const int kPairFirstNode = 0;
  static const int kPairFirstNodeDestination = 1;
  static const int kPairSecondNodeDestination = 2;
};

// Operator which inserts inactive nodes into a path and makes a pair of
// active nodes inactive.
class NodePairSwapActiveOperator : public PathWithPreviousNodesOperator {
 public:
  NodePairSwapActiveOperator(const std::vector<IntVar*>& vars,
                             const std::vector<IntVar*>& secondary_vars,
                             std::function<int(int64)> start_empty_path_class,
                             const RoutingNodePairs& node_pairs);
  ~NodePairSwapActiveOperator() override {}

  bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) override;
  bool MakeNeighbor() override;
  std::string DebugString() const override { return "NodePairSwapActiveOperator"; }

 private:
  void OnNodeInitialization() override;

  int inactive_node_;
  std::vector<int> pairs_;
};

// Operator which inserts pairs of inactive nodes into a path and makes an
// active node inactive.
// There are two versions:
// - one which makes inactive the node being replaced by the first node of the
//   pair (with swap_first true);
// - one which makes inactive the node being replaced by the second node of the
//   pair (with swap_first false).
template <bool swap_first>
class PairNodeSwapActiveOperator : public PathOperator {
 public:
  PairNodeSwapActiveOperator(const std::vector<IntVar*>& vars,
                             const std::vector<IntVar*>& secondary_vars,
                             std::function<int(int64)> start_empty_path_class,
                             const RoutingNodePairs& node_pairs);
  ~PairNodeSwapActiveOperator() override {}

  bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) override;
  bool MakeNeighbor() override;
  std::string DebugString() const override { return "PairNodeSwapActiveOperator"; }

 protected:
  bool OnSamePathAsPreviousBase(int64 base_index) override {
    // Both base nodes have to be on the same path since they represent the
    // nodes after which unactive node pairs will be moved.
    return true;
  }

  int64 GetBaseNodeRestartPosition(int base_index) override;

  // Required to ensure that after synchronization the operator is in a state
  // compatible with GetBaseNodeRestartPosition.
  bool RestartAtPathStartOnSynchronize() override { return true; }

 private:
  void OnNodeInitialization() override;

  int inactive_pair_;
  RoutingNodePairs pairs_;
};

// ==========================================================================
// Section: Implementations of the template classes declared above.

template <bool swap_first>
PairNodeSwapActiveOperator<swap_first>::PairNodeSwapActiveOperator(
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingNodePairs& node_pairs)
    : PathOperator(vars, secondary_vars, 2, std::move(start_empty_path_class)),
      inactive_pair_(0),
      pairs_(node_pairs) {}

template <bool swap_first>
int64 PairNodeSwapActiveOperator<swap_first>::GetBaseNodeRestartPosition(
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
    if (IsInactive(pairs_[i].first[0]) && IsInactive(pairs_[i].second[0])) {
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

template <bool swap_first>
bool PairNodeSwapActiveOperator<swap_first>::MakeNeighbor() {
  const int64 base = BaseNode(0);
  if (IsPathEnd(base)) {
    return false;
  }
  const int64 pair_first = pairs_[inactive_pair_].first[0];
  const int64 pair_second = pairs_[inactive_pair_].second[0];
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

}  // namespace operations_research

#endif  // OR_TOOLS_CONSTRAINT_SOLVER_ROUTING_NEIGHBORHOODS_H_
