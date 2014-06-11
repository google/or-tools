// TODO(user): Refactor this file to adhere to the SWIG style guide.

%include exception.i
%include util/java/data.swig
%include util/java/tuple_set.swig

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

// TODO(user): split out the callback code; it creates another file since
// it uses a different module.
%module(directors="1") main
%feature("director") LongResultCallback1;
%feature("director") LongResultCallback2;
%feature("director") LongResultCallback3;
%include <std_vector.i>
%{
#include <vector>
#include "base/callback.h"
#include "base/integral_types.h"

// When a director is created for a class with SWIG, the C++ part of the
// director keeps a JNI global reference to the Java part. This global reference
// only gets deleted in the destructor of the C++ part, but by default, this
// only happens when the Java part is processed by the GC (however, this never
// happens, because there is the JNI global reference...).
//
// To break the cycle, it is necessary to delete the C++ part manually. For the
// callback classes, this is done by deriving them from the respective C++
// ResultCallback classes. When the java callback class is asked for a C++
// callback class, it hands over its C++ part. It is expected, that whoever
// receives the C++ callback class, owns it and destroys it after they no longer
// need it. But by destroying it, they also break the reference cycle and the
// Java part may be processed by the GC.
//
// However, this behavior also means that the callback class can only be used
// in one context and that if its C++ callback class is not received by someone
// who destroys it in the end, it will stay in memory forever.
class LongResultCallback1 : private ResultCallback1<int64, int64> {
 public:
  LongResultCallback1() : used_as_permanent_handler_(false) {}
  virtual int64 run(int64) = 0;
  ResultCallback1<int64, int64>* GetPermanentCallback() {
    CHECK(!used_as_permanent_handler_);
    used_as_permanent_handler_ = true;
    return this;
  }
  virtual ~LongResultCallback1() {}
 private:
  virtual bool IsRepeatable() const { return true; }

  virtual int64 Run(int64 i) {
    return run(i);
  }
  bool used_as_permanent_handler_;
};

class LongResultCallback2 : private ResultCallback2<int64, int64, int64> {
 public:
  LongResultCallback2() : used_as_permanent_handler_(false) {}
  virtual ~LongResultCallback2() {}
  virtual int64 run(int64, int64) = 0;
  ResultCallback2<int64, int64, int64>* GetPermanentCallback() {
    CHECK(!used_as_permanent_handler_);
    used_as_permanent_handler_ = true;
    return this;
  }
 private:
  virtual bool IsRepeatable() const { return true; }

  virtual int64 Run(int64 i, int64 j) {
    return run(i, j);
  }
  bool used_as_permanent_handler_;
};

class LongResultCallback3 : private ResultCallback3<int64, int64, int64, int64> {
 public:
  LongResultCallback3() : used_as_permanent_handler_(false) {}
  virtual ~LongResultCallback3() {}
  virtual int64 run(int64, int64, int64) = 0;
  ResultCallback3<int64, int64, int64, int64>* GetPermanentCallback() {
    CHECK(!used_as_permanent_handler_);
    used_as_permanent_handler_ = true;
    return this;
  }
 private:
  virtual bool IsRepeatable() const { return true; }

  virtual int64 Run(int64 i, int64 j, int64 k) {
    return run(i, j, k);
  }
  bool used_as_permanent_handler_;
};
%}

class LongResultCallback1 : private ResultCallback1<int64, int64> {
 public:
  virtual int64 run(int64) = 0;
  ResultCallback1<int64, int64>* GetPermanentCallback();
  virtual ~LongResultCallback1();
 private:
  virtual bool IsRepeatable() const;
  virtual int64 Run(int64 i);
  bool used_as_permanent_handler_;
};
class LongResultCallback2 : private ResultCallback2<int64, int64, int64> {
 public:
  virtual int64 run(int64, int64) = 0;
  ResultCallback2<int64, int64, int64>* GetPermanentCallback();
  virtual ~LongResultCallback2();
 private:
  virtual bool IsRepeatable() const;
  virtual int64 Run(int64 i, int64 j);
  bool used_as_permanent_handler_;
};
class LongResultCallback3 : private ResultCallback3<int64, int64, int64, int64> {
 public:
  virtual int64 run(int64, int64, int64) = 0;
  ResultCallback3<int64, int64, int64, int64>* GetPermanentCallback();
  virtual ~LongResultCallback3();
 private:
  virtual bool IsRepeatable() const;
  virtual int64 Run(int64 i, int64 j, int64 k);
  bool used_as_permanent_handler_;
};

// Typemaps for callbacks in java.
%typemap(jstype) ResultCallback1<int64, int64>* "LongResultCallback1";
%typemap(javain) ResultCallback1<int64, int64>* "$descriptor(ResultCallback1<int64, int64>*).getCPtr($javainput.GetPermanentCallback())";
%typemap(jstype) ResultCallback2<int64, int64, int64>* "LongResultCallback2";
%typemap(javain) ResultCallback2<int64, int64, int64>* "$descriptor(ResultCallback2<int64, int64, int64>*).getCPtr($javainput.GetPermanentCallback())";

%typemap(jstype) ResultCallback3<int64, int64, int64, int64>*
"LongResultCallback3";
%typemap(javain) ResultCallback3<int64, int64, int64, int64>*
"$descriptor(ResultCallback3<int64, int64, int64, int64>*).getCPtr($javainput.GetPermanentCallback())";

%module(directors="1") operations_research;
%feature("director") DecisionBuilder;
%feature("director") Decision;
%feature("director") SearchMonitor;
%feature("director") Action;
%feature("director") LocalSearchOperator;
%feature("director") PathOperator;
%feature("director") BaseLns;
%feature("director") IntVarLocalSearchOperator;
%feature("director") SequenceVarLocalSearchOperator;
%feature("director") IntVarLocalSearchFilter;
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
    self->FindIndex(var, &index);
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

// Define templates instantiation after wrapping.
namespace operations_research {
%template(RevInteger) Rev<int64>;
%template(RevBool) Rev<bool>;
typedef Assignment::AssignmentContainer AssignmentContainer;
%template(AssignmentIntContainer) AssignmentContainer<IntVar, IntVarElement>;
}
