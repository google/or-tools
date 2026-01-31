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

/*
  This file contains docstrings for use in the Python bindings.
  Do not edit! They were automatically extracted by pybind11_mkdoc.
 */
#ifndef ORTOOLS_ROUTING_PYTHON_DOC_H_
#define ORTOOLS_ROUTING_PYTHON_DOC_H_

#define __EXPAND(x) x
#define __COUNT(_1, _2, _3, _4, _5, _6, _7, COUNT, ...) COUNT
#define __VA_SIZE(...) __EXPAND(__COUNT(__VA_ARGS__, 7, 6, 5, 4, 3, 2, 1))
#define __CAT1(a, b) a##b
#define __CAT2(a, b) __CAT1(a, b)
#define __DOC1(n1) __doc_##n1
#define __DOC2(n1, n2) __doc_##n1##_##n2
#define __DOC3(n1, n2, n3) __doc_##n1##_##n2##_##n3
#define __DOC4(n1, n2, n3, n4) __doc_##n1##_##n2##_##n3##_##n4
#define __DOC5(n1, n2, n3, n4, n5) __doc_##n1##_##n2##_##n3##_##n4##_##n5
#define __DOC6(n1, n2, n3, n4, n5, n6) \
  __doc_##n1##_##n2##_##n3##_##n4##_##n5##_##n6
#define __DOC7(n1, n2, n3, n4, n5, n6, n7) \
  __doc_##n1##_##n2##_##n3##_##n4##_##n5##_##n6##_##n7
#define DOC(...) \
  __EXPAND(__EXPAND(__CAT2(__DOC, __VA_SIZE(__VA_ARGS__)))(__VA_ARGS__))

#if defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

static const char* __doc_operations_research_routing_AppendTasksFromIntervals =
    R"doc()doc";

static const char* __doc_operations_research_routing_BoundCost =
    R"doc(A structure meant to store soft bounds and associated violation
constants. It is 'Simple' because it has one BoundCost per element, in
contrast to 'Multiple'. Design notes: - it is meant to store model
information to be shared through pointers, so it disallows copy and
assign to avoid accidental duplication. - it keeps soft bounds as an
array of structs to help cache, because code that uses such bounds
typically use both bound and cost. - soft bounds are named pairs,
prevents some mistakes. - using operator[] to access elements is not
interesting, because the structure will be accessed through pointers,
moreover having to type bound_cost reminds the user of the order if
they do a copy assignment of the element.)doc";

static const char* __doc_operations_research_routing_BoundCost_BoundCost =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_BoundCost_BoundCost_2 =
    R"doc()doc";

static const char* __doc_operations_research_routing_BoundCost_bound =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_BoundCost_cost =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_DisjunctivePropagator =
    R"doc(This class acts like a CP propagator: it takes a set of tasks given by
their start/duration/end features, and reduces the range of possible
values.)doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_ChainSpanMin =
        R"doc(Propagates a lower bound of the chain span, end[num_chain_tasks] -
start[0], to span_min.)doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_ChainSpanMinDynamic =  // NOLINT
    R"doc(Computes a lower bound of the span of the chain, taking into 
account only the first nonchain task. For more accurate results, this should
be called after Precedences(), otherwise the lower bound might be
lower than feasible.)doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_DetectablePrecedencesWithChain =  // NOLINT
    R"doc(Does detectable precedences deductions on tasks in the chain
precedence, taking the time windows of nonchain tasks into account.)doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_DistanceDuration =
        R"doc(Propagates distance_duration constraints, if any.)doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_EdgeFinding =  // NOLINT
    R"doc(Does edge-finding deductions on all tasks.)doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_ForbiddenIntervals =
        R"doc(Tasks might have holes in their domain, this enforces such 
holes.)doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_MirrorTasks =  // NOLINT
    R"doc(Transforms the problem with a time symmetry centered in 0. Returns
true for convenience.)doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_Precedences =  // NOLINT
    R"doc(Propagates the deductions from the chain of precedences, if there is
one.)doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_Propagate =  // NOLINT
    R"doc(Computes new bounds for all tasks, returns false if infeasible. This
does not compute a fixed point, so recalling it may filter more.)doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_Tasks =  // NOLINT
    R"doc(A structure to hold tasks described by their features. The first
num_chain_tasks are considered linked by a chain of precedences, i.e.
if i < j < num_chain_tasks, then end(i) <= start(j). This occurs
frequently in routing, and can be leveraged by some variants of
classic propagators.)doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_Tasks_Clear =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_Tasks_distance_duration =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_Tasks_duration_max =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_Tasks_duration_min =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_Tasks_end_max =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_Tasks_end_min =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_Tasks_forbidden_intervals =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_Tasks_is_preemptible =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_Tasks_num_chain_tasks =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_Tasks_span_max =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_Tasks_span_min =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_Tasks_start_max =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_Tasks_start_min =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_event_of_task =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_nonchain_tasks_by_start_max =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_tasks_by_end_max =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_tasks_by_start_min =
        R"doc(Mappings between events and tasks.)doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_theta_lambda_tree =
        R"doc(The main algorithm uses Vilim's theta tree data structure. See 
Petr Vilim's PhD thesis "Global Constraints in Scheduling".)doc";

static const char*
    __doc_operations_research_routing_DisjunctivePropagator_total_duration_before =  // NOLINT
    R"doc(Maps chain elements to the sum of chain task durations before 
them.)doc";

static const char* __doc_operations_research_routing_FillPathEvaluation =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_FillTravelBoundsOfVehicle =
    R"doc()doc";

static const char* __doc_operations_research_routing_FinalizerVariables =
    R"doc()doc";  // NOLINT

static const char*
    __doc_operations_research_routing_GlobalDimensionCumulOptimizer =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint =  // NOLINT
    R"doc(GlobalVehicleBreaksConstraint ensures breaks constraints are enforced
on all vehicles in the dimension passed to its constructor. It is
intended to be used for dimensions representing time. A break
constraint ensures break intervals fit on the route of a vehicle. For
a given vehicle, it forces break intervals to be disjoint from visit
intervals, where visit intervals start at CumulVar(node) and last for
node_visit_transit[node]. Moreover, it ensures that there is enough
time between two consecutive nodes of a route to do transit and
vehicle breaks, i.e. if Next(nodeA) = nodeB, CumulVar(nodeA) = tA and
CumulVar(nodeB) = tB, then SlackVar(nodeA) >= sum_{breaks \subseteq
[tA, tB)} duration(break).)doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_DebugString =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_FillPartialPathOfVehicle =  // NOLINT
    R"doc(Sets path_ to be the longest sequence such that _ path_[0] is the
start of the vehicle _ Next(path_[i-1]) is Bound() and has value
path_[i], followed by the end of the vehicle if the last node was not
an end.)doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_FillPathTravels =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_GlobalVehicleBreaksConstraint =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_InitialPropagate =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_Post =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_PropagateNode =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_PropagateVehicle =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_TaskTranslator =  // NOLINT
    R"doc(This translates pruning information to solver variables. If
constructed with an IntervalVar*, it follows the usual semantics of
IntervalVars. If constructed with an IntVar*, before_start and
after_start, operations are translated to simulate an interval that
starts at start - before_start and ends and start + after_start. If
constructed with nothing, the TaskTranslator will do nothing. This
class should have been an interface + subclasses, but that would force
pointers in the user's task vector, which means dynamic allocation.
With this union-like structure, a vector's reserved size will adjust
to usage and eventually no more dynamic allocation will be made.)doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_TaskTranslator_SetDurationMin =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_TaskTranslator_SetEndMax =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_TaskTranslator_SetEndMin =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_TaskTranslator_SetStartMax =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_TaskTranslator_SetStartMin =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_TaskTranslator_TaskTranslator =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_TaskTranslator_TaskTranslator_2 =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_TaskTranslator_TaskTranslator_3 =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_TaskTranslator_after_start =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_TaskTranslator_before_start =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_TaskTranslator_interval =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_TaskTranslator_start =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_dimension =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_disjunctive_propagator =  // NOLINT
    R"doc(This is used to restrict bounds of tasks.)doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_model =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_path =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_task_translators =  // NOLINT
    R"doc(Route and interval variables are normalized to the following 
values.)doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_tasks =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_travel_bounds =  // NOLINT
    R"doc(Used to help filling tasks_ at each propagation.)doc";

static const char*
    __doc_operations_research_routing_GlobalVehicleBreaksConstraint_vehicle_demons =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_IndexNeighborFinder =
    R"doc(Class to find index neighbors. Used by various parts of the vehicle
routing framework (heuristics, local search) which rely on finding
index neighbors (essentially to speed up search). It relies on having
coordinates.)doc";

static const char* __doc_operations_research_routing_IndexNeighborFinder_2 =
    R"doc(Class to find index neighbors. Used by various parts of the vehicle
routing framework (heuristics, local search) which rely on finding
index neighbors (essentially to speed up search). It relies on having
coordinates.)doc";

static const char*
    __doc_operations_research_routing_IndexNeighborFinder_FindIndexNeighbors =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_IndexNeighborFinder_IndexNeighborFinder =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_IndexNeighborFinder_IndexNeighborFinder_2 =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_IndexNeighborFinder_IndexNeighborFinder_3 =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_IndexNeighborFinder_manager =  // NOLINT
    R"doc(When the IndexNeighborFinder is created using NodeIndex as index, the
manager_ is used to translate from variable index to NodeIndex and
back when calling FindIndexNeighbors().)doc";

static const char*
    __doc_operations_research_routing_IndexNeighborFinder_operator_assign =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_IndexNeighborFinder_points =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_IntVarFilteredDecisionBuilder =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_LocalDimensionCumulOptimizer =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_LocalSearchPhaseParameters =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_MakeBinCapacities =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_MakeVehicleBreaksFilter =
    R"doc()doc";

static const char* __doc_operations_research_routing_PathsMetadata =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_PathsMetadata_End =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_PathsMetadata_Ends =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_PathsMetadata_GetPath =
    R"doc()doc";

static const char* __doc_operations_research_routing_PathsMetadata_IsEnd =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_PathsMetadata_IsStart =
    R"doc()doc";

static const char* __doc_operations_research_routing_PathsMetadata_NumPaths =
    R"doc()doc";

static const char* __doc_operations_research_routing_PathsMetadata_Paths =
    R"doc()doc";  // NOLINT

static const char*
    __doc_operations_research_routing_PathsMetadata_PathsMetadata =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_PathsMetadata_Start =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_PathsMetadata_Starts =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_PathsMetadata_end_of_path =
    R"doc()doc";

static const char* __doc_operations_research_routing_PathsMetadata_is_end =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_PathsMetadata_is_start =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_PathsMetadata_path_of_node =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_PathsMetadata_start_of_path =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Dimension =
    R"doc(for a given vehicle, it is passed as an external vector, it would be
better to have this information here.)doc";

static const char* __doc_operations_research_routing_Dimension_2 =
    R"doc(for a given vehicle, it is passed as an external vector, it would be
better to have this information here.)doc";

static const char*
    __doc_operations_research_routing_Dimension_AddNodePrecedence =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_AddNodePrecedence_2 =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_AllTransitEvaluatorSignsAreUnknown =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_AreVehicleTransitsPositive =
        R"doc(Returns true iff the transit evaluator of 'vehicle' is positive 
for all arcs.)doc";

static const char* __doc_operations_research_routing_Dimension_CloseModel =
    R"doc(Finalize the model of the dimension.)doc";

static const char* __doc_operations_research_routing_Dimension_CumulVar =
    R"doc(Get the cumul, transit and slack variables for the given node (given
as int64_t var index).)doc";

static const char* __doc_operations_research_routing_Dimension_FixedTransitVar =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_GetAllowedIntervalsInRange =
        R"doc(Returns allowed intervals for a given node in a given 
interval.)doc";

static const char*
    __doc_operations_research_routing_Dimension_GetBinaryTransitEvaluator =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_GetBreakDistanceDurationOfVehicle =  // NOLINT
    R"doc(Returns the pairs (distance, duration) specified by break distance
constraints.)doc";

static const char*
    __doc_operations_research_routing_Dimension_GetBreakIntervalsOfVehicle =
        R"doc(Returns the break intervals set by 
SetBreakIntervalsOfVehicle().)doc";

static const char* __doc_operations_research_routing_Dimension_GetCumulVarMax =
    R"doc(Gets the current maximum of the cumul variable associated to 
index.)doc";

static const char* __doc_operations_research_routing_Dimension_GetCumulVarMin =
    R"doc(Gets the current minimum of the cumul variable associated to 
index.)doc";

static const char*
    __doc_operations_research_routing_Dimension_GetCumulVarPiecewiseLinearCost =
        R"doc(Returns the piecewise linear cost of a cumul variable for a given
variable index. The returned pointer has the same validity as this
class.)doc";

static const char*
    __doc_operations_research_routing_Dimension_GetCumulVarSoftLowerBound =
        R"doc(Returns the soft lower bound of a cumul variable for a given 
variable index. The "hard" lower bound of the variable is returned if no soft
lower bound has been set.)doc";

static const char*
    __doc_operations_research_routing_Dimension_GetCumulVarSoftLowerBoundCoefficient =  // NOLINT
    R"doc(Returns the cost coefficient of the soft lower bound of a cumul
variable for a given variable index. If no soft lower bound has been
set, 0 is returned.)doc";

static const char*
    __doc_operations_research_routing_Dimension_GetCumulVarSoftUpperBound =  // NOLINT
    R"doc(Returns the soft upper bound of a cumul variable for a given variable
index. The "hard" upper bound of the variable is returned if no soft
upper bound has been set.)doc";

static const char*
    __doc_operations_research_routing_Dimension_GetCumulVarSoftUpperBoundCoefficient =  // NOLINT
    R"doc(Returns the cost coefficient of the soft upper bound of a cumul
variable for a given variable index. If no soft upper bound has been
set, 0 is returned.)doc";

static const char*
    __doc_operations_research_routing_Dimension_GetFirstPossibleGreaterOrEqualValueForNode =  // NOLINT
    R"doc(Returns the smallest value outside the forbidden intervals of node
'index' that is greater than or equal to a given 'min_value'.)doc";

static const char*
    __doc_operations_research_routing_Dimension_GetGlobalOptimizerOffset =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_GetLastPossibleLessOrEqualValueForNode =  // NOLINT
    R"doc(Returns the largest value outside the forbidden intervals of node
'index' that is less than or equal to a given 'max_value'. NOTE: If
this method is called with a max_value lower than the node's cumul
min, it will return -1.)doc";

static const char*
    __doc_operations_research_routing_Dimension_GetLocalOptimizerOffsetForVehicle =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_GetNodePrecedences =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_GetPathPrecedenceGraph =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_GetPickupToDeliveryLimitForPair =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_GetPostTravelEvaluatorOfVehicle =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_GetPreTravelEvaluatorOfVehicle =
        R"doc(!defined(SWIGPYTHON))doc";

