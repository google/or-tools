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

#pragma SWIG nowarn=312,325,451,454,503,362,509

// 312 suppresses warnings about (unnamed) nested classes that SWIG doesn't
// currently support.
// 325 suppresses warnings about named nested classes that SWIG doesn't
// currently support.
// 451 suppresses warnings about setting const char * variable may leak memory.
// 454 suppresses setting global ptr/ref variables may leak memory warning
// 503 suppresses warnings about identifiers that SWIG can't wrap without a
// rename.  For example, an operator< in a class without a rename.
// 362 is similar to 503 but for operator=.
// 509 suppresses warnings about shadowing methods with SWIG-specific ones, or
// when multiple wrapped methods map to the same wrapping method.

%include <typemaps.i>
%include <exception.i>

#ifdef SWIGPYTHON

// Add a char* cast to the SWIG 1.3.21 typemaps to remove a compiler warning.
%typemap(constcode) long long {
  PyObject *object = PyLong_FromLongLong($value);
  if (object) {
    int rc = PyDict_SetItemString(d, (char*) "$symname", object);
    Py_DECREF(object);
    // TODO(user): Check rc for error.
  }
}
%typemap(constcode) unsigned long long {
  PyObject *object = PyLong_FromUnsignedLongLong($value);
  if (object) {
    int rc = PyDict_SetItemString(d, (char*) "$symname", object);
    Py_DECREF(object);
    // TODO(user): Check rc for error.
  }
}

#endif

// Presence of deprecated attribute in protocol buffer will cause compilation
// by swig to break. This overrides the deprecation warning macro.
%define DEPRECATED_PROTOBUF_FIELD
%enddef

