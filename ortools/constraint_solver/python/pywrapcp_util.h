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
// It represents some of the inlined C++ content of ./constraint_solver.i,
// and was split out because it's a large enough chunk of C++ code.
//
// It can only be interpreted in the context of ./constraint_solver.i, where
// it is included.

#ifndef OR_TOOLS_CONSTRAINT_SOLVER_PYTHON_PYWRAPCP_UTIL_H_
#define OR_TOOLS_CONSTRAINT_SOLVER_PYTHON_PYWRAPCP_UTIL_H_

#include <string>
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"

using std::string;

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

  ~CallPyDecisionBuilder() override {
    Py_CLEAR(pydb_);
    Py_CLEAR(func_);
    Py_CLEAR(str_func_);
    Py_CLEAR(pysolver_);
    Py_CLEAR(pyarg_);
  }

  operations_research::Decision* Next(
      operations_research::Solver* const s) override {
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

  std::string DebugString() const override {
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


#endif  // OR_TOOLS_CONSTRAINT_SOLVER_PYTHON_PYWRAPCP_UTIL_H_
