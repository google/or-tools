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


#include "constraint_solver/routing.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <map>
#include <memory>
#include <unordered_set>

#include "base/callback.h"
#include "base/casts.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "google/protobuf/text_format.h"
#include "base/map_util.h"
#include "base/stl_util.h"
#include "base/thorough_hash.h"
#include "base/hash.h"
#include "constraint_solver/model.pb.h"
#include "graph/connectivity.h"
#include "graph/linear_assignment.h"
#include "util/saturated_arithmetic.h"

namespace operations_research {
class LocalSearchPhaseParameters;
}  // namespace operations_research

// TODO(user): Remove these flags after cleaning up the Savings heuristic.
DEFINE_double(savings_route_shape_parameter, 1.0,
              "Coefficient of the added arc in the savings definition."
              "Variation of this parameter may provide heuristic solutions "
              "which are closer to the optimal solution than otherwise obtained"
              " via the traditional algorithm where it is equal to 1.");
DEFINE_int64(savings_filter_neighbors, 0,
             "Use filter which filters the pair of orders considered in "
             "Savings first solution heuristic by limiting the number "
             "of neighbors considered for each node.");
DEFINE_int64(savings_filter_radius, 0,
             "Use filter which filters the pair of orders considered in "
             "Savings first solution heuristic by limiting the distance "
             "up to which a neighbor is considered for each node.");
DEFINE_int64(sweep_sectors, 1,
             "The number of sectors the space is divided before it is sweeped "
             "by the ray.");

// Trace settings

// TODO(user): Move most of the following settings to a model parameter
// proto.

#if defined(_MSC_VER)
namespace stdext {
template <>
size_t hash_value<operations_research::RoutingModel::NodeIndex>(
    const operations_research::RoutingModel::NodeIndex& a) {
  return a.value();
}
}  //  namespace stdext
#endif  // _MSC_VER

namespace operations_research {

namespace {

// Set of "light" constraints, well-suited for use within Local Search.
// These constraints are "checking" constraints, only triggered on WhenBound
// events. The provide very little (or no) domain filtering.
// TODO(user): Move to core constraintsolver library.

// Light one-dimension function-based element constraint ensuring:
// var == values(index).
// Doesn't perform bound reduction of the resulting variable until the index
// variable is bound.
// Ownership of the 'values' callback is taken by the constraint.
template <typename F>
class LightFunctionElementConstraint : public Constraint {
 public:
  LightFunctionElementConstraint(Solver* const solver, IntVar* const var,
                                 IntVar* const index, F values)
      : Constraint(solver),
        var_(var),
        index_(index),
        values_(std::move(values)) {}
  ~LightFunctionElementConstraint() override {}

  void Post() override {
    Demon* demon = MakeConstraintDemon0(
        solver(), this, &LightFunctionElementConstraint::IndexBound,
        "IndexBound");
    index_->WhenBound(demon);
  }

  void InitialPropagate() override {
    if (index_->Bound()) {
      IndexBound();
    }
  }

  std::string DebugString() const override {
    return "LightFunctionElementConstraint";
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(RoutingModelVisitor::kLightElement, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            var_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument,
                                            index_);
    // Warning: This will expand all values into a vector.
    visitor->VisitInt64ToInt64Extension(values_, index_->Min(), index_->Max());
    visitor->EndVisitConstraint(RoutingModelVisitor::kLightElement, this);
  }

 private:
  void IndexBound() { var_->SetValue(values_(index_->Min())); }

  IntVar* const var_;
  IntVar* const index_;
  F values_;
};

template <typename F>
Constraint* MakeLightElement(Solver* const solver, IntVar* const var,
                             IntVar* const index, F values) {
  return solver->RevAlloc(new LightFunctionElementConstraint<F>(
      solver, var, index, std::move(values)));
}

namespace {
Constraint* BuildLightElement(CpModelLoader* const builder,
                              const CpConstraint& proto) {
  IntExpr* index = nullptr;
  if (!builder->ScanArguments(ModelVisitor::kIndexArgument, proto, &index)) {
    return nullptr;
  }
  IntExpr* target = nullptr;
  if (!builder->ScanArguments(ModelVisitor::kTargetArgument, proto, &target)) {
    return nullptr;
  }
  if (proto.extensions_size() != 1) {
    return nullptr;
  }
  const int extension_tag_index =
      builder->TagIndex(ModelVisitor::kInt64ToInt64Extension);
  Solver::IndexEvaluator1 callback = MakeFunctionFromProto<int64>(
      builder, proto.extensions(0), extension_tag_index);
  return MakeLightElement(builder->solver(), target->Var(), index->Var(),
                          std::move(callback));
}
}  // namespace

// Light two-dimension function-based element constraint ensuring:
// var == values(index1, index2).
// Doesn't perform bound reduction of the resulting variable until the index
// variables are bound.
// Ownership of the 'values' callback is taken by the constraint.
template <typename F>
class LightFunctionElement2Constraint : public Constraint {
 public:
  LightFunctionElement2Constraint(Solver* const solver, IntVar* const var,
                                  IntVar* const index1, IntVar* const index2,
                                  F values)
      : Constraint(solver),
        var_(var),
        index1_(index1),
        index2_(index2),
        values_(std::move(values)) {}
  ~LightFunctionElement2Constraint() override {}
  void Post() override {
    Demon* demon = MakeConstraintDemon0(
        solver(), this, &LightFunctionElement2Constraint::IndexBound,
        "IndexBound");
    index1_->WhenBound(demon);
    index2_->WhenBound(demon);
  }
  void InitialPropagate() override { IndexBound(); }

  std::string DebugString() const override {
    return "LightFunctionElement2Constraint";
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(RoutingModelVisitor::kLightElement2, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            var_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndexArgument,
                                            index1_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kIndex2Argument,
                                            index2_);
    // Warning: This will expand all values into a vector.
    const int64 index1_min = index1_->Min();
    const int64 index1_max = index1_->Max();
    visitor->VisitIntegerArgument(ModelVisitor::kMinArgument, index1_min);
    visitor->VisitIntegerArgument(ModelVisitor::kMaxArgument, index1_max);
    for (int i = index1_min; i <= index1_max; ++i) {
      visitor->VisitInt64ToInt64Extension(
          [this, i](int64 j) { return values_(i, j); }, index2_->Min(),
          index2_->Max());
    }
    visitor->EndVisitConstraint(RoutingModelVisitor::kLightElement2, this);
  }

 private:
  void IndexBound() {
    if (index1_->Bound() && index2_->Bound()) {
      var_->SetValue(values_(index1_->Min(), index2_->Min()));
    }
  }

  IntVar* const var_;
  IntVar* const index1_;
  IntVar* const index2_;
  Solver::IndexEvaluator2 values_;
};

template <typename F>
Constraint* MakeLightElement2(Solver* const solver, IntVar* const var,
                              IntVar* const index1, IntVar* const index2,
                              F values) {
  return solver->RevAlloc(new LightFunctionElement2Constraint<F>(
      solver, var, index1, index2, std::move(values)));
}

namespace {
Constraint* BuildLightElement2(CpModelLoader* const builder,
                               const CpConstraint& proto) {
  Solver* const solver = builder->solver();
  IntExpr* index1 = nullptr;
  if (!builder->ScanArguments(ModelVisitor::kIndexArgument, proto, &index1)) {
    return nullptr;
  }
  IntExpr* index2 = nullptr;
  if (!builder->ScanArguments(ModelVisitor::kIndex2Argument, proto, &index2)) {
    return nullptr;
  }
  int64 index1_min = 0;
  if (!builder->ScanArguments(ModelVisitor::kMinArgument, proto, &index1_min)) {
    return nullptr;
  }
  int64 index1_max = 0;
  if (!builder->ScanArguments(ModelVisitor::kMaxArgument, proto, &index1_max)) {
    return nullptr;
  }
  const int extension_tag_index =
      builder->TagIndex(ModelVisitor::kInt64ToInt64Extension);
  ArrayWithOffset<Solver::IndexEvaluator1>* const array = solver->RevAlloc(
      new ArrayWithOffset<Solver::IndexEvaluator1>(index1_min, index1_max));
  for (int i = index1_min; i <= index1_max; ++i) {
    array->SetValue(
        i, MakeFunctionFromProto<int64>(
               builder, proto.extensions(i - index1_min), extension_tag_index));
  }
  IntExpr* target = nullptr;
  if (!builder->ScanArguments(ModelVisitor::kTargetArgument, proto, &target)) {
    return nullptr;
  }
  return MakeLightElement2(
      builder->solver(), target->Var(), index1->Var(), index2->Var(),
      [array](int64 i, int64 j) { return array->Evaluate(i)(j); });
}
}  // namespace

// PathOperator subclass storing current previous nodes.
// TODO(user): Move this to constraint_solveri.h and/or merge with
// PathOperator.
class PathWithPreviousNodesOperator : public PathOperator {
 public:
  PathWithPreviousNodesOperator(
      const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& secondary_vars, int number_of_base_nodes,
      std::function<int(int64)> start_empty_path_class)
      : PathOperator(vars, secondary_vars, number_of_base_nodes,
                     std::move(start_empty_path_class)) {
    int64 max_next = -1;
    for (const IntVar* const var : vars) {
      max_next = std::max(max_next, var->Max());
    }
    prevs_.resize(max_next + 1, -1);
  }
  ~PathWithPreviousNodesOperator() override {}
  int64 Prev(int64 node_index) const {
    DCHECK(!IsPathStart(node_index));
    return prevs_[node_index];
  }
  bool IsPathStart(int64 node_index) const { return prevs_[node_index] == -1; }
  std::string DebugString() const override {
    return "PathWithPreviousNodesOperator";
  }

 protected:
  void OnNodeInitialization() override {
    for (int node_index = 0; node_index < number_of_nexts(); ++node_index) {
      prevs_[Next(node_index)] = node_index;
    }
  }

 private:
  std::vector<int64> prevs_;
};

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
// This operator is extremelly useful to move chains of nodes which are located
// at the same place (for instance nodes part of a same stop).
// TODO(user): move this to local_search.cc and see if it can be merged with
// standard Relocate.

class MakeRelocateNeighborsOperator : public PathWithPreviousNodesOperator {
 public:
  MakeRelocateNeighborsOperator(
      const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& secondary_vars,
      std::function<int(int64)> start_empty_path_class,
      RoutingModel::TransitEvaluator2 arc_evaluator)
      : PathWithPreviousNodesOperator(vars, secondary_vars, 2,
                                      std::move(start_empty_path_class)),
        arc_evaluator_(std::move(arc_evaluator)) {}
  ~MakeRelocateNeighborsOperator() override {}
  bool MakeNeighbor() override {
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
    while (!IsPathEnd(next) &&
           arc_evaluator_(chain_end, next) <= max_arc_value) {
      chain_end = next;
      next = Next(chain_end);
    }
    return MoveChainAndRepair(before_chain, chain_end, destination);
  }
  std::string DebugString() const override { return "RelocateNeighbors"; }

 private:
  // Moves a chain starting after 'before_chain' and ending at 'chain_end'
  // after node 'destination'. Tries to repair the resulting solution by
  // checking if the new arc created after 'destination' is compatible with
  // NextVar domains, and moves the 'destination' down the path if the solution
  // is inconsistent. Iterates the process on the new arcs created before
  // the node 'destination' (if destination was moved).
  bool MoveChainAndRepair(int64 before_chain, int64 chain_end,
                          int64 destination) {
    if (MoveChain(before_chain, chain_end, destination)) {
      if (!IsPathStart(destination)) {
        int64 current = Prev(destination);
        int64 last = chain_end;
        if (current == last) {  // chain was just before destination
          current = before_chain;
        }
        while (last >= 0 && !IsPathStart(current)) {
          last = Reposition(current, last);
          current = Prev(current);
        }
      }
      return true;
    }
    return false;
  }

  // Moves node after 'before_to_move' down the path until a position is found
  // where NextVar domains are not violated, it it exists. Stops when reaching
  // position after 'up_to'.
  int64 Reposition(int64 before_to_move, int64 up_to) {
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

  RoutingModel::TransitEvaluator2 arc_evaluator_;
};

LocalSearchOperator* MakeRelocateNeighbors(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    RoutingModel::TransitEvaluator2 arc_evaluator) {
  return solver->RevAlloc(new MakeRelocateNeighborsOperator(
      vars, secondary_vars, std::move(start_empty_path_class),
      std::move(arc_evaluator)));
}

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
                         const RoutingModel::NodePairs& pairs)
      : PathOperator(vars, secondary_vars, 2,
                     std::move(start_empty_path_class)),
        inactive_pair_(0),
        pairs_(pairs) {}
  ~MakePairActiveOperator() override {}
  std::string DebugString() const override { return "MakePairActive"; }
  bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) override;
  bool MakeNeighbor() override;

 protected:
  bool OnSamePathAsPreviousBase(int64 base_index) override {
    // Both base nodes have to be on the same path since they represent the
    // nodes after which unactive node pairs will be moved.
    return true;
  }
  int64 GetBaseNodeRestartPosition(int base_index) override {
    // Base node 1 must be after base node 0 if they are both on the same path.
    if (base_index == 0 || StartNode(base_index) != StartNode(base_index - 1)) {
      return StartNode(base_index);
    } else {
      return BaseNode(base_index - 1);
    }
  }
  // Required to ensure that after synchronization the operator is in a state
  // compatible with GetBaseNodeRestartPosition.
  bool RestartAtPathStartOnSynchronize() override { return true; }

 private:
  void OnNodeInitialization() override;

  int inactive_pair_;
  RoutingModel::NodePairs pairs_;
};

void MakePairActiveOperator::OnNodeInitialization() {
  for (int i = 0; i < pairs_.size(); ++i) {
    if (IsInactive(pairs_[i].first[0]) && IsInactive(pairs_[i].second[0])) {
      inactive_pair_ = i;
      return;
    }
  }
  inactive_pair_ = pairs_.size();
}

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

LocalSearchOperator* MakePairActive(
    Solver* const solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingModel::NodePairs& pairs) {
  return solver->RevAlloc(new MakePairActiveOperator(
      vars, secondary_vars, std::move(start_empty_path_class), pairs));
}

// Operator which makes pairs of active nodes inactive.
class MakePairInactiveOperator : public PathWithPreviousNodesOperator {
 public:
  MakePairInactiveOperator(const std::vector<IntVar*>& vars,
                           const std::vector<IntVar*>& secondary_vars,
                           std::function<int(int64)> start_empty_path_class,
                           const RoutingModel::NodePairs& node_pairs)
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
  std::string DebugString() const override { return "MakePairInActive"; }
  bool MakeNeighbor() override {
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

 private:
  std::vector<int> pairs_;
};

LocalSearchOperator* MakePairInactive(
    Solver* const solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingModel::NodePairs& pairs) {
  return solver->RevAlloc(new MakePairInactiveOperator(
      vars, secondary_vars, std::move(start_empty_path_class), pairs));
}

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
                       const RoutingModel::NodePairs& node_pairs)
      : PathOperator(vars, secondary_vars, 3,
                     std::move(start_empty_path_class)) {
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
  ~PairRelocateOperator() override {}
  bool MakeNeighbor() override;
  std::string DebugString() const override { return "PairRelocateOperator"; }

 protected:
  bool OnSamePathAsPreviousBase(int64 base_index) override {
    // Both destination nodes must be on the same path.
    return base_index == kPairSecondNodeDestination;
  }
  int64 GetBaseNodeRestartPosition(int base_index) override {
    // Destination node of the second node of a pair must be after the
    // destination node of the first node of a pair.
    if (base_index == kPairSecondNodeDestination) {
      return BaseNode(kPairFirstNodeDestination);
    } else {
      return StartNode(base_index);
    }
  }

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

void PairRelocateOperator::OnNodeInitialization() {
  for (int i = 0; i < number_of_nexts(); ++i) {
    prevs_[Next(i)] = i;
  }
}

LocalSearchOperator* MakePairRelocate(
    Solver* const solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingModel::NodePairs& pairs) {
  return solver->RevAlloc(new PairRelocateOperator(
      vars, secondary_vars, std::move(start_empty_path_class), pairs));
}

// Operator which inserts inactive nodes into a path and makes a pair of
// active nodes inactive.
class NodePairSwapActiveOperator : public PathWithPreviousNodesOperator {
 public:
  NodePairSwapActiveOperator(const std::vector<IntVar*>& vars,
                             const std::vector<IntVar*>& secondary_vars,
                             std::function<int(int64)> start_empty_path_class,
                             const RoutingModel::NodePairs& node_pairs)
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
  ~NodePairSwapActiveOperator() override {}
  std::string DebugString() const override { return "NodePairSwapActiveOperator"; }
  bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) override;
  bool MakeNeighbor() override;

 private:
  void OnNodeInitialization() override;

  int inactive_node_;
  std::vector<int> pairs_;
};

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

LocalSearchOperator* NodePairSwapActive(
    Solver* const solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingModel::NodePairs& pairs) {
  return solver->RevAlloc(new NodePairSwapActiveOperator(
      vars, secondary_vars, std::move(start_empty_path_class), pairs));
}

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
                             const RoutingModel::NodePairs& node_pairs)
      : PathOperator(vars, secondary_vars, 2,
                     std::move(start_empty_path_class)),
        inactive_pair_(0),
        pairs_(node_pairs) {}
  ~PairNodeSwapActiveOperator() override {}
  std::string DebugString() const override { return "PairNodeSwapActiveOperator"; }
  bool MakeNextNeighbor(Assignment* delta, Assignment* deltadelta) override;
  bool MakeNeighbor() override;

 protected:
  bool OnSamePathAsPreviousBase(int64 base_index) override {
    // Both base nodes have to be on the same path since they represent the
    // nodes after which unactive node pairs will be moved.
    return true;
  }
  int64 GetBaseNodeRestartPosition(int base_index) override {
    // Base node 1 must be after base node 0 if they are both on the same path.
    if (base_index == 0 || StartNode(base_index) != StartNode(base_index - 1)) {
      return StartNode(base_index);
    } else {
      return BaseNode(base_index - 1);
    }
  }
  // Required to ensure that after synchronization the operator is in a state
  // compatible with GetBaseNodeRestartPosition.
  bool RestartAtPathStartOnSynchronize() override { return true; }

 private:
  void OnNodeInitialization() override;

  int inactive_pair_;
  RoutingModel::NodePairs pairs_;
};

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

LocalSearchOperator* PairNodeSwapActive(
    Solver* const solver, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    std::function<int(int64)> start_empty_path_class,
    const RoutingModel::NodePairs& pairs) {
  return solver->ConcatenateOperators(
      {solver->RevAlloc(new PairNodeSwapActiveOperator<true>(
           vars, secondary_vars, start_empty_path_class, pairs)),
       solver->RevAlloc(new PairNodeSwapActiveOperator<false>(
           vars, secondary_vars, std::move(start_empty_path_class), pairs))});
}

// Cached callbacks

class RoutingCache : public RoutingModel::NodeEvaluator2 {
 public:
  // Creates a new cached callback based on 'callback'. The cache object does
  // not take ownership of the callback; the user must ensure that the callback
  // gets deleted when it or the cache is no longer used.
  //
  // When used in the RoutingModel class, the constructor should not be called
  // directly, but through RoutingModel::NewCachedCallback that ensures that the
  // base callback is deleted properly.
  RoutingCache(RoutingModel::NodeEvaluator2* callback, int size)
      : cached_(size), cache_(size), callback_(CHECK_NOTNULL(callback)) {
    for (RoutingModel::NodeIndex i(0); i < RoutingModel::NodeIndex(size); ++i) {
      cached_[i].resize(size, false);
      cache_[i].resize(size, 0);
    }
    CHECK(callback->IsRepeatable());
  }
  bool IsRepeatable() const override { return true; }
  int64 Run(RoutingModel::NodeIndex i, RoutingModel::NodeIndex j) override {
    // This method does lazy caching of results of callbacks: first
    // checks if it has been run with these parameters before, and
    // returns previous result if so, or runs underlaying callback and
    // stores its result.
    // Not MT-safe.
    if (cached_[i][j]) {
      return cache_[i][j];
    } else {
      const int64 cached_value = callback_->Run(i, j);
      cached_[i][j] = true;
      cache_[i][j] = cached_value;
      return cached_value;
    }
  }

 private:
  ITIVector<RoutingModel::NodeIndex, ITIVector<RoutingModel::NodeIndex, bool>>
      cached_;
  ITIVector<RoutingModel::NodeIndex, ITIVector<RoutingModel::NodeIndex, int64>>
      cache_;
  RoutingModel::NodeEvaluator2* const callback_;
};

class StateDependentRoutingCache : public RoutingModel::VariableNodeEvaluator2 {
 public:
  // Creates a new cached callback based on 'callback'. The cache object does
  // not take ownership of the callback; the user must ensure that the callback
  // gets deleted when it or the cache is no longer used.
  //
  // When used in the RoutingModel class, the constructor should not be called
  // directly, but through RoutingModel::NewCachedStateDependentCallback that
  // ensures that the base callback is deleted properly.
  StateDependentRoutingCache(RoutingModel::VariableNodeEvaluator2* callback,
                             int size)
      : cache_(size), callback_(CHECK_NOTNULL(callback)) {
    for (RoutingModel::NodeIndex i(0); i < RoutingModel::NodeIndex(size); ++i) {
      cache_[i].resize(size, {nullptr, nullptr});
    }
    CHECK(callback->IsRepeatable());
  }
  ~StateDependentRoutingCache() override {
    std::unordered_set<RangeIntToIntFunction*> value_functions_delete;
    std::unordered_set<RangeMinMaxIndexFunction*> index_functions_delete;
    for (const auto& cache_line : cache_) {
      for (const RoutingModel::StateDependentTransit& transit : cache_line) {
        value_functions_delete.insert(transit.transit);
        index_functions_delete.insert(transit.transit_plus_identity);
      }
    }
    STLDeleteElements(&value_functions_delete);
    STLDeleteElements(&index_functions_delete);
  }
  bool IsRepeatable() const override { return true; }
  // This method returns cached results of the callback.
  RoutingModel::StateDependentTransit Run(RoutingModel::NodeIndex i,
                                          RoutingModel::NodeIndex j) override {
    RoutingModel::StateDependentTransit& cache_cell = cache_[i][j];
    if (cache_cell.transit == nullptr) {
      cache_cell = callback_->Run(i, j);
    }
    return cache_cell;
  }

 private:
  ITIVector<
      RoutingModel::NodeIndex,
      ITIVector<RoutingModel::NodeIndex, RoutingModel::StateDependentTransit>>
      cache_;
  RoutingModel::VariableNodeEvaluator2* const callback_;
};

// Evaluators

class MatrixEvaluator : public BaseObject {
 public:
  explicit MatrixEvaluator(std::vector<std::vector<int64>> values)
      : values_(std::move(values)) {}
  ~MatrixEvaluator() override {}
  int64 Value(RoutingModel::NodeIndex i, RoutingModel::NodeIndex j) const {
    return values_[i.value()][j.value()];
  }

 private:
  const std::vector<std::vector<int64>> values_;
};

class VectorEvaluator : public BaseObject {
 public:
  explicit VectorEvaluator(std::vector<int64> values)
      : values_(std::move(values)) {}
  ~VectorEvaluator() override {}
  int64 Value(RoutingModel::NodeIndex i, RoutingModel::NodeIndex j) const {
    return values_[i.value()];
  }

 private:
  const std::vector<int64> values_;
};

template <typename T>
class ConstantEvaluator : public BaseObject {
 public:
  explicit ConstantEvaluator(T value) : value_(value) {}
  ~ConstantEvaluator() override {}
  T Value(RoutingModel::NodeIndex i, RoutingModel::NodeIndex j) const {
    return value_;
  }

  static ResultCallback2<T, RoutingModel::NodeIndex, RoutingModel::NodeIndex>*
  MakeNodeEvaluatorCallback(T value, Solver* solver) {
    const ConstantEvaluator<T>* const constant_evaluator =
        solver->RevAlloc(new ConstantEvaluator<T>(value));
    return constant_evaluator->MakeNodeEvaluatorCallback();
  }

  ResultCallback2<T, RoutingModel::NodeIndex, RoutingModel::NodeIndex>*
  MakeNodeEvaluatorCallback() const {
    return NewPermanentCallback(this, &ConstantEvaluator<T>::Value);
  }

 private:
  const T value_;
};
}  // namespace

// ----- Routing model -----

static const int kUnassigned = -1;
static const int64 kNoPenalty = -1;

const RoutingModel::NodeIndex RoutingModel::kFirstNode(0);
const RoutingModel::NodeIndex RoutingModel::kInvalidNodeIndex(-1);

const RoutingModel::DisjunctionIndex RoutingModel::kNoDisjunction(-1);

const RoutingModel::DimensionIndex RoutingModel::kNoDimension(-1);

RoutingModel::RoutingModel(int nodes, int vehicles, NodeIndex depot)
    : RoutingModel(nodes, vehicles, depot, DefaultModelParameters()) {}

RoutingModel::RoutingModel(int nodes, int vehicles, NodeIndex depot,
                           const RoutingModelParameters& parameters)
    : RoutingModel(nodes, vehicles, std::vector<NodeIndex>(vehicles, depot),
                   std::vector<NodeIndex>(vehicles, depot), parameters) {}

RoutingModel::RoutingModel(
    int nodes, int vehicles,
    const std::vector<std::pair<NodeIndex, NodeIndex>>& start_ends)
    : RoutingModel(nodes, vehicles, start_ends, DefaultModelParameters()) {}

RoutingModel::RoutingModel(
    int nodes, int vehicles,
    const std::vector<std::pair<NodeIndex, NodeIndex>>& start_ends,
    const RoutingModelParameters& parameters)
    : nodes_(nodes),
      vehicles_(vehicles),
      no_cycle_constraint_(nullptr),
      cost_(nullptr),
      transit_cost_of_vehicle_(vehicles, nullptr),
      fixed_cost_of_vehicle_(vehicles),
      cost_class_index_of_vehicle_(vehicles_, CostClassIndex(-1)),
      cost_classes_(),
      costs_are_homogeneous_across_vehicles_(
          parameters.reduce_vehicle_cost_model()),
      cache_callbacks_(nodes <= parameters.max_callback_cache_size()),
      vehicle_class_index_of_vehicle_(vehicles_, VehicleClassIndex(-1)),
      starts_(vehicles),
      ends_(vehicles),
      closed_(false),
      status_(ROUTING_NOT_SOLVED),
      collect_assignments_(nullptr),
      solve_db_(nullptr),
      improve_db_(nullptr),
      restore_assignment_(nullptr),
      assignment_(nullptr),
      preassignment_(nullptr),
      limit_(nullptr),
      ls_limit_(nullptr),
      lns_limit_(nullptr) {
  VLOG(1) << "Model parameters:\n" << parameters.DebugString();
  ConstraintSolverParameters solver_parameters =
      parameters.has_solver_parameters() ? parameters.solver_parameters()
                                         : Solver::DefaultSolverParameters();
  solver_.reset(new Solver("Routing", solver_parameters));
  InitializeBuilders(solver_.get());
  CHECK_EQ(vehicles, start_ends.size());
#if defined(_MSC_VER)
  std::unordered_set<NodeIndex, TypedIntHasher<NodeIndex>> depot_set;
#else
  std::unordered_set<NodeIndex, std::hash<NodeIndex>> depot_set;
#endif
  for (const std::pair<NodeIndex, NodeIndex> start_end : start_ends) {
    depot_set.insert(start_end.first);
    depot_set.insert(start_end.second);
  }
  start_end_count_ = depot_set.size();
  Initialize();
  SetStartEnd(start_ends);
}

RoutingModel::RoutingModel(int nodes, int vehicles,
                           const std::vector<NodeIndex>& starts,
                           const std::vector<NodeIndex>& ends)
    : RoutingModel(nodes, vehicles, starts, ends, DefaultModelParameters()) {}

RoutingModel::RoutingModel(int nodes, int vehicles,
                           const std::vector<NodeIndex>& starts,
                           const std::vector<NodeIndex>& ends,
                           const RoutingModelParameters& parameters)
    : nodes_(nodes),
      vehicles_(vehicles),
      no_cycle_constraint_(nullptr),
      cost_(nullptr),
      transit_cost_of_vehicle_(vehicles, nullptr),
      fixed_cost_of_vehicle_(vehicles),
      cost_class_index_of_vehicle_(vehicles_, CostClassIndex(-1)),
      cost_classes_(),
      costs_are_homogeneous_across_vehicles_(
          parameters.reduce_vehicle_cost_model()),
      cache_callbacks_(nodes <= parameters.max_callback_cache_size()),
      vehicle_class_index_of_vehicle_(vehicles_, VehicleClassIndex(-1)),
      starts_(vehicles),
      ends_(vehicles),
      closed_(false),
      status_(ROUTING_NOT_SOLVED),
      collect_assignments_(nullptr),
      solve_db_(nullptr),
      improve_db_(nullptr),
      restore_assignment_(nullptr),
      assignment_(nullptr),
      preassignment_(nullptr),
      limit_(nullptr),
      ls_limit_(nullptr),
      lns_limit_(nullptr) {
  VLOG(1) << "Model parameters:\n" << parameters.DebugString();
  ConstraintSolverParameters solver_parameters =
      parameters.has_solver_parameters() ? parameters.solver_parameters()
                                         : Solver::DefaultSolverParameters();
  solver_.reset(new Solver("Routing", solver_parameters));
  InitializeBuilders(solver_.get());
  CHECK_EQ(vehicles, starts.size());
  CHECK_EQ(vehicles, ends.size());
#if defined(_MSC_VER)
  std::unordered_set<NodeIndex, TypedIntHasher<NodeIndex>> depot_set;
#else
  std::unordered_set<NodeIndex, std::hash<NodeIndex>> depot_set;
#endif
  std::vector<std::pair<NodeIndex, NodeIndex>> start_ends(starts.size());
  for (int i = 0; i < starts.size(); ++i) {
    depot_set.insert(starts[i]);
    depot_set.insert(ends[i]);
    start_ends[i] = std::make_pair(starts[i], ends[i]);
  }
  start_end_count_ = depot_set.size();
  Initialize();
  SetStartEnd(start_ends);
}

