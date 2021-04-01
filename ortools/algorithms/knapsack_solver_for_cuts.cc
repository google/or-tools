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

#include "ortools/algorithms/knapsack_solver_for_cuts.h"

#include <algorithm>
#include <cstdint>
#include <queue>
#include <utility>

#include "ortools/base/logging.h"

namespace operations_research {
namespace {

const int kNoSelection(-1);
const int kDefaultMasterPropagatorId(0);
const double kInfinity = std::numeric_limits<double>::infinity();

// Comparator used to sort item in decreasing efficiency order
// (see KnapsackCapacityPropagator).
struct CompareKnapsackItemsInDecreasingEfficiencyOrder {
  explicit CompareKnapsackItemsInDecreasingEfficiencyOrder(double _profit_max)
      : profit_max(_profit_max) {}
  bool operator()(const KnapsackItemForCutsPtr& item1,
                  const KnapsackItemForCutsPtr& item2) const {
    return item1->GetEfficiency(profit_max) > item2->GetEfficiency(profit_max);
  }
  const double profit_max;
};

// Comparator used to sort search nodes in the priority queue in order
// to pop first the node with the highest profit upper bound
// (see KnapsackSearchNodeForCuts). When two nodes have the same upper bound, we
// prefer the one with the highest current profit. This is usually the one
// closer to a leaf. In practice, the main advantage is to have smaller path.
struct CompareKnapsackSearchNodePtrInDecreasingUpperBoundOrder {
  bool operator()(const KnapsackSearchNodeForCuts* node_1,
                  const KnapsackSearchNodeForCuts* node_2) const {
    const double profit_upper_bound_1 = node_1->profit_upper_bound();
    const double profit_upper_bound_2 = node_2->profit_upper_bound();
    if (profit_upper_bound_1 == profit_upper_bound_2) {
      return node_1->current_profit() < node_2->current_profit();
    }
    return profit_upper_bound_1 < profit_upper_bound_2;
  }
};

using SearchQueue = std::priority_queue<
    KnapsackSearchNodeForCuts*, std::vector<KnapsackSearchNodeForCuts*>,
    CompareKnapsackSearchNodePtrInDecreasingUpperBoundOrder>;

}  // namespace

// ----- KnapsackSearchNodeForCuts -----
KnapsackSearchNodeForCuts::KnapsackSearchNodeForCuts(
    const KnapsackSearchNodeForCuts* const parent,
    const KnapsackAssignmentForCuts& assignment)
    : depth_(parent == nullptr ? 0 : parent->depth() + 1),
      parent_(parent),
      assignment_(assignment),
      current_profit_(0),
      profit_upper_bound_(kInfinity),
      next_item_id_(kNoSelection) {}

// ----- KnapsackSearchPathForCuts -----
KnapsackSearchPathForCuts::KnapsackSearchPathForCuts(
    const KnapsackSearchNodeForCuts* from, const KnapsackSearchNodeForCuts* to)
    : from_(from), via_(nullptr), to_(to) {}

void KnapsackSearchPathForCuts::Init() {
  const KnapsackSearchNodeForCuts* node_from =
      MoveUpToDepth(from_, to_->depth());
  const KnapsackSearchNodeForCuts* node_to = MoveUpToDepth(to_, from_->depth());
  DCHECK_EQ(node_from->depth(), node_to->depth());

  // Find common parent.
  while (node_from != node_to) {
    node_from = node_from->parent();
    node_to = node_to->parent();
  }
  via_ = node_from;
}

const KnapsackSearchNodeForCuts* MoveUpToDepth(
    const KnapsackSearchNodeForCuts* node, int depth) {
  while (node->depth() > depth) {
    node = node->parent();
  }
  return node;
}

// ----- KnapsackStateForCuts -----
KnapsackStateForCuts::KnapsackStateForCuts() : is_bound_(), is_in_() {}

void KnapsackStateForCuts::Init(int number_of_items) {
  is_bound_.assign(number_of_items, false);
  is_in_.assign(number_of_items, false);
}

// Returns false when the state is invalid.
bool KnapsackStateForCuts::UpdateState(
    bool revert, const KnapsackAssignmentForCuts& assignment) {
  if (revert) {
    is_bound_[assignment.item_id] = false;
  } else {
    if (is_bound_[assignment.item_id] &&
        is_in_[assignment.item_id] != assignment.is_in) {
      return false;
    }
    is_bound_[assignment.item_id] = true;
    is_in_[assignment.item_id] = assignment.is_in;
  }
  return true;
}

// ----- KnapsackPropagatorForCuts -----
KnapsackPropagatorForCuts::KnapsackPropagatorForCuts(
    const KnapsackStateForCuts* state)
    : items_(),
      current_profit_(0),
      profit_lower_bound_(0),
      profit_upper_bound_(kInfinity),
      state_(state) {}

KnapsackPropagatorForCuts::~KnapsackPropagatorForCuts() {}

void KnapsackPropagatorForCuts::Init(const std::vector<double>& profits,
                                     const std::vector<double>& weights,
                                     const double capacity) {
  const int number_of_items = profits.size();
  items_.clear();

  for (int i = 0; i < number_of_items; ++i) {
    items_.emplace_back(
        absl::make_unique<KnapsackItemForCuts>(i, weights[i], profits[i]));
  }
  capacity_ = capacity;
  current_profit_ = 0;
  profit_lower_bound_ = -kInfinity;
  profit_upper_bound_ = kInfinity;
  InitPropagator();
}

bool KnapsackPropagatorForCuts::Update(
    bool revert, const KnapsackAssignmentForCuts& assignment) {
  if (assignment.is_in) {
    if (revert) {
      current_profit_ -= items_[assignment.item_id]->profit;
      consumed_capacity_ -= items()[assignment.item_id]->weight;
    } else {
      current_profit_ += items_[assignment.item_id]->profit;
      consumed_capacity_ += items()[assignment.item_id]->weight;
      if (consumed_capacity_ > capacity_) {
        return false;
      }
    }
  }
  return true;
}

void KnapsackPropagatorForCuts::CopyCurrentStateToSolution(
    std::vector<bool>* solution) const {
  DCHECK(solution != nullptr);
  for (int i(0); i < items_.size(); ++i) {
    const int item_id = items_[i]->id;
    (*solution)[item_id] = state_->is_bound(item_id) && state_->is_in(item_id);
  }
  double remaining_capacity = capacity_ - consumed_capacity_;
  for (const KnapsackItemForCutsPtr& item : sorted_items_) {
    if (!state().is_bound(item->id)) {
      if (remaining_capacity >= item->weight) {
        remaining_capacity -= item->weight;
        (*solution)[item->id] = true;
      } else {
        return;
      }
    }
  }
}

void KnapsackPropagatorForCuts::ComputeProfitBounds() {
  set_profit_lower_bound(current_profit());
  break_item_id_ = kNoSelection;

  double remaining_capacity = capacity_ - consumed_capacity_;
  int break_sorted_item_id = kNoSelection;
  for (int sorted_id(0); sorted_id < sorted_items_.size(); ++sorted_id) {
    if (!state().is_bound(sorted_items_[sorted_id]->id)) {
      const KnapsackItemForCutsPtr& item = sorted_items_[sorted_id];
      break_item_id_ = item->id;
      if (remaining_capacity >= item->weight) {
        remaining_capacity -= item->weight;
        set_profit_lower_bound(profit_lower_bound() + item->profit);
      } else {
        break_sorted_item_id = sorted_id;
        break;
      }
    }
  }

  set_profit_upper_bound(profit_lower_bound());
  // If break_sorted_item_id == kNoSelection, then all remaining items fit into
  // the knapsack, and thus the lower bound on the profit equals the upper
  // bound. Otherwise, we compute a tight upper bound by filling the remaining
  // capacity of the knapsack with "fractional" items, in the decreasing order
  // of their efficiency.
  if (break_sorted_item_id != kNoSelection) {
    const double additional_profit =
        GetAdditionalProfitUpperBound(remaining_capacity, break_sorted_item_id);
    set_profit_upper_bound(profit_upper_bound() + additional_profit);
  }
}

void KnapsackPropagatorForCuts::InitPropagator() {
  consumed_capacity_ = 0;
  break_item_id_ = kNoSelection;
  sorted_items_.clear();
  sorted_items_.reserve(items().size());
  for (int i(0); i < items().size(); ++i) {
    sorted_items_.emplace_back(absl::make_unique<KnapsackItemForCuts>(
        i, items()[i]->weight, items()[i]->profit));
  }
  profit_max_ = 0;
  for (const KnapsackItemForCutsPtr& item : sorted_items_) {
    profit_max_ = std::max(profit_max_, item->profit);
  }
  profit_max_ += 1.0;
  CompareKnapsackItemsInDecreasingEfficiencyOrder compare_object(profit_max_);
  std::sort(sorted_items_.begin(), sorted_items_.end(), compare_object);
}

double KnapsackPropagatorForCuts::GetAdditionalProfitUpperBound(
    double remaining_capacity, int break_item_id) const {
  const int after_break_item_id = break_item_id + 1;
  double additional_profit_when_no_break_item = 0;
  if (after_break_item_id < sorted_items_.size()) {
    // As items are sorted by decreasing profit / weight ratio, and the current
    // weight is non-zero, the next_weight is non-zero too.
    const double next_weight = sorted_items_[after_break_item_id]->weight;
    const double next_profit = sorted_items_[after_break_item_id]->profit;
    additional_profit_when_no_break_item =
        std::max((remaining_capacity * next_profit) / next_weight, 0.0);
  }

  const int before_break_item_id = break_item_id - 1;
  double additional_profit_when_break_item = 0;
  if (before_break_item_id >= 0) {
    const double previous_weight = sorted_items_[before_break_item_id]->weight;
    // Having previous_weight == 0 means the total capacity is smaller than
    // the weight of the current item. In such a case the item cannot be part
    // of a solution of the local one dimension problem.
    if (previous_weight != 0) {
      const double previous_profit =
          sorted_items_[before_break_item_id]->profit;
      const double overused_capacity =
          sorted_items_[break_item_id]->weight - remaining_capacity;
      const double lost_profit_from_previous_item =
          (overused_capacity * previous_profit) / previous_weight;
      additional_profit_when_break_item = std::max(
          sorted_items_[break_item_id]->profit - lost_profit_from_previous_item,
          0.0);
    }
  }

  const double additional_profit = std::max(
      additional_profit_when_no_break_item, additional_profit_when_break_item);
  return additional_profit;
}

// ----- KnapsackSolverForCuts -----
KnapsackSolverForCuts::KnapsackSolverForCuts(std::string solver_name)
    : propagator_(&state_),
      best_solution_profit_(0),
      solver_name_(std::move(solver_name)) {}

void KnapsackSolverForCuts::Init(const std::vector<double>& profits,
                                 const std::vector<double>& weights,
                                 const double capacity) {
  const int number_of_items(profits.size());
  state_.Init(number_of_items);
  best_solution_.assign(number_of_items, false);
  CHECK_EQ(number_of_items, weights.size());

  propagator_.Init(profits, weights, capacity);
}

void KnapsackSolverForCuts::GetLowerAndUpperBoundWhenItem(int item_id,
                                                          bool is_item_in,
                                                          double* lower_bound,
                                                          double* upper_bound) {
  DCHECK(lower_bound != nullptr);
  DCHECK(upper_bound != nullptr);
  KnapsackAssignmentForCuts assignment(item_id, is_item_in);
  const bool fail = !IncrementalUpdate(false, assignment);
  if (fail) {
    *lower_bound = 0;
    *upper_bound = 0;
  } else {
    *lower_bound = propagator_.profit_lower_bound();
    *upper_bound = GetAggregatedProfitUpperBound();
  }

  const bool fail_revert = !IncrementalUpdate(true, assignment);
  if (fail_revert) {
    *lower_bound = 0;
    *upper_bound = 0;
  }
}

double KnapsackSolverForCuts::Solve(TimeLimit* time_limit,
                                    bool* is_solution_optimal) {
  DCHECK(time_limit != nullptr);
  DCHECK(is_solution_optimal != nullptr);
  best_solution_profit_ = 0;
  *is_solution_optimal = true;

  SearchQueue search_queue;
  const KnapsackAssignmentForCuts assignment(kNoSelection, true);
  auto root_node =
      absl::make_unique<KnapsackSearchNodeForCuts>(nullptr, assignment);
  root_node->set_current_profit(GetCurrentProfit());
  root_node->set_profit_upper_bound(GetAggregatedProfitUpperBound());
  root_node->set_next_item_id(GetNextItemId());
  search_nodes_.push_back(std::move(root_node));
  const KnapsackSearchNodeForCuts* current_node =
      search_nodes_.back().get();  // Start with the root node.

  if (MakeNewNode(*current_node, false)) {
    search_queue.push(search_nodes_.back().get());
  }
  if (MakeNewNode(*current_node, true)) {
    search_queue.push(search_nodes_.back().get());
  }

  int64_t number_of_nodes_visited = 0;
  while (!search_queue.empty() &&
         search_queue.top()->profit_upper_bound() > best_solution_profit_) {
    if (time_limit->LimitReached()) {
      *is_solution_optimal = false;
      break;
    }
    if (solution_upper_bound_threshold_ > -kInfinity &&
        GetAggregatedProfitUpperBound() < solution_upper_bound_threshold_) {
      *is_solution_optimal = false;
      break;
    }
    if (best_solution_profit_ > solution_lower_bound_threshold_) {
      *is_solution_optimal = false;
      break;
    }
    if (number_of_nodes_visited >= node_limit_) {
      *is_solution_optimal = false;
      break;
    }
    KnapsackSearchNodeForCuts* const node = search_queue.top();
    search_queue.pop();

    if (node != current_node) {
      KnapsackSearchPathForCuts path(current_node, node);
      path.Init();
      CHECK_EQ(UpdatePropagators(path), true);
      current_node = node;
    }
    number_of_nodes_visited++;

    if (MakeNewNode(*node, false)) {
      search_queue.push(search_nodes_.back().get());
    }
    if (MakeNewNode(*node, true)) {
      search_queue.push(search_nodes_.back().get());
    }
  }
  return best_solution_profit_;
}

// Returns false when at least one propagator fails.
bool KnapsackSolverForCuts::UpdatePropagators(
    const KnapsackSearchPathForCuts& path) {
  bool no_fail = true;
  // Revert previous changes.
  const KnapsackSearchNodeForCuts* node = &path.from();
  const KnapsackSearchNodeForCuts* const via = &path.via();
  while (node != via) {
    no_fail = IncrementalUpdate(true, node->assignment()) && no_fail;
    node = node->parent();
  }
  // Apply current changes.
  node = &path.to();
  while (node != via) {
    no_fail = IncrementalUpdate(false, node->assignment()) && no_fail;
    node = node->parent();
  }
  return no_fail;
}

double KnapsackSolverForCuts::GetAggregatedProfitUpperBound() {
  propagator_.ComputeProfitBounds();
  const double propagator_upper_bound = propagator_.profit_upper_bound();
  return std::min(kInfinity, propagator_upper_bound);
}

bool KnapsackSolverForCuts::MakeNewNode(const KnapsackSearchNodeForCuts& node,
                                        bool is_in) {
  if (node.next_item_id() == kNoSelection) {
    return false;
  }
  KnapsackAssignmentForCuts assignment(node.next_item_id(), is_in);
  KnapsackSearchNodeForCuts new_node(&node, assignment);

  KnapsackSearchPathForCuts path(&node, &new_node);
  path.Init();
  const bool no_fail = UpdatePropagators(path);
  if (no_fail) {
    new_node.set_current_profit(GetCurrentProfit());
    new_node.set_profit_upper_bound(GetAggregatedProfitUpperBound());
    new_node.set_next_item_id(GetNextItemId());
    UpdateBestSolution();
  }

  // Revert to be able to create another node from parent.
  KnapsackSearchPathForCuts revert_path(&new_node, &node);
  revert_path.Init();
  UpdatePropagators(revert_path);

  if (!no_fail || new_node.profit_upper_bound() < best_solution_profit_) {
    return false;
  }

  // The node is relevant.
  auto relevant_node =
      absl::make_unique<KnapsackSearchNodeForCuts>(&node, assignment);
  relevant_node->set_current_profit(new_node.current_profit());
  relevant_node->set_profit_upper_bound(new_node.profit_upper_bound());
  relevant_node->set_next_item_id(new_node.next_item_id());
  search_nodes_.push_back(std::move(relevant_node));

  return true;
}

bool KnapsackSolverForCuts::IncrementalUpdate(
    bool revert, const KnapsackAssignmentForCuts& assignment) {
  // Do not stop on a failure: To be able to be incremental on the update,
  // partial solution (state) and propagators must all be in the same state.
  bool no_fail = state_.UpdateState(revert, assignment);
  no_fail = propagator_.Update(revert, assignment) && no_fail;
  return no_fail;
}

void KnapsackSolverForCuts::UpdateBestSolution() {
  const double profit_lower_bound = propagator_.profit_lower_bound();

  if (best_solution_profit_ < profit_lower_bound) {
    best_solution_profit_ = profit_lower_bound;
    propagator_.CopyCurrentStateToSolution(&best_solution_);
  }
}

}  // namespace operations_research
