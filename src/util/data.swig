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

%include base/base.swig

%{
#include <vector>
using std::string;
%}

#if defined(SWIGPYTHON)
%{
inline int PyIntOrLong_Check(PyObject* obj) {
  return PyInt_Check(obj) || PyLong_Check(obj);
}
%}
namespace operations_research {
// --------- std::vector<data> wrapping ----------

// We can't just reuse the google.swig code (see LIST_OUTPUT_TYPEMAP in that
// file); for two reasons:
// 1) We want a few extra typemaps (ITI integers <-> int, for example), but
//    can't invoke google.swig's LIST_OUTPUT_TYPEMAP because it's only
//    defined there. We may reuse base/swig/python-swig.cc though, since it's
//    bundled with the google.swig rule.
// 2) We use a lot of function overloading, so we must add extra typechecks.
//
// Note(user): for an unknown reason, using the (handy) method PyObjAs()
// defined in base/swig/python-swig.cc seems to cause issues, so we can't
// use a generic, templated type checker.
%define PY_LIST_OUTPUT_TYPEMAP(type, checker, py_converter)
%typecheck(SWIG_TYPECHECK_POINTER) const std::vector<type>& {
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
 hash_set<type>* OUTPUT (hash_set<type> temp),
 std::set<type>* OUTPUT (std::set<type> temp) {
  $1 = &temp;
}
%typemap(argout) std::vector<type>* OUTPUT, std::set<type>* OUTPUT, hash_set<type>* OUTPUT {
  %append_output(list_output_helper($1, &py_converter));
}
%typemap(out) std::vector<type> {
  $result = vector_output_helper(&$1, &py_converter);
}
%typemap(out) std::vector<type>*, const std::vector<type>& {
  $result = vector_output_helper($1, &py_converter);
}
%enddef

PY_LIST_OUTPUT_TYPEMAP(int, PyInt_Check, PyInt_FromLong);
PY_LIST_OUTPUT_TYPEMAP(long long, PyIntOrLong_Check, PyInt_FromLong);
PY_LIST_OUTPUT_TYPEMAP(double, PyFloat_Check, PyFloat_FromDouble);

// Add conversion list(tuple(int)) -> std::vector<vector>.
// TODO(user): see if we can also get rid of this and utilize already
// existing code.
%typemap(in) const std::vector<std::vector<int64> >&
    (std::vector<std::vector<int64> > temp) {
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

%typecheck(SWIG_TYPECHECK_POINTER) const std::vector<std::vector<int64> >& {
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

%define PY_PROTO_TYPEMAP(PythonModule, CppType, PythonType)
%typecheck(SWIG_TYPECHECK_POINTER) const CppType&, CppType* {
  bool ok = false;
  PyObject* const module = PyImport_ImportModule("PythonModule");
  if (module != nullptr) {
    PyObject* const dict = PyModule_GetDict(module);
    if (dict != nullptr) {
      PyObject* const clss = PyDict_GetItemString(dict, "PythonType");
      if (clss != nullptr) {
        if (PyObject_IsInstance($input, clss)) {
          ok = true;
        }
        Py_DECREF(clss);
      }
      Py_DECREF(dict);
    }
    Py_DECREF(module);
  }
  $1 = ok ? 1 : 0;
}

%typemap(in) const CppType&, CppType* const, CppType* {
  $1 = new CppType;
  PyObject* const pyresult = PyObject_CallMethod(
      $input, const_cast<char*>("SerializeToString"), nullptr);
  if (pyresult != nullptr) {
    char* buffer = nullptr;
    long length = 0;
    int result = PyString_AsStringAndSize(pyresult, &buffer, &length);
    if (buffer != nullptr) {
      $1->ParseFromArray(buffer, length);
    }
    Py_DECREF(pyresult);
  }
}

%typemap(freearg) const CppType&, CppType* const, CppType* {
  delete $1;
}

%typemap(argout) CppType* const, CppType* {
  std::string encoded_protobuf;
  $1->SerializeToString(&encoded_protobuf);
  PyObject* const python_encoded_protobuf =
      PyString_FromStringAndSize(encoded_protobuf.c_str(),
                                 encoded_protobuf.size());
  if (python_encoded_protobuf != nullptr) {
    PyObject* const result = PyObject_CallMethod(
        $input, const_cast<char*>("ParseFromString"),
        const_cast<char*>("(O)"), python_encoded_protobuf);
    PyObject_Print($input, stdout, 0);
    Py_DECREF(python_encoded_protobuf);
    if (result != nullptr) { Py_DECREF(result); }
  }
}

%typemap(out) CppType* {
  PyObject* const module = PyImport_ImportModule("PythonModule");
  if (module != nullptr) {
    PyObject* const dict = PyModule_GetDict(module);
    if (dict != nullptr) {
      PyObject* const clss = PyDict_GetItemString(dict, "PythonType");
      if (clss != nullptr) {
        PyObject* const proto = _PyObject_New(clss);
        if (proto != nullptr) {
          std::string encoded_protobuf;
          $1->SerializeToString(&encoded_protobuf);
          PyObject* const python_encoded_protobuf = PyString_FromStringAndSize(
              encoded_protobuf.c_str(), encoded_protobuf.size());
          PyObject* const result = PyObject_CallMethod(
              proto, const_cast<char*>("ParseFromString"),
              const_cast<char*>("(O)"),
              python_encoded_protobuf);
          if (result != nullptr) { Py_DECREF(result); }
          $result = proto;
        }
        Py_DECREF(clss);
      }
      Py_DECREF(dict);
    }
    Py_DECREF(module);
  }
}
%enddef  // PY_PROTO_TYPEMAP

}  // namespace operations_research
#endif  // SWIGPYTHON

#if defined(SWIGJAVA)

%{
#include "base/jniutil.h"
#include "base/scoped_ptr.h"
%}

%include <std_vector.i>
%{
#include <vector>
#include "base/integral_types.h"
%}

// Typemaps to represent const std::vector<CType>& arguments as arrays of JavaType.
%define VECTOR_AS_JAVA_ARRAY(CType, JavaType, JavaTypeName)
%typemap(jni) const std::vector<CType>& "j" #JavaType "Array"
%typemap(jtype) const std::vector<CType>& #JavaType "[]"
%typemap(jstype) const std::vector<CType>& #JavaType "[]"
%typemap(javain) const std::vector<CType>& "$javainput"
%typemap(in) const std::vector<CType>& %{
  if($input) {
    $1 = new std::vector<CType>;
    const int size = jenv->GetArrayLength($input);
    $1->reserve(size);
    j ## JavaType *values = jenv->Get ## JavaTypeName ## ArrayElements((j ## JavaType ## Array)$input, NULL);
    for (int i = 0; i < size; ++i) {
      JavaType value = values[i];
      $1->emplace_back(value);
    }
    jenv->Release ## JavaTypeName ## ArrayElements((j ## JavaType ## Array)$input, values, JNI_ABORT);
  }
  else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null table");
    return $null;
  }
%}
%typemap(freearg) const std::vector<CType>& {
  delete $1;
}
%typemap(out) const std::vector<CType>& %{
  $result = jenv->New ## JavaTypeName ## Array($1->size());
  jenv->Set ## JavaTypeName ## ArrayRegion(
      $result, 0, $1->size(), reinterpret_cast<j ## CType*>(&(*$1)[0]));
%}
%typemap(javaout) const std::vector<CType> & {
  return $jnicall;
}
%enddef
VECTOR_AS_JAVA_ARRAY(int, int, Int);
VECTOR_AS_JAVA_ARRAY(int64, long, Long);

// Convert long[][] to std::vector<std::vector<int64> >
%typemap(jni) const std::vector<std::vector<int64> >& "jobjectArray"
%typemap(jtype) const std::vector<std::vector<int64> >& "long[][]"
%typemap(jstype) const std::vector<std::vector<int64> >& "long[][]"
%typemap(javain) const std::vector<std::vector<int64> >& "$javainput"
%typemap(in) const std::vector<std::vector<int64> >& (std::vector<std::vector<int64> > result) {
  const int size = jenv->GetArrayLength($input);
  result.clear();
  result.resize(size);
  for (int index1 = 0; index1 < size; ++index1) {
    jlongArray inner_array =
        (jlongArray)jenv->GetObjectArrayElement($input, index1);
    const int tuple_size = jenv->GetArrayLength(inner_array);
    result[index1].reserve(tuple_size);
    jlong* const values =
        jenv->GetLongArrayElements((jlongArray)inner_array, NULL);
    for (int index2 = 0; index2 < tuple_size; ++index2) {
      const int64 value = values[index2];
      result[index1].emplace_back(value);
    }
    jenv->ReleaseLongArrayElements((jlongArray)inner_array, values, JNI_ABORT);
    jenv->DeleteLocalRef(inner_array);
  }
  $1 = &result;
}

// SWIG macros to be used in generating Java wrappers for C++ protocol
// message parameters.  Each protocol message is serialized into
// byte[] before passing into (or returning from) C++ code.

// If the C++ function expects an input protocol message:
//   foo(const MyProto* message,...)
// Use PROTO_INPUT macro:
//   PROTO_INPUT(MyProto, com.google.proto.protos.test.MyProto, message)
//
// if the C++ function returns a protocol message:
//   MyProto* foo();
// Use PROTO2_RETURN macro:
//   PROTO2_RETURN(MyProto, com.google.proto.protos.test.MyProto, giveOwnership)
//   -> the 'giveOwnership' parameter should be true iff the C++ function
//      returns a new proto which should be deleted by the client.

// Passing each protocol message from Java to C++ by value. Each ProtocolMessage
// is serialized into byte[] when it is passed from Java to C++, the C++ code
// deserializes into C++ native protocol message.
//
// @param CppProtoType the fully qualified C++ protocol message type
// @param JavaProtoType the corresponding fully qualified Java protocol message
//        type
// @param param_name the parameter name
%define PROTO_INPUT(CppProtoType, JavaProtoType, param_name)
%typemap(jni) PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "jbyteArray"
%typemap(jtype) PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "byte[]"
%typemap(jstype) PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "JavaProtoType"
%typemap(javain) PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "$javainput.toByteArray()"
%typemap(in) PROTO_TYPE* INPUT (CppProtoType temp),
             PROTO_TYPE& INPUT (CppProtoType temp) {
  int proto_size = 0;
  std::unique_ptr<char[]> proto_buffer(
    JNIUtil::MakeCharArray(jenv, $input, &proto_size));
  bool parsed_ok = temp.ParseFromArray(proto_buffer.get(), proto_size);
  if (!parsed_ok) {
    SWIG_JavaThrowException(jenv,
                            SWIG_JavaRuntimeException,
                            "Unable to parse CppProtoType protocol message.");
  }
  $1 = &temp;
}

%apply PROTO_TYPE& INPUT { const CppProtoType& param_name }
%apply PROTO_TYPE& INPUT { CppProtoType& param_name }
%apply PROTO_TYPE* INPUT { const CppProtoType* param_name }
%apply PROTO_TYPE* INPUT { CppProtoType* param_name }

%enddef // end PROTO_INPUT

%define PROTO2_RETURN(CppProtoType, JavaProtoType, giveOwnership)
%typemap(jni) CppProtoType* "jbyteArray"
%typemap(jtype) CppProtoType* "byte[]"
%typemap(jstype) CppProtoType* "JavaProtoType"
%typemap(javaout) CppProtoType* {
  byte[] buf = $jnicall;
  if (buf == null || buf.length == 0) {
    return null;
  }
  try {
    return JavaProtoType.parseFrom(buf);
  } catch (com.google.protobuf.InvalidProtocolBufferException e) {
    throw new RuntimeException(
        "Unable to parse JavaProtoType protocol message.");
  }
}
%typemap(out) CppProtoType* {
  std::unique_ptr<char[]> buf(new char[$1->ByteSize()]);
  $1->SerializeWithCachedSizesToArray(reinterpret_cast<uint8*>(buf.get()));
  $result = JNIUtil::MakeJByteArray(jenv, buf.get(), $1->ByteSize());
  if (giveOwnership) {
    // To prevent a memory leak.
    delete $1;
    $1 = NULL;
  }
}
%enddef // end PROTO2_RETURN

#endif  // SWIGJAVA

#if defined(SWIGCSHARP)
%include base/base.swig

////////////////////////////////////////////////
//
// CS_TYPEMAP_STDVECTOR
//
// Map c# arrays to c++ vectors for POD types.
//
////////////////////////////////////////////////

// This typemap will transform a .net array into a pair length, c array.
// This pair is then used to rebuild a vector from it.
%define CS_TYPEMAP_STDVECTOR(TYPE, CTYPE, CSHARPTYPE)

%typemap(ctype)    const std::vector<TYPE>&  %{ int length$argnum, CTYPE* %}
%typemap(imtype)  const std::vector<TYPE>&  %{ int length$argnum, CSHARPTYPE[] %}
%typemap(cstype)  const std::vector<TYPE>&  %{ CSHARPTYPE[] %}
%typemap(csin)    const std::vector<TYPE>&  "$csinput.Length, $csinput"
%typemap(freearg)  const std::vector<TYPE>&  { delete $1; }
%typemap(in)    const std::vector<TYPE>&  %{
  $1 = new std::vector<TYPE>;
  $1->reserve(length$argnum);
  for(int i = 0; i < length$argnum; ++i)
    $1->emplace_back($input[i]);
  %}
%enddef // CS_TYPEMAP_STDVECTOR

////////////////////////////////////////////////
//
//    CS_TYPEMAP_STDVECTOR_IN1
//
// Map c# bi-dimensional arrays to c++ vectors of vectors for POD types.
//
////////////////////////////////////////////////

// This typemap will transform a multidimensional array into a triplet
// size_z, size_y, flattened array.  This is then recomposed in the
// final part to rebuild the std::vector<std::vector<T>> part.
%define CS_TYPEMAP_STDVECTOR_IN1(TYPE, CTYPE, CSHARPTYPE)
%typemap(ctype)    const std::vector<std::vector<TYPE> >&  %{ int len$argnum_1, int len$argnum_2, CTYPE* %}
%typemap(imtype)  const std::vector<std::vector<TYPE> >&  %{ int len$argnum_1, int len$argnum_2, CSHARPTYPE[] %}
%typemap(cstype)  const std::vector<std::vector<TYPE> >&  %{ CSHARPTYPE[,] %}
%typemap(csin)    const std::vector<std::vector<TYPE> >&  "$csinput.GetLength(0), $csinput.GetLength(1), Google.OrTools.Util.NestedArrayHelper.GetFlatArray($csinput)"
%typemap(in)    const std::vector<std::vector<TYPE> >&  (std::vector<std::vector<TYPE> > result) %{

  const int size_x = len$argnum_1;
  const int size_y = len$argnum_2;

  result.clear();
  result.resize(size_x);

  TYPE* inner_array = reinterpret_cast<TYPE*>($input);

  for (int index1 = 0; index1 < size_x; ++index1) {
    result[index1].reserve(size_y);
    for (int index2 = 0; index2 < size_y; ++index2) {
      const TYPE value = inner_array[index1 * size_y + index2];
      result[index1].emplace_back(value);
    }
  }

  $1 = &result;
%}
%enddef // CS_TYPEMAP_STDVECTOR_IN1

////////////////////////////////////////////////
//
//    CS_TYPEMAP_PTRARRAY
//
////////////////////////////////////////////////

// This typemap will perform the same transformation for an array of object.
// The result is an vector of the C objects.
%define CS_TYPEMAP_PTRARRAY(CTYPE, TYPE)

%typemap(cscode) CTYPE %{
  public static IntPtr[] getCPtr(TYPE[] arr) {
    IntPtr[] pointers = new IntPtr[arr.Length];
    for (int i = 0; i < arr.Length; i++) {
      pointers[i] = (IntPtr)TYPE.getCPtr(arr[i]);
    }
    return pointers;
  }
%}

%typemap(ctype)    CTYPE** "CTYPE**"

%typemap(imtype,
         inattributes="[In, Out, MarshalAs(UnmanagedType.LPArray)]",
         outattributes="[return: MarshalAs(UnmanagedType.LPArray)]")
CTYPE** "IntPtr[]"

%typemap(cstype)  CTYPE** "TYPE[]"
%typemap(csin)    CTYPE** "TYPE.getCPtr($csinput)"
%typemap(in)    CTYPE** "$1 = $input;"
%typemap(freearg)  CTYPE** ""
%typemap(argout)  CTYPE** ""

%enddef  // CS_TYPEMAP_PTRARRAY

////////////////////////////////////////////////
//
// CS_TYPEMAP_STDVECTOR_OBJECT
//
// Map c# arrays to c++ vectors for swiged C++ objects.
//
////////////////////////////////////////////////

%define CS_TYPEMAP_STDVECTOR_OBJECT(CTYPE, TYPE)

%typemap(cscode) CTYPE %{
  public static IntPtr[] getCPtr(TYPE[] arr) {
    IntPtr[] pointers = new IntPtr[arr.Length];
    for (int i = 0; i < arr.Length; i++)
      pointers[i] = (IntPtr)TYPE.getCPtr(arr[i]);
    return pointers;
  }
%}

%typemap(ctype) const std::vector<CTYPE*>& "int length$argnum, CTYPE**"
%typemap(imtype) const std::vector<CTYPE*>& "int length$argnum, IntPtr[]"
%typemap(cstype) const std::vector<CTYPE*>& "TYPE[]"
%typemap(csin) const std::vector<CTYPE*>& "$csinput.Length, TYPE.getCPtr($csinput)"
%typemap(in) const std::vector<CTYPE*>& (std::vector<CTYPE*> result) {
  result.reserve(length$argnum);
  for (int i = 0; i < length$argnum; i++) {
    result.emplace_back($input[i]);
  }
  $1 = &result;
}
%enddef  // CS_TYPEMAP_STDVECTOR_OBJECT

%{
#include <vector>
#include "base/integral_types.h"
%}

%{
#include <vector>
#include "base/integral_types.h"
%}

CS_TYPEMAP_STDVECTOR(int64, int64, long)
CS_TYPEMAP_STDVECTOR(int, int, int)
CS_TYPEMAP_STDVECTOR_IN1(int64, int64, long)
CS_TYPEMAP_STDVECTOR_IN1(int, int, int)

// SWIG macros to be used in generating C# wrappers for C++ protocol
// message parameters.  Each protocol message is serialized into
// byte[] before passing into (or returning from) C++ code.

// If the C++ function expects an input protocol message, transferring
// ownership to the caller (in C++):
//   foo(const MyProto* message,...)
// Use PROTO_INPUT macro:
//   PROTO_INPUT(MyProto, Google.Proto.Protos.Test.MyProto, message)
//
// if the C++ function returns a protocol message:
//   MyProto* foo();
// Use PROTO2_RETURN macro:
//   PROTO2_RETURN(MyProto, Google.Proto.Protos.Test.MyProto, true)
//
// Replace true by false if the C++ function returns a pointer to a
// protocol message object whose ownership is not transferred to the
// (C++) caller.
//
// Passing each protocol message from C# to C++ by value. Each ProtocolMessage
// is serialized into byte[] when it is passed from C# to C++, the C++ code
// deserializes into C++ native protocol message.
//
// @param CppProtoType the fully qualified C++ protocol message type
// @param CSharpProtoType the corresponding fully qualified C# protocol message
//        type
// @param param_name the parameter name
// @param deleteCppReturn indicates that the resulting object is a native
// (java, c#, python) object, and thus the C++ proto can be safely deleted
// after the conversion.
%define PROTO_INPUT(CppProtoType, CSharpProtoType, param_name)
%typemap(ctype)    PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "int proto_size, char*"
%typemap(imtype)  PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "int proto_size, byte[]"
%typemap(cstype)  PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "CSharpProtoType"
%typemap(csin)    PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "$csinput.GetByteArrayLength(), $csinput.ToByteArray()"
%typemap(in) PROTO_TYPE* INPUT (CppProtoType temp), PROTO_TYPE& INPUT (CppProtoType temp) {
  int proto_size = 0;
  std::unique_ptr<char[]> proto_buffer($input);
  bool parsed_ok = temp.ParseFromArray(proto_buffer.get(), proto_size);
  if (!parsed_ok) {
    SWIG_CSharpSetPendingException(
        SWIG_CSharpSystemException,
        "Unable to parse CppProtoType protocol message.");
  }
  $1 = &temp;
}

%apply PROTO_TYPE& INPUT { const CppProtoType& param_name }
%apply PROTO_TYPE& INPUT { CppProtoType& param_name }
%apply PROTO_TYPE* INPUT { const CppProtoType* param_name }
%apply PROTO_TYPE* INPUT { CppProtoType* param_name }

%enddef // end PROTO_INPUT

%define PROTO2_RETURN(CppProtoType, CSharpProtoType, deleteCppReturn)
%typemap(ctype) CppProtoType* "char*"
%typemap(imtype) CppProtoType* "byte[]"
%typemap(cstype) CppProtoType* "CSharpProtoType"
%typemap(csout) CppProtoType* {
  byte[] buf = $imcall;
  if (buf == null || buf.Length == 0) {
    return null;
  }
  try {
    return CSharpProtoType.ParseFrom(buf);
  }
  catch (Google.Protobuf.InvalidProtocolBufferException e) {
    throw new SystemException(
        "Unable to parse CSharpProtoType protocol message.");
  }
}
%typemap(out) CppProtoType* {
  std::unique_ptr<char[]> buf(new char[$1->ByteSize()]);
  $1->SerializeWithCachedSizesToArray(reinterpret_cast<uint8*>(buf.get()));
  $result = buf.get();
  if (deleteCppReturn) {
    // To prevent a memory leak.
    delete $1;
    $1 = NULL;
  }
}
%enddef // end PROTO2_RETURN_AND_DELETE
#endif  // SWIGCSHARP