void RoutingModel::Initialize() {
  const int size = Size();
  // Next variables
  solver_->MakeIntVarArray(size, 0, size + vehicles_ - 1, "Nexts", &nexts_);
  solver_->AddConstraint(solver_->MakeAllDifferent(nexts_, false));
  node_to_disjunctions_.resize(size + vehicles_);
  // Vehicle variables. In case that node i is not active, vehicle_vars_[i] is
  // bound to -1.
  solver_->MakeIntVarArray(size + vehicles_, -1, vehicles_ - 1, "Vehicles",
                           &vehicle_vars_);
  // Active variables
  solver_->MakeBoolVarArray(size, "Active", &active_);
  // Is-bound-to-end variables.
  solver_->MakeBoolVarArray(size + vehicles_, "IsBoundToEnd",
                            &is_bound_to_end_);
  // Cost cache
  cost_cache_.clear();
  cost_cache_.resize(size + vehicles_);
  for (int i = 0; i < size + vehicles_; ++i) {
    CostCacheElement& cache = cost_cache_[i];
    cache.index = kUnassigned;
    cache.cost_class_index = CostClassIndex(-1);
    cache.cost = 0;
  }
  preassignment_ = solver_->MakeAssignment();
}

void RoutingModel::InitializeBuilders(Solver* solver) {
  solver->RegisterBuilder(RoutingModelVisitor::kLightElement,
                          Solver::ConstraintBuilder(BuildLightElement));
  solver->RegisterBuilder(RoutingModelVisitor::kLightElement2,
                          Solver::ConstraintBuilder(BuildLightElement2));
}

RoutingModel::~RoutingModel() {
  STLDeleteElements(&owned_node_callbacks_);
  STLDeleteElements(&dimensions_);
  STLDeleteElements(&owned_state_dependent_callbacks_);
}

RoutingModelParameters RoutingModel::DefaultModelParameters() {
  RoutingModelParameters parameters;
  ConstraintSolverParameters* const solver_parameters =
      parameters.mutable_solver_parameters();
  *solver_parameters = Solver::DefaultSolverParameters();
  solver_parameters->set_compress_trail(
         ConstraintSolverParameters::COMPRESS_WITH_ZLIB);
  parameters.set_reduce_vehicle_cost_model(true);
  return parameters;
}

RoutingSearchParameters RoutingModel::DefaultSearchParameters() {
  static const char* const kSearchParameters =
      "first_solution_strategy: AUTOMATIC "
      "use_filtered_first_solution_strategy: true "
      "local_search_operators {"
      "  use_relocate: true"
      "  use_relocate_pair: true"
      "  use_relocate_neighbors: false"
      "  use_exchange: true"
      "  use_cross: true"
      "  use_cross_exchange: false"
      "  use_two_opt: true"
      "  use_or_opt: true"
      "  use_lin_kernighan: true"
      "  use_tsp_opt: false"
      "  use_make_active: true"
      "  use_relocate_and_make_active: false"  // costly if true by default
      "  use_make_inactive: true"
      "  use_make_chain_inactive: false"
      "  use_swap_active: true"
      "  use_extended_swap_active: false"
      "  use_node_pair_swap_active: true"
      "  use_path_lns: false"
      "  use_full_path_lns: false"
      "  use_tsp_lns: false"
      "  use_inactive_lns: false"
      "}"
      "local_search_metaheuristic: AUTOMATIC "
      "guided_local_search_lambda_coefficient: 0.1 "
      "use_depth_first_search: false "
      "optimization_step: 1 "
      "solution_limit: 0x7FFFFFFFFFFFFFFF "  // kint64max
      "time_limit_ms: 0x7FFFFFFFFFFFFFFF "   // kint64max
      "lns_time_limit_ms: 100 "
      "use_light_propagation: true "
      "fingerprint_arc_cost_evaluators: true ";
  RoutingSearchParameters parameters;
  if (!google::protobuf::TextFormat::ParseFromString(kSearchParameters, &parameters)) {
    LOG(ERROR) << "Unsupported default search parameters: "
               << kSearchParameters;
  }
  return parameters;
}

void RoutingModel::AddNoCycleConstraintInternal() {
  if (no_cycle_constraint_ == nullptr) {
    no_cycle_constraint_ = solver_->MakeNoCycle(nexts_, active_);
    solver_->AddConstraint(no_cycle_constraint_);
  }
}

bool RoutingModel::AddDimension(NodeEvaluator2* evaluator, int64 slack_max,
                                int64 capacity, bool fix_start_cumul_to_zero,
                                const std::string& dimension_name) {
  const std::vector<NodeEvaluator2*> evaluators(vehicles_, evaluator);
  std::vector<int64> capacities(vehicles_, capacity);
  return AddDimensionWithCapacityInternal(
      evaluators, slack_max, std::move(capacities), fix_start_cumul_to_zero,
      dimension_name);
}

bool RoutingModel::AddDimensionWithVehicleTransits(
    const std::vector<NodeEvaluator2*>& evaluators, int64 slack_max,
    int64 capacity, bool fix_start_cumul_to_zero,
    const std::string& dimension_name) {
  std::vector<int64> capacities(vehicles_, capacity);
  return AddDimensionWithCapacityInternal(
      evaluators, slack_max, std::move(capacities), fix_start_cumul_to_zero,
      dimension_name);
}

bool RoutingModel::AddDimensionWithVehicleCapacity(
    NodeEvaluator2* evaluator, int64 slack_max,
    std::vector<int64> vehicle_capacities, bool fix_start_cumul_to_zero,
    const std::string& dimension_name) {
  const std::vector<NodeEvaluator2*> evaluators(vehicles_, evaluator);
  return AddDimensionWithCapacityInternal(
      evaluators, slack_max, std::move(vehicle_capacities),
      fix_start_cumul_to_zero, dimension_name);
}

bool RoutingModel::AddDimensionWithVehicleTransitAndCapacity(
    const std::vector<NodeEvaluator2*>& evaluators, int64 slack_max,
    std::vector<int64> vehicle_capacities, bool fix_start_cumul_to_zero,
    const std::string& dimension_name) {
  return AddDimensionWithCapacityInternal(
      evaluators, slack_max, std::move(vehicle_capacities),
      fix_start_cumul_to_zero, dimension_name);
}

bool RoutingModel::AddDimensionWithCapacityInternal(
    const std::vector<NodeEvaluator2*>& evaluators, int64 slack_max,
    std::vector<int64> vehicle_capacities, bool fix_start_cumul_to_zero,
    const std::string& dimension_name) {
  CHECK_EQ(vehicles_, vehicle_capacities.size());
  return InitializeDimensionInternal(
      evaluators, std::vector<VariableNodeEvaluator2*>(), slack_max,
      fix_start_cumul_to_zero,
      new RoutingDimension(this, std::move(vehicle_capacities), dimension_name,
                           nullptr));
}

bool RoutingModel::InitializeDimensionInternal(
    const std::vector<NodeEvaluator2*>& evaluators,
    const std::vector<VariableNodeEvaluator2*>& state_dependent_evaluators,
    int64 slack_max, bool fix_start_cumul_to_zero,
    RoutingDimension* dimension) {
  CHECK(dimension != nullptr);
  CHECK_EQ(vehicles_, evaluators.size());
  CHECK((dimension->base_dimension_ == nullptr &&
         state_dependent_evaluators.empty()) ||
        vehicles_ == state_dependent_evaluators.size());
  if (!HasDimension(dimension->name())) {
    const DimensionIndex dimension_index(dimensions_.size());
    dimension_name_to_index_[dimension->name()] = dimension_index;
    dimensions_.push_back(dimension);
    std::vector<NodeEvaluator2*> cached_evaluators;
    for (NodeEvaluator2* const evaluator : evaluators) {
      CHECK(evaluator != nullptr);
      cached_evaluators.push_back(NewCachedCallback(evaluator));
    }
    std::vector<VariableNodeEvaluator2*> cached_state_dependent_evaluators;
    for (VariableNodeEvaluator2* const evaluator : state_dependent_evaluators) {
      CHECK(evaluator != nullptr);
      cached_state_dependent_evaluators.push_back(
          NewCachedStateDependentCallback(evaluator));
    }
    dimension->Initialize(cached_evaluators, cached_state_dependent_evaluators,
                          slack_max);
    solver_->AddConstraint(solver_->MakeDelayedPathCumul(
        nexts_, active_, dimension->cumuls(), dimension->transits()));
    if (fix_start_cumul_to_zero) {
      for (int i = 0; i < vehicles_; ++i) {
        IntVar* const start_cumul = dimension->CumulVar(Start(i));
        CHECK_EQ(0, start_cumul->Min());
        start_cumul->SetValue(0);
      }
    }
    return true;
  } else {
    delete dimension;
    std::unordered_set<NodeEvaluator2*> evaluator_set(evaluators.begin(),
                                                      evaluators.end());
    STLDeleteElements(&evaluator_set);
    std::unordered_set<VariableNodeEvaluator2*> dependent_evaluator_set(
        state_dependent_evaluators.begin(), state_dependent_evaluators.end());
    STLDeleteElements(&dependent_evaluator_set);
    return false;
  }
}

bool RoutingModel::AddConstantDimensionWithSlack(int64 value, int64 capacity,
                                                 int64 slack_max,
                                                 bool fix_start_cumul_to_zero,
                                                 const std::string& dimension_name) {
  return AddDimension(
      ConstantEvaluator<int64>::MakeNodeEvaluatorCallback(value, solver_.get()),
      slack_max, capacity, fix_start_cumul_to_zero, dimension_name);
}

bool RoutingModel::AddVectorDimension(std::vector<int64> values, int64 capacity,
                                      bool fix_start_cumul_to_zero,
                                      const std::string& dimension_name) {
  VectorEvaluator* const evaluator =
      solver_->RevAlloc(new VectorEvaluator(std::move(values)));
  return AddDimension(NewPermanentCallback(evaluator, &VectorEvaluator::Value),
                      0, capacity, fix_start_cumul_to_zero, dimension_name);
}

bool RoutingModel::AddMatrixDimension(std::vector<std::vector<int64>> values,
                                      int64 capacity,
                                      bool fix_start_cumul_to_zero,
                                      const std::string& dimension_name) {
  MatrixEvaluator* const evaluator =
      solver_->RevAlloc(new MatrixEvaluator(std::move(values)));
  return AddDimension(NewPermanentCallback(evaluator, &MatrixEvaluator::Value),
                      0, capacity, fix_start_cumul_to_zero, dimension_name);
}

namespace {
template <class A, class B>
static int64 ReturnZero(A a, B b) {
  return 0;
}

// RangeMakeElementExpr is an IntExpr that corresponds to a
// RangeIntToIntFunction indexed by an IntVar.
// Do not create this class dicretly, but rather use MakeRangeMakeElementExpr.
class RangeMakeElementExpr : public BaseIntExpr {
 public:
  RangeMakeElementExpr(const RangeIntToIntFunction* callback, IntVar* index,
                       Solver* s)
      : BaseIntExpr(s), callback_(CHECK_NOTNULL(callback)), index_(index) {
    CHECK(callback_ != nullptr);
    CHECK(index != nullptr);
  }

  int64 Min() const override {
    // Converting [index_->Min(), index_->Max()] to [idx_min, idx_max).
    const int idx_min = index_->Min();
    const int idx_max = index_->Max() + 1;
    return (idx_min < idx_max) ? callback_->RangeMin(idx_min, idx_max)
                               : kint64max;
  }
  void SetMin(int64 new_min) override {
    const int64 old_min = Min();
    const int64 old_max = Max();
    if (old_min < new_min && new_min <= old_max) {
      const int64 old_idx_min = index_->Min();
      const int64 old_idx_max = index_->Max() + 1;
      if (old_idx_min < old_idx_max) {
        const int64 new_idx_min = callback_->RangeFirstInsideInterval(
            old_idx_min, old_idx_max, new_min, old_max + 1);
        index_->SetMin(new_idx_min);
        if (new_idx_min < old_idx_max) {
          const int64 new_idx_max = callback_->RangeLastInsideInterval(
              new_idx_min, old_idx_max, new_min, old_max + 1);
          index_->SetMax(new_idx_max);
        }
      }
    }
  }
  int64 Max() const override {
    // Converting [index_->Min(), index_->Max()] to [idx_min, idx_max).
    const int idx_min = index_->Min();
    const int idx_max = index_->Max() + 1;
    return (idx_min < idx_max) ? callback_->RangeMax(idx_min, idx_max)
                               : kint64min;
  }
  void SetMax(int64 new_max) override {
    const int64 old_min = Min();
    const int64 old_max = Max();
    if (old_min <= new_max && new_max < old_max) {
      const int64 old_idx_min = index_->Min();
      const int64 old_idx_max = index_->Max() + 1;
      if (old_idx_min < old_idx_max) {
        const int64 new_idx_min = callback_->RangeFirstInsideInterval(
            old_idx_min, old_idx_max, old_min, new_max + 1);
        index_->SetMin(new_idx_min);
        if (new_idx_min < old_idx_max) {
          const int64 new_idx_max = callback_->RangeLastInsideInterval(
              new_idx_min, old_idx_max, old_min, new_max + 1);
          index_->SetMax(new_idx_max);
        }
      }
    }
  }
  void WhenRange(Demon* d) override { index_->WhenRange(d); }

 private:
  const RangeIntToIntFunction* const callback_;
  IntVar* const index_;
};

IntExpr* MakeRangeMakeElementExpr(const RangeIntToIntFunction* callback,
                                  IntVar* index, Solver* s) {
  return s->RegisterIntExpr(
      s->RevAlloc(new RangeMakeElementExpr(callback, index, s)));
}
}  // namespace

bool RoutingModel::AddDimensionDependentDimensionWithVehicleCapacityInternal(
    const std::vector<NodeEvaluator2*>& pure_transits,
    const std::vector<VariableNodeEvaluator2*>& dependent_transits,
    const RoutingDimension* base_dimension, int64 slack_max,
    std::vector<int64> vehicle_capacities, bool fix_start_cumul_to_zero,
    const std::string& name) {
  CHECK_EQ(vehicles_, vehicle_capacities.size());
  RoutingDimension* new_dimension = nullptr;
  if (base_dimension == nullptr) {
    new_dimension = new RoutingDimension(this, std::move(vehicle_capacities),
                                         name, RoutingDimension::SelfBased());
  } else {
    new_dimension = new RoutingDimension(this, std::move(vehicle_capacities),
                                         name, base_dimension);
  }
  return InitializeDimensionInternal(pure_transits, dependent_transits,
                                     slack_max, fix_start_cumul_to_zero,
                                     new_dimension);
}

bool RoutingModel::AddDimensionDependentDimensionWithVehicleCapacity(
    const std::vector<VariableNodeEvaluator2*>& dependent_evaluators,
    const RoutingDimension* base_dimension, int64 slack_max,
    std::vector<int64> vehicle_capacities, bool fix_start_cumul_to_zero,
    const std::string& name) {
  NodeEvaluator2* const zero_evaluator =
      NewPermanentCallback(&ReturnZero<NodeIndex, NodeIndex>);
  const std::vector<NodeEvaluator2*> pure_transits(vehicles_, zero_evaluator);
  return AddDimensionDependentDimensionWithVehicleCapacity(
      pure_transits, dependent_evaluators, base_dimension, slack_max,
      std::move(vehicle_capacities), fix_start_cumul_to_zero, name);
}

bool RoutingModel::AddDimensionDependentDimensionWithVehicleCapacity(
    NodeEvaluator2* pure_transits, VariableNodeEvaluator2* dependent_transits,
    const RoutingDimension* base_dimension, int64 slack_max,
    int64 vehicle_capacity, bool fix_start_cumul_to_zero, const std::string& name) {
  std::vector<NodeEvaluator2*> pure_evaluators(vehicles_, pure_transits);
  std::vector<VariableNodeEvaluator2*> transit_evaluators(vehicles_,
                                                          dependent_transits);
  std::vector<int64> vehicle_capacities(vehicles_, vehicle_capacity);
  return AddDimensionDependentDimensionWithVehicleCapacityInternal(
      pure_evaluators, transit_evaluators, base_dimension, slack_max,
      std::move(vehicle_capacities), fix_start_cumul_to_zero, name);
}

bool RoutingModel::AddDimensionDependentDimensionWithVehicleCapacity(
    VariableNodeEvaluator2* transits, const RoutingDimension* dimension,
    int64 slack_max, int64 vehicle_capacity, bool fix_start_cumul_to_zero,
    const std::string& name) {
  NodeEvaluator2* const zero_evaluator =
      NewPermanentCallback(&ReturnZero<NodeIndex, NodeIndex>);
  return AddDimensionDependentDimensionWithVehicleCapacity(
      zero_evaluator, transits, dimension, slack_max, vehicle_capacity,
      fix_start_cumul_to_zero, name);
}

RoutingModel::StateDependentTransit RoutingModel::MakeStateDependentTransit(
    const std::function<int64(int64)>& f, int64 domain_start,
    int64 domain_end) {
  const std::function<int64(int64)> g = [&f](int64 x) { return f(x) + x; };
  // The next line is safe, because MakeCachedIntToIntFunction does not count
  // on keeping the closure of its first argument alive.
  return {MakeCachedIntToIntFunction(f, domain_start, domain_end),
          MakeCachedRangeMinMaxIndexFunction(g, domain_start, domain_end)};
}

std::vector<std::string> RoutingModel::GetAllDimensionNames() const {
  std::vector<std::string> dimension_names;
  for (const auto& dimension_name_index : dimension_name_to_index_) {
    dimension_names.push_back(dimension_name_index.first);
  }
  return dimension_names;
}

bool RoutingModel::HasDimension(const std::string& dimension_name) const {
  return ContainsKey(dimension_name_to_index_, dimension_name);
}

RoutingModel::DimensionIndex RoutingModel::GetDimensionIndex(
    const std::string& dimension_name) const {
  return FindWithDefault(dimension_name_to_index_, dimension_name,
                         kNoDimension);
}

const RoutingDimension& RoutingModel::GetDimensionOrDie(
    const std::string& dimension_name) const {
  return *dimensions_[FindOrDie(dimension_name_to_index_, dimension_name)];
}

RoutingDimension* RoutingModel::GetMutableDimension(
    const std::string& dimension_name) const {
  const DimensionIndex index = GetDimensionIndex(dimension_name);
  if (index != kNoDimension) {
    return dimensions_[index];
  }
  return nullptr;
}

void RoutingModel::AddAllActive() {
  for (IntVar* const active : active_) {
    if (active->Max() != 0) {
      active->SetValue(1);
    }
  }
}

void RoutingModel::SetArcCostEvaluatorOfAllVehicles(NodeEvaluator2* evaluator) {
  CHECK_LT(0, vehicles_);
  for (int i = 0; i < vehicles_; ++i) {
    SetArcCostEvaluatorOfVehicle(evaluator, i);
  }
}

void RoutingModel::SetArcCostEvaluatorOfVehicle(NodeEvaluator2* evaluator,
                                                int vehicle) {
  CHECK(evaluator != nullptr);
  CHECK_LT(vehicle, vehicles_);
  CHECK(evaluator->IsRepeatable());
  transit_cost_of_vehicle_[vehicle] = evaluator;
  owned_node_callbacks_.insert(evaluator);
}

void RoutingModel::SetFixedCostOfAllVehicles(int64 cost) {
  for (int i = 0; i < vehicles_; ++i) {
    SetFixedCostOfVehicle(cost, i);
  }
}

int64 RoutingModel::GetFixedCostOfVehicle(int vehicle) const {
  CHECK_LT(vehicle, vehicles_);
  return fixed_cost_of_vehicle_[vehicle];
}

void RoutingModel::SetFixedCostOfVehicle(int64 cost, int vehicle) {
  CHECK_LT(vehicle, vehicles_);
  DCHECK_GE(cost, 0);
  fixed_cost_of_vehicle_[vehicle] = cost;
}

namespace {
// Some C++ versions used in the open-source export don't support comparison
// functors for STL containers; so we need a comparator class instead.
struct CostClassComparator {
  bool operator()(const RoutingModel::CostClass& a,
                  const RoutingModel::CostClass& b) const {
    return RoutingModel::CostClass::LessThan(a, b);
  }
};

struct VehicleClassComparator {
  bool operator()(const RoutingModel::VehicleClass& a,
                  const RoutingModel::VehicleClass& b) const {
    return RoutingModel::VehicleClass::LessThan(a, b);
  }
};
}  // namespace

// static
const RoutingModel::CostClassIndex RoutingModel::kCostClassIndexOfZeroCost =
    CostClassIndex(0);

uint64 RoutingModel::GetFingerprintOfEvaluator(
    RoutingModel::NodeEvaluator2* evaluator,
    bool fingerprint_arc_cost_evaluators) const {
  if (!fingerprint_arc_cost_evaluators) {
    // If we don't fingerprint the data returned by the evaluator, we can
    // just return the address as fingerprint (ensures that evaluators with the
    // same address are considered as equivalent).
    return bit_cast<uint64>(evaluator);
  }
  uint64 evaluator_fprint = 0;
  const int max_row_size = Size() + vehicles_;
  std::unique_ptr<int64[]> row(new int64[max_row_size]);
  for (int64 from = 0; from < Size(); ++from) {
    if (IsEnd(from)) continue;
    const bool from_start = IsStart(from);
    int row_size = 0;
    for (int64 to = 0; to < max_row_size; ++to) {
      // (from, from), (end, start) and (start, end) arcs are never
      // evaluated; some clients check this.
      if (from != to && !IsStart(to) && (!from_start || !IsEnd(to))) {
        row[row_size] = evaluator->Run(IndexToNode(from), IndexToNode(to));
        ++row_size;
      }
    }
    const int row_num_bytes = row_size * sizeof(int64);
    const uint64 fprint = ThoroughHash(
        reinterpret_cast<const char*>(row.get()), row_num_bytes);
    // MixTwoUInt64 never returns 0.
    evaluator_fprint = evaluator_fprint != 0
                           ? MixTwoUInt64(evaluator_fprint, fprint)
                           : fprint;
  }
  return evaluator_fprint;
}

void RoutingModel::ComputeCostClasses(
    const RoutingSearchParameters& parameters) {
  // First, detect if all non-null transit cost evaluators are equal.
  bool all_evaluators_equal = true;
  // Find first non-null evaluator.
  int evaluator_index = 0;
  while (evaluator_index < transit_cost_of_vehicle_.size() &&
         transit_cost_of_vehicle_[evaluator_index] == nullptr) {
    ++evaluator_index;
  }
  // Compare non-null evaluators.
  if (evaluator_index < transit_cost_of_vehicle_.size()) {
    const NodeEvaluator2* reference_evaluator =
        transit_cost_of_vehicle_[evaluator_index];
    while (++evaluator_index < transit_cost_of_vehicle_.size()) {
      NodeEvaluator2* const evaluator =
          transit_cost_of_vehicle_[evaluator_index];
      if (evaluator != nullptr && evaluator != reference_evaluator) {
        all_evaluators_equal = false;
        break;
      }
    }
  }

  // Then we create and reduce the cost classes.
  cost_classes_.reserve(vehicles_);
  cost_classes_.clear();
  cost_class_index_of_vehicle_.assign(vehicles_, CostClassIndex(-1));
  std::map<CostClass, CostClassIndex, CostClassComparator> cost_class_map;

  // Pre-insert the built-in cost class 'zero cost' with index 0.
  const uint64 kNullEvaluatorFprint = 0;
  NodeEvaluator2* const null_evaluator = nullptr;
  std::unordered_map<NodeEvaluator2*, uint64> evaluator_to_fprint;
  std::unordered_map<uint64, NodeEvaluator2*> fprint_to_cached_evaluator;
  NodeEvaluator2* const zero_evaluator =
      NewPermanentCallback(&ReturnZero<NodeIndex, NodeIndex>);
  owned_node_callbacks_.insert(zero_evaluator);
  evaluator_to_fprint[null_evaluator] = kNullEvaluatorFprint;
  fprint_to_cached_evaluator[kNullEvaluatorFprint] = zero_evaluator;
  const CostClass zero_cost_class(zero_evaluator);
  cost_classes_.push_back(zero_cost_class);
  DCHECK_EQ(zero_evaluator,
            cost_classes_[kCostClassIndexOfZeroCost].arc_cost_evaluator);
  cost_class_map[zero_cost_class] = kCostClassIndexOfZeroCost;

  // Determine the canonicalized cost class for each vehicle, and insert it as
  // a new cost class if it doesn't exist already. Building cached evaluators on
  // the way.
  const uint64 kAllEquivalentEvaluatorFprint = 1;
  bool has_vehicle_with_zero_cost_class = false;
  for (int vehicle = 0; vehicle < transit_cost_of_vehicle_.size(); ++vehicle) {
    NodeEvaluator2* const uncached_evaluator =
        transit_cost_of_vehicle_[vehicle];
    uint64 evaluator_fprint = kNullEvaluatorFprint;
    // We try really hard not to evaluate the fingerprint of an evaluator, if
    // we can avoid to: we detect duplicate evaluators, for example, and if
    // there's only one evaluator callback used, we don't bother computing its
    // fingerprint.
    if (!FindCopy(evaluator_to_fprint, uncached_evaluator, &evaluator_fprint)) {
      evaluator_fprint =
          all_evaluators_equal
              ? kAllEquivalentEvaluatorFprint
              : GetFingerprintOfEvaluator(
                    uncached_evaluator,
                    parameters.fingerprint_arc_cost_evaluators());
      evaluator_to_fprint[uncached_evaluator] = evaluator_fprint;
    }
    NodeEvaluator2** cached_evaluator =
        &LookupOrInsert(&fprint_to_cached_evaluator, evaluator_fprint, nullptr);
    if (*cached_evaluator == nullptr) {
      *cached_evaluator = NewCachedCallback(uncached_evaluator);
    }
    CostClass cost_class(*cached_evaluator);
    // Insert the dimension data in a canonical way.
    for (const RoutingDimension* const dimension : dimensions_) {
      const int64 coeff = dimension->vehicle_span_cost_coefficients()[vehicle];
      if (coeff == 0) continue;
      cost_class.dimension_transit_evaluator_class_and_cost_coefficient
          .push_back({dimension->vehicle_to_class(vehicle), coeff, dimension});
    }
    std::sort(cost_class.dimension_transit_evaluator_class_and_cost_coefficient
                  .begin(),
              cost_class.dimension_transit_evaluator_class_and_cost_coefficient
                  .end());
    // Try inserting the CostClass, if it's not already present.
    const CostClassIndex num_cost_classes(cost_classes_.size());
    const CostClassIndex cost_class_index =
        LookupOrInsert(&cost_class_map, cost_class, num_cost_classes);
    if (cost_class_index == kCostClassIndexOfZeroCost) {
      has_vehicle_with_zero_cost_class = true;
    } else if (cost_class_index == num_cost_classes) {  // New cost class.
      cost_classes_.push_back(cost_class);
    }
    cost_class_index_of_vehicle_[vehicle] = cost_class_index;
  }

  // TRICKY:
  // If some vehicle had the "zero" cost class, then we'll have homogeneous
  // vehicles iff they all have that cost class (i.e. cost class count = 1).
  // If none of them have it, then we have homogeneous costs iff there are two
  // cost classes: the unused "zero" cost class and the one used by all
  // vehicles.
  // Note that we always need the zero cost class, even if no vehicle uses it,
  // because we use it in the vehicle_var = -1 scenario (i.e. unperformed).
  //
  // Fixed costs are simply ignored for computing these cost classes. They are
  // attached to start nodes directly.
  costs_are_homogeneous_across_vehicles_ &= has_vehicle_with_zero_cost_class
                                                ? GetCostClassesCount() == 1
                                                : GetCostClassesCount() <= 2;
}

