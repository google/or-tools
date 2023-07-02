/*
  This file contains docstrings for use in the Python bindings.
  Do not edit! They were automatically extracted by pybind11_mkdoc.
 */

#define __EXPAND(x)                                      x
#define __COUNT(_1, _2, _3, _4, _5, _6, _7, COUNT, ...)  COUNT
#define __VA_SIZE(...)                                   __EXPAND(__COUNT(__VA_ARGS__, 7, 6, 5, 4, 3, 2, 1, 0))
#define __CAT1(a, b)                                     a ## b
#define __CAT2(a, b)                                     __CAT1(a, b)
#define __DOC1(n1)                                       __doc_##n1
#define __DOC2(n1, n2)                                   __doc_##n1##_##n2
#define __DOC3(n1, n2, n3)                               __doc_##n1##_##n2##_##n3
#define __DOC4(n1, n2, n3, n4)                           __doc_##n1##_##n2##_##n3##_##n4
#define __DOC5(n1, n2, n3, n4, n5)                       __doc_##n1##_##n2##_##n3##_##n4##_##n5
#define __DOC6(n1, n2, n3, n4, n5, n6)                   __doc_##n1##_##n2##_##n3##_##n4##_##n5##_##n6
#define __DOC7(n1, n2, n3, n4, n5, n6, n7)               __doc_##n1##_##n2##_##n3##_##n4##_##n5##_##n6##_##n7
#define DOC(...)                                         __EXPAND(__EXPAND(__CAT2(__DOC, __VA_SIZE(__VA_ARGS__)))(__VA_ARGS__))

#if defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif


static const char *__doc_operations_research_BaseKnapsackSolver = R"doc()doc";

static const char *__doc_operations_research_BaseKnapsackSolver_2 = R"doc()doc";

static const char *__doc_operations_research_BaseKnapsackSolver_BaseKnapsackSolver = R"doc()doc";

static const char *__doc_operations_research_BaseKnapsackSolver_GetLowerAndUpperBoundWhenItem = R"doc()doc";

static const char *__doc_operations_research_BaseKnapsackSolver_GetName = R"doc()doc";

static const char *__doc_operations_research_BaseKnapsackSolver_Init = R"doc()doc";

static const char *__doc_operations_research_BaseKnapsackSolver_Solve = R"doc()doc";

static const char *__doc_operations_research_BaseKnapsackSolver_best_solution = R"doc()doc";

static const char *__doc_operations_research_BaseKnapsackSolver_solver_name = R"doc()doc";

static const char *__doc_operations_research_KnapsackAssignment = R"doc()doc";

static const char *__doc_operations_research_KnapsackAssignment_KnapsackAssignment = R"doc()doc";

static const char *__doc_operations_research_KnapsackAssignment_is_in = R"doc()doc";

static const char *__doc_operations_research_KnapsackAssignment_item_id = R"doc()doc";

static const char *__doc_operations_research_KnapsackCapacityPropagator = R"doc()doc";

static const char *__doc_operations_research_KnapsackCapacityPropagator_ComputeProfitBounds = R"doc()doc";

static const char *__doc_operations_research_KnapsackCapacityPropagator_CopyCurrentStateToSolutionPropagator = R"doc()doc";

static const char *__doc_operations_research_KnapsackCapacityPropagator_GetAdditionalProfit = R"doc()doc";

static const char *__doc_operations_research_KnapsackCapacityPropagator_GetNextItemId = R"doc()doc";

static const char *__doc_operations_research_KnapsackCapacityPropagator_InitPropagator = R"doc()doc";

static const char *__doc_operations_research_KnapsackCapacityPropagator_KnapsackCapacityPropagator = R"doc()doc";

static const char *__doc_operations_research_KnapsackCapacityPropagator_UpdatePropagator = R"doc()doc";

static const char *__doc_operations_research_KnapsackCapacityPropagator_break_item_id = R"doc()doc";

static const char *__doc_operations_research_KnapsackCapacityPropagator_capacity = R"doc()doc";

static const char *__doc_operations_research_KnapsackCapacityPropagator_consumed_capacity = R"doc()doc";

static const char *__doc_operations_research_KnapsackCapacityPropagator_profit_max = R"doc()doc";

static const char *__doc_operations_research_KnapsackCapacityPropagator_sorted_items = R"doc()doc";

static const char *__doc_operations_research_KnapsackGenericSolver = R"doc()doc";

static const char *__doc_operations_research_KnapsackGenericSolver_Clear = R"doc()doc";

