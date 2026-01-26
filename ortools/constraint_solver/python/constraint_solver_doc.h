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

static const char* __doc_ABSL_DECLARE_FLAG = R"doc()doc";

static const char* __doc_File = R"doc()doc";

static const char* __doc_operations_research_Assignment =
    R"doc(An Assignment is a variable -> domains mapping, used to report
solutions to the user.)doc";

static const char* __doc_operations_research_Assignment_2 =
    R"doc(An Assignment is a variable -> domains mapping, used to report
solutions to the user.)doc";

static const char* __doc_operations_research_AssignmentContainer = R"doc()doc";

static const char* __doc_operations_research_AssignmentContainer_Add =
    R"doc()doc";

static const char* __doc_operations_research_AssignmentContainer_AddAtPosition =
    R"doc(Advanced usage: Adds element at a given position; position has to have
been allocated with AssignmentContainer::Resize() beforehand.)doc";

static const char*
    __doc_operations_research_AssignmentContainer_AreAllElementsBound =
        R"doc()doc";

static const char*
    __doc_operations_research_AssignmentContainer_AssignmentContainer =
        R"doc()doc";

static const char* __doc_operations_research_AssignmentContainer_Clear =
    R"doc()doc";

static const char* __doc_operations_research_AssignmentContainer_Contains =
    R"doc()doc";

static const char* __doc_operations_research_AssignmentContainer_Copy =
    R"doc(Copies all the elements of 'container' to this container, clearing its
previous content.)doc";

static const char*
    __doc_operations_research_AssignmentContainer_CopyIntersection =
        R"doc(Copies the elements of 'container' which are already in the calling
container.)doc";

static const char* __doc_operations_research_AssignmentContainer_Element =
    R"doc()doc";

static const char* __doc_operations_research_AssignmentContainer_Element_2 =
    R"doc()doc";

static const char*
    __doc_operations_research_AssignmentContainer_ElementPtrOrNull =
        R"doc()doc";

static const char* __doc_operations_research_AssignmentContainer_Empty =
    R"doc()doc";

static const char*
    __doc_operations_research_AssignmentContainer_EnsureMapIsUpToDate =
        R"doc()doc";

static const char* __doc_operations_research_AssignmentContainer_FastAdd =
    R"doc(Adds element without checking its presence in the container.)doc";

static const char* __doc_operations_research_AssignmentContainer_Find =
    R"doc()doc";

static const char*
    __doc_operations_research_AssignmentContainer_MutableElement = R"doc()doc";

static const char*
    __doc_operations_research_AssignmentContainer_MutableElement_2 =
        R"doc()doc";

static const char*
    __doc_operations_research_AssignmentContainer_MutableElementOrNull =
        R"doc()doc";

static const char* __doc_operations_research_AssignmentContainer_Resize =
    R"doc(Advanced usage: Resizes the container, potentially adding elements
with null variables.)doc";

static const char* __doc_operations_research_AssignmentContainer_Restore =
    R"doc()doc";

static const char* __doc_operations_research_AssignmentContainer_Size =
    R"doc()doc";

static const char* __doc_operations_research_AssignmentContainer_Store =
    R"doc()doc";

static const char* __doc_operations_research_AssignmentContainer_elements =
    R"doc()doc";

static const char* __doc_operations_research_AssignmentContainer_elements_2 =
    R"doc()doc";

static const char* __doc_operations_research_AssignmentContainer_elements_map =
    R"doc()doc";

static const char* __doc_operations_research_AssignmentContainer_operator_eq =
    R"doc(Returns true if this and 'container' both represent the same V* -> E
map. Runs in linear time; requires that the == operator on the type E
is well defined.)doc";

static const char* __doc_operations_research_AssignmentContainer_operator_ne =
    R"doc()doc";

static const char* __doc_operations_research_AssignmentElement = R"doc()doc";

static const char* __doc_operations_research_AssignmentElement_Activate =
    R"doc()doc";

static const char* __doc_operations_research_AssignmentElement_Activated =
    R"doc()doc";

static const char*
    __doc_operations_research_AssignmentElement_AssignmentElement = R"doc()doc";

static const char* __doc_operations_research_AssignmentElement_Deactivate =
    R"doc()doc";

static const char* __doc_operations_research_AssignmentElement_activated =
    R"doc()doc";

static const char* __doc_operations_research_AssignmentProto = R"doc()doc";

static const char* __doc_operations_research_Assignment_Activate = R"doc()doc";

static const char* __doc_operations_research_Assignment_Activate_2 =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_Activate_3 =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_ActivateObjective =
    R"doc()doc";

static const char*
    __doc_operations_research_Assignment_ActivateObjectiveFromIndex =
        R"doc()doc";

static const char* __doc_operations_research_Assignment_Activated = R"doc()doc";

static const char* __doc_operations_research_Assignment_Activated_2 =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_Activated_3 =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_ActivatedObjective =
    R"doc()doc";

static const char*
    __doc_operations_research_Assignment_ActivatedObjectiveFromIndex =
        R"doc()doc";

static const char* __doc_operations_research_Assignment_Add = R"doc()doc";

static const char* __doc_operations_research_Assignment_Add_2 = R"doc()doc";

static const char* __doc_operations_research_Assignment_Add_3 = R"doc()doc";

static const char* __doc_operations_research_Assignment_Add_4 = R"doc()doc";

static const char* __doc_operations_research_Assignment_Add_5 = R"doc()doc";

static const char* __doc_operations_research_Assignment_Add_6 = R"doc()doc";

static const char* __doc_operations_research_Assignment_AddObjective =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_AddObjectives =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_AreAllElementsBound =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_Assignment =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_Assignment_2 =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_Assignment_3 =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_BackwardSequence =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_Bound = R"doc()doc";

static const char* __doc_operations_research_Assignment_Clear = R"doc()doc";

static const char* __doc_operations_research_Assignment_ClearObjective =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_Contains = R"doc()doc";

static const char* __doc_operations_research_Assignment_Contains_2 =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_Contains_3 =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_Copy =
    R"doc(Copies 'assignment' to the current assignment, clearing its previous
content.)doc";

static const char* __doc_operations_research_Assignment_CopyIntersection =
    R"doc(Copies the intersection of the two assignments to the current
assignment.)doc";

static const char* __doc_operations_research_Assignment_Deactivate =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_Deactivate_2 =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_Deactivate_3 =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_DeactivateObjective =
    R"doc()doc";

static const char*
    __doc_operations_research_Assignment_DeactivateObjectiveFromIndex =
        R"doc()doc";

static const char* __doc_operations_research_Assignment_DebugString =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_DurationMax =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_DurationMin =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_DurationValue =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_Empty = R"doc()doc";

static const char* __doc_operations_research_Assignment_EndMax = R"doc()doc";

static const char* __doc_operations_research_Assignment_EndMin = R"doc()doc";

static const char* __doc_operations_research_Assignment_EndValue = R"doc()doc";

static const char* __doc_operations_research_Assignment_FastAdd =
    R"doc(Adds without checking if variable has been previously added.)doc";

static const char* __doc_operations_research_Assignment_FastAdd_2 =
    R"doc(Adds without checking if variable has been previously added.)doc";

static const char* __doc_operations_research_Assignment_FastAdd_3 =
    R"doc(Adds without checking if the variable had been previously added.)doc";

static const char* __doc_operations_research_Assignment_ForwardSequence =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_HasObjective =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_HasObjectiveFromIndex =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_IntVarContainer =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_IntervalVarContainer =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_Load =
    R"doc(Loads an assignment from a file; does not add variables to the
assignment (only the variables contained in the assignment are
modified).)doc";

static const char* __doc_operations_research_Assignment_Load_2 = R"doc()doc";

static const char* __doc_operations_research_Assignment_Load_3 =
    R"doc(#if !defined(SWIG))doc";

static const char* __doc_operations_research_Assignment_Max = R"doc()doc";

static const char* __doc_operations_research_Assignment_Min = R"doc()doc";

static const char* __doc_operations_research_Assignment_MutableIntVarContainer =
    R"doc()doc";

static const char*
    __doc_operations_research_Assignment_MutableIntervalVarContainer =
        R"doc()doc";

static const char*
    __doc_operations_research_Assignment_MutableSequenceVarContainer =
        R"doc()doc";

static const char* __doc_operations_research_Assignment_NumIntVars =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_NumIntervalVars =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_NumObjectives =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_NumSequenceVars =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_Objective = R"doc()doc";

static const char* __doc_operations_research_Assignment_ObjectiveBound =
    R"doc()doc";

static const char*
    __doc_operations_research_Assignment_ObjectiveBoundFromIndex = R"doc()doc";

static const char* __doc_operations_research_Assignment_ObjectiveFromIndex =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_ObjectiveMax =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_ObjectiveMaxFromIndex =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_ObjectiveMin =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_ObjectiveMinFromIndex =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_ObjectiveValue =
    R"doc()doc";

static const char*
    __doc_operations_research_Assignment_ObjectiveValueFromIndex = R"doc()doc";

static const char* __doc_operations_research_Assignment_PerformedMax =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_PerformedMin =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_PerformedValue =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_Restore = R"doc()doc";

static const char* __doc_operations_research_Assignment_Save =
    R"doc(Saves the assignment to a file.)doc";

static const char* __doc_operations_research_Assignment_Save_2 = R"doc()doc";

static const char* __doc_operations_research_Assignment_Save_3 = R"doc()doc";

static const char* __doc_operations_research_Assignment_SequenceVarContainer =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_SetBackwardSequence =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_SetDurationMax =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_SetDurationMin =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_SetDurationRange =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_SetDurationValue =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_SetEndMax = R"doc()doc";

static const char* __doc_operations_research_Assignment_SetEndMin = R"doc()doc";

static const char* __doc_operations_research_Assignment_SetEndRange =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_SetEndValue =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_SetForwardSequence =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_SetMax = R"doc()doc";

static const char* __doc_operations_research_Assignment_SetMin = R"doc()doc";

static const char* __doc_operations_research_Assignment_SetObjectiveMax =
    R"doc()doc";

static const char*
    __doc_operations_research_Assignment_SetObjectiveMaxFromIndex = R"doc()doc";

static const char* __doc_operations_research_Assignment_SetObjectiveMin =
    R"doc()doc";

static const char*
    __doc_operations_research_Assignment_SetObjectiveMinFromIndex = R"doc()doc";

static const char* __doc_operations_research_Assignment_SetObjectiveRange =
    R"doc()doc";

static const char*
    __doc_operations_research_Assignment_SetObjectiveRangeFromIndex =
        R"doc()doc";

static const char* __doc_operations_research_Assignment_SetObjectiveValue =
    R"doc()doc";

static const char*
    __doc_operations_research_Assignment_SetObjectiveValueFromIndex =
        R"doc()doc";

static const char* __doc_operations_research_Assignment_SetPerformedMax =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_SetPerformedMin =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_SetPerformedRange =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_SetPerformedValue =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_SetRange = R"doc()doc";

static const char* __doc_operations_research_Assignment_SetSequence =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_SetStartMax =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_SetStartMin =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_SetStartRange =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_SetStartValue =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_SetUnperformed =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_SetValue = R"doc()doc";

static const char* __doc_operations_research_Assignment_Size = R"doc()doc";

static const char* __doc_operations_research_Assignment_StartMax = R"doc()doc";

static const char* __doc_operations_research_Assignment_StartMin = R"doc()doc";

static const char* __doc_operations_research_Assignment_StartValue =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_Store = R"doc()doc";

static const char* __doc_operations_research_Assignment_Unperformed =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_Value = R"doc()doc";

static const char* __doc_operations_research_Assignment_int_var_container =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_interval_var_container =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_objective_elements =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_operator_assign =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_operator_eq =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_operator_ne =
    R"doc()doc";

static const char* __doc_operations_research_Assignment_sequence_var_container =
    R"doc()doc";

static const char* __doc_operations_research_BaseObject =
    R"doc(A BaseObject is the root of all reversibly allocated objects. A
DebugString method and the associated << operator are implemented as a
convenience.)doc";

static const char* __doc_operations_research_BaseObject_2 =
    R"doc(A BaseObject is the root of all reversibly allocated objects. A
DebugString method and the associated << operator are implemented as a
convenience.)doc";

static const char* __doc_operations_research_BaseObject_BaseObject =
    R"doc()doc";

static const char* __doc_operations_research_BaseObject_BaseObject_2 =
    R"doc()doc";

static const char* __doc_operations_research_BaseObject_DebugString =
    R"doc()doc";

static const char* __doc_operations_research_BaseObject_operator_assign =
    R"doc()doc";

static const char* __doc_operations_research_CastConstraint =
    R"doc(Cast constraints are special channeling constraints designed to keep a
variable in sync with an expression. They are created internally when
Var() is called on a subclass of IntExpr.)doc";

static const char* __doc_operations_research_CastConstraint_2 =
    R"doc(Cast constraints are special channeling constraints designed to keep a
variable in sync with an expression. They are created internally when
Var() is called on a subclass of IntExpr.)doc";

static const char* __doc_operations_research_CastConstraint_CastConstraint =
    R"doc()doc";

static const char* __doc_operations_research_CastConstraint_target_var =
    R"doc()doc";

static const char* __doc_operations_research_CastConstraint_target_var_2 =
    R"doc()doc";

static const char* __doc_operations_research_ClockTimer = R"doc()doc";

static const char* __doc_operations_research_Constraint =
    R"doc(A constraint is the main modeling object. It provides two methods: -
Post() is responsible for creating the demons and attaching them to
immediate demons(). - InitialPropagate() is called once just after
Post and performs the initial propagation. The subsequent propagations
will be performed by the demons Posted during the post() method.)doc";

static const char* __doc_operations_research_Constraint_2 =
    R"doc(A constraint is the main modeling object. It provides two methods: -
Post() is responsible for creating the demons and attaching them to
immediate demons(). - InitialPropagate() is called once just after
Post and performs the initial propagation. The subsequent propagations
will be performed by the demons Posted during the post() method.)doc";

static const char* __doc_operations_research_Constraint_Accept =
    R"doc(Accepts the given visitor.)doc";

static const char* __doc_operations_research_Constraint_Constraint =
    R"doc()doc";

static const char* __doc_operations_research_Constraint_Constraint_2 =
    R"doc()doc";

static const char* __doc_operations_research_Constraint_DebugString =
    R"doc()doc";

static const char* __doc_operations_research_Constraint_InitialPropagate =
    R"doc(This method performs the initial propagation of the constraint. It is
called just after the post.)doc";

static const char* __doc_operations_research_Constraint_IsCastConstraint =
    R"doc(Is the constraint created by a cast from expression to integer
variable?)doc";

static const char* __doc_operations_research_Constraint_Post =
    R"doc(This method is called when the constraint is processed by the solver.
Its main usage is to attach demons to variables.)doc";

static const char* __doc_operations_research_Constraint_PostAndPropagate =
    R"doc(Calls Post and then Propagate to initialize the constraints. This is
usually done in the root node.)doc";

static const char* __doc_operations_research_Constraint_Var =
    R"doc(Creates a Boolean variable representing the status of the constraint
(false = constraint is violated, true = constraint is satisfied). It
returns nullptr if the constraint does not support this API.)doc";

static const char* __doc_operations_research_Constraint_operator_assign =
    R"doc()doc";

static const char* __doc_operations_research_CpRandomSeed = R"doc()doc";

static const char* __doc_operations_research_Decision =
    R"doc(A Decision represents a choice point in the search tree. The two main
methods are Apply() to go left, or Refute() to go right.)doc";

static const char* __doc_operations_research_Decision_2 =
    R"doc(A Decision represents a choice point in the search tree. The two main
methods are Apply() to go left, or Refute() to go right.)doc";

static const char* __doc_operations_research_DecisionBuilder =
    R"doc(A DecisionBuilder is responsible for creating the search tree. The
important method is Next(), which returns the next decision to
execute.)doc";

static const char* __doc_operations_research_DecisionBuilder_2 =
    R"doc(A DecisionBuilder is responsible for creating the search tree. The
important method is Next(), which returns the next decision to
execute.)doc";

static const char* __doc_operations_research_DecisionBuilder_Accept =
    R"doc()doc";

static const char* __doc_operations_research_DecisionBuilder_AppendMonitors =
    R"doc(This method will be called at the start of the search. It asks the
decision builder if it wants to append search monitors to the list of
active monitors for this search. Please note there are no checks at
this point for duplication.)doc";

static const char* __doc_operations_research_DecisionBuilder_DebugString =
    R"doc()doc";

static const char* __doc_operations_research_DecisionBuilder_DecisionBuilder =
    R"doc()doc";

static const char* __doc_operations_research_DecisionBuilder_DecisionBuilder_2 =
    R"doc()doc";

static const char* __doc_operations_research_DecisionBuilder_GetName =
    R"doc()doc";

static const char* __doc_operations_research_DecisionBuilder_Next =
    R"doc(This is the main method of the decision builder class. It must return
a decision (an instance of the class Decision). If it returns nullptr,
this means that the decision builder has finished its work.)doc";

static const char* __doc_operations_research_DecisionBuilder_name = R"doc()doc";

static const char* __doc_operations_research_DecisionBuilder_operator_assign =
    R"doc()doc";

static const char* __doc_operations_research_DecisionBuilder_set_name =
    R"doc()doc";

static const char* __doc_operations_research_DecisionVisitor =
    R"doc(A DecisionVisitor is used to inspect a decision. It contains virtual
methods for all type of 'declared' decisions.)doc";

static const char* __doc_operations_research_DecisionVisitor_2 =
    R"doc(A DecisionVisitor is used to inspect a decision. It contains virtual
methods for all type of 'declared' decisions.)doc";

static const char* __doc_operations_research_DecisionVisitor_DecisionVisitor =
    R"doc()doc";

static const char* __doc_operations_research_DecisionVisitor_DecisionVisitor_2 =
    R"doc()doc";

static const char*
    __doc_operations_research_DecisionVisitor_VisitRankFirstInterval =
        R"doc()doc";

static const char*
    __doc_operations_research_DecisionVisitor_VisitRankLastInterval =
        R"doc()doc";

static const char*
    __doc_operations_research_DecisionVisitor_VisitScheduleOrExpedite =
        R"doc()doc";

static const char*
    __doc_operations_research_DecisionVisitor_VisitScheduleOrPostpone =
        R"doc()doc";

static const char*
    __doc_operations_research_DecisionVisitor_VisitSetVariableValue =
        R"doc()doc";

static const char*
    __doc_operations_research_DecisionVisitor_VisitSplitVariableDomain =
        R"doc()doc";

static const char*
    __doc_operations_research_DecisionVisitor_VisitUnknownDecision =
        R"doc()doc";

static const char* __doc_operations_research_DecisionVisitor_operator_assign =
    R"doc()doc";

static const char* __doc_operations_research_Decision_Accept =
    R"doc(Accepts the given visitor.)doc";

static const char* __doc_operations_research_Decision_Apply =
    R"doc(Apply will be called first when the decision is executed.)doc";

static const char* __doc_operations_research_Decision_DebugString = R"doc()doc";

static const char* __doc_operations_research_Decision_Decision = R"doc()doc";

static const char* __doc_operations_research_Decision_Decision_2 = R"doc()doc";

static const char* __doc_operations_research_Decision_Refute =
    R"doc(Refute will be called after a backtrack.)doc";

static const char* __doc_operations_research_Decision_operator_assign =
    R"doc()doc";

static const char* __doc_operations_research_DefaultPhaseParameters =
    R"doc(This struct holds all parameters for the default search.
DefaultPhaseParameters is only used by Solver::MakeDefaultPhase
methods. Note this is for advanced users only.)doc";

static const char*
    __doc_operations_research_DefaultPhaseParameters_DefaultPhaseParameters =
        R"doc()doc";

static const char*
    __doc_operations_research_DefaultPhaseParameters_DisplayLevel = R"doc()doc";

static const char*
    __doc_operations_research_DefaultPhaseParameters_DisplayLevel_NONE =
        R"doc()doc";

static const char*
    __doc_operations_research_DefaultPhaseParameters_DisplayLevel_NORMAL =
        R"doc()doc";

static const char*
    __doc_operations_research_DefaultPhaseParameters_DisplayLevel_VERBOSE =
        R"doc()doc";

static const char*
    __doc_operations_research_DefaultPhaseParameters_ValueSelection =
        R"doc()doc";

static const char*
    __doc_operations_research_DefaultPhaseParameters_ValueSelection_SELECT_MAX_IMPACT =
        R"doc()doc";

static const char*
    __doc_operations_research_DefaultPhaseParameters_ValueSelection_SELECT_MIN_IMPACT =
        R"doc()doc";

static const char*
    __doc_operations_research_DefaultPhaseParameters_VariableSelection =
        R"doc()doc";

static const char*
    __doc_operations_research_DefaultPhaseParameters_VariableSelection_CHOOSE_MAX_AVERAGE_IMPACT =
        R"doc()doc";

static const char*
    __doc_operations_research_DefaultPhaseParameters_VariableSelection_CHOOSE_MAX_SUM_IMPACT =
        R"doc()doc";

static const char*
    __doc_operations_research_DefaultPhaseParameters_VariableSelection_CHOOSE_MAX_VALUE_IMPACT =
        R"doc()doc";

static const char*
    __doc_operations_research_DefaultPhaseParameters_decision_builder =
        R"doc(When defined, this overrides the default impact based decision
builder.)doc";

static const char*
    __doc_operations_research_DefaultPhaseParameters_display_level =
        R"doc(This represents the amount of information displayed by the default
search. NONE means no display, VERBOSE means extra information.)doc";

static const char*
    __doc_operations_research_DefaultPhaseParameters_heuristic_num_failures_limit =
        R"doc(The failure limit for each heuristic that we run.)doc";

static const char*
    __doc_operations_research_DefaultPhaseParameters_heuristic_period =
        R"doc(The distance in nodes between each run of the heuristics. A negative
or null value will mean that we will not run heuristics at all.)doc";

static const char*
    __doc_operations_research_DefaultPhaseParameters_initialization_splits =
        R"doc(Maximum number of intervals that the initialization of impacts will
scan per variable.)doc";

static const char*
    __doc_operations_research_DefaultPhaseParameters_persistent_impact =
        R"doc(Whether to keep the impact from the first search for other searches,
or to recompute the impact for each new search.)doc";

static const char*
    __doc_operations_research_DefaultPhaseParameters_random_seed =
        R"doc(Seed used to initialize the random part in some heuristics.)doc";

static const char*
    __doc_operations_research_DefaultPhaseParameters_run_all_heuristics =
        R"doc(The default phase will run heuristics periodically. This parameter
indicates if we should run all heuristics, or a randomly selected one.)doc";

static const char*
    __doc_operations_research_DefaultPhaseParameters_use_last_conflict =
        R"doc(Should we use last conflict method. The default is false.)doc";

static const char*
    __doc_operations_research_DefaultPhaseParameters_value_selection_schema =
        R"doc(This parameter describes which value to select for a given var.)doc";

static const char*
    __doc_operations_research_DefaultPhaseParameters_var_selection_schema =
        R"doc(This parameter describes how the next variable to instantiate will be
chosen.)doc";

static const char* __doc_operations_research_Demon =
    R"doc(A Demon is the base element of a propagation queue. It is the main
object responsible for implementing the actual propagation of the
constraint and pruning the inconsistent values in the domains of the
variables. The main concept is that demons are listeners that are
attached to the variables and listen to their modifications. There are
two methods: - Run() is the actual method called when the demon is
processed. - priority() returns its priority. Standard priorities are
slow, normal or fast. "immediate" is reserved for variables and is
treated separately.)doc";

static const char* __doc_operations_research_Demon_2 =
    R"doc(A Demon is the base element of a propagation queue. It is the main
object responsible for implementing the actual propagation of the
constraint and pruning the inconsistent values in the domains of the
variables. The main concept is that demons are listeners that are
attached to the variables and listen to their modifications. There are
two methods: - Run() is the actual method called when the demon is
processed. - priority() returns its priority. Standard priorities are
slow, normal or fast. "immediate" is reserved for variables and is
treated separately.)doc";

static const char* __doc_operations_research_DemonProfiler = R"doc()doc";

static const char* __doc_operations_research_Demon_DebugString = R"doc()doc";

static const char* __doc_operations_research_Demon_Demon =
    R"doc(This indicates the priority of a demon. Immediate demons are treated
separately and corresponds to variables.)doc";

static const char* __doc_operations_research_Demon_Demon_2 = R"doc()doc";

static const char* __doc_operations_research_Demon_Run =
    R"doc(This is the main callback of the demon.)doc";

static const char* __doc_operations_research_Demon_desinhibit =
    R"doc(This method un-inhibits the demon that was previously inhibited.)doc";

static const char* __doc_operations_research_Demon_inhibit =
    R"doc(This method inhibits the demon in the search tree below the current
position.)doc";

static const char* __doc_operations_research_Demon_operator_assign =
    R"doc()doc";

static const char* __doc_operations_research_Demon_priority =
    R"doc(This method returns the priority of the demon. Usually a demon is
fast, slow or normal. Immediate demons are reserved for internal use
to maintain variables.)doc";

static const char* __doc_operations_research_Demon_set_stamp = R"doc()doc";

static const char* __doc_operations_research_Demon_stamp = R"doc()doc";

static const char* __doc_operations_research_Demon_stamp_2 = R"doc()doc";

static const char* __doc_operations_research_Dimension = R"doc()doc";

static const char* __doc_operations_research_DisjunctiveConstraint =
    R"doc()doc";

static const char* __doc_operations_research_DisjunctiveConstraint_2 =
    R"doc()doc";

static const char*
    __doc_operations_research_DisjunctiveConstraint_DisjunctiveConstraint =
        R"doc()doc";

static const char*
    __doc_operations_research_DisjunctiveConstraint_DisjunctiveConstraint_2 =
        R"doc()doc";

static const char*
    __doc_operations_research_DisjunctiveConstraint_MakeSequenceVar =
        R"doc(Creates a sequence variable from the constraint.)doc";

static const char*
    __doc_operations_research_DisjunctiveConstraint_SetTransitionTime =
        R"doc(Add a transition time between intervals. It forces the distance
between the end of interval a and start of interval b that follows it
to be at least transition_time(a, b). This function must always return
a positive or null value.)doc";

static const char*
    __doc_operations_research_DisjunctiveConstraint_TransitionTime =
        R"doc()doc";

static const char* __doc_operations_research_DisjunctiveConstraint_actives =
    R"doc()doc";

static const char* __doc_operations_research_DisjunctiveConstraint_intervals =
    R"doc()doc";

static const char* __doc_operations_research_DisjunctiveConstraint_nexts =
    R"doc()doc";

static const char*
    __doc_operations_research_DisjunctiveConstraint_operator_assign =
        R"doc()doc";

static const char* __doc_operations_research_DisjunctiveConstraint_time_cumuls =
    R"doc()doc";

static const char* __doc_operations_research_DisjunctiveConstraint_time_slacks =
    R"doc()doc";

static const char*
    __doc_operations_research_DisjunctiveConstraint_transition_time =
        R"doc()doc";