bool RoutingModel::VehicleClass::LessThan(const VehicleClass& a,
                                          const VehicleClass& b) {
  if (a.cost_class_index != b.cost_class_index) {
    return a.cost_class_index < b.cost_class_index;
  }
  if (a.fixed_cost != b.fixed_cost) {
    return a.fixed_cost < b.fixed_cost;
  }
  if (a.start != b.start) {
    return a.start < b.start;
  }
  if (a.end != b.end) {
    return a.end < b.end;
  }
  if (a.unvisitable_nodes_fprint != b.unvisitable_nodes_fprint) {
    return a.unvisitable_nodes_fprint < b.unvisitable_nodes_fprint;
  }
  if (a.dimension_start_cumuls_min != b.dimension_start_cumuls_min) {
    return a.dimension_start_cumuls_min < b.dimension_start_cumuls_min;
  }
  if (a.dimension_start_cumuls_max != b.dimension_start_cumuls_max) {
    return a.dimension_start_cumuls_max < b.dimension_start_cumuls_max;
  }
  if (a.dimension_end_cumuls_min != b.dimension_end_cumuls_min) {
    return a.dimension_end_cumuls_min < b.dimension_end_cumuls_min;
  }
  if (a.dimension_end_cumuls_max != b.dimension_end_cumuls_max) {
    return a.dimension_end_cumuls_max < b.dimension_end_cumuls_max;
  }
  if (a.dimension_capacities != b.dimension_capacities) {
    return a.dimension_capacities < b.dimension_capacities;
  }
  return a.dimension_evaluator_classes < b.dimension_evaluator_classes;
}

void RoutingModel::ComputeVehicleClasses() {
  vehicle_classes_.reserve(vehicles_);
  vehicle_classes_.clear();
  vehicle_class_index_of_vehicle_.assign(vehicles_, VehicleClassIndex(-1));
  std::map<VehicleClass, VehicleClassIndex, VehicleClassComparator>
      vehicle_class_map;
  const int nodes_unvisitability_num_bytes = (vehicle_vars_.size() + 7) / 8;
  std::unique_ptr<char[]> nodes_unvisitability_bitmask(
      new char[nodes_unvisitability_num_bytes]);
  for (int vehicle = 0; vehicle < transit_cost_of_vehicle_.size(); ++vehicle) {
    VehicleClass vehicle_class;
    vehicle_class.cost_class_index = cost_class_index_of_vehicle_[vehicle];
    vehicle_class.fixed_cost = fixed_cost_of_vehicle_[vehicle];
    vehicle_class.start = IndexToNode(Start(vehicle));
    vehicle_class.end = IndexToNode(End(vehicle));
    for (const RoutingDimension* const dimension : dimensions_) {
      IntVar* const start_cumul_var = dimension->cumuls()[Start(vehicle)];
      vehicle_class.dimension_start_cumuls_min.push_back(
          start_cumul_var->Min());
      vehicle_class.dimension_start_cumuls_max.push_back(
          start_cumul_var->Max());
      IntVar* const end_cumul_var = dimension->cumuls()[End(vehicle)];
      vehicle_class.dimension_end_cumuls_min.push_back(end_cumul_var->Min());
      vehicle_class.dimension_end_cumuls_max.push_back(end_cumul_var->Max());
      vehicle_class.dimension_capacities.push_back(
          dimension->vehicle_capacities()[vehicle]);
      vehicle_class.dimension_evaluator_classes.push_back(
          dimension->vehicle_to_class(vehicle));
    }
    memset(nodes_unvisitability_bitmask.get(), 0,
           nodes_unvisitability_num_bytes);
    for (int index = 0; index < vehicle_vars_.size(); ++index) {
      IntVar* const vehicle_var = vehicle_vars_[index];
      if (!IsStart(index) && !IsEnd(index) && !vehicle_var->Contains(vehicle)) {
        nodes_unvisitability_bitmask[index / CHAR_BIT] |= 1U
                                                          << (index % CHAR_BIT);
      }
    }
    vehicle_class.unvisitable_nodes_fprint = ThoroughHash(
        nodes_unvisitability_bitmask.get(), nodes_unvisitability_num_bytes);
    const VehicleClassIndex num_vehicle_classes(vehicle_classes_.size());
    const VehicleClassIndex vehicle_class_index =
        LookupOrInsert(&vehicle_class_map, vehicle_class, num_vehicle_classes);
    if (vehicle_class_index == num_vehicle_classes) {  // New vehicle class
      vehicle_classes_.push_back(vehicle_class);
    }
    vehicle_class_index_of_vehicle_[vehicle] = vehicle_class_index;
  }
}

void RoutingModel::AddDisjunction(const std::vector<NodeIndex>& nodes) {
  AddDisjunctionInternal(nodes, kNoPenalty, 1);
}

void RoutingModel::AddDisjunction(const std::vector<NodeIndex>& nodes,
                                  int64 penalty) {
  AddDisjunction(nodes, penalty, 1);
}

void RoutingModel::AddDisjunction(const std::vector<NodeIndex>& nodes,
                                  int64 penalty, int64 max_cardinality) {
  CHECK_GE(penalty, 0) << "Penalty must be positive";
  CHECK_GE(max_cardinality, 1);
  AddDisjunctionInternal(nodes, penalty, max_cardinality);
}

void RoutingModel::AddDisjunctionInternal(const std::vector<NodeIndex>& nodes,
                                          int64 penalty,
                                          int64 max_cardinality) {
  const int size = disjunctions_.size();
  disjunctions_.resize(size + 1);
  std::vector<int>& disjunction_nodes = disjunctions_.back().nodes;
  disjunction_nodes.resize(nodes.size());
  for (int i = 0; i < nodes.size(); ++i) {
    CHECK_NE(kUnassigned, node_to_index_[nodes[i]]);
    disjunction_nodes[i] = node_to_index_[nodes[i]];
  }
  disjunctions_.back().value.penalty = penalty;
  disjunctions_.back().value.max_cardinality = max_cardinality;
  for (const NodeIndex node : nodes) {
    node_to_disjunctions_[node_to_index_[node]].push_back(
        DisjunctionIndex(size));
  }
}

IntVar* RoutingModel::CreateDisjunction(DisjunctionIndex disjunction) {
  const std::vector<int>& nodes = disjunctions_[disjunction].nodes;
  const int nodes_size = nodes.size();
  std::vector<IntVar*> disjunction_vars(nodes_size);
  for (int i = 0; i < nodes_size; ++i) {
    const int node = nodes[i];
    CHECK_LT(node, Size());
    disjunction_vars[i] = ActiveVar(node);
  }
  const int64 max_cardinality =
      disjunctions_[disjunction].value.max_cardinality;
  IntVar* no_active_var = solver_->MakeBoolVar();
  IntVar* number_active_vars = solver_->MakeIntVar(0, max_cardinality);
  solver_->AddConstraint(
      solver_->MakeSumEquality(disjunction_vars, number_active_vars));
  solver_->AddConstraint(solver_->MakeIsDifferentCstCt(
      number_active_vars, max_cardinality, no_active_var));
  const int64 penalty = disjunctions_[disjunction].value.penalty;
  if (penalty < 0) {
    no_active_var->SetMax(0);
    return nullptr;
  } else {
    return solver_->MakeProd(no_active_var, penalty)->Var();
  }
}

void RoutingModel::AddSoftSameVehicleConstraint(
    const std::vector<NodeIndex>& nodes, int64 cost) {
  if (!nodes.empty()) {
    ValuedNodes<int64> same_vehicle_cost;
    for (const NodeIndex node : nodes) {
      same_vehicle_cost.nodes.push_back(node_to_index_[node]);
    }
    same_vehicle_cost.value = cost;
    same_vehicle_costs_.push_back(same_vehicle_cost);
  }
}

IntVar* RoutingModel::CreateSameVehicleCost(int index) {
  const std::vector<int>& nodes = same_vehicle_costs_[index].nodes;
  CHECK(!nodes.empty());
  std::vector<IntVar*> vehicle_counts;
  solver_->MakeIntVarArray(vehicle_vars_.size() + 1, 0, nodes.size() + 1,
                           &vehicle_counts);
  std::vector<int64> vehicle_values(vehicle_vars_.size() + 1);
  for (int i = 0; i < vehicle_vars_.size(); ++i) {
    vehicle_values[i] = i;
  }
  vehicle_values[vehicle_vars_.size()] = -1;
  std::vector<IntVar*> vehicle_vars;
  for (const int node : nodes) {
    vehicle_vars.push_back(vehicle_vars_[node]);
  }
  solver_->AddConstraint(solver_->MakeDistribute(vehicle_vars, vehicle_counts));
  std::vector<IntVar*> vehicle_used;
  for (int i = 0; i < vehicle_vars_.size() + 1; ++i) {
    vehicle_used.push_back(
        solver_->MakeIsGreaterOrEqualCstVar(vehicle_counts[i], 1));
  }
  vehicle_used.push_back(solver_->MakeIntConst(-1));
  return solver_
      ->MakeProd(solver_->MakeMax(solver_->MakeSum(vehicle_used), 0),
                 same_vehicle_costs_[index].value)
      ->Var();
}

void RoutingModel::AddLocalSearchOperator(LocalSearchOperator* ls_operator) {
  extra_operators_.push_back(ls_operator);
}

int64 RoutingModel::GetDepot() const { return vehicles() > 0 ? Start(0) : -1; }

void RoutingModel::SetStartEnd(
    const std::vector<std::pair<NodeIndex, NodeIndex>>& start_ends) {
  CHECK_EQ(start_ends.size(), vehicles_);
  const int size = Size();
#if defined(_MSC_VER)
  std::unordered_set<NodeIndex, TypedIntHasher<NodeIndex>> starts;
  std::unordered_set<NodeIndex, TypedIntHasher<NodeIndex>> ends;
#else
  std::unordered_set<NodeIndex, std::hash<NodeIndex>> starts;
  std::unordered_set<NodeIndex, std::hash<NodeIndex>> ends;
#endif
  for (const std::pair<NodeIndex, NodeIndex> start_end : start_ends) {
    const NodeIndex start = start_end.first;
    const NodeIndex end = start_end.second;
    CHECK_GE(start, 0);
    CHECK_GE(end, 0);
    CHECK_LE(start, nodes_);
    CHECK_LE(end, nodes_);
    starts.insert(start);
    ends.insert(end);
  }
  index_to_node_.resize(size + vehicles_);
  node_to_index_.resize(nodes_, kUnassigned);
  int index = 0;
  for (NodeIndex i = kFirstNode; i < nodes_; ++i) {
    if (starts.count(i) != 0 || ends.count(i) == 0) {
      index_to_node_[index] = i;
      node_to_index_[i] = index;
      ++index;
    }
  }
#if defined(_MSC_VER)
  std::unordered_set<NodeIndex, TypedIntHasher<NodeIndex>> node_set;
#else
  std::unordered_set<NodeIndex, std::hash<NodeIndex>> node_set;
#endif
  index_to_vehicle_.resize(size + vehicles_, kUnassigned);
  for (int i = 0; i < vehicles_; ++i) {
    const NodeIndex start = start_ends[i].first;
    if (node_set.count(start) == 0) {
      node_set.insert(start);
      const int start_index = node_to_index_[start];
      starts_[i] = start_index;
      CHECK_NE(kUnassigned, start_index);
      index_to_vehicle_[start_index] = i;
    } else {
      starts_[i] = index;
      index_to_node_[index] = start;
      index_to_vehicle_[index] = i;
      ++index;
    }
  }
  for (int i = 0; i < vehicles_; ++i) {
    NodeIndex end = start_ends[i].second;
    index_to_node_[index] = end;
    ends_[i] = index;
    CHECK_LE(size, index);
    index_to_vehicle_[index] = i;
    ++index;
  }

  // Logging model information.
  VLOG(1) << "Number of nodes: " << nodes_;
  VLOG(1) << "Number of vehicles: " << vehicles_;
  for (int index = 0; index < index_to_node_.size(); ++index) {
    VLOG(2) << "Variable index " << index << " -> Node index "
            << index_to_node_[index];
  }
  for (NodeIndex node = kFirstNode; node < node_to_index_.size(); ++node) {
    VLOG(2) << "Node index " << node << " -> Variable index "
            << node_to_index_[node];
  }
}

// TODO(user): Remove the need for the homogeneous version once the
// vehicle var to cost class element constraint is fast enough.
void RoutingModel::AppendHomogeneousArcCosts(
    const RoutingSearchParameters& parameters, int node_index,
    std::vector<IntVar*>* cost_elements) {
  CHECK(cost_elements != nullptr);
  const auto arc_cost_evaluator = [this, node_index](int64 next_index) {
    return GetHomogeneousCost(node_index, next_index);
  };
  if (UsesLightPropagation(parameters)) {
    // Only supporting positive costs.
    // TODO(user): Detect why changing lower bound to kint64min stalls
    // the search in GLS in some cases (Solomon instances for instance).
    IntVar* const base_cost_var = solver_->MakeIntVar(0, kint64max);
    solver_->AddConstraint(MakeLightElement(
        solver_.get(), base_cost_var, nexts_[node_index], arc_cost_evaluator));
    IntVar* const var =
        solver_->MakeProd(base_cost_var, active_[node_index])->Var();
    cost_elements->push_back(var);
  } else {
    IntExpr* const expr =
        solver_->MakeElement(arc_cost_evaluator, nexts_[node_index]);
    IntVar* const var = solver_->MakeProd(expr, active_[node_index])->Var();
    cost_elements->push_back(var);
  }
}

void RoutingModel::AppendArcCosts(const RoutingSearchParameters& parameters,
                                  int node_index,
                                  std::vector<IntVar*>* cost_elements) {
  CHECK(cost_elements != nullptr);
  DCHECK_GT(vehicles_, 0);
  if (UsesLightPropagation(parameters)) {
    // Only supporting positive costs.
    // TODO(user): Detect why changing lower bound to kint64min stalls
    // the search in GLS in some cases (Solomon instances for instance).
    IntVar* const base_cost_var = solver_->MakeIntVar(0, kint64max);
    solver_->AddConstraint(MakeLightElement2(
        solver_.get(), base_cost_var, nexts_[node_index],
        vehicle_vars_[node_index], [this, node_index](int64 to, int64 vehicle) {
          return GetArcCostForVehicle(node_index, to, vehicle);
        }));
    IntVar* const var =
        solver_->MakeProd(base_cost_var, active_[node_index])->Var();
    cost_elements->push_back(var);
  } else {
    IntVar* const vehicle_class_var =
        solver_
            ->MakeElement(
                [this](int64 index) {
                  return SafeGetCostClassInt64OfVehicle(index);
                },
                vehicle_vars_[node_index])
            ->Var();
    IntExpr* const expr = solver_->MakeElement(
        [this, node_index](int64 next, int64 vehicle_class) {
          return GetArcCostForClass(node_index, next, vehicle_class);
        },
        nexts_[node_index], vehicle_class_var);
    IntVar* const var = solver_->MakeProd(expr, active_[node_index])->Var();
    cost_elements->push_back(var);
  }
}

int RoutingModel::GetVehicleStartClass(int64 start_index) const {
  const int vehicle = index_to_vehicle_[start_index];
  if (vehicle != kUnassigned) {
    return GetVehicleClassIndexOfVehicle(vehicle).value();
  }
  return kUnassigned;
}

bool RoutingModel::ValidateSearchParameters(
    const RoutingSearchParameters& search_parameters) {
  bool valid = true;
  const FirstSolutionStrategy::Value first_solution_strategy =
      search_parameters.first_solution_strategy();
  if (GetFirstSolutionDecisionBuilder(search_parameters) == nullptr) {
    LOG(ERROR) << "Undefined first solution strategy: "
               << first_solution_strategy;
    valid = false;
  }
  if (first_solution_strategy == FirstSolutionStrategy::SWEEP &&
      sweep_arranger() == nullptr) {
    LOG(ERROR) << "Undefined sweep arranger for ROUTING_SWEEP strategy.";
    valid = false;
  }
  if (!valid) {
    status_ = ROUTING_INVALID;
    return false;
  }
  return true;
}

void RoutingModel::QuietCloseModel() {
  QuietCloseModelWithParameters(DefaultSearchParameters());
}

void RoutingModel::CloseModel() {
  CloseModelWithParameters(DefaultSearchParameters());
}

class RoutingModelInspector : public ModelVisitor {
 public:
  explicit RoutingModelInspector(RoutingModel* model) : model_(model) {
    same_vehicle_components_.Init(model->Size());
    for (const std::string& name : model->GetAllDimensionNames()) {
      RoutingDimension* const dimension = model->GetMutableDimension(name);
      const std::vector<IntVar*>& cumuls = dimension->cumuls();
      for (int i = 0; i < cumuls.size(); ++i) {
        cumul_to_dim_indices_[cumuls[i]] = {dimension, i};
      }
    }
    const std::vector<IntVar*>& vehicle_vars = model->VehicleVars();
    for (int i = 0; i < vehicle_vars.size(); ++i) {
      vehicle_var_to_indices_[vehicle_vars[i]] = i;
    }
    RegisterInspectors();
  }
  ~RoutingModelInspector() override {}
  void EndVisitModel(const std::string& solver_name) override {
    // Compact same vehicle component indices.
    std::unordered_map<int, int> component_indices;
    int component_index = 0;
    for (int node = 0; node < model_->Size(); ++node) {
      const int component =
          same_vehicle_components_.GetClassRepresentative(node);
      if (InsertIfNotPresent(&component_indices, component, component_index)) {
        ++component_index;
      }
    }
    model_->InitSameVehicleGroups(component_indices.size());
    for (int node = 0; node < model_->Size(); ++node) {
      const int component =
          same_vehicle_components_.GetClassRepresentative(node);
      DCHECK(ContainsKey(component_indices, component));
      model_->SetSameVehicleGroup(
          node, FindWithDefault(component_indices, component, 0));
    }
    // TODO(user): Perform transitive closure of dimension precedence graphs.
    // TODO(user): Have a single annotated precedence graph.
  }
  void EndVisitConstraint(const std::string& type_name,
                          const Constraint* const constraint) override {
    FindWithDefault(constraint_inspectors_, type_name, []() {})();
  }
  void VisitIntegerExpressionArgument(const std::string& type_name,
                                      IntExpr* const expr) override {
    FindWithDefault(expr_inspectors_, type_name,
                    [](const IntExpr* expr) {})(expr);
  }
  void VisitIntegerArrayArgument(const std::string& arg_name,
                                 const std::vector<int64>& values) override {
    FindWithDefault(array_inspectors_, arg_name,
                    [](const std::vector<int64>& int_array) {})(values);
  }

 private:
  using ExprInspector = std::function<void(const IntExpr*)>;
  using ArrayInspector = std::function<void(const std::vector<int64>&)>;
  using ConstraintInspector = std::function<void()>;

  void RegisterInspectors() {
    expr_inspectors_[kExpressionArgument] = [this](const IntExpr* expr) {
      expr_ = expr;
    };
    expr_inspectors_[kLeftArgument] = [this](const IntExpr* expr) {
      left_ = expr;
    };
    expr_inspectors_[kRightArgument] = [this](const IntExpr* expr) {
      right_ = expr;
    };
    array_inspectors_[kStartsArgument] = [this](
        const std::vector<int64>& int_array) { starts_argument_ = int_array; };
    array_inspectors_[kEndsArgument] = [this](
        const std::vector<int64>& int_array) { ends_argument_ = int_array; };
    constraint_inspectors_[kNotMember] = [this]() {
      std::pair<RoutingDimension*, int> dim_index;
      if (FindCopy(cumul_to_dim_indices_, expr_, &dim_index)) {
        RoutingDimension* const dimension = dim_index.first;
        const int index = dim_index.second;
        dimension->forbidden_intervals_[index].InsertIntervals(starts_argument_,
                                                               ends_argument_);
        VLOG(2) << dimension->name() << " " << index << ": "
                << dimension->forbidden_intervals_[index].DebugString();
      }
      expr_ = nullptr;
      starts_argument_.clear();
      ends_argument_.clear();
    };
    constraint_inspectors_[kEquality] = [this]() {
      int left_index = 0;
      int right_index = 0;
      if (FindCopy(vehicle_var_to_indices_, left_, &left_index) &&
          FindCopy(vehicle_var_to_indices_, right_, &right_index)) {
        VLOG(2) << "Vehicle variables for " << left_index << " and "
                << right_index << " are equal.";
        same_vehicle_components_.AddArc(left_index, right_index);
      }
      left_ = nullptr;
      right_ = nullptr;
    };
    constraint_inspectors_[kLessOrEqual] = [this]() {
      std::pair<RoutingDimension*, int> left_index;
      std::pair<RoutingDimension*, int> right_index;
      if (FindCopy(cumul_to_dim_indices_, left_, &left_index) &&
          FindCopy(cumul_to_dim_indices_, right_, &right_index)) {
        RoutingDimension* const dimension = left_index.first;
        if (dimension == right_index.first) {
          VLOG(2) << "For dimension " << dimension->name() << ", cumul for "
                  << left_index.second << " is less than " << right_index.second
                  << ".";
          dimension->precedence_graph_.AddArc(left_index.second,
                                              right_index.second);
        }
      }
      left_ = nullptr;
      right_ = nullptr;
    };
  }

  RoutingModel* const model_;
  ConnectedComponents<int, int> same_vehicle_components_;
  std::unordered_map<const IntExpr*, std::pair<RoutingDimension*, int>>
      cumul_to_dim_indices_;
  std::unordered_map<const IntExpr*, int> vehicle_var_to_indices_;
  std::unordered_map<std::string, ExprInspector> expr_inspectors_;
  std::unordered_map<std::string, ArrayInspector> array_inspectors_;
  std::unordered_map<std::string, ConstraintInspector> constraint_inspectors_;
  const IntExpr* expr_ = nullptr;
  const IntExpr* left_ = nullptr;
  const IntExpr* right_ = nullptr;
  std::vector<int64> starts_argument_;
  std::vector<int64> ends_argument_;
};

void RoutingModel::CloseModelWithParameters(
    const RoutingSearchParameters& parameters) {
  if (closed_) {
    LOG(WARNING) << "Model already closed";
    return;
  }
  closed_ = true;

  for (RoutingDimension* const dimension : dimensions_) {
    dimension->CloseModel(UsesLightPropagation(parameters));
  }
  ComputeCostClasses(parameters);
  ComputeVehicleClasses();
  vehicle_start_class_callback_ = [this](int64 start) {
    return GetVehicleStartClass(start);
  };

  AddNoCycleConstraintInternal();

  const int size = Size();

  // Vehicle variable constraints
  for (int i = 0; i < vehicles_; ++i) {
    solver_->AddConstraint(solver_->MakeEquality(vehicle_vars_[starts_[i]],
                                                 solver_->MakeIntConst(i)));
    solver_->AddConstraint(solver_->MakeEquality(vehicle_vars_[ends_[i]],
                                                 solver_->MakeIntConst(i)));
  }

  // If there is only one vehicle in the model the vehicle variables will have
  // a maximum domain of [-1, 0]. If a node is performed/active then its vehicle
  // variable will be reduced to [0] making the path-cumul constraint below
  // useless. If the node is unperformed/unactive then its vehicle variable will
  // be reduced to [-1] in any case.
  if (vehicles_ > 1) {
    std::vector<IntVar*> zero_transit(size, solver_->MakeIntConst(Zero()));
    solver_->AddConstraint(solver_->MakeDelayedPathCumul(
        nexts_, active_, vehicle_vars_, zero_transit));
  }

  // Set all active unless there are disjunctions
  if (disjunctions_.size() == 0) {
    AddAllActive();
  }

  // Reduce domain of next varibles.
  for (int i = 0; i < size; ++i) {
    // No variable can point back to a start.
    nexts_[i]->RemoveValues(starts_);
    // Extra constraint to state an active node can't point to itself.
    solver_->AddConstraint(
        solver_->MakeIsDifferentCstCt(nexts_[i], i, active_[i]));
  }

  // Add constraints to bind vehicle_vars_[i] to -1 in case that node i is not
  // active.
  for (int i = 0; i < size; ++i) {
    solver_->AddConstraint(
        solver_->MakeIsDifferentCstCt(vehicle_vars_[i], -1, active_[i]));
  }

  // Associate first and "logical" last nodes
  for (int i = 0; i < vehicles_; ++i) {
    for (int j = 0; j < vehicles_; ++j) {
      if (i != j) {
        nexts_[starts_[i]]->RemoveValue(ends_[j]);
      }
    }
  }

  // Constraining is_bound_to_end_ variables.
  for (const int64 end : ends_) {
    is_bound_to_end_[end]->SetValue(1);
  }

  std::vector<IntVar*> cost_elements;
  // Arc and dimension costs.
  if (vehicles_ > 0) {
    if (GetNonZeroCostClassesCount() > 0) {
      for (int node_index = 0; node_index < size; ++node_index) {
        if (CostsAreHomogeneousAcrossVehicles()) {
          AppendHomogeneousArcCosts(parameters, node_index, &cost_elements);
        } else {
          AppendArcCosts(parameters, node_index, &cost_elements);
        }
      }
    }
  }
  // Dimension span costs
  for (const RoutingDimension* dimension : dimensions_) {
    dimension->SetupGlobalSpanCost(&cost_elements);
    dimension->SetupSlackAndDependentTransitCosts(&cost_elements);
  }
  // Penalty costs
  for (DisjunctionIndex i(0); i < disjunctions_.size(); ++i) {
    IntVar* penalty_var = CreateDisjunction(i);
    if (penalty_var != nullptr) {
      cost_elements.push_back(penalty_var);
    }
  }
  // Soft cumul upper bound costs
  for (const RoutingDimension* dimension : dimensions_) {
    dimension->SetupCumulVarSoftLowerBoundCosts(&cost_elements);
    dimension->SetupCumulVarSoftUpperBoundCosts(&cost_elements);
  }
  // Same vehicle costs
  for (int i = 0; i < same_vehicle_costs_.size(); ++i) {
    cost_elements.push_back(CreateSameVehicleCost(i));
  }
  cost_ = solver_->MakeSum(cost_elements)->Var();
  cost_->set_name("Cost");

  // Precedences
  std::vector<std::pair<int, int>> precedences;
  for (const auto& pair : pickup_delivery_pairs_) {
    precedences.push_back({pair.first[0], pair.second[0]});
  }
  solver_->AddConstraint(
      solver_->MakePathPrecedenceConstraint(nexts_, precedences));

  // Detect constraints
  std::unique_ptr<RoutingModelInspector> inspector(
      new RoutingModelInspector(this));
  solver_->Accept(inspector.get());

  // Dimension precedences, discovered by model inspection (which must be
  // performed before adding path transit precedences.
  for (const RoutingDimension* const dimension : dimensions_) {
    const ReverseArcListGraph<int, int>& graph =
        dimension->GetPrecedenceGraph();
    std::vector<std::pair<int, int>> precedences;
    for (const auto tail : graph.AllNodes()) {
      for (const auto head : graph[tail]) {
        precedences.emplace_back(tail, head);
      }
    }
    if (!precedences.empty()) {
      solver_->AddConstraint(solver_->MakePathTransitPrecedenceConstraint(
          nexts_, dimension->transits(), precedences));
    }
  }

  // Keep this out of SetupSearch as this contains static search objects.
  // This will allow calling SetupSearch multiple times with different search
  // parameters.
  CreateNeighborhoodOperators();
  CreateFirstSolutionDecisionBuilders(parameters);
  if (!ValidateSearchParameters(parameters)) {
    return;
  }
  SetupSearch(parameters);
}

struct Link {
  Link(std::pair<int, int> link, double value, int vehicle_class,
       int64 start_depot, int64 end_depot)
      : link(link),
        value(value),
        vehicle_class(vehicle_class),
        start_depot(start_depot),
        end_depot(end_depot) {}
  ~Link() {}

  std::pair<int, int> link;
  int64 value;
  int vehicle_class;
  int64 start_depot;
  int64 end_depot;
};

struct LinkSort {
  bool operator()(const Link& link1, const Link& link2) const {
    return (link1.value > link2.value);
  }
} LinkComparator;

struct VehicleClass {
  VehicleClass(RoutingModel::NodeIndex start_node, int64 start_index,
               RoutingModel::NodeIndex end_node, int64 end_index,
               RoutingModel::CostClassIndex cost_class_index)
      : start_node(start_node),
        end_node(end_node),
        cost_class_index(cost_class_index),
        start_depot(start_index),
        end_depot(end_index),
        vehicle_class_index(-1) {}

  RoutingModel::NodeIndex start_node;
  RoutingModel::NodeIndex end_node;
  RoutingModel::CostClassIndex cost_class_index;
  int64 start_depot;
  int64 end_depot;
  int64 vehicle_class_index;

  // Comparators.
  static bool Equals(const VehicleClass& a, const VehicleClass& b) {
    return (a.start_node == b.start_node && a.end_node == b.end_node &&
            a.cost_class_index == b.cost_class_index);
  }
  static bool LessThan(const VehicleClass& a, const VehicleClass& b) {
    if (a.start_node != b.start_node) return a.start_node < b.start_node;
    if (a.end_node != b.end_node) return a.end_node < b.end_node;
    return a.cost_class_index < b.cost_class_index;
  }
};

