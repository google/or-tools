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

%include exception.i
%include util/csharp/data.swig
%include util/csharp/tuple_set.swig

// ############ BEGIN DUPLICATED CODE BLOCK ############
// IMPORTANT: keep this code block in sync with the .swig
// files in ../python and ../csharp.

// Protect from failure.
%define PROTECT_FROM_FAILURE(Method, GetSolver)
%exception Method {
  operations_research::Solver* const solver = GetSolver;
  FailureProtect protect;
  Closure* const intercept = NewCallback<>(&protect, &FailureProtect::JumpBack);
  solver->set_fail_intercept(intercept);
  if (setjmp(protect.exception_buffer) == 0) {
    $action
    solver->clear_fail_intercept();
    delete intercept;
  } else {
    solver->clear_fail_intercept();
    SWIG_CSharpSetPendingException(SWIG_CSharpApplicationException, "fail");
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

// Callback wrapping.
%{
#include <vector>
#include "base/callback.h"
#include "base/integral_types.h"
%}

// TODO(user): split out the callback code; it creates another file since
// it uses a different module.
%module(directors="1") main
%feature("director") LongResultCallback1;
%feature("director") LongResultCallback2;
%feature("director") LongResultCallback3;
%{
#include <vector>
#include "base/callback.h"
#include "base/integral_types.h"

class LongResultCallback1 {
 public:
  virtual int64 Run(int64) = 0;
  ResultCallback1<int64, int64>* GetPermanentCallback() {
    return NewPermanentCallback(this, &LongResultCallback1::Run);
  }
  virtual ~LongResultCallback1() {}
};
class LongResultCallback2 {
 public:
  virtual int64 Run(int64, int64) = 0;
  ResultCallback2<int64, int64, int64>* GetPermanentCallback() {
    return NewPermanentCallback(this, &LongResultCallback2::Run);
  }
  virtual ~LongResultCallback2() {}
};
class LongResultCallback3 {
 public:
  virtual int64 Run(int64, int64, int64) = 0;
  ResultCallback3<int64, int64, int64, int64>* GetPermanentCallback() {
    return NewPermanentCallback(this, &LongResultCallback3::Run);
  }
  virtual ~LongResultCallback3() {}
};
%}

class LongResultCallback1 {
 public:
  virtual int64 Run(int64) = 0;
  ResultCallback1<int64, int64>* GetPermanentCallback();
  virtual ~LongResultCallback1();
};
class LongResultCallback2 {
 public:
  virtual int64 Run(int64, int64) = 0;
  ResultCallback2<int64, int64, int64>* GetPermanentCallback();
  virtual ~LongResultCallback2();
};
class LongResultCallback3 {
 public:
  virtual int64 Run(int64, int64, int64) = 0;
  ResultCallback3<int64, int64, int64, int64>* GetPermanentCallback();
  virtual ~LongResultCallback3();
};

// Typemaps for callbacks in csharp.
%typemap(cstype) ResultCallback1<int64, int64>* "LongResultCallback1";
%typemap(csin) ResultCallback1<int64, int64>*
    "$descriptor(ResultCallback1<int64, int64>*)
     .getCPtr($csinput.GetPermanentCallback())";
%typemap(cstype) ResultCallback2<int64, int64, int64>* "LongResultCallback2";
%typemap(csin) ResultCallback2<int64, int64, int64>*
    "$descriptor(ResultCallback2<int64, int64, int64>*)
     .getCPtr($csinput.GetPermanentCallback())";
%typemap(cstype) ResultCallback3<int64, int64, int64, int64>*
    "LongResultCallback3";
%typemap(csin) ResultCallback3<int64, int64, int64, int64>*
    "$descriptor(ResultCallback3<int64, int64, int64, int64>*)
     .getCPtr($csinput.GetPermanentCallback())";

%module(directors="1", allprotected="1") operations_research;
%feature("director") Action;
%feature("director") BaseLNS;
%feature("director") Decision;
%feature("director") DecisionBuilder;
%feature("director") IntVarLocalSearchFilter;
%feature("director") IntVarLocalSearchOperator;
%feature("director") SequenceVarLocalSearchOperator;
%feature("director") LocalSearchOperator;
%feature("director") OptimizeVar;
%feature("director") SearchLimit;
%feature("director") SearchMonitor;
%feature("director") SymmetryBreaker;

%include "std_vector.i"

%template(IntVector) std::vector<int>;

%{
#include <setjmp.h>

#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"

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

%ignore operations_research::Solver::MakeIntVarArray;
%ignore operations_research::Solver::MakeBoolVarArray;
%ignore operations_research::Solver::MakeFixedDurationIntervalVarArray;
%ignore operations_research::IntVarLocalSearchFilter::FindIndex;
%ignore operations_research::VarLocalSearchOperator::Value;
%ignore operations_research::VarLocalSearchOperator::OldValue;
%ignore operations_research::SequenceVarLocalSearchOperator::Sequence;
%ignore operations_research::SequenceVarLocalSearchOperator::OldSequence;
%ignore operations_research::SolutionCollector::ForwardSequence;
%ignore operations_research::SolutionCollector::BackwardSequence;
%ignore operations_research::SolutionCollector::Unperformed;
%ignore operations_research::SequenceVarElement::ForwardSequence;
%ignore operations_research::SequenceVarElement::BackwardSequence;
%ignore operations_research::SequenceVarElement::Unperformed;
%ignore operations_research::Assignment::ForwardSequence;
%ignore operations_research::Assignment::BackwardSequence;
%ignore operations_research::Assignment::Unperformed;

// Generic rename rule.
%rename("%(camelcase)s", %$isfunction) "";

// Rename rule on DecisionBuilder;
%rename (NextWrap) operations_research::DecisionBuilder::Next;

// Rename rule on SearchLimit
%rename (IsCrossed) operations_research::SearchLimit::crossed;

// Rename rules on Solver.
%rename (Add) operations_research::Solver::AddConstraint;

// Rename rule on DisjunctiveConstraint.
%rename (SequenceVar) operations_research::DisjunctiveConstraint::MakeSequenceVar;

// Generic rename rules.
%rename (ToString) *::DebugString;

// Keep the .solver() API.
%rename (solver) *::solver;

// Rename NewSearch and EndSearch to add pinning.
%rename (NewSearchAux) operations_research::Solver::NewSearch;
%rename (EndSearchAux) operations_research::Solver::EndSearch;

%typemap(csinterfaces_derived) operations_research::Constraint "IConstraintWithStatus";
%typemap(cscode) operations_research::Constraint %{
  public static implicit operator IntVar(Constraint eq)
  {
    return eq.Var();
  }

  public static implicit operator IntExpr(Constraint eq)
  {
    return eq.Var();
  }
  public static IntExpr operator+(Constraint a, Constraint b) {
    return a.solver().MakeSum(a.Var(), b.Var());
  }
  public static IntExpr operator+(Constraint a, long v) {
    return a.solver().MakeSum(a.Var(), v);
  }
  public static IntExpr operator+(long v, Constraint a) {
    return a.solver().MakeSum(a.Var(), v);
  }
  public static IntExpr operator-(Constraint a, Constraint b) {
    return a.solver().MakeDifference(a.Var(), b.Var());
  }
  public static IntExpr operator-(Constraint a, long v) {
    return a.solver().MakeSum(a.Var(), -v);
  }
  public static IntExpr operator-(long v, Constraint a) {
    return a.solver().MakeDifference(v, a.Var());
  }
  public static IntExpr operator*(Constraint a, Constraint b) {
    return a.solver().MakeProd(a.Var(), b.Var());
  }
  public static IntExpr operator*(Constraint a, long v) {
    return a.solver().MakeProd(a.Var(), v);
  }
  public static IntExpr operator*(long v, Constraint a) {
    return a.solver().MakeProd(a.Var(), v);
  }
  public static IntExpr operator/(Constraint a, long v) {
    return a.solver().MakeDiv(a.Var(), v);
  }
  public static IntExpr operator-(Constraint a) {
    return a.solver().MakeOpposite(a.Var());
  }
  public IntExpr Abs() {
    return this.solver().MakeAbs(this.Var());
  }
  public IntExpr Square() {
    return this.solver().MakeSquare(this.Var());
  }
  public static WrappedConstraint operator ==(Constraint a, long v) {
    return new WrappedConstraint(a.solver().MakeEquality(a.Var(), v));
  }
  public static WrappedConstraint operator ==(long v, Constraint a) {
    return new WrappedConstraint(a.solver().MakeEquality(a.Var(), v));
  }
  public static WrappedConstraint operator !=(Constraint a, long v) {
    return new WrappedConstraint(a.solver().MakeNonEquality(a.Var(), v));
  }
  public static WrappedConstraint operator !=(long v, Constraint a) {
    return new WrappedConstraint(a.solver().MakeNonEquality(a.Var(), v));
  }
  public static WrappedConstraint operator >=(Constraint a, long v) {
    return new WrappedConstraint(a.solver().MakeGreaterOrEqual(a.Var(), v));
  }
  public static WrappedConstraint operator >=(long v, Constraint a) {
    return new WrappedConstraint(a.solver().MakeLessOrEqual(a.Var(), v));
  }
  public static WrappedConstraint operator >(Constraint a, long v) {
    return new WrappedConstraint(a.solver().MakeGreater(a.Var(), v));
  }
  public static WrappedConstraint operator >(long v, Constraint a) {
    return new WrappedConstraint(a.solver().MakeLess(a.Var(), v));
  }
  public static WrappedConstraint operator <=(Constraint a, long v) {
    return new WrappedConstraint(a.solver().MakeLessOrEqual(a.Var(), v));
  }
  public static WrappedConstraint operator <=(long v, Constraint a) {
    return new WrappedConstraint(a.solver().MakeGreaterOrEqual(a.Var(), v));
  }
  public static WrappedConstraint operator <(Constraint a, long v) {
    return new WrappedConstraint(a.solver().MakeLess(a.Var(), v));
  }
  public static WrappedConstraint operator <(long v, Constraint a) {
    return new WrappedConstraint(a.solver().MakeGreater(a.Var(), v));
  }
  public static WrappedConstraint operator >=(Constraint a, Constraint b) {
    return new WrappedConstraint(a.solver().MakeGreaterOrEqual(a.Var(), b.Var()));
  }
  public static WrappedConstraint operator >(Constraint a, Constraint b) {
    return new WrappedConstraint(a.solver().MakeGreater(a.Var(), b.Var()));
  }
  public static WrappedConstraint operator <=(Constraint a, Constraint b) {
    return new WrappedConstraint(a.solver().MakeLessOrEqual(a.Var(), b.Var()));
  }
  public static WrappedConstraint operator <(Constraint a, Constraint b) {
    return new WrappedConstraint(a.solver().MakeLess(a.Var(), b.Var()));
  }
  public static ConstraintEquality operator ==(Constraint a, Constraint b) {
    return new ConstraintEquality(a, b, true);
  }
  public static ConstraintEquality operator !=(Constraint a, Constraint b) {
    return new ConstraintEquality(a, b, false);
  }
%}


// Add arithmetic operators to integer expressions.
%typemap(cscode) operations_research::IntExpr %{
  public static IntExpr operator+(IntExpr a, IntExpr b) {
    return a.solver().MakeSum(a, b);
  }
  public static IntExpr operator+(IntExpr a, long v) {
    return a.solver().MakeSum(a, v);
  }
  public static IntExpr operator+(long v, IntExpr a) {
    return a.solver().MakeSum(a, v);
  }
  public static IntExpr operator-(IntExpr a, IntExpr b) {
    return a.solver().MakeDifference(a, b);
  }
  public static IntExpr operator-(IntExpr a, long v) {
    return a.solver().MakeSum(a, -v);
  }
  public static IntExpr operator-(long v, IntExpr a) {
    return a.solver().MakeDifference(v, a);
  }
  public static IntExpr operator*(IntExpr a, IntExpr b) {
    return a.solver().MakeProd(a, b);
  }
  public static IntExpr operator*(IntExpr a, long v) {
    return a.solver().MakeProd(a, v);
  }
  public static IntExpr operator*(long v, IntExpr a) {
    return a.solver().MakeProd(a, v);
  }
  public static IntExpr operator/(IntExpr a, long v) {
    return a.solver().MakeDiv(a, v);
  }
  public static IntExpr operator-(IntExpr a) {
    return a.solver().MakeOpposite(a);
  }
  public IntExpr Abs() {
    return this.solver().MakeAbs(this);
  }
  public IntExpr Square() {
    return this.solver().MakeSquare(this);
  }
  public static IntExprEquality operator ==(IntExpr a, IntExpr b) {
    return new IntExprEquality(a, b, true);
  }
  public static IntExprEquality operator !=(IntExpr a, IntExpr b) {
    return new IntExprEquality(a, b, false);
  }
  public static WrappedConstraint operator ==(IntExpr a, long v) {
    return new WrappedConstraint(a.solver().MakeEquality(a, v));
  }
  public static WrappedConstraint operator !=(IntExpr a, long v) {
    return new WrappedConstraint(a.solver().MakeNonEquality(a.Var(), v));
  }
  public static WrappedConstraint operator >=(IntExpr a, long v) {
    return new WrappedConstraint(a.solver().MakeGreaterOrEqual(a, v));
  }
  public static WrappedConstraint operator >(IntExpr a, long v) {
    return new WrappedConstraint(a.solver().MakeGreater(a, v));
  }
  public static WrappedConstraint operator <=(IntExpr a, long v) {
    return new WrappedConstraint(a.solver().MakeLessOrEqual(a, v));
  }
  public static WrappedConstraint operator <(IntExpr a, long v) {
    return new WrappedConstraint(a.solver().MakeLess(a, v));
  }
  public static WrappedConstraint operator >=(IntExpr a, IntExpr b) {
    return new WrappedConstraint(a.solver().MakeGreaterOrEqual(a.Var(), b.Var()));
  }
  public static WrappedConstraint operator >(IntExpr a, IntExpr b) {
    return new WrappedConstraint(a.solver().MakeGreater(a.Var(), b.Var()));
  }
  public static WrappedConstraint operator <=(IntExpr a, IntExpr b) {
    return new WrappedConstraint(a.solver().MakeLessOrEqual(a.Var(), b.Var()));
  }
  public static WrappedConstraint operator <(IntExpr a, IntExpr b) {
    return new WrappedConstraint(a.solver().MakeLess(a.Var(), b.Var()));
  }
%}

CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::IntVar, IntVar)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::SearchMonitor, SearchMonitor)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::DecisionBuilder, DecisionBuilder)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::IntervalVar, IntervalVar)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::SequenceVar, SequenceVar)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::LocalSearchOperator, LocalSearchOperator)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::LocalSearchFilter, LocalSearchFilter)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::SymmetryBreaker, SymmetryBreaker)

