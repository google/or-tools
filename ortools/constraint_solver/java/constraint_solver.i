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

// TODO(user): Refactor this file to adhere to the SWIG style guide.

%include "enumsimple.swg"
%include "exception.i"

%include "ortools/base/base.i"
%include "ortools/util/java/tuple_set.i"
%include "ortools/util/java/vector.i"
%include "ortools/util/java/functions.i"

%include "ortools/util/java/proto.i"

// Remove swig warnings
%warnfilter(473) operations_research::DecisionBuilder;
// TODO(user): Remove this warnfilter.

// We need to forward-declare the proto here, so that PROTO_INPUT involving it
// works correctly. The order matters very much: this declaration needs to be
// before the %{ #include ".../constraint_solver.h" %}.
namespace operations_research {
class ConstraintSolverParameters;
class SearchLimitParameters;
}  // namespace operations_research


// Include the files we want to wrap a first time.
%{
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/java/javawrapcp_util.h"
#include "ortools/constraint_solver/search_limit.pb.h"
#include "ortools/constraint_solver/solver_parameters.pb.h"

// Supporting structure for the PROTECT_FROM_FAILURE macro.
#include "setjmp.h"
struct FailureProtect {
  jmp_buf exception_buffer;
  void JumpBack() { longjmp(exception_buffer, 1); }
};
%}

// ############ BEGIN DUPLICATED CODE BLOCK ############
// IMPORTANT: keep this code block in sync with the .i
// files in ../python and ../csharp.

// Protect from failure.
%define PROTECT_FROM_FAILURE(Method, GetSolver)
%exception Method {
  operations_research::Solver* const solver = GetSolver;
  FailureProtect protect;
  solver->set_fail_intercept([&protect]() { protect.JumpBack(); });
  if (setjmp(protect.exception_buffer) == 0) {
    $action
    solver->clear_fail_intercept();
  } else {
    solver->clear_fail_intercept();
    jclass fail_class = jenv->FindClass(
        "com/google/ortools/constraintsolver/"
        "Solver$FailException");
    jenv->ThrowNew(fail_class, "fail");
    return $null;
  }
}
%enddef

namespace operations_research {
PROTECT_FROM_FAILURE(IntExpr::SetValue(int64 v), arg1->solver());
PROTECT_FROM_FAILURE(IntExpr::SetMin(int64 v), arg1->solver());
PROTECT_FROM_FAILURE(IntExpr::SetMax(int64 v), arg1->solver());
PROTECT_FROM_FAILURE(IntExpr::SetRange(int64 mi, int64 ma), arg1->solver());
PROTECT_FROM_FAILURE(IntVar::RemoveValue(int64 v), arg1->solver());
PROTECT_FROM_FAILURE(IntVar::RemoveValues(const std::vector<int64>& values),
                     arg1->solver());
PROTECT_FROM_FAILURE(IntervalVar::SetStartMin(int64 m), arg1->solver());
PROTECT_FROM_FAILURE(IntervalVar::SetStartMax(int64 m), arg1->solver());
PROTECT_FROM_FAILURE(IntervalVar::SetStartRange(int64 mi, int64 ma),
                     arg1->solver());
PROTECT_FROM_FAILURE(IntervalVar::SetDurationMin(int64 m), arg1->solver());
PROTECT_FROM_FAILURE(IntervalVar::SetDurationMax(int64 m), arg1->solver());
PROTECT_FROM_FAILURE(IntervalVar::SetDurationRange(int64 mi, int64 ma),
                     arg1->solver());
PROTECT_FROM_FAILURE(IntervalVar::SetEndMin(int64 m), arg1->solver());
PROTECT_FROM_FAILURE(IntervalVar::SetEndMax(int64 m), arg1->solver());
PROTECT_FROM_FAILURE(IntervalVar::SetEndRange(int64 mi, int64 ma),
                     arg1->solver());
PROTECT_FROM_FAILURE(IntervalVar::SetPerformed(bool val), arg1->solver());
PROTECT_FROM_FAILURE(Solver::AddConstraint(Constraint* const ct), arg1);
PROTECT_FROM_FAILURE(Solver::Fail(), arg1);
#undef PROTECT_FROM_FAILURE
}  // namespace operations_research

// ############ END DUPLICATED CODE BLOCK ############

%module(directors="1", allprotected="1") operations_research;

%typemap(javaimports) operations_research::Solver %{
import com.google.ortools.constraintsolver.ConstraintSolverParameters;
import com.google.ortools.constraintsolver.SearchLimitParameters;
%}

%feature("director") operations_research::DecisionBuilder;
%feature("director") operations_research::Decision;
%feature("director") operations_research::SearchMonitor;
%feature("director") operations_research::SymmetryBreaker;


%{
#include <setjmp.h>

#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"

namespace operations_research {
namespace swig_util {
class SolverToVoid {
 public:
  virtual ~SolverToVoid() {}
  virtual void Run(Solver*) = 0;
};
}  // namespace swig_util
}  // namespace operations_research
%}

// Renaming

%ignore operations_research::Solver::MakeIntVarArray;
%ignore operations_research::Solver::MakeBoolVarArray;
%ignore operations_research::Solver::MakeFixedDurationIntervalVarArray;
%ignore operations_research::IntVarLocalSearchFilter::FindIndex;
%ignore operations_research::Solver::set_fail_intercept;
%ignore operations_research::PropagationBaseObject::set_action_on_fail;

