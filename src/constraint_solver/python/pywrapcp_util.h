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
// This .h file cannot be used in isolation!
// It represents some of the inlined C++ content of ./constraint_solver.swig,
// and was split out because it's a large enough chunk of C++ code.
//
// It can only be interpreted in the context of ./constraint_solver.swig, where
// it is included.

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_PYTHON_PYWRAPCP_UTIL_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_PYTHON_PYWRAPCP_UTIL_H_

#include <setjmp.h>  // For FailureProtect. See below.
#include <string>
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"

using std::string;

// Used in the PROTECT_FROM_FAILURE macro. See below.
struct FailureProtect {
  jmp_buf exception_buffer;
  void JumpBack() { longjmp(exception_buffer, 1); }
};

class CallPyDecisionBuilder : public operations_research::DecisionBuilder {
 public:
  explicit CallPyDecisionBuilder(PyObject* pydb)
      : pysolver_(nullptr), pyarg_(nullptr) {
    Py_INCREF(pydb);
    pydb_ = pydb;
    func_ = PyObject_GetAttrString(pydb_, "NextWrapper");
    Py_XINCREF(func_);
    str_func_ = PyObject_GetAttrString(pydb_, "DebugString");
    Py_XINCREF(str_func_);
  }

  virtual ~CallPyDecisionBuilder() {
    Py_CLEAR(pydb_);
    Py_CLEAR(func_);
    Py_CLEAR(str_func_);
    Py_CLEAR(pysolver_);
    Py_CLEAR(pyarg_);
  }

  virtual operations_research::Decision* Next(
      operations_research::Solver* const s) {
    if (pysolver_ == nullptr) {
      pysolver_ = SWIG_NewPointerObj(s, SWIGTYPE_p_operations_research__Solver,
                                     SWIG_POINTER_EXCEPTION);
      pyarg_ = Py_BuildValue("(O)", pysolver_);
    }
    operations_research::Decision* result = nullptr;
    PyObject* pyresult = PyEval_CallObject(func_, pyarg_);
    if (pyresult) {
      if (SWIG_ConvertPtr(pyresult, reinterpret_cast<void**>(&result),
                          SWIGTYPE_p_operations_research__Decision,
                          SWIG_POINTER_EXCEPTION | 0) == -1) {
        LOG(INFO) << "Error in type from python Decision";
      }
      Py_DECREF(pyresult);
    } else {  // something went wrong, we fail.
      PyErr_SetString(PyExc_RuntimeError,
                      "ResultCallback<std::string> invocation failed.");
      s->FinishCurrentSearch();
    }
    return result;
  }

  virtual std::string DebugString() const {
    std::string result = "PyDecisionBuilder";
    if (str_func_) {
      PyObject* pyresult = PyEval_CallObject(str_func_, nullptr);
      if (pyresult) {
        result = PyString_AsString(pyresult);
        Py_DECREF(pyresult);
      }
    }
    return result;
  }

 private:
  PyObject* pysolver_;
  PyObject* pyarg_;
  PyObject* pydb_;
  PyObject* func_;
  PyObject* str_func_;
};

class PyLNSNoValues : public operations_research::BaseLNS {
 public:
  PyLNSNoValues(const std::vector<operations_research::IntVar*>& vars, PyObject* op)
      : BaseLNS(vars), op_(op) {
    Py_INCREF(op_);
    init_func_ = PyObject_GetAttrString(op_, "InitFragments");
    Py_XINCREF(init_func_);
    fragment_func_ = PyObject_GetAttrString(op_, "NextFragment");
    Py_XINCREF(fragment_func_);
  }

  virtual ~PyLNSNoValues() {
    Py_CLEAR(op_);
    Py_CLEAR(init_func_);
    Py_CLEAR(fragment_func_);
  }

  virtual void InitFragments() {
    if (init_func_) {
      PyObject* pyresult = PyEval_CallObject(init_func_, nullptr);
      Py_XDECREF(pyresult);
    }
  }

  virtual bool NextFragment(std::vector<int>* fragment) {
    PyObject* list = PyList_New(0);
    PyObject* args = Py_BuildValue(const_cast<char*>("(O)"), list);
    PyObject* pyresult = PyEval_CallObject(fragment_func_, args);
    Py_DECREF(args);
    const int size = PyList_Size(list);
    for (size_t i = 0; i < size; ++i) {
      const int val = PyInt_AsLong(PyList_GetItem(list, i));
      fragment->push_back(val);
    }
    Py_DECREF(list);
    bool result = false;
    if (pyresult) {
      result = PyInt_AsLong(pyresult);
      Py_DECREF(pyresult);
    }
    return result;
  }

  virtual std::string DebugString() const { return "PyLNSNoValues()"; }

 private:
  PyObject* op_;
  PyObject* init_func_;
  PyObject* fragment_func_;
};

class PyLNS : public operations_research::BaseLNS {
 public:
  PyLNS(const std::vector<operations_research::IntVar*>& vars, PyObject* op)
      : BaseLNS(vars), op_(op) {
    Py_INCREF(op_);
    init_func_ = nullptr;
    if (PyObject_HasAttrString(op_, "InitFragments")) {
      init_func_ = PyObject_GetAttrString(op_, "InitFragments");
      Py_INCREF(init_func_);
    }
    fragment_func_ = nullptr;
    if (PyObject_HasAttrString(op_, "NextFragment")) {
      fragment_func_ = PyObject_GetAttrString(op_, "NextFragment");
      Py_INCREF(fragment_func_);
    }
    base_lns_ = SWIG_NewPointerObj(
        this, SWIGTYPE_p_operations_research__BaseLNS, SWIG_POINTER_EXCEPTION);
    Py_INCREF(base_lns_);
  }
  virtual ~PyLNS() {
    Py_CLEAR(op_);
    Py_CLEAR(init_func_);
    Py_CLEAR(fragment_func_);
    Py_CLEAR(base_lns_);
  }

  virtual void InitFragments() {
    if (init_func_) {
      PyObject* pyresult = PyEval_CallObject(init_func_, nullptr);
      Py_XDECREF(pyresult);
    }
  }

  virtual bool NextFragment(std::vector<int>* fragment) {
    PyObject* list = PyList_New(0);
    PyObject* args = Py_BuildValue(const_cast<char*>("(OO)"), list, base_lns_);
    PyObject* pyresult = PyEval_CallObject(fragment_func_, args);
    Py_DECREF(args);
    const int size = PyList_Size(list);
    for (size_t i = 0; i < size; ++i) {
      const int val = PyInt_AsLong(PyList_GetItem(list, i));
      fragment->push_back(val);
    }
    Py_DECREF(list);
    bool result = false;
    if (pyresult) {
      result = PyInt_AsLong(pyresult);
      Py_DECREF(pyresult);
    }
    return result;
  }

  virtual std::string DebugString() const { return "PyLNS()"; }

 private:
  PyObject* op_;
  PyObject* init_func_;
  PyObject* fragment_func_;
  PyObject* base_lns_;
};

// Conversion helpers. In SWIG python, "PyObjAs" is the method that determines
// wheter a python object can be interpreted as a C++ object.

#endif  // OR_TOOLS_CONSTRAINT_SOLVER_PYTHON_PYWRAPCP_UTIL_H_