static const char* __doc_operations_research_ImprovementSearchLimit =
    R"doc()doc";

static const char* __doc_operations_research_ImprovementSearchLimit_2 =
    R"doc()doc";

static const char* __doc_operations_research_ImprovementSearchLimit_AtSolution =
    R"doc()doc";

static const char*
    __doc_operations_research_ImprovementSearchLimit_CheckWithOffset =
        R"doc()doc";

static const char* __doc_operations_research_ImprovementSearchLimit_Copy =
    R"doc()doc";

static const char*
    __doc_operations_research_ImprovementSearchLimit_ImprovementSearchLimit =
        R"doc()doc";

static const char*
    __doc_operations_research_ImprovementSearchLimit_ImprovementSearchLimit_2 =
        R"doc()doc";

static const char* __doc_operations_research_ImprovementSearchLimit_Init =
    R"doc()doc";

static const char* __doc_operations_research_ImprovementSearchLimit_Install =
    R"doc()doc";

static const char* __doc_operations_research_ImprovementSearchLimit_MakeClone =
    R"doc()doc";

static const char*
    __doc_operations_research_ImprovementSearchLimit_best_objectives =
        R"doc()doc";

static const char*
    __doc_operations_research_ImprovementSearchLimit_gradient_stage =
        R"doc()doc";

static const char*
    __doc_operations_research_ImprovementSearchLimit_improvement_rate_coefficient =
        R"doc()doc";

static const char*
    __doc_operations_research_ImprovementSearchLimit_improvement_rate_solutions_distance =
        R"doc()doc";

static const char*
    __doc_operations_research_ImprovementSearchLimit_improvements = R"doc()doc";

static const char* __doc_operations_research_ImprovementSearchLimit_maximize =
    R"doc()doc";

static const char*
    __doc_operations_research_ImprovementSearchLimit_objective_offsets =
        R"doc()doc";

static const char*
    __doc_operations_research_ImprovementSearchLimit_objective_scaling_factors =
        R"doc()doc";

static const char*
    __doc_operations_research_ImprovementSearchLimit_objective_updated =
        R"doc()doc";

static const char*
    __doc_operations_research_ImprovementSearchLimit_objective_vars =
        R"doc()doc";

static const char* __doc_operations_research_ImprovementSearchLimit_thresholds =
    R"doc()doc";

static const char* __doc_operations_research_InitAndGetValues =
    R"doc(Utility class to encapsulate an IntVarIterator and use it in a range-
based loop. See the code snippet above IntVarIterator.

It contains DEBUG_MODE-enabled code that DCHECKs that the same
iterator instance isn't being iterated on in multiple places
simultaneously.)doc";

static const char* __doc_operations_research_InitAndGetValues_InitAndGetValues =
    R"doc()doc";

static const char* __doc_operations_research_InitAndGetValues_Iterator =
    R"doc()doc";

static const char* __doc_operations_research_InitAndGetValues_Iterator_2 =
    R"doc()doc";

static const char* __doc_operations_research_InitAndGetValues_Iterator_Begin =
    R"doc(These are the only way to construct an Iterator.)doc";

static const char* __doc_operations_research_InitAndGetValues_Iterator_End =
    R"doc()doc";

static const char*
    __doc_operations_research_InitAndGetValues_Iterator_Iterator = R"doc()doc";

static const char* __doc_operations_research_InitAndGetValues_Iterator_is_end =
    R"doc()doc";

static const char* __doc_operations_research_InitAndGetValues_Iterator_it =
    R"doc()doc";

static const char*
    __doc_operations_research_InitAndGetValues_Iterator_operator_inc =
        R"doc()doc";

static const char*
    __doc_operations_research_InitAndGetValues_Iterator_operator_mul =
        R"doc()doc";

static const char*
    __doc_operations_research_InitAndGetValues_Iterator_operator_ne =
        R"doc()doc";

static const char* __doc_operations_research_InitAndGetValues_begin =
    R"doc()doc";

static const char* __doc_operations_research_InitAndGetValues_begin_was_called =
    R"doc()doc";

static const char* __doc_operations_research_InitAndGetValues_end = R"doc()doc";

static const char* __doc_operations_research_InitAndGetValues_it = R"doc()doc";

static const char* __doc_operations_research_IntExpr =
    R"doc(The class IntExpr is the base of all integer expressions in constraint
programming. It contains the basic protocol for an expression: -
setting and modifying its bound - querying if it is bound - listening
to events modifying its bounds - casting it into a variable (instance
of IntVar))doc";

static const char* __doc_operations_research_IntExpr_2 =
    R"doc(The class IntExpr is the base of all integer expressions in constraint
programming. It contains the basic protocol for an expression: -
setting and modifying its bound - querying if it is bound - listening
to events modifying its bounds - casting it into a variable (instance
of IntVar))doc";

static const char* __doc_operations_research_IntExpr_Accept =
    R"doc(Accepts the given visitor.)doc";

static const char* __doc_operations_research_IntExpr_Bound =
    R"doc(Returns true if the min and the max of the expression are equal.)doc";

static const char* __doc_operations_research_IntExpr_IntExpr = R"doc()doc";

static const char* __doc_operations_research_IntExpr_IntExpr_2 = R"doc()doc";

static const char* __doc_operations_research_IntExpr_IsVar =
    R"doc(Returns true if the expression is indeed a variable.)doc";

static const char* __doc_operations_research_IntExpr_Max = R"doc()doc";

static const char* __doc_operations_research_IntExpr_Min = R"doc()doc";

static const char* __doc_operations_research_IntExpr_Range =
    R"doc(By default calls Min() and Max(), but can be redefined when Min and
Max code can be factorized.)doc";

static const char* __doc_operations_research_IntExpr_SetMax = R"doc()doc";

static const char* __doc_operations_research_IntExpr_SetMin = R"doc()doc";

static const char* __doc_operations_research_IntExpr_SetRange =
    R"doc(This method sets both the min and the max of the expression.)doc";

static const char* __doc_operations_research_IntExpr_SetValue =
    R"doc(This method sets the value of the expression.)doc";

static const char* __doc_operations_research_IntExpr_Var =
    R"doc(Creates a variable from the expression.)doc";

static const char* __doc_operations_research_IntExpr_VarWithName =
    R"doc(Creates a variable from the expression and set the name of the
resulting var. If the expression is already a variable, then it will
set the name of the expression, possibly overwriting it. This is just
a shortcut to Var() followed by set_name().)doc";

static const char* __doc_operations_research_IntExpr_WhenRange =
    R"doc(Attach a demon that will watch the min or the max of the expression.)doc";

static const char* __doc_operations_research_IntExpr_WhenRange_2 =
    R"doc(Attach a demon that will watch the min or the max of the expression.)doc";

static const char* __doc_operations_research_IntExpr_WhenRange_3 =
    R"doc(Attach a demon that will watch the min or the max of the expression.)doc";

static const char* __doc_operations_research_IntExpr_operator_assign =
    R"doc()doc";

static const char* __doc_operations_research_IntVar =
    R"doc(The class IntVar is a subset of IntExpr. In addition to the IntExpr
protocol, it offers persistence, removing values from the domains, and
a finer model for events.)doc";

static const char* __doc_operations_research_IntVar_2 =
    R"doc(The class IntVar is a subset of IntExpr. In addition to the IntExpr
protocol, it offers persistence, removing values from the domains, and
a finer model for events.)doc";

static const char* __doc_operations_research_IntVarAssignment = R"doc()doc";

static const char* __doc_operations_research_IntVarElement = R"doc()doc";

static const char* __doc_operations_research_IntVarElement_Bound = R"doc()doc";

static const char* __doc_operations_research_IntVarElement_Clone = R"doc()doc";

static const char* __doc_operations_research_IntVarElement_Copy = R"doc()doc";

static const char* __doc_operations_research_IntVarElement_DebugString =
    R"doc()doc";

static const char* __doc_operations_research_IntVarElement_IntVarElement =
    R"doc()doc";

static const char* __doc_operations_research_IntVarElement_IntVarElement_2 =
    R"doc()doc";

static const char* __doc_operations_research_IntVarElement_LoadFromProto =
    R"doc()doc";

static const char* __doc_operations_research_IntVarElement_Max = R"doc()doc";

static const char* __doc_operations_research_IntVarElement_Min = R"doc()doc";

static const char* __doc_operations_research_IntVarElement_Reset = R"doc()doc";

static const char* __doc_operations_research_IntVarElement_Restore =
    R"doc()doc";

static const char* __doc_operations_research_IntVarElement_SetMax = R"doc()doc";

static const char* __doc_operations_research_IntVarElement_SetMin = R"doc()doc";

static const char* __doc_operations_research_IntVarElement_SetRange =
    R"doc()doc";

static const char* __doc_operations_research_IntVarElement_SetValue =
    R"doc()doc";

static const char* __doc_operations_research_IntVarElement_Store = R"doc()doc";

static const char* __doc_operations_research_IntVarElement_Value = R"doc()doc";

static const char* __doc_operations_research_IntVarElement_Var = R"doc()doc";

static const char* __doc_operations_research_IntVarElement_WriteToProto =
    R"doc()doc";

static const char* __doc_operations_research_IntVarElement_max = R"doc()doc";

static const char* __doc_operations_research_IntVarElement_min = R"doc()doc";

static const char* __doc_operations_research_IntVarElement_operator_eq =
    R"doc()doc";

static const char* __doc_operations_research_IntVarElement_operator_ne =
    R"doc()doc";

static const char* __doc_operations_research_IntVarElement_var = R"doc()doc";

static const char* __doc_operations_research_IntVarIterator =
    R"doc(IntVar* current_var; std::unique_ptr<IntVarIterator>
it(current_var->MakeHoleIterator(false)); for (const int64_t hole :
InitAndGetValues(it)) { /// use the hole })doc";

static const char* __doc_operations_research_IntVarIterator_DebugString =
    R"doc(Pretty Print.)doc";

static const char* __doc_operations_research_IntVarIterator_Init =
    R"doc(This method must be called before each loop.)doc";

static const char* __doc_operations_research_IntVarIterator_Next =
    R"doc(This method moves the iterator to the next value.)doc";

static const char* __doc_operations_research_IntVarIterator_Ok =
    R"doc(This method indicates if we can call Value() or not.)doc";

static const char* __doc_operations_research_IntVarIterator_Value =
    R"doc(This method returns the current value of the iterator.)doc";

static const char* __doc_operations_research_IntVarLocalSearchFilter =
    R"doc()doc";

static const char* __doc_operations_research_IntVar_Accept =
    R"doc(Accepts the given visitor.)doc";

static const char* __doc_operations_research_IntVar_Contains =
    R"doc(This method returns whether the value 'v' is in the domain of the
variable.)doc";

static const char* __doc_operations_research_IntVar_IntVar = R"doc()doc";

static const char* __doc_operations_research_IntVar_IntVar_2 = R"doc()doc";

static const char* __doc_operations_research_IntVar_IntVar_3 = R"doc()doc";

static const char* __doc_operations_research_IntVar_IsDifferent = R"doc()doc";

static const char* __doc_operations_research_IntVar_IsEqual =
    R"doc(IsEqual)doc";

static const char* __doc_operations_research_IntVar_IsGreaterOrEqual =
    R"doc()doc";

static const char* __doc_operations_research_IntVar_IsLessOrEqual = R"doc()doc";

static const char* __doc_operations_research_IntVar_IsVar = R"doc()doc";

static const char* __doc_operations_research_IntVar_MakeDomainIterator =
    R"doc(Creates a domain iterator. When 'reversible' is false, the returned
object is created on the normal C++ heap and the solver does NOT take
ownership of the object.)doc";

static const char* __doc_operations_research_IntVar_MakeHoleIterator =
    R"doc(Creates a hole iterator. When 'reversible' is false, the returned
object is created on the normal C++ heap and the solver does NOT take
ownership of the object.)doc";

static const char* __doc_operations_research_IntVar_OldMax =
    R"doc(Returns the previous max.)doc";

static const char* __doc_operations_research_IntVar_OldMin =
    R"doc(Returns the previous min.)doc";

static const char* __doc_operations_research_IntVar_RemoveInterval =
    R"doc(This method removes the interval 'l' .. 'u' from the domain of the
variable. It assumes that 'l' <= 'u'.)doc";

static const char* __doc_operations_research_IntVar_RemoveValue =
    R"doc(This method removes the value 'v' from the domain of the variable.)doc";

static const char* __doc_operations_research_IntVar_RemoveValues =
    R"doc(This method remove the values from the domain of the variable.)doc";

static const char* __doc_operations_research_IntVar_SetValues =
    R"doc(This method intersects the current domain with the values in the
array.)doc";

static const char* __doc_operations_research_IntVar_Size =
    R"doc(This method returns the number of values in the domain of the
variable.)doc";

static const char* __doc_operations_research_IntVar_Value =
    R"doc(This method returns the value of the variable. This method checks
before that the variable is bound.)doc";

static const char* __doc_operations_research_IntVar_Var = R"doc()doc";

static const char* __doc_operations_research_IntVar_VarType = R"doc()doc";

static const char* __doc_operations_research_IntVar_WhenBound =
    R"doc(This method attaches a demon that will be awakened when the variable
is bound.)doc";

static const char* __doc_operations_research_IntVar_WhenBound_2 =
    R"doc(This method attaches a closure that will be awakened when the variable
is bound.)doc";

static const char* __doc_operations_research_IntVar_WhenBound_3 =
    R"doc(This method attaches an action that will be awakened when the variable
is bound.)doc";

static const char* __doc_operations_research_IntVar_WhenDomain =
    R"doc(This method attaches a demon that will watch any domain modification
of the domain of the variable.)doc";

static const char* __doc_operations_research_IntVar_WhenDomain_2 =
    R"doc(This method attaches a closure that will watch any domain modification
of the domain of the variable.)doc";

static const char* __doc_operations_research_IntVar_WhenDomain_3 =
    R"doc(This method attaches an action that will watch any domain modification
of the domain of the variable.)doc";

static const char* __doc_operations_research_IntVar_index =
    R"doc(Returns the index of the variable.)doc";

static const char* __doc_operations_research_IntVar_index_2 = R"doc()doc";

static const char* __doc_operations_research_IntVar_operator_assign =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar =
    R"doc(Interval variables are often used in scheduling. The main
characteristics of an IntervalVar are the start position, duration,
and end date. All these characteristics can be queried and set, and
demons can be posted on their modifications.

An important aspect is optionality: an IntervalVar can be performed or
not. If unperformed, then it simply does not exist, and its
characteristics cannot be accessed any more. An interval var is
automatically marked as unperformed when it is not consistent anymore
(start greater than end, duration < 0...))doc";

static const char* __doc_operations_research_IntervalVar_2 =
    R"doc(Interval variables are often used in scheduling. The main
characteristics of an IntervalVar are the start position, duration,
and end date. All these characteristics can be queried and set, and
demons can be posted on their modifications.

An important aspect is optionality: an IntervalVar can be performed or
not. If unperformed, then it simply does not exist, and its
characteristics cannot be accessed any more. An interval var is
automatically marked as unperformed when it is not consistent anymore
(start greater than end, duration < 0...))doc";

static const char* __doc_operations_research_IntervalVarAssignment =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement = R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_Bound =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_Clone =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_Copy =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_DebugString =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_DurationMax =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_DurationMin =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_DurationValue =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_EndMax =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_EndMin =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_EndValue =
    R"doc()doc";

static const char*
    __doc_operations_research_IntervalVarElement_IntervalVarElement =
        R"doc()doc";

static const char*
    __doc_operations_research_IntervalVarElement_IntervalVarElement_2 =
        R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_LoadFromProto =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_PerformedMax =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_PerformedMin =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_PerformedValue =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_Reset =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_Restore =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_SetDurationMax =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_SetDurationMin =
    R"doc()doc";

static const char*
    __doc_operations_research_IntervalVarElement_SetDurationRange = R"doc()doc";

static const char*
    __doc_operations_research_IntervalVarElement_SetDurationValue = R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_SetEndMax =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_SetEndMin =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_SetEndRange =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_SetEndValue =
    R"doc()doc";

static const char*
    __doc_operations_research_IntervalVarElement_SetPerformedMax = R"doc()doc";

static const char*
    __doc_operations_research_IntervalVarElement_SetPerformedMin = R"doc()doc";

static const char*
    __doc_operations_research_IntervalVarElement_SetPerformedRange =
        R"doc()doc";

static const char*
    __doc_operations_research_IntervalVarElement_SetPerformedValue =
        R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_SetStartMax =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_SetStartMin =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_SetStartRange =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_SetStartValue =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_StartMax =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_StartMin =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_StartValue =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_Store =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_Var =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_WriteToProto =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_duration_max =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_duration_min =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_end_max =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_end_min =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_operator_eq =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_operator_ne =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_performed_max =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_performed_min =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_start_max =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_start_min =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVarElement_var =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_Accept =
    R"doc(Accepts the given visitor.)doc";

static const char* __doc_operations_research_IntervalVar_CannotBePerformed =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_DurationExpr =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_DurationMax =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_DurationMin =
    R"doc(These methods query, set, and watch the duration of the interval var.)doc";

static const char* __doc_operations_research_IntervalVar_EndExpr = R"doc()doc";

static const char* __doc_operations_research_IntervalVar_EndMax = R"doc()doc";

static const char* __doc_operations_research_IntervalVar_EndMin =
    R"doc(These methods query, set, and watch the end position of the interval
var.)doc";

static const char* __doc_operations_research_IntervalVar_IntervalVar =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_IntervalVar_2 =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_IsPerformedBound =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_MayBePerformed =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_MustBePerformed =
    R"doc(These methods query, set, and watch the performed status of the
interval var.)doc";

static const char* __doc_operations_research_IntervalVar_OldDurationMax =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_OldDurationMin =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_OldEndMax =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_OldEndMin =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_OldStartMax =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_OldStartMin =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_PerformedExpr =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_SafeDurationExpr =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_SafeEndExpr =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_SafeStartExpr =
    R"doc(These methods create expressions encapsulating the start, end and
duration of the interval var. If the interval var is unperformed, they
will return the unperformed_value.)doc";

static const char* __doc_operations_research_IntervalVar_SetDurationMax =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_SetDurationMin =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_SetDurationRange =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_SetEndMax =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_SetEndMin =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_SetEndRange =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_SetPerformed =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_SetStartMax =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_SetStartMin =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_SetStartRange =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_StartExpr =
    R"doc(These methods create expressions encapsulating the start, end and
duration of the interval var. Please note that these must not be used
if the interval var is unperformed.)doc";

static const char* __doc_operations_research_IntervalVar_StartMax = R"doc()doc";

static const char* __doc_operations_research_IntervalVar_StartMin =
    R"doc(These methods query, set, and watch the start position of the interval
var.)doc";

static const char* __doc_operations_research_IntervalVar_WasPerformedBound =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_WhenAnything =
    R"doc(Attaches a demon awakened when anything about this interval changes.)doc";

static const char* __doc_operations_research_IntervalVar_WhenAnything_2 =
    R"doc(Attaches a closure awakened when anything about this interval changes.)doc";

static const char* __doc_operations_research_IntervalVar_WhenAnything_3 =
    R"doc(Attaches an action awakened when anything about this interval changes.)doc";

static const char* __doc_operations_research_IntervalVar_WhenDurationBound =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_WhenDurationBound_2 =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_WhenDurationBound_3 =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_WhenDurationRange =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_WhenDurationRange_2 =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_WhenDurationRange_3 =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_WhenEndBound =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_WhenEndBound_2 =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_WhenEndBound_3 =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_WhenEndRange =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_WhenEndRange_2 =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_WhenEndRange_3 =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_WhenPerformedBound =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_WhenPerformedBound_2 =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_WhenPerformedBound_3 =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_WhenStartBound =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_WhenStartBound_2 =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_WhenStartBound_3 =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_WhenStartRange =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_WhenStartRange_2 =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_WhenStartRange_3 =
    R"doc()doc";

static const char* __doc_operations_research_IntervalVar_operator_assign =
    R"doc()doc";

static const char* __doc_operations_research_LightIntFunctionElementCt =
    R"doc()doc";

static const char* __doc_operations_research_LightIntIntFunctionElementCt =
    R"doc()doc";

static const char* __doc_operations_research_LocalSearch = R"doc()doc";

static const char* __doc_operations_research_LocalSearchFilter = R"doc()doc";

static const char* __doc_operations_research_LocalSearchFilterManager =
    R"doc()doc";

static const char* __doc_operations_research_LocalSearchMonitor = R"doc()doc";

static const char* __doc_operations_research_LocalSearchOperator = R"doc()doc";

static const char* __doc_operations_research_LocalSearchPhaseParameters =
    R"doc()doc";

static const char* __doc_operations_research_LocalSearchProfiler = R"doc()doc";

static const char* __doc_operations_research_ModelCache = R"doc()doc";

static const char* __doc_operations_research_ModelVisitor =
    R"doc(Model visitor.)doc";

static const char* __doc_operations_research_ModelVisitor_2 =
    R"doc(Model visitor.)doc";

static const char* __doc_operations_research_ModelVisitor_BeginVisitConstraint =
    R"doc()doc";

static const char* __doc_operations_research_ModelVisitor_BeginVisitExtension =
    R"doc()doc";

static const char*
    __doc_operations_research_ModelVisitor_BeginVisitIntegerExpression =
        R"doc()doc";

static const char* __doc_operations_research_ModelVisitor_BeginVisitModel =
    R"doc(Begin/End visit element.)doc";

static const char* __doc_operations_research_ModelVisitor_EndVisitConstraint =
    R"doc()doc";

static const char* __doc_operations_research_ModelVisitor_EndVisitExtension =
    R"doc()doc";

static const char*
    __doc_operations_research_ModelVisitor_EndVisitIntegerExpression =
        R"doc()doc";

static const char* __doc_operations_research_ModelVisitor_EndVisitModel =
    R"doc()doc";

static const char*
    __doc_operations_research_ModelVisitor_VisitInt64ToBoolExtension =
        R"doc(Using SWIG on callbacks is troublesome, so we hide these methods
during the wrapping.)doc";

static const char*
    __doc_operations_research_ModelVisitor_VisitInt64ToInt64AsArray =
        R"doc(Expands function as array when index min is 0.)doc";

static const char*
    __doc_operations_research_ModelVisitor_VisitInt64ToInt64Extension =
        R"doc()doc";

static const char* __doc_operations_research_ModelVisitor_VisitIntegerArgument =
    R"doc(Visit integer arguments.)doc";

static const char*
    __doc_operations_research_ModelVisitor_VisitIntegerArrayArgument =
        R"doc()doc";

static const char*
    __doc_operations_research_ModelVisitor_VisitIntegerExpressionArgument =
        R"doc(Visit integer expression argument.)doc";

static const char*
    __doc_operations_research_ModelVisitor_VisitIntegerMatrixArgument =
        R"doc()doc";

static const char* __doc_operations_research_ModelVisitor_VisitIntegerVariable =
    R"doc()doc";

static const char*
    __doc_operations_research_ModelVisitor_VisitIntegerVariable_2 = R"doc()doc";

static const char*
    __doc_operations_research_ModelVisitor_VisitIntegerVariableArrayArgument =
        R"doc()doc";

static const char*
    __doc_operations_research_ModelVisitor_VisitIntegerVariableEvaluatorArgument =
        R"doc(Helpers.)doc";

static const char*
    __doc_operations_research_ModelVisitor_VisitIntervalArgument =
        R"doc(Visit interval argument.)doc";

static const char*
    __doc_operations_research_ModelVisitor_VisitIntervalArrayArgument =
        R"doc()doc";

static const char*
    __doc_operations_research_ModelVisitor_VisitIntervalVariable = R"doc()doc";

static const char*
    __doc_operations_research_ModelVisitor_VisitSequenceArgument =
        R"doc(Visit sequence argument.)doc";

static const char*
    __doc_operations_research_ModelVisitor_VisitSequenceArrayArgument =
        R"doc()doc";

static const char*
    __doc_operations_research_ModelVisitor_VisitSequenceVariable = R"doc()doc";

static const char* __doc_operations_research_NumericalRev =
    R"doc(Subclass of Rev<T> which adds numerical operations.)doc";

static const char* __doc_operations_research_NumericalRevArray =
    R"doc(Subclass of RevArray<T> which adds numerical operations.)doc";

static const char* __doc_operations_research_NumericalRevArray_Add =
    R"doc()doc";

static const char* __doc_operations_research_NumericalRevArray_Decr =
    R"doc()doc";

static const char* __doc_operations_research_NumericalRevArray_Incr =
    R"doc()doc";

static const char*
    __doc_operations_research_NumericalRevArray_NumericalRevArray = R"doc()doc";

static const char* __doc_operations_research_NumericalRev_Add = R"doc()doc";

static const char* __doc_operations_research_NumericalRev_Decr = R"doc()doc";

static const char* __doc_operations_research_NumericalRev_Incr = R"doc()doc";

static const char* __doc_operations_research_NumericalRev_NumericalRev =
    R"doc()doc";

static const char* __doc_operations_research_ObjectiveMonitor = R"doc()doc";

static const char* __doc_operations_research_ObjectiveMonitor_2 = R"doc()doc";

static const char* __doc_operations_research_ObjectiveMonitor_Accept =
    R"doc()doc";

static const char* __doc_operations_research_ObjectiveMonitor_AcceptDelta =
    R"doc()doc";

static const char* __doc_operations_research_ObjectiveMonitor_AtSolution =
    R"doc()doc";

static const char*
    __doc_operations_research_ObjectiveMonitor_BestInternalValue = R"doc()doc";

static const char* __doc_operations_research_ObjectiveMonitor_BestValue =
    R"doc()doc";

static const char*
    __doc_operations_research_ObjectiveMonitor_CurrentInternalValue =
        R"doc()doc";

static const char*
    __doc_operations_research_ObjectiveMonitor_CurrentInternalValuesAreConstraining =
        R"doc()doc";

static const char* __doc_operations_research_ObjectiveMonitor_EnterSearch =
    R"doc()doc";

static const char*
    __doc_operations_research_ObjectiveMonitor_MakeMinimizationVarsLessOrEqualWithSteps =
        R"doc()doc";

static const char*
    __doc_operations_research_ObjectiveMonitor_MakeMinimizationVarsLessOrEqualWithStepsStatus =
        R"doc()doc";

static const char* __doc_operations_research_ObjectiveMonitor_Maximize =
    R"doc()doc";

static const char* __doc_operations_research_ObjectiveMonitor_MinimizationVar =
    R"doc()doc";

static const char* __doc_operations_research_ObjectiveMonitor_ObjectiveMonitor =
    R"doc()doc";

static const char*
    __doc_operations_research_ObjectiveMonitor_ObjectiveMonitor_2 = R"doc()doc";

static const char* __doc_operations_research_ObjectiveMonitor_ObjectiveVar =
    R"doc()doc";

static const char*
    __doc_operations_research_ObjectiveMonitor_SetCurrentInternalValue =
        R"doc()doc";

static const char* __doc_operations_research_ObjectiveMonitor_Size =
    R"doc()doc";