// This method causes issues with our std::vector<int64> wrapping. It's not really
// part of the public API anyway.
%ignore operations_research::ToInt64Vector;

%rename (nextWrap) operations_research::DecisionBuilder::Next;
%rename (toString) *::DebugString;
%rename (tryDecisions) operations_research::Solver::Try;

// Rename rules on Assignment.
%rename (activate) operations_research::Assignment::Activate;
%rename (activateObjective) operations_research::Assignment::ActivateObjective;
%rename (activated) operations_research::Assignment::Activated;
%rename (activatedObjective) operations_research::Assignment::ActivatedObjective;
%rename (add) operations_research::Assignment::Add;
%rename (addObjective) operations_research::Assignment::AddObjective;
%rename (clear) operations_research::Assignment::Clear;
%rename (contains) operations_research::Assignment::Contains;
%rename (copy) operations_research::Assignment::Copy;
%rename (deactivate) operations_research::Assignment::Deactivate;
%rename (deactivateObjective) operations_research::Assignment::DeactivateObjective;
%rename (durationMax) operations_research::Assignment::DurationMax;
%rename (durationMin) operations_research::Assignment::DurationMin;
%rename (durationValue) operations_research::Assignment::DurationValue;
%rename (empty) operations_research::Assignment::Empty;
%rename (endMax) operations_research::Assignment::EndMax;
%rename (endMin) operations_research::Assignment::EndMin;
%rename (endValue) operations_research::Assignment::EndValue;
%rename (hasObjective) operations_research::Assignment::HasObjective;
%rename (intVarContainer) operations_research::Assignment::IntVarContainer;
%rename (load) operations_research::Assignment::Load;
%rename (objective) operations_research::Assignment::Objective;
%rename (objectiveBound) operations_research::Assignment::ObjectiveBound;
%rename (objectiveMax) operations_research::Assignment::ObjectiveMax;
%rename (objectiveMin) operations_research::Assignment::ObjectiveMin;
%rename (objectiveValue) operations_research::Assignment::ObjectiveValue;
%rename (performedMax) operations_research::Assignment::PerformedMax;
%rename (performedMin) operations_research::Assignment::PerformedMin;
%rename (performedValue) operations_research::Assignment::PerformedValue;
%rename (restore) operations_research::Assignment::Restore;
%rename (save) operations_research::Assignment::Save;
%rename (size) operations_research::Assignment::Size;
%rename (setDurationMax) operations_research::Assignment::SetDurationMax;
%rename (setDurationMin) operations_research::Assignment::SetDurationMin;
%rename (setDurationRange) operations_research::Assignment::SetDurationRange;
%rename (setDurationValue) operations_research::Assignment::SetDurationValue;
%rename (setEndMax) operations_research::Assignment::SetEndMax;
%rename (setEndMin) operations_research::Assignment::SetEndMin;
%rename (setEndRange) operations_research::Assignment::SetEndRange;
%rename (setEndValue) operations_research::Assignment::SetEndValue;
%rename (setObjectiveMax) operations_research::Assignment::SetObjectiveMax;
%rename (setObjectiveMin) operations_research::Assignment::SetObjectiveMin;
%rename (setObjectiveRange) operations_research::Assignment::SetObjectiveRange;
%rename (setObjectiveValue) operations_research::Assignment::SetObjectiveValue;
%rename (setPerformedMax) operations_research::Assignment::SetPerformedMax;
%rename (setPerformedMin) operations_research::Assignment::SetPerformedMin;
%rename (setPerformedRange) operations_research::Assignment::SetPerformedRange;
%rename (setPerformedValue) operations_research::Assignment::SetPerformedValue;
%rename (setStartMax) operations_research::Assignment::SetStartMax;
%rename (setStartMin) operations_research::Assignment::SetStartMin;
%rename (setStartRange) operations_research::Assignment::SetStartRange;
%rename (setStartValue) operations_research::Assignment::SetStartValue;
%rename (size) operations_research::Assignment::Size;
%rename (startMax) operations_research::Assignment::StartMax;
%rename (startMin) operations_research::Assignment::StartMin;
%rename (startValue) operations_research::Assignment::StartValue;
%rename (store) operations_research::Assignment::Store;

// Rename rules on AssignmentContainer;
%rename (add) operations_research::AssignmentContainer::Add;
%rename (addAtPosition) operations_research::AssignmentContainer::AddAtPosition;
%rename (clear) operations_research::AssignmentContainer::Clear;
%rename (resize) operations_research::AssignmentContainer::Rezize;
%rename (empty) operations_research::AssignmentContainer::Empty;
%rename (copy) operations_research::AssignmentContainer::Copy;
%rename (contains) operations_research::AssignmentContainer::Contains;
%rename (MutableElement) operations_research::AssignmentContainer::MutableElement;
// No MutableElementOrNull
%rename (element) operations_research::AssignmentContainer::Element;
// No ElementPtrOrNull
// %unignore AssignmentContainer::elements;
%rename (size) operations_research::AssignmentContainer::Size;
%rename (store) operations_research::AssignmentContainer::Store;
%rename (restore) operations_research::AssignmentContainer::restore;

