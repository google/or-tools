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

%feature("director") BaseLns;
%feature("director") Decision;
%feature("director") DecisionBuilder;
%feature("director") IntVarLocalSearchFilter;
%feature("director") IntVarLocalSearchOperator;
%feature("director") LocalSearchOperator;
%feature("director") OptimizeVar;
%feature("director") SearchLimit;
%feature("director") SearchMonitor;
%feature("director") SequenceVarLocalSearchOperator;
%feature("director") SolutionCollector;
%feature("director") SymmetryBreaker;
%{
#include <setjmp.h>

#include <string>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/util/functions_swig_helpers.h"
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

//typedef int64_t int64;
//typedef uint64_t uint64;

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

// Since knapsack_solver.i and constraint_solver.i both need to
// instantiate the vector template, but their csharp_wrap.cc
// files end up being compiled into the same .dll, we must name the
// vector template differently.
%template(CpIntVector) std::vector<int>;
%template(CpInt64Vector) std::vector<int64>;
%template(CpIntVectorVector) std::vector<std::vector<int> >;
%template(CpInt64VectorVector) std::vector<std::vector<int64> >;

%define CS_TYPEMAP_STDVECTOR_OBJECT(CTYPE, TYPE)
SWIG_STD_VECTOR_ENHANCED(operations_research::CTYPE*);
%template(TYPE ## Vector) std::vector<CTYPE*>;
%enddef  // CS_TYPEMAP_STDVECTOR_OBJECT

CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::IntVar, IntVar)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::SearchMonitor, SearchMonitor)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::DecisionBuilder, DecisionBuilder)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::IntervalVar, IntervalVar)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::SequenceVar, SequenceVar)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::LocalSearchOperator, LocalSearchOperator)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::LocalSearchFilter, LocalSearchFilter)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::SymmetryBreaker, SymmetryBreaker)

%ignore operations_research::PropagationBaseObject::set_action_on_fail;

// Generic rename rule.
%rename("%(camelcase)s", %$isfunction) "";

// Rename rules on Demon.
%feature("director") operations_research::Demon;
%feature("nodirector") operations_research::Demon::inhibit;
%feature("nodirector") operations_research::Demon::desinhibit;
%rename (RunWrapper) operations_research::Demon::Run;
%rename (Inhibit) operations_research::Demon::inhibit;
%rename (Desinhibit) operations_research::Demon::desinhibit;

// Rename rules on Constraint.
%feature("director") operations_research::Constraint;
%rename (InitialPropagateWrapper) operations_research::Constraint::InitialPropagate;
%ignore operations_research::Constraint::PostAndPropagate;
%feature ("nodirector") operations_research::Constraint::Accept;
%ignore operations_research::Constraint::Accept;
%feature ("nodirector") operations_research::Constraint::Var;
%feature ("nodirector") operations_research::Constraint::IsCastConstraint;
%ignore operations_research::Constraint::IsCastConstraint;

// Rename rule on DecisionBuilder;
%rename (NextWrapper) operations_research::DecisionBuilder::Next;

// Rename rule on DecisionBuilder;
%rename (ApplyWrapper) operations_research::Decision::Apply;
%rename (RefuteWrapper) operations_research::Decision::Refute;

// Rename rule on SearchLimit
%rename (IsCrossed) operations_research::SearchLimit::crossed;

// Rename rule on DisjunctiveConstraint.
%rename (SequenceVar) operations_research::DisjunctiveConstraint::MakeSequenceVar;
// Keep reference to delegate to avoid GC to collect them early
%typemap(cscode) operations_research::DisjunctiveConstraint %{
  // Store list of delegates to avoid the GC to reclaim them.
  private List<IndexEvaluator2> indexEvaluator2Callbacks;
  // Ensure that the GC does not collect any IndexEvaluator1Callback set from C#
  // as the underlying C++ class will only store a pointer to it (i.e. no ownership).
  private IndexEvaluator2 StoreIndexEvaluator2(IndexEvaluator2 c) {
    if (indexEvaluator2Callbacks == null) indexEvaluator2Callbacks = new List<IndexEvaluator2>();
    indexEvaluator2Callbacks.Add(c);
    return c;
  }
%}

// Generic rename rules.
%rename (ToString) *::DebugString;

