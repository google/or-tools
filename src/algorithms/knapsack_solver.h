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

//
// This library solves knapsacks:
//   - 0-1 knapsack problems,
//   - Multi-dimensional knapsack problems,
//   - TODO(user) Multi-dimensional knapsack problem with n-ary conflicts
//     between items.
//
// Given n items, each with a profit and a weight, given a knapsack of
// capacity c, the goal is to find a subset of items which fits inside c
// and maximizes the total profit.
// The knapsack problem can easily be extended from 1 to d dimensions.
// As an example, this can be useful to constrain the maximum number of
// items inside the knapsack.
// Without loss of generality, profits and weights are assumed to be positive.
//
// From a mathematical point of view, the multi-dimensional knapsack problem
// can be modeled by d linear constraints:
// ForEach(j:1..d)(Sum(i:1..n)(weight_ij * item_i) <= c_j
// where item_i is a 0-1 integer variable.
// Then the goal is to maximize: Sum(i:1..n)(profit_i * item_i).
//
// There are several ways to solve knapsack problems. One of the most
// efficient ways is based on dynamic programming (mainly when weights, profits
// and dimensions are small, the algorithm runs in pseudo polynomial time).
// Unfortunately when adding conflict constraints the problem becomes strongly
// NP-hard, i.e. there is no pseudo-polynomial algorithm to solve it.
// That's the reason why the most of the following code is based on branch and
// bound search.
//
// For instance to solve a 2-dimension knapsack problem with 9 items,
// one just has to feed a profit vector with the 9 profits, a vector of 2
// vectors for weights, and a vector of capacities.
// E.g.:
//   vector: profits = [1, 2, 3, 4, 5, 6, 7, 8, 9]
//   vector of vector: weights = [ [1, 2, 3, 4, 5, 6, 7, 8, 9],
//                                 [1, 1, 1, 1, 1, 1, 1, 1, 1]]
//   vector: capacities = [34, 4]
// And then:
//   KnapsackSolver solver(KnapsackSolver::KNAPSACK_MULTIDIMENSION_SOLVER,
//                         "Multi-dimensional solver");
//   solver.Init(profits, weights, capacities);
//   int64 profit = solver.Solve();

#ifndef OR_TOOLS_ALGORITHMS_KNAPSACK_SOLVER_H_
#define OR_TOOLS_ALGORITHMS_KNAPSACK_SOLVER_H_

#include <math.h>
#include <memory>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "util/time_limit.h"

