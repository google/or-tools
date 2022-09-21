// Copyright 2010-2022 Google LLC
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

%{
#include <vector>
#include "ortools/base/integral_types.h"
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
%define PROTO_INPUT(CppProtoType, CSharpProtoType, param_name)
%typemap(ctype)  PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "int " #param_name "_size, uint8_t*"
%typemap(imtype) PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "int " #param_name "_size, byte[]"
%typemap(cstype) PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "CSharpProtoType"
%typemap(csin)   PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "ProtoHelper.ProtoToByteArray($csinput, out var buffer), buffer"
%typemap(in)     PROTO_TYPE* INPUT, PROTO_TYPE& INPUT {
  $1 = new CppProtoType;
  bool parsed_ok = $1->ParseFromArray($input, param_name ## _size);
  if (!parsed_ok) {
    SWIG_CSharpSetPendingException(
        SWIG_CSharpSystemException,
        "Unable to parse CppProtoType protocol message.");
  }
}
%typemap(freearg) PROTO_TYPE* INPUT, PROTO_TYPE& INPUT {
  delete $1;
 }
%apply PROTO_TYPE& INPUT { const CppProtoType& param_name }
%apply PROTO_TYPE& INPUT { CppProtoType& param_name }
%apply PROTO_TYPE* INPUT { const CppProtoType* param_name }
%apply PROTO_TYPE* INPUT { CppProtoType* param_name }
%enddef // end PROTO_INPUT

%define PROTO2_RETURN(CppProtoType, CSharpProtoType)
%typemap(ctype)  CppProtoType "uint8_t*"
%typemap(imtype) CppProtoType "System.IntPtr"
%typemap(cstype) CppProtoType "CSharpProtoType"
%typemap(csout)  CppProtoType {
  System.IntPtr dataPointer = $imcall;
  try
  {
    int size = System.Runtime.InteropServices.Marshal.ReadInt32(dataPointer);
    unsafe
    {
      var data = new System.ReadOnlySpan<byte>((dataPointer + sizeof(int)).ToPointer(), size);
      return CSharpProtoType.Parser.ParseFrom(data);
    }
  } catch (Google.Protobuf.InvalidProtocolBufferException /*e*/)
  {
    throw new System.Exception("Unable to parse CSharpProtoType protocol message.");
  }
  finally
  {
    Google.OrTools.Init.CppBridge.DeleteByteArray(dataPointer);
  }
}
%typemap(out) CppProtoType {
  const long size = $1.ByteSizeLong();
  $result = new uint8_t[size + 4];
  $1.SerializeWithCachedSizesToArray($result + 4);
  $result[0] = size & 0xFF;
  $result[1] = (size >> 8) & 0xFF;
  $result[2] = (size >> 16) & 0xFF;
  $result[3] = (size >> 24) & 0xFF;
}

%enddef // end PROTO2_RETURN