static const char *__doc_operations_research_KnapsackGenericSolver_GetAggregatedProfitUpperBound = R"doc()doc";

static const char *__doc_operations_research_KnapsackGenericSolver_GetCurrentProfit = R"doc()doc";

static const char *__doc_operations_research_KnapsackGenericSolver_GetLowerAndUpperBoundWhenItem = R"doc()doc";

static const char *__doc_operations_research_KnapsackGenericSolver_GetNextItemId = R"doc()doc";

static const char *__doc_operations_research_KnapsackGenericSolver_GetNumberOfItems = R"doc()doc";

static const char *__doc_operations_research_KnapsackGenericSolver_HasOnePropagator = R"doc()doc";

static const char *__doc_operations_research_KnapsackGenericSolver_IncrementalUpdate = R"doc()doc";

static const char *__doc_operations_research_KnapsackGenericSolver_Init = R"doc()doc";

static const char *__doc_operations_research_KnapsackGenericSolver_KnapsackGenericSolver = R"doc()doc";

static const char *__doc_operations_research_KnapsackGenericSolver_MakeNewNode = R"doc()doc";

static const char *__doc_operations_research_KnapsackGenericSolver_Solve = R"doc()doc";

static const char *__doc_operations_research_KnapsackGenericSolver_UpdateBestSolution = R"doc()doc";

static const char *__doc_operations_research_KnapsackGenericSolver_UpdatePropagators = R"doc()doc";

static const char *__doc_operations_research_KnapsackGenericSolver_best_solution = R"doc()doc";

static const char *__doc_operations_research_KnapsackGenericSolver_best_solution_2 = R"doc()doc";

static const char *__doc_operations_research_KnapsackGenericSolver_best_solution_profit = R"doc()doc";

static const char *__doc_operations_research_KnapsackGenericSolver_primary_propagator_id = R"doc()doc";

static const char *__doc_operations_research_KnapsackGenericSolver_propagators = R"doc()doc";

static const char *__doc_operations_research_KnapsackGenericSolver_search_nodes = R"doc()doc";

static const char *__doc_operations_research_KnapsackGenericSolver_set_primary_propagator_id = R"doc()doc";

static const char *__doc_operations_research_KnapsackGenericSolver_state = R"doc()doc";

static const char *__doc_operations_research_KnapsackItem = R"doc()doc";

static const char *__doc_operations_research_KnapsackItem_GetEfficiency = R"doc()doc";

static const char *__doc_operations_research_KnapsackItem_KnapsackItem = R"doc()doc";

static const char *__doc_operations_research_KnapsackItem_id = R"doc()doc";

static const char *__doc_operations_research_KnapsackItem_profit = R"doc()doc";

static const char *__doc_operations_research_KnapsackItem_weight = R"doc()doc";

static const char *__doc_operations_research_KnapsackPropagator = R"doc()doc";

static const char *__doc_operations_research_KnapsackPropagator_ComputeProfitBounds = R"doc()doc";

static const char *__doc_operations_research_KnapsackPropagator_CopyCurrentStateToSolution = R"doc()doc";

static const char *__doc_operations_research_KnapsackPropagator_CopyCurrentStateToSolutionPropagator = R"doc()doc";

static const char *__doc_operations_research_KnapsackPropagator_GetNextItemId = R"doc()doc";

static const char *__doc_operations_research_KnapsackPropagator_Init = R"doc()doc";

static const char *__doc_operations_research_KnapsackPropagator_InitPropagator = R"doc()doc";

static const char *__doc_operations_research_KnapsackPropagator_KnapsackPropagator = R"doc()doc";

static const char *__doc_operations_research_KnapsackPropagator_Update = R"doc()doc";

static const char *__doc_operations_research_KnapsackPropagator_UpdatePropagator = R"doc()doc";

static const char *__doc_operations_research_KnapsackPropagator_current_profit = R"doc()doc";

static const char *__doc_operations_research_KnapsackPropagator_current_profit_2 = R"doc()doc";

static const char *__doc_operations_research_KnapsackPropagator_items = R"doc()doc";

static const char *__doc_operations_research_KnapsackPropagator_items_2 = R"doc()doc";

static const char *__doc_operations_research_KnapsackPropagator_profit_lower_bound = R"doc()doc";

static const char *__doc_operations_research_KnapsackPropagator_profit_lower_bound_2 = R"doc()doc";

static const char *__doc_operations_research_KnapsackPropagator_profit_upper_bound = R"doc()doc";

