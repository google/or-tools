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

#ifndef ORTOOLS_ALGORITHMS_PYTHON_KNAPSACK_SOLVER_DOC_H_
#define ORTOOLS_ALGORITHMS_PYTHON_KNAPSACK_SOLVER_DOC_H_
// clang-format off
/*
  This file contains docstrings for use in the Python bindings.
  Do not edit! They were automatically extracted by pybind11_mkdoc.
 */

#define MKD_EXPAND(x)                                      x
#define MKD_COUNT(_1, _2, _3, _4, _5, _6, _7, COUNT, ...)  COUNT
#define MKD_VA_SIZE(...)                                   MKD_EXPAND(MKD_COUNT(__VA_ARGS__, 7, 6, 5, 4, 3, 2, 1, 0))
#define MKD_CAT1(a, b)                                     a ## b
#define MKD_CAT2(a, b)                                     MKD_CAT1(a, b)
#define MKD_DOC1(n1)                                       mkd_doc_##n1
#define MKD_DOC2(n1, n2)                                   mkd_doc_##n1##_##n2
#define MKD_DOC3(n1, n2, n3)                               mkd_doc_##n1##_##n2##_##n3
#define MKD_DOC4(n1, n2, n3, n4)                           mkd_doc_##n1##_##n2##_##n3##_##n4
#define MKD_DOC5(n1, n2, n3, n4, n5)                       mkd_doc_##n1##_##n2##_##n3##_##n4##_##n5
#define MKD_DOC6(n1, n2, n3, n4, n5, n6)                   mkd_doc_##n1##_##n2##_##n3##_##n4##_##n5##_##n6
#define MKD_DOC7(n1, n2, n3, n4, n5, n6, n7)               mkd_doc_##n1##_##n2##_##n3##_##n4##_##n5##_##n6##_##n7
#define DOC(...)                                           MKD_EXPAND(MKD_EXPAND(MKD_CAT2(MKD_DOC, MKD_VA_SIZE(__VA_ARGS__)))(__VA_ARGS__))

#if defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif


static const char *mkd_doc_operations_research_BaseKnapsackSolver = R"doc(This is the base class for knapsack solvers.)doc";

static const char *mkd_doc_operations_research_BaseKnapsackSolver_2 = R"doc(This is the base class for knapsack solvers.)doc";

static const char *mkd_doc_operations_research_BaseKnapsackSolver_BaseKnapsackSolver = R"doc()doc";

static const char *mkd_doc_operations_research_BaseKnapsackSolver_GetLowerAndUpperBoundWhenItem = R"doc()doc";

static const char *mkd_doc_operations_research_BaseKnapsackSolver_GetName = R"doc()doc";

static const char *mkd_doc_operations_research_BaseKnapsackSolver_Init = R"doc(Initializes the solver and enters the problem to be solved.)doc";

static const char *mkd_doc_operations_research_BaseKnapsackSolver_Solve = R"doc(Solves the problem and returns the profit of the optimal solution.)doc";

static const char *mkd_doc_operations_research_BaseKnapsackSolver_best_solution = R"doc(Returns true if the item 'item_id' is packed in the optimal knapsack.)doc";

static const char *mkd_doc_operations_research_BaseKnapsackSolver_solver_name = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackAssignment =
R"doc(KnapsackAssignment is a small struct used to pair an item with its
assignment. It is mainly used for search nodes and updates.)doc";

static const char *mkd_doc_operations_research_KnapsackAssignment_KnapsackAssignment = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackAssignment_is_in = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackAssignment_item_id = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackCapacityPropagator =
R"doc( KnapsackCapacityPropagator is a KnapsackPropagator used to enforce
a capacity constraint.


As a KnapsackPropagator is supposed to compute profit lower and upper
bounds, and get the next item to select, it can be seen as a 0-1
Knapsack solver. The most efficient way to compute the upper bound is
to iterate on items in profit-per-unit-weight decreasing order. The
break item is commonly defined as the first item for which there is
not enough remaining capacity. Selecting this break item as the next-
item-to-assign usually gives the best results (see Greenberg &
Hegerich).\n This is exactly what is implemented in this class.\n When
there is only one propagator, it is possible to compute a better
profit lower bound almost for free. During the scan to find the break
element all unbound items are added just as if they were part of the
current solution. This is used in both ComputeProfitBounds and
CopyCurrentSolutionPropagator.\n For incrementality reasons, the ith
item should be accessible in O(1). That's the reason why the item
vector has to be duplicated 'sorted_items_'.)doc";

