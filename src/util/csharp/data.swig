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
%include base/base.swig

%{
#include <vector>
#include "base/integral_types.h"
using std::string;
%}

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
%typemap(imtype)   const std::vector<TYPE>&  %{ int length$argnum, CSHARPTYPE[] %}
%typemap(cstype)   const std::vector<TYPE>&  %{ CSHARPTYPE[] %}
%typemap(csin)     const std::vector<TYPE>&  "$csinput.Length, $csinput"
%typemap(freearg)  const std::vector<TYPE>&  { delete $1; }
%typemap(in)       const std::vector<TYPE>&  %{
  $1 = new std::vector<TYPE>;
  $1->reserve(length$argnum);
  for(int i = 0; i < length$argnum; ++i) {
    $1->emplace_back($input[i]);
  }
%}
%enddef // CS_TYPEMAP_STDVECTOR

////////////////////////////////////////////////
//
//    CS_TYPEMAP_STDVECTOR_IN1
//
// Map c# bi-dimensional arrays to c++ vectors of vectors for POD types.
//
////////////////////////////////////////////////

%define CS_TYPEMAP_STDVECTOR_IN1(TYPE, CTYPE, CSHARPTYPE)
%typemap(ctype)  const std::vector<std::vector<TYPE> >&  %{
  int len$argnum_1, int len$argnum_2, CTYPE*
%}
%typemap(imtype) const std::vector<std::vector<TYPE> >&  %{
  int len$argnum_1, int len$argnum_2, CSHARPTYPE[]
%}
%typemap(cstype) const std::vector<std::vector<TYPE> >&  %{ CSHARPTYPE[,] %}
%typemap(csin)   const std::vector<std::vector<TYPE> >&  "$csinput.GetLength(0),$csinput.GetLength(1), NestedArrayHelper.GetFlatArray($csinput)"
%typemap(in)     const std::vector<std::vector<TYPE> >&  (std::vector<std::vector<TYPE> > result) %{
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

%typemap(ctype) CTYPE** "CTYPE**"

%typemap(imtype,
         inattributes="[In, Out, MarshalAs(UnmanagedType.LPArray)]",
         outattributes="[return: MarshalAs(UnmanagedType.LPArray)]")
CTYPE** "IntPtr[]"

%typemap(cstype)  CTYPE** "TYPE[]"
%typemap(csin)    CTYPE** "TYPE.getCPtr($csinput)"
%typemap(in)      CTYPE** "$1 = $input;"
%typemap(freearg) CTYPE** ""
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

%typemap(ctype)  const std::vector<CTYPE*>& "int length$argnum, CTYPE**"
%typemap(imtype) const std::vector<CTYPE*>& "int length$argnum, IntPtr[]"
%typemap(cstype) const std::vector<CTYPE*>& "TYPE[]"
%typemap(csin)   const std::vector<CTYPE*>& "$csinput.Length, TYPE.getCPtr($csinput)"
%typemap(in)     const std::vector<CTYPE*>& (std::vector<CTYPE*> result) {
  result.reserve(length$argnum);
  for (int i = 0; i < length$argnum; i++) {
    result.emplace_back($input[i]);
  }
  $1 = &result;
}
%enddef  // CS_TYPEMAP_STDVECTOR_OBJECT

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
%typemap(ctype)  PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "int proto_size, char*"
%typemap(imtype) PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "int proto_size, byte[]"
%typemap(cstype) PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "CSharpProtoType"
%typemap(csin)   PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "$csinput.GetByteArrayLength(), $csinput.ToByteArray()"
%typemap(in)     PROTO_TYPE* INPUT (CppProtoType temp), PROTO_TYPE& INPUT (CppProtoType temp) {
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
%typemap(ctype)  CppProtoType* "char*"
%typemap(imtype) CppProtoType* "byte[]"
%typemap(cstype) CppProtoType* "CSharpProtoType"
%typemap(csout)  CppProtoType* {
  byte[] buf = $imcall;
  if (buf == null || buf.Length == 0) {
    return null;
  }
  try {
    return CSharpProtoType.ParseFrom(buf);
  } catch (Google.Protobuf.InvalidProtocolBufferException e) {
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
