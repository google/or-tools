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

// This library solves 0-1 one-dimensional knapsack problems with fractional
// profits and weights using the branch and bound algorithm. Note that
// algorithms/knapsack_solver uses 'int64_t' for the profits and the weights.
// TODO(user): Merge this code with algorithms/knapsack_solver.
//
// Given n items, each with a profit and a weight and a knapsack of
// capacity c, the goal is to find a subset of the items which fits inside c
// and maximizes the total profit.
// Without loss of generality, profits and weights are assumed to be positive.
//
// From a mathematical point of view, the one-dimensional knapsack problem
// can be modeled by linear constraint:
// Sum(i:1..n)(weight_i * item_i) <= c,
// where item_i is a 0-1 integer variable.
// The goal is to maximize: Sum(i:1..n)(profit_i * item_i).
//
// Example Usage:
// std::vector<double> profits = {0, 0.5, 0.4, 1, 1, 1.1};
// std::vector<double> weights = {9, 6, 2, 1.5, 1.5, 1.5};
// KnapsackSolverForCuts solver("solver");
// solver.Init(profits, weights, capacity);
// bool is_solution_optimal = false;
// std::unique_ptr<TimeLimit> time_limit =
//     std::make_unique<TimeLimit>(time_limit_seconds); // Set the time limit.
// const double profit = solver.Solve(time_limit.get(), &is_solution_optimal);
// const int number_of_items(profits.size());
// for (int item_id(0); item_id < number_of_items; ++item_id) {
//   solver.best_solution(item_id); // Access the solution.
// }

#ifndef OR_TOOLS_ALGORITHMS_KNAPSACK_SOLVER_FOR_CUTS_H_
#define OR_TOOLS_ALGORITHMS_KNAPSACK_SOLVER_FOR_CUTS_H_

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "ortools/base/int_type.h"
#include "ortools/base/logging.h"
#include "ortools/util/time_limit.h"

namespace operations_research {

// ----- KnapsackAssignmentForCuts -----
// KnapsackAssignmentForCuts is a small struct used to pair an item with
// its assignment. It is mainly used for search nodes and updates.
struct KnapsackAssignmentForCuts {
  KnapsackAssignmentForCuts(int item_id, bool is_in)
      : item_id(item_id), is_in(is_in) {}

  int item_id;
  bool is_in;
};

// ----- KnapsackItemForCuts -----
// KnapsackItemForCuts is a small struct to pair an item weight with its
// corresponding profit.
// The aim of the knapsack problem is to pack as many valuable items as
// possible. A straight forward heuristic is to take those with the greatest
// profit-per-unit-weight. This ratio is called efficiency in this
// implementation. So items will be grouped in vectors, and sorted by
// decreasing efficiency.
struct KnapsackItemForCuts {
  KnapsackItemForCuts(int id, double weight, double profit)
      : id(id), weight(weight), profit(profit) {}

  double GetEfficiency(double profit_max) const {
    return (weight > 0) ? profit / weight : profit_max;
  }

  // The 'id' field is used to retrieve the initial item in order to
  // communicate with other propagators and state.
  const int id;
  const double weight;
  const double profit;
};
using KnapsackItemForCutsPtr = std::unique_ptr<KnapsackItemForCuts>;

// ----- KnapsackSearchNodeForCuts -----
// KnapsackSearchNodeForCuts is a class used to describe a decision in the
// decision search tree.
// The node is defined by a pointer to the parent search node and an
// assignment (see KnapsackAssignmentForCuts).
// As the current state is not explicitly stored in a search node, one should
// go through the search tree to incrementally build a partial solution from
// a previous search node.
class KnapsackSearchNodeForCuts {
 public:
  KnapsackSearchNodeForCuts(const KnapsackSearchNodeForCuts* parent,
                            const KnapsackAssignmentForCuts& assignment);

  KnapsackSearchNodeForCuts(const KnapsackSearchNodeForCuts&) = delete;
  KnapsackSearchNodeForCuts& operator=(const KnapsackSearchNodeForCuts&) =
      delete;