namespace operations_research {

// ----- KnapsackSolver -----
// KnapsackSolver is a factory for knapsack solvers. Several solvers are
// implemented, some can deal with a limited number of items, some can deal with
// several dimensions...
// Currently 4 algorithms are implemented:
//  - KNAPSACK_BRUTE_FORCE_SOLVER: Limited to 30 items and one dimension, this
//    solver uses a brute force algorithm, ie. explores all possible states.
//    Experiments show competitive performance for instances with less than
//    15 items.
//  - KNAPSACK_64ITEMS_SOLVER: Limited to 64 items and one dimension, this
//    solver uses a branch & bound algorithm. This solver is about 4 times
//    faster than KNAPSACK_MULTIDIMENSION_SOLVER.
//  - KNAPSACK_DYNAMIC_PROGRAMMING_SOLVER: Limited to one dimension, this solver
//    is based on a dynamic programming algorithm. The time and space
//    complexity is O(capacity * number_of_items).
//  - KNAPSACK_MULTIDIMENSION_BRANCH_AND_BOUND_SOLVER: This solver can deal
//    with both large number of items and several dimensions. This solver is
//    based on branch and bound.
//  - KNAPSACK_MULTIDIMENSION_CBC_MIP_SOLVER: This solver can deal with both
//    large number of items and several dimensions. This solver is based on
//    Integer Programming solver CBC.
//  - KNAPSACK_MULTIDIMENSION_GLPK_MIP_SOLVER: This solver can deal with both
//    large number of items and several dimensions. This solver is based on
//    Integer Programming solver GLPK.
//
// KnapsackSolver also implements a problem reduction algorithm based on lower
// and upper bounds (see Ingargolia and Korsh: A reduction algorithm for
// zero-one single knapsack problems. Management Science, 1973). This reduction
// method is preferred to better algorithms (see, for instance, Martello
// and Toth: A new algorithm for the 0-1 knapsack problem. Management Science,
// 1988), because it remains valid with more complex problems, e.g.,
// multi-dimensional, conflicts...
//
// The main idea is to compute lower and upper bounds for each item in or out
// of the knapsack; if the best lower bound is strictly greater than the upper
// bound when an item is in, then this item is surely not in the optimal
// solution.
class BaseKnapsackSolver;

class KnapsackSolver {
 public:
  enum SolverType {
    KNAPSACK_BRUTE_FORCE_SOLVER = 0,
    KNAPSACK_64ITEMS_SOLVER = 1,
    KNAPSACK_DYNAMIC_PROGRAMMING_SOLVER = 2,
    #if defined(USE_CBC)
    KNAPSACK_MULTIDIMENSION_CBC_MIP_SOLVER = 3,
    #endif  // USE_CBC
    #if defined(USE_GLPK)
    KNAPSACK_MULTIDIMENSION_GLPK_MIP_SOLVER = 4,
    #endif  // USE_GLPK
    KNAPSACK_MULTIDIMENSION_BRANCH_AND_BOUND_SOLVER = 5,
    #if defined(USE_SCIP)
    KNAPSACK_MULTIDIMENSION_SCIP_MIP_SOLVER = 6,
    #endif  // USE_SCIP
  };

  explicit KnapsackSolver(const std::string& solver_name);
  KnapsackSolver(SolverType solver_type, const std::string& solver_name);
  virtual ~KnapsackSolver();

  // Initializes the solver and enters the problem to be solved.
  void Init(const std::vector<int64>& profits,
            const std::vector<std::vector<int64> >& weights,
            const std::vector<int64>& capacities);

  // Solves the problem and returns the profit of the optimal solution.
  int64 Solve();

  // Returns true if the item 'item_id' is packed in the optimal knapsack.
  bool BestSolutionContains(int item_id) const;
  // Returns true if the solution was proven optimal.
  bool IsSolutionOptimal() const { return is_solution_optimal_; }
  std::string GetName() const;

  bool use_reduction() const { return use_reduction_; }
  void set_use_reduction(bool use_reduction) { use_reduction_ = use_reduction; }

  // Time limit in seconds. When a finite time limit is set the solution
  // obtained might not be optimal if the limit is reached.
  void set_time_limit(double time_limit_seconds) {
    time_limit_seconds_ = time_limit_seconds;
  }

 private:
  // Trivial reduction of capacity constraints when the capacity is higher than
  // the sum of the weights of the items. Returns the number of reduced items.
  int ReduceCapacities(int num_items,
                       const std::vector<std::vector<int64> >& weights,
                       const std::vector<int64>& capacities,
                       std::vector<std::vector<int64> >* reduced_weights,
                       std::vector<int64>* reduced_capacities);
  int ReduceProblem(int num_items);
  void ComputeAdditionalProfit(const std::vector<int64>& profits);
  void InitReducedProblem(const std::vector<int64>& profits,
                          const std::vector<std::vector<int64> >& weights,
                          const std::vector<int64>& capacities);

  std::unique_ptr<BaseKnapsackSolver> solver_;
  std::vector<bool> known_value_;
  std::vector<bool> best_solution_;
  bool is_solution_optimal_ = false;
  std::vector<int> mapping_reduced_item_id_;
  bool is_problem_solved_;
  int64 additional_profit_;
  bool use_reduction_;
  double time_limit_seconds_;
  std::unique_ptr<TimeLimit> time_limit_;