// Rename rules on AssignmentElement;
%rename (activate) operations_research::AssignmentElement::Activate;
%rename (deactivate) operations_research::AssignmentElement::Deactivate;
%rename (activated) operations_research::AssignmentElement::Activated;

// Rename rules on IntVarElement
%rename (reset) operations_research::IntVarElement::Reset;
%rename (clone) operations_research::IntVarElement::Clone;
%rename (copy) operations_research::IntVarElement::Copy;
%rename (store) operations_research::IntVarElement::Store;
%rename (restore) operations_research::IntVarElement::Restore;
// No LoadFromProto
// No WriteToProto
%rename (min) operations_research::IntVarElement::Min;
%rename (setMin) operations_research::IntVarElement::SetMin;
%rename (max) operations_research::IntVarElement::Max;
%rename (setMax) operations_research::IntVarElement::SetMax;
%rename (value) operations_research::IntVarElement::Value;
%rename (setValue) operations_research::IntVarElement::SetValue;
%rename (setRange) operations_research::IntVarElement::SetRange;
%rename (var) operations_research::IntVarElement::Var;

// Rename rules on SolutionCollector.
%rename (add) operations_research::SolutionCollector::Add;
%rename (addObjective) operations_research::SolutionCollector::AddObjective;
%rename (durationValue) operations_research::SolutionCollector::DurationValue;
%rename (endValue) operations_research::SolutionCollector::EndValue;
%rename (objectiveValue) operations_research::SolutionCollector::objective_value;
%rename (performedValue) operations_research::SolutionCollector::PerformedValue;
%rename (solutionCount) operations_research::SolutionCollector::solution_count;
%rename (startValue) operations_research::SolutionCollector::StartValue;
%rename (wallTime) operations_research::SolutionCollector::wall_time;