static const char*
    __doc_operations_research_routing_Dimension_GetQuadraticCostSoftSpanUpperBoundForVehicle =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_GetSlackCostCoefficientForVehicle =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_GetSlackCostCoefficientForVehicleClass =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_GetSoftSpanUpperBoundForVehicle =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_GetSpanCostCoefficientForVehicle =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_GetSpanCostCoefficientForVehicleClass =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_GetSpanUpperBoundForVehicle =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_GetTransitEvaluatorSign =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Dimension_GetTransitValue =
    R"doc(Returns the transition value for a given pair of nodes (as var index);
this value is the one taken by the corresponding transit variable when
the 'next' variable for 'from_index' is bound to 'to_index'.)doc";

static const char*
    __doc_operations_research_routing_Dimension_GetTransitValueFromClass =
        R"doc(Same as above but taking a vehicle class of the dimension instead 
of a vehicle (the class of a vehicle can be obtained with
vehicle_to_class()).)doc";

static const char*
    __doc_operations_research_routing_Dimension_GetUnaryTransitEvaluator =
        R"doc(Returns the unary callback evaluating the transit value between 
two node indices for a given vehicle. If the corresponding callback is not
unary, returns a null callback.)doc";

static const char*
    __doc_operations_research_routing_Dimension_HasBreakConstraints =  // NOLINT
    R"doc(Returns true if any break interval or break distance was 
defined.)doc";

static const char*
    __doc_operations_research_routing_Dimension_HasCumulVarPiecewiseLinearCost =
        R"doc(Returns true if a piecewise linear cost has been set for a given
variable index.)doc";

static const char*
    __doc_operations_research_routing_Dimension_HasCumulVarSoftLowerBound =
        R"doc(Returns true if a soft lower bound has been set for a given 
variable index.)doc";

static const char*
    __doc_operations_research_routing_Dimension_HasCumulVarSoftUpperBound =
        R"doc(Returns true if a soft upper bound has been set for a given 
variable index.)doc";

static const char*
    __doc_operations_research_routing_Dimension_HasPickupToDeliveryLimits =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_HasQuadraticCostSoftSpanUpperBounds =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_HasSoftSpanUpperBounds =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Dimension_Initialize =
    R"doc()doc";  // NOLINT

static const char*
    __doc_operations_research_routing_Dimension_InitializeBreaks =  // NOLINT
    R"doc(Sets up vehicle_break_intervals_, vehicle_break_distance_duration_,
pre_travel_evaluators and post_travel_evaluators.)doc";

static const char*
    __doc_operations_research_routing_Dimension_InitializeCumuls =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_InitializeTransitVariables =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_InitializeTransits =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Dimension_IsUnary =
    R"doc(Returns true iff all transit evaluators for this dimension are 
unary.)doc";

static const char* __doc_operations_research_routing_Dimension_NodePrecedence =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_NodePrecedence_first_node =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_NodePrecedence_offset =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_NodePrecedence_second_node =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_PiecewiseLinearCost =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_PiecewiseLinearCost_PiecewiseLinearCost =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_PiecewiseLinearCost_cost =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_PiecewiseLinearCost_var =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Dimension_Dimension =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Dimension_Dimension_2 =
    R"doc()doc";

static const char* __doc_operations_research_routing_Dimension_Dimension_3 =
    R"doc()doc";

static const char* __doc_operations_research_routing_Dimension_SelfBased =
    R"doc()doc";  // NOLINT

static const char*
    __doc_operations_research_routing_Dimension_SetBreakDistanceDurationOfVehicle =  // NOLINT
    R"doc(With breaks supposed to be consecutive, this forces the distance
between breaks of size at least minimum_break_duration to be at most
distance. This supposes that the time until route start and after
route end are infinite breaks.)doc";

static const char*
    __doc_operations_research_routing_Dimension_SetBreakIntervalsOfVehicle =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_SetBreakIntervalsOfVehicle_2 =
        R"doc(Deprecated, sets pre_travel(i, j) = node_visit_transit[i].)doc";

static const char*
    __doc_operations_research_routing_Dimension_SetBreakIntervalsOfVehicle_3 =
        R"doc(Deprecated, sets pre_travel(i, j) = node_visit_transit[i] and
post_travel(i, j) = delays(i, j).)doc";

static const char*
    __doc_operations_research_routing_Dimension_SetCumulVarPiecewiseLinearCost =
        R"doc(Sets a piecewise linear cost on the cumul variable of a given 
variable index. If f is a piecewise linear function, the resulting cost at
'index' will be f(CumulVar(index)). As of 3/2017, only non-decreasing
positive cost functions are supported.)doc";

static const char*
    __doc_operations_research_routing_Dimension_SetCumulVarRange =  // NOLINT
    R"doc(Restricts the range of the cumul variable associated to index.)doc";

static const char*
    __doc_operations_research_routing_Dimension_SetCumulVarSoftLowerBound =
        R"doc(Sets a soft lower bound to the cumul variable of a given variable
index. If the value of the cumul variable is less than the bound, a
cost proportional to the difference between this value and the bound
is added to the cost function of the model: cumulVar > lower_bound ->
cost = 0 cumulVar <= lower_bound -> cost = coefficient * (lower_bound
- cumulVar). This is also handy to model earliness costs when the
dimension represents time.)doc";

static const char*
    __doc_operations_research_routing_Dimension_SetCumulVarSoftUpperBound =
        R"doc(Sets a soft upper bound to the cumul variable of a given variable
index. If the value of the cumul variable is greater than the bound, a
cost proportional to the difference between this value and the bound
is added to the cost function of the model: cumulVar <= upper_bound ->
cost = 0 cumulVar > upper_bound -> cost = coefficient * (cumulVar -
upper_bound) This is also handy to model tardiness costs when the
dimension represents time.)doc";

static const char*
    __doc_operations_research_routing_Dimension_SetGlobalSpanCostCoefficient =
        R"doc(Sets a cost proportional to the *global* dimension span, that is 
the difference between the largest value of route end cumul variables and
the smallest value of route start cumul variables. In other words:
global_span_cost = coefficient * (Max(dimension end value) -
Min(dimension start value)).)doc";

static const char*
    __doc_operations_research_routing_Dimension_SetOffsetForGlobalOptimizer =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_SetPickupToDeliveryLimitFunctionForPair =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_SetQuadraticCostSoftSpanUpperBoundForVehicle =  // NOLINT
    R"doc(If the span of vehicle on this dimension is larger than bound, the
cost will be increased by cost * (span - bound)^2.)doc";

static const char*
    __doc_operations_research_routing_Dimension_SetSlackCostCoefficientForAllVehicles =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_SetSlackCostCoefficientForVehicle =  // NOLINT
    R"doc(Sets a cost proportional to the dimension total slack on a given
vehicle, or on all vehicles at once. "coefficient" must be
nonnegative. This is handy to model costs only proportional to idle
time when the dimension represents time. The cost for a vehicle is
slack_cost = coefficient * (dimension end value - dimension start
value - total_transit).)doc";

static const char*
    __doc_operations_research_routing_Dimension_SetSoftSpanUpperBoundForVehicle =  // NOLINT
    R"doc(If the span of vehicle on this dimension is larger than bound, the
cost will be increased by cost * (span - bound).)doc";

static const char*
    __doc_operations_research_routing_Dimension_SetSpanCostCoefficientForAllVehicles =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_SetSpanCostCoefficientForVehicle =  // NOLINT
    R"doc(Sets a cost proportional to the dimension span on a given vehicle, or
on all vehicles at once. "coefficient" must be nonnegative. This is
handy to model costs proportional to idle time when the dimension
represents time. The cost for a vehicle is span_cost = coefficient *
(dimension end value - dimension start value).)doc";

static const char*
    __doc_operations_research_routing_Dimension_SetSpanUpperBoundForVehicle =
        R"doc(!defined(SWIGPYTHON) Sets an upper bound on the dimension span on 
a given vehicle. This is the preferred way to limit the "length" of the
route of a vehicle according to a dimension.)doc";

static const char*
    __doc_operations_research_routing_Dimension_SetVehicleOffsetsForLocalOptimizer =  // NOLINT
    R"doc(Moves elements of "offsets" into 
vehicle_offsets_for_local_optimizer_.)doc";

static const char*
    __doc_operations_research_routing_Dimension_SetupCumulVarPiecewiseLinearCosts =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_SetupCumulVarSoftLowerBoundCosts =  // NOLINT
    R"doc(Sets up the cost variables related to cumul soft lower 
bounds.)doc";

static const char*
    __doc_operations_research_routing_Dimension_SetupCumulVarSoftUpperBoundCosts =  // NOLINT
    R"doc(Sets up the cost variables related to cumul soft upper bounds.)doc";

static const char*
    __doc_operations_research_routing_Dimension_SetupGlobalSpanCost =  // NOLINT
    R"doc(Sets up the cost variables related to the global span and 
per-vehicle span costs (only for the "slack" part of the latter).)doc";

static const char*
    __doc_operations_research_routing_Dimension_SetupSlackAndDependentTransitCosts =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_ShortestTransitionSlack =  // NOLINT
    R"doc(It makes sense to use the function only for self-dependent 
dimension. For such dimensions the value of the slack of a node determines the
transition cost of the next transit. Provided that 1. cumul[node] is
fixed, 2. next[node] and next[next[node]] (if exists) are fixed, the
value of slack[node] for which cumul[next[node]] + transit[next[node]]
is minimized can be found in O(1) using this function.)doc";

static const char* __doc_operations_research_routing_Dimension_SlackVar =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Dimension_SoftBound =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Dimension_SoftBound_bound =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_SoftBound_coefficient =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Dimension_SoftBound_var =
    R"doc()doc";

static const char* __doc_operations_research_routing_Dimension_TransitVar =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Dimension_base_dimension =
    R"doc(Returns the parent in the dependency tree if any or nullptr 
otherwise.)doc";

static const char*
    __doc_operations_research_routing_Dimension_base_dimension_2 =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_break_constraints_are_initialized =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Dimension_capacity_vars =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_class_evaluators =  // NOLINT
    R"doc(Values in class_evaluators_ correspond to the evaluators in
Model::transit_evaluators_ for each vehicle class.)doc";

static const char*
    __doc_operations_research_routing_Dimension_class_transit_evaluator =  // NOLINT
    R"doc(Returns the callback evaluating the transit value between two node
indices for a given vehicle class.)doc";

static const char*
    __doc_operations_research_routing_Dimension_cumul_var_piecewise_linear_cost =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_cumul_var_soft_lower_bound =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_cumul_var_soft_upper_bound =
        R"doc()doc";

static const char* __doc_operations_research_routing_Dimension_cumuls =
    R"doc(Like CumulVar(), TransitVar(), SlackVar() but return the whole
variable vectors instead (indexed by int64_t var index).)doc";

static const char* __doc_operations_research_routing_Dimension_cumuls_2 =
    R"doc()doc";  // NOLINT

static const char*
    __doc_operations_research_routing_Dimension_dependent_transits =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Dimension_fixed_transits =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_fixed_transits_2 =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_forbidden_intervals =  // NOLINT
    R"doc(Returns forbidden intervals for each node.)doc";

static const char*
    __doc_operations_research_routing_Dimension_forbidden_intervals_2 =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_global_optimizer_offset =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_global_span_cost_coefficient =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_global_span_cost_coefficient_2 =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_local_optimizer_offset_for_vehicle =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Dimension_model =
    R"doc(Returns the model on which the dimension was created.)doc";

static const char* __doc_operations_research_routing_Dimension_model_2 =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Dimension_name =
    R"doc(Returns the name of the dimension.)doc";

static const char* __doc_operations_research_routing_Dimension_name_2 =
    R"doc()doc";  // NOLINT

static const char*
    __doc_operations_research_routing_Dimension_node_precedences =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Dimension_operator_assign =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_path_precedence_graph =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_pickup_to_delivery_limits_per_pair_index =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Dimension_slacks =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Dimension_slacks_2 =
    R"doc()doc";  // NOLINT

static const char*
    __doc_operations_research_routing_Dimension_state_dependent_class_evaluators =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_state_dependent_vehicle_to_class =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_transit_evaluator =  // NOLINT
    R"doc(Returns the callback evaluating the transit value between two node
indices for a given vehicle.)doc";

static const char* __doc_operations_research_routing_Dimension_transits =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Dimension_transits_2 =
    R"doc()doc";  // NOLINT

static const char*
    __doc_operations_research_routing_Dimension_vehicle_break_distance_duration =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_vehicle_break_intervals =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_vehicle_capacities =  // NOLINT
    R"doc(Returns the capacities for all vehicles.)doc";

static const char*
    __doc_operations_research_routing_Dimension_vehicle_capacities_2 =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_vehicle_post_travel_evaluators =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_vehicle_pre_travel_evaluators =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_vehicle_quadratic_cost_soft_span_upper_bound =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_vehicle_slack_cost_coefficients =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_vehicle_slack_cost_coefficients_2 =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_vehicle_soft_span_upper_bound =
        R"doc(nullptr if not defined.)doc";

static const char*
    __doc_operations_research_routing_Dimension_vehicle_span_cost_coefficients =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_vehicle_span_cost_coefficients_2 =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_vehicle_span_upper_bounds =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_vehicle_span_upper_bounds_2 =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_vehicle_to_class =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Dimension_vehicle_to_class_2 =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model = R"doc()doc";

static const char* __doc_operations_research_routing_ModelVisitor =
    R"doc(Routing model visitor.)doc";

static const char* __doc_operations_research_routing_Model_ActiveVar =
    R"doc(Returns the active variable of the node corresponding to index.)doc";

static const char* __doc_operations_research_routing_Model_ActiveVehicleVar =
    R"doc(Returns the active variable of the vehicle. It will be equal to 1 iff
the route of the vehicle is not empty, 0 otherwise.)doc";

static const char*
    __doc_operations_research_routing_Model_AddAtSolutionCallback =  // NOLINT
    R"doc(Adds a callback called each time a solution is found during the
search. This is a shortcut to creating a monitor to call the callback
on AtSolution() and adding it with AddSearchMonitor. If
track_unchecked_neighbors is true, the callback will also be called on
AcceptUncheckedNeighbor() events, which is useful to grab solutions
obtained when solver_parameters.check_solution_period > 1 (aka
fastLS).)doc";

static const char*
    __doc_operations_research_routing_Model_AddConstantDimension =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_AddConstantDimensionWithSlack =
        R"doc(Creates a dimension where the transit variable is constrained to 
be equal to 'value'; 'capacity' is the upper bound of the cumul
variables. 'name' is the name used to reference the dimension; this
name is used to get cumul and transit variables from the routing
model. Returns a pair consisting of an index to the registered unary
transit callback and a bool denoting whether the dimension has been
created. It is false if a dimension with the same name has already
been created (and doesn't create the new dimension but still register
a new callback).)doc";