// The RouteConstructor creates the routes of a VRP instance subject to its
// constraints by iterating on a list of arcs appearing in descending order
// of priority.
// TODO(user): Use the dimension class in this class.
// TODO(user): Add support for vehicle-dependent dimension transits.
class RouteConstructor {
 public:
  RouteConstructor(Assignment* const assignment, RoutingModel* const model,
                   bool check_assignment, int64 nodes_number,
                   const std::vector<Link>& links_list,
                   const std::vector<VehicleClass>& vehicle_classes)
      : assignment_(assignment),
        model_(model),
        check_assignment_(check_assignment),
        solver_(model_->solver()),
        nodes_number_(nodes_number),
        links_list_(links_list),
        vehicle_classes_(vehicle_classes),
        nexts_(model_->Nexts()),
        in_route_(nodes_number_, -1),
        final_routes_(),
        node_to_chain_index_(nodes_number, -1),
        node_to_vehicle_class_index_(nodes_number, -1) {
    {
      const std::vector<std::string> dimension_names =
          model_->GetAllDimensionNames();
      dimensions_.assign(dimension_names.size(), nullptr);
      for (int i = 0; i < dimension_names.size(); ++i) {
        dimensions_[i] = &model_->GetDimensionOrDie(dimension_names[i]);
      }
    }
    cumuls_.resize(dimensions_.size());
    for (std::vector<int64>& cumuls : cumuls_) {
      cumuls.resize(nodes_number_);
    }
    new_possible_cumuls_.resize(dimensions_.size());
  }

  ~RouteConstructor() {}

  void Construct() {
    model_->solver()->TopPeriodicCheck();
    // Initial State: Each order is served by its own vehicle.
    for (int node = 0; node < nodes_number_; ++node) {
      if (!model_->IsStart(node) && !model_->IsEnd(node)) {
        std::vector<int> route(1, node);
        routes_.push_back(route);
        in_route_[node] = routes_.size() - 1;
      }
    }

    for (const Link& link : links_list_) {
      model_->solver()->TopPeriodicCheck();
      const int node1 = link.link.first;
      const int node2 = link.link.second;
      const int vehicle_class = link.vehicle_class;
      const int64 start_depot = link.start_depot;
      const int64 end_depot = link.end_depot;

      // Initialisation of cumuls_ if the nodes are encountered for first time
      if (node_to_vehicle_class_index_[node1] < 0) {
        for (int dimension_index = 0; dimension_index < dimensions_.size();
             ++dimension_index) {
          cumuls_[dimension_index][node1] =
              std::max(dimensions_[dimension_index]->GetTransitValue(
                           start_depot, node1, 0),
                       dimensions_[dimension_index]->CumulVar(node1)->Min());
        }
      }
      if (node_to_vehicle_class_index_[node2] < 0) {
        for (int dimension_index = 0; dimension_index < dimensions_.size();
             ++dimension_index) {
          cumuls_[dimension_index][node2] =
              std::max(dimensions_[dimension_index]->GetTransitValue(
                           start_depot, node2, 0),
                       dimensions_[dimension_index]->CumulVar(node2)->Min());
        }
      }

      const int route_index1 = in_route_[node1];
      const int route_index2 = in_route_[node2];
      const bool merge =
          route_index1 >= 0 && route_index2 >= 0 &&
          FeasibleMerge(routes_[route_index1], routes_[route_index2], node1,
                        node2, route_index1, route_index2, vehicle_class,
                        start_depot, end_depot);
      if (Merge(merge, route_index1, route_index2)) {
        node_to_vehicle_class_index_[node1] = vehicle_class;
        node_to_vehicle_class_index_[node2] = vehicle_class;
      }
    }

    model_->solver()->TopPeriodicCheck();
    // Beyond this point not checking limits anymore as the rest of the code is
    // linear and that given we managed to build a solution would be stupid to
    // drop it now.
    for (int chain_index = 0; chain_index < chains_.size(); ++chain_index) {
      if (!ContainsKey(deleted_chains_, chain_index)) {
        final_chains_.push_back(chains_[chain_index]);
      }
    }
    std::sort(final_chains_.begin(), final_chains_.end(), ChainComparator);
    for (int route_index = 0; route_index < routes_.size(); ++route_index) {
      if (!ContainsKey(deleted_routes_, route_index)) {
        final_routes_.push_back(routes_[route_index]);
      }
    }
    std::sort(final_routes_.begin(), final_routes_.end(), RouteComparator);

    const int extra_vehicles = std::max(
        0, static_cast<int>(final_chains_.size()) - model_->vehicles());
    // Bind the Start and End of each chain
    int chain_index = 0;
    for (chain_index = extra_vehicles; chain_index < final_chains_.size();
         ++chain_index) {
      if (chain_index - extra_vehicles >= model_->vehicles()) {
        break;
      }
      const int start = final_chains_[chain_index].head;
      const int end = final_chains_[chain_index].tail;
      assignment_->Add(
          model_->NextVar(model_->Start(chain_index - extra_vehicles)));
      assignment_->SetValue(
          model_->NextVar(model_->Start(chain_index - extra_vehicles)), start);
      assignment_->Add(nexts_[end]);
      assignment_->SetValue(nexts_[end],
                            model_->End(chain_index - extra_vehicles));
    }

    // Create the single order routes
    for (int route_index = 0; route_index < final_routes_.size();
         ++route_index) {
      if (chain_index - extra_vehicles >= model_->vehicles()) {
        break;
      }
      DCHECK_LT(route_index, final_routes_.size());
      const int head = final_routes_[route_index].front();
      const int tail = final_routes_[route_index].back();
      if (head == tail && head < model_->Size()) {
        assignment_->Add(
            model_->NextVar(model_->Start(chain_index - extra_vehicles)));
        assignment_->SetValue(
            model_->NextVar(model_->Start(chain_index - extra_vehicles)), head);
        assignment_->Add(nexts_[tail]);
        assignment_->SetValue(nexts_[tail],
                              model_->End(chain_index - extra_vehicles));
        ++chain_index;
      }
    }

    // Unperformed
    for (int index = 0; index < model_->Size(); ++index) {
      IntVar* const next = nexts_[index];
      if (!assignment_->Contains(next)) {
        assignment_->Add(next);
        if (next->Contains(index)) {
          assignment_->SetValue(next, index);
        }
      }
    }
  }

  const std::vector<std::vector<int>>& final_routes() const {
    return final_routes_;
  }

 private:
  enum MergeStatus { FIRST_SECOND, SECOND_FIRST, NO_MERGE };

  struct RouteSort {
    bool operator()(const std::vector<int>& route1,
                    const std::vector<int>& route2) {
      return (route1.size() < route2.size());
    }
  } RouteComparator;

  struct Chain {
    int head;
    int tail;
    int nodes;
  };

  struct ChainSort {
    bool operator()(const Chain& chain1, const Chain& chain2) const {
      return (chain1.nodes < chain2.nodes);
    }
  } ChainComparator;

  bool Head(int node) const {
    return (node == routes_[in_route_[node]].front());
  }

  bool Tail(int node) const {
    return (node == routes_[in_route_[node]].back());
  }

  bool FeasibleRoute(const std::vector<int>& route, int64 route_cumul,
                     int dimension_index) {
    const RoutingDimension& dimension = *dimensions_[dimension_index];
    std::vector<int>::const_iterator it = route.begin();
    int64 cumul = route_cumul;
    while (it != route.end()) {
      const int previous = *it;
      const int64 cumul_previous = cumul;
      InsertOrDie(&(new_possible_cumuls_[dimension_index]), previous,
                  cumul_previous);
      ++it;
      if (it == route.end()) {
        return true;
      }
      const int next = *it;
      int64 available_from_previous =
          cumul_previous + dimension.GetTransitValue(previous, next, 0);
      int64 available_cumul_next =
          std::max(cumuls_[dimension_index][next], available_from_previous);

      const int64 slack = available_cumul_next - available_from_previous;
      if (slack > dimension.SlackVar(previous)->Max()) {
        available_cumul_next =
            available_from_previous + dimension.SlackVar(previous)->Max();
      }

      if (available_cumul_next > dimension.CumulVar(next)->Max()) {
        return false;
      }
      if (available_cumul_next <= cumuls_[dimension_index][next]) {
        return true;
      }
      cumul = available_cumul_next;
    }
    return true;
  }

  bool CheckRouteConnection(const std::vector<int>& route1,
                            const std::vector<int>& route2, int dimension_index,
                            int64 start_depot, int64 end_depot) {
    const int tail1 = route1.back();
    const int head2 = route2.front();
    const int tail2 = route2.back();
    const RoutingDimension& dimension = *dimensions_[dimension_index];
    int non_depot_node = -1;
    for (int node = 0; node < nodes_number_; ++node) {
      if (!model_->IsStart(node) && !model_->IsEnd(node)) {
        non_depot_node = node;
        break;
      }
    }
    CHECK_GE(non_depot_node, 0);
    const int64 depot_threashold =
        std::max(dimension.SlackVar(non_depot_node)->Max(),
                 dimension.CumulVar(non_depot_node)->Max());

    int64 available_from_tail1 = cumuls_[dimension_index][tail1] +
                                 dimension.GetTransitValue(tail1, head2, 0);
    int64 new_available_cumul_head2 =
        std::max(cumuls_[dimension_index][head2], available_from_tail1);

    const int64 slack = new_available_cumul_head2 - available_from_tail1;
    if (slack > dimension.SlackVar(tail1)->Max()) {
      new_available_cumul_head2 =
          available_from_tail1 + dimension.SlackVar(tail1)->Max();
    }

    bool feasible_route = true;
    if (new_available_cumul_head2 > dimension.CumulVar(head2)->Max()) {
      return false;
    }
    if (new_available_cumul_head2 <= cumuls_[dimension_index][head2]) {
      return true;
    }

    feasible_route =
        FeasibleRoute(route2, new_available_cumul_head2, dimension_index);
    const int64 new_possible_cumul_tail2 =
        ContainsKey(new_possible_cumuls_[dimension_index], tail2)
            ? new_possible_cumuls_[dimension_index][tail2]
            : cumuls_[dimension_index][tail2];

    if (!feasible_route || (new_possible_cumul_tail2 +
                                dimension.GetTransitValue(tail2, end_depot, 0) >
                            depot_threashold)) {
      return false;
    }
    return true;
  }

  bool FeasibleMerge(const std::vector<int>& route1,
                     const std::vector<int>& route2, int node1, int node2,
                     int route_index1, int route_index2, int vehicle_class,
                     int64 start_depot, int64 end_depot) {
    if ((route_index1 == route_index2) || !(Tail(node1) && Head(node2))) {
      return false;
    }

    // Vehicle Class Check
    if (!((node_to_vehicle_class_index_[node1] == -1 &&
           node_to_vehicle_class_index_[node2] == -1) ||
          (node_to_vehicle_class_index_[node1] == vehicle_class &&
           node_to_vehicle_class_index_[node2] == -1) ||
          (node_to_vehicle_class_index_[node1] == -1 &&
           node_to_vehicle_class_index_[node2] == vehicle_class) ||
          (node_to_vehicle_class_index_[node1] == vehicle_class &&
           node_to_vehicle_class_index_[node2] == vehicle_class))) {
      return false;
    }

    // Check Route1 -> Route2 connection for every dimension
    bool merge = true;
    for (int dimension_index = 0; dimension_index < dimensions_.size();
         ++dimension_index) {
      new_possible_cumuls_[dimension_index].clear();
      merge = merge && CheckRouteConnection(route1, route2, dimension_index,
                                            start_depot, end_depot);
      if (!merge) {
        return false;
      }
    }
    return true;
  }

  bool CheckTempAssignment(Assignment* const temp_assignment,
                           int new_chain_index, int old_chain_index, int head1,
                           int tail1, int head2, int tail2) {
    const int start = head1;
    temp_assignment->Add(model_->NextVar(model_->Start(new_chain_index)));
    temp_assignment->SetValue(model_->NextVar(model_->Start(new_chain_index)),
                              start);
    temp_assignment->Add(nexts_[tail1]);
    temp_assignment->SetValue(nexts_[tail1], head2);
    temp_assignment->Add(nexts_[tail2]);
    temp_assignment->SetValue(nexts_[tail2], model_->End(new_chain_index));
    for (int chain_index = 0; chain_index < chains_.size(); ++chain_index) {
      if ((chain_index != new_chain_index) &&
          (chain_index != old_chain_index) &&
          (!ContainsKey(deleted_chains_, chain_index))) {
        const int start = chains_[chain_index].head;
        const int end = chains_[chain_index].tail;
        temp_assignment->Add(model_->NextVar(model_->Start(chain_index)));
        temp_assignment->SetValue(model_->NextVar(model_->Start(chain_index)),
                                  start);
        temp_assignment->Add(nexts_[end]);
        temp_assignment->SetValue(nexts_[end], model_->End(chain_index));
      }
    }
    return solver_->Solve(solver_->MakeRestoreAssignment(temp_assignment));
  }

  bool UpdateAssignment(const std::vector<int>& route1,
                        const std::vector<int>& route2) {
    bool feasible = true;
    const int head1 = route1.front();
    const int tail1 = route1.back();
    const int head2 = route2.front();
    const int tail2 = route2.back();
    const int chain_index1 = node_to_chain_index_[head1];
    const int chain_index2 = node_to_chain_index_[head2];
    if (chain_index1 < 0 && chain_index2 < 0) {
      const int chain_index = chains_.size();
      if (check_assignment_) {
        Assignment* const temp_assignment =
            solver_->MakeAssignment(assignment_);
        feasible = CheckTempAssignment(temp_assignment, chain_index, -1, head1,
                                       tail1, head2, tail2);
      }
      if (feasible) {
        Chain chain;
        chain.head = head1;
        chain.tail = tail2;
        chain.nodes = 2;
        node_to_chain_index_[head1] = chain_index;
        node_to_chain_index_[tail2] = chain_index;
        chains_.push_back(chain);
      }
    } else if (chain_index1 >= 0 && chain_index2 < 0) {
      if (check_assignment_) {
        Assignment* const temp_assignment =
            solver_->MakeAssignment(assignment_);
        feasible =
            CheckTempAssignment(temp_assignment, chain_index1, chain_index2,
                                head1, tail1, head2, tail2);
      }
      if (feasible) {
        node_to_chain_index_[tail2] = chain_index1;
        chains_[chain_index1].head = head1;
        chains_[chain_index1].tail = tail2;
        ++chains_[chain_index1].nodes;
      }
    } else if (chain_index1 < 0 && chain_index2 >= 0) {
      if (check_assignment_) {
        Assignment* const temp_assignment =
            solver_->MakeAssignment(assignment_);
        feasible =
            CheckTempAssignment(temp_assignment, chain_index2, chain_index1,
                                head1, tail1, head2, tail2);
      }
      if (feasible) {
        node_to_chain_index_[head1] = chain_index2;
        chains_[chain_index2].head = head1;
        chains_[chain_index2].tail = tail2;
        ++chains_[chain_index2].nodes;
      }
    } else {
      if (check_assignment_) {
        Assignment* const temp_assignment =
            solver_->MakeAssignment(assignment_);
        feasible =
            CheckTempAssignment(temp_assignment, chain_index1, chain_index2,
                                head1, tail1, head2, tail2);
      }
      if (feasible) {
        node_to_chain_index_[tail2] = chain_index1;
        chains_[chain_index1].head = head1;
        chains_[chain_index1].tail = tail2;
        chains_[chain_index1].nodes += chains_[chain_index2].nodes;
        deleted_chains_.insert(chain_index2);
      }
    }
    if (feasible) {
      assignment_->Add(nexts_[tail1]);
      assignment_->SetValue(nexts_[tail1], head2);
    }
    return feasible;
  }

  bool Merge(bool merge, int index1, int index2) {
    if (merge) {
      if (UpdateAssignment(routes_[index1], routes_[index2])) {
        // Connection Route1 -> Route2
        for (const int node : routes_[index2]) {
          in_route_[node] = index1;
          routes_[index1].push_back(node);
        }
        for (int dimension_index = 0; dimension_index < dimensions_.size();
             ++dimension_index) {
          for (const std::pair<int, int64> new_possible_cumul :
               new_possible_cumuls_[dimension_index]) {
            cumuls_[dimension_index][new_possible_cumul.first] =
                new_possible_cumul.second;
          }
        }
        deleted_routes_.insert(index2);
        return true;
      }
    }
    return false;
  }

  Assignment* const assignment_;
  RoutingModel* const model_;
  const bool check_assignment_;
  Solver* const solver_;
  const int64 nodes_number_;
  const std::vector<Link> links_list_;
  const std::vector<VehicleClass> vehicle_classes_;
  std::vector<IntVar*> nexts_;
  std::vector<const RoutingDimension*> dimensions_;  // Not owned.
  std::vector<std::vector<int64>> cumuls_;
  std::vector<std::unordered_map<int, int64>> new_possible_cumuls_;
  std::vector<std::vector<int>> routes_;
  std::vector<int> in_route_;
  std::unordered_set<int> deleted_routes_;
  std::vector<std::vector<int>> final_routes_;
  std::vector<Chain> chains_;
  std::unordered_set<int> deleted_chains_;
  std::vector<Chain> final_chains_;
  std::vector<int> node_to_chain_index_;
  std::vector<int> node_to_vehicle_class_index_;
};


void GetVehicleClasses(const RoutingModel& model,
                       std::vector<VehicleClass>* vehicle_classes) {
  vehicle_classes->clear();
  vehicle_classes->reserve(model.vehicles());
  for (int vehicle = 0; vehicle < model.vehicles(); ++vehicle) {
    const int64 start_index = model.Start(vehicle);
    const int64 end_index = model.End(vehicle);
    vehicle_classes->push_back(
        VehicleClass(model.IndexToNode(start_index), start_index,
                     model.IndexToNode(end_index), end_index,
                     model.GetCostClassIndexOfVehicle(vehicle)));
  }
  std::sort(vehicle_classes->begin(), vehicle_classes->end(),
            &VehicleClass::LessThan);
  vehicle_classes->erase(
      std::unique(vehicle_classes->begin(), vehicle_classes->end(),
                  &VehicleClass::Equals),
      vehicle_classes->end());
  // Populate the vehicle_class_index.
  for (int i = 0; i < vehicle_classes->size(); ++i) {
    (*vehicle_classes)[i].vehicle_class_index = i;
  }
}

// Desicion Builder building a first solution based on Savings (Clarke & Wright)
// heuristic for Vehicle Routing Problem.
class SavingsBuilder : public DecisionBuilder {
 public:
  SavingsBuilder(RoutingModel* const model, bool check_assignment)
      : model_(model), check_assignment_(check_assignment) {}
  ~SavingsBuilder() override {}

  Decision* Next(Solver* const solver) override {
    // Setup the model of the instance for the Savings Algorithm
    ModelSetup();

    // Create the Savings List
    CreateSavingsList();

    // Build the assignment routes for the model
    Assignment* const assignment = solver->MakeAssignment();
    route_constructor_.reset(
        new RouteConstructor(assignment, model_, check_assignment_,
                             nodes_number_, savings_list_, vehicle_classes_));
    // This call might cause backtracking if the search limit is reached.
    route_constructor_->Construct();
    route_constructor_.reset(nullptr);
    // This call might cause backtracking if the solution is not feasible.
    assignment->Restore();

    return nullptr;
  }

 private:
  void ModelSetup() {
    nodes_number_ = model_->Size() + model_->vehicles();
    neighbors_.resize(nodes_number_);
    route_shape_parameter_ = FLAGS_savings_route_shape_parameter;

    int64 savings_filter_neighbors = FLAGS_savings_filter_neighbors;
    int64 savings_filter_radius = FLAGS_savings_filter_radius;
    if (!savings_filter_neighbors && !savings_filter_radius) {
      savings_filter_neighbors = model_->nodes();
      savings_filter_radius = -1;
    }

    // For each node consider as neighbors the nearest nodes.
    {
      for (int node = 0; node < nodes_number_; ++node) {
        model_->solver()->TopPeriodicCheck();
        neighbors_[node].reserve(nodes_number_);
        for (int neighbor = 0; neighbor < nodes_number_; ++neighbor) {
          if (model_->HasIndex(model_->IndexToNode(neighbor))) {
            neighbors_[node].push_back(neighbor);
          }
        }
      }
    }

    // Setting Up Costs
    for (int node = 0; node < nodes_number_; ++node) {
      model_->solver()->TopPeriodicCheck();
      std::vector<int64> costs_from_node(nodes_number_);
      for (const int neighbor : neighbors_[node]) {
        // TODO(user): Take vehicle class into account here.
        const int64 cost = model_->GetHomogeneousCost(node, neighbor);
        costs_from_node[neighbor] = cost;
      }
      costs_.push_back(costs_from_node);
    }

    // Find the different vehicle classes
    GetVehicleClasses(*model_, &vehicle_classes_);
  }

  void CreateSavingsList() {
    for (const VehicleClass& vehicle_class : vehicle_classes_) {
      int64 start_depot = vehicle_class.start_depot;
      int64 end_depot = vehicle_class.end_depot;
      int vehicle_class_index = vehicle_class.vehicle_class_index;
      for (int node = 0; node < nodes_number_; ++node) {
        model_->solver()->TopPeriodicCheck();
        for (const int neighbor : neighbors_[node]) {
          if (node != start_depot && node != end_depot &&
              neighbor != start_depot && neighbor != end_depot &&
              node != neighbor) {
            const double saving =
                costs_[node][start_depot] + costs_[end_depot][neighbor] -
                route_shape_parameter_ * costs_[node][neighbor];
            Link link(std::make_pair(node, neighbor), saving,
                      vehicle_class_index, start_depot, end_depot);
            savings_list_.push_back(link);
          }
        }
      }
      std::stable_sort(savings_list_.begin(), savings_list_.end(),
                       LinkComparator);
    }
  }

  RoutingModel* const model_;
  std::unique_ptr<RouteConstructor> route_constructor_;
  const bool check_assignment_;
  std::vector<std::string> dimensions_;
  int64 nodes_number_;
  std::vector<std::vector<int64>> costs_;
  std::vector<std::vector<int>> neighbors_;
  std::vector<Link> savings_list_;
  double route_shape_parameter_;
  std::vector<VehicleClass> vehicle_classes_;
};

#ifndef SWIG
struct SweepNode {
  SweepNode(const RoutingModel::NodeIndex node, const double angle,
            const double distance)
      : node(node), angle(angle), distance(distance) {}
  ~SweepNode() {}

  RoutingModel::NodeIndex node;
  double angle;
  double distance;
};

struct SweepNodeSortAngle {
  bool operator()(const SweepNode& node1, const SweepNode& node2) const {
    return (node1.angle < node2.angle);
  }
} SweepNodeAngleComparator;

struct SweepNodeSortDistance {
  bool operator()(const SweepNode& node1, const SweepNode& node2) const {
    return (node1.distance < node2.distance);
  }
} SweepNodeDistanceComparator;

SweepArranger::SweepArranger(
    const ITIVector<RoutingModel::NodeIndex, std::pair<int64, int64>>& points)
    : coordinates_(2 * points.size(), 0), sectors_(1) {
  for (RoutingModel::NodeIndex i(0); i < points.size(); ++i) {
    coordinates_[2 * i] = points[i].first;
    coordinates_[2 * i + 1] = points[i].second;
  }
}

// Splits the space of the nodes into sectors and sorts the nodes of each
// sector with ascending angle from the depot.
void SweepArranger::ArrangeNodes(std::vector<RoutingModel::NodeIndex>* nodes) {
  const double pi_rad = 3.14159265;
  // Suppose that the center is at x0, y0.
  const int x0 = coordinates_[RoutingModel::NodeIndex(0)];
  const int y0 = coordinates_[RoutingModel::NodeIndex(1)];

  std::vector<SweepNode> sweep_nodes;
  for (RoutingModel::NodeIndex node(0);
       node < static_cast<int>(coordinates_.size()) / 2; ++node) {
    const int x = coordinates_[2 * node];
    const int y = coordinates_[2 * node + 1];
    const double x_delta = x - x0;
    const double y_delta = y - y0;
    double square_distance = x_delta * x_delta + y_delta * y_delta;
    double angle = square_distance == 0 ? 0 : std::atan2(y_delta, x_delta);
    angle = angle >= 0 ? angle : 2 * pi_rad + angle;
    SweepNode sweep_node(node, angle, square_distance);
    sweep_nodes.push_back(sweep_node);
  }
  std::sort(sweep_nodes.begin(), sweep_nodes.end(),
            SweepNodeDistanceComparator);

  const int size = static_cast<int>(sweep_nodes.size()) / sectors_;
  for (int sector = 0; sector < sectors_; ++sector) {
    std::vector<SweepNode> cluster;
    std::vector<SweepNode>::iterator begin =
        sweep_nodes.begin() + sector * size;
    std::vector<SweepNode>::iterator end =
        sector == sectors_ - 1 ? sweep_nodes.end()
                               : sweep_nodes.begin() + (sector + 1) * size;
    std::sort(begin, end, SweepNodeAngleComparator);
  }
  for (const SweepNode& sweep_node : sweep_nodes) {
    nodes->push_back(sweep_node.node);
  }
}

// Decision Builder building a first solution based on Sweep heuristic for
// Vehicle Routing Problem.
// Suitable only when distance is considered as the cost.
class SweepBuilder : public DecisionBuilder {
 public:
  SweepBuilder(RoutingModel* const model, bool check_assignment)
      : model_(model), check_assignment_(check_assignment) {}
  ~SweepBuilder() override {}

  Decision* Next(Solver* const solver) override {
    // Setup the model of the instance for the Sweep Algorithm
    ModelSetup();

    // Build the assignment routes for the model
    Assignment* const assignment = solver->MakeAssignment();
    route_constructor_.reset(
        new RouteConstructor(assignment, model_, check_assignment_,
                             nodes_number_, links_, vehicle_classes_));
    // This call might cause backtracking if the search limit is reached.
    route_constructor_->Construct();
    route_constructor_.reset(nullptr);
    // This call might cause backtracking if the solution is not feasible.
    assignment->Restore();

    return nullptr;
  }

 private:
  void ModelSetup() {
    const int depot = model_->GetDepot();
    nodes_number_ = model_->nodes();
    if (FLAGS_sweep_sectors > 0 && FLAGS_sweep_sectors < nodes_number_) {
      model_->sweep_arranger()->SetSectors(FLAGS_sweep_sectors);
    }
    std::vector<RoutingModel::NodeIndex> nodes;
    model_->sweep_arranger()->ArrangeNodes(&nodes);
    for (int i = 0; i < nodes.size() - 1; ++i) {
      const RoutingModel::NodeIndex first = nodes[i];
      const RoutingModel::NodeIndex second = nodes[i + 1];
      if (model_->HasIndex(first) && model_->HasIndex(second)) {
        const int64 first_index = model_->NodeToIndex(first);
        const int64 second_index = model_->NodeToIndex(second);
        if (first_index != depot && second_index != depot) {
          Link link(std::make_pair(first_index, second_index), 0, 0, depot,
                    depot);
          links_.push_back(link);
        }
      }
    }
  }

  RoutingModel* const model_;
  std::unique_ptr<RouteConstructor> route_constructor_;
  const bool check_assignment_;
  int64 nodes_number_;
  std::vector<Link> links_;
  std::vector<VehicleClass> vehicle_classes_;
};
#endif

// Decision builder building a solution with a single path without propagating.
// Is very fast but has a very high probability of failing if the problem
// contains other constraints than path-related constraints.
// Based on an addition heuristics extending a path from its start node with
// the cheapest arc according to an evaluator.
// TODO(user): Deprecate this in favor of
// CheapestAdditionFilteredDecisionBuilder if performance is similar.
class FastOnePathBuilder : public DecisionBuilder {
 public:
  // Takes ownership of evaluator.
  FastOnePathBuilder(RoutingModel* const model,
                     ResultCallback2<int64, int64, int64>* evaluator)
      : model_(model), evaluator_(evaluator) {
    CHECK(evaluator_->IsRepeatable());
  }
  ~FastOnePathBuilder() override {}
  Decision* Next(Solver* const solver) override {
    int64 index = -1;
    if (!FindPathStart(&index)) {
      return nullptr;
    }
    IntVar* const* nexts = model_->Nexts().data();
    // Need to allocate in a reversible way so that if restoring the assignment
    // fails, the assignment gets de-allocated.
    Assignment* const assignment = solver->MakeAssignment();
    Assignment::IntContainer* const container =
        assignment->MutableIntVarContainer();
    added_.assign(model_->Size(), false);
    int64 next = FindCheapestValue(index);
    while (next >= 0) {
      added_[index] = true;
      container->FastAdd(nexts[index])->SetValue(next);
      index = next;
      model_->ForEachNodeInDisjunctionWithMaxCardinalityFromIndex(
          index, 1, [this, index, container, nexts](int alternate) {
            if (index != alternate) {
              added_[alternate] = true;
              container->FastAdd(nexts[alternate])->SetValue(alternate);
            }
          });
      next = FindCheapestValue(index);
    }
    // Make unassigned nexts loop to themselves.
    // TODO(user): Make finalization more robust, might have some next
    // variables non-instantiated.
    for (int index = 0; index < model_->Size(); ++index) {
      if (!added_[index]) {
        added_[index] = true;
        IntVar* const next = nexts[index];
        IntVarElement* const element = container->FastAdd(next);
        if (next->Contains(index)) {
          element->SetValue(index);
        }
      }
    }
    assignment->Restore();
    return nullptr;
  }