// Rename rules on Solver.
%rename (acceptedNeighbors) operations_research::Solver::accepted_neighbors;
%rename (addBacktrackAction) operations_research::Solver::AddBacktrackAction;
%rename (addConstraint) operations_research::Solver::AddConstraint;
%rename (checkAssignment) operations_research::Solver::CheckAssignment;
%rename (compose) operations_research::Solver::Compose;
%rename (concatenateOperators) operations_research::Solver::ConcatenateOperators;
%rename (defaultSolverParameters) operations_research::Solver::DefaultSolverParameters;
%rename (endSearch) operations_research::Solver::EndSearch;
%rename (exportProfilingOverview) operations_research::Solver::ExportProfilingOverview;
%rename (fail) operations_research::Solver::Fail;
%rename (filteredNeighbors) operations_research::Solver::filtered_neighbors;
%rename (getTime) operations_research::Solver::GetTime;
%rename (makeAbs) operations_research::Solver::MakeAbs;
%rename (makeAllDifferent) operations_research::Solver::MakeAllDifferent;
%rename (makeAllSolutionCollector) operations_research::Solver::MakeAllSolutionCollector;
%rename (makeAllowedAssignment) operations_research::Solver::MakeAllowedAssignments;
%rename (makeAssignVariableValue) operations_research::Solver::MakeAssignVariableValue;
%rename (makeAssignVariableValueOrFail) operations_research::Solver::MakeAssignVariableValueOrFail;
%rename (makeAssignVariablesValues) operations_research::Solver::MakeAssignVariablesValues;
%rename (makeAssignment) operations_research::Solver::MakeAssignment;
%rename (makeBestValueSolutionCollector) operations_research::Solver::MakeBestValueSolutionCollector;
%rename (makeBetweenCt) operations_research::Solver::MakeBetweenCt;
%rename (makeBoolVar) operations_research::Solver::MakeBoolVar;
%rename (makeBranchesLimit) operations_research::Solver::MakeBranchesLimit;
%rename (makeClosureDemon) operations_research::Solver::MakeClosureDemon;
%rename (makeConstantRestart) operations_research::Solver::MakeConstantRestart;
%rename (makeConvexPiecewiseExpr) operations_research::Solver::MakeConvexPiecewiseExpr;
%rename (makeCount) operations_research::Solver::MakeCount;
%rename (makeCumulative) operations_research::Solver::MakeCumulative;
%rename (makeCustomLimit) operations_research::Solver::MakeCustomLimit;
%rename (makeDecision) operations_research::Solver::MakeDecision;
%rename (makeDecisionBuilderFromAssignment) operations_research::Solver::MakeDecisionBuilderFromAssignment;
%rename (makeDefaultPhase) operations_research::Solver::MakeDefaultPhase;
%rename (makeDefaultSearchLimitParameters) operations_research::Solver::MakeDefaultSearchLimitParameters;
%rename (makeDifference) operations_research::Solver::MakeDifference;
%rename (makeDeviation) operations_research::Solver::MakeDeviation;
%rename (makeDisjunctiveConstraint) operations_research::Solver::MakeDisjunctiveConstraint;
%rename (makeDistribute) operations_research::Solver::MakeDistribute;
%rename (makeDiv) operations_research::Solver::MakeDiv;
%rename (makeElement) operations_research::Solver::MakeElement;
%rename (makeEquality) operations_research::Solver::MakeEquality;
%rename (makeFailDecision) operations_research::Solver::MakeFailDecision;
%rename (makeFailuresLimit) operations_research::Solver::MakeFailuresLimit;
%rename (makeFalseConstraint) operations_research::Solver::MakeFalseConstraint;
%rename (makeFirstSolutionCollector) operations_research::Solver::MakeFirstSolutionCollector;
%rename (makeFixedDurationIntervalVar) operations_research::Solver::MakeFixedDurationIntervalVar;
%rename (makeFixedInterval) operations_research::Solver::MakeFixedInterval;
%rename (makeGreater) operations_research::Solver::MakeGreater;
%rename (makeGreaterOrEqual) operations_research::Solver::MakeGreaterOrEqual;
%rename (makeGuidedLocalSearch) operations_research::Solver::MakeGuidedLocalSearch;
%rename (makeIntConst) operations_research::Solver::MakeIntConst;
%rename (makeIntVar) operations_research::Solver::MakeIntVar;
%rename (makeIntervalVarRelation) operations_research::Solver::MakeIntervalVarRelation;
%rename (makeIntervalVarRelationWithDelay) operations_research::Solver::MakeIntervalVarRelationWithDelay;
%rename (makeIsBetweenCt) operations_research::Solver::MakeIsBetweenCt;
%rename (makeIsDifferentCstCt) operations_research::Solver::MakeIsDifferentCstCt;
%rename (makeIsDifferentCstCt) operations_research::Solver::MakeIsDifferentCt;
%rename (makeIsDifferentCstVar) operations_research::Solver::MakeIsDifferentCstVar;
%rename (makeIsDifferentCstVar) operations_research::Solver::MakeIsDifferentVar;
%rename (makeIsEqualCstCt) operations_research::Solver::MakeIsEqualCstCt;
%rename (makeIsEqualCstVar) operations_research::Solver::MakeIsEqualCstVar;
%rename (makeIsEqualVar) operations_research::Solver::MakeIsEqualCt;
%rename (makeIsEqualVar) operations_research::Solver::MakeIsEqualVar;
%rename (makeIsGreaterCstCt) operations_research::Solver::MakeIsGreaterCstCt;
%rename (makeIsGreaterCstVar) operations_research::Solver::MakeIsGreaterCstVar;
%rename (makeIsGreaterCt) operations_research::Solver::MakeIsGreaterCt;
%rename (makeIsGreaterOrEqualCstCt) operations_research::Solver::MakeIsGreaterOrEqualCstCt;
%rename (makeIsGreaterOrEqualCstVar) operations_research::Solver::MakeIsGreaterOrEqualCstVar;
%rename (makeIsGreaterOrEqualCt) operations_research::Solver::MakeIsGreaterOrEqualCt;
%rename (makeIsGreaterOrEqualVar) operations_research::Solver::MakeIsGreaterOrEqualVar;
%rename (makeIsGreaterVar) operations_research::Solver::MakeIsGreaterVar;
%rename (makeIsLessCstCt) operations_research::Solver::MakeIsLessCstCt;
%rename (makeIsLessCstVar) operations_research::Solver::MakeIsLessCstVar;
%rename (makeIsLessCt) operations_research::Solver::MakeIsLessCt;
%rename (makeIsLessOrEqualCstCt) operations_research::Solver::MakeIsLessOrEqualCstCt;
%rename (makeIsLessOrEqualCstVar) operations_research::Solver::MakeIsLessOrEqualCstVar;
%rename (makeIsLessOrEqualCt) operations_research::Solver::MakeIsLessOrEqualCt;
%rename (makeIsLessOrEqualVar) operations_research::Solver::MakeIsLessOrEqualVar;
%rename (makeIsLessVar) operations_research::Solver::MakeIsLessVar;
%rename (makeIsMemberCt) operations_research::Solver::MakeIsMemberCt;
%rename (makeIsMemberVar) operations_research::Solver::MakeIsMemberVar;
%rename (makeLastSolutionCollector) operations_research::Solver::MakeLastSolutionCollector;
%rename (makeLess) operations_research::Solver::MakeLess;
%rename (makeLessOrEqual) operations_research::Solver::MakeLessOrEqual;
%rename (makeLimit) operations_research::Solver::MakeLimit;
%rename (makeLocalSearchObjectiveFilter) operations_research::Solver::MakeLocalSearchObjectiveFilter;
%rename (makeLocalSearchPhase) operations_research::Solver::MakeLocalSearchPhase;
%rename (makeLocalSearchPhaseParameters) operations_research::Solver::MakeLocalSearchPhaseParameters;
%rename (makeLubyRestart) operations_research::Solver::MakeLubyRestart;
%rename (makeMapDomain) operations_research::Solver::MakeMapDomain;
%rename (makeMax) operations_research::Solver::MakeMax;
%rename (makeMaximize) operations_research::Solver::MakeMaximize;
%rename (makeMemberCt) operations_research::Solver::MakeMemberCt;
%rename (makeMin) operations_research::Solver::MakeMin;
%rename (makeMinimize) operations_research::Solver::MakeMinimize;
%rename (makeMirrorInterval) operations_research::Solver::MakeMirrorInterval;
%rename (makeNeighborhoodLimit) operations_research::Solver::MakeNeighborhoodLimit;
%rename (makeNoCycle) operations_research::Solver::MakeNoCycle;
%rename (makeNonEquality) operations_research::Solver::MakeNonEquality;
%rename (makeOperator) operations_research::Solver::MakeOperator;
%rename (makeOpposite) operations_research::Solver::MakeOpposite;
%rename (makeOptimize) operations_research::Solver::MakeOptimize;
%rename (makePack) operations_research::Solver::MakePack;
%rename (makePathCumul) operations_research::Solver::MakePathCumul;
%rename (makePhase) operations_research::Solver::MakePhase;
%rename (makeProd) operations_research::Solver::MakeProd;
%rename (makeRandomLnsOperator) operations_research::Solver::MakeRandomLnsOperator;
%rename (makeRankFirstInterval) operations_research::Solver::MakeRankFirstInterval;
%rename (makeRankLastInterval) operations_research::Solver::MakeRankLastInterval;
%rename (makeRestoreAssignment) operations_research::Solver::MakeRestoreAssignment;
%rename (makeScalProd) operations_research::Solver::MakeScalProd;
%rename (makeScalProdEquality) operations_research::Solver::MakeScalProdEquality;
%rename (makeScalProdGreaterOrEqual) operations_research::Solver::MakeScalProdGreaterOrEqual;
%rename (makeScalProdLessOrEqual) operations_research::Solver::MakeScalProdLessOrEqual;
%rename (makeScheduleOrPostpone) operations_research::Solver::MakeScheduleOrPostpone;
%rename (makeSearchLog) operations_research::Solver::MakeSearchLog;
%rename (makeSearchTrace) operations_research::Solver::MakeSearchTrace;
%rename (makeSemiContinuousExpr) operations_research::Solver::MakeSemiContinuousExpr;
%rename (makeSequenceVar) operations_research::Solver::MakeSequenceVar;
%rename (makeSimulatedAnnealing) operations_research::Solver::MakeSimulatedAnnealing;
%rename (makeSolutionsLimit) operations_research::Solver::MakeSolutionsLimit;
%rename (makeSolveOnce) operations_research::Solver::MakeSolveOnce;
%rename (makeSortingConstraint) operations_research::Solver::MakeSortingConstraint;
%rename (makeSplitVariableDomain) operations_research::Solver::MakeSplitVariableDomain;
%rename (makeSquare) operations_research::Solver::MakeSquare;
%rename (makeStoreAssignment) operations_research::Solver::MakeStoreAssignment;
%rename (makeSum) operations_research::Solver::MakeSum;
%rename (makeSumEquality) operations_research::Solver::MakeSumEquality;
%rename (makeSumGreaterOrEqual) operations_research::Solver::MakeSumGreaterOrEqual;
%rename (makeSumLessOrEqual) operations_research::Solver::MakeSumLessOrEqual;
%rename (makeSymmetryManager) operations_research::Solver::MakeSymmetryManager;
%rename (makeTabuSearch) operations_research::Solver::MakeTabuSearch;
%rename (makeTemporalDisjunction) operations_research::Solver::MakeTemporalDisjunction;
%rename (makeTimeLimit) operations_research::Solver::MakeTimeLimit;
%rename (makeTransitionConstraint) operations_research::Solver::MakeTransitionConstraint;
%rename (makeTreeMonitor) operations_research::Solver::MakeTreeMonitor;
%rename (makeTrueConstraint) operations_research::Solver::MakeTrueConstraint;
%rename (makeWeightedMaximize) operations_research::Solver::MakeWeightedMaximize;
%rename (makeWeightedMinimize) operations_research::Solver::MakeWeightedMinimize;
%rename (makeWeightedOptimize) operations_research::Solver::MakeWeightedOptimize;
%rename (newSearch) operations_research::Solver::NewSearch;
%rename (nextSolution) operations_research::Solver::NextSolution;
%rename (rand32) operations_research::Solver::Rand32;
%rename (rand64) operations_research::Solver::Rand64;
%rename (randomConcatenateOperators) operations_research::Solver::RandomConcatenateOperators;
%rename (rankFirst) operations_research::SequenceVar::RankFirst;
%rename (rankNotFirst) operations_research::SequenceVar::RankNotFirst;
%rename (rankLast) operations_research::SequenceVar::RankLast;
%rename (rankNotLast) operations_research::SequenceVar::RankNotLast;
%rename (rankSequence) operations_research::SequenceVar::RankSequence;
%rename (reSeed) operations_research::Solver::ReSeed;
%rename (searchDepth) operations_research::Solver::SearchDepth;
%rename (searchLeftDepth) operations_research::Solver::SearchLeftDepth;
%rename (solve) operations_research::Solver::Solve;
%rename (solveAndCommit) operations_research::Solver::SolveAndCommit;
%rename (solveDepth) operations_research::Solver::SolveDepth;
%rename (updateLimits) operations_research::Solver::UpdateLimits;
%rename (wallTime) operations_research::Solver::wall_time;