static const char* __doc_operations_research_routing_Model_AddDimension =
    R"doc(Creates a dimension where the transit variable is constrained to be
equal to evaluator(i, next(i)); 'slack_max' is the upper bound of the
slack variable and 'capacity' is the upper bound of the cumul
variables. 'name' is the name used to reference the dimension; this
name is used to get cumul and transit variables from the routing
model. Returns false if a dimension with the same name has already
been created (and doesn't create the new dimension). Takes ownership
of the callback 'evaluator'.)doc";

static const char*
    __doc_operations_research_routing_Model_AddDimensionDependentDimensionWithVehicleCapacity =  // NOLINT
    R"doc(Creates a dimension with transits depending on the cumuls of another
dimension. 'pure_transits' are the per-vehicle fixed transits as
above. 'dependent_transits' is a vector containing for each vehicle an
index to a registered state dependent transit callback.
'base_dimension' indicates the dimension from which the cumul variable
is taken. If 'base_dimension' is nullptr, then the newly created
dimension is self-based.)doc";

static const char*
    __doc_operations_research_routing_Model_AddDimensionDependentDimensionWithVehicleCapacity_2 =  // NOLINT
    R"doc(As above, but pure_transits are taken to be zero evaluators.)doc";

static const char*
    __doc_operations_research_routing_Model_AddDimensionDependentDimensionWithVehicleCapacity_3 =  // NOLINT
    R"doc(Homogeneous versions of the functions above.)doc";

static const char*
    __doc_operations_research_routing_Model_AddDimensionDependentDimensionWithVehicleCapacity_4 =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_AddDimensionDependentDimensionWithVehicleCapacityInternal =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_AddDimensionWithCapacityInternal =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_AddDimensionWithVehicleCapacity =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_AddDimensionWithVehicleTransitAndCapacity =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_AddDimensionWithVehicleTransits =
        R"doc()doc";

static const char* __doc_operations_research_routing_Model_AddDisjunction =
    R"doc(Adds a disjunction constraint on the indices: exactly
'max_cardinality' of the indices are active. Start and end indices of
any vehicle cannot be part of a disjunction.

If a penalty is given, at most 'max_cardinality' of the indices can be
active, and if less are active, 'penalty' is payed per inactive index.
This is equivalent to adding the constraint: p + Sum(i)active[i] ==
max_cardinality where p is an integer variable, and the following cost
to the cost function: p * penalty. 'penalty' must be positive to make
the disjunction optional; a negative penalty will force
'max_cardinality' indices of the disjunction to be performed, and
therefore p == 0. Note: passing a vector with a single index will
model an optional index with a penalty cost if it is not visited.)doc";

static const char*
    __doc_operations_research_routing_Model_AddHardTypeIncompatibility =  // NOLINT
    R"doc(Incompatibilities: Two nodes with "hard" incompatible types cannot
share the same route at all, while with a "temporal" incompatibility
they can't be on the same route at the same time.)doc";

static const char*
    __doc_operations_research_routing_Model_AddIntervalToAssignment =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_AddLocalSearchFilter =  // NOLINT
    R"doc(Adds a custom local search filter to the list of filters used to speed
up local search by pruning unfeasible variable assignments. Calling
this method after the routing model has been closed (CloseModel() or
Solve() has been called) has no effect. The routing model does not
take ownership of the filter.)doc";

static const char*
    __doc_operations_research_routing_Model_AddLocalSearchOperator =  // NOLINT
    R"doc(Adds a local search operator to the set of operators used to solve the
vehicle routing problem.)doc";

static const char* __doc_operations_research_routing_Model_AddMatrixDimension =
    R"doc(Creates a dimension where the transit variable is constrained to be
equal to 'values[i][next(i)]' for node i; 'capacity' is the upper
bound of the cumul variables. 'name' is the name used to reference the
dimension; this name is used to get cumul and transit variables from
the routing model. Returns a pair consisting of an index to the
registered transit callback and a bool denoting whether the dimension
has been created. It is false if a dimension with the same name has
already been created (and doesn't create the new dimension but still
register a new callback).)doc";

static const char*
    __doc_operations_research_routing_Model_AddNoCycleConstraintInternal =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_AddPickupAndDelivery =  // NOLINT
    R"doc(Notifies that index1 and index2 form a pair of nodes which should
belong to the same route. This methods helps the search find better
solutions, especially in the local search phase. It should be called
each time you have an equality constraint linking the vehicle
variables of two node (including for instance pickup and delivery
problems): Solver* const solver = routing.solver(); int64_t index1 =
manager.NodeToIndex(node1); int64_t index2 =
manager.NodeToIndex(node2);
solver->AddConstraint(solver->MakeEquality(
routing.VehicleVar(index1), routing.VehicleVar(index2)));
routing.AddPickupAndDelivery(index1, index2);)doc";

static const char*
    __doc_operations_research_routing_Model_AddPickupAndDeliverySets =  // NOLINT
    R"doc(Same as AddPickupAndDelivery but notifying that the performed node
from the disjunction of index 'pickup_disjunction' is on the same
route as the performed node from the disjunction of index
'delivery_disjunction'.)doc";

static const char*
    __doc_operations_research_routing_Model_AddPickupAndDeliverySetsInternal =
        R"doc(Sets up pickup and delivery sets.)doc";

static const char*
    __doc_operations_research_routing_Model_AddRequiredTypeAlternativesWhenAddingType =  // NOLINT
    R"doc(If type_D depends on type_R when adding type_D, any node_D of 
type_D and VisitTypePolicy TYPE_ADDED_TO_VEHICLE or
TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED requires at least one type_R on
its vehicle at the time node_D is visited.)doc";

static const char*
    __doc_operations_research_routing_Model_AddRequiredTypeAlternativesWhenRemovingType =  // NOLINT
    R"doc(The following requirements apply when visiting dependent nodes that
remove their type from the route, i.e. type_R must be on the vehicle
when type_D of VisitTypePolicy ADDED_TYPE_REMOVED_FROM_VEHICLE,
TYPE_ON_VEHICLE_UP_TO_VISIT or TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED
is visited.)doc";

static const char* __doc_operations_research_routing_Model_AddResourceGroup =
    R"doc(Adds a resource group to the routing model and returns a pointer to
it.)doc";

static const char*
    __doc_operations_research_routing_Model_AddSameVehicleRequiredTypeAlternatives =  // NOLINT
    R"doc(Requirements: NOTE: As of 2019-04, cycles in the requirement graph
 are not supported, and lead to the dependent nodes being skipped if
possible (otherwise the model is considered infeasible). The following
functions specify that "dependent_type" requires at least one of the
types in "required_type_alternatives".

For same-vehicle requirements, a node of dependent type type_D
requires at least one node of type type_R among the required
alternatives on the same route.)doc";

static const char* __doc_operations_research_routing_Model_AddSearchMonitor =
    R"doc(Adds a search monitor to the search used to solve the routing model.)doc";
static const char*
    __doc_operations_research_routing_Model_AddSoftSameVehicleConstraint =  // NOLINT
    R"doc(Adds a soft constraint to force a set of variable indices to be on the
same vehicle. If all nodes are not on the same vehicle, each extra
vehicle used adds 'cost' to the cost function.)doc";

static const char*
    __doc_operations_research_routing_Model_AddTemporalTypeIncompatibility =
        R"doc()doc";

static const char* __doc_operations_research_routing_Model_AddToAssignment =
    R"doc(Adds an extra variable to the vehicle routing assignment.)doc";

static const char*
    __doc_operations_research_routing_Model_AddVariableMaximizedByFinalizer =
        R"doc(Adds a variable to maximize in the solution finalizer (see above 
for information on the solution finalizer).)doc";

static const char*
    __doc_operations_research_routing_Model_AddVariableMinimizedByFinalizer =
        R"doc(Adds a variable to minimize in the solution finalizer. The 
solution finalizer is called each time a solution is found during the search
and allows to instantiate secondary variables (such as dimension cumul
variables).)doc";

static const char*
    __doc_operations_research_routing_Model_AddVariableTargetToFinalizer =  // NOLINT
    R"doc(Add a variable to set the closest possible to the target value in the
solution finalizer.)doc";

static const char* __doc_operations_research_routing_Model_AddVectorDimension =
    R"doc(Creates a dimension where the transit variable is constrained to be
equal to 'values[i]' for node i; 'capacity' is the upper bound of the
cumul variables. 'name' is the name used to reference the dimension;
this name is used to get cumul and transit variables from the routing
model. Returns a pair consisting of an index to the registered unary
transit callback and a bool denoting whether the dimension has been
created. It is false if a dimension with the same name has already
been created (and doesn't create the new dimension but still register
a new callback).)doc";

static const char*
    __doc_operations_research_routing_Model_AddWeightedVariableMaximizedByFinalizer =  // NOLINT
    R"doc(Adds a variable to maximize in the solution finalizer, with a 
weighted priority: the higher the more priority it has.)doc";

static const char*
    __doc_operations_research_routing_Model_AddWeightedVariableMinimizedByFinalizer =  // NOLINT
    R"doc(Adds a variable to minimize in the solution finalizer, with a 
weighted priority: the higher the more priority it has.)doc";

static const char*
    __doc_operations_research_routing_Model_AddWeightedVariableTargetToFinalizer =  // NOLINT
    R"doc(Same as above with a weighted priority: the higher the cost, the 
more priority it has to be set close to the target value.)doc";

static const char* __doc_operations_research_routing_Model_AppendArcCosts =
    R"doc()doc";  // NOLINT

static const char*
    __doc_operations_research_routing_Model_AppendAssignmentIfFeasible =  // NOLINT
    R"doc(Append an assignment to a vector of assignments if it is 
feasible.)doc";

static const char*
    __doc_operations_research_routing_Model_AppendHomogeneousArcCosts =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_ApplyLocks =
    R"doc(Applies a lock chain to the next search. 'locks' represents an ordered
vector of nodes representing a partial route which will be fixed
during the next search; it will constrain next variables such that:
next[locks[i]] == locks[i+1].

Returns the next variable at the end of the locked chain; this
variable is not locked. An assignment containing the locks can be
obtained by calling PreAssignment().)doc";

static const char*
    __doc_operations_research_routing_Model_ApplyLocksToAllVehicles =  // NOLINT
    R"doc(Applies lock chains to all vehicles to the next search, such that
locks[p] is the lock chain for route p. Returns false if the locks do
not contain valid routes; expects that the routes do not contain the
depots, i.e. there are empty vectors in place of empty routes. If
close_routes is set to true, adds the end nodes to the route of each
vehicle and deactivates other nodes. An assignment containing the
locks can be obtained by calling PreAssignment().)doc";

static const char*
    __doc_operations_research_routing_Model_ArcIsMoreConstrainedThanArc =  // NOLINT
    R"doc(Returns whether the arc from->to1 is more constrained than from->to2,
taking into account, in order: - whether the destination node isn't an
end node - whether the destination node is mandatory - whether the
destination node is bound to the same vehicle as the source - the
"primary constrained" dimension (see SetPrimaryConstrainedDimension)
It then breaks ties using, in order: - the arc cost (taking
unperformed penalties into account) - the size of the vehicle vars of
"to1" and "to2" (lowest size wins) - the value: the lowest value of
the indices to1 and to2 wins. See the .cc for details. The more
constrained arc is typically preferable when building a first
solution. This method is intended to be used as a callback for the
BestValueByComparisonSelector value selector. Args: from: the variable
index of the source node to1: the variable index of the first
candidate destination node. to2: the variable index of the second
candidate destination node.)doc";

static const char*
    __doc_operations_research_routing_Model_AreRoutesInterdependent =  // NOLINT
    R"doc(Returns true if routes are interdependent. This means that any
modification to a route might impact another.)doc";

static const char* __doc_operations_research_routing_Model_AssignmentToRoutes =
    R"doc(Converts the solution in the given assignment to routes for all
vehicles. Expects that assignment contains a valid solution (i.e.
routes for all vehicles end with an end index for that vehicle).)doc";

static const char* __doc_operations_research_routing_Model_CancelSearch =
    R"doc(Cancels the current search.)doc";

static const char*
    __doc_operations_research_routing_Model_CheckIfAssignmentIsFeasible =  // NOLINT
    R"doc(Checks if an assignment is feasible.)doc";

static const char* __doc_operations_research_routing_Model_CheckLimit =
    R"doc(Returns true if the search limit has been crossed with the given time
offset.)doc";

static const char* __doc_operations_research_routing_Model_CloseModel =
    R"doc(Closes the current routing model; after this method is called, no
modification to the model can be done, but RoutesToAssignment becomes
available. Note that CloseModel() is automatically called by Solve()
and other methods that produce solution. This is equivalent to calling
CloseModelWithParameters(DefaultRoutingSearchParameters()).)doc";

static const char*
    __doc_operations_research_routing_Model_CloseModelWithParameters =  // NOLINT
    R"doc(Same as above taking search parameters (as of 10/2015 some the
parameters have to be set when closing the model).)doc";

static const char*
    __doc_operations_research_routing_Model_CompactAndCheckAssignment =  // NOLINT
    R"doc(Same as CompactAssignment() but also checks the validity of the final
compact solution; if it is not valid, no attempts to repair it are
made (instead, the method returns nullptr).)doc";

static const char* __doc_operations_research_routing_Model_CompactAssignment =
    R"doc(Returns a compacted version of the given assignment, in which all
vehicles with id lower or equal to some N have non-empty routes, and
all vehicles with id greater than N have empty routes. Does not take
ownership of the returned object. If found, the cost of the compact
assignment is the same as in the original assignment and it preserves
the values of 'active' variables. Returns nullptr if a compact
assignment was not found. This method only works in homogenous mode,
and it only swaps equivalent vehicles (vehicles with the same start
and end nodes). When creating the compact assignment, the empty plan
is replaced by the route assigned to the compatible vehicle with the
highest id. Note that with more complex constraints on vehicle
variables, this method might fail even if a compact solution exists.
This method changes the vehicle and dimension variables as necessary.
While compacting the solution, only basic checks on vehicle variables
are performed; if one of these checks fails no attempts to repair it
are made (instead, the method returns nullptr).)doc";

static const char*
    __doc_operations_research_routing_Model_CompactAssignmentInternal =  // NOLINT
    R"doc(See CompactAssignment. Checks the final solution if
check_compact_assignment is true.)doc";

static const char* __doc_operations_research_routing_Model_ComputeCostClasses =
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_ComputeLowerBound =
    R"doc(Computes a lower bound to the routing problem solving a linear
assignment problem. The routing model must be closed before calling
this method. Note that problems with node disjunction constraints
(including optional nodes) and non-homogenous costs are not supported
(the method returns 0 in these cases).)doc";

static const char*
    __doc_operations_research_routing_Model_ComputeResourceClasses =  // NOLINT
    R"doc(Computes resource classes for all resource groups in the model.)doc";