static const char* __doc_operations_research_ObjectiveMonitor_Step =
    R"doc()doc";

static const char* __doc_operations_research_ObjectiveMonitor_best_values =
    R"doc()doc";

static const char* __doc_operations_research_ObjectiveMonitor_current_values =
    R"doc()doc";

static const char*
    __doc_operations_research_ObjectiveMonitor_found_initial_solution =
        R"doc()doc";

static const char*
    __doc_operations_research_ObjectiveMonitor_minimization_vars = R"doc()doc";

static const char*
    __doc_operations_research_ObjectiveMonitor_minimization_vars_2 =
        R"doc()doc";

static const char* __doc_operations_research_ObjectiveMonitor_objective_vars =
    R"doc()doc";

static const char* __doc_operations_research_ObjectiveMonitor_objective_vars_2 =
    R"doc()doc";

static const char* __doc_operations_research_ObjectiveMonitor_operator_assign =
    R"doc()doc";

static const char* __doc_operations_research_ObjectiveMonitor_steps =
    R"doc()doc";

static const char* __doc_operations_research_ObjectiveMonitor_upper_bounds =
    R"doc()doc";

static const char* __doc_operations_research_One =
    R"doc(This method returns 1)doc";

static const char* __doc_operations_research_OptimizeVar =
    R"doc(This class encapsulates an objective. It requires the direction
(minimize or maximize), the variable to optimize, and the improvement
step.)doc";

static const char* __doc_operations_research_OptimizeVar_2 =
    R"doc(This class encapsulates an objective. It requires the direction
(minimize or maximize), the variable to optimize, and the improvement
step.)doc";

static const char* __doc_operations_research_OptimizeVar_AcceptSolution =
    R"doc()doc";

static const char* __doc_operations_research_OptimizeVar_ApplyBound =
    R"doc()doc";

static const char* __doc_operations_research_OptimizeVar_AtSolution =
    R"doc()doc";

static const char* __doc_operations_research_OptimizeVar_BeginNextDecision =
    R"doc(Internal methods.)doc";

static const char* __doc_operations_research_OptimizeVar_DebugString =
    R"doc()doc";

static const char* __doc_operations_research_OptimizeVar_Name = R"doc()doc";

static const char* __doc_operations_research_OptimizeVar_OptimizeVar =
    R"doc()doc";

static const char* __doc_operations_research_OptimizeVar_OptimizeVar_2 =
    R"doc()doc";

static const char* __doc_operations_research_OptimizeVar_RefuteDecision =
    R"doc()doc";

static const char* __doc_operations_research_OptimizeVar_best =
    R"doc(Returns the best value found during search.)doc";

static const char* __doc_operations_research_OptimizeVar_var =
    R"doc(Returns the variable that is optimized.)doc";

static const char* __doc_operations_research_Pack = R"doc()doc";

static const char* __doc_operations_research_Pack_2 = R"doc()doc";

static const char* __doc_operations_research_Pack_Accept = R"doc()doc";

static const char*
    __doc_operations_research_Pack_AddCountAssignedItemsDimension =
        R"doc(This dimension links 'count_var' to the actual number of items
assigned to a bin in the pack.)doc";

static const char* __doc_operations_research_Pack_AddCountUsedBinDimension =
    R"doc(This dimension links 'count_var' to the actual number of bins used in
the pack.)doc";

static const char*
    __doc_operations_research_Pack_AddSumVariableWeightsLessOrEqualConstantDimension =
        R"doc(This dimension imposes: forall b in bins, sum (i in items: usage[i] *
is_assigned(i, b)) <= capacity[b] where is_assigned(i, b) is true if
and only if item i is assigned to the bin b.

This can be used to model shapes of items by linking variables of the
same item on parallel dimensions with an allowed assignment
constraint.)doc";

static const char*
    __doc_operations_research_Pack_AddWeightedSumEqualVarDimension =
        R"doc(This dimension imposes that for all bins b, the weighted sum
(weights[i]) of all objects i assigned to 'b' is equal to loads[b].)doc";

static const char*
    __doc_operations_research_Pack_AddWeightedSumEqualVarDimension_2 =
        R"doc(This dimension imposes that for all bins b, the weighted sum
(weights->Run(i, b)) of all objects i assigned to 'b' is equal to
loads[b].)doc";

static const char*
    __doc_operations_research_Pack_AddWeightedSumLessOrEqualConstantDimension =
        R"doc(This dimension imposes that for all bins b, the weighted sum
(weights[i]) of all objects i assigned to 'b' is less or equal
'bounds[b]'.)doc";

static const char*
    __doc_operations_research_Pack_AddWeightedSumLessOrEqualConstantDimension_2 =
        R"doc(This dimension imposes that for all bins b, the weighted sum
(weights->Run(i)) of all objects i assigned to 'b' is less or equal to
'bounds[b]'. Ownership of the callback is transferred to the pack
constraint.)doc";

static const char*
    __doc_operations_research_Pack_AddWeightedSumLessOrEqualConstantDimension_3 =
        R"doc(This dimension imposes that for all bins b, the weighted sum
(weights->Run(i, b) of all objects i assigned to 'b' is less or equal
to 'bounds[b]'. Ownership of the callback is transferred to the pack
constraint.)doc";

static const char*
    __doc_operations_research_Pack_AddWeightedSumOfAssignedDimension =
        R"doc(This dimension enforces that cost_var == sum of weights[i] for all
objects 'i' assigned to a bin.)doc";

static const char* __doc_operations_research_Pack_Assign = R"doc()doc";

static const char* __doc_operations_research_Pack_AssignAllPossibleToBin =
    R"doc()doc";

static const char* __doc_operations_research_Pack_AssignAllRemainingItems =
    R"doc()doc";

static const char* __doc_operations_research_Pack_AssignFirstPossibleToBin =
    R"doc()doc";

static const char* __doc_operations_research_Pack_AssignVar = R"doc()doc";

static const char* __doc_operations_research_Pack_ClearAll = R"doc()doc";

static const char* __doc_operations_research_Pack_DebugString = R"doc()doc";

static const char* __doc_operations_research_Pack_InitialPropagate =
    R"doc()doc";

static const char* __doc_operations_research_Pack_IsAssignedStatusKnown =
    R"doc()doc";

static const char* __doc_operations_research_Pack_IsInProcess = R"doc()doc";

static const char* __doc_operations_research_Pack_IsPossible = R"doc()doc";

static const char* __doc_operations_research_Pack_IsUndecided = R"doc()doc";

static const char* __doc_operations_research_Pack_OneDomain = R"doc()doc";

static const char* __doc_operations_research_Pack_Pack = R"doc()doc";

static const char* __doc_operations_research_Pack_Post = R"doc()doc";

static const char* __doc_operations_research_Pack_Propagate = R"doc()doc";

static const char* __doc_operations_research_Pack_PropagateDelayed =
    R"doc()doc";

static const char* __doc_operations_research_Pack_RemoveAllPossibleFromBin =
    R"doc()doc";

static const char* __doc_operations_research_Pack_SetAssigned = R"doc()doc";

static const char* __doc_operations_research_Pack_SetImpossible = R"doc()doc";

static const char* __doc_operations_research_Pack_SetUnassigned = R"doc()doc";

static const char* __doc_operations_research_Pack_UnassignAllRemainingItems =
    R"doc()doc";

static const char* __doc_operations_research_Pack_bins = R"doc()doc";

static const char* __doc_operations_research_Pack_demon = R"doc()doc";

static const char* __doc_operations_research_Pack_dims = R"doc()doc";

static const char* __doc_operations_research_Pack_forced = R"doc()doc";

static const char* __doc_operations_research_Pack_holes = R"doc()doc";

static const char* __doc_operations_research_Pack_in_process = R"doc()doc";

static const char* __doc_operations_research_Pack_removed = R"doc()doc";

static const char* __doc_operations_research_Pack_stamp = R"doc()doc";

static const char* __doc_operations_research_Pack_to_set = R"doc()doc";

static const char* __doc_operations_research_Pack_to_unset = R"doc()doc";

static const char* __doc_operations_research_Pack_unprocessed = R"doc()doc";

static const char* __doc_operations_research_Pack_vars = R"doc()doc";

static const char* __doc_operations_research_ProfiledDecisionBuilder =
    R"doc()doc";

static const char* __doc_operations_research_ProfiledDecisionBuilder_2 =
    R"doc()doc";

static const char* __doc_operations_research_ProfiledDecisionBuilder_Accept =
    R"doc()doc";

static const char*
    __doc_operations_research_ProfiledDecisionBuilder_AppendMonitors =
        R"doc()doc";

static const char*
    __doc_operations_research_ProfiledDecisionBuilder_DebugString = R"doc()doc";

static const char* __doc_operations_research_ProfiledDecisionBuilder_Next =
    R"doc()doc";

static const char*
    __doc_operations_research_ProfiledDecisionBuilder_ProfiledDecisionBuilder =
        R"doc()doc";

static const char* __doc_operations_research_ProfiledDecisionBuilder_db =
    R"doc()doc";

static const char* __doc_operations_research_ProfiledDecisionBuilder_name =
    R"doc()doc";

static const char* __doc_operations_research_ProfiledDecisionBuilder_name_2 =
    R"doc()doc";

static const char* __doc_operations_research_ProfiledDecisionBuilder_seconds =
    R"doc()doc";

static const char* __doc_operations_research_ProfiledDecisionBuilder_seconds_2 =
    R"doc()doc";

static const char* __doc_operations_research_ProfiledDecisionBuilder_timer =
    R"doc()doc";

static const char* __doc_operations_research_PropagationBaseObject =
    R"doc(The PropagationBaseObject is a subclass of BaseObject that is also
friend to the Solver class. It allows accessing methods useful when
writing new constraints or new expressions.)doc";

static const char* __doc_operations_research_PropagationBaseObject_2 =
    R"doc(The PropagationBaseObject is a subclass of BaseObject that is also
friend to the Solver class. It allows accessing methods useful when
writing new constraints or new expressions.)doc";

static const char* __doc_operations_research_PropagationBaseObject_BaseName =
    R"doc(Returns a base name for automatic naming.)doc";

static const char* __doc_operations_research_PropagationBaseObject_DebugString =
    R"doc()doc";

static const char* __doc_operations_research_PropagationBaseObject_EnqueueAll =
    R"doc()doc";

static const char*
    __doc_operations_research_PropagationBaseObject_EnqueueDelayedDemon =
        R"doc(This method pushes the demon onto the propagation queue. It will be
processed directly if the queue is empty. It will be enqueued
according to its priority otherwise.)doc";

static const char* __doc_operations_research_PropagationBaseObject_EnqueueVar =
    R"doc()doc";

static const char* __doc_operations_research_PropagationBaseObject_ExecuteAll =
    R"doc()doc";

static const char* __doc_operations_research_PropagationBaseObject_FreezeQueue =
    R"doc(This method freezes the propagation queue. It is useful when you need
to apply multiple modifications at once.)doc";

static const char* __doc_operations_research_PropagationBaseObject_HasName =
    R"doc(Returns whether the object has been named or not.)doc";

static const char*
    __doc_operations_research_PropagationBaseObject_PropagationBaseObject =
        R"doc()doc";

static const char*
    __doc_operations_research_PropagationBaseObject_PropagationBaseObject_2 =
        R"doc()doc";

static const char*
    __doc_operations_research_PropagationBaseObject_UnfreezeQueue =
        R"doc(This method unfreezes the propagation queue. All modifications that
happened when the queue was frozen will be processed.)doc";

static const char* __doc_operations_research_PropagationBaseObject_name =
    R"doc(Object naming.)doc";

static const char*
    __doc_operations_research_PropagationBaseObject_operator_assign =
        R"doc()doc";

static const char*
    __doc_operations_research_PropagationBaseObject_reset_action_on_fail =
        R"doc(This method clears the failure callback.)doc";

static const char*
    __doc_operations_research_PropagationBaseObject_set_action_on_fail =
        R"doc()doc";

static const char* __doc_operations_research_PropagationBaseObject_set_name =
    R"doc()doc";

static const char*
    __doc_operations_research_PropagationBaseObject_set_variable_to_clean_on_fail =
        R"doc(Shortcut for variable cleaner.)doc";

static const char* __doc_operations_research_PropagationBaseObject_solver =
    R"doc()doc";

static const char* __doc_operations_research_PropagationBaseObject_solver_2 =
    R"doc()doc";

static const char* __doc_operations_research_PropagationMonitor = R"doc()doc";

static const char* __doc_operations_research_Queue = R"doc()doc";

static const char* __doc_operations_research_RegularLimit =
    R"doc(Usual limit based on wall_time, number of explored branches and number
of failures in the search tree)doc";

static const char* __doc_operations_research_RegularLimit_2 =
    R"doc(Usual limit based on wall_time, number of explored branches and number
of failures in the search tree)doc";

static const char* __doc_operations_research_RegularLimitParameters =
    R"doc()doc";

static const char*
    __doc_operations_research_RegularLimit_AbsoluteSolverDeadline = R"doc()doc";

static const char* __doc_operations_research_RegularLimit_Accept = R"doc()doc";

static const char* __doc_operations_research_RegularLimit_CheckTime =
    R"doc()doc";

static const char* __doc_operations_research_RegularLimit_CheckWithOffset =
    R"doc()doc";

static const char* __doc_operations_research_RegularLimit_Copy = R"doc()doc";

static const char* __doc_operations_research_RegularLimit_DebugString =
    R"doc()doc";

static const char* __doc_operations_research_RegularLimit_ExitSearch =
    R"doc()doc";

static const char* __doc_operations_research_RegularLimit_GetPercent =
    R"doc()doc";

static const char* __doc_operations_research_RegularLimit_Init = R"doc()doc";

static const char* __doc_operations_research_RegularLimit_Install = R"doc()doc";

static const char*
    __doc_operations_research_RegularLimit_IsUncheckedSolutionLimitReached =
        R"doc()doc";

static const char* __doc_operations_research_RegularLimit_MakeClone =
    R"doc()doc";

static const char* __doc_operations_research_RegularLimit_MakeIdenticalClone =
    R"doc()doc";

static const char* __doc_operations_research_RegularLimit_ProgressPercent =
    R"doc()doc";

static const char* __doc_operations_research_RegularLimit_RegularLimit =
    R"doc()doc";

static const char* __doc_operations_research_RegularLimit_TimeElapsed =
    R"doc()doc";

static const char* __doc_operations_research_RegularLimit_UpdateLimits =
    R"doc()doc";

static const char* __doc_operations_research_RegularLimit_branches =
    R"doc()doc";

static const char* __doc_operations_research_RegularLimit_branches_2 =
    R"doc()doc";

static const char* __doc_operations_research_RegularLimit_branches_offset =
    R"doc()doc";

static const char* __doc_operations_research_RegularLimit_check_count =
    R"doc()doc";

static const char* __doc_operations_research_RegularLimit_cumulative =
    R"doc(If cumulative if false, then the limit applies to each search
independently. If it's true, the limit applies globally to all search
for which this monitor is used. When cumulative is true, the offset
fields have two different meanings depending on context: - within a
search, it's an offset to be subtracted from the current value -
outside of search, it's the amount consumed in previous searches)doc";

static const char* __doc_operations_research_RegularLimit_duration_limit =
    R"doc()doc";

static const char* __doc_operations_research_RegularLimit_duration_limit_2 =
    R"doc()doc";

static const char* __doc_operations_research_RegularLimit_failures =
    R"doc()doc";

static const char* __doc_operations_research_RegularLimit_failures_2 =
    R"doc()doc";

static const char* __doc_operations_research_RegularLimit_failures_offset =
    R"doc()doc";

static const char* __doc_operations_research_RegularLimit_last_time_elapsed =
    R"doc()doc";

static const char* __doc_operations_research_RegularLimit_next_check =
    R"doc()doc";

static const char* __doc_operations_research_RegularLimit_smart_time_check =
    R"doc()doc";

static const char* __doc_operations_research_RegularLimit_solutions =
    R"doc()doc";

static const char* __doc_operations_research_RegularLimit_solutions_2 =
    R"doc()doc";

static const char* __doc_operations_research_RegularLimit_solutions_offset =
    R"doc()doc";

static const char*
    __doc_operations_research_RegularLimit_solver_time_at_limit_start =
        R"doc()doc";

static const char* __doc_operations_research_RegularLimit_wall_time =
    R"doc()doc";

static const char* __doc_operations_research_Rev =
    R"doc(This class adds reversibility to a POD type. It contains the stamp
optimization. i.e. the SaveValue call is done only once per node of
the search tree. Please note that actual stamps always starts at 1,
thus an initial value of 0 will always trigger the first SaveValue.)doc";

static const char* __doc_operations_research_RevArray =
    R"doc(Reversible array of POD types. It contains the stamp optimization.
I.e., the SaveValue call is done only once per node of the search
tree. Please note that actual stamp always starts at 1, thus an
initial value of 0 always triggers the first SaveValue.)doc";

static const char* __doc_operations_research_RevArray_RevArray = R"doc()doc";

static const char* __doc_operations_research_RevArray_SetValue = R"doc()doc";

static const char* __doc_operations_research_RevArray_Value = R"doc()doc";

static const char* __doc_operations_research_RevArray_operator_array =
    R"doc()doc";

static const char* __doc_operations_research_RevArray_size = R"doc()doc";

static const char* __doc_operations_research_RevArray_size_2 = R"doc()doc";

static const char* __doc_operations_research_RevArray_stamps = R"doc()doc";

static const char* __doc_operations_research_RevArray_values = R"doc()doc";

static const char* __doc_operations_research_RevBitMatrix = R"doc()doc";

static const char* __doc_operations_research_Rev_Rev = R"doc()doc";

static const char* __doc_operations_research_Rev_SetValue = R"doc()doc";

static const char* __doc_operations_research_Rev_Value = R"doc()doc";

static const char* __doc_operations_research_Rev_stamp = R"doc()doc";

static const char* __doc_operations_research_Rev_value = R"doc()doc";

static const char* __doc_operations_research_Search = R"doc()doc";

static const char* __doc_operations_research_SearchLimit =
    R"doc(Base class of all search limits.)doc";

static const char* __doc_operations_research_SearchLimit_2 =
    R"doc(Base class of all search limits.)doc";

static const char* __doc_operations_research_SearchLimit_BeginNextDecision =
    R"doc()doc";

static const char* __doc_operations_research_SearchLimit_Check =
    R"doc(This method is called to check the status of the limit. A return value
of true indicates that we have indeed crossed the limit. In that case,
this method will not be called again and the remaining search will be
discarded.)doc";

static const char* __doc_operations_research_SearchLimit_CheckWithOffset =
    R"doc(Same as Check() but adds the 'offset' value to the current time when
time is considered in the limit.)doc";

static const char* __doc_operations_research_SearchLimit_Copy =
    R"doc(Copy a limit. Warning: leads to a direct (no check) downcasting of
'limit' so one needs to be sure both SearchLimits are of the same
type.)doc";

static const char* __doc_operations_research_SearchLimit_DebugString =
    R"doc()doc";

static const char* __doc_operations_research_SearchLimit_EnterSearch =
    R"doc(Internal methods.)doc";

static const char* __doc_operations_research_SearchLimit_Init =
    R"doc(This method is called when the search limit is initialized.)doc";

static const char* __doc_operations_research_SearchLimit_Install = R"doc()doc";

static const char* __doc_operations_research_SearchLimit_MakeClone =
    R"doc(Allocates a clone of the limit.)doc";

static const char* __doc_operations_research_SearchLimit_PeriodicCheck =
    R"doc()doc";

static const char* __doc_operations_research_SearchLimit_RefuteDecision =
    R"doc()doc";

static const char* __doc_operations_research_SearchLimit_SearchLimit =
    R"doc()doc";

static const char* __doc_operations_research_SearchLimit_SearchLimit_2 =
    R"doc()doc";

static const char* __doc_operations_research_SearchLimit_TopPeriodicCheck =
    R"doc()doc";

static const char* __doc_operations_research_SearchLimit_crossed =
    R"doc(Returns true if the limit has been crossed.)doc";

static const char* __doc_operations_research_SearchLimit_crossed_2 =
    R"doc()doc";

static const char* __doc_operations_research_SearchLimit_operator_assign =
    R"doc()doc";

static const char* __doc_operations_research_SearchMonitor =
    R"doc(A search monitor is a simple set of callbacks to monitor all search
events)doc";

static const char* __doc_operations_research_SearchMonitor_2 =
    R"doc(A search monitor is a simple set of callbacks to monitor all search
events)doc";

static const char* __doc_operations_research_SearchMonitor_Accept =
    R"doc(Accepts the given model visitor.)doc";

static const char* __doc_operations_research_SearchMonitor_AcceptDelta =
    R"doc()doc";

static const char* __doc_operations_research_SearchMonitor_AcceptNeighbor =
    R"doc(After accepting a neighbor during local search.)doc";

static const char* __doc_operations_research_SearchMonitor_AcceptSolution =
    R"doc(This method is called when a solution is found. It asserts whether the
solution is valid. A value of false indicates that the solution should
be discarded.)doc";

static const char*
    __doc_operations_research_SearchMonitor_AcceptUncheckedNeighbor =
        R"doc(After accepting an unchecked neighbor during local search.)doc";

static const char* __doc_operations_research_SearchMonitor_AfterDecision =
    R"doc(Just after refuting or applying the decision, apply is true after
Apply. This is called only if the Apply() or Refute() methods have not
failed.)doc";

static const char* __doc_operations_research_SearchMonitor_ApplyDecision =
    R"doc(Before applying the decision.)doc";

static const char* __doc_operations_research_SearchMonitor_AtSolution =
    R"doc(This method is called when a valid solution is found. If the return
value is true, then search will resume after. If the result is false,
then search will stop there.)doc";

static const char* __doc_operations_research_SearchMonitor_BeginFail =
    R"doc(Just when the failure occurs.)doc";

static const char*
    __doc_operations_research_SearchMonitor_BeginInitialPropagation =
        R"doc(Before the initial propagation.)doc";

static const char* __doc_operations_research_SearchMonitor_BeginNextDecision =
    R"doc(Before calling DecisionBuilder::Next.)doc";

static const char* __doc_operations_research_SearchMonitor_EndFail =
    R"doc(After completing the backtrack.)doc";

static const char*
    __doc_operations_research_SearchMonitor_EndInitialPropagation =
        R"doc(After the initial propagation.)doc";

static const char* __doc_operations_research_SearchMonitor_EndNextDecision =
    R"doc(After calling DecisionBuilder::Next, along with the returned decision.)doc";

static const char* __doc_operations_research_SearchMonitor_EnterSearch =
    R"doc(Beginning of the search.)doc";

static const char* __doc_operations_research_SearchMonitor_ExitSearch =
    R"doc(End of the search.)doc";

static const char* __doc_operations_research_SearchMonitor_Install =
    R"doc(Registers itself on the solver such that it gets notified of the
search and propagation events. Override to incrementally install
listeners for specific events.)doc";

static const char*
    __doc_operations_research_SearchMonitor_IsUncheckedSolutionLimitReached =
        R"doc(Returns true if the limit of solutions has been reached including
unchecked solutions.)doc";

static const char* __doc_operations_research_SearchMonitor_ListenToEvent =
    R"doc()doc";

static const char* __doc_operations_research_SearchMonitor_AtLocalOptimum =
    R"doc(When a local optimum is reached. If 'true' is returned, the last
solution is discarded and the search proceeds with the next one.)doc";

static const char* __doc_operations_research_SearchMonitor_NoMoreSolutions =
    R"doc(When the search tree is finished.)doc";

static const char* __doc_operations_research_SearchMonitor_PeriodicCheck =
    R"doc(Periodic call to check limits in long running methods.)doc";

static const char* __doc_operations_research_SearchMonitor_ProgressPercent =
    R"doc(Returns a percentage representing the propress of the search before
reaching limits.)doc";

static const char* __doc_operations_research_SearchMonitor_RefuteDecision =
    R"doc(Before refuting the decision.)doc";

static const char* __doc_operations_research_SearchMonitor_RestartSearch =
    R"doc(Restart the search.)doc";

static const char* __doc_operations_research_SearchMonitor_SearchMonitor =
    R"doc()doc";

static const char* __doc_operations_research_SearchMonitor_SearchMonitor_2 =
    R"doc()doc";

static const char* __doc_operations_research_SearchMonitor_operator_assign =
    R"doc()doc";

static const char* __doc_operations_research_SearchMonitor_solver = R"doc()doc";

static const char* __doc_operations_research_SearchMonitor_solver_2 =
    R"doc()doc";

static const char* __doc_operations_research_SequenceVar =
    R"doc(A sequence variable is a variable whose domain is a set of possible
orderings of the interval variables. It allows ordering of tasks. It
has two sets of methods: ComputePossibleFirstsAndLasts(), which
returns the list of interval variables that can be ranked first or
last; and RankFirst/RankNotFirst/RankLast/RankNotLast, which can be
used to create the search decision.)doc";

static const char* __doc_operations_research_SequenceVar_2 =
    R"doc(A sequence variable is a variable whose domain is a set of possible
orderings of the interval variables. It allows ordering of tasks. It
has two sets of methods: ComputePossibleFirstsAndLasts(), which
returns the list of interval variables that can be ranked first or
last; and RankFirst/RankNotFirst/RankLast/RankNotLast, which can be
used to create the search decision.)doc";

static const char* __doc_operations_research_SequenceVarAssignment =
    R"doc()doc";

static const char* __doc_operations_research_SequenceVarElement =
    R"doc(The SequenceVarElement stores a partial representation of ranked
interval variables in the underlying sequence variable. This
representation consists of three vectors: - the forward sequence. That
is the list of interval variables ranked first in the sequence. The
first element of the backward sequence is the first interval in the
sequence variable. - the backward sequence. That is the list of
interval variables ranked last in the sequence. The first element of
the backward sequence is the last interval in the sequence variable. -
The list of unperformed interval variables. Furthermore, if all
performed variables are ranked, then by convention, the
forward_sequence will contain all such variables and the
backward_sequence will be empty.)doc";

static const char*
    __doc_operations_research_SequenceVarElement_BackwardSequence = R"doc()doc";

static const char* __doc_operations_research_SequenceVarElement_Bound =
    R"doc()doc";

static const char*
    __doc_operations_research_SequenceVarElement_CheckClassInvariants =
        R"doc()doc";

static const char* __doc_operations_research_SequenceVarElement_Clone =
    R"doc()doc";

static const char* __doc_operations_research_SequenceVarElement_Copy =
    R"doc()doc";

static const char* __doc_operations_research_SequenceVarElement_DebugString =
    R"doc()doc";

static const char*
    __doc_operations_research_SequenceVarElement_ForwardSequence = R"doc()doc";

static const char* __doc_operations_research_SequenceVarElement_LoadFromProto =
    R"doc()doc";

static const char* __doc_operations_research_SequenceVarElement_Reset =
    R"doc()doc";

static const char* __doc_operations_research_SequenceVarElement_Restore =
    R"doc()doc";

