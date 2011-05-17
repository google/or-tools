// Copyright 2010-2011 Google
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
#include <vector>
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

// ----- std::vector<data> wrapping -----

// Add conversion rules for std::vector<int64>.
%typemap(in) const std::vector<int64>& (std::vector<int64> temp) {
  if (!PyTuple_Check($input) && !PyList_Check($input)) {
    PyErr_SetString(PyExc_TypeError, "Expecting a sequence");
    SWIG_fail;
  }
  bool is_tuple = PyTuple_Check($input);
  temp.resize(is_tuple ? PyTuple_Size($input) : PyList_Size($input));
  for (size_t i = 0; i < temp.size(); ++i) {
    temp[i] = PyInt_AsLong(is_tuple ? PyTuple_GetItem($input, i) :
                           PyList_GetItem($input, i));
  }
  $1 = &temp;
}

%typecheck(SWIG_TYPECHECK_POINTER) const std::vector<int64>& {
  if (!PyTuple_Check($input) && !PyList_Check($input)) {
    $1 = 0;
  } else {
    const bool is_tuple = PyTuple_Check($input);
    const int size = is_tuple ? PyTuple_Size($input) : PyList_Size($input);
    bool failed = false;
    for (size_t i = 0; i < size; ++i) {
      PyObject* const obj =
          is_tuple ? PyTuple_GetItem($input, i) : PyList_GetItem($input, i);
      if (!PyInt_Check(obj) && !PyLong_Check(obj)) {
        failed = true;
        break;
      }
    }
    $1 = failed ? 0 : 1;
  }
}

// Add conversion list(tuple(int)) -> std::vector<vector>.
%typemap(in) const std::vector<std::vector<int64> >&
    (std::vector<std::vector<int64> > temp) {
  if (!PyList_Check($input)) {
    PyErr_SetString(PyExc_TypeError, "Expecting a list of tuples");
    SWIG_fail;
  }
  int len = PyList_Size($input);
  int arity = -1;
  if (len > 0) {
    temp.resize(len);
    for (size_t i = 0; i < len; ++i) {
      PyObject *tuple = PyList_GetItem($input, i);
      if (!PyTuple_Check(tuple) && !PyList_Check(tuple)) {
        PyErr_SetString(PyExc_TypeError, "Expecting a sequence");
        SWIG_fail;
      }
      bool is_tuple = PyTuple_Check(tuple);
      int local_arity = is_tuple ? PyTuple_Size(tuple) : PyList_Size(tuple);
      if (arity != -1 && arity != local_arity) {
        PyErr_SetString(PyExc_TypeError, "Tuples should have the same arity");
        SWIG_fail;
      }
      if (arity == -1) {
        arity = local_arity;
      }
      temp[i].resize(arity);
      for (size_t j = 0; j < local_arity; ++j) {
        temp[i][j] = PyInt_AsLong(is_tuple ?
                                  PyTuple_GetItem(tuple, j) :
                                  PyList_GetItem(tuple, j));
      }
    }
  }
  $1 = &temp;
}

%typecheck(SWIG_TYPECHECK_POINTER) const std::vector<std::vector<int64> >& {
  if (!PyList_Check($input)) {
    $1 = 0;
  } else {
    const int size = PyList_Size($input);
    bool failed = false;
    for (size_t i = 0; i < size; ++i) {
      PyObject* const tuple = PyList_GetItem($input, i);
      if (!PyTuple_Check(tuple) && !PyList_Check(tuple)) {
        $1 = 0;
        break;
      } else {
        const bool is_tuple = PyTuple_Check(tuple);
        const int arity = is_tuple ? PyTuple_Size(tuple) : PyList_Size(tuple);
        for (size_t j = 0; j < arity; ++j) {
          PyObject* const entry =
              is_tuple ? PyTuple_GetItem(tuple, j) : PyList_GetItem(tuple, j);
          if (!PyInt_Check(entry) && !PyLong_Check(entry)) {
            failed = true;
            break;
          }
        }
      }
      if (failed) {
        break;
      }
    }
    $1 = failed ? 0 : 1;
  }
}
}  // namespace operations_research
#endif  // SWIGPYTHON

#if defined(SWIGJAVA)
%module(directors="1") main
%feature("director") LongResultCallback1;
%feature("director") LongResultCallback2;
%{
#include <vector>
#include "base/callback.h"
#include "base/integral_types.h"

class LongResultCallback1 {
 public:
  virtual int64 run(int64) = 0;
  ResultCallback1<int64, int64>* GetPermanentCallback() {
    return NewPermanentCallback(this, &LongResultCallback1::run);
  }
  virtual ~LongResultCallback1() {}
};
class LongResultCallback2 {
 public:
  virtual int64 run(int64, int64) = 0;
  ResultCallback2<int64, int64, int64>* GetPermanentCallback() {
    return NewPermanentCallback(this, &LongResultCallback2::run);
  }
  virtual ~LongResultCallback2() {}
};
class LongResultCallback3 {
 public:
  virtual int64 run(int64, int64, int64) = 0;
  ResultCallback3<int64, int64, int64, int64>* GetPermanentCallback() {
    return NewPermanentCallback(this, &LongResultCallback3::run);
  }
  virtual ~LongResultCallback3() {}
};

%}

