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
%include ortools/base/base.i

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
%typemap(ctype)  PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "int proto_size, uint8*"
%typemap(imtype) PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "int proto_size, byte[]"
%typemap(cstype) PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "CSharpProtoType"
%typemap(csin)   PROTO_TYPE* INPUT, PROTO_TYPE& INPUT "$csinput.CalculateSize(), ProtoHelper.ProtoToByteArray($csinput)"
%typemap(in)     PROTO_TYPE* INPUT, PROTO_TYPE& INPUT {
  $1 = new CppProtoType;
  bool parsed_ok = $1->ParseFromArray($input, proto_size);
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
%typemap(ctype)  CppProtoType "uint8*"
%typemap(imtype) CppProtoType "System.IntPtr"
%typemap(cstype) CppProtoType "CSharpProtoType"
%typemap(csout)  CppProtoType {
  byte[] tmp = new byte[4];
  System.IntPtr data = $imcall;
  System.Runtime.InteropServices.Marshal.Copy(data, tmp, 0, 4);
  int size = Convert.ToInt32(tmp[0]) +
      Convert.ToInt32(tmp[1]) * 0xFF +
      Convert.ToInt32(tmp[2]) * 0xFFFF +
      Convert.ToInt32(tmp[3]) * 0xFFFFFF;
  byte[] buf = new byte[size + 4];
  System.Runtime.InteropServices.Marshal.Copy(data, buf, 0, size + 4);
  // TODO(user): delete the C++ buffer.
  try {
    Google.Protobuf.CodedInputStream input =
        new Google.Protobuf.CodedInputStream(buf, 4, size);
    CSharpProtoType proto = new CSharpProtoType();
    proto.MergeFrom(input);
    return proto;
  } catch (Google.Protobuf.InvalidProtocolBufferException e) {
    throw new SystemException(
        "Unable to parse CSharpProtoType protocol message.");
  }
}
%typemap(out) CppProtoType {
  const int size = $1.ByteSize();
  $result = new uint8[size + 4];
  $1.SerializeWithCachedSizesToArray($result + 4);
  $result[0] = size & 0xFF;
  $result[1] = (size << 8) & 0xFF;
  $result[2] = (size << 16) & 0xFF;
  $result[3] = (size << 24) & 0xFF;
}
%enddef // end PROTO2_RETURN