static const char*
    __doc_operations_research_SequenceVarElement_SequenceVarElement =
        R"doc()doc";

static const char*
    __doc_operations_research_SequenceVarElement_SequenceVarElement_2 =
        R"doc()doc";

static const char*
    __doc_operations_research_SequenceVarElement_SetBackwardSequence =
        R"doc()doc";

static const char*
    __doc_operations_research_SequenceVarElement_SetForwardSequence =
        R"doc()doc";

static const char* __doc_operations_research_SequenceVarElement_SetSequence =
    R"doc()doc";

static const char* __doc_operations_research_SequenceVarElement_SetUnperformed =
    R"doc()doc";

static const char* __doc_operations_research_SequenceVarElement_Store =
    R"doc()doc";

static const char* __doc_operations_research_SequenceVarElement_Unperformed =
    R"doc()doc";

static const char* __doc_operations_research_SequenceVarElement_Var =
    R"doc()doc";

static const char* __doc_operations_research_SequenceVarElement_WriteToProto =
    R"doc()doc";

static const char*
    __doc_operations_research_SequenceVarElement_backward_sequence =
        R"doc()doc";

static const char*
    __doc_operations_research_SequenceVarElement_forward_sequence = R"doc()doc";

static const char* __doc_operations_research_SequenceVarElement_operator_eq =
    R"doc()doc";

static const char* __doc_operations_research_SequenceVarElement_operator_ne =
    R"doc()doc";

static const char* __doc_operations_research_SequenceVarElement_unperformed =
    R"doc()doc";

static const char* __doc_operations_research_SequenceVarElement_var =
    R"doc()doc";

static const char* __doc_operations_research_SequenceVar_Accept =
    R"doc(Accepts the given visitor.)doc";

static const char* __doc_operations_research_SequenceVar_ActiveHorizonRange =
    R"doc(Returns the minimum start min and the maximum end max of all unranked
interval vars in the sequence.)doc";

static const char*
    __doc_operations_research_SequenceVar_ComputeBackwardFrontier = R"doc()doc";

static const char*
    __doc_operations_research_SequenceVar_ComputeForwardFrontier = R"doc()doc";

static const char*
    __doc_operations_research_SequenceVar_ComputePossibleFirstsAndLasts =
        R"doc(Computes the set of indices of interval variables that can be ranked
first in the set of unranked activities.)doc";

static const char* __doc_operations_research_SequenceVar_ComputeStatistics =
    R"doc(Compute statistics on the sequence.)doc";

static const char* __doc_operations_research_SequenceVar_DebugString =
    R"doc()doc";

static const char* __doc_operations_research_SequenceVar_DurationRange =
    R"doc(Returns the minimum and maximum duration of combined interval vars in
the sequence.)doc";

static const char* __doc_operations_research_SequenceVar_FillSequence =
    R"doc(Clears 'rank_first' and 'rank_last', and fills them with the intervals
in the order of the ranks. If all variables are ranked, 'rank_first'
will contain all variables, and 'rank_last' will contain none.
'unperformed' will contains all such interval variables. rank_first
and rank_last represents different directions. rank_first[0]
corresponds to the first interval of the sequence. rank_last[0]
corresponds to the last interval of the sequence.)doc";

static const char* __doc_operations_research_SequenceVar_HorizonRange =
    R"doc(Returns the minimum start min and the maximum end max of all interval
vars in the sequence.)doc";

static const char* __doc_operations_research_SequenceVar_Interval =
    R"doc(Returns the index_th interval of the sequence.)doc";

static const char* __doc_operations_research_SequenceVar_Next =
    R"doc(Returns the next of the index_th interval of the sequence.)doc";

static const char* __doc_operations_research_SequenceVar_RankFirst =
    R"doc(Ranks the index_th interval var first of all unranked interval vars.
After that, it will no longer be considered ranked.)doc";

static const char* __doc_operations_research_SequenceVar_RankLast =
    R"doc(Ranks the index_th interval var first of all unranked interval vars.
After that, it will no longer be considered ranked.)doc";

static const char* __doc_operations_research_SequenceVar_RankNotFirst =
    R"doc(Indicates that the index_th interval var will not be ranked first of
all currently unranked interval vars.)doc";

static const char* __doc_operations_research_SequenceVar_RankNotLast =
    R"doc(Indicates that the index_th interval var will not be ranked first of
all currently unranked interval vars.)doc";

static const char* __doc_operations_research_SequenceVar_RankSequence =
    R"doc(Applies the following sequence of ranks, ranks first, then rank last.
rank_first and rank_last represents different directions.
rank_first[0] corresponds to the first interval of the sequence.
rank_last[0] corresponds to the last interval of the sequence. All
intervals in the unperformed vector will be marked as such.)doc";

static const char* __doc_operations_research_SequenceVar_SequenceVar =
    R"doc()doc";

static const char* __doc_operations_research_SequenceVar_UpdatePrevious =
    R"doc()doc";

static const char* __doc_operations_research_SequenceVar_intervals =
    R"doc()doc";

static const char* __doc_operations_research_SequenceVar_nexts = R"doc()doc";

static const char* __doc_operations_research_SequenceVar_previous = R"doc()doc";

static const char* __doc_operations_research_SequenceVar_size =
    R"doc(Returns the number of interval vars in the sequence.)doc";

static const char* __doc_operations_research_SetAssignmentFromAssignment =
    R"doc(Given a "source_assignment", clears the "target_assignment" and adds
all IntVars in "target_vars", with the values of the variables set
according to the corresponding values of "source_vars" in
"source_assignment". source_vars and target_vars must have the same
number of elements. The source and target assignments can belong to
different Solvers.)doc";

static const char* __doc_operations_research_SimpleRevFIFO = R"doc()doc";

static const char* __doc_operations_research_SolutionCollector =
    R"doc(This class is the root class of all solution collectors. It implements
a basic query API to be used independently of the collector used.)doc";

static const char* __doc_operations_research_SolutionCollector_2 =
    R"doc(This class is the root class of all solution collectors. It implements
a basic query API to be used independently of the collector used.)doc";

static const char* __doc_operations_research_SolutionCollector_Add =
    R"doc(Add API.)doc";

static const char* __doc_operations_research_SolutionCollector_Add_2 =
    R"doc()doc";

static const char* __doc_operations_research_SolutionCollector_Add_3 =
    R"doc()doc";

static const char* __doc_operations_research_SolutionCollector_Add_4 =
    R"doc()doc";

static const char* __doc_operations_research_SolutionCollector_Add_5 =
    R"doc()doc";

static const char* __doc_operations_research_SolutionCollector_Add_6 =
    R"doc()doc";

static const char* __doc_operations_research_SolutionCollector_AddObjective =
    R"doc()doc";

static const char* __doc_operations_research_SolutionCollector_AddObjectives =
    R"doc()doc";

static const char*
    __doc_operations_research_SolutionCollector_BackwardSequence =
        R"doc(This is a shortcut to get the BackwardSequence of 'var' in the nth
solution. The backward sequence is the list of ranked interval
variables starting from the end of the sequence.)doc";

static const char*
    __doc_operations_research_SolutionCollector_BuildSolutionDataForCurrentState =
        R"doc()doc";

static const char* __doc_operations_research_SolutionCollector_DebugString =
    R"doc()doc";

static const char* __doc_operations_research_SolutionCollector_DurationValue =
    R"doc(This is a shortcut to get the DurationValue of 'var' in the nth
solution.)doc";

static const char* __doc_operations_research_SolutionCollector_EndValue =
    R"doc(This is a shortcut to get the EndValue of 'var' in the nth solution.)doc";

static const char* __doc_operations_research_SolutionCollector_EnterSearch =
    R"doc(Beginning of the search.)doc";

static const char* __doc_operations_research_SolutionCollector_ForwardSequence =
    R"doc(This is a shortcut to get the ForwardSequence of 'var' in the nth
solution. The forward sequence is the list of ranked interval
variables starting from the start of the sequence.)doc";

static const char* __doc_operations_research_SolutionCollector_FreeSolution =
    R"doc()doc";

static const char* __doc_operations_research_SolutionCollector_Install =
    R"doc()doc";

static const char*
    __doc_operations_research_SolutionCollector_ObjectiveValueFromIndex =
        R"doc(Returns the value of the index-th objective of the nth solution.)doc";

static const char* __doc_operations_research_SolutionCollector_PerformedValue =
    R"doc(This is a shortcut to get the PerformedValue of 'var' in the nth
solution.)doc";

static const char* __doc_operations_research_SolutionCollector_PopSolution =
    R"doc(Remove and delete the last popped solution.)doc";

static const char* __doc_operations_research_SolutionCollector_Push =
    R"doc()doc";

static const char* __doc_operations_research_SolutionCollector_PushSolution =
    R"doc(Push the current state as a new solution.)doc";

static const char*
    __doc_operations_research_SolutionCollector_SolutionCollector = R"doc()doc";

static const char*
    __doc_operations_research_SolutionCollector_SolutionCollector_2 =
        R"doc()doc";

static const char*
    __doc_operations_research_SolutionCollector_SolutionCollector_3 =
        R"doc()doc";

static const char* __doc_operations_research_SolutionCollector_SolutionData =
    R"doc()doc";

static const char*
    __doc_operations_research_SolutionCollector_SolutionData_ObjectiveValue =
        R"doc()doc";

static const char*
    __doc_operations_research_SolutionCollector_SolutionData_ObjectiveValueFromIndex =
        R"doc()doc";

static const char*
    __doc_operations_research_SolutionCollector_SolutionData_branches =
        R"doc()doc";

static const char*
    __doc_operations_research_SolutionCollector_SolutionData_failures =
        R"doc()doc";

static const char*
    __doc_operations_research_SolutionCollector_SolutionData_operator_lt =
        R"doc()doc";

static const char*
    __doc_operations_research_SolutionCollector_SolutionData_solution =
        R"doc()doc";

static const char*
    __doc_operations_research_SolutionCollector_SolutionData_time = R"doc()doc";

static const char* __doc_operations_research_SolutionCollector_StartValue =
    R"doc(This is a shortcut to get the StartValue of 'var' in the nth solution.)doc";

static const char* __doc_operations_research_SolutionCollector_Unperformed =
    R"doc(This is a shortcut to get the list of unperformed of 'var' in the nth
solution.)doc";

static const char* __doc_operations_research_SolutionCollector_Value =
    R"doc(This is a shortcut to get the Value of 'var' in the nth solution.)doc";

static const char* __doc_operations_research_SolutionCollector_branches =
    R"doc(Returns the number of branches when the nth solution was found.)doc";

static const char* __doc_operations_research_SolutionCollector_check_index =
    R"doc()doc";

static const char* __doc_operations_research_SolutionCollector_failures =
    R"doc(Returns the number of failures encountered at the time of the nth
solution.)doc";

static const char* __doc_operations_research_SolutionCollector_has_solution =
    R"doc(Returns whether any solutions were stored during the search.)doc";

static const char*
    __doc_operations_research_SolutionCollector_last_solution_or_null =
        R"doc(Returns the last solution if there are any, nullptr otherwise.)doc";

static const char* __doc_operations_research_SolutionCollector_objective_value =
    R"doc(Returns the objective value of the nth solution.)doc";

static const char* __doc_operations_research_SolutionCollector_operator_assign =
    R"doc()doc";

static const char* __doc_operations_research_SolutionCollector_prototype =
    R"doc()doc";

static const char*
    __doc_operations_research_SolutionCollector_recycle_solutions = R"doc()doc";

static const char* __doc_operations_research_SolutionCollector_solution =
    R"doc(Returns the nth solution.)doc";

static const char* __doc_operations_research_SolutionCollector_solution_count =
    R"doc(Returns how many solutions were stored during the search.)doc";

static const char* __doc_operations_research_SolutionCollector_solution_data =
    R"doc()doc";

static const char* __doc_operations_research_SolutionCollector_solution_pool =
    R"doc()doc";

static const char* __doc_operations_research_SolutionCollector_wall_time =
    R"doc(Returns the wall time in ms for the nth solution.)doc";

static const char* __doc_operations_research_SolutionPool =
    R"doc(This class is used to manage a pool of solutions. It can transform a
single point local search into a multipoint local search.)doc";

static const char* __doc_operations_research_SolutionPool_2 =
    R"doc(This class is used to manage a pool of solutions. It can transform a
single point local search into a multipoint local search.)doc";

static const char* __doc_operations_research_SolutionPool_GetNextSolution =
    R"doc(This method is called when the local search starts a new neighborhood
to initialize the default assignment.)doc";

static const char* __doc_operations_research_SolutionPool_Initialize =
    R"doc(This method is called to initialize the solution pool with the
assignment from the local search.)doc";

static const char* __doc_operations_research_SolutionPool_RegisterNewSolution =
    R"doc(This method is called when a new solution has been accepted by the
local search.)doc";

static const char* __doc_operations_research_SolutionPool_SolutionPool =
    R"doc()doc";

static const char* __doc_operations_research_SolutionPool_SyncNeeded =
    R"doc(This method checks if the local solution needs to be updated with an
external one.)doc";

static const char* __doc_operations_research_Solver =
    R"doc(Solver Class

A solver represents the main computation engine. It implements the
entire range of Constraint Programming protocols: - Reversibility -
Propagation - Search

Usually, Constraint Programming code consists of - the creation of the
Solver, - the creation of the decision variables of the model, - the
creation of the constraints of the model and their addition to the
solver() through the AddConstraint() method, - the creation of the
main DecisionBuilder class, - the launch of the solve() method with
the decision builder.

For the time being, Solver is neither MT_SAFE nor MT_HOT.)doc";

static const char* __doc_operations_research_Solver_ABSL_DEPRECATED =
    R"doc()doc";

static const char* __doc_operations_research_Solver_ABSL_DEPRECATED_2 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_Accept =
    R"doc(Accepts the given model visitor.)doc";

static const char* __doc_operations_research_Solver_ActiveSearch =
    R"doc(Returns the active search, nullptr outside search.)doc";

static const char* __doc_operations_research_Solver_AddBacktrackAction =
    R"doc(When SaveValue() is not the best way to go, one can create a
reversible action that will be called upon backtrack. The "fast"
parameter indicates whether we need restore all values saved through
SaveValue() before calling this method.)doc";

static const char* __doc_operations_research_Solver_AddCastConstraint =
    R"doc(Adds 'constraint' to the solver and marks it as a cast constraint,
that is, a constraint created calling Var() on an expression. This is
used internally.)doc";

static const char* __doc_operations_research_Solver_AddConstraint =
    R"doc(Adds the constraint 'c' to the model.

After calling this method, and until there is a backtrack that undoes
the addition, any assignment of variables to values must satisfy the
given constraint in order to be considered feasible. There are two
fairly different use cases:

- the most common use case is modeling: the given constraint is really
part of the problem that the user is trying to solve. In this use
case, AddConstraint is called outside of search (i.e., with ``state()
== OUTSIDE_SEARCH``). Most users should only use AddConstraint in this
way. In this case, the constraint will belong to the model forever: it
cannot be removed by backtracking.

- a rarer use case is that 'c' is not a real constraint of the model.
It may be a constraint generated by a branching decision (a constraint
whose goal is to restrict the search space), a symmetry breaking
constraint (a constraint that does restrict the search space, but in a
way that cannot have an impact on the quality of the solutions in the
subtree), or an inferred constraint that, while having no semantic
value to the model (it does not restrict the set of solutions), is
worth having because we believe it may strengthen the propagation. In
these cases, it happens that the constraint is added during the search
(i.e., with state() == IN_SEARCH or state() == IN_ROOT_NODE). When a
constraint is added during a search, it applies only to the subtree of
the search tree rooted at the current node, and will be automatically
removed by backtracking.

This method does not take ownership of the constraint. If the
constraint has been created by any factory method (Solver::MakeXXX),
it will automatically be deleted. However, power users who implement
their own constraints should do:
solver.AddConstraint(solver.RevAlloc(new MyConstraint(...));)doc";

static const char* __doc_operations_research_Solver_AddLocalSearchMonitor =
    R"doc(Adds the local search monitor to the solver. This is called internally
when a propagation monitor is passed to the Solve() or NewSearch()
method.)doc";

static const char* __doc_operations_research_Solver_AddPropagationMonitor =
    R"doc(Adds the propagation monitor to the solver. This is called internally
when a propagation monitor is passed to the Solve() or NewSearch()
method.)doc";

static const char* __doc_operations_research_Solver_BacktrackOneLevel =
    R"doc()doc";

static const char* __doc_operations_research_Solver_BacktrackToSentinel =
    R"doc()doc";

static const char* __doc_operations_research_Solver_BinaryIntervalRelation =
    R"doc(This enum is used in Solver::MakeIntervalVarRelation to specify the
temporal relation between the two intervals t1 and t2.)doc";

static const char*
    __doc_operations_research_Solver_BinaryIntervalRelation_ENDS_AFTER_END =
        R"doc(t1 ends after t2 end, i.e. End(t1) >= End(t2) + delay.)doc";

static const char*
    __doc_operations_research_Solver_BinaryIntervalRelation_ENDS_AFTER_START =
        R"doc(t1 ends after t2 start, i.e. End(t1) >= Start(t2) + delay.)doc";

static const char*
    __doc_operations_research_Solver_BinaryIntervalRelation_ENDS_AT_END =
        R"doc(t1 ends at t2 end, i.e. End(t1) == End(t2) + delay.)doc";

static const char*
    __doc_operations_research_Solver_BinaryIntervalRelation_ENDS_AT_START =
        R"doc(t1 ends at t2 start, i.e. End(t1) == Start(t2) + delay.)doc";

static const char*
    __doc_operations_research_Solver_BinaryIntervalRelation_STARTS_AFTER_END =
        R"doc(t1 starts after t2 end, i.e. Start(t1) >= End(t2) + delay.)doc";

static const char*
    __doc_operations_research_Solver_BinaryIntervalRelation_STARTS_AFTER_START =
        R"doc(t1 starts after t2 start, i.e. Start(t1) >= Start(t2) + delay.)doc";

static const char*
    __doc_operations_research_Solver_BinaryIntervalRelation_STARTS_AT_END =
        R"doc(t1 starts at t2 end, i.e. Start(t1) == End(t2) + delay.)doc";

static const char*
    __doc_operations_research_Solver_BinaryIntervalRelation_STARTS_AT_START =
        R"doc(t1 starts at t2 start, i.e. Start(t1) == Start(t2) + delay.)doc";

static const char*
    __doc_operations_research_Solver_BinaryIntervalRelation_STAYS_IN_SYNC =
        R"doc(STARTS_AT_START and ENDS_AT_END at the same time. t1 starts at t2
start, i.e. Start(t1) == Start(t2) + delay. t1 ends at t2 end, i.e.
End(t1) == End(t2).)doc";

static const char* __doc_operations_research_Solver_Cache =
    R"doc(Returns the cache of the model.)doc";

static const char* __doc_operations_research_Solver_CastExpression =
    R"doc(Internal. If the variables is the result of expr->Var(), this method
returns expr, nullptr otherwise.)doc";

static const char* __doc_operations_research_Solver_CheckAssignment =
    R"doc(Checks whether the given assignment satisfies all relevant
constraints.)doc";

static const char* __doc_operations_research_Solver_CheckConstraint =
    R"doc(Checks whether adding this constraint will lead to an immediate
failure. It will return false if the model is already inconsistent, or
if adding the constraint makes it inconsistent.)doc";

static const char* __doc_operations_research_Solver_CheckFail = R"doc()doc";

static const char* __doc_operations_research_Solver_ClearLocalSearchState =
    R"doc(Clears the local search state.)doc";

static const char* __doc_operations_research_Solver_ClearNeighbors =
    R"doc(Manipulate neighbors count; to be used for testing purposes only.
TODO(user): Find a workaround to avoid exposing this.)doc";

static const char* __doc_operations_research_Solver_Compose =
    R"doc(Creates a decision builder which sequentially composes decision
builders. At each leaf of a decision builder, the next decision
builder is therefore called. For instance, Compose(db1, db2) will
result in the following tree: d1 tree | / | \ | db1 leaves | / | \ |
db2 tree db2 tree db2 tree |)doc";

static const char* __doc_operations_research_Solver_Compose_2 = R"doc()doc";

static const char* __doc_operations_research_Solver_Compose_3 = R"doc()doc";

static const char* __doc_operations_research_Solver_Compose_4 = R"doc()doc";

static const char* __doc_operations_research_Solver_ConcatenateOperators =
    R"doc(Creates a local search operator which concatenates a vector of
operators. Each operator from the vector is called sequentially. By
default, when a neighbor is found the neighborhood exploration
restarts from the last active operator (the one which produced the
neighbor). This can be overridden by setting restart to true to force
the exploration to start from the first operator in the vector.

The default behavior can also be overridden using an evaluation
callback to set the order in which the operators are explored (the
callback is called in LocalSearchOperator::Start()). The first
argument of the callback is the index of the operator which produced
the last move, the second argument is the index of the operator to be
evaluated. Ownership of the callback is taken by ConcatenateOperators.

Example:

const int kPriorities = {10, 100, 10, 0}; int64_t Evaluate(int
active_operator, int current_operator) { return
kPriorities[current_operator]; }

LocalSearchOperator* concat = solver.ConcatenateOperators(operators,
NewPermanentCallback(&Evaluate));

The elements of the vector operators will be sorted by increasing
priority and explored in that order (tie-breaks are handled by keeping
the relative operator order in the vector). This would result in the
following order: operators[3], operators[0], operators[2],
operators[1].)doc";

static const char* __doc_operations_research_Solver_ConcatenateOperators_2 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_ConcatenateOperators_3 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_CurrentlyInSolve =
    R"doc(Returns true whether the current search has been created using a
Solve() call instead of a NewSearch one. It returns false if the
solver is not in search at all.)doc";

static const char* __doc_operations_research_Solver_DebugString =
    R"doc(misc debug string.)doc";

static const char* __doc_operations_research_Solver_DecisionModification =
    R"doc(The Solver is responsible for creating the search tree. Thanks to the
DecisionBuilder, it creates a new decision with two branches at each
node: left and right. The DecisionModification enum is used to specify
how the branch selector should behave.)doc";

static const char*
    __doc_operations_research_Solver_DecisionModification_KEEP_LEFT =
        R"doc(Right branches are ignored. This is used to make the code faster when
backtrack makes no sense or is not useful. This is faster as there is
no need to create one new node per decision.)doc";

static const char*
    __doc_operations_research_Solver_DecisionModification_KEEP_RIGHT =
        R"doc(Left branches are ignored. This is used to make the code faster when
backtrack makes no sense or is not useful. This is faster as there is
no need to create one new node per decision.)doc";

static const char*
    __doc_operations_research_Solver_DecisionModification_KILL_BOTH =
        R"doc(Backtracks to the previous decisions, i.e. left and right branches are
not applied.)doc";

static const char*
    __doc_operations_research_Solver_DecisionModification_NO_CHANGE =
        R"doc(Keeps the default behavior, i.e. apply left branch first, and then
right branch in case of backtracking.)doc";

static const char*
    __doc_operations_research_Solver_DecisionModification_SWITCH_BRANCHES =
        R"doc(Applies right branch first. Left branch will be applied in case of
backtracking.)doc";

static const char* __doc_operations_research_Solver_DefaultSolverParameters =
    R"doc(Create a ConstraintSolverParameters proto with all the default values.)doc";

static const char* __doc_operations_research_Solver_DemonPriority =
    R"doc(This enum represents the three possible priorities for a demon in the
Solver queue. Note: this is for advanced users only.)doc";

static const char*
    __doc_operations_research_Solver_DemonPriority_DELAYED_PRIORITY =
        R"doc(DELAYED_PRIORITY is the lowest priority: Demons will be processed
after VAR_PRIORITY and NORMAL_PRIORITY demons.)doc";

static const char*
    __doc_operations_research_Solver_DemonPriority_NORMAL_PRIORITY =
        R"doc(NORMAL_PRIORITY is the highest priority: Demons will be processed
first.)doc";

static const char* __doc_operations_research_Solver_DemonPriority_VAR_PRIORITY =
    R"doc(VAR_PRIORITY is between DELAYED_PRIORITY and NORMAL_PRIORITY.)doc";

static const char* __doc_operations_research_Solver_EndSearch = R"doc()doc";

static const char* __doc_operations_research_Solver_EnqueueAll = R"doc()doc";

static const char* __doc_operations_research_Solver_EnqueueDelayedDemon =
    R"doc()doc";

static const char* __doc_operations_research_Solver_EnqueueVar = R"doc()doc";

static const char* __doc_operations_research_Solver_EvaluatorLocalSearchOperators =
    R"doc(This enum is used in Solver::MakeOperator associated with an evaluator
to specify the neighborhood to create.)doc";

static const char*
    __doc_operations_research_Solver_EvaluatorLocalSearchOperators_LK =
        R"doc(Lin-Kernighan local search. While the accumulated local gain is
positive, perform a 2opt or a 3opt move followed by a series of 2opt
moves. Return a neighbor for which the global gain is positive.)doc";

static const char*
    __doc_operations_research_Solver_EvaluatorLocalSearchOperators_TSPLNS =
        R"doc(TSP-base LNS. Randomly merge consecutive nodes until n "meta"-nodes
remain and solve the corresponding TSP. This is an "unlimited"
neighborhood which must be stopped by search limits. To force
diversification, the operator iteratively forces each node to serve as
base of a meta-node.)doc";

static const char*
    __doc_operations_research_Solver_EvaluatorLocalSearchOperators_TSPOPT =
        R"doc(Sliding TSP operator. Uses an exact dynamic programming algorithm to
solve the TSP corresponding to path sub-chains. For a subchain 1 -> 2
-> 3 -> 4 -> 5 -> 6, solves the TSP on nodes A, 2, 3, 4, 5, where A is
a merger of nodes 1 and 6 such that cost(A,i) = cost(1,i) and
cost(i,A) = cost(i,6).)doc";

static const char* __doc_operations_research_Solver_EvaluatorStrategy =
    R"doc(This enum is used by Solver::MakePhase to specify how to select
variables and values during the search. In Solver::MakePhase(const
std::vector<IntVar*>&, IntVarStrategy, IntValueStrategy), variables
are selected first, and then the associated value. In
Solver::MakePhase(const std::vector<IntVar*>& vars, IndexEvaluator2,
EvaluatorStrategy), the selection is done scanning every pair
<variable, possible value>. The next selected pair is then the best
among all possibilities, i.e. the pair with the smallest evaluation.
As this is costly, two options are offered: static or dynamic
evaluation.)doc";

static const char*
    __doc_operations_research_Solver_EvaluatorStrategy_CHOOSE_DYNAMIC_GLOBAL_BEST =
        R"doc(Pairs are compared each time a variable is selected. That way all
pairs are relevant and evaluation is accurate. This strategy runs in
O(number-of-pairs) at each variable selection, versus O(1) in the
static version.)doc";

static const char*
    __doc_operations_research_Solver_EvaluatorStrategy_CHOOSE_STATIC_GLOBAL_BEST =
        R"doc(Pairs are compared at the first call of the selector, and results are
