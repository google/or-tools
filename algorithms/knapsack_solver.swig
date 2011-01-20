// Copyright (C) 2010 and onwards Google
// Author: lperron@google.com (Laurent Perron)

%include base/base.swig

// Include the file we want to wrap a first time.
#ifdef SWIGPYTHON
%{
#include "algorithms/knapsack_solver.h"
%}
#endif

namespace operations_research {
// Add conversion rules for vector<int64>.
%typemap(in) const vector<int64>& (vector<int64> temp) {
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

%typecheck(SWIG_TYPECHECK_POINTER) const vector<int64>& {
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

// Add conversion list(tuple(int)) -> vector<vector> for knapsack.
%typemap(in) const vector<vector<int64> >&
    (vector<vector<int64> > temp) {
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

%typecheck(SWIG_TYPECHECK_POINTER) const vector<vector<int64> >& {
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
}

%include "algorithms/knapsack_solver.h"
