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
%include "std_vector.i"

%include "base/base.swig"
%include "util/java/proto.swig"
%include "util/java/tuple_set.swig"
%include "util/java/vector.swig"
%include "util/java/functions.swig"

// Remove swig warnings
%warnfilter(473) operations_research::DecisionBuilder;
// TODO(user): Remove this warnfilter.

// ############ BEGIN DUPLICATED CODE BLOCK ############
// IMPORTANT: keep this code block in sync with the .swig
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

%module(directors="1") operations_research;

%feature("director") DecisionBuilder;
%feature("director") Decision;
%feature("director") SearchMonitor;
%feature("director") LocalSearchOperator;
%feature("director") PathOperator;
%feature("director") BaseLns;
%feature("director") IntVarLocalSearchOperator;
%feature("director") SequenceVarLocalSearchOperator;
%feature("director") IntVarLocalSearchFilter;
%template(IntVector) std::vector<int>;


%{
#include <setjmp.h>

#include <vector>

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
%}

// Renaming

%ignore operations_research::Solver::MakeIntVarArray;
%ignore operations_research::Solver::MakeBoolVarArray;
%ignore operations_research::Solver::MakeFixedDurationIntervalVarArray;
%ignore operations_research::IntVarLocalSearchFilter::FindIndex;

// This method causes issues with our std::vector<int64> wrapping. It's not really
// part of the public API anyway.
%ignore operations_research::ToInt64Vector;

%rename (nextWrap) operations_research::DecisionBuilder::Next;
%rename (toString) *::DebugString;
%rename (tryDecisions) operations_research::Solver::Try;

%rename("%(lowercamelcase)s", %$isfunction) "";

namespace operations_research {
%define CONVERT_VECTOR(type)
%typemap(jni) const std::vector<type*>& "jobjectArray"
%typemap(jtype) const std::vector<type*>& "type[]"
%typemap(jstype) const std::vector<type*>& "type[]"
%typemap(javain) const std::vector<type*>& "$javainput"
%typemap(in) const std::vector<type*>& (std::vector<type*> result) {
  jclass object_class =
    jenv->FindClass("com/google/ortools/"
                    "constraintsolver/type");
  if (nullptr == object_class)
    return $null;
  jmethodID method_id =
      jenv->GetStaticMethodID(object_class,
                              "getCPtr",
                              "(Lcom/google/ortools/"
                              "constraintsolver/type;)J");
  assert(method_id != nullptr);
  for (int i = 0; i < jenv->GetArrayLength($input); i++) {
    jobject elem = jenv->GetObjectArrayElement($input, i);
    jlong ptr_value = jenv->CallStaticLongMethod(object_class, method_id, elem);
    result.push_back(reinterpret_cast<operations_research::type*>(ptr_value));
  }
  $1 = &result;
}
%enddef
CONVERT_VECTOR(IntVar);
CONVERT_VECTOR(SearchMonitor);
CONVERT_VECTOR(DecisionBuilder);
CONVERT_VECTOR(IntervalVar);
CONVERT_VECTOR(SequenceVar);
CONVERT_VECTOR(LocalSearchOperator);
CONVERT_VECTOR(LocalSearchFilter);

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
  int Index(IntVar* const var) {
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

namespace operations_research {
class LocalSearchPhaseParameters {
 public:
  LocalSearchPhaseParameters();
  ~LocalSearchPhaseParameters();
};
}  // namespace operations_research


// Wrap cp includes
%include "constraint_solver/constraint_solver.h"
%include "constraint_solver/constraint_solveri.h"

// Define templates instantiation after wrapping.
namespace operations_research {
%template(RevInteger) Rev<int>;
%template(RevLong) Rev<int64>;
%template(RevBool) Rev<bool>;
%template(AssignmentIntContainer) AssignmentContainer<IntVar, IntVarElement>;
}  // namespace operations_research