cached. Next calls to the selector use the previous computation, and
so are not up-to-date, e.g. some <variable, value> pairs may not be
possible anymore due to propagation since the first to call.)doc";

static const char* __doc_operations_research_Solver_ExecuteAll = R"doc()doc";

static const char* __doc_operations_research_Solver_ExportProfilingOverview =
    R"doc(Exports the profiling information in a human readable overview. The
parameter profile_level used to create the solver must be set to true.)doc";

static const char* __doc_operations_research_Solver_Fail =
    R"doc(Abandon the current branch in the search tree. A backtrack will
follow.)doc";

static const char* __doc_operations_research_Solver_FinishCurrentSearch =
    R"doc(Tells the solver to kill or restart the current search.)doc";

static const char* __doc_operations_research_Solver_FreezeQueue = R"doc()doc";

static const char*
    __doc_operations_research_Solver_GetConstraintSolverStatistics =
        R"doc(Returns detailed cp search statistics.)doc";

static const char* __doc_operations_research_Solver_GetLocalSearchMonitor =
    R"doc(Returns the local search monitor.)doc";

static const char* __doc_operations_research_Solver_GetLocalSearchStatistics =
    R"doc(Returns detailed local search statistics.)doc";

static const char* __doc_operations_research_Solver_GetName = R"doc(Naming)doc";

static const char* __doc_operations_research_Solver_GetNewIntVarIndex =
    R"doc(Variable indexing (note that indexing is not reversible). Returns a
new index for an IntVar.)doc";

static const char*
    __doc_operations_research_Solver_GetOrCreateLocalSearchState =
        R"doc(Returns (or creates) an assignment representing the state of local
search.)doc";

static const char* __doc_operations_research_Solver_GetPropagationMonitor =
    R"doc(Returns the propagation monitor.)doc";

static const char* __doc_operations_research_Solver_HasName =
    R"doc(Returns whether the object has been named or not.)doc";

static const char* __doc_operations_research_Solver_ImprovementSearchLimit =
    R"doc(Limits the search based on the improvements of 'objective_var'. Stops
the search when the improvement rate gets lower than a threshold
value. This threshold value is computed based on the improvement rate
during the first phase of the search.)doc";

static const char* __doc_operations_research_Solver_IncrementNeighbors =
    R"doc()doc";

static const char*
    __doc_operations_research_Solver_IncrementUncheckedSolutionCounter =
        R"doc()doc";

static const char* __doc_operations_research_Solver_Init = R"doc()doc";

static const char* __doc_operations_research_Solver_InitCachedConstraint =
    R"doc()doc";

static const char* __doc_operations_research_Solver_InitCachedIntConstants =
    R"doc()doc";

static const char* __doc_operations_research_Solver_InstrumentsDemons =
    R"doc(Returns whether we are instrumenting demons.)doc";

static const char* __doc_operations_research_Solver_InstrumentsVariables =
    R"doc(Returns whether we are tracing variables.)doc";

static const char* __doc_operations_research_Solver_IntValueStrategy =
    R"doc(This enum describes the strategy used to select the next variable
value to set.)doc";

static const char*
    __doc_operations_research_Solver_IntValueStrategy_ASSIGN_CENTER_VALUE =
        R"doc(Selects the first possible value which is the closest to the center of
the domain of the selected variable. The center is defined as (min +
max) / 2.)doc";

static const char*
    __doc_operations_research_Solver_IntValueStrategy_ASSIGN_MAX_VALUE =
        R"doc(Selects the max value of the selected variable.)doc";

static const char*
    __doc_operations_research_Solver_IntValueStrategy_ASSIGN_MIN_VALUE =
        R"doc(Selects the min value of the selected variable.)doc";

static const char*
    __doc_operations_research_Solver_IntValueStrategy_ASSIGN_RANDOM_VALUE =
        R"doc(Selects randomly one of the possible values of the selected variable.)doc";

static const char*
    __doc_operations_research_Solver_IntValueStrategy_INT_VALUE_DEFAULT =
        R"doc(The default behavior is ASSIGN_MIN_VALUE.)doc";

static const char*
    __doc_operations_research_Solver_IntValueStrategy_INT_VALUE_SIMPLE =
        R"doc(The simple selection is ASSIGN_MIN_VALUE.)doc";

static const char*
    __doc_operations_research_Solver_IntValueStrategy_SPLIT_LOWER_HALF =
        R"doc(Split the domain in two around the center, and choose the lower part
first.)doc";

static const char*
    __doc_operations_research_Solver_IntValueStrategy_SPLIT_UPPER_HALF =
        R"doc(Split the domain in two around the center, and choose the lower part
first.)doc";

static const char* __doc_operations_research_Solver_IntVarStrategy =
    R"doc(This enum describes the strategy used to select the next branching
variable at each node during the search.)doc";

static const char*
    __doc_operations_research_Solver_IntVarStrategy_CHOOSE_FIRST_UNBOUND =
        R"doc(Select the first unbound variable. Variables are considered in the
order of the vector of IntVars used to create the selector.)doc";

static const char*
    __doc_operations_research_Solver_IntVarStrategy_CHOOSE_HIGHEST_MAX =
        R"doc(Among unbound variables, select the variable with the highest maximal
value. In case of a tie, the first one is selected, first being
defined by the order in the vector of IntVars used to create the
selector.)doc";

static const char*
    __doc_operations_research_Solver_IntVarStrategy_CHOOSE_LOWEST_MIN =
        R"doc(Among unbound variables, select the variable with the smallest minimal
value. In case of a tie, the first one is selected, "first" defined by
the order in the vector of IntVars used to create the selector.)doc";

static const char*
    __doc_operations_research_Solver_IntVarStrategy_CHOOSE_MAX_REGRET_ON_MIN =
        R"doc(Among unbound variables, select the variable with the largest gap
between the first and the second values of the domain.)doc";

static const char*
    __doc_operations_research_Solver_IntVarStrategy_CHOOSE_MAX_SIZE =
        R"doc(Among unbound variables, select the variable with the highest size. In
case of a tie, the first one is selected, first being defined by the
order in the vector of IntVars used to create the selector.)doc";

static const char*
    __doc_operations_research_Solver_IntVarStrategy_CHOOSE_MIN_SIZE =
        R"doc(Among unbound variables, select the variable with the smallest size.
In case of a tie, the first one is selected, first being defined by
the order in the vector of IntVars used to create the selector.)doc";

static const char*
    __doc_operations_research_Solver_IntVarStrategy_CHOOSE_MIN_SIZE_HIGHEST_MAX =
        R"doc(Among unbound variables, select the variable with the smallest size,
i.e., the smallest number of possible values. In case of a tie, the
selected variable is the one with the highest max value. In case of a
tie, the first one is selected, first being defined by the order in
the vector of IntVars used to create the selector.)doc";

static const char*
    __doc_operations_research_Solver_IntVarStrategy_CHOOSE_MIN_SIZE_HIGHEST_MIN =
        R"doc(Among unbound variables, select the variable with the smallest size,
i.e., the smallest number of possible values. In case of a tie, the
selected variable is the one with the highest min value. In case of a
tie, the first one is selected, first being defined by the order in
the vector of IntVars used to create the selector.)doc";

static const char*
    __doc_operations_research_Solver_IntVarStrategy_CHOOSE_MIN_SIZE_LOWEST_MAX =
        R"doc(Among unbound variables, select the variable with the smallest size,
i.e., the smallest number of possible values. In case of a tie, the
selected variables is the one with the lowest max value. In case of a
tie, the first one is selected, first being defined by the order in
the vector of IntVars used to create the selector.)doc";

static const char*
    __doc_operations_research_Solver_IntVarStrategy_CHOOSE_MIN_SIZE_LOWEST_MIN =
        R"doc(Among unbound variables, select the variable with the smallest size,
i.e., the smallest number of possible values. In case of a tie, the
selected variables is the one with the lowest min value. In case of a
tie, the first one is selected, first being defined by the order in
the vector of IntVars used to create the selector.)doc";

static const char* __doc_operations_research_Solver_IntVarStrategy_CHOOSE_PATH =
    R"doc(Selects the next unbound variable on a path, the path being defined by
the variables: var[i] corresponds to the index of the next of i.)doc";

static const char*
    __doc_operations_research_Solver_IntVarStrategy_CHOOSE_RANDOM =
        R"doc(Randomly select one of the remaining unbound variables.)doc";

static const char*
    __doc_operations_research_Solver_IntVarStrategy_INT_VAR_DEFAULT =
        R"doc(The default behavior is CHOOSE_FIRST_UNBOUND.)doc";

static const char*
    __doc_operations_research_Solver_IntVarStrategy_INT_VAR_SIMPLE =
        R"doc(The simple selection is CHOOSE_FIRST_UNBOUND.)doc";

static const char* __doc_operations_research_Solver_IntegerCastInfo =
    R"doc(Holds semantic information stating that the 'expression' has been cast
into 'variable' using the Var() method, and that 'maintainer' is
responsible for maintaining the equality between 'variable' and
'expression'.)doc";

static const char*
    __doc_operations_research_Solver_IntegerCastInfo_IntegerCastInfo =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_IntegerCastInfo_IntegerCastInfo_2 =
        R"doc()doc";

static const char* __doc_operations_research_Solver_IntegerCastInfo_expression =
    R"doc()doc";

static const char* __doc_operations_research_Solver_IntegerCastInfo_maintainer =
    R"doc()doc";

static const char* __doc_operations_research_Solver_IntegerCastInfo_variable =
    R"doc()doc";

static const char* __doc_operations_research_Solver_InternalSaveValue =
    R"doc()doc";

static const char* __doc_operations_research_Solver_InternalSaveValue_2 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_InternalSaveValue_3 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_InternalSaveValue_4 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_InternalSaveValue_5 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_InternalSaveValue_6 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_InternalSaveValue_7 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_IntervalStrategy =
    R"doc(This enum describes the straregy used to select the next interval
variable and its value to be fixed.)doc";

static const char*
    __doc_operations_research_Solver_IntervalStrategy_INTERVAL_DEFAULT =
        R"doc(The default is INTERVAL_SET_TIMES_FORWARD.)doc";

static const char*
    __doc_operations_research_Solver_IntervalStrategy_INTERVAL_SET_TIMES_BACKWARD =
        R"doc(Selects the variable with the highest ending time of all variables,
and fixes the ending time to this highest values.)doc";

static const char*
    __doc_operations_research_Solver_IntervalStrategy_INTERVAL_SET_TIMES_FORWARD =
        R"doc(Selects the variable with the lowest starting time of all variables,
and fixes its starting time to this lowest value.)doc";

static const char*
    __doc_operations_research_Solver_IntervalStrategy_INTERVAL_SIMPLE =
        R"doc(The simple is INTERVAL_SET_TIMES_FORWARD.)doc";

static const char* __doc_operations_research_Solver_IsADifference =
    R"doc(Internal.)doc";

static const char* __doc_operations_research_Solver_IsBooleanVar =
    R"doc(Returns true if expr represents either boolean_var or 1 - boolean_var.
In that case, it fills inner_var and is_negated to be true if the
expression is 1 - boolean_var -- equivalent to not(boolean_var).)doc";

static const char*
    __doc_operations_research_Solver_IsLocalSearchProfilingEnabled =
        R"doc(Returns whether we are profiling local search.)doc";

static const char* __doc_operations_research_Solver_IsProduct =
    R"doc(Returns true if expr represents a product of a expr and a constant. In
that case, it fills inner_expr and coefficient with these, and returns
true. In the other case, it fills inner_expr with expr, coefficient
with 1, and returns false.)doc";

static const char* __doc_operations_research_Solver_IsProfilingEnabled =
    R"doc(Returns whether we are profiling the solver.)doc";

static const char*
    __doc_operations_research_Solver_IsUncheckedSolutionLimitReached =
        R"doc()doc";

static const char* __doc_operations_research_Solver_JumpToSentinel =
    R"doc()doc";

static const char* __doc_operations_research_Solver_JumpToSentinelWhenNested =
    R"doc()doc";

static const char* __doc_operations_research_Solver_LocalSearchFilterBound =
    R"doc(This enum is used in Solver::MakeLocalSearchObjectiveFilter. It
specifies the behavior of the objective filter to create. The goal is
to define under which condition a move is accepted based on the
current objective value.)doc";

static const char* __doc_operations_research_Solver_LocalSearchFilterBound_EQ =
    R"doc(Move is accepted when the current objective value is in the interval
objective.Min .. objective.Max.)doc";

static const char* __doc_operations_research_Solver_LocalSearchFilterBound_GE =
    R"doc(Move is accepted when the current objective value >= objective.Min.)doc";

static const char* __doc_operations_research_Solver_LocalSearchFilterBound_LE =
    R"doc(Move is accepted when the current objective value <= objective.Max.)doc";

static const char* __doc_operations_research_Solver_LocalSearchOperators =
    R"doc(This enum is used in Solver::MakeOperator to specify the neighborhood
to create.)doc";

static const char* __doc_operations_research_Solver_LocalSearchOperators_CROSS =
    R"doc(Operator which cross exchanges the starting chains of 2 paths,
including exchanging the whole paths. First and last nodes are not
moved. Possible neighbors for the paths 1 -> 2 -> 3 -> 4 -> 5 and 6 ->
7 -> 8 (where (1, 5) and (6, 8) are first and last nodes of the paths
and can therefore not be moved): 1 -> [7] -> 3 -> 4 -> 5 6 -> [2] -> 8
1 -> [7] -> 4 -> 5 6 -> [2 -> 3] -> 8 1 -> [7] -> 5 6 -> [2 -> 3 -> 4]
-> 8)doc";

static const char*
    __doc_operations_research_Solver_LocalSearchOperators_DECREMENT =
        R"doc(Operator which defines a neighborhood to decrement values. The
behavior is the same as INCREMENT, except values are decremented
instead of incremented.)doc";

static const char*
    __doc_operations_research_Solver_LocalSearchOperators_EXCHANGE =
        R"doc(Operator which exchanges the positions of two nodes. Possible
neighbors for the path 1 -> 2 -> 3 -> 4 -> 5 (where (1, 5) are first
and last nodes of the path and can therefore not be moved): 1 -> [3]
-> [2] -> 4 -> 5 1 -> [4] -> 3 -> [2] -> 5 1 -> 2 -> [4] -> [3] -> 5)doc";

static const char*
    __doc_operations_research_Solver_LocalSearchOperators_EXTENDEDSWAPACTIVE =
        R"doc(Operator which makes an inactive node active and an active one
inactive. It is similar to SwapActiveOperator except that it tries to
insert the inactive node in all possible positions instead of just the
position of the node made inactive. Possible neighbors for the path 1
-> 2 -> 3 -> 4 with 5 inactive (where 1 and 4 are first and last nodes
of the path) are: 1 -> [5] -> 3 -> 4 with 2 inactive 1 -> 3 -> [5] ->
4 with 2 inactive 1 -> [5] -> 2 -> 4 with 3 inactive 1 -> 2 -> [5] ->
4 with 3 inactive)doc";

static const char*
    __doc_operations_research_Solver_LocalSearchOperators_FULLPATHLNS =
        R"doc(Operator which relaxes one entire path and all inactive nodes, thus
defining num_paths neighbors.)doc";

static const char*
    __doc_operations_research_Solver_LocalSearchOperators_INCREMENT =
        R"doc(Operator which defines one neighbor per variable. Each neighbor tries
to increment by one the value of the corresponding variable. When a
new solution is found the neighborhood is rebuilt from scratch, i.e.,
tries to increment values in the variable order. Consider for instance
variables x and y. x is incremented one by one to its max, and when it
is not possible to increment x anymore, y is incremented once. If this
is a solution, then next neighbor tries to increment x.)doc";

static const char*
    __doc_operations_research_Solver_LocalSearchOperators_MAKEACTIVE =
        R"doc(Operator which inserts an inactive node into a path. Possible
neighbors for the path 1 -> 2 -> 3 -> 4 with 5 inactive (where 1 and 4
are first and last nodes of the path) are: 1 -> [5] -> 2 -> 3 -> 4 1
-> 2 -> [5] -> 3 -> 4 1 -> 2 -> 3 -> [5] -> 4)doc";

static const char*
    __doc_operations_research_Solver_LocalSearchOperators_MAKECHAININACTIVE =
        R"doc(Operator which makes a "chain" of path nodes inactive. Possible
neighbors for the path 1 -> 2 -> 3 -> 4 (where 1 and 4 are first and
last nodes of the path) are: 1 -> 3 -> 4 with 2 inactive 1 -> 2 -> 4
with 3 inactive 1 -> 4 with 2 and 3 inactive)doc";

static const char*
    __doc_operations_research_Solver_LocalSearchOperators_MAKEINACTIVE =
        R"doc(Operator which makes path nodes inactive. Possible neighbors for the
path 1 -> 2 -> 3 -> 4 (where 1 and 4 are first and last nodes of the
path) are: 1 -> 3 -> 4 with 2 inactive 1 -> 2 -> 4 with 3 inactive)doc";

static const char* __doc_operations_research_Solver_LocalSearchOperators_OROPT =
    R"doc(Relocate: OROPT and RELOCATE. Operator which moves a sub-chain of a
path to another position; the specified chain length is the fixed
length of the chains being moved. When this length is 1, the operator
simply moves a node to another position. Possible neighbors for the
path 1 -> 2 -> 3 -> 4 -> 5, for a chain length of 2 (where (1, 5) are
first and last nodes of the path and can therefore not be moved): 1 ->
4 -> [2 -> 3] -> 5 1 -> [3 -> 4] -> 2 -> 5

Using Relocate with chain lengths of 1, 2 and 3 together is equivalent
to the OrOpt operator on a path. The OrOpt operator is a limited
version of 3Opt (breaks 3 arcs on a path).)doc";

static const char* __doc_operations_research_Solver_LocalSearchOperators_PATHLNS =
    R"doc(Operator which relaxes two sub-chains of three consecutive arcs each.
Each sub-chain is defined by a start node and the next three arcs.
Those six arcs are relaxed to build a new neighbor. PATHLNS explores
all possible pairs of starting nodes and so defines n^2 neighbors, n
being the number of nodes. Note that the two sub-chains can be part of
the same path; they even may overlap.)doc";

static const char*
    __doc_operations_research_Solver_LocalSearchOperators_RELOCATE =
        R"doc(Relocate neighborhood with length of 1 (see OROPT comment).)doc";

static const char*
    __doc_operations_research_Solver_LocalSearchOperators_SIMPLELNS =
        R"doc(Operator which defines one neighbor per variable. Each neighbor
relaxes one variable. When a new solution is found the neighborhood is
rebuilt from scratch. Consider for instance variables x and y. First x
is relaxed and the solver is looking for the best possible solution
(with only x relaxed). Then y is relaxed, and the solver is looking
for a new solution. If a new solution is found, then the next variable
to be relaxed is x.)doc";

static const char*
    __doc_operations_research_Solver_LocalSearchOperators_SWAPACTIVE =
        R"doc(Operator which replaces an active node by an inactive one. Possible
neighbors for the path 1 -> 2 -> 3 -> 4 with 5 inactive (where 1 and 4
are first and last nodes of the path) are: 1 -> [5] -> 3 -> 4 with 2
inactive 1 -> 2 -> [5] -> 4 with 3 inactive)doc";

static const char*
    __doc_operations_research_Solver_LocalSearchOperators_TWOOPT =
        R"doc(Operator which reverses a sub-chain of a path. It is called TwoOpt
because it breaks two arcs on the path; resulting paths are called
two-optimal. Possible neighbors for the path 1 -> 2 -> 3 -> 4 -> 5
(where (1, 5) are first and last nodes of the path and can therefore
not be moved): 1 -> [3 -> 2] -> 4 -> 5 1 -> [4 -> 3 -> 2] -> 5 1 -> 2
-> [4 -> 3] -> 5)doc";

static const char*
    __doc_operations_research_Solver_LocalSearchOperators_UNACTIVELNS =
        R"doc(Operator which relaxes all inactive nodes and one sub-chain of six
consecutive arcs. That way the path can be improved by inserting
inactive nodes or swapping arcs.)doc";

static const char* __doc_operations_research_Solver_LocalSearchProfile =
    R"doc(Returns local search profiling information in a human readable format.)doc";

static const char* __doc_operations_research_Solver_MakeAbs = R"doc(|expr|)doc";

static const char* __doc_operations_research_Solver_MakeAbsEquality =
    R"doc(Creates the constraint abs(var) == abs_var.)doc";

static const char* __doc_operations_research_Solver_MakeAcceptFilter =
    R"doc(Local Search Filters)doc";

static const char* __doc_operations_research_Solver_MakeActionDemon =
    R"doc(Creates a demon from a callback.)doc";

static const char* __doc_operations_research_Solver_MakeAllDifferent =
    R"doc(All variables are pairwise different. This corresponds to the stronger
version of the propagation algorithm.)doc";

static const char* __doc_operations_research_Solver_MakeAllDifferent_2 =
    R"doc(All variables are pairwise different. If 'stronger_propagation' is
true, stronger, and potentially slower propagation will occur. This
API will be deprecated in the future.)doc";

static const char* __doc_operations_research_Solver_MakeAllDifferentExcept =
    R"doc(All variables are pairwise different, unless they are assigned to the
escape value.)doc";

static const char* __doc_operations_research_Solver_MakeAllSolutionCollector =
    R"doc(Collect all solutions of the search.)doc";

static const char* __doc_operations_research_Solver_MakeAllSolutionCollector_2 =
    R"doc(Collect all solutions of the search. The variables will need to be
added later.)doc";

static const char* __doc_operations_research_Solver_MakeAllowedAssignments =
    R"doc(This method creates a constraint where the graph of the relation
between the variables is given in extension. There are 'arity'
variables involved in the relation and the graph is given by a integer
tuple set.)doc";

static const char* __doc_operations_research_Solver_MakeApplyBranchSelector =
    R"doc(Creates a decision builder that will set the branch selector.)doc";

static const char* __doc_operations_research_Solver_MakeAssignVariableValue =
    R"doc(Decisions.)doc";

static const char*
    __doc_operations_research_Solver_MakeAssignVariableValueOrDoNothing =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_MakeAssignVariableValueOrFail =
        R"doc()doc";

static const char* __doc_operations_research_Solver_MakeAssignVariablesValues =
    R"doc()doc";

static const char*
    __doc_operations_research_Solver_MakeAssignVariablesValuesOrDoNothing =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_MakeAssignVariablesValuesOrFail =
        R"doc()doc";

static const char* __doc_operations_research_Solver_MakeAssignment =
    R"doc(This method creates an empty assignment.)doc";

static const char* __doc_operations_research_Solver_MakeAssignment_2 =
    R"doc(This method creates an assignment which is a copy of 'a'.)doc";

static const char* __doc_operations_research_Solver_MakeAtMost =
    R"doc(|{i | vars[i] == value}| <= max_count)doc";

static const char* __doc_operations_research_Solver_MakeAtSolutionCallback =
    R"doc()doc";

static const char*
    __doc_operations_research_Solver_MakeBestLexicographicValueSolutionCollector =
        R"doc(Same as above, but supporting lexicographic objectives; 'maximize'
specifies the optimization direction for each objective in
'assignment'.)doc";

static const char*
    __doc_operations_research_Solver_MakeBestLexicographicValueSolutionCollector_2 =
        R"doc(Same as above, but supporting lexicographic objectives; 'maximize'
specifies the optimization direction for each objective.)doc";

static const char*
    __doc_operations_research_Solver_MakeBestValueSolutionCollector =
        R"doc(Collect the solution corresponding to the optimal value of the
objective of 'assignment'; if 'assignment' does not have an objective
no solution is collected. This collector only collects one solution
corresponding to the best objective value (the first one found).)doc";

static const char*
    __doc_operations_research_Solver_MakeBestValueSolutionCollector_2 =
        R"doc(Collect the solution corresponding to the optimal value of the
objective of the internal assignment; if this assignment does not have
an objective no solution is collected. This collector only collects
one solution corresponding to the best objective value (the first one
found). The variables and objective(s) will need to be added later.)doc";

static const char* __doc_operations_research_Solver_MakeBetweenCt =
    R"doc((l <= expr <= u))doc";

static const char* __doc_operations_research_Solver_MakeBoolVar =
    R"doc(MakeBoolVar will create a variable with a {0, 1} domain.)doc";

static const char* __doc_operations_research_Solver_MakeBoolVar_2 =
    R"doc(MakeBoolVar will create a variable with a {0, 1} domain.)doc";

static const char* __doc_operations_research_Solver_MakeBoolVarArray =
    R"doc(This method will append the vector vars with 'var_count' boolean
variables having name "name<i>" where <i> is the index of the
variable.)doc";

static const char* __doc_operations_research_Solver_MakeBoolVarArray_2 =
    R"doc(This method will append the vector vars with 'var_count' boolean
variables having no names.)doc";

static const char* __doc_operations_research_Solver_MakeBoolVarArray_3 =
    R"doc(Same but allocates an array and returns it.)doc";

static const char* __doc_operations_research_Solver_MakeCircuit =
    R"doc(Force the "nexts" variable to create a complete Hamiltonian path.)doc";

static const char* __doc_operations_research_Solver_MakeClosureDemon =
    R"doc(!defined(SWIG) Creates a demon from a closure.)doc";

static const char* __doc_operations_research_Solver_MakeConditionalExpression =
    R"doc(Conditional Expr condition ? expr : unperformed_value)doc";

static const char* __doc_operations_research_Solver_MakeConstantRestart =
    R"doc(This search monitor will restart the search periodically after
'frequency' failures.)doc";

static const char* __doc_operations_research_Solver_MakeConstraintAdder =
    R"doc(Returns a decision builder that will add the given constraint to the
model.)doc";

static const char*
    __doc_operations_research_Solver_MakeConstraintInitialPropagateCallback =
        R"doc(This method is a specialized case of the MakeConstraintDemon method to
call the InitiatePropagate of the constraint 'ct'.)doc";

static const char* __doc_operations_research_Solver_MakeConvexPiecewiseExpr =
    R"doc(Convex piecewise function.)doc";

static const char* __doc_operations_research_Solver_MakeCount =
    R"doc(|{i | vars[i] == value}| == max_count)doc";

static const char* __doc_operations_research_Solver_MakeCount_2 =
    R"doc(|{i | vars[i] == value}| == max_count)doc";

static const char* __doc_operations_research_Solver_MakeCover =
    R"doc(This constraint states that the target_var is the convex hull of the
intervals. If none of the interval variables is performed, then the
target var is unperformed too. Also, if the target variable is
unperformed, then all the intervals variables are unperformed too.)doc";

static const char* __doc_operations_research_Solver_MakeCumulative =
    R"doc(This constraint forces that, for any integer t, the sum of the demands
corresponding to an interval containing t does not exceed the given
capacity.

Intervals and demands should be vectors of equal size.

Demands should only contain non-negative values. Zero values are
supported, and the corresponding intervals are filtered out, as they
neither impact nor are impacted by this constraint.)doc";

static const char* __doc_operations_research_Solver_MakeCumulative_2 =
    R"doc(This constraint forces that, for any integer t, the sum of the demands