  DISALLOW_COPY_AND_ASSIGN(KnapsackSolver);
};

#if !defined(SWIG)
// The following code defines needed classes for the KnapsackGenericSolver
// class which is the entry point to extend knapsack with new constraints such
// as conflicts between items.
//
// Constraints are enforced using KnapsackPropagator objects, in the current
// code there is one propagator per dimension (KnapsackCapacityPropagator).
// One of those propagators, named master propagator, is used to guide the
// search, i.e. decides which item should be assigned next.
// Roughly speaking the search algorithm is:
//  - While not optimal
//    - Select next search node to expand
//    - Select next item_i to assign (using master propagator)
//    - Generate a new search node where item_i is in the knapsack
//      - Check validity of this new partial solution (using propagators)
//      - If valid, add this new search node to the search
//    - Generate a new search node where item_i is not in the knapsack
//      - Check validity of this new partial solution (using propagators)
//      - If valid, add this new search node to the search
//
// TODO(user): Add a new propagator class for conflict constraint.
// TODO(user): Add a new propagator class used as a guide when the problem has
// several dimensions.

// ----- KnapsackAssignement -----
// KnapsackAssignement is a small struct used to pair an item with its
// assignment. It is mainly used for search nodes and updates.
struct KnapsackAssignment {
  KnapsackAssignment(int _item_id, bool _is_in)
      : item_id(_item_id), is_in(_is_in) {}
  int item_id;
  bool is_in;
};

// ----- KnapsackItem -----
// KnapsackItem is a small struct to pair an item weight with its
// corresponding profit.
// The aim of the knapsack problem is to pack as many valuable items as
// possible. A straight forward heuristic is to take those with the greatest
// profit-per-unit-weight. This ratio is called efficiency in this
// implementation. So items will be grouped in vectors, and sorted by
// decreasing efficiency.
// Note that profits are duplicated for each dimension. This is done to
// simplify the code, especially the GetEfficiency method and vector sorting.
// As there usually are only few dimensions, the overhead should not be an
// issue.
struct KnapsackItem {
  KnapsackItem(int _id, int64 _weight, int64 _profit)
      : id(_id), weight(_weight), profit(_profit) {}
  double GetEfficiency(int64 profit_max) const {
    return (weight > 0)
               ? static_cast<double>(profit) / static_cast<double>(weight)
               : static_cast<double>(profit_max);
  }

  // The 'id' field is used to retrieve the initial item in order to
  // communicate with other propagators and state.
  const int id;
  const int64 weight;
  const int64 profit;
};
typedef KnapsackItem* KnapsackItemPtr;

// ----- KnapsackSearchNode -----
// KnapsackSearchNode is a class used to describe a decision in the decision
// search tree.
// The node is defined by a pointer to the parent search node and an
// assignment (see KnapsackAssignement).
// As the current state is not explicitly stored in a search node, one should
// go through the search tree to incrementally build a partial solution from
// a previous search node.
class KnapsackSearchNode {
 public:
  KnapsackSearchNode(const KnapsackSearchNode* const parent,
                     const KnapsackAssignment& assignment);
  int depth() const { return depth_; }
  const KnapsackSearchNode* const parent() const { return parent_; }
  const KnapsackAssignment& assignment() const { return assignment_; }

  int64 current_profit() const { return current_profit_; }
  void set_current_profit(int64 profit) { current_profit_ = profit; }

  int64 profit_upper_bound() const { return profit_upper_bound_; }
  void set_profit_upper_bound(int64 profit) { profit_upper_bound_ = profit; }

  int next_item_id() const { return next_item_id_; }
  void set_next_item_id(int id) { next_item_id_ = id; }

 private:
  // 'depth' field is used to navigate efficiently through the search tree
  // (see KnapsackSearchPath).
  int depth_;
  const KnapsackSearchNode* const parent_;
  KnapsackAssignment assignment_;

  // 'current_profit' and 'profit_upper_bound' fields are used to sort search
  // nodes using a priority queue. That allows to pop the node with the best
  // upper bound, and more importantly to stop the search when optimality is
  // proved.
  int64 current_profit_;
  int64 profit_upper_bound_;

