// Copyright 2010-2018 Google LLC
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
%include "stdint.i"

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

typedef int64_t int64;
typedef uint64_t uint64;

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
PROTECT_FROM_FAILURE(IntExpr::SetRange(int64 l, int64 u), arg1->solver());
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
PROTECT_FROM_FAILURE(Solver::AddConstraint(Constraint* const c), arg1);
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
%feature("director") operations_research::DecisionVisitor;
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

// Rename rules on Decision.
%rename (apply) operations_research::Decision::Apply;
%rename (refute) operations_research::Decision::Refute;

// Rename rules on DecisionBuilder.
%rename (nextWrap) operations_research::DecisionBuilder::Next;

// Rename rules on DecisionVisitor.
%rename (visitRankFirstInterval) operations_research::DecisionVisitor::VisitRankFirstInterval;
%rename (visitRankLastInterval) operations_research::DecisionVisitor::VisitRankLastInterval;
%rename (visitScheduleOrExpedite) operations_research::DecisionVisitor::VisitScheduleOrExpedite;
%rename (visitScheduleOrPostpone) operations_research::DecisionVisitor::VisitScheduleOrPostpone;
%rename (visitSetVariableValue) operations_research::DecisionVisitor::VisitSetVariableValue;
%rename (visitSplitVariableDomain) operations_research::DecisionVisitor::VisitSplitVariableDomain;
%rename (visitUnknownDecision) operations_research::DecisionVisitor::VisitUnknownDecision;

// Rename rules on ModelVisitor.
%rename (beginVisitConstraint) operations_research::ModelVisitor::BeginVisitConstraint;
%rename (beginVisitExtension) operations_research::ModelVisitor::BeginVisitExtension;
%rename (beginVisitIntegerExpression) operations_research::ModelVisitor::BeginVisitIntegerExpression;
%rename (beginVisitModel) operations_research::ModelVisitor::BeginVisitModel;
%rename (endVisitConstraint) operations_research::ModelVisitor::EndVisitConstraint;
%rename (endVisitExtension) operations_research::ModelVisitor::EndVisitExtension;
%rename (endVisitIntegerExpression) operations_research::ModelVisitor::EndVisitIntegerExpression;
%rename (endVisitModel) operations_research::ModelVisitor::EndVisitModel;
%rename (visitIntegerArgument) operations_research::ModelVisitor::VisitIntegerArgument;
%rename (visitIntegerArrayArgument) operations_research::ModelVisitor::VisitIntegerArrayArgument;
%rename (visitIntegerExpressionArgument) operations_research::ModelVisitor::VisitIntegerExpressionArgument;
%rename (visitIntegerMatrixArgument) operations_research::ModelVisitor::VisitIntegerMatrixArgument;
%rename (visitIntegerVariableArrayArgument) operations_research::ModelVisitor::VisitIntegerVariableArrayArgument;
%rename (visitIntegerVariable) operations_research::ModelVisitor::VisitIntegerVariable;
%rename (visitIntervalArgument) operations_research::ModelVisitor::VisitIntervalArgument;
%rename (visitIntervalArrayArgument) operations_research::ModelVisitor::VisitIntervalArrayArgument;
%rename (visitIntervalVariable) operations_research::ModelVisitor::VisitIntervalVariable;
%rename (visitSequenceArgument) operations_research::ModelVisitor::VisitSequenceArgument;
%rename (visitSequenceArrayArgument) operations_research::ModelVisitor::VisitSequenceArrayArgument;
%rename (visitSequenceVariable) operations_research::ModelVisitor::VisitSequenceVariable;

// Rename rules on SymmetryBreaker.
%rename (addIntegerVariableEqualValueClause) operations_research::SymmetryBreaker::AddIntegerVariableEqualValueClause;
%rename (addIntegerVariableGreaterOrEqualValueClause) operations_research::SymmetryBreaker::AddIntegerVariableGreaterOrEqualValueClause;
%rename (addIntegerVariableLessOrEqualValueClause) operations_research::SymmetryBreaker::AddIntegerVariableLessOrEqualValueClause;

// Rename rules on ModelCache.
%rename (clear) operations_research::ModelCache::Clear;
%rename (findExprConstantExpression) operations_research::ModelCache::FindExprConstantExpression;
%rename (findExprExprConstantExpression) operations_research::ModelCache::FindExprExprConstantExpression;
%rename (findExprExprConstraint) operations_research::ModelCache::FindExprExprConstraint;
%rename (findExprExpression) operations_research::ModelCache::FindExprExpression;
%rename (findExprExprExpression) operations_research::ModelCache::FindExprExprExpression;
%rename (findVarArrayConstantArrayExpression) operations_research::ModelCache::FindVarArrayConstantArrayExpression;
%rename (findVarArrayConstantExpression) operations_research::ModelCache::FindVarArrayConstantExpression;
%rename (findVarArrayExpression) operations_research::ModelCache::FindVarArrayExpression;
%rename (findVarConstantArrayExpression) operations_research::ModelCache::FindVarConstantArrayExpression;
%rename (findVarConstantConstantConstraint) operations_research::ModelCache::FindVarConstantConstantConstraint;
%rename (findVarConstantConstantExpression) operations_research::ModelCache::FindVarConstantConstantExpression;
%rename (findVarConstantConstraint) operations_research::ModelCache::FindVarConstantConstraint;
%rename (findVoidConstraint) operations_research::ModelCache::FindVoidConstraint;
%rename (insertExprConstantExpression) operations_research::ModelCache::InsertExprConstantExpression;
%rename (insertExprExprConstantExpression) operations_research::ModelCache::InsertExprExprConstantExpression;
%rename (insertExprExprConstraint) operations_research::ModelCache::InsertExprExprConstraint;
%rename (insertExprExpression) operations_research::ModelCache::InsertExprExpression;
%rename (insertExprExprExpression) operations_research::ModelCache::InsertExprExprExpression;
%rename (insertVarArrayConstantArrayExpression) operations_research::ModelCache::InsertVarArrayConstantArrayExpression;
%rename (insertVarArrayConstantExpression) operations_research::ModelCache::InsertVarArrayConstantExpression;
%rename (insertVarArrayExpression) operations_research::ModelCache::InsertVarArrayExpression;
%rename (insertVarConstantArrayExpression) operations_research::ModelCache::InsertVarConstantArrayExpression;
%rename (insertVarConstantConstantConstraint) operations_research::ModelCache::InsertVarConstantConstantConstraint;
%rename (insertVarConstantConstantExpression) operations_research::ModelCache::InsertVarConstantConstantExpression;
%rename (insertVarConstantConstraint) operations_research::ModelCache::InsertVarConstantConstraint;
%rename (insertVoidConstraint) operations_research::ModelCache::InsertVoidConstraint;

// Rename rules on RevPartialSequence.
%rename (isRanked) operations_research::RevPartialSequence::IsRanked;
%rename (numFirstRanked) operations_research::RevPartialSequence::NumFirstRanked;
%rename (numLastRanked) operations_research::RevPartialSequence::NumLastRanked;
%rename (rankFirst) operations_research::RevPartialSequence::RankFirst;
%rename (rankLast) operations_research::RevPartialSequence::RankLast;
%rename (size) operations_research::RevPartialSequence::Size;