corresponding to an interval containing t does not exceed the given
capacity.

Intervals and demands should be vectors of equal size.

Demands should only contain non-negative values. Zero values are
supported, and the corresponding intervals are filtered out, as they
neither impact nor are impacted by this constraint.)doc";

static const char* __doc_operations_research_Solver_MakeCumulative_3 =
    R"doc(This constraint forces that, for any integer t, the sum of the demands
corresponding to an interval containing t does not exceed the given
capacity.

Intervals and demands should be vectors of equal size.

Demands should only contain non-negative values. Zero values are
supported, and the corresponding intervals are filtered out, as they
neither impact nor are impacted by this constraint.)doc";

static const char* __doc_operations_research_Solver_MakeCumulative_4 =
    R"doc(This constraint enforces that, for any integer t, the sum of the
demands corresponding to an interval containing t does not exceed the
given capacity.

Intervals and demands should be vectors of equal size.

Demands should only contain non-negative values. Zero values are
supported, and the corresponding intervals are filtered out, as they
neither impact nor are impacted by this constraint.)doc";

static const char* __doc_operations_research_Solver_MakeCumulative_5 =
    R"doc(This constraint enforces that, for any integer t, the sum of demands
corresponding to an interval containing t does not exceed the given
capacity.

Intervals and demands should be vectors of equal size.

Demands should be positive.)doc";

static const char* __doc_operations_research_Solver_MakeCumulative_6 =
    R"doc(This constraint enforces that, for any integer t, the sum of demands
corresponding to an interval containing t does not exceed the given
capacity.

Intervals and demands should be vectors of equal size.

Demands should be positive.)doc";

static const char* __doc_operations_research_Solver_MakeDecision = R"doc()doc";

static const char*
    __doc_operations_research_Solver_MakeDecisionBuilderFromAssignment =
        R"doc(Returns a decision builder for which the left-most leaf corresponds to
assignment, the rest of the tree being explored using 'db'.)doc";

static const char* __doc_operations_research_Solver_MakeDefaultPhase =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeDefaultPhase_2 =
    R"doc()doc";

static const char*
    __doc_operations_research_Solver_MakeDefaultRegularLimitParameters =
        R"doc(Creates a regular limit proto containing default values.)doc";

static const char* __doc_operations_research_Solver_MakeDefaultSolutionPool =
    R"doc(Solution Pool.)doc";

static const char*
    __doc_operations_research_Solver_MakeDelayedConstraintInitialPropagateCallback =
        R"doc(This method is a specialized case of the MakeConstraintDemon method to
call the InitiatePropagate of the constraint 'ct' with low priority.)doc";

static const char* __doc_operations_research_Solver_MakeDelayedPathCumul =
    R"doc(Delayed version of the same constraint: propagation on the nexts
variables is delayed until all constraints have propagated.)doc";

static const char* __doc_operations_research_Solver_MakeDeviation =
    R"doc(Deviation constraint: sum_i |n * vars[i] - total_sum| <= deviation_var
and sum_i vars[i] == total_sum n = #vars)doc";

static const char* __doc_operations_research_Solver_MakeDifference =
    R"doc(left - right)doc";

static const char* __doc_operations_research_Solver_MakeDifference_2 =
    R"doc(value - expr)doc";

static const char* __doc_operations_research_Solver_MakeDisjunctiveConstraint =
    R"doc(This constraint forces all interval vars into an non-overlapping
sequence. Intervals with zero duration can be scheduled anywhere.)doc";

static const char* __doc_operations_research_Solver_MakeDistribute =
    R"doc(Aggregated version of count: |{i | v[i] == values[j]}| == cards[j])doc";

static const char* __doc_operations_research_Solver_MakeDistribute_2 =
    R"doc(Aggregated version of count: |{i | v[i] == values[j]}| == cards[j])doc";

static const char* __doc_operations_research_Solver_MakeDistribute_3 =
    R"doc(Aggregated version of count: |{i | v[i] == j}| == cards[j])doc";

static const char* __doc_operations_research_Solver_MakeDistribute_4 =
    R"doc(Aggregated version of count with bounded cardinalities: forall j in 0
.. card_size - 1: card_min <= |{i | v[i] == j}| <= card_max)doc";

static const char* __doc_operations_research_Solver_MakeDistribute_5 =
    R"doc(Aggregated version of count with bounded cardinalities: forall j in 0
.. card_size - 1: card_min[j] <= |{i | v[i] == j}| <= card_max[j])doc";

static const char* __doc_operations_research_Solver_MakeDistribute_6 =
    R"doc(Aggregated version of count with bounded cardinalities: forall j in 0
.. card_size - 1: card_min[j] <= |{i | v[i] == j}| <= card_max[j])doc";

static const char* __doc_operations_research_Solver_MakeDistribute_7 =
    R"doc(Aggregated version of count with bounded cardinalities: forall j in 0
.. card_size - 1: card_min[j] <= |{i | v[i] == values[j]}| <=
card_max[j])doc";

static const char* __doc_operations_research_Solver_MakeDistribute_8 =
    R"doc(Aggregated version of count with bounded cardinalities: forall j in 0
.. card_size - 1: card_min[j] <= |{i | v[i] == values[j]}| <=
card_max[j])doc";

static const char* __doc_operations_research_Solver_MakeDiv =
    R"doc(expr / value (integer division))doc";

static const char* __doc_operations_research_Solver_MakeDiv_2 =
    R"doc(numerator / denominator (integer division). Terms need to be positive.)doc";

static const char* __doc_operations_research_Solver_MakeElement =
    R"doc(values[index])doc";

static const char* __doc_operations_research_Solver_MakeElement_2 =
    R"doc(values[index])doc";

static const char* __doc_operations_research_Solver_MakeElement_3 =
    R"doc(Function-based element. The constraint takes ownership of the
callback. The callback must be able to cope with any possible value in
the domain of 'index' (potentially negative ones too).)doc";

static const char* __doc_operations_research_Solver_MakeElement_4 =
    R"doc(2D version of function-based element expression, values(expr1, expr2).)doc";

static const char* __doc_operations_research_Solver_MakeElement_5 =
    R"doc(vars[expr])doc";

static const char* __doc_operations_research_Solver_MakeElement_6 =
    R"doc(vars(argument))doc";

static const char* __doc_operations_research_Solver_MakeElementEquality =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeElementEquality_2 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeElementEquality_3 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeElementEquality_4 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeEnterSearchCallback =
    R"doc(----- Callback-based search monitors -----)doc";

static const char* __doc_operations_research_Solver_MakeEquality =
    R"doc(left == right)doc";

static const char* __doc_operations_research_Solver_MakeEquality_2 =
    R"doc(expr == value)doc";

static const char* __doc_operations_research_Solver_MakeEquality_3 =
    R"doc(expr == value)doc";

static const char* __doc_operations_research_Solver_MakeEquality_4 =
    R"doc(This constraints states that the two interval variables are equal.)doc";

static const char* __doc_operations_research_Solver_MakeExitSearchCallback =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeFailDecision =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeFalseConstraint =
    R"doc(This constraint always fails.)doc";

static const char* __doc_operations_research_Solver_MakeFalseConstraint_2 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeFirstSolutionCollector =
    R"doc(Collect the first solution of the search.)doc";

static const char* __doc_operations_research_Solver_MakeFirstSolutionCollector_2 =
    R"doc(Collect the first solution of the search. The variables will need to
be added later.)doc";

static const char*
    __doc_operations_research_Solver_MakeFixedDurationEndSyncedOnEndIntervalVar =
        R"doc(Creates an interval var with a fixed duration whose end is
synchronized with the end of another interval, with a given offset.
The performed status is also in sync with the performed status of the
given interval variable.)doc";

static const char*
    __doc_operations_research_Solver_MakeFixedDurationEndSyncedOnStartIntervalVar =
        R"doc(Creates an interval var with a fixed duration whose end is
synchronized with the start of another interval, with a given offset.
The performed status is also in sync with the performed status of the
given interval variable.)doc";

static const char*
    __doc_operations_research_Solver_MakeFixedDurationIntervalVar =
        R"doc(Creates an interval var with a fixed duration. The duration must be
greater than 0. If optional is true, then the interval can be
performed or unperformed. If optional is false, then the interval is
always performed.)doc";

static const char*
    __doc_operations_research_Solver_MakeFixedDurationIntervalVar_2 =
        R"doc(Creates a performed interval var with a fixed duration. The duration
must be greater than 0.)doc";

static const char*
    __doc_operations_research_Solver_MakeFixedDurationIntervalVar_3 =
        R"doc(Creates an interval var with a fixed duration, and performed_variable.
The duration must be greater than 0.)doc";

static const char*
    __doc_operations_research_Solver_MakeFixedDurationIntervalVarArray =
        R"doc(This method fills the vector with 'count' interval variables built
with the corresponding parameters.)doc";

static const char*
    __doc_operations_research_Solver_MakeFixedDurationIntervalVarArray_2 =
        R"doc(This method fills the vector with 'count' interval var built with the
corresponding start variables.)doc";

static const char*
    __doc_operations_research_Solver_MakeFixedDurationIntervalVarArray_3 =
        R"doc(This method fills the vector with interval variables built with the
corresponding start variables.)doc";

static const char*
    __doc_operations_research_Solver_MakeFixedDurationIntervalVarArray_4 =
        R"doc(This method fills the vector with interval variables built with the
corresponding start variables.)doc";

static const char*
    __doc_operations_research_Solver_MakeFixedDurationIntervalVarArray_5 =
        R"doc(This method fills the vector with interval variables built with the
corresponding start and performed variables.)doc";

static const char*
    __doc_operations_research_Solver_MakeFixedDurationIntervalVarArray_6 =
        R"doc(This method fills the vector with interval variables built with the
corresponding start and performed variables.)doc";

static const char*
    __doc_operations_research_Solver_MakeFixedDurationStartSyncedOnEndIntervalVar =
        R"doc(Creates an interval var with a fixed duration whose start is
synchronized with the end of another interval, with a given offset.
The performed status is also in sync with the performed status of the
given interval variable.)doc";

static const char*
    __doc_operations_research_Solver_MakeFixedDurationStartSyncedOnStartIntervalVar =
        R"doc(Creates an interval var with a fixed duration whose start is
synchronized with the start of another interval, with a given offset.
The performed status is also in sync with the performed status of the
given interval variable.)doc";

static const char* __doc_operations_research_Solver_MakeFixedInterval =
    R"doc(Creates a fixed and performed interval.)doc";

static const char* __doc_operations_research_Solver_MakeGenericTabuSearch =
    R"doc(Creates a Tabu Search based on the vars |vars|. A solution is "tabu"
if all the vars in |vars| keep their value.)doc";

static const char* __doc_operations_research_Solver_MakeGreater =
    R"doc(left > right)doc";

static const char* __doc_operations_research_Solver_MakeGreater_2 =
    R"doc(expr > value)doc";

static const char* __doc_operations_research_Solver_MakeGreater_3 =
    R"doc(expr > value)doc";

static const char* __doc_operations_research_Solver_MakeGreaterOrEqual =
    R"doc(left >= right)doc";

static const char* __doc_operations_research_Solver_MakeGreaterOrEqual_2 =
    R"doc(expr >= value)doc";

static const char* __doc_operations_research_Solver_MakeGreaterOrEqual_3 =
    R"doc(expr >= value)doc";

static const char* __doc_operations_research_Solver_MakeGuidedLocalSearch =
    R"doc(Creates a Guided Local Search monitor. Description here:
http://en.wikipedia.org/wiki/Guided_Local_Search)doc";

static const char* __doc_operations_research_Solver_MakeGuidedLocalSearch_2 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeIfThenElseCt =
    R"doc(Special cases with arrays of size two.)doc";

static const char* __doc_operations_research_Solver_MakeIndexExpression =
    R"doc(Returns the expression expr such that vars[expr] == value. It assumes
that vars are all different.)doc";

static const char* __doc_operations_research_Solver_MakeIndexOfConstraint =
    R"doc(This constraint is a special case of the element constraint with an
array of integer variables, where the variables are all different and
the index variable is constrained such that vars[index] == target.)doc";

static const char*
    __doc_operations_research_Solver_MakeIndexOfFirstMaxValueConstraint =
        R"doc(Creates a constraint that binds the index variable to the index of the
first variable with the maximum value.)doc";

static const char*
    __doc_operations_research_Solver_MakeIndexOfFirstMinValueConstraint =
        R"doc(Creates a constraint that binds the index variable to the index of the
first variable with the minimum value.)doc";

static const char* __doc_operations_research_Solver_MakeIntConst =
    R"doc(IntConst will create a constant expression.)doc";

static const char* __doc_operations_research_Solver_MakeIntConst_2 =
    R"doc(IntConst will create a constant expression.)doc";

static const char* __doc_operations_research_Solver_MakeIntVar =
    R"doc(MakeIntVar will create the best range based int var for the bounds
given.)doc";

static const char* __doc_operations_research_Solver_MakeIntVar_2 =
    R"doc(MakeIntVar will create a variable with the given sparse domain.)doc";

static const char* __doc_operations_research_Solver_MakeIntVar_3 =
    R"doc(MakeIntVar will create a variable with the given sparse domain.)doc";

static const char* __doc_operations_research_Solver_MakeIntVar_4 =
    R"doc(MakeIntVar will create the best range based int var for the bounds
given.)doc";

static const char* __doc_operations_research_Solver_MakeIntVar_5 =
    R"doc(MakeIntVar will create a variable with the given sparse domain.)doc";

static const char* __doc_operations_research_Solver_MakeIntVar_6 =
    R"doc(MakeIntVar will create a variable with the given sparse domain.)doc";

static const char* __doc_operations_research_Solver_MakeIntVarArray =
    R"doc(This method will append the vector vars with 'var_count' variables
having bounds vmin and vmax and having name "name<i>" where <i> is the
index of the variable.)doc";

static const char* __doc_operations_research_Solver_MakeIntVarArray_2 =
    R"doc(This method will append the vector vars with 'var_count' variables
having bounds vmin and vmax and having no names.)doc";

static const char* __doc_operations_research_Solver_MakeIntVarArray_3 =
    R"doc(Same but allocates an array and returns it.)doc";

static const char* __doc_operations_research_Solver_MakeIntervalRelaxedMax =
    R"doc(Creates and returns an interval variable that wraps around the given
one, relaxing the max start and end. Relaxing means making unbounded
when optional. If the variable is non optional, this method returns
interval_var.

More precisely, such an interval variable behaves as follows: * When
the underlying must be performed, the returned interval variable
behaves exactly as the underlying; * When the underlying may or may
not be performed, the returned interval variable behaves like the
underlying, except that it is unbounded on the max side; * When the
underlying cannot be performed, the returned interval variable is of
duration 0 and must be performed in an interval unbounded on both
sides.

This is very useful for implementing propagators that may only modify
the start min or end min.)doc";

static const char* __doc_operations_research_Solver_MakeIntervalRelaxedMin =
    R"doc(Creates and returns an interval variable that wraps around the given
one, relaxing the min start and end. Relaxing means making unbounded
when optional. If the variable is non-optional, this method returns
interval_var.

More precisely, such an interval variable behaves as follows: * When
the underlying must be performed, the returned interval variable
behaves exactly as the underlying; * When the underlying may or may
not be performed, the returned interval variable behaves like the
underlying, except that it is unbounded on the min side; * When the
underlying cannot be performed, the returned interval variable is of
duration 0 and must be performed in an interval unbounded on both
sides.

This is very useful to implement propagators that may only modify the
start max or end max.)doc";

static const char* __doc_operations_research_Solver_MakeIntervalVar =
    R"doc(Creates an interval var by specifying the bounds on start, duration,
and end.)doc";

static const char* __doc_operations_research_Solver_MakeIntervalVarArray =
    R"doc(This method fills the vector with 'count' interval var built with the
corresponding parameters.)doc";

static const char* __doc_operations_research_Solver_MakeIntervalVarRelation =
    R"doc(This method creates a relation between an interval var and a date.)doc";

static const char* __doc_operations_research_Solver_MakeIntervalVarRelation_2 =
    R"doc(This method creates a relation between two interval vars.)doc";

static const char*
    __doc_operations_research_Solver_MakeIntervalVarRelationWithDelay =
        R"doc(This method creates a relation between two interval vars. The given
delay is added to the second interval. i.e.: t1 STARTS_AFTER_END of t2
with a delay of 2 means t1 will start at least two units of time after
the end of t2.)doc";

static const char*
    __doc_operations_research_Solver_MakeInversePermutationConstraint =
        R"doc(Creates a constraint that enforces that 'left' and 'right' both
represent permutations of [0..left.size()-1], and that 'right' is the
inverse permutation of 'left', i.e. for all i in [0..left.size()-1],
right[left[i]] = i.)doc";

static const char* __doc_operations_research_Solver_MakeIsBetweenCt =
    R"doc(b == (l <= expr <= u))doc";

static const char* __doc_operations_research_Solver_MakeIsBetweenVar =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeIsDifferentCstCt =
    R"doc(boolvar == (var != value))doc";

static const char* __doc_operations_research_Solver_MakeIsDifferentCstVar =
    R"doc(status var of (var != value))doc";

static const char* __doc_operations_research_Solver_MakeIsDifferentCt =
    R"doc(b == (v1 != v2))doc";

static const char* __doc_operations_research_Solver_MakeIsDifferentVar =
    R"doc(status var of (v1 != v2))doc";

static const char* __doc_operations_research_Solver_MakeIsEqualCstCt =
    R"doc(boolvar == (var == value))doc";

static const char* __doc_operations_research_Solver_MakeIsEqualCstVar =
    R"doc(status var of (var == value))doc";

static const char* __doc_operations_research_Solver_MakeIsEqualCt =
    R"doc(b == (v1 == v2))doc";

static const char* __doc_operations_research_Solver_MakeIsEqualVar =
    R"doc(status var of (v1 == v2))doc";

static const char* __doc_operations_research_Solver_MakeIsGreaterCstCt =
    R"doc(b == (v > c))doc";

static const char* __doc_operations_research_Solver_MakeIsGreaterCstVar =
    R"doc(status var of (var > value))doc";

static const char* __doc_operations_research_Solver_MakeIsGreaterCt =
    R"doc(b == (left > right))doc";

static const char* __doc_operations_research_Solver_MakeIsGreaterOrEqualCstCt =
    R"doc(boolvar == (var >= value))doc";

static const char* __doc_operations_research_Solver_MakeIsGreaterOrEqualCstVar =
    R"doc(status var of (var >= value))doc";

static const char* __doc_operations_research_Solver_MakeIsGreaterOrEqualCt =
    R"doc(b == (left >= right))doc";

static const char* __doc_operations_research_Solver_MakeIsGreaterOrEqualVar =
    R"doc(status var of (left >= right))doc";

static const char* __doc_operations_research_Solver_MakeIsGreaterVar =
    R"doc(status var of (left > right))doc";

static const char* __doc_operations_research_Solver_MakeIsLessCstCt =
    R"doc(b == (v < c))doc";

static const char* __doc_operations_research_Solver_MakeIsLessCstVar =
    R"doc(status var of (var < value))doc";

static const char* __doc_operations_research_Solver_MakeIsLessCt =
    R"doc(b == (left < right))doc";

static const char* __doc_operations_research_Solver_MakeIsLessOrEqualCstCt =
    R"doc(boolvar == (var <= value))doc";

static const char* __doc_operations_research_Solver_MakeIsLessOrEqualCstVar =
    R"doc(status var of (var <= value))doc";

static const char* __doc_operations_research_Solver_MakeIsLessOrEqualCt =
    R"doc(b == (left <= right))doc";

static const char* __doc_operations_research_Solver_MakeIsLessOrEqualVar =
    R"doc(status var of (left <= right))doc";

static const char* __doc_operations_research_Solver_MakeIsLessVar =
    R"doc(status var of (left < right))doc";

static const char*
    __doc_operations_research_Solver_MakeIsLexicalLessOrEqualWithOffsetsCt =
        R"doc()doc";

static const char* __doc_operations_research_Solver_MakeIsMemberCt =
    R"doc(boolvar == (expr in set))doc";

static const char* __doc_operations_research_Solver_MakeIsMemberCt_2 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeIsMemberVar =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeIsMemberVar_2 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeLastSolutionCollector =
    R"doc(Collect the last solution of the search.)doc";

static const char* __doc_operations_research_Solver_MakeLastSolutionCollector_2 =
    R"doc(Collect the last solution of the search. The variables will need to be
added later.)doc";

static const char* __doc_operations_research_Solver_MakeLess =
    R"doc(left < right)doc";

static const char* __doc_operations_research_Solver_MakeLess_2 =
    R"doc(expr < value)doc";

static const char* __doc_operations_research_Solver_MakeLess_3 =
    R"doc(expr < value)doc";

static const char* __doc_operations_research_Solver_MakeLessOrEqual =
    R"doc(left <= right)doc";

static const char* __doc_operations_research_Solver_MakeLessOrEqual_2 =
    R"doc(expr <= value)doc";

static const char* __doc_operations_research_Solver_MakeLessOrEqual_3 =
    R"doc(expr <= value)doc";

static const char* __doc_operations_research_Solver_MakeLexicalLess =
    R"doc(Creates a constraint that enforces that left is lexicographically less
than right.)doc";

static const char* __doc_operations_research_Solver_MakeLexicalLessOrEqual =
    R"doc(Creates a constraint that enforces that left is lexicographically less
than or equal to right.)doc";

static const char*
    __doc_operations_research_Solver_MakeLexicalLessOrEqualWithOffsets =
        R"doc(Creates a constraint that enforces that left is lexicographically less
than or equal to right with an offset. This means that for the first
index i such that left[i] is not in [right[i] - (offset[i] - 1),
right[i]], left[i] + offset[i] <= right[i]. Offset values must be > 0.)doc";

static const char* __doc_operations_research_Solver_MakeLexicographicOptimize =
    R"doc(Creates a lexicographic objective, following the order of the
variables given. Each variable has a corresponding optimization
direction and step.)doc";

static const char*
    __doc_operations_research_Solver_MakeLexicographicSimulatedAnnealing =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_MakeLexicographicTabuSearch = R"doc()doc";

static const char* __doc_operations_research_Solver_MakeLightElement =
    R"doc(Returns a light one-dimension function-based element constraint
ensuring var == values(index). The constraint does not perform bound
reduction of the resulting variable until the index variable is bound.
If deep_serialize returns false, the model visitor will not extract
all possible values from the values function.)doc";

static const char* __doc_operations_research_Solver_MakeLightElement_2 =
    R"doc(Light two-dimension function-based element constraint ensuring var ==
values(index1, index2). The constraint does not perform bound
reduction of the resulting variable until the index variables are
bound. If deep_serialize returns false, the model visitor will not
extract all possible values from the values function.)doc";

static const char* __doc_operations_research_Solver_MakeLocalSearchPhase =
    R"doc(Local Search decision builders factories. Local search is used to
improve a given solution. This initial solution can be specified
either by an Assignment or by a DecisionBulder, and the corresponding
variables, the initial solution being the first solution found by the
DecisionBuilder. The LocalSearchPhaseParameters parameter holds the
actual definition of the local search phase: - a local search operator
used to explore the neighborhood of the current solution, - a decision
builder to instantiate unbound variables once a neighbor has been
defined; in the case of LNS-based operators instantiates fragment
variables; search monitors can be added to this sub-search by wrapping
the decision builder with MakeSolveOnce. - a search limit specifying
how long local search looks for neighbors before accepting one; the
last neighbor is always taken and in the case of a greedy search, this
corresponds to the best local neighbor; first-accept (which is the
default behavior) can be modeled using a solution found limit of 1, -
a vector of local search filters used to speed up the search by
pruning unfeasible neighbors. Metaheuristics can be added by defining
specialized search monitors; currently down/up-hill climbing is
available through OptimizeVar, as well as Guided Local Search, Tabu
Search and Simulated Annealing.)doc";

static const char* __doc_operations_research_Solver_MakeLocalSearchPhase_2 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeLocalSearchPhase_3 =
    R"doc(Variant with a sub_decison_builder specific to the first solution.)doc";

static const char* __doc_operations_research_Solver_MakeLocalSearchPhase_4 =
    R"doc()doc";

static const char*
    __doc_operations_research_Solver_MakeLocalSearchPhaseParameters =
        R"doc(Local Search Phase Parameters)doc";

static const char*
    __doc_operations_research_Solver_MakeLocalSearchPhaseParameters_2 =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_MakeLocalSearchPhaseParameters_3 =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_MakeLocalSearchPhaseParameters_4 =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_MakeLocalSearchPhaseParameters_5 =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_MakeLocalSearchPhaseParameters_6 =
        R"doc()doc";

static const char* __doc_operations_research_Solver_MakeLubyRestart =
    R"doc(This search monitor will restart the search periodically. At the
iteration n, it will restart after scale_factor * Luby(n) failures
where Luby is the Luby Strategy (i.e. 1 1 2 1 1 2 4 1 1 2 1 1 2 4
8...).)doc";

static const char* __doc_operations_research_Solver_MakeMapDomain =
    R"doc(This constraint maps the domain of 'var' onto the array of variables
'actives'. That is for all i in [0 .. size - 1]: actives[i] == 1 <=>
var->Contains(i);)doc";

static const char* __doc_operations_research_Solver_MakeMax =
    R"doc(std::max(vars))doc";

static const char* __doc_operations_research_Solver_MakeMax_2 =
    R"doc(std::max(left, right))doc";

static const char* __doc_operations_research_Solver_MakeMax_3 =
    R"doc(std::max(expr, value))doc";

static const char* __doc_operations_research_Solver_MakeMax_4 =
    R"doc(std::max(expr, value))doc";

static const char* __doc_operations_research_Solver_MakeMaxEquality =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeMaximize =
    R"doc(Creates a maximization objective.)doc";

static const char* __doc_operations_research_Solver_MakeMemberCt =
    R"doc(expr in set. Propagation is lazy, i.e. this constraint does not
creates holes in the domain of the variable.)doc";

static const char* __doc_operations_research_Solver_MakeMemberCt_2 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeMin =
    R"doc(std::min(vars))doc";

static const char* __doc_operations_research_Solver_MakeMin_2 =
    R"doc(std::min (left, right))doc";

static const char* __doc_operations_research_Solver_MakeMin_3 =
    R"doc(std::min(expr, value))doc";

static const char* __doc_operations_research_Solver_MakeMin_4 =
    R"doc(std::min(expr, value))doc";

static const char* __doc_operations_research_Solver_MakeMinEquality =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeMinimize =
    R"doc(Creates a minimization objective.)doc";

static const char* __doc_operations_research_Solver_MakeMirrorInterval =
    R"doc(Creates an interval var that is the mirror image of the given one,
that is, the interval var obtained by reversing the axis.)doc";

static const char* __doc_operations_research_Solver_MakeModulo =
    R"doc(Modulo expression x % mod (with the python convention for modulo).)doc";

static const char* __doc_operations_research_Solver_MakeModulo_2 =
    R"doc(Modulo expression x % mod (with the python convention for modulo).)doc";