 private:
  bool FindPathStart(int64* index) const {
    IntVar* const* nexts = model_->Nexts().data();
    const int size = model_->Size();
    // Try to extend an existing path
    for (int i = size - 1; i >= 0; --i) {
      if (nexts[i]->Bound()) {
        const int next = nexts[i]->Value();
        if (next < size && !nexts[next]->Bound()) {
          *index = next;
          return true;
        }
      }
    }
    // Pick path start
    for (int i = size - 1; i >= 0; --i) {
      if (!nexts[i]->Bound()) {
        bool has_possible_prev = false;
        for (int j = 0; j < size; ++j) {
          if (nexts[j]->Contains(i)) {
            has_possible_prev = true;
            break;
          }
        }
        if (!has_possible_prev) {
          *index = i;
          return true;
        }
      }
    }
    // Pick first unbound
    for (int i = 0; i < size; ++i) {
      if (!nexts[i]->Bound()) {
        *index = i;
        return true;
      }
    }
    return false;
  }

  int64 FindCheapestValue(int index) const {
    IntVar* const* nexts = model_->Nexts().data();
    const int size = model_->Size();
    int64 best_evaluation = kint64max;
    int64 best_value = -1;
    if (index < size) {
      IntVar* const next = nexts[index];
      std::unique_ptr<IntVarIterator> it(next->MakeDomainIterator(false));
      for (const int64 value : InitAndGetValues(it.get())) {
        if (value != index && (value >= size || !added_[value])) {
          const int64 evaluation = evaluator_->Run(index, value);
          if (evaluation <= best_evaluation) {
            best_evaluation = evaluation;
            best_value = value;
          }
        }
      }
    }
    return best_value;
  }

  RoutingModel* const model_;
  // added_[node] is true if node had been added to the solution.
  std::vector<bool> added_;
  std::unique_ptr<ResultCallback2<int64, int64, int64>> evaluator_;
};

// Decision builder to build a solution with all nodes inactive. It does no
// branching and may fail if some nodes cannot be made inactive.

class AllUnperformed : public DecisionBuilder {
 public:
  // Does not take ownership of model.
  explicit AllUnperformed(RoutingModel* const model) : model_(model) {}
  ~AllUnperformed() override {}
  Decision* Next(Solver* const solver) override {
    // Solver::(Un)FreezeQueue is private, passing through the public API
    // on PropagationBaseObject.
    model_->CostVar()->FreezeQueue();
    for (int i = 0; i < model_->Size(); ++i) {
      if (!model_->IsStart(i)) {
        model_->ActiveVar(i)->SetValue(0);
      }
    }
    model_->CostVar()->UnfreezeQueue();
    return nullptr;
  }

 private:
  RoutingModel* const model_;
};

void RoutingModel::AddSearchMonitor(SearchMonitor* const monitor) {
  monitors_.push_back(monitor);
}

namespace {
class AtSolutionCallbackMonitor : public SearchMonitor {
 public:
  AtSolutionCallbackMonitor(Solver* solver, std::function<void()> callback)
      : SearchMonitor(solver), callback_(std::move(callback)) {}
  bool AtSolution() override {
    callback_();
    return false;
  }

 private:
  std::function<void()> callback_;
};
}  // namespace

void RoutingModel::AddAtSolutionCallback(std::function<void()> callback) {
  AddSearchMonitor(solver_->RevAlloc(
      new AtSolutionCallbackMonitor(solver_.get(), std::move(callback))));
}

const Assignment* RoutingModel::Solve(const Assignment* assignment) {
  return SolveFromAssignmentWithParameters(assignment,
                                           DefaultSearchParameters());
}

const Assignment* RoutingModel::SolveWithParameters(
    const RoutingSearchParameters& parameters) {
  return SolveFromAssignmentWithParameters(nullptr, parameters);
}

const Assignment* RoutingModel::SolveFromAssignmentWithParameters(
    const Assignment* assignment, const RoutingSearchParameters& parameters) {
  QuietCloseModelWithParameters(parameters);
  VLOG(1) << "Search parameters:\n" << parameters.DebugString();
  if (status_ == ROUTING_INVALID) {
    return nullptr;
  }
  solver_->UpdateLimits(parameters.time_limit_ms(), kint64max, kint64max,
                        parameters.solution_limit(), limit_);
  solver_->UpdateLimits(parameters.time_limit_ms(), kint64max, kint64max, 1,
                        ls_limit_);
  solver_->UpdateLimits(parameters.lns_time_limit_ms(), kint64max, kint64max,
                        kint64max, lns_limit_);
  const int64 start_time_ms = solver_->wall_time();
  if (nullptr == assignment) {
    solver_->Solve(solve_db_, monitors_);
  } else {
    assignment_->Copy(assignment);
    solver_->Solve(improve_db_, monitors_);
  }
  const int64 elapsed_time_ms = solver_->wall_time() - start_time_ms;
  if (collect_assignments_->solution_count() == 1) {
    status_ = ROUTING_SUCCESS;
    return collect_assignments_->solution(0);
  } else {
    if (elapsed_time_ms >= parameters.time_limit_ms()) {
      status_ = ROUTING_FAIL_TIMEOUT;
    } else {
      status_ = ROUTING_FAIL;
    }
    return nullptr;
  }
}

// Computing a lower bound to the cost of a vehicle routing problem solving a
// a linear assignment problem (minimum-cost perfect bipartite matching).
// A bipartite graph is created with left nodes representing the nodes of the
// routing problem and right nodes representing possible node successors; an
// arc between a left node l and a right node r is created if r can be the
// node folowing l in a route (Next(l) = r); the cost of the arc is the transit
// cost between l and r in the routing problem.
// This is a lower bound given the solution to assignment problem does not
// necessarily produce a (set of) closed route(s) from a starting node to an
// ending node.
int64 RoutingModel::ComputeLowerBound() {
  if (!closed_) {
    LOG(WARNING) << "Non-closed model not supported.";
    return 0;
  }
  if (!CostsAreHomogeneousAcrossVehicles()) {
    LOG(WARNING) << "Non-homogeneous vehicle costs not supported";
    return 0;
  }
  if (disjunctions_.size() > 0) {
    LOG(WARNING)
        << "Node disjunction constraints or optional nodes not supported.";
    return 0;
  }
  const int num_nodes = Size() + vehicles_;
  ForwardStarGraph graph(2 * num_nodes, num_nodes * num_nodes);
  LinearSumAssignment<ForwardStarGraph> linear_sum_assignment(graph, num_nodes);
  // Adding arcs for non-end nodes, based on possible values of next variables.
  // Left nodes in the bipartite are indexed from 0 to num_nodes - 1; right
  // nodes are indexed from num_nodes to 2 * num_nodes - 1.
  for (int tail = 0; tail < Size(); ++tail) {
    std::unique_ptr<IntVarIterator> iterator(
        nexts_[tail]->MakeDomainIterator(false));
    for (const int64 head : InitAndGetValues(iterator.get())) {
      // Given there are no disjunction constraints, a node cannot point to
      // itself. Doing this explicitely given that outside the search,
      // propagation hasn't removed this value from next variables yet.
      if (head == tail) {
        continue;
      }
      // The index of a right node in the bipartite graph is the index
      // of the successor offset by the number of nodes.
      const ArcIndex arc = graph.AddArc(tail, num_nodes + head);
      const CostValue cost = GetHomogeneousCost(tail, head);
      linear_sum_assignment.SetArcCost(arc, cost);
    }
  }
  // The linear assignment library requires having as many left and right nodes.
  // Therefore we are creating fake assignments for end nodes, forced to point
  // to the equivalent start node with a cost of 0.
  for (int tail = Size(); tail < num_nodes; ++tail) {
    const ArcIndex arc = graph.AddArc(tail, num_nodes + starts_[tail - Size()]);
    linear_sum_assignment.SetArcCost(arc, 0);
  }
  if (linear_sum_assignment.ComputeAssignment()) {
    return linear_sum_assignment.GetCost();
  }
  return 0;
}

bool RoutingModel::RouteCanBeUsedByVehicle(const Assignment& assignment,
                                           int start_index, int vehicle) const {
  int current_index =
      IsStart(start_index) ? Next(assignment, start_index) : start_index;
  while (!IsEnd(current_index)) {
    const IntVar* const vehicle_var = VehicleVar(current_index);
    if (!vehicle_var->Contains(vehicle)) {
      return false;
    }
    const int next_index = Next(assignment, current_index);
    CHECK_NE(next_index, current_index) << "Inactive node inside a route";
    current_index = next_index;
  }
  return true;
}

bool RoutingModel::ReplaceUnusedVehicle(
    int unused_vehicle, int active_vehicle,
    Assignment* const compact_assignment) const {
  CHECK(compact_assignment != nullptr);
  CHECK(!IsVehicleUsed(*compact_assignment, unused_vehicle));
  CHECK(IsVehicleUsed(*compact_assignment, active_vehicle));
  // Swap NextVars at start nodes.
  const int unused_vehicle_start = Start(unused_vehicle);
  IntVar* const unused_vehicle_start_var = NextVar(unused_vehicle_start);
  const int unused_vehicle_end = End(unused_vehicle);
  const int active_vehicle_start = Start(active_vehicle);
  const int active_vehicle_end = End(active_vehicle);
  IntVar* const active_vehicle_start_var = NextVar(active_vehicle_start);
  const int active_vehicle_next =
      compact_assignment->Value(active_vehicle_start_var);
  compact_assignment->SetValue(unused_vehicle_start_var, active_vehicle_next);
  compact_assignment->SetValue(active_vehicle_start_var, End(active_vehicle));

  // Update VehicleVars along the route, update the last NextVar.
  int current_index = active_vehicle_next;
  while (!IsEnd(current_index)) {
    IntVar* const vehicle_var = VehicleVar(current_index);
    compact_assignment->SetValue(vehicle_var, unused_vehicle);
    const int next_index = Next(*compact_assignment, current_index);
    if (IsEnd(next_index)) {
      IntVar* const last_next_var = NextVar(current_index);
      compact_assignment->SetValue(last_next_var, End(unused_vehicle));
    }
    current_index = next_index;
  }

  // Update dimensions: update transits at the start.
  for (const RoutingDimension* const dimension : dimensions_) {
    const std::vector<IntVar*>& transit_variables = dimension->transits();
    IntVar* const unused_vehicle_transit_var =
        transit_variables[unused_vehicle_start];
    IntVar* const active_vehicle_transit_var =
        transit_variables[active_vehicle_start];
    const bool contains_unused_vehicle_transit_var =
        compact_assignment->Contains(unused_vehicle_transit_var);
    const bool contains_active_vehicle_transit_var =
        compact_assignment->Contains(active_vehicle_transit_var);
    if (contains_unused_vehicle_transit_var !=
        contains_active_vehicle_transit_var) {
      // TODO(user): clarify the expected trigger rate of this LOG.
      LOG(INFO) << "The assignment contains transit variable for dimension '"
                << dimension->name() << "' for some vehicles, but not for all";
      return false;
    }
    if (contains_unused_vehicle_transit_var) {
      const int64 old_unused_vehicle_transit =
          compact_assignment->Value(unused_vehicle_transit_var);
      const int64 old_active_vehicle_transit =
          compact_assignment->Value(active_vehicle_transit_var);
      compact_assignment->SetValue(unused_vehicle_transit_var,
                                   old_active_vehicle_transit);
      compact_assignment->SetValue(active_vehicle_transit_var,
                                   old_unused_vehicle_transit);
    }

    // Update dimensions: update cumuls at the end.
    const std::vector<IntVar*>& cumul_variables = dimension->cumuls();
    IntVar* const unused_vehicle_cumul_var =
        cumul_variables[unused_vehicle_end];
    IntVar* const active_vehicle_cumul_var =
        cumul_variables[active_vehicle_end];
    const int64 old_unused_vehicle_cumul =
        compact_assignment->Value(unused_vehicle_cumul_var);
    const int64 old_active_vehicle_cumul =
        compact_assignment->Value(active_vehicle_cumul_var);
    compact_assignment->SetValue(unused_vehicle_cumul_var,
                                 old_active_vehicle_cumul);
    compact_assignment->SetValue(active_vehicle_cumul_var,
                                 old_unused_vehicle_cumul);
  }
  return true;
}

Assignment* RoutingModel::CompactAssignment(
    const Assignment& assignment) const {
  return CompactAssignmentInternal(assignment, false);
}

Assignment* RoutingModel::CompactAndCheckAssignment(
    const Assignment& assignment) const {
  return CompactAssignmentInternal(assignment, true);
}

Assignment* RoutingModel::CompactAssignmentInternal(
    const Assignment& assignment, bool check_compact_assignment) const {
  CHECK_EQ(assignment.solver(), solver_.get());
  if (!CostsAreHomogeneousAcrossVehicles()) {
    LOG(WARNING)
        << "The costs are not homogeneous, routes cannot be rearranged";
    return nullptr;
  }

  std::unique_ptr<Assignment> compact_assignment(new Assignment(&assignment));
  for (int vehicle = 0; vehicle < vehicles_ - 1; ++vehicle) {
    if (IsVehicleUsed(*compact_assignment, vehicle)) {
      continue;
    }
    const int vehicle_start = Start(vehicle);
    const int vehicle_end = End(vehicle);
    // Find the last vehicle, that can swap routes with this one.
    int swap_vehicle = vehicles_ - 1;
    bool has_more_vehicles_with_route = false;
    for (; swap_vehicle > vehicle; --swap_vehicle) {
      // If a vehicle was already swapped, it will appear in compact_assignment
      // as unused.
      if (!IsVehicleUsed(*compact_assignment, swap_vehicle) ||
          !IsVehicleUsed(*compact_assignment, swap_vehicle)) {
        continue;
      }
      has_more_vehicles_with_route = true;
      const int swap_vehicle_start = Start(swap_vehicle);
      const int swap_vehicle_end = End(swap_vehicle);
      if (IndexToNode(vehicle_start) != IndexToNode(swap_vehicle_start) ||
          IndexToNode(vehicle_end) != IndexToNode(swap_vehicle_end)) {
        continue;
      }

      // Check that updating VehicleVars is OK.
      if (RouteCanBeUsedByVehicle(*compact_assignment, swap_vehicle_start,
                                  vehicle)) {
        break;
      }
    }

    if (swap_vehicle == vehicle) {
      if (has_more_vehicles_with_route) {
        // No route can be assigned to this vehicle, but there are more vehicles
        // with a route left. This would leave a gap in the indices.
        // TODO(user): clarify the expected trigger rate of this LOG.
        LOG(INFO) << "No vehicle that can be swapped with " << vehicle
                  << " was found";
        return nullptr;
      } else {
        break;
      }
    } else {
      if (!ReplaceUnusedVehicle(vehicle, swap_vehicle,
                                compact_assignment.get())) {
        return nullptr;
      }
    }
  }
  if (check_compact_assignment &&
      !solver_->CheckAssignment(compact_assignment.get())) {
    // TODO(user): clarify the expected trigger rate of this LOG.
    LOG(WARNING) << "The compacted assignment is not a valid solution";
    return nullptr;
  }
  return compact_assignment.release();
}

int RoutingModel::FindNextActive(int index,
                                 const std::vector<int>& nodes) const {
  ++index;
  CHECK_LE(0, index);
  const int size = nodes.size();
  while (index < size && ActiveVar(nodes[index])->Max() == 0) {
    ++index;
  }
  return index;
}

IntVar* RoutingModel::ApplyLocks(const std::vector<int>& locks) {
  // TODO(user): Replace calls to this method with calls to
  // ApplyLocksToAllVehicles and remove this method?
  CHECK_EQ(vehicles_, 1);
  preassignment_->Clear();
  IntVar* next_var = nullptr;
  int lock_index = FindNextActive(-1, locks);
  const int size = locks.size();
  if (lock_index < size) {
    next_var = NextVar(locks[lock_index]);
    preassignment_->Add(next_var);
    for (lock_index = FindNextActive(lock_index, locks); lock_index < size;
         lock_index = FindNextActive(lock_index, locks)) {
      preassignment_->SetValue(next_var, locks[lock_index]);
      next_var = NextVar(locks[lock_index]);
      preassignment_->Add(next_var);
    }
  }
  return next_var;
}

bool RoutingModel::ApplyLocksToAllVehicles(
    const std::vector<std::vector<NodeIndex>>& locks, bool close_routes) {
  preassignment_->Clear();
  return RoutesToAssignment(locks, true, close_routes, preassignment_);
}

int64 RoutingModel::GetNumberOfDecisionsInFirstSolution(
    const RoutingSearchParameters& parameters) const {
  IntVarFilteredDecisionBuilder* const decision_builder =
      GetFilteredFirstSolutionDecisionBuilderOrNull(parameters);
  return decision_builder != nullptr ? decision_builder->number_of_decisions()
                                     : 0;
}

int64 RoutingModel::GetNumberOfRejectsInFirstSolution(
    const RoutingSearchParameters& parameters) const {
  IntVarFilteredDecisionBuilder* const decision_builder =
      GetFilteredFirstSolutionDecisionBuilderOrNull(parameters);
  return decision_builder != nullptr ? decision_builder->number_of_rejects()
                                     : 0;
}

bool RoutingModel::WriteAssignment(const std::string& file_name) const {
  if (collect_assignments_->solution_count() == 1 && assignment_ != nullptr) {
    assignment_->Copy(collect_assignments_->solution(0));
    return assignment_->Save(file_name);
  } else {
    return false;
  }
}

Assignment* RoutingModel::ReadAssignment(const std::string& file_name) {
  QuietCloseModel();
  CHECK(assignment_ != nullptr);
  if (assignment_->Load(file_name)) {
    return DoRestoreAssignment();
  }
  return nullptr;
}

Assignment* RoutingModel::RestoreAssignment(const Assignment& solution) {
  QuietCloseModel();
  CHECK(assignment_ != nullptr);
  assignment_->Copy(&solution);
  return DoRestoreAssignment();
}

Assignment* RoutingModel::DoRestoreAssignment() {
  if (status_ == ROUTING_INVALID) {
    return nullptr;
  }
  solver_->Solve(restore_assignment_, monitors_);
  if (collect_assignments_->solution_count() == 1) {
    status_ = ROUTING_SUCCESS;
    return collect_assignments_->solution(0);
  } else {
    status_ = ROUTING_FAIL;
    return nullptr;
  }
  return nullptr;
}

bool RoutingModel::RoutesToAssignment(
    const std::vector<std::vector<NodeIndex>>& routes,
    bool ignore_inactive_nodes, bool close_routes,
    Assignment* const assignment) const {
  CHECK(assignment != nullptr);
  if (!closed_) {
    LOG(ERROR) << "The model is not closed yet";
    return false;
  }
  const int num_routes = routes.size();
  if (num_routes > vehicles_) {
    LOG(ERROR) << "The number of vehicles in the assignment (" << routes.size()
               << ") is greater than the number of vehicles in the model ("
               << vehicles_ << ")";
    return false;
  }

  std::unordered_set<int> visited_indices;
  // Set value to NextVars based on the routes.
  for (int vehicle = 0; vehicle < num_routes; ++vehicle) {
    const std::vector<NodeIndex>& route = routes[vehicle];
    int from_index = Start(vehicle);
    std::pair<std::unordered_set<int>::iterator, bool> insert_result =
        visited_indices.insert(from_index);
    if (!insert_result.second) {
      LOG(ERROR) << "Index " << from_index << " (start node for vehicle "
                 << vehicle << ") was already used";
      return false;
    }

    for (const NodeIndex to_node : route) {
      if (to_node < 0 || to_node >= nodes()) {
        LOG(ERROR) << "Invalid node index: " << to_node;
        return false;
      }
      const int to_index = NodeToIndex(to_node);
      if (to_index < 0 || to_index >= Size()) {
        LOG(ERROR) << "Invalid index: " << to_index << " from node " << to_node;
        return false;
      }

      IntVar* const active_var = ActiveVar(to_index);
      if (active_var->Max() == 0) {
        if (ignore_inactive_nodes) {
          continue;
        } else {
          LOG(ERROR) << "Index " << to_index << " (node " << to_node
                     << ") is not active";
          return false;
        }
      }

      insert_result = visited_indices.insert(to_index);
      if (!insert_result.second) {
        LOG(ERROR) << "Index " << to_index << " (node " << to_node
                   << ") is used multiple times";
        return false;
      }

      const IntVar* const vehicle_var = VehicleVar(to_index);
      if (!vehicle_var->Contains(vehicle)) {
        LOG(ERROR) << "Vehicle " << vehicle << " is not allowed at index "
                   << to_index << " (node " << to_node << ")";
        return false;
      }

      IntVar* const from_var = NextVar(from_index);
      if (!assignment->Contains(from_var)) {
        assignment->Add(from_var);
      }
      assignment->SetValue(from_var, to_index);

      from_index = to_index;
    }

    if (close_routes) {
      IntVar* const last_var = NextVar(from_index);
      if (!assignment->Contains(last_var)) {
        assignment->Add(last_var);
      }
      assignment->SetValue(last_var, End(vehicle));
    }
  }

  // Do not use the remaining vehicles.
  for (int vehicle = num_routes; vehicle < vehicles_; ++vehicle) {
    const int start_index = Start(vehicle);
    // Even if close_routes is false, we still need to add the start index to
    // visited_indices so that deactivating other nodes works correctly.
    std::pair<std::unordered_set<int>::iterator, bool> insert_result =
        visited_indices.insert(start_index);
    if (!insert_result.second) {
      LOG(ERROR) << "Index " << start_index << " is used multiple times";
      return false;
    }
    if (close_routes) {
      IntVar* const start_var = NextVar(start_index);
      if (!assignment->Contains(start_var)) {
        assignment->Add(start_var);
      }
      assignment->SetValue(start_var, End(vehicle));
    }
  }

  // Deactivate other nodes (by pointing them to themselves).
  if (close_routes) {
    for (int index = 0; index < Size(); ++index) {
      if (!ContainsKey(visited_indices, index)) {
        IntVar* const next_var = NextVar(index);
        if (!assignment->Contains(next_var)) {
          assignment->Add(next_var);
        }
        assignment->SetValue(next_var, index);
      }
    }
  }

  return true;
}

Assignment* RoutingModel::ReadAssignmentFromRoutes(
    const std::vector<std::vector<NodeIndex>>& routes,
    bool ignore_inactive_nodes) {
  QuietCloseModel();
  if (!RoutesToAssignment(routes, ignore_inactive_nodes, true, assignment_)) {
    return nullptr;
  }
  // DoRestoreAssignment() might still fail when checking constraints (most
  // constraints are not verified by RoutesToAssignment) or when filling in
  // dimension variables.
  return DoRestoreAssignment();
}

void RoutingModel::AssignmentToRoutes(
    const Assignment& assignment,
    std::vector<std::vector<NodeIndex>>* const routes) const {
  CHECK(closed_);
  CHECK(routes != nullptr);

  const int model_size = Size();
  routes->resize(vehicles_);
  for (int vehicle = 0; vehicle < vehicles_; ++vehicle) {
    std::vector<NodeIndex>* const vehicle_route = &routes->at(vehicle);
    vehicle_route->clear();

    int num_visited_nodes = 0;
    const int first_index = Start(vehicle);
    const IntVar* const first_var = NextVar(first_index);
    CHECK(assignment.Contains(first_var));
    CHECK(assignment.Bound(first_var));
    int current_index = assignment.Value(first_var);
    while (!IsEnd(current_index)) {
      vehicle_route->push_back(IndexToNode(current_index));

      const IntVar* const next_var = NextVar(current_index);
      CHECK(assignment.Contains(next_var));
      CHECK(assignment.Bound(next_var));
      current_index = assignment.Value(next_var);

      ++num_visited_nodes;
      CHECK_LE(num_visited_nodes, model_size)
          << "The assignment contains a cycle";
    }
  }
}

RoutingModel::NodeIndex RoutingModel::IndexToNode(int64 index) const {
  DCHECK_LT(index, index_to_node_.size());
  return index_to_node_[index];
}

int64 RoutingModel::NodeToIndex(NodeIndex node) const {
  DCHECK_LT(node, node_to_index_.size());
  DCHECK_NE(node_to_index_[node], kUnassigned)
      << "RoutingModel::NodeToIndex should not be used for Start or End nodes";
  return node_to_index_[node];
}

bool RoutingModel::HasIndex(NodeIndex node) const {
  return node < node_to_index_.size() && node_to_index_[node] != kUnassigned;
}

int64 RoutingModel::GetArcCostForClassInternal(
    int64 i, int64 j, CostClassIndex cost_class_index) {
  DCHECK(closed_);
  DCHECK_GE(cost_class_index, 0);
  DCHECK_LT(cost_class_index, cost_classes_.size());
  CostCacheElement* const cache = &cost_cache_[i];
  // See the comment in CostCacheElement in the .h for the int64->int cast.
  if (cache->index == static_cast<int>(j) &&
      cache->cost_class_index == cost_class_index) {
    return cache->cost;
  }
  const NodeIndex node_i = IndexToNode(i);
  const NodeIndex node_j = IndexToNode(j);
  int64 cost = 0;
  const CostClass& cost_class = cost_classes_[cost_class_index];
  if (!IsStart(i)) {
    // TODO(user): fix overflows.
    cost = cost_class.arc_cost_evaluator->Run(node_i, node_j) +
           GetDimensionTransitCostSum(i, j, cost_class);
  } else if (!IsEnd(j)) {
    // Apply route fixed cost on first non-first/last node, in other words on
    // the arc from the first node to its next node if it's not the last node.
    cost = cost_class.arc_cost_evaluator->Run(node_i, node_j) +
           GetDimensionTransitCostSum(i, j, cost_class) +
           fixed_cost_of_vehicle_[index_to_vehicle_[i]];
  } else {
    // If there's only the first and last nodes on the route, it is considered
    // as an empty route thus the cost of 0.
    cost = 0;
  }
  cache->index = static_cast<int>(j);
  cache->cost_class_index = cost_class_index;
  cache->cost = cost;
  return cost;
}

bool RoutingModel::IsStart(int64 index) const {
  return !IsEnd(index) && index_to_vehicle_[index] != kUnassigned;
}

bool RoutingModel::IsVehicleUsed(const Assignment& assignment,
                                 int vehicle) const {
  CHECK_GE(vehicle, 0);
  CHECK_LT(vehicle, vehicles_);
  CHECK_EQ(solver_.get(), assignment.solver());
  IntVar* const start_var = NextVar(Start(vehicle));
  CHECK(assignment.Contains(start_var));
  return !IsEnd(assignment.Value(start_var));
}

const std::vector<IntVar*>& RoutingModel::CumulVars(
    const std::string& dimension_name) const {
  return GetDimensionOrDie(dimension_name).cumuls();
}

int64 RoutingModel::Next(const Assignment& assignment, int64 index) const {
  CHECK_EQ(solver_.get(), assignment.solver());
  IntVar* const next_var = NextVar(index);
  CHECK(assignment.Contains(next_var));
  CHECK(assignment.Bound(next_var));
  return assignment.Value(next_var);
}

int64 RoutingModel::GetArcCostForVehicle(int64 i, int64 j, int64 vehicle) {
  if (i != j && vehicle >= 0) {
    return GetArcCostForClassInternal(i, j,
                                      GetCostClassIndexOfVehicle(vehicle));
  } else {
    return 0;
  }
}

int64 RoutingModel::GetArcCostForClass(
    int64 i, int64 j, int64 /*CostClassIndex*/ cost_class_index) {
  if (i != j) {
    return GetArcCostForClassInternal(i, j, CostClassIndex(cost_class_index));
  } else {
    return 0;
  }
}

int64 RoutingModel::GetArcCostForFirstSolution(int64 i, int64 j) {
  // Return high cost if connecting to an end (or bound-to-end) node;
  // this is used in the cost-based first solution strategies to avoid closing
  // routes too soon.
  if (!is_bound_to_end_ct_added_.Switched()) {
    // Lazily adding path-cumul constraint propagating connection to route end,
    // as it can be pretty costly in the general case.
    std::vector<IntVar*> zero_transit(Size(), solver_->MakeIntConst(Zero()));
    solver_->AddConstraint(solver_->MakeDelayedPathCumul(
        nexts_, active_, is_bound_to_end_, zero_transit));
    is_bound_to_end_ct_added_.Switch(solver_.get());
  }
  if (is_bound_to_end_[j]->Min() == 1) return kint64max;
  // TODO(user): Take vehicle into account.
  return GetHomogeneousCost(i, j);
}

int64 RoutingModel::GetDimensionTransitCostSum(
    int64 i, int64 j, const CostClass& cost_class) const {
  int64 cost = 0;
  for (const auto& evaluator_and_coefficient :
       cost_class.dimension_transit_evaluator_class_and_cost_coefficient) {
    DCHECK_GT(evaluator_and_coefficient.cost_coefficient, 0);
    cost += evaluator_and_coefficient.cost_coefficient *
            evaluator_and_coefficient.dimension->GetTransitValueFromClass(
                i, j, evaluator_and_coefficient.transit_evaluator_class);
  }
  return cost;
}