  int depth() const { return depth_; }
  const KnapsackSearchNodeForCuts* const parent() const { return parent_; }
  const KnapsackAssignmentForCuts& assignment() const { return assignment_; }

  double current_profit() const { return current_profit_; }
  void set_current_profit(double profit) { current_profit_ = profit; }

  double profit_upper_bound() const { return profit_upper_bound_; }
  void set_profit_upper_bound(double profit) { profit_upper_bound_ = profit; }

  int next_item_id() const { return next_item_id_; }
  void set_next_item_id(int id) { next_item_id_ = id; }

 private:
  // 'depth_' is used to navigate efficiently through the search tree.
  int depth_;
  const KnapsackSearchNodeForCuts* const parent_;
  KnapsackAssignmentForCuts assignment_;

  // 'current_profit_' and 'profit_upper_bound_' fields are used to sort search
  // nodes using a priority queue. That allows to pop the node with the best
  // upper bound, and more importantly to stop the search when optimality is
  // proved.
  double current_profit_;
  double profit_upper_bound_;

  // 'next_item_id_' field allows to avoid an O(number_of_items) scan to find
  // next item to select. This is done for free by the upper bound computation.
  int next_item_id_;
};

// ----- KnapsackSearchPathForCuts -----
// KnapsackSearchPathForCuts is a small class used to represent the path between
// a node to another node in the search tree.
// As the solution state is not stored for each search node, the state should
// be rebuilt at each node. One simple solution is to apply all decisions
// between the node 'to' and the root. This can be computed in
// O(number_of_items).
//
// However, it is possible to achieve better average complexity. Two
// consecutively explored nodes are usually close enough (i.e., much less than
// number_of_items) to benefit from an incremental update from the node
// 'from' to the node 'to'.
//
// The 'via' field is the common parent of 'from' field and 'to' field.
// So the state can be built by reverting all decisions from 'from' to 'via'
// and then applying all decisions from 'via' to 'to'.
class KnapsackSearchPathForCuts {
 public:
  KnapsackSearchPathForCuts(const KnapsackSearchNodeForCuts* from,
                            const KnapsackSearchNodeForCuts* to);

  KnapsackSearchPathForCuts(const KnapsackSearchPathForCuts&) = delete;
  KnapsackSearchPathForCuts& operator=(const KnapsackSearchPathForCuts&) =
      delete;

  void Init();
  const KnapsackSearchNodeForCuts& from() const { return *from_; }
  const KnapsackSearchNodeForCuts& via() const { return *via_; }
  const KnapsackSearchNodeForCuts& to() const { return *to_; }

 private:
  const KnapsackSearchNodeForCuts* from_;
  const KnapsackSearchNodeForCuts* via_;  // Computed in 'Init'.
  const KnapsackSearchNodeForCuts* to_;
};

// From the given node, this method moves up the tree and returns the node at
// given depth.
const KnapsackSearchNodeForCuts* MoveUpToDepth(
    const KnapsackSearchNodeForCuts* node, int depth);

// ----- KnapsackStateForCuts -----
// KnapsackStateForCuts represents a partial solution to the knapsack problem.
class KnapsackStateForCuts {
 public:
  KnapsackStateForCuts();

  KnapsackStateForCuts(const KnapsackStateForCuts&) = delete;
  KnapsackStateForCuts& operator=(const KnapsackStateForCuts&) = delete;

  // Initializes vectors with number_of_items set to false (i.e. not bound yet).
  void Init(int number_of_items);

  // Updates the state by applying or reverting a decision.
  // Returns false if fails, i.e. trying to apply an inconsistent decision
  // to an already assigned item.
  bool UpdateState(bool revert, const KnapsackAssignmentForCuts& assignment);

  int GetNumberOfItems() const { return is_bound_.size(); }
  bool is_bound(int id) const { return is_bound_.at(id); }
  bool is_in(int id) const { return is_in_.at(id); }