%{
#include <memory>
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

#ifdef SWIGJAVA
%include "enumsimple.swg"

%typemap(jni) void* INOUT "jbyteArray"
%typemap(jtype) void* INOUT "byte[]"
%typemap(jstype) void* INOUT "byte[]"
%typemap(javain) void* INOUT "$javainput"
%typemap(in) void* INOUT {
  $1 = (void *) JCALL2(GetByteArrayElements, jenv, $input, 0);
}
%typemap(freearg) void* INOUT {
  JCALL3(ReleaseByteArrayElements, jenv, $input, (jbyte *) $1, 0);
}
%apply void* INOUT { const void* INOUT };
%typemap(freearg) const void* INOUT {
  JCALL3(ReleaseByteArrayElements, jenv, $input, (jbyte *) $1, JNI_ABORT);
}
%typemap(argout) void* OUTPUT { }

#endif

// DocId

#ifdef SWIGPYTHON

%typemap(in) DocId {
  if (!PyLong_Check($input)) {
    PyErr_SetString(PyExc_TypeError, "Expecting a pylong (DocId)");
    return NULL;
  }
  unsigned long long docid = PyLong_AsUnsignedLongLong($input);
  if (docid == -1 && PyErr_Occurred()) {
    return NULL;
  }
  $1 = DocId(docid);
}
%typemap(in) DocId *(DocId temp) {
  if (!PyLong_Check($input)) {
    PyErr_SetString(PyExc_TypeError, "Expecting a pylong (DocId)");
    return NULL;
  }
  unsigned long long docid = PyLong_AsUnsignedLongLong($input);
  if (docid == -1 && PyErr_Occurred()) {
    return NULL;
  }
  temp = DocId(docid);
  $1 = &temp;
}
%typemap(out) DocId {
  char buf[200];
  sprintf(buf, "%llu", DocidForPrintf($1));
  $result = PyLong_FromString(buf, NULL, 0);
}
%typemap(out) DocId * {
  char buf[200];
  sprintf(buf, "%llu", DocidForPrintf(*$1));
  $result = PyLong_FromString(buf, NULL, 0);
}

#endif

#ifdef SWIGJAVA

%typemap(jni)    DocId, const DocId & "jlong"
%typemap(jtype)  DocId, const DocId & "long"
%typemap(jstype) DocId, const DocId & "long"
%typemap(javain) DocId, const DocId & "$javainput"
%typemap(javaout) DocId, const DocId & "{ return $jnicall; }"
%typemap(in)     DocId, const DocId & {
  $1 = DocId($input);
}
%typemap(out)    DocId, const DocId & {
  $result = DocidForPrintf($1);
}

#endif

// std::string

#ifdef SWIGJAVA
%include <std_string.i>
#endif

namespace std {
  class string;

  %typemap(typecheck) string = char *;
  %typemap(typecheck) const string & = char *;

#ifdef SWIGJAVA

  %typemap(jni) string * OUTPUT "jobjectArray"
  %typemap(jtype) string * OUTPUT "String[]"
  %typemap(jstype) string * OUTPUT "String[]"
  %typemap(javain) string * OUTPUT "$javainput"

  %typemap(in) string * OUTPUT (string temp) {
    $1 = &temp;
  }

  %typemap(argout) string * OUTPUT {
    // We construct the returned String from a byte array instead of
    // using NewStringUTF so that we can properly deal with null
    // characters (they show up in protocol buffers).
    jclass string_class = JCALL1(FindClass, jenv, "java/lang/String");
    jmethodID string_init = JCALL3(GetMethodID, jenv, string_class, \
                                   "<init>", "([BLjava/lang/String;)V");
    jbyteArray bytes = JCALL1(NewByteArray, jenv, $1->size());
    JCALL4(SetByteArrayRegion, jenv, bytes, 0, $1->size(), \
           (jbyte *) $1->data());
    jstring charcode = JCALL1(NewStringUTF, jenv, "UTF-8");
    jobject jstr = JCALL4(NewObject, jenv, string_class, string_init, bytes,
                          charcode);
    JCALL1(DeleteLocalRef, jenv, bytes);
    JCALL3(SetObjectArrayElement, jenv, $input, 0, jstr);
  }

  %typemap(freearg) string * OUTPUT {
  }

#endif
}

class string;

%typemap(typecheck) string = char *;
%typemap(typecheck) const string & = char *;

#ifdef SWIGPYTHON
// Swig2 no longer protects char* from being NULL, so we must use explicit check
// like %apply Pointer NONNULL {const char *};
%typemap(check) Pointer NONNULL {
  if ($1 == NULL) {
    SWIG_exception(SWIG_TypeError, "'$1_name' must not be None");
  }
}

// We no longer provide typemaps for std:: namespace.
#define USE_EXPLICIT_STD_TYPEMAPS

%{
#include "base/python-swig.cc"
%}

%typemap(in) string {
  if (!PyObjAs<string>($input, &$1)) return NULL;
}

%typemap(in) const string& (string temp) {
  if (!PyObjAs<string>($input, &temp)) return NULL;
  $1 = &temp;
}

%typemap(out) string {
  $result = PyString_FromStringAndSize($1.data(), $1.size());
}

%typemap(out) const string& {
  $result = PyString_FromStringAndSize($1->data(), $1->size());
}

%typemap(in, numinputs = 0) string* OUTPUT (string temp) {
  $1 = &temp;
}

%typemap(argout) string * OUTPUT {
  PyObject *str = PyString_FromStringAndSize($1->data(), $1->length());
  if (!str) SWIG_fail;
  %append_output(str);
}

%typemap(argout) string* INOUT = string* OUTPUT;

%typemap(varout) string {
  $result = PyString_FromStringAndSize($1.data(), $1.size());
}
#endif  // SWIGPYTHON

#ifdef SWIGJAVA

%typemap(jni) string "jstring"
%typemap(jtype) string "String"
%typemap(jstype) string "String"
%typemap(javadirectorin) string "$jniinput"
%typemap(javadirectorout) string "$javacall"

%typemap(in) string %{
  if($input) {
    const char *pstr = (const char *)jenv->GetStringUTFChars($input, 0);
    if (!pstr) return $null;
    $1 =  string(pstr);
    jenv->ReleaseStringUTFChars($input, pstr);
  }
  else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null string");
    return $null;
  }
%}

%typemap(out) string %{
  $result = jenv->NewStringUTF($1.c_str());
%}

%typemap(javain) string "$javainput"

%typemap(javaout) string {
    return $jnicall;
}

%typemap(jni) const string & "jstring"
%typemap(jtype) const string & "String"
%typemap(jstype) const string & "String"
%typemap(javadirectorin) const string & "$jniinput"
%typemap(javadirectorout) const string & "$javacall"

%typemap(in) const string & (string temp) %{
  $1 = NULL;
  if($input) {
    const char *pstr = (const char *)jenv->GetStringUTFChars($input, 0);
    if (!pstr) return $null;
    temp = string(pstr);
    $1 = &temp;
    jenv->ReleaseStringUTFChars($input, pstr);
  }
  else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null string");
    return $null;
  }
%}