// Rename rules on UnsortedNullableRevBitset.
%rename (activeWordSize) operations_research::UnsortedNullableRevBitset::ActiveWordSize;
%rename (empty) operations_research::UnsortedNullableRevBitset::Empty;
%rename (init) operations_research::UnsortedNullableRevBitset::Init;
%rename (intersects) operations_research::UnsortedNullableRevBitset::Intersects;
%rename (revAnd) operations_research::UnsortedNullableRevBitset::RevAnd;
%rename (revSubtract) operations_research::UnsortedNullableRevBitset::RevSubtract;

// Rename rules on Assignment.
%rename (activate) operations_research::Assignment::Activate;
%rename (activateObjective) operations_research::Assignment::ActivateObjective;
%rename (activated) operations_research::Assignment::Activated;
%rename (activatedObjective) operations_research::Assignment::ActivatedObjective;
%rename (add) operations_research::Assignment::Add;
%rename (addObjective) operations_research::Assignment::AddObjective;
%rename (backwardSequence) operations_research::Assignment::BackwardSequence;
%rename (clear) operations_research::Assignment::Clear;
%rename (contains) operations_research::Assignment::Contains;
%rename (copy) operations_research::Assignment::Copy;
%rename (copyIntersection) operations_research::Assignment::CopyIntersection;
%rename (deactivate) operations_research::Assignment::Deactivate;
%rename (deactivateObjective) operations_research::Assignment::DeactivateObjective;
%rename (durationMax) operations_research::Assignment::DurationMax;
%rename (durationMin) operations_research::Assignment::DurationMin;
%rename (durationValue) operations_research::Assignment::DurationValue;
%rename (empty) operations_research::Assignment::Empty;
%rename (endMax) operations_research::Assignment::EndMax;
%rename (endMin) operations_research::Assignment::EndMin;
%rename (endValue) operations_research::Assignment::EndValue;
%rename (fastAdd) operations_research::Assignment::FastAdd;
%rename (forwardSequence) operations_research::Assignment::ForwardSequence;
%rename (hasObjective) operations_research::Assignment::HasObjective;
%rename (intVarContainer) operations_research::Assignment::IntVarContainer;
%rename (intervalVarContainer) operations_research::Assignment::IntervalVarContainer;
%rename (load) operations_research::Assignment::Load;
%rename (mutableIntVarContainer) operations_research::Assignment::MutableIntVarContainer;
%rename (mutableIntervalVarContainer) operations_research::Assignment::MutableIntervalVarContainer;
%rename (mutableSequenceVarContainer) operations_research::Assignment::MutableSequenceVarContainer;
%rename (numIntVars) operations_research::Assignment::NumIntVars;
%rename (numIntervalVars) operations_research::Assignment::NumIntervalVars;
%rename (numSequenceVars) operations_research::Assignment::NumSequenceVars;
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
%rename (sequenceVarContainer) operations_research::Assignment::SequenceVarContainer;
%rename (setBackwardSequence) operations_research::Assignment::SetBackwardSequence;
%rename (setDurationMax) operations_research::Assignment::SetDurationMax;
%rename (setDurationMin) operations_research::Assignment::SetDurationMin;
%rename (setDurationRange) operations_research::Assignment::SetDurationRange;
%rename (setDurationValue) operations_research::Assignment::SetDurationValue;
%rename (setEndMax) operations_research::Assignment::SetEndMax;
%rename (setEndMin) operations_research::Assignment::SetEndMin;
%rename (setEndRange) operations_research::Assignment::SetEndRange;
%rename (setEndValue) operations_research::Assignment::SetEndValue;
%rename (setForwardSequence) operations_research::Assignment::SetForwardSequence;
%rename (setObjectiveMax) operations_research::Assignment::SetObjectiveMax;
%rename (setObjectiveMin) operations_research::Assignment::SetObjectiveMin;
%rename (setObjectiveRange) operations_research::Assignment::SetObjectiveRange;
%rename (setObjectiveValue) operations_research::Assignment::SetObjectiveValue;
%rename (setPerformedMax) operations_research::Assignment::SetPerformedMax;
%rename (setPerformedMin) operations_research::Assignment::SetPerformedMin;
%rename (setPerformedRange) operations_research::Assignment::SetPerformedRange;
%rename (setPerformedValue) operations_research::Assignment::SetPerformedValue;
%rename (setSequence) operations_research::Assignment::SetSequence;
%rename (setStartMax) operations_research::Assignment::SetStartMax;
%rename (setStartMin) operations_research::Assignment::SetStartMin;
%rename (setStartRange) operations_research::Assignment::SetStartRange;
%rename (setStartValue) operations_research::Assignment::SetStartValue;
%rename (setUnperformed) operations_research::Assignment::SetUnperformed;
%rename (size) operations_research::Assignment::Size;
%rename (startMax) operations_research::Assignment::StartMax;
%rename (startMin) operations_research::Assignment::StartMin;
%rename (startValue) operations_research::Assignment::StartValue;
%rename (store) operations_research::Assignment::Store;
%rename (unperformed) operations_research::Assignment::Unperformed;

// Rename rules on AssignmentContainer;
%rename (add) operations_research::AssignmentContainer::Add;
%rename (addAtPosition) operations_research::AssignmentContainer::AddAtPosition;
%rename (clear) operations_research::AssignmentContainer::Clear;
%rename (fastAdd) operations_research::AssignmentContainer::FastAdd;
%rename (resize) operations_research::AssignmentContainer::Resize;
%rename (empty) operations_research::AssignmentContainer::Empty;
%rename (copy) operations_research::AssignmentContainer::Copy;
%rename (copyIntersection) operations_research::AssignmentContainer::CopyIntersection;
%rename (contains) operations_research::AssignmentContainer::Contains;
%rename (mutableElement) operations_research::AssignmentContainer::MutableElement;
// No MutableElementOrNull
%ignore operations_research::AssignmentContainer::MutableElementOrNull;
%rename (element) operations_research::AssignmentContainer::Element;
// No ElementPtrOrNull
%ignore operations_research::AssignmentContainer::ElementPtrOrNull;
// %unignore AssignmentContainer::elements;
%rename (size) operations_research::AssignmentContainer::Size;
%rename (store) operations_research::AssignmentContainer::Store;
%rename (restore) operations_research::AssignmentContainer::Restore;

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
%ignore operations_research::IntVarElement::LoadFromProto;
// No WriteToProto
%ignore operations_research::IntVarElement::WriteToProto;
%rename (min) operations_research::IntVarElement::Min;
%rename (setMin) operations_research::IntVarElement::SetMin;
%rename (max) operations_research::IntVarElement::Max;
%rename (setMax) operations_research::IntVarElement::SetMax;
%rename (value) operations_research::IntVarElement::Value;
%rename (setValue) operations_research::IntVarElement::SetValue;
%rename (setRange) operations_research::IntVarElement::SetRange;
%rename (var) operations_research::IntVarElement::Var;