// Rename rules on IntVar and IntExpr.
%rename (var) operations_research::IntExpr::Var;
%rename (range) operations_research::IntExpr::Range;
%rename (addName) operations_research::IntVar::AddName;
%rename (isVar) operations_research::IntExpr::IsVar;
%rename (removeValue) operations_research::IntVar::RemoveValue;
%rename (removeValues) operations_research::IntVar::RemoveValues;
%rename (removeInterval) operations_research::IntVar::RemoveInterval;
%rename (contains) operations_research::IntVar::Contains;

// Rename rules on Constraint.
%rename (var) operations_research::Constraint::Var;

// Rename rule on Disjunctive Constraint.
%rename (makeSequenceVar) operations_research::DisjunctiveConstraint::MakeSequenceVar;

// Generic rename rules.
%rename (bound) *::Bound;
%rename (max) *::Max;
%rename (min) *::Min;
%rename (setMax) *::SetMax;
%rename (setMin) *::SetMin;
%rename (setRange) *::SetRange;
%rename (setValue) *::SetValue;
%rename (setValue) *::SetValues;
%rename (value) *::Value;
%rename (accept) *::Accept;

// Rename rules on PropagationBaseObject.
%rename (setName) operations_research::PropagationBaseObject::set_name;

// Rename rules on Search Monitor
%rename (acceptDelta) operations_research::SearchMonitor::AcceptDelta;
%rename (acceptNeighbor) operations_research::SearchMonitor::AcceptNeighbor;
%rename (acceptSolution) operations_research::SearchMonitor::AcceptSolution;
%rename (afterDecision) operations_research::SearchMonitor::AfterDecision;
%rename (applyDecision) operations_research::SearchMonitor::ApplyDecision;
%rename (atSolution) operations_research::SearchMonitor::AtSolution;
%rename (beginFail) operations_research::SearchMonitor::BeginFail;
%rename (beginInitialPropagation) operations_research::SearchMonitor::BeginInitialPropagation;
%rename (beginNextDecision) operations_research::SearchMonitor::BeginNextDecision;
%rename (endFail) operations_research::SearchMonitor::EndFail;
%rename (endInitialPropagation) operations_research::SearchMonitor::EndInitialPropagation;
%rename (endNextDecision) operations_research::SearchMonitor::EndNextDecision;
%rename (enterSearch) operations_research::SearchMonitor::EnterSearch;
%rename (finishCurrentSearch) operations_research::SearchMonitor::FinishCurrentSearch;
%rename (localOptimum) operations_research::SearchMonitor::LocalOptimum;
%rename (noMoreSolutions) operations_research::SearchMonitor::NoMoreSolutions;
%rename (periodicCheck) operations_research::SearchMonitor::PeriodicCheck;
%rename (refuteDecision) operations_research::SearchMonitor::RefuteDecision;
%rename (restartCurrentSearch) operations_research::SearchMonitor::RestartCurrentSearch;
%rename (restartSearch) operations_research::SearchMonitor::RestartSearch;