static const char *mkd_doc_operations_research_KnapsackCapacityPropagator_ComputeProfitBounds = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackCapacityPropagator_CopyCurrentStateToSolutionPropagator = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackCapacityPropagator_GetAdditionalProfit =
R"doc(An obvious additional profit upper bound corresponds to the linear
relaxation: remaining_capacity * efficiency of the break item.
It is possible to do better in O(1), using Martello-Toth bound U2. The
main idea is to enforce integrality constraint on the break item, ie.
either the break item is part of the solution, either it is not. So
basically the linear relaxation is done on the item before the break
item, or the one after the break item. This is what
GetAdditionalProfit method implements.)doc";

static const char *mkd_doc_operations_research_KnapsackCapacityPropagator_GetNextItemId = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackCapacityPropagator_InitPropagator =
R"doc(Initializes KnapsackCapacityPropagator (e.g., sort items in decreasing
order).)doc";

static const char *mkd_doc_operations_research_KnapsackCapacityPropagator_KnapsackCapacityPropagator = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackCapacityPropagator_KnapsackCapacityPropagator_2 = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackCapacityPropagator_UpdatePropagator =
R"doc(Updates internal data structure incrementally (i.e.,
'consumed_capacity_') to avoid a O(number_of_items) scan.)doc";

static const char *mkd_doc_operations_research_KnapsackCapacityPropagator_break_item_id = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackCapacityPropagator_capacity = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackCapacityPropagator_consumed_capacity = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackCapacityPropagator_operator_assign = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackCapacityPropagator_profit_max = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackCapacityPropagator_sorted_items = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackGenericSolver =
R"doc( KnapsackGenericSolver is the multi-dimensional knapsack solver class.


In the current implementation, the next item to assign is given by the
primary propagator. Using SetPrimaryPropagator allows changing the
default (propagator of the first dimension), and selecting another
dimension when more constrained. TODO(user): In the case of a multi-
dimensional knapsack problem, implement an aggregated propagator to
combine all dimensions and give a better guide to select the next item
(see, for instance, Dobson's aggregated efficiency).)doc";

static const char *mkd_doc_operations_research_KnapsackGenericSolver_Clear = R"doc(Clears internal data structure.)doc";

static const char *mkd_doc_operations_research_KnapsackGenericSolver_GetAggregatedProfitUpperBound = R"doc(Gets the aggregated (min) profit upper bound among all propagators.)doc";

static const char *mkd_doc_operations_research_KnapsackGenericSolver_GetCurrentProfit = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackGenericSolver_GetLowerAndUpperBoundWhenItem = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackGenericSolver_GetNextItemId = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackGenericSolver_GetNumberOfItems = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackGenericSolver_HasOnePropagator = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackGenericSolver_IncrementalUpdate =
R"doc(Updates all propagators reverting/applying one decision. Return true
if fails. Note that, even if fails, all propagators should be updated
to be in a stable state in order to stay incremental.)doc";

static const char *mkd_doc_operations_research_KnapsackGenericSolver_Init = R"doc(Initializes the solver and enters the problem to be solved.)doc";

static const char *mkd_doc_operations_research_KnapsackGenericSolver_KnapsackGenericSolver = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackGenericSolver_KnapsackGenericSolver_2 = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackGenericSolver_MakeNewNode =
R"doc(Returns true if new relevant search node was added to the nodes array,
that means this node should be added to the search queue too.)doc";

static const char *mkd_doc_operations_research_KnapsackGenericSolver_Solve = R"doc(Solves the problem and returns the profit of the optimal solution.)doc";

static const char *mkd_doc_operations_research_KnapsackGenericSolver_UpdateBestSolution = R"doc(Updates the best solution if the current solution has a better profit.)doc";

static const char *mkd_doc_operations_research_KnapsackGenericSolver_UpdatePropagators =
R"doc(Updates all propagators reverting/applying all decision on the path.
Returns true if fails. Note that, even if fails, all propagators
should be updated to be in a stable state in order to stay
incremental.)doc";

