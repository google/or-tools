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


#include "ortools/algorithms/knapsack_solver.h"

#include <algorithm>
#include <queue>
#include <string>
#include <vector>

#include "ortools/base/stl_util.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/util/bitset.h"
#include "ortools/util/time_limit.h"

namespace operations_research {

namespace {
const int kNoSelection = -1;
const int kMasterPropagatorId = 0;
const int kMaxNumberOfBruteForceItems = 30;
const int kMaxNumberOf64Items = 64;

// Comparator used to sort item in decreasing efficiency order
// (see KnapsackCapacityPropagator).
struct CompareKnapsackItemsInDecreasingEfficiencyOrder {
  explicit CompareKnapsackItemsInDecreasingEfficiencyOrder(int64 _profit_max)
      : profit_max(_profit_max) {}
  bool operator()(const KnapsackItemPtr& item1,
                  const KnapsackItemPtr& item2) const {
    return item1->GetEfficiency(profit_max) > item2->GetEfficiency(profit_max);
  }
  const int64 profit_max;
};

// Comparator used to sort search nodes in the priority queue in order
// to pop first the node with the highest profit upper bound
// (see KnapsackSearchNode). When two nodes have the same upper bound, we
// prefer the one with the highest current profit, ie. usually the one closer
// to a leaf. In practice, the main advantage is to have smaller path.
struct CompareKnapsackSearchNodePtrInDecreasingUpperBoundOrder {
  bool operator()(const KnapsackSearchNode* node_1,
                  const KnapsackSearchNode* node_2) const {
    const int64 profit_upper_bound_1 = node_1->profit_upper_bound();
    const int64 profit_upper_bound_2 = node_2->profit_upper_bound();
    if (profit_upper_bound_1 == profit_upper_bound_2) {
      return node_1->current_profit() < node_2->current_profit();
    }
    return profit_upper_bound_1 < profit_upper_bound_2;
  }
};

typedef std::priority_queue<
    KnapsackSearchNode*, std::vector<KnapsackSearchNode*>,
    CompareKnapsackSearchNodePtrInDecreasingUpperBoundOrder>
    SearchQueue;

// Returns true when value_1 * value_2 may overflow int64.
inline bool WillProductOverflow(int64 value_1, int64 value_2) {
  const int MostSignificantBitPosition1 = MostSignificantBitPosition64(value_1);
  const int MostSignificantBitPosition2 = MostSignificantBitPosition64(value_2);
  // The sum should be less than 61 to be safe as we are only considering the
  // most significant bit and dealing with int64 instead of uint64.
  const int kOverflow = 61;
  return MostSignificantBitPosition1 + MostSignificantBitPosition2 > kOverflow;
}

// Returns an upper bound of (numerator_1 * numerator_2) / denominator
int64 UpperBoundOfRatio(int64 numerator_1, int64 numerator_2,
                        int64 denominator) {
  DCHECK_GT(denominator, 0LL);
  if (!WillProductOverflow(numerator_1, numerator_2)) {
    const int64 numerator = numerator_1 * numerator_2;
    // Round to zero.
    const int64 result = numerator / denominator;
    return result;
  } else {
    const double ratio =
        (static_cast<double>(numerator_1) * static_cast<double>(numerator_2)) /
        static_cast<double>(denominator);
    // Round near.
    const int64 result = static_cast<int64>(floor(ratio + 0.5));
    return result;
  }
}

}  // namespace

// ----- KnapsackSearchNode -----
KnapsackSearchNode::KnapsackSearchNode(const KnapsackSearchNode* const parent,
                                       const KnapsackAssignment& assignment)
    : depth_((parent == nullptr) ? 0 : parent->depth() + 1),
      parent_(parent),
      assignment_(assignment),
      current_profit_(0),
      profit_upper_bound_(kint64max),
      next_item_id_(kNoSelection) {}

// ----- KnapsackSearchPath -----
KnapsackSearchPath::KnapsackSearchPath(const KnapsackSearchNode& from,
                                       const KnapsackSearchNode& to)
    : from_(from), via_(nullptr), to_(to) {}

void KnapsackSearchPath::Init() {
  const KnapsackSearchNode* node_from = MoveUpToDepth(from_, to_.depth());
  const KnapsackSearchNode* node_to = MoveUpToDepth(to_, from_.depth());
  CHECK_EQ(node_from->depth(), node_to->depth());

  // Find common parent.
  while (node_from != node_to) {
    node_from = node_from->parent();
    node_to = node_to->parent();
  }
  via_ = node_from;
}

const KnapsackSearchNode* KnapsackSearchPath::MoveUpToDepth(
    const KnapsackSearchNode& node, int depth) const {
  const KnapsackSearchNode* current_node = &node;
  while (current_node->depth() > depth) {
    current_node = current_node->parent();
  }
  return current_node;
}

// ----- KnapsackState -----
KnapsackState::KnapsackState() : is_bound_(), is_in_() {}

void KnapsackState::Init(int number_of_items) {
  is_bound_.assign(number_of_items, false);
  is_in_.assign(number_of_items, false);
}

// Returns false when the state is invalid.
bool KnapsackState::UpdateState(bool revert,
                                const KnapsackAssignment& assignment) {
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

// ----- KnapsackPropagator -----
KnapsackPropagator::KnapsackPropagator(const KnapsackState& state)
    : items_(),
      current_profit_(0),
      profit_lower_bound_(0),
      profit_upper_bound_(kint64max),
      state_(state) {}

KnapsackPropagator::~KnapsackPropagator() { STLDeleteElements(&items_); }

void KnapsackPropagator::Init(const std::vector<int64>& profits,
                              const std::vector<int64>& weights) {
  const int number_of_items = profits.size();
  items_.assign(number_of_items, static_cast<KnapsackItemPtr>(nullptr));
  for (int i = 0; i < number_of_items; ++i) {
    items_[i] = new KnapsackItem(i, weights[i], profits[i]);
  }
  current_profit_ = 0;
  profit_lower_bound_ = kint64min;
  profit_upper_bound_ = kint64max;
  InitPropagator();
}

bool KnapsackPropagator::Update(bool revert,
                                const KnapsackAssignment& assignment) {
  if (assignment.is_in) {
    if (revert) {
      current_profit_ -= items_[assignment.item_id]->profit;
    } else {
      current_profit_ += items_[assignment.item_id]->profit;
    }
  }
  return UpdatePropagator(revert, assignment);
}

void KnapsackPropagator::CopyCurrentStateToSolution(
    bool has_one_propagator, std::vector<bool>* solution) const {
  CHECK_NOTNULL(solution);
  for (const KnapsackItem* const item : items_) {
    const int item_id = item->id;
    (*solution)[item_id] = state_.is_bound(item_id) && state_.is_in(item_id);
  }
  if (has_one_propagator) {
    CopyCurrentStateToSolutionPropagator(solution);
  }
}

// ----- KnapsackCapacityPropagator -----
KnapsackCapacityPropagator::KnapsackCapacityPropagator(
    const KnapsackState& state, int64 capacity)
    : KnapsackPropagator(state),
      capacity_(capacity),
      consumed_capacity_(0),
      break_item_id_(kNoSelection),
      sorted_items_(),
      profit_max_(0) {}

KnapsackCapacityPropagator::~KnapsackCapacityPropagator() {}

// TODO(user): Make it more incremental, by saving the break item in a
// search node for instance.
void KnapsackCapacityPropagator::ComputeProfitBounds() {
  set_profit_lower_bound(current_profit());
  break_item_id_ = kNoSelection;

  int64 remaining_capacity = capacity_ - consumed_capacity_;
  int break_sorted_item_id = kNoSelection;
  const int number_of_sorted_items = sorted_items_.size();
  for (int sorted_id = 0; sorted_id < number_of_sorted_items; ++sorted_id) {
    const KnapsackItem* const item = sorted_items_[sorted_id];
    if (!state().is_bound(item->id)) {
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

  if (break_sorted_item_id != kNoSelection) {
    const int64 additional_profit =
        GetAdditionalProfit(remaining_capacity, break_sorted_item_id);
    set_profit_upper_bound(profit_upper_bound() + additional_profit);
  }
}

void KnapsackCapacityPropagator::InitPropagator() {
  consumed_capacity_ = 0;
  break_item_id_ = kNoSelection;
  sorted_items_ = items();
  profit_max_ = 0;
  for (const KnapsackItem* const item : sorted_items_) {
    profit_max_ = std::max(profit_max_, item->profit);
  }
  ++profit_max_;
  CompareKnapsackItemsInDecreasingEfficiencyOrder compare_object(profit_max_);
  std::sort(sorted_items_.begin(), sorted_items_.end(), compare_object);
}

// Returns false when the propagator fails.
bool KnapsackCapacityPropagator::UpdatePropagator(
    bool revert, const KnapsackAssignment& assignment) {
  if (assignment.is_in) {
    if (revert) {
      consumed_capacity_ -= items()[assignment.item_id]->weight;
    } else {
      consumed_capacity_ += items()[assignment.item_id]->weight;
      if (consumed_capacity_ > capacity_) {
        return false;
      }
    }
  }
  return true;
}

void KnapsackCapacityPropagator::CopyCurrentStateToSolutionPropagator(
    std::vector<bool>* solution) const {
  CHECK_NOTNULL(solution);
  int64 remaining_capacity = capacity_ - consumed_capacity_;
  for (const KnapsackItem* const item : sorted_items_) {
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

int64 KnapsackCapacityPropagator::GetAdditionalProfit(int64 remaining_capacity,
                                                      int break_item_id) const {
  const int after_break_item_id = break_item_id + 1;
  int64 additional_profit_when_no_break_item = 0;
  if (after_break_item_id < sorted_items_.size()) {
    // As items are sorted by decreasing profit / weight ratio, and the current
    // weight is non-zero, the next_weight is non-zero too.
    const int64 next_weight = sorted_items_[after_break_item_id]->weight;
    const int64 next_profit = sorted_items_[after_break_item_id]->profit;
    additional_profit_when_no_break_item =
        UpperBoundOfRatio(remaining_capacity, next_profit, next_weight);
  }

  const int before_break_item_id = break_item_id - 1;
  int64 additional_profit_when_break_item = 0;
  if (before_break_item_id >= 0) {
    const int64 previous_weight = sorted_items_[before_break_item_id]->weight;
    // Having previous_weight == 0 means the total capacity is smaller than
    // the weight of the current item. In such a case the item cannot be part
    // of a solution of the local one dimension problem.
    if (previous_weight != 0) {
      const int64 previous_profit = sorted_items_[before_break_item_id]->profit;
      const int64 overused_capacity =
          sorted_items_[break_item_id]->weight - remaining_capacity;
      const int64 ratio = UpperBoundOfRatio(overused_capacity, previous_profit,
                                            previous_weight);
      additional_profit_when_break_item =
          sorted_items_[break_item_id]->profit - ratio;
    }
  }

  const int64 additional_profit = std::max(additional_profit_when_no_break_item,
                                           additional_profit_when_break_item);
  CHECK_GE(additional_profit, 0);
  return additional_profit;
}

// ----- KnapsackGenericSolver -----
KnapsackGenericSolver::KnapsackGenericSolver(const std::string& solver_name)
    : BaseKnapsackSolver(solver_name),
      propagators_(),
      master_propagator_id_(kMasterPropagatorId),
      search_nodes_(),
      state_(),
      best_solution_profit_(0),
      best_solution_() {}

KnapsackGenericSolver::~KnapsackGenericSolver() { Clear(); }

void KnapsackGenericSolver::Init(const std::vector<int64>& profits,
                                 const std::vector<std::vector<int64>>& weights,
                                 const std::vector<int64>& capacities) {
  CHECK_EQ(capacities.size(), weights.size());

  Clear();
  const int number_of_items = profits.size();
  const int number_of_dimensions = weights.size();
  state_.Init(number_of_items);
  best_solution_.assign(number_of_items, false);
  for (int i = 0; i < number_of_dimensions; ++i) {
    CHECK_EQ(number_of_items, weights[i].size());

    KnapsackCapacityPropagator* propagator =
        new KnapsackCapacityPropagator(state_, capacities[i]);
    propagator->Init(profits, weights[i]);
    propagators_.push_back(propagator);
  }
  master_propagator_id_ = kMasterPropagatorId;
}

void KnapsackGenericSolver::GetLowerAndUpperBoundWhenItem(int item_id,
                                                          bool is_item_in,
                                                          int64* lower_bound,
                                                          int64* upper_bound) {
  CHECK_NOTNULL(lower_bound);
  CHECK_NOTNULL(upper_bound);
  KnapsackAssignment assignment(item_id, is_item_in);
  const bool fail = !IncrementalUpdate(false, assignment);
  if (fail) {
    *lower_bound = 0LL;
    *upper_bound = 0LL;
  } else {
    *lower_bound = (HasOnePropagator()) ? propagators_[master_propagator_id_]
                                              ->profit_lower_bound()
                                        : 0LL;
    *upper_bound = GetAggregatedProfitUpperBound();
  }

  const bool fail_revert = !IncrementalUpdate(true, assignment);
  if (fail_revert) {
    *lower_bound = 0LL;
    *upper_bound = 0LL;
  }
}

int64 KnapsackGenericSolver::Solve(TimeLimit* time_limit,
                                   bool* is_solution_optimal) {
  DCHECK(time_limit != nullptr);
  DCHECK(is_solution_optimal != nullptr);
  best_solution_profit_ = 0LL;
  *is_solution_optimal = true;

  SearchQueue search_queue;
  const KnapsackAssignment assignment(kNoSelection, true);
  KnapsackSearchNode* root_node = new KnapsackSearchNode(nullptr, assignment);
  root_node->set_current_profit(GetCurrentProfit());
  root_node->set_profit_upper_bound(GetAggregatedProfitUpperBound());
  root_node->set_next_item_id(GetNextItemId());
  search_nodes_.push_back(root_node);

  if (MakeNewNode(*root_node, false)) {
    search_queue.push(search_nodes_.back());
  }
  if (MakeNewNode(*root_node, true)) {
    search_queue.push(search_nodes_.back());
  }

  KnapsackSearchNode* current_node = root_node;
  while (!search_queue.empty() &&
         search_queue.top()->profit_upper_bound() > best_solution_profit_) {
    if (time_limit->LimitReached()) {
      *is_solution_optimal = false;
      break;
    }
    KnapsackSearchNode* const node = search_queue.top();
    search_queue.pop();

    if (node != current_node) {
      KnapsackSearchPath path(*current_node, *node);
      path.Init();
      const bool no_fail = UpdatePropagators(path);
      current_node = node;
      CHECK_EQ(no_fail, true);
    }

    if (MakeNewNode(*node, false)) {
      search_queue.push(search_nodes_.back());
    }
    if (MakeNewNode(*node, true)) {
      search_queue.push(search_nodes_.back());
    }
  }
  return best_solution_profit_;
}

void KnapsackGenericSolver::Clear() {
  STLDeleteElements(&propagators_);
  STLDeleteElements(&search_nodes_);
}

// Returns false when at least one propagator fails.
bool KnapsackGenericSolver::UpdatePropagators(const KnapsackSearchPath& path) {
  bool no_fail = true;
  // Revert previous changes.
  const KnapsackSearchNode* node = &path.from();
  const KnapsackSearchNode* via = &path.via();
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

int64 KnapsackGenericSolver::GetAggregatedProfitUpperBound() const {
  int64 upper_bound = kint64max;
  for (KnapsackPropagator* const prop : propagators_) {
    prop->ComputeProfitBounds();
    const int64 propagator_upper_bound = prop->profit_upper_bound();
    upper_bound = std::min(upper_bound, propagator_upper_bound);
  }
  return upper_bound;
}

bool KnapsackGenericSolver::MakeNewNode(const KnapsackSearchNode& node,
                                        bool is_in) {
  if (node.next_item_id() == kNoSelection) {
    return false;
  }
  KnapsackAssignment assignment(node.next_item_id(), is_in);
  KnapsackSearchNode new_node(&node, assignment);

  KnapsackSearchPath path(node, new_node);
  path.Init();
  const bool no_fail = UpdatePropagators(path);
  if (no_fail) {
    new_node.set_current_profit(GetCurrentProfit());
    new_node.set_profit_upper_bound(GetAggregatedProfitUpperBound());
    new_node.set_next_item_id(GetNextItemId());
    UpdateBestSolution();
  }

  // Revert to be able to create another node from parent.
  KnapsackSearchPath revert_path(new_node, node);
  revert_path.Init();
  UpdatePropagators(revert_path);

  if (!no_fail || new_node.profit_upper_bound() < best_solution_profit_) {
    return false;
  }

  // The node is relevant.
  KnapsackSearchNode* relevant_node = new KnapsackSearchNode(&node, assignment);
  relevant_node->set_current_profit(new_node.current_profit());
  relevant_node->set_profit_upper_bound(new_node.profit_upper_bound());
  relevant_node->set_next_item_id(new_node.next_item_id());
  search_nodes_.push_back(relevant_node);

  return true;
}

bool KnapsackGenericSolver::IncrementalUpdate(
    bool revert, const KnapsackAssignment& assignment) {
  // Do not stop on a failure: To be able to be incremental on the update,
  // partial solution (state) and propagators must all be in the same state.
  bool no_fail = state_.UpdateState(revert, assignment);
  for (KnapsackPropagator* const prop : propagators_) {
    no_fail = prop->Update(revert, assignment) && no_fail;
  }
  return no_fail;
}

void KnapsackGenericSolver::UpdateBestSolution() {
  const int64 profit_lower_bound =
      (HasOnePropagator())
          ? propagators_[master_propagator_id_]->profit_lower_bound()
          : propagators_[master_propagator_id_]->current_profit();

  if (best_solution_profit_ < profit_lower_bound) {
    best_solution_profit_ = profit_lower_bound;
    propagators_[master_propagator_id_]
        ->CopyCurrentStateToSolution(HasOnePropagator(), &best_solution_);
  }
}

// ----- KnapsackBruteForceSolver -----
// KnapsackBruteForceSolver solves the 0-1 knapsack problem when the number of
// items is less or equal to 30 with brute force, ie. explores all states.
// Experiments show better results than KnapsackGenericSolver when the
// number of items is less than 15.
class KnapsackBruteForceSolver : public BaseKnapsackSolver {
 public:
  explicit KnapsackBruteForceSolver(const std::string& solver_name);

  // Initializes the solver and enters the problem to be solved.
  void Init(const std::vector<int64>& profits,
            const std::vector<std::vector<int64>>& weights,
            const std::vector<int64>& capacities) override;

  // Solves the problem and returns the profit of the optimal solution.
  int64 Solve(TimeLimit* time_limit, bool* is_solution_optimal) override;

  // Returns true if the item 'item_id' is packed in the optimal knapsack.
  bool best_solution(int item_id) const override {
    return (best_solution_ & OneBit32(item_id)) != 0U;
  }

 private:
  int num_items_;
  int64 profits_weights_[kMaxNumberOfBruteForceItems * 2];
  int64 capacity_;
  int64 best_solution_profit_;
  uint32 best_solution_;

  DISALLOW_COPY_AND_ASSIGN(KnapsackBruteForceSolver);
};

KnapsackBruteForceSolver::KnapsackBruteForceSolver(const std::string& solver_name)
    : BaseKnapsackSolver(solver_name),
      num_items_(0),
      capacity_(0LL),
      best_solution_profit_(0LL),
      best_solution_(0U) {}

void KnapsackBruteForceSolver::Init(
    const std::vector<int64>& profits,
    const std::vector<std::vector<int64>>& weights,
    const std::vector<int64>& capacities) {
  // TODO(user): Implement multi-dimensional brute force solver.
  CHECK_EQ(weights.size(), 1)
      << "Brute force solver only works with one dimension.";
  CHECK_EQ(capacities.size(), weights.size());

  num_items_ = profits.size();
  CHECK_EQ(num_items_, weights.at(0).size());
  CHECK_LE(num_items_, kMaxNumberOfBruteForceItems)
      << "To use KnapsackBruteForceSolver the number of items should be "
      << "less than " << kMaxNumberOfBruteForceItems
      << ". Current value: " << num_items_ << ".";

  for (int i = 0; i < num_items_; ++i) {
    profits_weights_[i * 2] = profits.at(i);
    profits_weights_[i * 2 + 1] = weights.at(0).at(i);
  }
  capacity_ = capacities.at(0);
}

int64 KnapsackBruteForceSolver::Solve(TimeLimit* time_limit,
                                      bool* is_solution_optimal) {
  DCHECK(is_solution_optimal != nullptr);
  *is_solution_optimal = true;
  best_solution_profit_ = 0LL;
  best_solution_ = 0U;

  const uint32 num_states = OneBit32(num_items_);
  uint32 prev_state = 0U;
  uint64 sum_profit = 0ULL;
  uint64 sum_weight = 0ULL;
  uint32 diff_state = 0U;
  uint32 local_state = 0U;
  int item_id = 0;
  // This loop starts at 1, because state = 0 was already considered previously,
  // ie. when no items are in, sum_profit = 0.
  for (uint32 state = 1U; state < num_states; ++state, ++prev_state) {
    diff_state = state ^ prev_state;
    local_state = state;
    item_id = 0;
    while (diff_state) {
      if (diff_state & 1U) {     // There is a diff.
        if (local_state & 1U) {  // This item is now in the knapsack.
          sum_profit += profits_weights_[item_id];
          sum_weight += profits_weights_[item_id + 1];
          CHECK_LT(item_id + 1, 2 * num_items_);
        } else {  // This item has been removed of the knapsack.
          sum_profit -= profits_weights_[item_id];
          sum_weight -= profits_weights_[item_id + 1];
          CHECK_LT(item_id + 1, 2 * num_items_);
        }
      }
      item_id += 2;
      local_state = local_state >> 1;
      diff_state = diff_state >> 1;
    }

    if (sum_weight <= capacity_ && best_solution_profit_ < sum_profit) {
      best_solution_profit_ = sum_profit;
      best_solution_ = state;
    }
  }

  return best_solution_profit_;
}

// ----- KnapsackItemWithEfficiency -----
// KnapsackItem is a small struct to pair an item weight with its
// corresponding profit.
// This struct is used by Knapsack64ItemsSolver. As this solver deals only
// with one dimension, that's more efficient to store 'efficiency' than
// computing it on the fly.
struct KnapsackItemWithEfficiency {
  KnapsackItemWithEfficiency(int _id, int64 _profit, int64 _weight,
                             int64 _profit_max)
      : id(_id),
        profit(_profit),
        weight(_weight),
        efficiency((weight > 0) ? static_cast<double>(_profit) /
                                      static_cast<double>(_weight)
                                : static_cast<double>(_profit_max)) {}

  int id;
  int64 profit;
  int64 weight;
  double efficiency;
};

// ----- Knapsack64ItemsSolver -----
// Knapsack64ItemsSolver solves the 0-1 knapsack problem when the number of
// items is less or equal to 64. This implementation is about 4 times faster
// than KnapsackGenericSolver.
class Knapsack64ItemsSolver : public BaseKnapsackSolver {
 public:
  explicit Knapsack64ItemsSolver(const std::string& solver_name);

  // Initializes the solver and enters the problem to be solved.
  void Init(const std::vector<int64>& profits,
            const std::vector<std::vector<int64>>& weights,
            const std::vector<int64>& capacities) override;

  // Solves the problem and returns the profit of the optimal solution.
  int64 Solve(TimeLimit* time_limit, bool* is_solution_optimal) override;

  // Returns true if the item 'item_id' is packed in the optimal knapsack.
  bool best_solution(int item_id) const override {
    return (best_solution_ & OneBit64(item_id)) != 0ULL;
  }

 private:
  int GetBreakItemId(int64 capacity) const;
  void GetLowerAndUpperBound(int64* lower_bound, int64* upper_bound) const;
  void GoToNextState(bool has_failed);
  void BuildBestSolution();

  std::vector<KnapsackItemWithEfficiency> sorted_items_;
  std::vector<int64> sum_profits_;
  std::vector<int64> sum_weights_;
  int64 capacity_;
  uint64 state_;
  int state_depth_;

  int64 best_solution_profit_;
  uint64 best_solution_;
  int best_solution_depth_;

  // Sum of weights of included item in state.
  int64 state_weight_;
  // Sum of profits of non included items in state.
  int64 rejected_items_profit_;
  // Sum of weights of non included items in state.
  int64 rejected_items_weight_;
};

// Comparator used to sort item in decreasing efficiency order
bool CompareKnapsackItemWithEfficiencyInDecreasingEfficiencyOrder(
    const KnapsackItemWithEfficiency& item1,
    const KnapsackItemWithEfficiency& item2) {
  return item1.efficiency > item2.efficiency;
}

// ----- Knapsack64ItemsSolver -----
Knapsack64ItemsSolver::Knapsack64ItemsSolver(const std::string& solver_name)
    : BaseKnapsackSolver(solver_name),
      sorted_items_(),
      sum_profits_(),
      sum_weights_(),
      capacity_(0LL),
      state_(0ULL),
      state_depth_(0),
      best_solution_profit_(0LL),
      best_solution_(0ULL),
      best_solution_depth_(0),
      state_weight_(0LL),
      rejected_items_profit_(0LL),
      rejected_items_weight_(0LL) {}

void Knapsack64ItemsSolver::Init(const std::vector<int64>& profits,
                                 const std::vector<std::vector<int64>>& weights,
                                 const std::vector<int64>& capacities) {
  CHECK_EQ(weights.size(), 1)
      << "Brute force solver only works with one dimension.";
  CHECK_EQ(capacities.size(), weights.size());

  sorted_items_.clear();
  sum_profits_.clear();
  sum_weights_.clear();

  capacity_ = capacities[0];
  const int num_items = profits.size();
  CHECK_LE(num_items, kMaxNumberOf64Items)
      << "To use Knapsack64ItemsSolver the number of items should be "
      << "less than " << kMaxNumberOf64Items << ". Current value: " << num_items
      << ".";
  int64 profit_max = *std::max_element(profits.begin(), profits.end());

  for (int i = 0; i < num_items; ++i) {
    sorted_items_.push_back(
        KnapsackItemWithEfficiency(i, profits[i], weights[0][i], profit_max));
  }

  std::sort(sorted_items_.begin(), sorted_items_.end(),
            CompareKnapsackItemWithEfficiencyInDecreasingEfficiencyOrder);

  int64 sum_profit = 0;
  int64 sum_weight = 0;
  sum_profits_.push_back(sum_profit);
  sum_weights_.push_back(sum_weight);
  for (int i = 0; i < num_items; ++i) {
    sum_profit += sorted_items_[i].profit;
    sum_weight += sorted_items_[i].weight;

    sum_profits_.push_back(sum_profit);
    sum_weights_.push_back(sum_weight);
  }
}

int64 Knapsack64ItemsSolver::Solve(TimeLimit* time_limit,
                                   bool* is_solution_optimal) {
  DCHECK(is_solution_optimal != nullptr);
  *is_solution_optimal = true;
  const int num_items = sorted_items_.size();
  state_ = 1ULL;
  state_depth_ = 0;
  state_weight_ = sorted_items_[0].weight;
  rejected_items_profit_ = 0LL;
  rejected_items_weight_ = 0LL;
  best_solution_profit_ = 0LL;
  best_solution_ = 0ULL;
  best_solution_depth_ = 0;

  int64 lower_bound = 0LL;
  int64 upper_bound = 0LL;
  bool fail = false;
  while (state_depth_ >= 0) {
    fail = false;
    if (state_weight_ > capacity_ || state_depth_ >= num_items) {
      fail = true;
    } else {
      GetLowerAndUpperBound(&lower_bound, &upper_bound);
      if (best_solution_profit_ < lower_bound) {
        best_solution_profit_ = lower_bound;
        best_solution_ = state_;
        best_solution_depth_ = state_depth_;
      }
    }
    fail = fail || best_solution_profit_ >= upper_bound;
    GoToNextState(fail);
  }

  BuildBestSolution();
  return best_solution_profit_;
}

int Knapsack64ItemsSolver::GetBreakItemId(int64 capacity) const {
  std::vector<int64>::const_iterator binary_search_iterator =
      std::upper_bound(sum_weights_.begin(), sum_weights_.end(), capacity);
  return static_cast<int>(binary_search_iterator - sum_weights_.begin()) - 1;
}

// This method is called for each possible state.
// Lower and upper bounds can be equal from one state to another.
// For instance state 1010???? and state 101011?? have exactly the same
// bounds. So it sounds like a good idea to cache those bounds.
// Unfortunately, experiments show equivalent results with or without this
// code optimization (only 1/7 of calls can be reused).
// In order to simplify the code, this optimization is not implemented.
void Knapsack64ItemsSolver::GetLowerAndUpperBound(int64* lower_bound,
                                                  int64* upper_bound) const {
  const int64 available_capacity = capacity_ + rejected_items_weight_;
  const int break_item_id = GetBreakItemId(available_capacity);
  const int num_items = sorted_items_.size();
  if (break_item_id >= num_items) {
    *lower_bound = sum_profits_[num_items] - rejected_items_profit_;
    *upper_bound = *lower_bound;
    return;
  }

  *lower_bound = sum_profits_[break_item_id] - rejected_items_profit_;
  *upper_bound = *lower_bound;
  const int64 consumed_capacity = sum_weights_[break_item_id];
  const int64 remaining_capacity = available_capacity - consumed_capacity;
  const double efficiency = sorted_items_[break_item_id].efficiency;
  const int64 additional_profit =
      static_cast<int64>(remaining_capacity * efficiency);
  *upper_bound += additional_profit;
}

// As state_depth_ is the position of the most significant bit on state_
// it is possible to remove the loop and so be in O(1) instead of O(depth).
// In such a case rejected_items_profit_ is computed using sum_profits_ array.
// Unfortunately experiments show smaller computation time using the 'while'
// (10% speed-up). That's the reason why the loop version is implemented.
void Knapsack64ItemsSolver::GoToNextState(bool has_failed) {
  uint64 mask = OneBit64(state_depth_);
  if (!has_failed) {  // Go to next item.
    ++state_depth_;
    state_ = state_ | (mask << 1);
    state_weight_ += sorted_items_[state_depth_].weight;
  } else {
    // Backtrack to last item in.
    while ((state_ & mask) == 0ULL && state_depth_ >= 0) {
      const KnapsackItemWithEfficiency& item = sorted_items_[state_depth_];
      rejected_items_profit_ -= item.profit;
      rejected_items_weight_ -= item.weight;
      --state_depth_;
      mask = mask >> 1ULL;
    }

    if (state_ & mask) {  // Item was in, remove it.
      state_ = state_ & ~mask;
      const KnapsackItemWithEfficiency& item = sorted_items_[state_depth_];
      rejected_items_profit_ += item.profit;
      rejected_items_weight_ += item.weight;
      state_weight_ -= item.weight;
    }
  }
}

void Knapsack64ItemsSolver::BuildBestSolution() {
  int64 remaining_capacity = capacity_;
  int64 check_profit = 0LL;

  // Compute remaining capacity at best_solution_depth_ to be able to redo
  // the GetLowerAndUpperBound computation.
  for (int i = 0; i <= best_solution_depth_; ++i) {
    if (best_solution_ & OneBit64(i)) {
      remaining_capacity -= sorted_items_[i].weight;
      check_profit += sorted_items_[i].profit;
    }
  }

  // Add all items till the break item.
  const int num_items = sorted_items_.size();
  for (int i = best_solution_depth_ + 1; i < num_items; ++i) {
    int64 weight = sorted_items_[i].weight;
    if (remaining_capacity >= weight) {
      remaining_capacity -= weight;
      check_profit += sorted_items_[i].profit;
      best_solution_ = best_solution_ | OneBit64(i);
    } else {
      best_solution_ = best_solution_ & ~OneBit64(i);
    }
  }
  CHECK_EQ(best_solution_profit_, check_profit);

  // Items were sorted by efficiency, solution should be unsorted to be
  // in user order.
  // Note that best_solution_ will not be in the same order than other data
  // structures anymore.
  uint64 tmp_solution = 0ULL;
  for (int i = 0; i < num_items; ++i) {
    if (best_solution_ & OneBit64(i)) {
      const int original_id = sorted_items_[i].id;
      tmp_solution = tmp_solution | OneBit64(original_id);
    }
  }

  best_solution_ = tmp_solution;
}

// ----- KnapsackDynamicProgrammingSolver -----
// KnapsackDynamicProgrammingSolver solves the 0-1 knapsack problem
// using dynamic programming. This algorithm is pseudo-polynomial because it
// depends on capacity, ie. the time and space complexity is
// O(capacity * number_of_items).
// The implemented algorithm is 'DP-3' in "Knapsack problems", Hans Kellerer,
// Ulrich Pferschy and David Pisinger, Springer book (ISBN 978-3540402862).
class KnapsackDynamicProgrammingSolver : public BaseKnapsackSolver {
 public:
  explicit KnapsackDynamicProgrammingSolver(const std::string& solver_name);

  // Initializes the solver and enters the problem to be solved.
  void Init(const std::vector<int64>& profits,
            const std::vector<std::vector<int64>>& weights,
            const std::vector<int64>& capacities) override;

  // Solves the problem and returns the profit of the optimal solution.
  int64 Solve(TimeLimit* time_limit, bool* is_solution_optimal) override;

  // Returns true if the item 'item_id' is packed in the optimal knapsack.
  bool best_solution(int item_id) const override {
    return best_solution_.at(item_id);
  }

 private:
  int64 SolveSubProblem(int64 capacity, int num_items);

  std::vector<int64> profits_;
  std::vector<int64> weights_;
  int64 capacity_;
  std::vector<int64> computed_profits_;
  std::vector<int> selected_item_ids_;
  std::vector<bool> best_solution_;
};

// ----- KnapsackDynamicProgrammingSolver -----
KnapsackDynamicProgrammingSolver::KnapsackDynamicProgrammingSolver(
    const std::string& solver_name)
    : BaseKnapsackSolver(solver_name),
      profits_(),
      weights_(),
      capacity_(0),
      computed_profits_(),
      selected_item_ids_(),
      best_solution_() {}

void KnapsackDynamicProgrammingSolver::Init(
    const std::vector<int64>& profits,
    const std::vector<std::vector<int64>>& weights,
    const std::vector<int64>& capacities) {
  CHECK_EQ(weights.size(), 1)
      << "Current implementation of the dynamic programming solver only deals"
      << " with one dimension.";
  CHECK_EQ(capacities.size(), weights.size());

  profits_ = profits;
  weights_ = weights[0];
  capacity_ = capacities[0];
}

int64 KnapsackDynamicProgrammingSolver::SolveSubProblem(int64 capacity,
                                                        int num_items) {
  const int64 capacity_plus_1 = capacity + 1;
  std::fill_n(selected_item_ids_.begin(), capacity_plus_1, 0);
  std::fill_n(computed_profits_.begin(), capacity_plus_1, 0LL);
  for (int item_id = 0; item_id < num_items; ++item_id) {
    const int64 item_weight = weights_[item_id];
    const int64 item_profit = profits_[item_id];
    for (int64 used_capacity = capacity; used_capacity >= item_weight;
         --used_capacity) {
      if (computed_profits_[used_capacity - item_weight] + item_profit >
          computed_profits_[used_capacity]) {
        computed_profits_[used_capacity] =
            computed_profits_[used_capacity - item_weight] + item_profit;
        selected_item_ids_[used_capacity] = item_id;
      }
    }
  }
  return selected_item_ids_.at(capacity);
}

int64 KnapsackDynamicProgrammingSolver::Solve(TimeLimit* time_limit,
                                              bool* is_solution_optimal) {
  DCHECK(is_solution_optimal != nullptr);
  *is_solution_optimal = true;
  const int64 capacity_plus_1 = capacity_ + 1;
  selected_item_ids_.assign(capacity_plus_1, 0);
  computed_profits_.assign(capacity_plus_1, 0LL);

  int64 remaining_capacity = capacity_;
  int num_items = profits_.size();
  best_solution_.assign(num_items, false);

  while (remaining_capacity > 0 && num_items > 0) {
    const int selected_item_id = SolveSubProblem(remaining_capacity, num_items);
    remaining_capacity -= weights_[selected_item_id];
    num_items = selected_item_id;
    if (remaining_capacity >= 0) {
      best_solution_[selected_item_id] = true;
    }
  }

  return computed_profits_[capacity_];
}

// ----- KnapsackMIPSolver -----
class KnapsackMIPSolver : public BaseKnapsackSolver {
 public:
  KnapsackMIPSolver(MPSolver::OptimizationProblemType problem_type,
                    const std::string& solver_name);

  // Initializes the solver and enters the problem to be solved.
  void Init(const std::vector<int64>& profits,
            const std::vector<std::vector<int64>>& weights,
            const std::vector<int64>& capacities) override;

  // Solves the problem and returns the profit of the optimal solution.
  int64 Solve(TimeLimit* time_limit, bool* is_solution_optimal) override;

  // Returns true if the item 'item_id' is packed in the optimal knapsack.
  bool best_solution(int item_id) const override {
    return best_solution_.at(item_id);
  }

 private:
  MPSolver::OptimizationProblemType problem_type_;
  std::vector<int64> profits_;
  std::vector<std::vector<int64>> weights_;
  std::vector<int64> capacities_;
  std::vector<bool> best_solution_;
};

KnapsackMIPSolver::KnapsackMIPSolver(
    MPSolver::OptimizationProblemType problem_type, const std::string& solver_name)
    : BaseKnapsackSolver(solver_name),
      problem_type_(problem_type),
      profits_(),
      weights_(),
      capacities_(),
      best_solution_() {}

void KnapsackMIPSolver::Init(const std::vector<int64>& profits,
                             const std::vector<std::vector<int64>>& weights,
                             const std::vector<int64>& capacities) {
  profits_ = profits;
  weights_ = weights;
  capacities_ = capacities;
}

int64 KnapsackMIPSolver::Solve(TimeLimit* time_limit,
                               bool* is_solution_optimal) {
  DCHECK(is_solution_optimal != nullptr);
  *is_solution_optimal = true;
  MPSolver solver(GetName(), problem_type_);

  const int num_items = profits_.size();
  std::vector<MPVariable*> variables;
  solver.MakeBoolVarArray(num_items, "x", &variables);

  // Add constraints.
  const int num_dimensions = capacities_.size();
  for (int i = 0; i < num_dimensions; ++i) {
    MPConstraint* const ct = solver.MakeRowConstraint(0LL, capacities_.at(i));
    for (int j = 0; j < num_items; ++j) {
      ct->SetCoefficient(variables.at(j), weights_.at(i).at(j));
    }
  }

  // Define objective to minimize. Minimization is used instead of maximization
  // because of an issue with CBC solver which does not always find the optimal
  // solution on maximization problems.
  MPObjective* const objective = solver.MutableObjective();
  for (int j = 0; j < num_items; ++j) {
    objective->SetCoefficient(variables.at(j), -profits_.at(j));
  }
  objective->SetMinimization();

  solver.SuppressOutput();
  solver.Solve();

  // Store best solution.
  const float kRoundNear = 0.5;
  best_solution_.assign(num_items, false);
  for (int j = 0; j < num_items; ++j) {
    const double value = variables.at(j)->solution_value();
    best_solution_.at(j) = value >= kRoundNear;
  }

  return -objective->Value() + kRoundNear;
}

// ----- KnapsackSolver -----
KnapsackSolver::KnapsackSolver(const std::string& solver_name)
    : KnapsackSolver(KNAPSACK_MULTIDIMENSION_BRANCH_AND_BOUND_SOLVER,
                     solver_name) {}

KnapsackSolver::KnapsackSolver(SolverType solver_type,
                               const std::string& solver_name)
    : solver_(),
      known_value_(),
      best_solution_(),
      mapping_reduced_item_id_(),
      is_problem_solved_(false),
      additional_profit_(0LL),
      use_reduction_(true),
      time_limit_seconds_(std::numeric_limits<double>::infinity()) {
  switch (solver_type) {
    case KNAPSACK_BRUTE_FORCE_SOLVER:
      solver_.reset(new KnapsackBruteForceSolver(solver_name));
      break;
    case KNAPSACK_64ITEMS_SOLVER:
      solver_.reset(new Knapsack64ItemsSolver(solver_name));
      break;
    case KNAPSACK_DYNAMIC_PROGRAMMING_SOLVER:
      solver_.reset(new KnapsackDynamicProgrammingSolver(solver_name));
      break;
    case KNAPSACK_MULTIDIMENSION_BRANCH_AND_BOUND_SOLVER:
      solver_.reset(new KnapsackGenericSolver(solver_name));
      break;
    #if defined(USE_CBC)
    case KNAPSACK_MULTIDIMENSION_CBC_MIP_SOLVER:
      solver_.reset(new KnapsackMIPSolver(
          MPSolver::CBC_MIXED_INTEGER_PROGRAMMING, solver_name));
      break;
    #endif  // USE_CBC
    #if defined(USE_GLPK)
    case KNAPSACK_MULTIDIMENSION_GLPK_MIP_SOLVER:
      solver_.reset(new KnapsackMIPSolver(
          MPSolver::GLPK_MIXED_INTEGER_PROGRAMMING, solver_name));
      break;
    #endif  // USE_GLPK
    #if defined(USE_SCIP)
    case KNAPSACK_MULTIDIMENSION_SCIP_MIP_SOLVER:
      solver_.reset(new KnapsackMIPSolver(
          MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING, solver_name));
      break;
    #endif  // USE_SCIP
    default:
      LOG(FATAL) << "Unknown knapsack solver type.";
  }
}

KnapsackSolver::~KnapsackSolver() {}

void KnapsackSolver::Init(const std::vector<int64>& profits,
                          const std::vector<std::vector<int64>>& weights,
                          const std::vector<int64>& capacities) {
  time_limit_.reset(new TimeLimit(time_limit_seconds_));
  is_solution_optimal_ = false;
  additional_profit_ = 0LL;
  is_problem_solved_ = false;

  const int num_items = profits.size();
  std::vector<std::vector<int64>> reduced_weights;
  std::vector<int64> reduced_capacities;
  if (use_reduction_) {
    const int num_reduced_items = ReduceCapacities(
        num_items, weights, capacities, &reduced_weights, &reduced_capacities);
    if (num_reduced_items > 0) {
      ComputeAdditionalProfit(profits);
    }
  } else {
    reduced_weights = weights;
    reduced_capacities = capacities;
  }
  if (!is_problem_solved_) {
    solver_->Init(profits, reduced_weights, reduced_capacities);
    if (use_reduction_) {
      const int num_reduced_items = ReduceProblem(num_items);

      if (num_reduced_items > 0) {
        ComputeAdditionalProfit(profits);
      }

      if (num_reduced_items > 0 && num_reduced_items < num_items) {
        InitReducedProblem(profits, reduced_weights, reduced_capacities);
      }
    }
  }
  if (is_problem_solved_) {
    is_solution_optimal_ = true;
  }
}

int KnapsackSolver::ReduceCapacities(
    int num_items, const std::vector<std::vector<int64>>& weights,
    const std::vector<int64>& capacities,
    std::vector<std::vector<int64>>* reduced_weights,
    std::vector<int64>* reduced_capacities) {
  known_value_.assign(num_items, false);
  best_solution_.assign(num_items, false);
  mapping_reduced_item_id_.assign(num_items, 0);
  std::vector<bool> active_capacities(weights.size(), true);
  int number_of_active_capacities = 0;
  for (int i = 0; i < weights.size(); ++i) {
    int64 max_weight = 0;
    for (int64 weight : weights[i]) {
      max_weight += weight;
    }
    if (max_weight <= capacities[i]) {
      active_capacities[i] = false;
    } else {
      ++number_of_active_capacities;
    }
  }
  reduced_weights->reserve(number_of_active_capacities);
  reduced_capacities->reserve(number_of_active_capacities);
  for (int i = 0; i < weights.size(); ++i) {
    if (active_capacities[i]) {
      reduced_weights->push_back(weights[i]);
      reduced_capacities->push_back(capacities[i]);
    }
  }
  if (reduced_capacities->empty()) {
    // There are no capacity constraints in the problem so we can reduce all
    // items and just add them to the best solution.
    for (int item_id = 0; item_id < num_items; ++item_id) {
      known_value_[item_id] = true;
      best_solution_[item_id] = true;
    }
    is_problem_solved_ = true;
    // All items are reduced.
    return num_items;
  }
  // There are still capacity constraints so no item reduction is done.
  return 0;
}

int KnapsackSolver::ReduceProblem(int num_items) {
  known_value_.assign(num_items, false);
  best_solution_.assign(num_items, false);
  mapping_reduced_item_id_.assign(num_items, 0);
  additional_profit_ = 0LL;

  for (int item_id = 0; item_id < num_items; ++item_id) {
    mapping_reduced_item_id_[item_id] = item_id;
  }

  int64 best_lower_bound = 0LL;
  std::vector<int64> J0_upper_bounds(num_items, kint64max);
  std::vector<int64> J1_upper_bounds(num_items, kint64max);
  for (int item_id = 0; item_id < num_items; ++item_id) {
    if (time_limit_->LimitReached()) {
      break;
    }
    int64 lower_bound = 0LL;
    int64 upper_bound = kint64max;
    solver_->GetLowerAndUpperBoundWhenItem(item_id, false, &lower_bound,
                                           &upper_bound);
    J1_upper_bounds.at(item_id) = upper_bound;
    best_lower_bound = std::max(best_lower_bound, lower_bound);

    solver_->GetLowerAndUpperBoundWhenItem(item_id, true, &lower_bound,
                                           &upper_bound);
    J0_upper_bounds.at(item_id) = upper_bound;
    best_lower_bound = std::max(best_lower_bound, lower_bound);
  }

  int num_reduced_items = 0;
  for (int item_id = 0; item_id < num_items; ++item_id) {
    if (best_lower_bound > J0_upper_bounds[item_id]) {
      known_value_[item_id] = true;
      best_solution_[item_id] = false;
      ++num_reduced_items;
    } else if (best_lower_bound > J1_upper_bounds[item_id]) {
      known_value_[item_id] = true;
      best_solution_[item_id] = true;
      ++num_reduced_items;
    }
  }

  is_problem_solved_ = num_reduced_items == num_items;
  return num_reduced_items;
}

void KnapsackSolver::ComputeAdditionalProfit(
    const std::vector<int64>& profits) {
  const int num_items = profits.size();
  additional_profit_ = 0LL;
  for (int item_id = 0; item_id < num_items; ++item_id) {
    if (known_value_[item_id] && best_solution_[item_id]) {
      additional_profit_ += profits[item_id];
    }
  }
}

void KnapsackSolver::InitReducedProblem(
    const std::vector<int64>& profits,
    const std::vector<std::vector<int64>>& weights,
    const std::vector<int64>& capacities) {
  const int num_items = profits.size();
  const int num_dimensions = capacities.size();

  std::vector<int64> reduced_profits;
  for (int item_id = 0; item_id < num_items; ++item_id) {
    if (!known_value_[item_id]) {
      mapping_reduced_item_id_[item_id] = reduced_profits.size();
      reduced_profits.push_back(profits[item_id]);
    }
  }

  std::vector<std::vector<int64>> reduced_weights;
  std::vector<int64> reduced_capacities = capacities;
  for (int dim = 0; dim < num_dimensions; ++dim) {
    const std::vector<int64>& one_dimension_weights = weights[dim];
    std::vector<int64> one_dimension_reduced_weights;
    for (int item_id = 0; item_id < num_items; ++item_id) {
      if (known_value_[item_id]) {
        if (best_solution_[item_id]) {
          reduced_capacities[dim] -= one_dimension_weights[item_id];
        }
      } else {
        one_dimension_reduced_weights.push_back(one_dimension_weights[item_id]);
      }
    }
    reduced_weights.push_back(one_dimension_reduced_weights);
  }
  solver_->Init(reduced_profits, reduced_weights, reduced_capacities);
}

int64 KnapsackSolver::Solve() {
  return additional_profit_ +
         ((is_problem_solved_) ? 0 : solver_->Solve(time_limit_.get(),
                                                    &is_solution_optimal_));
}

bool KnapsackSolver::BestSolutionContains(int item_id) const {
  const int mapped_item_id =
      (use_reduction_) ? mapping_reduced_item_id_[item_id] : item_id;
  return (use_reduction_ && known_value_[item_id])
             ? best_solution_[item_id]
             : solver_->best_solution(mapped_item_id);
}

std::string KnapsackSolver::GetName() const { return solver_->GetName(); }

// ----- BaseKnapsackSolver -----
void BaseKnapsackSolver::GetLowerAndUpperBoundWhenItem(int item_id,
                                                       bool is_item_in,
                                                       int64* lower_bound,
                                                       int64* upper_bound) {
  CHECK_NOTNULL(lower_bound);
  CHECK_NOTNULL(upper_bound);
  *lower_bound = 0LL;
  *upper_bound = kint64max;
}

}  // namespace operations_research