// LocalSearchOperator
%feature("director") operations_research::LocalSearchOperator;
%rename (nextNeighbor) operations_research::LocalSearchOperator::MakeNextNeighbor;
%rename (start) operations_research::LocalSearchOperator::Start;

// VarLocalSearchOperator<>
// Ignored:
// - Start()
// - SkipUnchanged()
// - ApplyChanges()
// - RevertChanges()
%rename (size) operations_research::VarLocalSearchOperator::Size;
%rename (value) operations_research::VarLocalSearchOperator::Value;
%rename (isIncremental) operations_research::VarLocalSearchOperator::IsIncremental;
%rename (onStart) operations_research::VarLocalSearchOperator::OnStart;
%rename (oldValue) operations_research::VarLocalSearchOperator::OldValue;
%rename (setValue) operations_research::VarLocalSearchOperator::SetValue;
%rename (var) operations_research::VarLocalSearchOperator::Var;
%rename (activated) operations_research::VarLocalSearchOperator::Activated;
%rename (activate) operations_research::VarLocalSearchOperator::Activate;
%rename (deactivate) operations_research::VarLocalSearchOperator::Deactivate;
%rename (addVars) operations_research::VarLocalSearchOperator::AddVars;

// IntVarLocalSearchOperator
%feature("director") operations_research::IntVarLocalSearchOperator;
%rename (size) operations_research::IntVarLocalSearchOperator::Size;
%rename (oneNeighbor) operations_research::IntVarLocalSearchOperator::MakeOneNeighbor;
%rename (value) operations_research::IntVarLocalSearchOperator::Value;
%rename (isIncremental) operations_research::IntVarLocalSearchOperator::IsIncremental;
%rename (onStart) operations_research::IntVarLocalSearchOperator::OnStart;
%rename (oldValue) operations_research::IntVarLocalSearchOperator::OldValue;
%rename (setValue) operations_research::IntVarLocalSearchOperator::SetValue;
%rename (var) operations_research::IntVarLocalSearchOperator::Var;
%rename (activated) operations_research::IntVarLocalSearchOperator::Activated;
%rename (activate) operations_research::IntVarLocalSearchOperator::Activate;
%rename (deactivate) operations_research::IntVarLocalSearchOperator::Deactivate;
%rename (addVars) operations_research::IntVarLocalSearchOperator::AddVars;
%ignore operations_research::IntVarLocalSearchOperator::MakeNextNeighbor;