static const char *mkd_doc_operations_research_KnapsackGenericSolver_best_solution = R"doc(Returns true if the item 'item_id' is packed in the optimal knapsack.)doc";

static const char *mkd_doc_operations_research_KnapsackGenericSolver_best_solution_2 = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackGenericSolver_best_solution_profit = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackGenericSolver_operator_assign = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackGenericSolver_primary_propagator_id = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackGenericSolver_propagators = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackGenericSolver_search_nodes = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackGenericSolver_set_primary_propagator_id =
R"doc(Sets which propagator should be used to guide the search.
'primary_propagator_id' should be in 0..p-1 with p the number of
propagators.)doc";

static const char *mkd_doc_operations_research_KnapsackGenericSolver_state = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackItem =
R"doc(KnapsackItem is a small struct to pair an item weight with its
corresponding profit.


The aim of the knapsack problem is to pack as many valuable items as
possible. A straight forward heuristic is to take those with the
greatest profit-per-unit-weight. This ratio is called efficiency in
this implementation. So items will be grouped in vectors, and sorted
by decreasing efficiency. Note that profits are duplicated for each
dimension. This is done to simplify the code, especially the
GetEfficiency method and vector sorting. As there usually are only few
dimensions, the overhead should not be an issue.)doc";

static const char *mkd_doc_operations_research_KnapsackItem_GetEfficiency = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackItem_KnapsackItem = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackItem_id =
R"doc(The 'id' field is used to retrieve the initial item in order to
communicate with other propagators and state.)doc";

static const char *mkd_doc_operations_research_KnapsackItem_profit = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackItem_weight = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackPropagator =
R"doc( KnapsackPropagator is the base class for modeling and propagating a
constraint given an assignment.


When some work has to be done both by the base and the derived class,
a protected pure virtual method ending by 'Propagator' is defined. For
instance, 'Init' creates a vector of items, and then calls
'InitPropagator' to let the derived class perform its own
initialization.)doc";

static const char *mkd_doc_operations_research_KnapsackPropagator_ComputeProfitBounds =
R"doc(ComputeProfitBounds should set 'profit_lower_bound_' and
'profit_upper_bound_' which are constraint specific.)doc";

static const char *mkd_doc_operations_research_KnapsackPropagator_CopyCurrentStateToSolution =
R"doc(Copies the current state into 'solution'. All unbound items are set to
false (i.e. not in the knapsack). When 'has_one_propagator' is true,
CopyCurrentSolutionPropagator is called to have a better solution.
When there is only one propagator there is no need to check the
solution with other propagators, so the partial solution can be
smartly completed.)doc";

static const char *mkd_doc_operations_research_KnapsackPropagator_CopyCurrentStateToSolutionPropagator =
R"doc(Copies the current state into 'solution'. Only unbound items have to
be copied as CopyCurrentSolution was already called with current
state. This method is useful when a propagator is able to find a
better solution than the blind instantiation to false of unbound
items.)doc";

static const char *mkd_doc_operations_research_KnapsackPropagator_GetNextItemId =
R"doc(Returns the id of next item to assign. Returns kNoSelection when all
items are bound.)doc";

static const char *mkd_doc_operations_research_KnapsackPropagator_Init = R"doc(Initializes data structure and then calls InitPropagator.)doc";

static const char *mkd_doc_operations_research_KnapsackPropagator_InitPropagator =
R"doc(Initializes data structure. This method is called after initialization
of KnapsackPropagator data structure.)doc";

static const char *mkd_doc_operations_research_KnapsackPropagator_KnapsackPropagator = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackPropagator_KnapsackPropagator_2 = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackPropagator_Update =
R"doc(Updates data structure and then calls UpdatePropagator. Returns false
when failure.)doc";

static const char *mkd_doc_operations_research_KnapsackPropagator_UpdatePropagator =
R"doc(Updates internal data structure incrementally. This method is called
after update of KnapsackPropagator data structure.)doc";

static const char *mkd_doc_operations_research_KnapsackPropagator_current_profit = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackPropagator_current_profit_2 = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackPropagator_items = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackPropagator_items_2 = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackPropagator_operator_assign = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackPropagator_profit_lower_bound = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackPropagator_profit_lower_bound_2 = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackPropagator_profit_upper_bound = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackPropagator_profit_upper_bound_2 = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackPropagator_set_profit_lower_bound = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackPropagator_set_profit_upper_bound = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackPropagator_state = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackPropagator_state_2 = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSearchNode =
R"doc(KnapsackSearchNode is a class used to describe a decision in the
decision search tree.