// Rename rules on IntervalVarElement
%rename (clone) operations_research::IntervalVarElement::Clone;
%rename (copy) operations_research::IntervalVarElement::Copy;
%rename (durationMax) operations_research::IntervalVarElement::DurationMax;
%rename (durationMin) operations_research::IntervalVarElement::DurationMin;
%rename (durationValue) operations_research::IntervalVarElement::DurationValue;
%rename (endMax) operations_research::IntervalVarElement::EndMax;
%rename (endMin) operations_research::IntervalVarElement::EndMin;
%rename (endValue) operations_research::IntervalVarElement::EndValue;
// No LoadFromProto
%ignore operations_research::IntervalVarElement::LoadFromProto;
%rename (performedMax) operations_research::IntervalVarElement::PerformedMax;
%rename (performedMin) operations_research::IntervalVarElement::PerformedMin;
%rename (performedValue) operations_research::IntervalVarElement::PerformedValue;
%rename (reset) operations_research::IntervalVarElement::Reset;
%rename (restore) operations_research::IntervalVarElement::Restore;
%rename (setDurationMax) operations_research::IntervalVarElement::SetDurationMax;
%rename (setDurationMin) operations_research::IntervalVarElement::SetDurationMin;
%rename (setDurationRange) operations_research::IntervalVarElement::SetDurationRange;
%rename (setDurationValue) operations_research::IntervalVarElement::SetDurationValue;
%rename (setEndMax) operations_research::IntervalVarElement::SetEndMax;
%rename (setEndMin) operations_research::IntervalVarElement::SetEndMin;
%rename (setEndRange) operations_research::IntervalVarElement::SetEndRange;
%rename (setEndValue) operations_research::IntervalVarElement::SetEndValue;
%rename (setPerformedMax) operations_research::IntervalVarElement::SetPerformedMax;
%rename (setPerformedMin) operations_research::IntervalVarElement::SetPerformedMin;
%rename (setPerformedRange) operations_research::IntervalVarElement::SetPerformedRange;
%rename (setPerformedValue) operations_research::IntervalVarElement::SetPerformedValue;
%rename (setStartMax) operations_research::IntervalVarElement::SetStartMax;
%rename (setStartMin) operations_research::IntervalVarElement::SetStartMin;
%rename (setStartRange) operations_research::IntervalVarElement::SetStartRange;
%rename (setStartValue) operations_research::IntervalVarElement::SetStartValue;
%rename (startMax) operations_research::IntervalVarElement::StartMax;
%rename (startMin) operations_research::IntervalVarElement::StartMin;
%rename (startValue) operations_research::IntervalVarElement::StartValue;
%rename (store) operations_research::IntervalVarElement::Store;
%rename (var) operations_research::IntervalVarElement::Var;
// No WriteToProto
%ignore operations_research::IntervalVarElement::WriteToProto;

// Rename rules on SequenceVarElement.
%rename (backwardSequence) operations_research::SequenceVarElement::BackwardSequence;
%rename (clone) operations_research::SequenceVarElement::Clone;
%rename (copy) operations_research::SequenceVarElement::Copy;
%rename (forwardSequence) operations_research::SequenceVarElement::ForwardSequence;
// No LoadFromProto
%ignore operations_research::SequenceVarElement::LoadFromProto;
%rename (reset) operations_research::SequenceVarElement::Reset;
%rename (restore) operations_research::SequenceVarElement::Restore;
%rename (setBackwardSequence) operations_research::SequenceVarElement::SetBackwardSequence;
%rename (setForwardSequence) operations_research::SequenceVarElement::SetForwardSequence;
%rename (setSequence) operations_research::SequenceVarElement::SetSequence;
%rename (setUnperformed) operations_research::SequenceVarElement::SetUnperformed;
%rename (store) operations_research::SequenceVarElement::Store;
%rename (unperformed) operations_research::SequenceVarElement::Unperformed;
%rename (var) operations_research::SequenceVarElement::Var;
// No WriteToProto
%ignore operations_research::SequenceVarElement::WriteToProto;

// Rename rules on SolutionCollector.
%rename (add) operations_research::SolutionCollector::Add;
%rename (addObjective) operations_research::SolutionCollector::AddObjective;
%rename (backwardSequence) operations_research::SolutionCollector::BackwardSequence;
%rename (durationValue) operations_research::SolutionCollector::DurationValue;
%rename (endValue) operations_research::SolutionCollector::EndValue;
%rename (forwardSequence) operations_research::SolutionCollector::ForwardSequence;
%rename (objectiveValue) operations_research::SolutionCollector::objective_value;
%rename (performedValue) operations_research::SolutionCollector::PerformedValue;
%rename (solutionCount) operations_research::SolutionCollector::solution_count;
%rename (startValue) operations_research::SolutionCollector::StartValue;
%rename (unperformed) operations_research::SolutionCollector::Unperformed;
%rename (wallTime) operations_research::SolutionCollector::wall_time;

// Rename rules on SolutionPool.
%rename (getNextSolution) operations_research::SolutionPool::GetNextSolution;
%rename (initialize) operations_research::SolutionPool::Initialize;
%rename (registerNewSolution) operations_research::SolutionPool::RegisterNewSolution;
%rename (syncNeeded) operations_research::SolutionPool::SyncNeeded;