class LongResultCallback1 {
 public:
  virtual int64 run(int64) = 0;
  ResultCallback1<int64, int64>* GetPermanentCallback();
  virtual ~LongResultCallback1();
};
class LongResultCallback2 {
 public:
  virtual int64 run(int64, int64) = 0;
  ResultCallback2<int64, int64, int64>* GetPermanentCallback();
  virtual ~LongResultCallback2();
};
class LongResultCallback3 {
 public:
  virtual int64 run(int64, int64, int64) = 0;
  ResultCallback3<int64, int64, int64, int64>* GetPermanentCallback();
  virtual ~LongResultCallback3();
};

// Typemaps for callbacks in java.
%typemap(jstype) ResultCallback1<int64, int64>* "LongResultCallback1";
%typemap(javain) ResultCallback1<int64, int64>* "SWIGTYPE_p_ResultCallback1Tlong_long_long_long_t.getCPtr($javainput.GetPermanentCallback())";

%typemap(jstype) ResultCallback2<int64, int64, int64>* "LongResultCallback2";
%typemap(javain) ResultCallback2<int64, int64, int64>* "SWIGTYPE_p_ResultCallback2Tlong_long_long_long_long_long_t.getCPtr($javainput.GetPermanentCallback())";

%typemap(jstype) ResultCallback3<int64, int64, int64, int64>*
"LongResultCallback3";
%typemap(javain) ResultCallback3<int64, int64, int64, int64>*
 "SWIGTYPE_p_ResultCallback3Tlong_long_long_long_long_long_long_long_t.getCPtr($javainput.GetPermanentCallback())";


// Typemaps to represent const std::vector<int64>& arguments as arrays of long.
%typemap(jni) const std::vector<int64>& "jobjectArray"
%typemap(jtype) const std::vector<int64>& "long[]"
%typemap(jstype) const std::vector<int64>& "long[]"
%typemap(javain) const std::vector<int64>& "$javainput"
%typemap(in) const std::vector<int64>& %{
  if($input) {
    $1 = new std::vector<int64>;
    int size = jenv->GetArrayLength($input);
    jlong *values = jenv->GetLongArrayElements((jlongArray)$input, NULL);
    for (int i = 0; i < size; ++i) {
      long value = values[i];
      $1->push_back(value);
    }
  }
  else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null table");
    return $null;
  }
%}
%typemap(freearg) const std::vector<int64>& {
  delete $1;
}

// Typemaps to represent const std::vector<int>& arguments as arrays of long.
%typemap(jni) const std::vector<int>& "jobjectArray"
%typemap(jtype) const std::vector<int>& "int[]"
%typemap(jstype) const std::vector<int>& "int[]"
%typemap(javain) const std::vector<int>& "$javainput"

%typemap(in) const std::vector<int>& %{
  if($input) {
    $1 = new std::vector<int>;
    int size = jenv->GetArrayLength($input);
    jint *values = jenv->GetIntArrayElements((jintArray)$input, NULL);
    for (int i = 0; i < size; ++i) {
      int value = values[i];
      $1->push_back(value);
    }
  }
  else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null table");
    return $null;
  }
%}
%typemap(freearg) const std::vector<int>& {
  delete $1;
}

// Convert long[][] to std::vector<std::vector<int64> >
%typemap(jni) const std::vector<std::vector<int64> >& "jobjectArray"
%typemap(jtype) const std::vector<std::vector<int64> >& "long[][]"
%typemap(jstype) const std::vector<std::vector<int64> >& "long[][]"
%typemap(javain) const std::vector<std::vector<int64> >& "$javainput"
%typemap(in) const std::vector<std::vector<int64> >& (std::vector<std::vector<int64> > result) {
  const int size = jenv->GetArrayLength($input);
  result.clear();
  result.resize(size);
  for (int index1 = 0; index1 < size; ++index1) {
    jlongArray inner_array =
        (jlongArray)jenv->GetObjectArrayElement($input, index1);
    const int tuple_size = jenv->GetArrayLength(inner_array);
    jlong* const values =
        jenv->GetLongArrayElements((jlongArray)inner_array, NULL);
    for (int index2 = 0; index2 < tuple_size; ++index2) {
      const int64 value = values[index2];
      result[index1].push_back(value);
    }
  }
  $1 = &result;
}

#endif  // SWIGJAVA