%typemap(out) const string & %{
  $result = jenv->NewStringUTF($1->c_str());
%}

%typemap(javain) const string & "$javainput"

%typemap(javaout) const string & {
    return $jnicall;
}

%typemap(jni) string * OUTPUT "jobjectArray"
%typemap(jtype) string * OUTPUT "String[]"
%typemap(jstype) string * OUTPUT "String[]"
%typemap(javain) string * OUTPUT "$javainput"

%typemap(in) string * OUTPUT (string temp) {
  $1 = &temp;
}

%typemap(argout) string * OUTPUT {
  // We construct the returned String from a byte array instead of
  // using NewStringUTF so that we can properly deal with null
  // characters (they show up in protocol buffers).
  jclass string_class = JCALL1(FindClass, jenv, "java/lang/String");
  jmethodID string_init = JCALL3(GetMethodID, jenv, string_class, \
                                 "<init>", "([BLjava/lang/String;)V");
  jbyteArray bytes = JCALL1(NewByteArray, jenv, $1->size());
  JCALL4(SetByteArrayRegion, jenv, bytes, 0, $1->size(), \
         (jbyte *) $1->data());
  jstring charcode = JCALL1(NewStringUTF, jenv, "UTF-8");
  jobject jstr = JCALL4(NewObject, jenv, string_class, string_init, bytes,
                        charcode);
  JCALL1(DeleteLocalRef, jenv, bytes);
  JCALL3(SetObjectArrayElement, jenv, $input, 0, jstr);
}

%typemap(freearg) string * OUTPUT {
}

#endif  // SWIGJAVA

%apply const string & {string &};
%apply const string & {string *};

#ifdef SWIGPYTHON

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

%typemap(in, numinputs=0) char ** OUTPUT (char *temp) "$1 = &temp;"
%typemap(argout) char ** OUTPUT {
%#if PY_VERSION_HEX >= 0x03030000  // Py3.3+
  %append_output(*$1? PyUnicode_FromString(*$1) : PyUnicode_New(0, 0));
%#else
  %append_output(*$1? PyBytes_FromString(*$1)
                    : PyBytes_FromStringAndSize(NULL, 0));
%#endif
}

// STL std::vector<T> for common types

%typecheck(SWIG_TYPECHECK_STRING_ARRAY)
    std::vector<string>, const std::vector<string>&, std::vector<string>*,
    set<string>, const set<string>&, set<string>* {
  PyObject *it = PyObject_GetIter($input);
  if (it != NULL) {
    Py_DECREF(it);
    $1 = 1;
  } else {
    $1 = 0;
  }
}

template<class T> class std::vector {
  %typemap(in) const std::vector<T>& (std::vector<T> temp) {
    if (!vector_input_wrap_helper($input, &temp, $descriptor(T*))) SWIG_fail;
    $1 = &temp;
  }
};

// Partial specialization for std::vectors of pointers.
template<class T> class std::vector<T*> {
  %typemap(in) const std::vector<T*>& (std::vector<T*> temp) {
    if (!vector_input_wrap_helper($input, &temp, $descriptor(T*))) SWIG_fail;
    $1 = &temp;
  }
  %typemap(in, numinputs=0) std::vector<T*>* OUTPUT (std::vector<T*> temp) {
    $1 = &temp;
  }
  %typemap(argout) std::vector<T*>* OUTPUT {
    %append_output(vector_output_wrap_helper($1, $descriptor(T*)));
  }
  %typemap(in, numinputs=0) std::vector<T*>* OUTPUT_NEW = std::vector<T*>* OUTPUT;
  %typemap(argout) std::vector<T*>* OUTPUT_NEW {
    %append_output(vector_output_wrap_helper($1, $descriptor(T*), true));
  }
  %typemap(out) std::vector<T*> {
    $result = vector_output_wrap_helper(&$1,$descriptor(T*));
  }
  %typemap(out) const std::vector<T*>& {
    $result = vector_output_wrap_helper($1,$descriptor(T*));
  }
};