The node is defined by a pointer to the parent search node and an
assignment (see KnapsackAssignement). As the current state is not
explicitly stored in a search node, one should go through the search
tree to incrementally build a partial solution from a previous search
node.)doc";

static const char *mkd_doc_operations_research_KnapsackSearchNode_KnapsackSearchNode = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSearchNode_KnapsackSearchNode_2 = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSearchNode_assignment = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSearchNode_assignment_2 = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSearchNode_current_profit = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSearchNode_current_profit_2 =
R"doc('current_profit' and 'profit_upper_bound' fields are used to sort
search nodes using a priority queue. That allows to pop the node with
the best upper bound, and more importantly to stop the search when
optimality is proved.)doc";

static const char *mkd_doc_operations_research_KnapsackSearchNode_depth = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSearchNode_depth_2 =
R"doc('depth' field is used to navigate efficiently through the search tree
(see KnapsackSearchPath).)doc";

static const char *mkd_doc_operations_research_KnapsackSearchNode_next_item_id = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSearchNode_next_item_id_2 =
R"doc('next_item_id' field allows to avoid an O(number_of_items) scan to
find next item to select. This is done for free by the upper bound
computation.)doc";

static const char *mkd_doc_operations_research_KnapsackSearchNode_operator_assign = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSearchNode_parent = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSearchNode_parent_2 = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSearchNode_profit_upper_bound = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSearchNode_profit_upper_bound_2 = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSearchNode_set_current_profit = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSearchNode_set_next_item_id = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSearchNode_set_profit_upper_bound = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSearchPath =
R"doc(KnapsackSearchPath is a small class used to represent the path between
a node to another node in the search tree.


As the solution state is not stored for each search node, the state
should be rebuilt at each node. One simple solution is to apply all
decisions between the node 'to' and the root. This can be computed in
O(number_of_items).

However, it is possible to achieve better average complexity. Two
consecutively explored nodes are usually close enough (i.e., much less
than number_of_items) to benefit from an incremental update from the
node 'from' to the node 'to'.

The 'via' field is the common parent of 'from' field and 'to' field.
So the state can be built by reverting all decisions from 'from' to
'via' and then applying all decisions from 'via' to 'to'.)doc";

static const char *mkd_doc_operations_research_KnapsackSearchPath_Init = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSearchPath_KnapsackSearchPath = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSearchPath_KnapsackSearchPath_2 = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSearchPath_MoveUpToDepth = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSearchPath_from = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSearchPath_from_2 = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSearchPath_operator_assign = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSearchPath_to = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSearchPath_to_2 = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSearchPath_via = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSearchPath_via_2 = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSolver =
R"doc( @file
This library solves knapsack problems. Problems the library solves
include:
- 0-1 knapsack problems, - Multi-dimensional knapsack problems,

Given n items, each with a profit and a weight, given a knapsack of
capacity c, the goal is to find a subset of items which fits inside c
and maximizes the total profit.\n The knapsack problem can easily be
extended from 1 to d dimensions. As an example, this can be useful to
constrain the maximum number of items inside the knapsack.\n Without
loss of generality, profits and weights are assumed to be positive.
From a mathematical point of view, the multi-dimensional knapsack
problem can be modeled by d linear constraints:```
ForEach(j:1..d)(Sum(i:1..n)(weight_ij * item_i) <= c_j
where item_i is a 0-1 integer variable.
```



Then the goal is to maximize:```
Sum(i:1..n)(profit_i * item_i).
```