bool RoutingModel::ArcIsMoreConstrainedThanArc(int64 from, int64 to1,
                                               int64 to2) {
  // Deal with end nodes: never pick an end node over a non-end node.
  if (IsEnd(to1) || IsEnd(to2)) {
    if (IsEnd(to1) != IsEnd(to2)) return IsEnd(to2);
    // If both are end nodes, we don't care; the right end node will be picked
    // by constraint propagation. Break the tie by index.
    return to1 < to2;
  }

  // Look whether they are mandatory (must be performed) or optional.
  const bool mandatory1 = active_[to1]->Min() == 1;
  const bool mandatory2 = active_[to2]->Min() == 1;
  // Always pick a mandatory node over a non-mandatory one.
  if (mandatory1 != mandatory2) return mandatory1;

  // Look at the vehicle variables.
  IntVar* const src_vehicle_var = VehicleVar(from);
  // In case the source vehicle is bound, "src_vehicle" will be it.
  // Otherwise, it'll be set to some possible source vehicle that
  // isn't -1 (if possible).
  const int64 src_vehicle = src_vehicle_var->Max();
  if (src_vehicle_var->Bound()) {
    IntVar* const to1_vehicle_var = VehicleVar(to1);
    IntVar* const to2_vehicle_var = VehicleVar(to2);
    // Subtle: non-mandatory node have kNoVehicle as possible value for
    // their vehicle variable. So they're effectively "bound" when their domain
    // size is 2.
    const bool bound1 =
        mandatory1 ? to1_vehicle_var->Bound() : (to1_vehicle_var->Size() <= 2);
    const bool bound2 =
        mandatory2 ? to2_vehicle_var->Bound() : (to2_vehicle_var->Size() <= 2);
    // Prefer a destination bound to a given vehicle, even if it's not
    // bound to the right one (the propagation will quickly rule it out).
    if (bound1 != bound2) return bound1;
    if (bound1) {  // same as bound1 && bound2.
      // Min() will return kNoVehicle for optional nodes. Thus we use Max().
      const int64 vehicle1 = to1_vehicle_var->Max();
      const int64 vehicle2 = to2_vehicle_var->Max();
      // Prefer a destination bound to the right vehicle.
      // TODO(user): cover this clause in a unit test.
      if ((vehicle1 == src_vehicle) != (vehicle2 == src_vehicle)) {
        return vehicle1 == src_vehicle;
      }
      // If no destination is bound to the right vehicle, whatever we
      // return doesn't matter: both are infeasible. To be consistent, we
      // just break the tie.
      if (vehicle1 != src_vehicle) return to1 < to2;
    }
  }
  // At this point, either both destinations are bound to the source vehicle,
  // or none of them is bound, or the source vehicle isn't bound.
  // We don't bother inspecting the domains of the vehicle variables further.

  // Inspect the primary constrained dimension, if any.
  // TODO(user): try looking at all the dimensions, not just the primary one,
  // and reconsider the need for a "primary" dimension.
  if (!GetPrimaryConstrainedDimension().empty()) {
    const std::vector<IntVar*>& cumul_vars =
        CumulVars(GetPrimaryConstrainedDimension());
    IntVar* const dim1 = cumul_vars[to1];
    IntVar* const dim2 = cumul_vars[to2];
    // Prefer the destination that has a lower upper bound for the constrained
    // dimension.
    if (dim1->Max() != dim2->Max()) return dim1->Max() < dim2->Max();
    // TODO(user): evaluate the *actual* Min() of each cumul variable in the
    // scenario where the corresponding arc from->to is performed, and pick
    // the destination with the lowest value.
  }

  // Break ties on equally constrained nodes with the (cost - unperformed
  // penalty).
  {
    const /*CostClassIndex*/ int64 cost_class_index =
        SafeGetCostClassInt64OfVehicle(src_vehicle);
    const int64 cost1 = CapSub(GetArcCostForClass(from, to1, cost_class_index),
                               UnperformedPenalty(to1));
    const int64 cost2 = CapSub(GetArcCostForClass(from, to2, cost_class_index),
                               UnperformedPenalty(to2));
    if (cost1 != cost2) return cost1 < cost2;
  }

  // Further break ties by looking at the size of the VehicleVar.
  {
    const int64 num_vehicles1 = VehicleVar(to1)->Size();
    const int64 num_vehicles2 = VehicleVar(to2)->Size();
    if (num_vehicles1 != num_vehicles2) return num_vehicles1 < num_vehicles2;
  }

  // Break perfect ties by value.
  return to1 < to2;
}

int64 RoutingModel::UnperformedPenalty(int64 var_index) const {
  return UnperformedPenaltyOrValue(0, var_index);
}

int64 RoutingModel::UnperformedPenaltyOrValue(int64 default_value,
                                              int64 var_index) const {
  if (active_[var_index]->Min() == 1) return kint64max;  // Forced active.
  const std::vector<DisjunctionIndex>& disjunction_indices =
      GetDisjunctionIndicesFromVariableIndex(var_index);
  if (disjunction_indices.size() != 1) return default_value;
  const DisjunctionIndex disjunction_index = disjunction_indices[0];
  if (disjunctions_[disjunction_index].nodes.size() != 1) return default_value;
  DCHECK_EQ(var_index, disjunctions_[disjunction_index].nodes[0]);
  // The disjunction penalty can't be kNoPenalty, otherwise we would have
  // caught it earlier (the node would be forced active).
  DCHECK_GE(disjunctions_[disjunction_index].value.penalty, 0);
  return disjunctions_[disjunction_index].value.penalty;
}

std::string RoutingModel::DebugOutputAssignment(
    const Assignment& solution_assignment,
    const std::string& dimension_to_print) const {
  for (int i = 0; i < Size(); ++i) {
    if (!solution_assignment.Bound(NextVar(i))) {
      LOG(DFATAL)
          << "DebugOutputVehicleSchedules() called on incomplete solution:"
          << " NextVar(" << i << ") is unbound.";
      return "";
    }
  }
  std::string output;
  std::unordered_set<std::string> dimension_names;
  if (dimension_to_print == "") {
    const std::vector<std::string> all_dimension_names = GetAllDimensionNames();
    dimension_names.insert(all_dimension_names.begin(),
                           all_dimension_names.end());
  } else {
    dimension_names.insert(dimension_to_print);
  }
  for (int vehicle = 0; vehicle < vehicles(); ++vehicle) {
    int empty_vehicle_range_start = vehicle;
    while (vehicle < vehicles() &&
           IsEnd(solution_assignment.Value(NextVar(Start(vehicle))))) {
      vehicle++;
    }
    if (empty_vehicle_range_start != vehicle) {
      if (empty_vehicle_range_start == vehicle - 1) {
        StringAppendF(&output, "Vehicle %d: empty", empty_vehicle_range_start);
      } else {
        StringAppendF(&output, "Vehicles %d-%d: empty",
                      empty_vehicle_range_start, vehicle - 1);
      }
      output.append("\n");
    }
    if (vehicle < vehicles()) {
      StringAppendF(&output, "Vehicle %d:", vehicle);
      int64 index = Start(vehicle);
      for (;;) {
        const IntVar* vehicle_var = VehicleVar(index);
        StringAppendF(&output,
                      "%" GG_LL_FORMAT "d Vehicle(%" GG_LL_FORMAT "d) ", index,
                      solution_assignment.Value(vehicle_var));
        for (const RoutingDimension* const dimension : dimensions_) {
          if (ContainsKey(dimension_names, dimension->name())) {
            const IntVar* const var = dimension->CumulVar(index);
            StringAppendF(
                &output, "%s(%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d) ",
                dimension->name().c_str(), solution_assignment.Min(var),
                solution_assignment.Max(var));
          }
        }
        if (IsEnd(index)) break;
        index = solution_assignment.Value(NextVar(index));
        if (IsEnd(index)) output.append("Route end ");
      }
      output.append("\n");
    }
  }
  output.append("Unperformed nodes: ");
  for (int i = 0; i < Size(); ++i) {
    if (!IsEnd(i) && !IsStart(i) &&
        solution_assignment.Value(NextVar(i)) == i) {
      StringAppendF(&output, "%d ", i);
    }
  }
  output.append("\n");
  return output;
}

Assignment* RoutingModel::GetOrCreateAssignment() {
  if (assignment_ == nullptr) {
    assignment_ = solver_->MakeAssignment();
    assignment_->Add(nexts_);
    if (!CostsAreHomogeneousAcrossVehicles()) {
      assignment_->Add(vehicle_vars_);
    }
    assignment_->AddObjective(cost_);
  }
  return assignment_;
}

SearchLimit* RoutingModel::GetOrCreateLimit() {
  if (limit_ == nullptr) {
    limit_ =
        solver_->MakeLimit(kint64max, kint64max, kint64max, kint64max, true);
  }
  return limit_;
}

SearchLimit* RoutingModel::GetOrCreateLocalSearchLimit() {
  if (ls_limit_ == nullptr) {
    ls_limit_ = solver_->MakeLimit(kint64max, kint64max, kint64max, 1, true);
  }
  return ls_limit_;
}

SearchLimit* RoutingModel::GetOrCreateLargeNeighborhoodSearchLimit() {
  if (lns_limit_ == nullptr) {
    lns_limit_ = solver_->MakeLimit(kint64max, kint64max, kint64max, kint64max);
  }
  return lns_limit_;
}

LocalSearchOperator* RoutingModel::CreateInsertionOperator() {
  std::vector<IntVar*> empty;
  LocalSearchOperator* insertion_operator =
      MakeLocalSearchOperator<MakeActiveOperator>(
          solver_.get(), nexts_,
          CostsAreHomogeneousAcrossVehicles() ? empty : vehicle_vars_,
          vehicle_start_class_callback_);
  if (!pickup_delivery_pairs_.empty()) {
    insertion_operator = solver_->ConcatenateOperators(
        {MakePairActive(
             solver_.get(), nexts_,
             CostsAreHomogeneousAcrossVehicles() ? empty : vehicle_vars_,
             vehicle_start_class_callback_, pickup_delivery_pairs_),
         insertion_operator});
  }
  return insertion_operator;
}

LocalSearchOperator* RoutingModel::CreateMakeInactiveOperator() {
  std::vector<IntVar*> empty;
  LocalSearchOperator* make_inactive_operator =
      MakeLocalSearchOperator<MakeInactiveOperator>(
          solver_.get(), nexts_,
          CostsAreHomogeneousAcrossVehicles() ? empty : vehicle_vars_,
          vehicle_start_class_callback_);
  if (!pickup_delivery_pairs_.empty()) {
    make_inactive_operator = solver_->ConcatenateOperators(
        {MakePairInactive(
             solver_.get(), nexts_,
             CostsAreHomogeneousAcrossVehicles() ? empty : vehicle_vars_,
             vehicle_start_class_callback_, pickup_delivery_pairs_),
         make_inactive_operator});
  }
  return make_inactive_operator;
}

#define CP_ROUTING_ADD_OPERATOR(operator_type, cp_operator_type)    \
  if (CostsAreHomogeneousAcrossVehicles()) {                        \
    local_search_operators_[operator_type] =                        \
        solver_->MakeOperator(nexts_, Solver::cp_operator_type);    \
  } else {                                                          \
    local_search_operators_[operator_type] = solver_->MakeOperator( \
        nexts_, vehicle_vars_, Solver::cp_operator_type);           \
  }

#define CP_ROUTING_ADD_OPERATOR2(operator_type, cp_operator_class)     \
  local_search_operators_[operator_type] =                             \
      MakeLocalSearchOperator<cp_operator_class>(                      \
          solver_.get(), nexts_,                                       \
          CostsAreHomogeneousAcrossVehicles() ? std::vector<IntVar*>() \
                                              : vehicle_vars_,         \
          vehicle_start_class_callback_);

#define CP_ROUTING_ADD_CALLBACK_OPERATOR(operator_type, cp_operator_type)  \
  if (CostsAreHomogeneousAcrossVehicles()) {                               \
    local_search_operators_[operator_type] = solver_->MakeOperator(        \
        nexts_, [this](int64 i, int64 j,                                   \
                       int64 k) { return GetArcCostForVehicle(i, j, k); }, \
        Solver::cp_operator_type);                                         \
  } else {                                                                 \
    local_search_operators_[operator_type] =                               \
        solver_->MakeOperator(nexts_, vehicle_vars_,                       \
                              [this](int64 i, int64 j, int64 k) {          \
                                return GetArcCostForVehicle(i, j, k);      \
                              },                                           \
                              Solver::cp_operator_type);                   \
  }

void RoutingModel::CreateNeighborhoodOperators() {
  local_search_operators_.clear();
  local_search_operators_.resize(LOCAL_SEARCH_OPERATOR_COUNTER, nullptr);
  CP_ROUTING_ADD_OPERATOR2(RELOCATE, Relocate);
  std::vector<IntVar*> empty;
  local_search_operators_[RELOCATE_PAIR] = MakePairRelocate(
      solver_.get(), nexts_,
      CostsAreHomogeneousAcrossVehicles() ? empty : vehicle_vars_,
      vehicle_start_class_callback_, pickup_delivery_pairs_);
  local_search_operators_[RELOCATE_NEIGHBORS] = MakeRelocateNeighbors(
      solver_.get(), nexts_,
      CostsAreHomogeneousAcrossVehicles() ? empty : vehicle_vars_,
      vehicle_start_class_callback_,
      [this](int64 from, int64 to) { return GetHomogeneousCost(from, to); });
  local_search_operators_[NODE_PAIR_SWAP] = solver_->ConcatenateOperators(
      {NodePairSwapActive(
           solver_.get(), nexts_,
           CostsAreHomogeneousAcrossVehicles() ? empty : vehicle_vars_,
           vehicle_start_class_callback_, pickup_delivery_pairs_),
       PairNodeSwapActive(
           solver_.get(), nexts_,
           CostsAreHomogeneousAcrossVehicles() ? empty : vehicle_vars_,
           vehicle_start_class_callback_, pickup_delivery_pairs_)});
  CP_ROUTING_ADD_OPERATOR2(EXCHANGE, Exchange);
  CP_ROUTING_ADD_OPERATOR2(CROSS, Cross);
  CP_ROUTING_ADD_OPERATOR2(TWO_OPT, TwoOpt);
  CP_ROUTING_ADD_OPERATOR(OR_OPT, OROPT);
  CP_ROUTING_ADD_CALLBACK_OPERATOR(LIN_KERNIGHAN, LK);
  local_search_operators_[MAKE_ACTIVE] = CreateInsertionOperator();
  CP_ROUTING_ADD_OPERATOR2(RELOCATE_AND_MAKE_ACTIVE,
                           RelocateAndMakeActiveOperator);
  CP_ROUTING_ADD_OPERATOR2(MAKE_ACTIVE_AND_RELOCATE, MakeActiveAndRelocate);
  local_search_operators_[MAKE_INACTIVE] = CreateMakeInactiveOperator();
  CP_ROUTING_ADD_OPERATOR2(MAKE_CHAIN_INACTIVE, MakeChainInactiveOperator);
  CP_ROUTING_ADD_OPERATOR2(SWAP_ACTIVE, SwapActiveOperator);
  CP_ROUTING_ADD_OPERATOR2(EXTENDED_SWAP_ACTIVE, ExtendedSwapActiveOperator);
  CP_ROUTING_ADD_CALLBACK_OPERATOR(TSP_OPT, TSPOPT);
  CP_ROUTING_ADD_CALLBACK_OPERATOR(TSP_LNS, TSPLNS);
  CP_ROUTING_ADD_OPERATOR(PATH_LNS, PATHLNS);
  CP_ROUTING_ADD_OPERATOR(FULL_PATH_LNS, FULLPATHLNS);
  CP_ROUTING_ADD_OPERATOR(INACTIVE_LNS, UNACTIVELNS);
}

#undef CP_ROUTING_ADD_CALLBACK_OPERATOR
#undef CP_ROUTING_ADD_OPERATOR2
#undef CP_ROUTING_ADD_OPERATOR

#define CP_ROUTING_PUSH_OPERATOR(operator_type, operator_method, operators) \
  if (search_parameters.local_search_operators().use_##operator_method()) { \
    operators.push_back(local_search_operators_[operator_type]);            \
  }

LocalSearchOperator* RoutingModel::GetNeighborhoodOperators(
    const RoutingSearchParameters& search_parameters) const {
  std::vector<LocalSearchOperator*> operators = extra_operators_;
  if (pickup_delivery_pairs_.size() > 0) {
    CP_ROUTING_PUSH_OPERATOR(RELOCATE_PAIR, relocate_pair, operators);
    CP_ROUTING_PUSH_OPERATOR(NODE_PAIR_SWAP, node_pair_swap_active, operators);
  }
  if (vehicles_ > 1) {
    CP_ROUTING_PUSH_OPERATOR(RELOCATE, relocate, operators);
    CP_ROUTING_PUSH_OPERATOR(EXCHANGE, exchange, operators);
    CP_ROUTING_PUSH_OPERATOR(CROSS, cross, operators);
  }
  if (pickup_delivery_pairs_.size() > 0 ||
      search_parameters.local_search_operators().use_relocate_neighbors()) {
    operators.push_back(local_search_operators_[RELOCATE_NEIGHBORS]);
  }
  const LocalSearchMetaheuristic::Value local_search_metaheuristic =
      search_parameters.local_search_metaheuristic();
  if (local_search_metaheuristic != LocalSearchMetaheuristic::TABU_SEARCH &&
      local_search_metaheuristic !=
          LocalSearchMetaheuristic::OBJECTIVE_TABU_SEARCH &&
      local_search_metaheuristic !=
          LocalSearchMetaheuristic::SIMULATED_ANNEALING) {
    CP_ROUTING_PUSH_OPERATOR(LIN_KERNIGHAN, lin_kernighan, operators);
  }
  CP_ROUTING_PUSH_OPERATOR(TWO_OPT, two_opt, operators);
  CP_ROUTING_PUSH_OPERATOR(OR_OPT, or_opt, operators);
  if (disjunctions_.size() != 0) {
    CP_ROUTING_PUSH_OPERATOR(MAKE_INACTIVE, make_inactive, operators);
    CP_ROUTING_PUSH_OPERATOR(MAKE_CHAIN_INACTIVE, make_chain_inactive,
                             operators);
    CP_ROUTING_PUSH_OPERATOR(MAKE_ACTIVE, make_active, operators);

    // The relocate_and_make_active parameter activates all neighborhoods
    // relocating a node together with making another active.
    CP_ROUTING_PUSH_OPERATOR(RELOCATE_AND_MAKE_ACTIVE, relocate_and_make_active,
                             operators);
    CP_ROUTING_PUSH_OPERATOR(MAKE_ACTIVE_AND_RELOCATE, relocate_and_make_active,
                             operators);

    CP_ROUTING_PUSH_OPERATOR(SWAP_ACTIVE, swap_active, operators);
    CP_ROUTING_PUSH_OPERATOR(EXTENDED_SWAP_ACTIVE, extended_swap_active,
                             operators);
  }
  // TODO(user): move the following operators to a second local search
  // loop.
  if (local_search_metaheuristic != LocalSearchMetaheuristic::TABU_SEARCH &&
      local_search_metaheuristic !=
          LocalSearchMetaheuristic::OBJECTIVE_TABU_SEARCH &&
      local_search_metaheuristic !=
          LocalSearchMetaheuristic::SIMULATED_ANNEALING) {
    CP_ROUTING_PUSH_OPERATOR(TSP_OPT, tsp_opt, operators);
  }
  if (local_search_metaheuristic != LocalSearchMetaheuristic::TABU_SEARCH &&
      local_search_metaheuristic !=
          LocalSearchMetaheuristic::OBJECTIVE_TABU_SEARCH &&
      local_search_metaheuristic !=
          LocalSearchMetaheuristic::SIMULATED_ANNEALING) {
    CP_ROUTING_PUSH_OPERATOR(TSP_LNS, tsp_lns, operators);
  }
  CP_ROUTING_PUSH_OPERATOR(FULL_PATH_LNS, full_path_lns, operators);
  CP_ROUTING_PUSH_OPERATOR(PATH_LNS, path_lns, operators);
  if (disjunctions_.size() != 0) {
    CP_ROUTING_PUSH_OPERATOR(INACTIVE_LNS, inactive_lns, operators);
  }
  return solver_->ConcatenateOperators(operators);
}

#undef CP_ROUTING_PUSH_OPERATOR

const std::vector<LocalSearchFilter*>&
RoutingModel::GetOrCreateLocalSearchFilters() {
  // Note on objective injection from one filter to another.
  // As of 2013/01, three filters evaluate sub-parts of the objective
  // function:
  // - NodeDisjunctionFilter: takes disjunction penalty costs into account,
  // - PathCumulFilter: takes dimension span costs into account,
  // - LocalSearchObjectiveFilter: takes dimension "arc" costs into account.
  // To be able to filter cost values properly, a filter needs to be aware of
  // cost bounds computed by other filters before it (for the same delta).
  // Communication of cost between filters is done through callbacks,
  // LocalSearchObjectiveFilter sending total arc costs to
  // NodeDisjunctionFilter, itself sending this cost + total penalty cost to
  // PathCumulFilters (if you have several of these, they send updated costs to
  // each other too). Callbacks are called on OnSynchronize to send the cost
  // of the current solution and on Accept to send the cost of solution deltas.
  if (filters_.empty()) {
    std::vector<RoutingLocalSearchFilter*> path_cumul_filters;
    RoutingLocalSearchFilter* path_cumul_filter = nullptr;
    for (const RoutingDimension* dimension : dimensions_) {
      Solver::ObjectiveWatcher objective_callback = nullptr;
      if (path_cumul_filter != nullptr) {
        objective_callback = [path_cumul_filter](int64 value) {
          return path_cumul_filter->InjectObjectiveValue(value);
        };
      }
      path_cumul_filter =
          MakePathCumulFilter(*this, *dimension, objective_callback);
      path_cumul_filters.push_back(path_cumul_filter);
    }
    // Due to the way cost injection is setup, path filters have to be
    // called in reverse order.
    std::reverse(path_cumul_filters.begin(), path_cumul_filters.end());
    RoutingLocalSearchFilter* node_disjunction_filter = nullptr;
    if (!disjunctions_.empty()) {
      Solver::ObjectiveWatcher objective_callback = nullptr;
      if (path_cumul_filter != nullptr) {
        objective_callback = [path_cumul_filter](int64 value) {
          return path_cumul_filter->InjectObjectiveValue(value);
        };
      }
      node_disjunction_filter =
          MakeNodeDisjunctionFilter(*this, objective_callback);
    }
    Solver::ObjectiveWatcher objective_callback = nullptr;
    if (node_disjunction_filter != nullptr) {
      objective_callback = [node_disjunction_filter](int64 value) {
        return node_disjunction_filter->InjectObjectiveValue(value);
      };
    } else if (path_cumul_filter != nullptr) {
      objective_callback = [path_cumul_filter](int64 value) {
        return path_cumul_filter->InjectObjectiveValue(value);
      };
    }
    if (CostsAreHomogeneousAcrossVehicles()) {
      LocalSearchFilter* filter = solver_->MakeLocalSearchObjectiveFilter(
          nexts_, [this](int64 i, int64 j) { return GetHomogeneousCost(i, j); },
          objective_callback, cost_, Solver::LE, Solver::SUM);
      filters_.push_back(filter);
    } else {
      LocalSearchFilter* filter = solver_->MakeLocalSearchObjectiveFilter(
          nexts_, vehicle_vars_,
          [this](int64 i, int64 j, int64 k) {
            return GetArcCostForVehicle(i, j, k);
          },
          objective_callback, cost_, Solver::LE, Solver::SUM);
      filters_.push_back(filter);
    }
    filters_.push_back(solver_->MakeVariableDomainFilter());
    if (node_disjunction_filter != nullptr) {
      // Must be added after ObjectiveFilter.
      filters_.push_back(node_disjunction_filter);
    }
    if (pickup_delivery_pairs_.size() > 0) {
      filters_.push_back(
          MakeNodePrecedenceFilter(*this, pickup_delivery_pairs_));
    }
    filters_.push_back(MakeVehicleVarFilter(*this));
    // Must be added after NodeDisjunctionFilter and ObjectiveFilter.
    filters_.insert(filters_.end(), path_cumul_filters.begin(),
                    path_cumul_filters.end());
    filters_.insert(filters_.end(), extra_filters_.begin(),
                    extra_filters_.end());
  }
  return filters_;
}
const std::vector<LocalSearchFilter*>&
RoutingModel::GetOrCreateFeasibilityFilters() {
  if (feasibility_filters_.empty()) {
    for (const RoutingDimension* const dimension : dimensions_) {
      feasibility_filters_.push_back(
          MakePathCumulFilter(*this, *dimension, nullptr));
    }
    if (!disjunctions_.empty()) {
      feasibility_filters_.push_back(MakeNodeDisjunctionFilter(*this, nullptr));
    }
    feasibility_filters_.push_back(solver_->MakeVariableDomainFilter());
    if (pickup_delivery_pairs_.size() > 0) {
      feasibility_filters_.push_back(
          MakeNodePrecedenceFilter(*this, pickup_delivery_pairs_));
    }
    feasibility_filters_.push_back(MakeVehicleVarFilter(*this));
    feasibility_filters_.insert(feasibility_filters_.end(),
                                extra_filters_.begin(), extra_filters_.end());
  }
  return feasibility_filters_;
}

DecisionBuilder* RoutingModel::CreateSolutionFinalizer() {
  std::vector<DecisionBuilder*> decision_builders;
  decision_builders.push_back(solver_->MakePhase(
      nexts_, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE));
  for (IntVar* const variable : variables_minimized_by_finalizer_) {
    decision_builders.push_back(solver_->MakePhase(
        variable, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE));
  }
  for (IntVar* const variable : variables_maximized_by_finalizer_) {
    decision_builders.push_back(solver_->MakePhase(
        variable, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MAX_VALUE));
  }
  return solver_->Compose(decision_builders);
}