  // 'next_item_id' field allows to avoid an O(number_of_items) scan to find
  // next item to select. This is done for free by the upper bound computation.
  int next_item_id_;

  DISALLOW_COPY_AND_ASSIGN(KnapsackSearchNode);
};

// ----- KnapsackSearchPath -----
// KnapsackSearchPath is a small class used to represent the path between a
// node to another node in the search tree.
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
class KnapsackSearchPath {
 public:
  KnapsackSearchPath(const KnapsackSearchNode& from,
                     const KnapsackSearchNode& to);
  void Init();
  const KnapsackSearchNode& from() const { return from_; }
  const KnapsackSearchNode& via() const { return *via_; }
  const KnapsackSearchNode& to() const { return to_; }
  const KnapsackSearchNode* MoveUpToDepth(const KnapsackSearchNode& node,
                                          int depth) const;

 private:
  const KnapsackSearchNode& from_;
  const KnapsackSearchNode* via_;  // Computed in 'Init'.
  const KnapsackSearchNode& to_;

  DISALLOW_COPY_AND_ASSIGN(KnapsackSearchPath);
};

// ----- KnapsackState -----
// KnapsackState represents a partial solution to the knapsack problem.
class KnapsackState {
 public:
  KnapsackState();

  // Initializes vectors with number_of_items set to false (i.e. not bound yet).
  void Init(int number_of_items);
  // Updates the state by applying or reverting a decision.
  // Returns false if fails, i.e. trying to apply an inconsistent decision
  // to an already assigned item.
  bool UpdateState(bool revert, const KnapsackAssignment& assignment);

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

  DISALLOW_COPY_AND_ASSIGN(KnapsackState);
};

// ----- KnapsackPropagator -----
// KnapsackPropagator is the base class for modeling and propagating a
// constraint given an assignment.
//
// When some work has to be done both by the base and the derived class,
// a protected pure virtual method ending by 'Propagator' is defined.
// For instance, 'Init' creates a vector of items, and then calls
// 'InitPropagator' to let the derived class perform its own initialization.
class KnapsackPropagator {
 public:
  explicit KnapsackPropagator(const KnapsackState& state);
  virtual ~KnapsackPropagator();

  // Initializes data structure and then calls InitPropagator.
  void Init(const std::vector<int64>& profits,
            const std::vector<int64>& weights);

  // Updates data structure and then calls UpdatePropagator.
  // Returns false when failure.
  bool Update(bool revert, const KnapsackAssignment& assignment);
  // ComputeProfitBounds should set 'profit_lower_bound_' and
  // 'profit_upper_bound_' which are constraint specific.
  virtual void ComputeProfitBounds() = 0;
  // Returns the id of next item to assign.
  // Returns kNoSelection when all items are bound.
  virtual int GetNextItemId() const = 0;

  int64 current_profit() const { return current_profit_; }
  int64 profit_lower_bound() const { return profit_lower_bound_; }
  int64 profit_upper_bound() const { return profit_upper_bound_; }

  // Copies the current state into 'solution'.
  // All unbound items are set to false (i.e. not in the knapsack).
  // When 'has_one_propagator' is true, CopyCurrentSolutionPropagator is called
  // to have a better solution. When there is only one propagator
  // there is no need to check the solution with other propagators, so the
  // partial solution can be smartly completed.
  void CopyCurrentStateToSolution(bool has_one_propagator,
                                  std::vector<bool>* solution) const;

 protected:
  // Initializes data structure. This method is called after initialization
  // of KnapsackPropagator data structure.
  virtual void InitPropagator() = 0;

  // Updates internal data structure incrementally. This method is called
  // after update of KnapsackPropagator data structure.
  virtual bool UpdatePropagator(bool revert,
                                const KnapsackAssignment& assignment) = 0;

  // Copies the current state into 'solution'.
  // Only unbound items have to be copied as CopyCurrentSolution was already
  // called with current state.
  // This method is useful when a propagator is able to find a better solution
  // than the blind instantiation to false of unbound items.
  virtual void CopyCurrentStateToSolutionPropagator(std::vector<bool>* solution)
      const = 0;

