// Copyright 2010 Google
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

#pragma SWIG nowarn=312,451,454,503,362

// 312 suppresses warnings about nested classes that SWIG doesn't currently
// support.
// 451 suppresses warnings about setting const char * variable may leak memory.
// 454 suppresses setting global ptr/ref variables may leak memory warning
// 503 suppresses warnings about identifiers that SWIG can't wrap without a
// rename.  For example, an operator< in a class without a rename.
// 362 is similar to 503 but for operator=.

%include <typemaps.i>
%include <exception.i>

// Add a char* cast to the SWIG 1.3.21 typemaps to remove a compiler warning.
%typemap(constcode) long long {
  PyObject *object = PyLong_FromLongLong($value);
  if (object) {
    int rc = PyDict_SetItemString(d, (char*) "$symname", object);
    Py_DECREF(object);
  }
}
%typemap(constcode) unsigned long long {
  PyObject *object = PyLong_FromUnsignedLongLong($value);
  if (object) {
    int rc = PyDict_SetItemString(d, (char*) "$symname", object);
    Py_DECREF(object);
  }
}

%{
#include <vector>
#ifdef __GNUC__
#include <ext/hash_map>
#include <ext/hash_set>
#else
#include <hash_map>
#include <hash_set>
#endif
#include <set>
#include <map>
#include <string>
#include "base/basictypes.h"

%}

// Typedefs and typemaps do not interact the way one would expect.
// E.g., "typedef int int32;" alone does *not* mean that typemap
// "int * OUTPUT" also applies to "int32 * OUTPUT".  We must say
// "%apply int * OUTPUT { int32 * OUTPUT };" explicitly.  Therefore,
// all typemaps in this file operate on C++ type names, not Google
// type names.  Google typedefs are placed at the very end along with
// the necessary %apply macros.  See COPY_TYPEMAPS below for details.

// void *

// std::string

namespace std {
  class string;

  %typemap(typecheck) string = char *;
  %typemap(typecheck) const string & = char *;

  %typemap(in) string {
    char * buf;
    Py_ssize_t len;
    if (PyString_AsStringAndSize($input, &buf, &len) == -1)
      return NULL;
    $1 = string(buf, len);
  }

  %typemap(in) const string & (string temp) {
    char * buf;
    Py_ssize_t len;
    if (PyString_AsStringAndSize($input, &buf, &len) == -1)
      return NULL;
    temp = string(buf, len);
    $1 = &temp;
  }

  %typemap(out) string {
    $result = PyString_FromStringAndSize($1.data(), $1.size());
  }

  %typemap(out) const string & {
    $result = PyString_FromStringAndSize($1->data(), $1->size());
  }

  %typemap(in, numinputs = 0) string * OUTPUT (string temp) {
    temp = string();
    $1 = &temp;
  }

  %typemap(argout, fragment = "t_output_helper") string * OUTPUT {
    $result = t_output_helper(
      $result,
      PyString_FromStringAndSize($1->data(), $1->length()));
  }

  %typemap(varout) string {
    $result = PyString_FromStringAndSize($1.data(), $1.size());
  }

}

%apply const std::string & {std::string &};
%apply const std::string & {std::string *};

// For some versions of the C++ compiler, string is not the same as
// std::string.

class string;

%typemap(typecheck) string = char *;
%typemap(typecheck) const string & = char *;

%typemap(in) string {
  char * buf;
  Py_ssize_t len;
  if (PyString_AsStringAndSize($input, &buf, &len) == -1)
    return NULL;
  $1 = string(buf, len);
}

%typemap(in) const string & (string temp) {
  char * buf;
  Py_ssize_t len;
  if (PyString_AsStringAndSize($input, &buf, &len) == -1)
    return NULL;
  temp = string(buf, len);
  $1 = &temp;
}

%typemap(out) string {
  $result = PyString_FromStringAndSize($1.data(), $1.size());
}

%typemap(out) const string & {
  $result = PyString_FromStringAndSize($1->data(), $1->size());
}

%typemap(in, numinputs = 0) string * OUTPUT (string temp) {
  temp = string();
  $1 = &temp;
}

%typemap(argout, fragment = "t_output_helper") string * OUTPUT {
  $result = t_output_helper(
    $result,
    PyString_FromStringAndSize($1->data(), $1->length()));
}

