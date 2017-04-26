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

%typemap(csimports) SWIGTYPE %{
using System;
using System.Runtime.InteropServices;
using System.Collections;
%}

%include "exception.i"
%include "std_vector.i"
%include "ortools/util/csharp/tuple_set.i"
%include "ortools/util/csharp/functions.i"
%include "ortools/util/csharp/proto.i"

// We need to forward-declare the proto here, so that PROTO_INPUT involving it
// works correctly. The order matters very much: this declaration needs to be
// before the %{ #include ".../constraint_solver.h" %}.
namespace operations_research {
class ConstraintSolverParameters;
class SearchLimitParameters;
}  // namespace operations_research

%module(directors="1", allprotected="1") operations_research;
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
%feature("director") SymmetryBreaker;
%{
#include <setjmp.h>

#include <string>
#include <vector>

#include "ortools/base/callback.h"
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

%ignore operations_research::Solver::MakeIntVarArray;
%ignore operations_research::Solver::MakeBoolVarArray;
%ignore operations_research::Solver::MakeFixedDurationIntervalVarArray;
%ignore operations_research::IntVarLocalSearchFilter::FindIndex;
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

// Rename rules on Solver.
%rename (Add) operations_research::Solver::AddConstraint;

// Rename rule on DisjunctiveConstraint.
%rename (SequenceVar) operations_research::DisjunctiveConstraint::MakeSequenceVar;

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
%feature("director") operations_research::IntVarLocalSearchFilter;
%unignore operations_research::LocalSearchFilter::Accept;
%unignore operations_research::LocalSearchFilter::Synchronize;
%unignore operations_research::LocalSearchFilter::IsIncremental;

// IntVarLocalSearchFilter
// Ignored:
// - IsVarSynced()
%feature("director") operations_research::IntVarLocalSearchFilter;
%feature("nodirector") operations_research::IntVarLocalSearchFilter::Synchronize;  // Inherited.
%ignore operations_research::IntVarLocalSearchFilter::FindIndex;
%unignore operations_research::IntVarLocalSearchFilter::AddVars;  // Inherited.
%unignore operations_research::IntVarLocalSearchFilter::IsIncremental;
%unignore operations_research::IntVarLocalSearchFilter::OnSynchronize;
%unignore operations_research::IntVarLocalSearchFilter::Size;
%unignore operations_research::IntVarLocalSearchFilter::Start;
%unignore operations_research::IntVarLocalSearchFilter::Value;
%unignore operations_research::IntVarLocalSearchFilter::Var;  // Inherited.

// Rename NewSearch and EndSearch to add pinning. See the overrides of
// NewSearch in ../../open_source/csharp/constraint_solver/SolverHelper.cs
%rename (NewSearchAux) operations_research::Solver::NewSearch;
%rename (EndSearchAux) operations_research::Solver::EndSearch;

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

namespace operations_research {
// Take care of API with function.  SWIG doesn't wrap std::function<>
// properly, so we write our custom wrappers for all methods involving
// std::function<>.
%ignore Solver::MakeSearchLog(
    int branch_count,
    std::function<std::string()> display_callback);
%ignore Solver::MakeSearchLog(
    int branch_count,
    IntVar* objective,
    std::function<std::string()> display_callback);
%ignore Solver::MakeSearchLog(
    int branch_count,
    OptimizeVar* const objective,
    std::function<std::string()> display_callback);

%ignore Solver::MakeActionDemon;
%ignore Solver::MakeCustomLimit(std::function<bool()> limiter);
%ignore Solver::MakeElement(IndexEvaluator1 values, IntVar* const index);
%ignore Solver::MakeMonotonicElement(IndexEvaluator1 values, bool increasing,
                                     IntVar* const index);
%ignore Solver::MakeElement(IndexEvaluator2 values, IntVar* const index1,
                            IntVar* const index2);
%ignore Solver::MakePathCumul(const std::vector<IntVar*>& nexts,
                              const std::vector<IntVar*>& active,
                              const std::vector<IntVar*>& cumuls,
                              IndexEvaluator2 transit_evaluator);
%ignore Solver::MakePathCumul(const std::vector<IntVar*>& nexts,
                              const std::vector<IntVar*>& active,
                              const std::vector<IntVar*>& cumuls,
                              const std::vector<IntVar*>& slacks,
                              IndexEvaluator2 transit_evaluator);
%ignore Solver::MakeNoCycle(const std::vector<IntVar*>& nexts,
                            const std::vector<IntVar*>& active,
                            IndexFilter1 sink_handler = nullptr);
%ignore Solver::MakeNoCycle(const std::vector<IntVar*>& nexts,
                            const std::vector<IntVar*>& active,
                            IndexFilter1 sink_handler, bool assume_paths);
%ignore Solver::MakeGuidedLocalSearch(bool maximize, IntVar* const objective,
                                      IndexEvaluator2 objective_function,
                                      int64 step, const std::vector<IntVar*>& vars,
                                      double penalty_factor);
%ignore Solver::MakeGuidedLocalSearch(bool maximize, IntVar* const objective,
                                      IndexEvaluator3 objective_function,
                                      int64 step, const std::vector<IntVar*>& vars,
                                      const std::vector<IntVar*>& secondary_vars,
                                      double penalty_factor);
%ignore Solver::MakePhase(const std::vector<IntVar*>& vars,
                          IndexEvaluator1 var_evaluator,
                          IntValueStrategy val_str);

%ignore Solver::MakePhase(const std::vector<IntVar*>& vars,
                          IntVarStrategy var_str, IndexEvaluator2 val_eval);

%ignore Solver::MakePhase(
    const std::vector<IntVar*>& vars, IntVarStrategy var_str,
    VariableValueComparator var_val1_val2_comparator);

%ignore Solver::MakePhase(const std::vector<IntVar*>& vars,
                          IndexEvaluator1 var_evaluator,
                          IndexEvaluator2 val_eval);

%ignore Solver::MakePhase(const std::vector<IntVar*>& vars,
                          IntVarStrategy var_str, IndexEvaluator2 val_eval,
                          IndexEvaluator1 tie_breaker);

%ignore Solver::MakePhase(const std::vector<IntVar*>& vars,
                          IndexEvaluator1 var_evaluator,
                          IndexEvaluator2 val_eval,
                          IndexEvaluator1 tie_breaker);
%ignore Solver::MakePhase(const std::vector<IntVar*>& vars,
                          IndexEvaluator2 evaluator, EvaluatorStrategy str);
%ignore Solver::MakePhase(const std::vector<IntVar*>& vars,
                          IndexEvaluator2 evaluator,
                          IndexEvaluator1 tie_breaker,
                          EvaluatorStrategy str);
%ignore Solver::MakeOperator(const std::vector<IntVar*>& vars,
                             IndexEvaluator3 evaluator,
                             EvaluatorLocalSearchOperators op);
%ignore Solver::MakeOperator(const std::vector<IntVar*>& vars,
                             const std::vector<IntVar*>& secondary_vars,
                             IndexEvaluator3 evaluator,
                             EvaluatorLocalSearchOperators op);
%ignore Solver::MakeLocalSearchObjectiveFilter(
    const std::vector<IntVar*>& vars, IndexEvaluator2 values,
    IntVar* const objective, Solver::LocalSearchFilterBound filter_enum,
    Solver::LocalSearchOperation op_enum);
%ignore Solver::MakeLocalSearchObjectiveFilter(
    const std::vector<IntVar*>& vars, IndexEvaluator2 values,
    ObjectiveWatcher delta_objective_callback, IntVar* const objective,
    Solver::LocalSearchFilterBound filter_enum,
    Solver::LocalSearchOperation op_enum);
%ignore Solver::MakeLocalSearchObjectiveFilter(
    const std::vector<IntVar*>& vars, const std::vector<IntVar*>& secondary_vars,
    Solver::IndexEvaluator3 values, IntVar* const objective,
    Solver::LocalSearchFilterBound filter_enum,
    Solver::LocalSearchOperation op_enum);
%ignore Solver::MakeLocalSearchObjectiveFilter(
    const std::vector<IntVar*>& vars, const std::vector<IntVar*>& secondary_vars,
    Solver::IndexEvaluator3 values, ObjectiveWatcher delta_objective_callback,
    IntVar* const objective, Solver::LocalSearchFilterBound filter_enum,
    Solver::LocalSearchOperation op_enum);
%ignore Solver::ConcatenateOperators(
    const std::vector<LocalSearchOperator*>& ops,
    std::function<int64(int, int)> evaluator);
%ignore Solver::MakeClosureDemon(std::function<void()> closure);
%ignore Solver::set_fail_intercept;

%extend Solver {
  IntExpr* MakeElement(swig_util::LongToLong* values, IntVar* const index) {
    return $self->MakeElement(
        [values](int64 i) { return values->Run(i); }, index);
  }
  IntExpr* MakeMonotonicElement(swig_util::LongToLong* values, bool increasing,
                                IntVar* const index) {
    return $self->MakeMonotonicElement(
        [values](int64 i) { return values->Run(i); }, increasing, index);
  }
  IntExpr* MakeElement(swig_util::LongLongToLong* values, IntVar* const index1,
                       IntVar* const index2) {
    return $self->MakeElement(
        [values](int64 i, int64 j) { return values->Run(i, j); }, index1,
        index2);
  }
  Constraint* MakePathCumul(const std::vector<IntVar*>& nexts,
                            const std::vector<IntVar*>& active,
                            const std::vector<IntVar*>& cumuls,
                            swig_util::LongLongToLong* transit_evaluator) {
    return $self->MakePathCumul(
        nexts, active, cumuls,
        [transit_evaluator](int64 i, int64 j) {
          return transit_evaluator->Run(i, j); });
  }
  Constraint* MakePathCumul(const std::vector<IntVar*>& nexts,
                            const std::vector<IntVar*>& active,
                            const std::vector<IntVar*>& cumuls,
                            const std::vector<IntVar*>& slacks,
                            swig_util::LongLongToLong* transit_evaluator) {
    return $self->MakePathCumul(nexts, active, cumuls, slacks,
        [transit_evaluator](int64 i, int64 j) {
          return transit_evaluator->Run(i, j); });
  }
  Constraint* MakeNoCycle(const std::vector<IntVar*>& nexts,
                          const std::vector<IntVar*>& active,
                          swig_util::LongToBoolean* sink_handler = nullptr) {
    if (sink_handler == nullptr) {
      return $self->MakeNoCycle(nexts, active, nullptr);
    } else {
      return $self->MakeNoCycle(nexts, active,
                                [sink_handler](int64 i) {
                                  return sink_handler->Run(i); });
    }
  }
  Constraint* MakeNoCycle(const std::vector<IntVar*>& nexts,
                          const std::vector<IntVar*>& active,
                          swig_util::LongToBoolean* sink_handler, bool assume_paths) {
    return $self->MakeNoCycle(nexts, active,
                              [sink_handler](int64 i) {
                                return sink_handler->Run(i); });
  }
  SearchMonitor* MakeGuidedLocalSearch(bool maximize, IntVar* const objective,
                                       swig_util::LongLongToLong* objective_function,
                                       int64 step, const std::vector<IntVar*>& vars,
                                       double penalty_factor) {
    return $self->MakeGuidedLocalSearch(
        maximize, objective,
        [objective_function](int64 i, int64 j) {
            return objective_function->Run(i, j); },
        step, vars, penalty_factor);
  }
  SearchMonitor* MakeGuidedLocalSearch(bool maximize, IntVar* const objective,
                                       swig_util::LongLongLongToLong* objective_function,
                                       int64 step, const std::vector<IntVar*>& vars,
                                       const std::vector<IntVar*>& secondary_vars,
                                       double penalty_factor) {
    return $self->MakeGuidedLocalSearch(
        maximize, objective,
        [objective_function](int64 i, int64 j, int64 k) {
          return objective_function->Run(i, j, k); },
        step, vars, secondary_vars, penalty_factor);
  }
  DecisionBuilder* MakePhase(const std::vector<IntVar*>& vars,
                             swig_util::LongToLong* var_evaluator,
                             IntValueStrategy val_str) {
    return $self->MakePhase(vars, [var_evaluator](int64 i) {
        return var_evaluator->Run(i); }, val_str);
  }
  DecisionBuilder* MakePhase(const std::vector<IntVar*>& vars,
                             IntVarStrategy var_str,
                             swig_util::LongLongToLong* val_eval) {
    operations_research::Solver::IndexEvaluator2 func =
         [val_eval](int64 i, int64 j) { return val_eval->Run(i, j); };
    return $self->MakePhase(vars, var_str, func);
  }
  DecisionBuilder* MakePhase(
      const std::vector<IntVar*>& vars, IntVarStrategy var_str,
      swig_util::LongLongLongToBoolean* var_val1_val2_comparator) {
    operations_research::Solver::VariableValueComparator comp =
        [var_val1_val2_comparator](int64 i, int64 j, int64 k) {
      return var_val1_val2_comparator->Run(i, j, k); };
    return $self->MakePhase(vars, var_str, comp);
  }
  DecisionBuilder* MakePhase(const std::vector<IntVar*>& vars,
                             swig_util::LongToLong* var_evaluator,
                             swig_util::LongLongToLong* val_eval) {
    return $self->MakePhase(vars, [var_evaluator](int64 i) { return var_evaluator->Run(i); }, [val_eval](int64 i, int64 j) { return val_eval->Run(i, j); });
  }
  DecisionBuilder* MakePhase(const std::vector<IntVar*>& vars,
                             IntVarStrategy var_str,
                             swig_util::LongLongToLong* val_eval,
                             swig_util::LongToLong* tie_breaker) {
    return $self->MakePhase(
        vars, var_str,
        [val_eval](int64 i, int64 j) { return val_eval->Run(i, j); },
        [tie_breaker](int64 i) { return tie_breaker->Run(i); });
  }
  DecisionBuilder* MakePhase(const std::vector<IntVar*>& vars,
                             swig_util::LongToLong* var_evaluator,
                             swig_util::LongLongToLong* val_eval,
                             swig_util::LongToLong* tie_breaker) {
    return $self->MakePhase(
        vars,
        [var_evaluator](int64 i) { return var_evaluator->Run(i); },
        [val_eval](int64 i, int64 j) { return val_eval->Run(i, j); },
        [tie_breaker](int64 i) { return tie_breaker->Run(i); });
  }
  DecisionBuilder* MakePhase(const std::vector<IntVar*>& vars,
                             swig_util::LongLongToLong* evaluator,
                             EvaluatorStrategy str) {
    return $self->MakePhase(
        vars,
        [evaluator](int64 i, int64 j) { return evaluator->Run(i, j); },
        str);
  }
  DecisionBuilder* MakePhase(const std::vector<IntVar*>& vars,
                             swig_util::LongLongToLong* evaluator,
                             swig_util::LongToLong* tie_breaker,
                             EvaluatorStrategy str) {
    return $self->MakePhase(
        vars,
        [evaluator](int64 i, int64 j) { return evaluator->Run(i, j); },
        [tie_breaker](int64 i) { return tie_breaker->Run(i); }, str);
  }
  LocalSearchOperator* MakeOperator(const std::vector<IntVar*>& vars,
                                    swig_util::LongLongLongToLong* evaluator,
                                    EvaluatorLocalSearchOperators op) {
    return $self->MakeOperator(
        vars,
        [evaluator](int64 i, int64 j, int64 k) {
          return evaluator->Run(i, j, k); }, op);
  }
  LocalSearchOperator* MakeOperator(const std::vector<IntVar*>& vars,
                                    const std::vector<IntVar*>& secondary_vars,
                                    swig_util::LongLongLongToLong* evaluator,
                                    EvaluatorLocalSearchOperators op) {
    return $self->MakeOperator(
        vars, secondary_vars,
        [evaluator](int64 i, int64 j, int64 k) {
          return evaluator->Run(i, j, k); }, op);
  }
  LocalSearchFilter* MakeLocalSearchObjectiveFilter(
      const std::vector<IntVar*>& vars, swig_util::LongLongToLong* values,
      IntVar* const objective, Solver::LocalSearchFilterBound filter_enum,
      Solver::LocalSearchOperation op_enum) {
    return $self->MakeLocalSearchObjectiveFilter(
        vars, [values](int64 i, int64 j) { return values->Run(i, j); },
        objective, filter_enum, op_enum);
  }
  LocalSearchFilter* MakeLocalSearchObjectiveFilter(
      const std::vector<IntVar*>& vars, swig_util::LongLongToLong* values,
      swig_util::LongToVoid* delta_objective_callback, IntVar* const objective,
      Solver::LocalSearchFilterBound filter_enum,
      Solver::LocalSearchOperation op_enum) {
    return $self->MakeLocalSearchObjectiveFilter(
        vars, [values](int64 i, int64 j) { return values->Run(i, j); },
        [delta_objective_callback](int64 i) {
          return delta_objective_callback->Run(i); },
        objective, filter_enum, op_enum);
  }
  LocalSearchFilter* MakeLocalSearchObjectiveFilter(
      const std::vector<IntVar*>& vars, const std::vector<IntVar*>& secondary_vars,
      swig_util::LongLongLongToLong* values, IntVar* const objective,
      Solver::LocalSearchFilterBound filter_enum,
      Solver::LocalSearchOperation op_enum) {
    return $self->MakeLocalSearchObjectiveFilter(
        vars, secondary_vars,
        [values](int64 i, int64 j, int64 k) { return values->Run(i, j, k); },
        objective, filter_enum, op_enum);
  }
  LocalSearchFilter* MakeLocalSearchObjectiveFilter(
      const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& secondary_vars,
      swig_util::LongLongLongToLong* values,
      swig_util::LongToVoid* delta_objective_callback,
      IntVar* const objective, Solver::LocalSearchFilterBound filter_enum,
      Solver::LocalSearchOperation op_enum) {
    return $self->MakeLocalSearchObjectiveFilter(
        vars, secondary_vars,
        [values](int64 i, int64 j, int64 k) { return values->Run(i, j, k); },
        [delta_objective_callback](int64 i) {
          return delta_objective_callback->Run(i); },
        objective, filter_enum, op_enum);
  }
  LocalSearchOperator* ConcatenateOperators(
      const std::vector<LocalSearchOperator*>& ops,
      swig_util::IntIntToLong* evaluator) {
    return $self->ConcatenateOperators(ops, [evaluator](int i, int64 j) {
        return evaluator->Run(i, j); });
  }
  SearchMonitor* MakeSearchLog(
      int branch_count,
      OptimizeVar* const objective,
      swig_util::VoidToString* display_callback) {
    return $self->MakeSearchLog(branch_count, objective, [display_callback]() {
        return display_callback->Run();
      });
  }
  SearchMonitor* MakeSearchLog(
      int branch_count,
      IntVar* const obj_var,
      swig_util::VoidToString* display_callback) {
    return $self->MakeSearchLog(branch_count, obj_var, [display_callback]() {
        return display_callback->Run();
      });
  }
  SearchMonitor* MakeSearchLog(
      int branch_count,
      swig_util::VoidToString* display_callback) {
    return $self->MakeSearchLog(branch_count, [display_callback]() {
        return display_callback->Run();
      });
  }
  SearchLimit* MakeCustomLimit(swig_util::VoidToBoolean* limiter) {
    return $self->MakeCustomLimit([limiter]() { return limiter->Run(); });
  }
  Demon* MakeClosureDemon(swig_util::VoidToVoid* closure) {
    return $self->MakeClosureDemon([closure]() { return closure->Run(); });
  }
}  // extend Solver

%ignore DisjunctiveConstraint::SetTransitionTime(Solver::IndexEvaluator2 transit_evaluator);
%extend DisjunctiveConstraint {
  void SetTransitionTime(swig_util::LongLongToLong* transit_evaluator) {
    $self->SetTransitionTime([transit_evaluator](int64 i, int64 j) { return transit_evaluator->Run(i, j); });
  }
}  // extend DisjunctiveConstraint

%ignore Pack::AddWeightedSumLessOrEqualConstantDimension(
      Solver::IndexEvaluator1 weights, const std::vector<int64>& bounds);
%ignore Pack::AddWeightedSumLessOrEqualConstantDimension(
    Solver::IndexEvaluator2 weights, const std::vector<int64>& bounds);
%ignore Pack::AddWeightedSumEqualVarDimension(Solver::IndexEvaluator2 weights,
                                              const std::vector<IntVar*>& loads);
%extend Pack {
void AddWeightedSumLessOrEqualConstantDimension(
    swig_util::LongToLong* weights, const std::vector<int64>& bounds) {
  operations_research::Solver::IndexEvaluator1 eval = [weights](int64 i) {
    return weights->Run(i);
  };
  return $self->AddWeightedSumLessOrEqualConstantDimension(eval, bounds);
}

void AddWeightedSumLessOrEqualConstantDimension(
    swig_util::LongLongToLong* weights, const std::vector<int64>& bounds) {
  operations_research::Solver::IndexEvaluator2 eval =
      [weights](int64 i, int64 j) { return weights->Run(i, j); };
  return $self->AddWeightedSumLessOrEqualConstantDimension(eval, bounds);
}
void AddWeightedSumEqualVarDimension(
    swig_util::LongLongToLong* weights, const std::vector<IntVar*>& loads) {
  return $self->AddWeightedSumEqualVarDimension([weights](int64 i, int64 j) { return weights->Run(i, j); }, loads);
}

}  // extend Pack

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
}
