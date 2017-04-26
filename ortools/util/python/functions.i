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

// This file provides swig wrapping for some specialization of std::function
// parameters. Currently, swig does not support much of C++11 features, and
// especially not the std::function.
//
// For now, clients must %include this.
// TODO(user): move the C++ code to a separate file, so that clients can
// simply %import this (and %{ #include %} the C++ file).
//
// Usage and tests in pywrapfunctions_test.py

%include "ortools/base/base.i"

%{
#include <functional>

// Wrap std::function<std::string()>

static std::string PyFunctionVoidToString(PyObject* pyfunc) {
  PyObject* pyresult = PyObject_CallFunctionObjArgs(pyfunc, nullptr);
  std::string result;
  if (!pyresult) {
    PyErr_SetString(PyExc_RuntimeError,
                    "std::function<std::string()> invocation failed.");
  } else {
    result = PyString_AsString(pyresult);
    Py_DECREF(pyresult);
  }
  return result;
}
%}

%typecheck(SWIG_TYPECHECK_POINTER) std::function<std::string()> {
  $1 = PyCallable_Check($input);
}

%typemap(in) std::function<std::string()> {
  $1 = [$input]() { return PyFunctionVoidToString($input); };
}

// Wrap std::function<int64(int64)>

%{
static int64 PyFunctionInt64ToInt64(PyObject* pyfunc, int64 i) {
  // () needed to force creation of one-element tuple
  PyObject* pyresult = PyEval_CallFunction(pyfunc, "(l)", static_cast<long>(i));
  int64 result = 0;
  if (!pyresult) {
    PyErr_SetString(PyExc_RuntimeError,
                    "std::function<int64(int64)> invocation failed.");
  } else {
    result = PyInt_AsLong(pyresult);
    Py_DECREF(pyresult);
  }
  return result;
}
%}

%typecheck(SWIG_TYPECHECK_POINTER) std::function<int64(int64)> {
  $1 = PyCallable_Check($input);
}

%typemap(in) std::function<int64(int64)> {
  $1 = [$input](int64 index) { return PyFunctionInt64ToInt64($input, index); };
}

// Wrap std::function<int64(int64, int64)>

%{
static int64 PyFunctionInt64Int64ToInt64(PyObject* pyfunc, int64 i, int64 j) {
  PyObject* pyresult = PyEval_CallFunction(pyfunc, "ll", static_cast<long>(i),
                                           static_cast<long>(j));
  int64 result = 0;
  if (!pyresult) {
    PyErr_SetString(PyExc_RuntimeError,
                    "std::function<int64(int64, int64)> invocation failed.");
  } else {
    result = PyInt_AsLong(pyresult);
    Py_DECREF(pyresult);
  }
  return result;
}
%}

%typecheck(SWIG_TYPECHECK_POINTER) std::function<int64(int64, int64)> {
  $1 = PyCallable_Check($input);
}

%typemap(in) std::function<int64(int64, int64)> {
  $1 = [$input](int64 i, int64 j) {
    return PyFunctionInt64Int64ToInt64($input, i, j);
  };
}

// Wrap std::function<int64(int64, int64, int64)>

%{
static int64 PyFunctionInt64Int64Int64ToInt64(PyObject* pyfunc,
                                              int64 i, int64 j, int64 k) {
  PyObject* pyresult = PyEval_CallFunction(pyfunc, "lll", static_cast<long>(i),
                                           static_cast<long>(j),
                                           static_cast<long>(k));
  int64 result = 0;
  if (!pyresult) {
    PyErr_SetString(
        PyExc_RuntimeError,
        "std::function<int64(int64, int64, int64)> invocation failed.");
  } else {
    result = PyInt_AsLong(pyresult);
    Py_DECREF(pyresult);
  }
  return result;
}
%}

%typecheck(SWIG_TYPECHECK_POINTER) std::function<int64(int64, int64, int64)> {
  $1 = PyCallable_Check($input);
}

%typemap(in) std::function<int64(int64, int64, int64)> {
  $1 = [$input](int64 i, int64 j, int64 k) {
    return PyFunctionInt64Int64Int64ToInt64($input, i, j, k);
  };
}

