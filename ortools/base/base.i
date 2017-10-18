// Copyright 2010-2017 Google
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

#ifdef SWIGPYTHON

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
#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "ortools/base/basictypes.h"
#include "ortools/base/python-swig.h"
%}

// Typedefs and typemaps do not interact the way one would expect.
// E.g., "typedef int int32;" alone does *not* mean that typemap
// "int * OUTPUT" also applies to "int32 * OUTPUT".  We must say
// "%apply int * OUTPUT { int32 * OUTPUT };" explicitly.  Therefore,
// all typemaps in this file operate on C++ type names, not Google
// type names.  Google typedefs are placed at the very end along with
// the necessary %apply macros.  See COPY_TYPEMAPS below for details.

%include "std_string.i"

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


// STL std::vector<T> for common types

// Get const std::vector<std::string>& "in" typemap.
%include "python/std_vector.i"
%include "python/std_map.i"
%include "python/std_set.i"
%include "python/std_list.i"

%define LIST_OUTPUT_TYPEMAP(type, py_converter)
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
%typemap(argout)
    std::vector<type>* OUTPUT,
    std::set<type>* OUTPUT,
    std::unordered_set<type>* OUTPUT {
  %append_output(list_output_helper($1, &py_converter));
}
%typemap(out) std::vector<type> {
  $result = vector_output_helper(&$1, &py_converter);
}
%typemap(out) std::vector<type>*, const std::vector<type>& {
  $result = vector_output_helper($1, &py_converter);
}
%enddef

LIST_OUTPUT_TYPEMAP(bool, PyBool_FromLong);
LIST_OUTPUT_TYPEMAP(signed char, PyInt_FromLong);
LIST_OUTPUT_TYPEMAP(short, PyInt_FromLong);
LIST_OUTPUT_TYPEMAP(unsigned short, PyInt_FromLong);
LIST_OUTPUT_TYPEMAP(int, PyInt_FromLong);
LIST_OUTPUT_TYPEMAP(unsigned int, PyLong_FromUnsignedLong);
LIST_OUTPUT_TYPEMAP(long, PyInt_FromLong);
LIST_OUTPUT_TYPEMAP(unsigned long, PyLong_FromUnsignedLong);
LIST_OUTPUT_TYPEMAP(long long, PyLong_FromLongLong);
LIST_OUTPUT_TYPEMAP(unsigned long long, PyLong_FromUnsignedLongLong);
LIST_OUTPUT_TYPEMAP(std::string, SwigString_FromString);
LIST_OUTPUT_TYPEMAP(char *, PyBytes_FromString);
LIST_OUTPUT_TYPEMAP(double, PyFloat_FromDouble);
LIST_OUTPUT_TYPEMAP(float, PyFloat_FromDouble);

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
%apply std::vector<oldtype> * OUTPUT { std::vector<newtype> * OUTPUT };
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

#endif  // SWIGPYTHON

#ifdef SWIGJAVA
%{
#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "ortools/base/basictypes.h"
%}

%include <std_string.i>

%apply const std::string & {std::string &};
%apply const std::string & {std::string *};

// std::string
%typemap(jni) std::string "jstring"
%typemap(jtype) std::string "String"
%typemap(jstype) std::string "String"
%typemap(javadirectorin) std::string "$jniinput"
%typemap(javadirectorout) std::string "$javacall"

%typemap(in) std::string
%{ if(!$input) {
     SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null std::string");
     return $null;
    }
    const char *$1_pstr = (const char *)jenv->GetStringUTFChars($input, 0);
    if (!$1_pstr) return $null;
    $1.assign($1_pstr);
    jenv->ReleaseStringUTFChars($input, $1_pstr); %}

%typemap(directorout) std::string
%{ if(!$input) {
     SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null std::string");
     return $null;
   }
   const char *$1_pstr = (const char *)jenv->GetStringUTFChars($input, 0);
   if (!$1_pstr) return $null;
   $result.assign($1_pstr);
   jenv->ReleaseStringUTFChars($input, $1_pstr); %}