// Keep the .solver() API.
%rename (solver) *::solver;

// LocalSearchOperator
%feature("director") operations_research::LocalSearchOperator;
%unignore operations_research::LocalSearchOperator::MakeNextNeighbor;
%unignore operations_research::LocalSearchOperator::Start;

// VarLocalSearchOperator<>
// Ignored:
// - Start()
// - SkipUnchanged()
// - ApplyChanges()
// - RevertChanges()
%unignore operations_research::VarLocalSearchOperator::Size;
%unignore operations_research::VarLocalSearchOperator::Value;
%unignore operations_research::VarLocalSearchOperator::IsIncremental;
%unignore operations_research::VarLocalSearchOperator::OnStart;
%unignore operations_research::VarLocalSearchOperator::OldValue;
%unignore operations_research::VarLocalSearchOperator::SetValue;
%unignore operations_research::VarLocalSearchOperator::Var;
%unignore operations_research::VarLocalSearchOperator::Activated;
%unignore operations_research::VarLocalSearchOperator::Activate;
%unignore operations_research::VarLocalSearchOperator::Deactivate;
%unignore operations_research::VarLocalSearchOperator::AddVars;

// IntVarLocalSearchOperator
%feature("director") operations_research::IntVarLocalSearchOperator;
%unignore operations_research::IntVarLocalSearchOperator::Size;
%unignore operations_research::IntVarLocalSearchOperator::MakeOneNeighbor;
%unignore operations_research::IntVarLocalSearchOperator::Value;
%unignore operations_research::IntVarLocalSearchOperator::IsIncremental;
%unignore operations_research::IntVarLocalSearchOperator::OnStart;
%unignore operations_research::IntVarLocalSearchOperator::OldValue;
%unignore operations_research::IntVarLocalSearchOperator::SetValue;
%unignore operations_research::IntVarLocalSearchOperator::Var;
%unignore operations_research::IntVarLocalSearchOperator::Activated;
%unignore operations_research::IntVarLocalSearchOperator::Activate;
%unignore operations_research::IntVarLocalSearchOperator::Deactivate;
%unignore operations_research::IntVarLocalSearchOperator::AddVars;
%ignore operations_research::IntVarLocalSearchOperator::MakeNextNeighbor;

%feature("director") operations_research::BaseLns;
%unignore operations_research::BaseLns::InitFragments;
%unignore operations_research::BaseLns::NextFragment;
%feature ("nodirector") operations_research::BaseLns::OnStart;
%feature ("nodirector") operations_research::BaseLns::SkipUnchanged;
%feature ("nodirector") operations_research::BaseLns::MakeOneNeighbor;
%unignore operations_research::BaseLns::IsIncremental;
%unignore operations_research::BaseLns::AppendToFragment;
%unignore operations_research::BaseLns::FragmentSize;

// ChangeValue
%feature("director") operations_research::ChangeValue;
%unignore operations_research::ChangeValue::ModifyValue;

// SequenceVarLocalSearchOperator
// Ignored:
// - Sequence()
// - OldSequence()
// - SetForwardSequence()
// - SetBackwardSequence()
%feature("director") operations_research::SequenceVarLocalSearchOperator;
%unignore operations_research::SequenceVarLocalSearchOperator::Start;

// PathOperator
// Ignored:
// - SkipUnchanged()
// - Next()
// - Path()
// - number_of_nexts()
%feature("director") operations_research::PathOperator;
%unignore operations_research::PathOperator::MakeNeighbor;

// LocalSearchFilter
%feature("director") operations_research::LocalSearchFilter;
%unignore operations_research::LocalSearchFilter::Accept;
%unignore operations_research::LocalSearchFilter::Synchronize;
%unignore operations_research::LocalSearchFilter::IsIncremental;

// IntVarLocalSearchFilter
// Ignored:
// - IsVarSynced()
%feature("director") operations_research::IntVarLocalSearchFilter;
%feature("nodirector") operations_research::IntVarLocalSearchFilter::Synchronize;  // Inherited.
%ignore operations_research::IntVarLocalSearchFilter::FindIndex;
%ignore operations_research::IntVarLocalSearchFilter::IntVarLocalSearchFilter(
    const std::vector<IntVar*>& vars,
    Solver::ObjectiveWatcher objective_callback);