  const KnapsackState& state() const { return state_; }
  const std::vector<KnapsackItemPtr>& items() const { return items_; }

  void set_profit_lower_bound(int64 profit) { profit_lower_bound_ = profit; }
  void set_profit_upper_bound(int64 profit) { profit_upper_bound_ = profit; }

 private:
  std::vector<KnapsackItemPtr> items_;
  int64 current_profit_;
  int64 profit_lower_bound_;
  int64 profit_upper_bound_;
  const KnapsackState& state_;

  DISALLOW_COPY_AND_ASSIGN(KnapsackPropagator);
};

// ----- KnapsackCapacityPropagator -----
// KnapsackCapacityPropagator is a KnapsackPropagator used to enforce
// a capacity constraint.
// As a KnapsackPropagator is supposed to compute profit lower and upper
// bounds, and get the next item to select, it can be seen as a 0-1 Knapsack
// solver. The most efficient way to compute the upper bound is to iterate on
// items in profit-per-unit-weight decreasing order. The break item is
// commonly defined as the first item for which there is not enough remaining
// capacity. Selecting this break item as the next-item-to-assign usually
// gives the best results (see Greenberg & Hegerich).
//
// This is exactly what is implemented in this class.
//
// When there is only one propagator, it is possible to compute a better
// profit lower bound almost for free. During the scan to find the
// break element all unbound items are added just as if they were part of
// the current solution. This is used in both ComputeProfitBounds and
// CopyCurrentSolutionPropagator.
// For incrementality reasons, the ith item should be accessible in O(1). That's
// the reason why the item vector has to be duplicated 'sorted_items_'.
class KnapsackCapacityPropagator : public KnapsackPropagator {
 public:
  KnapsackCapacityPropagator(const KnapsackState& state, int64 capacity);
  ~KnapsackCapacityPropagator() override;
  void ComputeProfitBounds() override;
  int GetNextItemId() const override { return break_item_id_; }

 protected:
  // Initializes KnapsackCapacityPropagator (e.g., sort items in decreasing
  // order).
  void InitPropagator() override;
  // Updates internal data structure incrementally (i.e., 'consumed_capacity_')
  // to avoid a O(number_of_items) scan.
  bool UpdatePropagator(bool revert,
                        const KnapsackAssignment& assignment) override;
  void CopyCurrentStateToSolutionPropagator(
      std::vector<bool>* solution) const override;

 private:
  // An obvious additional profit upper bound corresponds to the linear
  // relaxation: remaining_capacity * efficiency of the break item.
  // It is possible to do better in O(1), using Martello-Toth bound U2.
  // The main idea is to enforce integrality constraint on the break item,
  // ie. either the break item is part of the solution, either it is not.
  // So basically the linear relaxation is done on the item before the break
  // item, or the one after the break item.
  // This is what GetAdditionalProfit method implements.
  int64 GetAdditionalProfit(int64 remaining_capacity, int break_item_id) const;

  const int64 capacity_;
  int64 consumed_capacity_;
  int break_item_id_;
  std::vector<KnapsackItemPtr> sorted_items_;
  int64 profit_max_;

  DISALLOW_COPY_AND_ASSIGN(KnapsackCapacityPropagator);
};

// ----- BaseKnapsackSolver -----
// This is the base class for knapsack solvers.
class BaseKnapsackSolver {
 public:
  explicit BaseKnapsackSolver(const std::string& solver_name)
      : solver_name_(solver_name) {}
  virtual ~BaseKnapsackSolver() {}

  // Initializes the solver and enters the problem to be solved.
  virtual void Init(const std::vector<int64>& profits,
                    const std::vector<std::vector<int64> >& weights,
                    const std::vector<int64>& capacities) = 0;