static const char*
    __doc_operations_research_routing_Model_ComputeVehicleClasses =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_ComputeVehicleTypes =
    R"doc(The following method initializes the vehicle_type_container_: -
Computes the vehicle types of vehicles and stores it in
type_index_of_vehicle. - The vehicle classes corresponding to each
vehicle type index are stored and sorted by fixed cost in
sorted_vehicle_classes_per_type. - The vehicles for each vehicle class
are stored in vehicles_per_vehicle_class.)doc";

static const char*
    __doc_operations_research_routing_Model_ConcatenateOperators =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_CostCacheElement =
    R"doc(Storage of a cost cache element corresponding to a cost arc ending at
node 'index' and on the cost class 'cost_class'.)doc";

static const char*
    __doc_operations_research_routing_Model_CostCacheElement_cost =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_CostCacheElement_cost_class_index =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_CostCacheElement_index =  // NOLINT
    R"doc(This is usually an int64_t, but using an int here decreases the RAM
usage, and should be fine since in practice we never have more than
1<<31 vars. Note(user): on 2013-11, microbenchmarks on the arc costs
callbacks also showed a 2% speed-up thanks to using int rather than
int64_t.)doc";

static const char* __doc_operations_research_routing_Model_CostClass =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Model_CostClass_CostClass =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_CostClass_DimensionCost =  // NOLINT
    R"doc(Only dimensions that have non-zero cost evaluator and a non-zero cost
coefficient (in this cost class) are listed here. Since we only need
their transit evaluator (the raw version that takes var index, not
Node Index) and their span cost coefficient, we just store those. This
is sorted by the natural operator < (and *not* by DimensionIndex).)doc";

static const char*
    __doc_operations_research_routing_Model_CostClass_DimensionCost_dimension =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_CostClass_DimensionCost_operator_lt =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_CostClass_DimensionCost_slack_cost_coefficient =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_CostClass_DimensionCost_span_cost_coefficient =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_CostClass_DimensionCost_transit_evaluator_class =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_CostClass_dimension_transit_evaluator_class_and_cost_coefficient =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_CostClass_evaluator_index =  // NOLINT
    R"doc(Index of the arc cost evaluator, registered in the Model class.)doc";

static const char* __doc_operations_research_routing_Model_CostVar =
    R"doc(Returns the global cost variable which is being minimized.)doc";

static const char*
    __doc_operations_research_routing_Model_CostsAreHomogeneousAcrossVehicles =
        R"doc(Whether costs are homogeneous across all vehicles.)doc";

static const char* __doc_operations_research_routing_Model_CreateCPOperator =
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_CreateCPOperator_2 =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_CreateCPOperatorWithNeighbors =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_CreateCPOperatorWithNeighbors_2 =
        R"doc()doc";

static const char* __doc_operations_research_routing_Model_CreateDisjunction =
    R"doc(Returns nullptr if no penalty cost, otherwise returns penalty
variable.)doc";

static const char*
    __doc_operations_research_routing_Model_CreateFirstSolutionDecisionBuilders =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_CreateInsertionOperator =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_CreateIntVarFilteredDecisionBuilder =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_CreateLocalSearchFilters =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_CreateLocalSearchParameters =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_CreateMakeInactiveOperator =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_CreateNeighborhoodOperators =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_CreateOperator =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Model_CreateOperator_2 =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_CreateOperatorWithNeighbors =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_CreateOperatorWithNeighborsRatio =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_CreateOperatorWithNeighborsRatio_2 =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_CreateOperatorWithNeighborsRatio_3 =
        R"doc()doc";

static const char* __doc_operations_research_routing_Model_CreatePairOperator =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_CreatePairOperator_2 =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_CreatePrimaryLocalSearchDecisionBuilder =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_CreateSameVehicleCost =  // NOLINT
    R"doc(Returns the cost variable related to the soft same vehicle constraint
of index 'vehicle_index'.)doc";

static const char*
    __doc_operations_research_routing_Model_CreateSolutionFinalizer =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_DebugOutputAssignment =  // NOLINT
    R"doc(Print some debugging information about an assignment, including the
feasible intervals of the CumulVar for dimension "dimension_to_print"
at each step of the routes. If "dimension_to_print" is omitted, all
dimensions will be printed.)doc";

static const char*
    __doc_operations_research_routing_Model_DetectImplicitPickupAndDeliveries =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_DimensionCumulOptimizers =  // NOLINT
    R"doc(Internal struct used to store the lp/mp versions of the local and
global cumul optimizers for a given dimension.)doc";

static const char*
    __doc_operations_research_routing_Model_DimensionCumulOptimizers_lp_optimizer =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_DimensionCumulOptimizers_mp_optimizer =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_DisjunctionValues =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_DisjunctionValues_max_cardinality =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_DisjunctionValues_penalty =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_DoRestoreAssignment =
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_End =
    R"doc(Returns the variable index of the ending node of a vehicle 
route.)doc";

static const char*
    __doc_operations_research_routing_Model_FastSolveFromAssignmentWithParameters =  // NOLINT
    R"doc(Improves a given assignment using unchecked local search. If
check_solution_in_cp is true the final solution will be checked with
the CP solver. As of 11/2023, only works with greedy descent.)doc";

static const char* __doc_operations_research_routing_Model_FilterOptions =
    R"doc()doc";  // NOLINT

static const char*
    __doc_operations_research_routing_Model_FilterOptions_filter_objective =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_FilterOptions_filter_with_cp_solver =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_FilterOptions_operator_eq =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_FinalizeAllowedVehicles =  // NOLINT
    R"doc(The following method looks at node-to-vehicle assignment feasibility
based on node transit values on unary dimensions: If the (absolute)
value of transit for a node is greater than the vehicle capacity on
any dimension, this node cannot be served by this vehicle and the
latter is thus removed from allowed_vehicles_[node].)doc";

static const char* __doc_operations_research_routing_Model_FinalizeVisitTypes =
    R"doc(This method scans the visit types and sets up the following members: -
single_nodes_of_type_[type] contains indices of nodes of visit type
"type" which are not part of any pickup/delivery pair. -
pair_indices_of_type_[type] is the set of "pair_index" such that
pickup_delivery_pairs_[pair_index] has at least one pickup or delivery
with visit type "type". - topologically_sorted_visit_types_ contains
the visit types in topological order based on required-->dependent
arcs from the visit type requirements.)doc";

static const char*
    __doc_operations_research_routing_Model_FindErrorInSearchParametersForModel =  // NOLINT
    R"doc(Checks that the current search parameters are valid for the current model's specific settings.)doc";  // NOLINT

static const char* __doc_operations_research_routing_Model_FindNextActive =
    R"doc(Returns the first active variable index in 'indices' starting from
index + 1.)doc";

static const char*
    __doc_operations_research_routing_Model_ForEachNodeInDisjunctionWithMaxCardinalityFromIndex =  // NOLINT
    R"doc(Calls f for each variable index of indices in the same disjunctions as
the node corresponding to the variable index 'index'; only
disjunctions of cardinality 'cardinality' are considered.)doc";

static const char*
    __doc_operations_research_routing_Model_GetAllDimensionNames =  // NOLINT
    R"doc(Outputs the names of all dimensions added to the routing engine.)doc";

static const char*
    __doc_operations_research_routing_Model_GetAmortizedLinearCostFactorOfVehicles =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetAmortizedQuadraticCostFactorOfVehicles =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_GetArcCostForClass =
    R"doc(Returns the cost of the segment between two nodes for a given cost
class. Input are variable indices of nodes and the cost class. Unlike
GetArcCostForVehicle(), if cost_class is kNoCost, then the returned
cost won't necessarily be zero: only some of the components of the
cost that depend on the cost class will be omited. See the code for
details.)doc";

static const char*
    __doc_operations_research_routing_Model_GetArcCostForClassInternal =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetArcCostForFirstSolution =  // NOLINT
    R"doc(Returns the cost of the arc in the context of the first solution
strategy. This is typically a simplification of the actual cost; see
the .cc.)doc";

static const char*
    __doc_operations_research_routing_Model_GetArcCostForVehicle =  // NOLINT
    R"doc(Returns the cost of the transit arc between two nodes for a given
vehicle. Input are variable indices of node. This returns 0 if vehicle
< 0.)doc";

static const char*
    __doc_operations_research_routing_Model_GetAutomaticFirstSolutionStrategy =
        R"doc(Returns the automatic first solution strategy selected.)doc";

static const char* __doc_operations_research_routing_Model_GetBinCapacities =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetCostClassIndexOfVehicle =  // NOLINT
    R"doc(Get the cost class index of the given vehicle.)doc";

static const char* __doc_operations_research_routing_Model_GetCostClassesCount =
    R"doc(Returns the number of different cost classes in the model.)doc";

static const char* __doc_operations_research_routing_Model_GetCumulBounds =
    R"doc()doc";  // NOLINT

static const char*
    __doc_operations_research_routing_Model_GetDeliveryPositions =  // NOLINT
    R"doc(Returns the pickup and delivery positions where the node is a
delivery.)doc";

static const char* __doc_operations_research_routing_Model_GetDepot =
    R"doc(Returns the variable index of the first starting or ending node of all
routes. If all routes start and end at the same node (single depot),
this is the node returned.)doc";

static const char* __doc_operations_research_routing_Model_GetDimensionIndex =
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_GetDimensionOrDie =
    R"doc(Returns a dimension from its name. Dies if the dimension does not
exist.)doc";

static const char*
    __doc_operations_research_routing_Model_GetDimensionResourceGroupIndex =
        R"doc(Returns the index of the resource group attached to the dimension.
DCHECKS that there's exactly one resource group for this dimension.)doc";

static const char*
    __doc_operations_research_routing_Model_GetDimensionResourceGroupIndices =
        R"doc(Returns the indices of resource groups for this dimension. This 
method can only be called after the model has been closed.)doc";

static const char*
    __doc_operations_research_routing_Model_GetDimensionTransitCostSum =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_GetDimensions =
    R"doc(Returns all dimensions of the model.)doc";

static const char*
    __doc_operations_research_routing_Model_GetDimensionsWithGlobalCumulOptimizers =  // NOLINT
    R"doc(Returns the dimensions which have
[global|local]_dimension_optimizers_.)doc";

static const char*
    __doc_operations_research_routing_Model_GetDimensionsWithLocalCumulOptimizers =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetDimensionsWithSoftOrSpanCosts =
        R"doc(Returns dimensions with soft or vehicle span costs.)doc";

static const char*
    __doc_operations_research_routing_Model_GetDisjunctionIndices =  // NOLINT
    R"doc(Returns the indices of the disjunctions to which an index 
belongs.)doc";

static const char*
    __doc_operations_research_routing_Model_GetDisjunctionMaxCardinality =
        R"doc(Returns the maximum number of possible active nodes of the node
disjunction of index 'index'.)doc";

static const char*
    __doc_operations_research_routing_Model_GetDisjunctionNodeIndices =  // NOLINT
    R"doc(Returns the variable indices of the nodes in the disjunction of index
'index'.)doc";

static const char*
    __doc_operations_research_routing_Model_GetDisjunctionPenalty =  // NOLINT
    R"doc(Returns the penalty of the node disjunction of index 'index'.)doc";

static const char*
    __doc_operations_research_routing_Model_GetFilteredFirstSolutionDecisionBuilderOrNull =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetFirstSolutionDecisionBuilder =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetFixedCostOfVehicle =  // NOLINT
    R"doc(Returns the route fixed cost taken into account if the route of the
vehicle is not empty, aka there's at least one node on the route other
than the first and last nodes.)doc";

static const char*
    __doc_operations_research_routing_Model_GetGlobalCumulOptimizerIndex =
        R"doc(Returns the internal global/local optimizer index for the given
dimension if any, and -1 otherwise.)doc";

static const char*
    __doc_operations_research_routing_Model_GetHardTypeIncompatibilitiesOfType =
        R"doc(Returns visit types incompatible with a given type.)doc";

static const char* __doc_operations_research_routing_Model_GetHomogeneousCost =
    R"doc(Returns the cost of the segment between two nodes supposing all
vehicle costs are the same (returns the cost for the first vehicle
otherwise).)doc";

static const char*
    __doc_operations_research_routing_Model_GetImplicitUniquePickupAndDeliveryPairs =  // NOLINT
    R"doc(Returns implicit pickup and delivery pairs currently in the model.
Pairs are implicit if they are not linked by a pickup and delivery
constraint but that for a given unary dimension, the first element of
the pair has a positive demand d, and the second element has a demand
of -d.)doc";

static const char*
    __doc_operations_research_routing_Model_GetLocalCumulOptimizerIndex =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetMaximumNumberOfActiveVehicles =
        R"doc(Returns the maximum number of active vehicles.)doc";

static const char*
    __doc_operations_research_routing_Model_GetMutableCPInterrupt =  // NOLINT
    R"doc(Returns the atomic<bool> to stop the CP solver.)doc";

static const char*
    __doc_operations_research_routing_Model_GetMutableCPSatInterrupt =  // NOLINT
    R"doc(Returns the atomic<bool> to stop the CP-SAT solver.)doc";

static const char* __doc_operations_research_routing_Model_GetMutableDimension =
    R"doc(Returns a dimension from its name. Returns nullptr if the dimension
does not exist.)doc";

static const char*
    __doc_operations_research_routing_Model_GetMutableGlobalCumulLPOptimizer =
        R"doc(Returns the global/local dimension cumul optimizer for a given
dimension, or nullptr if there is none.)doc";

static const char*
    __doc_operations_research_routing_Model_GetMutableGlobalCumulMPOptimizer =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetMutableLocalCumulLPOptimizer =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetMutableLocalCumulMPOptimizer =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetNeighborhoodOperators =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_GetNextVarMax =
    R"doc(Get the current maximum of the next variable associated to 
index.)doc";

static const char* __doc_operations_research_routing_Model_GetNextVarMin =
    R"doc(Get the current minimum of the next variable associated to 
index.)doc";

static const char*
    __doc_operations_research_routing_Model_GetNonZeroCostClassesCount =  // NOLINT
    R"doc(Ditto, minus the 'always zero', built-in cost class.)doc";

static const char*
    __doc_operations_research_routing_Model_GetNumOfSingletonNodes =  // NOLINT
    R"doc(Returns the number of non-start/end nodes which do not appear in a
pickup/delivery pair.)doc";

static const char*
    __doc_operations_research_routing_Model_GetNumberOfDecisionsInFirstSolution =  // NOLINT
    R"doc(Returns statistics on first solution search, number of decisions 
sent to filters, number of decisions rejected by filters.)doc";

static const char*
    __doc_operations_research_routing_Model_GetNumberOfDisjunctions =  // NOLINT
    R"doc(Returns the number of node disjunctions in the model.)doc";

static const char*
    __doc_operations_research_routing_Model_GetNumberOfRejectsInFirstSolution =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetNumberOfVisitTypes =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetOrCreateAssignment =  // NOLINT
    R"doc(Set of auxiliary methods used to setup the search.)doc";