// Rename rules on Solver.
%rename (acceptedNeighbors) operations_research::Solver::accepted_neighbors;
%rename (activeSearch) operations_research::Solver::ActiveSearch;
%rename (addBacktrackAction) operations_research::Solver::AddBacktrackAction;
%rename (addCastConstraint) operations_research::Solver::AddCastConstraint;
%rename (addConstraint) operations_research::Solver::AddConstraint;
%rename (addLocalSearchMonitor) operations_research::Solver::AddLocalSearchMonitor;
%rename (addPropagationMonitor) operations_research::Solver::AddPropagationMonitor;
%rename (cache) operations_research::Solver::Cache;
%rename (castExpression) operations_research::Solver::CastExpression;
%rename (checkAssignment) operations_research::Solver::CheckAssignment;
%rename (checkConstraint) operations_research::Solver::CheckConstraint;
%rename (checkFail) operations_research::Solver::CheckFail;
%rename (compose) operations_research::Solver::Compose;
%rename (concatenateOperators) operations_research::Solver::ConcatenateOperators;
%rename (currentlyInSolve) operations_research::Solver::CurrentlyInSolve;
%rename (defaultSolverParameters) operations_research::Solver::DefaultSolverParameters;
%rename (endSearch) operations_research::Solver::EndSearch;
%rename (exportProfilingOverview) operations_research::Solver::ExportProfilingOverview;
%rename (fail) operations_research::Solver::Fail;
%rename (filteredNeighbors) operations_research::Solver::filtered_neighbors;
%rename (finishCurrentSearch) operations_research::Solver::FinishCurrentSearch;
%rename (getLocalSearchMonitor) operations_research::Solver::GetLocalSearchMonitor;
%rename (getPropagationMonitor) operations_research::Solver::GetPropagationMonitor;
%rename (getTime) operations_research::Solver::GetTime;
%rename (hasName) operations_research::Solver::HasName;
%rename (instrumentsDemons) operations_research::Solver::InstrumentsDemons;
%rename (instrumentsVariables) operations_research::Solver::InstrumentsVariables;
%rename (isLocalSearchProfilingEnabled) operations_research::Solver::IsLocalSearchProfilingEnabled;
%rename (isProfilingEnabled) operations_research::Solver::IsProfilingEnabled;
%rename (localSearchProfile) operations_research::Solver::LocalSearchProfile;
%rename (makeAbs) operations_research::Solver::MakeAbs;
%rename (makeAbsEquality) operations_research::Solver::MakeAbsEquality;
%rename (makeAllDifferent) operations_research::Solver::MakeAllDifferent;
%rename (makeAllDifferentExcept) operations_research::Solver::MakeAllDifferentExcept;
%rename (makeAllSolutionCollector) operations_research::Solver::MakeAllSolutionCollector;
%rename (makeAllowedAssignment) operations_research::Solver::MakeAllowedAssignments;
%rename (makeApplyBranchSelector) operations_research::Solver::MakeApplyBranchSelector;
%rename (makeAssignVariableValue) operations_research::Solver::MakeAssignVariableValue;
%rename (makeAssignVariableValueOrFail) operations_research::Solver::MakeAssignVariableValueOrFail;
%rename (makeAssignVariablesValues) operations_research::Solver::MakeAssignVariablesValues;
%rename (makeAssignment) operations_research::Solver::MakeAssignment;
%rename (makeAtMost) operations_research::Solver::MakeAtMost;
%rename (makeAtSolutionCallback) operations_research::Solver::MakeAtSolutionCallback;
%rename (makeBestValueSolutionCollector) operations_research::Solver::MakeBestValueSolutionCollector;
%rename (makeBetweenCt) operations_research::Solver::MakeBetweenCt;
%rename (makeBoolVar) operations_research::Solver::MakeBoolVar;
%rename (makeBranchesLimit) operations_research::Solver::MakeBranchesLimit;
%rename (makeCircuit) operations_research::Solver::MakeCircuit;
%rename (makeClosureDemon) operations_research::Solver::MakeClosureDemon;
%rename (makeConditionalExpression) operations_research::Solver::MakeConditionalExpression;
%rename (makeConstantRestart) operations_research::Solver::MakeConstantRestart;
%rename (makeConstraintAdder) operations_research::Solver::MakeConstraintAdder;
%rename (makeConstraintInitialPropagateCallback) operations_research::Solver::MakeConstraintInitialPropagateCallback;
%rename (makeConvexPiecewiseExpr) operations_research::Solver::MakeConvexPiecewiseExpr;
%rename (makeCount) operations_research::Solver::MakeCount;
%rename (makeCover) operations_research::Solver::MakeCover;
%rename (makeCumulative) operations_research::Solver::MakeCumulative;
%rename (makeCustomLimit) operations_research::Solver::MakeCustomLimit;
%rename (makeDecision) operations_research::Solver::MakeDecision;
%rename (makeDecisionBuilderFromAssignment) operations_research::Solver::MakeDecisionBuilderFromAssignment;
%rename (makeDefaultPhase) operations_research::Solver::MakeDefaultPhase;
%rename (makeDefaultSearchLimitParameters) operations_research::Solver::MakeDefaultSearchLimitParameters;
%rename (makeDefaultSolutionPool) operations_research::Solver::MakeDefaultSolutionPool;
%rename (makeDelayedConstraintInitialPropagateCallback) operations_research::Solver::MakeDelayedConstraintInitialPropagateCallback;
%rename (makeDelayedPathCumul) operations_research::Solver::MakeDelayedPathCumul;
%rename (makeDeviation) operations_research::Solver::MakeDeviation;
%rename (makeDifference) operations_research::Solver::MakeDifference;
%rename (makeDisjunctiveConstraint) operations_research::Solver::MakeDisjunctiveConstraint;
%rename (makeDistribute) operations_research::Solver::MakeDistribute;
%rename (makeDiv) operations_research::Solver::MakeDiv;
%rename (makeElement) operations_research::Solver::MakeElement;
%rename (makeElementEquality) operations_research::Solver::MakeElementEquality;
%rename (makeEnterSearchCallback) operations_research::Solver::MakeEnterSearchCallback;
%rename (makeEquality) operations_research::Solver::MakeEquality;
%rename (makeExitSearchCallback) operations_research::Solver::MakeExitSearchCallback;
%rename (makeFailDecision) operations_research::Solver::MakeFailDecision;
%rename (makeFailuresLimit) operations_research::Solver::MakeFailuresLimit;
%rename (makeFalseConstraint) operations_research::Solver::MakeFalseConstraint;
%rename (makeFirstSolutionCollector) operations_research::Solver::MakeFirstSolutionCollector;
%rename (makeFixedDurationEndSyncedOnEndIntervalVar) operations_research::Solver::MakeFixedDurationEndSyncedOnEndIntervalVar;
%rename (makeFixedDurationEndSyncedOnStartIntervalVar) operations_research::Solver::MakeFixedDurationEndSyncedOnStartIntervalVar;
%rename (makeFixedDurationIntervalVar) operations_research::Solver::MakeFixedDurationIntervalVar;
%rename (makeFixedDurationStartSyncedOnEndIntervalVar) operations_research::Solver::MakeFixedDurationStartSyncedOnEndIntervalVar;
%rename (makeFixedDurationStartSyncedOnStartIntervalVar) operations_research::Solver::MakeFixedDurationStartSyncedOnStartIntervalVar;
%rename (makeFixedInterval) operations_research::Solver::MakeFixedInterval;
%rename (makeGenericTabuSearch) operations_research::Solver::MakeGenericTabuSearch;
%rename (makeGreater) operations_research::Solver::MakeGreater;
%rename (makeGreaterOrEqual) operations_research::Solver::MakeGreaterOrEqual;
%rename (makeGuidedLocalSearch) operations_research::Solver::MakeGuidedLocalSearch;
%rename (makeIfThenElseCt) operations_research::Solver::MakeIfThenElseCt;
%rename (makeIndexExpression) operations_research::Solver::MakeIndexExpression;
%rename (makeIndexOfConstraint) operations_research::Solver::MakeIndexOfConstraint;
%rename (makeIndexOfFirstMaxValueConstraint) operations_research::Solver::MakeIndexOfFirstMaxValueConstraint;
%rename (makeIndexOfFirstMinValueConstraint) operations_research::Solver::MakeIndexOfFirstMinValueConstraint;
%rename (makeIntConst) operations_research::Solver::MakeIntConst;
%rename (makeIntVar) operations_research::Solver::MakeIntVar;
%rename (makeIntervalRelaxedMax) operations_research::Solver::MakeIntervalRelaxedMax;
%rename (makeIntervalRelaxedMin) operations_research::Solver::MakeIntervalRelaxedMin;
%rename (makeIntervalVar) operations_research::Solver::MakeIntervalVar;
%rename (makeIntervalVarArray) operations_research::Solver::MakeIntervalVarArray;
%rename (makeIntervalVarRelation) operations_research::Solver::MakeIntervalVarRelation;
%rename (makeIntervalVarRelationWithDelay) operations_research::Solver::MakeIntervalVarRelationWithDelay;
%rename (makeInversePermutationConstraint) operations_research::Solver::MakeInversePermutationConstraint;
%rename (makeIsBetweenCt) operations_research::Solver::MakeIsBetweenCt;
%rename (makeIsBetweenVar) operations_research::Solver::MakeIsBetweenVar;
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
%rename (makeLexicalLess) operations_research::Solver::MakeLexicalLess;
%rename (makeLexicalLessOrEqual) operations_research::Solver::MakeLexicalLessOrEqual;
%rename (makeLimit) operations_research::Solver::MakeLimit;
%rename (makeLocalSearchPhase) operations_research::Solver::MakeLocalSearchPhase;
%rename (makeLocalSearchPhaseParameters) operations_research::Solver::MakeLocalSearchPhaseParameters;
%rename (makeLubyRestart) operations_research::Solver::MakeLubyRestart;
%rename (makeMapDomain) operations_research::Solver::MakeMapDomain;
%rename (makeMax) operations_research::Solver::MakeMax;
%rename (makeMaxEquality) operations_research::Solver::MakeMaxEquality;
%rename (makeMaximize) operations_research::Solver::MakeMaximize;
%rename (makeMemberCt) operations_research::Solver::MakeMemberCt;
%rename (makeMin) operations_research::Solver::MakeMin;
%rename (makeMinEquality) operations_research::Solver::MakeMinEquality;
%rename (makeMinimize) operations_research::Solver::MakeMinimize;
%rename (makeMirrorInterval) operations_research::Solver::MakeMirrorInterval;
%rename (makeModulo) operations_research::Solver::MakeModulo;
%rename (makeMonotonicElement) operations_research::Solver::MakeMonotonicElement;
%rename (makeMoveTowardTargetOperator) operations_research::Solver::MakeMoveTowardTargetOperator;
%rename (makeNBestValueSolutionCollector) operations_research::Solver::MakeNBestValueSolutionCollector;
%rename (makeNeighborhoodLimit) operations_research::Solver::MakeNeighborhoodLimit;
%rename (makeNestedOptimize) operations_research::Solver::MakeNestedOptimize;
%rename (makeNoCycle) operations_research::Solver::MakeNoCycle;
%rename (makeNonEquality) operations_research::Solver::MakeNonEquality;
%rename (makeNonOverlappingBoxesConstraint) operations_research::Solver::MakeNonOverlappingBoxesConstraint;
%rename (makeNonOverlappingNonStrictBoxesConstraint) operations_research::Solver::MakeNonOverlappingNonStrictBoxesConstraint;
%rename (makeNotBetweenCt) operations_research::Solver::MakeNotBetweenCt;
%rename (makeNotMemberCt) operations_research::Solver::MakeNotMemberCt;
%rename (makeNullIntersect) operations_research::Solver::MakeNullIntersect;
%rename (makeNullIntersectExcept) operations_research::Solver::MakeNullIntersectExcept;
%rename (makeOperator) operations_research::Solver::MakeOperator;
%rename (makeOpposite) operations_research::Solver::MakeOpposite;
%rename (makeOptimize) operations_research::Solver::MakeOptimize;
%rename (makePack) operations_research::Solver::MakePack;
%rename (makePathConnected) operations_research::Solver::MakePathConnected;
%rename (makePathCumul) operations_research::Solver::MakePathCumul;
%rename (makePhase) operations_research::Solver::MakePhase;
%rename (makePower) operations_research::Solver::MakePower;
%rename (makePrintModelVisitor) operations_research::Solver::MakePrintModelVisitor;
%rename (makeProd) operations_research::Solver::MakeProd;
%rename (makeRandomLnsOperator) operations_research::Solver::MakeRandomLnsOperator;
%rename (makeRankFirstInterval) operations_research::Solver::MakeRankFirstInterval;
%rename (makeRankLastInterval) operations_research::Solver::MakeRankLastInterval;
%rename (makeRestoreAssignment) operations_research::Solver::MakeRestoreAssignment;
%rename (makeScalProd) operations_research::Solver::MakeScalProd;
%rename (makeScalProdEquality) operations_research::Solver::MakeScalProdEquality;
%rename (makeScalProdGreaterOrEqual) operations_research::Solver::MakeScalProdGreaterOrEqual;
%rename (makeScalProdLessOrEqual) operations_research::Solver::MakeScalProdLessOrEqual;
%rename (makeScheduleOrExpedite) operations_research::Solver::MakeScheduleOrExpedite;
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
%rename (makeStatisticsModelVisitor) operations_research::Solver::MakeStatisticsModelVisitor;
%rename (makeStoreAssignment) operations_research::Solver::MakeStoreAssignment;
%rename (makeStrictDisjunctiveConstraint) operations_research::Solver::MakeStrictDisjunctiveConstraint;
%rename (makeSubCircuit) operations_research::Solver::MakeSubCircuit;
%rename (makeSum) operations_research::Solver::MakeSum;
%rename (makeSumEquality) operations_research::Solver::MakeSumEquality;
%rename (makeSumGreaterOrEqual) operations_research::Solver::MakeSumGreaterOrEqual;
%rename (makeSumLessOrEqual) operations_research::Solver::MakeSumLessOrEqual;
%rename (makeSumObjectiveFilter) operations_research::Solver::MakeSumObjectiveFilter;
%rename (makeSymmetryManager) operations_research::Solver::MakeSymmetryManager;
%rename (makeTabuSearch) operations_research::Solver::MakeTabuSearch;
%rename (makeTemporalDisjunction) operations_research::Solver::MakeTemporalDisjunction;
%rename (makeTimeLimit) operations_research::Solver::MakeTimeLimit;
%rename (makeTransitionConstraint) operations_research::Solver::MakeTransitionConstraint;
%rename (makeTreeMonitor) operations_research::Solver::MakeTreeMonitor;
%rename (makeTrueConstraint) operations_research::Solver::MakeTrueConstraint;
%rename (makeVariableDomainFilter) operations_research::Solver::MakeVariableDomainFilter;
%rename (makeVariableGreaterOrEqualValue) operations_research::Solver::MakeVariableGreaterOrEqualValue;
%rename (makeVariableLessOrEqualValue) operations_research::Solver::MakeVariableLessOrEqualValue;
%rename (makeWeightedMaximize) operations_research::Solver::MakeWeightedMaximize;
%rename (makeWeightedMinimize) operations_research::Solver::MakeWeightedMinimize;
%rename (makeWeightedOptimize) operations_research::Solver::MakeWeightedOptimize;
%rename (memoryUsage) operations_research::Solver::MemoryUsage;
%rename (nameAllVariables) operations_research::Solver::NameAllVariables;
%rename (newSearch) operations_research::Solver::NewSearch;
%rename (nextSolution) operations_research::Solver::NextSolution;
%rename (popState) operations_research::Solver::PopState;
%rename (pushState) operations_research::Solver::PushState;
%rename (rand32) operations_research::Solver::Rand32;
%rename (rand64) operations_research::Solver::Rand64;
%rename (randomConcatenateOperators) operations_research::Solver::RandomConcatenateOperators;
%rename (rankFirst) operations_research::SequenceVar::RankFirst;
%rename (rankLast) operations_research::SequenceVar::RankLast;
%rename (rankNotFirst) operations_research::SequenceVar::RankNotFirst;
%rename (rankNotLast) operations_research::SequenceVar::RankNotLast;
%rename (rankSequence) operations_research::SequenceVar::RankSequence;
%rename (reSeed) operations_research::Solver::ReSeed;
%rename (registerDemon) operations_research::Solver::RegisterDemon;
%rename (registerIntExpr) operations_research::Solver::RegisterIntExpr;
%rename (registerIntVar) operations_research::Solver::RegisterIntVar;
%rename (registerIntervalVar) operations_research::Solver::RegisterIntervalVar;
%rename (restartCurrentSearch) operations_research::Solver::RestartCurrentSearch;
%rename (restartSearch) operations_research::Solver::RestartSearch;
%rename (searchContext) operations_research::Solver::SearchContext;
%rename (searchDepth) operations_research::Solver::SearchDepth;
%rename (searchLeftDepth) operations_research::Solver::SearchLeftDepth;
%rename (setBranchSelector) operations_research::Solver::SetBranchSelector;
%rename (setSearchContext) operations_research::Solver::SetSearchContext;
%rename (shouldFail) operations_research::Solver::ShouldFail;
%rename (solve) operations_research::Solver::Solve;
%rename (solveAndCommit) operations_research::Solver::SolveAndCommit;
%rename (solveDepth) operations_research::Solver::SolveDepth;
%rename (topPeriodicCheck) operations_research::Solver::TopPeriodicCheck;
%rename (topProgressPercent) operations_research::Solver::TopProgressPercent;
%rename (tryDecisions) operations_research::Solver::Try;
%rename (updateLimits) operations_research::Solver::UpdateLimits;
%rename (wallTime) operations_research::Solver::wall_time;