static const char* __doc_operations_research_Solver_MakeMonotonicElement =
    R"doc(Function based element. The constraint takes ownership of the
callback. The callback must be monotonic. It must be able to cope with
any possible value in the domain of 'index' (potentially negative ones
too). Furtermore, monotonicity is not checked. Thus giving a non-
monotonic function, or specifying an incorrect increasing parameter
will result in undefined behavior.)doc";

static const char* __doc_operations_research_Solver_MakeMoveTowardTargetOperator =
    R"doc(Creates a local search operator that tries to move the assignment of
some variables toward a target. The target is given as an Assignment.
This operator generates neighbors in which the only difference
compared to the current state is that one variable that belongs to the
target assignment is set to its target value.)doc";

static const char*
    __doc_operations_research_Solver_MakeMoveTowardTargetOperator_2 =
        R"doc(Creates a local search operator that tries to move the assignment of
some variables toward a target. The target is given either as two
vectors: a vector of variables and a vector of associated target
values. The two vectors should be of the same length. This operator
generates neighbors in which the only difference compared to the
current state is that one variable that belongs to the given vector is
set to its target value.)doc";

static const char*
    __doc_operations_research_Solver_MakeNBestLexicographicValueSolutionCollector =
        R"doc(Same as above but supporting lexicographic objectives; 'maximize'
specifies the optimization direction for each objective.)doc";

static const char*
    __doc_operations_research_Solver_MakeNBestLexicographicValueSolutionCollector_2 =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_MakeNBestValueSolutionCollector =
        R"doc(Same as MakeBestValueSolutionCollector but collects the best
solution_count solutions. Collected solutions are sorted in increasing
optimality order (the best solution is the last one).)doc";

static const char*
    __doc_operations_research_Solver_MakeNBestValueSolutionCollector_2 =
        R"doc()doc";

static const char* __doc_operations_research_Solver_MakeNeighborhoodLimit =
    R"doc(Creates a local search operator that wraps another local search
operator and limits the number of neighbors explored (i.e., calls to
MakeNextNeighbor from the current solution (between two calls to
Start()). When this limit is reached, MakeNextNeighbor() returns
false. The counter is cleared when Start() is called.)doc";

static const char* __doc_operations_research_Solver_MakeNestedOptimize =
    R"doc(NestedOptimize will collapse a search tree described by a decision
builder 'db' and a set of monitors and wrap it into a single point. If
there are no solutions to this nested tree, then NestedOptimize will
fail. If there are solutions, it will find the best as described by
the mandatory objective in the solution as well as the optimization
direction, instantiate all variables to this solution, and return
nullptr.)doc";

static const char* __doc_operations_research_Solver_MakeNestedOptimize_2 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeNestedOptimize_3 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeNestedOptimize_4 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeNestedOptimize_5 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeNestedOptimize_6 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeNoCycle =
    R"doc(Prevent cycles. The "nexts" variables represent the next in the chain.
"active" variables indicate if the corresponding next variable is
active; this could be useful to model unperformed nodes in a routing
problem. A callback can be added to specify sink values (by default
sink values are values >= vars.size()). Ownership of the callback is
passed to the constraint. If assume_paths is either not specified or
true, the constraint assumes the "nexts" variables represent paths
(and performs a faster propagation); otherwise the constraint assumes
they represent a forest.)doc";

static const char* __doc_operations_research_Solver_MakeNoCycle_2 = R"doc()doc";

static const char* __doc_operations_research_Solver_MakeNonEquality =
    R"doc(left != right)doc";

static const char* __doc_operations_research_Solver_MakeNonEquality_2 =
    R"doc(expr != value)doc";

static const char* __doc_operations_research_Solver_MakeNonEquality_3 =
    R"doc(expr != value)doc";

static const char*
    __doc_operations_research_Solver_MakeNonOverlappingBoxesConstraint =
        R"doc(This constraint states that all the boxes must not overlap. The
coordinates of box i are: (x_vars[i], y_vars[i]), (x_vars[i],
y_vars[i] + y_size[i]), (x_vars[i] + x_size[i], y_vars[i]), (x_vars[i]
+ x_size[i], y_vars[i] + y_size[i]). The sizes must be non-negative.
Boxes with a zero dimension can be pushed like any box.)doc";

static const char*
    __doc_operations_research_Solver_MakeNonOverlappingBoxesConstraint_2 =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_MakeNonOverlappingBoxesConstraint_3 =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_MakeNonOverlappingNonStrictBoxesConstraint =
        R"doc(This constraint states that all the boxes must not overlap. The
coordinates of box i are: (x_vars[i], y_vars[i]), (x_vars[i],
y_vars[i] + y_size[i]), (x_vars[i] + x_size[i], y_vars[i]), (x_vars[i]
+ x_size[i], y_vars[i] + y_size[i]). The sizes must be positive. Boxes
with a zero dimension can be placed anywhere.)doc";

static const char*
    __doc_operations_research_Solver_MakeNonOverlappingNonStrictBoxesConstraint_2 =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_MakeNonOverlappingNonStrictBoxesConstraint_3 =
        R"doc()doc";

static const char* __doc_operations_research_Solver_MakeNotBetweenCt =
    R"doc((expr < l || expr > u) This constraint is lazy as it will not make
holes in the domain of variables. It will propagate only when
expr->Min() >= l or expr->Max() <= u.)doc";

static const char* __doc_operations_research_Solver_MakeNotMemberCt =
    R"doc(expr not in set.)doc";

static const char* __doc_operations_research_Solver_MakeNotMemberCt_2 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeNotMemberCt_3 =
    R"doc(expr should not be in the list of forbidden intervals
[start[i]..end[i]].)doc";

static const char* __doc_operations_research_Solver_MakeNotMemberCt_4 =
    R"doc(expr should not be in the list of forbidden intervals
[start[i]..end[i]].)doc";

static const char* __doc_operations_research_Solver_MakeNotMemberCt_5 =
    R"doc(expr should not be in the list of forbidden intervals.)doc";

static const char* __doc_operations_research_Solver_MakeNullIntersect =
    R"doc(Creates a constraint that states that all variables in the first
vector are different from all variables in the second group. Thus the
set of values in the first vector does not intersect with the set of
values in the second vector.)doc";

static const char* __doc_operations_research_Solver_MakeNullIntersectExcept =
    R"doc(Creates a constraint that states that all variables in the first
vector are different from all variables from the second group, unless
they are assigned to the escape value. Thus the set of values in the
first vector minus the escape value does not intersect with the set of
values in the second vector.)doc";

static const char* __doc_operations_research_Solver_MakeOperator =
    R"doc(Local Search Operators.)doc";

static const char* __doc_operations_research_Solver_MakeOperator_2 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeOperator_3 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeOperator_4 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeOpposite =
    R"doc(-expr)doc";

static const char* __doc_operations_research_Solver_MakeOptimize =
    R"doc(Creates a objective with a given sense (true = maximization).)doc";

static const char* __doc_operations_research_Solver_MakePack =
    R"doc(This constraint packs all variables onto 'number_of_bins' variables.
For any given variable, a value of 'number_of_bins' indicates that the
variable is not assigned to any bin. Dimensions, i.e., cumulative
constraints on this packing, can be added directly from the pack
class.)doc";

static const char* __doc_operations_research_Solver_MakePathConnected =
    R"doc(Check whether more propagation is needed.)doc";

static const char* __doc_operations_research_Solver_MakePathCumul =
    R"doc(Creates a constraint which accumulates values along a path such that:
cumuls[next[i]] = cumuls[i] + transits[i]. Active variables indicate
if the corresponding next variable is active; this could be useful to
model unperformed nodes in a routing problem.)doc";

static const char* __doc_operations_research_Solver_MakePathCumul_2 =
    R"doc(Creates a constraint which accumulates values along a path such that:
cumuls[next[i]] = cumuls[i] + transit_evaluator(i, next[i]). Active
variables indicate if the corresponding next variable is active; this
could be useful to model unperformed nodes in a routing problem.
Ownership of transit_evaluator is taken and it must be a repeatable
callback.)doc";

static const char* __doc_operations_research_Solver_MakePathCumul_3 =
    R"doc(Creates a constraint which accumulates values along a path such that:
cumuls[next[i]] = cumuls[i] + transit_evaluator(i, next[i]) +
slacks[i]. Active variables indicate if the corresponding next
variable is active; this could be useful to model unperformed nodes in
a routing problem. Ownership of transit_evaluator is taken and it must
be a repeatable callback.)doc";

static const char*
    __doc_operations_research_Solver_MakePathEnergyCostConstraint = R"doc()doc";

static const char*
    __doc_operations_research_Solver_MakePathPrecedenceConstraint =
        R"doc(the implementation can easily be modified to do that; evaluate the
impact on models solved with local search.)doc";

static const char*
    __doc_operations_research_Solver_MakePathPrecedenceConstraint_2 =
        R"doc(Same as MakePathPrecedenceConstraint but ensures precedence pairs on
some paths follow a LIFO or FIFO order. LIFO order: given 2 pairs
(a,b) and (c,d), if a is before c on the path then d must be before b
or b must be before c. FIFO order: given 2 pairs (a,b) and (c,d), if a
is before c on the path then b must be before d. LIFO (resp. FIFO)
orders are enforced only on paths starting by indices in
lifo_path_starts (resp. fifo_path_start).)doc";

static const char*
    __doc_operations_research_Solver_MakePathTransitPrecedenceConstraint =
        R"doc(Same as MakePathPrecedenceConstraint but will force i to be before j
if the sum of transits on the path from i to j is strictly positive.)doc";

static const char* __doc_operations_research_Solver_MakePhase =
    R"doc(for all other functions that have several homonyms in this .h).)doc";

static const char* __doc_operations_research_Solver_MakePhase_2 = R"doc()doc";

static const char* __doc_operations_research_Solver_MakePhase_3 = R"doc()doc";

static const char* __doc_operations_research_Solver_MakePhase_4 =
    R"doc(var_val1_val2_comparator(var, val1, val2) is true iff assigning value
"val1" to variable "var" is better than assigning value "val2".)doc";

static const char* __doc_operations_research_Solver_MakePhase_5 = R"doc()doc";

static const char* __doc_operations_research_Solver_MakePhase_6 = R"doc()doc";

static const char* __doc_operations_research_Solver_MakePhase_7 = R"doc()doc";

static const char* __doc_operations_research_Solver_MakePhase_8 =
    R"doc(Shortcuts for small arrays.)doc";

static const char* __doc_operations_research_Solver_MakePhase_9 = R"doc()doc";

static const char* __doc_operations_research_Solver_MakePhase_10 = R"doc()doc";

static const char* __doc_operations_research_Solver_MakePhase_11 = R"doc()doc";

static const char* __doc_operations_research_Solver_MakePhase_12 =
    R"doc(Returns a decision builder which assigns values to variables which
minimize the values returned by the evaluator. The arguments passed to
the evaluator callback are the indices of the variables in vars and
the values of these variables. Ownership of the callback is passed to
the decision builder.)doc";

static const char* __doc_operations_research_Solver_MakePhase_13 =
    R"doc(Returns a decision builder which assigns values to variables which
minimize the values returned by the evaluator. In case of tie breaks,
the second callback is used to choose the best index in the array of
equivalent pairs with equivalent evaluations. The arguments passed to
the evaluator callback are the indices of the variables in vars and
the values of these variables. Ownership of the callback is passed to
the decision builder.)doc";

static const char* __doc_operations_research_Solver_MakePhase_14 =
    R"doc(Scheduling phases.)doc";

static const char* __doc_operations_research_Solver_MakePhase_15 = R"doc()doc";

static const char* __doc_operations_research_Solver_MakePiecewiseLinearExpr =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakePower =
    R"doc(expr ^ n (n > 0))doc";

static const char* __doc_operations_research_Solver_MakePrintModelVisitor =
    R"doc(Prints the model.)doc";

static const char* __doc_operations_research_Solver_MakeProd =
    R"doc(left * right)doc";

static const char* __doc_operations_research_Solver_MakeProd_2 =
    R"doc(expr * value)doc";

static const char*
    __doc_operations_research_Solver_MakeProfiledDecisionBuilderWrapper =
        R"doc(Activates profiling on a decision builder.)doc";

static const char* __doc_operations_research_Solver_MakeRandomLnsOperator =
    R"doc(Creates a large neighborhood search operator which creates fragments
(set of relaxed variables) with up to number_of_variables random
variables (sampling with replacement is performed meaning that at most
number_of_variables variables are selected). Warning: this operator
will always return neighbors; using it without a search limit will
result in a non-ending search. Optionally a random seed can be
specified.)doc";

static const char* __doc_operations_research_Solver_MakeRandomLnsOperator_2 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeRankFirstInterval =
    R"doc(Returns a decision that tries to rank first the ith interval var in
the sequence variable.)doc";

static const char* __doc_operations_research_Solver_MakeRankLastInterval =
    R"doc(Returns a decision that tries to rank last the ith interval var in the
sequence variable.)doc";

static const char* __doc_operations_research_Solver_MakeRejectFilter =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeRestoreAssignment =
    R"doc(Returns a DecisionBuilder which restores an Assignment (calls void
Assignment::Restore()))doc";

static const char* __doc_operations_research_Solver_MakeScalProd =
    R"doc(scalar product)doc";

static const char* __doc_operations_research_Solver_MakeScalProd_2 =
    R"doc(scalar product)doc";

static const char* __doc_operations_research_Solver_MakeScalProdEquality =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeScalProdEquality_2 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeScalProdEquality_3 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeScalProdEquality_4 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeScalProdGreaterOrEqual =
    R"doc()doc";

static const char*
    __doc_operations_research_Solver_MakeScalProdGreaterOrEqual_2 = R"doc()doc";

static const char* __doc_operations_research_Solver_MakeScalProdLessOrEqual =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeScalProdLessOrEqual_2 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeScheduleOrExpedite =
    R"doc(Returns a decision that tries to schedule a task at a given time. On
the Apply branch, it will set that interval var as performed and set
its end to 'est'. On the Refute branch, it will just update the
'marker' to 'est' - 1. This decision is used in the
INTERVAL_SET_TIMES_BACKWARD strategy.)doc";

static const char* __doc_operations_research_Solver_MakeScheduleOrPostpone =
    R"doc(Returns a decision that tries to schedule a task at a given time. On
the Apply branch, it will set that interval var as performed and set
its start to 'est'. On the Refute branch, it will just update the
'marker' to 'est' + 1. This decision is used in the
INTERVAL_SET_TIMES_FORWARD strategy.)doc";

static const char* __doc_operations_research_Solver_MakeSearchLog =
    R"doc(The SearchMonitors below will display a periodic search log on
LOG(INFO) every branch_period branches explored.)doc";

static const char* __doc_operations_research_Solver_MakeSearchLog_2 =
    R"doc(At each solution, this monitor also display the var value.)doc";

static const char* __doc_operations_research_Solver_MakeSearchLog_3 =
    R"doc(At each solution, this monitor will also display result of @p
display_callback.)doc";

static const char* __doc_operations_research_Solver_MakeSearchLog_4 =
    R"doc(At each solution, this monitor will display the 'var' value and the
result of @p display_callback.)doc";

static const char* __doc_operations_research_Solver_MakeSearchLog_5 =
    R"doc(At each solution, this monitor will display the 'vars' values and the
result of @p display_callback.)doc";

static const char* __doc_operations_research_Solver_MakeSearchLog_6 =
    R"doc(OptimizeVar Search Logs At each solution, this monitor will also
display the 'opt_var' value.)doc";

static const char* __doc_operations_research_Solver_MakeSearchLog_7 =
    R"doc(Creates a search monitor that will also print the result of the
display callback.)doc";

static const char* __doc_operations_research_Solver_MakeSearchLog_8 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeSearchProgressBar =
    R"doc(Creates a search monitor tracking the progress of the search in a
progress bar. If a search limit is specified in the search, the bar
shows the progress percentage before reaching the limit. If no limit
is specified, an activity bar is displayed.)doc";

static const char* __doc_operations_research_Solver_MakeSearchTrace =
    R"doc(Creates a search monitor that will trace precisely the behavior of the
search. Use this only for low level debugging.)doc";

static const char* __doc_operations_research_Solver_MakeSemiContinuousExpr =
    R"doc(Semi continuous Expression (x <= 0 -> f(x) = 0; x > 0 -> f(x) = ax +
b) a >= 0 and b >= 0)doc";

static const char* __doc_operations_research_Solver_MakeSimulatedAnnealing =
    R"doc(Creates a Simulated Annealing monitor.)doc";

static const char* __doc_operations_research_Solver_MakeSolveOnce =
    R"doc(SolveOnce will collapse a search tree described by a decision builder
'db' and a set of monitors and wrap it into a single point. If there
are no solutions to this nested tree, then SolveOnce will fail. If
there is a solution, it will find it and returns nullptr.)doc";

static const char* __doc_operations_research_Solver_MakeSolveOnce_2 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeSolveOnce_3 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeSolveOnce_4 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeSolveOnce_5 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeSolveOnce_6 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeSortingConstraint =
    R"doc(Creates a constraint binding the arrays of variables "vars" and
"sorted_vars": sorted_vars[0] must be equal to the minimum of all
variables in vars, and so on: the value of sorted_vars[i] must be
equal to the i-th value of variables invars.

This constraint propagates in both directions: from "vars" to
"sorted_vars" and vice-versa.

Behind the scenes, this constraint maintains that: - sorted is always
increasing. - whatever the values of vars, there exists a permutation
that injects its values into the sorted variables.

For more info, please have a look at: https://mpi-
inf.mpg.de/~mehlhorn/ftp/Mehlhorn-Thiel.pdf)doc";

static const char* __doc_operations_research_Solver_MakeSplitVariableDomain =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeSquare =
    R"doc(expr * expr)doc";

static const char* __doc_operations_research_Solver_MakeStatisticsModelVisitor =
    R"doc(Displays some nice statistics on the model.)doc";

static const char* __doc_operations_research_Solver_MakeStoreAssignment =
    R"doc(Returns a DecisionBuilder which stores an Assignment (calls void
Assignment::Store()))doc";

static const char*
    __doc_operations_research_Solver_MakeStrictDisjunctiveConstraint =
        R"doc(This constraint forces all interval vars into an non-overlapping
sequence. Intervals with zero durations cannot overlap with over
intervals.)doc";

static const char* __doc_operations_research_Solver_MakeSubCircuit =
    R"doc(Force the "nexts" variable to create a complete Hamiltonian path for
those that do not loop upon themselves.)doc";

static const char* __doc_operations_research_Solver_MakeSum =
    R"doc(left + right.)doc";

static const char* __doc_operations_research_Solver_MakeSum_2 =
    R"doc(expr + value.)doc";

static const char* __doc_operations_research_Solver_MakeSum_3 =
    R"doc(sum of all vars.)doc";

static const char* __doc_operations_research_Solver_MakeSumEquality =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeSumEquality_2 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeSumGreaterOrEqual =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeSumLessOrEqual =
    R"doc(Variation on arrays.)doc";

static const char* __doc_operations_research_Solver_MakeSumObjectiveFilter =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeSumObjectiveFilter_2 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeSymmetryManager =
    R"doc(Symmetry Breaking.)doc";

static const char* __doc_operations_research_Solver_MakeSymmetryManager_2 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeSymmetryManager_3 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeSymmetryManager_4 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeSymmetryManager_5 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MakeTabuSearch =
    R"doc(Creates a Tabu Search monitor. In the context of local search the
behavior is similar to MakeOptimize(), creating an objective in a
given sense. The behavior differs once a local optimum is reached:
thereafter solutions which degrade the value of the objective are
allowed if they are not "tabu". A solution is "tabu" if it doesn't
respect the following rules: - improving the best solution found so
far - variables in the "keep" list must keep their value, variables in
the "forbid" list must not take the value they have in the list.
Variables with new values enter the tabu lists after each new solution
found and leave the lists after a given number of iterations (called
tenure). Only the variables passed to the method can enter the lists.
The tabu criterion is softened by the tabu factor which gives the
number of "tabu" violations which is tolerated; a factor of 1 means no
violations allowed; a factor of 0 means all violations are allowed.)doc";

static const char* __doc_operations_research_Solver_MakeTemporalDisjunction =
    R"doc(This constraint implements a temporal disjunction between two interval
vars t1 and t2. 'alt' indicates which alternative was chosen (alt == 0
is equivalent to t1 before t2).)doc";

static const char* __doc_operations_research_Solver_MakeTemporalDisjunction_2 =
    R"doc(This constraint implements a temporal disjunction between two interval
vars.)doc";

static const char* __doc_operations_research_Solver_MakeTransitionConstraint =
    R"doc(This constraint create a finite automaton that will check the sequence
of variables vars. It uses a transition table called
'transition_table'. Each transition is a triple (current_state,
variable_value, new_state). The initial state is given, and the set of
accepted states is decribed by 'final_states'. These states are hidden
inside the constraint. Only the transitions (i.e. the variables) are
visible.)doc";

static const char* __doc_operations_research_Solver_MakeTransitionConstraint_2 =
    R"doc(This constraint create a finite automaton that will check the sequence
of variables vars. It uses a transition table called
'transition_table'. Each transition is a triple (current_state,
variable_value, new_state). The initial state is given, and the set of
accepted states is decribed by 'final_states'. These states are hidden
inside the constraint. Only the transitions (i.e. the variables) are
visible.)doc";

static const char* __doc_operations_research_Solver_MakeTrueConstraint =
    R"doc(This constraint always succeeds.)doc";

static const char* __doc_operations_research_Solver_MakeVariableDegreeVisitor =
    R"doc(Compute the number of constraints a variable is attached to.)doc";

static const char* __doc_operations_research_Solver_MakeVariableDomainFilter =
    R"doc()doc";

static const char*
    __doc_operations_research_Solver_MakeVariableGreaterOrEqualValue =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_MakeVariableLessOrEqualValue = R"doc()doc";

static const char* __doc_operations_research_Solver_MakeWeightedMaximize =
    R"doc(Creates a maximization weigthed objective.)doc";

static const char* __doc_operations_research_Solver_MakeWeightedMaximize_2 =
    R"doc(Creates a maximization weigthed objective.)doc";

static const char* __doc_operations_research_Solver_MakeWeightedMinimize =
    R"doc(Creates a minimization weighted objective. The actual objective is
scalar_prod(sub_objectives, weights).)doc";

static const char* __doc_operations_research_Solver_MakeWeightedMinimize_2 =
    R"doc(Creates a minimization weighted objective. The actual objective is
scalar_prod(sub_objectives, weights).)doc";

static const char* __doc_operations_research_Solver_MakeWeightedOptimize =
    R"doc(Creates a weighted objective with a given sense (true = maximization).)doc";

static const char* __doc_operations_research_Solver_MakeWeightedOptimize_2 =
    R"doc(Creates a weighted objective with a given sense (true = maximization).)doc";

static const char* __doc_operations_research_Solver_MarkerType =
    R"doc(This enum is used internally in private methods Solver::PushState and
Solver::PopState to tag states in the search tree.)doc";

static const char* __doc_operations_research_Solver_MarkerType_CHOICE_POINT =
    R"doc()doc";

static const char*
    __doc_operations_research_Solver_MarkerType_REVERSIBLE_ACTION = R"doc()doc";

static const char* __doc_operations_research_Solver_MarkerType_SENTINEL =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MarkerType_SIMPLE_MARKER =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MemoryUsage =
    R"doc(Current memory usage in bytes)doc";

static const char* __doc_operations_research_Solver_MonitorEvent =
    R"doc(Search monitor events.)doc";

static const char* __doc_operations_research_Solver_MonitorEvent_kAccept =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MonitorEvent_kAcceptDelta =
    R"doc()doc";

static const char*
    __doc_operations_research_Solver_MonitorEvent_kAcceptNeighbor = R"doc()doc";

static const char*
    __doc_operations_research_Solver_MonitorEvent_kAcceptSolution = R"doc()doc";

static const char*
    __doc_operations_research_Solver_MonitorEvent_kAcceptUncheckedNeighbor =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_MonitorEvent_kAfterDecision = R"doc()doc";

static const char*
    __doc_operations_research_Solver_MonitorEvent_kApplyDecision = R"doc()doc";

static const char* __doc_operations_research_Solver_MonitorEvent_kAtSolution =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MonitorEvent_kBeginFail =
    R"doc()doc";

static const char*
    __doc_operations_research_Solver_MonitorEvent_kBeginInitialPropagation =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_MonitorEvent_kBeginNextDecision =
        R"doc()doc";

static const char* __doc_operations_research_Solver_MonitorEvent_kEndFail =
    R"doc()doc";

static const char*
    __doc_operations_research_Solver_MonitorEvent_kEndInitialPropagation =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_MonitorEvent_kEndNextDecision =
        R"doc()doc";

static const char* __doc_operations_research_Solver_MonitorEvent_kEnterSearch =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MonitorEvent_kExitSearch =
    R"doc()doc";

static const char*
    __doc_operations_research_Solver_MonitorEvent_kIsUncheckedSolutionLimitReached =
        R"doc()doc";

static const char* __doc_operations_research_Solver_MonitorEvent_kLast =
    R"doc()doc";

static const char* __doc_operations_research_Solver_MonitorEvent_kLocalOptimum =
    R"doc()doc";

static const char*
    __doc_operations_research_Solver_MonitorEvent_kNoMoreSolutions =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_MonitorEvent_kPeriodicCheck = R"doc()doc";

static const char*
    __doc_operations_research_Solver_MonitorEvent_kProgressPercent =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_MonitorEvent_kRefuteDecision = R"doc()doc";

static const char*
    __doc_operations_research_Solver_MonitorEvent_kRestartSearch = R"doc()doc";

static const char*
    __doc_operations_research_Solver_MultiArmedBanditConcatenateOperators =
        R"doc(Creates a local search operator which concatenates a vector of
operators. Uses Multi-Armed Bandit approach for choosing the next
operator to use. Sorts operators based on Upper Confidence Bound
Algorithm which evaluates each operator as sum of average improvement
and exploration function.

Updates the order of operators when accepts a neighbor with objective
improvement.)doc";

static const char* __doc_operations_research_Solver_NameAllVariables =
    R"doc(Returns whether all variables should be named.)doc";

static const char* __doc_operations_research_Solver_NewSearch =
    R"doc(@{ Decomposed search. The code for a top level search should look like
solver->NewSearch(db); while (solver->NextSolution()) { //.. use the
current solution } solver()->EndSearch();)doc";

static const char* __doc_operations_research_Solver_NewSearch_2 = R"doc()doc";

static const char* __doc_operations_research_Solver_NewSearch_3 = R"doc()doc";

static const char* __doc_operations_research_Solver_NewSearch_4 = R"doc()doc";

static const char* __doc_operations_research_Solver_NewSearch_5 = R"doc()doc";

static const char* __doc_operations_research_Solver_NewSearch_6 = R"doc()doc";

static const char* __doc_operations_research_Solver_NextSolution = R"doc()doc";