static const char*
    __doc_operations_research_routing_Model_GetOrCreateCumulativeLimit =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetOrCreateFirstSolutionLargeNeighborhoodSearchLimit =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetOrCreateLargeNeighborhoodSearchLimit =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_GetOrCreateLimit =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetOrCreateLocalSearchFilterManager =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetOrCreateLocalSearchLimit =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetOrCreateNodeNeighborsByCostClass =  // NOLINT
    R"doc(Returns neighbors of all nodes for every cost class. The result is
cached and is computed once. The number of neighbors considered is
based on a ratio of non-vehicle nodes, specified by neighbors_ratio,
with a minimum of min-neighbors node considered.)doc";

static const char*
    __doc_operations_research_routing_Model_GetOrCreateNodeNeighborsByCostClass_2 =  // NOLINT
    R"doc(Returns parameters.num_neighbors neighbors of all nodes for every 
cost class. The result is cached and is computed once.)doc";

static const char*
    __doc_operations_research_routing_Model_GetOrCreateTmpAssignment =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetPairIndicesOfType =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_GetPathsMetadata =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetPerfectBinaryDisjunctions =
        R"doc(Returns the list of all perfect binary disjunctions, as pairs of
variable indices: a disjunction is "perfect" when its variables do not
appear in any other disjunction. Each pair is sorted (lowest variable
index first), and the output vector is also sorted (lowest pairs
first).)doc";

static const char*
    __doc_operations_research_routing_Model_GetPickupAndDeliveryDisjunctions =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetPickupAndDeliveryPairs =  // NOLINT
    R"doc(Returns pickup and delivery pairs currently in the model.)doc";

static const char*
    __doc_operations_research_routing_Model_GetPickupAndDeliveryPolicyOfVehicle =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_GetPickupPositions =
    R"doc(Returns the pickup and delivery positions where the node is a 
pickup.)doc";

static const char*
    __doc_operations_research_routing_Model_GetPrimaryConstrainedDimension =
        R"doc(Get the primary constrained dimension, or an empty string if it is
unset.)doc";

static const char* __doc_operations_research_routing_Model_GetResourceGroup =
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_GetResourceGroups =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetRoutesFromAssignment =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetSameVehicleIndicesOfIndex =  // NOLINT
    R"doc(Returns variable indices of nodes constrained to be on the same 
route.)doc";

static const char* __doc_operations_research_routing_Model_GetSearchMonitors =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetSingleNodesOfType =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetTemporalTypeIncompatibilitiesOfType =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetTopologicallySortedVisitTypes =
        R"doc()doc";

static const char* __doc_operations_research_routing_Model_GetUnaryDimensions =
    R"doc(Returns dimensions for which all transit evaluators are unary.)doc";

static const char*
    __doc_operations_research_routing_Model_GetVehicleClassIndexOfVehicle =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetVehicleClassesCount =  // NOLINT
    R"doc(Returns the number of different vehicle classes in the model.)doc";

static const char* __doc_operations_research_routing_Model_GetVehicleOfClass =
    R"doc(Returns a vehicle of the given vehicle class, and -1 if there are no
vehicles for this class.)doc";

static const char*
    __doc_operations_research_routing_Model_GetVehicleStartClass =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_GetVehicleTypeContainer =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_GetVisitType =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Model_GetVisitTypePolicy =
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_HasDimension =
    R"doc(Returns true if a dimension exists for a given dimension name.)doc";

static const char*
    __doc_operations_research_routing_Model_HasGlobalCumulOptimizer =  // NOLINT
    R"doc(Returns whether the given dimension has global/local cumul 
optimizers.)doc";

static const char*
    __doc_operations_research_routing_Model_HasHardTypeIncompatibilities =  // NOLINT
    R"doc(Returns true iff any hard (resp. temporal) type incompatibilities have
been added to the model.)doc";

static const char*
    __doc_operations_research_routing_Model_HasLocalCumulOptimizer =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_HasMandatoryDisjunctions =  // NOLINT
    R"doc(Returns true if the model contains mandatory disjunctions (ones with
kNoPenalty as penalty).)doc";

static const char*
    __doc_operations_research_routing_Model_HasMaxCardinalityConstrainedDisjunctions =  // NOLINT
    R"doc(Returns true if the model contains at least one disjunction which 
is constrained by its max_cardinality.)doc";

static const char*
    __doc_operations_research_routing_Model_HasSameVehicleTypeRequirements =
        R"doc(Returns true iff any same-route (resp. temporal) type requirements
have been added to the model.)doc";

static const char*
    __doc_operations_research_routing_Model_HasTemporalTypeIncompatibilities =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_HasTemporalTypeRequirements =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_HasTypeRegulations =
    R"doc(Returns true iff the model has any incompatibilities or requirements
set on node types.)doc";

static const char*
    __doc_operations_research_routing_Model_HasVehicleWithCostClassIndex =
        R"doc(Returns true iff the model contains a vehicle with the given
cost_class_index.)doc";

static const char*
    __doc_operations_research_routing_Model_IgnoreDisjunctionsAlreadyForcedToZero =  // NOLINT
    R"doc(SPECIAL: Makes the solver ignore all the disjunctions whose active
variables are all trivially zero (i.e. Max() == 0), by setting their
max_cardinality to 0. This can be useful when using the
BaseBinaryDisjunctionNeighborhood operators, in the context of arc-
based routing.)doc";

static const char*
    __doc_operations_research_routing_Model_InitSameVehicleGroups =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_Initialize =
    R"doc(Internal methods.)doc";

static const char*
    __doc_operations_research_routing_Model_InitializeDimensionInternal =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_IsDelivery =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Model_IsEnd =
    R"doc(Returns true if 'index' represents the last node of a route.)doc";

static const char* __doc_operations_research_routing_Model_IsMatchingModel =
    R"doc(Returns true if a vehicle/node matching problem is detected.)doc";

static const char* __doc_operations_research_routing_Model_IsPickup =
    R"doc(Returns whether the node is a pickup (resp. delivery).)doc";

static const char* __doc_operations_research_routing_Model_IsStart =
    R"doc(Returns true if 'index' represents the first node of a route.)doc";

static const char*
    __doc_operations_research_routing_Model_IsVehicleAllowedForIndex =  // NOLINT
    R"doc(Returns true if a vehicle is allowed to visit a given node.)doc";

static const char* __doc_operations_research_routing_Model_IsVehicleUsed =
    R"doc(Returns true if the route of 'vehicle' is non empty in 
'assignment'.)doc";

static const char*
    __doc_operations_research_routing_Model_IsVehicleUsedWhenEmpty =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_LogSolution =
    R"doc(Log a solution.)doc";

static const char*
    __doc_operations_research_routing_Model_MakeGreedyDescentLSOperator =  // NOLINT
    R"doc(Perhaps move it to constraint_solver.h. MakeGreedyDescentLSOperator
creates a local search operator that tries to improve the initial
assignment by moving a logarithmically decreasing step away in each
possible dimension.)doc";

static const char*
    __doc_operations_research_routing_Model_MakeGuidedSlackFinalizer =  // NOLINT
    R"doc(MakeGuidedSlackFinalizer creates a DecisionBuilder for the slacks of a
dimension using a callback to choose which values to start with. The
finalizer works only when all next variables in the model have been
fixed. It has the following two characteristics: 1. It follows the
routes defined by the nexts variables when choosing a variable to make
a decision on. 2. When it comes to choose a value for the slack of
node i, the decision builder first calls the callback with argument i,
and supposingly the returned value is x it creates decisions slack[i]
= x, slack[i] = x + 1, slack[i] = x - 1, slack[i] = x + 2, etc.)doc";

static const char*
    __doc_operations_research_routing_Model_MakeSelfDependentDimensionFinalizer =  // NOLINT
    R"doc(__SWIG__ MakeSelfDependentDimensionFinalizer is a finalizer for 
the slacks of a self-dependent dimension. It makes an extensive use of the
caches of the state dependent transits. In detail,
MakeSelfDependentDimensionFinalizer returns a composition of a local
search decision builder with a greedy descent operator for the cumul
of the start of each route and a guided slack finalizer. Provided
there are no time windows and the maximum slacks are large enough,
once the cumul of the start of route is fixed, the guided finalizer
can find optimal values of the slacks for the rest of the route in
time proportional to the length of the route. Therefore the composed
finalizer generally works in time O(log(t)*n*m), where t is the latest
possible departute time, n is the number of nodes in the network and m
is the number of vehicles.)doc";

static const char*
    __doc_operations_research_routing_Model_MakeStateDependentTransit =  // NOLINT
    R"doc(Creates a cached StateDependentTransit from an std::function.)doc";

static const char*
    __doc_operations_research_routing_Model_MutablePreAssignment =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_Next =
    R"doc(Assignment inspection Returns the variable index of the node directly
after the node corresponding to 'index' in 'assignment'.)doc";

static const char* __doc_operations_research_routing_Model_NextVar =
    R"doc(!defined(SWIGPYTHON) Returns the next variable of the node
corresponding to index. Note that NextVar(index) == index is
equivalent to ActiveVar(index) == 0.)doc";

static const char* __doc_operations_research_routing_Model_Nexts =
    R"doc(Returns all next variables of the model, such that Nexts(i) is the
next variable of the node corresponding to i.)doc";

static const char*
    __doc_operations_research_routing_Model_NodeNeighborsByCostClass =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_NodeNeighborsByCostClass_ComputeNeighbors =  // NOLINT
    R"doc(Computes num_neighbors neighbors of all nodes for every cost class
 in routing_model.)doc";

static const char*
    __doc_operations_research_routing_Model_NodeNeighborsByCostClass_GetNeighborsOfNodeForCostClass =  // NOLINT
    R"doc(Returns the neighbors of the given node for the given 
cost_class.)doc";

static const char*
    __doc_operations_research_routing_Model_NodeNeighborsByCostClass_NodeNeighborsByCostClass =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_NodeNeighborsByCostClass_all_nodes =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_NodeNeighborsByCostClass_node_index_to_neighbors_by_cost_class =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_NodeNeighborsParameters =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_NodeNeighborsParameters_add_vehicle_starts_to_neighbors =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_NodeNeighborsParameters_num_neighbors =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_NodeNeighborsParameters_operator_eq =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_OptimizeCumulsOfDimensionFromAssignmentWithCumulDependentTransits =  // NOLINT
    R"doc(This method optimizes the cumuls of the 'original_assignment' for the
given 'dimension'. The goal of the optimizer is to find an optimal
scheduling w.r.t the original costs in the model, while also
minimizing the "error" in the transit value considered between
consecutive nodes compared to the given piecewise linear formulations.)doc";

static const char*
    __doc_operations_research_routing_Model_PackCumulsOfOptimizerDimensionsFromAssignment =  // NOLINT
    R"doc(For every dimension in the model with an optimizer in
local/global_dimension_optimizers_, this method tries to pack the
cumul values of the dimension, such that: - The cumul costs (span
costs, soft lower and upper bound costs, etc) are minimized. - The
cumuls of the ends of the routes are minimized for this given minimal
cumul cost. - Given these minimal end cumuls, the route start cumuls
are maximized. Returns the assignment resulting from allocating these
packed cumuls with the solver, and nullptr if these cumuls could not
be set by the solver.)doc";

static const char*
    __doc_operations_research_routing_Model_PickupAndDeliveryPolicy =  // NOLINT
    R"doc(Types of precedence policy applied to pickup and delivery pairs.)doc";

static const char*
    __doc_operations_research_routing_Model_PickupAndDeliveryPolicy_PICKUP_AND_DELIVERY_FIFO =  // NOLINT
    R"doc(Deliveries must be performed in the same order as pickups.)doc";

static const char*
    __doc_operations_research_routing_Model_PickupAndDeliveryPolicy_PICKUP_AND_DELIVERY_LIFO =  // NOLINT
    R"doc(Deliveries must be performed in reverse order of pickups.)doc";

static const char*
    __doc_operations_research_routing_Model_PickupAndDeliveryPolicy_PICKUP_AND_DELIVERY_NO_ORDER =  // NOLINT
    R"doc(Any precedence is accepted.)doc";

static const char*
    __doc_operations_research_routing_Model_PickupDeliveryPosition =  // NOLINT
    R"doc(The position of a node in the set of pickup and delivery pairs.)doc";

static const char*
    __doc_operations_research_routing_Model_PickupDeliveryPosition_alternative_index =  // NOLINT
    R"doc(The index of the node in the vector of pickup (resp. delivery)
alternatives of the pair.)doc";

static const char*
    __doc_operations_research_routing_Model_PickupDeliveryPosition_pd_pair_index =  // NOLINT
    R"doc(The index of the pickup and delivery pair within which the node
appears.)doc";

static const char* __doc_operations_research_routing_Model_PreAssignment =
    R"doc(Returns an assignment used to fix some of the variables of the
problem. In practice, this assignment locks partial routes of the
problem. This can be used in the context of locking the parts of the
routes which have already been driven in online routing problems.)doc";

static const char* __doc_operations_research_routing_Model_QuietCloseModel =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_QuietCloseModelWithParameters =
        R"doc()doc";

static const char* __doc_operations_research_routing_Model_ReadAssignment =
    R"doc(Reads an assignment from a file and returns the current solution.
Returns nullptr if the file cannot be opened or if the assignment is
not valid.)doc";

static const char*
    __doc_operations_research_routing_Model_ReadAssignmentFromRoutes =  // NOLINT
    R"doc(Restores the routes as the current solution. Returns nullptr if the
solution cannot be restored (routes do not contain a valid solution).
Note that calling this method will run the solver to assign values to
the dimension variables; this may take considerable amount of time,
especially when using dimensions with slack.)doc";

static const char*
    __doc_operations_research_routing_Model_RegisterStateDependentTransitCallback =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RegisterTransitCallback =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RegisterTransitMatrix =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RegisterUnaryTransitCallback =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RegisterUnaryTransitVector =  // NOLINT
    R"doc(Registers 'callback' and returns its index. The sign parameter allows
to notify the solver that the callback only return values of the given
sign. This can help the solver, but passing an incorrect sign may
crash in non-opt compilation mode, and yield incorrect results in opt.)doc";

static const char* __doc_operations_research_routing_Model_RemainingTime =
    R"doc(Returns the time left in the search limit.)doc";

static const char*
    __doc_operations_research_routing_Model_ReplaceUnusedVehicle =  // NOLINT
    R"doc(Replaces the route of unused_vehicle with the route of active_vehicle
in compact_assignment. Expects that unused_vehicle is a vehicle with
an empty route and that the route of active_vehicle is non-empty. Also
expects that 'assignment' contains the original assignment, from which
compact_assignment was created. Returns true if the vehicles were
successfully swapped; otherwise, returns false.)doc";

static const char* __doc_operations_research_routing_Model_ResourceGroup =
    R"doc(A ResourceGroup defines a set of available Resources with attributes