/*
// Pre-works for getting rid of explicit OUTPUT typemaps.
// TODO(user): Fix OUTPUT typemaps into defaults.
template<class T> class std::vector<T> {
%typemap(in) const std::vector<T>& (std::vector<T> temp) {
  if (!vector_input_helper($input, &temp, PyObjAs<T>)) return NULL;
  $1 = &temp;
}
%typemap(out) std::vector<T> {
  $result = vector_output_helper(&$1, PyObjFrom<T>);
}
%typemap(in, numinputs=0) std::vector<T>* (std::vector<T> temp) {}
%typemap(argout) std::vector<T>* {
  %append_output(vector_output_helper($1, PyObjFrom<T>));
}
};
*/
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
 hash_set<type>* OUTPUT (hash_set<type> temp),
 set<type>* OUTPUT (set<type> temp) {
  $1 = &temp;
}
%typemap(argout) std::vector<type>* OUTPUT, set<type>* OUTPUT, hash_set<type>* OUTPUT {
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
LIST_OUTPUT_TYPEMAP(string, SwigString_FromString);
LIST_OUTPUT_TYPEMAP(char *, PyBytes_FromString);
LIST_OUTPUT_TYPEMAP(double, PyFloat_FromDouble);
LIST_OUTPUT_TYPEMAP(float, PyFloat_FromDouble);

#undef LIST_OUTPUT_TYPEMAP
/*
// Backward compatibility.
// TODO(user): Remove %apply OUTPUT from .swig files.
%apply std::vector<string>* {std::vector<string>* OUTPUT};
%apply std::vector<bool>*   {std::vector<bool>*   OUTPUT};
%apply std::vector<int>*    {std::vector<int>*    OUTPUT};
%apply set<string>*    {set<string>*    OUTPUT};
%apply hash_set<string>* {hash_set<string>* OUTPUT};
*/

#endif  // SWIGPYTHON

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

#ifdef SWIGPYTHON

%apply char ** OUTPUT           { char ** OUTPUT2 };

#endif

// these are copied from basictypes.h

%define COPY_TYPEMAPS(oldtype, newtype)
typedef oldtype newtype;
%apply oldtype * OUTPUT { newtype * OUTPUT };
%apply oldtype & OUTPUT { newtype & OUTPUT };
%apply oldtype * INPUT { newtype * INPUT };
%apply oldtype & INPUT { newtype & INPUT };
%apply oldtype * INOUT { newtype * INOUT };
%apply oldtype & INOUT { newtype & INOUT };
#ifdef SWIGPYTHON
%apply std::vector<oldtype> * OUTPUT { std::vector<newtype> * OUTPUT };
#endif
%enddef

COPY_TYPEMAPS(signed char, schar);
COPY_TYPEMAPS(unsigned char, uint8);
COPY_TYPEMAPS(short, int16);
COPY_TYPEMAPS(unsigned short, uint16);
COPY_TYPEMAPS(int, int32);
COPY_TYPEMAPS(unsigned int, uint32);
COPY_TYPEMAPS(long long, int64);
COPY_TYPEMAPS(unsigned long long, uint64);

// Since we can't use __SIZE_TYPE__ directly (not defined in SWIG)...
#ifdef ARCH_PIII
COPY_TYPEMAPS(unsigned int, size_t);
#else
COPY_TYPEMAPS(unsigned long, size_t);
#endif
//COPY_TYPEMAPS(unsigned short, mode_t); makes files/srcfs.swig fail
COPY_TYPEMAPS(unsigned int, mode_t);
COPY_TYPEMAPS(long, time_t);
COPY_TYPEMAPS(uint64, Fprint);

#undef COPY_TYPEMAPS

#ifdef SWIGPYTHON  // not yet defined for others

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
// This accepts negative Python int values as twos compliment but not
// negative Python long values.  Horrible? Yes!
// Some sad code may depends on at least the acceptance of negative ints
// by now so undoing it will take some care as its own CL.  This code also
// ignores errors and leaves the associated python exception unhandled. :(
%typemap(in) uint64 {
  // TODO(user): Check if another implementation
  // from hosting/images/util/image-hosting-utils.swig is better. May be not.
%#if PY_MAJOR_VERSION < 3
  if (PyInt_Check($input)) {
    $1 = static_cast<uint64>(PyInt_AsLong($input));
  } else
%#endif
  if (PyLong_Check($input)) {
    $1 = static_cast<uint64>(PyLong_AsUnsignedLongLong($input));
  } else {
    SWIG_exception(SWIG_TypeError,
                   "int or long value expected for argument \"$1_name\"");
  }
  if (PyErr_Occurred()) return NULL;
}
%typemap(in) uint32 {
  if (!PyIndex_Check($input)) {  // Matches int and long, not float.
    SWIG_exception(SWIG_TypeError, "integer required for argument \"$1_name\"");
  }

  // This Python C API accepts all Python numeric types as input.
  int64 oversized_value = (int64) PyLong_AsLongLong($input);
  if (oversized_value == -1 && PyErr_Occurred()) {
    SWIG_fail;
  }
  if (oversized_value < ((int64)-0x7fffffff) - 1 ||
      oversized_value > 0xffffffff) {
    PyErr_SetString(PyExc_OverflowError,
                    "int does not fit in 32 bits for argument \"$1_name\"");
    SWIG_fail;
  }
  $1 = (uint32) oversized_value;
}


// SWIG 2.0.4's default generated SWIG_AsVal_float does not let +inf, -inf or
// NaN through the range check properly.  It also raised OverflowError if the
// value was too large or small for a 32-bit float.  All of these behaviors
// are undesirable.  Fixed here.
// TODO(user): A generic version of this for all languages is needed.
%typecheck(SWIG_TYPECHECK_FLOAT) float {
  $1 = PyFloat_Check($input)? 1: SwigPyIntOrLong_Check($input)? 1: 0;
}
%typemap(in) float {
  double full_value;
  if (PyFloat_Check($input)) {
    full_value = PyFloat_AsDouble($input);
%#if PY_MAJOR_VERSION < 3
  } else if (PyInt_Check($input)) {
    full_value = PyInt_AsLong($input);
%#endif
  } else if (PyLong_Check($input)) {
    full_value = PyLong_AsDouble($input);
    if (PyErr_Occurred()) {
      SWIG_fail;
    }
  } else {
    SWIG_exception(SWIG_TypeError, "float or int expected");
  }
  $1 = full_value;
}
#endif  // SWIGPYTHON

// The SWIGged version of an abstract class must be concrete if any methods
// return objects of the abstract type.  See, for example, "File" which is
// the return type of InputBuffer::GetFile().  The class will still be
// abstract if its constructor is protected -- SWIG can't call non-public
// methods, and neither can arbitrary C++ code.

#define ABSTRACT { }

// in SWIG, we want to ignore flag declarations

#define DECLARE_PRODUCT_bool(x, y, z)
#define DECLARE_PRODUCT_int32(x, y, z)
#define DECLARE_PRODUCT_int64(x, y, z)
#define DECLARE_PRODUCT_double(x, y, z)
#define DECLARE_PRODUCT_string(x, y, z)

#define DECLARE_bool(x)
#define DECLARE_int32(x)
#define DECLARE_int64(x)
#define DECLARE_double(x)
#define DECLARE_string(x)

// SWIG doesn't like C++ attribute decorations.
#define MUST_USE_RESULT

// When a method returns a pointer or reference to a subobject of the
// receiver, it should be marked with SWIG_RETURN_POINTER_TO_SUBOBJECT.
// This ensures that the wrapper of the subobject keeps the wrapper of
// the parent object alive, which indirectly keeps the subobject alive.
#ifdef SWIGPYTHON
%define SWIG_RETURN_POINTER_TO_SUBOBJECT(cpp_method, py_method)
%feature("shadow") cpp_method %{
  def py_method(*args):
    result = $action(*args)
    if result is not None:
      result.keepalive = args[0]
    return result
%}
%enddef
#else
#define SWIG_RETURN_POINTER_TO_SUBOBJECT(cpp_method, py_method)
#endif  // PYTHON

// the following, plus some magic in the Makefile, causes us to load
// the appropriate shared library without having to do anything manually.

#ifdef SWIG_JAVA_SO
SWIG_JAVA_SO
#endif


#ifdef SWIGJAVA

%typemap(in) std::vector<string>* OUTPUT (std::vector<string> cVec){
  // the input is a java Std::Vector object
  // $1 is the argument passed to c version act.
  $1 = &cVec;
}

%typemap(jni) std::vector<string>* OUTPUT "jobject"
%typemap(jtype) std::vector<string>* OUTPUT "java.util.List<String>"
%typemap(jstype) std::vector<string>* OUTPUT "java.util.List<String>"
%typemap(javain) std::vector<string>* OUTPUT "$javainput"

%typemap(argout) std::vector<string>* OUTPUT {
  // convert std::vector<string> to a Java List.
  // $1 is a c++ object of std::vector<string>
  jclass jstd::vector =  JCALL1(GetObjectClass, jenv, $input);
  if (jstd::vector == 0) {
        return $null;
  }
  jmethodID jaddID = JCALL3(GetMethodID, jenv, jstd::vector, "add", "(Ljava/lang/Object;)Z");
  if (jaddID == 0) {
        return $null;
  }

  for (std::vector<string>::iterator it = ($1)->begin(); it != ($1)->end(); ++it) {
    // create a Java string object
    jstring temp_string = JCALL1(NewStringUTF, jenv, (it)->c_str());
    if (temp_string == NULL) {
      return $null;
    }
    // put the string object into the std::vector object by calling List.add()
    JCALL3(CallBooleanMethod, jenv, $input, jaddID, temp_string);
    if (JCALL0(ExceptionCheck, jenv)) {
         return $null;
    }
    JCALL1(DeleteLocalRef, jenv, temp_string);
  }
}

%typemap(in)  std::vector<bool>* OUTPUT (std::vector<bool> cVec){
  // the input is a java  std::vector object
  // $1 is the argument passed to c version act.
  $1 = &cVec;
}

%typemap(jni)  std::vector<bool>* OUTPUT "jobject"
%typemap(jtype)  std::vector<bool>* OUTPUT "java.util.List<Boolean>"
%typemap(jstype)  std::vector<bool>* OUTPUT "java.util.List<Boolean>"
%typemap(javain)  std::vector<bool>* OUTPUT "$javainput"

%typemap(argout) std::vector<bool>* OUTPUT {
  // convert std::vector<bool> to a Java List.
  // $1 is a c++ object of std::vector<bool>
  jclass jstd::vector = JCALL1(GetObjectClass, jenv, $input);
  if (jstd::vector == 0) {
    return $null;
  }
  jmethodID jaddID = JCALL3(GetMethodID, jenv, jstd::vector, "add",
      "(Ljava/lang/Object;)Z");
  if (jaddID == 0) {
    return $null;
  }

  jclass bool_class = JCALL1(FindClass, jenv, "java/lang/Boolean");
  jmethodID bool_constructor = JCALL3(GetMethodID, jenv, bool_class, "<init>",
      "(Z)V");
  for (std::vector<bool>::iterator it = ($1)->begin(); it != ($1)->end(); ++it) {
    // create a Java Boolean object
    jboolean temp_bool = (jboolean) *it;
    jobject temp_bool_object = JCALL3(NewObject, jenv, bool_class,
        bool_constructor, temp_bool);
    // put the bool into the std::vector object by calling List.add()
    JCALL3(CallBooleanMethod, jenv, $input, jaddID, temp_bool_object);
    if (JCALL0(ExceptionCheck, jenv)) {
      return $null;
    }
    JCALL1(DeleteLocalRef, jenv, temp_bool_object);
  }
}

#endif  // SWIGJAVA

// Typemaps for map<string, string> as both an input and an output argument.

#ifdef SWIGPYTHON

%typemap(in) const map<string, string>& (map<string, string> temp) {
  if (!PyDict_Check($input))
    SWIG_exception(SWIG_TypeError, "dictionary expected");
  PyObject *key, *val;
  Py_ssize_t pos = 0;
  while (PyDict_Next($input, &pos, &key, &val)) {
    if (!PyObjAs<string>(key, NULL))
      SWIG_exception(SWIG_TypeError, "non-string key in dictionary");
    if (!PyObjAs<string>(val, NULL))
      SWIG_exception(SWIG_TypeError, "non-string value in dictionary");
    temp[SwigString_AsString(key)] = SwigString_AsString(val);
  }
  $1 = &temp;
}

%typemap(in, numinputs=0) map<string, string>* (map<string, string> temp) {
  $1 = &temp;
}
%typemap(argout) map<string, string>* {
  Py_XDECREF($result);
  $result = PyDict_New();
  if (!$result) {
    SWIG_exception(SWIG_MemoryError, "PyDict_New() failed");
  }
  for (map<string, string>::const_iterator it = $1->begin();
       it != $1->end(); ++it) {
    PyObject *value = SwigString_FromString(it->second);
    if (!value) {
      Py_DECREF($result);
      SWIG_exception(SWIG_MemoryError, "SwigString_FromString() failed");
    }
    if (PyDict_SetItemString($result, it->first.c_str(), value) == -1) {
      Py_DECREF($result);
      Py_DECREF(value);
      SWIG_exception(SWIG_MemoryError, "PyDict_SetItemString() failed");
    }
    Py_DECREF(value);
  }
}

#endif  // SWIGPYTHON