 private:
  // Vectors 'is_bound_' and 'is_in_' contain a boolean value for each item.
  // 'is_bound_(item_i)' is false when there is no decision for item_i yet.
  // When item_i is bound, 'is_in_(item_i)' represents the presence (true) or
  // the absence (false) of item_i in the current solution.
  std::vector<bool> is_bound_;
  std::vector<bool> is_in_;
};

// ----- KnapsackPropagatorForCuts -----
// KnapsackPropagatorForCuts is used to enforce a capacity constraint.
// It is supposed to compute profit lower and upper bounds, and get the next
// item to select, it can be seen as a 0-1 Knapsack solver. The most efficient
// way to compute the upper bound is to iterate on items in
// profit-per-unit-weight decreasing order. The break item is commonly defined
// as the first item for which there is not enough remaining capacity. Selecting
// this break item as the next-item-to-assign usually gives the best results
// (see Greenberg & Hegerich).
//
// This is exactly what is implemented in this class.
//
// It is possible to compute a better profit lower bound almost for free. During
// the scan to find the break element all unbound items are added just as if
// they were part of the current solution. This is used in both
// ComputeProfitBounds() and CopyCurrentSolution(). For incrementality reasons,
// the ith item should be accessible in O(1). That's the reason why the item
// vector has to be duplicated 'sorted_items_'.
class KnapsackPropagatorForCuts {
 public:
  explicit KnapsackPropagatorForCuts(const KnapsackStateForCuts* state);
  ~KnapsackPropagatorForCuts();

  KnapsackPropagatorForCuts(const KnapsackPropagatorForCuts&) = delete;
  KnapsackPropagatorForCuts& operator=(const KnapsackPropagatorForCuts&) =
      delete;

  // Initializes the data structure and then calls InitPropagator.
  void Init(const std::vector<double>& profits,
            const std::vector<double>& weights, double capacity);

  // Updates data structure. Returns false on failure.
  bool Update(bool revert, const KnapsackAssignmentForCuts& assignment);
  // ComputeProfitBounds should set 'profit_lower_bound_' and
  // 'profit_upper_bound_' which are constraint specific.
  void ComputeProfitBounds();
  // Returns the id of next item to assign.
  // Returns kNoSelection when all items are bound.
  int GetNextItemId() const { return break_item_id_; }

  double current_profit() const { return current_profit_; }
  double profit_lower_bound() const { return profit_lower_bound_; }
  double profit_upper_bound() const { return profit_upper_bound_; }

  // Copies the current state into 'solution'.
  // All unbound items are set to false (i.e. not in the knapsack).
  void CopyCurrentStateToSolution(std::vector<bool>* solution) const;

  // Initializes the propagator. This method is called by Init() after filling
  // the fields defined in this class.
  void InitPropagator();

  const KnapsackStateForCuts& state() const { return *state_; }
  const std::vector<KnapsackItemForCutsPtr>& items() const { return items_; }

  void set_profit_lower_bound(double profit) { profit_lower_bound_ = profit; }
  void set_profit_upper_bound(double profit) { profit_upper_bound_ = profit; }

 private:
  // An obvious additional profit upper bound corresponds to the linear
  // relaxation: remaining_capacity * efficiency of the break item.
  // It is possible to do better in O(1), using Martello-Toth bound U2.
  // The main idea is to enforce integrality constraint on the break item,
  // i.e. either the break item is part of the solution, or it is not.
  // So basically the linear relaxation is done on the item before the break
  // item, or the one after the break item. This is what GetAdditionalProfit
  // method implements.
  double GetAdditionalProfitUpperBound(double remaining_capacity,
                                       int break_item_id) const;

  double capacity_;
  double consumed_capacity_;
  int break_item_id_;
  std::vector<KnapsackItemForCutsPtr> sorted_items_;
  double profit_max_;
  std::vector<KnapsackItemForCutsPtr> items_;
  double current_profit_;
  double profit_lower_bound_;
  double profit_upper_bound_;
  const KnapsackStateForCuts* const state_;
};

// ----- KnapsackSolverForCuts -----
// KnapsackSolverForCuts is the one-dimensional knapsack solver class.
// In the current implementation, the next item to assign is given by the
// primary propagator. Using SetPrimaryPropagator allows changing the default
// (propagator of the first dimension).
class KnapsackSolverForCuts {
 public:
  explicit KnapsackSolverForCuts(std::string solver_name);

