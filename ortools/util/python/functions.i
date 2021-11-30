// Copyright 2010-2021 Google LLC
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

// A copyable, ref-counted python pointer.
// TODO(user): Make it movable-only when we support generalized lambda
// capture.
class SharedPyPtr {
 public:
  explicit SharedPyPtr(PyObject* obj) : obj_(obj) { Py_INCREF(obj_); }
  SharedPyPtr(const SharedPyPtr& other) : obj_(other.obj_) { Py_INCREF(obj_); }

  ~SharedPyPtr() { Py_DECREF(obj_); }

  PyObject* get() const { return obj_; }

 private:
  // We do not follow the rule of three as we only want to copy construct.
  SharedPyPtr& operator=(const SharedPyPtr&);

  PyObject* const obj_;
};

template <typename ReturnT>
static ReturnT HandleResult(PyObject* pyresult) {
  // This zero-initializes builtin types.
  ReturnT result = ReturnT();
  if (!pyresult) {
    if (!PyErr_Occurred()) {
      PyErr_SetString(PyExc_RuntimeError,
                      "SWIG std::function invocation failed.");
    }
    return result;
  } else {
    if (!PyObjAs<ReturnT>(pyresult, &result)) {
      if (!PyErr_Occurred()) {
        PyErr_SetString(PyExc_RuntimeError,
                        "SWIG std::function invocation failed.");
      }
    }
    Py_DECREF(pyresult);
  }
  return result;
}

template <>
void HandleResult<void>(PyObject * pyresult) {
  if (!pyresult) {
    if (!PyErr_Occurred()) {
      PyErr_SetString(PyExc_RuntimeError,
                      "SWIG std::function invocation failed.");
    }
  } else {
    Py_DECREF(pyresult);
  }
}

template <typename ReturnT, typename... Args>
static ReturnT InvokePythonCallableReturning(PyObject* pyfunc,
                                             const char* format, Args... args) {
  // The const_cast is safe (it's here only because the python API is not
  // const-correct).
  return HandleResult<ReturnT>(
      PyObject_CallFunction(pyfunc, const_cast<char*>(format), args...));
}

template <typename ReturnT>
static ReturnT InvokePythonCallableReturning(PyObject* pyfunc) {
  return HandleResult<ReturnT>(PyObject_CallFunctionObjArgs(pyfunc, nullptr));
}

%}

// Wrap std::function<std::string()>

%typecheck(SWIG_TYPECHECK_POINTER) std::function<std::string()> {
  $1 = PyCallable_Check($input);
}

%typemap(in) std::function<std::string()> {
  SharedPyPtr input($input);
  $1 = [input]() { return InvokePythonCallableReturning<std::string>(input.get()); };
}

// Wrap std::function<int64_t(int64_t)>

%typecheck(SWIG_TYPECHECK_POINTER) std::function<int64_t(int64_t)> {
  $1 = PyCallable_Check($input);
}

%typemap(in) std::function<int64_t(int64_t)> {
  SharedPyPtr input($input);
  $1 = [input](int64_t index) {
    return InvokePythonCallableReturning<int64_t>(input.get(), "(L)", index);
  };
}

// Wrap std::function<int64_t(int64_t, int64_t)>

%typecheck(SWIG_TYPECHECK_POINTER) std::function<int64_t(int64_t, int64_t)> {
  $1 = PyCallable_Check($input);
}

%typemap(in) std::function<int64_t(int64_t, int64_t)> {
  SharedPyPtr input($input);
  $1 = [input](int64_t i, int64_t j) {
    return InvokePythonCallableReturning<int64_t>(input.get(), "LL", i, j);
  };
}

// Wrap std::function<int64_t(int64_t, int64_t, int64_t)>

%typecheck(SWIG_TYPECHECK_POINTER) std::function<int64_t(int64_t, int64_t, int64_t)> {
  $1 = PyCallable_Check($input);
}

%typemap(in) std::function<int64_t(int64_t, int64_t, int64_t)> {
  SharedPyPtr input($input);
  $1 = [input](int64_t i, int64_t j, int64_t k) {
    return InvokePythonCallableReturning<int64_t>(input.get(), "LLL", i, j, k);
  };
}

// Wrap std::function<int64_t(int)>

%typecheck(SWIG_TYPECHECK_POINTER) std::function<int64_t(int)> {
  $1 = PyCallable_Check($input);
}

%typemap(in) std::function<int64_t(int)> {
  SharedPyPtr input($input);
  $1 = [input](int index) {
    return InvokePythonCallableReturning<int64_t>(input.get(), "(i)", index);
  };
}

// Wrap std::function<int64_t(int, int)>

%typecheck(SWIG_TYPECHECK_POINTER) std::function<int64_t(int, int)> {
  $1 = PyCallable_Check($input);
}

%typemap(in) std::function<int64_t(int, int)> {
  SharedPyPtr input($input);
  $1 = [input](int i, int j) {
    return InvokePythonCallableReturning<int64_t>(input.get(), "ii", i, j);
  };
}

// Wrap std::function<bool(int64_t)>

%typecheck(SWIG_TYPECHECK_POINTER) std::function<bool(int64_t)> {
  $1 = PyCallable_Check($input);
}

%typemap(in) std::function<bool(int64_t)> {
  SharedPyPtr input($input);
  $1 = [input](int64_t index) {
    return InvokePythonCallableReturning<bool>(input.get(), "(L)", index);
  };
}

// Wrap std::function<bool()>

%typecheck(SWIG_TYPECHECK_POINTER) std::function<bool()> {
  $1 = PyCallable_Check($input);
}

%typemap(in) std::function<bool()> {
  SharedPyPtr input($input);
  $1 = [input]() { return InvokePythonCallableReturning<bool>(input.get()); };
}

// Wrap std::function<void()>

%typecheck(SWIG_TYPECHECK_POINTER) std::function<void()> {
  $1 = PyCallable_Check($input);
}

%typemap(in) std::function<void()> {
  SharedPyPtr input($input);
  $1 = [input]() { return InvokePythonCallableReturning<void>(input.get()); };
}

// Wrap std::function<void(std::string)>

%typecheck(SWIG_TYPECHECK_POINTER) std::function<void(const std::string&)> {
  $1 = PyCallable_Check($input);
}

%typemap(in) std::function<void(const std::string&)> {
  SharedPyPtr input($input);
  $1 = [input](const std::string& str) {
    PyObject* py_str = PyUnicode_FromStringAndSize(str.c_str(), str.size());
    PyObject* result;
    SWIG_PYTHON_THREAD_BEGIN_BLOCK;
    result = PyObject_CallFunction(input.get(), "O", py_str);
    SWIG_PYTHON_THREAD_END_BLOCK;
    Py_DECREF(py_str);
    return result;
  };
}