There are several ways to solve knapsack problems. One of the most
efficient is based on dynamic programming (mainly when weights,
profits and dimensions are small, and the algorithm runs in pseudo
polynomial time). Unfortunately, when adding conflict constraints the
problem becomes strongly NP-hard, i.e. there is no pseudo-polynomial
algorithm to solve it.\n That's the reason why the most of the
following code is based on branch and bound search.\n For instance to
solve a 2-dimensional knapsack problem with 9 items, one just has to
feed a profit vector with the 9 profits, a vector of 2 vectors for
weights, and a vector of capacities.\n E.g.:\n **Python:**```
{.py}
profits = [ 1, 2, 3, 4, 5, 6, 7, 8, 9 ]
weights = [ [ 1, 2, 3, 4, 5, 6, 7, 8, 9 ],
[ 1, 1, 1, 1, 1, 1, 1, 1, 1 ]
]
capacities = [ 34, 4 ]
solver = knapsack_solver.KnapsackSolver(
knapsack_solver.SolverType
.KNAPSACK_MULTIDIMENSION_BRANCH_AND_BOUND_SOLVER,
'Multi-dimensional solver')
solver.init(profits, weights, capacities)
profit = solver.solve()
```



**C++:**```
{.cpp}
const std::vector<int64_t> profits = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
const std::vector<std::vector<int64_t>> weights =
{ { 1, 2, 3, 4, 5, 6, 7, 8, 9 },
{ 1, 1, 1, 1, 1, 1, 1, 1, 1 } };
const std::vector<int64_t> capacities = { 34, 4 };
KnapsackSolver solver(
KnapsackSolver::KNAPSACK_MULTIDIMENSION_BRANCH_AND_BOUND_SOLVER,
"Multi-dimensional solver");
solver.Init(profits, weights, capacities);
const int64_t profit = solver.Solve();
```



**Java:**```
{.java}
final long[] profits = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
final long[][] weights = { { 1, 2, 3, 4, 5, 6, 7, 8, 9 },
{ 1, 1, 1, 1, 1, 1, 1, 1, 1 } };
final long[] capacities = { 34, 4 };
KnapsackSolver solver = new KnapsackSolver(
KnapsackSolver.SolverType.KNAPSACK_MULTIDIMENSION_BRANCH_AND_BOUND_SOLVER,
"Multi-dimensional solver");
solver.init(profits, weights, capacities);
final long profit = solver.solve();
```)doc";

static const char *mkd_doc_operations_research_KnapsackSolver_BestSolutionContains = R"doc(Returns true if the item 'item_id' is packed in the optimal knapsack.)doc";

static const char *mkd_doc_operations_research_KnapsackSolver_ComputeAdditionalProfit = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSolver_GetName = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSolver_Init = R"doc(Initializes the solver and enters the problem to be solved.)doc";

static const char *mkd_doc_operations_research_KnapsackSolver_InitReducedProblem = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSolver_IsSolutionOptimal = R"doc(Returns true if the solution was proven optimal.)doc";

static const char *mkd_doc_operations_research_KnapsackSolver_KnapsackSolver = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSolver_KnapsackSolver_2 = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSolver_KnapsackSolver_3 = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSolver_ReduceCapacities =
R"doc(Trivial reduction of capacity constraints when the capacity is higher
than the sum of the weights of the items. Returns the number of
reduced items.)doc";

static const char *mkd_doc_operations_research_KnapsackSolver_ReduceProblem = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSolver_Solve = R"doc(Solves the problem and returns the profit of the optimal solution.)doc";

static const char *mkd_doc_operations_research_KnapsackSolver_SolverType =
R"doc(Enum controlling which underlying algorithm is used.


This enum is passed to the constructor of the KnapsackSolver object.
It selects which solving method will be used.)doc";

static const char *mkd_doc_operations_research_KnapsackSolver_SolverType_KNAPSACK_64ITEMS_SOLVER =
R"doc(Optimized method for single dimension small problems

Limited to 64 items and one dimension, this solver uses a branch &
bound algorithm. This solver is about 4 times faster than
KNAPSACK_MULTIDIMENSION_BRANCH_AND_BOUND_SOLVER.)doc";

static const char *mkd_doc_operations_research_KnapsackSolver_SolverType_KNAPSACK_BRUTE_FORCE_SOLVER =
R"doc(Brute force method.

Limited to 30 items and one dimension, this solver uses a brute force
algorithm, ie. explores all possible states. Experiments show
competitive performance for instances with less than 15 items.)doc";

static const char *mkd_doc_operations_research_KnapsackSolver_SolverType_KNAPSACK_DIVIDE_AND_CONQUER_SOLVER =
R"doc(Divide and Conquer approach for single dimension problems

Limited to one dimension, this solver is based on a divide and conquer
technique and is suitable for larger problems than Dynamic Programming
Solver. The time complexity is O(capacity * number_of_items) and the
space complexity is O(capacity + number_of_items).)doc";

