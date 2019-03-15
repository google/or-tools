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

%typemap(csimports) SWIGTYPE %{
using System;
using System.Runtime.InteropServices;
using System.Collections;
using System.Collections.Generic;
%}

%include "enumsimple.swg"
%include "stdint.i"
%include "exception.i"
%include "std_vector.i"
%include "std_common.i"
%include "std_string.i"

%include "ortools/base/base.i"
%include "ortools/util/csharp/tuple_set.i"
%include "ortools/util/csharp/vector.i"
%include "ortools/util/csharp/proto.i"

// We need to forward-declare the proto here, so that PROTO_INPUT involving it
// works correctly. The order matters very much: this declaration needs to be
// before the %{ #include ".../constraint_solver.h" %}.
namespace operations_research {
class ConstraintSolverParameters;
class SearchLimitParameters;
}  // namespace operations_research

%module(directors="1") operations_research;
#pragma SWIG nowarn=473

%{
#include <setjmp.h>

#include <string>
#include <vector>
#include <functional>

#include "ortools/base/integral_types.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/search_limit.pb.h"
#include "ortools/constraint_solver/solver_parameters.pb.h"

namespace operations_research {
class LocalSearchPhaseParameters {
 public:
  LocalSearchPhaseParameters() {}
  ~LocalSearchPhaseParameters() {}
};
}  // namespace operations_research

struct FailureProtect {
  jmp_buf exception_buffer;
  void JumpBack() {
    longjmp(exception_buffer, 1);
  }
};
%}

/* allow partial c# classes */
%typemap(csclassmodifiers) SWIGTYPE "public partial class"

// TODO(user): Try to allow this per class (difficult with CpIntVector).

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
    SWIG_CSharpSetPendingException(SWIG_CSharpApplicationException, "fail");
    return $null;
  }
}
%enddef  // PROTECT_FROM_FAILURE

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

%apply int64 * INOUT { int64 *const marker };
%apply int64 * OUTPUT { int64 *l, int64 *u, int64 *value };

// Since knapsack_solver.i and constraint_solver.i both need to
// instantiate the vector template, but their csharp_wrap.cc
// files end up being compiled into the same .dll, we must name the
// vector template differently.

%template(CpIntVector) std::vector<int>;
// IMPORTANT(corentinl) this template for vec<vec<T>> must be call BEFORE
// we redefine typemap of vec<T> in VECTOR_AS_CSHARP_ARRAY
%template(CpIntVectorVector) std::vector<std::vector<int> >;
VECTOR_AS_CSHARP_ARRAY(int, int, int, CpIntVector);
MATRIX_AS_CSHARP_ARRAY(int, int, int, CpIntVectorVector);

%template(CpInt64Vector) std::vector<int64>;
// IMPORTANT(corentinl) this template for vec<vec<T>> must be call BEFORE
// we redefine typemap of vec<T> in VECTOR_AS_CSHARP_ARRAY
%template(CpInt64VectorVector) std::vector<std::vector<int64> >;
VECTOR_AS_CSHARP_ARRAY(int64, int64, long, CpInt64Vector);
MATRIX_AS_CSHARP_ARRAY(int64, int64, long, CpInt64VectorVector);

// Types in Proxy class:
// Solver.cs:
// Solver::Foo($cstype $csinput, ...) {Solver_Foo_SWIG($csin, ...);}
// constraint_solverPINVOKE.cs:
// $csout Solver_Foo_SWIG($imtype $input, ...) {...}
// constraint_solver_csharp_wrap.cc:
// $out CSharp_Solver_Foo__($ctype $input, ...) {...; $in; PInvoke($1...); return $out;}
%define DEFINE_ARGS_TO_R_CALLBACK(
  TYPE, DELEGATE,
  LAMBDA_RETURN, CAST_DELEGATE, LAMBDA_PARAM, LAMBDA_CALL)