// Rename rules on IntExpr.
%rename (castToVar) operations_research::BaseIntExpr::CastToVar;
%rename (isVar) operations_research::IntExpr::IsVar;
%rename (range) operations_research::IntExpr::Range;
%rename (var) operations_research::IntExpr::Var;
%rename (varWithName) operations_research::IntExpr::VarWithName;
%rename (whenRange) operations_research::IntExpr::WhenRange;

// Rename rules on IntVar.
%rename (addName) operations_research::IntVar::AddName;
%rename (contains) operations_research::IntVar::Contains;
%rename (isDifferent) operations_research::IntVar::IsDifferent;
%rename (isEqual) operations_research::IntVar::IsEqual;
%rename (isGreaterOrEqual) operations_research::IntVar::IsGreaterOrEqual;
%rename (isLessOrEqual) operations_research::IntVar::IsLessOrEqual;
%rename (makeDomainIterator) operations_research::IntVar::MakeDomainIterator;
%rename (makeHoleIterator) operations_research::IntVar::MakeHoleIterator;
%rename (oldMax) operations_research::IntVar::OldMax;
%rename (oldMin) operations_research::IntVar::OldMin;
%rename (removeInterval) operations_research::IntVar::RemoveInterval;
%rename (removeValue) operations_research::IntVar::RemoveValue;
%rename (removeValues) operations_research::IntVar::RemoveValues;
%rename (size) operations_research::IntVar::Size;
%rename (varType) operations_research::IntVar::VarType;
%rename (whenBound) operations_research::IntVar::WhenBound;
%rename (whenDomain) operations_research::IntVar::WhenDomain;

