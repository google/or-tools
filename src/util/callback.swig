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