%unignore operations_research::IntVarLocalSearchFilter::AddVars;  // Inherited.
%unignore operations_research::IntVarLocalSearchFilter::IsIncremental;
%unignore operations_research::IntVarLocalSearchFilter::OnSynchronize;
%unignore operations_research::IntVarLocalSearchFilter::Size;
%unignore operations_research::IntVarLocalSearchFilter::Start;
%unignore operations_research::IntVarLocalSearchFilter::Value;
%unignore operations_research::IntVarLocalSearchFilter::Var;  // Inherited.
// Extend IntVarLocalSearchFilter with an intuitive API.
%extend operations_research::IntVarLocalSearchFilter {
  int Index(IntVar* const var) {
    int64 index = -1;
    $self->FindIndex(var, &index);
    return index;
  }
}
// Keep reference to delegate to avoid GC to collect them early
%typemap(cscode) operations_research::IntVarLocalSearchFilter %{
  // Store list of delegates to avoid the GC to reclaim them.
  private ObjectiveWatcher objectiveWatcherCallbacks;
  // Ensure that the GC does not collect any IndexEvaluator1Callback set from C#
  // as the underlying C++ class will only store a pointer to it (i.e. no ownership).
  private ObjectiveWatcher StoreObjectiveWatcher(ObjectiveWatcher c) {
    objectiveWatcherCallbacks = c;
    return c;
  }
%}

// Transform IntVar.
%ignore operations_research::IntVar::MakeDomainIterator;
%ignore operations_research::IntVar::MakeHoleIterator;

%extend operations_research::IntVar {
  operations_research::IntVarIterator* GetDomain() {
    return $self->MakeDomainIterator(false);
  }

  operations_research::IntVarIterator* GetHoles() {
    return $self->MakeHoleIterator(false);
  }
}

%typemap(csinterfaces_derived) operations_research::IntVarIterator "IEnumerable";

%typemap(csinterfaces_derived) operations_research::Constraint "IConstraintWithStatus";