// Rename rules on IntVarIterator.
%rename (init) operations_research::IntVarIterator::Init;
%rename (next) operations_research::IntVarIterator::Next;
%rename (ok) operations_research::IntVarIterator::Ok;

// Rename rules on BooleanVar.
%rename (baseName) operations_research::BooleanVar::BaseName;
%rename (isDifferent) operations_research::BooleanVar::IsDifferent;
%rename (isEqual) operations_research::BooleanVar::IsEqual;
%rename (isGreaterOrEqual) operations_research::BooleanVar::IsGreaterOrEqual;
%rename (isLessOrEqual) operations_research::BooleanVar::IsLessOrEqual;
%rename (makeDomainIterator) operations_research::BooleanVar::MakeDomainIterator;
%rename (makeHoleIterator) operations_research::BooleanVar::MakeHoleIterator;
%rename (rawValue) operations_research::BooleanVar::RawValue;
%rename (restoreValue) operations_research::BooleanVar::RestoreValue;
%rename (size) operations_research::BooleanVar::Size;
%rename (varType) operations_research::BooleanVar::VarType;
%rename (whenBound) operations_research::BooleanVar::WhenBound;
%rename (whenDomain) operations_research::BooleanVar::WhenDomain;
%rename (whenRange) operations_research::BooleanVar::WhenRange;

// Rename rules on IntervalVar.
%rename (cannotBePerformed) operations_research::IntervalVar::CannotBePerformed;
%rename (durationExpr) operations_research::IntervalVar::DurationExpr;
%rename (durationMax) operations_research::IntervalVar::DurationMax;
%rename (durationMin) operations_research::IntervalVar::DurationMin;
%rename (endExpr) operations_research::IntervalVar::EndExpr;
%rename (endMax) operations_research::IntervalVar::EndMax;
%rename (endMin) operations_research::IntervalVar::EndMin;
%rename (isPerformedBound) operations_research::IntervalVar::IsPerformedBound;
%rename (mayBePerformed) operations_research::IntervalVar::MayBePerformed;
%rename (mustBePerformed) operations_research::IntervalVar::MustBePerformed;
%rename (oldDurationMax) operations_research::IntervalVar::OldDurationMax;
%rename (oldDurationMin) operations_research::IntervalVar::OldDurationMin;
%rename (oldEndMax) operations_research::IntervalVar::OldEndMax;
%rename (oldEndMin) operations_research::IntervalVar::OldEndMin;
%rename (oldStartMax) operations_research::IntervalVar::OldStartMax;
%rename (oldStartMin) operations_research::IntervalVar::OldStartMin;
%rename (performedExpr) operations_research::IntervalVar::PerformedExpr;
%rename (safeDurationExpr) operations_research::IntervalVar::SafeDurationExpr;
%rename (safeEndExpr) operations_research::IntervalVar::SafeEndExpr;
%rename (safeStartExpr) operations_research::IntervalVar::SafeStartExpr;
%rename (setDurationMax) operations_research::IntervalVar::SetDurationMax;
%rename (setDurationMin) operations_research::IntervalVar::SetDurationMin;
%rename (setDurationRange) operations_research::IntervalVar::SetDurationRange;
%rename (setEndMax) operations_research::IntervalVar::SetEndMax;
%rename (setEndMin) operations_research::IntervalVar::SetEndMin;
%rename (setEndRange) operations_research::IntervalVar::SetEndRange;
%rename (setPerformed) operations_research::IntervalVar::SetPerformed;
%rename (setStartMax) operations_research::IntervalVar::SetStartMax;
%rename (setStartMin) operations_research::IntervalVar::SetStartMin;
%rename (setStartRange) operations_research::IntervalVar::SetStartRange;
%rename (startExpr) operations_research::IntervalVar::StartExpr;
%rename (startMax) operations_research::IntervalVar::StartMax;
%rename (startMin) operations_research::IntervalVar::StartMin;
%rename (wasPerformedBound) operations_research::IntervalVar::WasPerformedBound;
%rename (whenAnything) operations_research::IntervalVar::WhenAnything;
%rename (whenDurationBound) operations_research::IntervalVar::WhenDurationBound;
%rename (whenDurationRange) operations_research::IntervalVar::WhenDurationRange;
%rename (whenEndBound) operations_research::IntervalVar::WhenEndBound;
%rename (whenEndRange) operations_research::IntervalVar::WhenEndRange;
%rename (whenPerformedBound) operations_research::IntervalVar::WhenPerformedBound;
%rename (whenStartBound) operations_research::IntervalVar::WhenStartBound;
%rename (whenStartRange) operations_research::IntervalVar::WhenStartRange;

// Rename rules on OptimizeVar.
%rename (applyBound) operations_research::OptimizeVar::ApplyBound;
%rename (print) operations_research::OptimizeVar::Print;
%rename (var) operations_research::OptimizeVar::Var;

// Rename rules on SequenceVar.
%rename (computePossibleFirstsAndLasts) operations_research::SequenceVar::ComputePossibleFirstsAndLasts;
%rename (fillSequence) operations_research::SequenceVar::FillSequence;
%rename (interval) operations_research::SequenceVar::Interval;
%rename (next) operations_research::SequenceVar::Next;

// Rename rules on Constraint.
%rename (initialPropagate) operations_research::Constraint::InitialPropagate;
%rename (isCastConstraint) operations_research::Constraint::IsCastConstraint;
%rename (postAndPropagate) operations_research::Constraint::PostAndPropagate;
%rename (post) operations_research::Constraint::Post;
%rename (var) operations_research::Constraint::Var;

