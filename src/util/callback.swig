// Copyright 2010-2013 Google
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

%include base/base.swig

%{
#include "base/callback.h"
#include "base/integral_types.h"
using std::string;
%}

#if defined(SWIGPYTHON)
namespace operations_research {
// ----- Callback Wrapping -----
%{
static string PyCallbackString(PyObject* pyfunc) {
   string result;
   PyObject* arglist = Py_BuildValue("()");
   PyObject* pyresult = PyEval_CallObject(pyfunc, arglist);
   Py_DECREF(arglist);
   if (pyresult) {
     result = PyString_AsString(pyresult);
     Py_DECREF(pyresult);
   }
   return result;
}
%}

%typemap(in) ResultCallback<string>* {
  if (!PyCallable_Check($input)) {
      PyErr_SetString(PyExc_TypeError, "Need a callable object!");
      SWIG_fail;
  }
  $1 = NewPermanentCallback(&PyCallbackString, $input);
}

%{
static int64 PyCallback1Int64Int64(PyObject* pyfunc, int64 i) {
   int64 result = 0;
   // Cast to int needed, no int64 support
   // () needed to force creation of one-element tuple
   PyObject* arglist = Py_BuildValue("(l)", static_cast<int>(i));
   PyObject* pyresult = PyEval_CallObject(pyfunc, arglist);
   Py_DECREF(arglist);
   if (pyresult) {
     result = PyInt_AsLong(pyresult);
   }
   Py_XDECREF(pyresult);
   return result;
}
%}

%typemap(in) ResultCallback1<int64, int64>* {
  if (!PyCallable_Check($input)) {
      PyErr_SetString(PyExc_TypeError, "Need a callable object!");
      SWIG_fail;
  }
  $1 = NewPermanentCallback(&PyCallback1Int64Int64, $input);
}

%{
static int64 PyCallback2Int64Int64Int64(PyObject* pyfunc, int64 i, int64 j) {
   int64 result = 0;
   // Cast to int needed, no int64 support
   PyObject* arglist = Py_BuildValue("ll",
                                     static_cast<int>(i),
                                     static_cast<int>(j));
   PyObject* pyresult = PyEval_CallObject(pyfunc, arglist);
   Py_DECREF(arglist);
   if (pyresult) {
     result = PyInt_AsLong(pyresult);
   }
   Py_XDECREF(pyresult);
   return result;
}
%}

%typemap(in) ResultCallback2<int64, int64, int64>* {
  if (!PyCallable_Check($input)) {
      PyErr_SetString(PyExc_TypeError, "Need a callable object!");
      SWIG_fail;
  }
  $1 = NewPermanentCallback(&PyCallback2Int64Int64Int64, $input);
}

%{
  static int64 PyCallback3Int64Int64Int64Int64(PyObject* pyfunc,
                                               int64 i, int64 j, int64 k) {
   int64 result = 0;
   // Cast to int needed, no int64 support
   PyObject* arglist = Py_BuildValue("lll",
                                     static_cast<int>(i),
                                     static_cast<int>(j),
                                     static_cast<int>(k));
   PyObject* pyresult = PyEval_CallObject(pyfunc, arglist);
   Py_DECREF(arglist);
   if (pyresult) {
     result = PyInt_AsLong(pyresult);
   }
   Py_XDECREF(pyresult);
   return result;
}
%}

%typemap(in) ResultCallback3<int64, int64, int64, int64>* {
  if (!PyCallable_Check($input)) {
      PyErr_SetString(PyExc_TypeError, "Need a callable object!");
      SWIG_fail;
  }
  $1 = NewPermanentCallback(&PyCallback3Int64Int64Int64Int64, $input);
}

%{
static bool PyCallbackBool(PyObject* pyfunc) {
   bool result = false;
   // "()" needed to force creation of empty argument list
   PyObject* arglist = Py_BuildValue("()");
   PyObject* pyresult = PyEval_CallObject(pyfunc, arglist);
   Py_DECREF(arglist);
   if (pyresult) {
     // no PyBool_AsBool so do this instead:
     if (pyresult == Py_True) {
       result = true;
     } else {
       result = false;
     }
   }
   Py_XDECREF(pyresult);
   return result;
}
%}

%typemap(in) ResultCallback<bool>* {
  if (!PyCallable_Check($input)) {
      PyErr_SetString(PyExc_TypeError, "Need a callable object!");
      SWIG_fail;
  }
  $1 = NewPermanentCallback(&PyCallbackBool, $input);
}
}  // namespace operations_research
#endif  // SWIGPYTHON

#if defined(SWIGJAVA)

%{
#include "base/jniutil.h"
#include "base/scoped_ptr.h"
%}

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
#endif  // SWIGJAVA

#if defined(SWIGCSHARP)
%include base/base.swig