// Solver
namespace operations_research {
// Ignore rules on Solver.
%ignore Solver::MakeIntVarArray;
%ignore Solver::MakeBoolVarArray;
%ignore Solver::MakeFixedDurationIntervalVarArray;
// Take care of API with function. SWIG doesn't wrap std::function<> properly,
// so we write our custom wrappers for all methods involving std::function<>.
%ignore Solver::MakeSearchLog(
    int branch_period,
    std::function<std::string()> display_callback);
%ignore Solver::MakeSearchLog(
    int branch_period,
    IntVar* var,
    std::function<std::string()> display_callback);
%ignore Solver::MakeSearchLog(
    int branch_period,
    OptimizeVar* const opt_var,
    std::function<std::string()> display_callback);

%ignore Solver::MakeActionDemon;
%ignore Solver::MakeCustomLimit(std::function<bool()> limiter);

%ignore Solver::ConcatenateOperators(
    const std::vector<LocalSearchOperator*>& ops,
    std::function<int64(int, int)> evaluator);
%ignore Solver::MakeClosureDemon(std::function<void()> closure);
%ignore Solver::set_fail_intercept;
// Rename rules on Solver.
%rename (Add) Solver::AddConstraint;
// Rename NewSearch and EndSearch to add pinning. See the overrides of
// NewSearch in ../../open_source/csharp/constraint_solver/SolverHelper.cs
%rename (NewSearchAux) Solver::NewSearch;
%rename (EndSearchAux) Solver::EndSearch;

// Define the delegate IndexEvaluator[1-3] callback types.
// This replace the IndexEvaluator[1-3] in the C# proxy class
%typemap(csimports) Solver %{
  using System.Collections.Generic; // List<>

  public delegate long IndexEvaluator1(long u);
  public delegate long IndexEvaluator2(long u, long v);
  public delegate long IndexEvaluator3(long u, long v, long w);

  public delegate bool IndexFilter1(long u);
  public delegate void ObjectiveWatcher(long u);
%}

// Keep reference to delegate to avoid GC to collect them early
%typemap(cscode) Solver %{
  // Store list of delegates to avoid the GC to reclaim them.
  private List<IndexEvaluator1> indexEvaluator1Callbacks;
  private List<IndexEvaluator2> indexEvaluator2Callbacks;
  private List<IndexEvaluator3> indexEvaluator3Callbacks;

  private List<IndexFilter1> indexFilter1Callbacks;
  private List<ObjectiveWatcher> objectiveWatcherCallbacks;

  // Ensure that the GC does not collect any IndexEvaluator1Callback set from C#
  // as the underlying C++ class will only store a pointer to it (i.e. no ownership).
  private IndexEvaluator1 StoreIndexEvaluator1(IndexEvaluator1 c) {
    if (indexEvaluator1Callbacks == null) indexEvaluator1Callbacks = new List<IndexEvaluator1>();
    indexEvaluator1Callbacks.Add(c);
    return c;
  }
  private IndexEvaluator2 StoreIndexEvaluator2(IndexEvaluator2 c) {
    if (indexEvaluator2Callbacks == null) indexEvaluator2Callbacks = new List<IndexEvaluator2>();
    indexEvaluator2Callbacks.Add(c);
    return c;
  }
  private IndexEvaluator3 StoreIndexEvaluator3(IndexEvaluator3 c) {
    if (indexEvaluator3Callbacks == null) indexEvaluator3Callbacks = new List<IndexEvaluator3>();
    indexEvaluator3Callbacks.Add(c);
    return c;
  }

  private IndexFilter1 StoreIndexFilter1(IndexFilter1 c) {
    if (indexFilter1Callbacks == null) indexFilter1Callbacks = new List<IndexFilter1>();
    indexFilter1Callbacks.Add(c);
    return c;
  }
  private ObjectiveWatcher StoreObjectiveWatcher(ObjectiveWatcher c) {
    if (objectiveWatcherCallbacks == null) objectiveWatcherCallbacks = new List<ObjectiveWatcher>();
    objectiveWatcherCallbacks.Add(c);
    return c;
  }
%}

// Types in Foo.cs Foo::f(cstype csinput, ...) {Foo_f_SWIG(csin, ...);}
// e.g.:
// Foo::f(IndexEvaluator1 arg1) {
//  ...
//  ...PINVOKE.Foo_f_SWIG(..., StoreIndexEvaluator1(arg1), ...);
// }
%typemap(cstype, out="IntPtr") Solver::IndexEvaluator1 "IndexEvaluator1"
%typemap(csin) Solver::IndexEvaluator1 "StoreIndexEvaluator1($csinput)"
%typemap(cstype, out="IntPtr") Solver::IndexEvaluator2 "IndexEvaluator2"
%typemap(csin) Solver::IndexEvaluator2 "StoreIndexEvaluator2($csinput)"
%typemap(cstype, out="IntPtr") Solver::IndexEvaluator3 "IndexEvaluator3"
%typemap(csin) Solver::IndexEvaluator3 "StoreIndexEvaluator3($csinput)"

%typemap(cstype, out="IntPtr") Solver::IndexFilter1 "IndexFilter1"
%typemap(csin) Solver::IndexFilter1 "StoreIndexFilter1($csinput)"
%typemap(cstype, out="IntPtr") Solver::ObjectiveWatcher "ObjectiveWatcher"
%typemap(csin) Solver::ObjectiveWatcher "StoreObjectiveWatcher($csinput)"
// Type in the prototype of PINVOKE function.
%typemap(imtype, out="IntPtr") Solver::IndexEvaluator1 "IndexEvaluator1"
%typemap(imtype, out="IntPtr") Solver::IndexEvaluator2 "IndexEvaluator2"
%typemap(imtype, out="IntPtr") Solver::IndexEvaluator3 "IndexEvaluator3"

%typemap(imtype, out="IntPtr") Solver::IndexFilter1 "IndexFilter1"
%typemap(imtype, out="IntPtr") Solver::ObjectiveWatcher "ObjectiveWatcher"

// Type use in module_csharp_wrap.h function declaration.
// since SWIG generate code as: `ctype argX` we can't use a C function pointer type.
%typemap(ctype) Solver::IndexEvaluator1 "void*" // "int64 (*)(int64)"
%typemap(ctype) Solver::IndexEvaluator2 "void*" // "int64 (*)(int64, int64)"
%typemap(ctype) Solver::IndexEvaluator3 "void*" // "int64 (*)(int64, int64, int64)"

%typemap(ctype) Solver::IndexFilter1 "void*" // "bool (*)(int64)"
%typemap(ctype) Solver::ObjectiveWatcher "void*" // "void (*)(int64)"

// Convert in module_csharp_wrap.cc input argument (delegate marshaled in C function pointer) to original std::function<...>
%typemap(in) Solver::IndexEvaluator1  %{
  $1 = [$input](int64 u) -> int64 {
    return (*(int64 (*)(int64))$input)(u);
    };
%}
%typemap(in) Solver::IndexEvaluator2  %{
  $1 = [$input](int64 u, int64 v) -> int64 {
    return (*(int64 (*)(int64, int64))$input)(u, v);};
%}
%typemap(in) Solver::IndexEvaluator3  %{
  $1 = [$input](int64 u, int64 v, int64 w) -> int64 {
    return (*(int64 (*)(int64, int64, int64))$input)(u, v, w);};
%}

%typemap(in) Solver::IndexFilter1  %{
  $1 = [$input](int64 u) -> bool {
    return (*(bool (*)(int64))$input)(u);};
%}
%typemap(in) Solver::ObjectiveWatcher %{
  $1 = [$input](int64 u) -> void {
    return (*(void (*)(int64))$input)(u);};
%}

%extend Solver {
  //LocalSearchOperator* ConcatenateOperators(
  //    const std::vector<LocalSearchOperator*>& ops,
  //    swig_util::IntIntToLong* evaluator) {
  //  return $self->ConcatenateOperators(ops, [evaluator](int i, int64 j) {
  //      return evaluator->Run(i, j); });
  //}
  //SearchMonitor* MakeSearchLog(
  //    int branch_count,
  //    OptimizeVar* const objective,
  //    swig_util::VoidToString* display_callback) {
  //  return $self->MakeSearchLog(branch_count, objective, [display_callback]() {
  //      return display_callback->Run();
  //    });
  //}
  //SearchMonitor* MakeSearchLog(
  //    int branch_count,
  //    IntVar* const obj_var,
  //    swig_util::VoidToString* display_callback) {
  //  return $self->MakeSearchLog(branch_count, obj_var, [display_callback]() {
  //      return display_callback->Run();
  //    });
  //}
  //SearchMonitor* MakeSearchLog(
  //    int branch_count,
  //    swig_util::VoidToString* display_callback) {
  //  return $self->MakeSearchLog(branch_count, [display_callback]() {
  //      return display_callback->Run();
  //    });
  //}
  //SearchLimit* MakeCustomLimit(swig_util::VoidToBoolean* limiter) {
  //  return $self->MakeCustomLimit([limiter]() { return limiter->Run(); });
  //}
  //Demon* MakeClosureDemon(swig_util::VoidToVoid* closure) {
  //  return $self->MakeClosureDemon([closure]() { return closure->Run(); });
  //}
}  // extend Solver

// No custom wrapping for this method, we simply ignore it.
%ignore SearchLog::SearchLog(
    Solver* const s, OptimizeVar* const obj, IntVar* const var,
    std::function<std::string()> display_callback, int period);

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

class LocalSearchPhaseParameters {
 public:
  LocalSearchPhaseParameters();
  ~LocalSearchPhaseParameters();
};

// Pack
// Keep reference to delegate to avoid GC to collect them early
%typemap(cscode) Pack %{
  // Store list of delegates to avoid the GC to reclaim them.
  private List<IndexEvaluator1> indexEvaluator1Callbacks;
  private List<IndexEvaluator2> indexEvaluator2Callbacks;
  // Ensure that the GC does not collect any IndexEvaluator1Callback set from C#
  // as the underlying C++ class will only store a pointer to it (i.e. no ownership).
  private IndexEvaluator1 StoreIndexEvaluator1(IndexEvaluator1 c) {
    if (indexEvaluator1Callbacks == null) indexEvaluator1Callbacks = new List<IndexEvaluator1>();
    indexEvaluator1Callbacks.Add(c);
    return c;
  }
  private IndexEvaluator2 StoreIndexEvaluator2(IndexEvaluator2 c) {
    if (indexEvaluator2Callbacks == null) indexEvaluator2Callbacks = new List<IndexEvaluator2>();
    indexEvaluator2Callbacks.Add(c);
    return c;
  }
%}

}  // namespace operations_research

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

// Wrap cp includes
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
