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

// SWIG wrapping of some Callbacks in python.
// This doesn't directly expose a C++ API; rather it provides typemaps that can
// be used by other .i files to easily expose APIs that use callbacks.
// See its usage in ../../constraint_solver/python/constraint_solver.i.
//
// TODO(user): Nuke this file and use the callback wrapping code in
// base/swig/python/... when it is available.

%include "ortools/base/base.i"

%import "ortools/base/callback.h"

%{
static std::string PyCallbackString(PyObject* pyfunc) {
  PyObject* pyresult = PyObject_CallFunctionObjArgs(pyfunc, nullptr);
  std::string result;
  if (!pyresult) {
    PyErr_SetString(PyExc_RuntimeError,
                    "ResultCallback<std::string> invocation failed.");
  } else {
    result = PyString_AsString(pyresult);
    Py_DECREF(pyresult);
  }
  return result;
}
%}

%typemap(in) ResultCallback<std::string>* {
  if (!PyCallable_Check($input)) {
    PyErr_SetString(PyExc_TypeError, "Need a callable object!");
    SWIG_fail;
  }
  $1 = NewPermanentCallback(&PyCallbackString, $input);
}

%{
static int64 PyCallback1Int64Int64(PyObject* pyfunc, int64 i) {
  // () needed to force creation of one-element tuple
  PyObject* pyresult = PyEval_CallFunction(pyfunc, "(l)", static_cast<long>(i));
  int64 result = 0;
  if (!pyresult) {
    PyErr_SetString(PyExc_RuntimeError,
                    "ResultCallback1<int64, int64> invocation failed.");
  } else {
    result = PyInt_AsLong(pyresult);
    Py_DECREF(pyresult);
  }
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
  PyObject* pyresult = PyEval_CallFunction(pyfunc, "ll", static_cast<long>(i),
                                           static_cast<long>(j));
  int64 result = 0;
  if (!pyresult) {
    PyErr_SetString(PyExc_RuntimeError,
                    "ResultCallback2<int64, int64, int64> invocation failed.");
  } else {
    result = PyInt_AsLong(pyresult);
    Py_DECREF(pyresult);
  }
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
  PyObject* pyresult = PyEval_CallFunction(pyfunc, "lll", static_cast<long>(i),
                                           static_cast<long>(j),
                                           static_cast<long>(k));
  int64 result = 0;
  if (!pyresult) {
    PyErr_SetString(
        PyExc_RuntimeError,
        "ResultCallback3<int64, int64, int64, int64> invocation failed.");
  } else {
    result = PyInt_AsLong(pyresult);
    Py_DECREF(pyresult);
  }
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
  PyObject* pyresult = PyObject_CallFunctionObjArgs(pyfunc, nullptr);
  bool result = false;
  if (!pyresult) {
    PyErr_SetString(PyExc_RuntimeError,
                    "ResultCallback<bool> invocation failed.");
  } else {
    result = PyObject_IsTrue(pyresult);
    Py_DECREF(pyresult);
  }
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