void RoutingModel::CreateFirstSolutionDecisionBuilders(
    const RoutingSearchParameters& search_parameters) {
  first_solution_decision_builders_.resize(
      FirstSolutionStrategy_Value_Value_ARRAYSIZE, nullptr);
  first_solution_filtered_decision_builders_.resize(
      FirstSolutionStrategy_Value_Value_ARRAYSIZE, nullptr);
  DecisionBuilder* const finalize_solution = CreateSolutionFinalizer();
  // Default heuristic
  first_solution_decision_builders_
      [FirstSolutionStrategy::FIRST_UNBOUND_MIN_VALUE] = finalize_solution;
  // Global cheapest addition heuristic.
  first_solution_decision_builders_
      [FirstSolutionStrategy::GLOBAL_CHEAPEST_ARC] = solver_->MakePhase(
          nexts_,
          [this](int64 i, int64 j) { return GetArcCostForFirstSolution(i, j); },
          Solver::CHOOSE_STATIC_GLOBAL_BEST);
  // Cheapest addition heuristic.
  Solver::IndexEvaluator2 eval = [this](int64 i, int64 j) {
    return GetArcCostForFirstSolution(i, j);
  };
  first_solution_decision_builders_[FirstSolutionStrategy::LOCAL_CHEAPEST_ARC] =
      solver_->MakePhase(nexts_, Solver::CHOOSE_FIRST_UNBOUND, eval);
  // Path-based cheapest addition heuristic.
  first_solution_decision_builders_[FirstSolutionStrategy::PATH_CHEAPEST_ARC] =
      solver_->MakePhase(nexts_, Solver::CHOOSE_PATH, eval);
  if (vehicles() == 1 && pickup_delivery_pairs_.empty()) {
    DecisionBuilder* fast_one_path_builder =
        solver_->RevAlloc(new FastOnePathBuilder(
            this, NewPermanentCallback(
                      this, &RoutingModel::GetArcCostForFirstSolution)));
    first_solution_decision_builders_
        [FirstSolutionStrategy::PATH_CHEAPEST_ARC] =
            solver_->Try(fast_one_path_builder,
                         first_solution_decision_builders_
                             [FirstSolutionStrategy::PATH_CHEAPEST_ARC]);
  } else if (search_parameters.use_filtered_first_solution_strategy()) {
    first_solution_filtered_decision_builders_
        [FirstSolutionStrategy::PATH_CHEAPEST_ARC] = solver_->RevAlloc(
            new EvaluatorCheapestAdditionFilteredDecisionBuilder(
                this, NewPermanentCallback(
                          this, &RoutingModel::GetArcCostForFirstSolution),
                GetOrCreateFeasibilityFilters()));
    first_solution_decision_builders_
        [FirstSolutionStrategy::PATH_CHEAPEST_ARC] =
            solver_->Try(first_solution_filtered_decision_builders_
                             [FirstSolutionStrategy::PATH_CHEAPEST_ARC],
                         first_solution_decision_builders_
                             [FirstSolutionStrategy::PATH_CHEAPEST_ARC]);
  }
  // Path-based most constrained arc addition heuristic.
  Solver::VariableValueComparator comp = [this](int64 i, int64 j, int64 k) {
    return ArcIsMoreConstrainedThanArc(i, j, k);
  };

  first_solution_decision_builders_
      [FirstSolutionStrategy::PATH_MOST_CONSTRAINED_ARC] =
          solver_->MakePhase(nexts_, Solver::CHOOSE_PATH, comp);
  if (search_parameters.use_filtered_first_solution_strategy()) {
    first_solution_filtered_decision_builders_
        [FirstSolutionStrategy::PATH_MOST_CONSTRAINED_ARC] = solver_->RevAlloc(
            new ComparatorCheapestAdditionFilteredDecisionBuilder(
                this, comp, GetOrCreateFeasibilityFilters()));
    first_solution_decision_builders_
        [FirstSolutionStrategy::PATH_MOST_CONSTRAINED_ARC] = solver_->Try(
            first_solution_filtered_decision_builders_
                [FirstSolutionStrategy::PATH_MOST_CONSTRAINED_ARC],
            first_solution_decision_builders_
                [FirstSolutionStrategy::PATH_MOST_CONSTRAINED_ARC]);
  }
  // Evaluator-based path heuristic.
  if (first_solution_evaluator_ != nullptr) {
    first_solution_decision_builders_
        [FirstSolutionStrategy::EVALUATOR_STRATEGY] = solver_->MakePhase(
            nexts_, Solver::CHOOSE_PATH, first_solution_evaluator_);
  } else {
    first_solution_decision_builders_
        [FirstSolutionStrategy::EVALUATOR_STRATEGY] = nullptr;
  }
  // All unperformed heuristic.
  first_solution_decision_builders_[FirstSolutionStrategy::ALL_UNPERFORMED] =
      solver_->RevAlloc(new AllUnperformed(this));
  // Best insertion heuristic.
  SearchLimit* const ls_limit = solver_->MakeLimit(
      search_parameters.time_limit_ms(), kint64max, kint64max, kint64max, true);
  DecisionBuilder* const finalize = solver_->MakeSolveOnce(
      finalize_solution, GetOrCreateLargeNeighborhoodSearchLimit());
  LocalSearchPhaseParameters* const insertion_parameters =
      solver_->MakeLocalSearchPhaseParameters(CreateInsertionOperator(),
                                              finalize, ls_limit,
                                              GetOrCreateLocalSearchFilters());
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(GetOrCreateLimit());
  std::vector<IntVar*> decision_vars = nexts_;
  if (!CostsAreHomogeneousAcrossVehicles()) {
    decision_vars.insert(decision_vars.end(), vehicle_vars_.begin(),
                         vehicle_vars_.end());
  }
  first_solution_decision_builders_[FirstSolutionStrategy::BEST_INSERTION] =
      solver_->MakeNestedOptimize(
          solver_->MakeLocalSearchPhase(
              decision_vars, solver_->RevAlloc(new AllUnperformed(this)),
              insertion_parameters),
          GetOrCreateAssignment(), false, search_parameters.optimization_step(),
          monitors);
  first_solution_decision_builders_[FirstSolutionStrategy::BEST_INSERTION] =
      solver_->Compose(first_solution_decision_builders_
                           [FirstSolutionStrategy::BEST_INSERTION],
                       finalize);
  // Global cheapest insertion
  first_solution_filtered_decision_builders_
      [FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION] =
          solver_->RevAlloc(new GlobalCheapestInsertionFilteredDecisionBuilder(
              this,
              NewPermanentCallback(this, &RoutingModel::GetArcCostForVehicle),
              NewPermanentCallback(this,
                                   &RoutingModel::UnperformedPenaltyOrValue, 0),
              GetOrCreateFeasibilityFilters()));
  first_solution_decision_builders_
      [FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION] =
          solver_->Try(first_solution_filtered_decision_builders_
                           [FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION],
                       first_solution_decision_builders_
                           [FirstSolutionStrategy::BEST_INSERTION]);
  // Local cheapest insertion
  first_solution_filtered_decision_builders_
      [FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION] =
          solver_->RevAlloc(new LocalCheapestInsertionFilteredDecisionBuilder(
              this,
              NewPermanentCallback(this, &RoutingModel::GetArcCostForVehicle),
              GetOrCreateFeasibilityFilters()));
  first_solution_decision_builders_
      [FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION] =
          solver_->Try(first_solution_filtered_decision_builders_
                           [FirstSolutionStrategy::LOCAL_CHEAPEST_INSERTION],
                       first_solution_decision_builders_
                           [FirstSolutionStrategy::BEST_INSERTION]);
  // Savings
  if (search_parameters.use_filtered_first_solution_strategy()) {
    first_solution_filtered_decision_builders_[FirstSolutionStrategy::SAVINGS] =
        solver_->RevAlloc(new SavingsFilteredDecisionBuilder(
            this, FLAGS_savings_filter_neighbors,
            GetOrCreateFeasibilityFilters()));
    first_solution_decision_builders_[FirstSolutionStrategy::SAVINGS] =
        solver_->Try(first_solution_filtered_decision_builders_
                         [FirstSolutionStrategy::SAVINGS],
                     solver_->RevAlloc(new SavingsBuilder(this, true)));
  } else {
    first_solution_decision_builders_[FirstSolutionStrategy::SAVINGS] =
        solver_->RevAlloc(new SavingsBuilder(this, true));
    DecisionBuilder* savings_builder =
        solver_->RevAlloc(new SavingsBuilder(this, false));
    first_solution_decision_builders_[FirstSolutionStrategy::SAVINGS] =
        solver_->Try(
            savings_builder,
            first_solution_decision_builders_[FirstSolutionStrategy::SAVINGS]);
  }
  // Sweep
  first_solution_decision_builders_[FirstSolutionStrategy::SWEEP] =
      solver_->RevAlloc(new SweepBuilder(this, true));
  DecisionBuilder* sweep_builder =
      solver_->RevAlloc(new SweepBuilder(this, false));
  first_solution_decision_builders_[FirstSolutionStrategy::SWEEP] =
      solver_->Try(
          sweep_builder,
          first_solution_decision_builders_[FirstSolutionStrategy::SWEEP]);
  // Christofides
  first_solution_decision_builders_[FirstSolutionStrategy::CHRISTOFIDES] =
      solver_->RevAlloc(new ChristofidesFilteredDecisionBuilder(
          this, GetOrCreateFeasibilityFilters()));
  // Automatic
  // TODO(user): make this smarter.
  if (pickup_delivery_pairs_.empty()) {
    first_solution_decision_builders_[FirstSolutionStrategy::AUTOMATIC] =
        first_solution_decision_builders_
            [FirstSolutionStrategy::PATH_CHEAPEST_ARC];
  } else {
    first_solution_decision_builders_[FirstSolutionStrategy::AUTOMATIC] =
        first_solution_decision_builders_
            [FirstSolutionStrategy::PARALLEL_CHEAPEST_INSERTION];
  }
}

DecisionBuilder* RoutingModel::GetFirstSolutionDecisionBuilder(
    const RoutingSearchParameters& search_parameters) const {
  const FirstSolutionStrategy::Value first_solution_strategy =
      search_parameters.first_solution_strategy();
  if (first_solution_strategy < first_solution_decision_builders_.size()) {
    return first_solution_decision_builders_[first_solution_strategy];
  } else {
    return nullptr;
  }
}

IntVarFilteredDecisionBuilder*
RoutingModel::GetFilteredFirstSolutionDecisionBuilderOrNull(
    const RoutingSearchParameters& search_parameters) const {
  const FirstSolutionStrategy::Value first_solution_strategy =
      search_parameters.first_solution_strategy();
  return first_solution_filtered_decision_builders_[first_solution_strategy];
}

LocalSearchPhaseParameters* RoutingModel::CreateLocalSearchParameters(
    const RoutingSearchParameters& search_parameters) {
  return solver_->MakeLocalSearchPhaseParameters(
      GetNeighborhoodOperators(search_parameters),
      solver_->MakeSolveOnce(CreateSolutionFinalizer(),
                             GetOrCreateLargeNeighborhoodSearchLimit()),
      GetOrCreateLocalSearchLimit(), GetOrCreateLocalSearchFilters());
}

DecisionBuilder* RoutingModel::CreateLocalSearchDecisionBuilder(
    const RoutingSearchParameters& search_parameters) {
  const int size = Size();
  DecisionBuilder* first_solution =
      GetFirstSolutionDecisionBuilder(search_parameters);
  LocalSearchPhaseParameters* const parameters =
      CreateLocalSearchParameters(search_parameters);
  if (CostsAreHomogeneousAcrossVehicles()) {
    return solver_->MakeLocalSearchPhase(nexts_, first_solution, parameters);
  } else {
    const int all_size = size + size + vehicles_;
    std::vector<IntVar*> all_vars(all_size);
    for (int i = 0; i < size; ++i) {
      all_vars[i] = nexts_[i];
    }
    for (int i = size; i < all_size; ++i) {
      all_vars[i] = vehicle_vars_[i - size];
    }
    return solver_->MakeLocalSearchPhase(all_vars, first_solution, parameters);
  }
}

void RoutingModel::SetupDecisionBuilders(
    const RoutingSearchParameters& search_parameters) {
  if (search_parameters.use_depth_first_search()) {
    solve_db_ = GetFirstSolutionDecisionBuilder(search_parameters);
  } else {
    solve_db_ = CreateLocalSearchDecisionBuilder(search_parameters);
  }
  CHECK(preassignment_ != nullptr);
  DecisionBuilder* restore_preassignment =
      solver_->MakeRestoreAssignment(preassignment_);
  solve_db_ = solver_->Compose(restore_preassignment, solve_db_);
  improve_db_ =
      solver_->Compose(restore_preassignment,
                       solver_->MakeLocalSearchPhase(
                           GetOrCreateAssignment(),
                           CreateLocalSearchParameters(search_parameters)));
  restore_assignment_ =
      solver_->Compose(solver_->MakeRestoreAssignment(GetOrCreateAssignment()),
                       CreateSolutionFinalizer());
}

void RoutingModel::SetupMetaheuristics(
    const RoutingSearchParameters& search_parameters) {
  SearchMonitor* optimize;
  const LocalSearchMetaheuristic::Value metaheuristic =
      search_parameters.local_search_metaheuristic();
  switch (metaheuristic) {
    case LocalSearchMetaheuristic::GUIDED_LOCAL_SEARCH:
      if (CostsAreHomogeneousAcrossVehicles()) {
        optimize = solver_->MakeGuidedLocalSearch(
            false, cost_,
            [this](int64 i, int64 j) { return GetHomogeneousCost(i, j); },
            search_parameters.optimization_step(), nexts_,
            search_parameters.guided_local_search_lambda_coefficient());
      } else {
        optimize = solver_->MakeGuidedLocalSearch(
            false, cost_,
            [this](int64 i, int64 j, int64 k) {
              return GetArcCostForVehicle(i, j, k);
            },
            search_parameters.optimization_step(), nexts_, vehicle_vars_,
            search_parameters.guided_local_search_lambda_coefficient());
      }
      break;
    case LocalSearchMetaheuristic::SIMULATED_ANNEALING:
      optimize = solver_->MakeSimulatedAnnealing(
          false, cost_, search_parameters.optimization_step(), 100);
      break;
    case LocalSearchMetaheuristic::TABU_SEARCH:
      optimize = solver_->MakeTabuSearch(false, cost_,
                                         search_parameters.optimization_step(),
                                         nexts_, 10, 10, .8);
      break;
    case LocalSearchMetaheuristic::OBJECTIVE_TABU_SEARCH:
      optimize = solver_->MakeObjectiveTabuSearch(
          false, cost_, search_parameters.optimization_step(), 100);
      break;
    default:
      optimize =
          solver_->MakeMinimize(cost_, search_parameters.optimization_step());
  }
  monitors_.push_back(optimize);
}

void RoutingModel::SetupAssignmentCollector() {
  Assignment* full_assignment = solver_->MakeAssignment();
  for (const RoutingDimension* const dimension : dimensions_) {
    full_assignment->Add(dimension->cumuls());
  }
  for (IntVar* const extra_var : extra_vars_) {
    full_assignment->Add(extra_var);
  }
  for (IntervalVar* const extra_interval : extra_intervals_) {
    full_assignment->Add(extra_interval);
  }
  full_assignment->Add(nexts_);
  full_assignment->Add(active_);
  full_assignment->Add(vehicle_vars_);
  full_assignment->AddObjective(cost_);

  collect_assignments_ =
      solver_->MakeBestValueSolutionCollector(full_assignment, false);
  monitors_.push_back(collect_assignments_);
}

void RoutingModel::SetupTrace(
    const RoutingSearchParameters& search_parameters) {
  if (search_parameters.log_search()) {
    const int kLogPeriod = 10000;
    monitors_.push_back(solver_->MakeSearchLog(kLogPeriod, cost_));
  }
}

void RoutingModel::SetupSearchMonitors(
    const RoutingSearchParameters& search_parameters) {
  monitors_.push_back(GetOrCreateLimit());
  SetupMetaheuristics(search_parameters);
  SetupAssignmentCollector();
  SetupTrace(search_parameters);
}

bool RoutingModel::UsesLightPropagation(
    const RoutingSearchParameters& search_parameters) const {
  return search_parameters.use_light_propagation() &&
         !search_parameters.use_depth_first_search() &&
         search_parameters.first_solution_strategy() !=
             FirstSolutionStrategy::FIRST_UNBOUND_MIN_VALUE;
}

void RoutingModel::AddVariableMinimizedByFinalizer(IntVar* var) {
  CHECK(var != nullptr);
  variables_minimized_by_finalizer_.push_back(var);
}

void RoutingModel::AddVariableMaximizedByFinalizer(IntVar* var) {
  CHECK(var != nullptr);
  variables_maximized_by_finalizer_.push_back(var);
}

void RoutingModel::SetupSearch(
    const RoutingSearchParameters& search_parameters) {
  SetupDecisionBuilders(search_parameters);
  SetupSearchMonitors(search_parameters);
}

void RoutingModel::AddToAssignment(IntVar* const var) {
  extra_vars_.push_back(var);
}

void RoutingModel::AddIntervalToAssignment(IntervalVar* const interval) {
  extra_intervals_.push_back(interval);
}

RoutingModel::NodeEvaluator2* RoutingModel::NewCachedCallback(
    NodeEvaluator2* callback) {
  const int size = node_to_index_.size();
  if (cache_callbacks_) {
    NodeEvaluator2* cached_evaluator = nullptr;
    if (!FindCopy(cached_node_callbacks_, callback, &cached_evaluator)) {
      cached_evaluator = new RoutingCache(callback, size);
      cached_node_callbacks_[callback] = cached_evaluator;
      // Make sure that both the cache and the base callback get deleted
      // properly.
      owned_node_callbacks_.insert(callback);
      owned_node_callbacks_.insert(cached_evaluator);
    }
    return cached_evaluator;
  } else {
    owned_node_callbacks_.insert(callback);
    return callback;
  }
}

// NewCachedStateDependentCallback returns a new evaluator creating at most one
// RangeIntToIntFunction per pair of nodes. The evaluator manages the cached
// functions, while the routing model takes owenership of both the old and the
// new callbacks.
RoutingModel::VariableNodeEvaluator2*
RoutingModel::NewCachedStateDependentCallback(
    VariableNodeEvaluator2* callback) {
  const int size = node_to_index_.size();
  VariableNodeEvaluator2* cached_evaluator = nullptr;
  if (!FindCopy(cached_state_dependent_callbacks_, callback,
                &cached_evaluator)) {
    cached_evaluator = new StateDependentRoutingCache(callback, size);
    cached_state_dependent_callbacks_[callback] = cached_evaluator;
    owned_state_dependent_callbacks_.insert(callback);
    owned_state_dependent_callbacks_.insert(cached_evaluator);
  }
  return cached_evaluator;
}

// BEGIN(DEPRECATED)
// Deprecated RoutingModel methods. See the .h.
// DON'T REMOVE RASHLY! These methods might still be used by old open-source
// users, even if they are unused at Google.
void RoutingModel::SetCost(NodeEvaluator2* e) {
  SetArcCostEvaluatorOfAllVehicles(e);
}
void RoutingModel::SetVehicleCost(int v, NodeEvaluator2* e) {
  return SetArcCostEvaluatorOfVehicle(e, v);
}
int64 RoutingModel::GetRouteFixedCost() const {
  return GetFixedCostOfVehicle(0);
}
void RoutingModel::SetRouteFixedCost(int64 c) { SetFixedCostOfAllVehicles(c); }
int64 RoutingModel::GetVehicleFixedCost(int v) const {
  return GetFixedCostOfVehicle(v);
}
void RoutingModel::SetVehicleFixedCost(int v, int64 c) {
  SetFixedCostOfVehicle(c, v);
}
bool RoutingModel::homogeneous_costs() const {
  return CostsAreHomogeneousAcrossVehicles();
}
int RoutingModel::GetVehicleCostCount() const {
  return GetNonZeroCostClassesCount();
}
int64 RoutingModel::GetCost(int64 i, int64 j, int64 v) {
  return GetArcCostForVehicle(i, j, v);
}
int64 RoutingModel::GetVehicleClassCost(int64 i, int64 j, int64 c) {
  return GetArcCostForClass(i, j, c);
}
void RoutingModel::SetDimensionTransitCost(const std::string& name, int64 coeff) {
  GetMutableDimension(name)->SetSpanCostCoefficientForAllVehicles(coeff);
}
int64 RoutingModel::GetDimensionTransitCost(const std::string& name) const {
  return HasDimension(name)
             ? GetDimensionOrDie(name).vehicle_span_cost_coefficients()[0]
             : 0;
}
void RoutingModel::SetDimensionSpanCost(const std::string& name, int64 coeff) {
  GetMutableDimension(name)->SetGlobalSpanCostCoefficient(coeff);
}
int64 RoutingModel::GetDimensionSpanCost(const std::string& name) const {
  return HasDimension(name)
             ? GetDimensionOrDie(name).global_span_cost_coefficient()
             : 0;
}
int64 RoutingModel::GetTransitValue(const std::string& dimension_name,
                                    int64 from_index, int64 to_index,
                                    int64 vehicle) const {
  DimensionIndex dimension_index(-1);
  if (FindCopy(dimension_name_to_index_, dimension_name, &dimension_index)) {
    return dimensions_[dimension_index]->GetTransitValue(from_index, to_index,
                                                         vehicle);
  } else {
    return 0;
  }
}
void RoutingModel::SetCumulVarSoftUpperBound(NodeIndex node, const std::string& name,
                                             int64 ub, int64 coeff) {
  GetMutableDimension(name)->SetCumulVarSoftUpperBound(node, ub, coeff);
}
bool RoutingModel::HasCumulVarSoftUpperBound(NodeIndex node,
                                             const std::string& name) const {
  return HasDimension(name) &&
         GetDimensionOrDie(name).HasCumulVarSoftUpperBound(node);
}
int64 RoutingModel::GetCumulVarSoftUpperBound(NodeIndex node,
                                              const std::string& name) const {
  return HasDimension(name)
             ? GetDimensionOrDie(name).GetCumulVarSoftUpperBound(node)
             : kint64max;
}
int64 RoutingModel::GetCumulVarSoftUpperBoundCoefficient(
    NodeIndex node, const std::string& name) const {
  return HasDimension(name)
             ? GetDimensionOrDie(name).GetCumulVarSoftUpperBoundCoefficient(
                   node)
             : 0;
}
void RoutingModel::SetStartCumulVarSoftUpperBound(int vehicle,
                                                  const std::string& name, int64 ub,
                                                  int64 coeff) {
  GetMutableDimension(name)->SetStartCumulVarSoftUpperBound(vehicle, ub, coeff);
}
bool RoutingModel::HasStartCumulVarSoftUpperBound(int vehicle,
                                                  const std::string& name) const {
  return HasDimension(name) &&
         GetDimensionOrDie(name).HasStartCumulVarSoftUpperBound(vehicle);
}
int64 RoutingModel::GetStartCumulVarSoftUpperBound(int vehicle,
                                                   const std::string& name) const {
  return HasDimension(name)
             ? GetDimensionOrDie(name).GetStartCumulVarSoftUpperBound(vehicle)
             : kint64max;
}
int64 RoutingModel::GetStartCumulVarSoftUpperBoundCoefficient(
    int vehicle, const std::string& name) const {
  return HasDimension(name)
             ? GetDimensionOrDie(name)
                   .GetStartCumulVarSoftUpperBoundCoefficient(vehicle)
             : 0;
}
void RoutingModel::SetEndCumulVarSoftUpperBound(int vehicle, const std::string& name,
                                                int64 ub, int64 coeff) {
  GetMutableDimension(name)->SetEndCumulVarSoftUpperBound(vehicle, ub, coeff);
}
bool RoutingModel::HasEndCumulVarSoftUpperBound(int vehicle,
                                                const std::string& name) const {
  return HasDimension(name) &&
         GetDimensionOrDie(name).HasEndCumulVarSoftUpperBound(vehicle);
}
int64 RoutingModel::GetEndCumulVarSoftUpperBound(int vehicle,
                                                 const std::string& name) const {
  return HasDimension(name)
             ? GetDimensionOrDie(name).GetEndCumulVarSoftUpperBound(vehicle)
             : kint64max;
}
int64 RoutingModel::GetEndCumulVarSoftUpperBoundCoefficient(
    int vehicle, const std::string& name) const {
  return HasDimension(name)
             ? GetDimensionOrDie(name).GetEndCumulVarSoftUpperBoundCoefficient(
                   vehicle)
             : 0;
}
IntVar* RoutingModel::CumulVar(int64 index, const std::string& name) const {
  return HasDimension(name) ? GetDimensionOrDie(name).CumulVar(index) : nullptr;
}
IntVar* RoutingModel::TransitVar(int64 index, const std::string& name) const {
  return HasDimension(name) ? GetDimensionOrDie(name).TransitVar(index)
                            : nullptr;
}
IntVar* RoutingModel::SlackVar(int64 index, const std::string& name) const {
  return HasDimension(name) ? GetDimensionOrDie(name).SlackVar(index) : nullptr;
}

// END(DEPRECATED)

const char RoutingModelVisitor::kLightElement[] = "LightElement";
const char RoutingModelVisitor::kLightElement2[] = "LightElement2";

RoutingDimension::RoutingDimension(RoutingModel* model,
                                   std::vector<int64> vehicle_capacities,
                                   const std::string& name,
                                   const RoutingDimension* base_dimension)
    : vehicle_capacities_(std::move(vehicle_capacities)),
      base_dimension_(base_dimension),
      global_span_cost_coefficient_(0),
      model_(model),
      name_(name) {
  CHECK(model != nullptr);
  vehicle_span_upper_bounds_.assign(model->vehicles(), kint64max);
  vehicle_span_cost_coefficients_.assign(model->vehicles(), 0);
}

RoutingDimension::RoutingDimension(RoutingModel* model,
                                   std::vector<int64> vehicle_capacities,
                                   const std::string& name, SelfBased)
    : RoutingDimension(model, std::move(vehicle_capacities), name, this) {}

void RoutingDimension::Initialize(
    const std::vector<RoutingModel::NodeEvaluator2*>& transit_evaluators,
    const std::vector<RoutingModel::VariableNodeEvaluator2*>&
        state_dependent_node_evaluators,
    int64 slack_max) {
  InitializeCumuls();
  InitializeTransits(transit_evaluators, state_dependent_node_evaluators,
                     slack_max);
}

namespace {
// Very light version of the RangeLessOrEqual constraint (see ./range_cst.cc).
// Only performs initial propagation and then checks the compatibility of the
// variable domains without domain pruning.
// This is useful when to avoid ping-pong effects with costly constraints
// such as the PathCumul constraint.
// This constraint has not been added to the cp library (in range_cst.cc) given
// it only does checking and no propagation (except the initial propagation)
// and is only fit for local search, in particular in the context of vehicle
// routing.
class LightRangeLessOrEqual : public Constraint {
 public:
  LightRangeLessOrEqual(Solver* const s, IntExpr* const l, IntExpr* const r);
  ~LightRangeLessOrEqual() override {}
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  IntVar* Var() override {
    return solver()->MakeIsLessOrEqualVar(left_, right_);
  }
  // TODO(user): introduce a kLightLessOrEqual tag.
  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitConstraint(ModelVisitor::kLessOrEqual, this);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kLeftArgument, left_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kRightArgument,
                                            right_);
    visitor->EndVisitConstraint(ModelVisitor::kLessOrEqual, this);
  }

 private:
  void CheckRange();

  IntExpr* const left_;
  IntExpr* const right_;
  Demon* demon_;
};

LightRangeLessOrEqual::LightRangeLessOrEqual(Solver* const s, IntExpr* const l,
                                             IntExpr* const r)
    : Constraint(s), left_(l), right_(r), demon_(nullptr) {}

void LightRangeLessOrEqual::Post() {
  demon_ = MakeConstraintDemon0(
      solver(), this, &LightRangeLessOrEqual::CheckRange, "CheckRange");
  left_->WhenRange(demon_);
  right_->WhenRange(demon_);
}

void LightRangeLessOrEqual::InitialPropagate() {
  left_->SetMax(right_->Max());
  right_->SetMin(left_->Min());
  if (left_->Max() <= right_->Min()) {
    demon_->inhibit(solver());
  }
}

void LightRangeLessOrEqual::CheckRange() {
  if (left_->Min() > right_->Max()) {
    solver()->Fail();
  }
  if (left_->Max() <= right_->Min()) {
    demon_->inhibit(solver());
  }
}

std::string LightRangeLessOrEqual::DebugString() const {
  return left_->DebugString() + " < " + right_->DebugString();
}

}  // namespace

void RoutingDimension::InitializeCumuls() {
  Solver* const solver = model_->solver();
  const int size = model_->Size() + model_->vehicles();
  const auto capacity_range = std::minmax_element(vehicle_capacities_.begin(),
                                                  vehicle_capacities_.end());
  const int64 min_capacity = *capacity_range.first;
  CHECK_GE(min_capacity, 0);
  const int64 max_capacity = *capacity_range.second;
  solver->MakeIntVarArray(size, 0LL, max_capacity, name_, &cumuls_);
  forbidden_intervals_.resize(size);
  capacity_vars_.clear();
  if (min_capacity != max_capacity) {
    solver->MakeIntVarArray(size, 0LL, kint64max, &capacity_vars_);
    for (int i = 0; i < size; ++i) {
      IntVar* const capacity_var = capacity_vars_[i];
      if (i < model_->Size()) {
        IntVar* const capacity_active = solver->MakeBoolVar();
        solver->AddConstraint(
            solver->MakeLessOrEqual(model_->ActiveVar(i), capacity_active));
        solver->AddConstraint(solver->MakeIsLessOrEqualCt(
            cumuls_[i], capacity_var, capacity_active));
      } else {
        solver->AddConstraint(
            solver->MakeLessOrEqual(cumuls_[i], capacity_var));
      }
    }
  }
}

namespace {
template <typename NodeEvaluator>
auto WrappedEvaluator(RoutingModel* model, NodeEvaluator* evaluator, int64 from,
                      int64 to)
    -> decltype(evaluator->Run(model->IndexToNode(to),
                               model->IndexToNode(to))) {
  CHECK(evaluator != nullptr);
  return evaluator->Run(model->IndexToNode(from), model->IndexToNode(to));
}

template <int64 value>
int64 IthElementOrValue(const std::vector<int64>& v, int64 index) {
  return index >= 0 ? v[index] : value;
}

template <typename NodeEvaluator, typename TransitEvaluator>
void ComputeTransitClasses(const std::vector<NodeEvaluator*>& node_evaluators,
                           RoutingModel* const model,
                           std::vector<TransitEvaluator>* class_evaluators,
                           std::vector<int64>* vehicle_to_class) {
  CHECK(model != nullptr);
  CHECK(class_evaluators != nullptr);
  CHECK(vehicle_to_class != nullptr);
  class_evaluators->clear();
  vehicle_to_class->resize(node_evaluators.size(), -1);
  std::unordered_map<NodeEvaluator*, int64> evaluator_to_class;
  for (int i = 0; i < node_evaluators.size(); ++i) {
    NodeEvaluator* const evaluator = node_evaluators[i];
    int evaluator_class = -1;
    if (!FindCopy(evaluator_to_class, evaluator, &evaluator_class)) {
      evaluator_class = class_evaluators->size();
      evaluator_to_class[evaluator] = evaluator_class;
      class_evaluators->emplace_back([model, evaluator](int64 from, int64 to) {
        DCHECK(evaluator != nullptr);
        return evaluator->Run(model->IndexToNode(from), model->IndexToNode(to));
      });
    }
    (*vehicle_to_class)[i] = evaluator_class;
  }
}
}  // namespace