namespace operations_research {
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
    return $self->solver()->MakeIntervalVarRelation($self,
                                                    operations_research::Solver::STARTS_AFTER_START,
                                                    other);
  }
  Constraint* StartsAtEnd(IntervalVar* other) {
    return $self->solver()->MakeIntervalVarRelation($self, operations_research::Solver::STARTS_AT_END, other);
  }
  Constraint* StartsAtStart(IntervalVar* other) {
    return $self->solver()->MakeIntervalVarRelation($self,
                                                    operations_research::Solver::STARTS_AT_START,
                                                    other);
  }
  Constraint* EndsAfter(int64 date) {
    return $self->solver()->MakeIntervalVarRelation($self,
                                                    operations_research::Solver::ENDS_AFTER,
                                                    date);
  }
  Constraint* EndsAt(int64 date) {
    return $self->solver()->MakeIntervalVarRelation($self,
                                                    operations_research::Solver::ENDS_AT,
                                                    date);
  }
  Constraint* EndsBefore(int64 date) {
    return $self->solver()->MakeIntervalVarRelation($self,
                                                    operations_research::Solver::ENDS_BEFORE,
                                                    date);
  }
  Constraint* StartsAfter(int64 date) {
    return $self->solver()->MakeIntervalVarRelation($self,
                                                    operations_research::Solver::STARTS_AFTER,
                                                    date);
  }
  Constraint* StartsAt(int64 date) {
    return $self->solver()->MakeIntervalVarRelation($self,
                                                    operations_research::Solver::STARTS_AT,
                                                    date);
  }
  Constraint* StartsBefore(int64 date) {
    return $self->solver()->MakeIntervalVarRelation($self,
                                                    operations_research::Solver::STARTS_BEFORE,
                                                    date);
  }
  Constraint* CrossesDate(int64 date) {
    return $self->solver()->MakeIntervalVarRelation($self,
                                                    operations_research::Solver::CROSS_DATE,
                                                    date);
  }
  Constraint* AvoidsDate(int64 date) {
    return $self->solver()->MakeIntervalVarRelation($self,
                                                    operations_research::Solver::AVOID_DATE,
                                                    date);
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

%typemap(cscode) Solver %{
  public IntVar[] MakeIntVarArray(int count, long min, long max) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeIntVar(min, max);
    }
    return array;
  }

  public IntVar[] MakeIntVarArray(int count, long min, long max, string name) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      string var_name = name + i;
      array[i] = MakeIntVar(min, max, var_name);
    }
    return array;
  }

  public IntVar[] MakeIntVarArray(int count, long[] values) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeIntVar(values);
    }
    return array;
  }

  public IntVar[] MakeIntVarArray(int count, long[] values, string name) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      string var_name = name + i;
      array[i] = MakeIntVar(values, var_name);
    }
    return array;
  }

  public IntVar[] MakeIntVarArray(int count, int[] values) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeIntVar(values);
    }
    return array;
  }

  public IntVar[] MakeIntVarArray(int count, int[] values, string name) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      string var_name = name + i;
      array[i] = MakeIntVar(values, var_name);
    }
    return array;
  }

  public IntVar[] MakeBoolVarArray(int count) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeBoolVar();
    }
    return array;
  }

  public IntVar[] MakeBoolVarArray(int count, string name) {
    IntVar[] array = new IntVar[count];
    for (int i = 0; i < count; ++i) {
      string var_name = name + i;
      array[i] = MakeBoolVar(var_name);
    }
    return array;
  }

  public IntVar[,] MakeIntVarMatrix(int rows, int cols, long min, long max) {
    IntVar[,] array = new IntVar[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        array[i,j] = MakeIntVar(min, max);
      }
    }
    return array;
  }

  public IntVar[,] MakeIntVarMatrix(int rows, int cols,
                                    long min, long max, string name) {
    IntVar[,] array = new IntVar[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        string var_name = name + "["+ i + ", " + j +"]";
        array[i,j] = MakeIntVar(min, max, var_name);
      }
    }
    return array;
  }

  public IntVar[,] MakeIntVarMatrix(int rows, int cols, long[] values) {
    IntVar[,] array = new IntVar[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        array[i,j] = MakeIntVar(values);
      }
    }
    return array;
  }

  public IntVar[,] MakeIntVarMatrix(int rows, int cols,
                                    long[] values, string name) {
    IntVar[,] array = new IntVar[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        string var_name = name + "["+ i + ", " + j +"]";
        array[i,j] = MakeIntVar(values, var_name);
      }
    }
    return array;
  }

  public IntVar[,] MakeIntVarMatrix(int rows, int cols, int[] values) {
    IntVar[,] array = new IntVar[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        array[i,j] = MakeIntVar(values);
      }
    }
    return array;
  }

  public IntVar[,] MakeIntVarMatrix(int rows, int cols,
                                    int[] values, string name) {
    IntVar[,] array = new IntVar[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        string var_name = name + "["+ i + ", " + j +"]";
        array[i,j] = MakeIntVar(values, var_name);
      }
    }
    return array;
  }

  public IntVar[,] MakeBoolVarMatrix(int rows, int cols) {
    IntVar[,] array = new IntVar[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        array[i,j] = MakeBoolVar();
      }
    }
    return array;
  }

  public IntVar[,] MakeBoolVarMatrix(int rows, int cols, string name) {
    IntVar[,] array = new IntVar[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        string var_name = name + "["+ i + ", " + j +"]";
        array[i,j] = MakeBoolVar(var_name);
      }
    }
    return array;
  }

  public IntervalVar[] MakeFixedDurationIntervalVarArray(int count,
                                                         long start_min,
                                                         long start_max,
                                                         long duration,
                                                         bool optional) {
    IntervalVar[] array = new IntervalVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeFixedDurationIntervalVar(start_min,
                                              start_max,
                                              duration,
                                              optional,
                                              "");
    }
    return array;
  }

  public IntervalVar[] MakeFixedDurationIntervalVarArray(int count,
                                                         long start_min,
                                                         long start_max,
                                                         long duration,
                                                         bool optional,
                                                         string name) {
    IntervalVar[] array = new IntervalVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeFixedDurationIntervalVar(start_min,
                                              start_max,
                                              duration,
                                              optional,
                                              name + i);
    }
    return array;
  }

  public IntervalVar[] MakeFixedDurationIntervalVarArray(int count,
                                                         long[] start_min,
                                                         long[] start_max,
                                                         long[] duration,
                                                         bool optional,
                                                         string name) {
    IntervalVar[] array = new IntervalVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeFixedDurationIntervalVar(start_min[i],
                                              start_max[i],
                                              duration[i],
                                              optional,
                                              name + i);
    }
    return array;
  }

  public IntervalVar[] MakeFixedDurationIntervalVarArray(int count,
                                                         int[] start_min,
                                                         int[] start_max,
                                                         int[] duration,
                                                         bool optional,
                                                         string name) {
    IntervalVar[] array = new IntervalVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeFixedDurationIntervalVar(start_min[i],
                                              start_max[i],
                                              duration[i],
                                              optional,
                                              name + i);
    }
    return array;
  }
  public IntervalVar[] MakeFixedDurationIntervalVarArray(IntVar[] starts,
                                                         int[] durations,
                                                         string name) {
    int count = starts.Length;
    IntervalVar[] array = new IntervalVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeFixedDurationIntervalVar(starts[i],
                                              durations[i],
                                              name + i);
    }
    return array;
  }
  public IntervalVar[] MakeFixedDurationIntervalVarArray(IntVar[] starts,
                                                         long[] durations,
                                                         string name) {
    int count = starts.Length;
    IntervalVar[] array = new IntervalVar[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeFixedDurationIntervalVar(starts[i],
                                              durations[i],
                                              name + i);
    }
    return array;
  }
  public void NewSearch(DecisionBuilder db) {
    pinned_decision_builder_ = db;
    pinned_search_monitors_.Clear();
    NewSearchAux(db);
  }

  public void NewSearch(DecisionBuilder db, SearchMonitor sm1) {
    pinned_decision_builder_ = db;
    pinned_search_monitors_.Clear();
    pinned_search_monitors_.Add(sm1);
    NewSearchAux(db, sm1);
  }


  public void NewSearch(DecisionBuilder db,
                        SearchMonitor sm1,
                        SearchMonitor sm2) {
    pinned_decision_builder_ = db;
    pinned_search_monitors_.Clear();
    pinned_search_monitors_.Add(sm1);
    pinned_search_monitors_.Add(sm2);
    NewSearchAux(db, sm1, sm2);
  }

  public void NewSearch(DecisionBuilder db,
                        SearchMonitor sm1,
                        SearchMonitor sm2,
                        SearchMonitor sm3) {
    pinned_decision_builder_ = db;
    pinned_search_monitors_.Clear();
    pinned_search_monitors_.Add(sm1);
    pinned_search_monitors_.Add(sm2);
    pinned_search_monitors_.Add(sm3);
    NewSearchAux(db, sm1, sm2, sm3);
  }

  public void NewSearch(DecisionBuilder db,
                        SearchMonitor sm1,
                        SearchMonitor sm2,
                        SearchMonitor sm3,
                        SearchMonitor sm4) {
    pinned_decision_builder_ = db;
    pinned_search_monitors_.Clear();
    pinned_search_monitors_.Add(sm1);
    pinned_search_monitors_.Add(sm2);
    pinned_search_monitors_.Add(sm3);
    pinned_search_monitors_.Add(sm4);
    NewSearchAux(db, sm1, sm2, sm3, sm4);
  }

  public void NewSearch(DecisionBuilder db, SearchMonitor[] monitors) {
    pinned_decision_builder_ = db;
    pinned_search_monitors_.Clear();
    pinned_search_monitors_.AddRange(monitors);
    NewSearchAux(db, monitors);
  }

  public void EndSearch() {
    pinned_decision_builder_ = null;
    pinned_search_monitors_.Clear();
    EndSearchAux();
  }

  private System.Collections.Generic.List<SearchMonitor> pinned_search_monitors_
      = new System.Collections.Generic.List<SearchMonitor>();
  private DecisionBuilder pinned_decision_builder_;