// Wrap std::function<int64(int)>

%{
static int64 PyFunctionIntToInt64(PyObject* pyfunc, int i) {
  // () needed to force creation of one-element tuple
  PyObject* pyresult = PyEval_CallFunction(pyfunc, "(i)", i);
  int64 result = 0;
  if (!pyresult) {
    PyErr_SetString(PyExc_RuntimeError,
                    "std::function<int64(int)> invocation failed.");
  } else {
    result = PyInt_AsLong(pyresult);
    Py_DECREF(pyresult);
  }
  return result;
}
%}

%typecheck(SWIG_TYPECHECK_POINTER) std::function<int64(int)> {
  $1 = PyCallable_Check($input);
}

%typemap(in) std::function<int64(int)> {
  $1 = [$input](int index) { return PyFunctionIntToInt64($input, index); };
}

// Wrap std::function<int64(int, int)>

%{
static int64 PyFunctionIntIntToInt64(PyObject* pyfunc, int i, int j) {
  PyObject* pyresult = PyEval_CallFunction(pyfunc, "ii", i, j);
  int64 result = 0;
  if (!pyresult) {
    PyErr_SetString(PyExc_RuntimeError,
                    "std::function<int64(int, int)> invocation failed.");
  } else {
    result = PyInt_AsLong(pyresult);
    Py_DECREF(pyresult);
  }
  return result;
}
%}

%typecheck(SWIG_TYPECHECK_POINTER) std::function<int64(int, int)> {
  $1 = PyCallable_Check($input);
}

%typemap(in) std::function<int64(int, int)> {
  $1 = [$input](int i, int j) {
    return PyFunctionIntIntToInt64($input, i, j);
  };
}

// Wrap std::function<bool(int64)>

%{
static bool PyFunctionInt64ToBool(PyObject* pyfunc, int64 i) {
  // () needed to force creation of one-element tuple
  PyObject* pyresult = PyEval_CallFunction(pyfunc, "(l)", static_cast<long>(i));
  bool result = 0;
  if (!pyresult) {
    PyErr_SetString(PyExc_RuntimeError,
                    "std::function<bool(int64)> invocation failed.");
  } else {
    result = PyObject_IsTrue(pyresult);
    Py_DECREF(pyresult);
  }
  return result;
}
%}

%typecheck(SWIG_TYPECHECK_POINTER) std::function<bool(int64)> {
  $1 = PyCallable_Check($input);
}

%typemap(in) std::function<bool(int64)> {
  $1 = [$input](int64 index) { return PyFunctionInt64ToBool($input, index); };
}

// Wrap std::function<bool()>

%{
static bool PyFunctionVoidToBool(PyObject* pyfunc) {
  PyObject* pyresult = PyObject_CallFunctionObjArgs(pyfunc, nullptr);
  bool result = false;
  if (!pyresult) {
    PyErr_SetString(PyExc_RuntimeError,
                    "std::function<bool()> invocation failed.");
  } else {
    result = PyObject_IsTrue(pyresult);
    Py_DECREF(pyresult);
  }
  return result;
}
%}

%typecheck(SWIG_TYPECHECK_POINTER) std::function<bool()> {
  $1 = PyCallable_Check($input);
}

%typemap(in) std::function<bool()> {
  $1 = [$input]() { return PyFunctionVoidToBool($input); };
}

%{
static void PyFunctionVoidToVoid(PyObject* pyfunc) {
  PyObject* pyresult = PyObject_CallFunctionObjArgs(pyfunc, nullptr);
  if (!pyresult) {
    PyErr_SetString(PyExc_RuntimeError,
                    "std::function<void()> invocation failed.");
  } else {
    Py_DECREF(pyresult);
  }
}
%}

%typecheck(SWIG_TYPECHECK_POINTER) std::function<void()> {
  $1 = PyCallable_Check($input);
}

%typemap(in) std::function<void()> {
  $1 = [$input]() { return PyFunctionVoidToVoid($input); };
}
