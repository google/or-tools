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

/* Global JNI reference deleter */
class GlobalRefGuard {
  JNIEnv *jenv_;
  jobject jref_;
  // non-copyable
  GlobalRefGuard(const GlobalRefGuard &) = delete;
  GlobalRefGuard &operator=(const GlobalRefGuard &) = delete;
  public:
  GlobalRefGuard(JNIEnv *jenv, jobject jref): jenv_(jenv), jref_(jref) {}
  ~GlobalRefGuard() { jenv_->DeleteGlobalRef(jref_); }
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

%module(directors="1") operations_research;

%{
#include <setjmp.h>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
%}

// Use to correctly wrap Solver::MakeScheduleOrPostpone.
%apply int64 * INOUT { int64 *const marker };
// Use to correctly wrap arguments otherwise SWIG will wrap them as
// SWIGTYPE_p_long_long opaque pointer.
%apply int64 * OUTPUT { int64 *l, int64 *u, int64 *value };

// Types in Proxy class (e.g. Solver.java) e.g.:
// Solver::f(jstype $javainput, ...) {Solver_f_SWIG(javain, ...);}
#define VAR_ARGS(X...) X
// Methods taking parameters
%define DEFINE_ARGS_TO_R_CALLBACK(
  TYPE,
  JAVA_TYPE, JAVA_METHOD, JAVA_SIGN,
  LAMBDA_RETURN, JNI_METHOD, LAMBDA_PARAM, LAMBDA_CALL)
  %typemap(in) TYPE %{
    jclass $input_object_class = jenv->GetObjectClass($input);
    if (nullptr == $input_object_class) return $null;
    jmethodID $input_method_id = jenv->GetMethodID(
      $input_object_class, JAVA_METHOD, JAVA_SIGN);
    assert($input_method_id != nullptr);
    // $input will be deleted once this function return.
    jobject $input_object = jenv->NewGlobalRef($input);

    // Global JNI reference deleter
    auto $input_guard = std::make_shared<GlobalRefGuard>(jenv, $input_object);
    $1 = [jenv, $input_object, $input_method_id, $input_guard](LAMBDA_PARAM) -> LAMBDA_RETURN {
      return jenv->JNI_METHOD($input_object, $input_method_id, LAMBDA_CALL);
    };
  %}
  // These 4 typemaps tell SWIG which JNI and Java types to use.
  %typemap(jni) TYPE "jobject" // Type used in the JNI C.
  %typemap(jtype) TYPE "JAVA_TYPE" // Type used in the JNI.java.
  %typemap(jstype) TYPE "JAVA_TYPE" // Type used in the Proxy class.
  %typemap(javain) TYPE "$javainput" // passing the Callback to JNI java class.
%enddef

// Method taking no parameters
%define DEFINE_VOID_TO_R_CALLBACK(
  TYPE,
  JAVA_TYPE, JAVA_METHOD, JAVA_SIGN,
  LAMBDA_RETURN, JNI_METHOD)
  %typemap(in) TYPE %{
    jclass $input_object_class = jenv->GetObjectClass($input);
    if (nullptr == $input_object_class) return $null;
    jmethodID $input_method_id = jenv->GetMethodID(
      $input_object_class, JAVA_METHOD, JAVA_SIGN);
    assert($input_method_id != nullptr);
    // $input will be deleted once this function return.
    jobject $input_object = jenv->NewGlobalRef($input);

    // Global JNI reference deleter
    auto $input_guard = std::make_shared<GlobalRefGuard>(jenv, $input_object);
    $1 = [jenv, $input_object, $input_method_id, $input_guard]() -> LAMBDA_RETURN {
      return jenv->JNI_METHOD($input_object, $input_method_id);
    };
  %}
  // These 4 typemaps tell SWIG which JNI and Java types to use.
  %typemap(jni) TYPE "jobject" // Type used in the JNI C.
  %typemap(jtype) TYPE "JAVA_TYPE" // Type used in the JNI.java.
  %typemap(jstype) TYPE "JAVA_TYPE" // Type used in the Proxy class.
  %typemap(javain) TYPE "$javainput" // passing the Callback to JNI java class.
%enddef

// Method taking no parameters and returning a std::string
%define DEFINE_VOID_TO_STRING_CALLBACK(
  TYPE,
  JAVA_TYPE, JAVA_METHOD, JAVA_SIGN)
  %typemap(in) TYPE %{
    jclass $input_object_class = jenv->GetObjectClass($input);
    if (nullptr == $input_object_class) return $null;
    jmethodID $input_method_id = jenv->GetMethodID(
      $input_object_class, JAVA_METHOD, JAVA_SIGN);
    assert($input_method_id != nullptr);
    // $input will be deleted once this function return.
    jobject $input_object = jenv->NewGlobalRef($input);

    // Global JNI reference deleter
    auto $input_guard = std::make_shared<GlobalRefGuard>(jenv, $input_object);
    $1 = [jenv, $input_object, $input_method_id, $input_guard]() -> std::string {
      jstring js = (jstring) jenv->CallObjectMethod($input_object, $input_method_id);
      // convert the Java String to const char* C string.
      const char* c_str(jenv->GetStringUTFChars(js, 0));
      // copy the C string to std::string
      std::string str(c_str);
      // release the C string.
      jenv->ReleaseStringUTFChars(js, c_str);
      return str;
    };
  %}
  // These 4 typemaps tell SWIG which JNI and Java types to use.
  %typemap(jni) TYPE "jobject" // Type used in the JNI C.
  %typemap(jtype) TYPE "JAVA_TYPE" // Type used in the JNI.java.
  %typemap(jstype) TYPE "JAVA_TYPE" // Type used in the Proxy class.
  %typemap(javain) TYPE "$javainput" // passing the Callback to JNI java class.
%enddef

// Method taking a solver as parameter and returning nothing
%define DEFINE_SOLVER_TO_VOID_CALLBACK(
  TYPE,
  JAVA_TYPE, JAVA_METHOD, JAVA_SIGN)
  %typemap(in) TYPE %{
    jclass $input_object_class = jenv->GetObjectClass($input);
    if (nullptr == $input_object_class) return $null;
    jmethodID $input_method_id = jenv->GetMethodID(
      $input_object_class, JAVA_METHOD, JAVA_SIGN);
    assert($input_method_id != nullptr);
    // $input will be deleted once this function return.
    jobject $input_object = jenv->NewGlobalRef($input);

    // Global JNI reference deleter
    auto $input_guard = std::make_shared<GlobalRefGuard>(jenv, $input_object);
    $1 = [jenv, $input_object, $input_method_id,
    $input_guard](operations_research::Solver* solver) -> void {
      jclass solver_class = jenv->FindClass(
          "com/google/ortools/constraintsolver/Solver");
      assert(nullptr != solver_class);
      jmethodID solver_constructor = jenv->GetMethodID(solver_class, "<init>", "(JZ)V");
      assert(nullptr != solver_constructor);

      // Create a Java Solver class from the C++ Solver*
      jobject solver_object = jenv->NewObject(
          solver_class, solver_constructor, solver, /*OwnMemory=*/false);

      // Call the java Callback passing the Java Solver object.
      jenv->CallVoidMethod($input_object, $input_method_id, solver_object);
    };
  %}
  // These 4 typemaps tell SWIG which JNI and Java types to use.
  %typemap(jni) TYPE "jobject" // Type used in the JNI C.
  %typemap(jtype) TYPE "JAVA_TYPE" // Type used in the JNI.java.
  %typemap(jstype) TYPE "JAVA_TYPE" // Type used in the Proxy class.
  %typemap(javain) TYPE "$javainput" // passing the Callback to JNI java class.
%enddef

%{
#include <memory> // std::make_shared<GlobalRefGuard>
%}

DEFINE_SOLVER_TO_VOID_CALLBACK(
  std::function<void(operations_research::Solver*)>,
  Consumer<Solver>, "accept", "(Ljava/lang/Object;)V")

DEFINE_VOID_TO_STRING_CALLBACK(
  std::function<std::string()>,
  Supplier<String>, "get", "()Ljava/lang/Object;")

DEFINE_VOID_TO_R_CALLBACK(
  std::function<bool()>,
  BooleanSupplier, "getAsBoolean", "()Z",
  bool, CallBooleanMethod)

DEFINE_VOID_TO_R_CALLBACK(
  std::function<void()>,
  Runnable, "run", "()V",
  void, CallVoidMethod)

DEFINE_ARGS_TO_R_CALLBACK(
  std::function<int(int64)>,
  LongToIntFunction, "applyAsInt", "(J)I",
  int, CallIntMethod,
  VAR_ARGS(long t),
  VAR_ARGS((jlong)t))

DEFINE_ARGS_TO_R_CALLBACK(
  std::function<int64(int64)>,
  LongUnaryOperator, "applyAsLong", "(J)J",
  long, CallLongMethod, VAR_ARGS(long t), VAR_ARGS((jlong)t))

DEFINE_ARGS_TO_R_CALLBACK(
  std::function<int64(int64, int64)>,
  LongBinaryOperator, "applyAsLong", "(JJ)J",
  long, CallLongMethod,
  VAR_ARGS(long t, long u),
  VAR_ARGS((jlong)t, (jlong)u))

DEFINE_ARGS_TO_R_CALLBACK(
  std::function<int64(int64, int64, int64)>,
  LongTernaryOperator, "applyAsLong", "(JJJ)J",
  long, CallLongMethod,
  VAR_ARGS(long t, long u, long v),
  VAR_ARGS((jlong)t, (jlong)u, (jlong)v))

DEFINE_ARGS_TO_R_CALLBACK(
  std::function<int64(int, int)>,
  IntIntToLongFunction, "applyAsLong", "(II)J",
  long, CallLongMethod,
  VAR_ARGS(int t, int u),
  VAR_ARGS((jint)t, (jint)u))

DEFINE_ARGS_TO_R_CALLBACK(
  std::function<bool(int64)>,
  LongPredicate, "test", "(J)Z",
  bool, CallBooleanMethod,
  VAR_ARGS(long t),
  VAR_ARGS((jlong)t))

DEFINE_ARGS_TO_R_CALLBACK(
  std::function<bool(int64, int64, int64)>,
  LongTernaryPredicate, "test", "(JJJ)Z",
  bool, CallBooleanMethod,
  VAR_ARGS(long t, long u, long v),
  VAR_ARGS((jlong)t, (jlong)u, (jlong)v))

DEFINE_ARGS_TO_R_CALLBACK(
  std::function<void(int64)>,
  LongConsumer, "accept", "(J)V",
  void, CallVoidMethod,
  VAR_ARGS(long t),
  VAR_ARGS((jlong)t))

#undef VAR_ARGS
#undef DEFINE_SOLVER_TO_VOID_CALLBACK
#undef DEFINE_ARGS_TO_R_CALLBACK
#undef DEFINE_VOID_TO_R_CALLBACK
#undef DEFINE_VOID_TO_STRING_CALLBACK

// Renaming
namespace operations_research {

// This method causes issues with our std::vector<int64> wrapping. It's not really
// part of the public API anyway.
%ignore ToInt64Vector;

// Decision
%feature("director") Decision;
%unignore Decision;
%rename (apply) Decision::Apply;
%rename (refute) Decision::Refute;

// DecisionBuilder
%feature("director") DecisionBuilder;
%unignore DecisionBuilder;
%rename (nextWrap) DecisionBuilder::Next;

// DecisionVisitor
%feature("director") DecisionVisitor;
%unignore DecisionVisitor;
%rename (visitRankFirstInterval) DecisionVisitor::VisitRankFirstInterval;
%rename (visitRankLastInterval) DecisionVisitor::VisitRankLastInterval;
%rename (visitScheduleOrExpedite) DecisionVisitor::VisitScheduleOrExpedite;
%rename (visitScheduleOrPostpone) DecisionVisitor::VisitScheduleOrPostpone;
%rename (visitSetVariableValue) DecisionVisitor::VisitSetVariableValue;
%rename (visitSplitVariableDomain) DecisionVisitor::VisitSplitVariableDomain;
%rename (visitUnknownDecision) DecisionVisitor::VisitUnknownDecision;

// ModelVisitor
%unignore ModelVisitor;
%rename (beginVisitConstraint) ModelVisitor::BeginVisitConstraint;
%rename (beginVisitExtension) ModelVisitor::BeginVisitExtension;
%rename (beginVisitIntegerExpression) ModelVisitor::BeginVisitIntegerExpression;
%rename (beginVisitModel) ModelVisitor::BeginVisitModel;
%rename (endVisitConstraint) ModelVisitor::EndVisitConstraint;
%rename (endVisitExtension) ModelVisitor::EndVisitExtension;
%rename (endVisitIntegerExpression) ModelVisitor::EndVisitIntegerExpression;
%rename (endVisitModel) ModelVisitor::EndVisitModel;
%rename (visitIntegerArgument) ModelVisitor::VisitIntegerArgument;
%rename (visitIntegerArrayArgument) ModelVisitor::VisitIntegerArrayArgument;
%rename (visitIntegerExpressionArgument) ModelVisitor::VisitIntegerExpressionArgument;
%rename (visitIntegerMatrixArgument) ModelVisitor::VisitIntegerMatrixArgument;
%rename (visitIntegerVariableArrayArgument) ModelVisitor::VisitIntegerVariableArrayArgument;
%rename (visitIntegerVariable) ModelVisitor::VisitIntegerVariable;
%rename (visitIntervalArgument) ModelVisitor::VisitIntervalArgument;
%rename (visitIntervalArrayArgument) ModelVisitor::VisitIntervalArrayArgument;
%rename (visitIntervalVariable) ModelVisitor::VisitIntervalVariable;
%rename (visitSequenceArgument) ModelVisitor::VisitSequenceArgument;
%rename (visitSequenceArrayArgument) ModelVisitor::VisitSequenceArrayArgument;
%rename (visitSequenceVariable) ModelVisitor::VisitSequenceVariable;

// SymmetryBreaker
%feature("director") SymmetryBreaker;
%unignore SymmetryBreaker;
%rename (addIntegerVariableEqualValueClause) SymmetryBreaker::AddIntegerVariableEqualValueClause;
%rename (addIntegerVariableGreaterOrEqualValueClause) SymmetryBreaker::AddIntegerVariableGreaterOrEqualValueClause;
%rename (addIntegerVariableLessOrEqualValueClause) SymmetryBreaker::AddIntegerVariableLessOrEqualValueClause;

// ModelCache
%unignore ModelCache;
%rename (clear) ModelCache::Clear;
%rename (findExprConstantExpression) ModelCache::FindExprConstantExpression;
%rename (findExprExprConstantExpression) ModelCache::FindExprExprConstantExpression;
%rename (findExprExprConstraint) ModelCache::FindExprExprConstraint;
%rename (findExprExpression) ModelCache::FindExprExpression;
%rename (findExprExprExpression) ModelCache::FindExprExprExpression;
%rename (findVarArrayConstantArrayExpression) ModelCache::FindVarArrayConstantArrayExpression;
%rename (findVarArrayConstantExpression) ModelCache::FindVarArrayConstantExpression;
%rename (findVarArrayExpression) ModelCache::FindVarArrayExpression;
%rename (findVarConstantArrayExpression) ModelCache::FindVarConstantArrayExpression;
%rename (findVarConstantConstantConstraint) ModelCache::FindVarConstantConstantConstraint;
%rename (findVarConstantConstantExpression) ModelCache::FindVarConstantConstantExpression;
%rename (findVarConstantConstraint) ModelCache::FindVarConstantConstraint;
%rename (findVoidConstraint) ModelCache::FindVoidConstraint;
%rename (insertExprConstantExpression) ModelCache::InsertExprConstantExpression;
%rename (insertExprExprConstantExpression) ModelCache::InsertExprExprConstantExpression;
%rename (insertExprExprConstraint) ModelCache::InsertExprExprConstraint;
%rename (insertExprExpression) ModelCache::InsertExprExpression;
%rename (insertExprExprExpression) ModelCache::InsertExprExprExpression;
%rename (insertVarArrayConstantArrayExpression) ModelCache::InsertVarArrayConstantArrayExpression;
%rename (insertVarArrayConstantExpression) ModelCache::InsertVarArrayConstantExpression;
%rename (insertVarArrayExpression) ModelCache::InsertVarArrayExpression;
%rename (insertVarConstantArrayExpression) ModelCache::InsertVarConstantArrayExpression;
%rename (insertVarConstantConstantConstraint) ModelCache::InsertVarConstantConstantConstraint;
%rename (insertVarConstantConstantExpression) ModelCache::InsertVarConstantConstantExpression;
%rename (insertVarConstantConstraint) ModelCache::InsertVarConstantConstraint;
%rename (insertVoidConstraint) ModelCache::InsertVoidConstraint;

// RevPartialSequence
%unignore RevPartialSequence;
%rename (isRanked) RevPartialSequence::IsRanked;
%rename (numFirstRanked) RevPartialSequence::NumFirstRanked;
%rename (numLastRanked) RevPartialSequence::NumLastRanked;
%rename (rankFirst) RevPartialSequence::RankFirst;
%rename (rankLast) RevPartialSequence::RankLast;
%rename (size) RevPartialSequence::Size;

// UnsortedNullableRevBitset
// TODO(user): Remove from constraint_solveri.h (only use by table.cc)
%ignore UnsortedNullableRevBitset;

// Assignment
%unignore Assignment;
%rename (activate) Assignment::Activate;
%rename (activateObjective) Assignment::ActivateObjective;
%rename (activated) Assignment::Activated;
%rename (activatedObjective) Assignment::ActivatedObjective;
%rename (add) Assignment::Add;
%rename (addObjective) Assignment::AddObjective;
%rename (backwardSequence) Assignment::BackwardSequence;
%rename (clear) Assignment::Clear;
%rename (contains) Assignment::Contains;
%rename (copy) Assignment::Copy;
%rename (copyIntersection) Assignment::CopyIntersection;
%rename (deactivate) Assignment::Deactivate;
%rename (deactivateObjective) Assignment::DeactivateObjective;
%rename (durationMax) Assignment::DurationMax;
%rename (durationMin) Assignment::DurationMin;
%rename (durationValue) Assignment::DurationValue;
%rename (empty) Assignment::Empty;
%rename (endMax) Assignment::EndMax;
%rename (endMin) Assignment::EndMin;
%rename (endValue) Assignment::EndValue;
%rename (fastAdd) Assignment::FastAdd;
%rename (forwardSequence) Assignment::ForwardSequence;
%rename (hasObjective) Assignment::HasObjective;
%rename (intVarContainer) Assignment::IntVarContainer;
%rename (intervalVarContainer) Assignment::IntervalVarContainer;
%rename (load) Assignment::Load;
%rename (mutableIntVarContainer) Assignment::MutableIntVarContainer;
%rename (mutableIntervalVarContainer) Assignment::MutableIntervalVarContainer;
%rename (mutableSequenceVarContainer) Assignment::MutableSequenceVarContainer;
%rename (numIntVars) Assignment::NumIntVars;
%rename (numIntervalVars) Assignment::NumIntervalVars;
%rename (numSequenceVars) Assignment::NumSequenceVars;
%rename (objective) Assignment::Objective;
%rename (objectiveBound) Assignment::ObjectiveBound;
%rename (objectiveMax) Assignment::ObjectiveMax;
%rename (objectiveMin) Assignment::ObjectiveMin;
%rename (objectiveValue) Assignment::ObjectiveValue;
%rename (performedMax) Assignment::PerformedMax;
%rename (performedMin) Assignment::PerformedMin;
%rename (performedValue) Assignment::PerformedValue;
%rename (restore) Assignment::Restore;
%rename (save) Assignment::Save;
%rename (size) Assignment::Size;
%rename (sequenceVarContainer) Assignment::SequenceVarContainer;
%rename (setBackwardSequence) Assignment::SetBackwardSequence;
%rename (setDurationMax) Assignment::SetDurationMax;
%rename (setDurationMin) Assignment::SetDurationMin;
%rename (setDurationRange) Assignment::SetDurationRange;
%rename (setDurationValue) Assignment::SetDurationValue;
%rename (setEndMax) Assignment::SetEndMax;
%rename (setEndMin) Assignment::SetEndMin;
%rename (setEndRange) Assignment::SetEndRange;
%rename (setEndValue) Assignment::SetEndValue;
%rename (setForwardSequence) Assignment::SetForwardSequence;
%rename (setObjectiveMax) Assignment::SetObjectiveMax;
%rename (setObjectiveMin) Assignment::SetObjectiveMin;
%rename (setObjectiveRange) Assignment::SetObjectiveRange;
%rename (setObjectiveValue) Assignment::SetObjectiveValue;
%rename (setPerformedMax) Assignment::SetPerformedMax;
%rename (setPerformedMin) Assignment::SetPerformedMin;
%rename (setPerformedRange) Assignment::SetPerformedRange;
%rename (setPerformedValue) Assignment::SetPerformedValue;
%rename (setSequence) Assignment::SetSequence;
%rename (setStartMax) Assignment::SetStartMax;
%rename (setStartMin) Assignment::SetStartMin;
%rename (setStartRange) Assignment::SetStartRange;
%rename (setStartValue) Assignment::SetStartValue;
%rename (setUnperformed) Assignment::SetUnperformed;
%rename (size) Assignment::Size;
%rename (startMax) Assignment::StartMax;
%rename (startMin) Assignment::StartMin;
%rename (startValue) Assignment::StartValue;
%rename (store) Assignment::Store;
%rename (unperformed) Assignment::Unperformed;

// template AssignmentContainer<>
%ignore AssignmentContainer::MutableElementOrNull;
%ignore AssignmentContainer::ElementPtrOrNull;
%ignore AssignmentContainer::elements;
%rename (add) AssignmentContainer::Add;
%rename (addAtPosition) AssignmentContainer::AddAtPosition;
%rename (clear) AssignmentContainer::Clear;
%rename (element) AssignmentContainer::Element;
%rename (fastAdd) AssignmentContainer::FastAdd;
%rename (resize) AssignmentContainer::Resize;
%rename (empty) AssignmentContainer::Empty;
%rename (copy) AssignmentContainer::Copy;
%rename (copyIntersection) AssignmentContainer::CopyIntersection;
%rename (contains) AssignmentContainer::Contains;
%rename (mutableElement) AssignmentContainer::MutableElement;
%rename (size) AssignmentContainer::Size;
%rename (store) AssignmentContainer::Store;
%rename (restore) AssignmentContainer::Restore;

// AssignmentElement
%unignore AssignmentElement;
%rename (activate) AssignmentElement::Activate;
%rename (deactivate) AssignmentElement::Deactivate;
%rename (activated) AssignmentElement::Activated;

// IntVarElement
%unignore IntVarElement;
%ignore IntVarElement::LoadFromProto;
%ignore IntVarElement::WriteToProto;
%rename (reset) IntVarElement::Reset;
%rename (clone) IntVarElement::Clone;
%rename (copy) IntVarElement::Copy;
%rename (store) IntVarElement::Store;
%rename (restore) IntVarElement::Restore;
%rename (min) IntVarElement::Min;
%rename (setMin) IntVarElement::SetMin;
%rename (max) IntVarElement::Max;
%rename (setMax) IntVarElement::SetMax;
%rename (value) IntVarElement::Value;
%rename (setValue) IntVarElement::SetValue;
%rename (setRange) IntVarElement::SetRange;
%rename (var) IntVarElement::Var;

// IntervalVarElement
%unignore IntervalVarElement;
%ignore IntervalVarElement::LoadFromProto;
%ignore IntervalVarElement::WriteToProto;
%rename (clone) IntervalVarElement::Clone;
%rename (copy) IntervalVarElement::Copy;
%rename (durationMax) IntervalVarElement::DurationMax;
%rename (durationMin) IntervalVarElement::DurationMin;
%rename (durationValue) IntervalVarElement::DurationValue;
%rename (endMax) IntervalVarElement::EndMax;
%rename (endMin) IntervalVarElement::EndMin;
%rename (endValue) IntervalVarElement::EndValue;
%rename (performedMax) IntervalVarElement::PerformedMax;
%rename (performedMin) IntervalVarElement::PerformedMin;
%rename (performedValue) IntervalVarElement::PerformedValue;
%rename (reset) IntervalVarElement::Reset;
%rename (restore) IntervalVarElement::Restore;
%rename (setDurationMax) IntervalVarElement::SetDurationMax;
%rename (setDurationMin) IntervalVarElement::SetDurationMin;
%rename (setDurationRange) IntervalVarElement::SetDurationRange;
%rename (setDurationValue) IntervalVarElement::SetDurationValue;
%rename (setEndMax) IntervalVarElement::SetEndMax;
%rename (setEndMin) IntervalVarElement::SetEndMin;
%rename (setEndRange) IntervalVarElement::SetEndRange;
%rename (setEndValue) IntervalVarElement::SetEndValue;
%rename (setPerformedMax) IntervalVarElement::SetPerformedMax;
%rename (setPerformedMin) IntervalVarElement::SetPerformedMin;
%rename (setPerformedRange) IntervalVarElement::SetPerformedRange;
%rename (setPerformedValue) IntervalVarElement::SetPerformedValue;
%rename (setStartMax) IntervalVarElement::SetStartMax;
%rename (setStartMin) IntervalVarElement::SetStartMin;
%rename (setStartRange) IntervalVarElement::SetStartRange;
%rename (setStartValue) IntervalVarElement::SetStartValue;
%rename (startMax) IntervalVarElement::StartMax;
%rename (startMin) IntervalVarElement::StartMin;
%rename (startValue) IntervalVarElement::StartValue;
%rename (store) IntervalVarElement::Store;
%rename (var) IntervalVarElement::Var;

// SequenceVarElement
%unignore SequenceVarElement;
%ignore SequenceVarElement::LoadFromProto;
%ignore SequenceVarElement::WriteToProto;
%rename (backwardSequence) SequenceVarElement::BackwardSequence;
%rename (clone) SequenceVarElement::Clone;
%rename (copy) SequenceVarElement::Copy;
%rename (forwardSequence) SequenceVarElement::ForwardSequence;
%rename (reset) SequenceVarElement::Reset;
%rename (restore) SequenceVarElement::Restore;
%rename (setBackwardSequence) SequenceVarElement::SetBackwardSequence;
%rename (setForwardSequence) SequenceVarElement::SetForwardSequence;
%rename (setSequence) SequenceVarElement::SetSequence;
%rename (setUnperformed) SequenceVarElement::SetUnperformed;
%rename (store) SequenceVarElement::Store;
%rename (unperformed) SequenceVarElement::Unperformed;
%rename (var) SequenceVarElement::Var;

// SolutionCollector
%unignore SolutionCollector;
%rename (add) SolutionCollector::Add;
%rename (addObjective) SolutionCollector::AddObjective;
%rename (backwardSequence) SolutionCollector::BackwardSequence;
%rename (durationValue) SolutionCollector::DurationValue;
%rename (endValue) SolutionCollector::EndValue;
%rename (forwardSequence) SolutionCollector::ForwardSequence;
%rename (objectiveValue) SolutionCollector::objective_value;
%rename (performedValue) SolutionCollector::PerformedValue;
%rename (solutionCount) SolutionCollector::solution_count;
%rename (startValue) SolutionCollector::StartValue;
%rename (unperformed) SolutionCollector::Unperformed;
%rename (wallTime) SolutionCollector::wall_time;

// SolutionPool
%unignore SolutionPool;
%rename (getNextSolution) SolutionPool::GetNextSolution;
%rename (initialize) SolutionPool::Initialize;
%rename (registerNewSolution) SolutionPool::RegisterNewSolution;
%rename (syncNeeded) SolutionPool::SyncNeeded;

// Solver
%unignore Solver;
%typemap(javaimports) Solver %{
import com.google.ortools.constraintsolver.ConstraintSolverParameters;
import com.google.ortools.constraintsolver.SearchLimitParameters;

// Used to wrap DisplayCallback (std::function<std::string()>)
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/Supplier.html
import java.util.function.Supplier;
// Used to wrap std::function<bool()>
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/BooleanSupplier.html
import java.util.function.BooleanSupplier;

// Used to wrap IndexEvaluator1 (std::function<int64(int64)>)
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/LongUnaryOperator.html
import java.util.function.LongUnaryOperator;
// Used to wrap IndexEvaluator2 (std::function<int64(int64, int64)>)
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/LongBinaryOperator.html
import java.util.function.LongBinaryOperator;
// Used to wrap IndexEvaluator3 (std::function<int64(int64, int64, int64)>)
// note: Java does not provide TernaryOperator so we provide it.
import com.google.ortools.constraintsolver.LongTernaryOperator;
// Used to wrap std::function<int64(int, int)>
// note: Java does not provide it, so we provide it.
import com.google.ortools.constraintsolver.IntIntToLongFunction;

// Used to wrap IndexFilter1 (std::function<bool(int64)>)
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/LongPredicate.html
import java.util.function.LongPredicate;

// Used to wrap std::function<bool(int64, int64, int64)>
// note: Java does not provide TernaryPredicate so we provide it
import com.google.ortools.constraintsolver.LongTernaryPredicate;

// Used to wrap std::function<void(Solver*)>
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/Consumer.html
import java.util.function.Consumer;

// Used to wrap ObjectiveWatcher (std::function<void(int64)>)
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/LongConsumer.html
import java.util.function.LongConsumer;

// Used to wrap Closure (std::function<void()>)
// see https://docs.oracle.com/javase/8/docs/api/java/lang/Runnable.html
import java.lang.Runnable;
%}
// note: SWIG does not support multiple %typemap(javacode) Type, so we have to
// define all Solver tweak here (ed and not in the macro DEFINE_CALLBACK_*)
%typemap(javacode) Solver %{
  /**
   * This exceptions signal that a failure has been raised in the C++ world.
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
%ignore Solver::SearchLogParameters;
%ignore Solver::ActiveSearch;
%ignore Solver::SetSearchContext;
%ignore Solver::SearchContext;
%ignore Solver::MakeSearchLog(SearchLogParameters parameters);
%ignore Solver::MakeIntVarArray;
%ignore Solver::MakeIntervalVarArray;
%ignore Solver::MakeBoolVarArray;
%ignore Solver::MakeFixedDurationIntervalVarArray;
%ignore Solver::SetBranchSelector;
%ignore Solver::MakeApplyBranchSelector;
%ignore Solver::MakeAtMost;
%ignore Solver::demon_profiler;
%ignore Solver::set_fail_intercept;
%unignore Solver::Solver;
%rename (acceptedNeighbors) Solver::accepted_neighbors;
%rename (addBacktrackAction) Solver::AddBacktrackAction;
%rename (addCastConstraint) Solver::AddCastConstraint;
%rename (addConstraint) Solver::AddConstraint;
%rename (addLocalSearchMonitor) Solver::AddLocalSearchMonitor;
%rename (addPropagationMonitor) Solver::AddPropagationMonitor;
%rename (cache) Solver::Cache;
%rename (castExpression) Solver::CastExpression;
%rename (checkAssignment) Solver::CheckAssignment;
%rename (checkConstraint) Solver::CheckConstraint;
%rename (checkFail) Solver::CheckFail;
%rename (compose) Solver::Compose;
%rename (concatenateOperators) Solver::ConcatenateOperators;
%rename (currentlyInSolve) Solver::CurrentlyInSolve;
%rename (defaultSolverParameters) Solver::DefaultSolverParameters;
%rename (endSearch) Solver::EndSearch;
%rename (exportProfilingOverview) Solver::ExportProfilingOverview;
%rename (fail) Solver::Fail;
%rename (filteredNeighbors) Solver::filtered_neighbors;
%rename (finishCurrentSearch) Solver::FinishCurrentSearch;
%rename (getLocalSearchMonitor) Solver::GetLocalSearchMonitor;
%rename (getPropagationMonitor) Solver::GetPropagationMonitor;
%rename (getTime) Solver::GetTime;
%rename (hasName) Solver::HasName;
%rename (instrumentsDemons) Solver::InstrumentsDemons;
%rename (instrumentsVariables) Solver::InstrumentsVariables;
%rename (isLocalSearchProfilingEnabled) Solver::IsLocalSearchProfilingEnabled;
%rename (isProfilingEnabled) Solver::IsProfilingEnabled;
%rename (localSearchProfile) Solver::LocalSearchProfile;
%rename (makeAbs) Solver::MakeAbs;
%rename (makeAbsEquality) Solver::MakeAbsEquality;
%rename (makeAllDifferent) Solver::MakeAllDifferent;
%rename (makeAllDifferentExcept) Solver::MakeAllDifferentExcept;
%rename (makeAllSolutionCollector) Solver::MakeAllSolutionCollector;
%rename (makeAllowedAssignment) Solver::MakeAllowedAssignments;
%rename (makeAssignVariableValue) Solver::MakeAssignVariableValue;
%rename (makeAssignVariableValueOrFail) Solver::MakeAssignVariableValueOrFail;
%rename (makeAssignVariablesValues) Solver::MakeAssignVariablesValues;
%rename (makeAssignment) Solver::MakeAssignment;
%rename (makeAtSolutionCallback) Solver::MakeAtSolutionCallback;
%rename (makeBestValueSolutionCollector) Solver::MakeBestValueSolutionCollector;
%rename (makeBetweenCt) Solver::MakeBetweenCt;
%rename (makeBoolVar) Solver::MakeBoolVar;
%rename (makeBranchesLimit) Solver::MakeBranchesLimit;
%rename (makeCircuit) Solver::MakeCircuit;
%rename (makeClosureDemon) Solver::MakeClosureDemon;
%rename (makeConditionalExpression) Solver::MakeConditionalExpression;
%rename (makeConstantRestart) Solver::MakeConstantRestart;
%rename (makeConstraintAdder) Solver::MakeConstraintAdder;
%rename (makeConstraintInitialPropagateCallback) Solver::MakeConstraintInitialPropagateCallback;
%rename (makeConvexPiecewiseExpr) Solver::MakeConvexPiecewiseExpr;
%rename (makeCount) Solver::MakeCount;
%rename (makeCover) Solver::MakeCover;
%rename (makeCumulative) Solver::MakeCumulative;
%rename (makeCustomLimit) Solver::MakeCustomLimit;
%rename (makeDecision) Solver::MakeDecision;
%rename (makeDecisionBuilderFromAssignment) Solver::MakeDecisionBuilderFromAssignment;
%rename (makeDefaultPhase) Solver::MakeDefaultPhase;
%rename (makeDefaultSearchLimitParameters) Solver::MakeDefaultSearchLimitParameters;
%rename (makeDefaultSolutionPool) Solver::MakeDefaultSolutionPool;
%rename (makeDelayedConstraintInitialPropagateCallback) Solver::MakeDelayedConstraintInitialPropagateCallback;
%rename (makeDelayedPathCumul) Solver::MakeDelayedPathCumul;
%rename (makeDeviation) Solver::MakeDeviation;
%rename (makeDifference) Solver::MakeDifference;
%rename (makeDisjunctiveConstraint) Solver::MakeDisjunctiveConstraint;
%rename (makeDistribute) Solver::MakeDistribute;
%rename (makeDiv) Solver::MakeDiv;
%rename (makeElement) Solver::MakeElement;
%rename (makeElementEquality) Solver::MakeElementEquality;
%rename (makeEnterSearchCallback) Solver::MakeEnterSearchCallback;
%rename (makeEquality) Solver::MakeEquality;
%rename (makeExitSearchCallback) Solver::MakeExitSearchCallback;
%rename (makeFailDecision) Solver::MakeFailDecision;
%rename (makeFailuresLimit) Solver::MakeFailuresLimit;
%rename (makeFalseConstraint) Solver::MakeFalseConstraint;
%rename (makeFirstSolutionCollector) Solver::MakeFirstSolutionCollector;
%rename (makeFixedDurationEndSyncedOnEndIntervalVar) Solver::MakeFixedDurationEndSyncedOnEndIntervalVar;
%rename (makeFixedDurationEndSyncedOnStartIntervalVar) Solver::MakeFixedDurationEndSyncedOnStartIntervalVar;
%rename (makeFixedDurationIntervalVar) Solver::MakeFixedDurationIntervalVar;
%rename (makeFixedDurationStartSyncedOnEndIntervalVar) Solver::MakeFixedDurationStartSyncedOnEndIntervalVar;
%rename (makeFixedDurationStartSyncedOnStartIntervalVar) Solver::MakeFixedDurationStartSyncedOnStartIntervalVar;
%rename (makeFixedInterval) Solver::MakeFixedInterval;
%rename (makeGenericTabuSearch) Solver::MakeGenericTabuSearch;
%rename (makeGreater) Solver::MakeGreater;
%rename (makeGreaterOrEqual) Solver::MakeGreaterOrEqual;
%rename (makeGuidedLocalSearch) Solver::MakeGuidedLocalSearch;
%rename (makeIfThenElseCt) Solver::MakeIfThenElseCt;
%rename (makeIndexExpression) Solver::MakeIndexExpression;
%rename (makeIndexOfConstraint) Solver::MakeIndexOfConstraint;
%rename (makeIndexOfFirstMaxValueConstraint) Solver::MakeIndexOfFirstMaxValueConstraint;
%rename (makeIndexOfFirstMinValueConstraint) Solver::MakeIndexOfFirstMinValueConstraint;
%rename (makeIntConst) Solver::MakeIntConst;
%rename (makeIntVar) Solver::MakeIntVar;
%rename (makeIntervalRelaxedMax) Solver::MakeIntervalRelaxedMax;
%rename (makeIntervalRelaxedMin) Solver::MakeIntervalRelaxedMin;
%rename (makeIntervalVar) Solver::MakeIntervalVar;
%rename (makeIntervalVarRelation) Solver::MakeIntervalVarRelation;
%rename (makeIntervalVarRelationWithDelay) Solver::MakeIntervalVarRelationWithDelay;
%rename (makeInversePermutationConstraint) Solver::MakeInversePermutationConstraint;
%rename (makeIsBetweenCt) Solver::MakeIsBetweenCt;
%rename (makeIsBetweenVar) Solver::MakeIsBetweenVar;
%rename (makeIsDifferentCstCt) Solver::MakeIsDifferentCstCt;
%rename (makeIsDifferentCstCt) Solver::MakeIsDifferentCt;
%rename (makeIsDifferentCstVar) Solver::MakeIsDifferentCstVar;
%rename (makeIsDifferentCstVar) Solver::MakeIsDifferentVar;
%rename (makeIsEqualCstCt) Solver::MakeIsEqualCstCt;
%rename (makeIsEqualCstVar) Solver::MakeIsEqualCstVar;
%rename (makeIsEqualVar) Solver::MakeIsEqualCt;
%rename (makeIsEqualVar) Solver::MakeIsEqualVar;
%rename (makeIsGreaterCstCt) Solver::MakeIsGreaterCstCt;
%rename (makeIsGreaterCstVar) Solver::MakeIsGreaterCstVar;
%rename (makeIsGreaterCt) Solver::MakeIsGreaterCt;
%rename (makeIsGreaterOrEqualCstCt) Solver::MakeIsGreaterOrEqualCstCt;
%rename (makeIsGreaterOrEqualCstVar) Solver::MakeIsGreaterOrEqualCstVar;
%rename (makeIsGreaterOrEqualCt) Solver::MakeIsGreaterOrEqualCt;
%rename (makeIsGreaterOrEqualVar) Solver::MakeIsGreaterOrEqualVar;
%rename (makeIsGreaterVar) Solver::MakeIsGreaterVar;
%rename (makeIsLessCstCt) Solver::MakeIsLessCstCt;
%rename (makeIsLessCstVar) Solver::MakeIsLessCstVar;
%rename (makeIsLessCt) Solver::MakeIsLessCt;
%rename (makeIsLessOrEqualCstCt) Solver::MakeIsLessOrEqualCstCt;
%rename (makeIsLessOrEqualCstVar) Solver::MakeIsLessOrEqualCstVar;
%rename (makeIsLessOrEqualCt) Solver::MakeIsLessOrEqualCt;
%rename (makeIsLessOrEqualVar) Solver::MakeIsLessOrEqualVar;
%rename (makeIsLessVar) Solver::MakeIsLessVar;
%rename (makeIsMemberCt) Solver::MakeIsMemberCt;
%rename (makeIsMemberVar) Solver::MakeIsMemberVar;
%rename (makeLastSolutionCollector) Solver::MakeLastSolutionCollector;
%rename (makeLess) Solver::MakeLess;
%rename (makeLessOrEqual) Solver::MakeLessOrEqual;
%rename (makeLexicalLess) Solver::MakeLexicalLess;
%rename (makeLexicalLessOrEqual) Solver::MakeLexicalLessOrEqual;
%rename (makeLimit) Solver::MakeLimit;
%rename (makeLocalSearchPhase) Solver::MakeLocalSearchPhase;
%rename (makeLocalSearchPhaseParameters) Solver::MakeLocalSearchPhaseParameters;
%rename (makeLubyRestart) Solver::MakeLubyRestart;
%rename (makeMapDomain) Solver::MakeMapDomain;
%rename (makeMax) Solver::MakeMax;
%rename (makeMaxEquality) Solver::MakeMaxEquality;
%rename (makeMaximize) Solver::MakeMaximize;
%rename (makeMemberCt) Solver::MakeMemberCt;
%rename (makeMin) Solver::MakeMin;
%rename (makeMinEquality) Solver::MakeMinEquality;
%rename (makeMinimize) Solver::MakeMinimize;
%rename (makeMirrorInterval) Solver::MakeMirrorInterval;
%rename (makeModulo) Solver::MakeModulo;
%rename (makeMonotonicElement) Solver::MakeMonotonicElement;
%rename (makeMoveTowardTargetOperator) Solver::MakeMoveTowardTargetOperator;
%rename (makeNBestValueSolutionCollector) Solver::MakeNBestValueSolutionCollector;
%rename (makeNeighborhoodLimit) Solver::MakeNeighborhoodLimit;
%rename (makeNestedOptimize) Solver::MakeNestedOptimize;
%rename (makeNoCycle) Solver::MakeNoCycle;
%rename (makeNonEquality) Solver::MakeNonEquality;
%rename (makeNonOverlappingBoxesConstraint) Solver::MakeNonOverlappingBoxesConstraint;
%rename (makeNonOverlappingNonStrictBoxesConstraint) Solver::MakeNonOverlappingNonStrictBoxesConstraint;
%rename (makeNotBetweenCt) Solver::MakeNotBetweenCt;
%rename (makeNotMemberCt) Solver::MakeNotMemberCt;
%rename (makeNullIntersect) Solver::MakeNullIntersect;
%rename (makeNullIntersectExcept) Solver::MakeNullIntersectExcept;
%rename (makeOperator) Solver::MakeOperator;
%rename (makeOpposite) Solver::MakeOpposite;
%rename (makeOptimize) Solver::MakeOptimize;
%rename (makePack) Solver::MakePack;
%rename (makePathConnected) Solver::MakePathConnected;
%rename (makePathCumul) Solver::MakePathCumul;
%rename (makePhase) Solver::MakePhase;
%rename (makePower) Solver::MakePower;
%rename (makePrintModelVisitor) Solver::MakePrintModelVisitor;
%rename (makeProd) Solver::MakeProd;
%rename (makeRandomLnsOperator) Solver::MakeRandomLnsOperator;
%rename (makeRankFirstInterval) Solver::MakeRankFirstInterval;
%rename (makeRankLastInterval) Solver::MakeRankLastInterval;
%rename (makeRestoreAssignment) Solver::MakeRestoreAssignment;
%rename (makeScalProd) Solver::MakeScalProd;
%rename (makeScalProdEquality) Solver::MakeScalProdEquality;
%rename (makeScalProdGreaterOrEqual) Solver::MakeScalProdGreaterOrEqual;
%rename (makeScalProdLessOrEqual) Solver::MakeScalProdLessOrEqual;
%rename (makeScheduleOrExpedite) Solver::MakeScheduleOrExpedite;
%rename (makeScheduleOrPostpone) Solver::MakeScheduleOrPostpone;
%rename (makeSearchLog) Solver::MakeSearchLog;
%rename (makeSearchTrace) Solver::MakeSearchTrace;
%rename (makeSemiContinuousExpr) Solver::MakeSemiContinuousExpr;
%rename (makeSequenceVar) Solver::MakeSequenceVar;
%rename (makeSimulatedAnnealing) Solver::MakeSimulatedAnnealing;
%rename (makeSolutionsLimit) Solver::MakeSolutionsLimit;
%rename (makeSolveOnce) Solver::MakeSolveOnce;
%rename (makeSortingConstraint) Solver::MakeSortingConstraint;
%rename (makeSplitVariableDomain) Solver::MakeSplitVariableDomain;
%rename (makeSquare) Solver::MakeSquare;
%rename (makeStatisticsModelVisitor) Solver::MakeStatisticsModelVisitor;
%rename (makeStoreAssignment) Solver::MakeStoreAssignment;
%rename (makeStrictDisjunctiveConstraint) Solver::MakeStrictDisjunctiveConstraint;
%rename (makeSubCircuit) Solver::MakeSubCircuit;
%rename (makeSum) Solver::MakeSum;
%rename (makeSumEquality) Solver::MakeSumEquality;
%rename (makeSumGreaterOrEqual) Solver::MakeSumGreaterOrEqual;
%rename (makeSumLessOrEqual) Solver::MakeSumLessOrEqual;
%rename (makeSumObjectiveFilter) Solver::MakeSumObjectiveFilter;
%rename (makeSymmetryManager) Solver::MakeSymmetryManager;
%rename (makeTabuSearch) Solver::MakeTabuSearch;
%rename (makeTemporalDisjunction) Solver::MakeTemporalDisjunction;
%rename (makeTimeLimit) Solver::MakeTimeLimit;
%rename (makeTransitionConstraint) Solver::MakeTransitionConstraint;
%rename (makeTreeMonitor) Solver::MakeTreeMonitor;
%rename (makeTrueConstraint) Solver::MakeTrueConstraint;
%rename (makeVariableDomainFilter) Solver::MakeVariableDomainFilter;
%rename (makeVariableGreaterOrEqualValue) Solver::MakeVariableGreaterOrEqualValue;
%rename (makeVariableLessOrEqualValue) Solver::MakeVariableLessOrEqualValue;
%rename (makeWeightedMaximize) Solver::MakeWeightedMaximize;
%rename (makeWeightedMinimize) Solver::MakeWeightedMinimize;
%rename (makeWeightedOptimize) Solver::MakeWeightedOptimize;
%rename (memoryUsage) Solver::MemoryUsage;
%rename (nameAllVariables) Solver::NameAllVariables;
%rename (newSearch) Solver::NewSearch;
%rename (nextSolution) Solver::NextSolution;
%rename (popState) Solver::PopState;
%rename (pushState) Solver::PushState;
%rename (rand32) Solver::Rand32;
%rename (rand64) Solver::Rand64;
%rename (randomConcatenateOperators) Solver::RandomConcatenateOperators;
%rename (reSeed) Solver::ReSeed;
%rename (registerDemon) Solver::RegisterDemon;
%rename (registerIntExpr) Solver::RegisterIntExpr;
%rename (registerIntVar) Solver::RegisterIntVar;
%rename (registerIntervalVar) Solver::RegisterIntervalVar;
%rename (restartCurrentSearch) Solver::RestartCurrentSearch;
%rename (restartSearch) Solver::RestartSearch;
%rename (searchDepth) Solver::SearchDepth;
%rename (searchLeftDepth) Solver::SearchLeftDepth;
%rename (shouldFail) Solver::ShouldFail;
%rename (solve) Solver::Solve;
%rename (solveAndCommit) Solver::SolveAndCommit;
%rename (solveDepth) Solver::SolveDepth;
%rename (topPeriodicCheck) Solver::TopPeriodicCheck;
%rename (topProgressPercent) Solver::TopProgressPercent;
%rename (tryDecisions) Solver::Try;
%rename (updateLimits) Solver::UpdateLimits;
%rename (wallTime) Solver::wall_time;


// BaseIntExpr
%unignore BaseIntExpr;
%rename (castToVar) BaseIntExpr::CastToVar;

// IntExpr
%unignore IntExpr;
%rename (isVar) IntExpr::IsVar;
%rename (range) IntExpr::Range;
%rename (var) IntExpr::Var;
%rename (varWithName) IntExpr::VarWithName;
%rename (whenRange) IntExpr::WhenRange;

// IntVar
%unignore IntVar;
%rename (addName) IntVar::AddName;
%rename (contains) IntVar::Contains;
%rename (isDifferent) IntVar::IsDifferent;
%rename (isEqual) IntVar::IsEqual;
%rename (isGreaterOrEqual) IntVar::IsGreaterOrEqual;
%rename (isLessOrEqual) IntVar::IsLessOrEqual;
%rename (makeDomainIterator) IntVar::MakeDomainIterator;
%rename (makeHoleIterator) IntVar::MakeHoleIterator;
%rename (oldMax) IntVar::OldMax;
%rename (oldMin) IntVar::OldMin;
%rename (removeInterval) IntVar::RemoveInterval;
%rename (removeValue) IntVar::RemoveValue;
%rename (removeValues) IntVar::RemoveValues;
%rename (size) IntVar::Size;
%rename (varType) IntVar::VarType;
%rename (whenBound) IntVar::WhenBound;
%rename (whenDomain) IntVar::WhenDomain;

// IntVarIterator
%unignore IntVarIterator;
%rename (init) IntVarIterator::Init;
%rename (next) IntVarIterator::Next;
%rename (ok) IntVarIterator::Ok;

// BooleanVar
%unignore BooleanVar;
%rename (baseName) BooleanVar::BaseName;
%rename (isDifferent) BooleanVar::IsDifferent;
%rename (isEqual) BooleanVar::IsEqual;
%rename (isGreaterOrEqual) BooleanVar::IsGreaterOrEqual;
%rename (isLessOrEqual) BooleanVar::IsLessOrEqual;
%rename (makeDomainIterator) BooleanVar::MakeDomainIterator;
%rename (makeHoleIterator) BooleanVar::MakeHoleIterator;
%rename (rawValue) BooleanVar::RawValue;
%rename (restoreValue) BooleanVar::RestoreValue;
%rename (size) BooleanVar::Size;
%rename (varType) BooleanVar::VarType;
%rename (whenBound) BooleanVar::WhenBound;
%rename (whenDomain) BooleanVar::WhenDomain;
%rename (whenRange) BooleanVar::WhenRange;

// IntervalVar
%unignore IntervalVar;
%rename (cannotBePerformed) IntervalVar::CannotBePerformed;
%rename (durationExpr) IntervalVar::DurationExpr;
%rename (durationMax) IntervalVar::DurationMax;
%rename (durationMin) IntervalVar::DurationMin;
%rename (endExpr) IntervalVar::EndExpr;
%rename (endMax) IntervalVar::EndMax;
%rename (endMin) IntervalVar::EndMin;
%rename (isPerformedBound) IntervalVar::IsPerformedBound;
%rename (mayBePerformed) IntervalVar::MayBePerformed;
%rename (mustBePerformed) IntervalVar::MustBePerformed;
%rename (oldDurationMax) IntervalVar::OldDurationMax;
%rename (oldDurationMin) IntervalVar::OldDurationMin;
%rename (oldEndMax) IntervalVar::OldEndMax;
%rename (oldEndMin) IntervalVar::OldEndMin;
%rename (oldStartMax) IntervalVar::OldStartMax;
%rename (oldStartMin) IntervalVar::OldStartMin;
%rename (performedExpr) IntervalVar::PerformedExpr;
%rename (safeDurationExpr) IntervalVar::SafeDurationExpr;
%rename (safeEndExpr) IntervalVar::SafeEndExpr;
%rename (safeStartExpr) IntervalVar::SafeStartExpr;
%rename (setDurationMax) IntervalVar::SetDurationMax;
%rename (setDurationMin) IntervalVar::SetDurationMin;
%rename (setDurationRange) IntervalVar::SetDurationRange;
%rename (setEndMax) IntervalVar::SetEndMax;
%rename (setEndMin) IntervalVar::SetEndMin;
%rename (setEndRange) IntervalVar::SetEndRange;
%rename (setPerformed) IntervalVar::SetPerformed;
%rename (setStartMax) IntervalVar::SetStartMax;
%rename (setStartMin) IntervalVar::SetStartMin;
%rename (setStartRange) IntervalVar::SetStartRange;
%rename (startExpr) IntervalVar::StartExpr;
%rename (startMax) IntervalVar::StartMax;
%rename (startMin) IntervalVar::StartMin;
%rename (wasPerformedBound) IntervalVar::WasPerformedBound;
%rename (whenAnything) IntervalVar::WhenAnything;
%rename (whenDurationBound) IntervalVar::WhenDurationBound;
%rename (whenDurationRange) IntervalVar::WhenDurationRange;
%rename (whenEndBound) IntervalVar::WhenEndBound;
%rename (whenEndRange) IntervalVar::WhenEndRange;
%rename (whenPerformedBound) IntervalVar::WhenPerformedBound;
%rename (whenStartBound) IntervalVar::WhenStartBound;
%rename (whenStartRange) IntervalVar::WhenStartRange;

// OptimizeVar
%unignore OptimizeVar;
%rename (applyBound) OptimizeVar::ApplyBound;
%rename (print) OptimizeVar::Print;
%rename (var) OptimizeVar::Var;

// SequenceVar
%unignore SequenceVar;
%ignore SequenceVar::ComputePossibleFirstsAndLasts;
%ignore SequenceVar::FillSequence;
%rename (rankFirst) SequenceVar::RankFirst;
%rename (rankLast) SequenceVar::RankLast;
%rename (rankNotFirst) SequenceVar::RankNotFirst;
%rename (rankNotLast) SequenceVar::RankNotLast;
%rename (rankSequence) SequenceVar::RankSequence;
%rename (interval) SequenceVar::Interval;
%rename (next) SequenceVar::Next;

// Constraint
%unignore Constraint;
%rename (initialPropagate) Constraint::InitialPropagate;
%rename (isCastConstraint) Constraint::IsCastConstraint;
%rename (postAndPropagate) Constraint::PostAndPropagate;
%rename (post) Constraint::Post;
%rename (var) Constraint::Var;

// DisjunctiveConstraint
%unignore DisjunctiveConstraint;
%typemap(javaimports) DisjunctiveConstraint %{
// Used to wrap IndexEvaluator2
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/LongBinaryOperator.html
import java.util.function.LongBinaryOperator;
%}
%rename (makeSequenceVar) DisjunctiveConstraint::MakeSequenceVar;
%rename (setTransitionTime) DisjunctiveConstraint::SetTransitionTime;
%rename (transitionTime) DisjunctiveConstraint::TransitionTime;

// Pack
%unignore Pack;
%typemap(javaimports) Pack %{
// Used to wrap IndexEvaluator1
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/LongUnaryOperator.html
import java.util.function.LongUnaryOperator;
// Used to wrap IndexEvaluator2
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/LongBinaryOperator.html
import java.util.function.LongBinaryOperator;
%}
%rename (addCountAssignedItemsDimension) Pack::AddCountAssignedItemsDimension;
%rename (addCountUsedBinDimension) Pack::AddCountUsedBinDimension;
%rename (addSumVariableWeightsLessOrEqualConstantDimension) Pack::AddSumVariableWeightsLessOrEqualConstantDimension;
%rename (addWeightedSumEqualVarDimension) Pack::AddWeightedSumEqualVarDimension;
%rename (addWeightedSumLessOrEqualConstantDimension) Pack::AddWeightedSumLessOrEqualConstantDimension;
%rename (addWeightedSumOfAssignedDimension) Pack::AddWeightedSumOfAssignedDimension;
%rename (assignAllPossibleToBin) Pack::AssignAllPossibleToBin;
%rename (assignAllRemainingItems) Pack::AssignAllRemainingItems;
%rename (assignFirstPossibleToBin) Pack::AssignFirstPossibleToBin;
%rename (assign) Pack::Assign;
%rename (assignVar) Pack::AssignVar;
%rename (clearAll) Pack::ClearAll;
%rename (isAssignedStatusKnown) Pack::IsAssignedStatusKnown;
%rename (isPossible) Pack::IsPossible;
%rename (isUndecided) Pack::IsUndecided;
%rename (oneDomain) Pack::OneDomain;
%rename (propagateDelayed) Pack::PropagateDelayed;
%rename (propagate) Pack::Propagate;
%rename (removeAllPossibleFromBin) Pack::RemoveAllPossibleFromBin;
%rename (setAssigned) Pack::SetAssigned;
%rename (setImpossible) Pack::SetImpossible;
%rename (setUnassigned) Pack::SetUnassigned;
%rename (unassignAllRemainingItems) Pack::UnassignAllRemainingItems;

// PropagationBaseObject
%unignore PropagationBaseObject;
%ignore PropagationBaseObject::ExecuteAll;
%ignore PropagationBaseObject::EnqueueAll;
%ignore PropagationBaseObject::set_action_on_fail;
%rename (baseName) PropagationBaseObject::BaseName;
%rename (enqueueDelayedDemon) PropagationBaseObject::EnqueueDelayedDemon;
%rename (enqueueVar) PropagationBaseObject::EnqueueVar;
%rename (freezeQueue) PropagationBaseObject::FreezeQueue;
%rename (hasName) PropagationBaseObject::HasName;
%rename (setName) PropagationBaseObject::set_name;
%rename (unfreezeQueue) PropagationBaseObject::UnfreezeQueue;

// SearchMonitor
%feature("director") SearchMonitor;
%unignore SearchMonitor;
%rename (acceptDelta) SearchMonitor::AcceptDelta;
%rename (acceptNeighbor) SearchMonitor::AcceptNeighbor;
%rename (acceptSolution) SearchMonitor::AcceptSolution;
%rename (afterDecision) SearchMonitor::AfterDecision;
%rename (applyDecision) SearchMonitor::ApplyDecision;
%rename (atSolution) SearchMonitor::AtSolution;
%rename (beginFail) SearchMonitor::BeginFail;
%rename (beginInitialPropagation) SearchMonitor::BeginInitialPropagation;
%rename (beginNextDecision) SearchMonitor::BeginNextDecision;
%rename (endFail) SearchMonitor::EndFail;
%rename (endInitialPropagation) SearchMonitor::EndInitialPropagation;
%rename (endNextDecision) SearchMonitor::EndNextDecision;
%rename (enterSearch) SearchMonitor::EnterSearch;
%rename (exitSearch) SearchMonitor::ExitSearch;
%rename (finishCurrentSearch) SearchMonitor::FinishCurrentSearch;
%rename (install) SearchMonitor::Install;
%rename (localOptimum) SearchMonitor::LocalOptimum;
%rename (noMoreSolutions) SearchMonitor::NoMoreSolutions;
%rename (periodicCheck) SearchMonitor::PeriodicCheck;
%rename (progressPercent) SearchMonitor::ProgressPercent;
%rename (refuteDecision) SearchMonitor::RefuteDecision;
%rename (restartCurrentSearch) SearchMonitor::RestartCurrentSearch;
%rename (restartSearch) SearchMonitor::RestartSearch;

// SearchLimit
%unignore SearchLimit;
%rename (check) SearchLimit::Check;
%rename (copy) SearchLimit::Copy;
%rename (init) SearchLimit::Init;
%rename (makeClone) SearchLimit::MakeClone;

// SearchLog
%unignore SearchLog;
%typemap(javaimports) SearchLog %{
// Used to wrap DisplayCallback (std::function<std::string()>)
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/Supplier.html
import java.util.function.Supplier;
%}
%rename (maintain) SearchLog::Maintain;
%rename (outputDecision) SearchLog::OutputDecision;

// LocalSearchMonitor
%unignore LocalSearchMonitor;
%rename (beginAcceptNeighbor) LocalSearchMonitor::BeginAcceptNeighbor;
%rename (beginFiltering) LocalSearchMonitor::BeginFiltering;
%rename (beginFilterNeighbor) LocalSearchMonitor::BeginFilterNeighbor;
%rename (beginMakeNextNeighbor) LocalSearchMonitor::BeginMakeNextNeighbor;
%rename (beginOperatorStart) LocalSearchMonitor::BeginOperatorStart;
%rename (endAcceptNeighbor) LocalSearchMonitor::EndAcceptNeighbor;
%rename (endFiltering) LocalSearchMonitor::EndFiltering;
%rename (endFilterNeighbor) LocalSearchMonitor::EndFilterNeighbor;
%rename (endMakeNextNeighbor) LocalSearchMonitor::EndMakeNextNeighbor;
%rename (endOperatorStart) LocalSearchMonitor::EndOperatorStart;

// PropagationMonitor
%unignore PropagationMonitor;
%rename (beginConstraintInitialPropagation) PropagationMonitor::BeginConstraintInitialPropagation;
%rename (beginDemonRun) PropagationMonitor::BeginDemonRun;
%rename (beginNestedConstraintInitialPropagation) PropagationMonitor::BeginNestedConstraintInitialPropagation;
%rename (endConstraintInitialPropagation) PropagationMonitor::EndConstraintInitialPropagation;
%rename (endDemonRun) PropagationMonitor::EndDemonRun;
%rename (endNestedConstraintInitialPropagation) PropagationMonitor::EndNestedConstraintInitialPropagation;
%rename (endProcessingIntegerVariable) PropagationMonitor::EndProcessingIntegerVariable;
%rename (install) PropagationMonitor::Install;
%rename (popContext) PropagationMonitor::PopContext;
%rename (pushContext) PropagationMonitor::PushContext;
%rename (rankFirst) PropagationMonitor::RankFirst;
%rename (rankLast) PropagationMonitor::RankLast;
%rename (rankNotFirst) PropagationMonitor::RankNotFirst;
%rename (rankNotLast) PropagationMonitor::RankNotLast;
%rename (rankSequence) PropagationMonitor::RankSequence;
%rename (registerDemon) PropagationMonitor::RegisterDemon;
%rename (removeInterval) PropagationMonitor::RemoveInterval;
%rename (removeValue) PropagationMonitor::RemoveValue;
%rename (removeValues) PropagationMonitor::RemoveValues;
%rename (setDurationMax) PropagationMonitor::SetDurationMax;
%rename (setDurationMin) PropagationMonitor::SetDurationMin;
%rename (setDurationRange) PropagationMonitor::SetDurationRange;
%rename (setEndMax) PropagationMonitor::SetEndMax;
%rename (setEndMin) PropagationMonitor::SetEndMin;
%rename (setEndRange) PropagationMonitor::SetEndRange;
%rename (setPerformed) PropagationMonitor::SetPerformed;
%rename (setStartMax) PropagationMonitor::SetStartMax;
%rename (setStartMin) PropagationMonitor::SetStartMin;
%rename (setStartRange) PropagationMonitor::SetStartRange;
%rename (startProcessingIntegerVariable) PropagationMonitor::StartProcessingIntegerVariable;

// IntVarLocalSearchHandler
%unignore IntVarLocalSearchHandler;
%rename (addToAssignment) IntVarLocalSearchHandler::AddToAssignment;
%rename (onAddVars) IntVarLocalSearchHandler::OnAddVars;
%rename (onRevertChanges) IntVarLocalSearchHandler::OnRevertChanges;
%rename (valueFromAssignent) IntVarLocalSearchHandler::ValueFromAssignent;

// SequenceVarLocalSearchHandler
%unignore SequenceVarLocalSearchHandler;
%rename (addToAssignment) SequenceVarLocalSearchHandler::AddToAssignment;
%rename (onAddVars) SequenceVarLocalSearchHandler::OnAddVars;
%rename (onRevertChanges) SequenceVarLocalSearchHandler::OnRevertChanges;
%rename (valueFromAssignent) SequenceVarLocalSearchHandler::ValueFromAssignent;

// LocalSearchOperator
%feature("director") LocalSearchOperator;
%unignore LocalSearchOperator;
%rename (nextNeighbor) LocalSearchOperator::MakeNextNeighbor;
%rename (reset) LocalSearchOperator::Reset;
%rename (start) LocalSearchOperator::Start;

// VarLocalSearchOperator<>
%unignore VarLocalSearchOperator;
%ignore VarLocalSearchOperator::Start;
%ignore VarLocalSearchOperator::ApplyChanges;
%ignore VarLocalSearchOperator::RevertChanges;
%ignore VarLocalSearchOperator::SkipUnchanged;
%rename (size) VarLocalSearchOperator::Size;
%rename (value) VarLocalSearchOperator::Value;
%rename (isIncremental) VarLocalSearchOperator::IsIncremental;
%rename (onStart) VarLocalSearchOperator::OnStart;
%rename (oldValue) VarLocalSearchOperator::OldValue;
%rename (setValue) VarLocalSearchOperator::SetValue;
%rename (var) VarLocalSearchOperator::Var;
%rename (activated) VarLocalSearchOperator::Activated;
%rename (activate) VarLocalSearchOperator::Activate;
%rename (deactivate) VarLocalSearchOperator::Deactivate;
%rename (addVars) VarLocalSearchOperator::AddVars;

// IntVarLocalSearchOperator
%feature("director") IntVarLocalSearchOperator;
%unignore IntVarLocalSearchOperator;
%ignore IntVarLocalSearchOperator::MakeNextNeighbor;
%rename (size) IntVarLocalSearchOperator::Size;
%rename (oneNeighbor) IntVarLocalSearchOperator::MakeOneNeighbor;
%rename (value) IntVarLocalSearchOperator::Value;
%rename (isIncremental) IntVarLocalSearchOperator::IsIncremental;
%rename (onStart) IntVarLocalSearchOperator::OnStart;
%rename (oldValue) IntVarLocalSearchOperator::OldValue;
%rename (setValue) IntVarLocalSearchOperator::SetValue;
%rename (var) IntVarLocalSearchOperator::Var;
%rename (activated) IntVarLocalSearchOperator::Activated;
%rename (activate) IntVarLocalSearchOperator::Activate;
%rename (deactivate) IntVarLocalSearchOperator::Deactivate;
%rename (addVars) IntVarLocalSearchOperator::AddVars;

// BaseLns
%feature("director") BaseLns;
%unignore BaseLns;
%rename (initFragments) BaseLns::InitFragments;
%rename (nextFragment) BaseLns::NextFragment;
%feature ("nodirector") BaseLns::OnStart;
%feature ("nodirector") BaseLns::SkipUnchanged;
%feature ("nodirector") BaseLns::MakeOneNeighbor;
%rename (isIncremental) BaseLns::IsIncremental;
%rename (appendToFragment) BaseLns::AppendToFragment;
%rename(fragmentSize) BaseLns::FragmentSize;

// ChangeValue
%feature("director") ChangeValue;
%unignore ChangeValue;
%rename (modifyValue) ChangeValue::ModifyValue;

// SequenceVarLocalSearchOperator
%feature("director") SequenceVarLocalSearchOperator;
%unignore SequenceVarLocalSearchOperator;
%ignore SequenceVarLocalSearchOperator::OldSequence;
%ignore SequenceVarLocalSearchOperator::Sequence;
%ignore SequenceVarLocalSearchOperator::SetBackwardSequence;
%ignore SequenceVarLocalSearchOperator::SetForwardSequence;
%rename (start) SequenceVarLocalSearchOperator::Start;

// PathOperator
%feature("director") PathOperator;
%unignore PathOperator;
%typemap(javaimports) PathOperator %{
// Used to wrap start_empty_path_class see:
// https://docs.oracle.com/javase/8/docs/api/java/util/function/LongToIntFunction.html
import java.util.function.LongToIntFunction;
%}
%ignore PathOperator::Next;
%ignore PathOperator::Path;
%ignore PathOperator::SkipUnchanged;
%ignore PathOperator::number_of_nexts;
%rename (getBaseNodeRestartPosition) PathOperator::GetBaseNodeRestartPosition;
%rename (initPosition) PathOperator::InitPosition;
%rename (neighbor) PathOperator::MakeNeighbor;
%rename (onSamePathAsPreviousBase) PathOperator::OnSamePathAsPreviousBase;
%rename (restartAtPathStartOnSynchronize) PathOperator::RestartAtPathStartOnSynchronize;
%rename (setNextBaseToIncrement) PathOperator::SetNextBaseToIncrement;

// PathWithPreviousNodesOperator
%unignore PathWithPreviousNodesOperator;
%rename (isPathStart) PathWithPreviousNodesOperator::IsPathStart;
%rename (prev) PathWithPreviousNodesOperator::Prev;

// LocalSearchFilter
%feature("director") LocalSearchFilter;
%unignore LocalSearchFilter;
%rename (accept) LocalSearchFilter::Accept;
%rename (getAcceptedObjectiveValue) LocalSearchFilter::GetAcceptedObjectiveValue;
%rename (getSynchronizedObjectiveValue) LocalSearchFilter::GetSynchronizedObjectiveValue;
%rename (isIncremental) LocalSearchFilter::IsIncremental;
%rename (synchronize) LocalSearchFilter::Synchronize;

// IntVarLocalSearchFilter
%feature("director") IntVarLocalSearchFilter;
%unignore IntVarLocalSearchFilter;
%typemap(javaimports) IntVarLocalSearchFilter %{
// Used to wrap ObjectiveWatcher
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/LongConsumer.html
import java.util.function.LongConsumer;
%}
%ignore IntVarLocalSearchFilter::FindIndex;
%ignore IntVarLocalSearchFilter::IsVarSynced;
%feature("nodirector") IntVarLocalSearchFilter::Synchronize;  // Inherited.
%rename (addVars) IntVarLocalSearchFilter::AddVars;  // Inherited.
%rename (injectObjectiveValue) IntVarLocalSearchFilter::InjectObjectiveValue;
%rename (isIncremental) IntVarLocalSearchFilter::IsIncremental;
%rename (onSynchronize) IntVarLocalSearchFilter::OnSynchronize;
%rename (setObjectiveWatcher) IntVarLocalSearchFilter::SetObjectiveWatcher;
%rename (size) IntVarLocalSearchFilter::Size;
%rename (start) IntVarLocalSearchFilter::Start;
%rename (value) IntVarLocalSearchFilter::Value;
%rename (var) IntVarLocalSearchFilter::Var;  // Inherited.
%extend IntVarLocalSearchFilter {
  int index(IntVar* const var) {
    int64 index = -1;
    $self->FindIndex(var, &index);
    return index;
  }
}

// Demon
%unignore Demon;
%rename (run) Demon::Run;

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

#undef CONVERT_VECTOR

}  // namespace operations_research

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

// Add needed import to mainJNI.java
%pragma(java) jniclassimports=%{
// Used to wrap std::function<std::string()>
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/Supplier.html
import java.util.function.Supplier;

// Used to wrap std::function<bool()>
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/BooleanSupplier.html
import java.util.function.BooleanSupplier;

// Used to wrap std::function<int(int64)>
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/LongToIntFunction.html
import java.util.function.LongToIntFunction;

// Used to wrap std::function<int64(int64)>
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/LongUnaryOperator.html
import java.util.function.LongUnaryOperator;

// Used to wrap std::function<int64(int64, int64)>
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/LongBinaryOperator.html
import java.util.function.LongBinaryOperator;

// Used to wrap std::function<int64(int64, int64, int64)>
// note: Java does not provide TernaryOperator so we provide it
import com.google.ortools.constraintsolver.LongTernaryOperator;

// Used to wrap std::function<int64(int, int)>
// note: Java does not provide it, so we provide it.
import com.google.ortools.constraintsolver.IntIntToLongFunction;

// Used to wrap std::function<bool(int64)>
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/LongPredicate.html
import java.util.function.LongPredicate;

// Used to wrap std::function<bool(int64, int64, int64)>
// note: Java does not provide TernaryPredicate so we provide it
import com.google.ortools.constraintsolver.LongTernaryPredicate;

// Used to wrap std::function<void(Solver*)>
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/Consumer.html
import java.util.function.Consumer;

// Used to wrap std::function<void(int64)>
// see https://docs.oracle.com/javase/8/docs/api/java/util/function/LongConsumer.html
import java.util.function.LongConsumer;

// Used to wrap std::function<void()>
// see https://docs.oracle.com/javase/8/docs/api/java/lang/Runnable.html
import java.lang.Runnable;
%}

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

namespace operations_research {

// Globals
// IMPORTANT(corentinl): Globals will be placed in main.java
// i.e. use `import com.[...].constraintsolver.main`
%ignore FillValues;
%rename (areAllBooleans) AreAllBooleans;
%rename (areAllBound) AreAllBound;
%rename (areAllBoundTo) AreAllBoundTo;
%rename (maxVarArray) MaxVarArray;
%rename (minVarArray) MinVarArray;
%rename (posIntDivDown) PosIntDivDown;
%rename (posIntDivUp) PosIntDivUp;
%rename (setAssignmentFromAssignment) SetAssignmentFromAssignment;
%rename (zero) Zero;
}  // namespace operations_research

// Wrap cp includes
// TODO(user): Use ignoreall/unignoreall for this one. A lot of work.
//swiglint: disable include-h-allglobals
%include "ortools/constraint_solver/constraint_solver.h"
%include "ortools/constraint_solver/constraint_solveri.h"
%include "ortools/constraint_solver/java/javawrapcp_util.h"

// Define templates instantiation after wrapping.
namespace operations_research {
%template(RevInteger) Rev<int>;
%template(RevLong) Rev<int64>;
%template(RevBool) Rev<bool>;
%template(AssignmentIntContainer) AssignmentContainer<IntVar, IntVarElement>;
%template(AssignmentIntervalContainer) AssignmentContainer<IntervalVar, IntervalVarElement>;
%template(AssignmentSequenceContainer) AssignmentContainer<SequenceVar, SequenceVarElement>;
}  // namespace operations_research