%}

%extend Assignment {
  int ForwardSequenceSize(const SequenceVar* const v) const {
    return $self->ForwardSequence(v).size();
  }
  int ForwardSequenceValueAt(const SequenceVar* const v, int i) const {
    return $self->ForwardSequence(v)[i];
  }
  int BackwardSequenceSize(const SequenceVar* const v) const {
    return $self->BackwardSequence(v).size();
  }
  int BackwardSequenceValueAt(const SequenceVar* const v, int position) const {
    return $self->BackwardSequence(v)[position];
  }
  int UnperformedSize(const SequenceVar* const v) const {
    return $self->Unperformed(v).size();
  }
  int UnperformedValueAt(const SequenceVar* const v, int position) const {
    return $self->Unperformed(v)[position];
  }
}

%typemap(cscode) Assignment %{
  public int[] ForwardSequence(SequenceVar v) {
    int size = ForwardSequenceSize(v);
    int[] result = new int[size];
    for (int i = 0; i < size; ++i) result[i] = ForwardSequenceValueAt(v, i);
    return result;
  }
  public int[] BackwardSequence(SequenceVar v) {
    int size = BackwardSequenceSize(v);
    int[] result = new int[size];
    for (int i = 0; i < size; ++i) result[i] = BackwardSequenceValueAt(v, i);
    return result;
  }
  public int[] Unperformed(SequenceVar v) {
    int size = UnperformedSize(v);
    int[] result = new int[size];
    for (int i = 0; i < size; ++i) result[i] = UnperformedValueAt(v, i);
    return result;
  }
%}