static const char* __doc_operations_research_Solver_Now =
    R"doc(The 'absolute time' as seen by the solver. Unless a user-provided
clock was injected via SetClock() (eg. for unit tests), this is a real
walltime, shifted so that it was 0 at construction. All so-called
"walltime" limits are relative to this time.)doc";

static const char* __doc_operations_research_Solver_OptimizationDirection =
    R"doc(Optimization directions.)doc";

static const char*
    __doc_operations_research_Solver_OptimizationDirection_MAXIMIZATION =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_OptimizationDirection_MINIMIZATION =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_OptimizationDirection_NOT_SET =
        R"doc()doc";

static const char* __doc_operations_research_Solver_ParentSearch =
    R"doc(Returns the Search object which is the parent of the active search,
i.e., the search below the top of the stack. If the active search is
at the bottom of the stack, returns the active search.)doc";

static const char*
    __doc_operations_research_Solver_PathEnergyCostConstraintSpecification =
        R"doc(A constraint that maintains the energy cost of paths. Energy is the
integral of force applied over distance. More formally, the energy
used on a path is: energy[path] = sum(node | paths[node] == path /\
node not end) forces[next[node]] * distances[node] where forces[n] is
the force needed to move loads accumulated until, but excluding weight
and distances[n] is the distance from n to its successor. For
instance, if a path has a route with two pickup/delivery pairs where
the first shipment weighs 1 unit, the second weighs 2 units, and the
distance between nodes is one, the {force/distance} of nodes would be:
start{0/1} P1{0/1} P2{1/1} D1{3/1} D2{2/1} end{0/0}. The energy would
be 0*1 + 1*1 + 3*1 + 2*1 + 0*1. The cost per unit of energy is
cost_per_unit_below_threshold until the force reaches the threshold,
then it is cost_per_unit_above_threshold: min(threshold,
force.CumulVar(Next(node))) * distance.TransitVar(node) *
cost_per_unit_below_threshold + max(0, force.CumulVar(Next(node)) -
threshold) * distance.TransitVar(node) *
cost_per_unit_above_threshold.)doc";

static const char*
    __doc_operations_research_Solver_PathEnergyCostConstraintSpecification_EnergyCost =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_PathEnergyCostConstraintSpecification_EnergyCost_IsNull =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_PathEnergyCostConstraintSpecification_EnergyCost_cost_per_unit_above_threshold =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_PathEnergyCostConstraintSpecification_EnergyCost_cost_per_unit_below_threshold =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_PathEnergyCostConstraintSpecification_EnergyCost_threshold =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_PathEnergyCostConstraintSpecification_costs =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_PathEnergyCostConstraintSpecification_distances =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_PathEnergyCostConstraintSpecification_forces =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_PathEnergyCostConstraintSpecification_nexts =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_PathEnergyCostConstraintSpecification_path_ends =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_PathEnergyCostConstraintSpecification_path_energy_costs =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_PathEnergyCostConstraintSpecification_path_starts =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_PathEnergyCostConstraintSpecification_path_used_when_empty =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_PathEnergyCostConstraintSpecification_paths =
        R"doc()doc";

static const char* __doc_operations_research_Solver_PopState = R"doc()doc";

static const char* __doc_operations_research_Solver_PopState_2 = R"doc()doc";

static const char* __doc_operations_research_Solver_ProcessConstraints =
    R"doc()doc";

static const char* __doc_operations_research_Solver_PushSentinel = R"doc()doc";

static const char* __doc_operations_research_Solver_PushState =
    R"doc(The PushState and PopState methods manipulates the states of the
reversible objects. They are visible only because they are useful to
write unitary tests.)doc";

static const char* __doc_operations_research_Solver_PushState_2 =
    R"doc(Initialization. To be called by the constructors only.)doc";

static const char* __doc_operations_research_Solver_Rand32 =
    R"doc(Returns a random value between 0 and 'size' - 1;)doc";

static const char* __doc_operations_research_Solver_Rand64 =
    R"doc(Returns a random value between 0 and 'size' - 1;)doc";

static const char* __doc_operations_research_Solver_RandomConcatenateOperators =
    R"doc(Randomized version of local search concatenator; calls a random
operator at each call to MakeNextNeighbor().)doc";

static const char*
    __doc_operations_research_Solver_RandomConcatenateOperators_2 =
        R"doc(Randomized version of local search concatenator; calls a random
operator at each call to MakeNextNeighbor(). The provided seed is used
to initialize the random number generator.)doc";

static const char* __doc_operations_research_Solver_ReSeed =
    R"doc(Reseed the solver random generator.)doc";

static const char* __doc_operations_research_Solver_RegisterDemon =
    R"doc(Adds a new demon and wraps it inside a DemonProfiler if necessary.)doc";

static const char* __doc_operations_research_Solver_RegisterIntExpr =
    R"doc(Registers a new IntExpr and wraps it inside a TraceIntExpr if
necessary.)doc";

static const char* __doc_operations_research_Solver_RegisterIntVar =
    R"doc(Registers a new IntVar and wraps it inside a TraceIntVar if necessary.)doc";

static const char* __doc_operations_research_Solver_RegisterIntervalVar =
    R"doc(Registers a new IntervalVar and wraps it inside a TraceIntervalVar if
necessary.)doc";

static const char* __doc_operations_research_Solver_RegularLimit =
    R"doc(Creates a search limit that constrains the running time.)doc";

static const char* __doc_operations_research_Solver_RestartCurrentSearch =
    R"doc()doc";

static const char* __doc_operations_research_Solver_RestartSearch = R"doc()doc";

static const char* __doc_operations_research_Solver_RevAlloc =
    R"doc(Registers the given object as being reversible. By calling this
method, the caller gives ownership of the object to the solver, which
will delete it when there is a backtrack out of the current state.

Returns the argument for convenience: this way, the caller may
directly invoke a constructor in the argument, without having to store
the pointer first.

This function is only for users that define their own subclasses of
BaseObject: for all subclasses predefined in the library, the
corresponding factory methods (e.g., MakeIntVar(...),
MakeAllDifferent(...) already take care of the registration.)doc";

static const char* __doc_operations_research_Solver_RevAllocArray =
    R"doc(Like RevAlloc() above, but for an array of objects: the array must
have been allocated with the new[] operator. The entire array will be
deleted when backtracking out of the current state.

This method is valid for arrays of int, int64_t, uint64_t, bool,
BaseObject*, IntVar*, IntExpr*, and Constraint*.)doc";

static const char* __doc_operations_research_Solver_RunUncheckedLocalSearch =
    R"doc(Experimental: runs a local search on the given initial solution,
checking the feasibility and the objective value of solutions using
the filter manager only (solutions are never restored in the CP
world). Only greedy descent is supported.)doc";

static const char*
    __doc_operations_research_Solver_RunUncheckedLocalSearchInternal =
        R"doc()doc";

static const char* __doc_operations_research_Solver_SafeRevAlloc = R"doc()doc";

static const char* __doc_operations_research_Solver_SafeRevAllocArray =
    R"doc()doc";

static const char* __doc_operations_research_Solver_SafeRevAllocArray_2 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_SafeRevAllocArray_3 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_SafeRevAllocArray_4 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_SafeRevAllocArray_5 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_SafeRevAllocArray_6 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_SafeRevAllocArray_7 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_SafeRevAllocArray_8 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_SaveAndAdd =
    R"doc(All-in-one SaveAndAdd_value.)doc";

static const char* __doc_operations_research_Solver_SaveAndSetValue =
    R"doc(All-in-one SaveAndSetValue.)doc";

static const char* __doc_operations_research_Solver_SaveValue =
    R"doc(SaveValue() saves the value of the corresponding object. It must be
called before modifying the object. The value will be restored upon
backtrack.)doc";

static const char* __doc_operations_research_Solver_SearchContext = R"doc()doc";

static const char* __doc_operations_research_Solver_SearchContext_2 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_SearchDepth =
    R"doc(Gets the search depth of the current active search. Returns -1 if
there is no active search opened.)doc";

static const char* __doc_operations_research_Solver_SearchLeftDepth =
    R"doc(Gets the search left depth of the current active search. Returns -1 if
there is no active search opened.)doc";

static const char* __doc_operations_research_Solver_SearchLimit =
    R"doc(Creates a search limit that is reached when either of the underlying
limit is reached. That is, the returned limit is more stringent than
both argument limits.)doc";

static const char* __doc_operations_research_Solver_SearchLogParameters =
    R"doc(Creates a search monitor from logging parameters.)doc";

static const char*
    __doc_operations_research_Solver_SearchLogParameters_branch_period =
        R"doc(SearchMonitors will display a periodic search log every branch_period
branches explored.)doc";

static const char*
    __doc_operations_research_Solver_SearchLogParameters_display_callback =
        R"doc(SearchMonitors will display the result of display_callback at each new
solution found and when the search finishes if
display_on_new_solutions_only is false.)doc";

static const char*
    __doc_operations_research_Solver_SearchLogParameters_display_on_new_solutions_only =
        R"doc(To be used to protect from cases where display_callback assumes
variables are instantiated, which only happens in AtSolution().)doc";

static const char*
    __doc_operations_research_Solver_SearchLogParameters_objective =
        R"doc(SearchMonitors will display values of objective or variables (both
cannot be used together).)doc";

static const char*
    __doc_operations_research_Solver_SearchLogParameters_offsets = R"doc()doc";

static const char*
    __doc_operations_research_Solver_SearchLogParameters_scaling_factors =
        R"doc(When displayed, objective or var values will be scaled and offset by
the given values in the following way: scaling_factor * (value +
offset).)doc";

static const char*
    __doc_operations_research_Solver_SearchLogParameters_variables =
        R"doc()doc";

static const char* __doc_operations_research_Solver_SequenceStrategy =
    R"doc(Used for scheduling. Not yet implemented.)doc";

static const char*
    __doc_operations_research_Solver_SequenceStrategy_CHOOSE_MIN_SLACK_RANK_FORWARD =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_SequenceStrategy_CHOOSE_RANDOM_RANK_FORWARD =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_SequenceStrategy_SEQUENCE_DEFAULT =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_SequenceStrategy_SEQUENCE_SIMPLE =
        R"doc()doc";

static const char* __doc_operations_research_Solver_SetBranchSelector =
    R"doc(Sets the given branch selector on the current active search.)doc";

static const char* __doc_operations_research_Solver_SetClock =
    R"doc(Set the clock in the timer. Does not take ownership. For dependency
injection.)doc";

static const char* __doc_operations_research_Solver_SetName = R"doc()doc";

static const char* __doc_operations_research_Solver_SetSearchContext =
    R"doc()doc";

static const char* __doc_operations_research_Solver_SetUseFastLocalSearch =
    R"doc(enabled for metaheuristics. Disables/enables fast local search.)doc";

static const char* __doc_operations_research_Solver_ShouldFail =
    R"doc(See http://cs/file:constraint_solver.swig%20ShouldFail.)doc";

static const char* __doc_operations_research_Solver_Solve =
    R"doc(@{ Solves the problem using the given DecisionBuilder and returns true
if a solution was found and accepted.

These methods are the ones most users should use to search for a
solution. Note that the definition of 'solution' is subtle. A solution
here is defined as a leaf of the search tree with respect to the given
decision builder for which there is no failure. What this means is
that, contrary to intuition, a solution may not have all variables of
the model bound. It is the responsibility of the decision builder to
keep returning decisions until all variables are indeed bound. The
most extreme counterexample is calling Solve with a trivial decision
builder whose Next() method always returns nullptr. In this case,
Solve immediately returns 'true', since not assigning any variable to
any value is a solution, unless the root node propagation discovers
that the model is infeasible.

This function must be called either from outside of search, or from
within the Next() method of a decision builder.

Solve will terminate whenever any of the following event arise: * A
search monitor asks the solver to terminate the search by calling
solver()->FinishCurrentSearch(). * A solution is found that is
accepted by all search monitors, and none of the search monitors
decides to search for another one.

Upon search termination, there will be a series of backtracks all the
way to the top level. This means that a user cannot expect to inspect
the solution by querying variables after a call to Solve(): all the
information will be lost. In order to do something with the solution,
the user must either:

* Use a search monitor that can process such a leaf. See, in
particular, the SolutionCollector class. * Do not use Solve. Instead,
use the more fine-grained approach using methods NewSearch(...),
NextSolution(), and EndSearch().

Parameter ``db``:
    The decision builder that will generate the search tree.

Parameter ``monitors``:
    A vector of search monitors that will be notified of various
    events during the search. In their reaction to these events, such
    monitors may influence the search.)doc";

static const char* __doc_operations_research_Solver_Solve_2 = R"doc()doc";

static const char* __doc_operations_research_Solver_Solve_3 = R"doc()doc";

static const char* __doc_operations_research_Solver_Solve_4 = R"doc()doc";

static const char* __doc_operations_research_Solver_Solve_5 = R"doc()doc";

static const char* __doc_operations_research_Solver_Solve_6 = R"doc()doc";

static const char* __doc_operations_research_Solver_SolveAndCommit =
    R"doc(SolveAndCommit using a decision builder and up to three search
monitors, usually one for the objective, one for the limits and one to
collect solutions.

The difference between a SolveAndCommit() and a Solve() method call is
the fact that SolveAndCommit will not backtrack all modifications at
the end of the search. This method is only usable during the Next()
method of a decision builder.)doc";

static const char* __doc_operations_research_Solver_SolveAndCommit_2 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_SolveAndCommit_3 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_SolveAndCommit_4 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_SolveAndCommit_5 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_SolveDepth =
    R"doc(Gets the number of nested searches. It returns 0 outside search, 1
during the top level search, 2 or more in case of nested searches.)doc";

static const char* __doc_operations_research_Solver_Solver =
    R"doc(Solver API)doc";

static const char* __doc_operations_research_Solver_Solver_2 = R"doc()doc";

static const char* __doc_operations_research_Solver_Solver_3 = R"doc()doc";

static const char* __doc_operations_research_Solver_SolverState =
    R"doc(This enum represents the state of the solver w.r.t. the search.)doc";

static const char* __doc_operations_research_Solver_SolverState_AT_SOLUTION =
    R"doc(After successful NextSolution and before EndSearch.)doc";

static const char* __doc_operations_research_Solver_SolverState_IN_ROOT_NODE =
    R"doc(Executing the root node.)doc";

static const char* __doc_operations_research_Solver_SolverState_IN_SEARCH =
    R"doc(Executing the search code.)doc";

static const char*
    __doc_operations_research_Solver_SolverState_NO_MORE_SOLUTIONS =
        R"doc(After failed NextSolution and before EndSearch.)doc";

static const char* __doc_operations_research_Solver_SolverState_OUTSIDE_SEARCH =
    R"doc(Before search, after search.)doc";

static const char*
    __doc_operations_research_Solver_SolverState_PROBLEM_INFEASIBLE =
        R"doc(After search, the model is infeasible.)doc";

static const char* __doc_operations_research_Solver_TopLevelSearch =
    R"doc(Returns the Search object that is at the bottom of the search stack.
Contrast with ActiveSearch(), which returns the search at the top of
the stack.)doc";

static const char* __doc_operations_research_Solver_TopPeriodicCheck =
    R"doc(Performs PeriodicCheck on the top-level search; for instance, can be
called from a nested solve to check top-level limits.)doc";

static const char* __doc_operations_research_Solver_TopProgressPercent =
    R"doc(Returns a percentage representing the propress of the search before
reaching the limits of the top-level search (can be called from a
nested solve).)doc";

static const char* __doc_operations_research_Solver_Try =
    R"doc("Try"-builders "recursively". For instance, Try(a,b,c,d) will give a
tree unbalanced to the right, whereas Try(Try(a,b), Try(b,c)) will
give a balanced tree. Investigate if we should only provide the binary
version and/or if we should balance automatically.)doc";

static const char* __doc_operations_research_Solver_Try_2 = R"doc()doc";

static const char* __doc_operations_research_Solver_Try_3 = R"doc()doc";

static const char* __doc_operations_research_Solver_Try_4 = R"doc()doc";

static const char* __doc_operations_research_Solver_UnaryIntervalRelation =
    R"doc(This enum is used in Solver::MakeIntervalVarRelation to specify the
temporal relation between an interval t and an integer d.)doc";

static const char*
    __doc_operations_research_Solver_UnaryIntervalRelation_AVOID_DATE =
        R"doc(STARTS_AFTER or ENDS_BEFORE, i.e. d is not in t. t starts after d,
i.e. Start(t) >= d. t ends before d, i.e. End(t) <= d.)doc";

static const char*
    __doc_operations_research_Solver_UnaryIntervalRelation_CROSS_DATE =
        R"doc(STARTS_BEFORE and ENDS_AFTER at the same time, i.e. d is in t. t
starts before d, i.e. Start(t) <= d. t ends after d, i.e. End(t) >= d.)doc";

static const char*
    __doc_operations_research_Solver_UnaryIntervalRelation_ENDS_AFTER =
        R"doc(t ends after d, i.e. End(t) >= d.)doc";

static const char*
    __doc_operations_research_Solver_UnaryIntervalRelation_ENDS_AT =
        R"doc(t ends at d, i.e. End(t) == d.)doc";

static const char*
    __doc_operations_research_Solver_UnaryIntervalRelation_ENDS_BEFORE =
        R"doc(t ends before d, i.e. End(t) <= d.)doc";

static const char*
    __doc_operations_research_Solver_UnaryIntervalRelation_STARTS_AFTER =
        R"doc(t starts after d, i.e. Start(t) >= d.)doc";

static const char*
    __doc_operations_research_Solver_UnaryIntervalRelation_STARTS_AT =
        R"doc(t starts at d, i.e. Start(t) == d.)doc";

static const char*
    __doc_operations_research_Solver_UnaryIntervalRelation_STARTS_BEFORE =
        R"doc(t starts before d, i.e. Start(t) <= d.)doc";

static const char* __doc_operations_research_Solver_UnfreezeQueue = R"doc()doc";

static const char* __doc_operations_research_Solver_UnsafeRevAlloc =
    R"doc()doc";

static const char* __doc_operations_research_Solver_UnsafeRevAllocArray =
    R"doc()doc";

static const char* __doc_operations_research_Solver_UnsafeRevAllocArrayAux =
    R"doc()doc";

static const char* __doc_operations_research_Solver_UnsafeRevAllocAux =
    R"doc(UnsafeRevAlloc is used internally for cells in SimpleRevFIFO and other
structures like this.)doc";

static const char* __doc_operations_research_Solver_UseFastLocalSearch =
    R"doc(Returns true if fast local search is enabled.)doc";

static const char* __doc_operations_research_Solver_VirtualMemorySize =
    R"doc(Current virtual memory size in bytes)doc";

static const char* __doc_operations_research_Solver_accepted_neighbors =
    R"doc(The number of accepted neighbors.)doc";

static const char* __doc_operations_research_Solver_accepted_neighbors_2 =
    R"doc()doc";

static const char*
    __doc_operations_research_Solver_additional_constraint_index = R"doc()doc";

static const char*
    __doc_operations_research_Solver_additional_constraints_list = R"doc()doc";

static const char*
    __doc_operations_research_Solver_additional_constraints_parent_list =
        R"doc()doc";

static const char* __doc_operations_research_Solver_anonymous_variable_index =
    R"doc()doc";

static const char* __doc_operations_research_Solver_balancing_decision =
    R"doc()doc";

static const char* __doc_operations_research_Solver_balancing_decision_2 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_branches =
    R"doc(The number of branches explored since the creation of the solver.)doc";

static const char* __doc_operations_research_Solver_branches_2 = R"doc()doc";

static const char* __doc_operations_research_Solver_cached_constants =
    R"doc()doc";

static const char* __doc_operations_research_Solver_cast_constraints =
    R"doc()doc";

static const char* __doc_operations_research_Solver_cast_information =
    R"doc()doc";

static const char* __doc_operations_research_Solver_check_alloc_state =
    R"doc()doc";

static const char* __doc_operations_research_Solver_clear_fail_intercept =
    R"doc()doc";

static const char* __doc_operations_research_Solver_const_parameters =
    R"doc()doc";

static const char* __doc_operations_research_Solver_constraint_index =
    R"doc()doc";

static const char* __doc_operations_research_Solver_constraints =
    R"doc(Counts the number of constraints that have been added to the solver
before the search.)doc";

static const char* __doc_operations_research_Solver_constraints_list =
    R"doc()doc";

static const char* __doc_operations_research_Solver_context =
    R"doc(Gets the current context of the search.)doc";

static const char* __doc_operations_research_Solver_context_2 = R"doc()doc";

static const char* __doc_operations_research_Solver_decisions = R"doc()doc";

static const char* __doc_operations_research_Solver_demon_profiler =
    R"doc(Access to demon profiler.)doc";

static const char* __doc_operations_research_Solver_demon_profiler_2 =
    R"doc(Demon monitor)doc";

static const char* __doc_operations_research_Solver_demon_runs =
    R"doc(The number of demons executed during search for a given priority.)doc";

static const char* __doc_operations_research_Solver_demon_runs_2 = R"doc()doc";

static const char* __doc_operations_research_Solver_empty_name = R"doc()doc";

static const char* __doc_operations_research_Solver_fail_decision = R"doc()doc";

static const char* __doc_operations_research_Solver_fail_intercept =
    R"doc(intercept failures)doc";

static const char* __doc_operations_research_Solver_fail_stamp =
    R"doc(The fail_stamp() is incremented after each backtrack.)doc";

static const char* __doc_operations_research_Solver_fail_stamp_2 = R"doc()doc";

static const char* __doc_operations_research_Solver_fails = R"doc()doc";

static const char* __doc_operations_research_Solver_failures =
    R"doc(The number of failures encountered since the creation of the solver.)doc";

static const char* __doc_operations_research_Solver_false_constraint =
    R"doc()doc";

static const char* __doc_operations_research_Solver_filtered_neighbors =
    R"doc(The number of filtered neighbors (neighbors accepted by filters).)doc";

static const char* __doc_operations_research_Solver_filtered_neighbors_2 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_local_search_monitor =
    R"doc()doc";

static const char* __doc_operations_research_Solver_local_search_profiler =
    R"doc(Local search profiler monitor)doc";

static const char* __doc_operations_research_Solver_local_search_state =
    R"doc(Local search state.)doc";

static const char* __doc_operations_research_Solver_model_cache = R"doc()doc";

static const char* __doc_operations_research_Solver_model_name =
    R"doc(Returns the name of the model.)doc";

static const char* __doc_operations_research_Solver_name = R"doc()doc";

static const char* __doc_operations_research_Solver_neighbors =
    R"doc(The number of neighbors created.)doc";

static const char* __doc_operations_research_Solver_neighbors_2 = R"doc()doc";

static const char* __doc_operations_research_Solver_num_int_vars = R"doc()doc";

static const char* __doc_operations_research_Solver_operator_assign =
    R"doc()doc";

static const char* __doc_operations_research_Solver_optimization_direction =
    R"doc(The direction of optimization, getter and setter.)doc";

static const char* __doc_operations_research_Solver_optimization_direction_2 =
    R"doc()doc";

static const char* __doc_operations_research_Solver_parameters =
    R"doc(Stored Parameters.)doc";

static const char* __doc_operations_research_Solver_parameters_2 = R"doc()doc";

static const char* __doc_operations_research_Solver_print_trace = R"doc()doc";

static const char* __doc_operations_research_Solver_propagation_monitor =
    R"doc()doc";

static const char* __doc_operations_research_Solver_propagation_object_names =
    R"doc()doc";

static const char* __doc_operations_research_Solver_queue = R"doc()doc";

static const char* __doc_operations_research_Solver_random = R"doc()doc";

static const char* __doc_operations_research_Solver_reset_action_on_fail =
    R"doc()doc";

static const char* __doc_operations_research_Solver_searches = R"doc()doc";

static const char* __doc_operations_research_Solver_set_action_on_fail =
    R"doc()doc";

static const char* __doc_operations_research_Solver_set_context =
    R"doc(Sets the current context of the search.)doc";

static const char* __doc_operations_research_Solver_set_fail_intercept =
    R"doc()doc";

static const char* __doc_operations_research_Solver_set_optimization_direction =
    R"doc()doc";

static const char*
    __doc_operations_research_Solver_set_variable_to_clean_on_fail =
        R"doc()doc";

static const char* __doc_operations_research_Solver_should_fail = R"doc()doc";

static const char* __doc_operations_research_Solver_solutions =
    R"doc(The number of solutions found since the start of the search.)doc";

static const char* __doc_operations_research_Solver_stamp =
    R"doc(The stamp indicates how many moves in the search tree we have
performed. It is useful to detect if we need to update same lazy
structures.)doc";

static const char* __doc_operations_research_Solver_state =
    R"doc(State of the solver.)doc";

static const char* __doc_operations_research_Solver_state_2 = R"doc()doc";

static const char* __doc_operations_research_Solver_timer = R"doc()doc";

static const char* __doc_operations_research_Solver_tmp_vector =
    R"doc(Unsafe temporary vector. It is used to avoid leaks in operations that
need storage and that may fail. See IntVar::SetValues() for instance.
It is not locked; do not use in a multi-threaded or reentrant setup.)doc";

static const char* __doc_operations_research_Solver_trail = R"doc()doc";

static const char* __doc_operations_research_Solver_true_constraint =
    R"doc(Cached constraints.)doc";

static const char* __doc_operations_research_Solver_unchecked_solutions =
    R"doc(The number of unchecked solutions found by local search.)doc";

static const char*
    __doc_operations_research_Solver_unnamed_enum_at_util_operations_research_constraint_solver_constraint_solver_h_3315_3 =
        R"doc(interval of constants cached, inclusive:)doc";

static const char*
    __doc_operations_research_Solver_unnamed_enum_at_util_operations_research_constraint_solver_constraint_solver_h_3315_3_MAX_CACHED_INT_CONST =
        R"doc()doc";

static const char*
    __doc_operations_research_Solver_unnamed_enum_at_util_operations_research_constraint_solver_constraint_solver_h_3315_3_MIN_CACHED_INT_CONST =
        R"doc()doc";

static const char* __doc_operations_research_Solver_use_fast_local_search =
    R"doc(Local search mode)doc";

static const char* __doc_operations_research_Solver_wall_time =
    R"doc(DEPRECATED: Use Now() instead. Time elapsed, in ms since the creation
of the solver.)doc";

static const char* __doc_operations_research_StateInfo = R"doc()doc";

static const char* __doc_operations_research_SymmetryBreaker = R"doc()doc";

static const char* __doc_operations_research_Trail = R"doc()doc";

static const char* __doc_operations_research_Zero =
    R"doc(This method returns 0. It is useful when 0 can be cast either as a
pointer or as an integer value and thus lead to an ambiguous function
call.)doc";

static const char* __doc_operations_research_operator_lshift = R"doc()doc";

static const char* __doc_operations_research_operator_lshift_2 = R"doc()doc";

static const char* __doc_operations_research_operator_lshift_3 = R"doc()doc";

static const char* __doc_util_Clock = R"doc()doc";

#if defined(__GNUG__)
#pragma GCC diagnostic pop
#endif
