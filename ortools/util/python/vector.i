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

// TODO(user): make this SWIG file comply with the SWIG style guide.
%include "ortools/base/base.i"

%import "ortools/base/integral_types.h"

namespace operations_research {
// --------- std::vector<data> wrapping ----------

// We can't just reuse the google.i code (see LIST_OUTPUT_TYPEMAP in that
// file); for two reasons:
// 1) We want a few extra typemaps (ITI integers <-> int, for example), but
//    can't invoke google.i's LIST_OUTPUT_TYPEMAP because it's only
//    defined there. We may reuse base/swig/python-swig.cc though, since it's
//    bundled with the google.i rule.
// 2) We use a lot of function overloading, so we must add extra typechecks.
//
// Note(user): for an unknown reason, using the (handy) method PyObjAs()
// defined in base/swig/python-swig.cc seems to cause issues, so we can't
// use a generic, templated type checker.
%define PY_LIST_OUTPUT_TYPEMAP(type, checker, py_converter)
%typecheck(SWIG_TYPECHECK_POINTER) const std::vector<type>&,
                                   std::vector<type>,
                                   const std::vector<type>* {
  if (!PyTuple_Check($input) && !PyList_Check($input)) {
    $1 = 0;
  } else {
    const bool is_tuple = PyTuple_Check($input);
    const size_t size = is_tuple ? PyTuple_Size($input) : PyList_Size($input);
    size_t i = 0;
    while (i < size && checker(is_tuple ? PyTuple_GetItem($input, i)
                                        : PyList_GetItem($input, i))) {
      ++i;
    }
    $1 = i == size;
  }
}
%typemap(in) std::vector<type>(std::vector<type> temp) {
  if (!vector_input_helper($input, &temp, PyObjAs<type>)) {
    if (!PyErr_Occurred())
      SWIG_Error(SWIG_TypeError, "sequence(type) expected");
    return NULL;
  }
  $1 = temp;
}
%typemap(in) const std::vector<type>& (std::vector<type> temp),
             const std::vector<type>* (std::vector<type> temp) {
  if (!vector_input_helper($input, &temp, PyObjAs<type>)) {
    if (!PyErr_Occurred())
      SWIG_Error(SWIG_TypeError, "sequence(type) expected");
    return NULL;
  }
  $1 = &temp;
}
%typemap(in,numinputs=0)
 std::vector<type>* OUTPUT (std::vector<type> temp),
 std::unordered_set<type>* OUTPUT (std::unordered_set<type> temp),
 std::set<type>* OUTPUT (std::set<type> temp) {
  $1 = &temp;
}
%typemap(argout) std::vector<type>* OUTPUT, std::set<type>* OUTPUT, std::unordered_set<type>* OUTPUT {
  %append_output(list_output_helper($1, &py_converter));
}
%typemap(out) std::vector<type> {
  $result = vector_output_helper(&$1, &py_converter);
}
%typemap(out) std::vector<type>*, const std::vector<type>& {
  $result = vector_output_helper($1, &py_converter);
}


%apply const std::vector<type>& { const std::vector<type>& }
%apply const std::vector<type>* { const std::vector<type>* }
%apply std::vector<type>* { std::vector<type>* }
%apply std::vector<type> { std::vector<type> }

%apply std::vector<type>* OUTPUT { std::vector<type>* OUTPUT }
%apply std::set<type>* OUTPUT { std::set<type>* OUTPUT }

%enddef  // PY_LIST_OUTPUT_TYPEMAP

PY_LIST_OUTPUT_TYPEMAP(int, PyInt_Check, PyInt_FromLong);
PY_LIST_OUTPUT_TYPEMAP(long long, SwigPyIntOrLong_Check, PyInt_FromLong);
PY_LIST_OUTPUT_TYPEMAP(double, PyFloat_Check, PyFloat_FromDouble);

// Add conversion list(tuple(int)) -> std::vector<std::vector>.
// TODO(user): see if we can also get rid of this and utilize already
// existing code.
%define PY_LIST_LIST_INPUT_TYPEMAP(type, checker)
%typemap(in) const std::vector<std::vector<type> >&
    (std::vector<std::vector<type> > temp) {
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
        bool success = PyObjAs<type>(is_tuple ?
                                     PyTuple_GetItem(tuple, j) :
                                     PyList_GetItem(tuple, j), &temp[i][j]);
        if (!success) {
          SWIG_fail;
        }
      }
    }
  }
  $1 = &temp;
}

%typecheck(SWIG_TYPECHECK_POINTER) const std::vector<std::vector<type> >& {
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
          if (!checker(entry)) {
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

%apply const std::vector<std::vector<type> >& { const std::vector<std::vector<type> >& }
%enddef  // PY_LIST_LIST_INPUT_TYPEMAP

PY_LIST_LIST_INPUT_TYPEMAP(long long, SwigPyIntOrLong_Check);
}  // namespace operations_research