  // Gets the lower and upper bound when the item is in or out of the knapsack.
  // To ensure objects are correctly initialized, this method should not be
  // called before ::Init.
  virtual void GetLowerAndUpperBoundWhenItem(int item_id, bool is_item_in,
                                             int64* lower_bound,
                                             int64* upper_bound);

  // Solves the problem and returns the profit of the optimal solution.
  virtual int64 Solve(TimeLimit* time_limit, bool* is_solution_optimal) = 0;

  // Returns true if the item 'item_id' is packed in the optimal knapsack.
  virtual bool best_solution(int item_id) const = 0;

  virtual std::string GetName() const { return solver_name_; }

 private:
  const std::string solver_name_;
};

// ----- KnapsackGenericSolver -----
// KnapsackGenericSolver is the multi-dimensional knapsack solver class.
// In the current implementation, the next item to assign is given by the
// master propagator. Using SetMasterPropagator allows changing the default
// (propagator of the first dimension), and selecting another dimension when
// more constrained.
// TODO(user): In the case of a multi-dimensional knapsack problem, implement
// an aggregated propagator to combine all dimensions and give a better guide
// to select the next item (see, for instance, Dobson's aggregated efficiency).
class KnapsackGenericSolver : public BaseKnapsackSolver {
 public:
  explicit KnapsackGenericSolver(const std::string& solver_name);
  ~KnapsackGenericSolver() override;

  // Initializes the solver and enters the problem to be solved.
  void Init(const std::vector<int64>& profits,
            const std::vector<std::vector<int64> >& weights,
            const std::vector<int64>& capacities) override;
  int GetNumberOfItems() const { return state_.GetNumberOfItems(); }
  void GetLowerAndUpperBoundWhenItem(int item_id, bool is_item_in,
                                     int64* lower_bound,
                                     int64* upper_bound) override;

  // Sets which propagator should be used to guide the search.
  // 'master_propagator_id' should be in 0..p-1 with p the number of
  // propagators.
  void set_master_propagator_id(int master_propagator_id) {
    master_propagator_id_ = master_propagator_id;
  }

  // Solves the problem and returns the profit of the optimal solution.
  int64 Solve(TimeLimit* time_limit, bool* is_solution_optimal) override;
  // Returns true if the item 'item_id' is packed in the optimal knapsack.
  bool best_solution(int item_id) const override {
    return best_solution_.at(item_id);
  }

 private:
  // Clears internal data structure.
  void Clear();

  // Updates all propagators reverting/applying all decision on the path.
  // Returns true if fails. Note that, even if fails, all propagators should
  // be updated to be in a stable state in order to stay incremental.
  bool UpdatePropagators(const KnapsackSearchPath& path);
  // Updates all propagators reverting/applying one decision.
  // Return true if fails. Note that, even if fails, all propagators should
  // be updated to be in a stable state in order to stay incremental.
  bool IncrementalUpdate(bool revert, const KnapsackAssignment& assignment);
  // Updates the best solution if the current solution has a better profit.
  void UpdateBestSolution();

  // Returns true if new relevant search node was added to the nodes array, that
  // means this node should be added to the search queue too.
  bool MakeNewNode(const KnapsackSearchNode& node, bool is_in);

  // Gets the aggregated (min) profit upper bound among all propagators.
  int64 GetAggregatedProfitUpperBound() const;
  bool HasOnePropagator() const { return propagators_.size() == 1; }
  int64 GetCurrentProfit() const {
    return propagators_.at(master_propagator_id_)->current_profit();
  }
  int64 GetNextItemId() const {
    return propagators_.at(master_propagator_id_)->GetNextItemId();
  }

  std::vector<KnapsackPropagator*> propagators_;
  int master_propagator_id_;
  std::vector<KnapsackSearchNode*> search_nodes_;
  KnapsackState state_;
  int64 best_solution_profit_;
  std::vector<bool> best_solution_;

  DISALLOW_COPY_AND_ASSIGN(KnapsackGenericSolver);
};
#endif  // SWIG
}  // namespace operations_research

#endif  // OR_TOOLS_ALGORITHMS_KNAPSACK_SOLVER_H_