static const char *mkd_doc_operations_research_KnapsackSolver_SolverType_KNAPSACK_DYNAMIC_PROGRAMMING_SOLVER =
R"doc(Dynamic Programming approach for single dimension problems

Limited to one dimension, this solver is based on a dynamic
programming algorithm. The time and space complexity is O(capacity *
number_of_items).)doc";

static const char *mkd_doc_operations_research_KnapsackSolver_SolverType_KNAPSACK_MULTIDIMENSION_BRANCH_AND_BOUND_SOLVER =
R"doc(Generic Solver.

This solver can deal with both large number of items and several
dimensions. This solver is based on branch and bound.)doc";

static const char *mkd_doc_operations_research_KnapsackSolver_SolverType_KNAPSACK_MULTIDIMENSION_CBC_MIP_SOLVER =
R"doc(CBC Based Solver

 This solver can deal with both large number of items and several
dimensions. This solver is based on Integer Programming solver CBC.)doc";

static const char *mkd_doc_operations_research_KnapsackSolver_SolverType_KNAPSACK_MULTIDIMENSION_CPLEX_MIP_SOLVER =
R"doc(CPLEX based solver

This solver can deal with both large number of items and several
dimensions. This solver is based on Integer Programming solver CPLEX.)doc";

static const char *mkd_doc_operations_research_KnapsackSolver_SolverType_KNAPSACK_MULTIDIMENSION_CP_SAT_SOLVER =
R"doc(CP-SAT based solver

This solver can deal with both large number of items and several
dimensions. This solver is based on the CP-SAT solver)doc";

static const char *mkd_doc_operations_research_KnapsackSolver_SolverType_KNAPSACK_MULTIDIMENSION_SCIP_MIP_SOLVER =
R"doc(SCIP based solver

This solver can deal with both large number of items and several
dimensions. This solver is based on Integer Programming solver SCIP.)doc";

static const char *mkd_doc_operations_research_KnapsackSolver_SolverType_KNAPSACK_MULTIDIMENSION_XPRESS_MIP_SOLVER =
R"doc(XPRESS based solver

This solver can deal with both large number of items and several
dimensions. This solver is based on Integer Programming solver XPRESS.)doc";

static const char *mkd_doc_operations_research_KnapsackSolver_additional_profit = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSolver_best_solution = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSolver_is_problem_solved = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSolver_is_solution_optimal = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSolver_known_value = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSolver_mapping_reduced_item_id = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSolver_operator_assign = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSolver_set_time_limit =
R"doc(Time limit in seconds.


When a finite time limit is set the solution obtained might not be
optimal if the limit is reached.)doc";

static const char *mkd_doc_operations_research_KnapsackSolver_set_use_reduction = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSolver_solver = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSolver_time_limit = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSolver_time_limit_seconds = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSolver_use_reduction = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackSolver_use_reduction_2 = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackState = R"doc(KnapsackState represents a partial solution to the knapsack problem.)doc";

static const char *mkd_doc_operations_research_KnapsackState_GetNumberOfItems = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackState_Init =
R"doc(Initializes vectors with number_of_items set to false (i.e. not bound
yet).)doc";

static const char *mkd_doc_operations_research_KnapsackState_KnapsackState = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackState_KnapsackState_2 = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackState_UpdateState =
R"doc(Updates the state by applying or reverting a decision. Returns false
if fails, i.e. trying to apply an inconsistent decision to an already
assigned item.)doc";

static const char *mkd_doc_operations_research_KnapsackState_is_bound = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackState_is_bound_2 =
R"doc(Vectors 'is_bound_' and 'is_in_' contain a boolean value for each
item. 'is_bound_(item_i)' is false when there is no decision for
item_i yet. When item_i is bound, 'is_in_(item_i)' represents the
presence (true) or the absence (false) of item_i in the current
solution.)doc";

static const char *mkd_doc_operations_research_KnapsackState_is_in = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackState_is_in_2 = R"doc()doc";

static const char *mkd_doc_operations_research_KnapsackState_operator_assign = R"doc()doc";

#if defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

// clang-format on
#endif  // ORTOOLS_ALGORITHMS_PYTHON_KNAPSACK_SOLVER_DOC_H_