static const char *__doc_operations_research_KnapsackPropagator_profit_upper_bound_2 = R"doc()doc";

static const char *__doc_operations_research_KnapsackPropagator_set_profit_lower_bound = R"doc()doc";

static const char *__doc_operations_research_KnapsackPropagator_set_profit_upper_bound = R"doc()doc";

static const char *__doc_operations_research_KnapsackPropagator_state = R"doc()doc";

static const char *__doc_operations_research_KnapsackPropagator_state_2 = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchNode = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchNode_KnapsackSearchNode = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchNode_assignment = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchNode_assignment_2 = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchNode_current_profit = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchNode_current_profit_2 = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchNode_depth = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchNode_depth_2 = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchNode_next_item_id = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchNode_next_item_id_2 = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchNode_parent = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchNode_parent_2 = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchNode_profit_upper_bound = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchNode_profit_upper_bound_2 = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchNode_set_current_profit = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchNode_set_next_item_id = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchNode_set_profit_upper_bound = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchPath = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchPath_Init = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchPath_KnapsackSearchPath = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchPath_MoveUpToDepth = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchPath_from = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchPath_from_2 = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchPath_to = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchPath_to_2 = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchPath_via = R"doc()doc";

static const char *__doc_operations_research_KnapsackSearchPath_via_2 = R"doc()doc";

static const char *__doc_operations_research_KnapsackSolver =
R"doc(This library solves knapsack problems.

Problems the library solves include: - 0-1 knapsack problems, - Multi-
dimensional knapsack problems,

Given n items, each with a profit and a weight, given a knapsack of
capacity c, the goal is to find a subset of items which fits inside c
and maximizes the total profit. The knapsack problem can easily be
extended from 1 to d dimensions. As an example, this can be useful to
constrain the maximum number of items inside the knapsack. Without
loss of generality, profits and weights are assumed to be positive.

From a mathematical point of view, the multi-dimensional knapsack
problem can be modeled by d linear constraints:

ForEach(j:1..d)(Sum(i:1..n)(weight_ij * item_i) <= c_j where item_i is
a 0-1 integer variable.

Then the goal is to maximize:

Sum(i:1..n)(profit_i * item_i).

There are several ways to solve knapsack problems. One of the most
efficient is based on dynamic programming (mainly when weights,
profits and dimensions are small, and the algorithm runs in pseudo
polynomial time). Unfortunately, when adding conflict constraints the
problem becomes strongly NP-hard, i.e. there is no pseudo-polynomial
algorithm to solve it. That's the reason why the most of the following
code is based on branch and bound search.

For instance to solve a 2-dimensional knapsack problem with 9 items,
one just has to feed a profit vector with the 9 profits, a vector of 2
vectors for weights, and a vector of capacities. E.g.:

**Python:**

```
{.py}
profits = [ 1, 2, 3, 4, 5, 6, 7, 8, 9 ]
weights = [ [ 1, 2, 3, 4, 5, 6, 7, 8, 9 ],
[ 1, 1, 1, 1, 1, 1, 1, 1, 1 ]
]
capacities = [ 34, 4 ]

solver = pywrapknapsack_solver.KnapsackSolver(
pywrapknapsack_solver.KnapsackSolver
.KNAPSACK_MULTIDIMENSION_BRANCH_AND_BOUND_SOLVER,
'Multi-dimensional solver')
solver.Init(profits, weights, capacities)
profit = solver.Solve()
```

**C++:**

```
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

**Java:**

```
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

static const char *__doc_operations_research_KnapsackSolver_BestSolutionContains = R"doc(Returns true if the item 'item_id' is packed in the optimal knapsack.)doc";

static const char *__doc_operations_research_KnapsackSolver_ComputeAdditionalProfit = R"doc()doc";

static const char *__doc_operations_research_KnapsackSolver_GetName = R"doc()doc";

static const char *__doc_operations_research_KnapsackSolver_Init = R"doc(Initializes the solver and enters the problem to be solved.)doc";

static const char *__doc_operations_research_KnapsackSolver_InitReducedProblem = R"doc()doc";

static const char *__doc_operations_research_KnapsackSolver_IsSolutionOptimal = R"doc(Returns true if the solution was proven optimal.)doc";

static const char *__doc_operations_research_KnapsackSolver_KnapsackSolver = R"doc()doc";

static const char *__doc_operations_research_KnapsackSolver_KnapsackSolver_2 = R"doc()doc";

static const char *__doc_operations_research_KnapsackSolver_ReduceCapacities = R"doc()doc";

static const char *__doc_operations_research_KnapsackSolver_ReduceProblem = R"doc()doc";

static const char *__doc_operations_research_KnapsackSolver_Solve = R"doc(Solves the problem and returns the profit of the optimal solution.)doc";

static const char *__doc_operations_research_KnapsackSolver_SolverType =
R"doc(Enum controlling which underlying algorithm is used.

This enum is passed to the constructor of the KnapsackSolver object.
It selects which solving method will be used.)doc";