on one or multiple dimensions. For every ResourceGroup in the model,
each (used) vehicle in the solution which requires a resource (see
NotifyVehicleRequiresResource()) from this group must be assigned to
exactly 1 resource, and each resource can in turn be assigned to at
most 1 vehicle requiring it. This vehicle-to-resource assignment will
apply the corresponding Attributes to the dimensions affected by the
resource group. NOTE: As of 2021/07, each ResourceGroup can only
affect a single Dimension at a time, i.e. all Resources in a
group must apply attributes to the same single dimension.)doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_AddResource =  // NOLINT
    R"doc(Adds a Resource with the given attributes for the corresponding
dimension. Returns the index of the added resource in resources_.)doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_Attributes =  // NOLINT
    R"doc(Attributes for a dimension.)doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_Attributes_Attributes =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_Attributes_Attributes_2 =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_Attributes_end_domain =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_Attributes_end_domain_2 =  // NOLINT
    R"doc(end_domain_.Min() <= cumul[End(v)] <= end_domain_.Max())doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_Attributes_start_domain =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_Attributes_start_domain_2 =  // NOLINT
    R"doc(The following domains constrain the dimension start/end cumul of 
the vehicle assigned to this resource: start_domain_.Min() <=
cumul[Start(v)] <= start_domain_.Max())doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_ClearAllowedResourcesForVehicle =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_ComputeResourceClasses =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_GetAffectedDimensionIndices =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_GetDimensionAttributesForClass =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_GetResource =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_GetResourceClassIndex =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_GetResourceClassesCount =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_GetResourceIndicesInClass =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_GetResourceIndicesPerClass =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_GetResources =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_GetResourcesMarkedAllowedForVehicle =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_GetVehiclesRequiringAResource =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_ResourceGroup_Index =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_IsResourceAllowedForVehicle =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_NotifyVehicleRequiresAResource =  // NOLINT
    R"doc(Notifies that the given vehicle index requires a resource from this
group if the vehicle is used (i.e. if its route is non-empty or
vehicle_used_when_empty_[vehicle] is true).)doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_Resource =  // NOLINT
    R"doc(A Resource sets attributes (costs/constraints) for a set of
dimensions.)doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_ResourceGroup =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_Resource_GetDefaultAttributes =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_Resource_GetDimensionAttributes =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_Resource_Resource =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_Resource_SetDimensionAttributes =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_Resource_dimension_attributes =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_Resource_model =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_SetAllowedResourcesForVehicle =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_ResourceGroup_Size =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_VehicleRequiresAResource =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_affected_dimension_indices =  // NOLINT
    R"doc(All indices of dimensions affected by this resource group.)doc";

static const char* __doc_operations_research_routing_Model_ResourceGroup_index =
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_ResourceGroup_model =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_resource_class_indices =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_resource_indices_per_class =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_resources =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_vehicle_requires_resource =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_ResourceGroup_vehicles_requiring_resource =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_ResourceVar =
    R"doc(Returns the resource variable for the given vehicle index in the given
resource group. If a vehicle doesn't require a resource from the
corresponding resource group, then ResourceVar(v, r_g) == -1.)doc";

static const char* __doc_operations_research_routing_Model_ResourceVars =
    R"doc(Returns vehicle resource variables for a given resource group, such
that ResourceVars(r_g)[v] is the resource variable for vehicle 'v' in
resource group 'r_g'.)doc";

static const char* __doc_operations_research_routing_Model_RestoreAssignment =
    R"doc(Restores an assignment as a solution in the routing model and returns
the new solution. Returns nullptr if the assignment is not valid.)doc";

static const char*
    __doc_operations_research_routing_Model_RouteCanBeUsedByVehicle =  // NOLINT
    R"doc(Checks that all nodes on the route starting at start_index (using the
solution stored in assignment) can be visited by the given vehicle.)doc";

static const char*
    __doc_operations_research_routing_Model_RouteDimensionTravelInfo =  // NOLINT
    R"doc(Contains the information needed by the solver to optimize a
dimension's cumuls with travel-start dependent transit values.)doc";

static const char*
    __doc_operations_research_routing_Model_RouteDimensionTravelInfo_DebugString =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RouteDimensionTravelInfo_TransitionInfo =  // NOLINT
    R"doc(Contains the information for a single transition on the 
route.)doc";

static const char*
    __doc_operations_research_routing_Model_RouteDimensionTravelInfo_TransitionInfo_DebugString =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RouteDimensionTravelInfo_TransitionInfo_PiecewiseLinearFormulation =  // NOLINT
    R"doc(The following struct defines a piecewise linear formulation, with
int64_t values for the "anchor" x and y values, and potential double
values for the slope of each linear function.)doc";

static const char*
    __doc_operations_research_routing_Model_RouteDimensionTravelInfo_TransitionInfo_PiecewiseLinearFormulation_DebugString =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RouteDimensionTravelInfo_TransitionInfo_PiecewiseLinearFormulation_x_anchors =  // NOLINT
    R"doc(The set of *increasing* anchor cumul values for the 
interpolation.)doc";

static const char*
    __doc_operations_research_routing_Model_RouteDimensionTravelInfo_TransitionInfo_PiecewiseLinearFormulation_y_anchors =  // NOLINT
    R"doc(The y values used for the interpolation: For any x anchor value, let i
be an index such that x_anchors[i] ≤ x < x_anchors[i+1], then the y
value for x is y_anchors[i] * (1-λ) + y_anchors[i+1] * λ, with λ = (x
- x_anchors[i]) / (x_anchors[i+1] - x_anchors[i]).)doc";

static const char*
    __doc_operations_research_routing_Model_RouteDimensionTravelInfo_TransitionInfo_compressed_travel_value_lower_bound =  // NOLINT
    R"doc(The hard lower bound of the compressed travel value that will be
enforced by the scheduling module.)doc";

static const char*
    __doc_operations_research_routing_Model_RouteDimensionTravelInfo_TransitionInfo_post_travel_transit_value =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RouteDimensionTravelInfo_TransitionInfo_pre_travel_transit_value =  // NOLINT
    R"doc(The parts of the transit which occur pre/post travel between the
nodes. The total transit between the two nodes i and j is =
pre_travel_transit_value + travel(i, j) + post_travel_transit_value.)doc";

static const char*
    __doc_operations_research_routing_Model_RouteDimensionTravelInfo_TransitionInfo_travel_compression_cost =  // NOLINT
    R"doc(travel_compression_cost models the cost of the difference between the
(real) travel value Tᵣ given by travel_start_dependent_travel and the
compressed travel value considered in the scheduling.)doc";

static const char*
    __doc_operations_research_routing_Model_RouteDimensionTravelInfo_TransitionInfo_travel_start_dependent_travel =  // NOLINT
    R"doc(Models the (real) travel value Tᵣ, for this transition based on the
departure value of the travel.)doc";

static const char*
    __doc_operations_research_routing_Model_RouteDimensionTravelInfo_TransitionInfo_travel_value_upper_bound =  // NOLINT
    R"doc(The hard upper bound of the (real) travel value Tᵣ (see above). This
value should be chosen so as to prevent the overall cost of the model
(dimension costs + travel_compression_cost) to overflow.)doc";

static const char*
    __doc_operations_research_routing_Model_RouteDimensionTravelInfo_transition_info =  // NOLINT
    R"doc(For each node #i on the route, transition_info[i] contains the
relevant information for the travel between nodes #i and #(i + 1) on
the route.)doc";

static const char*
    __doc_operations_research_routing_Model_RouteDimensionTravelInfo_travel_cost_coefficient =  // NOLINT
    R"doc(The cost per unit of travel for this vehicle.)doc";

static const char* __doc_operations_research_routing_Model_RoutesToAssignment =
    R"doc(Fills an assignment from a specification of the routes of the
vehicles. The routes are specified as lists of variable indices that
appear on the routes of the vehicles. The indices of the outer vector
in 'routes' correspond to vehicles IDs, the inner vector contains the
variable indices on the routes for the given vehicle. The inner
vectors must not contain the start and end indices, as these are
determined by the routing model. Sets the value of NextVars in the
assignment, adding the variables to the assignment if necessary. The
method does not touch other variables in the assignment. The method
can only be called after the model is closed. With
ignore_inactive_indices set to false, this method will fail (return
nullptr) in case some of the route contain indices that are
deactivated in the model; when set to true, these indices will be
skipped. Returns true if routes were successfully loaded. However,
such assignment still might not be a valid solution to the routing
problem due to more complex constraints; it is advisible to call
solver()->CheckSolution() afterwards.)doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator =  // NOLINT
    R"doc(Local search move operator usable in routing.)doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_CROSS =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_CROSS_EXCHANGE =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_EXCHANGE =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_EXCHANGE_PAIR =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_EXCHANGE_RELOCATE_PAIR =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_EXCHANGE_SUBTRIP =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_EXTENDED_SWAP_ACTIVE =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_FULL_PATH_LNS =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_GLOBAL_CHEAPEST_INSERTION_CLOSE_NODES_LNS =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_GLOBAL_CHEAPEST_INSERTION_EXPENSIVE_CHAIN_LNS =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_GLOBAL_CHEAPEST_INSERTION_PATH_LNS =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_INACTIVE_LNS =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_LIGHT_RELOCATE_PAIR =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_LIN_KERNIGHAN =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_LOCAL_CHEAPEST_INSERTION_CLOSE_NODES_LNS =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_LOCAL_CHEAPEST_INSERTION_EXPENSIVE_CHAIN_LNS =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_LOCAL_CHEAPEST_INSERTION_PATH_LNS =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_LOCAL_SEARCH_OPERATOR_COUNTER =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_MAKE_ACTIVE =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_MAKE_ACTIVE_AND_RELOCATE =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_MAKE_CHAIN_INACTIVE =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_MAKE_INACTIVE =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_NODE_PAIR_SWAP =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_OR_OPT =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_PATH_LNS =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_RELOCATE =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_RELOCATE_AND_MAKE_ACTIVE =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_RELOCATE_EXPENSIVE_CHAIN =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_RELOCATE_NEIGHBORS =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_RELOCATE_PAIR =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_RELOCATE_PATH_GLOBAL_CHEAPEST_INSERTION_INSERT_UNPERFORMED =  // NOLINT
    R"doc()doc";  // NOLINT

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_RELOCATE_SUBTRIP =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_SHORTEST_PATH_SWAP_ACTIVE =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_SWAP_ACTIVE =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_TSP_LNS =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_TSP_OPT =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_RoutingLocalSearchOperator_TWO_OPT =
        R"doc()doc";

static const char* __doc_operations_research_routing_Model_RoutingModel =
    R"doc(Constructor taking an index manager. The version which does not take
RoutingModelParameters is equivalent to passing
DefaultRoutingModelParameters().)doc";

static const char* __doc_operations_research_routing_Model_Model_2 =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Model_Model_3 =
    R"doc()doc";  // NOLINT

static const char*
    __doc_operations_research_routing_Model_SafeGetCostClassInt64OfVehicle =
        R"doc()doc";

static const char* __doc_operations_research_routing_Model_SecondaryOptimizer =
    R"doc(Class used to solve a secondary model within a first solution
strategy.)doc";

static const char*
    __doc_operations_research_routing_Model_SecondaryOptimizer_SecondaryOptimizer =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_SecondaryOptimizer_Solve =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_SecondaryOptimizer_call_count =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_SecondaryOptimizer_model =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_SecondaryOptimizer_search_parameters =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_SecondaryOptimizer_solve_period =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_SecondaryOptimizer_state =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_SecondaryOptimizer_var_to_index =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_SetAllowedVehiclesForIndex =  // NOLINT
    R"doc(Sets the vehicles which can visit a given node. If the node is in a
disjunction, this will not prevent it from being unperformed.
Specifying an empty vector of vehicles has no effect (all vehicles
will be allowed to visit the node).)doc";

static const char*
    __doc_operations_research_routing_Model_SetAmortizedCostFactorsOfAllVehicles =  // NOLINT
    R"doc(The following methods set the linear and quadratic cost factors of
vehicles (must be positive values). The default value of these
parameters is zero for all vehicles.

When set, the cost_ of the model will contain terms aiming at reducing
the number of vehicles used in the model, by adding the following to
the objective for every vehicle v: INDICATOR(v used in the model) *
[linear_cost_factor_of_vehicle_[v] -
quadratic_cost_factor_of_vehicle_[v]*(square of length of route v)]
i.e. for every used vehicle, we add the linear factor as fixed cost,
and subtract the square of the route length multiplied by the
quadratic factor. This second term aims at making the routes as dense
as possible.

Sets the linear and quadratic cost factor of all vehicles.)doc";

static const char*
    __doc_operations_research_routing_Model_SetAmortizedCostFactorsOfVehicle =
        R"doc(Sets the linear and quadratic cost factor of the given 
vehicle.)doc";

static const char*
    __doc_operations_research_routing_Model_SetArcCostEvaluatorOfAllVehicles =
        R"doc(Sets the cost function of the model such that the cost of a 
segment of a route between node 'from' and 'to' is evaluator(from, to), whatever
the route or vehicle performing the route.)doc";

static const char*
    __doc_operations_research_routing_Model_SetArcCostEvaluatorOfVehicle =
        R"doc(Sets the cost function for a given vehicle route.)doc";

static const char*
    __doc_operations_research_routing_Model_SetAssignmentFromOtherModelAssignment =  // NOLINT
    R"doc(Given a "source_model" and its "source_assignment", resets
"target_assignment" with the IntVar variables (nexts_, and
vehicle_vars_ if costs aren't homogeneous across vehicles) of "this"
model, with the values set according to those in "other_assignment".
The objective_element of target_assignment is set to this->cost_.)doc";

static const char*
    __doc_operations_research_routing_Model_SetFirstSolutionEvaluator =  // NOLINT
    R"doc(Takes ownership of evaluator.)doc";

static const char*
    __doc_operations_research_routing_Model_SetFixedCostOfAllVehicles =  // NOLINT
    R"doc(Sets the fixed cost of all vehicle routes. It is equivalent to calling
SetFixedCostOfVehicle on all vehicle routes.)doc";

static const char*
    __doc_operations_research_routing_Model_SetFixedCostOfVehicle =  // NOLINT
    R"doc(Sets the fixed cost of one vehicle route.)doc";

static const char*
    __doc_operations_research_routing_Model_SetIndexNeighborFinder =  // NOLINT
    R"doc(Sets the sweep arranger to be used by routing heuristics; ownership of
the arranger is taken.)doc";

static const char*
    __doc_operations_research_routing_Model_SetMaximumNumberOfActiveVehicles =
        R"doc(Constrains the maximum number of active vehicles, aka the number 
of vehicles which do not have an empty route. For instance, this can be
used to limit the number of routes in the case where there are fewer
drivers than vehicles and that the fleet of vehicle is heterogeneous.)doc";