%typemap(varout) string {
  $result = PyString_FromStringAndSize($1.data(), $1.size());
}

%apply const string & {string &};
%apply const string & {string *};

// Support for those popular buffer-pointer/length input pairs
%typemap(in) (void *INPUT, unsigned int LENGTH) (Py_ssize_t len) {
  if (PyObject_AsReadBuffer($input, (const void**) &$1, &len) != 0)
    return NULL;
  if (((Py_ssize_t)($2_type)len) != len) {
    SWIG_exception(SWIG_ValueError, "input data too large");
  }
  $2 = ($2_type)len;
}

%typemap(in) (void *INPUT, uint64 LENGTH) (Py_ssize_t len) {
  if (PyObject_AsReadBuffer($input, (const void**) &$1, &len) != 0)
    return NULL;
  $2 = len;
}

// char **

%typemap(in, numinputs=0) char ** OUTPUT {
}
%typemap(argout, fragment="t_output_helper") char ** OUTPUT {
  char* tmpstr = NULL;
  if ($1 != NULL) tmpstr = *$1;
  $result = t_output_helper($result, PyString_FromString(tmpstr));
}


// STL vector<T> for common types

// Get const std::vector<std::string>& "in" typemap.
%include "python/std_vector.i"
%include "python/std_map.i"
%include "python/std_set.i"
%include "python/std_list.i"

// Helper function for turning a C++ vector<T> (or any other instance that
// supports the vector<T>-like iterator interface) into a Python list of Ts.
// The converter function converts a C++ object of type const T or const T&
// into the corresponding Python object.
%{
template<class T, class TR>
static PyObject* list_output_helper(const T* vec,
                                    PyObject* (*converter)(const TR x)) {
  if (vec == NULL) {
    // Return a nice out-of-band value if the pointer is NULL.
    Py_INCREF(Py_None);
    return Py_None;
  }

  PyObject* const lst = PyList_New(vec->size());
  if (lst == NULL)
    return NULL;

  int i = 0;
  for (typename T::const_iterator pt = vec->begin();
       pt != vec->end();
       ++pt) {
    PyObject* const obj = converter(*pt);
    if (!obj) {
      Py_DECREF(lst);
      return NULL;
    }
    PyList_SET_ITEM(lst, i++, obj);
  }

  return lst;
}

template<class T, class TR>
static PyObject* vector_output_helper(const std::vector<T>* vec,
                                      PyObject* (*converter)(const TR x)) {
  return list_output_helper(vec, converter);
}

static inline PyObject* PyString_FromCXXString(const std::string & x) {
  return PyString_FromStringAndSize(x.data(), x.size());
}
%}

%define LIST_OUTPUT_TYPEMAP(type, py_converter)
%typemap(in,numinputs=0) vector<type> * OUTPUT(vector<type> temp) {
  $1 = &temp;
}
%typemap(argout, fragment = "t_output_helper") vector<type> * OUTPUT {
  $result = t_output_helper($result, list_output_helper($1, &py_converter));
}
%typemap(in,numinputs=0) set<type> * OUTPUT(set<type> temp) {
  $1 = &temp;
}
%typemap(argout, fragment = "t_output_helper") set<type> * OUTPUT {
  $result = t_output_helper($result, list_output_helper($1, &py_converter));
}
%enddef

LIST_OUTPUT_TYPEMAP(bool, PyInt_FromLong);
LIST_OUTPUT_TYPEMAP(signed char, PyInt_FromLong);
LIST_OUTPUT_TYPEMAP(short, PyInt_FromLong);
LIST_OUTPUT_TYPEMAP(unsigned short, PyInt_FromLong);
LIST_OUTPUT_TYPEMAP(int, PyInt_FromLong);
LIST_OUTPUT_TYPEMAP(unsigned int, PyLong_FromUnsignedLong);
LIST_OUTPUT_TYPEMAP(long, PyInt_FromLong);
LIST_OUTPUT_TYPEMAP(unsigned long, PyLong_FromUnsignedLong);
LIST_OUTPUT_TYPEMAP(long long, PyLong_FromLongLong);
LIST_OUTPUT_TYPEMAP(unsigned long long, PyLong_FromUnsignedLongLong);
LIST_OUTPUT_TYPEMAP(std::string, PyString_FromCXXString);
LIST_OUTPUT_TYPEMAP(char *, PyString_FromString);
LIST_OUTPUT_TYPEMAP(double, PyFloat_FromDouble);