%typemap(cstype, out="IntPtr") TYPE %{ DELEGATE %}
%typemap(csin) TYPE %{ Store ## DELEGATE ## ($csinput) %}
%typemap(imtype, out="IntPtr") TYPE %{ DELEGATE  %}
// Type use in module_csharp_wrap.h function declaration.
// since SWIG generate code as: `$ctype argX` we can't use a C function pointer type.
%typemap(ctype) TYPE %{ void * %}
// Convert in module_csharp_wrap.cc input argument
// (delegate marshaled in C function pointer) to original std::function<...>
%typemap(in) TYPE  %{
  $1 = [$input]LAMBDA_PARAM -> LAMBDA_RETURN {
    return (CAST_DELEGATE$input)LAMBDA_CALL;
    };
%}
%enddef

%define DEFINE_VOID_TO_STRING_CALLBACK(TYPE, DELEGATE)
%typemap(cstype, out="IntPtr") TYPE %{ DELEGATE %}
%typemap(csin) TYPE %{ Store ## DELEGATE ## ($csinput) %}
%typemap(imtype, out="IntPtr") TYPE %{ DELEGATE  %}
// Type use in module_csharp_wrap.h function declaration.
// since SWIG generate code as: `$ctype argX` we can't use a C function pointer type.
%typemap(ctype) TYPE %{ void * %}
// Convert in module_csharp_wrap.cc input argument
// (delegate marshaled in C function pointer) to original std::function<...>
%typemap(in) TYPE  %{
  $1 = [$input]() -> std::string {
    std::string result;
    return result.assign((*(char* (*)()) $input)());
  };
%}
%enddef

DEFINE_ARGS_TO_R_CALLBACK(
  std::function<void(operations_research::Solver*)>, SolverToVoid,
  void, *(void(*)(operations_research::Solver*)), (operations_research::Solver* s), (s))

DEFINE_VOID_TO_STRING_CALLBACK(
  std::function<std::string()>, VoidToString)

DEFINE_ARGS_TO_R_CALLBACK(
  std::function<bool()>, VoidToBoolean,
  bool, *(bool(*)()), (), ())

DEFINE_ARGS_TO_R_CALLBACK(
  std::function<void()>, VoidToVoid,
  void, *(void(*)()), (), ())

DEFINE_ARGS_TO_R_CALLBACK(
  std::function<int(int64)>, LongToInt,
  int, *(int(*)(int64)), (int64 t), (t))

DEFINE_ARGS_TO_R_CALLBACK(
  std::function<int64(int64)>, LongToLong,
  int64, *(int64(*)(int64)), (int64 t), (t))
DEFINE_ARGS_TO_R_CALLBACK(
  std::function<int64(int64, int64)>, LongLongToLong,
  int64, *(int64(*)(int64, int64)), (int64 t, int64 u), (t, u))
DEFINE_ARGS_TO_R_CALLBACK(
  std::function<int64(int64, int64, int64)>, LongLongLongToLong,
  int64, *(int64(*)(int64, int64, int64)), (int64 t, int64 u, int64 v), (t, u, v))

DEFINE_ARGS_TO_R_CALLBACK(
  std::function<int64(int, int)>, IntIntToLong,
  int64, *(int64(*)(int, int)), (int t, int u), (t, u))

DEFINE_ARGS_TO_R_CALLBACK(
  std::function<bool(int64)>, LongToBoolean,
  bool, *(bool(*)(int64)), (int64 t), (t))

DEFINE_ARGS_TO_R_CALLBACK(
  std::function<bool(int64, int64, int64)>, LongLongLongToBoolean,
  bool, *(bool(*)(int64, int64, int64)), (int64 t, int64 u, int64 v), (t, u, v))

DEFINE_ARGS_TO_R_CALLBACK(
  std::function<void(int64)>, LongToVoid,
  void, *(void(*)(int64)), (int64 t), (t))

#undef DEFINE_ARGS_TO_R_CALLBACK
#undef DEFINE_VOID_TO_STRING_CALLBACK

// Renaming
namespace operations_research {

// Decision
%feature("director") Decision;
%unignore Decision;
// Methods:
%rename (ApplyWrapper) Decision::Apply;
%rename (RefuteWrapper) Decision::Refute;

// DecisionBuilder
%feature("director") DecisionBuilder;
%unignore DecisionBuilder;
// Methods:
%rename (NextWrapper) DecisionBuilder::Next;

// SymmetryBreaker
%feature("director") SymmetryBreaker;
%unignore SymmetryBreaker;

// UnsortedNullableRevBitset
// TODO(corentinl) To removed from constraint_solveri.h (only use by table.cc)
%ignore UnsortedNullableRevBitset;

// Assignment
%unignore Assignment;
// Ignored:
%ignore Assignment::Load;
%ignore Assignment::Save;

// template AssignmentContainer<>
// Ignored:
%ignore AssignmentContainer::MutableElement;
%ignore AssignmentContainer::MutableElementOrNull;
%ignore AssignmentContainer::ElementPtrOrNull;
%ignore AssignmentContainer::elements;

// AssignmentElement
%unignore AssignmentElement;
// Methods:
%unignore AssignmentElement::Activate;
%unignore AssignmentElement::Deactivate;
%unignore AssignmentElement::Activated;

// IntVarElement
%unignore IntVarElement;
// Ignored:
%ignore IntVarElement::LoadFromProto;
%ignore IntVarElement::WriteToProto;

// IntervalVarElement
%unignore IntervalVarElement;
// Ignored:
%ignore IntervalVarElement::LoadFromProto;
%ignore IntervalVarElement::WriteToProto;

// SequenceVarElement
%unignore SequenceVarElement;
// Ignored:
%ignore SequenceVarElement::LoadFromProto;
%ignore SequenceVarElement::WriteToProto;

// SolutionCollector
%feature("director") SolutionCollector;
%unignore SolutionCollector;

// Solver
%unignore Solver;
%typemap(cscode) Solver %{
  // Store list of delegates to avoid the GC to reclaim them.
  // This avoid the GC to collect any callback (i.e. delegate) set from C#.
  // The underlying C++ class will only store a pointer to it (i.e. no ownership).
  private List<VoidToString> displayCallbacks;
  private VoidToString StoreVoidToString(VoidToString c) {
    if (displayCallbacks == null)
      displayCallbacks = new List<VoidToString>();
    displayCallbacks.Add(c);
    return c;
  }

  private List<LongToLong> LongToLongCallbacks;
  private LongToLong StoreLongToLong(LongToLong c) {
    if (LongToLongCallbacks == null)
      LongToLongCallbacks = new List<LongToLong>();
    LongToLongCallbacks.Add(c);
    return c;
  }
  private List<LongLongToLong> LongLongToLongCallbacks;
  private LongLongToLong StoreLongLongToLong(LongLongToLong c) {
    if (LongLongToLongCallbacks == null)
      LongLongToLongCallbacks = new List<LongLongToLong>();
    LongLongToLongCallbacks.Add(c);
    return c;
  }
  private List<LongLongLongToLong> LongLongLongToLongCallbacks;
  private LongLongLongToLong StoreLongLongLongToLong(LongLongLongToLong c) {
    if (LongLongLongToLongCallbacks == null)
      LongLongLongToLongCallbacks = new List<LongLongLongToLong>();
    LongLongLongToLongCallbacks.Add(c);
    return c;
  }

  private List<VoidToBoolean> limiterCallbacks;
  private VoidToBoolean StoreVoidToBoolean(VoidToBoolean limiter) {
    if (limiterCallbacks == null)
      limiterCallbacks = new List<VoidToBoolean>();
    limiterCallbacks.Add(limiter);
    return limiter;
  }

  private List<LongLongLongToBoolean> variableValueComparatorCallbacks;
  private LongLongLongToBoolean StoreLongLongLongToBoolean(
    LongLongLongToBoolean c) {
    if (variableValueComparatorCallbacks == null)
      variableValueComparatorCallbacks = new List<LongLongLongToBoolean>();
    variableValueComparatorCallbacks.Add(c);
    return c;
  }

  private List<LongToBoolean> indexFilter1Callbacks;
  private LongToBoolean StoreLongToBoolean(LongToBoolean c) {
    if (indexFilter1Callbacks == null)
      indexFilter1Callbacks = new List<LongToBoolean>();
    indexFilter1Callbacks.Add(c);
    return c;
  }

  private List<LongToVoid> objectiveWatcherCallbacks;
  private LongToVoid StoreLongToVoid(LongToVoid c) {
    if (objectiveWatcherCallbacks == null)
      objectiveWatcherCallbacks = new List<LongToVoid>();
    objectiveWatcherCallbacks.Add(c);
    return c;
  }

  private List<SolverToVoid> actionCallbacks;
  private SolverToVoid StoreSolverToVoid(SolverToVoid action) {
    if (actionCallbacks == null)
      actionCallbacks = new List<SolverToVoid>();
    actionCallbacks.Add(action);
    return action;
  }

  private List<VoidToVoid> closureCallbacks;
  private VoidToVoid StoreVoidToVoid(VoidToVoid closure) {
    if (closureCallbacks == null)
      closureCallbacks = new List<VoidToVoid>();
    closureCallbacks.Add(closure);
    return closure;
  }

  // note: Should be store in LocalSearchOperator
  private List<IntIntToLong> evaluatorCallbacks;
  private IntIntToLong StoreIntIntToLong(IntIntToLong evaluator) {
    if (evaluatorCallbacks == null)
      evaluatorCallbacks = new List<IntIntToLong>();
    evaluatorCallbacks.Add(evaluator);
    return evaluator;
  }
%}
// Ignored:
%ignore Solver::SearchLogParameters;
%ignore Solver::ActiveSearch;
%ignore Solver::SetSearchContext;
%ignore Solver::SearchContext;
%ignore Solver::MakeSearchLog(SearchLogParameters parameters);
%ignore Solver::MakeIntVarArray;
%ignore Solver::MakeBoolVarArray;
%ignore Solver::MakeFixedDurationIntervalVarArray;
%ignore Solver::SetBranchSelector;
%ignore Solver::MakeApplyBranchSelector;
%ignore Solver::MakeAtMost;
%ignore Solver::demon_profiler;
%ignore Solver::set_fail_intercept;
%ignore Solver::tmp_vector_;
// Methods:
%rename (Add) Solver::AddConstraint;
// Rename NewSearch and EndSearch to add pinning. See the overrides of
// NewSearch in ../../open_source/csharp/constraint_solver/SolverHelper.cs
%rename (NewSearchAux) Solver::NewSearch;
%rename (EndSearchAux) Solver::EndSearch;

// IntExpr
%unignore IntExpr;
%typemap(cscode) IntExpr %{
  // Keep reference to delegate to avoid GC to collect them early
  private List<VoidToVoid> closureCallbacks;
  private VoidToVoid StoreVoidToVoid(VoidToVoid closure) {
    if (closureCallbacks == null)
      closureCallbacks = new List<VoidToVoid>();
    closureCallbacks.Add(closure);
    return closure;
  }
%}
// Methods:
%extend IntExpr {
  Constraint* MapTo(const std::vector<IntVar*>& vars) {
    return $self->solver()->MakeMapDomain($self->Var(), vars);
  }
  IntExpr* IndexOf(const std::vector<int64>& vars) {
    return $self->solver()->MakeElement(vars, $self->Var());
  }
  IntExpr* IndexOf(const std::vector<IntVar*>& vars) {
    return $self->solver()->MakeElement(vars, $self->Var());
  }
  IntVar* IsEqual(int64 value) {
    return $self->solver()->MakeIsEqualCstVar($self->Var(), value);
  }
  IntVar* IsDifferent(int64 value) {
    return $self->solver()->MakeIsDifferentCstVar($self->Var(), value);
  }
  IntVar* IsGreater(int64 value) {
    return $self->solver()->MakeIsGreaterCstVar($self->Var(), value);
  }
  IntVar* IsGreaterOrEqual(int64 value) {
    return $self->solver()->MakeIsGreaterOrEqualCstVar($self->Var(), value);
  }
  IntVar* IsLess(int64 value) {
    return $self->solver()->MakeIsLessCstVar($self->Var(), value);
  }
  IntVar* IsLessOrEqual(int64 value) {
    return $self->solver()->MakeIsLessOrEqualCstVar($self->Var(), value);
  }
  IntVar* IsMember(const std::vector<int64>& values) {
    return $self->solver()->MakeIsMemberVar($self->Var(), values);
  }
  IntVar* IsMember(const std::vector<int>& values) {
    return $self->solver()->MakeIsMemberVar($self->Var(), values);
  }
  Constraint* Member(const std::vector<int64>& values) {
    return $self->solver()->MakeMemberCt($self->Var(), values);
  }
  Constraint* Member(const std::vector<int>& values) {
    return $self->solver()->MakeMemberCt($self->Var(), values);
  }
  IntVar* IsEqual(IntExpr* const other) {
    return $self->solver()->MakeIsEqualVar($self->Var(), other->Var());
  }
  IntVar* IsDifferent(IntExpr* const other) {
    return $self->solver()->MakeIsDifferentVar($self->Var(), other->Var());
  }
  IntVar* IsGreater(IntExpr* const other) {
    return $self->solver()->MakeIsGreaterVar($self->Var(), other->Var());
  }
  IntVar* IsGreaterOrEqual(IntExpr* const other) {
    return $self->solver()->MakeIsGreaterOrEqualVar($self->Var(), other->Var());
  }
  IntVar* IsLess(IntExpr* const other) {
    return $self->solver()->MakeIsLessVar($self->Var(), other->Var());
  }
  IntVar* IsLessOrEqual(IntExpr* const other) {
    return $self->solver()->MakeIsLessOrEqualVar($self->Var(), other->Var());
  }
  OptimizeVar* Minimize(long step) {
    return $self->solver()->MakeMinimize($self->Var(), step);
  }
  OptimizeVar* Maximize(long step) {
    return $self->solver()->MakeMaximize($self->Var(), step);
  }
}

// IntVar
%unignore IntVar;
%typemap(cscode) IntVar %{
  // Keep reference to delegate to avoid GC to collect them early
  private List<VoidToVoid> closureCallbacks;
  private VoidToVoid StoreVoidToVoid(VoidToVoid closure) {
    if (closureCallbacks == null)
      closureCallbacks = new List<VoidToVoid>();
    closureCallbacks.Add(closure);
    return closure;
  }
%}
// Ignored:
%ignore IntVar::MakeDomainIterator;
%ignore IntVar::MakeHoleIterator;
// Methods:
%extend IntVar {
  IntVarIterator* GetDomain() {
    return $self->MakeDomainIterator(false);
  }
  IntVarIterator* GetHoles() {
    return $self->MakeHoleIterator(false);
  }
}

%typemap(csinterfaces_derived) IntVarIterator "IEnumerable";

// IntervalVar
%unignore IntervalVar;
%typemap(cscode) IntervalVar %{
  // Keep reference to delegate to avoid GC to collect them early
  private List<VoidToVoid> closureCallbacks;
  private VoidToVoid StoreVoidToVoid(VoidToVoid closure) {
    if (closureCallbacks == null)
      closureCallbacks = new List<VoidToVoid>();
    closureCallbacks.Add(closure);
    return closure;
  }
%}
// Extend IntervalVar with an intuitive API to create precedence constraints.
%extend IntervalVar {
  Constraint* EndsAfterEnd(IntervalVar* other) {
    return $self->solver()->MakeIntervalVarRelation($self, operations_research::Solver::ENDS_AFTER_END, other);
  }
  Constraint* EndsAfterStart(IntervalVar* other) {
    return $self->solver()->MakeIntervalVarRelation($self, operations_research::Solver::ENDS_AFTER_START, other);
  }
  Constraint* EndsAtEnd(IntervalVar* other) {
    return $self->solver()->MakeIntervalVarRelation($self, operations_research::Solver::ENDS_AT_END, other);
  }
  Constraint* EndsAtStart(IntervalVar* other) {
    return $self->solver()->MakeIntervalVarRelation($self, operations_research::Solver::ENDS_AT_START, other);
  }
  Constraint* StartsAfterEnd(IntervalVar* other) {
    return $self->solver()->MakeIntervalVarRelation($self, operations_research::Solver::STARTS_AFTER_END, other);
  }
  Constraint* StartsAfterStart(IntervalVar* other) {
    return $self->solver()->MakeIntervalVarRelation($self, operations_research::Solver::STARTS_AFTER_START, other);
  }
  Constraint* StartsAtEnd(IntervalVar* other) {
    return $self->solver()->MakeIntervalVarRelation($self, operations_research::Solver::STARTS_AT_END, other);
  }
  Constraint* StartsAtStart(IntervalVar* other) {
    return $self->solver()->MakeIntervalVarRelation($self, operations_research::Solver::STARTS_AT_START, other);
  }
  Constraint* EndsAfterEndWithDelay(IntervalVar* other, int64 delay) {
    return $self->solver()->MakeIntervalVarRelationWithDelay($self, operations_research::Solver::ENDS_AFTER_END, other, delay);
  }
  Constraint* EndsAfterStartWithDelay(IntervalVar* other, int64 delay) {
    return $self->solver()->MakeIntervalVarRelationWithDelay($self, operations_research::Solver::ENDS_AFTER_START, other, delay);
  }
  Constraint* EndsAtEndWithDelay(IntervalVar* other, int64 delay) {
    return $self->solver()->MakeIntervalVarRelationWithDelay($self, operations_research::Solver::ENDS_AT_END, other, delay);
  }
  Constraint* EndsAtStartWithDelay(IntervalVar* other, int64 delay) {
    return $self->solver()->MakeIntervalVarRelationWithDelay($self, operations_research::Solver::ENDS_AT_START, other, delay);
  }
  Constraint* StartsAfterEndWithDelay(IntervalVar* other, int64 delay) {
    return $self->solver()->MakeIntervalVarRelationWithDelay($self, operations_research::Solver::STARTS_AFTER_END, other, delay);
  }
  Constraint* StartsAfterStartWithDelay(IntervalVar* other, int64 delay) {
    return $self->solver()->MakeIntervalVarRelationWithDelay($self, operations_research::Solver::STARTS_AFTER_START, other, delay);
  }
  Constraint* StartsAtEndWithDelay(IntervalVar* other, int64 delay) {
    return $self->solver()->MakeIntervalVarRelationWithDelay($self, operations_research::Solver::STARTS_AT_END, other, delay);
  }
  Constraint* StartsAtStartWithDelay(IntervalVar* other, int64 delay) {
    return $self->solver()->MakeIntervalVarRelationWithDelay($self, operations_research::Solver::STARTS_AT_START, other, delay);
  }
  Constraint* EndsAfter(int64 date) {
    return $self->solver()->MakeIntervalVarRelation($self, operations_research::Solver::ENDS_AFTER, date);
  }
  Constraint* EndsAt(int64 date) {
    return $self->solver()->MakeIntervalVarRelation($self, operations_research::Solver::ENDS_AT, date);
  }
  Constraint* EndsBefore(int64 date) {
    return $self->solver()->MakeIntervalVarRelation($self, operations_research::Solver::ENDS_BEFORE, date);
  }
  Constraint* StartsAfter(int64 date) {
    return $self->solver()->MakeIntervalVarRelation($self, operations_research::Solver::STARTS_AFTER, date);
  }
  Constraint* StartsAt(int64 date) {
    return $self->solver()->MakeIntervalVarRelation($self, operations_research::Solver::STARTS_AT, date);
  }
  Constraint* StartsBefore(int64 date) {
    return $self->solver()->MakeIntervalVarRelation($self, operations_research::Solver::STARTS_BEFORE, date);
  }
  Constraint* CrossesDate(int64 date) {
    return $self->solver()->MakeIntervalVarRelation($self, operations_research::Solver::CROSS_DATE, date);
  }
  Constraint* AvoidsDate(int64 date) {
    return $self->solver()->MakeIntervalVarRelation($self, operations_research::Solver::AVOID_DATE, date);
  }
  IntervalVar* RelaxedMax() {
    return $self->solver()->MakeIntervalRelaxedMax($self);
  }
  IntervalVar* RelaxedMin() {
    return $self->solver()->MakeIntervalRelaxedMin($self);
  }
}

// OptimizeVar
%feature("director") OptimizeVar;
%unignore OptimizeVar;
// Methods:
%unignore OptimizeVar::ApplyBound;
%unignore OptimizeVar::Print;
%unignore OptimizeVar::Var;

// SequenceVar
%unignore SequenceVar;
// Ignored:
%ignore SequenceVar::ComputePossibleFirstsAndLasts;
%ignore SequenceVar::FillSequence;

// Constraint
%feature("director") Constraint;
%unignore Constraint;
%typemap(csinterfaces_derived) Constraint "IConstraintWithStatus";
// Ignored:
%ignore Constraint::PostAndPropagate;
// Methods:
%rename (InitialPropagateWrapper) Constraint::InitialPropagate;
%feature ("nodirector") Constraint::Accept;
%feature ("nodirector") Constraint::Var;
%feature ("nodirector") Constraint::IsCastConstraint;

// DisjunctiveConstraint
%unignore DisjunctiveConstraint;
%typemap(cscode) DisjunctiveConstraint %{
  // Store list of delegates to avoid the GC to reclaim them.
  private List<LongLongToLong> LongLongToLongCallbacks;
  // Ensure that the GC does not collect any IndexEvaluator1Callback set from C#
  // as the underlying C++ class will only store a pointer to it (i.e. no ownership).
  private LongLongToLong StoreLongLongToLong(LongLongToLong c) {
    if (LongLongToLongCallbacks == null)
      LongLongToLongCallbacks = new List<LongLongToLong>();
    LongLongToLongCallbacks.Add(c);
    return c;
  }
%}
// Methods:
%rename (SequenceVar) DisjunctiveConstraint::MakeSequenceVar;

// Pack
%unignore Pack;
%typemap(cscode) Pack %{
  // Store list of delegates to avoid the GC to reclaim them.
  private List<LongToLong> LongToLongCallbacks;
  private List<LongLongToLong> LongLongToLongCallbacks;
  // Ensure that the GC does not collect any IndexEvaluator1Callback set from C#
  // as the underlying C++ class will only store a pointer to it (i.e. no ownership).
  private LongToLong StoreLongToLong(LongToLong c) {
    if (LongToLongCallbacks == null)
      LongToLongCallbacks = new List<LongToLong>();
    LongToLongCallbacks.Add(c);
    return c;
  }
  private LongLongToLong StoreLongLongToLong(LongLongToLong c) {
    if (LongLongToLongCallbacks == null)
      LongLongToLongCallbacks = new List<LongLongToLong>();
    LongLongToLongCallbacks.Add(c);
    return c;
  }
%}

// PropagationBaseObject
%unignore PropagationBaseObject;
// Ignored:
%ignore PropagationBaseObject::ExecuteAll;
%ignore PropagationBaseObject::EnqueueAll;
%ignore PropagationBaseObject::set_action_on_fail;

// SearchMonitor
%feature("director") SearchMonitor;
%unignore SearchMonitor;

// SearchLimit
%feature("director") SearchLimit;
%unignore SearchLimit;
// Methods:
%rename (IsCrossed) SearchLimit::crossed;

// Searchlog
%unignore SearchLog;
// Ignored:
// No custom wrapping for this method, we simply ignore it.
%ignore SearchLog::SearchLog(
    Solver* const s, OptimizeVar* const obj, IntVar* const var,
    double scaling_factor,
    std::function<std::string()> display_callback, int period);
// Methods:
%unignore SearchLog::Maintain;
%unignore SearchLog::OutputDecision;

// IntVarLocalSearchHandler
%ignore IntVarLocalSearchHandler;

// SequenceVarLocalSearchHandler
%ignore SequenceVarLocalSearchHandler;

// LocalSearchOperator
%feature("director") LocalSearchOperator;
%unignore LocalSearchOperator;
// Methods:
%unignore LocalSearchOperator::MakeNextNeighbor;
%unignore LocalSearchOperator::Reset;
%unignore LocalSearchOperator::Start;

// VarLocalSearchOperator<>
%unignore VarLocalSearchOperator;
// Ignored:
%ignore VarLocalSearchOperator::Start;
%ignore VarLocalSearchOperator::ApplyChanges;
%ignore VarLocalSearchOperator::RevertChanges;
%ignore VarLocalSearchOperator::SkipUnchanged;
// Methods:
%unignore VarLocalSearchOperator::Size;
%unignore VarLocalSearchOperator::Value;
%unignore VarLocalSearchOperator::IsIncremental;
%unignore VarLocalSearchOperator::OnStart;
%unignore VarLocalSearchOperator::OldValue;
%unignore VarLocalSearchOperator::SetValue;
%unignore VarLocalSearchOperator::Var;
%unignore VarLocalSearchOperator::Activated;
%unignore VarLocalSearchOperator::Activate;
%unignore VarLocalSearchOperator::Deactivate;
%unignore VarLocalSearchOperator::AddVars;

// IntVarLocalSearchOperator
%feature("director") IntVarLocalSearchOperator;
%unignore IntVarLocalSearchOperator;
// Ignored:
%ignore IntVarLocalSearchOperator::MakeNextNeighbor;
// Methods:
%unignore IntVarLocalSearchOperator::Size;
%unignore IntVarLocalSearchOperator::MakeOneNeighbor;
%unignore IntVarLocalSearchOperator::Value;
%unignore IntVarLocalSearchOperator::IsIncremental;
%unignore IntVarLocalSearchOperator::OnStart;
%unignore IntVarLocalSearchOperator::OldValue;
%unignore IntVarLocalSearchOperator::SetValue;
%unignore IntVarLocalSearchOperator::Var;
%unignore IntVarLocalSearchOperator::Activated;
%unignore IntVarLocalSearchOperator::Activate;
%unignore IntVarLocalSearchOperator::Deactivate;
%unignore IntVarLocalSearchOperator::AddVars;

// BaseLns
%feature("director") BaseLns;
%unignore BaseLns;
// Methods:
%unignore BaseLns::InitFragments;
%unignore BaseLns::NextFragment;
%feature ("nodirector") BaseLns::OnStart;
%feature ("nodirector") BaseLns::SkipUnchanged;
%feature ("nodirector") BaseLns::MakeOneNeighbor;
%unignore BaseLns::IsIncremental;
%unignore BaseLns::AppendToFragment;
%unignore BaseLns::FragmentSize;

// ChangeValue
%feature("director") ChangeValue;
%unignore ChangeValue;
// Methods:
%unignore ChangeValue::ModifyValue;

// SequenceVarLocalSearchOperator
%feature("director") SequenceVarLocalSearchOperator;
%unignore SequenceVarLocalSearchOperator;
// Ignored:
%ignore SequenceVarLocalSearchOperator::SetBackwardSequence;
%ignore SequenceVarLocalSearchOperator::SetForwardSequence;
// Methods:
%unignore SequenceVarLocalSearchOperator::OldSequence;
%unignore SequenceVarLocalSearchOperator::Sequence;
%unignore SequenceVarLocalSearchOperator::Start;

// PathOperator
%feature("director") PathOperator;
%unignore PathOperator;
%typemap(cscode) PathOperator %{
  // Keep reference to delegate to avoid GC to collect them early
  private List<LongToInt> startEmptyPathCallbacks;
  private LongToInt StoreLongToInt(LongToInt path) {
    if (startEmptyPathCallbacks == null)
      startEmptyPathCallbacks = new List<LongToInt>();
    startEmptyPathCallbacks.Add(path);
    return path;
  }
%}
// Ignored:
%ignore PathOperator::PathOperator;
%ignore PathOperator::Next;
%ignore PathOperator::Path;
%ignore PathOperator::SkipUnchanged;
%ignore PathOperator::number_of_nexts;
// Methods:
%unignore PathOperator::MakeNeighbor;

// LocalSearchFilter
%feature("director") LocalSearchFilter;
%unignore LocalSearchFilter;
// Methods:
%unignore LocalSearchFilter::Accept;
%unignore LocalSearchFilter::Synchronize;
%unignore LocalSearchFilter::IsIncremental;

// IntVarLocalSearchFilter
%feature("director") IntVarLocalSearchFilter;
%unignore IntVarLocalSearchFilter;
%typemap(cscode) IntVarLocalSearchFilter %{
  // Store list of delegates to avoid the GC to reclaim them.
  private LongToVoid objectiveWatcherCallbacks;
  // Ensure that the GC does not collect any IndexEvaluator1Callback set from C#
  // as the underlying C++ class will only store a pointer to it (i.e. no ownership).
  private LongToVoid StoreLongToVoid(LongToVoid c) {
    objectiveWatcherCallbacks = c;
    return c;
  }
%}
// Ignored:
%ignore IntVarLocalSearchFilter::FindIndex;
%ignore IntVarLocalSearchFilter::IntVarLocalSearchFilter(
    const std::vector<IntVar*>& vars,
    Solver::ObjectiveWatcher objective_callback);
%ignore IntVarLocalSearchFilter::IsVarSynced;
// Methods:
%feature("nodirector") IntVarLocalSearchFilter::Synchronize;  // Inherited.
%unignore IntVarLocalSearchFilter::AddVars;  // Inherited.
%unignore IntVarLocalSearchFilter::IsIncremental;
%unignore IntVarLocalSearchFilter::OnSynchronize;
%unignore IntVarLocalSearchFilter::Size;
%unignore IntVarLocalSearchFilter::Start;
%unignore IntVarLocalSearchFilter::Value;
%unignore IntVarLocalSearchFilter::Var;  // Inherited.
// Extend IntVarLocalSearchFilter with an intuitive API.
%extend IntVarLocalSearchFilter {
  int Index(IntVar* const var) {
    int64 index = -1;
    $self->FindIndex(var, &index);
    return index;
  }
}

// Demon
%feature("director") Demon;
%unignore Demon;
// Methods:
%feature("nodirector") Demon::inhibit;
%feature("nodirector") Demon::desinhibit;
%rename (RunWrapper) Demon::Run;
%rename (Inhibit) Demon::inhibit;
%rename (Desinhibit) Demon::desinhibit;

class LocalSearchPhaseParameters {
 public:
  LocalSearchPhaseParameters();
  ~LocalSearchPhaseParameters();
};

}  // namespace operations_research

%define CONVERT_VECTOR(CTYPE, TYPE)
SWIG_STD_VECTOR_ENHANCED(CTYPE*);
%template(TYPE ## Vector) std::vector<CTYPE*>;
%enddef  // CONVERT_VECTOR

CONVERT_VECTOR(operations_research::IntVar, IntVar)
CONVERT_VECTOR(operations_research::SearchMonitor, SearchMonitor)
CONVERT_VECTOR(operations_research::DecisionBuilder, DecisionBuilder)
CONVERT_VECTOR(operations_research::IntervalVar, IntervalVar)
CONVERT_VECTOR(operations_research::SequenceVar, SequenceVar)
CONVERT_VECTOR(operations_research::LocalSearchOperator, LocalSearchOperator)
CONVERT_VECTOR(operations_research::LocalSearchFilter, LocalSearchFilter)
CONVERT_VECTOR(operations_research::SymmetryBreaker, SymmetryBreaker)

#undef CONVERT_VECTOR

// Generic rename rule
%rename("%(camelcase)s", %$isfunction) "";
%rename (ToString) *::DebugString;
%rename (solver) *::solver;

%pragma(csharp) imclassimports=%{
// Used to wrap DisplayCallback (std::function<std::string()>)
public delegate string VoidToString();

// Used to wrap std::function<bool()>
public delegate bool VoidToBoolean();

// Used to wrap std::function<int(int64)>
public delegate int LongToInt(long t);

// Used to wrap IndexEvaluator1 (std::function<int64(int64)>)
public delegate long LongToLong(long t);
// Used to wrap IndexEvaluator2 (std::function<int64(int64, int64)>)
public delegate long LongLongToLong(long t, long u);
// Used to wrap IndexEvaluator3 (std::function<int64(int64, int64, int64)>)
public delegate long LongLongLongToLong(long t, long u, long v);

// Used to wrap std::function<int64(int, int)>
public delegate long IntIntToLong(int t, int u);

// Used to wrap IndexFilter1 (std::function<bool(int64)>)
public delegate bool LongToBoolean(long t);

// Used to wrap std::function<bool(int64, int64, int64)>
public delegate bool LongLongLongToBoolean(long t, long u, long v);

// Used to wrap std::function<void(Solver*)>
public delegate void SolverToVoid(Solver s);

// Used to wrap ObjectiveWatcher (std::function<void(int64)>)
public delegate void LongToVoid(long t);

// Used to wrap Closure (std::function<void()>)
public delegate void VoidToVoid();
%}

// Protobuf support
PROTO_INPUT(operations_research::ConstraintSolverParameters,
            Google.OrTools.ConstraintSolver.ConstraintSolverParameters,
            parameters)
PROTO2_RETURN(operations_research::ConstraintSolverParameters,
              Google.OrTools.ConstraintSolver.ConstraintSolverParameters)

PROTO_INPUT(operations_research::SearchLimitParameters,
            Google.OrTools.ConstraintSolver.SearchLimitParameters,
            proto)
PROTO2_RETURN(operations_research::SearchLimitParameters,
              Google.OrTools.ConstraintSolver.SearchLimitParameters)

PROTO_INPUT(operations_research::CpModel,
            Google.OrTools.ConstraintSolver.CpModel,
            proto)
PROTO2_RETURN(operations_research::CpModel,
              Google.OrTools.ConstraintSolver.CpModel)

namespace operations_research {
// Globals
// IMPORTANT(corentinl): Global will be placed in operations_research_constraint_solver.cs
// Ignored:
%ignore FillValues;
}  // namespace operations_research

// Wrap cp includes
// TODO(user): Replace with %ignoreall/%unignoreall
//swiglint: disable include-h-allglobals
%include "ortools/constraint_solver/constraint_solver.h"
%include "ortools/constraint_solver/constraint_solveri.h"

namespace operations_research {
%template(RevInteger) Rev<int64>;
%template(RevBool) Rev<bool>;
typedef Assignment::AssignmentContainer AssignmentContainer;
%template(AssignmentIntContainer) AssignmentContainer<IntVar, IntVarElement>;
%template(AssignmentIntervalContainer) AssignmentContainer<IntervalVar, IntervalVarElement>;
%template(AssignmentSequenceContainer) AssignmentContainer<SequenceVar, SequenceVarElement>;
}