static const char* __doc_operations_research_routing_Model_SetNextVarRange =
    R"doc(e.g. intersection with a set of values, removing a set of values,
membership test... on a per-need basis. Restricts the range of the
next variable associated to index.)doc";

static const char*
    __doc_operations_research_routing_Model_SetOptimizedWithCumulDependentTransits =  // NOLINT
    R"doc(Notifies that the given dimension's cumuls can be optimized with
cumul-dependent transits.)doc";

static const char*
    __doc_operations_research_routing_Model_SetPathEnergyCostOfVehicle =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_SetPathEnergyCostsOfVehicle =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_SetPickupAndDeliveryPolicyOfAllVehicles =  // NOLINT
    R"doc(Sets the Pickup and delivery policy of all vehicles. It is 
equivalent to calling SetPickupAndDeliveryPolicyOfVehicle on all vehicles.)doc";

static const char*
    __doc_operations_research_routing_Model_SetPickupAndDeliveryPolicyOfVehicle =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_SetPrimaryConstrainedDimension =
        R"doc(Set the given dimension as "primary constrained". As of August 
2013, this is only used by ArcIsMoreConstrainedThanArc(). "dimension" must
be the name of an existing dimension, or be empty, in which case there
will not be a primary dimension after this call.)doc";

static const char* __doc_operations_research_routing_Model_SetSameVehicleGroup =
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_SetSecondaryModel =
    R"doc(Sets a secondary solver (routing model + parameters) which can be used
to run sub-solves while building a first solution.)doc";

static const char* __doc_operations_research_routing_Model_SetSweepArranger =
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_SetTabuVarsCallback =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_SetVehicleUsedWhenEmpty =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_SetVisitType =
    R"doc()doc";  // NOLINT

static const char*
    __doc_operations_research_routing_Model_SetupAssignmentCollector =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_SetupDecisionBuilders =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_SetupImprovementLimit =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_SetupMetaheuristics =
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_SetupSearch =
    R"doc(Sets up search objects, such as decision builders and monitors.)doc";

static const char* __doc_operations_research_routing_Model_SetupSearchMonitors =
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_SetupTrace =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Model_Size =
    R"doc(Returns the number of next variables in the model.)doc";

static const char* __doc_operations_research_routing_Model_Solve =
    R"doc(Solves the current routing model; closes the current model. This is
equivalent to calling
SolveWithParameters(DefaultRoutingSearchParameters()) or
SolveFromAssignmentWithParameters(assignment,
DefaultRoutingSearchParameters()).)doc";

static const char*
    __doc_operations_research_routing_Model_SolveFromAssignmentWithParameters =
        R"doc(Same as above, except that if assignment is not null, it will be 
used as the initial solution.)doc";

static const char*
    __doc_operations_research_routing_Model_SolveFromAssignmentsWithParameters =
        R"doc(Same as above but will try all assignments in order as first 
solutions until one succeeds.)doc";

static const char* __doc_operations_research_routing_Model_SolveMatchingModel =
    R"doc(Solve matching problem with min-cost flow and store result in
assignment.)doc";

static const char*
    __doc_operations_research_routing_Model_SolveWithIteratedLocalSearch =
        R"doc(Solves the current routing model by using an Iterated Local Search
approach.)doc";

static const char* __doc_operations_research_routing_Model_SolveWithParameters =
    R"doc(Solves the current routing model with the given parameters. If
'solutions' is specified, it will contain the k best solutions found
during the search (from worst to best, including the one returned by
this method), where k corresponds to the
'number_of_solutions_to_collect' in 'search_parameters'. Note that the
Assignment returned by the method and the ones in solutions are owned
by the underlying solver and should not be deleted.)doc";

static const char* __doc_operations_research_routing_Model_Start =
    R"doc(Model inspection. Returns the variable index of the starting node of a
vehicle route.)doc";

static const char*
    __doc_operations_research_routing_Model_StateDependentTransit =  // NOLINT
    R"doc(What follows is relevant for models with time/state dependent
transits. Such transits, say from node A to node B, are functions f:
int64_t->int64_t of the cumuls of a dimension. The user is free to
implement the abstract RangeIntToIntFunction interface, but it is
expected that the implementation of each method is quite fast. For
performance-related reasons, StateDependentTransit keeps an additional
pointer to a RangeMinMaxIndexFunction, with similar functionality to
RangeIntToIntFunction, for g(x) = f(x)+x, where f is the transit from
A to B. In most situations the best solutions are problem-specific,
but in case of doubt the user may use the MakeStateDependentTransit
function from the routing library, which works out-of-the-box, with
very good running time, but memory inefficient in some situations.)doc";

static const char*
    __doc_operations_research_routing_Model_StateDependentTransitCallback =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_StateDependentTransit_transit =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_StateDependentTransit_transit_plus_identity =  // NOLINT
    R"doc(f(x))doc";

static const char*
    __doc_operations_research_routing_Model_StoreDimensionCumulOptimizers =  // NOLINT
    R"doc(Creates global and local cumul optimizers for the dimensions needing
them, and stores them in the corresponding
[local|global]_dimension_optimizers_ vectors. This function also
computes and stores the "offsets" for these dimensions, used in the
local/global optimizers to simplify LP computations.

Note on the offsets computation: The global/local cumul offsets are
used by the respective optimizers to have smaller numbers, and
therefore better numerical behavior in the LP. These offsets are used
as a minimum value for the cumuls over the route (or globally), i.e. a
value we consider all cumuls to be greater or equal to. When transits
are all positive, the cumuls of every node on a route is necessarily
greater than the cumul of its start. Therefore, the local offset for a
vehicle can be set to the minimum of its start node's cumul, and for
the global optimizers, to the min start cumul over all vehicles.
However, to be able to distinguish between infeasible nodes (i.e.
nodes for which the cumul upper bound is less than the min cumul of
the vehicle's start), we set the offset to "min_start_cumul" - 1. By
doing so, all infeasible nodes described above will have bounds of [0,
0]. Example: Start cumul bounds: [11, 20] --> offset = 11 - 1 = 10.
Two nodes with cumul bounds. Node1: [5, 10], Node2: [7, 20] After
applying the offset to the above windows, they become: Vehicle: [1,
10]. Node1: [0, 0] (infeasible). Node2: [0, 10].

On the other hand, when transits on a route can be negative, no
assumption can be made on the cumuls of nodes wrt the start cumuls,
and the offset is therefore set to 0.)doc";

static const char* __doc_operations_research_routing_Model_TimeBuffer =
    R"doc(Returns the time buffer to safely return a solution.)doc";

static const char*
    __doc_operations_research_routing_Model_TopologicallySortVisitTypes =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_TransitCallback =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_TransitEvaluatorSign =  // NOLINT
    R"doc(Represents the sign of values returned by a transit evaluator.)doc";

static const char*
    __doc_operations_research_routing_Model_TransitEvaluatorSign_kTransitEvaluatorSignNegativeOrZero =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_TransitEvaluatorSign_kTransitEvaluatorSignPositiveOrZero =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_TransitEvaluatorSign_kTransitEvaluatorSignUnknown =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_UnaryTransitCallbackOrNull =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_UnperformedPenalty =
    R"doc(Get the "unperformed" penalty of a node. This is only well defined if
the node is only part of a single Disjunction, and that disjunction
has a penalty. For forced active nodes returns max int64_t. In all other
cases, this returns 0.)doc";

static const char*
    __doc_operations_research_routing_Model_UnperformedPenaltyOrValue =  // NOLINT
    R"doc(Same as above except that it returns default_value instead of 0 when
penalty is not well defined (default value is passed as first argument
to simplify the usage of the method in a callback).)doc";

static const char*
    __doc_operations_research_routing_Model_UpdateSearchFromParametersIfNeeded =
        R"doc(Updates search objects if parameters have changed.)doc";

static const char* __doc_operations_research_routing_Model_UpdateTimeLimit =
    R"doc(Updates the time limit of the search limit.)doc";

static const char*
    __doc_operations_research_routing_Model_UsesLightPropagation =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_ValuedNodes =
    R"doc(Structure storing a value for a set of variable indices. Is used to
store data for index disjunctions (variable indices, max_cardinality
and penalty when unperformed).)doc";

static const char* __doc_operations_research_routing_Model_ValuedNodes_indices =
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_ValuedNodes_value =
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_VariableValuePair =
    R"doc(Struct used to store a variable value.)doc";

static const char*
    __doc_operations_research_routing_Model_VariableValuePair_value =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_VariableValuePair_var_index =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_VehicleIndex =
    R"doc(Returns the vehicle of the given start/end index, and -1 if the given
index is not a vehicle start/end.)doc";

static const char*
    __doc_operations_research_routing_Model_VehicleRouteConsideredVar =  // NOLINT
    R"doc(Returns the variable specifying whether or not the given vehicle route
is considered for costs and constraints. It will be equal to 1 iff the
route of the vehicle is not empty OR vehicle_used_when_empty_[vehicle]
is true.)doc";

static const char*
    __doc_operations_research_routing_Model_VehicleTypeContainer =  // NOLINT
    R"doc(Struct used to sort and store vehicles by their type. Two vehicles
have the same "vehicle type" iff they have the same cost class and
start/end nodes.)doc";

static const char*
    __doc_operations_research_routing_Model_VehicleTypeContainer_NumTypes =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_VehicleTypeContainer_Type =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_VehicleTypeContainer_VehicleClassEntry =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_VehicleTypeContainer_VehicleClassEntry_fixed_cost =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_VehicleTypeContainer_VehicleClassEntry_operator_lt =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_VehicleTypeContainer_VehicleClassEntry_vehicle_class =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_VehicleTypeContainer_sorted_vehicle_classes_per_type =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_VehicleTypeContainer_type_index_of_vehicle =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_VehicleTypeContainer_vehicles_per_vehicle_class =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_VehicleVar =
    R"doc(Returns the vehicle variable of the node corresponding to index. Note
that VehicleVar(index) == -1 is equivalent to ActiveVar(index) == 0.)doc";

static const char* __doc_operations_research_routing_Model_VehicleVars =
    R"doc(Returns all vehicle variables of the model, such that VehicleVars(i)
is the vehicle variable of the node corresponding to i.)doc";

static const char* __doc_operations_research_routing_Model_VisitTypePolicy =
    R"doc(Set the node visit types and incompatibilities/requirements between
the types (see below).

NOTE: Before adding any incompatibilities and/or requirements on
types: 1) All corresponding node types must have been set. 2)
CloseVisitTypes() must be called so all containers are resized
accordingly.

The following enum is used to describe how a node with a given type
'T' impacts the number of types 'T' on the route when visited, and
thus determines how temporal incompatibilities and requirements take
effect.)doc";

static const char*
    __doc_operations_research_routing_Model_VisitTypePolicy_ADDED_TYPE_REMOVED_FROM_VEHICLE =  // NOLINT
    R"doc(When visited, one instance of type 'T' previously added to the route
(TYPE_ADDED_TO_VEHICLE), if any, is removed from the vehicle. If the
type was not previously added to the route or all added instances have
already been removed, this visit has no effect on the types.)doc";

static const char*
    __doc_operations_research_routing_Model_VisitTypePolicy_TYPE_ADDED_TO_VEHICLE =  // NOLINT
    R"doc(When visited, the number of types 'T' on the vehicle increases by one.)doc";
static const char*
    __doc_operations_research_routing_Model_VisitTypePolicy_TYPE_ON_VEHICLE_UP_TO_VISIT =  // NOLINT
    R"doc(With the following policy, the visit enforces that type 'T' is
considered on the route from its start until this node is visited.)doc";

static const char*
    __doc_operations_research_routing_Model_VisitTypePolicy_TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED =  // NOLINT
    R"doc(The visit doesn't have an impact on the number of types 'T' on the
route, as it's (virtually) added and removed directly. This policy can
be used for visits which are part of an incompatibility or requirement
set without affecting the type count on the route.)doc";

static const char* __doc_operations_research_routing_Model_WriteAssignment =
    R"doc(Writes the current solution to a file containing an AssignmentProto.
Returns false if the file cannot be opened or if there is no current
solution.)doc";

static const char* __doc_operations_research_routing_Model_active = R"doc()doc";

static const char* __doc_operations_research_routing_Model_assignment =
    R"doc()doc";  // NOLINT

static const char*
    __doc_operations_research_routing_Model_at_solution_monitors =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_automatic_first_solution_strategy =
        R"doc()doc";

static const char* __doc_operations_research_routing_Model_bin_capacities =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Model_cache_callbacks =
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_closed = R"doc()doc";

static const char* __doc_operations_research_routing_Model_collect_assignments =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_collect_one_assignment =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_collect_secondary_ls_assignments =
        R"doc()doc";

static const char* __doc_operations_research_routing_Model_cost =
    R"doc(Costs)doc";  // NOLINT

static const char* __doc_operations_research_routing_Model_cost_cache =
    R"doc()doc";  // NOLINT

static const char*
    __doc_operations_research_routing_Model_cost_class_index_of_vehicle =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_cost_classes =
    R"doc()doc";  // NOLINT

static const char*
    __doc_operations_research_routing_Model_costs_are_homogeneous_across_vehicles =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_cumulative_limit =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_dimension_cumuls_optimized_with_cumul_dependent_transits =  // NOLINT
    R"doc(Whether or not a given dimension's cumuls could be optimized with
cumul-dependent transits. This is false by default for all dimensions,
and can be set to true specifically for a dimension through
SetOptimizedWithCumulDependentTransits().)doc";

static const char*
    __doc_operations_research_routing_Model_dimension_local_optimizer_for_cumul_dependent_transits =  // NOLINT
    R"doc(When dimension_cumuls_optimized_with_cumul_dependent_transits_[d] =
true for a dimension which doesn't require an LP/MP optimizer based on
the constraints, we create and store a local MP optimizer required for
optimizing with cumul-dependent transits.)doc";

static const char*
    __doc_operations_research_routing_Model_dimension_name_to_index =  // NOLINT
    R"doc(Dimensions)doc";

static const char*
    __doc_operations_research_routing_Model_dimension_resource_group_indices =
        R"doc(Stores the set of resource groups related to each dimension.)doc";

static const char* __doc_operations_research_routing_Model_dimensions =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Model_disjunctions =
    R"doc(Disjunctions)doc";

static const char*
    __doc_operations_research_routing_Model_enable_deep_serialization =  // NOLINT
    R"doc(Returns the value of the internal enable_deep_serialization_
parameter.)doc";

static const char*
    __doc_operations_research_routing_Model_enable_deep_serialization_2 =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_extra_filters =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Model_extra_intervals =
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_extra_operators =
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_extra_vars =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Model_finalizer_variables =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_first_solution_decision_builders =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_first_solution_evaluator =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_first_solution_evaluator_2 =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_first_solution_filtered_decision_builders =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_first_solution_lns_limit =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_fixed_cost_of_vehicle =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_global_dimension_optimizers =  // NOLINT
    R"doc(TODO(user): Define a new Dimension[Global|Local]OptimizerIndex