#undef LIST_OUTPUT_TYPEMAP

%apply bool * OUTPUT            { bool * OUTPUT2 };
%apply int * OUTPUT             { int * OUTPUT2 };
%apply short * OUTPUT           { short * OUTPUT2 };
%apply long * OUTPUT            { long * OUTPUT2 };
%apply unsigned * OUTPUT        { unsigned * OUTPUT2 };
%apply unsigned short * OUTPUT  { unsigned short * OUTPUT2 };
%apply unsigned long * OUTPUT   { unsigned long * OUTPUT2 };
%apply unsigned char * OUTPUT   { unsigned char * OUTPUT2 };
%apply signed char * OUTPUT     { signed char * OUTPUT2 };
%apply double * OUTPUT          { double * OUTPUT2 };
%apply float * OUTPUT           { float * OUTPUT2 };
%apply char ** OUTPUT           { char ** OUTPUT2 };

// these are copied from basictypes.h

%define COPY_TYPEMAPS(oldtype, newtype)
typedef oldtype newtype;
%apply oldtype * OUTPUT { newtype * OUTPUT };
%apply oldtype & OUTPUT { newtype & OUTPUT };
%apply oldtype * INPUT { newtype * INPUT };
%apply oldtype & INPUT { newtype & INPUT };
%apply oldtype * INOUT { newtype * INOUT };
%apply oldtype & INOUT { newtype & INOUT };
%apply vector<oldtype> * OUTPUT { vector<newtype> * OUTPUT };
%enddef

COPY_TYPEMAPS(signed char, schar);
COPY_TYPEMAPS(short, int16);
COPY_TYPEMAPS(unsigned short, uint16);
COPY_TYPEMAPS(int, int32);
COPY_TYPEMAPS(unsigned int, uint32);
COPY_TYPEMAPS(long long, int64);
COPY_TYPEMAPS(unsigned long long, uint64);

COPY_TYPEMAPS(unsigned int, size_t);
COPY_TYPEMAPS(unsigned int, mode_t);
COPY_TYPEMAPS(long, time_t);
COPY_TYPEMAPS(uint64, Fprint);

#undef COPY_TYPEMAPS

%apply (void * INPUT, unsigned int LENGTH)
     { (void * INPUT, uint32 LENGTH) }
%apply (void * INPUT, uint64 LENGTH)
     { (void * INPUT, size_t LENGTH) };

%apply (void * INPUT, unsigned int LENGTH)
     { (const void * INPUT, unsigned int LENGTH) };
%apply (void * INPUT, unsigned int LENGTH)
     { (const void * INPUT, uint32 LENGTH) };
%apply (void * INPUT, uint64 LENGTH)
     { (const void * INPUT, size_t LENGTH) };

%apply (void * INPUT, unsigned int LENGTH)
     { (const char * INPUT, unsigned int LENGTH) };
%apply (void * INPUT, unsigned int LENGTH)
     { (const char * INPUT, uint32 LENGTH) };
%apply (void * INPUT, uint64 LENGTH)
     { (const char * INPUT, size_t LENGTH) };

// We accept either python ints or longs for uint64 arguments.
%typemap(in) uint64 {
  if (PyInt_Check($input)) {
    $1 = static_cast<uint64>(PyInt_AsLong($input));
  } else if (PyLong_Check($input)) {
    $1 = static_cast<uint64>(PyLong_AsUnsignedLongLong($input));
  } else {
    SWIG_exception(SWIG_TypeError,
                   "int or long value expected for argument \"$1_name\"")
  }
}

// When a method returns a pointer or reference to a subobject of the
// receiver, it should be marked with SWIG_RETURN_POINTER_TO_SUBOBJECT.
// This ensures that the wrapper of the subobject keeps the wrapper of
// the parent object alive, which indirectly keeps the subobject alive.
%define SWIG_RETURN_POINTER_TO_SUBOBJECT(cpp_method, py_method)
%feature("shadow") cpp_method %{
  def py_method(*args):
    result = $action(*args)
    if result is not None:
      result.keepalive = args[0]
    return result
%}
%enddef
