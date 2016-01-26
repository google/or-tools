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
%}

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