%feature("director") operations_research::BaseLns;
%rename (initFragments) operations_research::BaseLns::InitFragments;
%rename (nextFragment) operations_research::BaseLns::NextFragment;
%feature ("nodirector") operations_research::BaseLns::OnStart;
%feature ("nodirector") operations_research::BaseLns::SkipUnchanged;
%feature ("nodirector") operations_research::BaseLns::MakeOneNeighbor;
%rename (isIncremental) operations_research::BaseLns::IsIncremental;
%rename (appendToFragment) operations_research::BaseLns::AppendToFragment;
%rename(fragmentSize) operations_research::BaseLns::FragmentSize;

// ChangeValue
%feature("director") operations_research::ChangeValue;
%rename (modifyValue) operations_research::ChangeValue::ModifyValue;

// SequenceVarLocalSearchOperator
// Ignored:
// - Sequence()
// - OldSequence()
// - SetForwardSequence()
// - SetBackwardSequence()
%feature("director") operations_research::SequenceVarLocalSearchOperator;
%rename (start) operations_research::SequenceVarLocalSearchOperator::Start;

// PathOperator
// Ignored:
// - SkipUnchanged()
// - Next()
// - Path()
// - number_of_nexts()
%feature("director") operations_research::PathOperator;
%rename (neighbor) operations_research::PathOperator::MakeNeighbor;

// LocalSearchFilter
%feature("director") operations_research::IntVarLocalSearchFilter;
%rename (accept) operations_research::LocalSearchFilter::Accept;
%rename (synchronize) operations_research::LocalSearchFilter::Synchronize;
%rename (isIncremental) operations_research::LocalSearchFilter::IsIncremental;

// IntVarLocalSearchFilter
// Ignored:
// - IsVarSynced()
%feature("director") operations_research::IntVarLocalSearchFilter;
%feature("nodirector") operations_research::IntVarLocalSearchFilter::Synchronize;  // Inherited.
%ignore operations_research::IntVarLocalSearchFilter::FindIndex;
%rename (addVars) operations_research::IntVarLocalSearchFilter::AddVars;  // Inherited.
%rename (isIncremental) operations_research::IntVarLocalSearchFilter::IsIncremental;
%rename (onSynchronize) operations_research::IntVarLocalSearchFilter::OnSynchronize;
%rename (size) operations_research::IntVarLocalSearchFilter::Size;
%rename (start) operations_research::IntVarLocalSearchFilter::Start;
%rename (value) operations_research::IntVarLocalSearchFilter::Value;
%rename (var) operations_research::IntVarLocalSearchFilter::Var;  // Inherited.

namespace operations_research {

// Typemaps to represent const std::vector<CType*>& arguments as arrays of
// JavaType, where CType is not a primitive type.
// TODO(user): See if it makes sense to move this
// ortools/util/vector.i.

// CastOp defines how to cast the output of CallStaticLongMethod to CType*;
// its first argument is CType, its second is the output of
// CallStaticLongMethod.
%define CONVERT_VECTOR_WITH_CAST(CType, JavaType, CastOp)
%typemap(jni) const std::vector<CType*>& "jobjectArray"
%typemap(jtype) const std::vector<CType*>& "JavaType[]"
%typemap(jstype) const std::vector<CType*>& "JavaType[]"
%typemap(javain) const std::vector<CType*>& "$javainput"
%typemap(in) const std::vector<CType*>& (std::vector<CType*> result) {
  jclass object_class =
    jenv->FindClass("com/google/ortools/"
                    "constraintsolver/JavaType");
  if (nullptr == object_class)
    return $null;
  jmethodID method_id =
      jenv->GetStaticMethodID(object_class,
                              "getCPtr",
                              "(Lcom/google/ortools/"
                              "constraintsolver/JavaType;)J");
  assert(method_id != nullptr);
  for (int i = 0; i < jenv->GetArrayLength($input); i++) {
    jobject elem = jenv->GetObjectArrayElement($input, i);
    jlong ptr_value = jenv->CallStaticLongMethod(object_class, method_id, elem);
    result.push_back(CastOp(CType, ptr_value));
  }
  $1 = &result;
}
%typemap(out) const std::vector<CType*>& {
  jclass object_class =
      jenv->FindClass("com/google/ortools/constraintsolver/JavaType");
  $result = jenv->NewObjectArray($1->size(), object_class, 0);
  if (nullptr != object_class) {
    jmethodID ctor = jenv->GetMethodID(object_class,"<init>", "(JZ)V");
    for (int i = 0; i < $1->size(); ++i) {
      jlong obj_ptr = 0;
      *((operations_research::CType **)&obj_ptr) = (*$1)[i];
      jobject elem = jenv->NewObject(object_class, ctor, obj_ptr, false);
      jenv->SetObjectArrayElement($result, i, elem);
    }
  }
}
%typemap(javaout) const std::vector<CType*> & {
  return $jnicall;
}
%enddef

%define REINTERPRET_CAST(CType, ptr)
reinterpret_cast<operations_research::CType*>(ptr)
%enddef

%define CONVERT_VECTOR(CType, JavaType)
CONVERT_VECTOR_WITH_CAST(CType, JavaType, REINTERPRET_CAST);
%enddef

CONVERT_VECTOR(IntVar, IntVar);
CONVERT_VECTOR(SearchMonitor, SearchMonitor);
CONVERT_VECTOR(DecisionBuilder, DecisionBuilder);
CONVERT_VECTOR(IntervalVar, IntervalVar);
CONVERT_VECTOR(SequenceVar, SequenceVar);
CONVERT_VECTOR(LocalSearchOperator, LocalSearchOperator);
CONVERT_VECTOR(LocalSearchFilter, LocalSearchFilter);
CONVERT_VECTOR(SymmetryBreaker, SymmetryBreaker);

%typemap(javacode) Solver %{
  /**
   * This exceptions signal that a failure has been raised in the C++ world.
   *
   */
  public static class FailException extends Exception {
    public FailException() {
      super();
    }

    public FailException(String message) {
      super(message);
    }
  }

  public IntVar[] makeIntVarArray(int count, long min, long max) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = makeIntVar(min, max);
    }
    return array;
  }

  public IntVar[] makeIntVarArray(int count, long min, long max, String name) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      String var_name = name + i;
      array[i] = makeIntVar(min, max, var_name);
    }
    return array;
  }

  public IntVar[] makeBoolVarArray(int count) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = makeBoolVar();
    }
    return array;
  }

  public IntVar[] makeBoolVarArray(int count, String name) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      String var_name = name + i;
      array[i] = makeBoolVar(var_name);
    }
    return array;
  }

  public IntervalVar[] makeFixedDurationIntervalVarArray(int count,
                                                         long start_min,
                                                         long start_max,
                                                         long duration,
                                                         boolean optional) {
    IntervalVar[] array = new IntervalVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = makeFixedDurationIntervalVar(start_min,
                                              start_max,
                                              duration,
                                              optional,
                                              "");
    }
    return array;
  }

  public IntervalVar[] makeFixedDurationIntervalVarArray(int count,
                                                         long start_min,
                                                         long start_max,
                                                         long duration,
                                                         boolean optional,
                                                         String name) {
    IntervalVar[] array = new IntervalVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = makeFixedDurationIntervalVar(start_min,
                                              start_max,
                                              duration,
                                              optional,
                                              name + i);
    }
    return array;
  }

