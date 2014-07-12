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
%include util/csharp/tuple_set.swig
%include "std_vector.i"

/* allow partial c# classes */
%typemap(csclassmodifiers) SWIGTYPE "public partial class"

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
%module(directors="1", allprotected="1") operations_research;
%feature("director") LongResultCallback1;
%feature("director") LongResultCallback2;
%feature("director") LongResultCallback3;
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

%{
#include <setjmp.h>

#include <vector>

#include "base/callback.h"
#include "base/integral_types.h"
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

%template(CpIntVector) std::vector<int>;
%template(CpInt64Vector) std::vector<int64>;

%define CS_TYPEMAP_STDVECTOR_OBJECT(CTYPE, TYPE)
SWIG_STD_VECTOR_ENHANCED(operations_research::CTYPE*);
%template(Cp ## TYPE ## Vector) std::vector<CTYPE*>;
%enddef  // CS_TYPEMAP_STDVECTOR_OBJECT

CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::IntVar, IntVar)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::SearchMonitor, SearchMonitor)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::DecisionBuilder, DecisionBuilder)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::IntervalVar, IntervalVar)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::SequenceVar, SequenceVar)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::LocalSearchOperator, LocalSearchOperator)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::LocalSearchFilter, LocalSearchFilter)
CS_TYPEMAP_STDVECTOR_OBJECT(operations_research::SymmetryBreaker, SymmetryBreaker)

%ignore operations_research::Solver::MakeIntVarArray;
%ignore operations_research::Solver::MakeBoolVarArray;
%ignore operations_research::Solver::MakeFixedDurationIntervalVarArray;
%ignore operations_research::IntVarLocalSearchFilter::FindIndex;

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

%extend IntVarLocalSearchFilter {
  int Index(IntVar* const var) {
    int64 index = -1;
    $self->FindIndex(var, &index);
    return index;
  }
}

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
}