void RoutingDimension::InitializeTransits(
    const std::vector<RoutingModel::NodeEvaluator2*>& node_evaluators,
    const std::vector<RoutingModel::VariableNodeEvaluator2*>&
        state_dependent_node_evaluators,
    int64 slack_max) {
  CHECK_EQ(model_->vehicles(), node_evaluators.size());
  CHECK(base_dimension_ == nullptr ||
        model_->vehicles() == state_dependent_node_evaluators.size());
  for (int64 index = 0; index < model_->vehicles(); ++index) {
    CHECK(node_evaluators[index] != nullptr);
    CHECK(node_evaluators[index]->IsRepeatable());
    CHECK(base_dimension_ == nullptr ||
          state_dependent_node_evaluators[index] != nullptr);
    CHECK(base_dimension_ == nullptr ||
          state_dependent_node_evaluators[index]->IsRepeatable());
  }
  Solver* const solver = model_->solver();
  const int size = model_->Size();
  transits_.resize(size, nullptr);
  fixed_transits_.resize(size, nullptr);
  slacks_.resize(size, nullptr);
  dependent_transits_.resize(size, nullptr);
  ComputeTransitClasses(node_evaluators, model_, &class_evaluators_,
                        &vehicle_to_class_);
  if (base_dimension_ != nullptr) {
    ComputeTransitClasses(state_dependent_node_evaluators, model_,
                          &state_dependent_class_evaluators_,
                          &state_dependent_vehicle_to_class_);
  }

  const Solver::IndexEvaluator1 dependent_vehicle_class_function =
      [this](int index) {
        return (0 <= index && index < state_dependent_vehicle_to_class_.size())
                   ? state_dependent_vehicle_to_class_[index]
                   : state_dependent_class_evaluators_.size();
      };

  CHECK(!class_evaluators_.empty());
  CHECK(base_dimension_ == nullptr ||
        !state_dependent_class_evaluators_.empty());
  const std::string slack_name = name_ + " slack";
  const std::string transit_name = name_ + " fixed transit";

  for (int64 i = 0; i < size; ++i) {
    fixed_transits_[i] = solver->MakeIntVar(kint64min, kint64max, transit_name);
    // Setting dependent_transits_[i].
    if (base_dimension_ != nullptr) {
      if (state_dependent_class_evaluators_.size() == 1) {
        std::vector<IntVar*> transition_variables(cumuls_.size(), nullptr);
        for (int64 j = 0; j < cumuls_.size(); ++j) {
          transition_variables[j] =
              MakeRangeMakeElementExpr(
                  state_dependent_class_evaluators_[0](i, j).transit,
                  base_dimension_->CumulVar(i), solver)
                  ->Var();
        }
        dependent_transits_[i] =
            solver->MakeElement(transition_variables, model_->NextVar(i))
                ->Var();
      } else {
        IntVar* const vehicle_class_var =
            solver
                ->MakeElement(dependent_vehicle_class_function,
                              model_->VehicleVar(i))
                ->Var();
        std::vector<IntVar*> transit_for_vehicle;
        transit_for_vehicle.reserve(state_dependent_class_evaluators_.size() +
                                    1);
        for (const auto& evaluator : state_dependent_class_evaluators_) {
          std::vector<IntVar*> transition_variables(cumuls_.size(), nullptr);
          for (int64 j = 0; j < cumuls_.size(); ++j) {
            transition_variables[j] =
                MakeRangeMakeElementExpr(evaluator(i, j).transit,
                                         base_dimension_->CumulVar(i), solver)
                    ->Var();
          }
          transit_for_vehicle.push_back(
              solver->MakeElement(transition_variables, model_->NextVar(i))
                  ->Var());
        }
        transit_for_vehicle.push_back(solver->MakeIntConst(Zero()));
        dependent_transits_[i] =
            solver->MakeElement(transit_for_vehicle, vehicle_class_var)->Var();
      }
    } else {
      dependent_transits_[i] = solver->MakeIntConst(Zero());
    }

    // Summing fixed transits, dependent transits and the slack.
    IntExpr* transit_expr = fixed_transits_[i];
    if (dependent_transits_[i]->Min() != 0 ||
        dependent_transits_[i]->Max() != 0) {
      transit_expr = solver->MakeSum(transit_expr, dependent_transits_[i]);
    }

    if (slack_max == 0) {
      slacks_[i] = solver->MakeIntConst(Zero());
    } else {
      slacks_[i] = solver->MakeIntVar(0, slack_max, slack_name);
      transit_expr = solver->MakeSum(slacks_[i], transit_expr);
    }
    transits_[i] = transit_expr->Var();
  }
}

namespace {

// Break constraint ensures break intervals fit on the route of a vehicle.
// It posts a disjunction constraint on break intervals + intervals
// corresponding to route nodes. For each node, |break_intervals|+1 intervals
// are created representing the fixed transit after the node; the transit
// can therefore be interrupted at most |break_intervals|+1 times. The
// constraint ensures that the sum of the duration of the "node" intervals
// is at least the value of the fixed transit after the node.
class BreakConstraint : public Constraint {
 public:
  BreakConstraint(const RoutingDimension* dimension, int vehicle,
                  std::vector<IntervalVar*> break_intervals)
      : Constraint(dimension->model()->solver()),
        dimension_(dimension),
        vehicle_(vehicle),
        break_intervals_(std::move(break_intervals)),
        status_(solver()->MakeBoolVar(StrCat("status", vehicle))) {}
  void Post() override {
    RoutingModel* const model = dimension_->model();
    solver()->AddConstraint(
        solver()->MakePathConnected(model->Nexts(), {model->Start(vehicle_)},
                                    {model->End(vehicle_)}, {status_}));
    status_->WhenBound(MakeDelayedConstraintDemon0(
        solver(), this, &BreakConstraint::PathClosed, "PathClosed"));
  }
  void InitialPropagate() override {
    if (status_->Bound()) {
      PathClosed();
    }
  }

 private:
  void PathClosed() {
    // TODO(user): Make sure that 0 duration intervals are pushed at the end
    // of the list.
    if (status_->Max() == 0) {
      for (IntervalVar* const break_interval : break_intervals_) {
        break_interval->SetPerformed(false);
      }
    } else {
      RoutingModel* const model = dimension_->model();
      int64 current = model->Start(vehicle_);
      std::vector<IntervalVar*> vehicle_intervals = break_intervals_;
      while (!model->IsEnd(current)) {
        const int next = model->NextVar(current)->Min();
        std::vector<IntervalVar*> transit_intervals;
        IntervalVar* last = nullptr;
        for (int i = 0; i <= break_intervals_.size(); ++i) {
          IntervalVar* const interval = solver()->MakeIntervalVar(
              dimension_->CumulVar(current)->Min(),
              dimension_->CumulVar(next)->Max(), 0,
              dimension_->FixedTransitVar(current)->Value(), 0, kint64max,
              false, StrCat(current, "-", i));
          transit_intervals.push_back(interval);
          vehicle_intervals.push_back(interval);
          // Order transit intervals to cut symmetries.
          if (last != nullptr) {
            solver()->AddConstraint(solver()->MakeIntervalVarRelation(
                interval, Solver::STARTS_AFTER_END, last));
            last = interval;
          }
        }
        std::vector<IntVar*> durations(transit_intervals.size());
        for (int i = 0; i < transit_intervals.size(); ++i) {
          durations[i] = transit_intervals[i]->DurationExpr()->Var();
          if (i == 0) {
            solver()->AddConstraint(
                solver()->MakeEquality(transit_intervals[i]->StartExpr(),
                                       dimension_->CumulVar(current)));
          } else {
            solver()->AddConstraint(
                solver()->MakeGreaterOrEqual(transit_intervals[i]->StartExpr(),
                                             dimension_->CumulVar(current)));
          }
          if (i == break_intervals_.size()) {
            solver()->AddConstraint(solver()->MakeEquality(
                dimension_->CumulVar(next), transit_intervals[i]->EndExpr()));
          } else {
            solver()->AddConstraint(solver()->MakeGreaterOrEqual(
                dimension_->CumulVar(next), transit_intervals[i]->EndExpr()));
          }
        }
        solver()->AddConstraint(solver()->MakeGreaterOrEqual(
            solver()->MakeSum(durations),
            dimension_->FixedTransitVar(current)->Value()));
        current = next;
      }
      solver()->AddConstraint(solver()->MakeStrictDisjunctiveConstraint(
          vehicle_intervals, StrCat("Vehicle breaks ", vehicle_)));
    }
  }

  const RoutingDimension* const dimension_;
  const int vehicle_;
  const std::vector<IntervalVar*> break_intervals_;
  IntVar* const status_;
};

Constraint* MakeBreakConstraint(const RoutingDimension* dimension, int vehicle,
                                std::vector<IntervalVar*> break_intervals) {
  Solver* const solver = dimension->model()->solver();
  return solver->RevAlloc(
      new BreakConstraint(dimension, vehicle, std::move(break_intervals)));
}

}  // namespace

void RoutingDimension::CloseModel(bool use_light_propagation) {
  Solver* const solver = model_->solver();
  const auto capacity_lambda = [this](int64 vehicle) {
    return vehicle >= 0 ? vehicle_capacities_[vehicle] : kint64max;
  };
  for (int i = 0; i < capacity_vars_.size(); ++i) {
    IntVar* const vehicle_var = model_->VehicleVar(i);
    IntVar* const capacity_var = capacity_vars_[i];
    if (use_light_propagation) {
      solver->AddConstraint(
          MakeLightElement(solver, capacity_var, vehicle_var, capacity_lambda));
    } else {
      solver->AddConstraint(solver->MakeEquality(
          capacity_var,
          solver->MakeElement(capacity_lambda, vehicle_var)->Var()));
    }
  }
  const Solver::IndexEvaluator1 vehicle_class_function = [this](int index) {
    return IthElementOrValue<-1>(vehicle_to_class_, index);
  };
  for (int i = 0; i < fixed_transits_.size(); ++i) {
    IntVar* const next_var = model_->NextVar(i);
    IntVar* const fixed_transit = fixed_transits_[i];
    const auto transit_vehicle_evaluator = [this, i](int64 to,
                                                     int64 eval_index) {
      return eval_index >= 0
                 ? class_evaluators_[vehicle_to_class_[eval_index]](i, to)
                 : 0LL;
    };
    if (use_light_propagation) {
      if (class_evaluators_.size() == 1) {
        solver->AddConstraint(MakeLightElement(
            solver, fixed_transit, next_var,
            [this, i](int64 to) { return class_evaluators_[0](i, to); }));
      } else {
        solver->AddConstraint(MakeLightElement2(solver, fixed_transit, next_var,
                                                model_->VehicleVar(i),
                                                transit_vehicle_evaluator));
      }
    } else {
      if (class_evaluators_.size() == 1) {
        solver->AddConstraint(solver->MakeEquality(
            fixed_transit,
            solver
                ->MakeElement(
                    [this, i](int64 to) { return class_evaluators_[0](i, to); },
                    model_->NextVar(i))
                ->Var()));
      } else {
        IntVar* const vehicle_class_var =
            solver->MakeElement(vehicle_class_function, model_->VehicleVar(i))
                ->Var();
        solver->AddConstraint(solver->MakeEquality(
            fixed_transit, solver
                               ->MakeElement(transit_vehicle_evaluator,
                                             next_var, vehicle_class_var)
                               ->Var()));
      }
    }
  }
}

int64 RoutingDimension::GetTransitValue(int64 from_index, int64 to_index,
                                        int64 vehicle) const {
  DCHECK(class_evaluators_[vehicle_to_class_[vehicle]] != nullptr);
  return class_evaluators_[vehicle_to_class_[vehicle]](from_index, to_index);
}

void RoutingDimension::SetSpanUpperBoundForVehicle(int64 upper_bound,
                                                   int vehicle) {
  CHECK_GE(vehicle, 0);
  CHECK_LT(vehicle, vehicle_span_upper_bounds_.size());
  CHECK_GE(upper_bound, 0);
  vehicle_span_upper_bounds_[vehicle] = upper_bound;
  Solver* const solver = model_->solver();
  IntVar* const start = cumuls_[model_->Start(vehicle)];
  IntVar* const end = cumuls_[model_->End(vehicle)];
  solver->AddConstraint(
      solver->MakeLessOrEqual(solver->MakeDifference(end, start), upper_bound));
}

void RoutingDimension::SetSpanCostCoefficientForVehicle(int64 coefficient,
                                                        int vehicle) {
  CHECK_GE(vehicle, 0);
  CHECK_LT(vehicle, vehicle_span_cost_coefficients_.size());
  CHECK_GE(coefficient, 0);
  vehicle_span_cost_coefficients_[vehicle] = coefficient;
}

void RoutingDimension::SetSpanCostCoefficientForAllVehicles(int64 coefficient) {
  CHECK_GE(coefficient, 0);
  vehicle_span_cost_coefficients_.assign(model_->vehicles(), coefficient);
}

void RoutingDimension::SetGlobalSpanCostCoefficient(int64 coefficient) {
  CHECK_GE(coefficient, 0);
  global_span_cost_coefficient_ = coefficient;
}

void RoutingDimension::SetCumulVarSoftUpperBound(RoutingModel::NodeIndex node,
                                                 int64 upper_bound,
                                                 int64 coefficient) {
  if (model_->HasIndex(node)) {
    const int64 index = model_->NodeToIndex(node);
    if (!model_->IsStart(index) && !model_->IsEnd(index)) {
      SetCumulVarSoftUpperBoundFromIndex(index, upper_bound, coefficient);
      return;
    }
  }
  VLOG(2) << "Cannot set soft upper bound on start or end nodes";
}

bool RoutingDimension::HasCumulVarSoftUpperBound(
    RoutingModel::NodeIndex node) const {
  if (model_->HasIndex(node)) {
    const int64 index = model_->NodeToIndex(node);
    if (!model_->IsStart(index) && !model_->IsEnd(index)) {
      return HasCumulVarSoftUpperBoundFromIndex(index);
    }
  }
  VLOG(2) << "Cannot get soft upper bound on start or end nodes";
  return false;
}

int64 RoutingDimension::GetCumulVarSoftUpperBound(
    RoutingModel::NodeIndex node) const {
  if (model_->HasIndex(node)) {
    const int64 index = model_->NodeToIndex(node);
    if (!model_->IsStart(index) && !model_->IsEnd(index)) {
      return GetCumulVarSoftUpperBoundFromIndex(index);
    }
  }
  VLOG(2) << "Cannot get soft upper bound on start or end nodes";
  return kint64max;
}

int64 RoutingDimension::GetCumulVarSoftUpperBoundCoefficient(
    RoutingModel::NodeIndex node) const {
  if (model_->HasIndex(node)) {
    const int64 index = model_->NodeToIndex(node);
    if (!model_->IsStart(index) && !model_->IsEnd(index)) {
      return GetCumulVarSoftUpperBoundCoefficientFromIndex(index);
    }
  }
  VLOG(2) << "Cannot get soft upper bound on start or end nodes";
  return 0;
}

void RoutingDimension::SetStartCumulVarSoftUpperBound(int vehicle,
                                                      int64 upper_bound,
                                                      int64 coefficient) {
  SetCumulVarSoftUpperBoundFromIndex(model_->Start(vehicle), upper_bound,
                                     coefficient);
}

bool RoutingDimension::HasStartCumulVarSoftUpperBound(int vehicle) const {
  return HasCumulVarSoftUpperBoundFromIndex(model_->Start(vehicle));
}

int64 RoutingDimension::GetStartCumulVarSoftUpperBound(int vehicle) const {
  return GetCumulVarSoftUpperBoundFromIndex(model_->Start(vehicle));
}

int64 RoutingDimension::GetStartCumulVarSoftUpperBoundCoefficient(
    int vehicle) const {
  return GetCumulVarSoftUpperBoundCoefficientFromIndex(model_->Start(vehicle));
}

void RoutingDimension::SetEndCumulVarSoftUpperBound(int vehicle,
                                                    int64 upper_bound,
                                                    int64 coefficient) {
  SetCumulVarSoftUpperBoundFromIndex(model_->End(vehicle), upper_bound,
                                     coefficient);
}

bool RoutingDimension::HasEndCumulVarSoftUpperBound(int vehicle) const {
  return HasCumulVarSoftUpperBoundFromIndex(model_->End(vehicle));
}

int64 RoutingDimension::GetEndCumulVarSoftUpperBound(int vehicle) const {
  return GetCumulVarSoftUpperBoundFromIndex(model_->End(vehicle));
}

int64 RoutingDimension::GetEndCumulVarSoftUpperBoundCoefficient(
    int vehicle) const {
  return GetCumulVarSoftUpperBoundCoefficientFromIndex(model_->End(vehicle));
}

void RoutingDimension::SetCumulVarSoftUpperBoundFromIndex(int64 index,
                                                          int64 upper_bound,
                                                          int64 coefficient) {
  if (index >= cumul_var_soft_upper_bound_.size()) {
    cumul_var_soft_upper_bound_.resize(index + 1);
  }
  SoftBound* const soft_upper_bound = &cumul_var_soft_upper_bound_[index];
  soft_upper_bound->var = cumuls_[index];
  soft_upper_bound->bound = upper_bound;
  soft_upper_bound->coefficient = coefficient;
}

bool RoutingDimension::HasCumulVarSoftUpperBoundFromIndex(int64 index) const {
  return (index < cumul_var_soft_upper_bound_.size() &&
          cumul_var_soft_upper_bound_[index].var != nullptr);
}

int64 RoutingDimension::GetCumulVarSoftUpperBoundFromIndex(int64 index) const {
  if (index < cumul_var_soft_upper_bound_.size() &&
      cumul_var_soft_upper_bound_[index].var != nullptr) {
    return cumul_var_soft_upper_bound_[index].bound;
  }
  return cumuls_[index]->Max();
}

int64 RoutingDimension::GetCumulVarSoftUpperBoundCoefficientFromIndex(
    int64 index) const {
  if (index < cumul_var_soft_upper_bound_.size() &&
      cumul_var_soft_upper_bound_[index].var != nullptr) {
    return cumul_var_soft_upper_bound_[index].coefficient;
  }
  return 0;
}

void RoutingDimension::SetupCumulVarSoftUpperBoundCosts(
    std::vector<IntVar*>* cost_elements) const {
  CHECK(cost_elements != nullptr);
  Solver* const solver = model_->solver();
  for (int i = 0; i < cumul_var_soft_upper_bound_.size(); ++i) {
    const SoftBound& soft_bound = cumul_var_soft_upper_bound_[i];
    if (soft_bound.var != nullptr) {
      IntExpr* const expr = solver->MakeSemiContinuousExpr(
          solver->MakeSum(soft_bound.var, -soft_bound.bound), 0,
          soft_bound.coefficient);
      IntVar* cost_var = nullptr;
      if (model_->IsEnd(i)) {
        // No active variable for end nodes, always active.
        cost_var = expr->Var();
      } else {
        cost_var = solver->MakeProd(expr, model_->ActiveVar(i))->Var();
      }
      cost_elements->push_back(cost_var);
      // TODO(user): Check if it wouldn't be better to minimize
      // soft_bound.var here.
      model_->AddVariableMinimizedByFinalizer(cost_var);
    }
  }
}

void RoutingDimension::SetCumulVarSoftLowerBound(RoutingModel::NodeIndex node,
                                                 int64 lower_bound,
                                                 int64 coefficient) {
  if (model_->HasIndex(node)) {
    const int64 index = model_->NodeToIndex(node);
    if (!model_->IsStart(index) && !model_->IsEnd(index)) {
      SetCumulVarSoftLowerBoundFromIndex(index, lower_bound, coefficient);
      return;
    }
  }
  VLOG(2) << "Cannot set soft lower bound on start or end nodes";
}

bool RoutingDimension::HasCumulVarSoftLowerBound(
    RoutingModel::NodeIndex node) const {
  if (model_->HasIndex(node)) {
    const int64 index = model_->NodeToIndex(node);
    if (!model_->IsStart(index) && !model_->IsEnd(index)) {
      return HasCumulVarSoftLowerBoundFromIndex(index);
    }
  }
  VLOG(2) << "Cannot get soft lower bound on start or end nodes";
  return false;
}

int64 RoutingDimension::GetCumulVarSoftLowerBound(
    RoutingModel::NodeIndex node) const {
  if (model_->HasIndex(node)) {
    const int64 index = model_->NodeToIndex(node);
    if (!model_->IsStart(index) && !model_->IsEnd(index)) {
      return GetCumulVarSoftLowerBoundFromIndex(index);
    }
  }
  VLOG(2) << "Cannot get soft lower bound on start or end nodes";
  return 0;
}
int64 RoutingDimension::GetCumulVarSoftLowerBoundCoefficient(
    RoutingModel::NodeIndex node) const {
  if (model_->HasIndex(node)) {
    const int64 index = model_->NodeToIndex(node);
    if (!model_->IsStart(index) && !model_->IsEnd(index)) {
      return GetCumulVarSoftLowerBoundCoefficientFromIndex(index);
    }
  }
  VLOG(2) << "Cannot get soft lower bound on start or end nodes";
  return 0;
}

void RoutingDimension::SetStartCumulVarSoftLowerBound(int vehicle,
                                                      int64 lower_bound,
                                                      int64 coefficient) {
  SetCumulVarSoftLowerBoundFromIndex(model_->Start(vehicle), lower_bound,
                                     coefficient);
}

bool RoutingDimension::HasStartCumulVarSoftLowerBound(int vehicle) const {
  return HasCumulVarSoftLowerBoundFromIndex(model_->Start(vehicle));
}

int64 RoutingDimension::GetStartCumulVarSoftLowerBound(int vehicle) const {
  return GetCumulVarSoftLowerBoundFromIndex(model_->Start(vehicle));
}

int64 RoutingDimension::GetStartCumulVarSoftLowerBoundCoefficient(
    int vehicle) const {
  return GetCumulVarSoftLowerBoundCoefficientFromIndex(model_->Start(vehicle));
}

void RoutingDimension::SetEndCumulVarSoftLowerBound(int vehicle,
                                                    int64 lower_bound,
                                                    int64 coefficient) {
  SetCumulVarSoftLowerBoundFromIndex(model_->End(vehicle), lower_bound,
                                     coefficient);
}

bool RoutingDimension::HasEndCumulVarSoftLowerBound(int vehicle) const {
  return HasCumulVarSoftLowerBoundFromIndex(model_->End(vehicle));
}

int64 RoutingDimension::GetEndCumulVarSoftLowerBound(int vehicle) const {
  return GetCumulVarSoftLowerBoundFromIndex(model_->End(vehicle));
}

int64 RoutingDimension::GetEndCumulVarSoftLowerBoundCoefficient(
    int vehicle) const {
  return GetCumulVarSoftLowerBoundCoefficientFromIndex(model_->End(vehicle));
}

void RoutingDimension::SetCumulVarSoftLowerBoundFromIndex(int64 index,
                                                          int64 lower_bound,
                                                          int64 coefficient) {
  if (index >= cumul_var_soft_lower_bound_.size()) {
    cumul_var_soft_lower_bound_.resize(index + 1);
  }
  SoftBound* const soft_lower_bound = &cumul_var_soft_lower_bound_[index];
  soft_lower_bound->var = cumuls_[index];
  soft_lower_bound->bound = lower_bound;
  soft_lower_bound->coefficient = coefficient;
}

bool RoutingDimension::HasCumulVarSoftLowerBoundFromIndex(int64 index) const {
  return (index < cumul_var_soft_lower_bound_.size() &&
          cumul_var_soft_lower_bound_[index].var != nullptr);
}

int64 RoutingDimension::GetCumulVarSoftLowerBoundFromIndex(int64 index) const {
  if (index < cumul_var_soft_lower_bound_.size() &&
      cumul_var_soft_lower_bound_[index].var != nullptr) {
    return cumul_var_soft_lower_bound_[index].bound;
  }
  return cumuls_[index]->Min();
}

int64 RoutingDimension::GetCumulVarSoftLowerBoundCoefficientFromIndex(
    int64 index) const {
  if (index < cumul_var_soft_lower_bound_.size() &&
      cumul_var_soft_lower_bound_[index].var != nullptr) {
    return cumul_var_soft_lower_bound_[index].coefficient;
  }
  return 0;
}

void RoutingDimension::SetupCumulVarSoftLowerBoundCosts(
    std::vector<IntVar*>* cost_elements) const {
  CHECK(cost_elements != nullptr);
  Solver* const solver = model_->solver();
  for (int i = 0; i < cumul_var_soft_lower_bound_.size(); ++i) {
    const SoftBound& soft_bound = cumul_var_soft_lower_bound_[i];
    if (soft_bound.var != nullptr) {
      IntExpr* const expr = solver->MakeSemiContinuousExpr(
          solver->MakeDifference(soft_bound.bound, soft_bound.var), 0,
          soft_bound.coefficient);
      IntVar* cost_var = nullptr;
      if (model_->IsEnd(i)) {
        // No active variable for end nodes, always active.
        cost_var = expr->Var();
      } else {
        cost_var = solver->MakeProd(expr, model_->ActiveVar(i))->Var();
      }
      cost_elements->push_back(cost_var);
      model_->AddVariableMaximizedByFinalizer(soft_bound.var);
    }
  }
}

void RoutingDimension::SetupGlobalSpanCost(
    std::vector<IntVar*>* cost_elements) const {
  CHECK(cost_elements != nullptr);
  Solver* const solver = model_->solver();
  if (global_span_cost_coefficient_ != 0) {
    std::vector<IntVar*> end_cumuls;
    for (int i = 0; i < model_->vehicles(); ++i) {
      end_cumuls.push_back(cumuls_[model_->End(i)]);
    }
    IntVar* const max_end_cumul = solver->MakeMax(end_cumuls)->Var();
    model_->AddVariableMinimizedByFinalizer(max_end_cumul);
    std::vector<IntVar*> start_cumuls;
    for (int i = 0; i < model_->vehicles(); ++i) {
      start_cumuls.push_back(cumuls_[model_->Start(i)]);
    }
    IntVar* const min_start_cumul = solver->MakeMin(start_cumuls)->Var();
    model_->AddVariableMaximizedByFinalizer(min_start_cumul);
    cost_elements->push_back(
        solver
            ->MakeProd(solver->MakeDifference(max_end_cumul, min_start_cumul),
                       global_span_cost_coefficient_)
            ->Var());
  }
}

void RoutingDimension::SetBreakIntervalsOfVehicle(
    std::vector<IntervalVar*> breaks, int vehicle) {
  if (!breaks.empty()) {
    for (IntervalVar* const interval : breaks) {
      model_->AddIntervalToAssignment(interval);
      model_->AddVariableMinimizedByFinalizer(
          interval->SafeStartExpr(0)->Var());
    }
    model_->solver()->AddConstraint(
        MakeBreakConstraint(this, vehicle, std::move(breaks)));
  }
}

void RoutingDimension::SetupSlackAndDependentTransitCosts(
    std::vector<IntVar*>* cost_elements) const {
  if (model_->vehicles() == 0) return;
  // Figure out whether all vehicles have the same span cost coefficient or not.
  bool all_vehicle_span_costs_are_equal = true;
  for (int i = 1; i < model_->vehicles(); ++i) {
    all_vehicle_span_costs_are_equal &= vehicle_span_cost_coefficients_[i] ==
                                        vehicle_span_cost_coefficients_[0];
  }

  if (all_vehicle_span_costs_are_equal &&
      vehicle_span_cost_coefficients_[0] == 0) {
    return;  // No vehicle span cost.
  }

  // Make sure that the vehicle's start cumul will be maximized in the end;
  // and that the vehicle's end cumul and the node's slacks will be minimized.
  // Note that we don't do that if there was no span cost (see the return
  // clause above), because in that case we want the dimension cumul to
  // remain unconstrained. Since transitions depend on base dimensions, we
  // have to make sure the slacks of base dimensions are taken care of.
  // Also, it makes more sense to make decisions from the root of the tree
  // towards to leaves, and hence the slacks are pushed in reverse order.
  std::vector<const RoutingDimension*> dimensions_with_relevant_slacks = {this};
  while (true) {
    const RoutingDimension* next =
        dimensions_with_relevant_slacks.back()->base_dimension_;
    if (next == nullptr || next == dimensions_with_relevant_slacks.back()) {
      break;
    }
    dimensions_with_relevant_slacks.push_back(next);
  }

  for (auto it = dimensions_with_relevant_slacks.rbegin();
       it != dimensions_with_relevant_slacks.rend(); ++it) {
    for (int i = 0; i < model_->vehicles(); ++i) {
      model_->AddVariableMaximizedByFinalizer((*it)->cumuls_[model_->Start(i)]);
      model_->AddVariableMinimizedByFinalizer((*it)->cumuls_[model_->End(i)]);
    }
    for (IntVar* const slack : (*it)->slacks_) {
      model_->AddVariableMinimizedByFinalizer(slack);
    }
  }

  // Add the span cost element for the slacks (the transit component is already
  // taken into account by the arc cost callbacks like GetArcCostForVehicle()).
  CHECK(cost_elements != nullptr);
  Solver* const solver = model_->solver();

  for (int var_index = 0; var_index < model_->Size(); ++var_index) {
    if (all_vehicle_span_costs_are_equal) {
      cost_elements->push_back(
          solver
              ->MakeProd(solver->MakeProd(
                             solver->MakeSum(slacks_[var_index],
                                             dependent_transits_[var_index]),
                             vehicle_span_cost_coefficients_[0]),
                         model_->ActiveVar(var_index))
              ->Var());
    } else {
      IntVar* cost_coefficient_var =
          solver
              ->MakeElement(
                  [this](int index) {
                    return IthElementOrValue<0LL>(
                        vehicle_span_cost_coefficients_, index);
                  },
                  model_->VehicleVar(var_index))
              ->Var();
      cost_elements->push_back(
          solver
              ->MakeProd(solver->MakeSum(slacks_[var_index],
                                         dependent_transits_[var_index]),
                         cost_coefficient_var)
              ->Var());
      // TODO(user): verify that there isn't any benefit in explicitly making
      // a product with ActiveVar(). There shouldn't be, since ActiveVar() == 0
      // should be equivalent to VehicleVar() == -1.
    }
  }
}
}  // namespace operations_research