type and use it to define ITIVectors and for the dimension to
optimizer index mappings below.)doc";

static const char*
    __doc_operations_research_routing_Model_global_optimizer_index =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_has_hard_type_incompatibilities =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_has_same_vehicle_type_requirements =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_has_temporal_type_incompatibilities =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_has_temporal_type_requirements =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_has_vehicle_with_zero_cost_class =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_implicit_pickup_delivery_pairs_without_alternatives =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_improve_db =
    R"doc()doc";  // NOLINT

static const char*
    __doc_operations_research_routing_Model_index_neighbor_finder =  // NOLINT
    R"doc(Returns the index neighbor finder to be used by routing 
heuristics.)doc";

static const char*
    __doc_operations_research_routing_Model_index_neighbor_finder_2 =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_index_to_delivery_positions =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_index_to_disjunctions =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_index_to_equivalence_class =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_index_to_pickup_positions =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_index_to_type_policy =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_index_to_visit_type =
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_interrupt_cp =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Model_interrupt_cp_sat =
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_is_bound_to_end =
    R"doc(is_bound_to_end_[i] will be true iff the path starting at var #i is
fully bound and reaches the end of a route, i.e. either: - IsEnd(i) is
true - or nexts_[i] is bound and is_bound_to_end_[nexts_[i].Value()]
is true.)doc";

static const char*
    __doc_operations_research_routing_Model_is_bound_to_end_ct_added =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_limit = R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_linear_cost_factor_of_vehicle =
        R"doc()doc";

static const char* __doc_operations_research_routing_Model_lns_limit =
    R"doc()doc";  // NOLINT

static const char*
    __doc_operations_research_routing_Model_local_dimension_optimizers =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_local_optimizer_index =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_local_optimum_reached =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_local_search_filter_managers =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_local_search_operators =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_ls_limit =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Model_manager =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Model_max_active_vehicles =
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_metaheuristic =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Model_monitors =
    R"doc()doc";  // NOLINT

static const char*
    __doc_operations_research_routing_Model_monitors_after_setup =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_monitors_before_setup =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_nexts =
    R"doc(Decision variables: indexed by int64_t var index.)doc";

static const char* __doc_operations_research_routing_Model_no_cycle_constraint =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_node_neighbors_by_cost_class_per_size =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_nodes =
    R"doc(Sizes and indices Returns the number of nodes in the model.)doc";

static const char* __doc_operations_research_routing_Model_nodes_2 =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Model_num_vehicle_classes =
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_num_visit_types =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_objective_lower_bound =  // NOLINT
    R"doc(Returns the current lower bound found by internal solvers during the
search.)doc";

static const char*
    __doc_operations_research_routing_Model_objective_lower_bound_2 =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_operator_assign =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_optimized_dimensions_assignment_collector =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_pair_indices_of_type =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_paths_metadata =
    R"doc()doc";  // NOLINT

static const char*
    __doc_operations_research_routing_Model_pickup_delivery_disjunctions =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_pickup_delivery_pairs =  // NOLINT
    R"doc(Pickup and delivery)doc";

static const char* __doc_operations_research_routing_Model_preassignment =
    R"doc()doc";  // NOLINT

static const char*
    __doc_operations_research_routing_Model_primary_constrained_dimension =
        R"doc()doc";

static const char* __doc_operations_research_routing_Model_primary_ls_operator =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_quadratic_cost_factor_of_vehicle =
        R"doc()doc";

static const char* __doc_operations_research_routing_Model_resource_groups =
    R"doc(Resource Groups. If resource_groups_ is not empty, then for each group
of resources, each (used) vehicle must be assigned to exactly 1
resource, and each resource can in turn be assigned to at most 1
vehicle.)doc";

static const char* __doc_operations_research_routing_Model_resource_vars =
    R"doc(Resource variables, indexed first by resource group index and then by
vehicle index. A resource variable can have a negative value of -1,
iff the corresponding vehicle doesn't require a resource from this
resource group, OR if the vehicle is unused (i.e. no visits on its
route and vehicle_used_when_empty_[v] is false).)doc";

static const char* __doc_operations_research_routing_Model_restore_assignment =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_restore_tmp_assignment =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_same_vehicle_costs =
    R"doc(Same vehicle costs)doc";

static const char* __doc_operations_research_routing_Model_same_vehicle_group =
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_same_vehicle_groups =
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_search_log =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Model_search_parameters =
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_secondary_ls_db =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_secondary_ls_monitors =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_secondary_ls_operator =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_secondary_model =
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_secondary_optimizer =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_secondary_parameters =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_single_nodes_of_type =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_solve_db =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Model_solver =
    R"doc(Returns the underlying constraint solver. Can be used to add extra
constraints and/or modify search algorithms.)doc";

static const char* __doc_operations_research_routing_Model_solver_2 =
    R"doc(Model)doc";  // NOLINT

static const char* __doc_operations_research_routing_Model_start_end_count =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_state_dependent_transit_evaluators =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_state_dependent_transit_evaluators_cache =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_status =
    R"doc(Returns the current status of the routing model.)doc";

static const char* __doc_operations_research_routing_Model_status_2 =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Model_sweep_arranger =
    R"doc(Returns the sweep arranger to be used by routing heuristics.)doc";

static const char* __doc_operations_research_routing_Model_sweep_arranger_2 =
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_tabu_var_callback =
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_time_buffer =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Model_tmp_assignment =
    R"doc()doc";  // NOLINT

static const char*
    __doc_operations_research_routing_Model_topologically_sorted_visit_types =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_transit_evaluator_sign =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_transit_evaluators =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_unary_transit_evaluators =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_Model_vehicle_active =
    R"doc()doc";  // NOLINT

static const char*
    __doc_operations_research_routing_Model_vehicle_amortized_cost_factors_set =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_vehicle_class_index_of_vehicle =
        R"doc(Index by source index.)doc";

static const char*
    __doc_operations_research_routing_Model_vehicle_pickup_delivery_policy =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_vehicle_route_considered =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_vehicle_start_class_callback =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_vehicle_to_transit_cost =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_vehicle_type_container =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_Model_vehicle_used_when_empty =  // NOLINT
    R"doc(vehicle_used_when_empty_[vehicle] determines if "vehicle" should be
taken into account for costs (arc costs, span costs, etc.) and
constraints (eg. resources) even when the route of the vehicle is
empty (i.e. goes straight from its start to its end).

NOTE1: A vehicle's fixed cost is added iff the vehicle serves nodes on
its route, regardless of this variable's value.

NOTE2: The default value for this boolean is 'false' for all vehicles,
i.e. by default empty routes will not contribute to the cost nor be
considered for constraints.)doc";

static const char* __doc_operations_research_routing_Model_vehicle_vars =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_Model_vehicles =
    R"doc(Returns the number of vehicle routes in the model.)doc";

static const char* __doc_operations_research_routing_Model_vehicles_2 =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_SimpleBoundCosts =
    R"doc()doc";  // NOLINT

static const char*
    __doc_operations_research_routing_SimpleBoundCosts_SimpleBoundCosts =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_SimpleBoundCosts_SimpleBoundCosts_2 =
        R"doc()doc";

static const char* __doc_operations_research_routing_SimpleBoundCosts_Size =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_SimpleBoundCosts_bound_cost =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_SimpleBoundCosts_bound_cost_2 =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_SimpleBoundCosts_bound_costs =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_SimpleBoundCosts_operator_assign =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_SolveModelWithSat =
    R"doc(Attempts to solve the model using the cp-sat solver. As of 5/2019,
will solve the TSP corresponding to the model if it has a single
vehicle. Therefore the resulting solution might not actually be
feasible. Will return false if a solution could not be found.)doc";

static const char* __doc_operations_research_routing_SweepArranger =
    R"doc()doc";  // NOLINT

static const char* __doc_operations_research_routing_TravelBounds = R"doc()doc";

static const char* __doc_operations_research_routing_TravelBounds_max_travels =
    R"doc()doc";

static const char* __doc_operations_research_routing_TravelBounds_min_travels =
    R"doc()doc";

static const char* __doc_operations_research_routing_TravelBounds_post_travels =
    R"doc()doc";

static const char* __doc_operations_research_routing_TravelBounds_pre_travels =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeIncompatibilityChecker =  // NOLINT
    R"doc(Checker for type incompatibilities.)doc";

static const char*
    __doc_operations_research_routing_TypeIncompatibilityChecker_CheckTypeRegulations =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeIncompatibilityChecker_HasRegulationsToCheck =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeIncompatibilityChecker_TypeIncompatibilityChecker =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeIncompatibilityChecker_check_hard_incompatibilities =  // NOLINT
    R"doc(NOTE(user): As temporal incompatibilities are always verified with
this checker, we only store 1 boolean indicating whether or not hard
incompatibilities are also verified.)doc";

static const char* __doc_operations_research_routing_TypeRegulationsChecker =
    R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeRegulationsChecker_CheckTypeRegulations =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeRegulationsChecker_CheckVehicle =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeRegulationsChecker_FinalizeCheck =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeRegulationsChecker_HasRegulationsToCheck =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeRegulationsChecker_InitializeCheck =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeRegulationsChecker_OnInitializeCheck =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeRegulationsChecker_TypeCurrentlyOnRoute =  // NOLINT
    R"doc(Returns true iff there's at least one instance of the given type 
on the route when scanning the route at the given position 'pos'. This is
the case iff we have at least one added but non-removed instance of
the type, or if
occurrences_of_type_[type].last_type_on_vehicle_up_to_visit is greater
than 'pos'.)doc";

static const char*
    __doc_operations_research_routing_TypeRegulationsChecker_TypeOccursOnRoute =
        R"doc(Returns true iff any occurrence of the given type was seen on the
route, i.e. iff the added count for this type is positive, or if a
node of this type and policy TYPE_ON_VEHICLE_UP_TO_VISIT is visited on
the route (see TypePolicyOccurrence.last_type_on_vehicle_up_to_visit).)doc";

static const char*
    __doc_operations_research_routing_TypeRegulationsChecker_TypePolicyOccurrence =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeRegulationsChecker_TypePolicyOccurrence_num_type_added_to_vehicle =  // NOLINT
    R"doc(Number of TYPE_ADDED_TO_VEHICLE and
TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED node type policies seen on the
route.)doc";

static const char*
    __doc_operations_research_routing_TypeRegulationsChecker_TypePolicyOccurrence_num_type_removed_from_vehicle =  // NOLINT
    R"doc(Number of ADDED_TYPE_REMOVED_FROM_VEHICLE (effectively removing a type
from the route) and TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED node type
policies seen on the route. This number is always <=
num_type_added_to_vehicle, as a type is only actually removed if it
was on the route before.)doc";

static const char*
    __doc_operations_research_routing_TypeRegulationsChecker_TypePolicyOccurrence_position_of_last_type_on_vehicle_up_to_visit =  // NOLINT
    R"doc(Position of the last node of policy TYPE_ON_VEHICLE_UP_TO_VISIT
visited on the route. If positive, the type is considered on the
vehicle from the start of the route until this position.)doc";

static const char*
    __doc_operations_research_routing_TypeRegulationsChecker_TypeRegulationsChecker =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeRegulationsChecker_current_route_visits =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeRegulationsChecker_model =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeRegulationsChecker_occurrences_of_type =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_routing_TypeRegulationsConstraint =
    R"doc(The following constraint ensures that incompatibilities and
requirements between types are respected.

It verifies both "hard" and "temporal" incompatibilities. Two nodes
with hard incompatible types cannot be served by the same vehicle at
all, while with a temporal incompatibility they can't be on the same
route at the same time. The VisitTypePolicy of a node determines how
visiting it impacts the type count on the route.

For example, for - three temporally incompatible types T1 T2 and T3 -
2 pairs of nodes a1/r1 and a2/r2 of type T1 and T2 respectively, with
- a1 and a2 of VisitTypePolicy TYPE_ADDED_TO_VEHICLE - r1 and r2 of
policy ADDED_TYPE_REMOVED_FROM_VEHICLE - 3 nodes A, UV and AR of type
T3, respectively with type policies TYPE_ADDED_TO_VEHICLE,
TYPE_ON_VEHICLE_UP_TO_VISIT and TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED
the configurations UV --> a1 --> r1 --> a2 --> r2, a1 --> r1 --> a2
--> r2 --> A and a1 --> r1 --> AR --> a2 --> r2 are acceptable,
whereas the configurations a1 --> a2 --> r1 --> ..., or A --> a1 -->
r1 --> ..., or a1 --> r1 --> UV --> ... are not feasible.

It also verifies same-vehicle and temporal type requirements. A node
of type T_d with a same-vehicle requirement for type T_r needs to be
served by the same vehicle as a node of type T_r. Temporal
requirements, on the other hand, can take effect either when the
dependent type is being added to the route or when it's removed from
it, which is determined by the dependent node's VisitTypePolicy. In
the above example: - If T3 is required on the same vehicle as T1, A,
AR or UV must be on the same vehicle as a1. - If T2 is required when
adding T1, a2 must be visited *before* a1, and if r2 is also visited
on the route, it must be *after* a1, i.e. T2 must be on the vehicle
when a1 is visited: ... --> a2 --> ... --> a1 --> ... --> r2 --> ... -
If T3 is required when removing T1, T3 needs to be on the vehicle when
r1 is visited: ... --> A --> ... --> r1 --> ... OR ... --> r1 --> ...
--> UV --> ...)doc";

static const char*
    __doc_operations_research_routing_TypeRegulationsConstraint_CheckRegulationsOnVehicle =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeRegulationsConstraint_InitialPropagate =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeRegulationsConstraint_Post =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeRegulationsConstraint_PropagateNodeRegulations =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeRegulationsConstraint_TypeRegulationsConstraint =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeRegulationsConstraint_incompatibility_checker =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeRegulationsConstraint_model =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeRegulationsConstraint_requirement_checker =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeRegulationsConstraint_vehicle_demons =
        R"doc()doc";

static const char* __doc_operations_research_routing_TypeRequirementChecker =
    R"doc(Checker for type requirements.)doc";

static const char*
    __doc_operations_research_routing_TypeRequirementChecker_CheckRequiredTypesCurrentlyOnRoute =  // NOLINT
    R"doc(Verifies that for each set in required_type_alternatives, at least one
of the required types is on the route at position 'pos'.)doc";

static const char*
    __doc_operations_research_routing_TypeRequirementChecker_CheckTypeRegulations =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeRequirementChecker_FinalizeCheck =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeRequirementChecker_HasRegulationsToCheck =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeRequirementChecker_OnInitializeCheck =
        R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeRequirementChecker_TypeRequirementChecker =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_routing_TypeRequirementChecker_types_with_same_vehicle_requirements_on_route =  // NOLINT
    R"doc()doc";

#if defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

#endif  // ORTOOLS_ROUTING_PYTHON_DOC_H_