// Rename rule on Disjunctive Constraint.
%rename (makeSequenceVar) operations_research::DisjunctiveConstraint::MakeSequenceVar;
%rename (setTransitionTime) operations_research::DisjunctiveConstraint::SetTransitionTime;
%rename (transitionTime) operations_research::DisjunctiveConstraint::TransitionTime;

// Rename rule on Pack.
%rename (addCountAssignedItemsDimension) operations_research::Pack::AddCountAssignedItemsDimension;
%rename (addCountUsedBinDimension) operations_research::Pack::AddCountUsedBinDimension;
%rename (addSumVariableWeightsLessOrEqualConstantDimension) operations_research::Pack::AddSumVariableWeightsLessOrEqualConstantDimension;
%rename (addWeightedSumEqualVarDimension) operations_research::Pack::AddWeightedSumEqualVarDimension;
%rename (addWeightedSumLessOrEqualConstantDimension) operations_research::Pack::AddWeightedSumLessOrEqualConstantDimension;
%rename (addWeightedSumOfAssignedDimension) operations_research::Pack::AddWeightedSumOfAssignedDimension;
%rename (assignAllPossibleToBin) operations_research::Pack::AssignAllPossibleToBin;
%rename (assignAllRemainingItems) operations_research::Pack::AssignAllRemainingItems;
%rename (assignFirstPossibleToBin) operations_research::Pack::AssignFirstPossibleToBin;
%rename (assign) operations_research::Pack::Assign;
%rename (assignVar) operations_research::Pack::AssignVar;
%rename (clearAll) operations_research::Pack::ClearAll;
%rename (isAssignedStatusKnown) operations_research::Pack::IsAssignedStatusKnown;
%rename (isPossible) operations_research::Pack::IsPossible;
%rename (isUndecided) operations_research::Pack::IsUndecided;
%rename (oneDomain) operations_research::Pack::OneDomain;
%rename (propagateDelayed) operations_research::Pack::PropagateDelayed;
%rename (propagate) operations_research::Pack::Propagate;
%rename (removeAllPossibleFromBin) operations_research::Pack::RemoveAllPossibleFromBin;
%rename (setAssigned) operations_research::Pack::SetAssigned;
%rename (setImpossible) operations_research::Pack::SetImpossible;
%rename (setUnassigned) operations_research::Pack::SetUnassigned;
%rename (unassignAllRemainingItems) operations_research::Pack::UnassignAllRemainingItems;

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
%rename (toString) *::DebugString;

// Rename rules on PropagationBaseObject.
%rename (baseName) operations_research::PropagationBaseObject::BaseName;
%rename (enqueueAll) operations_research::PropagationBaseObject::EnqueueAll;
%rename (enqueueDelayedDemon) operations_research::PropagationBaseObject::EnqueueDelayedDemon;
%rename (enqueueVar) operations_research::PropagationBaseObject::EnqueueVar;
%rename (executeAll) operations_research::PropagationBaseObject::ExecuteAll;
%rename (freezeQueue) operations_research::PropagationBaseObject::FreezeQueue;
%rename (hasName) operations_research::PropagationBaseObject::HasName;
%rename (setName) operations_research::PropagationBaseObject::set_name;
%rename (unfreezeQueue) operations_research::PropagationBaseObject::UnfreezeQueue;

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
%rename (exitSearch) operations_research::SearchMonitor::ExitSearch;
%rename (finishCurrentSearch) operations_research::SearchMonitor::FinishCurrentSearch;
%rename (install) operations_research::SearchMonitor::Install;
%rename (localOptimum) operations_research::SearchMonitor::LocalOptimum;
%rename (noMoreSolutions) operations_research::SearchMonitor::NoMoreSolutions;
%rename (periodicCheck) operations_research::SearchMonitor::PeriodicCheck;
%rename (progressPercent) operations_research::SearchMonitor::ProgressPercent;
%rename (refuteDecision) operations_research::SearchMonitor::RefuteDecision;
%rename (restartCurrentSearch) operations_research::SearchMonitor::RestartCurrentSearch;
%rename (restartSearch) operations_research::SearchMonitor::RestartSearch;

// Rename rules on SearchLimit.
%rename (check) operations_research::SearchLimit::Check;
%rename (copy) operations_research::SearchLimit::Copy;
%rename (init) operations_research::SearchLimit::Init;
%rename (makeClone) operations_research::SearchLimit::MakeClone;

// Rename rules on SearchLog.
%rename (maintain) operations_research::SearchLog::Maintain;
%rename (outputDecision) operations_research::SearchLog::OutputDecision;

// Rename rules on LocalSearchMonitor.
%rename (beginAcceptNeighbor) operations_research::LocalSearchMonitor::BeginAcceptNeighbor;
%rename (beginFiltering) operations_research::LocalSearchMonitor::BeginFiltering;
%rename (beginFilterNeighbor) operations_research::LocalSearchMonitor::BeginFilterNeighbor;
%rename (beginMakeNextNeighbor) operations_research::LocalSearchMonitor::BeginMakeNextNeighbor;
%rename (beginOperatorStart) operations_research::LocalSearchMonitor::BeginOperatorStart;
%rename (endAcceptNeighbor) operations_research::LocalSearchMonitor::EndAcceptNeighbor;
%rename (endFiltering) operations_research::LocalSearchMonitor::EndFiltering;
%rename (endFilterNeighbor) operations_research::LocalSearchMonitor::EndFilterNeighbor;
%rename (endMakeNextNeighbor) operations_research::LocalSearchMonitor::EndMakeNextNeighbor;
%rename (endOperatorStart) operations_research::LocalSearchMonitor::EndOperatorStart;

// Rename rules on PropagationMonitor.
%rename (beginConstraintInitialPropagation) operations_research::PropagationMonitor::BeginConstraintInitialPropagation;
%rename (beginDemonRun) operations_research::PropagationMonitor::BeginDemonRun;
%rename (beginNestedConstraintInitialPropagation) operations_research::PropagationMonitor::BeginNestedConstraintInitialPropagation;
%rename (endConstraintInitialPropagation) operations_research::PropagationMonitor::EndConstraintInitialPropagation;
%rename (endDemonRun) operations_research::PropagationMonitor::EndDemonRun;
%rename (endNestedConstraintInitialPropagation) operations_research::PropagationMonitor::EndNestedConstraintInitialPropagation;
%rename (endProcessingIntegerVariable) operations_research::PropagationMonitor::EndProcessingIntegerVariable;
%rename (install) operations_research::PropagationMonitor::Install;
%rename (popContext) operations_research::PropagationMonitor::PopContext;
%rename (pushContext) operations_research::PropagationMonitor::PushContext;
%rename (rankFirst) operations_research::PropagationMonitor::RankFirst;
%rename (rankLast) operations_research::PropagationMonitor::RankLast;
%rename (rankNotFirst) operations_research::PropagationMonitor::RankNotFirst;
%rename (rankNotLast) operations_research::PropagationMonitor::RankNotLast;
%rename (rankSequence) operations_research::PropagationMonitor::RankSequence;
%rename (registerDemon) operations_research::PropagationMonitor::RegisterDemon;
%rename (removeInterval) operations_research::PropagationMonitor::RemoveInterval;
%rename (removeValue) operations_research::PropagationMonitor::RemoveValue;
%rename (removeValues) operations_research::PropagationMonitor::RemoveValues;
%rename (setDurationMax) operations_research::PropagationMonitor::SetDurationMax;
%rename (setDurationMin) operations_research::PropagationMonitor::SetDurationMin;
%rename (setDurationRange) operations_research::PropagationMonitor::SetDurationRange;
%rename (setEndMax) operations_research::PropagationMonitor::SetEndMax;
%rename (setEndMin) operations_research::PropagationMonitor::SetEndMin;
%rename (setEndRange) operations_research::PropagationMonitor::SetEndRange;
%rename (setPerformed) operations_research::PropagationMonitor::SetPerformed;
%rename (setStartMax) operations_research::PropagationMonitor::SetStartMax;
%rename (setStartMin) operations_research::PropagationMonitor::SetStartMin;
%rename (setStartRange) operations_research::PropagationMonitor::SetStartRange;
%rename (startProcessingIntegerVariable) operations_research::PropagationMonitor::StartProcessingIntegerVariable;