  KnapsackSolverForCuts(const KnapsackSolverForCuts&) = delete;
  KnapsackSolverForCuts& operator=(const KnapsackSolverForCuts&) = delete;

  // Initializes the solver and enters the problem to be solved.
  void Init(const std::vector<double>& profits,
            const std::vector<double>& weights, const double capacity);
  int GetNumberOfItems() const { return state_.GetNumberOfItems(); }

  // Gets the lower and the upper bound when the item is in or out of the
  // knapsack. To ensure objects are correctly initialized, this method should
  // not be called before Init().
  void GetLowerAndUpperBoundWhenItem(int item_id, bool is_item_in,
                                     double* lower_bound, double* upper_bound);

  // Get the best upper bound found so far.
  double GetUpperBound() { return GetAggregatedProfitUpperBound(); }

  // The solver stops if a solution with profit better than
  // 'solution_lower_bound_threshold' is found.
  void set_solution_lower_bound_threshold(
      const double solution_lower_bound_threshold) {
    solution_lower_bound_threshold_ = solution_lower_bound_threshold;
  }

  // The solver stops if the upper bound on profit drops below
  // 'solution_upper_bound_threshold'.
  void set_solution_upper_bound_threshold(
      const double solution_upper_bound_threshold) {
    solution_upper_bound_threshold_ = solution_upper_bound_threshold;
  }

  // Stops the knapsack solver after processing 'node_limit' nodes.
  void set_node_limit(const int64_t node_limit) { node_limit_ = node_limit; }

  // Solves the problem and returns the profit of the best solution found.
  double Solve(TimeLimit* time_limit, bool* is_solution_optimal);
  // Returns true if the item 'item_id' is packed in the optimal knapsack.
  bool best_solution(int item_id) const {
    DCHECK(item_id < best_solution_.size());
    return best_solution_[item_id];
  }

  const std::string& GetName() const { return solver_name_; }

 private:
  // Updates propagator reverting/applying all decision on the path. Returns
  // true if the propagation fails. Note that even if it fails, propagator
  // should be updated to be in a stable state in order to stay incremental.
  bool UpdatePropagators(const KnapsackSearchPathForCuts& path);
  // Updates propagator reverting/applying one decision. Returns true if
  // the propagation fails. Note that even if it fails, propagator should
  // be updated to be in a stable state in order to stay incremental.
  bool IncrementalUpdate(bool revert,
                         const KnapsackAssignmentForCuts& assignment);
  // Updates the best solution if the current solution has a better profit.
  void UpdateBestSolution();

  // Returns true if new relevant search node was added to the nodes array. That
  // means this node should be added to the search queue too.
  bool MakeNewNode(const KnapsackSearchNodeForCuts& node, bool is_in);

  // Gets the aggregated (min) profit upper bound among all propagators.
  double GetAggregatedProfitUpperBound();
  double GetCurrentProfit() const { return propagator_.current_profit(); }
  int GetNextItemId() const { return propagator_.GetNextItemId(); }

  KnapsackPropagatorForCuts propagator_;
  std::vector<std::unique_ptr<KnapsackSearchNodeForCuts>> search_nodes_;
  KnapsackStateForCuts state_;
  double best_solution_profit_;
  std::vector<bool> best_solution_;
  const std::string solver_name_;
  double solution_lower_bound_threshold_ =
      std::numeric_limits<double>::infinity();
  double solution_upper_bound_threshold_ =
      -std::numeric_limits<double>::infinity();
  int64_t node_limit_ = std::numeric_limits<int64_t>::max();
};
// TODO(user) : Add reduction algorithm.

}  // namespace operations_research

#endif  // OR_TOOLS_ALGORITHMS_KNAPSACK_SOLVER_FOR_CUTS_H_