static const char *__doc_operations_research_KnapsackSolver_SolverType_KNAPSACK_64ITEMS_SOLVER =
R"doc(Optimized method for single dimension small problems

Limited to 64 items and one dimension, this solver uses a branch &
bound algorithm. This solver is about 4 times faster than
KNAPSACK_MULTIDIMENSION_BRANCH_AND_BOUND_SOLVER.)doc";

static const char *__doc_operations_research_KnapsackSolver_SolverType_KNAPSACK_BRUTE_FORCE_SOLVER =
R"doc(Brute force method.

Limited to 30 items and one dimension, this solver uses a brute force
algorithm, ie. explores all possible states. Experiments show
competitive performance for instances with less than 15 items.)doc";

static const char *__doc_operations_research_KnapsackSolver_SolverType_KNAPSACK_DIVIDE_AND_CONQUER_SOLVER =
R"doc(Divide and Conquer approach for single dimension problems

Limited to one dimension, this solver is based on a divide and conquer
technique and is suitable for larger problems than Dynamic Programming
Solver. The time complexity is O(capacity * number_of_items) and the
space complexity is O(capacity + number_of_items).)doc";

static const char *__doc_operations_research_KnapsackSolver_SolverType_KNAPSACK_DYNAMIC_PROGRAMMING_SOLVER =
R"doc(Dynamic Programming approach for single dimension problems

Limited to one dimension, this solver is based on a dynamic
programming algorithm. The time and space complexity is O(capacity *
number_of_items).)doc";

static const char *__doc_operations_research_KnapsackSolver_SolverType_KNAPSACK_MULTIDIMENSION_BRANCH_AND_BOUND_SOLVER =
R"doc(Generic Solver.

This solver can deal with both large number of items and several
dimensions. This solver is based on branch and bound.)doc";

static const char *__doc_operations_research_KnapsackSolver_additional_profit = R"doc()doc";

static const char *__doc_operations_research_KnapsackSolver_best_solution = R"doc()doc";

static const char *__doc_operations_research_KnapsackSolver_is_problem_solved = R"doc()doc";

static const char *__doc_operations_research_KnapsackSolver_is_solution_optimal = R"doc()doc";

static const char *__doc_operations_research_KnapsackSolver_known_value = R"doc()doc";

static const char *__doc_operations_research_KnapsackSolver_mapping_reduced_item_id = R"doc()doc";

static const char *__doc_operations_research_KnapsackSolver_set_time_limit =
R"doc(Time limit in seconds.

When a finite time limit is set the solution obtained might not be
optimal if the limit is reached.)doc";

static const char *__doc_operations_research_KnapsackSolver_set_use_reduction = R"doc()doc";

static const char *__doc_operations_research_KnapsackSolver_solver = R"doc()doc";

static const char *__doc_operations_research_KnapsackSolver_time_limit = R"doc()doc";

static const char *__doc_operations_research_KnapsackSolver_time_limit_seconds = R"doc()doc";

static const char *__doc_operations_research_KnapsackSolver_use_reduction = R"doc()doc";

static const char *__doc_operations_research_KnapsackSolver_use_reduction_2 = R"doc()doc";

static const char *__doc_operations_research_KnapsackState = R"doc()doc";

static const char *__doc_operations_research_KnapsackState_GetNumberOfItems = R"doc()doc";

static const char *__doc_operations_research_KnapsackState_Init = R"doc()doc";

static const char *__doc_operations_research_KnapsackState_KnapsackState = R"doc()doc";

static const char *__doc_operations_research_KnapsackState_UpdateState = R"doc()doc";

static const char *__doc_operations_research_KnapsackState_is_bound = R"doc()doc";

static const char *__doc_operations_research_KnapsackState_is_bound_2 = R"doc()doc";

static const char *__doc_operations_research_KnapsackState_is_in = R"doc()doc";

static const char *__doc_operations_research_KnapsackState_is_in_2 = R"doc()doc";

#if defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