%}

%extend IntVarLocalSearchFilter {
  int index(IntVar* const var) {
    int64 index = -1;
    $self->FindIndex(var, &index);
    return index;
  }
}

}  // namespace operations_research

// Create std::function wrappers.
WRAP_STD_FUNCTION_JAVA(
    LongToLong,
    "com/google/ortools/constraintsolver/",
    int64, Long, int64)
WRAP_STD_FUNCTION_JAVA(
    LongLongToLong,
    "com/google/ortools/constraintsolver/",
    int64, Long, int64, int64)
WRAP_STD_FUNCTION_JAVA(
    IntIntToLong,
    "com/google/ortools/constraintsolver/",
    int64, Long, int, int)
WRAP_STD_FUNCTION_JAVA(
    LongLongLongToLong,
    "com/google/ortools/constraintsolver/",
    int64, Long, int64, int64, int64)
WRAP_STD_FUNCTION_JAVA(
    LongToBoolean,
    "com/google/ortools/constraintsolver/",
    bool, Boolean, int64)
WRAP_STD_FUNCTION_JAVA(
    VoidToBoolean,
    "com/google/ortools/constraintsolver/",
    bool, Boolean)
WRAP_STD_FUNCTION_JAVA(
    LongLongLongToBoolean,
    "com/google/ortools/constraintsolver/",
    bool, Boolean, int64, int64, int64)
WRAP_STD_FUNCTIONS_WITH_VOID_JAVA("com/google/ortools/constraintsolver/")
WRAP_STD_FUNCTION_JAVA_CLASS_TO_VOID(
    SolverToVoid,
    "com/google/ortools/constraintsolver/",
    Solver)

// Protobuf support
PROTO_INPUT(operations_research::ConstraintSolverParameters,
            com.google.ortools.constraintsolver.ConstraintSolverParameters,
            parameters)
PROTO2_RETURN(operations_research::ConstraintSolverParameters,
              com.google.ortools.constraintsolver.ConstraintSolverParameters)

PROTO_INPUT(operations_research::SearchLimitParameters,
            com.google.ortools.constraintsolver.SearchLimitParameters,
            proto)
PROTO2_RETURN(operations_research::SearchLimitParameters,
              com.google.ortools.constraintsolver.SearchLimitParameters)

// Wrap cp includes
%include "ortools/constraint_solver/constraint_solver.h"
%include "ortools/constraint_solver/constraint_solveri.h"
%include "ortools/constraint_solver/java/javawrapcp_util.h"

namespace operations_research {
namespace swig_util {
class SolverToVoid {
 public:
  virtual ~SolverToVoid() {}
  virtual void Run(Solver*) = 0;
};
}  // namespace swig_util
}  // namespace operations_research


// Define templates instantiation after wrapping.
namespace operations_research {
%template(RevInteger) Rev<int>;
%template(RevLong) Rev<int64>;
%template(RevBool) Rev<bool>;
%template(AssignmentIntContainer) AssignmentContainer<IntVar, IntVarElement>;
}  // namespace operations_research