// Rename rules on IntVarLocalSearchHandler.
%rename (addToAssignment) operations_research::IntVarLocalSearchHandler::AddToAssignment;
%rename (onAddVars) operations_research::IntVarLocalSearchHandler::OnAddVars;
%rename (onRevertChanges) operations_research::IntVarLocalSearchHandler::OnRevertChanges;
%rename (valueFromAssignent) operations_research::IntVarLocalSearchHandler::ValueFromAssignent;

// LocalSearchOperator
%feature("director") operations_research::LocalSearchOperator;
%rename (nextNeighbor) operations_research::LocalSearchOperator::MakeNextNeighbor;
%rename (reset) operations_research::LocalSearchOperator::Reset;
%rename (start) operations_research::LocalSearchOperator::Start;

// VarLocalSearchOperator<>
// Ignored:
// - Start()
// - SkipUnchanged()
// - ApplyChanges()
// - RevertChanges()
%ignore operations_research::VarLocalSearchOperator::ApplyChanges;
%ignore operations_research::VarLocalSearchOperator::RevertChanges;
%ignore operations_research::VarLocalSearchOperator::SkipUnchanged;
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
%ignore operations_research::SequenceVarLocalSearchOperator::OldSequence;
%ignore operations_research::SequenceVarLocalSearchOperator::Sequence;
%ignore operations_research::SequenceVarLocalSearchOperator::SetBackwardSequence;
%ignore operations_research::SequenceVarLocalSearchOperator::SetForwardSequence;
%rename (start) operations_research::SequenceVarLocalSearchOperator::Start;

// Rename rules on SequenceVarLocalSearchHandler.
%rename (addToAssignment) operations_research::SequenceVarLocalSearchHandler::AddToAssignment;
%rename (onAddVars) operations_research::SequenceVarLocalSearchHandler::OnAddVars;
%rename (onRevertChanges) operations_research::SequenceVarLocalSearchHandler::OnRevertChanges;
%rename (valueFromAssignent) operations_research::SequenceVarLocalSearchHandler::ValueFromAssignent;


// PathOperator
// Ignored:
// - SkipUnchanged()
// - Next()
// - Path()
// - number_of_nexts()
%feature("director") operations_research::PathOperator;
%ignore operations_research::PathOperator::Next;
%ignore operations_research::PathOperator::Path;
%ignore operations_research::PathOperator::SkipUnchanged;
%rename (neighbor) operations_research::PathOperator::MakeNeighbor;

// Rename rules on PathWithPreviousNodesOperator.
%rename (isPathStart) operations_research::PathWithPreviousNodesOperator::IsPathStart;
%rename (prev) operations_research::PathWithPreviousNodesOperator::Prev;

// LocalSearchFilter
%feature("director") operations_research::LocalSearchFilter;
%rename (accept) operations_research::LocalSearchFilter::Accept;
%rename (getAcceptedObjectiveValue) operations_research::LocalSearchFilter::GetAcceptedObjectiveValue;
%rename (getSynchronizedObjectiveValue) operations_research::LocalSearchFilter::GetSynchronizedObjectiveValue;
%rename (isIncremental) operations_research::LocalSearchFilter::IsIncremental;
%rename (synchronize) operations_research::LocalSearchFilter::Synchronize;

// IntVarLocalSearchFilter
// Ignored:
// - IsVarSynced()
%feature("director") operations_research::IntVarLocalSearchFilter;
%feature("nodirector") operations_research::IntVarLocalSearchFilter::Synchronize;  // Inherited.
%ignore operations_research::IntVarLocalSearchFilter::FindIndex;
%ignore operations_research::IntVarLocalSearchFilter::IsVarSynced;
%rename (addVars) operations_research::IntVarLocalSearchFilter::AddVars;  // Inherited.
%rename (injectObjectiveValue) operations_research::IntVarLocalSearchFilter::InjectObjectiveValue;
%rename (isIncremental) operations_research::IntVarLocalSearchFilter::IsIncremental;
%rename (onSynchronize) operations_research::IntVarLocalSearchFilter::OnSynchronize;
%rename (size) operations_research::IntVarLocalSearchFilter::Size;
%rename (start) operations_research::IntVarLocalSearchFilter::Start;
%rename (value) operations_research::IntVarLocalSearchFilter::Value;
%rename (var) operations_research::IntVarLocalSearchFilter::Var;  // Inherited.

// Demon.
%rename (run) operations_research::Demon::Run;

namespace operations_research {

%define CONVERT_VECTOR(CType, JavaType)
CONVERT_VECTOR_WITH_CAST(CType, JavaType, REINTERPRET_CAST,
    com/google/ortools/constraintsolver);
%enddef

CONVERT_VECTOR(operations_research::IntVar, IntVar);
CONVERT_VECTOR(operations_research::SearchMonitor, SearchMonitor);
CONVERT_VECTOR(operations_research::DecisionBuilder, DecisionBuilder);
CONVERT_VECTOR(operations_research::IntervalVar, IntervalVar);
CONVERT_VECTOR(operations_research::SequenceVar, SequenceVar);
CONVERT_VECTOR(operations_research::LocalSearchOperator, LocalSearchOperator);
CONVERT_VECTOR(operations_research::LocalSearchFilter, LocalSearchFilter);
CONVERT_VECTOR(operations_research::SymmetryBreaker, SymmetryBreaker);

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
    IntToLong,
    "com/google/ortools/constraintsolver/",
    int64, Long, int)
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

// IMPORTANT(corentinl): These functions from constraint_solveri.h are global, so in
// java they are in the main.java (import com.[...].constraintsolver.main).
%rename (areAllBooleans) operations_research::AreAllBooleans;
%rename (areAllBound) operations_research::AreAllBound;
%rename (areAllBoundTo) operations_research::AreAllBoundTo;
%rename (fillValues) operations_research::FillValues;
%rename (maxVarArray) operations_research::MaxVarArray;
%rename (minVarArray) operations_research::MinVarArray;
%rename (posIntDivDown) operations_research::PosIntDivDown;
%rename (posIntDivUp) operations_research::PosIntDivUp;
%rename (setAssignmentFromAssignment) operations_research::SetAssignmentFromAssignment;
%rename (zero) operations_research::Zero;

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
%template(IntContainer) AssignmentContainer<IntVar, IntVarElement>;
%template(IntervalContainer) AssignmentContainer<IntervalVar, IntervalVarElement>;
%template(SequenceContainer) AssignmentContainer<SequenceVar,SequenceVarElement>;
}  // namespace operations_research