%typemap(directorin,descriptor="Ljava/lang/String;") std::string
%{ $input = jenv->NewStringUTF($1.c_str()); %}

%typemap(out) std::string
%{ $result = jenv->NewStringUTF($1.c_str()); %}

%typemap(javain) std::string "$javainput"

%typemap(javaout) std::string {
    return $jnicall;
  }

%typemap(typecheck) std::string = char *;

%typemap(throws) std::string
%{ SWIG_JavaThrowException(jenv, SWIG_JavaRuntimeException, $1.c_str());
   return $null; %}

// const std::string &
%typemap(jni) const std::string & "jstring"
%typemap(jtype) const std::string & "String"
%typemap(jstype) const std::string & "String"
%typemap(javadirectorin) const std::string & "$jniinput"
%typemap(javadirectorout) const std::string & "$javacall"

%typemap(in) const std::string &
%{ if(!$input) {
     SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null std::string");
     return $null;
   }
   const char *$1_pstr = (const char *)jenv->GetStringUTFChars($input, 0);
   if (!$1_pstr) return $null;
   std::string $1_str($1_pstr);
   $1 = &$1_str;
   jenv->ReleaseStringUTFChars($input, $1_pstr); %}

%typemap(directorout,warning=SWIGWARN_TYPEMAP_THREAD_UNSAFE_MSG) const std::string &
%{ if(!$input) {
     SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null std::string");
     return $null;
   }
   const char *$1_pstr = (const char *)jenv->GetStringUTFChars($input, 0);
   if (!$1_pstr) return $null;
   /* possible thread - reentrant code problem */
   static std::string $1_str;
   $1_str = $1_pstr;
   $result = &$1_str;
   jenv->ReleaseStringUTFChars($input, $1_pstr); %}

%typemap(directorin,descriptor="Ljava/lang/String;") const std::string &
%{ $input = jenv->NewStringUTF($1.c_str()); %}

%typemap(out) const std::string &
%{ $result = jenv->NewStringUTF($1->c_str()); %}

%typemap(javain) const std::string & "$javainput"

%typemap(javaout) const std::string & {
    return $jnicall;
  }

%typemap(typecheck) const std::string & = char *;

%typemap(throws) const std::string &
%{ SWIG_JavaThrowException(jenv, SWIG_JavaRuntimeException, $1.c_str());
   return $null;
%}

%define COPY_TYPEMAPS(oldtype, newtype)
typedef oldtype newtype;
%enddef

COPY_TYPEMAPS(signed char, schar);
COPY_TYPEMAPS(int, int32);
COPY_TYPEMAPS(unsigned int, uint32);
COPY_TYPEMAPS(long long, int64);
COPY_TYPEMAPS(unsigned long long, uint64);

#undef COPY_TYPEMAPS
#endif  // SWIGJAVA

#ifdef SWIGCSHARP
%include "enumsimple.swg"
%{
#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "ortools/base/basictypes.h"
%}

%include <std_string.i>

%apply const std::string & {std::string &};
%apply const std::string & {std::string *};

%define COPY_TYPEMAPS(oldtype, newtype)
typedef oldtype newtype;
%enddef

COPY_TYPEMAPS(signed char, schar);
COPY_TYPEMAPS(int, int32);
COPY_TYPEMAPS(unsigned int, uint32);
COPY_TYPEMAPS(long long, int64);
COPY_TYPEMAPS(unsigned long long, uint64);

#undef COPY_TYPEMAPS
#endif  // SWIGCSHARP

// SWIG macros for explicit API declaration.
// Usage:
//
// %ignoreall
// %unignore SomeName;   // namespace / class / method
// %include "somelib.h"
// %unignoreall  // mandatory closing "bracket"
%define %ignoreall %ignore ""; %enddef
%define %unignore %rename("%s") %enddef
%define %unignoreall %rename("%s") ""; %enddef