%extend SequenceVarLocalSearchOperator {
  int SequenceSize(int index) const {
    return $self->Sequence(index).size();
  }
  int SequenceValueAt(int index, int position) const {
    return $self->Sequence(index)[position];
  }
  int OldSequenceSize(int index) const {
    return $self->OldSequence(index).size();
  }
  int OldSequenceValueAt(int index, int position) const {
    return $self->OldSequence(index)[position];
  }
}

%typemap(cscode) SequenceVarLocalSearchOperator %{
  public int[] Sequence(int index) {
    int size = SequenceSize(index);
    int[] result = new int[size];
    for (int position = 0; position < size; ++position) {
      result[position] = SequenceValueAt(index, position);
    }
    return result;
  }
  public int[] OldSequence(int index) {
    int size = OldSequenceSize(index);
    int[] result = new int[size];
    for (int position = 0; position < size; ++position) {
      result[position] = OldSequenceValueAt(index, position);
    }
    return result;
  }
%}

%extend IntVarLocalSearchFilter {
  int Index(IntVar* const var) {
    int64 index = -1;
    $self->FindIndex(var, &index);
    return index;
  }
}
}  // namespace operations_research

namespace operations_research {
class LocalSearchPhaseParameters {
 public:
  LocalSearchPhaseParameters();
  ~LocalSearchPhaseParameters();
};
}  // namespace operations_research

// Wrap cp includes
%include constraint_solver/constraint_solver.h
%include constraint_solver/constraint_solveri.h

namespace operations_research {
%template(RevInteger) Rev<int64>;
%template(RevBool) Rev<bool>;
typedef Assignment::AssignmentContainer AssignmentContainer;
%template(AssignmentIntContainer) AssignmentContainer<IntVar, IntVarElement>;

// We can't use VarLocalSearchOperator::[Old]Value() because it's problematic
// when instantiated on the SequenceVarLocalSearchOperator subclass (because
// their return type is a const std::vector<int>&). So we ignore them and
// redefine them here, only for the base class of IntVarLocalSearchOperator.
%extend VarLocalSearchOperator<operations_research::IntVar, int64,
                               operations_research::IntVarLocalSearchHandler> {
  int64 Value(int64 i) const { return $self->Value(i); }
  int64 OldValue(int64 i) const { return $self->OldValue(i); }
}
}