%{
#include <vector>
#include "base/callback.h"
#include "base/integral_types.h"
%}

%module(directors="1") main
%feature("director") LongResultCallback1;
%feature("director") LongResultCallback2;
%feature("director") LongResultCallback3;
%{
#include <vector>
#include "base/callback.h"
#include "base/integral_types.h"

class LongResultCallback1 : public ResultCallback1<int64, int64> {
 public:
  LongResultCallback1() : used_as_permanent_handler_(false) {}
  virtual ~LongResultCallback1() {}
  virtual int64 Run(int64 i) = 0;
  ResultCallback1<int64, int64>* GetPermanentCallback() {
    CHECK(!used_as_permanent_handler_);
    used_as_permanent_handler_ = true;
    return this;
  }
 private:
  virtual bool IsRepeatable() const { return true; }

  bool used_as_permanent_handler_;
};

class LongResultCallback2 : public ResultCallback2<int64, int64, int64> {
 public:
  LongResultCallback2() : used_as_permanent_handler_(false) {}
  virtual ~LongResultCallback2() {}
  virtual int64 Run(int64, int64) = 0;
  ResultCallback2<int64, int64, int64>* GetPermanentCallback() {
    CHECK(!used_as_permanent_handler_);
    used_as_permanent_handler_ = true;
    return this;
  }
 private:
  virtual bool IsRepeatable() const { return true; }

  bool used_as_permanent_handler_;
};

class LongResultCallback3 : public ResultCallback3<int64, int64, int64, int64> {
 public:
  LongResultCallback3() : used_as_permanent_handler_(false) {}
  virtual ~LongResultCallback3() {}
  virtual int64 Run(int64, int64, int64) = 0;
    ResultCallback3<int64, int64, int64, int64>* GetPermanentCallback() {
    CHECK(!used_as_permanent_handler_);
    used_as_permanent_handler_ = true;
    return this;
  }
 private:
  virtual bool IsRepeatable() const { return true; }

  bool used_as_permanent_handler_;
};

%}

%typemap(cscode) LongResultCallback1 %{
  public SWIGTYPE_p_ResultCallback1T_long_long_long_long_t DisownAndGetPermanentCallback() {
    swigCMemOwn = false;
    return GetPermanentCallback();
  }
%}

%typemap(cscode)  LongResultCallback2 %{
  public SWIGTYPE_p_ResultCallback2T_long_long_long_long_long_long_t DisownAndGetPermanentCallback() {
    swigCMemOwn = false;
    return GetPermanentCallback();
  }
%}

%typemap(cscode)  LongResultCallback3 %{
  public SWIGTYPE_p_ResultCallback3T_long_long_long_long_long_long_long_long_t DisownAndGetPermanentCallback() {
    swigCMemOwn = false;
    return GetPermanentCallback();
  }
%}

class LongResultCallback1 : private ResultCallback1<int64, int64> {
 public:
  virtual int64 Run(int64 i) = 0;
  ResultCallback1<int64, int64>* GetPermanentCallback();
  virtual ~LongResultCallback1();
 private:
  virtual bool IsRepeatable() const;
  bool used_as_permanent_handler_;
};
class LongResultCallback2 : private ResultCallback2<int64, int64, int64> {
 public:
  virtual int64 Run(int64, int64) = 0;
  ResultCallback2<int64, int64, int64>* GetPermanentCallback();
  virtual ~LongResultCallback2();
 private:
  virtual bool IsRepeatable() const;
  bool used_as_permanent_handler_;
};
class LongResultCallback3 : private ResultCallback3<int64, int64, int64, int64> {
 public:
  virtual int64 Run(int64, int64, int64) = 0;
  ResultCallback3<int64, int64, int64, int64>* GetPermanentCallback();
  virtual ~LongResultCallback3();
 private:
  virtual bool IsRepeatable() const;
  bool used_as_permanent_handler_;
};

// Typemaps for callbacks in csharp.
%typemap(cstype) ResultCallback1<int64, int64>* "LongResultCallback1";
%typemap(csin) ResultCallback1<int64, int64>* "$descriptor(ResultCallback1<int64, int64>*).getCPtr($csinput.DisownAndGetPermanentCallback())";
%typemap(cstype) ResultCallback2<int64, int64, int64>* "LongResultCallback2";
%typemap(csin) ResultCallback2<int64, int64, int64>* "$descriptor(ResultCallback2<int64, int64, int64>*).getCPtr($csinput.DisownAndGetPermanentCallback())";
%typemap(cstype) ResultCallback3<int64, int64, int64, int64>*
"LongResultCallback3";
%typemap(csin) ResultCallback3<int64, int64, int64, int64>*
"$descriptor(ResultCallback3<int64, int64, int64, int64>*).getCPtr($csinput.DisownAndGetPermanentCallback())";
#endif  // SWIGCSHARP
